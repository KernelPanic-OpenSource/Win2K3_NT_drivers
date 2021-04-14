/*--------------------------------------------------------------------------
*
*   Copyright (C) Cyclades Corporation, 2000-2001.
*   All rights reserved.
*
*   Cyclades-Z Enumerator Driver
*	
*   This file:      pnp.c
*
*   Description:    This module contains contains the plugplay calls
*                   PNP / WDM BUS driver.
*					
*   Notes:          This code supports Windows 2000 and Windows XP,
*                   x86 and ia64 processors.
*
*   Complies with Cyclades SW Coding Standard rev 1.3.
*
*--------------------------------------------------------------------------
*/

/*-------------------------------------------------------------------------
*
*	Change History
*
*--------------------------------------------------------------------------
*   Initial implementation based on Microsoft sample code.
*
*--------------------------------------------------------------------------
*/

#include "pch.h"

static const PHYSICAL_ADDRESS CyzPhysicalZero = {0};

// FANNY_ADDPAGABLE_LATER
//#ifdef ALLOC_PRAGMA
//#pragma alloc_text (PAGE, Cycladz_AddDevice)
//#pragma alloc_text (PAGE, Cycladz_PnP)
//#pragma alloc_text (PAGE, Cycladz_FDO_PnP)
//#pragma alloc_text (PAGE, Cycladz_PDO_PnP)
//#pragma alloc_text (PAGE, Cycladz_PnPRemove)
//#pragma alloc_text (PAGE, Cycladz_StartDevice)
////#pragma alloc_text (PAGE, Cycladz_Remove)
//#endif


NTSTATUS
Cycladz_AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT BusPhysicalDeviceObject
    )
/*++
Routine Description.
    A bus has been found.  Attach our FDO to it.
    Allocate any required resources.  Set things up.  And be prepared for the
    first ``start device.''

Arguments:
    DriverObject - This very self referenced driver.

    BusPhysicalDeviceObject - Device object representing the bus.  That to which
        we attach a new FDO.

--*/
{
    NTSTATUS            status;
    PDEVICE_OBJECT      deviceObject;
    PFDO_DEVICE_DATA    DeviceData;
    ULONG               nameLength;
    ULONG               i;
    INTERFACE_TYPE      interfaceType;
    ULONG               interfaceTypeLength;
    ULONG               uiNumber,uiNumberLength;
    
    PAGED_CODE ();

    Cycladz_KdPrint_Def (SER_DBG_PNP_TRACE, ("Add Device: 0x%x\n",
                                              BusPhysicalDeviceObject));
    //
    // Create our FDO
    //

    status = IoCreateDevice(DriverObject, sizeof(FDO_DEVICE_DATA), NULL,
                           FILE_DEVICE_BUS_EXTENDER, 0, TRUE, &deviceObject);

    if (NT_SUCCESS (status)) {
        DeviceData = (PFDO_DEVICE_DATA) deviceObject->DeviceExtension;
        RtlFillMemory (DeviceData, sizeof (FDO_DEVICE_DATA), 0);

        DeviceData->IsFDO = TRUE;
        DeviceData->DebugLevel = SER_DEFAULT_DEBUG_OUTPUT_LEVEL;
        DeviceData->Self = deviceObject;
        DeviceData->DriverObject = DriverObject;
        for (i=0; i<CYZ_MAX_PORTS; i++) {
           DeviceData->AttachedPDO[i] = NULL;
        }
        DeviceData->NumPDOs = 0;

        DeviceData->DeviceState = PowerDeviceD0;
        DeviceData->SystemState = PowerSystemWorking; // FANNY: This seems to be not needed

        DeviceData->SystemWake=PowerSystemUnspecified;
        DeviceData->DeviceWake=PowerDeviceUnspecified;

        INITIALIZE_PNP_STATE(DeviceData);

        // Set the PDO for use with PlugPlay functions
        DeviceData->UnderlyingPDO = BusPhysicalDeviceObject;


        //
        // Attach our filter driver to the device stack.
        // the return value of IoAttachDeviceToDeviceStack is the top of the
        // attachment chain.  This is where all the IRPs should be routed.
        //
        // Our filter will send IRPs to the top of the stack and use the PDO
        // for all PlugPlay functions.
        //
        DeviceData->TopOfStack
            = IoAttachDeviceToDeviceStack(deviceObject, BusPhysicalDeviceObject);

        deviceObject->Flags |= DO_BUFFERED_IO;

        // Bias outstanding request to 1 so that we can look for a
        // transition to zero when processing the remove device PlugPlay IRP.
        DeviceData->OutstandingIO = 1;

        KeInitializeEvent(&DeviceData->RemoveEvent, SynchronizationEvent,
                        FALSE);

        //
        // Tell the PlugPlay system that this device will need an interface
        // device class shingle.
        //
        // It may be that the driver cannot hang the shingle until it starts
        // the device itself, so that it can query some of its properties.
        // (Aka the shingles guid (or ref string) is based on the properties
        // of the device.)
        //
        status = IoRegisterDeviceInterface (BusPhysicalDeviceObject,
                                            (LPGUID) &GUID_CYCLADESZ_BUS_ENUMERATOR,
                                            NULL,
                                            &DeviceData->DevClassAssocName);

        if (!NT_SUCCESS (status)) {
            CyzLogError(DriverObject, NULL, CyzPhysicalZero, CyzPhysicalZero,
                        0, 0, 0, 0, status, CYZ_REGISTER_INTERFACE_FAILURE,
                        0, NULL, 0, NULL);
            Cycladz_KdPrint_Def (SER_DBG_PNP_ERROR,
                                ("AddDevice: IoRegisterDCA failed (%x)", status));
            goto CycladzAddDevice_Error;
        }

        //
        // If for any reason you need to save values in a safe location that
        // clients of this DeviceClassAssociate might be interested in reading
        // here is the time to do so, with the function
        // IoOpenDeviceClassRegistryKey
        // the symbolic link name used is was returned in
        // DeviceData->DevClassAssocName (the same name which is returned by
        // IoGetDeviceClassAssociations and the SetupAPI equivs.
        //

#if DBG
{
        PWCHAR deviceName = NULL;

        status = IoGetDeviceProperty (BusPhysicalDeviceObject,
                                      DevicePropertyPhysicalDeviceObjectName,0,
                                      NULL,&nameLength);

        if ((nameLength != 0) && (status == STATUS_BUFFER_TOO_SMALL)) {
            deviceName = ExAllocatePool (NonPagedPool, nameLength);

            if (NULL == deviceName) {
               goto someDebugStuffExit;
            }

            IoGetDeviceProperty (BusPhysicalDeviceObject,
                                 DevicePropertyPhysicalDeviceObjectName,
                                 nameLength, deviceName, &nameLength);

            Cycladz_KdPrint_Def (SER_DBG_PNP_TRACE,
                               ("AddDevice: %x to %x->%x (%ws) \n",
                                 deviceObject, DeviceData->TopOfStack,
                                 BusPhysicalDeviceObject, deviceName));
        }

someDebugStuffExit:;
        if (deviceName != NULL) {
            ExFreePool(deviceName);
        }
}
#endif

        DeviceData->IsPci = 1; // Z is always PCI

        status = IoGetDeviceProperty (BusPhysicalDeviceObject,
                                      DevicePropertyUINumber ,
                                      sizeof(uiNumber),
                                      &uiNumber,
                                      &uiNumberLength);

        if (!NT_SUCCESS (status)) {
            uiNumber = 0xFFFFFFFF;

            Cycladz_KdPrint_Def (SER_DBG_PNP_ERROR,
                                ("AddDevice: IoGetDeviceProperty DevicePropertyUINumber failed (%x)", 
                                  status));
        }

        DeviceData->UINumber = uiNumber;

        //
        // Turn on the shingle and point it to the given device object.
        //
        status = IoSetDeviceInterfaceState (
                        &DeviceData->DevClassAssocName,
                        TRUE);

        if (!NT_SUCCESS (status)) {
            Cycladz_KdPrint_Def (SER_DBG_PNP_ERROR,
                                ("AddDevice: IoSetDeviceClass failed (%x)", status));
            //return status;
            goto CycladzAddDevice_Error;
        }

        deviceObject->Flags |= DO_POWER_PAGABLE;
        deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
    } else {
      CyzLogError(DriverObject, NULL, CyzPhysicalZero, CyzPhysicalZero,
                  0, 0, 0, 0, status, CYZ_DEVICE_CREATION_FAILURE,
                  0, NULL, 0, NULL);
    }

    return status;


CycladzAddDevice_Error:

    if (DeviceData->DevClassAssocName.Buffer) {
       RtlFreeUnicodeString(&DeviceData->DevClassAssocName);
    }

    if (DeviceData->TopOfStack) {
       IoDetachDevice(DeviceData->TopOfStack);
    }
    if (deviceObject) {
       IoDeleteDevice(deviceObject);
    }
 
    return status;
}

