// The necromancer's marks. On the ground: deferred decals, so they lie on the floor however it slopes or steps.
//
//   Rise:  the circle a raised undead climbs out of: an outer ring of runes and a hexagram inside, turning opposite
//          ways, flaring when the undead comes up and fading after.
//   Brand: under a tagged enemy while the tag lasts, following it.
//
// And around a tagged enemy, a halo: a ring of runes floating round its middle (a flat translucent mesh on the
// enemy, placed each frame) that snaps in from wide when the tag lands, then turns, sways and breathes.
//
// They use the game's damage-area decal material (M_Decal_DamageArea: a translucent stain decal whose shape is its
// texture parameter "O"), as a dynamic instance per decal with our pictures (raise_ring.png, raise_hex.png from the
// pack) and colours. Every machine draws its own; nothing here is sent.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"

namespace crabmods::necro {

class Runes {
public:
    // The pictures, and something rooted to hold the material (a static mesh: AddMaterial). false without the
    // material; then there are no marks.
    // halo: the halo's mesh; addPiece puts a mesh component on an actor, placed by us. Without them, no halos.
    bool Init(void* ring, void* hex, void* keeper, void* halo, void* (*addPiece)(void*, void*)) {
        ring_ = ring, hex_ = hex, haloMesh_ = halo, addPiece_ = addPiece;
        mat_ = ui::FindObject(kMaterial);
        if (!mat_) {
            static void* lib = ui::FindObject(L"/Script/Engine.Default__KismetSystemLibrary");
            struct {
                int32_t index = 0, serial = 0, tag = 0, pad = 0;  // TSoftObjectPtr: the weak pointer, unset
                uint64_t path;
                TArray<wchar_t> sub{};
                void* ret = nullptr;
            } p{};
            p.path = ui::Name(kMaterial);
            static_assert(sizeof(p) == 0x30);
            if (lib) ui::Call(lib, L"LoadAsset_Blocking", &p);
            mat_ = p.ret;
        }
        if (!mat_ || !ring_ || !hex_) {
            CRAB_LOG("necromancer: no decal material (%p) or pictures; raises and tags have no marks", mat_);
            return mat_ = nullptr, false;
        }
        struct {
            void* material;
            uint32_t ret[2];
        } k{mat_, {}};
        if (keeper) ui::Call(keeper, L"AddMaterial", &k);
        return true;
    }

    // A raise circle at a spot on the ground, radius in cm; the undead comes up rise seconds after.
    void Rise(void* world, Vec3 at, float radius, float rise) {
        if (!Ready(world)) return;
        float yaw = float(rand() % 360);
        Add(world, {Kind::Ring, nullptr, at, radius, Now(), Now() + rise + 1.7, yaw, 40.f, rise});
        Add(world, {Kind::Hex, nullptr, at, radius * 0.95f, Now(), Now() + rise + 1.7, -yaw, -75.f, rise});
    }

    // The tagged enemies now (alive, in this world) and their marks' sizes: marks come and go to match.
    void Brands(void* world, const std::unordered_map<void*, float>& tagged) {
        Halos(world, tagged);
        if (!Ready(world)) return;
        for (auto& m : marks_)
            if (m.kind == Kind::Brand && m.follow && !tagged.count(m.follow)) m.end = 0.0, m.follow = nullptr;
        for (auto& [e, radius] : tagged) {
            bool have = false;
            for (auto& m : marks_) have |= m.follow == e;
            Vec3 at;
            if (!have && LocationOf(e, at)) Add(world, {Kind::Brand, e, at, radius, Now(), 1e30, float(rand() % 360), 90.f, 0.f});
        }
    }

