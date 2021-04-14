/*++
Copyright (c) 2001-2002  Microsoft Corporation

Module Name:

    CRC.c

Abstract:

    CRC provides the function to calculate the checksum for
    the read/write disk I/O.

Environment:

    kernel mode only

Notes:

--*/
#include "Filter.h"
#include "Device.h"
#include "CRC.h"
#include "Util.h"

#if DBG_WMI_TRACING
    //
    // for any file that has software tracing printouts, you must include a
    // header file <filename>.tmh
    // this file will be generated by the WPP processing phase
    //
    #include "CRC.tmh"
#endif

//
//  there are several different implementations of computing the CheckSum.
//  this one is same as the one under:
//  Base\ntos\rtl\checksum.c
//

ULONG32 RtlCrc32Table[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};



ULONG32
ComputeCheckSum(
    ULONG32 PartialCrc,
    PUCHAR Buffer,
    ULONG Length
    )

/*++

Routine Description:

    Compute the CRC32 as specified in in IS0 3309. See RFC-1662 and RFC-1952
    for implementation details and references.

    Pre- and post-conditioning (one's complement) is done by this function, so
    it should not be done by the caller. That is, do:

        Crc = RtlComputeCrc32 ( 0, buffer, length );

    instead of

        Crc = RtlComputeCrc32 ( 0xffffffff, buffer, length );

    or
        Crc = RtlComputeCrc32 ( 0xffffffff, buffer, length) ^ 0xffffffff;


Arguments:

    PartialCrc - A partially calculated CRC32.

    Buffer - The buffer you want to CRC.

    Length - The length of the buffer in bytes.

Return Value:

    The updated CRC32 value.

Environment:

    Kernel mode at IRQL of APC_LEVEL or below, User mode, or within
    the boot-loader.

--*/

{
    ULONG32 Crc;
    ULONG i;

    //
    // Compute the CRC32 checksum.
    //

    Crc = PartialCrc ^ 0xffffffffL;

    for (i = 0; i < Length; i++) {
        Crc = RtlCrc32Table [(Crc ^ Buffer [ i ]) & 0xff] ^ (Crc >> 8);
    }

    Crc = (Crc ^ 0xffffffffL);

    //
    // modified version
    //
    return (Crc != 0) ? Crc:1;
}

USHORT
ComputeCheckSum16(
    ULONG32 PartialCrc,
    PUCHAR Buffer,
    ULONG Length
    )
{
    ULONG32 Crc;
    ULONG   i;
    USHORT  CrcShort;

    //
    // Compute the CRC32 checksum.
    //

    Crc = PartialCrc ^ 0xffffffffL;

    for (i = 0; i < Length; i++) {
        Crc = RtlCrc32Table [(Crc ^ Buffer [ i ]) & 0xff] ^ (Crc >> 8);
    }

    Crc = (Crc ^ 0xffffffffL);
    CrcShort = (USHORT)( (Crc >> 16) ^ (Crc & 0x0000FFFFL) );

    return (CrcShort != 0)? CrcShort:1;
}



VOID
InvalidateChecksums(
    IN  PDEVICE_EXTENSION       deviceExtension,
    IN  ULONG                   ulLogicalBlockAddr,
    IN  ULONG                   ulTotalLength)
/*++

Routine Description:

    Invalidate checksums for the given range

    Must be called with SyncEvent HELD

Arguments:

    deviceExtension     -   device information.
    ulLogiceBlockAddr   -   Logic Block Address
    ulLength            -   in bytes

Return Value:

    N/A
    This fn needs to be called with Spin-Lock Held

[DISPATCH_LEVEL]

--*/

{
    ASSERT((ulTotalLength % deviceExtension->ulSectorSize) == 0);

    if (ulTotalLength){
        ULONG numSectors = ulTotalLength/deviceExtension->ulSectorSize;
        ULONG startSector = ulLogicalBlockAddr;
        ULONG endSector = startSector+numSectors-1;
        ULONG i;
        
        for (i = startSector; i <= endSector; i++){
            /*
             *  Invalidate the checksum by writing a checksum value of 0 to it.
             */
            VerifyOrStoreSectorCheckSum(deviceExtension, i, 0, TRUE, FALSE, NULL, TRUE);
        }
    }
    
}


/*++

Routine Description:

   Verify the CRC against what's in the CRC table. In the case if the Page
   wasn't allocated or loaded, TRUE will be returned.

Arguments:

Return Value:

  True if CRC matches, false otherwise

--*/

