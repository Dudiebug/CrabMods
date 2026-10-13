// Melee mods: stronger numbers for the game's own melee mods, so they are worth taking. Each mod's data asset
// gets new values at every level start; the game reads them from there, and its descriptions follow. Big Claws
// and Blender also get a cap: the game's lookup of a melee mod's buff (level x buff) is held to it.
#include <algorithm>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"

namespace crabmods {
namespace {

// ECrabMeleeModType
constexpr uint8_t Vampire = 1, BigClaws = 2, SharpClaws = 3, IronClaws = 4, TimeClaws = 5, Blender = 6,
                  IceClaws = 7, ArcaneClaws = 11;

class MeleeMods : public Item {
public:
    MeleeMods() : Item("meleeMods", 170) {}

    std::string DefaultIni() const override {
        return R"(# Melee mods: stronger numbers for the game's own melee mods. The game's values are in brackets.
enabled = true

# Melee damage increase, in percent: Sharp Claws [100], Time Claws [150], Iron Claws [300].
sharpClawsPercent = 200
timeClawsPercent = 300
ironClawsPercent = 500

# Iron Claws' melee cooldown increase, in percent [40].
ironClawsCooldownPercent = 20

# Big Claws: melee range increase per level, in percent [20], and the most it can give [no cap].
bigClawsPerLevel = 3
bigClawsMax = 35

# Blender: melee cooldown reduction per level, in percent [20], and the most it can give [no cap: 100% at level 5].
blenderPerLevel = 15
blenderMax = 75

# Health healed by Vampire per kill [5].
vampireHealth = 15

# Ice, Fire, Lightning, Poison and Arcane Claws: their stacks per hit are multiplied by this [1].
elementalClawsMultiplier = 2
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "sharpClawsPercent") sharp_ = ParseFloat(v);
        else if (k == "timeClawsPercent") time_ = ParseFloat(v);
        else if (k == "ironClawsPercent") iron_ = ParseFloat(v);
        else if (k == "ironClawsCooldownPercent") ironCooldown_ = ParseFloat(v);
        else if (k == "bigClawsPerLevel") big_ = ParseFloat(v);
        else if (k == "bigClawsMax") bigMax_ = ParseFloat(v);
        else if (k == "blenderPerLevel") blender_ = ParseFloat(v);
        else if (k == "blenderMax") blenderMax_ = ParseFloat(v);
        else if (k == "vampireHealth") vampire_ = ParseFloat(v);
        else if (k == "elementalClawsMultiplier") elemental_ = ParseFloat(v);
    }
    std::string Title() const override { return "Stronger Melee Mods"; }
    std::string Describe() const override {
        if (!enabled_) return "off (the game's values)";
        return "Sharp " + Num(sharp_) + "%, Time " + Num(time_) + "%, Iron " + Num(iron_) + "% (cooldown +" +
               Num(ironCooldown_) + "%), Big " + Num(big_) + "% (max " + Num(bigMax_) + "%), Blender " + Num(blender_) +
               "% (max " + Num(blenderMax_) + "%), Vampire " +
               Num(vampire_) + ", elemental x" + Num(elemental_);
    }

    // Always called, so that switching it off (a host's settings) puts the game's values back.
    void OnBeginPlay(void* lm) override {
        auto* list = Spawnables(lm, off::Spawnables_MeleeMods);
        if (!list || !list->Data) return;
        int changed = 0;
        for (int i = 0; i < list->Num; i++) {
            void* da = list->Data[i];
            if (!da) continue;
            auto [it, fresh] = game_.try_emplace(da, Values{At<float>(da, off::DA_BaseBuff),
                                                            At<float>(da, off::DA_BaseDebuff)});
            Values v = enabled_ ? Mine(At<uint8_t>(da, off::DA_ItemType), it->second) : it->second;
            float& buff = At<float>(da, off::DA_BaseBuff);
            float& debuff = At<float>(da, off::DA_BaseDebuff);
            if (buff != v.buff || debuff != v.debuff) changed++;
            buff = v.buff;
            debuff = v.debuff;
        }
        if (changed) CRAB_LOG("meleeMods: %s values for %d melee mods", enabled_ ? "new" : "the game's", changed);
    }

    void InstallHooks() override {
        g_self = this;
        Hook(va::ACrabPS_GetMeleeModBuff, &HkBuff, &oBuff, "ACrabPS::GetMeleeModBuff (Big Claws, Blender caps)");
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!enabled_ || !ps || !game_.count(da)) return {};
        uint8_t type = At<uint8_t>(da, off::DA_ItemType);
        if (type != BigClaws && type != Blender) return {};
        float max = type == BigClaws ? bigMax_ : blenderMax_;
        return (type == BigClaws ? "Melee range: +" : "Melee cooldown: -") + Num(HkBuff(ps, type) * 100.f) +
               "% (max " + Num(max) + "%)";
    }

private:
    // Every machine: the buff as a fraction (Big Claws 0.03 a level), held to the cap.
    static float HkBuff(void* ps, uint8_t type) {
        float buff = oBuff(ps, type);
        MeleeMods* self = g_self;
        if (!self || !self->enabled_) return buff;
        if (type == BigClaws) return std::min(buff, self->bigMax_ * 0.01f);
        if (type == Blender) return std::min(buff, self->blenderMax_ * 0.01f);
        return buff;
    }

    static inline float (*oBuff)(void*, uint8_t) = nullptr;
    static inline MeleeMods* g_self = nullptr;

    struct Values {
        float buff, debuff;
    };

    Values Mine(uint8_t type, Values game) const {
        switch (type) {
        case SharpClaws: return {sharp_, game.debuff};
        case TimeClaws: return {time_, game.debuff};
        case IronClaws: return {iron_, ironCooldown_};
        case BigClaws: return {big_, game.debuff};
        case Blender: return {blender_, game.debuff};
        case Vampire: return {vampire_, game.debuff};
        }
        if (type >= IceClaws && type <= ArcaneClaws) return {game.buff * elemental_, game.debuff};
        return game;  // Brawler and anything new
    }

    bool enabled_ = true;
    float sharp_ = 200.f, time_ = 300.f, iron_ = 500.f, ironCooldown_ = 20.f, big_ = 3.f, bigMax_ = 35.f,
          blender_ = 15.f, blenderMax_ = 75.f,
          vampire_ = 15.f, elemental_ = 2.f;
    std::unordered_map<void*, Values> game_;  // each asset's own values, as first seen
};

MeleeMods g_item;

}  // namespace
}  // namespace crabmods
