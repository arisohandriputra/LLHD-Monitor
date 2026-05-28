# LLHD Monitor - Low-Level HDD Health Monitor

A free, lightweight Windows utility that reads **S.M.A.R.T. data** directly from your HDD, SSD, NVMe, and USB drives and turns it into a plain health and performance score with live temperature, read/write speed measurement, and a system tray that keeps watch in the background.

**Website & Documentation → [hddmonitor.github.io](https://hddmonitor.github.io/)**

---

## Features

- Real-time **health and performance scores** calculated from live S.M.A.R.T. attributes, refreshed every 5 seconds
- Full **S.M.A.R.T. attribute table** with 80+ attributes including current value, worst value, threshold, raw reading, and pass/fail status
- **NVMe support** via Health Info Log Page (0x02), including composite temperature, available spare, percentage used, power cycles, unsafe shutdowns, media errors, and more
- Supports **HDD, SATA SSD, M.2 SATA SSD, NVMe SSD, and USB/External** drives
- **Live temperature** (°C) and **read/write speed** (MB/s) per drive
- **System tray** with a per-drive health icon: hover for tooltip, right-click for menu, balloon notifications on health changes
- **Start with Windows** - optional autostart on login, toggleable from the tray menu, launches minimized to tray so it stays out of your way
- Multi-drive sidebar with up to 8 drives and color-coded health indicators (green >= 70%, yellow 40-69%, red < 40%)
- **Single-instance guard** - launching again brings the existing window to the front
- Direct low-level hardware access via Windows IOCTL with no middle layers and no abstraction
- Single portable `.exe` under 1 MB with no installer, no background service, and no telemetry

## Screenshots

<img width="756" height="618" alt="image" src="https://github.com/user-attachments/assets/2a8e1ca1-9f3e-48f1-9771-03958680cbdc" />

## Requirements

| | |
|---|---|
| OS | Windows 7 / 8 / 10 / 11 (64-bit recommended) |
| Runtime | None - statically linked, no external dependencies |

## Usage

1. Download `LLHDMonitor.exe` from [Releases](../../releases)
2. Double-click to launch. Windows will show a **UAC prompt** - click **Yes**
3. Your drives appear automatically in the left panel. Click any drive to see its full health report

> The application's embedded manifest requests administrator rights automatically (`requireAdministrator`). You do not need to right-click and choose "Run as administrator" - the UAC prompt appears on its own. LLHD Monitor does **not** write anything to your drives.

On first launch, **Start with Windows** is enabled automatically. You can toggle it any time by right-clicking the tray icon.

For a full usage guide, see the **[Documentation](https://hddmonitor.github.io/docs.html)**.

## Building from Source

Requires **MinGW-w64** (`g++` + `windres`).

```sh
# Clone the repo
git clone https://github.com/arisohandriputra/llhd-monitor.git
cd llhd-monitor

# Build (auto-detects compiler)
make

# Output
bin/LLHDMonitor.exe
```

To clean build artifacts:

```sh
make clean
```

Cross-compiling on Linux requires `x86_64-w64-mingw32-g++` and `x86_64-w64-mingw32-windres`. The Makefile detects the platform and selects the right toolchain automatically.

## Project Structure

```
llhd-monitor/
├── src/
│   ├── main.cpp        # Entry point, single-instance guard, window registration
│   ├── mainwnd.cpp     # Main window, UI logic, system tray, autostart
│   ├── mainwnd.h       # Window constants, control IDs, function declarations
│   ├── smart.cpp       # S.M.A.R.T. and NVMe data acquisition, speed measurement
│   ├── smart.h         # Structures, constants, 80+ attribute name table
│   ├── resource.h      # Resource ID definitions
│   ├── app.rc          # Windows resource file (icon, version info)
│   ├── app.manifest    # UAC elevation + Common Controls v6 + DPI awareness
│   └── app.ico
└── Makefile
```

## Drive Support

| Type | Detection Method |
|---|---|
| HDD | `SMART_SEND_DRIVE_COMMAND` + ATA pass-through |
| SATA SSD | S.M.A.R.T. via `SMART_RCV_DRIVE_DATA` |
| M.2 SATA SSD | Auto-detected by querying adapter bus type |
| NVMe SSD | `StorageDeviceProtocolSpecificProperty` (Log Page 0x02) |
| USB / External | SAT (SCSI/ATA Translation) pass-through |

## License

Released under the **MIT License**. See [LICENSE](LICENSE) for details.

## Author

**Ari Sohandri Putra** - [github.com/arisohandriputra](https://github.com/arisohandriputra)
