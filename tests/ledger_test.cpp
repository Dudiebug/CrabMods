// Ledger tests. Simulates the host poll loop from main.cpp with in-memory players.
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <map>

#include "../crabsync/ledger.hpp"

using namespace crabsync;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

constexpr ItemId DoubleShot = 0x1000, BigMag = 0x1010, Collector = 0x2000, Relic1 = 0x3000;
constexpr ItemId Pistol = 0x9000, Shotgun = 0x9010;

struct Sim {
    Ledger ledger;
    std::map<PlayerKey, Inventory> players;
    bool modelMax = false;  // max health = base max, as UpdateMaxHealth computes it after a base write

    explicit Sim(Settings s = {}) : ledger(s) {}

    // Mirrors Sync(): absorb everyone's changes, then drive everyone to the target.
    void Poll() {
        if (!ledger.hasTeam()) {
            std::vector<const Inventory*> all;
            for (auto& [k, v] : players) all.push_back(&v);
            ledger.seed(all);
        }
        std::vector<PlayerKey> keys;
        std::vector<Inventory> now;
        for (auto& [k, v] : players) {
            keys.push_back(k);
            now.push_back(v);
        }
        ledger.absorbAll(keys, now);
        for (auto& [k, v] : players) {
            v = ledger.target(v);
            // ApplyTarget writes the base, UpdateMaxHealth sets the max (cutting health down to it), then
            // health is set, capped to the new max.
            if (modelMax && v.life == Life::Alive) {
                v.maxHealth = v.baseMax;
                v.health = std::min(v.health, v.maxHealth);
            }
            ledger.setBaseline(k, v);
        }
    }
};

static int Level(const Inventory& inv, int cat, ItemId id) {
    const Item* i = FindItem(inv.items[cat], id);
    return i ? i->level : 0;
}

static void Pickup(Inventory& inv, int cat, ItemId id, int n = 1) {
    Item* i = FindItem(inv.items[cat], id);
    if (i)
        i->level = uint8_t(std::min(255, i->level + n));
    else
        inv.items[cat].push_back({id, uint8_t(n), {}, 0.f});
}

static void TestPickupPropagates() {
    std::puts("pickup propagates");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Pickup(s.players[1], WeaponMods, DoubleShot);
    s.Poll();
    CHECK(Level(s.players[2], WeaponMods, DoubleShot) == 1);
    CHECK(Level(s.players[1], WeaponMods, DoubleShot) == 1);
}

static void TestNoFeedback() {
    std::puts("no feedback across many polls");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.players[3] = {};
    s.Poll();
    Pickup(s.players[2], Perks, Collector);
    for (int i = 0; i < 20; i++) s.Poll();
    for (auto& [k, v] : s.players) CHECK(Level(v, Perks, Collector) == 1);
    s.players[1].crystals += 50;
    for (int i = 0; i < 20; i++) s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.crystals == 50);
}

static void TestSimultaneousPickupsStack() {
    std::puts("simultaneous pickups stack");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Pickup(s.players[1], WeaponMods, DoubleShot);
    Pickup(s.players[2], WeaponMods, DoubleShot);
    s.Poll();
    CHECK(Level(s.players[1], WeaponMods, DoubleShot) == 2);
    CHECK(Level(s.players[2], WeaponMods, DoubleShot) == 2);
}

static void TestSalvageRemovesForEveryone() {
    std::puts("salvage removes for everyone and pays the shared wallet");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Pickup(s.players[1], WeaponMods, BigMag, 3);
    s.Poll();
    auto& b = s.players[2].items[WeaponMods];
    b.erase(b.begin());
    s.players[2].crystals += 25;
    s.Poll();
    CHECK(Level(s.players[1], WeaponMods, BigMag) == 0);
    CHECK(s.players[1].crystals == 25);
}

