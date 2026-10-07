// MainFrame.h - SDI main frame window
#pragma once

class CMainFrame : public CFrameWnd
{
    DECLARE_DYNCREATE(CMainFrame)
public:
    CMainFrame() = default;
    virtual ~CMainFrame() = default;

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;
    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CSplitterWnd m_wndSplitter;

    static const int MIN_WIDTH  = 960;
    static const int MIN_HEIGHT = 680;
    static const int NAV_WIDTH  = 200;  // initial left-pane width (pixels)
};
