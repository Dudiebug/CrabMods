// Slot items, one legendary per inventory group: Bandolier (weapon mods), Tool Belt (ability mods), Scabbard
// (melee mods) and Backpack (perks). Each level gives that group 5 more slots.
//
// Host: the player's slot count (ACrabPS NumWeaponModSlots, ...) is raised by the bonus. What has been added is
// kept in the unused byte right after the count, so the bonus is never added twice: a new run or a challenge
// sets the count from scratch (the byte goes back to 0), and between islands the count comes back from the
// players' auto save with the bonus in it (the byte is set to what the items give).
//
// Every machine: the inventory menu has one slot widget per slot the game knows of (24 / 12 / 12 / 24). Before
// it lays them out, more are made like the first, and the slots part of the menu is put in a scroll box.
#include <algorithm>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class SlotItem;
std::vector<SlotItem*> g_slotItems;

class SlotItem : public AssetItem {
public:
    SlotItem(const char* section, int order, const AssetSpec& spec, int numSlots, const char* what)
        : AssetItem(section, order, spec, rarity::Legendary), numSlots_(numSlots), what_(what) {
        g_slotItems.push_back(this);
    }

    std::string DefaultIni() const override {
        return "# " + std::string(Narrow(spec_.name)) + ", a legendary " + KindName() + ": more " + what_ +
               " slots.\nenabled = true\n\n# Slots for each level.\nslotsPerLevel = 5\n\n"
               "# Which chests it drops from: common, epic, legendary or greed.\nrarity = legendary\n";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "slotsPerLevel") perLevel_ = std::clamp(int(ParseFloat(v)), 1, 50);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + std::to_string(perLevel_) + " " + what_ + " slots per level, " + RarityName(rarity_);
    }

    void InstallHooks() override {
        static bool done = false;
        if (done) return;
        done = true;
        Hook(va::ACrabPS_RestoreAutoSave, &HkRestore, &oRestore, "ACrabPS::RestoreAutoSave");
        Hook(va::ACrabPS_ResetForNewRun, &HkReset, &oReset, "ACrabPS::ResetForNewRun");
        Hook(va::ACrabPS_ApplyChallengeSlots, &HkChallenge, &oChallenge, "ACrabPS::ApplyChallengeSlots");
        Hook(va::UCrabInventoryUI_Refresh, &HkRefresh, &oRefresh, "UCrabInventoryUI::Refresh");
    }

    // Host: the count follows the items' level.
    void OnTick(void* world) override {
        if (!da_) return;
        ForEachPlayer(world, [&](void* ps) {
            if (At<uint8_t>(ps, off::Actor_Role) != ROLE_Authority) return;
            uint8_t& added = Added(ps);
            int want = Bonus(ps);
            if (added == want) return;
            uint8_t& n = At<uint8_t>(ps, numSlots_);
            int held = At<TArray<uint8_t>>(ps, numSlots_ + 8).Num;  // the items follow the count
            int before = n;
            n = uint8_t(std::clamp(n + want - added, std::max(held, 0), 255));
            added = uint8_t(want);
            Fn<void (*)(void*)>(va::FMulticastDelegate_Broadcast0)(&At<uint8_t>(ps, off::PS_OnInventoryChanged));
            CRAB_LOG("%s: player %p has %d %s slots (was %d)", Section(), ps, int(n), what_, before);
        });
    }

protected:
    float PrimaryBuff() const override { return float(perLevel_); }
    void Customize(void* da) override { At<bool>(da, off::DA_bGetBuffAsMultiplier) = false; }

