/*=============================================================================
	CapturePin.h: CapturePin definition
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef _CAPTUREPIN_HEADER_
#define _CAPTUREPIN_HEADER_

#if _WINDOWS

#pragma warning(disable : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(disable : 4264) // 'virtual_function' : no override available for virtual member function from base 
#pragma pack(push,8)
#include <streams.h>
#pragma pack(pop)
#pragma warning(default : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(default : 4264) // 'virtual_function' : no override available for virtual member function from base


class FCapturePin : public CSourceStream
{
protected:

	/** To track where we are in the file */
	INT FramesWritten;
	/** Do we need to clear the buffer? */
	UBOOL bZeroMemory; 
	/** The time stamp for each sample */
	CRefTime SampleTime;
	/** The length of the frame, used for playback */
	const REFERENCE_TIME FrameLength;
	/** Rect containing entire screen coordinates */
	RECT Screen;                     
	/** The current image height */
	INT ImageHeight;
	/** And current image width */
	INT ImageWidth;
	/** Time in msec between frames */
	INT RepeatTime;
	/** Screen bit depth */
	INT CurrentBitDepth;

	/** Stores the media type to use */
	CMediaType MediaType;
	/** Protects our internal state */
	CCritSec SharedState;
	/** Figures out our media type for us */
	CImageDisplay Display;

public:

	FCapturePin(HRESULT *phr, CSource *pFilter);
	~FCapturePin();

	// Override the version that offers exactly one media type
	HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
	HRESULT FillBuffer(IMediaSample *pSample);

	// the loop executed while running
	HRESULT DoBufferProcessingLoop(void);

	// Set the agreed media type and set up the necessary parameters
	HRESULT SetMediaType(const CMediaType *pMediaType);

	// Support multiple display formats
	HRESULT CheckMediaType(const CMediaType *pMediaType);
	HRESULT GetMediaType(INT iPosition, CMediaType *pmt);

	// Quality control
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.
	STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q)
	{
		return E_FAIL;
	}

};
#endif //#if _WINDOWS


#endif	//#ifndef _CAPTUREPIN_HEADER_

