// CrabSDK: the UE 4.27 core types every generated header builds on. Hand-written (the generator does not touch
// this file); layouts follow UE 4.27 and are checked against the game's reflection data where it has them.
// All addresses are for CrabChampions-Win64-Shipping.exe, PE timestamp 0x6921D2F7 (see re/REPORT.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace CrabSDK {

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

// --- The running game image -------------------------------------------------------------------------------------

namespace Build {
constexpr uintptr_t ImageBase = 0x140000000;    // the exe's preferred base; every address in the SDK is relative to it
constexpr uint32 TimeDateStamp = 0x6921D2F7;    // the build the SDK was generated from
}  // namespace Build

// The exe's base address at runtime, read from the PEB so the SDK needs no <windows.h>.
uintptr_t GetImageBase();

// Converts a static address (as printed in the SDK, e.g. 0x1410d96f0) to the running process.
inline uintptr_t Rebase(uintptr_t staticVa) { return GetImageBase() + (staticVa - Build::ImageBase); }

// True when the running exe is the build the SDK was generated from. Check it before touching anything else:
// after a game update every address and offset may be wrong.
bool IsExpectedBuild();

// --- Engine addresses the core types use -----------------------------------------------------------------------

namespace Offsets {
constexpr uintptr_t NamePoolData = 0x144282980;       // FNamePool
constexpr uintptr_t FName_Ctor = 0x140ed46e0;         // FName*(FName*, const wchar_t*, EFindName)
constexpr uintptr_t StaticFindObject = 0x1410d96f0;   // UObject*(UClass*, UObject* Outer, const wchar_t*, bool Exact)
constexpr uintptr_t NewObject = 0x14078b370;          // see common/crab/game.hpp for the parameters
constexpr uintptr_t FMemory_Realloc = 0x140e017e0;    // void*(void*, size_t, uint32 Alignment)
constexpr uintptr_t FMemory_Free = 0x140df0dc0;       // void(void*)
constexpr uintptr_t GEngine = 0x1443fe458;            // UEngine*
constexpr uintptr_t UClass_FindFunctionByName = 0x140f66a80;  // UFunction*(UClass*, FName, int IncludeSuper)
constexpr int ProcessEventIndex = 0x220 / 8;          // UObject vtable slot of ProcessEvent
}  // namespace Offsets

namespace FMemory {
void* Malloc(size_t size, uint32 alignment = 0);
void* Realloc(void* ptr, size_t size, uint32 alignment = 0);
void Free(void* ptr);
}  // namespace FMemory

// --- Containers --------------------------------------------------------------------------------------------------

template <class T>
class TArray {
public:
    T* Data = nullptr;
    int32 NumElements = 0;
    int32 MaxElements = 0;

    int32 Num() const { return NumElements; }
    int32 Max() const { return MaxElements; }
    bool IsValidIndex(int32 i) const { return Data && i >= 0 && i < NumElements; }
    T& operator[](int32 i) { return Data[i]; }
    const T& operator[](int32 i) const { return Data[i]; }
    T* begin() { return Data; }
    T* end() { return Data + NumElements; }
    const T* begin() const { return Data; }
    const T* end() const { return Data + NumElements; }
};

// A UE string. One you build from a literal points at your memory: only hand it to the game as a read-only
// parameter, never store it into a game object.
class FString : public TArray<wchar_t> {
public:
    FString() = default;
    FString(const wchar_t* s) {
        if (s && *s) {
            Data = const_cast<wchar_t*>(s);
            NumElements = MaxElements = static_cast<int32>(std::wcslen(s)) + 1;
        }
    }
    const wchar_t* c_str() const { return Data ? Data : L""; }
    std::wstring ToWString() const { return Data ? std::wstring(Data) : std::wstring(); }
    std::string ToString() const;  // UTF-8
};

// TMap and TSet keep their exact size (0x50) but no accessors: their hashing is engine-internal.
template <class K, class V>
struct alignas(8) TMap { uint8 Opaque[0x50]; };
template <class E>
struct alignas(8) TSet { uint8 Opaque[0x50]; };

template <class A, class B>
struct TPair {
    A Key;
    B Value;
};

// --- Names and text -----------------------------------------------------------------------------------------------

class FName {
public:
    int32 ComparisonIndex = 0;
    int32 Number = 0;

