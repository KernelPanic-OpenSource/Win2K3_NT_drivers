/*++

Copyright (C) Microsoft Corporation, 1996 - 1999

Module Name:

    pdo.c

Abstract:

    This module contains the dispatch routines for scsiport's physical device
    objects

Authors:

    Peter Wieland

Environment:

    Kernel mode only

Notes:

Revision History:

--*/

#include "port.h"

#if DBG
static const char *__file__ = __FILE__;
#endif

VOID
SpAdapterCleanup(
    IN PADAPTER_EXTENSION DeviceExtension
    );

VOID
SpReapChildren(
    IN PADAPTER_EXTENSION Adapter
    );

BOOLEAN
SpTerminateAdapterSynchronized (
    IN PADAPTER_EXTENSION Adapter
    );

BOOLEAN
SpRemoveAdapterSynchronized(
    IN PADAPTER_EXTENSION Adapter
    );

VOID
SpFlushAllRequests(
    IN PVOID HwDeviceExtension,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN UCHAR SrbStatus
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SpDeleteLogicalUnit)
#pragma alloc_text(PAGE, SpRemoveLogicalUnit)
#pragma alloc_text(PAGE, SpWaitForRemoveLock)
#pragma alloc_text(PAGE, SpAdapterCleanup)
#pragma alloc_text(PAGE, SpReapChildren)

#pragma alloc_text(PAGELOCK, ScsiPortRemoveAdapter)
#endif


BOOLEAN
SpRemoveLogicalUnit(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN UCHAR RemoveType
    )

{
    PADAPTER_EXTENSION adapterExtension = LogicalUnit->AdapterExtension;

    ULONG isRemoved;
    ULONG oldDebugLevel;

    PAGED_CODE();

    if(LogicalUnit->CommonExtension.IsRemoved != REMOVE_COMPLETE) {

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            SpWaitForRemoveLock(LogicalUnit->DeviceObject, SP_BASE_REMOVE_LOCK );

            //
            // If the device was claimed we should release it now.
            //

            if(LogicalUnit->IsClaimed) {
                LogicalUnit->IsClaimed = FALSE;
                LogicalUnit->IsLegacyClaim = FALSE;
            }

        }

        DebugPrint((1, "SpRemoveLogicalUnit - %sremoving device %#p\n",
                    (RemoveType == IRP_MN_SURPRISE_REMOVAL) ? "surprise " : "",
                    LogicalUnit));

        //
        // If the lun isn't marked as missing yet or is marked as missing but
        // PNP hasn't been informed yet then we cannot delete it.  Set it back
        // to the NO_REMOVE state so that we'll be able to attempt a rescan.
        //
        // Likewise if the lun is invisible then just swallow the remove
        // operation now that we've cleared any existing claims.
        //

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            //
            // If the device is not missing or is missing but is still
            // enumerated then don't finish destroying it.
            //

            if((LogicalUnit->IsMissing == TRUE) &&
               (LogicalUnit->IsEnumerated == FALSE)) {

                // do nothing here - fall through and destroy the device.

            } else {

                DebugPrint((1, "SpRemoveLogicalUnit - device is not missing "
                               "and will not be destroyed\n"));

                SpAcquireRemoveLock(LogicalUnit->DeviceObject, SP_BASE_REMOVE_LOCK);

                LogicalUnit->CommonExtension.IsRemoved = NO_REMOVE;

                return FALSE;
            }

        } else if((LogicalUnit->IsVisible == FALSE) &&
                  (LogicalUnit->IsMissing == FALSE)) {

            //
            // The surprise remove came because the device is no longer
            // visible.  We don't want to destroy it.
            //

            return FALSE;
        }

        //
        // Mark the device as uninitialized so that we'll go back and
        // recreate all the necessary stuff if it gets restarted.
        //

        LogicalUnit->CommonExtension.IsInitialized = FALSE;

        //
        // Delete the device map entry for this one (if any).
        //

        SpDeleteDeviceMapEntry(&(LogicalUnit->CommonExtension));

        if(RemoveType == IRP_MN_REMOVE_DEVICE) {

            ASSERT(LogicalUnit->RequestTimeoutCounter == -1);
            ASSERT(LogicalUnit->ReadyLogicalUnit == NULL);
            ASSERT(LogicalUnit->PendingRequest == NULL);
            ASSERT(LogicalUnit->BusyRequest == NULL);
            ASSERT(LogicalUnit->QueueCount == 0);

            LogicalUnit->CommonExtension.IsRemoved = REMOVE_COMPLETE;

            //
            // Yank this out of the logical unit list.
            //

            SpRemoveLogicalUnitFromBin(LogicalUnit->AdapterExtension,
                                       LogicalUnit);

            LogicalUnit->PathId = 0xff;
            LogicalUnit->TargetId = 0xff;
            LogicalUnit->Lun = 0xff;

            //
            // If this device wasn't temporary then delete it.
            //

            if(LogicalUnit->IsTemporary == FALSE) {
                SpDeleteLogicalUnit(LogicalUnit);
            }
        }
    }

    return TRUE;
}


