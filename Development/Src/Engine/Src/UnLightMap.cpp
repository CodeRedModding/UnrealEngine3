/*=============================================================================
	UnLightMap.cpp: Light-map implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMeshClasses.h"

FLightmassDebugOptions GLightmassDebugOptions;

/** Whether to compress lightmaps */
UBOOL GAllowLightmapCompression = TRUE;

/** Whether to encode lightmaps as we process (if FALSE, we wait until all mappings are processed before packing/encoding) */
UBOOL GAllowEagerLightmapEncode = TRUE;

/** Whether to use bilinear filtering on lightmaps */
UBOOL GUseBilinearLightmaps = TRUE;

/** Whether to allow padding around mappings. Old-style UE3 lighting doesn't use this. */
UBOOL GAllowLightmapPadding = TRUE;

/** Whether to attempt to optimize light and shadow map size by repacking textures at lower resolutions */
UBOOL GRepackLightAndShadowMapTextures = TRUE;

/** Counts the number of lightmap textures generated each lighting build. */
INT GLightmapCounter = 0;

/** Whether to allow lighting builds to generate streaming lightmaps. */
UBOOL GAllowStreamingLightmaps = FALSE;

/** Largest boundingsphere radius to use when packing lightmaps into a texture atlas. */
FLOAT GMaxLightmapRadius = 5000.0f;	//10000.0;	//2000.0f;

/** The quality level of DXT encoding for lightmaps (values come from nvtt::Quality enum) */
INT GLightmapEncodeQualityLevel = 2; // nvtt::Quality_Production

/** The quality level of the current lighting build */
ELightingBuildQuality GLightingBuildQuality = Quality_Preview;

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && !CONSOLE
	/** Information about the lightmap sample that is selected */
	extern FSelectedLightmapSample GCurrentSelectedLightmapSample;
#endif

/** Maximum light intensity stored in vertex/ texture lightmaps. */
#define MAX_LIGHT_INTENSITY	16.f

#if (_MSC_VER || PLATFORM_MACOSX) && !CONSOLE
	// NOTE: We're only counting the top-level mip-map for the following variables.
	/** Total number of texels allocated for all lightmap textures. */
	QWORD GNumLightmapTotalTexels = 0;
	/** Total number of texels used if the texture was non-power-of-two. */
	QWORD GNumLightmapTotalTexelsNonPow2 = 0;
	/** Number of lightmap textures generated. */
	INT GNumLightmapTextures = 0;
	/** Total number of mapped texels. */
	QWORD GNumLightmapMappedTexels = 0;
	/** Total number of unmapped texels. */
	QWORD GNumLightmapUnmappedTexels = 0;
	/** Whether to allow cropping of unmapped borders in lightmaps and shadowmaps. Controlled by BaseEngine.ini setting. */
	UBOOL GAllowLightmapCropping = FALSE;
	/** Total lightmap texture memory size (in bytes), including GLightmapTotalStreamingSize. */
	QWORD GLightmapTotalSize = 0;
	/** Total lightmap texture memory size on an Xbox 360 (in bytes). */
	QWORD GLightmapTotalSize360 = 0;
	/** Total memory size for streaming lightmaps (in bytes). */
	QWORD GLightmapTotalStreamingSize = 0;
#endif

#include "UnTextureLayout.h"

IMPLEMENT_CLASS(ULightMapTexture2D);

void FLightMap::Serialize(FArchive& Ar)
{
	Ar << LightGuids;
}

void FLightMap::FinishCleanup()
{
	delete this;
}


/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void ULightMapTexture2D::InitializeIntrinsicPropertyValues()
{
	UTexture2D::InitializeIntrinsicPropertyValues();

	LODGroup = TEXTUREGROUP_Lightmap;
}


void ULightMapTexture2D::Serialize(FArchive& Ar)
{
	if ( Ar.IsSaving() )
	{
		ULinker* Linker = GetLinker();
		if ( Linker && Linker->Summary.PackageFlags & PKG_StoreCompressed && TextureFileCacheName == NAME_None )
		{
			NeverStream = TRUE;
		}
	}

	Super::Serialize(Ar);

	if ( Ar.Ver() >= VER_LIGHTMAPFLAGS )
	{
		DWORD Flags = LightmapFlags;
		Ar << Flags;
		LightmapFlags = ELightMapFlags( Flags );
	}
	else if ( Ar.Ver() >= VER_LIGHTMAPTEXTURE_VARIABLE )
	{
		UBOOL bSimpleLightmap = (LightmapFlags & LMF_SimpleLightmap) ? TRUE : FALSE;
		Ar << bSimpleLightmap;
		LightmapFlags = bSimpleLightmap ? LMF_SimpleLightmap : LMF_None;
	}
	else if ( Ar.IsLoading() )
	{
		UBOOL bSimpleLightmap = FALSE;
#if _WINDOWS && !SHIPPING_PC_GAME
		FNameEntry* NameEntry = FName::GetEntry( GetPureName().GetIndex() );
		if ( NameEntry->IsUnicode() )
		{
			bSimpleLightmap = (*NameEntry->GetUniName() == TEXT('S')) ? TRUE : FALSE;
		}
		else
		{
			bSimpleLightmap = (*NameEntry->GetAnsiName() == 'S') ? TRUE : FALSE;
		}
#endif
		LightmapFlags = bSimpleLightmap ? LMF_SimpleLightmap : LMF_None;
	}

	// To handle the case of rebuilding lighting on a map that is saved compressed
	if( Ar.IsLoading() ) 
	{ 
		ULinker* Linker = Ar.GetLinker();
		if( Linker && ( Linker->Summary.PackageFlags & PKG_StoreCompressed ) && TextureFileCacheName == NAME_None )
		{ 
			NeverStream = TRUE; 
		} 
	}

	LODGroup = TEXTUREGROUP_Lightmap;
}

/** 
 * Returns a one line description of an object for viewing in the generic browser
 */
FString ULightMapTexture2D::GetDesc()
{
	return FString::Printf( TEXT("Lightmap: %dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name );
}

/** 
 * Returns detailed info to populate listview columns
 */
FString ULightMapTexture2D::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeY );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		break;
	}
	return( Description );
}

/** Lightmap resolution scaling factors for debugging.  The defaults are to use the original resolution unchanged. */
FLOAT TextureMappingDownsampleFactor0 = 1.0f;
INT TextureMappingMinDownsampleSize0 = 16;
FLOAT TextureMappingDownsampleFactor1 = 1.0f;
INT TextureMappingMinDownsampleSize1 = 128;
FLOAT TextureMappingDownsampleFactor2 = 1.0f;
INT TextureMappingMinDownsampleSize2 = 256;

static INT AdjustTextureMappingSize(INT InSize)
{
	INT NewSize = InSize;
	if (InSize > TextureMappingMinDownsampleSize0 && InSize <= TextureMappingMinDownsampleSize1)
	{
		NewSize = appTrunc(InSize * TextureMappingDownsampleFactor0);
	}
	else if (InSize > TextureMappingMinDownsampleSize1 && InSize <= TextureMappingMinDownsampleSize2)
	{
		NewSize = appTrunc(InSize * TextureMappingDownsampleFactor1);
	}
	else if (InSize > TextureMappingMinDownsampleSize2)
	{
		NewSize = appTrunc(InSize * TextureMappingDownsampleFactor2);
	}
	return NewSize;
}

FStaticLightingMesh::FStaticLightingMesh(
	INT InNumTriangles,
	INT InNumShadingTriangles,
	INT InNumVertices,
	INT InNumShadingVertices,
	INT InTextureCoordinateIndex,
	UBOOL bInCastShadow,
	UBOOL bInSelfShadowOnly,
	UBOOL bInTwoSidedMaterial,
	const TArray<ULightComponent*>& InRelevantLights,
	const UPrimitiveComponent* const InComponent,
	const FBox& InBoundingBox,
	const FGuid& InGuid
	):
	NumTriangles(InNumTriangles),
	NumShadingTriangles(InNumShadingTriangles),
	NumVertices(InNumVertices),
	NumShadingVertices(InNumShadingVertices),
	TextureCoordinateIndex(InTextureCoordinateIndex),
	bCastShadow(bInCastShadow && InComponent->bCastStaticShadow),
	bSelfShadowOnly(bInSelfShadowOnly),
	bTwoSidedMaterial(bInTwoSidedMaterial),
	RelevantLights(InRelevantLights),
	Component(InComponent),
	BoundingBox(InBoundingBox),
	Guid(appCreateGuid()),
	SourceMeshGuid(InGuid)
{}

FStaticLightingTextureMapping::FStaticLightingTextureMapping(FStaticLightingMesh* InMesh,UObject* InOwner,INT InSizeX,INT InSizeY,INT InLightmapTextureCoordinateIndex,UBOOL bInForceDirectLightMap,UBOOL bInBilinearFilter):
	FStaticLightingMapping(InMesh,InOwner,bInForceDirectLightMap),
	SizeX(AdjustTextureMappingSize(InSizeX)),
	SizeY(AdjustTextureMappingSize(InSizeY)),
	LightmapTextureCoordinateIndex(InLightmapTextureCoordinateIndex),
	bBilinearFilter(bInBilinearFilter)
{}

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

/**
 * An allocation of a region of light-map texture to a specific light-map.
 */
struct FLightMapAllocation
{
	/** 
	 * Basic constructor
	 */
	FLightMapAllocation()
	{
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = 0;
		MappedRect.Max.Y = 0;
		Primitive = NULL;
		LightmapFlags = LMF_None;
		bSkipEncoding = FALSE;
		bInstancedStaticMesh = FALSE;
	}

	/**
	 * Copy construct from FQuantizedLightmapData
	 */
	FLightMapAllocation(const FQuantizedLightmapData* QuantizedData)
		: TotalSizeX(QuantizedData->SizeX)
		, TotalSizeY(QuantizedData->SizeY)
		, RawData(QuantizedData->Data)
	{
		appMemcpy(Scale, QuantizedData->Scale, sizeof(Scale));
		PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = TotalSizeX;
		MappedRect.Max.Y = TotalSizeY;
		Primitive = NULL;
		LightmapFlags = LMF_None;
		bSkipEncoding = FALSE;
		bInstancedStaticMesh = FALSE;
	}

	FLightMap2D*	LightMap;
	UObject*		Outer;

	UObject*		Primitive;

	/** Upper-left X-coordinate in the texture atlas. */
	INT				OffsetX;
	/** Upper-left Y-coordinate in the texture atlas. */
	INT				OffsetY;
	/** Total number of texels along the X-axis. */
	INT				TotalSizeX;
	/** Total number of texels along the Y-axis. */
	INT				TotalSizeY;
	/** The rectangle of mapped texels within this mapping that is placed in the texture atlas. */
	FIntRect		MappedRect;
	UBOOL			bDebug;
	ELightMapPaddingType			PaddingType;
	ELightMapFlags					LightmapFlags;
	TArray<FLightMapCoefficients>	RawData;
	FLOAT							Scale[NUM_STORED_LIGHTMAP_COEF][3];
	UMaterialInterface*				Material;
	/** Bounds of the primitive that the mapping is applied to. */
	FBoxSphereBounds				Bounds;
	/** True if we can skip encoding this allocation because it's similar enough to an existing
	    allocation at the same offset */
	UBOOL bSkipEncoding;
	/** True if this allocation is for an instanced static mesh for which want to force grouping */
	UBOOL bInstancedStaticMesh;
};

/** Wrapper class to keep track of an async DXT-compression task */
struct FLightMapAsyncTask
{
	/**
	 * Initializes the member variables.
	 *
	 * @param InTexture				Pending texture to compress (contains 4 textures, 3 coefficient textures and 1 simple lightmap)
	 * @param InTask				Async task that performs the DXT-compression
	 * @param InCoefficientIndex	Coefficient index, indicating which texture the task is working on
	 * @param InMipIndex			Mip-level index, indicating which mip-level the task is working on
	 */
	FLightMapAsyncTask( struct FLightMapPendingTexture* InTexture, FAsyncTask<FAsyncDXTCompress>* InTask, UINT InCoefficientIndex, INT InMipIndex )
	:	Texture(InTexture)
	,	Task(InTask)
	,	CoefficientIndex(InCoefficientIndex)
	,	MipIndex( InMipIndex )
	{
	}

	/** Destructor. Deletes the async work and all the memory used for the compression task. */
	~FLightMapAsyncTask()
	{
		delete Task;
		Task = NULL;
	}

	/** Pending texture to compress (contains 4 textures, 3 coefficient textures and 1 simple lightmap) */
	struct FLightMapPendingTexture* Texture;
	/** Async task that performs the DXT-compression */
	FAsyncTask<FAsyncDXTCompress>* Task;
	/** Coefficient index, indicating which texture the task is working on */
	UINT CoefficientIndex;
	/** Mip-level index, indicating which mip-level the task is working on */
	INT MipIndex;
};

/**
 * A light-map texture which has been partially allocated, but not yet encoded.
 */
struct FLightMapPendingTexture : public FTextureLayout
{
	/** All mip-compression tasks: One task per mip-level, for each texture. */
	static TArray<FLightMapAsyncTask*>	TotalAsyncTasks;
	/** Pending PVRTC compression tasks */
	static FAsyncPVRTCCompressor PVRTCCompressor;

	/**
	 * Checks for any completed asynchronous DXT compression tasks and finishes the texture creation.
	 * It will block until there are no more than 'NumUnfinishedTasksAllowed' tasks left unfinished.
	 *
	 * @param NumUnfinishedTasksAllowed		Maximum number of unfinished tasks to allow, before returning
	 */
	static void							FinishCompletedTasks( INT NumUnfinishedTasksAllowed );

	/** Helper class to keep track of the asynchronous tasks for the 4 lightmap textures. */
	struct FCoefficientTexture
	{
		/** Constructor */
		FCoefficientTexture()
		:	NumCompletedAsyncTasks(0)
		,	Texture(NULL)
		{
		}

		/** Destructor */
		~FCoefficientTexture()
		{
        }

		/** Owning container for the compression tasks for all mip-levels of a texture.	*/
		TIndirectArray<FLightMapAsyncTask>	MipTasks;
		/** Number of compression tasks that has completed so far.						*/
		INT							NumCompletedAsyncTasks;
		/** The lightmap texture.														*/
		ULightMapTexture2D*			Texture;
		/** The source data that is being compressed by the asynchronous tasks.			*/
		TArray< TArray<FColor> >	MipData;
	};

