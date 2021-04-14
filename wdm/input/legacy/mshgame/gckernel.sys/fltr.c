//	@doc
/**********************************************************************
*
*	@module	FLTR.c	|
*
*	Implementation of basic IRP handlers for Filter Device Objects
*
*	History
*	----------------------------------------------------------
*	Mitchell S. Dernis	Original
*
*	(c) 1986-1998 Microsoft Corporation. All right reserved.
*
*	@topic	FLTR	|
*	The non-Power and PnP IRP handler routines are handled in this module
*	for all Device Objects created as filters for the raw HID-Pdos.
*
**********************************************************************/
#define __DEBUG_MODULE_IN_USE__ GCK_FLTR_C

#include <wdm.h>
#include "Debug.h"
#include "GckShell.h"

DECLARE_MODULE_DEBUG_LEVEL((DBG_WARN|DBG_ERROR|DBG_CRITICAL));

//
//	Mark the pageable routines as such
//
#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, GCK_FLTR_DriverEntry)
#pragma alloc_text (PAGE, GCK_FLTR_Create)
#pragma alloc_text (PAGE, GCK_FLTR_Close)
#pragma alloc_text (PAGE, GCK_FLTR_Ioctl)
#pragma alloc_text (PAGE, GCK_FLTR_Unload)
#endif
/***********************************************************************************
**
**	NTSTATUS GCK_FLTR_DriverEntry(IN PDRIVER_OBJECT  pDriverObject,  IN PUNICODE_STRING pRegistryPath )
**
**	@func	Initializing the portions of the driver related to the filter devices.
**
**	@rdesc	STATUS_SUCCESS
**
*************************************************************************************/
NTSTATUS GCK_FLTR_DriverEntry
(
	IN PDRIVER_OBJECT  pDriverObject,	// @parm Driver Object
	IN PUNICODE_STRING puniRegistryPath	// @parm Path to driver specific registry section.
)
{	
	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(puniRegistryPath);
	//
	//	Initialize Globals related to filter devices
	//
	GCK_DBG_TRACE_PRINT(("Initializing globals\n"));
	Globals.ulFilteredDeviceCount = 0;
	Globals.pFilterObjectList = NULL;
	Globals.pSWVB_FilterExt=NULL;				// Nobody owns the virtual bus yet.
	Globals.pVirtualKeyboardPdo = NULL;		// No keyboard object
	Globals.ulVirtualKeyboardRefCount = 0;	// No keyboard users
	ExInitializeFastMutex(&Globals.FilterObjectListFMutex);
	return	STATUS_SUCCESS;
}

/***********************************************************************************
**
**	NTSTATUS GCK_FLTR_Create ( IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp )
**
**	@func	Handles the IRP_MJ_CREATE for filter devices
**			- Called generated by Win32 API CreateFile or OpenFile
**
**	@rdesc	STATUS_SUCCESS, or various error codes
**
*************************************************************************************/
NTSTATUS GCK_FLTR_Create (
	IN PDEVICE_OBJECT pDeviceObject,	// @parm DO target for IRP
	IN PIRP pIrp						// @parm IRP
)
{
    
	NTSTATUS            NtStatus = STATUS_SUCCESS;
	PGCK_FILTER_EXT		pFilterExt;
	PDEVICE_OBJECT		pCurDeviceObject;
	PIO_STACK_LOCATION	pIrpStack;
	KEVENT				SyncEvent;
	USHORT				usShareAccess;
		
	PAGED_CODE ();

	GCK_DBG_ENTRY_PRINT (("Entering GCK_FLTR\n"));
	
	//cast device extension to proper type
	pFilterExt = (PGCK_FILTER_EXT) pDeviceObject->DeviceExtension;

	// Just an extra sanity check
	ASSERT(	GCK_DO_TYPE_FILTER == pFilterExt->ulGckDevObjType);
    
	//
    // Increment IRP count, ASAP, note we couldn't do this until now, as we
    // knew not whether the device extension was really a GCK_FILTER_EXT
	//
	GCK_IncRemoveLock(&pFilterExt->RemoveLock);
    
	//	Make sure we are not in the process of going away
	if( 
		GCK_STATE_STARTED != pFilterExt->eDeviceState &&
		GCK_STATE_STOP_PENDING != pFilterExt->eDeviceState
	)
	{
        GCK_DBG_WARN_PRINT(("Create while remove pending\n"));
		NtStatus = STATUS_DELETE_PENDING;
        pIrp->IoStatus.Information = 0;
        pIrp->IoStatus.Status = NtStatus;
        IoCompleteRequest (pIrp, IO_NO_INCREMENT);
	} 
	else // process this
	{
        GCK_DBG_TRACE_PRINT(("GCK_Create calling lower driver\n"));
		pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

		//
		//We lie to hidclass about the desired share access,
		//because it doesn't know not to count us.
		//Notice below that we restore this before passing to our
		//internal poll routines, so that we can track it there.
		//
		usShareAccess=pIrpStack->Parameters.Create.ShareAccess;
		pIrpStack->Parameters.Create.ShareAccess = FILE_READ_DATA|FILE_WRITE_DATA;

		//This is our internal poll trying to create a file object, just call down
		if(	pFilterExt->InternalPoll.InternalCreateThread == KeGetCurrentThread())
		{
			IoSkipCurrentIrpStackLocation(pIrp);
			NtStatus = IoCallDriver (pFilterExt->pTopOfStack, pIrp);
		}
		//This is a request from above, capture on the way up
		else
		{
			GCK_IP_AddFileObject(
				pFilterExt,
				pIrpStack->FileObject,
				pIrpStack->Parameters.Create.ShareAccess,
				pIrpStack->Parameters.Create.SecurityContext->DesiredAccess
				);

			GCKF_KickDeviceForData(pFilterExt);

			// Call create synchronously
			KeInitializeEvent(&SyncEvent, SynchronizationEvent,	FALSE);
       		IoCopyCurrentIrpStackLocationToNext(pIrp);
			IoSetCompletionRoutine(
				pIrp,
				GCK_FLTR_CreateComplete,
				(PVOID)&SyncEvent,
				TRUE,
				TRUE,
				TRUE
			);
			IoCallDriver (pFilterExt->pTopOfStack, pIrp);
			KeWaitForSingleObject(&SyncEvent, Executive, KernelMode, FALSE, NULL);
			NtStatus = pIrp->IoStatus.Status;
       		// If create succeeded, we need to remember the FileObject
			GCK_IP_ConfirmFileObject(pFilterExt, pIrpStack->FileObject, (BOOLEAN)(NT_SUCCESS(pIrp->IoStatus.Status) ? TRUE : FALSE) );
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);	// This was misssing!!!
		}
    }

    //
	//	We are done with this IRP, so decrement the outstanding count
	//	and signal remove if this was the last outstanding IRP
	//
	GCK_DecRemoveLock(&pFilterExt->RemoveLock);

    GCK_DBG_EXIT_PRINT(("Exiting GCK_Create(2). Status: 0x%0.8x\n", NtStatus));
    return NtStatus;
}

