// MainFrame.cpp - SDI main frame window
#include "pch.h"
#include "MainFrame.h"
#include "NavTreeView.h"
#include "DiskForgeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
    if (!m_wndSplitter.CreateStatic(this, 1, 2))
        return FALSE;
    if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CNavTreeView),
                                   CSize(NAV_WIDTH, 0), pContext))
        return FALSE;
    if (!m_wndSplitter.CreateView(0, 1, RUNTIME_CLASS(CDiskForgeDlg),
                                   CSize(0, 0), pContext))
        return FALSE;
    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs))
        return FALSE;
    cs.style = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE;
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    SetWindowText(_T("DiskForge  [requires Administrator privileges]"));
    SetIcon(AfxGetApp()->LoadStandardIcon(IDI_APPLICATION), TRUE);
    SetIcon(AfxGetApp()->LoadStandardIcon(IDI_APPLICATION), FALSE);

    SetWindowPos(nullptr, 100, 50, MIN_WIDTH, MIN_HEIGHT,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CFrameWnd::OnGetMinMaxInfo(lpMMI);
}
