// ShellIcons.h - Windows stock icons (SHGetStockIconInfo) for the ribbon,
// the System Browser tree and its content list, so the UI uses the same
// drive / folder / document pictures as Explorer without shipping bitmaps.
#pragma once
#include "pch.h"
#include <shellapi.h>

class CShellIcons
{
public:
    // Indices into SmallList().
    enum Index { Disk, Table, Partition, Boot, Allocation, Folder, FolderOpen, File, Orphans, Field, Count };
    static const int kDeletedOverlay = 1; // INDEXTOOVERLAYMASK(1) marks deleted entries

    static CImageList& SmallList();

    // Caller owns the returned icon (DestroyIcon) - the ribbon buttons are
    // created with bAutoDestroyIcon so they free them.
    static HICON Load(SHSTOCKICONID id, bool large);
};
