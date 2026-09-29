// ============================================================================
//  File: test_skin_sandbox/sandbox_manager.cpp
//  Description: Standalone C++ (MSVC x64) test utility for local data structures
//               and safe SEH (__try / __except) exception handling mechanisms.
// ============================================================================

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <iomanip>
#include <cstdint>
#include <cassert>

// ----------------------------------------------------------------------------
// Low-Level Safe Memory Operations (SEH wrappers isolated from C++ stack frames)
// ----------------------------------------------------------------------------
namespace SafeMemory {

    // Safely reads a 32-bit unsigned integer from a memory address.
    static bool SafeReadUInt32(const uint32_t* pAddress, uint32_t* pOutValue, DWORD* pOutErrorCode = nullptr) {
        __try {
            *pOutValue = *pAddress;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutErrorCode) {
                *pOutErrorCode = GetExceptionCode();
            }
            return false;
        }
    }

    // Safely writes a 32-bit unsigned integer to a memory address.
    static bool SafeWriteUInt32(uint32_t* pAddress, uint32_t value, DWORD* pOutErrorCode = nullptr) {
        __try {
            *pAddress = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutErrorCode) {
                *pOutErrorCode = GetExceptionCode();
            }
            return false;
        }
    }

    // Safely writes a float to a memory address.
    static bool SafeWriteFloat(float* pAddress, float value, DWORD* pOutErrorCode = nullptr) {
        __try {
            *pAddress = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutErrorCode) {
                *pOutErrorCode = GetExceptionCode();
            }
            return false;
        }
    }
}

// ----------------------------------------------------------------------------
// Data Structures
// ----------------------------------------------------------------------------
struct SkinDataRecord {
    uint32_t definitionIndex = 0;
    uint32_t paintKit        = 0;
    uint32_t seed            = 0;
    float    wear            = 0.001f;
    uint32_t statTrakCount   = 0;
    char     customName[32]  = { 0 };
};

enum class ValidationStatus {
    Valid,
    InvalidDefinition,
    InvalidWearRange,
    NullPointerAccess,
    ExceptionCaught
};

// ----------------------------------------------------------------------------
// Sandbox Manager Class
// ----------------------------------------------------------------------------
class SandboxManager {
private:
    std::unordered_map<uint32_t, SkinDataRecord> m_dataStore;

public:
    SandboxManager() = default;
    ~SandboxManager() = default;

    // Registers a validated skin configuration
    bool RegisterSkin(uint32_t itemID, const SkinDataRecord& record) {
        if (!ValidateRecord(record)) {
            return false;
        }
        m_dataStore[itemID] = record;
        return true;
    }

    // Validates logical bounds for skin record fields
    static bool ValidateRecord(const SkinDataRecord& record) {
        if (record.definitionIndex == 0) {
            return false;
        }
        if (record.wear < 0.0f || record.wear > 1.0f) {
            return false;
        }
        return true;
    }

    // Safely updates a field on a raw pointer target using Structured Exception Handling
    ValidationStatus UpdateSkinDefinitionSafely(uint32_t* pTargetDef, uint32_t newDef) {
        if (!pTargetDef) {
            return ValidationStatus::NullPointerAccess;
        }

        if (newDef == 0) {
            return ValidationStatus::InvalidDefinition;
        }

        DWORD exceptionCode = 0;
        if (!SafeMemory::SafeWriteUInt32(pTargetDef, newDef, &exceptionCode)) {
            std::cout << "  [SEH Guard] Intercepted illegal memory write! Exception Code: 0x" 
                      << std::hex << exceptionCode << std::dec << "\n";
            return ValidationStatus::ExceptionCaught;
        }

        return ValidationStatus::Valid;
    }

    // Safely updates skin wear float on a raw pointer target using Structured Exception Handling
    ValidationStatus UpdateSkinWearSafely(float* pTargetWear, float newWear) {
        if (!pTargetWear) {
            return ValidationStatus::NullPointerAccess;
        }

        if (newWear < 0.0f || newWear > 1.0f) {
            return ValidationStatus::InvalidWearRange;
        }

        DWORD exceptionCode = 0;
        if (!SafeMemory::SafeWriteFloat(pTargetWear, newWear, &exceptionCode)) {
            std::cout << "  [SEH Guard] Intercepted illegal memory write! Exception Code: 0x" 
                      << std::hex << exceptionCode << std::dec << "\n";
            return ValidationStatus::ExceptionCaught;
        }

        return ValidationStatus::Valid;
    }

