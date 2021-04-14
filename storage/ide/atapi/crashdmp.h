/*++

Copyright (C) 1997-99  Microsoft Corporation

Module Name:

    crashdmp.h

Abstract:

--*/

#if !defined (___crashdmp_h___)
#define ___crashdmp_h___

#include <idedump.h>

typedef
VOID
(*PSTALL_ROUTINE) (
    IN ULONG Delay
    );
     
typedef struct _CRASHDUMP_INIT_DATA {

    ULONG               CheckSum;

    UCHAR               PathId;
    UCHAR               TargetId;
    UCHAR               Lun;

    PHW_DEVICE_EXTENSION LiveHwDeviceExtension;

} CRASHDUMP_INIT_DATA, *PCRASHDUMP_INIT_DATA;
      
typedef struct _CRASHDUMP_DATA {

    PCRASHDUMP_INIT_DATA    CrashInitData;

    ULONG                   BytesPerSector;

    LARGE_INTEGER           PartitionOffset;

    PSTALL_ROUTINE          StallRoutine;

    SCSI_REQUEST_BLOCK      Srb;

    HW_DEVICE_EXTENSION     HwDeviceExtension;

    ULONG                   MaxBlockSize;

} CRASHDUMP_DATA, *PCRASHDUMP_DATA;

ULONG
AtapiCrashDumpDriverEntry (
    PVOID Context
    );

//
// crash dump privates
//
BOOLEAN
AtapiCrashDumpOpen (
    IN LARGE_INTEGER PartitionOffset
    );

NTSTATUS
AtapiCrashDumpIdeWrite (
    IN PLARGE_INTEGER DiskByteOffset,
    IN PMDL Mdl
    );

VOID
AtapiCrashDumpFinish (
    VOID
    );
                         
NTSTATUS
AtapiCrashDumpIdeWritePio (
    IN PSCSI_REQUEST_BLOCK Srb
    );

NTSTATUS
AtapiDumpCallback(
    IN PKBUGCHECK_DATA BugcheckData,
    IN PVOID BugcheckBuffer,
    IN ULONG BugcheckBufferLength,
    IN PULONG BugcheckBufferUsed
    );


//
// Validate that the duplicate definitions in IDEDUMP.H are the same as
// the ATAPI ones are.
//

C_ASSERT (sizeof (COMMAND_LOG) == sizeof (ATAPI_DUMP_COMMAND_LOG));
C_ASSERT (sizeof (ATAPI_DUMP_BMSTATUS) == sizeof (BMSTATUS));
C_ASSERT (ATAPI_DUMP_COMMAND_LOG_COUNT == MAX_COMMAND_LOG_ENTRIES);
C_ASSERT (ATAPI_DUMP_BMSTATUS_NO_ERROR == BMSTATUS_NO_ERROR);
C_ASSERT (ATAPI_DUMP_BMSTATUS_NOT_REACH_END_OF_TRANSFER == BMSTATUS_NOT_REACH_END_OF_TRANSFER);
C_ASSERT (ATAPI_DUMP_BMSTATUS_ERROR_TRANSFER == BMSTATUS_ERROR_TRANSFER);
C_ASSERT (ATAPI_DUMP_BMSTATUS_INTERRUPT == BMSTATUS_INTERRUPT);

#endif // ___crashdmp_h___