NTSTATUS
Cycladz_PnP (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
/*++
Routine Description:
    Answer the plethora of Irp Major PnP IRPS.
--*/
{
    PIO_STACK_LOCATION      irpStack;
    NTSTATUS                status;
    PCOMMON_DEVICE_DATA     commonData;
    KIRQL                   oldIrq;
#if DBG
    UCHAR                   MinorFunction;
#endif

    PAGED_CODE ();

    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (irpStack->MajorFunction == IRP_MJ_PNP);
#if DBG
    MinorFunction = irpStack->MinorFunction;
#endif

    commonData = (PCOMMON_DEVICE_DATA) DeviceObject->DeviceExtension;

    //
    // If removed, fail the request and get out
    //

    if (commonData->DevicePnPState == Deleted) {   // if (commonData->Removed) added in build 2072.

        Cycladz_KdPrint(commonData, SER_DBG_PNP_TRACE,
                        ("PNP: removed DO: %x got IRP: %x\n", DeviceObject, 
                         Irp));

        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        goto PnPDone;
    }

    //
    // Call either the FDO or PDO Pnp code
    //

    if (commonData->IsFDO) {
        Cycladz_KdPrint(commonData, SER_DBG_PNP_TRACE,
                         ("FDO(%x):%s IRP:%x\n", DeviceObject, 
                          PnPMinorFunctionString(irpStack->MinorFunction),Irp));

        status = Cycladz_FDO_PnP(DeviceObject, Irp, irpStack,
                    (PFDO_DEVICE_DATA) commonData);
        goto PnPDone;

    } 
    
    //
    // PDO
    //
    
    Cycladz_KdPrint(commonData, SER_DBG_PNP_TRACE,
                    ("PDO(%x):%s IRP:%x\n", DeviceObject, 
                     PnPMinorFunctionString(irpStack->MinorFunction),Irp));

    status = Cycladz_PDO_PnP(DeviceObject, Irp, irpStack,
                             (PPDO_DEVICE_DATA) commonData);

PnPDone:;
    return status;
}

NTSTATUS
Cycladz_FDO_PnP (
    IN PDEVICE_OBJECT       DeviceObject,
    IN PIRP                 Irp,
    IN PIO_STACK_LOCATION   IrpStack,
    IN PFDO_DEVICE_DATA     DeviceData
    )
/*++
Routine Description:
    Handle requests from the PlugPlay system for the BUS itself

    NB: the various Minor functions of the PlugPlay system will not be
    overlapped and do not have to be reentrant

--*/
{
    NTSTATUS    status;
    KIRQL       oldIrq;
    KEVENT      event;
    ULONG       length;
    ULONG       i;
    PLIST_ENTRY entry;
    PPDO_DEVICE_DATA    pdoData;
    PDEVICE_RELATIONS   relations;
    PIO_STACK_LOCATION  stack;
    PRTL_QUERY_REGISTRY_TABLE QueryTable = NULL;
    ULONG DebugLevelDefault = SER_DEFAULT_DEBUG_OUTPUT_LEVEL;
    HANDLE      instanceKey;
    UNICODE_STRING  keyName;
    ULONG       numOfPorts;

    PAGED_CODE ();

    status = Cycladz_IncIoCount (DeviceData);
    if (!NT_SUCCESS (status)) {
        //Irp->IoStatus.Information = 0; Removed in build 2072
        Irp->IoStatus.Status = status;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

    stack = IoGetCurrentIrpStackLocation (Irp);

    switch (IrpStack->MinorFunction) {

       case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: {

         PIO_RESOURCE_REQUIREMENTS_LIST pReqList;
         PIO_RESOURCE_LIST pResList;
         PIO_RESOURCE_DESCRIPTOR pResDesc;
         ULONG i, j;
         ULONG reqCnt;
         ULONG gotPLX;
         ULONG gotMemory;
         ULONG gotInt;
         ULONG listNum;

         // FANNY: The serial driver had it as SynchronizationEvent.
         KeInitializeEvent(&event, NotificationEvent, FALSE);

         IoCopyCurrentIrpStackLocationToNext(Irp);
         IoSetCompletionRoutine(Irp, CycladzSyncCompletion, &event,
                                TRUE, TRUE, TRUE);

         status = IoCallDriver(DeviceData->TopOfStack, Irp);


         //
         // Wait for lower drivers to be done with the Irp
         //

         if (status == STATUS_PENDING) {
            KeWaitForSingleObject (&event, Executive, KernelMode, FALSE,
                                   NULL);
         }

         if (Irp->IoStatus.Information == 0) {
            if (stack->Parameters.FilterResourceRequirements
                .IoResourceRequirementList == 0) {
               Cycladz_KdPrint(DeviceData, SER_DBG_CYCLADES, ("Can't filter NULL resources!"
                                                       "\n"));
               status = Irp->IoStatus.Status;
               IoCompleteRequest (Irp, IO_NO_INCREMENT);
               Cycladz_DecIoCount (DeviceData);
               return status;
            }

            Irp->IoStatus.Information = (ULONG_PTR)stack->Parameters
                                        .FilterResourceRequirements
                                        .IoResourceRequirementList;

         }


         //
         // Force ISR ports in IO_RES_REQ_LIST to shared status
         // Force interrupts to shared status
         //

         //
         // We will only process the first list -- multiport boards
         // should not have alternative resources
         //

         pReqList = (PIO_RESOURCE_REQUIREMENTS_LIST)Irp->IoStatus.Information;
         pResList = &pReqList->List[0];

         Cycladz_KdPrint(DeviceData, SER_DBG_CYCLADES, ("------- List has %x lists "
                                                      "(including alternatives)\n",
                                                        pReqList->AlternativeLists));

         for (listNum = 0; listNum < (pReqList->AlternativeLists);
              listNum++) {
            gotPLX = 0;
            gotMemory = 0;
            gotInt = 0;

            Cycladz_KdPrint(DeviceData, SER_DBG_CYCLADES, ("------- List has %x resources in it\n",
                                                           pResList->Count));

            for (j = 0; (j < pResList->Count); j++) {
               pResDesc = &pResList->Descriptors[j];

               switch (pResDesc->Type) {
               case CmResourceTypeMemory:
                  if (pResDesc->u.Memory.Length == CYZ_RUNTIME_LENGTH) {
                      gotPLX = 1;
                      //wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
                      //pResDesc->ShareDisposition = CmResourceShareShared; 
                      //wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
                      //pResDesc->ShareDisposition = CmResourceShareDriverExclusive; 
                      //Cycladz_KdPrint(DeviceData,SER_DBG_CYCLADES,
                      //                              ("------- Sharing PLX Memory for "
                      //                               "device %x\n", DeviceData->TopOfStack));

                  } else {
                      gotMemory = 1;
                      //wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
                      //pResDesc->ShareDisposition = CmResourceShareShared; 
                      //wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww
                      //pResDesc->ShareDisposition = CmResourceShareDriverExclusive; 
                      //Cycladz_KdPrint(DeviceData,SER_DBG_CYCLADES,
                      //                              ("------- Sharing Board Memory for "
                      //                               "device %x\n", DeviceData->TopOfStack));
                  }
                  break;

               case CmResourceTypePort:
                  Cycladz_KdPrint(DeviceData,SER_DBG_CYCLADES,
                                         ("------- We should not have Port resource\n"));
                  break;

               case CmResourceTypeInterrupt:
                  gotInt = 1;
                  if (DeviceData->IsPci) {
                      pResDesc->ShareDisposition = CmResourceShareShared;
                  }
                  Cycladz_KdPrint(DeviceData,SER_DBG_CYCLADES,("------- Sharing interrupt for "
                                                 "device %x\n", DeviceData->TopOfStack));
                  break;

               default:
                  break;
               }

               //
               // If we found what we need, we can break out of the loop
               //

               // FANNY: STRANGE, THERE ARE TWICE FOR EACH TYPE. IT SEEMS THAT 
               // BOTH RAW AND TRANSLATED ARE LISTED.
               // (gotPLX && gotMemory && gotInt) {
               // break;
               //
            }

            pResList = (PIO_RESOURCE_LIST)((PUCHAR)pResList
                                           + sizeof(IO_RESOURCE_LIST)
                                           + sizeof(IO_RESOURCE_DESCRIPTOR)
                                           * (pResList->Count - 1));
         }



         Irp->IoStatus.Status = STATUS_SUCCESS;
         IoCompleteRequest (Irp, IO_NO_INCREMENT);
         Cycladz_DecIoCount (DeviceData);
         return STATUS_SUCCESS;
    }

    case IRP_MN_START_DEVICE:
        //
        // BEFORE you are allowed to ``touch'' the device object to which
        // the FDO is attached (that send an irp from the bus to the Device
        // object to which the bus is attached).   You must first pass down
        // the start IRP.  It might not be powered on, or able to access or
        // something.
        //


        // FANNY_TODO
        // SHOULD I CALL MmLockPagableCodeSection as the serial driver?


//        if (DeviceData->Started) {
//            Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
//                ("Device already started\n"));
//            status = STATUS_SUCCESS;
//            break;
//        }


        KeInitializeEvent (&event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext (Irp);

        IoSetCompletionRoutine (Irp,
                                CycladzSyncCompletion,
                                &event,
                                TRUE,
                                TRUE,
                                TRUE);

        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            // wait for it...

            status = KeWaitForSingleObject (&event,
                                            Executive,
                                            KernelMode,
                                            FALSE, // Not allertable
                                            NULL); // No timeout structure

            ASSERT (STATUS_SUCCESS == status);

            status = Irp->IoStatus.Status;
        }

        if (NT_SUCCESS(status)) {

            //
            // Get the debug level from the registry
            //

            if (NULL == (QueryTable = ExAllocatePool(                                        
                                        PagedPool,
                                        sizeof(RTL_QUERY_REGISTRY_TABLE)*2
                                        ))) {
                Cycladz_KdPrint (DeviceData, SER_DBG_PNP_ERROR,
                                ("Failed to allocate memory to query registy\n"));
                DeviceData->DebugLevel = DebugLevelDefault;
            } else {
                RtlZeroMemory(
                           QueryTable,
                           sizeof(RTL_QUERY_REGISTRY_TABLE)*2
                           );

                QueryTable[0].QueryRoutine = NULL;
                QueryTable[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
                QueryTable[0].EntryContext = &DeviceData->DebugLevel;
                QueryTable[0].Name      = L"DebugLevel";
                QueryTable[0].DefaultType   = REG_DWORD;
                QueryTable[0].DefaultData   = &DebugLevelDefault;
                QueryTable[0].DefaultLength= sizeof(ULONG);

                // CIMEXCIMEX: The rest of the table isn't filled in!  Comment changed bld 2128

                if (!NT_SUCCESS(RtlQueryRegistryValues(
                    RTL_REGISTRY_SERVICES,
                    L"cyclad-z",
                    QueryTable,
                    NULL,
                    NULL))) {
                    Cycladz_KdPrint (DeviceData,SER_DBG_PNP_ERROR,
                        ("Failed to get debug level from registry.  "
                         "Using default\n"));
                    DeviceData->DebugLevel = DebugLevelDefault;
                }

                ExFreePool( QueryTable );
            }

            status = Cycladz_GetResourceInfo(DeviceObject,                    
                        IrpStack->Parameters.StartDevice.AllocatedResources,
                        IrpStack->Parameters.StartDevice.AllocatedResourcesTranslated);

            if (NT_SUCCESS(status)) {

                ULONG numberOfResources = CYZ_NUMBER_OF_RESOURCES;
                if (!DeviceData->IsPci) {
                    numberOfResources--;
                }
                status = Cycladz_BuildResourceList(&DeviceData->PChildResourceList,
                                    &DeviceData->PChildResourceListSize,
                                    IrpStack->Parameters.StartDevice.AllocatedResources,
                                    numberOfResources);

                if (!NT_SUCCESS(status)) {
                    goto CaseSTART_end;
                }

                status = Cycladz_BuildResourceList(&DeviceData->PChildResourceListTr,
                                    &DeviceData->PChildResourceListSizeTr,
                                    IrpStack->Parameters.StartDevice.AllocatedResourcesTranslated,
                                    numberOfResources);

                if (!NT_SUCCESS(status)) {
                    goto CaseSTART_end;
                }
              
                status = Cycladz_BuildRequirementsList(&DeviceData->PChildRequiredList,
                                    IrpStack->Parameters.StartDevice.AllocatedResources,
                                    numberOfResources);

                if (!NT_SUCCESS(status)) {
                    goto CaseSTART_end;
                }

                //
                // See if we are in the proper power state.
                //

                if (DeviceData->DeviceState != PowerDeviceD0) {

                    status = Cycladz_GotoPowerState(DeviceData->UnderlyingPDO, DeviceData, 
                                              PowerDeviceD0);

                    if (!NT_SUCCESS(status)) {
                        goto CaseSTART_end;
                    }
                }
              
                numOfPorts=Cycladz_DoesBoardExist(DeviceData);
                if (!numOfPorts){
                    Cycladz_KdPrint(DeviceData,SER_DBG_CYCLADES,("Does Port exist test failed\n"));
                    status = STATUS_SERIAL_NO_DEVICE_INITED;
                    goto CaseSTART_end;
                }
                Cycladz_KdPrint(DeviceData,SER_DBG_CYCLADES,("Board found!\n"));

                // Save number of ports to the Registry, so that Property Page
                // code can retrieve it.
    
                IoOpenDeviceRegistryKey(DeviceData->UnderlyingPDO,PLUGPLAY_REGKEY_DEVICE,
                    STANDARD_RIGHTS_WRITE,&instanceKey);

                RtlInitUnicodeString(&keyName,L"NumOfPorts");
                ZwSetValueKey(instanceKey,&keyName,0,REG_DWORD,&numOfPorts,sizeof(ULONG));

                ZwFlushKey(instanceKey);
                ZwClose(instanceKey);

                Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                                ("Start Device: Device started successfully\n"));
                SET_NEW_PNP_STATE(DeviceData, Started);

                // TODO: FOR NOW, LET'S KEEP THIS DEVICE IN POWER D0. 
                // THE SERIAL DRIVER SEEMS TO POWER DOWN TO D3, AND BECOME D0 DURING OPEN.
                // BUT NOT SURE IF THE BOARD NEED TO BE IN D0 WHILE THE CHILD DEVICES
                // ARE ENUMARATED.

            }                                
        }

CaseSTART_end:
        if (!NT_SUCCESS(status)) {
            Cycladz_ReleaseResources(DeviceData);
        }
        
        //
        // We must now complete the IRP, since we stopped it in the
        // completetion routine with MORE_PROCESSING_REQUIRED.
        //

        //Irp->IoStatus.Information = 0;  Removed in build 2072
        break;

    case IRP_MN_QUERY_STOP_DEVICE:

        //
        // Test to see if there are any PDO created as children of this FDO
        // If there are then conclude the device is busy and fail the
        // query stop.
        //
        // CIMEXCIMEX   (BUGBUG replaced by CIMEXCIMEX on build 2128 - Fanny)
        // We could do better, by seing if the children PDOs are actually
        // currently open.  If they are not then we could stop, get new
        // resouces, fill in the new resouce values, and then when a new client
        // opens the PDO use the new resources.  But this works for now.
        //
//TODO FANNY: FOR NOW WE WILL ALWAYS ACCEPT TO STOP DEVICE. REVIEW THIS LATER...
//        if (DeviceData->AttachedPDO) {
//            status = STATUS_UNSUCCESSFUL;
//        } else {
//            status = STATUS_SUCCESS;
//        }

        status = STATUS_SUCCESS;

        Irp->IoStatus.Status = status;

        if (NT_SUCCESS(status)) {
           SET_NEW_PNP_STATE(DeviceData, StopPending);
           IoSkipCurrentIrpStackLocation (Irp);
           status = IoCallDriver (DeviceData->TopOfStack, Irp);
        } else {
           IoCompleteRequest(Irp, IO_NO_INCREMENT);
        }

        Cycladz_DecIoCount (DeviceData);
        return status;

    case IRP_MN_CANCEL_STOP_DEVICE:

        KeInitializeEvent (&event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext (Irp);

        IoSetCompletionRoutine (Irp,
                                CycladzSyncCompletion,
                                &event,
                                TRUE,
                                TRUE,
                                TRUE);

        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            // wait for it...

            status = KeWaitForSingleObject (&event,
                                            Executive,
                                            KernelMode,
                                            FALSE, // Not allertable
                                            NULL); // No timeout structure

            ASSERT (STATUS_SUCCESS == status);

            status = Irp->IoStatus.Status;
        }

        if(StopPending == DeviceData->DevicePnPState)
        {
            //
            // We did receive a query-stop, so restore.
            //             
            RESTORE_PREVIOUS_PNP_STATE(DeviceData);
            ASSERT(DeviceData->DevicePnPState == Started);
        }        

        break;

    case IRP_MN_STOP_DEVICE:

        //
        // After the start IRP has been sent to the lower driver object, the
        // bus may NOT send any more IRPS down ``touch'' until another START
        // has occured.
        // What ever access is required must be done before the Irp is passed
        // on.
        //
        // Stop device means that the resources given durring Start device
        // are no revoked.  So we need to stop using them
        //
        if (DeviceData->Runtime && DeviceData->BoardMemory) {
            ULONG mail_box_0;

            mail_box_0 = CYZ_READ_ULONG(&(DeviceData->Runtime)->mail_box_0);
			if ((mail_box_0 == 0) || z_fpga_check(DeviceData)) {
				z_stop_cpu(DeviceData);
			}
        }
        Cycladz_ReleaseResources(DeviceData);

        SET_NEW_PNP_STATE(DeviceData, Stopped);

        //
        // We don't need a completion routine so fire and forget.
        //
        // Set the current stack location to the next stack location and
        // call the next device object.
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        Cycladz_DecIoCount (DeviceData);
        return status;

    case IRP_MN_QUERY_REMOVE_DEVICE:
        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //

        SET_NEW_PNP_STATE(DeviceData, RemovePending);

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);
        Cycladz_DecIoCount (DeviceData);
        return status;

    case IRP_MN_CANCEL_REMOVE_DEVICE:

        //
        // If we were to fail this call then we would need to complete the
        // IRP here.  Since we are not, set the status to SUCCESS and
        // call the next driver.
        //
        
        //
        // First check to see whether you have received cancel-remove
        // without first receiving a query-remove. This could happen if 
        // someone above us fails a query-remove and passes down the 
        // subsequent cancel-remove.
        //
        
        if(RemovePending == DeviceData->DevicePnPState)
        {
            //
            // We did receive a query-remove, so restore.
            //             
            RESTORE_PREVIOUS_PNP_STATE(DeviceData);
        }
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);
        Cycladz_DecIoCount (DeviceData);
        return status;
        
    case IRP_MN_SURPRISE_REMOVAL:

        SET_NEW_PNP_STATE(DeviceData, SurpriseRemovePending);

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);
        Cycladz_DecIoCount (DeviceData);
        return status;

    case IRP_MN_REMOVE_DEVICE:

        //
        // The PlugPlay system has detected the removal of this device.  We
        // have no choice but to detach and delete the device object.
        // (If we wanted to express and interest in preventing this removal,
        // we should have filtered the query remove and query stop routines.)
        //
        // Note! we might receive a remove WITHOUT first receiving a stop.
        // ASSERT (!DeviceData->Removed);

        // We will accept no new requests
        //
//        DeviceData->Removed = TRUE;
        SET_NEW_PNP_STATE(DeviceData, Deleted);

        //
        // Complete any outstanding IRPs queued by the driver here.
        //

        //
        // Make the DCA go away.  Some drivers may choose to remove the DCA
        // when they receive a stop or even a query stop.  We just don't care.
        //
        IoSetDeviceInterfaceState (&DeviceData->DevClassAssocName, FALSE);

        //
        // Here if we had any outstanding requests in a personal queue we should
        // complete them all now.
        //
        // Note, the device is guarenteed stopped, so we cannot send it any non-
        // PNP IRPS.
        //

        //
        // Wait for all outstanding requests to complete
        //
        Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
            ("Waiting for outstanding requests\n"));
        i = InterlockedDecrement (&DeviceData->OutstandingIO);

        ASSERT (0 < i);

        if (0 != InterlockedDecrement (&DeviceData->OutstandingIO)) {
            Cycladz_KdPrint (DeviceData, SER_DBG_PNP_INFO,
                          ("Remove Device waiting for request to complete\n"));

            KeWaitForSingleObject (&DeviceData->RemoveEvent,
                                   Executive,
                                   KernelMode,
                                   FALSE, // Not Alertable
                                   NULL); // No timeout
        }

        // Stop hw
        if (DeviceData->Runtime && DeviceData->BoardMemory) {
            ULONG mail_box_0;

            mail_box_0 = CYZ_READ_ULONG(&(DeviceData->Runtime)->mail_box_0);
            if ((mail_box_0 == 0) || z_fpga_check(DeviceData)) {
                z_stop_cpu(DeviceData);
            }
            //z_reset_board(DeviceData);
        }

        //
        // Fire and forget
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        //
        // Free the associated resources
        //

        //
        // Detach from the underlying devices.
        //
        Cycladz_KdPrint(DeviceData, SER_DBG_PNP_INFO,
                        ("IoDetachDevice: 0x%x\n", DeviceData->TopOfStack));
        IoDetachDevice (DeviceData->TopOfStack);

        //
        // Clean up any resources here
        //
        Cycladz_ReleaseResources(DeviceData);

        ExFreePool (DeviceData->DevClassAssocName.Buffer);
        Cycladz_KdPrint(DeviceData, SER_DBG_PNP_INFO,
                        ("IoDeleteDevice: 0x%x\n", DeviceObject));

        //
        // Remove any PDO's we ejected
        //
