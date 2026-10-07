// WipeDiskDialog.h - UI for the Wipe Disk/Partition feature.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "WipeEngine.h"


class CWipeDiskDialog : public CDialogEx
{
public:
    explicit CWipeDiskDialog(CWnd* pParent = nullptr);
    virtual ~CWipeDiskDialog();

    enum { IDD = IDD_WIPEDISK };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnCancel();
    virtual void OnOK() {}

    afx_msg void OnComboDriveChange();
    afx_msg void OnRadioMode();
    afx_msg void OnBtnChoosePartition();
    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnStart();
    afx_msg void OnBtnCancelWipe();
    afx_msg void OnBtnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox     m_comboDrive;
    CButton       m_radioWhole;
    CButton       m_radioPart;
    CButton       m_btnChoosePart;
    CStatic       m_staticPart;
    CComboBox     m_comboPattern;
    CButton       m_btnRefresh;
    CStatic       m_staticSummary;
    CProgressCtrl m_progress;
    CStatic       m_staticStatus;
    CButton       m_btnStart;
    CButton       m_btnCancelWipe;
    CButton       m_btnClose;

    std::vector<DriveInfo> m_drives;
    PartitionEntry         m_chosenPartition;
    bool                   m_hasChosenPartition;

    CWipeEngine   m_engine;
    bool          m_wipeRunning;
    CWinThread*   m_pThread;

    static const int MIN_WIDTH  = 680;
    static const int MIN_HEIGHT = 420;
    static const UINT_PTR TIMER_ID = 3;

    void RefreshDriveList();
    void UpdateChosenPartitionLabel();
    void UpdateModeEnablement();
    void UpdateSummary();
    void SetUiEnabled(bool enabled);
    bool BuildJob(WipeJob& jobOut, CString& errorOut);

    static UINT __cdecl WipeThreadProc(LPVOID pParam);
};
