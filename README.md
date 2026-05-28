# LLHD Monitor - Low-Level HDD Health Monitor

A free, lightweight Windows utility that reads **S.M.A.R.T. data** directly from your HDD, SSD, and NVMe drives and turns it into a plain health and performance score.

**Website & Documentation → [hddmonitor.github.io](https://hddmonitor.github.io/)**

---

## Features

- Real-time **health and performance scores** calculated from live S.M.A.R.T. attributes
- Full **S.M.A.R.T. attribute table** with current value, worst value, threshold, and raw reading
- **NVMe support** via Health Info Log Page (0x02)
- Supports **HDD, SATA SSD, NVMe, and USB** drives
- Direct low-level hardware access — no middle layers, no abstraction
- Multi-drive sidebar — see all drives at a glance with color-coded health
- Single portable `.exe`, no installer, no background service, no telemetry

## Screenshots

<img width="756" height="618" alt="image" src="https://github.com/user-attachments/assets/2a8e1ca1-9f3e-48f1-9771-03958680cbdc" />

## Requirements

| | |
|---|---|
| OS | Windows 7 / 8 / 10 / 11 (32-bit or 64-bit) |
| Privileges | Administrator (required for hardware access) |
| Runtime | None — statically linked, no external dependencies |

## Usage

1. Download `LLHDMonitor.exe` from [Releases](../../releases)
2. Select a drive from the sidebar and read the results

> Administrator is required because reading raw S.M.A.R.T. data uses kernel-level Windows I/O control codes. The application does **not** write anything to your drives.

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

## Project Structure

```
llhd-monitor/
├── src/
│   ├── main.cpp        # Entry point and single-instance guard
│   ├── mainwnd.cpp     # Main window and UI logic
│   ├── mainwnd.h
│   ├── smart.cpp       # S.M.A.R.T. and NVMe data acquisition
│   ├── smart.h         # Structures, constants, attribute names
│   ├── resource.h
│   ├── app.rc          # Windows resource file
│   ├── app.manifest    # Common Controls v6 manifest
│   └── app.ico
└── Makefile
```

## License

Released under the **MIT License**. See [LICENSE](LICENSE) for details.

## Author

**Ari Sohandri Putra** — [github.com/arisohandriputra](https://github.com/arisohandriputra)
