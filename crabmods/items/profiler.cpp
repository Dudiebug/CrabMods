// Profiler: where the game's frame time goes, to find what a performance mod should fix. On the game thread it
// times every actor tick and component tick (by class), every script call through ProcessEvent (blueprint
// events and functions, RPCs; by function), garbage collection, and every event CrabMods passes to its items.
// Every few seconds the log gets the frame times, a per-frame breakdown of the game thread, and the classes and
// functions that cost the most; frames that take long (hitches) are logged as they happen, with what took the
// longest in them. The HUD can show fps and the game thread's time. Local only: the host's settings never switch it
// on or off. Its hooks are only installed when it is on at start (the timing itself costs a little).
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../profiler.hpp"

namespace crabmods {

namespace {

enum Cat : uint8_t { kActor, kComponent, kScript, kGC, kItem, kCats };
constexpr const char* kCatTitle[kCats] = {"actor ticks, by class", "component ticks, by class",
                                          "script calls (ProcessEvent: blueprints, RPCs), by function",
                                          "garbage collection", "CrabMods items, by event"};

struct Key {
    uint8_t cat;
    const void* a;  // UClass*, UFunction*, a GC step's name, Item*
    const void* b;  // the item's event
    bool operator==(const Key& o) const { return cat == o.cat && a == o.a && b == o.b; }
};
struct KeyHash {
    size_t operator()(const Key& k) const {
        return (std::hash<const void*>()(k.a) * 31 + std::hash<const void*>()(k.b)) * 7 + k.cat;
    }
};

// One class / function / event: its name (taken when first seen: a class may be gone by the report).
struct Row {
    std::string name;
    uint64_t calls = 0;
    int64_t total = 0, max = 0;  // ns
};

double Ms(int64_t ns) { return double(ns) / 1e6; }
double Us(int64_t ns) { return double(ns) / 1e3; }

void* ClassOf(void* obj) { return At<void*>(obj, off::Object_Class); }

// The game thread's time in one frame, by what took it.
struct FrameParts {
    int64_t tick = 0;                  // UGameEngine::Tick, CrabMods' frame events included
    int64_t part[kCats] = {};          // outermost calls only, so nothing counts twice within a category
    uint32_t count[kCats] = {};
};

// One stretch of time (the report's, or the HUD's).
struct Window {
    int64_t start = 0;
    std::vector<float> frames;  // ms between frames
    FrameParts sum;
    int64_t tickMax = 0;

    void Clear(int64_t now) {
        start = now;
        frames.clear();
        sum = {};
        tickMax = 0;
    }
    void Frame(float ms, const FrameParts& f) {
        frames.push_back(ms);
        sum.tick += f.tick;
        tickMax = std::max(tickMax, f.tick);
        for (int c = 0; c < kCats; c++) {
            sum.part[c] += f.part[c];
            sum.count[c] += f.count[c];
        }
    }
};

float Percentile(std::vector<float> v, float p) {
    if (v.empty()) return 0.f;
    size_t n = std::min(v.size() - 1, size_t(std::lround(p * float(v.size() - 1))));
    std::nth_element(v.begin(), v.begin() + n, v.end());
    return v[n];
}

class Profiler : public Item {
public:
    Profiler() : Item("profiler", 995) {}

    std::string DefaultIni() const override {
        return R"(# Profiler: measures where the game's frame time goes, to find what slows it down. Every few seconds the log
# (CrabMods.log) gets the frame times, how the game thread's time splits between actor ticks, component ticks,
# garbage collection and the rest, and the classes and blueprint functions that cost the most. Long frames (hitches)
# are logged as they happen. Off by default: the timing costs a little. Takes effect when the game starts. Only
# your own game; the host's settings don't change it.
enabled = false

# Seconds between reports in the log.
reportSeconds = 10
# How many entries each list in a report shows (actor classes, component classes, script functions, items).
top = 15
# Log every frame that takes at least this many milliseconds, with what took the longest in it. 0: never.
hitchMs = 50
# Time script calls (ProcessEvent: blueprint events and functions, RPCs). The most frequent calls, so the most
# costly to time.
scripts = true
# Show fps and the game thread's time on the HUD, after the version.
hud = true
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") prof::g_on = on_ = ParseBool(v);
        else if (k == "reportSeconds") reportSeconds_ = std::fmax(ParseFloat(v), 1.f);
        else if (k == "top") top_ = std::max(0, int(ParseFloat(v)));
        else if (k == "hitchMs") hitchMs_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "scripts") scripts_ = ParseBool(v);
        else if (k == "hud") hud_ = ParseBool(v);
    }
    bool Enabled() const override { return on_; }
    bool LocalSetting(const std::string&) const override { return true; }
    std::string Title() const override { return "Profiler"; }
    std::string Describe() const override {
        if (!on_) return "off";
        return "report every " + Num(reportSeconds_) + " s, top " + std::to_string(top_) +
               (hitchMs_ > 0.f ? ", hitches from " + Num(hitchMs_) + " ms" : "") + (scripts_ ? ", scripts" : "") +
               (hud_ ? ", on the HUD" : "");
    }

