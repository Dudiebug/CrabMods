// Every CrabMods item is an Item: one [section] of CrabMods.ini plus whichever game events it reacts to. Items
// register themselves (a static instance in crabmods/items/<name>.cpp) and main.cpp calls them from its hooks.
//
// AssetItem is the common case: a new pickup, made at runtime as a copy of one of the game's own assets with
// new text, rarity and an unused type byte, and added to the loot lists so the game rolls it like any other.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <crab/log.hpp>
#include <crab/ue.hpp>

#include "items.hpp"

namespace crabmods {

using namespace crab;

class Item;
inline std::vector<Item*>& Items() {
    static std::vector<Item*> items;
    return items;
}

class Item {
public:
    // section: the ini section and the name used in the log. order: position in the ini and the log.
    Item(const char* section, int order) : section_(section), order_(order) { Items().push_back(this); }
    virtual ~Item() = default;

    const char* Section() const { return section_; }
    int Order() const { return order_; }

    // --- Settings ---
    // The body of the item's [section] in a new CrabMods.ini: '#' comment lines and "key = value" lines with
    // the defaults, ending in a newline. No section header.
    virtual std::string DefaultIni() const = 0;
    // One key from the item's section. May throw on a bad number (the caller ignores that key).
    virtual void Set(const std::string& key, const std::string& value) {}
    // false: switched off in the ini, so none of the events below are called.
    virtual bool Enabled() const { return true; }
    // The settings as one log line, without the item name ("+4% per empty slot, epic").
    virtual std::string Describe() const = 0;

    // --- Events (game thread) ---
    // Every level start (ACrabLM::BeginPlay), on every machine.
    virtual void OnBeginPlay(void* lm) {}
    // Extra damage for this player as a fraction (0.05 = +5%). It is added to the player's DamageMultiplier,
    // alongside the game's own damage perks. Called on whichever machine computes the player's damage.
    virtual float DamageBonus(void* ps) { return 0.f; }
    // A character is about to take damage (UCrabHC::TakeDamage, on the host). hc: UCrabHC*, info:
    // FCrabDamageInfo* (off::DamageInfo_*). Changes to info->Damage apply.
    virtual void OnTakeDamage(void* hc, void* info) {}
    // A player's hit, after the game applied their damage buffs, crits (DamageInfo_Tags gets 1) and elemental
    // perks to it (host, inside TakeDamage, after OnTakeDamage). Changes to info's damage and debuffs apply.
    virtual void OnOutgoingDamage(void* ps, void* info) {}
    // Loot rolls (host): how much more likely da is to be offered to this player, as a factor on its
    // SpawnWeight. 1 = unchanged, 0 = never. The factors of all items are multiplied.
    virtual float LootWeight(void* ps, void* da) { return 1.f; }
    // Loot rolls (host): lets da also be offered by chests of another loot pool (a regular chest: Damage to
    // Skill) when rolling for this player. 0 = not added; otherwise its SpawnWeight factor there, used instead
    // of this item's LootWeight (the largest wins when several items add it).
    virtual float OtherChestWeight(void* ps, void* da) { return 0.f; }
    // Island-end rewards (host): the chests about to spawn for this player (chest classes), after the game's
    // bonus chests, Care Package and totems and before High Roller. Items may add or remove entries (Append).
    virtual void OnRewardChests(void* ps, TArray<void*>& chests) {}
    // A chest's loot (host): what it is about to spawn for the player who opened it, right after the roll.
    // pickups: TArray<FCrabPickupInfo> (0x50 each: the DA at +0, its level, a uint8, at +8). Items may change
    // the entries.
    virtual void OnChestLoot(void* chest, void* ps, TArray<uint8_t>& pickups) {}

