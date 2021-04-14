/*++

Copyright (c) 1999  Microsoft Corporation

Abstract:

    @doc
    @module Provider.hxx | Declarations used by the Software Snapshot Provider interface
    @end

Author:

    Adi Oltean  [aoltean]   07/13/1999

Revision History:

    Name        Date        Comments

    aoltean     07/13/1999  Created.
    aoltean     08/17/1999  Change CommitSnapshots to CommitSnapshot
    aoltean     09/23/1999  Using CComXXX classes for better memory management
                            Renaming back XXXSnapshots -> XXXSnapshot
    aoltean     09/26/1999  Returning a Provider Id in OnRegister
    aoltean     09/09/1999  Adding PostCommitSnapshots
                            dss->vss
	aoltean		09/20/1999	Making asserts more cleaner.
	aoltean		09/21/1999	Small renames

--*/


/////////////////////////////////////////////////////////////////////////////
//  Includes



#include "stdafx.hxx"
#include <winnt.h>

#include "vs_idl.hxx"

#include "resource.h"
#include "vs_inc.hxx"
#include "vs_sec.hxx"
#include "ichannel.hxx"
#include "ntddsnap.h"

#include "swprv.hxx"

#include "copy.hxx"
#include "pointer.hxx"
#include "enum.hxx"

#include "qsnap.hxx"
#include "provider.hxx"
#include "diff.hxx"
#include "alloc.hxx"

#include "vs_quorum.hxx"

////////////////////////////////////////////////////////////////////////
//  Standard foo for file name aliasing.  This code block must be after
//  all includes of VSS header files.
//
#ifdef VSS_FILE_ALIAS
#undef VSS_FILE_ALIAS
#endif
#define VSS_FILE_ALIAS "SPRPROVC"
//
////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  Global Definitions

CVssCriticalSection CVsSoftwareProvider::m_cs;

CVssDLList<CVssQueuedSnapshot*>	 CVssQueuedSnapshot::m_list;


STDMETHODIMP CVsSoftwareProvider::SetContext(
		IN  	LONG 	lContext				
		)
/*++

Routine description:

    Implements IVssSnapshotProvider::SetContext

Error codes:

    E_ACCESSDENIED
        - The user is not an administrator
    E_INVALIDARG
        - Invalid argument
    E_OUTOFMEMORY
        - lock failures.
    VSS_E_BAD_STATE
        - Attempt to change the context while it is frozen. It is illegal to
        change the context after the first call on the IVssSnapshotProvider object.

--*/
{
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::SetContext");

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                      L"The client is not an administrator");

        // Trace parameters
        ft.Trace( VSSDBG_SWPRV, L"Parameters: lContext = %ld\n", lContext );

        // Lock in order to update both variables atomically
		// The critical section will be left automatically at the end of scope.
        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        // Check if the context has been freezed
        if (m_bContextFrozen) {
            BS_ASSERT(false);
            ft.Throw( VSSDBG_SWPRV, VSS_E_BAD_STATE,
                      L"The context is already frozen");
        }

        // Change the context
        // Our provider is a DIFFERENTIAL snapshot provider, therefore reject PLEX snapshots
        if (lContext == VSS_CTX_ALL) {
            m_lSnapContext = lContext;
            m_bContextFrozen = true;
        } else {
            if (lContext & VSS_VOLSNAP_ATTR_PLEX) 
                ft.Throw( VSSDBG_SWPRV, VSS_E_UNSUPPORTED_CONTEXT, L"Invalid context (Plex)");
    
            if (lContext & VSS_VOLSNAP_ATTR_DIFFERENTIAL) {
                // Note (752794): In the future we may need to store the exact context in SWPRV
                // This requires more code changes though in filtering code (find.cxx , delete.cxx),
                // validation code (persist.cxx) and others. 
                // Right now we are just removing the differential attribute from the SWPRV internal context
                ft.Trace( VSSDBG_SWPRV, L"Removing DIFFERENTIAL attribute from internal snapshot context\n");
                lContext &= ~(VSS_VOLSNAP_ATTR_DIFFERENTIAL);
            }
    
            switch(lContext) {
            case VSS_CTX_CLIENT_ACCESSIBLE:
            case VSS_CTX_BACKUP:
            case VSS_CTX_FILE_SHARE_BACKUP:
            case VSS_CTX_NAS_ROLLBACK:  
            case VSS_CTX_APP_ROLLBACK:  
                m_lSnapContext = lContext;
                m_bContextFrozen = true;
                break;
    
            default:
                ft.Throw( VSSDBG_SWPRV, VSS_E_UNSUPPORTED_CONTEXT, L"Invalid context");
            }
        }
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}


LONG CVsSoftwareProvider::GetContextInternal() const
/*++

Routine description:

    Returns the current context

--*/
{
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::GetContextInternal");

    return m_lSnapContext;
}


void CVsSoftwareProvider::FreezeContext()
/*++

Routine description:

    Freezes the current context. To be called in IVssSoftwareProvider methods.

--*/
{
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::FreezeContext");

    // m_bContextFrozen may be already true...
    m_bContextFrozen = true;
}



/////////////////////////////////////////////////////////////////////////////
//  Definitions


STDMETHODIMP CVsSoftwareProvider::BeginPrepareSnapshot(
    IN      VSS_ID          SnapshotSetId,
    IN      VSS_ID          SnapshotId,
    IN      VSS_PWSZ     pwszVolumeName,
    IN      LONG             lNewContext      
    )

/*++

Description:

	Creates a Queued Snapshot object to be committed later.

Algorithm:

	1) Creates an internal VSS_SNAPSHOT_PROP structure that will keep most of the properties.
	2) Creates an CVssQueuedSnapshot object and insert it into the global queue of snapshots pending to commit.
	3) Set the state of the snapshot as PREPARING.
	4) If needed, create the snapshot object and return it to the caller.

Remarks:

	The queued snapshot object keeps a reference count. At the end of this function it will be:
		1 = the queued snap obj is reffered by the global queue (if no snapshot COM object was returned)
		2 = reffered by the global queue and by the returned snapshot COM object

Called by:

	IVssCoordinator::AddToSnapshotSet

Error codes:

    VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER
        - Volume not supported by provider.
    VSS_E_MAXIMUM_NUMBER_OF_SNAPSHOTS_REACHED
        - Maximum number of snapshots reached.
    E_ACCESSDENIED
        - The user is not an administrator
    E_INVALIDARG
        - Invalid arguments

    [CVssSoftwareProvider::GetVolumeInformation]
        E_OUTOFMEMORY
        VSS_E_PROVIDER_VETO
            An error occured while opening the IOCTL channel. The error is logged.
        E_UNEXPECTED
            Unexpected programming error. Nothing is logged.
        VSS_E_OBJECT_NOT_FOUND
            The device does not exist or it is not ready.

--*/


