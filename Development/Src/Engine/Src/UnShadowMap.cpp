/*=============================================================================
	UnShadowMap.cpp: Shadow-map implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMeshClasses.h"
#include "EngineFoliageClasses.h"

#include "UnTextureLayout.h"

IMPLEMENT_CLASS(UShadowMapTexture2D);
IMPLEMENT_CLASS(UShadowMap2D);
IMPLEMENT_CLASS(UShadowMap1D);


#if !CONSOLE
	// NOTE: We're only counting the top-level mip-map for the following variables.
	/** Total number of texels allocated for all shadowmap textures. */
	QWORD GNumShadowmapTotalTexels = 0;
	/** Number of shadowmap textures generated. */
	INT GNumShadowmapTextures = 0;
	/** Total number of mapped texels. */
	QWORD GNumShadowmapMappedTexels = 0;
	/** Total number of unmapped texels. */
	QWORD GNumShadowmapUnmappedTexels = 0;
	/** Whether to allow cropping of unmapped borders in lightmaps and shadowmaps. Controlled by BaseEngine.ini setting. */
	extern UBOOL GAllowLightmapCropping;
	/** Total shadowmap texture memory size (in bytes), including GShadowmapTotalStreamingSize. */
	QWORD GShadowmapTotalSize = 0;
	/** Total shadowmap texture memory size on an Xbox 360 (in bytes). */
	QWORD GShadowmapTotalSize360 = 0;
	/** Total texture memory size for streaming shadowmaps. */
	QWORD GShadowmapTotalStreamingSize = 0;

	/** Whether to allow lighting builds to generate streaming lightmaps. */
	extern UBOOL GAllowStreamingLightmaps;
#endif

void UShadowMapTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	LODGroup = TEXTUREGROUP_Shadowmap;
}

// Editor thumbnail viewer interface. Unfortunately this is duplicated across all UTexture implementations.

/** 
 * Returns a one line description of an object for viewing in the generic browser
 */
