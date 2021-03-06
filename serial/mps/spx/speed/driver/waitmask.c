/*++

Copyright (c) 1991, 1992, 1993 Microsoft Corporation

Module Name:

    waitmask.c

Abstract:

    This module contains the code that is very specific to get/set/wait
    on event mask operations in the serial driver

Author:

    Anthony V. Ercolano 26-Sep-1991

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"

// Prototypes
BOOLEAN SerialGrabWaitFromIsr(IN PVOID Context);
BOOLEAN SerialGiveWaitToIsr(IN PVOID Context);
BOOLEAN SerialFinishOldWait(IN PVOID Context);
// End of prototypes    
    

#ifdef ALLOC_PRAGMA
#endif


NTSTATUS
SerialStartMask(IN PPORT_DEVICE_EXTENSION pPort)
/*++

Routine Description:

    This routine is used to process the set mask and wait
    mask ioctls.  Calls to this routine are serialized by
    placing irps in the list under the protection of the
    cancel spin lock.

Arguments:

    Extension - A pointer to the serial device extension.

Return Value:

    Will return pending for everything put the first
    request that we actually process.  Even in that
    case it will return pending unless it can complete
    it right away.


--*/
{

    //
    // The current stack location.  This contains much of the
    // information we need to process this particular request.
    //
    PIO_STACK_LOCATION IrpSp;

    PIRP NewIrp;

    BOOLEAN SetFirstStatus = FALSE;
    NTSTATUS FirstStatus;

    SerialDump(SERDIAG3,("In SerialStartMask\n"));

    ASSERT(pPort->CurrentMaskIrp);

    do 
	{
        SerialDump(SERDIAG4,("STARTMASK - CurrentMaskIrp: %x\n", pPort->CurrentMaskIrp));

        IrpSp = IoGetCurrentIrpStackLocation(pPort->CurrentMaskIrp);

        ASSERT((IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_WAIT_ON_MASK) 
			|| (IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_SET_WAIT_MASK));
                

        if(IrpSp->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_SET_WAIT_MASK)
		{

            SerialDump(SERDIAG4, ("SERIAL - %x is a SETMASK irp\n", pPort->CurrentMaskIrp));
                
            // Complete the old wait if there is one.
            KeSynchronizeExecution(pPort->Interrupt, SerialFinishOldWait, pPort);
                

            //
            // Any current waits should be on its way to completion
            // at this point.  There certainly shouldn't be any
            // irp mask location.
            //

            ASSERT(!pPort->IrpMaskLocation);

            pPort->CurrentMaskIrp->IoStatus.Status = STATUS_SUCCESS;

            if(!SetFirstStatus) 
			{
                SerialDump(SERDIAG4,("%x was the first irp processed by this\n"
									   "------- invocation of startmask\n", pPort->CurrentMaskIrp));
                    
                FirstStatus = STATUS_SUCCESS;
                SetFirstStatus = TRUE;
            }

            // The following call will also cause the current call to be completed.
            SerialGetNextIrp(pPort, &pPort->CurrentMaskIrp, &pPort->MaskQueue, &NewIrp, TRUE);
                
                
            SerialDump(SERDIAG4,("Perhaps another mask irp was found in the queue\n"
                "------- %x/%x <- values should be the same\n", pPort->CurrentMaskIrp, NewIrp));

        } 
		else 
		{
            //
            // First make sure that we have a non-zero mask.
            // If the app queues a wait on a zero mask it can't
            // be statisfied so it makes no sense to start it.
            //

            if((!pPort->IsrWaitMask) || (pPort->CurrentWaitIrp)) 
			{

                SerialDump(SERDIAG4,("WaitIrp is invalid\n"
                    "------- IsrWaitMask: %x\n"
                    "------- CurrentWaitIrp: %x\n",
                     pPort->IsrWaitMask,
                     pPort->CurrentWaitIrp));
                     

                pPort->CurrentMaskIrp->IoStatus.Status = STATUS_INVALID_PARAMETER;

                if(!SetFirstStatus) 
				{
                    SerialDump(SERDIAG4,("%x was the first irp processed by this\n"
                         "------- invocation of startmask\n", pPort->CurrentMaskIrp));
                        
                    FirstStatus = STATUS_INVALID_PARAMETER;
                    SetFirstStatus = TRUE;
                }

                SerialGetNextIrp(pPort, &pPort->CurrentMaskIrp, &pPort->MaskQueue, &NewIrp, TRUE);
                    
                    
                    
                SerialDump(SERDIAG4,("Perhaps another mask irp was found in the queue\n"
                    "------- %x/%x <- values should be the same\n",
                    pPort->CurrentMaskIrp,NewIrp));

            } 
			else 
			{
                KIRQL OldIrql;

                //
                // Make the current mask irp the current wait irp and
                // get a new current mask irp.  Note that when we get
                // the new current mask irp we DO NOT complete the
                // old current mask irp (which is now the current wait
                // irp.
                //
                // Then under the protection of the cancel spin lock
                // we check to see if the current wait irp needs to
                // be canceled
                //

                IoAcquireCancelSpinLock(&OldIrql);

                if(pPort->CurrentMaskIrp->Cancel) 
				{
                    SerialDump(SERDIAG4, ("%x irp was already marked as cancelled\n", pPort->CurrentMaskIrp));
                         
                    IoReleaseCancelSpinLock(OldIrql);
                    pPort->CurrentMaskIrp->IoStatus.Status = STATUS_CANCELLED;

                    if(!SetFirstStatus) 
					{
                        SerialDump(SERDIAG4, ("%x was the first irp processed by this\n"
                             "------- invocation of startmask\n", pPort->CurrentMaskIrp));
                            
                        FirstStatus = STATUS_CANCELLED;
                        SetFirstStatus = TRUE;
                    }

                    SerialGetNextIrp(pPort, &pPort->CurrentMaskIrp, &pPort->MaskQueue, &NewIrp, TRUE);
                        
                    SerialDump(SERDIAG4,("Perhaps another mask irp was found in the queue\n"
                        "------- %x/%x <- values should be the same\n", pPort->CurrentMaskIrp, NewIrp));
                } 
				else 
				{

                    SerialDump(SERDIAG4, ("%x will become the current wait irp\n", pPort->CurrentMaskIrp));
                        
                    if(!SetFirstStatus) 
					{

                        SerialDump(SERDIAG4,("%x was the first irp processed by this\n"
                            "------- invocation of startmask\n", pPort->CurrentMaskIrp));
                            
                        FirstStatus = STATUS_PENDING;
                        SetFirstStatus = TRUE;

                        //
                        // If we haven't already set a first status
                        // then there is a chance that this packet
                        // was never on the queue.  We should mark
                        // it as pending.
                        //

                        IoMarkIrpPending(pPort->CurrentMaskIrp);
                    }

                    //
                    // There should never be a mask location when
                    // there isn't a current wait irp.  At this point
                    // there shouldn't be a current wait irp also.
                    //

                    ASSERT(!pPort->IrpMaskLocation);
                    ASSERT(!pPort->CurrentWaitIrp);

                    pPort->CurrentWaitIrp = pPort->CurrentMaskIrp;
                    SERIAL_INIT_REFERENCE(pPort->CurrentWaitIrp);
                    IoSetCancelRoutine(pPort->CurrentWaitIrp, SerialCancelWait);
                        
                    //
                    // Since the cancel routine has a reference to#
                    // the irp we need to update the reference
                    // count.
                    //

                    SERIAL_SET_REFERENCE(pPort->CurrentWaitIrp, SERIAL_REF_CANCEL);

                    KeSynchronizeExecution(pPort->Interrupt, SerialGiveWaitToIsr, pPort);
                        

                    //
                    // Since it isn't really the mask irp anymore,
                    // null out that pointer.
                    //

                    pPort->CurrentMaskIrp = NULL;

                    IoReleaseCancelSpinLock(OldIrql);

                    SerialGetNextIrp(pPort, &pPort->CurrentMaskIrp, &pPort->MaskQueue, &NewIrp, FALSE);
                       
                    SerialDump(SERDIAG4,("Perhaps another mask irp was found in the queue\n"
                        "------- %x/%x <- values should be the same\n", pPort->CurrentMaskIrp, NewIrp));
                       
                }

            }

        }

    } while (NewIrp);

    return FirstStatus;

}


