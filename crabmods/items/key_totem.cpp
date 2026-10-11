// Key totem: the lobby's key totem lets you choose what to unlock. Instead of one random locked item, a row of
// locked items comes out of the totem; the one you take is unlocked (and is yours to use, as the game's one is).
// Once everything is unlocked it offers a choice of any weapon mods, ability mods, melee mods, perks and relics.
//
// The game picks the item on the player's own machine (from their save) and asks the server to spawn it, so:
//   player's machine  ClientOnInteractedWithKeyTotem: sends several items to spawn, then a null to end the list
//   host              ServerSpawnKeyTotemPickup: spawns each one in a row that only that player can take
//   host              a pickup taken: the rest of the row goes away; tells the player's machine which one
//   player's machine  that message: unlocks it in the save
// It needs CrabMods on the host and on the player's machine; otherwise the game's own totem is used.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kKeyTotem = 3;  // ECrabTotemType
constexpr int kMaxChoices = 8;
constexpr wchar_t kPickedTag[] = L"keyTotem picked ";

class KeyTotem;
KeyTotem* g_self;

class KeyTotem : public Item {
public:
    KeyTotem() : Item("keyTotem", 190) { g_self = this; }

    std::string DefaultIni() const override {
        return R"(# Key totem: the lobby's key totem lets you choose what it unlocks. A row of locked items comes out and
# the one you take is unlocked. Once everything is unlocked, it offers any weapon mods, ability mods, melee
# mods, perks and relics to choose from. Needs CrabMods on the host and on your machine.
enabled = true

# How many items to choose from (1 to 8).
choices = 3
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "choices") choices_ = std::clamp(int(ParseFloat(v)), 1, kMaxChoices);
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override { return "choose from " + std::to_string(choices_) + " items"; }

    void InstallHooks() override {
        Hook(va::ACrabTotem_Interact, &HkTotemInteract, &oTotemInteract, "ACrabTotem::Interact");
        Hook(va::ACrabPC_ClientOnInteractedWithKeyTotem_Impl, &HkInteracted, &oInteracted,
             "ACrabPC::ClientOnInteractedWithKeyTotem");
        Hook(va::ACrabInteractPickup_Interact, &HkPickupInteract, &oPickupInteract, "ACrabInteractPickup::Interact");
        // ServerSpawnKeyTotemPickup's implementation is a few bytes: swap its virtual table slot instead.
        auto** slot = reinterpret_cast<SpawnFn*>(Addr(va::ACrabPC_VTable + va::ACrabPC_ServerSpawnKeyTotemPickup_Slot));
        DWORD old;
        if (VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) {
            oSpawn = *slot;
            *slot = &HkSpawn;
            VirtualProtect(slot, sizeof *slot, old, &old);
        }
        CRAB_LOG("hook ACrabPC::ServerSpawnKeyTotemPickup (virtual table): %s", oSpawn ? "MH_OK" : "failed");
    }

    // Player's machine: the host said which of the row was taken.
    bool OnClientMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, std::size(kPickedTag) - 1, kPickedTag) != 0) return false;
        size_t i = wcstoul(text.c_str() + std::size(kPickedTag) - 1, nullptr, 10);
        if (i < offered_.size() && unlocking_) {
            void* da = offered_[i];
            void* gi = GameInstanceOf(pc);
            if (gi && !Unlocked(gi, da)) {
                Fn<void (*)(void*, void*)>(va::UCrabGI_UnlockPickup)(gi, da);
                CRAB_LOG("keyTotem: unlocked %s", Name(da).c_str());
            }
        }
        offered_.clear();
        return true;
    }