FString UShadowMapTexture2D::GetDesc()
{
	return FString::Printf( TEXT("Shadowmap: %dx%d [%s]"), SizeX, SizeY, GPixelFormats[Format].Name );
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UShadowMapTexture2D::GetDetailedDescription( INT InIndex )
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

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

/**
 * Checks if a shadowmap texel is mapped or not.
 *
 * @param MappingData	Array of shadowmap texels
 * @param X				X-coordinate for the texel to check
 * @param Y				Y-coordinate for the texel to check
 * @param Pitch			Number of texels per row
 * @return				TRUE if the texel is mapped
 */
FORCEINLINE UBOOL IsTexelMapped( const TArray<FQuantizedShadowSample>& MappingData, INT X, INT Y, INT Pitch )
{
	const FQuantizedShadowSample& Sample = MappingData(Y * Pitch + X);
	UBOOL bIsMapped = (Sample.Coverage > 0);
	return bIsMapped;
}

struct FShadowMapAllocation
{
	UShadowMap2D*			ShadowMap;
	UMaterialInterface*		Material;
	UObject*				TextureOuter;
	UObject*				Primitive;
	FGuid					LightGuid;
	/** Upper-left X-coordinate in the texture atlas. */
	INT						OffsetX;
	/** Upper-left Y-coordinate in the texture atlas. */
	INT						OffsetY;
	/** Total number of texels along the X-axis. */
	INT						TotalSizeX;
	/** Total number of texels along the Y-axis. */
	INT						TotalSizeY;
	/** The rectangle of mapped texels within this mapping that is placed in the texture atlas. */
	FIntRect				MappedRect;
	ELightMapPaddingType			PaddingType;
	TArray<FQuantizedShadowSample>	RawData;
	TArray<FQuantizedSignedDistanceFieldShadowSample> RawSignedDistanceFieldData;

	/** Bounds of the primitive that the mapping is applied to. */
	FBoxSphereBounds	Bounds;
	/** Bit-field with shadowmap flags. */
	EShadowMapFlags		ShadowmapFlags;

	/** True if we can skip encoding this allocation because it's similar enough to an existing
	    allocation at the same offset */
	UBOOL bSkipEncoding;
	/** True if this allocation is for an instanced static mesh for which want to force grouping */
	UBOOL bInstancedStaticMesh;

	template <class T>
	TArray<T>& GetRawData() 
	{ 
		TArray<T> Temp;
		check(0);
		return Temp;
	}

	template <>
	TArray<FQuantizedShadowSample>& GetRawData() 
	{ 
		return RawData;
	}

	template <>
	TArray<FQuantizedSignedDistanceFieldShadowSample>& GetRawData() 
	{ 
		return RawSignedDistanceFieldData;
	}

	FShadowMapAllocation()
	{
		PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = 0;
		MappedRect.Max.Y = 0;
		Primitive = NULL;
		bSkipEncoding = FALSE;
		bInstancedStaticMesh = FALSE;
	}
};



namespace TextureLayoutTools
{
	/**
	 * Computes the root mean square deviation for the difference between two allocations
	 *
	 * @param	AllocationA		First allocation to compare
	 * @param	AllocationB		Second allocation to compare
	 *
	 * @return	The root mean sequence deviation of the allocation's shadow map image difference
	 */
	template< typename T >
	DOUBLE ComputeRootMeanSquareDeviationForAllocations( FShadowMapAllocation& AllocationA, FShadowMapAllocation& AllocationB )
	{
		if( AllocationA.GetRawData<T>().Num() > 0 &&
			AllocationA.GetRawData<T>().Num() == AllocationB.GetRawData<T>().Num() )
		{
			// Compute the difference between the two images
			static TArray< DOUBLE > ValueDifferences;	// Static to reduce heap alloc thrashing
			ValueDifferences.Reset();
			{
				const BYTE* BytesArrayA = (BYTE*)AllocationA.GetRawData<T>().GetData();
				const BYTE* BytesArrayB = (BYTE*)AllocationB.GetRawData<T>().GetData();
				const INT ByteCount = AllocationA.GetRawData<T>().GetTypeSize() * AllocationA.GetRawData<T>().Num();
				ComputeDifferenceArray( BytesArrayA, BytesArrayB, ByteCount, ValueDifferences );
			}

			// Compute the root mean square deviation for the difference image
			const DOUBLE RMSD = ComputeRootMeanSquareDeviation( ValueDifferences.GetData(), ValueDifferences.Num() );
			return RMSD;
		}

		// Images are not compatible
		return 9999999.0;
	}
}


/** Largest boundingsphere radius to use when packing shadowmaps into a texture atlas. */
FLOAT GMaxShadowmapRadius = 2000.0f;

/** Whether to try to pack procbuilding lightmaps/shadowmaps into the same texture. */
extern UBOOL GGroupComponentLightmaps;

struct FShadowMapPendingTexture : FTextureLayout
{
	TArray<FShadowMapAllocation*> Allocations;

	UObject* Outer;
	UMaterialInterface* Material;
	FGuid LightGuid;
	UBOOL bShadowFactorData;
	UBOOL bInstancedStaticMesh;

	/** Bounds for all shadowmaps in this texture. */
	FBoxSphereBounds	Bounds;
	/** Bit-field with shadowmap flags that are shared among all shadowmaps in this texture. */
	EShadowMapFlags		ShadowmapFlags;

	/**
	 * Minimal initialization constructor.
	 */
	FShadowMapPendingTexture(UINT InSizeX,UINT InSizeY,UBOOL bInShadowFactorData):
		FTextureLayout(1,1,InSizeX,InSizeY,true),
		bShadowFactorData(bInShadowFactorData),
		bInstancedStaticMesh(FALSE)
	{}

	UBOOL AddElement( FShadowMapAllocation& Allocation, const UBOOL bForceIntoThisTexture = FALSE )
	{
		if( !bForceIntoThisTexture )
		{
			// Must be in the same package
			if ( Outer != Allocation.TextureOuter )
			{
				return FALSE;
			}

			// Must both be of the same type (standard or signed distance field)
			if ( bShadowFactorData != Allocation.ShadowMap->IsShadowFactorTexture() )
			{
				return FALSE;
			}

			// InstancedStaticMesh allocations can only be stored in InstancedStaticMesh shadowmap textures
			if (Allocation.bInstancedStaticMesh && !bInstancedStaticMesh)
			{
				return FALSE;
			}

			// InstancedStaticMeshComponent shadowmap textures must contain only instances from the same InstancedStaticMeshComponent.
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
			if ( (ShadowmapFlags & SMF_Streamed) != (Allocation.ShadowmapFlags & SMF_Streamed) )
			{
				return FALSE;
			}

			// If this is a streaming shadowmap?
			if ( ShadowmapFlags & SMF_Streamed )
			{
				UBOOL bPerformDistanceCheck = TRUE;

				// Try to group shadowmaps from the same component into the same texture (by disregarding the distance check)
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

				// Don't pack together shadowmaps that are too far apart.
				if ( bPerformDistanceCheck && NewBounds.SphereRadius > GMaxShadowmapRadius && NewBounds.SphereRadius > (Bounds.SphereRadius + SMALL_NUMBER) )
				{
					return FALSE;
				}
			}
		}

		// Whether we should combine mappings that are either exactly the same or very similar.
		// This requires some extra computation but may greatly reduce the memory used.
		const UBOOL bCombineSimilarMappings = GEngine->bCombineSimilarMappings || 
			(bInstancedStaticMesh && (Allocation.Primitive->GetOuter() == NULL || !Allocation.Primitive->GetOuter()->IsA(AInstancedFoliageActor::StaticClass())));
		const DOUBLE MaxAllowedRMSDForCombine = GEngine->MaxRMSDForCombiningMappings;

		UBOOL bFoundExistingMapping = FALSE;
		if( bCombineSimilarMappings ) 
		{
			// Check to see if this allocation closely matches an existing shadow map allocation.  If they're
			// almost exactly the same then we'll discard this allocation and use the existing mapping.
			for( INT PackedShadowMapIndex = 0; PackedShadowMapIndex < Allocations.Num(); ++PackedShadowMapIndex )						
			{
				FShadowMapAllocation& PackedAllocation = *Allocations( PackedShadowMapIndex );

				if( PackedAllocation.MappedRect == Allocation.MappedRect &&
					PackedAllocation.ShadowmapFlags == Allocation.ShadowmapFlags &&
					PackedAllocation.PaddingType == Allocation.PaddingType /*&&
					PackedAllocation.Bounds.Origin == Allocation.Bounds.Origin &&
					PackedAllocation.Bounds.SphereRadius == Allocation.Bounds.SphereRadius &&
					PackedAllocation.Bounds.BoxExtent == Allocation.Bounds.BoxExtent &&
					PackedAllocation.TotalSizeX == Allocation.TotalSizeX && 
					PackedAllocation.TotalSizeY == Allocation.TotalSizeY */ )
				{
					// Are the image exactly the same?
					if( PackedAllocation.RawData == Allocation.RawData &&
						PackedAllocation.RawSignedDistanceFieldData == Allocation.RawSignedDistanceFieldData )
					{
						// Shadow maps are the same!
						bFoundExistingMapping = TRUE;
						Allocation.OffsetX = PackedAllocation.OffsetX;
						Allocation.OffsetY = PackedAllocation.OffsetY;
						Allocation.bSkipEncoding = TRUE;

						// warnf( TEXT( "ShadowMapPacking: Exact Shared Allocation (%i texels)" ), Allocation.MappedRect.Area() );
						break;
					}

					// Images aren't an exact match, but check to see if they're close enough
					// @todo: Should compute RMSD per channel and use a separate max RMSD threshold for each
					const DOUBLE RMSD = Min(
						TextureLayoutTools::ComputeRootMeanSquareDeviationForAllocations< FQuantizedShadowSample >( PackedAllocation, Allocation ),
						TextureLayoutTools::ComputeRootMeanSquareDeviationForAllocations< FQuantizedSignedDistanceFieldShadowSample >( PackedAllocation, Allocation ) );
					if( RMSD <= MaxAllowedRMSDForCombine )
					{
						// Shadow maps are close enough!
						bFoundExistingMapping = TRUE;
						Allocation.OffsetX = PackedAllocation.OffsetX;
						Allocation.OffsetY = PackedAllocation.OffsetY;
						Allocation.bSkipEncoding = TRUE;

						// warnf( TEXT( "ShadowMapPacking: Approx Shared Allocation (RMSD %f) (%i texels)" ), RMSD, Allocation.MappedRect.Area() );
						break;
					}
				}
			}
		}

		if( !bFoundExistingMapping )
		{
			// 4-byte alignment, no more padding
			UINT PaddingBaseX = 0;
			UINT PaddingBaseY = 0;

			// See if the new one will fit in the existing texture
			if ( !FTextureLayout::AddElement(PaddingBaseX,PaddingBaseY,Allocation.MappedRect.Width(),Allocation.MappedRect.Height()) )
			{
				return FALSE;
			}

			// warnf( TEXT( "ShadowMapPacking: New Allocation (%i texels)" ), Allocation.MappedRect.Area() );

			// Position the shadow-maps in the middle of their padded space.
			Allocation.OffsetX = PaddingBaseX;
			Allocation.OffsetY = PaddingBaseY;
			Bounds = bEmptyTexture ? Allocation.Bounds : NewBounds;
		}


		// Add the shadow-map to the list of shadow-maps allocated space in this texture.
		Allocations.AddItem(&Allocation);

		return TRUE;
	}
};

static TIndirectArray<FShadowMapAllocation> PendingShadowMaps;
static UINT PendingShadowMapSize;
/** If TRUE, update the status when encoding light maps */
UBOOL UShadowMap2D::bUpdateStatus = TRUE;

#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

UShadowMap2D::UShadowMap2D( UObject* Primitive, const FShadowMapData2D& RawData,const FGuid& InLightGuid,UMaterialInterface* Material,const FBoxSphereBounds& Bounds,ELightMapPaddingType InPaddingType, EShadowMapFlags InShadowmapFlags, INT InInstanceIndex ):
	LightGuid(InLightGuid),
	Component(Cast<UInstancedStaticMeshComponent>(Primitive)),
	InstanceIndex(InInstanceIndex)
{
#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
	// Add a pending allocation for this shadow-map.
	FShadowMapAllocation* Allocation = new(PendingShadowMaps) FShadowMapAllocation;
	Allocation->ShadowMap		= this;
	Allocation->Material		= Material;
	Allocation->LightGuid		= InLightGuid;
	Allocation->TextureOuter	= GetOutermost();
	Allocation->Primitive		= Primitive;
	Allocation->TotalSizeX		= RawData.GetSizeX();
	Allocation->TotalSizeY		= RawData.GetSizeY();
	Allocation->MappedRect		= FIntRect( 0, 0, RawData.GetSizeX(), RawData.GetSizeY() );
	Allocation->PaddingType		= InPaddingType;
	Allocation->Bounds			= Bounds;
	// Set flag if this is an allocation for a static mesh component that has forced instance grouping.
	const UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(Primitive);
	Allocation->bInstancedStaticMesh = InstancedStaticMeshComponent && InstancedStaticMeshComponent->bDontResolveInstancedLightmaps;

	// Check to compress shadow map or not
	EShadowMapFlags SMFlags = InShadowmapFlags;
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo && WorldInfo->LightmassSettings.bCompressShadowmap)
	{
		SMFlags = EShadowMapFlags(SMFlags | SMF_Compressed);
	}
	Allocation->ShadowmapFlags	= SMFlags;
	if ( !GAllowStreamingLightmaps )
	{
		Allocation->ShadowmapFlags = EShadowMapFlags( Allocation->ShadowmapFlags & ~SMF_Streamed );
	}

	switch (RawData.GetType())
	{
		case FShadowMapData2D::SHADOW_FACTOR_DATA:
		case FShadowMapData2D::SHADOW_FACTOR_DATA_QUANTIZED:
			// If the data is already quantized, this will just copy the data
			RawData.Quantize( Allocation->RawData );
			bIsShadowFactorTexture = TRUE;
		break;

		case FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA:
		case FShadowMapData2D::SHADOW_SIGNED_DISTANCE_FIELD_DATA_QUANTIZED:
			// If the data is already quantized, this will just copy the data
			RawData.Quantize( Allocation->RawSignedDistanceFieldData );
			bIsShadowFactorTexture = FALSE;
		break;
	}

	// Track the size of pending light-maps.
	PendingShadowMapSize += Allocation->TotalSizeX * Allocation->TotalSizeY;

	// If we're allowed to eagerly encode shadowmaps, check to see if we're ready
	if (GAllowEagerLightmapEncode)
	{
		// Once there are enough pending shadow-maps, flush encoding.
		const UINT PackedLightAndShadowMapTextureSize = GWorld->GetWorldInfo()->PackedLightAndShadowMapTextureSize;
		const UINT MaxPendingShadowMapSize = Square(PackedLightAndShadowMapTextureSize) * 4;
		if(PendingShadowMapSize >= MaxPendingShadowMapSize)
		{
			EncodeTextures( TRUE );
		}
	}
#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
}