static void TestSharedWallet() {
    std::puts("crystals: gains and spending from different players combine");
    Sim s;
    s.players[1].crystals = 100;
    s.players[2].crystals = 100;
    s.Poll();
    s.players[1].crystals += 10;
    s.players[2].crystals -= 30;
    s.Poll();
    CHECK(s.players[1].crystals == 80);
    CHECK(s.players[2].crystals == 80);
    s.players[1].crystals = 0;  // spends everything
    s.players[2].crystals -= 5;  // same tick: would go below zero
    s.Poll();
    CHECK(s.players[2].crystals == 0);
}

static void TestEquipmentNewestWins() {
    std::puts("equipment: newest change wins");
    Sim s;
    s.players[1].equip[Weapon] = Pistol;
    s.players[2].equip[Weapon] = Pistol;
    s.Poll();
    s.players[2].equip[Weapon] = Shotgun;
    s.Poll();
    CHECK(s.players[1].equip[Weapon] == Shotgun);
}

static void TestStartingWeaponsKept() {
    std::puts("starting weapons are kept until someone swaps");
    Sim s;
    s.players[1].equip[Weapon] = Pistol;
    s.players[2].equip[Weapon] = Shotgun;
    s.Poll();
    s.Poll();
    CHECK(s.players[1].equip[Weapon] == Pistol);
    CHECK(s.players[2].equip[Weapon] == Shotgun);
}

static void TestItemOrderIsStable() {
    std::puts("each player's item order is kept, so target equals state after one apply");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    Pickup(s.players[1], WeaponMods, DoubleShot);
    Pickup(s.players[1], WeaponMods, BigMag);
    Pickup(s.players[2], WeaponMods, BigMag);
    Pickup(s.players[2], WeaponMods, DoubleShot);
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(s.ledger.target(v) == v);
    CHECK(s.players[2].items[WeaponMods][0].id == BigMag);
}

static void TestRestoreIsNotAPickup() {
    std::puts("autosave restore after travel is not counted as a pickup");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Pickup(s.players[1], Relics, Relic1);
    Pickup(s.players[1], WeaponMods, DoubleShot, 2);
    s.Poll();
    Inventory saved1 = s.players[1], saved2 = s.players[2];

    // Travel: new PlayerState objects (new keys) with default state, then each re-uploads its save.
    s.ledger.clearBaselines();
    s.players.clear();
    s.players[11] = {};
    s.players[12] = {};
    s.Poll();  // fresh players are driven to the team state
    s.players[11] = saved1;
    s.ledger.forget(11);
    s.Poll();
    s.players[12] = saved2;
    s.ledger.forget(12);
    s.Poll();
    for (auto& [k, v] : s.players) {
        CHECK(Level(v, WeaponMods, DoubleShot) == 2);
        CHECK(Level(v, Relics, Relic1) == 1);
    }
}

static void TestStaleRestoreIsCorrected() {
    std::puts("a stale restore is corrected to the team state");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Inventory stale = s.players[2];
    Pickup(s.players[1], Perks, Collector);
    s.Poll();
    s.players[2] = stale;  // its snapshot missed the last pickup
    s.ledger.forget(2);
    s.Poll();
    CHECK(Level(s.players[2], Perks, Collector) == 1);
    CHECK(Level(s.players[1], Perks, Collector) == 1);
}

static void TestDisabledCategoryStaysLocal() {
    std::puts("disabled categories stay per-player");
    Settings st;
    st.cats[Perks] = false;
    Sim s(st);
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Pickup(s.players[1], Perks, Collector);
    Pickup(s.players[1], WeaponMods, DoubleShot);
    s.Poll();
    CHECK(Level(s.players[1], Perks, Collector) == 1);
    CHECK(Level(s.players[2], Perks, Collector) == 0);
    CHECK(Level(s.players[2], WeaponMods, DoubleShot) == 1);
}

