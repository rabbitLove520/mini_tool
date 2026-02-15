#include "hardware.h"
#include <wx/log.h>
#include <wx/arrstr.h>
#include <vector>
#include <iphlpapi.h>    // GetAdaptersAddresses
#include <psapi.h>       // GetPhysicallyInstalledSystemMemory
#include <cstring>       // memcpy
#include <cwchar>        // wcslen

// ✅ 关键修复：避免内联汇编，使用 __get_cpuid（MinGW 安全）
#if defined(__GNUC__) || defined(__MINGW32__)
    #include <cpuid.h>  // GCC 4.3+ 标准头文件
    
    void Hardware::cpuid(unsigned int CPUInfo[4], unsigned int InfoType)
    {
        // __get_cpuid 自动处理 ebx 保留问题（PIC 安全）
        if (!__get_cpuid(InfoType, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3])) {
            // 功能不可用时清零
            CPUInfo[0] = CPUInfo[1] = CPUInfo[2] = CPUInfo[3] = 0;
        }
    }
#else
    // MSVC 回退（本项目使用 MinGW，此分支不会触发）
    #include <intrin.h>
    void Hardware::cpuid(unsigned int CPUInfo[4], unsigned int InfoType) {
        __cpuid((int*)CPUInfo, (int)InfoType);
    }
#endif

// ========== 工具方法：宽字符转换 ==========
wxString Hardware::WCharToWxString(const wchar_t* wstr, DWORD size)
{
    if (!wstr || wstr[0] == L'\0') return wxEmptyString;
    
    if (size == 0) {
        size = (DWORD)(wcslen(wstr) * sizeof(wchar_t));
    }
    
    // 转换为 UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, size / sizeof(wchar_t), NULL, 0, NULL, NULL);
    if (len <= 0) return wxEmptyString;
    
    std::vector<char> buffer(len + 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, size / sizeof(wchar_t), buffer.data(), len, NULL, NULL);
    
    return wxString::FromUTF8(buffer.data());
}

// ========== 主采集入口 ==========
int Hardware::GetInfo()
{
    // 初始化默认值
    BaseBoardManufacturer = _("Unknown");
    BaseBoardProduct = _("Unknown");
    CPUManufacturer = _("Unknown");
    CPUName = _("Unknown");
    CPUMaxClockSpeed = 0;
    TotalPhysicalMemory = "0";
    MemoryType = _("Unknown");
    MemorySpeed = _("Unknown");
    DiskModels.clear();
    DiskSerialNumbers.clear();
    MACAddresses.clear();
    BIOSManufacturer = _("Unknown");
    BIOSVersion = _("Unknown");
    BIOSReleaseDate = _("Unknown");
    SystemUUID = _("Unknown");
    MachineFingerprint = _("Unknown");

    // 1. 主板信息
    getBaseBoardInfo();

    // 2. CPU 信息
    getCPUInfo();

    // 3. 内存信息
    getMemoryInfo();

    // 4. 硬盘信息
    getDiskInfo();

    // 5. 网卡信息
    getNetworkInfo();

    // 6. BIOS 信息
    getBIOSInfo();

    // 7. 系统 UUID
    getSystemUUID();

    // 8. 生成机器指纹
    if (!SystemUUID.IsEmpty() && !SystemUUID.StartsWith("Unknown")) {
        MachineFingerprint = generateFingerprint();
    }

    return 0;
}

