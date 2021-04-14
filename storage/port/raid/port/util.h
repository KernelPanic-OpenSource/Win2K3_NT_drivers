/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    util.h

Abstract:

    Utilities for RAIDPORT driver.

Author:

    Matthew D Hendel (math) 20-Apr-2000

Revision History:

--*/

#pragma once



typedef enum _DEVICE_STATE {
    DeviceStateNotPresent       = 0,    // FDO only
    DeviceStateWorking          = 1,
    DeviceStateStopped          = 2,
    DeviceStatePendingStop      = 3,
    DeviceStatePendingRemove    = 4,
    DeviceStateSurpriseRemoval  = 5,
    DeviceStateDeleted          = 6,
    DeviceStateDisabled         = 7     // PDO only
} DEVICE_STATE, *PDEVICE_STATE;


DEVICE_STATE
INLINE
StorSetDeviceState(
    IN PDEVICE_STATE DeviceState,
    IN DEVICE_STATE NewDeviceState
    )
{
    DEVICE_STATE PriorState;

    //
    // NB: It is not necessary to perform this operation interlocked as
    // we will never receive multiple PNP irps for the same device object
    // simultaneously.
    //
    
    PriorState = *DeviceState;
    *DeviceState = NewDeviceState;

    return PriorState;
}


//
// Unless otherwise specified, the default timeout for requests originated
// in the port driver is 10 sec.
//

#define DEFAULT_IO_TIMEOUT      (10)

//
// Unless otherwise specified the default link timeout is 30 sec.
//

#define DEFAULT_LINK_TIMEOUT    (30)

//
// When we send down a SRB_FUNCTION_RESET_XXX (lun, target, bus) we must
// specify a reset timeout with the request. This timeout will be at
// minimum MINIMUM_RESET_TIMEOUT, and it may be larger if the default
// timeout for the Unit/HBA is larger.
//

#define MINIMUM_RESET_TIMEOUT   (30)

//
// The time to pause, in seconds, after we issue a bus-reset to an adapter.
//

#define DEFAULT_RESET_HOLD_TIME (4)

//
// Default rescan period is the shortest period we will not initiate a rescan
// on a QDR within.
//

#define DEFAULT_RESCAN_PERIOD   (30 * SECONDS)

//
// Number of elements in the tag queue, numbered 0 through TAG_QUEUE_SIZE - 1.
//

#define TAG_QUEUE_SIZE     (255)


//
// Number of times to retry inquiry commands. RetryCount of two means three
// total attempts.
//

#define RAID_INQUIRY_RETRY_COUNT    (2)

//
// Lengths of various identifiers.
//
                                     
#define SCSI_BUS_NAME_LENGTH            (sizeof ("SCSI"))
#define MAX_DEVICE_NAME_LENGTH          (sizeof ("Sequential"))
#define SCSI_VENDOR_ID_LENGTH           (8)
#define SCSI_PRODUCT_ID_LENGTH          (16)
#define SCSI_REVISION_ID_LENGTH         (4)
#define SCSI_SERIAL_NUMBER_LENGTH       (32)

#define MAX_GENERIC_DEVICE_NAME_LENGTH  (sizeof ("ScsiCardReader"))

#define HARDWARE_B_D_V_LENGTH       (SCSI_BUS_NAME_LENGTH +                 \
                                     MAX_DEVICE_NAME_LENGTH +               \
                                     SCSI_VENDOR_ID_LENGTH + 1)

#define HARDWARE_B_D_V_P_LENGTH     (HARDWARE_B_D_V_LENGTH +                \
                                     SCSI_PRODUCT_ID_LENGTH)

#define HARDWARE_B_D_V_P_R_LENGTH   (HARDWARE_B_D_V_P_LENGTH +              \
                                     SCSI_REVISION_ID_LENGTH)

#define HARDWARE_B_V_P_LENGTH       (SCSI_BUS_NAME_LENGTH +                 \
                                     SCSI_VENDOR_ID_LENGTH +                \
                                     SCSI_PRODUCT_ID_LENGTH + 1)

