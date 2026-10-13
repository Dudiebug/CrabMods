// Greed Run, a game mode: a portal in the lobby, opposite the Arcade and Holdout portals, starts a normal run where
// everything is Greed (plans/greed-mode.md).
// - Host, lobby: after the lobby's own portals, spawns a New Run portal (SpawnablesDA.NewRunPortal) with the Greed
//   reward in its PortalInfo. That replicated reward recolours it and gives it the Greed icon on every machine, and
//   tells CrabMods on each machine that this New Run portal is the Greed Run one (its card is renamed).
// - Host: when the party takes it (ACrabGM_EnterChosenPortal), the mode is on until the next lobby. A Continue Run
//   portal takes the mode the run was saved with (GreedRun.state next to the DLL). The host tells every player.
// - Host, the run: every island reward chest is a Greed chest, and a Greed chest's roll offers every item (any loot
//   pool, the usual rarity odds, the game's own Greed items too) with extraChoices more choices.
// - Every item is its Greed version: each pickup that appears (chests, totems, the shop) comes higher (mods
//   extraLevels, perks perkExtraLevels: a level 3 perk is 3x as strong), and each level of a non-Greed item you hold
//   carries a curse. Every machine shows it: the card says "Greed" in the Greed colour, the item's text its curse.
// - Curses are costs no item can cancel (no stat that more damage or health items would just add back): the fight
//   gets harder through the game's own Greed perk rules (more enemies like Juggernaut, tougher enemies like Double
//   Trouble, enemies hitting harder), or crystals, healing and ability cooldown. Each perk has its
//   own, themed on it (kPerkCurses): damage perks spawn more enemies, health perks give enemies more health. Mods
//   get a smaller one by loot pool. Enemy curses add up over the whole team, as the game's Greed perks do.
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../world.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

constexpr wchar_t kTag[] = L"greedRun ";  // host to player: "greedRun 1" / "greedRun 0"

enum Curse {
    MoreEnemies,     // more enemies spawn (as Juggernaut), the whole team's added up
    TougherEnemies,  // enemies have more health (as Double Trouble), the whole team's added up
    DamageTaken,     // enemies hit you harder
    Healing,         // less healing
    CrystalShare,    // a share of your crystals lost each time you're hit
    Cooldown,        // longer ability cooldown
    kCurses
};

// A curse per level held, in percent.
struct PerkCurse {
    const char* perk;  // DA_Perk_<perk>
    Curse curse;
    float perLevel;
};

// The sizes. A Greed perk comes at level 3, so it costs 3x these: one damage perk is +30% enemies.
constexpr float kMore = 10, kTougher = 15, kHarder = 12, kHeal = 15, kCrystals = 3, kCooldown = 15;

