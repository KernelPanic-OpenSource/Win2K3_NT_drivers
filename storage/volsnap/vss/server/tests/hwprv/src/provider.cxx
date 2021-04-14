/*++

Copyright (c) 1999  Microsoft Corporation

Abstract:

    @doc
    @module Provider.hxx | Declarations used by the Software Snapshot Provider interface
    @end

Author:

    Brian Berkowitz  [brianb]   05/01/2001

Revision History:

    Name        Date        Comments


--*/


/////////////////////////////////////////////////////////////////////////////
//  Includes


#include "stdafx.hxx"
#include <winnt.h>

//  Generated MIDL header
#include "vss.h"
#include "vdslun.h"
#include "vsprov.h"
#include "vscoordint.h"
#include "hwprv.h"


#include "resource.h"
#include "vs_inc.hxx"
#include "vs_sec.hxx"
#include "vs_reg.hxx"
#include "hwprv.hxx"


#include "provider.hxx"

////////////////////////////////////////////////////////////////////////
//  Standard foo for file name aliasing.  This code block must be after
//  all includes of VSS header files.
//
#ifdef VSS_FILE_ALIAS
#undef VSS_FILE_ALIAS
#endif
#define VSS_FILE_ALIAS "TSTHWPRC"
//
////////////////////////////////////////////////////////////////////////

CHardwareTestProvider::CHardwareTestProvider () :
    m_SnapshotSetId(GUID_NULL),
    m_maskSnapshotDisks(0),
    m_cDiskIds(0),
    m_pDynamicDiskInfo(NULL)
    {
    for(UINT i = 0; i < 32; i++)
        {
        m_rgpSourceLuns[i] = NULL;
        m_rgpDestinationLuns[i] = NULL;
        }

    memset(m_rgDiskSignatures, 0, sizeof(m_rgDiskSignatures));
    m_bRebuilt = false;
    }

CHardwareTestProvider::~CHardwareTestProvider()
    {
    ClearConfiguration();
    if (m_pDynamicDiskInfo)
        {
        do
            {
            DYNAMIC_DISK_INFO *pNext = m_pDynamicDiskInfo->m_next;
            delete m_pDynamicDiskInfo->m_wszVolumes;
            delete m_pDynamicDiskInfo;
            m_pDynamicDiskInfo = pNext;
            } while(m_pDynamicDiskInfo);
        }
    }


