// Original Build Date : 13/03/2012
// Author  : Ari Sohandri Putra
// GitHub  : https://github.com/arisohandriputra
// Project : LLHD Monitor - Low-Level HDD Monitor
// File    : smart.h - S.M.A.R.T. structures, constants, and function declarations

#pragma once
#ifndef SMART_H
#define SMART_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

#ifndef SMART_GET_VERSION
#define SMART_GET_VERSION           CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)
#endif
#ifndef SMART_SEND_DRIVE_COMMAND
#define SMART_SEND_DRIVE_COMMAND    CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif
#ifndef SMART_RCV_DRIVE_DATA
#define SMART_RCV_DRIVE_DATA        CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif

#define ATAPI_ID_CMD                0xA1
#define ID_CMD                      0xEC
#define SMART_CMD                   0xB0
#define SMART_CYL_LOW               0x4F
#define SMART_CYL_HI                0xC2
#define SMART_READ_DATA             0xD0
#define SMART_READ_THRESHOLDS       0xD1
#define SMART_ENABLE                0xD8
#define SMART_RETURN_STATUS         0xDA

#define READ_ATTRIBUTE_BUFFER_SIZE  512
#define IDENTIFY_BUFFER_SIZE        512
#define READ_THRESHOLD_BUFFER_SIZE  512

#define SAT_ATA_PASSTHROUGH_12      0xA1
#define SAT_ATA_PASSTHROUGH_16      0x85

#define SAT_PROTO_NON_DATA          (3 << 1)
#define SAT_PROTO_PIO_IN            (4 << 1)
#define SAT_PROTO_PIO_OUT           (5 << 1)

#define SAT_FLAGS_TDIR_FROM_DEV     0x08
#define SAT_FLAGS_BYTE_BLOCK        0x04
#define SAT_FLAGS_TLEN_SECTOR_CNT   0x02

typedef enum _DRIVE_TYPE {
    DRIVE_TYPE_UNKNOWN  = 0,
    DRIVE_TYPE_HDD      = 1,
    DRIVE_TYPE_SSD_SATA = 2,
    DRIVE_TYPE_NVME     = 3,
    DRIVE_TYPE_USB      = 4,
    DRIVE_TYPE_M2_SATA  = 5
} DRIVE_TYPE;

#pragma pack(push, 1)
typedef struct _NVME_HEALTH_INFO_LOG {
    BYTE    CriticalWarning;
    BYTE    CompositeTemperature[2];
    BYTE    AvailableSpare;
    BYTE    AvailableSpareThreshold;
    BYTE    PercentageUsed;
    BYTE    EnduranceGroupSummary;
    BYTE    Reserved7[25];
    BYTE    DataUnitsRead[16];
    BYTE    DataUnitsWritten[16];
    BYTE    HostReadCommands[16];
    BYTE    HostWriteCommands[16];
    BYTE    ControllerBusyTime[16];
    BYTE    PowerCycles[16];
    BYTE    PowerOnHours[16];
    BYTE    UnsafeShutdowns[16];
    BYTE    MediaErrors[16];
    BYTE    NumErrLogEntries[16];
    ULONG   WarningCompTempTime;
    ULONG   CriticalCompTempTime;
    USHORT  TempSensor[8];
    ULONG   ThermalMgmtTemp1TransCnt;
    ULONG   ThermalMgmtTemp2TransCnt;
    ULONG   TotalTimeThermalMgmtTemp1;
    ULONG   TotalTimeThermalMgmtTemp2;
    BYTE    Reserved232[280];
} NVME_HEALTH_INFO_LOG;
#pragma pack(pop)

#define NVME_CRIT_WARN_SPARE_BELOW_THRESH   0x01
#define NVME_CRIT_WARN_TEMP_THRESHOLD       0x02
#define NVME_CRIT_WARN_RELIABILITY_DEGRADED 0x04
#define NVME_CRIT_WARN_READ_ONLY            0x08
#define NVME_CRIT_WARN_VOLATILE_MEM_BACKUP  0x10

#pragma pack(push, 1)

typedef struct _DRIVE_COMMAND_STRUCT {
    IDEREGS irDriveRegs;
    BYTE    bDriveNumber;
    BYTE    bReserved[3];
    DWORD   dwBufferSize;
    BYTE    bBuffer[1];
} DRIVE_COMMAND_STRUCT;

typedef struct _DRIVE_SMART_READ_DATA {
    SENDCMDINPARAMS   cip;
    BYTE              bBuffer[READ_ATTRIBUTE_BUFFER_SIZE];
} DRIVE_SMART_READ_DATA;

typedef struct _SMART_ATTRIBUTE {
    BYTE  bAttrID;
    WORD  wStatusFlags;
    BYTE  bAttrValue;
    BYTE  bWorstValue;
    BYTE  bRawValue[6];
    BYTE  bReserved;
} SMART_ATTRIBUTE;