    // Every frame: turns, grows, flares, fades and follows; removes what is done.
    void Frame(void* world) {
        if (world != world_) marks_.clear(), halos_.clear(), world_ = world;  // the old level's went with it
        double now = Now();
        for (Halo& h : halos_) PlaceHalo(h, float(now - h.start));
        for (auto it = marks_.begin(); it != marks_.end();) {
            Mark& m = *it;
            float t = float(now - m.start);
            if (now >= m.end) {
                Destroy(m.comp);
                it = marks_.erase(it);
                continue;
            }
            if (m.follow) LocationOf(m.follow, m.at);
            float grow, amount, glow;
            if (m.kind == Kind::Brand) {
                grow = 1.f + 0.06f * std::sin(t * 5.f);
                amount = 2.5f * std::min(t / 0.2f, 1.f);
                glow = 5.f + 2.5f * std::sin(t * 5.f);
            } else {
                float k = std::min(t / 0.3f, 1.f);
                grow = 0.35f + 0.65f * (1.f - (1.f - k) * (1.f - k) * (1.f - k));
                float left = float(m.end - now);
                amount = 3.f * k * std::min(left / 0.7f, 1.f);
                float f = (t - m.rise) / 0.18f;
                glow = 5.f + 18.f * std::exp(-f * f);  // the flare as the undead comes up
            }
            Place(m, m.yaw + m.spin * t, grow);
            Scalar(m.mid, nameAmount_, amount);
            Scalar(m.mid, nameEmissive_, glow);
            ++it;
        }
    }

private:
    static constexpr const wchar_t* kMaterial = L"/Game/FX/Material/M_Decal_DamageArea.M_Decal_DamageArea";
    static constexpr float kDepth = 260.f;  // how far up and down from the spot it reaches, cm
    enum class Kind { Ring, Hex, Brand };
    struct Mark {
        Kind kind;
        void* follow;  // the tagged enemy (Brand)
        Vec3 at;
        float radius;
        double start, end;
        float yaw, spin;  // degrees, degrees a second
        float rise;       // seconds to the flare
        void* comp = nullptr;
        void* mid = nullptr;
    };

    static double Now() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    bool Ready(void* world) {
        if (world != world_) marks_.clear(), halos_.clear(), world_ = world;
        return mat_ && world && marks_.size() < 96;
    }

    void Add(void* world, Mark m) {
        static void* statics = ui::FindObject(va::GameplayStaticsPath);
        struct {
            void* context;
            void* material;
            Vec3 size;
            Vec3 loc;
            Vec3 rot;
            float life = 0.f;  // removed by Frame
            void* ret = nullptr;
        } p{world, mat_, {kDepth, m.radius, m.radius}, m.at, {-90.f, m.yaw, 0.f}};
        static_assert(sizeof(p) == 0x40);
        if (!statics || !ui::Call(statics, L"SpawnDecalAtLocation", &p) || !p.ret) return;
        m.comp = p.ret;
        ui::Call(m.comp, L"CreateDynamicMaterialInstance", &m.mid);
        if (!m.mid) return Destroy(m.comp);
        float fade = 0.f;
        ui::Call(m.comp, L"SetFadeScreenSize", &fade);  // never culled for being small on screen
        struct {
            uint64_t name;
            void* texture;
        } tex{ui::Name(L"O"), m.kind == Kind::Ring ? ring_ : hex_};
        ui::Call(m.mid, L"SetTextureParameterValue", &tex);
        struct {
            uint64_t name;
            float c[4];
        } tint{ui::Name(L"Tint"), {0.35f, 0.95f, 0.75f, 1.f}};  // the undead's ghost green
        if (m.kind == Kind::Brand) tint.c[0] = 0.69f, tint.c[1] = 0.42f, tint.c[2] = 1.f;  // the whip's soul violet
        ui::Call(m.mid, L"SetVectorParameterValue", &tint);
        Scalar(m.mid, ui::Name(L"Distortion_Amount"), 0.f);  // crisp lines, not the damage area's wobble
        Scalar(m.mid, ui::Name(L"Noise_Power"), 0.f);
        Scalar(m.mid, ui::Name(L"O_Power"), 1.f);
        if (!nameAmount_) nameAmount_ = ui::Name(L"O_Amount"), nameEmissive_ = ui::Name(L"Emissive");
        Scalar(m.mid, nameAmount_, 0.f);
        marks_.push_back(m);
    }

