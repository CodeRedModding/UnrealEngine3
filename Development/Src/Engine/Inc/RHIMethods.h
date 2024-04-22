/*=============================================================================
	RHIMETHOD_SPECIFIERSs.h: The RHI method definitions.  The same methods are defined multiple places, so they're simply included from this file where necessary.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// DEFINE_RHIMETHOD is used by the includer to modify the RHI method definitions.
// It's defined by the dynamic RHI to pass parameters from the statically bound RHI method to the dynamically bound RHI method.
// To enable that, the parameter list must be given a second time, as they will be passed to the dynamically bound RHI method.
// The last parameter should be return if the method returns a value, and nothing otherwise.
#ifndef DEFINE_RHIMETHOD
#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) Type Name Parameters
#endif

//
// RHI resource management functions.
//

DEFINE_RHIMETHOD(
	FSamplerStateRHIRef,
	CreateSamplerState,
	(const FSamplerStateInitializerRHI& Initializer),(Initializer),
	return,
	return new TNullRHIResource<RRT_SamplerState>();
	);
DEFINE_RHIMETHOD(
	FRasterizerStateRHIRef,
	CreateRasterizerState,
	(const FRasterizerStateInitializerRHI& Initializer),
	(Initializer),
	return,
	return new TNullRHIResource<RRT_RasterizerState>();
	);
DEFINE_RHIMETHOD(
	FDepthStateRHIRef,
	CreateDepthState,
	(const FDepthStateInitializerRHI& Initializer),
	(Initializer),
	return,
	return new TNullRHIResource<RRT_DepthState>();
	);
DEFINE_RHIMETHOD(
	FStencilStateRHIRef,
	CreateStencilState,
	(const FStencilStateInitializerRHI& Initializer),
	(Initializer),
	return,
	return new TNullRHIResource<RRT_StencilState>();
	);
DEFINE_RHIMETHOD(
	FBlendStateRHIRef,
	CreateBlendState,
	(const FBlendStateInitializerRHI& Initializer),
	(Initializer),
	return,
	return new TNullRHIResource<RRT_BlendState>();
	);
// more extensive method than CreateBlendState to create a blend state (MRT + Alpha2Coverage)
DEFINE_RHIMETHOD(
	FBlendStateRHIRef,
	CreateMRTBlendState,
	(const FMRTBlendStateInitializerRHI& Initializer),
	(Initializer),
	return,
	return new TNullRHIResource<RRT_BlendState>();
	);

DEFINE_RHIMETHOD(
	FVertexDeclarationRHIRef,
	CreateVertexDeclaration,
	(const FVertexDeclarationElementList& Elements),
	(Elements),
	return,
	return new TNullRHIResource<RRT_VertexDeclaration>();
	);

DEFINE_RHIMETHOD(
	FVertexDeclarationRHIRef,
	CreateVertexDeclaration,
	(const FVertexDeclarationElementList& Elements, FName DeclName),
	(Elements, DeclName),
	return,
	return new TNullRHIResource<RRT_VertexDeclaration>();
);

DEFINE_RHIMETHOD(FPixelShaderRHIRef,CreatePixelShader,(const TArray<BYTE>& Code),(Code),return,return new TNullRHIResource<RRT_PixelShader>(););
DEFINE_RHIMETHOD(FVertexShaderRHIRef,CreateVertexShader,(const TArray<BYTE>& Code),(Code),return,return new TNullRHIResource<RRT_VertexShader>(););

#if WITH_D3D11_TESSELLATION
DEFINE_RHIMETHOD(FHullShaderRHIRef,CreateHullShader,(const TArray<BYTE>& Code),(Code),return,return new TNullRHIResource<RRT_HullShader>(););
DEFINE_RHIMETHOD(FDomainShaderRHIRef,CreateDomainShader,(const TArray<BYTE>& Code),(Code),return,return new TNullRHIResource<RRT_DomainShader>(););

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, hull shader, domain shader and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param HullShader - existing hull shader
 * @param DomainShader - existing domain shader
 * @param GeometryShader - existing geometry shader
 * @param PixelShader - existing pixel shader
 * @param MobileGlobalShaderType - global shader type to use for mobile
 */
DEFINE_RHIMETHOD(
	FBoundShaderStateRHIRef,
	CreateBoundShaderStateD3D11,
	(FVertexDeclarationRHIParamRef VertexDeclaration, DWORD *StreamStrides, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader, EMobileGlobalShaderType MobileGlobalShaderType),
	(VertexDeclaration, StreamStrides, VertexShader, HullShader, DomainShader, PixelShader, GeometryShader, MobileGlobalShaderType),
	return,
	return new TNullRHIResource<RRT_BoundShaderState>();
	);

