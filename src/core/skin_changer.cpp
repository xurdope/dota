// ============================================================
//  InventoryChanger — Per-hero, per-slot item override
//
//  Scan flow:
//    1. Iterate entity list -> find hero entities by m_nHeroID
//    2. For each hero with active overrides:
//       a. Read m_hMyWearables CUtlVector
//       b. Resolve each handle to a wearable entity ptr
//       c. Patch defIndex / style / wear fallback fields
//    3. Direct Standalone Wearable Entity Scan (Armory Preview Models)
//    4. Trigger scene dirty flags for visual update
// ============================================================
#include "skin_changer.h"
#include "offset_scanner.h"
#include "memory_guard.h"
#include "pattern_scan.h"
#include "econ_hook.h"
#include "../system/system_utils.h"
#include "../sdk/offsets.h"
#include "../data/item_schema.h"
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdarg>

// ─────────────────────────────────────────────────────────────
//  EntitySystem lookup (interface -> pattern fallback)
// ─────────────────────────────────────────────────────────────
using CreateInterfaceFn = void* (*)(const char*, int*);

static bool IsValidVtable(uintptr_t vtbl) {
    if (!SafeMemoryOps::IsValid(vtbl)) return false;
    uintptr_t fn0 = 0;
    if (!SafeMemoryOps::Read64(vtbl, fn0) || !SafeMemoryOps::IsValid(fn0)) return false;
    return true;
}

static uintptr_t FindEntitySystemViaInterface() {
    static const char* names[] = {
        "Source2GameEntities001", "GameEntitySystem_001", "Source2GameEntities_001", "EntitySystem_001"
    };

    HMODULE hClient = GetModuleHandleA("client.dll");
    if (hClient) {
        auto fn = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hClient, "CreateInterface"));
        if (fn) {
            for (auto name : names) {
                int ret = 0;
                void* p = fn(name, &ret);
                if (p && ret == 0) return reinterpret_cast<uintptr_t>(p);
            }
        }
    }

    HMODULE hEngine = GetModuleHandleA("engine2.dll");
    if (hEngine) {
        auto fn = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hEngine, "CreateInterface"));
        if (fn) {
            for (auto name : names) {
                int ret = 0;
                void* p = fn(name, &ret);
                if (p && ret == 0) return reinterpret_cast<uintptr_t>(p);
            }
        }
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────
//  Default Item Mapping Helper
// ─────────────────────────────────────────────────────────────
static const char* GetHeroNameByID(uint32_t heroID) {
    switch (heroID) {
        case 14:  return "Pudge";
        case 8:   return "Juggernaut";
        case 44:  return "Phantom Assassin";
        case 1:   return "Anti-Mage";
        case 11:  return "Shadow Fiend";
        case 109: return "Terrorblade";
        case 5:   return "Crystal Maiden";
        case 21:  return "Windranger";
        case 39:  return "Queen of Pain";
        case 67:  return "Spectre";
        case 74:  return "Invoker";
        case 41:  return "Faceless Void";
        case 7:   return "Earthshaker";
        case 2:   return "Axe";
        case 49:  return "Dragon Knight";
        case 25:  return "Lina";
        case 89:  return "Monkey King";
        case 84:  return "Ogre Magi";
        case 18:  return "Sven";
        case 42:  return "Wraith King";
        default:  return nullptr;
    }
}

