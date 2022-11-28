/*=============================================================================
	D3DRenderDevice.h: Unreal Direct3D render device definition.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	FD3DViewport
//

struct FD3DViewport
{
	FChildViewport*					Viewport;
	UINT							SizeX,
									SizeY;

	// Constructor.

	FD3DViewport(FChildViewport* InViewport,UINT InSizeX,UINT InSizeY);
};

//
//	FD3DVertexDeclarationId
//

struct FD3DVertexDeclarationId
{
	TArray<D3DVERTEXELEMENT9>	Elements;

	// Equality operator.

	friend UBOOL operator==(const FD3DVertexDeclarationId& IdA,const FD3DVertexDeclarationId& IdB)
	{
		if(IdA.Elements.Num() != IdB.Elements.Num())
			return 0;
		for(UINT ElementIndex = 0;ElementIndex < (UINT)IdA.Elements.Num();ElementIndex++)
		{
			const D3DVERTEXELEMENT9&	ElementA = IdA.Elements(ElementIndex);
			const D3DVERTEXELEMENT9&	ElementB = IdB.Elements(ElementIndex);
			if(ElementA.Stream != ElementB.Stream)
				return 0;
			if(ElementA.Offset != ElementB.Offset)
				return 0;
			if(ElementA.Type != ElementB.Type)
				return 0;
			if(ElementA.Method != ElementB.Method)
				return 0;
			if(ElementA.Usage != ElementB.Usage)
				return 0;
			if(ElementA.UsageIndex != ElementB.UsageIndex)
				return 0;
		}
		return 1;
	}

	// GetTypeHash

	friend FORCEINLINE DWORD GetTypeHash(const FD3DVertexDeclarationId& Id)
	{
		DWORD	CRC = 0;
		for(UINT ElementIndex = 0;ElementIndex < (UINT)Id.Elements.Num();ElementIndex++)
			CRC = appMemCrc(&Id.Elements(ElementIndex),sizeof(D3DVERTEXELEMENT9),CRC);
		return CRC;
	}
};

//
//	FD3DCompiledShaderCode
//

struct FD3DCompiledShaderCode
{
	TArray<DWORD>	Code;
	UINT			NumInstructions;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FD3DCompiledShaderCode& S)
	{
		return Ar << S.Code << S.NumInstructions;
	}

	// Equality operator.

	friend UBOOL operator==(const FD3DCompiledShaderCode& CodeA,const FD3DCompiledShaderCode& CodeB)
	{
		if(CodeA.Code.Num() != CodeB.Code.Num())
			return 0;
		return !appMemcmp(&CodeA.Code(0),&CodeB.Code(0),CodeA.Code.Num() * sizeof(DWORD));
	}

	// GetTypeHash

	friend FORCEINLINE DWORD GetTypeHash(const FD3DCompiledShaderCode& C)
	{
		return appMemCrc(&C.Code(0),sizeof(DWORD) * C.Code.Num());
	}
};

//
//	FD3DCompiledShader
//

struct FD3DCompiledShader
{
	INT									VertexShaderCode,
										PixelShaderCode;
	TArray<FD3DUnmappedShaderParameter>	UnmappedParameters;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FD3DCompiledShader& S)
	{
		return Ar << S.VertexShaderCode << S.PixelShaderCode << S.UnmappedParameters;
	}
};

//
//	UD3DRenderDevice
//

class UD3DRenderDevice : public URenderDevice, FResourceClient, FPrecacheInterface
{
	DECLARE_CLASS(UD3DRenderDevice,URenderDevice,CLASS_Config,D3DDrv);
public:

	// Configuration.

	UBOOL					UseTrilinear,
							UseRefDevice,
							DisableFPBlending,
							DisableFPTranslucency,
							DisableFPFiltering,
							DisablePrecaching,
							DisableHWShadowMaps,
							DisableMultiLightPath,
							DisableShaderModel3,
							EnableOcclusionQuery;
	INT						AdapterNumber,
							LevelOfAnisotropy,
							MaxTranslucencyLayers;
	UBOOL					AccurateStats,
							UseStaticLighting;
	UINT					MinShadowResolution,
							MaxShadowResolution;
	FLOAT					ShadowResolutionScale;

	UBOOL					UsePostProcessEffects;
	INT						BlurBufferResolutionDivisor;
	FLOAT					BlurAttenuation,
							BlurAlpha;

	FStringNoInit			ShaderPath;

	// Render targets.

#ifndef XBOX
	FD3DRenderTarget*			LightingRenderTargets[2];
#else
	FD3DRenderTarget*			LightingRenderTarget;
#endif

	FD3DRenderTarget*			LightRenderTarget;
	FD3DRenderTarget*			BlurRenderTargets[2];
	FD3DRenderTarget*			ManualExposureRenderTarget;
	FD3DRenderTarget*			ShadowDepthRenderTarget;

	TD3DRef<IDirect3DTexture9>  HWShadowDepthBufferTexture;

	FD3DDepthStencilSurface*	ShadowDepthBuffer;
	
	UINT						BufferSizeX,
								BufferSizeY,
								BlurBufferSizeX,
								BlurBufferSizeY,
								CachedShadowBufferSize,
								CachedBlurBufferDivisor;

	// Cached shader source files and compiled shaders.

	TMap<FString,FString>										ShaderFileCache;

	TArray<TD3DRef<IDirect3DPixelShader9> >						PixelShaders;
	TArray<TD3DRef<IDirect3DVertexShader9> >					VertexShaders;
	TArray<FD3DCompiledShaderCode>								CompiledShaders;
	TMap<FD3DCompiledShaderCode,INT>							CompiledShaderMap;
	TMap<FD3DPersistentMeshLightShaderId,FD3DCompiledShader>	CompiledMeshLightShaders;
	TMap<FD3DPersistentMeshShaderId,FD3DCompiledShader>			CompiledMeshShaders;
	TMap<FGuid,FD3DCompiledShader>								CompiledEmissiveTileShaders;
	TMap<FGuid,FD3DCompiledShader>								CompiledEmissiveTileHitProxyShaders;

	TD3DRef<IDirect3DVertexShader9>	CurrentVertexShader;
	TD3DRef<IDirect3DPixelShader9>	CurrentPixelShader;

	// Vertex declaration cache.

	TMap<FD3DVertexDeclarationId,TD3DRef<IDirect3DVertexDeclaration9> >	CachedVertexDeclarations;
	TD3DRef<IDirect3DVertexDeclaration9> CanvasVertexDeclaration;

	// Resources.

	FD3DResourceCache			Resources;

	FD3DMeshLightShaderCache	MeshLightShaders;
	FD3DMeshShaderCache			MeshShaders;
	FD3DEmissiveTileShaderCache	EmissiveTileShaders;
	FD3DEmissiveTileShaderCache	EmissiveTileHitProxyShaders;
	FD3DBlurShaderCache			BlurShaders;

	FD3DResourceSet				MiscResources;	// Dynamic vertex/index buffers.

	TArrayNoInit<FD3DResourceSet*>	ResourceSets; // NoInit so the resource sets can add themselves to the list from the UD3DRenderDevice constructor without construct order problems.

	// Misc global shaders.

	TD3DGlobalShaderCache<FD3DSpriteShader,SF_Sprite>					SpriteShader;
	TD3DGlobalShaderCache<FD3DSpriteHitProxyShader,SF_SpriteHitProxy>	SpriteHitProxyShader;
	TD3DGlobalShaderCache<FD3DAccumulateImageShader,SF_AccumulateImage>	AccumulateImageShader;
	TD3DGlobalShaderCache<FD3DBlendImageShader,SF_BlendImage>			BlendImageShader;
	TD3DGlobalShaderCache<FD3DFinishImageShader,SF_FinishImage>			FinishImageShader;
	TD3DGlobalShaderCache<FD3DFogShader,SF_Fog>							FogShader;
	TD3DGlobalShaderCache<FD3DShadowVolumeShader,SF_ShadowVolume>		ShadowVolumeShader;
	TD3DGlobalShaderCache<FD3DExposureShader,SF_Exposure>				ExposureShader;
	TD3DGlobalShaderCache<FD3DFillShader,SF_Fill>						FillShader;
	TD3DGlobalShaderCache<FD3DCopyShader,SF_Copy>						CopyShader;
	TD3DGlobalShaderCache<FD3DCopyFillShader,SF_CopyFill>				CopyFillShader;
	TD3DGlobalShaderCache<FD3DOcclusionQueryShader,SF_OcclusionQuery>	OcclusionQueryShader;
	TD3DGlobalShaderCache<FD3DTileShader,SF_Tile>						TileShader;
	TD3DGlobalShaderCache<FD3DTileShader,SF_TileHitProxy>				TileHitProxyShader;
	TD3DGlobalShaderCache<FD3DTileShader,SF_SolidTile>					SolidTileShader;
	TD3DGlobalShaderCache<FD3DTileShader,SF_SolidTileHitProxy>			SolidTileHitProxyShader;

	// Dynamic vertex/index buffers.

	FD3DDynamicIndexBuffer*		DynamicIndexBuffer16;
	FD3DDynamicIndexBuffer*		DynamicIndexBuffer32;

	// Hit detection.

	TD3DRef<IDirect3DSurface9>	HitProxyBuffer;
	UINT						HitProxyBufferSizeX,
								HitProxyBufferSizeY;
	TArray<HHitProxy*>			HitProxies;
	FChildViewport*				CachedHitProxyViewport;

	// Direct3D interfaces.

	TD3DRef<IDirect3D9>			Direct3D;
	TD3DRef<IDirect3DSurface9>	BackBuffer,
								DepthBuffer;
	TD3DRef<IDirect3DQuery9>	EventQuery;

	// Variables.

	TArray<FD3DViewport*>	Viewports;
	FD3DViewport*			FullscreenViewport;
	UINT					DeviceSizeX,
							DeviceSizeY;
	UBOOL					DeviceFullscreen;
	HWND					DeviceWindow;

	D3DCAPS9				DeviceCaps;
	UBOOL					DeviceLost;

	UBOOL					Supports16bitRT,
							SupportsFPBlending,
							SupportsFPFiltering,
							SupportsHWShadowMaps,
							SupportsShaderModel3,
							SupportsMultiLightPath;

	UBOOL					UseFPBlending,
							UseFPTranslucency,
							UseFPFiltering,
							UseHWShadowMaps,
							UseShaderModel3,
							UseMultiLightPath;

	UBOOL					DumpRenderTargets,
							DumpHitProxies;

	// Constructor.

	UD3DRenderDevice();

	// GetViewportIndex

	INT GetViewportIndex(FChildViewport* Viewport)
	{
		for(INT ViewportIndex = 0;ViewportIndex < Viewports.Num();ViewportIndex++)
			if(Viewports(ViewportIndex)->Viewport == Viewport)
				return ViewportIndex;
		return INDEX_NONE;
	}

	// CleanupDevice

	void CleanupDevice();

	// InitDevice

	void InitDevice();

	// CheckDeviceLost

	UBOOL CheckDeviceLost();

	// SerializeCompiledShaders

	void SerializeCompiledShaders(FArchive& Ar);

	// GetShaderCodeIndex

	INT GetShaderCodeIndex(void* CodeBuffer,UINT CodeSize);

	// SetVertexShader

	void SetVertexShader(const TD3DRef<IDirect3DVertexShader9>& VertexShader);

	// SetPixelShader

	void SetPixelShader(const TD3DRef<IDirect3DPixelShader9>& PixelShader);

	// Precache

	void Precache(FD3DViewport* Viewport);

	// CacheBuffers

	void CacheBuffers(UINT MinSizeX,UINT MinSizeY);

	// AllocateBuffers

	void AllocateBuffers(UINT MinSizeX,UINT MinSizeY);

	// DumpRenderTarget

	void DumpRenderTarget(const TD3DRef<IDirect3DSurface9>& RenderTarget,TArray<FColor>& OutputData,UINT& OutputSizeX,UINT& OutputSizeY);

	// SaveRenderTarget

	void SaveRenderTarget(const TD3DRef<IDirect3DSurface9>& RenderTarget,const TCHAR* BaseFilename,UBOOL Alpha = 0);

	// GetDynamicIndexBuffer

	FD3DDynamicIndexBuffer* GetDynamicIndexBuffer(UINT MinSize,UINT Stride);

	// GetShaderSource

	FString GetShaderSource(const TCHAR* Filename);

	// GetBufferMemorySize

	UINT GetBufferMemorySize();

	// CreateVertexDeclaration

	IDirect3DVertexDeclaration9* CreateVertexDeclaration(const TArray<D3DVERTEXELEMENT9>& Elements);

	// GetCanvasVertexDeclaration

	IDirect3DVertexDeclaration9* GetCanvasVertexDeclaration();

	// UpdateRenderOptions

	void UpdateRenderOptions();

	// InvalidateHitProxyCache

	void InvalidateHitProxyCache();

	// FResourceClient interface.

	virtual void FreeResource(FResource* Resource);
	virtual void UpdateResource(FResource* Resource);
	
	/**
	 * Request an increase or decrease in the amount of miplevels a texture uses.
	 *
	 * @param	Texture				texture to adjust
	 * @param	RequestedMips		miplevels the texture should have
	 * @return	NULL if a decrease is requested, pointer to information about request otherwise (NOTE: pointer gets freed in FinalizeMipRequest)
	 */
	virtual void RequestMips( FTextureBase* Texture, UINT RequestedMips, FTextureMipRequest* TextureMipRequest );

	/**
	 * Finalizes a mip request. This gets called after all requested data has finished loading.	
	 *
	 * @param	TextureMipRequest	Mip request that is being finalized.
	 */
	virtual void FinalizeMipRequest( FTextureMipRequest* TextureMipRequest );

	// UObject interface.

	void StaticConstructor();
	virtual void Destroy();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void Serialize(FArchive& Ar);

	// FExec interface.

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	// URenderDevice interface.

	virtual void Init();
	virtual void Flush();

	virtual void CreateViewport(FChildViewport* Viewport);
	virtual void ResizeViewport(FChildViewport* Viewport);
	virtual void DestroyViewport(FChildViewport* Viewport);

	virtual void DrawViewport(FChildViewport* Viewport,UBOOL Synchronous);
	virtual void ReadPixels(FChildViewport* Viewport,FColor* OutputBuffer);

	virtual void GetHitProxyMap(FChildViewport* Viewport,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<HHitProxy*>& OutMap);
	virtual void InvalidateHitProxies(FChildViewport* Viewport);

	virtual TArray<FMaterialError> CompileMaterial(FMaterial* Material);

	// FPrecacheInterface interface.

	virtual void CacheResource(FResource* Resource);
};

#define AUTO_INITIALIZE_REGISTRANTS_D3DDRV UD3DRenderDevice::StaticClass();
