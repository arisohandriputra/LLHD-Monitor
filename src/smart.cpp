// Original Build Date : 13/03/2012
// Author  : Ari Sohandri Putra
// GitHub  : https://github.com/arisohandriputra
// Project : LLHD Monitor - Low-Level HDD Monitor
// File    : smart.cpp - S.M.A.R.T. and NVMe drive data acquisition

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "smart.h"
#pragma pack(push, 1)
typedef struct _SAT_PASSTHROUGH_BUF {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG    Filler;
    BYTE     SenseBuf[32];
} SAT_PASSTHROUGH_BUF;
#pragma pack(pop)

#define SAT_SENSEBUF_OFFSET  (sizeof(SCSI_PASS_THROUGH_DIRECT) + sizeof(ULONG))
#ifndef StorageDeviceProtocolSpecificProperty
#define StorageDeviceProtocolSpecificProperty ((STORAGE_PROPERTY_ID)49)
#endif
#define MY_ProtocolTypeUnknown  0
#define MY_ProtocolTypeScsi     1
#define MY_ProtocolTypeAta      2
#define MY_ProtocolTypeNvme     3
#define MY_ProtocolTypeSd       4
#define MY_NVMeDataTypeUnknown      0
#define MY_NVMeDataTypeIdentify     1
#define MY_NVMeDataTypeLogPage      2
#define MY_NVMeDataTypeFeature      3
#define NVME_LOG_PAGE_HEALTH_INFO   0x02
#define MY_AtaDataTypeUnknown           0
#define MY_AtaDataTypeIdentify          1
#define MY_AtaDataTypeSmartData         2
#define MY_AtaDataTypeSmartThresholds   3

#pragma pack(push, 1)
typedef struct _MY_STORAGE_PROTOCOL_SPECIFIC_DATA {
    ULONG ProtocolType;
    ULONG DataType;
    ULONG ProtocolDataRequestValue;
    ULONG ProtocolDataRequestSubValue;
    ULONG ProtocolDataOffset;
    ULONG ProtocolDataLength;
    ULONG FixedProtocolReturnData;
    ULONG Reserved[3];
} MY_STORAGE_PROTOCOL_SPECIFIC_DATA;

typedef struct _MY_STORAGE_PROTOCOL_QUERY {
    ULONG PropertyId;
    ULONG QueryType;
    MY_STORAGE_PROTOCOL_SPECIFIC_DATA ProtocolSpecific;
} MY_STORAGE_PROTOCOL_QUERY;

typedef struct _MY_PROTOCOL_DATA_DESCRIPTOR {
    ULONG Version;
    ULONG Size;
    MY_STORAGE_PROTOCOL_SPECIFIC_DATA ProtocolSpecificData;
    BYTE  Data[512];
} MY_PROTOCOL_DATA_DESCRIPTOR;
#pragma pack(pop)

static void TrimStr(char* sz)
{
    int len, start, j;
    if (!sz || !sz[0]) return;
    len = (int)strlen(sz);
    while (len > 0 && sz[len - 1] == ' ') sz[--len] = '\0';
    start = 0;
    while (sz[start] == ' ') start++;
    if (start > 0) {
        for (j = 0; sz[start + j] != '\0'; j++)
            sz[j] = sz[start + j];
        sz[j] = '\0';
    }
}

static void SwapATAString(char* szDst, const WORD* pSrc, int nWords)
{
    int i;
    for (i = 0; i < nWords; i++) {
        szDst[i * 2]     = (char)(pSrc[i] >> 8);
        szDst[i * 2 + 1] = (char)(pSrc[i] & 0xFF);
    }
    szDst[nWords * 2] = '\0';
    int len = (int)strlen(szDst);
    while (len > 0 && szDst[len - 1] == ' ') {
        szDst[--len] = '\0';
    }
    int start = 0;
    while (szDst[start] == ' ') start++;
    if (start > 0) {
        int j;
        for (j = 0; szDst[start + j] != '\0'; j++)
            szDst[j] = szDst[start + j];
        szDst[j] = '\0';
    }
}

static unsigned __int64 NVMeRead128Lo(const BYTE* p)
{
    unsigned __int64 lo = 0;
    int i;
    for (i = 7; i >= 0; i--) {
        lo = (lo << 8) | p[i];
    }
    return lo;
}

static WORD ReadLE16(const BYTE* p)
{
    return (WORD)p[0] | ((WORD)p[1] << 8);
}

const char* GetAttrName(BYTE bID)
{
    int i = 0;
    while (g_AttrNames[i].szName != NULL) {
        if (g_AttrNames[i].bID == bID)
            return g_AttrNames[i].szName;
        i++;
    }
    return "Unknown Attribute";
}

BOOL IsAttrCritical(BYTE bID)
{
    int i = 0;
    while (g_AttrNames[i].szName != NULL) {
        if (g_AttrNames[i].bID == bID)
            return g_AttrNames[i].bCritical;
        i++;
    }
    return FALSE;
}

const char* GetDriveTypeName(DRIVE_TYPE eType)
{
    switch (eType) {
    case DRIVE_TYPE_HDD:      return "HDD";
    case DRIVE_TYPE_SSD_SATA: return "SSD (SATA)";
    case DRIVE_TYPE_NVME:     return "NVMe SSD";
    case DRIVE_TYPE_USB:      return "USB/External";
    case DRIVE_TYPE_M2_SATA:  return "SSD";
    default:                  return "Unknown";
    }
}

BOOL OpenDrive(int nDrive, HANDLE* phDrive)
{
    char szPath[32];
    _snprintf(szPath, sizeof(szPath), "\\\\.\\PhysicalDrive%d", nDrive);

    *phDrive = CreateFileA(
        szPath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL
    );

    if (*phDrive == INVALID_HANDLE_VALUE) {
        *phDrive = CreateFileA(
            szPath, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL
        );
    }

    return (*phDrive != INVALID_HANDLE_VALUE);
}

static BOOL IsUSBDrive(HANDLE hDrive)
{
    STORAGE_PROPERTY_QUERY spq;
    ZeroMemory(&spq, sizeof(spq));
    spq.PropertyId = StorageAdapterProperty;
    spq.QueryType  = PropertyStandardQuery;

    BYTE outBuf[512];
    ZeroMemory(outBuf, sizeof(outBuf));
    DWORD dwBytes = 0;

    if (!DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
                         &spq, sizeof(spq), outBuf, sizeof(outBuf),
                         &dwBytes, NULL))
        return FALSE;

    if (dwBytes < 9) return FALSE;
    BYTE bBusType = outBuf[8];
    return (bBusType == 7);
}

BOOL IsNVMeDrive(HANDLE hDrive)
{
    STORAGE_PROPERTY_QUERY spq;
    ZeroMemory(&spq, sizeof(spq));
    spq.PropertyId = StorageAdapterProperty;
    spq.QueryType  = PropertyStandardQuery;

    BYTE outBuf[512];
    ZeroMemory(outBuf, sizeof(outBuf));
    DWORD dwBytes = 0;

    if (!DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
                         &spq, sizeof(spq), outBuf, sizeof(outBuf),
                         &dwBytes, NULL))
        return FALSE;

    if (dwBytes < 9) return FALSE;

    BYTE bBusType = outBuf[8];
    if (bBusType == 17)
        return TRUE;

    BYTE nvmeBuf[sizeof(MY_STORAGE_PROTOCOL_QUERY) + sizeof(NVME_HEALTH_INFO_LOG) + 64];
    ZeroMemory(nvmeBuf, sizeof(nvmeBuf));
    MY_STORAGE_PROTOCOL_QUERY* pQ = (MY_STORAGE_PROTOCOL_QUERY*)nvmeBuf;
    DWORD dwBytesRet = 0;

    pQ->PropertyId = (ULONG)StorageDeviceProtocolSpecificProperty;
    pQ->QueryType  = 0;
    pQ->ProtocolSpecific.ProtocolType              = MY_ProtocolTypeNvme;
    pQ->ProtocolSpecific.DataType                  = MY_NVMeDataTypeLogPage;
    pQ->ProtocolSpecific.ProtocolDataRequestValue  = NVME_LOG_PAGE_HEALTH_INFO;
    pQ->ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    pQ->ProtocolSpecific.ProtocolDataOffset        = sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    pQ->ProtocolSpecific.ProtocolDataLength        = sizeof(NVME_HEALTH_INFO_LOG);

    BOOL bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        nvmeBuf, sizeof(nvmeBuf), nvmeBuf, sizeof(nvmeBuf), &dwBytesRet, NULL);

    return (bOK && dwBytesRet >= 64);
}