#define HARDWARE_B_V_P_R0_LENGTH    (SCSI_BUS_NAME_LENGTH +                 \
                                     SCSI_VENDOR_ID_LENGTH +                \
                                     SCSI_PRODUCT_ID_LENGTH +               \
                                     SCSI_REVISION_ID_LENGTH + 1)

#define HARDWARE_V_P_R0_LENGTH      (SCSI_VENDOR_ID_LENGTH +                \
                                     SCSI_PRODUCT_ID_LENGTH +               \
                                     SCSI_REVISION_ID_LENGTH + 1)


#define HARDWARE_ID_LENGTH          (HARDWARE_B_D_V_LENGTH +                \
                                     HARDWARE_B_D_V_P_LENGTH +              \
                                     HARDWARE_B_D_V_P_R_LENGTH +            \
                                     HARDWARE_B_V_P_LENGTH +                \
                                     HARDWARE_B_V_P_R0_LENGTH +             \
                                     HARDWARE_V_P_R0_LENGTH +               \
                                     MAX_GENERIC_DEVICE_NAME_LENGTH + 2)


#define DEVICE_ID_LENGTH            (HARDWARE_B_D_V_P_R_LENGTH +            \
                                     sizeof ("&Ven_") +                     \
                                     sizeof ("&Prod_") +                    \
                                     sizeof ("&Rev_"))


#define INSTANCE_ID_LENGTH          (20)

#define COMPATIBLE_ID_LENGTH        (SCSI_BUS_NAME_LENGTH +                 \
                                     MAX_DEVICE_NAME_LENGTH +               \
                                     1 +                                    \
                                     sizeof ("SCSI\\RAW") +                 \
                                     1 +                                    \
                                     1)                                     \




//
// Max wait is the length to wait for the remlock, in minutes.
// The high water is the estimated high water mark.
//

#define REMLOCK_MAX_WAIT            (1)         // Minutes
#define REMLOCK_HIGH_MARK           (1000)      

//
// Minor code used by the HBA to signal to the LUN that this is
// an enumeration IRP. The major code is IRP_MJ_SCSI.
//

#define STOR_MN_ENUMERATION_IRP     (0xF0)

ULONG
RaidMinorFunctionFromIrp(
    IN PIRP Irp
    );

ULONG
RaidMajorFunctionFromIrp(
    IN PIRP Irp
    );

ULONG
RaidIoctlFromIrp(
    IN PIRP Irp
    );

PSCSI_REQUEST_BLOCK
RaidSrbFromIrp(
    IN PIRP Irp
    );

UCHAR
RaidSrbFunctionFromIrp(
    IN PIRP Irp
    );

UCHAR
RaidScsiOpFromIrp(
    IN PIRP Irp
    );

NTSTATUS
RaidNtStatusFromScsiStatus(
    IN ULONG ScsiStatus
    );

UCHAR
RaidNtStatusToSrbStatus(
    IN NTSTATUS Status
    );

NTSTATUS
RaidNtStatusFromBoolean(
    IN BOOLEAN Succ
    );

POWER_STATE_TYPE
RaidPowerTypeFromIrp(
    IN PIRP Irp
    );

POWER_STATE
RaidPowerStateFromIrp(
    IN PIRP Irp
    );

INTERFACE_TYPE
RaGetBusInterface(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
RaForwardIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RaSendIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
    
NTSTATUS
RaForwardIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
RaForwardPowerIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );  

NTSTATUS
RaDuplicateUnicodeString(
    OUT PUNICODE_STRING DestString,
    IN PUNICODE_STRING SourceString,
    IN POOL_TYPE Pool,
    IN PVOID IoObject
    );

ULONG
RaSizeOfCmResourceList(
    IN PCM_RESOURCE_LIST ResourceList
    );

PCM_RESOURCE_LIST
RaDuplicateCmResourceList(
    IN POOL_TYPE PoolType,
    IN PCM_RESOURCE_LIST ResourceList,
    IN ULONG Tag
    );

