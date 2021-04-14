/*--------------------------------------------------------------------------
*	
*   Copyright (C) Cyclades Corporation, 1996-2001.
*   All rights reserved.
*	
*   Cyclom-Y Port Driver
*	
*   This file:      cyyqset.c
*	
*   Description:    This module contains the code related to query/set
*                   file operations in the Cyclom-Y Port driver.
*
*   Notes:          This code supports Windows 2000 and Windows XP,
*                   x86 and IA64 processors.
*	
*   Complies with Cyclades SW Coding Standard rev 1.3.
*	
*--------------------------------------------------------------------------
*/

/*-------------------------------------------------------------------------
*
*   Change History
*
*--------------------------------------------------------------------------
*
*
*--------------------------------------------------------------------------
*/


#include "precomp.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESRP0,CyyQueryInformationFile)
#pragma alloc_text(PAGESRP0,CyySetInformationFile)
#endif


NTSTATUS
CyyQueryInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*--------------------------------------------------------------------------
    CyyQueryInformationFile()
    
    Routine Description: This routine is used to query the end of file
    information on the opened serial port. Any other file information
    request is retured with an invalid parameter.
    This routine always returns an end of file of 0.

    Arguments:

    DeviceObject - Pointer to the device object for this device
    Irp - Pointer to the IRP for the current request

    Return Value: The function value is the final status of the call
--------------------------------------------------------------------------*/
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;	// current stack location
    
    NTSTATUS status;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    if ((status = CyyIRPPrologue(Irp,
                                 (PCYY_DEVICE_EXTENSION)DeviceObject->
                                 DeviceExtension)) != STATUS_SUCCESS) {
      if (status != STATUS_PENDING) {
         CyyCompleteRequest((PCYY_DEVICE_EXTENSION)DeviceObject->
                            DeviceExtension, Irp, IO_NO_INCREMENT);
      }
      return status;
    }

    CyyDbgPrintEx(CYYIRPPATH, "Dispatch entry for: %x\n", Irp);

    if (CyyCompleteIfError(DeviceObject, Irp) != STATUS_SUCCESS) {

        return STATUS_CANCELLED;
    }
    
    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    Irp->IoStatus.Information = 0L;
    Status = STATUS_SUCCESS;
    if (IrpSp->Parameters.QueryFile.FileInformationClass ==
        FileStandardInformation) {

        PFILE_STANDARD_INFORMATION Buf = Irp->AssociatedIrp.SystemBuffer;

        Buf->AllocationSize.QuadPart = 0;
        Buf->EndOfFile = Buf->AllocationSize;
        Buf->NumberOfLinks = 0;
        Buf->DeletePending = FALSE;
        Buf->Directory = FALSE;
        Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);

    } else if (IrpSp->Parameters.QueryFile.FileInformationClass ==
               FilePositionInformation) {

        ((PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->
            CurrentByteOffset.QuadPart = 0;
        Irp->IoStatus.Information = sizeof(FILE_POSITION_INFORMATION);

    } else {
        Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
    }

    CyyCompleteRequest((PCYY_DEVICE_EXTENSION)DeviceObject->
                          DeviceExtension, Irp, 0);
    return Status;
}

NTSTATUS
CyySetInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*--------------------------------------------------------------------------
    CyySetInformationFile()
    
    Routine Description: This routine is used to set the end of file
    information on the opened serial port. Any other file information
    request is retured with an invalid parameter.

    This routine always ignores the actual end of file since
    the query information code always returns an end of file of 0.

    Arguments:

    DeviceObject - Pointer to the device object for this device
    Irp - Pointer to the IRP for the current request

    Return Value: The function value is the final status of the call
--------------------------------------------------------------------------*/
{
    NTSTATUS Status;
    
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    if ((Status = CyyIRPPrologue(Irp,
                                 (PCYY_DEVICE_EXTENSION)DeviceObject->
                                 DeviceExtension)) != STATUS_SUCCESS) {
      if(Status != STATUS_PENDING) {
         CyyCompleteRequest((PCYY_DEVICE_EXTENSION)DeviceObject->
                            DeviceExtension, Irp, IO_NO_INCREMENT);
      }
      return Status;
   }

    CyyDbgPrintEx(CYYIRPPATH, "Dispatch entry for: %x\n", Irp);

    if (CyyCompleteIfError(DeviceObject, Irp) != STATUS_SUCCESS) {

        return STATUS_CANCELLED;
    }
    
    Irp->IoStatus.Information = 0L;
    if ((IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass ==
         FileEndOfFileInformation) ||
        (IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass ==
         FileAllocationInformation)) {

        Status = STATUS_SUCCESS;
    } else {
        Status = STATUS_INVALID_PARAMETER;
    }

    Irp->IoStatus.Status = Status;

    CyyCompleteRequest((PCYY_DEVICE_EXTENSION)DeviceObject->
                        DeviceExtension, Irp, 0);

    return Status;
}