static BOOL GetNVMeIdentify(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BYTE bigBuf[sizeof(MY_STORAGE_PROTOCOL_QUERY) + 4096 + 64];
    ZeroMemory(bigBuf, sizeof(bigBuf));
    MY_STORAGE_PROTOCOL_QUERY* pQ = (MY_STORAGE_PROTOCOL_QUERY*)bigBuf;
    DWORD dwBytes = 0;

    pQ->PropertyId = (ULONG)StorageDeviceProtocolSpecificProperty;
    pQ->QueryType  = 0;
    pQ->ProtocolSpecific.ProtocolType              = MY_ProtocolTypeNvme;
    pQ->ProtocolSpecific.DataType                  = MY_NVMeDataTypeIdentify;
    pQ->ProtocolSpecific.ProtocolDataRequestValue  = 1;
    pQ->ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    pQ->ProtocolSpecific.ProtocolDataOffset        = sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    pQ->ProtocolSpecific.ProtocolDataLength        = 4096;

    BOOL bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        bigBuf, sizeof(bigBuf),
        bigBuf, sizeof(bigBuf),
        &dwBytes, NULL);

    if (!bOK || dwBytes < 128)
        return FALSE;

    BYTE* pData = bigBuf + sizeof(ULONG) + sizeof(ULONG) + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);

    char szSerial[21];
    ZeroMemory(szSerial, sizeof(szSerial));
    memcpy(szSerial, pData + 4, 20);
    szSerial[20] = '\0';
    int n = 20;
    while (n > 0 && (szSerial[n-1] == ' ' || szSerial[n-1] == '\0')) {
        szSerial[--n] = '\0';
    }
    strncpy(pInfo->szSerial, szSerial, 20);
    pInfo->szSerial[20] = '\0';
    TrimStr(pInfo->szSerial);
    char szModel[41];
    ZeroMemory(szModel, sizeof(szModel));
    memcpy(szModel, pData + 24, 40);
    szModel[40] = '\0';
    n = 40;
    while (n > 0 && (szModel[n-1] == ' ' || szModel[n-1] == '\0')) {
        szModel[--n] = '\0';
    }
    strncpy(pInfo->szModel, szModel, 40);
    pInfo->szModel[40] = '\0';
    TrimStr(pInfo->szModel);
    char szFW[9];
    ZeroMemory(szFW, sizeof(szFW));
    memcpy(szFW, pData + 64, 8);
    szFW[8] = '\0';
    n = 8;
    while (n > 0 && (szFW[n-1] == ' ' || szFW[n-1] == '\0')) {
        szFW[--n] = '\0';
    }
    strncpy(pInfo->szFirmware, szFW, 8);
    pInfo->szFirmware[8] = '\0';

    return (pInfo->szModel[0] != '\0' || pInfo->szSerial[0] != '\0');
}

BOOL GetNVMeHealthLog(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BYTE buf[sizeof(MY_STORAGE_PROTOCOL_QUERY) + sizeof(NVME_HEALTH_INFO_LOG) + 64];
    ZeroMemory(buf, sizeof(buf));
    MY_STORAGE_PROTOCOL_QUERY* pQ = (MY_STORAGE_PROTOCOL_QUERY*)buf;
    DWORD dwBytes = 0;

    pQ->PropertyId = (ULONG)StorageDeviceProtocolSpecificProperty;
    pQ->QueryType  = 0;
    pQ->ProtocolSpecific.ProtocolType              = MY_ProtocolTypeNvme;
    pQ->ProtocolSpecific.DataType                  = MY_NVMeDataTypeLogPage;
    pQ->ProtocolSpecific.ProtocolDataRequestValue  = NVME_LOG_PAGE_HEALTH_INFO;
    pQ->ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    pQ->ProtocolSpecific.ProtocolDataOffset        = sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    pQ->ProtocolSpecific.ProtocolDataLength        = sizeof(NVME_HEALTH_INFO_LOG);

    BOOL bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        buf, sizeof(buf), buf, sizeof(buf), &dwBytes, NULL);

    if (!bOK || dwBytes < (ULONG)(sizeof(ULONG) * 2 + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA) + 8))
        return FALSE;

    BYTE* pData = buf + sizeof(ULONG) + sizeof(ULONG) + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);

    BOOL bAllZero = TRUE;
    int k;
    for (k = 0; k < 16; k++) {
        if (pData[k] != 0x00) { bAllZero = FALSE; break; }
    }
    if (bAllZero) return FALSE;

    memcpy(&pInfo->nvmeHealth, pData, sizeof(NVME_HEALTH_INFO_LOG));
    pInfo->bIsNVMe = TRUE;
    pInfo->bSMART_Supported = TRUE;
    pInfo->bSMART_Enabled   = TRUE;

    WORD wTempK = ReadLE16(pInfo->nvmeHealth.CompositeTemperature);
    if (wTempK > 273 && wTempK < 400) {
        pInfo->nTemperatureC = (int)wTempK - 273;
    }

    return TRUE;
}

static BOOL GetNVMeCapacity(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    DISK_GEOMETRY_EX geo;
    ZeroMemory(&geo, sizeof(geo));
    DWORD dwBytes = 0;

    if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        NULL, 0, &geo, sizeof(geo), &dwBytes, NULL)) {
        pInfo->dwCapacityMB = (DWORD)(geo.DiskSize.QuadPart / (1024 * 1024));
        return TRUE;
    }
    return FALSE;
}

static BOOL GetNVMeHealthLogFallback(HANDLE hDrive, DRIVE_INFO* pInfo)
{
#define NVME_FALLBACK_BUF_SIZE 4096
    BYTE* pBuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, NVME_FALLBACK_BUF_SIZE);
    if (!pBuf) return FALSE;

    MY_STORAGE_PROTOCOL_QUERY* pq = (MY_STORAGE_PROTOCOL_QUERY*)pBuf;
    pq->PropertyId = (ULONG)StorageDeviceProtocolSpecificProperty;
    pq->QueryType  = 0;
    pq->ProtocolSpecific.ProtocolType             = MY_ProtocolTypeNvme;
    pq->ProtocolSpecific.DataType                 = MY_NVMeDataTypeLogPage;
    pq->ProtocolSpecific.ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
    pq->ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    pq->ProtocolSpecific.ProtocolDataOffset       = sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    pq->ProtocolSpecific.ProtocolDataLength       = sizeof(NVME_HEALTH_INFO_LOG);

    DWORD dwBytes = 0;
    BOOL bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        pBuf, NVME_FALLBACK_BUF_SIZE,
        pBuf, NVME_FALLBACK_BUF_SIZE,
        &dwBytes, NULL);

    if (!bOK || dwBytes < (DWORD)(sizeof(ULONG)*2 + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA) + 8)) {
        HeapFree(GetProcessHeap(), 0, pBuf);
        return FALSE;
    }

    BYTE* pData = pBuf + sizeof(ULONG) + sizeof(ULONG) + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    BOOL bAllZero = TRUE;
    int k;
    for (k = 0; k < 16; k++) {
        if (pData[k] != 0x00) { bAllZero = FALSE; break; }
    }
    if (bAllZero) {
        HeapFree(GetProcessHeap(), 0, pBuf);
        return FALSE;
    }

    memcpy(&pInfo->nvmeHealth, pData, sizeof(NVME_HEALTH_INFO_LOG));
    pInfo->bIsNVMe = TRUE;
    pInfo->bSMART_Supported = TRUE;
    pInfo->bSMART_Enabled   = TRUE;

    WORD wTempK = (WORD)(pInfo->nvmeHealth.CompositeTemperature[0] |
                        ((WORD)pInfo->nvmeHealth.CompositeTemperature[1] << 8));
    if (wTempK > 273 && wTempK < 400)
        pInfo->nTemperatureC = (int)wTempK - 273;

    HeapFree(GetProcessHeap(), 0, pBuf);
    return TRUE;
#undef NVME_FALLBACK_BUF_SIZE
}

static BYTE GetStorageBusType(HANDLE hDrive)
{
    STORAGE_PROPERTY_QUERY spq;
    ZeroMemory(&spq, sizeof(spq));
    spq.PropertyId = StorageAdapterProperty;
    spq.QueryType  = PropertyStandardQuery;

    BYTE outBuf[512];
    ZeroMemory(outBuf, sizeof(outBuf));
    DWORD dwBytes = 0;

    if (!DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
                         &spq, sizeof(spq), outBuf, sizeof(outBuf),
                         &dwBytes, NULL))
        return 0;

    if (dwBytes < 9) return 0;
    return outBuf[8]; 
}

