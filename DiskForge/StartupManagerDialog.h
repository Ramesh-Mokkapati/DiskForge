// StartupManagerDialog.h - View, add, delete, and enable/disable Windows
// startup registry entries (HKCU/HKLM Run and RunOnce).
#pragma once
#include "resource.h"
#include <vector>


struct StartupEntry {
    CString name;
    CString command;
    HKEY    hive;       // HKEY_CURRENT_USER or HKEY_LOCAL_MACHINE
    bool    isRunOnce;
    bool    enabled;
    CString hiveLabel;  // "HKCU" or "HKLM"
    CString keyLabel;   // "Run" or "RunOnce"
};

// ============================================================
//  CAddStartupEntryDialog  (modal sub-dialog)
// ============================================================
class CAddStartupEntryDialog : public CDialogEx
{
public:
    CAddStartupEntryDialog(CWnd* pParent = nullptr);
    enum { IDD = IDD_ADDSTARTUPENTRY };

    CString m_name;
    CString m_command;
    HKEY    m_hive;
    bool    m_runOnce;

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnBtnBrowse();
    afx_msg void OnBtnAdd();
    afx_msg void OnBtnCancel();
    DECLARE_MESSAGE_MAP()

private:
    CEdit   m_editName;
    CEdit   m_editCmd;
    CButton m_radioHkcu;
    CButton m_radioHklm;
    CButton m_chkRunOnce;
    CButton m_btnBrowse;
    CButton m_btnAdd;
    CButton m_btnCancel;
    CStatic m_staticName;
    CStatic m_staticCmd;
};

// ============================================================
//  CStartupManagerDialog  (resizable modal dialog)
// ============================================================
class CStartupManagerDialog : public CDialogEx
{
public:
    CStartupManagerDialog(CWnd* pParent = nullptr);
    enum { IDD = IDD_STARTUPMANAGER };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() {}

    afx_msg void OnBtnAdd();
    afx_msg void OnBtnDelete();
    afx_msg void OnBtnToggle();
    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnClose();
    afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    DECLARE_MESSAGE_MAP()

private:
    CListCtrl m_list;
    CButton   m_btnAdd;
    CButton   m_btnDelete;
    CButton   m_btnToggle;
    CButton   m_btnRefresh;
    CButton   m_btnClose;
    CStatic   m_staticStatus;

    std::vector<StartupEntry> m_entries;

    void LoadEntries();
    void PopulateList();
    void LayoutControls(int cx, int cy);
    void SetStatus(const CString& msg);
    int  GetSelectedIndex();
    void UpdateButtonState();

    bool GetStartupApprovedStatus(HKEY hive, const CString& name, bool& enabled);
    bool SetStartupApprovedStatus(HKEY hive, const CString& name, bool enable);
    bool DeleteFromRegistry(const StartupEntry& e);
    bool AddToRegistry(const CString& name, const CString& command, HKEY hive, bool runOnce);

    static const int MIN_W = 660;
    static const int MIN_H = 360;
};
