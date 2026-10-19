// PropertiesWnd.h - Properties docking pane (from the NewTest framework).
// Shows the fields of whatever is selected in the System Browser - a boot
// record's BPB, the FAT/MFT statistics, or one file's metadata and
// recoverability - grouped in a read-only property grid.
#pragma once
#include "FileSystem.h"

class CPropertiesToolBar : public CMFCToolBar
{
public:
    virtual void OnUpdateCmdUI(CFrameWnd* /*pTarget*/, BOOL bDisableIfNoHndler)
    {
        CMFCToolBar::OnUpdateCmdUI((CFrameWnd*)GetOwner(), bDisableIfNoHndler);
    }
    virtual BOOL AllowShowOnList() const { return FALSE; }
};

class CPropertiesWnd : public CDockablePane
{
public:
    CPropertiesWnd() noexcept = default;

    void AdjustLayout();
    void SetVSDotNetLook(BOOL bSet)
    {
        m_wndPropList.SetVSDotNetLook(bSet);
        m_wndPropList.SetGroupNameFullWidth(bSet);
    }

    // Replaces the grid with 'fields' under the object name 'title'.
    void ShowFields(const CString& title, const std::vector<FieldRow>& fields);

protected:
    CFont                m_fntPropList;
    CComboBox            m_wndObjectCombo;
    CPropertiesToolBar   m_wndToolBar;
    CMFCPropertyGridCtrl m_wndPropList;
    int                  m_nComboHeight = 0;

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnExpandAllProperties();
    afx_msg void OnUpdateExpandAllProperties(CCmdUI* pCmdUI);
    afx_msg void OnSortProperties();
    afx_msg void OnUpdateSortProperties(CCmdUI* pCmdUI);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);

    DECLARE_MESSAGE_MAP()

    void InitPropList();
    void SetPropListFont();
};
