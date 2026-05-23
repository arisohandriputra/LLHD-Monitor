#pragma once
#ifndef MAINWND_H
#define MAINWND_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "smart.h"
#include "resource.h"
#define IDC_DRIVE_LIST       1001
#define IDC_HEALTH_STATIC    1002
#define IDC_HEALTH_BAR_FRAME 1003
#define IDC_MODEL_STATIC     1004
#define IDC_SERIAL_STATIC    1005
#define IDC_FIRMWARE_STATIC  1006
#define IDC_SIZE_STATIC      1007
#define IDC_STATUS_STATIC    1008
#define IDC_ATTR_LIST        1009
#define IDC_REFRESH_BTN      1010
#define IDC_HEALTH_LABEL     1011
#define IDC_PREDICT_STATIC   1012
#define IDM_ABOUT            2001
#define IDM_EXIT             2002
#define IDT_REFRESH          3001
#define REFRESH_INTERVAL_MS  5000
#define WINDOW_W    710
#define WINDOW_H    590
#define CLR_BG          RGB(240, 242, 247)
#define CLR_PANEL       RGB(255, 255, 255)
#define CLR_BORDER      RGB(180, 185, 200)
#define CLR_TEXT        RGB(30,  35,  50)
#define CLR_TEXT_DIM    RGB(100, 105, 120)
#define CLR_GREEN       RGB(30,  150, 60)
#define CLR_YELLOW      RGB(180, 120, 0)
#define CLR_RED         RGB(190, 30,  30)
#define CLR_ACCENT      RGB(30,  100, 210)
#define CLR_HEADER      RGB(225, 228, 240)
#define CLR_ROW1        RGB(255, 255, 255)
#define CLR_ROW2        RGB(245, 247, 252)
#define MAX_DRIVES 8

extern DRIVE_INFO  g_Drives[MAX_DRIVES];
extern int         g_nDriveCount;
extern int         g_nSelectedDrive;
extern HINSTANCE   g_hInst;
extern HWND        g_hMainWnd;
extern HWND        g_hHealthBar;
extern HBRUSH  g_hbrBG;
extern HBRUSH  g_hbrPanel;
extern HBRUSH  g_hbrGreen;
extern HBRUSH  g_hbrYellow;
extern HBRUSH  g_hbrRed;
extern HFONT   g_hFontTitle;
extern HFONT   g_hFontNormal;
extern HFONT   g_hFontSmall;
extern HFONT   g_hFontBig;

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HealthBarWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void    RegisterHealthBarClass(HINSTANCE hInst);
void    RepaintHealthBar(void);
void    CreateControls(HWND hWnd);
void    CreateGDIObjects(void);
void    DestroyGDIObjects(void);
void    RefreshData(HWND hWnd);
void    UpdateDriveCombo(HWND hWnd);
void    UpdateDriveInfo(HWND hWnd, int nDrive);
void    UpdateAttrList(HWND hWnd, int nDrive);
void    PaintMain(HWND hWnd, HDC hdc);
void    ShowAboutDialog(HWND hWnd);
COLORREF GetHealthColor(int nHealth);

#endif