#if _MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER
INT CompareShadowmaps( FShadowMapAllocation* A, FShadowMapAllocation* B )
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
	return B->TotalSizeX * B->TotalSizeY - A->TotalSizeX * A->TotalSizeY;
}
IMPLEMENT_COMPARE_POINTER(FShadowMapAllocation,UnShadowMap,{ return CompareShadowmaps(A, B); });

/**
 * Executes all pending shadow-map encoding requests.
 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
 */

extern UBOOL NVTTCompress(UTexture2D* Texture2D, void* SourceData, EPixelFormat PixelFormat, INT SizeX, INT SizeY, UBOOL SRGB, UBOOL bIsNormalMap,  UBOOL bUseCUDAAcceleration, UBOOL bSupportDXT1a, INT QualityLevel );
extern UBOOL GUseCUDAAcceleration;
extern INT GLightmapEncodeQualityLevel;
extern UBOOL GAllowLightmapCompression;

void UShadowMap2D::EncodeTextures( UBOOL bLightingSuccessful )
{
	if ( bLightingSuccessful )
	{
		GWarn->BeginSlowTask(TEXT("Encoding shadow-maps"),FALSE);
		const INT PackedLightAndShadowMapTextureSize = GWorld->GetWorldInfo()->PackedLightAndShadowMapTextureSize;

		// Reset the pending shadow-map size.
		PendingShadowMapSize = 0;

		// Sort the light-maps in descending order by size.
		Sort<USE_COMPARE_POINTER(FShadowMapAllocation,UnShadowMap)>((FShadowMapAllocation**)PendingShadowMaps.GetData(),PendingShadowMaps.Num());

		// Allocate texture space for each light-map.
		TIndirectArray<FShadowMapPendingTexture> PendingTextures;
		for(INT ShadowMapIndex = 0;ShadowMapIndex < PendingShadowMaps.Num();ShadowMapIndex++)
		{
			FShadowMapAllocation& Allocation = PendingShadowMaps(ShadowMapIndex);
			
			if ( GAllowLightmapCropping && Allocation.ShadowMap->bIsShadowFactorTexture )
			{
				CropUnmappedTexels( Allocation.RawData, Allocation.TotalSizeX, Allocation.TotalSizeY, Allocation.MappedRect );
			}

			// Find an existing texture which the light-map can be stored in.
			FShadowMapPendingTexture* Texture = NULL;
			for(INT TextureIndex = 0;TextureIndex < PendingTextures.Num();TextureIndex++)
			{
				FShadowMapPendingTexture& ExistingTexture = PendingTextures(TextureIndex);

				// See if the new one will fit in the existing texture
				if ( ExistingTexture.AddElement( Allocation ) )
				{
					Texture = &ExistingTexture;
					break;
				}
			}

			// If there is no appropriate texture, create a new one.
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
				Texture				= ::new(PendingTextures) FShadowMapPendingTexture(NewTextureSizeX,NewTextureSizeY,Allocation.ShadowMap->bIsShadowFactorTexture);
				Texture->Material	= Allocation.Material;
				Texture->LightGuid	= Allocation.LightGuid;
				Texture->Outer		= Allocation.TextureOuter;
				Texture->Bounds		= Allocation.Bounds;
				Texture->ShadowmapFlags = Allocation.ShadowmapFlags;
				Texture->bInstancedStaticMesh = Allocation.bInstancedStaticMesh;
				verify( Texture->AddElement( Allocation, TRUE ) );
			}
		}
		
		
		if( GRepackLightAndShadowMapTextures )
		{
			// Optimize shadow map size by attempting to repack all shadow map textures at a smaller resolution.  In
			// general, this results in fewer rectangular shadow map textures and more-tightly packed square textures.
			RepackShadowMapTextures( PendingTextures, PackedLightAndShadowMapTextureSize );
		}

		
		for(INT TextureIndex = 0;TextureIndex < PendingTextures.Num();TextureIndex++)
		{
			if (bUpdateStatus)
			{
				GWarn->StatusUpdatef(TextureIndex,PendingTextures.Num(),LocalizeSecure(LocalizeUnrealEd(TEXT("EncodingShadowMapsF")),TextureIndex,PendingTextures.Num()));
			}

			FShadowMapPendingTexture& PendingTexture = PendingTextures(TextureIndex);

			UBOOL bShouldCompressed = ((PendingTexture.ShadowmapFlags & SMF_Compressed) != 0) && GAllowLightmapCompression;
			// Create the shadow-map texture.
			UShadowMapTexture2D* Texture = new(PendingTexture.Outer) UShadowMapTexture2D;
			Texture->SizeX			= PendingTexture.GetSizeX();
			Texture->SizeY			= PendingTexture.GetSizeY();
			Texture->Filter			= GUseBilinearLightmaps ? TF_Linear : TF_Nearest;
			//@todo - Compress shadow factor textures to DXT5 alpha
			Texture->Format			= bShouldCompressed ? PF_DXT1 : PF_G8;
			// Shadow factor textures benefit from being stored in gamma space, since they need more precision near 0 to avoid banding in the darks.
			// Signed distance field textures get stored in linear space, since they need more precision near .5.
			Texture->SRGB			= PendingTexture.bShadowFactorData ? TRUE : FALSE;
			Texture->LODGroup		= TEXTUREGROUP_Shadowmap;
			Texture->ShadowmapFlags	= PendingTexture.ShadowmapFlags;
			Texture->GenerateTextureFileCacheGUID(TRUE);

			if (PendingTexture.bShadowFactorData)
			{
				// Create the uncompressed top mip-level.
				TArray< TArray<FQuantizedShadowSample> > MipData;
				EncodeSingleTexture<FQuantizedShadowSample>(PendingTexture, Texture, MipData);

				// Copy the mip-map data into the UShadowMapTexture2D's mip-map array.
				for(INT MipIndex = 0;MipIndex < MipData.Num();MipIndex++)
				{				
					UINT MipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
					UINT MipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);

					if (!bShouldCompressed)
					{
						// Copy this mip-level into the texture's mips array.
						FTexture2DMipMap* MipMap = new(Texture->Mips) FTexture2DMipMap;
						MipMap->SizeX = MipSizeX;
						MipMap->SizeY = MipSizeY;
						MipMap->Data.Lock( LOCK_READ_WRITE );
						BYTE* DestMipData = (BYTE*) MipMap->Data.Realloc( MipSizeX * MipSizeY );
						for(UINT Y = 0;Y < MipSizeY;Y++)
						{
							for(UINT X = 0;X < MipSizeX;X++)
							{
								const FQuantizedShadowSample& SourceSample = MipData(MipIndex)(Y * MipSizeX + X);
								DestMipData[ Y * MipSizeX + X ] = SourceSample.Visibility;
							}
						}
						MipMap->Data.Unlock();
					}
					else
					{
						// need multi-threaded?
						TArray<FColor> Data;
						Data.Empty( MipSizeX * MipSizeY * sizeof(FColor) );
						for(UINT Y = 0;Y < MipSizeY;Y++)
						{
							for(UINT X = 0;X < MipSizeX;X++)
							{
								const FQuantizedShadowSample& SourceSample = MipData(MipIndex)(Y * MipSizeX + X);
								Data.AddItem(FColor(SourceSample.Visibility, SourceSample.Visibility, SourceSample.Visibility, 255));
							}
						}
						verify(NVTTCompress(Texture, &Data(0), PF_DXT1, MipSizeX, MipSizeY, Texture->SRGB, FALSE, GUseCUDAAcceleration, FALSE, GLightmapEncodeQualityLevel ));
					}
				}
			}
			else
			{
				// Create the uncompressed top mip-level.
				TArray< TArray<FQuantizedSignedDistanceFieldShadowSample> > MipData;
				EncodeSingleTexture<FQuantizedSignedDistanceFieldShadowSample>(PendingTexture, Texture, MipData);

				// Copy the mip-map data into the UShadowMapTexture2D's mip-map array.
				for(INT MipIndex = 0;MipIndex < MipData.Num();MipIndex++)
				{
					UINT MipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
					UINT MipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);

					if (!bShouldCompressed)
					{
						// Copy this mip-level into the texture's mips array.
						FTexture2DMipMap* MipMap = new(Texture->Mips) FTexture2DMipMap;
						MipMap->SizeX = MipSizeX;
						MipMap->SizeY = MipSizeY;
						MipMap->Data.Lock( LOCK_READ_WRITE );

						BYTE* DestMipData = (BYTE*) MipMap->Data.Realloc( MipSizeX * MipSizeY );
						for(UINT Y = 0;Y < MipSizeY;Y++)
						{
							for(UINT X = 0;X < MipSizeX;X++)
							{
								const FQuantizedSignedDistanceFieldShadowSample& SourceSample = MipData(MipIndex)(Y * MipSizeX + X);
								DestMipData[ Y * MipSizeX + X ] = SourceSample.Distance;
							}
						}
						MipMap->Data.Unlock();
					}
					else
					{
						TArray<FColor> Data;
						Data.Empty( MipSizeX * MipSizeY * sizeof(FColor) );
						for(UINT Y = 0;Y < MipSizeY;Y++)
						{
							for(UINT X = 0;X < MipSizeX;X++)
							{
								const FQuantizedSignedDistanceFieldShadowSample& SourceSample = MipData(MipIndex)(Y * MipSizeX + X);
								Data.AddItem(FColor(SourceSample.Distance, SourceSample.Distance, SourceSample.Distance, 255));
							}
						}
						verify(NVTTCompress(Texture, &Data(0), PF_DXT1, MipSizeX, MipSizeY, Texture->SRGB, FALSE, GUseCUDAAcceleration, FALSE, GLightmapEncodeQualityLevel ));
					}
				}
			}

			// Update stats.
			INT TextureSize				= Texture->CalcTextureMemorySize( TMC_AllMips );
			GNumShadowmapTotalTexels	+= Texture->SizeX * Texture->SizeY;
			GNumShadowmapTextures++;
			GShadowmapTotalSize			+= TextureSize;
			GShadowmapTotalSize360		+= Texture->Get360Size( Texture->Mips.Num() );
			GShadowmapTotalStreamingSize += (PendingTexture.ShadowmapFlags & SMF_Streamed) ? TextureSize : 0;
			UPackage* TexturePackage = Texture->GetOutermost();
			for ( INT LevelIndex=0; TexturePackage && LevelIndex < GWorld->Levels.Num(); LevelIndex++ )
			{
				ULevel* Level = GWorld->Levels(LevelIndex);
				UPackage* LevelPackage = Level->GetOutermost();
				if ( TexturePackage == LevelPackage )
				{
					Level->ShadowmapTotalSize += FLOAT(TextureSize) / 1024.0f;
					break;
				}
			}

			// Update the texture resource.
			Texture->UpdateResource();
		}

		PendingTextures.Empty();
		PendingShadowMaps.Empty();

		GWarn->EndSlowTask();
	}
	else
	{
		PendingShadowMaps.Empty();
	}
}