{
    UNREFERENCED_PARAMETER(lNewContext);

    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::BeginPrepareSnapshot" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
             L"  SnapshotId = " WSTR_GUID_FMT 	L"\n"
             L"  SnapshotSetId = " WSTR_GUID_FMT 	L"\n"
             L"  VolumeName = %s,\n"
             L"  ppSnapshot = %p,\n",
             GUID_PRINTF_ARG( SnapshotId ),
             GUID_PRINTF_ARG( SnapshotSetId ),
             pwszVolumeName);

        // Argument validation
		if ( SnapshotSetId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotSetId == GUID_NULL");
        if ( pwszVolumeName == NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"pwszVolumeName is NULL");
        if ( SnapshotId == GUID_NULL)
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Snapshot ID is NULL");

        if (m_ProviderInstanceID == GUID_NULL)
            ft.Throw( VSSDBG_SWPRV, E_OUTOFMEMORY, L"The Provider instance ID could not be generated");

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        // Check to see if the volume is supported.
        // This may throw VSS_E_OBJECT_NOT_FOUND or even VSS_E_PROVIDER_VETO if an error occurs.
        //
        LONG lVolAttr = GetVolumeInformationFlags(pwszVolumeName, GetContextInternal());
        if ((lVolAttr & VSS_VOLATTR_SUPPORTED_FOR_SNAPSHOT) == 0)
            ft.Throw( VSSDBG_SWPRV, VSS_E_VOLUME_NOT_SUPPORTED_BY_PROVIDER, L"Volume not supported");

        //
        //  Freeze the context
        //
        FreezeContext();
        BS_ASSERT((GetContextInternal() == VSS_CTX_BACKUP) 
            || (GetContextInternal() == VSS_CTX_CLIENT_ACCESSIBLE)
            || (GetContextInternal() == VSS_CTX_NAS_ROLLBACK)
            || (GetContextInternal() == VSS_CTX_APP_ROLLBACK)
            || (GetContextInternal() == VSS_CTX_FILE_SHARE_BACKUP)
            );

        //
        //  Remove the non-autodelete snapshots from previous snapshot sets, if any
        //
        RemoveSnapshotsFromGlobalList(m_ProviderInstanceID, SnapshotSetId, VSS_QST_REMOVE_ALL_NON_AUTORELEASE);

        //
        //  Get local computer name
        //

        // compute size of computer name.  Initially a null output buffer is
        // passed in in order to figure out the size of the computer name.  We
        // expect that the error from this call is ERROR_MORE_DATA.
        DWORD cwc = 0;
        BOOL bResult = ::GetComputerNameEx(ComputerNameDnsFullyQualified, NULL, &cwc);
        BS_ASSERT(bResult == FALSE);
        DWORD dwErr = GetLastError();
        if (dwErr != ERROR_MORE_DATA)
            ft.TranslateInternalProviderError(VSSDBG_SWPRV, 
                HRESULT_FROM_WIN32(dwErr),
                VSS_E_PROVIDER_VETO,
                L"GetComputerNameEx(%d, NULL, [%lu]) [%ld]", 
                ComputerNameDnsFullyQualified,
                cwc, (LONG)bResult);

        // allocate space for computer name
        CVssAutoPWSZ awszComputerName;
        awszComputerName.Allocate(cwc);

        // get the computer name
        if (!GetComputerNameEx(ComputerNameDnsFullyQualified, awszComputerName.GetRef(), &cwc))
            ft.TranslateInternalProviderError(VSSDBG_SWPRV, 
                HRESULT_FROM_WIN32(GetLastError()),
                VSS_E_PROVIDER_VETO,
                L"GetComputerNameEx(%d, %p, %lu)", 
                ComputerNameDnsFullyQualified,
                awszComputerName.GetRef(),
                cwc);

        //
		// Create the structure that will keep the prepared snapshot state.
        //

		VSS_OBJECT_PROP_Ptr ptrSnapshot;
		ptrSnapshot.InitializeAsSnapshot( ft,
			SnapshotId,
			SnapshotSetId,
			0,
			NULL,
			pwszVolumeName,
			awszComputerName,
			awszComputerName,
			NULL,
			NULL,
			VSS_SWPRV_ProviderId,
			// always add DIFFERENTIAL attribute
            (GetContextInternal() | (VSS_VOLSNAP_ATTR_DIFFERENTIAL)),  
			0,
			VSS_SS_PREPARING
			);

		// Create the snapshot object. After this assignment the ref count becomes 1.
		CComPtr<CVssQueuedSnapshot> ptrQueuedSnap = new CVssQueuedSnapshot(
            ptrSnapshot, m_ProviderInstanceID, GetContextInternal());
		if (ptrQueuedSnap == NULL)
			ft.Throw( VSSDBG_SWPRV, E_OUTOFMEMORY, L"Memory allocation error");

		// The structure was detached into the queued object
		// since the ownership was passed to the constructor.
		ptrSnapshot.Reset();

		// Add the snapshot object to the global queue. No exceptions should be thrown here.
		// The reference count will be 2.
		ptrQueuedSnap->AttachToGlobalList();

        // The destructor for the smart pointer will be called. The reference count will be 1
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}



STDMETHODIMP CVsSoftwareProvider::EndPrepareSnapshots(
    IN      VSS_ID          SnapshotSetId
    )

