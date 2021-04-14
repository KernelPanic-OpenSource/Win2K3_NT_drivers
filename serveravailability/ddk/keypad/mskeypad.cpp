/*++

Copyright (c) 1991 - 2002 Microsoft Corporation

Module Name:

    ##    ##  ###  ##  ## ##### ##  ## #####    ###   #####       ####  #####  #####
    ###  ### ##  # ## ##  ##    ##  ## ##  ##   ###   ##  ##     ##   # ##  ## ##  ##
    ######## ###   ####   ##     ####  ##  ##  ## ##  ##   ##    ##     ##  ## ##  ##
    # ### ##  ###  ###    #####  ####  ##  ##  ## ##  ##   ##    ##     ##  ## ##  ##
    #  #  ##   ### ####   ##      ##   #####  ####### ##   ##    ##     #####  #####
    #     ## #  ## ## ##  ##      ##   ##     ##   ## ##  ##  ## ##   # ##     ##
    #     ##  ###  ##  ## #####   ##   ##     ##   ## #####   ##  ####  ##     ##

Abstract:

    This module contains the entire implementation of
    the virtual keypad miniport driver.

@@BEGIN_DDKSPLIT
Author:

    Wesley Witt (wesw) 1-Oct-2001

@@END_DDKSPLIT
Environment:

    Kernel mode only.

Notes:


--*/

#include "mskeypad.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif





VOID
MsKeypadCancelRoutine(
    IN PVOID DeviceExtensionIn,
    IN PIRP Irp,
    IN BOOLEAN CurrentIo
    )

/*++

Routine Description:

    This function is the miniport's IRP cancel routine.

Arguments:

    DeviceExtension - Pointer to the mini-port's device extension.
    CurrentIo       - TRUE if this is called for the current I/O

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceExtensionIn;
    KIRQL OldIrql;
    if (CurrentIo) {
        KeAcquireSpinLock( &DeviceExtension->DeviceLock, &OldIrql );
        DeviceExtension->Keypress = 0;
        DeviceExtension->DataBuffer = NULL;
        KeReleaseSpinLock( &DeviceExtension->DeviceLock, OldIrql );
    }
}


NTSTATUS
MsKeypadRead(
    IN PVOID DeviceExtensionIn,
    IN PIRP Irp,
    IN PVOID FsContext,
    IN LONGLONG StartingOffset,
    IN PVOID DataBuffer,
    IN ULONG DataBufferLength
    )

/*++

Routine Description:

   This routine processes the read requests for the local display miniport.

Arguments:

   DeviceExtensionIn    - Miniport's device extension
   StartingOffset       - Starting offset for the I/O
   DataBuffer           - Pointer to the data buffer
   DataBufferLength     - Length of the data buffer in bytes

Return Value:

   NT status code.

--*/

{
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceExtensionIn;
    KIRQL OldIrql;
    KeAcquireSpinLock( &DeviceExtension->DeviceLock, &OldIrql );
    if (DeviceExtension->Keypress) {
        *((PUCHAR)DataBuffer) = DeviceExtension->Keypress;
        KeReleaseSpinLock( &DeviceExtension->DeviceLock, OldIrql );
        return STATUS_SUCCESS;
    }
    DeviceExtension->DataBuffer = (PUCHAR) DataBuffer;
    KeReleaseSpinLock( &DeviceExtension->DeviceLock, OldIrql );
    return STATUS_PENDING;
}