/***********************************************************************************
**
**	NTSTATUS GCK_FLTR_CreateComplete(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID pContext)
**
**	@mfunc	Completion Routine for IRP_MJ_CREATE
**
**	@rdesc	STATUS_SUCCESS
**
*************************************************************************************/
NTSTATUS
GCK_FLTR_CreateComplete
(
	IN PDEVICE_OBJECT pDeviceObject,
	IN PIRP pIrp,
	IN PVOID pContext
)
{
	PKEVENT pSyncEvent;
	UNREFERENCED_PARAMETER(pDeviceObject);
	UNREFERENCED_PARAMETER(pIrp);

	// Cast context to device extension
	pSyncEvent = (PKEVENT) pContext;
	KeSetEvent(pSyncEvent, IO_NO_INCREMENT, FALSE);
		
	//Done with this IRP, never need to see it again
	return STATUS_MORE_PROCESSING_REQUIRED;
}


/***********************************************************************************
**
**	NTSTATUS GCK_FLTR_Close ( IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp )
**
**	@func	Handles IRP_MJ_CLOSE for filter device objects - Call generated by Win32 API CloseFile
**
**	@rdesc	STATUS_SUCCESS or various errors
**
*************************************************************************************/
NTSTATUS GCK_FLTR_Close (
	IN PDEVICE_OBJECT pDeviceObject,	// @parm DO target for IRP
	IN PIRP pIrp						// @parm IRP
)
{
	NTSTATUS            NtStatus = STATUS_SUCCESS;
	PGCK_FILTER_EXT		pFilterExt;
	PIO_STACK_LOCATION	pIrpStack;

	PAGED_CODE ();
	
	GCK_DBG_ENTRY_PRINT (("GCK_Close, pDO = 0x%0.8x, pIrp = 0x%0.8x\n", pDeviceObject, pIrp));
    
	//cast device extension to proper type
	pFilterExt = (PGCK_FILTER_EXT) pDeviceObject->DeviceExtension;

	// Just an extra sanity check
	ASSERT(	GCK_DO_TYPE_FILTER == pFilterExt->ulGckDevObjType);
    
	//
    // Increment IRP count, ASAP, note we couldn't do this until now, as we
    // knew not whether the device extension was really a GCK_FILTER_EXT
	//
	GCK_IncRemoveLock(&pFilterExt->RemoveLock);

	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

	//cleanup pending IO and our tracking of the FileObject -
	//however not applicable if it is our internal polling open, that is being close
	if(pIrpStack->FileObject != pFilterExt->InternalPoll.pInternalFileObject)
	{
		//Complete pending I\O on FileObject
		if(
			GCK_STATE_STARTED == pFilterExt->eDeviceState ||
			GCK_STATE_STOP_PENDING == pFilterExt->eDeviceState
		)
		{
			//There is no pending I\O if the device is not started yet
			GCKF_CompleteReadRequestsForFileObject(pFilterExt, pIrpStack->FileObject);
		}
		//forget file object
		GCK_IP_RemoveFileObject(pFilterExt, pIrpStack->FileObject);
	}
	//send the Irp along
	IoSkipCurrentIrpStackLocation (pIrp);
	NtStatus = IoCallDriver(pFilterExt->pTopOfStack, pIrp);

		
	// Decrement outstanding IO and signal if went to zero
	GCK_DecRemoveLock(&pFilterExt->RemoveLock);

	GCK_DBG_EXIT_PRINT(("Exiting GCK_Close(2). Status: 0x%0.8x\n", NtStatus));
    return NtStatus;
}

