// SmartDataReader.cpp
#include "pch.h"
#include "SmartDataReader.h"

namespace
{
    // ---- Legacy "Disk Failure Prediction" IOCTLs -----------------------
    // Not declared under these names in the public SDK headers, but these
    // are long-standing, publicly documented control codes used by many
    // disk utilities (disk.sys handles them for ATA/SATA drives on standard
    // controllers). FILE_ANY_ACCESS means a GENERIC_READ-only handle is
    // sufficient - no write access is requested anywhere in this file.
    const DWORD DFP_GET_VERSION = CTL_CODE(IOCTL_DISK_BASE, 0x0800, METHOD_BUFFERED, FILE_ANY_ACCESS);
    const DWORD DFP_RECEIVE_DRIVE_DATA = CTL_CODE(IOCTL_DISK_BASE, 0x0802, METHOD_BUFFERED, FILE_ANY_ACCESS);

    //const DWORD CAP_SMART_CMD = 0x00000004;

    //const BYTE SMART_CYL_LOW   = 0x4F;
    //const BYTE SMART_CYL_HI    = 0xC2;
    //const BYTE SMART_CMD       = 0xB0;
    const BYTE SMART_READ_DATA = 0xD0;

#pragma pack(push, 1)
    struct GetVersionOutParams
    {
        BYTE  bVersion;
        BYTE  bRevision;
        BYTE  bReserved;
        BYTE  bIDEDeviceMap;
        DWORD fCapabilities;
        DWORD dwReserved[4];
    };

    struct IdeRegs
    {
        BYTE bFeaturesReg;
        BYTE bSectorCountReg;
        BYTE bSectorNumberReg;
        BYTE bCylLowReg;
        BYTE bCylHighReg;
        BYTE bDriveHeadReg;
        BYTE bCommandReg;
        BYTE bReserved;
    };

    struct SendCmdInParams
    {
        DWORD   cBufferSize;
        IdeRegs irDriveRegs;
        BYTE    bDriveNumber;
        BYTE    bReserved[3];
        DWORD   dwReserved[4];
        BYTE    bBuffer[512];
    };

    struct DriverStatus
    {
        BYTE  bDriverError;
        BYTE  bIDEStatus;
        BYTE  bReserved[2];
        DWORD dwReserved[2];
    };

    struct SendCmdOutParams
    {
        DWORD        cBufferSize;
        DriverStatus status;
        BYTE         bBuffer[512];
    };
#pragma pack(pop)
}

bool CSmartDataReader::QueryPredictFailure(HANDLE h, bool& failurePredicted)
{
    STORAGE_PREDICT_FAILURE pf = {};
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_PREDICT_FAILURE, nullptr, 0,
        &pf, sizeof(pf), &bytesReturned, nullptr);
    if (!ok)
        return false;

    failurePredicted = (pf.PredictFailure != 0);
    return true;
}

CString CSmartDataReader::AttributeName(BYTE id)
{
    switch (id)
    {
    case 1:   return _T("Raw Read Error Rate");
    case 3:   return _T("Spin-Up Time");
    case 4:   return _T("Start/Stop Count");
    case 5:   return _T("Reallocated Sectors Count");
    case 7:   return _T("Seek Error Rate");
    case 9:   return _T("Power-On Hours");
    case 10:  return _T("Spin Retry Count");
    case 12:  return _T("Power Cycle Count");
    case 168: return _T("SATA Phy Error Count");
    case 170: return _T("Bad Block Count (SSD)");
    case 173: return _T("Wear Leveling Count (SSD)");
    case 184: return _T("End-to-End Error");
    case 187: return _T("Reported Uncorrectable Errors");
    case 188: return _T("Command Timeout");
    case 189: return _T("High Fly Writes");
    case 190: return _T("Airflow Temperature");
    case 192: return _T("Power-Off Retract Count");
    case 193: return _T("Load Cycle Count");
    case 194: return _T("Temperature");
    case 196: return _T("Reallocated Event Count");
    case 197: return _T("Current Pending Sector Count");
    case 198: return _T("Uncorrectable Sector Count");
    case 199: return _T("UDMA CRC Error Count");
    case 200: return _T("Multi-Zone Error Rate");
    case 233: return _T("Media Wearout Indicator (SSD)");
    case 241: return _T("Total LBAs Written (SSD)");
    case 242: return _T("Total LBAs Read (SSD)");
    default:
    {
        CString s;
        s.Format(_T("Attribute 0x%02X"), id);
        return s;
    }
    }
}