/*++

Description:

	This function gets called by the coordinator as a rendez-vous method
    in order to finish the prepare phase for snapshots
    (like ending the background prepare tasks or performing the lengthly operations before
    issuing the snapshots freeze).

	This function acts on the given snapshot set (i.e. to call IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT
    on each snapshotted volume)

Algorithm:

	For each preparing snapshot (but not prepared yet) in this snapshot set:
		2) Call IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT
		3) Change the state of the snapshot to VSS_SS_PREPARED

	Compute the number of prepared snapshots.

	If a snapshot fails in operations above then the coordinator is responsible to Abort

Called by:

	IVssCoordinator::DoSnapshotsSet in the first phase (i.e. EndPrepare All Snapshots).

Remarks:

	- While calling this, Lovelace is not holding yet writes on snapshotted volumes.
	- The coordinator may issue many EndPrepareSnapshots calls for the same Snapshot Set ID.
	- This function can be called on a subsequent retry of DoSnapshotSet or immediately
	after PrepareSnapshots therefore the state of all snapshots must be PREPARING before calling this function.

Return codes:

    E_ACCESSDENIED
    E_INVALIDARG
    VSS_E_PROVIDER_VETO
        - runtime errors. Logging done.
    VSS_E_INSUFFICIENT_STORAGE
        Not enough disk storage to create a snapshot (for ex. diff area)
        (remark: when a snapshot is not created since there is not enough disk space 
        this error is not guaranteed to be returned. VSS_E_PROVIDER_VETO or VSS_E_OBJECT_NOT_FOUND 
        may also be returned in that case.)
    VSS_E_OBJECT_NOT_FOUND
        If the internal volume name does not correspond to an existing volume then
        return VSS_E_OBJECT_NOT_FOUND (Bug 227375)
    E_UNEXPECTED
        - dev errors. No logging.
    E_OUTOFMEMORY

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::EndPrepareSnapshots" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        // Argument validation
		if ( SnapshotSetId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotSetId == GUID_NULL");

		// Trace parameters
        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
			L"  SnapshotSetId = " WSTR_GUID_FMT L"\n",
			GUID_PRINTF_ARG( SnapshotSetId )
			);

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Make sure that the context is frozen
        //
        BS_ASSERT(IsContextFrozen());

        //
        // Allocate the diff areas
        //

        CVssDiffAreaAllocator allocator(GetContextInternal(), SnapshotSetId);

        // Compute all new diff areas
        // This method may throw
        allocator.AssignDiffAreas();

        //
        // Change the state for the existing snapshots
        //

		CVssSnapIterator snapIterator;
        while (true)
        {
			CComPtr<CVssQueuedSnapshot> ptrQueuedSnapshot = snapIterator.GetNextBySnapshotSet(SnapshotSetId);

			// End of enumeration?
			if (ptrQueuedSnapshot == NULL)
				break;

			// Get the snapshot structure
			PVSS_SNAPSHOT_PROP pProp = ptrQueuedSnapshot->GetSnapshotProperties();
			BS_ASSERT(pProp != NULL);

            ft.Trace( VSSDBG_SWPRV, L"Field values for %p: \n"
                 L"  SnapshotSetId = " WSTR_GUID_FMT L"\n"
                 L"  SnapshotId = " WSTR_GUID_FMT L"\n"
                 L"  VolumeName = %s\n"
                 L"  Creation timestamp = " WSTR_LONGLONG_FMT L"\n"
                 L"  lAttributes = 0x%08lx\n"
                 L"  status = %d\n",
                 pProp,
                 GUID_PRINTF_ARG( pProp->m_SnapshotSetId ),
                 GUID_PRINTF_ARG( pProp->m_SnapshotId ),
				 pProp->m_pwszOriginalVolumeName,
                 LONGLONG_PRINTF_ARG( pProp->m_tsCreationTimestamp ),
                 pProp->m_lSnapshotAttributes,
				 pProp->m_eStatus);

			// Deal only with the snapshots that must be pre-committed.
			switch(ptrQueuedSnapshot->GetStatus())
			{
			case  VSS_SS_PREPARING:

                {
                    // Purge previous Timewarp snapshots, as needed
                    DWORD dwMaxCAOlderShadowCopies = (m_dwMaxCAShadowCopies > 0)? m_dwMaxCAShadowCopies - 1: 0;
                    PurgeSnapshotsOnVolume(
                        pProp->m_pwszOriginalVolumeName, 
                        false,
                        dwMaxCAOlderShadowCopies
                        );
                }

                // Remark - we are supposing here that only one snaphsot set can be
                // in progress. We are not checking again if the volume has snapshots.

				// Mark the state of this snapshot as failed
                // in order to correctly handle the state
				ptrQueuedSnapshot->MarkAsProcessingPrepare();

				// Open the volume IOCTL channel for that snapshot.
				ptrQueuedSnapshot->OpenVolumeChannel();
					
				// Send the IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT ioctl.
				ptrQueuedSnapshot->PrepareForSnapshotIoctl();

				// Mark the snapshot as prepared
				ptrQueuedSnapshot->MarkAsPrepared();
				break;

			case VSS_SS_PREPARED:

				// Snapshot was already prepared in another call
				break;

			default:
				BS_ASSERT(false);
				ft.Throw( VSSDBG_SWPRV, E_UNEXPECTED, L"bad state %d", ptrQueuedSnapshot->GetStatus());
			}
        }

        // Commit all diff areas allocations
        // (otherwise the diff areas changes will be rollbacked in destructor)
        allocator.Commit();
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}


STDMETHODIMP CVsSoftwareProvider::PreCommitSnapshots(
    IN      VSS_ID          SnapshotSetId
    )

/*++

Description:

	This function gets called by the coordinator in order to pre-commit all snapshots
	on the given snapshot set

Algorithm:

	For each prepared snapshot (but not precommitted yet) in this snapshot set:
		1) Change the state of the snapshot to VSS_SS_PRECOMMITTED

	Compute the number of pre-committed snapshots.

	If a snapshot fails in operations above then the coordinator is responsible to Abort

Called by:

	IVssCoordiantor::DoSnapshotsSet in the second phase (i.e. Pre-Commit All Snapshots).

Remarks:

	- While calling this, Lovelace is not holding yet writes on snapshotted volumes.
	- The coordinator may issue many PreCommitSnapshots calls for the same Snapshot Set ID.
	- This function can be called on a subsequent retry of DoSnapshotSet or immediately
	after EndPrepareSnapshots therefore the state of all snapshots must be PREPARED before calling this function.

Return codes:

    E_ACCESSDENIED
    E_INVALIDARG
    VSS_E_PROVIDER_VETO
        - runtime errors (for example: cannot find diff areas). Logging done.
    VSS_E_OBJECT_NOT_FOUND
        If the internal volume name does not correspond to an existing volume then
        return VSS_E_OBJECT_NOT_FOUND (Bug 227375)
    E_UNEXPECTED
        - dev errors. No logging.
    E_OUTOFMEMORY

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::PreCommitSnapshots" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        // Argument validation
		if ( SnapshotSetId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotSetId == GUID_NULL");

		// Trace parameters
        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
			L"  SnapshotSetId = " WSTR_GUID_FMT L"\n",
			GUID_PRINTF_ARG( SnapshotSetId )
			);

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Make sure that the context is frozen
        //
        BS_ASSERT(IsContextFrozen());

		CVssSnapIterator snapIterator;
        while (true)
        {
			CComPtr<CVssQueuedSnapshot> ptrQueuedSnapshot = snapIterator.GetNextBySnapshotSet(SnapshotSetId);

			// End of enumeration?
			if (ptrQueuedSnapshot == NULL)
				break;

			// Get the snapshot structure
			PVSS_SNAPSHOT_PROP pProp = ptrQueuedSnapshot->GetSnapshotProperties();
			BS_ASSERT(pProp != NULL);

            ft.Trace( VSSDBG_SWPRV, L"Field values for %p: \n"
                 L"  SnapshotSetId = " WSTR_GUID_FMT L"\n"
                 L"  SnapshotId = " WSTR_GUID_FMT L"\n"
                 L"  VolumeName = %s\n"
                 L"  Creation timestamp = " WSTR_LONGLONG_FMT L"\n"
                 L"  lAttributes = 0x%08lx\n"
                 L"  status = %d\n",
                 pProp,
                 GUID_PRINTF_ARG( pProp->m_SnapshotSetId ),
                 GUID_PRINTF_ARG( pProp->m_SnapshotId ),
				 pProp->m_pwszOriginalVolumeName,
                 LONGLONG_PRINTF_ARG( pProp->m_tsCreationTimestamp ),
                 pProp->m_lSnapshotAttributes,
				 pProp->m_eStatus);

			// Deal only with the snapshots that must be pre-committed.
			switch(ptrQueuedSnapshot->GetStatus())
			{
			case  VSS_SS_PREPARED:

				// Mark the snapshot as processing pre-commit
				ptrQueuedSnapshot->MarkAsProcessingPreCommit();

				// Mark the snapshot as pre-committed
                // Do nothing in Babbage provider
				ptrQueuedSnapshot->MarkAsPreCommitted();

				break;

			case VSS_SS_PRECOMMITTED:

				// Snapshot was already pre-committed in another call
				break;

			default:
				BS_ASSERT(false);
				ft.Throw( VSSDBG_SWPRV, E_UNEXPECTED, L"bad state %d", ptrQueuedSnapshot->GetStatus());
			}
        }
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}


STDMETHODIMP CVsSoftwareProvider::CommitSnapshots(
    IN      VSS_ID          SnapshotSetId
    )

/*++

Description:

	This function gets called by the coordinator in order to commit all snapshots
	on the given snapshot set (i.e. to call IOCTL_VOLSNAP_COMMIT_SNAPSHOT on each snapshotted volume)

Algorithm:

	For each precommitted (but not yet committed) snapshot in this snapshot set:
		2) Call IOCTL_VOLSNAP_COMMIT_SNAPSHOT
		3) Change the state of the snapshot to VSS_SS_COMMITTED

	Return the number of committed snapshots, if success.
	Otherwise return 0 (even if some snapshots were committed).

	If a snapshot fails in operations above then the coordinator is responsible to Abort

Called by:

	IVssCoordinator::DoSnapshotsSet in the third phase (i.e. Commit All Snapshots).

Remarks:

	- While calling this, Lovelace is already holding writes on snapshotted volumes.
	- The coordinator may issue many CommitSnapshots calls for the same Snapshot Set ID.

Return codes:

    E_ACCESSDENIED
    E_INVALIDARG
    VSS_E_PROVIDER_VETO
        - runtime errors (for example: cannot find diff areas). Logging done.
    VSS_E_OBJECT_NOT_FOUND
        If the internal volume name does not correspond to an existing volume then
        return VSS_E_OBJECT_NOT_FOUND (Bug 227375)
    E_UNEXPECTED
        - dev errors. No logging.
    E_OUTOFMEMORY

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::CommitSnapshots" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        // Argument validation
		if ( SnapshotSetId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotSetId == GUID_NULL");

		// Trace parameters
        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
			L"  SnapshotSetId = " WSTR_GUID_FMT L"\n",
			GUID_PRINTF_ARG( SnapshotSetId ));

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Make sure that the context is frozen
        //
        BS_ASSERT(IsContextFrozen());

		CVssSnapIterator snapIterator;
        while (true)
        {
			CComPtr<CVssQueuedSnapshot> ptrQueuedSnapshot = snapIterator.GetNextBySnapshotSet(SnapshotSetId);

			// End of enumeration?
			if (ptrQueuedSnapshot == NULL)
				break;

			// Get the snapshot structure
			PVSS_SNAPSHOT_PROP pProp = ptrQueuedSnapshot->GetSnapshotProperties();
			BS_ASSERT(pProp != NULL);

            ft.Trace( VSSDBG_SWPRV, L"Field values for %p: \n"
                 L"  SnapshotSetId = " WSTR_GUID_FMT L"\n"
                 L"  SnapshotId = " WSTR_GUID_FMT L"\n"
                 L"  VolumeName = %s\n"
                 L"  Creation timestamp = " WSTR_LONGLONG_FMT L"\n"
                 L"  lAttributes = 0x%08lx\n"
                 L"  status = %d\n",
                 pProp,
                 GUID_PRINTF_ARG( pProp->m_SnapshotSetId ),
                 GUID_PRINTF_ARG( pProp->m_SnapshotId ),
				 pProp->m_pwszOriginalVolumeName,
                 LONGLONG_PRINTF_ARG( pProp->m_tsCreationTimestamp ),
                 pProp->m_lSnapshotAttributes,
				 pProp->m_eStatus);

			// Commit the snapshot, if not failed in pre-commit phase.
			switch(ptrQueuedSnapshot->GetStatus())
			{
			case VSS_SS_PRECOMMITTED:

				// Mark the snapshot as processing commit
				ptrQueuedSnapshot->MarkAsProcessingCommit();

				// Send the IOCTL_VOLSNAP_COMMIT_SNAPSHOT ioctl.
				ptrQueuedSnapshot->CommitSnapshotIoctl();

				// Mark the snapshot as committed
				ptrQueuedSnapshot->MarkAsCommitted();
				break;

			case VSS_SS_COMMITTED:

				// Commit was already done.
				// The provider may receive many CommitSnapshots
				// calls for the same Snapshot Set ID.
				break;

			default:
				BS_ASSERT(false);
				ft.Throw( VSSDBG_SWPRV, E_UNEXPECTED, L"bad state %d", ptrQueuedSnapshot->GetStatus());
			}
        }
    }
    VSS_STANDARD_CATCH(ft)

	return ft.hr;
}


STDMETHODIMP CVsSoftwareProvider::PostCommitSnapshots(
    IN      VSS_ID          SnapshotSetId,
    IN      LONG            lSnapshotsCount
    )

/*++

Description:

	This function gets called by the coordinator as a last phase after commit for all snapshots
	on the given snapshot set

Algorithm:

	For each committed snapshot in this snapshot set:
		1) Call IOCTL_VOLSNAP_END_COMMIT_SNAPSHOT. The purpose of this
			IOCTL is to get the Snapshot Device object name.
		2) Create a unique snapshot ID
		3) Change the state of the snapshot to VSS_SS_CREATED
		4) Set the "number of committed snapshots" attribute of the snapshot set
		5) Save the snapshot properties using the IOCTL_VOLSNAP_SET_APPLICATION_INFO ioctl.
		6) If everything is OK then remove all snapshots from the global list.

	Keep the number of post-committed snapshots.

	If a snapshot fails in operations above then the coordinator is responsible to Abort

Called by:

	IVssCoordinator::DoSnapshotsSet in the third phase (i.e. Commit All Snapshots), after releasing writes
	by Lovelace

Remarks:

	- While calling this, Lovelace is not holding writes anymore.
	- The coordinator may issue many PostCommitSnapshots calls for the same Snapshot Set ID.

Return codes:

    E_ACCESSDENIED
    E_INVALIDARG
    VSS_E_PROVIDER_VETO
        - runtime errors (for example: cannot find diff areas). Logging done.
    VSS_E_OBJECT_NOT_FOUND
        If the internal volume name does not correspond to an existing volume then
        return VSS_E_OBJECT_NOT_FOUND (Bug 227375)
    E_UNEXPECTED
        - dev errors. No logging.
    E_OUTOFMEMORY

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::PostCommitSnapshots" );
	LONG lProcessedSnapshotsCount = 0;

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
				  L"  SnapshotSetId = " WSTR_GUID_FMT L" \n"
				  L"  lSnapshotsCount = %ld",
				  GUID_PRINTF_ARG( SnapshotSetId ),
				  lSnapshotsCount);

        // Argument validation
		if ( SnapshotSetId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotSetId == GUID_NULL");
		if ( lSnapshotsCount < 0 )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"lSnapshotsCount < 0");

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Make sure that the context is frozen
        //
        BS_ASSERT(IsContextFrozen());

		// On each committed snapshot store the lSnapshotsCount
		CVssSnapIterator snapIterator;
        while (true)
        {
			CComPtr<CVssQueuedSnapshot> ptrQueuedSnapshot = snapIterator.GetNextBySnapshotSet(SnapshotSetId);

			// End of enumeration?
			if (ptrQueuedSnapshot == NULL)
				break;

			// Get the snapshot structure
			PVSS_SNAPSHOT_PROP pProp = ptrQueuedSnapshot->GetSnapshotProperties();
			BS_ASSERT(pProp != NULL);

            ft.Trace( VSSDBG_SWPRV, L"Field values for %p: \n"
                 L"  SnapshotSetId = " WSTR_GUID_FMT L"\n"
                 L"  SnapshotId = " WSTR_GUID_FMT L"\n"
                 L"  VolumeName = %s\n"
                 L"  Creation timestamp = " WSTR_LONGLONG_FMT L"\n"
                 L"  lAttributes = 0x%08lx\n"
                 L"  status = %d\n",
                 pProp,
                 GUID_PRINTF_ARG( pProp->m_SnapshotSetId ),
                 GUID_PRINTF_ARG( pProp->m_SnapshotId ),
				 pProp->m_pwszOriginalVolumeName,
                 LONGLONG_PRINTF_ARG( pProp->m_tsCreationTimestamp ),
                 pProp->m_lSnapshotAttributes,
				 pProp->m_eStatus);

			// Get the snapshot volume name and set the snapshot data.
			switch(ptrQueuedSnapshot->GetStatus())
			{
			case VSS_SS_COMMITTED:

				// Mark the snapshot as processing post-commit
				ptrQueuedSnapshot->MarkAsProcessingPostCommit();

				// Remark: the snapshot device name will not be persisted

				// Fill the required properties - BEFORE the snapshot properties are saved!
				ptrQueuedSnapshot->SetPostcommitInfo(lSnapshotsCount);

				// Send the IOCTL_VOLSNAP_END_COMMIT_SNAPSHOT ioctl.
				// Get the snapshot device name
				ptrQueuedSnapshot->EndCommitSnapshotIoctl(pProp);
				ft.Trace( VSSDBG_SWPRV, L"Snapshot created");

				// Increment the number of processed snapshots
				lProcessedSnapshotsCount++;

				// Mark the snapshot as created
				ptrQueuedSnapshot->MarkAsCreated();

				break;
				
			case VSS_SS_CREATED:

				// This snapshot is already created.
				// The provider may receive many PostCommitSnapshots
				// calls for the same Snapshot Set ID.
				break;

			default:
				BS_ASSERT(false);
				ft.Throw( VSSDBG_SWPRV, E_UNEXPECTED, L"bad state %d", ptrQueuedSnapshot->GetStatus());
			}
        } // end while(true)
    }
    VSS_STANDARD_CATCH(ft)

	// If an error occured then the coordinator is responsible to call AbortSnapshots
    return ft.hr;
}

STDMETHODIMP CVsSoftwareProvider::PreFinalCommitSnapshots(
    IN      VSS_ID          SnapshotSetId
    )
{
    UNREFERENCED_PARAMETER(SnapshotSetId);
    
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::PreFinalCommitSnapshots" );

    ft.hr = E_NOTIMPL;
    return ft.hr;
}

STDMETHODIMP CVsSoftwareProvider::PostFinalCommitSnapshots(
    IN      VSS_ID          SnapshotSetId
    )
{
    UNREFERENCED_PARAMETER(SnapshotSetId);
    
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::PostFinalCommitSnapshots" );

    ft.hr = E_NOTIMPL;
    return ft.hr;
}
    
STDMETHODIMP CVsSoftwareProvider::AbortSnapshots(
    IN      VSS_ID          SnapshotSetId
    )

/*++

Description:

	This function gets called by the coordinator as to abort all snapshots from the given snapshot set.
    The snapshots are "reset" to the preparing state, so that a new DoSnapshotSet sequence can start.

Algorithm:

 	For each pre-committed snapshot in this snapshot set calls IOCTL_VOLSNAP_ABORT_PREPARED_SNAPSHOT.
 	For each committed or created snapshot it deletes the snapshot

Called by:

	IVssCoordinator::DoSnapshotsSet to abort precommitted snapshots

Remarks:

	- While calling this, Lovelace is not holding writes on snapshotted volumes.
	- The coordinator may receive many AbortSnapshots calls for the same Snapshot Set ID.

Return codes:

    E_ACCESSDENIED
    E_INVALIDARG
    VSS_E_PROVIDER_VETO
        - runtime errors (for example: cannot find diff areas). Logging done.
    VSS_E_OBJECT_NOT_FOUND
        If the internal volume name does not correspond to an existing volume then
        return VSS_E_OBJECT_NOT_FOUND (Bug 227375)
    E_UNEXPECTED
        - dev errors. No logging.
    E_OUTOFMEMORY

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::AbortSnapshots" );

    HRESULT hIncompleteError = S_OK;	
    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        // Argument validation
		if ( SnapshotSetId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotSetId == GUID_NULL");

		// Trace parameters
        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
			L"  SnapshotSetId = " WSTR_GUID_FMT L"\n",
			GUID_PRINTF_ARG( SnapshotSetId ));

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Make sure that the context is frozen
        //
        BS_ASSERT(IsContextFrozen());

        LONG lProcessedSnapshotsCount = 0;
        CVssSnapIterator snapIterator;
        while (true)
        {
			CComPtr<CVssQueuedSnapshot> ptrQueuedSnapshot = snapIterator.GetNextBySnapshotSet(SnapshotSetId);

			// End of enumeration?
			if (ptrQueuedSnapshot == NULL)
				break;

			// Get the snapshot structure
			PVSS_SNAPSHOT_PROP pProp = ptrQueuedSnapshot->GetSnapshotProperties();
			BS_ASSERT(pProp != NULL);

            ft.Trace( VSSDBG_SWPRV, L"Field values for %p: \n"
                 L"  SnapshotSetId = " WSTR_GUID_FMT L"\n"
                 L"  SnapshotId = " WSTR_GUID_FMT L"\n"
                 L"  VolumeName = %s\n"
                 L"  Creation timestamp = " WSTR_LONGLONG_FMT L"\n"
                 L"  lAttributes = 0x%08lx\n"
                 L"  status = %d\n",
                 pProp,
                 GUID_PRINTF_ARG( pProp->m_SnapshotSetId ),
                 GUID_PRINTF_ARG( pProp->m_SnapshotId ),
				 pProp->m_pwszOriginalVolumeName,
                 LONGLONG_PRINTF_ARG( pProp->m_tsCreationTimestamp ),
                 pProp->m_lSnapshotAttributes,
				 pProp->m_eStatus);

			// Switch the snapshot back to "Preparing" state
			switch(ptrQueuedSnapshot->GetStatus())
			{
			case VSS_SS_PREPARING:
			case VSS_SS_PROCESSING_PREPARE: // Bug 207793

                // Nothing to do.
				break;

			case VSS_SS_PREPARED:
			case VSS_SS_PROCESSING_PRECOMMIT:
			case VSS_SS_PRECOMMITTED:

				// If snapshot was prepared, send IOCTL_VOLSNAP_ABORT_PREPARED_SNAPSHOT.
				ft.hr = ptrQueuedSnapshot->AbortPreparedSnapshotIoctl();
				if (ft.HrFailed())
				{
					ft.Trace( VSSDBG_SWPRV,
                                L"sending IOCTL_VOLSNAP_ABORT_PREPARED_SNAPSHOT failed 0x%08lx", ft.hr);
					ft.hr = S_OK;
					hIncompleteError = S_FALSE;
				}
                break;

			case VSS_SS_PROCESSING_COMMIT:
			case VSS_SS_COMMITTED:
			case VSS_SS_PROCESSING_POSTCOMMIT:
			case VSS_SS_CREATED:

			    try
                {
    				// If snapshot was committed, delete the snapshot
    				LONG lDeletedSnapshots = 0;
    				VSS_ID NondeletedSnapshotID = GUID_NULL;
    				ft.hr = CVsSoftwareProvider::InternalDeleteSnapshots(pProp->m_SnapshotId,
    				            VSS_OBJECT_SNAPSHOT,
            					GetContextInternal(),
    				            &lDeletedSnapshots,
    				            &NondeletedSnapshotID);
    				if (ft.HrFailed())
    				{
    					ft.Trace( VSSDBG_SWPRV,
                                    L"InternalDeleteSnapshots failed (%ld) " WSTR_GUID_FMT L"0x%08lx",
                                    lDeletedSnapshots,
                                    GUID_PRINTF_ARG(NondeletedSnapshotID),
                                    ft.hr);
    					ft.hr = S_OK;
    					hIncompleteError = S_FALSE;
    				}
    			}
			    VSS_STANDARD_CATCH(ft)
			
                break;

			default:
				BS_ASSERT(false);
			}

            // Reset the snapshot as preparing
            ptrQueuedSnapshot->ResetAsPreparing();

			lProcessedSnapshotsCount++;
        }

        ft.Trace( VSSDBG_SWPRV, L"%ld snapshots were aborted", lProcessedSnapshotsCount);
    }
    VSS_STANDARD_CATCH(ft)

    if (SUCCEEDED(ft.hr))
    	ft.hr = hIncompleteError;
    
    return ft.hr;
}


STDMETHODIMP CVsSoftwareProvider::GetSnapshotProperties(
    IN      VSS_ID          SnapshotId,
    OUT     PVSS_SNAPSHOT_PROP  pSavedProp
    )
/*++

Routine description:

    Implements IVssSoftwareSnapshotProvider::GetSnapshotProperties

Throws:

    E_OUTOFMEMORY
    E_INVALIDARG
    E_UNEXPECTED
        - Dev error. No logging.
    E_ACCESSDENIED
        - The user is not a backup operator or administrator

    [FindPersistedSnapshotByID() failures]
        E_OUTOFMEMORY

        [EnumerateSnapshots() failures]
            VSS_E_PROVIDER_VETO
                - On runtime errors (like Unpack)
            E_OUTOFMEMORY

--*/
{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVssSoftwareProvider::GetSnapshotProperties" );

    try
    {
        // Initialize [out] arguments
        VssZeroOut( pSavedProp );

        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        // Trace parameters
        ft.Trace( VSSDBG_SWPRV, L"Parameters: pSavedProp = %p", pSavedProp );

        // Argument validation
		BS_ASSERT(pSavedProp);
		if ( SnapshotId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotId == GUID_NULL");
        if (pSavedProp == NULL)
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"NULL pSavedProp");

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        // Create the collection object. Initial reference count is 0.
        VSS_OBJECT_PROP_Array* pArray = new VSS_OBJECT_PROP_Array;
        if (pArray == NULL)
            ft.Throw( VSSDBG_SWPRV, E_OUTOFMEMORY, L"Memory allocation error.");

        // Get the pointer to the IUnknown interface.
		// The only purpose of this is to use a smart ptr to destroy correctly the array on error.
		// Now pArray's reference count becomes 1 (because of the smart pointer).
        CComPtr<IUnknown> pArrayItf = static_cast<IUnknown*>(pArray);
        BS_ASSERT(pArrayItf);

		// Get the list of snapshots in the give array
		CVssQueuedSnapshot::EnumerateSnapshots( true, SnapshotId, GetContextInternal(), pArray);

        // Extract the element from the array.
        if (pArray->GetSize() == 0)
            ft.Throw( VSSDBG_SWPRV, VSS_E_OBJECT_NOT_FOUND, L"Snapshot not found.");

        // Get the snapshot structure
    	VSS_OBJECT_PROP_Ptr& ptrObj = (*pArray)[0];
    	VSS_OBJECT_PROP* pObj = ptrObj.GetStruct();
    	BS_ASSERT(pObj);
    	BS_ASSERT(pObj->Type == VSS_OBJECT_SNAPSHOT);
    	VSS_SNAPSHOT_PROP* pSnap = &(pObj->Obj.Snap);

        // Fill out the [out] parameter
        VSS_OBJECT_PROP_Copy::copySnapshot(pSavedProp, pSnap);
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}


STDMETHODIMP CVsSoftwareProvider::SetSnapshotProperty(
	IN   VSS_ID  			SnapshotId,
	IN   VSS_SNAPSHOT_PROPERTY_ID	eSnapshotPropertyId,
	IN   VARIANT 			vProperty
	)
/*++

Routine description:

    Implements IVssSoftwareSnapshotProvider::SetSnapshotProperty


--*/
{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVssSoftwareProvider::SetSnapshotProperty" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        // Argument checking
		if ( SnapshotId == GUID_NULL )
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"SnapshotId == GUID_NULL");

        // Further argument checking
        CComVariant value = vProperty;
        switch(eSnapshotPropertyId)
        {
            case VSS_SPROPID_SNAPSHOT_ATTRIBUTES:
                if (value.vt != VT_I4)
                {
                    BS_ASSERT(false); // The coordinator must give us the right data
                    ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Invalid variant %ul for property %d", 
                        value.vt, eSnapshotPropertyId);
                }
                
                // Trace parameters
                ft.Trace( VSSDBG_SWPRV, L"Parameters: SnapshotId: " WSTR_GUID_FMT 
                    L", eSnapshotPropertyId = %d, vProperty = 0x%08lx" , 
                    GUID_PRINTF_ARG(SnapshotId), eSnapshotPropertyId, value.lVal );
                break;
            case VSS_SPROPID_EXPOSED_NAME:
            case VSS_SPROPID_EXPOSED_PATH:
            case VSS_SPROPID_SERVICE_MACHINE:
                if (value.vt != VT_BSTR)
                {
                    BS_ASSERT(false); // The coordinator must give us the right data
                    ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Invalid variant %ul for property %d", 
                        value.vt, eSnapshotPropertyId);
                }

                // Trace parameters
                ft.Trace( VSSDBG_SWPRV, L"Parameters: SnapshotId: " WSTR_GUID_FMT 
                    L", eSnapshotPropertyId = %d, vProperty = %s" ,
                    GUID_PRINTF_ARG(SnapshotId), eSnapshotPropertyId, value.bstrVal );

                // We should not allow empty strings
                if (value.bstrVal == NULL)
                    ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"NULL exposed name/path", value.bstrVal);
                
                break;
            default:
                ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Invalid property %d", eSnapshotPropertyId);
        }

        // Load the snapshot properties
        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        // Create the collection object. Initial reference count is 0.
        VSS_OBJECT_PROP_Array* pArray = new VSS_OBJECT_PROP_Array;
        if (pArray == NULL)
            ft.Throw( VSSDBG_SWPRV, E_OUTOFMEMORY, L"Memory allocation error.");

        // Get the pointer to the IUnknown interface.
		// The only purpose of this is to use a smart ptr to destroy correctly the array on error.
		// Now pArray's reference count becomes 1 (because of the smart pointer).
        CComPtr<IUnknown> pArrayItf = static_cast<IUnknown*>(pArray);
        BS_ASSERT(pArrayItf);

		// Get the list of snapshots in the give array
		CVssQueuedSnapshot::EnumerateSnapshots( true, SnapshotId, GetContextInternal(), pArray);

        // Extract the element from the array. Throw if snapshot not found.
        if (pArray->GetSize() == 0)
            ft.Throw( VSSDBG_SWPRV, VSS_E_OBJECT_NOT_FOUND, L"Snapshot not found.");

        // Get the snapshot structure
    	VSS_OBJECT_PROP_Ptr& ptrObj = (*pArray)[0];
    	VSS_OBJECT_PROP* pObj = ptrObj.GetStruct();
    	BS_ASSERT(pObj);
    	BS_ASSERT(pObj->Type == VSS_OBJECT_SNAPSHOT);
    	VSS_SNAPSHOT_PROP* pSnap = &(pObj->Obj.Snap);

    	// Set the member in the structure
        switch(eSnapshotPropertyId)
        {
            case VSS_SPROPID_SNAPSHOT_ATTRIBUTES:
                // The attributes must be applied to the snapshot attributes
                // Currently, the only attributes that can be changed here are the exposure attributes
                BS_ASSERT(value.vt == VT_I4);
                if ((((LONG)value.lVal ^ pSnap->m_lSnapshotAttributes) & ~x_lInputAttributes) !=0)
                	ft.Throw (VSSDBG_SWPRV, E_INVALIDARG, L"Invalid attributes 0x%08lx, previous attributes are 0x%08lx", (LONG)value.lVal,
                																					pSnap->m_lSnapshotAttributes);
                
                pSnap->m_lSnapshotAttributes = (LONG)value.lVal;
                break;

            case VSS_SPROPID_EXPOSED_NAME:
                // Change the exposed name
                BS_ASSERT(value.vt == VT_BSTR);
                ::VssFreeString(pSnap->m_pwszExposedName);
                ::VssSafeDuplicateStr(ft, pSnap->m_pwszExposedName, value.bstrVal);
                break;

            case VSS_SPROPID_EXPOSED_PATH:
                // Change the exposed path
                BS_ASSERT(value.vt == VT_BSTR);
                ::VssFreeString(pSnap->m_pwszExposedPath);
                ::VssSafeDuplicateStr(ft, pSnap->m_pwszExposedPath, value.bstrVal);
                break;

            case VSS_SPROPID_SERVICE_MACHINE:
                // Change the service machine
                BS_ASSERT(value.vt == VT_BSTR);
                ::VssFreeString(pSnap->m_pwszServiceMachine);
                ::VssSafeDuplicateStr(ft, pSnap->m_pwszServiceMachine, value.bstrVal);
                break;

            default:
                BS_ASSERT(false);
                ft.Throw( VSSDBG_SWPRV, E_UNEXPECTED, L"Invalid property %d", eSnapshotPropertyId);
        }

        // Open a IOCTL channel to the snapshot
        BS_ASSERT(pSnap->m_pwszSnapshotDeviceObject);
    	CVssIOCTLChannel snapshotIChannel;	
    	snapshotIChannel.Open(ft, pSnap->m_pwszSnapshotDeviceObject, false, true, VSS_ICHANNEL_LOG_PROV);

    	// Save the snapshot properties
    	LONG lStructureContext = pSnap->m_lSnapshotAttributes & ~ x_lNonCtxAttributes;
    	CVssQueuedSnapshot::SaveStructure(snapshotIChannel, pSnap, lStructureContext, false);

    	// Send the IOCTL
    	snapshotIChannel.Call(ft, IOCTL_VOLSNAP_SET_APPLICATION_INFO, true, VSS_ICHANNEL_LOG_PROV);
    }
    VSS_STANDARD_CATCH(ft)
    return ft.hr;
}

