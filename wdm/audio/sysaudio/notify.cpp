//---------------------------------------------------------------------------
//
//  Module:   notify.cpp
//
//  Description:
//
//
//@@BEGIN_MSINTERNAL
//  Development Team:
//     Mike McLaughlin
//
//  History:   Date	  Author      Comment
//
//  To Do:     Date	  Author      Comment
//
//@@END_MSINTERNAL
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1996-1999 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

#include "common.h"

//
// Include safe string library for safe string manipulation. 
//
#define STRSAFE_NO_DEPRECATE // Use safe and unsafe functions interchangeably.
#include "strsafe.h"

#define DEVICE_NAME_TAG         L"\\\\?\\"

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

CONST GUID *apguidCategories[] = {
    &KSCATEGORY_AUDIO,
    &KSCATEGORY_AUDIO_GFX,
    &KSCATEGORY_TOPOLOGY,
    &KSCATEGORY_BRIDGE,
    &KSCATEGORY_RENDER,
    &KSCATEGORY_CAPTURE,
    &KSCATEGORY_MIXER,
    &KSCATEGORY_DATATRANSFORM,
    &KSCATEGORY_ACOUSTIC_ECHO_CANCEL,
    &KSCATEGORY_INTERFACETRANSFORM,
    &KSCATEGORY_MEDIUMTRANSFORM,
    &KSCATEGORY_DATACOMPRESSOR,
    &KSCATEGORY_DATADECOMPRESSOR,
    &KSCATEGORY_COMMUNICATIONSTRANSFORM,
    &KSCATEGORY_SPLITTER,
    &KSCATEGORY_AUDIO_SPLITTER,
    &KSCATEGORY_SYNTHESIZER,
    &KSCATEGORY_DRM_DESCRAMBLE,
    &KSCATEGORY_MICROPHONE_ARRAY_PROCESSOR,
};

ULONG aulFilterType[] = {
    FILTER_TYPE_AUDIO,
    FILTER_TYPE_GFX,
    FILTER_TYPE_TOPOLOGY,
    FILTER_TYPE_BRIDGE,
    FILTER_TYPE_RENDERER,
    FILTER_TYPE_CAPTURER,
    FILTER_TYPE_MIXER,
    FILTER_TYPE_DATA_TRANSFORM,
    FILTER_TYPE_AEC,
    FILTER_TYPE_INTERFACE_TRANSFORM,
    FILTER_TYPE_MEDIUM_TRANSFORM,
    FILTER_TYPE_DATA_TRANSFORM,
    FILTER_TYPE_DATA_TRANSFORM,
    FILTER_TYPE_COMMUNICATION_TRANSFORM,
    FILTER_TYPE_SPLITTER,
    FILTER_TYPE_SPLITTER,
    FILTER_TYPE_SYNTHESIZER,
    FILTER_TYPE_DRM_DESCRAMBLE,
    FILTER_TYPE_MIC_ARRAY_PROCESSOR,
};

PVOID pNotificationHandle = NULL;

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

NTSTATUS
RegisterForPlugPlayNotifications(
)
{
    NTSTATUS Status;

    DPF(50, "RegisterForPlugPlayNotifications");
    ASSERT(gpDeviceInstance != NULL);
    ASSERT(gpDeviceInstance->pPhysicalDeviceObject != NULL);

    Status = IoRegisterPlugPlayNotification(
      EventCategoryDeviceInterfaceChange,
      PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
      (LPGUID)&KSCATEGORY_AUDIO,
      gpDeviceInstance->pPhysicalDeviceObject->DriverObject,
      (NTSTATUS (*)(PVOID, PVOID)) AudioDeviceInterfaceNotification,
      NULL,
      &pNotificationHandle);

    if(!NT_SUCCESS(Status)) {
        Trap();
        goto exit;
    }

exit:
    return(Status);
}

VOID
UnregisterForPlugPlayNotifications(
)
{
    if(pNotificationHandle != NULL) {
        IoUnregisterPlugPlayNotification(pNotificationHandle);
    }
}

