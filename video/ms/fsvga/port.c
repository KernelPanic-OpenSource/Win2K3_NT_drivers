/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    port.c

Abstract:

    This is the console fullscreen driver for the VGA card.

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "fsvga.h"

#define MAKEWORD(a, b) (USHORT)((((USHORT)a) & 0xFF) | ((((USHORT)(b)) << 8) & 0xFF00))

VOID
GetHardwareScrollReg(
    PPORT_LIST PortList,
    PEMULATE_BUFFER_INFORMATION EmulateInfo
    )

/*++

Routine Description:

    This routine gets the hardware scrolls register value.

Arguments:

Return Value:

--*/

{
    UCHAR low;
    UCHAR high;
    UCHAR mid;

    WRITE_PORT_UCHAR(PortList[CRTCAddressPortColor].Port,
                      IND_START_ADRS_L);
    low = READ_PORT_UCHAR(PortList[CRTCDataPortColor].Port);

    WRITE_PORT_UCHAR(PortList[CRTCAddressPortColor].Port,
                      IND_START_ADRS_H);
    high = READ_PORT_UCHAR(PortList[CRTCDataPortColor].Port);

    EmulateInfo->StartAddress = MAKEWORD(low, high);

    WRITE_PORT_UCHAR(PortList[CRTCAddressPortColor].Port,
                      IND_LINE_COMPARE);
    low = READ_PORT_UCHAR(PortList[CRTCDataPortColor].Port);

    WRITE_PORT_UCHAR(PortList[CRTCAddressPortColor].Port,
                      IND_LINE_COMPARE8);
    mid = READ_PORT_UCHAR(PortList[CRTCDataPortColor].Port);
    mid = (mid >> 4) & 1;

    WRITE_PORT_UCHAR(PortList[CRTCAddressPortColor].Port,
                      IND_LINE_COMPARE9);
    high = READ_PORT_UCHAR(PortList[CRTCDataPortColor].Port);
    high = (high >> 5) & 2;

    high |= mid;
    EmulateInfo->LineCompare = MAKEWORD(low, high);
}

VOID
SetGRAMWriteMode(
    PPORT_LIST PortList
    )

/*++

Routine Description:

    This routine sets the write mode of graphics register.

Arguments:

Return Value:

--*/

{
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_GRAPH_MODE, M_PROC_WRITE+M_DATA_READ));

    //
    // Set up to write data without interacting with the latches.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_DATA_ROTATE, DR_SET));

    //
    // Enable all the available EGA planes.
    //
    WRITE_PORT_USHORT(PortList[SEQAddressPort].Port,
                      MAKEWORD(IND_MAP_MASK, GRAPH_ADDR_MASK));
    //
    // Use all pixel positions.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_BIT_MASK, BIT_MASK_DEFAULT));

}

VOID
SetGRAMCopyMode(
    PPORT_LIST PortList
    )

/*++

Routine Description:

    This routine sets the copy mode of graphics register.

Arguments:

Return Value:

--*/

{
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_GRAPH_MODE, M_LATCH_WRITE+M_COLOR_READ));

    //
    // Set up to write data without interacting with the latches.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_DATA_ROTATE, DR_SET));

    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_COLOR_DONT_CARE, 0));
}

VOID
SetGRAMInvertMode(
    PPORT_LIST PortList
    )

/*++

Routine Description:

    This routine sets the invert mode of graphics register.

Arguments:

Return Value:

--*/

{
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_GRAPH_MODE, M_AND_WRITE+M_COLOR_READ));

    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_DATA_ROTATE, DR_XOR));

    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_BIT_MASK, BIT_MASK_DEFAULT));

    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_COLOR_DONT_CARE, 0));

    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET, 0xff));
}

VOID
set_opaque_bkgnd_proc(
    PPORT_LIST PortList,
    PEMULATE_BUFFER_INFORMATION EmulateInfo,
    PUCHAR FrameBuffer,
    USHORT Attributes
    )

/*++

set_opaque_bkgnd

  Set the VGA registers for drawing a full screen byte with opaque
  font and opaque background.

Created.

--*/

{
    UCHAR ColorFg = Attributes & 0x0f;
    UCHAR ColorBg = (Attributes & 0xf0) >> 4;

    if (Attributes & COMMON_LVB_REVERSE_VIDEO)
    {
        Attributes = ColorBg;
        ColorBg = ColorFg;
        ColorFg = (UCHAR)Attributes;
    }

    if (EmulateInfo->ColorFg == ColorFg &&
        EmulateInfo->ColorBg == ColorBg)
        return;

    EmulateInfo->ColorFg = ColorFg;
    EmulateInfo->ColorBg = ColorBg;

    ColorSetDirect(PortList, FrameBuffer, ColorFg, ColorBg);
}

VOID
ColorSetGridMask(
    PPORT_LIST PortList,
    UCHAR BitMask
    )
{

    //
    // That color is used for all planes.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET_ENABLE, GRAPH_ADDR_MASK));

    //
    // Change the Set/Reset register to be all set.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET, 0x07));

    //
    // Use specified pixel positions.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_BIT_MASK, BitMask));

    //
    // Set up to write data without interacting with the latches.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_DATA_ROTATE, DR_SET));
}

VOID
ColorSetDirect(
    PPORT_LIST PortList,
    PUCHAR FrameBuffer,
    UCHAR ColorFg,
    UCHAR ColorBg
    )

/*++

ColorSetDirect

  Set the VGA registers for drawing a full screen byte with opaque
  font and opaque background.

Created.

--*/

{
    //
    // Set up to write data without interacting with the latches.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_DATA_ROTATE, DR_SET));

    //
    // Put the background color in the Set/Reset register.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET, ColorBg));

    //
    // That color is used for all planes.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET_ENABLE, GRAPH_ADDR_MASK));

    //
    // This gets our background color into the latches.
    //
    AccessGRAM_WR(FrameBuffer, GRAPH_ADDR_MASK);

    //
    // Change the Set/Reset register to be all zeroes.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET, 0));

    //
    // The Set/Reset enable register now flags where the foreground/background colors are the same.
    //
    ColorFg = ~(ColorFg ^ ColorBg);
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_SET_RESET_ENABLE, ColorFg));

    //
    // Color differences will be xor'd with the latches.
    //
    WRITE_PORT_USHORT(PortList[GRAPHAddressPort].Port,
                      MAKEWORD(IND_DATA_ROTATE, DR_XOR));


}
