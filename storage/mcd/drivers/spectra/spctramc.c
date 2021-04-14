
/*++

Copyright (C) Microsoft Corporation, 1996 - 1999

Module Name:

    spctramc.c

Abstract:

    This module contains device-specific routines for the following
    Spectralogic medium changers: 
            - Spectra 4000, 5000, 9000, 10000

Author:

    davet (Dave Therrien - HighGround Systems)

Environment:

    kernel mode only

Revision History:


--*/

#include "ntddk.h"
#include "mcd.h"
#include "spctramc.h"

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
    needed by the changers.

Arguments:


Return Value:

    Size, in bytes.

--*/

{

    return sizeof(CHANGER_DATA);
}


typedef struct _SERIALNUMBER {
    UCHAR DeviceType;
    UCHAR PageCode;
    UCHAR Reserved;
    UCHAR PageLength;
    UCHAR SerialNumber[VPD_SERIAL_NUMBER_LENGTH];
} SERIALNUMBER, *PSERIALNUMBER;



NTSTATUS
ChangerInitialize(
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA  changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    NTSTATUS       status;
    PINQUIRYDATA   dataBuffer;
    PSERIALNUMBER  serialBuffer;
    PVUPL_MODE_PAGE modeBuffer;
    PCDB           cdb;
    ULONG          length;
    SCSI_REQUEST_BLOCK srb;

    changerData->Size = sizeof(CHANGER_DATA);

    //
    // Build address mapping.
    //

    status = ExaBuildAddressMapping(DeviceObject);
    if (!NT_SUCCESS(status)) {
        DebugPrint((1,
                    "BuildAddressMapping failed. %x\n", status));
        // Don't fail this here , this unit has a problem with 
        // being not ready for a long time 

        // return status;
    }

    //
    // Get inquiry data.
    //

    dataBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned,
                                                sizeof(INQUIRYDATA));
    if (!dataBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Now get the full inquiry information for the device.

    RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);
    srb.TimeOutValue = 10;
    srb.CdbLength = 6;

    cdb = (PCDB)srb.Cdb;
    cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
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

        length = dataBuffer->AdditionalLength +
                             FIELD_OFFSET(INQUIRYDATA, Reserved);

        if (length > srb.DataTransferLength) {
            length = srb.DataTransferLength;
        }

        RtlMoveMemory(&changerData->InquiryData, dataBuffer, length);

        if (RtlCompareMemory(dataBuffer->ProductId,"4000",4) == 4) {
            changerData->DriveID = S_4mm_4000;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"5000",4) == 4) {
            changerData->DriveID = S_4mm_5000;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"9000",4) == 4) {
            changerData->DriveID = S_8mm_EXB;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"10000",5) == 5) {
            changerData->DriveID = S_8mm_SONY;
        } else if (RtlCompareMemory(dataBuffer->ProductId, "215", 3)) {
            changerData->DriveID = S_8mm_AIT;
        }
    }

    // -------------------------------------------------------------------
    // 
    // Get serial number page for the 10000 and Treefrog units only
    // not supported on 4000, 5000 and 9000 units !

    if (changerData->DriveID == S_8mm_SONY) {

        serialBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(SERIALNUMBER));
        if (!serialBuffer) {
            ChangerClassFreePool(dataBuffer);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(serialBuffer, sizeof(SERIALNUMBER));

        RtlZeroMemory(&srb, SCSI_REQUEST_BLOCK_SIZE);
        srb.TimeOutValue = 10;
        srb.CdbLength = 6;

        cdb = (PCDB)srb.Cdb;
        cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;

        // Set EVPD
        cdb->CDB6INQUIRY.Reserved1 = 1;

        // Unit serial number page.
        cdb->CDB6INQUIRY.PageCode = 0x80;

        // Set allocation length to inquiry data buffer size.
        cdb->CDB6INQUIRY.AllocationLength = sizeof(SERIALNUMBER);

        status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         &srb,
                                         serialBuffer,
                                         sizeof(SERIALNUMBER),
                                         FALSE);

        if (SRB_STATUS(srb.SrbStatus) == SRB_STATUS_SUCCESS ||
            SRB_STATUS(srb.SrbStatus) == SRB_STATUS_DATA_OVERRUN) {

            RtlMoveMemory(changerData->SerialNumber, serialBuffer->SerialNumber, 
                          VPD_SERIAL_NUMBER_LENGTH);

            ChangerClassFreePool(serialBuffer);
        }
    }

    ChangerClassFreePool(dataBuffer);

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
    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension = DeviceObject->DeviceExtension;
    PCHANGER_DATA  changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);

    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;
    PIRP irp = Srb->OriginalRequest;


    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        switch (senseBuffer->SenseKey & 0xf) {

        case SCSI_SENSE_NOT_READY:
           if (senseBuffer->AdditionalSenseCode == 0x04) {
                switch (senseBuffer->AdditionalSenseCodeQualifier) {
                    case 0x84:
                        *Retry = FALSE;
                        *Status = STATUS_DEVICE_DOOR_OPEN;
                        break;
                    default:
                        break;
                }
            }
            break;

        case SCSI_SENSE_HARDWARE_ERROR:
           changerData->DeviceStatus = SPECTRA_HW_ERROR;
           if (senseBuffer->AdditionalSenseCode == 0x85) {
                switch (senseBuffer->AdditionalSenseCodeQualifier) {
                    case 0x23:
                        *Retry = FALSE;
                        *Status = STATUS_DEVICE_NOT_CONNECTED;
                        break;
                    default:
                        break;
                }
            }
            break;

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
    changers.

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
    PVUPL_MODE_PAGE vuplModePage;
    NTSTATUS status;
    ULONG    length;
    PVOID    modeBuffer;
    PCDB     cdb;

    if (addressMapping->Initialized != TRUE) {
        status = ExaBuildAddressMapping(DeviceObject);
        if (status != STATUS_SUCCESS) {
            DebugPrint((1,
                       "Spctrmc: InitElementStatus: Build address map failed %x\n",
                       status));
            return status;
        }
    }

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (srb == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    // ----------------------------------------------------------
    // 
    // Get Mode Sense Page 1D - Element address assignment page.

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned,
                                sizeof(MODE_PARAMETER_HEADER) +
                                sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, sizeof(MODE_PARAMETER_HEADER) +
                              sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER) +
                              sizeof(MODE_ELEMENT_ADDRESS_PAGE);
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_ELEMENT_ADDRESS;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    // Send the request.
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

    // Fill in values.

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(changerParameters, sizeof(GET_CHANGER_PARAMETERS));

    elementAddressPage = modeBuffer;
    (ULONG_PTR)elementAddressPage += sizeof(MODE_PARAMETER_HEADER);

    changerParameters->Size = sizeof(GET_CHANGER_PARAMETERS);
    changerParameters->NumberTransportElements = 
                 elementAddressPage->NumberTransportElements[1];
    changerParameters->NumberTransportElements |= 
                 (elementAddressPage->NumberTransportElements[0] << 8);

    changerParameters->NumberStorageElements = 
                 elementAddressPage->NumberStorageElements[1];
    changerParameters->NumberStorageElements |= 
                 (elementAddressPage->NumberStorageElements[0] << 8);

    changerParameters->NumberIEElements = 
                 elementAddressPage->NumberIEPortElements[1];
    changerParameters->NumberIEElements |= 
                 (elementAddressPage->NumberIEPortElements[0] << 8);
    
    changerParameters->NumberDataTransferElements = 
                 elementAddressPage->NumberDataXFerElements[1];
    changerParameters->NumberDataTransferElements |= 
                 (elementAddressPage->NumberDataXFerElements[0] << 8);

    changerParameters->NumberOfDoors = 0;
    if (changerData->DriveID == S_8mm_AIT) {
        changerParameters->NumberOfDoors = 1;
    }

    changerParameters->NumberCleanerSlots = 0;

    changerParameters->FirstSlotNumber = 1;
    changerParameters->FirstDriveNumber =  1;
    changerParameters->FirstTransportNumber = 0;
    changerParameters->FirstIEPortNumber = 0;
    changerParameters->FirstCleanerSlotAddress = 0;

    changerParameters->MagazineSize = 
                     changerParameters->NumberStorageElements;

    changerParameters->DriveCleanTimeout = 600;

    ChangerClassFreePool(modeBuffer);

    // ----------------------------------------------------------
    // 
    // Get Mode Sense Page 1E - transport geometry mode sense.

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, 
                             sizeof(MODE_PARAMETER_HEADER) +
                             sizeof(MODE_PAGE_TRANSPORT_GEOMETRY));
    if (!modeBuffer) {
        ChangerClassFreePool(srb);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    RtlZeroMemory(modeBuffer, sizeof(MODE_PARAMETER_HEADER) +
                              sizeof(MODE_TRANSPORT_GEOMETRY_PAGE));
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER) + 
                              sizeof(MODE_TRANSPORT_GEOMETRY_PAGE);
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_TRANSPORT_GEOMETRY;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    // Send the request.
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
        changerParameters->Features1 = CHANGER_IEPORT_USER_CONTROL_OPEN |
                                                                   CHANGER_IEPORT_USER_CONTROL_CLOSE ;

    // initialize Features 0 and then set flip bit... 
    changerParameters->Features0 = 
             transportGeometryPage->Flip ? CHANGER_MEDIUM_FLIP : 0;

    // Features based on manual, nothing programatic.
    changerParameters->Features0 |= 
               CHANGER_STATUS_NON_VOLATILE           | 
               CHANGER_LOCK_UNLOCK                   |                                   
               CHANGER_POSITION_TO_ELEMENT           |
               CHANGER_REPORT_IEPORT_STATE           |
               CHANGER_DRIVE_CLEANING_REQUIRED       |
               CHANGER_PREDISMOUNT_EJECT_REQUIRED;

        // Only the IEPORT can be locked and unlocked
    changerParameters->LockUnlockCapabilities = LOCK_UNLOCK_IEPORT;

    // Serial number not supported on 4000 and 9000 libraries
    // Serial number is supported on 5000, 10000 and Treefrog units
    if (changerData->DriveID == S_8mm_SONY) {
        changerParameters->Features0 |= 
               CHANGER_SERIAL_NUMBER_VALID;
    } 
   
    ChangerClassFreePool(modeBuffer);

    // ----------------------------------------------------------
    // 
    // Get Mode Sense Page 00 - Vendor Unique Parameter List Page
    //                          Is Barcode reader installed ? 

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    length =  sizeof(MODE_PARAMETER_HEADER) + 
              sizeof(VUPL_MODE_PAGE);

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
    cdb->MODE_SENSE.PageCode = 0;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

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
    vuplModePage = modeBuffer;
    (ULONG_PTR)vuplModePage += sizeof(MODE_PARAMETER_HEADER);

    // EBarCo is set if there is a barcode reader installed
    if (vuplModePage->EBarCo == 1) { 
         changerParameters->Features0 |=
                             CHANGER_BAR_CODE_SCANNER_INSTALLED; 
    }

    // ----------------------------------------------------------
    // 
    // Get Mode Sense Page 1F - Device Capabilities Page

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

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
    capabilitiesPage = modeBuffer;
    (ULONG_PTR)capabilitiesPage += sizeof(MODE_PARAMETER_HEADER);

    // Fill in values in Features that are contained in this page.

    changerParameters->Features0 |= 
     capabilitiesPage->MediumTransport ? CHANGER_STORAGE_DRIVE : 0;
    changerParameters->Features0 |= 
     capabilitiesPage->StorageLocation ? CHANGER_STORAGE_SLOT : 0;
    changerParameters->Features0 |= 
     capabilitiesPage->IEPort ? CHANGER_STORAGE_IEPORT : 0;
    changerParameters->Features0 |= 
     capabilitiesPage->DataXFer ? CHANGER_STORAGE_DRIVE : 0;

    // Determine all the move from and exchange from 
    // capabilities of this device.

    changerParameters->MoveFromTransport = 
     capabilitiesPage->MTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromTransport |= 
     capabilitiesPage->MTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromTransport |= 
     capabilitiesPage->MTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromTransport |= 
     capabilitiesPage->MTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromSlot = 
     capabilitiesPage->STtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromSlot |= 
     capabilitiesPage->STtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromSlot |= 
     capabilitiesPage->STtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromSlot |= 
     capabilitiesPage->STtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromIePort = 
     capabilitiesPage->IEtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromIePort |= 
     capabilitiesPage->IEtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromIePort |= 
     capabilitiesPage->IEtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromIePort |= 
     capabilitiesPage->IEtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromDrive = 
     capabilitiesPage->DTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromDrive |= 
     capabilitiesPage->DTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromDrive |= 
     capabilitiesPage->DTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->MoveFromDrive |= 
     capabilitiesPage->DTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromTransport = 
     capabilitiesPage->XMTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromTransport |= 
     capabilitiesPage->XMTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromTransport |= 
     capabilitiesPage->XMTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromTransport |= 
     capabilitiesPage->XMTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromSlot = 
     capabilitiesPage->XSTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromSlot |= 
     capabilitiesPage->XSTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromSlot |= 
     capabilitiesPage->XSTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromSlot |= 
     capabilitiesPage->XSTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromIePort = 
     capabilitiesPage->XIEtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromIePort |= 
     capabilitiesPage->XIEtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromIePort |= 
     capabilitiesPage->XIEtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromIePort |= 
     capabilitiesPage->XIEtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromDrive = 
     capabilitiesPage->XDTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromDrive |= 
     capabilitiesPage->XDTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromDrive |= 
     capabilitiesPage->XDTtoIE ? CHANGER_TO_IEPORT : 0;
    changerParameters->ExchangeFromDrive |= 
     capabilitiesPage->XDTtoDT ? CHANGER_TO_DRIVE : 0;

        
        // legal Position capabilities... 
        changerParameters->PositionCapabilities = 
                        CHANGER_TO_SLOT | 
                        CHANGER_TO_IEPORT | 
                        CHANGER_TO_DRIVE;
                

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

    srb->CdbLength = CDB6GENERIC_LENGTH;
    cdb->CDB6GENERIC.OperationCode = SCSIOP_TEST_UNIT_READY;
    srb->TimeOutValue = 20;

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

    RtlMoveMemory(productData->SerialNumber, changerData->SerialNumber, 
                  VPD_SERIAL_NUMBER_LENGTH);

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

    This routine sets the state of the Door or IEPort. 

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
    BOOLEAN             writeToDevice = FALSE;

    // could lock the front panel, but it may be needed for other 
    // operator tasks. 
    if ((setAccess->Element.ElementType == ChangerKeypad) ||
        (setAccess->Element.ElementType == ChangerDoor)) {
              return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (setAccess->Element.ElementType == ChangerIEPort) {

        // Do Prevent/Allow Medium  Removal... 
        srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
        if (!srb) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
        cdb = (PCDB)srb->Cdb;

        srb->CdbLength = CDB6GENERIC_LENGTH;
        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

        srb->DataBuffer = NULL;
        srb->DataTransferLength = 0;
        srb->TimeOutValue = 10;

        if (controlOperation == LOCK_ELEMENT) {
            cdb->MEDIA_REMOVAL.Prevent = 1;
        } else if (controlOperation == UNLOCK_ELEMENT) {
            cdb->MEDIA_REMOVAL.Prevent = 0;
        } else {
            status = STATUS_INVALID_PARAMETER;
        }
    } else {
       return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (NT_SUCCESS(status)) {
        status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             srb->DataBuffer,
                                             srb->DataTransferLength,
                                             FALSE);
    }

    if (srb->DataBuffer) {
        ChangerClassFreePool(srb->DataBuffer);
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

    This routine builds and issues a read element status command 
    for either all elements or the
    specified element type. The buffer returned is used to build 
    the user buffer.

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
    ELEMENT_TYPE        elementType;
    PSCSI_REQUEST_BLOCK srb;
    PCDB     cdb;
    ULONG    length;
    ULONG    statusPages;
    NTSTATUS status;
    PVOID    statusBuffer;
    PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG    outputBuffLen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    // Get the element type.
    elementType = readElementStatus->ElementList.Element.ElementType;
    element = &readElementStatus->ElementList.Element;

    // Q215 does not report IEPORT status

    if (elementType == AllElements) {
        statusPages = 4;
    } else {
        statusPages = 1;
    } 

    if (readElementStatus->VolumeTagInfo) {
        length = sizeof(ELEMENT_STATUS_HEADER) + 
            (statusPages * sizeof(ELEMENT_STATUS_PAGE)) +
            (SPC_FULL_SIZE * 
                readElementStatus->ElementList.NumberOfElements);
    } else {
        length = sizeof(ELEMENT_STATUS_HEADER) + 
            (statusPages * sizeof(ELEMENT_STATUS_PAGE)) +
            (SPC_PARTIAL_SIZE * 
                readElementStatus->ElementList.NumberOfElements);
    }

    statusBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, length);
    if (!statusBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(statusBuffer, length);

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->DataBuffer = statusBuffer;
    srb->DataTransferLength = length;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    cdb->READ_ELEMENT_STATUS.OperationCode = SCSIOP_READ_ELEMENT_STATUS;
    cdb->READ_ELEMENT_STATUS.ElementType = (UCHAR)elementType;
    cdb->READ_ELEMENT_STATUS.VolTag = readElementStatus->VolumeTagInfo;

    cdb->READ_ELEMENT_STATUS.StartingElementAddress[0] =
        (UCHAR)((element->ElementAddress + 
        addressMapping->FirstElement[element->ElementType]) >> 8);
    cdb->READ_ELEMENT_STATUS.StartingElementAddress[1] =
        (UCHAR)((element->ElementAddress + 
        addressMapping->FirstElement[element->ElementType]) & 0xFF);

    cdb->READ_ELEMENT_STATUS.NumberOfElements[0] =         
        (UCHAR)(readElementStatus->ElementList.NumberOfElements >> 8);
    cdb->READ_ELEMENT_STATUS.NumberOfElements[1] =         (UCHAR)(readElementStatus->ElementList.NumberOfElements & 0xFF);

    cdb->READ_ELEMENT_STATUS.AllocationLength[0] =
                                      (UCHAR)(length >> 16);
    cdb->READ_ELEMENT_STATUS.AllocationLength[1] =
                                      (UCHAR)(length >> 8);
    cdb->READ_ELEMENT_STATUS.AllocationLength[2] =
                                      (UCHAR)(length & 0xFF);

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);
    if (NT_SUCCESS(status) ||
        (status == STATUS_DATA_OVERRUN)) {
        PELEMENT_STATUS_HEADER statusHeader = statusBuffer;
        PELEMENT_STATUS_PAGE statusPage;
        PSPC_ED elementDescriptor;
        ULONG numberElements = 
             readElementStatus->ElementList.NumberOfElements;
        LONG remainingElements;
        LONG typeCount;
        BOOLEAN tagInfo = readElementStatus->VolumeTagInfo;
        LONG i;
        ULONG descriptorLength;

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

        // Determine total number elements returned.
        remainingElements = statusHeader->NumberOfElements[1];
        remainingElements |= (statusHeader->NumberOfElements[0] << 8);

        // The buffer is composed of a header, status page, 
        // and element descriptors.
        // Point each element to it's respective place in the buffer.

        (ULONG_PTR)statusPage = (ULONG_PTR)statusHeader;
        (ULONG_PTR)statusPage += sizeof(ELEMENT_STATUS_HEADER);

        elementType = statusPage->ElementType;

        (ULONG_PTR)elementDescriptor = (ULONG_PTR)statusPage;
        (ULONG_PTR)elementDescriptor += sizeof(ELEMENT_STATUS_PAGE);

        descriptorLength = 
           statusPage->ElementDescriptorLength[1];
        descriptorLength |= 
           (statusPage->ElementDescriptorLength[0] << 8);

        // Determine the number of elements of this type reported.
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

        // Fill in user buffer.
        elementStatus = Irp->AssociatedIrp.SystemBuffer;
        RtlZeroMemory(elementStatus, outputBuffLen);

        do {
            for (i = 0; i < typeCount; i++, remainingElements--) {

                // Get the address for this element.

                elementStatus->Element.ElementAddress =
                 elementDescriptor->SPC_FED.ElementAddress[1];

                elementStatus->Element.ElementAddress |=
                  (elementDescriptor->SPC_FED.ElementAddress[0] << 8);

                // Account for address mapping.
                elementStatus->Element.ElementAddress -= 
                   addressMapping->FirstElement[elementType];

                // Set the element type.
                elementStatus->Element.ElementType = elementType;

                if (elementDescriptor->SPC_FED.SValid) {

                    ULONG  j;
                    USHORT tmpAddress;


                    // Source address is valid. 
                    // Determine the device specific address.
                    tmpAddress = elementDescriptor->SPC_FED.SourceStorageElementAddress[1];
                    tmpAddress |= (elementDescriptor->SPC_FED.SourceStorageElementAddress[0] << 8);

                    // Now convert to 0-based values.
                    for (j = 1; j <= ChangerDrive; j++) {
                        if (addressMapping->FirstElement[j] <= tmpAddress) {
                            if (tmpAddress < (addressMapping->NumberOfElements[j] + addressMapping->FirstElement[j])) {
                                elementStatus->SrcElementAddress.ElementType = j;
                                break;
                            }
                        }
                    }

                    elementStatus->SrcElementAddress.ElementAddress =
                         tmpAddress - addressMapping->FirstElement[j];

                }

                // Build Flags field.

                elementStatus->Flags = 
                 elementDescriptor->SPC_FED.Full;
                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.Exception << 2);
                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.Accessible << 3);

                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.LunValid << 12);
                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.IdValid << 13);
                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.NotThisBus << 15);

                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.Invert << 22);
                elementStatus->Flags |= 
                 (elementDescriptor->SPC_FED.SValid << 23);


                if (tagInfo) {
                    RtlMoveMemory(elementStatus->PrimaryVolumeID, 
                        elementDescriptor->SPC_FED.PrimaryVolumeTag, 
                        MAX_VOLUME_ID_SIZE);
                    elementStatus->Flags |= ELEMENT_STATUS_PVOLTAG;
                }

                if (elementStatus->Flags & ELEMENT_STATUS_EXCEPT) {
                    elementStatus->ExceptionCode = MapExceptionCodes(elementDescriptor);
                }

                // if the Pvoltag field is all nulls, this indicates a missing 
                // barcode label as well. 

                if (elementStatus->Flags & ELEMENT_STATUS_PVOLTAG) {

                    ULONG index;
                    for (index = 0; index < MAX_VOLUME_ID_SIZE; index++) {
                        if (elementStatus->PrimaryVolumeID[index] != '\0') {
                            break;
                        }
                    }

                    //
                    // Determine if the volume id was all spaces. Do an extra check to see if media is
                    // actually present, for the unit will set the PVOLTAG flag whether media is present or not.
                    //

                    if ((index == MAX_VOLUME_ID_SIZE) && 
                        (elementStatus->Flags & ELEMENT_STATUS_FULL)) {

                        elementStatus->Flags &= ~ELEMENT_STATUS_PVOLTAG;
                        elementStatus->Flags |= ELEMENT_STATUS_EXCEPT;
                        elementStatus->ExceptionCode = ERROR_LABEL_UNREADABLE;
                    }
                }


                if (elementDescriptor->SPC_FED.IdValid) {
                    elementStatus->TargetId = 
                     elementDescriptor->SPC_FED.BusAddress;
                }
                if (elementDescriptor->SPC_FED.LunValid) {
                    elementStatus->Lun = elementDescriptor->SPC_FED.Lun;
                }



                // Get next descriptor.
                (ULONG_PTR)elementDescriptor += descriptorLength;

                // Advance to the next entry in the user 
                // buffer and element descriptor array.
                elementStatus += 1;
            }

            if (remainingElements > 0) {
                // Get next status page.
                (ULONG_PTR)statusPage = (ULONG_PTR)elementDescriptor;
                elementType = statusPage->ElementType;

                // Point to decriptors.
                (ULONG_PTR)elementDescriptor = (ULONG_PTR)statusPage;
                (ULONG_PTR)elementDescriptor += sizeof(ELEMENT_STATUS_PAGE);

                descriptorLength = 
                   statusPage->ElementDescriptorLength[1];
                descriptorLength |= 
                   (statusPage->ElementDescriptorLength[0] << 8);

                // Determine the number of this element type reported.
                typeCount = statusPage->DescriptorByteCount[2];
                typeCount |= (statusPage->DescriptorByteCount[1] << 8);
                typeCount |= (statusPage->DescriptorByteCount[0] << 16);

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

        Irp->IoStatus.Information = 
                sizeof(CHANGER_ELEMENT_STATUS) * numberElements;

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

    This routine issues the necessary command to either 
    initialize all elements or the specified range of elements 
    using the normal SCSI-2 command, or a vendor-unique
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

    if (addressMapping->Initialized != TRUE) {
        status = ExaBuildAddressMapping(DeviceObject);
        if (status != STATUS_SUCCESS) {
            DebugPrint((1,
                       "Spctrmc: InitElementStatus: Build address map failed %x\n",
                       status));
            return status;
        }
    }

    // IES w/Range is only supported on Q215
    // 
    if (initElementStatus->ElementList.Element.ElementType != 
                                                 AllElements) {
        return STATUS_INVALID_DEVICE_REQUEST;
    } 

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    // All elements requested...
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = fdoExtension->TimeOutValue;
    srb->DataTransferLength = 0;

    cdb->INIT_ELEMENT_STATUS.OperationCode = 
                                  SCSIOP_INIT_ELEMENT_STATUS;
    cdb->INIT_ELEMENT_STATUS.NoBarCode = 
        initElementStatus->BarCodeScan ? 0 : 1;

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = 
                     sizeof(CHANGER_INITIALIZE_ELEMENT_STATUS);
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

    This routine issues the appropriate command to set the 
    robotic mechanism to the specified
    element address. Normally used to optimize moves or 
    exchanges by pre-positioning the picker.

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
    USHORT              transport;
    USHORT              destination;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

    if ((setPosition->Destination.ElementType == ChangerKeypad) ||
        (setPosition->Destination.ElementType == ChangerDoor)   ||
        (setPosition->Destination.ElementType == ChangerMaxElement)) {
        return STATUS_INVALID_PARAMETER;
    }

    // Verify transport, source, and dest. are within range.
    // Convert from 0-based to device-specific addressing.

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

    // Convert to device addresses.

    transport += addressMapping->FirstElement[ChangerTransport];
    destination += 
     addressMapping->FirstElement[setPosition->Destination.ElementType];

    if (setPosition->Flip) {
        return STATUS_INVALID_PARAMETER;
    }

    // Build srb and cdb.

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    srb->DataTransferLength = 0;
    srb->TimeOutValue = fdoExtension->TimeOutValue;

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB10GENERIC_LENGTH;
    cdb->POSITION_TO_ELEMENT.OperationCode = SCSIOP_POSITION_TO_ELEMENT;

    // Build device-specific addressing.

    cdb->POSITION_TO_ELEMENT.TransportElementAddress[0] =
                    (UCHAR)(transport >> 8);
    cdb->POSITION_TO_ELEMENT.TransportElementAddress[1] = 
                    (UCHAR)(transport & 0xFF);

    cdb->POSITION_TO_ELEMENT.DestinationElementAddress[0] = 
                    (UCHAR)(destination >> 8);
    cdb->POSITION_TO_ELEMENT.DestinationElementAddress[1] = 
                    (UCHAR)(destination & 0xFF);

    cdb->POSITION_TO_ELEMENT.Flip = setPosition->Flip;

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
}



