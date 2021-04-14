/*++
Copyright (C) Microsoft Corporation, 1998 - 1999

Module Name:

    RedBook.c

Abstract:

    This command line utility adds and removes redbook
    for a given drive.

Author:

    Henry Gabryjelski (henrygab)

Environment:

    user mode only

Notes:


Revision History:

    07-30-98 : Created

--*/

#include "propp.h"
#include "storprop.h"

//
// redefine these to do what i want them to.
// allows the appearance of structured c++ with
// the performance of c.
//

#ifdef TRY
#undef TRY
#endif

#ifdef LEAVE
#undef LEAVE
#endif

#ifdef FINALLY
#undef FINALLY
#endif

#define TRY
#define LEAVE   goto __label;
#define FINALLY __label:

//
// just to give out unique errors
//

#define ERROR_REDBOOK_FILTER        0x80ff00f0L
#define ERROR_REDBOOK_PASS_THROUGH  0x80ff00f1L


#if DBG

#ifdef UNICODE
#define DbgPrintAllMultiSz DbgPrintAllMultiSzW
#else
#define DbgPrintAllMultiSz DbgPrintAllMultiSzA
#endif // UNICODE

VOID DbgPrintAllMultiSzW(WCHAR *String)
{
    ULONG i = 0;
    while(*String != UNICODE_NULL) {
        DebugPrint((1, "StorProp => MultiSz %3d: %ws\n", i++, String));
        while (*String != UNICODE_NULL) {
            String++;
        }
        String++; // go past the first NULL
    }
}

VOID DbgPrintAllMultiSzA(CHAR *String)
{
    ULONG i = 0;
    while(*String != ANSI_NULL) {
        DebugPrint((1, "StorProp => MultiSz %3d: %ws\n", i++, String));
        while (*String != ANSI_NULL) {
            String++;
        }
        String++; // go past the first NULL
    }
}

#else // !DBG

#define DbgPrintAllMultiSz
#define DbgPrintAllMultiSz

#endif // DBG


////////////////////////////////////////////////////////////////////////////////
// Local prototypes, not exported anywhere

BOOL
IsUserAdmin();

LONG
RedbookpUpperFilterRegDelete(IN HDEVINFO HDevInfo, IN PSP_DEVINFO_DATA DevInfoData);

LONG
RedbookpUpperFilterRegInstall(IN HDEVINFO HDevInfo, IN PSP_DEVINFO_DATA DevInfoData);

BOOLEAN
UtilpIsSingleSzOfMultiSzInMultiSz(
    IN LPTSTR FindOneOfThese,
    IN LPTSTR WithinThese
    );
DWORD
UtilpMultiSzSearchAndDeleteCaseInsensitive(
    LPTSTR  FindThis,
    LPTSTR  FindWithin,
    DWORD  *NewStringLength
    );

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// the actual callbacks should do very little, codewise
//
////////////////////////////////////////////////////////////////////////////////
DWORD
CdromCddaInfo(
    IN     HDEVINFO HDevInfo,
    IN     PSP_DEVINFO_DATA DevInfoData,
       OUT PREDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO CddaInfo,
    IN OUT PULONG BufferSize
    )