static void TestEnhancementsAndBuffs() {
    std::puts("enhancements and accumulated buffs propagate");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Pickup(s.players[1], WeaponMods, DoubleShot);
    s.Poll();
    FindItem(s.players[2].items[WeaponMods], DoubleShot)->enhancements = {3, 7};
    FindItem(s.players[1].items[WeaponMods], DoubleShot)->buff += 0.5f;
    FindItem(s.players[2].items[WeaponMods], DoubleShot)->buff += 0.25f;
    s.Poll();
    for (auto& [k, v] : s.players) {
        const Item* i = FindItem(v.items[WeaponMods], DoubleShot);
        CHECK(i && i->enhancements == (std::vector<uint8_t>{3, 7}));
        CHECK(i && i->buff == 0.75f);
    }
}

static void TestSlots() {
    std::puts("slot purchases are shared");
    Sim s;
    s.players[1].slots = {3, 2, 2, 3};
    s.players[2].slots = {3, 2, 2, 3};
    s.Poll();
    s.players[2].slots[PerkSlots]++;
    s.players[2].crystals = 0;
    s.Poll();
    CHECK(s.players[1].slots[PerkSlots] == 4);
}

static void TestGameWideChangeCountedOnce() {
    std::puts("a change the game makes to everyone at once is counted once");
    Sim s;
    for (PlayerKey k : {1, 2, 3}) s.players[k].slots = {16, 4, 4, 16};
    s.Poll();
    for (auto& [k, v] : s.players) v.slots = {2, 1, 1, 2};  // run start resets everyone's slots
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.slots == (std::array<uint8_t, NumSlotKinds>{2, 1, 1, 2}));
    for (auto& [k, v] : s.players) v.crystals += 34;  // the game pays everyone
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.crystals == 34);
    s.players[1].crystals += 5;  // one player's own pickup still counts
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.crystals == 39);
}

static void TestHeldPlayerSharedEventCountedOnce() {
    std::puts("a held (mid-change) player still counts towards a shared event, and is absorbed later");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    for (auto& [k, v] : s.players) v.crystals += 34;  // the game pays everyone...
    Pickup(s.players[1], Perks, Collector);            // ...while player 1 is mid-pickup
    std::vector<PlayerKey> keys{1, 2};
    std::vector<Inventory> now{s.players[1], s.players[2]};
    std::vector<bool> hold{true, false};
    s.ledger.absorbAll(keys, now, nullptr, &hold);
    CHECK(s.ledger.team().crystals == 34);
    CHECK(Level(s.ledger.team(), Perks, Collector) == 0);  // not taken in yet
    s.ledger.setBaseline(2, s.players[2]);
    s.Poll();  // player 1 has held still: now absorbed
    for (auto& [k, v] : s.players) {
        CHECK(v.crystals == 34);
        CHECK(Level(v, Perks, Collector) == 1);
    }
}

static void TestResetForgetsTeam() {
    std::puts("reset starts a fresh team");
    Sim s;
    s.players[1] = {};
    s.Poll();
    Pickup(s.players[1], Perks, Collector);
    s.Poll();
    s.ledger.reset();
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    CHECK(Level(s.players[2], Perks, Collector) == 0);
}

static void Spawn(Inventory& inv, uintptr_t body, float hp, float max = 100.f) {
    inv.life = Life::Alive;
    inv.body = body;
    inv.health = hp;
    inv.maxHealth = max;
    inv.baseMax = max;
}

// Mirrors the game changing one player's base max (and so their max, cutting health down to it).
static void AddBase(Inventory& inv, float d) {
    inv.baseMax += d;
    inv.maxHealth = inv.baseMax;
    inv.health = std::min(inv.health, inv.maxHealth);
}

// Mirrors the game swapping a dead player's character for a spectator in the frame they die.
static void Spectate(Inventory& inv, uintptr_t spectator) {
    inv.life = Life::Dead;
    inv.body = spectator;
    inv.health = inv.maxHealth = 0.f;
}

// Mirrors the game killing a character: health 0, dead.
static void Hit(Inventory& inv, float dmg) {
    inv.health = std::max(0.f, inv.health - dmg);
    if (inv.health <= 0.f) inv.life = Life::Dead;
}

