// Key totem: the lobby's key totem lets you choose what to unlock. Using it opens a menu in the game's own style
// (its confirmation prompt, with every locked item's pickup card as a button, category tabs and a search box);
// the item you choose is unlocked and comes out of the totem, as the game's own one does. Once everything is
// unlocked the menu has every item.
//
// The game picks the item on the player's own machine (from their save), unlocks it and asks the server to spawn
// it. The menu only changes which item that is, so it works with any host.
//
// Without the menu (menu = false, or the game's prompt couldn't be found) a row of choices comes out of the
// totem instead, which needs CrabMods on the host:
//   player's machine  ClientOnInteractedWithKeyTotem: sends several items to spawn, then a null to end the list
//   host              ServerSpawnKeyTotemPickup: spawns each one in a row that only that player can take
//   host              a pickup taken: the rest of the row goes away; tells the player's machine which one
//   player's machine  that message: unlocks it in the save
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <random>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
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
        return R"(# Key totem: the lobby's key totem lets you choose what it unlocks, from a menu of every locked item (with
# category tabs and a search box). Once everything is unlocked, the menu has every item.
enabled = true

# true: choose from the menu (works with any host). false: a row of random locked items comes out of the totem
# and you pick one up (needs CrabMods on the host too).
menu = true

# Without the menu: how many items are in the row (1 to 8).
choices = 3
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "choices") choices_ = std::clamp(int(ParseFloat(v)), 1, kMaxChoices);
        else if (k == "menu") menu_ = ParseBool(v);
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override {
        return menu_ ? "choose any item from a menu" : "choose from " + std::to_string(choices_) + " items in a row";
    }

    // Player's machine: keeps the menu up until a choice is made (something else may have closed it), and
    // forgets it on a new level (its widgets are gone).
    void OnTick(void* world) override {
        if (!open_.widget) return;
        if (LevelManagerOf(world) != open_.lm) {
            open_ = {};
            return;
        }
        Search();  // in case typing wasn't caught
        void* pc = open_.pc;
        if (At<void*>(pc, off::PC_OpenMenu)) return;
        bool inViewport = false;
        ui::Call(open_.widget, L"IsInViewport", &inViewport);
        if (!inViewport) {
            int32_t z = kMenuZOrder;
            ui::Call(open_.widget, L"AddToViewport", &z);
        }
        Prepare();
        Show(open_.widget, pc);
    }

    void InstallHooks() override {
        Hook(va::UUserWidget_execCancelLatentActions, &HkClicked, &oClicked, "UUserWidget::execCancelLatentActions");
        SwapVirtual(va::UCrabConfirmationPromptUI_VTable + va::FocusMenu_CanClose_Slot, &HkCanClose, &oCanClose,
                    "UCrabConfirmationPromptUI::CanClose");
        SwapVirtual(va::UCrabConfirmationPromptUI_VTable + va::FocusMenu_NextSubmenu_Slot, &HkNextSubmenu,
                    &oNextSubmenu, "UCrabConfirmationPromptUI::NextSubmenu");
        SwapVirtual(va::UCrabConfirmationPromptUI_VTable + va::FocusMenu_PreviousSubmenu_Slot, &HkPreviousSubmenu,
                    &oPreviousSubmenu, "UCrabConfirmationPromptUI::PreviousSubmenu");
        Hook(va::ACrabTotem_Interact, &HkTotemInteract, &oTotemInteract, "ACrabTotem::Interact");
        Hook(va::ACrabPC_ClientOnInteractedWithKeyTotem_Impl, &HkInteracted, &oInteracted,
             "ACrabPC::ClientOnInteractedWithKeyTotem");
        Hook(va::ACrabInteractPickup_Interact, &HkPickupInteract, &oPickupInteract, "ACrabInteractPickup::Interact");
        // ServerSpawnKeyTotemPickup's implementation is a few bytes: swap its virtual table slot instead.
        SwapVirtual(va::ACrabPC_VTable + va::ACrabPC_ServerSpawnKeyTotemPickup_Slot, &HkSpawn, &oSpawn,
                    "ACrabPC::ServerSpawnKeyTotemPickup");
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

    static std::wstring WString(void* da, int offset) {
        auto& s = At<TArray<wchar_t>>(da, offset);
        std::wstring out;
        for (int i = 0; s.Data && i < s.Num && s.Data[i]; i++) out.push_back(s.Data[i]);
        return out;
    }
    static std::wstring Lower(std::wstring s) {
        for (wchar_t& c : s) c = wchar_t(std::towlower(c));
        return s;
    }

    struct Candidate {
        void* da;
        int list;  // which unlockables list: weapons, abilities, melee, weapon mods, ability mods, melee mods,
                   // perks, relics
    };

    // The game's choice is replaced by a menu, else by a row of choices when the host runs CrabMods, else it
    // is the game's own.
    static void HkInteracted(void* pc, void* totem) {
        KeyTotem& self = *g_self;
        if (!self.enabled_ || !pc || !totem) return oInteracted(pc, totem);
        std::vector<Candidate> items;
        bool unlocking = false;
        if (!self.Pick(pc, items, unlocking)) return oInteracted(pc, totem);
        if (self.menu_ && self.OpenMenu(pc, totem, items, unlocking)) return;
        if (HostHasCrabMods(pc) && self.SendRow(pc, totem, self.Some(items), unlocking)) return;
        oInteracted(pc, totem);
    }

    // The locked items while there are any, else every item.
    bool Pick(void* pc, std::vector<Candidate>& out, bool& unlocking) {
        void* lm = LevelManagerOf(pc);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        void* gi = GameInstanceOf(pc);
        void* sg = gi ? At<void*>(gi, off::GI_SaveGame) : nullptr;
        if (!sp || !sg) return false;
        std::vector<Candidate> locked, any;
        for (int k = 0; k < 8; k++) {
            auto& all = At<TArray<void*>>(sp, off::Spawnables_Unlockables[k]);
            auto& mine = At<TArray<void*>>(sg, off::SG_Unlocked[k]);
            for (int i = 0; i < all.Num; i++)
                if (void* da = all.Data[i]) (Contains(mine, da) ? any : locked).push_back({da, k});
        }
        unlocking = !locked.empty();
        out = unlocking ? std::move(locked) : std::move(any);
        return !out.empty();
    }

    // choices_ of them at random, for the row.
    std::vector<void*> Some(std::vector<Candidate> items) {
        std::shuffle(items.begin(), items.end(), rng_);
        std::vector<void*> das;
        for (size_t i = 0; i < items.size() && int(i) < choices_; i++) das.push_back(items[i].da);
        return das;
    }

    static std::string Names(const std::vector<void*>& das) {
        std::string names;
        for (void* da : das) names += (names.empty() ? "" : ", ") + Name(da);
        return names;
    }

    bool SendRow(void* pc, void* totem, const std::vector<void*>& das, bool unlocking) {
        offered_ = das;
        unlocking_ = unlocking;
        auto spawn = Fn<void (*)(void*, void*, void*)>(va::ACrabPC_ServerSpawnKeyTotemPickup);
        for (void* da : offered_) spawn(pc, totem, da);
        spawn(pc, totem, nullptr);  // the end of the list
        CRAB_LOG("keyTotem: offered a row of %s%s", Names(das).c_str(), unlocking ? "" : " (everything is unlocked)");
        return true;
    }

    // --- The menu (player's machine) ---
    //
    // The game's confirmation prompt with its YES / NO row hidden. Under its message: category tabs (as in the
    // settings menu), a search box, and every item's pickup card as a button, in a scroll box. It can't be closed
    // without a choice (the keys are spent). A choice unlocks the item and asks the server for it, as the game's
    // own key totem does.
    //
    // Clicks and typing are caught by binding them to UUserWidget::CancelLatentActions (hooked): a card's click on
    // the card, a tab's on a hidden card kept for it (its relay), the search box's typing on the menu itself.

    static constexpr int32_t kMenuZOrder = 2;  // as the game's menus
    static constexpr int kTabs = 9;            // all, then one per unlockables list
    static constexpr const wchar_t* kTabNames[kTabs] = {L"ALL",         L"WEAPONS",      L"ABILITIES",
                                                        L"MELEE",       L"WEAPON MODS",  L"ABILITY MODS",
                                                        L"MELEE MODS",  L"PERKS",        L"RELICS"};
    static constexpr float kListWidth = 1300.f;  // three 400-wide cards on their plates, with the gaps
    static constexpr float kListHeight = 600.f;

    struct Entry {
        void* da = nullptr;
        int tab = 0;
        void* button = nullptr;
        void* card = nullptr;
        std::wstring text;  // name, type and description in lower case, for the search
        bool shown = true;
    };
    struct Menu {
        void* widget = nullptr;
        void* pc = nullptr;
        void* lm = nullptr;
        void* totem = nullptr;
        bool unlocking = false;
        std::vector<Entry> items;
        void* tabs[kTabs]{};       // null: no items of that kind
        void* tabRelays[kTabs]{};  // what each tab's click calls CancelLatentActions on
        int tab = 0;
        void* search = nullptr;
        void* scroll = nullptr;
        void* nothing = nullptr;  // "NOTHING MATCHES"
        std::wstring query;       // lower case
    };

    static void* VirtualOf(void* obj, int slot) { return (*reinterpret_cast<void***>(obj))[slot / 8]; }
    static void CallVirtual(void* obj, int slot) { reinterpret_cast<void (*)(void*)>(VirtualOf(obj, slot))(obj); }

    // Binds a dynamic multicast delegate (a button's OnClicked, ...) to target's CancelLatentActions.
    static void Bind(void* w, int delegate, void* target) {
        alignas(8) uint8_t unused[16]{};
        Fn<void (*)(void*, void*, void*, uint64_t)>(va::AddUniqueDynamic)(&At<uint8_t>(w, delegate), target, unused,
                                                                          ui::Name(L"CancelLatentActions"));
    }
    static void SetTints(uint8_t* style, ui::LinearColor normal, ui::LinearColor hovered, ui::LinearColor pressed) {
        At<ui::LinearColor>(style, off::ButtonStyle_NormalTint) = normal;
        At<ui::LinearColor>(style, off::ButtonStyle_HoveredTint) = hovered;
        At<ui::LinearColor>(style, off::ButtonStyle_PressedTint) = pressed;
    }
    static void* AddRow(void* vbox, void* w, float top) {
        void* s = ui::AddToVerticalBox(vbox, w);
        ui::SetPadding(s, {0, top, 0, 0});
        ui::SetHAlign(s, ui::Center);
        return s;
    }

    bool OpenMenu(void* pc, void* totem, std::vector<Candidate> items, bool unlocking) {
        void* cls = ui::FindObject(va::ConfirmationPromptClassPath);
        void* clsCls = cls ? At<void*>(cls, off::Object_Class) : nullptr;
        if (!clsCls || ObjectName(clsCls) != "WidgetBlueprintGeneratedClass") {
            CRAB_LOG("keyTotem: the game's prompt isn't loaded, no menu");
            return false;
        }
        void* w = ui::CreateWidget(pc, cls);
        if (!IsA(w, StaticClass(va::UCrabConfirmationPromptUI_StaticClass))) return false;
        void* msg = At<void*>(w, off::Prompt_MessageText);
        void* yes = At<void*>(w, off::Prompt_YesButton);
        void* vbox = ui::Parent(msg);
        void* canvas = At<void*>(w, off::FocusMenu_Canvas);
        if (!msg || !yes || !vbox || !canvas) return false;

        // As in the inventory: by kind, then rarity, then name.
        std::stable_sort(items.begin(), items.end(), [](const Candidate& a, const Candidate& b) {
            if (a.list != b.list) return a.list < b.list;
            uint8_t ra = At<uint8_t>(a.da, off::DA_Rarity), rb = At<uint8_t>(b.da, off::DA_Rarity);
            if (ra != rb) return ra < rb;
            return WString(a.da, off::DA_Name) < WString(b.da, off::DA_Name);
        });

        Menu m;
        m.widget = w, m.pc = pc, m.lm = LevelManagerOf(pc), m.totem = totem, m.unlocking = unlocking;
        // The prompt's font: its message bigger, as a menu's title; the rest at its size.
        ui::Font font, title;
        ui::FontOf(msg, 16, font);
        if (ui::FontOf(msg, 24, title)) ui::SetFont(msg, title);
        // From the YES button's look: tabs as the settings menu's (no plate until hovered), and cards on a dark,
        // nearly opaque plate (they're see-through, made to float over the world).
        alignas(8) uint8_t tabStyle[off::ButtonStyle_Size], cardStyle[off::ButtonStyle_Size];
        std::memcpy(tabStyle, &At<uint8_t>(yes, off::Button_WidgetStyle), sizeof tabStyle);
        std::memcpy(cardStyle, tabStyle, sizeof cardStyle);
        SetTints(tabStyle, {1, 1, 1, 0}, {1, 1, 1, 0.1f}, {1, 1, 1, 0.25f});
        SetTints(cardStyle, {0.01f, 0.01f, 0.02f, 0.92f}, {0.3f, 0.3f, 0.36f, 0.96f}, {0.5f, 0.5f, 0.56f, 1.f});

        // Tabs: all, and each kind there's something of.
        int count[kTabs]{};
        for (const Candidate& c : items) count[0]++, count[c.list + 1]++;
        void* tabsBox = ui::NewWidget(w, va::UHorizontalBox_StaticClass);
        void* relayClass = At<void*>(items[0].da, off::DA_InteractUIClass);
        if (!tabsBox) return false;
        for (int t = 0; t < kTabs; t++) {
            if (!count[t]) continue;
            void* btn = ui::NewWidget(w, va::UButton_StaticClass);
            void* label = ui::NewWidget(w, va::UTextBlock_StaticClass);
            void* relay = ui::CreateWidget(pc, relayClass);
            if (!btn || !label || !relay) return false;
            ui::Call(btn, L"SetStyle", tabStyle);
            ui::SetText(label, kTabNames[t]);
            ui::SetFont(label, font);
            ui::SetPadding(ui::SetContent(btn, label), {16, 8, 16, 8});
            ui::AddToHorizontalBox(tabsBox, btn);
            ui::SetVisibility(relay, ui::Collapsed);
            ui::AddToVerticalBox(vbox, relay);  // kept with the menu, never shown
            Bind(btn, off::Button_OnClicked, relay);
            m.tabs[t] = btn;
            m.tabRelays[t] = relay;
        }

        void* search = ui::NewWidget(w, va::UEditableTextBox_StaticClass);
        if (!search) return false;
        StyleSearch(search, yes, font);
        ui::Text hint;
        if (ui::MakeText(L"SEARCH", hint)) ui::Call(search, L"SetHintText", hint.bytes);
        Bind(search, off::EditableTextBox_OnTextChanged, w);

        // The cards, three to a row, in a scroll box.
        void* wrap = ui::NewWidget(w, va::UWrapBox_StaticClass);
        void* scroll = ui::NewWidget(w, va::UScrollBox_StaticClass);
        void* size = ui::NewWidget(w, va::USizeBox_StaticClass);
        void* nothing = ui::NewWidget(w, va::UTextBlock_StaticClass);
        if (!wrap || !scroll || !size || !nothing) return false;
        At<float>(wrap, off::WrapBox_WrapSize) = kListWidth;
        At<uint8_t>(wrap, off::WrapBox_bExplicitWrapSize) |= 1;
        float gap[2] = {10, 10};
        ui::Call(wrap, L"SetInnerSlotPadding", gap);
        for (const Candidate& c : items) {
            Entry e;
            e.da = c.da;
            e.tab = c.list + 1;
            e.button = ui::NewWidget(w, va::UButton_StaticClass);
            e.card = ui::CreateWidget(pc, At<void*>(c.da, off::DA_InteractUIClass));
            if (!e.button || !e.card) return false;
            ui::Call(e.button, L"SetStyle", cardStyle);
            std::wstring type = FillCard(e.card, c.da);
            e.text = Lower(WString(c.da, off::DA_Name) + L"\n" + type + L"\n" + WString(c.da, off::DA_Description));
            ui::SetVisibility(e.card, ui::HitTestInvisible);  // the button takes the clicks
            ui::SetPadding(ui::SetContent(e.button, e.card), {6, 6, 6, 6});
            ui::AddToWrapBox(wrap, e.button);
            Bind(e.button, off::Button_OnClicked, e.card);
            m.items.push_back(std::move(e));
        }
        bool many = m.items.size() > 6;
        ui::AddChild(scroll, wrap);
        uint8_t follow = 2;  // EScrollWhenFocusChanges::AnimatedScroll: gamepad focus stays in view
        ui::Call(scroll, L"SetScrollWhenFocusChanges", &follow);
        ui::Call(scroll, L"SetAlwaysShowScrollbar", &many);
        ui::SetContent(size, scroll);
        float width = kListWidth + 20, height = kListHeight;
        ui::Call(size, L"SetWidthOverride", &width);
        ui::Call(size, many ? L"SetHeightOverride" : L"SetMaxDesiredHeight", &height);  // no jumping while searching
        ui::SetText(nothing, L"NOTHING MATCHES");
        ui::SetFont(nothing, font);

        ui::SetVisibility(ui::Parent(yes), ui::Collapsed);  // YES / NO
        AddRow(vbox, tabsBox, 20);
        AddRow(vbox, search, 12);
        AddRow(vbox, size, 20);
        AddRow(vbox, nothing, 20);
        // The game behind is blurred, as behind its other menus.
        if (void* blur = ui::NewWidget(w, va::UBackgroundBlur_StaticClass)) {
            float strength = 20.f;
            ui::Call(blur, L"SetBlurStrength", &strength);
            void* s = ui::AddToCanvas(canvas, blur);
            ui::AnchorData fill{{0, 0, 0, 0}, 0, 0, 1, 1, 0, 0};
            ui::Call(s, L"SetLayout", &fill);
            int32_t z = -2;
            ui::Call(s, L"SetZOrder", &z);
        }

        m.search = search, m.scroll = scroll, m.nothing = nothing;
        int32_t z = kMenuZOrder;
        ui::Call(w, L"AddToViewport", &z);
        Fn<void (*)(void*, bool)>(va::UI_SetShown)(w, true);
        CallVirtual(w, va::FocusMenu_Init_Slot);
        open_ = std::move(m);
        Filter();
        Show(w, pc);
        CRAB_LOG("keyTotem: menu of %d items%s", int(open_.items.size()), unlocking ? "" : " (everything is unlocked)");
        return true;
    }

    // The search box in the look of the game's buttons: the YES button's plate, the prompt's font, light text.
    static void StyleSearch(void* box, void* yes, const ui::Font& font) {
        uint8_t* style = &At<uint8_t>(box, off::EditableTextBox_Style);
        const uint8_t* plate = &At<uint8_t>(yes, off::Button_WidgetStyle + off::ButtonStyle_NormalBrush);
        const float alpha[3] = {0.1f, 0.15f, 0.25f};  // normal, hovered, focused
        for (int i = 0; i < 3; i++) {
            uint8_t* brush = style + off::TextBoxStyle_Brushes[i];
            std::memcpy(brush, plate, off::SlateBrush_Size);
            // Shared pointers in the copy (not counted): the tint's link and the renderer's handle, made anew.
            std::memset(brush + off::SlateBrush_Tint + 0x18, 0, 16);
            std::memset(brush + off::SlateBrush_ResourceHandle, 0, 16);
            At<ui::LinearColor>(brush, off::SlateBrush_Tint) = {1, 1, 1, alpha[i]};
            At<uint8_t>(brush, off::SlateBrush_Tint + 0x10) = 0;  // ESlateColorStylingMode::UseColor_Specified
        }
        At<ui::Margin>(style, off::TextBoxStyle_Padding) = {14, 8, 14, 8};
        ui::Font f = font;
        if (At<void*>(f.bytes, off::FontInfo_FontObject) && !At<void*>(f.bytes, off::FontInfo_CompositeFont))
            std::memcpy(style + off::TextBoxStyle_Font, f.bytes, off::FontInfo_Size);
        At<ui::LinearColor>(style, off::TextBoxStyle_ForegroundColor) = {1, 1, 1, 0.8f};
        At<uint8_t>(style, off::TextBoxStyle_ForegroundColor + 0x10) = 0;
        At<ui::LinearColor>(style, off::TextBoxStyle_BackgroundColor) = {1, 1, 1, 1};
        At<uint8_t>(style, off::TextBoxStyle_BackgroundColor + 0x10) = 0;
        At<float>(box, off::EditableTextBox_MinWidth) = 500.f;
    }

    // The pickup's own card, filled as the game fills it over a pickup, 400 wide, without its "PICK UP" row and
    // its blur (it's on a plate, and hundreds of blurs would be slow). Returns the type line ("Epic Perk").
    static std::wstring FillCard(void* card, void* da) {
        static const wchar_t* const kTypes[] = {L"",           L"Weapon",    L"Ability", L"Melee", L"Weapon Mod",
                                                L"Ability Mod", L"Melee Mod", L"Perk",    L"Relic"};
        static const wchar_t* const kRarities[] = {L"", L"Common", L"Epic", L"Legendary", L"Greed"};
        uint8_t type = At<uint8_t>(da, off::DA_PickupType), rarity = At<uint8_t>(da, off::DA_Rarity);
        std::wstring t = type < std::size(kTypes) ? kTypes[type] : L"";
        if (type > 3 && rarity < std::size(kRarities) && *kRarities[rarity]) t = std::wstring(kRarities[rarity]) + L" " + t;
        TArray<wchar_t> ts{t.data(), int(t.size()) + 1, int(t.size()) + 1};
        Fn<void (*)(void*, void*)>(va::InteractUI_SetIcon)(card, At<void*>(da, off::DA_Icon));
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetType)(card, &ts);
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetName)(card, &At<TArray<wchar_t>>(da, off::DA_Name));
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetDescription)(card,
                                                                        &At<TArray<wchar_t>>(da, off::DA_Description));
        ui::LinearColor color{};
        Fn<void* (*)(void*, ui::LinearColor*)>(va::PickupDA_GetRarityColor)(da, &color);
        Fn<void (*)(void*, const ui::LinearColor*)>(va::InteractUI_SetRarityColor)(card, &color);
        Fn<void (*)(void*, uint8_t)>(va::InteractUI_SetLootPool)(card, At<uint8_t>(da, off::DA_LootPool));
        if (void* pickUp = At<void*>(card, off::InteractUI_InteractMessageText))
            ui::SetVisibility(ui::Parent(ui::Parent(ui::Parent(pickUp))), ui::Collapsed);
        void* blurClass = StaticClass(va::UBackgroundBlur_StaticClass);
        void* p = ui::Parent(At<void*>(card, off::InteractUI_NameText));
        for (int depth = 0; p && depth < 16; depth++, p = ui::Parent(p))
            if (IsA(p, blurClass)) {
                float none = 0;
                ui::Call(p, L"SetBlurStrength", &none);
                break;
            }
        void* tree = At<void*>(card, off::UserWidget_WidgetTree);
        if (void* root = tree ? At<void*>(tree, off::WidgetTree_RootWidget) : nullptr) {
            float width = 400.f;
            ui::Call(root, L"SetWidthOverride", &width);
        }
        return t;
    }

    // Every word typed is in the item's text.
    static bool Matches(const std::wstring& text, const std::wstring& query) {
        size_t i = 0;
        while (i < query.size()) {
            size_t j = query.find(L' ', i);
            if (j == std::wstring::npos) j = query.size();
            if (j > i && text.find(query.substr(i, j - i)) == std::wstring::npos) return false;
            i = j + 1;
        }
        return true;
    }

    // Shows the cards of the chosen tab that match the search.
    void Filter() {
        Menu& m = open_;
        int shown = 0;
        for (Entry& e : m.items) {
            bool show = (m.tab == 0 || e.tab == m.tab) && Matches(e.text, m.query);
            if (show != e.shown) ui::SetVisibility(e.button, show ? ui::Visible : ui::Collapsed);
            e.shown = show;
            shown += show;
        }
        ui::SetVisibility(m.nothing, shown ? ui::Collapsed : ui::HitTestInvisible);
        for (int t = 0; t < kTabs; t++)  // the chosen tab bright, as in the settings menu
            if (m.tabs[t]) ui::SetColorAndOpacity(m.tabs[t], {1, 1, 1, t == m.tab ? 1.f : 0.5f});
        ui::Call(m.scroll, L"ScrollToStart");
        Prepare();
    }

    void SetTab(int t) {
        if (t == open_.tab) return;
        open_.tab = t;
        Filter();
    }

    void Search() {
        std::wstring q = Lower(ui::TextToString(&At<uint8_t>(open_.search, off::EditableTextBox_Text)));
        if (q == open_.query) return;
        open_.query = std::move(q);
        Filter();
    }

    // What the prompt's own setup would otherwise put back: its message, and gamepad focus moving between the
    // shown cards instead of YES / NO.
    void Prepare() {
        void* w = open_.widget;
        ui::SetText(At<void*>(w, off::Prompt_MessageText),
                    open_.unlocking ? L"CHOOSE AN ITEM TO UNLOCK" : L"CHOOSE AN ITEM");
        auto& focusable = At<TArray<void*>>(w, off::FocusMenu_Focusable);
        focusable.Num = 0;
        for (Entry& e : open_.items)
            if (e.shown) Append(focusable, e.button);
    }

    // As the game opens a menu: shown, refreshed, the player's open menu (input and cursor follow it), the
    // gameplay UI hidden.
    static void Show(void* w, void* pc) {
        Fn<void (*)(void*, bool)>(va::UI_SetShown)(w, true);
        CallVirtual(w, va::FocusMenu_Refresh_Slot);
        At<void*>(pc, off::PC_OpenMenu) = w;
        Fn<void (*)(void*)>(va::ACrabPC_UpdateInputMode)(pc);
        if (void* hud = At<void*>(pc, off::PC_HUD)) Fn<void (*)(void*, bool)>(va::UI_SetShown)(hud, false);
    }

    void Choose(size_t i) {
        Menu m = std::move(open_);
        open_ = {};
        CallVirtual(m.widget, va::FocusMenu_PressSound_Slot);
        ui::RemoveFromParent(m.widget);
        if (At<void*>(m.pc, off::PC_OpenMenu) == m.widget)
            Fn<void (*)(void*)>(va::ACrabPC_CloseMenuRestoreGame)(m.pc);
        void* da = m.items[i].da;
        void* gi = GameInstanceOf(m.pc);
        if (m.unlocking && gi && !Unlocked(gi, da)) Fn<void (*)(void*, void*)>(va::UCrabGI_UnlockPickup)(gi, da);
        // One item: the server spawns it as the game's own key totem item (a CrabMods host, for this player only).
        Fn<void (*)(void*, void*, void*)>(va::ACrabPC_ServerSpawnKeyTotemPickup)(m.pc, m.totem, da);
        CRAB_LOG("keyTotem: chose %s%s", Name(da).c_str(), m.unlocking ? " (unlocked)" : "");
    }

    // Cards, tabs and the search box call UUserWidget::CancelLatentActions (see above).
    using ExecFn = void (*)(void*, void*, void*);
    static inline ExecFn oClicked;
    static void HkClicked(void* context, void* frame, void* result) {
        oClicked(context, frame, result);
        KeyTotem& self = *g_self;
        Menu& m = self.open_;
        if (!m.widget || !context) return;
        if (context == m.widget) return self.Search();
        for (int t = 0; t < kTabs; t++)
            if (m.tabRelays[t] == context) {
                CallVirtual(m.widget, va::FocusMenu_PressSound_Slot);
                return self.SetTab(t);
            }
        for (size_t i = 0; i < m.items.size(); i++)
            if (m.items[i].card == context) return self.Choose(i);
    }

    // The Next / Previous Submenu keys (the bumpers) switch tabs, but not while typing in the search box.
    using SubmenuFn = void (*)(void*);
    static inline SubmenuFn oNextSubmenu, oPreviousSubmenu;
    static void HkNextSubmenu(void* w) {
        if (!g_self->Cycle(w, 1)) oNextSubmenu(w);
    }
    static void HkPreviousSubmenu(void* w) {
        if (!g_self->Cycle(w, -1)) oPreviousSubmenu(w);
    }
    bool Cycle(void* w, int dir) {
        Menu& m = open_;
        if (!w || w != m.widget) return false;
        bool focus = false, inside = false;
        ui::Call(m.search, L"HasKeyboardFocus", &focus);
        ui::Call(m.search, L"HasFocusedDescendants", &inside);
        if (focus || inside) return true;
        int t = m.tab;
        do t = (t + dir + kTabs) % kTabs;
        while (!m.tabs[t]);
        if (t == m.tab) return true;
        CallVirtual(w, va::FocusMenu_PressSound_Slot);
        SetTab(t);
        for (Entry& e : m.items)  // gamepad focus was on a card that may be gone
            if (e.shown) {
                ui::Call(e.button, L"SetKeyboardFocus");
                break;
            }
        return true;
    }

    // Escape and the gamepad's back button close a menu only if it allows it.
    using CanCloseFn = bool (*)(void*);
    static inline CanCloseFn oCanClose;
    static bool HkCanClose(void* w) { return w != g_self->open_.widget && oCanClose(w); }

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
    bool menu_ = true;
    int choices_ = 3;
    Menu open_;
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
