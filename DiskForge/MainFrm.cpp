// MainFrm.cpp - Main frame (NewTest framework: ribbon, Outlook-style tools
// bar, caption bar, docking panes) hosting the System Browser.
#include "pch.h"
#include "DiskForge.h"
#include "MainFrm.h"
#include "BrowserTreeView.h"
#include "BrowserListView.h"
#include "ShellIcons.h"
#include "VolumeMap.h"
#include "PartitionDialog.h"
#include "DiskCopyDialog.h"
#include "BadSectorScanDialog.h"
#include "SmartHealthDialog.h"
#include "MbrGptConvertDialog.h"
#include "PartitionEditDialog.h"
#include "WipeDiskDialog.h"
#include "EfiBootDialog.h"
#include "LostPartitionDialog.h"
#include "FileRecoveryDialog.h"
#include <functional>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    // Ribbon button with Windows stock icons (large + small).
    CMFCRibbonButton* StockButton(UINT id, LPCTSTR text, SHSTOCKICONID icon, bool large = true)
    {
        HICON big = large ? CShellIcons::Load(icon, true) : nullptr;
        HICON smallIcon = CShellIcons::Load(icon, false);
        auto* b = new CMFCRibbonButton(id, text, big, FALSE, smallIcon, TRUE, TRUE);
        b->SetAlwaysLargeImage(large ? TRUE : FALSE);
        return b;
    }

    // A recovered file's name, made safe for the destination filesystem.
    CString SafeFileName(CString name)
    {
        static const TCHAR bad[] = _T("<>:\"/\\|?*");
        for (int i = 0; i < name.GetLength(); ++i)
            if (name[i] < 32 || _tcschr(bad, name[i]))
                name.SetAt(i, _T('_'));
        name.TrimRight(_T(". "));
        return name.IsEmpty() ? CString(_T("unnamed")) : name;
    }

    // folder\name, or folder\name (2).ext if that exists already.
    CString UniquePath(const CString& folder, const CString& name)
    {
        CString path = folder + _T("\\") + name;
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
            return path;
        int dot = name.ReverseFind(_T('.'));
        CString base = dot > 0 ? name.Left(dot) : name, ext = dot > 0 ? name.Mid(dot) : CString();
        for (int n = 2; ; ++n)
        {
            path.Format(_T("%s\\%s (%d)%s"), folder.GetString(), base.GetString(), n, ext.GetString());
            if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES)
                return path;
        }
    }
}

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
    ON_WM_CREATE()
    ON_WM_GETMINMAXINFO()
    ON_WM_SETTINGCHANGE()
    ON_COMMAND_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CMainFrame::OnApplicationLook)
    ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CMainFrame::OnUpdateApplicationLook)
    ON_COMMAND(ID_TOOLS_OPTIONS, &CMainFrame::OnOptions)

    ON_COMMAND(ID_VIEW_OUTPUTWND, &CMainFrame::OnViewOutputWindow)
    ON_UPDATE_COMMAND_UI(ID_VIEW_OUTPUTWND, &CMainFrame::OnUpdateViewOutputWindow)
    ON_COMMAND(ID_VIEW_PROPERTIESWND, &CMainFrame::OnViewPropertiesWindow)
    ON_UPDATE_COMMAND_UI(ID_VIEW_PROPERTIESWND, &CMainFrame::OnUpdateViewPropertiesWindow)
    ON_COMMAND(ID_VIEW_HEXWND, &CMainFrame::OnViewHexWindow)
    ON_UPDATE_COMMAND_UI(ID_VIEW_HEXWND, &CMainFrame::OnUpdateViewHexWindow)
    ON_COMMAND(ID_VIEW_NAVIGATION, &CMainFrame::OnViewNavigation)
    ON_UPDATE_COMMAND_UI(ID_VIEW_NAVIGATION, &CMainFrame::OnUpdateViewNavigation)
    ON_COMMAND(ID_VIEW_CAPTION_BAR, &CMainFrame::OnViewCaptionBar)
    ON_UPDATE_COMMAND_UI(ID_VIEW_CAPTION_BAR, &CMainFrame::OnUpdateViewCaptionBar)

    ON_COMMAND(ID_DISK_REFRESH, &CMainFrame::OnDiskRefresh)
    ON_COMMAND(ID_DISK_OPEN_IMAGE, &CMainFrame::OnDiskOpenImage)
    ON_COMMAND(ID_BROWSE_SHOW_DELETED, &CMainFrame::OnShowDeleted)
    ON_UPDATE_COMMAND_UI(ID_BROWSE_SHOW_DELETED, &CMainFrame::OnUpdateShowDeleted)
    ON_COMMAND(ID_BROWSE_DELETED_ONLY, &CMainFrame::OnDeletedOnly)
    ON_UPDATE_COMMAND_UI(ID_BROWSE_DELETED_ONLY, &CMainFrame::OnUpdateDeletedOnly)
    ON_COMMAND(ID_BROWSE_RECOVER, &CMainFrame::OnRecover)
    ON_UPDATE_COMMAND_UI(ID_BROWSE_RECOVER, &CMainFrame::OnUpdateRecover)
    ON_COMMAND(ID_BROWSE_SHOW_META, &CMainFrame::OnShowMeta)
    ON_UPDATE_COMMAND_UI(ID_BROWSE_SHOW_META, &CMainFrame::OnUpdateEntryCommand)
    ON_COMMAND(ID_BROWSE_SHOW_DATA, &CMainFrame::OnShowData)
    ON_UPDATE_COMMAND_UI(ID_BROWSE_SHOW_DATA, &CMainFrame::OnUpdateEntryCommand)
    ON_COMMAND(ID_BROWSE_OPEN, &CMainFrame::OnBrowseOpen)
    ON_COMMAND(ID_BROWSE_UP, &CMainFrame::OnBrowseUp)

    ON_COMMAND(ID_SECTOR_FIRST, &CMainFrame::OnSectorFirst)
    ON_COMMAND(ID_SECTOR_PREV, &CMainFrame::OnSectorPrev)
    ON_COMMAND(ID_SECTOR_NEXT, &CMainFrame::OnSectorNext)
    ON_COMMAND(ID_SECTOR_LAST, &CMainFrame::OnSectorLast)
    ON_COMMAND(ID_SECTOR_GOTO, &CMainFrame::OnSectorGoto)
    ON_COMMAND(ID_SECTOR_EDIT, &CMainFrame::OnSectorGoto)
    ON_UPDATE_COMMAND_UI_RANGE(ID_SECTOR_FIRST, ID_SECTOR_GOTO, &CMainFrame::OnUpdateSectorCommand)

    ON_COMMAND(ID_TOOL_PARTITIONS, &CMainFrame::OnToolPartitions)
    ON_COMMAND(ID_TOOL_DISKCOPY, &CMainFrame::OnToolDiskCopy)
    ON_COMMAND(ID_TOOL_BADSECTOR, &CMainFrame::OnToolBadSector)
    ON_COMMAND(ID_TOOL_SMART, &CMainFrame::OnToolSmart)
    ON_COMMAND(ID_TOOL_MBRGPT, &CMainFrame::OnToolMbrGpt)
    ON_COMMAND(ID_TOOL_PARTEDIT, &CMainFrame::OnToolPartEdit)
    ON_COMMAND(ID_TOOL_WIPE, &CMainFrame::OnToolWipe)
    ON_COMMAND(ID_TOOL_EFIBOOT, &CMainFrame::OnToolEfiBoot)
    ON_COMMAND(ID_TOOL_LOSTPART, &CMainFrame::OnToolLostPart)
    ON_COMMAND(ID_TOOL_FILERECOVERY, &CMainFrame::OnToolFileRecovery)
