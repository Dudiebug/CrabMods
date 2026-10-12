// The Soul Whip's lash, simulated: a chain of points with fixed segment lengths under gravity and a little air
// drag (Verlet, fixed 1/240 s substeps), pinned to the handle, stiff near the handle and limp toward the tip, and
// lying on the floor rather than through it. The crab's wrist moves the handle: at rest it just follows the hand;
// a shot is a real whip stroke (the wrist draws back and up over the shell, snaps forward at the target and stops
// dead), and the stop sends a loop rolling down the cord. The crack is when that loop turns over at the tip: the
// tip's speed peaks, and TakeCrack reports it (that is when the crack sound plays). Pure math (no game objects),
// so the tests can run it too. Units are the game's: cm, seconds, UE axes (Z up).
#pragma once
#include <algorithm>
#include <cmath>

namespace crabmods::necro {

struct V3 {
    float x = 0, y = 0, z = 0;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(V3 a) { return std::sqrt(Dot(a, a)); }
inline V3 Normal(V3 a, V3 fallback = {0, 0, 1}) {
    float l = Length(a);
    return l > 1e-6f ? a * (1.f / l) : fallback;
}

// A rotation as UE's FQuat (x, y, z, w).
struct Q4 {
    float x = 0, y = 0, z = 0, w = 1;
};
inline V3 Rotate(const Q4& q, V3 v) {  // FQuat::RotateVector
    V3 u{q.x, q.y, q.z};
    V3 t = Cross(u, v) * 2.f;
    return v + t * q.w + Cross(u, t);
}
// The rotation whose X, Y, Z axes are x, y, z (orthonormal, z = x cross y), as FQuat(FMatrix) makes it.
inline Q4 FromAxes(V3 x, V3 y, V3 z) {
    float m[3][3] = {{x.x, x.y, x.z}, {y.x, y.y, y.z}, {z.x, z.y, z.z}};
    float tr = m[0][0] + m[1][1] + m[2][2];
    Q4 q;
    if (tr > 0.f) {
        float inv = 1.f / std::sqrt(tr + 1.f), s = 0.5f * inv;
        q.w = 0.5f / inv;
        q.x = (m[1][2] - m[2][1]) * s;
        q.y = (m[2][0] - m[0][2]) * s;
        q.z = (m[0][1] - m[1][0]) * s;
        return q;
    }
    int i = m[1][1] > m[0][0] ? 1 : 0;
    if (m[2][2] > m[i][i]) i = 2;
    static const int next[3] = {1, 2, 0};
    int j = next[i], k = next[j];
    float inv = 1.f / std::sqrt(m[i][i] - m[j][j] - m[k][k] + 1.f), s = 0.5f * inv;
    float qt[4];
    qt[i] = 0.5f / inv;
    qt[3] = (m[j][k] - m[k][j]) * s;
    qt[j] = (m[i][j] + m[j][i]) * s;
    qt[k] = (m[i][k] + m[k][i]) * s;
    return {qt[0], qt[1], qt[2], qt[3]};
}

class Lash {
public:
    static constexpr int kMaxSegments = 16;
    // A stroke, in seconds from the shot: the wrist draws back, snaps forward, then eases back to rest.
    static constexpr float kDrawBack = 0.11f;
    static constexpr float kSnap = 0.07f;
    static constexpr float kStrikeSeconds = 0.6f;

    // lengths: each segment's length at scale 1, from the handle out.
    void Init(const float* lengths, int n) {
        n_ = std::clamp(n, 1, kMaxSegments);
        for (int i = 0; i < n_; i++) len_[i] = lengths[i];
        live_ = false;
    }
    int Segments() const { return n_; }
    float RestLength(int i) const { return len_[i]; }
    const V3& Point(int i) const { return p_[i]; }
    bool Live() const { return live_; }
    bool Striking() const { return stroke_ >= 0.f; }
    float TipSpeed() const { return tipSpeed_; }