VOID
DecrementAddRemoveCount(
)
{
    if(InterlockedDecrement(&glPendingAddDelete) == 0) {
        DPF(50, "DecrementAddRemoveCount: sending event");
        KsGenerateEventList(
          NULL,
          KSEVENT_SYSAUDIO_ADDREMOVE_DEVICE,
          &gEventQueue,
          KSEVENTS_SPINLOCK,
          &gEventLock);
    }
}

NTSTATUS
AddFilterWorker(
    PWSTR pwstrDeviceInterface,
    PVOID pReference
)
{
    AddFilter(pwstrDeviceInterface, NULL);
    ExFreePool(pwstrDeviceInterface);
    DecrementAddRemoveCount();

    // Dereference sysaudio PDO.
    KsDereferenceSoftwareBusObject(gpDeviceInstance->pDeviceHeader);

    return(STATUS_SUCCESS);
}

NTSTATUS
DeleteFilterWorker(
    PWSTR pwstrDeviceInterface,
    PVOID pReference
)
{
    DeleteFilter(pwstrDeviceInterface);
    ExFreePool(pwstrDeviceInterface);
    DecrementAddRemoveCount();

    // Dereference sysaudio PDO.
    KsDereferenceSoftwareBusObject(gpDeviceInstance->pDeviceHeader);
    
    return(STATUS_SUCCESS);
}

NTSTATUS
AudioDeviceInterfaceNotification(
    IN PDEVICE_INTERFACE_CHANGE_NOTIFICATION pNotification,
    IN PVOID Context
)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWSTR pwstrDeviceInterface;

    DPF1(50, "AudioDeviceInterfaceNotification: (%s)",
      DbgUnicode2Sz(pNotification->SymbolicLinkName->Buffer));

    //
    // SECURITY NOTE:
    // We trust the Buffer, because it is passed to us as part of notification
    // from PnP subsystem.
    //
    pwstrDeviceInterface = (PWSTR)
        ExAllocatePoolWithTag(
            PagedPool,
            (wcslen(pNotification->SymbolicLinkName->Buffer) + 1) * sizeof(WCHAR),
            POOLTAG_SYSA);
    if(pwstrDeviceInterface == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    // The notification sends null terminated unicode strings
    wcscpy(pwstrDeviceInterface, pNotification->SymbolicLinkName->Buffer);

    if(IsEqualGUID(&pNotification->Event, &GUID_DEVICE_INTERFACE_ARRIVAL)) {
        //
        // Keep a reference so that SWENUM does not REMOVE the device
        // when the Worker thread is running.
        // If the thread is scheduled successfully, it will remove the reference
        // when exiting.
        //
        Status = KsReferenceSoftwareBusObject(gpDeviceInstance->pDeviceHeader);
        if(!NT_SUCCESS(Status)) {
            goto exit;
        }

        InterlockedIncrement(&glPendingAddDelete);
        Status = QueueWorkList(
          (UTIL_PFN)AddFilterWorker,
          pwstrDeviceInterface,
          NULL);
        if (!NT_SUCCESS(Status)) {
            KsDereferenceSoftwareBusObject(gpDeviceInstance->pDeviceHeader);

        }
    }
    else if(IsEqualGUID(&pNotification->Event, &GUID_DEVICE_INTERFACE_REMOVAL)) {
        //
        // Keep a reference so that SWENUM does not REMOVE the device
        // when the Worker thread is running.
        // If the thread is scheduled successfully, it will remove the reference
        // when exiting.
        //
        Status = KsReferenceSoftwareBusObject(gpDeviceInstance->pDeviceHeader);
        if(!NT_SUCCESS(Status)) {
            goto exit;
        }

        InterlockedIncrement(&glPendingAddDelete);
        Status = QueueWorkList(
          (UTIL_PFN)DeleteFilterWorker,
          pwstrDeviceInterface,
          NULL);
        if (!NT_SUCCESS(Status)) {
            KsDereferenceSoftwareBusObject(gpDeviceInstance->pDeviceHeader);
        }
    }
    else {
        //
        // SECURITY NOTE:
        // Sysaudio is registering only for EventCategoryDeviceInterfaceChange.
        // This should send ARRIVAL and REMOVAL.
        // If anything else comes up, we will return SUCCESS.
        // However we are making sure that pwstrDeviceInterface is not leaked.
        //
        if (pwstrDeviceInterface) {
            ExFreePool(pwstrDeviceInterface);
            pwstrDeviceInterface = NULL;
        }
    }

exit:
    if (!NT_SUCCESS(Status))
    {
        if (pwstrDeviceInterface) {
            ExFreePool(pwstrDeviceInterface);
            pwstrDeviceInterface = NULL;
        }
    }
    
    return(Status);
}


