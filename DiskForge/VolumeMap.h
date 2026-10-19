// VolumeMap.h - Maps mounted Windows volumes (drive letters) to the physical
// disk extents they occupy, so the System Browser can label partitions with
// their drive letter, and file recovery can warn before writing a recovered
// file onto the very disk area it is being recovered from.
#pragma once
#include "pch.h"

struct VolumeExtent
{
    CString   root;          // "C:\"
    DWORD     diskNumber = 0;
    ULONGLONG startByte = 0;
    ULONGLONG lengthBytes = 0;
};

class CVolumeMap
{
public:
    static std::vector<VolumeExtent> Enumerate();

    // Drive letters ("C:", "D:") whose extents start at startLBA on the disk.
    static CString LettersFor(const std::vector<VolumeExtent>& map, DWORD diskNumber,
                              ULONGLONG startLBA, DWORD sectorSize);

    // Extents of the volume holding 'path' (empty when it can't be resolved,
    // e.g. a network share).
    static std::vector<VolumeExtent> ExtentsForPath(const CString& path);
};