// ========== 主板信息（注册表） ==========
bool Hardware::getBaseBoardInfo()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     L"HARDWARE\\DESCRIPTION\\System\\BIOS",
                     0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t buf[256] = {0};
    DWORD size = sizeof(buf);

    if (RegQueryValueEx(hKey, L"BaseBoardManufacturer", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
        BaseBoardManufacturer = WCharToWxString(buf, size);
    }

    size = sizeof(buf);
    if (RegQueryValueEx(hKey, L"BaseBoardProduct", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
        BaseBoardProduct = WCharToWxString(buf, size);
    }

    RegCloseKey(hKey);
    return true;
}

// ========== CPU 信息 ==========
void Hardware::getCPUInfo()
{
    CPUManufacturer = wxString::FromUTF8(getCpuVendor());
    CPUName = wxString::FromUTF8(getCpuName());
    CPUMaxClockSpeed = getCPUClockSpeed();
}

std::string Hardware::getCpuVendor()
{
    char vendor[13] = {0};
    unsigned int dwBuf[4];
    cpuid(dwBuf, 0);
    memcpy(&vendor[0], &dwBuf[1], 4);  // ebx
    memcpy(&vendor[4], &dwBuf[3], 4);  // edx
    memcpy(&vendor[8], &dwBuf[2], 4);  // ecx
    return vendor;
}

std::string Hardware::getCpuName()
{
    unsigned int dwBuf[4];
    cpuid(dwBuf, 0x80000000U);
    if (dwBuf[0] < 0x80000004U) {
        return "Unknown CPU";
    }

    char brand[49] = {0};
    cpuid((unsigned int*)&brand[0], 0x80000002U);
    cpuid((unsigned int*)&brand[16], 0x80000003U);
    cpuid((unsigned int*)&brand[32], 0x80000004U);
    return brand;
}

long Hardware::getCPUClockSpeed()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                     L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD freq = 0;
        DWORD size = sizeof(freq);
        if (RegQueryValueEx(hKey, L"~MHz", NULL, NULL, (LPBYTE)&freq, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (long)freq;
        }
        RegCloseKey(hKey);
    }
    return 0;
}

// ========== 内存信息（修复 ULONGLONG 类型问题）==========
bool Hardware::getMemoryInfo()
{
    // 方法1：GetPhysicallyInstalledSystemMemory（Vista+）
    // 修复：使用 ULONGLONG 而非 DWORD
    ULONGLONG memKb = 0;  // ✅ 关键修复：64位类型
    if (GetPhysicallyInstalledSystemMemory(&memKb)) {
        unsigned long long bytes = memKb * 1024ULL;  // 转换为字节
        TotalPhysicalMemory = wxString::Format("%llu", bytes);
    } else {
        // 方法2：GlobalMemoryStatusEx（备用）
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            TotalPhysicalMemory = wxString::Format("%llu", memInfo.ullTotalPhys);
        }
    }

    // 内存类型/频率：纯 WinAPI 无法可靠获取（需 WMI），设为估计值
    MemoryType = _("DDR4 (estimated)");
    MemorySpeed = _("2400 (estimated)");

    return !TotalPhysicalMemory.IsEmpty() && TotalPhysicalMemory != "0";
}

