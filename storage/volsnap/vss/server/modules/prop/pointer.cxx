/*++

Copyright (c) 1999  Microsoft Corporation

Abstract:

    @doc
    @module Pointer.cxx | Implementation of VSS_OBJECT_PROP_Ptr class
    @end

Author:

    Adi Oltean  [aoltean]  09/21/1999

Revision History:

    Name        Date        Comments

    aoltean     09/21/1999	VSS_OBJECT_PROP_Ptr as a pointer to the properties structure.
							This pointer will serve as element in CSimpleArray constructs.
	aoltean		09/22/1999	Adding InitializeAsEmpty and Print
	aoltean		09/24/1999	Moving into modules/prop
	aoltean		03/27/2000	Adding Writers
	aoltean		03/01/2001	Adding mgmt objects

--*/


/////////////////////////////////////////////////////////////////////////////
//  Needed includes


#include "stdafx.hxx"

#include "vs_idl.hxx"
#include "vs_inc.hxx"

#include "copy.hxx"	
#include "pointer.hxx"	

////////////////////////////////////////////////////////////////////////
//  Standard foo for file name aliasing.  This code block must be after
//  all includes of VSS header files.
//
#ifdef VSS_FILE_ALIAS
#undef VSS_FILE_ALIAS
#endif
#define VSS_FILE_ALIAS "PRPPNTRC"
//
////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  VSS_OBJECT_PROP_Ptr class


void VSS_OBJECT_PROP_Ptr::InitializeAsSnapshot(
    IN  CVssFunctionTracer& ft,
	IN  VSS_ID SnapshotId,
	IN  VSS_ID SnapshotSetId,
	IN  LONG lSnapshotsCount,
	IN  VSS_PWSZ pwszSnapshotDeviceObject,
	IN  VSS_PWSZ pwszOriginalVolumeName,
	IN  VSS_PWSZ pwszOriginatingMachine,
	IN  VSS_PWSZ pwszServiceMachine,
	IN  VSS_PWSZ pwszExposedName,
	IN  VSS_PWSZ pwszExposedPath,
	IN  VSS_ID ProviderId,
	IN  LONG lSnapshotAttributes,
	IN  VSS_TIMESTAMP tsCreationTimestamp,
	IN  VSS_SNAPSHOT_STATE eStatus
    ) throw (HRESULT)
