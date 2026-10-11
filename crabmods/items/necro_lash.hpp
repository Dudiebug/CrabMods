// The Soul Whip's lash: where each point of the cord is, from the handle out. Drawn, not simulated: at rest it
// droops out of the handle toward the ground, sways a little, and trails behind as you move (the handle's
// direction and speed are smoothed, so the hand's small bobbing doesn't shake it). A crack unrolls it from the
// handle to the tip toward the target, holds it there a moment, and lets it fall back. Every segment keeps its
// length throughout. Pure math (no game objects), so the tests can run it too. Units are the game's: cm,
// seconds, UE axes (Z up).
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
    static constexpr float kStrikeOut = 0.13f;     // a crack: the cord unrolls to the target,
    static constexpr float kStrikeHold = 0.05f;    // stays out,
    static constexpr float kStrikeSeconds = 0.34f; // and has fallen back by now

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
    bool Striking() const { return crack_ >= 0.f; }

    void Reset(V3 anchor, V3 dir) {
        last_ = anchor;
        dir_ = Normal(dir);
        vel_ = {};
        crack_ = -1.f;
        live_ = true;
    }

    // target: where the shot lands.
    void Crack(V3 target) {
        crack_ = 0.f;
        target_ = target;
    }

    // anchor/dir: where the lash leaves the handle, and the handle's direction there. floorZ: the ground.
    void Step(V3 anchor, V3 dir, float scale, float floorZ, float dt) {
        if (!live_ || Length(anchor - last_) > 400.f * scale) Reset(anchor, dir);
        dt = std::clamp(dt, 0.f, 0.1f);
        if (dt > 0.f) {
            V3 v = (anchor - last_) * (1.f / dt);
            vel_ = vel_ + (v - vel_) * (1.f - std::exp(-dt / 0.15f));
            float speed = Length(vel_);
            if (speed > 1500.f) vel_ = vel_ * (1500.f / speed);
            dir_ = Normal(dir_ + (Normal(dir, dir_) - dir_) * (1.f - std::exp(-dt / 0.08f)), dir_);
            time_ += dt;
        }
        last_ = anchor;
        if (crack_ >= 0.f && (crack_ += dt) > kStrikeSeconds) crack_ = -1.f;

        // At rest: out of the handle, bending down more and more toward the tip, swept back by the speed, swaying.
        const V3 down{0, 0, -1};
        V3 side = Normal(Cross(dir_, {0, 0, 1}), {0, 1, 0});
        float speed = Length(vel_);
        V3 back = speed > 1.f ? vel_ * (-1.f / speed) : V3{};
        float sweep = std::min(speed / 600.f, 1.f);
        V3 rest[kMaxSegments];
        for (int i = 0; i < n_; i++) {
            float f = float(i + 1) / n_;
            float g = std::min(1.f, 1.6f * f);
            V3 d = dir_ * (1.f - g) + down * g;
            d = d + back * (0.9f * sweep * f);
            d = d + side * (0.1f * f * std::sin(6.2831853f * (0.5f * time_ - 0.7f * f)));
            rest[i] = Normal(d, down);
        }

        // A crack: each segment turns from its rest direction to the target's, the handle's end first and the tip
        // last (the cord unrolls), lifted a little on the way; then the whole of it eases back to rest.
        float w = StrikeWeight();
        V3 to = Normal(target_ - anchor, dir_);
        float u = crack_ >= 0.f ? std::min(crack_ / kStrikeOut, 1.f) : 0.f;
        p_[0] = anchor;
        for (int i = 0; i < n_; i++) {
            V3 d = rest[i];
            if (w > 0.f) {
                float f = float(i) / n_;
                float k = Smooth(u * 1.6f - f * 0.6f);
                V3 out = Normal(rest[i] * (1.f - k) + to * k + V3{0, 0, 1} * (0.6f * std::sin(3.14159265f * k)), to);
                d = Normal(rest[i] * (1.f - w) + out * w, out);
            }
            p_[i + 1] = p_[i] + d * (len_[i] * scale);
            // Resting on the ground rather than through it: the droop bends up along the floor.
            float floor = floorZ + 1.5f * scale;
            if (p_[i + 1].z < floor) {
                V3 seg = p_[i + 1] - p_[i];
                float l = Length(seg);
                seg.z = std::max(seg.z, floor - p_[i].z);
                p_[i + 1] = p_[i] + Normal(seg, d) * l;
            }
        }
    }

private:
    static float Smooth(float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3.f - 2.f * x);
    }
    // How much of the crack shows: in fast, held, then eased back to rest.
    float StrikeWeight() const {
        if (crack_ < 0.f) return 0.f;
        if (crack_ < 0.03f) return Smooth(crack_ / 0.03f);
        float out = kStrikeOut + kStrikeHold;
        return crack_ < out ? 1.f : 1.f - Smooth((crack_ - out) / (kStrikeSeconds - out));
    }

    int n_ = 0;
    float len_[kMaxSegments] = {};
    V3 p_[kMaxSegments + 1], last_{}, dir_{0, 0, 1}, vel_{}, target_{};
    float crack_ = -1.f, time_ = 0.f;
    bool live_ = false;
};

}  // namespace crabmods::necro