NTSTATUS
RaQueryInterface(
    IN PDEVICE_OBJECT DeviceObject,
    IN PCGUID InterfaceType,
    IN USHORT InterfaceSize,
    IN USHORT InterfaceVersion,
    IN PINTERFACE Interface,
    IN PVOID InterfaceSpecificData
    );

VOID
RaFixupIds(
    IN PWCHAR Id,
    IN BOOLEAN MultiSz
    );

VOID
RaCopyPaddedString(
    OUT PCHAR Dest,
    IN ULONG DestLength,
    IN PCHAR Source,
    IN ULONG SourceLength
    );


typedef struct _RAID_FIXED_POOL {

    //
    // Buffer to allocate from.
    //
    
    PUCHAR Buffer;

    //
    // Number of elements in the pool.
    //
    
    ULONG NumberOfElements;

    //
    // Size of each element.
    //
    
    SIZE_T SizeOfElement;

} RAID_FIXED_POOL, *PRAID_FIXED_POOL;



VOID
RaidCreateFixedPool(
    IN PRAID_FIXED_POOL Pool
    );

VOID
RaidInitializeFixedPool(
    OUT PRAID_FIXED_POOL Pool,
    IN PVOID Buffer,
    IN ULONG NumberOfElements,
    IN SIZE_T SizeOfElement
    );

VOID
RaidDeleteFixedPool(
    IN PRAID_FIXED_POOL Pool
    );

PVOID
RaidAllocateFixedPoolElement(
    IN PRAID_FIXED_POOL Pool,
    IN ULONG Index
    );

PVOID
RaidGetFixedPoolElement(
    IN PRAID_FIXED_POOL Pool,
    IN ULONG Index
    );

VOID
RaidFreeFixedPoolElement(
    IN PRAID_FIXED_POOL Pool,
    IN ULONG Element
    );

//
// A list for managing entries in the tagged queue list.
//

typedef struct _QUEUE_TAG_LIST {

    //
    // Spinlock held while accessing the queue list.
    //
    
    KSPIN_LOCK Lock;

    //
    // Number of elements in the list.
    //
    
    ULONG Count;

    //
    // Hint to speed up tag allocation.
    //
    
    ULONG Hint;

    //
    // BitMap to hold the tag values.
    //
    
    RTL_BITMAP BitMap;

    //
    // BitMap Buffer.
    //

    PULONG Buffer;

    //
    // Number of outstanding tags. This could probably be DBG only.
    //

    ULONG OutstandingTags;

    //
    // The maximum number of tags that were outstanding at one time.
    //

    ULONG HighWaterMark;
    
    
} QUEUE_TAG_LIST, *PQUEUE_TAG_LIST;


VOID
RaCreateTagList(
    OUT PQUEUE_TAG_LIST TagList
    );

VOID
RaDeleteTagList(
    IN PQUEUE_TAG_LIST TagList
    );

NTSTATUS
RaInitializeTagList(
    IN OUT PQUEUE_TAG_LIST TagList,
    IN ULONG TagCount,
    IN PVOID IoObject
    );

ULONG
RaAllocateTag(
    IN OUT PQUEUE_TAG_LIST TagList
    );

ULONG
RaAllocateSpecificTag(
    IN OUT PQUEUE_TAG_LIST TagList,
    IN ULONG SpecificTag
    );

VOID
RaFreeTag(
    IN OUT PQUEUE_TAG_LIST TagList,
    IN ULONG Tag
    );
    


//
// RAID_MEMORY_REGION represents a region of physical contiguous memory.
// Generally, this is used for DMA common buffer regions.
//

typedef struct _RAID_MEMORY_REGION {

    //
    // Beginning virtual address of the region.
    //
    
    PUCHAR VirtualBase;

    //
    // Beginning physical address of the region.
    //
    
    PHYSICAL_ADDRESS PhysicalBase;

    //
    // Length of the region. (Is there any need to make this a SIZE_T
    // value?)
    //
    
    ULONG Length;
    
} RAID_MEMORY_REGION, *PRAID_MEMORY_REGION;


//
// Creation and destruction of the memory region.
//

