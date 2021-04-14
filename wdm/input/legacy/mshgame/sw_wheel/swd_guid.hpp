/****************************************************************************

    MODULE:     	SWD_GUID.HPP
	Tab Settings:	5 9
	Copyright 1995, 1996, Microsoft Corporation, 	All Rights Reserved.

    PURPOSE:            CLSIDs and IIDs defined for DirectInputForce

    FUNCTIONS:

	Author(s):	Name:
	----------	----------------
		MEA		Manolito E. Adan
		MLD		Michael L. Day

	Revision History:
	-----------------
	Version 	Date        Author  Comments
	-------     ------  	-----   -------------------------------------------
   	1.0    		06-Feb-97   MEA     original, Based on SWForce
				23-Feb-97	MEA		Modified for DirectInput FF Device Driver
	2.0			29-Jun-98	MLD		Added Saitek Modification

****************************************************************************/
#ifndef _SWD_GUID_SEEN
#define _SWD_GUID_SEEN

#ifdef INITGUIDS
#include <initguid.h>
#endif //INITGUIDS


/*
 * GUIDs
 *
 */

#ifdef SAITEK

//
// --- Saitek Force Feedback Device Driver Interface
//
DEFINE_GUID(CLSID_DirectInputEffectDriver, /* {0A98BE81-0F6D-11d2-BCE0-0000F8757F9F} */
	0xa98be81,
	0xf6d,
	0x11d2,
	0xbc, 0xe0, 0x0, 0x0, 0xf8, 0x75, 0x7f, 0x9f
);


// For use in creating registry
#define CLSID_DirectInputEffectDriver_String TEXT("{0A98BE81-0F6D-11d2-BCE0-0000F8757F9F}")
#define DRIVER_OBJECT_NAME TEXT("Saitek Force Feedback Effect Driver Object")
#define PROGID_NAME TEXT("Saitek Force Feedback Effect Driver 2.0")
#define PROGID_NOVERSION_NAME TEXT("Saitek Force Feedback Effect Driver")
#define THREADING_MODEL_STRING TEXT("Both")

#else // #ifdef SAITEK

//
// --- SideWinder Force Feedback Device Driver Interface
//
// --- Old one
//DEFINE_GUID(CLSID_DirectInputEffectDriver, /* e84cd1b1-81fa-11d0-94ab-0080c74c7e95 */
DEFINE_GUID(CLSID_DirectInputEffectDriver, /* 0d33e080-da1f-11d1-9483-00c04fc2aa8f */
    0x0d33e080,
    0xda1f,
    0x11d1,
    0x94, 0x83, 0x00, 0xc0, 0x4f, 0xc2, 0xaa, 0x8f
);


// For use in creating registry
#define CLSID_DirectInputEffectDriver_String TEXT("{0d33e080-da1f-11d1-9483-00c04fc2aa8f}")
#define DRIVER_OBJECT_NAME TEXT("Microsoft SideWinder Force Feedback Effect Driver Object")
#define PROGID_NAME TEXT("Microsoft SideWinder Force Feedback Effect Driver 2.0")
#define PROGID_NOVERSION_NAME TEXT("Microsoft SideWinder Force Feedback Effect Driver")
#define THREADING_MODEL_STRING TEXT("Both")

#endif // #ifdef SAITEK

//
// --- Effect GUIDs
//
DEFINE_GUID(GUID_Wall, /* e84cd1a1-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a1,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_ProcessList, /* e84cd1a2-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a2,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

// Built in ROM Effects
DEFINE_GUID(GUID_RandomNoise, /* e84cd1a3-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a3,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_AircraftCarrierTakeOff, /* e84cd1a4-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a4,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_BasketballDribble, /* e84cd1a5-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a5,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_CarEngineIdle, /* e84cd1a6-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a6,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_ChainsawIdle, /* e84cd1a7-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a7,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_ChainsawInAction, /* e84cd1a8-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a8,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_DieselEngineIdle, /* e84cd1a9-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1a9,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_Jump, /* e84cd1aa-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1aa,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_Land, /* e84cd1ab-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1ab,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_MachineGun, /* e84cd1ac-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1ac,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_Punched, /* e84cd1ad-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1ad,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_RocketLaunch, /* e84cd1ae-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1ae,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_SecretDoor, /* e84cd1af-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1af,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );
DEFINE_GUID(GUID_SwitchClick, /* e84cd1b0-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b0,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_WindGust, /* e84cd1b1-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b1,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_WindShear, /* e84cd1b2-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b2,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Pistol, /* e84cd1b3-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b3,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Shotgun, /* e84cd1b4-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b4,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Laser1, /* e84cd1b5-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b5,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Laser2, /* e84cd1b6-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b6,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Laser3, /* e84cd1b7-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b7,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Laser4, /* e84cd1b8-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b8,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Laser5, /* e84cd1b9-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1b9,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Laser6, /* e84cd1ba-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1ba,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_OutOfAmmo, /* e84cd1bb-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1bb,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_LightningGun, /* e84cd1bc-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1bc,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Missile, /* e84cd1bd-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1bd,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_GatlingGun, /* e84cd1be-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1be,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_ShortPlasma, /* e84cd1bf-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1bf,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_PlasmaCannon1, /* e84cd1c0-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1c0,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_PlasmaCannon2, /* e84cd1c1-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1c1,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_Cannon, /* e84cd1c2-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1c2,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_RawForce, /* e84cd1c6-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1c6,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_VFXEffect, /* e84cd1c7-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1c7,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

DEFINE_GUID(GUID_RTCSpring, /* e84cd1c8-81fa-11d0-94ab-0080c74c7e95 */
    0xe84cd1c8,
    0x81fa,
    0x11d0,
    0x94, 0xab, 0x00, 0x80, 0xc7, 0x4c, 0x7e, 0x95
  );

//
// --- UNUSED but reserved for future GUIDs
//


#endif //_SWD_GUID_SEEN