STDMETHODIMP CHardwareTestProvider::OnLoad
    (
    IN      IUnknown* pCallback
    )
    {
    UNREFERENCED_PARAMETER(pCallback);

    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::OnLoad");

    try
        {
        m_cs.Init();

        CVssSafeAutomaticLock lock(m_cs);
        ClearConfiguration();
        LoadConfigurationData();

        // clear various bits of snapshot goo
        m_bHidden = false;
        m_SnapshotSetId = GUID_NULL;
        m_maskSnapshotDisks = 0;
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::OnUnload
    (
    IN      BOOL    bForceUnload
    )
    {
    UNREFERENCED_PARAMETER(bForceUnload);

    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::OnUnload");

    CVssSafeAutomaticLock lock(m_cs);

    ClearConfiguration();

    return ft.hr;
    }


    //
    //  IVssHardwareSnapshotProvider
    //

STDMETHODIMP CHardwareTestProvider::FillInLunInfo
    (
    IN VSS_PWSZ wszDevice,
    IN OUT VDS_LUN_INFORMATION *pLunInformation,
    OUT BOOL *pbIsSupported
    )
    {
    CVssFunctionTracer ft(VSSDBG_COORD, L"CHardwareTestProvider::FillInLunInfo");

    CVssSafeAutomaticLock lock(m_cs);
    m_bRebuilt = false;
    try
        {
        if (pbIsSupported == NULL || pLunInformation == NULL)
            throw(VSSDBG_VSSTEST, E_INVALIDARG, L"NULL output parameter");

        *pbIsSupported = FALSE;
        bool bFound = false;

        RebuildLunInfo();

        for(DWORD iDestLun = 0; iDestLun < m_cDiskIds; iDestLun++)
            {
            if (IsMatchLun(*pLunInformation, *(m_rgpDestinationLuns[iDestLun])))
                {
                bFound = true;
                break;
                }
            }

        if (bFound)
            {
            pLunInformation->m_rgInterconnects = (VDS_INTERCONNECT *) CoTaskMemAlloc(sizeof(VDS_INTERCONNECT));
            if (pLunInformation->m_rgInterconnects == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

            memset(pLunInformation->m_rgInterconnects, 0, sizeof(VDS_INTERCONNECT));
            pLunInformation->m_cInterconnects = 1;
            pLunInformation->m_rgInterconnects->m_pbAddress = (BYTE *) CoTaskMemAlloc(sizeof(ULONG));
            if (pLunInformation->m_rgInterconnects->m_pbAddress == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

            pLunInformation->m_rgInterconnects->m_cbAddress = sizeof(ULONG);
            pLunInformation->m_rgInterconnects->m_addressType = VDS_IA_FCPH;
            pLunInformation->m_rgInterconnects->m_pbPort = (BYTE *) CoTaskMemAlloc(sizeof(ULONG));
            if (pLunInformation->m_rgInterconnects->m_pbPort == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

            *((ULONG *) (pLunInformation->m_rgInterconnects->m_pbPort)) = 0x8828080;
            pLunInformation->m_rgInterconnects->m_cbPort = sizeof(ULONG);
            for(iDestLun = 0; iDestLun < m_cDiskIds; iDestLun++)
                {
                if (IsMatchLun(*pLunInformation, *(m_rgpDestinationLuns[iDestLun])))
                    {
                    *(ULONG *) (pLunInformation->m_rgInterconnects->m_pbAddress) = iDestLun;
                    break;
                    }
                }

            *pbIsSupported = true;
            }
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::AreLunsSupported
    (
    IN  LONG lLunCount,
    IN  LONG lContext,
    IN  VSS_PWSZ *rgwszDevices,
    IN OUT  VDS_LUN_INFORMATION *rgLunInformation,
    OUT BOOL *pbIsSupported
    )
    {
    UNREFERENCED_PARAMETER(rgwszDevices);
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::AreLunsSupported");

    CVssSafeAutomaticLock lock(m_cs);
    m_bRebuilt = false;
    try
        {
        if (pbIsSupported == NULL || rgLunInformation == NULL)
            throw(VSSDBG_VSSTEST, E_INVALIDARG, L"NULL output parameter");

        *pbIsSupported = FALSE;
        VDS_LUN_INFORMATION *pLunInformation = rgLunInformation;
        bool bFound = false;

        RebuildLunInfo();

        for(LONG iLun = 0; iLun < lLunCount; iLun++, pLunInformation++)
            {
            bFound = false;
            for(DWORD iSourceLun = 0; iSourceLun < m_cDiskIds; iSourceLun++)
                {
                if (IsMatchLun(*pLunInformation, *(m_rgpSourceLuns[iSourceLun])))
                    {
                    bFound = true;
                    break;
                    }
                }

            if (!bFound)
                break;
            }

        if (bFound)
            {
            pLunInformation = rgLunInformation;
            for(LONG iLun = 0; iLun < lLunCount; iLun++, pLunInformation++)
                {
                pLunInformation->m_rgInterconnects = (VDS_INTERCONNECT *) CoTaskMemAlloc(sizeof(VDS_INTERCONNECT));
                if (pLunInformation->m_rgInterconnects == NULL)
                    ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

                memset(pLunInformation->m_rgInterconnects, 0, sizeof(VDS_INTERCONNECT));
                pLunInformation->m_cInterconnects = 1;
                pLunInformation->m_rgInterconnects->m_pbAddress = (BYTE *) CoTaskMemAlloc(sizeof(ULONG));
                if (pLunInformation->m_rgInterconnects->m_pbAddress == NULL)
                    ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

                pLunInformation->m_rgInterconnects->m_cbAddress = sizeof(ULONG);
                pLunInformation->m_rgInterconnects->m_addressType = VDS_IA_FCPH;
                pLunInformation->m_rgInterconnects->m_pbPort = (BYTE *) CoTaskMemAlloc(sizeof(ULONG));
                if (pLunInformation->m_rgInterconnects->m_pbPort == NULL)
                    ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

                *((ULONG *) (pLunInformation->m_rgInterconnects->m_pbPort)) = 0x8828080;
                pLunInformation->m_rgInterconnects->m_cbPort = sizeof(ULONG);
                for(DWORD iSourceLun = 0; iSourceLun < m_cDiskIds; iSourceLun++)
                    {
                    if (IsMatchLun(*pLunInformation, *(m_rgpSourceLuns[iSourceLun])))
                        {
                        *(ULONG *) (pLunInformation->m_rgInterconnects->m_pbAddress) = iSourceLun;
                        break;
                        }
                    }
                }

            *pbIsSupported = true;
            }
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }


STDMETHODIMP CHardwareTestProvider::BeginPrepareSnapshot
    (
    IN      VSS_ID          SnapshotSetId,
    IN      VSS_ID          SnapshotId,
    IN      LONG            lContext,
    IN      LONG            lLunCount,
    IN OUT  VSS_PWSZ        *rgwszDevices,
    IN      VDS_LUN_INFORMATION *rgLunInformation
    )
    {
    UNREFERENCED_PARAMETER(SnapshotId);
    UNREFERENCED_PARAMETER(rgwszDevices);
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::BeginPrepareSnapshot");

    CVssSafeAutomaticLock lock(m_cs);
    m_bRebuilt = false;
    try
        {
        if (m_SnapshotSetId == GUID_NULL)
            {
            m_lContext = lContext;
            m_SnapshotSetId = SnapshotSetId;
            m_maskSnapshotDisks = 0;
            }

        VDS_LUN_INFORMATION *pLunInformation = rgLunInformation;

        for(LONG iLun = 0; iLun < lLunCount; iLun++, pLunInformation++)
            {
            if (pLunInformation->m_cInterconnects != 1 ||
                pLunInformation->m_rgInterconnects == NULL ||
                pLunInformation->m_rgInterconnects[0].m_cbAddress != sizeof(ULONG) ||
                pLunInformation->m_rgInterconnects[0].m_cbPort != sizeof(ULONG) ||
                *(ULONG *) (pLunInformation->m_rgInterconnects[0].m_pbPort) != 0x8828080)
                ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"Drive not supported");

            ULONG iDisk = *(ULONG *) (pLunInformation->m_rgInterconnects[0].m_pbAddress);
            *(ULONG *) (pLunInformation->m_rgInterconnects[0].m_pbAddress) |= 0x80000000;

            m_maskSnapshotDisks |= 1 << iDisk;
            }
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }

// free all the components of a VDS_LUN_INFO structure
void CHardwareTestProvider::FreeLunInfo
    (
    IN VDS_LUN_INFORMATION *rgLunInfo,
    UINT cLuns
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CVssTestHardwareProvider::FreeLunInfo");

    // check if pointer is NULL
    if (rgLunInfo == NULL)
        return;

    // loop through individual luns
    for(DWORD iLun = 0; iLun < cLuns; iLun++)
        {
        // get lun info
        VDS_LUN_INFORMATION *pLun = &rgLunInfo[iLun];

        // free up strings
        if (pLun->m_szVendorId)
            CoTaskMemFree(pLun->m_szVendorId);

        if (pLun->m_szProductId)
            CoTaskMemFree(pLun->m_szProductId);

        if (pLun->m_szProductRevision)
            CoTaskMemFree(pLun->m_szProductRevision);

        if (pLun->m_szSerialNumber)
            CoTaskMemFree(pLun->m_szSerialNumber);


        // point to VDS_STORAGE_DEVICE_ID_DESCRIPTOR
        // get number of VDS_STORAGE_IDENTIFIERs in the descriptor
        ULONG cIds = pLun->m_deviceIdDescriptor.m_cIdentifiers;

        // point to first VDS_STORAGE_IDENTIFIER
        VDS_STORAGE_IDENTIFIER *pvsi = pLun->m_deviceIdDescriptor.m_rgIdentifiers;
        for(ULONG iId = 0; iId < cIds; iId++, pvsi++)
            {
            // free up data for identifier
            if (pvsi->m_rgbIdentifier)
                CoTaskMemFree(pvsi->m_rgbIdentifier);
            }

        // free up array of identifiers
        if (pLun->m_deviceIdDescriptor.m_rgIdentifiers)
            CoTaskMemFree(pLun->m_deviceIdDescriptor.m_rgIdentifiers);

        // point to first VDS_INTERCONNECT
        VDS_INTERCONNECT *pInterconnect = pLun->m_rgInterconnects;

        for(ULONG iInterconnect = 0; iInterconnect < pLun->m_cInterconnects; iInterconnect++, pInterconnect++)
            {
            // free up address if there is one
            if (pInterconnect->m_pbAddress)
                CoTaskMemFree(pInterconnect->m_pbAddress);

            if (pInterconnect->m_pbPort)
                CoTaskMemFree(pInterconnect->m_pbPort);
            }

        // free up array of interconnects
        if (pLun->m_rgInterconnects)
            CoTaskMemFree(pLun->m_rgInterconnects);
        }

    // free up array of lun information
    CoTaskMemFree(rgLunInfo);
    }

void CHardwareTestProvider::CopyString(LPSTR &szCopy, LPCSTR szSource)
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::CopyString");

    if (szSource == NULL)  {
        szCopy = NULL;
        return;
    }
    szCopy = (LPSTR) CoTaskMemAlloc(strlen(szSource) + 1);
    if (szCopy == NULL)
        ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

    strcpy(szCopy, szSource);
    }

void CHardwareTestProvider::CopyBinaryData
    (
    LPBYTE &rgb,
    UINT cbSource,
    const BYTE *rgbSource
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::CopyBinaryData");

    rgb = (BYTE *) CoTaskMemAlloc(cbSource);
    if (rgb == NULL)
        ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

    memcpy(rgb, rgbSource, cbSource);
    }



STDMETHODIMP CHardwareTestProvider::GetTargetLuns
    (
    IN      LONG        lLunCount,
    IN      VSS_PWSZ    *rgwszDevices,
    IN      VDS_LUN_INFORMATION *rgSourceLuns,
    IN OUT  VDS_LUN_INFORMATION *rgDestinationLuns
    )
    {
    UNREFERENCED_PARAMETER(rgwszDevices);
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::GetTargetLuns");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        memset(rgDestinationLuns, 0, lLunCount * sizeof(VDS_LUN_INFORMATION));
        for(LONG iLun = 0; iLun < lLunCount; iLun++)
            {
            VDS_LUN_INFORMATION *pLunInfo = NULL;
            for(DWORD iDisk = 0; iDisk < m_cDiskIds; iDisk++)
                {
                if (IsMatchLun(*m_rgpSourceLuns[iDisk], rgSourceLuns[iLun]))
                    pLunInfo = m_rgpDestinationLuns[iDisk];
                }

            if (pLunInfo == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_UNEXPECTED, L"Can't find source lun");

            VDS_LUN_INFORMATION *pDestLun = &rgDestinationLuns[iLun];
            VDS_LUN_INFORMATION *pSourceLun = &rgSourceLuns[iLun];

            pDestLun->m_BusType = pLunInfo->m_BusType;
            pDestLun->m_DeviceType = pLunInfo->m_DeviceType;
            pDestLun->m_DeviceTypeModifier = pLunInfo->m_DeviceTypeModifier;
            pDestLun->m_bCommandQueueing = pLunInfo->m_bCommandQueueing;
            pDestLun->m_diskSignature = pLunInfo->m_diskSignature;
            CopyString(pDestLun->m_szVendorId, pLunInfo->m_szVendorId);
            CopyString(pDestLun->m_szProductId, pLunInfo->m_szProductId);
            CopyString(pDestLun->m_szProductRevision, pLunInfo->m_szProductRevision);
            CopyString(pDestLun->m_szSerialNumber, pLunInfo->m_szSerialNumber);
            ULONG cIds = pLunInfo->m_deviceIdDescriptor.m_cIdentifiers;
            pDestLun->m_deviceIdDescriptor.m_version = pLunInfo->m_deviceIdDescriptor.m_version;
            pDestLun->m_deviceIdDescriptor.m_cIdentifiers = cIds;
            pDestLun->m_deviceIdDescriptor.m_rgIdentifiers = (VDS_STORAGE_IDENTIFIER *) CoTaskMemAlloc(cIds * sizeof(VDS_STORAGE_DEVICE_ID_DESCRIPTOR));
            memset(pDestLun->m_deviceIdDescriptor.m_rgIdentifiers, 0, cIds * sizeof(VDS_STORAGE_DEVICE_ID_DESCRIPTOR));
            VDS_STORAGE_IDENTIFIER *pSourceId = pLunInfo->m_deviceIdDescriptor.m_rgIdentifiers;
            VDS_STORAGE_IDENTIFIER *pDestId = pDestLun->m_deviceIdDescriptor.m_rgIdentifiers;
            for(ULONG iId = 0; iId < cIds; iId++, pSourceId++, pDestId++)
                {
                memcpy(pDestId, pSourceId, FIELD_OFFSET(VDS_STORAGE_IDENTIFIER, m_rgbIdentifier));
                CopyBinaryData(pDestId->m_rgbIdentifier, pSourceId->m_cbIdentifier, pSourceId->m_rgbIdentifier);
                }

            pDestLun->m_cInterconnects = 1;
            pDestLun->m_rgInterconnects = (VDS_INTERCONNECT *) CoTaskMemAlloc(sizeof(VDS_INTERCONNECT));
            memset(pDestLun->m_rgInterconnects, 0, sizeof(VDS_INTERCONNECT));
            VDS_INTERCONNECT *pDestInterconnect = pDestLun->m_rgInterconnects;
            VDS_INTERCONNECT *pSourceInterconnect = pSourceLun->m_rgInterconnects;
            BS_ASSERT(((*(ULONG *) (pSourceInterconnect->m_pbAddress)) & 0x80000000) != 0)
            pDestInterconnect->m_addressType = VDS_IA_FCPH;
            pDestInterconnect->m_pbAddress = (BYTE *) CoTaskMemAlloc(sizeof(ULONG));
            if (pDestInterconnect->m_pbAddress == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

            pDestInterconnect->m_cbAddress = sizeof(ULONG);
            *(ULONG *) (pDestInterconnect->m_pbAddress) = iDisk;
            pDestInterconnect->m_pbPort = (BYTE *) CoTaskMemAlloc(sizeof(ULONG));
            if (pDestInterconnect->m_pbPort == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Out of memory.");

            *(ULONGLONG *) (pDestInterconnect->m_pbPort) = 0x9364321;
            pDestInterconnect->m_cbPort = sizeof(ULONG);
            }
        }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
        FreeLunInfo(rgDestinationLuns, lLunCount);

    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::LocateLuns
    (
    IN      LONG        lLunCount,
    IN      VDS_LUN_INFORMATION *rgSourceLuns
    )
    {
    UNREFERENCED_PARAMETER(lLunCount);
    UNREFERENCED_PARAMETER(rgSourceLuns);
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::LocateLuns");

    CVssSafeAutomaticLock lock(m_cs);
    m_bRebuilt = false;
    try
        {
        CVssAutoWin32Handle hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"Global\\VSNAP_START_IMPORT");
        if (hEvent == NULL)
            {
            ft.hr = HRESULT_FROM_WIN32(GetLastError());
            ft.CheckForError(VSSDBG_COORD, L"CreateEvent");
            }

        CVssAutoWin32Handle hEventComplete = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"Global\\VSNAP_START_IMPORT_COMPLETED");
        if (hEventComplete == NULL)
            {
            ft.hr = HRESULT_FROM_WIN32(GetLastError());
            ft.CheckForError(VSSDBG_COORD, L"CreateEvent");
            }


        if (!SetEvent(hEvent))
            {
            ft.hr = HRESULT_FROM_WIN32(GetLastError());
            ft.CheckForError(VSSDBG_COORD, L"SetEvent");
            }

        if (WaitForSingleObject(hEventComplete, INFINITE) == WAIT_FAILED)
            {
            ft.hr = HRESULT_FROM_WIN32(GetLastError());
            ft.CheckForError(VSSDBG_COORD, L"WaitForSingleObject");
            }
        }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
        ft.hr = VSS_E_PROVIDER_VETO;

    return ft.hr;
    }

// perform a device i/o control, growing the buffer if necessary
BOOL CHardwareTestProvider::DoDeviceIoControl
    (
    IN HANDLE hDevice,              // handle
    IN DWORD ioctl,                 // device ioctl to perform
    IN const LPBYTE pbQuery,        // input buffer
    IN DWORD cbQuery,               // size of input buffer
    IN OUT LPBYTE *ppbOut,          // pointer to output buffer
    IN OUT DWORD *pcbOut            // pointer to size of output buffer
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::DoDeviceIoControl");

    // loop in case buffer is too small for query result.
    while(TRUE)
        {
        DWORD dwSize;
        if (*ppbOut == NULL)
            {
            // allocate buffer for result of query
            *ppbOut = new BYTE[*pcbOut];
            if (*ppbOut == NULL)
                ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Couldn't allocate query results buffer");
            }

        // do query
        if (!DeviceIoControl
                (
                hDevice,
                ioctl,
                pbQuery,
                cbQuery,
                *ppbOut,
                *pcbOut,
                &dwSize,
                NULL
                ))
            {
            // query failed
            DWORD dwErr = GetLastError();

            if (dwErr == ERROR_INVALID_FUNCTION ||
                dwErr == ERROR_NOT_SUPPORTED ||
                dwErr == ERROR_NOT_READY)
                return FALSE;


            if (dwErr == ERROR_INSUFFICIENT_BUFFER ||
                dwErr == ERROR_MORE_DATA)
                {
                // buffer wasn't big enough allocate a new
                // buffer of the specified return size
                delete *ppbOut;
                *ppbOut = NULL;
                *pcbOut *= 2;
                continue;
                }

            // all other errors are remapped (and potentially logged)
            ft.hr = HRESULT_FROM_WIN32(dwErr);
            ft.CheckForError(VSSDBG_VSSTEST, L"DeviceIoControl %d");
            }

        break;
        }

    return TRUE;
    }


bool CHardwareTestProvider::BuildLunInfoFromDrive
    (
    IN HANDLE hDrive,
    OUT VDS_LUN_INFORMATION **ppLun,
    OUT DWORD &cPartitions
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::BuildLunInfoFromDrive");

    LPBYTE bufQuery = NULL;
    VDS_LUN_INFORMATION *pLun;
    try
        {
        BOOL b;

        // allocate lun information
        pLun = (VDS_LUN_INFORMATION *) CoTaskMemAlloc(sizeof(VDS_LUN_INFORMATION));
        if (pLun == NULL)
            ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Cannot allocate LUN information array");

        // clear lun information in case we throw
        memset(pLun, 0, sizeof(VDS_LUN_INFORMATION));

        // query to get STORAGE_DEVICE_OBJECT
        STORAGE_PROPERTY_QUERY query;
        DWORD cbQuery = 1024;


        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        b = DoDeviceIoControl
                (
                hDrive,
                IOCTL_STORAGE_QUERY_PROPERTY,
                (LPBYTE) &query,
                sizeof(query),
                (LPBYTE *) &bufQuery,
                &cbQuery
                );

        if (!b)
            {
            delete bufQuery;
            CoTaskMemFree(pLun);
            return false;
            }


        // coerce to STORAGE_DEVICE_DESCRIPTOR
        STORAGE_DEVICE_DESCRIPTOR *pDesc = (STORAGE_DEVICE_DESCRIPTOR *) bufQuery;

        pLun->m_version = VER_VDS_LUN_INFORMATION;
        pLun->m_DeviceType = pDesc->DeviceType;
        pLun->m_DeviceTypeModifier = pDesc->DeviceTypeModifier;
        pLun->m_bCommandQueueing = pDesc->CommandQueueing;

        // copy bus type
        pLun->m_BusType = (VDS_STORAGE_BUS_TYPE) pDesc->BusType;

        // copy in various strings
        if (pDesc->VendorIdOffset)
            CopySDString(&pLun->m_szVendorId, pDesc, pDesc->VendorIdOffset);

        if (pDesc->ProductIdOffset)
            CopySDString(&pLun->m_szProductId, pDesc, pDesc->ProductIdOffset);

        if (pDesc->ProductRevisionOffset)
            CopySDString(&pLun->m_szProductRevision, pDesc, pDesc->ProductRevisionOffset);

        if (pDesc->SerialNumberOffset)
            CopySDString(&pLun->m_szSerialNumber, pDesc, pDesc->SerialNumberOffset);

        // query for STORAGE_DEVICE_ID_DESCRIPTOR
        query.PropertyId = StorageDeviceIdProperty;
        query.QueryType = PropertyStandardQuery;

        if (DoDeviceIoControl
                (
                hDrive,
                IOCTL_STORAGE_QUERY_PROPERTY,
                (LPBYTE) &query,
                sizeof(query),
                (LPBYTE *) &bufQuery,
                &cbQuery
                ))
            {
            // coerce buffer to STORAGE_DEVICE_ID_DESCRIPTOR
            STORAGE_DEVICE_ID_DESCRIPTOR *pDevId = (STORAGE_DEVICE_ID_DESCRIPTOR *) bufQuery;
            CopyStorageDeviceIdDescriptorToLun(pDevId, pLun);
            }

        if (DoDeviceIoControl
                (
                hDrive,
                IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                NULL,
                0,
                (LPBYTE *) &bufQuery,
                &cbQuery
                ))
            {
            // compute signature from layout
            DRIVE_LAYOUT_INFORMATION_EX *pLayout = (DRIVE_LAYOUT_INFORMATION_EX *) bufQuery;
            VSS_ID signature = GUID_NULL;

            cPartitions = pLayout->PartitionCount;

            switch(pLayout->PartitionStyle)
                {
                default:
                    BS_ASSERT(FALSE);
                    break;

                case PARTITION_STYLE_RAW:
                    break;

                case PARTITION_STYLE_GPT:
                    // use GPT DiskId as signature
                    signature = pLayout->Gpt.DiskId;
                    break;

                case PARTITION_STYLE_MBR:
                    // use 32 bit Mbr signature as high part of guid.  Remainder
                    // of guid is 0.
                    signature.Data1 = pLayout->Mbr.Signature;
                    break;
                }

            // save disk signature
            pLun->m_diskSignature = signature;
            }
        }
    VSS_STANDARD_CATCH(ft)

    delete bufQuery;
    if (ft.HrFailed())
        {
        FreeLunInfo(pLun, 1);
        ft.Throw(VSSDBG_VSSTEST, ft.hr, L"Rethrowing");
        }

    *ppLun = pLun;

    return TRUE;
    }



STDMETHODIMP CHardwareTestProvider::OnLunEmpty
    (
    IN VSS_PWSZ wszDevice,
    IN VDS_LUN_INFORMATION *pInfo
    )
    {
    UNREFERENCED_PARAMETER(wszDevice);
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::OnLunEmpty");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        for(DWORD iDisk = 0; iDisk < m_cDiskIds; iDisk++)
            {
            if (IsMatchLun(*(m_rgpDestinationLuns[iDisk]), *pInfo))
                break;
            }

        if (iDisk == m_cDiskIds)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"Cannot locate lun");

        // reexpose partitions after lun is freed so we see something change
        DWORD diskId;
        bool bIsDynamic;

        if (TranslateDisk(m_rgDestinationDiskIds[iDisk], diskId, bIsDynamic))
            {
            if (bIsDynamic)
                HideAndExposeDynamicVolumes(m_rgDestinationDiskIds[iDisk], false, true);
            else
                HideAndExposePartitions(diskId, m_rgcDestinationPartitions[iDisk], false, true);
            }

        AddFreedLun(wszDevice);
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }

// IVssProviderCreateSnapshotSet interface
STDMETHODIMP CHardwareTestProvider::EndPrepareSnapshots
    (
    IN      VSS_ID          SnapshotSetId
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::EndPrepareSnapshots");
    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::PreCommitSnapshots
    (
    IN      VSS_ID          SnapshotSetId
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::PreCommitSnapshots");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");
        }
    VSS_STANDARD_CATCH(ft)
    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::PreFinalCommitSnapshots
    (
    IN      VSS_ID          SnapshotSetId
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::PreFinalCommitSnapshots");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");
        }
    VSS_STANDARD_CATCH(ft)
    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::PostFinalCommitSnapshots
    (
    IN      VSS_ID          SnapshotSetId
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::PostFinalCommitSnapshots");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");
        }
    VSS_STANDARD_CATCH(ft)
    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::CommitSnapshots
    (
    IN      VSS_ID          SnapshotSetId
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::CommitSnapshots");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");

        m_bHidden = true;
        LONG mask = 1;
        for(UINT Disk = 0; mask != 0; mask = mask << 1, Disk++)
            {
            if ((m_maskSnapshotDisks & mask) != 0)
                {
                DWORD diskId;
                bool bIsDynamic;

                if (!TranslateDisk(m_rgDestinationDiskIds[Disk], diskId, bIsDynamic))
                    ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"Unexpected error");

                if (bIsDynamic)
                    HideAndExposeDynamicVolumes(m_rgDestinationDiskIds[Disk], true, false);
                else
                    HideAndExposePartitions(diskId, m_rgcDestinationPartitions[Disk], true, false);
                }
            }
        }
    VSS_STANDARD_CATCH(ft)

    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::PostCommitSnapshots
    (
    IN      VSS_ID          SnapshotSetId,
    IN      LONG            lSnapshotsCount
    )
    {
    UNREFERENCED_PARAMETER(lSnapshotsCount);

    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::PostCommitSnapshots");

    CVssSafeAutomaticLock lock(m_cs);
    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");
        }
    VSS_STANDARD_CATCH(ft)

    m_SnapshotSetId = GUID_NULL;
    m_maskSnapshotDisks = 0;
    m_bHidden = false;

    return ft.hr;
    }

STDMETHODIMP CHardwareTestProvider::AbortSnapshots
    (
    IN      VSS_ID          SnapshotSetId
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::AbortSnapshots");

    try
        {
        if (SnapshotSetId != m_SnapshotSetId)
            ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"snapshot set mismatch");

        if (m_bHidden)
            {
            UINT Disk = 0;
            for(LONG mask = 1; mask != 0; mask = mask << 1)
                {
                if ((m_maskSnapshotDisks & mask) != 0)
                    {
                    DWORD diskId;
                    bool bIsDynamic;

                    if (!TranslateDisk(m_rgDestinationDiskIds[Disk], diskId, bIsDynamic))
                        ft.Throw(VSSDBG_VSSTEST, VSS_E_PROVIDER_VETO, L"Unexpected error");

                    if (bIsDynamic)
                        HideAndExposeDynamicVolumes(m_rgDestinationDiskIds[Disk], false, true);
                    else
                        HideAndExposePartitions(diskId, m_rgcDestinationPartitions[Disk], false, true);
                    }

                Disk++;
                }
            }

        m_bHidden = false;
        m_SnapshotSetId = GUID_NULL;
        m_maskSnapshotDisks = 0;
        }
    VSS_STANDARD_CATCH(ft)


    return ft.hr;
    }