/**
 * Static: Repacks shadow map textures to optimize texture memory usage
 *
 * @param	PendingTextures							(In, Out) Array of packed shadow map textures
 * @param	InPackedLightAndShadowMapTextureSize	Target texture size for light and shadow maps
 */
void UShadowMap2D::RepackShadowMapTextures( TIndirectArray<FShadowMapPendingTexture>& PendingTextures, const INT InPackedLightAndShadowMapTextureSize )
{
	TArray<FShadowMapAllocation*> NewPendingShadowMaps;
	TArray<FShadowMapPendingTexture*> NewPendingTextures;

	UBOOL bRepackSucceeded = TRUE;
	for(INT TextureIndex = 0;TextureIndex < PendingTextures.Num();TextureIndex++)
	{
		const FShadowMapPendingTexture* OriginalTexture = &PendingTextures( TextureIndex );
		TArray<FShadowMapAllocation*> ShadowMapsToRepack;


		// Make a list of shadow maps to repack, copying the original allocations
		for( INT CurAllocation = 0; CurAllocation < OriginalTexture->Allocations.Num(); ++CurAllocation )
		{
			const FShadowMapAllocation* OriginalAllocation = OriginalTexture->Allocations( CurAllocation );

			FShadowMapAllocation* ShadowMapCopy = new FShadowMapAllocation();
			*ShadowMapCopy = *OriginalAllocation;
			ShadowMapsToRepack.AddItem( ShadowMapCopy );
			NewPendingShadowMaps.AddItem( ShadowMapCopy );
		}


		// Sort the shadow-maps in descending order by size.
		Sort<USE_COMPARE_POINTER(FShadowMapAllocation,UnShadowMap)>((FShadowMapAllocation**)ShadowMapsToRepack.GetData(),ShadowMapsToRepack.Num());


		// How many square powers of two sizes to attempt repacking this texture into, smallest first
		const INT MaxAttempts = 4;
		FShadowMapPendingTexture* RepackedTexture = NULL;
		for( INT AttemptIndex = 0; AttemptIndex <= MaxAttempts; ++AttemptIndex )
		{
			INT ShadowMapIndex = 0;
			for( ; ShadowMapIndex < ShadowMapsToRepack.Num(); ++ShadowMapIndex )
			{
				FShadowMapAllocation& Allocation = *ShadowMapsToRepack( ShadowMapIndex );

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
					RepackedTexture				= new FShadowMapPendingTexture(NewTextureSizeX,NewTextureSizeY,Allocation.ShadowMap->bIsShadowFactorTexture);
					RepackedTexture->Material	= Allocation.Material;
					RepackedTexture->LightGuid	= Allocation.LightGuid;
					RepackedTexture->Outer		= Allocation.TextureOuter;
					RepackedTexture->Bounds		= Allocation.Bounds;
					RepackedTexture->ShadowmapFlags = Allocation.ShadowmapFlags;
					if( !RepackedTexture->AddElement( Allocation, bForceIntoThisTexture ) )
					{
						// This mapping didn't fit into the shadow map texture; bail out
						break;
					}
				}
			}

			if( ShadowMapIndex == ShadowMapsToRepack.Num() )
			{
				// All shadow maps were successfully packed into the texture!
				break;
			}
			else
			{
				// Failed to pack shadow maps into the texture
				delete RepackedTexture;
				RepackedTexture = NULL;
			}
		}

		// The texture should have always been repacked successfully.  If this assert goes off it means
		// that repacking is no longer deterministic with the first shadow map pack phase.  We always repack
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
		// Replace the textures with our repacked textures
		check( NewPendingTextures.Num() == PendingTextures.Num() );
		PendingTextures.Empty();
		for( INT TextureIndex = 0; TextureIndex < NewPendingTextures.Num(); ++TextureIndex )
		{
			PendingTextures.AddRawItem( NewPendingTextures( TextureIndex ) );
		}

		// Replace the old shadow map array with the new one.  This will delete the old shadow map allocations.
		check( NewPendingShadowMaps.Num() == PendingShadowMaps.Num() );
		PendingShadowMaps.Empty();
		for( INT CurShadowMapIndex = 0; CurShadowMapIndex < NewPendingShadowMaps.Num(); ++CurShadowMapIndex )
		{
			PendingShadowMaps.AddRawItem( NewPendingShadowMaps( CurShadowMapIndex ) );
		}
	}
}