    // Laid out fresh, hanging from the handle.
    void Reset(V3 anchor, V3 dir, float scale = 1.f) {
        dir_ = Normal(dir);
        scale_ = scale;
        p_[0] = o_[0] = anchor;
        for (int i = 0; i < n_; i++) {
            float g = std::min(1.f, 1.6f * float(i + 1) / n_);
            V3 d = Normal(dir_ * (1.f - g) + V3{0, 0, -1} * g, {0, 0, -1});
            p_[i + 1] = o_[i + 1] = p_[i] + d * (len_[i] * scale);
        }
        hand_ = anchor;
        stroke_ = -1.f;
        armed_ = cracked_ = false;
        acc_ = 0.f;
        live_ = true;
    }

    // target: where the shot lands.
    void Crack(V3 target) {
        stroke_ = 0.f;
        target_ = target;
        peak_ = 0.f;
        armed_ = true;
        cracked_ = false;
    }
    // True once per stroke, when the tip cracks; at: where.
    bool TakeCrack(V3& at) {
        if (!cracked_) return false;
        cracked_ = false;
        at = crackAt_;
        return true;
    }

    // anchor/dir: where the lash leaves the handle, and the handle's direction there. floorZ: the ground.
    void Step(V3 anchor, V3 dir, float scale, float floorZ, float dt) {
        if (!live_ || Length(anchor - hand_) > 400.f * scale) Reset(anchor, dir, scale);
        dt = std::clamp(dt, 0.f, 0.1f);
        scale_ = scale;
        // The handle's direction, smoothed: the hand's small bobbing doesn't shake the cord.
        dir_ = Normal(dir_ + (Normal(dir, dir_) - dir_) * (1.f - std::exp(-dt / 0.08f)), dir_);
        V3 from = hand_;
        hand_ = anchor;
        float start = acc_;
        acc_ += dt;
        float done = 0.f;
        int steps = 0;
        while (acc_ >= kSub) {
            acc_ -= kSub;
            done += kSub;
            if (++steps > 48) continue;  // a long hitch: skip ahead rather than stall
            float u = dt > 0.f ? std::clamp((done - start) / dt, 0.f, 1.f) : 1.f;
            Substep(from + (anchor - from) * u, floorZ);
        }
    }

private:
    static constexpr float kSub = 1.f / 240.f;
    static float Smooth(float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3.f - 2.f * x);
    }

    // The wrist during a stroke: the handle's direction (the arm's own swing moves the anchor).
    void Wrist(V3 anchor, V3& dir) const {
        dir = dir_;
        if (stroke_ < 0.f) return;
        V3 to = Normal(target_ - anchor, dir_);
        V3 up = Normal(V3{0, 0, 1} - to * to.z, {0, 0, 1});
        V3 back = Normal(up * 0.8f - to * 0.6f, up);  // up and behind: over the shell, never through the body
        V3 fwd = Normal(to - up * 0.15f, to);
        float t = stroke_;
        if (t < kDrawBack) {
            float w = Smooth(t / kDrawBack);
            dir = Normal(dir_ * (1.f - w) + back * w, back);
        } else if (t < kDrawBack + kSnap) {
            float u = (t - kDrawBack) / kSnap;
            u = u * u;  // accelerating, to a dead stop
            dir = Normal(back * (1.f - u) + fwd * u + up * (0.6f * std::sin(3.14159265f * u)), fwd);
        } else {
            float w = Smooth((t - kDrawBack - kSnap) / (kStrikeSeconds - kDrawBack - kSnap));
            dir = Normal(fwd * (1.f - w) + dir_ * w, dir_);
        }
    }