NTSTATUS
AddFilter(
    PWSTR pwstrDeviceInterface,
    PFILTER_NODE *ppFilterNode	// if !NULL, physical connection addfilter
)
{
    PFILTER_NODE pFilterNodeDuplicate = NULL;
    PFILTER_NODE pFilterNode = NULL;
    UNICODE_STRING ustrFilterName;
    UNICODE_STRING ustrAliasName;
    UNICODE_STRING ustrName;
    NTSTATUS Status;
    ULONG fulType;
    int i;

    DPF1(50, "AddFilter: (%s)", DbgUnicode2Sz(pwstrDeviceInterface));

    fulType = 0;
    RtlInitUnicodeString(&ustrFilterName, pwstrDeviceInterface);
    
    //
    // For each Interface in apguidCategories, get interface alias of 
    // the new device. Check for duplicate interfaces.
    //
    for(i = 0; i < SIZEOF_ARRAY(apguidCategories); i++) {
        Status = IoGetDeviceInterfaceAlias(
          &ustrFilterName,
          apguidCategories[i],
          &ustrAliasName);

        if(NT_SUCCESS(Status)) {
            HANDLE hAlias;

            Status = OpenDevice(ustrAliasName.Buffer, &hAlias); 

            if(NT_SUCCESS(Status)) {
                DPF2(100, "AddFilter: alias (%s) aulFilterType %08x",
                  DbgUnicode2Sz(ustrAliasName.Buffer),
                  aulFilterType[i]);

                fulType |= aulFilterType[i];
                ZwClose(hAlias);

                if(pFilterNodeDuplicate == NULL) {
                    FOR_EACH_LIST_ITEM(gplstFilterNode, pFilterNode) {
                        if(pFilterNode->GetDeviceInterface() == NULL) {
                            continue;
                        }
                        RtlInitUnicodeString(
                          &ustrName,
                          pFilterNode->GetDeviceInterface());

                        if(RtlEqualUnicodeString(
                          &ustrAliasName,
                          &ustrName,
                          TRUE)) {
                            DPF(50, "AddFilter: dup");
                            pFilterNodeDuplicate = pFilterNode;
                            break;
                        }
                    } END_EACH_LIST_ITEM
                }
            }
            else {
                DPF1(10, "AddFilter: OpenDevice FAILED on alias (%s)",
                  DbgUnicode2Sz(ustrAliasName.Buffer));
            }
            RtlFreeUnicodeString(&ustrAliasName);
        }
    }
    
    pFilterNode = pFilterNodeDuplicate;
    Status = STATUS_SUCCESS;

    //
    // Create a new Filter_Node if this is not a duplicate.
    //
    if(pFilterNodeDuplicate == NULL) {
        pFilterNode = new FILTER_NODE(fulType);
        if(pFilterNode == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            Trap();
            goto exit;
        }
        Status = pFilterNode->Create(pwstrDeviceInterface);
        if(!NT_SUCCESS(Status)) {
            goto exit;
        }
        Status = pFilterNode->DuplicateForCapture();
        if(!NT_SUCCESS(Status)) {
            goto exit;
        }
        DPF1(50, "AddFilter: new CFilterNode fulType %08x", fulType);
    }

    //
    // If this is called from Interface Notification Callback,
    // create a new DeviceNode for the new FilterNode.
    //
    if(ppFilterNode == NULL) {
        if(pFilterNode->GetType() & FILTER_TYPE_ENDPOINT) {

            //
            // Check if a  DeviceNode has already been created for 
            // this FilterNode. 
            //
            if (NULL != pFilterNodeDuplicate && 
                NULL != pFilterNodeDuplicate->pDeviceNode) {
                DPF1(5, "Duplicate FilterNode %X. Skip DeviceNode Create", 
                    pFilterNode);
            }
            else {
                pFilterNode->pDeviceNode = new DEVICE_NODE;
                if(pFilterNode->pDeviceNode == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    Trap();
                    goto exit;
                }
                
                Status = pFilterNode->pDeviceNode->Create(pFilterNode);
                if(!NT_SUCCESS(Status)) {
                    goto exit;
                }
            }
        }
        else {
            DPF(50, "AddFilter: DestroyAllGraphs");
            DestroyAllGraphs();
        }
    }
    
exit:
    if(!NT_SUCCESS(Status)) {
        DPF2(5, "AddFilter: FAILED (%s) %08x",
          DbgUnicode2Sz(pwstrDeviceInterface),
          Status);

        if(pFilterNode != NULL && pFilterNodeDuplicate == NULL) {
            delete pFilterNode;
            pFilterNode = NULL;
        }
    }
    if(ppFilterNode != NULL) {
        *ppFilterNode = pFilterNode;
    }
    return(Status);
}

