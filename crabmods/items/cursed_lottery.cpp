// Cursed Lottery, a Greed relic: a big pile of crystals when you pick it up (more the further the run has gone),
// and then almost none of the crystals you'd gain for as long as you hold it. Money Is Power does nothing while you
// hold it. It can't be dropped (that would end the curse, and picking it up again would pay out again).
//
// Host: ACrabPlayerC::GivePickup hands a player each pickup; when it gives this relic to someone who didn't
// hold it, they get the crystals. CrabSync gives every player copies of pickups through the same function, so
// only the game's own calls pay out (the shared wallet would get it once per player otherwise).
// Every gain goes through ACrabPS::AddCrystals (costs too, as negative amounts): a holder's gains are cut to a
// share, with the fraction carried over so small gains still add up. ServerDropPickup is refused for it.
// Every machine: Money Is Power adds min(crystals, 25000 x level) x 0.00004 to the damage pool (0x140d23700); a
// holder's DamageBonus takes the same amount off. Money Is Power's card says so.
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../world.hpp"

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
                     L"A big pile of crystals, and then almost none",
                     L"Gain 15000 crystals, more each island, but only 0.01% of the crystals you'd gain after. Money "
                     L"Is Power does nothing. Can't be dropped",
                     213,
                     63 /* HighRoller */, pool::Greed, 0xFF, 0.15f},
                    rarity::Greed) {
        self_ = this;
    }

    std::string DefaultIni() const override {
        return R"(# Cursed Lottery, a Greed relic: a big pile of crystals when you pick it up, and then you only get a tiny
# share of the crystals you'd gain while you hold it. Money Is Power does nothing while you hold it. It can't be
# dropped.
enabled = true

# Crystals when you pick it up on island 0. The payout grows by this much again every islandsToDouble islands
# (15000 on island 0, 30000 on island 14, 45000 on island 28 with the defaults). 0 for a fixed payout.
crystals = 15000
islandsToDouble = 14

# The share of crystal gains you still get while holding it, in percent (0.01 = one crystal in 10000).
gainPercent = 0.01

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "crystals") crystals_ = int(std::fmin(std::fmax(ParseFloat(v), 0.f), 2.0e9f));
        else if (k == "islandsToDouble") islandsToDouble_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 1000.f);
        else if (k == "gainPercent") gain_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::to_string(crystals_) + " crystals on pickup" +
               (islandsToDouble_ > 0.f ? " (+100% every " + Num(islandsToDouble_) + " islands)" : std::string()) +
               ", then " + Num(gain_) + "% of crystal gains, no Money Is Power, " + RarityName(rarity_);
    }

    // Takes off what Money Is Power adds.
    float DamageBonus(void* ps) override {
        if (!Active() || Level(ps) == 0) return 0.f;
        int level = 0;
        ForEachItem(ps, off::PS_Perks, [&](void* da, int l) {
            if (At<uint8_t>(da, off::DA_ItemType) == kMoneyIsPower) level += l;
        });
        if (level == 0) return 0.f;
        int crystals = std::max(At<int>(ps, off::PS_Crystals), 0);
        return -float(std::min(crystals, kMoneyCap * level)) * kMoneyPerCrystal;
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        // Money Is Power's card: the game's text, and a line about the curse while the Lottery can drop.
        void* money = FindByType(Spawnables(lm, off::Spawnables_Perks), kMoneyIsPower);
        if (!money) return;
        auto& s = At<TArray<wchar_t>>(money, off::DA_LevelDescription);
        if (moneyText_.empty())
            for (int i = 0; s.Data && i < s.Num && s.Data[i]; i++) moneyText_.push_back(s.Data[i]);
        std::wstring text = moneyText_;
        if (Active()) {
            while (!text.empty() && (text.back() == L'.' || text.back() == L' ')) text.pop_back();
            text += L". Does nothing while you hold Cursed Lottery";
        }
        SetFString(money, off::DA_LevelDescription, text.c_str());
    }

    void InstallHooks() override {
        Hook(va::ACrabPlayerC_GivePickup, &HkGive, &oGive, "ACrabPlayerC::GivePickup");
        Hook(va::ACrabPS_AddCrystals, &HkAddCrystals, &oAddCrystals, "ACrabPS::AddCrystals");
        Hook(va::ACrabPlayerC_ServerDropPickup, &HkDrop, &oDrop, "ACrabPlayerC::ServerDropPickup");
    }

protected:
    void Customize(void* da) override {
        std::string g = Num(gain_);
        std::wstring text = L"Gain " + std::to_wstring(crystals_) + L" crystals" +
                            (islandsToDouble_ > 0.f ? L", more each island," : L",") + L" but only " +
                            std::wstring(g.begin(), g.end()) +
                            L"% of the crystals you'd gain after. Money Is Power does nothing. Can't be dropped";
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

    static constexpr uint8_t kMoneyIsPower = 73;    // PerkType
    static constexpr int kMoneyCap = 25000;          // crystals counted per Money Is Power level
    static constexpr float kMoneyPerCrystal = 4e-5f;  // +100% at the cap

    bool Active() const { return enabled_ && da_; }

    // crystals x (1 + island / islandsToDouble).
    int Payout(void* ps) const {
        void* gs = GameStateOf(ps);
        int island = gs ? std::max(At<int>(gs, off::GS_CurrentIsland), 0) : 0;
        double scale = islandsToDouble_ > 0.f ? 1.0 + island / double(islandsToDouble_) : 1.0;
        return int(std::fmin(crystals_ * scale, 2.0e9));
    }

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
        int won = self.Payout(ps);
        if (won > 0) oAddCrystals(ps, won);
        CRAB_LOG("Cursed Lottery: %p won %d crystals", ps, won);
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

    int crystals_ = 15000;
    float islandsToDouble_ = 14.f;
    float gain_ = 0.01f;
    std::wstring moneyText_;  // Money Is Power's own level description
    std::unordered_map<void*, double> carry_;  // player state -> fraction of a crystal owed
};

CursedLottery g_item;

}  // namespace
}  // namespace crabmods