BOOLEAN
SerialGrabWaitFromIsr(IN PVOID Context)
/*++

Routine Description:

    This routine will check to see if the ISR still knows about
    a wait irp by checking to see if the IrpMaskLocation is non-null.
    If it is then it will zero the Irpmasklocation (which in effect
    grabs the irp away from the isr).  This routine is only called
    buy the cancel code for the wait.

    NOTE: This is called by KeSynchronizeExecution.

Arguments:

    Context - A pointer to the device extension

Return Value:

    Always FALSE.

--*/

{

    PPORT_DEVICE_EXTENSION pPort = Context;

    SerialDump(SERDIAG3,("In SerialGrabWaitFromIsr\n"));
        
        
        

    if(pPort->IrpMaskLocation) 
	{
        SerialDump(SERDIAG4,("The isr still owns the irp %x, mask location is %x\n"
             "------- and system buffer is %x\n",
             pPort->CurrentWaitIrp, pPort->IrpMaskLocation,
             pPort->CurrentWaitIrp->AssociatedIrp.SystemBuffer));
            

        // The isr still "owns" the irp.

        *pPort->IrpMaskLocation = 0;
        pPort->IrpMaskLocation = NULL;

        pPort->CurrentWaitIrp->IoStatus.Information = sizeof(ULONG);

        //
        // Since the isr no longer references the irp we need to
        // decrement the reference count.
        //

        SERIAL_CLEAR_REFERENCE(pPort->CurrentWaitIrp, SERIAL_REF_ISR);
            
    }

    return FALSE;
}


