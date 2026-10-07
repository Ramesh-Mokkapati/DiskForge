// NavTreeView.cpp - Left-pane navigation tree
#include "pch.h"
#include "NavTreeView.h"
#include "DiskForgeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CNavTreeView, CTreeView)

BEGIN_MESSAGE_MAP(CNavTreeView, CTreeView)
    ON_NOTIFY_REFLECT(TVN_SELCHANGED, &CNavTreeView::OnSelChanged)
END_MESSAGE_MAP()

void CNavTreeView::OnInitialUpdate()
{
    CTreeView::OnInitialUpdate();

    CTreeCtrl& tree = GetTreeCtrl();
    tree.ModifyStyle(0, TVS_SHOWSELALWAYS | TVS_FULLROWSELECT);

    static const struct { LPCTSTR label; UINT id; } k_items[] = {
        { _T("Partition Table"),       IDC_BTN_PARTITIONS    },
        { _T("Disk Copy"),             IDC_BTN_DISKCOPY      },
        { _T("Bad Sector Scan"),       IDC_BTN_BADSECTORSCAN },
        { _T("S.M.A.R.T. Health"),     IDC_BTN_SMARTHEALTH   },
        { _T("MBR to GPT Convert"),    IDC_BTN_MBRGPT        },
        { _T("Partition Editor"),      IDC_BTN_PARTEDIT      },
        { _T("Wipe Disk"),             IDC_BTN_WIPEDISK      },
        { _T("EFI Boot Manager"),      IDC_BTN_EFIBOOT       },
        { _T("Lost Partition Search"), IDC_BTN_LOSTPART      },
        { _T("File Recovery"),         IDC_BTN_FILERECOVERY  },
        { _T("File Unlocker"),         IDC_BTN_FILEUNLOCKER  },
        { _T("Startup Manager"),       IDC_BTN_STARTUP       },
        { _T("Registry Scanner"),      IDC_BTN_REGSCAN       },
    };

    for (auto& item : k_items)
    {
        HTREEITEM h = tree.InsertItem(item.label, TVI_ROOT, TVI_LAST);
        tree.SetItemData(h, (DWORD_PTR)item.id);
    }
}

CDiskForgeDlg* CNavTreeView::GetMainView()
{
    CSplitterWnd* pSplitter = DYNAMIC_DOWNCAST(CSplitterWnd, GetParent());
    if (!pSplitter) return nullptr;
    return DYNAMIC_DOWNCAST(CDiskForgeDlg, pSplitter->GetPane(0, 1));
}

void CNavTreeView::OnSelChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    CDiskForgeDlg* pMain = GetMainView();
    if (!pMain) return;

    HTREEITEM hSel = GetTreeCtrl().GetSelectedItem();
    if (!hSel) return;

    UINT id = (UINT)GetTreeCtrl().GetItemData(hSel);
    pMain->PostMessage(WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0);
}