    void InstallHooks() override;

    // --- Timing (game thread only; calls on other threads are counted) ---
    struct Span {
        int64_t start;
        bool outer;
    };
    bool Timing() {
        if (!gameThread_) return false;
        if (GetCurrentThreadId() == gameThread_) return true;
        offThread_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    Span Begin(Cat c) { return {prof::Now(), depth_[c]++ == 0}; }
    void End(const Span& s, Cat c, const void* a, const void* b = nullptr) {
        int64_t ns = prof::Now() - s.start;
        depth_[c]--;
        auto [it, fresh] = rows_.try_emplace(Key{c, a, b});
        Row& r = it->second;
        if (fresh) r.name = Name(c, a, b);
        r.calls++;
        r.total += ns;
        r.max = std::max(r.max, ns);
        if (s.outer) {
            frame_.part[c] += ns;
            frame_.count[c]++;
        }
        // The frame's slowest calls, for the hitch log.
        if (ns > slow_[2].ns) {
            slow_[2] = {ns, &r};
            std::sort(std::begin(slow_), std::end(slow_), [](auto& x, auto& y) { return x.ns > y.ns; });
        }
    }

    // CrabMods' own dispatch (main.cpp).
    void Enter() {
        if (depth_[kItem]++ == 0) itemStart_ = prof::Now();
    }
    void Leave() {
        if (--depth_[kItem] > 0) return;
        frame_.part[kItem] += prof::Now() - itemStart_;
        frame_.count[kItem]++;
    }
    void Add(Item* item, const char* event, int64_t ns) {
        auto [it, fresh] = rows_.try_emplace(Key{kItem, item, event});
        Row& r = it->second;
        if (fresh) r.name = std::string(item->Section()) + "." + event;
        r.calls++;
        r.total += ns;
        r.max = std::max(r.max, ns);
    }

    void EndFrame(int64_t tickStart, int64_t tickEnd) {
        if (!gameThread_) gameThread_ = GetCurrentThreadId();
        int64_t now = prof::Now();
        frame_.tick = tickEnd - tickStart;
        if (!lastFrame_) {
            lastFrame_ = tickStart;
            report_.Clear(now);
            hudWin_.Clear(now);
            ResetFrame();
            return;
        }
        float ms = float(Ms(tickStart - lastFrame_));
        lastFrame_ = tickStart;
        report_.Frame(ms, frame_);
        hudWin_.Frame(ms, frame_);
        if (hitchMs_ > 0.f && ms >= hitchMs_ && hitches_++ < kMaxHitches) LogHitch(ms);
        ResetFrame();

        if (Ms(now - hudWin_.start) >= 1000.0) {
            hudText_ = HudSummary(hudWin_, now);
            hudWin_.Clear(now);
        }
        if (Ms(now - report_.start) >= reportSeconds_ * 1000.0) Report(now);
    }

    std::wstring HudText() const { return on_ && hud_ ? hudText_ : std::wstring(); }
    bool Scripts() const { return scripts_; }

private:
    static constexpr int kMaxHitches = 20;  // logged per report

    static std::string Name(Cat c, const void* a, const void*) {
        switch (c) {
        case kActor:
        case kComponent: return ObjectName(const_cast<void*>(a));
        case kScript: {
            void* fn = const_cast<void*>(a);
            return ObjectName(At<void*>(fn, off::Object_Outer)) + "." + ObjectName(fn);
        }
        case kGC: return static_cast<const char*>(a);
        default: return "?";
        }
    }

    void ResetFrame() {
        frame_ = {};
        for (auto& s : slow_) s = {};
    }

    void LogHitch(float ms) {
        int64_t parts = 0;
        for (int c = 0; c < kItem; c++) parts += frame_.part[c] * (c != kScript);  // scripts run inside the rest
        std::string slow;
        for (auto& s : slow_)
            if (s.row) {
                char b[160];
                snprintf(b, sizeof b, "%s%s %.2f ms", slow.empty() ? "" : ", ", s.row->name.c_str(), Ms(s.ns));
                slow += b;
            }
        CRAB_LOG("profiler: hitch, frame %.1f ms; game thread %.1f ms: actors %.2f, components %.2f, GC %.2f, "
                 "other %.2f (scripts %.2f, CrabMods %.2f within these); slowest: %s",
                 ms, Ms(frame_.tick), Ms(frame_.part[kActor]), Ms(frame_.part[kComponent]), Ms(frame_.part[kGC]),
                 Ms(std::max<int64_t>(0, frame_.tick - parts)), Ms(frame_.part[kScript]), Ms(frame_.part[kItem]),
                 slow.empty() ? "nothing timed" : slow.c_str());
    }

    static std::wstring HudSummary(Window& w, int64_t now) {
        double sec = Ms(now - w.start) / 1000.0;
        size_t n = w.frames.size();
        if (!n || sec <= 0.0) return {};
        float worst = *std::max_element(w.frames.begin(), w.frames.end());
        wchar_t buf[128];
        swprintf(buf, 128, L"%.0f fps, game %.1f ms, worst frame %.0f ms", double(n) / sec, Ms(w.sum.tick) / double(n),
                 double(worst));
        return buf;
    }

    void Report(int64_t now) {
        Window& w = report_;
        size_t n = w.frames.size();
        double sec = Ms(now - w.start) / 1000.0;
        if (n) {
            double sum = 0.0;
            for (float f : w.frames) sum += f;
            float worst = *std::max_element(w.frames.begin(), w.frames.end());
            double frame = sum / double(n), tick = Ms(w.sum.tick) / double(n);
            auto per = [&](Cat c) { return Ms(w.sum.part[c]) / double(n); };
            auto cnt = [&](Cat c) { return double(w.sum.count[c]) / double(n); };
            double other = tick - per(kActor) - per(kComponent) - per(kGC);
            CRAB_LOG("profiler: === %.1f s, %zu frames, %.1f fps; frame ms avg %.2f, median %.2f, 99%% %.2f, worst %.2f",
                     sec, n, double(n) / sec, frame, Percentile(w.frames, 0.5f), Percentile(w.frames, 0.99f), worst);
            CRAB_LOG("profiler: game thread (UGameEngine::Tick) avg %.2f ms, worst %.2f ms; the rest of the frame, "
                     "%.2f ms, is rendering, waiting for the GPU and the frame cap",
                     tick, Ms(w.tickMax), std::max(0.0, frame - tick));
            CRAB_LOG("profiler: per frame: actor ticks %.2f ms (%.0f actors), component ticks %.2f ms (%.0f), garbage "
                     "collection %.3f ms, other engine work %.2f ms; within these: scripts %.2f ms (%.0f calls), "
                     "CrabMods %.3f ms",
                     per(kActor), cnt(kActor), per(kComponent), cnt(kComponent), per(kGC), std::max(0.0, other),
                     per(kScript), cnt(kScript), per(kItem));
            uint64_t off = offThread_.exchange(0);
            if (off) CRAB_LOG("profiler: %llu ticks or calls ran on other threads (not timed)", (unsigned long long)off);
            if (hitches_ > kMaxHitches) CRAB_LOG("profiler: %d more hitches not logged", hitches_ - kMaxHitches);
        }
        if (top_ > 0 && n) {
            std::vector<const std::pair<const Key, Row>*> rows[kCats];
            for (auto& kv : rows_) rows[kv.first.cat].push_back(&kv);
            for (int c = 0; c < kCats; c++) {
                auto& v = rows[c];
                if (v.empty()) continue;
                std::sort(v.begin(), v.end(), [](auto a, auto b) { return a->second.total > b->second.total; });
                CRAB_LOG("profiler: %s (of %zu):", kCatTitle[c], v.size());
                for (size_t i = 0; i < v.size() && int(i) < top_; i++) {
                    const Row& r = v[i]->second;
                    CRAB_LOG("profiler:   %-56s %7.3f ms/frame %8.1f calls/frame %9.2f us avg %9.2f us worst",
                             r.name.c_str(), Ms(r.total) / double(n), double(r.calls) / double(n),
                             Us(r.total) / double(r.calls), Us(r.max));
                }
            }
        }
        rows_.clear();
        for (auto& s : slow_) s = {};
        hitches_ = 0;
        w.Clear(now);
    }

    bool on_ = false, hud_ = true, scripts_ = true;
    float reportSeconds_ = 10.f, hitchMs_ = 50.f;
    int top_ = 15;

    DWORD gameThread_ = 0;  // set by the first frame; nothing is timed before
    std::atomic<uint64_t> offThread_{0};
    std::unordered_map<Key, Row, KeyHash> rows_;  // since the last report
    int depth_[kCats] = {};
    int64_t itemStart_ = 0;  // the outermost dispatch's
    FrameParts frame_;
    struct Slow {
        int64_t ns = 0;
        const Row* row = nullptr;
    } slow_[3];
    int hitches_ = 0;
    int64_t lastFrame_ = 0;
    Window report_, hudWin_;
    std::wstring hudText_;
};

Profiler g_item;

// --- Hooks ---

using TickFn = void (*)(void*, float, int, int, void*);
TickFn oActorTick, oComponentTick;
using ProcessEventFn = void (*)(void*, void*, void*);
ProcessEventFn oProcessEvent;
using CollectFn = void (*)(uint32_t, bool);
CollectFn oCollect;
using PurgeFn = void (*)(bool, float);
PurgeFn oPurge;

void HkActorTick(void* fn, float dt, int type, int thread, void* ev) {
    void* target = At<void*>(fn, off::TickFunction_Target);
    if (!target || !g_item.Timing()) return oActorTick(fn, dt, type, thread, ev);
    void* cls = ClassOf(target);  // before the tick: it may destroy the actor
    auto s = g_item.Begin(kActor);
    oActorTick(fn, dt, type, thread, ev);
    g_item.End(s, kActor, cls);
}
void HkComponentTick(void* fn, float dt, int type, int thread, void* ev) {
    void* target = At<void*>(fn, off::TickFunction_Target);
    if (!target || !g_item.Timing()) return oComponentTick(fn, dt, type, thread, ev);
    void* cls = ClassOf(target);
    auto s = g_item.Begin(kComponent);
    oComponentTick(fn, dt, type, thread, ev);
    g_item.End(s, kComponent, cls);
}
void HkProcessEvent(void* obj, void* fn, void* params) {
    if (!fn || !g_item.Timing()) return oProcessEvent(obj, fn, params);
    auto s = g_item.Begin(kScript);
    oProcessEvent(obj, fn, params);
    g_item.End(s, kScript, fn);
}
void HkCollect(uint32_t keepFlags, bool fullPurge) {
    if (!g_item.Timing()) return oCollect(keepFlags, fullPurge);
    auto s = g_item.Begin(kGC);
    oCollect(keepFlags, fullPurge);
    g_item.End(s, kGC, fullPurge ? "CollectGarbage (full purge)" : "CollectGarbage");
}
void HkPurge(bool useTimeLimit, float timeLimit) {
    if (!g_item.Timing()) return oPurge(useTimeLimit, timeLimit);
    auto s = g_item.Begin(kGC);
    oPurge(useTimeLimit, timeLimit);
    g_item.End(s, kGC, "IncrementalPurgeGarbage");
}

void Profiler::InstallHooks() {
    if (!on_) return;
    Hook(va::FActorTickFunction_ExecuteTick, &HkActorTick, &oActorTick, "FActorTickFunction::ExecuteTick");
    Hook(va::FActorComponentTickFunction_ExecuteTick, &HkComponentTick, &oComponentTick,
         "FActorComponentTickFunction::ExecuteTick");
    Hook(va::CollectGarbageInternal, &HkCollect, &oCollect, "CollectGarbageInternal");
    Hook(va::IncrementalPurgeGarbage, &HkPurge, &oPurge, "IncrementalPurgeGarbage");
    if (scripts_) Hook(va::UObject_ProcessEvent, &HkProcessEvent, &oProcessEvent, "UObject::ProcessEvent");
}

}  // namespace

namespace prof {
void Enter() { g_item.Enter(); }
void Leave() { g_item.Leave(); }
void Add(Item* item, const char* event, int64_t ns) { g_item.Add(item, event, ns); }
void EndFrame(int64_t tickStart, int64_t tickEnd) { g_item.EndFrame(tickStart, tickEnd); }
std::wstring HudText() { return g_item.HudText(); }
}  // namespace prof

}  // namespace crabmods