/***********************************************************************************
**
**	NTSTATUS GCK_FLTR_Read (IN PDEVICE_OBJECT pDeviceObject,	IN PIRP pIrp)
**
**	@func	Handles IRP_MJ_READ for filter device objects - Generated by Win32 ReadFile
**
**	@rdesc	STATUS_SUCCESS, or various errors
**
*************************************************************************************/
NTSTATUS GCK_FLTR_Read 
(
	IN PDEVICE_OBJECT pDeviceObject,	// @parm Target of IRP
	IN PIRP pIrp						// @parm IRP to handle
)
{
	NTSTATUS            NtStatus;
	LARGE_INTEGER       lgiWaitTime;
	PGCK_FILTER_EXT		pFilterExt;
	PIO_STACK_LOCATION	pIrpStack;
	PIO_STACK_LOCATION	pPrivateIrpStack;
	PVOID				pvIrpBuffer;
	PFILE_OBJECT		pIrpsFileObject;
	static	int			iEnterCount=0;
	unsigned int i=0;
	
	GCK_DBG_RT_ENTRY_PRINT(("Entering GCK_Read. pDO = 0x%0.8x, pIrp = 0x%0.8x\n", pDeviceObject, pIrp));

    //cast device extension to proper type
	pFilterExt = (PGCK_FILTER_EXT) pDeviceObject->DeviceExtension;

	// Just an extra sanity check
	ASSERT(	GCK_DO_TYPE_FILTER == pFilterExt->ulGckDevObjType);
	
	//	Increment IRP count while we handle this one
	GCK_IncRemoveLock(&pFilterExt->RemoveLock);
    
	//	If we have been removed, we shouldn't be getting read IRPs
	if(
		GCK_STATE_STARTED != pFilterExt->eDeviceState &&
		GCK_STATE_STOP_PENDING  != pFilterExt->eDeviceState
	)
	{
		GCK_DBG_WARN_PRINT(( "GCK_Read called with delete pending\n"));
		NtStatus = STATUS_DELETE_PENDING;
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = NtStatus;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);

		//	Decrement outstanding IRP count, and signal if it want to zero
		GCK_DecRemoveLock(&pFilterExt->RemoveLock);
	} 
	else 
	{
		//
		//	Send Reqeust to filter, the filterhooks module is responsible
		//	for all the necessary Asynchronous IRP handling steps, we just
		//	return status pending.
		//
		NtStatus = GCKF_IncomingReadRequests(pFilterExt, pIrp);

		//	Poll the lower driver, if one is not yet pending.
		GCK_IP_OneTimePoll(pFilterExt);
	}
	
	GCK_DBG_RT_EXIT_PRINT(("Exiting GCK_Read(2). Status: 0x%0.8x\n", NtStatus)); 
    return NtStatus;
}

/***********************************************************************************
**
**	NTSTATUS GCK_FLTR_Ioctl (IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp)
**
**	@mfunc	Handles all IOCTLs for the filter devices - this is just a straight pass through
**
**	@rdesc	STATUS_SUCCESS, various errors
**
*************************************************************************************/
NTSTATUS GCK_FLTR_Ioctl 
(
	IN PDEVICE_OBJECT pDeviceObject,	// @parm pointer to Device Object
	IN PIRP pIrp						// @parm pointer to IRP
)
{
	PGCK_FILTER_EXT		pFilterExt;
	NTSTATUS			NtStatus;

	PAGED_CODE ();
	GCK_DBG_ENTRY_PRINT(("Entering GCK_FLTR_Ioctl, pDeviceObject = 0x%0.8x, pIRP = 0x%0.8x\n", pDeviceObject, pIrp));
	//cast device extension to proper type
	pFilterExt = (PGCK_FILTER_EXT) pDeviceObject->DeviceExtension;

    //
	// If we have been removed we need to refuse this IRP
	//
	if (GCK_STATE_REMOVED == pFilterExt->eDeviceState) {
		GCK_DBG_TRACE_PRINT(("GCK_FLTR_Ioctl called while delete pending\n"));
		ASSERT(FALSE);
		NtStatus = STATUS_DELETE_PENDING;
		pIrp->IoStatus.Information = 0;
        pIrp->IoStatus.Status = NtStatus;
		IoCompleteRequest (pIrp, IO_NO_INCREMENT);
    }
	else 
	{
	    // Send the IRP on unchanged.
	    IoSkipCurrentIrpStackLocation (pIrp);
        NtStatus = IoCallDriver (pFilterExt->pTopOfStack, pIrp);
    }

	GCK_DBG_EXIT_PRINT(("Exiting GCK_FLTR_Ioctl, Status: 0x%0.8x\n", NtStatus));
    return NtStatus;
}