/*++

Routine Description:

    Returns whether the drive is a 'known good' drive.
    Returns whether the drive supports CDDA at all.
    Returns whether the drive supports accurate CDDA for only some read sizes.
    ...

Arguments:

    CDDAInfo must point to a pre-allocated buffer for this info
    BufferSize will give size of this buffer, to allow for more fields
    to be added later in a safe manner.

Return Value:

    will return ERROR_SUCCESS/STATUS_SUCCESS (both zero)

Notes:

    If cannot open these registry keys, will default to FALSE,
    since the caller will most likely not have the ability to enable
    redbook anyways.

--*/
{
    HKEY enumHandle = INVALID_HANDLE_VALUE;
    HKEY subkeyHandle = INVALID_HANDLE_VALUE;
    REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO info;
    ULONG i;
    DWORD dataType;
    DWORD dataSize;
    LONG error;

    error = ERROR_SUCCESS;

    if ((*BufferSize == 0)  ||  (CddaInfo == NULL)) {

        *BufferSize = sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO);
        return ERROR_INSUFFICIENT_BUFFER;

    }


    RtlZeroMemory(CddaInfo, *BufferSize);
    RtlZeroMemory(&info, sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO));

    info.Version = REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO_VERSION;

    TRY {

        enumHandle = SetupDiOpenDevRegKey(HDevInfo,
                                          DevInfoData,
                                          DICS_FLAG_GLOBAL,
                                          0,
                                          DIREG_DEV,
                                          KEY_READ);

        if (enumHandle == INVALID_HANDLE_VALUE) {
            DebugPrint((1, "StorProp.CddaInfo => unable to open dev key\n"));
            error = ERROR_OUT_OF_PAPER;
            LEAVE;
        }

        error = RegOpenKey(enumHandle, TEXT("DigitalAudio"), &subkeyHandle);
        if (error != ERROR_SUCCESS) {
            DebugPrint((1, "StorProp.CddaInfo => unable to open subkey\n"));
            LEAVE;
        }

        for (i=0; i<3; i++) {

            PBYTE buffer;
            TCHAR * keyName;

            if (i == 0) {
                keyName = TEXT("CDDAAccurate");
                buffer = (PBYTE)(&info.Accurate);
            } else if (i == 1) {
                keyName = TEXT("CDDASupported");
                buffer = (PBYTE)(&info.Supported);
            } else if (i == 2) {
                keyName = TEXT("ReadSizesSupported");
                buffer = (PBYTE)(&info.AccurateMask0);

#if DBG
            } else {
                DebugPrint((0, "StorProp.CddaInfo => Looping w/o handling\n"));
                DebugBreak();
#endif

            }


            dataSize = sizeof(DWORD);
            error = RegQueryValueEx(subkeyHandle,
                                    keyName,
                                    NULL,
                                    &dataType,
                                    buffer,
                                    &dataSize);

            if (error != ERROR_SUCCESS) {
                DebugPrint((1, "StorProp.CddaInfo => unable to query %ws %x\n",
                            keyName, error));
                LEAVE;
            }
            if (dataType != REG_DWORD) {
                DebugPrint((1, "StorProp.CddaInfo => %ws wrong data type (%x)\n",
                            keyName, dataType));
                error = ERROR_INVALID_DATA;
                LEAVE;
            }

            DebugPrint((1, "StorProp.CddaInfo => %ws == %x\n",
                        keyName, *buffer));

        }

    } FINALLY {

        if (subkeyHandle != INVALID_HANDLE_VALUE) {
            RegCloseKey(subkeyHandle);
        }
        if (enumHandle != INVALID_HANDLE_VALUE) {
            RegCloseKey(enumHandle);
        }

        if (error == ERROR_SUCCESS) {

            //
            // everything succeeded -- copy only the amount they requested
            // and don't care about it being aligned on any particular buffer size.
            // this is the only other place the user buffer should be modified
            //
            if (*BufferSize > sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO)) {
                *BufferSize = sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO);
            }

            DebugPrint((2, "StorProp.CddaInfo => everything worked\n"));
            RtlCopyMemory(CddaInfo, &info, *BufferSize);

        } else {

            DebugPrint((2, "StorProp.CddaInfo => something failed\n"));
            *BufferSize = 0;

        }

    }

    return error;
}


BOOL
CdromKnownGoodDigitalPlayback(
    IN HDEVINFO HDevInfo,
    IN PSP_DEVINFO_DATA DevInfoData
    )
/*++

Routine Description:

    Returns whether this drive is a 'known good' drive.

Arguments:

Return Value:

Notes:

    default to FALSE, since if fails, caller probably does not
    have ability to enable redbook anyways.
    this routine is outdated -- callers should call CdromCddaInfo()
    directly for more exact information.

--*/
{
    REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO CddaInfo;
    ULONG bufferSize;
    DWORD error;

    bufferSize = sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO);

#if DBG
    DbgPrint("\n\nOutdated call to CdromKnownGoodDigitalPlayback(), "
             "should be calling CdromCddaInfo()\n\n");
