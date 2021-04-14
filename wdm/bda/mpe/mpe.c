///////////////////////////////////////////////////////////////////////////
//
//
// Copyright (c) 1996, 1997  Microsoft Corporation
//
//
// Module Name:
//      mpe.c
//
// Abstract:
//
//
// Author:
//
//      P Porzuczek
//
// Environment:
//
// Revision History:
//
//
//////////////////////////////////////////////////////////////////////////////

#include <wdm.h>
#include <strmini.h>
#include <ksmedia.h>
#include <BdaTypes.h>
#include <BdaMedia.h>

#include "Mpe.h"
#include "MpeMedia.h"
#include "MpeStream.h"

#include "Main.h"
#include "filter.h"


#pragma pack (1)

typedef struct
{
    BYTE   table_id;
    USHORT section_syntax_indicator : 1;
    USHORT private_indicator: 1;
    USHORT reserved1: 2;
    USHORT section_length: 12;
    BYTE   MAC_address_6;
    BYTE   MAC_address_5;
    BYTE   reserved2 : 2;
    BYTE   payload_scrambling : 2;
    BYTE   address_scrambling : 2;
    BYTE   LLC_SNAP_flag : 1;
    BYTE   current_next_indicator : 1;
    BYTE   section_number;
    BYTE   last_section_number;
    BYTE   MAC_address_4;
    BYTE   MAC_address_3;
    BYTE   MAC_address_2;
    BYTE   MAC_address_1;
    BYTE   Data [0];

} SECTION_HEADER, *PSECTION_HEADER;

typedef struct
{
    BYTE   dsap;
    BYTE   ssap;
    BYTE   cntl;
    BYTE   org [3];
    USHORT type;
    BYTE Data [0];

} LLC_SNAP, *PLLC_SNAP;


typedef struct
{
    BYTE MAC_Dest_Address [6];
    BYTE MAC_Src_Address [6];
    USHORT usLength;

} MAC_Address, *PMAC_Address;

typedef struct _HEADER_IP_
{
    UCHAR  ucVersion_Length;
    UCHAR  ucTOS;
    USHORT usLength;
    USHORT usId;
    USHORT usFlags_Offset;
    UCHAR  ucTTL;
    UCHAR  ucProtocol;
    USHORT usHdrChecksum;
    UCHAR  ucSrcAddress [4];
    UCHAR  ucDestAddress [4];

} HEADER_IP, *PHEADER_IP;


#pragma pack ()



#define ES2(s) ((((s) >> 8) & 0x00FF) + (((s) << 8) & 0xFF00))


#ifdef DBG

//////////////////////////////////////////////////////////////////////////////
VOID
DumpData (
    PUCHAR pData,
    ULONG  ulSize
    )
//////////////////////////////////////////////////////////////////////////////
{
  ULONG  ulCount;
  ULONG  ul;
  UCHAR  uc;

  while (ulSize)
  {
      ulCount = 16 < ulSize ? 16 : ulSize;

      for (ul = 0; ul < ulCount; ul++)
      {
          uc = *pData;

          TEST_DEBUG (TEST_DBG_RECV, ("%02X ", uc));
          ulSize -= 1;
          pData  += 1;
      }

      TEST_DEBUG (TEST_DBG_RECV, ("\n"));
  }

}

#endif   //DBG



//////////////////////////////////////////////////////////////////////////////
BOOLEAN
ValidSection (
    PSECTION_HEADER pSection
    )
