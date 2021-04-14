/*++

Copyright (c) 1999 Microsoft Corporation

Module Name:

    thread.c

Abstract:


Environment:

    kernel mode only

Notes:

Revision History:

    6-20-99 : created

--*/

#include "common.h"

#ifdef ALLOC_PRAGMA
#endif

// non paged functions
// USBPORT_CreateWorkerThread
// USBPORT_WorkerThreadStart
// USBPORT_SignalWorker


//NOTE perhaps one thread for all drivers will be enough
// we need to research this


// BUGBUG
// not a WDM function, see if we can do a runtime detect

/*
NTKERNELAPI
LONG
KeSetBasePriorityThread (
    IN PKTHREAD Thread,
    IN LONG Increment
    );

VOID
USBPORT_SetBasePriorityThread(
    PKTHREAD Thread,
    LONG Increment
    )
{
    //KeSetBasePriorityThread(Thread, Increment);
}
*/

VOID
USBPORT_WorkerThread(
    PVOID StartContext
    )
/*++

Routine Description:

    start the worker thread

Arguments:

Return Value:

    none

--*/
{
    PDEVICE_EXTENSION devExt;
    PDEVICE_OBJECT fdoDeviceObject;
    KIRQL irql;

    fdoDeviceObject = StartContext;
    GET_DEVICE_EXT(devExt, fdoDeviceObject);
    ASSERT_FDOEXT(devExt);

    devExt->Fdo.WorkerPkThread = KeGetCurrentThread();
    // priority setting optimal for suspend/resume

    // increment by 7, value suggested by perf team
    //USBPORT_SetBasePriorityThread(devExt->Fdo.WorkerPkThread, 7);

    // hurry up and wait
    do {

        LARGE_INTEGER t1, t2;

        KeQuerySystemTime(&t1);

        KeWaitForSingleObject(
                    &devExt->Fdo.WorkerThreadEvent,
                    Suspended,
                    KernelMode,
                    FALSE,
                    NULL);

        KeQuerySystemTime(&t2);
        // deltaT in 100ns units 10 of these per ms
        // div by 10000 to get ms

        // compute how long we were idle
        devExt->Fdo.StatWorkIdleTime =
            (ULONG) ((t2.QuadPart - t1.QuadPart) / 10000);

        // see if we have work to do
        LOGENTRY(NULL, fdoDeviceObject, LOG_NOISY, 'wakW', 0, 0,
            devExt->Fdo.StatWorkIdleTime);

        // if someone is setting the event we stall here, the event will
        // be signalled and we will reset it.  This is OK because we
        // have not done any work yet
        KeAcquireSpinLock(&devExt->Fdo.WorkerThreadSpin.sl, &irql);
        // if someone sets the event they will stall here, until we reset
        // the event -- it will cause us to loop around again but that is
        // no big deal.
        KeResetEvent(&devExt->Fdo.WorkerThreadEvent);
        KeReleaseSpinLock(&devExt->Fdo.WorkerThreadSpin.sl, irql);
        // now doing work
        // at this point once work is complete we will wait until someone
        // else signals

        // don't do work unless we are started
        if (TEST_FLAG(devExt->Fdo.MpStateFlags, MP_STATE_STARTED)) {
            USBPORT_DoSetPowerD0(fdoDeviceObject);

            // BUGBUG HP ia64 fix
            if (TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_SIGNAL_RH)) {
                PDEVICE_OBJECT usb2Fdo;
                PDEVICE_EXTENSION usb2DevExt;

                usb2Fdo = USBPORT_FindUSB2Controller(fdoDeviceObject);

                GET_DEVICE_EXT(usb2DevExt, usb2Fdo);
                ASSERT_FDOEXT(usb2DevExt);

                USBPORT_DoRootHubCallback(fdoDeviceObject, usb2Fdo);
                CLEAR_FDO_FLAG(devExt, USBPORT_FDOFLAG_SIGNAL_RH);

                // allow 2.0 controller to suspend
                InterlockedDecrement(&usb2DevExt->Fdo.PendingRhCallback);
                LOGENTRY(NULL, fdoDeviceObject, LOG_PNP, 'prh-', 0, 0,
                    usb2DevExt->Fdo.PendingRhCallback);
            }

            if (TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_CATC_TRAP)) {
                USBPORT_EndTransmitTriggerPacket(fdoDeviceObject);
            }

            USBPORT_Worker(fdoDeviceObject);
        }

    } while (!TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_KILL_THREAD));

    // cancel any wake irp we may have pending
    USBPORT_DisarmHcForWake(fdoDeviceObject);

    LOGENTRY(NULL, fdoDeviceObject, LOG_MISC, 'Ttrm', 0, 0, 0);

    // kill ourselves
    PsTerminateSystemThread(STATUS_SUCCESS);

}


