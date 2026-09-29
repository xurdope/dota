#pragma once
// ============================================================
//  Dota 2 Source 2 — Memory Offsets
// ============================================================
#include <cstdint>

namespace Offsets {

    // ─── Главные указатели систем ────────────────────────────
    constexpr uintptr_t dwEntitySystem    = 0x0;
    constexpr uintptr_t dwLocalPlayerPawn = 0x0;
    constexpr uintptr_t dwViewMatrix      = 0x0;

    // ─── C_BaseEntity / CEntityInstance ──────────────────────
    constexpr uintptr_t m_hOwnerEntity   = 0x3C8;
    constexpr uintptr_t m_iTeamNum       = 0x3B4;
    constexpr uintptr_t m_pGameSceneNode = 0x118;

    // ─── CGameSceneNode ──────────────────────────────────────
    constexpr uintptr_t m_vecAbsOrigin   = 0xC8;

    // ─── C_DOTA_Unit_Hero ────────────────────────────────────
    constexpr uintptr_t m_nHeroID        = 0x588;
    constexpr uintptr_t m_hMyWearables   = 0xE50;
    constexpr uintptr_t m_iHealth        = 0x33C;
    constexpr uintptr_t m_iMaxHealth     = 0x340;
    constexpr uintptr_t m_flMana         = 0x440;
    constexpr uintptr_t m_flMaxMana      = 0x444;

    // ─── C_DOTA_Item_Wearable ───────────────────────────────
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1B8;
    constexpr uintptr_t m_nStyle               = 0x1BC;
    constexpr uintptr_t m_nFallbackPaintKit     = 0x1C0;
    constexpr uintptr_t m_flFallbackWear         = 0x1C4;
    constexpr uintptr_t m_nFallbackSeed         = 0x1C8;

    // ─── Entity Identity ─────────────────────────────────────
    constexpr uintptr_t m_EntityIdentityHandle = 0x08;

    // ─── Entity List (chunks) ────────────────────────────────
    constexpr uintptr_t kEntityListOff    = 0x28;
    constexpr uintptr_t kEntityStride     = 0x70;
    constexpr uintptr_t kEntityPtrOff     = 0x00;
    constexpr int       kEntitiesPerChunk = 512;
    constexpr int       kMaxEntities      = 0x4000;

    // ─── CUtlVector layout ───────────────────────────────────
    constexpr uintptr_t kUtlVecDataOff = 0x00;  // pointer to data array
    constexpr uintptr_t kUtlVecSizeOff = 0x08;  // int32 count

} // namespace Offsets