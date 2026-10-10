#!/usr/bin/env bash
# Builds the loader (xinput1_3.dll) and Arsenal.dll with llvm-mingw.
# Usage: ./build.sh [package] [install]
#   package  build/Arsenal-<version>.zip, the release download
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

# The mod loader, shared with CrabSync: the game imports xinput1_3.dll, so Windows loads it, and it loads
# the other DLLs beside it.
"$CXX" "${CXXFLAGS[@]}" loader/xinput_proxy.cpp loader/xinput1_3.def "${LDFLAGS[@]}" -o "$OUT/xinput1_3.dll"

# Writes its own ini (with defaults) and log next to itself on first launch.
"$CXX" "${CXXFLAGS[@]}" arsenal/main.cpp "${MH_OBJS[@]}" "${LDFLAGS[@]}" -o "$OUT/Arsenal.dll"
echo "built: $OUT/xinput1_3.dll $OUT/Arsenal.dll"

for arg in "$@"; do
    case "$arg" in
    package)
        VER=$(sed -n 's/.*kVersion\[\] = "\(.*\)";/\1/p' arsenal/main.cpp)
        PKG="$OUT/Arsenal-$VER"
        rm -rf "$PKG" "$PKG.zip"
        mkdir -p "$PKG"
        cp "$OUT/xinput1_3.dll" "$OUT/Arsenal.dll" packaging/README.txt LICENSE "$PKG/"
        cp third_party/minhook/LICENSE.txt "$PKG/MinHook-LICENSE.txt"
        (cd "$PKG" && zip -q -X "../Arsenal-$VER.zip" *)
        echo "packaged: $PKG.zip"
        ;;
    install)
        GAME="${CRAB_GAME:-$HOME/.local/share/Steam/steamapps/common/Crab Champions}/CrabChampions/Binaries/Win64"
        cp "$OUT/xinput1_3.dll" "$GAME/"
        cp "$OUT/Arsenal.dll" "$GAME/"
        echo "installed to $GAME (Arsenal.ini and Arsenal.log appear there on first launch)"
        ;;
    esac
done