//FANNY: CHANGED TO SUPPORT MORE THAN ONE CHILD DEVICE
//        if (DeviceData->AttachedPDO != NULL) {
//           ASSERT(DeviceData->NumPDOs == 1);
//
//           Cycladz_PnPRemove(DeviceData->AttachedPDO, DeviceData->PdoData);
//           DeviceData->PdoData = NULL;
//           DeviceData->AttachedPDO = NULL;
//           DeviceData->NumPDOs = 0;
//        }

        i=DeviceData->NumPDOs;
        while(i--) {
           if (DeviceData->AttachedPDO[i] != NULL) {
              //(DeviceData->PdoData[i])->Attached = FALSE; Moved to PDO IRP_MN_SURPRISE_REMOVAL.
              if(SurpriseRemovePending != (DeviceData->PdoData[i])->DevicePnPState) {
                 Cycladz_PnPRemove(DeviceData->AttachedPDO[i], DeviceData->PdoData[i]);
              }
              DeviceData->PdoData[i] = NULL;
              DeviceData->AttachedPDO[i] = NULL;
           }
        }
        DeviceData->NumPDOs = 0;

        IoDeleteDevice(DeviceObject);

        return status;


    case IRP_MN_QUERY_DEVICE_RELATIONS:
        Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE, 
                    ("\tQueryDeviceRelation Type: %d\n", 
                    IrpStack->Parameters.QueryDeviceRelations.Type));

        if (BusRelations != IrpStack->Parameters.QueryDeviceRelations.Type) {
            //
            // We don't support this
            //
            Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                ("Query Device Relations - Non bus\n"));
            goto CYZ_FDO_PNP_DEFAULT;
        }

        Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
            ("\tQuery Bus Relations\n"));

        // Check for new devices or if old devices still there.
        status = Cycladz_ReenumerateDevices(Irp, DeviceData );

        //
        // Tell the plug and play system about all the PDOs.
        //
        // There might also be device relations below and above this FDO,
        // so, be sure to propagate the relations from the upper drivers.
        //
        // No Completion routine is needed so long as the status is preset
        // to success.  (PDOs complete plug and play irps with the current
        // IoStatus.Status and IoStatus.Information as the default.)
        //

        //KeAcquireSpinLock (&DeviceData->Spin, &oldIrq);

        i = (0 == Irp->IoStatus.Information) ? 0 :
            ((PDEVICE_RELATIONS) Irp->IoStatus.Information)->Count;
        // The current number of PDOs in the device relations structure

        Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                           ("#PDOS = %d + %d\n", i, DeviceData->NumPDOs));

        length = sizeof(DEVICE_RELATIONS) +
                ((DeviceData->NumPDOs + i) * sizeof (PDEVICE_OBJECT));

        relations = (PDEVICE_RELATIONS) ExAllocatePool (NonPagedPool, length);

        if (NULL == relations) {
           Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
           IoCompleteRequest(Irp, IO_NO_INCREMENT);
           Cycladz_DecIoCount(DeviceData);
           return STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Copy in the device objects so far
        //
        if (i) {
            RtlCopyMemory (
                  relations->Objects,
                  ((PDEVICE_RELATIONS) Irp->IoStatus.Information)->Objects,
                  i * sizeof (PDEVICE_OBJECT));
        }

        relations->Count = DeviceData->NumPDOs + i;


        //
        // For each PDO on this bus add a pointer to the device relations
        // buffer, being sure to take out a reference to that object.
        // The PlugPlay system will dereference the object when it is done with
        // it and free the device relations buffer.
        //

        //FANNY: CHANGED TO SUPPORT ADDITIONAL CHILD DEVICES
//        if (DeviceData->NumPDOs) {
//            relations->Objects[relations->Count-1] = DeviceData->AttachedPDO;
//            ObReferenceObject (DeviceData->AttachedPDO);
//        }

        for (i=0; i< DeviceData->NumPDOs; i++) {
           relations->Objects[relations->Count - DeviceData->NumPDOs + i] = 
                                                               DeviceData->AttachedPDO[i];
           ObReferenceObject (DeviceData->AttachedPDO[i]);

           Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE,
                           ("Child PDOS: %x\n", DeviceData->AttachedPDO[i]));
        }

        //
        // Set up and pass the IRP further down the stack
        //
        Irp->IoStatus.Status = STATUS_SUCCESS;

        if (0 != Irp->IoStatus.Information) {
            ExFreePool ((PVOID) Irp->IoStatus.Information);
        }
        Irp->IoStatus.Information = (ULONG_PTR)relations;

        IoSkipCurrentIrpStackLocation (Irp);
        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        Cycladz_DecIoCount (DeviceData);

        return status;

    case IRP_MN_QUERY_CAPABILITIES: {

        PIO_STACK_LOCATION  irpSp;

        //
        // Send this down to the PDO first
        //

        KeInitializeEvent (&event, NotificationEvent, FALSE);
        IoCopyCurrentIrpStackLocationToNext (Irp);

        IoSetCompletionRoutine (Irp,
                                CycladzSyncCompletion,
                                &event,
                                TRUE,
                                TRUE,
                                TRUE);

        status = IoCallDriver (DeviceData->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            // wait for it...

            status = KeWaitForSingleObject (&event,
                                            Executive,
                                            KernelMode,
                                            FALSE, // Not allertable
                                            NULL); // No timeout structure

            ASSERT (STATUS_SUCCESS == status);

            status = Irp->IoStatus.Status;
        }

        if (NT_SUCCESS(status)) {

            irpSp = IoGetCurrentIrpStackLocation(Irp);

            DeviceData->SystemWake
                = irpSp->Parameters.DeviceCapabilities.Capabilities->SystemWake;
            DeviceData->DeviceWake
                = irpSp->Parameters.DeviceCapabilities.Capabilities->DeviceWake;

            Cycladz_KdPrint(DeviceData, SER_DBG_PNP_INFO, ("SystemWake %d\n",DeviceData->SystemWake)); 
            Cycladz_KdPrint(DeviceData, SER_DBG_PNP_INFO, ("DeviceWake %d\n",DeviceData->DeviceWake)); 
        }

        break;
    }

    default:
        //
        // In the default case we merely call the next driver since
        // we don't know what to do.
        //
        Cycladz_KdPrint(DeviceData, SER_DBG_PNP_TRACE, 
                 ("FDO(%x):%s not handled\n", DeviceObject,
                        PnPMinorFunctionString(IrpStack->MinorFunction)));
