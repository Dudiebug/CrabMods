// Shellbreaker's chain, simulated: a string of points from the handle's eye to the blade's eye under gravity and a
// little air drag (Verlet, fixed 1/240 s substeps, the Soul Whip's lash way). A chain only resists stretching: it
// goes slack and piles up, never pushes. Its length can change at any time (it pays out for the hook and reels
// back in).
//
// The swings aren't animations. A swing pulls the blade toward a moving target (and the links behind it toward the
// line from the hand, as a fast swing throws the chain straight); the physics does the rest. When a swing ends the
// chain keeps its momentum, so the blade follows through, wraps and rebounds on its own. For the hook, the blade
// can be pinned to a point (flying out, or stuck in a wall).
//
// Pure math (no game objects). Units are the game's: cm, seconds, UE axes (Z up).
#pragma once
#include <algorithm>
#include <cmath>

#include "necro_lash.hpp"

namespace crabmods::shell {

using necro::Cross;
using necro::Dot;
using necro::Length;
using necro::Normal;
using necro::V3;

class Chain {
public:
    static constexpr int kSegments = 24;

    int Segments() const { return kSegments; }
    const V3& Point(int i) const { return p_[i]; }
    bool Live() const { return live_; }
    float RestLength() const { return length_; }
    // The blade's direction: along the last links.
    V3 BladeDir() const { return Normal(p_[kSegments] - p_[kSegments - 2], dir_); }
    V3 TipVelocity() const { return tipVel_; }

    // Laid out fresh, hanging from the handle.
    void Reset(V3 hand, V3 dir, float length) {
        length_ = std::max(length, 1.f);
        dir_ = Normal(dir);
        float seg = length_ / kSegments;
        p_[0] = o_[0] = hand;
        for (int i = 0; i < kSegments; i++) {
            float g = std::min(1.f, 2.f * float(i + 1) / kSegments);
            V3 d = Normal(dir_ * (1.f - g) + V3{0, 0, -1} * g, {0, 0, -1});
            p_[i + 1] = o_[i + 1] = p_[i] + d * seg;
        }
        hand_ = hand;
        acc_ = 0.f;
        pulling_ = pinned_ = false;
        live_ = true;
    }

    void SetLength(float length) { length_ = std::max(length, 1.f); }

    // Swings: the blade is drawn toward target at rate (1/s: how much of the gap closes, as 1 - e^(-rate t)), the
    // links behind it toward the line from the hand by body (0..1) of that.
    void Pull(V3 target, float rate, float body) {
        pulling_ = true;
        target_ = target;
        rate_ = rate;
        body_ = body;
    }
    void Release() { pulling_ = false; }

    // The hook: the blade's eye held at a point (it can move: the blade flying out).
    void Pin(V3 at) {
        pinned_ = true;
        pin_ = at;
    }
    void Unpin() { pinned_ = false; }
    bool Pinned() const { return pinned_; }

    // hand/dir: the handle's eye and direction; scale: the crab's; floorZ: the ground.
    void Step(V3 hand, V3 dir, float floorZ, float dt) {
        if (!live_ || Length(hand - hand_) > 600.f) Reset(hand, dir, length_);
        dt = std::clamp(dt, 0.f, 0.1f);
        dir_ = Normal(dir, dir_);
        V3 from = hand_;
        hand_ = hand;
        V3 lastTip = p_[kSegments];
        float start = acc_, done = 0.f;
        acc_ += dt;
        int steps = 0;
        while (acc_ >= kSub) {
            acc_ -= kSub;
            done += kSub;
            if (++steps > 48) continue;  // a long hitch: skip ahead rather than stall
            float u = dt > 0.f ? std::clamp((done - start) / dt, 0.f, 1.f) : 1.f;
            Substep(from + (hand - from) * u, floorZ);
        }
        if (dt > 0.f) tipVel_ = (p_[kSegments] - lastTip) * (1.f / dt);
    }

private:
    static constexpr float kSub = 1.f / 240.f;
    static constexpr int kIterations = 6;
    static constexpr float kBladeWeight = 0.35f;  // the blade is heavier than a link: it gives less

    void Substep(V3 hand, float floorZ) {
        const int n = kSegments;
        const float seg = length_ / n;
        // Move: Verlet, with gravity and air drag.
        const float drag = std::exp(-1.5f * kSub);
        const V3 g{0, 0, -980.f * kSub * kSub};
        for (int i = 1; i <= n; i++) {
            V3 v = (p_[i] - o_[i]) * drag;
            o_[i] = p_[i];
            p_[i] = p_[i] + v + g;
        }
        p_[0] = hand;
        // A swing: the blade toward its target, the links toward the line out to it. Moving the points (not their
        // last positions) is a push: it becomes speed, so the chain keeps going when the swing ends.
        if (pulling_ && !pinned_) {
            float k = 1.f - std::exp(-rate_ * kSub);
            p_[n] = p_[n] + (target_ - p_[n]) * k;
            for (int i = 1; i < n; i++) {
                float f = float(i) / n;
                V3 want = hand + (target_ - hand) * f;
                p_[i] = p_[i] + (want - p_[i]) * (k * body_ * f);
            }
        }
        if (pinned_) p_[n] = o_[n] = pin_;
        // Lengths: the links only pull (a chain goes slack). The hand never gives; a pinned blade neither.
        for (int it = 0; it < kIterations; it++)
            for (int i = 0; i < n; i++) {
                V3 d = p_[i + 1] - p_[i];
                float l = Length(d);
                if (l <= seg || l < 1e-6f) continue;
                float wa = i == 0 ? 0.f : 1.f;
                float wb = i + 1 == n ? (pinned_ ? 0.f : kBladeWeight) : 1.f;
                if (wa + wb <= 0.f) continue;
                V3 fix = d * ((l - seg) / l / (wa + wb));
                p_[i] = p_[i] + fix * wa;
                p_[i + 1] = p_[i + 1] - fix * wb;
            }
        // Then exact from the hand out, so the chain is never longer than it is (unless the blade is pinned).
        if (!pinned_)
            for (int i = 0; i < n; i++) {
                V3 d = p_[i + 1] - p_[i];
                float l = Length(d);
                if (l > seg) {
                    V3 want = p_[i] + d * (seg / l);
                    o_[i + 1] = o_[i + 1] + (want - p_[i + 1]) * 0.9f;  // no energy gained from the fix
                    p_[i + 1] = want;
                }
            }
        float floor = floorZ + 2.f;
        for (int i = 1; i <= n; i++)
            if (p_[i].z < floor) {
                p_[i].z = floor;
                o_[i].x += (p_[i].x - o_[i].x) * 0.35f;  // friction on the ground
                o_[i].y += (p_[i].y - o_[i].y) * 0.35f;
            }
    }

    V3 p_[kSegments + 1], o_[kSegments + 1], hand_{}, dir_{1, 0, 0}, target_{}, pin_{}, tipVel_{};
    float length_ = 150.f, acc_ = 0.f, rate_ = 0.f, body_ = 0.f;
    bool live_ = false, pulling_ = false, pinned_ = false;
};

}  // namespace crabmods::shell
