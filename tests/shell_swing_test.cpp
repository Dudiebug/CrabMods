// Shellbreaker's swing (crabmods/items/shell_swing.hpp), run the way the game runs it: each frame the swing picks
// a launch velocity, then the game's movement adds gravity and moves the crab, and the floor stops it. Swings from
// standing and from running, under light and heavy gravity, at 30 to 144 frames a second: the crab is carried
// forward and up in an arc, the chain holds, and nothing builds up speed.
#include <cstdio>

#include "../crabmods/items/shell_swing.hpp"

using namespace crabmods::shell;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

struct Run {
    float maxSpeed = 0, maxZ = 0, minZ = 1e9f, forward = 0, worstStretch = 0, endSpeed = 0;
};

// The crab stands (or runs) on flat ground at z = 0 and hooks a point ahead and up; it holds for `seconds`.
static Run Simulate(float gravity, float fps, V3 startVel, V3 pin, float seconds, V3 input = {}, float startZ = 0.f,
                    bool keepHeight = true) {
    Run run;
    float dt = 1.f / fps;
    V3 pos{0, 0, startZ};
    Swing swing;
    SwingTuning tune;
    tune.keepHeight = keepHeight;
    V3 vel = swing.Begin(pin, pos, startVel, 0.f, tune);
    for (float t = 0; t < seconds; t += dt) {
        SwingStepResult s = swing.Step(pos, vel, input, dt, tune);
        vel = s.velocity;
        vel.z -= gravity * dt;  // the game's movement: gravity, then the move
        pos = pos + vel * dt;
        if (pos.z < 0.f) pos.z = 0.f, vel.z = std::max(vel.z, 0.f);
        run.maxSpeed = std::max(run.maxSpeed, Length(vel));
        run.maxZ = std::max(run.maxZ, pos.z);
        if (t > 0.4f) run.minZ = std::min(run.minZ, pos.z);
        if (t > 0.6f) run.worstStretch = std::max(run.worstStretch, Length(pos - pin) - swing.length);
    }
    run.forward = pos.x;
    run.endSpeed = Length(vel);
    return run;
}

int main() {
    const V3 sky{1300.f, 0.f, 1800.f};  // a point in the air ahead, like the sky anchor
    for (float g : {980.f, 1960.f, 3000.f})
        for (float fps : {30.f, 60.f, 144.f}) {
            // From standing: lifted off the ground and carried forward, never flung.
            Run a = Simulate(g, fps, {0, 0, 0}, sky, 1.0f, {}, 0.f, false);
            CHECK(a.maxZ > 150.f);
            CHECK(a.forward > 300.f);
            CHECK(a.maxSpeed < 3000.f);
            // Running: the speed goes into the arc.
            Run b = Simulate(g, fps, {1100, 0, 0}, sky, 1.0f, {}, 0.f, false);
            CHECK(b.forward > 900.f);
            CHECK(b.maxSpeed <= 4000.f + g / fps + 1.f);
            // Held a long time, steering: it swings back and forth but never runs away, and the chain holds.
            Run c = Simulate(g, fps, {1100, 0, 0}, sky, 12.f, {1, 0, 0}, 0.f, false);
            CHECK(c.maxSpeed <= 4000.f + g / fps + 1.f);
            CHECK(c.worstStretch < 200.f);
            if (fps == 60.f)
                std::printf("  g %4.0f: stand up %4.0f fwd %4.0f top %4.0f | run fwd %4.0f top %4.0f | held top %4.0f stretch %3.0f\n",
                            g, a.maxZ, a.forward, a.maxSpeed, b.forward, b.maxSpeed, c.maxSpeed, c.worstStretch);
        }
    // Keeping height: from the ground it lifts to the clearance and stays there; from the air it stays at the height
    // it started at; either way it keeps going forward under the hook point. Held on, it swings up at the far end.
    for (float g : {980.f, 1960.f, 3000.f})
        for (float fps : {30.f, 60.f, 144.f}) {
            Run a = Simulate(g, fps, {1100, 0, 0}, sky, 1.0f);
            CHECK(a.minZ > 100.f && a.maxZ < 200.f);
            CHECK(a.forward > 1100.f);
            Run b = Simulate(g, fps, {1100, 0, 0}, {1300.f, 0.f, 2400.f}, 1.0f, {}, 600.f);
            CHECK(b.minZ > 540.f && b.maxZ < 660.f);
            CHECK(b.forward > 1100.f);
            Run c = Simulate(g, fps, {1100, 0, 0}, {1300.f, 0.f, 2400.f}, 4.0f, {1, 0, 0}, 600.f);
            CHECK(c.maxZ > 800.f);
            CHECK(c.maxSpeed <= 4000.f + g / fps + 1.f);
            if (fps == 60.f)
                std::printf("  level g %4.0f: ground z %3.0f..%3.0f fwd %4.0f | air z %3.0f..%3.0f fwd %4.0f | held up to %4.0f top %4.0f%c",
                            g, a.minZ, a.maxZ, a.forward, b.minZ, b.maxZ, b.forward, c.maxZ, c.maxSpeed, 10);
        }
    // Slack: closer than the chain, the swing leaves the crab alone.
    {
        Swing s;
        s.pin = {0, 0, 1000}, s.length = s.goal = 2000.f;
        SwingTuning plain;
        plain.keepHeight = false;
        CHECK(!s.Step({0, 0, 0}, {5, 0, 0}, {}, 0.016f, plain).taut);
    }
    // The old bug: the pull is set each frame, not added, so held past the length it doesn't pile up.
    {
        Swing s;
        s.pin = {0, 0, 1000}, s.length = s.goal = 500.f;
        V3 vel{};
        SwingTuning plain;
        plain.keepHeight = false;
        for (int i = 0; i < 60; i++) vel = s.Step({0, 0, 0}, vel, {}, 1.f / 60, plain).velocity;
        CHECK(Length(vel) <= 1500.f + 1.f);
    }
    std::printf(g_failures ? "shell swing tests: %d FAILED\n" : "shell swing tests: all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
