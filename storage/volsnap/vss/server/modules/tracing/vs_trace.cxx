/*++

Copyright (c) 1998  Microsoft Corporation

Module Name:

    vs_trace.cxx

Abstract:

    This module defines the global debug\trace facilities used by the
	Long Term Storage service.
	
	Previous name: bsdebug.cxx

Author:


Revision History:
	Name		Date		Comments
    ssteiner    06/03/98    Made numerious changes and removed iostream
                            dependencies, added a few new registry entries and
                            added serialization.
	aoltean		06/06/99	Taken from atl30\atlbase.h in order to avoid linking ATL with BSCommon.lib
    ssteiner    05/15/00    Fixed bug #116688.  Added file locking to prevent multiple processes from
                            interferring with writing to the trace file.  Added code to place a UNICODE
                            BOM at the beginning of the trace file.
--*/

//
//  ***** Includes *****
//

#pragma warning(disable:4290)
#pragma warning(disable:4127)

#include <wtypes.h>
#pragma warning( disable: 4201 )    // C4201: nonstandard extension used : nameless struct/union
#include <winioctl.h>
#pragma warning( default: 4201 )	// C4201: nonstandard extension used : nameless struct/union
#include <winbase.h>
#include <wchar.h>
#include <string.h>
#include <iostream.h>
#include <fstream.h>
#include <stdio.h>
#include <process.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <vssmsg.h>

// Enabling asserts in ATL and VSS
#include "vs_assert.hxx"


#include <oleauto.h>
#include <stddef.h>
#pragma warning( disable: 4127 )    // warning C4127: conditional expression is constant
#include <atlconv.h>
#include <atlbase.h>
#include <ntverp.h>
#include <comadmin.h>

// SEH disabling (from nt\internal\com)
#include <globalopt.h>


#include "vs_inc.hxx"
#include "vs_idl.hxx"

////////////////////////////////////////////////////////////////////////
//  Standard foo for file name aliasing.  This code block must be after
//  all includes of VSS header files.
//
#ifdef VSS_FILE_ALIAS
#undef VSS_FILE_ALIAS
#endif
#define VSS_FILE_ALIAS "TRCTRCC"
//
////////////////////////////////////////////////////////////////////////

//
//  The following global, g_cDbgTrace must be declared BEFORE any of our
//  objects including _Module, since some of our static objects have destructors
//  that call trace methods.  The following pragma ensures that this
//  module's static objects are initialized before any of our other
//  static objects, assuming they don't use this same pragma.
//
#pragma warning(disable:4073) // ignore init_seg warning
#pragma init_seg(lib)


CBsDbgTrace g_cDbgTrace;


static VOID MakeFileUnicode(
    IN HANDLE hFile
    );

/////////////////////////////////////////////////////////////////////////////
// constants
//

const WCHAR	VSS_TRACINGKEYPATH[]	=
			L"SYSTEM\\CurrentControlSet\\Services\\VSS\\Debug\\Tracing";

const WCHAR	SETUP_KEY[]	=
			L"SYSTEM\\Setup";

const WCHAR	SETUP_INPROGRESS_REG[]	=
			L"SystemSetupInProgress";

const DWORD SETUP_INPROGRESS_VALUE = 1;

const x_nMaxCoCreateInstanceRetries = 10;


/////////////////////////////////////////////////////////////////////////////
//  Globals
//

//
//  NOTE: g_cDbgTrace, the global instance of this class is declared in
//  ltss\modules\ltssvc\src\ltssvc.cxx since we have to make sure
//  this object is the last one being destructed, otherwise possible
//  calls to this object will fail.
//

//
// Define a TLS var, stores the CLTermStg & intention list index.
// The index is a counter that is incremented and set for each thread
// coming into the service, in the CLTermStg::FinalConstruct method.
// The counter is also incremented and set for each intention list
// thread that is created by the service.
//
//  WARNING
//
//
//_declspec( thread ) DWORD CBsDbgTrace::m_dwContextNum = 0;

//
//  Queries a registry value name and if found sets dwValue to the value.
//  If the value name is not found, dwValue remains unchanged.
//
static DWORD
QuerySetValue (
    IN CRegKey &cRegKey,
    IN OUT DWORD &dwValue,
    IN LPCWSTR pwszValueName
    )
{
    DWORD dwReadValue = 0;
    DWORD dwResult = cRegKey.QueryValue( dwReadValue, pwszValueName );
    
    if ( dwResult == ERROR_SUCCESS )
        dwValue = dwReadValue;

    return dwResult;
}

//
//  Queries a registry value name and if found sets bValue to the value.
//  If the value name is not found, bValue remains unchanged.
//
static DWORD
QuerySetValue (
    IN CRegKey &cRegKey,
    IN OUT BOOL &bValue,
    IN LPCWSTR pwszValueName
    )
{
    DWORD dwReadValue = 0;
    DWORD dwResult = cRegKey.QueryValue( dwReadValue, pwszValueName );

    if ( dwResult == ERROR_SUCCESS )
        bValue = (BOOL)(dwReadValue != 0);

    return dwResult;
}

//
//  Queries a registry value name and if found sets wsValue to the value.
//  If the value name is not found, wsValue remains unchanged.
//
static DWORD
QuerySetValue (
    IN CRegKey &cRegKey,
    OUT LPWSTR &wsValue, // If allocated, must be freed before calling with ::VssFreeString
    IN LPCWSTR pwszValueName
    )
{
	WCHAR pszValueBuffer[_MAX_PATH];
    DWORD dwCount = _MAX_PATH;
    DWORD dwResult = cRegKey.QueryValue( pszValueBuffer, pwszValueName, &dwCount );

    BS_ASSERT(wsValue == NULL);
    if ( dwResult == ERROR_SUCCESS ) 
        ::VssDuplicateStr(wsValue, pszValueBuffer);

    return dwResult;
}