END_MESSAGE_MAP()

CMainFrame::CMainFrame() noexcept
{
    theApp.m_nAppLook = theApp.GetInt(_T("ApplicationLook"), ID_VIEW_APPLOOK_VS_2008);
}

// ---------------------------------------------------------------------------
// Creation

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
        return -1;

    SetWindowText(_T("DiskForge"));

    m_wndRibbonBar.Create(this);
    InitializeRibbon();
    SetMenu(nullptr); // the ribbon replaces IDR_MAINFRAME's menu

    if (!m_wndStatusBar.Create(this))
        return -1;
    CString pane1, pane2;
    VERIFY(pane1.LoadString(IDS_STATUS_PANE1));
    VERIFY(pane2.LoadString(IDS_STATUS_PANE2));
    m_wndStatusBar.AddElement(new CMFCRibbonStatusBarPane(ID_STATUSBAR_PANE1, pane1, TRUE, nullptr,
        _T("Scanning the Master File Table... 100%   (1234 live, 1234 deleted item(s) shown)")), pane1);
    m_wndStatusBar.AddExtendedElement(new CMFCRibbonStatusBarPane(ID_STATUSBAR_PANE2, pane2, TRUE, nullptr,
        _T("\\\\.\\PhysicalDrive10  LBA 12345678901")), pane2);

    // Visual Studio 2005 style docking and auto-hide
    CDockingManager::SetDockingMode(DT_SMART);
    EnableAutoHidePanes(CBRS_ALIGN_ANY);

    // The Tools bar is created at the left, so temporarily disable docking there.
    EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM | CBRS_ALIGN_RIGHT);
    if (!CreateOutlookBar(m_wndNavigationBar, ID_VIEW_NAVIGATION, 220))
        return -1;
    if (!CreateCaptionBar())
        return -1;
    EnableDocking(CBRS_ALIGN_LEFT);
    EnableAutoHidePanes(CBRS_ALIGN_RIGHT);

    if (!CreateDockingWindows())
        return -1;

    m_wndHex.EnableDocking(CBRS_ALIGN_ANY);
    m_wndOutput.EnableDocking(CBRS_ALIGN_ANY);
    m_wndProperties.EnableDocking(CBRS_ALIGN_ANY);
    DockPane(&m_wndHex);
    DockPaneLeftOf(&m_wndOutput, &m_wndHex);
    DockPane(&m_wndProperties);

    OnApplicationLook(theApp.m_nAppLook);

    Log(_T("DiskForge started. Select a disk in the System Browser and expand it: Partition Table -> ")
        _T("Partition -> Boot Record -> File Allocation -> File System."));
    return 0;
}

