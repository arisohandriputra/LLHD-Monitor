# LLHD Monitor
**Low-Level HDD Monitor** - lightweight Windows disk health monitor using S.M.A.R.T. data.

---

## Features
- **Drive support** > ATA / SATA HDD, SATA SSD, M.2 SATA, NVMe, USB (SAT passthrough)
- **S.M.A.R.T. attributes** > full table with color-coded status badges (OK / Warning / FAILED)
- **Health & Performance bars** > visual percentage with gradient indicators
- **Temperature monitoring** > per-drive in Celsius, shown in drive list
- **History graph** > tracks health over time, saved across sessions
- **System tray** > health icon per drive, right-click menu
- **Smart alerts** (tray balloon, no spam) > fires only on critical conditions:
  - Temperature ≥ 55 °C (warning) / ≥ 65 °C (critical)
  - Health < 40% (warning) / < 20% (critical)
  - Drive self-reports failure (SMART `Predict Failure`)
  - NVMe critical warning bits set
  - Reallocated or uncorrectable sectors detected
- **Screenshot export** > `File › Save Screenshot` or `Ctrl+S`, saves cursor-free PNG to `Documents\LLHD_Screenshots\`

## Requirements
- Windows XP/Vista/7/8/8.1/10/11

## Build
```
make
```
Requires **MinGW-w64** (`x86_64-w64-mingw32-g++`). Output: `bin/LLHDMonitor.exe`.

## Configuration
Alert thresholds are `#define` constants at the top of `src/mainwnd.cpp`:
```c
#define ALERT_TEMP_WARN_C      55
#define ALERT_TEMP_CRITICAL_C  65
#define ALERT_HEALTH_WARN      40
#define ALERT_HEALTH_CRITICAL  20
```