bool CHardwareTestProvider::cmp_str_eq(LPCSTR sz1, LPCSTR sz2)
    {
    if (sz1 == NULL && sz2 == NULL)
        return true;
    else if (sz1 != NULL && sz2 != NULL)
        {
        while(*sz1 == *sz2 && *sz1 != '\0')
            {
            sz1++;
            sz2++;
            }

        if (*sz1 == *sz2)
            return true;

        if (*sz1 == '\0' && *sz2 == ' ')
            {
            while(*sz2 != '\0')
                {
                if (*sz2 != ' ')
                    return false;

                sz2++;
                }

            return true;
            }
        else if (*sz1 == ' ' && *sz2 == ' ')
            {
            while(*sz1 != '\0')
                {
                if (*sz1 != ' ')
                    return false;

                sz1++;
                }

            return true;
            }

        return false;
        }
    else
        return false;
    }




// determine if two VDS_LUN_INFORMATION structures match
bool CHardwareTestProvider::IsMatchLun
    (
    const VDS_LUN_INFORMATION &info1,
    const VDS_LUN_INFORMATION &info2
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::IsMatchLun");

    if (info1.m_DeviceType != info2.m_DeviceType)
        return false;

    if (info1.m_DeviceTypeModifier != info2.m_DeviceTypeModifier)
        return false;

    if (info1.m_BusType != info2.m_BusType)
        return false;

    if (!cmp_str_eq(info1.m_szSerialNumber, info2.m_szSerialNumber))
        return false;

    if (!cmp_str_eq(info1.m_szVendorId, info2.m_szVendorId))
        return false;

    if (!cmp_str_eq(info1.m_szProductId, info2.m_szProductId))
        return false;

    if (!cmp_str_eq(info1.m_szProductRevision, info2.m_szProductRevision))
        return false;

    if (info1.m_diskSignature != info2.m_diskSignature)
        return false;

    if (!IsMatchDeviceIdDescriptor(info1.m_deviceIdDescriptor, info2.m_deviceIdDescriptor))
        return false;

    return true;
    }


