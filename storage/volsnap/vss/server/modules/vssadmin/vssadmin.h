/*++

Copyright (c) 1999  Microsoft Corporation

Abstract:

    @doc
    @module vssadmin.hxx | header of VSS demo
    @end

Author:

    Adi Oltean  [aoltean]  09/17/1999

TBD:
	
	Add comments.

Revision History:

    Name        Date        Comments
    aoltean     09/17/1999  Created

--*/


#ifndef __VSS_DEMO_H_
#define __VSS_DEMO_H_


/////////////////////////////////////////////////////////////////////////////
//  Defines and pragmas

// C4290: C++ Exception Specification ignored
#pragma warning(disable:4290)
// warning C4511: copy constructor could not be generated
#pragma warning(disable:4511)
// warning C4127: conditional expression is constant
#pragma warning(disable:4127)


/////////////////////////////////////////////////////////////////////////////
//  Includes


#include <wtypes.h>
#include <stddef.h>
#include <oleauto.h>
#include <comadmin.h>

// Enabling asserts in ATL and VSS
#include "vs_assert.hxx"

// ATL
#include <atlconv.h>
#include <atlbase.h>

// Application specific
#include "vs_inc.hxx"

// Generated MIDL headers
#include "vs_idl.hxx"

#include "copy.hxx"
#include "pointer.hxx"

#include "resource.h"

#include "vssmsg.h"
#include "msg.h"

#include "vswriter.h"
#include "vsbackup.h"

/////////////////////////////////////////////////////////////////////////////
//  Constants

const x_nStringBufferSize = 1024;	    // Includes the zero character

const x_nPollingInterval  = 2500;     // Three seconds

const x_nMaxRetriesCount = 4;        // Retries for polling

const WCHAR x_wszVssOptBoolTrue[] = L"TRUE";

#define VSSADM_E_NO_ITEMS_IN_QUERY          S_FALSE
#define VSSADM_E_FIRST_PARSING_ERROR        0x1001
#define VSSADM_E_INVALID_NUMBER             0x1001
#define VSSADM_E_INVALID_COMMAND            0x1002
#define VSSADM_E_INVALID_OPTION             0x1003
#define VSSADM_E_INVALID_OPTION_VALUE       0x1004
#define VSSADM_E_DUPLICATE_OPTION           0x1005
#define VSSADM_E_OPTION_NOT_ALLOWED_FOR_COMMAND 0x1006
#define VSSADM_E_REQUIRED_OPTION_MISSING    0x1007
#define VSSADM_E_INVALID_SET_OF_OPTIONS     0x1008
#define VSSADM_E_SNAPSHOT_NOT_FOUND	      0x1009
#define VSSADM_E_DELETION_DENIED			0x100a
#define VSSADM_E_LAST_PARSING_ERROR         0x100a

// Note: if any skus are added in the CVssSKU class, they need to 
// be updated here.  Make sure to update the SKU_INT and SKU_A 
// definitions as well.
#define SKU_C   CVssSKU::VSS_SKU_CLIENT
#define SKU_S   CVssSKU::VSS_SKU_SERVER
#define SKU_N   CVssSKU::VSS_SKU_NAS
#define SKU_I   CVssSKU::VSS_SKU_INVALID

#define SKU_INT  ((DWORD)(~SKU_C & ~ SKU_S & ~SKU_N))		

#define SKU_A   ( SKU_C | SKU_S | SKU_N | SKU_INT) // 0xffff
#define SKU_SN  ( SKU_S | SKU_N )
#define SKU_SNI (SKU_S | SKU_N | SKU_INT)

enum EVssAdmSnapshotType
{
    VSSADM_ST_FIRST = 0,
    VSSADM_ST_NAS_ROLLBACK = 0,
    VSSADM_ST_PERSISTENT_TIMEWARP,
    VSSADM_ST_TIMEWARP,
    VSSADM_ST_NUM_TYPES,
    VSSADM_ST_INVALID,
    VSSADM_ST_ALL
};

struct SVssAdmSnapshotTypeName
{
    LPCWSTR pwszName;
    DWORD dwSKUs;       // Specifies which SKUs this type is supported for snapshot creation using vssadmin, formed from ORing CVssSKU::EVssSKUType
    LONG lSnapshotContext;  // The snapshot context from vss.idl
    LONG pwszDescription;
};

//
//  List of all options.  This list must remain in sync with the g_asAdmOptions list.
//
enum EVssAdmOption
{
    VSSADM_O_FIRST = 0,
    VSSADM_O_ALL = 0,
    VSSADM_O_AUTORETRY,
    VSSADM_O_EXPOSE_USING,
    VSSADM_O_FOR,
    VSSADM_O_MAXSIZE,
    VSSADM_O_OLDEST,
    VSSADM_O_ON,
    VSSADM_O_PROVIDER,
    VSSADM_O_QUIET,
    VSSADM_O_SET,
    VSSADM_O_SHAREPATH,
    VSSADM_O_SNAPSHOT,
    VSSADM_O_SNAPTYPE,
    VSSADM_O_NUM_OPTIONS,
    VSSADM_O_INVALID
};