// Each perk's own curse: damage perks spawn more enemies, health perks give enemies more health, close range and
// speed perks get you hit harder, economy perks cost crystals, luck perks (better loot) make enemies tougher.
constexpr PerkCurse kPerkCurses[] = {
    // Damage
    {"DamageCombo", MoreEnemies, kMore}, {"DangerClose", DamageTaken, kHarder}, {"EagleEye", MoreEnemies, kMore},
    {"EnhancedTurrets", MoreEnemies, kMore}, {"Equalizer", TougherEnemies, kTougher},
    {"ExplosiveArmor", DamageTaken, kHarder}, {"PowerArmor", Healing, kHeal}, {"Assassin", TougherEnemies, kTougher},
    {"Checklist", MoreEnemies, kMore}, {"Collector", CrystalShare, kCrystals}, {"CrimsonHaze", MoreEnemies, kMore},
    {"DamageAura", DamageTaken, kHarder}, {"ExplodingEnemies", MoreEnemies, kMore},
    {"Gemstone", TougherEnemies, kTougher}, {"OrbitingScythes", MoreEnemies, kMore},
    {"SilverLining", DamageTaken, kHarder}, {"TastyOrange", MoreEnemies, kMore},
    // Critical
    {"AntiCrit", TougherEnemies, kTougher}, {"CriticalArrow", MoreEnemies, kMore},
    {"CriticalThinking", TougherEnemies, kTougher}, {"HotShot", TougherEnemies, kTougher},
    {"PowerPunch", TougherEnemies, kTougher}, {"Sharpshooter", TougherEnemies, kTougher},
    {"CriticalBlast", MoreEnemies, kMore}, {"MegaCrit", TougherEnemies, kTougher},
    {"CriticalLightning", MoreEnemies, kMore},
    // Elemental
    {"ElementalExpert", TougherEnemies, kTougher}, {"ElementalSpecialist", TougherEnemies, kTougher},
    {"Firestarter", Healing, kHeal}, {"HighVoltage", MoreEnemies, kMore}, {"HotSteam", TougherEnemies, kTougher},
    {"IceCold", MoreEnemies, kMore}, {"PoisonousArmor", DamageTaken, kHarder}, {"PotentMagic", Cooldown, kCooldown},
    {"Toxic", TougherEnemies, kTougher}, {"FireAura", DamageTaken, kHarder}, {"IceAura", DamageTaken, kHarder},
    {"LightningAura", DamageTaken, kHarder}, {"PoisonAura", DamageTaken, kHarder},
    {"FlammableArmor", DamageTaken, kHarder}, {"ElectricEnemies", MoreEnemies, kMore},
    {"FlammableEnemies", MoreEnemies, kMore}, {"FreezingEnemies", MoreEnemies, kMore},
    {"PoisonousEnemies", MoreEnemies, kMore}, {"IceDash", Cooldown, kCooldown}, {"LightningDash", Cooldown, kCooldown},
    // Speed
    {"SpeedDemon", DamageTaken, kHarder}, {"Stamina", DamageTaken, kHarder}, {"SpeedIsPower", MoreEnemies, kMore},
    {"DaggerDash", DamageTaken, kHarder}, {"Powerslide", DamageTaken, kHarder},
    // Luck: better loot, tougher enemies
    {"BountyHunter", TougherEnemies, kTougher}, {"Driller", TougherEnemies, kTougher}, {"StreamerLoot", TougherEnemies, kTougher},
    {"DoubleVision", TougherEnemies, kTougher}, {"PerformanceBonus", DamageTaken, kHarder},
    {"CrystalAsteroids", TougherEnemies, kTougher}, {"FaultyChests", TougherEnemies, kTougher},
    {"RareTreasure", TougherEnemies, kTougher},
    // Health: you get tougher, so do they
    {"Banana", TougherEnemies, kTougher}, {"Bulletproof", MoreEnemies, kMore}, {"Bullseye", TougherEnemies, kTougher},
    {"Endurance", TougherEnemies, kTougher}, {"Fortitude", TougherEnemies, kTougher},
    {"HardTarget", MoreEnemies, kMore}, {"Mango", TougherEnemies, kTougher}, {"PersonalSpace", TougherEnemies, kTougher},
    {"Regenerator", MoreEnemies, kMore}, {"Scavenger", TougherEnemies, kTougher},
    {"ValuedCustomer", CrystalShare, kCrystals}, {"Vitality", DamageTaken, kHarder},
    {"AmberResin", TougherEnemies, kTougher}, {"GraveDodger", DamageTaken, kHarder},
    {"GrimReaper", TougherEnemies, kTougher}, {"HealthIsPower", TougherEnemies, kTougher},
    {"Survivor", MoreEnemies, kMore},
    // Economy: money costs money
    {"CrystalCombo", CrystalShare, kCrystals}, {"BonusCrystals", CrystalShare, kCrystals},
    {"Snatcher", DamageTaken, kHarder}, {"GoldCoating", CrystalShare, kCrystals}, {"Paycheck", CrystalShare, kCrystals},
    {"SpecialDelivery", CrystalShare, kCrystals}, {"TonysBlackCard", CrystalShare, kCrystals},
    {"MoneyIsPower", CrystalShare, 4},
    // Skill
    {"CrystalFertilizer", MoreEnemies, kMore}, {"Magnify", TougherEnemies, kTougher}, {"Slugger", MoreEnemies, kMore},
    {"AllYouCanEat", Cooldown, kCooldown}, {"BigChests", TougherEnemies, kTougher}, {"Shockwave", DamageTaken, kHarder},
    {"SturdyTotems", Cooldown, kCooldown}, {"TotemEnthusiast", Cooldown, kCooldown},
    {"BonusChests", MoreEnemies, kMore}, {"CarePackage", TougherEnemies, kTougher}, {"LevelUp", TougherEnemies, kTougher},
    // CrabMods
    {"ArmyOfTheDead", MoreEnemies, kMore}, {"Legion", MoreEnemies, kMore}, {"HollowHunger", Healing, kHeal},
    {"Autoloader", Cooldown, kCooldown}, {"ComboCatalyst", MoreEnemies, kMore},
    {"CollateralDamage", DamageTaken, kHarder}, {"FinishingMove", TougherEnemies, kTougher},
    {"Overflow", TougherEnemies, kTougher}, {"ThickShell", MoreEnemies, kMore}, {"Underdog", DamageTaken, kHarder},
    {"Untouchable", MoreEnemies, kMore}, {"Hoarder", CrystalShare, kCrystals}, {"Pathfinder", TougherEnemies, kTougher},
    {"RiggedTotems", Cooldown, kCooldown}, {"JackOfAllTrades", TougherEnemies, kTougher},
    {"Minimalist", TougherEnemies, kTougher}, {"CommunitySupport", MoreEnemies, kMore},
    {"GreedIsPower", TougherEnemies, kTougher}, {"GrimHarvest", Healing, kHeal}, {"HungryGhosts", MoreEnemies, kMore},
    {"LastRites", TougherEnemies, kTougher}, {"PlagueBearers", MoreEnemies, kMore},
    {"SnapDecision", Cooldown, kCooldown}, {"SoulJar", TougherEnemies, kTougher},
};

