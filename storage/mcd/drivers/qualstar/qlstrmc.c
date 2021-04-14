
/*++

Copyright (C) Microsoft Corporation, 1996 - 1999

Module Name:

    qlstrmc.c

Abstract:

    This module contains device-specific routines for Qualstar's 
        medium changers:
            TLS-4xxx (w/Exabyte 8mm Drives, Sony SDX300 8mm Drives)
            TLS-2xxx (w/HP, Conner/Seagate, Sony 4mm Drives)

Author:

    davet (Dave Therrien)

Environment:

    kernel mode only

Revision History:


--*/

#include "ntddk.h"
#include "mcd.h"
#include "qlstrmc.h"


#ifdef  ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)

#pragma alloc_text(PAGE, ChangerExchangeMedium)
#pragma alloc_text(PAGE, ChangerGetElementStatus)
#pragma alloc_text(PAGE, ChangerGetParameters)
#pragma alloc_text(PAGE, ChangerGetProductData)
#pragma alloc_text(PAGE, ChangerGetStatus)
#pragma alloc_text(PAGE, ChangerInitialize)
#pragma alloc_text(PAGE, ChangerInitializeElementStatus)
#pragma alloc_text(PAGE, ChangerMoveMedium)
#pragma alloc_text(PAGE, ChangerPerformDiagnostics)
#pragma alloc_text(PAGE, ChangerQueryVolumeTags)
#pragma alloc_text(PAGE, ChangerReinitializeUnit)
#pragma alloc_text(PAGE, ChangerSetAccess)
#pragma alloc_text(PAGE, ChangerSetPosition)
#pragma alloc_text(PAGE, ElementOutOfRange)
#pragma alloc_text(PAGE, MapExceptionCodes)
#pragma alloc_text(PAGE, ExaBuildAddressMapping)
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    MCD_INIT_DATA mcdInitData;

    RtlZeroMemory(&mcdInitData, sizeof(MCD_INIT_DATA));

    mcdInitData.InitDataSize = sizeof(MCD_INIT_DATA);

    mcdInitData.ChangerAdditionalExtensionSize = ChangerAdditionalExtensionSize;

    mcdInitData.ChangerError = ChangerError;

    mcdInitData.ChangerInitialize = ChangerInitialize;

    mcdInitData.ChangerPerformDiagnostics = ChangerPerformDiagnostics;

    mcdInitData.ChangerGetParameters = ChangerGetParameters;
    mcdInitData.ChangerGetStatus = ChangerGetStatus;
    mcdInitData.ChangerGetProductData = ChangerGetProductData;
    mcdInitData.ChangerSetAccess = ChangerSetAccess;
    mcdInitData.ChangerGetElementStatus = ChangerGetElementStatus;
    mcdInitData.ChangerInitializeElementStatus = ChangerInitializeElementStatus;
    mcdInitData.ChangerSetPosition = ChangerSetPosition;
    mcdInitData.ChangerExchangeMedium = ChangerExchangeMedium;
    mcdInitData.ChangerMoveMedium = ChangerMoveMedium;
    mcdInitData.ChangerReinitializeUnit = ChangerReinitializeUnit;
    mcdInitData.ChangerQueryVolumeTags = ChangerQueryVolumeTags;

    return ChangerClassInitialize(DriverObject, RegistryPath, 
                                  &mcdInitData);
}


ULONG
ChangerAdditionalExtensionSize(
    VOID
    )

/*++

Routine Description:

    This routine returns the additional device extension size
    needed by the qualstar changers.

Arguments:


Return Value:

    Size, in bytes.

--*/

{

    return sizeof(CHANGER_DATA);
}