void CMainFrame::InitializeRibbon()
{
    // ---- Application button and its menu
    m_MainButton.SetImage(IDB_MAIN);
    m_MainButton.SetText(_T("\nf"));
    m_MainButton.SetToolTipText(_T("File"));
    m_wndRibbonBar.SetApplicationButton(&m_MainButton, CSize(45, 45));

    CMFCRibbonMainPanel* pMainPanel = m_wndRibbonBar.AddMainCategory(_T("File"), 0, 0);
    pMainPanel->Add(StockButton(ID_DISK_REFRESH, _T("&Refresh Disks"), SIID_DRIVEFIXED));
    pMainPanel->Add(StockButton(ID_DISK_OPEN_IMAGE, _T("&Open Disk Image..."), SIID_MEDIABLANKCD));
    pMainPanel->Add(StockButton(ID_TOOLS_OPTIONS, _T("&Customize..."), SIID_AUTOLIST));
    pMainPanel->Add(new CMFCRibbonSeparator(TRUE));
    pMainPanel->Add(StockButton(ID_APP_ABOUT, _T("&About DiskForge"), SIID_INFO));
    pMainPanel->AddToBottom(new CMFCRibbonMainPanelButton(ID_APP_EXIT, _T("E&xit"),
                                                          CShellIcons::Load(SIID_ERROR, false)));

    // ---- Home
    CMFCRibbonCategory* home = m_wndRibbonBar.AddCategory(_T("&Home"), 0, 0);

    CMFCRibbonPanel* disks = home->AddPanel(_T("Disks"));
    disks->Add(StockButton(ID_DISK_REFRESH, _T("Refresh Disks"), SIID_DRIVEFIXED));
    disks->Add(StockButton(ID_DISK_OPEN_IMAGE, _T("Open Image"), SIID_MEDIABLANKCD));

    CMFCRibbonPanel* browse = home->AddPanel(_T("System Browser"));
    browse->Add(StockButton(ID_BROWSE_RECOVER, _T("Recover Files"), SIID_RECYCLERFULL));
    browse->Add(new CMFCRibbonCheckBox(ID_BROWSE_SHOW_DELETED, _T("Show deleted files")));
    browse->Add(new CMFCRibbonCheckBox(ID_BROWSE_DELETED_ONLY, _T("Deleted files only")));
    browse->Add(StockButton(ID_BROWSE_UP, _T("Up one level"), SIID_FOLDER, false));
    browse->AddSeparator();
    browse->Add(StockButton(ID_BROWSE_SHOW_META, _T("Show record"), SIID_FIND, false));
    browse->Add(StockButton(ID_BROWSE_SHOW_DATA, _T("Show data"), SIID_DOCNOASSOC, false));
    browse->Add(StockButton(ID_BROWSE_OPEN, _T("Open"), SIID_FOLDEROPEN, false));

    CMFCRibbonPanel* sector = home->AddPanel(_T("Sector View"));
    CMFCRibbonEdit* lba = new CMFCRibbonEdit(ID_SECTOR_EDIT, 110, _T("LBA:"));
    lba->SetEditText(_T("0"));
    sector->Add(lba);
    sector->Add(new CMFCRibbonButton(ID_SECTOR_GOTO, _T("Go to sector")));
    CMFCRibbonButtonsGroup* nav = new CMFCRibbonButtonsGroup;
    nav->AddButton(new CMFCRibbonButton(ID_SECTOR_FIRST, _T("|<")));
    nav->AddButton(new CMFCRibbonButton(ID_SECTOR_PREV, _T(" < ")));
    nav->AddButton(new CMFCRibbonButton(ID_SECTOR_NEXT, _T(" > ")));
    nav->AddButton(new CMFCRibbonButton(ID_SECTOR_LAST, _T(">|")));
    sector->Add(nav);

    CMFCRibbonPanel* tools = home->AddPanel(_T("Disk Tools"));
    tools->Add(StockButton(ID_TOOL_PARTITIONS, _T("Partition Table"), SIID_STACK));
    tools->Add(StockButton(ID_TOOL_BADSECTOR, _T("Bad Sector Scan"), SIID_FIND));
    tools->Add(StockButton(ID_TOOL_SMART, _T("Disk Health"), SIID_INFO));
    tools->Add(StockButton(ID_TOOL_LOSTPART, _T("Lost Partitions"), SIID_DRIVEUNKNOWN));
    tools->Add(StockButton(ID_TOOL_FILERECOVERY, _T("FAT32 Recovery"), SIID_RECYCLER));

    CMFCRibbonPanel* write = home->AddPanel(_T("Modify Disks (writes)"));
    write->Add(StockButton(ID_TOOL_DISKCOPY, _T("Disk Copy"), SIID_MEDIACDBURN));
    write->Add(StockButton(ID_TOOL_PARTEDIT, _T("Partition Editor"), SIID_RENAME));
    write->Add(StockButton(ID_TOOL_MBRGPT, _T("MBR to GPT"), SIID_SHIELD));
    write->Add(StockButton(ID_TOOL_WIPE, _T("Wipe Disk"), SIID_DELETE));
    write->Add(StockButton(ID_TOOL_EFIBOOT, _T("UEFI Boot"), SIID_KEY));

    // ---- View
    CMFCRibbonCategory* view = m_wndRibbonBar.AddCategory(_T("&View"), 0, 0);
    CMFCRibbonPanel* panes = view->AddPanel(_T("Panes"));
    panes->Add(new CMFCRibbonCheckBox(ID_VIEW_OUTPUTWND, _T("Output")));
    panes->Add(new CMFCRibbonCheckBox(ID_VIEW_PROPERTIESWND, _T("Properties")));
    panes->Add(new CMFCRibbonCheckBox(ID_VIEW_HEXWND, _T("Sector View")));
    panes->Add(new CMFCRibbonCheckBox(ID_VIEW_NAVIGATION, _T("Tools Bar")));
    panes->Add(new CMFCRibbonCheckBox(ID_VIEW_CAPTION_BAR, _T("Caption Bar")));
    panes->Add(new CMFCRibbonCheckBox(ID_VIEW_STATUS_BAR, _T("Status Bar")));

    CMFCRibbonPanel* look = view->AddPanel(_T("Appearance"));
    CMFCRibbonButton* styles = StockButton((UINT)-1, _T("Application\nLook"), SIID_DESKTOPPC);
    styles->SetMenu(IDR_THEME_MENU, FALSE, TRUE);
    look->Add(styles);
    look->Add(StockButton(ID_TOOLS_OPTIONS, _T("Customize"), SIID_AUTOLIST));

    // ---- Right side of the tab row: style menu and About (NewTest layout)
    CMFCRibbonButton* tabStyle = new CMFCRibbonButton((UINT)-1, _T("Style"), -1, -1);
    tabStyle->SetMenu(IDR_THEME_MENU, FALSE, TRUE);
    m_wndRibbonBar.AddToTabs(tabStyle);
    m_wndRibbonBar.AddToTabs(new CMFCRibbonButton(ID_APP_ABOUT, _T("\na"), CShellIcons::Load(SIID_HELP, false),
                                                  FALSE, nullptr, TRUE, TRUE));

    CList<UINT, UINT> qat;
    qat.AddTail(ID_DISK_REFRESH);
    qat.AddTail(ID_BROWSE_RECOVER);
    m_wndRibbonBar.SetQuickAccessCommands(qat);
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
    // NewTest's explorer layout: tree on the left, list on the right.
    if (!m_wndSplitter.CreateStatic(this, 1, 2))
        return FALSE;
    if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CBrowserTreeView), CSize(360, 100), pContext) ||
        !m_wndSplitter.CreateView(0, 1, RUNTIME_CLASS(CBrowserListView), CSize(100, 100), pContext))
    {
        m_wndSplitter.DestroyWindow();
        return FALSE;
    }
    m_splitterReady = true;
    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWndEx::PreCreateWindow(cs))
        return FALSE;
    cs.style = WS_OVERLAPPED | WS_CAPTION | FWS_ADDTOTITLE | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
               WS_MAXIMIZE | WS_SYSMENU;
    return TRUE;
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CFrameWndEx::OnGetMinMaxInfo(lpMMI);
}

BOOL CMainFrame::CreateDockingWindows()
{
    CString title;
    VERIFY(title.LoadString(IDS_OUTPUT_WND));
    if (!m_wndOutput.Create(title, this, CRect(0, 0, 400, 200), TRUE, ID_VIEW_OUTPUTWND,
                            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI))
        return FALSE;

    VERIFY(title.LoadString(IDS_HEX_WND));
    if (!m_wndHex.Create(title, this, CRect(0, 0, 700, 260), TRUE, ID_VIEW_HEXWND,
                         WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI))
        return FALSE;

    VERIFY(title.LoadString(IDS_PROPERTIES_WND));
    if (!m_wndProperties.Create(title, this, CRect(0, 0, 320, 200), TRUE, ID_VIEW_PROPERTIESWND,
                                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI))
        return FALSE;

    SetDockingWindowIcons(theApp.m_bHiColorIcons);
    return TRUE;
}

