// The necromancer kit's art and sounds, carried inside CrabMods.dll (resource 100, made by
// art/necromancer/bake_pack.py) and turned into game objects at run time: textures from PNGs, the whip's meshes
// from triangle lists, and sounds played by a small mixer of our own (the engine can't load a sound file at run
// time). Sounds are stored as Ogg Vorbis and decoded once, when the pack loads.
#pragma once
#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../item.hpp"
#include "../ui.hpp"

// third_party/stb/stb_vorbis.c, built as C by build.sh.
extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels, int* sample_rate,
                                        short** output);

namespace crabmods::necro {

// --- The pack ---------------------------------------------------------------------------------------------

struct Blob {
    const uint8_t* data = nullptr;
    uint32_t size = 0;
    explicit operator bool() const { return data != nullptr; }
};

class Pack {
public:
    bool Load() {
        if (loaded_) return !entries_.empty();
        loaded_ = true;
        HMODULE self = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&Pack::Anchor), &self))
            return false;
        HRSRC r = FindResourceW(self, MAKEINTRESOURCEW(100), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
        HGLOBAL g = r ? LoadResource(self, r) : nullptr;
        const uint8_t* p = g ? static_cast<const uint8_t*>(LockResource(g)) : nullptr;
        uint32_t n = r ? SizeofResource(self, r) : 0;
        if (!p || n < 8 || std::memcmp(p, "CRPK", 4) != 0) {
            CRAB_LOG("necromancer: the art pack is missing from CrabMods.dll");
            return false;
        }
        uint32_t count;
        std::memcpy(&count, p + 4, 4);
        if (8 + uint64_t(count) * 56 > n) return false;
        for (uint32_t i = 0; i < count; i++) {
            const uint8_t* e = p + 8 + i * 56;
            uint32_t off, size;
            std::memcpy(&off, e + 48, 4);
            std::memcpy(&size, e + 52, 4);
            if (uint64_t(off) + size > n) continue;
            entries_[std::string(reinterpret_cast<const char*>(e), strnlen(reinterpret_cast<const char*>(e), 48))] =
                {p + off, size};
        }
        DecodeSounds();
        CRAB_LOG("necromancer: art pack loaded, %zu files", entries_.size());
        return true;
    }
    Blob Get(const std::string& name) const {
        auto it = entries_.find(name);
        return it == entries_.end() ? Blob{} : it->second;
    }
    // Every file whose name starts with prefix, sorted by name.
    std::vector<Blob> All(const std::string& prefix) const {
        std::vector<std::pair<std::string, Blob>> found;
        for (auto& [name, b] : entries_)
            if (name.rfind(prefix, 0) == 0) found.push_back({name, b});
        std::sort(found.begin(), found.end(), [](auto& a, auto& b) { return a.first < b.first; });
        std::vector<Blob> out;
        for (auto& f : found) out.push_back(f.second);
        return out;
    }

private:
    static void Anchor() {}

    // Each name.ogg becomes name.pcm: mono 16-bit at Mixer::kRate, the format the mixer plays.
    void DecodeSounds() {
        std::vector<std::string> oggs;
        for (auto& [name, b] : entries_)
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".ogg") == 0) oggs.push_back(name);
        size_t bytes = 0;
        for (auto& name : oggs) {
            Blob b = entries_[name];
            entries_.erase(name);
            int channels = 0, rate = 0;
            short* pcm = nullptr;
            int frames = stb_vorbis_decode_memory(b.data, int(b.size), &channels, &rate, &pcm);
            if (frames <= 0 || channels != 1 || rate != 32000) {
                CRAB_LOG("necromancer: can't decode %s", name.c_str());
                free(pcm);
                continue;
            }
            sounds_.emplace_back(pcm, &free);
            entries_[name.substr(0, name.size() - 4) + ".pcm"] = {reinterpret_cast<const uint8_t*>(pcm),
                                                                   uint32_t(frames) * 2};
            bytes += size_t(frames) * 2;
        }
        if (!oggs.empty()) CRAB_LOG("necromancer: %zu sounds decoded (%zu KB)", oggs.size(), bytes / 1024);
    }

    bool loaded_ = false;
    std::unordered_map<std::string, Blob> entries_;
    std::vector<std::unique_ptr<short, void (*)(void*)>> sounds_;
};