VOID
INLINE
RaidCreateRegion(
    OUT PRAID_MEMORY_REGION Region
    )
{
    RtlZeroMemory (Region, sizeof (RAID_MEMORY_REGION));
}

VOID
INLINE
RaidInitializeRegion(
    IN OUT PRAID_MEMORY_REGION Region,
    IN PVOID VirtualAddress,
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG Length
    )
{
    ASSERT (Region->Length == 0);

    Region->VirtualBase = VirtualAddress;
    Region->PhysicalBase = PhysicalAddress;
    Region->Length = Length;
}

VOID
INLINE
RaidDereferenceRegion(
    IN OUT PRAID_MEMORY_REGION Region
    )
{
    Region->VirtualBase = 0;
    Region->PhysicalBase.QuadPart = 0;
    Region->Length = 0;
}

BOOLEAN
INLINE
RaidIsRegionInitialized(
    IN PRAID_MEMORY_REGION Region
    )
{
    return (Region->Length != 0);
}

VOID
INLINE
RaidDeleteRegion(
    IN OUT PRAID_MEMORY_REGION Region
    )
{
    ASSERT (Region->VirtualBase == 0);
    ASSERT (Region->PhysicalBase.QuadPart == 0);
    ASSERT (Region->Length == 0);
}


//
// Operations on the memory region.
//

PVOID
INLINE
RaidRegionGetVirtualBase(
    IN PRAID_MEMORY_REGION Region
    )
{
    ASSERT (RaidIsRegionInitialized (Region));
    return Region->VirtualBase; 
}

PHYSICAL_ADDRESS
INLINE
RaidRegionGetPhysicalBase(
    IN PRAID_MEMORY_REGION Region
    )
{
    ASSERT (RaidIsRegionInitialized (Region));
    return Region->PhysicalBase;
}

ULONG
INLINE
RaidRegionGetSize(
    IN PRAID_MEMORY_REGION Region
    )
{
    ASSERT (RaidIsRegionInitialized (Region));
    return Region->Length;
}

BOOLEAN
INLINE
RaidRegionInPhysicalRange(
    IN PRAID_MEMORY_REGION Region,
    IN PHYSICAL_ADDRESS PhysicalAddress
    )
{
    return IN_RANGE (Region->PhysicalBase.QuadPart,
                     PhysicalAddress.QuadPart,
                     Region->PhysicalBase.QuadPart + Region->Length);
}

BOOLEAN
INLINE
RaidRegionInVirtualRange(
    IN PRAID_MEMORY_REGION Region,
    IN PVOID VirtualAddress
    )
{
    return IN_RANGE (Region->VirtualBase,
                     (PUCHAR)VirtualAddress,
                     Region->VirtualBase + Region->Length);
}

BOOLEAN
INLINE
RaidRegionGetPhysicalAddress(
    IN PRAID_MEMORY_REGION Region,
    IN PVOID VirtualAddress,
    OUT PPHYSICAL_ADDRESS PhysicalAddress,
    OUT PULONG Length OPTIONAL
    )
/*++

Routine Description:

    Get a physical address for a specific virtual address within
    the region.

Arguments:

    Region - Supplies a pointer to a region object that contain the
            specified virtual address.

    VirtualAddress - Supplies the source virtual address.

    PhysicalAddress - Buffer where the physical address for this virtual
            address will be stored on success.

    Length - Optional out parameter taking the length of the physical
            region that is valid.

Return Value:

    TRUE - If the operation succeeded.

    FALSE - If the virtual address was out of range.

--*/
{
    ULONG Offset;

    //
    // If the virtual address isn't within range, fail.
    //
    
    if (!RaidRegionInVirtualRange (Region, VirtualAddress)) {
        return FALSE;
    }

    Offset = (ULONG)((PUCHAR)VirtualAddress - Region->VirtualBase);
    PhysicalAddress->QuadPart = Region->PhysicalBase.QuadPart + Offset;

    if (Length) {
        *Length = Region->Length - Offset;
    }

    //
    // Check that we did the fixup correctly.
    //
    
    ASSERT (RaidRegionInPhysicalRange (Region, *PhysicalAddress));
    
    return TRUE;
}

    

