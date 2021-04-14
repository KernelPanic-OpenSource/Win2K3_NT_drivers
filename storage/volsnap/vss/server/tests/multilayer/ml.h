/*
**++
**
** Copyright (c) 2000-2001  Microsoft Corporation
**
**
** Module Name:
**
**	    ml.h
**
**
** Abstract:
**
**	    Test program to exercise backup and multilayer snapshots
**
** Author:
**
**	    Adi Oltean      [aoltean]       02/22/2001
**
** Revision History:
**
**--
*/

#ifndef __ML_HEADER_H__
#define __ML_HEADER_H__

#if _MSC_VER > 1000
#pragma once
#endif


/*
** Defines
**
**
**	   C4290: C++ Exception Specification ignored
** warning C4511: 'CVssCOMApplication' : copy constructor could not be generated
** warning C4127: conditional expression is constant
*/
#pragma warning(disable:4290)
#pragma warning(disable:4511)
#pragma warning(disable:4127)


/*
** Includes
*/

// Disable warning: 'identifier' : identifier was truncated to 'number' characters in the debug information
//#pragma warning(disable:4786)

//
// C4290: C++ Exception Specification ignored
//
#pragma warning(disable:4290)

//
// C4511: copy constructor could not be generated
//
#pragma warning(disable:4511)

//
//  Warning: ATL debugging turned off (BUG 250939)
//
//  #ifdef _DEBUG
//  #define _ATL_DEBUG_INTERFACES
//  #define _ATL_DEBUG_QI
//  #define _ATL_DEBUG_REFCOUNT
//  #endif // _DEBUG

#include <windows.h>
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
#include <stddef.h>
#include <stdlib.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>

// Enabling asserts in ATL and VSS
#include "vs_assert.hxx"


#include <oleauto.h>
#include <stddef.h>
#pragma warning( disable: 4127 )    // warning C4127: conditional expression is constant
#include <atlconv.h>
#include <atlbase.h>


// VSS standard headers 
#include <vss.h>
#include <vscoordint.h>
#include <vsswprv.h>
#include <vsmgmt.h>

#include <vswriter.h>
#include <vsbackup.h>

#include <vs_inc.hxx>

#include "objects.h"
#include "cmdparse.h"
#include "macros.h"

extern CComModule _Module;
#include <atlcom.h>

///////////////////////////////////////////////////////////////////////////////
// Useful macros 



inline void VsmlCopy(
	IN  WCHAR* wszDestBuffer, 
	IN	WCHAR* wszSourceBuffer,
	IN  DWORD dwBufferLen
	)
{
	::ZeroMemory(wszDestBuffer, dwBufferLen * sizeof(WCHAR));
	::wcsncpy(wszDestBuffer, wszSourceBuffer, dwBufferLen - 1);
}



