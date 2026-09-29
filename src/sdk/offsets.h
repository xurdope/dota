#pragma once
#include <cstdint>

// ============================================================
//  Dota 2 Source 2 — Memory Offsets (Капибариный стандарт)
// ============================================================

namespace Offsets {

    // ─── Главные указатели систем ────────────────────────────
    constexpr uintptr_t dwEntitySystem    = 0x0; // Динамический поиск через интерфейс или паттерн
    constexpr uintptr_t dwLocalPlayerPawn = 0x0; // Через локальный контроллер
    constexpr uintptr_t dwViewMatrix      = 0x0; 

    // ─── C_BaseEntity / CEntityInstance ──────────────────────
    constexpr uintptr_t m_hOwnerEntity   = 0x3C8; 
    constexpr uintptr_t m_iTeamNum       = 0x3B4;
    constexpr uintptr_t m_pGameSceneNode = 0x118; 

    // ─── CGameSceneNode ──────────────────────────────────────
    constexpr uintptr_t m_vecAbsOrigin   = 0xC8;  

    // ─── C_DOTA_Unit_Hero ────────────────────────────────────
    constexpr uintptr_t m_nHeroID        = 0x5AC;
    constexpr uintptr_t m_hMyWearables   = 0xEF8;
    constexpr uintptr_t m_iHealth        = 0x33C;
    constexpr uintptr_t m_iMaxHealth     = 0x340;
    constexpr uintptr_t m_flMana         = 0x440;
    constexpr uintptr_t m_flMaxMana      = 0x444;

    // ─── C_DOTA_Item_Wearable (Предметы / Шмотки) ───────────
    constexpr uintptr_t m_iItemDefinitionIndex = 0x1B8;
    constexpr uintptr_t m_nStyle               = 0x1BC;
    constexpr uintptr_t m_nFallbackPaintKit     = 0x1C0;
    constexpr uintptr_t m_flFallbackWear         = 0x1C4;
    constexpr uintptr_t m_nFallbackSeed         = 0x1C8;

    // ─── CEntityIdentity & CGameEntitySystem (Source 2 Core) ─
    constexpr uintptr_t m_EntityIdentityHandle = 0x10;

    // Актуальные размеры чанков сущностей Source 2 (проверено патчами движка)
    constexpr uintptr_t kEntityListOff    = 0x10;
    constexpr uintptr_t kEntityStride     = 0x70; // Скорректировано под актуальный размер блока энтити
    constexpr uintptr_t kEntityPtrOff     = 0x00;
    constexpr int       kEntitiesPerChunk = 512;
    constexpr int       kMaxEntities      = 0x4000;

    // ─── CUtlVector (64-bit) ─────────────────────────────────
    constexpr uintptr_t kUtlVecDataOff = 0x00;
    constexpr uintptr_t kUtlVecSizeOff = 0x08;

} // namespace Offsets