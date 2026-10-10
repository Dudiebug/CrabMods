// Team-inventory ledger: game-agnostic sync logic, unit-tested in tests/ledger_test.cpp.
//
// The host keeps one team inventory plus, for each player, the state last observed or written there
// (the baseline). Any difference between a player's live state and their baseline is that player's
// own action (pickup, salvage, purchase, upgrade); it is folded into the team once. Every player is
// then driven to the team state. Because the baseline is refreshed after each write, the ledger never
// mistakes its own writes for player actions, so nothing is counted twice.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace crabsync {

enum Cat : int { WeaponMods, AbilityMods, MeleeMods, Perks, Relics, NumCats };
enum Equip : int { Weapon, Ability, Melee, NumEquip };
enum SlotKind : int { WeaponModSlots, AbilityModSlots, MeleeModSlots, PerkSlots, NumSlotKinds };

inline const char* CatName(int c) {
    static const char* n[] = {"weapon mod", "ability mod", "melee mod", "perk", "relic"};
    return n[c];
}
inline const char* EquipName(int e) {
    static const char* n[] = {"weapon", "ability", "melee"};
    return n[e];
}

using ItemId = uintptr_t;  // the item's data asset pointer
using PlayerKey = uintptr_t;

struct Item {
    ItemId id = 0;
    uint8_t level = 0;  // the stack count
    std::vector<uint8_t> enhancements;
    float buff = 0.f;  // AccumulatedBuff
    bool operator==(const Item&) const = default;
};

// Whether the player has a live character whose health can be read. None covers loading, spectating
// and pawns the game has not initialised yet, so they are never mistaken for a player at 0 HP.
enum class Life : uint8_t { None, Alive, Dead };

struct Inventory {
    std::array<ItemId, NumEquip> equip{};
    std::array<uint8_t, NumSlotKinds> slots{};
    uint32_t crystals = 0;
    std::array<std::vector<Item>, NumCats> items;
    Life life = Life::None;
    uintptr_t body = 0;  // the character health was read from; a new character is not a health change
    float health = 0.f;
    float maxHealth = 0.f;
    bool operator==(const Inventory&) const = default;
};

struct Settings {
    std::array<bool, NumEquip> equip{true, true, true};
    std::array<bool, NumCats> cats{true, true, true, true, true};
    bool slots = true;
    bool crystals = true;
    bool enhancements = true;
    bool buffs = true;
    bool health = true;  // one shared health pool; when it reaches 0 everyone dies
};

// One absorbed change, for logging. id is 0 for scalar changes.
struct Change {
    std::string what;
    ItemId id = 0;
};

// Everything but health: whether two reads show the same inventory.
inline bool SameItems(const Inventory& a, const Inventory& b) {
    return a.equip == b.equip && a.slots == b.slots && a.crystals == b.crystals && a.items == b.items;
}

const Item* FindItem(const std::vector<Item>& v, ItemId id);
Item* FindItem(std::vector<Item>& v, ItemId id);

class Ledger {
public:
    explicit Ledger(Settings s = {}) : s_(s) {}

    const Settings& settings() const { return s_; }
    void setSettings(const Settings& s) { s_ = s; }

    bool hasTeam() const { return hasTeam_; }
    const Inventory& team() const { return team_; }

    // A run ended: forget the team and every baseline.
    void reset();
    // A new world loaded: every PlayerState is a new object, so baselines are meaningless.
    void clearBaselines() { base_.clear(); }
    // The player's state was replaced wholesale (autosave restore) or they left.
    void forget(PlayerKey k) { base_.erase(k); }
    // Drop baselines for players not in `present`.
    void retain(const std::vector<PlayerKey>& present);
    bool hasBaseline(PlayerKey k) const { return base_.count(k) != 0; }
    const Inventory* baseline(PlayerKey k) const {
        auto it = base_.find(k);
        return it == base_.end() ? nullptr : &it->second;
    }

    // Start a team from the players' current states: union of items, max slots and crystals, and the
    // health of the least damaged player alive.
    void seed(const std::vector<const Inventory*>& players);

    // Fold the player's changes since their baseline into the team and make `current` the new
    // baseline. No-op (returns false) without a team or a baseline.
    bool absorb(PlayerKey k, const Inventory& current, std::vector<Change>* changes = nullptr);

    // absorb() for everyone at once. A slot or crystal change that shows up identically on every player
    // in the same poll is one game event (the game paying everyone crystals, or resetting slots when a
    // run starts), so it is counted once instead of once per player. changes, if given, gets one list
    // per player. Players marked in hold (still mid-change) count towards spotting a shared event but are
    // not absorbed yet; their change is taken in on a later call.
    void absorbAll(const std::vector<PlayerKey>& keys, const std::vector<Inventory>& now,
                   std::vector<std::vector<Change>>* changes = nullptr, const std::vector<bool>* hold = nullptr);

    void setBaseline(PlayerKey k, const Inventory& inv) { base_[k] = inv; }

    // The state `current` should be driven to: the team for synced parts, `current` otherwise.
    Inventory target(const Inventory& current) const;

private:
    void absorbItem(int cat, const Item* before, const Item* now, std::vector<Change>* changes);
    void absorbHealth(const Inventory& before, const Inventory& now, std::vector<Change>* changes);

    Settings s_;
    bool hasTeam_ = false;
    Inventory team_;
    void updateTeamHealth();

    bool healthValid_ = false;  // the pool is meaningful once someone alive has been seen
    // The pool is stored as damage taken, so it grows when max health grows (a player joins, a max
    // health pickup): team health = max health - damage_.
    float damage_ = 0.f;
    float teamMax_ = 0.f;  // the highest max health of anyone alive
    std::unordered_map<PlayerKey, Inventory> base_;
};

}  // namespace crabsync
