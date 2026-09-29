#pragma once
#include <windows.h>
#include <functional>
#include <cstdint>

namespace MemoryGuard {

    // Проверяет, что указатель указывает на читаемую/исполняемую память
    inline bool IsValidPointer(const void* ptr) {
        if (!ptr) return false;
        const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;

        const DWORD mask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);

        return !(mbi.Protect & PAGE_GUARD) && (mbi.Protect & mask);
    }

    // Проверяет, что можно писать
    inline bool IsWritablePointer(const void* ptr) {
        if (!ptr) return false;
        const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0) return false;
        if (mbi.State != MEM_COMMIT) return false;

        const DWORD mask = (PAGE_READWRITE | PAGE_WRITECOPY |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);

        return !(mbi.Protect & PAGE_GUARD) && (mbi.Protect & mask);
    }

    // Безопасное чтение T по адресу. Возвращает false, если адрес невалиден.
    template<typename T>
    inline bool SafeRead(uintptr_t addr, T& out) {
        if (!IsValidPointer(reinterpret_cast<const void*>(addr))) return false;
        out = *reinterpret_cast<const T*>(addr);
        return true;
    }

    // Безопасная запись T по адресу.
    template<typename T>
    inline bool SafeWrite(uintptr_t addr, const T& value) {
        if (!IsWritablePointer(reinterpret_cast<const void*>(addr))) return false;
        *reinterpret_cast<T*>(addr) = value;
        return true;
    }

    // Выполняет void-функцию под Windows SEH + C++ try/catch.
    bool SafeExecute(const std::function<void()>& func);

} // namespace MemoryGuard