// make sure that the storage device id descriptors match
bool CHardwareTestProvider::IsMatchDeviceIdDescriptor
    (
    IN const VDS_STORAGE_DEVICE_ID_DESCRIPTOR &desc1,
    IN const VDS_STORAGE_DEVICE_ID_DESCRIPTOR &desc2
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::IsMatchDeviceIdDescriptor");

    VDS_STORAGE_IDENTIFIER *pId1 = desc1.m_rgIdentifiers;
    VDS_STORAGE_IDENTIFIER *rgId2 = desc2.m_rgIdentifiers;

    for(ULONG iIdentifier = 0; iIdentifier < desc1.m_cIdentifiers; iIdentifier++, pId1++)
        {
        if (IsConflictingIdentifier(pId1, rgId2, desc2.m_cIdentifiers))
            return false;
        }

    return true;
    }

// determine if there is a conflicting identifier in an array of storage
// identifiers
bool CHardwareTestProvider::IsConflictingIdentifier
    (
    IN const VDS_STORAGE_IDENTIFIER *pId1,
    IN const VDS_STORAGE_IDENTIFIER *rgId2,
    IN ULONG cId2
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::IsConflictingIdentifier");

    const VDS_STORAGE_IDENTIFIER *pId2 = rgId2;

    for(UINT iid = 0; iid < cId2; iid++)
        {
        if (pId1->m_Type == pId2->m_Type)
            {
            if (pId1->m_CodeSet != pId2->m_CodeSet ||
                pId1->m_cbIdentifier != pId2->m_cbIdentifier ||
                memcmp(pId1->m_rgbIdentifier, pId2->m_rgbIdentifier, pId1->m_cbIdentifier) != 0)
                return true;
            }
        }

    return false;
    }