BOOLEAN
INLINE
RaidRegionGetVirtualAddress(
    IN PRAID_MEMORY_REGION Region,
    IN PHYSICAL_ADDRESS PhysicalAddress,
    OUT PVOID* VirtualAddress,
    OUT PULONG Length OPTIONAL
    )
{
    ULONG Offset;
    
    //
    // If the physical address isn't within range, fail.
    //
    
    if (!RaidRegionInPhysicalRange (Region, PhysicalAddress)) {
        return FALSE;
    }

    Offset = (ULONG)(Region->PhysicalBase.QuadPart - PhysicalAddress.QuadPart);
    VirtualAddress = (PVOID)(Region->VirtualBase + Offset);

    if (Length) {
        *Length = Region->Length - Offset;
    }

    return TRUE;
}


NTSTATUS
RaidAllocateAddressMapping(
    IN PMAPPED_ADDRESS* ListHead,
    IN PHYSICAL_ADDRESS Address,
    IN PVOID MappedAddress,
    IN ULONG NumberOfBytes,
    IN ULONG BusNumber,
    IN PVOID IoObject
    );

NTSTATUS
RaidFreeAddressMapping(
    IN PMAPPED_ADDRESS* ListHead,
    IN PVOID MappedAddress
    );

NTSTATUS
RaidHandleCreateCloseIrp(
    IN DEVICE_STATE DeviceState,
    IN PIRP Irp
    );


//
// Irp state tracking
//

typedef struct _EX_DEVICE_QUEUE_ENTRY {
    LIST_ENTRY DeviceListEntry;
    ULONG SortKey;
    BOOLEAN Inserted;
    UCHAR State;
    struct {
        UCHAR Solitary : 1;
        UCHAR reserved0 : 7;
        
    };
    UCHAR reserved1;
} EX_DEVICE_QUEUE_ENTRY, *PEX_DEVICE_QUEUE_ENTRY;

C_ASSERT (sizeof (EX_DEVICE_QUEUE_ENTRY) == sizeof (KDEVICE_QUEUE_ENTRY));


//
// Port processing irp means the port driver is currently executing
// instructions to complete the irp. The irp is NOT waiting for
// resources on any queue.
//

#define RaidPortProcessingIrp           (0xA8)

//
// Pending resources is when the irp is in an IO queue awaiting
// resources.
//

#define RaidPendingResourcesIrp         (0xA9)

//
// The irp moves into state WaitingIoQueue callback when it's awaiting
// the ioqueue to call it back, e.g., in the solitary request processing
// logic. Most requests will not take on this state, and instead will
// transition directly from RaidPendingResources -> RaidMiniportProcessing.
//

#define RaidWaitingIoQueueCallback      (0xAD)

//
// The irp takes on state Miniport Processing while the miniport has
// control over the irp. That is, between the time we call HwStartIo
// and when the miniport calls ScsiPortNotification with a completion
// status for the irp.
//

#define RaidMiniportProcessingIrp       (0xAA)

//
// The irp takes on the Pending Completion state when it is moved to
// the completed list.
//

#define RaidPendingCompletionIrp        (0xAB)

//
// We set the irp state to Completed just before we call IoCompleteRequest
// for the irp.
//

#define RaidCompletedIrp                (0xAC)


typedef UCHAR RAID_IRP_STATE;

VOID
INLINE
RaidSetIrpState(
    IN PIRP Irp,
    IN RAID_IRP_STATE State
    )
{
    ((PEX_DEVICE_QUEUE_ENTRY)&Irp->Tail.Overlay.DeviceQueueEntry)->State = State;
}

    

RAID_IRP_STATE
INLINE
RaidGetIrpState(
    IN PIRP Irp
    )
{
    return ((PEX_DEVICE_QUEUE_ENTRY)&Irp->Tail.Overlay.DeviceQueueEntry)->State;
}

