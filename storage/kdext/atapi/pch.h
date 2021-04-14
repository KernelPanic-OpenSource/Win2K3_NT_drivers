//+-------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992 - 1999
//
//  File:       PCH.cxx
//
//  Contents:   Pre-compiled header
//
//  History:    21-Dec-92       BartoszM        Created
//
//--------------------------------------------------------------------------

// #define KDEXTMODE // BUGBUG ?


#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <ntosp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <scsi.h>
#include <srb.h>

#define KDEXT_64BIT
#include <wdbgexts.h>
#include <dbgeng.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// undef the wdbgexts
//
#undef DECLARE_API
#define DECLARE_API(extension) CPPMOD HRESULT CALLBACK extension(PDEBUG_CLIENT Client, PCSTR args)

#undef ASSERT
#undef ASSERTMSG
#if DBG
    #define ASSERT( exp ) if (!(exp)){ RtlAssert( #exp, __FILE__, __LINE__, NULL ); }
    #define ASSERTMSG( msg, exp ) if (!(exp)){ RtlAssert( #exp, __FILE__, __LINE__, msg ); }
#else
    #define ASSERT( exp )
    #define ASSERTMSG( msg, exp )
#endif 


#define PCI_SLOT_NUMBER ULONG



#define OFFSET(struct, elem)    ((char *) &(struct->elem) - (char *) struct)

#define _DRIVER

#define KDBG_EXT

#include "ntdddisk.h"
#include "ntddscsi.h"
#include "ntddstor.h"

#include "wmistr.h"

#define PRINT_FLAGS(Flags,b)      if (Flags & b) {dprintf(#b", ");}

#define BAD_VALUE  (ULONG64)-1

typedef struct {
    ULONG Flag;
    PUCHAR Name;
} FLAG_NAME, *PFLAG_NAME;

#define FLAG_NAME(flag)           {flag, #flag}

extern FLAG_NAME SrbFlags[];
extern FLAG_NAME LuFlags[];
extern FLAG_NAME PortFlags[];
extern FLAG_NAME DevFlags[];

PUCHAR
DevicePowerStateToString(
    IN DEVICE_POWER_STATE State
    );

PUCHAR
SystemPowerStateToString(
    IN DEVICE_POWER_STATE State
    );

VOID
xdprintf(
    ULONG  Depth,
    PCCHAR S,
    ...
    );

VOID
DumpFlags(
    ULONG Depth,
    PUCHAR Name,
    ULONG Flags,
    PFLAG_NAME FlagTable
    );

ULONG64 GetULONGField(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
USHORT GetUSHORTField(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
UCHAR GetUCHARField(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
ULONG64 GetFieldAddr(ULONG64 StructAddr, LPCSTR StructType, LPCSTR FieldName);
ULONG64 GetContainingRecord(ULONG64 FieldAddr, LPCSTR StructType, LPCSTR FieldName);
ULONG DumpObjListQueue(ULONG Depth, PCHAR Title, PCHAR StructName, PCHAR ListEntryFieldName, ULONG64 ListHeadAddr);
ULONG DumpIrpQueue(ULONG Depth, PCHAR Title, ULONG64 ListHeadAddr);
ULONG DumpIrpDeviceQueue(ULONG Depth, PCHAR Title, ULONG64 ListHeadAddr);
BOOLEAN FindObjInListQueue(ULONG64 ListHeadAddr, ULONG64 SpecificObjAddr, PCHAR StructName, PCHAR ListEntryFieldName);
BOOLEAN FindIrpInQueue(ULONG64 ListHeadAddr, ULONG64 IrpAddr);
BOOLEAN FindIrpInDeviceQueue(ULONG64 ListHeadAddr, ULONG64 IrpAddr);
BOOLEAN IsAtapiPdo(ULONG64 DevObjAddr);
BOOLEAN IsAtapiFdo(ULONG64 DevObjAddr);
BOOLEAN FindIrpInPdo(ULONG Depth, ULONG64 PdoAddr, ULONG64 IrpAddr, BOOLEAN SearchParent);
BOOLEAN FindIrpInFdo(ULONG Depth, ULONG64 FdoAddr, ULONG64 IrpAddr);



extern WINDBG_EXTENSION_APIS ExtensionApis;

#ifdef __cplusplus
}
#endif