void CMainFrame::SetDockingWindowIcons(BOOL bHiColorIcons)
{
    const int cx = ::GetSystemMetrics(SM_CXSMICON), cy = ::GetSystemMetrics(SM_CYSMICON);
    HICON hOutput = (HICON)::LoadImage(::AfxGetResourceHandle(),
        MAKEINTRESOURCE(bHiColorIcons ? IDI_OUTPUT_WND_HC : IDI_OUTPUT_WND), IMAGE_ICON, cx, cy, 0);
    m_wndOutput.SetIcon(hOutput, FALSE);
    HICON hProps = (HICON)::LoadImage(::AfxGetResourceHandle(),
        MAKEINTRESOURCE(bHiColorIcons ? IDI_PROPERTIES_WND_HC : IDI_PROPERTIES_WND), IMAGE_ICON, cx, cy, 0);
    m_wndProperties.SetIcon(hProps, FALSE);
    m_wndHex.SetIcon(CShellIcons::Load(SIID_DRIVEFIXED, false), FALSE);
}

BOOL CMainFrame::CreateOutlookBar(CMFCOutlookBar& bar, UINT uiID, int nInitialWidth)
{
    bar.SetMode2003();

    CString strTemp;
    VERIFY(strTemp.LoadString(IDS_SHORTCUTS));
    if (!bar.Create(strTemp, this, CRect(0, 0, nInitialWidth, 32000), uiID, WS_CHILD | WS_VISIBLE | CBRS_LEFT))
        return FALSE;

    CMFCOutlookBarTabCtrl* pOutlookBar = (CMFCOutlookBarTabCtrl*)bar.GetUnderlyingWindow();
    if (pOutlookBar == nullptr)
        return FALSE;
    pOutlookBar->EnableInPlaceEdit(FALSE);

    // can float, can autohide, can resize, CAN NOT CLOSE
    DWORD dwStyle = AFX_CBRS_FLOAT | AFX_CBRS_AUTOHIDE | AFX_CBRS_RESIZE;
    CRect rectDummy(0, 0, 0, 0);
    const DWORD dwTreeStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS |
                              TVS_SHOWSELALWAYS | TVS_TRACKSELECT | TVS_FULLROWSELECT;

    // Page 1: disk tools
    m_wndToolsTree.Create(dwTreeStyle & ~TVS_HASLINES, rectDummy, &bar, 1200);
    HTREEITEM ro = m_wndToolsTree.AddCommand(_T("Read-only"), 0);
    m_wndToolsTree.AddCommand(_T("Partition Table"), ID_TOOL_PARTITIONS, ro);
    m_wndToolsTree.AddCommand(_T("Bad Sector Scan"), ID_TOOL_BADSECTOR, ro);
    m_wndToolsTree.AddCommand(_T("Disk Health (S.M.A.R.T.)"), ID_TOOL_SMART, ro);
    HTREEITEM rw = m_wndToolsTree.AddCommand(_T("Modify disks (writes)"), 0);
    m_wndToolsTree.AddCommand(_T("Disk Copy / Restore Image"), ID_TOOL_DISKCOPY, rw);
    m_wndToolsTree.AddCommand(_T("Partition Editor"), ID_TOOL_PARTEDIT, rw);
    m_wndToolsTree.AddCommand(_T("MBR to GPT Conversion"), ID_TOOL_MBRGPT, rw);
    m_wndToolsTree.AddCommand(_T("Wipe Disk / Partition"), ID_TOOL_WIPE, rw);
    m_wndToolsTree.Expand(ro, TVE_EXPAND);
    m_wndToolsTree.Expand(rw, TVE_EXPAND);
    VERIFY(strTemp.LoadString(IDS_PAGE_TOOLS));
    pOutlookBar->AddControl(&m_wndToolsTree, strTemp, 0, TRUE, dwStyle);

    // Page 2: recovery
    m_wndRecoveryTree.Create(dwTreeStyle & ~(TVS_HASLINES | TVS_LINESATROOT), rectDummy, &bar, 1201);
    m_wndRecoveryTree.AddCommand(_T("Recover selected files"), ID_BROWSE_RECOVER);
    m_wndRecoveryTree.AddCommand(_T("Show / hide deleted files"), ID_BROWSE_SHOW_DELETED);
    m_wndRecoveryTree.AddCommand(_T("List deleted files only"), ID_BROWSE_DELETED_ONLY);
    m_wndRecoveryTree.AddCommand(_T("Lost Partition Search"), ID_TOOL_LOSTPART);
    m_wndRecoveryTree.AddCommand(_T("FAT32 File Recovery (classic)"), ID_TOOL_FILERECOVERY);
    VERIFY(strTemp.LoadString(IDS_PAGE_RECOVERY));
    pOutlookBar->AddControl(&m_wndRecoveryTree, strTemp, 1, TRUE, dwStyle);

    // Page 3: boot and firmware
    m_wndBootTree.Create(dwTreeStyle & ~(TVS_HASLINES | TVS_LINESATROOT), rectDummy, &bar, 1202);
    m_wndBootTree.AddCommand(_T("UEFI Boot Entries (writes)"), ID_TOOL_EFIBOOT);
    m_wndBootTree.AddCommand(_T("MBR to GPT Conversion (writes)"), ID_TOOL_MBRGPT);
    m_wndBootTree.AddCommand(_T("Partition Table"), ID_TOOL_PARTITIONS);
    VERIFY(strTemp.LoadString(IDS_PAGE_BOOT));
    pOutlookBar->AddControl(&m_wndBootTree, strTemp, 2, TRUE, dwStyle);

    bar.SetPaneStyle(bar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
    pOutlookBar->SetImageList(theApp.m_bHiColorIcons ? IDB_PAGES_HC : IDB_PAGES, 24);
    pOutlookBar->SetToolbarImageList(theApp.m_bHiColorIcons ? IDB_PAGES_SMALL_HC : IDB_PAGES_SMALL, 16);
    pOutlookBar->RecalcLayout();

    CMFCOutlookBarTabCtrl::EnableAnimation(theApp.GetInt(_T("OutlookAnimation"), TRUE));
    bar.SetButtonsFont(&afxGlobalData.fontBold);
    return TRUE;
}

BOOL CMainFrame::CreateCaptionBar()
{
    if (!m_wndCaptionBar.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, this, ID_VIEW_CAPTION_BAR, -1, TRUE))
        return FALSE;

    CString text, tip;
    VERIFY(text.LoadString(IDS_CAPTION_BUTTON));
    m_wndCaptionBar.SetButton(text, ID_DISK_REFRESH, CMFCCaptionBar::ALIGN_LEFT, FALSE);
    VERIFY(tip.LoadString(IDS_CAPTION_BUTTON_TIP));
    m_wndCaptionBar.SetButtonToolTip(tip);

    VERIFY(text.LoadString(IDS_CAPTION_TEXT));
    m_wndCaptionBar.SetText(text, CMFCCaptionBar::ALIGN_LEFT);

    m_wndCaptionBar.SetBitmap(IDB_INFO, RGB(255, 255, 255), FALSE, CMFCCaptionBar::ALIGN_LEFT);
    VERIFY(tip.LoadString(IDS_CAPTION_IMAGE_TIP));
    VERIFY(text.LoadString(IDS_CAPTION_IMAGE_TEXT));
    m_wndCaptionBar.SetImageToolTip(tip, text);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Look and panes

void CMainFrame::OnApplicationLook(UINT id)
{
    CWaitCursor wait;
    theApp.m_nAppLook = id;

    switch (theApp.m_nAppLook)
    {
    case ID_VIEW_APPLOOK_WIN_2000:
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManager));
        m_wndRibbonBar.SetWindows7Look(FALSE);
        break;
    case ID_VIEW_APPLOOK_OFF_XP:
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOfficeXP));
        m_wndRibbonBar.SetWindows7Look(FALSE);
        break;
    case ID_VIEW_APPLOOK_WIN_XP:
        CMFCVisualManagerWindows::m_b3DTabsXPTheme = TRUE;
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
        m_wndRibbonBar.SetWindows7Look(FALSE);
        break;
    case ID_VIEW_APPLOOK_OFF_2003:
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2003));
        CDockingManager::SetDockingMode(DT_SMART);
        m_wndRibbonBar.SetWindows7Look(FALSE);
        break;
    case ID_VIEW_APPLOOK_VS_2005:
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2005));
        CDockingManager::SetDockingMode(DT_SMART);
        m_wndRibbonBar.SetWindows7Look(FALSE);
        break;
    case ID_VIEW_APPLOOK_VS_2008:
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2008));
        CDockingManager::SetDockingMode(DT_SMART);
        m_wndRibbonBar.SetWindows7Look(FALSE);
        break;
    case ID_VIEW_APPLOOK_WINDOWS_7:
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows7));
        CDockingManager::SetDockingMode(DT_SMART);
        m_wndRibbonBar.SetWindows7Look(TRUE);
        break;
    default:
        switch (theApp.m_nAppLook)
        {
        case ID_VIEW_APPLOOK_OFF_2007_BLUE:
            CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_LunaBlue);
            break;
        case ID_VIEW_APPLOOK_OFF_2007_BLACK:
            CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
            break;
        case ID_VIEW_APPLOOK_OFF_2007_SILVER:
            CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Silver);
            break;
        case ID_VIEW_APPLOOK_OFF_2007_AQUA:
            CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Aqua);
            break;
        }
        CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
        CDockingManager::SetDockingMode(DT_SMART);
        m_wndRibbonBar.SetWindows7Look(FALSE);
    }

    m_wndOutput.UpdateFonts();
    RedrawWindow(nullptr, nullptr, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);
    theApp.WriteInt(_T("ApplicationLook"), theApp.m_nAppLook);
}