VOID
INLINE
RaidSetEntryState(
    IN PKDEVICE_QUEUE_ENTRY Entry,
    IN RAID_IRP_STATE State
    )
{
    ((PEX_DEVICE_QUEUE_ENTRY)Entry)->State = State;
}

//
// Completion wrapper function.
//

NTSTATUS
INLINE
RaidCompleteRequestEx (
    IN PIRP Irp,
    IN CCHAR PriorityBoost,
    IN NTSTATUS Status
    )
{
    RAID_IRP_STATE IrpState;

    IrpState = RaidGetIrpState (Irp);

    ASSERT (IrpState == RaidPortProcessingIrp ||
            IrpState == RaidPendingResourcesIrp ||
            IrpState == RaidMiniportProcessingIrp ||
            IrpState == RaidPendingCompletionIrp);

    RaidSetIrpState (Irp, RaidCompletedIrp);
    Irp->IoStatus.Status = Status;

    IoCompleteRequest (Irp, PriorityBoost);

    return Status;
}
    
NTSTATUS
INLINE
RaidCompleteRequest(
    IN PIRP Irp,
    IN NTSTATUS Status
    )
{
    return RaidCompleteRequestEx (Irp, IO_NO_INCREMENT, Status);
}



//
// Error log information
//

typedef struct _RAID_ALLOCATION_ERROR {
    IO_ERROR_LOG_PACKET Packet;
    POOL_TYPE PoolType;
    SIZE_T NumberOfBytes;
    ULONG Tag;
} RAID_ALLOCATION_ERROR, *PRAID_ALLOCATION_ERROR;

typedef struct _RAID_IO_ERROR {
    IO_ERROR_LOG_PACKET Packet;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR _unused;
    ULONG ErrorCode;
    ULONG UniqueId;
} RAID_IO_ERROR, *PRAID_IO_ERROR;


PVOID
RaidAllocatePool(
    IN POOL_TYPE PoolType,
    IN SIZE_T NumberOfBytes,
    IN ULONG Tag,
    IN PVOID IoObject
    );

//
// Memory allocated with RaidAllocatePool MUST be freed by RaidFreePool.
//

#define RaidFreePool ExFreePoolWithTag


#define VERIFY_DISPATCH_LEVEL() ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);


extern LONG RaidUnloggedErrors;

#define RAID_ERROR_NO_MEMORY    (10)

ULONG
RaidScsiErrorToIoError(
    IN ULONG ErrorCode
    );


PVOID
RaidGetSystemAddressForMdl(
    IN PMDL,
    IN MM_PAGE_PRIORITY Priority,
    IN PVOID DeviceObject
    );
    
NTSTATUS
StorCreateScsiSymbolicLink(
    IN PUNICODE_STRING DeviceName,
    OUT PULONG PortNumber OPTIONAL
    );

NTSTATUS
StorDeleteScsiSymbolicLink(
    IN ULONG ScsiPortNumber
    );

ULONG
RaidCreateDeviceName(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PUNICODE_STRING DeviceName
    );

NTSTATUS
StorDuplicateUnicodeString(
    IN PUNICODE_STRING Source,
    IN PUNICODE_STRING Dest
    );

#define RtlDuplicateUnicodeString(X,Y,Z) (StorDuplicateUnicodeString(Y,Z))



VOID
INLINE
ASSERT_IO_OBJECT(
    IN PVOID IoObject
    )
{
    //
    // The IO object must be either a device object or a driver object.
    // NB: Should probably protect this with a read check as well. 
    //
    
    ASSERT (IoObject != NULL);
    ASSERT (((PDEVICE_OBJECT)IoObject)->Type == IO_TYPE_DEVICE ||
            ((PDRIVER_OBJECT)IoObject)->Type == IO_TYPE_DRIVER);
}

BOOLEAN
StorCreateAnsiString(
    OUT PANSI_STRING AnsiString,
    IN PCSTR String,
    IN ULONG Length,
    IN POOL_TYPE PoolType,
    IN PVOID IoObject
    );

VOID
StorFreeAnsiString(
    IN PANSI_STRING String
    );