static void TestHealthPoolShared() {
    std::puts("health: damage and healing hit the shared pool");
    Sim s;
    Spawn(s.players[1], 101, 100);
    Spawn(s.players[2], 102, 100);
    s.Poll();
    Hit(s.players[1], 30);
    s.Poll();
    CHECK(s.players[2].health == 70.f);
    Hit(s.players[1], 10);
    Hit(s.players[2], 15);
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.health == 45.f);
    s.players[2].health += 80;  // healing past max is capped
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.health == 100.f);
}

static void TestHealthPoolEmptyKillsEveryone() {
    std::puts("health: when the pool hits 0 everyone dies");
    Sim s;
    Spawn(s.players[1], 101, 20);
    Spawn(s.players[2], 102, 20);
    Spawn(s.players[3], 103, 20);
    s.Poll();
    Hit(s.players[2], 50);
    s.Poll();
    for (auto& [k, v] : s.players) {
        CHECK(v.life == Life::Dead);
        CHECK(v.health == 0.f);
    }
}

static void TestHealthEmptyPoolKillsOnce() {
    std::puts("health: an empty pool kills who was alive then, never a character that spawns later");
    Sim s;
    Spawn(s.players[1], 101, 50);
    s.Poll();
    Hit(s.players[1], 60);
    s.Poll();
    CHECK(s.players[1].life == Life::Dead);
    Spawn(s.players[1], 201, 40);  // respawned (or a lobby character) while the old pool is still empty
    for (int i = 0; i < 3; i++) s.Poll();
    CHECK(s.players[1].life == Life::Alive && s.players[1].health >= 1.f);
}

static void TestHealthStaleSeedDoesNotKill() {
    std::puts("health: a team started from a badly hurt character does not kill a fresh one");
    Sim s;
    Spawn(s.players[1], 101, 314, 613);  // the run's last character, 299 damage taken
    s.Poll();
    Spawn(s.players[1], 201, 100, 100);  // the lobby character: less max health than the damage
    for (int i = 0; i < 3; i++) s.Poll();
    CHECK(s.players[1].life == Life::Alive);
}

static void TestHealthLobbyFullAndSafe() {
    std::puts("health: in the lobby everyone is at full (pool-sized) health and nothing is shared or fatal");
    Sim s;
    s.ledger.setLobby(true);
    Spawn(s.players[1], 101, 314, 613);  // the team starts from the run's last, badly hurt character
    Spawn(s.players[2], 102, 100, 350);
    s.Poll();
    Spawn(s.players[1], 201, 100, 350);  // lobby character: max below the old damage
    Hit(s.players[2], 40);
    for (int i = 0; i < 3; i++) s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.life == Life::Alive && v.health == 350.f);
}

static void TestHealthNoFalseWipe() {
    std::puts("health: loading, respawned or uninitialised characters never drain the pool");
    Sim s;
    Spawn(s.players[1], 101, 60);
    Spawn(s.players[2], 102, 60);
    s.Poll();
    s.players[1] = Inventory{};  // character gone (map loading, spectating)
    s.Poll();
    CHECK(s.players[2].life == Life::Alive && s.players[2].health == 60.f);
    Spawn(s.players[1], 201, 100);  // new character at full health: not a heal, gets the pool value
    s.Poll();
    CHECK(s.players[1].health == 60.f);
    CHECK(s.players[2].health == 60.f);
}

static void TestHealthWaitsForSomeoneAlive() {
    std::puts("health: no pool until someone is alive, then everyone starts from it");
    Sim s;
    s.players[1] = {};
    s.players[2] = {};
    s.Poll();
    Spawn(s.players[1], 101, 90);
    s.Poll();
    Spawn(s.players[2], 102, 100);
    s.Poll();
    CHECK(s.players[1].life == Life::Alive && s.players[2].life == Life::Alive);
    CHECK(s.players[2].health == 90.f);
}

