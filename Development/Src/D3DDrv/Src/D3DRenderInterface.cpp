/*=============================================================================
	D3DRenderInterface.cpp: Unreal Direct3D rendering interface implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"
// Needed for FTerrainLightMap
#include "UnTerrain.h"

//
//	FD3DRenderInterface::BeginScene
//

void FD3DRenderInterface::BeginScene(FD3DViewport* Viewport,UBOOL InHitTesting)
{
	CurrentViewport = Viewport;
	HitTesting = InHitTesting;
	CurrentHitProxyIndex = 0;

	// Begin rendering.

	HRESULT	Result = GDirect3DDevice->BeginScene();
	if(FAILED(Result))
		appErrorf(TEXT("BeginScene failed: %s"),*D3DError(Result));

	// Set initial state.

	if(HitTesting)
	{
		GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->HitProxyBuffer);
		SetViewport(0,0,CurrentViewport->SizeX,CurrentViewport->SizeY);
		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,0,0,0);
	}
	else
	{
		GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->BackBuffer);
		SetViewport(0,0,CurrentViewport->SizeX,CurrentViewport->SizeY);
	}

	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	GDirect3DDevice->SetDepthStencilSurface(NULL);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_ALWAYS);
	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	GD3DRenderDevice->CurrentVertexShader = NULL;
	GD3DRenderDevice->CurrentPixelShader = NULL;
}

//
//	FD3DRenderInterface::EndScene
//

void FD3DRenderInterface::EndScene(UBOOL Synchronous)
{
	FlushBatchedPrimitives();

	GD3DRenderDevice->DumpRenderTargets = 0;

	FCycleCounterSection	FinishCycleCounter(GD3DStats.FinishRenderTime);

	SetHitProxy(NULL);

	// Reset device state.

	GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->BackBuffer);
	GDirect3DDevice->SetDepthStencilSurface(GD3DRenderDevice->DepthBuffer);

	for(UINT TextureIndex = 0;TextureIndex < MAX_TEXTURES;TextureIndex++)
		GDirect3DDevice->SetTexture(TextureIndex,NULL);

	GD3DRenderDevice->SetVertexShader(NULL);

	for(UINT StreamIndex = 0;StreamIndex < 16;StreamIndex++)
		GDirect3DDevice->SetStreamSource(StreamIndex,NULL,0,0);

	GDirect3DDevice->SetIndices(NULL);

	GD3DRenderDevice->SetPixelShader(NULL);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,0);

	// Notify D3D that we're done with the frame.

	GDirect3DDevice->EndScene();

	HRESULT Result = S_OK;
	if(!HitTesting && !GIsRequestingExit)
	{
		if(GD3DRenderDevice->FullscreenViewport == CurrentViewport)
			Result = GDirect3DDevice->Present(NULL,NULL,NULL,NULL);
		else
		{
			RECT	SourceRect;
			SourceRect.left = SourceRect.top = 0;
			SourceRect.right = CurrentViewport->SizeX;
			SourceRect.bottom = CurrentViewport->SizeY;
			Result = GDirect3DDevice->Present(&SourceRect,NULL,(HWND)CurrentViewport->Viewport->GetWindow(),NULL);
		}
	}

#ifdef XBOX
	DeferredFreeResources();
#endif

	if( Result == D3DERR_DEVICELOST )
		GD3DRenderDevice->DeviceLost = 1;

	if( Synchronous )
		Wait();

	CurrentViewport = NULL;

	if( GStreamingStats.Enabled || GMemoryStats.Enabled )
	{
		for(UINT SetIndex = 0;SetIndex < (UINT)GD3DRenderDevice->ResourceSets.Num();SetIndex++)
		{	
			for(FD3DResource* Resource = GD3DRenderDevice->ResourceSets(SetIndex)->FirstResource;Resource;Resource = Resource->NextResource)
			{
				UBOOL				IsTextureType	=	Resource->ResourceType->IsA(FTexture2D::StaticType)
													||	Resource->ResourceType->IsA(FTexture3D::StaticType)
													||	Resource->ResourceType->IsA(FTextureCube::StaticType);
				FD3DTexture*		Texture			=	IsTextureType ? (FD3DTexture*) Resource : NULL;
				FD3DIndexBuffer*	IndexBuffer		=	Resource->ResourceType->IsA(FIndexBuffer::StaticType) ? (FD3DIndexBuffer*) Resource : NULL;
				FD3DVertexBuffer*	VertexBuffer	= 	Resource->ResourceType->IsA(FVertexBuffer::StaticType) ? (FD3DVertexBuffer*) Resource : NULL;
			
				if( Texture )
				{
					FLOAT TextureSize		= Resource->GetSize() / 1024.f / 1024.f;
					FLOAT MaxTextureSize	= Resource->GetMaxSize() / 1024.f / 1024.f;
					
					GStreamingStats.MaxTextureSize.Value += MaxTextureSize;
					
					if( Texture->IsStreamingTexture )
					{
						GStreamingStats.TextureSize.Value += TextureSize;
					}

					if( Resource->ResourceType->IsA(FStaticModelLightTexture::StaticType) )
					{
						GMemoryStats.BSPShadowMapSize.Value += TextureSize;
					}
					else
					if( Resource->ResourceType->IsA(FStaticTerrainLight::StaticType) )
					{
						GMemoryStats.TerrainShadowMapSize.Value += TextureSize;
					}
					else 
					if( Resource->ResourceType->IsA(UShadowMap::StaticType) )
					{
						GMemoryStats.StaticMeshShadowMapSize.Value += TextureSize;
					}
					else
					{
						GMemoryStats.TextureSize.Value += TextureSize;
					}
				}
				else if( IndexBuffer )
				{
					GMemoryStats.IndexBufferSize.Value += Resource->GetSize() / 1024.f / 1024.f;
				}
				else if( VertexBuffer )
				{
					GMemoryStats.VertexBufferSize.Value += Resource->GetSize() / 1024.f / 1024.f;
				}
			}
		}
	}

	GD3DStats.BufferSize.Value += GD3DRenderDevice->GetBufferMemorySize();
}

//
//	FD3DRenderInterface::SetViewport
//

void FD3DRenderInterface::SetViewport(UINT X,UINT Y,UINT Width,UINT Height)
{
	D3DVIEWPORT9	Viewport;
	Viewport.X = X;
	Viewport.Y = Y;
	Viewport.Width = Width;
	Viewport.Height = Height;
	Viewport.MinZ = 0.0f;
	Viewport.MaxZ = 1.0f;
	GDirect3DDevice->SetViewport(&Viewport);
}

//
//	FD3DRenderInterface::DrawQuad
//

void FD3DRenderInterface::DrawQuad(FLOAT X,FLOAT Y,INT XL,INT YL,FLOAT U,FLOAT V,FLOAT UL,FLOAT VL,FLOAT Z)
{
	HRESULT	Result;

	// Determine the size of the current viewport.
	D3DVIEWPORT9 Viewport;
	Result = GDirect3DDevice->GetViewport(&Viewport);
	if(FAILED(Result))
		appErrorf(TEXT("GetViewport failed: %s"),*D3DError(Result));

	// Fill the surface.

	FD3DCanvasVertex	Vertices[4];
	Vertices[0].Set(X,Y,0.0f,Viewport.Width,Viewport.Height,U,V);
	Vertices[1].Set(X + XL,Y,0.0f,Viewport.Width,Viewport.Height,U + UL,V);
	Vertices[2].Set(X + XL,Y + YL,0.0f,Viewport.Width,Viewport.Height,U + UL,V + VL);
	Vertices[3].Set(X,Y + YL,0.0f,Viewport.Width,Viewport.Height,U,V + VL);

    GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
	GDirect3DDevice->DrawPrimitiveUP(
		D3DPT_TRIANGLEFAN,
		2,
		Vertices,
		sizeof(FD3DCanvasVertex)
		);
}

//
//	FD3DRenderInterface::FillSurface
//

void FD3DRenderInterface::FillSurface(const TD3DRef<IDirect3DSurface9>& Surface,FLOAT X,FLOAT Y,INT XL,INT YL,FLOAT U,FLOAT V,FLOAT UL,FLOAT VL,UBOOL Add)
{
	BEGIND3DCYCLECOUNTER(GD3DStats.FillRenderTime);

	// Fill in optional parameters.

	D3DSURFACE_DESC	SurfaceDesc;
	HRESULT	Result = Surface->GetDesc(&SurfaceDesc);
	if(FAILED(Result))
		appErrorf(TEXT("Failed to get surface description: %s"),*D3DError(Result));

	if(XL == -1)
		XL = SurfaceDesc.Width;

	if(YL == -1)
		YL = SurfaceDesc.Height;

	// Save state.

	TD3DRef<IDirect3DSurface9>	SavedRenderTarget;
	D3DVIEWPORT9				SavedViewport;
	DWORD						SavedZEnable,
								SavedCullMode,
								SavedStencilEnable,
								SavedScissorTestEnable,
								SavedAlphaBlendEnable;

	GDirect3DDevice->GetViewport(&SavedViewport);
	GDirect3DDevice->GetRenderTarget(0,&SavedRenderTarget.Handle);
	GDirect3DDevice->GetRenderState(D3DRS_ZENABLE,&SavedZEnable);
	GDirect3DDevice->GetRenderState(D3DRS_CULLMODE,&SavedCullMode);
	GDirect3DDevice->GetRenderState(D3DRS_STENCILENABLE,&SavedStencilEnable);
	GDirect3DDevice->GetRenderState(D3DRS_SCISSORTESTENABLE,&SavedScissorTestEnable);
	GDirect3DDevice->GetRenderState(D3DRS_ALPHABLENDENABLE,&SavedAlphaBlendEnable);

	// Set state.

	GDirect3DDevice->SetRenderTarget(0,Surface);

	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,Add);
	GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
	GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE);
	GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);

	// Draw the quad.

	DrawQuad(X,Y,XL,YL,U,V,UL,VL);

	// Restore state.

	GDirect3DDevice->SetRenderTarget(0,SavedRenderTarget);
	GDirect3DDevice->SetViewport(&SavedViewport);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,SavedZEnable);
	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,SavedCullMode);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,SavedAlphaBlendEnable);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE,SavedStencilEnable);
	GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,SavedScissorTestEnable);

	ENDD3DCYCLECOUNTER;
}

//
//	FD3DRenderInterface::FlushBatchedPrimitives
//

void FD3DRenderInterface::FlushBatchedPrimitives()
{
	check(!GD3DSceneRenderer);

	if(BatchedIndices.Num())
	{
		FCycleCounterSection	CycleCounter(GD3DStats.TileRenderTime);

		if(HitTesting)
		{
			if(BatchedTexture)
			{
				GD3DRenderDevice->TileHitProxyShader->Set(BatchedTexture);
			}
			else
			{
				GD3DRenderDevice->SolidTileHitProxyShader->Set(NULL);
			}
			GDirect3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE,1);
			GDirect3DDevice->SetRenderState(D3DRS_ALPHAREF,0);
			GDirect3DDevice->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);
		}
		else
		{
			if(BatchedTexture)
			{
				GD3DRenderDevice->TileShader->Set(BatchedTexture);
			}
			else
			{
				GD3DRenderDevice->SolidTileShader->Set(NULL);
			}
			GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,BatchedAlphaBlend);
			GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
			GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
		}

		UINT	NumPrimitives = 0;
		switch(BatchedPrimitiveType)
		{
		case D3DPT_TRIANGLELIST:
			NumPrimitives = BatchedIndices.Num() / 3;
			break;
		case D3DPT_LINELIST:
			NumPrimitives = BatchedIndices.Num() / 2;
			break;
		}

		GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
		GDirect3DDevice->DrawIndexedPrimitiveUP(
			BatchedPrimitiveType,
			0,
			BatchedVertices.Num(),
			NumPrimitives,
			&BatchedIndices(0),
			D3DFMT_INDEX16,
			&BatchedVertices(0),
			sizeof(FD3DCanvasVertex)
			);

		GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
		GDirect3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE,0);
		
		BatchedVertices.Empty();
		BatchedIndices.Empty();
		if(BatchedTexture)
			BatchedTexture->RemoveRef();
		BatchedTexture = NULL;

		GD3DStats.TileBatches.Value++;
	}
}

//
//	FD3DRenderInterface::SetBatchState
//

void FD3DRenderInterface::SetBatchState(D3DPRIMITIVETYPE PrimitiveType,FD3DTexture* Texture,UBOOL AlphaBlend)
{
	UBOOL	StateChanged	= (BatchedPrimitiveType != PrimitiveType) || (BatchedTexture != Texture) || (BatchedAlphaBlend != AlphaBlend),
			TooManyIndices	= (BatchedIndices.Num()+6) * sizeof(_WORD)				>= 65536, //@todo xenon: DrawPrimitiveUP is limited to 64K data with July XDK
			TooManyVertices	= (BatchedIndices.Num()+4) * sizeof(FD3DCanvasVertex)	>= 65536; //@todo xenon: DrawPrimitiveUP is limited to 64K data with July XDK

	if( (StateChanged || TooManyIndices || TooManyVertices) && BatchedIndices.Num() )
		FlushBatchedPrimitives();

	if(!BatchedIndices.Num())
	{
		BatchedPrimitiveType = PrimitiveType;
		BatchedTexture = Texture;
		if(Texture)
			Texture->AddRef();
		BatchedAlphaBlend = AlphaBlend;
	}
}

//
//	FD3DRenderInterface::Clear
//

void FD3DRenderInterface::Clear(const FColor& Color)
{
	check(!GD3DSceneRenderer);

	FlushBatchedPrimitives();

	if(!HitTesting)
	{
		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_RGBA(Color.R,Color.G,Color.B,Color.A),1.0f,0);
	}
}

//
//	FD3DRenderInterface::DrawLine2D
//

void FD3DRenderInterface::DrawLine2D(INT X1,INT Y1,INT X2,INT Y2,FColor Color)
{
	check(!GD3DSceneRenderer);

	X1 = (X1 * Zoom2D) + Origin2D.X;
	Y1 = (Y1 * Zoom2D) + Origin2D.Y;
	X2 = (X2 * Zoom2D) + Origin2D.X;
	Y2 = (Y2 * Zoom2D) + Origin2D.Y;

	SetBatchState(D3DPT_LINELIST,NULL,0);

	INT					BaseVertexIndex = BatchedVertices.Add(2);
	FD3DCanvasVertex*	Vertices = &BatchedVertices(BaseVertexIndex);

	Vertices[0].Set(X1,Y1,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,0.0f,0.0f,0.0f,Color);
	Vertices[1].Set(X2,Y2,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,0.0f,0.0f,0.0f,Color);

	BatchedIndices.AddItem(BaseVertexIndex + 0);
	BatchedIndices.AddItem(BaseVertexIndex + 1);
}

//
//	FD3DRenderInterface::DrawTile
//

void FD3DRenderInterface::DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT W,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture3D* Texture,UBOOL AlphaBlend)
{
	check(!GD3DSceneRenderer);

	X = (X * Zoom2D) + Origin2D.X;
	Y = (Y * Zoom2D) + Origin2D.Y;
	SizeX *= Zoom2D;
	SizeY *= Zoom2D;

	// If it's not on the screen, skip it
	if( (X + (INT)SizeX) < 0 || (Y + (INT)SizeY) < 0 )
		return;

	SetBatchState(D3DPT_TRIANGLELIST,Texture ? (FD3DTexture*)GD3DRenderDevice->Resources.GetCachedResource(Texture) : NULL,AlphaBlend);

	FCycleCounterSection	CycleCounter(GD3DStats.TileRenderTime);
	GD3DStats.Tiles.Value++;

	INT					BaseVertexIndex = BatchedVertices.Add(4);
	FD3DCanvasVertex*	Vertices = &BatchedVertices(BaseVertexIndex);

	Vertices[0].Set(X,Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U,V,FColor(Color),W);
	Vertices[1].Set(X + SizeX,Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U + SizeU,V,FColor(Color),W);
	Vertices[2].Set(X,Y + SizeY,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U,V + SizeV,FColor(Color),W);
	Vertices[3].Set(X + SizeX,Y + SizeY,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U + SizeU,V + SizeV,FColor(Color),W);

	BatchedIndices.AddItem(BaseVertexIndex + 0);
	BatchedIndices.AddItem(BaseVertexIndex + 1);
	BatchedIndices.AddItem(BaseVertexIndex + 3);

	BatchedIndices.AddItem(BaseVertexIndex + 0);
	BatchedIndices.AddItem(BaseVertexIndex + 3);
	BatchedIndices.AddItem(BaseVertexIndex + 2);
}

//
//	FD3DRenderInterface::DrawTile
//

void FD3DRenderInterface::DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture2D* Texture,UBOOL AlphaBlend)
{
	check(!GD3DSceneRenderer);

	X = (X * Zoom2D) + Origin2D.X;
	Y = (Y * Zoom2D) + Origin2D.Y;
	SizeX *= Zoom2D;
	SizeY *= Zoom2D;

	// If it's not on the screen, skip it
	if( (X + (INT)SizeX) < 0 || (Y + (INT)SizeY) < 0 )
		return;

	SetBatchState(D3DPT_TRIANGLELIST,Texture ? (FD3DTexture*)GD3DRenderDevice->Resources.GetCachedResource(Texture) : NULL,AlphaBlend);

	FCycleCounterSection	CycleCounter(GD3DStats.TileRenderTime);
	GD3DStats.Tiles.Value++;

	INT					BaseVertexIndex = BatchedVertices.Add(4);
	FD3DCanvasVertex*	Vertices = &BatchedVertices(BaseVertexIndex);

	Vertices[0].Set(X,Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U,V,0.0f,FColor(Color));
	Vertices[1].Set(X + SizeX,Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U + SizeU,V,0.0f,FColor(Color));
	Vertices[2].Set(X,Y + SizeY,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U,V + SizeV,0.0f,FColor(Color));
	Vertices[3].Set(X + SizeX,Y + SizeY,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U + SizeU,V + SizeV,0.0f,FColor(Color));

	BatchedIndices.AddItem(BaseVertexIndex + 0);
	BatchedIndices.AddItem(BaseVertexIndex + 1);
	BatchedIndices.AddItem(BaseVertexIndex + 3);

	BatchedIndices.AddItem(BaseVertexIndex + 0);
	BatchedIndices.AddItem(BaseVertexIndex + 3);
	BatchedIndices.AddItem(BaseVertexIndex + 2);
}

//
//	FD3DRenderInterface::DrawTile
//

void FD3DRenderInterface::DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	check(!GD3DSceneRenderer);

	X = (X * Zoom2D) + Origin2D.X;
	Y = (Y * Zoom2D) + Origin2D.Y;
	SizeX *= Zoom2D;
	SizeY *= Zoom2D;

	// If it's not on the screen, skip it
	if( (X + (INT)SizeX) < 0 || (Y + (INT)SizeY) < 0 )
		return;
	
	FlushBatchedPrimitives();

	FCycleCounterSection	CycleCounter(GD3DStats.TileRenderTime);
	GD3DStats.Tiles.Value++;
	
	FD3DCanvasVertex	Vertices[4];

	Vertices[0].Set(X,Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U,V);
	Vertices[1].Set(X + SizeX,Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U + SizeU,V);
	Vertices[2].Set(X + SizeX,Y + SizeY,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U + SizeU,V + SizeV);
	Vertices[3].Set(X,Y + SizeY,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U,V + SizeV);

	if(HitTesting)
	{
		FD3DEmissiveTileShader*	CachedShader = GD3DRenderDevice->EmissiveTileHitProxyShaders.GetCachedShader(Material);
		CachedShader->Set(MaterialInstance);
	}
	else
	{
		FD3DEmissiveTileShader*	CachedShader = GD3DRenderDevice->EmissiveTileShaders.GetCachedShader(Material);
		CachedShader->Set(MaterialInstance);
		GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
		GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
		GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	}

	GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
	GDirect3DDevice->DrawPrimitiveUP(
		D3DPT_TRIANGLEFAN,
		2,
		Vertices,
		sizeof(FD3DCanvasVertex)
		);

	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
}

//
//	FD3DRenderInterface::DrawTriangle2D
//

void FD3DRenderInterface::DrawTriangle2D(
	const FIntPoint& Vert0, FLOAT U0, FLOAT V0, 
	const FIntPoint& Vert1, FLOAT U1, FLOAT V1, 
	const FIntPoint& Vert2, FLOAT U2, FLOAT V2,
	const FLinearColor& Color, FTexture2D* Texture, UBOOL AlphaBlend)
{
	check(!GD3DSceneRenderer);

	SetBatchState(D3DPT_TRIANGLELIST,Texture ? (FD3DTexture*)GD3DRenderDevice->Resources.GetCachedResource(Texture) : NULL,AlphaBlend);

	FCycleCounterSection	CycleCounter(GD3DStats.TileRenderTime);
	GD3DStats.Tiles.Value++;

	INT					BaseVertexIndex = BatchedVertices.Add(3);
	FD3DCanvasVertex*	Vertices = &BatchedVertices(BaseVertexIndex);

	Vertices[0].Set(Zoom2D * Vert0.X + Origin2D.X,Zoom2D * Vert0.Y + Origin2D.Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U0,V0,0.0f,Color);
	Vertices[1].Set(Zoom2D * Vert1.X + Origin2D.X,Zoom2D * Vert1.Y + Origin2D.Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U1,V1,0.0f,Color);
	Vertices[2].Set(Zoom2D * Vert2.X + Origin2D.X,Zoom2D * Vert2.Y + Origin2D.Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U2,V2,0.0f,Color);

	BatchedIndices.AddItem(BaseVertexIndex + 0);
	BatchedIndices.AddItem(BaseVertexIndex + 1);
	BatchedIndices.AddItem(BaseVertexIndex + 2);
}

//
//	FD3DRenderInterface::DrawTriangle2D
//

void FD3DRenderInterface::DrawTriangle2D(
	const FIntPoint& Vert0, FLOAT U0, FLOAT V0, 
	const FIntPoint& Vert1, FLOAT U1, FLOAT V1, 
	const FIntPoint& Vert2, FLOAT U2, FLOAT V2,
	FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	check(!GD3DSceneRenderer);

	FlushBatchedPrimitives();

	FCycleCounterSection	CycleCounter(GD3DStats.TileRenderTime);
	GD3DStats.Tiles.Value++;

	FD3DCanvasVertex	Vertices[3];

	Vertices[0].Set(Zoom2D * Vert0.X + Origin2D.X,Zoom2D * Vert0.Y + Origin2D.Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U0,V0);
	Vertices[1].Set(Zoom2D * Vert1.X + Origin2D.X,Zoom2D * Vert1.Y + Origin2D.Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U1,V1);
	Vertices[2].Set(Zoom2D * Vert2.X + Origin2D.X,Zoom2D * Vert2.Y + Origin2D.Y,0.0f,CurrentViewport->SizeX,CurrentViewport->SizeY,U2,V2);

	if(HitTesting)
	{
		FD3DEmissiveTileShader*	CachedShader = (FD3DEmissiveTileShader*)GD3DRenderDevice->EmissiveTileHitProxyShaders.GetCachedShader(Material);
		CachedShader->Set(MaterialInstance);
	}
	else
	{
		FD3DEmissiveTileShader*	CachedShader = (FD3DEmissiveTileShader*)GD3DRenderDevice->EmissiveTileShaders.GetCachedShader(Material);
		CachedShader->Set(MaterialInstance);
	}

	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
	GDirect3DDevice->DrawPrimitiveUP(
		D3DPT_TRIANGLEFAN,
		1,
		Vertices,
		sizeof(FD3DCanvasVertex)
		);

	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
}

//
//	FD3DRenderInterface::SetHitProxy
//

void FD3DRenderInterface::SetHitProxy(HHitProxy* HitProxy)
{
	if(HitTesting)
	{
		if(!GD3DSceneRenderer)
			FlushBatchedPrimitives();

		if(HitProxy)
		{
			check(GD3DRenderDevice->HitProxies.FindItemIndex(HitProxy) == INDEX_NONE);
			CurrentHitProxyIndex = GD3DRenderDevice->HitProxies.AddItem(HitProxy);
			HitProxy->Order = CurrentHitProxyIndex;
		}
		else
			CurrentHitProxyIndex = 0;
	}
	else
		delete HitProxy;

}

//
//	FD3DRenderInterface::Wait
//

void FD3DRenderInterface::Wait()
{
	FCycleCounterSection	WaitCycleCounter(GD3DStats.WaitingTime);
#ifndef XBOX
	if( !GD3DRenderDevice->DeviceLost )
	{
		// Wait for the video card to finish rendering.
		HRESULT	Result = GD3DRenderDevice->EventQuery->Issue(D3DISSUE_END);
		if(FAILED(Result))
			appErrorf(TEXT("Failed to issue event query: %s"),*D3DError(Result));

		BOOL	QueryResult = FALSE;
		while(1)
		{
			Result = GD3DRenderDevice->EventQuery->GetData(&QueryResult,sizeof(BOOL),D3DGETDATA_FLUSH);
			if(FAILED(Result))
			{
				if( Result == D3DERR_DEVICELOST )
				{
					GD3DRenderDevice->DeviceLost = 1;
					break;
				}
				else
					appErrorf(TEXT("Query::GetData failed: %s"),*D3DError(Result));
			}
			if(Result == S_OK && QueryResult)
				break;
		};
	}
#endif
}

//
//	FD3DRenderInterface::DrawScene
//

void FD3DRenderInterface::DrawScene(const FSceneContext& Context)
{
	check(!GD3DSceneRenderer);

	FlushBatchedPrimitives();

    FCycleCounterSection	SceneCycleCounter(GD3DStats.SceneRenderTime);

	GD3DSceneRenderer = new FD3DSceneRenderer(Context);

	if(HitTesting)
	{
		GD3DSceneRenderer->RenderHitProxies();
	}
	else if(Context.View->ShowFlags & SHOW_HitProxies)
	{
		TArray<HHitProxy*> SavedHitProxies = GD3DRenderDevice->HitProxies;
		HitTesting = 1;
		GD3DRenderDevice->HitProxies.Empty();

		GD3DSceneRenderer->RenderHitProxies();

		GD3DRenderDevice->HitProxies = SavedHitProxies;
		HitTesting = 0;
	}
	else
		GD3DSceneRenderer->Render();

	delete GD3DSceneRenderer;
	GD3DSceneRenderer = NULL;
}