// copy a string from a STORAGE_DEVICE_DESCRIPTOR.  It is returned as a
// CoTaskAllocated string.
void CHardwareTestProvider::CopySDString
    (
    LPSTR *ppszNew,
    STORAGE_DEVICE_DESCRIPTOR *pdesc,
    DWORD offset
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::CopySDString");

    // point to start of string
    LPSTR szId = (LPSTR)((BYTE *) pdesc + offset);
    UINT cwc = (UINT) strlen(szId);
    while(cwc > 0 && szId[cwc-1] == ' ')
        cwc--;

    if (cwc == 0)
        *ppszNew = NULL;
    else
        {
        // allocate string
        *ppszNew = (LPSTR) CoTaskMemAlloc(cwc + 1);
        if (*ppszNew == NULL)
            ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Cannot allocate string.");

        // copy value into string
        memcpy(*ppszNew, szId, cwc);
        (*ppszNew)[cwc] = L'\0';
        }
    }



void CHardwareTestProvider::HideAndExposePartitions
    (
    UINT DiskNo,
    DWORD cPartitions,
    bool fHide,
    bool fExpose
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::HideAndExposePartitions");

    HANDLE rgh[256];
    BS_ASSERT(cPartitions < 256);
    for(UINT i = 0; i < cPartitions; i++)
        rgh[i] = INVALID_HANDLE_VALUE;

    try
        {
        WCHAR volName[128];

        for(UINT i = 0; i < cPartitions; i++)
            {
            swprintf
                (
                volName,
                L"\\\\?\\GlobalRoot\\Device\\Harddisk%d\\Partition%d",
                DiskNo,
                i
                );

            rgh[i] = CreateFile
                        (
                        volName,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                        );

            if (rgh[i] == INVALID_HANDLE_VALUE)
                {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_FILE_NOT_FOUND ||
                    dwErr == ERROR_PATH_NOT_FOUND)
                    continue;

                ft.hr = HRESULT_FROM_WIN32(dwErr);
                ft.CheckForError(VSSDBG_VSSTEST, L"CreateFile(VOLUME)");
                }
            }

        DoHideAndExpose(rgh, cPartitions, fHide, fExpose);
        }
    VSS_STANDARD_CATCH(ft)

    for(i = 0; i < cPartitions; i++)
        {
        if (rgh[i] != INVALID_HANDLE_VALUE)
            CloseHandle(rgh[i]);
        }

    }

