// Pathfinder, a perk in Portal Ring's place: an extra portal choice after each island for every level. The relic
// itself no longer drops while this is on.
//
// Host: SpawnPortals (0x140d00210) walks the game state's PlayerArray and adds one portal for each player who
// HasRelic(Portal Ring); holding the perk answers yes (Item::GrantsRelic). For the levels past the first, the player
// is listed again that many more times while the portals spawn: the array is swapped for a longer copy in
// OnSpawnPortals and put back in OnPortalsSpawned. Nothing else SpawnPortals does with the players minds a repeat
// (the other walk only asks whether anyone qualifies). With more portals than reward pools, the game stops at the
// last pool.
#include <vector>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kPortalRing = 10;  // ECrabRelicType::PortalRing

class Pathfinder : public AssetItem {
public:
    Pathfinder()
        : AssetItem("pathfinder", 186,
                    {Kind::Perk, L"DA_Perk_Pathfinder", L"Pathfinder", L"More portal choices after each island",
                     L"Extra portal choices after each island: PrimaryBuff", 250, Perk_Collector, pool::Skill},
                    rarity::Common) {
        g_grantedRelics[kPortalRing] = true;
    }

    std::string DefaultIni() const override {
        return R"(# Pathfinder, a perk in place of the Portal Ring relic: an extra portal choice after each island for every
# level. While it's on, Portal Ring doesn't drop.
enabled = true

# Which chests it drops from: common, epic, legendary or greed.
rarity = common
)";
    }
    std::string Describe() const override { return std::string("+1 portal choice per level, ") + RarityName(rarity_); }

    bool GrantsRelic(void* ps, uint8_t type) override { return type == kPortalRing && Level(ps) > 0; }
    float LootWeight(void* ps, void* da) override {
        bool ring = da && IsA(da, StaticClass(va::UCrabRelicDA_StaticClass)) &&
                    At<uint8_t>(da, off::DA_ItemType) == kPortalRing;
        return ring ? 0.f : 1.f;
    }

    void OnSpawnPortals(void* lm) override {
        void* gs = GameStateOf(lm);
        if (!gs || !da_) return;
        auto& players = At<TArray<void*>>(gs, off::GS_PlayerArray);
        std::vector<void*> longer(players.Data, players.Data + players.Num);
        ForEachPlayer(gs, [&](void* ps) {
            for (int i = Level(ps); i > 1; i--) longer.push_back(ps);
        });
        if (longer.size() == size_t(players.Num)) return;
        saved_ = players;
        gs_ = gs;
        list_ = std::move(longer);
        players.Data = list_.data();
        players.Num = players.Max = int(list_.size());
        CRAB_LOG("Pathfinder: %d extra portals past the first per holder", players.Num - saved_.Num);
    }
    void OnPortalsSpawned(void* lm) override {
        if (!gs_) return;
        At<TArray<void*>>(gs_, off::GS_PlayerArray) = saved_;
        gs_ = nullptr;
        list_.clear();
    }

protected:
    float PrimaryBuff() const override { return 1.f; }
    void Customize(void* da) override { At<bool>(da, off::DA_bGetBuffAsMultiplier) = false; }

private:
    void* gs_ = nullptr;     // the game state whose PlayerArray is swapped, while it is
    TArray<void*> saved_{};  // its own array, to put back
    std::vector<void*> list_;
};

Pathfinder g_item;

}  // namespace
}  // namespace crabmods