CYZ_FDO_PNP_DEFAULT:

        //
        // Fire and Forget
        //
        IoSkipCurrentIrpStackLocation (Irp);

        //
        // Done, do NOT complete the IRP, it will be processed by the lower
        // device object, which will complete the IRP
        //

        status = IoCallDriver (DeviceData->TopOfStack, Irp);
        Cycladz_DecIoCount (DeviceData);
        return status;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    Cycladz_DecIoCount (DeviceData);
    return status;
}


NTSTATUS
Cycladz_PDO_PnP (IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp,
                 IN PIO_STACK_LOCATION IrpStack, IN PPDO_DEVICE_DATA DeviceData)
/*++
Routine Description:
    Handle requests from the PlugPlay system for the devices on the BUS

--*/
{
   PDEVICE_CAPABILITIES    deviceCapabilities;
   ULONG                   information;
   PWCHAR                  buffer;
   ULONG                   length, i, j;
   NTSTATUS                status;
   KIRQL                   oldIrq;
   HANDLE                  keyHandle;
   UNICODE_STRING          keyName;
   PWCHAR returnBuffer = NULL;

   PAGED_CODE();

   status = Irp->IoStatus.Status;

   //
   // NB: since we are a bus enumerator, we have no one to whom we could
   // defer these irps.  Therefore we do not pass them down but merely
   // return them.
   //

   switch (IrpStack->MinorFunction) {
   case IRP_MN_QUERY_CAPABILITIES:

      //
      // Get the packet.
      //

      deviceCapabilities=IrpStack->Parameters.DeviceCapabilities.Capabilities;

      //
      // Set the capabilities.
      //

      deviceCapabilities->Version = 1;
      deviceCapabilities->Size = sizeof (DEVICE_CAPABILITIES);

      //
      // We cannot wake the system.
      //

      deviceCapabilities->SystemWake 
          = ((PFDO_DEVICE_DATA)DeviceData->ParentFdo->DeviceExtension)
            ->SystemWake;
      deviceCapabilities->DeviceWake 
          = ((PFDO_DEVICE_DATA)DeviceData->ParentFdo->DeviceExtension)
            ->DeviceWake;

      //
      // We have no latencies
      //

      deviceCapabilities->D1Latency = 0;
      deviceCapabilities->D2Latency = 0;
      deviceCapabilities->D3Latency = 0;

      deviceCapabilities->UniqueID = FALSE;

      // 
      // Initialize supported DeviceState
      //

      deviceCapabilities->DeviceState[PowerSystemWorking] = PowerDeviceD0;
      deviceCapabilities->DeviceState[PowerSystemSleeping1] = PowerDeviceD3;
      deviceCapabilities->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
      deviceCapabilities->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
      deviceCapabilities->DeviceState[PowerSystemHibernate] = PowerDeviceD3;
      deviceCapabilities->DeviceState[PowerSystemShutdown] = PowerDeviceD3;

      status = STATUS_SUCCESS;
      break;

   case IRP_MN_QUERY_DEVICE_TEXT: {
      if ((IrpStack->Parameters.QueryDeviceText.DeviceTextType
          != DeviceTextDescription) || DeviceData->DevDesc.Buffer == NULL) {
         break;
      }

// FANNY - CHANGE TO MaximumLength
//      returnBuffer = ExAllocatePool(PagedPool, DeviceData->DevDesc.Length);
      returnBuffer = ExAllocatePool(PagedPool, DeviceData->DevDesc.MaximumLength);

      Cycladz_KdPrint(DeviceData, SER_DBG_CYCLADES,("returnBuffer %x\n", returnBuffer));

      if (returnBuffer == NULL) {
         status = STATUS_INSUFFICIENT_RESOURCES;
         break;
      }

      status = STATUS_SUCCESS;

// FANNY - CHANGE TO MaximumLength
//      RtlCopyMemory(returnBuffer, DeviceData->DevDesc.Buffer,
//                    DeviceData->DevDesc.Length);
      RtlCopyMemory(returnBuffer, DeviceData->DevDesc.Buffer,
                    DeviceData->DevDesc.MaximumLength);

      Cycladz_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                            ("TextID: buf %ws\n", returnBuffer));

      Cycladz_KdPrint(DeviceData, SER_DBG_CYCLADES,
                            ("DevDesc.Length is %d and DevDesc.MaximumLength is %d\n", 
                              DeviceData->DevDesc.Length,DeviceData->DevDesc.MaximumLength));

      Irp->IoStatus.Information = (ULONG_PTR)returnBuffer;
      break;
   }


   case IRP_MN_QUERY_ID:
      //
      // Query the IDs of the device
      //

      switch (IrpStack->Parameters.QueryId.IdType) {

      case BusQueryDeviceID:
      case BusQueryHardwareIDs:
      case BusQueryCompatibleIDs:
      case BusQueryInstanceID:
         {
            PUNICODE_STRING pId;
            status = STATUS_SUCCESS;

            switch (IrpStack->Parameters.QueryId.IdType) {
            case BusQueryDeviceID:
               pId = &DeviceData->DeviceIDs;
               break;

            case BusQueryHardwareIDs:
               pId = &DeviceData->HardwareIDs;
               break;

            case BusQueryCompatibleIDs:
               pId = &DeviceData->CompIDs;
               break;

            case BusQueryInstanceID:
            // Build an instance ID.  This is what PnP uses to tell if it has
            // seen this thing before or not.  Build it from the first hardware
            // id and the port number.
               pId = &DeviceData->InstanceIDs;
               break;
            }

            buffer = pId->Buffer;

            if (buffer != NULL) {
               // FANNY CHANGED
               //length = pId->Length;
               length = pId->MaximumLength;
               returnBuffer = ExAllocatePool(PagedPool, length);
               if (returnBuffer != NULL) {
#if DBG
                  RtlFillMemory(returnBuffer, length, 0xff);
#endif
                  // FANNY CHANGED
                  //RtlCopyMemory(returnBuffer, buffer, pId->Length);
                  RtlCopyMemory(returnBuffer, buffer, length);
               } else {
                  status = STATUS_INSUFFICIENT_RESOURCES;
               }
            } else {
               // FANNY ADDED
               status = STATUS_NOT_FOUND;
            }

            Cycladz_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                            ("ID: Unicode 0x%x\n", pId));
            Cycladz_KdPrint(DeviceData, SER_DBG_PNP_TRACE,
                            ("ID: buf 0x%x\n", returnBuffer));

            Irp->IoStatus.Information = (ULONG_PTR)returnBuffer;
         }
         break;

      }
      break;

      case IRP_MN_QUERY_BUS_INFORMATION: {
       PPNP_BUS_INFORMATION pBusInfo;
       PFDO_DEVICE_DATA parentExtension;
       parentExtension = (DeviceData->ParentFdo)->DeviceExtension;

       ASSERTMSG("Cycladz appears not to be the sole bus?!?",
                 Irp->IoStatus.Information == (ULONG_PTR)NULL);

       pBusInfo = ExAllocatePool(PagedPool, sizeof(PNP_BUS_INFORMATION));

       if (pBusInfo == NULL) {
          status = STATUS_INSUFFICIENT_RESOURCES;
          break;
       }

       pBusInfo->BusTypeGuid = GUID_BUS_TYPE_CYCLADESZ;
       if (parentExtension->IsPci) {
         pBusInfo->LegacyBusType = PCIBus;
       } else {
         pBusInfo->LegacyBusType = Isa;
       }

       //
       // We really can't track our bus number since we can be torn
       // down with our bus
       //

       //pBusInfo->BusNumber = 0;
       pBusInfo->BusNumber = parentExtension->UINumber;

       Irp->IoStatus.Information = (ULONG_PTR)pBusInfo;
       status = STATUS_SUCCESS;
       break;
       }

   case IRP_MN_QUERY_DEVICE_RELATIONS:
      Cycladz_KdPrint (DeviceData, SER_DBG_PNP_TRACE, 
                    ("\tQueryDeviceRelation Type: %d\n", 
                    IrpStack->Parameters.QueryDeviceRelations.Type));

      switch (IrpStack->Parameters.QueryDeviceRelations.Type) {
      case TargetDeviceRelation: {
         PDEVICE_RELATIONS pDevRel;

         //
         // No one else should respond to this since we are the PDO
         //

         ASSERT(Irp->IoStatus.Information == 0);

         if (Irp->IoStatus.Information != 0) {
            break;
         }


         pDevRel = ExAllocatePool(PagedPool, sizeof(DEVICE_RELATIONS));

         if (pDevRel == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
         }

         pDevRel->Count = 1;
         pDevRel->Objects[0] = DeviceObject;
         ObReferenceObject(DeviceObject);

         status = STATUS_SUCCESS;
         Irp->IoStatus.Information = (ULONG_PTR)pDevRel;
         break;
      }


      default:
         break;
      }

      break;

   case IRP_MN_START_DEVICE:

      //
      // Set the hw resources in the registry for this device.
      //

      status = IoOpenDeviceRegistryKey(DeviceObject, PLUGPLAY_REGKEY_DEVICE,
                                       STANDARD_RIGHTS_WRITE, &keyHandle);

      if (!NT_SUCCESS(status)) {
         //
         // This is a fatal error.  If we can't get to our registry key,
         // we are sunk.
         //
         Cycladz_KdPrint(DeviceData, SER_DBG_SS_ERROR,
                          ("IoOpenDeviceRegistryKey failed - %x\n", status));
      } else {

         ULONG portIndex;
         PFDO_DEVICE_DATA parentExtension;
         
         // Set the Port Index in the Registry
         
         RtlInitUnicodeString(&keyName, L"PortIndex");

         portIndex = DeviceData->PortIndex;

         //
         // Doesn't matter whether this works or not.
         //

         ZwSetValueKey(keyHandle, &keyName, 0, REG_DWORD, &portIndex,
                       sizeof(ULONG));

         parentExtension = (DeviceData->ParentFdo)->DeviceExtension;

         RtlInitUnicodeString(&keyName, L"PortResources");

         status = ZwSetValueKey(keyHandle, &keyName, 0, REG_RESOURCE_LIST, 
                       parentExtension->PChildResourceList,
                       parentExtension->PChildResourceListSize);

         RtlInitUnicodeString(&keyName, L"PortResourcesTr");

         status = ZwSetValueKey(keyHandle, &keyName, 0, REG_RESOURCE_LIST, 
                       parentExtension->PChildResourceListTr,
                       parentExtension->PChildResourceListSizeTr);

         RtlInitUnicodeString(&keyName, L"FirmwareVersion");

         status = ZwSetValueKey(keyHandle, &keyName, 0, REG_DWORD, 
                       &parentExtension->FirmwareVersion,
                       sizeof(ULONG));
         
         ZwFlushKey(keyHandle);
         ZwClose(keyHandle);
      }

      SET_NEW_PNP_STATE(DeviceData, Started);
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_QUERY_STOP_DEVICE:

      //
      // No reason here why we can't stop the device.
      // If there were a reason we should speak now for answering success
      // here may result in a stop device irp.
      //

      SET_NEW_PNP_STATE(DeviceData, StopPending);
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_CANCEL_STOP_DEVICE:
      //
      // The stop was canceled.  Whatever state we set, or resources we put
      // on hold in anticipation of the forcoming STOP device IRP should be
      // put back to normal.  Someone, in the long list of concerned parties,
      // has failed the stop device query.
      //

      //
      // First check to see whether you have received cancel-stop
      // without first receiving a query-stop. This could happen if someone
      // above us fails a query-stop and passes down the subsequent
      // cancel-stop.
      //
        
      if(StopPending == DeviceData->DevicePnPState)
      {
          //
          // We did receive a query-stop, so restore.
          //             
          RESTORE_PREVIOUS_PNP_STATE(DeviceData);
      }
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_STOP_DEVICE:

      //
      // Here we shut down the device.  The opposite of start.
      //

      SET_NEW_PNP_STATE(DeviceData, Stopped);
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_QUERY_REMOVE_DEVICE:
      //
      // Just like Query Stop only now the impending doom is the remove irp
      //
      SET_NEW_PNP_STATE(DeviceData, RemovePending);
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_CANCEL_REMOVE_DEVICE:
      //
      // Clean up a remove that did not go through, just like cancel STOP.
      //
      //

      // First check to see whether you have received cancel-remove
      // without first receiving a query-remove. This could happen if 
      // someone above us fails a query-remove and passes down the 
      // subsequent cancel-remove.
      //
       
      if(RemovePending == DeviceData->DevicePnPState)
      {
          //
          // We did receive a query-remove, so restore.
          //             
          RESTORE_PREVIOUS_PNP_STATE(DeviceData);
      }
      status = STATUS_SUCCESS;
      break;

   case IRP_MN_SURPRISE_REMOVAL:

        //
        // We should stop all access to the device and relinquish all the
        // resources. Let's just mark that it happened and we will do 
        // the cleanup later in IRP_MN_REMOVE_DEVICE.
        //

        SET_NEW_PNP_STATE(DeviceData, SurpriseRemovePending);
        DeviceData->Attached = FALSE;
        status = STATUS_SUCCESS;
        break;

   case IRP_MN_REMOVE_DEVICE:

      //
      // Attached is only set to FALSE by the enumeration process.
      //
      if (!DeviceData->Attached) {

          SET_NEW_PNP_STATE(DeviceData, Deleted);
          status = Cycladz_PnPRemove(DeviceObject, DeviceData);
      }
      else {    // else added in build 2128 - Fanny
          //
          // Succeed the remove
          ///
          SET_NEW_PNP_STATE(DeviceData, NotStarted);
          status = STATUS_SUCCESS;
      }

