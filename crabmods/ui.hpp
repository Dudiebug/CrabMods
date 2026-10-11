// Widgets (UMG) from code: making the engine's own widgets and calling their blueprint functions by name, as a
// blueprint would. Everything here runs on the game thread, on the machine that shows the UI.
#pragma once
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>

#include <crab/log.hpp>
#include <crab/ue.hpp>

namespace crabmods::ui {

using namespace crab;

struct Margin {
    float left, top, right, bottom;
};
struct LinearColor {
    float r, g, b, a;
};

enum Visibility : uint8_t { Visible = 0, Collapsed = 1, Hidden = 2, HitTestInvisible = 3, SelfHitTestInvisible = 4 };
enum Align : uint8_t { Fill = 0, Left = 1, Top = 1, Center = 2, Right = 3, Bottom = 3 };

inline uint64_t Name(const wchar_t* s) {
    uint64_t n = 0;
    Fn<void* (*)(uint64_t*, const wchar_t*, int)>(va::FName_Ctor)(&n, s, 1 /*FNAME_Add*/);
    return n;
}

inline std::string Narrow(const wchar_t* s) {
    std::string out;
    for (; *s; s++) out.push_back(*s < 0x80 ? char(*s) : '?');
    return out;
}

// Calls the blueprint-callable function fn on obj (ProcessEvent), with its parameters laid out as the engine
// does (return value last). false if obj is null or its class has no such function.
inline bool Call(void* obj, const wchar_t* fn, void* params = nullptr) {
    if (!obj) return false;
    void* cls = At<void*>(obj, off::Object_Class);
    void* f = Fn<void* (*)(void*, uint64_t, int)>(va::UClass_FindFunctionByName)(cls, Name(fn), 1);
    if (!f) {
        CRAB_LOG("ui: %s has no function %s", ObjectName(cls).c_str(), Narrow(fn).c_str());
        return false;
    }
    void** vtable = *reinterpret_cast<void***>(obj);
    reinterpret_cast<void (*)(void*, void*, void*)>(vtable[va::UObject_ProcessEvent_Slot / 8])(obj, f, params);
    return true;
}

// The loaded object at a full path ("/Game/UI/Menu/UI_X.UI_X_C"), or null.
inline void* FindObject(const wchar_t* path) {
    return Fn<void* (*)(void*, void*, const wchar_t*, bool)>(va::StaticFindObject)(nullptr, nullptr, path, false);
}

// A widget blueprint's instance for the player (UUserWidget::CreateWidget), with its own widget tree built.
inline void* CreateWidget(void* pc, void* cls) {
    if (!pc || !cls) return nullptr;
    return Fn<void* (*)(void*, void*, uint64_t)>(va::CreateWidget)(pc, cls, 0);
}

// An engine widget (UButton, UScrollBox, ...) made inside a user widget's tree, like WidgetTree::ConstructWidget.
inline void* NewWidget(void* userWidget, uintptr_t staticClassVa) {
    void* tree = userWidget ? At<void*>(userWidget, off::UserWidget_WidgetTree) : nullptr;
    if (!tree) return nullptr;
    using NewObjectFn = void* (*)(void*, void*, uint64_t, uint32_t, void*, bool, void*, void*);
    return Fn<NewObjectFn>(va::NewObject)(tree, StaticClass(staticClassVa), 0, 0, nullptr, false, nullptr, nullptr);
}

// --- UWidget ---
inline void* Parent(void* w) {
    void* r = nullptr;
    Call(w, L"GetParent", &r);
    return r;
}
inline void SetVisibility(void* w, Visibility v) {
    uint8_t p = v;
    Call(w, L"SetVisibility", &p);
}
inline void RemoveFromParent(void* w) { Call(w, L"RemoveFromParent"); }
inline void* SlotOf(void* w) { return w ? At<void*>(w, off::Widget_Slot) : nullptr; }

// --- Panels: each returns the new child's slot ---
inline void* AddChild(void* panel, void* child) {
    struct {
        void* child;
        void* ret;
    } p{child, nullptr};
    Call(panel, L"AddChild", &p);
    return p.ret;
}
inline bool RemoveChild(void* panel, void* child) {
    struct {
        void* child;
        bool ret;
        uint8_t pad[7];
    } p{child, false, {}};
    Call(panel, L"RemoveChild", &p);
    return p.ret;
}
inline int ChildCount(void* panel) {
    int32_t n = 0;
    Call(panel, L"GetChildrenCount", &n);
    return n;
}
inline void* ChildAt(void* panel, int i) {
    struct {
        int32_t i;
        uint32_t pad;
        void* ret;
    } p{i, 0, nullptr};
    Call(panel, L"GetChildAt", &p);
    return p.ret;
}
inline void* AddToVerticalBox(void* box, void* child) {
    struct {
        void* child;
        void* ret;
    } p{child, nullptr};
    Call(box, L"AddChildToVerticalBox", &p);
    return p.ret;
}
inline void* AddToCanvas(void* canvas, void* child) {
    struct {
        void* child;
        void* ret;
    } p{child, nullptr};
    Call(canvas, L"AddChildToCanvas", &p);
    return p.ret;
}
inline void* AddToGrid(void* grid, void* child, int row, int column) {
    struct {
        void* child;
        int32_t row, column;
        void* ret;
    } p{child, row, column, nullptr};
    Call(grid, L"AddChildToUniformGrid", &p);
    return p.ret;
}
// UContentWidget (UButton, UBorder, USizeBox, ...)
inline void* SetContent(void* w, void* child) {
    struct {
        void* child;
        void* ret;
    } p{child, nullptr};
    Call(w, L"SetContent", &p);
    return p.ret;
}

// --- Slots ---
inline void SetPadding(void* slot, Margin m) { Call(slot, L"SetPadding", &m); }
inline void SetHAlign(void* slot, Align a) {
    uint8_t p = a;
    Call(slot, L"SetHorizontalAlignment", &p);
}
inline void SetVAlign(void* slot, Align a) {
    uint8_t p = a;
    Call(slot, L"SetVerticalAlignment", &p);
}

struct AnchorData {
    Margin offsets;
    float minX, minY, maxX, maxY;
    float alignX, alignY;
};
static_assert(sizeof(AnchorData) == 0x28);

inline void* AddToHorizontalBox(void* box, void* child) {
    struct {
        void* child;
        void* ret;
    } p{child, nullptr};
    Call(box, L"AddChildToHorizontalBox", &p);
    return p.ret;
}
inline void* AddToWrapBox(void* box, void* child) {
    struct {
        void* child;
        void* ret;
    } p{child, nullptr};
    Call(box, L"AddChildToWrapBox", &p);
    return p.ret;
}

// --- UTextBlock, through the game's own helper (FText from the string) ---
inline void SetText(void* textBlock, const wchar_t* text) {
    if (!textBlock) return;
    int n = int(wcslen(text)) + 1;
    TArray<wchar_t> s{const_cast<wchar_t*>(text), n, n};
    Fn<void (*)(void*, void*, const TArray<wchar_t>*)>(va::UI_SetTextFromString)(nullptr, textBlock, &s);
}

// A font (FSlateFontInfo) to pass to SetFont: a text block's own, at another size. Passed by value through
// ProcessEvent, so the copy here is never destroyed (the engine copies it properly).
struct Font {
    alignas(8) uint8_t bytes[off::FontInfo_Size];
};
inline bool FontOf(void* textBlock, int size, Font& out) {
    if (!textBlock) return false;
    std::memcpy(out.bytes, &At<uint8_t>(textBlock, off::TextBlock_Font), sizeof out.bytes);
    At<int32_t>(out.bytes, off::FontInfo_PointSize) = size;
    return true;
}
inline void SetFont(void* textBlock, const Font& f) {
    Font copy = f;
    Call(textBlock, L"SetFont", copy.bytes);
}
// UButton / UUserWidget: tints the content
inline void SetColorAndOpacity(void* w, LinearColor c) { Call(w, L"SetColorAndOpacity", &c); }

// FText from a string (UKismetTextLibrary::Conv_StringToText). The text is kept (never freed): pass it by value
// to blueprint functions (SetHintText, ...).
struct Text {
    alignas(8) uint8_t bytes[0x18];
};
inline bool MakeText(const wchar_t* s, Text& out) {
    static void* lib = FindObject(va::KismetTextLibraryPath);
    if (!lib) return false;
    int n = int(wcslen(s)) + 1;
    struct {
        TArray<wchar_t> in;
        Text ret;
    } p{{const_cast<wchar_t*>(s), n, n}, {}};
    if (!Call(lib, L"Conv_StringToText", &p)) return false;
    out = p.ret;
    return true;
}
// The string of an FText (UKismetTextLibrary::Conv_TextToString), e.g. what's typed in a UEditableTextBox.
inline std::wstring TextToString(const void* text) {
    static void* lib = FindObject(va::KismetTextLibraryPath);
    struct {
        Text in;
        TArray<wchar_t> ret;
    } p{};
    std::memcpy(p.in.bytes, text, sizeof p.in.bytes);  // a reference parameter: not copied or freed by the call
    if (!lib || !Call(lib, L"Conv_TextToString", &p)) return {};
    std::wstring out;
    for (int i = 0; p.ret.Data && i < p.ret.Num && p.ret.Data[i]; i++) out.push_back(p.ret.Data[i]);
    if (p.ret.Data) Fn<void (*)(void*)>(va::FMemory_Free)(p.ret.Data);
    return out;
}

}  // namespace crabmods::ui