static BOOL GetNVMeInfo(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BOOL bOK = FALSE;

    if (!GetNVMeIdentify(hDrive, pInfo)) {
        STORAGE_PROPERTY_QUERY spq;
        ZeroMemory(&spq, sizeof(spq));
        spq.PropertyId = StorageDeviceProperty;
        spq.QueryType  = PropertyStandardQuery;

        BYTE outBuf[1024];
        ZeroMemory(outBuf, sizeof(outBuf));
        DWORD dwBytes = 0;

        if (DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
                            &spq, sizeof(spq), outBuf, sizeof(outBuf),
                            &dwBytes, NULL)) {
            STORAGE_DEVICE_DESCRIPTOR* pDesc = (STORAGE_DEVICE_DESCRIPTOR*)outBuf;

            if (pDesc->ProductIdOffset && pDesc->ProductIdOffset < sizeof(outBuf)) {
                const char* szProd = (const char*)outBuf + pDesc->ProductIdOffset;
                strncpy(pInfo->szModel, szProd, 40);
                pInfo->szModel[40] = '\0';
                TrimStr(pInfo->szModel);
            }
            if (pDesc->SerialNumberOffset && pDesc->SerialNumberOffset < sizeof(outBuf)) {
                const char* szSer = (const char*)outBuf + pDesc->SerialNumberOffset;
                strncpy(pInfo->szSerial, szSer, 20);
                pInfo->szSerial[20] = '\0';
                TrimStr(pInfo->szSerial);
            }
            if (pDesc->ProductRevisionOffset && pDesc->ProductRevisionOffset < sizeof(outBuf)) {
                const char* szRev = (const char*)outBuf + pDesc->ProductRevisionOffset;
                strncpy(pInfo->szFirmware, szRev, 8);
                pInfo->szFirmware[8] = '\0';
            }
            bOK = TRUE;
        }
    } else {
        bOK = TRUE;
    }

    if (!bOK) return FALSE;

    GetNVMeCapacity(hDrive, pInfo);

    if (!GetNVMeHealthLog(hDrive, pInfo))
        GetNVMeHealthLogFallback(hDrive, pInfo);

    pInfo->eType = DRIVE_TYPE_NVME;
    pInfo->bIsNVMe = TRUE;

    return TRUE;
}

BOOL GetNVMeHealthLogEx(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    if (GetNVMeHealthLog(hDrive, pInfo))
        return TRUE;
    return GetNVMeHealthLogFallback(hDrive, pInfo);
}

int CalculateHealthNVMe(DRIVE_INFO* pInfo)
{
    if (!pInfo->bIsNVMe || !pInfo->bSMART_Supported)
        return -1;

    NVME_HEALTH_INFO_LOG* pLog = &pInfo->nvmeHealth;

    if (pLog->CriticalWarning & NVME_CRIT_WARN_READ_ONLY)
        return 0;
    int nPercentUsed = (int)pLog->PercentageUsed;
    int nHealth = 100 - nPercentUsed;
    if (nHealth < 0) nHealth = 0;
    int nSpare  = (int)pLog->AvailableSpare;
    int nThresh = (int)pLog->AvailableSpareThreshold;
    if (nSpare < nThresh) {
        int nDeficit = nThresh - nSpare;
        int nPenalty = nDeficit * 2;
        if (nPenalty > 30) nPenalty = 30;
        nHealth -= nPenalty;
    }

    if (pLog->CriticalWarning & NVME_CRIT_WARN_RELIABILITY_DEGRADED)
        nHealth -= 25;

    if (pLog->CriticalWarning & NVME_CRIT_WARN_SPARE_BELOW_THRESH)
        nHealth -= 10;

    if (pLog->CriticalWarning & NVME_CRIT_WARN_TEMP_THRESHOLD)
        nHealth -= 5;

    if (nHealth < 0)   nHealth = 0;
    if (nHealth > 100) nHealth = 100;

    return nHealth;
}

DRIVE_TYPE DetectDriveType(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    if (pInfo->bIsNVMe)
        return DRIVE_TYPE_NVME;

    if (pInfo->bIsUSB)
        return DRIVE_TYPE_USB;

    BYTE bBusType = GetStorageBusType(hDrive);

    STORAGE_PROPERTY_QUERY spq2;
    ZeroMemory(&spq2, sizeof(spq2));
    spq2.PropertyId = StorageDeviceProperty;
    spq2.QueryType  = PropertyStandardQuery;
    BYTE devBuf[256];
    ZeroMemory(devBuf, sizeof(devBuf));
    DWORD dwDevBytes = 0;
    BOOL bHasDevProp = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        &spq2, sizeof(spq2), devBuf, sizeof(devBuf), &dwDevBytes, NULL);
    BYTE bMediaType = bHasDevProp && dwDevBytes >= 8 ? devBuf[6] : 0; 

    BYTE inBuf[sizeof(SENDCMDINPARAMS) - 1 + IDENTIFY_BUFFER_SIZE];
    BYTE outBuf[sizeof(SENDCMDOUTPARAMS) - 1 + IDENTIFY_BUFFER_SIZE];
    DWORD dwBytesReturned = 0;

    ZeroMemory(inBuf,  sizeof(inBuf));
    ZeroMemory(outBuf, sizeof(outBuf));

    SENDCMDINPARAMS* pCip = (SENDCMDINPARAMS*)inBuf;
    pCip->cBufferSize                 = IDENTIFY_BUFFER_SIZE;
    pCip->irDriveRegs.bSectorCountReg = 1;
    pCip->irDriveRegs.bSectorNumberReg= 1;
    pCip->irDriveRegs.bDriveHeadReg   = 0xA0;
    pCip->irDriveRegs.bCommandReg     = ID_CMD;
    pCip->bDriveNumber                = 0;

    BOOL bOK = DeviceIoControl(hDrive, SMART_RCV_DRIVE_DATA,
        pCip, sizeof(SENDCMDINPARAMS) - 1,
        outBuf, sizeof(SENDCMDOUTPARAMS) - 1 + IDENTIFY_BUFFER_SIZE,
        &dwBytesReturned, NULL);

    if (bOK) {
        SENDCMDOUTPARAMS* pCop = (SENDCMDOUTPARAMS*)outBuf;
        WORD* pIdent = (WORD*)pCop->bBuffer;

        WORD wRotRate = pIdent[217];
        if (wRotRate == 0x0001) {
            WORD wFormFactor = pIdent[168];
            if (wFormFactor == 0x0003 || wFormFactor == 0x0005)
                return DRIVE_TYPE_M2_SATA;
            return DRIVE_TYPE_SSD_SATA;
        }
        else if (wRotRate >= 0x0401)
            return DRIVE_TYPE_HDD;
    }

    if (bBusType == 8) {
        const char* szModel = pInfo->szModel;
        if (strstr(szModel, "SSD") || strstr(szModel, "Solid") ||
            strstr(szModel, "FLASH") || strstr(szModel, "Flash") ||
            strstr(szModel, "MX") || strstr(szModel, "860") ||
            strstr(szModel, "870") || strstr(szModel, "BX") ||
            strstr(szModel, "M.2") || strstr(szModel, "SATA"))
            return DRIVE_TYPE_SSD_SATA;
    }

    const char* szModel = pInfo->szModel;
    if (strstr(szModel, "SSD") || strstr(szModel, "Solid") ||
        strstr(szModel, "FLASH") || strstr(szModel, "Flash") ||
        strstr(szModel, "MX") || strstr(szModel, "860") ||
        strstr(szModel, "870") || strstr(szModel, "BX"))
        return DRIVE_TYPE_SSD_SATA;

    (void)bMediaType;

    return DRIVE_TYPE_HDD;
}

BOOL EnableSMART(HANDLE hDrive, int nDrive)
{
    SENDCMDINPARAMS cip;
    SENDCMDOUTPARAMS cop;
    DWORD dwBytesReturned = 0;

    ZeroMemory(&cip, sizeof(cip));
    ZeroMemory(&cop, sizeof(cop));

    cip.cBufferSize                 = 0;
    cip.irDriveRegs.bFeaturesReg    = SMART_ENABLE;
    cip.irDriveRegs.bSectorCountReg = 1;
    cip.irDriveRegs.bSectorNumberReg= 1;
    cip.irDriveRegs.bCylLowReg      = SMART_CYL_LOW;
    cip.irDriveRegs.bCylHighReg     = SMART_CYL_HI;
    cip.irDriveRegs.bDriveHeadReg   = 0xA0;
    cip.irDriveRegs.bCommandReg     = SMART_CMD;
    cip.bDriveNumber                = 0;

    return DeviceIoControl(hDrive, SMART_SEND_DRIVE_COMMAND,
        &cip, sizeof(SENDCMDINPARAMS) - 1,
        &cop, sizeof(SENDCMDOUTPARAMS) - 1,
        &dwBytesReturned, NULL);
}