// Changed in build 2072
//      status = STATUS_SUCCESS;

      break;

   case IRP_MN_QUERY_RESOURCES: {
#if 0
      PCM_RESOURCE_LIST pChildRes, pQueryRes;
      PFDO_DEVICE_DATA parentExtension;
      ULONG listSize;

      parentExtension = (DeviceData->ParentFdo)->DeviceExtension;
      pChildRes = parentExtension->PChildResourceList;
      listSize = parentExtension->PChildResourceListSize;

      if (pChildRes) {
         pQueryRes = ExAllocatePool(PagedPool, listSize);
         if (pQueryRes == NULL) {
            Irp->IoStatus.Information = (ULONG_PTR) NULL;
            status = STATUS_INSUFFICIENT_RESOURCES;
         } else {
            RtlCopyMemory(pQueryRes,pChildRes,listSize);
            Irp->IoStatus.Information = (ULONG_PTR)pQueryRes;
            status = STATUS_SUCCESS;
         }
      }
#endif
      break;

   }

   case IRP_MN_QUERY_RESOURCE_REQUIREMENTS: {
#if 0      
      PIO_RESOURCE_REQUIREMENTS_LIST pChildReq, pQueryReq;
      PFDO_DEVICE_DATA parentExtension;

      parentExtension = (DeviceData->ParentFdo)->DeviceExtension;
      pChildReq = parentExtension->PChildRequiredList;
      if (pChildReq) {
         pQueryReq = ExAllocatePool(PagedPool, pChildReq->ListSize);
         if (pQueryReq == NULL) {
            Irp->IoStatus.Information = (ULONG_PTR) NULL;
            status = STATUS_INSUFFICIENT_RESOURCES;
         } else {
            RtlCopyMemory(pQueryReq,pChildReq,pChildReq->ListSize);
            Irp->IoStatus.Information = (ULONG_PTR)pQueryReq;
            status = STATUS_SUCCESS;
         }
      }
#endif
      break;
   }

   case IRP_MN_READ_CONFIG:
   case IRP_MN_WRITE_CONFIG: // we have no config space
   case IRP_MN_EJECT:
   case IRP_MN_SET_LOCK:
   case IRP_MN_QUERY_INTERFACE: // We do not have any non IRP based interfaces.
   default:
      Cycladz_KdPrint(DeviceData, SER_DBG_PNP_TRACE, 
                 ("PDO(%x):%s not handled\n", DeviceObject,
                        PnPMinorFunctionString(IrpStack->MinorFunction)));

      // For PnP requests to the PDO that we do not understand we should
      // return the IRP WITHOUT setting the status or information fields.
      // They may have already been set by a filter (eg acpi).
      break;
   }

   Irp->IoStatus.Status = status;
   IoCompleteRequest (Irp, IO_NO_INCREMENT);

   return status;
}

NTSTATUS
Cycladz_PnPRemove (PDEVICE_OBJECT Device, PPDO_DEVICE_DATA PdoData)
/*++
Routine Description:
    The PlugPlay subsystem has instructed that this PDO should be removed.

    We should therefore
    - Complete any requests queued in the driver
    - If the device is still attached to the system,
      then complete the request and return.
    - Otherwise, cleanup device specific allocations, memory, events...
    - Call IoDeleteDevice
    - Return from the dispatch routine.

    Note that if the device is still connected to the bus (IE in this case
    the control panel has not yet told us that the serial device has
    disappeared) then the PDO must remain around, and must be returned during
    any query Device relaions IRPS.

--*/

{
   Cycladz_KdPrint(PdoData, SER_DBG_PNP_TRACE,
                        ("Cycladz_PnPRemove: 0x%x\n", Device));
    //
    // Complete any outstanding requests with STATUS_DELETE_PENDING.
    //
    // Serenum does not queue any irps at this time so we have nothing to do.
    //

    //REMOVED BY FANNY. THIS CHECK IS ALREADY DONE AT IRP_MN_REMOVE_DEVICE PDO.
    //if (PdoData->Attached) {
    //    return STATUS_SUCCESS;
    //}
    //PdoData->Removed = TRUE;
    
    //
    // Free any resources.
    //

    CycladzFreeUnicodeString(&PdoData->HardwareIDs);
    //CycladzFreeUnicodeString(&PdoData->CompIDs); We never allocate CompIDs.
    RtlFreeUnicodeString(&PdoData->DeviceIDs);
    RtlFreeUnicodeString(&PdoData->InstanceIDs);
    RtlFreeUnicodeString(&PdoData->DevDesc);

    Cycladz_KdPrint(PdoData, SER_DBG_PNP_INFO,
                        ("IoDeleteDevice: 0x%x\n", Device));

    IoDeleteDevice(Device);


    return STATUS_SUCCESS;
}


NTSTATUS
Cycladz_GetResourceInfo(IN PDEVICE_OBJECT PDevObj,
                    IN PCM_RESOURCE_LIST PResList,
                    IN PCM_RESOURCE_LIST PTrResList)
/*++

Routine Description:

    This routine gets the resources that PnP allocated to this device.


Arguments:

   PDevObj    -  Pointer to the devobj that is starting

   PResList   -  Pointer to the untranslated resources needed by this device

   PTrResList -  Pointer to the translated resources needed by this device


  Return Value:

    STATUS_SUCCESS on success, something else appropriate on failure


--*/

{
   PFDO_DEVICE_DATA pDevExt = PDevObj->DeviceExtension;
   NTSTATUS status = STATUS_SUCCESS;

   ULONG count;
   ULONG i;
   PCM_PARTIAL_RESOURCE_LIST pPartialResourceList, pPartialTrResourceList;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialResourceDesc, pPartialTrResourceDesc;
   PCM_FULL_RESOURCE_DESCRIPTOR pFullResourceDesc = NULL,
                                pFullTrResourceDesc = NULL;
   KAFFINITY Affinity;
   KINTERRUPT_MODE InterruptMode;
   ULONG zero = 0;
   
   PAGED_CODE();

   // Let's get our resources
   pFullResourceDesc   = &PResList->List[0];
   pFullTrResourceDesc = &PTrResList->List[0];

   if (pFullResourceDesc) {
      pPartialResourceList    = &pFullResourceDesc->PartialResourceList;
      pPartialResourceDesc    = pPartialResourceList->PartialDescriptors;
      count                   = pPartialResourceList->Count;

      pDevExt->InterfaceType  = pFullResourceDesc->InterfaceType;
      pDevExt->BusNumber      = pFullResourceDesc->BusNumber;

      for (i = 0;     i < count;     i++, pPartialResourceDesc++) {

         switch (pPartialResourceDesc->Type) {
         case CmResourceTypeMemory: {

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypeMemory\n"));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Memory.Start = %x\n",
                                           pPartialResourceDesc->u.Memory.Start));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Memory.Length = %x\n",
                                           pPartialResourceDesc->u.Memory.Length));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Flags = %x\n",
                                           pPartialResourceDesc->Flags));
            
            if (pPartialResourceDesc->u.Memory.Length == CYZ_RUNTIME_LENGTH) {
               pDevExt->PhysicalRuntime = pPartialResourceDesc->u.Memory.Start;
               pDevExt->RuntimeLength = pPartialResourceDesc->u.Memory.Length;
            } else {
               pDevExt->PhysicalBoardMemory = pPartialResourceDesc->u.Memory.Start;
               pDevExt->BoardMemoryLength = pPartialResourceDesc->u.Memory.Length;
            }
            break;
         }
         case CmResourceTypePort: {

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypePort\n"));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Port.Start = %x\n",
                                           pPartialResourceDesc->u.Port.Start));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Port.Length = %x\n",
                                           pPartialResourceDesc->u.Port.Length));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Flags = %x\n",
                                           pPartialResourceDesc->Flags));

            break;
         }

         case CmResourceTypeInterrupt: {

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypeInterrupt\n"));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Interrupt.Level = %x\n",
                                           pPartialResourceDesc->u.Interrupt.Level));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Interrupt.Vector = %x\n",
                                           pPartialResourceDesc->u.Interrupt.Vector));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Interrupt.Affinity = %x\n",
                                           pPartialResourceDesc->u.Interrupt.Affinity));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Flags = %x\n",
                                           pPartialResourceDesc->Flags));