NTSTATUS
MsKeypadDeviceIoctl(
    IN PVOID DeviceExtensionIn,
    IN PIRP Irp,
    IN PVOID FsContext,
    IN ULONG FunctionCode,
    IN PVOID InputBuffer,
    IN ULONG InputBufferLength,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This function is called by the SAPORT driver so that
    the mini-port driver can service an IOCTL call.

Arguments:

    DeviceExtension     - A pointer to the mini-port's device extension
    FunctionCode        - The IOCTL function code
    InputBuffer         - Pointer to the input buffer, contains the data sent down by the I/O
    InputBufferLength   - Length in bytes of the InputBuffer
    OutputBuffer        - Pointer to the output buffer, contains the data generated by this call
    OutputBufferLength  - Length in bytes of the OutputBuffer

Context:

    IRQL: IRQL PASSIVE_LEVEL, arbitrary thread context

Return Value:

    If the function succeeds, it must return STATUS_SUCCESS.
    Otherwise, it must return one of the error status values defined in ntstatus.h.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceExtensionIn;
    KIRQL OldIrql;


    switch (FunctionCode) {
        case FUNC_SA_GET_VERSION:
            *((PULONG)OutputBuffer) = SA_INTERFACE_VERSION;
            break;

        case FUNC_VDRIVER_INIT:
            if (InputBufferLength != sizeof(UCHAR) || InputBuffer == NULL) {
                return STATUS_BUFFER_TOO_SMALL;
            }
            KeAcquireSpinLock( &DeviceExtension->DeviceLock, &OldIrql );
            if (DeviceExtension->DataBuffer) {
                DeviceExtension->Keypress = *((PUCHAR)InputBuffer);
                DeviceExtension->DataBuffer[0] = DeviceExtension->Keypress;
                DeviceExtension->DataBuffer = NULL;
                KeReleaseSpinLock( &DeviceExtension->DeviceLock, OldIrql );
                SaPortCompleteRequest( DeviceExtension, NULL, sizeof(UCHAR), STATUS_SUCCESS, TRUE );
                KeAcquireSpinLock( &DeviceExtension->DeviceLock, &OldIrql );
                DeviceExtension->Keypress = 0;
            }
            KeReleaseSpinLock( &DeviceExtension->DeviceLock, OldIrql );
            break;

        default:
            Status = STATUS_NOT_SUPPORTED;
            REPORT_ERROR( SA_DEVICE_KEYPAD, "Unsupported device control", Status );
            break;
    }

    return Status;
}


NTSTATUS
MsKeypadHwInitialize(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID DeviceExtensionIn,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResources,
    IN ULONG PartialResourceCount
    )

/*++

Routine Description:

    This function is called by the SAPORT driver so that
    the mini-port driver can initialize it's hardware
    resources.

Arguments:

    DeviceObject            - Pointer to the target device object.
    Irp                     - Pointer to an IRP structure that describes the requested I/O operation.
    DeviceExtension         - A pointer to the mini-port's device extension.
    PartialResources        - Pointer to the translated resources alloacted by the system.
    PartialResourceCount    - The number of resources in the PartialResources array.

Context:

    IRQL: IRQL PASSIVE_LEVEL, system thread context

Return Value:

    If the function succeeds, it must return STATUS_SUCCESS.
    Otherwise, it must return one of the error status values defined in ntstatus.h.

--*/

{
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION) DeviceExtensionIn;
    KeInitializeSpinLock( &DeviceExtension->DeviceLock );
    return STATUS_SUCCESS;
}


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is the driver's entry point, called by the I/O system
    to load the driver.  The driver's entry points are initialized and
    a spin lock to control paging is initialized.

    In DBG mode, this routine also examines the registry for special
    debug parameters.

Arguments:

    DriverObject - a pointer to the object that represents this device driver.
    RegistryPath - a pointer to this driver's key in the Services tree.

Return Value:

    STATUS_SUCCESS

--*/

{
    NTSTATUS Status;
    SAPORT_INITIALIZATION_DATA SaPortInitData;


    RtlZeroMemory( &SaPortInitData, sizeof(SAPORT_INITIALIZATION_DATA) );

    SaPortInitData.StructSize = sizeof(SAPORT_INITIALIZATION_DATA);
    SaPortInitData.DeviceType = SA_DEVICE_KEYPAD;
    SaPortInitData.HwInitialize = MsKeypadHwInitialize;
    SaPortInitData.DeviceIoctl = MsKeypadDeviceIoctl;
    SaPortInitData.Read = MsKeypadRead;
    SaPortInitData.CancelRoutine = MsKeypadCancelRoutine;

    SaPortInitData.DeviceExtensionSize = sizeof(DEVICE_EXTENSION);

    Status = SaPortInitialize( DriverObject, RegistryPath, &SaPortInitData );
    if (!NT_SUCCESS(Status)) {
        REPORT_ERROR( SA_DEVICE_KEYPAD, "SaPortInitialize failed\n", Status );
        return Status;
    }

    return STATUS_SUCCESS;
}