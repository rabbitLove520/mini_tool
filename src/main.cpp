/**
 * main.cpp - Hardware Inspector 应用程序入口点
 * 
 * 项目结构:
 *   ├── main.cpp    : 应用初始化与入口
 *   ├── window.h/cpp: UI界面逻辑
 *   └── hardware.h/cpp: 硬件采集业务逻辑
 */

#include "window.h"
#include "hardware.h"
#include <wx/wx.h>
#include <wx/image.h>  // 支持常见图片格式

// ========== 应用程序类 ==========
class MiniToolApp : public wxApp
{
public:
    virtual bool OnInit() override;
    virtual int OnExit() override;
    
private:
    bool InitResources();
};

// ========== 应用初始化 ==========
bool MiniToolApp::OnInit()
{
    // 1. 初始化国际化（支持多语言）
    wxLocale locale;
    locale.Init(wxLANGUAGE_DEFAULT);

    wxLog::SetActiveTarget(new wxLogStderr());
    
    // 2. 初始化图片处理器（支持PNG/JPEG等）
    wxImage::AddHandler(new wxPNGHandler());
    wxImage::AddHandler(new wxJPEGHandler());
    
    // 3. 初始化硬件采集模块（可选预热）
    wxLogMessage("Hardware Inspector 启动中...");
    
    // 4. 创建主窗口
    MainWindow* frame = new MainWindow("Hardware Inspector");
    frame->Show(true);
    
    // 5. 设置应用名称（用于系统任务栏显示）
    SetAppName("HardwareInspector");
    
    wxLogMessage("应用启动成功");
    return true;
}

// ========== 应用退出清理 ==========
int MiniToolApp::OnExit()
{
    wxLogMessage("Hardware Inspector 正在退出...");
    // 此处可添加资源清理逻辑（如关闭日志文件等）
    return wxApp::OnExit();
}

// ========== 应用入口宏 ==========
wxIMPLEMENT_APP(MiniToolApp);

// ========== Windows 特定: 设置控制台窗口标题（调试用）==========
#ifdef __WXMSW__
#include <windows.h>
class AutoConsoleTitleSetter {
public:
    AutoConsoleTitleSetter() {
        // 仅在调试模式下设置控制台标题
        #ifdef _DEBUG
            SetConsoleTitle(L"Hardware Inspector - Debug Console");
        #endif
    }
} g_consoleTitleSetter;
#endif