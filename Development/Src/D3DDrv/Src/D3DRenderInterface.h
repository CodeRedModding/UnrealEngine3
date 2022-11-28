/*=============================================================================
	D3DRenderInterface.h: Unreal Direct3D rendering interface definition.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	FD3DCanvasVertex
//

struct FD3DCanvasVertex
{
	FPlane	Position;
	FLOAT	U,
			V,
			W;
	FColor	Color;

	/**
	 * Initialize the vertex.
	 */
	void Set(FLOAT X,FLOAT Y,FLOAT Z,UINT RenderTargetSizeX,UINT RenderTargetSizeY,FLOAT InU,FLOAT InV,FLOAT InW = 0.0f,FColor InColor = FColor(255,255,255))
	{
		Position = FPlane(
			-1.0f + (X - 0.5) / ((FLOAT)RenderTargetSizeX / 2.0f),
			+1.0f - (Y - 0.5) / ((FLOAT)RenderTargetSizeY / 2.0f),
			Z,
			1.0f
			);
		Color = InColor;
		U = InU;
		V = InV;
		W = InW;
	}
};

//
//	FD3DRenderInterface
//

struct FD3DRenderInterface : public FRenderInterface
{
	struct FD3DViewport*		CurrentViewport;
	UBOOL						HitTesting;
	DWORD						CurrentHitProxyIndex;

	TArray<FD3DCanvasVertex>	BatchedVertices;
	TArray<_WORD>				BatchedIndices;
	D3DPRIMITIVETYPE			BatchedPrimitiveType;
	FD3DTexture*				BatchedTexture;
	UBOOL						BatchedAlphaBlend;

#ifdef XBOX
	/**
	 * Struct holding necessary information to clean up a resource.	
	 */
	struct FXenonResource
	{
		/** Base address of physical allocation */
		void*					BaseAddress;
		/** D3D header of resource				*/
		IDirect3DResource9*		Resource;

		/** 
		 * Constructor
		 *
		 * @param InBaseAddress		physical base address of resource
		 * @param InResource		D3D header of resource
		 */
		FXenonResource( void* InBaseAddress, IDirect3DResource9* InResource )
		:	BaseAddress( InBaseAddress ),
			Resource( InResource )
		{}
	};
	/** Used for deferred freeing of resources */
	TArray<FXenonResource>		FreedResources;

	/**
	 * DeferredFreeResources
	 *
	 * Physically frees resources that have been marked for deletion over the course of the frame and being kept track 
	 * of in FreedResources. This is an intermediate solution to get better performance on implementations of the platform 
	 * specific resource handling that require waiting for the HW before freeing a resource. Ideally we shouldn't be 
	 * allocating and destroying resources on a per frame basis.
	 *
	 * @todo xenon: make this obsolete!
	 */
	void DeferredFreeResources();
#endif

	// Constructor.

	FD3DRenderInterface():
		FRenderInterface(),
		CurrentViewport(NULL)
	{}

	// BeginScene

	void BeginScene(FD3DViewport* Viewport,UBOOL InHitTesting = 0);

	// EndScene

	void EndScene(UBOOL Synchronous);

	// Wait

	void Wait();

	// SetViewport

	void SetViewport(UINT X,UINT Y,UINT Width,UINT Height);

	// DrawQuad

	void DrawQuad(FLOAT X,FLOAT Y,INT XL,INT YL,FLOAT U = 0.0f,FLOAT V = 0.0f,FLOAT UL = 1.0f,FLOAT VL = 1.0f,FLOAT Z = 0.0f);

	// FillSurface

	void FillSurface(const TD3DRef<IDirect3DSurface9>& Surface,FLOAT X = 0,FLOAT Y = 0,INT XL = -1,INT YL = -1,FLOAT U = 0.0f,FLOAT V = 0.0f,FLOAT UL = 1.0f,FLOAT VL = 1.0f,UBOOL Add = 0);

	// FlushBatchedPrimitives - Render any batched primitives.

	void FlushBatchedPrimitives();

	// SetBatchState - Sets the batch state to the given values, flushing batched primitives if the previous state differs.

	void SetBatchState(D3DPRIMITIVETYPE PrimitiveType,FD3DTexture* Texture,UBOOL AlphaBlend);

	// FRenderInterface interface.

	virtual void Clear(const FColor& Color);

	virtual void DrawScene(const FSceneContext& Context);

	virtual void DrawLine2D(INT X1,INT Y1,INT X2,INT Y2,FColor Color);

	virtual void DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT W,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture3D* Texture = NULL,UBOOL AlphaBlend = 1);
	virtual void DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,const FLinearColor& Color,FTexture2D* Texture = NULL,UBOOL AlphaBlend = 1);
	virtual void DrawTile(INT X,INT Y,UINT SizeX,UINT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FMaterial* Material,FMaterialInstance* MaterialInstance);

	virtual void DrawTriangle2D(
		const FIntPoint& Vert0, FLOAT U0, FLOAT V0, 
		const FIntPoint& Vert1, FLOAT U1, FLOAT V1, 
		const FIntPoint& Vert2, FLOAT U2, FLOAT V2,
		const FLinearColor& Color, FTexture2D* Texture = NULL, UBOOL AlphaBlend = 1);
	virtual void DrawTriangle2D(
		const FIntPoint& Vert0, FLOAT U0, FLOAT V0, 
		const FIntPoint& Vert1, FLOAT U1, FLOAT V1, 
		const FIntPoint& Vert2, FLOAT U2, FLOAT V2,
		FMaterial* Material,FMaterialInstance* MaterialInstance);

	virtual void SetHitProxy(HHitProxy* Hit);
	virtual UBOOL IsHitTesting() { return HitTesting; }
};