NTSTATUS
ChangerExchangeMedium(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    None of the units support exchange medium.

Arguments:

    DeviceObject
    Irp

Return Value:

    STATUS_INVALID_DEVICE_REQUEST

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
    USHORT              transport;
    USHORT              source;
    USHORT              destination;
    PSCSI_REQUEST_BLOCK srb;
    PCDB                cdb;
    NTSTATUS            status;

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
    // Build srb and cdb.
    //

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    //
    // If the source or destination is an IEPORT,
    // do an allow before the move and a prevent after the move.
    // This works around the behaviour of the device whereby a PreventMediumRemoval
    // inhibits a MoveMedium to/from the IEPORT.
    //

    if ((moveMedium->Destination.ElementType == ChangerIEPort) ||
        (moveMedium->Source.ElementType == ChangerIEPort)) {

        //
        // Send an allow to clear the prevent for IEPORT extend/retract.
        //

        cdb = (PCDB)srb->Cdb;
        srb->CdbLength = CDB6GENERIC_LENGTH;
        srb->DataTransferLength = 0;
        srb->TimeOutValue = fdoExtension->TimeOutValue;
        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
        cdb->MEDIA_REMOVAL.Prevent = 0;

        status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             NULL,
                                             0,
                                             FALSE);
        status = STATUS_SUCCESS;
    }

    // Convert to device addresses.
    //
    transport += 
      addressMapping->FirstElement[ChangerTransport];
    source += 
      addressMapping->FirstElement[moveMedium->Source.ElementType];
    destination += 
      addressMapping->FirstElement[moveMedium->Destination.ElementType];

    if (moveMedium->Flip) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    srb->CdbLength = CDB12GENERIC_LENGTH;
    srb->TimeOutValue = fdoExtension->TimeOutValue;
    srb->DataTransferLength = 0;

    cdb->MOVE_MEDIUM.OperationCode = SCSIOP_MOVE_MEDIUM;

    cdb->MOVE_MEDIUM.TransportElementAddress[0] = 
      (UCHAR)(transport >> 8);
    cdb->MOVE_MEDIUM.TransportElementAddress[1] = 
      (UCHAR)(transport & 0xFF);

    cdb->MOVE_MEDIUM.SourceElementAddress[0] = 
      (UCHAR)(source >> 8);
    cdb->MOVE_MEDIUM.SourceElementAddress[1] = 
      (UCHAR)(source & 0xFF);

    cdb->MOVE_MEDIUM.DestinationElementAddress[0] = 
      (UCHAR)(destination >> 8);
    cdb->MOVE_MEDIUM.DestinationElementAddress[1] = 
      (UCHAR)(destination & 0xFF);

    cdb->MOVE_MEDIUM.Flip = moveMedium->Flip;

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         NULL,
                                         0,
                                         FALSE);

    if (NT_SUCCESS(status)) {
        Irp->IoStatus.Information = sizeof(CHANGER_MOVE_MEDIUM);
    }


    if ((moveMedium->Destination.ElementType == ChangerIEPort) ||
        (moveMedium->Source.ElementType == ChangerIEPort)) {

        //
        // Send a prevent to prevent further IEPORT extend/retract.
        //

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
        cdb = (PCDB)srb->Cdb;
        srb->CdbLength = CDB6GENERIC_LENGTH;
        srb->DataTransferLength = 0;
        srb->TimeOutValue = fdoExtension->TimeOutValue;
        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;
        cdb->MEDIA_REMOVAL.Prevent = 1;

        ChangerClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             NULL,
                                             0,
                                             FALSE);
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

    // there is no command on these libraries to home or reinit the 
    // changer mechanism

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
        addressMapping->FirstElement[i] = SPC_NO_ELEMENT;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb->Cdb;

    //
    // Build a mode sense - Element address assignment page.
    //

    modeBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned,
                                sizeof(MODE_PARAMETER_HEADER) +
                                sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    if (!modeBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(modeBuffer, sizeof(MODE_PARAMETER_HEADER) + 
                              sizeof(MODE_ELEMENT_ADDRESS_PAGE));
    srb->CdbLength = CDB6GENERIC_LENGTH;
    srb->TimeOutValue = 20;
    srb->DataTransferLength = sizeof(MODE_PARAMETER_HEADER) + 
                              sizeof(MODE_ELEMENT_ADDRESS_PAGE);
    srb->DataBuffer = modeBuffer;

    cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
    cdb->MODE_SENSE.PageCode = MODE_PAGE_ELEMENT_ADDRESS;
    cdb->MODE_SENSE.Dbd = 1;
    cdb->MODE_SENSE.AllocationLength = (UCHAR)srb->DataTransferLength;

    status = ChangerClassSendSrbSynchronous(DeviceObject,
                                         srb,
                                         srb->DataBuffer,
                                         srb->DataTransferLength,
                                         FALSE);

    elementAddressPage = modeBuffer;
    (ULONG_PTR)elementAddressPage += sizeof(MODE_PARAMETER_HEADER);

    if (NT_SUCCESS(status)) {
        // Build address mapping.
        //

        addressMapping->FirstElement[ChangerTransport] = 
         (elementAddressPage->MediumTransportElementAddress[0] << 8) |
         elementAddressPage->MediumTransportElementAddress[1];

        addressMapping->FirstElement[ChangerDrive] = 
         (elementAddressPage->FirstDataXFerElementAddress[0] << 8) |
         elementAddressPage->FirstDataXFerElementAddress[1];

        addressMapping->FirstElement[ChangerIEPort] = 
         (elementAddressPage->FirstIEPortElementAddress[0] << 8) |
         elementAddressPage->FirstIEPortElementAddress[1];

        addressMapping->FirstElement[ChangerSlot] = 
         (elementAddressPage->FirstStorageElementAddress[0] << 8) |
         elementAddressPage->FirstStorageElementAddress[1];

        addressMapping->FirstElement[ChangerDoor] = 0;
        addressMapping->FirstElement[ChangerKeypad] = 0;

        addressMapping->NumberOfElements[ChangerTransport] = 
         elementAddressPage->NumberTransportElements[1];
        addressMapping->NumberOfElements[ChangerTransport] |= 
         (elementAddressPage->NumberTransportElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDrive] = 
         elementAddressPage->NumberDataXFerElements[1];
        addressMapping->NumberOfElements[ChangerDrive] |= 
         (elementAddressPage->NumberDataXFerElements[0] << 8);

        addressMapping->NumberOfElements[ChangerIEPort] = 
         elementAddressPage->NumberIEPortElements[1];
        addressMapping->NumberOfElements[ChangerIEPort] |= 
         (elementAddressPage->NumberIEPortElements[0] << 8);

        addressMapping->NumberOfElements[ChangerSlot] = 
         elementAddressPage->NumberStorageElements[1];
        addressMapping->NumberOfElements[ChangerSlot] |= 
         (elementAddressPage->NumberStorageElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDoor] = 1;
        addressMapping->NumberOfElements[ChangerKeypad] = 1;

        addressMapping->Initialized = TRUE;
    }


    // Determine the lowest element address for use with AllElements.
    //
    for (i = 0; i < ChangerDrive; i++) {
        if (addressMapping->FirstElement[i] < 
                         addressMapping->FirstElement[AllElements]) {
            addressMapping->FirstElement[AllElements] = 
                                     addressMapping->FirstElement[i];
        }
    }

    ChangerClassFreePool(modeBuffer);
    ChangerClassFreePool(srb);

    return status;
}