//
//  LIst of all commands.  This list must remain in sync with the g_asAdmCommands list.
//
enum EVssAdmCommand
{
    VSSADM_C_FIRST = 0,
    VSSADM_C_ADD_DIFFAREA_INT = 0,
    VSSADM_C_ADD_DIFFAREA_PUB,
    VSSADM_C_CREATE_SNAPSHOT_INT,
    VSSADM_C_CREATE_SNAPSHOT_PUB,
    VSSADM_C_DELETE_SNAPSHOTS_INT,
    VSSADM_C_DELETE_SNAPSHOTS_PUB,
    VSSADM_C_DELETE_DIFFAREAS_INT,
    VSSADM_C_DELETE_DIFFAREAS_PUB,
    VSSADM_C_EXPOSE_SNAPSHOT,
    VSSADM_C_LIST_PROVIDERS,
    VSSADM_C_LIST_SNAPSHOTS_INT,
    VSSADM_C_LIST_SNAPSHOTS_PUB,
    VSSADM_C_LIST_DIFFAREAS_INT,
    VSSADM_C_LIST_DIFFAREAS_PUB,
    VSSADM_C_LIST_VOLUMES_INT,
    VSSADM_C_LIST_VOLUMES_PUB,
    VSSADM_C_LIST_WRITERS,
    VSSADM_C_RESIZE_DIFFAREA_INT,
    VSSADM_C_RESIZE_DIFFAREA_PUB,
    VSSADM_C_NUM_COMMANDS,
    VSSADM_C_INVALID
};

enum EVssAdmOptionType
{
    VSSADM_OT_BOOL = 0,  // no qualifier on the option, i.e. /quiet, TRUE if present
    VSSADM_OT_STR,
    VSSADM_OT_NUM
};

struct SVssAdmOption
{
    EVssAdmOption eOpt;
    LPCWSTR pwszOptName;  // The option name as typed on the command-line, i.e. the "for" in /for=XXXX
    EVssAdmOptionType eOptType;
};

//
//  Specifies the validity of the option for a particular command.
//
enum EVssAdmOptionFlag
{
    V_NO = 0,  //  Option not allowed
    V_YES,     //  Option manditory
    V_OPT      //  Option optional
};

//
//  The main command structure.  The commands are structured like:
//  vssadmin <pwszMajorOption> <pwszMinorOption> <OPTIONS>
//
struct SVssAdmCommandsEntry
{
    LPCWSTR pwszMajorOption;
    LPCWSTR pwszMinorOption;
    EVssAdmCommand eAdmCmd;
    DWORD dwSKUs;       // Specifies which SKUs this command is supported, formed from ORing CVssSKU::EVssSKUType
    LONG lMsgGen;
    LONG lMsgDetail;
    BOOL bShowSSTypes;  //  If true, in detailed usage show a list of valid snapshot types at end of message
    EVssAdmOptionFlag aeOptionFlags[VSSADM_O_NUM_OPTIONS]; // Array of option flags indexed by EVssAdmOption
};


//
//  The structure of the parsed command.  One of these is created by the 
//  ParseCmdLine method.
//
struct SVssAdmParsedCommand
{
    EVssAdmCommand eAdmCmd;
    LPWSTR apwszOptionValues[VSSADM_O_NUM_OPTIONS];

    //  Simple initializer constructor
    SVssAdmParsedCommand()
    {
        eAdmCmd = VSSADM_C_INVALID;
        // psUnnamedOptions = NULL;
        
        //  Clear out the option values arrays
        for ( INT i = 0; i < VSSADM_O_NUM_OPTIONS; ++i )
            apwszOptionValues[ i ] = NULL;
    };
    ~SVssAdmParsedCommand()
    {
        //  Free any allocated memory
        for ( INT i = 0; i < VSSADM_O_NUM_OPTIONS; ++i )
            ::VssFreeString( apwszOptionValues[ i ] );
        
    }
};

extern const SVssAdmOption g_asAdmOptions[];
extern const SVssAdmCommandsEntry g_asAdmCommands[];
extern const SVssAdmSnapshotTypeName g_asAdmTypeNames[];

LPWSTR GuidToString(
    IN GUID guid
    );

LPWSTR LonglongToString(
    IN LONGLONG llValue
    );

LPWSTR DateTimeToString(
    IN VSS_TIMESTAMP *pTimeStamp
    );

WCHAR MyGetChar(
    );

BOOL MapVssErrorToMsg(
	IN HRESULT hr,
	OUT LONG *plMsgNum
    ) throw( HRESULT );
   