STDMETHODIMP CVsSoftwareProvider::RevertToSnapshot(
   IN       VSS_ID              SnapshotId
)
{
    UNREFERENCED_PARAMETER(SnapshotId);
    
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::RevertToSnapshot");

    ft.hr = E_NOTIMPL;
    return ft.hr;
}

STDMETHODIMP CVsSoftwareProvider::QueryRevertStatus(
   IN      VSS_PWSZ                         pwszVolume,
   OUT    IVssAsync**                  ppAsync
 )
{
    UNREFERENCED_PARAMETER(pwszVolume);
    UNREFERENCED_PARAMETER(ppAsync);
    
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::QueryRevertStatus");

    ft.hr = E_NOTIMPL;
    return ft.hr;
}

void CVsSoftwareProvider::RemoveSnapshotsFromGlobalList(
	IN	VSS_ID FilterID,
	IN	VSS_ID CurrentSnapshotSetID,
    IN  VSS_QSNAP_REMOVE_TYPE eRemoveType
	) throw(HRESULT)

/*++

Description:

	Detach from the global list all snapshots in this snapshot set

Remark:

	We detach all snapshots at once only in case of total success or total failure.
	This is because we want to be able to retry DoSnapshotSet if a failure happens.
	Therefore we must keep the list of snapshots as long as the client wants.

    VSS_QST_REMOVE_SPECIFIC_QS,  // Remove the remaining specific QS     (called in Provider itf. destructor)
    VSS_QST_REMOVE_ALL_QS,       // Remove all remaining QS              (called in OnUnload)

Called by:

	PostCommitSnapshots, AbortSnapshots, destructor and OnUnload

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::RemoveSnapshotsFromGlobalList" );
		
    ft.Trace( VSSDBG_SWPRV, L"FilterId = " WSTR_GUID_FMT L"CurrentSSID = " WSTR_GUID_FMT L"eRemoveType = %d",
                            GUID_PRINTF_ARG(FilterID),GUID_PRINTF_ARG(CurrentSnapshotSetID),eRemoveType );

	// For each snapshot in the snapshot set...
	LONG lProcessedSnapshotsCount = 0;
	CVssSnapIterator snapIterator;
    while (true)
    {
        CComPtr<CVssQueuedSnapshot> ptrQueuedSnapshot;

        // Check if we need to return all snapshots (OnUnload case)
        switch( eRemoveType ) {
        case VSS_QST_REMOVE_ALL_QS:
            BS_ASSERT( FilterID == GUID_NULL )
	        BS_ASSERT(CurrentSnapshotSetID == GUID_NULL);
		    ptrQueuedSnapshot = snapIterator.GetNext();
            break;
        case VSS_QST_REMOVE_SPECIFIC_QS:
	        BS_ASSERT(FilterID != GUID_NULL);
	        BS_ASSERT(CurrentSnapshotSetID == GUID_NULL);
		    ptrQueuedSnapshot = snapIterator.GetNextByProviderInstance(FilterID);
            break;
        case VSS_QST_REMOVE_ALL_NON_AUTORELEASE:
	        BS_ASSERT(FilterID != GUID_NULL);
	        BS_ASSERT(CurrentSnapshotSetID != GUID_NULL);
		    ptrQueuedSnapshot = snapIterator.GetNextByProviderInstance(FilterID);
            // BUG 385538: This is a snapshot that belongs to the same snapshot set. Ignore it.
	        if (ptrQueuedSnapshot && (CurrentSnapshotSetID == ptrQueuedSnapshot->GetSnapshotSetID()))
	            continue;
            // If we are trying to remove the non-autodelete qs then ignore the autodelete ones.
            // These ones will be removed when the interface is released
            if (ptrQueuedSnapshot &&
                (0 == (ptrQueuedSnapshot->GetContextInternal() & VSS_VOLSNAP_ATTR_NO_AUTO_RELEASE)))
            {
                // This is an auto-release snapshot.
                // Since the current context didn't change, it should also refer to autorelease snapshots
		        continue;
            }
            break;
        default:
            BS_ASSERT(false);
            ptrQueuedSnapshot = NULL;
        }

		// End of enumeration?
		if (ptrQueuedSnapshot == NULL)
			break;

		// Get the snapshot structure
		PVSS_SNAPSHOT_PROP pProp = ptrQueuedSnapshot->GetSnapshotProperties();
		BS_ASSERT(pProp != NULL);

        ft.Trace( VSSDBG_SWPRV, L"Field values for %p: \n"
             L" *ProvInstanceId = " WSTR_GUID_FMT L"\n"
             L"  SnapshotId = " WSTR_GUID_FMT L"\n"
             L"  SnapshotSetId = " WSTR_GUID_FMT L"\n"
             L"  VolumeName = %s\n"
             L"  Creation timestamp = " WSTR_LONGLONG_FMT L"\n"
             L"  lAttributes = 0x%08lx\n"
             L"  status = %d\n",
             pProp,
             GUID_PRINTF_ARG( ptrQueuedSnapshot->GetProviderInstanceId() ),
             GUID_PRINTF_ARG( pProp->m_SnapshotSetId ),
             GUID_PRINTF_ARG( pProp->m_SnapshotId ),
			 pProp->m_pwszOriginalVolumeName,
             LONGLONG_PRINTF_ARG( pProp->m_tsCreationTimestamp ),
             pProp->m_lSnapshotAttributes,
			 pProp->m_eStatus);

        // The destructor (and the autodelete stuff) is called here.
		ptrQueuedSnapshot->DetachFromGlobalList();
		lProcessedSnapshotsCount++;
	}

	ft.Trace( VSSDBG_SWPRV, L" %ld snapshots were detached", lProcessedSnapshotsCount);
}


STDMETHODIMP CVsSoftwareProvider::OnLoad(
	IN  	IUnknown* pCallback	
    )
{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::OnLoad" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED, L"Access denied");

        try
        {
            CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

            // Purge all hidden snapshots that may become available for delete
            PurgeSnapshots(true);
        }
        VSS_STANDARD_CATCH(ft)

        // Log warning and ignore error code
        // (bug 406920	CVsSoftwareProvider::OnLoad fails if drive goes away)
        if (ft.HrFailed())
            ft.LogGenericWarning(VSSDBG_SWPRV, L"PurgeSnapshots(true)");
    }
    VSS_STANDARD_CATCH(ft)

    return S_OK;
    UNREFERENCED_PARAMETER(pCallback);
}


STDMETHODIMP CVsSoftwareProvider::OnUnload(
	IN  	BOOL	bForceUnload				
    )
{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::OnUnload" );

    try
    {
        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED, L"Access denied");

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        // Remove all snapshots that belong to all provider instance IDs
    	RemoveSnapshotsFromGlobalList(GUID_NULL, GUID_NULL, VSS_QST_REMOVE_ALL_QS);
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    UNREFERENCED_PARAMETER(bForceUnload);
}


STDMETHODIMP CVsSoftwareProvider::IsVolumeSupported(
    IN      VSS_PWSZ        pwszVolumeName,
    OUT     BOOL *          pbSupportedByThisProvider
    )

/*++

Description:

    This call is used to check if a volume can be snapshotted or not by the
    corresponding provider.

Parameters
    pwszVolumeName
        [in] The volume name to be checked. It must be one of those returned by
        GetVolumeNameForVolumeMountPoint, in other words in
        the \\?\Volume{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}\ format
        with the corresponding unique ID.(with trailing backslash)
    pbSupportedByThisProvider
        [out] Non-NULL pointer that receives TRUE if the volume can be
        snapshotted using this provider or FALSE otherwise.

Return codes
    S_OK
        The function completed with success
    E_ACCESSDENIED
        The user is not an administrator.
    E_INVALIDARG
        NULL pointers passed as parameters or a volume name in an invalid format.
    E_OUTOFMEMORY
        Out of memory or other system resources
    E_UNEXPECTED
        Unexpected programming error. Logging not done and not needed.
    VSS_E_PROVIDER_VETO
        An error occured while opening the IOCTL channel. The error is logged.

    [CVssSoftwareProvider::GetVolumeInformation]
        E_OUTOFMEMORY
        VSS_E_PROVIDER_VETO
            An error occured while opening the IOCTL channel. The error is logged.
        E_UNEXPECTED
            Unexpected programming error. Nothing is logged.
        VSS_E_OBJECT_NOT_FOUND
            The device does not exist or it is not ready.


Remarks
    The function will return TRUE in the pbSupportedByThisProvider
    parameter if it is possible to create a snapshot on the given volume.
    The function must return TRUE on that volume even if the current
    configuration does not allow the creation of a snapshot on that volume.
    For example, if the maximum number of snapshots were reached on the
    given volume (and therefore no more snapshots can be created on that volume),
    the method must still indicate that the volume can be snapshotted.

--*/


