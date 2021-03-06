/////////////////////////////////////////////////////////////////////////////
//
//
// Copyright (c) 1996, 1997  Microsoft Corporation
//
//
// Module Name:
//      test.c
//
// Abstract:
//
//
//
// Author:
//
//      P Porzuczek
//
// Environment:
//
// Revision History:
//
//
//////////////////////////////////////////////////////////////////////////////

//
//
#include <wdm.h>
#include <memory.h>
#include "Main.h"

#if DBG

/////////////////////////////////////////////////////////////////////////////
//
// Default debug mode
//

ULONG TestDebugFlag = TEST_DBG_NONE;

/////////////////////////////////////////////////////////////////////////////
// Debugging definitions
//


//
// Debug tracing defintions
//
#define TEST_LOG_SIZE 256
UCHAR TestLogBuffer[TEST_LOG_SIZE]={0};
ULONG TestLogLoc = 0;

/////////////////////////////////////////////////////////////////////////////
//
// Logging function in debug builds
//
extern VOID
TestLog (
    UCHAR c         // input character
    )
/////////////////////////////////////////////////////////////////////////////
{
    TestLogBuffer[TestLogLoc++] = c;

    TestLogBuffer[(TestLogLoc + 4) % TEST_LOG_SIZE] = '\0';

    if (TestLogLoc >= TEST_LOG_SIZE) {
        TestLogLoc = 0;
    }
}

#else // DBG == 0

ULONG TestDebugFlag = 0;

#endif // DBG

//////////////////////////////////////////////////////////////////////////////////////
NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT    pDriverObject,
    IN PUNICODE_STRING   pszuRegistryPath
    )
//////////////////////////////////////////////////////////////////////////////////////
{
    NTSTATUS                        ntStatus = STATUS_SUCCESS;

    //
    // Register the Slip Class binding
    //
    ntStatus = SlipDriverInitialize (pDriverObject,  pszuRegistryPath);
    if (ntStatus != STATUS_SUCCESS)
    {
        goto ret;
    }

ret:

    TEST_DEBUG (TEST_DBG_TRACE, ("Driver Entry complete, ntStatus: %08X\n", ntStatus));

    return ntStatus;
}
