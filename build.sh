#!/usr/bin/env bash
# Builds the loader (xinput1_3.dll), CrabSync.dll and the ledger tests with llvm-mingw.
# Usage: ./build.sh [test] [package] [install]
#   test     run the ledger tests (with wine on Linux)
#   package  build/CrabSync-<version>.zip, the release download
#   install  copy the DLLs into the game (set CRAB_GAME to the game folder if it is not the Steam default)
set -euo pipefail
cd "$(dirname "$0")"

TC="${LLVM_MINGW:-$HOME/.local/opt/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64}/bin"
CXX="$TC/x86_64-w64-mingw32-clang++"
CC="$TC/x86_64-w64-mingw32-clang"
OUT=build
mkdir -p "$OUT/obj"

CXXFLAGS=(-std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter -Icommon -Ithird_party/minhook/include)
LDFLAGS=(-shared -static -static-libgcc -static-libstdc++ -Wl,--kill-at -s)

# MinHook (C)
MH=third_party/minhook/src
MH_OBJS=()
for f in "$MH/buffer.c" "$MH/hook.c" "$MH/trampoline.c" "$MH/hde/hde64.c"; do
    o="$OUT/obj/mh_$(basename "${f%.c}").o"
    "$CC" -O2 -c "$f" -o "$o"
    MH_OBJS+=("$o")
done

# The mod loader: the game imports xinput1_3.dll, so Windows loads it, and it loads the other DLLs beside it.
"$CXX" "${CXXFLAGS[@]}" loader/xinput_proxy.cpp loader/xinput1_3.def "${LDFLAGS[@]}" -o "$OUT/xinput1_3.dll"

# Each mod writes its own ini (with defaults) and log next to itself on first launch.
"$CXX" "${CXXFLAGS[@]}" crabsync/main.cpp crabsync/ledger.cpp crabsync/game_inv.cpp "${MH_OBJS[@]}" \
    "${LDFLAGS[@]}" -o "$OUT/CrabSync.dll"

"$CXX" "${CXXFLAGS[@]}" tests/ledger_test.cpp crabsync/ledger.cpp -static -o "$OUT/ledger_test.exe"
echo "built: $OUT/xinput1_3.dll $OUT/CrabSync.dll $OUT/ledger_test.exe"

for arg in "$@"; do
    case "$arg" in
    test)
        # Runs the Windows test binary with Proton's wine in a throwaway prefix.
        WINE="${WINE:-$(ls -d "$HOME"/.local/share/Steam/compatibilitytools.d/GE-Proton*/files/bin/wine64 2>/dev/null | tail -1 || true)}"
        WINE="${WINE:-wine}"
        export WINEPREFIX="${WINEPREFIX:-$PWD/$OUT/wineprefix}" WINEDEBUG=-all
        "$WINE" "$OUT/ledger_test.exe"
        ;;
    package)
        VER=$(sed -n 's/.*kVersion\[\] = "\(.*\)";/\1/p' crabsync/main.cpp)
        PKG="$OUT/CrabSync-$VER"
        rm -rf "$PKG" "$PKG.zip"
        mkdir -p "$PKG"
        cp "$OUT/xinput1_3.dll" "$OUT/CrabSync.dll" packaging/README.txt LICENSE "$PKG/"
        cp third_party/minhook/LICENSE.txt "$PKG/MinHook-LICENSE.txt"
        (cd "$PKG" && zip -q -X "../CrabSync-$VER.zip" *)
        echo "packaged: $PKG.zip"
        ;;
    install)
        GAME="${CRAB_GAME:-$HOME/.local/share/Steam/steamapps/common/Crab Champions}/CrabChampions/Binaries/Win64"
        cp "$OUT/xinput1_3.dll" "$GAME/"
        cp "$OUT/CrabSync.dll" "$GAME/"
        echo "installed to $GAME (CrabSync.ini and CrabSync.log appear there on first launch)"
        ;;
    esac
done
