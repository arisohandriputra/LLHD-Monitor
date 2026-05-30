// Project : LLHD Monitor - Low-Level HDD Monitor
// File    : smart_history.h - S.M.A.R.T. historical data storage and graph declarations

#pragma once
#ifndef SMART_HISTORY_H
#define SMART_HISTORY_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "smart.h"


#ifndef MAX_DRIVES
#define MAX_DRIVES 8
#endif

#define HISTORY_MAX_SAMPLES     120
#define HISTORY_TRACKED_ATTRS   8
#define HISTORY_SERIAL_LEN      21
#define HISTORY_FILE_MAGIC      0x4C4C4844
#define HISTORY_FILE_VERSION    1


static const BYTE g_HistoryAttrIDs[HISTORY_TRACKED_ATTRS] = {
    0xC2,
    0x05,
    0xC5,
    0xC6,
    0x09,
    0x0C,
    0x01,
    0xC7
};

static const char* g_HistoryAttrShortNames[HISTORY_TRACKED_ATTRS] = {
    "Temp (C)",
    "Realloc'd Sect.",
    "Pending Sect.",
    "Uncorrect. Sect.",
    "Power-On Hrs",
    "Power Cycles",
    "Read Err Rate",
    "CRC Errors"
};


typedef struct _HISTORY_SAMPLE {
    DWORD   dwTimestamp;
    int     nHealthPercent;
    DWORD   dwAttrRaw[HISTORY_TRACKED_ATTRS];
} HISTORY_SAMPLE;

typedef struct _DRIVE_HISTORY {
    char            szSerial[HISTORY_SERIAL_LEN];
    int             nSampleCount;
    int             nWriteHead;
    HISTORY_SAMPLE  aSamples[HISTORY_MAX_SAMPLES];
} DRIVE_HISTORY;


#define LLHD_GRAPH_CLASS        "LLHDGraphWnd"
#define IDC_GRAPH_WND           2100
#define IDC_GRAPH_COMBO_ATTR    2101
#define IDC_GRAPH_BTN_CLEAR     2102
#define IDC_GRAPH_LABEL_ATTR    2103
#define IDC_GRAPH_TAB           2104
#define IDC_HISTORY_BTN         2105


#define GRAPH_TAB_HEALTH        0
#define GRAPH_TAB_ATTR          1


extern DRIVE_HISTORY    g_DriveHistory[MAX_DRIVES];


void    History_Init(void);


void    History_Record(DRIVE_INFO* pInfo, int nDriveCount);


BOOL    History_Save(void);
BOOL    History_Load(void);


void    History_Clear(const char* szSerial);


int     History_FindSlot(const char* szSerial);


void    Graph_RegisterClass(HINSTANCE hInst);


void    Graph_ShowWindow(HWND hParent, HINSTANCE hInst, int nDriveIdx);


void    Graph_Repaint(void);


void    Graph_DestroyAll(void);


void    Graph_Paint(HDC hdc, const RECT* prcClient,
                    const DRIVE_HISTORY* pHist,
                    int nAttrIdx,
                    HFONT hFontSmall, HFONT hFontNormal);

#endif
