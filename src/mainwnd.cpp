#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mainwnd.h"
#include "smart.h"

static unsigned __int64 NVMeRead128Lo(const BYTE* p)
{
    unsigned __int64 lo = 0;
    int i;
    for (i = 7; i >= 0; i--) lo = (lo << 8) | p[i];
    return lo;
}

static WORD ReadLE16(const BYTE* p)
{
    return (WORD)p[0] | ((WORD)p[1] << 8);
}

DRIVE_INFO  g_Drives[MAX_DRIVES];
int         g_nDriveCount    = 0;
int         g_nSelectedDrive = 0;
HINSTANCE   g_hInst          = NULL;
HWND        g_hMainWnd       = NULL;
HWND        g_hHealthBar     = NULL;

HBRUSH  g_hbrBG     = NULL;
HBRUSH  g_hbrPanel  = NULL;
HBRUSH  g_hbrGreen  = NULL;
HBRUSH  g_hbrYellow = NULL;
HBRUSH  g_hbrRed    = NULL;
HFONT   g_hFontTitle  = NULL;
HFONT   g_hFontNormal = NULL;
HFONT   g_hFontSmall  = NULL;
HFONT   g_hFontBig    = NULL;

void CreateGDIObjects(void)
{
    g_hbrBG     = CreateSolidBrush(CLR_BG);
    g_hbrPanel  = CreateSolidBrush(CLR_PANEL);
    g_hbrGreen  = CreateSolidBrush(CLR_GREEN);
    g_hbrYellow = CreateSolidBrush(CLR_YELLOW);
    g_hbrRed    = CreateSolidBrush(CLR_RED);

    g_hFontTitle  = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontNormal = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontSmall  = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    g_hFontBig    = CreateFontA(-32, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

void DestroyGDIObjects(void)
{
    if (g_hbrBG)     DeleteObject(g_hbrBG);
    if (g_hbrPanel)  DeleteObject(g_hbrPanel);
    if (g_hbrGreen)  DeleteObject(g_hbrGreen);
    if (g_hbrYellow) DeleteObject(g_hbrYellow);
    if (g_hbrRed)    DeleteObject(g_hbrRed);
    if (g_hFontTitle)  DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontSmall)  DeleteObject(g_hFontSmall);
    if (g_hFontBig)    DeleteObject(g_hFontBig);
}

COLORREF GetHealthColor(int nHealth)
{
    if (nHealth < 0)   return CLR_ACCENT;
    if (nHealth >= 70) return CLR_GREEN;
    if (nHealth >= 40) return CLR_YELLOW;
    return CLR_RED;
}

LRESULT CALLBACK HealthBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            int nHealth = -1;
            if (g_nDriveCount > 0 &&
                g_nSelectedDrive >= 0 &&
                g_nSelectedDrive < g_nDriveCount)
            {
                nHealth = g_Drives[g_nSelectedDrive].nHealthPercent;
            }

            HBRUSH hbrBg = CreateSolidBrush(CLR_PANEL);
            FillRect(hdc, &rc, hbrBg);
            DeleteObject(hbrBg);

            int nBarPct = (nHealth < 0) ? 100 : nHealth;
            int nBarW   = (nBarPct > 0)
                          ? (rc.right - rc.left - 4) * nBarPct / 100
                          : 4;
            if (nBarW > 0) {
                RECT rcFill = { rc.left + 2, rc.top + 2,
                                rc.left + 2 + nBarW, rc.bottom - 2 };
                COLORREF clr = GetHealthColor(nHealth);
                HBRUSH hbrFill = CreateSolidBrush(clr);
                FillRect(hdc, &rcFill, hbrFill);
                DeleteObject(hbrFill);
            }

            HPEN hPen = CreatePen(PS_SOLID, 1, CLR_BORDER);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hbrNull = (HBRUSH)GetStockObject(NULL_BRUSH);
            HBRUSH hOldBr  = (HBRUSH)SelectObject(hdc, hbrNull);
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBr);
            DeleteObject(hPen);

            char szPct[16];
            if (nHealth < 0)
                _snprintf(szPct, sizeof(szPct), "N/A");
            else
                _snprintf(szPct, sizeof(szPct), "%d%%", nHealth);

            HFONT hUseFont = g_hFontBig ? g_hFontBig
                           : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hUseFont);

            COLORREF clrText;
            if      (nHealth < 0)    clrText = RGB(255, 255, 255);
            else if (nHealth >= 70)  clrText = RGB(255, 255, 255);
            else if (nHealth >= 40)  clrText = RGB(60,  40,  0  );
            else                     clrText = RGB(160, 0,   0  );
            SetTextColor(hdc, clrText);
            SetBkMode(hdc, TRANSPARENT);
            DrawTextA(hdc, szPct, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, hOldFont);

            EndPaint(hWnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

void RegisterHealthBarClass(HINSTANCE hInst)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = HealthBarWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = NULL;
    wc.lpszClassName = "LLHDHealthBar";
    RegisterClassA(&wc);
}