BOOL GetIdentifyData(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo)
{
    BYTE inBuf[sizeof(SENDCMDINPARAMS) - 1 + IDENTIFY_BUFFER_SIZE];
    BYTE outBuf[sizeof(SENDCMDOUTPARAMS) - 1 + IDENTIFY_BUFFER_SIZE];
    DWORD dwBytesReturned = 0;

    ZeroMemory(inBuf,  sizeof(inBuf));
    ZeroMemory(outBuf, sizeof(outBuf));

    SENDCMDINPARAMS* pCip = (SENDCMDINPARAMS*)inBuf;
    pCip->cBufferSize                 = IDENTIFY_BUFFER_SIZE;
    pCip->irDriveRegs.bSectorCountReg = 1;
    pCip->irDriveRegs.bSectorNumberReg= 1;
    pCip->irDriveRegs.bDriveHeadReg   = 0xA0;
    pCip->irDriveRegs.bCommandReg     = ID_CMD;
    pCip->bDriveNumber                = 0;

    BOOL bOK = DeviceIoControl(hDrive, SMART_RCV_DRIVE_DATA,
        pCip, sizeof(SENDCMDINPARAMS) - 1,
        outBuf, sizeof(SENDCMDOUTPARAMS) - 1 + IDENTIFY_BUFFER_SIZE,
        &dwBytesReturned, NULL);

    if (!bOK) return FALSE;

    SENDCMDOUTPARAMS* pCop = (SENDCMDOUTPARAMS*)outBuf;
    WORD* pIdent = (WORD*)pCop->bBuffer;

    SwapATAString(pInfo->szSerial,   &pIdent[10], 10);
    SwapATAString(pInfo->szFirmware, &pIdent[23], 4);
    SwapATAString(pInfo->szModel,    &pIdent[27], 20);

    DWORD dwSectors28 = ((DWORD)pIdent[61] << 16) | pIdent[60];
    unsigned __int64 qwSectors = 0;
    if (pIdent[83] & 0x0400) {
        qwSectors = ((unsigned __int64)pIdent[100]) |
                    ((unsigned __int64)pIdent[101] << 16) |
                    ((unsigned __int64)pIdent[102] << 32) |
                    ((unsigned __int64)pIdent[103] << 48);
    }
    if (qwSectors == 0) qwSectors = (unsigned __int64)dwSectors28;
    pInfo->dwCapacityMB = (DWORD)(qwSectors * 512 / (1024 * 1024));

    pInfo->bSMART_Supported = (pIdent[82] & 0x0001) ? TRUE : FALSE;
    pInfo->bSMART_Enabled   = (pIdent[85] & 0x0001) ? TRUE : FALSE;

    return TRUE;
}

BOOL GetIdentifyDataUSB(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    STORAGE_PROPERTY_QUERY spq;
    ZeroMemory(&spq, sizeof(spq));
    spq.PropertyId = StorageDeviceProperty;
    spq.QueryType  = PropertyStandardQuery;

    BYTE outBuf[1024];
    ZeroMemory(outBuf, sizeof(outBuf));
    DWORD dwBytes = 0;

    BOOL bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        &spq, sizeof(spq), outBuf, sizeof(outBuf), &dwBytes, NULL);

    if (!bOK) return FALSE;

    STORAGE_DEVICE_DESCRIPTOR* pDesc = (STORAGE_DEVICE_DESCRIPTOR*)outBuf;

    if (pDesc->ProductIdOffset && pDesc->ProductIdOffset < sizeof(outBuf)) {
        const char* szProd = (const char*)outBuf + pDesc->ProductIdOffset;
        strncpy(pInfo->szModel, szProd, 40);
        pInfo->szModel[40] = '\0';
        TrimStr(pInfo->szModel);
    }
    if (pDesc->SerialNumberOffset && pDesc->SerialNumberOffset < sizeof(outBuf)) {
        const char* szSer = (const char*)outBuf + pDesc->SerialNumberOffset;
        strncpy(pInfo->szSerial, szSer, 20);
        pInfo->szSerial[20] = '\0';
        TrimStr(pInfo->szSerial);
    }
    if (pDesc->ProductRevisionOffset && pDesc->ProductRevisionOffset < sizeof(outBuf)) {
        const char* szRev = (const char*)outBuf + pDesc->ProductRevisionOffset;
        strncpy(pInfo->szFirmware, szRev, 8);
        pInfo->szFirmware[8] = '\0';
    }

    BYTE capBuf[64];
    ZeroMemory(capBuf, sizeof(capBuf));
    DWORD dwCap = 0;
    BOOL bCapOK = DeviceIoControl(hDrive,
        CTL_CODE(0x0000002d, 0x0450, METHOD_BUFFERED, FILE_READ_ACCESS),
        NULL, 0, capBuf, sizeof(capBuf), &dwCap, NULL);

    if (bCapOK && dwCap >= 16) {
        ULONGLONG* pLen = (ULONGLONG*)(capBuf + 8);
        if (*pLen > (1024ULL * 1024ULL))
            pInfo->dwCapacityMB = (DWORD)(*pLen / (1024ULL * 1024ULL));
    }

    if (pInfo->dwCapacityMB <= 100) {
        DISK_GEOMETRY_EX geo;
        ZeroMemory(&geo, sizeof(geo));
        if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                            NULL, 0, &geo, sizeof(geo), &dwBytes, NULL)) {
            DWORD dwGeoCap = (DWORD)(geo.DiskSize.QuadPart / (1024 * 1024));
            if (dwGeoCap > pInfo->dwCapacityMB)
                pInfo->dwCapacityMB = dwGeoCap;
        }
    }

    pInfo->bSMART_Supported = FALSE;
    pInfo->bSMART_Enabled   = FALSE;
    pInfo->bIsUSB           = TRUE;
    pInfo->eType            = DRIVE_TYPE_USB;
    pInfo->nHealthPercent   = -1;

    return TRUE;
}

#pragma pack(push, 1)
typedef struct _SAT16_PASSTHROUGH_BUF {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG    Filler;
    BYTE     SenseBuf[32];
} SAT16_PASSTHROUGH_BUF;
#pragma pack(pop)
#define SAT16_SENSEBUF_OFFSET  (sizeof(SCSI_PASS_THROUGH_DIRECT) + sizeof(ULONG))

static BOOL SATSendCommand12(HANDLE hDrive, BYTE bFeatures, BYTE bSectorCnt,
    BYTE bLBALow, BYTE bCylLow, BYTE bCylHigh, BYTE bCommand, BYTE bProtocol,
    BYTE* pDataBuf, DWORD dwDataLen)
{
    SAT_PASSTHROUGH_BUF sptwb;
    DWORD dwBytesReturned = 0;
    ZeroMemory(&sptwb, sizeof(sptwb));

    sptwb.sptd.Length          = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptwb.sptd.CdbLength       = 12;
    sptwb.sptd.SenseInfoLength = (UCHAR)sizeof(sptwb.SenseBuf);
    sptwb.sptd.SenseInfoOffset = SAT_SENSEBUF_OFFSET;
    sptwb.sptd.TimeOutValue    = 30;

    if (pDataBuf && dwDataLen > 0) {
        sptwb.sptd.DataIn             = SCSI_IOCTL_DATA_IN;
        sptwb.sptd.DataTransferLength = dwDataLen;
        sptwb.sptd.DataBuffer         = pDataBuf;
    } else {
        sptwb.sptd.DataIn             = SCSI_IOCTL_DATA_UNSPECIFIED;
        sptwb.sptd.DataTransferLength = 0;
        sptwb.sptd.DataBuffer         = NULL;
    }

    sptwb.sptd.Cdb[0] = 0xA1;
    sptwb.sptd.Cdb[1] = bProtocol;
    sptwb.sptd.Cdb[2] = (pDataBuf && dwDataLen > 0)
                         ? (SAT_FLAGS_TDIR_FROM_DEV | SAT_FLAGS_BYTE_BLOCK | SAT_FLAGS_TLEN_SECTOR_CNT)
                         : 0x00;
    sptwb.sptd.Cdb[3] = bFeatures;
    sptwb.sptd.Cdb[4] = bSectorCnt;
    sptwb.sptd.Cdb[5] = bLBALow;
    sptwb.sptd.Cdb[6] = bCylLow;
    sptwb.sptd.Cdb[7] = bCylHigh;
    sptwb.sptd.Cdb[8] = 0xA0;
    sptwb.sptd.Cdb[9] = bCommand;

    return DeviceIoControl(hDrive, IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptwb, sizeof(sptwb), &sptwb, sizeof(sptwb), &dwBytesReturned, NULL);
}

