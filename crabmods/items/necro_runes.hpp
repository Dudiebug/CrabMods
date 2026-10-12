// The raise circle: a deferred decal, so it lies on the floor however it slopes or steps. An outer ring of runes and
// a hexagram inside, turning opposite ways, flaring when the undead comes up and fading after.
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
    bool Init(void* ring, void* hex, void* keeper) {
        ring_ = ring, hex_ = hex;
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
        Add(world, {Kind::Ring, at, radius, Now(), Now() + rise + 1.7, yaw, 40.f, rise});
        Add(world, {Kind::Hex, at, radius * 0.95f, Now(), Now() + rise + 1.7, -yaw, -75.f, rise});
    }

    // Every frame: turns, grows, flares, fades and follows; removes what is done.
    void Frame(void* world) {
        if (world != world_) marks_.clear(), world_ = world;  // the old level's decals went with it
        double now = Now();
        for (auto it = marks_.begin(); it != marks_.end();) {
            Mark& m = *it;
            float t = float(now - m.start);
            if (now >= m.end) {
                Destroy(m.comp);
                it = marks_.erase(it);
                continue;
            }
            float k = std::min(t / 0.3f, 1.f);
            float grow = 0.35f + 0.65f * (1.f - (1.f - k) * (1.f - k) * (1.f - k));
            float left = float(m.end - now);
            float amount = 3.f * k * std::min(left / 0.7f, 1.f);
            float f = (t - m.rise) / 0.18f;
            float glow = 5.f + 18.f * std::exp(-f * f);  // the flare as the undead comes up
            Place(m, m.yaw + m.spin * t, grow);
            Scalar(m.mid, nameAmount_, amount);
            Scalar(m.mid, nameEmissive_, glow);
            ++it;
        }
    }

private:
    static constexpr const wchar_t* kMaterial = L"/Game/FX/Material/M_Decal_DamageArea.M_Decal_DamageArea";
    static constexpr float kDepth = 260.f;  // how far up and down from the spot it reaches, cm
    enum class Kind { Ring, Hex };
    struct Mark {
        Kind kind;
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
        if (world != world_) marks_.clear(), world_ = world;
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

    void *mat_ = nullptr, *ring_ = nullptr, *hex_ = nullptr;
    void* world_ = nullptr;
    uint64_t nameAmount_ = 0, nameEmissive_ = 0;
    std::vector<Mark> marks_;
};

}  // namespace crabmods::necro
