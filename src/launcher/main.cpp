#pragma comment(lib, "advapi32.lib")

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

// ============================================================================
// Console Output & Logging Utilities
// ============================================================================
enum class LogLevel {
    Info,
    Success,
    Warning,
    Error,
    Debug
};

void SetConsoleColor(LogLevel level) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    switch (level) {
    case LogLevel::Info:
        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Cyan
        break;
    case LogLevel::Success:
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Bright Green
        break;
    case LogLevel::Warning:
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Bright Yellow
        break;
    case LogLevel::Error:
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY); // Bright Red
        break;
    case LogLevel::Debug:
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY); // Magenta
        break;
    }
}

void ResetConsoleColor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Default White/Gray
}

std::string GetSystemErrorString(DWORD errorCode) {
    if (errorCode == 0) return "No error";
    
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    // Trim trailing newlines
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
}

void Log(LogLevel level, const std::string& prefix, const std::string& message) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    std::cout << "[";
    std::cout << std::setfill('0') << std::setw(2) << st.wHour << ":"
              << std::setfill('0') << std::setw(2) << st.wMinute << ":"
              << std::setfill('0') << std::setw(2) << st.wSecond;
    std::cout << "] ";

    SetConsoleColor(level);
    std::cout << prefix << " ";
    ResetConsoleColor();

    std::cout << message << std::endl;
}

void LogInfo(const std::string& msg)    { Log(LogLevel::Info,    "[i]", msg); }
void LogSuccess(const std::string& msg) { Log(LogLevel::Success, "[+]", msg); }
void LogWarning(const std::string& msg) { Log(LogLevel::Warning, "[*]", msg); }
void LogError(const std::string& msg)   { Log(LogLevel::Error,   "[-]", msg); }
void LogDebug(const std::string& msg)   { Log(LogLevel::Debug,   "[~]", msg); }

// ============================================================================
// System Privilege Helper
// ============================================================================
bool EnableDebugPrivilege() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LogWarning("Failed to open process token. Error: " + std::to_string(GetLastError()));
        return false;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
        LogWarning("LookupPrivilegeValue failed. Error: " + std::to_string(GetLastError()));
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        LogWarning("AdjustTokenPrivileges failed. Error: " + std::to_string(GetLastError()));
        CloseHandle(hToken);
        return false;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        LogWarning("SeDebugPrivilege is not held by the caller. Try running launcher as Administrator.");
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    LogSuccess("SeDebugPrivilege enabled successfully.");
    return true;
}