#endif // DBG

    error = CdromCddaInfo(HDevInfo, DevInfoData, &CddaInfo, &bufferSize);

    if (error != ERROR_SUCCESS) {
        return FALSE;
    }

    if (bufferSize <= sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO)) {
        return FALSE;
    }

    if (CddaInfo.Accurate) {
        return TRUE;
    }

    if (CddaInfo.Supported && CddaInfo.AccurateMask0) {
        return TRUE;
    }

    return FALSE;

}


LONG
CdromEnableDigitalPlayback(
    IN HDEVINFO HDevInfo,
    IN PSP_DEVINFO_DATA DevInfoData,
    IN BOOLEAN ForceUnknown
)
/*++

Routine Description:

    Enables redbook
        1) add redbook to filter list (if not there)
        2) if not on stack (via test of guid) re-start stack
        3) if still not on stack, error
        4) set wmi guid item enabled

Arguments:

    DevInfo      - the device to enable it on
    DevInfoData  -
    ForceUnknown - will set a popup if not a known good drive and this is false

Return Value:

    ERROR_XXX value

--*/
{
    LONG status;
    SP_DEVINSTALL_PARAMS devInstallParameters;
    REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO digitalInfo;
    ULONG digitalInfoSize;
    BOOLEAN enableIt;

    //
    // restrict to administrator ???
    //

    if (!IsUserAdmin()) {
        DebugPrint((1, "StorProp.Enable => you need to be administrator to "
                    "enable redbook\n"));
        return ERROR_ACCESS_DENIED;
    }

    digitalInfoSize = sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO);
    RtlZeroMemory(&digitalInfo, digitalInfoSize);

    status = CdromCddaInfo(HDevInfo, DevInfoData,
                           &digitalInfo, &digitalInfoSize);

    if (status != ERROR_SUCCESS) {

        DebugPrint((1, "StorProp.Enable => not success getting info %x\n",
                    status));

        //
        // fake some info
        //

        digitalInfo.Version = REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO_VERSION;
        digitalInfo.Accurate = 0;
        digitalInfo.Supported = 1;
        digitalInfo.AccurateMask0 = -1;
        digitalInfoSize = sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO);

    }

    if (digitalInfoSize < sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO)) {
        DebugPrint((3, "StorProp.Enable => returned %x bytes? (not %x)\n",
                    digitalInfoSize,
                    sizeof(REDBOOK_DIGITAL_AUDIO_EXTRACTION_INFO)
                    ));
        return ERROR_ACCESS_DENIED;
    }

    if (!digitalInfo.Supported) {
        DebugPrint((1, "StorProp.Enable => This drive will never "
                    "support redbook\n"));
    //    return ERROR_INVALID_FUNCTION; // log an error here?
    }

    //
    // if it's not accurate AND we don't have the compensating info AND
    // they didn't force it to install, then popup a dialog.
    //

    if (!(digitalInfo.Accurate) &&
        !(digitalInfo.AccurateMask0) &&
        !(ForceUnknown)
        ) {

        BOOLEAN okToProceed = FALSE;
        TCHAR buffer[MAX_PATH+1];
        TCHAR bufferTitle[MAX_PATH+1];

        buffer[0] = '\0';
        bufferTitle[0] = '\0';
        buffer[MAX_PATH] = '\0';
        bufferTitle[MAX_PATH] = '\0';

        //
        // not forced, and not known good.  pop up a box asking permission
        //
        LoadString(ModuleInstance,
                   REDBOOK_UNKNOWN_DRIVE_CONFIRM,
                   buffer,
                   MAX_PATH);
        LoadString(ModuleInstance,
                   REDBOOK_UNKNOWN_DRIVE_CONFIRM_TITLE,
                   bufferTitle,
                   MAX_PATH);
        if (MessageBox(GetDesktopWindow(),
                       buffer,
                       bufferTitle,
                       MB_YESNO          |  // ok and cancel buttons
                       MB_ICONQUESTION   |  // question icon
                       MB_DEFBUTTON2     |  // cancel is default
                       MB_SYSTEMMODAL       // must respond to this box
                       ) == IDYES) {
            okToProceed = TRUE;
        }

        if (!okToProceed) {
            DebugPrint((3, "StorProp.Enable => User did not force installation "
                        "on unknown drive\n"));
            return ERROR_REDBOOK_FILTER;
        }
    }

    //
    // ensure it is in the filter list
    //

    RedbookpUpperFilterRegInstall(HDevInfo, DevInfoData);

    //
    // restart the device to load redbook
    //

    if (!UtilpRestartDevice(HDevInfo, DevInfoData)) {

        DebugPrint((1, "StorProp.Enable => Restart failed\n"));

    } else {

        DebugPrint((1, "StorProp.Enable => Restart succeeded\n"));

    }
    return ERROR_SUCCESS;

}


