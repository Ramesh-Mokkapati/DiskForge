// MainFrm.h - Main frame, built on the NewTest framework:
//
//   Ribbon (Home: browser / sector / tools; View: panes and look)
//   Tools bar (Outlook-style, left)  |  System Browser tree | content list
//   Caption bar (top)                 |  Properties pane (right)
//   Output + Sector View panes (bottom), ribbon status bar
//
// The frame coordinates the System Browser views with the panes, and runs
// every ribbon command (including the existing disk tool dialogs).
#pragma once
#include "OutputWnd.h"
#include "PropertiesWnd.h"
#include "HexPane.h"
#include "ViewTree.h"
#include "DiskForgeDoc.h"

class CBrowserTreeView;
class CBrowserListView;

class COutlookBar : public CMFCOutlookBar
{
    virtual BOOL AllowShowOnPaneMenu() const { return TRUE; }
    virtual void GetPaneName(CString& strName) const
    {
        BOOL bNameValid = strName.LoadString(IDS_OUTLOOKBAR);
        if (!bNameValid)
            strName.Empty();
    }
};

class CMainFrame : public CFrameWndEx
{
protected: // create from serialization only
    CMainFrame() noexcept;
    DECLARE_DYNCREATE(CMainFrame)

public:
    virtual ~CMainFrame() = default;

    CBrowserTreeView* GetTreeView();
    CBrowserListView* GetListView();

    // Called by the System Browser views.
    void OnBrowserNodeSelected(BrowserNode* node);
    void OnBrowserEntrySelected(BrowserNode* node, const FsEntry& entry);
    void OnBrowserPartitionSelected(BrowserNode* node, int part);
    void ShowProperties(const CString& title, const std::vector<FieldRow>& fields);

    void Log(const CString& text);
    void SetStatusText(const CString& text);

    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
    CMFCRibbonBar               m_wndRibbonBar;
    CMFCRibbonApplicationButton m_MainButton;
    CMFCRibbonStatusBar         m_wndStatusBar;
    COutputWnd                  m_wndOutput;
    CPropertiesWnd              m_wndProperties;
    CHexPane                    m_wndHex;
    COutlookBar                 m_wndNavigationBar;
    CViewTree                   m_wndToolsTree;
    CViewTree                   m_wndRecoveryTree;
    CViewTree                   m_wndBootTree;
    CMFCCaptionBar              m_wndCaptionBar;
    CSplitterWnd                m_wndSplitter;
    bool                        m_splitterReady = false;

    CDiskForgeDoc* Doc();
    void InitializeRibbon();
    BOOL CreateDockingWindows();
    void SetDockingWindowIcons(BOOL bHiColorIcons);
    BOOL CreateOutlookBar(CMFCOutlookBar& bar, UINT uiID, int nInitialWidth);
    BOOL CreateCaptionBar();
    void ShowHex(int disk, ULONGLONG lba, const CString& what);
    void UpdateSectorUi();
    bool SelectedDiskPath(CString& devicePath);
    void RecoverEntries(BrowserNode* node, const std::vector<FsEntry>& entries);

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnApplicationLook(UINT id);
    afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);
    afx_msg void OnOptions();
    afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);

    // View
    afx_msg void OnViewOutputWindow();
    afx_msg void OnUpdateViewOutputWindow(CCmdUI* pCmdUI);
    afx_msg void OnViewPropertiesWindow();
    afx_msg void OnUpdateViewPropertiesWindow(CCmdUI* pCmdUI);
    afx_msg void OnViewHexWindow();
    afx_msg void OnUpdateViewHexWindow(CCmdUI* pCmdUI);
    afx_msg void OnViewNavigation();
    afx_msg void OnUpdateViewNavigation(CCmdUI* pCmdUI);
    afx_msg void OnViewCaptionBar();
    afx_msg void OnUpdateViewCaptionBar(CCmdUI* pCmdUI);

    // System Browser
    afx_msg void OnDiskRefresh();
    afx_msg void OnDiskOpenImage();
    afx_msg void OnShowDeleted();
    afx_msg void OnUpdateShowDeleted(CCmdUI* pCmdUI);
    afx_msg void OnDeletedOnly();
    afx_msg void OnUpdateDeletedOnly(CCmdUI* pCmdUI);
    afx_msg void OnRecover();
    afx_msg void OnUpdateRecover(CCmdUI* pCmdUI);
    afx_msg void OnShowMeta();
    afx_msg void OnShowData();
    afx_msg void OnUpdateEntryCommand(CCmdUI* pCmdUI);
    afx_msg void OnBrowseOpen();
    afx_msg void OnBrowseUp();

    // Sector View
    afx_msg void OnSectorFirst();
    afx_msg void OnSectorPrev();
    afx_msg void OnSectorNext();
    afx_msg void OnSectorLast();
    afx_msg void OnSectorGoto();
    afx_msg void OnUpdateSectorCommand(CCmdUI* pCmdUI);

    // Disk tools
    afx_msg void OnToolPartitions();
    afx_msg void OnToolDiskCopy();
    afx_msg void OnToolBadSector();
    afx_msg void OnToolSmart();
    afx_msg void OnToolMbrGpt();
    afx_msg void OnToolPartEdit();
    afx_msg void OnToolWipe();
    afx_msg void OnToolEfiBoot();
    afx_msg void OnToolLostPart();
    afx_msg void OnToolFileRecovery();

    DECLARE_MESSAGE_MAP()

private:
    static const int MIN_WIDTH  = 960;
    static const int MIN_HEIGHT = 680;
};
