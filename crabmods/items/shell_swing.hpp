// Shellbreaker's swing on the hook: the chain holds the crab at its length from the hook point, so a fall becomes
// an arc. Pure math (no game objects), so the tests can run it. Units are the game's: cm, seconds, UE axes (Z up).
//
// The game's own movement keeps doing gravity; each frame Step takes the crab's velocity and returns the one to
// launch it with. Past the chain's length the speed along the chain is replaced (never added to), so the pull back
// can't build up from frame to frame.
//
// From the ground, a hook point ahead and above would put the bottom of the arc under the floor, and the crab would
// only drag along it. So the chain shortens after it catches until the arc's bottom clears the floor, which lifts
// the crab, and a swing starts with at least entrySpeed forward.
//
// With keepHeight (the default) the swing holds the crab at the height it started at (at least `clearance` over
// the floor) while it passes under the hook point: its height is held, it keeps its speed across, and the movement
// keys speed it up or steer it. Once it is out on the far side as far as the chain reaches (its length when it
// caught), the chain holds it like a pendulum and it swings up, ready to let go. Reeling in raises that height.
#pragma once
#include <algorithm>
#include <cmath>

#include "shell_chain.hpp"

namespace crabmods::shell {

struct SwingTuning {
    float pump = 1200.f;        // cm/s per second the movement keys add across the chain
    float maxSpeed = 4000.f;    // cm/s: a swing never goes faster
    float catchLift = 0.85f;    // the chain shortens to this share of its length after it catches...
    float clearance = 150.f;    // ...or more, until the arc's bottom is this far above the floor
    float takeUp = 1500.f;      // cm/s it shortens at
    float entrySpeed = 1300.f;  // cm/s forward (toward the hook point), at least, as the swing starts
    float pullRate = 8.f;       // how fast the chain takes back an overshoot: this x the overshoot, per second
    float maxPull = 1500.f;     // cm/s, at most
    bool keepHeight = true;     // hold the crab at the height it started at
};

struct SwingStepResult {
    V3 velocity;
    bool taut = false;  // the chain is holding the crab (launch it with `velocity`)
};

struct Swing {
    V3 pin{};
    float length = 0.f, goal = 0.f;
    float holdZ = 0.f;  // keepHeight: the height the crab is held at
    float reach = 0.f;  // keepHeight: the chain's length when it caught, as long as it gets

    // The chain catches with the crab at `pos`, the floor under it at `floorZ` (the crab's own height when it stands
    // there). Returns the velocity to start the swing with.
    V3 Begin(V3 hookPoint, V3 pos, V3 vel, float floorZ, const SwingTuning& t = {}) {
        pin = hookPoint;
        length = std::max(150.f, Length(pos - pin));
        goal = std::max(150.f, std::min(length * t.catchLift, pin.z - floorZ - t.clearance));
        holdZ = std::min(std::max(pos.z, floorZ + t.clearance), pin.z - 150.f);
        reach = length;
        V3 flat{pin.x - pos.x, pin.y - pos.y, 0.f};
        if (Length(flat) < 1.f) return vel;
        V3 fwd = Normal(flat);
        float along = Dot(vel, fwd);
        if (along < t.entrySpeed) vel = vel + fwd * (t.entrySpeed - along);
        return vel;
    }

    // Reeling in (left click) shortens the chain further.
    void Reel(float cmPerSecond, float dt) {
        goal = std::max(150.f, goal - cmPerSecond * dt);
        holdZ = std::min(holdZ + cmPerSecond * dt, pin.z - 150.f);
    }

    // `input`: the way the player steers, flat, length 0..1.
    SwingStepResult Step(V3 pos, V3 vel, V3 input, float dt, const SwingTuning& t = {}) {
        if (t.keepHeight) {
            // Under the hook point, within the chain's reach at holdZ: hold the height, keep the speed across.
            float drop = pin.z - holdZ, flat = Length(V3{pos.x - pin.x, pos.y - pin.y, 0.f});
            float span = reach > drop ? std::sqrt(reach * reach - drop * drop) : 0.f;
            if (flat < span + 5.f) {  // (it starts right at the edge)
                length = std::max(150.f, Length(pos - pin));
                V3 v{vel.x, vel.y, std::clamp((holdZ - pos.z) * t.pullRate, -t.maxPull, t.maxPull)};
                if (Length(input) > 0.1f) v = v + V3{input.x, input.y, 0.f} * (t.pump * dt);
                float speed = Length(v);
                if (speed > t.maxSpeed) v = v * (t.maxSpeed / speed);
                return {v, true};
            }
            length = reach;  // out at the end of the chain: a pendulum from here
        } else if (length > goal) {
            length = std::max(goal, length - t.takeUp * dt);
        }
        SwingStepResult out{vel, false};
        V3 r = pos - pin;
        float dist = Length(r);
        if (dist < 1.f || dist < length) return out;  // slack: the crab moves on its own
        V3 n = r * (1.f / dist);
        V3 across = vel - n * Dot(vel, n);
        if (Length(input) > 0.1f) across = across + (input - n * Dot(input, n)) * (t.pump * dt);
        float pull = std::min((dist - length) * t.pullRate, t.maxPull);
        V3 v = across - n * pull;
        float speed = Length(v);
        if (speed > t.maxSpeed) v = v * (t.maxSpeed / speed);
        out.velocity = v;
        out.taut = true;
        return out;
    }
};

}  // namespace crabmods::shell