//
//  ***** class definitions *****
//


CBsDbgTrace::CBsDbgTrace()

/*++

Routine Description:

    Constructor method. Default values are given to operational
    parameters and overwritten using values from the registry if
    set.  Also prints out the trace file banner.

Arguments:

    NONE

Return Value:

    NONE

--*/
{
	m_bInitialized = false;
	m_bTracingEnabled = false;
	m_pcs = NULL;
    Initialize( TRUE );
}


CBsDbgTrace::~CBsDbgTrace()
/*++

Routine Description:

    Destructor method.  Prints out the last record in the NTMS


Arguments:

	LONG Indent - NOT USED YET [todo] this is the indentation indicator
	LONG Level - this is the debug trace level

Return Value:

    BOOL

--*/
{
    if ( !m_bInitialized )
        return;

    if ( m_bTracingEnabled ) {
        //
        //  Write out a finished tracing message
        //
        m_pcs->Enter();
        BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"****************************************************************" ) );
        BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  TRACING FINISHED - ProcessId: 0x%x, ContextId: 0x%x",
            m_dwCurrentProcessId, m_dwContextId ) );
        WCHAR pwszCurrentTime[128] = L"";
        time_t ltime;
        time( &ltime );
        struct tm * pToday = localtime( &ltime );
		if (pToday) {
			if (0 == wcsftime( pwszCurrentTime, SIZEOF_ARRAY(pwszCurrentTime), L"%c", pToday ))
			    pwszCurrentTime[0] = L'\0';
		}
        BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  Current time: %s", pwszCurrentTime ) );
        BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  Elapsed time: %d seconds", ltime- m_lTimeStarted ) );
   	    BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  Command-line: %s", GetCommandLineW() ) );
        BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"****************************************************************" ) );
        m_pcs->Leave();

        //
        //  Make sure the file is flushed before leaving
        //
        if ( m_bTraceToFile ) {
            m_hTraceFile = ::CreateFile( m_pwszTraceFileName?
                                            m_pwszTraceFileName :
                                            BS_DBG_TRACE_FILE_NAME_DFLT,
                                         GENERIC_WRITE,
                                         FILE_SHARE_READ,
                                         NULL,
                                         OPEN_ALWAYS,
                                         FILE_ATTRIBUTE_NORMAL,
                                         NULL );
            if ( m_hTraceFile != INVALID_HANDLE_VALUE ) {
                ::FlushFileBuffers( m_hTraceFile );
                ::CloseHandle( m_hTraceFile );
                m_hTraceFile = INVALID_HANDLE_VALUE;
            }
        }
    }

    ::VssFreeString(m_pwszTraceFileName);

    //
    //  Delete the critical section
    //
    delete m_pcs;
    m_pcs = NULL;
    m_bInitialized = FALSE;
}

//
//  In certain cases the global trace object doesn't seem to get it's constructor called.
//  To fix this problem, this function was added to perform the initialization of the
//  object.  This function is called both in the constructor and the set context call
//  which all DLLs that use the trace class call.
//
VOID
CBsDbgTrace::Initialize(
    IN  BOOL bInConstructor
    )
{
    if ( !m_bInitialized )
    {
		try
			{
			//
			//  Get the critical section created first
			//
			m_pcs = new CBsCritSec;
			if ( m_pcs == NULL )
				throw E_OUTOFMEMORY;
			m_bInitialized = TRUE;

			m_bTracingEnabled       = FALSE;
			m_bTraceToFile          = BS_DBG_TRACE_TO_FILE_DFLT;
			m_bTraceToDebugger      = BS_DBG_TRACE_TO_DEBUGGER_DFLT;
			m_bTraceEnterExit       = BS_DBG_TRACE_ENTER_EXIT_DFLT;
			m_dwTraceLevel          = BS_DBG_TRACE_LEVEL_DFLT;
			m_bTraceFileLineInfo    = BS_DBG_TRACE_FILE_LINE_INFO_DFLT;
			m_bTraceTimestamp       = BS_DBG_TRACE_TIMESTAMP_DFLT;
			m_pwszTraceFileName     = NULL;
			m_bForceFlush           = BS_DBG_TRACE_FORCE_FLUSH_DFLT;
			m_dwTraceIndent         = 0;
			m_bInTrace              = FALSE;
			m_hTraceFile            = INVALID_HANDLE_VALUE;
			m_dwLineNum             = 0;
			m_dwCurrentProcessId    = GetCurrentProcessId();
			m_bIsDuringSetup        = FALSE;
			
			LARGE_INTEGER liTimer;
			if ( ::QueryPerformanceCounter( &liTimer ) )
				{
				//  Got high performance counter, use the low part
				m_dwContextId = liTimer.LowPart;
				}
			else
				{
				m_dwContextId = ::GetTickCount();
				}

			ReadRegistry();

			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"****************************************************************" ) );
			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  TRACING STARTED - ProcessId: 0x%x, ContextId: 0x%x",
				m_dwCurrentProcessId, m_dwContextId ) );
			if ( !bInConstructor )
				BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  N.B. NOT INITIALIZED BY THE CONSTRUCTOR" ) );

			WCHAR pwszCurrentTime[128] = L"";
			time( &m_lTimeStarted );
            struct tm * pToday = localtime( &m_lTimeStarted );
			if (pToday) {
    			if (0 == wcsftime( pwszCurrentTime, SIZEOF_ARRAY(pwszCurrentTime), L"%c", pToday ))
    			    pwszCurrentTime[0] = L'\0';
			}
			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  Current time: %s", pwszCurrentTime ) );
			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  Command-line: %s", GetCommandLineW() ) );
			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  Product version: %d.%d.%d.%d", VER_PRODUCTVERSION ) );
			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"**  VSS SKU ID: 0x%02x", (UINT)CVssSKU::GetSKU() ) );			
			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"****************************************************************" ) );

			if ( CVssSKU::GetSKU() == CVssSKU::VSS_SKU_INVALID )
    			BsDebugTraceAlways( 0, DEBUG_TRACE_ALL, ( L"ERROR: Unable to determine the Windows SKU" ) );			    
			}
		catch(...)
			{
			delete m_pcs;
			m_pcs = NULL;
			m_bInitialized = false;
			m_bTracingEnabled = false;
			}
		}

}