#ifndef POLL
            pDevExt->OriginalIrql = pPartialResourceDesc->u.Interrupt.Level;
            pDevExt->OriginalVector =pPartialResourceDesc->u.Interrupt.Vector;
            Affinity = pPartialResourceDesc->u.Interrupt.Affinity;

            if (pPartialResourceDesc->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {
               InterruptMode = Latched;
            } else {
               InterruptMode = LevelSensitive;
            }
#endif
            break;
         }

         case CmResourceTypeDeviceSpecific: {
            PCM_SERIAL_DEVICE_DATA sDeviceData;

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypeDeviceSpecific\n"));

            break;
         }


         default: {
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceType = %x\n",
                                                      pPartialResourceDesc->Type));
            break;
         }
         }   // switch (pPartialResourceDesc->Type)
      }       // for (i = 0;     i < count;     i++, pPartialResourceDesc++)
   }           // if (pFullResourceDesc)



//SEE_LATER_IF_IT_SHOULD_BE_ADDED
//   //
//   // Do the same for the translated resources
//   //
//
//   gotInt = 0;
//   gotISR = 0;
//   gotIO = 0;
//   curIoIndex = 0;
//
   if (pFullTrResourceDesc) {
      pPartialTrResourceList = &pFullTrResourceDesc->PartialResourceList;
      pPartialTrResourceDesc = pPartialTrResourceList->PartialDescriptors;
      count = pPartialTrResourceList->Count;

      //
      // Reload PConfig with the translated values for later use
      //

      pDevExt->InterfaceType  = pFullTrResourceDesc->InterfaceType;
      pDevExt->BusNumber      = pFullTrResourceDesc->BusNumber;

//FANNY
//      pDevExt->TrInterruptStatus = SerialPhysicalZero;

      for (i = 0;     i < count;     i++, pPartialTrResourceDesc++) {

         switch (pPartialTrResourceDesc->Type) {
         case CmResourceTypeMemory: {

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypeMemory translated\n"));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Memory.Start = %x\n",
                                           pPartialTrResourceDesc->u.Memory.Start));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Memory.Length = %x\n",
                                           pPartialTrResourceDesc->u.Memory.Length));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Flags = %x\n",
                                           pPartialTrResourceDesc->Flags));

            if (pPartialTrResourceDesc->u.Memory.Length == CYZ_RUNTIME_LENGTH) {
               pDevExt->TranslatedRuntime = pPartialTrResourceDesc->u.Memory.Start;
               pDevExt->RuntimeLength = pPartialTrResourceDesc->u.Memory.Length;
            } else {
               pDevExt->TranslatedBoardMemory = pPartialTrResourceDesc->u.Memory.Start;
               pDevExt->BoardMemoryLength = pPartialTrResourceDesc->u.Memory.Length;
            }
            break;
         }
         case CmResourceTypePort: {

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypePort translated\n"));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Port.Start = %x\n",
                                           pPartialTrResourceDesc->u.Port.Start));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Port.Length = %x\n",
                                           pPartialTrResourceDesc->u.Port.Length));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Flags = %x\n",
                                           pPartialTrResourceDesc->Flags));

            break;
         }

         case CmResourceTypeInterrupt: {

            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("CmResourceTypeInterrupt translated\n"));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Interrupt.Level = %x\n",
                                           pPartialTrResourceDesc->u.Interrupt.Level));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Interrupt.Vector = %x\n",
                                           pPartialTrResourceDesc->u.Interrupt.Vector));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("u.Interrupt.Affinity = %x\n",
                                           pPartialTrResourceDesc->u.Interrupt.Affinity));
            Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Flags = %x\n",
                                           pPartialTrResourceDesc->Flags));
#ifndef POLL
            pDevExt->Vector = pPartialTrResourceDesc->u.Interrupt.Vector;
            pDevExt->Irql = (KIRQL) pPartialTrResourceDesc->u.Interrupt.Level;
            Affinity = pPartialTrResourceDesc->u.Interrupt.Affinity;
#endif
            break;
         }

         default: {
               break;
         }
         }   // switch (pPartialTrResourceDesc->Type)
      }       // for (i = 0;     i < count;     i++, pPartialTrResourceDesc++)
   }           // if (pFullTrResourceDesc)


   //
   // Do some error checking on the configuration info we have.
   //
   // Make sure that the interrupt is non zero (which we defaulted
   // it to).
   //
   // Make sure that the portaddress is non zero (which we defaulted
   // it to).
   //
   // Make sure that the DosDevices is not NULL (which we defaulted
   // it to).
   //
   // We need to make sure that if an interrupt status
   // was specified, that a port index was also specfied,
   // and if so that the port index is <= maximum ports
   // on a board.
   //
   // We should also validate that the bus type and number
   // are correct.
   //
   // We will also validate that the interrupt mode makes
   // sense for the bus.
   //

   if (!pDevExt->TranslatedRuntime.LowPart && pDevExt->IsPci) {

      CyzLogError(
                    pDevExt->DriverObject,
                    NULL,
                    pDevExt->PhysicalBoardMemory,
                    CyzPhysicalZero,
                    0,
                    0,
                    0,
                    0,
                    STATUS_SUCCESS,
                    CYZ_INVALID_RUNTIME_REGISTERS,
                    0,
                    NULL,
                    0,
                    NULL
                    );
      Cycladz_KdPrint (pDevExt,SER_DBG_CYCLADES,
                  ("Bogus Runtime address %x\n",
                   pDevExt->TranslatedRuntime.LowPart));

      status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetResourceInfo_Cleanup;
   }


   if (!pDevExt->TranslatedBoardMemory.LowPart) {

      CyzLogError(
                    pDevExt->DriverObject,
                    NULL,
                    pDevExt->PhysicalBoardMemory,
                    CyzPhysicalZero,
                    0,
                    0,
                    0,
                    0,
                    STATUS_SUCCESS,
                    CYZ_INVALID_BOARD_MEMORY,
                    0,
                    NULL,
                    0,
                    NULL
                    );
      Cycladz_KdPrint (pDevExt,SER_DBG_CYCLADES,
                  ("Bogus BoardMemory address %x\n",
                   pDevExt->TranslatedBoardMemory.LowPart));

      status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetResourceInfo_Cleanup;
   }
#ifndef POLL

   if (!pDevExt->OriginalVector) {

      CyzLogError(
                    pDevExt->DriverObject,
                    NULL,
                    pDevExt->PhysicalBoardMemory,
                    CyzPhysicalZero,
                    0,
                    0,
                    0,
                    0,
                    STATUS_SUCCESS,
                    CYZ_INVALID_INTERRUPT,
                    0,
                    NULL,
                    0,
                    NULL
                    );
      Cycladz_KdPrint (pDevExt,SER_DBG_CYCLADES,("Bogus vector %x\n",
                             pDevExt->OriginalVector));

      status = STATUS_INSUFFICIENT_RESOURCES;
      goto GetResourceInfo_Cleanup;
   }
#endif

   //
   // We don't want to cause the hal to have a bad day,
   // so let's check the interface type and bus number.
   //
   // We only need to check the registry if they aren't
   // equal to the defaults.
   //

   if (pDevExt->BusNumber != 0) {

      BOOLEAN foundIt = 0;

      if (pDevExt->InterfaceType >= MaximumInterfaceType) {

         CyzLogError(
                       pDevExt->DriverObject,
                       NULL,
                       pDevExt->PhysicalBoardMemory,
                       CyzPhysicalZero,
                       0,
                       0,
                       0,
                       pDevExt->InterfaceType,
                       STATUS_SUCCESS,
                       CYZ_UNKNOWN_BUS,
                       0,
                       NULL,
                       0,
                       NULL
                       );
         Cycladz_KdPrint (pDevExt,SER_DBG_CYCLADES,
                  ("Invalid Bus type %x\n", pDevExt->BusNumber));

         //status = SERIAL_UNKNOWN_BUS;
         status = STATUS_INSUFFICIENT_RESOURCES;
         goto GetResourceInfo_Cleanup;
      }    

      IoQueryDeviceDescription(
                              (INTERFACE_TYPE *)&pDevExt->InterfaceType,
                              &zero,
                              NULL,
                              NULL,
                              NULL,
                              NULL,
                              Cycladz_ItemCallBack,
                              &foundIt
                              );

      if (!foundIt) {

         CyzLogError(
                       pDevExt->DriverObject,
                       NULL,
                       pDevExt->PhysicalBoardMemory,
                       CyzPhysicalZero,
                       0,
                       0,
                       0,
                       pDevExt->InterfaceType,
                       STATUS_SUCCESS,
                       CYZ_BUS_NOT_PRESENT,
                       0,
                       NULL,
                       0,
                       NULL
                       );
         Cycladz_KdPrint(
                   pDevExt,
                   SER_DBG_CYCLADES,
                   ("There aren't that many of those\n"
                    "------- busses on this system,%x\n",
                    pDevExt->BusNumber)
                   );

         //status = SERIAL_BUS_NOT_PRESENT;
         status = STATUS_INSUFFICIENT_RESOURCES;
         goto GetResourceInfo_Cleanup;
      }
   }


   //
   // Dump out the board configuration.
   //

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES, ("PhysicalRuntime: %x\n",
                          pDevExt->PhysicalRuntime.LowPart));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES, ("TranslatedRuntime: %x\n",
                          pDevExt->TranslatedRuntime.LowPart));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES, ("RuntimeLength: %x\n",
                          pDevExt->RuntimeLength));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES, ("PhysicalBoardMemory: %x\n",
                          pDevExt->PhysicalBoardMemory.LowPart));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES, ("TranslatedBoardMemory: %x\n",
                          pDevExt->TranslatedBoardMemory.LowPart));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES, ("BoardMemoryLength: %x\n",
                          pDevExt->BoardMemoryLength));
#ifndef POLL
   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("OriginalIrql = %x\n",
                          pDevExt->OriginalIrql));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("OriginalVector = %x\n",
                          pDevExt->OriginalVector));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Irql = %x\n",
                          pDevExt->Irql));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Vector = %x\n",
                          pDevExt->Vector));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Affinity = %x\n",
                          Affinity));
