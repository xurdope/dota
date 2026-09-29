// ============================================================
//  InventoryChanger — Per-hero, per-slot item override
//  Overplus-style auto-unlock with crash-safe patching
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
#include <chrono>
#include <atomic>
#include <cstdarg>
#include <cstring>

// ─────────────────────────────────────────────────────────────
//  Helper: VTable validation
// ─────────────────────────────────────────────────────────────
static bool IsValidVtable(uintptr_t vtbl) {
    if (!SafeMemoryOps::IsValid(vtbl)) return false;
    uintptr_t fn0 = 0;
    if (!SafeMemoryOps::Read64(vtbl, fn0) || !SafeMemoryOps::IsValid(fn0)) return false;
    return true;
}

// ─────────────────────────────────────────────────────────────
//  Helper: Find EntitySystem via CreateInterface
// ─────────────────────────────────────────────────────────────
using CreateInterfaceFn = void* (*)(const char*, int*);

static uintptr_t FindEntitySystemViaInterface() {
    static const char* names[] = {
        "Source2GameEntities001", "GameEntitySystem_001",
        "Source2GameEntities_001", "EntitySystem_001"
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
//  Singleton
// ─────────────────────────────────────────────────────────────
SkinChanger& SkinChanger::GetInstance() {
    static SkinChanger inst;
    return inst;
}

// ─────────────────────────────────────────────────────────────
//  Initialize — Overplus-style auto-unlock
// ─────────────────────────────────────────────────────────────
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

    // Auto-enable Full Sets for ALL heroes
    int setCnt = 0;
    const FullSetEntry* allSets = GetFullSetsDB(setCnt);
    for (int i = 0; i < setCnt; ++i) {
        for (int k = 0; k < allSets[i].itemCount; ++k) {
            const auto& setItem = allSets[i].items[k];
            AddItemOverride(allSets[i].heroID, setItem.slot, setItem.defIndex, setItem.style, 0.001f, true);
        }
    }

    LogDebug("INFO", "[InvChanger] Overplus-style Automatic Skin Unlocker Active!");
    LogDebug("INFO", "[InvChanger] Loaded %d full sets auto-applied.", setCnt);
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
                                if (SafeMemoryOps::Read32(identAddr + Offsets::m_EntityIdentityHandle, handleVal) &&
                                    ((handleVal & 0x7FFF) == (uint32_t)slot)) {
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

    uintptr_t es = FindEntitySystemViaInterface();
    if (es && SafeMemoryOps::IsValid(es)) {
        if (ProbeLayout(es, m_esLayout)) {
            m_pEntitySystem = es;
            LogDebug("SUCCESS", "[InvChanger] EntitySystem @ 0x%p via interface", (void*)es);
            return true;
        }
    }

    struct Pat { const char* pat; int rel; int sz; };
    static const Pat candidates[] = {
        { "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 01", 3, 7 },
        { "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 08", 3, 7 }
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

// ─────────────────────────────────────────────────────────────
//  ApplyPatches — Main patching logic (CRASH-SAFE)
// ─────────────────────────────────────────────────────────────
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
        dynOff.m_hMyWearables, 0xF08, 0xF10, 0xF00, 0xEF8, 0xF18, 0xF20, 0xEE8, 0xE50
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

    // ── CRASH-SAFE patch lambda ──
    auto PatchWearableAddress = [&](uintptr_t wearPtr, uint32_t targetDefIndex, uint32_t targetStyle, float targetWear) -> bool {
        (void)targetWear;
        if (!SafeMemoryOps::IsValid(wearPtr)) return false;

        uintptr_t targetOff = (dynOff.m_iItemDefinitionIndex >= 0x1A0 && dynOff.m_iItemDefinitionIndex <= 0x220)
                              ? dynOff.m_iItemDefinitionIndex
                              : 0x1B8;

        uintptr_t writeAddr = wearPtr + targetOff;
        if (!SafeMemoryOps::IsValid(writeAddr) || !SafeMemoryOps::IsValid(writeAddr + 4)) return false;

        uint32_t curDef = 0;
        if (SafeMemoryOps::Read32(writeAddr, curDef) && curDef == targetDefIndex) {
            return true; // Already patched
        }

        bool ok = false;
        if (SafeWrite32(writeAddr, targetDefIndex)) {
            if (targetStyle > 0) {
                SafeWrite32(writeAddr + 4, targetStyle);
            }
            // Only write fake ID if address is valid
            uintptr_t fakeIdAddr = writeAddr - 8;
            if (SafeMemoryOps::IsValid(fakeIdAddr) && SafeMemoryOps::IsValid(fakeIdAddr + 8)) {
                const uint64_t fakeItemID = 20000000000ULL + (uint64_t)targetDefIndex;
                SafeMemoryOps::Write<uint64_t>(fakeIdAddr, fakeItemID);
            }
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

            // ── 1. Hero entity scan ───────
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

                        if (!SafeMemoryOps::IsValid(wearVec) || !SafeMemoryOps::IsValid(wearVec + 8))
                            continue;

                        if (!SafeMemoryOps::Read64(wearVec + Offsets::kUtlVecDataOff, wearData) ||
                            !SafeMemoryOps::Read32(wearVec + Offsets::kUtlVecSizeOff, wearCount) ||
                            wearCount == 0 || wearCount > 32 || !SafeMemoryOps::IsValid(wearData))
                            continue;

                        for (uint32_t wi = 0; wi < wearCount && wi < (uint32_t)INV_MAX_SLOTS; ++wi) {
                            uintptr_t handleAddr = wearData + wi * 4;
                            if (!SafeMemoryOps::IsValid(handleAddr)) continue;

                            uint32_t handleVal = 0xFFFFFFFF;
                            if (!SafeMemoryOps::Read32(handleAddr, handleVal) || handleVal == 0xFFFFFFFF) continue;

                            uintptr_t wearPtr = ResolveHandle(chunkBase, handleVal);
                            if (!wearPtr || !SafeMemoryOps::IsValid(wearPtr)) continue;

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

                            // 1. Direct item override rule
                            auto ruleIt = itemSnap.find(origDef);
                            if (ruleIt != itemSnap.end() && ruleIt->second.enabled && ruleIt->second.overrideDefIndex != 0) {
                                if (PatchWearableAddress(wearPtr, ruleIt->second.overrideDefIndex, ruleIt->second.style, ruleIt->second.wear)) {
                                    ++wearablesPatched;
                                    dw.patchedDefIndex = ruleIt->second.overrideDefIndex;
                                    patched = true;
                                }
                            }

                            // 2. Per-hero slot override
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

                            // 3. Unmapped item fallback
                            if (!patched) {
                                for (int s2 = 0; s2 < INV_MAX_SLOTS; ++s2) {
                                    const SlotOverride& ovr = inv.slots[s2];
                                    if (ovr.enabled && ovr.defIndex != 0) {
                                        int reqSlot = GetItemSlotFromDB(ovr.defIndex);
                                        if (reqSlot == s2 || reqSlot == (int)wi || s2 == (int)wi || reqSlot == -1) {
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

            // ── 2. Standalone Wearable Entity Scan ────────
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
                                auto it2 = invSnap.find(ownerHeroID);
                                if (it2 != invSnap.end() && it2->second.enabled) {
                                    int slot = dbSlot >= 0 ? dbSlot : 0;
                                    const SlotOverride& ovr = it2->second.slots[slot];
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

// ─────────────────────────────────────────────────────────────
//  Tick — CRASH-SAFE with lobby delay
// ─────────────────────────────────────────────────────────────
void SkinChanger::Tick() {
    if (!m_enabled) return;

    static std::atomic<bool> inTick{false};
    if (inTick.exchange(true)) return;

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

    // ── CRASH FIX: Don't patch during first 5 seconds (lobby loading) ──
    using clk = std::chrono::steady_clock;
    static auto firstTickTime = clk::now();
    static bool initialized = false;
    auto now = clk::now();

    if (!initialized) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - firstTickTime).count() >= 5) {
            initialized = true;
        } else {
            return; // Skip patching for first 5 seconds
        }
    }

    static auto lastPatch = clk::now();
    bool doForce = m_needsRescan;
    if (doForce || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPatch).count() >= 250) {
        lastPatch     = now;
        m_needsRescan = false;
        MemoryGuard::SafeExecute([this]() {
            ApplyPatches();
        });
    }
}

// ─────────────────────────────────────────────────────────────
//  Master toggle
// ─────────────────────────────────────────────────────────────
void SkinChanger::SetEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_enabled = enabled;
    LogDebug("INFO", "[InvChanger] State: %s", enabled ? "ON" : "OFF");
}

bool SkinChanger::IsEnabled() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_enabled;
}

// ─────────────────────────────────────────────────────────────
//  Override Rule Registration
// ─────────────────────────────────────────────────────────────
void SkinChanger::AddItemOverride(uint32_t heroID, int slot, uint32_t defIndex, uint32_t style, float wear, bool enabled) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto& inv = m_heroInventories[heroID];
    inv.enabled = true;
    if (slot >= 0 && slot < INV_MAX_SLOTS) {
        inv.slots[slot].enabled  = enabled;
        inv.slots[slot].defIndex = defIndex;
        inv.slots[slot].style    = style;
        inv.slots[slot].wear     = wear;
    }
}

void SkinChanger::AddItemOverride(uint32_t originalDefIndex, uint32_t overrideDefIndex, uint32_t style, float wear) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    ItemOverrideRule rule;
    rule.originalDefIndex = originalDefIndex;
    rule.overrideDefIndex = overrideDefIndex;
    rule.style            = style;
    rule.wear             = wear;
    rule.enabled          = true;
    m_itemOverrides[originalDefIndex] = rule;
}

void SkinChanger::RemoveItemOverride(uint32_t heroID, int slot) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_heroInventories.find(heroID);
    if (it != m_heroInventories.end() && slot >= 0 && slot < INV_MAX_SLOTS) {
        it->second.slots[slot] = SlotOverride{};
    }
}

void SkinChanger::RemoveItemOverride(uint32_t originalDefIndex) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_itemOverrides.erase(originalDefIndex);
}

bool SkinChanger::FindItemOverride(uint32_t origDefIndex, ItemOverrideRule& outRule) const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_itemOverrides.find(origDefIndex);
    if (it != m_itemOverrides.end()) {
        outRule = it->second;
        return true;
    }
    return false;
}

int SkinChanger::GetActiveOverridesCount() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    int count = 0;
    for (auto& [heroID, inv] : m_heroInventories) {
        if (!inv.enabled) continue;
        for (int i = 0; i < INV_MAX_SLOTS; ++i) {
            if (inv.slots[i].enabled && inv.slots[i].defIndex != 0) count++;
        }
    }
    for (auto& [defIdx, rule] : m_itemOverrides) {
        if (rule.enabled && rule.overrideDefIndex != 0) count++;
    }
    return count;
}

