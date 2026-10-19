// PropertiesWnd.cpp - Properties docking pane
#include "pch.h"
#include "PropertiesWnd.h"
#include "resource.h"
#include "DiskForge.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CPropertiesWnd, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_COMMAND(ID_EXPAND_ALL, OnExpandAllProperties)
    ON_UPDATE_COMMAND_UI(ID_EXPAND_ALL, OnUpdateExpandAllProperties)
    ON_COMMAND(ID_SORTPROPERTIES, OnSortProperties)
    ON_UPDATE_COMMAND_UI(ID_SORTPROPERTIES, OnUpdateSortProperties)
    ON_WM_SETFOCUS()
    ON_WM_SETTINGCHANGE()
END_MESSAGE_MAP()

void CPropertiesWnd::AdjustLayout()
{
    if (GetSafeHwnd() == nullptr || (AfxGetMainWnd() != nullptr && AfxGetMainWnd()->IsIconic()))
        return;

    CRect rectClient;
    GetClientRect(rectClient);
    int cyTlb = m_wndToolBar.CalcFixedLayout(FALSE, TRUE).cy;

    m_wndObjectCombo.SetWindowPos(nullptr, rectClient.left, rectClient.top, rectClient.Width(), m_nComboHeight,
                                  SWP_NOACTIVATE | SWP_NOZORDER);
    m_wndToolBar.SetWindowPos(nullptr, rectClient.left, rectClient.top + m_nComboHeight, rectClient.Width(), cyTlb,
                              SWP_NOACTIVATE | SWP_NOZORDER);
    m_wndPropList.SetWindowPos(nullptr, rectClient.left, rectClient.top + m_nComboHeight + cyTlb, rectClient.Width(),
                               rectClient.Height() - (m_nComboHeight + cyTlb), SWP_NOACTIVATE | SWP_NOZORDER);
}

int CPropertiesWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CDockablePane::OnCreate(lpCreateStruct) == -1)
        return -1;

    CRect rectDummy;
    rectDummy.SetRectEmpty();

    const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_BORDER | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    if (!m_wndObjectCombo.Create(dwViewStyle, rectDummy, this, 1))
        return -1;
    m_wndObjectCombo.AddString(_T("(nothing selected)"));
    m_wndObjectCombo.SetCurSel(0);

    CRect rectCombo;
    m_wndObjectCombo.GetClientRect(&rectCombo);
    m_nComboHeight = rectCombo.Height();

    if (!m_wndPropList.Create(WS_VISIBLE | WS_CHILD, rectDummy, this, 2))
        return -1;

    InitPropList();

    m_wndToolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_PROPERTIES);
    m_wndToolBar.LoadToolBar(IDR_PROPERTIES, 0, 0, TRUE /* Is locked */);
    m_wndToolBar.CleanUpLockedImages();
    m_wndToolBar.LoadBitmap(theApp.m_bHiColorIcons ? IDB_PROPERTIES_HC : IDR_PROPERTIES, 0, 0, TRUE /* Locked */);
    m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
    m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP |
                              CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));
    m_wndToolBar.SetOwner(this);
    m_wndToolBar.SetRouteCommandsViaFrame(FALSE);

    AdjustLayout();
    return 0;
}

void CPropertiesWnd::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    AdjustLayout();
}

void CPropertiesWnd::OnExpandAllProperties()
{
    m_wndPropList.ExpandAll();
}

void CPropertiesWnd::OnUpdateExpandAllProperties(CCmdUI* /*pCmdUI*/)
{
}

void CPropertiesWnd::OnSortProperties()
{
    m_wndPropList.SetAlphabeticMode(!m_wndPropList.IsAlphabeticMode());
}

void CPropertiesWnd::OnUpdateSortProperties(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_wndPropList.IsAlphabeticMode());
}

void CPropertiesWnd::InitPropList()
{
    SetPropListFont();
    m_wndPropList.EnableHeaderCtrl(FALSE);
    m_wndPropList.EnableDescriptionArea();
    m_wndPropList.SetVSDotNetLook();
}

void CPropertiesWnd::ShowFields(const CString& title, const std::vector<FieldRow>& fields)
{
    if (!m_wndPropList.GetSafeHwnd())
        return;

    m_wndObjectCombo.ResetContent();
    m_wndObjectCombo.AddString(title.IsEmpty() ? CString(_T("(nothing selected)")) : title);
    m_wndObjectCombo.SetCurSel(0);

    m_wndPropList.SetRedraw(FALSE);
    m_wndPropList.RemoveAll();

    CMFCPropertyGridProperty* group = nullptr;
    CString groupName;
    for (const FieldRow& f : fields)
    {
        if (!group || f.group != groupName)
        {
            // Reuse an earlier group of the same name so rows stay together.
            group = nullptr;
            for (int i = 0; i < m_wndPropList.GetPropertyCount(); ++i)
                if (CString(m_wndPropList.GetProperty(i)->GetName()) == f.group)
                    group = m_wndPropList.GetProperty(i);
            if (!group)
            {
                group = new CMFCPropertyGridProperty(f.group.IsEmpty() ? CString(_T("General")) : f.group);
                m_wndPropList.AddProperty(group, FALSE, FALSE);
            }
            groupName = f.group;
        }

        CString desc = f.description;
        if (!f.offset.IsEmpty())
            desc = _T("Offset ") + f.offset + (desc.IsEmpty() ? CString() : _T(" - ") + desc);
        CMFCPropertyGridProperty* p = new CMFCPropertyGridProperty(f.name, (_variant_t)(LPCTSTR)f.value, desc);
        p->AllowEdit(FALSE);
        group->AddSubItem(p);
    }

    m_wndPropList.SetRedraw(TRUE);
    m_wndPropList.AdjustLayout();
    m_wndPropList.RedrawWindow();
}

void CPropertiesWnd::OnSetFocus(CWnd* pOldWnd)
{
    CDockablePane::OnSetFocus(pOldWnd);
    m_wndPropList.SetFocus();
}

void CPropertiesWnd::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
    CDockablePane::OnSettingChange(uFlags, lpszSection);
    SetPropListFont();
}

void CPropertiesWnd::SetPropListFont()
{
    ::DeleteObject(m_fntPropList.Detach());

    LOGFONT lf;
    afxGlobalData.fontRegular.GetLogFont(&lf);

    NONCLIENTMETRICS info;
    info.cbSize = sizeof(info);
    afxGlobalData.GetNonClientMetrics(info);

    lf.lfHeight = info.lfMenuFont.lfHeight;
    lf.lfWeight = info.lfMenuFont.lfWeight;
    lf.lfItalic = info.lfMenuFont.lfItalic;

    m_fntPropList.CreateFontIndirect(&lf);
    m_wndPropList.SetFont(&m_fntPropList);
    m_wndObjectCombo.SetFont(&m_fntPropList);
}