BOOLEAN
SerialGiveWaitToIsr(IN PVOID Context)
/*++

Routine Description:

    This routine simply sets a variable in the device extension
    so that the isr knows that we have a wait irp.

    NOTE: This is called by KeSynchronizeExecution.

    NOTE: This routine assumes that it is called with the
          cancel spinlock held.

Arguments:

    Context - Simply a pointer to the device extension.

Return Value:

    Always FALSE.

--*/

{

    PPORT_DEVICE_EXTENSION pPort = Context;

    SerialDump(SERDIAG3,("In SerialGiveWaitToIsr\n"));
        
    //
    // There certainly shouldn't be a current mask location at
    // this point since we have a new current wait irp.
    //

    ASSERT(!pPort->IrpMaskLocation);

    //
    // The isr may or may not actually reference this irp.  It
    // won't if the wait can be satisfied immediately.  However,
    // since it will then go through the normal completion sequence,
    // we need to have an incremented reference count anyway.
    //

    SERIAL_SET_REFERENCE(pPort->CurrentWaitIrp, SERIAL_REF_ISR);
        
        
       

    if(!pPort->HistoryMask) 
	{
        SerialDump(SERDIAG4, ("No events occured prior to the wait call\n"));
            
        //
        // Although this wait might not be for empty transmit
        // queue, it doesn't hurt anything to set it to false.
        //

        pPort->EmptiedTransmit = FALSE;

        //
        // Record where the "completion mask" should be set.
        //

        pPort->IrpMaskLocation = pPort->CurrentWaitIrp->AssociatedIrp.SystemBuffer;
        

        SerialDump(SERDIAG4,("The isr owns the irp %x, mask location is %x\n"
             "------- and system buffer is %x\n",
             pPort->CurrentWaitIrp, pPort->IrpMaskLocation,
             pPort->CurrentWaitIrp->AssociatedIrp.SystemBuffer));
    } 
	else 
	{

        SerialDump(SERDIAG4, ("%x occurred prior to the wait - starting the\n"
             "------- completion code for %x\n",
             pPort->HistoryMask, pPort->CurrentWaitIrp));
            
        *((ULONG *)pPort->CurrentWaitIrp->AssociatedIrp.SystemBuffer) = pPort->HistoryMask;
           
        pPort->HistoryMask = 0;
        pPort->CurrentWaitIrp->IoStatus.Information = sizeof(ULONG);
        pPort->CurrentWaitIrp->IoStatus.Status = STATUS_SUCCESS;

 		// Mark IRP as about to complete normally to prevent cancel & timer DPCs
		// from doing so before DPC is allowed to run.
		//SERIAL_SET_REFERENCE(pPort->CurrentWaitIrp, SERIAL_REF_COMPLETING);
       
		KeInsertQueueDpc(&pPort->CommWaitDpc, NULL, NULL);
            
    }

    return FALSE;
}


BOOLEAN
SerialFinishOldWait(IN PVOID Context)
/*++

Routine Description:

    This routine will check to see if the ISR still knows about
    a wait irp by checking to see if the Irpmasklocation is non-null.
    If it is then it will zero the Irpmasklocation (which in effect
    grabs the irp away from the isr).  This routine is only called
    buy the cancel code for the wait.

    NOTE: This is called by KeSynchronizeExecution.

Arguments:

    Context - A pointer to the device extension

Return Value:

    Always FALSE.

--*/

