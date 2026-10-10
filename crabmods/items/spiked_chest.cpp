// Spiked Chest: the Epic chest a Spiked island rewards, paid for with a share of your base max health (the
// game's own costs 25%). CrabMods sets its cost and the island portal's description to match.
// - The cost is the Cost of BP_Chest_Spiked's class default object, which every spiked chest copies when it
//   spawns. Only the host's matters: the host spawns the chests and takes the payment.
// - The portal text is a fixed-length literal in the game's own strings, rewritten in place on every machine
//   (the description is built by each player's game). In multiplayer it uses the host's setting like the rest.
#include <algorithm>
#include <cwchar>
#include <iterator>

#include "../item.hpp"

namespace crabmods {
namespace {

constexpr int kGameCost = 25;

class SpikedChest : public Item {
public:
    SpikedChest() : Item("spikedChest", 140) {}

    std::string DefaultIni() const override {
        return R"(# The Spiked island's Epic chest costs this percent of your base max health to open (the game's own
# is 25). 0 to 99. The island's portal says the same.
costPercent = 35
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "costPercent") cost_ = std::clamp(int(ParseFloat(v) + 0.5f), 0, 99);
    }
    std::string Describe() const override {
        return std::to_string(cost_) + "% of base max health" + (cost_ == kGameCost ? " (the game's own)" : "");
    }

    void OnBeginPlay(void* lm) override {
        SetCost(lm);
        SetPortalText();
    }

private:
    void SetCost(void* lm) {
        void* spawnables = At<void*>(lm, off::LM_SpawnablesDA);
        void* cls = spawnables ? At<void*>(spawnables, off::Spawnables_SpikedChest) : nullptr;
        void* cdo = cls ? At<void*>(cls, off::Class_DefaultObject) : nullptr;
        // Only the class's own default object, and only a chest that costs max health.
        if (!cdo || At<void*>(cdo, off::Object_Class) != cls ||
            !(At<uint32_t>(cdo, off::Object_Flags) & RF_ClassDefaultObject) ||
            At<uint8_t>(cdo, off::Chest_CurrencyType) != 3 /*MaxHealth*/) {
            if (!warned_) CRAB_LOG("spikedChest: can't find the Spiked Chest's defaults; its cost is unchanged");
            warned_ = true;
            return;
        }
        int& cost = At<int32_t>(cdo, off::Chest_Cost);
        if (cost == cost_) return;
        CRAB_LOG("Spiked Chest costs %d%% of base max health (was %d%%)", cost_, cost);
        cost = cost_;
    }

    void SetPortalText() {
        if (text_ == cost_) return;
        wchar_t s[va::kSpikedChestDescriptionLen + 1];
        if (cost_ == 0)
            swprintf(s, std::size(s), L"A chest that contains Epic loot and costs nothing to open");
        else
            swprintf(s, std::size(s), L"A chest that contains Epic loot but costs %d%% of your base max health to open",
                     cost_);
        // Pad with spaces to the literal's length: the game copies exactly that many characters.
        size_t n = wcslen(s);
        for (; n < size_t(va::kSpikedChestDescriptionLen); n++) s[n] = L' ';
        s[n] = 0;
        auto* dst = reinterpret_cast<wchar_t*>(Addr(va::Text_SpikedChestDescription));
        if (wcsncmp(dst, L"A chest that contains Epic loot ", 32) != 0) {
            CRAB_LOG("spikedChest: the portal description isn't where expected; left unchanged");
            text_ = cost_;
            return;
        }
        DWORD old;
        if (!VirtualProtect(dst, sizeof s, PAGE_READWRITE, &old)) return;
        memcpy(dst, s, sizeof s);
        VirtualProtect(dst, sizeof s, old, &old);
        text_ = cost_;
    }

    int cost_ = 35;
    int text_ = kGameCost;  // the cost the portal text says now
    bool warned_ = false;
};

SpikedChest g_item;

}  // namespace
}  // namespace crabmods
