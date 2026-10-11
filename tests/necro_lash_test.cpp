// The Soul Whip's lash simulation (crabmods/items/necro_lash.hpp): it hangs from the handle, keeps its length,
// follows the hand calmly, unrolls toward the target on a crack and settles again.
#include <cmath>
#include <cstdio>

#include "../crabmods/items/necro_lash.hpp"

using namespace crabmods::necro;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

// The whip's rig: 12 cord pieces and the tip (cm, at the crab's 1.8 scale).
static const float kLengths[13] = {14.4f, 14.4f, 14.4f, 14.4f, 14.4f, 14.4f, 14.4f,
                                   14.4f, 14.4f, 14.4f, 14.4f, 14.4f, 18.f};

static bool Finite(const Lash& l) {
    for (int i = 0; i <= l.Segments(); i++) {
        V3 p = l.Point(i);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
    }
    return true;
}
static float Total(const Lash& l) {
    float t = 0;
    for (int i = 0; i < l.Segments(); i++) t += Length(l.Point(i + 1) - l.Point(i));
    return t;
}

static void Hangs() {
    Lash l;
    l.Init(kLengths, 13);
    V3 anchor{0, 0, 100}, dir = Normal({0.287f, 0.2f, 0.937f});
    for (int f = 0; f < 240; f++) l.Step(anchor, dir, 1.f, 0.f, 1.f / 60);
    CHECK(Finite(l));
    float rest = 0;
    for (float x : kLengths) rest += x;
    CHECK(std::fabs(Total(l) - rest) < rest * 0.05f);
    CHECK(l.Point(13).z < anchor.z);       // the tip droops
    CHECK(l.Point(13).z >= 0.f);           // but not through the ground
    CHECK(Length(l.Point(0) - anchor) < 1e-3f);
    std::printf("  rest: tip at %.0f %.0f %.0f, length %.0f of %.0f\n", l.Point(13).x, l.Point(13).y, l.Point(13).z,
                Total(l), rest);
}

static void Cracks() {
    Lash l;
    l.Init(kLengths, 13);
    V3 anchor{0, 0, 100}, dir = Normal({0.287f, 0.2f, 0.937f});
    for (int f = 0; f < 120; f++) l.Step(anchor, dir, 1.f, 0.f, 1.f / 60);
    V3 target{1500, 300, 0};  // where the shot lands: 15 m out, on the ground
    V3 to = Normal(target - anchor);
    l.Crack(target);
    float best = -1, longest = 0, shortest = 1e9f;
    for (int f = 0; f < 30; f++) {
        l.Step(anchor, dir, 1.f, 0.f, 1.f / 60);
        best = std::fmax(best, Dot(Normal(l.Point(13) - anchor), to));
        longest = std::fmax(longest, Total(l));
        shortest = std::fmin(shortest, Total(l));
        CHECK(Finite(l));
        CHECK(Length(l.Point(0) - anchor) < 1e-3f);  // still in the hand
    }
    std::printf("  crack: tip lines up with the target to %.3f, lash %.0f..%.0f cm\n", best, shortest, longest);
    CHECK(best > 0.97f);                             // unrolled toward the target
    CHECK(longest < 191.5f && shortest > 150.f);     // never stretched (only bent along the ground)
    CHECK(!l.Striking());                            // let go after the strike
    for (int f = 0; f < 240; f++) l.Step(anchor, dir, 1.f, 0.f, 1.f / 60);
    CHECK(Finite(l));
    CHECK(std::fabs(Total(l) - 191.f) < 191.f * 0.05f);  // back to its length
}

static void Calm() {
    // The hand bobbing a centimetre at 8 Hz: the tip barely moves.
    Lash l;
    l.Init(kLengths, 13);
    V3 dir = Normal({0.287f, 0.2f, 0.937f});
    float lo = 1e9f, hi = -1e9f;
    for (int f = 0; f < 600; f++) {
        V3 anchor{0, 0, 100 + std::sin(f / 60.f * 8 * 6.2831853f)};
        l.Step(anchor, dir, 1.f, 0.f, 1.f / 60);
        if (f > 300) lo = std::fmin(lo, l.Point(13).z), hi = std::fmax(hi, l.Point(13).z);
    }
    std::printf("  bobbing hand: tip moves %.1f cm up and down\n", hi - lo);
    CHECK(hi - lo < 4.f);
}

static void FollowsAndTeleports() {
    Lash l;
    l.Init(kLengths, 13);
    V3 dir{0, 0, 1};
    for (int f = 0; f < 300; f++) l.Step({f * 10.f, 0, 100}, dir, 1.f, 0.f, 1.f / 60);  // running at 6 m/s
    CHECK(Finite(l));
    CHECK(l.Point(13).x < l.Point(0).x);  // trails behind
    l.Step({5000, 5000, 100}, dir, 1.f, 0.f, 1.f / 60);  // a teleport: laid out fresh
    CHECK(Length(l.Point(13) - V3{5000, 5000, 100}) < 200.f);
    // Huge and zero frame times don't break it.
    l.Step({5000, 5000, 100}, dir, 1.f, 0.f, 10.f);
    l.Step({5000, 5000, 100}, dir, 1.f, 0.f, 0.f);
    CHECK(Finite(l));
}

static void Rotations() {
    // FromAxes then Rotate gives the axes back.
    V3 x = Normal({1, 2, 3}), z = Normal(Cross(x, {0, 1, 0})), y = Cross(z, x);
    Q4 q = FromAxes(x, y, z);
    CHECK(Length(Rotate(q, {1, 0, 0}) - x) < 1e-4f);
    CHECK(Length(Rotate(q, {0, 1, 0}) - y) < 1e-4f);
    CHECK(Length(Rotate(q, {0, 0, 1}) - z) < 1e-4f);
    V3 x2{-1, 0, 0}, z2{0, 0, 1}, y2 = Cross(z2, x2);  // the trace <= 0 branch
    Q4 q2 = FromAxes(x2, y2, z2);
    CHECK(Length(Rotate(q2, {1, 0, 0}) - x2) < 1e-4f);
    CHECK(Length(Rotate(q2, {0, 1, 0}) - y2) < 1e-4f);
}

int main() {
    Hangs();
    Cracks();
    Calm();
    FollowsAndTeleports();
    Rotations();
    std::printf(g_failures ? "necro lash tests: %d failed\n" : "necro lash tests: all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
