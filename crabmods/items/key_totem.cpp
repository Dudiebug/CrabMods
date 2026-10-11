// Key totem: the lobby's key totem lets you choose what to unlock. Using it opens a menu in the game's own style
// (its confirmation prompt, with the pickups' own cards as buttons); the item you choose is unlocked and comes
// out of the totem, as the game's own one does. Once everything is unlocked it offers a choice of any weapon
// mods, ability mods, melee mods, perks and relics.
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
        return R"(# Key totem: the lobby's key totem lets you choose what it unlocks, from a menu of locked items. Once
# everything is unlocked, it offers any weapon mods, ability mods, melee mods, perks and relics to choose from.
enabled = true

# How many items to choose from (1 to 8).
choices = 3

# true: choose from a menu (works with any host). false: a row of the items comes out of the totem and you
# pick one up (needs CrabMods on the host too).
menu = true
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "choices") choices_ = std::clamp(int(ParseFloat(v)), 1, kMaxChoices);
        else if (k == "menu") menu_ = ParseBool(v);
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override {
        return "choose from " + std::to_string(choices_) + " items" + (menu_ ? ", in a menu" : ", in a row");
    }

    // Player's machine: keeps the menu up until a choice is made (something else may have closed it), and
    // forgets it on a new level (its widgets are gone).
    void OnTick(void* world) override {
        if (!open_.widget) return;
        if (LevelManagerOf(world) != open_.lm) {
            open_ = {};
            return;
        }
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

    // The game's choice is replaced by a menu, else by a row of choices when the host runs CrabMods, else it
    // is the game's own.
    static void HkInteracted(void* pc, void* totem) {
        KeyTotem& self = *g_self;
        if (!self.enabled_ || !pc || !totem) return oInteracted(pc, totem);
        std::vector<void*> das;
        bool unlocking = false;
        if (!self.Pick(pc, das, unlocking)) return oInteracted(pc, totem);
        if (self.menu_ && self.OpenMenu(pc, totem, das, unlocking)) return;
        if (HostHasCrabMods(pc) && self.SendRow(pc, totem, das, unlocking)) return;
        oInteracted(pc, totem);
    }

    // Up to choices_ items: locked ones while there are any, else any weapon mods, ability mods, melee mods,
    // perks and relics.
    bool Pick(void* pc, std::vector<void*>& out, bool& unlocking) {
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
                else if (k >= 3) any.push_back(da);
            }
        }
        unlocking = !locked.empty();
        out = unlocking ? locked : any;
        if (out.empty()) return false;
        std::shuffle(out.begin(), out.end(), rng_);
        out.resize(std::min<size_t>(out.size(), choices_));
        return true;
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
    // The game's confirmation prompt with its YES / NO row hidden and the items' pickup cards as buttons under
    // its message. It can't be closed without a choice (the keys are spent). A choice unlocks the item and asks
    // the server for it, as the game's own key totem does.

    static constexpr int32_t kMenuZOrder = 2;  // as the game's menus

    struct Menu {
        void* widget = nullptr;
        void* pc = nullptr;
        void* lm = nullptr;
        void* totem = nullptr;
        bool unlocking = false;
        std::vector<void*> das, buttons, cards;
    };

    static void* VirtualOf(void* obj, int slot) { return (*reinterpret_cast<void***>(obj))[slot / 8]; }
    static void CallVirtual(void* obj, int slot) { reinterpret_cast<void (*)(void*)>(VirtualOf(obj, slot))(obj); }

    bool OpenMenu(void* pc, void* totem, const std::vector<void*>& das, bool unlocking) {
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

        Menu m{w, pc, LevelManagerOf(pc), totem, unlocking, das, {}, {}};
        int n = int(das.size());
        int cols = n <= 4 ? n : (n + 1) / 2;  // at most 2 rows
        void* grid = ui::NewWidget(w, va::UUniformGridPanel_StaticClass);
        if (!grid) return false;
        ui::Margin gap{8, 8, 8, 8};
        ui::Call(grid, L"SetSlotPadding", &gap);
        for (int i = 0; i < n; i++) {
            void* btn = ui::NewWidget(w, va::UButton_StaticClass);
            void* card = ui::CreateWidget(pc, At<void*>(das[i], off::DA_InteractUIClass));
            if (!btn || !card) return false;
            ui::Call(btn, L"SetStyle", &At<uint8_t>(yes, off::Button_WidgetStyle));  // the YES button's look
            FillCard(card, das[i], n > 3);
            ui::SetVisibility(card, ui::HitTestInvisible);  // the button takes the clicks
            ui::SetPadding(ui::SetContent(btn, card), {6, 6, 6, 6});
            ui::AddToGrid(grid, btn, i / cols, i % cols);
            alignas(8) uint8_t unused[16]{};
            Fn<void (*)(void*, void*, void*, uint64_t)>(va::AddUniqueDynamic)(
                &At<uint8_t>(btn, off::Button_OnClicked), card, unused, ui::Name(L"CancelLatentActions"));
            m.buttons.push_back(btn);
            m.cards.push_back(card);
        }
        ui::SetVisibility(ui::Parent(yes), ui::Collapsed);  // YES / NO
        void* gridSlot = ui::AddToVerticalBox(vbox, grid);
        ui::SetPadding(gridSlot, {0, 25, 0, 0});
        ui::SetHAlign(gridSlot, ui::Center);
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

        int32_t z = kMenuZOrder;
        ui::Call(w, L"AddToViewport", &z);
        Fn<void (*)(void*, bool)>(va::UI_SetShown)(w, true);
        CallVirtual(w, va::FocusMenu_Init_Slot);
        open_ = std::move(m);
        Prepare();
        Show(w, pc);
        CRAB_LOG("keyTotem: menu of %s%s", Names(das).c_str(), unlocking ? "" : " (everything is unlocked)");
        return true;
    }

    // The pickup's own card, filled as the game fills it over a pickup, without its "PICK UP" row.
    static void FillCard(void* card, void* da, bool narrow) {
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
        void* tree = At<void*>(card, off::UserWidget_WidgetTree);
        void* root = tree ? At<void*>(tree, off::WidgetTree_RootWidget) : nullptr;
        if (narrow && root) {  // four cards to a row
            float width = 400.f;
            ui::Call(root, L"SetWidthOverride", &width);
        }
    }

    // What the prompt's own setup would otherwise put back: its message, and gamepad focus moving between the
    // cards instead of YES / NO.
    void Prepare() {
        void* w = open_.widget;
        ui::SetText(At<void*>(w, off::Prompt_MessageText),
                    open_.unlocking ? L"CHOOSE AN ITEM TO UNLOCK" : L"CHOOSE AN ITEM");
        auto& focusable = At<TArray<void*>>(w, off::FocusMenu_Focusable);
        focusable.Num = 0;
        for (void* b : open_.buttons) Append(focusable, b);
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
        void* da = m.das[i];
        void* gi = GameInstanceOf(m.pc);
        if (m.unlocking && gi && !Unlocked(gi, da)) Fn<void (*)(void*, void*)>(va::UCrabGI_UnlockPickup)(gi, da);
        // One item: the server spawns it as the game's own key totem item (a CrabMods host, for this player only).
        Fn<void (*)(void*, void*, void*)>(va::ACrabPC_ServerSpawnKeyTotemPickup)(m.pc, m.totem, da);
        CRAB_LOG("keyTotem: chose %s%s", Name(da).c_str(), m.unlocking ? " (unlocked)" : "");
    }

    // The cards' buttons call UUserWidget::CancelLatentActions on their card when clicked.
    using ExecFn = void (*)(void*, void*, void*);
    static inline ExecFn oClicked;
    static void HkClicked(void* context, void* frame, void* result) {
        oClicked(context, frame, result);
        Menu& m = g_self->open_;
        if (!m.widget || !context) return;
        auto it = std::find(m.cards.begin(), m.cards.end(), context);
        if (it != m.cards.end()) g_self->Choose(size_t(it - m.cards.begin()));
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
