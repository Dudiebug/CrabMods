// Cursed Lottery, a Greed relic: a huge pile of crystals when you pick it up, and then almost none of the
// crystals you'd gain for as long as you hold it. It can't be dropped (that would end the curse, and picking it
// up again would pay out again).
//
// Host: ACrabPlayerC::GivePickup hands a player each pickup; when it gives this relic to someone who didn't
// hold it, they get the crystals. CrabSync gives every player copies of pickups through the same function, so
// only the game's own calls pay out (the shared wallet would get it once per player otherwise).
// Every gain goes through ACrabPS::AddCrystals (costs too, as negative amounts): a holder's gains are cut to a
// share, with the fraction carried over so small gains still add up. ServerDropPickup is refused for it.
#include <windows.h>

#include <cmath>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"

namespace crabmods {
namespace {

// true: the address is in the game's own code (not CrabSync's or ours).
bool InGame(uintptr_t at) {
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return at >= base && at < base + nt->OptionalHeader.SizeOfImage;
}

class CursedLottery : public AssetItem {
public:
    CursedLottery()
        : AssetItem("cursedLottery", 125,
                    {Kind::Relic, L"DA_Relic_CursedLottery", L"Cursed Lottery",
                     L"A gigantic amount of crystals, and then almost none",
                     L"Gain 100000 crystals, but only 0.01% of the crystals you'd gain after. Can't be dropped", 213,
                     63 /* HighRoller */, pool::Greed, 0xFF, 0.15f},
                    rarity::Greed) {
        self_ = this;
    }

    std::string DefaultIni() const override {
        return R"(# Cursed Lottery, a Greed relic: a gigantic amount of crystals when you pick it up, and then you only get a
# tiny share of the crystals you'd gain while you hold it. It can't be dropped.
enabled = true

# Crystals when you pick it up.
crystals = 100000

# The share of crystal gains you still get while holding it, in percent (0.01 = one crystal in 10000).
gainPercent = 0.01

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "crystals") crystals_ = int(std::fmin(std::fmax(ParseFloat(v), 0.f), 2.0e9f));
        else if (k == "gainPercent") gain_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::to_string(crystals_) + " crystals on pickup, then " + Num(gain_) + "% of crystal gains, " +
               RarityName(rarity_);
    }

    void InstallHooks() override {
        Hook(va::ACrabPlayerC_GivePickup, &HkGive, &oGive, "ACrabPlayerC::GivePickup");
        Hook(va::ACrabPS_AddCrystals, &HkAddCrystals, &oAddCrystals, "ACrabPS::AddCrystals");
        Hook(va::ACrabPlayerC_ServerDropPickup, &HkDrop, &oDrop, "ACrabPlayerC::ServerDropPickup");
    }

protected:
    void Customize(void* da) override {
        std::string g = Num(gain_);
        std::wstring text = L"Gain " + std::to_wstring(crystals_) + L" crystals, but only " +
                            std::wstring(g.begin(), g.end()) +
                            L"% of the crystals you'd gain after. Can't be dropped";
        SetFString(da, off::DA_LevelDescription, text.c_str());
    }

private:
    using GiveFn = void (*)(void*, void*);
    using AddCrystalsFn = void (*)(void*, int);
    using DropFn = void (*)(void*, void*, void*);
    static inline GiveFn oGive;
    static inline AddCrystalsFn oAddCrystals;
    static inline DropFn oDrop;
    static inline CursedLottery* self_;

    bool Active() const { return enabled_ && da_; }

    static void HkGive(void* pc, void* info) {
        CursedLottery& self = *self_;
        void* ps = pc ? At<void*>(pc, off::Pawn_PlayerState) : nullptr;
        if (!self.Active() || !info || !ps || At<void*>(info, 0) != self.da_) return oGive(pc, info);
        uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        int before = self.Level(ps);
        oGive(pc, info);
        if (before > 0 || self.Level(ps) == 0) return;
        if (At<uint8_t>(ps, off::Actor_Role) != ROLE_Authority || !InGame(caller)) {
            CRAB_LOG("Cursed Lottery: %p got a copy (not the game's pickup), no crystals", ps);
            return;
        }
        self.carry_[ps] = 0.0;
        if (self.crystals_ > 0) oAddCrystals(ps, self.crystals_);
        CRAB_LOG("Cursed Lottery: %p won %d crystals", ps, self.crystals_);
        ShowMessage(ps, L"Cursed Lottery: you won the jackpot! Crystals are scarce from now on");
    }

    static void HkAddCrystals(void* ps, int amount) {
        CursedLottery& self = *self_;
        if (amount <= 0 || !ps || !self.Active() || self.Level(ps) == 0) return oAddCrystals(ps, amount);
        double& carry = self.carry_[ps];
        carry += double(amount) * self.gain_ / 100.0;
        int kept = int(std::floor(carry));
        carry -= kept;
        if (kept > 0) oAddCrystals(ps, kept);
    }

    static void HkDrop(void* pc, void* da, void* info) {
        CursedLottery& self = *self_;
        void* ps = pc ? At<void*>(pc, off::Pawn_PlayerState) : nullptr;
        if (self.Active() && da && da == self.da_ && ps && self.Level(ps) > 0) {
            ShowMessage(ps, L"Cursed Lottery is cursed: it can't be dropped");
            return;
        }
        oDrop(pc, da, info);
    }

    int crystals_ = 100000;
    float gain_ = 0.01f;
    std::unordered_map<void*, double> carry_;  // player state -> fraction of a crystal owed
};

CursedLottery g_item;

}  // namespace
}  // namespace crabmods