	/** Helper data to keep track of the asynchronous tasks for the 4 lightmap textures. */
	FCoefficientTexture				Textures[NUM_STORED_LIGHTMAP_COEF];

	TArray<FLightMapAllocation*>	Allocations;
	UMaterialInterface*				Material;
	UObject*						Outer;

	/** Bounding volume for all mappings within this texture.							*/
	FBoxSphereBounds				Bounds;

	/** Lightmap streaming flags that must match in order to be stored in this texture.	*/
	ELightMapFlags					LightmapFlags;

	/** Is this lightmap texture for a group of InstancedStaticMesh instances.			*/
	UBOOL							bInstancedStaticMesh;

	FLightMapPendingTexture(UINT InSizeX,UINT InSizeY)
		:	FTextureLayout(GAllowLightmapCompression ? GPixelFormats[PF_DXT1].BlockSizeX : GPixelFormats[PF_A8R8G8B8].BlockSizeX,
			GAllowLightmapCompression ? GPixelFormats[PF_DXT1].BlockSizeY : GPixelFormats[PF_A8R8G8B8].BlockSizeY,
			InSizeX,InSizeY,true)
		,	Bounds(0)
		,	LightmapFlags( LMF_None )
		,	bInstancedStaticMesh(FALSE)
	{}

	~FLightMapPendingTexture()
	{
	}

	/**
	 * Processes the 3 coefficient textures and the SimpleLightmap texture and starts
	 * asynchronous compression tasks for all mip-levels of the 4 textures.
	 */
	void StartEncoding();

	/**
	 * Creates and starts a new asynchronous compression task.
	 * If there are too currently on-going tasks, block until enough of them has finished.
	 *
	 * @param CoefficientIndex	Lightmap coefficient index
	 * @param MipIndex			Index of the mip-level we're compressing
	 * @param PixelFormat		Texture format
	 * @param MipSizeX			Number of texels along the X-axis for this mip-level
	 * @param MipSizeY			Number of texels along the Y-axis for this mip-level
	 * @param EncodeQualityLevel	A value from nvtt::Quality that represents the quality of the lightmap encoding
	 */
	void AddCompressionTask( UINT CoefficientIndex, INT MipIndex, EPixelFormat PixelFormat, UINT MipSizeX, UINT MipSizeY );

	/**
	 * Notifies the FLightMapPendingTexture that one of the mip-level compression tasks has finished.
	 * Once the tasks for all mip-levels has finished, it will call FinishEncoding().
	 *
	 * @param Task				The task the finished.
	 * @return					TRUE if FLightMapPendingTexture has fully completed all work and should be deleted.
	 */
	UBOOL NotifyTaskCompleted( FLightMapAsyncTask* Task );

	/**
	 * Called once the compression tasks for all mip-levels of a texture has finished.
	 * Copies the compressed data into each of the mip-levels of the texture and deletes the tasks.
	 *
	 * @param CoefficientIndex	Texture coefficient index, identifying the specific texture with this FLightMapPendingTexture.
	 */
	void FinishEncoding( UINT CoefficientIndex );

	/**
	 * Finds a free area in the texture large enough to contain a surface with the given size.
	 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
	 * set to the coordinates of the upper left corner of the free area and the function return true.
	 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
	 *
	 * If the allocation succeeded, Allocation.OffsetX and Allocation.OffsetY will be set to the upper-left corner
	 * of the area allocated.
	 *
	 * @param Allocation	Lightmap allocation to try to fit
	 * @param bForceIntoThisTexture	True if we should ignore distance and other factors when considering whether the mapping should be packed into this texture
	 *
	 * @return	True if succeeded, false otherwise.
	 */
	UBOOL AddElement(FLightMapAllocation& Allocation, const UBOOL bForceIntoThisTexture = FALSE );

private:
	FName GetLightmapName(INT TextureIndex, INT CoefficientIndex);
};

/** All mip-compression tasks: One task per mip-level, for each texture. */
TArray<FLightMapAsyncTask*>	FLightMapPendingTexture::TotalAsyncTasks;

FAsyncPVRTCCompressor FLightMapPendingTexture::PVRTCCompressor;

/**
 * Checks for any completed asynchronous DXT compression tasks and finishes the texture creation.
 * It will block until there are no more than 'NumUnfinishedTasksAllowed' tasks left unfinished.
 *
 * @param NumUnfinishedTasksAllowed		Maximum number of unfinished tasks to allow, before returning
 */
void FLightMapPendingTexture::FinishCompletedTasks( INT NumUnfinishedTasksAllowed )
{
	do
	{
		// Check for completed async compression tasks.
		for ( INT TaskIndex=0; TaskIndex < TotalAsyncTasks.Num(); )
		{
			FLightMapAsyncTask* Task = TotalAsyncTasks(TaskIndex);
			if ( Task->Task->IsDone() )
			{
				TotalAsyncTasks.RemoveSwap(TaskIndex);
				FLightMapPendingTexture* PendingTexture = Task->Texture;
				UBOOL bFullyCompleted = PendingTexture->NotifyTaskCompleted( Task );
				if ( bFullyCompleted )
				{
					delete PendingTexture;
				}
			}
			else
			{
				++TaskIndex;
			}
		}

		// If we still have too many unfinished tasks, wait for someone to finish.
		if ( TotalAsyncTasks.Num() > NumUnfinishedTasksAllowed )
		{
			appSleep(0.1f);
		}

	} while ( TotalAsyncTasks.Num() > NumUnfinishedTasksAllowed );
}


/**
 * Creates and starts a new asynchronous compression task.
 * If there are too currently on-going tasks, block until enough of them has finished.
 *
 * @param CoefficientIndex	Lightmap coefficient index
 * @param MipIndex			Index of the mip-level we're compressing
 * @param PixelFormat		Texture format
 * @param MipSizeX			Number of texels along the X-axis for this mip-level
 * @param MipSizeY			Number of texels along the Y-axis for this mip-level
 */
void FLightMapPendingTexture::AddCompressionTask( UINT CoefficientIndex, INT MipIndex, EPixelFormat PixelFormat, UINT MipSizeX, UINT MipSizeY )
{
	// Create and start the async DXT compression task.
	FCoefficientTexture& Texture = Textures[CoefficientIndex];

	FAsyncTask<FAsyncDXTCompress>* Task = new FAsyncTask<FAsyncDXTCompress>( Texture.MipData(MipIndex).GetData(), PixelFormat, MipSizeX, MipSizeY, TRUE, FALSE, FALSE, GLightmapEncodeQualityLevel );

	Task->StartBackgroundTask();

	// Create a new task wrapper.
	FLightMapAsyncTask* CompressionTask = new FLightMapAsyncTask( this, Task, CoefficientIndex, MipIndex );
	Texture.MipTasks.AddItem( CompressionTask );
	TotalAsyncTasks.AddItem( CompressionTask );
}

/**
 * Notifies the FLightMapPendingTexture that one of the mip-level compression tasks has finished.
 * Once the tasks for all mip-levels has finished, it will call FinishEncoding().
 *
 * @param Task				The task the finished.
 * @return					TRUE if FLightMapPendingTexture has fully completed all work and should be deleted.
 */
