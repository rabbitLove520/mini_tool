#ifndef HARDWARE_H
#define HARDWARE_H

#include <wx/wx.h>
#include <windows.h>
#include <vector>
#include <string>

// MinGW 不支持 #pragma comment，需在链接时手动指定库：
//   -ladvapi32 -liphlpapi -lole32 -loleaut32 -luuid

// ========== 硬件采集类 ==========
class Hardware
{
public:
    // ===== 硬件信息字段 =====
    // 主板
    wxString BaseBoardManufacturer;  // 主板制造商
    wxString BaseBoardProduct;       // 主板型号
    
    // CPU
    wxString CPUManufacturer;        // CPU厂商 (GenuineIntel/AMD)
    wxString CPUName;                // CPU型号字符串
    long CPUMaxClockSpeed;           // CPU主频 (MHz)
    
    // 内存
    wxString TotalPhysicalMemory;    // 总物理内存 (bytes)
    wxString MemoryType;             // 内存类型 (e.g., "DDR4")
    wxString MemorySpeed;            // 内存频率 (MHz)
    
    // 硬盘
    std::vector<wxString> DiskModels;          // 硬盘型号列表
    std::vector<wxString> DiskSerialNumbers;   // 硬盘序列号列表
    
    // 网卡
    std::vector<wxString> MACAddresses;        // MAC地址列表
    
    // BIOS
    wxString BIOSManufacturer;       // BIOS制造商
    wxString BIOSVersion;            // BIOS版本
    wxString BIOSReleaseDate;        // BIOS发布日期
    
    // 系统
    wxString SystemUUID;             // 系统UUID (机器唯一标识)
    wxString MachineFingerprint;     // 生成的机器指纹（用于授权绑定）
    
    // ===== 接口方法 =====
    int GetInfo();  // 主采集入口，返回0表示成功
    
    // 辅助方法：格式化内存大小（bytes → GB）
    static wxString FormatMemorySize(const wxString& bytesStr);
    
private:
    // ===== CPUID (MinGW 兼容) =====
    #if defined(__GNUC__) || defined(__MINGW32__)
        void cpuid(unsigned int CPUInfo[4], unsigned int InfoType);
    #else
        #include <intrin.h>
        void cpuid(unsigned int CPUInfo[4], unsigned int InfoType) {
            __cpuid((int*)CPUInfo, (int)InfoType);
        }
    #endif
    
    std::string getCpuVendor();
    std::string getCpuName();
    long getCPUClockSpeed();
    
    // ===== 硬件采集模块 =====
    bool getBaseBoardInfo();   // 主板（注册表）
    void getCPUInfo();         // CPU（CPUID + 注册表）
    bool getMemoryInfo();      // 内存（注册表 + 备用方案）
    bool getDiskInfo();        // 硬盘（注册表 + IOCTL 备用）
    bool getNetworkInfo();     // 网卡（GetAdaptersAddresses）
    bool getBIOSInfo();        // BIOS（注册表）
    bool getSystemUUID();      // 系统UUID（注册表）
    
    wxString generateFingerprint() const;  // 生成机器指纹
    
    // ===== 工具方法 =====
    static wxString WCharToWxString(const wchar_t* wstr, DWORD size = 0);
};

#endif // HARDWARE_H