BOOLEAN
VerifyCheckSum(
    IN  PDEVICE_EXTENSION       deviceExtension,
    IN  PIRP                    Irp,
    IN  ULONG                   ulLogicalBlockAddr,
    IN  ULONG                   ulTotalLength,
    IN  PVOID                   pData,
    IN  BOOLEAN                 bWrite)
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK pSRB = irpStack->Parameters.Scsi.Srb;
    BOOLEAN isPagingRequest = (pSRB->SrbFlags & SRB_CLASS_FLAGS_PAGING) ? TRUE : FALSE;
    ULONG   ulSectorSize;
    ULONG   ulCRCStartAddr          = (ulLogicalBlockAddr + CRC_BLOCK_UNIT - 1) / CRC_BLOCK_UNIT * CRC_BLOCK_UNIT; 
    ULONG   ulCRCDataPtr            = ulCRCStartAddr - ulLogicalBlockAddr;
    ULONG   ulLength                = ulTotalLength;
    ULONG   StartSector;
    ULONG   EndSector;
    ULONG       i;

    ASSERT((ulTotalLength % deviceExtension->ulSectorSize) == 0);
    ASSERT( ulLength );
    //
    //  Find out which CRC Table we need.
    //
    StartSector  = ulLogicalBlockAddr ;
    EndSector    = StartSector + ( ulTotalLength / deviceExtension->ulSectorSize) - 1;
    ulCRCDataPtr = 0;

    for (i = StartSector; i <= EndSector; i++){
        USHORT checkSum;
        KIRQL oldIrql;

        checkSum = ComputeCheckSum16( 0, 
                                 (PUCHAR)((PUCHAR)pData + (ULONG_PTR)(ulCRCDataPtr * deviceExtension->ulSectorSize)), 
                                 deviceExtension->ulSectorSize );

        /*
         *  Log the checksum synchronously with the I/O or I/O completion.
         *  This is valuable because we may have to defer the actual checksum comparison.
         */
        KeAcquireSpinLock(&deviceExtension->SpinLock, &oldIrql);
        deviceExtension->SectorDataLog[deviceExtension->SectorDataLogNextIndex].SectorNumber = i;
        deviceExtension->SectorDataLog[deviceExtension->SectorDataLogNextIndex].CheckSum = checkSum;
        deviceExtension->SectorDataLog[deviceExtension->SectorDataLogNextIndex].IsWrite = bWrite;
        deviceExtension->SectorDataLogNextIndex++;
        deviceExtension->SectorDataLogNextIndex %= NUM_SECTORDATA_LOGENTRIES;
        KeReleaseSpinLock(&deviceExtension->SpinLock, oldIrql);        

        /*
         *  ISSUE-2002/7/24-ervinp: 
         *                 if at passive, want to pass in (!isPagingRequest) for PagingOk param,
         *                 but paging causes problems even when servicing non-paging requests (why?),
         *                 so always avoid paging.
         */
        VerifyOrStoreSectorCheckSum(deviceExtension, i, checkSum, bWrite, FALSE, Irp, TRUE);
        
        ulCRCDataPtr++;
    }
    
    return TRUE; 
}


/*
 *  VerifyOrStoreSectorCheckSum
 *
 *      If our checksum arrays are allocated and resident (or if PagingOk), 
 *      then do the checksum comparison (for reads) or store the new checksum (for writes).
 *      Otherwise, queue a workItem to do this retroactively.
 *
 *      If PagingOk is TRUE, must be called at PASSIVE IRQL and with SyncEvent HELD.
 */