UBOOL FLightMapPendingTexture::NotifyTaskCompleted( FLightMapAsyncTask* Task )
{
	FCoefficientTexture& Texture = Textures[Task->CoefficientIndex];
	Texture.NumCompletedAsyncTasks++;
	if ( Texture.NumCompletedAsyncTasks == Texture.MipData.Num() )
	{
		FinishEncoding(Task->CoefficientIndex);
	}

	// Check to see if all textures have been fully completed.
	for ( INT Index=0; Index < NUM_STORED_LIGHTMAP_COEF; ++Index )
	{
		if ( Textures[Index].MipData.Num() > 0 )
		{
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Called once the compression tasks for all mip-levels of a texture has finished.
 * Copies the compressed data into each of the mip-levels of the texture and deletes the tasks.
 *
 * @param CoefficientIndex	Texture coefficient index, identifying the specific texture with this FLightMapPendingTexture.
 */
void FLightMapPendingTexture::FinishEncoding( UINT CoefficientIndex )
{
	FCoefficientTexture& CoefficientTexture = Textures[CoefficientIndex];
	UTexture2D* Texture2D = CoefficientTexture.Texture;

	for ( INT MipIndex=0; MipIndex < CoefficientTexture.MipTasks.Num(); ++MipIndex )
	{
		FLightMapAsyncTask& CompressionTask = CoefficientTexture.MipTasks( MipIndex );
		check( CompressionTask.CoefficientIndex == CoefficientIndex );
		check( CompressionTask.MipIndex == MipIndex );
		const void* CompressedData = CompressionTask.Task->GetTask().GetResultData();
		INT CompressedSize = CompressionTask.Task->GetTask().GetResultSize();
		check(CompressedData != NULL && CompressedSize > 0);

		// Add a mip level to Texture2D and copy over the compressed data
		FTexture2DMipMap* MipMap = new(Texture2D->Mips) FTexture2DMipMap;
		MipMap->SizeX = Max<UINT>(CompressionTask.Task->GetTask().GetSizeX(), GPixelFormats[CompressionTask.Task->GetTask().GetPixelFormat()].BlockSizeX);
		MipMap->SizeY = Max<UINT>(CompressionTask.Task->GetTask().GetSizeY(), GPixelFormats[CompressionTask.Task->GetTask().GetPixelFormat()].BlockSizeY);
		MipMap->Data.Lock(LOCK_READ_WRITE);
		INT NumElements = CompressedSize / MipMap->Data.GetElementSize();
		appMemcpy( MipMap->Data.Realloc(NumElements), CompressedData, CompressedSize );
		MipMap->Data.Unlock();
	}

	if ( CoefficientIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF )
	{
		INT TextureSize = Texture2D->CalcTextureMemorySize( TMC_AllMips );
		GLightmapTotalSize += TextureSize;
		GLightmapTotalSize360 += Texture2D->Get360Size( Texture2D->Mips.Num() );
		GLightmapTotalStreamingSize += (LightmapFlags & LMF_Streamed) ? TextureSize : 0;

		UPackage* TexturePackage = Texture2D->GetOutermost();
		for ( INT LevelIndex=0; TexturePackage && LevelIndex < GWorld->Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = GWorld->Levels(LevelIndex);
			UPackage* LevelPackage = Level->GetOutermost();
			if ( TexturePackage == LevelPackage )
			{
				Level->LightmapTotalSize += FLOAT(Texture2D->CalcTextureMemorySize( TMC_AllMips )) / 1024.0f;
				break;
			}
		}
	}

	// Check to see if we should compress lightmaps to PVRTC
	// Cache PVRTC Textures if we are in cooking for a mobile platform or were asked for always optimize for mobile
	UE3::EPlatformType Platform = ParsePlatformType( appCmdLine() );
	UBOOL bShouldCachePVRTCTextures = ( Platform & UE3::PLATFORM_Mobile ) || GAlwaysOptimizeContentForMobile;

	// If in other mode, check the config file for this setting
	if( !bShouldCachePVRTCTextures )
	{
		GConfig->GetBool(TEXT("MobileSupport"), TEXT("bShouldCachePVRTCTextures"), bShouldCachePVRTCTextures, GEngineIni);
	}

	// Compress PVRTC textures if the lighting build quality is production, we are allowed to compress pvrtc textures and if we are encoding a simple lightmap.
	if( GLightingBuildQuality == Quality_Production && bShouldCachePVRTCTextures && CoefficientIndex == SIMPLE_LIGHTMAP_COEF_INDEX )
	{
		PVRTCCompressor.AddTexture( Texture2D, FALSE, CoefficientTexture.MipData(0).GetData() );
	}

	// Free all uncompressed source data for this coefficient texture.
	CoefficientTexture.MipData.Empty();

	// Delete all mip tasks.
	CoefficientTexture.MipTasks.Empty();

	// Update the texture resource.
	Texture2D->UpdateResource();
}

/** Whether to try to pack procbuilding lightmaps/shadowmaps into the same texture. */
UBOOL GGroupComponentLightmaps = TRUE;

/**
 * Finds a free area in the texture large enough to contain a surface with the given size.
 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
 * set to the coordinates of the upper left corner of the free area and the function return true.
 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
 *
 * If the allocation succeeded, Allocation.OffsetX and Allocation.OffsetY will be set to the upper-left corner
 * of the allocated area.
 *
 * @param Allocation	Lightmap allocation to try to fit
 * @param bForceIntoThisTexture	True if we should ignore distance and other factors when considering whether the mapping should be packed into this texture
 *
 * @return	True if succeeded, false otherwise.
 */
UBOOL FLightMapPendingTexture::AddElement(FLightMapAllocation& Allocation, UBOOL bForceIntoThisTexture )
{
	if( !bForceIntoThisTexture )
	{
		// Don't pack lightmaps from different packages into the same texture.
		if ( Outer != Allocation.Outer )
		{
			return FALSE;
		}

		// InstancedStaticMeshComponent lightmaps must be stored in InstancedStaticMeshComponent lightmap textures.
		if (Allocation.bInstancedStaticMesh && !bInstancedStaticMesh)
		{
			return FALSE;
		}

		// InstancedStaticMeshComponent lightmap textures must grouped instances from the same primitive.
		if ( bInstancedStaticMesh && Allocations.Num() > 0 && Allocations(0)->Primitive != Allocation.Primitive)
		{
			return FALSE;
		}
	}

	const FBoxSphereBounds NewBounds = Bounds + Allocation.Bounds;
	const UBOOL bEmptyTexture = Allocations.Num() == 0;

	if ( !bEmptyTexture && !bForceIntoThisTexture )
	{
		// Don't mix streaming lightmaps with non-streaming lightmaps.
		if ( (LightmapFlags & LMF_Streamed) != (Allocation.LightmapFlags & LMF_Streamed) )
		{
			return FALSE;
		}

		// Is this a streaming lightmap?
		if ( LightmapFlags & LMF_Streamed )
		{
			UBOOL bPerformDistanceCheck = TRUE;

			// Try to group lightmaps from the same component into the same texture (by disregarding the distance check)
			UBOOL bTryGrouping = Allocation.Primitive->IsA( UInstancedStaticMeshComponent::StaticClass() ) ? TRUE : FALSE;
			if ( GGroupComponentLightmaps && bTryGrouping )
			{
				for ( INT MemberIndex=0; MemberIndex < Allocations.Num(); ++MemberIndex )
				{
					UObject* MemberPrimitive = Allocations( MemberIndex )->Primitive;
					UBOOL bHasGroupCandidate = MemberPrimitive->IsA( UInstancedStaticMeshComponent::StaticClass() ) ? TRUE : FALSE;
					if ( bHasGroupCandidate && (Allocation.Primitive == MemberPrimitive || Allocation.Primitive->GetOuter() == MemberPrimitive->GetOuter()) )
					{
						bPerformDistanceCheck = FALSE;
						break;
					}
				}
			}

			// Don't pack together lightmaps that are too far apart
			if ( bPerformDistanceCheck && NewBounds.SphereRadius > GMaxLightmapRadius && NewBounds.SphereRadius > (Bounds.SphereRadius + SMALL_NUMBER) )
			{
				return FALSE;
			}
		}
	}



	// Whether we should combine mappings that are either exactly the same or very similar.
	// This requires some extra computation but may greatly reduce the memory used.
	const UBOOL bCombineSimilarMappings = GEngine->bCombineSimilarMappings || bInstancedStaticMesh;
	const DOUBLE MaxAllowedRMSDForCombine = GEngine->MaxRMSDForCombiningMappings;

	UBOOL bFoundExistingMapping = FALSE;
	if( bCombineSimilarMappings ) 
	{
		// Check to see if this allocation closely matches an existing light map allocation.  If they're
		// almost exactly the same then we'll discard this allocation and use the existing mapping.
		for( INT PackedLightMapIndex = 0; PackedLightMapIndex < Allocations.Num(); ++PackedLightMapIndex )						
		{
			const FLightMapAllocation& PackedAllocation = *Allocations( PackedLightMapIndex );

			if( PackedAllocation.MappedRect == Allocation.MappedRect &&
				PackedAllocation.LightmapFlags == Allocation.LightmapFlags &&
				PackedAllocation.PaddingType == Allocation.PaddingType &&
				PackedAllocation.Scale == Allocation.Scale /*&&
				PackedAllocation.Bounds.Origin == Allocation.Bounds.Origin &&
				PackedAllocation.Bounds.SphereRadius == Allocation.Bounds.SphereRadius &&
				PackedAllocation.Bounds.BoxExtent == Allocation.Bounds.BoxExtent */ )
			{
			    if( PackedAllocation.RawData == Allocation.RawData )
			    {
				    // Light maps are the same!
				    bFoundExistingMapping = TRUE;
				    Allocation.OffsetX = PackedAllocation.OffsetX;
				    Allocation.OffsetY = PackedAllocation.OffsetY;
					Allocation.bSkipEncoding = TRUE;
    
				    // warnf( TEXT( "LightMapPacking: Exact Shared Allocation (%i texels)" ), Allocation.MappedRect.Area() );
				    break;
			    }

			    // Images aren't an exact match, but check to see if they're close enough
			    if( Allocation.RawData.Num() > 0 &&
				    PackedAllocation.RawData.Num() == Allocation.RawData.Num() )
			    {
				    // Compute the difference between the two images
				    // @todo: Should compute RMSD per channel and use a separate max RMSD threshold for each
				    static TArray< DOUBLE > ValueDifferences;	// Static to reduce heap alloc thrashing
				    ValueDifferences.Reset();
				    {
					    const BYTE* BytesArrayA = (BYTE*)PackedAllocation.RawData.GetData();
					    const BYTE* BytesArrayB = (BYTE*)Allocation.RawData.GetData();
					    const INT ByteCount = PackedAllocation.RawData.GetTypeSize() * PackedAllocation.RawData.Num();
					    TextureLayoutTools::ComputeDifferenceArray( BytesArrayA, BytesArrayB, ByteCount, ValueDifferences );
				    }
    
				    // Compute the root mean square deviation for the difference image
				    const DOUBLE RMSD = TextureLayoutTools::ComputeRootMeanSquareDeviation( ValueDifferences.GetData(), ValueDifferences.Num() );
				    if( RMSD <= MaxAllowedRMSDForCombine )
				    {
					    // Light maps are close enough!
					    bFoundExistingMapping = TRUE;
					    Allocation.OffsetX = PackedAllocation.OffsetX;
					    Allocation.OffsetY = PackedAllocation.OffsetY;
						Allocation.bSkipEncoding = TRUE;
    
					    // warnf( TEXT( "LightMapPacking: Approx Shared Allocation (RMSD %f) (%i texels)" ), RMSD, Allocation.MappedRect.Area() );
					    break;
				    }
			    }
			}
		}
	}

	if( !bFoundExistingMapping )
	{
		UINT BaseX = 0;
		UINT BaseY = 0;
		if( !FTextureLayout::AddElement( BaseX, BaseY, Allocation.MappedRect.Width(), Allocation.MappedRect.Height() ) )
		{
			return FALSE;
		}

		// warnf( TEXT( "LightMapPacking: New Allocation (%i texels)" ), Allocation.MappedRect.Area() );

		// Save the position the light-maps (the Allocation.MappedRect portion) in the texture atlas.
		Allocation.OffsetX = BaseX;
		Allocation.OffsetY = BaseY;
		Bounds = bEmptyTexture ? Allocation.Bounds : NewBounds;
	}

	return TRUE;
}

/** Whether to color each lightmap texture with a different (random) color. */
UBOOL GVisualizeLightmapTextures = FALSE;

/**
 * Processes the 3 coefficient textures and the SimpleLightmap texture and starts
 * asynchronous compression tasks for all mip-levels of the 4 textures.
 */
void FLightMapPendingTexture::StartEncoding()
{
	GLightmapCounter++;

	FColor TextureColor;
	if ( GVisualizeLightmapTextures )
	{
		TextureColor = FColor::MakeRandomColor();
	}

	// Encode and compress the coefficient textures.
	for(UINT CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex++)
	{
		// Skip generating simple lightmaps if wanted.
		if( !GEngine->bShouldGenerateSimpleLightmaps && CoefficientIndex == SIMPLE_LIGHTMAP_COEF_INDEX )
		{
			continue;
		}

		// Create the light-map texture for this coefficient.
		FCoefficientTexture& CoefficientTexture = Textures[CoefficientIndex];
		ULightMapTexture2D* Texture = new(Outer, GetLightmapName(GLightmapCounter, CoefficientIndex)) ULightMapTexture2D;
		CoefficientTexture.Texture = Texture;
		Texture->SizeX		= GetSizeX();
		Texture->SizeY		= GetSizeY();
		Texture->Filter		= GUseBilinearLightmaps ? TF_Linear : TF_Nearest;
		Texture->Format		= GAllowLightmapCompression ? PF_DXT1 : PF_A8R8G8B8;
		Texture->LODGroup	= TEXTUREGROUP_Lightmap;
		DWORD SimpleLightmapFlag = (CoefficientIndex == SIMPLE_LIGHTMAP_COEF_INDEX) ? LMF_SimpleLightmap : LMF_None;
		Texture->LightmapFlags = ELightMapFlags( SimpleLightmapFlag | LightmapFlags );
		Texture->GenerateTextureFileCacheGUID(TRUE);

		UINT MaxNumMips = Max(appCeilLogTwo(Texture->SizeX),appCeilLogTwo(Texture->SizeY)) + 1;
		UINT NumMips = 1;
		if (GAllowLightmapCompression)
		{
			NumMips = MaxNumMips;
		}
		else
		{
			//@todo. Fix this case. When not compressing light maps, there is no lower mip data!
			// This is due to the compression code actually generating the mips.
		}

		// Create the uncompressed top mip-level.
		TArray<FColor>* TopMipData = new(CoefficientTexture.MipData) TArray<FColor>();
		TopMipData->Empty(GetSizeX() * GetSizeY());
		TopMipData->AddZeroed(GetSizeX() * GetSizeY());
		FIntRect TextureRect( MAXINT, MAXINT, MININT, MININT );
		for(INT AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
		{
			FLightMapAllocation* Allocation = Allocations(AllocationIndex);
			// Link the light-map to the texture.
			Allocation->LightMap->Textures[CoefficientIndex] = Texture;
			Allocation->LightMap->ScaleVectors[CoefficientIndex] = FVector4(
				Allocation->Scale[CoefficientIndex][0],
				Allocation->Scale[CoefficientIndex][1],
				Allocation->Scale[CoefficientIndex][2],
				1.0f
				);

			// Skip encoding of this texture if we were asked not to bother
			if( !Allocation->bSkipEncoding )
			{
			    TextureRect.Min.X = Min<INT>( TextureRect.Min.X, Allocation->OffsetX );
			    TextureRect.Min.Y = Min<INT>( TextureRect.Min.Y, Allocation->OffsetY );
			    TextureRect.Max.X = Max<INT>( TextureRect.Max.X, Allocation->OffsetX + Allocation->MappedRect.Width() );
			    TextureRect.Max.Y = Max<INT>( TextureRect.Max.Y, Allocation->OffsetY + Allocation->MappedRect.Height() );
    
			    // Copy the raw data for this light-map into the raw texture data array.
			    for(INT Y = Allocation->MappedRect.Min.Y; Y < Allocation->MappedRect.Max.Y; ++Y)
			    {
				    for(INT X = Allocation->MappedRect.Min.X; X < Allocation->MappedRect.Max.X; ++X)
				    {
					    INT DestY = Y - Allocation->MappedRect.Min.Y + Allocation->OffsetY;
					    INT DestX = X - Allocation->MappedRect.Min.X + Allocation->OffsetX;
					    FColor& DestColor = (*TopMipData)(DestY * Texture->SizeX + DestX);
					    const FLightMapCoefficients& SourceCoefficients = Allocation->RawData(Y * Allocation->TotalSizeX + X);
#if VISUALIZE_PACKING						
					    if( X == Allocation->MappedRect.Min.X || Y == Allocation->MappedRect.Min.Y ||
						    X == Allocation->MappedRect.Max.X-1 || Y == Allocation->MappedRect.Max.Y-1 ||
						    X == Allocation->MappedRect.Min.X+1 || Y == Allocation->MappedRect.Min.Y+1 ||
						    X == Allocation->MappedRect.Max.X-2 || Y == Allocation->MappedRect.Max.Y-2 )
					    {
						    DestColor = FColor(255,0,0);
					    }
					    else
					    {
						    DestColor = FColor(0,255,0);
					    }
#else
					    DestColor.R = SourceCoefficients.Coefficients[CoefficientIndex][0];
					    DestColor.G = SourceCoefficients.Coefficients[CoefficientIndex][1];
					    DestColor.B = SourceCoefficients.Coefficients[CoefficientIndex][2];

					    if ( GVisualizeLightmapTextures )
					    {
						    DestColor = TextureColor;
					    }

					    DestColor.A = SourceCoefficients.Coverage;
					    if ( SourceCoefficients.Coverage > 0 )
					    {
						    GNumLightmapMappedTexels++;
					    }
					    else
					    {
						    GNumLightmapUnmappedTexels++;
					    }

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && !CONSOLE
					    INT PaddedX = X;
					    INT PaddedY = Y;
					    if (GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
					    {
						    if (Allocation->TotalSizeX - 2 > 0 && Allocation->TotalSizeY - 2 > 0)
						    {
							    PaddedX -= 1;
							    PaddedY -= 1;
						    }
					    }

					    if (Allocation->bDebug
						    && PaddedX == GCurrentSelectedLightmapSample.LocalX
						    && PaddedY == GCurrentSelectedLightmapSample.LocalY)
					    {
						    GCurrentSelectedLightmapSample.OriginalColor = DestColor;
						    extern FColor GTexelSelectionColor;
						    DestColor = GTexelSelectionColor;
					    }
#endif
#endif
				    }
			    }
		    }
		}

		GNumLightmapTotalTexels += Texture->SizeX * Texture->SizeY;
		GNumLightmapTotalTexelsNonPow2 += TextureRect.Width() * TextureRect.Height();
		GNumLightmapTextures++;

		// for simple lightmaps, copy the top level into source art
		if (CoefficientIndex == SIMPLE_LIGHTMAP_COEF_INDEX)
		{
			check(TopMipData);
			FPNGHelper PNG;
			PNG.InitRaw( TopMipData->GetData(), TopMipData->GetTypeSize() * TopMipData->Num(), GetSizeX(), GetSizeY());
			const TArray<BYTE>& CompressedData = PNG.GetCompressedData();
			check( CompressedData.Num() );

			// We have compressed source art, now store it.
			CoefficientTexture.Texture->SetCompressedSourceArt(CompressedData.GetData(), CompressedData.Num());
			CoefficientTexture.Texture->OriginalSizeX = GetSizeX();
			CoefficientTexture.Texture->OriginalSizeY = GetSizeY();
		}

		for(UINT MipIndex = 1; MipIndex < NumMips; MipIndex++)
		{
			const UINT SourceMipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> (MipIndex - 1));
			const UINT SourceMipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> (MipIndex - 1));
			const UINT DestMipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
			const UINT DestMipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);

			// Downsample the previous mip-level, taking into account which texels are mapped.
			TArray<FColor>* NextMipData = new(CoefficientTexture.MipData) TArray<FColor>();
			const TArray<FColor>& LastMipData = CoefficientTexture.MipData(MipIndex - 1);
			NextMipData->Empty(DestMipSizeX * DestMipSizeY);
			NextMipData->Add(DestMipSizeX * DestMipSizeY);

			const UINT MipFactorX = SourceMipSizeX / DestMipSizeX;
			const UINT MipFactorY = SourceMipSizeY / DestMipSizeY;

			for(UINT Y = 0; Y < DestMipSizeY; Y++)
			{
				for(UINT X = 0; X < DestMipSizeX; X++)
				{
					FLinearColor AccumulatedColor = FLinearColor::Black;
					UINT Coverage = 0;

					const UINT MinSourceY = (Y + 0) * MipFactorY;
					const UINT MaxSourceY = (Y + 1) * MipFactorY;
					for(UINT SourceY = MinSourceY; SourceY < MaxSourceY; SourceY++)
					{
						const UINT MinSourceX = (X + 0) * MipFactorX;
						const UINT MaxSourceX = (X + 1) * MipFactorX;
						for(UINT SourceX = MinSourceX; SourceX < MaxSourceX; SourceX++)
						{
							const FColor& SourceColor = LastMipData(SourceY * SourceMipSizeX + SourceX);
							if(SourceColor.A)
							{
								AccumulatedColor += FLinearColor(SourceColor) * SourceColor.A;
								Coverage += SourceColor.A;
							}
						}
					}
					FColor& DestColor = (*NextMipData)(Y * DestMipSizeX + X);
					if ( GVisualizeLightmapTextures )
					{
						DestColor = TextureColor;
					}
					else if(Coverage)
					{
						FColor AverageColor(AccumulatedColor / Coverage);
						DestColor = FColor(AverageColor.R,AverageColor.G,AverageColor.B,Coverage / (MipFactorX * MipFactorY));
					}
					else
					{
						DestColor = (*NextMipData)(Y * DestMipSizeX + X) = FColor(0,0,0,0);
					}
				}
			}
		}

		// Expand texels which are mapped into adjacent texels which are not mapped to avoid artifacts when using texture filtering.
		for(INT MipIndex = 0; MipIndex < CoefficientTexture.MipData.Num(); MipIndex++)
		{
			TArray<FColor>& MipLevelData = CoefficientTexture.MipData(MipIndex);
			UINT MipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
			UINT MipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);
			for(UINT DestY = 0;DestY < MipSizeY;DestY++)
			{
				for(UINT DestX = 0; DestX < MipSizeX; DestX++)
				{
					FColor& DestColor = MipLevelData(DestY * MipSizeX + DestX);
					if(DestColor.A == 0)
					{
						FLinearColor AccumulatedColor = FLinearColor::Black;
						UINT Coverage = 0;

						const INT MinSourceY = Max((INT)DestY - 1, (INT)0);
						const INT MaxSourceY = Min((INT)DestY + 1, (INT)MipSizeY);
						for(INT SourceY = MinSourceY; SourceY < MaxSourceY; SourceY++)
						{
							const INT MinSourceX = Max((INT)DestX - 1, (INT)0);
							const INT MaxSourceX = Min((INT)DestX + 1, (INT)MipSizeX);
							for(INT SourceX = MinSourceX; SourceX < MaxSourceX; SourceX++)
							{
								FColor& SourceColor = MipLevelData(SourceY * MipSizeX + SourceX);
								if(SourceColor.A)
								{
									static const UINT Weights[3][3] =
									{
										{ 1, 255, 1 },
										{ 255, 0, 255 },
										{ 1, 255, 1 },
									};
									AccumulatedColor += FLinearColor(SourceColor) * SourceColor.A * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
									Coverage += SourceColor.A * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
								}
							}
						}

						if(Coverage)
						{
							DestColor = FColor(AccumulatedColor / Coverage);
							DestColor.A = 0;
						}
					}
				}
			}
		}

		CoefficientTexture.MipTasks.Reserve( CoefficientTexture.MipData.Num() );
		for(INT MipIndex = 0; MipIndex < CoefficientTexture.MipData.Num(); MipIndex++)
		{
			UINT MipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
			UINT MipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);
			EPixelFormat PixelFormat = GAllowLightmapCompression ? PF_DXT1 : PF_A8R8G8B8;
			AddCompressionTask( CoefficientIndex, MipIndex, PixelFormat, MipSizeX, MipSizeY );
		}
	}

	for(INT AllocationIndex = 0; AllocationIndex < Allocations.Num(); AllocationIndex++)
	{
		FLightMapAllocation* Allocation = Allocations(AllocationIndex);

		INT PaddedSizeX = Allocation->TotalSizeX;
		INT PaddedSizeY = Allocation->TotalSizeY;
		INT BaseX = Allocation->OffsetX - Allocation->MappedRect.Min.X;
		INT BaseY = Allocation->OffsetY - Allocation->MappedRect.Min.Y;
#if !CONSOLE
		if (GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
		{
			if ((PaddedSizeX - 2 > 0) && ((PaddedSizeY - 2) > 0))
			{
				PaddedSizeX -= 2;
				PaddedSizeY -= 2;
				BaseX += 1;
				BaseY += 1;
			}
		}
#endif	//#if !CONSOLE
		
		// Calculate the coordinate scale/biases this light-map.
		FVector2D Scale((FLOAT)PaddedSizeX / (FLOAT)GetSizeX(), (FLOAT)PaddedSizeY / (FLOAT)GetSizeY());
		FVector2D Bias((FLOAT)BaseX / (FLOAT)GetSizeX(), (FLOAT)BaseY / (FLOAT)GetSizeY());

		// let the lightmap finish up after being encoded, setting the scale/bias, and returning if it can be deleted
		check( Allocation->LightMap );
		Allocation->LightMap->FinalizeEncoding(Scale, Bias, Textures[0].Texture);

		// Free the light-map's raw data.
		Allocation->RawData.Empty();
	}
}

FName FLightMapPendingTexture::GetLightmapName(INT TextureIndex, INT CoefficientIndex)
{
	check(CoefficientIndex >= 0 && CoefficientIndex < NUM_STORED_LIGHTMAP_COEF);
	FString PotentialName = TEXT("");
	UObject* ExistingObject = NULL;
	INT LightmapIndex = 0;
	// Search for an unused name
	do
	{
		if (CoefficientIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF)
		{
			checkAtCompileTime(NUM_DIRECTIONAL_LIGHTMAP_COEF == 2, CodeAssumesTwoDirectionalCoefficients);
			if (CoefficientIndex == 0)
			{
				PotentialName = FString(TEXT("NormalizedAverageColor")) + appItoa(LightmapIndex) + TEXT("_") + appItoa(TextureIndex);
			}
			else
			{
				PotentialName = FString(TEXT("DirectionalMaxComponent")) + appItoa(LightmapIndex) + TEXT("_") + appItoa(TextureIndex);
			}
		}
		else
		{
			PotentialName = FString(TEXT("SimpleLightmap")) + TEXT("_") + appItoa(LightmapIndex) + TEXT("_") + appItoa(TextureIndex);
		}
		ExistingObject = FindObject<UObject>(Outer, *PotentialName);
		LightmapIndex++;
	}
	while (ExistingObject != NULL);
	return FName(*PotentialName);
}

/** The light-maps which have not yet been encoded into textures. */
static TIndirectArray<FLightMapAllocation> PendingLightMaps;
static UINT PendingLightMapSize = 0;

void ApplySimpleLightmapModification( FLightMapAllocation* Allocation )
{
#if WITH_EDITORONLY_DATA
	// For simple lightmaps, we may have an optional texture used to modulate them
	if( Allocation->Primitive->GetClass() == UStaticMeshComponent::StaticClass() )
	{
		UStaticMeshComponent* StaticMeshPrimitive = (UStaticMeshComponent*)Allocation->Primitive;
		if( StaticMeshPrimitive->bUseSimpleLightmapModifications &&
			StaticMeshPrimitive->SimpleLightmapModificationTexture &&
			StaticMeshPrimitive->SimpleLightmapModificationTexture->GetClass() == UTexture2D::StaticClass() )
		{
			UTexture2D* LightmapModificationTexture2D = (UTexture2D*)StaticMeshPrimitive->SimpleLightmapModificationTexture;

			// Only textures that have been set up properly as lightmap modification textures can be used.
			// A map-check is also in place to warn users that any texture used for lightmap modification
			// must be marked as such.
			if( LightmapModificationTexture2D->CompressionSettings == TC_SimpleLightmapModification )
			{
				FTextureMipBulkData* RawTextureBulkData = &(LightmapModificationTexture2D->Mips(0).Data);
				if( RawTextureBulkData->GetBulkDataSize() > 0 )
				{
					BYTE* RawTextureData = (BYTE*)RawTextureBulkData->Lock( LOCK_READ_ONLY );

					// Get the texels into floats
					TArray<FLOAT> FinalTextureData( RawTextureBulkData->GetBulkDataSize() );
					for( INT i = 0; i < RawTextureBulkData->GetBulkDataSize(); i++ )
					{
						FinalTextureData(i) = (FLOAT)(RawTextureData[i] / 255.0f);
					}
					// Done with the original data now
					RawTextureBulkData->Unlock();

					INT SrcW = LightmapModificationTexture2D->GetOriginalSurfaceWidth();
					INT SrcH = LightmapModificationTexture2D->GetOriginalSurfaceHeight();
					INT DstW = Allocation->TotalSizeX;
					INT DstH = Allocation->TotalSizeY;

					// Do we need to scale the modulation texture?
					FLOAT StepSizeX = SrcW / (FLOAT)DstW;
					FLOAT StepSizeY = SrcH / (FLOAT)DstH;
					if( StepSizeX != 1.0f || StepSizeY != 1.0f )
					{
						TArray<FLOAT> FinalTextureDataCopy( FinalTextureData );
						FinalTextureData.Reset( DstW * DstH * 4 );
						FinalTextureData.Add( DstW * DstH * 4 );
						FLOAT SrcX = 0.0f;
						FLOAT SrcY = 0.0f;
						INT SrcPixelPos = 0;
						INT DstPixelPos = 0;

						for( INT Y = 0; Y < DstH; Y++ )
						{
							SrcX = 0.0f;
							DstPixelPos = Y * DstW;
							for( INT X = 0; X < DstW; X++ )
							{
								INT PixelCount = 0;
								FLOAT EndX = SrcX + StepSizeX;
								FLOAT EndY = SrcY + StepSizeY;

								// Generate a rectangular region of pixels and then find the average color of the region.
								INT PosY = Clamp<INT>( appTrunc( SrcY + 0.5f ), 0, (SrcH - 1) );
								INT PosX = Clamp<INT>( appTrunc( SrcX + 0.5f ), 0, (SrcW - 1) );
								INT EndPosY = Clamp<INT>( appTrunc( EndY + 0.5f ), 0, (SrcH - 1) );
								INT EndPosX = Clamp<INT>( appTrunc( EndX + 0.5f ), 0, (SrcW - 1) );

								FLOAT ColorSum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
								for(INT PixelY = PosY; PixelY <= EndPosY; PixelY++)
								{
									for(INT PixelX = PosX; PixelX <= EndPosX; PixelX++)
									{
										SrcPixelPos = PixelY * SrcW + PixelX;
										ColorSum[0] += FinalTextureDataCopy(SrcPixelPos * 4 + 0);
										ColorSum[1] += FinalTextureDataCopy(SrcPixelPos * 4 + 1);
										ColorSum[2] += FinalTextureDataCopy(SrcPixelPos * 4 + 2);
										ColorSum[3] += FinalTextureDataCopy(SrcPixelPos * 4 + 3);
										PixelCount++;
									}
								}
								FinalTextureData(DstPixelPos * 4 + 0) = ColorSum[0] / (FLOAT)PixelCount;
								FinalTextureData(DstPixelPos * 4 + 1) = ColorSum[1] / (FLOAT)PixelCount;
								FinalTextureData(DstPixelPos * 4 + 2) = ColorSum[2] / (FLOAT)PixelCount;
								FinalTextureData(DstPixelPos * 4 + 3) = ColorSum[3] / (FLOAT)PixelCount;

								SrcX += StepSizeX;
								DstPixelPos++;
							}
							SrcY += StepSizeY;
						}
					}

					if( StaticMeshPrimitive->SimpleLightmapModificationFunction == UStaticMeshComponent::MLMF_ModulateAlpha )
					{
						// Pre-scale the RGB by the computed Alpha
						for( INT T = 0; T < DstW * DstH * 4; T+=4 )
						{
							FLOAT Alpha = FinalTextureData(T + 3);
							FinalTextureData(T + 0) *= Alpha;
							FinalTextureData(T + 1) *= Alpha;
							FinalTextureData(T + 2) *= Alpha;
						}
					}

					// Modulate the lightmap
					for( INT T = 0; T < DstW * DstH; T++ )
					{
						FLightMapCoefficients& DestCoefficients = Allocation->RawData(T);
						// Modification texture format is BGRA
						DestCoefficients.Coefficients[SIMPLE_LIGHTMAP_COEF_INDEX][0] *= FinalTextureData(T * 4 + 2);
						DestCoefficients.Coefficients[SIMPLE_LIGHTMAP_COEF_INDEX][1] *= FinalTextureData(T * 4 + 1);
						DestCoefficients.Coefficients[SIMPLE_LIGHTMAP_COEF_INDEX][2] *= FinalTextureData(T * 4 + 0);
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

/** If TRUE, update the status when encoding light maps */
UBOOL FLightMap2D::bUpdateStatus = TRUE;

FLightMap2D* FLightMap2D::AllocateLightMap(UObject* LightMapOuter,FLightMapData2D*& RawData, FQuantizedLightmapData*& SourceQuantizedData, UMaterialInterface* Material, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags InLightmapFlags )
{
	// If the light-map has no lights in it, return NULL.
	if(!RawData && !SourceQuantizedData)
	{
		return NULL;
	}

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

	// keep track of whether or not we need to quantize the RawData
	UBOOL bNeedsToQuantize = FALSE;

	FLightMapAllocation* Allocation;
	if (SourceQuantizedData == NULL)
	{
		Allocation = new(PendingLightMaps) FLightMapAllocation;
		bNeedsToQuantize = TRUE;
	}
	else
	{
		Allocation = new(PendingLightMaps) FLightMapAllocation(SourceQuantizedData);
	}

	Allocation->Material	= Material;
	Allocation->Outer		= LightMapOuter->GetOutermost();
	Allocation->PaddingType	= InPaddingType;
	Allocation->LightmapFlags = InLightmapFlags;
	Allocation->Bounds		= Bounds;
	Allocation->Primitive	= LightMapOuter;
	if ( !GAllowStreamingLightmaps )
	{
		Allocation->LightmapFlags = ELightMapFlags( Allocation->LightmapFlags & ~LMF_Streamed );
	}

	// Create a new light-map.
	FLightMap2D* LightMap = new FLightMap2D(bNeedsToQuantize ? RawData->LightGuids : SourceQuantizedData->LightGuids);
	Allocation->LightMap = LightMap;

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && !CONSOLE
	// Detect if this allocation belongs to the texture mapping that was being debugged
	//@todo - this only works for mappings that can be uniquely identified by a single component, BSP for example does not work.
	if (GCurrentSelectedLightmapSample.Component && GCurrentSelectedLightmapSample.Component == LightMapOuter)
	{
		GCurrentSelectedLightmapSample.Lightmap = LightMap;
		Allocation->bDebug = TRUE;
	}
	else
	{
		Allocation->bDebug = FALSE;
	}
#endif

	// Quantized data is no longer needed now that FLightMapAllocation has made a copy of it
	if (SourceQuantizedData != NULL)
	{
		delete SourceQuantizedData;
		SourceQuantizedData = NULL;
	}

	if (bNeedsToQuantize)
	{
		// Calculate the range of each coefficient in this light-map.
		FLOAT MaxCoefficient[NUM_STORED_LIGHTMAP_COEF][3];

		// calc the max
		for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
			{
				MaxCoefficient[CoefficientIndex][ColorIndex] = 0;
			}
		}
		for(UINT Y = 0;Y < RawData->GetSizeY();Y++)
		{
			for(UINT X = 0;X < RawData->GetSizeX();X++)
			{
				const FLightSample& SourceSample = (*RawData)(X,Y);
				if(SourceSample.bIsMapped)
				{
					for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
					{
						for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
						{
							MaxCoefficient[CoefficientIndex][ColorIndex] = Clamp(
								SourceSample.Coefficients[CoefficientIndex][ColorIndex],
								MaxCoefficient[CoefficientIndex][ColorIndex],
								MAX_LIGHT_INTENSITY
								);
						}
					}
				}
			}
		}

		// For simple lightmaps, always use 2x lightmap scaling for now
		for (INT ColorIndex = 0; ColorIndex < 3; ColorIndex++)
		{
			MaxCoefficient[SIMPLE_LIGHTMAP_COEF_INDEX][ColorIndex] = 2.0f;
		}

		// Calculate the scale/bias for the light-map coefficients.
		FLOAT InvCoefficientScale[NUM_STORED_LIGHTMAP_COEF][3];
		for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
			{
				Allocation->Scale[CoefficientIndex][ColorIndex] = MaxCoefficient[CoefficientIndex][ColorIndex];
				InvCoefficientScale[CoefficientIndex][ColorIndex] = 1.0f / Max<FLOAT>(MaxCoefficient[CoefficientIndex][ColorIndex],DELTA);
			}
		}

		// Quantize the coefficients for this texture.
		Allocation->TotalSizeX = RawData->GetSizeX();
		Allocation->TotalSizeY = RawData->GetSizeY();
		Allocation->MappedRect = FIntRect( 0, 0, Allocation->TotalSizeX, Allocation->TotalSizeY );
		Allocation->RawData.Empty(RawData->GetSizeX() * RawData->GetSizeY());
		Allocation->RawData.Add(RawData->GetSizeX() * RawData->GetSizeY());
		for(UINT Y = 0;Y < RawData->GetSizeY();Y++)
		{
			for(UINT X = 0;X < RawData->GetSizeX();X++)
			{
				const FLightSample& SourceSample = (*RawData)(X,Y);
				FLightMapCoefficients& DestCoefficients = Allocation->RawData(Y * RawData->GetSizeX() + X);

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && !CONSOLE
				if (Allocation->bDebug)
				{
					INT PaddedX = X;
					INT PaddedY = Y;
					if (GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
					{
						if (Allocation->TotalSizeX - 2 > 0 && Allocation->TotalSizeY - 2 > 0)
						{
							PaddedX -= 1;
							PaddedY -= 1;
						}
					}

					if (PaddedX == GCurrentSelectedLightmapSample.LocalX
						&& PaddedY == GCurrentSelectedLightmapSample.LocalY)
					{
						INT TempBreak = 0;
					}
				}
#endif
				DestCoefficients.Coverage = SourceSample.bIsMapped ? 255 : 0;
				for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
				{
					for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
					{
						DestCoefficients.Coefficients[CoefficientIndex][ColorIndex] = (BYTE)Clamp<INT>(
							appTrunc(
								appPow(
									SourceSample.Coefficients[CoefficientIndex][ColorIndex] * InvCoefficientScale[CoefficientIndex][ColorIndex],
									1.0f / 2.2f
									) * 255.0f
								),
							0,
							255
						);
					}
				}
			}
		}
	}

	// RawData is no longer needed now it has been quantized
	delete RawData;
	RawData = NULL;

	// For simple lightmaps, we may have an optional texture to modulate in
	ApplySimpleLightmapModification( Allocation );

	// Track the size of pending light-maps.
	PendingLightMapSize += ((Allocation->TotalSizeX + 3) & ~3) * ((Allocation->TotalSizeY + 3) & ~3);

	// If we're allowed to eagerly encode lightmaps, check to see if we're ready
	if (GAllowEagerLightmapEncode)
	{
		// Once there are enough pending light-maps, flush encoding.
		const UINT PackedLightAndShadowMapTextureSize = GWorld->GetWorldInfo()->PackedLightAndShadowMapTextureSize;
		const UINT MaxPendingLightMapSize = Square(PackedLightAndShadowMapTextureSize) * 4;
		if(PendingLightMapSize >= MaxPendingLightMapSize)
		{
			EncodeTextures( TRUE, FALSE );
		}
	}

	return LightMap;
#else
	return NULL;
#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
INT CompareLightmaps( FLightMapAllocation* A, FLightMapAllocation* B )
{
	PTRINT InstancedMeshA = A->Primitive->IsA(UInstancedStaticMeshComponent::StaticClass()) ? PTRINT(A->Primitive) : 0;
	PTRINT InstancedMeshB = B->Primitive->IsA(UInstancedStaticMeshComponent::StaticClass()) ? PTRINT(B->Primitive) : 0;
	PTRINT ProcBuildingA = InstancedMeshA ? PTRINT(A->Primitive->GetOuter()) : 0;
	PTRINT ProcBuildingB = InstancedMeshB ? PTRINT(B->Primitive->GetOuter()) : 0;

	// Sort on ProcBuilding first.
	PTRINT ProcBuildingDiff = ProcBuildingB - ProcBuildingA;
	if ( GGroupComponentLightmaps && ProcBuildingDiff )
	{
		return ProcBuildingDiff;
	}

	// Sort on InstancedMeshComponent second.
	PTRINT InstanceMeshDiff = InstancedMeshB - InstancedMeshA;
	if ( GGroupComponentLightmaps && InstanceMeshDiff )
	{
		return InstanceMeshDiff;
	}

	// Sort on bounding box size third.
	return Max(B->TotalSizeX,B->TotalSizeY) - Max(A->TotalSizeX,A->TotalSizeY);
}
IMPLEMENT_COMPARE_POINTER(FLightMapAllocation,UnLightMap,{ return CompareLightmaps(A, B); });

#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
/**
 * Executes all pending light-map encoding requests.
 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
 * @param	bForceCompletion	Force all encoding to be fully completed (they may be asynchronous).
 */
void FLightMap2D::EncodeTextures( UBOOL bLightingSuccessful, UBOOL bForceCompletion )
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	if ( bLightingSuccessful )
	{
		GWarn->BeginSlowTask(TEXT("Encoding light-maps"),FALSE);
		INT PackedLightAndShadowMapTextureSize = GWorld->GetWorldInfo()->PackedLightAndShadowMapTextureSize;

		// Reset the pending light-map size.
		PendingLightMapSize = 0;

		// Sort the light-maps in descending order by size.
		Sort<USE_COMPARE_POINTER(FLightMapAllocation,UnLightMap)>((FLightMapAllocation**)PendingLightMaps.GetData(),PendingLightMaps.Num());

		// Allocate texture space for each light-map.
		TArray<FLightMapPendingTexture*> PendingTextures;

		for(INT LightMapIndex = 0;LightMapIndex < PendingLightMaps.Num();LightMapIndex++)
		{
			FLightMapAllocation& Allocation = PendingLightMaps(LightMapIndex);
			FLightMapPendingTexture* Texture = NULL;

			if ( GAllowLightmapCropping )
			{
				CropUnmappedTexels( Allocation.RawData, Allocation.TotalSizeX, Allocation.TotalSizeY, Allocation.MappedRect );
			}

			// Find an existing texture which the light-map can be stored in.
			INT FoundIndex = -1;
			for(INT TextureIndex = 0;TextureIndex < PendingTextures.Num();TextureIndex++)
			{
				FLightMapPendingTexture* ExistingTexture = PendingTextures(TextureIndex);

				// Lightmaps will always be 4-pixel aligned...
				if ( ExistingTexture->AddElement( Allocation ) )
				{
					Texture = ExistingTexture;
					FoundIndex = TextureIndex;
					break;
				}
			}

			if(!Texture)
			{
				INT NewTextureSizeX = PackedLightAndShadowMapTextureSize;
				INT NewTextureSizeY = PackedLightAndShadowMapTextureSize;

				// Calculate best texture size based on LightMapWidth and InstanceCount.
				if (Allocation.bInstancedStaticMesh)
				{
					INT	LightMapWidth	= 0;
					INT	LightMapHeight	= 0;
					INT Count			= 0;

					UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(Allocation.Primitive);
					InstancedStaticMeshComponent->GetLightMapResolution(LightMapWidth,LightMapHeight);
					Count = appRound(appSqrt(InstancedStaticMeshComponent->GetInstanceCount()));
					NewTextureSizeX = NewTextureSizeY = appRoundUpToPowerOfTwo(LightMapWidth*Count);

					if( NewTextureSizeX > 4096 )
					{
						GWarn->MapCheck_Add( MCTYPE_ERROR, InstancedStaticMeshComponent->GetOuter(), *FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "MapCheck_Message_InstancedStaticMesh_Texture_Size" )),
							*InstancedStaticMeshComponent->StaticMesh->GetName(),
							InstancedStaticMeshComponent->StaticMesh->LightMapResolution, 
							InstancedStaticMeshComponent->GetInstanceCount() ) ), TEXT( "InstancedStaticMesh_Texture_Size" ), MCGROUP_DEFAULT );
					}
				}

				if(Allocation.MappedRect.Width() > NewTextureSizeX || Allocation.MappedRect.Height() > NewTextureSizeY)
				{
					NewTextureSizeX = appRoundUpToPowerOfTwo(Allocation.MappedRect.Width());
					NewTextureSizeY = appRoundUpToPowerOfTwo(Allocation.MappedRect.Height());
				}

				// If there is no existing appropriate texture, create a new one.
				Texture = new FLightMapPendingTexture( NewTextureSizeX, NewTextureSizeY );
				PendingTextures.AddItem( Texture );
				Texture->Material = Allocation.Material;
				Texture->Outer = Allocation.Outer;
				Texture->Bounds = Allocation.Bounds;
				Texture->LightmapFlags = Allocation.LightmapFlags;
				Texture->bInstancedStaticMesh = Allocation.bInstancedStaticMesh;
				verify( Texture->AddElement( Allocation, TRUE ) );
				FoundIndex = PendingTextures.Num() - 1;
			}

// 			FBox AllocBox = Allocation.Bounds.GetBox();
//		 	FBox TextureBox = Texture->Bounds.GetBox();
//	 		debugf( TEXT("Lightmap %d, %s, texture, %d, origin, %.1f, %.1f, %.1f, extent, %.1f, %.1f, %.1f, textureorigin, %.1f, %.1f, %.1f, textureextent, %.1f, %.1f, %.1f, texturesize, %d, %d, mapsize, %d, %d"), 
//	 			LightMapIndex, *Allocation.Primitive->GetFullName(), FoundIndex,
//	 			AllocBox.Min.X, AllocBox.Min.Y, AllocBox.Min.Z, AllocBox.Max.X, AllocBox.Max.Y, AllocBox.Max.Z,
//	 			TextureBox.Min.X, TextureBox.Min.Y, TextureBox.Min.Z, TextureBox.Max.X, TextureBox.Max.Y, TextureBox.Max.Z,
//	 			Texture->GetSizeX(), Texture->GetSizeY(),
//	 			Allocation.MappedRect.Width(), Allocation.MappedRect.Height() );

			Texture->Allocations.AddItem(&Allocation);
		}


		if( GRepackLightAndShadowMapTextures )
		{
			// Optimize light map size by attempting to repack all light map textures at a smaller resolution.  In
			// general, this results in fewer rectangular light map textures and more-tightly packed square textures.
			RepackLightMapTextures( PendingTextures, PackedLightAndShadowMapTextureSize );
		}


		// Encode all the pending textures.
		for(INT TextureIndex = 0; TextureIndex < PendingTextures.Num(); TextureIndex++)
		{
			if (bUpdateStatus)
			{
				GWarn->StatusUpdatef(TextureIndex,PendingTextures.Num(),LocalizeSecure(LocalizeUnrealEd(TEXT("EncodingLightMapsF")),TextureIndex,PendingTextures.Num()));
			}
			FLightMapPendingTexture* PendingTexture = PendingTextures(TextureIndex);
			PendingTexture->StartEncoding();
		}

		PendingTextures.Empty();
		PendingLightMaps.Empty();

		if ( bForceCompletion )
		{
			// Block until there are 0 unfinished tasks, making sure all compression has completed.
			FLightMapPendingTexture::FinishCompletedTasks( 0 );

			// Compress any texture to PVRTC if needed.
			FLightMapPendingTexture::PVRTCCompressor.CompressTextures();
		}
	
		
		// End the encoding lighmaps slow task
		GWarn->EndSlowTask();
		
	}
	else
	{
		PendingLightMaps.Empty();
	}
#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}



/**
 * Static: Repacks light map textures to optimize texture memory usage
 *
 * @param	PendingTexture							(In, Out) Array of packed light map textures
 * @param	InPackedLightAndShadowMapTextureSize	Target texture size for light and shadow maps
 */
void FLightMap2D::RepackLightMapTextures( TArray<FLightMapPendingTexture*>& PendingTextures, const INT InPackedLightAndShadowMapTextureSize )
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	TArray<FLightMapAllocation*> NewPendingLightMaps;
	TArray<FLightMapPendingTexture*> NewPendingTextures;

	UBOOL bRepackSucceeded = TRUE;
	for(INT TextureIndex = 0;TextureIndex < PendingTextures.Num();TextureIndex++)
	{
		const FLightMapPendingTexture* OriginalTexture = PendingTextures( TextureIndex );
		TArray<FLightMapAllocation*> LightMapsToRepack;


		// Make a list of light maps to repack, copying the original allocations
		for( INT CurAllocation = 0; CurAllocation < OriginalTexture->Allocations.Num(); ++CurAllocation )
		{
			const FLightMapAllocation* OriginalAllocation = OriginalTexture->Allocations( CurAllocation );

			FLightMapAllocation* LightMapCopy = new FLightMapAllocation();
			*LightMapCopy = *OriginalAllocation;
			LightMapsToRepack.AddItem( LightMapCopy );
			NewPendingLightMaps.AddItem( LightMapCopy );
		}


		// Sort the light-maps in descending order by size.
		Sort<USE_COMPARE_POINTER(FLightMapAllocation,UnLightMap)>((FLightMapAllocation**)LightMapsToRepack.GetData(),LightMapsToRepack.Num());


		// How many square powers of two sizes to attempt repacking this texture into, smallest first
		const INT MaxAttempts = 4;
		FLightMapPendingTexture* RepackedTexture = NULL;
		for( INT AttemptIndex = 0; AttemptIndex <= MaxAttempts; ++AttemptIndex )
		{
			INT LightMapIndex = 0;
			for( ; LightMapIndex < LightMapsToRepack.Num(); ++LightMapIndex )
			{
				FLightMapAllocation& Allocation = *LightMapsToRepack( LightMapIndex );

				// Never perform distance/component tests when repacking into the same texture
				const UBOOL bForceIntoThisTexture = TRUE;

				if( RepackedTexture != NULL )
				{
					if( !RepackedTexture->AddElement( Allocation, bForceIntoThisTexture ) )
					{
						// At least one allocation didn't fit into the texture at the new resolution, so bail out.
						break;
					}
				}
				else
				{
					// Start with the smallest allowed texture size and work our way up to the texture's original size
					INT MaxTextureSizeForThisAttempt = InPackedLightAndShadowMapTextureSize;
					const UBOOL bRepackingAtFullResolution = ( AttemptIndex == MaxAttempts );
					if( !bRepackingAtFullResolution )
					{
						MaxTextureSizeForThisAttempt /= ( 1 << ( MaxAttempts - AttemptIndex ) );
						if( MaxTextureSizeForThisAttempt < 32 )
						{
							MaxTextureSizeForThisAttempt = 32;
						}
					}

					INT NewTextureSizeX = MaxTextureSizeForThisAttempt;
					INT NewTextureSizeY = MaxTextureSizeForThisAttempt;

					// If we're repacking at full resolution, then make sure the texture is the same size that
					// it was when we packed it the first time.
					if( bRepackingAtFullResolution )
					{
						if(Allocation.MappedRect.Width() > NewTextureSizeX || Allocation.MappedRect.Height() > NewTextureSizeY)
						{
							NewTextureSizeX = appRoundUpToPowerOfTwo(Allocation.MappedRect.Width());
							NewTextureSizeY = appRoundUpToPowerOfTwo(Allocation.MappedRect.Height());
						}
					}

					// If there is no existing appropriate texture, create a new one.
					RepackedTexture = new FLightMapPendingTexture( NewTextureSizeX, NewTextureSizeY );
					RepackedTexture->Material = Allocation.Material;
					RepackedTexture->Outer = Allocation.Outer;
					RepackedTexture->Bounds = Allocation.Bounds;
					RepackedTexture->LightmapFlags = Allocation.LightmapFlags;
					if( !RepackedTexture->AddElement( Allocation, bForceIntoThisTexture ) )
					{
						// This mapping didn't fit into the light map texture; bail out
						break;
					}
				}

				RepackedTexture->Allocations.AddItem(&Allocation);
			}

			if( LightMapIndex == LightMapsToRepack.Num() )
			{
				// All light maps were successfully packed into the texture!
				break;
			}
			else
			{
				// Failed to pack light maps into the texture
				delete RepackedTexture;
				RepackedTexture = NULL;
			}
		}

		// The texture should have always been repacked successfully.  If this assert goes off it means
		// that repacking is no longer deterministic with the first light map pack phase.  We always repack
		// textures after attempting to use lower resolution square textures.
		if( ensure( RepackedTexture != NULL ) )
		{
			NewPendingTextures.AddItem( RepackedTexture );
		}
		else
		{
			// Failed to repack at least one texture.  This should really never happen.
			bRepackSucceeded = FALSE;
			break;
		}
	}

	if( bRepackSucceeded )
	{
		// Destroy the original textures
		check( NewPendingTextures.Num() == PendingTextures.Num() );
		for( INT OldTextureIndex = 0; OldTextureIndex < PendingTextures.Num(); ++OldTextureIndex )
		{
			delete PendingTextures( OldTextureIndex );
			PendingTextures( OldTextureIndex ) = NULL;
		}

		// Replace the textures with our repacked textures
		PendingTextures = NewPendingTextures;


		// Replace the old light map array with the new one.  This will delete the old light map allocations.
		check( NewPendingLightMaps.Num() == PendingLightMaps.Num() );
		PendingLightMaps.Empty();
		for( INT CurLightMapIndex = 0; CurLightMapIndex < NewPendingLightMaps.Num(); ++CurLightMapIndex )
		{
			PendingLightMaps.AddRawItem( NewPendingLightMaps( CurLightMapIndex ) );
		}
	}
#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}




FLightMap2D::FLightMap2D(UBOOL InbAllowDirectionalLightMaps)
:	FLightMap(InbAllowDirectionalLightMaps)
{
	for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		Textures[CoefficientIndex] = NULL;
	}
}

FLightMap2D::FLightMap2D(const TArray<FGuid>& InLightGuids)
{
	LightGuids = InLightGuids;
	for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		Textures[CoefficientIndex] = NULL;
	}
}

const UTexture2D* FLightMap2D::GetTexture(UINT BasisIndex) const
{
	check(IsValid(BasisIndex));
	return Textures[BasisIndex];
}

UTexture2D* FLightMap2D::GetTexture(UINT BasisIndex)
{
	check(IsValid(BasisIndex));
	return Textures[BasisIndex];
}

/**
 * Returns whether the specified basis has a valid lightmap texture or not.
 * @param	BasisIndex - The basis index.
 * @return	TRUE if the specified basis has a valid lightmap texture, otherwise FALSE
 */
UBOOL FLightMap2D::IsValid(UINT BasisIndex) const
{
	if (bAllowDirectionalLightMaps)
	{
		return (BasisIndex < SIMPLE_LIGHTMAP_COEF_INDEX);
	}
	return (BasisIndex >= SIMPLE_LIGHTMAP_COEF_INDEX && BasisIndex < NUM_STORED_LIGHTMAP_COEF);
}

struct FLegacyLightMapTextureInfo
{
	ULightMapTexture2D* Texture;
	FLinearColor Scale;
	FLinearColor Bias;

	friend FArchive& operator<<(FArchive& Ar,FLegacyLightMapTextureInfo& I)
	{
		return Ar << I.Texture << I.Scale << I.Bias;
	}
};

/**
 * Finalizes the lightmap after encodeing, including setting the UV scale/bias for this lightmap 
 * inside the larger UTexture2D that this lightmap is in
 * 
 * @param Scale UV scale (size)
 * @param Bias UV Bias (offset)
 * @param ALightmapTexture One of the lightmap textures that this lightmap was put into
 *
 * @return TRUE if the lightmap should be deleted after this call
 */
void FLightMap2D::FinalizeEncoding(const FVector2D& Scale, const FVector2D& Bias, UTexture2D* ALightmapTexture)
{
	CoordinateScale = Scale;
	CoordinateBias = Bias;
}

void FLightMap2D::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		UObject::AddReferencedObject(ObjectArray,Textures[CoefficientIndex]);
	}
}

