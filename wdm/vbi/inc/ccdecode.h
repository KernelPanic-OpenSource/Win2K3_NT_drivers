//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1997  Microsoft Corporation.  All Rights Reserved.
//
//
//  History:
//              22-Aug-97   TKB     Created Initial Interface Version
//
//==========================================================================;

#ifndef __CCDECODE_H
#define __CCDECODE_H

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <icodec.h>

#pragma warning(disable:4355)

//////////////////////////////////////////////////////////////
// ICCOutputPin::   Closed Captioning Output Pin Interface
//////////////////////////////////////////////////////////////

class ICCOutputPin : public IVBIOutputPin
	{
    // Usable public interfaces
public:
    ICCOutputPin(IKSDriver &driver, int nPin, PKSDATARANGE pKSDataRange ) :
        IVBIOutputPin( driver, nPin, pKSDataRange, sizeof(VBICODECFILTERING_CC_SUBSTREAMS)  ),
	    m_ScanlinesRequested(*this,KSPROPERTY_VBICODECFILTERING_SCANLINES_REQUESTED_BIT_ARRAY,
                             sizeof(VBICODECFILTERING_SCANLINES)),
	    m_ScanlinesDiscovered(*this,KSPROPERTY_VBICODECFILTERING_SCANLINES_DISCOVERED_BIT_ARRAY,
                              sizeof(VBICODECFILTERING_SCANLINES)),
	    m_SubstreamsRequested(*this,KSPROPERTY_VBICODECFILTERING_SUBSTREAMS_REQUESTED_BIT_ARRAY,
                              sizeof(VBICODECFILTERING_CC_SUBSTREAMS) ),
	    m_SubstreamsDiscovered(*this,KSPROPERTY_VBICODECFILTERING_SUBSTREAMS_DISCOVERED_BIT_ARRAY,
                               sizeof(VBICODECFILTERING_CC_SUBSTREAMS) ),
	    m_Statistics(*this,KSPROPERTY_VBICODECFILTERING_STATISTICS,
                     sizeof(VBICODECFILTERING_STATISTICS_CC_PIN))
        {}
    ~ICCOutputPin();

    // Pin specific properties (does not affect other pins)
    IScanlinesProperty	m_ScanlinesRequested;
	IScanlinesProperty	m_ScanlinesDiscovered;

	ISubstreamsProperty	m_SubstreamsRequested;
	ISubstreamsProperty	m_SubstreamsDiscovered;

    IStatisticsProperty m_Statistics;

    // Helper functions and internal data
protected:
    
    };



//////////////////////////////////////////////////////////////
// ICCDecode::      Closed Captioning Codec Interface
//////////////////////////////////////////////////////////////

class ICCDecode : public IVBICodec
    {
    // Usable public interfaces
public:
    ICCDecode();
    ~ICCDecode();

    // Call to make sure construction was successful
    BOOL IsValid() { return IVBICodec::IsValid() && m_OutputPin.IsValid(); }
        
    // Typically line 21 for actual closed captioning data (default)
    int AddRequestedScanline(int nScanline);    // Adds _another_ scanline to the request list.
    int ClearRequestedScanlines();              // Use this to reset requested scanlines to none.
    int GetDiscoveredScanlines(VBICODECFILTERING_SCANLINES &ScanlineBitArray);

    // One of KS_CC_SUBSTREAM_ODD(default), KS_CC_SUBSTREAM_EVEN
    // Readible closed captioning data is usually on the ODD field.
    int AddRequestedVideoField(int nField);     // Adds _another_ field to the request list.
    int ClearRequestedVideoFields();            // Use this to reset requested fields to none.
    int GetDiscoveredVideoFields(VBICODECFILTERING_CC_SUBSTREAMS &bitArray);

	// Statistics Property Control
	int GetCodecStatistics(VBICODECFILTERING_STATISTICS_CC &CodecStatistics);
	int SetCodecStatistics(VBICODECFILTERING_STATISTICS_CC &CodecStatistics);
  	int GetPinStatistics(VBICODECFILTERING_STATISTICS_CC_PIN &PinStatistics);
	int SetPinStatistics(VBICODECFILTERING_STATISTICS_CC_PIN &PinStatistics);

    // Read function (call "overlapped" at THREAD_PRIORITY_ABOVE_NORMAL to avoid data loss)
    int ReadData( LPBYTE lpBuffer, int nBytes, DWORD *lpcbReturned, LPOVERLAPPED lpOS )
        { return m_OutputPin.ReadData( lpBuffer, nBytes, lpcbReturned, lpOS ); }
    int GetOverlappedResult( LPOVERLAPPED lpOS, LPDWORD lpdwTransferred = NULL, BOOL bWait=TRUE )
        { return m_OutputPin.GetOverlappedResult(lpOS, lpdwTransferred, bWait ); }

    // Helper functions and internal data
    // Actual Pin instance [w/properties] (set by above to control filtering & to get discovered)
    ICCOutputPin       m_OutputPin;

    // Additional driver global properties
    IStatisticsProperty m_Statistics;
protected:
};

#pragma warning(default:4355)

#endif