NTSTATUS
DeleteFilter(
    PWSTR pwstrDeviceInterface
)
{
    UNICODE_STRING ustrFilterName;
    UNICODE_STRING ustrAliasName;
    UNICODE_STRING ustrName;
    PFILTER_NODE pFilterNode;
    NTSTATUS Status;
    int i;

    DPF1(50, "DeleteFilter: (%s)", DbgUnicode2Sz(pwstrDeviceInterface));

    RtlInitUnicodeString(&ustrFilterName, pwstrDeviceInterface);

    //
    // First delete all filter nodes which have the device interface which is
    // going away
    //
    FOR_EACH_LIST_ITEM_DELETE(gplstFilterNode, pFilterNode) {
        if(pFilterNode->GetDeviceInterface() == NULL) {
            continue;
        }
        RtlInitUnicodeString(
          &ustrName,
          pFilterNode->GetDeviceInterface());

        if(RtlEqualUnicodeString(
          &ustrFilterName,
          &ustrName,
          TRUE)) {
            delete pFilterNode;
            DELETE_LIST_ITEM(gplstFilterNode);
        }
    } END_EACH_LIST_ITEM

    for(i = 0; i < SIZEOF_ARRAY(apguidCategories); i++) {

        //
        // According to PnP group, it is perfectly safe to ask for aliases 
        // during removal. The interface itself will be enabled or disabled. But
        // we will still get the correct aliases.
        //
        Status = IoGetDeviceInterfaceAlias(
          &ustrFilterName,
          apguidCategories[i],
          &ustrAliasName);

        if(NT_SUCCESS(Status)) {
            FOR_EACH_LIST_ITEM_DELETE(gplstFilterNode, pFilterNode) {

                if(pFilterNode->GetDeviceInterface() == NULL) {
                    continue;
                }
                RtlInitUnicodeString(
                  &ustrName,
                  pFilterNode->GetDeviceInterface());

                if(RtlEqualUnicodeString(
                  &ustrAliasName,
                  &ustrName,
                  TRUE)) {
                    delete pFilterNode;
                    DELETE_LIST_ITEM(gplstFilterNode);
                }

            } END_EACH_LIST_ITEM

            RtlFreeUnicodeString(&ustrAliasName);
        }
    }
    
    return(STATUS_SUCCESS);
}

#define GFX_VERBOSE_LEVEL 50

