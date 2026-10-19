// DiskForge.cpp - Main application entry point
#include "pch.h"
#include "DiskForge.h"
#include "DiskForgeDoc.h"
#include "MainFrm.h"
#include "BrowserTreeView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CDiskForgeApp theApp;

BEGIN_MESSAGE_MAP(CDiskForgeApp, CWinAppEx)
    ON_COMMAND(ID_APP_ABOUT, &CDiskForgeApp::OnAppAbout)
END_MESSAGE_MAP()

CDiskForgeApp::CDiskForgeApp() noexcept
{
    m_bHiColorIcons = TRUE;
    m_nAppLook = 0;
    SetAppID(_T("DiskForge.AppID.NoVersion"));
}

BOOL CDiskForgeApp::InitInstance()
{
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinAppEx::InitInstance();

    if (!AfxOleInit())
        return FALSE;
    AfxEnableControlContainer();
    EnableTaskbarInteraction(FALSE);

    // Window layout, ribbon customisation and the chosen look persist here.
    SetRegistryKey(_T("DiskForge"));
    // Own section for this layout, so docking state saved by earlier builds
    // (different panes and IDs) is never applied to it.
    SetRegistryBase(_T("Workspace.SystemBrowser"));

    InitContextMenuManager();
    InitShellManager();
    InitKeyboardManager();
    InitTooltipManager();
    CMFCToolTipInfo ttParams;
    ttParams.m_bVislManagerTheme = TRUE;
    GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL, RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

    CSingleDocTemplate* pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CDiskForgeDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CBrowserTreeView));
    if (!pDocTemplate)
        return FALSE;
    AddDocTemplate(pDocTemplate);

    CCommandLineInfo cmdInfo;
    cmdInfo.m_nShellCommand = CCommandLineInfo::FileNew;
    if (!ProcessShellCommand(cmdInfo))
        return FALSE;

    m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
    m_pMainWnd->UpdateWindow();

    // "DiskForge.exe <image file>" opens a raw disk image in the System Browser.
    if (__argc > 1 && GetFileAttributes(__wargv[1]) != INVALID_FILE_ATTRIBUTES)
    {
        CFrameWnd* frame = DYNAMIC_DOWNCAST(CFrameWnd, m_pMainWnd);
        CDiskForgeDoc* doc = frame ? DYNAMIC_DOWNCAST(CDiskForgeDoc, frame->GetActiveDocument()) : nullptr;
        CString err;
        if (doc && !doc->AddImage(__wargv[1], err))
            AfxMessageBox(_T("Could not open the image:\r\n") + err, MB_ICONERROR);
    }
    return TRUE;
}

int CDiskForgeApp::ExitInstance()
{
    AfxOleTerm(FALSE);
    return CWinAppEx::ExitInstance();
}

void CDiskForgeApp::PreLoadState()
{
    GetContextMenuManager()->AddMenu(_T("Browser"), IDR_POPUP_BROWSER);
}

// ---------------------------------------------------------------------------
// About box

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}
};

void CDiskForgeApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}
