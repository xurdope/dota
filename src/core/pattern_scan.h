#pragma once
#include <cstdint>
#include <string>

namespace PatternScan {

    // Ищет паттерн в памяти модуля. Возвращает VA первого совпадения или 0.
    uintptr_t Find(const char* moduleName, const char* pattern);

    uintptr_t GetModuleBase(const char* moduleName);
    size_t    GetModuleSize(const char* moduleName);

    // Разрешает RIP-relative указатель и разыменовывает его (для глобальных ptr).
    uintptr_t ResolveRelativePointer(const char* moduleName,
                                     const char* pattern,
                                     int relOffset,
                                     int instrSize);

    // Разрешает RIP-relative адрес БЕЗ разыменования (для адреса переменной).
    uintptr_t ResolveAddress(const char* moduleName,
                             const char* pattern,
                             int relOffset,
                             int instrSize);

} // namespace PatternScan