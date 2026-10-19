// HexPane.h - "Sector View" docking pane: the raw-sector hex viewer that used
// to fill DiskForge's main window, now a pane that follows the System
// Browser (selecting a partition, boot record, MFT record or file jumps it to
// the matching LBA). Navigation lives on the ribbon's Sector panel.
#pragma once
#include "HexView.h"
#include "DiskManager.h"

class CHexPane : public CDockablePane
{
public:
    CHexPane() noexcept = default;

    // Opens devicePath (if not already open) and shows sector 'lba'.
    // 'what' describes the sector ("NTFS boot record", "MFT record 42", ...).
    bool ShowSector(const CString& devicePath, ULONGLONG lba, const CString& what = CString());
    bool Step(LONGLONG delta);
    bool ShowFirst();
    bool ShowLast();

    bool      IsOpen() const { return m_dm.IsOpen(); }
    ULONGLONG CurrentLBA() const { return m_lba; }
    ULONGLONG TotalSectors() const { return m_dm.GetTotalSectors(); }
    CString   DevicePath() const { return m_device; }
    CString   LastError() const { return m_error; }
    void      Close();

protected:
    CHexView     m_hexView;
    CStatic      m_label;
    CFont        m_labelFont;
    CDiskManager m_dm;
    CString      m_device;
    CString      m_what;
    CString      m_error;
    ULONGLONG    m_lba = 0;

    void UpdateLabel();

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()
};
