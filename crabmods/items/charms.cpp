// Charms, a kind of item of their own that gives inventory space: Bandolier (weapon mod slots), Tool Belt (ability
// mod slots), Scabbard (melee mod slots), Backpack (perk slots) and Reliquary (relic slots). Each level gives 5
// more. Charms have a section of their own in the inventory menu, and their own colour and "Charm" on their card.
//
// The game has no such kind, so a charm is held as a relic (the relics list is replicated and in the auto save),
// and everything that sees relics is told apart:
// - every machine: a charm can always be picked up (it never takes a relic slot); the relic grid of the inventory
//   menu leaves charms out, and the menu gets a CHARMS grid below its slots.
// - host: the same charm again adds its level to the one held (the game would add a second entry).
//
// Host: the player's slot count (ACrabPS NumWeaponModSlots, ...) is raised by the bonus. What has been added is
// kept in the unused byte right after the count, so the bonus is never added twice: a new run or a challenge
// sets the count from scratch (the byte goes back to 0), and between islands the count comes back from the
// players' auto save with the bonus in it (the byte is set to what the charms give).
//
// Relics have no slot count: the game allows 10, in a pickup's check (can the player take this relic) and in the
// menu. Every machine: both are raised by the Reliquary's slots, from the replicated inventory.
//
// Every machine: the inventory menu has one slot widget per slot the game knows of (24 / 12 / 12 / 24 / 10).
// Before it lays them out, more are made like the first, and the slots part of the menu is put in a scroll box.
#include <algorithm>
#include <cstring>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Charm;
std::vector<Charm*> g_charms;

constexpr int kRelicSize = 0x28;  // FCrabRelic: DA, then FCrabInventoryInfo (Level, Enhancements, AccumulatedBuff)
constexpr int kRelicInfo = 0x8;
constexpr int kRelicEnhancements = 0x10;

class Charm : public AssetItem {
public:
    Charm(const char* section, int order, const AssetSpec& spec, int numSlots, const char* what, uint8_t rarity)
        : AssetItem(section, order, spec, rarity), numSlots_(numSlots), what_(what) {
        g_charms.push_back(this);
    }

    std::string DefaultIni() const override {
        return "# " + std::string(ui::Narrow(spec_.name)) + ", a charm: more " + what_ +
               " slots. Charms have their own section in the inventory, never take a relic slot, and the same\n"
               "# charm again levels it up.\nenabled = true\n\n"
               "# Slots for each level" +
               (IsRelic() ? std::string(" (on top of the game's 10)") : std::string()) +
               ".\nslotsPerLevel = 5\n\n"
               "# Which chests it drops from: common, epic, legendary or greed (and relic chests).\nrarity = " + std::string(RarityName(rarity_)) + "\n";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "slotsPerLevel") perLevel_ = std::clamp(int(ParseFloat(v)), 1, 50);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "charm, +" + std::to_string(perLevel_) + " " + what_ + " slots per level, " + RarityName(rarity_);
    }

    void InstallHooks() override {
        static bool done = false;
        if (done) return;
        done = true;
        Hook(va::ACrabPS_RestoreAutoSave, &HkRestore, &oRestore, "ACrabPS::RestoreAutoSave");
        Hook(va::ACrabPS_ResetForNewRun, &HkReset, &oReset, "ACrabPS::ResetForNewRun");
        Hook(va::ACrabPS_ApplyChallengeSlots, &HkChallenge, &oChallenge, "ACrabPS::ApplyChallengeSlots");
        Hook(va::ACrabPS_AddRelic, &HkAddRelic, &oAddRelic, "ACrabPS::AddRelic");
        Hook(va::ACrabPS_GetRelics, &HkGetRelics, &oGetRelics, "ACrabPS::GetRelics");
        Hook(va::UCrabInventoryUI_Refresh, &HkRefresh, &oRefresh, "UCrabInventoryUI::Refresh");
        Hook(va::ACrabPS_CanPickupRelic, &HkCanRelic, &oCanRelic, "ACrabPS::CanPickupRelic");
        Hook(va::UCrabInventoryUI_SetupGrid, &HkSetupGrid, &oSetupGrid, "UCrabInventoryUI::SetupGrid");
    }

    // Host: the count follows the charms' level.
    void OnTick(void* world) override {
        if (!da_ || IsRelic()) return;
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

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        return "Total " + std::string(what_) + " slots gained: " + std::to_string(Bonus(ps));
    }

    static bool Is(void* da) {
        if (!da) return false;
        for (Charm* c : g_charms)
            if (c->da_ == da) return true;
        return false;
    }