    FName() = default;
    explicit FName(const wchar_t* s);  // finds or adds the name
    bool IsNone() const { return ComparisonIndex == 0 && Number == 0; }
    std::string ToString() const;
    bool operator==(const FName& o) const { return ComparisonIndex == o.ComparisonIndex && Number == o.Number; }
    bool operator!=(const FName& o) const { return !(*this == o); }
};

// FText is a shared pointer plus flags; read it through UKismetTextLibrary.
struct alignas(8) FText { uint8 Opaque[0x18]; };

// --- Object references ----------------------------------------------------------------------------------------------

class UObject;
class UField;
class UStruct;
class UClass;
class UFunction;
class UScriptStruct;
class UEnum;
class UInterface;
class UPackage;
class UPackageMap;

struct FWeakObjectPtr {
    int32 ObjectIndex = 0;
    int32 ObjectSerialNumber = 0;
};

template <class T>
struct TWeakObjectPtr : FWeakObjectPtr {};

template <class T>
struct TSubclassOf {
    UClass* Class = nullptr;
    TSubclassOf() = default;
    TSubclassOf(UClass* c) : Class(c) {}
    operator UClass*() const { return Class; }
};

template <class T>
struct TScriptInterface {
    UObject* ObjectPointer = nullptr;
    void* InterfacePointer = nullptr;
};

template <class T>
struct TSoftObjectPtr {
    FWeakObjectPtr WeakPtr;
    int32 TagAtLastTest;
    FName AssetPathName;     // FSoftObjectPath
    FString SubPathString;
};

template <class T>
struct TSoftClassPtr : TSoftObjectPtr<T> {};

template <class T>
struct TLazyObjectPtr {
    FWeakObjectPtr WeakPtr;
    int32 TagAtLastTest;
    uint32 Guid[4];
};

// FProperty (UE 4.27 properties are FFields, not UObjects) is only ever used through pointers here.
struct FField;
struct FProperty;

struct FFieldPath {
    FField* ResolvedField;
    TWeakObjectPtr<UStruct> ResolvedOwner;
    TArray<FName> Path;
};

template <class E>
struct TEnumAsByte {
    uint8 Value;
    operator E() const { return static_cast<E>(Value); }
    TEnumAsByte& operator=(E e) { Value = static_cast<uint8>(e); return *this; }
};

// --- Delegates ------------------------------------------------------------------------------------------------------

struct FScriptDelegate {
    FWeakObjectPtr Object;
    FName FunctionName;
};

struct FMulticastInlineDelegate {
    TArray<FScriptDelegate> InvocationList;
};

// Sparse delegates store their bindings outside the object; this byte only says whether any exist.
struct FMulticastSparseDelegate {
    uint8 bIsBound;
};

// --- UObject core (UE 4.27 layouts) -----------------------------------------------------------------------------------

class UObject {
public:
    void** VTable;            // 0x00
    int32 ObjectFlags;        // 0x08 EObjectFlags
    int32 InternalIndex;      // 0x0C index in GUObjectArray
    UClass* Class;            // 0x10
    FName Name;               // 0x18
    UObject* Outer;           // 0x20

    static UClass* StaticClass();

    // A loaded object by full path, e.g. L"/Script/CrabChampions.Default__CrabPS". Null if it is not loaded.
    static UObject* FindObject(const wchar_t* path, UClass* cls = nullptr);
    template <class T>
    static T* FindObject(const wchar_t* path) { return static_cast<T*>(FindObject(path, T::StaticClass())); }

    std::string GetName() const { return Name.ToString(); }
    std::string GetFullName() const;   // "Class Outer.Outer.Name"
    bool IsA(const UClass* cls) const;
    template <class T>
    bool IsA() const { return IsA(T::StaticClass()); }
    bool IsDefaultObject() const { return (ObjectFlags & 0x10) != 0; }  // RF_ClassDefaultObject

    // Calls a UFunction through the object's ProcessEvent (vtable slot 0x220). This is what every generated
    // function wrapper does; a Net function called this way is sent over the network.
    void ProcessEvent(UFunction* fn, void* params) const;
};

class UField : public UObject {
public:
    UField* Next;                            // 0x28
};