VOID VerifyOrStoreSectorCheckSum(   PDEVICE_EXTENSION DeviceExtension, 
                                                                        ULONG SectorNum, 
                                                                        USHORT CheckSum, 
                                                                        BOOLEAN IsWrite,
                                                                        BOOLEAN PagingOk,
                                                                        PIRP OriginalIrpOrCopy OPTIONAL,
                                                                        BOOLEAN IsSynchronousCheck)
{
    ULONG regionIndex = SectorNum/CRC_MDL_LOGIC_BLOCK_SIZE;
    PCRC_MDL_ITEM pCRCMdlItem = &DeviceExtension->CRCMdlLists.pMdlItems[regionIndex];
    ULONG arrayIndex = SectorNum % CRC_MDL_LOGIC_BLOCK_SIZE;
    PDEFERRED_CHECKSUM_ENTRY defCheckEntry = NULL;   
    BOOLEAN doCheck, doCheckNow;
    KIRQL oldIrql;
    
    ASSERT(!PagingOk || (KeGetCurrentIrql() < DISPATCH_LEVEL));
    ASSERT (regionIndex <= DeviceExtension->CRCMdlLists.ulMaxItems);
    
    /*
     *  If PagingOk, then we are holding the SyncEvent, and we do not want to grab the spinlock
     *  because AllocAndMapPages needs to be called at passivel level so it can alloc paged pool.
     *  If !PagingOk, then we need to grab the spinlock to check if the checksum arrays are locked,
     *  and hold the spinlock across the check.
     *  So either way, we have either the SyncEvent or the spinlock and we are synchronized with
     *  the reallocation of the checksum arrays that the ReadCapacity completion may have to do.
     */
    if (!PagingOk){   
        /*
         *  Try to alloc this first, since we won't be able to after acquiring the spinlock.
         */
        defCheckEntry = NewDeferredCheckSumEntry(DeviceExtension, SectorNum, CheckSum, IsWrite);

        KeAcquireSpinLock(&DeviceExtension->SpinLock, &oldIrql);       
    }

    if (DeviceExtension->CRCMdlLists.mdlItemsAllocated && !DeviceExtension->NeedCriticalRecovery){
        if (PagingOk){
            /*
             *  Allocate the checksum arrays if needed.  
             *  This also locks down the array.
             */
            NTSTATUS allocStatus = AllocAndMapPages(DeviceExtension, SectorNum, 1);
            if (NT_SUCCESS(allocStatus)){
                doCheck = doCheckNow = TRUE;
            }
            else {
                DBGERR(("AllocAndMapPages failed with %xh", allocStatus));
                doCheck = doCheckNow = FALSE;

                /*
                 *  If this is a write, then we may have an out-of-date checksum that we can't access now.
                 *  So we need to recover.
                 */
                if (IsWrite){
                    DeviceExtension->NeedCriticalRecovery = TRUE;
                }
            }
        }
        else {
            /*
             *  We cannot do paging now.  
             *  But we don't want to queue a workItem for every sector, because this would kill perf.
             *  So we will do the checksum check/update opportunistically if the checksum arrays 
             *  happen to be allocated locked in memory.
             */
            BOOLEAN pagingNeeded = !(pCRCMdlItem->checkSumsArraysAllocated && pCRCMdlItem->checkSumsArraysLocked);
            if (pagingNeeded){
                doCheckNow = FALSE;
            }
            else if (DeviceExtension->CheckInProgress){
                /*
                 *  This prevents a race with the workItem.
                 */
                doCheckNow = FALSE;
            }
            else {
                /*
                 *  If there are any outstanding deferred checksums that overlap with this request,
                 *  then we will defer this one too so that they don't get out of order.
                 */
                PLIST_ENTRY listEntry = DeviceExtension->DeferredCheckSumList.Flink;
                doCheckNow = TRUE;
                while (listEntry != &DeviceExtension->DeferredCheckSumList){
                    PDEFERRED_CHECKSUM_ENTRY thisDefEntry = CONTAINING_RECORD(listEntry, DEFERRED_CHECKSUM_ENTRY, ListEntry);
                    if (thisDefEntry->SectorNum == SectorNum){
                        doCheckNow = FALSE;
                        break;
                    }
                    listEntry = listEntry->Flink;                
                }                
            }
            doCheck = TRUE;                                    
        }
    }
    else {
        doCheck = doCheckNow = FALSE;
    }
    
    if (doCheck){

        if (doCheckNow){

            /*
             *  If we didn't grab the lock above, grab it now to synchronize access to the checksums.
             *  The checksum arrays are locked now, so it will be ok to touch them at raised irql.
             */
            if (PagingOk){   
                KeAcquireSpinLock(&DeviceExtension->SpinLock, &oldIrql);
            }

            ASSERT(pCRCMdlItem->checkSumsArraysLocked);
            
            // ISSUE: these don't work quite as expected
            // ASSERT(MmIsAddressValid(&pCRCMdlItem->checkSumsArray[arrayIndex]));
            // ASSERT(MmIsAddressValid(&pCRCMdlItem->checkSumsArrayCopy[arrayIndex]));
            
            if (pCRCMdlItem->checkSumsArray[arrayIndex] != pCRCMdlItem->checkSumsArrayCopy[arrayIndex]){
                /*
                 *  Our two copies of the checksum don't match,
                 *  possibly because they were paged out to disk and corrupted there.
                 */
                DeviceExtension->IsRaisingException = TRUE;                
                DeviceExtension->ExceptionSector = SectorNum;
                DeviceExtension->ExceptionIrpOrCopyPtr = OriginalIrpOrCopy;
                DeviceExtension->ExceptionCheckSynchronous = IsSynchronousCheck;
                KeBugCheckEx(DRIVER_VERIFIER_DETECTED_VIOLATION,
                           (ULONG_PTR)0xA2,
                           (ULONG_PTR)OriginalIrpOrCopy,
                           (ULONG_PTR)DeviceExtension->LowerDeviceObject,
                            (ULONG_PTR)SectorNum);
            }
            else if (!pCRCMdlItem->checkSumsArray[arrayIndex] || IsWrite){
                pCRCMdlItem->checkSumsArray[arrayIndex] = pCRCMdlItem->checkSumsArrayCopy[arrayIndex] = CheckSum;    
            } 
            else if (pCRCMdlItem->checkSumsArray[arrayIndex] != CheckSum){

                DBGERR(("Disk Integrity Verifier (crcdisk): checksum for sector %xh does not match (%xh (current) != %xh (recorded)), devObj=%ph ", SectorNum, (ULONG)CheckSum, (ULONG)pCRCMdlItem->checkSumsArray[arrayIndex], DeviceExtension->DeviceObject));
                    
                DeviceExtension->IsRaisingException = TRUE;                 
                DeviceExtension->ExceptionSector = SectorNum;
                DeviceExtension->ExceptionIrpOrCopyPtr = OriginalIrpOrCopy;
                DeviceExtension->ExceptionCheckSynchronous = IsSynchronousCheck;
                KeBugCheckEx(DRIVER_VERIFIER_DETECTED_VIOLATION,
                           (ULONG_PTR)(IsSynchronousCheck ? 0xA0 : 0xA1),
                           (ULONG_PTR)OriginalIrpOrCopy,
                           (ULONG_PTR)DeviceExtension->LowerDeviceObject,
                            (ULONG_PTR)SectorNum);
            }

            DeviceExtension->DbgNumChecks++;
            UpdateRegionAccessTimeStamp(DeviceExtension, regionIndex);

            if (PagingOk){
                KeReleaseSpinLock(&DeviceExtension->SpinLock, oldIrql);
            }
            
        }
        else {
            /*
             *  We cannot do the check now, so queue a workItem to do it later.
             *  Note that spinlock is always held in this case to synchronize queuing of the workItem.
             */
            ASSERT(!PagingOk);
            if (defCheckEntry){
                if (OriginalIrpOrCopy){
                    RtlCopyMemory((PUCHAR)defCheckEntry->IrpCopyBytes, OriginalIrpOrCopy, min(sizeof(defCheckEntry->IrpCopyBytes), sizeof(IRP)+OriginalIrpOrCopy->StackCount*sizeof(IO_STACK_LOCATION)));
                    if (IsSynchronousCheck){
                        PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(OriginalIrpOrCopy);
                        defCheckEntry->SrbCopy = *irpSp->Parameters.Scsi.Srb;
                        if (OriginalIrpOrCopy->MdlAddress){
                            ULONG mdlCopyBytes = min(OriginalIrpOrCopy->MdlAddress->Size, sizeof(defCheckEntry->MdlCopyBytes));
                            RtlCopyMemory((PUCHAR)defCheckEntry->MdlCopyBytes, OriginalIrpOrCopy->MdlAddress, mdlCopyBytes);
                        }                            
                    }
                }
                
                InsertTailList(&DeviceExtension->DeferredCheckSumList, &defCheckEntry->ListEntry);
                if (!DeviceExtension->IsCheckSumWorkItemOutstanding){
                    DeviceExtension->IsCheckSumWorkItemOutstanding = TRUE;
                    IoQueueWorkItem(DeviceExtension->CheckSumWorkItem, CheckSumWorkItemCallback, HyperCriticalWorkQueue, DeviceExtension);
                }
                defCheckEntry = NULL;
            }
            else {
                /*
                 *  We can't defer the crc check/recording due to memory constraints.
                 *  If this was a write, invalidate the old checksum, since it is now invalid.
                 */
                if (IsWrite){
                    if (pCRCMdlItem->checkSumsArraysLocked){
                        pCRCMdlItem->checkSumsArray[arrayIndex] = pCRCMdlItem->checkSumsArrayCopy[arrayIndex] = 0;    
                    }
                    else {
                        /*
                         *  We were not able to record or defer-write the new checksum 
                         *  because its checksum array is paged out and we are completely out of memory.
                         *  Since we page out the overflow of our checksum arrays, 
                         *  this will happen only under extreme memory stress when all nonpaged pool is consumed.
                         *  One of our checksums is invalid now, so we just flag ourselves to recover before doing any more checks.
                         */
                        DeviceExtension->NeedCriticalRecovery = TRUE;
                        if (!DeviceExtension->IsCheckSumWorkItemOutstanding){
                            DeviceExtension->IsCheckSumWorkItemOutstanding = TRUE;
                            IoQueueWorkItem(DeviceExtension->CheckSumWorkItem, CheckSumWorkItemCallback, HyperCriticalWorkQueue, DeviceExtension);
                        }
                    }
                }
            }
        }  
    }
    
    if (!PagingOk){
        KeReleaseSpinLock(&DeviceExtension->SpinLock, oldIrql);

        /*
         *  Free this if we didn't use it
         */
        if (defCheckEntry){
            FreeDeferredCheckSumEntry(DeviceExtension, defCheckEntry);
        }
    }

}


