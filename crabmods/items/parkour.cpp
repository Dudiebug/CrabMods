// Parkour: brings back the Parkour island, which the game dropped in 2025. Its portal still exists
// (BP_Portal_Parkour_C), but nothing offers it and the biomes list no Parkour maps. The maps come from an older
// build of the game: each player builds pakchunk77-ParkourMaps_P.pak from their own copy of it
// (re/scripts/parkour_pak.py) and puts it in Content/Paks/~mods.
// - Every machine: adds the two maps to the world as streaming levels, unloaded, named as the game's own islands are
//   ("Arctic_Parkour_01"), so the game's level travel finds them by name. Then it tells the host it has them.
// - Host: when every player has the maps, the next islands' Demolition portal is sometimes a Parkour portal instead
//   (islands 1 to 3, where the game offers Demolition), and picking a Parkour map gives the biome's: Arctic's own
//   for Arctic, the Tropical one for the others (as the old game did).
#include <algorithm>
#include <cwchar>
#include <random>
#include <string>
#include <unordered_set>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

struct ParkourMap {
    const wchar_t* package;  // the map's package, which the add-on pak holds
    const wchar_t* name;     // its island name (the end of the package)
};
constexpr ParkourMap kMaps[] = {
    {L"/Game/Island/Arctic/Arctic_Parkour_01", L"Arctic_Parkour_01"},
    {L"/Game/Island/Tropical/Tropical_Parkour_01", L"Tropical_Parkour_01"},
};
constexpr wchar_t kHave[] = L"parkour have";

using PickIslandFn = void* (*)(void*, void*, void*, void*);
PickIslandFn oPickIsland;

class Parkour;
Parkour* g_self;

class Parkour : public Item {
public:
    Parkour() : Item("parkour", 210) { g_self = this; }

    std::string DefaultIni() const override {
        return R"(# Parkour: brings back the Parkour island as a portal choice. Needs the Parkour maps add-on
# (pakchunk77-ParkourMaps_P.pak in CrabChampions/Content/Paks/~mods) on every player's game; without it on
# everyone, Parkour isn't offered.
enabled = true

# Percent chance that the Demolition portal is a Parkour portal instead (islands 1 to 3). 0 to 100.
chance = 50
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "chance") chance_ = std::clamp(ParseFloat(v), 0.f, 100.f);
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override { return Num(chance_) + "% of Demolition portals"; }
    std::string Title() const override { return "Parkour Islands"; }
    const char* CatalogKind() const override { return "Island"; }

    void InstallHooks() override { Hook(va::ACrabLM_PickIsland, &HkPickIsland, &oPickIsland, "ACrabLM::PickIsland"); }

    void OnTick(void* world) override {
        if (world != world_) {
            world_ = world;
            have_.clear();
            told_ = nullptr;
        }
        if (missing_ || !Register(world)) return;
        // Tell the host (itself included) this machine has the maps, once per player controller.
        void* lm = LevelManagerOf(world);
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        void* pc = c ? At<void*>(c, off::Pawn_Controller) : nullptr;
        if (pc && pc != told_ && (At<uint8_t>(pc, off::Actor_Role) == ROLE_Authority || HostHasCrabMods(pc))) {
            told_ = pc;
            SendToHost(pc, kHave);
        }
    }

    bool OnHostMessage(void* pc, const std::wstring& text) override {
        if (text != kHave) return false;
        if (have_.insert(pc).second) CRAB_LOG("parkour: %s has the Parkour maps", PlayerName(pc).c_str());
        return true;
    }

    void OnSpawnPortals(void* lm) override {
        void* da = At<void*>(lm, off::LM_SpawnablesDA);
        if (!da || !At<void*>(da, off::Spawnables_ParkourPortal) || chance_ <= 0.f) return;
        bool everyone = true;
        ForEachPlayer(lm, [&](void* ps) { everyone &= have_.count(At<void*>(ps, off::Actor_Owner)) > 0; });
        if (!everyone) return;
        if (std::uniform_real_distribution<float>(0.f, 100.f)(rng_) >= chance_) return;
        swapped_ = da;
        demolition_ = At<void*>(da, off::Spawnables_DemolitionPortal);
        At<void*>(da, off::Spawnables_DemolitionPortal) = At<void*>(da, off::Spawnables_ParkourPortal);
    }
    void OnPortalsSpawned(void* lm) override {
        if (!swapped_) return;
        At<void*>(swapped_, off::Spawnables_DemolitionPortal) = demolition_;
        swapped_ = nullptr;
    }