VOID
USBPORT_TerminateWorkerThread(
    PDEVICE_OBJECT FdoDeviceObject
    )
/*++

Routine Description:

    Terminate the USBPORT Worker thread synchronously

Arguments:

Return Value:

    none

--*/
{
    PDEVICE_EXTENSION devExt;
    NTSTATUS status;
    PVOID threadObject;
    KIRQL irql;

    GET_DEVICE_EXT(devExt, FdoDeviceObject);
    ASSERT_FDOEXT(devExt);

    if (!TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_THREAD_INIT)) {
        return;
    }

    // signal our thread to terminate

    LOGENTRY(NULL, FdoDeviceObject, LOG_PNP, 'Tthr', 0, 0, 0);
    SET_FDO_FLAG(devExt, USBPORT_FDOFLAG_KILL_THREAD);

    // reference it so it won't go away before
    // we wait for it to finish

    status = ObReferenceObjectByHandle(devExt->Fdo.WorkerThreadHandle,
                                       SYNCHRONIZE,
                                       NULL,
                                       KernelMode,
                                       &threadObject,
                                       NULL);

    USBPORT_ASSERT(NT_SUCCESS(status))

    // signal worker takes the spinlock so on the off chance that
    // there is work being done this will stall
    USBPORT_SignalWorker(FdoDeviceObject);

    LOGENTRY(NULL, FdoDeviceObject, LOG_PNP, 'ThWt', 0, 0, status);
    // wait for thread to finish
    KeWaitForSingleObject(
                    threadObject,
                    Executive,
                    KernelMode,
                    FALSE,
                    NULL);

    ObDereferenceObject(threadObject);
    ZwClose(devExt->Fdo.WorkerThreadHandle);
    devExt->Fdo.WorkerThreadHandle = NULL;

    LOGENTRY(NULL, FdoDeviceObject, LOG_PNP, 'TthD', 0, 0, 0);

    CLEAR_FDO_FLAG(devExt, USBPORT_FDOFLAG_THREAD_INIT);

}


NTSTATUS
USBPORT_CreateWorkerThread(
    PDEVICE_OBJECT FdoDeviceObject
    )
/*++

Routine Description:

    Create the USBPORT Worker thread

Arguments:

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    PDEVICE_EXTENSION devExt;

    GET_DEVICE_EXT(devExt, FdoDeviceObject);
    ASSERT_FDOEXT(devExt);

    CLEAR_FDO_FLAG(devExt, USBPORT_FDOFLAG_KILL_THREAD);

    // initialize to NOT signaled
    // we initialize here because the we may signal
    // the event before the thread starts if we get
    // an interrupt.

    KeInitializeEvent(&devExt->Fdo.WorkerThreadEvent,
                      NotificationEvent,
                      FALSE);

    ntStatus =
        PsCreateSystemThread(&devExt->Fdo.WorkerThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        (HANDLE)0L,
        NULL,
        USBPORT_WorkerThread,
        FdoDeviceObject);

    if (NT_SUCCESS(ntStatus)) {
        SET_FDO_FLAG(devExt, USBPORT_FDOFLAG_THREAD_INIT);
    }

    LOGENTRY(NULL, FdoDeviceObject, LOG_PNP, 'crTH', 0, 0, ntStatus);

    return ntStatus;
}


VOID
USBPORT_SignalWorker(
    PDEVICE_OBJECT FdoDeviceObject
    )
/*++

Routine Description:

    Signal that there is work to do.

Arguments:

Return Value:

    None.

--*/
{
    PDEVICE_EXTENSION devExt;
    KIRQL irql;

    GET_DEVICE_EXT(devExt, FdoDeviceObject);
    ASSERT_FDOEXT(devExt);

    devExt->Fdo.StatWorkSignalCount++;

    KeAcquireSpinLock(&devExt->Fdo.WorkerThreadSpin.sl, &irql);
    LOGENTRY(NULL, FdoDeviceObject, LOG_NOISY, 'sigW', FdoDeviceObject, 0, 0);
    KeSetEvent(&devExt->Fdo.WorkerThreadEvent,
               1,
               FALSE);
    KeReleaseSpinLock(&devExt->Fdo.WorkerThreadSpin.sl, irql);
}


VOID
USBPORT_PowerWork(
    PVOID Context
    )
