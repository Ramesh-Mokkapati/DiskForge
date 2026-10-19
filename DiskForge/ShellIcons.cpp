// ShellIcons.cpp
#include "pch.h"
#include "ShellIcons.h"

HICON CShellIcons::Load(SHSTOCKICONID id, bool large)
{
    SHSTOCKICONINFO sii = {};
    sii.cbSize = sizeof(sii);
    if (FAILED(SHGetStockIconInfo(id, SHGSI_ICON | (large ? SHGSI_LARGEICON : SHGSI_SMALLICON), &sii)))
        return nullptr;
    return sii.hIcon;
}

CImageList& CShellIcons::SmallList()
{
    static CImageList list;
    if (list.GetSafeHandle())
        return list;

    const int cx = GetSystemMetrics(SM_CXSMICON), cy = GetSystemMetrics(SM_CYSMICON);
    list.Create(cx, cy, ILC_COLOR32 | ILC_MASK, Count + 1, 4);

    static const SHSTOCKICONID ids[Count] = {
        SIID_DRIVEFIXED,    // Disk
        SIID_STACK,         // Table
        SIID_DRIVEUNKNOWN,  // Partition
        SIID_SOFTWARE,      // Boot
        SIID_AUTOLIST,      // Allocation
        SIID_FOLDER,        // Folder
        SIID_FOLDEROPEN,    // FolderOpen
        SIID_DOCNOASSOC,    // File
        SIID_RECYCLERFULL,  // Orphans
        SIID_INFO,          // Field
    };
    for (SHSTOCKICONID id : ids)
    {
        HICON h = Load(id, false);
        list.Add(h ? h : AfxGetApp()->LoadStandardIcon(IDI_APPLICATION));
        if (h)
            DestroyIcon(h);
    }

    HICON del = Load(SIID_DELETE, false);
    int overlayImage = list.Add(del ? del : AfxGetApp()->LoadStandardIcon(IDI_ERROR));
    if (del)
        DestroyIcon(del);
    list.SetOverlayImage(overlayImage, kDeletedOverlay);
    return list;
}