VOID
SpDeleteLogicalUnit(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    )

/*++

Routine Description:

    This routine will release any resources held for the logical unit, mark the
    device extension as deleted, and call the io system to actually delete
    the object.  The device object will be deleted once it's reference count
    drops to zero.

Arguments:

    LogicalUnit - the device object for the logical unit to be deleted.

Return Value:

    none

--*/

{
    PAGED_CODE();

    ASSERT(LogicalUnit->ReadyLogicalUnit == NULL);
    ASSERT(LogicalUnit->PendingRequest == NULL);
    ASSERT(LogicalUnit->BusyRequest == NULL);
    ASSERT(LogicalUnit->QueueCount == 0);

    ASSERT(LogicalUnit->PathId == 0xff);
    ASSERT(LogicalUnit->TargetId == 0xff);
    ASSERT(LogicalUnit->Lun == 0xff);

    //
    // Unregister with WMI.
    //

    if(LogicalUnit->CommonExtension.WmiInitialized == TRUE) {

        //
        // Destroy all our WMI resources and unregister with WMI.
        //

        IoWMIRegistrationControl(LogicalUnit->DeviceObject,
                                 WMIREG_ACTION_DEREGISTER);

        // 
        // We should be asking the WmiFreeRequestList of remove some
        // free cells.

        LogicalUnit->CommonExtension.WmiInitialized = FALSE;
        SpWmiDestroySpRegInfo(LogicalUnit->DeviceObject);
    }

#if DBG
    // ASSERT(LogicalUnit->CommonExtension.RemoveTrackingList == NULL);
    ExDeleteNPagedLookasideList(
        &(LogicalUnit->CommonExtension.RemoveTrackingLookasideList));
#endif

    //
    // If the request sense irp still exists, delete it.
    //

    if(LogicalUnit->RequestSenseIrp != NULL) {
        IoFreeIrp(LogicalUnit->RequestSenseIrp);
        LogicalUnit->RequestSenseIrp = NULL;
    }

    if(LogicalUnit->HwLogicalUnitExtension != NULL) {
        ExFreePool(LogicalUnit->HwLogicalUnitExtension);
        LogicalUnit->HwLogicalUnitExtension = NULL;
    }

    if(LogicalUnit->SerialNumber.Buffer != NULL) {
        ExFreePool(LogicalUnit->SerialNumber.Buffer);
        RtlInitAnsiString(&(LogicalUnit->SerialNumber), NULL);
    }

    if(LogicalUnit->DeviceIdentifierPage != NULL) {
        ExFreePool(LogicalUnit->DeviceIdentifierPage);
        LogicalUnit->DeviceIdentifierPage = NULL;
    }

    //
    // If this lun is temporary then clear the RescanLun field in the adapter.
    //

    if(LogicalUnit->IsTemporary) {
        ASSERT(LogicalUnit->AdapterExtension->RescanLun = LogicalUnit);
        LogicalUnit->AdapterExtension->RescanLun = NULL;
    } else {
        ASSERT(LogicalUnit->AdapterExtension->RescanLun != LogicalUnit);
    }

    IoDeleteDevice(LogicalUnit->DeviceObject);

    return;
}


