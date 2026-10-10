// Mod loader. The game imports xinput1_3.dll, so Windows loads this with the game. It forwards every
// XInput export to the system DLL (controllers keep working) and loads every other DLL
// in the game exe's folder (CrabSync.dll, future mods), so mods are installed by dropping them in.
// Under Proton launch with: WINEDLLOVERRIDES="xinput1_3=n,b" %command%
#include <windows.h>

#include <string>

namespace {

HMODULE g_real = nullptr;
FARPROC g_procs[12] = {};

// Export ordinals of the real xinput1_3.dll, in g_procs order.
constexpr WORD kOrdinals[12] = {2, 3, 4, 5, 6, 7, 8, 100, 101, 102, 103, 0};

void LoadReal() {
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    std::wstring p(path, n);
    p += L"\\xinput1_3.dll";
    g_real = LoadLibraryW(p.c_str());
    if (!g_real) return;
    for (int i = 0; kOrdinals[i]; i++) g_procs[i] = GetProcAddress(g_real, MAKEINTRESOURCEA(kOrdinals[i]));
}

DWORD WINAPI LoadMods(LPVOID) {
    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe, n);
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*.dll").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (lstrcmpiW(fd.cFileName, L"xinput1_3.dll") == 0) continue;  // this loader
        LoadLibraryW((dir + fd.cFileName).c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

template <int I, class R, class... A>
R Forward(R fail, A... args) {
    if (!g_procs[I]) return fail;
    return reinterpret_cast<R(WINAPI*)(A...)>(g_procs[I])(args...);
}

}  // namespace

extern "C" {
// Argument types are opaque here; every XInput function takes DWORD/pointer/BOOL arguments.
__declspec(dllexport) DWORD WINAPI XInputGetState(DWORD a, void* b) { return Forward<0>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b); }
__declspec(dllexport) DWORD WINAPI XInputSetState(DWORD a, void* b) { return Forward<1>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b); }
__declspec(dllexport) DWORD WINAPI XInputGetCapabilities(DWORD a, DWORD b, void* c) { return Forward<2>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b, c); }
__declspec(dllexport) void WINAPI XInputEnable(BOOL a) {
    if (g_procs[3]) reinterpret_cast<void(WINAPI*)(BOOL)>(g_procs[3])(a);
}
__declspec(dllexport) DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD a, GUID* b, GUID* c) { return Forward<4>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b, c); }
__declspec(dllexport) DWORD WINAPI XInputGetBatteryInformation(DWORD a, BYTE b, void* c) { return Forward<5>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b, c); }
__declspec(dllexport) DWORD WINAPI XInputGetKeystroke(DWORD a, DWORD b, void* c) { return Forward<6>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b, c); }
__declspec(dllexport) DWORD WINAPI XInputGetStateEx(DWORD a, void* b) { return Forward<7>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b); }
__declspec(dllexport) DWORD WINAPI XInputWaitForGuideButton(DWORD a, DWORD b, void* c) { return Forward<8>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a, b, c); }
__declspec(dllexport) DWORD WINAPI XInputCancelGuideButtonWait(DWORD a) { return Forward<9>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a); }
__declspec(dllexport) DWORD WINAPI XInputPowerOffController(DWORD a) { return Forward<10>(DWORD(ERROR_DEVICE_NOT_CONNECTED), a); }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        LoadReal();
        // Mods hook game code; loading them off the loader lock keeps DllMain trivial.
        CloseHandle(CreateThread(nullptr, 0, LoadMods, nullptr, 0, nullptr));
    }
    return TRUE;
}
