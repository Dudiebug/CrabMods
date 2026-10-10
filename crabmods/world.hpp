// The game world as items see it: the level manager, the players, their characters and health.
#pragma once
#include <crab/ue.hpp>

namespace crabmods {

using namespace crab;

// The world any actor or component is in (UEngine::GetWorldFromContextObject, as the game's own code calls it).
inline void* WorldOf(void* context) {
    void* engine = *reinterpret_cast<void**>(Addr(va::GEngine));
    if (!engine || !context) return nullptr;
    return Fn<void* (*)(void*, void*, int)>(va::UEngine_GetWorldFromContextObject)(engine, context, 1);
}

// ACrabGS of the world, or null (main menu, or not a Crab game state).
inline void* GameStateOf(void* context) {
    void* world = WorldOf(context);
    void* gs = world ? At<void*>(world, off::World_GameState) : nullptr;
    return IsA(gs, StaticClass(va::ACrabGS_StaticClass)) ? gs : nullptr;
}

inline void* LevelManagerOf(void* context) {
    void* gs = GameStateOf(context);
    return gs ? At<void*>(gs, off::GS_LM) : nullptr;
}

// Calls f(ACrabPS*) for every player in the game, the local one included.
template <class F>
void ForEachPlayer(void* context, F&& f) {
    void* gs = GameStateOf(context);
    if (!gs) return;
    auto& players = At<TArray<void*>>(gs, off::GS_PlayerArray);
    void* psClass = StaticClass(va::ACrabPS_StaticClass);
    for (int i = 0; i < players.Num; i++)
        if (void* ps = players.Data[i]; IsA(ps, psClass)) f(ps);
}

inline int PlayerCount(void* context) {
    int n = 0;
    ForEachPlayer(context, [&](void*) { n++; });
    return n;
}

// The player's character (ACrabC*), or null while it has none (between levels, spectating).
inline void* PawnOf(void* ps) {
    void* pawn = ps ? At<void*>(ps, off::PS_Pawn) : nullptr;
    return IsA(pawn, StaticClass(va::ACrabC_StaticClass)) ? pawn : nullptr;
}

// Down: dead and waiting to be revived. A dead player spectates (their pawn is an ACrabSpectatorC); their
// character is marked eliminated in the moment before that. Between levels (no pawn) nobody is down.
inline bool IsDown(void* ps) {
    void* pawn = ps ? At<void*>(ps, off::PS_Pawn) : nullptr;
    if (!pawn) return false;
    if (IsA(pawn, StaticClass(va::ACrabSpectatorC_StaticClass))) return true;
    return IsA(pawn, StaticClass(va::ACrabC_StaticClass)) && (At<uint8_t>(pawn, off::C_bIsEliminated) & 1);
}

// The character's health component (UCrabHC*).
inline void* HealthOf(void* ps) {
    void* c = PawnOf(ps);
    return c ? At<void*>(c, off::C_HC) : nullptr;
}

// Current / max health in 0..1, or -1 without a character.
inline float HealthFraction(void* ps) {
    void* hc = HealthOf(ps);
    if (!hc) return -1.f;
    float max = At<float>(hc, off::HC_CurrentMaxHealth);
    return max > 0.f ? At<float>(hc, off::HC_CurrentHealth) / max : -1.f;
}

// The player that owns a health component, or null for enemies.
inline void* PlayerOfHealth(void* hc) {
    void* c = hc ? At<void*>(hc, off::HC_OwningC) : nullptr;
    void* ps = c ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
    return IsA(ps, StaticClass(va::ACrabPS_StaticClass)) ? ps : nullptr;
}

}  // namespace crabmods