VOID
ScsiPortRemoveAdapter(
    IN PDEVICE_OBJECT AdapterObject,
    IN BOOLEAN Surprise
    )
{
    PADAPTER_EXTENSION adapter = AdapterObject->DeviceExtension;
    PCOMMON_EXTENSION commonExtension = AdapterObject->DeviceExtension;

    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    ASSERT_FDO(AdapterObject);
    ASSERT(adapter->IsPnp);

    //
    // Set the flag PD_ADAPTER_REMOVED to keep scsiport from calling into the
    // miniport after we've started this teardown.
    //

    if(Surprise == FALSE) {
        PVOID sectionHandle;
        KIRQL oldIrql;

        //
        // Wait until all outstanding requests have been completed.  If the
        // adapter was surprise removed, we don't need to wait on the remove
        // lock again, since we already waited for it in the surprise remove
        // path.
        //

        if (commonExtension->CurrentPnpState != IRP_MN_SURPRISE_REMOVAL) {
            SpWaitForRemoveLock(AdapterObject, AdapterObject);
        }

        //
        // If the device is started we should uninitialize the miniport and
        // release it's resources.  Fortunately this is exactly what stop does.
        //

        if((commonExtension->CurrentPnpState != IRP_MN_SURPRISE_REMOVAL) &&
           ((commonExtension->CurrentPnpState == IRP_MN_START_DEVICE) ||
            (commonExtension->PreviousPnpState == IRP_MN_START_DEVICE))) {

            //
            // Okay.  If this adapter can't support remove then we're dead
            //

            ASSERT(SpIsAdapterControlTypeSupported(adapter, ScsiStopAdapter) == TRUE);

            //
            // Stop the miniport now that it's safe.
            //

            SpEnableDisableAdapter(adapter, FALSE);

            //
            // Mark the adapter as removed.
            //

    #ifdef ALLOC_PRAGMA
            sectionHandle = MmLockPagableCodeSection(ScsiPortRemoveAdapter);
            InterlockedIncrement(&SpPAGELOCKLockCount);
    #endif
            KeAcquireSpinLock(&(adapter->SpinLock), &oldIrql);
            adapter->SynchronizeExecution(adapter->InterruptObject,
                                          SpRemoveAdapterSynchronized,
                                          adapter);

            KeReleaseSpinLock(&(adapter->SpinLock), oldIrql);

    #ifdef ALLOC_PRAGMA
            InterlockedDecrement(&SpPAGELOCKLockCount);
            MmUnlockPagableImageSection(sectionHandle);
    #endif

        }
        SpReapChildren(adapter);
    }

    if(commonExtension->WmiInitialized == TRUE) {

        //
        // Destroy all our WMI resources and unregister with WMI.
        //

        IoWMIRegistrationControl(AdapterObject, WMIREG_ACTION_DEREGISTER);
        SpWmiRemoveFreeMiniPortRequestItems(adapter);
        commonExtension->WmiInitialized = FALSE;
        commonExtension->WmiMiniPortInitialized = FALSE;
    }

    //
    // If we are surprise removed, the following gets executed twice, but
    // it's safe to do so.
    //

    SpDeleteDeviceMapEntry(commonExtension);
    SpDestroyAdapter(adapter, Surprise);

    return;
}