static BOOL SATSendCommand16(HANDLE hDrive, BYTE bFeatures, BYTE bSectorCnt,
    BYTE bLBALow, BYTE bCylLow, BYTE bCylHigh, BYTE bCommand, BYTE bProtocol,
    BYTE* pDataBuf, DWORD dwDataLen)
{
    SAT16_PASSTHROUGH_BUF sptwb;
    DWORD dwBytesReturned = 0;
    ZeroMemory(&sptwb, sizeof(sptwb));

    sptwb.sptd.Length          = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptwb.sptd.CdbLength       = 16;
    sptwb.sptd.SenseInfoLength = (UCHAR)sizeof(sptwb.SenseBuf);
    sptwb.sptd.SenseInfoOffset = (ULONG)SAT16_SENSEBUF_OFFSET;
    sptwb.sptd.TimeOutValue    = 30;

    if (pDataBuf && dwDataLen > 0) {
        sptwb.sptd.DataIn             = SCSI_IOCTL_DATA_IN;
        sptwb.sptd.DataTransferLength = dwDataLen;
        sptwb.sptd.DataBuffer         = pDataBuf;
    } else {
        sptwb.sptd.DataIn             = SCSI_IOCTL_DATA_UNSPECIFIED;
        sptwb.sptd.DataTransferLength = 0;
        sptwb.sptd.DataBuffer         = NULL;
    }

    sptwb.sptd.Cdb[0]  = 0x85;
    sptwb.sptd.Cdb[1]  = bProtocol;
    sptwb.sptd.Cdb[2]  = (pDataBuf && dwDataLen > 0)
                          ? (SAT_FLAGS_TDIR_FROM_DEV | SAT_FLAGS_BYTE_BLOCK | SAT_FLAGS_TLEN_SECTOR_CNT)
                          : 0x00;
    sptwb.sptd.Cdb[3]  = 0;
    sptwb.sptd.Cdb[4]  = bFeatures;
    sptwb.sptd.Cdb[5]  = 0;
    sptwb.sptd.Cdb[6]  = bSectorCnt;
    sptwb.sptd.Cdb[7]  = 0;
    sptwb.sptd.Cdb[8]  = bLBALow;
    sptwb.sptd.Cdb[9]  = 0;
    sptwb.sptd.Cdb[10] = bCylLow;
    sptwb.sptd.Cdb[11] = 0;
    sptwb.sptd.Cdb[12] = bCylHigh;
    sptwb.sptd.Cdb[13] = 0xA0;
    sptwb.sptd.Cdb[14] = bCommand;
    sptwb.sptd.Cdb[15] = 0;

    return DeviceIoControl(hDrive, IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptwb, sizeof(sptwb), &sptwb, sizeof(sptwb), &dwBytesReturned, NULL);
}

static BOOL SATSendCommand(HANDLE hDrive, BYTE bFeatures, BYTE bSectorCnt,
    BYTE bLBALow, BYTE bCylLow, BYTE bCylHigh, BYTE bCommand, BYTE bProtocol,
    BYTE* pDataBuf, DWORD dwDataLen)
{
    if (SATSendCommand12(hDrive, bFeatures, bSectorCnt, bLBALow,
                         bCylLow, bCylHigh, bCommand, bProtocol, pDataBuf, dwDataLen))
        return TRUE;
    return SATSendCommand16(hDrive, bFeatures, bSectorCnt, bLBALow,
                            bCylLow, bCylHigh, bCommand, bProtocol, pDataBuf, dwDataLen);
}

BOOL GetIdentifyDataSAT(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BYTE dataBuf[IDENTIFY_BUFFER_SIZE];
    ZeroMemory(dataBuf, sizeof(dataBuf));

    BOOL bOK = SATSendCommand(hDrive, 0, 1, 0, 0, 0,
        ID_CMD, SAT_PROTO_PIO_IN, dataBuf, IDENTIFY_BUFFER_SIZE);

    if (!bOK) return FALSE;

    WORD* pIdent = (WORD*)dataBuf;

    SwapATAString(pInfo->szSerial,   &pIdent[10], 10);
    SwapATAString(pInfo->szFirmware, &pIdent[23], 4);
    SwapATAString(pInfo->szModel,    &pIdent[27], 20);

    DWORD dwSectors28 = ((DWORD)pIdent[61] << 16) | pIdent[60];
    unsigned __int64 qwSectors = 0;
    if (pIdent[83] & 0x0400) {
        qwSectors = ((unsigned __int64)pIdent[100]) |
                    ((unsigned __int64)pIdent[101] << 16) |
                    ((unsigned __int64)pIdent[102] << 32) |
                    ((unsigned __int64)pIdent[103] << 48);
    }
    if (qwSectors == 0) qwSectors = (unsigned __int64)dwSectors28;
    pInfo->dwCapacityMB = (DWORD)(qwSectors * 512 / (1024 * 1024));

    if (pInfo->dwCapacityMB <= 100) {
        DISK_GEOMETRY_EX geo;
        ZeroMemory(&geo, sizeof(geo));
        DWORD dwGeoBytes = 0;
        if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                            NULL, 0, &geo, sizeof(geo), &dwGeoBytes, NULL)) {
            DWORD dwGeoCap = (DWORD)(geo.DiskSize.QuadPart / (1024 * 1024));
            if (dwGeoCap > pInfo->dwCapacityMB)
                pInfo->dwCapacityMB = dwGeoCap;
        }
    }

    pInfo->bSMART_Supported = TRUE;
    pInfo->bSMART_Enabled   = (pIdent[85] & 0x0001) ? TRUE : FALSE;
    pInfo->bIsUSB           = TRUE;

    return TRUE;
}

BOOL GetSMARTAttributesSAT(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BYTE dataBuf[READ_ATTRIBUTE_BUFFER_SIZE];
    ZeroMemory(dataBuf, sizeof(dataBuf));

    SATSendCommand(hDrive, SMART_ENABLE, 1, 1,
        SMART_CYL_LOW, SMART_CYL_HI, SMART_CMD,
        SAT_PROTO_NON_DATA, NULL, 0);

    BOOL bOK = SATSendCommand(hDrive, SMART_READ_DATA, 1, 0x00,
        SMART_CYL_LOW, SMART_CYL_HI, SMART_CMD,
        SAT_PROTO_PIO_IN, dataBuf, READ_ATTRIBUTE_BUFFER_SIZE);

    if (!bOK) return FALSE;

    BOOL bAllZero = TRUE, bAllFF = TRUE;
    int k;
    for (k = 2; k < 20; k++) {
        if (dataBuf[k] != 0x00) bAllZero = FALSE;
        if (dataBuf[k] != 0xFF) bAllFF = FALSE;
    }
    if (bAllZero || bAllFF) return FALSE;

    memcpy(&pInfo->attrData, dataBuf, sizeof(SMART_ATTRIBUTE_DATA));
    return TRUE;
}

BOOL GetSMARTThresholdsSAT(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BYTE dataBuf[READ_THRESHOLD_BUFFER_SIZE];
    ZeroMemory(dataBuf, sizeof(dataBuf));

    BOOL bOK = SATSendCommand(hDrive, SMART_READ_THRESHOLDS, 1, 0x00,
        SMART_CYL_LOW, SMART_CYL_HI, SMART_CMD,
        SAT_PROTO_PIO_IN, dataBuf, READ_THRESHOLD_BUFFER_SIZE);

    if (!bOK) return FALSE;

    memcpy(&pInfo->threshData, dataBuf, sizeof(SMART_THRESHOLD_DATA));
    return TRUE;
}

BOOL GetSMARTAttributes(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo)
{
    BYTE inBuf[sizeof(SENDCMDINPARAMS) - 1];
    BYTE outBuf[sizeof(SENDCMDOUTPARAMS) - 1 + READ_ATTRIBUTE_BUFFER_SIZE];
    DWORD dwBytesReturned = 0;

    ZeroMemory(inBuf,  sizeof(inBuf));
    ZeroMemory(outBuf, sizeof(outBuf));

    SENDCMDINPARAMS* pCip = (SENDCMDINPARAMS*)inBuf;
    pCip->cBufferSize                 = READ_ATTRIBUTE_BUFFER_SIZE;
    pCip->irDriveRegs.bFeaturesReg    = SMART_READ_DATA;
    pCip->irDriveRegs.bSectorCountReg = 1;
    pCip->irDriveRegs.bSectorNumberReg= 1;
    pCip->irDriveRegs.bCylLowReg      = SMART_CYL_LOW;
    pCip->irDriveRegs.bCylHighReg     = SMART_CYL_HI;
    pCip->irDriveRegs.bDriveHeadReg   = 0xA0;
    pCip->irDriveRegs.bCommandReg     = SMART_CMD;
    pCip->bDriveNumber                = 0;

    BOOL bOK = DeviceIoControl(hDrive, SMART_RCV_DRIVE_DATA,
        pCip, sizeof(SENDCMDINPARAMS) - 1,
        outBuf, sizeof(SENDCMDOUTPARAMS) - 1 + READ_ATTRIBUTE_BUFFER_SIZE,
        &dwBytesReturned, NULL);

    if (!bOK) return FALSE;

    SENDCMDOUTPARAMS* pCop = (SENDCMDOUTPARAMS*)outBuf;
    memcpy(&pInfo->attrData, pCop->bBuffer, sizeof(SMART_ATTRIBUTE_DATA));
    return TRUE;
}

