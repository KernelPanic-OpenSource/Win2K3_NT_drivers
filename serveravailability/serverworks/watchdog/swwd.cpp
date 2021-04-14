/*++

Copyright (c) 1991 - 2001 Microsoft Corporation

Module Name:

     ###  ##  #  ## ##  #  ## #####       ####  #####  #####
    ##  # ## ### ## ## ### ## ##  ##     ##   # ##  ## ##  ##
    ###   ## ### ## ## ### ## ##   ##    ##     ##  ## ##  ##
     ###  ## # # ## ## # # ## ##   ##    ##     ##  ## ##  ##
      ###  ### ###   ### ###  ##   ##    ##     #####  #####
    #  ##  ### ###   ### ###  ##  ##  ## ##   # ##     ##
     ###   ##   ##   ##   ##  #####   ##  ####  ##     ##

Abstract:

    This module contains the entire implementation of
    the watchdog miniport for the ServerWorks
    CSB5 server chip set.

Author:

    Wesley Witt (wesw) 1-Oct-2001

Environment:

    Kernel mode only.

Notes:


--*/

#include "swwd.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif



NTSTATUS
SaWdDeviceIoctl(
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

    DeviceExtensionIn   - A pointer to the mini-port's device extension
    Irp                 - IO request packet pointer
    FsContext           - Context pointer
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
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceExtensionIn;
    NTSTATUS Status = STATUS_SUCCESS;
    PSA_WD_CAPS WdCaps = NULL;
    UCHAR Control;
    ULONG TimerValue;


    switch (FunctionCode) {
        case FUNC_SA_GET_VERSION:
            *((PULONG)OutputBuffer) = SA_INTERFACE_VERSION;
            break;

        case FUNC_SA_GET_CAPABILITIES:
            WdCaps = (PSA_WD_CAPS)OutputBuffer;
            WdCaps->SizeOfStruct = sizeof(SA_WD_CAPS);
            WdCaps->Minimum = 1;
            WdCaps->Maximum = 512;
            break;

        case FUNC_SAWD_DISABLE:
            ExAcquireFastMutex( &DeviceExtension->WdIoLock );
            Control = READ_REGISTER_UCHAR( DeviceExtension->WdMemBase );
            if (*((PULONG)InputBuffer) == 1) {
                SETBITS( Control, WATCHDOG_CONTROL_ENABLE );
                SETBITS( Control, WATCHDOG_CONTROL_TRIGGER );
            } else {
                CLEARBITS( Control, WATCHDOG_CONTROL_ENABLE );
            }
            WRITE_REGISTER_UCHAR( DeviceExtension->WdMemBase, Control );
            ExReleaseFastMutex( &DeviceExtension->WdIoLock );
            break;

        case FUNC_SAWD_QUERY_EXPIRE_BEHAVIOR:
            ExAcquireFastMutex( &DeviceExtension->WdIoLock );
            Control = READ_REGISTER_UCHAR( DeviceExtension->WdMemBase );
            if (Control & WATCHDOG_CONTROL_TIMER_MODE) {
                *((PULONG)OutputBuffer) = 1;
            } else {
                *((PULONG)OutputBuffer) = 0;
            }
            ExReleaseFastMutex( &DeviceExtension->WdIoLock );
            break;

        case FUNC_SAWD_SET_EXPIRE_BEHAVIOR:
            ExAcquireFastMutex( &DeviceExtension->WdIoLock );
            Control = READ_REGISTER_UCHAR( DeviceExtension->WdMemBase );
            if (*((PULONG)InputBuffer) == 1) {
                SETBITS( Control, WATCHDOG_CONTROL_TIMER_MODE );
            } else {
                CLEARBITS( Control, WATCHDOG_CONTROL_TIMER_MODE );
            }
            WRITE_REGISTER_UCHAR( DeviceExtension->WdMemBase, Control );
            ExReleaseFastMutex( &DeviceExtension->WdIoLock );
            break;

        case FUNC_SAWD_PING:
            ExAcquireFastMutex( &DeviceExtension->WdIoLock );
            Control = READ_REGISTER_UCHAR( DeviceExtension->WdMemBase );
            SETBITS( Control, WATCHDOG_CONTROL_TRIGGER );
            WRITE_REGISTER_UCHAR( DeviceExtension->WdMemBase, Control );
            ExReleaseFastMutex( &DeviceExtension->WdIoLock );
            break;

        case FUNC_SAWD_QUERY_TIMER:
            ExAcquireFastMutex( &DeviceExtension->WdIoLock );
            TimerValue = READ_REGISTER_ULONG( (PULONG)(DeviceExtension->WdMemBase+4) );
            *((PULONG)OutputBuffer) = TimerValue & 0x1ff;
            ExReleaseFastMutex( &DeviceExtension->WdIoLock );
            break;

        case FUNC_SAWD_SET_TIMER:
            ExAcquireFastMutex( &DeviceExtension->WdIoLock );
            TimerValue = *((PULONG)InputBuffer) & 0x1ff;
            WRITE_REGISTER_ULONG( (PULONG)(DeviceExtension->WdMemBase+4), TimerValue );
            ExReleaseFastMutex( &DeviceExtension->WdIoLock );
            break;

        default:
            Status = STATUS_NOT_SUPPORTED;
            REPORT_ERROR( SA_DEVICE_WATCHDOG, "Unsupported device control", Status );
            break;
    }

    return Status;
}


NTSTATUS
SaWdHwInitialize(
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
    PDEVICE_EXTENSION DeviceExtension = (PDEVICE_EXTENSION)DeviceExtensionIn;
    NTSTATUS Status;
    ULONG i;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR ResourceMemory = NULL;


    for (i=0; i<PartialResourceCount; i++) {
        if (PartialResources[i].Type == CmResourceTypeMemory) {
            ResourceMemory = &PartialResources[i];
        }
    }

    if (ResourceMemory == NULL) {
        REPORT_ERROR( SA_DEVICE_WATCHDOG, "Missing memory resource", STATUS_UNSUCCESSFUL );
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Setup the memory base address
    //

    DeviceExtension->WdMemBase = (PUCHAR) SaPortGetVirtualAddress(
        DeviceExtension,
        ResourceMemory->u.Memory.Start,
        ResourceMemory->u.Memory.Length
        );
    if (DeviceExtension->WdMemBase == NULL) {
        REPORT_ERROR( SA_DEVICE_WATCHDOG, "SaPortGetVirtualAddress failed", STATUS_NO_MEMORY );
        return STATUS_NO_MEMORY;
    }

    ExInitializeFastMutex( &DeviceExtension->WdIoLock );

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
    a mutex to control paging is initialized.

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
    SaPortInitData.DeviceType = SA_DEVICE_WATCHDOG;
    SaPortInitData.HwInitialize = SaWdHwInitialize;
    SaPortInitData.DeviceIoctl = SaWdDeviceIoctl;

    SaPortInitData.DeviceExtensionSize = sizeof(DEVICE_EXTENSION);

    Status = SaPortInitialize( DriverObject, RegistryPath, &SaPortInitData );
    if (!NT_SUCCESS(Status)) {
        REPORT_ERROR( SA_DEVICE_WATCHDOG, "SaPortInitialize failed\n", Status );
        return Status;
    }

    return STATUS_SUCCESS;
}