private:
    using InteractFn = void (*)(void*, void*);
    using InteractedFn = void (*)(void*, void*);
    using SpawnFn = void (*)(void*, void*, void*);
    static inline InteractFn oTotemInteract, oPickupInteract;
    static inline InteractedFn oInteracted;
    static inline SpawnFn oSpawn;

    // --- Player's machine ---

    static void* GameInstanceOf(void* context) {
        void* world = WorldOf(context);
        void* gi = world ? At<void*>(world, off::World_GameInstance) : nullptr;
        return IsA(gi, StaticClass(va::UCrabGI_StaticClass)) ? gi : nullptr;
    }
    static bool Contains(const TArray<void*>& a, void* v) {
        for (int i = 0; i < a.Num; i++)
            if (a.Data[i] == v) return true;
        return false;
    }
    // Whether the save has da unlocked (in any of its lists).
    static bool Unlocked(void* gi, void* da) {
        void* sg = At<void*>(gi, off::GI_SaveGame);
        if (!sg) return true;
        for (int list : off::SG_Unlocked)
            if (Contains(At<TArray<void*>>(sg, list), da)) return true;
        return false;
    }
    static std::string Name(void* da) {
        auto& s = At<TArray<wchar_t>>(da, off::DA_Name);
        std::string out;
        for (int i = 0; s.Data && i < s.Num && s.Data[i]; i++) out.push_back(char(s.Data[i]));
        return out;
    }

    // The game's choice is replaced by a row of choices when the host runs CrabMods; otherwise the game's own.
    static void HkInteracted(void* pc, void* totem) {
        if (!g_self->enabled_ || !HostHasCrabMods(pc) || !g_self->Offer(pc, totem)) oInteracted(pc, totem);
    }

    bool Offer(void* pc, void* totem) {
        void* lm = LevelManagerOf(pc);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        void* gi = GameInstanceOf(pc);
        void* sg = gi ? At<void*>(gi, off::GI_SaveGame) : nullptr;
        if (!sp || !sg) return false;
        std::vector<void*> locked, any;
        for (int k = 0; k < 8; k++) {
            auto& all = At<TArray<void*>>(sp, off::Spawnables_Unlockables[k]);
            auto& mine = At<TArray<void*>>(sg, off::SG_Unlocked[k]);
            for (int i = 0; i < all.Num; i++) {
                void* da = all.Data[i];
                if (!da) continue;
                if (!Contains(mine, da)) locked.push_back(da);
                else if (k >= 3) any.push_back(da);  // weapon mods, ability mods, melee mods, perks, relics
            }
        }
        unlocking_ = !locked.empty();
        std::vector<void*>& from = unlocking_ ? locked : any;
        if (from.empty()) return false;
        std::shuffle(from.begin(), from.end(), rng_);
        from.resize(std::min<size_t>(from.size(), choices_));
        offered_ = from;
        auto spawn = Fn<void (*)(void*, void*, void*)>(va::ACrabPC_ServerSpawnKeyTotemPickup);
        std::string names;
        for (void* da : offered_) {
            spawn(pc, totem, da);
            names += (names.empty() ? "" : ", ") + Name(da);
        }
        spawn(pc, totem, nullptr);  // the end of the list
        CRAB_LOG("keyTotem: offered %s%s", names.c_str(), unlocking_ ? "" : " (everything is unlocked)");
        return true;
    }

    // --- Host ---

    struct Row {
        void* totem = nullptr;
        void* pc = nullptr;
        int spawned = 0;
        bool listed = false;  // the player's CrabMods sent the end of the list: it wants to know the pick
        std::vector<void*> pickups;
    };

    // Only a player who just used the key totem may have items spawned from it; the game itself doesn't check.
    static void HkTotemInteract(void* totem, void* c) {
        if (!totem || !c || At<uint8_t>(totem, off::Totem_Type) != kKeyTotem) return oTotemInteract(totem, c);
        void* pc = At<void*>(c, off::Pawn_Controller);
        int before = At<TArray<void*>>(totem, off::Totem_UniqueIDInteractions).Num;
        g_self->Forget(LevelManagerOf(totem));
        // Set first: for the host's own player the row is asked for inside the call.
        if (pc) g_self->rows_[pc] = Row{totem, pc, 0, false, {}};
        oTotemInteract(totem, c);
        if (pc && At<TArray<void*>>(totem, off::Totem_UniqueIDInteractions).Num == before) {
            auto it = g_self->rows_.find(pc);
            if (it != g_self->rows_.end() && it->second.pickups.empty()) g_self->rows_.erase(it);
        }
    }

    static void HkSpawn(void* pc, void* totem, void* da) {
        if (!g_self->enabled_ || !pc || !totem) return oSpawn(pc, totem, da);
        auto it = g_self->rows_.find(pc);
        if (it == g_self->rows_.end() || it->second.totem != totem) {
            CRAB_LOG("keyTotem: ignored a key totem pickup that player %p didn't pay for", pc);
            return;
        }
        Row& row = it->second;
        if (!da) {
            row.listed = true;
            return;
        }
        if (row.listed || row.spawned >= g_self->choices_) return;
        if (void* p = g_self->SpawnAt(row, da)) row.pickups.push_back(p);
        row.spawned++;
    }

    // The game's spot 350 units in front of the totem, then alternately right and left of it.
    void* SpawnAt(Row& row, void* da) {
        void* lm = LevelManagerOf(row.totem);
        void* root = At<void*>(row.totem, off::Actor_RootComponent);
        void* c = At<void*>(row.pc, off::Controller_Pawn);
        void* ps = c ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
        if (!lm || !root) return nullptr;
        Vec3 at = At<Vec3>(root, off::SceneComponent_Location);
        const float* q = &At<float>(root, off::SceneComponent_Rotation);  // x, y, z, w
        Vec3 fwd{1 - 2 * (q[1] * q[1] + q[2] * q[2]), 2 * (q[0] * q[1] + q[3] * q[2]), 0};
        float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y);
        if (len < 1e-3f) fwd = {1, 0, 0}, len = 1;
        fwd.x /= len, fwd.y /= len;
        int k = row.spawned;
        float side = (k + 1) / 2 * (k % 2 ? 200.f : -200.f);
        Vec3 loc{at.x + fwd.x * 350 - fwd.y * side, at.y + fwd.y * 350 + fwd.x * side, at.z};
        alignas(8) uint8_t info[off::PickupInfo_Size];
        std::memset(info, 0, sizeof info);
        At<void*>(info, off::PickupInfo_DA) = da;
        At<uint8_t>(info, off::PickupInfo_Level) = 1;
        At<uint8_t>(info, off::PickupInfo_MaxPickups) = 1;
        At<int32_t>(info, off::PickupInfo_SalvageReward) = -1;
        At<void*>(info, off::PickupInfo_OwningPS) = ps;
        At<Vec3>(info, off::PickupInfo_CosmeticSpawnLoc) = Vec3{at.x, at.y, at.z + 500};
        return Fn<void* (*)(void*, const void*, Vec3*)>(va::ACrabLM_SpawnPickup)(lm, info, &loc);
    }

    static void HkPickupInteract(void* pickup, void* c) {
        uint8_t before = pickup ? At<uint8_t>(pickup, off::Pickup_NumTimesPickedUp) : 0;
        oPickupInteract(pickup, c);
        if (!pickup || At<uint8_t>(pickup, off::Pickup_NumTimesPickedUp) == before) return;
        for (auto it = g_self->rows_.begin(); it != g_self->rows_.end(); ++it) {
            Row& row = it->second;
            auto p = std::find(row.pickups.begin(), row.pickups.end(), pickup);
            if (p == row.pickups.end()) continue;
            for (void* other : row.pickups)
                if (other != pickup) Fn<void (*)(void*)>(va::ACrabInteractPickup_Deactivate)(other);
            if (row.listed) SendToPlayer(row.pc, kPickedTag + std::to_wstring(p - row.pickups.begin()));
            g_self->rows_.erase(it);
            return;
        }
    }

    // A new level: the rows and their pickups are gone.
    void Forget(void* lm) {
        if (lm == lm_) return;
        lm_ = lm;
        rows_.clear();
    }

    bool enabled_ = true;
    int choices_ = 3;
    std::mt19937 rng_{std::random_device{}()};
    // Player's machine: what was offered, in order, and whether taking one unlocks it.
    std::vector<void*> offered_;
    bool unlocking_ = false;
    // Host: each player's row from the key totem they just used.
    void* lm_ = nullptr;
    std::unordered_map<void*, Row> rows_;
};

KeyTotem g_item;

}  // namespace
}  // namespace crabmods