void CMainFrame::OnUpdateApplicationLook(CCmdUI* pCmdUI)
{
    pCmdUI->SetRadio(theApp.m_nAppLook == pCmdUI->m_nID);
}

void CMainFrame::OnOptions()
{
    CMFCRibbonCustomizeDialog dlg(this, &m_wndRibbonBar);
    dlg.DoModal();
}

void CMainFrame::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
    CFrameWndEx::OnSettingChange(uFlags, lpszSection);
    m_wndOutput.UpdateFonts();
}

void CMainFrame::OnViewOutputWindow()
{
    m_wndOutput.ShowPane(!m_wndOutput.IsVisible(), FALSE, TRUE);
}
void CMainFrame::OnUpdateViewOutputWindow(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndOutput.IsVisible());
}
void CMainFrame::OnViewPropertiesWindow()
{
    m_wndProperties.ShowPane(!m_wndProperties.IsVisible(), FALSE, TRUE);
}
void CMainFrame::OnUpdateViewPropertiesWindow(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndProperties.IsVisible());
}
void CMainFrame::OnViewHexWindow()
{
    m_wndHex.ShowPane(!m_wndHex.IsVisible(), FALSE, TRUE);
}
void CMainFrame::OnUpdateViewHexWindow(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndHex.IsVisible());
}
void CMainFrame::OnViewNavigation()
{
    m_wndNavigationBar.ShowPane(!m_wndNavigationBar.IsVisible(), FALSE, TRUE);
}
void CMainFrame::OnUpdateViewNavigation(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndNavigationBar.IsVisible());
}
void CMainFrame::OnViewCaptionBar()
{
    m_wndCaptionBar.ShowWindow(m_wndCaptionBar.IsVisible() ? SW_HIDE : SW_SHOW);
    RecalcLayout(FALSE);
}
void CMainFrame::OnUpdateViewCaptionBar(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndCaptionBar.IsVisible());
}

// ---------------------------------------------------------------------------
// Helpers

CDiskForgeDoc* CMainFrame::Doc()
{
    return DYNAMIC_DOWNCAST(CDiskForgeDoc, GetActiveDocument());
}

CBrowserTreeView* CMainFrame::GetTreeView()
{
    return m_splitterReady ? DYNAMIC_DOWNCAST(CBrowserTreeView, m_wndSplitter.GetPane(0, 0)) : nullptr;
}

CBrowserListView* CMainFrame::GetListView()
{
    return m_splitterReady ? DYNAMIC_DOWNCAST(CBrowserListView, m_wndSplitter.GetPane(0, 1)) : nullptr;
}

void CMainFrame::Log(const CString& text)
{
    m_wndOutput.AddLine(text);
}

void CMainFrame::SetStatusText(const CString& text)
{
    if (CMFCRibbonBaseElement* p = m_wndStatusBar.FindElement(ID_STATUSBAR_PANE1))
    {
        p->SetText(text);
        p->Redraw();
    }
}