void FLightMap2D::Serialize(FArchive& Ar)
{
	FLightMap::Serialize(Ar);

	if (Ar.IsLoading() && Ar.Ver() < VER_MAXCOMPONENT_LIGHTMAP_ENCODING)
	{
		ULightMapTexture2D* LegacyTextures[4];
		FVector4 LegacyScaleVectors[4];

		for(UINT CoefficientIndex = 0;CoefficientIndex < 4;CoefficientIndex++)
		{
			Ar << LegacyTextures[CoefficientIndex];
			Ar << (FVector&)LegacyScaleVectors[CoefficientIndex];
		}
	}
	else
	{
		for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			Ar << Textures[CoefficientIndex];

			//@warning This used to use the FVector serialization (wrong). Now it would have used
			//         the FVector4 serialization (correct), but this would cause backwards compability
			//         problems.
			//         We are now assuming that FVector and the first 12 bytes of FVector4 are binary compatible.
			Ar << (FVector&)ScaleVectors[CoefficientIndex];
		}
	}

	Ar << CoordinateScale << CoordinateBias;

}

FLightMapInteraction FLightMap2D::GetInteraction() const
{
	if (bAllowDirectionalLightMaps)
	{
		UBOOL bValidTextures = TRUE;
		for (INT TextureIndex = 0; TextureIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF; TextureIndex++)
		{
			bValidTextures = bValidTextures && Textures[TextureIndex];
		}
		// When the FLightMap2D is first created, the textures aren't set, so that case needs to be handled.
		if(bValidTextures)
		{
			return FLightMapInteraction::Texture(Textures,ScaleVectors,CoordinateScale,CoordinateBias,TRUE);
		}
	}
	else
	{
		if(Textures[SIMPLE_LIGHTMAP_COEF_INDEX])
		{
			return FLightMapInteraction::Texture(Textures,ScaleVectors,CoordinateScale,CoordinateBias,FALSE);
		}
	}
	return FLightMapInteraction::None();
}



