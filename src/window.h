#ifndef WINDOW_H
#define WINDOW_H

#include <wx/wx.h>
#include <wx/thread.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <vector>

struct HardwareData
{
    wxString BaseBoardManufacturer, BaseBoardProduct;
    wxString CPUManufacturer, CPUName;
    long CPUMaxClockSpeed;
    wxString TotalPhysicalMemory, MemoryType, MemorySpeed;
    std::vector<wxString> DiskModels, DiskSerialNumbers;
    std::vector<wxString> MACAddresses;
    wxString BIOSManufacturer, BIOSVersion, BIOSReleaseDate;
    wxString SystemUUID, MachineFingerprint;
    wxDateTime CollectionTime;
    
    HardwareData() : CPUMaxClockSpeed(0) {}
    HardwareData(const HardwareData& other) { *this = other; }
    HardwareData& operator=(const HardwareData& other) {
        BaseBoardManufacturer = other.BaseBoardManufacturer;
        BaseBoardProduct = other.BaseBoardProduct;
        CPUManufacturer = other.CPUManufacturer;
        CPUName = other.CPUName;
        CPUMaxClockSpeed = other.CPUMaxClockSpeed;
        TotalPhysicalMemory = other.TotalPhysicalMemory;
        MemoryType = other.MemoryType;
        MemorySpeed = other.MemorySpeed;
        DiskModels = other.DiskModels;
        DiskSerialNumbers = other.DiskSerialNumbers;
        MACAddresses = other.MACAddresses;
        BIOSManufacturer = other.BIOSManufacturer;
        BIOSVersion = other.BIOSVersion;
        BIOSReleaseDate = other.BIOSReleaseDate;
        SystemUUID = other.SystemUUID;
        MachineFingerprint = other.MachineFingerprint;
        CollectionTime = other.CollectionTime;
        return *this;
    }
};

class HardwareCollectorThread : public wxThread
{
public:
    HardwareCollectorThread(wxEvtHandler* eventHandler);
    virtual ~HardwareCollectorThread() {}
protected:
    virtual ExitCode Entry() override;
private:
    wxEvtHandler* m_eventHandler;
};

class MainWindow : public wxFrame
{
public:
    MainWindow(const wxString& title);
    ~MainWindow();
    
private:
    // UI 控件指针
    wxStaticText* m_fingerprintText;
    wxStaticText* m_boardManufacturerText;
    wxStaticText* m_boardProductText;
    wxStaticText* m_cpuInfoText;
    wxStaticText* m_memInfoText;
    wxStaticText* m_biosInfoText;
    wxStaticText* m_uuidText;
    
    wxListCtrl* m_diskList;
    wxListCtrl* m_netList;
    wxStaticText* m_statusLabel;
    wxGauge* m_progress;
    
    HardwareData m_hardwareData;
    
    // 事件处理器
    void OnHardwareCollected(wxThreadEvent& event);
    void OnRefresh(wxCommandEvent& event);
    void OnCopyAll(wxCommandEvent& event);
    void OnCopyFingerprint(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    
    void StartHardwareCollection();
    void PopulateUI(const HardwareData& data);
    wxString GenerateTextReport(const HardwareData& data) const;
    
    wxDECLARE_EVENT_TABLE();
};

#endif // WINDOW_H