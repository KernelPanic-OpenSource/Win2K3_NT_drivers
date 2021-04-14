/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:

    Coord.hxx

Abstract:

    Declaration of CVssCoordinator


    Adi Oltean  [aoltean]  07/09/1999

TBD:
	
	Add comments.

Revision History:

    Name        Date        Comments
    aoltean     07/09/1999  Created
    aoltean     07/23/1999  Moving list in List.hxx,
                            Changing base classes for the Coordinator class
                            Fixing prototypes for RegisterProvider
    aoltean     07/23/1999  Adding support for Software and test provider
    aoltean     08/26/1999  Adding Recursive deletion methods
    aoltean     08/27/1999  Adding security and provider name unicity checking.
    aoltean     09/03/1999  Adding registry path constants and GetProviderProperties
    aoltean     09/09/1999  Adding constants from coord.cxx
                            dss -> vss
	aoltean		09/21/1999	Changing GetProviderProperties prototype
	aoltean		09/22/1999	Adding TransferEnumeratorContentsToArray
	aoltean		09/27/1999	Moving parts into provmgr.hxx, admin.hxx
	aoltean		10/12/1999	Adding thread set
	aoltean		10/13/1999	Moving most stuff in CVssSnapshotSetObject


--*/

#ifndef __VSS_COORD_HXX__
#define __VSS_COORD_HXX__

#if _MSC_VER > 1000
#pragma once
#endif

////////////////////////////////////////////////////////////////////////
//  Standard foo for file name aliasing.  This code block must be after
//  all includes of VSS header files.
//
#ifdef VSS_FILE_ALIAS
#undef VSS_FILE_ALIAS
#endif
#define VSS_FILE_ALIAS "CORCOORH"
//
////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CVssCoordinator

class CVssSnapshotSetObject;