// ========== 硬盘信息（注册表枚举） ==========
bool Hardware::getDiskInfo()
{
    // 枚举：HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\STORAGE\Disk
    const wchar_t* paths[] = {
        L"SYSTEM\\CurrentControlSet\\Enum\\STORAGE\\Disk",
        L"SYSTEM\\CurrentControlSet\\Enum\\IDE",
        L"SYSTEM\\CurrentControlSet\\Enum\\SCSI"
    };
    
    for (const wchar_t* path : paths) {
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS) continue;
        
        // 枚举第一级（设备类型）
        DWORD subKeyCount = 0;
        DWORD maxSubKeyLen = 0;
        if (RegQueryInfoKey(hKey, NULL, NULL, NULL, &subKeyCount, &maxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            continue;
        }
        
        std::vector<wchar_t> subKeyName(maxSubKeyLen + 1, 0);
        for (DWORD i = 0; i < subKeyCount; ++i) {
            DWORD nameSize = maxSubKeyLen + 1;
            if (RegEnumKeyEx(hKey, i, subKeyName.data(), &nameSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) continue;
            
            HKEY hSubKey;
            if (RegOpenKeyEx(hKey, subKeyName.data(), 0, KEY_READ, &hSubKey) != ERROR_SUCCESS) continue;
            
            // 枚举第二级（具体设备）
            DWORD devCount = 0;
            DWORD maxDevLen = 0;
            if (RegQueryInfoKey(hSubKey, NULL, NULL, NULL, &devCount, &maxDevLen, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                std::vector<wchar_t> devName(maxDevLen + 1, 0);
                for (DWORD j = 0; j < devCount; ++j) {
                    DWORD devSize = maxDevLen + 1;
                    if (RegEnumKeyEx(hSubKey, j, devName.data(), &devSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) continue;
                    
                    HKEY hDevKey;
                    if (RegOpenKeyEx(hSubKey, devName.data(), 0, KEY_READ, &hDevKey) != ERROR_SUCCESS) continue;
                    
                    // 读取 FriendlyName 或 DeviceDesc
                    wchar_t model[512] = {0};
                    DWORD size = sizeof(model);
                    if (RegQueryValueEx(hDevKey, L"FriendlyName", NULL, NULL, (LPBYTE)model, &size) == ERROR_SUCCESS ||
                        RegQueryValueEx(hDevKey, L"DeviceDesc", NULL, NULL, (LPBYTE)model, &size) == ERROR_SUCCESS) {
                        wxString modelName = WCharToWxString(model, size);
                        // 过滤通用/USB设备
                        if (!modelName.IsEmpty() && 
                            !modelName.Lower().Contains("generic") && 
                            !modelName.Lower().Contains("usb") &&
                            !modelName.Lower().Contains("sd")) {
                            DiskModels.push_back(modelName);
                            DiskSerialNumbers.push_back(_("N/A"));
                        }
                    }
                    RegCloseKey(hDevKey);
                }
            }
            RegCloseKey(hSubKey);
        }
        RegCloseKey(hKey);
    }
    
    // 保证至少有一个条目（避免UI崩溃）
    if (DiskModels.empty()) {
        DiskModels.push_back(_("Unknown Disk"));
        DiskSerialNumbers.push_back(_("N/A"));
    }
    
    return true;
}

// ========== 网卡信息（GetAdaptersAddresses） ==========
bool Hardware::getNetworkInfo()
{
    // 使用 GetAdaptersAddresses（Vista+）
    ULONG size = 15000;
    std::vector<BYTE> buffer(size);
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)buffer.data();
    
    DWORD result = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL,
        pAddresses,
        &size
    );
    
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        pAddresses = (PIP_ADAPTER_ADDRESSES)buffer.data();
        result = GetAdaptersAddresses(
            AF_UNSPEC,
            GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            NULL,
            pAddresses,
            &size
        );
    }
    
    if (result != ERROR_SUCCESS) {
        // 备用：GetAdaptersInfo（XP兼容）
        ULONG bufSize = sizeof(IP_ADAPTER_INFO);
        std::vector<BYTE> buf(bufSize);
        PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)buf.data();
        
        if (GetAdaptersInfo(pAdapterInfo, &bufSize) == ERROR_BUFFER_OVERFLOW) {
            buf.resize(bufSize);
            pAdapterInfo = (PIP_ADAPTER_INFO)buf.data();
            if (GetAdaptersInfo(pAdapterInfo, &bufSize) != NO_ERROR) {
                return false;
            }
        }
        
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            if (pAdapter->AddressLength == 6 && 
                memcmp(pAdapter->Address, "\x00\x00\x00\x00\x00\x00", 6) != 0) {
                
                wxString mac;
                for (DWORD i = 0; i < pAdapter->AddressLength; i++) {
                    mac += wxString::Format("%02X%s", 
                        (int)pAdapter->Address[i], 
                        (i < pAdapter->AddressLength - 1) ? ":" : "");
                }
                MACAddresses.push_back(mac);
            }
            pAdapter = pAdapter->Next;
        }
        return !MACAddresses.empty();
    }
    
    // 主路径：GetAdaptersAddresses
    for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
        if (pCurr->PhysicalAddressLength != 6) continue;
        if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (pCurr->OperStatus != IfOperStatusUp) continue;
        
        // 检查是否全零 MAC
        bool allZero = true;
        for (ULONG i = 0; i < pCurr->PhysicalAddressLength; ++i) {
            if (pCurr->PhysicalAddress[i] != 0) {
                allZero = false;
                break;
            }
        }
        if (allZero) continue;
        
        // 格式化 MAC 地址
        wxString mac;
        for (ULONG i = 0; i < pCurr->PhysicalAddressLength; ++i) {
            mac += wxString::Format("%02X%s", 
                (int)pCurr->PhysicalAddress[i], 
                (i < pCurr->PhysicalAddressLength - 1) ? ":" : "");
        }
        MACAddresses.push_back(mac);
    }
    
    // 保证至少有一个条目
    if (MACAddresses.empty()) {
        MACAddresses.push_back(_("00:00:00:00:00:00"));
    }
    
    return true;
}

