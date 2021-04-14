/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:

    svc.hxx

Abstract:

    Includes the class definition of CVsServiceModule which manages the SCM service.

Author:

    Adi Oltean  [aoltean]   06/30/1999

Revision History:

    Name        Date        Comments

    aoltean     06/30/1999  Created.
    aoltean     07/23/1999  Changing the AppId
    aoltean     09/09/1999  dss -> vss

--*/

#ifndef __VSS_SVC_HXX__
#define __VSS_SVC_HXX__

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
#define VSS_FILE_ALIAS "CORSVCH"
//
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//  Defines

//
//  Our server is free threaded. With these macros we define the standard CComObjectThreadModel
//  and CComGlobalsThreadModel as being the CComMultiThreadModel.
//  Also the CComObjectRoot class will be defined as CComObjectRootEx<CComMultiThreadModel>
//  Warning: these macros does not affect the threading model of our server!
//  (which is specified by CoInitializeEx)
//
#undef  _ATL_SINGLE_THREADED
#undef  _ATL_APARTMENT_THREADED
#define _ATL_FREE_THREADED

// VssApi shim exports - These are only used by VssSvc.
typedef HRESULT ( APIENTRY *PFunc_SimulateSnapshotFreezeInternal ) (
    IN GUID     guidSnapshotSetId,
    IN ULONG    ulOptionFlags,
    IN ULONG    ulVolumeCount,
    IN LPWSTR  *ppwszVolumeNamesArray,
    IN volatile bool *pbCancelAsync
    );

typedef HRESULT ( APIENTRY *PFunc_SimulateSnapshotThawInternal ) (
    IN GUID guidSnapshotSetId
    );

__declspec(dllexport)
HRESULT APIENTRY RegisterSnapshotSubscriptions(
    OUT PFunc_SimulateSnapshotFreezeInternal *ppFuncFreeze,
    OUT PFunc_SimulateSnapshotThawInternal *ppFuncThaw
    );

__declspec(dllexport)
HRESULT APIENTRY UnregisterSnapshotSubscriptions(void);

////////////////////////////////////////////////////////////////////////
//  Global constants
//

const WCHAR   x_wszServiceName[]        = L"VSS";



////////////////////////////////////////////////////////////////////////
//  Defined classes
//

class CVsServiceModule :
    public CComModule
{
// Constructors/destructors
private:
    CVsServiceModule(const CVsServiceModule&);

public:

    CVsServiceModule();

// Setup
protected:

    HRESULT RegisterServer(
        IN BOOL bRegTypeLib
        );

    void CreateLaunchPermissions();

// Logging
public:

    void WaitForSubscribingCompletion() throw(HRESULT);

// Service-related operations
protected:

    void StartDispatcher();

    static void WINAPI _ServiceMain(
        IN DWORD dwArgc,
        IN LPTSTR* lpszArgv
        );

    void ServiceMain(
        IN DWORD dwArgc,
        IN LPTSTR* lpszArgv
        );

    static DWORD WINAPI _Handler(
        IN DWORD dwOpcode,
        IN DWORD dwEventType,
        LPVOID lpEventData,
        LPVOID lpEventContext
        );

    DWORD Handler(
        IN DWORD dwOpcode,
        IN DWORD dwEventType,
        LPVOID lpEventData,
        LPVOID lpEventContext
        );

    void SetServiceStatus(
        IN  DWORD dwState,
        IN  bool bThrowOnError = true
        ) throw(HRESULT);

    HINSTANCE   GetInstance() const { return m_hInstance; };

// Event handlers
protected:

    virtual void OnInitializing() throw(HRESULT);

    virtual void OnRunning() throw(HRESULT);

    virtual void OnStopping();

    virtual void OnSignalShutdown();

// Entry point
public:

    int WINAPI _WinMain(
        IN HINSTANCE hInstance,
        IN LPTSTR lpCmdLine
        );

// Shutdown control
public:

    bool IsDuringShutdown() const { return m_bShutdownInProgress; };

// Life-management control (used in CComObject constructors and destructors)
public:

    LONG Lock();

    LONG Unlock();

// Simulate snapshot control
public:
    void GetSimulateFunctions(
        OUT PFunc_SimulateSnapshotFreezeInternal *ppvSimulateFreeze,
        OUT PFunc_SimulateSnapshotThawInternal *ppvSimulateThaw
        );

// Data members
protected:

    SERVICE_STATUS_HANDLE m_hServiceStatus;
    HANDLE m_hDiskNotify;
    SERVICE_STATUS m_status;
    DWORD   m_dwThreadID;
    CVssAutoWin32HandleNullChecked m_hShutdownTimer;   // the "idle timeout" timer
    volatile bool   m_bShutdownInProgress;
    HANDLE  m_hSubscriptionsInitializeEvent;
    bool    m_bCOMStarted;
    HINSTANCE m_hInstance;
    PFunc_SimulateSnapshotFreezeInternal m_pvFuncSimulateSnapshotFreezeInternal;
    PFunc_SimulateSnapshotThawInternal m_pvFuncSimulateSnapshotThawInternal;
};


// This is the new creator class that fails during shutdown.
// This class is used in OBJECT_ENTRY macro in svc.cxx
template <class T, const CLSID* pclsid = &CLSID_NULL>
class _VssCreatorClass
{
public:
    static HRESULT WINAPI CreateInstance(void* pv, REFIID riid, LPVOID* ppv)
    {
        CVssFunctionTracer ft( VSSDBG_COORD, L"_VssCreatorClass::CreateInstance");

        ft.Trace(VSSDBG_COORD, L"Creating an instance with class ID " WSTR_GUID_FMT, GUID_PRINTF_ARG(*pclsid));

        // Create the instance
        ft.hr = CComCoClass<T, pclsid>::_CreatorClass::CreateInstance(pv, riid, ppv);

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

//
//  You may derive a class from CComModule and use it if you want to override
//  something, but do not change the name of _Module
//

extern CVsServiceModule _Module;
#include <atlcom.h>


#endif // __VSS_SVC_HXX__

