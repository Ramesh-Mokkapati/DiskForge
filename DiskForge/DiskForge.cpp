// DiskForge.cpp - Main application entry point
#include "pch.h"
#include "DiskForge.h"
#include "DiskForgeDlg.h"
#include "DiskForgeDoc.h"
#include "MainFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CDiskForgeApp theApp;

BEGIN_MESSAGE_MAP(CDiskForgeApp, CWinApp)
    ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

CDiskForgeApp::CDiskForgeApp()
{
    SetAppID(_T("DiskForge.AppID.NoVersion"));
}

BOOL CDiskForgeApp::InitInstance()
{
    CWinApp::InitInstance();

    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    SetRegistryKey(_T("DiskForge"));

    CSingleDocTemplate* pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CDiskForgeDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CDiskForgeDlg));
    if (!pDocTemplate) return FALSE;
    AddDocTemplate(pDocTemplate);

    CCommandLineInfo cmdInfo;
    cmdInfo.m_nShellCommand = CCommandLineInfo::FileNew;
    if (!ProcessShellCommand(cmdInfo)) return FALSE;

    m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
    m_pMainWnd->UpdateWindow();
    return TRUE;
}

int CDiskForgeApp::ExitInstance()
{
    return CWinApp::ExitInstance();
}
