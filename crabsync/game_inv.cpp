#include "game_inv.hpp"

#include <crab/ue.hpp>

using namespace crab;

namespace crabsync {

namespace {

// FCrabInventoryInfo (0x20) and the 0x28-byte FCrab{WeaponMod,AbilityMod,MeleeMod,Perk,Relic}.
struct FInvInfo {
    uint8_t Level;
    uint8_t pad0[7];
    TArray<uint8_t> Enhancements;
    float AccumulatedBuff;
    uint32_t pad1;
};
struct FItem {
    void* DA;
    FInvInfo Info;
};
static_assert(sizeof(FInvInfo) == 0x20 && sizeof(FItem) == 0x28);

struct FCrabPickupInfo {
    void* PickupDA;
    FInvInfo InventoryInfo;
    uint8_t rest[0x28];  // MaxPickups, Cost, OwningPS, CosmeticSpawnLoc, ... (unused by GivePickup)
};
static_assert(sizeof(FCrabPickupInfo) == 0x50);

struct FCrabDamageInfo {
    void* DamagedActor;
    float Damage;
    uint8_t CrabDamageType;  // ECrabDamageType
    uint8_t pad0[3];
    TArray<uint8_t> DamageTags;
    TArray<uint64_t> Debuffs;
    float DamageLoc[3];
    float KnockbackStrength;
    void* DamagingC;
};
static_assert(sizeof(FCrabDamageInfo) == 0x48);
constexpr uint8_t kDamageUnblockable = 12;  // skips armor plates and damage modifiers

constexpr int kArrayOff[NumCats] = {off::PS_WeaponMods, off::PS_AbilityMods, off::PS_MeleeMods, off::PS_Perks,
                                    off::PS_Relics};
constexpr int kSlotOff[NumSlotKinds] = {off::PS_NumWeaponModSlots, off::PS_NumAbilityModSlots,
                                        off::PS_NumMeleeModSlots, off::PS_NumPerkSlots};
constexpr int kEquipOff[NumEquip] = {off::PS_WeaponDA, off::PS_AbilityDA, off::PS_MeleeDA};
constexpr uintptr_t kRemove[NumCats] = {va::ACrabPS_RemoveWeaponMod, va::ACrabPS_RemoveAbilityMod,
                                        va::ACrabPS_RemoveMeleeMod, va::ACrabPS_RemovePerk, va::ACrabPS_RemoveRelic};

TArray<FItem>& Items(void* ps, int c) { return At<TArray<FItem>>(ps, kArrayOff[c]); }

FItem* FindLive(void* ps, int c, ItemId id) {
    auto& a = Items(ps, c);
    for (int i = 0; i < a.Num; i++)
        if (reinterpret_cast<ItemId>(a.Data[i].DA) == id) return &a.Data[i];
    return nullptr;
}

// Source info pointing at our own buffer; the game's copy-constructor allocates its own copy.
FInvInfo MakeInfo(uint8_t level, const std::vector<uint8_t>& enh, float buff) {
    FInvInfo i{};
    i.Level = level;
    i.Enhancements = {const_cast<uint8_t*>(enh.data()), int32_t(enh.size()), int32_t(enh.size())};
    i.AccumulatedBuff = buff;
    return i;
}

void* PlayerCharacter(void* ps) {
    static void* cls = StaticClass(va::ACrabPlayerC_StaticClass);
    void* pawn = At<void*>(ps, off::PS_Pawn);
    return IsA(pawn, cls) ? pawn : nullptr;
}

// The character's health component, if the PlayerState has an initialised character.
void* HealthComp(void* ps, void** pawnOut) {
    static void* cls = StaticClass(va::ACrabC_StaticClass);
    void* pawn = At<void*>(ps, off::PS_Pawn);
    if (!IsA(pawn, cls)) return nullptr;
    void* hc = At<void*>(pawn, off::C_HC);
    if (!hc || At<void*>(hc, off::HC_OwningC) != pawn || At<float>(hc, off::HC_CurrentMaxHealth) <= 0.f) return nullptr;
    if (pawnOut) *pawnOut = pawn;
    return hc;
}

// Kills the character through the game's own damage path so elimination, spectating and game over
// run exactly as for a real death. Health is first dropped to 1 so one-shot protection (which only
// saves characters above 85% health) does not apply, and death protection is marked as used: the
// shared pool is empty, so everyone dies.
void Kill(void* pawn, void* hc) {
    At<float>(hc, off::HC_CurrentHealth) = 1.f;
    Fn<void (*)(void*)>(va::UCrabHC_OnRep_HealthInfo)(hc);
    At<uint8_t>(hc, off::HC_bDeathProtectionUsed) = 1;
    FCrabDamageInfo d{};
    d.DamagedActor = pawn;
    d.Damage = 1e9f;
    d.CrabDamageType = kDamageUnblockable;
    Fn<void (*)(void*, FCrabDamageInfo*)>(va::UCrabHC_TakeDamage)(hc, &d);
}

void GivePickup(void* pc, void* da, const FInvInfo& info) {
    FCrabPickupInfo p{};
    p.PickupDA = da;
    p.InventoryInfo = info;
    Fn<void (*)(void*, FCrabPickupInfo*)>(va::ACrabPlayerC_GivePickup)(pc, &p);
}

void AppendSilent(void* ps, int c, const Item& it) {
    auto& a = Items(ps, c);
    int n = a.Num;
    a.Num = n + 1;
    if (a.Max < a.Num) Fn<void (*)(void*)>(va::TArray28_ResizeGrow)(&a);
    FItem* e = &a.Data[n];
    e->DA = reinterpret_cast<void*>(it.id);
    FInvInfo src = MakeInfo(it.level, it.enhancements, it.buff);
    Fn<void* (*)(FInvInfo*, const FInvInfo*)>(va::FCrabInventoryInfo_CopyConstruct)(&e->Info, &src);
}

void SetEnhancements(FItem* e, const std::vector<uint8_t>& enh) {
    if (e->Info.Enhancements.Data) Fn<void (*)(void*)>(va::FMemory_Free)(e->Info.Enhancements.Data);
    FInvInfo src = MakeInfo(e->Info.Level, enh, e->Info.AccumulatedBuff);
    Fn<void* (*)(FInvInfo*, const FInvInfo*)>(va::FCrabInventoryInfo_CopyConstruct)(&e->Info, &src);
}

}  // namespace

void ReadInventory(void* ps, Inventory& out) {
    for (int e = 0; e < NumEquip; e++) out.equip[e] = At<ItemId>(ps, kEquipOff[e]);
    for (int s = 0; s < NumSlotKinds; s++) out.slots[s] = At<uint8_t>(ps, kSlotOff[s]);
    out.crystals = At<uint32_t>(ps, off::PS_Crystals);
    out.life = Life::None;
    out.body = 0;
    out.health = out.maxHealth = 0.f;
    void* pawn = nullptr;
    if (void* hc = HealthComp(ps, &pawn)) {
        bool dead = At<uint8_t>(hc, off::HC_bDead) || (At<uint8_t>(pawn, off::C_bIsEliminated) & 1);
        out.life = dead ? Life::Dead : Life::Alive;
        out.body = reinterpret_cast<uintptr_t>(pawn);
        out.health = dead ? 0.f : At<float>(hc, off::HC_CurrentHealth);
        out.maxHealth = At<float>(hc, off::HC_CurrentMaxHealth);
    }
    for (int c = 0; c < NumCats; c++) {
        auto& a = Items(ps, c);
        auto& v = out.items[c];
        v.clear();
        for (int i = 0; i < a.Num; i++) {
            const FItem& f = a.Data[i];
            if (!f.DA) continue;
            Item it;
            it.id = reinterpret_cast<ItemId>(f.DA);
            it.level = f.Info.Level;
            if (f.Info.Enhancements.Data && f.Info.Enhancements.Num > 0)
                it.enhancements.assign(f.Info.Enhancements.Data, f.Info.Enhancements.Data + f.Info.Enhancements.Num);
            it.buff = f.Info.AccumulatedBuff;
            v.push_back(std::move(it));
        }
    }
}

bool ApplyTarget(void* ps, const Inventory& cur, const Inventory& tgt, bool announce) {
    void* pc = PlayerCharacter(ps);
    bool dirty = false;
    bool done = true;
    int gives = 0;  // GivePickup calls this poll; the rest wait in the queue
    auto canGive = [&] {
        if (gives == 0) return true;
        done = false;
        return false;
    };

    // Equipment swaps spawn the new weapon actor, so they must go through GivePickup; without a
    // pawn (dead, spectating) they are retried on a later tick.
    for (int e = 0; e < NumEquip; e++) {
        if (tgt.equip[e] && tgt.equip[e] != cur.equip[e] && pc && canGive()) {
            GivePickup(pc, reinterpret_cast<void*>(tgt.equip[e]), MakeInfo(1, {}, 0.f));
            gives++;
        }
    }

    if (tgt.crystals != cur.crystals) {
        At<uint32_t>(ps, off::PS_Crystals) = tgt.crystals;
        Fn<void (*)(void*)>(va::ACrabPS_BroadcastCrystals)(ps);
    }

    for (int s = 0; s < NumSlotKinds; s++) {
        if (tgt.slots[s] != cur.slots[s]) {
            At<uint8_t>(ps, kSlotOff[s]) = tgt.slots[s];
            dirty = true;
        }
    }

    for (int c = 0; c < NumCats; c++) {
        if (tgt.items[c] == cur.items[c]) continue;
        for (auto& have : cur.items[c]) {
            if (!FindItem(tgt.items[c], have.id)) {
                uint8_t type = At<uint8_t>(reinterpret_cast<void*>(have.id), off::DA_ItemType);
                Fn<void (*)(void*, uint8_t)>(kRemove[c])(ps, type);
            }
        }
        for (auto& want : tgt.items[c]) {
            // GivePickup can reallocate the array, so live pointers are re-found after each call.
            FItem* live = FindLive(ps, c, want.id);
            if (!live) {
                if (announce && pc) {
                    if (!canGive()) continue;
                    GivePickup(pc, reinterpret_cast<void*>(want.id), MakeInfo(want.level, want.enhancements, want.buff));
                    gives++;
                } else {
                    AppendSilent(ps, c, want);
                    dirty = true;
                }
                continue;
            }
            if (want.level > live->Info.Level && announce && pc) {
                if (!canGive()) continue;
                GivePickup(pc, reinterpret_cast<void*>(want.id), MakeInfo(want.level - live->Info.Level, {}, 0.f));
                gives++;
                live = FindLive(ps, c, want.id);
                if (!live) continue;
            }
            if (live->Info.Level != want.level) {
                live->Info.Level = want.level;
                dirty = true;
            }
            std::vector<uint8_t> enh;
            if (live->Info.Enhancements.Data && live->Info.Enhancements.Num > 0)
                enh.assign(live->Info.Enhancements.Data, live->Info.Enhancements.Data + live->Info.Enhancements.Num);
            if (enh != want.enhancements) {
                SetEnhancements(live, want.enhancements);
                dirty = true;
            }
            if (live->Info.AccumulatedBuff != want.buff) {
                live->Info.AccumulatedBuff = want.buff;
                dirty = true;
            }
        }
    }

    if (dirty) {
        Fn<void (*)(void*)>(va::FMulticastDelegate_Broadcast0)(static_cast<char*>(ps) + off::PS_OnInventoryChanged);
        Fn<void (*)(void*)>(va::ACrabPS_UpdateMaxHealth)(ps);
    }

    // Health last, against the same character it was read from (max health may just have changed).
    void* pawn = nullptr;
    void* hc = HealthComp(ps, &pawn);
    if (hc && cur.life == Life::Alive && reinterpret_cast<uintptr_t>(pawn) == cur.body) {
        if (tgt.life == Life::Dead) {
            Kill(pawn, hc);
        } else if (tgt.health != cur.health) {
            float max = At<float>(hc, off::HC_CurrentMaxHealth);
            At<float>(hc, off::HC_CurrentHealth) = tgt.health < max ? tgt.health : max;
            Fn<void (*)(void*)>(va::UCrabHC_OnRep_HealthInfo)(hc);  // health bar + listeners on the host
        }
    }
    return done;
}

}  // namespace crabsync
