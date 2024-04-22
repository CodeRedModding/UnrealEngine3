/*=============================================================================
	D3D9Util.h: D3D RHI utility implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

/** Returns the string equivalent of the input ErrorCode. */
FString GetD3D9ErrorString(HRESULT ErrorCode)
{
	FString ErrorCodeText;
#define D3DERR(x) case x: ErrorCodeText = TEXT(#x); break;
	switch(ErrorCode)
	{
		D3DERR(D3D_OK)
		D3DERR(D3DERR_WRONGTEXTUREFORMAT)
		D3DERR(D3DERR_UNSUPPORTEDCOLOROPERATION)
		D3DERR(D3DERR_UNSUPPORTEDCOLORARG)
		D3DERR(D3DERR_UNSUPPORTEDALPHAOPERATION)
		D3DERR(D3DERR_UNSUPPORTEDALPHAARG)
		D3DERR(D3DERR_TOOMANYOPERATIONS)
		D3DERR(D3DERR_CONFLICTINGTEXTUREFILTER)
		D3DERR(D3DERR_UNSUPPORTEDFACTORVALUE)
		D3DERR(D3DERR_CONFLICTINGRENDERSTATE)
		D3DERR(D3DERR_UNSUPPORTEDTEXTUREFILTER)
		D3DERR(D3DERR_CONFLICTINGTEXTUREPALETTE)
		D3DERR(D3DERR_DRIVERINTERNALERROR)
		D3DERR(D3DERR_NOTFOUND)
		D3DERR(D3DERR_MOREDATA)
		D3DERR(D3DERR_DEVICELOST)
		D3DERR(D3DERR_DEVICENOTRESET)
		D3DERR(D3DERR_NOTAVAILABLE)
		D3DERR(D3DERR_OUTOFVIDEOMEMORY)
		D3DERR(D3DERR_INVALIDDEVICE)
		D3DERR(D3DERR_INVALIDCALL)
		D3DERR(D3DERR_DRIVERINVALIDCALL)
		D3DERR(D3DXERR_INVALIDDATA)
		D3DERR(E_OUTOFMEMORY)
		default: ErrorCodeText = FString::Printf(TEXT("%08X"),(INT)ErrorCode);
	}
#undef D3DERR
	return ErrorCodeText;
}

FString GetD3DTextureFormatString(D3DFORMAT TextureFormat)
{
	FString TextureFormatText;
#define D3DFORMATCASE(x) case x: TextureFormatText = TEXT(#x); break;
	switch(TextureFormat)
	{
		D3DFORMATCASE(D3DFMT_A8R8G8B8)
		D3DFORMATCASE(D3DFMT_X8R8G8B8)
		D3DFORMATCASE(D3DFMT_DXT1)
		D3DFORMATCASE(D3DFMT_DXT3)
		D3DFORMATCASE(D3DFMT_DXT5)
		D3DFORMATCASE(D3DFMT_A16B16G16R16F)
		D3DFORMATCASE(D3DFMT_A32B32G32R32F)
		D3DFORMATCASE(D3DFMT_UNKNOWN)
		D3DFORMATCASE(D3DFMT_L8)
		D3DFORMATCASE(D3DFMT_UYVY)
		D3DFORMATCASE(D3DFMT_D24S8)
		D3DFORMATCASE(D3DFMT_D24X8)
		D3DFORMATCASE(D3DFMT_R32F)
		D3DFORMATCASE(D3DFMT_G16R16)
		D3DFORMATCASE(D3DFMT_G16R16F)
		D3DFORMATCASE(D3DFMT_G32R32F)
		D3DFORMATCASE(D3DFMT_A2B10G10R10)
		D3DFORMATCASE(D3DFMT_A16B16G16R16)
		D3DFORMATCASE(D3DFMT_V8U8)
		D3DFORMATCASE(D3DFMT_A1);
		default: TextureFormatText = FString::Printf(TEXT("%08X"),(INT)TextureFormat);
	}
#undef D3DFORMATCASE
	return TextureFormatText;
}

