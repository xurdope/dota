#include "pattern_scan.h"
#include "memory_guard.h"
#include "../system/system_utils.h"
#include <windows.h>
#include <vector>
#include <sstream>
#include <cstring>

namespace PatternScan {

namespace {

    struct ParsedPattern {
        std::vector<uint8_t> bytes;
        std::vector<bool>    mask;   // true = сравнивать
    };

    ParsedPattern ParsePattern(const char* pattern) {
        ParsedPattern result;
        std::istringstream iss(pattern);
        std::string token;
        while (iss >> token) {
            if (token == "?" || token == "??") {
                result.bytes.push_back(0x00);
                result.mask.push_back(false);
            } else {
                try {
                    result.bytes.push_back(static_cast<uint8_t>(
                        std::stoul(token, nullptr, 16)));
                    result.mask.push_back(true);
                }
                catch (...) {
                    SystemUtils::Log("[PatternScan] Bad token in pattern: '%s'", token.c_str());
                    result.bytes.clear();
                    result.mask.clear();
                    return result;
                }
            }
        }
        return result;
    }

    // Сканирует один непрерывный регион памяти.
    uintptr_t ScanRegion(uintptr_t start, size_t length,
                         const std::vector<uint8_t>& bytes,
                         const std::vector<bool>& mask)
    {
        if (bytes.empty() || length < bytes.size()) return 0;

        const size_t patLen = bytes.size();
        const uint8_t* mem  = reinterpret_cast<const uint8_t*>(start);

        for (size_t i = 0; i + patLen <= length; ++i) {
            bool found = true;
            for (size_t j = 0; j < patLen; ++j) {
                if (mask[j] && mem[i + j] != bytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return start + i;
        }
        return 0;
    }

    // Сканирует диапазон модуля, обходя регионы через VirtualQuery.
    // Это критично — нельзя читать всю память подряд, можно наткнуться
    // на PAGE_NOACCESS и получить access violation.
    uintptr_t ScanRange(uintptr_t start, size_t length,
                        const std::vector<uint8_t>& bytes,
                        const std::vector<bool>& mask)
    {
        if (bytes.empty()) return 0;

        uintptr_t addr = start;
        uintptr_t end  = start + length;

        while (addr < end) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0)
                break;

            uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            uintptr_t regionEnd  = regionBase + mbi.RegionSize;
            if (regionEnd > end) regionEnd = end;

            bool readable = (mbi.State == MEM_COMMIT) &&
                            !(mbi.Protect & PAGE_GUARD) &&
                            !(mbi.Protect & PAGE_NOACCESS) &&
                            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE |
                                            PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));

            if (readable && regionEnd > addr) {
                // Ограничиваем скан началом валидного региона
                uintptr_t scanStart = (addr > regionBase) ? addr : regionBase;
                uintptr_t hit = ScanRegion(scanStart, regionEnd - scanStart, bytes, mask);
                if (hit) return hit;
            }

            addr = regionEnd;
            if (addr <= regionBase) break; // защита от бесконечного цикла
        }
        return 0;
    }

} // anon namespace

uintptr_t GetModuleBase(const char* moduleName) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    return hMod ? reinterpret_cast<uintptr_t>(hMod) : 0;
}

size_t GetModuleSize(const char* moduleName) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return 0;

    auto* dosHdr = reinterpret_cast<IMAGE_DOS_HEADER*>(hMod);
    if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    auto* ntHdr = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(hMod) + dosHdr->e_lfanew);
    if (ntHdr->Signature != IMAGE_NT_SIGNATURE) return 0;

    return ntHdr->OptionalHeader.SizeOfImage;
}

uintptr_t Find(const char* moduleName, const char* pattern) {
    uintptr_t base = GetModuleBase(moduleName);
    if (!base) {
        SystemUtils::Log("[PatternScan] Module '%s' not found.", moduleName);
        return 0;
    }

    size_t size = GetModuleSize(moduleName);
    if (!size) {
        SystemUtils::Log("[PatternScan] Failed to get size of '%s'.", moduleName);
        return 0;
    }

    ParsedPattern parsed = ParsePattern(pattern);
    if (parsed.bytes.empty()) {
        SystemUtils::Log("[PatternScan] Empty/invalid pattern.");
        return 0;
    }

    uintptr_t result = ScanRange(base, size, parsed.bytes, parsed.mask);
    if (!result) {
        SystemUtils::Log("[PatternScan] Pattern not found in '%s': %s", moduleName, pattern);
    }
    return result;
}

uintptr_t ResolveRelativePointer(const char* moduleName,
                                  const char* pattern,
                                  int relOffset,
                                  int instrSize)
{
    uintptr_t match = Find(moduleName, pattern);
    if (!match) return 0;

    // Проверяем, что можем прочитать rel (4 байта после match+relOffset)
    int32_t rel = 0;
    if (!MemoryGuard::SafeRead<int32_t>(match + relOffset, rel)) {
        SystemUtils::Log("[PatternScan] Cannot read rel at 0x%p", (void*)(match + relOffset));
        return 0;
    }

    uintptr_t absAddr = match + instrSize + rel;
    uintptr_t value = 0;
    if (!MemoryGuard::SafeRead<uintptr_t>(absAddr, value)) {
        SystemUtils::Log("[PatternScan] Cannot deref abs addr 0x%p", (void*)absAddr);
        return 0;
    }

    SystemUtils::Log("[PatternScan] Resolved ptr: hit=0x%p rel=0x%X abs=0x%p val=0x%p",
        (void*)match, (uint32_t)rel, (void*)absAddr, (void*)value);
    return value;
}

uintptr_t ResolveAddress(const char* moduleName,
                         const char* pattern,
                         int relOffset,
                         int instrSize)
{
    uintptr_t match = Find(moduleName, pattern);
    if (!match) return 0;

    int32_t rel = 0;
    if (!MemoryGuard::SafeRead<int32_t>(match + relOffset, rel)) {
        SystemUtils::Log("[PatternScan] Cannot read rel at 0x%p", (void*)(match + relOffset));
        return 0;
    }

    uintptr_t absAddr = match + instrSize + rel;
    SystemUtils::Log("[PatternScan] Resolved addr: hit=0x%p rel=0x%X addr=0x%p",
        (void*)match, (uint32_t)rel, (void*)absAddr);
    return absAddr;
}

} // namespace PatternScan