class ATL_NO_VTABLE CVssCoordinator : 
    public CComObjectRoot,
    public CComCoClass<CVssCoordinator, &CLSID_VSSCoordinator>,
    public IVssCoordinator,
    public CVssAdmin,  // This one inherits from IVssAdmin
    public IVssShim
{

// Constructors& Destructors
private:
	CVssCoordinator(const CVssCoordinator&);

public:
    
    CVssCoordinator() {};
    ~CVssCoordinator();

    // This is the new creator class that fails during shutdown.
    // This class is used in OBJECT_ENTRY macro in svc.cxx
    class _CreatorClass
    {
    public:
    	static HRESULT WINAPI CreateInstance(void* pv, REFIID riid, LPVOID* ppv)
    	{
        	CVssFunctionTracer ft( VSSDBG_COORD, L"CVssCoordinator::_CreatorClass::CreateInstance");

        	// Create the instance
    	    ft.hr = CComCoClass<CVssCoordinator, &CLSID_VSSCoordinator>::_CreatorClass::CreateInstance(pv, riid, ppv);

            // Check for failures
            if (ft.HrFailed()) {
                ft.Trace(VSSDBG_COORD, L"Error while creating the coordinator object [0x%08lx]", ft.hr);
                return ft.hr;
            }

    	    // If the shutdown event occurs during creation we release the instance and return nothing (BUG 265455).
    	    // The _Module::Lock() will "detect" anyway that a shutdown already started BUT we cannot fail at that point.
    	    if (_Module.IsDuringShutdown()) {
                ft.Trace(VSSDBG_COORD, L"Warning: Shutdown started while while creating the coordinator object.");
    	        
    	        // Release the interface that we just created...
    	        BS_ASSERT(ppv);
    	        BS_ASSERT(*ppv);
    	        IUnknown* pUnk = (IUnknown*)(*ppv);
    	        (*ppv) = NULL;
    	        pUnk->Release();

    	        // Return E_OUTOFMEMORY
    	        ft.hr = E_OUTOFMEMORY;
    	        return ft.hr;
    	    }

            // Now we can be sure that the idle shutdown cannot be started since 
            // the _Module.Lock() method is already called
    	    return ft.hr;
    	}
    };

// ATL Stuff
public:

	DECLARE_REGISTRY_RESOURCEID( IDR_COORD )

	BEGIN_COM_MAP( CVssCoordinator )
		COM_INTERFACE_ENTRY( IVssCoordinator )
		COM_INTERFACE_ENTRY( IVssAdmin )
		COM_INTERFACE_ENTRY( IVssShim )
	END_COM_MAP()

// Overrides
public:

    // IVssCoordinator interface
    
	STDMETHOD(SetContext)(
		IN		LONG     lContext
		);

	STDMETHOD(StartSnapshotSet)(
		OUT		VSS_ID*     pSnapshotSetId
		);

    STDMETHOD(AddToSnapshotSet)(                          
        IN      VSS_PWSZ    pwszVolumeName,              
        IN      VSS_ID      ProviderId,                
		OUT 	VSS_ID		*pSnapshotId
        );                                             
                                                       
    STDMETHOD(DoSnapshotSet)(                             
        IN      IDispatch*  pCallback,
        OUT     IVssAsync** ppAsync        
        );                                             
                                                       
	STDMETHOD(GetSnapshotProperties)(
		IN  	VSS_ID			SnapshotId,
		OUT 	VSS_SNAPSHOT_PROP	*pProp
		);

	STDMETHOD(ExposeSnapshot)(
        IN      VSS_ID SnapshotId,
        IN      VSS_PWSZ wszPathFromRoot,
        IN      LONG lAttributes,
        IN      VSS_PWSZ wszExpose,
        OUT     VSS_PWSZ *pwszExposed
        );

    STDMETHOD(ImportSnapshots)(
		IN      BSTR bstrXMLSnapshotSet,
		OUT     IVssAsync** ppAsync
		);

    STDMETHOD(Query)(                                     
        IN      VSS_ID          QueriedObjectId,       
        IN      VSS_OBJECT_TYPE eQueriedObjectType,    
        IN      VSS_OBJECT_TYPE eReturnedObjectsType,  
        OUT     IVssEnumObject**ppEnum                
        );                                             

    STDMETHOD(DeleteSnapshots)(                         
        IN      VSS_ID          SourceObjectId,      
		IN      VSS_OBJECT_TYPE eSourceObjectType,
		IN		BOOL			bForceDelete,			
		OUT		LONG*			plDeletedSnapshots,		
		OUT		VSS_ID*			pNondeletedSnapshotID
        );                                           

	STDMETHOD(BreakSnapshotSet)(
		IN  	VSS_ID		    SnapshotSetId
		);

    STDMETHOD(RevertToSnapshot)(
        IN     VSS_ID     SnapshotId,
        IN      BOOL       bForceDismount
        );
    
    STDMETHOD(QueryRevertStatus)(
        IN     VSS_PWSZ       pwszVolume,
        OUT  IVssAsync **ppAsync
        );
    
    STDMETHOD(IsVolumeSupported)( 
        IN      VSS_ID          ProviderId,                
        IN      VSS_PWSZ        pwszVolumeName, 
        OUT     BOOL *          pbSupportedByThisProvider
        );

    STDMETHOD(IsVolumeSnapshotted)( 
        IN      VSS_ID          ProviderId,                
        IN      VSS_PWSZ        pwszVolumeName, 
        OUT     BOOL *          pbSnapshotsPresent,
    	OUT 	LONG *		    plSnapshotCompatibility
        );

    STDMETHOD(SetWriterInstances)( 
		IN  	LONG		    lWriterInstanceIdCount, 				
        IN      VSS_ID          *rgWriterInstanceId
        );

    // IVssShim

	STDMETHOD(SimulateSnapshotFreeze)(
	    IN      VSS_ID          guidSnapshotSetId,
		IN      ULONG           ulOptionFlags,	
		IN      ULONG           ulVolumeCount,	
		IN      VSS_PWSZ*       ppwszVolumeNamesArray,
		OUT     IVssAsync**     ppAsync 					
		);												

	STDMETHOD(SimulateSnapshotThaw)(
	    IN      VSS_ID            guidSnapshotSetId
	    );

	STDMETHOD(WaitForSubscribingCompletion)();												

// Implementation
private:

	CComPtr<CVssSnapshotSetObject>	m_pSnapshotSet;
	CComPtr<CVssShimObject>	        m_pShim;
};



#endif // __VSS_COORD_HXX__
