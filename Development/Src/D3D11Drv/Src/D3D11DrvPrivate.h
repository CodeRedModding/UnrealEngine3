/*=============================================================================
	D3D11DrvPrivate.h: Private D3D RHI definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __D3D11DRVPRIVATE_H__
#define __D3D11DRVPRIVATE_H__

// Definitions.
#define DEBUG_SHADERS 0
#define D3D11 1

// Dependencies
#include "Engine.h"
#include "D3D11Drv.h"

/**
 * The D3D RHI stats.
 */
enum ED3D11RHIStats
{
	STAT_D3D11PresentTime = STAT_D3D11RHIFirstStat,
	STAT_D3D11DrawPrimitiveCalls,
	STAT_D3D11Triangles,
	STAT_D3D11Lines,
	STAT_D3D11CreateTextureTime,
	STAT_D3D11LockTextureTime,
	STAT_D3D11UnlockTextureTime,
	STAT_D3D11CopyTextureTime,
	STAT_D3D11CopyMipToMipAsyncTime,
	STAT_D3D11UploadTextureMipTime,
	STAT_D3D11CreateBoundShaderStateTime,
	STAT_D3D11ConstantBufferUpdateTime,
};


/**
 * Find an appropriate DXGI format for the input texture format and SRGB setting.  If the incoming
 * format is recognized as typeless, a fully formed format will be returned.
 *
 * @InFormat The input format who's SRGB format we must find
 */
inline DXGI_FORMAT FindDXGIFormat(DXGI_FORMAT InFormat,UBOOL bSRGB)
{
	if(bSRGB)
	{
	    switch(InFormat)
	    {
	    case DXGI_FORMAT_R8G8B8A8_UNORM:
	    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	    case DXGI_FORMAT_BC1_UNORM:
	    case DXGI_FORMAT_BC1_TYPELESS:
		    return DXGI_FORMAT_BC1_UNORM_SRGB;
	    case DXGI_FORMAT_BC2_UNORM:
	    case DXGI_FORMAT_BC2_TYPELESS:
		    return DXGI_FORMAT_BC2_UNORM_SRGB;
	    case DXGI_FORMAT_BC3_UNORM:
	    case DXGI_FORMAT_BC3_TYPELESS:
		    return DXGI_FORMAT_BC3_UNORM_SRGB;
	    };
	}
	else
	{
	    switch(InFormat)
	    {
	    case DXGI_FORMAT_R8G8B8A8_UNORM:
	    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		    return DXGI_FORMAT_R8G8B8A8_UNORM;
	    case DXGI_FORMAT_BC1_UNORM:
	    case DXGI_FORMAT_BC1_TYPELESS:
		    return DXGI_FORMAT_BC1_UNORM;
	    case DXGI_FORMAT_BC2_UNORM:
	    case DXGI_FORMAT_BC2_TYPELESS:
		    return DXGI_FORMAT_BC2_UNORM;
	    case DXGI_FORMAT_BC3_UNORM:
	    case DXGI_FORMAT_BC3_TYPELESS:
		    return DXGI_FORMAT_BC3_UNORM;
	    };
	}
	return InFormat;
}

/**
 * Find the appropriate typeless DXGI format for the input texture format.
 *
 * @InFormat The input format who's typeless format we must find
 * @bTargetable	If the surface we are getting the format for is for a render target
 */
inline DXGI_FORMAT FindTypelessDXGIFormat(DXGI_FORMAT InFormat)
{

	switch(InFormat)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_TYPELESS;
	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_TYPELESS;
	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_TYPELESS;
	};

	return InFormat;
}

#endif