BOOL
CBsDbgTrace::IsDuringSetup()
{
    return m_bIsDuringSetup;
}



HRESULT
CBsDbgTrace::ReadRegistry()
/*++

Routine Description:

    Tries to read debug specific values from the registry and adds
    the values if they don't exist.

Arguments:

    NONE

Return Value:

    HRESULT

--*/
{

	DWORD dwRes;
	CRegKey cRegKeySetup;
	CRegKey cRegKeyTracing;


    m_bTracingEnabled = FALSE;
    m_bIsDuringSetup = FALSE;
    
	//
	// Open the Setup key
	//

	dwRes = cRegKeySetup.Open( HKEY_LOCAL_MACHINE, SETUP_KEY, KEY_READ );
	if ( dwRes == ERROR_SUCCESS ) {
        DWORD dwSetupInProgress = 0;
        QuerySetValue( cRegKeySetup, dwSetupInProgress, SETUP_INPROGRESS_REG );
        m_bIsDuringSetup = ( dwSetupInProgress == SETUP_INPROGRESS_VALUE );
	}
	
	//
	// Open the VSS tracing key
	//

	dwRes = cRegKeyTracing.Open( HKEY_LOCAL_MACHINE, VSS_TRACINGKEYPATH, KEY_READ );
	if ( dwRes == ERROR_SUCCESS ) {
		
        // The name of the optional trace file
        QuerySetValue( cRegKeyTracing, m_pwszTraceFileName, BS_DBG_TRACE_FILE_NAME_REG );

        // The trace level determines what type of traciung will occur. Zero
        // indicates that no tracing will occur, and is the default.
        QuerySetValue( cRegKeyTracing, m_dwTraceLevel, BS_DBG_TRACE_LEVEL_REG );

        // The TraceEnterExit flag determines whether or not function entry & exit
        // information is output to the trace file & the debug output stream.
        QuerySetValue( cRegKeyTracing, m_bTraceEnterExit, BS_DBG_TRACE_ENTER_EXIT_REG );

        // The TraceToFile flag determines whether or not trace information is output to
        // the trace file. If this value is FALSE, no output is sent to the trace file.
        QuerySetValue( cRegKeyTracing, m_bTraceToFile, BS_DBG_TRACE_TO_FILE_REG );

        // The TraceToDebugger flag determines whether or not trace information is output
        // to the debugger. If this value is FALSE, no output is sent to the debugger.
        QuerySetValue( cRegKeyTracing, m_bTraceToDebugger, BS_DBG_TRACE_TO_DEBUGGER_REG );

        // The Timestamp flag determines whether or not timestamp
        // information is output to the trace file & the debug output stream.
        QuerySetValue( cRegKeyTracing, m_bTraceTimestamp, BS_DBG_TRACE_TIMESTAMP_REG );

        // The FileLineInfo flag determines whether or not the module file name
        // and line number information is output to the trace file & the debug
        // output stream.
        QuerySetValue( cRegKeyTracing, m_bTraceFileLineInfo, BS_DBG_TRACE_FILE_LINE_INFO_REG );

        // The TraceForceFlush flag specifies whether or not after each trace message is
        // written to the trace file a forced flush occurs.  If enabled, no trace records
        // are ever lost, however, performance is greatly reduced.
        QuerySetValue( cRegKeyTracing, m_bForceFlush, BS_DBG_TRACE_FORCE_FLUSH_REG );

        // Determine if tracing should be enabled
        if ( m_bTraceToDebugger || m_bTraceToFile )
            m_bTracingEnabled = TRUE;

	}

    return S_OK;
}


HRESULT
CBsDbgTrace::PrePrint(
    IN LPCWSTR pwszSourceFileName,
    IN DWORD dwLineNum,
    IN DWORD dwIndent,
    IN DWORD dwLevel,
    IN LPCWSTR pwszFunctionName,
    IN BOOL bTraceEnter
    )