DEFINE_RHIMETHOD(
	void,
	SetDomainShaderParameter,
	(FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	(DomainShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	,
	);

DEFINE_RHIMETHOD(
	void,
	SetHullShaderParameter,
	(FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	(HullShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	,
	);

DEFINE_RHIMETHOD(
	FGeometryShaderRHIRef,
	CreateGeometryShader,
	(const TArray<BYTE>& Code),
	(Code),
	return,
	return new TNullRHIResource<RRT_GeometryShader>();
	);

DEFINE_RHIMETHOD(
	FComputeShaderRHIRef,
	CreateComputeShader,
	(const TArray<BYTE>& Code),
	(Code),
	return,
	return new TNullRHIResource<RRT_ComputeShader>();
	);

DEFINE_RHIMETHOD(
	void,
	DispatchComputeShader,
	(FComputeShaderRHIParamRef ComputeShader, UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ),
	(ComputeShader, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ),
	,
	);
#endif

// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
// @param Count >0
// @param Data must not be 0
DEFINE_RHIMETHOD(
	void,
	SetMultipleViewports,
	(UINT Count, FViewPortBounds* Data),
	(Count, Data),
	,
	);

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param PixelShader - existing pixel shader
 * @param MobileGlobalShaderType - global shader type to use for mobile
 */
DEFINE_RHIMETHOD(
	FBoundShaderStateRHIRef,
	CreateBoundShaderState,
	(FVertexDeclarationRHIParamRef VertexDeclaration, DWORD *StreamStrides, FVertexShaderRHIParamRef VertexShader, FPixelShaderRHIParamRef PixelShader, EMobileGlobalShaderType MobileGlobalShaderType),
	(VertexDeclaration,StreamStrides,VertexShader,PixelShader,MobileGlobalShaderType),
	return,
	return new TNullRHIResource<RRT_BoundShaderState>();
	);

DEFINE_RHIMETHOD(
	FIndexBufferRHIRef,
	CreateIndexBuffer,
	(UINT Stride,UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage),
	(Stride,Size,ResourceArray,InUsage),
	return,
	if (ResourceArray) { ResourceArray->Discard(); } return new TNullRHIResource<RRT_IndexBuffer>();
	);

DEFINE_RHIMETHOD(
	FIndexBufferRHIRef,
	CreateInstancedIndexBuffer,
	(UINT Stride,UINT Size,DWORD InUsage,UINT PreallocateInstanceCount,UINT& OutNumInstances),
	(Stride,Size,InUsage,PreallocateInstanceCount,OutNumInstances),
	return,
	OutNumInstances = 1; return new TNullRHIResource<RRT_IndexBuffer>();
	);

/**
 * Create index buffer pointing to same memory as a vertex buffer
 * Not supported on all platforms
 */
DEFINE_RHIMETHOD(
	FIndexBufferRHIRef,
	CreateAliasedIndexBuffer,
	(FVertexBufferRHIParamRef InBuffer, UINT Stride),
	(InBuffer, Stride),
	return,
	return new TNullRHIResource<RRT_IndexBuffer>();
	);

DEFINE_RHIMETHOD(
	void*,
	LockIndexBuffer,
	(FIndexBufferRHIParamRef IndexBuffer,UINT Offset,UINT Size),
	(IndexBuffer,Offset,Size),
	return,
	return GetStaticBuffer();
	);
DEFINE_RHIMETHOD(void,UnlockIndexBuffer,(FIndexBufferRHIParamRef IndexBuffer),(IndexBuffer),,);

/**
 * @param ResourceArray - An optional pointer to a resource array containing the resource's data.
 */
DEFINE_RHIMETHOD(
	FVertexBufferRHIRef,
	CreateVertexBuffer,
	(UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage),
	(Size,ResourceArray,InUsage),
	return,
	if (ResourceArray) { ResourceArray->Discard(); } return new TNullRHIResource<RRT_VertexBuffer>();
	);

DEFINE_RHIMETHOD(
	void*,
	LockVertexBuffer,
	(FVertexBufferRHIParamRef VertexBuffer,UINT Offset,UINT SizeRHI,UBOOL bReadOnlyInsteadOfWriteOnly),
	(VertexBuffer,Offset,SizeRHI,bReadOnlyInsteadOfWriteOnly),
	return,
	return GetStaticBuffer();
	);
DEFINE_RHIMETHOD(void,UnlockVertexBuffer,(FVertexBufferRHIParamRef VertexBuffer),(VertexBuffer),,);

/**
 * Retrieves texture memory stats.
 *
 * @param	OutAllocatedMemorySize	[out]	Size of allocated memory, in bytes
 * @param	OutAvailableMemorySize	[out]	Size of available memory, in bytes
 * @param	OutPendingMemoryAdjustment	[out]	Upcoming adjustments to allocated memory, in bytes (async reallocations)
 *
 * @return TRUE if supported, FALSE otherwise
 */
DEFINE_RHIMETHOD(UBOOL,GetTextureMemoryStats,( INT& AllocatedMemorySize, INT& AvailableMemorySize, INT& OutPendingMemoryAdjustment ),(AllocatedMemorySize,AvailableMemorySize,OutPendingMemoryAdjustment),return,return FALSE);

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return TRUE if successful, FALSE otherwise
 */
DEFINE_RHIMETHOD(UBOOL,GetTextureMemoryVisualizeData,( FColor* TextureData, INT SizeX, INT SizeY, INT Pitch, INT PixelSize ),(TextureData,SizeX,SizeY,Pitch,PixelSize),return,return FALSE);

/**
* Creates a 2D RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
#if FLASH
DEFINE_RHIMETHOD(
	FTexture2DRHIRef,
	CreateFlashTexture2D,
	(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,void* Mip0, UINT BulkDataSize),
	(SizeX,SizeY,Format,NumMips,Flags,Mip0,BulkDataSize),
	return,
	return new TNullRHIResource<RRT_Texture2D>();
	);
#endif

DEFINE_RHIMETHOD(
	FTexture2DRHIRef,
	CreateTexture2D,
	(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData),
	(SizeX,SizeY,Format,NumMips,Flags,BulkData),
	return,
	return new TNullRHIResource<RRT_Texture2D>();
	);

#if XBOX
/**
 * Creates a 2D RHI texture resource, from a specific location if the platform supports it.
 * @param SizeX - width of the texture to create
 * @param SizeY - height of the texture to create
 * @param Format - EPixelFormat texture format
 * @param Flags - ETextureCreateFlags creation flags
 * @param Location - Memory location in which the texture data resides
 */
DEFINE_RHIMETHOD(
	FTexture2DRHIRef,
	CreateTexture2DExplicit,
	(UINT SizeX,UINT SizeY,BYTE Format,DWORD Flags,UPTRINT Location),
	(SizeX,SizeY,Format,Flags,Location),
	return,
	return new TNullRHIResource<RRT_Texture2D>();
	);
#endif


#if XBOX || PLATFORM_SUPPORTS_D3D10_PLUS
/**
* Creates a Array RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param SizeZ - depth of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
DEFINE_RHIMETHOD(
	 FTexture2DArrayRHIRef,
	 CreateTexture2DArray,
	 (UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData),
	 (SizeX,SizeY,SizeZ,Format,NumMips,Flags,BulkData),
	 return,
	 return new TNullRHIResource<RRT_Texture2DArray>();
);

#endif

#if PLATFORM_SUPPORTS_D3D10_PLUS

/**
* Creates a 3d RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param SizeZ - depth of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
* @param Data - Data to initialize the texture with
*/
DEFINE_RHIMETHOD(
	FTexture3DRHIRef,
	CreateTexture3D,
	(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,DWORD Flags,const BYTE* Data),
	(SizeX,SizeY,SizeZ,Format,NumMips,Flags,Data),
	return,
	return new TNullRHIResource<RRT_Texture3D>();
	);

/**
* Generates mip maps for a surface.
*/
DEFINE_RHIMETHOD(
	void,
	GenerateMips,
	(FSurfaceRHIParamRef Surface),
	(Surface),
	return,
	return;
	);

#endif

/**
 * Tries to reallocate the texture without relocation. Returns a new valid reference to the resource if successful.
 * Both the old and new reference refer to the same texture (at least the shared mip-levels) and both can be used or released independently.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @return				- New reference to the updated texture, or an invalid reference upon failure
 */
DEFINE_RHIMETHOD(
	FTexture2DRHIRef,
	ReallocateTexture2D,
	(FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY),
	(Texture2D, NewMipCount, NewSizeX, NewSizeY),
	return,
	return new TNullRHIResource<RRT_Texture2D>();
	);

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
DEFINE_RHIMETHOD(
	UINT,
	GetTextureSize,
	(FTexture2DRHIParamRef TextureRHI),
	(TextureRHI),
	return,
	return 0;
	);

#if XBOX
/**
 * Computes the size in memory required by a texture.
 *
 * @param SizeX - width of the texture
 * @param SizeY - height of the texture
 * @param Format - EPixelFormat texture format
 * @param NumMips  - number of mips or 0 for full mip pyramid
 * @param Flags - ETextureCreateFlags creation flags
 * @return					- Size in Bytes
 */
DEFINE_RHIMETHOD(
	UINT,
	GetTextureSize,
    (UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,DWORD Flags),
    (SizeX,SizeY,Format,NumMips,Flags),
	return,
	return 0;
	);

/**
 * Returns the base address of the texture
 *
 * @param	TextureRHI		- Texture we want to know the base of
 * @return					- Base address of the texture
 */
DEFINE_RHIMETHOD(
	void*,
	GetTextureBase,
	(FTexture2DRHIParamRef TextureRHI),
	(TextureRHI),
	return,
	return NULL;
	);
#endif

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIFinalizeAsyncReallocateTexture2D() must be called to complete the reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
DEFINE_RHIMETHOD(
	FTexture2DRHIRef,
	AsyncReallocateTexture2D,
	(FTexture2DRHIParamRef Texture2D, INT NewMipCount, INT NewSizeX, INT NewSizeY, FThreadSafeCounter* RequestStatus),
	(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus),
	return,
	RequestStatus->Decrement(); return new TNullRHIResource<RRT_Texture2D>();
	);

/**
 * Finalizes an async reallocation request.
 * If bBlockUntilCompleted is FALSE, it will only poll the status and finalize if the reallocation has completed.
 *
 * @param Texture2D				- Texture to finalize the reallocation for
 * @param bBlockUntilCompleted	- Whether the function should block until the reallocation has completed
 * @return						- Current reallocation status:
 *	TexRealloc_Succeeded	Reallocation succeeded
 *	TexRealloc_Failed		Reallocation failed
 *	TexRealloc_InProgress	Reallocation is still in progress, try again later
 */
DEFINE_RHIMETHOD(
	ETextureReallocationStatus,
	FinalizeAsyncReallocateTexture2D,
	(FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted),
	(Texture2D, bBlockUntilCompleted),
	return,
	return TexRealloc_Succeeded;
	);

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If TRUE, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
DEFINE_RHIMETHOD(
	ETextureReallocationStatus,
	CancelAsyncReallocateTexture2D,
	(FTexture2DRHIParamRef Texture2D, UBOOL bBlockUntilCompleted),
	(Texture2D, bBlockUntilCompleted),
	return,
	return TexRealloc_Succeeded;
);


/**
* Locks an RHI texture's mip surface for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - mip level index for the surface to retrieve
* @param bIsDataBeingWrittenTo - used to affect the lock flags 
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
DEFINE_RHIMETHOD(
	void*,
	LockTexture2D,
	(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail),
	(Texture,MipIndex,bIsDataBeingWrittenTo,DestStride,bLockWithinMiptail),
	return,
	DestStride = 0; return GetStaticBuffer()
	);

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
DEFINE_RHIMETHOD(
	void,
	UnlockTexture2D,
	(FTexture2DRHIParamRef Texture,UINT MipIndex,UBOOL bLockWithinMiptail),
	(Texture,MipIndex,bLockWithinMiptail),
	,
	);

#if PLATFORM_SUPPORTS_D3D10_PLUS

/**
* Locks an RHI texture's mip surface for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - mip level index for the surface to retrieve
* @param bIsDataBeingWrittenTo - used to affect the lock flags 
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
DEFINE_RHIMETHOD(
	void*,
	LockTexture2DArray,
	(FTexture2DArrayRHIParamRef Texture,UINT TextureIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail),
	(Texture,TextureIndex,MipIndex,bIsDataBeingWrittenTo,DestStride,bLockWithinMiptail),
	return,
	DestStride = 0; return GetStaticBuffer()
	);

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
DEFINE_RHIMETHOD(
	void,
	UnlockTexture2DArray,
	(FTexture2DArrayRHIParamRef Texture,UINT TextureIndex,UINT MipIndex,UBOOL bLockWithinMiptail),
	(Texture,TextureIndex,MipIndex,bLockWithinMiptail),
	,
	);

#endif

/**
* Updates a region of a 2D texture from system memory
* @param Texture - the RHI texture resource to update
* @param MipIndex - mip level index to be modified
* @param n - number of rectangles to copy
* @param rects - rectangles to copy from source image data
* @param pitch - size in bytes of each line of source image
* @param sbpp - size in bytes of each pixel of source image (must match texture, passed in because some drivers do not maintain it in refs)
* @param psrc - source image data (in same pixel format as texture)
* @return TRUE if supported, FALSE otherwise
*/
DEFINE_RHIMETHOD(
    UBOOL,
    UpdateTexture2D,
    (FTexture2DRHIParamRef Texture,UINT MipIndex,UINT n,const struct FUpdateTextureRegion2D* rects,UINT pitch,UINT sbpp,BYTE* psrc),
    (Texture,MipIndex,n,rects,pitch,sbpp,psrc),
    return,
    return FALSE;
    );

/**
 *	Returns the resolve target of a surface.
 *	@param SurfaceRHI	- Surface from which to get the resolve target
 *	@return				- Resolve target texture associated with the surface
 */
DEFINE_RHIMETHOD(
	FTexture2DRHIRef,
	GetResolveTarget,
	(FSurfaceRHIParamRef SurfaceRHI),
	(SurfaceRHI),
	return,
	return FTexture2DRHIRef()
	);

/**
* For platforms that support packed miptails return the first mip level which is packed in the mip tail
* @return mip level for mip tail or -1 if mip tails are not used
*/
DEFINE_RHIMETHOD(INT,GetMipTailIdx,(FTexture2DRHIParamRef Texture),(Texture),return,return INDEX_NONE);

/**
 * Copies a region within the same mip levels of one texture to another.  An optional region can be speci
 * Note that the textures must be the same size and of the same format.
 * @param DstTexture - destination texture resource to copy to
 * @param MipIdx - mip level for the surface to copy from/to. This mip level should be valid for both source/destination textures
 * @param BaseSizeX - width of the texture (base level). Same for both source/destination textures
 * @param BaseSizeY - height of the texture (base level). Same for both source/destination textures 
 * @param Format - format of the texture. Same for both source/destination textures
 * @param Region - list of regions to specify rects and source textures for the copy
 */
DEFINE_RHIMETHOD(
	void,
	CopyTexture2D,
	(FTexture2DRHIParamRef DstTexture, UINT MipIdx, INT BaseSizeX, INT BaseSizeY, INT Format, const TArray<struct FCopyTextureRegion2D>& Regions),
	(DstTexture,MipIdx,BaseSizeX,BaseSizeY,Format,Regions),
	,
	);

/**
 * Copies texture data from one mip to another
 * Note that the mips must be the same size and of the same format.
 * @param SrcTexture Source texture to copy from
 * @param SrcMipIndex Mip index into the source texture to copy data from
 * @param DestTexture Destination texture to copy to
 * @param DestMipIndex Mip index in the destination texture to copy to - note this is probably different from source mip index if the base widths/heights are different
 * @param Size Size of mip data
 * @param Counter Thread safe counter used to flag end of transfer
 */
DEFINE_RHIMETHOD(
	void,
	CopyMipToMipAsync,
	(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex, INT Size, FThreadSafeCounter& Counter),
	(SrcTexture,SrcMipIndex,DestTexture,DestMipIndex,Size,Counter),
	,
	);

/**
 * Copies mip data from one location to another, selectively copying only used memory based on
 * the texture tiling memory layout of the given mip level
 * Note that the mips must be the same size and of the same format.
 * @param Texture - texture to base memory layout on
 * @param Src - source memory base address to copy from
 * @param Dst - destination memory base address to copy to
 * @param MemSize - total size of mip memory
 * @param MipIdx - mip index to base memory layout on
 */
DEFINE_RHIMETHOD(
	void,
	SelectiveCopyMipData,
	(FTexture2DRHIParamRef Texture, BYTE *Src, BYTE *Dst, UINT MemSize, UINT MipIdx),
	(Texture,Src,Dst,MemSize,MipIdx),
	return,
	appMemcpy(Dst, Src, MemSize);
	);

/**
 * Finalizes an asynchronous mip-to-mip copy.
 * This must be called once asynchronous copy has signaled completion by decrementing the counter passed to CopyMipToMipAsync.
 * @param SrcText Source texture to copy from
 * @param SrcMipIndex Mip index into the source texture to copy data from
 * @param DestText Destination texture to copy to
 * @param DestMipIndex Mip index in the destination texture to copy to - note this is probably different from source mip index if the base widths/heights are different
 */
DEFINE_RHIMETHOD(
	void,
	FinalizeAsyncMipCopy,
	(FTexture2DRHIParamRef SrcTexture, INT SrcMipIndex, FTexture2DRHIParamRef DestTexture, INT DestMipIndex),
	(SrcTexture,SrcMipIndex,DestTexture,DestMipIndex),
	,
	);

/**
 * Create resource memory to be shared by multiple RHI resources.
 * Allocations from GPUMem_TexturePool may fail due to OOM, but it will try a full defragmentation pass if needed.
 * When allocated from the gamethread, the user can call GStreamingManager->StreamOutTextureData() to make room.
 *
 * @param MemType - Which type of memory to allocate. 
 * @param Size - aligned size of allocation
 * @return shared memory resource RHI ref
 */
DEFINE_RHIMETHOD(FSharedMemoryResourceRHIRef,CreateSharedMemory,(EGPUMemoryType MemType,SIZE_T Size),(MemType, Size),return,return new TNullRHIResource<RRT_SharedMemoryResource>(););

/**
 * Creates a RHI texture and if the platform supports it overlaps it in memory with another texture
 * Note that modifying this texture will modify the memory of the overlapped texture as well
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The 2d texture to use the memory from if the platform allows
 * @param Flags - Surface creation flags
 * @return The surface that was created.
 */
DEFINE_RHIMETHOD(
	FSharedTexture2DRHIRef,
	CreateSharedTexture2D,
	(UINT SizeX,UINT SizeY,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemory,DWORD Flags),
	(SizeX,SizeY,Format,NumMips,SharedMemory,Flags),
	return,
	return new TNullRHIResource<RRT_SharedTexture2D>();
	);

/**
 * Creates a RHI texture array and if the platform supports it overlaps it in memory with another texture
 * Note that modifying this texture will modify the memory of the overlapped texture as well
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param SizeZ - The depth of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The 2d texture to use the memory from if the platform allows
 * @param Flags - Surface creation flags
 * @return The surface that was created.
 */
DEFINE_RHIMETHOD(
	FSharedTexture2DArrayRHIRef,
	CreateSharedTexture2DArray,
	(UINT SizeX,UINT SizeY,UINT SizeZ,BYTE Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemory,DWORD Flags),
	(SizeX,SizeY,SizeZ,Format,NumMips,SharedMemory,Flags),
	return,
	return new TNullRHIResource<RRT_SharedTexture2DArray>();
	);

/**
* Creates a Cube RHI texture resource
* @param Size - width/height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
DEFINE_RHIMETHOD(
	FTextureCubeRHIRef,
	CreateTextureCube,
	(UINT Size,BYTE Format,UINT NumMips,DWORD Flags,FResourceBulkDataInterface* BulkData),
	(Size,Format,NumMips,Flags,BulkData),
	return,
	return new TNullRHIResource<RRT_TextureCube>();
	);

/**
* Locks an RHI texture's mip surface for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - mip level index for the surface to retrieve
* @param bIsDataBeingWrittenTo - used to affect the lock flags 
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
DEFINE_RHIMETHOD(
	void*,
	LockTextureCubeFace,
	(FTextureCubeRHIParamRef Texture,UINT FaceIndex,UINT MipIndex,UBOOL bIsDataBeingWrittenTo,UINT& DestStride,UBOOL bLockWithinMiptail),
	(Texture,FaceIndex,MipIndex,bIsDataBeingWrittenTo,DestStride,bLockWithinMiptail),
	return,
	DestStride = 0; return GetStaticBuffer();
	);

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - mip level index for the surface to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
DEFINE_RHIMETHOD(
	void,
	UnlockTextureCubeFace,
	(FTextureCubeRHIParamRef Texture,UINT FaceIndex,UINT MipIndex,UBOOL bLockWithinMiptail),
	(Texture,FaceIndex,MipIndex,bLockWithinMiptail),
	,
	);

/**
 * Creates a RHI surface that can be bound as a render target.
 * Note that a surface cannot be created which is both resolvable AND readable.
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The 2d texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
 * @param Flags - Surface creation flags
 * @param UsageStr - Text describing usage for this surface
 * @return The surface that was created.
 */
DEFINE_RHIMETHOD(
	FSurfaceRHIRef,
	CreateTargetableSurface,
	(UINT SizeX,UINT SizeY,BYTE Format,FTexture2DRHIParamRef ResolveTargetTexture,DWORD Flags,const TCHAR* UsageStr),
	(SizeX,SizeY,Format,ResolveTargetTexture,Flags,UsageStr),
	return,
	return new TNullRHIResource<RRT_Surface>();
	);

#if !RHI_UNIFIED_MEMORY && !USE_NULL_RHI
DEFINE_RHIMETHOD(
    void,
    GetTargetSurfaceSize,
    (FSurfaceRHIParamRef InSurface, UINT& OutSizeX,UINT& OutSizeY),
    (InSurface,OutSizeX,OutSizeY),
    return,
    return;
);
#endif

#if XBOX
/**
 * Creates a RHI surface that can be bound as a render target, from a specific location if the platform supports it.
 * Note that a surface cannot be created which is both resolvable AND readable.
 * @param SizeX - The width of the surface to create.
 * @param SizeY - The height of the surface to create.
 * @param Format - The surface format to create.
 * @param ResolveTargetTexture - The 2d texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
 * @param Flags - Surface creation flags
 * @param Location - Location at which the surface memory resides
 * @return The surface that was created.
 */
DEFINE_RHIMETHOD(
	FSurfaceRHIRef,
	CreateTargetableSurfaceExplicit,
	(UINT SizeX,UINT SizeY,BYTE Format,FTexture2DRHIParamRef ResolveTargetTexture,DWORD Flags, UPTRINT Location),
	(SizeX,SizeY,Format,ResolveTargetTexture,Flags,Location),
	return,
	return new TNullRHIResource<RRT_Surface>();
	);
#endif

/**
* Creates a RHI surface that can be bound as a render target and can resolve w/ a cube texture
* Note that a surface cannot be created which is both resolvable AND readable.
* @param SizeX - The width of the surface to create.
* @param Format - The surface format to create.
* @param ResolveTargetTexture - The cube texture which the surface will be resolved to.  It must have been allocated with bResolveTargetable=TRUE
* @param CubeFace - face from resolve texture to use as surface
* @param Flags - Surface creation flags
* @param UsageStr - Text describing usage for this surface
* @return The surface that was created.
*/
DEFINE_RHIMETHOD(
	FSurfaceRHIRef,
	CreateTargetableCubeSurface,
	(UINT SizeX,BYTE Format,FTextureCubeRHIParamRef ResolveTargetTexture,ECubeFace CubeFace,DWORD Flags,const TCHAR* UsageStr),
	(SizeX,Format,ResolveTargetTexture,CubeFace,Flags,UsageStr),
	return,
	return new TNullRHIResource<RRT_Surface>();
	);

#if XBOX
/**
* Calculates the required size of a surface.
* @param SizeX - The width of the surface.
* @param SizeX - The height of the surface.
* @param Format - The surface format.
* @return The required size of the surface
*/
DEFINE_RHIMETHOD(
	UPTRINT,
	CalcTargetableSurfaceSize,
	(UINT SizeX,UINT SizeY,BYTE Format),
	(SizeX,SizeY,Format),
	return,
	return 0;
	);
#endif

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param bKeepOriginalSurface - TRUE if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
DEFINE_RHIMETHOD(
	void,
	CopyToResolveTarget,
	(FSurfaceRHIParamRef SourceSurface, UBOOL bKeepOriginalSurface, const FResolveParams& ResolveParams),
	(SourceSurface,bKeepOriginalSurface,ResolveParams),
	,
	);

/**
 * Copies the contents of the given surface's resolve target texture back to the surface.
 * If the surface isn't currently allocated, the copy may be deferred until the next time it is allocated.
 * @param DestSurface - surface with a resolve texture to copy from
 */
DEFINE_RHIMETHOD(void,CopyFromResolveTarget,(FSurfaceRHIParamRef DestSurface),(DestSurface),,);

/**
* Copies the contents of the given surface's resolve target texture back to the surface without doing
* anything to the pixels (no exponent correction, no gamma correction).
* If the surface isn't currently allocated, the copy may be deferred until the next time it is allocated.
*
* @param DestSurface - surface with a resolve texture to copy from
*/
DEFINE_RHIMETHOD(void,CopyFromResolveTargetFast,(FSurfaceRHIParamRef DestSurface),(DestSurface),,);

/**
* Copies a subset of the contents of the given surface's resolve target texture back to the surface without doing
* anything to the pixels (no exponent correction, no gamma correction).
* If the surface isn't currently allocated, the copy may be deferred until the next time it is allocated.
*
* @param DestSurface - surface with a resolve texture to copy from
*/
DEFINE_RHIMETHOD(void,CopyFromResolveTargetRectFast,(FSurfaceRHIParamRef DestSurface, FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2),(DestSurface,X1,Y1,X2,Y2),,);

/**
 *	Copies the contents of the back buffer to specified texture.
 *	@param ResolveParams Required resolve params
 */
DEFINE_RHIMETHOD(void,CopyFrontBufferToTexture,( const FResolveParams& ResolveParams ),(ResolveParams),,);

/**
 * Notifies the driver (and our RHI layer) that the content of the specified buffers of the current
 * rendertarget are no longer needed and can be undefined from now on.
 * This allows us to avoid saving an unused renderbuffer to main memory (when used after rendering)
 * or restoring a renderbuffer from memory (when used before rendering). This can be a significant
 * performance cost on some platforms (e.g. tile-based GPUs).
 *
 * @param RenderBufferTypes		Binary bitfield of flags from ERenderBufferTypes
 */
DEFINE_RHIMETHOD(void,DiscardRenderBuffer,( DWORD RenderBufferTypes ),(RenderBufferTypes),,);

/**
 * Reads the contents of a surface to an output buffer.
 */
DEFINE_RHIMETHOD(
	void,
	ReadSurfaceData,
	(FSurfaceRHIParamRef Surface,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData,FReadSurfaceDataFlags InFlags),
	(Surface,MinX,MinY,MaxX,MaxY,OutData,InFlags),
	,
	);

DEFINE_RHIMETHOD(
	void,
	ReadSurfaceDataMSAA,
	(FSurfaceRHIParamRef Surface,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags),
	(Surface,MinX,MinY,MaxX,MaxY,OutData,InFlags),
	,
	);

DEFINE_RHIMETHOD(
	void,
	ReadSurfaceFloatData,
	(FSurfaceRHIParamRef Surface,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<FFloat16Color>& OutData,ECubeFace CubeFace),
	(Surface,MinX,MinY,MaxX,MaxY,OutData,CubeFace),
	,
	);

DEFINE_RHIMETHOD(FOcclusionQueryRHIRef,CreateOcclusionQuery,(),(),return,return new TNullRHIResource<RRT_OcclusionQuery>(););
DEFINE_RHIMETHOD(void,ResetOcclusionQuery,(FOcclusionQueryRHIParamRef OcclusionQuery),(OcclusionQuery),,);
DEFINE_RHIMETHOD(
	UBOOL,
	GetOcclusionQueryResult,
	(FOcclusionQueryRHIParamRef OcclusionQuery,DWORD& OutNumPixels,UBOOL bWait),
	(OcclusionQuery,OutNumPixels,bWait),
	return,
	return TRUE
	);

DEFINE_RHIMETHOD(void,BeginDrawingViewport,(FViewportRHIParamRef Viewport),(Viewport),,);
DEFINE_RHIMETHOD(void,EndDrawingViewport,(FViewportRHIParamRef Viewport,UBOOL bPresent,UBOOL bLockToVsync),(Viewport,bPresent,bLockToVsync),,);
/**
 * Determine if currently drawing the viewport
 *
 * @return TRUE if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
DEFINE_RHIMETHOD(UBOOL,IsDrawingViewport,(),(),return,return FALSE;);
DEFINE_RHIMETHOD(FSurfaceRHIRef,GetViewportBackBuffer,(FViewportRHIParamRef Viewport),(Viewport),return,return new TNullRHIResource<RRT_Surface>(););
DEFINE_RHIMETHOD(FSurfaceRHIRef,GetViewportDepthBuffer,(FViewportRHIParamRef Viewport),(Viewport),return,return new TNullRHIResource<RRT_Surface>(););

DEFINE_RHIMETHOD(void,BeginScene,(),(),return,return;);
DEFINE_RHIMETHOD(void,EndScene,(),(),return,return;);

/*
 * Acquires or releases ownership of the platform-specific rendering context for the calling thread
 */
DEFINE_RHIMETHOD(void,AcquireThreadOwnership,(),(),return,return;);
DEFINE_RHIMETHOD(void,ReleaseThreadOwnership,(),(),return,return;);

/*
 * Returns the total GPU time taken to render the last frame. Same metric as appCycles().
 */
DEFINE_RHIMETHOD(DWORD,GetGPUFrameCycles,(),(),return,return 0;);

/*
 * Returns an approximation of the available video memory that textures can use, rounded to the nearest MB, in MB.
 */
DEFINE_RHIMETHOD(DWORD,GetAvailableTextureMemory,(),(),return,return 0;);

/**
 * The following RHI functions must be called from the main thread.
 */
DEFINE_RHIMETHOD(
	FViewportRHIRef,
	CreateViewport,
	(void* WindowHandle,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen),
	(WindowHandle,SizeX,SizeY,bIsFullscreen),
	return,
	return new FNullViewportRHI();
	);
DEFINE_RHIMETHOD(void,ResizeViewport,(FViewportRHIParamRef Viewport,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen),(Viewport,SizeX,SizeY,bIsFullscreen),,);
DEFINE_RHIMETHOD(void,Tick,( FLOAT DeltaTime ),(DeltaTime),,);

//
// RHI commands.
//

// Vertex state.
DEFINE_RHIMETHOD(
	void,
	SetStreamSource,
	(UINT StreamIndex,FVertexBufferRHIParamRef VertexBuffer,UINT Stride,UINT Offset,UBOOL bUseInstanceIndex,UINT NumVerticesPerInstance,UINT NumInstances),
	(StreamIndex,VertexBuffer,Stride,Offset,bUseInstanceIndex,NumVerticesPerInstance,NumInstances),
	,
	);

// Rasterizer state.
DEFINE_RHIMETHOD(void,SetRasterizerState,(FRasterizerStateRHIParamRef NewState),(NewState),,);
DEFINE_RHIMETHOD(void,SetRasterizerStateImmediate,(const FRasterizerStateInitializerRHI& ImmediateState),(ImmediateState),,);
DEFINE_RHIMETHOD(void,SetViewport,(UINT MinX,UINT MinY,FLOAT MinZ,UINT MaxX,UINT MaxY,FLOAT MaxZ),(MinX,MinY,MinZ,MaxX,MaxY,MaxZ),,);
DEFINE_RHIMETHOD(void,SetScissorRect,(UBOOL bEnable,UINT MinX,UINT MinY,UINT MaxX,UINT MaxY),(bEnable,MinX,MinY,MaxX,MaxY),,);
DEFINE_RHIMETHOD(void,SetDepthBoundsTest,(UBOOL bEnable,const FVector4& ClipSpaceNearPos,const FVector4& ClipSpaceFarPos),(bEnable,ClipSpaceNearPos,ClipSpaceFarPos),,);

// Shader state.
/**
 * Set bound shader state. This will set the vertex decl/shader, and pixel shader
 * @param BoundShaderState - state resource
 */
DEFINE_RHIMETHOD(void,SetBoundShaderState,(FBoundShaderStateRHIParamRef BoundShaderState),(BoundShaderState),,);

/**
 * Set sampler state without modifying texture assignments.  
 * This is only valid for RHI's which support separate sampler state and texture state, like D3D 11.
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerStateOnly,
	(FPixelShaderRHIParamRef PixelShader,UINT SamplerIndex,FSamplerStateRHIParamRef NewState),
	(PixelShader,SamplerIndex,NewState),
	,
	);

/** 
 * Set texture state without modifying sampler state.  
 * This is only valid for RHI's which support separate sampler state and texture state, like D3D 11.
 */
DEFINE_RHIMETHOD(
	void,
	SetTextureParameter,
	(FPixelShaderRHIParamRef PixelShader,UINT TextureIndex,FTextureRHIParamRef NewTexture),
	(PixelShader,TextureIndex,NewTexture),
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD(
	void,
	SetSurfaceParameter,
	(FPixelShaderRHIParamRef PixelShader,UINT TextureIndex,FSurfaceRHIParamRef NewSurface),
	(PixelShader,TextureIndex,NewSurface),
	,
	);

/**
 * Sets sampler state.
 *
 * @param PixelShader	The pixelshader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerState,
	(FPixelShaderRHIParamRef PixelShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter),
	(PixelShader,TextureIndex,SamplerIndex,NewState,NewTexture,MipBias,LargestMip,SmallestMip,bForceLinearMinFilter),
	,
	);

/**
 * Sets sampler state.
 *
 * @param VertexShader	The vertexshader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index and OpenGL where it's the texture unit.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param BaseIndex		Ignored on all platforms except OpenGL, where it's used to calculate sampler index
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerState,
	(FVertexShaderRHIParamRef VertexShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter),
	(VertexShader,TextureIndex,SamplerIndex,NewState,NewTexture,MipBias,LargestMip,SmallestMip,bForceLinearMinFilter),
	,
	);

/**
* Sets vertex texture sampler state.
*
* @param SamplerIndex	Vertex texture sampler index.
* @param NewTextureRHI	Texture to set.
*/
DEFINE_RHIMETHOD(
				 void,
				 SetVertexTexture,
				 (UINT SamplerIndex,FTextureRHIParamRef NewTextureRHI),
				 (SamplerIndex,NewTextureRHI),
				 ,
				 );


#if WITH_D3D11_TESSELLATION
/**
 * Sets sampler state.
 *
 * @param GeometryShaderRHI	The geometry shader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerState,
	(FGeometryShaderRHIParamRef GeometryShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter),
	(GeometryShader,TextureIndex,SamplerIndex,NewState,NewTexture,MipBias,LargestMip,SmallestMip,bForceLinearMinFilter),
	,
	);

/**
 * Sets sampler state.
 *
 * @param ComputeShaderRHI	The compute shader using the sampler.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerState,
	(FComputeShaderRHIParamRef ComputeShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter),
	(ComputeShader,TextureIndex,SamplerIndex,NewState,NewTexture,MipBias,LargestMip,SmallestMip,bForceLinearMinFilter),
	,
	);

/**
 * Sets sampler state for a domain shader.
 *
 * @param DomainShader	The DomainShader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerState,
	(FDomainShaderRHIParamRef DomainShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter),
	(DomainShader,TextureIndex,SamplerIndex,NewState,NewTexture,MipBias,LargestMip,SmallestMip,bForceLinearMinFilter),
	,
	);

/**
 * Sets sampler state for a hull shader.
 *
 * @param HullShader	The HullShader using the sampler for the next drawcalls.
 * @param TextureIndex	Used as sampler index on all platforms except D3D11, where it's the texture resource index.
 * @param SamplerIndex	Ignored on all platforms except D3D11, where it's the sampler index.
 * @param MipBias		Mip bias to use for the texture
 * @param LargestMip	Largest-resolution mip-level to use (zero-based, e.g. 0). -1 means use default settings. (FLOAT on PS3, INT on Xbox/D3D9, ignored on D3D11)
 * @param SmallestMip	Smallest-resolution mip-level to use (zero-based, e.g. 12). -1 means use default settings. (FLOAT on PS3, INT on Xbox, ignored on other platforms)
 */
DEFINE_RHIMETHOD(
	void,
	SetSamplerState,
	(FHullShaderRHIParamRef HullShader,UINT TextureIndex,UINT SamplerIndex,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip,UBOOL bForceLinearMinFilter),
	(HullShader,TextureIndex,SamplerIndex,NewState,NewTexture,MipBias,LargestMip,SmallestMip,bForceLinearMinFilter),
	,
	);

/** Set the shader resource view of a surface. This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD(
	 void,
	 SetSurfaceParameter,
	 (FComputeShaderRHIParamRef ComputeShader,UINT TextureIndex,FSurfaceRHIParamRef NewSurface),
	 (ComputeShader,TextureIndex,NewSurface),
	 ,
	 );

/** Set the unordered access view of a surface. This is used for binding UAV to the compute shader. */
DEFINE_RHIMETHOD(
	 void,
	 SetUAVParameter,
	 (FComputeShaderRHIParamRef ComputeShader,UINT TextureIndex,FSurfaceRHIParamRef NewSurface),
	 (ComputeShader,TextureIndex,NewSurface),
	 ,
	 );
#endif

/**
 * Returns the slot index and the size of a mobile uniform parameter.
 *
 * @param ParamName		Name of the uniform parameter to check for.
 * @param OutNumBytes	[out] Set to the size of the parameter value, in bytes, if the parameter was found.
 * @return				Parameter slot index, or -1 if the parameter was not found.
 */
DEFINE_RHIMETHOD(
				 INT,
				 GetMobileUniformSlotIndexByName,
				 (FName ParamName, WORD& OutNumBytes),
				 (ParamName, OutNumBytes),
				 return,
				 return -1);

DEFINE_RHIMETHOD(
				 void,
				 SetMobileTextureSamplerState,
				 (FPixelShaderRHIParamRef PixelShader,const INT MobileTextureUnit,FSamplerStateRHIParamRef NewState,FTextureRHIParamRef NewTexture,FLOAT MipBias,FLOAT LargestMip,FLOAT SmallestMip),
				 (PixelShader,MobileTextureUnit,NewState,NewTexture,MipBias,LargestMip,SmallestMip),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileSimpleParams,
				 (const EBlendMode InBlendMode),
				 (InBlendMode),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileMaterialVertexParams,
				 (const FMobileMaterialVertexParams& InVertexParams),
				 (InVertexParams),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileMaterialPixelParams,
				 (const FMobileMaterialPixelParams& InPixelParams),
				 (InPixelParams),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileMeshVertexParams,
				 (const FMobileMeshVertexParams& InMeshParams),
				 (InMeshParams),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileMeshPixelParams,
				 (const FMobileMeshPixelParams& InMeshParams),
				 (InMeshParams),
				 ,
				 );

DEFINE_RHIMETHOD(
				 FLOAT,
				 GetMobilePercentColorFade,
				 (),
				 (),
				 return,
				 return 0.0f);

DEFINE_RHIMETHOD(
				 void,
				 SetMobileFogParams,
				 (const UBOOL bInEnabled, const FLOAT InFogStart, const FLOAT InFogEnd, const FColor& InFogColor),
				 (bInEnabled, InFogStart, InFogEnd, InFogColor),
				 ,
				 );

DEFINE_RHIMETHOD(
				void,
				SetMobileHeightFogParams,
				(const struct FHeightFogParams& Params),
				(Params),
				,
				);

DEFINE_RHIMETHOD(
				 void,
				 SetMobileBumpOffsetParams,
				 (const UBOOL bInEnabled, const FLOAT InBumpEnd),
				 (bInEnabled, InBumpEnd),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileGammaCorrection,
				 (const UBOOL bInEnabled),
				 (bInEnabled),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileTextureTransformOverride,
				 (TMatrix<3,3>& InOverrideTransform),
				 (InOverrideTransform),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileDistanceFieldParams,
				 (const struct FMobileDistanceFieldParams& Params),
				 (Params),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileColorGradingParams,
				 (const struct FMobileColorGradingParams& Params),
				 (Params),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void*,
				 GetMobileProgramInstance,
				 (),
				 (),
				 return,
				 return NULL
				 );

DEFINE_RHIMETHOD(
				 void,
				 SetMobileProgramInstance,
				 (void* ProgramInstance),
				 (ProgramInstance),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 ResetTrackedPrimitive,
				 (),
				 (),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 CycleTrackedPrimitiveMode,
				 (),
				 (),
				 ,
				 );

DEFINE_RHIMETHOD(
				 void,
				 IncrementTrackedPrimitive,
				 (const INT InDelta),
				 (InDelta),
				 ,
				 );


DEFINE_RHIMETHOD(
	void,
	ClearSamplerBias,
	(),
	(),
	,
	);
DEFINE_RHIMETHOD(
	void,
	SetVertexShaderParameter,
	(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	(VertexShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	,
	);
DEFINE_RHIMETHOD(
	void,
	SetVertexShaderBoolParameter,
	(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue),
	(VertexShader,BufferIndex,BaseIndex,NewValue),
	,
	);	
DEFINE_RHIMETHOD(
	void,
	SetVertexShaderFloatArray,
	(FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumValues,const FLOAT* FloatValues, INT ParamIndex),
	(VertexShader,BufferIndex,BaseIndex,NumValues,FloatValues,ParamIndex),
	,
	);
DEFINE_RHIMETHOD(
	void,
	SetPixelShaderParameter,
	(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	(PixelShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	,
	);
DEFINE_RHIMETHOD(
	void,
	SetPixelShaderBoolParameter,
	(FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue),
	(PixelShader,BufferIndex,BaseIndex,NewValue),
	,
	);
DEFINE_RHIMETHOD(
	 void,
	 SetShaderParameter,
	 (FVertexShaderRHIParamRef VertexShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	 (VertexShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	 ,
	 );
DEFINE_RHIMETHOD(
	 void,
	 SetShaderParameter,
	 (FPixelShaderRHIParamRef PixelShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	 (PixelShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	 ,
	 );

#if WITH_D3D11_TESSELLATION

DEFINE_RHIMETHOD(
	 void,
	 SetShaderBoolParameter,
	 (FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue),
	 (HullShader,BufferIndex,BaseIndex,NewValue),
	 ,
	 );	
DEFINE_RHIMETHOD(
	 void,
	 SetShaderBoolParameter,
	 (FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UBOOL NewValue),
	 (DomainShader,BufferIndex,BaseIndex,NewValue),
	 ,
	 );	

DEFINE_RHIMETHOD(
	 void,
	 SetShaderParameter,
	 (FHullShaderRHIParamRef HullShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	 (HullShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	 ,
	 );
DEFINE_RHIMETHOD(
	 void,
	 SetShaderParameter,
	 (FDomainShaderRHIParamRef DomainShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	 (DomainShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	 ,
	 );
DEFINE_RHIMETHOD(
	 void,
	 SetShaderParameter,
	 (FGeometryShaderRHIParamRef GeometryShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	 (GeometryShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	 ,
	 );
DEFINE_RHIMETHOD(
	 void,
	 SetShaderParameter,
	 (FComputeShaderRHIParamRef ComputeShader,UINT BufferIndex,UINT BaseIndex,UINT NumBytes,const void* NewValue, INT ParamIndex),
	 (ComputeShader,BufferIndex,BaseIndex,NumBytes,NewValue,ParamIndex),
	 ,
	 );
#endif

DEFINE_RHIMETHOD(void,SetRenderTargetBias,(FLOAT ColorBias),(ColorBias),,);

/**
 * Set engine vertex shader parameters for the view.
 * @param View					The current view
 */
DEFINE_RHIMETHOD(
	void,
	SetViewParameters,
	(const FSceneView& View),
	(View),
	,
	);

/**
 * Set engine shader parameters for the view.
 * @param View					The current view
 * @param ViewProjectionMatrix	Matrix that transforms from world space to projection space for the view
 * @param DiffuseOverride		Material diffuse input override
 * @param SpecularOverride		Material specular input override
 */
DEFINE_RHIMETHOD(
	void,
	SetViewParametersWithOverrides,
	(const FSceneView& View,const FMatrix& ViewProjectionMatrix,const FVector4& DiffuseOverride,const FVector4& SpecularOverride),
	(View,ViewProjectionMatrix,DiffuseOverride,SpecularOverride),
	,
	);

/**
 * Set engine pixel shader parameters for the view.
 * Some platforms needs to set this for each pixelshader, whereas others can set it once, globally.
 * @param View								The current view
 * @param PixelShader						The pixel shader to set the parameters for
 * @param SceneDepthCalcParameter			Handle for the scene depth calc parameter (PSR_MinZ_MaxZ_Ratio). May be NULL.
 * @param ScreenPositionScaleBiasParameter	Handle for the screen position scale and bias parameter (PSR_ScreenPositionScaleBias). May be NULL.
 * @param ScreenAndTexelParameter			Handle for the screen and texel size parameter (PSR_ScreenAndTexelSize). May be NULL.
 */
DEFINE_RHIMETHOD(
	void,
	SetViewPixelParameters,
	(const FSceneView* View,FPixelShaderRHIParamRef PixelShader,const class FShaderParameter* SceneDepthCalcParameter,const class FShaderParameter* ScreenPositionScaleBiasParameter,const class FShaderParameter* ScreenAndTexelSizeParameter),
	(View,PixelShader,SceneDepthCalcParameter,ScreenPositionScaleBiasParameter,ScreenAndTexelSizeParameter),
	,
	);

/**
 * Control the GPR (General Purpose Register) allocation 
 * @param NumVertexShaderRegisters - num of GPRs to allocate for the vertex shader (default is 64)
 * @param NumPixelShaderRegisters - num of GPRs to allocate for the pixel shader (default is 64)
 */
DEFINE_RHIMETHOD(
	void,
	SetShaderRegisterAllocation,
	(UINT NumVertexShaderRegisters,UINT NumPixelShaderRegisters),
	(NumVertexShaderRegisters,NumPixelShaderRegisters),
	,
	);

/**
 * Optimizes pixel shaders that are heavily texture fetch bound due to many L2 cache misses.
 * @param PixelShader	The pixel shader to optimize texture fetching for
 */
DEFINE_RHIMETHOD(void,ReduceTextureCachePenalty,(FPixelShaderRHIParamRef PixelShader),(PixelShader),,);

// Output state.
DEFINE_RHIMETHOD(void,SetDepthState,(FDepthStateRHIParamRef NewState),(NewState),,);
DEFINE_RHIMETHOD(void,SetStencilState,(FStencilStateRHIParamRef NewState),(NewState),,);
// Allows to set the blend state, parameter can be created with RHICreateBlendState() or RHICreateMRTBlendState()
DEFINE_RHIMETHOD(void,SetBlendState,(FBlendStateRHIParamRef NewState),(NewState),,);
// only implemented for X360, should be removed as SetBlendState already can set multiple render targets on other platforms (D3D11)
DEFINE_RHIMETHOD(void,SetMRTBlendState,(FBlendStateRHIParamRef NewState,UINT TargetIndex),(NewState,TargetIndex),,);
DEFINE_RHIMETHOD(void,SetRenderTarget,(FSurfaceRHIParamRef NewRenderTarget,FSurfaceRHIParamRef NewDepthStencilTarget),(NewRenderTarget,NewDepthStencilTarget),,);
// TargetIndex numbering starts with 0 but #0 should be set with SetRenderTarget()
DEFINE_RHIMETHOD(void,SetMRTRenderTarget,(FSurfaceRHIParamRef NewRenderTarget,UINT TargetIndex),(NewRenderTarget,TargetIndex),,);
DEFINE_RHIMETHOD(void,SetColorWriteEnable,(UBOOL bEnable),(bEnable),,);
DEFINE_RHIMETHOD(void,SetColorWriteMask,(UINT ColorWriteMask),(ColorWriteMask),,);
DEFINE_RHIMETHOD(void,SetMRTColorWriteEnable,(UBOOL bEnable,UINT TargetIndex),(bEnable,TargetIndex),,);
DEFINE_RHIMETHOD(void,SetMRTColorWriteMask,(UINT ColorWriteMask,UINT TargetIndex),(ColorWriteMask,TargetIndex),,);

// Hi stencil optimization
DEFINE_RHIMETHOD(void,BeginHiStencilRecord,(UBOOL bCompareFunctionEqual, UINT RefValue),(bCompareFunctionEqual, RefValue),,);
DEFINE_RHIMETHOD(void,BeginHiStencilPlayback,(UBOOL bFlush),(bFlush),,);
DEFINE_RHIMETHOD(void,EndHiStencil,(),(),,);

// Occlusion queries.
DEFINE_RHIMETHOD(void,BeginOcclusionQuery,(FOcclusionQueryRHIParamRef OcclusionQuery),(OcclusionQuery),,);
DEFINE_RHIMETHOD(void,EndOcclusionQuery,(FOcclusionQueryRHIParamRef OcclusionQuery),(OcclusionQuery),,);

// Primitive drawing.
DEFINE_RHIMETHOD(
	void,
	DrawPrimitive,
	(UINT PrimitiveType,UINT BaseVertexIndex,UINT NumPrimitives),
	(PrimitiveType,BaseVertexIndex,NumPrimitives),
	,
	);
DEFINE_RHIMETHOD(
	void,
	DrawIndexedPrimitive,
	(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives),
	(IndexBuffer,PrimitiveType,BaseVertexIndex,MinIndex,NumVertices,StartIndex,NumPrimitives),
	,
	);

/**
 * Draws a primitive with pre-vertex-shader culling.
 * The parameters are the same as RHIDrawIndexedPrimitive, plus the primitive's LocalToWorld transform to use for culling.
 */
DEFINE_RHIMETHOD(
	void,
	DrawIndexedPrimitive_PreVertexShaderCulling,
	(FIndexBufferRHIParamRef IndexBuffer,UINT PrimitiveType,INT BaseVertexIndex,UINT MinIndex,UINT NumVertices,UINT StartIndex,UINT NumPrimitives,const FMatrix& LocalToWorld,const void* PlatformMeshData),
	(IndexBuffer,PrimitiveType,BaseVertexIndex,MinIndex,NumVertices,StartIndex,NumPrimitives,LocalToWorld,PlatformMeshData),
	,
	);

// Immediate Primitive drawing
/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex 
 * @param OutVertexData Reference to the allocated vertex memory
 */
DEFINE_RHIMETHOD(
	void,
	BeginDrawPrimitiveUP,
	(UINT PrimitiveType,UINT NumPrimitives,UINT NumVertices,UINT VertexDataStride,void*& OutVertexData),
	(PrimitiveType,NumPrimitives,NumVertices,VertexDataStride,OutVertexData),
	,
	OutVertexData = GetStaticBuffer();
	);

/**
 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
 */
DEFINE_RHIMETHOD(void,EndDrawPrimitiveUP,(),(),,);

/**
 * Draw a primitive using the vertices passed in
 * VertexData is NOT created by BeginDrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
 * @param VertexDataStride Size of each vertex
 */
DEFINE_RHIMETHOD(
	void,
	DrawPrimitiveUP,
	(UINT PrimitiveType, UINT NumPrimitives, const void* VertexData,UINT VertexDataStride),
	(PrimitiveType,NumPrimitives,VertexData,VertexDataStride),
	,
	);

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex
 * @param OutVertexData Reference to the allocated vertex memory
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumIndices Number of indices to be written
 * @param IndexDataStride Size of each index (either 2 or 4 bytes)
 * @param OutIndexData Reference to the allocated index memory
 */
DEFINE_RHIMETHOD(
	void,
	BeginDrawIndexedPrimitiveUP,
	(UINT PrimitiveType,UINT NumPrimitives,UINT NumVertices,UINT VertexDataStride,void*& OutVertexData,UINT MinVertexIndex,UINT NumIndices,UINT IndexDataStride,void*& OutIndexData),
	(PrimitiveType,NumPrimitives,NumVertices,VertexDataStride,OutVertexData,MinVertexIndex,NumIndices,IndexDataStride,OutIndexData),
	,
	OutVertexData = GetStaticBuffer();
	OutIndexData = GetStaticBuffer();
	);

/**
 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
 */
DEFINE_RHIMETHOD(void,EndDrawIndexedPrimitiveUP,(),(),,);

/**
 * Draw a primitive using the vertices passed in as described the passed in indices. 
 * IndexData and VertexData are NOT created by BeginDrawIndexedPrimitveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumVertices The number of vertices in the vertex buffer
 * @param NumPrimitives THe number of primitives described by the index buffer
 * @param IndexData The memory preallocated in RHIBeginDrawIndexedPrimitiveUP
 * @param IndexDataStride The size of one index
 * @param VertexData The memory preallocate in RHIBeginDrawIndexedPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
DEFINE_RHIMETHOD(
	void,
	DrawIndexedPrimitiveUP,
	(UINT PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT NumPrimitives,const void* IndexData,UINT IndexDataStride,const void* VertexData,UINT VertexDataStride),
	(PrimitiveType,MinVertexIndex,NumVertices,NumPrimitives,IndexData,IndexDataStride,VertexData,VertexDataStride),
	,
	);

#if NGP
/**
 * A version of DrawIndexedPrimitiveUP that takes vertex and index data that is GPU accessible and can be
 * rendered from directly, without needing to be copied off (it will stay around while the GPU is using it)
 *
 * Currently only supported on NGP
 */
DEFINE_RHIMETHOD(
	void,
	DrawIndexedPrimitiveUP_StaticGPUMemory,
	(UINT PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT NumPrimitives,const void* IndexData,UINT IndexDataStride,const void* VertexData,UINT VertexDataStride),
	(PrimitiveType,MinVertexIndex,NumVertices,NumPrimitives,IndexData,IndexDataStride,VertexData,VertexDataStride),
	,
	);
#endif

/**
 * Draw a sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite particles
 */
DEFINE_RHIMETHOD(void,DrawSpriteParticles,(const FMeshBatch& Mesh),(Mesh),,);

/**
 * Draw a sprite subuv particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
DEFINE_RHIMETHOD(void,DrawSubUVParticles,(const FMeshBatch& Mesh),(Mesh),,);

/**
 * Draw a point sprite particle emitter.
 *
 * @param Mesh The mesh element containing the data for rendering the sprite subuv particles
 */
DEFINE_RHIMETHOD(void, DrawPointSpriteParticles, (const FMeshBatch& Mesh),(Mesh),,);

// Raster operations.
// This method only cleas the MRT
DEFINE_RHIMETHOD(
	void,
	Clear,
	(UBOOL bClearColor,const FLinearColor& Color,UBOOL bClearDepth,FLOAT Depth,UBOOL bClearStencil,DWORD Stencil),
	(bClearColor,Color,bClearDepth,Depth,bClearStencil,Stencil),
	,
	);

// Kick the rendering commands that are currently queued up in the GPU command buffer.
DEFINE_RHIMETHOD(void,KickCommandBuffer,(),(),,);

// Blocks the CPU until the GPU catches up and goes idle.
DEFINE_RHIMETHOD(void,BlockUntilGPUIdle,(),(),,);

// Operations to suspend title rendering and yield control to the system
DEFINE_RHIMETHOD(void,SuspendRendering,(),(),,);
DEFINE_RHIMETHOD(void,ResumeRendering,(),(),,);
DEFINE_RHIMETHOD(UBOOL,IsRenderingSuspended,(),(),return,return FALSE;);

// MSAA-specific functions
DEFINE_RHIMETHOD(void,RestoreColorDepth,(FTexture2DRHIParamRef ColorTexture, FTexture2DRHIParamRef DepthTexture),(ColorTexture,DepthTexture),,);
DEFINE_RHIMETHOD(void,SetTessellationMode,(ETessellationMode TessellationMode, FLOAT MinTessellation, FLOAT MaxTessellation),(TessellationMode,MinTessellation,MaxTessellation),,);

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If TRUE, ignore refresh rates.
 *
 *	@return	UBOOL				TRUE if successfully filled the array
 */
DEFINE_RHIMETHOD(
	UBOOL,
	GetAvailableResolutions,
	(FScreenResolutionArray& Resolutions, UBOOL bIgnoreRefreshRate),
	(Resolutions,bIgnoreRefreshRate),
	return,
	return FALSE;
	);

/**
 * Returns a supported screen resolution that most closely matches input.
 * @param Width - Input: Desired resolution width in pixels. Output: A width that the platform supports.
 * @param Height - Input: Desired resolution height in pixels. Output: A height that the platform supports.
 */
DEFINE_RHIMETHOD(void,GetSupportedResolution,(UINT& Width,UINT& Height),(Width,Height),,);

/**
 *	Sets the maximum viewport size the application is expecting to need for the time being, or zero for
 *	no preference.  This is used as a hint to the RHI to reduce redundant device resets when viewports
 *	are created or destroyed (typically in the editor.)
 *
 *	@param NewLargestExpectedViewportWidth Maximum width of all viewports (or zero if not known)
 *	@param NewLargestExpectedViewportHeight Maximum height of all viewports (or zero if not known)
 */
DEFINE_RHIMETHOD(
	void,
	SetLargestExpectedViewportSize,
	( UINT NewLargestExpectedViewportWidth, UINT NewLargestExpectedViewportHeight ),
	( NewLargestExpectedViewportWidth, NewLargestExpectedViewportHeight ),
	,
	);

/**
 * Checks if a texture is still in use by the GPU.
 * @param Texture - the RHI texture resource to check
 * @param MipIndex - Which mipmap we're interested in
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
DEFINE_RHIMETHOD(
	UBOOL,
	IsBusyTexture2D,
	(FTexture2DRHIParamRef Texture, UINT MipIndex),
	( Texture, MipIndex ),
	return,
	return FALSE;
	);

/**
 * Checks if a vertex buffer is still in use by the GPU.
 * @param VertexBuffer - the RHI texture resource to check
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
DEFINE_RHIMETHOD(
	UBOOL,
	IsBusyVertexBuffer,
	(FVertexBufferRHIParamRef VertexBuffer),
	( VertexBuffer ),
	return,
	return FALSE;
	);

/*
 * Creates a "StereoFix" texture, which will be used on subsequent calls to UpdateStereoFixTexture
 */
DEFINE_RHIMETHOD(
    FTexture2DRHIRef,
    CreateStereoFixTexture,
    (),
    (),
    return,
    return new TNullRHIResource<RRT_Texture2D>();
    );

/**
 * Updates the "StereoFix" texture if stereo is enabled and paramteres have changes.
 * @param Texture - the RHI texture to update with the current stereo parameters, which must've been allocated by the function RHICreateStereoFixTexture.
 */
DEFINE_RHIMETHOD(void,UpdateStereoFixTexture,(FTexture2DRHIParamRef Texture),(Texture),,);

#if CONSOLE

DEFINE_RHIMETHOD(
	QWORD,
	SetFence,
	(),
	(),
	return,
	return 0;
	);

DEFINE_RHIMETHOD(
	UBOOL,
	IsFencePending,
	(QWORD Fence),
	(Fence),
	return,
	return FALSE;
	);

DEFINE_RHIMETHOD(
	void,
	WaitFence,
    (QWORD Fence),
    (Fence),
	return,
	return;
	);

#endif

#if WITH_GFx && NGP
DEFINE_RHIMETHOD(
    void,
    SetMobileGFxParams,
    (const EMobileGFxBlendMode InBlendMode),
    (InBlendMode),
    ,
    );
#endif