    // The map for a Parkour island, when the biome lists none.
    bool PickParkour(void* lm, void* out) {
        if (!enabled_ || missing_) return false;
        void* biome = At<void*>(lm, off::LM_BiomeDA);
        const ParkourMap& m = ObjectName(biome).find("Arctic") != std::string::npos ? kMaps[0] : kMaps[1];
        At<uint64_t>(out, 0) = ui::Name(m.name);
        At<float>(out, 8) = 1.f;
        CRAB_LOG("parkour: %s island %s", ObjectName(biome).c_str(), ui::Narrow(m.name).c_str());
        return true;
    }

private:
    // Adds the maps to the world's streaming levels if they aren't there yet. false: they can't be (no add-on).
    bool Register(void* world) {
        auto& levels = At<TArray<void*>>(world, off::World_StreamingLevels);
        for (const ParkourMap& m : kMaps) {
            uint64_t package = ui::Name(m.package);
            bool there = false;
            for (int i = 0; i < levels.Num && !there; i++)
                there = levels.Data[i] && At<uint64_t>(levels.Data[i], off::LevelStreaming_PackageNameToLoad) == package;
            if (there) continue;
            static void* cdo = ui::FindObject(L"/Script/Engine.Default__LevelStreamingDynamic");
            // The name override is only the level's name: the engine puts the package's folder in front of it
            // ("/Game/Island/Arctic/" + "Arctic_Parkour_01"). A whole package path there doubles the folder, and
            // the engine stops the game (issue 2).
            int n = int(wcslen(m.package) + 1), nn = int(wcslen(m.name) + 1);
            struct {
                void* context;
                TArray<wchar_t> level;
                float location[3];
                float rotation[3];
                bool ok;
                TArray<wchar_t> nameOverride;
                void* result;
            } p{world, {const_cast<wchar_t*>(m.package), n, n}, {}, {}, false,
                {const_cast<wchar_t*>(m.name), nn, nn}, nullptr};
            static_assert(sizeof p == 0x50);
            if (!ui::Call(cdo, L"LoadLevelInstance", &p) || !p.ok || !p.result) {
                // Once added, a failure means the map's package is still in memory from the last world (the
                // engine refuses a name that's taken): try again later.
                if (added_) return false;
                CRAB_LOG("parkour: %s isn't installed (pakchunk77-ParkourMaps_P.pak in Content/Paks/~mods): no "
                         "Parkour islands", ui::Narrow(m.package).c_str());
                missing_ = true;
                return false;
            }
            // Loaded only when the game travels to it.
            bool no = false;
            ui::Call(p.result, L"SetShouldBeLoaded", &no);
            ui::Call(p.result, L"SetShouldBeVisible", &no);
            At<uint8_t>(p.result, off::LevelStreamingDynamic_Initially) &= ~3;
            CRAB_LOG("parkour: added %s", ui::Narrow(m.name).c_str());
            added_ = true;
        }
        return true;
    }

    static std::string PlayerName(void* pc) {
        void* ps = pc ? At<void*>(pc, off::Controller_PlayerState) : nullptr;
        return ps ? FStringToUtf8(At<TArray<wchar_t>>(ps, off::PS_PlayerName)) : "a player";
    }

    static void* HkPickIsland(void* lm, void* out, void* islands, void* recent) {
        if (lm && out && islands && recent == static_cast<char*>(lm) + off::LM_RecentParkour &&
            static_cast<TArray<uint8_t>*>(islands)->Num == 0 && g_self->PickParkour(lm, out))
            return out;
        return oPickIsland(lm, out, islands, recent);
    }

    bool enabled_ = true;
    float chance_ = 50.f;
    bool missing_ = false;
    bool added_ = false;  // the maps were added to a world before
    void* world_ = nullptr;
    void* told_ = nullptr;                // the player controller the host was told through
    std::unordered_set<void*> have_;      // host: player controllers whose machines have the maps
    void* swapped_ = nullptr;             // the SpawnablesDA whose Demolition portal is Parkour's for now
    void* demolition_ = nullptr;
    std::mt19937 rng_{std::random_device{}()};
};

Parkour g_parkour;

}  // namespace
}  // namespace crabmods