// A perk not in the table: a curse by its loot pool.
PerkCurse PerkCurseOf(uint8_t lootPool) {
    switch (lootPool) {
    case pool::Critical: case pool::Health: case pool::Luck: return {nullptr, TougherEnemies, kTougher};
    case pool::Speed: return {nullptr, DamageTaken, kHarder};
    case pool::Economy: return {nullptr, CrystalShare, kCrystals};
    case pool::Skill: return {nullptr, Cooldown, kCooldown};
    }
    return {nullptr, MoreEnemies, kMore};
}

// A mod's or relic's smaller curse, by its loot pool.
PerkCurse ModCurseOf(uint8_t lootPool) {
    switch (lootPool) {
    case pool::Damage: case pool::Critical: case pool::Health: case pool::Luck: return {nullptr, TougherEnemies, 5};
    case pool::Speed: return {nullptr, DamageTaken, 4};
    case pool::Economy: return {nullptr, CrystalShare, 1};
    case pool::Skill: return {nullptr, Cooldown, 5};
    }
    return {nullptr, MoreEnemies, 3};
}

using EnterPortalFn = void (*)(void*);
using PortalUIFn = void (*)(void*);
EnterPortalFn oEnterPortal;
PortalUIFn oInitUI, oUpdateUI;

class GreedRun;
GreedRun* g_self;

class GreedRun : public Item {
public:
    GreedRun() : Item("greedRun", 1000) { g_self = this; }

    std::string DefaultIni() const override {
        return R"(# Greed Run, a game mode: a portal in the lobby, opposite the minigames, starts a normal run where everything is
# Greed. Every chest is a Greed chest that can offer any item, and every item comes higher but carries a curse for
# each level you hold: more enemies, tougher enemies, enemies hitting harder, less healing, lost crystals
# or a longer ability cooldown. Every item's text shows its curse. You can always leave an item behind.
enabled = true

# Levels added to every perk that appears (2 = a level 3 perk, 3x as strong), and to every mod (relics stay level 1).
perkExtraLevels = 2
extraLevels = 1
# Size of every curse, in percent of the built-in amounts: perks' own, and the smaller ones of mods and relics.
perkCurseScale = 100
modCurseScale = 100
# Choices added to every Greed chest (it has 1 of its own).
extraChoices = 2

# Where the portal stands in the lobby (the mirror of the Arcade and Holdout portals).
portalX = -5757
portalY = 5000
portalZ = 1000
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "extraLevels") extraLevels_ = std::clamp(int(ParseFloat(v)), 0, 10);
        else if (k == "perkExtraLevels") perkExtraLevels_ = std::clamp(int(ParseFloat(v)), 0, 10);
        else if (k == "perkCurseScale") perkScale_ = std::max(ParseFloat(v), 0.f) / 100.f;
        else if (k == "modCurseScale") modScale_ = std::max(ParseFloat(v), 0.f) / 100.f;
        else if (k == "extraChoices") extraChoices_ = std::clamp(int(ParseFloat(v)), 0, 10);
        else if (k == "portalX") portal_.x = ParseFloat(v);
        else if (k == "portalY") portal_.y = ParseFloat(v);
        else if (k == "portalZ") portal_.z = ParseFloat(v);
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override {
        return "+" + std::to_string(extraLevels_) + " levels (perks +" + std::to_string(perkExtraLevels_) + "), +" +
               std::to_string(extraChoices_) + " choices, curses per level (perks x" + Num(perkScale_) + ", mods x" +
               Num(modScale_) + ")";
    }
    std::string Title() const override { return "Greed Run"; }
    const char* CatalogKind() const override { return "Game mode"; }