// --- Calling engine functions often -------------------------------------------------------------------------

// A blueprint-callable function found once and called through ProcessEvent, for calls made thousands of times
// (ui::Call looks the function up on every call).
class Method {
public:
    explicit Method(const wchar_t* name) : name_(name) {}
    bool operator()(void* obj, void* params) {
        if (!obj) return false;
        void* cls = At<void*>(obj, off::Object_Class);
        if (cls != cls_) {
            cls_ = cls;
            fn_ = Fn<void* (*)(void*, uint64_t, int)>(va::UClass_FindFunctionByName)(cls, ui::Name(name_), 1);
            if (!fn_) CRAB_LOG("necromancer: %s has no function %s", ObjectName(cls).c_str(), ui::Narrow(name_).c_str());
        }
        if (!fn_) return false;
        void** vtable = *reinterpret_cast<void***>(obj);
        reinterpret_cast<void (*)(void*, void*, void*)>(vtable[va::UObject_ProcessEvent_Slot / 8])(obj, fn_, params);
        return true;
    }

private:
    const wchar_t* name_;
    void* cls_ = nullptr;
    void* fn_ = nullptr;
};

// FTransform: SIMD-aligned in the engine, which copies it with aligned loads.
struct alignas(16) Transform {
    float rot[4] = {0, 0, 0, 1};
    float loc[3] = {0, 0, 0};
    float pad0 = 0;
    float scale[3] = {1, 1, 1};
    float pad1 = 0;
};
static_assert(sizeof(Transform) == 0x30);

inline void* TransientPackage() {
    static void* p = ui::FindObject(L"/Engine/Transient");
    return p;
}

// --- Textures and materials -------------------------------------------------------------------------------

// A texture from a PNG (UKismetRenderingLibrary::ImportBufferAsTexture2D). It lives in the transient package
// and is collected like any object nothing refers to: keep it in a rooted material (Keep below).
inline void* Texture(Blob png) {
    static void* lib = ui::FindObject(L"/Script/Engine.Default__KismetRenderingLibrary");
    if (!lib || !png) return nullptr;
    struct {
        void* context;
        TArray<uint8_t> buffer;
        void* ret;
    } p{TransientPackage(), {const_cast<uint8_t*>(png.data), int32_t(png.size), int32_t(png.size)}, nullptr};
    ui::Call(lib, L"ImportBufferAsTexture2D", &p);
    return p.ret;
}

// Sets a texture parameter of a dynamic material. A parameter the material doesn't use still holds the texture:
// that is how the HUD's textures stay alive, kept by the whip's rooted mesh -> material.
inline void Keep(void* mid, const wchar_t* param, void* texture) {
    struct {
        uint64_t name;
        void* texture;
    } p{ui::Name(param), texture};
    ui::Call(mid, L"SetTextureParameterValue", &p);
}

// An unlit material showing a texture as it is: the engine's own one for widgets drawn in the world
// (Widget3DPassThrough_Opaque, texture parameter SlateUI), as a dynamic instance in the transient package.
// translucent: Widget3DPassThrough_Translucent, which takes the texture's alpha as its opacity.
inline void* TextureMaterial(void* texture, bool translucent = false) {
    static void* lib = ui::FindObject(L"/Script/Engine.Default__KismetMaterialLibrary");
    void* parent = ui::FindObject(
        translucent ? L"/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"
                    : L"/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque");
    if (!lib || !parent) {
        CRAB_LOG("necromancer: no Widget3DPassThrough_%s material", translucent ? "Translucent" : "Opaque");
        return nullptr;
    }
    struct {
        void* context;
        void* parent;
        uint64_t name;
        uint8_t flags;  // EMIDCreationFlags::Transient: outer = the transient package
        void* ret;
    } p{TransientPackage(), parent, 0, 1, nullptr};
    ui::Call(lib, L"CreateDynamicMaterialInstance", &p);
    if (p.ret && texture) Keep(p.ret, L"SlateUI", texture);
    return p.ret;
}

// --- Meshes -----------------------------------------------------------------------------------------------