class UStruct : public UField {
public:
    // FStructBaseChain (the fast IsA path).
    UStruct** StructBaseChainArray;          // 0x30
    int32 NumStructBasesInChainMinusOne;     // 0x38
    uint8 Pad_3C[0x4];
    UStruct* SuperStruct;                    // 0x40
    UField* Children;                        // 0x48 functions
    FField* ChildProperties;                 // 0x50 properties
    int32 PropertiesSize;                    // 0x58
    int32 MinAlignment;                      // 0x5C
    TArray<uint8> Script;                    // 0x60
    FProperty* PropertyLink;                 // 0x70
    FProperty* RefLink;                      // 0x78
    FProperty* DestructorLink;               // 0x80
    FProperty* PostConstructLink;            // 0x88
    TArray<UObject*> ScriptAndPropertyObjectReferences;  // 0x90
    void* UnresolvedScriptProperties;        // 0xA0
    void* UnversionedSchema;                 // 0xA8

    bool IsChildOf(const UStruct* base) const;
};

class UFunction : public UStruct {
public:
    uint32 FunctionFlags;                    // 0xB0 EFunctionFlags
    uint8 NumParms;                          // 0xB4
    uint8 Pad_B5[0x1];
    uint16 ParmsSize;                        // 0xB6
    uint16 ReturnValueOffset;                // 0xB8
    uint16 RPCId;                            // 0xBA
    uint16 RPCResponseId;                    // 0xBC
    uint8 Pad_BE[0x2];
    FProperty* FirstPropertyToInit;          // 0xC0
    UFunction* EventGraphFunction;           // 0xC8
    int32 EventGraphCallOffset;              // 0xD0
    uint8 Pad_D4[0x4];
    void* Func;                              // 0xD8 the native exec thunk
};

class UClass : public UStruct {
public:
    void* ClassConstructor;                  // 0xB0
    void* ClassVTableHelperCtorCaller;       // 0xB8
    void* ClassAddReferencedObjects;         // 0xC0
    uint32 ClassUnique : 31;                 // 0xC8
    uint32 bCooked : 1;
    uint32 ClassFlags;                       // 0xCC EClassFlags
    uint64 ClassCastFlags;                   // 0xD0
    UClass* ClassWithin;                     // 0xD8
    UObject* ClassGeneratedBy;               // 0xE0
    FName ClassConfigName;                   // 0xE8
    uint8 ClassReps[0x10];                   // 0xF0 TArray<FRepRecord>
    TArray<UField*> NetFields;               // 0x100
    int32 FirstOwnedClassRep;                // 0x110
    uint8 Pad_114[0x4];
    UObject* ClassDefaultObject;             // 0x118
    uint8 Pad_120[0x110];                    // 0x120 SparseClassData, FuncMap, interfaces, ...

    // The function by name on this class or a parent; null if there is none.
    UFunction* FindFunction(const wchar_t* name) const;
};

class UScriptStruct : public UStruct {
public:
    uint32 StructFlags;                      // 0xB0
    bool bPrepareCppStructOpsCompleted;      // 0xB4
    uint8 Pad_B5[0x3];
    void* CppStructOps;                      // 0xB8
};

class UEnum : public UField {
public:
    FString CppType;                         // 0x30
    TArray<TPair<FName, int64>> Names;       // 0x40
    int32 CppForm;                           // 0x50
    uint8 Pad_54[0x4];
    void* EnumDisplayNameFn;                 // 0x58
};

class UInterface : public UObject {
public:
    static UClass* StaticClass();
};

// Not reflected by the game; only used through pointers.
class UPackage : public UObject {};
class UPackageMap : public UObject {};

static_assert(sizeof(UObject) == 0x28);
static_assert(sizeof(UField) == 0x30);
static_assert(sizeof(UStruct) == 0xB0);
static_assert(sizeof(UFunction) == 0xE0);
static_assert(sizeof(UClass) == 0x230);
static_assert(sizeof(UScriptStruct) == 0xC0);
static_assert(sizeof(UEnum) == 0x60);
static_assert(sizeof(FString) == 0x10);
static_assert(sizeof(FName) == 0x8);
static_assert(sizeof(FText) == 0x18);
static_assert(sizeof(TMap<int, int>) == 0x50);
static_assert(sizeof(TSoftObjectPtr<UObject>) == 0x28);
static_assert(sizeof(TLazyObjectPtr<UObject>) == 0x1C);
static_assert(sizeof(FFieldPath) == 0x20);
static_assert(sizeof(FScriptDelegate) == 0x10);
static_assert(sizeof(FMulticastInlineDelegate) == 0x10);

// Used by the generated wrappers: finds a UFunction once by its full path and caches it in `slot`.
UFunction* FindFunctionCached(UFunction*& slot, const wchar_t* path);

}  // namespace CrabSDK
