// SidebarButton.h - Flat, dark-themed nav-list button used in the app's
// left sidebar (visual pattern modeled after tools like CCleaner: a dark
// vertical list of navigation items with a hover highlight and a left
// accent bar, rather than a row of ordinary push buttons).
//
// Must be created with the BS_OWNERDRAW style for DrawItem() to be invoked.
#pragma once
#include "pch.h"

class CSidebarButton : public CButton
{
public:
    CSidebarButton();

protected:
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    DECLARE_MESSAGE_MAP()

private:
    bool m_hovered;
    bool m_tracking;
};