{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::IsVolumeSupported" );

    try
    {
        ::VssZeroOut(pbSupportedByThisProvider);

        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
             L"  pwszVolumeName = %p\n"
             L"  pbSupportedByThisProvider = %p\n",
             pwszVolumeName,
             pbSupportedByThisProvider);

        // Argument validation
        if ( (pwszVolumeName == NULL) || (wcslen(pwszVolumeName) == 0))
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"pwszVolumeName is NULL");
        if (pbSupportedByThisProvider == NULL)
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Invalid bool");

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Freeze the context
        //
        FreezeContext();

        // Get volume information. This may throw.
        LONG lVolAttr = GetVolumeInformationFlags(pwszVolumeName, GetContextInternal());
        (*pbSupportedByThisProvider) = ((lVolAttr & VSS_VOLATTR_SUPPORTED_FOR_SNAPSHOT) != 0);
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}




STDMETHODIMP CVsSoftwareProvider::IsVolumeSnapshotted(
    IN      VSS_PWSZ        pwszVolumeName,
    OUT     BOOL *          pbSnapshotsPresent,
	OUT 	LONG *		    plSnapshotCompatibility
    )

/*++

Description:

    This call is used to check if a volume can be snapshotted or not by the
    corresponding provider.

Parameters
    pwszVolumeName
        [in] The volume name to be checked. It must be one of those returned by
        GetVolumeNameForVolumeMountPoint, in other words in
        the \\?\Volume{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}\ format
        with the corresponding unique ID.(with trailing backslash)
    pbSnapshotPresent
        [out] Non-NULL pointer that receives TRUE if the volume has at least
        one snapshot or FALSE otherwise.
    plSnapshotCompatibility
        [out] Flags denoting the compatibility of the snapshotted volume with various operations

Return codes
    S_OK
        The function completed with success
    E_ACCESSDENIED
        The user is not an administrator.
    E_INVALIDARG
        NULL pointers passed as parameters or a volume name in an invalid format.
    E_OUTOFMEMORY
        Out of memory or other system resources
    E_UNEXPECTED
        Unexpected programming error. Logging not done and not needed.
    VSS_E_PROVIDER_VETO
        An error occured while opening the IOCTL channel. The error is logged.

    [CVssSoftwareProvider::GetVolumeInformation]
        E_OUTOFMEMORY
        VSS_E_PROVIDER_VETO
            An error occured while opening the IOCTL channel. The error is logged.
        E_UNEXPECTED
            Unexpected programming error. Nothing is logged.
        VSS_E_OBJECT_NOT_FOUND
            The device does not exist or it is not ready.


Remarks
    The function will return S_OK even if the current volume is a non-supported one.
    In this case FALSE must be returned in the pbSnapshotPresent parameter.

--*/