//////////////////////////////////////////////////////////////////////////////
{
    if (pSection->table_id != 0x3E)
    {
        return FALSE;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
BOOLEAN
ValidSnap (
    PLLC_SNAP pSnap
    )
//////////////////////////////////////////////////////////////////////////////
{

    if (pSnap->dsap != 0xAA)
    {
        return FALSE;
    }

    if (pSnap->ssap != 0xAA)
    {
        return FALSE;
    }

    if (pSnap->cntl != 0x03)
    {
        return FALSE;
    }

    if (pSnap->type != 0x0800)
    {
        return FALSE;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
VOID
NormalizeSection (
    PBYTE pStream,
    PSECTION_HEADER pSection
    )
//////////////////////////////////////////////////////////////////////////////
{
    PBYTE   pb = pStream;
    PUSHORT ps = (PUSHORT) pStream;

    if (pSection)
    {
        pSection->table_id = *pb;

        pb += 1;
        pSection->section_syntax_indicator = (*pb >> 7) & 0x01;
        pSection->private_indicator = (*pb >> 6 )& 0x01;
        pSection->reserved1 = (*pb >> 4) & 0x03;

        ps = (PUSHORT) pb;
        pSection->section_length = ES2 (*ps) & 0x0FFF;

        pb += 2;
        pSection->MAC_address_6 = *pb;

        pb += 1;
        pSection->MAC_address_5 = *pb;

        pb += 1;
        pSection->reserved2 = (*pb >> 6) & 0x03;
        pSection->payload_scrambling = (*pb >> 4) & 0x3;
        pSection->address_scrambling = (*pb >> 2) & 0x3;
        pSection->LLC_SNAP_flag = (*pb >> 1) & 0x01;
        pSection->current_next_indicator = *pb & 0x01;

        pb += 1;
        pSection->section_number = *pb;

        pb += 1;
        pSection->last_section_number = *pb;

        pb += 1;
        pSection->MAC_address_4 = *pb;

        pb += 1;
        pSection->MAC_address_3 = *pb;

        pb += 1;
        pSection->MAC_address_2 = *pb;

        pb += 1;
        pSection->MAC_address_1 = *pb;

    }

    return;

}

//////////////////////////////////////////////////////////////////////////////
VOID
NormalizeSnap (
    PBYTE pStream,
    PLLC_SNAP pSnap
    )
//////////////////////////////////////////////////////////////////////////////
{
    PUSHORT ps = (PUSHORT) pStream;

    if (pSnap)
    {
        pSnap->type = ES2 (pSnap->type);
    }

    return;

}

//////////////////////////////////////////////////////////////////////////////
//
//
VOID
DumpDataFormat (
    PKSDATAFORMAT   pF
    );


//////////////////////////////////////////////////////////////////////////////
VOID
MpeGetConnectionProperty(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PSTREAM pStream                     = (PSTREAM)pSrb->StreamObject->HwStreamExtension;
    PSTREAM_PROPERTY_DESCRIPTOR pSPD    = pSrb->CommandData.PropertyInfo;
    ULONG Id                            = pSPD->Property->Id;                // index of the property
    ULONG ulStreamNumber                = pSrb->StreamObject->StreamNumber;

    pSrb->ActualBytesTransferred = 0;

    switch (Id)
    {
        case KSPROPERTY_CONNECTION_ALLOCATORFRAMING:
        {
            PKSALLOCATOR_FRAMING Framing = (PKSALLOCATOR_FRAMING) pSPD->PropertyInfo;

            Framing->RequirementsFlags   = KSALLOCATOR_REQUIREMENTF_SYSTEM_MEMORY    |
                                           KSALLOCATOR_REQUIREMENTF_INPLACE_MODIFIER |
                                           KSALLOCATOR_REQUIREMENTF_PREFERENCES_ONLY;

            Framing->PoolType            = NonPagedPool;
            Framing->Frames              = 0;
            Framing->FrameSize           = 0;
            Framing->FileAlignment       = 0;         // None OR FILE_QUAD_ALIGNMENT-1 OR PAGE_SIZE-1;
            Framing->Reserved            = 0;

            switch (ulStreamNumber)
            {
                case MPE_IPV4:
                    Framing->Frames    = 16;
                    Framing->FrameSize = pStream->OpenedFormat.SampleSize;
                    pSrb->Status = STATUS_SUCCESS;
                    break;

                case MPE_STREAM:
                    Framing->Frames    = 32;
                    Framing->FrameSize = pStream->OpenedFormat.SampleSize;
                    pSrb->Status = STATUS_SUCCESS;
                    break;

                default:
                    pSrb->Status = STATUS_NOT_IMPLEMENTED;
                    break;
            }

            pSrb->ActualBytesTransferred = sizeof (KSALLOCATOR_FRAMING);
        }
        break;

        default:
            pSrb->Status = STATUS_NOT_IMPLEMENTED;
            break;
    }


    return;
}


//////////////////////////////////////////////////////////////////////////////
NTSTATUS
MpeDriverInitialize (
    IN PDRIVER_OBJECT    DriverObject,
    IN PUNICODE_STRING   RegistryPath
    )
//////////////////////////////////////////////////////////////////////////////
{
    NTSTATUS ntStatus                        = STATUS_SUCCESS;
    HW_INITIALIZATION_DATA   HwInitData;
    UNICODE_STRING           DeviceNameString;
    UNICODE_STRING           SymbolicNameString;

    RtlZeroMemory(&HwInitData, sizeof(HwInitData));
    HwInitData.HwInitializationDataSize = sizeof(HwInitData);


    ////////////////////////////////////////////////////////////////
    //
    // Setup the stream class dispatch table
    //
    HwInitData.HwInterrupt                 = NULL; // HwInterrupt is only for HW devices

    HwInitData.HwReceivePacket             = CodecReceivePacket;
    HwInitData.HwCancelPacket              = CodecCancelPacket;
    HwInitData.HwRequestTimeoutHandler     = CodecTimeoutPacket;

    HwInitData.DeviceExtensionSize         = sizeof(MPE_FILTER);
    HwInitData.PerRequestExtensionSize     = sizeof(SRB_EXTENSION);
    HwInitData.FilterInstanceExtensionSize = 0;
    HwInitData.PerStreamExtensionSize      = sizeof(STREAM);
    HwInitData.BusMasterDMA                = FALSE;
    HwInitData.Dma24BitAddresses           = FALSE;
    HwInitData.BufferAlignment             = 3;
    HwInitData.TurnOffSynchronization      = TRUE;
    HwInitData.DmaBufferSize               = 0;


    ntStatus = StreamClassRegisterAdapter (DriverObject, RegistryPath, &HwInitData);
    if (ntStatus != STATUS_SUCCESS)
    {
        goto ret;
    }

ret:

    return ntStatus;
}


//
//
//////////////////////////////////////////////////////////////////////////////
BOOLEAN
CodecInitialize (
    IN OUT PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    NTSTATUS ntStatus                           = STATUS_SUCCESS;
    BOOLEAN bStatus                             = FALSE;
    PPORT_CONFIGURATION_INFORMATION pConfigInfo = pSrb->CommandData.ConfigInfo;
    PMPE_FILTER pFilter                      = (PMPE_FILTER) pConfigInfo->HwDeviceExtension;

    //
    // Define the default return codes
    //
    pSrb->Status = STATUS_SUCCESS;
    bStatus = TRUE;

    //
    // Check out init flag so we don't try to init more then once.  The Streaming
    // Class driver appears to call the init handler several times for some reason.
    //
    if (pFilter->bInitializationComplete)
    {
        goto ret;
    }

    //
    // Initialize Statistics block
    //
    RtlZeroMemory(&pFilter->Stats, sizeof (STATS));


    if (pConfigInfo->NumberOfAccessRanges == 0)
    {
        pConfigInfo->StreamDescriptorSize = sizeof (HW_STREAM_HEADER) +
            DRIVER_STREAM_COUNT * sizeof (HW_STREAM_INFORMATION);

    }
    else
    {
        pSrb->Status = STATUS_NO_SUCH_DEVICE;
        bStatus = FALSE;
        goto ret;
    }


    //
    // Create a filter object to represent our context
    //
    pSrb->Status = CreateFilter (pConfigInfo->ClassDeviceObject->DriverObject, pConfigInfo->ClassDeviceObject, pFilter);
    if (pSrb->Status != STATUS_SUCCESS)
    {
        bStatus = FALSE;
        goto ret;
    }

    pFilter->bInitializationComplete = TRUE;

ret:

    return (bStatus);
}


//////////////////////////////////////////////////////////////////////////////
BOOLEAN
CodecUnInitialize (
    IN OUT PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    NTSTATUS ntStatus                           = STATUS_SUCCESS;
    BOOLEAN bStatus                             = FALSE;
    PPORT_CONFIGURATION_INFORMATION pConfigInfo = pSrb->CommandData.ConfigInfo;
    PMPE_FILTER pFilter                      = ((PMPE_FILTER)pSrb->HwDeviceExtension);
    PSTREAM pStream                             = NULL;


    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Codec Unitialize called\n"));

    if (pSrb->StreamObject != NULL)
    {
        pStream = (PSTREAM)pSrb->StreamObject->HwStreamExtension;
    }

    if (pStream)
    {

        //
        // Clean up the NAB_STREAM QUEUE used for deframing
        //
        //$$BUG
        //DeleteNabStreamQueue (pFilter);

        //
        // Clean up any queues we have and complete any outstanding SRB's
        //
        while (QueueRemove (&pSrb, &pFilter->StreamUserSpinLock, &pFilter->StreamContxList))
        {
            pSrb->Status = STATUS_CANCELLED;
            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
            TEST_DEBUG( TEST_DBG_SRB, ("MPE 5Completed SRB %08X\n", pSrb));

        }

        while (QueueRemove (&pSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue))
        {
            pSrb->Status = STATUS_CANCELLED;
            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
            TEST_DEBUG( TEST_DBG_SRB, ("MPE 6Completed SRB %08X\n", pSrb));
        }


        while (QueueRemove (&pSrb, &pFilter->StreamDataSpinLock, &pFilter->StreamDataQueue))
        {
            pSrb->Status = STATUS_CANCELLED;
            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
            TEST_DEBUG( TEST_DBG_SRB, ("MPE 7Completed SRB %08X\n", pSrb));
        }


        while (QueueRemove (&pSrb, &pFilter->StreamControlSpinLock, &pFilter->StreamControlQueue))
        {
            pSrb->Status = STATUS_CANCELLED;
            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
            TEST_DEBUG( TEST_DBG_SRB, ("MPE 8Completed SRB %08X\n", pSrb));
        }

    }


    while (QueueRemove (&pSrb, &pFilter->AdapterSRBSpinLock, &pFilter->AdapterSRBQueue))
    {
        pSrb->Status = STATUS_CANCELLED;
        StreamClassDeviceNotification (DeviceRequestComplete, pSrb->StreamObject, pSrb);
        TEST_DEBUG( TEST_DBG_RECV, ("MPE 9Completed SRB %08X\n", pSrb));
    }


    bStatus = TRUE;

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Codec Unitialize completed\n"));

    return (bStatus);
}


//////////////////////////////////////////////////////////////////////////////
VOID
CodecStreamInfo (
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    int j;

    PMPE_FILTER pFilter =
            ((PMPE_FILTER)pSrb->HwDeviceExtension);

    //
    // pick up the pointer to header which preceeds the stream info structs
    //
    PHW_STREAM_HEADER pstrhdr =
            (PHW_STREAM_HEADER)&(pSrb->CommandData.StreamBuffer->StreamHeader);

    //
    // pick up the pointer to the array of stream information data structures
    //
    PHW_STREAM_INFORMATION pstrinfo =
            (PHW_STREAM_INFORMATION)&(pSrb->CommandData.StreamBuffer->StreamInfo);


    //
    // Set the header
    //
    StreamHeader.NumDevPropArrayEntries = 0;
    StreamHeader.DevicePropertiesArray = (PKSPROPERTY_SET)NULL;

    *pstrhdr = StreamHeader;

    //
    // stuff the contents of each HW_STREAM_INFORMATION struct
    //
    for (j = 0; j < DRIVER_STREAM_COUNT; j++)
    {
       *pstrinfo++ = Streams[j].hwStreamInfo;
    }

    pSrb->Status = STATUS_SUCCESS;

}


//////////////////////////////////////////////////////////////////////////////
VOID
STREAMAPI
CodecCancelPacket(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PSTREAM  pStream = (PSTREAM)pSrb->StreamObject->HwStreamExtension;
    PMPE_FILTER  pFilter = ((PMPE_FILTER)pSrb->HwDeviceExtension);

    //
    // Check whether the SRB to cancel is in use by this stream
    //

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: CancelPacket Called\n"));

    //
    //$$BUG
    //
    //CancelNabStreamSrb (pFilter, pSrb);


    if (QueueRemoveSpecific (pSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue))
    {
        pSrb->Status = STATUS_CANCELLED;
        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 10Completed SRB %08X\n", pSrb));
        return;
    }


    if (QueueRemoveSpecific (pSrb, &pFilter->StreamDataSpinLock, &pFilter->StreamDataQueue))
    {
        pSrb->Status = STATUS_CANCELLED;
        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 11Completed SRB %08X\n", pSrb));
        return;
    }


    if (QueueRemoveSpecific (pSrb, &pFilter->StreamControlSpinLock, &pFilter->StreamControlQueue))
    {
        pSrb->Status = STATUS_CANCELLED;
        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 12Completed SRB %08X\n", pSrb));
        return;
    }

    if (QueueRemoveSpecific (pSrb, &pFilter->AdapterSRBSpinLock, &pFilter->AdapterSRBQueue))
    {
        pSrb->Status = STATUS_CANCELLED;
        StreamClassDeviceNotification (DeviceRequestComplete, pSrb->StreamObject, pSrb);
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 13Completed SRB %08X\n", pSrb));
        return;
    }

    return;
}


//////////////////////////////////////////////////////////////////////////////
VOID
STREAMAPI
CodecTimeoutPacket(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    //
    // if we timeout while playing, then we need to consider this
    // condition an error, and reset the hardware, and reset everything
    // as well as cancelling this and all requests
    //

    //
    // if we are not playing, and this is a CTRL request, we still
    // need to reset everything as well as cancelling this and all requests
    //

    //
    // if this is a data request, and the device is paused, we probably have
    // run out of data buffer, and need more time, so just reset the timer,
    // and let the packet continue
    //

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: TimeoutPacket Called\n"));

    pSrb->TimeoutCounter = 0;

    return;
}


//////////////////////////////////////////////////////////////////////////////
VOID
STREAMAPI
CodecReceivePacket(
    IN PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PMPE_FILTER pFilter = ((PMPE_FILTER)pSrb->HwDeviceExtension);


    //
    // Make sure queue & SL initted
    //
    if (!pFilter->bAdapterQueueInitialized)
    {
        InitializeListHead (&pFilter->AdapterSRBQueue);
        KeInitializeSpinLock (&pFilter->AdapterSRBSpinLock);
        pFilter->bAdapterQueueInitialized = TRUE;
    }

    //
    // Assume success
    //
    pSrb->Status = STATUS_SUCCESS;

    //
    // determine the type of packet.
    //
    QueueAdd (pSrb, &pFilter->AdapterSRBSpinLock, &pFilter->AdapterSRBQueue);
    TEST_DEBUG( TEST_DBG_SRB, ("MPE Queuing SRB %08X\n", pSrb));


    while (QueueRemove( &pSrb, &pFilter->AdapterSRBSpinLock, &pFilter->AdapterSRBQueue ))
    {
        switch (pSrb->Command)
        {

            case SRB_INITIALIZE_DEVICE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_INITIALIZE Command\n"));
                CodecInitialize(pSrb);
                break;

            case SRB_UNINITIALIZE_DEVICE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_UNINITIALIZE Command\n"));
                CodecUnInitialize(pSrb);
                break;

            case SRB_INITIALIZATION_COMPLETE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_INITIALIZE_COMPLETE Command\n"));
                pSrb->Status = STATUS_SUCCESS;
                break;

            case SRB_OPEN_STREAM:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_OPEN_STREAM Command\n"));
                OpenStream (pSrb);
                break;

            case SRB_CLOSE_STREAM:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_CLOSE_STREAM Command\n"));
                CloseStream (pSrb);
                break;

            case SRB_GET_STREAM_INFO:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_GET_STREAM_INFO Command\n"));
                CodecStreamInfo (pSrb);
                break;

            case SRB_GET_DATA_INTERSECTION:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_GET_DATA_INTERSECTION Command\n"));

                //
                // Compare our stream formats.  NOTE, the compare functions sets the SRB
                // status fields
                //
                CompareStreamFormat (pSrb);
                break;

            case SRB_OPEN_DEVICE_INSTANCE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_OPEN_DEVICE_INSTANCE Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_CLOSE_DEVICE_INSTANCE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_CLOSE_DEVICE_INSTANCE Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_UNKNOWN_DEVICE_COMMAND:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_UNKNOWN_DEVICE Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_CHANGE_POWER_STATE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_CHANGE_POWER_STATE Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_GET_DEVICE_PROPERTY:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_GET_DEVICE_PROPERTY Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_SET_DEVICE_PROPERTY:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_SET_DEVICE_PROPERTY Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_UNKNOWN_STREAM_COMMAND:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_UNKNOWN Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            default:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_DEFAULT Command\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

        };


        //
        // NOTE:
        //
        // All of the commands that we do, or do not understand can all be completed
        // syncronously at this point, so we can use a common callback routine here.
        // If any of the above commands require asyncronous processing, this will
        // have to change
        //

        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB Status returned: %08X\n", pSrb->Status));

        StreamClassDeviceNotification (DeviceRequestComplete, pFilter, pSrb);
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 14Completed SRB %08X\n", pSrb));

    }




}


//////////////////////////////////////////////////////////////////////////////
BOOL STREAMAPI
QueueAdd (
    IN PHW_STREAM_REQUEST_BLOCK pSrb,
    IN PKSPIN_LOCK pQueueSpinLock,
    IN PLIST_ENTRY pQueue
    )
//////////////////////////////////////////////////////////////////////////////
{
    KIRQL           Irql;
    PSRB_EXTENSION  pSrbExtension;

    pSrbExtension = ( PSRB_EXTENSION )pSrb->SRBExtension;

    KeAcquireSpinLock( pQueueSpinLock, &Irql );

    pSrbExtension->pSrb = pSrb;
    InsertTailList( pQueue, &pSrbExtension->ListEntry );

    KeReleaseSpinLock( pQueueSpinLock, Irql );

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
BOOL STREAMAPI
QueuePush (
    IN PHW_STREAM_REQUEST_BLOCK pSrb,
    IN PKSPIN_LOCK pQueueSpinLock,
    IN PLIST_ENTRY pQueue
    )
//////////////////////////////////////////////////////////////////////////////
{
    KIRQL           Irql;
    PSRB_EXTENSION  pSrbExtension;

    pSrbExtension = ( PSRB_EXTENSION )pSrb->SRBExtension;

    KeAcquireSpinLock( pQueueSpinLock, &Irql );

    pSrbExtension->pSrb = pSrb;
    InsertHeadList( pQueue, &pSrbExtension->ListEntry );

    KeReleaseSpinLock( pQueueSpinLock, Irql );

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
BOOL STREAMAPI
QueueAddIfNotEmpty (
    IN PHW_STREAM_REQUEST_BLOCK pSrb,
    IN PKSPIN_LOCK pQueueSpinLock,
    IN PLIST_ENTRY pQueue
    )
//////////////////////////////////////////////////////////////////////////////
{
   KIRQL           Irql;
   PSRB_EXTENSION  pSrbExtension;
   BOOL            bAddedSRB = FALSE;

   pSrbExtension = ( PSRB_EXTENSION )pSrb->SRBExtension;

   KeAcquireSpinLock( pQueueSpinLock, &Irql );

   if( !IsListEmpty( pQueue ))
   {
       pSrbExtension->pSrb = pSrb;
       InsertTailList (pQueue, &pSrbExtension->ListEntry );
       bAddedSRB = TRUE;
   }

   KeReleaseSpinLock( pQueueSpinLock, Irql );

   return bAddedSRB;
}

//////////////////////////////////////////////////////////////////////////////
BOOL STREAMAPI
QueueRemove (
    IN OUT PHW_STREAM_REQUEST_BLOCK * pSrb,
    IN PKSPIN_LOCK pQueueSpinLock,
    IN PLIST_ENTRY pQueue
    )
//////////////////////////////////////////////////////////////////////////////
{
   KIRQL    Irql;
   BOOL     bRemovedSRB = FALSE;

   KeAcquireSpinLock (pQueueSpinLock, &Irql);

   *pSrb =  (PHW_STREAM_REQUEST_BLOCK) NULL;

   if( !IsListEmpty( pQueue ))
   {
       PHW_STREAM_REQUEST_BLOCK *pCurrentSrb = NULL;
       PUCHAR Ptr                            = (PUCHAR) RemoveHeadList(pQueue);

       pCurrentSrb = (PHW_STREAM_REQUEST_BLOCK *) (((PUCHAR)Ptr) + sizeof (LIST_ENTRY));

       *pSrb = *pCurrentSrb;
       bRemovedSRB = TRUE;

   }

   KeReleaseSpinLock (pQueueSpinLock, Irql);

   return bRemovedSRB;
}

//////////////////////////////////////////////////////////////////////////////
BOOL STREAMAPI
QueueRemoveSpecific (
    IN PHW_STREAM_REQUEST_BLOCK pSrb,
    IN PKSPIN_LOCK pQueueSpinLock,
    IN PLIST_ENTRY pQueue
    )
//////////////////////////////////////////////////////////////////////////////
{
   KIRQL Irql;
   BOOL  bRemovedSRB = FALSE;
   PLIST_ENTRY pCurrentEntry;
   PHW_STREAM_REQUEST_BLOCK * pCurrentSrb;

   KeAcquireSpinLock( pQueueSpinLock, &Irql );

   if( !IsListEmpty( pQueue ))
   {
       pCurrentEntry = pQueue->Flink;
       while ((pCurrentEntry != pQueue ) && !bRemovedSRB)
       {
           pCurrentSrb = (PHW_STREAM_REQUEST_BLOCK * ) ((( PUCHAR )pCurrentEntry ) + sizeof( LIST_ENTRY ));

           if( *pCurrentSrb == pSrb )
           {
               RemoveEntryList( pCurrentEntry );
               bRemovedSRB = TRUE;
           }
           pCurrentEntry = pCurrentEntry->Flink;
       }
   }
   KeReleaseSpinLock( pQueueSpinLock, Irql );

   return bRemovedSRB;
}

//////////////////////////////////////////////////////////////////////////////
NTSTATUS
StreamIPIndicateEvent (
    PVOID pvEvent
)
//////////////////////////////////////////////////////////////////////////////
{
    return STATUS_NOT_IMPLEMENTED;
}



//////////////////////////////////////////////////////////////////////////////
BOOL
CompareGUIDsAndFormatSize(
    IN PKSDATARANGE pDataRange1,
    IN PKSDATARANGE pDataRange2,
    BOOLEAN bCheckSize
    )
//////////////////////////////////////////////////////////////////////////////
{
    BOOL bResult = FALSE;

    if ( IsEqualGUID(&pDataRange1->MajorFormat, &KSDATAFORMAT_TYPE_WILDCARD) ||
         IsEqualGUID(&pDataRange2->MajorFormat, &KSDATAFORMAT_TYPE_WILDCARD) ||
         IsEqualGUID(&pDataRange1->MajorFormat, &pDataRange2->MajorFormat) )
    {

        if ( IsEqualGUID(&pDataRange1->SubFormat, &KSDATAFORMAT_SUBTYPE_WILDCARD) ||
             IsEqualGUID(&pDataRange2->SubFormat, &KSDATAFORMAT_SUBTYPE_WILDCARD) ||
             IsEqualGUID(&pDataRange1->SubFormat, &pDataRange2->SubFormat) )
        {

            if ( IsEqualGUID(&pDataRange1->Specifier, &KSDATAFORMAT_SPECIFIER_WILDCARD) ||
                 IsEqualGUID(&pDataRange2->Specifier, &KSDATAFORMAT_SPECIFIER_WILDCARD) ||
                 IsEqualGUID(&pDataRange1->Specifier, &pDataRange2->Specifier) )
            {
                if ( !bCheckSize || pDataRange1->FormatSize == pDataRange2->FormatSize)
                {
                    bResult = TRUE;
                }
            }
        }
    }

    return bResult;

}

//////////////////////////////////////////////////////////////////////////////
VOID
DumpDataFormat (
    PKSDATAFORMAT   pF
    )
//////////////////////////////////////////////////////////////////////////////
{
    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: DATA Format\n"));
    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     Format Size:   %08X\n", pF->FormatSize));
    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     Flags:         %08X\n", pF->Flags));
    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     SampleSize:    %08X\n", pF->SampleSize));
    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     Reserved:      %08X\n", pF->Reserved));



    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     Major GUID:  %08X %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                                                pF->MajorFormat.Data1,
                                                pF->MajorFormat.Data2,
                                                pF->MajorFormat.Data3,
                                                pF->MajorFormat.Data4[0],
                                                pF->MajorFormat.Data4[1],
                                                pF->MajorFormat.Data4[2],
                                                pF->MajorFormat.Data4[3],
                                                pF->MajorFormat.Data4[4],
                                                pF->MajorFormat.Data4[5],
                                                pF->MajorFormat.Data4[6],
                                                pF->MajorFormat.Data4[7]
                                ));

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     Sub GUID:    %08X %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                                                pF->SubFormat.Data1,
                                                pF->SubFormat.Data2,
                                                pF->SubFormat.Data3,
                                                pF->SubFormat.Data4[0],
                                                pF->SubFormat.Data4[1],
                                                pF->SubFormat.Data4[2],
                                                pF->SubFormat.Data4[3],
                                                pF->SubFormat.Data4[4],
                                                pF->SubFormat.Data4[5],
                                                pF->SubFormat.Data4[6],
                                                pF->SubFormat.Data4[7]
                                ));

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE:     Specifier:   %08X %04X %04X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                                                pF->Specifier.Data1,
                                                pF->Specifier.Data2,
                                                pF->Specifier.Data3,
                                                pF->Specifier.Data4[0],
                                                pF->Specifier.Data4[1],
                                                pF->Specifier.Data4[2],
                                                pF->Specifier.Data4[3],
                                                pF->Specifier.Data4[4],
                                                pF->Specifier.Data4[5],
                                                pF->Specifier.Data4[6],
                                                pF->Specifier.Data4[7]
                                ));

    TEST_DEBUG (TEST_DBG_TRACE, ("\n"));
}