FString GetD3DTextureFlagString(DWORD TextureFlags)
{
	FString TextureFormatText = TEXT("");

	if (TextureFlags & D3DUSAGE_DEPTHSTENCIL)
	{
		TextureFormatText += TEXT("D3DUSAGE_DEPTHSTENCIL ");
	}

	if (TextureFlags & D3DUSAGE_RENDERTARGET)
	{
		TextureFormatText += TEXT("D3DUSAGE_RENDERTARGET ");
	}

	return TextureFormatText;
}

void VerifyD3D9Result(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,UINT Line)
{
	if (D3DResult == D3DERR_OUTOFVIDEOMEMORY || D3DResult == E_OUTOFMEMORY)
	{
		appMsgf(
			AMT_OK,
			*LocalizeError(TEXT("Error_RanOutOfVideoMemory"), TEXT("Launch"))
			);
		appRequestExit(TRUE);
	}
	else if(FAILED(D3DResult))
	{
		const FString& ErrorString = GetD3D9ErrorString(D3DResult);
		appErrorf(TEXT("%s failed \n at %s:%u \n with error %s"),ANSI_TO_TCHAR(Code),ANSI_TO_TCHAR(Filename),Line,*ErrorString);
	}
}

void VerifyD3D9CreateTextureResult(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,UINT Line,UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags)
{
	if (D3DResult == D3DERR_OUTOFVIDEOMEMORY || D3DResult == E_OUTOFMEMORY)
	{
		appMsgf(
			AMT_OK,
			*LocalizeError(TEXT("Error_RanOutOfVideoMemory"), TEXT("Launch"))
			);
		appRequestExit(TRUE);
	}
	else if(FAILED(D3DResult))
	{
		const FString& ErrorString = GetD3D9ErrorString(D3DResult);
		const FString& D3DFormatString = GetD3DTextureFormatString((D3DFORMAT)GPixelFormats[Format].PlatformFormat);
		appErrorf(
			TEXT("%s failed \n at %s:%u \n with error %s, \n SizeX=%i, SizeY=%i, Format=%s=%s, NumMips=%i, Flags=%s, TexMemoryAvailable=%dMB"),
			ANSI_TO_TCHAR(Code),
			ANSI_TO_TCHAR(Filename),
			Line,
			*ErrorString, 
			SizeX, 
			SizeY, 
			GPixelFormats[Format].Name, 
			*D3DFormatString, NumMips, 
			*GetD3DTextureFlagString(Flags),
			RHIGetAvailableTextureMemory());
	}
}

#if !USE_NULL_RHI
/**
 * Adds a PIX event using the D3DPerf api
 *
 * @param Color The color to draw the event as
 * @param Text The text displayed with the event
 */
void appBeginDrawEvent(const FColor& Color,const TCHAR* Text)
{
	GDynamicRHI->PushEvent(Text);
	D3DPERF_BeginEvent(Color.DWColor(),Text);
}

/**
 * Ends the current PIX event
 */
void appEndDrawEvent(void)
{
	GDynamicRHI->PopEvent();
	D3DPERF_EndEvent();
}

/**
 * Platform specific function for setting the value of a counter that can be
 * viewed in PIX.
 */
void appSetCounterValue(const TCHAR* CounterName, FLOAT Value)
{
}
#endif //USE_NULL_RHI

//
// Stat declarations.
//

DECLARE_STATS_GROUP(TEXT("D3D9RHI"),STATGROUP_D3D9RHI);
DECLARE_CYCLE_STAT(TEXT("Present time"),STAT_D3D9PresentTime,STATGROUP_D3D9RHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("DrawPrimitive calls"),STAT_D3D9DrawPrimitiveCalls,STATGROUP_D3D9RHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("Triangles drawn"),STAT_D3D9Triangles,STATGROUP_D3D9RHI);
DECLARE_DWORD_COUNTER_STAT(TEXT("Lines drawn"),STAT_D3D9Lines,STATGROUP_D3D9RHI);