    bool Active() const { return enabled_ && active_; }

    void InstallHooks() override {
        retHasPerk_ = Addr(va::Ret_EnemyHealthHasDoubleTrouble);
        retPerkDebuff_ = Addr(va::Ret_EnemyHealthDoubleTrouble);
        Hook(va::ACrabGM_EnterChosenPortal, &HkEnterPortal, &oEnterPortal, "ACrabGM::EnterChosenPortal");
        Hook(va::ACrabPortal_InitUI, &HkInitUI, &oInitUI, "ACrabPortal::InitUI");
        Hook(va::ACrabPortal_UpdateUI, &HkUpdateUI, &oUpdateUI, "ACrabPortal::UpdateUI");
        Hook(va::ACrabGS_EnemySpawnMultiplier, &HkSpawnMultiplier, &oSpawnMultiplier,
             "ACrabGS enemy spawn multiplier (Greed Run: more enemies)");
        Hook(va::ACrabPS_HasPerk, &HkHasPerk, &oHasPerk, "ACrabPS::HasPerk (Greed Run: tougher enemies)");
        Hook(va::ACrabPS_GetPerkDebuff, &HkPerkDebuff, &oPerkDebuff, "ACrabPS::GetPerkDebuff (Greed Run: tougher enemies)");
    }

    // --- The portal and the mode ---------------------------------------------------------------------------

    void OnPortalsSpawned(void* lm) override {
        if (Island(lm) == uint8_t(IslandType::Lobby) && IsHost(lm)) spawnPending_ = true;
    }

    void OnTick(void* world) override {
        void* gs = GameStateOf(world);
        if (!gs) return;
        uint8_t island = At<uint8_t>(gs, off::GS_CurrentIslandType);
        if (world != world_) {
            world_ = world;
            told_.clear();
            spawnedPortal_ = nullptr;
            lastIsland_ = 0xFF;
            leveled_.clear();
            markedLm_ = nullptr;
        }
        lm_ = LevelManagerOf(world);
        if (Active() && lm_ && lm_ != markedLm_) {
            markedLm_ = lm_;
            MarkItems(lm_);
        }
        // Back in the lobby: the run is over.
        if (island != lastIsland_) {
            lastIsland_ = island;
            if (island == uint8_t(IslandType::Lobby)) SetActive(false, "back in the lobby");
        }
        if (At<uint8_t>(gs, off::Actor_Role) != ROLE_Authority) return;
        if (spawnPending_ && island == uint8_t(IslandType::Lobby)) {
            spawnPending_ = false;
            SpawnPortal(world);
        }
        // Tell every player whether this is a Greed Run, once per player controller and each time it changes.
        ForEachPlayer(world, [&](void* ps) {
            void* pc = At<void*>(ps, off::Actor_Owner);
            if (!pc || pc == LocalPC(world)) return;
            if (told_.insert(pc).second) SendToPlayer(pc, kTag + std::wstring(Active() ? L"1" : L"0"));
        });
    }