//////////////////////////////////////////////////////////////////////////////
BOOL
CompareStreamFormat (
    IN PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    BOOL                        bStatus = FALSE;
    PSTREAM_DATA_INTERSECT_INFO pIntersectInfo;
    PKSDATARANGE                pDataRange1;
    PKSDATARANGE                pDataRange2;
    ULONG                       FormatSize = 0;
    ULONG                       ulStreamNumber;
    ULONG                       j;
    ULONG                       ulNumberOfFormatArrayEntries;
    PKSDATAFORMAT               *pAvailableFormats;


    pIntersectInfo = pSrb->CommandData.IntersectInfo;
    ulStreamNumber = pIntersectInfo->StreamNumber;


    pSrb->ActualBytesTransferred = 0;


    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Comparing Stream Formats\n"));


    //
    // Check that the stream number is valid
    //
    if (ulStreamNumber < DRIVER_STREAM_COUNT)
    {
        ulNumberOfFormatArrayEntries = Streams[ulStreamNumber].hwStreamInfo.NumberOfFormatArrayEntries;

        //
        // Get the pointer to the array of available formats
        //
        pAvailableFormats = Streams[ulStreamNumber].hwStreamInfo.StreamFormatsArray;

        //
        // Walk the formats supported by the stream searching for a match
        // of the three GUIDs which together define a DATARANGE
        //
        for (pDataRange1 = pIntersectInfo->DataRange, j = 0;
             j < ulNumberOfFormatArrayEntries;
             j++, pAvailableFormats++)

        {
            bStatus = FALSE;
            pSrb->Status = STATUS_UNSUCCESSFUL;

            pDataRange2 = *pAvailableFormats;

            if (CompareGUIDsAndFormatSize (pDataRange1, pDataRange2, TRUE))
            {

                ULONG   ulFormatSize = pDataRange2->FormatSize;

                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Stream Formats compare\n"));

                //
                // Is the caller trying to get the format, or the size of the format?
                //
                if (pIntersectInfo->SizeOfDataFormatBuffer == sizeof (ULONG))
                {
                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Returning Stream Format size\n"));

                    *(PULONG) pIntersectInfo->DataFormatBuffer = ulFormatSize;
                    pSrb->ActualBytesTransferred = sizeof (ULONG);
                    pSrb->Status = STATUS_SUCCESS;
                    bStatus = TRUE;
                }
                else
                {
                    //
                    // Verify that there is enough room in the supplied buffer for the whole thing
                    //
                    pSrb->Status = STATUS_BUFFER_TOO_SMALL;
                    bStatus = FALSE;

                    if (pIntersectInfo->SizeOfDataFormatBuffer >= ulFormatSize)
                    {
                        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Returning Stream Format\n"));
                        RtlCopyMemory (pIntersectInfo->DataFormatBuffer, pDataRange2, ulFormatSize);
                        pSrb->ActualBytesTransferred = ulFormatSize;
                        pSrb->Status = STATUS_SUCCESS;
                        bStatus = TRUE;
                    }
                    else
                    {
                        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Stream Format return buffer too small\n"));
                    }
                }
                break;
            }
            else
            {
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Stream Formats DO NOT compare\n"));
            }
        }

        if ( j >= ulNumberOfFormatArrayEntries )
        {
            pSrb->ActualBytesTransferred = 0;
            pSrb->Status = STATUS_UNSUCCESSFUL;
            bStatus = FALSE;
        }

    }
    else
    {
        pSrb->ActualBytesTransferred = 0;
        pSrb->Status = STATUS_NOT_IMPLEMENTED;
        bStatus = FALSE;
    }

    return bStatus;
}