void CMainFrame::ShowProperties(const CString& title, const std::vector<FieldRow>& fields)
{
    m_wndProperties.ShowFields(title, fields);
}

void CMainFrame::ShowHex(int disk, ULONGLONG lba, const CString& what)
{
    CDiskForgeDoc* doc = Doc();
    if (!doc || disk < 0 || disk >= (int)doc->m_disks.size())
        return;
    const CString& device = doc->m_disks[disk].drive.devicePath;
    if (!m_wndHex.ShowSector(device, lba, what))
    {
        Log(_T("Sector View: ") + m_wndHex.LastError());
        return;
    }
    UpdateSectorUi();
}

bool CMainFrame::SelectedDiskPath(CString& devicePath)
{
    CDiskForgeDoc* doc = Doc();
    if (!doc || doc->m_disks.empty())
        return false;
    int disk = 0;
    if (CBrowserTreeView* tree = GetTreeView())
        if (BrowserNode* n = tree->SelectedNode())
            disk = n->disk;
    devicePath = doc->m_disks[disk].drive.devicePath;
    return true;
}

// ---------------------------------------------------------------------------
// System Browser coordination

void CMainFrame::OnBrowserNodeSelected(BrowserNode* node)
{
    CBrowserListView* list = GetListView();
    CDiskForgeDoc* doc = Doc();
    if (!node || !doc)
        return;
    if (list)
        list->ShowNode(node);

    std::vector<FieldRow> fields;
    CString title;
    ULONGLONG lba = 0;
    CString what;
    bool showHex = true;

    switch (node->kind)
    {
    case NodeKind::Disk:
        title = _T("Physical disk");
        CBrowserListView::NodeFields(doc, node, fields);
        what = _T("MBR / protective MBR");
        break;
    case NodeKind::PartitionTable:
    {
        title = _T("Partition table");
        CBrowserListView::NodeFields(doc, node, fields);
        bool gpt = doc->m_disks[node->disk].scheme.Left(3) == _T("GPT");
        lba = gpt ? 1 : 0;
        what = gpt ? _T("GPT header") : _T("MBR partition table");
        break;
    }
    case NodeKind::Partition:
        title = _T("Partition");
        CBrowserListView::NodeFields(doc, node, fields);
        lba = node->startLBA;
        what = _T("first sector of the partition");
        break;
    case NodeKind::BootRecord:
        title = _T("Boot record");
        node->fs->BootRecordFields(fields);
        lba = node->fs->BootRecordLBA();
        what = node->fs->KindName() + _T(" boot record");
        break;
    case NodeKind::Allocation:
        title = node->fs->AllocationTitle();
        CBrowserListView::NodeFields(doc, node, fields);
        lba = node->fs->AllocationLBA();
        what = node->fs->AllocationTitle();
        break;
    case NodeKind::Directory:
        title = node->entry.name;
        if (!node->fs->NeedsIndex())
            node->fs->EntryFields(node->entry, fields);
        lba = node->entry.metaLBA ? node->entry.metaLBA : node->entry.firstLBA;
        what = node->fs->Kind() == FsKind::NTFS ? _T("MFT record of this folder") : _T("this folder's directory data");
        showHex = lba != 0;
        break;
    case NodeKind::Orphans:
        title = _T("Orphaned deleted files");
        showHex = false;
        break;
    }

    ShowProperties(title, fields);
    if (showHex)
        ShowHex(node->disk, lba, what);
}

void CMainFrame::OnBrowserEntrySelected(BrowserNode* node, const FsEntry& entry)
{
    if (!node || !node->fs)
        return;
    std::vector<FieldRow> fields;
    node->fs->EntryFields(entry, fields);
    ShowProperties(entry.name, fields);
    if (entry.metaLBA)
        ShowHex(node->disk, entry.metaLBA, node->fs->Kind() == FsKind::NTFS
            ? _T("MFT record of ") + entry.name : _T("directory entry of ") + entry.name);
}

void CMainFrame::OnBrowserPartitionSelected(BrowserNode* node, int part)
{
    CDiskForgeDoc* doc = Doc();
    if (!node || !doc || part < 0)
        return;
    BrowserNode temp;
    temp.kind = NodeKind::Partition;
    temp.disk = node->disk;
    temp.part = part;
    std::vector<FieldRow> fields;
    CBrowserListView::NodeFields(doc, &temp, fields);
    ShowProperties(_T("Partition"), fields);
    ShowHex(node->disk, doc->m_disks[node->disk].parts[part].startLBA, _T("first sector of the partition"));
}

// ---------------------------------------------------------------------------
// System Browser commands

void CMainFrame::OnDiskRefresh()
{
    m_wndHex.Close();
    ShowProperties(CString(), {});
    if (CDiskForgeDoc* doc = Doc())
    {
        doc->RefreshDisks();
        CString s;
        s.Format(_T("Found %u physical disk(s)."), (UINT)doc->m_disks.size());
        Log(s);
    }
}

void CMainFrame::OnDiskOpenImage()
{
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    _T("Raw disk images (*.img;*.dd;*.raw;*.bin;*.ima)|*.img;*.dd;*.raw;*.bin;*.ima|All files (*.*)|*.*||"),
                    this);
    if (dlg.DoModal() != IDOK)
        return;
    CDiskForgeDoc* doc = Doc();
    CString err;
    m_wndHex.Close();
    if (!doc || !doc->AddImage(dlg.GetPathName(), err))
    {
        AfxMessageBox(_T("Could not open the image:\r\n") + err, MB_ICONERROR);
        return;
    }
    Log(_T("Opened disk image ") + dlg.GetPathName() + _T(" (read-only)."));
}

void CMainFrame::OnShowDeleted()
{
    if (CDiskForgeDoc* doc = Doc())
    {
        doc->m_showDeleted = !doc->m_showDeleted;
        if (!doc->m_showDeleted)
            doc->m_deletedOnly = false;
        doc->UpdateAllViews(nullptr, CDiskForgeDoc::HINT_FILTER_CHANGED);
    }
}

void CMainFrame::OnUpdateShowDeleted(CCmdUI* pCmdUI)
{
    CDiskForgeDoc* doc = Doc();
    pCmdUI->SetCheck(doc && doc->m_showDeleted);
}