VOID
SpWaitForRemoveLock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID LockTag
    )
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    //
    // Mark the thing as removing
    //

    commonExtension->IsRemoved = REMOVE_PENDING;

    //
    // Release our outstanding lock.
    //

    SpReleaseRemoveLock(DeviceObject, LockTag);

    DebugPrint((4, "SpWaitForRemoveLock - Reference count is now %d\n",
                commonExtension->RemoveLock));

    KeWaitForSingleObject(&(commonExtension->RemoveEvent),
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    DebugPrint((4, "SpWaitForRemoveLock - removing device %#p\n",
                DeviceObject));

    return;
}


VOID
SpDestroyAdapter(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Surprise
    )
{
    SpReleaseAdapterResources(Adapter, FALSE, Surprise);
    SpAdapterCleanup(Adapter);
    return;
}


VOID
SpAdapterCleanup(
    IN PADAPTER_EXTENSION Adapter
    )

/*++

Routine Description:

    This routine cleans up the names associated with the specified adapter
    and the i/o system counts.

Arguments:

    Adapter - Supplies a pointer to the device extension to be deleted.

Return Value:

    None.

--*/

{
    PCOMMON_EXTENSION commonExtension = &(Adapter->CommonExtension);

    PAGED_CODE();

    //
    // If we assigned a port number to this adapter then attempt to delete the
    // symbolic links we created to it.
    //

    if(Adapter->PortNumber != -1) {

        PWCHAR wideNameStrings[] = {L"\\Device\\ScsiPort%d",
                                    L"\\DosDevices\\Scsi%d:"};
        ULONG i;

        for(i = 0; i < (sizeof(wideNameStrings) / sizeof(PWCHAR)); i++) {
            WCHAR wideLinkName[64];
            UNICODE_STRING unicodeLinkName;

            swprintf(wideLinkName, wideNameStrings[i], Adapter->PortNumber);
            RtlInitUnicodeString(&unicodeLinkName, wideLinkName);
            IoDeleteSymbolicLink(&unicodeLinkName);
        }

        Adapter->PortNumber = -1;

        //
        // Decrement the scsiport count.
        //

        IoGetConfigurationInformation()->ScsiPortCount--;
    }

    return;
}


VOID
SpReleaseAdapterResources(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Stop,
    IN BOOLEAN Surprise
    )

/*++

Routine Description:

    This function deletes all of the storage associated with a device
    extension, disconnects from the timers and interrupts and then deletes the
    object.   This function can be called at any time during the initialization.

Arguments:

    Adapter - Supplies a pointer to the device extesnion to be deleted.
    Surprise - This is redundant, but is used by stop, remove and surprise-remove
    SurpriseRemoved - Indicates that the adapter has been surprise-removed

Return Value:

    None.

--*/