protected:
    float PrimaryBuff() const override { return float(perLevel_); }
    void Customize(void* da) override { At<bool>(da, off::DA_bGetBuffAsMultiplier) = false; }

private:
    bool IsRelic() const { return numSlots_ == kRelics; }
    int Bonus(void* ps) const { return enabled_ ? std::min(perLevel_ * Level(ps), 255) : 0; }
    uint8_t& Added(void* ps) const { return At<uint8_t>(ps, numSlots_ + 1); }

    static bool IsType(uint8_t type) {
        for (Charm* c : g_charms)
            if (c->spec_.type == type) return true;
        return false;
    }

    // --- Host: where the count is set from scratch ---

    using PSFn = void (*)(void*);
    using RestoreFn = void (*)(void*, void*);
    static inline PSFn oReset, oChallenge;
    static inline RestoreFn oRestore;

    static void HkRestore(void* ps, void* save) {
        oRestore(ps, save);
        if (ps)
            for (Charm* s : g_charms)
                if (!s->IsRelic()) s->Added(ps) = uint8_t(s->Bonus(ps));
    }
    static void HkReset(void* ps) {
        oReset(ps);
        if (ps)
            for (Charm* s : g_charms)
                if (!s->IsRelic()) s->Added(ps) = 0;
    }
    // Only in a challenge run with the 8-slot modifier: the counts it changed start over.
    static void HkChallenge(void* ps) {
        if (!ps) return oChallenge(ps);
        uint8_t before[4];
        for (int g = 0; g < 4; g++) before[g] = At<uint8_t>(ps, kNumSlots[g]);
        oChallenge(ps);
        for (Charm* s : g_charms)
            for (int g = 0; g < 4; g++)
                if (s->numSlots_ == kNumSlots[g] && At<uint8_t>(ps, kNumSlots[g]) != before[g]) s->Added(ps) = 0;
    }

    // --- Host: the same charm again is a level more ---

    using AddRelicFn = void (*)(void*, const void*);
    static inline AddRelicFn oAddRelic;
    static void HkAddRelic(void* ps, const void* in) {
        void* relic = const_cast<void*>(in);
        void* da = relic ? At<void*>(relic, 0) : nullptr;
        if (!ps || !Is(da)) return oAddRelic(ps, relic);
        auto& a = At<TArray<uint8_t>>(ps, off::PS_Relics);
        for (int i = 0; a.Data && i < a.Num; i++) {
            uint8_t* e = a.Data + i * kRelicSize;
            if (At<void*>(e, 0) != da) continue;
            uint8_t& level = At<uint8_t>(e, kRelicInfo);
            level = uint8_t(std::min(level + std::max<int>(At<uint8_t>(relic, kRelicInfo), 1), 255));
            Fn<void (*)(void*)>(va::FMulticastDelegate_Broadcast0)(&At<uint8_t>(ps, off::PS_OnInventoryChanged));
            void* pc = At<void*>(ps, off::Actor_Owner);
            if (IsA(pc, StaticClass(va::ACrabPC_StaticClass)))
                Fn<void (*)(void*, void*, uint8_t)>(va::ACrabPC_ClientOnPickedUpPickup)(pc, da, level);
            CRAB_LOG("charms: player %p's %s is level %d", ps, ObjectName(da).c_str(), int(level));
            return;
        }
        oAddRelic(ps, relic);
    }

    // --- Every machine: relics ---

    static constexpr int kRelics = 0;  // numSlots_ of the relic charm: there's no count to keep
    static constexpr int kGameRelicSlots = 10;

    static int RelicSlots(void* ps) {
        int n = kGameRelicSlots;
        for (Charm* s : g_charms)
            if (s->IsRelic()) n += s->Bonus(ps);
        return std::min(n, 255);
    }

    using CanRelicFn = bool (*)(void*, uint8_t);
    static inline CanRelicFn oCanRelic;
    // A charm always (it levels up); a relic if not held (relics don't stack) and a relic slot is free.
    static bool HkCanRelic(void* ps, uint8_t type) {
        if (!ps) return oCanRelic(ps, type);
        if (IsType(type)) return true;
        bool held = false;
        int relics = 0;
        ForEachItem(ps, off::PS_Relics, [&](void* da, int) {
            if (Is(da)) return;
            relics++;
            held = held || At<uint8_t>(da, off::DA_ItemType) == type;
        });
        return !held && relics < RelicSlots(ps);
    }

    // --- Every machine: the inventory menu ---

    using RefreshFn = void (*)(void*);
    static inline RefreshFn oRefresh;

    static constexpr int kNumSlots[4] = {off::PS_NumWeaponModSlots, off::PS_NumAbilityModSlots,
                                         off::PS_NumMeleeModSlots, off::PS_NumPerkSlots};

    static inline bool hideCharms_ = false;  // while the menu lays out its relic grid

    static void HkRefresh(void* inv) {
        void* charms = nullptr;
        if (inv) {
            Grow(inv);
            charms = CharmGrid(inv);
        }
        hideCharms_ = true;
        oRefresh(inv);
        hideCharms_ = false;
        if (charms) FillCharms(inv, charms);
    }

    // The relic grid is laid out from a copy of the relics: charms are taken out of it.
    using GetRelicsFn = void* (*)(void*, void*);
    static inline GetRelicsFn oGetRelics;
    static void* HkGetRelics(void* ps, void* out) {
        void* r = oGetRelics(ps, out);
        if (!hideCharms_ || !r) return r;
        auto& a = *static_cast<TArray<uint8_t>*>(r);
        int n = 0;
        for (int i = 0; a.Data && i < a.Num; i++) {
            uint8_t* e = a.Data + i * kRelicSize;
            if (Is(At<void*>(e, 0))) {
                if (void* enh = At<void*>(e, kRelicEnhancements)) Fn<void (*)(void*)>(va::FMemory_Free)(enh);
                continue;
            }
            if (n != i) std::memcpy(a.Data + n * kRelicSize, e, kRelicSize);
            n++;
        }
        if (a.Data) a.Num = n;
        return r;
    }

    // The relic grid's count is the game's 10: its slots past that would show locked.
    using SetupGridFn = void (*)(void*, void*, uint32_t);
    static inline SetupGridFn oSetupGrid;
    static void HkSetupGrid(void* inv, void* grid, uint32_t count) {
        if (inv && grid && grid == At<void*>(inv, off::InventoryUI_RelicGrid))
            if (void* ps = At<void*>(inv, off::InventoryUI_OwningPS)) count = uint32_t(RelicSlots(ps));
        oSetupGrid(inv, grid, count);
    }

    static int Columns(void* grid) {
        int cols = 1;
        for (int i = 0, n = ui::ChildCount(grid); i < n; i++)
            if (void* s = ui::SlotOf(ui::ChildAt(grid, i)))
                cols = std::max(cols, At<int32_t>(s, off::UniformGridSlot_Column) + 1);
        return cols;
    }

    // Slot widgets like cls, for slots have..want-1 of the grid, set up as the menu's init sets up its own (hover,
    // gamepad focus). The game then fills them like the rest.
    static bool AddSlots(void* inv, void* grid, void* cls, int have, int want, int cols) {
        void* pc = nullptr;
        ui::Call(inv, L"GetOwningPlayer", &pc);
        if (!pc) return false;
        for (int i = have; i < want; i++) {
            void* slot = ui::CreateWidget(pc, cls);
            if (!slot) return false;
            ui::AddToGrid(grid, slot, i / cols, i % cols);
            reinterpret_cast<void (*)(void*)>((*reinterpret_cast<void***>(slot))[va::InventorySlotUI_Init_Slot / 8])(
                slot);
            Append(At<TArray<void*>>(inv, off::FocusMenu_Focusable), At<void*>(slot, off::InventorySlotUI_Button));
        }
        return true;
    }

    static void* FirstSlot(void* grid) {
        void* first = grid && ui::ChildCount(grid) ? ui::ChildAt(grid, 0) : nullptr;
        return IsA(first, StaticClass(va::UCrabInventorySlotUI_StaticClass)) ? first : nullptr;
    }

    // Makes slot widgets for slots past the menu's own, laid out in the grid's columns.
    static void Grow(void* inv) {
        void* ps = At<void*>(inv, off::InventoryUI_OwningPS);
        if (!ps) return;
        for (int g = 0; g < 5; g++) {
            void* grid = At<void*>(inv, g < 4 ? off::InventoryUI_Grids[g] : off::InventoryUI_RelicGrid);
            int want = g < 4 ? At<uint8_t>(ps, kNumSlots[g]) : RelicSlots(ps);
            int have = grid ? ui::ChildCount(grid) : 0;
            void* first = FirstSlot(grid);
            if (want <= have || !first) continue;
            if (!AddSlots(inv, grid, At<void*>(first, off::Object_Class), have, want, Columns(grid))) return;
            CRAB_LOG("inventory menu: %d more slots in grid %d", want - have, g);
            if (!Scrolls(inv, grid)) MakeScroll(inv, grid);
        }
    }

    // --- The CHARMS section: a title and a grid, below the slots in their scroll box ---

    // The scroll box holds the slots panel, then the title, then the charm grid (made once per menu).
    static void* CharmGrid(void* inv) {
        void* relics = At<void*>(inv, off::InventoryUI_RelicGrid);
        void* first = FirstSlot(relics);
        if (!first) return nullptr;
        if (!Scrolls(inv, relics)) MakeScroll(inv, relics);
        void* scroll = SlotsPanel(inv, relics);
        if (!IsA(scroll, StaticClass(va::UScrollBox_StaticClass))) return nullptr;
        int children = ui::ChildCount(scroll);
        if (children >= 3) return ui::ChildAt(scroll, 2);
        if (children != 1) return nullptr;

        void* title = ui::NewWidget(inv, va::UTextBlock_StaticClass);
        void* grid = ui::NewWidget(inv, va::UUniformGridPanel_StaticClass);
        if (!title || !grid) return nullptr;
        StyleTitle(title, relics);
        ui::SetText(title, L"CHARMS");
        void* ts = ui::AddChild(scroll, title);
        ui::SetPadding(ts, {0, 32, 0, 12});
        ui::SetHAlign(ts, ui::Center);

        ui::Margin pad = At<ui::Margin>(relics, kGrid_SlotPadding);
        float minW = At<float>(relics, kGrid_MinSlotWidth), minH = At<float>(relics, kGrid_MinSlotHeight);
        ui::Call(grid, L"SetSlotPadding", &pad);
        ui::Call(grid, L"SetMinDesiredSlotWidth", &minW);
        ui::Call(grid, L"SetMinDesiredSlotHeight", &minH);
        void* gs = ui::AddChild(scroll, grid);
        ui::SetPadding(gs, {0, 0, 0, 24});
        ui::SetHAlign(gs, ui::Center);
        int n = int(g_charms.size());
        if (!AddSlots(inv, grid, At<void*>(first, off::Object_Class), 0, n, std::min(Columns(relics), n))) return nullptr;
        CRAB_LOG("inventory menu: charms section, %d slots", n);
        return grid;
    }

    // As the menu's own titles look: the first text block next to the relic grid (or a level up) gives its font,
    // colour and shadow.
    static void StyleTitle(void* title, void* relics) {
        void* textClass = StaticClass(va::UTextBlock_StaticClass);
        void* like = nullptr;
        for (void* p = ui::Parent(relics); p && !like; p = ui::Parent(p)) {
            for (int i = 0, n = ui::ChildCount(p); i < n && !like; i++)
                if (void* c = ui::ChildAt(p, i); IsA(c, textClass)) like = c;
            if (IsA(p, StaticClass(va::UScrollBox_StaticClass))) break;
        }
        uint8_t center = 1;  // ETextJustify::Center
        ui::Call(title, L"SetJustification", &center);
        if (!like) return;
        ui::Font f;
        if (ui::FontOf(like, At<int32_t>(like, off::TextBlock_Font + off::FontInfo_PointSize), f)) ui::SetFont(title, f);
        struct {
            ui::LinearColor color;
            uint8_t rule = 0;  // ESlateColorStylingMode::UseColor_Specified
            uint8_t pad[7]{};
            void* shared[2]{};
        } color{At<ui::LinearColor>(like, kText_Color)};
        ui::Call(title, L"SetColorAndOpacity", &color);
        float shadow[2] = {At<float>(like, kText_ShadowOffset), At<float>(like, kText_ShadowOffset + 4)};
        ui::Call(title, L"SetShadowOffset", shadow);
        ui::LinearColor shadowColor = At<ui::LinearColor>(like, kText_ShadowColor);
        ui::Call(title, L"SetShadowColorAndOpacity", &shadowColor);
    }

    // After the menu's own layout: charm i in slot i.
    static void FillCharms(void* inv, void* grid) {
        void* ps = At<void*>(inv, off::InventoryUI_OwningPS);
        if (!ps) return;
        int slots = ui::ChildCount(grid);
        oSetupGrid(inv, grid, uint32_t(slots));
        auto& a = At<TArray<uint8_t>>(ps, off::PS_Relics);
        int j = 0;
        for (int i = 0; a.Data && i < a.Num && j < slots; i++) {
            uint8_t* e = a.Data + i * kRelicSize;
            void* da = At<void*>(e, 0);
            if (!Is(da)) continue;
            void* slot = ui::ChildAt(grid, j++);
            if (IsA(slot, StaticClass(va::UCrabInventorySlotUI_StaticClass)))
                Fn<void (*)(void*, void*, const void*)>(va::InventorySlotUI_Set)(slot, da, e + kRelicInfo);
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

    // UUniformGridPanel, UTextBlock
    static constexpr int kGrid_SlotPadding = 0x120;  // FMargin
    static constexpr int kGrid_MinSlotWidth = 0x130;
    static constexpr int kGrid_MinSlotHeight = 0x134;
    static constexpr int kText_Color = 0x150;         // FSlateColor: its specified colour first
    static constexpr int kText_ShadowOffset = 0x268;  // FVector2D
    static constexpr int kText_ShadowColor = 0x270;   // FLinearColor

    const int numSlots_;  // off::PS_Num*Slots, the items' array 8 bytes after it; kRelics for the relic charm
    const char* what_;
    int perLevel_ = 5;
};

// Charms are relics underneath (the template is Ring of Defense); their type ids are their own.
Charm g_bandolier("bandolier", 175,
                  {Kind::Relic, L"DA_Charm_Bandolier", L"Bandolier", L"More weapon mod slots",
                   L"+PrimaryBuff weapon mod slots", 240, Relic_RingOfDefense},
                  off::PS_NumWeaponModSlots, "weapon mod", rarity::Epic);
Charm g_toolBelt("toolBelt", 176,
                 {Kind::Relic, L"DA_Charm_ToolBelt", L"Tool Belt", L"More ability mod slots",
                  L"+PrimaryBuff ability mod slots", 241, Relic_RingOfDefense},
                 off::PS_NumAbilityModSlots, "ability mod", rarity::Epic);
Charm g_scabbard("scabbard", 177,
                 {Kind::Relic, L"DA_Charm_Scabbard", L"Scabbard", L"More melee mod slots",
                  L"+PrimaryBuff melee mod slots", 242, Relic_RingOfDefense},
                 off::PS_NumMeleeModSlots, "melee mod", rarity::Epic);
Charm g_backpack("backpack", 178,
                 {Kind::Relic, L"DA_Charm_Backpack", L"Backpack", L"More perk slots", L"+PrimaryBuff perk slots", 243,
                  Relic_RingOfDefense},
                 off::PS_NumPerkSlots, "perk", rarity::Epic);
Charm g_reliquary("reliquary", 179,
                  {Kind::Relic, L"DA_Charm_Reliquary", L"Reliquary", L"More relic slots", L"+PrimaryBuff relic slots",
                   244, Relic_RingOfDefense},
                  0 /* kRelics */, "relic", rarity::Legendary);

}  // namespace

bool IsCharm(void* da) { return Charm::Is(da); }

}  // namespace crabmods