typedef struct _SMART_THRESHOLD {
    BYTE  bAttrID;
    BYTE  bThresholdValue;
    BYTE  bReserved[10];
} SMART_THRESHOLD;

typedef struct _SMART_ATTRIBUTE_DATA {
    WORD            wRevisionNumber;
    SMART_ATTRIBUTE stAttributes[30];
} SMART_ATTRIBUTE_DATA;

typedef struct _SMART_THRESHOLD_DATA {
    WORD            wRevisionNumber;
    SMART_THRESHOLD stThresholds[30];
} SMART_THRESHOLD_DATA;

typedef struct _DRIVE_INFO {
    char        szModel[41];
    char        szSerial[21];
    char        szFirmware[9];
    DWORD       dwCapacityMB;
    BOOL        bSMART_Supported;
    BOOL        bSMART_Enabled;
    int         nHealthPercent;
    int         nPerformancePercent;
    BOOL        bPredictFailure;
    int         nDriveIndex;
    BOOL        bIsUSB;
    DRIVE_TYPE  eType;
    int         nTemperatureC;
    BOOL        bIsNVMe;
    int         nReadSpeedMBs;
    int         nWriteSpeedMBs;
    NVME_HEALTH_INFO_LOG nvmeHealth;
    SMART_ATTRIBUTE_DATA attrData;
    SMART_THRESHOLD_DATA threshData;
} DRIVE_INFO;

#pragma pack(pop)

typedef struct _ATTR_NAME {
    BYTE  bID;
    const char* szName;
    BOOL  bCritical;
} ATTR_NAME;

