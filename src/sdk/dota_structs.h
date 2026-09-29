#pragma once
#include <cstdint>
#include <cmath>
#include <windows.h>
#include "offsets.h"

template<typename T>
static inline T& Field(void* base, uintptr_t off) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + off);
}
template<typename T>
static inline const T& Field(const void* base, uintptr_t off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(base) + off);
}

// ─── Vector3 ─────────────────────────────────────────────────
struct Vector3 {
    float x=0, y=0, z=0;
    float Length2D() const { return sqrtf(x*x+y*y); }
    bool IsFinite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

// ─── CEntityHandle ───────────────────────────────────────────
struct CEntityHandle {
    uint32_t value = 0xFFFFFFFF;
    bool IsValid()  const { return value != 0xFFFFFFFF; }
    int  ToIndex()  const { return static_cast<int>(value & 0x7FFF); }
    int  ToSerial() const { return static_cast<int>((value >> 15) & 0x1FFFF); }
};

// ─── CGameSceneNode ──────────────────────────────────────────
class CGameSceneNode {
public:
    Vector3 GetAbsOrigin() const {
        return Field<Vector3>(this, Offsets::m_vecAbsOrigin);
    }
    CGameSceneNode()  = delete;
    ~CGameSceneNode() = delete;
};

// ─── C_BaseEntity ────────────────────────────────────────────
class C_BaseEntity {
public:
    CEntityHandle GetOwnerHandle() const {
        return Field<CEntityHandle>(this, Offsets::m_hOwnerEntity);
    }
    int32_t GetTeam() const {
        return Field<int32_t>(this, Offsets::m_iTeamNum);
    }
    CGameSceneNode* GetSceneNode() const {
        uintptr_t p = Field<uintptr_t>(this, Offsets::m_pGameSceneNode);
        return reinterpret_cast<CGameSceneNode*>(p);
    }

    // CEntityIdentity* лежит по (this - 0x8) в CEntityInstance (Source 2)
    uintptr_t GetIdentity() const {
        auto* self = reinterpret_cast<uint8_t*>(const_cast<C_BaseEntity*>(this));
        return *reinterpret_cast<uintptr_t*>(self - 0x8);
    }

    // CEntityHandle самого entity — лежит в CEntityIdentity
    // ⚠️ Оффсет 0x10 нужно проверить через ReClass
    CEntityHandle GetRefHandle() const {
        uintptr_t identity = GetIdentity();
        if (!identity) return {};
        return Field<CEntityHandle>(reinterpret_cast<void*>(identity),
                                    Offsets::m_EntityIdentityHandle);
    }

    C_BaseEntity()  = delete;
    ~C_BaseEntity() = delete;
};

// ─── C_DOTA_Item_Wearable ────────────────────────────────────
class C_DOTA_Item_Wearable : public C_BaseEntity {
public:
    uint32_t GetItemDefIndex() const {
        return Field<uint32_t>(this, Offsets::m_iItemDefinitionIndex);
    }
    void SetItemDefIndex(uint32_t v) {
        Field<uint32_t>(this, Offsets::m_iItemDefinitionIndex) = v;
    }
    uint32_t GetStyle() const { return Field<uint32_t>(this, Offsets::m_nStyle); }
    void     SetStyle(uint32_t s) { Field<uint32_t>(this, Offsets::m_nStyle) = s; }

    bool IsLikelyWearable() const {
        uint32_t def = GetItemDefIndex();
        return def > 0 && def < 100000;
    }

    C_DOTA_Item_Wearable()  = delete;
    ~C_DOTA_Item_Wearable() = delete;
};

// ─── C_DOTA_Unit_Hero ────────────────────────────────────────
class C_DOTA_Unit_Hero : public C_BaseEntity {
public:
    uint32_t GetHeroID()   const { return Field<uint32_t>(this, Offsets::m_nHeroID); }
    int32_t  GetHealth()   const { return Field<int32_t> (this, Offsets::m_iHealth); }
    int32_t  GetMaxHealth()const { return Field<int32_t> (this, Offsets::m_iMaxHealth); }
    float    GetMana()     const { return Field<float>   (this, Offsets::m_flMana); }
    float    GetMaxMana()  const { return Field<float>   (this, Offsets::m_flMaxMana); }

    bool IsValidHero() const {
        uint32_t hid = GetHeroID();
        if (hid == 0 || hid > 200) return false;
        int32_t hp = GetHealth();
        if (hp < 0 || hp > 100000) return false;
        return true;
    }

    Vector3 GetAbsOrigin() const {
        auto* node = GetSceneNode();
        if (!node) return {};
        return node->GetAbsOrigin();
    }

    CEntityHandle* GetWearableHandles(int& outCount) const {
        auto* self = reinterpret_cast<uint8_t*>(const_cast<C_DOTA_Unit_Hero*>(this));
        auto* vec  = self + Offsets::m_hMyWearables;

        auto* data = *reinterpret_cast<CEntityHandle**>(vec + Offsets::kUtlVecDataOff);
        int   cnt  = *reinterpret_cast<int*>(vec + Offsets::kUtlVecSizeOff);

        outCount = (cnt > 0 && cnt <= 64 && data) ? cnt : 0;
        return data;
    }

    C_DOTA_Unit_Hero()  = delete;
    ~C_DOTA_Unit_Hero() = delete;
};

// ─── HeroSnapshot ────────────────────────────────────────────
struct HeroSnapshot {
    bool      valid     = false;
    uint32_t  heroID    = 0;
    int32_t   health    = 0;
    int32_t   maxHealth = 1;
    float     mana      = 0;
    float     maxMana   = 1;
    int32_t   team      = 0;
    Vector3   origin    = {};
    uintptr_t ptr       = 0;
};