    // Retrieves registered item count
    size_t GetRegisteredCount() const {
        return m_dataStore.size();
    }
};

// ----------------------------------------------------------------------------
// Unit Tests Suite
// ----------------------------------------------------------------------------
static void RunUnitTests() {
    std::cout << "========================================================\n";
    std::cout << "        Skin Sandbox Data & SEH Unit Tests\n";
    std::cout << "========================================================\n\n";

    SandboxManager manager;
    int passed = 0;
    int total = 0;

    // Test 1: Valid Registration
    {
        total++;
        std::cout << "[Test 1] Registering valid SkinDataRecord...\n";
        SkinDataRecord validItem{};
        validItem.definitionIndex = 508; // AK-47
        validItem.paintKit        = 44;  // Case Hardened
        validItem.seed            = 661;
        validItem.wear            = 0.05f;

        if (manager.RegisterSkin(1, validItem) && manager.GetRegisteredCount() == 1) {
            std::cout << "  -> PASSED\n";
            passed++;
        } else {
            std::cout << "  -> FAILED\n";
        }
    }

    // Test 2: Invalid Out-of-Bounds Wear Validation
    {
        total++;
        std::cout << "\n[Test 2] Validating out-of-bounds wear value...\n";
        SkinDataRecord invalidItem{};
        invalidItem.definitionIndex = 7;
        invalidItem.wear            = 1.5f; // Out of range [0.0, 1.0]

        if (!manager.RegisterSkin(2, invalidItem)) {
            std::cout << "  -> PASSED (Correctly rejected invalid wear)\n";
            passed++;
        } else {
            std::cout << "  -> FAILED\n";
        }
    }

    // Test 3: Safe Memory Update on Valid Target Pointer
    {
        total++;
        std::cout << "\n[Test 3] Safe SEH field update on valid memory target...\n";
        SkinDataRecord activeItem{};
        activeItem.definitionIndex = 9;

        ValidationStatus status = manager.UpdateSkinDefinitionSafely(&activeItem.definitionIndex, 508);
        if (status == ValidationStatus::Valid && activeItem.definitionIndex == 508) {
            std::cout << "  -> PASSED (Field updated to " << activeItem.definitionIndex << ")\n";
            passed++;
        } else {
            std::cout << "  -> FAILED\n";
        }
    }

    // Test 4: SEH Interception on Invalid Memory Address (Access Violation)
    {
        total++;
        std::cout << "\n[Test 4] SEH interception on bad memory target (0xDEADBEEF)...\n";
        uint32_t* pBadAddress = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(0xDEADBEEF));

        ValidationStatus status = manager.UpdateSkinDefinitionSafely(pBadAddress, 508);
        if (status == ValidationStatus::ExceptionCaught) {
            std::cout << "  -> PASSED (SEH caught Access Violation safely without crashing process)\n";
            passed++;
        } else {
            std::cout << "  -> FAILED\n";
        }
    }

    // Test 5: Safe Read from Null Pointer
    {
        total++;
        std::cout << "\n[Test 5] Safe SEH memory read from nullptr...\n";
        uint32_t val = 0;
        DWORD err = 0;
        bool result = SafeMemory::SafeReadUInt32(nullptr, &val, &err);
        if (!result && err == EXCEPTION_ACCESS_VIOLATION) {
            std::cout << "  -> PASSED (SafeReadUInt32 returned false, caught EXCEPTION_ACCESS_VIOLATION)\n";
            passed++;
        } else {
            std::cout << "  -> FAILED\n";
        }
    }

    std::cout << "\n--------------------------------------------------------\n";
    std::cout << " Summary: " << passed << " / " << total << " tests passed successfully.\n";
    std::cout << "========================================================\n";
}

int main() {
    RunUnitTests();
    return 0;
}