{

    PCOMMON_EXTENSION commonExtension = &(Adapter->CommonExtension);
    ULONG j;
    PVOID tempPointer;

    PAGED_CODE();

#if DBG

    if(!Surprise && !Stop) {

        //
        // Free the Remove tracking lookaside list.
        //

        ExDeleteNPagedLookasideList(&(commonExtension->RemoveTrackingLookasideList));
    }
#endif

    //
    // Stop the time and disconnect the interrupt if they have been
    // initialized.  The interrupt object is connected after
    // timer has been initialized, and the interrupt object is connected, but
    // before the timer is started.
    //

    if(Adapter->DeviceObject->Timer != NULL) {
        IoStopTimer(Adapter->DeviceObject);
        KeCancelTimer(&(Adapter->MiniPortTimer));
    }

    if(Adapter->SynchronizeExecution != SpSynchronizeExecution) {

        if (Adapter->InterruptObject) {
            IoDisconnectInterrupt(Adapter->InterruptObject);
        }

        if (Adapter->InterruptObject2) {
            IoDisconnectInterrupt(Adapter->InterruptObject2);
            Adapter->InterruptObject2 = NULL;
        }

        //
        // SpSynchronizeExecution expects to get a pointer to the
        // adapter extension as the "interrupt" parameter.
        //

        Adapter->InterruptObject = (PVOID) Adapter;
        Adapter->SynchronizeExecution = SpSynchronizeExecution;
    }

    //
    // Delete the miniport's device extension
    //

    if (Adapter->HwDeviceExtension != NULL) {

        PHW_DEVICE_EXTENSION devExt =
            CONTAINING_RECORD(Adapter->HwDeviceExtension,
                              HW_DEVICE_EXTENSION,
                              HwDeviceExtension);

        ExFreePool(devExt);
        Adapter->HwDeviceExtension = NULL;
    }

    //
    // Free the configuration information structure.
    //

    if (Adapter->PortConfig) {
        ExFreePool(Adapter->PortConfig);
        Adapter->PortConfig = NULL;
    }

    //
    // Deallocate SCSIPORT WMI REGINFO information, if any.
    //

    SpWmiDestroySpRegInfo(Adapter->DeviceObject);

    //
    // Free the common buffer.
    //

    if (SpVerifyingCommonBuffer(Adapter)) {

        SpFreeCommonBufferVrfy(Adapter);

    } else {

        if (Adapter->SrbExtensionBuffer != NULL &&
            Adapter->CommonBufferSize != 0) {

            if (Adapter->DmaAdapterObject == NULL) {

                //
                // Since there is no adapter just free the non-paged pool.
                //

                ExFreePool(Adapter->SrbExtensionBuffer);

            } else {

                if(Adapter->UncachedExtensionIsCommonBuffer == FALSE) {
                    MmFreeContiguousMemorySpecifyCache(Adapter->SrbExtensionBuffer,
                                                       Adapter->CommonBufferSize,
                                                       MmCached);
                } else {

                    FreeCommonBuffer(
                        Adapter->DmaAdapterObject,
                        Adapter->CommonBufferSize,
                        Adapter->PhysicalCommonBuffer,
                        Adapter->SrbExtensionBuffer,
                        FALSE);
                }

            }
            Adapter->SrbExtensionBuffer = NULL;
        }
    }

    //
    // Get rid of our dma adapter.
    //

    if(Adapter->DmaAdapterObject != NULL) {
        PutDmaAdapter(Adapter->DmaAdapterObject);
        Adapter->DmaAdapterObject = NULL;
    }

    //
    // Free the SRB data array, if this is not a surprise remove. We should
    // not free the lookaside list in case of a surprise remove, as some 
    // requests might have allocated SRB_DATA blocks w/o holding the adapter 
    // remove lock. All other resources are required only if we ere able to 
    // acquire the adapter remove lock, which we guarantee we won't at this point.
    // It is safe to free the array during remove, because we delete all the LUN's
    // before realeasing adapters resources

    if( !Surprise ){ 

        if (Adapter->SrbDataListInitialized) {

            if(Adapter->EmergencySrbData != NULL) {

                ExFreeToNPagedLookasideList(
                    &Adapter->SrbDataLookasideList,
                    Adapter->EmergencySrbData);
                Adapter->EmergencySrbData = NULL;

            }

            ExDeleteNPagedLookasideList(&Adapter->SrbDataLookasideList);
            Adapter->SrbDataListInitialized = FALSE;
        }
    }


    if (Adapter->InquiryBuffer != NULL) {
        ExFreePool(Adapter->InquiryBuffer);
        Adapter->InquiryBuffer = NULL;
    }

    if (Adapter->InquirySenseBuffer != NULL) {
        ExFreePool(Adapter->InquirySenseBuffer);
        Adapter->InquirySenseBuffer = NULL;
    }
    
    if (Adapter->InquiryIrp != NULL) {
        IoFreeIrp(Adapter->InquiryIrp);
        Adapter->InquiryIrp = NULL;
    }
    
    if (Adapter->InquiryMdl != NULL) {
        IoFreeMdl(Adapter->InquiryMdl);
        Adapter->InquiryMdl = NULL;
    }

    //
    // Unmap any mapped areas.
    //

    SpReleaseMappedAddresses(Adapter);

    //
    // If we've got any resource lists allocated still we should free them
    // now.
    //

    if(Adapter->AllocatedResources != NULL) {
        ExFreePool(Adapter->AllocatedResources);
        Adapter->AllocatedResources = NULL;
    }

    if(Adapter->TranslatedResources != NULL) {
        ExFreePool(Adapter->TranslatedResources);
        Adapter->TranslatedResources = NULL;
    }

    //
    // Cleanup verifier resources.
    //

    if (SpVerifierActive(Adapter)) {
        SpDoVerifierCleanup(Adapter);
    }

#if defined(FORWARD_PROGRESS)
    //
    // Cleanup the adapter's reserved pages.
    //

    if (Adapter->ReservedPages != NULL) {
        MmFreeMappingAddress(Adapter->ReservedPages,
                             SCSIPORT_TAG_MAPPING_LIST);
        Adapter->ReservedPages = NULL;        
    }

    if (Adapter->ReservedMdl != NULL) {   
        IoFreeMdl(Adapter->ReservedMdl);
        Adapter->ReservedMdl = NULL;
    }
#endif

    Adapter->CommonExtension.IsInitialized = FALSE;

    return;
}


