/*=============================================================================
	D3DPixels.cpp: Direct3D pixel manipulation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	FFloatIEEE
//

struct FFloatIEEE
{
	union
	{
		struct
		{
			DWORD	Mantissa : 23,
					Exponent : 8,
					Sign : 1;
		} Components;

		FLOAT	Float;
	};
};

//
//	FD3DFloat16
//

struct FD3DFloat16
{
	union
	{
		struct
		{
			_WORD	Mantissa : 10,
					Exponent : 5,
					Sign : 1;
		} Components;

		_WORD	Encoded;
	};

	operator FLOAT()
	{
		FFloatIEEE	Result;

		Result.Components.Sign = Components.Sign;
		Result.Components.Exponent = Components.Exponent - 15 + 127; // Stored exponents are biased by half their range.
		Result.Components.Mantissa = Min<DWORD>(appFloor((FLOAT)Components.Mantissa / 1024.0f * 8388608.0f),(1 << 23) - 1);

		return Result.Float;
	}
};

//
//	UD3DRenderDevice::DumpRenderTarget
//

void UD3DRenderDevice::DumpRenderTarget(const TD3DRef<IDirect3DSurface9>& RenderTarget,TArray<FColor>& OutputData,UINT& OutputSizeX,UINT& OutputSizeY)
{
	HRESULT	Result;

	// Get information about the surface.

	D3DSURFACE_DESC	RenderTargetDesc;
	Result = RenderTarget->GetDesc(&RenderTargetDesc);
	if(FAILED(Result))
		appErrorf(TEXT("Failed to get render target description: %s"),*D3DError(Result));

	// Allocate the output buffer.

	OutputData.Add(RenderTargetDesc.Width * RenderTargetDesc.Height);
	OutputSizeX = RenderTargetDesc.Width;
	OutputSizeY = RenderTargetDesc.Height;

	// Read the data out of the temporary surface.

	D3DLOCKED_RECT	LockedRect;

#ifndef XBOX
	// Create a temporary surface to the hold the render target contents.

	TD3DRef<IDirect3DSurface9>	TempSurface;

	Result = GDirect3DDevice->CreateOffscreenPlainSurface( RenderTargetDesc.Width, RenderTargetDesc.Height, RenderTargetDesc.Format, D3DPOOL_SYSTEMMEM, &TempSurface.Handle, NULL );
	if(FAILED(Result))
		appErrorf(TEXT("Failed to create temporary system memory surface: %s"),*D3DError(Result));

	// Read the render target data into the temporary surface.

	Result = GDirect3DDevice->GetRenderTargetData(RenderTarget,TempSurface);
	if(FAILED(Result))
		appErrorf(TEXT("Failed to read render target data to temporary surface: %s"),*D3DError(Result));

	Result = TempSurface->LockRect(&LockedRect,NULL,D3DLOCK_READONLY);
#else
	// Create a temporary texture to hold the render target contents.

	TD3DRef<IDirect3DTexture9>	TempTexture;

	Result = GDirect3DDevice->CreateTexture( RenderTargetDesc.Width, RenderTargetDesc.Height, 1, 0, RenderTargetDesc.Format, D3DPOOL_MANAGED, &TempTexture.Handle, NULL );
	if(FAILED(Result))
		appErrorf(TEXT("Failed to create temporary texture: %s"),*D3DError(Result));

	// Move render target data into texture.

	TD3DRef<IDirect3DSurface9>	SavedRenderTarget;
	GDirect3DDevice->GetRenderTarget(0,&SavedRenderTarget.Handle);
	GDirect3DDevice->SetRenderTarget(0,RenderTarget.Handle);

	D3DRECT SrcRect;
	SrcRect.x1	= 0;
	SrcRect.x2	= RenderTargetDesc.Width;
	SrcRect.y1	= 0;
	SrcRect.y2	= RenderTargetDesc.Height;
	TempTexture->MoveResourceMemory( D3DMEM_VRAM );
	Result = GDirect3DDevice->Resolve( D3DRESOLVE_RENDERTARGET0, &SrcRect, TempTexture, NULL, 0, 0, 0, 0, 0, NULL );
	if(FAILED(Result))
		appErrorf(TEXT("Failed to resolve render target: %s"),*D3DError(Result));
	TempTexture->MoveResourceMemory( D3DMEM_AGP );

	GDirect3DDevice->SetRenderTarget(0, SavedRenderTarget.Handle);

	Result = TempTexture->LockRect(0,&LockedRect,NULL,D3DLOCK_READONLY);

	// Untile if necessary.

	XGTEXTURE_DESC	TextureDesc;
	BYTE*			UntiledImage = NULL;

	XGGetTextureDesc( TempTexture, 0, &TextureDesc );
	if( TextureDesc.Format & D3DFORMAT_TILED_MASK )
	{
		RECT SrcRect;
		SrcRect.top		= 0;
		SrcRect.bottom	= OutputSizeY;
		SrcRect.left	= 0;
		SrcRect.right	= OutputSizeX;

		UntiledImage = new BYTE[ OutputSizeY * LockedRect.Pitch ];
		XGUntileSurface( UntiledImage, LockedRect.Pitch, NULL, LockedRect.pBits, OutputSizeX, OutputSizeY, &SrcRect, TextureDesc.BitsPerPixel / 8 );
		LockedRect.pBits = UntiledImage;
	}
#endif
	if(FAILED(Result))
		appErrorf(TEXT("Failed to lock temporary surface: %s"),*D3DError(Result));

	if(RenderTargetDesc.Format == D3DFMT_R32F)
	{
		FLOAT	MinValue = 0.0f,
				MaxValue = 0.0f;

		for(UINT Y = 0;Y < RenderTargetDesc.Height;Y++)
		{
			FLOAT*	SrcPtr = (FLOAT*)((BYTE*)LockedRect.pBits + Y * LockedRect.Pitch);

			for(UINT X = 0;X < RenderTargetDesc.Width;X++)
			{
				MinValue = Min(*SrcPtr,MinValue);
				MaxValue = Max(*SrcPtr,MaxValue);
				SrcPtr++;
			}
		}

		for(UINT Y = 0;Y < RenderTargetDesc.Height;Y++)
		{
			FLOAT*	SrcPtr = (FLOAT*)((BYTE*)LockedRect.pBits + Y * LockedRect.Pitch);
			FColor*	DestPtr = &OutputData(Y * RenderTargetDesc.Width);

			for(UINT X = 0;X < RenderTargetDesc.Width;X++)
			{
				FLOAT	Value = *SrcPtr++;
				*DestPtr++ = FColor(FPlane(Value / MaxValue,Value / MinValue,0,0));
			}
		}
	}
	else if(RenderTargetDesc.Format == D3DFMT_A16B16G16R16F)
	{
		FPlane	MinValue(0.0f,0.0f,0.0f,0.0f),
				MaxValue(1.0f,1.0f,1.0f,1.0f);

		check(sizeof(FD3DFloat16)==sizeof(_WORD));

		for(UINT Y = 0;Y < RenderTargetDesc.Height;Y++)
		{
			FD3DFloat16*	SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + Y * LockedRect.Pitch);

			for(UINT X = 0;X < RenderTargetDesc.Width;X++)
			{
				MinValue.X = Min<FLOAT>(SrcPtr[0],MinValue.X);
				MinValue.Y = Min<FLOAT>(SrcPtr[1],MinValue.Y);
				MinValue.Z = Min<FLOAT>(SrcPtr[2],MinValue.Z);
				MinValue.W = Min<FLOAT>(SrcPtr[3],MinValue.W);
				MaxValue.X = Max<FLOAT>(SrcPtr[0],MaxValue.X);
				MaxValue.Y = Max<FLOAT>(SrcPtr[1],MaxValue.Y);
				MaxValue.Z = Max<FLOAT>(SrcPtr[2],MaxValue.Z);
				MaxValue.W = Max<FLOAT>(SrcPtr[3],MaxValue.W);
				SrcPtr += 4;
			}
		}

		for(UINT Y = 0;Y < RenderTargetDesc.Height;Y++)
		{
			FD3DFloat16*	SrcPtr = (FD3DFloat16*)((BYTE*)LockedRect.pBits + Y * LockedRect.Pitch);
			FColor*			DestPtr = &OutputData(Y * RenderTargetDesc.Width);

			for(UINT X = 0;X < RenderTargetDesc.Width;X++)
			{
				*DestPtr++ = FColor(
								FPlane(
									SrcPtr[0] / (MaxValue.X - MinValue.X),
									SrcPtr[1] / (MaxValue.Y - MinValue.Y),
									SrcPtr[2] / (MaxValue.Z - MinValue.Z),
									SrcPtr[3] / (MaxValue.W - MinValue.W)
									)
								);
				SrcPtr += 4;
			}
		}
	}
	else if(RenderTargetDesc.Format == D3DFMT_R5G6B5)
	{
		for(UINT Y = 0;Y < RenderTargetDesc.Height;Y++)
		{
			_WORD*	SrcPtr = (_WORD*)((BYTE*)LockedRect.pBits + Y * LockedRect.Pitch);
			FColor*	DestPtr = &OutputData(Y * RenderTargetDesc.Width);

			for(UINT X = 0;X < RenderTargetDesc.Width;X++)
			{
				*DestPtr++ = FColor(FPlane(((*SrcPtr & 0xf800) >> 11) / 31.0f,(FLOAT)((*SrcPtr & 0x07e0) >> 5) / 63.0f,(FLOAT)(*SrcPtr & 0x001f) / 31.0f,0));
				SrcPtr++;
			}
		}
	}
	else if(RenderTargetDesc.Format == D3DFMT_A8R8G8B8)
	{
		for(UINT Y = 0;Y < RenderTargetDesc.Height;Y++)
			appMemcpy( &OutputData(Y * RenderTargetDesc.Width), (BYTE*)LockedRect.pBits + Y * LockedRect.Pitch, RenderTargetDesc.Width * sizeof(FColor) );
	}
#ifndef XBOX
	verify(SUCCEEDED(TempSurface->UnlockRect()));
#else
	delete [] UntiledImage;
	verify(SUCCEEDED(TempTexture->UnlockRect(0)));
#endif
}

//
//	UD3DRenderDevice::SaveRenderTarget
//

void UD3DRenderDevice::SaveRenderTarget(const TD3DRef<IDirect3DSurface9>& RenderTarget,const TCHAR* BaseFilename,UBOOL Alpha)
{
	TArray<FColor>	Data;
	UINT			SizeX = 0,
					SizeY = 0;

	DumpRenderTarget(RenderTarget,Data,SizeX,SizeY);

	if(Alpha)
		for(UINT Y = 0;Y < SizeY;Y++)
			for(UINT X = 0;X < SizeX;X++)
				Data(X + Y * SizeX) = FColor(FPlane(1,1,1,1) * (FLOAT)Data(X + Y * SizeX).A / 255.0f);

	appCreateBitmap(BaseFilename,SizeX,SizeY,&Data(0));

}

//
//	UD3DRenderDevice::ReadPixels
//

void UD3DRenderDevice::ReadPixels(FChildViewport* Viewport,FColor* OutputBuffer)
{
	HRESULT	Result;

	// Find the corresponding D3D viewport.

	INT	ViewportIndex = GetViewportIndex(Viewport);
	check(ViewportIndex != INDEX_NONE);

	FD3DViewport*	D3DViewport = Viewports(ViewportIndex);

	if(FullscreenViewport && D3DViewport != FullscreenViewport)
		return;

	// Lock the backbuffer.

	D3DLOCKED_RECT	LockedRect;

	Result = BackBuffer->LockRect(&LockedRect,NULL,D3DLOCK_READONLY);
	if(FAILED(Result))
		appErrorf(TEXT("Failed to lock backbuffer: %s"),*D3DError(Result));

	// Copy the back buffer to the output buffer.

	for(UINT Y = 0;Y < D3DViewport->SizeY;Y++)
		appMemcpy(OutputBuffer + Y * D3DViewport->SizeX,(FColor*)((BYTE*)LockedRect.pBits + Y * LockedRect.Pitch),D3DViewport->SizeX * sizeof(FColor));

	// Unlock the backbuffer.

	Result = BackBuffer->UnlockRect();
	if(FAILED(Result))
		appErrorf(TEXT("Failed to unlock backbuffer: %s"),*D3DError(Result));

}