LONG
CdromDisableDigitalPlayback(IN HDEVINFO HDevInfo, IN PSP_DEVINFO_DATA DevInfoData)
{
    DWORD status = ERROR_SUCCESS;

    //
    // This API is restrict to admins only
    //

    if (!IsUserAdmin())
    {
        DebugPrint((1, "StorProp.Disable => User is not administrator\n"));
        return ERROR_ACCESS_DENIED;
    }

    //
    // Delete redbook from the upper filters list regardless
    //

    status = RedbookpUpperFilterRegDelete(HDevInfo, DevInfoData);

    if (status == ERROR_SUCCESS)
    {
        //
        // Restart the device to remove redbook from the stack
        //

        UtilpRestartDevice(HDevInfo, DevInfoData);
    }

    return status;
}


LONG
CdromIsDigitalPlaybackEnabled(IN HDEVINFO HDevInfo, IN PSP_DEVINFO_DATA DevInfoData, OUT PBOOLEAN Enabled)
{
    DWORD status = ERROR_SUCCESS;
    DWORD dwSize = 0;

    *Enabled = FALSE;

    status = SetupDiGetDeviceRegistryProperty(HDevInfo,
                                              DevInfoData,
                                              SPDRP_UPPERFILTERS,
                                              NULL,
                                              NULL,
                                              0,
                                              &dwSize) ? ERROR_SUCCESS : GetLastError();
    if (status == ERROR_INSUFFICIENT_BUFFER)
    {
        TCHAR* szBuffer = LocalAlloc(LPTR, dwSize);

        if (szBuffer)
        {
            if (SetupDiGetDeviceRegistryProperty(HDevInfo,
                                                 DevInfoData,
                                                 SPDRP_UPPERFILTERS,
                                                 NULL,
                                                 (PBYTE)szBuffer,
                                                 dwSize,
                                                 NULL))
            {
                if (UtilpIsSingleSzOfMultiSzInMultiSz(_T("redbook\0"), szBuffer))
                {
                    //
                    // Digital playback is indeed enabled
                    //

                    *Enabled = TRUE;
                }

                status = ERROR_SUCCESS;
            }
            else
            {
                status = GetLastError();
            }

            LocalFree(szBuffer);
        }
        else
        {
            status = ERROR_NOT_ENOUGH_MEMORY;
        }
    }
    else if (status == ERROR_INVALID_DATA)
    {
        //
        // There probably isn't any upper filter installed
        //

        status = ERROR_SUCCESS;
    }

    return status;
}


////////////////////////////////////////////////////////////////////////////////
//
// The support routines do all the work....
//
////////////////////////////////////////////////////////////////////////////////


HANDLE
UtilpGetDeviceHandle(
    HDEVINFO DevInfo,
    PSP_DEVINFO_DATA DevInfoData,
    LPGUID ClassGuid,
    DWORD DesiredAccess
    )