static void TestHealthPoolGrowsWithPlayers() {
    std::puts("health: a higher max (another player's 250) raises the pool; a lower one shrinks it");
    Sim s;
    Spawn(s.players[1], 101, 250, 250);
    s.Poll();
    Spawn(s.players[2], 102, 250, 250);
    for (auto& [k, v] : s.players) v.maxHealth = 500;  // the second player's base is added to everyone
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.health == 500.f);
    Hit(s.players[2], 100);
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.health == 400.f);
    s.players.erase(2);  // they leave: max drops back and the game clamps health to it
    s.players[1].maxHealth = 250;
    s.players[1].health = 250;
    s.Poll();
    CHECK(s.players[1].life == Life::Alive && s.players[1].health == 250.f);  // 400 cut down to 250
}

static void TestDeathAsSpectatorKillsEveryone() {
    std::puts("health: a player whose character became a spectator died, and everyone dies with them");
    Sim s;
    Spawn(s.players[1], 101, 100);
    Spawn(s.players[2], 102, 100);
    Spawn(s.players[3], 103, 100);
    s.Poll();
    Hit(s.players[1], 30);
    s.Poll();
    // The killing hit is never seen: in the next read player 2 is already a spectator.
    Spectate(s.players[2], 902);
    s.Poll();
    CHECK(s.players[1].life == Life::Dead && s.players[3].life == Life::Dead);
    s.Poll();  // the dead stay dead and nothing else happens
    CHECK(s.players[1].life == Life::Dead && s.players[3].life == Life::Dead);
}

static void TestMaxHealthShared() {
    std::puts("max health: one player's loss (Eternal Punishment) or gain is everyone's, and health follows the game's rules");
    Sim s;
    s.modelMax = true;
    Spawn(s.players[1], 101, 100, 200);
    Spawn(s.players[2], 102, 100, 200);
    s.Poll();
    // Player 1 is hit for 10 and loses 1 max health.
    Hit(s.players[1], 10);
    AddBase(s.players[1], -1.f);
    s.ledger.addBaseMax(-1.f);
    s.Poll();
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.baseMax == 199.f && v.maxHealth == 199.f && v.health == 90.f);
    // A totem doubles player 2's base: everyone's doubles, and the new max adds health.
    AddBase(s.players[2], 199.f);
    s.ledger.addBaseMax(199.f);
    s.Poll();
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.maxHealth == 398.f && v.health == 289.f);
    // A loss at full health only cuts health down to the new max.
    s.players[1].health = 398.f;
    s.Poll();
    AddBase(s.players[1], -98.f);
    s.ledger.addBaseMax(-98.f);
    s.Poll();
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.maxHealth == 300.f && v.health == 300.f);
}

static void TestChestCostSplit() {
    std::puts("max health: a chest's cost is split between the players (chestHealthCost = split)");
    Sim s;
    s.modelMax = true;
    for (PlayerKey k = 1; k <= 4; k++) Spawn(s.players[k], 100 + k, 400, 400);
    s.Poll();
    // A max health cost given without its fraction is divided by the number of players.
    AddBase(s.players[1], -80.f);
    s.ledger.chestCost(1, -80.f, 0.f);
    s.Poll();
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.maxHealth == 380.f && v.health == 380.f);
    // Player 2 opens a spiked chest: 40 health.
    Hit(s.players[2], 40);
    s.ledger.chestCost(2, 0.f, 40.f);
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.health == 370.f);
    // Ordinary damage afterwards is not refunded.
    Hit(s.players[2], 40);
    s.Poll();
    for (auto& [k, v] : s.players) CHECK(v.health == 330.f);
}

