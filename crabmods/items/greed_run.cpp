// Greed Run, a game mode: a portal in the lobby, opposite the Arcade and Holdout portals, starts a normal run where
// everything is Greed (plans/greed-mode.md).
// - Host, lobby: after the lobby's own portals, spawns a New Run portal (SpawnablesDA.NewRunPortal) with the Greed
//   reward in its PortalInfo. That replicated reward recolours it and gives it the Greed icon on every machine, and
//   tells CrabMods on each machine that this New Run portal is the Greed Run one (its card is renamed).
// - Host: when the party takes it (ACrabGM_EnterChosenPortal), the mode is on until the next lobby. A Continue Run
//   portal takes the mode the run was saved with (GreedRun.state next to the DLL). The host tells every player.
// - Host, the run: every island reward chest is a Greed chest, and a Greed chest's roll offers every item (any loot
//   pool, the usual rarity odds, the game's own Greed items too) with extraChoices more choices.
// - Every item is its Greed version: each pickup that appears (chests, totems, the shop) is extraLevels higher, and
//   each level of a non-Greed item you hold carries a curse picked by the item's loot pool. Every machine shows it:
//   the card says "Greed" in the Greed colour, and the item's text gets its curse.
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

// What each item's levels cost, by its loot pool.
enum Curse { MaxHealth, DamageTaken, FireRate, Damage, Crystals, Cooldown, OtherMaxHealth, kCurses };

Curse CurseOf(uint8_t lootPool) {
    switch (lootPool) {
    case pool::Damage: return MaxHealth;
    case pool::Critical: case pool::Speed: return DamageTaken;
    case pool::Elemental: return FireRate;
    case pool::Luck: case pool::Health: return Damage;
    case pool::Economy: return Crystals;
    case pool::Skill: return Cooldown;
    }
    return OtherMaxHealth;
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
# Greed. Every chest is a Greed chest that can offer any item, every item comes extraLevels higher, and each level of
# an item you hold carries a curse picked by its kind. You can always leave a chest's items on the ground.
enabled = true

# Levels added to every mod and perk that appears (relics stay level 1).
extraLevels = 1
# Choices added to every Greed chest (it has 1 of its own).
extraChoices = 2

# Curses for each level held, in percent (crystals: crystals lost per hit). By the item's kind:
maxHealthPerLevel = 4       # Damage items: less max health
damageTakenPerLevel = 4     # Critical and Speed items: more damage taken
fireRatePerLevel = 3        # Elemental items: slower fire rate
damagePerLevel = 3          # Luck and Health items: less damage
crystalsPerHit = 5          # Economy items: crystals lost each time you're hit
cooldownPerLevel = 5        # Skill items: longer ability cooldown
otherMaxHealthPerLevel = 3  # relics and everything else: less max health

# Where the portal stands in the lobby (the mirror of the Arcade and Holdout portals).
portalX = -5757
portalY = 5000
portalZ = 1000
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "extraLevels") extraLevels_ = std::clamp(int(ParseFloat(v)), 0, 10);
        else if (k == "extraChoices") extraChoices_ = std::clamp(int(ParseFloat(v)), 0, 10);
        else if (k == "maxHealthPerLevel") per_[MaxHealth] = Pct(v);
        else if (k == "damageTakenPerLevel") per_[DamageTaken] = Pct(v);
        else if (k == "fireRatePerLevel") per_[FireRate] = Pct(v);
        else if (k == "damagePerLevel") per_[Damage] = Pct(v);
        else if (k == "crystalsPerHit") per_[Crystals] = std::max(ParseFloat(v), 0.f);
        else if (k == "cooldownPerLevel") per_[Cooldown] = Pct(v);
        else if (k == "otherMaxHealthPerLevel") per_[OtherMaxHealth] = Pct(v);
        else if (k == "portalX") portal_.x = ParseFloat(v);
        else if (k == "portalY") portal_.y = ParseFloat(v);
        else if (k == "portalZ") portal_.z = ParseFloat(v);
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override {
        return "+" + std::to_string(extraLevels_) + " levels, +" + std::to_string(extraChoices_) +
               " choices, curses per level";
    }
    std::string Title() const override { return "Greed Run"; }
    const char* CatalogKind() const override { return "Game mode"; }

    bool Active() const { return enabled_ && active_; }

    void InstallHooks() override {
        Hook(va::ACrabGM_EnterChosenPortal, &HkEnterPortal, &oEnterPortal, "ACrabGM::EnterChosenPortal");
        Hook(va::ACrabPortal_InitUI, &HkInitUI, &oInitUI, "ACrabPortal::InitUI");
        Hook(va::ACrabPortal_UpdateUI, &HkUpdateUI, &oUpdateUI, "ACrabPortal::UpdateUI");
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
            level = uint8_t(std::min(int(level) + extraLevels_, 255));
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

    float MaxHealthMultiplierBonus(void* ps) override {
        if (!Active()) return 0.f;
        CurseLevels l = Held(ps);
        return std::max(-(l[MaxHealth] * per_[MaxHealth] + l[OtherMaxHealth] * per_[OtherMaxHealth]), -kFloor);
    }
    float DamageBonus(void* ps) override {
        if (!Active()) return 0.f;
        return std::max(-Held(ps)[Damage] * per_[Damage], -kFloor);
    }
    float FireRateBonus(void* ps) override {
        if (!Active()) return 0.f;
        return std::max(-Held(ps)[FireRate] * per_[FireRate], -kFloor);
    }
    void OnTakeDamage(void* hc, void* info) override {
        if (!Active()) return;
        void* ps = PlayerOfHealth(hc);
        float& damage = At<float>(info, off::DamageInfo_Damage);
        if (!ps || damage <= 0.f) return;
        CurseLevels l = Held(ps);
        damage *= 1.f + l[DamageTaken] * per_[DamageTaken];
        int lose = std::min(int(l[Crystals] * per_[Crystals]), At<int>(ps, off::PS_Crystals));
        if (lose > 0) Fn<void (*)(void*, int)>(va::ACrabPS_AddCrystals)(ps, -lose);
    }
    // The cooldown counts down 0.25 s per tick; giving part of it back makes it last 1 + k times as long.
    void OnCooldownTick(void* c, void* ps) override {
        if (!Active()) return;
        float k = Held(ps)[Cooldown] * per_[Cooldown];
        float& cd = At<float>(c, off::PlayerC_AbilityCooldown);
        if (k > 0.f && cd > 0.f) cd += 0.25f * k / (1.f + k);
    }
    // Fire rate is worked out when weapons change: work it out again when the Elemental levels do.
    void OnFrame(void* world, float) override {
        if (!enabled_) return;
        ForEachPlayer(world, [&](void* ps) {
            float f = Active() ? Held(ps)[FireRate] : 0.f;
            auto [it, fresh] = fireRate_.try_emplace(ps, f);
            if (fresh || it->second == f) return;
            it->second = f;
            RefreshFireRate(ps);
        });
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!Active() || !da || At<uint8_t>(da, off::DA_Rarity) == rarity::Greed) return {};
        int level = 0;
        for (Kind k : kKinds) level += LevelOf(ps, k, da);
        if (!level) return {};
        Curse c = CurseOf(At<uint8_t>(da, off::DA_LootPool));
        return "Greed curse: " + CurseText(c, level * per_[c]);
    }