/*++

Routine Description:

    Acquires the critical section so that other threads are
    now serialized.  Opens the trace file if necessary.
    N.B. Any A/V's in this code can cause a hang since the SEH translator function
    calls these trace functions.

Arguments:

    pszSourceFileName - Source file name of the module whose
        code called this method.
    dwLineNum - Line number in the source
    dwIndent - Number to increase or decrease the indendation level
    dwLevel - Trace level that specifies for which component
        the code resides in.
    pwszFunctionName - For entry/exit tracing.  Specifies the
        function name constains a call the a trace macro.
    bTraceEnter - True if this is a entry trace.

Return Value:

    HRESULT

--*/
{
    m_pcs->Enter();

    //
    //  Assume the trace macros have already filtered out traces based
    //  on m_bTracingEnabled and on the active trace level.
    //

    if ( m_bTracingEnabled && (dwLevel & m_dwTraceLevel) != 0) {
        if ( pwszSourceFileName == NULL )
            m_pwszSourceFileName = L"(Unknown source file)";
        else
        {
            //
            //  Keep only two levels deep of directory components
            //
            LPCWSTR pwszTemp = pwszSourceFileName + ::wcslen( pwszSourceFileName ) - 1;
            for ( int i = 0; pwszTemp > pwszSourceFileName && i < 3; ++i )
            {
                do
                {
                    --pwszTemp;
                }
                while( *pwszTemp != L'\\' && pwszTemp > pwszSourceFileName ) ;
            }
            if ( pwszTemp > pwszSourceFileName )
                m_pwszSourceFileName = pwszTemp + 1;
            else
                m_pwszSourceFileName = pwszSourceFileName;
        }

        m_pwszFunctionName   = pwszFunctionName;
        m_dwLineNum        = dwLineNum;
        m_bTraceEnter      = bTraceEnter;

        BS_ASSERT( m_hTraceFile == INVALID_HANDLE_VALUE );

        if ( m_bTraceToFile ) {
            m_hTraceFile = ::CreateFile( m_pwszTraceFileName?
                                            m_pwszTraceFileName :
                                            BS_DBG_TRACE_FILE_NAME_DFLT,
                                         GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL,
                                         OPEN_ALWAYS,
                                         FILE_ATTRIBUTE_NORMAL,
                                         NULL );
            if ( m_hTraceFile == INVALID_HANDLE_VALUE ) {
                //
                //  Error opening the file, print a message to the debugger if debugger
                //  tracing is enabled
                //
                Print( L"CBsDbgTrace::PrePrint: TRACING ERROR: Unable to open trace file, dwRet: %u", ::GetLastError() );
            } else {
                //
                //  Now lock the process from other processes and threads that are concurrently
                //  accessing the file.  Just lock the first byte of the file.
                //
                OVERLAPPED ovStart = { NULL, NULL, { 0, 0 }, 0 };
                if ( !::LockFileEx( m_hTraceFile,
                                    LOCKFILE_EXCLUSIVE_LOCK,
                                    0,
                                    1,
                                    0,
                                    &ovStart ) ) {
                    //
                    //  Tracing to file will be skipped for this record.  This should
                    //  never happen in practice.
                    //
                    ::CloseHandle( m_hTraceFile );
                    m_hTraceFile = INVALID_HANDLE_VALUE;

                    //
                    //  Try printing a trace message that will get to the debugger if debugger
                    //  tracing is enabled
                    //
                    Print( L"CBsDbgTrace::PrePrint: TRACING ERROR: Unable to lock trace file, skipping trace record, dwRet: %u", ::GetLastError() );
                } else {
                    //
                    //  If the file is new (empty) put the UNICODE BOM at the beginning of the file
                    //
                    LARGE_INTEGER liPointer;
                    if ( ::GetFileSizeEx( m_hTraceFile, &liPointer ) ) {
                        if ( liPointer.QuadPart == 0 )
                            ::MakeFileUnicode( m_hTraceFile );
                    }

                    //
                    //  Now move the file pointer to the end of the file
                    //
                    liPointer.QuadPart = 0;
                    if ( !::SetFilePointerEx( m_hTraceFile,
                                              liPointer,
                                              NULL,
                                              FILE_END ) ) {
                        //
                        //  Don't write to the file since it might overwrite valid records.
                        //  Tracing to file will be skipped for this record.  This should
                        //  never happen in practice.
                        //
                        ::CloseHandle( m_hTraceFile );
                        m_hTraceFile = INVALID_HANDLE_VALUE;

                        //
                        //  Try printing a trace message that will get to the debugger if debugger
                        //  tracing is enabled
                        //
                        Print( L"CBsDbgTrace::PrePrint: TRACING ERROR: Unable to set end of file, skipping trace record, dwRet: %u", ::GetLastError() );
                    }
                }
            }
        }

        m_bInTrace = TRUE;
    }

    return S_OK;
    UNREFERENCED_PARAMETER( dwIndent );
}

HRESULT
CBsDbgTrace::PostPrint(
    IN DWORD dwIndent
    )
/*++

Routine Description:

    Releases the critical section so that other threads
    can now call perform tracing.  Closes the trace file
    and resets variables.

Arguments:

    dwIndent - Number to increase or decrease the indendation level

Return Value:

    HRESULT

--*/
{
    if ( m_hTraceFile != INVALID_HANDLE_VALUE ) {
        OVERLAPPED ovStart = { NULL, NULL, { 0, 0 }, 0 };
        if ( !::UnlockFileEx( m_hTraceFile,
                              0,
                              1,
                              0,
                              &ovStart ) ) {
            Print( L"CBsDbgTrace::PrePrint: TRACING ERROR: Unable to unlock trace file, dwRet: %u", ::GetLastError() );
        }
        if ( m_bForceFlush )
            ::FlushFileBuffers( m_hTraceFile );
        ::CloseHandle( m_hTraceFile );
        m_hTraceFile = INVALID_HANDLE_VALUE;
    }

    m_pwszSourceFileName = NULL;
    m_pwszFunctionName = NULL;
    m_dwLineNum = 0;
    m_bInTrace  = FALSE;

    m_pcs->Leave();

    return S_OK;
    UNREFERENCED_PARAMETER( dwIndent );
}

HRESULT _cdecl
CBsDbgTrace::Print(
    IN LPCWSTR pwszFormatStr,
    IN ...
    )
/*++

Routine Description:

    Formats the trace message out to the trace file and/or debugger.

Arguments:

    pwszFormatStr - printf style format string
    ... - Arguments for the message

Return Value:

    HRESULT

--*/
{
    va_list pArg;

    if ( m_bInTrace ) {
        if ( m_bTraceTimestamp )
            StringCchPrintfW( STRING_CCH_PARAM(m_pwszOutBuf), 
                          L"[%010u,",
                          GetTickCount() );
        else
            StringCchPrintfW( STRING_CCH_PARAM(m_pwszOutBuf), 
                          L"[-," );

        StringCchPrintfW( m_pwszOutBuf + wcslen( m_pwszOutBuf ), 
                  SIZEOF_ARRAY(m_pwszOutBuf) - wcslen( m_pwszOutBuf ),
                  L"0x%06x:0x%04x:0x%08x] ",
                  m_dwCurrentProcessId,
                  GetCurrentThreadId(),
                  m_dwContextId );

        if ( m_bTraceFileLineInfo )
            StringCchPrintfW( m_pwszOutBuf + wcslen( m_pwszOutBuf ), 
                        SIZEOF_ARRAY(m_pwszOutBuf) - wcslen( m_pwszOutBuf ),
                        L"%s(%04u): ",
                        m_pwszSourceFileName,
                        m_dwLineNum );

        OutputString();

	    //
        // read the variable length parameter list into a formatted string
        //

        va_start( pArg, pwszFormatStr );
	    StringCchVPrintfW( STRING_CCH_PARAM(m_pwszOutBuf), pwszFormatStr, pArg );
	    va_end( pArg );

        OutputString();

        //
        //  Finish up with a carriage return.
        //
        wcscpy( m_pwszOutBuf, L"\r\n" );
        OutputString();
    }

    return S_OK;
}