// ============================================================================
// Process Discovery & Launching
// ============================================================================
DWORD FindProcessIdByName(const std::string& processName) {
    DWORD pid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        LogError("CreateToolhelp32Snapshot failed. Error: " + std::to_string(GetLastError()));
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            std::string exeFile;
#ifdef UNICODE
            std::wstring wExeFile(pe32.szExeFile);
            exeFile = std::string(wExeFile.begin(), wExeFile.end());
#else
            exeFile = pe32.szExeFile;
#endif
            if (_stricmp(exeFile.c_str(), processName.c_str()) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return pid;
}

bool AttemptLaunchProcess(const std::string& processName) {
    LogInfo("Attempting to launch process via Steam protocol (steam://rungameid/570)...");
    
    INT_PTR result = (INT_PTR)ShellExecuteA(
        NULL,
        "open",
        "steam://rungameid/570",
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if (result > 32) {
        LogSuccess("Launch command sent to Steam successfully.");
        return true;
    } else {
        LogError("ShellExecuteA failed to launch Steam application. Result code: " + std::to_string(result));
        return false;
    }
}

DWORD WaitOrLaunchProcess(const std::string& processName, uint32_t timeoutSeconds) {
    LogInfo("Searching for target process: " + processName);

    DWORD pid = FindProcessIdByName(processName);
    if (pid != 0) {
        LogSuccess("Found running process! PID: " + std::to_string(pid));
        return pid;
    }

    LogWarning("Target process '" + processName + "' is not running.");
    AttemptLaunchProcess(processName);

    LogInfo("Waiting for process '" + processName + "' to start (Timeout: " + std::to_string(timeoutSeconds) + "s)...");

    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        pid = FindProcessIdByName(processName);
        if (pid != 0) {
            LogSuccess("Target process detected! PID: " + std::to_string(pid));
            return pid;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();

        if (elapsed >= timeoutSeconds) {
            LogError("Timeout reached (" + std::to_string(timeoutSeconds) + "s). Target process was not found.");
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// ============================================================================
// Win32 Dynamic DLL Injection (LoadLibraryA Remote Thread)
// ============================================================================
bool InjectDllWin32(DWORD processId, const std::string& relativeOrAbsolutePath) {
    std::cout << "\n=======================================================\n";
    LogInfo("Starting DLL Injection Sequence...");
    std::cout << "=======================================================\n";

    // 1. Path Resolution & Validation
    std::string resolvedPath = relativeOrAbsolutePath;
    if (!fs::exists(resolvedPath)) {
        // Fallback: check relative to the launcher executable folder
        char exePathBuf[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(NULL, exePathBuf, MAX_PATH) > 0) {
            fs::path exeDir = fs::path(exePathBuf).parent_path();
            fs::path fallbackPath = exeDir / relativeOrAbsolutePath;
            if (fs::exists(fallbackPath)) {
                resolvedPath = fallbackPath.string();
            } else {
                // Also try filename only in exe folder
                fallbackPath = exeDir / fs::path(relativeOrAbsolutePath).filename();
                if (fs::exists(fallbackPath)) {
                    resolvedPath = fallbackPath.string();
                }
            }
        }
    }

    if (!fs::exists(resolvedPath)) {
        LogError("Target DLL file does not exist: " + relativeOrAbsolutePath);
        return false;
    }

    char absolutePath[MAX_PATH] = { 0 };
    DWORD pathLen = GetFullPathNameA(resolvedPath.c_str(), MAX_PATH, absolutePath, NULL);
    if (pathLen == 0 || pathLen >= MAX_PATH) {
        LogError("Failed to resolve absolute path for: " + resolvedPath);
        return false;
    }

    std::string fullDllPath(absolutePath);
    uintmax_t fileSize = fs::file_size(fullDllPath);

    LogInfo("Target Process PID : " + std::to_string(processId));
    LogInfo("Target DLL Path    : " + fullDllPath);
    LogInfo("Target DLL Size    : " + std::to_string(fileSize) + " bytes");

    // 2. Open Target Process
    LogInfo("Opening target process with memory allocation & thread privileges...");
    DWORD desiredAccess = PROCESS_CREATE_THREAD | 
                          PROCESS_VM_OPERATION | 
                          PROCESS_VM_WRITE | 
                          PROCESS_VM_READ | 
                          PROCESS_QUERY_INFORMATION;

    HANDLE hProcess = OpenProcess(desiredAccess, FALSE, processId);
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogError("OpenProcess failed. Error code: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        return false;
    }
    LogSuccess("Process handle acquired successfully (Handle: 0x" + std::to_string((uintptr_t)hProcess) + ")");

    // 3. Allocate Remote Memory for Path String
    size_t pathSizeBytes = fullDllPath.length() + 1;
    LogInfo("Allocating " + std::to_string(pathSizeBytes) + " bytes of virtual memory in target process...");

    LPVOID pRemoteBuffer = VirtualAllocEx(
        hProcess,
        NULL,
        pathSizeBytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!pRemoteBuffer) {
        DWORD err = GetLastError();
        LogError("VirtualAllocEx failed. Error code: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        CloseHandle(hProcess);
        return false;
    }

    std::stringstream ssAddr;
    ssAddr << "0x" << std::hex << std::uppercase << (uintptr_t)pRemoteBuffer;
    LogSuccess("Remote memory allocated at base address: " + ssAddr.str());

    // 4. Write DLL Path String to Remote Memory
    LogInfo("Writing DLL path string into allocated target process memory...");
    SIZE_T bytesWritten = 0;
    BOOL writeStatus = WriteProcessMemory(
        hProcess,
        pRemoteBuffer,
        fullDllPath.c_str(),
        pathSizeBytes,
        &bytesWritten
    );

    if (!writeStatus || bytesWritten != pathSizeBytes) {
        DWORD err = GetLastError();
        LogError("WriteProcessMemory failed. Bytes written: " + std::to_string(bytesWritten) + 
                 "/" + std::to_string(pathSizeBytes) + ". Error: " + GetSystemErrorString(err));
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    LogSuccess("Successfully wrote " + std::to_string(bytesWritten) + " bytes into target memory space.");

    // 5. Locate LoadLibraryA Address in Kernel32.dll
    LogInfo("Locating Kernel32!LoadLibraryA entry point...");
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        LogError("GetModuleHandleA('kernel32.dll') failed.");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibraryA) {
        LogError("GetProcAddress('LoadLibraryA') failed.");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::stringstream ssFunc;
    ssFunc << "0x" << std::hex << std::uppercase << (uintptr_t)pLoadLibraryA;
    LogSuccess("LoadLibraryA procedure located at: " + ssFunc.str());

    // 6. Create Remote Thread
    LogInfo("Creating remote thread inside target process...");
    DWORD remoteThreadId = 0;
    HANDLE hRemoteThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)pLoadLibraryA,
        pRemoteBuffer,
        0,
        &remoteThreadId
    );

    if (!hRemoteThread || hRemoteThread == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogError("CreateRemoteThread failed. Error code: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LogSuccess("Remote thread launched successfully! Thread ID: " + std::to_string(remoteThreadId));

    // 7. Wait for Execution Completion
    LogInfo("Waiting for remote thread to finish executing LoadLibraryA (Timeout: 15s)...");
    DWORD waitResult = WaitForSingleObject(hRemoteThread, 15000);

    bool success = false;
    if (waitResult == WAIT_OBJECT_0) {
        DWORD remoteExitCode = 0;
        if (GetExitCodeThread(hRemoteThread, &remoteExitCode)) {
            std::stringstream ssModHandle;
            ssModHandle << "0x" << std::hex << std::uppercase << remoteExitCode;

            if (remoteExitCode != 0) {
                LogSuccess("Remote thread executed successfully!");
                LogSuccess("Injected Module Base Handle returned by target: " + ssModHandle.str());
                success = true;
            } else {
                LogError("LoadLibraryA returned NULL (0x0) in target process!");
                LogError("Possible causes: missing DLL dependencies, architecture mismatch (x86 vs x64), or DllMain failure.");
            }
        } else {
            LogError("GetExitCodeThread failed. Error: " + std::to_string(GetLastError()));
        }
    } else if (waitResult == WAIT_TIMEOUT) {
        LogError("WaitForSingleObject timed out! Remote thread hung or deadlocked.");
    } else {
        LogError("WaitForSingleObject failed with code: " + std::to_string(waitResult));
    }

    // 8. Cleanup Remote Memory and Handles
    LogInfo("Cleaning up target memory allocation and handle references...");
    VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
    CloseHandle(hRemoteThread);
    CloseHandle(hProcess);

    if (success) {
        LogSuccess("Injection pipeline completed successfully!");
    } else {
        LogError("Injection pipeline failed.");
    }

    return success;
}

// ============================================================================
// Main Launcher Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    // Enable ANSI color escape support in Windows Terminal / Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    std::cout << "=======================================================\n";
    std::cout << "   Spectre Debug & Automation Launcher (Win32 / C++)\n";
    std::cout << "=======================================================\n\n";

    // Configuration defaults
    std::string targetProcess = "dota2.exe";
    std::string targetDll     = "bin\\Spectre.dll";
    uint32_t waitTimeoutSec   = 30;

    // CLI overrides
    if (argc >= 2) targetDll = argv[1];
    if (argc >= 3) targetProcess = argv[2];
    if (argc >= 4) waitTimeoutSec = (uint32_t)std::atoi(argv[3]);

    LogInfo("Configuration setup:");
    LogInfo(" - Target Process Name : " + targetProcess);
    LogInfo(" - Target Module Path  : " + targetDll);
    LogInfo(" - Launch Timeout (s)  : " + std::to_string(waitTimeoutSec));
    std::cout << "\n";

    // Step 1: Request SeDebugPrivilege
    EnableDebugPrivilege();

    // Step 2: Locate or Wait for Process
    DWORD targetPid = WaitOrLaunchProcess(targetProcess, waitTimeoutSec);
    if (targetPid == 0) {
        LogError("Fatal: Failed to locate target process. Exiting launcher.");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    // Step 3: Perform Injection
    bool injected = InjectDllWin32(targetPid, targetDll);

    std::cout << "\n=======================================================\n";
    if (injected) {
        LogSuccess("ALL OPERATIONS COMPLETED SUCCESSFULLY.");
    } else {
        LogError("LAUNCHER OPERATION FAILED.");
    }
    std::cout << "=======================================================\n";

    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return injected ? 0 : 1;
}
