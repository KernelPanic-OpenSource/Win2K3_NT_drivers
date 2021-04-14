/*++

Copyright (c) 1999  Microsoft Corporation

Abstract:

    @doc
    @module Snapshot.hxx | Declarations used by the Software Snapshot interface
    @end

Author:

    Adi Oltean  [aoltean]   01/24/2000

Revision History:

    Name        Date        Comments

    aoltean     01/24/2000  Created.

--*/


#ifndef __VSSW_DIFF_HXX__
#define __VSSW_DIFF_HXX__

#if _MSC_VER > 1000
#pragma once
#endif


/////////////////////////////////////////////////////////////////////////////
// Classes

//
//  @class CVsDiffArea | IVsDiffArea implementation
//
//  @index method | CVsDiffArea
//  @doc CVsDiffArea
//
class ATL_NO_VTABLE CVsDiffArea
{
// Constructors/ destructors
public:
	CVsDiffArea();
	~CVsDiffArea();

//  Operations
public:
    HRESULT Initialize(
	    IN      LPCWSTR pwszVolumeMountPoint	// DO NOT transfer ownership
        );

//  Ovverides
public:

	//
	//	interface IVsDiffArea
	//

    STDMETHOD(AddVolume)(                      			
        IN      VSS_PWSZ pwszVolumeMountPoint						
        );												
        
    STDMETHOD(Query)(									
        OUT     IVssEnumObject **ppEnum					
        );												

    STDMETHOD(Clear)(                      				
        );												

    STDMETHOD(GetUsedVolumeSpace)(                      
        OUT      LONGLONG* pllBytes						
        );												
        
    STDMETHOD(GetAllocatedVolumeSpace)(               	
        OUT      LONGLONG* pllBytes						
        );												
        
    STDMETHOD(GetMaximumVolumeSpace)(              		
        OUT      LONGLONG* pllBytes						
        );												
        
    STDMETHOD(SetAllocatedVolumeSpace)(               	
        IN      LONGLONG llBytes						
        );												
        
    STDMETHOD(SetMaximumVolumeSpace)(                   
        IN      LONGLONG llBytes						
        );												

// Data members
protected:
    CVssIOCTLChannel	m_volumeIChannel;

	// Critical section or avoiding race between tasks
	CComCriticalSection	m_cs;
};


#endif // __VSSW_DIFF_HXX__