static const ATTR_NAME g_AttrNames[] = {
    { 0x01, "Raw Read Error Rate",          TRUE  },
    { 0x02, "Throughput Performance",       FALSE },
    { 0x03, "Spin-Up Time",                 FALSE },
    { 0x04, "Start/Stop Count",             FALSE },
    { 0x05, "Reallocated Sectors Count",    TRUE  },
    { 0x06, "Read Channel Margin",          FALSE },
    { 0x07, "Seek Error Rate",              FALSE },
    { 0x08, "Seek Time Performance",        FALSE },
    { 0x09, "Power-On Hours",               FALSE },
    { 0x0A, "Spin Retry Count",             TRUE  },
    { 0x0B, "Calibration Retry Count",      FALSE },
    { 0x0C, "Power Cycle Count",            FALSE },
    { 0x0D, "Soft Read Error Rate",         FALSE },
    { 0x16, "Current Helium Level",         FALSE },
    { 0xAA, "Available Reserved Space",     FALSE },
    { 0xA0, "Unsafe Shutdown Count",        FALSE },
    { 0xA1, "Used Reserved Block Count Total", FALSE },
    { 0xA2, "Used Reserved Block Count Worst", FALSE },
    { 0xA3, "Initial Bad Block Count",      FALSE },
    { 0xA4, "Total Erase Count",            FALSE },
    { 0xA5, "Max Erase Count",              FALSE },
    { 0xA6, "Min Erase Count",              FALSE },
    { 0xA7, "Average Erase Count",          FALSE },
    { 0xA8, "Max Erase Count of Spec",      FALSE },
    { 0xA9, "Remaining Life Percentage",    FALSE },
    { 0xAB, "Program Fail Count",           TRUE  },
    { 0xAC, "Erase Fail Count",             TRUE  },
    { 0xAD, "Wear Leveling Count",          FALSE },
    { 0xAE, "Unexpected Power Loss",        FALSE },
    { 0xAF, "Power Loss Protection Fail",   TRUE  },
    { 0xB0, "Erase Fail Count (Chip)",      TRUE  },
    { 0xB1, "Wear Range Delta",             FALSE },
    { 0xB2, "Used Reserved Block Count",    FALSE },
    { 0xB3, "Used Reserved Block Count Total", FALSE },
    { 0xB4, "Unused Reserved Block Count Total", FALSE },
    { 0xB5, "Program Fail Count Total",     TRUE  },
    { 0xB6, "Erase Fail Count Total",       TRUE  },
    { 0xB7, "SATA Downshift Error Count",   TRUE  },
    { 0xB8, "End-to-End Error",             TRUE  },
    { 0xB9, "Head Stability",               FALSE },
    { 0xBA, "Induced Op-Vibration Detection", FALSE },
    { 0xBB, "Uncorrectable ECC Error",      TRUE  },
    { 0xBC, "Command Timeout",              TRUE  },
    { 0xBD, "High Fly Writes",              FALSE },
    { 0xBE, "Airflow Temperature",          FALSE },
    { 0xBF, "G-Sense Error Rate",           FALSE },
    { 0xC0, "Power-Off Retract Count",      FALSE },
    { 0xC1, "Load/Unload Cycle Count",      FALSE },
    { 0xC2, "Temperature",                  FALSE },
    { 0xC3, "Hardware ECC Recovered",       FALSE },
    { 0xC4, "Reallocation Event Count",     TRUE  },
    { 0xC5, "Current Pending Sectors",      TRUE  },
    { 0xC6, "Uncorrectable Sectors",        TRUE  },
    { 0xC7, "UltraDMA CRC Error Count",     TRUE  },
    { 0xC8, "Write Error Rate",             TRUE  },
    { 0xC9, "Soft Read Error Rate",         FALSE },
    { 0xCA, "Data Address Mark Errors",     TRUE  },
    { 0xCB, "Run Out Cancel",               FALSE },
    { 0xCC, "Soft ECC Correction",          FALSE },
    { 0xCD, "Thermal Asperity Rate",        FALSE },
    { 0xCE, "Flying Height",                FALSE },
    { 0xCF, "Spin High Current",            FALSE },
    { 0xD0, "Spin Buzz",                    FALSE },
    { 0xD1, "Offline Seek Performance",     FALSE },
    { 0xD3, "Vibration During Write",       FALSE },
    { 0xD4, "Shock During Write",           FALSE },
    { 0xDC, "Disk Shift",                   FALSE },
    { 0xDD, "G-Sense Error Rate Alt",       FALSE },
    { 0xDE, "Loaded Hours",                 FALSE },
    { 0xDF, "Load/Unload Retry Count",      FALSE },
    { 0xE0, "Load Friction",                FALSE },
    { 0xE1, "Load/Unload Cycle Count Alt",  FALSE },
    { 0xE2, "Load In-Time",                 FALSE },
    { 0xE3, "Torque Amplification Count",   FALSE },
    { 0xE4, "Power-Off Retract Cycle",      FALSE },
    { 0xE6, "GMR Head Amplitude",           FALSE },
    { 0xE7, "SSD Life Left / Temperature",  FALSE },
    { 0xE8, "Available Reserved Space",     FALSE },
    { 0xE9, "NAND Writes (GB) / Media Wearout", FALSE },
    { 0xEA, "Average Erase Count / Total Writes", FALSE },
    { 0xEB, "Good Block Count / NAND Endurance", FALSE },
    { 0xF0, "Head Flying Hours",            FALSE },
    { 0xF1, "Total LBAs Written",           FALSE },
    { 0xF2, "Total LBAs Read",              FALSE },
    { 0xF3, "Total LBAs Written Expanded",  FALSE },
    { 0xF4, "Total LBAs Read Expanded",     FALSE },
    { 0xF9, "NAND Writes (GiB)",            FALSE },
    { 0xFA, "Read Error Retry Rate",        FALSE },
    { 0xFB, "Minimum Spares Remaining",     FALSE },
    { 0xFC, "Newly Added Bad Flash Block",  TRUE  },
    { 0xFE, "Free Fall Protection",         FALSE },
    { 0x00, NULL,                           FALSE }
};

const char* GetAttrName(BYTE bID);
BOOL        IsAttrCritical(BYTE bID);
const char* GetDriveTypeName(DRIVE_TYPE eType);

BOOL  OpenDrive(int nDrive, HANDLE* phDrive);
BOOL  IsUSBDrive(HANDLE hDrive);
BOOL  EnableSMART(HANDLE hDrive, int nDrive);
BOOL  GetIdentifyData(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetIdentifyDataUSB(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetIdentifyDataSAT(HANDLE hDrive, DRIVE_INFO* pInfo);

BOOL  GetSMARTAttributes(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTThresholds(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTAttributesSAT(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTThresholdsSAT(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTViaStorageProtocol(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTPredictFailure(HANDLE hDrive, int nDrive, BOOL* pbFail);

BOOL  IsNVMeDrive(HANDLE hDrive);
BOOL  GetNVMeHealthLog(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetNVMeHealthLogEx(HANDLE hDrive, DRIVE_INFO* pInfo);
int   CalculateHealthNVMe(DRIVE_INFO* pInfo);

DRIVE_TYPE DetectDriveType(HANDLE hDrive, DRIVE_INFO* pInfo);

int   CalculateHealth(DRIVE_INFO* pInfo);
int   CalculatePerformance(DRIVE_INFO* pInfo);
int   ScanDrives(DRIVE_INFO* pDrives, int nMaxDrives);
void  FormatSize(DWORD dwMB, char* szBuf, int nBufLen);
DWORD GetRawValue(BYTE* pRaw);
int   MeasureReadSpeed(int nDriveIndex);
int   MeasureWriteSpeed(int nDriveIndex);

#endif