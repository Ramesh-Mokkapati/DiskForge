// HexView.h - Custom hex-dump view window
#pragma once
#include "pch.h"

// CHexView - owner-drawn scrollable hex dump control.
// Shows data as: offset | 16-byte hex groups | ASCII
class CHexView : public CWnd
{
public:
    CHexView();
    virtual ~CHexView();

    // Set the data to display and the sector/byte offset base
    void SetData(const std::vector<BYTE>& data, ULONGLONG baseOffset = 0);
    void ClearData();

    // Register the window class (call once before first Create)
    static bool RegisterClass();

    bool Create(CWnd* pParent, const RECT& rect, UINT nID);

protected:
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    std::vector<BYTE>   m_data;
    ULONGLONG           m_baseOffset;   // Byte offset of data[0] on disk
    int                 m_scrollPosV;   // Vertical scroll position (in rows)
    int                 m_scrollPosH;   // Horizontal scroll position (in pixels)
    int                 m_selectedByte; // Currently selected byte index (-1 = none)

    // Cached layout metrics (computed in RecalcLayout)
    CFont   m_font;
    int     m_charWidth;
    int     m_lineHeight;
    int     m_bytesPerRow;
    int     m_totalRows;
    int     m_visibleRows;

    // Column offsets (relative to left edge after H-scroll)
    int     m_colOffset;  // Offset column width in chars
    int     m_colHex;     // Start of hex region
    int     m_colAscii;   // Start of ASCII region
    int     m_totalWidth; // Total virtual width in pixels

    void RecalcLayout();
    void UpdateScrollBars();
    void DrawRow(CDC& dc, int rowIndex, int y, const CRect& clipRect);
    int  HitTestByte(CPoint pt) const;

    static const int BYTES_PER_ROW = 16;
    static const int OFFSET_CHARS  = 10; // "0x00000000: "
};
