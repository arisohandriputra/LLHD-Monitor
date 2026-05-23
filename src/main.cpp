#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "mainwnd.h"
#include "smart.h"
#include "resource.h"

static BOOL IsElevated(void)
{
    BOOL bElevated = FALSE;
    HANDLE hToken  = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION te;
        DWORD dwSize = 0;
        if (GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &dwSize))
            bElevated = te.TokenIsElevated;
        CloseHandle(hToken);
    }
    return bElevated;
}

static void RequestElevation(void)
{
    char szExe[MAX_PATH];
    GetModuleFileNameA(NULL, szExe, sizeof(szExe));
    ShellExecuteA(NULL, "runas", szExe, NULL, NULL, SW_SHOW);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    g_hInst = hInstance;

    OSVERSIONINFOA osv;
    ZeroMemory(&osv, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    GetVersionExA(&osv);

    if (osv.dwMajorVersion >= 6) {
        if (!IsElevated()) {
            int nRet = MessageBoxA(NULL,
                "LLHD Monitor requires Administrator privileges to\n"
                "read S.M.A.R.T. data from drives.\n\n"
                "Click Yes to restart as Administrator,\n"
                "or No to run in limited mode (no SMART data).",
                "LLHD Monitor - Elevation Required",
                MB_YESNO | MB_ICONQUESTION);
            if (nRet == IDYES) {
                RequestElevation();
                return 0;
            }
        }
    }

    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "LLHDMonitorMainWnd";
    wc.hIcon         = LoadIconA(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIconA(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "RegisterClassEx failed!", "Error", MB_ICONERROR);
        return 1;
    }

    int nScrW = GetSystemMetrics(SM_CXSCREEN);
    int nScrH = GetSystemMetrics(SM_CYSCREEN);
    int nX    = (nScrW - WINDOW_W) / 2;
    int nY    = (nScrH - WINDOW_H) / 2;

    HMENU hMenuBar = CreateMenu();
    HMENU hMenuFile = CreatePopupMenu();
    HMENU hMenuHelp = CreatePopupMenu();

    AppendMenuA(hMenuFile, MF_STRING, IDM_EXIT,  "E&xit");
    AppendMenuA(hMenuHelp, MF_STRING, IDM_ABOUT, "&About LLHD Monitor...");

    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hMenuFile, "&File");
    AppendMenuA(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, "&Help");

    HWND hWnd = CreateWindowExA(
        0,
        "LLHDMonitorMainWnd",
        "LLHD Monitor - LOW-LEVEL HDD MONITOR",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        nX, nY, WINDOW_W, WINDOW_H,
        NULL, hMenuBar, hInstance, NULL
    );

    if (!hWnd) {
        MessageBoxA(NULL, "CreateWindow failed!", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