HRESULT _cdecl
CBsDbgTrace::PrintEnterExit(
    IN LPCWSTR pwszFormatStr,
    IN ...
    )
/*++

Routine Description:

    Formats the entry/exit trace message out to the trace file and/or debugger.

Arguments:

    pwszFormatStr - printf style format string
    ... - Arguments for the message

Return Value:

    HRESULT

--*/
{
    va_list pArg;

    if ( m_bInTrace ) {
        if ( m_bTraceTimestamp )
            StringCchPrintfW( STRING_CCH_PARAM(m_pwszOutBuf), 
                      L"[%010u,",
                      GetTickCount() );
        else
            StringCchPrintfW( STRING_CCH_PARAM(m_pwszOutBuf), 
                      L"[-," );

        StringCchPrintfW( m_pwszOutBuf + wcslen( m_pwszOutBuf ), 
                  SIZEOF_ARRAY(m_pwszOutBuf) - wcslen( m_pwszOutBuf ), 
                  L"0x%06x:0x%04x:0x%08x] %s {%s}: ",
                  m_dwCurrentProcessId,
                  GetCurrentThreadId(),
                  m_dwContextId,
                  m_bTraceEnter ? L"ENTER" : L"EXIT ",
                  m_pwszFunctionName );

        OutputString();

	    //
        // read the variable length parameter list into a formatted string
        //

        va_start( pArg, pwszFormatStr );
	    StringCchVPrintfW( STRING_CCH_PARAM(m_pwszOutBuf), pwszFormatStr, pArg );
	    va_end( pArg );

        OutputString();

        //
        //  Finish up with a carriage return.
        //
        wcscpy( m_pwszOutBuf, L"\r\n" );
        OutputString();
    }

    return S_OK;
}

HRESULT
CBsDbgTrace::OutputString()
/*++

Routine Description:

    Prints the trace message out to the trace file and/or debugger.

Arguments:

    Assumes m_pwszOutBuf has the string to be printed.

Return Value:

    HRESULT

--*/
{
    //
    //  Make sure we didn't go off the end.  Can't use BS_ASSERT(), it
    //  will cause an deadlock.
    //
    _ASSERTE( wcslen( m_pwszOutBuf ) < BS_DBG_OUT_BUF_SIZE );

    //
    // Print to the debug stream for debug builds
    //
    if ( m_bTraceToDebugger )
        OutputDebugString( m_pwszOutBuf );

    //
    // If file tracing is enabled, dump to file
    //
    if ( m_hTraceFile != INVALID_HANDLE_VALUE ) {
        DWORD dwBytesWritten;
        ::WriteFile( m_hTraceFile,
                     m_pwszOutBuf,
                     (DWORD)(wcslen( m_pwszOutBuf ) * sizeof(WCHAR)),
                     &dwBytesWritten,
                     NULL );
    }

    return S_OK;
}


VOID CBsDbgTrace::SetContextNum(
    IN DWORD dwContextNum
    )
/*++

Routine Description:

    Use to be used to set the context number of the operation.  Now it is only
    used to determine if a DLL is loading using the trace class.

Arguments:

    LTS_CONTEXT_DELAYED_DLL - DLL is using the class object.

--*/
{
    if (dwContextNum == LTS_CONTEXT_DELAYED_DLL && !m_bInitialized )
    {
        Initialize();
    }

}


/*++

Routine Description:

    Puts the UNICODE UCS-2 BOM (Byte Order Mark) at the beginning of the file
    to let applications know that 1. this is a UCS-2 UNICODE file and 2. that
    the byte ordering is little-endian.

    Assumes the file is empty.

Arguments:

    hFile - Handle to the file

Return Value:

    <Enter return values here>

--*/
static VOID MakeFileUnicode(
    IN HANDLE hFile
    )
{
    BS_ASSERT( hFile != INVALID_HANDLE_VALUE );
    BYTE byteBOM[2] = { 0xFF, 0xFE };

    DWORD dwBytesWritten;
    ::WriteFile( hFile,
                 byteBOM,
                 sizeof byteBOM,
                 &dwBytesWritten,
                 NULL );
}


void __cdecl CVssFunctionTracer::TranslateError
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN HRESULT hr,
		IN LPCWSTR wszRoutine
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

--*/

    {
	if (hr == E_OUTOFMEMORY ||
		hr == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_SEARCH_HANDLES) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_LOG_SPACE) ||
		hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_USER_HANDLES))
		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected in function %s", wszRoutine);
	else
		{
		LogError(VSS_ERROR_UNEXPECTED_CALLING_ROUTINE, dbgInfo << wszRoutine << hr);
		Throw(dbgInfo, E_UNEXPECTED, L"Unexpected error in routine %s.  hr = 0x%08lx", wszRoutine, hr);
		}
	}


void __cdecl CVssFunctionTracer::TranslateComError
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

Throws:

    E_UNEXPECTED
        - on unrecognized error codes