VOID
SpReapChildren(
    IN PADAPTER_EXTENSION Adapter
    )
{
    ULONG j;

    PAGED_CODE();

    //
    // Run through the logical unit bins and remove any child devices which
    // remain.
    //

    for(j = 0; j < NUMBER_LOGICAL_UNIT_BINS; j++) {

        while(Adapter->LogicalUnitList[j].List != NULL) {

            PLOGICAL_UNIT_EXTENSION lun =
                Adapter->LogicalUnitList[j].List;

            lun->IsMissing = TRUE;
            lun->IsEnumerated = FALSE;

            SpRemoveLogicalUnit(lun, IRP_MN_REMOVE_DEVICE);
        }
    }

    //
    // Remove the initiator LUs.
    //

    for (j = 0; j < 8; j++) {
        PLOGICAL_UNIT_EXTENSION lu = Adapter->InitiatorLU[j];
        if (lu != NULL) {
            Adapter->InitiatorLU[j] = NULL;
        }
    }

    return;
}


VOID
SpTerminateAdapter(
    IN PADAPTER_EXTENSION Adapter
    )
/*++

Routine Description:

    This routine will terminate the miniport's control of the adapter.  It
    does not cleanly shutdown the miniport and should only be called when
    scsiport is notified that the adapter has been surprise removed.

    This works by synchronizing with the miniport and setting flags to
    disable any new calls into the miniport.  Once this has been done it can
    run through and complete any i/o requests which may still be inside
    the miniport.

Arguments:

    Adapter - the adapter to terminate.

Return Value:

    none

--*/

{
    KIRQL oldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

    KeAcquireSpinLockAtDpcLevel(&(Adapter->SpinLock));

    if (Adapter->CommonExtension.CurrentPnpState == IRP_MN_START_DEVICE) {

        //
        // TA synchronized will stop all calls into the miniport and complete
        // all active requests.
        //

        Adapter->SynchronizeExecution(Adapter->InterruptObject,
                                      SpTerminateAdapterSynchronized,
                                      Adapter);

        Adapter->CommonExtension.PreviousPnpState = 0xff;


        SpFlushAllRequests(Adapter->HwDeviceExtension,
                           0xff,
                           0xff,
                           0xff,
                           SRB_STATUS_NO_HBA);
        

        //
        // Stop the miniport timer
        //

        KeCancelTimer(&(Adapter->MiniPortTimer));

        //
        // We keep the device object timer running so that any held, busy or
        // otherwise deferred requests will have a chance to get flushed out.
        // We can give the whole process a boost by setting the adapter timeout
        // counter to 1 (it will go to zero in the tick handler) and running
        // the tick handler by hand here.
        //

        Adapter->PortTimeoutCounter = 1;
        ScsiPortTickHandler(Adapter->DeviceObject, NULL);

    } else {
        KeReleaseSpinLockFromDpcLevel(&(Adapter->SpinLock));
    }

    KeLowerIrql(oldIrql);

    return;
}