/*-----------------------------------------------------------------------------
	FInstancedLightMap2D
-----------------------------------------------------------------------------*/

FInstancedLightMap2D::FInstancedLightMap2D(class UInstancedStaticMeshComponent* InComponent, INT InInstancedIndex, const TArray<FGuid>& InLightGuids)
	: FLightMap2D(InLightGuids)
	, Component(InComponent)
	, InstanceIndex(InInstancedIndex)
{
}

/**
 * Allocates texture space for the light-map and stores the light-map's raw data for deferred encoding.
 * If the light-map has no lights in it, it will return NULL.
 * RawData and SourceQuantizedData will be deleted by this function.
 * @param	Component - The component that owns this lightmap
 * @param	InstanceIndex - Which instance in the component this lightmap is for
 * @param	RawData - The raw light-map data to fill the texture with.  
 * @param	SourceQuantizedData - If the data is already quantized, the values will be in here, and not in RawData.  
 * @param	Material - The material which the light-map will be rendered with.  Used as a hint to pack light-maps for the same material in the same texture.  Can be NULL.
 * @param	Bounds - The bounds of the primitive the light-map will be rendered on.  Used as a hint to pack light-maps on nearby primitives in the same texture.
 * @param	InPaddingType - the method for padding the lightmap.
 * @param	LightmapFlags - flags that determine how the lightmap is stored (e.g. streamed or not)
 * @param	QuantizationScale - The scale to use when quantizing the data
 */