int SkinChanger::GetPatchCount() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_patchCount;
}

bool SkinChanger::IsEntitySystemReady() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_pEntitySystem != 0 && m_esLayout.resolved;
}

// ─────────────────────────────────────────────────────────────
//  Per-hero inventory configuration
// ─────────────────────────────────────────────────────────────
HeroInventory SkinChanger::GetHeroInventoryCopy(uint32_t heroID) const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_heroInventories.find(heroID);
    if (it != m_heroInventories.end()) return it->second;
    return HeroInventory{};
}

bool SkinChanger::HeroHasOverrides(uint32_t heroID) const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    auto it = m_heroInventories.find(heroID);
    if (it == m_heroInventories.end() || !it->second.enabled) return false;
    for (int i = 0; i < INV_MAX_SLOTS; ++i) {
        if (it->second.slots[i].enabled && it->second.slots[i].defIndex != 0) return true;
    }
    return false;
}

void SkinChanger::SetHeroEnabled(uint32_t heroID, bool enabled) {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_heroInventories[heroID].enabled = enabled;
    m_needsRescan = true;
}

void SkinChanger::SetSlotOverride(uint32_t heroID, int slot, uint32_t defIndex, uint32_t style, float wear, bool enabled) {
    AddItemOverride(heroID, slot, defIndex, style, wear, enabled);
    m_needsRescan = true;
}