#define VSS_ERROR_CASE(wszBuffer, dwBufferLen, X) 	\
    case X: ::VsmlCopy(wszBuffer, VSS_MAKE_W(VSS_EVAL(#X)), dwBufferLen);  break;

#define WSTR_GUID_FMT  L"{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"

#define GUID_PRINTF_ARG( X )                                \
    (X).Data1,                                              \
    (X).Data2,                                              \
    (X).Data3,                                              \
    (X).Data4[0], (X).Data4[1], (X).Data4[2], (X).Data4[3], \
    (X).Data4[4], (X).Data4[5], (X).Data4[6], (X).Data4[7]


// Execute the given call and check that the return code must be S_OK
#define CHECK_SUCCESS( Call )                                                                           \
    {                                                                                                   \
        ft.hr = Call;                                                                                   \
        if (ft.hr != S_OK)                                                                              \
            ft.Err(VSSDBG_VSSTEST, ft.hr, L"\nError: \n\t- Call %S not succeeded. \n"                   \
                L"\t  Error code = 0x%08lx. Error description = %s\n",                                  \
                #Call, ft.hr, GetStringFromFailureType(ft.hr));                                         \
    }

#define CHECK_NOFAIL( Call )                                                                            \
    {                                                                                                   \
        ft.hr = Call;                                                                                   \
        if (ft.HrFailed())                                                                              \
            ft.Err(VSSDBG_VSSTEST, ft.hr, L"\nError: \n\t- Call %S not succeeded. \n"                   \
                L"\t  Error code = 0x%08lx. Error description = %s\n",                                  \
                #Call, ft.hr, GetStringFromFailureType(ft.hr));                                         \
    }


///////////////////////////////////////////////////////////////////////////////
// Constants

const MAX_TEXT_BUFFER   = 512;
const MAX_VOL_ITERATIONS = 10;
const VSS_SEED = 1234;

// The GUID that corresponds to the format used to store the
// Backup Snapshot Application Info in Client SKU
// {BCF5D39C-27A2-4b4c-B9AE-51B111DC9409}
const GUID VOLSNAP_APPINFO_GUID_BACKUP_CLIENT_SKU = 
{ 0xbcf5d39c, 0x27a2, 0x4b4c, { 0xb9, 0xae, 0x51, 0xb1, 0x11, 0xdc, 0x94, 0x9 } };


// The GUID that corresponds to the format used to store the
// Backup Snapshot Application Info in Server SKU
// {BAE53126-BC65-41d6-86CC-3D56A5CEE693}
const GUID VOLSNAP_APPINFO_GUID_BACKUP_SERVER_SKU = 
{ 0xbae53126, 0xbc65, 0x41d6, { 0x86, 0xcc, 0x3d, 0x56, 0xa5, 0xce, 0xe6, 0x93 } };


// The GUID that corresponds to the format used to store the
// Hidden (Inaccessible) Snapshot Application Info
// {F12142B4-9A4B-49af-A851-700C42FDC2BE}
const GUID VOLSNAP_APPINFO_GUID_SYSTEM_HIDDEN = 
{ 0xf12142b4, 0x9a4b, 0x49af, { 0xa8, 0x51, 0x70, 0xc, 0x42, 0xfd, 0xc2, 0xbe } };


// The GUID that corresponds to the format used to store the
// NAS Rollback Snapshot Application Info
// {D591D4F0-B920-459d-9FFF-09E032ECBB57}
const GUID VOLSNAP_APPINFO_GUID_NAS_ROLLBACK = 
{ 0xd591d4f0, 0xb920, 0x459d, { 0x9f, 0xff, 0x9, 0xe0, 0x32, 0xec, 0xbb, 0x57 } };


// The GUID that corresponds to the format used to store the
// APP Rollback Snapshot Application Info
// {AE9A9337-0048-4ed6-9874-71500654B7B3}
const GUID VOLSNAP_APPINFO_GUID_APP_ROLLBACK = 
{ 0xae9a9337, 0x48, 0x4ed6, { 0x98, 0x74, 0x71, 0x50, 0x6, 0x54, 0xb7, 0xb3 } };


// The GUID that corresponds to the format used to store the
// File Share Backup Snapshot Application Info
// {8F8F4EDD-E056-4690-BB9E-E35D4D41A4C0}
const GUID VOLSNAP_APPINFO_GUID_FILE_SHARE_BACKUP = 
{ 0x8f8f4edd, 0xe056, 0x4690, { 0xbb, 0x9e, 0xe3, 0x5d, 0x4d, 0x41, 0xa4, 0xc0 } };




// Internal flags
const LONG      x_nInternalFlagHidden = 0x00000001;




typedef enum _EVssTestType
{
    VSS_TEST_UNKNOWN = 0,
    VSS_TEST_NONE,
    VSS_TEST_QUERY_SNAPSHOTS,
    VSS_TEST_QUERY_SNAPSHOTS_ON_VOLUME,
    VSS_TEST_QUERY_VOLUMES,
    VSS_TEST_VOLSNAP_QUERY,
    VSS_TEST_DELETE_BY_SNAPSHOT_ID,
    VSS_TEST_DELETE_BY_SNAPSHOT_SET_ID,
    VSS_TEST_CREATE,
    VSS_TEST_ADD_DIFF_AREA,
    VSS_TEST_REMOVE_DIFF_AREA,
    VSS_TEST_CHANGE_DIFF_AREA_MAX_SIZE,
    VSS_TEST_QUERY_SUPPORTED_VOLUMES_FOR_DIFF_AREA,
    VSS_TEST_QUERY_DIFF_AREAS_FOR_VOLUME,
    VSS_TEST_QUERY_DIFF_AREAS_ON_VOLUME,
    VSS_TEST_QUERY_DIFF_AREAS_FOR_SNAPSHOT,
    VSS_TEST_IS_VOLUME_SNAPSHOTTED_C,
    VSS_TEST_SET_SNAPSHOT_PROPERTIES,
    VSS_TEST_ACCESS_CONTROL_SD,
    VSS_TEST_DIAG_WRITERS,
    VSS_TEST_DIAG_WRITERS_LOG,
	VSS_TEST_DIAG_WRITERS_CSV,
    VSS_TEST_DIAG_WRITERS_ON,
    VSS_TEST_DIAG_WRITERS_OFF,
	VSS_TEST_LIST_WRITERS,
} EVssTestType;


typedef enum _EVssQWSFlags
{
	VSS_QWS_THROW_ON_WRITER_FAILURE = 1,
	VSS_QWS_DISPLAY_WRITER_STATUS = 2,
};



///////////////////////////////////////////////////////////////////////////////
// Main class


class CVssMultilayerTest
{
    
// Constructors& destructors
private:
    CVssMultilayerTest();
    CVssMultilayerTest(const CVssMultilayerTest&);
    
public:
    CVssMultilayerTest(
        IN  INT nArgsCount,
        IN  WCHAR ** ppwszArgsArray
        );

    ~CVssMultilayerTest();

// Main routines
public:

    // Initialize internal members
    void Initialize();

    // Run the test
    void Run();

// Internal tests
public:

    // Queries the snapshots
    void QuerySnapshots();

    // Queries the snapshots on volume
    void QuerySnapshotsByVolume();

    // Creates a backup snapshot set
    void QuerySupportedVolumes();

    // Query using hte IOCTL
    void QueryVolsnap();

    // Delete by snapshot Id
    void DeleteBySnapshotId();

    // Delete by snapshot set Id
    void DeleteBySnapshotSetId();

    // Creates a backup snapshot set
    void PreloadExistingSnapshots();

    // Creates a timewarp snapshot set
    void CreateTimewarpSnapshotSet();

    // Creates a backup snapshot set
    void CreateBackupSnapshotSet();

    // Completes the backup
    void BackupComplete();

    void GatherWriterMetadata();

    void GatherWriterStatus(
        IN  LPCWSTR wszWhen,
		DWORD dwFlags = VSS_QWS_THROW_ON_WRITER_FAILURE
        );

    // Adds a diff area
    void AddDiffArea();

    // Removes a diff area
    void RemoveDiffArea();

    // Change diff area maximum size
    void ChangeDiffAreaMaximumSize();

    // Query volumes for diff area
    void QueryVolumesSupportedForDiffAreas();

    // Query volumes for diff area
    void QueryDiffAreasForVolume();

    // Query volumes on diff area
    void QueryDiffAreasOnVolume();

    // Query volumes on diff area
    void QueryDiffAreasForSnapshot();

    // Test if the volume is snapshotted using the "C" API
    void IsVolumeSnapshotted_C();

    // Test if the volume is snapshotted using the "C" API
    void SetSnapshotProperties();

	// Test the CVssSidCollection class
	void TestAccessControlSD();

	// Diagnose writers
	void DiagnoseWriters(
		IN EVssTestType eType
		);

	// List writers
	void TestListWriters();


// Command line processing
public:

    // Parse command line arguments 
    bool ParseCommandLine();

    // Print the usage
    bool PrintUsage(bool bThrow = true);

    // Returns true if there are tokens left
    bool TokensLeft();
        
    // Returns the current token
    VSS_PWSZ GetCurrentToken();

    // Go to next token
    void Shift();
        
    // Check if the current token matches the given argument
	bool Peek(
		IN	VSS_PWSZ pwszPattern
		) throw(HRESULT);
    
    // Match a pattern. If succeeds, shift to the next token.
	bool Match(
		IN	VSS_PWSZ pwszPattern
		) throw(HRESULT);
    
    // Extract a GUID. If succeeds, shift to the next token.
	bool Extract(
		IN OUT VSS_ID& Guid
		) throw(HRESULT);
    
    // Extract a string. If succeeds, shift to the next token.
	bool Extract(
		IN OUT VSS_PWSZ& pwsz
		) throw(HRESULT);
    
    // Extract an UINT. If succeeds, shift to the next token.
	bool Extract(
		IN OUT UINT& uint
		) throw(HRESULT);
    
    // Extract an UINT. If succeeds, shift to the next token.
	bool Extract(
		IN OUT LONGLONG& llValue
		) throw(HRESULT);
    
// Private methods:
private:

    LPCWSTR GetStringFromFailureType (HRESULT hrStatus);

	LPCWSTR GetStringFromWriterState(VSS_WRITER_STATE state);

    bool IsVolume( IN VSS_PWSZ pwszVolume );

    bool AddVolume( IN VSS_PWSZ pwszVolume, OUT bool & bAdded );

    INT RndDecision(IN INT nVariants = 2);

	LPWSTR DateTimeToString(
	    IN LONGLONG llTimestamp
	    );

	void DisplayCurrentTime();

// Implementation
private:

    // Global state
    bool                        m_bCoInitializeSucceeded;
    bool                        m_bAttachYourDebuggerNow;

    // Command line options
    unsigned int                m_uSeed;
    LONG                        m_lContext;
    EVssTestType                m_eTest;
    VSS_PWSZ                    m_pwszVolume;
    VSS_PWSZ                    m_pwszDiffAreaVolume;
    VSS_ID                      m_ProviderId;
    LONGLONG                    m_llMaxDiffArea;
    VSS_ID                      m_SnapshotId;
    VSS_ID                      m_SnapshotSetId;
    
    CComVariant                 m_value;
    UINT                        m_uPropertyId;

    // test-related members
    CVssSnapshotSetCollection   m_pSnapshotSetCollection;
    CVssVolumeMap               m_mapVolumes;
    CComPtr<IVssCoordinator>    m_pTimewarpCoord;
    CComPtr<IVssCoordinator>    m_pAllCoord;
    CComPtr<IVssBackupComponents>    m_pBackupComponents;

    // Command line
    INT                         m_nCurrentArgsCount;
    WCHAR **                    m_ppwszCurrentArgsArray;
};


#endif // __ML_HEADER_H__

