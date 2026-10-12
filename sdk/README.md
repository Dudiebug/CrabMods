# CrabSDK

A C++ SDK for writing native (DLL) mods for Crab Champions. It covers every reflected type in the game: 2181 classes, 2130 structs and 1009 enums, plus wrappers for 6209 `UFunction`s, game and engine alike.

It was generated from the static reflection tables in `CrabChampions-Win64-Shipping.exe` (UE 4.27, PE timestamp `0x6921D2F7`). See [`re/`](../re/README.md) for how. It contains no game code or assets.

## What you get

- **Exact layouts.** Every member sits at the offset the game uses. All 27,859 offsets are checked by `static_assert` in [`LayoutChecks.cpp`](LayoutChecks.cpp), and they pass under both MSVC and MinGW (clang) layout rules. A member whose type can't be reproduced in C++ becomes a byte array of the right size, so the layout never drifts.
- **`StaticClass()` and `GetDefaultObj()`** on every class, calling the game's own getter.
- **Function wrappers.** Each reflected function is a C++ method that fills its parameter block and calls `ProcessEvent`. Out parameters and return values come back as normal C++ values. Net functions are sent over the network like they are in the game.
- **Native addresses** in the comments: each native function's exec thunk and, where known, its C++ implementation, ready for hooking. Class headers list the vtable and constructor.
- **Core types** in `Basic.hpp`: `UObject`, `UClass`, `UFunction`, `FName`, `FString`, `TArray`, delegates, soft and weak pointers, `FindObject`, `IsA`, and `FMemory`.

## Using it

Add this folder to your include path and compile these files into your mod:

- `Basic.cpp` (always)
- `CrabChampions_functions.cpp` (the game's functions)
- `Engine_functions.cpp`, only if you call engine functions (it's large, about 10 s to compile)

```cpp
#include "CrabSDK.hpp"
using namespace CrabSDK;

void OnTick() {
    if (!IsExpectedBuild()) return;  // a game update changes every address: do nothing

    auto* gs = UObject::FindObject<ACrabGS>(L"/Script/CrabChampions.Default__CrabGS");
    ACrabPS* ps = /* from your hook, e.g. a player's PlayerState */ nullptr;
    if (ps) {
        ps->DamageMultiplier *= 1.1f;                     // members are plain fields
        bool isPlayer = ps->IsA<ACrabPS>();
    }
}
```

- Requires C++20. Works with MSVC, clang-cl and MinGW (llvm-mingw); the repo's mods build with llvm-mingw.
- `<windows.h>` can be included before or after the SDK. Eight names that clash with its macros get a trailing `_` (for example `PlaySound_`, `DrawText_`).
- A name that clashes with a C++ keyword or a type also gets a trailing `_`.
- `FString` values you build from a literal point at your memory. Pass them to the game as parameters, but never store them into a game object.
- `TMap` and `TSet` have the right size but no accessors.

## Layout notes

- The headers are wrapped in `#pragma pack(push, 8)` because UE builds with `/Zp8`. This matters for 18 base types (`ACrabC`, `USceneComponent`-derived components, `FRootMotionSource`) and their roughly 65 subclasses: there, MSVC places a subclass's first members inside the base class's 16-byte alignment padding. Those bases are emitted up to the point where the subclass begins. Their `sizeof` assert accepts both compilers' answers, and member offsets are identical under both.
- Classes have no C++ virtual functions. The vtable is the `VTable` pointer on `UObject`, and `ProcessEvent` is called through slot `0x220`.

## Regenerating

After a game update, rerun the scripts in [`re/`](../re/README.md#regenerating), then:

```bash
python re/scripts/emit_full_sdk.py      # rewrites everything here except Basic.hpp, Basic.cpp and this README
```

Then compile `LayoutChecks.cpp` to verify the result:

```bash
x86_64-w64-mingw32-clang++ -std=c++20 -fsyntax-only sdk/LayoutChecks.cpp
```

Also update `Build::TimeDateStamp` and the addresses in `Basic.hpp`, and `re/scripts/win_macros.txt` if a new name clashes with `<windows.h>`.