{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::IsVolumeSnapshotted" );

    try
    {
        ::VssZeroOut(pbSnapshotsPresent);

        // Access check
        if (!IsAdministrator())
            ft.Throw( VSSDBG_SWPRV, E_ACCESSDENIED,
                L"The client process is not running under an administrator account");

        ft.Trace( VSSDBG_SWPRV, L"Parameters: \n"
             L"  pwszVolumeName = %p\n"
             L"  pbSnapshotsPresent = %p\n"
             L"  plSnapshotCompatibility = %p\n",
             pwszVolumeName,
             pbSnapshotsPresent,
             plSnapshotCompatibility);

        // Argument validation
        if ( (pwszVolumeName == NULL) || (wcslen(pwszVolumeName) == 0))
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"pwszVolumeName is NULL");
        if (pbSnapshotsPresent == NULL)
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Invalid bool");
        if (plSnapshotCompatibility == NULL)
            ft.Throw( VSSDBG_SWPRV, E_INVALIDARG, L"Invalid plSnapshotCompatibility");

        CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());

        //
        //  Freeze the context
        //
        FreezeContext();

        // Get volume information. This may throw,
        LONG lVolAttr = GetVolumeInformationFlags(pwszVolumeName, GetContextInternal());

        // Bug 500069: Allow DEFRAG on Babbage snapshotted volumes
        (*pbSnapshotsPresent) = ((lVolAttr & VSS_VOLATTR_SNAPSHOTTED) != 0);
        (*plSnapshotCompatibility) = ((lVolAttr & VSS_VOLATTR_SNAPSHOTTED) != 0)?
            (/*VSS_SC_DISABLE_DEFRAG|*/VSS_SC_DISABLE_CONTENTINDEX): 0 ;
    }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
}


