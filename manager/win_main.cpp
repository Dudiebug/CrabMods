// CrabMods Manager on Windows (and under wine): the window, Direct3D 11, and the loop that draws app.cpp.
//   --selftest [--report]   no window: writes CrabModsManager-selftest.txt
//   --screenshot file.png   draws once and saves it; changes nothing (with --page, --select, --demo)
//   --updated               started by the manager it replaced
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>

#include "app.hpp"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"
#include "platform.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace mgr::plat {
extern HWND g_window;
}

using namespace mgr;

namespace {

ID3D11Device* g_device;
ID3D11DeviceContext* g_ctx;
IDXGISwapChain* g_swap;
ID3D11RenderTargetView* g_rtv;
float g_scale = 1;

void CreateTarget() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}
void DropTarget() {
    if (g_rtv) g_rtv->Release(), g_rtv = nullptr;
}

bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    // A machine without a usable GPU driver still gets the software renderer.
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
        if (D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &got,
                                          &g_ctx) == S_OK) {
            CreateTarget();
            return true;
        }
    return false;
}

bool SaveScreenshot(const fs::path& path) {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (!back) return false;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    bool ok = false;
    if (g_device->CreateTexture2D(&d, nullptr, &staging) == S_OK) {
        g_ctx->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE m;
        if (g_ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m) == S_OK) {
            std::vector<uint8_t> px(size_t(d.Width) * d.Height * 4);
            for (UINT y = 0; y < d.Height; y++)
                memcpy(&px[size_t(y) * d.Width * 4], static_cast<uint8_t*>(m.pData) + size_t(y) * m.RowPitch, d.Width * 4);
            g_ctx->Unmap(staging, 0);
            ok = WriteFile(path, Png(int(d.Width), int(d.Height), px));
        }
        staging->Release();
    }
    back->Release();
    return ok;
}

LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(w, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_SIZE:
        if (g_device && wp != SIZE_MINIMIZED) {
            DropTarget();
            g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            CreateTarget();
        }
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {LONG(app::kMinWidth * g_scale), LONG(app::kMinHeight * g_scale)};
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

std::string Utf8(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    std::vector<std::string> args;
    int argc = 0;
    if (wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        for (int i = 1; i < argc; i++) args.push_back(Utf8(argv[i]));
        LocalFree(argv);
    }
    auto has = [&](const char* flag) { return std::find(args.begin(), args.end(), flag) != args.end(); };
    auto value = [&](const char* flag) -> std::string {
        auto it = std::find(args.begin(), args.end(), flag);
        return it != args.end() && it + 1 != args.end() ? *(it + 1) : "";
    };
    if (has("--selftest")) return app::SelfTest(has("--report"));

    app::Options o;
    o.screenshot = value("--screenshot");
    o.page = value("--page");
    o.select = value("--select");
    o.demo = value("--demo");
    o.updated = has("--updated");

    std::error_code ec;
    fs::path old = plat::ExePath();
    old += ".old";
    if (o.screenshot.empty()) fs::remove(old, ec);  // left by a self-update
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = CreateSolidBrush(RGB(22, 24, 28));
    wc.lpszClassName = L"CrabModsManager";
    RegisterClassW(&wc);
    HDC screen = GetDC(nullptr);
    g_scale = std::max(1.f, GetDeviceCaps(screen, LOGPIXELSX) / 96.f);
    ReleaseDC(nullptr, screen);
    std::wstring title = std::wstring(L"CrabMods Manager ") + fs::path(PathU8(kVersion)).wstring();
    HWND wnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               int(1240 * g_scale), int(800 * g_scale), nullptr, nullptr, inst, nullptr);
    plat::g_window = wnd;
    if (!CreateDevice(wnd)) {
        MessageBoxW(wnd, L"Direct3D 11 isn't available, so the window can't be drawn.", L"CrabMods Manager", MB_ICONERROR);
        return 1;
    }
    g_scale = std::max(g_scale, ImGui_ImplWin32_GetDpiScaleForHwnd(wnd));
    ShowWindow(wnd, o.screenshot.empty() ? show : SW_SHOWNOACTIVATE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(wnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    app::Init(o, g_scale);

    int busyFrames = 3;
    double started = ImGui::GetTime();
    for (bool running = true; running && !app::WantsQuit();) {
        bool animate = busyFrames > 0 || app::Animating() || !o.screenshot.empty();
        // Wakes for input, a finished job (WM_APP) or, at the latest, after a second for the app's timers.
        MsgWaitForMultipleObjects(0, nullptr, FALSE, animate ? 16 : 1000, QS_ALLINPUT);
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            busyFrames = 3;
        }
        if (!running) break;
        app::Tick();
        busyFrames--;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app::Frame();
        ImGui::Render();
        const float clear[4] = {22 / 255.f, 24 / 255.f, 28 / 255.f, 1};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (!o.screenshot.empty() && ImGui::GetTime() - started > 1.5) {
            SaveScreenshot(PathU8(o.screenshot));
            break;
        }
        g_swap->Present(1, 0);
    }

    app::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(wnd);
    return 0;
}