private:
    static constexpr float kFloor = 0.9f;  // curses never take more than 90% of max health, damage or fire rate
    static constexpr Kind kKinds[] = {Kind::WeaponMod, Kind::AbilityMod, Kind::Perk, Kind::Relic, Kind::MeleeMod};

    struct CurseLevels {
        float n[kCurses] = {};
        float operator[](Curse c) const { return n[c]; }
    };

    static float Pct(const std::string& v) { return std::max(ParseFloat(v), 0.f) / 100.f; }
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

    // The player's levels of non-Greed items, by curse.
    CurseLevels Held(void* ps) const {
        CurseLevels l;
        if (!ps) return l;
        for (Kind k : kKinds)
            ForEachItem(ps, Info(k).playerArray, [&](void* da, int level) {
                if (At<uint8_t>(da, off::DA_Rarity) != rarity::Greed) l.n[CurseOf(At<uint8_t>(da, off::DA_LootPool))] += level;
            });
        return l;
    }

    std::string CurseText(Curse c, float amount) const {
        std::string pct = Num(AssetItem::Round(amount * 100.f)) + "%";
        switch (c) {
        case MaxHealth: case OtherMaxHealth: return "-" + pct + " max health";
        case DamageTaken: return "+" + pct + " damage taken";
        case FireRate: return "-" + pct + " fire rate";
        case Damage: return "-" + pct + " damage";
        case Crystals: return "lose " + Num(amount) + " crystals when hit";
        case Cooldown: return "+" + pct + " ability cooldown";
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
            Curse c = CurseOf(At<uint8_t>(da, off::DA_LootPool));
            std::string t = CurseText(c, per_[c]);
            std::wstring suffix = L"\nGreed: " + std::wstring(t.begin(), t.end()) + L" per level";
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
        wchar_t text[] = L"A new run where everything is Greed: every chest, every item. Every item comes a level "
                         L"higher, and every level you hold has a curse";
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
    int extraChoices_ = 2;
    float per_[kCurses] = {0.04f, 0.04f, 0.03f, 0.03f, 5.f, 0.05f, 0.03f};
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
    std::unordered_map<void*, float> fireRate_;
    std::unordered_map<std::pair<void*, int>, std::wstring, PairHash> original_;
};

GreedRun g_item;

}  // namespace

bool GreedRunActive() { return g_self && g_self->Active(); }
void GreedRunCard(void* pickup, bool before) {
    if (g_self) g_self->Card(pickup, before);
}

}  // namespace crabmods
