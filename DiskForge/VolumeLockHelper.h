// VolumeLockHelper.h - Shared helper for locking & dismounting every volume
// that lives on a given physical disk, before any raw sector write to that
// disk. Used by every write-capable module (DiskCopyEngine, MbrGptConverter,
// PartitionEditor) so a raw write never races with a mounted filesystem's
// cached state.
#pragma once
#include "pch.h"

class CVolumeLockHelper
{
public:
    // Returns the locked (and dismounted) volume handles; keep them open for
    // the duration of the raw write, then call ReleaseLockedVolumes().
    // allSucceeded is set to false if any volume on that disk could not be
    // locked - callers must treat that as fatal and not proceed with the raw
    // write in that case.
    static std::vector<HANDLE> LockAndDismountVolumesOnDisk(UINT diskIndex, bool& allSucceeded);
    static void ReleaseLockedVolumes(std::vector<HANDLE>& handles);
};