FInstancedLightMap2D* FInstancedLightMap2D::AllocateLightMap(UInstancedStaticMeshComponent* Component, INT InstanceIndex, FLightMapData2D*& RawData, struct FQuantizedLightmapData*& SourceQuantizedData, 
	UMaterialInterface* Material, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags InLightmapFlags, FLOAT QuantizationScale[NUM_STORED_LIGHTMAP_COEF][3])
{
	// If the light-map has no lights in it, return NULL.
	if(!RawData && !SourceQuantizedData)
	{
		return NULL;
	}

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

	// keep track of whether or not we need to quantize the RawData
	UBOOL bNeedsToQuantize = FALSE;

	FLightMapAllocation* Allocation;
	if (SourceQuantizedData == NULL)
	{
		Allocation = new(PendingLightMaps) FLightMapAllocation;
		bNeedsToQuantize = TRUE;
	}
	else
	{
		Allocation = new(PendingLightMaps) FLightMapAllocation(SourceQuantizedData);
	}

	// TODO: For mobile, we have options for setting a fixed lightmap scale and for
	// keeping the lightmap in linear space for performance reasons, both only on
	// simple lightmaps. If mobile supports this path at some point, we need to
	// hook that stuff up.

	// Calculate the scale/bias for the light-map coefficients.
	FLOAT InvCoefficientScale[NUM_STORED_LIGHTMAP_COEF][3];
	for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
	{
		for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
		{
			Allocation->Scale[CoefficientIndex][ColorIndex] = Min<FLOAT>(QuantizationScale[CoefficientIndex][ColorIndex], MAX_LIGHT_INTENSITY);
			InvCoefficientScale[CoefficientIndex][ColorIndex] = 1.0f / Max<FLOAT>(Allocation->Scale[CoefficientIndex][ColorIndex],DELTA);
		}
	}

	// we need to take the data and requantize it with the incoming QuantizationScale
	if (SourceQuantizedData)
	{
		for(UINT Y = 0;Y < SourceQuantizedData->SizeY;Y++)
		{
			for(UINT X = 0;X < SourceQuantizedData->SizeX;X++)
			{
				// get source from input, dest from the rectangular offset in the group
				const FLightMapCoefficients& SourceSample = SourceQuantizedData->Data(Y * SourceQuantizedData->SizeX + X);
				FLightMapCoefficients& DestSample = Allocation->RawData(Y * SourceQuantizedData->SizeX + X);

				// go over each coefficient and dequantize and requantize with new Scale
				for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
				{
					for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
					{
						// TODO: For mobile, we have options for setting a fixed lightmap scale and for
						// keeping the lightmap in linear space for performance reasons, both only on
						// simple lightmaps. If mobile supports this path at some point, we need to
						// hook that stuff up.

						// dequantize it
						FLOAT Dequantized = (FLOAT)SourceSample.Coefficients[CoefficientIndex][ColorIndex] / 255.0f;
						Dequantized = appPow(Dequantized, 2.2f) * SourceQuantizedData->Scale[CoefficientIndex][ColorIndex];

						// requantize it
						DestSample.Coefficients[CoefficientIndex][ColorIndex] = (BYTE)Clamp<INT>(
							appTrunc(
								appPow(
									Dequantized * InvCoefficientScale[CoefficientIndex][ColorIndex],
									1.0f / 2.2f
									) * 255.0f
								),
							0,
							255);
					}
				}
			}
		}
	}

	Allocation->Material	= Material;
	Allocation->Outer		= Component->GetOutermost();
	Allocation->PaddingType	= InPaddingType;
	Allocation->LightmapFlags = InLightmapFlags;
	Allocation->Bounds		= Bounds;
	Allocation->Primitive	= Component;

	// Set flag if this is an allocation for an instanced static mesh component that has forced instance grouping.
	Allocation->bInstancedStaticMesh = Component->bDontResolveInstancedLightmaps ? TRUE : FALSE;

	if ( !GAllowStreamingLightmaps )
	{
		Allocation->LightmapFlags = ELightMapFlags( Allocation->LightmapFlags & ~LMF_Streamed );
	}

	// Create a new light-map.
	FInstancedLightMap2D* LightMap = new FInstancedLightMap2D(Component, InstanceIndex, bNeedsToQuantize ? RawData->LightGuids : SourceQuantizedData->LightGuids);
	Allocation->LightMap = LightMap;


	// Quantized data is no longer needed now that FLightMapAllocation has made a copy of it
	if (SourceQuantizedData != NULL)
	{
		delete SourceQuantizedData;
		SourceQuantizedData = NULL;
	}


	if (bNeedsToQuantize)
	{
		// Quantize the coefficients for this texture.
		Allocation->TotalSizeX = RawData->GetSizeX();
		Allocation->TotalSizeY = RawData->GetSizeY();
		Allocation->MappedRect = FIntRect( 0, 0, Allocation->TotalSizeX, Allocation->TotalSizeY );
		Allocation->RawData.Empty(RawData->GetSizeX() * RawData->GetSizeY());
		Allocation->RawData.Add(RawData->GetSizeX() * RawData->GetSizeY());
		for(UINT Y = 0;Y < RawData->GetSizeY();Y++)
		{
			for(UINT X = 0;X < RawData->GetSizeX();X++)
			{
				const FLightSample& SourceSample = (*RawData)(X,Y);
				FLightMapCoefficients& DestCoefficients = Allocation->RawData(Y * RawData->GetSizeX() + X);

				DestCoefficients.Coverage = SourceSample.bIsMapped ? 255 : 0;
				for(INT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
				{
					for(INT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
					{
						// TODO: For mobile, we have options for setting a fixed lightmap scale and for
						// keeping the lightmap in linear space for performance reasons, both only on
						// simple lightmaps. If mobile supports this path at some point, we need to
						// hook that stuff up.

						DestCoefficients.Coefficients[CoefficientIndex][ColorIndex] = (BYTE)Clamp<INT>(
							appTrunc(
								appPow(
									SourceSample.Coefficients[CoefficientIndex][ColorIndex] * InvCoefficientScale[CoefficientIndex][ColorIndex],
									1.0f / 2.2f
									) * 255.0f
								),
							0,
							255);
					}
				}
			}
		}
	}

	// RawData is no longer needed now it has been quantized
	delete RawData;
	RawData = NULL;

	// For simple lightmaps, we may have an optional texture to modulate in
	ApplySimpleLightmapModification( Allocation );

	// Track the size of pending light-maps.
	PendingLightMapSize += ((Allocation->TotalSizeX + 3) & ~3) * ((Allocation->TotalSizeY + 3) & ~3);

	// If we're allowed to eagerly encode lightmaps, check to see if we're ready
	if (GAllowEagerLightmapEncode)
	{
		// Once there are enough pending light-maps, flush encoding.
		const UINT PackedLightAndShadowMapTextureSize = GWorld->GetWorldInfo()->PackedLightAndShadowMapTextureSize;
		const UINT MaxPendingLightMapSize = Square(PackedLightAndShadowMapTextureSize) * 4;
		if(PendingLightMapSize >= MaxPendingLightMapSize)
		{
			EncodeTextures( TRUE, FALSE );
		}
	}

	return LightMap;
#else
	return NULL;
#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}



/**
 * Finalizes the lightmap after encodeing, including setting the UV scale/bias for this lightmap 
 * inside the larger UTexture2D that this lightmap is in
 * 
 * @param Scale UV scale (size)
 * @param Bias UV Bias (offset)
 * @param ALightmapTexture One of the lightmap textures that this lightmap was put into
 *
 * @return TRUE if the lightmap should be deleted after this call
 */
void FInstancedLightMap2D::FinalizeEncoding(const FVector2D& Scale, const FVector2D& Bias, UTexture2D* ALightmapTexture)
{
	// all instances share a lightmap scale, but we set it in all lightmaps, since we may split up the component later
	CoordinateScale = Scale;

	Component->PerInstanceSMData(InstanceIndex).LightmapUVBias = Bias;

	// remember what UTexture2D this lightmap was put into, so we can later split up the component based on lightmap texture
	Component->CachedMappings(InstanceIndex).LightmapTexture = ALightmapTexture;
}

FInstancedLightMap2D::~FInstancedLightMap2D()
{
//	debugf(TEXT("Instnaed lightmap destructor!"));
}


FLightMap1D::FLightMap1D(UObject* InOwner, FLightMapData1D*& Data, FQuantizedLightmapData*& QuantizedData):
	Owner(InOwner),
	CachedSampleDataSize(0),
	CachedSampleData(NULL)
{
	// do we need to quantize the data?
	UBOOL bNeedsToQuantize = QuantizedData == NULL;

	LightGuids = bNeedsToQuantize ? Data->LightGuids : QuantizedData->LightGuids;
	if (bNeedsToQuantize)
	{
		// Calculate the maximum light coefficient for each color component.
		FLOAT MaxCoefficient[NUM_STORED_LIGHTMAP_COEF][3];
		for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			for(UINT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
			{
				MaxCoefficient[CoefficientIndex][ColorIndex] = 0;
			}
		}
		for(INT SampleIndex = 0;SampleIndex < Data->GetSize();SampleIndex++)
		{
			for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
			{
				for(UINT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
				{
					MaxCoefficient[CoefficientIndex][ColorIndex] = Clamp(
						(*Data)(SampleIndex).Coefficients[CoefficientIndex][ColorIndex],
						MaxCoefficient[CoefficientIndex][ColorIndex],
						MAX_LIGHT_INTENSITY
						);
				}
			}
		}

		// Calculate the scale and inverse scale for the quantized coefficients.
		FLOAT InvScale[NUM_STORED_LIGHTMAP_COEF][3];
		for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			for(UINT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
			{
				ScaleVectors[CoefficientIndex].Component(ColorIndex) = MaxCoefficient[CoefficientIndex][ColorIndex];
				InvScale[CoefficientIndex][ColorIndex] = 1.0f / Max(MaxCoefficient[CoefficientIndex][ColorIndex],DELTA);
			}
		}

		QuantizeBulkSamples(DirectionalSamples, *Data, InvScale, NUM_DIRECTIONAL_LIGHTMAP_COEF, 0);
		if( GEngine->bShouldGenerateSimpleLightmaps )
		{
			QuantizeBulkSamples(SimpleSamples, *Data, InvScale, NUM_SIMPLE_LIGHTMAP_COEF, SIMPLE_LIGHTMAP_COEF_INDEX);
		}

		// Delete the source lightmap data now that we're done with it
		delete Data;
		Data = NULL;
	}
	else
	{
		// copy over the scale values
		for(UINT CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			for(UINT ColorIndex = 0;ColorIndex < 3;ColorIndex++)
			{
				ScaleVectors[CoefficientIndex].Component(ColorIndex) = QuantizedData->Scale[CoefficientIndex][ColorIndex];
			}
		}

		// copy over the quantized data
		CopyQuantizedData(DirectionalSamples, QuantizedData, NUM_DIRECTIONAL_LIGHTMAP_COEF, 0);
		if( GEngine->bShouldGenerateSimpleLightmaps )
		{
			CopyQuantizedData(SimpleSamples, QuantizedData, NUM_SIMPLE_LIGHTMAP_COEF, SIMPLE_LIGHTMAP_COEF_INDEX);
		}

		// now that we've copied out the quantized data, we are done with it
		delete QuantizedData;
		QuantizedData = NULL;
	}

	check( CachedSampleData == NULL && CachedSampleDataSize == 0 );

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && !CONSOLE
	// Detect if this allocation belongs to the vertex mapping that was being debugged
	// This only works for mappings that can be uniquely identified by a single component
	if (GCurrentSelectedLightmapSample.Component && InOwner == GCurrentSelectedLightmapSample.Component)
	{
		GCurrentSelectedLightmapSample.Lightmap = this;
		if (AllowsDirectionalLightmaps())
		{
			FQuantizedDirectionalLightSample* Sample = (FQuantizedDirectionalLightSample*)DirectionalSamples.Lock(LOCK_READ_WRITE);
			for (INT CoefIndex = 0; CoefIndex < NUM_DIRECTIONAL_LIGHTMAP_COEF; CoefIndex++)
			{
				GCurrentSelectedLightmapSample.OriginalColor = Sample[GCurrentSelectedLightmapSample.VertexIndex].Coefficients[CoefIndex];
				extern FColor GTexelSelectionColor;
				Sample[GCurrentSelectedLightmapSample.VertexIndex].Coefficients[CoefIndex] = GTexelSelectionColor;
			}
			DirectionalSamples.Unlock();
		}
		else
		{
			FQuantizedSimpleLightSample* Sample = (FQuantizedSimpleLightSample*)SimpleSamples.Lock(LOCK_READ_WRITE);
			for (INT CoefIndex = 0; CoefIndex < NUM_SIMPLE_LIGHTMAP_COEF; CoefIndex++)
			{
				GCurrentSelectedLightmapSample.OriginalColor = Sample[GCurrentSelectedLightmapSample.VertexIndex].Coefficients[CoefIndex];
				extern FColor GTexelSelectionColor;
				Sample[GCurrentSelectedLightmapSample.VertexIndex].Coefficients[CoefIndex] = GTexelSelectionColor;
			}
			SimpleSamples.Unlock();
		}
	}
#endif

	InitResources();
}

/**
 * Scales, gamma corrects and quantizes the passed in samples and then stores them in BulkData.
 *
 * @param BulkData - The bulk data where the quantized samples are stored.
 * @param Data - The samples to process.
 * @param InvScale - 1 / max sample value.
 * @param NumCoefficients - Number of coefficients in the BulkData samples.
 * @param RelativeCoefficientOffset - Coefficient offset in the Data samples.
 */
template<class BulkDataType>
void FLightMap1D::QuantizeBulkSamples(
	BulkDataType& BulkData, 
	FLightMapData1D& Data, 
	const FLOAT InvScale[][3],
	UINT NumCoefficients, 
	UINT RelativeCoefficientOffset)
{
	// Rescale and quantize the light samples.
	BulkData.Lock( LOCK_READ_WRITE );
	typename BulkDataType::SampleType* SampleData = (typename BulkDataType::SampleType*) BulkData.Realloc( Data.GetSize() );
	for(INT SampleIndex = 0;SampleIndex < Data.GetSize();SampleIndex++)
	{
		typename BulkDataType::SampleType& DestSample = SampleData[SampleIndex];
		const FLightSample& SourceSample = Data(SampleIndex);
		for(UINT CoefficientIndex = 0;CoefficientIndex < NumCoefficients;CoefficientIndex++)
		{
			const UINT AbsoluteCoefficientIndex = CoefficientIndex + RelativeCoefficientOffset;
			DestSample.Coefficients[CoefficientIndex] = FColor(
				(BYTE)Clamp<INT>(appTrunc(appPow(SourceSample.Coefficients[AbsoluteCoefficientIndex][0] * InvScale[AbsoluteCoefficientIndex][0],1.0f / 2.2f) * 255.0f),0,255),
				(BYTE)Clamp<INT>(appTrunc(appPow(SourceSample.Coefficients[AbsoluteCoefficientIndex][1] * InvScale[AbsoluteCoefficientIndex][1],1.0f / 2.2f) * 255.0f),0,255),
				(BYTE)Clamp<INT>(appTrunc(appPow(SourceSample.Coefficients[AbsoluteCoefficientIndex][2] * InvScale[AbsoluteCoefficientIndex][2],1.0f / 2.2f) * 255.0f),0,255),
				0);
		}
	}
	BulkData.Unlock();
}

template<class BulkDataType>
void FLightMap1D::CopyQuantizedData(
	BulkDataType& BulkData, 
	FQuantizedLightmapData* QuantizedData, 
	UINT NumCoefficients, 
	UINT RelativeCoefficientOffset)
{
	// Rescale and quantize the light samples.
	BulkData.Lock( LOCK_READ_WRITE );
	typename BulkDataType::SampleType* SampleData = (typename BulkDataType::SampleType*) BulkData.Realloc( QuantizedData->SizeX );
	for(UINT SampleIndex = 0;SampleIndex < QuantizedData->SizeX;SampleIndex++)
	{
		typename BulkDataType::SampleType& DestSample = SampleData[SampleIndex];
		const FLightMapCoefficients& SourceSample = QuantizedData->Data(SampleIndex);
		for(UINT CoefficientIndex = 0;CoefficientIndex < NumCoefficients;CoefficientIndex++)
		{
			const UINT AbsoluteCoefficientIndex = CoefficientIndex + RelativeCoefficientOffset;
			DestSample.Coefficients[CoefficientIndex] = FColor(
				SourceSample.Coefficients[AbsoluteCoefficientIndex][0],
				SourceSample.Coefficients[AbsoluteCoefficientIndex][1],
				SourceSample.Coefficients[AbsoluteCoefficientIndex][2],
				0);
		}
	}
	BulkData.Unlock();
}

FLightMap1D::~FLightMap1D()
{
	const UINT BulkSampleSize = bAllowDirectionalLightMaps ? DirectionalSamples.GetBulkDataSize() : SimpleSamples.GetBulkDataSize();
	DEC_DWORD_STAT_BY( STAT_VertexLightingAndShadowingMemory, BulkSampleSize );
	CachedSampleDataSize = 0;
	if(CachedSampleData)
	{
		appFree(CachedSampleData);
		CachedSampleData = NULL;
	}
}

void FLightMap1D::Serialize(FArchive& Ar)
{
	FLightMap::Serialize(Ar);
	Ar << Owner;

	DirectionalSamples.Serialize( Ar, Owner );

	if (Ar.IsLoading() && Ar.Ver() < VER_MAXCOMPONENT_LIGHTMAP_ENCODING)
	{
		FVector4 LegacyScaleVectors[4];
		for (INT ElementIndex = 0; ElementIndex < 4; ElementIndex++)
		{
			Ar << LegacyScaleVectors[ElementIndex].X;
			Ar << LegacyScaleVectors[ElementIndex].Y;
			Ar << LegacyScaleVectors[ElementIndex].Z;
		}
	}
	else
	{
		for (INT ElementIndex = 0; ElementIndex < NUM_STORED_LIGHTMAP_COEF; ElementIndex++)
		{
			Ar << ScaleVectors[ElementIndex].X;
			Ar << ScaleVectors[ElementIndex].Y;
			Ar << ScaleVectors[ElementIndex].Z;
		}
	}

	SimpleSamples.Serialize( Ar, Owner );

	//remove the simple lighting bulk data when cooking for consoles.
	if (Ar.IsLoading() && (GCookingTarget & ( UE3::PLATFORM_Stripped ^ UE3::PLATFORM_Mobile )) )
	{
		SimpleSamples.RemoveBulkData();
	}
}

void FLightMap1D::InitResources()
{
	if(!CachedSampleData)
	{
		if (bAllowDirectionalLightMaps)
		{
			CachedSampleDataSize = DirectionalSamples.GetBulkDataSize();
			if (CachedSampleDataSize > 0)
			{
				DirectionalSamples.GetCopy( &CachedSampleData, TRUE );
			}
			// Remove unused simple samples outside the Editor.
			if( !GIsEditor )
			{
				SimpleSamples.RemoveBulkData();
			}
		}
		else
		{
			CachedSampleDataSize = SimpleSamples.GetBulkDataSize();
			if (CachedSampleDataSize > 0)
			{
				SimpleSamples.GetCopy( &CachedSampleData, !GAllowFullRHIReset );
			}
			// Remove unused directional samples outside the Editor.
			if( !GIsEditor )
			{
				DirectionalSamples.RemoveBulkData();
			}
		}
		INC_DWORD_STAT_BY( STAT_VertexLightingAndShadowingMemory, CachedSampleDataSize );
		
		// Only initialize resource if it has data to avoid 0 size assert.
		if( CachedSampleDataSize )
		{
			BeginInitResource(this);
		}
	}
}

void FLightMap1D::Cleanup()
{
	if ( IsInRenderingThread() )
	{
		ReleaseResource();
	}
	else
	{
		BeginReleaseResource(this);
	}
	FLightMap::Cleanup();
}

void FLightMap1D::InitRHI()
{
	// Create the light-map vertex buffer.
	VertexBufferRHI = RHICreateVertexBuffer(CachedSampleDataSize,NULL,RUF_Static);

	// Write the light-map data to the vertex buffer.
	void* Buffer = RHILockVertexBuffer(VertexBufferRHI,0,CachedSampleDataSize,FALSE);
	appMemcpy(Buffer,CachedSampleData,CachedSampleDataSize);
	
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

/**
 * Creates a new lightmap that is a copy of this light map, but where the sample data
 * has been remapped according to the specified sample index remapping.
 *
 * @param		SampleRemapping		Sample remapping: Dst[i] = Src[SampleRemapping[i]].
 * @return							The new lightmap.
 */
FLightMap1D* FLightMap1D::DuplicateWithRemappedVerts(const TArray<INT>& SampleRemapping)
{
	check( SampleRemapping.Num() > 0 );

	// Create a new light map.
	FLightMap1D* NewLightMap = NULL;

	// Lightmap source samples are only available while in editor
	if (GIsEditor)
	{
		NewLightMap = new FLightMap1D;

		// Copy over the owner and GUIDS.
		NewLightMap->LightGuids = LightGuids;
		NewLightMap->Owner = Owner;

		// Copy over coefficient scale vectors.
		for ( INT ElementIndex = 0 ; ElementIndex < NUM_STORED_LIGHTMAP_COEF ; ++ElementIndex )
		{
			NewLightMap->ScaleVectors[ElementIndex] = ScaleVectors[ElementIndex];
		}
		
		// Access the raw sample data.
		FQuantizedSimpleLightSample* SourceSimpleSampleData = NULL;
		FQuantizedDirectionalLightSample* SourceDirectionalSampleData = NULL;
		if(bAllowDirectionalLightMaps)
		{
			if(CachedSampleData)
			{
				SourceDirectionalSampleData = (FQuantizedDirectionalLightSample*)CachedSampleData;
			}
			else
			{
				DirectionalSamples.GetCopy((void**)&SourceDirectionalSampleData,TRUE);
				CachedSampleData = SourceDirectionalSampleData;
			}
			if(SimpleSamples.IsBulkDataLoaded())
			{
				SimpleSamples.GetCopy((void**)&SourceSimpleSampleData,FALSE);
			}
		}
		else
		{
			if(CachedSampleData)
			{
				SourceSimpleSampleData = (FQuantizedSimpleLightSample*)CachedSampleData;
			}
			else
			{
				SimpleSamples.GetCopy((void**)&SourceSimpleSampleData,TRUE);
				CachedSampleData = SourceSimpleSampleData;
			}
			if(DirectionalSamples.IsBulkDataLoaded())
			{
				DirectionalSamples.GetCopy((void**)&SourceDirectionalSampleData,FALSE);
			}
		}

		// If the light-map has simple sample data, remap it.
		if(SourceSimpleSampleData)
		{
			NewLightMap->SimpleSamples.Lock( LOCK_READ_WRITE );
			FQuantizedSimpleLightSample* DestSimpleSampleData = (FQuantizedSimpleLightSample*) NewLightMap->SimpleSamples.Realloc( SampleRemapping.Num() );
			for(INT SampleIndex = 0;SampleIndex < SampleRemapping.Num();SampleIndex++)
			{
				DestSimpleSampleData[SampleIndex] = SourceSimpleSampleData[SampleRemapping(SampleIndex)];
			}
			NewLightMap->SimpleSamples.Unlock();
		}

		// If the light-map has directional sample data, remap it.
		if(SourceDirectionalSampleData)
		{
			NewLightMap->DirectionalSamples.Lock( LOCK_READ_WRITE );
			FQuantizedDirectionalLightSample* DestDirectionalSampleData = (FQuantizedDirectionalLightSample*) NewLightMap->DirectionalSamples.Realloc( SampleRemapping.Num() );
			for(INT SampleIndex = 0;SampleIndex < SampleRemapping.Num();SampleIndex++)
			{
				DestDirectionalSampleData[SampleIndex] = SourceDirectionalSampleData[SampleRemapping(SampleIndex)];
			}
			NewLightMap->DirectionalSamples.Unlock();
		}

		// Initialize the new light-maps resources.
		NewLightMap->InitResources();

		// Free the temporary copy of the source simple sample data.
		if(CachedSampleData != SourceSimpleSampleData)
		{
			appFree(SourceSimpleSampleData);
		}

		// Free the temporary copy of the source directional sample data.
		if(CachedSampleData != SourceDirectionalSampleData)
		{
			appFree(SourceDirectionalSampleData);
		}
	}

	return NewLightMap;
}


/**
 * Gains access to the bulk data for the simple lightmap data
 * MUST be followed by an EndAccessToSimpleLightSamples() call to unload the data
 *
 * @return A buffer full of simple lightmap data
 */
FQuantizedSimpleLightSample* FLightMap1D::BeginAccessToSimpleLightSamples() const
{
	check(GIsRHIInitialized);

	// allocate space to get the vertices
	FQuantizedSimpleLightSample* DestBuffer = (FQuantizedSimpleLightSample*)appMalloc(SimpleSamples.GetBulkDataSize());

	// get a pointer to the verts
	FQuantizedSimpleLightSample* SrcSimpleSampleData = (FQuantizedSimpleLightSample*)RHILockVertexBuffer(VertexBufferRHI, 0, SimpleSamples.GetBulkDataSize(),TRUE);

	// copy to the destination
	appMemcpy(DestBuffer, SrcSimpleSampleData, SimpleSamples.GetBulkDataSize());

	// unlock RHI copy
	RHIUnlockVertexBuffer(VertexBufferRHI);
	
	return DestBuffer;
}

/**
 * Finishes access to the simple light samples and frees up any temp memory
 *
 * @param Data The data previously returned by BeginAccessToSimpleLightSamples()
 */
void FLightMap1D::EndAccessToSimpleLightSamples(FQuantizedSimpleLightSample* Data) const
{
	appFree(Data);
}

/**
 * Copies samples using the given remapping.
 *
 * @param SourceBulkData - The source samples
 * @param DestBulkData - The destination samples
 * @param SampleRemapping - Dst[i] = Src[SampleRemapping[i]].
 * @param NumCoefficients - Number of coefficients in the sample type.
 */
template<class BulkDataType>
void FLightMap1D::CopyBulkSamples(
	BulkDataType& SourceBulkData, 
	BulkDataType& DestBulkData, 
	const TArray<INT>& SampleRemapping,
	UINT NumCoefficients)
{
	check(IsInRenderingThread());

	// Copy over samples given the index remapping.
	typename BulkDataType::SampleType* SrcSimpleSampleData = 
		(typename BulkDataType::SampleType*)RHILockVertexBuffer(VertexBufferRHI,0,SourceBulkData.GetBulkDataSize(),TRUE);

	DestBulkData.Lock( LOCK_READ_WRITE );
	typename BulkDataType::SampleType* DstSimpleSampleData = (typename BulkDataType::SampleType*)DestBulkData.Realloc( SampleRemapping.Num() );

	for( INT SampleIndex = 0 ; SampleIndex < SampleRemapping.Num() ; ++SampleIndex )
	{
		const INT RemappedIndex = SampleRemapping(SampleIndex);
		checkSlow( RemappedIndex >= 0 && RemappedIndex < NumSamples() );
		const typename BulkDataType::SampleType& SrcSample = SrcSimpleSampleData[RemappedIndex];
		typename BulkDataType::SampleType& DstSample = DstSimpleSampleData[SampleIndex];
		for( UINT CoefficientIndex = 0 ; CoefficientIndex < NumCoefficients ; ++CoefficientIndex )
		{
			DstSample.Coefficients[CoefficientIndex] = SrcSample.Coefficients[CoefficientIndex];
		}
	}
	DestBulkData.Unlock();

	////// restore me
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

/*-----------------------------------------------------------------------------
	FQuantizedLightSample version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns whether single element serialization is required given an archive. This e.g.
 * can be the case if the serialization for an element changes and the single element
 * serialization code handles backward compatibility.
 */
template<class QuantizedLightSampleType>
UBOOL TQuantizedLightSampleBulkData<QuantizedLightSampleType>::RequiresSingleElementSerialization( FArchive& Ar )
{
	return FALSE;
}

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
template<class QuantizedLightSampleType>
INT TQuantizedLightSampleBulkData<QuantizedLightSampleType>::GetElementSize() const
{
	return sizeof(QuantizedLightSampleType);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
template<class QuantizedLightSampleType>
void TQuantizedLightSampleBulkData<QuantizedLightSampleType>::SerializeElement( FArchive& Ar, void* Data, INT ElementIndex )
{
	QuantizedLightSampleType* QuantizedLightSample = (QuantizedLightSampleType*)Data + ElementIndex;
	// serialize as colors
	const UINT NumCoefficients = sizeof(QuantizedLightSampleType) / sizeof(FColor);
	for(INT CoefficientIndex = 0; CoefficientIndex < NumCoefficients; CoefficientIndex++)
	{
		DWORD ColorDWORD = QuantizedLightSample->Coefficients[CoefficientIndex].DWColor();
		Ar << ColorDWORD;
		QuantizedLightSample->Coefficients[CoefficientIndex] = FColor(ColorDWORD);
	} 
};

FArchive& operator<<(FArchive& Ar,FLightMap*& R)
{
	DWORD LightMapType = FLightMap::LMT_None;
	if(Ar.IsSaving())
	{
		if(R != NULL)
		{
			if(R->GetLightMap1D())
			{
				LightMapType = FLightMap::LMT_1D;
			}
			else if(R->GetLightMap2D())
			{
				LightMapType = FLightMap::LMT_2D;
			}
		}
	}
	Ar << LightMapType;

	if(Ar.IsLoading())
	{
		if(LightMapType == FLightMap::LMT_1D)
		{
			R = new FLightMap1D();
		}
		else if(LightMapType == FLightMap::LMT_2D)
		{
			R = new FLightMap2D();
		}
	}

	if(R != NULL)
	{
		R->Serialize(Ar);

		if (Ar.IsLoading() && Ar.Ver() < VER_MAXCOMPONENT_LIGHTMAP_ENCODING
			&& (LightMapType == FLightMap::LMT_1D || LightMapType == FLightMap::LMT_2D))
		{
			// Discard legacy lightmaps
			delete R;
			R = NULL;
		}
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar,FLightMapSerializeHelper& R)
{
	if( Ar.IsLoading() )
	{
		DWORD LightMapType = FLightMap::LMT_None;
		Ar << LightMapType;
		FLightMap* NewLightmap = NULL;
		if(LightMapType == FLightMap::LMT_1D)
		{
			NewLightmap = new FLightMap1D(R.bAllowDirectionalLightMaps && GSystemSettings.bAllowDirectionalLightMaps);
		}
		else if(LightMapType == FLightMap::LMT_2D)
		{
			NewLightmap = new FLightMap2D(R.bAllowDirectionalLightMaps && GSystemSettings.bAllowDirectionalLightMaps);
		}
		if( NewLightmap != NULL )
		{
			NewLightmap->Serialize(Ar);

			if (Ar.IsLoading() && Ar.Ver() < VER_MAXCOMPONENT_LIGHTMAP_ENCODING
				&& (LightMapType == FLightMap::LMT_1D || LightMapType == FLightMap::LMT_2D))
			{
				// Discard legacy lightmaps
				delete NewLightmap;
				NewLightmap = NULL;
			}
			R.LightMapRef = NewLightmap;
		}
		else
		{
			R.LightMapRef = NULL;
		}
	}
	else
	{
		Ar << R.LightMapRef;
	}
	return Ar;
}

UBOOL FQuantizedLightmapData::HasNonZeroData() const
{
	// 1D lightmaps don't have a valid coverage amount, so they shouldn't be discarded if the coverage is 0
	BYTE MinCoverageThreshold = (SizeY == 1) ? 0 : 1;

	// Check all of the samples for a non-zero coverage (if valid) and at least one non-zero coefficient
	for (INT SampleIndex = 0; SampleIndex < Data.Num(); SampleIndex++)
	{
		const FLightMapCoefficients& LightmapSample = Data(SampleIndex);

		if (LightmapSample.Coverage >= MinCoverageThreshold)
		{
			for (INT CoefficentIndex = 0; CoefficentIndex < NUM_STORED_LIGHTMAP_COEF; CoefficentIndex++)
			{
				if ((LightmapSample.Coefficients[CoefficentIndex][0] != 0) || (LightmapSample.Coefficients[CoefficentIndex][1] != 0) || (LightmapSample.Coefficients[CoefficentIndex][2] != 0))
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}
