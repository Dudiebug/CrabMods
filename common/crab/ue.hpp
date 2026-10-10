// Minimal UE 4.27 runtime helpers: rebasing, TArray, IsA, FName -> string.
#pragma once
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "game.hpp"

namespace crab {

inline uintptr_t g_exeBase = 0;

// Returns false if the running exe is not the build our addresses were taken from.
inline bool InitImage() {
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    g_exeBase = base;
    return nt->FileHeader.TimeDateStamp == kExpectedTimestamp;
}

inline uintptr_t Addr(uintptr_t staticVa) { return g_exeBase + (staticVa - kImageBase); }

template <class F>
inline F Fn(uintptr_t staticVa) { return reinterpret_cast<F>(Addr(staticVa)); }

template <class T>
inline T& At(void* obj, int offset) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + offset); }

template <class T>
struct TArray {
    T* Data;
    int32_t Num;
    int32_t Max;
};
static_assert(sizeof(TArray<int>) == 0x10);

// UStruct::IsChildOf fast path, exactly as the game inlines it: the class's StructBaseChainArray
// (+0x30) holds every ancestor's FStructBaseChain at index NumStructBasesInChainMinusOne (+0x38).
inline bool IsA(void* obj, void* cls) {
    if (!obj || !cls) return false;
    auto oc = At<char*>(obj, off::Object_Class);
    if (!oc) return false;
    int depth = At<int32_t>(cls, 0x38);
    if (At<int32_t>(oc, 0x38) < depth) return false;
    return At<char**>(oc, 0x30)[depth] == static_cast<char*>(cls) + 0x30;
}

inline void* StaticClass(uintptr_t getterVa) { return Fn<void* (*)()>(getterVa)(); }

// FNamePool (UE 4.27): FNameEntryAllocator { SRWLOCK; u32 CurrentBlock; u32 Cursor; u8* Blocks[] }.
// An entry id splits into block (high 16 bits) and 2-byte-stride offset (low 16 bits).
inline std::string NameToString(uint32_t comparisonIndex, uint32_t number) {
    auto blocks = reinterpret_cast<uint8_t**>(Addr(va::NamePoolData) + 0x10);
    uint8_t* block = blocks[comparisonIndex >> 16];
    if (!block) return "<?>";
    uint8_t* entry = block + 2 * (comparisonIndex & 0xFFFF);
    uint16_t header = *reinterpret_cast<uint16_t*>(entry);
    int len = header >> 6;
    std::string s;
    if (header & 1) {
        auto w = reinterpret_cast<wchar_t*>(entry + 2);
        for (int i = 0; i < len; i++) s.push_back(w[i] < 0x80 ? char(w[i]) : '?');
    } else {
        s.assign(reinterpret_cast<char*>(entry + 2), len);
    }
    if (number) s += "_" + std::to_string(number - 1);
    return s;
}

inline std::string ObjectName(void* obj) {
    if (!obj) return "None";
    return NameToString(At<uint32_t>(obj, off::Object_Name), At<uint32_t>(obj, off::Object_Name + 4));
}

inline std::string FStringToUtf8(const TArray<wchar_t>& s) {
    std::string out;
    for (int i = 0; i < s.Num && s.Data && s.Data[i]; i++) out.push_back(s.Data[i] < 0x80 ? char(s.Data[i]) : '?');
    return out;
}

}  // namespace crab
