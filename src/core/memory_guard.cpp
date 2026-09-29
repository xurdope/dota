#include "memory_guard.h"
#include "../system/system_utils.h"

namespace MemoryGuard {

    namespace {
        using TargetFn = void(*)(void*);

        struct SEHContext {
            const std::function<void()>* funcPtr;
        };

        // Вызывается внутри __try. Никаких C++ объектов в этом скоупе.
        void SEHWrapperCallback(void* context) {
            auto* sehCtx = static_cast<SEHContext*>(context);
            if (sehCtx && sehCtx->funcPtr && *(sehCtx->funcPtr)) {
                (*(sehCtx->funcPtr))();
            }
        }

        // Отдельная функция для SEH — без C++ unwind в одном скоупе с __try.
        DWORD ExecuteWithSEH(TargetFn targetFunc, void* context) {
            __try {
                targetFunc(context);
                return 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return GetExceptionCode();
            }
        }
    } // anon namespace

    bool SafeExecute(const std::function<void()>& func) {
        if (!func) return false;

        SEHContext ctx{ &func };
        DWORD exceptionCode = 0;

        // Ловим C++ исключения (SEH их не берёт).
        try {
            exceptionCode = ExecuteWithSEH(SEHWrapperCallback, &ctx);
        }
        catch (const std::exception& e) {
            SystemUtils::Log("[MemoryGuard] C++ exception: %s", e.what());
            return false;
        }
        catch (...) {
            SystemUtils::Log("[MemoryGuard] Unknown C++ exception caught.");
            return false;
        }

        if (exceptionCode != 0) {
            SystemUtils::Log("[MemoryGuard] SEH EXCEPTION! Code: 0x%08X", exceptionCode);
            return false;
        }
        return true;
    }

} // namespace MemoryGuard