//////////////////////////////////////////////////////////////////////////////
VOID
CloseStream (
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    //
    // the stream extension structure is allocated by the stream class driver
    //
    PSTREAM         pStream                = (PSTREAM)pSrb->StreamObject->HwStreamExtension;
    PMPE_FILTER  pFilter                = (PMPE_FILTER)pSrb->HwDeviceExtension;
    ULONG           ulStreamNumber         = (ULONG) pSrb->StreamObject->StreamNumber;
    ULONG           ulStreamInstance       = pStream->ulStreamInstance;
    PHW_STREAM_REQUEST_BLOCK pCurrentSrb   = NULL;

    //
    // check that the stream index requested isn't too high
    // or that the maximum number of instances hasn't been exceeded
    //
    if (ulStreamNumber < DRIVER_STREAM_COUNT )
    {
        //
        // Flush the stream data queue
        //
        while (QueueRemove( &pCurrentSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue))
        {
           pCurrentSrb->Status = STATUS_CANCELLED;
           StreamClassStreamNotification( StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
           TEST_DEBUG( TEST_DBG_SRB, ("MPE 15Completed SRB %08X\n", pCurrentSrb));
        }

        //
        // Flush the stream data queue
        //
        while (QueueRemove( &pCurrentSrb, &pFilter->StreamDataSpinLock, &pFilter->StreamDataQueue))
        {
           pCurrentSrb->Status = STATUS_CANCELLED;
           StreamClassStreamNotification( StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
           TEST_DEBUG( TEST_DBG_SRB, ("MPE 16Completed SRB %08X\n", pCurrentSrb));
        }

        //
        // Flush the stream control queue
        //
        while (QueueRemove( &pCurrentSrb, &pFilter->StreamControlSpinLock, &pFilter->StreamControlQueue))
        {
           pCurrentSrb->Status = STATUS_CANCELLED;
           StreamClassStreamNotification (StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
           TEST_DEBUG( TEST_DBG_SRB, ("MPE 17Completed SRB %08X\n", pCurrentSrb));
        }

        //
        // Clear this streams spot in the filters stream array
        //
        pFilter->pStream[ulStreamNumber][ulStreamInstance] = NULL;

        //
        // decrement the stream instance count for this filter
        //
        pFilter->ulActualInstances[ulStreamNumber]--;


        //
        // Reset the stream state to stopped
        //
        pStream->KSState = KSSTATE_STOP;

        //
        //
        //
        pStream->hMasterClock = NULL;

        //
        // Cleanup the streams transform buffer
        //
        if (pStream->pTransformBuffer)
        {
            ExFreePool (pStream->pTransformBuffer);
            pStream->pTransformBuffer = NULL;
        }

        //
        // Reset the stream extension blob
        //
        RtlZeroMemory(pStream, sizeof (STREAM));

        pSrb->Status = STATUS_SUCCESS;

    }
    else
    {
        pSrb->Status = STATUS_INVALID_PARAMETER;
    }
}


//////////////////////////////////////////////////////////////////////////////
VOID
OpenStream (
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    //
    // the stream extension structure is allocated by the stream class driver
    //
    PSTREAM         pStream        = (PSTREAM)pSrb->StreamObject->HwStreamExtension;
    PMPE_FILTER    pFilter        = ((PMPE_FILTER)pSrb->HwDeviceExtension);
    ULONG           ulStreamNumber = (ULONG) pSrb->StreamObject->StreamNumber;
    PKSDATAFORMAT   pKSDataFormat  = (PKSDATAFORMAT)pSrb->CommandData.OpenFormat;

    //
    // Initialize the next stream life check time.
    //
    KeQuerySystemTime( &pFilter->liLastTimeChecked );

    //
    // check that the stream index requested isn't too high
    // or that the maximum number of instances hasn't been exceeded
    //
    if (ulStreamNumber < DRIVER_STREAM_COUNT )
    {
        ULONG ulStreamInstance;
        ULONG ulMaxInstances = Streams[ulStreamNumber].hwStreamInfo.NumberOfPossibleInstances;

        //
        // Search for next open slot
        //
        for (ulStreamInstance = 0; ulStreamInstance < ulMaxInstances; ++ulStreamInstance)
        {
            if (pFilter->pStream[ulStreamNumber][ulStreamInstance] == NULL)
            {
                break;
            }
        }

        if (ulStreamInstance < ulMaxInstances)
        {
            if (VerifyFormat(pKSDataFormat, ulStreamNumber, &pStream->MatchedFormat))
            {
                //
                // Initialize Data queues and SpinLocks
                //
                InitializeListHead(&pFilter->StreamControlQueue);
                KeInitializeSpinLock(&pFilter->StreamControlSpinLock);

                InitializeListHead(&pFilter->StreamDataQueue);
                KeInitializeSpinLock(&pFilter->StreamDataSpinLock);

                InitializeListHead(&pFilter->IpV4StreamDataQueue);
                KeInitializeSpinLock(&pFilter->IpV4StreamDataSpinLock);

                InitializeListHead(&pFilter->StreamContxList);
                KeInitializeSpinLock(&pFilter->StreamUserSpinLock);


                //
                // Maintain an array of all the StreamEx structures in the HwDevExt
                // so that we can reference IRPs from any stream
                //
                pFilter->pStream[ulStreamNumber][ulStreamInstance] = pStream;

                //
                // Save the Stream Format in the Stream Extension as well.
                //
                pStream->OpenedFormat = *pKSDataFormat;

                //
                // Set up pointers to the handlers for the stream data and control handlers
                //
                pSrb->StreamObject->ReceiveDataPacket =
                                                (PVOID) Streams[ulStreamNumber].hwStreamObject.ReceiveDataPacket;
                pSrb->StreamObject->ReceiveControlPacket =
                                                (PVOID) Streams[ulStreamNumber].hwStreamObject.ReceiveControlPacket;

                //
                // The DMA flag must be set when the device will be performing DMA directly
                // to the data buffer addresses passed in to the ReceiveDataPacket routines.
                //
                pSrb->StreamObject->Dma = Streams[ulStreamNumber].hwStreamObject.Dma;

                //
                // The PIO flag must be set when the mini driver will be accessing the data
                // buffers passed in using logical addressing
                //
                pSrb->StreamObject->Pio = Streams[ulStreamNumber].hwStreamObject.Pio;

                pSrb->StreamObject->Allocator = Streams[ulStreamNumber].hwStreamObject.Allocator;

                //
                // How many extra bytes will be passed up from the driver for each frame?
                //
                pSrb->StreamObject->StreamHeaderMediaSpecific =
                                        Streams[ulStreamNumber].hwStreamObject.StreamHeaderMediaSpecific;

                pSrb->StreamObject->StreamHeaderWorkspace =
                                        Streams[ulStreamNumber].hwStreamObject.StreamHeaderWorkspace;

                //
                // Indicate the clock support available on this stream
                //
                pSrb->StreamObject->HwClockObject =
                                        Streams[ulStreamNumber].hwStreamObject.HwClockObject;

                //
                // Increment the instance count on this stream
                //
                pStream->ulStreamInstance = ulStreamInstance;
                pFilter->ulActualInstances[ulStreamNumber]++;

                //
                // Allocate a transform buffer
                //
                pStream->pTransformBuffer = ExAllocatePool (NonPagedPool, sizeof(SECTION_HEADER) + 4096);

                if (pStream->pTransformBuffer == NULL)
                {
                    pSrb->Status = STATUS_NO_MEMORY;
                    return;
                }

                RtlZeroMemory (pStream->pTransformBuffer, sizeof(SECTION_HEADER) + 4096);

                //
                // Initalize persistent pointer to output buffer to NULL
                //
                pStream->pOut = NULL;

                //
                // Initialize the exepected section number to zero
                //
                pStream->bExpectedSection = 0;


                //
                // Retain a private copy of the HwDevExt and StreamObject in the stream extension
                // so we can use a timer
                //
                pStream->pFilter = pFilter;                     // For timer use
                pStream->pStreamObject = pSrb->StreamObject;        // For timer use


                pSrb->Status = STATUS_SUCCESS;

            }
            else
            {
                pSrb->Status = STATUS_INVALID_PARAMETER;
            }
        }
        else
        {
            pSrb->Status = STATUS_INVALID_PARAMETER;
        }

    }
    else
    {
        pSrb->Status = STATUS_INVALID_PARAMETER;
    }
}


//////////////////////////////////////////////////////////////////////////////
BOOLEAN
VerifyFormat(
    IN KSDATAFORMAT *pKSDataFormat,
    UINT StreamNumber,
    PKSDATARANGE pMatchedFormat
    )
//////////////////////////////////////////////////////////////////////////////
{
    BOOLEAN   bResult               = FALSE;
    ULONG     FormatCount           = 0;
    PKS_DATARANGE_VIDEO pThisFormat = NULL;

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Verify Format\n"));

    for (FormatCount = 0; !bResult && FormatCount < Streams[StreamNumber].hwStreamInfo.NumberOfFormatArrayEntries;
         FormatCount++ )
    {


        pThisFormat = (PKS_DATARANGE_VIDEO) Streams [StreamNumber].hwStreamInfo.StreamFormatsArray [FormatCount];

        if (CompareGUIDsAndFormatSize( pKSDataFormat, &pThisFormat->DataRange, FALSE ) )
        {
            bResult = FALSE;

            if (pThisFormat->DataRange.SampleSize >= pKSDataFormat->SampleSize)
            {
                bResult = TRUE;
            }
            else
            {
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: VerifyFormat: Data range Sample Sizes don't match\n"));
            }
        }
    }

    if (bResult == TRUE && pMatchedFormat)
    {
        *pMatchedFormat = pThisFormat->DataRange;
    }

    return bResult;
}


///////////////////////////////////////////////////////////////////////////////////////
NTSTATUS
GetOutputBuffer (
    PMPE_FILTER pFilter,
    PHW_STREAM_REQUEST_BLOCK *ppSrb,
    PUCHAR *ppBuffer,
    PULONG pulSize
    )
///////////////////////////////////////////////////////////////////////////////////////
{
    NTSTATUS status                   = STATUS_INSUFFICIENT_RESOURCES;
    PKSSTREAM_HEADER  pStreamHdr      = NULL;
    PHW_STREAM_REQUEST_BLOCK pSrb     = NULL;



    if (QueueRemove( &pSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue))
    {

        pStreamHdr = pSrb->CommandData.DataBufferArray;

        *ppSrb    = pSrb;
        *ppBuffer = pStreamHdr->Data;
        *pulSize  = pStreamHdr->FrameExtent;

        status = STATUS_SUCCESS;

    }

    return status;
}

//////////////////////////////////////////////////////////////////////////////
VOID
STREAMAPI
ReceiveDataPacket (
    IN PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PMPE_FILTER       pFilter         = (PMPE_FILTER) pSrb->HwDeviceExtension;
    PSTREAM           pStream         = (PSTREAM)pSrb->StreamObject->HwStreamExtension;
    int               iStream         = (int) pSrb->StreamObject->StreamNumber;
    PKSSTREAM_HEADER  pStreamHdr      = pSrb->CommandData.DataBufferArray;
    PKSDATAFORMAT     pKSDataFormat   = (PKSDATAFORMAT) &pStream->MatchedFormat;
    ULONG             ul              = 0;
    PHW_STREAM_REQUEST_BLOCK pOutSrb  = NULL;
    SECTION_HEADER    Section         = {0};
    PSECTION_HEADER   pSection        = NULL;
    PUCHAR            pIn             = NULL;
    PLLC_SNAP         pSnap           = NULL;
    ULONG             ulSize          = 0;
    ULONG             ulLength        = 0;

    PHEADER_IP        pIP             = NULL;


    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called\n"));

    //
    // Default to success, disable timeouts
    //
    pSrb->TimeoutCounter = 0;
    pSrb->Status = STATUS_SUCCESS;

    //
    // Check for last buffer
    //
    if (pStreamHdr->OptionsFlags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM)
    {
        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet is LAST PACKET\n"));

        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 18Completed SRB %08X\n", pSrb));

        return;
    }


    if (pStreamHdr->OptionsFlags != 0)
    {
        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: OptionsFlags: %08X\n", pStreamHdr->OptionsFlags));
    }


    //
    // determine the type of packet.
    //
    switch (pSrb->Command)
    {
        case SRB_WRITE_DATA:


            if (pStream->KSState == KSSTATE_STOP)
            {
                pSrb->Status = STATUS_SUCCESS;

                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_WRITE STOP SRB Status returned: %08X\n", pSrb->Status));

                StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
                TEST_DEBUG( TEST_DBG_SRB, ("MPE 19Completed SRB %08X\n", pSrb));

                break;
            }

            //
            // Update the total number of packets written statistic
            //
            pFilter->Stats.ulTotalSectionsWritten += 1;


            //
            // Handle data input, output requests differently.
            //
            switch (iStream)
            {
                //
                //  Frame input stream
                //
                case MPE_STREAM:
                {
                    ULONG             ulBuffers        = pSrb->NumberOfBuffers;
                    ULONG             ulSkip           = 0;

                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler - SRB_WRITE - MPE_STREAM\n"));

                    //
                    // Initialize SRB Status to success
                    //
                    pSrb->Status = STATUS_SUCCESS;

                    //
                    // copy the contents of all buffers into one big buffer
                    //
                    ASSERT( ulBuffers == 1);
                    {
                        //ASSERT( pStreamHdr);
                        //ASSERT( pStreamHdr->DataUsed <= (sizeof(SECTION_HEADER) + 4096));
                        if (   pStreamHdr
                            && (pStreamHdr->DataUsed <= (sizeof(SECTION_HEADER) + 4096))
                           )
                        {
                            // Copy the data
                            RtlCopyMemory (pStream->pTransformBuffer,
                                           pStreamHdr->Data,
                                           pStreamHdr->DataUsed
                                           );
                        }
                        else
                        	{
			   	            pFilter->Stats.ulTotalInvalidSections += 1;
	                     	StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
	                        pStream->bExpectedSection = 0;
        	                pStream->pOut = NULL;
                	        pOutSrb = NULL;
	                        TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Invalid TableID", pSrb));
       	                	break;

                           }
                    }

                    //
                    // Process the transform buffer
                    //
                    pSection = (PSECTION_HEADER) pStream->pTransformBuffer;
                    NormalizeSection (pStream->pTransformBuffer, &Section);

                    //
                    // Do a quick check of the section header to confirm it looks valid
                    //
                    if (! ValidSection (&Section))
                    {
                        //  Ignore non-MPE sections
                        //
                        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                        pStream->bExpectedSection = 0;

                        pFilter->Stats.ulTotalInvalidSections += 1;

                        //
                        // Since we're discarding the data at this point, we'll re-queue the output
                        // SRB and re-use it when we get re-synched.
                        //
                        if (pOutSrb)
                        {
                            //$REVIEW - Can this cause out of order completion of sections.
                            //
                            QueuePush (pOutSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                        }
                        pStream->pOut = NULL;
                        pOutSrb = NULL;
                        TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Invalid TableID", pSrb));
                        break;
                    }

                    //
                    // Update our UnNormalized section header with our normalized one.
                    //
		      RtlCopyMemory (pStream->pTransformBuffer, &Section, sizeof (SECTION_HEADER));

                    //
                    // Check our section number and see if it's what we expect
                    //
                    if (pSection->section_number != pStream->bExpectedSection)
                    {
                        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                        pStream->bExpectedSection = 0;

                        pFilter->Stats.ulTotalUnexpectedSections += 1;

                        //
                        // Since we're discarding the data at this point, we'll re-queue the output
                        // SRB and re-use it when we get re-synched.
                        //
                        if (pOutSrb)
                        {
                            //$REVIEW - Can this cause out of order completion of sections.
                            //
                            QueuePush (pOutSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                        }
                        pStream->pOut = NULL;
                        pOutSrb = NULL;
                        TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Invalid section_number", pSrb));
                        break;
                    }

                    //
                    // Process the 1st section
                    //
                    if (pSection->section_number == 0)
                    {
                        PMAC_Address pMAC = NULL;

                        //
                        // Initialize packet length to zero
                        //
                        ulLength = 0;

                        //
                        //
                        //
                        if (GetOutputBuffer (pFilter, &pOutSrb, &pStream->pOut, &ulSize) != STATUS_SUCCESS)
                        {
                            //
                            // Failure....no buffers available most likely
                            //
                            pFilter->Stats.ulTotalUnavailableOutputBuffers += 1;
                            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                            TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Can't get SRB for output pin", pSrb));
                            break;
                        }

                        if (ulSize < (pSection->section_length - (sizeof (SECTION_HEADER) - 3)))
                        {
                            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                            pStream->bExpectedSection = 0;
                            pFilter->Stats.ulTotalOutputBuffersTooSmall += 1;

                            //
                            // Since we're discarding the data at this point, we'll re-queue the output
                            // SRB and re-use it when we get re-synched.
                            //
                            if (pOutSrb)
                            {
                                //$REVIEW - Can this cause out of order completion of sections.
                                //
                                QueuePush (pOutSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                            }
                            pStream->pOut = NULL;
                            pOutSrb = NULL;

                            TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Section too big", pSrb));
                            break;
                        }

                        pIP = (PHEADER_IP) pSection->Data;
                        if (pSection->LLC_SNAP_flag == 0x1)
                        {
                            pSnap = (PLLC_SNAP) pSection->Data;
                            pIP = (PHEADER_IP) pSnap->Data;
                            ulSkip = sizeof( LLC_SNAP);
                        }

                        //
                        // Add the MAC address to the buffer.  The MAC address prefix's the IP packet
                        //
                        pMAC = (PMAC_Address) pStream->pOut;
                        pMAC->MAC_Dest_Address [0] = pSection->MAC_address_1;
                        pMAC->MAC_Dest_Address [1] = pSection->MAC_address_2;
                        pMAC->MAC_Dest_Address [2] = pSection->MAC_address_3;
                        pMAC->MAC_Dest_Address [3] = pSection->MAC_address_4;
                        pMAC->MAC_Dest_Address [4] = pSection->MAC_address_5;
                        pMAC->MAC_Dest_Address [5] = pSection->MAC_address_6;

                        pMAC->MAC_Src_Address [0] = 0x00;
                        pMAC->MAC_Src_Address [1] = 0x00;
                        pMAC->MAC_Src_Address [2] = 0x00;
                        pMAC->MAC_Src_Address [3] = 0x00;
                        pMAC->MAC_Src_Address [4] = 0x00;
                        pMAC->MAC_Src_Address [5] = 0x00;

                        pMAC->usLength = 0x0008;

                        //
                        // Adjust pointer to output buffer where we'll put data
                        //
                        pStream->pOut += sizeof (MAC_Address);

                        pIn = pSection->Data;

                        if (pSection->LLC_SNAP_flag == 0x1)
                        {
                            pSnap = (PLLC_SNAP) pSection->Data;

                            if (pSnap->type != 0x0008)
                            {
                                StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);

                                //
                                // Next expected Section should be zero
                                //
                                pStream->bExpectedSection = 0;
                                pFilter->Stats.ulTotalInvalidIPSnapHeaders += 1;

                                //
                                // Since we're discarding the data at this point, we'll re-queue the output
                                // SRB and re-use it when we get re-synched.
                                //
                                if (pOutSrb)
                                {
                                    //$REVIEW - Can this cause out of order completion of sections.
                                    //
                                    QueuePush (pOutSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                                }
                                pStream->pOut = NULL;
                                pOutSrb = NULL;

                                TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Bad Snap Type", pSrb));
                                break;
                            }

                            pIn = pSnap->Data;

                        }

                        ulLength = sizeof (MAC_Address);
                    }

                    //
                    // pOut should be NULL unless we've found the 1st section.
                    //
                    if (pStream->pOut)
                    {
                        ULONG ulTmp = 0;
                        PKSSTREAM_HEADER  pOutStreamHdr;

                        //
                        // Update the datasize field of the Output SRB
                        //
                        pOutStreamHdr = (PKSSTREAM_HEADER) pOutSrb->CommandData.DataBufferArray;


                        //
                        // Copy data from transform section to output SRB buffer
                        //
                        // Compute the number of bytes to copy.  We subtract of 9 bytes
                        // only if this is a LLSNAP packet.
                        //
                        ulTmp  = pSection->section_length;
                        ulTmp -= ulSkip;


                        ASSERT(pIn);
                        ASSERT(pStream->pOut);
	              

                        if (ulSize < (ulTmp +sizeof (MAC_Address) +3))
                        {
                            StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                            pStream->bExpectedSection = 0;
                            pFilter->Stats.ulTotalOutputBuffersTooSmall += 1;

                            //
                            // Since we're discarding the data at this point, we'll re-queue the output
                            // SRB and re-use it when we get re-synched.
                            //
                            if (pOutSrb)
                            {
                                //$REVIEW - Can this cause out of order completion of sections.
                                //
                                QueuePush (pOutSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                            }
                            pStream->pOut = NULL;
                            pOutSrb = NULL;

                            TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Section too big", pSrb));
                            break;
                        }

	              
		       RtlCopyMemory (pStream->pOut, pIn, ulTmp);
                       ulLength += ulTmp;
                       pOutStreamHdr->DataUsed += ulLength;

                        ulLength = 0;
		 }

                    if (pSection->section_number == pSection->last_section_number)
                    {

                        pFilter->Stats.ulTotalIPPacketsWritten += 1;

                        pOutSrb->Status = STATUS_SUCCESS;
                        StreamClassStreamNotification (StreamRequestComplete, pOutSrb->StreamObject, pOutSrb);
                        TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n", pSrb));

                        pOutSrb = NULL;
                        pStream->pOut    = NULL;
                        ulSize  = 0;
                    }
                    else
                    {
                     if (pOutSrb)
                            {
                                //$REVIEW - Can this cause out of order completion of sections.
                                //
                                QueuePush (pOutSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                            }

                    }


                    StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                    TEST_DEBUG( TEST_DBG_SRB, ("MPE 20Completed SRB %08X\n - Packet Sent", pSrb));

                }
                break;


                default:
                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called - SRB_WRITE - Default\n"));
                    pSrb->Status = STATUS_NOT_IMPLEMENTED;

                    //
                    // Update stats for Unkown packet count
                    //
                    pFilter->Stats.ulTotalUnknownPacketsWritten += 1;

                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: DEFAULT SRB Status returned: %08X\n", pSrb->Status));

                    StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                    TEST_DEBUG( TEST_DBG_SRB, ("MPE 22Completed SRB %08X\n", pSrb));

                    break;
            }
            break;


        case SRB_READ_DATA:

            if (pStream->KSState == KSSTATE_STOP)
            {
                pSrb->Status = STATUS_SUCCESS;
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_READ STOP SRB Status returned: %08X\n", pSrb->Status));

                StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
                TEST_DEBUG( TEST_DBG_SRB, ("MPE 23Completed SRB %08X\n", pSrb));

                break;
            }

            //
            // Update stats for Unkown packet count
            //
            pFilter->Stats.ulTotalPacketsRead += 1;

            switch (iStream)
            {
                #ifdef OLD

                case MPE_NET_CONTROL:
                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called - SRB_READ - STREAM_NET_CONTROL\n"));
                    pSrb->Status = STATUS_SUCCESS;
                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: MPE_NET_CONTROL SRB Status returned: %08X\n", pSrb->Status));
                    StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                    TEST_DEBUG( TEST_DBG_SRB, ("MPE 24Completed SRB %08X\n", pSrb));
                    break;

                #endif

                case MPE_IPV4:
                {
                    ULONG             ulBuffers       = pSrb->NumberOfBuffers;

                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called - SRB_READ - MPE_IPV4\n"));

                    if (pSrb->CommandData.DataBufferArray->FrameExtent < pKSDataFormat->SampleSize)
                    {
                        pSrb->Status = STATUS_BUFFER_TOO_SMALL;
                        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: MPE_IPV4 SRB Buffer too small.... Status returned: %08X\n", pSrb->Status));
                        StreamClassStreamNotification(StreamRequestComplete, pSrb->StreamObject, pSrb);
                        TEST_DEBUG( TEST_DBG_SRB, ("MPE 25Completed SRB %08X\n", pSrb));
                    }
                    else
                    {
                        //
                        // Take the SRB we get and  queue it up.  These Queued SRB's will be filled with data on a WRITE_DATA
                        // request, at which point they will be completed.
                        //
                        QueueAdd (pSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue);
                        TEST_DEBUG( TEST_DBG_SRB, ("MPE Queuing IPv4 SRB %08X\n", pSrb));


                        //
                        // Since the stream state may have changed while we were adding the SRB to the queue
                        // we'll check it again, and cancel it if necessary
                        //
                        if (pStream->KSState == KSSTATE_STOP)
                        {
                            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB_READ STOP SRB Status returned: %08X\n", pSrb->Status));

                            if (QueueRemoveSpecific (pSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue))
                            {
                                pSrb->Status = STATUS_CANCELLED;
                                StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb );
                                TEST_DEBUG( TEST_DBG_SRB, ("MPE 26Completed SRB %08X\n", pSrb));
                                return;
                            }
                            break;
                        }
                        
                    }
                }
                break;


                default:
                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called - SRB_READ - Default\n"));
                    pSrb->Status = STATUS_NOT_IMPLEMENTED;
                    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: DEFAULT SRB Status returned: %08X\n", pSrb->Status));
                    StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
                    TEST_DEBUG( TEST_DBG_SRB, ("MPE 27Completed SRB %08X\n", pSrb));
                    break;

            }
            break;

        default:

            //
            // invalid / unsupported command. Fail it as such
            //
            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called - Unsupported Command\n"));
            pSrb->Status = STATUS_NOT_IMPLEMENTED;
            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: DEFAULT SRB Status returned: %08X\n", pSrb->Status));
            StreamClassStreamNotification( StreamRequestComplete, pSrb->StreamObject, pSrb );
            TEST_DEBUG( TEST_DBG_SRB, ("MPE 28Completed SRB %08X\n", pSrb));
            ASSERT (FALSE);
            break;

    }


    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Data packet handler called...status: %08X\n", pSrb->Status));

    return;
}



//////////////////////////////////////////////////////////////////////////////
VOID
MpeGetProperty (
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PSTREAM_PROPERTY_DESCRIPTOR pSPD = pSrb->CommandData.PropertyInfo;

    pSrb->Status = STATUS_SUCCESS;

    if (IsEqualGUID (&KSPROPSETID_Connection, &pSPD->Property->Set))
    {
        MpeGetConnectionProperty (pSrb);
    }
    else
    {
        pSrb->Status = STATUS_NOT_IMPLEMENTED;
    }

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: MpeGetProperty Status: %08X\n", pSrb->Status));

    return;
}


//////////////////////////////////////////////////////////////////////////////
VOID
IndicateMasterClock(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PSTREAM pStream = (PSTREAM) pSrb->StreamObject->HwStreamExtension;

    pStream->hClock = pSrb->CommandData.MasterClockHandle;
}

//////////////////////////////////////////////////////////////////////////////
VOID
STREAMAPI
ReceiveCtrlPacket(
    IN PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PMPE_FILTER pFilter = (PMPE_FILTER) pSrb->HwDeviceExtension;
    PSTREAM pStream = (PSTREAM) pSrb->StreamObject->HwStreamExtension;

    TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler called\n"));

    pSrb->Status = STATUS_SUCCESS;

    QueueAdd (pSrb, &pFilter->StreamControlSpinLock, &pFilter->StreamControlQueue);
    TEST_DEBUG( TEST_DBG_SRB, ("MPE Queuing Control Packet SRB %08X\n", pSrb));

    while (QueueRemove (&pSrb, &pFilter->StreamControlSpinLock, &pFilter->StreamControlQueue))
    {
        //
        // determine the type of packet.
        //
        switch (pSrb->Command)
        {
            case SRB_PROPOSE_DATA_FORMAT:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Propose data format\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_SET_STREAM_STATE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Set Stream State\n"));
                pSrb->Status = STATUS_SUCCESS;
                MpeSetState (pSrb);
                break;

            case SRB_GET_STREAM_STATE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Get Stream State\n"));
                pSrb->Status = STATUS_SUCCESS;
                pSrb->CommandData.StreamState = pStream->KSState;
                pSrb->ActualBytesTransferred = sizeof (KSSTATE);
                break;

            case SRB_GET_STREAM_PROPERTY:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Get Stream Property\n"));
                MpeGetProperty(pSrb);
                break;

            case SRB_SET_STREAM_PROPERTY:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Set Stream Property\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

            case SRB_INDICATE_MASTER_CLOCK:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Indicate Master Clock\n"));
                pSrb->Status = STATUS_SUCCESS;
                IndicateMasterClock (pSrb);
                break;

            case SRB_SET_STREAM_RATE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Set Stream Rate\n"));
                pSrb->Status = STATUS_SUCCESS;
                break;

            case SRB_PROPOSE_STREAM_RATE:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Propose Stream Rate\n"));
                pSrb->Status = STATUS_SUCCESS;
                break;

            default:
                TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Receive Control packet handler - Default case\n"));
                pSrb->Status = STATUS_NOT_IMPLEMENTED;
                break;

        }

        TEST_DEBUG (TEST_DBG_TRACE, ("MPE: SRB Status returned: %08X\n", pSrb->Status));

        StreamClassStreamNotification (StreamRequestComplete, pSrb->StreamObject, pSrb);
        TEST_DEBUG( TEST_DBG_SRB, ("MPE 29Completed SRB %08X\n", pSrb));

    }

}



//////////////////////////////////////////////////////////////////////////////
VOID
MpeSetState(
    PHW_STREAM_REQUEST_BLOCK pSrb
    )
//////////////////////////////////////////////////////////////////////////////
{
    PMPE_FILTER pFilter                 = ((PMPE_FILTER) pSrb->HwDeviceExtension);
    PSTREAM pStream                      = (PSTREAM) pSrb->StreamObject->HwStreamExtension;
    PHW_STREAM_REQUEST_BLOCK pCurrentSrb = NULL;

    //
    // For each stream, the following states are used:
    //
    // Stop:    Absolute minimum resources are used.  No outstanding IRPs.
    // Acquire: KS only state that has no DirectShow correpondence
    //          Acquire needed resources.
    // Pause:   Getting ready to run.  Allocate needed resources so that
    //          the eventual transition to Run is as fast as possible.
    //          Read SRBs will be queued at either the Stream class
    //          or in your driver (depending on when you send "ReadyForNext")
    // Run:     Streaming.
    //
    // Moving to Stop to Run always transitions through Pause.
    //
    // But since a client app could crash unexpectedly, drivers should handle
    // the situation of having outstanding IRPs cancelled and open streams
    // being closed WHILE THEY ARE STREAMING!
    //
    // Note that it is quite possible to transition repeatedly between states:
    // Stop -> Pause -> Stop -> Pause -> Run -> Pause -> Run -> Pause -> Stop
    //
    switch (pSrb->CommandData.StreamState)
    {
        case KSSTATE_STOP:

            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Set Stream State KSSTATE_STOP\n"));

            pStream->KSState = pSrb->CommandData.StreamState; 
            //
            // If transitioning to STOP state, then complete any outstanding IRPs
            //
            while (QueueRemove(&pCurrentSrb, &pFilter->IpV4StreamDataSpinLock, &pFilter->IpV4StreamDataQueue))
            {
                pCurrentSrb->Status = STATUS_CANCELLED;
                pCurrentSrb->CommandData.DataBufferArray->DataUsed = 0;

                StreamClassStreamNotification(StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
                TEST_DEBUG( TEST_DBG_SRB, ("MPE 30Completed SRB %08X\n", pCurrentSrb));
           }
           while (QueueRemove(&pCurrentSrb, &pFilter->StreamControlSpinLock, &pFilter->StreamControlQueue))
            {
                pCurrentSrb->Status = STATUS_CANCELLED;
                pCurrentSrb->CommandData.DataBufferArray->DataUsed = 0;

                StreamClassStreamNotification(StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
                TEST_DEBUG( TEST_DBG_SRB, ("MPE 30Completed SRB %08X\n", pCurrentSrb));
            }

            while (QueueRemove(&pCurrentSrb, &pFilter->StreamDataSpinLock, &pFilter->StreamDataQueue))
            {
                pCurrentSrb->Status = STATUS_CANCELLED;
                pCurrentSrb->CommandData.DataBufferArray->DataUsed = 0;

                StreamClassStreamNotification(StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
                TEST_DEBUG( TEST_DBG_SRB, ("MPE 30Completed SRB %08X\n", pCurrentSrb));
            }

            while (QueueRemove(&pCurrentSrb, &pFilter->StreamUserSpinLock, &pFilter->StreamContxList))
            {
                pCurrentSrb->Status = STATUS_CANCELLED;
                pCurrentSrb->CommandData.DataBufferArray->DataUsed = 0;

                StreamClassStreamNotification(StreamRequestComplete, pCurrentSrb->StreamObject, pCurrentSrb);
                TEST_DEBUG( TEST_DBG_SRB, ("MPE 30Completed SRB %08X\n", pCurrentSrb));
            }
              
            
            pSrb->Status = STATUS_SUCCESS;
            break;


        case KSSTATE_ACQUIRE:
            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Set Stream State KSSTATE_ACQUIRE\n"));
            pStream->KSState = pSrb->CommandData.StreamState;
            pSrb->Status = STATUS_SUCCESS;
            break;

        case KSSTATE_PAUSE:
            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Set Stream State KSSTATE_PAUSE\n"));
            pStream->KSState = pSrb->CommandData.StreamState;
            pSrb->Status = STATUS_SUCCESS;
            break;

        case KSSTATE_RUN:
            TEST_DEBUG (TEST_DBG_TRACE, ("MPE: Set Stream State KSSTATE_RUN\n"));
            pStream->KSState = pSrb->CommandData.StreamState;
            pSrb->Status = STATUS_SUCCESS;
            break;

    } // end switch (pSrb->CommandData.StreamState)

    return;
}