VOID CheckSumWorkItemCallback(PDEVICE_OBJECT DevObj, PVOID Context)
{
    PDEVICE_EXTENSION DeviceExtension = Context;

    ASSERT(DeviceExtension->IsCheckSumWorkItemOutstanding);
    
    while (TRUE){
        PDEFERRED_CHECKSUM_ENTRY defCheckEntry;
        KIRQL oldIrql;

        KeAcquireSpinLock(&DeviceExtension->SpinLock, &oldIrql);

        if (IsListEmpty(&DeviceExtension->DeferredCheckSumList)){
            DeviceExtension->IsCheckSumWorkItemOutstanding = FALSE;
            defCheckEntry = NULL;
        }
        else {
            PLIST_ENTRY listEntry = RemoveHeadList(&DeviceExtension->DeferredCheckSumList);
            InitializeListHead(listEntry);
            defCheckEntry = CONTAINING_RECORD(listEntry, DEFERRED_CHECKSUM_ENTRY, ListEntry);

            /*
             *  We dequeued a deferredCheckSum entry.  DeferredCheckSumList is now empty,
             *  so after we drop the spinlock a DPC could race in and perform an out-of-order 
             *  checksum verification and possibly raise a false-positive bugcheck.  So we need to prevent this.
             */
            ASSERT(!DeviceExtension->CheckInProgress);  
            DeviceExtension->CheckInProgress++;
        }

        KeReleaseSpinLock(&DeviceExtension->SpinLock, oldIrql);

        if (defCheckEntry){
        
            AcquirePassiveLevelLock(DeviceExtension);
            VerifyOrStoreSectorCheckSum(    DeviceExtension, 
                                                              defCheckEntry->SectorNum, 
                                                              defCheckEntry->CheckSum, 
                                                              defCheckEntry->IsWrite,
                                                              TRUE,     // paging is ok now
                                                              (PIRP)(PVOID)(PUCHAR)defCheckEntry->IrpCopyBytes,
                                                              FALSE);
            DeviceExtension->DbgNumDeferredChecks++;
            ReleasePassiveLevelLock(DeviceExtension);

            FreeDeferredCheckSumEntry(DeviceExtension, defCheckEntry);
            
            KeAcquireSpinLock(&DeviceExtension->SpinLock, &oldIrql);
            ASSERT(DeviceExtension->CheckInProgress > 0);
            DeviceExtension->CheckInProgress--;
            KeReleaseSpinLock(&DeviceExtension->SpinLock, oldIrql);
        }
        else {
            break;
        }
    }

    /*
     *  If we need a critical recovery, do it now at passive level.
     *  The positioning of this call is very sensitive to race conditions.
     *  We have to make sure that this part of the workItem handling 
     *  will always follow the setting of the NeedCriticalRecovery flag.
     */
    if (DeviceExtension->NeedCriticalRecovery){
        AcquirePassiveLevelLock(DeviceExtension);
        DoCriticalRecovery(DeviceExtension);
        ReleasePassiveLevelLock(DeviceExtension);
    }
    
}