BOOLEAN
SpTerminateAdapterSynchronized(
    IN PADAPTER_EXTENSION Adapter
    )
{
    //
    // Disable the interrupt from coming in.
    //

    SET_FLAG(Adapter->InterruptData.InterruptFlags, PD_ADAPTER_REMOVED);


    ScsiPortCompleteRequest(Adapter->HwDeviceExtension,
                            0xff,
                            0xff,
                            0xff,
                            SRB_STATUS_NO_HBA);

    //
    //This needs to be done to start the next request sitting in the adapter
    //queue, otherwise surprise remove will wait forever for the remove
    //lockcount to be zero.
    //

    ScsiPortNotification(NextRequest,
                         Adapter->HwDeviceExtension);

    //
    // Run the completion DPC.
    //

    if(TEST_FLAG(Adapter->InterruptData.InterruptFlags, 
                 PD_NOTIFICATION_REQUIRED)) {
        SpRequestCompletionDpc(Adapter->DeviceObject);
    }

    return TRUE;
}

BOOLEAN
SpRemoveAdapterSynchronized(
    PADAPTER_EXTENSION Adapter
    )
{
    //
    // Disable the interrupt from coming in.
    //

    SET_FLAG(Adapter->InterruptData.InterruptFlags, PD_ADAPTER_REMOVED);

    return TRUE;
}

VOID
SpFlushAllRequests(
    IN PVOID HwDeviceExtension,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN UCHAR SrbStatus
    )

/*++

Routine Description:

    Flushes all requests in all the Lun device queues, busy and pending 
    requests on all Luns. The request stuck in the adapter's CurrentIrp
    field of the adapter due to PD_RESET_HELD will be taken care of by 
    the tick handler. In short all the requests stuck anywhere in ScsiPort 
    will be flushed. Unfortunately this is not enough to handle Surprise 
    Remove. We also need a small check in Startio so that we flush all 
    requests on the lun for which we got a request. 

Arguments:

    DeviceExtenson - Supplies the HBA miniport driver's adapter data storage.

    TargetId, Lun and PathId - specify device address on a SCSI bus.

    SrbStatus - Status to be returned in each completed SRB.

Return Value:

    None.

    Note: This routine must be called with the Adapter SpinLock held, and
          the lock would be released by this routine.    

--*/

