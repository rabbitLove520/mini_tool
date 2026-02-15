#include "window.h"
#include "hardware.h"
#include <wx/artprov.h>
#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/filefn.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/aboutdlg.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

// ========== 硬件采集线程实现 ==========
HardwareCollectorThread::HardwareCollectorThread(wxEvtHandler* eventHandler)
    : wxThread(wxTHREAD_DETACHED), m_eventHandler(eventHandler)
{
}

wxThread::ExitCode HardwareCollectorThread::Entry()
{
    Hardware hw;
    hw.GetInfo();
    
    HardwareData data;
    data.BaseBoardManufacturer = hw.BaseBoardManufacturer;
    data.BaseBoardProduct = hw.BaseBoardProduct;
    data.CPUManufacturer = hw.CPUManufacturer;
    data.CPUName = hw.CPUName;
    data.CPUMaxClockSpeed = hw.CPUMaxClockSpeed;
    data.TotalPhysicalMemory = hw.TotalPhysicalMemory;
    data.MemoryType = hw.MemoryType;
    data.MemorySpeed = hw.MemorySpeed;
    data.DiskModels = hw.DiskModels;
    data.DiskSerialNumbers = hw.DiskSerialNumbers;
    data.MACAddresses = hw.MACAddresses;
    data.BIOSManufacturer = hw.BIOSManufacturer;
    data.BIOSVersion = hw.BIOSVersion;
    data.BIOSReleaseDate = hw.BIOSReleaseDate;
    data.SystemUUID = hw.SystemUUID;
    data.MachineFingerprint = hw.MachineFingerprint;
    data.CollectionTime = wxDateTime::Now();
    
    wxThreadEvent* evt = new wxThreadEvent(wxEVT_THREAD, wxID_ANY);
    evt->SetPayload<HardwareData>(data);
    wxQueueEvent(m_eventHandler, evt);
    
    return (wxThread::ExitCode)0;
}

// ========== 事件表 ==========
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    EVT_THREAD(wxID_ANY, MainWindow::OnHardwareCollected)
    EVT_MENU(wxID_REFRESH, MainWindow::OnRefresh)
    EVT_MENU(wxID_COPY, MainWindow::OnCopyAll)
    EVT_MENU(wxID_SAVE, MainWindow::OnExport)
    EVT_MENU(wxID_EXIT, MainWindow::OnExit)
    EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
    EVT_BUTTON(wxID_ANY, MainWindow::OnCopyFingerprint)
wxEND_EVENT_TABLE()

// ========== 辅助函数 ==========
static bool wxStringToULL(const wxString& str, unsigned long long* out)
{
    if (str.IsEmpty()) return false;
    if (str.ToULongLong(out)) return true;
    wxString clean = str;
    clean.Replace(wxT(","), wxT(""));
    clean.Replace(wxT(" "), wxT(""));
    return clean.ToULongLong(out);
}

