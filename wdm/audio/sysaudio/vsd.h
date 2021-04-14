//---------------------------------------------------------------------------
//
//  Module:   		vsd.h
//
//  Description:	Virtual Source Data Class
//
//
//@@BEGIN_MSINTERNAL
//  Development Team:
//     Mike McLaughlin
//
//  History:   Date	  Author      Comment
//
//@@END_MSINTERNAL
//---------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1996-1999 Microsoft Corporation.  All Rights Reserved.
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Constants and Macros
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Classes
//---------------------------------------------------------------------------

typedef class CVirtualSourceData : public CObj
{
public:
    CVirtualSourceData(
	PDEVICE_NODE pDeviceNode
    );

    PTOPOLOGY_NODE pTopologyNode;
    LONG MinimumValue;				// range to convert from
    LONG MaximumValue;				//
    LONG Steps;					//
    LONG cChannels;
    BOOL fMuted[MAX_NUM_CHANNELS];		// muted if TRUE
    LONG lLevel[MAX_NUM_CHANNELS];
    LIST_VIRTUAL_NODE_DATA lstVirtualNodeData;
    DefineSignature(0x20445356);		// VSD

} VIRTUAL_SOURCE_DATA, *PVIRTUAL_SOURCE_DATA;

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Local prototypes
//---------------------------------------------------------------------------