template<class ShadowSampleType>
void UShadowMap2D::EncodeSingleTexture(FShadowMapPendingTexture& PendingTexture, UShadowMapTexture2D* Texture, TArray< TArray<ShadowSampleType> >& MipData)
{
	TArray<ShadowSampleType>* TopMipData = new(MipData) TArray<ShadowSampleType>();
	TopMipData->Empty(PendingTexture.GetSizeX() * PendingTexture.GetSizeY());
	TopMipData->AddZeroed(PendingTexture.GetSizeX() * PendingTexture.GetSizeY());
	for(INT AllocationIndex = 0;AllocationIndex < PendingTexture.Allocations.Num();AllocationIndex++)
	{
		FShadowMapAllocation& Allocation = *PendingTexture.Allocations(AllocationIndex);

		// Don't bother encoding this allocation if we were asked to skip it
		if( !Allocation.bSkipEncoding )
		{
			// Copy the raw data for this light-map into the raw texture data array.
			for(INT Y = Allocation.MappedRect.Min.Y; Y < Allocation.MappedRect.Max.Y; ++Y)
			{
				for(INT X = Allocation.MappedRect.Min.X; X < Allocation.MappedRect.Max.X; ++X)
				{
					INT DestY = Y - Allocation.MappedRect.Min.Y + Allocation.OffsetY;
					INT DestX = X - Allocation.MappedRect.Min.X + Allocation.OffsetX;
					ShadowSampleType& DestSample = (*TopMipData)(DestY * Texture->SizeX + DestX);
					const ShadowSampleType& SourceSample = Allocation.GetRawData<ShadowSampleType>()(Y * Allocation.TotalSizeX + X);
					DestSample = SourceSample;
#if !CONSOLE
					if ( SourceSample.Coverage > 0 )
					{
						GNumShadowmapMappedTexels++;
					}
					else
					{
						GNumShadowmapUnmappedTexels++;
					}
#endif
				}
			}
		}

		// Link the shadow-map to the texture.
		Allocation.ShadowMap->Texture = Texture;

		// Free the shadow-map's raw data.
		Allocation.GetRawData<ShadowSampleType>().Empty();

		INT PaddedSizeX = Allocation.TotalSizeX;
		INT PaddedSizeY = Allocation.TotalSizeY;
		INT BaseX = Allocation.OffsetX - Allocation.MappedRect.Min.X;
		INT BaseY = Allocation.OffsetY - Allocation.MappedRect.Min.Y;
#if !CONSOLE
		if (GLightmassDebugOptions.bPadMappings && (Allocation.PaddingType == LMPT_NormalPadding))
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
		// Calculate the coordinate scale/biases for each shadow-map stored in the texture.
		Allocation.ShadowMap->CoordinateScale = FVector2D(
			(FLOAT)PaddedSizeX / (FLOAT)PendingTexture.GetSizeX(),
			(FLOAT)PaddedSizeY / (FLOAT)PendingTexture.GetSizeY()
			);
		Allocation.ShadowMap->CoordinateBias = FVector2D(
			(FLOAT)BaseX / (FLOAT)PendingTexture.GetSizeX(),
			(FLOAT)BaseY / (FLOAT)PendingTexture.GetSizeY()
			);

		// If this shadow map is being generated for an instanced static mesh, then we'll update
		// the per-instance data with information about the mapping
		if( Allocation.ShadowMap->Component != NULL )
		{
			FInstancedStaticMeshInstanceData& InstanceData = Allocation.ShadowMap->Component->PerInstanceSMData( Allocation.ShadowMap->InstanceIndex );
			InstanceData.ShadowmapUVBias = Allocation.ShadowMap->CoordinateBias;

			// @todo: Support multiple shadow maps?
			FInstancedStaticMeshMappingInfo& InstanceMapping = Allocation.ShadowMap->Component->CachedMappings( Allocation.ShadowMap->InstanceIndex );
			check( InstanceMapping.ShadowmapTexture == NULL );

			// remember what UShadowMap2D this shadowmap was put into, so we can later split up the component based on shadowmap texture
			InstanceMapping.ShadowmapTexture = Allocation.ShadowMap;
		}
	}

	const UINT NumMips = Max(appCeilLogTwo(Texture->SizeX),appCeilLogTwo(Texture->SizeY)) + 1;
	for(UINT MipIndex = 1;MipIndex < NumMips;MipIndex++)
	{
		const UINT SourceMipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> (MipIndex - 1));
		const UINT SourceMipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> (MipIndex - 1));
		const UINT DestMipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
		const UINT DestMipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);

		// Downsample the previous mip-level, taking into account which texels are mapped.
		TArray<ShadowSampleType>* NextMipData = new(MipData) TArray<ShadowSampleType>();
		NextMipData->Empty(DestMipSizeX * DestMipSizeY);
		NextMipData->AddZeroed(DestMipSizeX * DestMipSizeY);
		const UINT MipFactorX = SourceMipSizeX / DestMipSizeX;
		const UINT MipFactorY = SourceMipSizeY / DestMipSizeY;
		for(UINT Y = 0;Y < DestMipSizeY;Y++)
		{
			for(UINT X = 0;X < DestMipSizeX;X++)
			{
				FLOAT AccumulatedFilterableComponents[ShadowSampleType::NumFilterableComponents];
				for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
				{
					AccumulatedFilterableComponents[i] = 0;
				}
				UINT Coverage = 0;
				for(UINT SourceY = Y * MipFactorY;SourceY < (Y + 1) * MipFactorY;SourceY++)
				{
					for(UINT SourceX = X * MipFactorX;SourceX < (X + 1) * MipFactorX;SourceX++)
					{
						const ShadowSampleType& SourceSample = MipData(MipIndex - 1)(SourceY * SourceMipSizeX + SourceX);
						if(SourceSample.Coverage)
						{
							for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
							{
								AccumulatedFilterableComponents[i] += SourceSample.GetFilterableComponent(i) * SourceSample.Coverage;
							}

							Coverage += SourceSample.Coverage;
						}
					}
				}
				ShadowSampleType& DestSample = (*NextMipData)(Y * DestMipSizeX + X);
				if(Coverage)
				{
					for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
					{
						DestSample.SetFilterableComponent(AccumulatedFilterableComponents[i] / (FLOAT)Coverage, i);
					}

					DestSample.Coverage = (BYTE)(Coverage / (MipFactorX * MipFactorY));
				}
				else
				{
					for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
					{
						AccumulatedFilterableComponents[i] = 0;
					}
					DestSample.Coverage = 0;
				}
			}
		}
	}

	const FIntPoint Neighbors[] = 
	{
		// Check immediate neighbors first
		FIntPoint(1,0),
		FIntPoint(0,1),
		FIntPoint(-1,0),
		FIntPoint(0,-1),
		// Check diagonal neighbors if no immediate neighbors are found
		FIntPoint(1,1),
		FIntPoint(-1,1),
		FIntPoint(-1,-1),
		FIntPoint(1,-1)
	};

	// Extrapolate texels which are mapped onto adjacent texels which are not mapped to avoid artifacts when using texture filtering.
	for(INT MipIndex = 0;MipIndex < MipData.Num();MipIndex++)
	{
		UINT MipSizeX = Max(GPixelFormats[Texture->Format].BlockSizeX,Texture->SizeX >> MipIndex);
		UINT MipSizeY = Max(GPixelFormats[Texture->Format].BlockSizeY,Texture->SizeY >> MipIndex);
		for(UINT DestY = 0;DestY < MipSizeY;DestY++)
		{
			for(UINT DestX = 0;DestX < MipSizeX;DestX++)
			{
				ShadowSampleType& DestSample = MipData(MipIndex)(DestY * MipSizeX + DestX);
				if (DestSample.Coverage == 0)
				{
					FLOAT ExtrapolatedFilterableComponents[ShadowSampleType::NumFilterableComponents];
					for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
					{
						ExtrapolatedFilterableComponents[i] = 0;
					}

					for (INT NeighborIndex = 0; NeighborIndex < ARRAY_COUNT(Neighbors); NeighborIndex++)
					{
						if (DestY + Neighbors[NeighborIndex].Y >= 0 
							&& DestY + Neighbors[NeighborIndex].Y < MipSizeY
							&& DestX + Neighbors[NeighborIndex].X >= 0 
							&& DestX + Neighbors[NeighborIndex].X < MipSizeX)
						{
							const ShadowSampleType& NeighborSample = MipData(MipIndex)((DestY + Neighbors[NeighborIndex].Y) * MipSizeX + DestX + Neighbors[NeighborIndex].X);
							if (NeighborSample.Coverage > 0)
							{
								if (DestY + Neighbors[NeighborIndex].Y * 2 >= 0 
									&& DestY + Neighbors[NeighborIndex].Y * 2 < MipSizeY
									&& DestX + Neighbors[NeighborIndex].X * 2 >= 0 
									&& DestX + Neighbors[NeighborIndex].X * 2 < MipSizeX)
								{
									// Lookup the second neighbor in the first neighbor's direction
									//@todo - check the second neighbor's coverage?
									const ShadowSampleType& SecondNeighborSample = MipData(MipIndex)((DestY + Neighbors[NeighborIndex].Y * 2) * MipSizeX + DestX + Neighbors[NeighborIndex].X * 2);
									for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
									{
										// Extrapolate while maintaining the first derivative, which is especially important for signed distance fields
										ExtrapolatedFilterableComponents[i] = NeighborSample.GetFilterableComponent(i) * 2.0f - SecondNeighborSample.GetFilterableComponent(i);
									}
								}
								else
								{
									// Couldn't find a second neighbor to use for extrapolating, just copy the neighbor's values
									for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
									{
										ExtrapolatedFilterableComponents[i] = NeighborSample.GetFilterableComponent(i);
									}
								}
								break;
							}
						}
					}
					for (INT i = 0; i < ShadowSampleType::NumFilterableComponents; i++)
					{
						DestSample.SetFilterableComponent(ExtrapolatedFilterableComponents[i], i);
					}
				}
			}
		}
	}
}