void RepaintHealthBar(void)
{
    if (g_hHealthBar) {
        InvalidateRect(g_hHealthBar, NULL, TRUE);
        UpdateWindow(g_hHealthBar);
    }
}

void UpdateDriveCombo(HWND hWnd)
{
    HWND hCombo = GetDlgItem(hWnd, IDC_DRIVE_LIST);

    int nPrevSel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
    if (nPrevSel == CB_ERR) nPrevSel = 0;

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

    int i;
    for (i = 0; i < g_nDriveCount; i++) {
        char szBuf[128];
        char szSize[32];
        FormatSize(g_Drives[i].dwCapacityMB, szSize, sizeof(szSize));
        const char* szType = GetDriveTypeName(g_Drives[i].eType);
        if (strlen(g_Drives[i].szModel) > 0)
            _snprintf(szBuf, sizeof(szBuf), "Drive %d: %s (%s) [%s]",
                      g_Drives[i].nDriveIndex, g_Drives[i].szModel, szSize, szType);
        else
            _snprintf(szBuf, sizeof(szBuf), "Drive %d: (unknown) [%s]",
                      g_Drives[i].nDriveIndex, szType);
        SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)szBuf);
    }

    if (g_nDriveCount > 0) {
        int nRestoreSel = (nPrevSel >= 0 && nPrevSel < g_nDriveCount) ? nPrevSel : 0;
        SendMessage(hCombo, CB_SETCURSEL, (WPARAM)nRestoreSel, 0);
    }
}