{

    PPORT_DEVICE_EXTENSION pPort = Context;

    SerialDump(SERDIAG3,("In SerialFinishOldWait\n"));
        
        
       
    if(pPort->IrpMaskLocation) 
	{

        SerialDump(SERDIAG4, ("The isr still owns the irp %x, mask location is %x\n"
             "------- and system buffer is %x\n",
             pPort->CurrentWaitIrp, pPort->IrpMaskLocation,
             pPort->CurrentWaitIrp->AssociatedIrp.SystemBuffer));
   
		//
        // The isr still "owns" the irp.
        //
        *pPort->IrpMaskLocation = 0;
        pPort->IrpMaskLocation = NULL;

        pPort->CurrentWaitIrp->IoStatus.Information = sizeof(ULONG);

        //
        // We don't decrement the reference since the completion routine
        // will do that.
        //

		// Mark IRP as about to complete normally to prevent cancel & timer DPCs
		// from doing so before DPC is allowed to run.
		//SERIAL_SET_REFERENCE(pPort->CurrentWaitIrp, SERIAL_REF_COMPLETING);

        KeInsertQueueDpc(&pPort->CommWaitDpc, NULL, NULL);
            
    }

    //
    // Don't wipe out any historical data we are still interested in.
    //

    pPort->HistoryMask &= *((ULONG *)pPort->CurrentMaskIrp->AssociatedIrp.SystemBuffer);
                                            
    pPort->IsrWaitMask = *((ULONG *)pPort->CurrentMaskIrp->AssociatedIrp.SystemBuffer);

	// Setup UART for special character detection
	if(pPort->IsrWaitMask & SERIAL_EV_RXFLAG)
	{
		pPort->UartConfig.SpecialMode |= UC_SM_DETECT_SPECIAL_CHAR;
		pPort->pUartLib->UL_SetConfig_XXXX(pPort->pUart, &pPort->UartConfig, UC_SPECIAL_MODE_MASK);
	}
	else
	{
		pPort->UartConfig.SpecialMode &= ~UC_SM_DETECT_SPECIAL_CHAR;
		pPort->pUartLib->UL_SetConfig_XXXX(pPort->pUart, &pPort->UartConfig, UC_SPECIAL_MODE_MASK);
	}

    SerialDump(SERDIAG4, ("Set mask location of %x, in irp %x, with system buffer of %x\n",
         pPort->IrpMaskLocation,
         pPort->CurrentMaskIrp, pPort->CurrentMaskIrp->AssociatedIrp.SystemBuffer));
        
        
    return FALSE;
}


VOID
SerialCancelWait(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
/*++

Routine Description:

    This routine is used to cancel a irp that is waiting on
    a comm event.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    None.

--*/

{
    PPORT_DEVICE_EXTENSION pPort = DeviceObject->DeviceExtension;

    SerialDump(SERDIAG3, ("In SerialCancelWait\n"));
        
    SerialDump(SERDIAG4, ("Canceling wait for irp %x\n", pPort->CurrentWaitIrp));
        
    SerialTryToCompleteCurrent(	pPort,
								SerialGrabWaitFromIsr,
								Irp->CancelIrql,
								STATUS_CANCELLED,
								&pPort->CurrentWaitIrp,
								NULL,
								NULL,
								NULL,
								NULL,
								NULL,
								SERIAL_REF_CANCEL);
        

}


VOID
SerialCompleteWait(IN PKDPC Dpc,
				   IN PVOID DeferredContext,
				   IN PVOID SystemContext1,
				   IN PVOID SystemContext2)
{

    PPORT_DEVICE_EXTENSION pPort = DeferredContext;
    KIRQL OldIrql;

    SerialDump(SERDIAG3, ("In SerialCompleteWait\n"));
       
        
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemContext1);
    UNREFERENCED_PARAMETER(SystemContext2);

    IoAcquireCancelSpinLock(&OldIrql);

    SerialDump(SERDIAG4, ("Completing wait for irp %x\n", pPort->CurrentWaitIrp));
   
	// Clear the normal complete reference.
	//SERIAL_CLEAR_REFERENCE(pPort->CurrentWaitIrp, SERIAL_REF_COMPLETING);

    SerialTryToCompleteCurrent(	pPort,
								NULL,
								OldIrql,
								STATUS_SUCCESS,
								&pPort->CurrentWaitIrp,
								NULL,
								NULL,
								NULL,
								NULL,
								NULL,
								SERIAL_REF_ISR);
								

}