/////////////////////////////////////////////////////////////////////////////
//	class CVssAdminCLI

class CCommandVerifier;

class CVssAdminCLI
{
// Enums and typedefs
private:

	enum _RETURN_VALUE
	{
		VSS_CMDRET_SUCCESS      = 0,
		VSS_CMDRET_EMPTY_RESULT = 1,
		VSS_CMDRET_ERROR        = 2,
	};

// Constructors& destructors
private:
	CVssAdminCLI(const CVssAdminCLI&);
	CVssAdminCLI();

public:
	CVssAdminCLI(
        IN INT argc,
        IN PWSTR argv[]
		);
	~CVssAdminCLI();

// Attributes
private:
    BOOL       IsQuiet() { return GetOptionValueBool( VSSADM_O_QUIET ); }
    
	INT        GetReturnValue() { return m_nReturnValue; };

    LPWSTR     GetOptionValueStr(
        IN EVssAdmOption eOption
        )
    {
        CVssFunctionTracer ft( VSSDBG_VSSADMIN, L"CVssAdminCLI::GetOptionValueStr" );

        BS_ASSERT( g_asAdmOptions[ eOption ].eOptType == VSSADM_OT_STR );
//        BS_ASSERT( g_asAdmCommands[ m_sParsedCommand.eAdmCmd].aeOptionFlags[ eOption ] != V_NO );
        return m_sParsedCommand.apwszOptionValues[ eOption ];
    };

    BOOL        GetOptionValueBool(
        IN EVssAdmOption eOption
        )
    {
        CVssFunctionTracer ft( VSSDBG_VSSADMIN, L"CVssAdminCLI::GetOptionValueBool" );

        BS_ASSERT( g_asAdmOptions[ eOption ].eOptType == VSSADM_OT_BOOL );
        BS_ASSERT( g_asAdmCommands[ m_sParsedCommand.eAdmCmd].aeOptionFlags[ eOption ] != V_NO );
        return m_sParsedCommand.apwszOptionValues[ eOption ] != NULL;
    };

    BOOL        GetOptionValueNum(
        IN EVssAdmOption eOption,
        OUT LONGLONG *pllValue,
    	IN BOOL bSuffixAllowed = TRUE    
        ) throw( HRESULT )
    {
        CVssFunctionTracer ft( VSSDBG_VSSADMIN, L"CVssAdminCLI::GetOptionValueNum" );

        BS_ASSERT( g_asAdmOptions[ eOption ].eOptType == VSSADM_OT_NUM );
        BS_ASSERT( g_asAdmCommands[ m_sParsedCommand.eAdmCmd].aeOptionFlags[ eOption ] != V_NO );
        if ( m_sParsedCommand.apwszOptionValues[ eOption ] == NULL )
        {
            BS_ASSERT( g_asAdmCommands[ m_sParsedCommand.eAdmCmd].aeOptionFlags[ eOption ] == V_OPT );
            //  Option wasn't specified on command line - an optional one
            *pllValue = 0;
            return FALSE; 
        }
        *pllValue = ScanNumber( m_sParsedCommand.apwszOptionValues[ eOption ], bSuffixAllowed );

        return TRUE;
    };


// Operations
public:

    static HRESULT Main(
        IN INT argc,
        IN PWSTR argv[]
	    );

private:

	void Initialize(
		) throw(HRESULT);

	BOOL ParseCmdLine(
		) throw(HRESULT);

	void DoProcessing(
		) throw(HRESULT);

	void Finalize();

// Processing
private:

	void PrintUsage(
		) throw(HRESULT);

    // The following are the methods that get called for each command.
	void AddDiffArea(
		) throw(HRESULT);
    
	void CreateSnapshot(
		) throw(HRESULT);

	void DeleteDiffAreas(
		) throw(HRESULT);

	void DeleteSnapshots(
		) throw(HRESULT);

	void ExposeSnapshot(
		) throw(HRESULT);

	void ListDiffAreas(
		) throw(HRESULT);
	
	void ListProviders(
		) throw(HRESULT);

	void ListSnapshots(
		) throw(HRESULT);

	void ListVolumes(
		) throw(HRESULT);

	void ListWriters(
		) throw(HRESULT);

	void ResizeDiffArea(
		) throw(HRESULT);

// Implementation
private:
    static BOOL UnloggableError(IN HRESULT hError);

    void GetDifferentialSoftwareSnapshotMgmtInterface(
        IN   VSS_ID ProviderId,
        IN   IVssSnapshotMgmt *pIMgmt,
    	OUT  IUnknown**  ppItf
    	);

    LPCWSTR GetVolumeDisplayName( 
        IN  LPCWSTR pwszVolumeName
        );
    
    LONG DetermineSnapshotType(
        IN LPCWSTR pwszType
        ) throw(HRESULT);

