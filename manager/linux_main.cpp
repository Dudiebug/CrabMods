// CrabMods Manager on Linux: the window (SDL2 with OpenGL 3) and the loop that draws app.cpp. The same options as
// on Windows:
//   --selftest [--report]   no window: writes CrabModsManager-selftest.txt
//   --screenshot file.png   draws once and saves it; changes nothing (with --page, --select, --demo)
//   --updated               started by the manager it replaced
#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>

#include "app.hpp"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "platform.hpp"

namespace mgr::plat {
extern SDL_Window* g_window;
extern uint32_t g_wakeEvent;
}  // namespace mgr::plat

using namespace mgr;

namespace {

bool SaveScreenshot(SDL_Window* w, const fs::path& path) {
    int width = 0, height = 0;
    SDL_GL_GetDrawableSize(w, &width, &height);
    std::vector<uint8_t> px(size_t(width) * height * 4), flipped(px.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // OpenGL's rows start at the bottom.
    for (int y = 0; y < height; y++)
        std::copy_n(&px[size_t(height - 1 - y) * width * 4], size_t(width) * 4, &flipped[size_t(y) * width * 4]);
    return WriteFile(path, Png(width, height, flipped));
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
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

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "CrabMods Manager: can't open a window: %s\n", SDL_GetError());
        return 1;
    }
    plat::g_wakeEvent = SDL_RegisterEvents(1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // SDL reports the desktop's scale as the display's DPI against 96.
    float ddpi = 0, scale = 1;
    if (SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr) == 0 && ddpi > 0) scale = std::clamp(ddpi / 96.f, 1.f, 3.f);
    std::string title = std::string("CrabMods Manager ") + kVersion;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (o.screenshot.empty() ? 0 : SDL_WINDOW_HIDDEN);
    SDL_Window* wnd = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, int(1240 * scale),
                                       int(800 * scale), flags);
    SDL_GLContext gl = wnd ? SDL_GL_CreateContext(wnd) : nullptr;
    if (!gl) {
        std::fprintf(stderr, "CrabMods Manager: OpenGL 3.2 isn't available: %s\n", SDL_GetError());
        return 1;
    }
    plat::g_window = wnd;
    SDL_SetWindowMinimumSize(wnd, int(app::kMinWidth * scale), int(app::kMinHeight * scale));
    SDL_GL_MakeCurrent(wnd, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(wnd, gl);
    ImGui_ImplOpenGL3_Init("#version 150");
    app::Init(o, scale);

    int busyFrames = 3;
    double started = ImGui::GetTime();
    for (bool running = true; running && !app::WantsQuit();) {
        bool animate = busyFrames > 0 || app::Animating() || !o.screenshot.empty();
        // Wakes for input, a finished job (the wake event) or, at the latest, after a second for the app's timers.
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, animate ? 16 : 1000)) {
            do {
                ImGui_ImplSDL2_ProcessEvent(&e);
                if (e.type == SDL_QUIT) running = false;
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == SDL_GetWindowID(wnd))
                    running = false;
                busyFrames = 3;
            } while (SDL_PollEvent(&e));
        }
        if (!running) break;
        app::Tick();
        busyFrames--;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        app::Frame();
        ImGui::Render();
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(wnd, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(22 / 255.f, 24 / 255.f, 28 / 255.f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!o.screenshot.empty() && ImGui::GetTime() - started > 1.5) {
            SaveScreenshot(wnd, PathU8(o.screenshot));
            break;
        }
        SDL_GL_SwapWindow(wnd);
    }

    app::Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    plat::g_window = nullptr;
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(wnd);
    SDL_Quit();
    return 0;
}
