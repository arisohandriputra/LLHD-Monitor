# LLHD Monitor

Low-Level HDD Health Monitor for Windows.
Reads S.M.A.R.T. data directly from your drives and gives you a plain health score,
live temperature, read/write speed, and a system tray icon that keeps watch in the background.

Homepage: http://hddmonitor.github.io/


## About

Written on March 13, 2012 by Ari Sohandri Putra as a personal tool for monitoring
hard drive health without having to deal with bloated third-party software.
The source code here is preserved as-is from the original write date.
It talks directly to the hardware through Windows IOCTL calls, no abstraction layers,
no background services, no extra junk.

<img width="751" height="574" alt="Capture" src="https://github.com/user-attachments/assets/0e078788-322b-4c13-8e2b-ec890f866c19" />

## Features

- Health and performance scores calculated from live S.M.A.R.T. data, refreshed every 5 seconds
- Full S.M.A.R.T. attribute table with 80+ attributes (current, worst, threshold, raw, pass/fail)
- NVMe support via Health Info Log Page (0x02)
- Supports HDD, SATA SSD, M.2 SATA SSD, NVMe SSD, and USB/external drives
- Live temperature (C) and read/write speed (MB/s) per drive
- System tray icon per drive with tooltip, right-click menu, and balloon notifications on health changes
- Optional autostart on Windows login, launches minimized to tray
- Up to 8 drives in the sidebar, color coded green (>=70%), yellow (40-69%), red (<40%)
- Single instance, launching again just brings the existing window forward
- Single portable .exe under 1 MB, no installer, no background service, no telemetry


## Requirements

- Windows 7 / 8 / 10 / 11 (64-bit recommended)
- No runtime or external dependencies, statically linked


## Usage

1. Download LLHDMonitor.exe from the Releases page
2. Double-click to run. Windows will ask for UAC permission, click Yes
3. Your drives appear in the left panel automatically. Click any drive to see its report.

The app requests admin rights on its own via the embedded manifest.
You do not need to right-click "Run as administrator".
LLHD Monitor does not write anything to your drives.

Start with Windows is enabled by default on first launch.
You can toggle it anytime from the tray icon right-click menu.

Docs: http://hddmonitor.github.io/docs.html


## Drive Support

    HDD             SMART_SEND_DRIVE_COMMAND + ATA pass-through
    SATA SSD        S.M.A.R.T. via SMART_RCV_DRIVE_DATA
    M.2 SATA SSD    auto-detected by querying adapter bus type
    NVMe SSD        StorageDeviceProtocolSpecificProperty (Log Page 0x02)
    USB / External  SAT (SCSI/ATA Translation) pass-through


## Building

Requires MinGW-w64 (g++ and windres).

    git clone https://github.com/arisohandriputra/llhd-monitor.git
    cd llhd-monitor
    make

Output: bin/LLHDMonitor.exe

Clean build artifacts:

    make clean

Cross-compiling on Linux requires x86_64-w64-mingw32-g++ and x86_64-w64-mingw32-windres.
The Makefile detects the platform and picks the right toolchain automatically.


## Source Layout

    llhd-monitor/
    |-- src/
    |   |-- main.cpp       entry point, single-instance guard, window registration
    |   |-- mainwnd.cpp    main window, UI logic, system tray, autostart
    |   |-- mainwnd.h      window constants, control IDs, function declarations
    |   |-- smart.cpp      S.M.A.R.T. and NVMe data acquisition, speed measurement
    |   |-- smart.h        structures, constants, 80+ attribute name table
    |   |-- resource.h     resource ID definitions
    |   |-- app.rc         Windows resource file (icon, version info)
    |   |-- app.manifest   UAC elevation + Common Controls v6 + DPI awareness
    |   `-- app.ico
    `-- Makefile


## License

MIT. See LICENSE.


## Author

Ari Sohandri Putra
http://github.com/arisohandriputra