void UpdateDriveInfo(HWND hWnd, int nDriveIdx)
{
    if (nDriveIdx < 0 || nDriveIdx >= g_nDriveCount) {
        SetDlgItemTextA(hWnd, IDC_MODEL_STATIC,    "Model        -");
        SetDlgItemTextA(hWnd, IDC_SERIAL_STATIC,   "Serial No.   -");
        SetDlgItemTextA(hWnd, IDC_FIRMWARE_STATIC, "Firmware     -");
        SetDlgItemTextA(hWnd, IDC_SIZE_STATIC,     "Capacity     -");
        SetDlgItemTextA(hWnd, IDC_STATUS_STATIC,   "S.M.A.R.T.   Not Available");
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC,  "");
        return;
    }

    DRIVE_INFO* pInfo = &g_Drives[nDriveIdx];
    char szBuf[192];

    _snprintf(szBuf, sizeof(szBuf), "Model        %s",
              (strlen(pInfo->szModel) ? pInfo->szModel : "-"));
    SetDlgItemTextA(hWnd, IDC_MODEL_STATIC, szBuf);

    _snprintf(szBuf, sizeof(szBuf), "Serial No.   %s",
              (strlen(pInfo->szSerial) ? pInfo->szSerial : "-"));
    SetDlgItemTextA(hWnd, IDC_SERIAL_STATIC, szBuf);

    _snprintf(szBuf, sizeof(szBuf), "Firmware     %s",
              (strlen(pInfo->szFirmware) ? pInfo->szFirmware : "-"));
    SetDlgItemTextA(hWnd, IDC_FIRMWARE_STATIC, szBuf);

    char szSize[32];
    FormatSize(pInfo->dwCapacityMB, szSize, sizeof(szSize));
    if (pInfo->nTemperatureC > 0)
        _snprintf(szBuf, sizeof(szBuf), "Capacity     %s   Type: %s   Temp: %d\xB0""C",
                  szSize, GetDriveTypeName(pInfo->eType), pInfo->nTemperatureC);
    else
        _snprintf(szBuf, sizeof(szBuf), "Capacity     %s   Type: %s",
                  szSize, GetDriveTypeName(pInfo->eType));
    SetDlgItemTextA(hWnd, IDC_SIZE_STATIC, szBuf);

    if (pInfo->bIsNVMe && pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf),
            "S.M.A.R.T.   NVMe Health Log   Spare: %d%%   Threshold: %d%%   Used: %d%%",
            (int)pInfo->nvmeHealth.AvailableSpare,
            (int)pInfo->nvmeHealth.AvailableSpareThreshold,
            (int)pInfo->nvmeHealth.PercentageUsed);
    } else if (pInfo->bIsUSB && !pInfo->bSMART_Supported) {
        strcpy(szBuf, "S.M.A.R.T.   Not available (USB bridge not supported)");
    } else if (pInfo->bIsUSB && pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf), "S.M.A.R.T.   USB SAT passthrough - %s",
                  pInfo->bSMART_Enabled ? "Enabled" : "Detected");
    } else if (pInfo->bSMART_Supported) {
        _snprintf(szBuf, sizeof(szBuf), "S.M.A.R.T.   Supported & %s",
                  pInfo->bSMART_Enabled ? "Enabled" : "Disabled");
    } else {
        strcpy(szBuf, "S.M.A.R.T.   Not Supported");
    }
    SetDlgItemTextA(hWnd, IDC_STATUS_STATIC, szBuf);

    char szReason[256];
    szReason[0] = '\0';
    if (pInfo->bIsNVMe && pInfo->bSMART_Supported) {
        BYTE crit = pInfo->nvmeHealth.CriticalWarning;
        if (crit != 0) {
            _snprintf(szReason, sizeof(szReason), "Critical Warning:%s%s%s%s%s",
                (crit & NVME_CRIT_WARN_SPARE_BELOW_THRESH)   ? " [Spare Low]"       : "",
                (crit & NVME_CRIT_WARN_TEMP_THRESHOLD)       ? " [Temp High]"       : "",
                (crit & NVME_CRIT_WARN_RELIABILITY_DEGRADED) ? " [Reliability!]"    : "",
                (crit & NVME_CRIT_WARN_READ_ONLY)            ? " [Read Only!]"      : "",
                (crit & NVME_CRIT_WARN_VOLATILE_MEM_BACKUP)  ? " [Volatile Mem]"    : "");
        }
    } else if (pInfo->bSMART_Supported && !pInfo->bIsUSB) {
        int k;
        DWORD dwR05=0, dwRC4=0, dwRC5=0, dwRC6=0, dwRBB=0;
        for (k = 0; k < 30; k++) {
            BYTE id = pInfo->attrData.stAttributes[k].bAttrID;
            DWORD dw = GetRawValue(pInfo->attrData.stAttributes[k].bRawValue);
            if      (id == 0x05) dwR05 = dw;
            else if (id == 0xC4) dwRC4 = dw;
            else if (id == 0xC5) dwRC5 = dw;
            else if (id == 0xC6) dwRC6 = dw;
            else if (id == 0xBB) dwRBB = dw;
        }
        if (dwRC6 > 0)
            _snprintf(szReason, sizeof(szReason),
                "Uncorrectable: %u   Reallocated: %u   Pending: %u   Events: %u",
                (unsigned)dwRC6, (unsigned)dwR05, (unsigned)dwRC5, (unsigned)dwRC4);
        else if (dwRC5 > 0 || dwR05 > 0)
            _snprintf(szReason, sizeof(szReason),
                "Reallocated: %u   Pending: %u   Events: %u   ECC: %u",
                (unsigned)dwR05, (unsigned)dwRC5, (unsigned)dwRC4, (unsigned)dwRBB);
    }

    if (pInfo->bIsUSB && !pInfo->bSMART_Supported) {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC,
            "USB/External drive detected. SMART not accessible (bridge does not support SAT passthrough).");
    } else if (pInfo->nHealthPercent < 0) {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC,
            "SMART data could not be read. Run as Administrator and click Refresh.");
    } else if (pInfo->bPredictFailure) {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, "!! DRIVE FAILURE PREDICTED BY DRIVE !!");
    } else if (pInfo->nHealthPercent < 40) {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "!! Poor Health - Back up data immediately!  %s", szReason);
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szMsg);
    } else if (pInfo->nHealthPercent < 70) {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "Caution - Monitor drive closely.  %s", szReason);
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szMsg);
    } else if (pInfo->nHealthPercent < 100) {
        char szMsg[320];
        _snprintf(szMsg, sizeof(szMsg), "Good.  %s",
            (szReason[0] ? szReason : "All critical attributes normal."));
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, szMsg);
    } else {
        SetDlgItemTextA(hWnd, IDC_PREDICT_STATIC, "Excellent - Drive is in perfect condition.");
    }

    RepaintHealthBar();
}