bool CSmartDataReader::IsCriticalAttribute(BYTE id)
{
    switch (id)
    {
    case 5: case 187: case 196: case 197: case 198:
        return true;
    default:
        return false;
    }
}

bool CSmartDataReader::ReadSmartAttributes(HANDLE h, std::vector<SmartAttribute>& attributesOut)
{
    GetVersionOutParams ver = {};
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(h, DFP_GET_VERSION, nullptr, 0, &ver, sizeof(ver), &bytesReturned, nullptr))
        return false; // Not supported by this driver/controller (common for NVMe, USB, some RAID)

    if (!(ver.fCapabilities & CAP_SMART_CMD))
        return false;

    SendCmdInParams in = {};
    in.cBufferSize = sizeof(in.bBuffer);
    in.irDriveRegs.bFeaturesReg = SMART_READ_DATA;
    in.irDriveRegs.bSectorCountReg = 1;
    in.irDriveRegs.bSectorNumberReg = 1;
    in.irDriveRegs.bCylLowReg = SMART_CYL_LOW;
    in.irDriveRegs.bCylHighReg = SMART_CYL_HI;
    in.irDriveRegs.bDriveHeadReg = 0xA0;
    in.irDriveRegs.bCommandReg = SMART_CMD;
    in.bDriveNumber = 0;

    SendCmdOutParams out = {};
    DWORD outBytes = 0;
    BOOL ok = DeviceIoControl(
        h, DFP_RECEIVE_DRIVE_DATA,
        &in, sizeof(in),
        &out, sizeof(out),
        &outBytes, nullptr);

    if (!ok)
        return false;

    // SMART attribute table: 2-byte revision, then up to 30 x 12-byte entries.
    const BYTE* data = out.bBuffer;
    for (int i = 0; i < 30; ++i)
    {
        const BYTE* entry = data + 2 + i * 12;
        BYTE id = entry[0];
        if (id == 0)
            continue; // Unused slot

        SmartAttribute attr;
        attr.id = id;
        attr.name = AttributeName(id);
        attr.currentValue = entry[3];
        attr.worstValue = entry[4];

        ULONGLONG raw = 0;
        for (int b = 0; b < 6; ++b)
            raw |= (ULONGLONG)entry[5 + b] << (8 * b);
        attr.rawValue = raw;
        attr.isCriticalWarning = IsCriticalAttribute(id) && raw > 0;

        attributesOut.push_back(attr);
    }

    return true;
}

SmartHealthResult CSmartDataReader::ReadHealth(const CString& devicePath)
{
    SmartHealthResult result;

    HANDLE h = CreateFile(
        devicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        result.unsupportedReason = _T("Could not open the drive to query health data.");
        return result;
    }

    bool failurePredicted = false;
    result.hasPredictFailureFlag = QueryPredictFailure(h, failurePredicted);
    result.failurePredicted = failurePredicted;

    std::vector<SmartAttribute> attrs;
    result.hasAttributeTable = ReadSmartAttributes(h, attrs);
    result.attributes = attrs;

    for (const auto& a : attrs)
    {
        if (a.id == 194 || a.id == 190)
        {
            // Convention (not guaranteed by the spec, but near-universal in
            // practice): the current temperature in Celsius is the low byte.
            result.temperatureCelsius = static_cast<int>(a.rawValue & 0xFF);
            break;
        }
    }

    CloseHandle(h);

    result.querySucceeded = result.hasPredictFailureFlag || result.hasAttributeTable;
    if (!result.querySucceeded)
    {
        result.unsupportedReason =
            _T("This drive/controller did not respond to either supported health-query method ")
            _T("(common for NVMe drives, many USB enclosures, and some RAID controllers).");
    }

    return result;
}
