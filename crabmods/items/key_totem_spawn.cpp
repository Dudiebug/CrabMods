// Key Totem Spawn: key totems also turn up in runs, on islands and in shops, as rarely as a Gold totem (spawn weight
// 0.3 beside the game's island totems' 6.8 and shop totems' 10.3). Using one costs 3 keys, as in the lobby, and offers
// a choice of random run items (key_totem.cpp).
//
// Host: the game picks each totem's class by weight and spawns it with UWorld::SpawnActor; at that one call, the
// class becomes the key totem's with the chance its weight would have (XL and Reroll totems are left alone). The
// key totem's blueprint is only loaded in the lobby, so it is loaded by path when needed.
#include <algorithm>
#include <cmath>
#include <random>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr wchar_t kKeyTotemPath[] = L"/Game/Blueprint/Totem/BP_Totem_Key.BP_Totem_Key_C";
constexpr uint8_t kShop = 9;  // GS_CurrentIslandType
constexpr int kXLTotem = 0x448, kRerollTotem = 0x450;  // SpawnablesDA
constexpr float kIslandWeights = 6.8f, kShopWeights = 10.3f;  // the game's totems' SpawnWeights, summed

class KeyTotemSpawn : public Item {
public:
    KeyTotemSpawn() : Item("keyTotemSpawn", 195) { g_self = this; }

    std::string DefaultIni() const override {
        return R"(# Key Totem Spawn: key totems also turn up in runs, on islands and in shops. Using one costs 3 keys and
# offers a choice of random Epic or Legendary items (the key totem menu's choices).
enabled = true

# How often, as a spawn weight beside the game's totems (Loot, Glass, Fuse, Crystal, Chance and Gamble totems are 1,
# Gold 0.3, Greed 0.4). 0.3 makes it as rare as the Gold totem.
weight = 0.3
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "weight") weight_ = std::fmax(ParseFloat(v), 0.f);
    }
    bool Enabled() const override { return enabled_; }
    std::string Title() const override { return "Key Totem Spawn"; }
    std::string Describe() const override { return "spawn weight " + Num(weight_); }

    void InstallHooks() override {
        retSite_ = Addr(va::Ret_SpawnTotem);
        if (*reinterpret_cast<const uint8_t*>(retSite_ - 5) != 0xE8) {
            CRAB_LOG("keyTotemSpawn: the totem spawn isn't where expected; off");
            retSite_ = 0;
        }
        Hook(va::UWorld_SpawnActor, &HkSpawnActor, &oSpawnActor, "UWorld::SpawnActor (totems)");
    }

private:
    using SpawnActorFn = void* (*)(void*, void*, const void*, const void*, const void*);
    static inline SpawnActorFn oSpawnActor;
    static inline KeyTotemSpawn* g_self = nullptr;

    static void* HkSpawnActor(void* world, void* cls, const void* loc, const void* rot, const void* params) {
        if (g_self && g_self->retSite_ &&
            reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == g_self->retSite_ && g_self->Enabled())
            cls = g_self->Choose(world, cls);
        return oSpawnActor(world, cls, loc, rot, params);
    }

    void* Choose(void* world, void* cls) {
        if (weight_ <= 0.f || !cls) return cls;
        void* lm = LevelManagerOf(world);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        void* gs = GameStateOf(world);
        if (!sp || !gs || cls == At<void*>(sp, kXLTotem) || cls == At<void*>(sp, kRerollTotem)) return cls;
        float others = At<uint8_t>(gs, off::GS_CurrentIslandType) == kShop ? kShopWeights : kIslandWeights;
        float p = weight_ / (others + weight_);
        if (std::uniform_real_distribution<float>(0.f, 1.f)(rng_) >= p) return cls;
        void* key = KeyClass();
        if (!key) return cls;
        CRAB_LOG("keyTotemSpawn: a key totem instead of %s", ObjectName(cls).c_str());
        return key;
    }

    // Found if loaded (a totem using it, the lobby), loaded by path otherwise. Not kept: garbage collection may
    // unload it once no totem uses it.
    void* KeyClass() {
        void* key = ui::FindObject(kKeyTotemPath);
        if (!key)
            key = Fn<void* (*)(void*, void*, const wchar_t*, const wchar_t*, uint32_t, void*, bool, void*)>(
                va::StaticLoadObject)(StaticClass(va::UClass_StaticClass), nullptr, kKeyTotemPath, nullptr, 0, nullptr,
                                      true, nullptr);
        if (!key && !warned_) {
            CRAB_LOG("keyTotemSpawn: couldn't load the key totem");
            warned_ = true;
        }
        return key;
    }

    bool enabled_ = true;
    float weight_ = 0.3f;
    uintptr_t retSite_ = 0;
    bool warned_ = false;
    std::minstd_rand rng_{std::random_device{}()};
};

KeyTotemSpawn g_item;

}  // namespace
}  // namespace crabmods