void UpdateAttrList(HWND hWnd, int nDriveIdx)
{
    HWND hList = GetDlgItem(hWnd, IDC_ATTR_LIST);
    ListView_DeleteAllItems(hList);

    if (nDriveIdx < 0 || nDriveIdx >= g_nDriveCount) return;

    DRIVE_INFO* pInfo = &g_Drives[nDriveIdx];

    if (pInfo->bIsUSB && !pInfo->bSMART_Supported) {
        LVITEMA lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = 0;
        lvi.iSubItem = 0;
        lvi.pszText  = (LPSTR)"--";
        ListView_InsertItem(hList, &lvi);
        lvi.iSubItem = 1;
        lvi.pszText  = (LPSTR)"SMART not available for USB/External drives";
        ListView_SetItem(hList, &lvi);
        return;
    }

    if (pInfo->bIsNVMe && pInfo->bSMART_Supported) {
        NVME_HEALTH_INFO_LOG* pLog = &pInfo->nvmeHealth;

        unsigned __int64 qwDataRead    = NVMeRead128Lo(pLog->DataUnitsRead);
        unsigned __int64 qwDataWritten = NVMeRead128Lo(pLog->DataUnitsWritten);
        unsigned __int64 qwPOH         = NVMeRead128Lo(pLog->PowerOnHours);
        unsigned __int64 qwPowerCycles = NVMeRead128Lo(pLog->PowerCycles);
        unsigned __int64 qwUnsafeSDs   = NVMeRead128Lo(pLog->UnsafeShutdowns);
        unsigned __int64 qwMediaErr    = NVMeRead128Lo(pLog->MediaErrors);
        unsigned __int64 qwErrLog      = NVMeRead128Lo(pLog->NumErrLogEntries);
        WORD wTempK = ReadLE16(pLog->CompositeTemperature);
        int  nTempC = (wTempK > 273) ? (int)wTempK - 273 : 0;

        int nRow = 0;
        char szVBuf[64];
        LVITEMA lvi;

        #define NVME_ADD_ROW(szId, szName_, szVal_, szStat_) \
        { \
            ZeroMemory(&lvi, sizeof(lvi)); \
            lvi.mask = LVIF_TEXT; lvi.iItem = nRow; lvi.iSubItem = 0; \
            lvi.pszText = (LPSTR)szId; ListView_InsertItem(hList, &lvi); \
            lvi.iSubItem = 1; lvi.pszText = (LPSTR)szName_; ListView_SetItem(hList, &lvi); \
            lvi.iSubItem = 2; lvi.pszText = (LPSTR)"--"; ListView_SetItem(hList, &lvi); \
            lvi.iSubItem = 3; lvi.pszText = (LPSTR)"--"; ListView_SetItem(hList, &lvi); \
            lvi.iSubItem = 4; lvi.pszText = (LPSTR)"--"; ListView_SetItem(hList, &lvi); \
            lvi.iSubItem = 5; lvi.pszText = (LPSTR)szVal_; ListView_SetItem(hList, &lvi); \
            lvi.iSubItem = 6; lvi.pszText = (LPSTR)szStat_; ListView_SetItem(hList, &lvi); \
            nRow++; \
        }

        char szCrit[64];
        if (pLog->CriticalWarning == 0)
            strcpy(szCrit, "0 (None)");
        else
            _snprintf(szCrit, sizeof(szCrit), "0x%02X (!)", pLog->CriticalWarning);
        NVME_ADD_ROW("01h", "Critical Warning", szCrit,
            (pLog->CriticalWarning ? "WARNING" : "OK"));

        _snprintf(szVBuf, sizeof(szVBuf), "%d C (%d K)", nTempC, (int)wTempK);
        NVME_ADD_ROW("02h", "Composite Temperature", szVBuf,
            (nTempC > 70 ? "Warning" : "OK"));

        char szSpare[32];
        _snprintf(szSpare, sizeof(szSpare), "%d %%", (int)pLog->AvailableSpare);
        NVME_ADD_ROW("03h", "Available Spare", szSpare,
            (pLog->AvailableSpare < pLog->AvailableSpareThreshold ? "WARNING" : "OK"));

        char szSpareThresh[32];
        _snprintf(szSpareThresh, sizeof(szSpareThresh), "%d %%", (int)pLog->AvailableSpareThreshold);
        NVME_ADD_ROW("04h", "Available Spare Threshold", szSpareThresh, "--");

        char szPctUsed[32];
        _snprintf(szPctUsed, sizeof(szPctUsed), "%d %%", (int)pLog->PercentageUsed);
        NVME_ADD_ROW("05h", "Percentage Used (Endurance)", szPctUsed,
            (pLog->PercentageUsed >= 100 ? "Warning" : "OK"));

        char szDUR[64];
        if (qwDataRead > 2048)
            _snprintf(szDUR, sizeof(szDUR), "%I64u GB", (unsigned __int64)(qwDataRead / 2048));
        else
            _snprintf(szDUR, sizeof(szDUR), "%I64u units", qwDataRead);
        NVME_ADD_ROW("06h", "Data Units Read", szDUR, "OK");

        char szDUW[64];
        if (qwDataWritten > 2048)
            _snprintf(szDUW, sizeof(szDUW), "%I64u GB", (unsigned __int64)(qwDataWritten / 2048));
        else
            _snprintf(szDUW, sizeof(szDUW), "%I64u units", qwDataWritten);
        NVME_ADD_ROW("07h", "Data Units Written", szDUW, "OK");

        char szPOH[32];
        _snprintf(szPOH, sizeof(szPOH), "%I64u hours", qwPOH);
        NVME_ADD_ROW("09h", "Power On Hours", szPOH, "OK");

        char szPC[32];
        _snprintf(szPC, sizeof(szPC), "%I64u", qwPowerCycles);
        NVME_ADD_ROW("0Ch", "Power Cycles", szPC, "OK");

        char szUS[32];
        _snprintf(szUS, sizeof(szUS), "%I64u", qwUnsafeSDs);
        NVME_ADD_ROW("10h", "Unsafe Shutdowns", szUS, "OK");

        char szME[32];
        _snprintf(szME, sizeof(szME), "%I64u", qwMediaErr);
        NVME_ADD_ROW("11h", "Media and Data Integrity Errors", szME,
            (qwMediaErr > 0 ? "WARNING" : "OK"));

        char szEL[32];
        _snprintf(szEL, sizeof(szEL), "%I64u", qwErrLog);
        NVME_ADD_ROW("12h", "Number of Error Log Entries", szEL,
            (qwErrLog > 0 ? "Warning" : "OK"));

        char szWCT[32];
        _snprintf(szWCT, sizeof(szWCT), "%u min", pLog->WarningCompTempTime);
        NVME_ADD_ROW("13h", "Warning Composite Temp Time", szWCT,
            (pLog->WarningCompTempTime > 0 ? "Warning" : "OK"));

        char szCCT[32];
        _snprintf(szCCT, sizeof(szCCT), "%u min", pLog->CriticalCompTempTime);
        NVME_ADD_ROW("14h", "Critical Composite Temp Time", szCCT,
            (pLog->CriticalCompTempTime > 0 ? "WARNING" : "OK"));

        #undef NVME_ADD_ROW
        return;
    }

    if (!pInfo->bSMART_Supported) return;

    int i, j, nRow = 0;
    for (i = 0; i < 30; i++) {
        SMART_ATTRIBUTE* pAttr = &pInfo->attrData.stAttributes[i];
        if (pAttr->bAttrID == 0) continue;

        BYTE bThresh = 0;
        for (j = 0; j < 30; j++) {
            if (pInfo->threshData.stThresholds[j].bAttrID == pAttr->bAttrID) {
                bThresh = pInfo->threshData.stThresholds[j].bThresholdValue;
                break;
            }
        }

        BOOL bFailed = (bThresh > 0 && pAttr->bAttrValue <= bThresh);

        LVITEMA lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        char szBuf[64];

        _snprintf(szBuf, sizeof(szBuf), "%02Xh", pAttr->bAttrID);
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = nRow;
        lvi.iSubItem = 0;
        lvi.pszText  = szBuf;
        ListView_InsertItem(hList, &lvi);

        lvi.iSubItem = 1;
        lvi.pszText  = (LPSTR)GetAttrName(pAttr->bAttrID);
        ListView_SetItem(hList, &lvi);

        _snprintf(szBuf, sizeof(szBuf), "%d", pAttr->bAttrValue);
        lvi.iSubItem = 2;
        lvi.pszText  = szBuf;
        ListView_SetItem(hList, &lvi);

        _snprintf(szBuf, sizeof(szBuf), "%d", pAttr->bWorstValue);
        lvi.iSubItem = 3;
        lvi.pszText  = szBuf;
        ListView_SetItem(hList, &lvi);

        _snprintf(szBuf, sizeof(szBuf), "%d", bThresh);
        lvi.iSubItem = 4;
        lvi.pszText  = szBuf;
        ListView_SetItem(hList, &lvi);

        DWORD dwRaw = GetRawValue(pAttr->bRawValue);
        _snprintf(szBuf, sizeof(szBuf), "%u", (unsigned)dwRaw);
        lvi.iSubItem = 5;
        lvi.pszText  = szBuf;
        ListView_SetItem(hList, &lvi);

        lvi.iSubItem = 6;
        if (bFailed)
            lvi.pszText = (LPSTR)"FAILED";
        else if (bThresh > 0 && pAttr->bAttrValue < bThresh + 10)
            lvi.pszText = (LPSTR)"Warning";
        else
            lvi.pszText = (LPSTR)"OK";
        ListView_SetItem(hList, &lvi);

        nRow++;
    }
}