    bool OnClientMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, std::size(kTag) - 1, kTag) != 0) return false;
        SetActive(text[std::size(kTag) - 1] == L'1', "the host says so");
        return true;
    }

    // The item text changes are put back by AssetItem's refresh at every level start; this item comes last.
    void OnBeginPlay(void* lm) override {
        lm_ = lm;
        if (Active()) {
            markedLm_ = lm;
            MarkItems(lm);
        }
    }

    // --- Chests ------------------------------------------------------------------------------------------------

    void OnRewardChests(void* ps, TArray<void*>& chests) override {
        if (!Active()) return;
        void* lm = LevelManagerOf(ps);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        void* greed = sp ? At<void*>(sp, off::Spawnables_GreedChest) : nullptr;
        if (!greed) return;
        for (int i = 0; i < chests.Num; i++) chests.Data[i] = greed;
    }

    // A Greed chest's roll (pool and rarity Greed) offers any item at the usual rarity odds: Random pool, Common
    // (the roll bumps choices to Epic and Legendary as usual). The game's own Greed items join at Common meanwhile.
    void BeforeLootRoll(uint8_t& lootPool, uint8_t& rarity, int& choices) override {
        if (!Active() || lootPool != pool::Greed || rarity != rarity::Greed || !lm_) return;
        lootPool = pool::Random;
        rarity = rarity::Common;
        choices += extraChoices_;
        auto& all = At<TArray<void*>>(lm_, off::LM_AllUnlockedPickups);
        for (int i = 0; i < all.Num; i++)
            if (void* da = all.Data[i]; da && At<uint8_t>(da, off::DA_Rarity) == rarity::Greed) {
                lowered_.push_back(da);
                At<uint8_t>(da, off::DA_Rarity) = rarity::Common;
            }
    }
    void AfterLootRoll() override {
        for (void* da : lowered_) At<uint8_t>(da, off::DA_Rarity) = rarity::Greed;
        lowered_.clear();
    }

    // --- Pickup cards ------------------------------------------------------------------------------------------

    void Card(void* pickup, bool before) {
        if (!Active() || !pickup) return;
        void* da = At<void*>(pickup, off::Pickup_PickupInfo + off::PickupInfo_DA);
        if (!da || At<uint8_t>(da, off::DA_Rarity) == rarity::Greed) return;
        uint8_t& level = At<uint8_t>(pickup, off::Pickup_PickupInfo + off::PickupInfo_Level);
        if (before) {
            // Host: once per pickup, before its card is filled and before it is sent to the players. Not a pickup a
            // player just dropped (they had it at that level already).
            if (At<uint8_t>(pickup, off::Actor_Role) != ROLE_Authority || g_droppingPickup || !Levels(da)) return;
            if (WorldOf(pickup) != world_ || !leveled_.insert(pickup).second) return;
            level = uint8_t(std::min(int(level) + (IsPerk(da) ? perkExtraLevels_ : extraLevels_), 255));
            return;
        }
        void* card = At<void*>(pickup, off::Pickup_UI);
        if (!card) return;
        const auto& name = At<TArray<wchar_t>>(da, off::DA_Name);
        std::wstring s = L"Greed " + (name.Data && name.Num > 1 ? std::wstring(name.Data, name.Num - 1) : L"");
        if (level >= 2) s += L" (Lv " + std::to_wstring(level) + L")";
        TArray<wchar_t> fs{s.data(), int(s.size()) + 1, int(s.size()) + 1};
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetName)(card, &fs);
        float color[4];
        Fn<void* (*)(float*, uint8_t)>(va::RarityColor)(color, rarity::Greed);
        Fn<void (*)(void*, const float*)>(va::InteractUI_SetRarityColor)(card, color);
    }

    // --- Curses ------------------------------------------------------------------------------------------------

    float HealingBonus(void* ps) override {
        if (!Active()) return 0.f;
        return std::max(-Held(ps)[Healing], -kMaxHealingCut);
    }
    void OnTakeDamage(void* hc, void* info) override {
        if (!Active()) return;
        void* ps = PlayerOfHealth(hc);
        float& damage = At<float>(info, off::DamageInfo_Damage);
        if (!ps || damage <= 0.f) return;
        CurseLevels l = Held(ps);
        damage *= 1.f + l[DamageTaken];
        int have = At<int>(ps, off::PS_Crystals);
        int lose = std::min(int(have * std::min(l[CrystalShare], kMaxCrystalShare)), have);
        if (lose > 0) Fn<void (*)(void*, int)>(va::ACrabPS_AddCrystals)(ps, -lose);
    }
    // The cooldown counts down 0.25 s per tick; giving part of it back makes it last 1 + k times as long.
    void OnCooldownTick(void* c, void* ps) override {
        if (!Active()) return;
        float k = Held(ps)[Cooldown];
        float& cd = At<float>(c, off::PlayerC_AbilityCooldown);
        if (k > 0.f && cd > 0.f) cd += 0.25f * k / (1.f + k);
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!Active() || !da || At<uint8_t>(da, off::DA_Rarity) == rarity::Greed) return {};
        int level = 0;
        for (Kind k : kKinds) level += LevelOf(ps, k, da);
        if (!level) return {};
        const Spec& sp = SpecOf(da);
        return "Greed curse: " + CurseText(sp.curse, level * Amount(sp));
    }

