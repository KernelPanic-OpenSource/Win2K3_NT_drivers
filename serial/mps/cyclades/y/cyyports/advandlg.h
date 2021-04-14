#ifndef PP_H
#define PP_H

#define SERIAL_ADVANCED_SETTINGS
#include "msports.h"

#ifdef USE_P_TRACE_ERR
#define P_TRACE_ERR(_x) MessageBox( GetFocus(), TEXT(_x), TEXT("ports traceerr"), MB_OK | MB_ICONINFORMATION );
#define W_TRACE_ERR(_x) MessageBox( GetFocus(), _x, TEXT("ports traceerr"), MB_OK | MB_ICONINFORMATION );
#else
#define P_TRACE_ERR(_x)
#define W_TRACE_ERR(_x)
#endif

#define DO_COM_PORT_RENAMES

#define RX_MIN 1
#define RX_MAX 14
#define TX_MIN 1
#define TX_MAX 16

TCHAR m_szDevMgrHelp[];

#if defined(_X86_)
//
// For NEC PC98. Following definition comes from user\inc\kbd.h.
// The value must be the same as value in kbd.h.
//
#define NLSKBD_OEM_NEC   0x0D
#endif // FE_SB && _X86_

//
// Structures
//

typedef struct _ADVANCED_DATA
{
    BOOL   HidePolling;
    BOOL   UseFifoBuffersControl;
    BOOL   UseFifoBuffers;
    BOOL   UseRxFIFOControl;
    BOOL   UseTxFIFOControl;
    DWORD  FifoRxMax;
    DWORD  FifoTxMax;
    DWORD  RxFIFO;
    DWORD  TxFIFO;
    DWORD  PollingPeriod;

    TCHAR  szComName[20];
    TCHAR  szNewComName[20];

    HKEY             hDeviceKey;         // (like ROOT\LEGACY_BEEP\0000)
    HCOMDB           hComDB;

    HDEVINFO         DeviceInfoSet;
    PSP_DEVINFO_DATA DeviceInfoData;

} ADVANCED_DATA, *PADVANCED_DATA;


///////////////////////////////////////////////////////////////////////////////////
// Advanced Dialog Prototypes
///////////////////////////////////////////////////////////////////////////////////

ULONG
FillPortNameCb(
    HWND           ParentHwnd,
    PADVANCED_DATA Params
    );

INT_PTR APIENTRY
AdvancedPortsDlgProc(
    IN HWND   hDlg,
    IN UINT   uMessage,
    IN WPARAM wParam,
    IN LPARAM lParam
    );

ULONG
FillAdvancedDlg(
    IN HWND             DialogHwnd,
    IN PADVANCED_DATA   AdvancedData
    );

BOOL
DisplayAdvancedDialog(
    IN     HWND             DialogHwnd,
    IN OUT PADVANCED_DATA   AdvancedData
);

void
InitializeControls(
    IN HWND             DialogHwnd,
    IN PADVANCED_DATA   AdvancedData
    );

void
RestoreAdvancedDefaultState(
    IN HWND             DialogHwnd,
    IN PADVANCED_DATA   AdvancedData
    );

ULONG
SaveAdvancedSettings(
    IN HWND             DialogHwnd,
    IN PADVANCED_DATA   AdvancedData
    );

void
SetTrackbarTicks(
    IN HWND   TrackbarHwnd,
    IN DWORD  MinVal,
    IN DWORD  MaxVal
    );

void
SetLabelText(
    IN HWND  LabelHwnd, 
    IN DWORD ResId,
    IN ULONG Value
    );

void
HandleTrackbarChange(
    IN HWND DialogHwnd, 
    IN HWND TrackbarHwnd
    );

void
EnableFifoControls(
    HWND DialogHwnd,
    BOOL Enabled
    );

// Context help header file and arrays for devmgr ports tab
// Created 2/21/98 by WGruber NTUA and DoronH NTDEV

//
// "Cyclom-Y Port Advanced Settings" Dialog Box
//

#define IDH_NOHELP      ((DWORD)-1)

#define IDH_DEVMGR_PORTSET_ADVANCED     15840   // "&Advanced" (Button)
#define IDH_DEVMGR_PORTSET_BPS      15841       // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_DATABITS     15842   // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_PARITY       15843   // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_STOPBITS     15844   // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_FLOW     15845       // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_DEFAULTS     15892   // "&Restore Defaults" (Button)

//
// "Advanced Communications Port Properties" Dialog Box
//
#define IDH_DEVMGR_PORTSET_ADV_USEFIFO  16885   // "&Use FIFO buffers (requires 16550 compatible UART)" (Button)
#define IDH_DEVMGR_PORTSET_ADV_TRANS    16842   // "" (msctls_trackbar32)
#define IDH_DEVMGR_PORTSET_ADV_DEVICES  161027  // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_ADV_RECV         16821       // "" (msctls_trackbar32)
#define IDH_DEVMGR_PORTSET_ADV_NUMBER   16846   // "" (ComboBox)
#define IDH_DEVMGR_PORTSET_ADV_DEFAULTS 16844

#endif // PP_H