PIRP
StorBuildSynchronousScsiRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    OUT PKEVENT Event,
    OUT PIO_STATUS_BLOCK IoStatusBlock
    );


//++
//
// BOOLEAN
// C_MATCH_FIELD_OFFSET(
//     Type1,
//     Type2,
//     FieldName
//     );
//
// Routine Description:
//
//   Verify that a field is at the same offset in one type as another.
//   This is done at compile time, so may be a part of a compile-time
//   C_ASSERT.
//
// Return Values:
//
//    TRUE - if the field offsets match.
//
//    FALSE - if the field offsets do not match.
//
//-- 

#define C_MATCH_FIELD_OFFSET(Type1, Type2, FieldName)\
    (FIELD_OFFSET (Type1, FieldName) == FIELD_OFFSET (Type1, FieldName))


//
// Verify that the kernel's SCATTER_GATHER_ELEMENT is the same as the
// storport.h STOR_SCATTER_GATHER_ELEMENT.
//
    
C_ASSERT (C_MATCH_FIELD_OFFSET (STOR_SCATTER_GATHER_ELEMENT, SCATTER_GATHER_ELEMENT, PhysicalAddress) &&
          C_MATCH_FIELD_OFFSET (STOR_SCATTER_GATHER_ELEMENT, SCATTER_GATHER_ELEMENT, Length) &&
          C_MATCH_FIELD_OFFSET (STOR_SCATTER_GATHER_ELEMENT, SCATTER_GATHER_ELEMENT, Reserved));

//
// Verify that the kernel's SCATTER_GATHER_LIST is the same as the storport.h
// STOR_SCATTER_GATHER_LIST. We just cast the list from one type to the other,
// so the had better be the same.
//

C_ASSERT (C_MATCH_FIELD_OFFSET (STOR_SCATTER_GATHER_LIST, SCATTER_GATHER_LIST, NumberOfElements) &&
          C_MATCH_FIELD_OFFSET (STOR_SCATTER_GATHER_LIST, SCATTER_GATHER_LIST, List));


VOID
RaidCancelIrp(
    IN PIO_QUEUE IoQueue,
    IN PVOID Context,
    IN PIRP Irp
    );

VOID
RaidCompleteRequestCallback(
    IN PSTOR_EVENT_QUEUE Queue,
    IN PVOID Context,
    IN PSTOR_EVENT_QUEUE_ENTRY Entry,
    IN PVOID RemoveEventRoutine
    );

VOID
RaidCompleteMiniportRequestCallback(
    IN PSTOR_EVENT_QUEUE Queue,
    IN PVOID Context,
    IN PSTOR_EVENT_QUEUE_ENTRY Entry,
    IN PVOID RemoveEventRoutine
    );

NTSTATUS
StorWaitForSingleObject(
    IN PVOID Object,
    IN BOOLEAN Alertable,
    IN PLONGLONG Timeout
    );

//
// The TEXT_SECTION macro is used to signify that a specific function resides
// within a specific code section. This is used by the an external tool to
// build the ALLOC_PRAGMA table(s).
//

#define TEXT_SECTION(SectionName)

//
// ASSERT that a UNICODE_STRING is NULL terminated, this is important as some
// functions require it.
//

#define ASSERT_UNC_STRING_IS_SZ(String)\
    ASSERT ((String)->Buffer != NULL &&\
            ((String)->MaximumLength > String->Length) &&\
            ((String)->Buffer [(String)->Length / sizeof (WCHAR)] == UNICODE_NULL))



//
// The type SMALL_INQUIRY_DATA is the first INQUIRYDATABUFFERSIZE bytes of
// the INQUIRY_DATA structure. Explicitly defining this structure avoids
// error prone pointer arithmetic when managing arrays of inquiry data.
//