#endif //_MSC_VER && !CONSOLE && !UE3_LEAN_AND_MEAN && !DEDICATED_SERVER

UShadowMap1D::UShadowMap1D(const FGuid& InLightGuid,const FShadowMapData1D& Data):
	LightGuid(InLightGuid)
{
	// Copy the shadow occlusion samples.
	Samples.Empty(Data.GetSize());
	for(INT SampleIndex = 0;SampleIndex < Data.GetSize();SampleIndex++)
	{
		Samples.AddItem(Data(SampleIndex));
	}

	// Initialize the vertex buffer.
	BeginInitResource(this);

	INC_DWORD_STAT_BY(STAT_VertexLightingAndShadowingMemory, Samples.GetResourceDataSize());
}

void UShadowMap1D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Samples;
	Ar << LightGuid;
}

void UShadowMap1D::PostLoad()
{
	Super::PostLoad();
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Initialize the vertex buffer.
		BeginInitResource(this);
		INC_DWORD_STAT_BY(STAT_VertexLightingAndShadowingMemory, Samples.GetResourceDataSize());
	}
}

void UShadowMap1D::BeginDestroy()
{
	Super::BeginDestroy();
#if EXPERIMENTAL_FAST_BOOT_IPHONE
	// the class default object gets tossed in UClass:Link() time, and blocks the game thread,
	// and CDO shouldn't have RT resources anyway?
	if (!IsTemplate())
#endif
	{
		BeginReleaseResource(this);
		DEC_DWORD_STAT_BY(STAT_VertexLightingAndShadowingMemory, Samples.GetResourceDataSize());
		ReleaseFence.BeginFence();
	}
}