    LPWSTR DetermineSnapshotType(
        IN LONG lSnapshotAttributes
        ) throw(HRESULT);

    void DisplayDiffAreasPrivate(
   	    IVssEnumMgmtObject *pIEnumMgmt	
	    ) throw(HRESULT);

    LPWSTR BuildSnapshotAttributeDisplayString(
        IN DWORD Attr
        ) throw(HRESULT);
    
	void DumpSnapshotTypes(
		) throw(HRESULT);

	LPCWSTR LoadString(
		IN	UINT nStringId
		) throw(HRESULT);

	LPCWSTR GetNextCmdlineToken(
		IN	bool bFirstToken = false
		) throw(HRESULT);

	bool Match(
		IN	LPCWSTR wszString,
		IN	LPCWSTR wszPatternString
		) throw(HRESULT);

	bool ScanGuid(
		IN	LPCWSTR wszString,
		OUT	VSS_ID& Guid
		) throw(HRESULT);

	void Output(
    	IN	LPCWSTR wszFormat,
		...
		) throw(HRESULT);

    void OutputMsg(
        IN  LONG msgId,
        ...
        ) throw(HRESULT);

    void OutputOnConsole(
        IN	LPCWSTR wszStr
        );

   
    LPWSTR GetMsg(
    	IN  BOOL bLineBreaks,	
        IN  LONG msgId,
        ...
        );

    void AppendMessageToStr(
        IN LPWSTR pwszString,
        IN SIZE_T cMaxStrLen,
        IN LONG lMsgId,
        IN DWORD AttrBit,
        IN LPCWSTR pwszDelimitStr
        ) throw( HRESULT );
    
    LONGLONG ScanNumber(
    	IN LPCWSTR pwszNumToConvert,
    	IN BOOL bSuffixAllowed
        ) throw( HRESULT );
    
    LPWSTR FormatNumber(
    	IN LONGLONG llNum
        ) throw(HRESULT);
    
    void OutputErrorMsg(
        IN  LONG msgId,
        ...
        ) throw(HRESULT);

    BOOL PromptUserForConfirmation(
    	IN LONG lPromptMsgId,
    	IN ULONG ulNum	
    	);

    void GetProviderId(
	    OUT	VSS_ID *pProviderId
        );
    
	LPCWSTR GetProviderName(
		IN	VSS_ID& ProviderId
		) throw(HRESULT);

    BOOL GetProviderIdByName(
	    IN  LPCWSTR pwszProviderName,
	    OUT	VSS_ID *pProviderId
	    ) throw(HRESULT);

// Data members
private:
    CCommandVerifier* m_pVerifier;
	HANDLE              m_hConsoleOutput;
    CVssSimpleMap<UINT, LPCWSTR> m_mapCachedResourceStrings;
    CVssSimpleMap<VSS_ID, LPCWSTR> m_mapCachedProviderNames;
    CVssSimpleMap<LPCWSTR, LPCWSTR> *m_pMapVolumeNames;
    
	INT                 m_nReturnValue;

    INT                 m_argc;
    PWSTR               *m_argv;
	
	EVssAdmCommand      m_eCommandType;
	SVssAdmParsedCommand m_sParsedCommand;
	VSS_OBJECT_TYPE		m_eFilterObjectType;
	VSS_OBJECT_TYPE		m_eListedObjectType;
	VSS_ID				m_FilterSnapshotId;
};

class CVssAutoSnapshotProperties
{
// Constructors/destructors
private:
	CVssAutoSnapshotProperties(const CVssAutoSnapshotProperties&);

public:
	CVssAutoSnapshotProperties(VSS_SNAPSHOT_PROP &Snap): m_pSnap(&Snap) {};
	CVssAutoSnapshotProperties(VSS_OBJECT_PROP &Prop): m_pSnap(&Prop.Obj.Snap) {};

	// Automatically closes the handle
	~CVssAutoSnapshotProperties() {
	    Clear();
	};

// Operations
public:

	// Returns the value
	VSS_SNAPSHOT_PROP *GetPtr() {
		return m_pSnap;
	}
	
	// NULLs out the pointer.  Used after a pointer has been transferred to another
	// funtion.
	void Transferred() {
		m_pSnap = NULL;
	}

	// Clears the contents of the auto string
	void Clear() {
	    if ( m_pSnap != NULL )
	    {
    		::VssFreeSnapshotProperties(m_pSnap);
    		m_pSnap = NULL;
	    }
	}

    // Returns the value to the actual pointer
	VSS_SNAPSHOT_PROP* operator->() const {
	    return m_pSnap;
	}
	
	// Returns the value of the actual pointer
	operator VSS_SNAPSHOT_PROP* () const {
		return m_pSnap;
	}

private:
	VSS_SNAPSHOT_PROP *m_pSnap;
};


#endif //__VSS_DEMO_H_