/*++

Routine Description:

    gets a handle for a device

Arguments:

    the name of the device to open

Return Value:

    handle to the device opened, which must be later closed by the caller.

Notes:

    this function is also in the class installer (syssetup.dll)
    so please propogate fixes there as well

--*/
{
    BOOL status;
    ULONG i;
    HANDLE fileHandle = INVALID_HANDLE_VALUE;


    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;

    HDEVINFO devInfoWithInterface = NULL;
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = NULL;
    PTSTR deviceInstanceId = NULL;

    ULONG deviceInterfaceDetailDataSize;
    ULONG deviceInstanceIdSize;



    TRY {

        //
        // get the ID for this device
        //

        for (i=deviceInstanceIdSize=0; i<2; i++) {

            if (deviceInstanceIdSize != 0) {

                //
                // deviceInstanceIdSize is returned in CHARACTERS
                // by SetupDiGetDeviceInstanceId(), so must allocate
                // returned size * sizeof(TCHAR)
                //

                deviceInstanceId =
                    LocalAlloc(LPTR, deviceInstanceIdSize * sizeof(TCHAR));

                if (deviceInstanceId == NULL) {
                    DebugPrint((1, "StorProp.GetDeviceHandle => Unable to "
                                "allocate for deviceInstanceId\n"));
                    LEAVE;
                }


            }

            status = SetupDiGetDeviceInstanceId(DevInfo,
                                                DevInfoData,
                                                deviceInstanceId,
                                                deviceInstanceIdSize,
                                                &deviceInstanceIdSize
                                                );
        }

        if (!status) {
            DebugPrint((1, "StorProp.GetDeviceHandle => Unable to get "
                        "Device IDs\n"));
            LEAVE;
        }

        //
        // Get all the cdroms in the system
        //

        devInfoWithInterface = SetupDiGetClassDevs(ClassGuid,
                                                   deviceInstanceId,
                                                   NULL,
                                                   DIGCF_DEVICEINTERFACE
                                                   );

        if (devInfoWithInterface == NULL) {
            DebugPrint((1, "StorProp.GetDeviceHandle => Unable to get "
                        "list of CdRom's in system\n"));
            LEAVE;
        }


        memset(&deviceInterfaceData, 0, sizeof(SP_DEVICE_INTERFACE_DATA));
        deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        status = SetupDiEnumDeviceInterfaces(devInfoWithInterface,
                                             NULL,
                                             ClassGuid,
                                             0,
                                             &deviceInterfaceData
                                             );

        if (!status) {
            DebugPrint((1, "StorProp.GetDeviceHandle => Unable to get "
                        "SP_DEVICE_INTERFACE_DATA\n"));
            LEAVE;
        }


        for (i=deviceInterfaceDetailDataSize=0; i<2; i++) {

            if (deviceInterfaceDetailDataSize != 0) {

                //
                // deviceInterfaceDetailDataSize is returned in BYTES
                // by SetupDiGetDeviceInstanceId(), so must allocate
                // returned size only
                //

                deviceInterfaceDetailData =
                    LocalAlloc (LPTR, deviceInterfaceDetailDataSize);

                if (deviceInterfaceDetailData == NULL) {
                    DebugPrint((1, "StorProp.GetDeviceHandle => Unable to "
                                "allocate for deviceInterfaceDetailData\n"));
                    LEAVE;
                }

                deviceInterfaceDetailData->cbSize =
                    sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            }

            status = SetupDiGetDeviceInterfaceDetail(devInfoWithInterface,
                                                     &deviceInterfaceData,
                                                     deviceInterfaceDetailData,
                                                     deviceInterfaceDetailDataSize,
                                                     &deviceInterfaceDetailDataSize,
                                                     NULL);
        }

        if (!status) {
            DebugPrint((1, "StorProp.GetDeviceHandle => Unable to get "
                        "DeviceInterfaceDetail\n"));
            LEAVE;
        }

        if (deviceInterfaceDetailDataSize <=
            FIELD_OFFSET(SP_DEVICE_INTERFACE_DETAIL_DATA, DevicePath)) {
            DebugPrint((1, "StorProp.GetDeviceHandle => No device path\n"));
            status = ERROR_PATH_NOT_FOUND;
            LEAVE;
        }

        //
        // no need to memcpy it, just use the path returned to us.
        //

        fileHandle = CreateFile(deviceInterfaceDetailData->DevicePath,
                                DesiredAccess,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                0,
                                NULL);

        if (fileHandle == INVALID_HANDLE_VALUE) {
            DebugPrint((1, "StorProp.GetDeviceHandle => Final CreateFile() "
                        "failed\n"));
            LEAVE;
        }

        DebugPrint((3, "StorProp.GetDeviceHandle => handle %x opened\n",
                    fileHandle));


    } FINALLY {

        if (devInfoWithInterface != NULL) {
            SetupDiDestroyDeviceInfoList(devInfoWithInterface);
        }

        if (deviceInterfaceDetailData != NULL) {
            LocalFree (deviceInterfaceDetailData);
        }

    }

    return fileHandle;
}