UBOOL UShadowMap1D::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && ReleaseFence.GetNumPendingFences() == 0;
}

void UShadowMap1D::InitRHI()
{
	// Compute the vertex buffer size.
	if( Samples.GetResourceDataSize() > 0 )
	{
		// Create the light-map vertex buffer.
		VertexBufferRHI = RHICreateVertexBuffer(Samples.GetResourceDataSize(),&Samples,RUF_Static);
	}
}

/**
 * Reorders the samples based on the given reodering map
 *
 * @param SampleRemapping The mapping of new sample index to old sample index
 */
void UShadowMap1D::ReorderSamples(const TArray<INT>& SampleRemapping)
{
	if (SampleRemapping.Num() != Samples.Num())
	{
		warnf(NAME_Warning, TEXT("Shadowmap has different number of samples than the remapping data, %d vs %d [%s]"), Samples.Num(), SampleRemapping.Num(), *GetFullName());
		return;
	}

	TArray<FLOAT> OriginalSamples = Samples;
	TArray<FLOAT> NewSamples;
	NewSamples.Add(OriginalSamples.Num());
	// remap the samples
	for (INT SampleIndex = 0; SampleIndex < OriginalSamples.Num(); SampleIndex++)
	{
		NewSamples(SampleIndex) = OriginalSamples(SampleRemapping(SampleIndex));
	}
	Samples = NewSamples;

	// since this is untested except in the cooking, break in the non-commandlet case until a test case exists
	checkf(GIsCooking, TEXT("Attempting to reorder a shadowmap in an untested use case"));

	/**
	 * We need to do something like this for other use cases of this function, but without a test case, leaving this out for now

		if ( GIsRHIInitialized && IsInRenderingThread() )
		{
			InitResource();
		}
		else
		{
			BeginInitResource(this);
		}

	*/
}

/**
 * Creates a new shadowmap that is a copy of this shadow map, but where the sample data
 * has been remapped according to the specified sample index remapping.
 *
 * @param SampleRemapping	Sample remapping: Dst[i] = Src[RemappedIndices[i]].
 * @param NewOuter Outer to use for the newly constructed copy
 * @return The new shadowmap.
 */
UShadowMap1D* UShadowMap1D::DuplicateWithRemappedVerts(const TArray<INT>& SampleRemapping, UObject* NewOuter)
{
	check( SampleRemapping.Num() > 0);

	UShadowMap1D* NewShadowMap = NULL;

	// Shadowmap source samples are only available while in editor
	if (GIsEditor)
	{
		NewOuter = NewOuter != NULL ? NewOuter : GetOuter();

		FShadowMapData1D NewSamples(SampleRemapping.Num());	
		// remap the samples
		for (INT SampleIndex = 0; SampleIndex < NewSamples.GetSize(); SampleIndex++)
		{
			NewSamples(SampleIndex) = Samples(SampleRemapping(SampleIndex));
		}
		// create the new shadow map object for use with this decal
		NewShadowMap = new(NewOuter) UShadowMap1D(LightGuid,NewSamples);
	}

	return NewShadowMap;
}