NTSTATUS
ChangerInitialize(
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA  changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    NTSTATUS       status;
    PINQUIRYDATA   dataBuffer;
    PCDB           cdb;
    ULONG          length;
    SCSI_REQUEST_BLOCK srb;
    PCONFIG_MODE_PAGE    modeBuffer;

    changerData->Size = sizeof(CHANGER_DATA);

    //
    // Build address mapping.
    //

    status = ExaBuildAddressMapping(DeviceObject);
    if (!NT_SUCCESS(status)) {
        DebugPrint((1,
                    "BuildAddressMapping failed. %x\n", status));
        return status;
    }

    //
    // Get inquiry data.
    //

    dataBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(INQUIRYDATA));
    if (!dataBuffer) {
        DebugPrint((1,
                    "BuildAddressMapping failed. %x\n", status));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now get the full inquiry information for the device.
    //

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // Set timeout value.
    //

    srb.TimeOutValue = 10;

    srb.CdbLength = 6;

    cdb = (PCDB)srb.Cdb;

    //
    // Set CDB operation code.
    //

    cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;

    //
    // Set allocation length to inquiry data buffer size.
    //

    cdb->CDB6INQUIRY.AllocationLength = sizeof(INQUIRYDATA);

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         dataBuffer,
                                         sizeof(INQUIRYDATA),
                                         FALSE);

    if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_SUCCESS ||
        SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

        //
        // Updated the length actually transfered.
        //

        length = dataBuffer->AdditionalLength + FIELD_OFFSET(INQUIRYDATA, Reserved);

        if (length > srb.DataTransferLength) {
            length = srb.DataTransferLength;
        }


        RtlMoveMemory(&changerData->InquiryData, dataBuffer, length);

        //
        // Determine drive id.
        //

        if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4210",8) == 8) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4210A",9) == 9) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4220",8) == 8) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4420",8) == 8) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4440",8) == 8) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4480",8) == 8) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-4660",8) == 8) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-46120",9) == 9) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-2218",8) == 8) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-2218A",9) == 9) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-2236",8) == 8) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-2436",8) == 8) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-2472",8) == 8) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-24144",9) == 9) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS SDX 220",11) == 11) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS SDX 420",11) == 11) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS SDX 440",11) == 11) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS SDX 480",11) == 11) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS SDX 660",11) == 11) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS SDX 6120",12) == 12) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS 4G 236",10) == 10) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS 4G 436",10) == 10) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS 4G 472",10) == 10) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"QLS 4G 4144",11) == 11) {
            changerData->DriveID = TLS_2xxx;
            changerData->DriveType = D_4MM;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"TLS-412360", 10) == 10) {
            changerData->DriveID = TLS_4xxx;
            changerData->DriveType = D_8MM;
        }
    }

    ChangerClassFreePool(dataBuffer);

    // 
    // -------------------------------------------------------------------------
    // 
    // THREE Library Config parameters will be set by this driver
    // 
    // 1) Config/Hndlr/DoorOpen/Hold
    //      ( to deal with someone opening the front door randomly

    
    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, 
                                   sizeof(CONFIG_MODE_PAGE));
    if (!modeBuffer) {
       return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, sizeof(CONFIG_MODE_PAGE));
    modeBuffer->PageCode = 0x3D;
    modeBuffer->PageLength = 0x28;
    modeBuffer->Length = 0;
    modeBuffer->Type = 0;
    modeBuffer->Write = 1;
    RtlMoveMemory(modeBuffer->VariableName,"HNDLR_DOOR_OPEN", 15);
    RtlMoveMemory(modeBuffer->VariableValue,"HOLD", 4);

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);
    srb.CdbLength = CDB6GENERIC_LENGTH;
    srb.TimeOutValue = 20;
    srb.DataTransferLength = sizeof(CONFIG_MODE_PAGE);
    srb.DataBuffer = modeBuffer;

    cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
    cdb->MODE_SELECT.PFBit = 1;
    cdb->MODE_SELECT.ParameterListLength = (UCHAR)srb.DataTransferLength;
   
    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             &srb,
                                             modeBuffer,
                                             sizeof(CONFIG_MODE_PAGE),
                                             TRUE);
    ChangerClassFreePool(modeBuffer);

    // -----------------------------------------------------------
    // 2) Config/Hndlr/Io Call Key / Disabled
    //      ( to allow NTMS to timeout )
   
    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, 
                                   sizeof(CONFIG_MODE_PAGE));
    if (!modeBuffer) {
       return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, sizeof(CONFIG_MODE_PAGE));
    modeBuffer->PageCode = 0x3D;
    modeBuffer->PageLength = 0x28;
    modeBuffer->Length = 0;
    modeBuffer->Type = 0;
    modeBuffer->Write = 1;
    RtlMoveMemory(modeBuffer->VariableName,"HNDLR_IO_CALL_KEY", 17);
    RtlMoveMemory(modeBuffer->VariableValue,"DISABLE", 7);

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);
    srb.CdbLength = CDB6GENERIC_LENGTH;
    srb.TimeOutValue = 20;
    srb.DataTransferLength = sizeof(CONFIG_MODE_PAGE);
    srb.DataBuffer = modeBuffer;

    cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
    cdb->MODE_SELECT.PFBit = 1;
    cdb->MODE_SELECT.ParameterListLength = (UCHAR)srb.DataTransferLength;
   
    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             &srb,
                                             modeBuffer,
                                             sizeof(CONFIG_MODE_PAGE),
                                             TRUE);
    ChangerClassFreePool(modeBuffer);

    // -----------------------------------------------------------
    // 3) Disable CALL key, only allow host control

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, 
                                   sizeof(CONFIG_MODE_PAGE));
    if (!modeBuffer) {
       return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, sizeof(CONFIG_MODE_PAGE));
    modeBuffer->PageCode = 0x3D;
    modeBuffer->PageLength = 0x28;
    modeBuffer->Length = 0;
    modeBuffer->Type = 0;
    modeBuffer->Write = 1;
    RtlMoveMemory(modeBuffer->VariableName,"HNDLR_IO_SLOT_ACCESS", 20);
    RtlMoveMemory(modeBuffer->VariableValue,"HOST", 4);

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);
    srb.CdbLength = CDB6GENERIC_LENGTH;
    srb.TimeOutValue = 20;
    srb.DataTransferLength = sizeof(CONFIG_MODE_PAGE);
    srb.DataBuffer = modeBuffer;

    cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
    cdb->MODE_SELECT.PFBit = 1;
    cdb->MODE_SELECT.ParameterListLength = (UCHAR)srb.DataTransferLength;
   
    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             &srb,
                                             modeBuffer,
                                             sizeof(CONFIG_MODE_PAGE),
                                             TRUE);
    ChangerClassFreePool(modeBuffer);

    // -----------------------------------------------------------

    return STATUS_SUCCESS;
}

VOID
ChangerError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )

/*++

Routine Description:

    This routine executes any device-specific error handling needed.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/
{

    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        switch (senseBuffer->SenseKey & 0xf) {

        case SCSI_SENSE_NOT_READY:

            if (senseBuffer->AdditionalSenseCode == 0x04) {
                switch (senseBuffer->AdditionalSenseCodeQualifier) {
                    case 0x83:

                        *Retry = FALSE;
                        *Status = STATUS_DEVICE_DOOR_OPEN;
                        break;
                }
            }

            break;

        case SCSI_SENSE_ILLEGAL_REQUEST:
            if (senseBuffer->AdditionalSenseCode == 0x80) {
                switch (senseBuffer->AdditionalSenseCodeQualifier) {
                    case 0x03:
                    case 0x04:

                        *Retry = FALSE;
                        *Status = STATUS_MAGAZINE_NOT_PRESENT;
                         break;
                    case 0x05:
                    case 0x06:
                        *Retry = TRUE;
                        *Status = STATUS_DEVICE_NOT_CONNECTED;
                        break;
                default:
                    break;
                }
            }
            break;

        case SCSI_SENSE_HARDWARE_ERROR: {
           DebugPrint((1, "Hardware Error - SenseCode %x, ASC %x, ASCQ %x\n",
                       senseBuffer->SenseKey,
                       senseBuffer->AdditionalSenseCode,
                       senseBuffer->AdditionalSenseCodeQualifier));
           changerData->HardwareError = TRUE;
           break;
        }

        default:
            break;
        }
    }


    return;
}

NTSTATUS
ChangerGetParameters(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine determines and returns the "drive parameters" of the
    qualstar changers.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION          fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA              changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING   addressMapping = &(changerData->AddressMapping);
    PSCSI_REQUEST_BLOCK        srb;
    PGET_CHANGER_PARAMETERS    changerParameters;
    PMODE_ELEMENT_ADDRESS_PAGE elementAddressPage;
    PMODE_TRANSPORT_GEOMETRY_PAGE transportGeometryPage;
    PMODE_DEVICE_CAPABILITIES_PAGE capabilitiesPage;
    NTSTATUS status;
    ULONG    length;
    PVOID    modeBuffer;
    PCDB     cdb;

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (srb == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    //
    // Build a mode sense - Element address assignment page.
    //

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(MODE_PARAMETER_HEADER)
                                + sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(modeBuffer, sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE);
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_ELEMENT_ADDRESS;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);

    if (!NT_SUCCESS(status)) {
        ChangerClassFreePool(srb);
        ChangerClassFreePool(modeBuffer);
        return status;
    }

    //
    // Fill in values.
    //

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(changerParameters, sizeof(GET_CHANGER_PARAMETERS));

    elementAddressPage = modeBuffer;
    (ULONG_PTR)elementAddressPage += sizeof(MODE_PARAMETER_HEADER);

    changerParameters->Size = sizeof(GET_CHANGER_PARAMETERS);
    changerParameters->NumberTransportElements = elementAddressPage->NumberTransportElements[1];
    changerParameters->NumberTransportElements |= (elementAddressPage->NumberTransportElements[0] << 8);

    changerParameters->NumberStorageElements = elementAddressPage->NumberStorageElements[1];
    changerParameters->NumberStorageElements |= (elementAddressPage->NumberStorageElements[0] << 8);

    changerParameters->NumberIEElements = elementAddressPage->NumberIEPortElements[1];
    changerParameters->NumberIEElements |= (elementAddressPage->NumberIEPortElements[0] << 8);

    changerParameters->NumberDataTransferElements = elementAddressPage->NumberDataXFerElements[1];
    changerParameters->NumberDataTransferElements |= (elementAddressPage->NumberDataXFerElements[0] << 8);
    changerParameters->NumberOfDoors = 1;

    // There can be many FIXED slots in the various models of these
    // libraries and none are identified as a cleaner. 
    // This unit also numbers its slots based on the
    // front panel setting. We will suggest setting 
    // the Configure\SCSI\StorageOrder to MAG making
    // slot 1..N order be A01, A02... B01, ... F1, F2... 
    changerParameters->NumberCleanerSlots = 0;

    changerParameters->FirstSlotNumber = 1;
    changerParameters->FirstDriveNumber =  1;
    changerParameters->FirstTransportNumber = 0;
    changerParameters->FirstIEPortNumber = 0;
    changerParameters->FirstCleanerSlotAddress = 0;

    if (changerData->DriveID == TLS_2xxx) {
        changerParameters->MagazineSize = 18;
    } else {
        changerParameters->MagazineSize = 10;
    }

    changerParameters->DriveCleanTimeout = 600;

    //
    // Free buffer.
    //

    ChangerClassFreePool(modeBuffer);

    //
    // build transport geometry mode sense.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(MODE_PARAMETER_HEADER)
                                + sizeof(MODE_PAGE_TRANSPORT_GEOMETRY));
    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(modeBuffer, sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_TRANSPORT_GEOMETRY_PAGE));
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_TRANSPORT_GEOMETRY_PAGE);
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_TRANSPORT_GEOMETRY;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);

    if (!NT_SUCCESS(status)) {
        ChangerClassFreePool(srb);
        ChangerClassFreePool(modeBuffer);
        return status;
    }

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    transportGeometryPage = modeBuffer;
    (ULONG_PTR)transportGeometryPage += sizeof(MODE_PARAMETER_HEADER);

        
    // initialize Features1  
    changerParameters->Features1 = 0;
        // Took away Exchange capability. The device supports the command
        // but a customer may load a medium into the exchange slot and 
        // cause an exchange to fail. Since the picker does not support 
        // holding two media, this device does not really support it. 
        // CHANGER_TRUE_EXCHANGE_CAPABLE;

    //
    // Determine if mc has 2-sided media.
    //

    changerParameters->Features0 = transportGeometryPage->Flip ? CHANGER_MEDIUM_FLIP : 0;



    //
    // Qualstar indicates whether a bar-code scanner is
    // attached by setting bit-0 in this byte.
    //

    changerParameters->Features0 |= ((changerData->InquiryData.VendorSpecific[19] & 0x1)) ?
                                         CHANGER_BAR_CODE_SCANNER_INSTALLED : 0;
    //
    // Features based on manual, nothing programatic.
    //

    changerParameters->Features0 |= CHANGER_INIT_ELEM_STAT_WITH_RANGE     |
                                    CHANGER_CARTRIDGE_MAGAZINE            |
                                    CHANGER_PREDISMOUNT_EJECT_REQUIRED    |
                                    CHANGER_DRIVE_CLEANING_REQUIRED;
 
    if (changerParameters->NumberIEElements != 0) {
        changerParameters->Features0 |= CHANGER_CLOSE_IEPORT                  |
                                        CHANGER_OPEN_IEPORT                   |
                                        CHANGER_LOCK_UNLOCK                   |
                                        CHANGER_REPORT_IEPORT_STATE; 

        changerParameters->LockUnlockCapabilities = LOCK_UNLOCK_IEPORT;
    }

    //
    // Free buffer.
    //

    ChangerClassFreePool(modeBuffer);

    //
    // build transport geometry mode sense.
    //


    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    //
    // Qualstar uses an addition 4 bytes...
    //

    length =  sizeof(MODE_PARAMETER_HEADER) +                            
              sizeof(MODE_DEVICE_CAPABILITIES_PAGE);

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, length);

    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, length);
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = length;
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CAPABILITIES;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);

    if (!NT_SUCCESS(status)) {
        ChangerClassFreePool(srb);
        ChangerClassFreePool(modeBuffer);
        return status;
    }

    //
    // Get the systembuffer and by-pass the mode header for the mode sense data.
    //

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    capabilitiesPage = modeBuffer;
    (ULONG_PTR)capabilitiesPage += sizeof(MODE_PARAMETER_HEADER);

    //
    // Fill in values in Features that are contained in this page.
    //

    changerParameters->Features0 |= capabilitiesPage->MediumTransport ? CHANGER_STORAGE_DRIVE : 0;
    changerParameters->Features0 |= capabilitiesPage->StorageLocation ? CHANGER_STORAGE_SLOT : 0;
    changerParameters->Features0 |= capabilitiesPage->IEPort ? CHANGER_STORAGE_IEPORT : 0;
    changerParameters->Features0 |= capabilitiesPage->DataXFer ? CHANGER_STORAGE_DRIVE : 0;

    //
    // Determine all the move from and exchange from capabilities of this device.
    //

    changerParameters->MoveFromTransport = capabilitiesPage->MTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromTransport |= capabilitiesPage->MTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromTransport |= capabilitiesPage->MTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromTransport |= capabilitiesPage->MTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromSlot = capabilitiesPage->STtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromSlot |= capabilitiesPage->STtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromSlot |= capabilitiesPage->STtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromSlot |= capabilitiesPage->STtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromIePort = capabilitiesPage->IEtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromIePort |= capabilitiesPage->IEtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromIePort |= capabilitiesPage->IEtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromIePort |= capabilitiesPage->IEtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromDrive = capabilitiesPage->DTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromDrive |= capabilitiesPage->DTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromDrive |= capabilitiesPage->DTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromDrive |= capabilitiesPage->DTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromTransport = 0;
    changerParameters->ExchangeFromSlot = 0;
    changerParameters->ExchangeFromIePort = 0;
    changerParameters->ExchangeFromDrive = 0;


        // legal Position capabilities... 
        changerParameters->PositionCapabilities = 0;
        
                
    ChangerClassFreePool(srb);
    ChangerClassFreePool(modeBuffer);

    Irp->IoStatus.Information = sizeof(GET_CHANGER_PARAMETERS);

    return STATUS_SUCCESS;
}



NTSTATUS
ChangerGetStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the status of the medium changer as determined through a TUR.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCDB     cdb;
    NTSTATUS status;

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    //
    // Build TUR.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB6GENERIC_LENGTH;
    cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
    srb->TimeOutValue = 20;

    //
    // Send SCSI command (CDB) to device
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);

    ChangerClassFreePool(srb);
    return status;
}



NTSTATUS
ChangerGetProductData(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns fields from the inquiry data useful for
    identifying the particular device.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_PRODUCT_DATA productData = Irp->AssociatedIrp.SystemBuffer;

    //
    // Copy cached inquiry data fields into the system buffer.
    //
    RtlZeroMemory(productData, sizeof(CHANGER_PRODUCT_DATA)); 

    RtlMoveMemory(productData->VendorId, changerData->InquiryData.VendorId, 
                  VENDOR_ID_LENGTH);

    RtlMoveMemory(productData->ProductId, changerData->InquiryData.ProductId, 
                  PRODUCT_ID_LENGTH);

    RtlMoveMemory(productData->Revision, changerData->InquiryData.ProductRevisionLevel,
                  REVISION_LENGTH);


    //
    // Indicate that this is a tape changer and that media isn't two-sided.
    //

    productData->DeviceType = MEDIUM_CHANGER;

    Irp->IoStatus.Information = sizeof(CHANGER_PRODUCT_DATA);
    return STATUS_SUCCESS;
}




NTSTATUS
ChangerSetAccess(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine sets the state of the door or IEPort. Value can be one of the
    following:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_SET_ACCESS setAccess = Irp->AssociatedIrp.SystemBuffer;
    ULONG               controlOperation = setAccess->Control;
    NTSTATUS            status = STATUS_SUCCESS;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;

    // should not be called by an app, but if it is, just return OK
    if ((setAccess->Element.ElementType == ChangerKeypad) || 
        (setAccess->Element.ElementType == ChangerDoor)) { 
                return STATUS_INVALID_DEVICE_REQUEST;
    }

    // if the unit has no IEPORT, fail the request
    if ((setAccess->Element.ElementType == ChangerIEPort) && 
        (changerData->AddressMapping.NumberOfElements[ChangerIEPort] == 0)) {
            return STATUS_INVALID_DEVICE_REQUEST;
    } 

    // Solenoid door locking is an option with this library
    // but do a prevent/allow in case it's present. 

    if (setAccess->Element.ElementType == ChangerIEPort)  {

        srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
        if (!srb) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
        cdb = (PCDB)srb->Cdb;

        if (controlOperation == LOCK_ELEMENT) {

            srb->CdbLength = CDB6GENERIC_LENGTH;
            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

            srb->DataTransferLength = 0;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            cdb->MEDIA_REMOVAL.Prevent = 1;
            cdb->MEDIA_REMOVAL.Control = 0;

        } else if (controlOperation == UNLOCK_ELEMENT) {

            srb->CdbLength = CDB6GENERIC_LENGTH;
            cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

            srb->DataTransferLength = 0;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            cdb->MEDIA_REMOVAL.Prevent = 0;
            cdb->MEDIA_REMOVAL.Control = 0;

        } else if (controlOperation == EXTEND_IEPORT) {

            srb->CdbLength = CDB12GENERIC_LENGTH;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            cdb->MOVE_MEDIUM.OperationCode = SCSIOP_MOVE_MEDIUM;

            cdb->MOVE_MEDIUM.TransportElementAddress[0] = 0;
            cdb->MOVE_MEDIUM.TransportElementAddress[1] = 0;

            cdb->MOVE_MEDIUM.SourceElementAddress[0] = 0;
            cdb->MOVE_MEDIUM.SourceElementAddress[1] = 0;

            cdb->MOVE_MEDIUM.DestinationElementAddress[0] = 0;
            cdb->MOVE_MEDIUM.DestinationElementAddress[1] = 0;

            cdb->MOVE_MEDIUM.Flip = 0;

            // Indicate that the IEPORT should be extended.
            cdb->MOVE_MEDIUM.Control = 0x40;

            srb->DataTransferLength = 0;

        } else if (controlOperation == RETRACT_IEPORT) {
            srb->CdbLength = CDB12GENERIC_LENGTH;
            srb->TimeOutValue = fdoExtension->TimeOutValue;

            cdb->MOVE_MEDIUM.OperationCode = SCSIOP_MOVE_MEDIUM;

            cdb->MOVE_MEDIUM.TransportElementAddress[0] = 0;
            cdb->MOVE_MEDIUM.TransportElementAddress[1] = 0;

            cdb->MOVE_MEDIUM.SourceElementAddress[0] = 0;
            cdb->MOVE_MEDIUM.SourceElementAddress[1] = 0;

            cdb->MOVE_MEDIUM.DestinationElementAddress[0] = 0;
            cdb->MOVE_MEDIUM.DestinationElementAddress[1] = 0;

            cdb->MOVE_MEDIUM.Flip = 0;

            // indicate that the IEPORT must be retracted
            cdb->MOVE_MEDIUM.Control = 0x80;

            srb->DataTransferLength = 0;

         } else {
             status = STATUS_INVALID_PARAMETER;
         }
    } else { // Illegal Element Type
        return STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(status)) {

        status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             NULL,
                                             0,
                                             FALSE);
    }

    ChangerClassFreePool(srb);
    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_SET_ACCESS);
    }

    return status;
}




NTSTATUS
ChangerGetElementStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine builds and issues a read element status command for either all elements or the
    specified element type. The buffer returned is used to build the user buffer.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA     changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING     addressMapping = &(changerData->AddressMapping);
    PCHANGER_READ_ELEMENT_STATUS readElementStatus = Irp->AssociatedIrp.SystemBuffer;
    PCHANGER_ELEMENT_STATUS      elementStatus;
    PCHANGER_ELEMENT    element;
    PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);
    ELEMENT_TYPE        elementType;
    PSCSI_REQUEST_BLOCK srb;
    PCDB     cdb;
    ULONG    length;
    ULONG    statusPages;
    ULONG    outputBuffLen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG    totalElements = readElementStatus->ElementList.NumberOfElements;
    NTSTATUS status;
    PVOID    statusBuffer;

    //
    // Determine the element type.
    //

    elementType = readElementStatus->ElementList.Element.ElementType;
    element = &readElementStatus->ElementList.Element;

    //
    // length will be based on whether vol. tags are returned and element type(s).
    //

    if (elementType == AllElements) {

        //
        // There should be 4 status pages in the returned buffer.
        //
        statusPages = 4;
    } else {
        statusPages = 1;
    }

    if ((readElementStatus->VolumeTagInfo) ||
        (elementType == ChangerDrive)) {

        //
        // Each descriptor will have an embedded volume tag buffer.
        //

        length = sizeof(ELEMENT_STATUS_HEADER) + 
                         (statusPages * sizeof(ELEMENT_STATUS_PAGE)) +
                 (QUAL_FULL_SIZE * readElementStatus->ElementList.NumberOfElements);
    } else {

        length = sizeof(ELEMENT_STATUS_HEADER) + 
                       (statusPages * sizeof(ELEMENT_STATUS_PAGE)) +
               (QUAL_PARTIAL_SIZE * readElementStatus->ElementList.NumberOfElements);

    }

    statusBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, length);

    if (!statusBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {
       ChangerClassFreePool(statusBuffer);
       return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(statusBuffer, length);

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->DataBuffer = statusBuffer;
    srb->DataTransferLength = length;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    cdb->READ_ELEMENT_STATUS.OperationCode = SCSIOP_READ_ELEMENT_STATUS;
    cdb->READ_ELEMENT_STATUS.ElementType = (UCHAR)elementType;
    cdb->READ_ELEMENT_STATUS.VolTag = readElementStatus->VolumeTagInfo;

    //
    // Fill in element addressing info based on the mapping values.
    //

    if (elementType == AllElements) {
       cdb->READ_ELEMENT_STATUS.StartingElementAddress[0] = 0;
       cdb->READ_ELEMENT_STATUS.StartingElementAddress[1] = 0;
    } else {
       cdb->READ_ELEMENT_STATUS.StartingElementAddress[0] =
           (UCHAR)((element->ElementAddress + addressMapping->FirstElement[element->ElementType]) >> 8);
   
       cdb->READ_ELEMENT_STATUS.StartingElementAddress[1] =
           (UCHAR)((element->ElementAddress + addressMapping->FirstElement[element->ElementType]) & 0xFF);
    }


    cdb->READ_ELEMENT_STATUS.NumberOfElements[0] = (UCHAR)(readElementStatus->ElementList.NumberOfElements >> 8);
    cdb->READ_ELEMENT_STATUS.NumberOfElements[1] = (UCHAR)(readElementStatus->ElementList.NumberOfElements & 0xFF);

    cdb->READ_ELEMENT_STATUS.AllocationLength[0] = (UCHAR)(length >> 16);
    cdb->READ_ELEMENT_STATUS.AllocationLength[1] = (UCHAR)(length >> 8);
    cdb->READ_ELEMENT_STATUS.AllocationLength[2] = (UCHAR)(length & 0xFF);

    //
    // Send SCSI command (CDB) to device
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);

    if ((NT_SUCCESS(status)) || 
          (status == STATUS_DATA_OVERRUN)) {

        PELEMENT_STATUS_HEADER statusHeader = statusBuffer;
        PELEMENT_STATUS_PAGE statusPage;
        PQUAL_ELEMENT_DESCRIPTOR elementDescriptor;
        ULONG numberElements = readElementStatus->ElementList.NumberOfElements;
        LONG remainingElements;
        LONG typeCount;
        BOOLEAN tagInfo = readElementStatus->VolumeTagInfo;
        LONG i;
        ULONG descriptorLength;

        //
        // If the status was STATUS_DATA_OVERRUN, check
        // if it was really DATA_OVERRUN or was it just
        // DATA_UNDERRUN
        //
        if (status == STATUS_DATA_OVERRUN) {
           if (srb->DataTransferLength < length) {
              DebugPrint((1, "Data Underrun reported as overrun.\n"));
              status = STATUS_SUCCESS;
           } else {
              DebugPrint((1, "Data Overrun in ChangerGetElementStatus.\n"));

              ChangerClassFreePool(srb);
              ChangerClassFreePool(statusBuffer);

              return status;
           }
        }

        //
        // Determine total number elements returned.
        //

        remainingElements = statusHeader->NumberOfElements[1];
        remainingElements |= (statusHeader->NumberOfElements[0] << 8);

        //
        // The buffer is composed of a header, status page, and element descriptors.
        // Point each element to it's respective place in the buffer.
        //

        (ULONG_PTR)statusPage = (ULONG_PTR)statusHeader;
        (ULONG_PTR)statusPage += sizeof(ELEMENT_STATUS_HEADER);

        elementType = statusPage->ElementType;

        (ULONG_PTR)elementDescriptor = (ULONG_PTR)statusPage;
        (ULONG_PTR)elementDescriptor += sizeof(ELEMENT_STATUS_PAGE);

        descriptorLength = statusPage->ElementDescriptorLength[1];
        descriptorLength |= (statusPage->ElementDescriptorLength[0] << 8);

        //
        // Determine the number of elements of this type reported.
        //

        typeCount =  statusPage->DescriptorByteCount[2];
        typeCount |=  (statusPage->DescriptorByteCount[1] << 8);
        typeCount |=  (statusPage->DescriptorByteCount[0] << 16);

        if (descriptorLength > 0) {
            typeCount /= descriptorLength;
        } else {
            typeCount = 0;
        }

        if ((typeCount == 0) &&
            (remainingElements > 0)) {
            --remainingElements;
        }

        //
        // Fill in user buffer.
        //

        elementStatus = Irp->AssociatedIrp.SystemBuffer;
        RtlZeroMemory(elementStatus, outputBuffLen);

        do {

            for (i = 0; i < typeCount; i++, remainingElements--) {

                //
                // Get the address for this element.
                //

                elementStatus->Element.ElementAddress =
                    elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.ElementAddress[1];
                elementStatus->Element.ElementAddress |=
                    (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.ElementAddress[0] << 8);

                //
                // Account for address mapping.
                //

                elementStatus->Element.ElementAddress -= addressMapping->FirstElement[elementType];

                //
                // Set the element type.
                //

                elementStatus->Element.ElementType = elementType;


                if (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.SValid) {

                    ULONG  j;
                    USHORT tmpAddress;


                    //
                    // Source address is valid. Determine the device specific address.
                    //

                    tmpAddress = elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.SourceStorageElementAddress[1];
                    tmpAddress |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.SourceStorageElementAddress[0] << 8);

                    //
                    // Now convert to 0-based values.
                    //

                    for (j = 1; j <= ChangerDrive; j++) {
                        if (addressMapping->FirstElement[j] <= tmpAddress) {
                            if (tmpAddress < (addressMapping->NumberOfElements[j] + addressMapping->FirstElement[j])) {
                                elementStatus->SrcElementAddress.ElementType = j;
                                break;
                            }
                        }
                    }

                    elementStatus->SrcElementAddress.ElementAddress = tmpAddress - addressMapping->FirstElement[j];

                }

                //
                // Build Flags field.
                //

                elementStatus->Flags = elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.Full;
                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.Exception << 2);
                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.Accessible << 3);

                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.LunValid << 12);
                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.IdValid << 13);
                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.NotThisBus << 15);

                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.Invert << 22);
                elementStatus->Flags |= (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.SValid << 23);


                elementStatus->ExceptionCode = MapExceptionCodes(elementDescriptor);

                if (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.IdValid) {
                    elementStatus->TargetId = elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.BusAddress;
                }
                if (elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.LunValid) {
                    elementStatus->Lun = elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.Lun;
                }

                if (tagInfo) {
                    RtlMoveMemory(elementStatus->PrimaryVolumeID, 
                                  elementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.VolumeTagDeviceID.VolumeTagInformation, 
                                  MAX_VOLUME_ID_SIZE);
                    elementStatus->Flags |= ELEMENT_STATUS_PVOLTAG;
                }

                if (elementType == ChangerDrive) {
                    if (outputBuffLen >=
                        (totalElements * sizeof(CHANGER_ELEMENT_STATUS_EX))) {
                    
                        PCHANGER_ELEMENT_STATUS_EX elementStatusEx;
                        PQUAL_FULL_ELEMENT_DESCRIPTOR elementDescPlus =
                            (PQUAL_FULL_ELEMENT_DESCRIPTOR) elementDescriptor;
                        PUCHAR idField = NULL;
                        ULONG inx;
                    
                        elementStatusEx = (PCHANGER_ELEMENT_STATUS_EX)elementStatus;
                    
                        if (statusPage->PVolTag) {
                            idField =  elementDescPlus->VolumeTagDeviceID.SerialNumber;
                        } else {
                            idField = elementDescPlus->DeviceID.SerialNumber;
                        }
                    
                        if ((*idField != '\0') && (*idField != ' ')) {
                    
                            RtlMoveMemory(elementStatusEx->SerialNumber,
                                          idField,
                                          SERIAL_NUMBER_LENGTH);
                    
                            DebugPrint((3, "Serial number : %s\n",
                                        elementStatusEx->SerialNumber));

                            elementStatusEx->Flags |= ELEMENT_STATUS_PRODUCT_DATA;
                        } 
                    }
                }

                //
                // Get next descriptor.
                //

                (ULONG_PTR)elementDescriptor += descriptorLength;

                //
                // Advance to the next entry in the user buffer and element descriptor array.
                //
                if (outputBuffLen >=
                    (totalElements * sizeof(CHANGER_ELEMENT_STATUS_EX))) {
                    (PUCHAR) elementStatus += sizeof(CHANGER_ELEMENT_STATUS_EX);
                } else {

                    elementStatus += 1;
                }

            }

            if (remainingElements > 0) {

                //
                // Get next status page.
                //

                (ULONG_PTR)statusPage = (ULONG_PTR)elementDescriptor;

                elementType = statusPage->ElementType;

                //
                // Point to decriptors.
                //

                (ULONG_PTR)elementDescriptor = (ULONG_PTR)statusPage;
                (ULONG_PTR)elementDescriptor += sizeof(ELEMENT_STATUS_PAGE);

                descriptorLength = statusPage->ElementDescriptorLength[1];
                descriptorLength |= (statusPage->ElementDescriptorLength[0] << 8);

                //
                // Determine the number of this element type reported.
                //

                typeCount =  statusPage->DescriptorByteCount[2];
                typeCount |=  (statusPage->DescriptorByteCount[1] << 8);
                typeCount |=  (statusPage->DescriptorByteCount[0] << 16);

                if (descriptorLength > 0) {
                    typeCount /= descriptorLength;
                } else {
                    typeCount = 0;
                }
        
                if ((typeCount == 0) &&
                    (remainingElements > 0)) {
                    --remainingElements;
                }
            }

        } while (remainingElements);

        if (outputBuffLen >= 
            (totalElements * sizeof(CHANGER_ELEMENT_STATUS_EX))) {

            Irp->IoStatus.Information = sizeof(CHANGER_ELEMENT_STATUS_EX) *
                                         numberElements;
        } else {
        
            Irp->IoStatus.Information = sizeof(CHANGER_ELEMENT_STATUS) * numberElements;
        }

    } 

    ChangerClassFreePool(srb);
    ChangerClassFreePool(statusBuffer);

    return status;
}



NTSTATUS
ChangerInitializeElementStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine issues the necessary command to either initialize all elements
    or the specified range of elements using the normal SCSI-2 command, or a vendor-unique
    range command.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{

    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_INITIALIZE_ELEMENT_STATUS initElementStatus = Irp->AssociatedIrp.SystemBuffer;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    if (initElementStatus->ElementList.Element.ElementType == AllElements) {

        //
        // Build the normal SCSI-2 command for all elements.
        //

        srb->CdbLength = CDB6GENERIC_LENGTH;
        cdb->INIT_ELEMENT_STATUS.OperationCode = SCSIOP_INIT_ELEMENT_STATUS;
        cdb->INIT_ELEMENT_STATUS.NoBarCode = initElementStatus->BarCodeScan ? 0 : 1;


        srb->TimeOutValue = fdoExtension->TimeOutValue;
        srb->DataTransferLength = 0;

    } else {

        PCHANGER_ELEMENT_LIST elementList = &initElementStatus->ElementList;
        PCHANGER_ELEMENT element = &elementList->Element;

        //
        // Use the vendor-unique initialize with range command
        //

        srb->CdbLength = CDB10GENERIC_LENGTH;
        cdb->INITIALIZE_ELEMENT_RANGE.OperationCode = SCSIOP_INIT_ELEMENT_RANGE;
        cdb->INITIALIZE_ELEMENT_RANGE.Range = 1;

        //
        // Addresses of elements need to be mapped from 0-based to device-specific.
        //

        cdb->INITIALIZE_ELEMENT_RANGE.FirstElementAddress[0] =
            (UCHAR)((element->ElementAddress + addressMapping->FirstElement[element->ElementType]) >> 8);
        cdb->INITIALIZE_ELEMENT_RANGE.FirstElementAddress[1] =
            (UCHAR)((element->ElementAddress + addressMapping->FirstElement[element->ElementType]) & 0xFF);

        cdb->INITIALIZE_ELEMENT_RANGE.NumberOfElements[0] = (UCHAR)(elementList->NumberOfElements >> 8);
        cdb->INITIALIZE_ELEMENT_RANGE.NumberOfElements[1] = (UCHAR)(elementList->NumberOfElements & 0xFF);

        //
        // Indicate whether to use bar code scanning.
        //

        cdb->INITIALIZE_ELEMENT_RANGE.NoBarCode = initElementStatus->BarCodeScan ? 0 : 1;

        srb->TimeOutValue = fdoExtension->TimeOutValue;
        srb->DataTransferLength = 0;

    }

    //
    // Send SCSI command (CDB) to device
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_INITIALIZE_ELEMENT_STATUS);
    }

    ChangerClassFreePool(srb);
    return status;
}



NTSTATUS
ChangerSetPosition(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine issues the appropriate command to set the robotic mechanism to the specified
    element address. Normally used to optimize moves or exchanges by pre-positioning the picker.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_SET_POSITION setPosition = Irp->AssociatedIrp.SystemBuffer;
    USHORT transport, maxTransport;
    USHORT destination, maxDest;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    return STATUS_INVALID_DEVICE_REQUEST;
/*

    if ((setPosition->Destination.ElementType == ChangerDoor) || 
        (setPosition->Destination.ElementType == ChangerKeypad) ||
        (setPosition->Destination.ElementType == ChangerTransport)) { 
        return STATUS_ILLEGAL_ELEMENT_ADDRESS;       
    } 

    //
    // Verify transport, source, and dest. are within range.
    // Convert from 0-based to device-specific addressing.
    //

    transport = (USHORT)(setPosition->Transport.ElementAddress);

    if (ElementOutOfRange(addressMapping, transport, ChangerTransport)) {

        DebugPrint((1,
                   "ChangerSetPosition: Transport element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    destination = (USHORT)(setPosition->Destination.ElementAddress);

    if (ElementOutOfRange(addressMapping, destination, setPosition->Destination.ElementType)) {
        DebugPrint((1,
                   "ChangerSetPosition: Destination element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }


    //
    // This library doesn't support 2-sided media.
    //

    if (setPosition->Flip) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB10GENERIC_LENGTH;
    cdb->POSITION_TO_ELEMENT.OperationCode = SCSIOP_POSITION_TO_ELEMENT;

    //
    // Build device-specific addressing.
    //

    cdb->POSITION_TO_ELEMENT.TransportElementAddress[0] = (UCHAR)(transport >> 8);
    cdb->POSITION_TO_ELEMENT.TransportElementAddress[1] = (UCHAR)(transport & 0xFF);

    cdb->POSITION_TO_ELEMENT.DestinationElementAddress[0] = (UCHAR)(destination >> 8);
    cdb->POSITION_TO_ELEMENT.DestinationElementAddress[1] = (UCHAR)(destination & 0xFF);

    //
    // Doesn't support two-sided media, but as a ref. source base, it should be noted.
    //

    cdb->POSITION_TO_ELEMENT.Flip = setPosition->Flip;


    srb->DataTransferLength = 0;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    //
    // Send SCSI command (CDB) to device
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         TRUE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_SET_POSITION);
    }

    ChangerClassFreePool(srb);
    return status;
*/
}



