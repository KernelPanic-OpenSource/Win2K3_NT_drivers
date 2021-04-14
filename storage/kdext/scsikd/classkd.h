
/*++

Copyright (C) Microsoft Corporation, 1999 - 1999

Module Name:

    classkd.h

Abstract:

    Debugger Extension header file

Author:

Environment:

Revision History:

--*/


#define BAD_VALUE  (ULONG64)-1


VOID
ClassDumpFdoExtensionExternal(
    IN IN ULONG64 FdoExtAddr,
    IN ULONG Detail,
    IN ULONG Depth
    );

VOID
ClassDumpFdoExtensionInternal(
    IN ULONG64 FdoDataAddr,
    IN ULONG Detail,
    IN ULONG Depth
    );

VOID
ClassDumpIds(
    ULONG64 devObjAddr,
    ULONG   detail
    );


char *DbgGetIoctlStr(ULONG ioctl);
char *DbgGetScsiOpStr(UCHAR ScsiOp);
char *DbgGetSrbStatusStr(UCHAR SrbStat);
char *DbgGetSenseCodeStr(UCHAR SrbStat, ULONG64 SenseDataAddr);
char *DbgGetAdditionalSenseCodeStr(UCHAR SrbStat, ULONG64 SenseDataAddr);
char *DbgGetAdditionalSenseCodeQualifierStr(UCHAR SrbStat, ULONG64 SenseDataAddr);
char *DbgGetMediaTypeStr(ULONG MediaType);
ULONG64 GetULONGField(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
USHORT GetUSHORTField(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
UCHAR GetUCHARField(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
ULONG64 GetFieldAddr(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
ULONG64 GetContainingRecord(ULONG64 FieldAddr, LPCSTR StructType, LPCSTR FieldName);

VOID ClassDumpTransferPacket(
    ULONG64 PktAddr, 
    BOOLEAN DumpPendingPkts, 
    BOOLEAN DumpFreePkts, 
    BOOLEAN DumpFullInfo, 
    ULONG Depth);

VOID ClassDumpTransferPacketLists(ULONG64 FdoDataAddr, ULONG Detail, ULONG Depth);
VOID ClassDumpPrivateErrorLogs(ULONG64 FdoDataAddr, ULONG Detail, ULONG Depth);
VOID ClassDumpPrivatePacketLogs(ULONG64 FdoDataAddr, ULONG Detail, ULONG Depth);
BOOLEAN ClassTryShowAllFDOs(ULONG Detail);


extern char *g_genericErrorHelpStr;