BOOL GetSMARTThresholds(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo)
{
    BYTE inBuf[sizeof(SENDCMDINPARAMS) - 1];
    BYTE outBuf[sizeof(SENDCMDOUTPARAMS) - 1 + READ_THRESHOLD_BUFFER_SIZE];
    DWORD dwBytesReturned = 0;

    ZeroMemory(inBuf,  sizeof(inBuf));
    ZeroMemory(outBuf, sizeof(outBuf));

    SENDCMDINPARAMS* pCip = (SENDCMDINPARAMS*)inBuf;
    pCip->cBufferSize                 = READ_THRESHOLD_BUFFER_SIZE;
    pCip->irDriveRegs.bFeaturesReg    = SMART_READ_THRESHOLDS;
    pCip->irDriveRegs.bSectorCountReg = 1;
    pCip->irDriveRegs.bSectorNumberReg= 1;
    pCip->irDriveRegs.bCylLowReg      = SMART_CYL_LOW;
    pCip->irDriveRegs.bCylHighReg     = SMART_CYL_HI;
    pCip->irDriveRegs.bDriveHeadReg   = 0xA0;
    pCip->irDriveRegs.bCommandReg     = SMART_CMD;
    pCip->bDriveNumber                = 0;

    BOOL bOK = DeviceIoControl(hDrive, SMART_RCV_DRIVE_DATA,
        pCip, sizeof(SENDCMDINPARAMS) - 1,
        outBuf, sizeof(SENDCMDOUTPARAMS) - 1 + READ_THRESHOLD_BUFFER_SIZE,
        &dwBytesReturned, NULL);

    if (!bOK) return FALSE;

    SENDCMDOUTPARAMS* pCop = (SENDCMDOUTPARAMS*)outBuf;
    memcpy(&pInfo->threshData, pCop->bBuffer, sizeof(SMART_THRESHOLD_DATA));
    return TRUE;
}

BOOL GetSMARTViaStorageProtocol(HANDLE hDrive, DRIVE_INFO* pInfo)
{
    BYTE buf[sizeof(MY_STORAGE_PROTOCOL_QUERY) + 512 + 64];
    ZeroMemory(buf, sizeof(buf));
    MY_STORAGE_PROTOCOL_QUERY* pQ = (MY_STORAGE_PROTOCOL_QUERY*)buf;
    DWORD dwBytes = 0;

    pQ->PropertyId = (ULONG)StorageDeviceProtocolSpecificProperty;
    pQ->QueryType  = 0;
    pQ->ProtocolSpecific.ProtocolType              = MY_ProtocolTypeAta;
    pQ->ProtocolSpecific.DataType                  = MY_AtaDataTypeSmartData;
    pQ->ProtocolSpecific.ProtocolDataRequestValue  = 0;
    pQ->ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    pQ->ProtocolSpecific.ProtocolDataOffset        = sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    pQ->ProtocolSpecific.ProtocolDataLength        = 512;

    BOOL bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        buf, sizeof(buf), buf, sizeof(buf), &dwBytes, NULL);

    if (!bOK || dwBytes < (ULONG)(sizeof(ULONG)*2 + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA) + 16))
        return FALSE;

    BYTE* pData = buf + sizeof(ULONG) + sizeof(ULONG) + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);

    BOOL bAllZero = TRUE;
    int k;
    for (k = 2; k < 30; k++) {
        if (pData[k] != 0x00) { bAllZero = FALSE; break; }
    }
    if (bAllZero) return FALSE;

    memcpy(&pInfo->attrData, pData, sizeof(SMART_ATTRIBUTE_DATA));

    ZeroMemory(buf, sizeof(buf));
    pQ->PropertyId = (ULONG)StorageDeviceProtocolSpecificProperty;
    pQ->QueryType  = 0;
    pQ->ProtocolSpecific.ProtocolType              = MY_ProtocolTypeAta;
    pQ->ProtocolSpecific.DataType                  = MY_AtaDataTypeSmartThresholds;
    pQ->ProtocolSpecific.ProtocolDataRequestValue  = 0;
    pQ->ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    pQ->ProtocolSpecific.ProtocolDataOffset        = sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
    pQ->ProtocolSpecific.ProtocolDataLength        = 512;

    bOK = DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY,
        buf, sizeof(buf), buf, sizeof(buf), &dwBytes, NULL);

    if (bOK && dwBytes >= (ULONG)(sizeof(ULONG)*2 + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA) + 16)) {
        pData = buf + sizeof(ULONG) + sizeof(ULONG) + sizeof(MY_STORAGE_PROTOCOL_SPECIFIC_DATA);
        memcpy(&pInfo->threshData, pData, sizeof(SMART_THRESHOLD_DATA));
    }

    return TRUE;
}

BOOL GetSMARTPredictFailure(HANDLE hDrive, int nDrive, BOOL* pbFail)
{
#pragma pack(push,1)
    typedef struct {
        DWORD   cBufferSize;
        DRIVERSTATUS DriverStatus;
        BYTE    bBuffer[16];
    } MY_OUTPARAMS;
#pragma pack(pop)

    SENDCMDINPARAMS cip;
    MY_OUTPARAMS    cop;
    DWORD dwBytesReturned = 0;

    ZeroMemory(&cip, sizeof(cip));
    ZeroMemory(&cop, sizeof(cop));
    *pbFail = FALSE;

    cip.cBufferSize                 = 0;
    cip.irDriveRegs.bFeaturesReg    = SMART_RETURN_STATUS;
    cip.irDriveRegs.bSectorCountReg = 1;
    cip.irDriveRegs.bSectorNumberReg= 1;
    cip.irDriveRegs.bCylLowReg      = SMART_CYL_LOW;
    cip.irDriveRegs.bCylHighReg     = SMART_CYL_HI;
    cip.irDriveRegs.bDriveHeadReg   = 0xA0;
    cip.irDriveRegs.bCommandReg     = SMART_CMD;
    cip.bDriveNumber                = 0;

    BOOL bOK = DeviceIoControl(hDrive, SMART_SEND_DRIVE_COMMAND,
        &cip, sizeof(SENDCMDINPARAMS) - 1,
        &cop, sizeof(MY_OUTPARAMS), &dwBytesReturned, NULL);

    if (bOK && cop.DriverStatus.bDriverError == 0) {
        BYTE bCylLow  = cop.bBuffer[3];
        BYTE bCylHigh = cop.bBuffer[4];
        if (bCylLow == 0xF4 && bCylHigh == 0x2C)
            *pbFail = TRUE;
    }

    return bOK;
}

DWORD GetRawValue(BYTE* pRaw)
{
    return ((DWORD)pRaw[3] << 24) |
           ((DWORD)pRaw[2] << 16) |
           ((DWORD)pRaw[1] <<  8) |
            (DWORD)pRaw[0];
}

static WORD GetRawValue16Lo(BYTE* pRaw)
{
    return ((WORD)pRaw[1] << 8) | (WORD)pRaw[0];
}

typedef struct _CRIT_ATTR {
    BYTE  bID;
    int   nWeight;
    BOOL  bUseRaw;
} CRIT_ATTR;

static const CRIT_ATTR g_CritAttrs[] = {
    { 0x05,  10,     TRUE  },
    { 0xC6,  10,     TRUE  },
    { 0xC5,   9,     TRUE  },
    { 0xBB,   8,     TRUE  },
    { 0xC4,   7,     TRUE  },
    { 0xBC,   6,     TRUE  },
    { 0x01,   5,     FALSE },
    { 0x0A,   7,     FALSE },
    { 0xB7,   5,     FALSE },
    { 0xB8,   6,     FALSE },
    { 0xC7,   4,     FALSE },
    { 0xC8,   4,     FALSE },
    { 0xCA,   5,     FALSE },
    { 0xAB,   6,     FALSE },
    { 0xAC,   6,     FALSE },
    { 0x00,   0,     FALSE }
};

static int GetCritWeight(BYTE bID)
{
    int i = 0;
    while (g_CritAttrs[i].bID != 0x00) {
        if (g_CritAttrs[i].bID == bID) return g_CritAttrs[i].nWeight;
        i++;
    }
    return 0;
}