// ========== 主窗口实现（标签文字放大，层次清晰）==========
MainWindow::MainWindow(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(800, 560)),
      m_fingerprintText(nullptr),
      m_boardManufacturerText(nullptr),
      m_boardProductText(nullptr),
      m_cpuInfoText(nullptr),
      m_memInfoText(nullptr),
      m_biosInfoText(nullptr),
      m_uuidText(nullptr),
      m_diskList(nullptr),
      m_netList(nullptr),
      m_statusLabel(nullptr),
      m_progress(nullptr)
{
    // 菜单栏
    wxMenu* menuFile = new wxMenu;
    menuFile->Append(wxID_SAVE, wxT("&导出...\tCtrl+S"));
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT, wxT("退出\tAlt+F4"));
    
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT, wxT("关于"));
    
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, wxT("&文件"));
    menuBar->Append(menuHelp, wxT("&帮助"));
    SetMenuBar(menuBar);
    
    // 工具栏
    wxToolBar* toolBar = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT | wxTB_NODIVIDER);
    toolBar->AddTool(wxID_REFRESH, wxT("🔄 刷新"), wxArtProvider::GetBitmap(wxART_REDO, wxART_TOOLBAR), wxT("刷新硬件信息"));
    toolBar->AddTool(wxID_COPY, wxT("📋 复制"), wxArtProvider::GetBitmap(wxART_COPY, wxART_TOOLBAR), wxT("复制全部信息"));
    toolBar->AddTool(wxID_SAVE, wxT("💾 导出"), wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR), wxT("导出报告"));
    toolBar->Realize();
    
    // 状态栏
    CreateStatusBar(2);
    SetStatusText(wxT("✓ 就绪"), 0);
    SetStatusText(wxT("v1.2"), 1);
    
    // 主布局
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // === 顶部：标题 + 指纹 ===
    wxPanel* topPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 50));
    topPanel->SetBackgroundColour(wxColour(245, 245, 245));
    
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxStaticText* titleText = new wxStaticText(topPanel, wxID_ANY, wxT("Hardware Inspector"));
    titleText->SetFont(titleText->GetFont().Bold().Larger().Larger());  // 标题更大更醒目
    topSizer->Add(titleText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 15);
    
    wxStaticText* fpLabel = new wxStaticText(topPanel, wxID_ANY, wxT("机器指纹:"));
    fpLabel->SetFont(fpLabel->GetFont().Bold());  // 指纹标签加粗
    fpLabel->SetForegroundColour(wxColour(90, 90, 90));
    m_fingerprintText = new wxStaticText(topPanel, wxID_ANY, wxT("N/A"));
    m_fingerprintText->SetForegroundColour(*wxBLUE);
    m_fingerprintText->SetFont(m_fingerprintText->GetFont().Bold().Larger());  // 指纹值更大
    
    wxButton* copyBtn = new wxButton(topPanel, wxID_ANY, wxT("📋 复制"), wxDefaultPosition, wxSize(70, -1));
    
    topSizer->AddStretchSpacer();
    topSizer->Add(fpLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    topSizer->Add(m_fingerprintText, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    topSizer->Add(copyBtn, 0, wxALIGN_CENTER_VERTICAL);
    
    topPanel->SetSizer(topSizer);
    mainSizer->Add(topPanel, 0, wxEXPAND | wxBOTTOM, 8);
    
    // === 信息区域：主板拆分为两行，标签放大 ===
    wxPanel* infoPanel = new wxPanel(this, wxID_ANY);
    wxFlexGridSizer* infoSizer = new wxFlexGridSizer(2, 15, 10);  // 行距微调至10，更宽松
    infoSizer->AddGrowableCol(1, 1);
    
    auto AddInfoRow = [&](const wxString& label, wxStaticText*& valueCtrl) {
        // ✅ 标签文字放大一级 + 灰色（更清晰易读）
        wxStaticText* labelCtrl = new wxStaticText(infoPanel, wxID_ANY, label);
        labelCtrl->SetFont(labelCtrl->GetFont().Larger());  // 放大标签
        labelCtrl->SetForegroundColour(wxColour(80, 80, 80));  // 深灰色，更柔和
        
        valueCtrl = new wxStaticText(infoPanel, wxID_ANY, wxT("N/A"));
        valueCtrl->SetFont(valueCtrl->GetFont().Bold().Larger());  // 值文字也稍大 + 粗体
        
        infoSizer->Add(labelCtrl, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        infoSizer->Add(valueCtrl, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    };
    
    // ✅ 主板拆分为两行独立显示（标签已放大）
    AddInfoRow(wxT("主板制造商:"), m_boardManufacturerText);
    AddInfoRow(wxT("主板型号:"), m_boardProductText);
    
    AddInfoRow(wxT("CPU 信息:"), m_cpuInfoText);
    AddInfoRow(wxT("内存信息:"), m_memInfoText);
    AddInfoRow(wxT("BIOS 信息:"), m_biosInfoText);
    AddInfoRow(wxT("系统 UUID:"), m_uuidText);
    
    infoPanel->SetSizer(infoSizer);
    mainSizer->Add(infoPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    
    // === 硬盘列表 ===
    wxStaticText* diskLabel = new wxStaticText(this, wxID_ANY, wxT("🗄️ 硬盘信息"));
    diskLabel->SetFont(diskLabel->GetFont().Bold().Larger());  // 区域标题放大加粗
    mainSizer->Add(diskLabel, 0, wxLEFT | wxTOP, 8);
    
    m_diskList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 100),
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    m_diskList->InsertColumn(0, wxT("型号"), wxLIST_FORMAT_LEFT, 380);
    m_diskList->InsertColumn(1, wxT("序列号"), wxLIST_FORMAT_LEFT, 180);
    mainSizer->Add(m_diskList, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
    
    // === 网卡列表 ===
    wxStaticText* netLabel = new wxStaticText(this, wxID_ANY, wxT("🌐 网络适配器"));
    netLabel->SetFont(netLabel->GetFont().Bold().Larger());  // 区域标题放大加粗
    mainSizer->Add(netLabel, 0, wxLEFT | wxTOP, 8);
    
    m_netList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 80),
                               wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    m_netList->InsertColumn(0, wxT("MAC 地址"), wxLIST_FORMAT_LEFT, 200);
    m_netList->InsertColumn(1, wxT("状态"), wxLIST_FORMAT_LEFT, 100);
    mainSizer->Add(m_netList, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);
    
    // === 底部状态栏 ===
    wxPanel* statusPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 28));  // 稍高
    statusPanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    
    wxBoxSizer* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    m_statusLabel = new wxStaticText(statusPanel, wxID_ANY, wxT("✓ 就绪"));
    m_statusLabel->SetFont(m_statusLabel->GetFont().Larger());  // 状态文字稍大
    m_statusLabel->SetForegroundColour(wxColour(70, 70, 70));
    m_progress = new wxGauge(statusPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(110, 16), wxGA_SMOOTH);
    m_progress->Hide();
    
    statusSizer->Add(m_statusLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);
    statusSizer->AddStretchSpacer();
    statusSizer->Add(m_progress, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    statusPanel->SetSizer(statusSizer);
    mainSizer->Add(statusPanel, 0, wxEXPAND);
    
    SetSizerAndFit(mainSizer);
    
    // 事件绑定
    Bind(wxEVT_BUTTON, &MainWindow::OnCopyFingerprint, this, copyBtn->GetId());
    
    // 启动采集
    StartHardwareCollection();
    
    Centre();
    Show();
}

MainWindow::~MainWindow() = default;

void MainWindow::StartHardwareCollection()
{
    m_statusLabel->SetLabel(wxT("🔄 采集硬件信息..."));
    m_progress->Show();
    m_progress->Pulse();
    Layout();
    
    HardwareCollectorThread* thread = new HardwareCollectorThread(this);
    if (thread->Create() != wxTHREAD_NO_ERROR) {
        m_statusLabel->SetLabel(wxT("❌ 线程创建失败"));
        m_progress->Hide();
        Layout();
        delete thread;
        
        // 降级：1秒后显示空界面
        wxTimer* timer = new wxTimer(this, wxID_ANY);
        Bind(wxEVT_TIMER, [this, timer](wxTimerEvent&) {
            PopulateUI(HardwareData());
            timer->Stop();
            delete timer;
        }, wxID_ANY);
        timer->Start(1000, wxTIMER_ONE_SHOT);
    } else {
        thread->Run();
    }
}

void MainWindow::OnHardwareCollected(wxThreadEvent& event)
{
    HardwareData data = event.GetPayload<HardwareData>();
    PopulateUI(data);
    m_hardwareData = data;
    
    m_statusLabel->SetLabel(wxString::Format(wxT("✓ 完成 %s"), data.CollectionTime.FormatTime().Mid(0, 8)));
    m_progress->Hide();
    Layout();
}

void MainWindow::PopulateUI(const HardwareData& data)
{
    // 机器指纹
    m_fingerprintText->SetLabel(data.MachineFingerprint.IsEmpty() ? wxT("N/A") : data.MachineFingerprint);
    
    // ✅ 主板拆分显示
    m_boardManufacturerText->SetLabel(
        data.BaseBoardManufacturer.IsEmpty() || data.BaseBoardManufacturer.Contains(wxT("Unknown")) 
            ? wxT("未知") 
            : data.BaseBoardManufacturer
    );
    
    m_boardProductText->SetLabel(
        data.BaseBoardProduct.IsEmpty() || data.BaseBoardProduct.Contains(wxT("Unknown")) 
            ? wxT("未知") 
            : data.BaseBoardProduct
    );
    
    // CPU 信息
    wxString cpuInfo = data.CPUName;
    if (data.CPUMaxClockSpeed > 0) {
        cpuInfo += wxString::Format(wxT(" @ %.2f GHz"), data.CPUMaxClockSpeed / 1000.0);
    }
    m_cpuInfoText->SetLabel(cpuInfo.IsEmpty() ? wxT("未知") : cpuInfo);
    
    // 内存信息
    unsigned long long bytes = 0;
    wxString memInfo = wxT("未知");
    if (!data.TotalPhysicalMemory.IsEmpty() && wxStringToULL(data.TotalPhysicalMemory, &bytes) && bytes > 0) {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        memInfo = wxString::Format(wxT("%.2f GB"), gb);
        if (!data.MemoryType.IsEmpty() && !data.MemoryType.Contains(wxT("Unknown"))) {
            memInfo += wxT(" (") + data.MemoryType + wxT(")");
        }
    }
    m_memInfoText->SetLabel(memInfo);
    
    // BIOS 信息
    wxString biosInfo = data.BIOSManufacturer;
    if (!data.BIOSVersion.IsEmpty() && !data.BIOSVersion.Contains(wxT("Unknown"))) {
        if (!biosInfo.IsEmpty()) biosInfo += wxT(" v");
        biosInfo += data.BIOSVersion;
    }
    m_biosInfoText->SetLabel(biosInfo.IsEmpty() ? wxT("未知") : biosInfo);
    
    // 系统 UUID
    m_uuidText->SetLabel(data.SystemUUID.IsEmpty() ? wxT("未知") : data.SystemUUID.Left(36));
    
    // 硬盘列表
    m_diskList->DeleteAllItems();
    size_t count = std::min(data.DiskModels.size(), data.DiskSerialNumbers.size());
    for (size_t i = 0; i < count; ++i) {
        long idx = m_diskList->InsertItem(i, data.DiskModels[i]);
        m_diskList->SetItem(idx, 1, data.DiskSerialNumbers[i]);
    }
    if (count == 0) {
        long idx = m_diskList->InsertItem(0, wxT("未检测到硬盘"));
        m_diskList->SetItem(idx, 1, wxT("N/A"));
    }
    
    // 网卡列表
    m_netList->DeleteAllItems();
    for (size_t i = 0; i < data.MACAddresses.size(); ++i) {
        long idx = m_netList->InsertItem(i, data.MACAddresses[i]);
        m_netList->SetItem(idx, 1, i == 0 ? wxT("✓ 活动") : wxT("– 备用"));
    }
    if (data.MACAddresses.empty()) {
        long idx = m_netList->InsertItem(0, wxT("未检测到网卡"));
        m_netList->SetItem(idx, 1, wxT("N/A"));
    }
}

void MainWindow::OnRefresh(wxCommandEvent& event)
{
    StartHardwareCollection();
}

void MainWindow::OnCopyAll(wxCommandEvent& event)
{
    if (m_hardwareData.MachineFingerprint.IsEmpty()) {
        wxMessageBox(wxT("请先完成硬件信息采集"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    
    wxString report = GenerateTextReport(m_hardwareData);
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(report));
        wxTheClipboard->Close();
        wxMessageBox(wxT("✓ 已复制到剪贴板"), wxT("成功"), wxOK | wxICON_INFORMATION, this);
    }
}

void MainWindow::OnCopyFingerprint(wxCommandEvent& event)
{
    if (m_hardwareData.MachineFingerprint.IsEmpty()) {
        wxMessageBox(wxT("请先完成硬件信息采集"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_hardwareData.MachineFingerprint));
        wxTheClipboard->Close();
        m_statusLabel->SetLabel(wxT("✓ 已复制指纹"));
    }
}

void MainWindow::OnExport(wxCommandEvent& event)
{
    if (m_hardwareData.MachineFingerprint.IsEmpty()) {
        wxMessageBox(wxT("请先完成硬件信息采集"), wxT("提示"), wxOK | wxICON_INFORMATION, this);
        return;
    }
    
    wxFileDialog saveDlg(this, wxT("导出报告"), "", wxT("hardware_report.txt"),
                        wxT("文本文件 (*.txt)|*.txt"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    
    if (saveDlg.ShowModal() == wxID_CANCEL) return;
    
    wxString content = GenerateTextReport(m_hardwareData);
    wxFileOutputStream output(saveDlg.GetPath());
    if (output.IsOk()) {
        wxTextOutputStream textOut(output, wxEOL_NATIVE);
        textOut.WriteString(content);
        wxMessageBox(wxString::Format(wxT("✓ 已保存至:\n%s"), saveDlg.GetFilename()), 
                    wxT("成功"), wxOK | wxICON_INFORMATION, this);
    }
}

void MainWindow::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MainWindow::OnAbout(wxCommandEvent& event)
{
    wxAboutDialogInfo info;
    info.SetName(wxT("Hardware Inspector"));
    info.SetVersion(wxT("1.2"));
    info.SetDescription(wxT("专业的硬件信息工具"));
    info.SetCopyright(wxT("(C) 2026 MiniTool Dev Team"));
    wxAboutBox(info);
}

wxString MainWindow::GenerateTextReport(const HardwareData& data) const
{
    wxString report;
    report << wxT("Hardware Inspection Report\n");
    report << wxT("===========================\n");
    report << wxString::Format(wxT("时间: %s\n"), data.CollectionTime.FormatISOCombined(' '));
    report << wxString::Format(wxT("系统: %s\n\n"), wxGetOsDescription());
    
    // ✅ 报告中也拆分主板信息
    report << wxString::Format(wxT("主板制造商: %s\n"), 
        data.BaseBoardManufacturer.IsEmpty() || data.BaseBoardManufacturer.Contains(wxT("Unknown"))
            ? wxT("N/A") 
            : data.BaseBoardManufacturer);
    
    report << wxString::Format(wxT("主板型号: %s\n"), 
        data.BaseBoardProduct.IsEmpty() || data.BaseBoardProduct.Contains(wxT("Unknown"))
            ? wxT("N/A") 
            : data.BaseBoardProduct);
    
    report << wxString::Format(wxT("CPU: %s @ %.2f GHz\n"), 
        data.CPUName.IsEmpty() ? wxT("N/A") : data.CPUName,
        data.CPUMaxClockSpeed > 0 ? data.CPUMaxClockSpeed / 1000.0 : 0.0);
    
    unsigned long long bytes = 0;
    if (!data.TotalPhysicalMemory.IsEmpty() && wxStringToULL(data.TotalPhysicalMemory, &bytes) && bytes > 0) {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        report << wxString::Format(wxT("内存: %.2f GB (%s)\n"), gb,
            data.MemoryType.IsEmpty() ? wxT("N/A") : data.MemoryType);
    }
    
    report << wxString::Format(wxT("BIOS: %s v%s\n"),
        data.BIOSManufacturer.IsEmpty() ? wxT("N/A") : data.BIOSManufacturer,
        data.BIOSVersion.IsEmpty() ? wxT("N/A") : data.BIOSVersion);
    
    report << wxString::Format(wxT("\n系统 UUID: %s\n"), 
        data.SystemUUID.IsEmpty() ? wxT("N/A") : data.SystemUUID);
    
    report << wxString::Format(wxT("机器指纹: %s\n"), 
        data.MachineFingerprint.IsEmpty() ? wxT("N/A") : data.MachineFingerprint);
    
    report << wxT("\n--- Hardware Inspector v1.2 ---");
    return report;
}