LONG CVsSoftwareProvider::GetVolumeInformationFlags(
    IN  LPCWSTR pwszVolumeName,
    IN  LONG    lContext,
    IN OUT CVssAutoPWSZ * pawszLastSnapshotName,  /* = NULL */
    IN  BOOL bThrowIfError                        /* = TRUE */
    ) throw(HRESULT)

/*++

Description:

    This function returns various attributes that describe
        - if the volume is supported by this provider
        - if the volume has snapshots.

Parameter:

    [in] The volume name to be checked. It must be one of those returned by
    GetVolumeNameForVolumeMountPoint, in other words in
    the \\?\Volume{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}\ format
    with the corresponding unique ID.(with trailing backslash)

    [in] The context.

Return values:

    A combination of _VSS_VOLUME_INFORMATION_ATTR flags.

    If pwszLastSnapshotName is not NULL, return the name of the last snapshot

Throws:

    E_OUTOFMEMORY
    VSS_E_PROVIDER_VETO
        An error occured while opening the IOCTL channel. The error is logged.
    E_UNEXPECTED
        Unexpected programming error. Nothing is logged.
    VSS_E_OBJECT_NOT_FOUND
        The device does not exist or it is not ready.

--*/

{
    CVssFunctionTracer ft( VSSDBG_SWPRV, L"CVsSoftwareProvider::GetVolumeInformationFlags" );
	CVssIOCTLChannel volumeIChannel;	// For checking the snapshots on a volume
    LONG lReturnedFlags = 0;

    // Argument validation
    BS_ASSERT(pwszVolumeName);
   	BS_ASSERT(::IsVolMgmtVolumeName( pwszVolumeName ));

    // Check if the volume is fixed (i.e. no CD-ROM, no removable)
    UINT uDriveType = ::GetDriveTypeW(pwszVolumeName);
    if ( uDriveType != DRIVE_FIXED) {
        ft.Trace( VSSDBG_SWPRV, L"Warning: Ignoring the non-fixed volume (%s) - %ud",
                  pwszVolumeName, uDriveType);
        return lReturnedFlags;
    }

    //
    //  Static member to keep the quorum volume (and not to recompute it each time)
    //
    //  NOTE: 
    //      We presume that the quorum volume doesn't change for the lifetime of this process, 
    //      so we don't need to recompute it each time (that would be inefficient)
    //
    static CVssClusterQuorumVolume objQuorumVolumeHolder;
    bool bIsQuorum = objQuorumVolumeHolder.IsQuorumVolume(pwszVolumeName);

    if (bIsQuorum)
        lReturnedFlags |= VSS_VOLATTR_IS_QUORUM;

    // Check if the volume is NTFS
    DWORD dwFileSystemFlags = 0;
    WCHAR wszFileSystemNameBuffer[MAX_PATH+1];
    wszFileSystemNameBuffer[0] = L'\0';
    if (!::GetVolumeInformationW(pwszVolumeName,
            NULL,   // lpVolumeNameBuffer
            0,      // nVolumeNameSize
            NULL,   // lpVolumeSerialNumber
            NULL,   // lpMaximumComponentLength
            &dwFileSystemFlags,
            wszFileSystemNameBuffer,
            MAX_PATH
            ))
    {
        DWORD dwLastErr =  GetLastError();
        ft.Trace( VSSDBG_SWPRV,
                  L"Warning: Error calling GetVolumeInformation on volume '%s' 0x%08lx",
                  pwszVolumeName, dwLastErr);

        // Check if the volume was found
        if ((dwLastErr == ERROR_NOT_READY) ||
            (dwLastErr == ERROR_DEVICE_NOT_CONNECTED)) {
            if (bThrowIfError)
                ft.Throw( VSSDBG_SWPRV, VSS_E_OBJECT_NOT_FOUND,
                      L"Volume not found: error calling GetVolumeInformation on volume '%s' : %u",
                      pwszVolumeName, dwLastErr );
            else
                return lReturnedFlags;
        }

        // Eliminate the cases when GetVolumeInformationW failed from other reasons
        if (dwLastErr != ERROR_UNRECOGNIZED_VOLUME) {
            // Log an warning. 
            // Make sure that GetVolumeInformationW doesn't fail here for obvious reasons.
            wszFileSystemNameBuffer[MAX_PATH]=L'\0';
            ft.LogGenericWarning(VSSDBG_SWPRV, 
                L"GetVolumeInformationW(%s,NULL,0,NULL,NULL,[0x%08lx], %s, %ld) == 0x%08lx", 
                    pwszVolumeName, dwFileSystemFlags, wszFileSystemNameBuffer, (LONG)MAX_PATH, dwLastErr);
            return lReturnedFlags;
        }

        // RAW volumes (error ERROR_UNRECOGNIZED_VOLUME) are supported by VSS (bug 209059, 457354)
        lReturnedFlags |= VSS_VOLATTR_VOLUME_IS_RAW;

        if ( (lContext != VSS_CTX_ALL) && (
             (lContext & VSS_VOLSNAP_ATTR_CLIENT_ACCESSIBLE) ||
             (lContext & VSS_VOLSNAP_ATTR_PERSISTENT) )
            ) {
            // ...but they are not supported for timewarp nor persistent snapshots (bug 504783)
            ft.Trace( VSSDBG_SWPRV, L"Encountered a RAW volume (%s)", pwszVolumeName);
            return lReturnedFlags;
        }
    }
    else
    {
        // We do not support snapshots on read-only volumes
        if (dwFileSystemFlags & FILE_READ_ONLY_VOLUME) {
            ft.Trace( VSSDBG_SWPRV, L"Encountered a read-only volume (%s)", pwszVolumeName);
            return lReturnedFlags;
        }

        // If the file system is NTFS then mark the volume as supported for diff area
        if (::wcscmp(wszFileSystemNameBuffer, wszFileSystemNameNTFS) == 0) {
            // Check first that the volume is large enough
            ULARGE_INTEGER ulnNotUsed1;
            ULARGE_INTEGER ulnTotalNumberOfBytes;
            ULARGE_INTEGER ulnNotUsed2;
            ulnTotalNumberOfBytes.QuadPart = 0;

            // Mark the volume as supported since it is NTFS and large enough
            if (::GetDiskFreeSpaceEx(pwszVolumeName,
                    &ulnNotUsed1,
                    &ulnTotalNumberOfBytes,
                    &ulnNotUsed2
                    )) {
                ft.Trace(VSSDBG_SWPRV, L"RANK: total=%I64d max=%I64d",
                     ulnTotalNumberOfBytes.QuadPart, (LONGLONG)x_nDefaultInitialSnapshotAllocation);
                if (ulnTotalNumberOfBytes.QuadPart >= (LONGLONG)x_nDefaultInitialSnapshotAllocation)
                {
                    // Supports diff area only if this is not the quorum volume (bug# 661583)
                    if (!bIsQuorum)
            	        lReturnedFlags |= VSS_VOLATTR_SUPPORTED_FOR_DIFF_AREA;
                }

            } else {
                DWORD dwLastErr = GetLastError();
                ft.Trace( VSSDBG_SWPRV, L"Cannot get the free space for volume (%s) - [0x%08lx]",
                          pwszVolumeName, dwLastErr);
                ft.LogGenericWarning(VSSDBG_SWPRV, 
                    L"GetDiskFreeSpaceEx() for %s failed with 0x%08lx", pwszVolumeName, dwLastErr);
            }
        }
        else if ( (lContext != VSS_CTX_ALL) && (
            (lContext & VSS_VOLSNAP_ATTR_CLIENT_ACCESSIBLE) ||
            (lContext & VSS_VOLSNAP_ATTR_PERSISTENT) )
            )
        {
            // For timewarp & persistent context do not support FAT
            ft.Trace( VSSDBG_SWPRV, L"Encountered a non-NTFS volume (%s) - %s",
                      pwszVolumeName, wszFileSystemNameBuffer);
            return lReturnedFlags;
        }
    }

    try {
        // Open the volume. Throw "object not found" if needed.
	    volumeIChannel.Open(ft, pwszVolumeName, true, true, VSS_ICHANNEL_LOG_PROV, 0);

        // Check to see if there are existing snapshots
	    ft.hr = volumeIChannel.Call(ft, IOCTL_VOLSNAP_QUERY_NAMES_OF_SNAPSHOTS);
        if (ft.HrFailed()) {
            // The volume is not even supported
            lReturnedFlags = 0;
            ft.Throw( VSSDBG_SWPRV, S_OK, 
                L"Volume is not supported by volsnap driver. Call returned 0x%08lx", ft.hr);
        }

	    // Mark the volume as supported since the query succeeded.
	    // But do not mark the quorum volume as supported for Timewarp.
        if ( !(bIsQuorum 
                && ((lContext & VSS_VOLSNAP_ATTR_CLIENT_ACCESSIBLE) != 0) 
                && (lContext != VSS_CTX_ALL)))
    	    lReturnedFlags |= VSS_VOLATTR_SUPPORTED_FOR_SNAPSHOT;

	    // Get the length of snapshot names multistring
	    ULONG ulMultiszLen;
	    volumeIChannel.Unpack(ft, &ulMultiszLen);

        // If the multistring is empty, then ulMultiszLen is necesarily 2
        // (i.e. two l"\0' characters)
        // Then mark the volume as snapshotted.
	    if (ulMultiszLen != x_nEmptyVssMultiszLen)
	    {
	        lReturnedFlags |= VSS_VOLATTR_SNAPSHOTTED;

            // Return the last snapshot if needed
       	    if (pawszLastSnapshotName)
       	    {
       	        CVssAutoPWSZ awszSnapshotName;
        	    while(volumeIChannel.UnpackZeroString(ft, awszSnapshotName.GetRef()))
        	        pawszLastSnapshotName->TransferFrom(awszSnapshotName);
       	    }
	    }
    }
    VSS_STANDARD_CATCH(ft);

    if (FAILED(ft.hr)  && bThrowIfError)
        ft.Throw( VSSDBG_SWPRV, ft.hr, L"Rethrowing");

    return lReturnedFlags;
}