static void TestSpikedChestsTotal() {
    std::puts("max health: every player opening a 35% spiked chest costs the team 35% in all");
    for (int n = 1; n <= 4; n++) {
        Sim s;
        s.modelMax = true;
        for (PlayerKey k = 1; k <= PlayerKey(n); k++) Spawn(s.players[k], 100 + k, 400, 400);
        s.Poll();
        for (PlayerKey k = 1; k <= PlayerKey(n); k++) {
            float d = -0.35f * s.players[k].baseMax;
            AddBase(s.players[k], d);
            s.ledger.chestCost(k, d, 0.f, 0.35f);
            s.Poll();
            s.Poll();
            if (n > 1 && k == 1) CHECK(s.players[2].maxHealth > 300.f);  // one chest is only a share
        }
        for (auto& [k, v] : s.players) CHECK(std::fabs(v.maxHealth - 260.f) < 0.5f && v.health == v.maxHealth);
    }
}

static void TestChestCostFullAndFree() {
    std::puts("max health: chestHealthCost = full charges the whole cost, free charges nothing");
    for (ChestCost mode : {ChestCost::Full, ChestCost::Free}) {
        Settings st;
        st.chestCost = mode;
        Sim s(st);
        s.modelMax = true;
        Spawn(s.players[1], 101, 400, 400);
        Spawn(s.players[2], 102, 400, 400);
        s.Poll();
        AddBase(s.players[1], -80.f);
        s.ledger.chestCost(1, -80.f, 0.f);
        Hit(s.players[2], 40);
        s.ledger.chestCost(2, 0.f, 40.f);
        s.Poll();
        s.Poll();
        float max = mode == ChestCost::Full ? 320.f : 400.f, hp = mode == ChestCost::Full ? 320.f : 400.f;
        for (auto& [k, v] : s.players) CHECK(v.maxHealth == max && v.health == hp);
    }
}

static void TestMaxHealthNotSharedInLobby() {
    std::puts("max health: the lobby keeps everyone's own");
    Sim s;
    s.modelMax = true;
    s.ledger.setLobby(true);
    Spawn(s.players[1], 101, 100, 100);
    Spawn(s.players[2], 102, 100, 100);
    s.Poll();
    AddBase(s.players[1], -10.f);
    s.ledger.addBaseMax(-10.f);
    s.Poll();
    CHECK(s.players[1].maxHealth == 90.f && s.players[2].maxHealth == 100.f);
}

static void TestHealthDisabledStaysLocal() {
    std::puts("health: syncHealth=false keeps health per-player");
    Settings st;
    st.health = false;
    Sim s(st);
    Spawn(s.players[1], 101, 100);
    Spawn(s.players[2], 102, 100);
    s.Poll();
    Hit(s.players[1], 100);
    s.Poll();
    CHECK(s.players[2].life == Life::Alive && s.players[2].health == 100.f);
}

int main() {
    TestHealthPoolShared();
    TestHealthPoolEmptyKillsEveryone();
    TestHealthEmptyPoolKillsOnce();
    TestHealthStaleSeedDoesNotKill();
    TestHealthLobbyFullAndSafe();
    TestHealthNoFalseWipe();
    TestHealthWaitsForSomeoneAlive();
    TestHealthPoolGrowsWithPlayers();
    TestHealthDisabledStaysLocal();
    TestDeathAsSpectatorKillsEveryone();
    TestMaxHealthShared();
    TestChestCostSplit();
    TestChestCostFullAndFree();
    TestSpikedChestsTotal();

    TestMaxHealthNotSharedInLobby();

    TestGameWideChangeCountedOnce();
    TestHeldPlayerSharedEventCountedOnce();
    TestPickupPropagates();
    TestNoFeedback();
    TestSimultaneousPickupsStack();
    TestSalvageRemovesForEveryone();
    TestSharedWallet();
    TestEquipmentNewestWins();
    TestStartingWeaponsKept();
    TestItemOrderIsStable();
    TestRestoreIsNotAPickup();
    TestStaleRestoreIsCorrected();
    TestDisabledCategoryStaysLocal();
    TestEnhancementsAndBuffs();
    TestSlots();
    TestResetForgetsTeam();
    std::printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
