//---------------------------------------------------------------------------
//
//  Module:   		vnd.h
//
//  Description:	Virtual Node Data Class
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

#define	MAX_NUM_CHANNELS	24

//---------------------------------------------------------------------------
// Classes
//---------------------------------------------------------------------------

typedef class CVirtualNodeData : public CListDoubleItem
{
public:
    CVirtualNodeData(
	PSTART_NODE_INSTANCE pStartNodeInstance,
	PVIRTUAL_SOURCE_DATA pVirtualSourceData
    );
    ~CVirtualNodeData();
    ENUMFUNC Destroy()
    {
	Assert(this);
	delete this;
	return(STATUS_CONTINUE);
    };
    PVIRTUAL_SOURCE_DATA pVirtualSourceData;
    PSTART_NODE_INSTANCE pStartNodeInstance;
    PFILE_OBJECT pFileObject;
    ULONG NodeId;
    LONG MinimumValue;				// Range to convert to
    LONG MaximumValue;				//
    LONG Steps;					//
    LONG lLevel[MAX_NUM_CHANNELS];		// Local volume
    DefineSignature(0x20444e56);		// VND

} VIRTUAL_NODE_DATA, *PVIRTUAL_NODE_DATA;

//---------------------------------------------------------------------------

typedef ListDoubleDestroy<VIRTUAL_NODE_DATA> LIST_VIRTUAL_NODE_DATA;

//---------------------------------------------------------------------------