    void Substep(V3 anchor, float floorZ) {
        V3 dir;
        Wrist(anchor, dir);
        if (stroke_ >= 0.f && (stroke_ += kSub) > kStrikeSeconds) stroke_ = -1.f;
        // Move: Verlet, with gravity and air drag.
        const float drag = std::exp(-1.2f * kSub);
        const V3 g{0, 0, -980.f * scale_ * kSub * kSub};
        V3 lastTip = p_[n_];
        for (int i = 1; i <= n_; i++) {
            V3 v = (p_[i] - o_[i]) * drag;
            o_[i] = p_[i];
            p_[i] = p_[i] + v + g;
        }
        p_[0] = anchor;
        // Stiffness: the first segments keep to the handle's direction and to their neighbour's, less and less
        // toward the tip. Then the lengths, handle first (the hand doesn't give); then the floor.
        // Stiffness: the stock (the first segments) is rigid along the handle; past it the cord keeps a little to
        // its neighbour's direction, less and less toward the tip. Only part of that bending becomes speed, so it
        // doesn't ring.
        for (int i = 0; i < n_; i++) {
            float f = float(i) / n_;
            float k = i < kStock ? 1.f : kBend * (1.f - f) * (1.f - f);
            if (k <= 0.f) continue;
            V3 along = i < kStock ? dir : Normal(p_[i] - p_[i - 1], dir);
            V3 want = p_[i] + along * (len_[i] * scale_);
            V3 move = (want - p_[i + 1]) * k;
            p_[i + 1] = p_[i + 1] + move;
            if (i >= kStock) o_[i + 1] = o_[i + 1] + move * 0.7f;
        }
        // Lengths: follow-the-leader from the handle out (exact, and the hand doesn't give), with the velocity
        // correction that keeps it from gaining energy (Mueller et al., "Fast Simulation of Inextensible Hair").
        V3 moved[kMaxSegments + 1] = {};
        for (int i = 0; i < n_; i++) {
            V3 d = p_[i + 1] - p_[i];
            float l = Length(d);
            V3 want = l > 1e-6f ? p_[i] + d * (len_[i] * scale_ / l) : p_[i] + dir * (len_[i] * scale_);
            moved[i + 1] = want - p_[i + 1];
            p_[i + 1] = want;
        }
        for (int i = kStock + 1; i < n_; i++) o_[i] = o_[i] + moved[i + 1] * 0.9f;
        float floor = floorZ + 1.5f * scale_;
        for (int i = 1; i <= n_; i++)
            if (p_[i].z < floor) {
                p_[i].z = floor;
                o_[i].x += (p_[i].x - o_[i].x) * 0.3f;  // friction on the ground
                o_[i].y += (p_[i].y - o_[i].y) * 0.3f;
            }
        // The crack: once the wrist has snapped, the tip's speed peaks as the loop turns over.
        tipSpeed_ = Length(p_[n_] - lastTip) / kSub;
        if (armed_ && stroke_ > kDrawBack + 0.5f * kSnap) {  // not the draw-back's own swing
            if (tipSpeed_ > peak_) {
                peak_ = tipSpeed_;
                peakAt_ = p_[n_];
            } else if (peak_ > kCrackSpeed * scale_ && tipSpeed_ < 0.8f * peak_) {
                armed_ = false;
                cracked_ = true;
                crackAt_ = peakAt_;
            }
        }
        if (stroke_ < 0.f) armed_ = false;
    }

public:
    static constexpr int kStock = 3;              // segments held rigid by the handle
    static constexpr float kBend = 0.02f;         // stiffness at the handle, falling off to none at the tip
    static constexpr float kCrackSpeed = 2000.f;  // cm/s: a tip faster than this cracks

private:
    int n_ = 0;
    float len_[kMaxSegments] = {};
    V3 p_[kMaxSegments + 1], o_[kMaxSegments + 1], hand_{}, dir_{0, 0, 1}, target_{}, crackAt_{}, peakAt_{};
    float stroke_ = -1.f, acc_ = 0.f, scale_ = 1.f, tipSpeed_ = 0.f, peak_ = 0.f;
    bool live_ = false, armed_ = false, cracked_ = false;
};

}  // namespace crabmods::necro