    // Portal rewards (host): how much more likely an island is to reward this loot pool (pool::*). The game
    // picks Damage, Critical, ... for each Arena/Horde/Demolition portal; 1 = unchanged.
    virtual float PortalRewardWeight(void* lm, uint8_t lootPool) { return 1.f; }
    // A player's ability cooldown is about to count down by 0.25 s (every 0.25 s while one runs, on the
    // owner's machine and possibly the host). c: ACrabPlayerC*, ps: its ACrabPS*. Lowering
    // off::PlayerC_AbilityCooldown speeds it up; keep it above 0 (0.01 at least) so the tick finishes it and
    // gives the charge back.
    virtual void OnCooldownTick(void* c, void* ps) {}
    // Extra max health multiplier for this player (0.03 = +3%), added beside Banana's when the game recomputes
    // max health (ACrabPS::UpdateMaxHealth: on the host, whenever the player's items change).
    virtual float MaxHealthMultiplierBonus(void* ps) { return 0.f; }
    // Extra fire rate for this player's weapons (0.12 = one Rapid Fire level), added beside Rapid Fire's when
    // the game recomputes a weapon's fire rate (on every machine). Call RefreshFireRate(ps) when it changes.
    virtual float FireRateBonus(void* ps) { return 0.f; }
    // Weapon fire, on the machine that controls the player (its shots deal the damage; the others see copies).
    // true: this player's next shot falls from above the aim point, the way Ice Staff fires.
    virtual bool ShotsFromAbove(void* ps) { return false; }
    // One projectile of a player's shot is about to appear at start, flying along dir; items may change both.
    // weapon: ACrabWeapon*.
    virtual void OnShot(void* weapon, void* ps, Vec3& start, Vec3& dir) {}
    // The player went down (host). c is their character, still in the world.
    virtual void OnDown(void* c, void* ps) {}
    // The player broke a destructible (pot, barrel, rock, crystal, nest, ...) during an island fight (host).
    virtual void OnDestructibleBroken(void* ps) {}
    // A player was healed past their max health by this much (on the host): the part of a heal that was lost.
    virtual void OnOverheal(void* ps, float amount) {}
    // A message from the host's CrabMods (SendToPlayer), on the player's machine. true: it was for this item.
    virtual bool OnClientMessage(void* pc, const std::wstring& text) { return false; }
    // Once at start, for items with hooks of their own (crab/hook.hpp), whether enabled or not.
    virtual void InstallHooks() {}
    // Four times a second on the game thread, on every machine. world: the game world (a UWorld).
    virtual void OnTick(void* world) {}

private:
    const char* section_;
    int order_;
};

// In main.cpp. true: this machine got the host's CrabMods settings through this player controller, so the host
// runs CrabMods.
bool HostHasCrabMods(void* pc);
// In main.cpp (host): sends text to the player's machine, where CrabMods passes it to OnClientMessage.
void SendToPlayer(void* pc, const std::wstring& text);

inline Item* FindItem(const std::string& section) {
    for (Item* i : Items())
        if (section == i->Section()) return i;
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------
// Settings parsing

inline bool ParseBool(const std::string& v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }
inline float ParseFloat(const std::string& v) { return std::stof(v); }
inline uint8_t ParseRarity(const std::string& v, uint8_t def) {
    if (v == "common" || v == "1") return rarity::Common;
    if (v == "epic" || v == "2") return rarity::Epic;
    if (v == "legendary" || v == "3") return rarity::Legendary;
    if (v == "greed" || v == "4") return rarity::Greed;
    return def;
}
inline const char* RarityName(uint8_t r) {
    switch (r) {
    case rarity::Common: return "common";
    case rarity::Epic: return "epic";
    case rarity::Legendary: return "legendary";
    case rarity::Greed: return "greed";
    }
    return "?";
}
// "3" for 3.0, "2.5" for 2.5: numbers in log lines and descriptions.
inline std::string Num(float v) {
    char b[32];
    snprintf(b, sizeof b, "%g", double(v));
    return b;
}

// ---------------------------------------------------------------------------------------------------------
// Inventory

enum class Kind : uint8_t { WeaponMod, AbilityMod, Perk, Relic, MeleeMod };

struct KindInfo {
    uintptr_t staticClass;   // va:: StaticClass getter
    int spawnablesList;      // off::Spawnables_*: the game's full list of this kind
    int playerArray;         // off::PS_*: TArray<FCrab...> on ACrabPS, 0x28 per entry, DA at +0, level at +8
    const char* iconPrefix;  // T_Icon_<Kind>_<Rarity> (ability mods still use the grenade mod icons)
};

inline const KindInfo& Info(Kind k) {
    static const KindInfo info[] = {
        {va::UCrabWeaponModDA_StaticClass, off::Spawnables_WeaponMods, off::PS_WeaponMods, "T_Icon_WeaponMod_"},
        {va::UCrabAbilityModDA_StaticClass, off::Spawnables_AbilityMods, off::PS_AbilityMods, "T_Icon_GrenadeMod_"},
        {va::UCrabPerkDA_StaticClass, off::Spawnables_Perks, off::PS_Perks, "T_Icon_Perk_"},
        {va::UCrabRelicDA_StaticClass, off::Spawnables_Relics, off::PS_Relics, "T_Icon_Relic_"},
        // No generic melee mod icons: the template's own stays.
        {va::UCrabMeleeModDA_StaticClass, off::Spawnables_MeleeMods, off::PS_MeleeMods, "T_Icon_MeleeMod_"},
    };
    return info[int(k)];
}

// Total level of da in the player's inventory of that kind (0 if not held).
inline int LevelOf(void* ps, Kind kind, void* da) {
    if (!ps || !da) return 0;
    int n = 0;
    ForEachItem(ps, Info(kind).playerArray, [&](void* d, int level) {
        if (d == da) n += level;
    });
    return n;
}

// ---------------------------------------------------------------------------------------------------------
// AssetItem

struct AssetSpec {
    Kind kind;
    const wchar_t* objectName;        // L"DA_Perk_Minimalist": must be unique and the same on every machine
    const wchar_t* name;              // shown in game
    const wchar_t* description;       // short text
    const wchar_t* levelDescription;  // long text; "PrimaryBuff" is replaced by the game with BaseBuff
    uint8_t type;                     // the item's type byte (+0xD8): unused by the game (>= 200), unique per kind
    uint8_t templateType;             // the game asset of the same kind to copy (its type byte)
    uint8_t lootPool = 0xFF;          // pool:: value, 0xFF keeps the template's
    uint8_t pickupTag = 0xFF;         // tag:: value, 0xFF keeps the template's
    float spawnWeight = -1.f;         // relative chance in a loot roll, < 0 keeps the template's
};

class AssetItem : public Item {
public:
    AssetItem(const char* section, int order, const AssetSpec& spec, uint8_t defaultRarity)
        : Item(section, order), spec_(spec), rarity_(defaultRarity) {}

    bool Enabled() const override { return enabled_; }
    // Handles "enabled" and "rarity". Subclasses handle their own keys and call this for the rest.
    void Set(const std::string& key, const std::string& value) override {
        if (key == "enabled") enabled_ = ParseBool(value);
        else if (key == "rarity") rarity_ = ParseRarity(value, rarity_);
    }
    // Creates the asset once, refreshes its settings and puts it in this level's loot lists.
    void OnBeginPlay(void* lm) override {
        if (!da_) {
            da_ = Create(lm);
            if (!da_) {
                CRAB_LOG("%s: could not create its asset yet", Section());
                return;
            }
        }
        Refresh(lm);
        AddToLoot(lm);
    }

    void* Asset() const { return da_; }
    Kind GetKind() const { return spec_.kind; }
    // The player's total level of this item, 0 if not held.
    int Level(void* ps) const { return da_ ? LevelOf(ps, spec_.kind, da_) : 0; }
    uint8_t Rarity() const { return rarity_; }

protected:
    // The number the game shows in place of "PrimaryBuff" in the level description (BaseBuff).
    virtual float PrimaryBuff() const { return 0.f; }
    // Called after the asset is made or its settings refreshed, for anything else the item changes.
    virtual void Customize(void* da) {}

    const AssetSpec spec_;
    bool enabled_ = true;
    uint8_t rarity_;
    void* da_ = nullptr;

private:
    void* Create(void* lm) {
        const KindInfo& k = Info(spec_.kind);
        auto* list = Spawnables(lm, k.spawnablesList);
        void* templ = FindByType(list, spec_.templateType);
        if (!templ && list && list->Num) templ = list->Data[0];
        if (!templ) return nullptr;
        void* da = MakeDA(StaticClass(k.staticClass), templ, spec_.objectName);
        if (!da) return nullptr;
        SetFString(da, off::DA_Name, spec_.name);
        SetFString(da, off::DA_Description, spec_.description);
        SetFString(da, off::DA_LevelDescription, spec_.levelDescription);
        At<bool>(da, off::DA_bRequiresUnlock) = false;
        At<bool>(da, off::DA_bRequiresMatchingPickupTag) = false;
        At<bool>(da, off::DA_bHyperbolicBuff) = false;
        At<bool>(da, off::DA_bLimited) = false;
        At<bool>(da, off::DA_bGetBuffAsMultiplier) = true;
        At<uint8_t>(da, off::DA_ItemType) = spec_.type;
        if (spec_.lootPool != 0xFF) At<uint8_t>(da, off::DA_LootPool) = spec_.lootPool;
        if (spec_.pickupTag != 0xFF) At<uint8_t>(da, off::DA_PickupTag) = spec_.pickupTag;
        if (spec_.spawnWeight >= 0.f) At<float>(da, off::DA_SpawnWeight) = spec_.spawnWeight;
        CRAB_LOG("created %s from %s", ObjectName(da).c_str(), ObjectName(templ).c_str());
        return da;
    }

    // Settings can change between levels (the host's settings arrive), so they are applied every time.
    void Refresh(void* lm) {
        At<uint8_t>(da_, off::DA_Rarity) = rarity_;
        At<float>(da_, off::DA_BaseBuff) = PrimaryBuff();
        if (iconRarity_ != rarity_) {
            // The game's generic icon for the kind and rarity, taken from any asset that uses it. Without
            // one (no asset of that rarity uses it) the template's icon stays.
            const KindInfo& k = Info(spec_.kind);
            if (void* icon = GenericIcon(Spawnables(lm, k.spawnablesList), k.iconPrefix, rarity_))
                At<void*>(da_, off::DA_Icon) = icon;
            iconRarity_ = rarity_;
            CRAB_LOG("%s: icon %s", Section(), ObjectName(At<void*>(da_, off::DA_Icon)).c_str());
        }
        Customize(da_);
    }

    // Loot rolls read LM.AllUnlockedPickups. Relic chests read SpawnablesDA.Relics instead, so relics go
    // there too.
    void AddToLoot(void* lm) {
        auto& pool = At<TArray<void*>>(lm, off::LM_AllUnlockedPickups);
        if (!Contains(pool, da_)) Append(pool, da_);
        if (spec_.kind != Kind::Relic) return;
        if (auto* list = Spawnables(lm, off::Spawnables_Relics); list && !Contains(*list, da_)) Append(*list, da_);
    }

    uint8_t iconRarity_ = 0;
};

}  // namespace crabmods