/*++

Routine Description:

    log the read I/O failure. [will convert ulBlocks into CRC Blocks]
    [CRC BlockNum = ulBlocks / CRC_BLOCK_UNIT]

Arguments:

    ulDiskId            -   disk id
    ulLogiceBlockAddr   -   Logic Block Address
    ulBlocks            -   the logic block number. (based on sector size)
    status              -   SRB_STATUS when read failed

Return Value:

    N/A

--*/
VOID
LogCRCReadFailure(
    IN ULONG       ulDiskId,
    IN ULONG       ulLogicalBlockAddr,
    IN ULONG       ulBlocks,
    IN NTSTATUS    status
    )
{
    //
    //  ulCRCAddr need to be CRC_BLOCK_UNIT aligned.
    //
    ULONG   ulCRCAddr     = ulLogicalBlockAddr / CRC_BLOCK_UNIT * CRC_BLOCK_UNIT;
    ULONG   ulCRCBlocks;
    
    //
    //  then update the ulBlock based on the new aligned block number
    //
    ulBlocks             += (ulLogicalBlockAddr - ulCRCAddr);
    
    ulCRCBlocks = ulBlocks / CRC_BLOCK_UNIT;
    
    if ( ulBlocks % CRC_BLOCK_UNIT )
        ulCRCBlocks ++;

    //
    //  now let's convert the ulCRCAddr to the real CRC Block Addr
    //
    ulCRCAddr  /= CRC_BLOCK_UNIT;

    #if DBG_WMI_TRACING
        WMI_TRACING((WMI_TRACING_CRC_READ_FAILED, "[_R_F] %u %X %X %X\n", ulDiskId, ulCRCAddr, ulCRCBlocks, status));
    #endif

}