//=============================================================================
// Assumptions:
//    - SysaudioGfx.ulType has been already validated.
//
NTSTATUS  AddGfx(
    PSYSAUDIO_GFX pSysaudioGfx,
    ULONG cbMaxLength
)
{
    NTSTATUS Status;
    PFILE_OBJECT pFileObject;
    PFILTER_NODE pFilterNode;
    ULONG Flags;
    PWSTR pwstrDeviceName;
    ULONG GfxOrderBase, GfxOrderCeiling;

    ASSERT(pSysaudioGfx);

    pFileObject = NULL;
    pwstrDeviceName = NULL;
    pFilterNode = NULL;
    GfxOrderBase = GfxOrderCeiling = 0;

    DPF1(GFX_VERBOSE_LEVEL, "AddGfx :: Request to add Gfx %x", pSysaudioGfx);
    DPF1(GFX_VERBOSE_LEVEL, "          hGfx    = %x", pSysaudioGfx->hGfx);
    DPF1(GFX_VERBOSE_LEVEL, "          ulOrder = %x", pSysaudioGfx->ulOrder);
    DPF1(GFX_VERBOSE_LEVEL, "          ulType  = %x", pSysaudioGfx->ulType);
    DPF1(GFX_VERBOSE_LEVEL, "          Flags   = %x", pSysaudioGfx->ulFlags);

    //
    // Setup GFX Order's base & ceiling for future usage
    //
    if (pSysaudioGfx->ulType == GFX_DEVICETYPE_RENDER) {
        GfxOrderBase = ORDER_RENDER_GFX_FIRST;
        GfxOrderCeiling = ORDER_RENDER_GFX_LAST;
    }

    if (pSysaudioGfx->ulType == GFX_DEVICETYPE_CAPTURE) {
        GfxOrderBase = ORDER_CAPTURE_GFX_FIRST;
        GfxOrderCeiling = ORDER_CAPTURE_GFX_LAST;
    }

    ASSERT(GfxOrderBase);
    ASSERT(GfxOrderCeiling);

    //
    // validate that order is within range
    //
    if (pSysaudioGfx->ulOrder >= (GfxOrderCeiling - GfxOrderBase)) {
        Status = STATUS_INVALID_PARAMETER;
        Trap();
        goto exit;
    }

    //
    // Allocate a Filter Node for the new GFX
    //
    pFilterNode = new FILTER_NODE(FILTER_TYPE_GFX);
    if(pFilterNode == NULL) {
        Trap();
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    
    pFilterNode->SetRenderCaptureFlags(pSysaudioGfx->ulType);
    
    //
    // Copy the Device Name (on which the gfx needs to be attached) into a local 
    // copy for our own use
    //
    Status = SafeCopyDeviceName(
        (PWSTR) ((CHAR *) pSysaudioGfx + pSysaudioGfx->ulDeviceNameOffset), 
        cbMaxLength,
        &pwstrDeviceName);
    if (!NT_SUCCESS(Status)) {
        goto exit;
    }

    DPF1(GFX_VERBOSE_LEVEL, "          On DI   = %s", DbgUnicode2Sz(pwstrDeviceName));

    //
    // Make sure that there are no other GFXes with the same order on this device
    //
    if ((FindGfx(pFilterNode,
                 0, // wild card for handle
                 pwstrDeviceName,
                 pSysaudioGfx->ulOrder+GfxOrderBase))) {
        delete [] pwstrDeviceName;
        Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    //
    // Get the FileObject of the GFX for future use
    // SECURITY NOTE:
    // The handle is coming from UserMode. So we have to specify UserMode.
    // Also we are explicitly interested in FileObjects. The rest should be 
    // rejected.
    //
    Status = ObReferenceObjectByHandle(
      pSysaudioGfx->hGfx,
      FILE_GENERIC_READ | FILE_GENERIC_WRITE,
      *IoFileObjectType,
      UserMode,
      (PVOID*)&pFileObject,
      NULL);

    if (!NT_SUCCESS(Status) || NULL == pFileObject) {
        DPF1(GFX_VERBOSE_LEVEL, "AddGfx :: ObReference failed %x", Status);
        delete [] pwstrDeviceName;
        goto exit;
    }

    //
    // Add the device name string to global memory to be freed
    //
    Status = pFilterNode->lstFreeMem.AddList(pwstrDeviceName);
    if(!NT_SUCCESS(Status)) {
        Trap();
        delete [] pwstrDeviceName;
        goto exit;
    }

    //
    // Indicate that this Gfx needs be loaded only on the device pointed to be 
    // pwstrDeviceName
    //
    Status = pFilterNode->AddDeviceInterfaceMatch(pwstrDeviceName);
    if(!NT_SUCCESS(Status)) {
        Trap();
        delete [] pwstrDeviceName;        
        goto exit;
    }

    //
    // Set the Gfx order in the filter node
    //
    pFilterNode->SetOrder(pSysaudioGfx->ulOrder+GfxOrderBase);

    //
    // Profile the GFX and create pin infos, logical filter nodes etc
    //
    Status = pFilterNode->ProfileFilter(pFileObject);
    if(!NT_SUCCESS(Status)) {
        Trap();
        goto exit;
    }

    //
    // Fix the GFX glitching problem. Send the property blindly to GFX
    // filter. KS will handle the property.
    // Failures are not important, ignore them.
    //
    SetKsFrameHolding(pFileObject);    

exit:
    if(!NT_SUCCESS(Status)) {
        DPF1(GFX_VERBOSE_LEVEL, "AddGfx :: Failed, Status = %x", Status);
        if(pFilterNode != NULL) {
            delete pFilterNode;
            pFilterNode = NULL;
        }
        if(pFileObject != NULL) {
            ObDereferenceObject(pFileObject);
        }
    }
    else {

        DPF1(GFX_VERBOSE_LEVEL, "AddGfx :: Added GFX FilterNode %x", pFilterNode);
        DPF1(GFX_VERBOSE_LEVEL, "            order = %x", pFilterNode->GetOrder());
        DPF1(GFX_VERBOSE_LEVEL, "            type  = %x", pFilterNode->GetType());
        DPF1(GFX_VERBOSE_LEVEL, "            flags = %x", pFilterNode->GetFlags());

        //
        // Setup file handle details for later use of
        // the user mode handle passed in
        //
        pFilterNode->SetFileDetails(pSysaudioGfx->hGfx,
                                    pFileObject,
                                    PsGetCurrentProcess());
        //
        // Force a rebuild of graph nodes
        //
        DestroyAllGraphs();
    }
    return(Status);
}

//=============================================================================
// Assumptions:
//    - SysaudioGfx.ulType has been already validated.
//
NTSTATUS RemoveGfx(
    PSYSAUDIO_GFX pSysaudioGfx,
    ULONG cbMaxLength
)
{
    NTSTATUS Status;
    PFILTER_NODE pFilterNode;
    PWSTR pwstrDeviceName;
    ULONG GfxOrderBase, GfxOrderCeiling;

    pFilterNode = NULL;
    GfxOrderBase = GfxOrderCeiling = 0;
    pwstrDeviceName = NULL;

    DPF1(GFX_VERBOSE_LEVEL, "RemoveGfx :: Request to remove Gfx %x", pSysaudioGfx);
    DPF1(GFX_VERBOSE_LEVEL, "          hGfx    = %x", pSysaudioGfx->hGfx);
    DPF1(GFX_VERBOSE_LEVEL, "          ulOrder = %x", pSysaudioGfx->ulOrder);
    DPF1(GFX_VERBOSE_LEVEL, "          ulType  = %x", pSysaudioGfx->ulType);
    DPF1(GFX_VERBOSE_LEVEL, "          Flags   = %x", pSysaudioGfx->ulFlags);

    //
    // Setup GFX Order's base & ceiling for future usage
    //
    if (pSysaudioGfx->ulType == GFX_DEVICETYPE_RENDER) {
        GfxOrderBase = ORDER_RENDER_GFX_FIRST;
        GfxOrderCeiling = ORDER_RENDER_GFX_LAST;
    }

    if (pSysaudioGfx->ulType == GFX_DEVICETYPE_CAPTURE ) {
        GfxOrderBase = ORDER_CAPTURE_GFX_FIRST;
        GfxOrderCeiling = ORDER_CAPTURE_GFX_LAST;
    }

    ASSERT(GfxOrderBase);
    ASSERT(GfxOrderCeiling);

    //
    // Copy the Device Name (on which the gfx needs to be attached) into a local copy for our own use
    //
    Status = SafeCopyDeviceName(
        (PWSTR) ((CHAR *) pSysaudioGfx + pSysaudioGfx->ulDeviceNameOffset), 
        cbMaxLength,
        &pwstrDeviceName);
    if (!NT_SUCCESS(Status)) {
        goto exit;
    }

    DPF1(GFX_VERBOSE_LEVEL, "          On DI   = %s", DbgUnicode2Sz(pwstrDeviceName));

    //
    // Find the FilterNode for the Gfx
    //
    if ((pFilterNode = FindGfx(NULL,
                               pSysaudioGfx->hGfx,
                               pwstrDeviceName,
                               pSysaudioGfx->ulOrder+GfxOrderBase)) == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    //
    // Should we validate the FileHandle Value?
    //

    //
    // Dereference the file object
    //
    pFilterNode->ClearFileDetails();
exit:
    if(!NT_SUCCESS(Status)) {
        DPF1(GFX_VERBOSE_LEVEL, "RemoveGfx :: Failed, Status = %x", Status);
        Trap();
    }
    else {
        delete pFilterNode;
    }
    delete pwstrDeviceName;
    return(Status);
}

PFILTER_NODE
FindGfx(
    PFILTER_NODE pnewFilterNode,
    HANDLE hGfx,
    PWSTR pwstrDeviceName,
    ULONG GfxOrder
)
{
    PFILTER_NODE pFilterNode;
    ULONG DeviceCount;
    UNICODE_STRING usInDevice, usfnDevice;
    PWSTR pwstr;

    DPF2(90, "FindGfx::   Looking for GFX with order = %x attached to %s)", GfxOrder, DbgUnicode2Sz(pwstrDeviceName));

    FOR_EACH_LIST_ITEM(gplstFilterNode, pFilterNode) {

        //
        // Skip the one we just added
        //
        if (pFilterNode == pnewFilterNode) {
            continue;
        }

        //
        // Check whether this pFilterNode matches the Gfx we are looking for
        //
        if (pFilterNode->DoesGfxMatch(hGfx, pwstrDeviceName, GfxOrder)) {
            return (pFilterNode);
        }

    } END_EACH_LIST_ITEM

    return(NULL);
}


//=============================================================================
//  
// Copies a UNICODE DeviceName to a new location. The source string is coming 
// from user mode.
// There are assumptions in the code that the size should be greater than 4  
// characters. (see DEVICE_NAME_TAG)
// Caller must make sure that this is BUFFERRED IO.
// 
NTSTATUS
SafeCopyDeviceName(
    PWSTR pwstrDeviceName,
    ULONG cbMaxLength,
    PWSTR *String
)
{
    NTSTATUS ntStatus;
    ULONG cchLength;
    PWSTR pwstrString = NULL;

    *String = NULL;

    //
    // SECURITY_NOTE:
    // pwstrDeviceName points to a NULL-terminated UNICODE string.
    // The string is coming from user mode, through BUFFERRED IO. So try/
    // except is not necessary. Also Probe would not catch any errors.
    // The IRP OutputBufferLength limits, the size of the string.
    //
    if (S_OK != 
        StringCchLength(pwstrDeviceName, (size_t) cbMaxLength / sizeof(WCHAR), (size_t *) &cchLength))
    {
        DPF(5, "SafeCopyDeviceName: DeviceName is not zero-terminated.");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    //
    // SECURITY NOTE:
    // There are assumptions further in the code about DeviceName string.
    // Make sure those assumptions hold.
    // One assumption is that DeviceName should be greater than 4  
    // characters.
    //
    if (cchLength <= wcslen(DEVICE_NAME_TAG)) {
        DPF(5, "SafeCopyDeviceName: DeviceName is not well-formed");
        ntStatus = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    pwstrString = new(WCHAR[cchLength + 1]) ;
    if(pwstrString == NULL) {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    //
    // SECURITY NOTE:
    // Note that Length does not include the terminating NULL.
    // Use n version of string copy in case the buffer changes.
    // Also make sure that the string is NULL terminated.
    //
    wcsncpy(pwstrString, pwstrDeviceName, cchLength);
    pwstrString[cchLength] = UNICODE_NULL;
    ntStatus = STATUS_SUCCESS;

exit:    
    *String = pwstrString;
    return ntStatus;
}

NTSTATUS
GetFilterTypeFromGuid(
    IN LPGUID pguid,
    OUT PULONG pfulType
)
{
    int i;
    for(i = 0; i < SIZEOF_ARRAY(apguidCategories); i++) {
        if (memcmp (apguidCategories[i], pguid, sizeof(GUID)) == 0) {
            *pfulType |= aulFilterType[i];
            return(STATUS_SUCCESS);
        }
    }
    return(STATUS_INVALID_DEVICE_REQUEST);
}

//---------------------------------------------------------------------------
//  End of File: notify.cpp
//---------------------------------------------------------------------------
