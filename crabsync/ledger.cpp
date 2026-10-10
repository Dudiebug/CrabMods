#include "ledger.hpp"

#include <algorithm>
#include <cstdio>

namespace crabsync {

const Item* FindItem(const std::vector<Item>& v, ItemId id) {
    for (auto& i : v)
        if (i.id == id) return &i;
    return nullptr;
}

Item* FindItem(std::vector<Item>& v, ItemId id) {
    for (auto& i : v)
        if (i.id == id) return &i;
    return nullptr;
}

static uint8_t ClampByte(int v) { return uint8_t(std::clamp(v, 0, 255)); }

void Ledger::reset() {
    hasTeam_ = false;
    healthValid_ = false;
    damage_ = teamMax_ = 0.f;
    team_ = {};
    base_.clear();
    poolEmpty_ = false;
    doomed_.clear();
    syncMax_ = false;
    refund_.clear();
}

float Ledger::chestShare(float cost) const {
    switch (s_.chestCost) {
    case ChestCost::Full: return cost;
    case ChestCost::Free: return 0.f;
    default: return cost / float(players_ ? players_ : 1);
    }
}

void Ledger::addBaseMax(float amount) {
    if (maxSynced()) team_.baseMax += amount;
}

void Ledger::chestCost(PlayerKey k, float baseMax, float health) {
    if (!maxSynced()) return;
    // The opener's own base is overwritten with the team's on the next poll, so only the share counts.
    team_.baseMax += chestShare(baseMax);
    if (health > 0.f) refund_[k] += health - chestShare(health);
}

void Ledger::updateTeamHealth() {
    team_.maxHealth = teamMax_;
    team_.health = healthValid_ ? std::max(0.f, teamMax_ - damage_) : 0.f;
}

void Ledger::retain(const std::vector<PlayerKey>& present) {
    for (auto it = base_.begin(); it != base_.end();) {
        if (std::find(present.begin(), present.end(), it->first) == present.end())
            it = base_.erase(it);
        else
            ++it;
    }
}

void Ledger::seed(const std::vector<const Inventory*>& players) {
    healthValid_ = false;
    damage_ = teamMax_ = 0.f;
    team_ = {};
    refund_.clear();
    // Everyone starts a run with the same base max; the highest covers anyone read mid-reset.
    syncMax_ = !players.empty();
    for (size_t i = 0; i < players.size(); i++)
        if (i == 0 || players[i]->baseMax > team_.baseMax) team_.baseMax = players[i]->baseMax;
    // Equipment is not seeded: everyone keeps their own starting gear until someone swaps.
    for (auto* p : players) {
        if (p->life == Life::Alive) {
            float dmg = std::max(0.f, p->maxHealth - p->health);
            if (!healthValid_ || dmg < damage_) damage_ = dmg;
            teamMax_ = std::max(teamMax_, p->maxHealth);
            healthValid_ = true;
        }
        for (int k = 0; k < NumSlotKinds; k++) team_.slots[k] = std::max(team_.slots[k], p->slots[k]);
        team_.crystals = std::max(team_.crystals, p->crystals);
        for (int c = 0; c < NumCats; c++) {
            for (auto& it : p->items[c]) {
                Item* t = FindItem(team_.items[c], it.id);
                if (!t)
                    team_.items[c].push_back(it);
                else if (it.level > t->level)
                    *t = it;
            }
        }
    }
    hasTeam_ = true;
    updateTeamHealth();
    // Starting from an already empty pool is not the pool emptying: nobody is killed for it.
    poolEmpty_ = poolIsEmpty();
    doomed_.clear();
}

void Ledger::absorbItem(int c, const Item* before, const Item* now, std::vector<Change>* changes) {
    ItemId id = now ? now->id : before->id;
    auto& team = team_.items[c];
    Item* t = FindItem(team, id);
    int delta = (now ? now->level : 0) - (before ? before->level : 0);

    if (!now) {
        // The player lost the whole stack (salvaged or swapped it out): the team loses it too.
        if (t) {
            team.erase(team.begin() + (t - team.data()));
            if (changes) changes->push_back({std::string("removed ") + CatName(c), id});
        }
        return;
    }
    if (!t) {
        if (delta <= 0) return;
        Item n = *now;
        n.level = ClampByte(delta);
        if (!s_.buffs) n.buff = 0.f;
        if (!s_.enhancements) n.enhancements.clear();
        team.push_back(std::move(n));
        if (changes) changes->push_back({std::string("gained ") + CatName(c) + " x" + std::to_string(delta), id});
        return;
    }
    if (delta) {
        t->level = ClampByte(t->level + delta);
        if (changes)
            changes->push_back({std::string(delta > 0 ? "gained " : "lost ") + CatName(c) + " x" +
                                    std::to_string(delta > 0 ? delta : -delta),
                                id});
    }
    if (s_.enhancements && before && now->enhancements != before->enhancements) {
        t->enhancements = now->enhancements;
        if (changes) changes->push_back({std::string("enhanced ") + CatName(c), id});
    }
    if (s_.buffs) {
        float d = now->buff - (before ? before->buff : 0.f);
        if (d != 0.f) t->buff += d;
    }
    if (t->level == 0) team.erase(team.begin() + (t - team.data()));
}

void Ledger::absorbHealth(PlayerKey k, const Inventory& before, const Inventory& now, std::vector<Change>* changes) {
    if (now.life == Life::None) return;
    if (now.life == Life::Alive) teamMax_ = std::max(teamMax_, now.maxHealth);
    if (!healthValid_) {
        if (now.life == Life::Alive) {
            damage_ = std::max(0.f, now.maxHealth - now.health);
            healthValid_ = true;
        }
        return;
    }
    if (before.life != Life::Alive) return;
    if (now.life == Life::Dead) {
        // Everyone shares one pool, so a death means it is empty: the rest die with them. This is also how
        // a death is seen at all: the game swaps the character for a spectator in the frame it dies, so
        // the killing hit is never read.
        if (damage_ < teamMax_) damage_ = teamMax_;
        refund_.erase(k);
        updateTeamHealth();
        if (changes) changes->push_back({"died: the team pool is empty"});
        return;
    }
    // Only a change on the same living character counts: a respawn, a revive or a character that just
    // appeared is a new starting point, not damage or healing.
    if (before.body != now.body) return;
    // A lowered max health cuts health down to it; that is not damage.
    float was = std::min(before.health, now.maxHealth);
    float d = now.health - was;
    if (auto r = refund_.find(k); r != refund_.end() && d < 0.f) {
        // A chest's cost: the team only pays its share.
        float back = std::min(r->second, -d);
        d += back;
        if ((r->second -= back) <= 0.f) refund_.erase(r);
    }
    if (d == 0.f) return;
    damage_ = std::max(0.f, damage_ - d);
    updateTeamHealth();
    if (changes && (d <= -1.f || d >= 1.f)) {
        char buf[96];
        snprintf(buf, sizeof buf, "%s %.0f health (team pool now %.0f/%.0f)", d > 0 ? "healed" : "took",
                 d > 0 ? d : -d, team_.health, team_.maxHealth);
        changes->push_back({buf});
    }
}

bool Ledger::absorb(PlayerKey k, const Inventory& now, std::vector<Change>* changes) {
    auto it = base_.find(k);
    if (!hasTeam_ || it == base_.end()) return false;
    const Inventory& before = it->second;
    if (before == now) return false;
    bool changed = false;

    for (int e = 0; e < NumEquip; e++) {
        if (s_.equip[e] && now.equip[e] != before.equip[e] && now.equip[e]) {
            team_.equip[e] = now.equip[e];
            changed = true;
            if (changes) changes->push_back({std::string("equipped ") + EquipName(e), now.equip[e]});
        }
    }
    if (s_.slots) {
        for (int s = 0; s < NumSlotKinds; s++) {
            int d = int(now.slots[s]) - int(before.slots[s]);
            if (d) {
                team_.slots[s] = ClampByte(team_.slots[s] + d);
                changed = true;
                if (changes) changes->push_back({"slots " + std::to_string(s) + (d > 0 ? " +" : " ") + std::to_string(d)});
            }
        }
    }
    if (s_.health && !lobby_) {
        float was = damage_, max = teamMax_;
        bool valid = healthValid_;
        absorbHealth(k, before, now, changes);
        updateTeamHealth();
        changed |= was != damage_ || max != teamMax_ || valid != healthValid_;
    }
    if (s_.crystals && now.crystals != before.crystals) {
        int64_t v = int64_t(team_.crystals) + int64_t(now.crystals) - int64_t(before.crystals);
        team_.crystals = uint32_t(std::clamp<int64_t>(v, 0, UINT32_MAX));
        changed = true;
        if (changes)
            changes->push_back({"crystals " + std::to_string(int64_t(now.crystals) - int64_t(before.crystals))});
    }
    for (int c = 0; c < NumCats; c++) {
        if (!s_.cats[c] || now.items[c] == before.items[c]) continue;
        changed = true;
        for (auto& i : now.items[c]) {
            const Item* b = FindItem(before.items[c], i.id);
            if (!b || !(*b == i)) absorbItem(c, b, &i, changes);
        }
        for (auto& b : before.items[c])
            if (!FindItem(now.items[c], b.id)) absorbItem(c, &b, nullptr, changes);
    }
    it->second = now;
    return changed;
}

void Ledger::absorbAll(const std::vector<PlayerKey>& keys, const std::vector<Inventory>& now,
                       std::vector<std::vector<Change>>* changes, const std::vector<bool>* hold) {
    if (changes) changes->assign(keys.size(), {});
    players_ = keys.size();
    // Recomputed each poll so a player leaving lowers it.
    float prevMax = teamMax_;
    teamMax_ = 0.f;
    for (auto& n : now)
        if (n.life == Life::Alive) teamMax_ = std::max(teamMax_, n.maxHealth);
    // A lower max cuts health down to it, as the game does, instead of taking the difference off it:
    // losing 5 max health at 100/1000 leaves 100/995, not 95/995. A higher max adds to health.
    if (s_.health && !lobby_ && healthValid_ && prevMax > 0.f && teamMax_ > 0.f && teamMax_ < prevMax)
        damage_ = std::max(0.f, damage_ - (prevMax - teamMax_));
    updateTeamHealth();
    if (!hasTeam_) return;

    float damageBefore = damage_;
    std::vector<size_t> seen;  // players with a baseline
    for (size_t i = 0; i < keys.size(); i++)
        if (base_.count(keys[i])) seen.push_back(i);
    // The change is kept by the first player, so put players being absorbed now first.
    std::stable_partition(seen.begin(), seen.end(), [&](size_t i) { return !hold || !(*hold)[i]; });
    if (seen.size() >= 2) {
        // A shared game event: every player changed by the same amount. Keep the first player's change
        // and fold it into the others' baselines so it is absorbed once.
        auto shared = [&](auto delta) {
            auto d0 = delta(base_[keys[seen[0]]], now[seen[0]]);
            if (!d0) return false;
            for (size_t j : seen)
                if (delta(base_[keys[j]], now[j]) != d0) return false;
            return true;
        };
        for (int s = 0; s < NumSlotKinds; s++) {
            if (shared([s](const Inventory& b, const Inventory& n) { return int(n.slots[s]) - int(b.slots[s]); }))
                for (size_t j = 1; j < seen.size(); j++) base_[keys[seen[j]]].slots[s] = now[seen[j]].slots[s];
        }
        if (shared([](const Inventory& b, const Inventory& n) { return int64_t(n.crystals) - int64_t(b.crystals); }))
            for (size_t j = 1; j < seen.size(); j++) base_[keys[seen[j]]].crystals = now[seen[j]].crystals;
    }

    for (size_t i = 0; i < keys.size(); i++)
        if (!hold || !(*hold)[i]) absorb(keys[i], now[i], changes ? &(*changes)[i] : nullptr);

    bool empty = poolIsEmpty();
    if (empty && !poolEmpty_ && damage_ > damageBefore) {
        // Damage emptied the pool: everyone alive now dies.
        doomed_.clear();
        for (auto& n : now)
            if (n.life == Life::Alive) doomed_.push_back(n.body);
    } else if (empty && doomed_.empty()) {
        // Empty without anyone taking the killing damage: a lower max (new or respawned characters) or a
        // pool left over from before. Start it again from whoever is alive instead of killing them.
        healthValid_ = false;
        for (auto& n : now) {
            if (n.life != Life::Alive) continue;
            float dmg = std::max(0.f, n.maxHealth - n.health);
            if (!healthValid_ || dmg < damage_) damage_ = dmg;
            healthValid_ = true;
        }
        updateTeamHealth();
        empty = poolIsEmpty();
    } else if (!empty) {
        doomed_.clear();
    }
    poolEmpty_ = empty;
}

Inventory Ledger::target(const Inventory& cur) const {
    Inventory t = cur;
    if (!hasTeam_) return t;
    for (int e = 0; e < NumEquip; e++)
        if (s_.equip[e] && team_.equip[e]) t.equip[e] = team_.equip[e];
    if (s_.slots) t.slots = team_.slots;
    if (s_.crystals) t.crystals = team_.crystals;
    if (maxSynced()) t.baseMax = team_.baseMax;

    if (s_.health && lobby_ && cur.life == Life::Alive) {
        t.health = cur.maxHealth;
    } else if (s_.health && healthValid_ && cur.life == Life::Alive) {
        if (std::find(doomed_.begin(), doomed_.end(), cur.body) != doomed_.end()) {
            t.life = Life::Dead;  // the pool emptied: everyone alive at that moment dies
            t.health = 0.f;
        } else {
            // Everyone has the same max once it is synced. One that is briefly lower (a character that just
            // spawned, or a max change not yet applied to everyone) is filled up to it, and kept alive.
            t.health = std::clamp(team_.health, std::min(1.f, cur.maxHealth), cur.maxHealth);

        }
    }
    for (int c = 0; c < NumCats; c++) {
        if (!s_.cats[c]) continue;
        // Keep the player's own order and append new items, matching how the game's arrays change
        // (removal compacts in place, additions go to the end), so applying it converges exactly.
        auto& out = t.items[c];
        out.clear();
        for (auto& mine : cur.items[c])
            if (const Item* ti = FindItem(team_.items[c], mine.id)) out.push_back(*ti);
        for (auto& ti : team_.items[c])
            if (!FindItem(cur.items[c], ti.id)) out.push_back(ti);
        for (auto& i : out) {
            const Item* mine = FindItem(cur.items[c], i.id);
            if (!s_.enhancements) i.enhancements = mine ? mine->enhancements : std::vector<uint8_t>{};
            if (!s_.buffs) i.buff = mine ? mine->buff : 0.f;
        }
    }
    return t;
}

}  // namespace crabsync