/*++

Routine Description:

    log the write I/O failure. [will convert ulBlocks into CRC Blocks]
    [CRC BlockNum = ulBlocks / CRC_BLOCK_UNIT]

Arguments:

    ulDiskId            -   disk id
    ulLogiceBlockAddr   -   Logic Block Address
    ulBlocks            -   the logic block number. (based on sector size)
    status              -   SRB_STATUS when read failed

Return Value:

    N/A

--*/
VOID
LogCRCWriteFailure(
    IN ULONG       ulDiskId,
    IN ULONG       ulLogicalBlockAddr,
    IN ULONG       ulBlocks,
    IN NTSTATUS    status
    )
{
    //
    //  ulCRCAddr need to be CRC_BLOCK_UNIT aligned.
    //
    ULONG   ulCRCAddr     = ulLogicalBlockAddr / CRC_BLOCK_UNIT * CRC_BLOCK_UNIT;
    ULONG   ulCRCBlocks;
    

    //
    //  then update the ulBlock based on the new aligned block number
    //
    ulBlocks             += (ulLogicalBlockAddr - ulCRCAddr);
    
    ulCRCBlocks = ulBlocks / CRC_BLOCK_UNIT;
    
    if ( ulBlocks % CRC_BLOCK_UNIT )
        ulCRCBlocks ++;

    //
    //  now let's convert the ulCRCAddr to the real CRC Block Addr
    //
    ulCRCAddr  /= CRC_BLOCK_UNIT;

    #if DBG_WMI_TRACING
        WMI_TRACING((WMI_TRACING_CRC_WRITE_FAILED, "[CRC_W_F] %u %X %X %X\n", ulDiskId, ulCRCAddr, ulCRCBlocks, status));
    #endif                    
}