/*++

Routine Description:

Arguments:

Return Value:

    None.

--*/
{
    PUSB_POWER_WORK powerWork = Context;

    USBPORT_DoSetPowerD0(powerWork->FdoDeviceObject);

    DECREMENT_PENDING_REQUEST_COUNT(powerWork->FdoDeviceObject, NULL);

    FREE_POOL(powerWork->FdoDeviceObject, powerWork);
}


VOID
USBPORT_QueuePowerWorkItem(
    PDEVICE_OBJECT FdoDeviceObject
    )
/*++

Routine Description:

Arguments:

Return Value:

    None.

--*/
{
    PUSB_POWER_WORK powerWork;

    ALLOC_POOL_Z(powerWork, NonPagedPool, sizeof(*powerWork));

    // if the allocation fails the power work will be
    // deferred to our worker thread, this workitem is
    // just an  optimization

    if (powerWork != NULL) {
        ExInitializeWorkItem(&powerWork->QueueItem,
                             USBPORT_PowerWork,
                             powerWork);
        powerWork->FdoDeviceObject = FdoDeviceObject;

        INCREMENT_PENDING_REQUEST_COUNT(FdoDeviceObject, NULL);
        ExQueueWorkItem(&powerWork->QueueItem,
                        CriticalWorkQueue);

    }
}


VOID
USBPORT_DoSetPowerD0(
    PDEVICE_OBJECT FdoDeviceObject
    )
/*++

Routine Description:

Arguments:

Return Value:

    None.

--*/
{
    KIRQL irql;
    PDEVICE_EXTENSION devExt;
    ULONG controllerDisarmTime;

    GET_DEVICE_EXT(devExt, FdoDeviceObject);
    ASSERT_FDOEXT(devExt);

    KeAcquireSpinLock(&devExt->Fdo.PowerSpin.sl, &irql);
    // see if we need to power on
    if (TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_NEED_SET_POWER_D0)) {

#ifdef XPSE
        LARGE_INTEGER dt, t1, t2;
#endif

        CLEAR_FDO_FLAG(devExt, USBPORT_FDOFLAG_NEED_SET_POWER_D0);
        KeReleaseSpinLock(&devExt->Fdo.PowerSpin.sl, irql);

#ifdef XPSE
        // compute time to thread signal and wake
        KeQuerySystemTime(&t1);
        dt.QuadPart = t1.QuadPart - devExt->Fdo.ThreadResumeTimeStart.QuadPart;

        devExt->Fdo.ThreadResumeTime = (ULONG) (dt.QuadPart/10000);

        USBPORT_KdPrint((1, "(%x)  ThreadResumeTime %d ms \n",
            devExt, devExt->Fdo.ThreadResumeTime));
#endif

        // synchronously cancel the wake irp we have
        // in PCI so we don't get a completeion while
        // we power up.
        KeQuerySystemTime(&t1);
        USBPORT_DisarmHcForWake(FdoDeviceObject);
        KeQuerySystemTime(&t2);
        dt.QuadPart = t2.QuadPart - t1.QuadPart;

        controllerDisarmTime = (ULONG) (dt.QuadPart/10000);
        USBPORT_KdPrint((1, "(%x)  ControllerDisarmTime %d ms \n",
            devExt, controllerDisarmTime));

#ifdef XPSE
        // time the hw resume/start
        KeQuerySystemTime(&t1);
#endif

        // The goal here is to wait for the USB2 and its CCs to start
        // then make sure that the 20 controller holds the shared port
        // semaphore

        if (TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_OFF)) {
            USBPORT_TurnControllerOn(FdoDeviceObject);
            USBPORT_SynchronizeControllersResume(FdoDeviceObject);

            if (TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_IS_CC)) {
                // if this is a CC then power the ports here
                // the USB 2 controller holds the semaphore on
                // return from USBPORT_SynchronizeControllersResume
                USBPORT_KdPrint((1, " >power-chirp CC ports (on)\n"));
                USBPORT_RootHub_PowerAndChirpAllCcPorts(FdoDeviceObject);
            }
        } else {
            // complete the power irp, the controller is on
            // but is still 'suspended'
            USBPORT_RestoreController(FdoDeviceObject);
            USBPORT_SynchronizeControllersResume(FdoDeviceObject);
        }



