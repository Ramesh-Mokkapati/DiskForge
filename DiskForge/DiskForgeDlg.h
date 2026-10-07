// DiskForgeDlg.h - Main dialog class
#pragma once

#include "resource.h"
#include "DiskManager.h"
#include "HexView.h"

class CDiskForgeDlg : public CFormView
{
    DECLARE_DYNCREATE(CDiskForgeDlg)
public:
    CDiskForgeDlg();
    virtual ~CDiskForgeDlg() = default;

    enum { IDD = 100 }; // Dialog template ID

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnInitialUpdate();
    afx_msg void OnDestroy();

    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnOpen();
    afx_msg void OnBtnGoto();
    afx_msg void OnBtnPrev();
    afx_msg void OnBtnNext();
    afx_msg void OnBtnFirst();
    afx_msg void OnBtnLast();
    afx_msg void OnBtnPartitions();
    afx_msg void OnBtnDiskCopy();
    afx_msg void OnBtnBadSectorScan();
    afx_msg void OnBtnSmartHealth();
    afx_msg void OnBtnMbrGptConvert();
    afx_msg void OnBtnPartitionEdit();
    afx_msg void OnBtnWipeDisk();
    afx_msg void OnBtnEfiBoot();
    afx_msg void OnBtnLostPartition();
    afx_msg void OnBtnFileRecovery();
    afx_msg void OnBtnFileUnlocker();
    afx_msg void OnBtnStartup();
    afx_msg void OnBtnRegScan();
    afx_msg void OnComboSelChange();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnEditSectorChange();

    DECLARE_MESSAGE_MAP()

private:
    // Controls
    CComboBox       m_comboDrives;
    CEdit           m_editSector;
    CStatic         m_staticDriveInfo;
    CStatic         m_staticSector;
    CStatic         m_staticStatus;
    CButton         m_btnOpen;
    CButton         m_btnGoto;
    CButton         m_btnPrev;
    CButton         m_btnNext;
    CButton         m_btnFirst;
    CButton         m_btnLast;
    CButton         m_btnRefresh;
    CProgressCtrl   m_progress;

    // Our custom hex viewer child window
    CHexView        m_hexView;

    // Disk management
    CDiskManager            m_diskManager;
    std::vector<DriveInfo>  m_drives;
    ULONGLONG               m_currentSector;
    bool                    m_driveOpen;

    // Helpers
    void RefreshDriveList();
    void UpdateDriveInfo();
    void LoadSector(ULONGLONG sectorNum);
    void UpdateNavigationState();
    void SetStatus(const CString& msg, COLORREF color = RGB(0, 0, 0));
    void LayoutControls(int cx, int cy);

    // Minimum content-pane size
    static const int MIN_WIDTH  = 940;
    static const int MIN_HEIGHT = 640;

    // For layout tracking
    CRect m_initRect;
};