void RefreshData(HWND hWnd)
{
    HWND hBtn = GetDlgItem(hWnd, IDC_REFRESH_BTN);
    EnableWindow(hBtn, FALSE);

    g_nDriveCount = ScanDrives(g_Drives, MAX_DRIVES);

    UpdateDriveCombo(hWnd);

    int nSel = (int)SendMessage(GetDlgItem(hWnd, IDC_DRIVE_LIST), CB_GETCURSEL, 0, 0);
    if (nSel == CB_ERR || nSel >= g_nDriveCount)
        nSel = 0;
    g_nSelectedDrive = nSel;

    UpdateDriveInfo(hWnd, g_nSelectedDrive);
    UpdateAttrList(hWnd, g_nSelectedDrive);
    RepaintHealthBar();
    InvalidateRect(hWnd, NULL, FALSE);
    UpdateWindow(hWnd);

    EnableWindow(hBtn, TRUE);
}

void ShowAboutDialog(HWND hWnd)
{
    MessageBoxA(hWnd,
        "LLHD Monitor  v1.0\r\n"
        "\r\n"
        "Low-Level HDD Monitor\r\n"
		"\r\n"
		"Author: Ari Sohandri Putra\r\n"
		"\r\n"
        "Supports ATA/SATA, NVMe, and USB drives\r\n"
        "\r\n",
        "About LLHD Monitor",
        MB_OK | MB_ICONINFORMATION);
}