private:
    static constexpr float kMaxHealingCut = 0.9f;    // healing never drops below 10%
    static constexpr float kMaxCrystalShare = 0.5f;  // no hit takes more than half your crystals
    static constexpr float kMaxMoreEnemies = 3.f;    // the team's curses add at most +300% enemies
    static constexpr float kMaxTougher = 4.f;        // and at most +400% enemy health a player
    static constexpr Kind kKinds[] = {Kind::WeaponMod, Kind::AbilityMod, Kind::Perk, Kind::Relic, Kind::MeleeMod};

    struct CurseLevels {
        float n[kCurses] = {};
        float operator[](Curse c) const { return n[c]; }
    };

    static uint8_t Island(void* context) {
        void* gs = GameStateOf(context);
        return gs ? At<uint8_t>(gs, off::GS_CurrentIslandType) : 0;
    }
    static bool IsHost(void* context) {
        void* gs = GameStateOf(context);
        return gs && At<uint8_t>(gs, off::Actor_Role) == ROLE_Authority;
    }
    static void* LocalPC(void* world) {
        void* lm = LevelManagerOf(world);
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        return c ? At<void*>(c, off::Pawn_Controller) : nullptr;
    }
    // Kinds of item that stack in levels (as Lucky Charm's).
    static bool Levels(void* da) {
        if (At<bool>(da, off::DA_bLimited)) return false;
        for (Kind k : {Kind::WeaponMod, Kind::AbilityMod, Kind::MeleeMod, Kind::Perk})
            if (IsA(da, StaticClass(Info(k).staticClass))) return true;
        return false;
    }

    static bool IsPerk(void* da) { return da && IsA(da, StaticClass(Info(Kind::Perk).staticClass)); }

    // An item's curse: a perk's own (kPerkCurses, else by its pool), a mod's or relic's by its pool. Looked up
    // once per item.
    struct Spec {
        Curse curse;
        bool perk;
        float perLevel;  // in percent
    };
    const Spec& SpecOf(void* da) {
        auto it = specs_.find(da);
        if (it != specs_.end()) return it->second;
        uint8_t lootPool = At<uint8_t>(da, off::DA_LootPool);
        PerkCurse mc = ModCurseOf(lootPool);
        Spec sp{mc.curse, false, mc.perLevel};
        if (IsPerk(da)) {
            PerkCurse pc = PerkCurseOf(lootPool);
            std::string name = ObjectName(da);
            if (name.rfind("DA_Perk_", 0) == 0) name.erase(0, 8);
            for (const PerkCurse& c : kPerkCurses)
                if (name == c.perk) pc = c;
            sp = {pc.curse, true, pc.perLevel};
        }
        return specs_.emplace(da, sp).first->second;
    }
    float Amount(const Spec& sp) const {
        float scale = sp.perk ? perkScale_ : modScale_;
        return sp.perLevel / 100.f * scale;
    }

    // The player's curses: each non-Greed item's curse times its level, added up by kind.
    CurseLevels Held(void* ps) {
        CurseLevels l;
        if (!ps) return l;
        for (Kind k : kKinds)
            ForEachItem(ps, Info(k).playerArray, [&](void* da, int level) {
                if (At<uint8_t>(da, off::DA_Rarity) == rarity::Greed) return;
                const Spec& sp = SpecOf(da);
                l.n[sp.curse] += level * Amount(sp);
            });
        return l;
    }

    std::string CurseText(Curse c, float amount) const {
        std::string pct = Num(AssetItem::Round(amount * 100.f)) + "%";
        switch (c) {
        case MoreEnemies: return "+" + pct + " enemies (the whole team)";
        case TougherEnemies: return "+" + pct + " enemy health (the whole team)";
        case DamageTaken: return "enemies hit you " + pct + " harder";
        case Cooldown: return "+" + pct + " ability cooldown";
        case Healing: return "-" + pct + " healing";
        case CrystalShare: return "lose " + pct + " of your crystals when hit";
        default: return {};
        }
    }

    void SetActive(bool on, const char* why) {
        if (on == active_) return;
        active_ = on;
        told_.clear();
        CRAB_LOG("greedRun: %s (%s)", on ? "Greed Run on" : "Greed Run off", why);
        // Marked from OnTick, where the level manager is known to be the current one.
        markedLm_ = nullptr;
        if (!on) UnmarkItems();
    }

    // The curse line at the end of every non-Greed item's texts, while the mode is on.
    void MarkItems(void* lm) {
        auto mark = [&](void* da) {
            if (!da || At<uint8_t>(da, off::DA_Rarity) == rarity::Greed) return;
            const Spec& sp = SpecOf(da);
            std::string t = CurseText(sp.curse, Amount(sp)) + " per level";
            if (sp.perk && perkExtraLevels_ > 0)
                t = "comes " + std::to_string(perkExtraLevels_) + " levels higher, but " + t;
            std::wstring suffix = L"\nGreed: " + std::wstring(t.begin(), t.end());
            for (int field : {off::DA_Description, off::DA_LevelDescription}) {
                const auto& s = At<TArray<wchar_t>>(da, field);
                std::wstring now = s.Data && s.Num > 1 ? std::wstring(s.Data, s.Num - 1) : L"";
                if (now.size() >= suffix.size() && now.compare(now.size() - suffix.size(), suffix.size(), suffix) == 0)
                    continue;
                original_[{da, field}] = now;
                SetFString(da, field, (now + suffix).c_str());
            }
        };
        auto& all = At<TArray<void*>>(lm, off::LM_AllUnlockedPickups);
        for (int i = 0; i < all.Num; i++) mark(all.Data[i]);
        for (Kind k : kKinds)
            if (auto* list = Spawnables(lm, Info(k).spawnablesList))
                for (int i = 0; i < list->Num; i++) mark(list->Data[i]);
    }
    void UnmarkItems() {
        for (auto& [key, text] : original_) SetFString(key.first, key.second, text.c_str());
        original_.clear();
    }

    void SpawnPortal(void* world) {
        void* lm = LevelManagerOf(world);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        void* cls = sp ? At<void*>(sp, off::Spawnables_NewRunPortal) : nullptr;
        if (!cls || !enabled_) return;
        alignas(16) uint8_t params[off::SpawnParams_Size] = {};
        Fn<void (*)(void*)>(va::FActorSpawnParameters_Ctor)(params);
        params[off::SpawnParams_Collision] = 1;  // AlwaysSpawn
        float rotation[3] = {};
        void* portal = Fn<void* (*)(void*, void*, const Vec3*, const float*, const void*)>(va::UWorld_SpawnActor)(
            world, cls, &portal_, rotation, params);
        if (!portal) {
            CRAB_LOG("greedRun: could not spawn the Greed Run portal");
            return;
        }
        alignas(8) uint8_t info[off::PortalInfo_Size] = {};
        info[0] = 1;  // bIsInited
        info[off::PortalInfo_RewardLootPool] = pool::Greed;
        Fn<void (*)(void*, void*)>(va::ACrabPortal_SetInfo)(portal, info);
        spawnedPortal_ = portal;
        CRAB_LOG("greedRun: Greed Run portal at (%g, %g, %g)", portal_.x, portal_.y, portal_.z);
    }

    // A New Run portal with the Greed reward is the Greed Run portal (on every machine: the reward is replicated).
    static bool IsGreedPortal(void* portal) {
        return portal && At<uint8_t>(portal, off::Portal_IslandType) == uint8_t(IslandType::NewRun) &&
               At<uint8_t>(portal, off::Portal_Info + off::PortalInfo_RewardLootPool) == pool::Greed;
    }

    static void Relabel(void* portal) {
        if (!g_self->enabled_ || !IsGreedPortal(portal)) return;
        void* card = At<void*>(portal, off::Pickup_UI);
        if (!card) return;
        wchar_t name[] = L"Greed Run";
        wchar_t text[] = L"A new run where everything is Greed: every chest, every item. Perks come 3x as strong, "
                         L"but each one brings more enemies, tougher enemies or another price of its own";
        TArray<wchar_t> n{name, int(std::size(name)), int(std::size(name))};
        TArray<wchar_t> t{text, int(std::size(text)), int(std::size(text))};
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetName)(card, &n);
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetDescription)(card, &t);
    }
    static void HkInitUI(void* portal) {
        oInitUI(portal);
        Relabel(portal);
    }
    static void HkUpdateUI(void* portal) {
        oUpdateUI(portal);
        Relabel(portal);
    }

    // --- Enemies (host) ----------------------------------------------------------------------------------------
    //
    // The game adds up every player's Juggernaut, Up The Ante and Rising Star for the enemy spawn multiplier, and
    // every player's Double Trouble for each new enemy's health multiplier. The curses join those sums.

    using SpawnMultiplierFn = float (*)(void*);
    using HasPerkFn = bool (*)(void*, uint8_t);
    using PerkDebuffFn = float (*)(void*, uint8_t);
    static inline SpawnMultiplierFn oSpawnMultiplier;
    static inline HasPerkFn oHasPerk;
    static inline PerkDebuffFn oPerkDebuff;
    static inline uintptr_t retHasPerk_ = 0, retPerkDebuff_ = 0;

    static float HkSpawnMultiplier(void* gs) {
        float r = oSpawnMultiplier(gs);
        GreedRun& self = *g_self;
        if (!gs || !self.Active()) return r;
        float more = 0.f;
        auto& players = At<TArray<void*>>(gs, off::GS_PlayerArray);
        for (int i = 0; i < players.Num; i++)
            if (IsA(players.Data[i], StaticClass(va::ACrabPS_StaticClass))) more += self.Held(players.Data[i])[MoreEnemies];
        return r + std::min(more, kMaxMoreEnemies);
    }
    float Tougher(void* ps) { return Active() && ps ? std::min(Held(ps)[TougherEnemies], kMaxTougher) : 0.f; }
    static bool HkHasPerk(void* ps, uint8_t type) {
        bool r = oHasPerk(ps, type);
        if (!r && reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == retHasPerk_)
            r = g_self->Tougher(ps) > 0.f;
        return r;
    }
    static float HkPerkDebuff(void* ps, uint8_t type) {
        float r = oPerkDebuff(ps, type);
        if (reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == retPerkDebuff_) r += g_self->Tougher(ps);
        return r;
    }

    // Host: the party is going through a portal. A new run is a Greed Run when it is the Greed Run portal; a
    // continued run is what it was saved as.
    static void HkEnterPortal(void* gm) {
        GreedRun& self = *g_self;
        void* portal = gm ? At<void*>(gm, off::GM_ChosenPortal) : nullptr;
        uint8_t type = portal ? At<uint8_t>(portal, off::Portal_IslandType) : 0;
        if (self.enabled_ && type == uint8_t(IslandType::NewRun)) {
            bool greed = IsGreedPortal(portal);
            self.SetActive(greed, greed ? "the party took the Greed Run portal" : "a normal run");
            Save(greed);
        } else if (self.enabled_ && type == uint8_t(IslandType::ContinueRun)) {
            self.SetActive(Load(), "continuing the saved run");
        }
        oEnterPortal(gm);
    }

    static std::wstring StatePath() { return ModuleDir() + L"GreedRun.state"; }
    static void Save(bool greed) {
        if (FILE* f = _wfopen(StatePath().c_str(), L"wb")) {
            fputc(greed ? '1' : '0', f);
            fclose(f);
        }
    }
    static bool Load() {
        FILE* f = _wfopen(StatePath().c_str(), L"rb");
        if (!f) return false;
        bool greed = fgetc(f) == '1';
        fclose(f);
        return greed;
    }

    struct PairHash {
        size_t operator()(const std::pair<void*, int>& p) const {
            return std::hash<void*>()(p.first) ^ size_t(p.second);
        }
    };

    bool enabled_ = true;
    bool active_ = false;
    int extraLevels_ = 1;
    int perkExtraLevels_ = 2;
    float perkScale_ = 1.f;
    float modScale_ = 1.f;
    int extraChoices_ = 2;
    Vec3 portal_{-5757.f, 5000.f, 1000.f};

    void* world_ = nullptr;
    void* lm_ = nullptr;
    void* markedLm_ = nullptr;
    void* spawnedPortal_ = nullptr;
    uint8_t lastIsland_ = 0xFF;
    bool spawnPending_ = false;
    std::unordered_set<void*> told_;
    std::unordered_set<void*> leveled_;
    std::vector<void*> lowered_;
    std::unordered_map<void*, Spec> specs_;
    std::unordered_map<std::pair<void*, int>, std::wstring, PairHash> original_;
};

GreedRun g_item;

}  // namespace

bool GreedRunActive() { return g_self && g_self->Active(); }
void GreedRunCard(void* pickup, bool before) {
    if (g_self) g_self->Card(pickup, before);
}

}  // namespace crabmods