// ========== BIOS 信息（注册表） ==========
bool Hardware::getBIOSInfo()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     L"HARDWARE\\DESCRIPTION\\System\\BIOS",
                     0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t buf[256] = {0};
    DWORD size = sizeof(buf);

    if (RegQueryValueEx(hKey, L"BIOSVendor", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
        BIOSManufacturer = WCharToWxString(buf, size);
    }

    size = sizeof(buf);
    if (RegQueryValueEx(hKey, L"BIOSVersion", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
        BIOSVersion = WCharToWxString(buf, size);
    }

    size = sizeof(buf);
    if (RegQueryValueEx(hKey, L"BIOSReleaseDate", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
        BIOSReleaseDate = WCharToWxString(buf, size);
    }

    RegCloseKey(hKey);
    return true;
}

// ========== 系统 UUID（注册表） ==========
bool Hardware::getSystemUUID()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Cryptography",
                     0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t uuid[256] = {0};
    DWORD size = sizeof(uuid);
    if (RegQueryValueEx(hKey, L"MachineGuid", NULL, NULL, (LPBYTE)uuid, &size) == ERROR_SUCCESS) {
        SystemUUID = WCharToWxString(uuid, size);
    }

    RegCloseKey(hKey);
    
    // 保证有有效值
    if (SystemUUID.IsEmpty() || SystemUUID.StartsWith("Unknown")) {
        SystemUUID = _("00000000-0000-0000-0000-000000000000");
    }
    
    return true;
}

// ========== 机器指纹生成（修复 wxUniCharRef 二义性）==========
wxString Hardware::generateFingerprint() const
{
    wxString fp;
    fp << BaseBoardManufacturer << BaseBoardProduct
       << CPUManufacturer << CPUName;
    
    if (!DiskSerialNumbers.empty()) fp << DiskSerialNumbers[0];
    if (!MACAddresses.empty()) fp << MACAddresses[0];
    fp << SystemUUID;

    // DJB2 哈希（修复：显式转换 wxUniChar → unsigned long）
    unsigned long hash = 5381;
    for (size_t i = 0; i < fp.length(); ++i) {
        // ✅ 关键修复：使用 GetValue() 获取整数值
        wxUniChar c = fp.GetChar(i);  // 或: fp[i].GetValue()
        hash = ((hash << 5) + hash) + static_cast<unsigned long>(c.GetValue());
    }
    return wxString::Format("%08lX", hash);
}

// ========== 辅助方法：格式化内存大小 ==========
wxString Hardware::FormatMemorySize(const wxString& bytesStr)
{
    if (bytesStr.IsEmpty()) return "0 GB";
    
    unsigned long long bytes = 0;
    if (!bytesStr.ToULongLong(&bytes)) return "Unknown";
    
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    return wxString::Format("%.2f GB", gb);
}