int CalculateHealth(DRIVE_INFO* pInfo)
{
    int i, j;
    if (pInfo->bIsNVMe)
        return CalculateHealthNVMe(pInfo);

    if (pInfo->bPredictFailure)
        return 0;

    if (pInfo->bIsUSB && !pInfo->bSMART_Supported)
        return -1;
    for (i = 0; i < 30; i++) {
        if (pInfo->attrData.stAttributes[i].bAttrID == 0xA9) {
            DWORD dwLife = GetRawValue(pInfo->attrData.stAttributes[i].bRawValue);
            if (dwLife > 0 && dwLife <= 100) {
                for (j = 0; j < 30; j++) {
                    SMART_ATTRIBUTE* pA = &pInfo->attrData.stAttributes[j];
                    if (pA->bAttrID == 0) continue;
                    BYTE bThresh = 0;
                    int k;
                    for (k = 0; k < 30; k++) {
                        if (pInfo->threshData.stThresholds[k].bAttrID == pA->bAttrID) {
                            bThresh = pInfo->threshData.stThresholds[k].bThresholdValue;
                            break;
                        }
                    }
                    if (bThresh > 0 && pA->bWorstValue > 0 && pA->bWorstValue <= bThresh)
                        return 0;
                }
                return (int)dwLife;
            }
            break;
        }
    }

    int nHealth = 100;

    for (i = 0; i < 30; i++) {
        SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
        if (pAttr->bAttrID != 0x05) continue;
        DWORD dwRaw = GetRawValue(pAttr->bRawValue);
        if (dwRaw > 0) {
            int nPenalty = (int)dwRaw;
            if (nPenalty > 100) nPenalty = 100;
            nHealth -= nPenalty;
        }
        break;
    }
    if (nHealth < 0) nHealth = 0;
    for (i = 0; i < 30; i++) {
        SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
        if (pAttr->bAttrID != 0xC5) continue;
        DWORD dwRaw = GetRawValue(pAttr->bRawValue);
        if (dwRaw > 0) {
            int nPenalty = (int)dwRaw * 2;
            if (nPenalty > 100) nPenalty = 100;
            nHealth -= nPenalty;
        }
        break;
    }
    if (nHealth < 0) nHealth = 0;

    for (i = 0; i < 30; i++) {
        SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
        if (pAttr->bAttrID != 0xC6) continue;
        DWORD dwRaw = GetRawValue(pAttr->bRawValue);
        if (dwRaw > 0) {
            int nPenalty = (int)dwRaw * 3;
            if (nPenalty > 100) nPenalty = 100;
            nHealth -= nPenalty;
        }
        break;
    }
    if (nHealth < 0) nHealth = 0;

    for (i = 0; i < 30; i++) {
        SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
        if (pAttr->bAttrID == 0) continue;
        if (pAttr->bAttrID == 0x05 || pAttr->bAttrID == 0xC5 || pAttr->bAttrID == 0xC6)
            continue;
        if (GetCritWeight(pAttr->bAttrID) == 0) continue;

        BYTE bThresh = 0;
        for (j = 0; j < 30; j++) {
            if (pInfo->threshData.stThresholds[j].bAttrID == pAttr->bAttrID) {
                bThresh = pInfo->threshData.stThresholds[j].bThresholdValue;
                break;
            }
        }
        if (bThresh == 0) continue;

        BYTE bWorst = pAttr->bWorstValue;
        if (bWorst == 0 || bWorst == 255) bWorst = pAttr->bAttrValue;
        if (bWorst == 0 || bWorst == 255) continue;

        if (bWorst <= bThresh)
            return 0;

        int nRange = 100 - (int)bThresh;
        if (nRange <= 0) continue;
        int nAttrHealth = ((int)bWorst - (int)bThresh) * 100 / nRange;
        if (nAttrHealth > 100) nAttrHealth = 100;
        if (nAttrHealth < nHealth) nHealth = nAttrHealth;
    }

    if (nHealth < 0)   nHealth = 0;
    if (nHealth > 100) nHealth = 100;
    return nHealth;
}

int CalculatePerformance(DRIVE_INFO* pInfo)
{
    if (pInfo->bIsUSB && !pInfo->bSMART_Supported)
        return -1;

    int i, j;


    int nCRCPerf = 100;
    for (i = 0; i < 30; i++) {
        SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
        if (pAttr->bAttrID != 0xC7) continue;
        DWORD dwCRC = GetRawValue(pAttr->bRawValue);
        if      (dwCRC == 0)   nCRCPerf = 100;
        else if (dwCRC < 10)   nCRCPerf = 75;
        else                   nCRCPerf = 50;
        break;
    }


    if (pInfo->bIsNVMe || pInfo->eType == DRIVE_TYPE_SSD_SATA || pInfo->eType == DRIVE_TYPE_M2_SATA) {
        int nPerf = (nCRCPerf * 25 + 100 * 75) / 100;
        if (nPerf < 0)   nPerf = 0;
        if (nPerf > 100) nPerf = 100;
        return nPerf;
    }





    static const BYTE sPerfIDs[] = { 0x07, 0x08, 0x02, 0x00 };
    int nPerfSum = 0, nPerfCount = 0;
    for (i = 0; sPerfIDs[i] != 0; i++) {
        BYTE bTargetID = sPerfIDs[i];
        for (j = 0; j < 30; j++) {
            SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[j];
            if (pAttr->bAttrID != bTargetID) continue;
            BYTE bThresh = 0;
            int k;
            for (k = 0; k < 30; k++) {
                if (pInfo->threshData.stThresholds[k].bAttrID == bTargetID) {
                    bThresh = pInfo->threshData.stThresholds[k].bThresholdValue;
                    break;
                }
            }
            int nVal = (int)pAttr->bAttrValue;
            if (bThresh > 0 && nVal > bThresh) {

                int nRange = 100 - (int)bThresh;
                int nAttrPerf = (nRange > 0)
                    ? ((nVal - (int)bThresh) * 100 / nRange)
                    : 100;
                if (nAttrPerf > 100) nAttrPerf = 100;
                nPerfSum += nAttrPerf;
            } else if (bThresh == 0 && nVal > 0) {

                nPerfSum += 100;
            } else {
                nPerfSum += 0;
            }
            nPerfCount++;
            break;
        }
    }

    int nSmartPerf = (nPerfCount > 0) ? (nPerfSum / nPerfCount) : 100;






    int nDMAPerf = 100;

    if (nCRCPerf <= 50)
        nDMAPerf = 60;
    else if (nCRCPerf <= 75)
        nDMAPerf = 80;


    int nPerf = (nSmartPerf * 25 + nDMAPerf * 50 + nCRCPerf * 25) / 100;
    if (nPerf < 0)   nPerf = 0;
    if (nPerf > 100) nPerf = 100;
    return nPerf;
}

void FormatSize(DWORD dwMB, char* szBuf, int nBufLen)
{
    if (dwMB >= 1024 * 1024)
        _snprintf(szBuf, nBufLen, "%.1f TB", (double)dwMB / (1024.0 * 1024.0));
    else if (dwMB >= 1024)
        _snprintf(szBuf, nBufLen, "%.1f GB", (double)dwMB / 1024.0);
    else
        _snprintf(szBuf, nBufLen, "%u MB", (unsigned)dwMB);
}


int MeasureReadSpeed(int nDriveIndex)
{
    char szPath[32];
    _snprintf(szPath, sizeof(szPath), "\\\\.\\PhysicalDrive%d", nDriveIndex);

    HANDLE hDrive = CreateFileA(szPath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE)
        return -1;

    const DWORD dwBufSize = 4 * 1024 * 1024;
    BYTE* pBuf = (BYTE*)VirtualAlloc(NULL, dwBufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pBuf) {
        CloseHandle(hDrive);
        return -1;
    }

    LARGE_INTEGER liFreq, liStart, liEnd;
    QueryPerformanceFrequency(&liFreq);

    const int nPasses = 4;
    DWORD dwTotalRead = 0;
    QueryPerformanceCounter(&liStart);

    for (int i = 0; i < nPasses; i++) {
        DWORD dwRead = 0;
        HANDLE hRaw = CreateFileA(szPath, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hRaw == INVALID_HANDLE_VALUE) break;
        if (!ReadFile(hRaw, pBuf, dwBufSize, &dwRead, NULL) || dwRead == 0) {
            CloseHandle(hRaw);
            break;
        }
        dwTotalRead += dwRead;
        CloseHandle(hRaw);
    }

    QueryPerformanceCounter(&liEnd);
    VirtualFree(pBuf, 0, MEM_RELEASE);
    CloseHandle(hDrive);

    if (dwTotalRead == 0)
        return -1;

    double dElapsed = (double)(liEnd.QuadPart - liStart.QuadPart) / (double)liFreq.QuadPart;
    if (dElapsed <= 0.0)
        return -1;

    int nSpeedMBs = (int)((double)dwTotalRead / (1024.0 * 1024.0) / dElapsed);
    return nSpeedMBs;
}