{
    PADAPTER_EXTENSION deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);
    ULONG binNumber;
    PIRP listIrp = NULL;
    PIRP nextIrp;
    PKDEVICE_QUEUE_ENTRY packet;
    PIO_STACK_LOCATION irpStack;
    PSCSI_REQUEST_BLOCK srb;

    for (binNumber = 0; binNumber < NUMBER_LOGICAL_UNIT_BINS; binNumber++) {

        PLOGICAL_UNIT_BIN bin = &deviceExtension->LogicalUnitList[binNumber];
        PLOGICAL_UNIT_EXTENSION LogicalUnit;
        ULONG limit = 0;

        LogicalUnit = bin->List;

        DebugPrint((2, "ScsiPortCompleteRequest: Completing requests in "
                       "bin %d [%#p]\n",
                    binNumber, bin));

        for(LogicalUnit = bin->List;
            LogicalUnit != NULL;
            LogicalUnit = LogicalUnit->NextLogicalUnit) {

            PLIST_ENTRY entry;

            ASSERT(limit++ < 1000);

            //
            // See if this logical unit matches the pattern.  Check for -1
            // first since this seems to be the most popular way to complete
            // requests.
            //

            if (((PathId == SP_UNTAGGED) || (PathId == LogicalUnit->PathId)) &&
                ((TargetId == SP_UNTAGGED) ||
                 (TargetId == LogicalUnit->TargetId)) &&
                ((Lun == SP_UNTAGGED) || (Lun == LogicalUnit->Lun))) {   

                //
                // The queue may not be busy so we have to use the IfBusy variant.  
                // Use a zero key to pull items from the head of it (if any are there)
                //

                while ((packet =
                        KeRemoveByKeyDeviceQueueIfBusy(
                            &(LogicalUnit->DeviceObject->DeviceQueue),
                            0))
                       != NULL) {

                    nextIrp = CONTAINING_RECORD(packet,
                                                IRP,
                                                Tail.Overlay.DeviceQueueEntry);

                    //
                    // Get the srb.
                    //

                    irpStack = IoGetCurrentIrpStackLocation(nextIrp);
                    srb = irpStack->Parameters.Scsi.Srb;

                    //
                    // Set the status code.
                    //  

                    srb->SrbStatus = SrbStatus;
                    nextIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;

                    //  
                    // Link the requests. They will be completed after the
                    // spinlock is released.
                    //

                    nextIrp->Tail.Overlay.ListEntry.Flink = 
                        (PLIST_ENTRY)listIrp;

                    listIrp = nextIrp;
                }

                //
                // If there is a pending request on the LU, add it to the list so it
                // gets flushed along with the queued requests.
                //

                if (LogicalUnit->PendingRequest != NULL) {

                    PIRP irp = LogicalUnit->PendingRequest->CurrentIrp;
                    srb = LogicalUnit->PendingRequest->CurrentSrb;

                    DebugPrint((1, "SpFlushReleaseQueue: flushing pending request irp:%p srb:%p\n", irp, srb));

                    srb->SrbStatus = SrbStatus;
                    irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
                    irp->Tail.Overlay.ListEntry.Flink = (PLIST_ENTRY) listIrp;
                    listIrp = irp;

                    LogicalUnit->PendingRequest = NULL;
                    ASSERT(LogicalUnit->LuFlags | LU_PENDING_LU_REQUEST);
                    CLEAR_FLAG(LogicalUnit->LuFlags, LU_PENDING_LU_REQUEST);

                }

                //
                // If there is a busy request on the LU, add it to the list so it
                // gets flushed along with the queued requests.
                //

                if ( LogicalUnit->BusyRequest ) {

                    PIRP irp = LogicalUnit->BusyRequest->CurrentIrp;
                    srb = LogicalUnit->BusyRequest->CurrentSrb;

                    DebugPrint((1, "SpFlushReleaseQueue: flushing busy request irp:%\
p srb:%p\n", irp, srb));

                    srb->SrbStatus = SrbStatus;
                    irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
                    irp->Tail.Overlay.ListEntry.Flink = (PLIST_ENTRY) listIrp;
                    listIrp = irp;

                    LogicalUnit->BusyRequest = NULL;
                    ASSERT((LU_LOGICAL_UNIT_IS_BUSY | LU_QUEUE_IS_FULL));

                    CLEAR_FLAG(LogicalUnit->LuFlags,
                               (LU_LOGICAL_UNIT_IS_BUSY | LU_QUEUE_IS_FULL));

                }

            }
        }
    }

    KeReleaseSpinLockFromDpcLevel(&(deviceExtension->SpinLock));

    //
    // Complete the flushed requests.
    //

    while (listIrp != NULL) {

        PSRB_DATA srbData;

        nextIrp = listIrp;
        listIrp = (PIRP) nextIrp->Tail.Overlay.ListEntry.Flink;

        //
        // Get the srb.
        //

        irpStack = IoGetCurrentIrpStackLocation(nextIrp);
        srb = irpStack->Parameters.Scsi.Srb;
        srbData = srb->OriginalRequest;

        srb->OriginalRequest = nextIrp;

        SpReleaseRemoveLock(deviceExtension->DeviceObject, nextIrp);
        SpCompleteRequest(deviceExtension->DeviceObject,
                          nextIrp,
                          srbData,
                          IO_NO_INCREMENT);
    }

    return;

}