/*++

Routine Description:

    log the Write reset. In the case a CRC_BLOCK_UNIT is partially written
    a LogCRCWriteReset need to be issued to invalide the previous CRC block.
    [will convert ulBlocks into CRC Blocks] 
    [CRC BlockNum = ulBlocks / CRC_BLOCK_UNIT]

Arguments:

    ulDiskId            -   disk id
    ulLogiceBlockAddr   -   Logic Block Address
    ulBlocks            -   the logic block number. (based on sector size)
    
Return Value:

    N/A

--*/
VOID
LogCRCWriteReset(
    IN ULONG       ulDiskId,
    IN ULONG       ulLogicalBlockAddr,
    IN ULONG       ulBlocks
    )
{
    //
    //  ulCRCAddr need to be CRC_BLOCK_UNIT aligned.
    //
    ULONG   ulCRCAddr     = ulLogicalBlockAddr / CRC_BLOCK_UNIT * CRC_BLOCK_UNIT;
    ULONG   ulCRCBlocks;
    
    //
    //  then update the ulBlock based on the new aligned block number
    //
    ulBlocks             += (ulLogicalBlockAddr - ulCRCAddr);
    
    ulCRCBlocks = ulBlocks / CRC_BLOCK_UNIT;
    
    if ( ulBlocks % CRC_BLOCK_UNIT )
        ulCRCBlocks ++;

    //
    //  now let's convert the ulCRCAddr to the real CRC Block Addr
    //
    ulCRCAddr  /= CRC_BLOCK_UNIT;

    #if DBG_WMI_TRACING
        WMI_TRACING((WMI_TRACING_CRC_WRITE_RESET, "[CRC_W_R] %u %X %X\n", ulDiskId, ulCRCAddr, ulCRCBlocks));
    #endif                
}


/*
 *  NewDeferredCheckSumEntry
 *
 *
 */
PDEFERRED_CHECKSUM_ENTRY NewDeferredCheckSumEntry(  PDEVICE_EXTENSION DeviceExtension,
                                                                                            ULONG SectorNum,
                                                                                            USHORT CheckSum,
                                                                                            BOOLEAN IsWrite)
{
    PDEFERRED_CHECKSUM_ENTRY defCheckEntry;

    defCheckEntry = AllocPool(DeviceExtension, NonPagedPool, sizeof(DEFERRED_CHECKSUM_ENTRY), FALSE);
    if (defCheckEntry){
        defCheckEntry->IsWrite = IsWrite;
        defCheckEntry->SectorNum = SectorNum;
        defCheckEntry->CheckSum = CheckSum;
        InitializeListHead(&defCheckEntry->ListEntry);
    }
    
    return defCheckEntry;
}


VOID FreeDeferredCheckSumEntry( PDEVICE_EXTENSION DeviceExtension,
                                                                    PDEFERRED_CHECKSUM_ENTRY DefCheckSumEntry)
{
    ASSERT(IsListEmpty(&DefCheckSumEntry->ListEntry));
    FreePool(DeviceExtension, DefCheckSumEntry, NonPagedPool);
}


#if 0
    VOID
    ReportChecksumMismatch (
        PDEVICE_EXTENSION DeviceExtension,
        ULONG SectorNum,
        PIRP OriginalIrpOrCopy,
        BOOLEAN IsSynchronousCheck,
        USHORT RecordedChecksum,
        USHORT CurrentChecksum
        )
    {
        extern PBOOLEAN KdDebuggerEnabled;
        extern ULONG DbgPrompt(PCH Prompt, PCH Response, ULONG MaximumResponseLength);
        
        if (*KdDebuggerEnabled){
            UCHAR response[2] = {0};
            
            DbgPrint("\nDisk Integrity Verifier (crcdisk): checksum for sector %xh does not match (%xh (current) != %xh (recorded)), devObj=%ph ", SectorNum, (ULONG)CurrentChecksum, (ULONG)RecordedChecksum, DeviceExtension->DeviceObject);
            DbgPrompt("\n Re-read sector, Break (RB) ? _", (PUCHAR)response, sizeof(response));

            while (TRUE){
                switch (response[0]){
                    case 'R':
                    case 'r':
                        // BUGBUG FINISH
                        break;

                    case 'B':
                    case 'b':
                        DbgBreakPoint();
                        break;

                    default:
                        continue;
                        break;
                }
                
                break;
            }
            
        }
        else {
            /*
             *  No debugger present.  Raise an exception.
             */
            DeviceExtension->IsRaisingException = TRUE;                 
            DeviceExtension->ExceptionSector = SectorNum;
            DeviceExtension->ExceptionIrpOrCopyPtr = OriginalIrpOrCopy;
            DeviceExtension->ExceptionCheckSynchronous = IsSynchronousCheck;
            KeBugCheckEx(DRIVER_VERIFIER_DETECTED_VIOLATION,
                       (ULONG_PTR)(IsSynchronousCheck ? 0xA0 : 0xA1),
                       (ULONG_PTR)OriginalIrpOrCopy,
                       (ULONG_PTR)DeviceExtension->LowerDeviceObject,
                        (ULONG_PTR)SectorNum);
        }
        
    }
#endif