ULONG
MapExceptionCodes(
    IN PSPC_ED ElementDescriptor
    )

/*++

Routine Description:

    This routine takes the sense data from the elementDescriptor 
    and creates the appropriate bitmap of values.

Arguments:

   ElementDescriptor - pointer to the descriptor page.

Return Value:

    Bit-map of exception codes.

--*/

{
    UCHAR asc = ElementDescriptor->SPC_FED.AdditionalSenseCode;
    UCHAR asq = ElementDescriptor->SPC_FED.AddSenseCodeQualifier;
    ULONG exceptionCode = 0;

    if (asc == 0x85) {
        switch (asq) {
                case 0x33:
                    exceptionCode = ERROR_LABEL_UNREADABLE;
                    break;

                case 0x23:
                    exceptionCode = ERROR_DRIVE_NOT_INSTALLED;
                    break;

                default:
                    exceptionCode = ERROR_UNHANDLED_ERROR;

        }
    }

    if (asc == 0x83) {    
        switch (asq) {
                case 0x00:
                case 0x01:
                    exceptionCode = ERROR_LABEL_UNREADABLE;
                    break;

                default:
                    exceptionCode = ERROR_UNHANDLED_ERROR;
        }
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
    } else if (AddressMap->FirstElement[ElementType] == SPC_NO_ELEMENT) {

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
   // Initialize the devicestatus in the device extension to
   // SPECTRA_DEVICE_PROBLEM_NONE. If the changer returns sense code
   // SCSI_SENSE_HARDWARE_ERROR on SelfTest, we'll set an appropriate
   // devicestatus.
   //
   changerData->DeviceStatus = SPECTRA_DEVICE_PROBLEM_NONE;

   changerDeviceError->ChangerProblemType = DeviceProblemNone;

   srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

   if (srb == NULL) {
      DebugPrint((1, "SPCTRAMC\\ChangerPerformDiagnostics : No memory\n"));
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
   // Set selftest bit in the CDB
   //
   cdb->CDB6GENERIC.CommandUniqueBits = 0x2;

   status =  ChangerClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);
   if (NT_SUCCESS(status)) {
      changerDeviceError->ChangerProblemType = DeviceProblemNone;
   } else if ((changerData->DeviceStatus) != SPECTRA_DEVICE_PROBLEM_NONE) {
         changerDeviceError->ChangerProblemType = DeviceProblemHardware;
   }
   
   ChangerClassFreePool(srb);
   return status;
}
