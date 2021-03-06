/*
 *************************************************************************
 *  File:       USBCCGP.C
 *
 *  Module:     USBCCGP.SYS
 *              USB Common Class Generic Parent driver.
 *
 *  Copyright (c) 1998  Microsoft Corporation
 *
 *
 *  Author:     ervinp
 *
 *************************************************************************
 */

#include <wdm.h>
#include <usbdi.h>
#include <usbdlib.h>
#include <usbioctl.h>

#include "usbccgp.h"
#include "debug.h"


#ifdef ALLOC_PRAGMA
        #pragma alloc_text(PAGE, DriverEntry)
        #pragma alloc_text(PAGE, USBC_AddDevice)
        #pragma alloc_text(PAGE, USBC_DriverUnload)
        #pragma alloc_text(PAGE, RegQueryGenericCompositeUSBDeviceString)
#endif


PWCHAR GenericCompositeUSBDeviceString = NULL;


NTSTATUS GetConfigValue(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
/*++

Routine Description:

	This routine is a callback routine for RtlQueryRegistryValues
    It is called for each entry in the Parameters
    node to set the config values. The table is set up
    so that this function will be called with correct default
    values for keys that are not present.

Arguments:

    ValueName - The name of the value (ignored).
	ValueType - The type of the value
	ValueData - The data for the value.
	ValueLength - The length of ValueData.
	Context - A pointer to the CONFIG structure.
	EntryContext - The index in Config->Parameters to save the value.

Return Value:

--*/
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PWCHAR tmpStr;

    switch (ValueType) {
//    case REG_DWORD:
//        *(PVOID*)EntryContext = *(PVOID*)ValueData;
//        break;
//    case REG_BINARY:
//        RtlCopyMemory(EntryContext, ValueData, ValueLength);
//        break;
    case REG_SZ:
        if (ValueLength) {
            tmpStr = ExAllocatePool(PagedPool, ValueLength);
            if (tmpStr) {
                RtlZeroMemory(tmpStr, ValueLength);
                RtlCopyMemory(tmpStr, ValueData, ValueLength);
                *(PWCHAR *)EntryContext = tmpStr;
            } else {
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            }
        } else {
            ntStatus = STATUS_INVALID_PARAMETER;
        }
        break;
    default:
//        TEST_TRAP();
        ntStatus = STATUS_INVALID_PARAMETER;
    }
    return ntStatus;
}



NTSTATUS RegQueryGenericCompositeUSBDeviceString(IN OUT PWCHAR *GenericCompositeUSBDeviceString)
{
    NTSTATUS ntStatus;
    RTL_QUERY_REGISTRY_TABLE QueryTable[2];
    PWCHAR usbstr = L"usbflags";
    PWCHAR valuename = L"GenericCompositeUSBDeviceString";

    PAGED_CODE();

    //
    // Set up QueryTable to do the following:
    //

    // Upgrade install flag
    QueryTable[0].QueryRoutine = GetConfigValue;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
    QueryTable[0].Name = valuename;
    QueryTable[0].EntryContext = GenericCompositeUSBDeviceString;
    QueryTable[0].DefaultType = 0;
    QueryTable[0].DefaultData = NULL;
    QueryTable[0].DefaultLength = 0;

    //
    // Stop
    //
    QueryTable[1].QueryRoutine = NULL;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = NULL;

    ntStatus = RtlQueryRegistryValues(
                RTL_REGISTRY_CONTROL,
                usbstr,
                QueryTable,					// QueryTable
                NULL,						// Context
                NULL);						// Environment

    return ntStatus;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObj, IN PUNICODE_STRING uniRegistryPath)
{

    PAGED_CODE();

    driverObj->MajorFunction[IRP_MJ_CREATE] =
        driverObj->MajorFunction[IRP_MJ_CLOSE] =
        driverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
        driverObj->MajorFunction[IRP_MJ_SYSTEM_CONTROL] =
        driverObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
        driverObj->MajorFunction[IRP_MJ_PNP] =
        driverObj->MajorFunction[IRP_MJ_POWER] = USBC_Dispatch;

    driverObj->DriverUnload = USBC_DriverUnload;
    driverObj->DriverExtension->AddDevice = (PDRIVER_ADD_DEVICE)USBC_AddDevice;

    RegQueryGenericCompositeUSBDeviceString(&GenericCompositeUSBDeviceString);

    return STATUS_SUCCESS;
}


/*
 *  USBC_AddDevice
 *
 */
NTSTATUS USBC_AddDevice(IN PDRIVER_OBJECT driverObj, IN PDEVICE_OBJECT pdo)
{
    NTSTATUS status;
    PDEVICE_OBJECT fdo = NULL;

    PAGED_CODE();

    status = IoCreateDevice(    driverObj,
                                sizeof(DEVEXT),
                                NULL,       // name for this device
                                FILE_DEVICE_UNKNOWN,
                                FILE_AUTOGENERATED_DEVICE_NAME,                // device characteristics
                                FALSE,      // not exclusive
                                &fdo);      // our device object

    if (NT_SUCCESS(status)){
        PDEVEXT devExt;
        PPARENT_FDO_EXT parentFdoExt;

        ASSERT(fdo);

        devExt = (PDEVEXT)fdo->DeviceExtension;
        RtlZeroMemory(devExt, sizeof(DEVEXT));

        devExt->signature = USBCCGP_TAG;
        devExt->isParentFdo = TRUE;

        parentFdoExt = &devExt->parentFdoExt;

        parentFdoExt->driverObj = driverObj;
        parentFdoExt->pdo = pdo;
        parentFdoExt->fdo = fdo;
        parentFdoExt->state = STATE_INITIALIZED;
        parentFdoExt->topDevObj = IoAttachDeviceToDeviceStack(fdo, pdo);

        parentFdoExt->pendingActionCount = 0;
        KeInitializeEvent(&parentFdoExt->removeEvent, NotificationEvent, FALSE);

        KeInitializeSpinLock(&parentFdoExt->parentFdoExtSpinLock);

        InitializeListHead(&parentFdoExt->functionWaitWakeIrpQueue);
        InitializeListHead(&parentFdoExt->pendingResetPortIrpQueue);
        InitializeListHead(&parentFdoExt->pendingCyclePortIrpQueue);

        fdo->Flags |= (parentFdoExt->topDevObj->Flags & DO_POWER_PAGABLE);
        fdo->Flags &= ~DO_DEVICE_INITIALIZING;

        DBGVERBOSE(("Created parent FDO %p", pdo));
    } 

    ASSERT(NT_SUCCESS(status));
    return status;
}


VOID USBC_DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    PAGED_CODE();

    if (GenericCompositeUSBDeviceString) {
        ExFreePool(GenericCompositeUSBDeviceString);
        GenericCompositeUSBDeviceString = NULL;
    }
}