#endif
   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("InterfaceType = %x\n",
                          pDevExt->InterfaceType));

   Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("BusNumber = %x\n",
                          pDevExt->BusNumber));

   // ABOVE: COPIED FROM SerialGetPortInfo
   // ------------------------------------

   // BELOW: COPIED FROM SerialInitController
   if (pDevExt->IsPci) {
      pDevExt->Runtime = MmMapIoSpace(pDevExt->TranslatedRuntime,
                                      pDevExt->RuntimeLength,
                                      FALSE);

      if (!pDevExt->Runtime){

         CyzLogError(
                       pDevExt->DriverObject,
                       NULL,
                       pDevExt->PhysicalBoardMemory,
                       CyzPhysicalZero,
                       0,
                       0,
                       0,
                       0,
                       STATUS_SUCCESS,
                       CYZ_RUNTIME_NOT_MAPPED,
                       0,
                       NULL,
                       0,
                       NULL
                       );
         Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Could not map memory for Runtime\n"));
         status = STATUS_NONE_MAPPED;
         goto GetResourceInfo_Cleanup;
      }
   }
   pDevExt->BoardMemory = MmMapIoSpace(pDevExt->TranslatedBoardMemory,
                                       pDevExt->BoardMemoryLength,
                                       FALSE);

   if (!pDevExt->BoardMemory){

      CyzLogError(
                    pDevExt->DriverObject,
                    NULL,
                    pDevExt->PhysicalBoardMemory,
                    CyzPhysicalZero,
                    0,
                    0,
                    0,
                    0,
                    STATUS_SUCCESS,
                    CYZ_BOARD_NOT_MAPPED,
                    0,
                    NULL,
                    0,
                    NULL
                    );
      Cycladz_KdPrint(pDevExt,SER_DBG_CYCLADES,("Could not map memory for DP memory"));
      status = STATUS_NONE_MAPPED;
      goto GetResourceInfo_Cleanup;
   }


GetResourceInfo_Cleanup:
   if (!NT_SUCCESS(status)) {
      
      if (pDevExt->Runtime) {
         MmUnmapIoSpace(pDevExt->Runtime, pDevExt->RuntimeLength);
	      pDevExt->Runtime = NULL;
      }

      if (pDevExt->BoardMemory) {
         MmUnmapIoSpace(pDevExt->BoardMemory, pDevExt->BoardMemoryLength);
 		   pDevExt->BoardMemory = NULL;
      }
   }

   Cycladz_KdPrint (pDevExt,SER_DBG_CYCLADES, ("leaving Cycladz_GetResourceInfo\n"));
   return status;
}

VOID
Cycladz_ReleaseResources(IN PFDO_DEVICE_DATA PDevExt)
{   
   Cycladz_KdPrint (PDevExt,SER_DBG_CYCLADES, ("entering Cycladz_ReleaseResources\n"));

   if (PDevExt->PChildRequiredList) {
      ExFreePool(PDevExt->PChildRequiredList);
      PDevExt->PChildRequiredList = NULL;
   }

   if (PDevExt->PChildResourceList) {
      ExFreePool(PDevExt->PChildResourceList);
      PDevExt->PChildResourceList = NULL;
   }

   if (PDevExt->PChildResourceListTr) {
      ExFreePool(PDevExt->PChildResourceListTr);
      PDevExt->PChildResourceListTr = NULL;
   }

   if (PDevExt->Runtime) {
      MmUnmapIoSpace(PDevExt->Runtime, PDevExt->RuntimeLength);
      PDevExt->Runtime = NULL;
   }

   if (PDevExt->BoardMemory) {
      MmUnmapIoSpace(PDevExt->BoardMemory, PDevExt->BoardMemoryLength);
      PDevExt->BoardMemory = NULL;
   }
   Cycladz_KdPrint (PDevExt,SER_DBG_CYCLADES, ("leaving Cycladz_ReleaseResources\n"));   
}


NTSTATUS
Cycladz_ItemCallBack(
                  IN PVOID Context,
                  IN PUNICODE_STRING PathName,
                  IN INTERFACE_TYPE BusType,
                  IN ULONG BusNumber,
                  IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
                  IN CONFIGURATION_TYPE ControllerType,
                  IN ULONG ControllerNumber,
                  IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
                  IN CONFIGURATION_TYPE PeripheralType,
                  IN ULONG PeripheralNumber,
                  IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
                  )

/*++

Routine Description:

    This routine is called to check if a particular item
    is present in the registry.

Arguments:

    Context - Pointer to a boolean.

    PathName - unicode registry path.  Not Used.

    BusType - Internal, Isa, ...

    BusNumber - Which bus if we are on a multibus system.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Controller type.

    ControllerNumber - Which controller if there is more than one
                       controller in the system.

    ControllerInformation - Array of pointers to the three pieces of
                            registry information.

    PeripheralType - Should be a peripheral.

    PeripheralNumber - Which peripheral - not used..

    PeripheralInformation - Configuration information. Not Used.

Return Value:

    STATUS_SUCCESS

--*/

{
   UNREFERENCED_PARAMETER (PathName);
   UNREFERENCED_PARAMETER (BusType);
   UNREFERENCED_PARAMETER (BusNumber);
   UNREFERENCED_PARAMETER (BusInformation);
   UNREFERENCED_PARAMETER (ControllerType);
   UNREFERENCED_PARAMETER (ControllerNumber);
   UNREFERENCED_PARAMETER (ControllerInformation);
   UNREFERENCED_PARAMETER (PeripheralType);
   UNREFERENCED_PARAMETER (PeripheralNumber);
   UNREFERENCED_PARAMETER (PeripheralInformation);

   PAGED_CODE();


   *((BOOLEAN *)Context) = TRUE;
   return STATUS_SUCCESS;
}


NTSTATUS
Cycladz_BuildRequirementsList(
                          OUT PIO_RESOURCE_REQUIREMENTS_LIST *PChildRequiredList_Pointer,
                          IN PCM_RESOURCE_LIST PResourceList, IN ULONG NumberOfResources
                          )
{

   NTSTATUS status = STATUS_SUCCESS;
   ULONG count;
   ULONG i,j;   
   PCM_FULL_RESOURCE_DESCRIPTOR    pFullResourceDesc = NULL;
   PCM_PARTIAL_RESOURCE_LIST       pPartialResourceList;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialResourceDesc;

   ULONG requiredLength;
   PIO_RESOURCE_REQUIREMENTS_LIST requiredList;
   PIO_RESOURCE_LIST       requiredResList;
   PIO_RESOURCE_DESCRIPTOR requiredResDesc;

   *PChildRequiredList_Pointer = NULL;

   // Validate input parameter

   if (PResourceList == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      goto CycladzBuildRequirementList_Error;
   }

   ASSERT(PResourceList->Count == 1);

   // Initialize requiredList

   requiredLength = sizeof(IO_RESOURCE_REQUIREMENTS_LIST) 
                + sizeof(IO_RESOURCE_DESCRIPTOR) * (NumberOfResources - 1);
 
   requiredList = ExAllocatePool(PagedPool, requiredLength);
   
   if (requiredList == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      goto CycladzBuildRequirementList_Error;
   }

   RtlZeroMemory(requiredList, requiredLength);

   // Get information from PResourceList and build requiredList

   pFullResourceDesc = &PResourceList->List[0];

   if (pFullResourceDesc) {
      pPartialResourceList = &pFullResourceDesc->PartialResourceList;
      pPartialResourceDesc = pPartialResourceList->PartialDescriptors;
      count                = pPartialResourceList->Count;

      if (count < NumberOfResources) {
         ExFreePool(requiredList);
         return STATUS_INSUFFICIENT_RESOURCES;
      }

      requiredList->ListSize = requiredLength;
      requiredList->InterfaceType = pFullResourceDesc->InterfaceType;
      requiredList->BusNumber     = pFullResourceDesc->BusNumber;
      requiredList->SlotNumber    = 0; //?????? There's no SlotNumber in the Resource List
      requiredList->AlternativeLists = 1;

      requiredResList = &requiredList->List[0];
      requiredResList->Count = NumberOfResources;

      requiredResDesc = &requiredResList->Descriptors[0];

      for (i=0,j=0; i<count && j<NumberOfResources;  i++,pPartialResourceDesc++) {
         
         switch (pPartialResourceDesc->Type) {
         case CmResourceTypeMemory: {
            requiredResDesc->Type = pPartialResourceDesc->Type;
            //requiredResDesc->ShareDisposition = pPartialResourceDesc->ShareDisposition;
            requiredResDesc->ShareDisposition = CmResourceShareShared;
            requiredResDesc->Flags = pPartialResourceDesc->Flags;
            requiredResDesc->u.Memory.Length = pPartialResourceDesc->u.Memory.Length;
            requiredResDesc->u.Memory.Alignment = 4;
            requiredResDesc->u.Memory.MinimumAddress = pPartialResourceDesc->u.Memory.Start;
            requiredResDesc->u.Memory.MaximumAddress.QuadPart 
                        = pPartialResourceDesc->u.Memory.Start.QuadPart 
                        + pPartialResourceDesc->u.Memory.Length - 1;
            requiredResDesc++;
            j++;
            break;
         }
         case CmResourceTypePort: {
            break;
         }
         case CmResourceTypeInterrupt: {
            requiredResDesc->Type = pPartialResourceDesc->Type;
            requiredResDesc->ShareDisposition = CmResourceShareShared;
            requiredResDesc->Flags = pPartialResourceDesc->Flags;
            requiredResDesc->u.Interrupt.MinimumVector 
                                             = pPartialResourceDesc->u.Interrupt.Vector;
            requiredResDesc->u.Interrupt.MaximumVector 
                                             = pPartialResourceDesc->u.Interrupt.Vector;
            requiredResDesc++;
            j++;
            break;
         }
         default: 
            break;
         } // end switch
         
      } // end for

   } // end if (pFullResourceDesc)

   *PChildRequiredList_Pointer = requiredList;


CycladzBuildRequirementList_Error:
   return status;

}

NTSTATUS
Cycladz_BuildResourceList(
                      OUT PCM_RESOURCE_LIST *POutList_Pointer,
                      OUT ULONG *ListSize_Pointer,
                      IN PCM_RESOURCE_LIST PInList,
                      IN ULONG NumberOfResources
                      )
{

   NTSTATUS status = STATUS_SUCCESS;
   ULONG i,j;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialInDesc;

   ULONG length;
   PCM_RESOURCE_LIST pOutList;
   PCM_PARTIAL_RESOURCE_DESCRIPTOR  pPartialOutDesc;

   *POutList_Pointer = NULL;
   *ListSize_Pointer =0;

   // Validate input parameter

   if (PInList == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      goto CycladzBuildResourceList_Error;
   }

   ASSERT(PInList->Count == 1);


   if (PInList->List[0].PartialResourceList.Count < NumberOfResources) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      goto CycladzBuildResourceList_Error;
   }
   
   // Initialize pOutList

   length = sizeof(CM_RESOURCE_LIST) 
            + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * (NumberOfResources - 1);
 
   pOutList = ExAllocatePool(PagedPool, length);
   
   if (pOutList == NULL) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      goto CycladzBuildResourceList_Error;
   }

   RtlZeroMemory(pOutList, length);
   
   // Get information from PInList and build pOutList

   pOutList->Count = 1; // not sure if we have to report Translated information too.
   pOutList->List[0].InterfaceType = PInList->List[0].InterfaceType;
   pOutList->List[0].BusNumber     = PInList->List[0].BusNumber;
   pOutList->List[0].PartialResourceList.Count = NumberOfResources;

   pPartialOutDesc = &pOutList->List[0].PartialResourceList.PartialDescriptors[0];
   pPartialInDesc  = &PInList->List[0].PartialResourceList.PartialDescriptors[0];

   for (i=0,j=0; i < PInList->List[0].PartialResourceList.Count; i++,pPartialInDesc++) {
      if (j==NumberOfResources) {
         break;
      }
      switch(pPartialInDesc->Type) {
      case CmResourceTypeMemory:
         pPartialOutDesc->ShareDisposition = CmResourceShareShared;
         pPartialOutDesc->Type             = pPartialInDesc->Type;
         pPartialOutDesc->Flags            = pPartialInDesc->Flags;
         pPartialOutDesc->u.Memory.Start   = pPartialInDesc->u.Memory.Start;
         pPartialOutDesc->u.Memory.Length  = pPartialInDesc->u.Memory.Length;
         pPartialOutDesc++;
         j++;
         break;
      case CmResourceTypeInterrupt:
         pPartialOutDesc->ShareDisposition = CmResourceShareShared;
         pPartialOutDesc->Type             = pPartialInDesc->Type;
         pPartialOutDesc->Flags            = pPartialInDesc->Flags;
         pPartialOutDesc->u.Interrupt.Level    = pPartialInDesc->u.Interrupt.Level;
         pPartialOutDesc->u.Interrupt.Vector   = pPartialInDesc->u.Interrupt.Vector;
         pPartialOutDesc->u.Interrupt.Affinity = pPartialInDesc->u.Interrupt.Affinity;
         pPartialOutDesc++;
         j++;
         break;
      default:
         break;
      } // end switch
   } // end for
   
   *POutList_Pointer = pOutList;
   *ListSize_Pointer = length;