--*/

    {
        CVssOutputBuffer buffer;
        if (!buffer.IsBufferValid())
            throw(E_OUTOFMEMORY);
    
        WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)
    
        // Get the HR from the internal member
        HRESULT hr = m_hr;
        if (hr == S_OK)
            hr = E_UNEXPECTED;

        // Get the COM error (in a fault-tolerant manner)
        CComPtr<IErrorInfo> pErrorInfo;
        CComBSTR bstrDescription;
        HRESULT hr2 = ::GetErrorInfo(0, &pErrorInfo);
        if (SUCCEEDED(hr2)) {
            if (pErrorInfo != NULL) {
                // Intentionally ignore return code
                pErrorInfo->GetDescription(&bstrDescription);
            }
        }

        // Setup a non-NULL error description always
        LPWSTR pwszErrorDescription = (LPWSTR) bstrDescription;
        if (NULL == pwszErrorDescription)
            pwszErrorDescription = L"";

        // Translate the obtained hr.
    	if (hr == E_OUTOFMEMORY ||
    		hr == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_SEARCH_HANDLES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_LOG_SPACE) ||
    		hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_USER_HANDLES))
    		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s - %s", 
    		    buffer.GetBuffer(), pwszErrorDescription);
    	else
		{
    		LogError(VSS_ERROR_UNEXPECTED_COM_ERRORCODE, 
    		    dbgInfo << buffer.GetBuffer() << pwszErrorDescription << hr);
    		Throw(dbgInfo, E_UNEXPECTED, L"Unexpected error: %s - %s [hr = 0x%08lx]", 
   		        buffer.GetBuffer(), pwszErrorDescription, hr);
		}
	}



void __cdecl CVssFunctionTracer::TranslateWin32Error
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

Throws:

    E_UNEXPECTED
        - on unrecognized error codes

--*/

    {
        CVssOutputBuffer buffer;
        if (!buffer.IsBufferValid())
            throw(E_OUTOFMEMORY);
    
        WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)
    
        // Get the HR from GetLastError - note that this API may return S_OK on Win32 failed calls
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        if (hr == S_OK)
            hr = E_UNEXPECTED;

        // Translate the obtained hr.
    	if (hr == E_OUTOFMEMORY ||
    		hr == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_SEARCH_HANDLES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_LOG_SPACE) ||
    		hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_USER_HANDLES))
    		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s", buffer.GetBuffer());
    	else
    		{
    		LogError(VSS_ERROR_UNEXPECTED_ERRORCODE, dbgInfo << buffer.GetBuffer() << hr);
    		Throw(dbgInfo, E_UNEXPECTED, L"Unexpected error: %s  [hr = 0x%08lx]", buffer.GetBuffer(), hr);
    		}
	}



void __cdecl CVssFunctionTracer::TranslateGenericError
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN HRESULT hr,
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

Throws:

    E_UNEXPECTED
        - on unrecognized error codes

--*/

    {
        CVssOutputBuffer buffer;
        if (!buffer.IsBufferValid())
            throw(E_OUTOFMEMORY);
    
        WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)

    	if (hr == E_OUTOFMEMORY ||
    		hr == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_SEARCH_HANDLES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_LOG_SPACE) ||
    		hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_USER_HANDLES))
    		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s", buffer.GetBuffer());
    	else
    		{
    		LogError(VSS_ERROR_UNEXPECTED_ERRORCODE, dbgInfo << buffer.GetBuffer() << hr);
    		Throw(dbgInfo, E_UNEXPECTED, L"Unexpected error: %s  [hr = 0x%08lx]", buffer.GetBuffer(), hr);
    		}
	}



void __cdecl CVssFunctionTracer::TranslateProviderError
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN GUID ProviderID,
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

	The error is coming from a provider call

Throws:

    E_OUTOFMEMORY
    VSS_E_UNEXPECTED_PROVIDER_ERROR
        - Unexpected provider error. The error code is logged into the event log.
    VSS_E_PROVIDER_VETO
        - Expected provider error. The provider already did the logging.

--*/

    {
    CVssOutputBuffer buffer;
    if (!buffer.IsBufferValid())
        throw(E_OUTOFMEMORY);

    WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)

	if (hr == E_OUTOFMEMORY)
		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s. Provider ID = " WSTR_GUID_FMT, 
		    buffer.GetBuffer(), GUID_PRINTF_ARG(ProviderID));
	else if (hr == E_INVALIDARG) {
		LogError(VSS_ERROR_CALLING_PROVIDER_ROUTINE_INVALIDARG, dbgInfo << ProviderID << buffer.GetBuffer() );
		Throw(dbgInfo, E_INVALIDARG, L"Invalid argument detected. %s. Provider ID = " WSTR_GUID_FMT, 
		    buffer.GetBuffer(), GUID_PRINTF_ARG(ProviderID));
	}
	else if (hr == VSS_E_PROVIDER_VETO)
		Throw(dbgInfo, VSS_E_PROVIDER_VETO, L"Provider veto detected. %s. Provider ID = " WSTR_GUID_FMT, 
		    buffer.GetBuffer(), GUID_PRINTF_ARG(ProviderID));
    else
		{
		LogError(VSS_ERROR_CALLING_PROVIDER_ROUTINE, dbgInfo << ProviderID << buffer.GetBuffer() << hr );
		Throw(dbgInfo, VSS_E_UNEXPECTED_PROVIDER_ERROR, 
		    L"Unexpected error calling a provider routine: %s  [hr = 0x%08lx] Provider ID = " WSTR_GUID_FMT, 
		    buffer.GetBuffer(), hr, GUID_PRINTF_ARG(ProviderID));
		}
	}



void __cdecl CVssFunctionTracer::TranslateInternalLovelaceError
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN HRESULT hr,
        IN BOOL bHold,
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

	The error is coming from a volsnap generic Ioctls (which Lovelace calls)

Throws:

    E_UNEXPECTED
        - on unrecognized error codes

