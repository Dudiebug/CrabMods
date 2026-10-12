// CrabSDK core implementation. Compile it into your mod together with the *_functions.cpp files you use.
#include "Basic.hpp"

#include <intrin.h>

namespace CrabSDK {

namespace {
template <class F>
F Fn(uintptr_t staticVa) { return reinterpret_cast<F>(Rebase(staticVa)); }

void AppendUtf8(std::string& out, const wchar_t* s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint32 c = static_cast<uint16>(s[i]);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < len) {
            uint32 lo = static_cast<uint16>(s[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
        }
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
}
}  // namespace

uintptr_t GetImageBase() {
    // PEB (gs:0x60) -> ImageBaseAddress (+0x10): the exe, whichever module this code lives in.
    static uintptr_t base = *reinterpret_cast<uintptr_t*>(__readgsqword(0x60) + 0x10);
    return base;
}

bool IsExpectedBuild() {
    auto base = reinterpret_cast<const uint8*>(GetImageBase());
    int32 peOffset = *reinterpret_cast<const int32*>(base + 0x3C);       // IMAGE_DOS_HEADER::e_lfanew
    uint32 stamp = *reinterpret_cast<const uint32*>(base + peOffset + 8);  // IMAGE_FILE_HEADER::TimeDateStamp
    return stamp == Build::TimeDateStamp;
}

void* FMemory::Malloc(size_t size, uint32 alignment) { return Realloc(nullptr, size, alignment); }
void* FMemory::Realloc(void* ptr, size_t size, uint32 alignment) {
    return Fn<void* (*)(void*, size_t, uint32)>(Offsets::FMemory_Realloc)(ptr, size, alignment);
}
void FMemory::Free(void* ptr) { Fn<void (*)(void*)>(Offsets::FMemory_Free)(ptr); }

std::string FString::ToString() const {
    std::string out;
    if (Data) AppendUtf8(out, Data, std::wcslen(Data));
    return out;
}

FName::FName(const wchar_t* s) {
    Fn<FName* (*)(FName*, const wchar_t*, int)>(Offsets::FName_Ctor)(this, s, 1);  // FNAME_Add
}

// FNamePool (UE 4.27): FNameEntryAllocator { SRWLOCK; uint32 CurrentBlock; uint32 CurrentByteCursor;
// uint8* Blocks[] }. An entry id is block (high 16 bits) and a 2-byte-stride offset (low 16 bits); each entry
// starts with a uint16 header: bit 0 wide, bits 6.. length.
std::string FName::ToString() const {
    auto blocks = reinterpret_cast<uint8**>(Rebase(Offsets::NamePoolData) + 0x10);
    uint8* block = blocks[static_cast<uint32>(ComparisonIndex) >> 16];
    if (!block) return "<invalid>";
    uint8* entry = block + 2 * (static_cast<uint32>(ComparisonIndex) & 0xFFFF);
    uint16 header = *reinterpret_cast<uint16*>(entry);
    int len = header >> 6;
    std::string s;
    if (header & 1) AppendUtf8(s, reinterpret_cast<wchar_t*>(entry + 2), len);
    else s.assign(reinterpret_cast<char*>(entry + 2), len);
    if (Number) s += "_" + std::to_string(Number - 1);
    return s;
}

UClass* UObject::StaticClass() {
    static UClass* cls = static_cast<UClass*>(FindObject(L"/Script/CoreUObject.Object"));
    return cls;
}

UClass* UInterface::StaticClass() {
    static UClass* cls = static_cast<UClass*>(FindObject(L"/Script/CoreUObject.Interface"));
    return cls;
}

UObject* UObject::FindObject(const wchar_t* path, UClass* cls) {
    return Fn<UObject* (*)(UClass*, UObject*, const wchar_t*, bool)>(Offsets::StaticFindObject)(cls, nullptr, path, false);
}

std::string UObject::GetFullName() const {
    std::string path = GetName();
    for (UObject* o = Outer; o; o = o->Outer) path = o->GetName() + "." + path;
    return (Class ? Class->GetName() : std::string("None")) + " " + path;
}

bool UStruct::IsChildOf(const UStruct* base) const {
    if (!base) return false;
    // The FStructBaseChain fast path, as the game inlines it: the chain array holds every ancestor's
    // FStructBaseChain subobject (at +0x30), indexed by depth.
    int32 depth = base->NumStructBasesInChainMinusOne;
    auto chain = reinterpret_cast<const uint8* const*>(StructBaseChainArray);
    return depth <= NumStructBasesInChainMinusOne &&
           chain[depth] == reinterpret_cast<const uint8*>(base) + 0x30;
}

bool UObject::IsA(const UClass* cls) const { return Class && cls && Class->IsChildOf(cls); }

void UObject::ProcessEvent(UFunction* fn, void* params) const {
    using PE = void (*)(const UObject*, UFunction*, void*);
    reinterpret_cast<PE>(VTable[Offsets::ProcessEventIndex])(this, fn, params);
}

UFunction* UClass::FindFunction(const wchar_t* name) const {
    FName n(name);
    return Fn<UFunction* (*)(const UClass*, FName, int)>(Offsets::UClass_FindFunctionByName)(this, n, 1);
}

UFunction* FindFunctionCached(UFunction*& slot, const wchar_t* path) {
    if (!slot) slot = static_cast<UFunction*>(UObject::FindObject(path));
    return slot;
}

}  // namespace CrabSDK