void CreateControls(HWND hWnd)
{
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC  = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    HWND hCombo = CreateWindowExA(
        0, "COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        10, 10, 500, 200,
        hWnd, (HMENU)IDC_DRIVE_LIST, g_hInst, NULL
    );
    SendMessage(hCombo, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hBtn = CreateWindowExA(
        0, "BUTTON", "Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        520, 10, 80, 24,
        hWnd, (HMENU)IDC_REFRESH_BTN, g_hInst, NULL
    );
    SendMessage(hBtn, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hLabel = CreateWindowExA(
        0, "STATIC", "DISK HEALTH",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 42, 190, 14,
        hWnd, (HMENU)IDC_HEALTH_LABEL, g_hInst, NULL
    );
    SendMessage(hLabel, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

    g_hHealthBar = CreateWindowExA(
        WS_EX_CLIENTEDGE, "LLHDHealthBar", "",
        WS_CHILD | WS_VISIBLE,
        10, 58, 190, 84,
        hWnd, (HMENU)IDC_HEALTH_BAR_FRAME, g_hInst, NULL
    );

    int nInfoX = 210, nInfoY = 42;
    int nInfoH = 17, nInfoGap = 2;
    int nInfoW = 482;

    HWND hModel = CreateWindowExA(0, "STATIC", "Model        -",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY, nInfoW, nInfoH,
        hWnd, (HMENU)IDC_MODEL_STATIC, g_hInst, NULL);
    SendMessage(hModel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hSerial = CreateWindowExA(0, "STATIC", "Serial No.   -",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 1, nInfoW, nInfoH,
        hWnd, (HMENU)IDC_SERIAL_STATIC, g_hInst, NULL);
    SendMessage(hSerial, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hFirm = CreateWindowExA(0, "STATIC", "Firmware     -",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 2, nInfoW, nInfoH,
        hWnd, (HMENU)IDC_FIRMWARE_STATIC, g_hInst, NULL);
    SendMessage(hFirm, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hSize = CreateWindowExA(0, "STATIC", "Capacity     -",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 3, nInfoW, nInfoH,
        hWnd, (HMENU)IDC_SIZE_STATIC, g_hInst, NULL);
    SendMessage(hSize, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hSmartSt = CreateWindowExA(0, "STATIC", "S.M.A.R.T.   -",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        nInfoX, nInfoY + (nInfoH + nInfoGap) * 4, nInfoW, nInfoH,
        hWnd, (HMENU)IDC_STATUS_STATIC, g_hInst, NULL);
    SendMessage(hSmartSt, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hPred = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 148, 682, 17,
        hWnd, (HMENU)IDC_PREDICT_STATIC, g_hInst, NULL);
    SendMessage(hPred, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    HWND hList = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWA, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        10, 170, 682, 354,
        hWnd, (HMENU)IDC_ATTR_LIST, g_hInst, NULL
    );
    SendMessage(hList, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    ListView_SetExtendedListViewStyle(hList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    ListView_SetBkColor(hList, CLR_PANEL);
    ListView_SetTextBkColor(hList, CLR_ROW1);
    ListView_SetTextColor(hList, CLR_TEXT);

    LVCOLUMNA col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.cx = 42;  col.pszText = (LPSTR)"ID";          ListView_InsertColumn(hList, 0, &col);
    col.cx = 215; col.pszText = (LPSTR)"Attribute";    ListView_InsertColumn(hList, 1, &col);
    col.cx = 48;  col.pszText = (LPSTR)"Value";        ListView_InsertColumn(hList, 2, &col);
    col.cx = 48;  col.pszText = (LPSTR)"Worst";        ListView_InsertColumn(hList, 3, &col);
    col.cx = 62;  col.pszText = (LPSTR)"Threshold";    ListView_InsertColumn(hList, 4, &col);
    col.cx = 90;  col.pszText = (LPSTR)"Raw Value";    ListView_InsertColumn(hList, 5, &col);
    col.cx = 70;  col.pszText = (LPSTR)"Status";       ListView_InsertColumn(hList, 6, &col);
}

static LRESULT HandleCtlColor(HWND hWnd, WPARAM wParam)
{
    HDC hdc = (HDC)wParam;
    SetTextColor(hdc, CLR_TEXT);
    SetBkColor(hdc, CLR_BG);
    return (LRESULT)g_hbrBG;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        g_hMainWnd = hWnd;
        RegisterHealthBarClass(g_hInst);
        CreateGDIObjects();
        CreateControls(hWnd);
        PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_REFRESH_BTN, BN_CLICKED), 0);
        return 0;

    case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, g_hbrBG);
        }
        return 1;

    case WM_CTLCOLORSTATIC:
        return HandleCtlColor(hWnd, wParam);

    case WM_CTLCOLORBTN:
        return (LRESULT)(HBRUSH)(COLOR_BTNFACE + 1);

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, CLR_TEXT);
            SetBkColor(hdc, CLR_PANEL);
            return (LRESULT)g_hbrPanel;
        }

    case WM_NOTIFY:
        {
            NMHDR* pHdr = (NMHDR*)lParam;
            if (pHdr->idFrom == IDC_ATTR_LIST && pHdr->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* pCD = (NMLVCUSTOMDRAW*)lParam;
                switch (pCD->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                    pCD->clrTextBk = (pCD->nmcd.dwItemSpec % 2 == 0) ? CLR_ROW1 : CLR_ROW2;
                    pCD->clrText   = CLR_TEXT;
                    return CDRF_NOTIFYSUBITEMDRAW;
                case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
                    {
                        if (pCD->iSubItem == 6) {
                            char szStatus[32];
                            LVITEMA lvi;
                            ZeroMemory(&lvi, sizeof(lvi));
                            lvi.mask       = LVIF_TEXT;
                            lvi.iItem      = (int)pCD->nmcd.dwItemSpec;
                            lvi.iSubItem   = 6;
                            lvi.pszText    = szStatus;
                            lvi.cchTextMax = sizeof(szStatus);
                            ListView_GetItem(pCD->nmcd.hdr.hwndFrom, &lvi);
                            if (strcmp(szStatus, "FAILED") == 0)
                                pCD->clrText = CLR_RED;
                            else if (strcmp(szStatus, "Warning") == 0)
                                pCD->clrText = CLR_YELLOW;
                            else
                                pCD->clrText = CLR_GREEN;
                        }
                        else if (pCD->iSubItem == 0) {
                            char szID[16];
                            LVITEMA lvi;
                            ZeroMemory(&lvi, sizeof(lvi));
                            lvi.mask       = LVIF_TEXT;
                            lvi.iItem      = (int)pCD->nmcd.dwItemSpec;
                            lvi.iSubItem   = 0;
                            lvi.pszText    = szID;
                            lvi.cchTextMax = sizeof(szID);
                            ListView_GetItem(pCD->nmcd.hdr.hwndFrom, &lvi);
                            BYTE bID = (BYTE)strtol(szID, NULL, 16);
                            if (IsAttrCritical(bID))
                                pCD->clrText = CLR_ACCENT;
                        }
                        return CDRF_NEWFONT;
                    }
                }
            }
        }
        return CDRF_DODEFAULT;

    case WM_COMMAND:
        {
            int nCtrl = LOWORD(wParam);
            int nCode = HIWORD(wParam);

            if (nCtrl == IDC_REFRESH_BTN && nCode == BN_CLICKED) {
                RefreshData(hWnd);
            }
            else if (nCtrl == IDC_DRIVE_LIST && nCode == CBN_SELCHANGE) {
                int nSel = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (nSel != CB_ERR) {
                    g_nSelectedDrive = nSel;
                    UpdateDriveInfo(hWnd, nSel);
                    UpdateAttrList(hWnd, nSel);
                    InvalidateRect(hWnd, NULL, FALSE);
                    UpdateWindow(hWnd);
                }
            }
            else if (nCtrl == IDM_ABOUT) {
                ShowAboutDialog(hWnd);
            }
            else if (nCtrl == IDM_EXIT) {
                DestroyWindow(hWnd);
            }
        }
        return 0;

    case WM_SIZE:
        {
            int cxClient = LOWORD(lParam);
            int cyClient = HIWORD(lParam);
            if (cxClient < 100 || cyClient < 100) break;

            HWND hCombo = GetDlgItem(hWnd, IDC_DRIVE_LIST);
            HWND hBtn   = GetDlgItem(hWnd, IDC_REFRESH_BTN);
            if (hCombo) SetWindowPos(hCombo, NULL, 10, 10, cxClient - 110, 24, SWP_NOZORDER);
            if (hBtn)   SetWindowPos(hBtn,   NULL, cxClient - 95, 10, 85, 24, SWP_NOZORDER);

            int nInfoW = cxClient - 220;
            HWND hArr[] = { GetDlgItem(hWnd, IDC_MODEL_STATIC),
                            GetDlgItem(hWnd, IDC_SERIAL_STATIC),
                            GetDlgItem(hWnd, IDC_FIRMWARE_STATIC),
                            GetDlgItem(hWnd, IDC_SIZE_STATIC),
                            GetDlgItem(hWnd, IDC_STATUS_STATIC) };
            int nInfoY = 42, nInfoH = 17, nInfoGap = 2;
            int k;
            for (k = 0; k < 5; k++) {
                if (hArr[k])
                    SetWindowPos(hArr[k], NULL, 210, nInfoY + (nInfoH + nInfoGap) * k,
                                 nInfoW, nInfoH, SWP_NOZORDER);
            }

            HWND hPred = GetDlgItem(hWnd, IDC_PREDICT_STATIC);
            if (hPred) SetWindowPos(hPred, NULL, 10, 148, cxClient - 20, 17, SWP_NOZORDER);
            HWND hList = GetDlgItem(hWnd, IDC_ATTR_LIST);
            if (hList) {
                int nListTop = 170;
                int nListH   = cyClient - nListTop - 8;
                if (nListH < 50) nListH = 50;
                SetWindowPos(hList, NULL, 10, nListTop, cxClient - 20, nListH, SWP_NOZORDER);
            }
        }
        return 0;

    case WM_DESTROY:
        DestroyGDIObjects();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}