--*/

    {
        CVssOutputBuffer buffer;
        if (!buffer.IsBufferValid())
            throw(E_OUTOFMEMORY);
    
        WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)

    	if (hr == E_OUTOFMEMORY ||
    		hr == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_SEARCH_HANDLES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_LOG_SPACE) ||
    		hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES) ||
    		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_USER_HANDLES))
    		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s", buffer.GetBuffer());
        else if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) ||
            hr ==  HRESULT_FROM_WIN32(ERROR_OPERATION_ABORTED)) 
            {
            // No need to log - this is a timeout case and Lovelave already logs an error for those
            HRESULT hrThrow = (bHold? VSS_E_FLUSH_WRITES_TIMEOUT : VSS_E_HOLD_WRITES_TIMEOUT);
    		Throw(dbgInfo, hrThrow, L"Timeout detected. %s  [hr = 0x%08lx]", buffer.GetBuffer(), hr);
            }
    	else
    		{
    		LogError(VSS_ERROR_UNEXPECTED_ERRORCODE, dbgInfo << buffer.GetBuffer() << hr);
    		Throw(dbgInfo, E_UNEXPECTED, L"Unexpected error: %s  [hr = 0x%08lx]", buffer.GetBuffer(), hr);
    		}
	}

void __cdecl CVssFunctionTracer::TranslateInternalProviderError
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN HRESULT hrToBeTreated,
		IN HRESULT hrToBeThrown,
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

	The error is coming from a provider call

Throws:

    E_OUTOFMEMORY
    
    hrToBeThrown

--*/

    {
    CVssOutputBuffer buffer;
    if (!buffer.IsBufferValid())
        throw(E_OUTOFMEMORY);

    WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)

    hr = hrToBeTreated;

	if (hr == E_OUTOFMEMORY ||
		hr == HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_SEARCH_HANDLES) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_LOG_SPACE) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_SYSTEM_RESOURCES) ||
		hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_USER_HANDLES))
		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s.", buffer.GetBuffer());
    else if (hr == HRESULT_FROM_WIN32(ERROR_DISK_FULL))
		Throw(dbgInfo, VSS_E_INSUFFICIENT_STORAGE, L"Out of disk space detected. %s.", buffer.GetBuffer());
	else if ( hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
	    hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED) || 
		hr == HRESULT_FROM_WIN32(ERROR_NOT_READY))
	    {
		LogError(VSS_ERROR_DEVICE_NOT_CONNECTED, dbgInfo << buffer.GetBuffer() );
		Throw(dbgInfo, VSS_E_OBJECT_NOT_FOUND, 
		    L"Invalid device when calling a provider routine: %s", buffer.GetBuffer());
    	}
    else if (hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT))
        {
		LogError(VSS_ERROR_COMMIT_TIMEOUT, dbgInfo << buffer.GetBuffer() );
		Throw(dbgInfo, hrToBeThrown, L"Timeout error calling a provider routine: %s.", buffer.GetBuffer());
        }
    else
		{
        if (hrToBeThrown != VSS_E_OBJECT_NOT_FOUND)
    		LogError(VSS_ERROR_UNEXPECTED_ERRORCODE, dbgInfo << buffer.GetBuffer() << hr );
		Throw(dbgInfo, hrToBeThrown, 
		    L"Unexpected error calling a provider routine: %s  [hr = 0x%08lx] ", buffer.GetBuffer(), hr );
		}
	}



void __cdecl CVssFunctionTracer::TranslateWriterReturnCode
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Translates an error into a well defined error code.  May log to
	the event log if the error is unexpected

	The error is coming from a writer call (CoCreteInstance of the writer COM+ event class
	or sending an event.

Throws:

    E_OUTOFMEMORY
    VSS_E_UNEXPECTED_WRITER_ERROR
        - Unexpected writer error. The error code is logged into the event log.

--*/

    {

    if (HrSucceeded()) {
        BS_ASSERT(hr == S_OK || hr == EVENT_S_NOSUBSCRIBERS || hr == EVENT_S_SOME_SUBSCRIBERS_FAILED);
        hr = S_OK;
        return;
    }
    
    CVssOutputBuffer buffer;
    if (!buffer.IsBufferValid())
        throw(E_OUTOFMEMORY);

    WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)
    
    if (hr == EVENT_E_ALL_SUBSCRIBERS_FAILED) {
		Trace( VSSDBG_COORD, L"%s event failed at one writer. hr = 0x%08lx", buffer.GetBuffer(), hr);
		hr = S_OK;
		// ignore the error;
        return;
        }
	else if (hr == E_OUTOFMEMORY)
		Throw(dbgInfo, E_OUTOFMEMORY, L"Out of memory detected. %s.", buffer.GetBuffer());
    else
		{
		LogError(VSS_ERROR_UNEXPECTED_WRITER_ERROR, dbgInfo << buffer.GetBuffer() << hr );
		Throw(dbgInfo, VSS_E_UNEXPECTED_WRITER_ERROR, 
		    L"Unexpected error calling a provider routine: %s  [hr = 0x%08lx] ", buffer.GetBuffer(), hr);
		}
    }



void __cdecl CVssFunctionTracer::LogGenericWarning
		(
		IN CVssDebugInfo dbgInfo,          // Caller debugging info
		IN LPCWSTR wszErrorTextFormat,
		IN ...
		)
/*++

Routine Description:

    Log a generic warning.

--*/

    {
    CVssOutputBuffer buffer;
    if (!buffer.IsBufferValid())
        throw(E_OUTOFMEMORY);

    WSNPRINTF_BUFFER(buffer, wszErrorTextFormat)

	LogError(VSS_WARNING_UNEXPECTED, dbgInfo << buffer.GetBuffer() << hr, EVENTLOG_WARNING_TYPE);
	Trace(dbgInfo, L"WARNING: %s [hr = 0x%08lx]", buffer.GetBuffer(), hr);
	}


