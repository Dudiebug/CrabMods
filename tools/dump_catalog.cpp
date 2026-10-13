// Writes the item lists the manager reads, CrabMods.catalog.json and CrabSync.catalog.json, from the freshly built
// DLLs. Run by ./build.sh on the build machine; the files go up with each release, so the manager never has to
// load a mod DLL to see what is in it.
// Usage: dump_catalog <build folder>
#include <windows.h>

#include <cstdio>
#include <string>

#include "../manager/core.hpp"

static bool Write(const std::wstring& path, const std::string& text) {
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) return false;
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    return fclose(f) == 0 && ok;
}

int wmain(int argc, wchar_t** argv) {
    std::wstring dir = argc > 1 ? std::wstring(argv[1]) + L"\\" : L"";
    using Fn = const char* (*)();
    HMODULE mods = LoadLibraryW((dir + L"CrabMods.dll").c_str());
    HMODULE sync = LoadLibraryW((dir + L"CrabSync.dll").c_str());
    Fn catalog = mods ? reinterpret_cast<Fn>(GetProcAddress(mods, "CrabModsCatalog")) : nullptr;
    Fn ini = sync ? reinterpret_cast<Fn>(GetProcAddress(sync, "CrabSyncDefaultIni")) : nullptr;
    if (!catalog || !ini) {
        std::fprintf(stderr, "dump_catalog: CrabMods.dll or CrabSync.dll (or their exports) not found\n");
        return 1;
    }
    std::string json = catalog();
    auto parsed = mgr::ParseCatalog(json);
    if (!parsed || parsed->items.empty()) {
        std::fprintf(stderr, "dump_catalog: CrabMods' catalog doesn't parse\n");
        return 1;
    }
    if (!Write(dir + L"CrabMods.catalog.json", json) ||
        !Write(dir + L"CrabSync.catalog.json", "{\"ini\": " + mgr::JsonString(ini()) + "}\n")) {
        std::fprintf(stderr, "dump_catalog: could not write the catalogs\n");
        return 1;
    }
    std::printf("catalogs: %zu CrabMods items, CrabSync's settings\n", parsed->items.size());
    return 0;
}
