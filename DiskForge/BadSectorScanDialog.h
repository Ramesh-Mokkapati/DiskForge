// BadSectorScanDialog.h - UI for the read-only Bad Sector Scan feature.
#pragma once
#include "resource.h"
#include "DiskManager.h"
#include "PartitionParser.h"
#include "BadSectorScanner.h"
#include "ScanMapView.h"
#include "SectorRepairer.h"


class CBadSectorScanDialog : public CDialogEx
{
public:
    explicit CBadSectorScanDialog(CWnd* pParent = nullptr);
    virtual ~CBadSectorScanDialog();

    enum { IDD = IDD_BADSECTORSCAN };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnCancel();
    virtual void OnOK() {}

    afx_msg void OnComboDriveChange();
    afx_msg void OnRadioMode();
    afx_msg void OnBtnChoosePartition();
    afx_msg void OnBtnStart();
    afx_msg void OnBtnStop();
    afx_msg void OnBtnRepair();
    afx_msg void OnListSelChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBtnClose();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CComboBox      m_comboDrive;
    CButton        m_radioWhole;
    CButton        m_radioPart;
    CButton        m_radioRange;
    CButton        m_btnChoosePart;
    CStatic        m_staticPart;
    CStatic        m_staticRangeStartLbl;
    CStatic        m_staticRangeCountLbl;
    CEdit          m_editRangeStart;
    CEdit          m_editRangeCount;
    CScanMapView   m_mapView;
    CStatic        m_staticStatus;
    CListCtrl      m_listBad;
    CButton        m_btnStart;
    CButton        m_btnStop;
    CButton        m_btnRepair;
    CButton        m_btnClose;

    std::vector<DriveInfo> m_drives;
    PartitionEntry         m_chosenPartition;
    bool                   m_hasChosenPartition;

    CBadSectorScanner      m_scanner;
    bool                   m_scanRunning;
    CWinThread*            m_pThread;
    size_t                 m_lastBadRangeCount; // avoids re-populating the list every tick
    DWORD                  m_lastSectorSize;    // sector size of the most recently run/running job
    CString                m_scannedDevicePath; // drive the current bad-sector list belongs to
    UINT                   m_scannedDriveIndex;
    std::vector<BadSectorRange> m_badRanges; // local copy, so Repair can look up a selected row

    static const int MIN_WIDTH  = 700;
    static const int MIN_HEIGHT = 520;
    static const UINT_PTR TIMER_ID = 2;

    void UpdateModeEnablement();
    void UpdateChosenPartitionLabel();
    bool BuildJob(ScanJob& jobOut, CString& errorOut);
    void SetUiEnabled(bool enabled);
    void PopulateBadList();

    static UINT __cdecl ScanThreadProc(LPVOID pParam);
};