typedef struct _SMALL_INQUIRY_DATA {
    UCHAR DeviceType : 5;
    UCHAR DeviceTypeQualifier : 3;
    UCHAR DeviceTypeModifier : 7;
    UCHAR RemovableMedia : 1;
    union {
        UCHAR Versions;
        struct {
            UCHAR ANSIVersion : 3;
            UCHAR ECMAVersion : 3;
            UCHAR ISOVersion : 2;
        };
    };
    UCHAR ResponseDataFormat : 4;
    UCHAR HiSupport : 1;
    UCHAR NormACA : 1;
    UCHAR TerminateTask : 1;
    UCHAR AERC : 1;
    UCHAR AdditionalLength;
    UCHAR Reserved;
    UCHAR Addr16 : 1;               // defined only for SIP devices.
    UCHAR Addr32 : 1;               // defined only for SIP devices.
    UCHAR AckReqQ: 1;               // defined only for SIP devices.
    UCHAR MediumChanger : 1;
    UCHAR MultiPort : 1;
    UCHAR ReservedBit2 : 1;
    UCHAR EnclosureServices : 1;
    UCHAR ReservedBit3 : 1;
    UCHAR SoftReset : 1;
    UCHAR CommandQueue : 1;
    UCHAR TransferDisable : 1;      // defined only for SIP devices.
    UCHAR LinkedCommands : 1;
    UCHAR Synchronous : 1;          // defined only for SIP devices.
    UCHAR Wide16Bit : 1;            // defined only for SIP devices.
    UCHAR Wide32Bit : 1;            // defined only for SIP devices.
    UCHAR RelativeAddressing : 1;
    UCHAR VendorId[8];
    UCHAR ProductId[16];
    UCHAR ProductRevisionLevel[4];
} SMALL_INQUIRY_DATA, *PSMALL_INQUIRY_DATA;

//
// Check that we actually did this correctly.
//

C_ASSERT (sizeof (SMALL_INQUIRY_DATA) == INQUIRYDATABUFFERSIZE);

//
// The SCSI_INQUIRY_DATA_INTERNAL structure is used for processing the
// IOCTL_SCSI_GET_INQUIRY_DATA command. It has the SMALL_INQUIRY_DATA
// embedded into the struct to avoid pointer arithmetic.
//

typedef struct _SCSI_INQUIRY_DATA_INTERNAL {
    UCHAR  PathId;
    UCHAR  TargetId;
    UCHAR  Lun;
    BOOLEAN  DeviceClaimed;
    ULONG  InquiryDataLength;
    ULONG  NextInquiryDataOffset;
    SMALL_INQUIRY_DATA InquiryData;
} SCSI_INQUIRY_DATA_INTERNAL, *PSCSI_INQUIRY_DATA_INTERNAL;


//
// This struct is used in the processing of IOCTL_SCSI_GET_INQUIRY_DATA.
//

typedef struct TEMPORARY_INQUIRY_BUS_INFO {
    ULONG NumberOfLogicalUnits;
    ULONG CurrentLun;
    PSCSI_INQUIRY_DATA_INTERNAL InquiryArray;   
} TEMPORARY_INQUIRY_BUS_INFO, *PTEMPORARY_INQUIRY_BUS_INFO;


#define ASSERT_POINTER_ALIGNED(Pointer)\
    ASSERT (ALIGN_DOWN_POINTER (Pointer, sizeof (PVOID)) == (PVOID)Pointer)

//
// It's dumb that we have to have private versions of the ALIGN_XXXX macros,
// but the macros as defined only deal with types, not sizes.
//

#define ALIGN_DOWN_LENGTH(length, size) \
    ((ULONG)(length) & ~((size) - 1))

#define ALIGN_UP_LENGTH(length, size) \
    (ALIGN_DOWN_LENGTH(((ULONG)(length) + (size) - 1), size))

#define ALIGN_DOWN_POINTER_LENGTH(address, size) \
        ((PVOID)((ULONG_PTR)(address) & ~((ULONG_PTR)(size) - 1)))

#define ALIGN_UP_POINTER_LENGTH(address, size) \
        (ALIGN_DOWN_POINTER_LENGTH(((ULONG_PTR)(address) + size - 1), size))


LARGE_INTEGER
FORCEINLINE
LARGE(
    IN ULONG64 Input
    )
{
    LARGE_INTEGER Output;

    Output.QuadPart = Input;
    return Output;
}
