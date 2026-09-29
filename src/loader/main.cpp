#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
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
// Console Styling & Formatted Logging
// ============================================================================
enum class LogLevel {
    Info,
    Success,
    Warning,
    Error,
    Debug,
    Status
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
    case LogLevel::Status:
        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_INTENSITY); // Bright Blue
        break;
    }
}

void ResetConsoleColor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
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
void LogStatus(const std::string& msg)  { Log(LogLevel::Status,  "[>]", msg); }

// ============================================================================
// Privilege Escalation
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
        LogWarning("SeDebugPrivilege is not held by the caller. Running launcher as Administrator is recommended.");
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    LogSuccess("SeDebugPrivilege enabled successfully.");
    return true;
}

// ============================================================================
// Process Discovery & Application Launch
// ============================================================================
DWORD FindProcessIdByName(const std::string& processName) {
    DWORD pid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
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

bool LaunchApplicationViaSteam() {
    LogInfo("Sending start request to Steam protocol (steam://rungameid/570)...");
    INT_PTR result = (INT_PTR)ShellExecuteA(
        NULL,
        "open",
        "steam://rungameid/570",
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if (result > 32) {
        LogSuccess("Steam application launch command executed successfully.");
        return true;
    } else {
        LogError("ShellExecuteA failed. Result code: " + std::to_string(result));
        return false;
    }
}

DWORD WaitOrLaunchProcess(const std::string& processName, uint32_t timeoutSeconds) {
    LogStatus("STEP 1: Locating target process '" + processName + "'...");

    DWORD pid = FindProcessIdByName(processName);
    if (pid != 0) {
        LogSuccess("Process detected! Target PID: " + std::to_string(pid));
        return pid;
    }

    LogWarning("Process '" + processName + "' is not currently running.");
    LaunchApplicationViaSteam();

    LogInfo("Waiting for process to appear (Timeout: " + std::to_string(timeoutSeconds) + "s)...");

    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        pid = FindProcessIdByName(processName);
        if (pid != 0) {
            LogSuccess("Target process launched and detected! PID: " + std::to_string(pid));
            return pid;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();

        if (elapsed >= timeoutSeconds) {
            LogError("Timeout waiting for process start (" + std::to_string(timeoutSeconds) + "s).");
            return 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// ============================================================================
// Module Inspection & Readiness Checking
// ============================================================================
bool CheckModuleLoadedSnapshot(DWORD processId, const std::string& moduleName, uintptr_t& outModuleBase) {
    outModuleBase = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    bool found = false;
    if (Module32First(hSnapshot, &me32)) {
        do {
            std::string modName;
#ifdef UNICODE
            std::wstring wMod(me32.szModule);
            modName = std::string(wMod.begin(), wMod.end());
#else
            modName = me32.szModule;
#endif
            if (_stricmp(modName.c_str(), moduleName.c_str()) == 0) {
                outModuleBase = (uintptr_t)me32.modBaseAddr;
                found = true;
                break;
            }
        } while (Module32Next(hSnapshot, &me32));
    }

    CloseHandle(hSnapshot);
    return found;
}

bool CheckModuleLoadedPsapi(HANDLE hProcess, const std::string& moduleName, uintptr_t& outModuleBase) {
    outModuleBase = 0;
    HMODULE hMods[1024];
    DWORD cbNeeded = 0;

    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
        size_t count = cbNeeded / sizeof(HMODULE);
        for (size_t i = 0; i < count; i++) {
            char szModName[MAX_PATH] = { 0 };
            if (GetModuleBaseNameA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(char))) {
                if (_stricmp(szModName, moduleName.c_str()) == 0) {
                    MODULEINFO mi = { 0 };
                    if (GetModuleInformation(hProcess, hMods[i], &mi, sizeof(mi))) {
                        outModuleBase = (uintptr_t)mi.lpBaseOfDll;
                    } else {
                        outModuleBase = (uintptr_t)hMods[i];
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool WaitForTargetModules(DWORD processId, HANDLE hProcess, const std::vector<std::string>& requiredModules, uint32_t timeoutSeconds) {
    LogStatus("STEP 2: Waiting for core engine modules to initialize...");
    
    for (const auto& modName : requiredModules) {
        LogInfo("Monitoring module initialization: " + modName);
        auto startTime = std::chrono::steady_clock::now();
        uintptr_t modBase = 0;
        bool loaded = false;

        while (true) {
            // Method A: PSAPI EnumProcessModulesEx
            if (CheckModuleLoadedPsapi(hProcess, modName, modBase)) {
                loaded = true;
                break;
            }

            // Method B: Toolhelp Snapshot Fallback
            if (CheckModuleLoadedSnapshot(processId, modName, modBase)) {
                loaded = true;
                break;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime
            ).count();

            if (elapsed >= timeoutSeconds) {
                LogError("Timeout waiting for module '" + modName + "' initialization (" + std::to_string(timeoutSeconds) + "s).");
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::stringstream ssBase;
        ssBase << "0x" << std::hex << std::uppercase << modBase;
        LogSuccess("Module '" + modName + "' fully loaded at base address: " + ssBase.str());
    }

    LogSuccess("All required target modules are fully loaded into process memory space.");
    return true;
}

// ============================================================================
// DLL Path Resolution
// ============================================================================
std::string ResolveDllPath(const std::string& inputPath) {
    std::string candidate = inputPath;
    if (fs::exists(candidate)) {
        char fullBuf[MAX_PATH] = { 0 };
        if (GetFullPathNameA(candidate.c_str(), MAX_PATH, fullBuf, NULL) > 0) {
            return std::string(fullBuf);
        }
    }

    // Fallback: Check relative to executable directory
    char exePathBuf[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(NULL, exePathBuf, MAX_PATH) > 0) {
        fs::path exeDir = fs::path(exePathBuf).parent_path();
        
        fs::path checkPath1 = exeDir / inputPath;
        if (fs::exists(checkPath1)) {
            char fullBuf[MAX_PATH] = { 0 };
            if (GetFullPathNameA(checkPath1.string().c_str(), MAX_PATH, fullBuf, NULL) > 0) {
                return std::string(fullBuf);
            }
        }

        fs::path checkPath2 = exeDir / fs::path(inputPath).filename();
        if (fs::exists(checkPath2)) {
            char fullBuf[MAX_PATH] = { 0 };
            if (GetFullPathNameA(checkPath2.string().c_str(), MAX_PATH, fullBuf, NULL) > 0) {
                return std::string(fullBuf);
            }
        }
    }

    return "";
}

// ============================================================================
// Controlled Remote Thread Injection
// ============================================================================
bool InjectModule(DWORD processId, HANDLE hProcess, const std::string& dllPath) {
    LogStatus("STEP 3: Executing synchronized DLL injection...");

    std::string fullPath = ResolveDllPath(dllPath);
    if (fullPath.empty()) {
        LogError("Target DLL file not found: " + dllPath);
        return false;
    }

    uintmax_t fileSize = fs::file_size(fullPath);
    LogInfo("Resolved DLL Path : " + fullPath);
    LogInfo("Module Image Size : " + std::to_string(fileSize) + " bytes");

    // 1. Memory Allocation
    size_t pathLengthBytes = fullPath.length() + 1;
    LogInfo("Allocating " + std::to_string(pathLengthBytes) + " bytes in target process virtual memory...");

    LPVOID pRemoteBuffer = VirtualAllocEx(
        hProcess,
        NULL,
        pathLengthBytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!pRemoteBuffer) {
        DWORD err = GetLastError();
        LogError("VirtualAllocEx failed. Error code: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        return false;
    }

    std::stringstream ssRemoteMem;
    ssRemoteMem << "0x" << std::hex << std::uppercase << (uintptr_t)pRemoteBuffer;
    LogSuccess("Remote memory allocated at: " + ssRemoteMem.str());

    // 2. Write Path String
    LogInfo("Writing path buffer to target memory...");
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteBuffer, fullPath.c_str(), pathLengthBytes, &bytesWritten) || bytesWritten != pathLengthBytes) {
        DWORD err = GetLastError();
        LogError("WriteProcessMemory failed. Error: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        return false;
    }
    LogSuccess("Successfully wrote " + std::to_string(bytesWritten) + " bytes.");

    // 3. Resolve Kernel32!LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibraryA) {
        LogError("Failed to resolve Kernel32!LoadLibraryA pointer.");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        return false;
    }

    std::stringstream ssFunc;
    ssFunc << "0x" << std::hex << std::uppercase << (uintptr_t)pLoadLibraryA;
    LogInfo("Kernel32!LoadLibraryA procedure entry point: " + ssFunc.str());

    // 4. Create Remote Thread
    LogInfo("Spawning synchronized remote thread in target process...");
    DWORD remoteThreadId = 0;
    HANDLE hThread = CreateRemoteThread(
        hProcess,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)pLoadLibraryA,
        pRemoteBuffer,
        0,
        &remoteThreadId
    );

    if (!hThread || hThread == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogError("CreateRemoteThread failed. Error: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        return false;
    }

    LogSuccess("Remote thread created successfully. Thread ID: " + std::to_string(remoteThreadId));

    // 5. Synchronized Execution Check
    LogInfo("Waiting for remote thread completion...");
    DWORD waitCode = WaitForSingleObject(hThread, 20000); // 20s wait

    bool injectionSuccess = false;
    if (waitCode == WAIT_OBJECT_0) {
        DWORD exitCode = 0;
        if (GetExitCodeThread(hThread, &exitCode)) {
            std::stringstream ssHandle;
            ssHandle << "0x" << std::hex << std::uppercase << exitCode;

            if (exitCode != 0) {
                LogSuccess("Remote LoadLibraryA returned valid HMODULE: " + ssHandle.str());
                injectionSuccess = true;
            } else {
                LogError("Remote LoadLibraryA returned NULL (0x0). Module failed to load inside target process.");
                LogError("Check module dependencies or internal initialization errors.");
            }
        } else {
            LogError("GetExitCodeThread failed.");
        }
    } else if (waitCode == WAIT_TIMEOUT) {
        LogError("WaitForSingleObject timed out. Remote thread execution hung.");
    } else {
        LogError("WaitForSingleObject returned error code: " + std::to_string(waitCode));
    }

    // Cleanup
    LogInfo("Freeing remote memory buffer and closing thread handle...");
    VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
    CloseHandle(hThread);

    return injectionSuccess;
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    std::cout << "=======================================================\n";
    std::cout << "   Spectre Source 2 Automation Loader (Win32 / C++)\n";
    std::cout << "=======================================================\n\n";

    std::string targetProcess = "dota2.exe";
    std::string targetDll     = "bin\\Spectre.dll";
    uint32_t waitTimeoutSec   = 45;

    if (argc >= 2) targetDll = argv[1];
    if (argc >= 3) targetProcess = argv[2];
    if (argc >= 4) waitTimeoutSec = (uint32_t)std::atoi(argv[3]);

    LogInfo("Configuration setup:");
    LogInfo(" - Target Application : " + targetProcess);
    LogInfo(" - Injection Module  : " + targetDll);
    LogInfo(" - Launch Timeout    : " + std::to_string(waitTimeoutSec) + " seconds");
    std::cout << "\n";

    EnableDebugPrivilege();

    // 1. Locate / Launch Process
    DWORD pid = WaitOrLaunchProcess(targetProcess, waitTimeoutSec);
    if (pid == 0) {
        LogError("Loader operation aborted: Target process not found.");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    // 2. Open Process Handle
    LogInfo("Opening target process handle...");
    DWORD desiredAccess = PROCESS_CREATE_THREAD | 
                          PROCESS_VM_OPERATION | 
                          PROCESS_VM_WRITE | 
                          PROCESS_VM_READ | 
                          PROCESS_QUERY_INFORMATION;

    HANDLE hProcess = OpenProcess(desiredAccess, FALSE, pid);
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogError("OpenProcess failed. Error code: " + std::to_string(err) + " (" + GetSystemErrorString(err) + ")");
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }
    LogSuccess("Acquired valid process handle.");

    // 3. Wait for Core Modules (client.dll, engine2.dll)
    std::vector<std::string> requiredModules = { "client.dll", "engine2.dll" };
    if (!WaitForTargetModules(pid, hProcess, requiredModules, waitTimeoutSec)) {
        LogError("Loader operation aborted: Core target modules failed to initialize.");
        CloseHandle(hProcess);
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
        return 1;
    }

    // Brief stabilization delay after module detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 4. Inject Module
    bool status = InjectModule(pid, hProcess, targetDll);

    CloseHandle(hProcess);

    std::cout << "\n=======================================================\n";
    if (status) {
        LogSuccess("LOADER OPERATION COMPLETED SUCCESSFULLY.");
    } else {
        LogError("LOADER OPERATION FAILED.");
    }
    std::cout << "=======================================================\n";

    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    return status ? 0 : 1;
}
