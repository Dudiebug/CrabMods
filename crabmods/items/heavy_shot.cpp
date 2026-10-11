// Heavy Shot: the game's own weapon mod, which only drops once you hold Bouncing Shot (its PickupTag is
// Bounce). Clearing that lets loot rolls offer it to anyone. Its effect is not tied to the weapon: every
// player weapon fires projectiles, and the game's own Heavy Shot code adds its gravity and damage to each one.
// Loot is rolled by the host.
#include "../item.hpp"

namespace crabmods {
namespace {

class HeavyShot : public Item {
public:
    HeavyShot() : Item("heavyShot", 30) {}

    std::string DefaultIni() const override {
        return R"(# Heavy Shot for All: the game's own Heavy Shot weapon mod only drops once you hold Bouncing Shot. This lets it
# drop for everyone, with any weapon.
enabled = true
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled" || k == "anyWeapon") anyWeapon_ = ParseBool(v);  // anyWeapon: before 0.9.0
    }
    bool Enabled() const override { return anyWeapon_; }
    std::string Title() const override { return "Heavy Shot for All"; }
    std::string Describe() const override { return anyWeapon_ ? "drops for every weapon" : "unchanged"; }

    void OnBeginPlay(void* lm) override {
        void* da = FindByType(Spawnables(lm, off::Spawnables_WeaponMods), WeaponMod_HeavyShot);
        if (!da) return;
        bool wasLocked = At<bool>(da, off::DA_bRequiresMatchingPickupTag);
        At<bool>(da, off::DA_bRequiresMatchingPickupTag) = false;
        if (logged_) return;
        logged_ = true;
        bool inPool = Contains(At<TArray<void*>>(lm, off::LM_AllUnlockedPickups), da);
        CRAB_LOG("Heavy Shot can now drop without Bouncing Shot (was %s)%s",
                 wasLocked ? "Bouncing Shot only" : "unrestricted",
                 inPool ? "" : "; it is not unlocked in this save yet, so it still won't drop");
    }

private:
    bool anyWeapon_ = true;
    bool logged_ = false;
};

HeavyShot g_item;

}  // namespace
}  // namespace crabmods
