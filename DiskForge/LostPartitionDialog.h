// LostPartitionDialog.h - UI for the read-only Lost Partition Search feature.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "LostPartitionScanner.h"


class CLostPartitionDialog : public CDialogEx
{
public:
    explicit CLostPartitionDialog(CWnd* pParent = nullptr);
    virtual ~CLostPartitionDialog();

    enum { IDD = IDD_LOSTPARTITION };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnCancel();
    virtual void OnOK() {}

    afx_msg void OnComboDriveChange();
    afx_msg void OnBtnStart();
    afx_msg void OnBtnStop();
    afx_msg void OnBtnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox     m_comboDrive;
    CStatic       m_staticFreeSpace;
    CListCtrl     m_listCandidates;
    CProgressCtrl m_progress;
    CStatic       m_staticStatus;
    CButton       m_btnStart;
    CButton       m_btnStop;
    CButton       m_btnClose;

    std::vector<DriveInfo> m_drives;
    CLostPartitionScanner  m_scanner;
    bool                   m_scanRunning;
    CWinThread*            m_pThread;
    size_t                 m_lastCandidateCount;

    static const int MIN_WIDTH  = 720;
    static const int MIN_HEIGHT = 460;
    static const UINT_PTR TIMER_ID = 4;

    void UpdateFreeSpacePreview();
    void SetUiEnabled(bool enabled);
    void PopulateCandidateList();

    static UINT __cdecl ScanThreadProc(LPVOID pParam);
};