void CHardwareTestProvider::HideAndExposeDynamicVolumes
    (
    DWORD signature,
    bool fHide,
    bool fExpose
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::HideAndExposeDynamicVolumes");

    HANDLE rgh[256];
    for(UINT i = 0; i < 256; i++)
        rgh[i] = INVALID_HANDLE_VALUE;

    UINT cVolumes = 0;

    try
        {
        DYNAMIC_DISK_INFO *pInfo = m_pDynamicDiskInfo;
        while(pInfo)
            {
            if (pInfo->m_signature == signature)
                break;

            pInfo = pInfo->m_next;
            }

        // if no information is found, then all volumes on the
        // disk are already hidden
        if (!pInfo)
            return;


        LPCWSTR wszVolumes = pInfo->m_wszVolumes;

        while(*wszVolumes)
            {
            BS_ASSERT(cVolumes < 256);
            rgh[cVolumes] = CreateFile
                                (
                                wszVolumes,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL
                                );

            if (rgh[cVolumes] == INVALID_HANDLE_VALUE)
                {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_FILE_NOT_FOUND ||
                    dwErr == ERROR_PATH_NOT_FOUND)
                    {
                    wszVolumes += wcslen(wszVolumes) + 1;
                    continue;
                    }

                ft.hr = HRESULT_FROM_WIN32(dwErr);
                ft.CheckForError(VSSDBG_VSSTEST, L"CreateFile(VOLUME)");
                }

            wszVolumes += wcslen(wszVolumes) + 1;
            cVolumes++;
            }

        DoHideAndExpose(rgh, cVolumes, fHide, fExpose);
        }
    VSS_STANDARD_CATCH(ft)

    for(i = 0; i < cVolumes; i++)
        {
        if (rgh[i] != INVALID_HANDLE_VALUE)
            CloseHandle(rgh[i]);
        }

    }



void CHardwareTestProvider::DoHideAndExpose
    (
    HANDLE *rgh,
    UINT cHandles,
    bool fHide,
    bool fExpose
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::DoHideAndExpose");

    DWORD size;
    VOLUME_GET_GPT_ATTRIBUTES_INFORMATION   getAttributesInfo;

    for(UINT i = 0; i < cHandles; i++)
        {
        if (!DeviceIoControl
                (
                rgh[i],
                IOCTL_VOLUME_GET_GPT_ATTRIBUTES,
                NULL,
                0,
                &getAttributesInfo,
                sizeof(getAttributesInfo),
                &size,
                NULL
                ))
            {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_INVALID_FUNCTION)
                continue;

            ft.hr = HRESULT_FROM_WIN32(dwErr);
            ft.CheckForError(VSSDBG_VSSTEST, L"DeviceIoControl(IOCTL_VOLUME_GET_GPT_ATTRIBUTES)");
            }

        if (fHide &&
            (getAttributesInfo.GptAttributes & GPT_BASIC_DATA_ATTRIBUTE_READ_ONLY) == 0)
            {
            // dismount volume before seting read-only bits
            if (!DeviceIoControl
                    (
                    rgh[i],
                    FSCTL_DISMOUNT_VOLUME,
                    NULL,
                    0,
                    NULL,
                    0,
                    &size,
                    NULL
                    ))
                {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_INVALID_FUNCTION)
                    continue;

                ft.hr = HRESULT_FROM_WIN32(dwErr);
                ft.CheckForError(VSSDBG_VSSTEST, L"DeviceIoControl(IOCTL_VOLUME_SET_GPT_ATTRIBUTES)");
                }
            }
        }

    for(i = 0; i < cHandles; i++)
        {
        if (rgh[i] != INVALID_HANDLE_VALUE)
            {
            DWORD size;
            VOLUME_GET_GPT_ATTRIBUTES_INFORMATION   getAttributesInfo;
            VOLUME_SET_GPT_ATTRIBUTES_INFORMATION setAttributesInfo;

            if (!DeviceIoControl
                    (
                    rgh[i],
                    IOCTL_VOLUME_GET_GPT_ATTRIBUTES,
                    NULL,
                    0,
                    &getAttributesInfo,
                    sizeof(getAttributesInfo),
                    &size,
                    NULL
                    ))
                {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_INVALID_FUNCTION)
                    continue;

                ft.hr = HRESULT_FROM_WIN32(dwErr);
                ft.CheckForError(VSSDBG_VSSTEST, L"DeviceIoControl(IOCTL_VOLUME_GET_GPT_ATTRIBUTES)");
                }

            memset(&setAttributesInfo, 0, sizeof(setAttributesInfo));
            setAttributesInfo.GptAttributes = getAttributesInfo.GptAttributes;
            setAttributesInfo.ApplyToAllConnectedVolumes = TRUE;
            if (fHide)
                {
                setAttributesInfo.GptAttributes |= GPT_BASIC_DATA_ATTRIBUTE_HIDDEN|GPT_BASIC_DATA_ATTRIBUTE_READ_ONLY|GPT_BASIC_DATA_ATTRIBUTE_NO_DRIVE_LETTER;
                if (!DeviceIoControl
                        (
                        rgh[i],
                        IOCTL_VOLUME_SET_GPT_ATTRIBUTES,
                        &setAttributesInfo,
                        sizeof(setAttributesInfo),
                        NULL,
                        0,
                        &size,
                        NULL
                        ))
                    {
                    DWORD dwErr = GetLastError();
                    if (dwErr == ERROR_INVALID_FUNCTION)
                        continue;

                    ft.hr = HRESULT_FROM_WIN32(dwErr);
                    ft.CheckForError(VSSDBG_VSSTEST, L"DeviceIoControl(IOCTL_VOLUME_SET_GPT_ATTRIBUTES)");
                    }
                }

            if (fExpose)
                {
                setAttributesInfo.GptAttributes &= ~GPT_BASIC_DATA_ATTRIBUTE_HIDDEN;
                if (!DeviceIoControl
                        (
                        rgh[i],
                        IOCTL_VOLUME_SET_GPT_ATTRIBUTES,
                        &setAttributesInfo,
                        sizeof(setAttributesInfo),
                        NULL,
                        0,
                        &size,
                        NULL
                        ))
                    {
                    ft.hr = HRESULT_FROM_WIN32(GetLastError());
                    ft.CheckForError(VSSDBG_VSSTEST, L"DeviceIoControl(IOCTL_VOLUME_SET_GPT_ATTRIBUTES)");
                    }
                }
            }
        }
    }


