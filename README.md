# LLHD Monitor

![Release](https://img.shields.io/badge/release-v1.0-blue) ![Platform](https://img.shields.io/badge/platform-Windows-lightgrey) ![License](https://img.shields.io/badge/license-MIT-green)

**Low-Level HDD Monitor** is a lightweight Windows disk health monitor
built upon S.M.A.R.T. data retrieval.

---

## Features

- **Drive Support** -- ATA/SATA HDD, SATA SSD, M.2 SATA, NVMe, USB (SAT passthrough)
- **S.M.A.R.T. Attributes** -- full attribute table with color-coded status badges (OK / Warning / FAILED)
- **Health and Performance Bars** -- visual percentage readout with gradient indicators
- **Temperature Monitoring** -- per-drive readout in Celsius, displayed in the drive list
- **History Graph** -- tracks health over time; state is preserved across sessions
- **System Tray** -- health icon per drive with a right-click context menu
- **Smart Alerts** (tray balloon, suppressed until critical) -- fires only under the following conditions:

  - Temperature at or above 55 deg C (warning) or 65 deg C (critical)
  - Health below 40 percent (warning) or 20 percent (critical)
  - Drive self-reports failure via S.M.A.R.T. Predict Failure
  - NVMe critical warning bits are set
  - Reallocated or uncorrectable sectors are detected

- **Screenshot Export** -- accessible from "File / Save Screenshot" or Ctrl+S;
  saves a cursor-free PNG to "Documents\LLHD_Screenshots\"

---

## Requirements

- Windows XP/Vista/7/8/8.1/10/11

---

## Build

```
make
```

Requires **MinGW-w64** (`x86_64-w64-mingw32-g++`). Output binary: `bin/LLHDMonitor.exe`.

---

## Configuration

Alert thresholds are defined as constants near the top of `src/mainwnd.cpp`:

```c
#define ALERT_TEMP_WARN_C      55
#define ALERT_TEMP_CRITICAL_C  65
#define ALERT_HEALTH_WARN      40
#define ALERT_HEALTH_CRITICAL  20
```

---

## License

MIT. See [LICENSE](LICENSE).

---

## Changelog

### v1.0

- Smart tray alerts covering temperature, health percentage, S.M.A.R.T. failure
  prediction, NVMe critical warnings, and reallocated or uncorrectable sectors
- Suppressed noisy hot-plug connect and disconnect notifications
- Added menubar with three menus: File (Save Screenshot, Exit),
  View (History Graph), and Help (About)
- Screenshot export to PNG via "File / Save Screenshot" or Ctrl+S;
  no cursor captured; output saved to "Documents\LLHD_Screenshots\"
- Save Screenshot is also accessible from the system tray right-click menu