CVsSoftwareProvider::CVsSoftwareProvider():
    m_bContextFrozen(false),
    m_lSnapContext(VSS_CTX_BACKUP),
    m_dwMaxCAShadowCopies(x_MaxCAShadowCopiesDefault)
{
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::CVsSoftwareProvider");

    // Create the provider instance Id (which is used to mark the queued snapshots that belongs to it)
    ft.hr = ::CoCreateGuid(&m_ProviderInstanceID);
    if (ft.HrFailed()) {
        m_ProviderInstanceID = GUID_NULL;
        ft.Trace( VSSDBG_SWPRV, L"CoCreateGuid failed 0x%08lx", ft.hr );
        // TBD: Add event log here.
    }
    else {
        BS_ASSERT(m_ProviderInstanceID != GUID_NULL);
    }

    // Try to read the "max shadow copies" registry key
    try
    {
        CVssRegistryKey keyMaxCAShadowCopies(KEY_READ);
        if (keyMaxCAShadowCopies.Open(HKEY_LOCAL_MACHINE, x_wszVssCASettingsPath))
        {
            DWORD dwMaxCAShadowCopies = 0;
            if (keyMaxCAShadowCopies.GetValue(x_wszVssCAMaxShadowCopiesValueName, 
                    dwMaxCAShadowCopies, false))
            {
                // Zero means "no limit"
                if (dwMaxCAShadowCopies == 0)
                    dwMaxCAShadowCopies = MAXDWORD;

                m_dwMaxCAShadowCopies = dwMaxCAShadowCopies;
            }
        }
    }
    VSS_STANDARD_CATCH(ft)

    // Ignore the falures - logging is already done
}


CVsSoftwareProvider::~CVsSoftwareProvider()
{
    CVssFunctionTracer ft(VSSDBG_SWPRV, L"CVsSoftwareProvider::~CVsSoftwareProvider");

    // Lock in order to update both variables atomically
    // The critical section will be left automatically at the end of scope.
    CVssAutomaticLock2 lock(CVsSoftwareProvider::GetGlobalCS());


    // Remove here all [Auto-Delete] queued snapshots that belong to this particular
    // Provider Instance ID.
    // The volume handle gets closed here and this will delete the underlying snapshots.
    if (m_ProviderInstanceID != GUID_NULL)
        RemoveSnapshotsFromGlobalList(m_ProviderInstanceID, GUID_NULL, VSS_QST_REMOVE_SPECIFIC_QS);
    else {
        BS_ASSERT(false);
    }
}