BOOLEAN
UtilpRestartDevice(
    IN HDEVINFO HDevInfo,
    IN PSP_DEVINFO_DATA DevInfoData
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    SP_PROPCHANGE_PARAMS parameters;
    SP_DEVINSTALL_PARAMS installParameters;
    BOOLEAN succeeded = FALSE;

    RtlZeroMemory(&parameters,        sizeof(SP_PROPCHANGE_PARAMS));
    RtlZeroMemory(&installParameters, sizeof(SP_DEVINSTALL_PARAMS));

    //
    // Initialize the SP_CLASSINSTALL_HEADER struct at the beginning of the
    // SP_PROPCHANGE_PARAMS struct.  this allows SetupDiSetClassInstallParams
    // to work.
    //

    parameters.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    parameters.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;

    //
    // Initialize SP_PROPCHANGE_PARAMS such that the device will be STOPPED
    //

    parameters.Scope       = DICS_FLAG_CONFIGSPECIFIC;
    parameters.HwProfile   = 0; // current profile

    //
    // prepare for the call to SetupDiCallClassInstaller (to stop the device)
    //

    parameters.StateChange = DICS_STOP;

    if (!SetupDiSetClassInstallParams(HDevInfo,
                                      DevInfoData,
                                      (PSP_CLASSINSTALL_HEADER)&parameters,
                                      sizeof(SP_PROPCHANGE_PARAMS))) {
        DebugPrint((1, "UtilpRestartDevice => Couldn't stop the device (%x)\n",
                    GetLastError()));
        goto FinishRestart;
    }

    //
    // actually stop the device
    //

    if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,
                                   HDevInfo,
                                   DevInfoData)) {
        DebugPrint((1, "UtilpRestartDevice => call to class installer "
                    "(STOP) failed (%x)\n", GetLastError()));
        goto FinishRestart;
    }



    //
    // prepare for the call to SetupDiCallClassInstaller (to start the device)
    //

    parameters.StateChange = DICS_START;


    if (!SetupDiSetClassInstallParams(HDevInfo,
                                      DevInfoData,
                                      (PSP_CLASSINSTALL_HEADER)&parameters,
                                      sizeof(SP_PROPCHANGE_PARAMS))) {
        DebugPrint((1, "UtilpRestartDevice => Couldn't stop the device (%x)\n",
                    GetLastError()));
        goto FinishRestart;
    }

    //
    // actually start the device
    //

    if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,
                                   HDevInfo,
                                   DevInfoData)) {
        DebugPrint((1, "UtilpRestartDevice => call to class installer "
                    "(STOP) failed (%x)\n", GetLastError()));
        goto FinishRestart;
    }

    succeeded = TRUE;

FinishRestart:

    //
    // this call will succeed, but we should still check the status
    //

    if (!SetupDiGetDeviceInstallParams(HDevInfo,
                                       DevInfoData,
                                       &installParameters)) {
        DebugPrint((1, "UtilpRestartDevice => Couldn't get the device install "
                    "paramters (%x)\n", GetLastError()));
        return FALSE;
    }

    if (TEST_FLAG(installParameters.Flags, DI_NEEDREBOOT)) {
        DebugPrint((1, "UtilpRestartDevice => Device needs a reboot.\n"));
        return FALSE;
    }
    if (TEST_FLAG(installParameters.Flags, DI_NEEDRESTART)) {
        DebugPrint((1, "UtilpRestartDevice => Device needs a restart(!).\n"));
        return FALSE;
    }

    if (succeeded) {
        DebugPrint((1, "UtilpRestartDevice => Device successfully stopped and "
                    "restarted.\n"));
        return TRUE;
    }

    SET_FLAG(installParameters.Flags, DI_NEEDRESTART);

    DebugPrint((1, "UtilpRestartDevice => Device needs to be restarted.\n"));
    SetupDiSetDeviceInstallParams(HDevInfo, DevInfoData, &installParameters);

    return FALSE;


}