int MeasureWriteSpeed(int nDriveIndex)
{
    char szPath[32];
    _snprintf(szPath, sizeof(szPath), "\\\\.\\PhysicalDrive%d", nDriveIndex);

    HANDLE hDrive = CreateFileA(szPath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE)
        return -1;

    DISK_PERFORMANCE dp1, dp2;
    ZeroMemory(&dp1, sizeof(dp1));
    ZeroMemory(&dp2, sizeof(dp2));
    DWORD dwBytes = 0;

    if (!DeviceIoControl(hDrive, IOCTL_DISK_PERFORMANCE,
                         NULL, 0, &dp1, sizeof(dp1), &dwBytes, NULL)) {
        CloseHandle(hDrive);
        return -1;
    }

    const DWORD dwBufSize = 4 * 1024 * 1024;
    BYTE* pBuf = (BYTE*)VirtualAlloc(NULL, dwBufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pBuf) {
        HANDLE hRaw = CreateFileA(szPath, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hRaw != INVALID_HANDLE_VALUE) {
            DWORD dwRead = 0;
            ReadFile(hRaw, pBuf, dwBufSize, &dwRead, NULL);
            CloseHandle(hRaw);
        }
        VirtualFree(pBuf, 0, MEM_RELEASE);
    }

    dwBytes = 0;
    if (!DeviceIoControl(hDrive, IOCTL_DISK_PERFORMANCE,
                         NULL, 0, &dp2, sizeof(dp2), &dwBytes, NULL)) {
        CloseHandle(hDrive);
        return -1;
    }

    CloseHandle(hDrive);

    LONGLONG llWritten = dp2.BytesWritten.QuadPart - dp1.BytesWritten.QuadPart;
    LONGLONG llReadTime = dp2.ReadTime.QuadPart - dp1.ReadTime.QuadPart;
    LONGLONG llWriteTime = dp2.WriteTime.QuadPart - dp1.WriteTime.QuadPart;

    if (llWritten <= 0 || llWriteTime <= 0)
        return 0;

    double dSec = (double)llWriteTime / 10000000.0;
    int nSpeedMBs = (int)((double)llWritten / (1024.0 * 1024.0) / dSec);
    (void)llReadTime;
    return nSpeedMBs;
}

int ScanDrives(DRIVE_INFO* pDrives, int nMaxDrives)
{
    int nFound = 0;
    int nDrive;

    int nScanLimit = 32;
    for (nDrive = 0; nDrive < nScanLimit && nFound < nMaxDrives; nDrive++) {
        HANDLE hDrive;
        if (!OpenDrive(nDrive, &hDrive))
            continue;

        DRIVE_INFO* pInfo = &pDrives[nFound];
        ZeroMemory(pInfo, sizeof(DRIVE_INFO));
        pInfo->nDriveIndex    = nDrive;
        pInfo->bIsUSB         = FALSE;
        pInfo->bIsNVMe        = FALSE;
        pInfo->nHealthPercent = 0;
        pInfo->nTemperatureC  = -1;
        pInfo->eType          = DRIVE_TYPE_UNKNOWN;

        if (IsNVMeDrive(hDrive)) {
            if (GetNVMeInfo(hDrive, pInfo)) {
                if (!pInfo->bSMART_Supported)
                    GetNVMeHealthLogEx(hDrive, pInfo);
                pInfo->nHealthPercent = CalculateHealthNVMe(pInfo);
                pInfo->nPerformancePercent = CalculatePerformance(pInfo);
                CloseHandle(hDrive);
                pInfo->nReadSpeedMBs  = MeasureReadSpeed(nDrive);
                pInfo->nWriteSpeedMBs = MeasureWriteSpeed(nDrive);
                nFound++;
                continue;
            }
            GetNVMeCapacity(hDrive, pInfo);
            GetNVMeHealthLogEx(hDrive, pInfo);
            if (pInfo->bSMART_Supported)
                pInfo->nHealthPercent = CalculateHealthNVMe(pInfo);
            else
                pInfo->nHealthPercent = -1;
            pInfo->eType   = DRIVE_TYPE_NVME;
            pInfo->bIsNVMe = TRUE;
            CloseHandle(hDrive);
            nFound++;
            continue;
        }

        BOOL bDetectedUSB = IsUSBDrive(hDrive);
        BOOL bIdentOK = FALSE;

        if (!bDetectedUSB) {
            bIdentOK = GetIdentifyData(hDrive, nDrive, pInfo);
        }

        if (!bIdentOK) {
            bIdentOK = GetIdentifyDataSAT(hDrive, pInfo);
            if (bIdentOK) pInfo->bIsUSB = TRUE;
        }

        if (!bIdentOK) {
            bIdentOK = GetIdentifyDataUSB(hDrive, pInfo);
            if (!bIdentOK) {
                CloseHandle(hDrive);
                continue;
            }
        }

        if (bDetectedUSB && !pInfo->bIsUSB) {
            pInfo->bIsUSB = TRUE;
            DRIVE_INFO infoSAT;
            ZeroMemory(&infoSAT, sizeof(infoSAT));
            if (GetIdentifyDataSAT(hDrive, &infoSAT)) {
                infoSAT.nDriveIndex = nDrive;
                memcpy(pInfo, &infoSAT, sizeof(DRIVE_INFO));
                pInfo->bIsUSB = TRUE;
            } else {
                pInfo->bSMART_Supported = FALSE;
                pInfo->bSMART_Enabled   = FALSE;
            }
        }

        if (!pInfo->bIsUSB)
            pInfo->eType = DetectDriveType(hDrive, pInfo);
        else
            pInfo->eType = DRIVE_TYPE_USB;

        if (!pInfo->bIsUSB && pInfo->bSMART_Supported) {
            EnableSMART(hDrive, nDrive);
            BOOL bAttrOK   = GetSMARTAttributes(hDrive, nDrive, pInfo);
            BOOL bThreshOK = GetSMARTThresholds(hDrive, nDrive, pInfo);

            if (bAttrOK)
                GetSMARTPredictFailure(hDrive, nDrive, &pInfo->bPredictFailure);

            if (!bAttrOK || !bThreshOK) {
                BOOL bAttrSAT   = GetSMARTAttributesSAT(hDrive, pInfo);
                BOOL bThreshSAT = GetSMARTThresholdsSAT(hDrive, pInfo);
                if (!bAttrSAT) {
                    pInfo->bSMART_Supported = FALSE;
                    pInfo->nHealthPercent   = -1;
                } else {
                    pInfo->bIsUSB = TRUE;
                    pInfo->eType  = DRIVE_TYPE_USB;
                }
            }

            int i;
            for (i = 0; i < 30; i++) {
                if (pInfo->attrData.stAttributes[i].bAttrID == 0xC2) {
                    pInfo->nTemperatureC = (int)GetRawValue16Lo(
                        pInfo->attrData.stAttributes[i].bRawValue);
                    if (pInfo->nTemperatureC < 0 || pInfo->nTemperatureC > 100)
                        pInfo->nTemperatureC = (int)pInfo->attrData.stAttributes[i].bAttrValue;
                    break;
                }
                if (pInfo->attrData.stAttributes[i].bAttrID == 0xBE && pInfo->nTemperatureC < 0) {
                    pInfo->nTemperatureC = (int)pInfo->attrData.stAttributes[i].bAttrValue;
                }
            }

        } else if (pInfo->bIsUSB && pInfo->bSMART_Supported) {
            BOOL bAttrOK   = GetSMARTAttributesSAT(hDrive, pInfo);
            BOOL bThreshOK = bAttrOK ? GetSMARTThresholdsSAT(hDrive, pInfo) : FALSE;
            (void)bThreshOK;

            if (!bAttrOK) {
                bAttrOK = GetSMARTViaStorageProtocol(hDrive, pInfo);
                bThreshOK = bAttrOK;
            }

            if (!bAttrOK) {
                pInfo->bSMART_Supported = FALSE;
                pInfo->nHealthPercent   = -1;
                CloseHandle(hDrive);
                nFound++;
                continue;
            }


            {
                int i;
                for (i = 0; i < 30; i++) {
                    if (pInfo->attrData.stAttributes[i].bAttrID == 0xC2) {
                        pInfo->nTemperatureC = (int)GetRawValue16Lo(
                            pInfo->attrData.stAttributes[i].bRawValue);
                        if (pInfo->nTemperatureC < 0 || pInfo->nTemperatureC > 100)
                            pInfo->nTemperatureC = (int)pInfo->attrData.stAttributes[i].bAttrValue;
                        break;
                    }
                    if (pInfo->attrData.stAttributes[i].bAttrID == 0xBE && pInfo->nTemperatureC < 0) {
                        pInfo->nTemperatureC = (int)pInfo->attrData.stAttributes[i].bAttrValue;
                    }
                }
            }
        }

        pInfo->nHealthPercent = CalculateHealth(pInfo);
        pInfo->nPerformancePercent = CalculatePerformance(pInfo);

        CloseHandle(hDrive);
        pInfo->nReadSpeedMBs  = MeasureReadSpeed(nDrive);
        pInfo->nWriteSpeedMBs = MeasureWriteSpeed(nDrive);
        nFound++;
    }

    return nFound;
}
