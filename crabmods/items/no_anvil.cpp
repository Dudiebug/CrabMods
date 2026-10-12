// No Anvil Portals: the portals after an island never lead to an Anvil island. A portal the game gives the Anvil
// reward gets another reward instead, picked by the game's weights from those no other portal has (main.cpp,
// HkSetPortalInfo). Host only: the host spawns the portals, and every player sees what it picked.
#include "../config.hpp"
#include "../item.hpp"

namespace crabmods {
namespace {

class NoAnvil : public Item {
public:
    NoAnvil() : Item("noAnvilPortals", 141) {}

    std::string DefaultIni() const override {
        return R"(# No Anvil Portals: portals never lead to an Anvil island; those portals get another reward instead.
# Switched off, Anvil islands come up as in the game.
enabled = true
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
    }
    std::string Describe() const override { return enabled_ ? "no Anvil islands" : "Anvil islands as in the game"; }
    std::string Title() const override { return "No Anvil Portals"; }

    float PortalRewardWeight(void* lm, uint8_t lootPool) override {
        return enabled_ && lootPool == pool::Anvil ? 0.f : 1.f;
    }

private:
    bool enabled_ = true;
};

NoAnvil g_item;

}  // namespace
}  // namespace crabmods