private:
    const char* KindName() const {
        switch (spec_.kind) {
        case Kind::WeaponMod: return "weapon mod";
        case Kind::AbilityMod: return "ability mod";
        case Kind::MeleeMod: return "melee mod";
        default: return "perk";
        }
    }
    static std::string Narrow(const wchar_t* s) { return ui::Narrow(s); }

    int Bonus(void* ps) const { return enabled_ ? std::min(perLevel_ * Level(ps), 255) : 0; }
    uint8_t& Added(void* ps) const { return At<uint8_t>(ps, numSlots_ + 1); }

    // --- Host: where the count is set from scratch ---

    using PSFn = void (*)(void*);
    using RestoreFn = void (*)(void*, void*);
    static inline PSFn oReset, oChallenge;
    static inline RestoreFn oRestore;

    static void HkRestore(void* ps, void* save) {
        oRestore(ps, save);
        if (ps)
            for (SlotItem* s : g_slotItems) s->Added(ps) = uint8_t(s->Bonus(ps));
    }
    static void HkReset(void* ps) {
        oReset(ps);
        if (ps)
            for (SlotItem* s : g_slotItems) s->Added(ps) = 0;
    }
    // Only in a challenge run with the 8-slot modifier: the counts it changed start over.
    static void HkChallenge(void* ps) {
        if (!ps) return oChallenge(ps);
        uint8_t before[4];
        for (int g = 0; g < 4; g++) before[g] = At<uint8_t>(ps, kNumSlots[g]);
        oChallenge(ps);
        for (SlotItem* s : g_slotItems)
            for (int g = 0; g < 4; g++)
                if (s->numSlots_ == kNumSlots[g] && At<uint8_t>(ps, kNumSlots[g]) != before[g]) s->Added(ps) = 0;
    }

    // --- Every machine: the inventory menu ---

    using RefreshFn = void (*)(void*);
    static inline RefreshFn oRefresh;

    static constexpr int kNumSlots[4] = {off::PS_NumWeaponModSlots, off::PS_NumAbilityModSlots,
                                         off::PS_NumMeleeModSlots, off::PS_NumPerkSlots};

    static void HkRefresh(void* inv) {
        if (inv) Grow(inv);
        oRefresh(inv);
    }

    // Makes slot widgets for slots past the menu's own, laid out in the grid's columns, set up as the menu's
    // init sets up its own (hover, gamepad focus). The game then fills them like the rest.
    static void Grow(void* inv) {
        void* ps = At<void*>(inv, off::InventoryUI_OwningPS);
        if (!ps) return;
        void* pc = nullptr;
        for (int g = 0; g < 4; g++) {
            void* grid = At<void*>(inv, off::InventoryUI_Grids[g]);
            int want = At<uint8_t>(ps, kNumSlots[g]);
            int have = grid ? ui::ChildCount(grid) : 0;
            if (want <= have || have == 0) continue;
            void* first = ui::ChildAt(grid, 0);
            if (!IsA(first, StaticClass(va::UCrabInventorySlotUI_StaticClass))) continue;
            int cols = 1;
            for (int i = 0; i < have; i++)
                if (void* s = ui::SlotOf(ui::ChildAt(grid, i)))
                    cols = std::max(cols, At<int32_t>(s, off::UniformGridSlot_Column) + 1);
            if (!pc) ui::Call(inv, L"GetOwningPlayer", &pc);
            if (!pc) return;
            void* cls = At<void*>(first, off::Object_Class);
            for (int i = have; i < want; i++) {
                void* slot = ui::CreateWidget(pc, cls);
                if (!slot) break;
                ui::AddToGrid(grid, slot, i / cols, i % cols);
                reinterpret_cast<void (*)(void*)>((*reinterpret_cast<void***>(slot))[va::InventorySlotUI_Init_Slot / 8])(
                    slot);
                Append(At<TArray<void*>>(inv, off::FocusMenu_Focusable), At<void*>(slot, off::InventorySlotUI_Button));
            }
            CRAB_LOG("inventory menu: %d more slots in grid %d", want - have, g);
            if (!Scrolls(inv, grid)) MakeScroll(inv, grid);
        }
    }

    // The part of the menu holding the grids: the grid's ancestor that sits on the menu's canvas.
    static void* SlotsPanel(void* inv, void* grid) {
        void* canvas = At<void*>(inv, off::FocusMenu_Canvas);
        void* w = grid;
        for (int depth = 0; w && depth < 16; depth++) {
            void* p = ui::Parent(w);
            if (!p) return nullptr;
            if (p == canvas) return w;
            w = p;
        }
        return nullptr;
    }
    static bool Scrolls(void* inv, void* grid) {
        return IsA(SlotsPanel(inv, grid), StaticClass(va::UScrollBox_StaticClass));
    }

    // Puts the slots panel in a scroll box taking its place on the canvas (same anchors, offsets and order).
    static void MakeScroll(void* inv, void* grid) {
        void* canvas = At<void*>(inv, off::FocusMenu_Canvas);
        void* panel = SlotsPanel(inv, grid);
        void* slot = ui::SlotOf(panel);
        void* scroll = ui::NewWidget(inv, va::UScrollBox_StaticClass);
        if (!canvas || !panel || !slot || !scroll) return;
        ui::AnchorData layout{};
        int32_t z = 0;
        if (!ui::Call(slot, L"GetLayout", &layout) || !ui::Call(slot, L"GetZOrder", &z)) return;
        ui::RemoveChild(canvas, panel);
        void* s = ui::AddToCanvas(canvas, scroll);
        ui::Call(s, L"SetLayout", &layout);
        ui::Call(s, L"SetZOrder", &z);
        bool autoSize = false;
        ui::Call(s, L"SetAutoSize", &autoSize);
        ui::AddChild(scroll, panel);
        uint8_t follow = 2;  // EScrollWhenFocusChanges::AnimatedScroll: gamepad focus stays in view
        ui::Call(scroll, L"SetScrollWhenFocusChanges", &follow);
        CRAB_LOG("inventory menu: the slots scroll now");
    }

    const int numSlots_;  // off::PS_Num*Slots; the items' array is 8 bytes after it
    const char* what_;
    int perLevel_ = 5;
};

SlotItem g_bandolier("bandolier", 175,
                     {Kind::WeaponMod, L"DA_WeaponMod_Bandolier", L"Bandolier", L"More weapon mod slots",
                      L"+PrimaryBuff weapon mod slots", 203, WeaponMod_StreakShot},
                     off::PS_NumWeaponModSlots, "weapon mod");
SlotItem g_toolBelt("toolBelt", 176,
                    {Kind::AbilityMod, L"DA_AbilityMod_ToolBelt", L"Tool Belt", L"More ability mod slots",
                     L"+PrimaryBuff ability mod slots", 201, AbilityMod_HeatSink},
                    off::PS_NumAbilityModSlots, "ability mod");
SlotItem g_scabbard("scabbard", 177,
                    {Kind::MeleeMod, L"DA_MeleeMod_Scabbard", L"Scabbard", L"More melee mod slots",
                     L"+PrimaryBuff melee mod slots", 200, MeleeMod_BigClaws},
                    off::PS_NumMeleeModSlots, "melee mod");
SlotItem g_backpack("backpack", 178,
                    {Kind::Perk, L"DA_Perk_Backpack", L"Backpack", L"More perk slots", L"+PrimaryBuff perk slots",
                     210, Perk_Collector},
                    off::PS_NumPerkSlots, "perk");

}  // namespace
}  // namespace crabmods
