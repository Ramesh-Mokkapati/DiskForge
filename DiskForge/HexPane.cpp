// HexPane.cpp - Sector View docking pane
#include "pch.h"
#include "HexPane.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CHexPane, CDockablePane)
    ON_WM_CREATE()
    ON_WM_SIZE()
END_MESSAGE_MAP()

int CHexPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CDockablePane::OnCreate(lpCreateStruct) == -1)
        return -1;

    CRect rectDummy(0, 0, 10, 10);
    if (!m_label.Create(_T("Select a disk, partition or file in the System Browser."),
                        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE, rectDummy, this, 1))
        return -1;
    m_label.SetFont(&afxGlobalData.fontBold);

    if (!m_hexView.Create(this, rectDummy, 2))
        return -1;
    return 0;
}

void CHexPane::OnSize(UINT nType, int cx, int cy)
{
    CDockablePane::OnSize(nType, cx, cy);
    if (!m_label.GetSafeHwnd())
        return;
    const int labelH = 22;
    m_label.SetWindowPos(nullptr, 4, 0, std::max<int>(0, cx - 8), labelH, SWP_NOZORDER | SWP_NOACTIVATE);
    m_hexView.SetWindowPos(nullptr, 0, labelH, cx, std::max<int>(0, cy - labelH), SWP_NOZORDER | SWP_NOACTIVATE);
}

void CHexPane::Close()
{
    m_dm.CloseDrive();
    m_device.Empty();
    m_lba = 0;
    if (m_hexView.GetSafeHwnd())
        m_hexView.ClearData();
    if (m_label.GetSafeHwnd())
        m_label.SetWindowText(_T("Select a disk, partition or file in the System Browser."));
}

bool CHexPane::ShowSector(const CString& devicePath, ULONGLONG lba, const CString& what)
{
    if (!m_dm.IsOpen() || m_device.CompareNoCase(devicePath) != 0)
    {
        m_dm.CloseDrive();
        m_device.Empty();
        if (!m_dm.OpenDrive(devicePath))
        {
            m_error = m_dm.GetLastError();
            return false;
        }
        m_device = devicePath;
    }

    if (lba >= m_dm.GetTotalSectors())
    {
        m_error.Format(_T("Sector %llu is past the end of the disk (last sector %llu)."), lba,
                       m_dm.GetTotalSectors() - 1);
        return false;
    }

    std::vector<BYTE> buf;
    if (!m_dm.ReadSector(lba, buf))
    {
        m_error = m_dm.GetLastError();
        m_hexView.ClearData();
        return false;
    }
    m_lba = lba;
    m_what = what;
    m_hexView.SetData(buf, lba * m_dm.GetBytesPerSector());
    UpdateLabel();
    return true;
}

bool CHexPane::Step(LONGLONG delta)
{
    if (!m_dm.IsOpen())
        return false;
    LONGLONG target = (LONGLONG)m_lba + delta;
    if (target < 0 || (ULONGLONG)target >= m_dm.GetTotalSectors())
        return false;
    return ShowSector(m_device, (ULONGLONG)target);
}

bool CHexPane::ShowFirst()
{
    return m_dm.IsOpen() && ShowSector(m_device, 0, _T("MBR / protective MBR"));
}

bool CHexPane::ShowLast()
{
    return m_dm.IsOpen() && ShowSector(m_device, m_dm.GetTotalSectors() - 1);
}

void CHexPane::UpdateLabel()
{
    CString text;
    text.Format(_T("%s   LBA %llu of %llu   (byte offset 0x%llX)"), m_device.GetString(), m_lba,
                m_dm.GetTotalSectors() - 1, m_lba * m_dm.GetBytesPerSector());
    if (!m_what.IsEmpty())
        text += _T("   -   ") + m_what;
    m_label.SetWindowText(text);
}
