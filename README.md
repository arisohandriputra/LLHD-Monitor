**LLHD Monitor v1.0**
=================
LOW-LEVEL HDD MONITOR
Supports ATA/SATA, NVMe, and USB drives

<img width="706" height="588" alt="image" src="https://github.com/user-attachments/assets/6ffc89e9-0ea2-4463-b915-68aaaca1d90e" />

BUILD REQUIREMENTS
------------------
- Visual Studio
- Windows SDK (included with VS)
- No external dependencies

HOW TO BUILD
------------
1. Open LLHDMonitor.sln in Visual Studio
2. Select Release | x86 (or x64)
3. Build Solution (F7)

RUNTIME REQUIREMENTS
--------------------
- Windows XP SP3 or later (10/11)
- comctl32.dll version 6.0 (included via manifest)

FEATURES
--------
- Reads S.M.A.R.T. data from ATA/SATA hard drives and SSDs
- Reads NVMe Health Information Log from NVMe SSDs
- Attempts SAT passthrough for USB-connected drives
- Displays all SMART attributes with ID, value, worst, threshold, raw value, status
- Color-coded health bar (green/yellow/red)
- Drive health percentage estimation
- Failure prediction indicator

NOTES
-----
- USB drives may not expose SMART data (depends on USB-SATA bridge)
- NVMe drives show Health Information Log fields instead of classic SMART attributes