LONG
RedbookpUpperFilterRegDelete(IN HDEVINFO HDevInfo, IN PSP_DEVINFO_DATA DevInfoData)
{
    DWORD status = ERROR_SUCCESS;
    DWORD dwSize = 0;

    status = SetupDiGetDeviceRegistryProperty(HDevInfo,
                                              DevInfoData,
                                              SPDRP_UPPERFILTERS,
                                              NULL,
                                              NULL,
                                              0,
                                              &dwSize) ? ERROR_SUCCESS : GetLastError();
    if (status == ERROR_INSUFFICIENT_BUFFER)
    {
        TCHAR* szBuffer = LocalAlloc(LPTR, dwSize);

        if (szBuffer)
        {
            if (SetupDiGetDeviceRegistryProperty(HDevInfo,
                                                 DevInfoData,
                                                 SPDRP_UPPERFILTERS,
                                                 NULL,
                                                 (PBYTE)szBuffer,
                                                 dwSize,
                                                 NULL))
            {
                if (UtilpMultiSzSearchAndDeleteCaseInsensitive(_T("redbook"), szBuffer, &dwSize))
                {
                    status = SetupDiSetDeviceRegistryProperty(HDevInfo,
                                                              DevInfoData,
                                                              SPDRP_UPPERFILTERS,
                                                              (dwSize == 0) ? NULL : (PBYTE)szBuffer,
                                                              dwSize) ? ERROR_SUCCESS : GetLastError();
                }
                else
                {
                    //
                    // Redbook isn't loaded for this device
                    //

                    status = ERROR_SUCCESS;
                }
            }
            else
            {
                status = GetLastError();
            }

            LocalFree(szBuffer);
        }
        else
        {
            status = ERROR_NOT_ENOUGH_MEMORY;
        }
    }
    else if (status == ERROR_INVALID_DATA)
    {
        //
        // There probably isn't any upper filter installed
        //

        status = ERROR_SUCCESS;
    }

    return status;
}


LONG
RedbookpUpperFilterRegInstall(IN HDEVINFO HDevInfo, IN PSP_DEVINFO_DATA DevInfoData)
{
    DWORD status = ERROR_SUCCESS;
    DWORD dwSize = 0;

    status = SetupDiGetDeviceRegistryProperty(HDevInfo,
                                              DevInfoData,
                                              SPDRP_UPPERFILTERS,
                                              NULL,
                                              NULL,
                                              0,
                                              &dwSize) ? ERROR_SUCCESS : GetLastError();
    if (status == ERROR_INSUFFICIENT_BUFFER)
    {
        TCHAR* szBuffer = LocalAlloc(LPTR, dwSize);

        if (szBuffer)
        {
            if (SetupDiGetDeviceRegistryProperty(HDevInfo,
                                                 DevInfoData,
                                                 SPDRP_UPPERFILTERS,
                                                 NULL,
                                                 (PBYTE)szBuffer,
                                                 dwSize,
                                                 NULL))
            {
                if (!UtilpIsSingleSzOfMultiSzInMultiSz(_T("redbook\0"), szBuffer))
                {
                    //
                    // Add Redbook to the beginning of the list
                    //

                    DWORD  dwNewSize   = dwSize + sizeof(_T("redbook"));
                    TCHAR* szNewBuffer = LocalAlloc(LPTR, dwNewSize);

                    if (szNewBuffer)
                    {
                        _tcscpy(szNewBuffer, _T("redbook"));

                        RtlCopyMemory(szNewBuffer + _tcslen(_T("redbook")) + 1, szBuffer, dwSize);

                        status = SetupDiSetDeviceRegistryProperty(HDevInfo,
                                                                  DevInfoData,
                                                                  SPDRP_UPPERFILTERS,
                                                                  (PBYTE)szNewBuffer,
                                                                  dwNewSize) ? ERROR_SUCCESS : GetLastError();

                        LocalFree(szNewBuffer);
                    }
                    else
                    {
                        status = ERROR_NOT_ENOUGH_MEMORY;
                    }
                }
                else
                {
                    //
                    // Redbook is already loaded for this device
                    //

                    status = ERROR_SUCCESS;
                }
            }
            else
            {
                status = GetLastError();
            }

            LocalFree(szBuffer);
        }
        else
        {
            status = ERROR_NOT_ENOUGH_MEMORY;
        }
    }
    else if (status == ERROR_INVALID_DATA)
    {
        //
        // There probably isn't any upper filter installed
        //

        TCHAR szBuffer[] = _T("redbook\0");

        dwSize = sizeof(szBuffer);

        status = SetupDiSetDeviceRegistryProperty(HDevInfo,
                                                  DevInfoData,
                                                  SPDRP_UPPERFILTERS,
                                                  (PBYTE)szBuffer,
                                                  dwSize) ? ERROR_SUCCESS : GetLastError();
    }

    return status;
}