// This method must be called prior to calling a CoCreateInstance that may start VSS
void CVssFunctionTracer::LogVssStartupAttempt()
{
    // the name of the Volume Snapshot Service
    const LPCWSTR wszVssvcServiceName = L"VSS";
    
    SC_HANDLE		shSCManager = NULL;
    SC_HANDLE		shSCService = NULL;

    try
	{
        //
        //  Check to see if VSSVC is running. If not, we are putting an entry into the trace log if enabled.
        //

    	// Connect to the local service control manager
        shSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_CONNECT);
        if (!shSCManager) 
            TranslateGenericError(VSSDBG_GEN, HRESULT_FROM_WIN32(GetLastError()), 
                L"OpenSCManager(NULL,NULL,SC_MANAGER_CONNECT)");

    	// Get a handle to the service
        shSCService = OpenService (shSCManager, wszVssvcServiceName, SERVICE_QUERY_STATUS);
        if (!shSCService) 
            TranslateGenericError(VSSDBG_GEN, HRESULT_FROM_WIN32(GetLastError()), 
                L" OpenService (shSCManager, \'%s\', SERVICE_QUERY_STATUS)", wszVssvcServiceName);

    	// Now query the service to see what state it is in at the moment.
        SERVICE_STATUS	sSStat;
        if (!QueryServiceStatus (shSCService, &sSStat))
            TranslateGenericError(VSSDBG_GEN, HRESULT_FROM_WIN32(GetLastError()), 
                L"QueryServiceStatus (shSCService, &sSStat)");

        // If the service is not running, then we will put an informational error log entry 
        // if (sSStat.dwCurrentState != SERVICE_RUNNING)
        //     LogError(VSS_INFO_SERVICE_STARTUP, 
        //         VSSDBG_GEN << GetCommandLineW() << (HRESULT)sSStat.dwCurrentState, 
        //         EVENTLOG_INFORMATION_TYPE);
        if (sSStat.dwCurrentState != SERVICE_RUNNING)
            Trace( VSSDBG_GEN, 
                L"Volume Snapshots Service information: Service starting at request of process '%s'. [0x%08x]",
                GetCommandLineW(), sSStat.dwCurrentState );        
	} VSS_STANDARD_CATCH ((*this));

    // Close handles
    if (NULL != shSCService) CloseServiceHandle (shSCService);
    if (NULL != shSCManager) CloseServiceHandle (shSCManager);
}


void CVssFunctionTracer::CoCreateInstanceWithLog(
		IN  CVssDebugInfo   dbgInfo,          // Caller debugging info
        IN  CLSID           ServerClassID,
        IN  LPCWSTR         ServerName,
        IN  DWORD           dwContext,
        IN  IID             InterfaceID,
        OUT IUnknown **     ppUnknown
        )
{
    for(int nRetries = 0; nRetries < x_nMaxCoCreateInstanceRetries; nRetries++)
    {
        hr = ::CoCreateInstance( 
                            ServerClassID, 
                            NULL, 
                            dwContext, 
                            InterfaceID, 
                            (void**)ppUnknown);
        if (hr != CO_E_SERVER_EXEC_FAILURE)
            break;
    }

    // Treatment of the error cases
    switch(hr)
    {
    case S_OK:
        break;
    case E_OUTOFMEMORY:
        break;
    case HRESULT_FROM_WIN32(ERROR_NOT_SAFEBOOT_SERVICE):
      	LogError( VSS_ERROR_STARTING_COM_SERVER_SAFEBOOT, 
      	    dbgInfo << ServerClassID << ServerName << hr );
        break;
    case EVENT_E_INTERNALERROR: // This is only for CLSID_CEventSystem
      	LogError( VSS_ERROR_STARTING_EVENTS_SAFEBOOT, 
      	    dbgInfo << ServerClassID << ServerName << hr );
        break;
    case CO_E_SERVER_EXEC_FAILURE:
      	LogError( VSS_ERROR_STARTING_COM_SERVER_HEAVYLOAD, 
      	    dbgInfo << ServerClassID << ServerName << hr );
        break;
    case E_NOINTERFACE:
    case REGDB_E_CLASSNOTREG:
        if ((ServerClassID == CLSID_CEventSystem) || 
            (ServerClassID == __uuidof(COMAdminCatalog)) ||
            (ServerClassID == CLSID_CEventClass))
          	LogError( VSS_ERROR_COMPLUS_CORRUPTED, 
          	    dbgInfo << ServerClassID << ServerName << hr );
        else
          	LogError( VSS_ERROR_COM_SERVER_NOT_REGISTERED, 
          	    dbgInfo << ServerClassID << ServerName << hr );
        break;
    default:
        BS_ASSERT(FAILED(hr));
      	LogError( VSS_ERROR_STARTING_COM_SERVER, 
      	    dbgInfo << ServerClassID << ServerName << hr );
        break;
    }        
}


//  Turns off SEH exception handing for COM servers (BUG# 530092)
void CVssFunctionTracer::ComDisableSEH(
        IN  CVssDebugInfo   dbgInfo          // Caller debugging info
) throw(HRESULT)
{
    // CoCreate the Global options object
    CComPtr<IGlobalOptions> ptrIGLB;
    hr = ptrIGLB.CoCreateInstance(CLSID_GlobalOptions, NULL, CLSCTX_INPROC_SERVER);
    if (HrFailed()) {
        LogError( VSS_ERROR_STARTING_COM_SERVER, 
            dbgInfo << CLSID_GlobalOptions << L"CLSID_GlobalOptions" << hr );
        Throw( dbgInfo, hr, L"CoCreateInstance(CLSID_GlobalOptions) failed [0x%08lx]");
    }

    // Set the global COM option
    hr = ptrIGLB->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE);
    if (HrFailed())
        TranslateGenericError( dbgInfo, hr, 
            L"pIGLB->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE)");
}