/*++

Routine description:

    Initialize a VSS_OBJECT_PROP_Ptr in order to keep a VSS_OBJECT_SNAPSHOT structure.

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_OBJECT_PROP_Ptr::InitializeAsSnapshot";
	VSS_OBJECT_PROP* pProp = NULL;

    try
    {
		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_OBJECT_UNION structure
        pProp = static_cast<VSS_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_OBJECT_PROP_Copy::init(pProp);
        pProp->Type = VSS_OBJECT_SNAPSHOT;

        // Getting the equivalent VSS_SNAPSHOT_PROP structure
        BS_ASSERT(pProp);
		VSS_SNAPSHOT_PROP& SnapshotProp = pProp->Obj.Snap;

		// Setting the internal members
		SnapshotProp.m_SnapshotId			= SnapshotId;
		SnapshotProp.m_SnapshotSetId		= SnapshotSetId;
		SnapshotProp.m_lSnapshotsCount      = lSnapshotsCount;
        ::VssSafeDuplicateStr(ft, SnapshotProp.m_pwszSnapshotDeviceObject, pwszSnapshotDeviceObject);
        ::VssSafeDuplicateStr(ft, SnapshotProp.m_pwszOriginalVolumeName, pwszOriginalVolumeName);
        ::VssSafeDuplicateStr(ft, SnapshotProp.m_pwszOriginatingMachine, pwszOriginatingMachine);
        ::VssSafeDuplicateStr(ft, SnapshotProp.m_pwszServiceMachine, pwszServiceMachine);
        ::VssSafeDuplicateStr(ft, SnapshotProp.m_pwszExposedName, pwszExposedName);
        ::VssSafeDuplicateStr(ft, SnapshotProp.m_pwszExposedPath, pwszExposedPath);
		SnapshotProp.m_ProviderId			= ProviderId;
		SnapshotProp.m_lSnapshotAttributes	= lSnapshotAttributes;
		SnapshotProp.m_tsCreationTimestamp = tsCreationTimestamp;
		SnapshotProp.m_eStatus				= eStatus;

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		if (pProp)
		{
			VSS_OBJECT_PROP_Copy::destroy(pProp); // destroy its contents.
			::CoTaskMemFree(static_cast<LPVOID>(pProp));
		}
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_OBJECT_PROP_Ptr::InitializeAsProvider(
    IN  CVssFunctionTracer& ft,
    IN	VSS_ID ProviderId,
    IN	VSS_PWSZ pwszProviderName,
    IN  VSS_PROVIDER_TYPE eProviderType,
    IN	VSS_PWSZ pwszProviderVersion,
    IN	VSS_ID ProviderVersionId,
	IN	CLSID ClassId
    ) throw (HRESULT)
/*++

Routine description:

    Initialize a VSS_OBJECT_PROP_Ptr in order to keep a VSS_OBJECT_PROVIDER structure.

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_OBJECT_PROP_Ptr::InitializeAsProvider";
	VSS_OBJECT_PROP* pProp = NULL;

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		// Assert parameters
		BS_ASSERT(pwszProviderName == NULL || pwszProviderName[0] != L'\0');
		BS_ASSERT(eProviderType == VSS_PROV_SYSTEM ||
            eProviderType == VSS_PROV_SOFTWARE ||
            eProviderType == VSS_PROV_HARDWARE
            );

		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_OBJECT_UNION structure
        pProp = static_cast<VSS_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_OBJECT_PROP_Copy::init(pProp);
        pProp->Type = VSS_OBJECT_PROVIDER;

        // Getting the equivalent VSS_SNAPSHOT_PROP structure
        BS_ASSERT(pProp);
		VSS_PROVIDER_PROP& ProviderProp = pProp->Obj.Prov;

		// Setting the internal members
		ProviderProp.m_ProviderId		 = ProviderId;
        ::VssSafeDuplicateStr(ft, ProviderProp.m_pwszProviderName, pwszProviderName);
		ProviderProp.m_eProviderType	 = eProviderType;
        ::VssSafeDuplicateStr(ft, ProviderProp.m_pwszProviderVersion, pwszProviderVersion);
		ProviderProp.m_ProviderVersionId = ProviderVersionId;
		ProviderProp.m_ClassId			 = ClassId;

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		if (pProp)
		{
			VSS_OBJECT_PROP_Copy::destroy(pProp); // destroy its contents.
			::CoTaskMemFree(static_cast<LPVOID>(pProp));
		}
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_OBJECT_PROP_Ptr::InitializeAsEmpty(
    IN  CVssFunctionTracer& ft
	)
/*++

Routine description:

    Initialize a VSS_OBJECT_PROP_Ptr in order to appear as an empty structure (to be filled later).

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_OBJECT_PROP_Ptr::InitializeEmpty";
	VSS_OBJECT_PROP* pProp = NULL;

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_OBJECT_UNION structure
        pProp = static_cast<VSS_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_OBJECT_PROP_Copy::init(pProp);

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		BS_ASSERT(pProp == NULL);
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_OBJECT_PROP_Ptr::Print(
    IN  CVssFunctionTracer& ft,
	IN  LPWSTR wszOutputBuffer,
	IN  LONG lBufferSize
	)
{
    WCHAR wszFunctionName[] = L"VSS_OBJECT_PROP_Ptr::Print";

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		if (m_pStruct == NULL)
			::StringCchPrintfW(wszOutputBuffer, lBufferSize, L"NULL object\n\n");

        // Effective copy
        switch(m_pStruct->Type)
        {
        case VSS_OBJECT_SNAPSHOT:

			::StringCchPrintfW(wszOutputBuffer, lBufferSize,
				L"Id = " WSTR_GUID_FMT L", "
				L"SnapshotSetId = " WSTR_GUID_FMT L"\n"
				L"SnapCount = %ld "
				L"DevObj = %s\n"
				L"OriginalVolumeName = %s\n"
				L"OriginatingMachine = %s\n"
				L"ServiceMachine = %s\n"
				L"Exposed name = %s\n"
				L"Exposed path = %s\n"
				L"ProviderId = " WSTR_GUID_FMT L"\n"
				L"Attributes = 0x%08lx\n"
				L"Timestamp = " WSTR_LONGLONG_FMT L"\n"
				L"Status = %d\n ",
				GUID_PRINTF_ARG( m_pStruct->Obj.Snap.m_SnapshotId ),
				GUID_PRINTF_ARG( m_pStruct->Obj.Snap.m_SnapshotSetId ),
				m_pStruct->Obj.Snap.m_lSnapshotsCount,
				m_pStruct->Obj.Snap.m_pwszSnapshotDeviceObject,
				m_pStruct->Obj.Snap.m_pwszOriginalVolumeName,
				m_pStruct->Obj.Snap.m_pwszOriginatingMachine,
				m_pStruct->Obj.Snap.m_pwszServiceMachine,
				m_pStruct->Obj.Snap.m_pwszExposedName,
				m_pStruct->Obj.Snap.m_pwszExposedPath,
				GUID_PRINTF_ARG( m_pStruct->Obj.Snap.m_ProviderId ),
				m_pStruct->Obj.Snap.m_lSnapshotAttributes,
				LONGLONG_PRINTF_ARG( m_pStruct->Obj.Snap.m_tsCreationTimestamp ),
				m_pStruct->Obj.Snap.m_eStatus);
            break;

        case VSS_OBJECT_PROVIDER:
            ::StringCchPrintfW(wszOutputBuffer, lBufferSize,
				L"m_ProviderId = " WSTR_GUID_FMT L"\n"
				L"m_pwszProviderName = %s\n"
				L"m_ProviderType = %d\n"
				L"m_pwszProviderVersion = %s\n"
				L"m_ProviderVersionId = " WSTR_GUID_FMT L"\n"
				L"m_ClassID: " WSTR_GUID_FMT L"\n\n",
                GUID_PRINTF_ARG( m_pStruct->Obj.Prov.m_ProviderId ),
                m_pStruct->Obj.Prov.m_pwszProviderName? m_pStruct->Obj.Prov.m_pwszProviderName: L"NULL",
                m_pStruct->Obj.Prov.m_eProviderType,
                m_pStruct->Obj.Prov.m_pwszProviderVersion? m_pStruct->Obj.Prov.m_pwszProviderVersion: L"NULL",
                GUID_PRINTF_ARG( m_pStruct->Obj.Prov.m_ProviderVersionId ),
                GUID_PRINTF_ARG( m_pStruct->Obj.Prov.m_ClassId )
                );
            break;

        default:
			ft.ErrBox( VSSDBG_GEN, E_UNEXPECTED,
					   L"%s: Wrong object type %d", wszFunctionName, m_pStruct->Type );
            break;
        }
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
		BS_ASSERT(m_pStruct == NULL);
		ft.Throw( VSSDBG_GEN, E_UNEXPECTED,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}



/////////////////////////////////////////////////////////////////////////////
//  VSS_MGMT_OBJECT_PROP_Ptr class


void VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsVolume(
		IN  CVssFunctionTracer& ft,
 		IN  VSS_PWSZ pwszVolumeName,
 		IN  VSS_PWSZ pwszVolumeDisplayName
		) throw (HRESULT)
/*++

Routine description:

    Initialize a VSS_MGMT_OBJECT_PROP_Ptr in order to keep a VSS_MGMT_OBJECT_SNAPSHOT structure.

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsVolume";
	VSS_MGMT_OBJECT_PROP* pProp = NULL;

    try
    {
		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_MGMT_OBJECT_UNION structure
        pProp = static_cast<VSS_MGMT_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_MGMT_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_MGMT_OBJECT_PROP_Copy::init(pProp);
        pProp->Type = VSS_MGMT_OBJECT_VOLUME;

        // Getting the equivalent VSS_SNAPSHOT_PROP structure
        BS_ASSERT(pProp);
		VSS_VOLUME_PROP& VolumeProp = pProp->Obj.Vol;

		// Setting the internal members
        ::VssSafeDuplicateStr(ft, VolumeProp.m_pwszVolumeName, pwszVolumeName);
        ::VssSafeDuplicateStr(ft, VolumeProp.m_pwszVolumeDisplayName, pwszVolumeDisplayName);

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		if (pProp)
		{
			VSS_MGMT_OBJECT_PROP_Copy::destroy(pProp); // destroy its contents.
			::CoTaskMemFree(static_cast<LPVOID>(pProp));
		}
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsDiffVolume(
		IN  CVssFunctionTracer& ft,
 		IN  VSS_PWSZ pwszVolumeName,
 		IN  VSS_PWSZ pwszVolumeDisplayName,
		IN  LONGLONG llVolumeFreeSpace,
		IN  LONGLONG llVolumeTotalSpace
		) throw (HRESULT)
/*++

Routine description:

    Initialize a VSS_MGMT_OBJECT_PROP_Ptr in order to keep a VSS_MGMT_OBJECT_PROVIDER structure.

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsDiffVolume";
	VSS_MGMT_OBJECT_PROP* pProp = NULL;

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		// Assert parameters
		BS_ASSERT(llVolumeFreeSpace >= 0);
		BS_ASSERT(llVolumeTotalSpace >= 0);

		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_MGMT_OBJECT_UNION structure
        pProp = static_cast<VSS_MGMT_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_MGMT_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_MGMT_OBJECT_PROP_Copy::init(pProp);
        pProp->Type = VSS_MGMT_OBJECT_DIFF_VOLUME;

        // Getting the equivalent VSS_SNAPSHOT_PROP structure
        BS_ASSERT(pProp);
		VSS_DIFF_VOLUME_PROP& DiffVolumeProp = pProp->Obj.DiffVol;

		// Setting the internal members
        ::VssSafeDuplicateStr(ft, DiffVolumeProp.m_pwszVolumeName, pwszVolumeName);
        ::VssSafeDuplicateStr(ft, DiffVolumeProp.m_pwszVolumeDisplayName, pwszVolumeDisplayName);
        DiffVolumeProp.m_llVolumeFreeSpace = llVolumeFreeSpace;
        DiffVolumeProp.m_llVolumeTotalSpace = llVolumeTotalSpace;

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		if (pProp)
		{
			VSS_MGMT_OBJECT_PROP_Copy::destroy(pProp); // destroy its contents.
			::CoTaskMemFree(static_cast<LPVOID>(pProp));
		}
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsDiffArea(
		IN  CVssFunctionTracer& ft,
 		IN  VSS_PWSZ pwszVolumeName,
 		IN  VSS_PWSZ pwszDiffAreaVolumeName,
		IN  LONGLONG llUsedDiffSpace,
		IN  LONGLONG llAllocatedDiffSpace,
		IN  LONGLONG llMaximumDiffSpace
		) throw (HRESULT)
/*++

Routine description:

    Initialize a VSS_MGMT_OBJECT_PROP_Ptr in order to keep a VSS_MGMT_OBJECT_PROVIDER structure.

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsProvider";
	VSS_MGMT_OBJECT_PROP* pProp = NULL;

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		// Assert parameters
		BS_ASSERT(llUsedDiffSpace >= 0);
		BS_ASSERT(llAllocatedDiffSpace >= 0);
		BS_ASSERT((llMaximumDiffSpace >= 0) || (llMaximumDiffSpace == -1));

		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_MGMT_OBJECT_UNION structure
        pProp = static_cast<VSS_MGMT_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_MGMT_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_MGMT_OBJECT_PROP_Copy::init(pProp);
        pProp->Type = VSS_MGMT_OBJECT_DIFF_AREA;

        // Getting the equivalent VSS_SNAPSHOT_PROP structure
        BS_ASSERT(pProp);
		VSS_DIFF_AREA_PROP& DiffAreaProp = pProp->Obj.DiffArea;

		// Setting the internal members
        ::VssSafeDuplicateStr(ft, DiffAreaProp.m_pwszVolumeName, pwszVolumeName);
        ::VssSafeDuplicateStr(ft, DiffAreaProp.m_pwszDiffAreaVolumeName, pwszDiffAreaVolumeName);
        DiffAreaProp.m_llMaximumDiffSpace = llMaximumDiffSpace;
        DiffAreaProp.m_llAllocatedDiffSpace = llAllocatedDiffSpace;
        DiffAreaProp.m_llUsedDiffSpace = llUsedDiffSpace;

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		if (pProp)
		{
			VSS_MGMT_OBJECT_PROP_Copy::destroy(pProp); // destroy its contents.
			::CoTaskMemFree(static_cast<LPVOID>(pProp));
		}
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_MGMT_OBJECT_PROP_Ptr::InitializeAsEmpty(
    IN  CVssFunctionTracer& ft
	)
/*++

Routine description:

    Initialize a VSS_MGMT_OBJECT_PROP_Ptr in order to appear as an empty structure (to be filled later).

Throws:

    E_OUTOFMEMORY

--*/
{
    WCHAR wszFunctionName[] = L"VSS_MGMT_OBJECT_PROP_Ptr::InitializeEmpty";
	VSS_MGMT_OBJECT_PROP* pProp = NULL;

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		// Assert the structure pointer to be set
        BS_ASSERT(m_pStruct == NULL);

        // Allocate the VSS_MGMT_OBJECT_UNION structure
        pProp = static_cast<VSS_MGMT_OBJECT_PROP*>(::CoTaskMemAlloc(sizeof(VSS_MGMT_OBJECT_PROP)));
        if (pProp == NULL)
            ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
					  L"%s: Error on allocating the Properties structure",
					  wszFunctionName );

        // Initialize the structure
		VSS_MGMT_OBJECT_PROP_Copy::init(pProp);

		// Setting the pointer field
		m_pStruct = pProp;
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
	    BS_ASSERT(ft.hr == E_OUTOFMEMORY);
		BS_ASSERT(m_pStruct == NULL);
		BS_ASSERT(pProp == NULL);
		ft.Throw( VSSDBG_GEN, E_OUTOFMEMORY,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


void VSS_MGMT_OBJECT_PROP_Ptr::Print(
    IN  CVssFunctionTracer& ft,
	IN  LPWSTR wszOutputBuffer,
	IN  LONG lBufferSize
	)
{
    WCHAR wszFunctionName[] = L"VSS_MGMT_OBJECT_PROP_Ptr::Print";

	// Reset the error code
	ft.hr = S_OK;
		
    try
    {
		if (m_pStruct == NULL)
			::StringCchPrintfW(wszOutputBuffer, lBufferSize, L"NULL object\n\n");

        switch(m_pStruct->Type)
        {
        case VSS_MGMT_OBJECT_VOLUME:

			::StringCchPrintfW(wszOutputBuffer, lBufferSize,
				L"VolumeName = %s\n"
				L"VolumeDisplayName = %s\n"
				L"lCurrentSnapshotCount = %ld \n",
				m_pStruct->Obj.Vol.m_pwszVolumeName,
				m_pStruct->Obj.Vol.m_pwszVolumeDisplayName);
            break;

        case VSS_MGMT_OBJECT_DIFF_VOLUME:
			::StringCchPrintfW(wszOutputBuffer, lBufferSize,
				L"VolumeName = %s\n"
				L"VolumeDisplayName = %s\n"
				L"llFreeSpace = " WSTR_LONGLONG_FMT L"\n"
				L"llTotalSpace = " WSTR_LONGLONG_FMT L"\n",
				m_pStruct->Obj.DiffVol.m_pwszVolumeName,
				m_pStruct->Obj.DiffVol.m_pwszVolumeDisplayName,
				m_pStruct->Obj.DiffVol.m_llVolumeFreeSpace,
				m_pStruct->Obj.DiffVol.m_llVolumeTotalSpace);
            break;

        case VSS_MGMT_OBJECT_DIFF_AREA:
			::StringCchPrintfW(wszOutputBuffer, lBufferSize,
				L"VolumeName = %s\n"
				L"VolumeDiffAreaName = %s\n"
				L"MaxSpace = " WSTR_LONGLONG_FMT L"\n"
				L"AllocatedSpace = " WSTR_LONGLONG_FMT L"\n"
				L"UsedSpace = " WSTR_LONGLONG_FMT L"\n",
				m_pStruct->Obj.DiffArea.m_pwszVolumeName,
				m_pStruct->Obj.DiffArea.m_pwszDiffAreaVolumeName,
				m_pStruct->Obj.DiffArea.m_llMaximumDiffSpace,
				m_pStruct->Obj.DiffArea.m_llAllocatedDiffSpace,
				m_pStruct->Obj.DiffArea.m_llUsedDiffSpace);
            break;


        default:
			ft.ErrBox( VSSDBG_GEN, E_UNEXPECTED,
					   L"%s: Wrong object type %d", wszFunctionName, m_pStruct->Type );
            break;
        }
    }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
	{
		BS_ASSERT(m_pStruct == NULL);
		ft.Throw( VSSDBG_GEN, E_UNEXPECTED,
				  L"%s: Error catched 0x%08lx", wszFunctionName, ft.hr );
	}
}


