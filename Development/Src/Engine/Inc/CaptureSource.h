/*=============================================================================
	CaptureSource.h: CaptureSource definition
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _CAPTURESOURCE_HEADER_
#define _CAPTURESOURCE_HEADER_

#if _WINDOWS

#include "CapturePin.h"

// {9A80E195-3BBA-4821-B18B-21BB496F80F8}
DEFINE_GUID(CLSID_CaptureSource, 
			0x9a80e195, 0x3bba, 0x4821, 0xb1, 0x8b, 0x21, 0xbb, 0x49, 0x6f, 0x80, 0xf8);

class FCaptureSource : public CSource
{

public:
	FCaptureSource(IUnknown *pUnk, HRESULT *phr);
	~FCaptureSource();
private:
	FCapturePin *CapturePin;

public:
	static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);  

};
#endif //#if _WINDOWS

#endif	//#ifndef _CAPTURESOURCE_HEADER_