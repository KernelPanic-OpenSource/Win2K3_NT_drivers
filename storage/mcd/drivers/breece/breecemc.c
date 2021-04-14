/*++

Copyright (C) Microsoft Corporation, 1996 - 1999

Module Name:

    breecemc.c

Abstract:

    This module contains device-specific routines for the following
    Breece Hill medium changers: 
            - Q7
            - Q47

Author:

    davet (Dave Therrien - HighGround Systems)

Environment:

    kernel mode only

Revision History:


--*/

#include "ntddk.h"
#include "mcd.h"
#include "breecemc.h"

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
    needed by the exabyte changers.

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

    changerData->Size = sizeof(CHANGER_DATA);

    //
    // Get inquiry data.
    //

    dataBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, sizeof(INQUIRYDATA));
    if (!dataBuffer) {
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

        if (RtlCompareMemory(dataBuffer->ProductId,"Quad 7",6) == 6) {
            changerData->DriveID = Q7;
        } else if (RtlCompareMemory(dataBuffer->ProductId,"Quad 47",7) == 7) {
            changerData->DriveID = Q47;
        } 
    }

    ChangerClassFreePool(dataBuffer);


    //
    // Build address mapping.
    //

    status = ExaBuildAddressMapping(DeviceObject);
    if (!NT_SUCCESS(status)) {
        DebugPrint((1,
                    "BuildAddressMapping failed. %x\n", status));
        return status;
    }

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
    ULONG deviceStatus;
    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;


    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        switch (senseBuffer->SenseKey & 0xf) {

        case SCSI_SENSE_NOT_READY:

           if (senseBuffer->AdditionalSenseCode == 0x04) {
                switch (senseBuffer->AdditionalSenseCodeQualifier) {
                    case 0x81:
                    case 0x82:
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
           deviceStatus = BREECE_HW_ERROR;

           switch (senseBuffer->AdditionalSenseCode) {
            case BREECE_ASC_HW_NOT_RESPONDING: {
               deviceStatus = BREECE_HW_ERROR;
               break;
            }

            case BREECE_ASC_PICK_PUT_ERROR: {
               deviceStatus = BREECE_CHM_ERROR;
              break;
            }

            case BREECE_ASC_DRIVE_ERROR: {
               deviceStatus = BREECE_DRIVE_ERROR;
               break;
            }

            case BREECE_ASC_DIAGNOSTIC_ERROR: {
               switch (senseBuffer->AdditionalSenseCodeQualifier) {
                  case BREECE_ASCQ_UNABLE_TO_OPEN_PICKER_JAW:
                  case BREECE_ASCQ_UNABLE_TO_CLOSE_PICKER_JAW: {
                     deviceStatus = BREECE_CHM_ERROR;
                     break;
                  }

                  case BREECE_ASCQ_THETA_AXIS_STUCK:
                  case BREECE_ASCQ_Y_AXIS_STUCK:
                  case BREECE_ASCQ_Z_AXIS_STUCK: {
                     deviceStatus = BREECE_CHM_MOVE_ERROR;
                  }

                  default: {
                     deviceStatus = BREECE_HW_ERROR;
                     break;
                  }
               } // switch (senseBuffer->AdditionalSenseCodeQualifier)

               break;
            }

            case BREECE_ASC_INTERNAL_HW_ERROR: 
            case BREECE_ASC_BARCODE_READ_ERROR: 
            case BREECE_ASC_INTERNAl_SW_ERROR: {
               deviceStatus = BREECE_HW_ERROR;
               break;
            }

            default: {
               deviceStatus = BREECE_HW_ERROR;
               break;
            }
           } // switch (senseBuffer->AdditionalSenseCode)

           changerData->DeviceStatus = deviceStatus;
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
    exabyte changers.

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

    // Breece Hill reports an IEPORT for both the Q7 and 47 
    // to represent an access to a 7 cartridge magazine. These
    // are really slots, not IEPORTs. 
    // The Q47 has an option called SCI/E which is a real IEPORT,
    // BUT IT IS NOT SUPPORTED BY NTMS PER AGREEMENT BY BREECE HILL
    // AND HIGHGROUND (3/31/98 - Ray Heineman, Brian Conrey of BH). 
    // BH DID NOT WANT TO FIX A BUG WITH THE 
    // INABILITY TO SCAN A MEDIUM THAT WAS INJECTED OR THE 
    // INNEFICIENCY OF DOING A SINGLE SLOT IES (which did them
    // all, not a single slot).  
    
    //if ((changerData->DriveID == Q47) && 
    //    ((changerData->InquiryData.VendorSpecific[19] & 0x2) == 0x2)) {
    //   changerParameters->NumberIEElements = 1;
    //} else { 
        changerParameters->NumberIEElements = 0;
    //} 

    changerParameters->NumberDataTransferElements = 
                 elementAddressPage->NumberDataXFerElements[1];
    changerParameters->NumberDataTransferElements |= 
                 (elementAddressPage->NumberDataXFerElements[0] << 8);

    changerParameters->NumberOfDoors = 1;

    changerParameters->NumberCleanerSlots = 0;

    changerParameters->FirstSlotNumber = 0;
    changerParameters->FirstDriveNumber =  0;
    changerParameters->FirstTransportNumber = 0;
    changerParameters->FirstIEPortNumber = 0;
    changerParameters->FirstCleanerSlotAddress = 0;

    changerParameters->MagazineSize = 7;

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
        changerParameters->Features1 = 0;

    // initialize Features 0 and then set flip bit... 
    changerParameters->Features0 = 
             transportGeometryPage->Flip ? CHANGER_MEDIUM_FLIP : 0;

    // Breece Hill Q7 and Q47 units do not set the Inquiry BC bit
    // but they always hav a barcode reader installed. 

    changerParameters->Features0 |=
                             CHANGER_BAR_CODE_SCANNER_INSTALLED; 


    // Features based on manual, nothing programatic.
    changerParameters->Features0 |= 
               CHANGER_STATUS_NON_VOLATILE           | 
               CHANGER_LOCK_UNLOCK                   |                                   
               CHANGER_CARTRIDGE_MAGAZINE            |
               CHANGER_DRIVE_CLEANING_REQUIRED       |
               CHANGER_PREDISMOUNT_EJECT_REQUIRED;

    // FIRMWARE BUG!
    // the spec says IES w/RANGE is supported, but
    // IES to one slot does them all and IES to IEPORT
    // doesn't do anything !
    //          CHANGER_INIT_ELEM_STAT_WITH_RANGE     |


    // Only the Door can be locked and unlocked
    // Door on Q7/Q47 is top access mechanism, not the big front door !
    // Big front door cannot be locked
    changerParameters->LockUnlockCapabilities = LOCK_UNLOCK_DOOR;


    // legal Position capabilities... 
    changerParameters->PositionCapabilities = 0;


    ChangerClassFreePool(modeBuffer);

    // ----------------------------------------------------------
    // 
    // Get Mode Sense Page 1F - Device Capabilities Page

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

    // BreeceHill uses an addition 4 bytes past the 
    // scsi-defined structure.

    length =  sizeof(MODE_PARAMETER_HEADER) + 
              sizeof(MODE_DEVICE_CAPABILITIES_PAGE) + 
              BREECE_DEVICE_CAP_EXTENSION;

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

    //
    // Get the systembuffer and by-pass the mode header for the mode sense data.
    //

    changerParameters = Irp->AssociatedIrp.SystemBuffer;
    capabilitiesPage = modeBuffer;
    (ULONG_PTR)capabilitiesPage += sizeof(MODE_PARAMETER_HEADER);

    // Fill in values in Features that are contained in this page.

    changerParameters->Features0 |= 
     capabilitiesPage->MediumTransport ? CHANGER_STORAGE_DRIVE : 0;
    changerParameters->Features0 |= 
     capabilitiesPage->StorageLocation ? CHANGER_STORAGE_SLOT : 0;
    changerParameters->Features0 |= 
     capabilitiesPage->DataXFer ? CHANGER_STORAGE_DRIVE : 0;

    // Determine all the move from and exchange from 
    // capabilities of this device.

    changerParameters->MoveFromTransport = 
     capabilitiesPage->MTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromTransport |= 
     capabilitiesPage->MTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromTransport |= 
     capabilitiesPage->MTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromSlot = 
     capabilitiesPage->STtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromSlot |= 
     capabilitiesPage->STtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromSlot |= 
     capabilitiesPage->STtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->MoveFromIePort = 0;

    changerParameters->MoveFromDrive = 
     capabilitiesPage->DTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->MoveFromDrive |= 
     capabilitiesPage->DTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->MoveFromDrive |= 
     capabilitiesPage->DTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromTransport = 
     capabilitiesPage->XMTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromTransport |= 
     capabilitiesPage->XMTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromTransport |= 
     capabilitiesPage->XMTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromSlot = 
     capabilitiesPage->XSTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromSlot |= 
     capabilitiesPage->XSTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromSlot |= 
     capabilitiesPage->XSTtoDT ? CHANGER_TO_DRIVE : 0;

    changerParameters->ExchangeFromIePort = 0;

    changerParameters->ExchangeFromDrive = 
     capabilitiesPage->XDTtoMT ? CHANGER_TO_TRANSPORT : 0;
    changerParameters->ExchangeFromDrive |= 
     capabilitiesPage->XDTtoST ? CHANGER_TO_SLOT : 0;
    changerParameters->ExchangeFromDrive |= 
     capabilitiesPage->XDTtoDT ? CHANGER_TO_DRIVE : 0;

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

    if ((setAccess->Element.ElementType == ChangerKeypad) || 
        (setAccess->Element.ElementType == ChangerIEPort)) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if ((controlOperation == EXTEND_IEPORT) ||
        (controlOperation == RETRACT_IEPORT)) { 
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;


    if ((controlOperation == LOCK_ELEMENT) || 
        (controlOperation == UNLOCK_ELEMENT)) {

        srb->CdbLength = CDB6GENERIC_LENGTH;
        cdb->MEDIA_REMOVAL.OperationCode = SCSIOP_MEDIUM_REMOVAL;

        srb->DataBuffer = NULL;
        srb->DataTransferLength = 0;
        srb->TimeOutValue = 10;

        if (controlOperation == LOCK_ELEMENT) {
            cdb->MEDIA_REMOVAL.Prevent = 1;
        } else if (controlOperation == UNLOCK_ELEMENT) {
            cdb->MEDIA_REMOVAL.Prevent = 0;
        }
    }

    if (NT_SUCCESS(status)) {
        status = ChangerClassSendSrbSynchronous(DeviceObject,
                                             srb,
                                             srb->DataBuffer,
                                             srb->DataTransferLength,
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

    PFUNCTIONAL_DEVICE_EXTENSION fdoExtension =DeviceObject->DeviceExtension;
    PCHANGER_DATA     changerData = (PCHANGER_DATA)(fdoExtension->CommonExtension.DriverData);
    PCHANGER_ADDRESS_MAPPING     addressMapping = &(changerData->AddressMapping);
    PCHANGER_READ_ELEMENT_STATUS readElementStatus = Irp->AssociatedIrp.SystemBuffer;
    PCHANGER_ELEMENT_STATUS      elementStatus;
    PCHANGER_ELEMENT    element;
    ELEMENT_TYPE        elementType;
    PSCSI_REQUEST_BLOCK srb;
    PCDB     cdb;
    ULONG    length;
    ULONG    numberElements;
    ULONG    statusPages;
    NTSTATUS status;
    PVOID    statusBuffer;
    BOOLEAN  tagInfo;
    PIO_STACK_LOCATION     irpStack = IoGetCurrentIrpStackLocation(Irp);
    ULONG    outputBuffLen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    // Get the element type.
    elementType = readElementStatus->ElementList.Element.ElementType;
    element = &readElementStatus->ElementList.Element;
    numberElements = readElementStatus->ElementList.NumberOfElements;
    tagInfo = readElementStatus->VolumeTagInfo;

    if (elementType == AllElements) {
        statusPages = 4;
        if ((changerData->DriveID == Q47) && 
            ((changerData->InquiryData.VendorSpecific[19] & 0x2) == 0x2)) {
    
            numberElements++;   // if ALL requested, and the app thinks there
                                // are no IEPORTs, we must set aside a buffer
                                // for the IEPORT element and hide that 
                                // information when it gets passed back to 
                                // the application
        }
    } else {
        statusPages = 1;
    } 

    length = sizeof(ELEMENT_STATUS_HEADER) + 
            (statusPages * sizeof(ELEMENT_STATUS_PAGE));

    if (tagInfo) {
        length += (BHT_FULL_SIZE * numberElements);
    } else {
        length += (BHT_PARTIAL_SIZE * numberElements);
    }

    statusBuffer = ChangerClassAllocatePool(NonPagedPoolCacheAligned, length);
    if (!statusBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(statusBuffer, length);

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        ChangerClassFreePool(statusBuffer);
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
    cdb->READ_ELEMENT_STATUS.VolTag = tagInfo;

    cdb->READ_ELEMENT_STATUS.StartingElementAddress[0] =
        (UCHAR)((element->ElementAddress + 
        addressMapping->FirstElement[element->ElementType]) >> 8);
    cdb->READ_ELEMENT_STATUS.StartingElementAddress[1] =
        (UCHAR)((element->ElementAddress + 
        addressMapping->FirstElement[element->ElementType]) & 0xFF);

    cdb->READ_ELEMENT_STATUS.NumberOfElements[0] =         
        (UCHAR)(numberElements >> 8);
    cdb->READ_ELEMENT_STATUS.NumberOfElements[1] =      
        (UCHAR)(numberElements & 0xFF);

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
    if (NT_SUCCESS(status) || (status == STATUS_DATA_OVERRUN)) {

        PELEMENT_STATUS_HEADER statusHeader = statusBuffer;
        PELEMENT_STATUS_PAGE statusPage;
        PBHT_ED elementDescriptor;
        LONG remainingElements;
        LONG typeCount;
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

        if (elementType == AllElements) {
            if ((changerData->DriveID == Q47) && 
                ((changerData->InquiryData.VendorSpecific[19] & 0x2) == 0x2)) {
    
                numberElements--;       
                                // Since this was incremented before to 
                                // create a RdElemStatus call that includes
                                // the IEPORT, it must be take out at this point
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

        //
        // Fill in user buffer.
        //
        elementStatus = Irp->AssociatedIrp.SystemBuffer;
        RtlZeroMemory(elementStatus, outputBuffLen);

        do {
            // hide the IEPORT from the application
            for (i = 0; i < typeCount; i++, remainingElements--) {

               // don't return IEPORT info to host
               if (elementType != ChangerIEPort) {

                    // Get the address for this element.
                    elementStatus->Element.ElementAddress =
                     elementDescriptor->BHT_FED.ElementAddress[1];
                    elementStatus->Element.ElementAddress |=
                     (elementDescriptor->BHT_FED.ElementAddress[0] << 8);

                    // Account for address mapping.
                    elementStatus->Element.ElementAddress -= 
                    addressMapping->FirstElement[elementType];

                    // Set the element type.
                    elementStatus->Element.ElementType = elementType;

                    if (elementDescriptor->BHT_FED.SValid) {

                        ULONG  j;
                        USHORT tmpAddress;

                        // Source address is valid. 
                        // Determine the device specific address.
                        tmpAddress = elementDescriptor->BHT_FED.SourceStorageElementAddress[1];
                        tmpAddress |= (elementDescriptor->BHT_FED.SourceStorageElementAddress[0] << 8);

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
                     elementDescriptor->BHT_FED.Full;
                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.Exception << 2);
                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.Accessible << 3);

                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.LunValid << 12);
                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.IdValid << 13);
                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.NotThisBus << 15);

                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.Invert << 22);
                    elementStatus->Flags |= 
                     (elementDescriptor->BHT_FED.SValid << 23);


                    elementStatus->ExceptionCode = 
                     MapExceptionCodes(elementDescriptor);

                    if (elementDescriptor->BHT_FED.IdValid) {
                        elementStatus->TargetId = 
                         elementDescriptor->BHT_FED.BusAddress;
                    }
                    if (elementDescriptor->BHT_FED.LunValid) {
                        elementStatus->Lun = elementDescriptor->BHT_FED.Lun;
                    }

                    if (tagInfo) {
                       RtlMoveMemory(elementStatus->PrimaryVolumeID, 
                            elementDescriptor->BHT_FED.PrimaryVolumeTag, 
                            MAX_VOLUME_ID_SIZE);
                       elementStatus->Flags |= ELEMENT_STATUS_PVOLTAG;
                    }

                    // Advance to the next entry in the user 
                    // buffer and element descriptor array.
                    elementStatus += 1;
               }
                 
               // even for IEPORT, walk across its data
               // Get next descriptor of this same element type.
               (ULONG_PTR)elementDescriptor += descriptorLength;

            } // end of loop for this element type

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


    if (initElementStatus->ElementList.Element.ElementType != AllElements) {
        return STATUS_INVALID_PARAMETER;
    }

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);
    cdb = (PCDB)srb->Cdb;

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
         (setPosition->Destination.ElementType == ChangerDoor)) {
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

    None of the exabyte units support exchange medium.

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

    //
    // Verify transport, source, and dest. are within range.
    // Convert from 0-based to device-specific addressing.
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


    if (moveMedium->Flip) {
        return STATUS_INVALID_PARAMETER;
    }

    srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);
    if (!srb) {
        return STATUS_INSUFFICIENT_RESOURCES;
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
        addressMapping->FirstElement[i] = BHT_NO_ELEMENT;
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

        addressMapping->NumberOfElements[ChangerIEPort] = 0;

        addressMapping->NumberOfElements[ChangerSlot] = 
         elementAddressPage->NumberStorageElements[1];
        addressMapping->NumberOfElements[ChangerSlot] |= 
         (elementAddressPage->NumberStorageElements[0] << 8);

        addressMapping->NumberOfElements[ChangerDoor] = 1;
        addressMapping->NumberOfElements[ChangerKeypad] = 0;

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
    IN PBHT_ED ElementDescriptor
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
    UCHAR asc = ElementDescriptor->BHT_FED.AdditionalSenseCode;
    UCHAR asq = ElementDescriptor->BHT_FED.AddSenseCodeQualifier;
    ULONG exceptionCode;

    switch (asc) {

        case 0x83:
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

                default:
                    exceptionCode = ERROR_UNHANDLED_ERROR;

            }
                        break; // 0x83

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
    } else if (AddressMap->FirstElement[ElementType] == BHT_NO_ELEMENT) {

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
   // BREECE_DEVICE_PROBLEM_NONE. If the changer returns sense code
   // SCSI_SENSE_HARDWARE_ERROR on SelfTest, we'll set an appropriate 
   // devicestatus.
   //
   changerData->DeviceStatus = BREECE_DEVICE_PROBLEM_NONE;

   changerDeviceError->ChangerProblemType = DeviceProblemNone;

   srb = ChangerClassAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

   if (srb == NULL) {
      DebugPrint((1, "BREECEMC\\ChangerPerformDiagnostics : No memory\n"));
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

   cdb->CDB6GENERIC.CommandUniqueBits = 0x2;

   status =  ChangerClassSendSrbSynchronous(DeviceObject,
                                     srb,
                                     srb->DataBuffer,
                                     srb->DataTransferLength,
                                     FALSE);
   if (NT_SUCCESS(status)) {
      changerDeviceError->ChangerProblemType = DeviceProblemNone;
   } else if ((changerData->DeviceStatus) != BREECE_DEVICE_PROBLEM_NONE) {
      switch (changerData->DeviceStatus) {
         case BREECE_HW_ERROR: {
            changerDeviceError->ChangerProblemType = DeviceProblemHardware;
            break;
         }

         case BREECE_CHM_ERROR: {
            changerDeviceError->ChangerProblemType = DeviceProblemCHMError;
            break;
         }

         case BREECE_DRIVE_ERROR: {
            changerDeviceError->ChangerProblemType = DeviceProblemDriveError;
            break;
         }

         case BREECE_CHM_MOVE_ERROR: {
            changerDeviceError->ChangerProblemType = DeviceProblemCHMMoveError;
            break;
         }

         default: {
            changerDeviceError->ChangerProblemType = DeviceProblemHardware;
            break;
         }
      } // switch (changerData->DeviceStatus)
   }

   ChangerClassFreePool(srb);
   return status;
}