void SkinChanger::ClearSlotOverride(uint32_t heroID, int slot) {
    RemoveItemOverride(heroID, slot);
    m_needsRescan = true;
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

// ─────────────────────────────────────────────────────────────
//  UI helpers
// ─────────────────────────────────────────────────────────────
void SkinChanger::ForceRescan() {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    m_needsRescan = true;
}

std::vector<DetectedWearable> SkinChanger::GetDetectedWearables() const {
    std::lock_guard<std::recursive_mutex> lk(m_mutex);
    return m_detectedWearables;
}

// ─────────────────────────────────────────────────────────────
//  Debug logging
// ─────────────────────────────────────────────────────────────
void SkinChanger::LogDebug(const char* level, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> dl(m_debugMutex);

    SkinDebugLogEntry entry;
    entry.message = buf;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    entry.timestamp = ts;

    if (strcmp(level, "SUCCESS") == 0) {
        entry.color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    } else if (strcmp(level, "ERROR") == 0) {
        entry.color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    } else if (strcmp(level, "WARN") == 0) {
        entry.color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
    } else {
        entry.color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }

    m_debugLogs.push_back(entry);
    if (m_debugLogs.size() > 200) m_debugLogs.pop_front();

    SystemUtils::Log("%s", buf);
}

void SkinChanger::ClearDebugLogs() {
    std::lock_guard<std::mutex> dl(m_debugMutex);
    m_debugLogs.clear();
    m_debugStats = {};
}

std::vector<SkinDebugLogEntry> SkinChanger::GetDebugLogs() const {
    std::lock_guard<std::mutex> dl(m_debugMutex);
    return std::vector<SkinDebugLogEntry>(m_debugLogs.begin(), m_debugLogs.end());
}

SkinDebugStats SkinChanger::GetDebugStats() const {
    std::lock_guard<std::mutex> dl(m_debugMutex);
    return m_debugStats;
}

// ─────────────────────────────────────────────────────────────
//  Memory operations
// ─────────────────────────────────────────────────────────────
uintptr_t SkinChanger::ResolveHandle(uintptr_t chunkBase, uint32_t handleVal) {
    const int entIdx = (int)(handleVal & 0x7FFF);
    const int chunk  = entIdx / Offsets::kEntitiesPerChunk;
    const int slotInChunk = entIdx % Offsets::kEntitiesPerChunk;

    if (chunk < 0 || chunk >= (Offsets::kMaxEntities / Offsets::kEntitiesPerChunk)) return 0;

    uintptr_t chunkPtr = 0;
    if (!SafeMemoryOps::Read64(chunkBase + (uintptr_t)chunk * 8, chunkPtr) || !SafeMemoryOps::IsValid(chunkPtr))
        return 0;

    uintptr_t identAddr = chunkPtr + (uintptr_t)slotInChunk * m_esLayout.stride;
    uintptr_t entPtr = 0;
    if (!SafeMemoryOps::IsValid(identAddr) || !SafeMemoryOps::Read64(identAddr + m_esLayout.ptrOff, entPtr) || !SafeMemoryOps::IsValid(entPtr))
        return 0;

    return entPtr;
}

bool SkinChanger::SafeWrite32(uintptr_t addr, uint32_t val) {
    return SafeMemoryOps::Write<uint32_t>(addr, val);
}

bool SkinChanger::SafeWriteFloat(uintptr_t addr, float val) {
    return SafeMemoryOps::Write<float>(addr, val);
}

bool SkinChanger::SafeWriteBool(uintptr_t addr, bool val) {
    return SafeMemoryOps::Write<bool>(addr, val);
}