// A static mesh from the pack's triangle list (u32 vertex count, then per vertex position xyz and uv), built at
// run time from a mesh description. Rooted. Faces are made both ways round, so winding doesn't matter.
inline void* Mesh(Blob b, void* material, const wchar_t* name) {
    static void* meshClass = ui::FindObject(L"/Script/Engine.StaticMesh");
    if (!meshClass || !b || b.size < 4) return nullptr;
    uint32_t n;
    std::memcpy(&n, b.data, 4);
    if (n % 3 || 4 + uint64_t(n) * 20 > b.size) return nullptr;
    using NewObjectFn = void* (*)(void*, void*, uint64_t, uint32_t, void*, bool, void*, void*);
    void* mesh = Fn<NewObjectFn>(va::NewObject)(TransientPackage(), meshClass, ui::Name(name),
                                                RF_Public | RF_MarkAsRootSet, nullptr, false, nullptr, nullptr);
    if (!mesh) return nullptr;
    uint32_t slot[2] = {};  // FName
    if (material) {
        struct {
            void* material;
            uint32_t ret[2];
        } p{material, {}};
        ui::Call(mesh, L"AddMaterial", &p);
        std::memcpy(slot, p.ret, sizeof slot);
    }
    struct {
        void* outer;
        void* ret;
    } d{mesh, nullptr};
    ui::Call(mesh, L"CreateStaticMeshDescription", &d);
    void* desc = d.ret;
    if (!desc) return nullptr;

    static Method createVertex(L"CreateVertex"), setPosition(L"SetVertexPosition"),
        createInstance(L"CreateVertexInstance"), setUV(L"SetVertexInstanceUV"), createTriangle(L"CreateTriangle");
    int32_t group = -1;
    ui::Call(desc, L"CreatePolygonGroup", &group);
    struct {
        int32_t group;
        uint32_t name[2];
    } g{group, {slot[0], slot[1]}};
    ui::Call(desc, L"SetPolygonGroupMaterialSlotName", &g);

    std::vector<int32_t> instances(n);
    for (uint32_t i = 0; i < n; i++) {
        float f[5];
        std::memcpy(f, b.data + 4 + i * 20, sizeof f);
        int32_t v = -1;
        createVertex(desc, &v);
        struct {
            int32_t v;
            float x, y, z;
        } sp{v, f[0], f[1], f[2]};
        setPosition(desc, &sp);
        struct {
            int32_t v, ret;
        } ci{v, -1};
        createInstance(desc, &ci);
        instances[i] = ci.ret;
        struct {
            int32_t vi;
            float u, v;
            int32_t index;
        } uv{ci.ret, f[3], f[4], 0};
        setUV(desc, &uv);
    }
    struct {
        int32_t group;
        uint32_t pad;
        TArray<int32_t> instances;
        TArray<int32_t> edges;
        int32_t ret;
    } t{group, 0, {}, {nullptr, 0, 0}, -1};
    for (uint32_t i = 0; i < n; i += 3) {
        int32_t tri[2][3] = {{instances[i], instances[i + 1], instances[i + 2]},
                             {instances[i], instances[i + 2], instances[i + 1]}};
        for (auto& ids : tri) {
            t.instances = {ids, 3, 3};
            createTriangle(desc, &t);
        }
    }
    if (t.edges.Data) Fn<void (*)(void*)>(va::FMemory_Free)(t.edges.Data);
    void* list[1] = {desc};
    struct {
        TArray<void*> descriptions;
        bool collision;
    } bp{{list, 1, 1}, false};
    ui::Call(mesh, L"BuildFromStaticMeshDescriptions", &bp);
    return mesh;
}

// --- Sounds -----------------------------------------------------------------------------------------------
//
// Mono 16-bit sounds at 32 kHz, mixed to stereo on a thread of our own and played through waveOut. It runs until
// the game closes (nothing joins it while Windows unloads the DLL).

class Mixer {
public:
    static constexpr int kRate = 32000;