void CMainFrame::OnDeletedOnly()
{
    if (CDiskForgeDoc* doc = Doc())
    {
        doc->m_deletedOnly = !doc->m_deletedOnly;
        if (doc->m_deletedOnly)
            doc->m_showDeleted = true;
        doc->UpdateAllViews(nullptr, CDiskForgeDoc::HINT_FILTER_CHANGED);
    }
}

void CMainFrame::OnUpdateDeletedOnly(CCmdUI* pCmdUI)
{
    CDiskForgeDoc* doc = Doc();
    pCmdUI->SetCheck(doc && doc->m_deletedOnly);
}

void CMainFrame::OnUpdateRecover(CCmdUI* pCmdUI)
{
    CBrowserListView* list = GetListView();
    pCmdUI->Enable(list && !list->SelectedEntries().empty());
}

void CMainFrame::OnUpdateEntryCommand(CCmdUI* pCmdUI)
{
    CBrowserListView* list = GetListView();
    pCmdUI->Enable(list && list->FocusedEntry() != nullptr);
}

void CMainFrame::OnRecover()
{
    CBrowserListView* list = GetListView();
    if (!list || !list->CurrentNode())
        return;
    std::vector<FsEntry> entries = list->SelectedEntries();
    if (entries.empty())
    {
        AfxMessageBox(_T("Select one or more files or folders in the list first."), MB_ICONINFORMATION);
        return;
    }
    RecoverEntries(list->CurrentNode(), entries);
}