// copy a storage device id descriptor to the VDS_LUN_INFORMATION structure.
// note that lun is modified whether an execption occurs or not.  FreeLunInfo
// will free up any data allocated here.
void CHardwareTestProvider::CopyStorageDeviceIdDescriptorToLun
    (
    IN STORAGE_DEVICE_ID_DESCRIPTOR *pDevId,
    IN VDS_LUN_INFORMATION *pLun
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::CopyStorageDeviceIdDescriptorToLun");

    BS_ASSERT(StorageIdTypeVendorSpecific == VDSStorageIdTypeVendorSpecific);
    BS_ASSERT(StorageIdTypeVendorId == VDSStorageIdTypeVendorId);
    BS_ASSERT(StorageIdTypeEUI64 == VDSStorageIdTypeEUI64);
    BS_ASSERT(StorageIdTypeFCPHName == VDSStorageIdTypeFCPHName);
    BS_ASSERT(StorageIdCodeSetAscii == VDSStorageIdCodeSetAscii);
    BS_ASSERT(StorageIdCodeSetBinary == VDSStorageIdCodeSetBinary);

    // get count of ids
    DWORD cIds = pDevId->NumberOfIdentifiers;

    // copy over version number and count of ids
    pLun->m_deviceIdDescriptor.m_version = pDevId->Version;
    pLun->m_deviceIdDescriptor.m_cIdentifiers = cIds;

    // allocate array of identifiers
    pLun->m_deviceIdDescriptor.m_rgIdentifiers =
        (VDS_STORAGE_IDENTIFIER *) CoTaskMemAlloc(sizeof(VDS_STORAGE_IDENTIFIER) * cIds);

    if (pLun->m_deviceIdDescriptor.m_rgIdentifiers == NULL)
        ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Cannot allocate device id descriptors");

    // clear descriptor.  If we throw, we want to make sure that all pointers
    // are null
    memset(pLun->m_deviceIdDescriptor.m_rgIdentifiers, 0, sizeof(VDS_STORAGE_IDENTIFIER) * cIds);

    // get pointer to first identifier in both STORAGE_DEVICE_ID_DESCRIPTOR and
    // VDS_STORAGE_DEVICE_ID_DESCRIPTOR
    STORAGE_IDENTIFIER *pId = (STORAGE_IDENTIFIER *) pDevId->Identifiers;
    VDS_STORAGE_IDENTIFIER *pvsi = (VDS_STORAGE_IDENTIFIER *) pLun->m_deviceIdDescriptor.m_rgIdentifiers;

    for(UINT i = 0; i < cIds; i++)
        {
        // copy over size of identifier
        pvsi->m_cbIdentifier = pId->IdentifierSize;

        // allocate space for identifier
        pvsi->m_rgbIdentifier = (BYTE *) CoTaskMemAlloc(pvsi->m_cbIdentifier);
        if (pvsi->m_rgbIdentifier == NULL)
            ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Cannot allocate storage identifier");

        // copy type and code set over
        pvsi->m_Type = (VDS_STORAGE_IDENTIFIER_TYPE) pId->Type;
        pvsi->m_CodeSet = (VDS_STORAGE_IDENTIFIER_CODE_SET) pId->CodeSet;

        // copy identifier
        memcpy(pvsi->m_rgbIdentifier, pId->Identifier, pvsi->m_cbIdentifier);

        // move to next identifier
        pId = (STORAGE_IDENTIFIER *) ((BYTE *) pId + pId->NextOffset);
        pvsi++;
        }
    }

void CHardwareTestProvider::RebuildLunInfo()
    {
    for(DWORD iDisk = 0; iDisk < m_cDiskIds; iDisk++)
        {
        DWORD diskId;
        bool bIsDynamic;

        if (m_rgpSourceLuns[iDisk] == NULL && TranslateDisk(m_rgSourceDiskIds[iDisk], diskId, bIsDynamic))
            BuildLunInfoForDisk(diskId, &m_rgpSourceLuns[iDisk], &m_rgcSourcePartitions[iDisk]);

        if (m_rgpDestinationLuns[iDisk] == NULL && TranslateDisk(m_rgDestinationDiskIds[iDisk], diskId, bIsDynamic))
            BuildLunInfoForDisk(diskId, &m_rgpDestinationLuns[iDisk], &m_rgcDestinationPartitions[iDisk]);
        }
    }

DWORD hexconvert(LPCWSTR wsz)
    {
    DWORD dwRet = 0;
    while (*wsz != L'\0')
        {
        dwRet *= 16;
        if (*wsz >= L'0' && *wsz <= L'9')
            dwRet += *wsz - L'0';
        else if (*wsz >= L'a' && *wsz <= L'f')
            dwRet += *wsz - L'a' + 10;
        else if (*wsz >= L'A' && *wsz <= L'F')
            dwRet += *wsz - L'A' + 10;

        wsz++;
        }

    return dwRet;
    }



void CHardwareTestProvider::LoadConfigurationData()
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::LoadConfigurationData");

    VSS_PWSZ wszSourceDisks = NULL;
    VSS_PWSZ wszDestinationDisks = NULL;
    m_cDiskIds = 0;
    try
        {
        CVssRegistryKey key;
        key.Open(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\HardwareTestProvider");
        key.GetValue(L"SourceDisks", wszSourceDisks);
        key.GetValue(L"DestinationDisks", wszDestinationDisks);
        LPWSTR wszSource = wszSourceDisks;
        LPWSTR wszDestination = wszDestinationDisks;

        if (wszSource != NULL && wszDestination != NULL)
            {
            while(*wszSource != L'\0' && *wszDestination != L'\0')
                {
                WCHAR *wszSourceNext = wcschr(wszSource, L',');
                WCHAR *wszDestinationNext = wcschr(wszDestination, L',');
                if (wszSourceNext != NULL)
                    *wszSourceNext = L'\0';

                if (wszDestinationNext != NULL)
                    *wszDestinationNext = L'\0';

                int Source = hexconvert(wszSource);
                int Destination = hexconvert(wszDestination);
                m_rgSourceDiskIds[m_cDiskIds] = Source;
                m_rgDestinationDiskIds[m_cDiskIds] = Destination;
                m_cDiskIds++;

                if (m_cDiskIds == 32)
                    break;

                if (wszSourceNext == NULL || wszDestinationNext == NULL)
                    break;

                wszSource = wszSourceNext + 1;
                wszDestination = wszDestinationNext + 1;
                }
            }

        RebuildSignatures();
        m_bRebuilt = true;
        RebuildLunInfo();
        }
    VSS_STANDARD_CATCH(ft)

    if (ft.HrFailed())
        ClearConfiguration();

    if (wszSourceDisks)
        CoTaskMemFree(wszSourceDisks);

    if (wszDestinationDisks)
        CoTaskMemFree(wszDestinationDisks);
    }



void CHardwareTestProvider::BuildLunInfoForDisk
    (
    IN DWORD Drive,
    OUT VDS_LUN_INFORMATION **ppLunInfo,
    OUT DWORD *pcPartitions
    )
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::BuildLunInfoForDisk");

    WCHAR wszbuf[32];
    swprintf(wszbuf, L"\\\\.\\PHYSICALDRIVE%u", Drive);
    CVssAutoWin32Handle hDisk = CreateFile
                    (
                    wszbuf,
                    GENERIC_READ|GENERIC_WRITE,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL
                    );

    if (hDisk != INVALID_HANDLE_VALUE)
        BuildLunInfoFromDrive(hDisk, ppLunInfo, *pcPartitions);
    }

void CHardwareTestProvider::ClearConfiguration()
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::ClearConfiguration");

    for(DWORD i = 0; i < m_cDiskIds; i++)
        {
        if (m_rgpSourceLuns[i] != NULL)
            {
            FreeLunInfo(m_rgpSourceLuns[i], 1);
            m_rgpSourceLuns[i] = NULL;
            }

        if (m_rgpDestinationLuns[i] != NULL)
            {
            FreeLunInfo(m_rgpDestinationLuns[i], 1);
            m_rgpDestinationLuns[i] = NULL;
            }
        }

    m_cDiskIds = 0;
    }