NTSTATUS
ChangerExchangeMedium(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    These library units support the exchange medium command but it has
    limitations since it is not a dual picker mechanism. Therefore, 
    this device will return STATUS_INVALID_DEVICE_REQUEST. 

Arguments:

    DeviceObject
    Irp

Return Value:

--*/

{
    return STATUS_INVALID_DEVICE_REQUEST;
}



NTSTATUS
ChangerMoveMedium(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/


{
    PFUNCTIONAL_DEVICE_EXTENSION   fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA       changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &(changerData->AddressMapping);
    PCHANGER_MOVE_MEDIUM moveMedium = Irp->AssociatedIrp.SystemBuffer;
    USHORT transport, maxTransport;
    USHORT source, maxSource;
    USHORT destination, maxDest;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    //
    // Verify transport, source, and dest. are within range.
    // Convert from 0-based to device-specific addressing.
    //

    transport = (USHORT)(moveMedium->Transport.ElementAddress);

    if (ElementOutOfRange(addressMapping, transport, ChangerTransport)) {

        DebugPrint((1,
                   "ChangerMoveMedium: Transport element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    source = (USHORT)(moveMedium->Source.ElementAddress);

    if (ElementOutOfRange(addressMapping, source, moveMedium->Source.ElementType)) {

        DebugPrint((1,
                   "ChangerMoveMedium: Source element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    destination = (USHORT)(moveMedium->Destination.ElementAddress);

    if (ElementOutOfRange(addressMapping, destination, moveMedium->Destination.ElementType)) {
        DebugPrint((1,
                   "ChangerMoveMedium: Destination element out of range.\n"));

        return STATUS_ILLEGAL_ELEMENT_ADDRESS;
    }

    //
    // Convert to device addresses.
    //

    transport += addressMapping->FirstElement[ChangerTransport];
    source += addressMapping->FirstElement[moveMedium->Source.ElementType];
    destination += addressMapping->FirstElement[moveMedium->Destination.ElementType];


    //
    // This library doesn't support 2-sided media.
    //

    if (moveMedium->Flip) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (!srb) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;
    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    cdb->MOVE_MEDIUM.OperationCode = SCSIOP_MOVE_MEDIUM;

    //
    // Build addressing values based on address map.
    //

    cdb->MOVE_MEDIUM.TransportElementAddress[0] = (UCHAR)(transport >> 8);
    cdb->MOVE_MEDIUM.TransportElementAddress[1] = (UCHAR)(transport & 0xFF);

    cdb->MOVE_MEDIUM.SourceElementAddress[0] = (UCHAR)(source >> 8);
    cdb->MOVE_MEDIUM.SourceElementAddress[1] = (UCHAR)(source & 0xFF);

    cdb->MOVE_MEDIUM.DestinationElementAddress[0] = (UCHAR)(destination >> 8);
    cdb->MOVE_MEDIUM.DestinationElementAddress[1] = (UCHAR)(destination & 0xFF);

    cdb->MOVE_MEDIUM.Flip = moveMedium->Flip;

    srb->DataTransferLength = 0;

    //
    // Send SCSI command (CDB) to device
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_MOVE_MEDIUM);
    }

    ChangerClassFreePool(srb);
    return status;
}



NTSTATUS
ChangerReinitializeUnit(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    // this device does not support a Rezero Unit Command
    return STATUS_INVALID_DEVICE_REQUEST;
}



NTSTATUS
ChangerQueryVolumeTags(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:


Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    return STATUS_INVALID_DEVICE_REQUEST;
}



NTSTATUS
ExaBuildAddressMapping(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine issues the appropriate mode sense commands and builds an
    array of element addresses. These are used to translate between the device-specific
    addresses and the zero-based addresses of the API.

Arguments:

    DeviceObject

Return Value:

    NTSTATUS

--*/
{

    PFUNCTIONAL_DEVICE_EXTENSION      fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA          changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING addressMapping = &changerData->AddressMapping;
    PSCSI_REQUEST_BLOCK    srb;
    PCDB                   cdb;
    NTSTATUS               status;
    PMODE_ELEMENT_ADDRESS_PAGE elementAddressPage;
    PVOID modeBuffer;
    ULONG i;

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    //
    // Set all FirstElements to NO_ELEMENT.
    //

    for (i = 0; i < ChangerMaxElement; i++) {
        addressMapping->FirstElement[i] = QLS_NO_ELEMENT;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb->Cdb;

    //
    // Build a mode sense - Element address assignment page.
    //

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(MODE_PARAMETER_HEADER)
                                + sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    if (!modeBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(modeBuffer, sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ELEMENT_ADDRESS_PAGE);
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_ELEMENT_ADDRESS;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    //
    // Send the request.
    //

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);


    elementAddressPage = modeBuffer;
    (ULONG_PTR)elementAddressPage += sizeof(MODE_PARAMETER_HEADER);

    if (NT_SUCCESS(status)) {

        //
        // Build address mapping.
        //

        addressMapping->FirstElement[ChangerTransport] = (elementAddressPage->MediumTransportElementAddress[0] << 8) |
                                                          elementAddressPage->MediumTransportElementAddress[1];
        addressMapping->FirstElement[ChangerDrive] = (elementAddressPage->FirstDataXFerElementAddress[0] << 8) |
                                                      elementAddressPage->FirstDataXFerElementAddress[1];
        addressMapping->FirstElement[ChangerIEPort] = (elementAddressPage->FirstIEPortElementAddress[0] << 8) |
                                                       elementAddressPage->FirstIEPortElementAddress[1];
        addressMapping->FirstElement[ChangerSlot] = (elementAddressPage->FirstStorageElementAddress[0] << 8) |
                                                     elementAddressPage->FirstStorageElementAddress[1];
        addressMapping->FirstElement[ChangerDoor] = 0;

        addressMapping->FirstElement[ChangerKeypad] = 0;

        addressMapping->NumberOfElements[ChangerTransport] = elementAddressPage->NumberTransportElements[1];
        addressMapping->NumberOfElements[ChangerTransport] |= (elementAddressPage->NumberTransportElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDrive] = elementAddressPage->NumberDataXFerElements[1];
        addressMapping->NumberOfElements[ChangerDrive] |= (elementAddressPage->NumberDataXFerElements[0] << 8);

        addressMapping->NumberOfElements[ChangerIEPort] = elementAddressPage->NumberIEPortElements[1];
        addressMapping->NumberOfElements[ChangerIEPort] |= (elementAddressPage->NumberIEPortElements[0] << 8);

        addressMapping->NumberOfElements[ChangerSlot] = elementAddressPage->NumberStorageElements[1];
        addressMapping->NumberOfElements[ChangerSlot] |= (elementAddressPage->NumberStorageElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDoor] = 1;
        addressMapping->NumberOfElements[ChangerKeypad] = 0;

        addressMapping->Initialized = TRUE;
    }


    //
    // Free buffer.
    //

    ChangerClassFreePool(modeBuffer);
    ChangerClassFreePool(srb);

    return status;
}



ULONG
MapExceptionCodes(
    IN PQUAL_ELEMENT_DESCRIPTOR ElementDescriptor
    )

/*++

Routine Description:

    This routine takes the sense data from the elementDescriptor and creates
    the appropriate bitmap of values.

Arguments:

   ElementDescriptor - pointer to the descriptor page.

Return Value:

    Bit-map of exception codes.

--*/

{
    UCHAR asq = ElementDescriptor->QUAL_FULL_ELEMENT_DESCRIPTOR.AddSenseCodeQualifier;
    ULONG exceptionCode;

    //
    // On this library, the additional sense code is always 0x83.
    //

    switch (asq) {
        case 0x0:
            exceptionCode = ERROR_LABEL_QUESTIONABLE;
            break;

        case 0x1:
            exceptionCode = ERROR_LABEL_UNREADABLE;
            break;

        case 0x2:
            exceptionCode = ERROR_SLOT_NOT_PRESENT;
            break;

        case 0x3:
            exceptionCode = ERROR_LABEL_QUESTIONABLE;
            break;


        case 0x4:
            exceptionCode = ERROR_DRIVE_NOT_INSTALLED;
            break;

        case 0x8:
        case 0x9:
        case 0xA:
            exceptionCode = ERROR_LABEL_UNREADABLE;
            break;

        default:
            exceptionCode = ERROR_UNHANDLED_ERROR;
    }

    return exceptionCode;

}




BOOLEAN
ElementOutOfRange(
    IN PCHANGER_ADDRESS_MAPPING AddressMap,
    IN USHORT ElementOrdinal,
    IN ELEMENT_TYPE ElementType
    )
/*++

Routine Description:

    This routine determines whether the element address passed in is within legal range for
    the device.

Arguments:

    AddressMap - The dds' address map array
    ElementOrdinal - Zero-based address of the element to check.
    ElementType

Return Value:

    TRUE if out of range

--*/
{

    if (ElementOrdinal >= AddressMap->NumberOfElements[ElementType]) {

        DebugPrint((1,
                   "ElementOutOfRange: Type %x, Ordinal %x, Max %x\n",
                   ElementType,
                   ElementOrdinal,
                   AddressMap->NumberOfElements[ElementType]));
        return TRUE;
    } else if (AddressMap->FirstElement[ElementType] == QLS_NO_ELEMENT) {

        DebugPrint((1,
                   "ElementOutOfRange: No Type %x present\n",
                   ElementType));

        return TRUE;
    }

    return FALSE;
}


NTSTATUS
ChangerPerformDiagnostics(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PWMI_CHANGER_PROBLEM_DEVICE_ERROR changerDeviceError
    )
/*+++

Routine Description :

   This routine performs diagnostics tests on the changer
   to determine if the device is working fine or not. If
   it detects any problem the fields in the output buffer
   are set appropriately.

Arguments :

   DeviceObject         -   Changer device object
   changerDeviceError   -   Buffer in which the diagnostic information
                            is returned.
Return Value :

   NTStatus
--*/
{

   PSCSI_REQUEST_BLOCK srb;
   PCDB                cdb;
   NTSTATUS            status;
   PCHANGER_DATA       changerData;
   PFUNCTIONAL_DEVICE_EXTENSION fdoExtension;
   CHANGER_DEVICE_PROBLEM_TYPE changerProblemType;
   ULONG changerId;
   PUCHAR  resultBuffer;
   ULONG length;

   fdoExtension = DeviceObject->DeviceExtension;
   changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
   
   //
   // Initialize the flag in the device extension to FALSE.
   // If the changer returns sense code SCSI_SENSE_HARDWARE_ERROR
   // on SelfTest, we'll set this flag to TRUE in ChangerError routine.
   //
   changerData->HardwareError = FALSE;

   changerDeviceError->ChangerProblemType = DeviceProblemNone;

   srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

   if (srb == NULL) {
      DebugPrint((1, "Qlstrmc\\ChangerPerformDiagnostics : No memory\n"));
      return STATUS_INSUFFICIENT_RESOURCES;
   }

   RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
   cdb = (PCDB)srb->Cdb;

   //
   // Set the SRB for Send Diagnostic command
   //
   srb->CdbLength = CDB6GENERIC_LENGTH;
   srb->TimeOutValue = 600;

   cdb->CDB6GENERIC.OperationCode = SCSIOP_SEND_DIAGNOSTIC;

   //
   // Set SelfTest bit in the CDB
   //
   cdb->CDB6GENERIC.CommandUniqueBits = 0x2;

   status =  ChangerClassSendSrbSynchronous(DeviceObject,
                                            srb,
                                            srb->DataBuffer,
                                            srb->DataTransferLength,
                                            FALSE);
   if (NT_SUCCESS(status)) {
      changerDeviceError->ChangerProblemType = DeviceProblemNone;
   } else if ((changerData->HardwareError) == TRUE) {
      changerDeviceError->ChangerProblemType = DeviceProblemHardware;
   }

   ChangerClassFreePool(srb);

   return status;
}