BOOLEAN
UtilpIsSingleSzOfMultiSzInMultiSz(
    IN LPTSTR FindOneOfThese,
    IN LPTSTR WithinThese
    )
/*++

Routine Description:

    Deletes all instances of a string from within a multi-sz.
    automagically operates on either unicode or ansi or ??

Arguments:

    FindOneOfThese - multisz to search with
    WithinThese    - multisz to search in

Return Value:

    1/20 of one cent, or the number of strings deleted, rounded down.

Notes:

    expect small inputs, so n*m is acceptable run time.

--*/
{
    LPTSTR searchFor;
    LPTSTR within;


    //
    // loop through all strings in FindOneOfThese
    //

    searchFor = FindOneOfThese;
    while ( _tcscmp(searchFor, TEXT("\0")) ) {

        //
        // loop through all strings in WithinThese
        //

        within = WithinThese;
        while ( _tcscmp(within, TEXT("\0"))) {

            //
            // if the are equal, return TRUE
            //

            if ( !_tcscmp(searchFor, within) ) {
                return TRUE;
            }

            within += _tcslen(within) + 1;
        } // end of WithinThese loop

        searchFor += _tcslen(searchFor) + 1;
    } // end of FindOneOfThese loop

    return FALSE;
}


DWORD
UtilpMultiSzSearchAndDeleteCaseInsensitive(
    LPTSTR  FindThis,
    LPTSTR  FindWithin,
    DWORD  *NewStringLength
    )
/*++

Routine Description:

    Deletes all instances of a string from within a multi-sz.
    automagically operates on either unicode or ansi or ??

Arguments:

    NewStringLength is in BYTES, not number of chars

Return Value:

    1/20 of one cent, or the number of strings deleted, rounded down.

--*/
{
    LPTSTR search;
    DWORD  charOffset;
    DWORD  instancesDeleted;

    if ((*NewStringLength) % sizeof(TCHAR)) {
        assert(!"String must be in bytes, does not divide by sizeof(TCHAR)\n");
        return 0;
    }

    if ((*NewStringLength) < sizeof(TCHAR)*2) {
        assert(!"String must be multi-sz, which requires at least two chars\n");
        return 0;
    }

    charOffset = 0;
    instancesDeleted = 0;
    search = FindWithin;

    //
    // loop while there string length is not zero
    // couldn't find a TNULL, or i'd just compare.
    //

    while (_tcsicmp(search, TEXT("\0")) != 0) {

        //
        // if this string matches...
        //

        if (_tcsicmp(search, FindThis) == 0) {

            //
            // the new length is smaller
            // remove the string (and terminating null)
            //

            instancesDeleted++;
            *NewStringLength -= (_tcslen(search) + 1) * sizeof(TCHAR);

            RtlMoveMemory(search,
                          search + _tcslen(search) + 1,
                          *NewStringLength - (charOffset * sizeof(TCHAR))
                          );

        } else {

            //
            // move current search pointer
            // increment current offset (in CHARS)
            //

            charOffset += _tcslen(search) + 1;
            search     += _tcslen(search) + 1;

        }

        //
        // it's that simple
        //
    }

    //
    // if deleted all strings, set to double-null
    //

    if (*NewStringLength == sizeof(TCHAR)) {
        FindWithin = TEXT("\0");
        *NewStringLength = 0;
    }

    return instancesDeleted;
}