    void Place(const Mark& m, float yaw, float scale) {
        struct {
            Vec3 loc;
            Vec3 rot;
            bool sweep = false;
            uint8_t hit[0x90] = {};  // FHitResult (0x88) at 0x1C, then bTeleport: all false and empty
        } p{m.at, {-90.f, yaw, 0.f}};
        static_assert(sizeof(p) >= 0xA8);
        ui::Call(m.comp, L"K2_SetWorldLocationAndRotation", &p);
        Vec3 s{1.f, scale, scale};
        ui::Call(m.comp, L"SetWorldScale3D", &s);
    }

    static void Scalar(void* mid, uint64_t name, float v) {
        struct {
            uint64_t name;
            float value;
        } p{name, v};
        ui::Call(mid, L"SetScalarParameterValue", &p);
    }

    static void Destroy(void* comp) {
        void* owner = nullptr;
        ui::Call(comp, L"GetOwner", &owner);
        if (owner) ui::Call(owner, L"K2_DestroyActor");
    }

    // --- Halos ---
    struct Halo {
        void* enemy;
        void* comp;
        double start;
        float yaw;
        float radius;  // the enemy's capsule's, cm
        float half;    // its half height
    };

    void Halos(void* world, const std::unordered_map<void*, float>& tagged) {
        if (world != world_) marks_.clear(), halos_.clear(), world_ = world;
        if (!haloMesh_ || !addPiece_ || !world) return;
        for (auto it = halos_.begin(); it != halos_.end();) {
            if (tagged.count(it->enemy)) {
                ++it;
                continue;
            }
            ui::Call(it->comp, L"K2_DestroyComponent", &it->comp);
            it = halos_.erase(it);
        }
        for (auto& [e, size] : tagged) {
            bool have = false;
            for (Halo& h : halos_) have |= h.enemy == e;
            if (have || halos_.size() >= 48) continue;
            void* comp = addPiece_(e, haloMesh_);
            if (!comp) continue;
            Halo h{e, comp, Now(), float(rand() % 360), 60.f, 60.f};
            if (void* capsule = At<void*>(e, off::Character_Capsule)) {
                ui::Call(capsule, L"GetScaledCapsuleRadius", &h.radius);
                ui::Call(capsule, L"GetScaledCapsuleHalfHeight", &h.half);
            }
            h.radius = std::clamp(h.radius, 30.f, 400.f);
            halos_.push_back(h);
            PlaceHalo(halos_.back(), 0.f);
        }
    }

    void PlaceHalo(Halo& h, float t) {
        Vec3 at;
        if (!LocationOf(h.enemy, at)) return;
        at.z += h.half * 0.15f + 6.f * std::sin(t * 2.2f);  // a little above its middle, bobbing
        float k = std::min(t / 0.28f, 1.f);
        float lock = 1.f + 1.6f * (1.f - k) * (1.f - k);  // the lock-on: in from wide
        float breathe = 1.f + 0.05f * std::sin(t * 5.f);
        float d = h.radius * 2.f * 1.55f * lock * breathe;  // the mesh is one unit across
        struct {
            Vec3 loc;
            Vec3 rot;
            bool sweep = false;
            uint8_t hit[0x90] = {};
        } p{at, {9.f * std::sin(t * 1.3f), h.yaw + 120.f * t + 260.f * (1.f - k), 9.f * std::cos(t * 1.1f)}};
        ui::Call(h.comp, L"K2_SetWorldLocationAndRotation", &p);
        Vec3 s{d, d, d};
        ui::Call(h.comp, L"SetWorldScale3D", &s);
    }

    void* haloMesh_ = nullptr;
    void* (*addPiece_)(void*, void*) = nullptr;
    std::vector<Halo> halos_;

    void *mat_ = nullptr, *ring_ = nullptr, *hex_ = nullptr;
    void* world_ = nullptr;
    uint64_t nameAmount_ = 0, nameEmissive_ = 0;
    std::vector<Mark> marks_;
};

}  // namespace crabmods::necro
