#!/usr/bin/env bash
# Builds the loader (xinput1_3.dll), CrabSync.dll, CrabMods.dll, CrabModsManager.exe and the tests with llvm-mingw.
# Usage: ./build.sh [test] [package] [install]
#   test     run the ledger and manager tests (with wine on Linux)
#   package  build/CrabSync-<version>.zip and build/CrabMods-<version>.zip, the release downloads (the raw DLLs
#            and CrabModsManager.exe are uploaded to the releases too: the manager installs from them)
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

# version_res <name> <version> <description> <file name>: a VERSIONINFO resource object, build/obj/<name>.res.o.
# The manager reads it to tell which version is installed.
version_res() {
    local v=$2 c
    c="$(echo "$v" | tr . ,),0"
    cat >"$OUT/obj/$1.rc" <<EOF
1 VERSIONINFO
FILEVERSION $c
PRODUCTVERSION $c
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName", "Dudiebug"
      VALUE "FileDescription", "$3"
      VALUE "FileVersion", "$v"
      VALUE "OriginalFilename", "$4"
      VALUE "ProductName", "CrabMods"
      VALUE "ProductVersion", "$v"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
EOF
    [ -n "${5:-}" ] && echo "$5" >>"$OUT/obj/$1.rc"
    "$TC/x86_64-w64-mingw32-windres" --include-dir manager "$OUT/obj/$1.rc" -O coff -o "$OUT/obj/$1.res.o"
}
version_of() { sed -n 's/.*kVersion\[\] = "\(.*\)";/\1/p' "$1"; }
version_res loader 1.0.0 "Crab Champions mod loader" xinput1_3.dll
version_res CrabSync "$(version_of crabsync/main.cpp)" "CrabSync for Crab Champions" CrabSync.dll
version_res CrabMods "$(version_of crabmods/main.cpp)" "CrabMods for Crab Champions" CrabMods.dll
# The manager also carries its font (resource 2).
version_res manager "$(version_of manager/platform.hpp)" "CrabMods Manager" CrabModsManager.exe \
    "2 RCDATA \"$PWD/manager/fonts/Roboto-Medium.ttf\""

# The mod loader: the game imports xinput1_3.dll, so Windows loads it, and it loads the other DLLs beside it.
"$CXX" "${CXXFLAGS[@]}" loader/xinput_proxy.cpp loader/xinput1_3.def "$OUT/obj/loader.res.o" "${LDFLAGS[@]}" \
    -o "$OUT/xinput1_3.dll"

# Each mod writes its own ini (with defaults) and log next to itself on first launch.
"$CXX" "${CXXFLAGS[@]}" crabsync/main.cpp crabsync/ledger.cpp crabsync/game_inv.cpp "${MH_OBJS[@]}" "$OUT/obj/CrabSync.res.o" \
    "${LDFLAGS[@]}" -o "$OUT/CrabSync.dll"
"$CXX" "${CXXFLAGS[@]}" crabmods/main.cpp crabmods/items/*.cpp "${MH_OBJS[@]}" "$OUT/obj/CrabMods.res.o" \
    "${LDFLAGS[@]}" -o "$OUT/CrabMods.dll"

# The manager: a Windows program (it runs under wine too), drawn with Dear ImGui on Direct3D 11.
IMGUI=third_party/imgui
IMGUI_OBJS=()
for f in "$IMGUI"/imgui.cpp "$IMGUI"/imgui_draw.cpp "$IMGUI"/imgui_tables.cpp "$IMGUI"/imgui_widgets.cpp \
    "$IMGUI"/backends/imgui_impl_win32.cpp "$IMGUI"/backends/imgui_impl_dx11.cpp; do
    o="$OUT/obj/imgui_$(basename "${f%.cpp}").o"
    [ "$o" -nt "$f" ] || "$CXX" -std=c++20 -O2 -I"$IMGUI" -c "$f" -o "$o"
    IMGUI_OBJS+=("$o")
done
"$CXX" "${CXXFLAGS[@]}" -I"$IMGUI" manager/main.cpp "${IMGUI_OBJS[@]}" "$OUT/obj/manager.res.o" -municode -mwindows -static -s \
    -lwinhttp -lcomctl32 -lshell32 -lole32 -lversion -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lgdi32 -limm32 -o "$OUT/CrabModsManager.exe"

"$CXX" "${CXXFLAGS[@]}" tests/ledger_test.cpp crabsync/ledger.cpp -static -o "$OUT/ledger_test.exe"
"$CXX" "${CXXFLAGS[@]}" tests/manager_test.cpp -static -o "$OUT/manager_test.exe"
echo "built: $OUT/xinput1_3.dll $OUT/CrabSync.dll $OUT/CrabMods.dll $OUT/CrabModsManager.exe (+ tests)"

# package_mod <Name> <source dir>: the loader, the mod, its README and the licenses.
package_mod() {
    local name=$1 dir=$2 ver pkg
    ver=$(version_of "$dir/main.cpp")
    pkg="$OUT/$name-$ver"
    rm -rf "$pkg" "$pkg.zip"
    mkdir -p "$pkg"
    cp "$OUT/xinput1_3.dll" "$OUT/$name.dll" LICENSE "$pkg/"
    cp "packaging/$name-README.txt" "$pkg/README.txt"
    cp third_party/minhook/LICENSE.txt "$pkg/MinHook-LICENSE.txt"
    (cd "$pkg" && zip -q -X "../$name-$ver.zip" *)
    echo "packaged: $pkg.zip"
}

for arg in "$@"; do
    case "$arg" in
    test)
        # Runs the Windows test binary with Proton's wine in a throwaway prefix.
        WINE="${WINE:-$(ls -d "$HOME"/.local/share/Steam/compatibilitytools.d/GE-Proton*/files/bin/wine64 2>/dev/null | tail -1 || true)}"
        WINE="${WINE:-wine}"
        export WINEPREFIX="${WINEPREFIX:-$PWD/$OUT/wineprefix}" WINEDEBUG=-all
        "$WINE" "$OUT/ledger_test.exe"
        "$WINE" "$OUT/manager_test.exe"
        ;;
    package)
        package_mod CrabSync crabsync
        package_mod CrabMods crabmods
        ;;
    install)
        GAME="${CRAB_GAME:-$HOME/.local/share/Steam/steamapps/common/Crab Champions}/CrabChampions/Binaries/Win64"
        # Overwriting DLLs the running game has loaded crashes it.
        if command -v pgrep >/dev/null && pgrep -f CrabChampions-Win64-Shipping >/dev/null; then
            echo "Crab Champions is running: close it, then install again" >&2
            exit 1
        fi
        cp "$OUT/xinput1_3.dll" "$OUT/CrabSync.dll" "$OUT/CrabMods.dll" "$GAME/"
        # CrabMods replaces the old standalone Arsenal.dll (its settings move into CrabMods.ini on first launch).
        rm -f "$GAME/Arsenal.dll" "$GAME/Arsenal.log"
        echo "installed to $GAME (each mod's ini and log appear there on first launch)"
        ;;
    esac
done