static void RegisterDefaultItemMappings(uint32_t heroID, int slot, uint32_t targetDefIndex, uint32_t style, float wear) {
    auto& sc = SkinChanger::GetInstance();
    if (targetDefIndex == 0) return;

    const char* hName = GetHeroNameByID(heroID);
    if (hName != nullptr) {
        int arcCount = 0, immCount = 0;
        const ItemEntry* arcanas = GetArcanasDB(arcCount);
        const ItemEntry* immortals = GetImmortalsDB(immCount);

        for (int i = 0; i < arcCount; ++i) {
            if (arcanas[i].defIndex != 0 && strcmp(arcanas[i].heroName, hName) == 0) {
                if (slot < 0 || arcanas[i].slot == slot || arcanas[i].slot == -1) {
                    sc.AddItemOverride(arcanas[i].defIndex, targetDefIndex, style, wear);
                }
            }
        }
        for (int i = 0; i < immCount; ++i) {
            if (immortals[i].defIndex != 0 && strcmp(immortals[i].heroName, hName) == 0) {
                if (slot < 0 || immortals[i].slot == slot || immortals[i].slot == -1) {
                    sc.AddItemOverride(immortals[i].defIndex, targetDefIndex, style, wear);
                }
            }
        }
    }

    if (heroID == 14 && slot == 0) { // Pudge Weapon / Hook
        static const uint32_t pudgeHookDefs[] = {
            4001, 125, 4007, 7061, 4052, 4268, 7356, 7357, 9662, 15887,
            4312, 4568, 4795, 4933, 5252, 6996, 7636, 7637, 7922, 8565
        };
        for (uint32_t orig : pudgeHookDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 8 && slot == 0) { // Juggernaut Weapon
        static const uint32_t juggDefs[] = { 4035, 69, 10006, 7035, 12083, 4100, 4178, 4202 };
        for (uint32_t orig : juggDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 44 && slot == 0) { // PA Weapon
        static const uint32_t paDefs[] = { 4216, 141, 9355, 15877, 4390, 4520 };
        for (uint32_t orig : paDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 1 && slot == 0) { // Anti-Mage Weapon
        static const uint32_t amDefs[] = { 4008, 1, 9258, 7030, 15890, 4200 };
        for (uint32_t orig : amDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 5 && slot == 1) { // Crystal Maiden Head
        static const uint32_t cmDefs[] = { 4026, 10037, 4150, 4220 };
        for (uint32_t orig : cmDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 11 && slot == 0) { // Shadow Fiend Arms
        static const uint32_t sfDefs[] = { 4061, 10045, 15905, 4300 };
        for (uint32_t orig : sfDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 109 && slot == 1) { // Terrorblade Head
        static const uint32_t tbDefs[] = { 4535, 10023, 7060, 15864 };
        for (uint32_t orig : tbDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 21 && slot == 0) { // Windranger Weapon
        static const uint32_t wrDefs[] = { 4261, 16730, 8014, 15897 };
        for (uint32_t orig : wrDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 39 && slot == 0) { // Queen of Pain Weapon
        static const uint32_t qopDefs[] = { 4211, 16770, 15947 };
        for (uint32_t orig : qopDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
    else if (heroID == 67 && slot == 0) { // Spectre Weapon
        static const uint32_t specDefs[] = { 4241, 16751, 4400 };
        for (uint32_t orig : specDefs) sc.AddItemOverride(orig, targetDefIndex, style, wear);
    }
}

// ─────────────────────────────────────────────────────────────
//  Singleton
// ─────────────────────────────────────────────────────────────
SkinChanger& SkinChanger::GetInstance() {
    static SkinChanger inst;
    return inst;
}

void SkinChanger::Initialize() {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_enabled       = true;
    m_needsRescan   = true;
    m_pEntitySystem = 0;
    m_esLayout      = ESLayout{};
    m_initAttempts  = 0;
    m_patchCount    = 0;
    m_heroInventories.clear();
    m_itemOverrides.clear();
    m_detectedWearables.clear();
    {
        std::lock_guard<std::mutex> dl(m_debugMutex);
        m_debugLogs.clear();
        m_debugStats = {};
    }
    EconHook::Install();
    OffsetScanner::GetInstance().Initialize();

    // Auto-enable Overplus Full Sets for ALL heroes by default on launch
    int setCnt = 0;
    const FullSetEntry* allSets = GetFullSetsDB(setCnt);
    for (int i = 0; i < setCnt; ++i) {
        for (int k = 0; k < allSets[i].itemCount; ++k) {
            const auto& setItem = allSets[i].items[k];
            AddItemOverride(allSets[i].heroID, setItem.slot, setItem.defIndex, setItem.style, 0.001f, true);
        }
    }

    LogDebug("INFO", "[InvChanger] Automatic Skin Unlocker Active! Default cosmetics auto-applied.");
}

// ─────────────────────────────────────────────────────────────
//  EntitySystem layout probing
// ─────────────────────────────────────────────────────────────
bool SkinChanger::ProbeLayout(uintptr_t es, ESLayout& outLayout) {
    if (!SafeMemoryOps::IsValid(es)) return false;

    static const uintptr_t offsetCandidates[] = {
        0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60
    };
    static const uintptr_t strideCandidates[] = { 0x70, 0x78, 0x80 };
    static const uintptr_t ptrOffCandidates[] = { 0x00, 0x08 };

    for (uintptr_t off : offsetCandidates) {
        for (bool indirect : { false, true }) {
            uintptr_t chunkArray = 0;
            if (indirect) {
                if (!SafeMemoryOps::Read64(es + off, chunkArray) || !SafeMemoryOps::IsValid(chunkArray)) continue;
            } else {
                chunkArray = es + off;
            }

            uintptr_t chunk0 = 0;
            if (!SafeMemoryOps::Read64(chunkArray, chunk0) || !SafeMemoryOps::IsValid(chunk0)) continue;

            for (uintptr_t stride : strideCandidates) {
                for (uintptr_t ptrOff : ptrOffCandidates) {
                    int validCount = 0;
                    for (int slot = 0; slot < 16; ++slot) {
                        uintptr_t identAddr = chunk0 + (uintptr_t)slot * stride;
                        if (!SafeMemoryOps::IsValid(identAddr)) break;

                        uintptr_t entPtr = 0;
                        if (SafeMemoryOps::Read64(identAddr + ptrOff, entPtr) && SafeMemoryOps::IsValid(entPtr)) {
                            uintptr_t vtbl = 0;
                            if (SafeMemoryOps::Read64(entPtr, vtbl) && IsValidVtable(vtbl)) {
                                uint32_t handleVal = 0;
                                if (SafeMemoryOps::Read32(identAddr + 0x08, handleVal) && ((handleVal & 0x7FFF) == (uint32_t)slot)) {
                                    validCount += 2;
                                } else {
                                    validCount += 1;
                                }
                            }
                        }
                    }

                    if (validCount >= 3) {
                        outLayout.listOffset = off;
                        outLayout.isIndirect = indirect;
                        outLayout.stride     = stride;
                        outLayout.ptrOff     = ptrOff;
                        outLayout.resolved   = true;
                        LogDebug("SUCCESS", "[InvChanger] Verified layout @ 0x%p: off=0x%X ind=%d stride=0x%X ptrOff=0x%X",
                                 (void*)es, (uint32_t)off, (int)indirect, (uint32_t)stride, (uint32_t)ptrOff);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool SkinChanger::TryResolveEntitySystem() {
    ++m_initAttempts;

    // 1. Try CreateInterface
    uintptr_t es = FindEntitySystemViaInterface();
    if (es && SafeMemoryOps::IsValid(es)) {
        if (ProbeLayout(es, m_esLayout)) {
            m_pEntitySystem = es;
            LogDebug("SUCCESS", "[InvChanger] EntitySystem @ 0x%p via interface", (void*)es);
            return true;
        }
    }

    // 2. Try pattern scanning
    struct Pat { const char* pat; int rel; int sz; };
    static const Pat candidates[] = {
        { "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 01",       3, 7 },
        { "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 08",       3, 7 }
    };

    for (auto& c : candidates) {
        uintptr_t addr = PatternScan::ResolveAddress("client.dll", c.pat, c.rel, c.sz);
        if (!addr) continue;

        uintptr_t candidateES = 0;
        if (SafeMemoryOps::Read64(addr, candidateES) && SafeMemoryOps::IsValid(candidateES)) {
            if (ProbeLayout(candidateES, m_esLayout)) {
                m_pEntitySystem = candidateES;
                LogDebug("SUCCESS", "[InvChanger] EntitySystem @ 0x%p via pattern", (void*)candidateES);
                return true;
            }
        }
        if (SafeMemoryOps::IsValid(addr)) {
            if (ProbeLayout(addr, m_esLayout)) {
                m_pEntitySystem = addr;
                LogDebug("SUCCESS", "[InvChanger] EntitySystem @ 0x%p via static pattern", (void*)addr);
                return true;
            }
        }
    }

    if (m_initAttempts % 10 == 1) {
        LogDebug("WARN", "[InvChanger] EntitySystem searching... (attempt %d)", m_initAttempts);
    }
    return false;
}

uintptr_t SkinChanger::ResolveHandle(uintptr_t chunkBase, uint32_t handleVal) {
    if (handleVal == 0xFFFFFFFF) return 0;
    const int entIdx      = (int)(handleVal & 0x7FFF);
    const int chunk       = entIdx / Offsets::kEntitiesPerChunk;
    const int slotInChunk = entIdx % Offsets::kEntitiesPerChunk;

    uintptr_t chunkPtr = 0;
    if (!SafeMemoryOps::Read64(chunkBase + (uintptr_t)chunk * 8, chunkPtr) || !SafeMemoryOps::IsValid(chunkPtr))
        return 0;

    uintptr_t identAddr = chunkPtr + (uintptr_t)slotInChunk * m_esLayout.stride;
    uintptr_t entityPtr = 0;
    if (SafeMemoryOps::IsValid(identAddr) && SafeMemoryOps::Read64(identAddr + m_esLayout.ptrOff, entityPtr) && SafeMemoryOps::IsValid(entityPtr))
        return entityPtr;

    for (uintptr_t altStride : { 0x78, 0x70, 0x80, 0x68 }) {
        identAddr = chunkPtr + (uintptr_t)slotInChunk * altStride;
        entityPtr = 0;
        if (SafeMemoryOps::IsValid(identAddr) && SafeMemoryOps::Read64(identAddr + m_esLayout.ptrOff, entityPtr) && SafeMemoryOps::IsValid(entityPtr))
            return entityPtr;
    }

    return 0;
}

bool SkinChanger::SafeWrite32(uintptr_t addr, uint32_t val) {
    if (!SafeMemoryOps::IsValid(addr)) return false;
    if (SafeMemoryOps::Write32(addr, val)) return true;
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(addr), 4, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = SafeMemoryOps::Write32(addr, val);
    VirtualProtect(reinterpret_cast<void*>(addr), 4, old, &old);
    return ok;
}

bool SkinChanger::SafeWriteFloat(uintptr_t addr, float val) {
    if (!SafeMemoryOps::IsValid(addr)) return false;
    return SafeMemoryOps::WriteFloat(addr, val);
}

bool SkinChanger::SafeWriteBool(uintptr_t addr, bool val) {
    if (!SafeMemoryOps::IsValid(addr)) return false;
    return SafeMemoryOps::WriteBool(addr, val);
}

static void TriggerNativeEngineReload(uintptr_t wearPtr) {
    (void)wearPtr;
    // Unsafe dirty flag write removed to prevent crashes on selection
}

void SkinChanger::ApplyPatches() {
    if (!m_pEntitySystem || !SafeMemoryOps::IsValid(m_pEntitySystem)) return;

    if (!m_esLayout.resolved) {
        if (!ProbeLayout(m_pEntitySystem, m_esLayout)) return;
    }

    uintptr_t chunkBase = 0;
    if (m_esLayout.isIndirect) {
        if (!SafeMemoryOps::Read64(m_pEntitySystem + m_esLayout.listOffset, chunkBase) || !SafeMemoryOps::IsValid(chunkBase)) {
            m_pEntitySystem = 0;
            m_esLayout.resolved = false;
            return;
        }
    } else {
        chunkBase = m_pEntitySystem + m_esLayout.listOffset;
    }

    uintptr_t chunk0Test = 0;
    if (!SafeMemoryOps::Read64(chunkBase, chunk0Test) || !SafeMemoryOps::IsValid(chunk0Test)) {
        m_esLayout.isIndirect = !m_esLayout.isIndirect;
        if (m_esLayout.isIndirect) {
            SafeMemoryOps::Read64(m_pEntitySystem + m_esLayout.listOffset, chunkBase);
        } else {
            chunkBase = m_pEntitySystem + m_esLayout.listOffset;
        }
        if (!SafeMemoryOps::Read64(chunkBase, chunk0Test) || !SafeMemoryOps::IsValid(chunk0Test)) {
            m_pEntitySystem = 0;
            m_esLayout.resolved = false;
            return;
        }
    }

    std::unordered_map<uint32_t, HeroInventory> invSnap;
    std::unordered_map<uint32_t, ItemOverrideRule> itemSnap;
    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        invSnap  = m_heroInventories;
        itemSnap = m_itemOverrides;
    }

    if (GetActiveOverridesCount() == 0) {
        std::lock_guard<std::mutex> dl(m_debugMutex);
        m_debugStats.tickCount++;
        return;
    }

    constexpr int NUM_CHUNKS = Offsets::kMaxEntities / Offsets::kEntitiesPerChunk;
    uint32_t scanned = 0, heroesFound = 0, wearablesPatched = 0;
    std::vector<DetectedWearable> detected;
    detected.reserve(64);

    const auto dynOff = OffsetScanner::GetInstance().GetOffsets();

    const uintptr_t wearOffCandidates[] = {
        dynOff.m_hMyWearables, 0xF08, 0xF10, 0xF00, 0xEF8, 0xF18, 0xF20, 0xEE8, 0xE80
    };
    const uintptr_t defOffCandidates[] = {
        (dynOff.m_iItemDefinitionIndex >= 0x1A0 && dynOff.m_iItemDefinitionIndex <= 0x220) ? dynOff.m_iItemDefinitionIndex : 0x1B8,
        0x1B8, 0x1B0, 0x1B4
    };
    const uintptr_t ownerOffCandidates[] = {
        dynOff.m_hOwnerEntity, 0x3C8, 0x3C0, 0x3CC, 0x3D0, 0x380, 0x310
    };
    const uintptr_t heroIDOffCandidates[] = {
        dynOff.m_nHeroID, 0x580, 0x584, 0x588, 0x578, 0x57C, 0x5AC, 0x5B8
    };

    auto PatchWearableAddress = [&](uintptr_t wearPtr, uint32_t targetDefIndex, uint32_t targetStyle, float targetWear) {
        (void)targetWear;
        if (!SafeMemoryOps::IsValid(wearPtr)) return false;

        uintptr_t targetOff = (dynOff.m_iItemDefinitionIndex >= 0x1A0 && dynOff.m_iItemDefinitionIndex <= 0x220)
                              ? dynOff.m_iItemDefinitionIndex
                              : 0x1B8;

        uint32_t curDef = 0;
        if (SafeMemoryOps::Read32(wearPtr + targetOff, curDef) && curDef == targetDefIndex) {
            return true; // Already patched
        }

        bool ok = false;
        if (SafeWrite32(wearPtr + targetOff, targetDefIndex)) {
            if (targetStyle > 0) {
                SafeWrite32(wearPtr + targetOff + 4, targetStyle);
            }
            // Write fake 64-bit Item ID at targetOff - 8 so Panorama UI recognizes item as OWNED / EQUIPPED ("ВЫБРАНО")
            const uint64_t fakeItemID = 20000000000ULL + (uint64_t)targetDefIndex;
            SafeMemoryOps::Write<uint64_t>(wearPtr + targetOff - 8, fakeItemID);
            ok = true;
        }
        return ok;
    };

    for (int c = 0; c < NUM_CHUNKS; ++c) {
        uintptr_t chunkPtr = 0;
        if (!SafeMemoryOps::Read64(chunkBase + (uintptr_t)c * 8, chunkPtr) || !SafeMemoryOps::IsValid(chunkPtr))
            continue;

        for (int s = 0; s < Offsets::kEntitiesPerChunk; ++s) {
            ++scanned;

            uintptr_t identAddr = chunkPtr + (uintptr_t)s * m_esLayout.stride;
            if (!SafeMemoryOps::IsValid(identAddr)) continue;

            uintptr_t entityPtr = 0;
            if (!SafeMemoryOps::Read64(identAddr + m_esLayout.ptrOff, entityPtr) || !SafeMemoryOps::IsValid(entityPtr)) {
                identAddr = chunkPtr + (uintptr_t)s * 0x78;
                if (!SafeMemoryOps::IsValid(identAddr) || !SafeMemoryOps::Read64(identAddr + m_esLayout.ptrOff, entityPtr) || !SafeMemoryOps::IsValid(entityPtr))
                    continue;
            }

            uintptr_t vtbl = 0;
            if (!SafeMemoryOps::Read64(entityPtr, vtbl) || !IsValidVtable(vtbl)) continue;

            // ── 1. Hero entity scan (In-Game & Main Menu Hero Preview) ───────
            uint32_t heroID = 0;
            for (uintptr_t hOff : heroIDOffCandidates) {
                uint32_t candidateID = 0;
                if (SafeMemoryOps::Read32(entityPtr + hOff, candidateID) && candidateID > 0 && candidateID < 250) {
                    heroID = candidateID;
                    break;
                }
            }

            if (heroID > 0) {
                OffsetScanner::GetInstance().ProbeHeroEntity(entityPtr, chunkBase, (int)m_esLayout.stride, (int)m_esLayout.ptrOff);

                auto it = invSnap.find(heroID);
                if (it != invSnap.end() && it->second.enabled) {
                    const HeroInventory& inv = it->second;
                    ++heroesFound;

                    for (uintptr_t wearOff : wearOffCandidates) {
                        uintptr_t wearVec  = entityPtr + wearOff;
                        uintptr_t wearData = 0;
                        uint32_t  wearCount = 0;
                        if (!SafeMemoryOps::Read64(wearVec + Offsets::kUtlVecDataOff, wearData) ||
                            !SafeMemoryOps::Read32(wearVec + (uintptr_t)Offsets::kUtlVecSizeOff, wearCount) ||
                            wearCount == 0 || wearCount > 32 || !SafeMemoryOps::IsValid(wearData))
                            continue;

                        for (uint32_t wi = 0; wi < wearCount && wi < (uint32_t)INV_MAX_SLOTS; ++wi) {
                            uint32_t handleVal = 0xFFFFFFFF;
                            if (!SafeMemoryOps::Read32(wearData + wi * 4, handleVal) || handleVal == 0xFFFFFFFF) continue;

                            uintptr_t wearPtr = ResolveHandle(chunkBase, handleVal);
                            if (!SafeMemoryOps::IsValid(wearPtr)) continue;

                            OffsetScanner::GetInstance().ProbeWearableEntity(wearPtr);

                            uint32_t origDef = 0;
                            for (uintptr_t defOff : defOffCandidates) {
                                uint32_t curDef = 0;
                                if (SafeMemoryOps::Read32(wearPtr + defOff, curDef) && curDef > 0 && curDef < 100000u) {
                                    origDef = curDef;
                                    break;
                                }
                            }
                            if (origDef == 0) continue;

                            DetectedWearable dw;
                            dw.addr             = wearPtr;
                            dw.heroID           = heroID;
                            dw.slotIndex        = (int)wi;
                            dw.originalDefIndex = origDef;
                            dw.patchedDefIndex  = 0;

                            bool patched = false;

                            // 1. Direct item override rule (itemSnap)
                            auto ruleIt = itemSnap.find(origDef);
                            if (ruleIt != itemSnap.end() && ruleIt->second.enabled && ruleIt->second.overrideDefIndex != 0) {
                                if (PatchWearableAddress(wearPtr, ruleIt->second.overrideDefIndex, ruleIt->second.style, ruleIt->second.wear)) {
                                    ++wearablesPatched;
                                    dw.patchedDefIndex = ruleIt->second.overrideDefIndex;
                                    patched = true;
                                }
                            }

                            // 2. Per-hero slot override (invSnap)
                            if (!patched) {
                                int targetSlot = GetItemSlotFromDB(origDef);
                                if (targetSlot >= 0 && targetSlot < INV_MAX_SLOTS) {
                                    const SlotOverride& ovr = inv.slots[targetSlot];
                                    if (ovr.enabled && ovr.defIndex != 0) {
                                        if (PatchWearableAddress(wearPtr, ovr.defIndex, ovr.style, ovr.wear)) {
                                            ++wearablesPatched;
                                            dw.patchedDefIndex = ovr.defIndex;
                                            patched = true;
                                        }
                                    }
                                }
                            }

                            // 3. Unmapped item fallback: apply active hero slot override
                            if (!patched) {
                                for (int s = 0; s < INV_MAX_SLOTS; ++s) {
                                    const SlotOverride& ovr = inv.slots[s];
                                    if (ovr.enabled && ovr.defIndex != 0) {
                                        int reqSlot = GetItemSlotFromDB(ovr.defIndex);
                                        if (reqSlot == s || reqSlot == (int)wi || s == (int)wi || reqSlot == -1) {
                                            if (PatchWearableAddress(wearPtr, ovr.defIndex, ovr.style, ovr.wear)) {
                                                ++wearablesPatched;
                                                dw.patchedDefIndex = ovr.defIndex;
                                                patched = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            detected.push_back(dw);
                        }
                        break;
                    }
                }
            }

            // ── 2. Standalone Wearable Entity Scan (Armory Preview Models) ────────
            uintptr_t wearVtbl = 0;
            if (SafeMemoryOps::Read64(entityPtr, wearVtbl) && IsValidVtable(wearVtbl)) {
                uint32_t candidateDef = 0;
                uintptr_t targetOff = (dynOff.m_iItemDefinitionIndex >= 0x1A0 && dynOff.m_iItemDefinitionIndex <= 0x220)
                                      ? dynOff.m_iItemDefinitionIndex
                                      : 0x1B8;

                if (SafeMemoryOps::Read32(entityPtr + targetOff, candidateDef) && candidateDef > 0 && candidateDef < 100000u) {
                    int dbSlot = GetItemSlotFromDB(candidateDef);
                    if (dbSlot >= 0 || candidateDef == 4001 || candidateDef == 125 || candidateDef == 4007 || candidateDef == 7061) {
                        uint32_t ownerHeroID = 0;
                        uintptr_t validOwnerPtr = 0;

                        for (uintptr_t ownerOff : ownerOffCandidates) {
                            uint32_t hOwner = 0xFFFFFFFF;
                            if (SafeMemoryOps::Read32(entityPtr + ownerOff, hOwner) && hOwner != 0xFFFFFFFF && hOwner != 0) {
                                uintptr_t ownerPtr = ResolveHandle(chunkBase, hOwner);
                                if (SafeMemoryOps::IsValid(ownerPtr)) {
                                    uintptr_t ownerVtbl = 0;
                                    if (SafeMemoryOps::Read64(ownerPtr, ownerVtbl) && IsValidVtable(ownerVtbl)) {
                                        validOwnerPtr = ownerPtr;
                                        for (uintptr_t hOff : heroIDOffCandidates) {
                                            uint32_t cID = 0;
                                            if (SafeMemoryOps::Read32(ownerPtr + hOff, cID) && cID > 0 && cID < 250) {
                                                ownerHeroID = cID;
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                        if (validOwnerPtr != 0) {
                            bool patched = false;
                            if (ownerHeroID > 0) {
                                auto it = invSnap.find(ownerHeroID);
                                if (it != invSnap.end() && it->second.enabled) {
                                    int slot = dbSlot >= 0 ? dbSlot : 0;
                                    const SlotOverride& ovr = it->second.slots[slot];
                                    if (ovr.enabled && ovr.defIndex != 0) {
                                        if (PatchWearableAddress(entityPtr, ovr.defIndex, ovr.style, ovr.wear)) {
                                            ++wearablesPatched;
                                            patched = true;
                                        }
                                    }
                                }
                            }

                            if (!patched) {
                                auto ruleIt = itemSnap.find(candidateDef);
                                if (ruleIt != itemSnap.end() && ruleIt->second.enabled && ruleIt->second.overrideDefIndex != 0) {
                                    if (PatchWearableAddress(entityPtr, ruleIt->second.overrideDefIndex, ruleIt->second.style, ruleIt->second.wear)) {
                                        ++wearablesPatched;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> dl(m_debugMutex);
        m_debugStats.totalEntitiesScanned = scanned;
        m_debugStats.econItemsFound       = heroesFound;
        m_debugStats.itemsPatched         = wearablesPatched;
        m_debugStats.activeOverridesCount = GetActiveOverridesCount();
        m_debugStats.tickCount++;
        if (wearablesPatched > 0) m_debugStats.safeExecuteSuccesses++;
    }
    m_patchCount = (int)wearablesPatched;

    {
        std::lock_guard<std::recursive_mutex> lk(m_mutex);
        m_detectedWearables = std::move(detected);
    }
}

void SkinChanger::Tick() {
    if (!m_enabled) return;

    static std::atomic<bool> inTick{false};
    if (inTick.exchange(true)) return; // Prevents concurrent multi-thread execution

    struct TickGuard {
        std::atomic<bool>& flag;
        ~TickGuard() { flag.store(false); }
    } guard{ inTick };

    if (!m_pEntitySystem || !SafeMemoryOps::IsValid(m_pEntitySystem)) {
        m_pEntitySystem = 0;
        using clk = std::chrono::steady_clock;
        static auto lastTry = clk::now();
        auto now = clk::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTry).count() >= 2000) {
            lastTry = now;
            MemoryGuard::SafeExecute([this]() {
                TryResolveEntitySystem();
            });
        }
        return;
    }

    using clk = std::chrono::steady_clock;
    static auto lastPatch = clk::now();
    auto now = clk::now();
    bool doForce = m_needsRescan;
    if (doForce || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPatch).count() >= 100) {
        lastPatch     = now;
        m_needsRescan = false;
        MemoryGuard::SafeExecute([this]() {
            ApplyPatches();
        });
    }
}

void SkinChanger::SetEnabled(bool enabled) {
    m_enabled = enabled;
    if (enabled) {
        m_needsRescan = true;
    }
    SystemUtils::Log("[InvChanger] State: %s", enabled ? "ON" : "OFF");
}

bool SkinChanger::IsEnabled()           const { return m_enabled; }
bool SkinChanger::IsEntitySystemReady() const { return m_pEntitySystem != 0 && SafeMemoryOps::IsValid(m_pEntitySystem); }
int  SkinChanger::GetPatchCount()       const { return m_patchCount; }
void SkinChanger::ForceRescan()               { m_needsRescan = true; }

void SkinChanger::AddItemOverride(uint32_t heroID, int slot, uint32_t defIndex, uint32_t style, float wear, bool enabled) {
    SetSlotOverride(heroID, slot, defIndex, style, wear, enabled);
    if (enabled && defIndex != 0) {
        EconHook::InjectFakeItem(heroID, (uint32_t)(slot >= 0 ? slot : 0), defIndex, style);
        RegisterDefaultItemMappings(heroID, slot, defIndex, style, wear);
    }
}

void SkinChanger::AddItemOverride(uint32_t originalDefIndex, uint32_t overrideDefIndex, uint32_t style, float wear) {
    if (originalDefIndex == 0) return;
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    if (overrideDefIndex == 0) {
        m_itemOverrides.erase(originalDefIndex);
    } else {
        ItemOverrideRule rule;
        rule.originalDefIndex = originalDefIndex;
        rule.overrideDefIndex = overrideDefIndex;
        rule.style = style;
        rule.wear = wear;
        rule.enabled = true;
        m_itemOverrides[originalDefIndex] = rule;
    }
    m_needsRescan = true;
    m_enabled = true;
    LogDebug("INFO", "[InvChanger] Item override #%u -> #%u registered.", originalDefIndex, overrideDefIndex);
}

void SkinChanger::RemoveItemOverride(uint32_t heroID, int slot) {
    ClearSlotOverride(heroID, slot);
}

void SkinChanger::RemoveItemOverride(uint32_t originalDefIndex) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_itemOverrides.erase(originalDefIndex);
    m_needsRescan = true;
}

bool SkinChanger::FindItemOverride(uint32_t origDefIndex, ItemOverrideRule& outRule) const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_itemOverrides.find(origDefIndex);
    if (it != m_itemOverrides.end() && it->second.enabled) {
        outRule = it->second;
        return true;
    }
    return false;
}

int SkinChanger::GetActiveOverridesCount() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    int count = 0;
    for (const auto& [hid, inv] : m_heroInventories) {
        if (!inv.enabled) continue;
        for (int i = 0; i < INV_MAX_SLOTS; ++i) {
            if (inv.slots[i].enabled && inv.slots[i].defIndex != 0) {
                count++;
            }
        }
    }
    for (const auto& [origDef, rule] : m_itemOverrides) {
        if (rule.enabled && rule.overrideDefIndex != 0) {
            count++;
        }
    }
    return count;
}

HeroInventory SkinChanger::GetHeroInventoryCopy(uint32_t heroID) const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_heroInventories.find(heroID);
    if (it != m_heroInventories.end()) return it->second;
    return HeroInventory{};
}

bool SkinChanger::HeroHasOverrides(uint32_t heroID) const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_heroInventories.find(heroID);
    if (it == m_heroInventories.end()) return false;
    if (!it->second.enabled) return false;
    for (int i = 0; i < INV_MAX_SLOTS; ++i)
        if (it->second.slots[i].enabled && it->second.slots[i].defIndex != 0) return true;
    return false;
}

void SkinChanger::SetHeroEnabled(uint32_t heroID, bool enabled) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_heroInventories[heroID].enabled = enabled;
    m_needsRescan = true;
}

void SkinChanger::SetSlotOverride(uint32_t heroID, int slot,
                                  uint32_t defIndex, uint32_t style, float wear, bool enabled) {
    if (slot < 0 || slot >= INV_MAX_SLOTS) return;
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto& inv = m_heroInventories[heroID];
    inv.slots[slot].defIndex = defIndex;
    inv.slots[slot].style    = style;
    inv.slots[slot].wear     = wear;
    inv.slots[slot].enabled  = enabled && (defIndex != 0);
    inv.enabled = true;
    m_needsRescan = true;
    m_enabled = true;
}

void SkinChanger::ClearSlotOverride(uint32_t heroID, int slot) {
    if (slot < 0 || slot >= INV_MAX_SLOTS) return;
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_heroInventories.find(heroID);
    if (it != m_heroInventories.end()) {
        it->second.slots[slot] = SlotOverride{};
        m_needsRescan = true;
    }
}

void SkinChanger::ClearHeroOverrides(uint32_t heroID) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_heroInventories.erase(heroID);
    m_needsRescan = true;
}

void SkinChanger::ClearAllOverrides() {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_heroInventories.clear();
    m_itemOverrides.clear();
    m_needsRescan = true;
}

std::vector<DetectedWearable> SkinChanger::GetDetectedWearables() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_detectedWearables;
}

void SkinChanger::LogDebug(const char* level, const char* fmt, ...) {
    char buf[512];
    va_list args; va_start(args, fmt); vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);

    auto now  = std::chrono::system_clock::now();
    auto tmt  = std::chrono::system_clock::to_time_t(now);
    struct tm tmb; localtime_s(&tmb, &tmt);
    char ts[16]; snprintf(ts, sizeof(ts), "%02d:%02d:%02d", tmb.tm_hour, tmb.tm_min, tmb.tm_sec);

    ImVec4 col = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    if      (!strcmp(level, "SUCCESS")) col = ImVec4(0.29f, 0.87f, 0.50f, 1.0f);
    else if (!strcmp(level, "ERROR"))   col = ImVec4(0.97f, 0.44f, 0.44f, 1.0f);
    else if (!strcmp(level, "WARN"))    col = ImVec4(0.98f, 0.75f, 0.14f, 1.0f);
    else if (!strcmp(level, "INFO"))    col = ImVec4(0.22f, 0.74f, 0.97f, 1.0f);

    {
        std::lock_guard<std::mutex> dl(m_debugMutex);
        m_debugLogs.push_back({ ts, buf, col });
        if (m_debugLogs.size() > 200) m_debugLogs.pop_front();
    }
    SystemUtils::Log("%s", buf);
}

void SkinChanger::ClearDebugLogs() {
    std::lock_guard<std::mutex> dl(m_debugMutex);
    m_debugLogs.clear();
}

std::vector<SkinDebugLogEntry> SkinChanger::GetDebugLogs() const {
    std::lock_guard<std::mutex> dl(m_debugMutex);
    return { m_debugLogs.begin(), m_debugLogs.end() };
}

SkinDebugStats SkinChanger::GetDebugStats() const {
    std::lock_guard<std::mutex> dl(m_debugMutex);
    SkinDebugStats s = m_debugStats;
    s.entitySystemAddr    = m_pEntitySystem;
    s.isEntitySystemReady = IsEntitySystemReady();
    s.activeOverridesCount = GetActiveOverridesCount();
    return s;
}