// add a lun to a file indicating the luns that were freed
void CHardwareTestProvider::AddFreedLun(LPCWSTR wsz)
    {
    DWORD Drive;
    swscanf(wsz, L"\\\\.\\PHYSICALDRIVE%u", &Drive);

    WCHAR buf[MAX_PATH * 2];
    if (!GetSystemDirectory(buf, sizeof(buf)/sizeof(WCHAR)))
        {
        wprintf(L"GetSystemDirectory failed due to error %d.\n", GetLastError());
        throw E_UNEXPECTED;
        }

    wcscat(buf, L"\\VSNAP_DELETED_LUNS");

    HANDLE h = CreateFile
                    (
                    buf,
                    GENERIC_READ|GENERIC_WRITE,
                    FILE_SHARE_READ,
                    NULL,
                    OPEN_ALWAYS,
                    0,
                    NULL
                    );

    if (h == INVALID_HANDLE_VALUE)
        return;

    if (SetFilePointer(h, 0, NULL, FILE_END) != 0xffffffff)
        {
        DWORD dwWritten;
        WriteFile(h, &Drive, sizeof(DWORD), &dwWritten, NULL);
        }

    CloseHandle(h);
    }

bool CHardwareTestProvider::TranslateDisk(DWORD signature, DWORD &DiskId, bool &bIsDynamic)
    {
    while(TRUE)
        {
        for(UINT iDisk = 0; iDisk < MAXDISKS; iDisk++)
            {
            if (m_rgDiskSignatures[iDisk] == signature)
                {
                if (GetDiskSignature(iDisk, bIsDynamic) != signature)
                    break;

                DiskId = iDisk;
                return true;
                }
            }

        // rebuild signatures if we haven't done so already
        if (m_bRebuilt)
            break;

        RebuildSignatures();
        m_bRebuilt = true;
        }

    return false;
    }

DWORD CHardwareTestProvider::GetDiskSignature(DWORD iDisk, bool &bIsDynamic)
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::GetDiskSignature");

    LPBYTE buf = NULL;
    DWORD cbOut = 1024;

    bIsDynamic = false;

    WCHAR wszBuf[64];
    swprintf(wszBuf, L"\\\\.\\PHYSICALDRIVE%u", iDisk);

    CVssAutoWin32Handle hDisk = CreateFile
                                (
                                wszBuf,
                                GENERIC_READ|GENERIC_WRITE,
                                FILE_SHARE_READ,
                                NULL,
                                OPEN_EXISTING,
                                0,
                                NULL
                                );

    if (hDisk == INVALID_HANDLE_VALUE)
        return 0;

    if (!DoDeviceIoControl
            (
            hDisk,
            IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
            NULL,
            0,
            &buf,
            &cbOut
            ))
        return 0;

    DRIVE_LAYOUT_INFORMATION_EX *pLayout = (DRIVE_LAYOUT_INFORMATION_EX *) buf;
    ULONG cPartitions = pLayout->PartitionCount;
    for(ULONG iPartition = 0; iPartition < cPartitions; iPartition++)
        {
        if (pLayout->PartitionEntry[iPartition].Mbr.PartitionType == PARTITION_LDM)
            bIsDynamic = true;
        }

    if (pLayout->PartitionStyle == PARTITION_STYLE_MBR)
        return pLayout->Mbr.Signature;
    else
        return 0;

    }


void CHardwareTestProvider::RebuildSignatures()
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::RebuildSignatures");

    for(UINT iDisk = 0; iDisk < MAXDISKS; iDisk++)
        {
        bool bIsDynamic;

        m_rgDiskSignatures[iDisk] = GetDiskSignature(iDisk, bIsDynamic);
        if (bIsDynamic)
            AddDynamicDiskInfo(m_rgDiskSignatures[iDisk], iDisk);
        }

    }

void CHardwareTestProvider::AddDynamicDiskInfo(DWORD signature, DWORD disk)
    {
    CVssFunctionTracer ft(VSSDBG_VSSTEST, L"CHardwareTestProvider::AddDynamicDiskInfo");

    DYNAMIC_DISK_INFO *pInfo = m_pDynamicDiskInfo;
    while(pInfo != NULL)
        {
        if (pInfo->m_signature == signature)
            break;

        pInfo = pInfo->m_next;
        }

    // dynamic volumes already found, do nothing
    if (pInfo)
        return;

    LPBYTE bufExtents = NULL;
    DWORD cbBufExtents = 1024;

    LPWSTR buf = new WCHAR[8192];
    if (buf == NULL)
        ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Cannot allocate string");

    LPWSTR wszVolumes = new WCHAR[256];
    if (wszVolumes == NULL)
        {
        delete buf;
        ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"Cannot allocate string");
        }

    wszVolumes[0] = L'\0';
    UINT cwcVolumes = 256;
    UINT iwcVolumes = 0;


    HANDLE hFind = FindFirstVolume(buf, 8192);
    try
        {
        if (hFind == INVALID_HANDLE_VALUE)
            {
            ft.hr = HRESULT_FROM_WIN32(GetLastError());
            ft.CheckForError(VSSDBG_VSSTEST, L"FindVolume");
            }

        while(TRUE)
            {
            // get rid of trailing backslash
            buf[wcslen(buf) - 1] = L'\0';
            CVssAutoWin32Handle hVol = CreateFile
                                        (
                                        buf,
                                        GENERIC_READ,
                                        FILE_SHARE_READ|FILE_SHARE_WRITE,
                                        NULL,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL
                                        );


            DWORD dwErr = GetLastError();

            if (hVol != INVALID_HANDLE_VALUE)
                {
                if (DoDeviceIoControl
                        (
                        hVol,
                        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                        NULL,
                        0,
                        &bufExtents,
                        &cbBufExtents
                        ))
                    {
                    VOLUME_DISK_EXTENTS *pDiskExtents = (VOLUME_DISK_EXTENTS *) bufExtents;
                    DWORD cExtents = pDiskExtents->NumberOfDiskExtents;
                    for(DWORD iExtent = 0; iExtent < cExtents; iExtent++)
                        {
                        if (pDiskExtents->Extents[iExtent].DiskNumber == disk)
                            {
                            UINT cwc = (UINT) wcslen(buf) + 1;
                            if (cwc + iwcVolumes + 1 > cwcVolumes)
                                {
                                LPWSTR wszNew = new WCHAR[cwc + iwcVolumes + 256];
                                if (wszNew == NULL)
                                    ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"cannot grow string");

                                wcscpy(wszNew, wszVolumes);
                                delete wszVolumes;
                                wszNew = wszVolumes;
                                cwcVolumes = iwcVolumes + cwc + 256;
                                }

                            wcscpy(wszVolumes + iwcVolumes, buf);
                            iwcVolumes += cwc;

                            // double null terminate MULTI_SZ string
                            wszVolumes[iwcVolumes + 1] = L'\0';
                            break;
                            }
                        }
                    }
                }

            if (!FindNextVolume(hFind, buf, 8192))
                {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_NO_MORE_FILES)
                    break;

                ft.hr = HRESULT_FROM_WIN32(dwErr);
                ft.CheckForError(VSSDBG_VSSTEST, L"FindNextVolume");
                }
            }
        }
    catch(...)
        {
        delete bufExtents;
        delete buf;
        delete wszVolumes;
        FindVolumeClose(hFind);
        throw;
        }

    delete bufExtents;
    delete buf;
    FindVolumeClose(hFind);

    if (wcslen(wszVolumes) == 0)
        delete wszVolumes;
    else
        {
        DYNAMIC_DISK_INFO *pInfo = new DYNAMIC_DISK_INFO;
        if (pInfo == NULL)
            {
            delete wszVolumes;
            ft.Throw(VSSDBG_VSSTEST, E_OUTOFMEMORY, L"cannot allocate DYNAMIC_DISK_INFO");
            }

        pInfo->m_signature = signature;
        pInfo->m_wszVolumes = wszVolumes;
        pInfo->m_next = m_pDynamicDiskInfo;
        m_pDynamicDiskInfo = pInfo;
        }
    }