#ifdef XPSE
        // compute time to start controller
        KeQuerySystemTime(&t2);
        dt.QuadPart = t2.QuadPart - t1.QuadPart;

        devExt->Fdo.ControllerResumeTime = (ULONG) (dt.QuadPart/10000);

        USBPORT_KdPrint((1, "(%x)  ControllerResumeTime %d ms \n",
            devExt, devExt->Fdo.ControllerResumeTime));

        // compute time to S0;
        KeQuerySystemTime(&t2);
        dt.QuadPart = t2.QuadPart - devExt->Fdo.S0ResumeTimeStart.QuadPart;

        devExt->Fdo.S0ResumeTime = (ULONG) (dt.QuadPart/10000);

        USBPORT_KdPrint((1, "(%x)  D0ResumeTime %d ms \n", devExt,
            devExt->Fdo.D0ResumeTime));
        USBPORT_KdPrint((1, "(%x)  S0ResumeTime %d ms \n", devExt,
            devExt->Fdo.S0ResumeTime));
#endif

        if (TEST_FDO_FLAG(devExt, USBPORT_FDOFLAG_RESUME_SIGNALLING)) {
            CLEAR_FDO_FLAG(devExt, USBPORT_FDOFLAG_RESUME_SIGNALLING);
            USBPORT_HcQueueWakeDpc(FdoDeviceObject);
        }
    } else {
        KeReleaseSpinLock(&devExt->Fdo.PowerSpin.sl, irql);
    }

}


VOID
USBPORT_SynchronizeControllersResume(
    PDEVICE_OBJECT FdoDeviceObject
    )
/*++

Routine Description:

    Synchronize the USB 2 controllers with companions.

    This routines blocks all dependent controllers unt their
    hardware is restored.  At that point it takes the CC lock
    for the USB 2 controller and allows all the controllers to
    resume.

    The CC lock protects the shared port registers from simultaneous
    access.

Arguments:

Return Value:

    None.

    The USB 2 controller holds the CC lock on return from this function

--*/
{
    PDEVICE_EXTENSION devExt;
    PDEVICE_OBJECT usb2Fdo;

    ASSERT_PASSIVE();

    GET_DEVICE_EXT(devExt, FdoDeviceObject);
    ASSERT_FDOEXT(devExt);

    LOGENTRY(NULL, FdoDeviceObject, LOG_RH, 'SYN2', FdoDeviceObject, 0, 0);

    if (USBPORT_IS_USB20(devExt)) {
        usb2Fdo =  FdoDeviceObject;
    } else {
        usb2Fdo =  USBPORT_FindUSB2Controller(FdoDeviceObject);
    }

    // may get NULL if no 2.0 controller registered
    // don't wait if not CCs or other controllers

    if (usb2Fdo) {
        PDEVICE_EXTENSION usb2DevExt, rhDevExt;

        LOGENTRY(NULL, FdoDeviceObject, LOG_RH, 'u2cc', FdoDeviceObject,
            usb2Fdo, 0);

        GET_DEVICE_EXT(usb2DevExt, usb2Fdo);
        ASSERT_FDOEXT(usb2DevExt);

        GET_DEVICE_EXT(rhDevExt, usb2DevExt->Fdo.RootHubPdo);
        ASSERT_PDOEXT(rhDevExt);


        // sync with the CC if this is a USB 2 controller
        // note that we only grab the CC lock if the root
        // hub PDO is enabled since it is released only
        // when the root hub is set to D0 -- this will never
        // happen if the rh is disabled

        if (USBPORT_IS_USB20(devExt) &&
            !TEST_FLAG(rhDevExt->PnpStateFlags, USBPORT_PNP_REMOVED)) {

            KeWaitForSingleObject(&usb2DevExt->Fdo.CcLock,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);
            USBPORT_ASSERT(!TEST_FDO_FLAG(usb2DevExt,
                    USBPORT_FDOFLAG_CC_LOCK));
            SET_FDO_FLAG(usb2DevExt, USBPORT_FDOFLAG_CC_LOCK);
            LOGENTRY(NULL, FdoDeviceObject, LOG_RH, 'grcc', FdoDeviceObject,
                usb2Fdo, 0);

            USBPORT_KdPrint((1, " >power 20 (on) %x\n",
                    FdoDeviceObject));
        }

        InterlockedDecrement(&usb2DevExt->Fdo.DependentControllers);

        // at this point any of the dependent controllers can continue

        do {
            USBPORT_Wait(FdoDeviceObject, 10);

            // sync with the CC if this is a USB 2 controller
            // note that we only grab the CC lock if the root
            // hub PDO is enabled since it is released only
            // when the root hub is set to D0 -- this will never
            // happen if the rh is disabled

        } while (usb2DevExt->Fdo.DependentControllers);

        LOGENTRY(NULL, FdoDeviceObject, LOG_RH, 'u2GO', FdoDeviceObject,
            usb2Fdo, 0);

    }

}

