// Original Build Date : 13/03/2012
// Author  : Ari Sohandri Putra
// GitHub  : https://github.com/arisohandriputra
// Project : LLHD Monitor - Low-Level HDD Monitor
// File    : mingw_compat.h - MinGW compatibility shims for SCSI and Windows version targets

#pragma once
#ifndef MINGW_COMPAT_H
#define MINGW_COMPAT_H

#ifndef WINVER
#  define WINVER         0x0600
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT   0x0600
#endif
#ifndef _WIN32_IE
#  define _WIN32_IE      0x0600
#endif
#ifndef NTDDI_VERSION
#  define NTDDI_VERSION  0x06000000
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

#ifndef _NTDDSCSI_H_
#define _NTDDSCSI_H_

#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_PASS_THROUGH         CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED,   FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED,   FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2

#pragma pack(push, 1)
typedef struct _SCSI_PASS_THROUGH_DIRECT {
    USHORT  Length;
    UCHAR   ScsiStatus;
    UCHAR   PathId;
    UCHAR   TargetId;
    UCHAR   Lun;
    UCHAR   CdbLength;
    UCHAR   SenseInfoLength;
    UCHAR   DataIn;
    ULONG   DataTransferLength;
    ULONG   TimeOutValue;
    PVOID   DataBuffer;
    ULONG   SenseInfoOffset;
    UCHAR   Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

typedef struct _SRB_IO_CONTROL {
    ULONG  HeaderLength;
    UCHAR  Signature[8];
    ULONG  Timeout;
    ULONG  ControlCode;
    ULONG  ReturnCode;
    ULONG  Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;
#pragma pack(pop)

#endif

#endif