void CMainFrame::RecoverEntries(BrowserNode* node, const std::vector<FsEntry>& entries)
{
    CDiskForgeDoc* doc = Doc();
    std::shared_ptr<IFileSystem> fs = node->fs;
    if (!doc || !fs)
        return;

    CFolderPickerDialog dlg(nullptr, 0, this);
    dlg.m_ofn.lpstrTitle = _T("Choose a folder - preferably on ANOTHER disk - for the recovered files");
    if (dlg.DoModal() != IDOK)
        return;
    CString dest = dlg.GetPathName();

    // Writing recovered data onto the volume it comes from can overwrite
    // the very clusters still waiting to be recovered.
    const DriveInfo& drive = doc->m_disks[node->disk].drive;
    ULONGLONG srcStart = node->startLBA * drive.bytesPerSector;
    ULONGLONG srcEnd = srcStart + node->sectors * drive.bytesPerSector;
    bool sameVolume = false;
    for (const VolumeExtent& v : CVolumeMap::ExtentsForPath(dest))
        if (v.diskNumber == drive.driveIndex && v.startByte < srcEnd && srcStart < v.startByte + v.lengthBytes)
            sameVolume = true;
    if (sameVolume &&
        AfxMessageBox(_T("The chosen folder is on the same partition the files are being recovered from.\r\n\r\n")
                      _T("Saving there can overwrite the deleted data before it has been read - including the ")
                      _T("rest of the files you are recovering. Choose a folder on another disk (for example a ")
                      _T("USB drive) instead.\r\n\r\nSave to this folder anyway?"),
                      MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES)
        return;

    CWaitCursor wait;
    int ok = 0, partial = 0, failed = 0;

    std::function<void(const FsEntry&, const CString&, int)> recover = [&](const FsEntry& e, const CString& folder, int depth)
    {
        CString target = UniquePath(folder, SafeFileName(e.name));
        if (e.isDirectory)
        {
            if (depth > 64)
                return;
            if (!CreateDirectory(target, nullptr))
            {
                ++failed;
                Log(_T("Could not create folder ") + target);
                return;
            }
            std::vector<FsEntry> children;
            CString err;
            if (!fs->ListDirectory(e, children, err))
            {
                ++failed;
                Log(_T("Could not read folder ") + e.name + _T(": ") + err);
                return;
            }
            for (const FsEntry& c : children)
                recover(c, target, depth + 1);
            return;
        }

        SetStatusText(_T("Recovering ") + e.name + _T("..."));
        ULONGLONG written = 0;
        CString err = fs->Recover(e, target, written);
        CString line;
        if (err.IsEmpty())
        {
            ++ok;
            line.Format(_T("Recovered %s (%s)%s"), target.GetString(), IFileSystem::FormatSize(written).GetString(),
                        e.isDeleted && e.recover != Recoverability::Good && e.recover != Recoverability::Excellent
                            ? _T(" - content may be damaged: ") + e.recoverNote : CString());
        }
        else if (written > 0)
        {
            ++partial;
            line.Format(_T("Partly recovered %s (%s of %s): %s"), target.GetString(),
                        IFileSystem::FormatSize(written).GetString(), IFileSystem::FormatSize(e.size).GetString(),
                        err.GetString());
        }
        else
        {
            ++failed;
            DeleteFile(target);
            line.Format(_T("Could not recover %s: %s"), e.name.GetString(), err.GetString());
        }
        Log(line);

        if (written > 0)
        {
            HANDLE h = CreateFile(target, FILE_WRITE_ATTRIBUTES, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE)
            {
                const FILETIME* c = (e.created.dwLowDateTime || e.created.dwHighDateTime) ? &e.created : nullptr;
                const FILETIME* m = (e.modified.dwLowDateTime || e.modified.dwHighDateTime) ? &e.modified : nullptr;
                SetFileTime(h, c, nullptr, m);
                CloseHandle(h);
            }
        }
    };

    for (const FsEntry& e : entries)
        recover(e, dest, 0);

    CString summary;
    summary.Format(_T("Recovery finished: %d file(s) recovered, %d partly recovered, %d failed."), ok, partial, failed);
    Log(summary);
    SetStatusText(summary);
    if (AfxMessageBox(summary + _T("\r\n\r\nOpen the destination folder?"),
                      MB_YESNO | (failed ? MB_ICONWARNING : MB_ICONINFORMATION)) == IDYES)
        ShellExecute(nullptr, _T("explore"), dest, nullptr, nullptr, SW_SHOWNORMAL);
}

void CMainFrame::OnShowMeta()
{
    CBrowserListView* list = GetListView();
    const FsEntry* e = list ? list->FocusedEntry() : nullptr;
    if (!e || !list->CurrentNode())
        return;
    if (!e->metaLBA)
    {
        Log(_T("No directory entry location is known for ") + e->name + _T("."));
        return;
    }
    m_wndHex.ShowPane(TRUE, FALSE, TRUE);
    ShowHex(list->CurrentNode()->disk, e->metaLBA, (list->CurrentNode()->fs->Kind() == FsKind::NTFS
        ? _T("MFT record of ") : _T("directory entry of ")) + e->name);
}

void CMainFrame::OnShowData()
{
    CBrowserListView* list = GetListView();
    const FsEntry* e = list ? list->FocusedEntry() : nullptr;
    if (!e || !list->CurrentNode())
        return;
    if (e->resident)
    {
        Log(e->name + _T(": the data is resident - it is stored inside the MFT record (Show record)."));
        OnShowMeta();
        return;
    }
    if (!e->firstLBA)
    {
        Log(_T("No data location is known for ") + e->name + _T(" (empty file, or not recorded)."));
        return;
    }
    m_wndHex.ShowPane(TRUE, FALSE, TRUE);
    ShowHex(list->CurrentNode()->disk, e->firstLBA, _T("first data sector of ") + e->name);
}

void CMainFrame::OnBrowseOpen()
{
    CBrowserListView* list = GetListView();
    CBrowserTreeView* tree = GetTreeView();
    if (!list || !tree)
        return;
    if (const FsEntry* e = list->FocusedEntry())
    {
        if (e->isDirectory)
            tree->OpenChildDirectory(FsEntry(*e));
        else
            OnShowData();
    }
    else if (list->FocusedPartition() >= 0)
        tree->OpenChildPartition(list->FocusedPartition());
}

void CMainFrame::OnBrowseUp()
{
    if (CBrowserTreeView* tree = GetTreeView())
        tree->SelectParent();
}

// ---------------------------------------------------------------------------
// Sector View navigation

void CMainFrame::OnUpdateSectorCommand(CCmdUI* pCmdUI)
{
    CDiskForgeDoc* doc = Doc();
    pCmdUI->Enable(doc && !doc->m_disks.empty());
}

void CMainFrame::OnSectorFirst()
{
    if (m_wndHex.IsOpen() ? m_wndHex.ShowFirst() : false)
        UpdateSectorUi();
    else
        OnSectorGoto();
}

void CMainFrame::OnSectorPrev()
{
    if (m_wndHex.Step(-1))
        UpdateSectorUi();
}

void CMainFrame::OnSectorNext()
{
    if (m_wndHex.Step(1))
        UpdateSectorUi();
}

void CMainFrame::OnSectorLast()
{
    if (m_wndHex.ShowLast())
        UpdateSectorUi();
}

void CMainFrame::UpdateSectorUi()
{
    CString s;
    s.Format(_T("%llu"), m_wndHex.CurrentLBA());
    if (CMFCRibbonEdit* edit = DYNAMIC_DOWNCAST(CMFCRibbonEdit, m_wndRibbonBar.FindByID(ID_SECTOR_EDIT)))
        edit->SetEditText(s);
    if (CMFCRibbonBaseElement* p = m_wndStatusBar.FindElement(ID_STATUSBAR_PANE2))
    {
        p->SetText(m_wndHex.DevicePath() + _T("  LBA ") + s);
        p->Redraw();
    }
}

void CMainFrame::OnSectorGoto()
{
    CString text = _T("0");
    if (CMFCRibbonEdit* edit = DYNAMIC_DOWNCAST(CMFCRibbonEdit, m_wndRibbonBar.FindByID(ID_SECTOR_EDIT)))
        text = edit->GetEditText();
    text.Trim();
    ULONGLONG lba = (text.Left(2).CompareNoCase(_T("0x")) == 0) ? _tcstoui64(text.GetString() + 2, nullptr, 16)
                                                                 : _tcstoui64(text.GetString(), nullptr, 10);
    CString device = m_wndHex.DevicePath();
    if (device.IsEmpty() && !SelectedDiskPath(device))
        return;
    m_wndHex.ShowPane(TRUE, FALSE, TRUE);
    if (m_wndHex.ShowSector(device, lba))
        UpdateSectorUi();
    else
        Log(_T("Sector View: ") + m_wndHex.LastError());
}

// ---------------------------------------------------------------------------
// Disk tools - each is an existing, self-contained dialog

void CMainFrame::OnToolPartitions()
{
    CString device;
    if (!SelectedDiskPath(device))
    {
        AfxMessageBox(_T("No physical disks are available (DiskForge must run as Administrator)."), MB_ICONWARNING);
        return;
    }
    CDiskManager dm;
    if (!dm.OpenDrive(device))
    {
        AfxMessageBox(_T("Could not open the drive:\r\n") + dm.GetLastError(), MB_ICONERROR);
        return;
    }
    // Read-only: the dialog only ever reads sectors via dm.
    CPartitionDialog dlg(dm, this);
    bool gotSelection = (dlg.DoModal() == IDOK && dlg.HasSelection());
    ULONGLONG sector = gotSelection ? dlg.GetSelectedSector() : 0;
    dm.CloseDrive();
    if (gotSelection)
    {
        m_wndHex.ShowPane(TRUE, FALSE, TRUE);
        if (m_wndHex.ShowSector(device, sector, _T("partition start")))
            UpdateSectorUi();
    }
}

void CMainFrame::OnToolDiskCopy()
{
    CDiskCopyDialog dlg(this);
    dlg.DoModal();
}

void CMainFrame::OnToolBadSector()
{
    CBadSectorScanDialog dlg(this);
    dlg.DoModal();
}

void CMainFrame::OnToolSmart()
{
    CSmartHealthDialog dlg(this);
    dlg.DoModal();
}

void CMainFrame::OnToolMbrGpt()
{
    // Write-capable: the dialog enforces Analyze -> backup -> type-to-confirm.
    CMbrGptConvertDialog dlg(this);
    dlg.DoModal();
    OnDiskRefresh(); // the partition table may have changed
}

void CMainFrame::OnToolPartEdit()
{
    // Write-capable: confirms each change before writing.
    CPartitionEditDialog dlg(this);
    dlg.DoModal();
    OnDiskRefresh();
}

void CMainFrame::OnToolWipe()
{
    // Write-capable: type-to-confirm gate before any write.
    CWipeDiskDialog dlg(this);
    dlg.DoModal();
    OnDiskRefresh();
}

void CMainFrame::OnToolEfiBoot()
{
    // Writes NVRAM boot variables only - no disk sectors.
    CEfiBootDialog dlg(this);
    dlg.DoModal();
}

void CMainFrame::OnToolLostPart()
{
    CLostPartitionDialog dlg(this);
    dlg.DoModal();
}

void CMainFrame::OnToolFileRecovery()
{
    CFileRecoveryDialog dlg(this);
    dlg.DoModal();
}