CycladzBuildResourceList_Error:
   return status;

}
#if 0

VOID
Cycladz_Delay(
	ULONG NumberOfMilliseconds
    )
/*--------------------------------------------------------------------------
    Cycladz_Delay()
    
    Routine Description: Delay routine.
    
    Arguments:
    
    NumberOfMilliseconds - Number of milliseconds to be delayed.
    
    Return Value: none.
--------------------------------------------------------------------------*/
{
    LARGE_INTEGER startOfSpin, nextQuery, difference, delayTime;

    delayTime.QuadPart = NumberOfMilliseconds*10*1000; // unit is 100ns
    KeQueryTickCount(&startOfSpin);

    do {			
        KeQueryTickCount(&nextQuery);
        difference.QuadPart = nextQuery.QuadPart - startOfSpin.QuadPart;
        ASSERT(KeQueryTimeIncrement() <= MAXLONG);
        if (difference.QuadPart * KeQueryTimeIncrement() >= 
                                        delayTime.QuadPart) {
            break;															
        }
    } while (1);

}
#endif

ULONG
Cycladz_DoesBoardExist(
                   IN PFDO_DEVICE_DATA Extension
                   )

/*++

Routine Description:

    This routine examines if the board is present.


Arguments:

    Extension - A pointer to a serial device extension.

Return Value:

    Will return number of ports.

--*/

{
   ULONG numPorts = 0;
   int z_load_status;
   ULONG j;
   ULONG n_channel;
   ULONG fw_version;   
   WCHAR FwVersionBuffer[10];
   UNICODE_STRING FwVersion;
   struct FIRM_ID *pt_firm_id;
   struct ZFW_CTRL *pt_zfw;
   struct BOARD_CTRL *pt_board_ctrl;
   LARGE_INTEGER d250ms = RtlConvertLongToLargeInteger(-250*10000);

   z_reset_board(Extension);

   z_load_status = z_load(Extension, 0, L"zlogic.cyz");
   Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("z_load returned %x\n",z_load_status));

   // Error injection
   //z_load_status = ZL_RET_FPGA_ERROR;
   //---

   switch (z_load_status){
   case ZL_RET_SUCCESS: 
      break;	// Success
   case ZL_RET_NO_MATCHING_FW_CONFIG:
      CyzLogError( Extension->DriverObject,Extension->Self,
                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
                   0,0,0,0,STATUS_SUCCESS,CYZ_NO_MATCHING_FW_CONFIG,
                   0,NULL,0,NULL);
      break;
   case ZL_RET_FILE_OPEN_ERROR:
      CyzLogError( Extension->DriverObject,Extension->Self,
                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
                   0,0,0,0,STATUS_SUCCESS,CYZ_FILE_OPEN_ERROR,
                   0,NULL,0,NULL);
      break;
   case ZL_RET_FPGA_ERROR:			
      CyzLogError( Extension->DriverObject,Extension->Self,
                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
                   0,0,0,0,STATUS_SUCCESS,CYZ_FPGA_ERROR,
                   0,NULL,0,NULL);
      break;
   case ZL_RET_FILE_READ_ERROR:
      CyzLogError( Extension->DriverObject,Extension->Self,
                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
                   0,0,0,0,STATUS_SUCCESS,CYZ_FILE_READ_ERROR,
                   0,NULL,0,NULL);
      break;
   }
   
   if (z_load_status != ZL_RET_SUCCESS) {
      goto DoesBoardExistEnd;
   }

   Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("z_load worked\n"));

   pt_firm_id = (struct FIRM_ID *) (Extension->BoardMemory + ID_ADDRESS);
											
   for (j=0; j<8; j++) {
      KeDelayExecutionThread(KernelMode,FALSE,&d250ms);

      if (CYZ_READ_ULONG(&pt_firm_id->signature) == ZFIRM_ID) {
         break;
      }				
   }	

   // Error injection
   //j=8;
   //--
			
   if (j==8) {
      if (CYZ_READ_ULONG(&pt_firm_id->signature) == ZFIRM_HLT) {
         Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("ZFIRM_HLT\n"));
         CyzLogError( Extension->DriverObject,Extension->Self,
                      Extension->PhysicalBoardMemory,CyzPhysicalZero,
                      0,0,0,0,STATUS_SUCCESS,CYZ_POWER_SUPPLY,
                      0,NULL,0,NULL);
      } else {
         Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("Firmware didn't start\n"));
         CyzLogError( Extension->DriverObject,Extension->Self,
                      Extension->PhysicalBoardMemory,CyzPhysicalZero,
                      0,0,0,0,STATUS_SUCCESS,CYZ_FIRMWARE_NOT_STARTED,
                      0,NULL,0,NULL);
      }
      goto DoesBoardExistEnd;
   }
									
   // Firmware was correclty loaded and initialized.
   // Get number of channels.

   pt_zfw = (struct ZFW_CTRL *)(Extension->BoardMemory +
                                CYZ_READ_ULONG(&pt_firm_id->zfwctrl_addr));

   pt_board_ctrl = &pt_zfw->board_ctrl;

   n_channel = CYZ_READ_ULONG(&pt_board_ctrl->n_channel);
   Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("n_channel = %d\n",n_channel));

   //------- TEMP CODE-----------
   //n_channel = 1;
   //------ END TEMP CODE -------

   fw_version = CYZ_READ_ULONG(&pt_board_ctrl->fw_version);

   RtlInitUnicodeString(&FwVersion, NULL);
   FwVersion.MaximumLength = sizeof(FwVersionBuffer);
   FwVersion.Buffer = FwVersionBuffer;
   RtlIntegerToUnicodeString(fw_version, 16, &FwVersion);
   
   CyzLogError( Extension->DriverObject,Extension->Self,
                Extension->PhysicalBoardMemory,CyzPhysicalZero,
                0,0,0,0,STATUS_SUCCESS,CYZ_FIRMWARE_VERSION,
                FwVersion.Length,FwVersion.Buffer,
                0,NULL);
   
   if (n_channel == 0) {
      Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("No channel\n"));
      CyzLogError( Extension->DriverObject,Extension->Self,
                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
                   0,0,0,0,STATUS_SUCCESS,CYZ_BOARD_WITH_NO_PORT,
                   0,NULL,0,NULL);
      goto DoesBoardExistEnd;
   }

   // Error injection
   //n_channel = 65;
   //-----

   if (n_channel > 64) {
      Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("Invalid number of channels (more than 64).\n"));
      CyzLogError( Extension->DriverObject,Extension->Self,
                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
                   0,0,0,0,STATUS_SUCCESS,CYZ_BOARD_WITH_TOO_MANY_PORTS,
                   0,NULL,0,NULL);
      goto DoesBoardExistEnd;
   }

//   // Error injection
//   //if (fw_version >= Z_COMPATIBLE_FIRMWARE) 
//   //-----
//
//   if (fw_version < Z_COMPATIBLE_FIRMWARE) {
//      Cycladz_KdPrint(Extension,SER_DBG_CYCLADES,("Incompatible firmware\n"));
//      CyzLogError( Extension->DriverObject,Extension->Self,
//                   Extension->PhysicalBoardMemory,CyzPhysicalZero,
//                   0,0,0,0,STATUS_SUCCESS,CYZ_INCOMPATIBLE_FIRMWARE,
//                   0,NULL,0,NULL);
//      goto DoesBoardExistEnd;
//   }

   Extension->FirmwareVersion = fw_version;
   Extension->NumPorts = n_channel;
   numPorts = n_channel;

DoesBoardExistEnd:

   return numPorts;

}

PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
)
{
    switch (MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            return "IRP_MN_START_DEVICE";
        case IRP_MN_QUERY_REMOVE_DEVICE:
            return "IRP_MN_QUERY_REMOVE_DEVICE";
        case IRP_MN_REMOVE_DEVICE:
            return "IRP_MN_REMOVE_DEVICE";
        case IRP_MN_CANCEL_REMOVE_DEVICE:
            return "IRP_MN_CANCEL_REMOVE_DEVICE";
        case IRP_MN_STOP_DEVICE:
            return "IRP_MN_STOP_DEVICE";
        case IRP_MN_QUERY_STOP_DEVICE:
            return "IRP_MN_QUERY_STOP_DEVICE";
        case IRP_MN_CANCEL_STOP_DEVICE:
            return "IRP_MN_CANCEL_STOP_DEVICE";
        case IRP_MN_QUERY_DEVICE_RELATIONS:
            return "IRP_MN_QUERY_DEVICE_RELATIONS";
        case IRP_MN_QUERY_INTERFACE:
            return "IRP_MN_QUERY_INTERFACE";
        case IRP_MN_QUERY_CAPABILITIES:
            return "IRP_MN_QUERY_CAPABILITIES";
        case IRP_MN_QUERY_RESOURCES:
            return "IRP_MN_QUERY_RESOURCES";
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
            return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
        case IRP_MN_QUERY_DEVICE_TEXT:
            return "IRP_MN_QUERY_DEVICE_TEXT";
        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
            return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
        case IRP_MN_READ_CONFIG:
            return "IRP_MN_READ_CONFIG";
        case IRP_MN_WRITE_CONFIG:
            return "IRP_MN_WRITE_CONFIG";
        case IRP_MN_EJECT:
            return "IRP_MN_EJECT";
        case IRP_MN_SET_LOCK:
            return "IRP_MN_SET_LOCK";
        case IRP_MN_QUERY_ID:
            return "IRP_MN_QUERY_ID";
        case IRP_MN_QUERY_PNP_DEVICE_STATE:
            return "IRP_MN_QUERY_PNP_DEVICE_STATE";
        case IRP_MN_QUERY_BUS_INFORMATION:
            return "IRP_MN_QUERY_BUS_INFORMATION";
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
            return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
        case IRP_MN_SURPRISE_REMOVAL:
            return "IRP_MN_SURPRISE_REMOVAL";
        case IRP_MN_QUERY_LEGACY_BUS_INFORMATION:
            return "IRP_MN_QUERY_LEGACY_BUS_INFORMATION";
        default:
            return "IRP_MN_?????";
    }
}