    // gain: 0..1 (and more), pan: -1 left .. 1 right; skip: seconds into the sound to start from.
    void Play(Blob pcm, float gain, float pan, float skip = 0.f) {
        if (!pcm || gain <= 0.001f) return;
        if (!Start()) return;
        Voice v;
        v.data = reinterpret_cast<const int16_t*>(pcm.data);
        v.length = pcm.size / 2;
        v.pos = std::min<size_t>(size_t(std::max(skip, 0.f) * kRate), v.length);
        pan = std::clamp(pan, -1.f, 1.f);
        float a = (pan + 1.f) * 0.7853982f;  // equal power
        v.left = gain * std::cos(a) * 1.4142136f;
        v.right = gain * std::sin(a) * 1.4142136f;
        std::lock_guard<std::mutex> lock(mutex_);
        if (voices_.size() < 48) voices_.push_back(v);
    }
    void SetVolume(float v) { volume_ = v; }
    // The game's own volume (Master x Gameplay), which our sounds follow on top of SetVolume's.
    void SetGameVolume(float v) { game_ = v; }

private:
    struct Voice {
        const int16_t* data;
        size_t length, pos;
        float left, right;
    };
    static constexpr int kBlock = 512;  // frames: 16 ms
    static constexpr int kBlocks = 4;

    bool Start() {
        if (running_) return true;
        if (failed_) return false;
        WAVEFORMATEX f{};
        f.wFormatTag = WAVE_FORMAT_PCM;
        f.nChannels = 2;
        f.nSamplesPerSec = kRate;
        f.wBitsPerSample = 16;
        f.nBlockAlign = 4;
        f.nAvgBytesPerSec = kRate * 4;
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event_ || waveOutOpen(&out_, WAVE_MAPPER, &f, DWORD_PTR(event_), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
            CRAB_LOG("necromancer: no sound device; the kit is silent");
            failed_ = true;
            return false;
        }
        running_ = true;
        thread_ = std::thread([this] { Run(); });
        return true;
    }

    void Run() {
        std::vector<int16_t> buffers[kBlocks];
        WAVEHDR headers[kBlocks]{};
        for (int i = 0; i < kBlocks; i++) {
            buffers[i].assign(kBlock * 2, 0);
            headers[i].lpData = reinterpret_cast<LPSTR>(buffers[i].data());
            headers[i].dwBufferLength = kBlock * 4;
            waveOutPrepareHeader(out_, &headers[i], sizeof(WAVEHDR));
            headers[i].dwFlags |= WHDR_DONE;
        }
        std::vector<float> mix(kBlock * 2);
        while (running_) {
            bool wrote = false;
            for (int i = 0; i < kBlocks; i++) {
                if (!(headers[i].dwFlags & WHDR_DONE)) continue;
                Mix(mix.data());
                for (int s = 0; s < kBlock * 2; s++)
                    buffers[i][s] = int16_t(std::clamp(std::lround(mix[s]), -32768L, 32767L));
                headers[i].dwFlags &= ~WHDR_DONE;
                waveOutWrite(out_, &headers[i], sizeof(WAVEHDR));
                wrote = true;
            }
            if (!wrote) WaitForSingleObject(event_, 50);
        }
        waveOutReset(out_);
        for (auto& h : headers) waveOutUnprepareHeader(out_, &h, sizeof(WAVEHDR));
        waveOutClose(out_);
        CloseHandle(event_);
    }

    void Mix(float* out) {
        std::fill(out, out + kBlock * 2, 0.f);
        float vol = volume_ * game_;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = voices_.begin(); it != voices_.end();) {
            Voice& v = *it;
            size_t n = std::min<size_t>(kBlock, v.length - v.pos);
            for (size_t s = 0; s < n; s++) {
                float x = v.data[v.pos + s] * vol;
                out[s * 2] += x * v.left;
                out[s * 2 + 1] += x * v.right;
            }
            v.pos += n;
            it = v.pos >= v.length ? voices_.erase(it) : std::next(it);
        }
        // A soft limit: many sounds at once shouldn't clip hard.
        for (int s = 0; s < kBlock * 2; s++) {
            float x = out[s] / 32768.f;
            out[s] = 32767.f * (std::fabs(x) < 0.7f ? x : std::copysign(0.7f + 0.3f * std::tanh((std::fabs(x) - 0.7f) / 0.3f), x));
        }
    }

    std::mutex mutex_;
    std::vector<Voice> voices_;
    std::atomic<bool> running_{false};
    bool failed_ = false;
    std::atomic<float> volume_{0.8f};
    std::atomic<float> game_{1.f};
    std::thread thread_;
    HWAVEOUT out_ = nullptr;
    HANDLE event_ = nullptr;
};

}  // namespace crabmods::necro
