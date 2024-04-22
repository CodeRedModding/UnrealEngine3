/*=============================================================================
	Texture2D.cpp: Implementation of UTexture2D.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

#if PS3
#include "FFileManagerPS3.h"
#include "PS3DownloadableContent.h"
#endif

#if _WINDOWS
#include "UnConsoleTools.h"
#include "UnConsoleSupportContainer.h"
#endif

#if WITH_SUBSTANCE_AIR == 1
#define WITH_SUBSTANCE_AIR_VERBOSE 0
#include "SubstanceAirGraph.h"
#include "SubstanceAirTextureClasses.h"
#include <atc_api.h>
atc_api::Reader* GSubstanceAirTextureReader = NULL;

/**
  * @brief This is an async RTC reader job used to read mipmaps
  */
class ATCReadWork : public FQueuedWork
{
	FString					TextureName;
	size_t					Width;
	size_t					Height;
	void *					Destination;
	size_t					DestSize;
	FThreadSafeCounter *	Counter;

public:
	ATCReadWork(
		const FString & TexName, 
		const size_t W, 
		const size_t H, 
		void* Dest, 
		const size_t DestSize, 
		FThreadSafeCounter* InCounter) :
			TextureName(TexName),
			Width(W), 
			Height(H),
			Destination(Dest),
			DestSize(DestSize),
			Counter(InCounter)
	{
	}

	virtual void DoThreadedWork()
	{
		if (!GSubstanceAirTextureReader)
		{
			return;
		}

		GSubstanceAirTextureReader->getMipMap(
			TCHAR_TO_UTF8(*TextureName),
			Width, 
			Height, 
			(char*)Destination, 
			DestSize);

		Counter->Decrement();
	}

	virtual void Abandon() {}

	virtual void Dispose() {}
};

#endif // WITH_SUBSTANCE_AIR


/*-----------------------------------------------------------------------------
	Global helper functions
-----------------------------------------------------------------------------*/

DECLARE_MEMORY_STAT2(TEXT("Packed Mip-Tail Savings"),STAT_TexturePool_PackMipTailSavings,STATGROUP_TexturePool,MCR_TexturePool1,FALSE);

DECLARE_STATS_GROUP( TEXT("TextureGroup"), STATGROUP_TextureGroup );

#define TEXGROUPSTAT(g) DECLARE_MEMORY_STAT2(TEXT(#g),STAT_##g,STATGROUP_TextureGroup,MCR_TexturePool1,FALSE);
FOREACH_ENUM_TEXTUREGROUP(TEXGROUPSTAT)
	TEXGROUPSTAT(TEXTUREGROUP_Unknown)
#undef TEXGROUPSTAT

/** Peak lightmap memory. Used by ChartCreation.cpp. */
STAT( DWORD GMaxTextureLightmapMemory = 0 );
/** Peak shadowmap memory. Used by ChartCreation.cpp. */
STAT( DWORD GMaxTextureShadowmapMemory = 0 );
/** Peak lightmap memory, if this was running on Xbox. Used by ChartCreation.cpp. */
STATWIN( DWORD GMaxTextureLightmapMemoryXbox = 0 );
/** Peak shadowmap memory, if this was running on Xbox. Used by ChartCreation.cpp. */
STATWIN( DWORD GMaxTextureShadowmapMemoryXbox = 0 );

#if STATS
inline void IncMemoryStats( const UTexture2D* Texture )
{
	if ( Texture->IsA( ULightMapTexture2D::StaticClass() ) && ((ULightMapTexture2D*)Texture)->IsSimpleLightmap() == FALSE )
	{
		INT TextureSize = Texture->CalcTextureMemorySize( Texture->Mips.Num() );
		INC_DWORD_STAT_BY( STAT_TextureLightmapMemory, TextureSize );
		GMaxTextureLightmapMemory = Max<DWORD>(GMaxTextureLightmapMemory, GStatManager.GetStatValueDWORD(STAT_TextureLightmapMemory));
	}
	else if ( Texture->IsA( UShadowMapTexture2D::StaticClass() ) )
	{
		INT TextureSize = Texture->CalcTextureMemorySize( Texture->Mips.Num() );
		INC_DWORD_STAT_BY( STAT_TextureShadowmapMemory, TextureSize );
		GMaxTextureShadowmapMemory = Max<DWORD>(GMaxTextureShadowmapMemory, GStatManager.GetStatValueDWORD(STAT_TextureShadowmapMemory));
	}
}

inline void DecMemoryStats( const UTexture2D* Texture )
{
	if ( Texture->IsA( ULightMapTexture2D::StaticClass() ) && ((ULightMapTexture2D*)Texture)->IsSimpleLightmap() == FALSE )
	{
		INT TextureSize = Texture->CalcTextureMemorySize( Texture->Mips.Num() );
		DEC_DWORD_STAT_BY( STAT_TextureLightmapMemory, TextureSize );
	}
	else if ( Texture->IsA( UShadowMapTexture2D::StaticClass() ) )
	{
		INT TextureSize = Texture->CalcTextureMemorySize( Texture->Mips.Num() );
		DEC_DWORD_STAT_BY( STAT_TextureShadowmapMemory, TextureSize );
	}
}

#if _WINDOWS
inline void IncMemoryStats360( const UTexture2D* Texture )
{
	if ( Texture->IsA( ULightMapTexture2D::StaticClass() ) && ((ULightMapTexture2D*)Texture)->IsSimpleLightmap() == FALSE )
	{
		INT TextureSize_360 = Texture->Get360Size( Texture->Mips.Num() );
		INC_DWORD_STAT_BY( STAT_XboxTextureLightmapMemory, TextureSize_360 );
		GMaxTextureLightmapMemoryXbox = Max<DWORD>(GMaxTextureLightmapMemoryXbox, GStatManager.GetStatValueDWORD(STAT_XboxTextureLightmapMemory));
	}
	else if ( Texture->IsA( UShadowMapTexture2D::StaticClass() ) )
	{
		INT TextureSize_360 = Texture->Get360Size( Texture->Mips.Num() );
		INC_DWORD_STAT_BY( STAT_XboxTextureShadowmapMemory, TextureSize_360 );
		GMaxTextureShadowmapMemoryXbox = Max<DWORD>(GMaxTextureShadowmapMemoryXbox, GStatManager.GetStatValueDWORD(STAT_XboxTextureShadowmapMemory));
	}
}

inline void DecMemoryStats360( const UTexture2D* Texture )
{
	if ( Texture->IsA( ULightMapTexture2D::StaticClass() ) && ((ULightMapTexture2D*)Texture)->IsSimpleLightmap() == FALSE )
	{
		INT TextureSize_360 = Texture->Get360Size( Texture->Mips.Num() );
		DEC_DWORD_STAT_BY( STAT_XboxTextureLightmapMemory, TextureSize_360 );
	}
	else if ( Texture->IsA( UShadowMapTexture2D::StaticClass() ) )
	{
		INT TextureSize_360 = Texture->Get360Size( Texture->Mips.Num() );
		DEC_DWORD_STAT_BY( STAT_XboxTextureShadowmapMemory, TextureSize_360 );
	}
}
#endif
#endif

/** Number of times to retry to reallocate a texture before trying a panic defragmentation, the first time. */
INT GDefragmentationRetryCounter = 10;
/** Number of times to retry to reallocate a texture before trying a panic defragmentation, subsequent times. */
INT GDefragmentationRetryCounterLong = 100;

/** Turn on ENABLE_TEXTURE_TRACKING in UnContentStreaming.cpp and setup GTrackedTextures to track specific textures through the streaming system. */
extern UBOOL TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, UBOOL bIsDestroying, UBOOL bEnableLogging, UBOOL bForceMipLevelsToBeResident );

IMPLEMENT_CLASS(UTexture2D);

/** Scoped debug info that provides the texture name to memory allocation and crash callstacks. */
class FTexture2DScopedDebugInfo : public FScopedDebugInfo
{
public:

	/** Initialization constructor. */
	FTexture2DScopedDebugInfo(const UTexture2D* InTexture):
		FScopedDebugInfo(0),
		Texture(InTexture)
	{}

	// FScopedDebugInfo interface.
	virtual FString GetFunctionName() const
	{
		return FString::Printf(
			TEXT("%s (%ux%u %s, %u mips, LODGroup=%u)"),
			*Texture->GetPathName(),
			Texture->SizeX,
			Texture->SizeY,
			GPixelFormats[Texture->Format].Name,
			Texture->Mips.Num(),
			Texture->LODGroup
			);
	}
	virtual FString GetFilename() const
	{
		return FString::Printf(
			TEXT("%s..\\..\\Development\\Src\\Engine\\%s"),
			appBaseDir(),
			ANSI_TO_TCHAR(__FILE__)
			);
	}
	virtual INT GetLineNumber() const
	{
		return __LINE__;
	}

private:

	const UTexture2D* Texture;
};

/*-----------------------------------------------------------------------------
	init static global instances
-----------------------------------------------------------------------------*/

/** First streamable texture link. Not handled by GC as BeginDestroy automatically unlinks.	*/
TLinkedList<UTexture2D*>* UTexture2D::FirstStreamableLink = NULL;
/** Current streamable texture link for iteration over textures. Not handled by GC as BeginDestroy automatically unlinks. */
TLinkedList<UTexture2D*>* UTexture2D::CurrentStreamableLink = NULL;
/** Number of streamable textures. */
INT UTexture2D::NumStreamableTextures = 0;

/*-----------------------------------------------------------------------------
	FTextureMipBulkData
-----------------------------------------------------------------------------*/

/**
* Get resource memory preallocated for serializing bulk data into
* This is typically GPU accessible memory to avoid multiple allocations copies from system memory
* If NULL is returned then default to allocating from system memory
*
* @param Owner	object with bulk data being serialized
* @param Idx	entry when serializing out of an array
* @return pointer to resource memory or NULL
*/
void* FTextureMipBulkData::GetBulkDataResourceMemory(UObject* Owner,INT MipIdx)
{
	// obtain the resource memory for the texture
	UTexture2D* Texture2D = CastChecked<UTexture2D>(Owner);
	// initialize the resource memory container with the first requested mip index
	FTexture2DResourceMem* ResourceMem = Texture2D->InitResourceMem(MipIdx);
	// get offset into the mip data based on the mip index requested
	void* Result = ResourceMem ? ResourceMem->GetMipData(MipIdx - Texture2D->FirstResourceMemMip) : NULL;
	if( Result )
	{
		// if we're using the resource memory container then the bulk data should never free this memory
		bShouldFreeOnEmpty = FALSE;
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	Linker file package summary texture allocation info
-----------------------------------------------------------------------------*/

FTextureAllocations::FTextureType::FTextureType()
:	SizeX(0)
,	SizeY(0)
,	NumMips(0)
,	Format(PF_Unknown)
,	TexCreateFlags(0)
,	NumExportIndicesProcessed(0)
{
}

FTextureAllocations::FTextureType::FTextureType( INT InSizeX, INT InSizeY, INT InNumMips, DWORD InFormat, DWORD InTexCreateFlags )
:	SizeX(InSizeX)
,	SizeY(InSizeY)
,	NumMips(InNumMips)
,	Format(InFormat)
,	TexCreateFlags(InTexCreateFlags)
,	NumExportIndicesProcessed(0)
{
}

/**
 * Serializes an FTextureType
 */
FArchive& operator<<( FArchive& Ar, FTextureAllocations::FTextureType& TextureType )
{
	Ar << TextureType.SizeX;
	Ar << TextureType.SizeY;
	Ar << TextureType.NumMips;
	Ar << TextureType.Format;
	Ar << TextureType.TexCreateFlags;
	Ar << TextureType.ExportIndices;
	return Ar;
}

/**
 * Serializes an FTextureAllocations struct
 */
FArchive& operator<<( FArchive& Ar, FTextureAllocations& TextureAllocations )
{
	INT NumSummaryTextures = 0;
	INT NumExportTexturesTotal = 0;
	INT NumExportTexturesAdded = 0;

	// Are we cooking a package?
	if ( Ar.IsSaving() && !Ar.IsTransacting() && GIsCooking )
	{
		ULinker* Linker = Ar.GetLinker();

		// Do we need to build the texture allocation data?
		if ( TextureAllocations.TextureTypes.Num() == 0 )
		{
			for ( FObjectIterator It; It; ++It )
			{
				UObject* Object = *It;
				if ( Object->HasAnyFlags(RF_TagExp) && !Object->HasAnyFlags(RF_ClassDefaultObject) && Object->IsA(UTexture2D::StaticClass()) )
				{
					UTexture2D* Texture2D = Cast<UTexture2D>(Object);
					INT PreAllocateSizeX = 0;
					INT PreAllocateSizeY = 0;
					INT PreAllocateNumMips = 0;
					DWORD TexCreateFlags = 0;
					if ( Texture2D->GetResourceMemSettings(Texture2D->FirstResourceMemMip, PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, TexCreateFlags ) )
					{
						TextureAllocations.AddResourceMemInfo(PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, Texture2D->Format, TexCreateFlags );
					}
				}
			}
		}
		// Do we need to fixup the export indices?
		else if ( Ar.GetLinker() )
		{
			NumSummaryTextures = 0;
			ULinker* Linker = Ar.GetLinker();
			for ( INT TypeIndex=0; TypeIndex < TextureAllocations.TextureTypes.Num(); ++TypeIndex )
			{
				FTextureAllocations::FTextureType& TextureType = TextureAllocations.TextureTypes( TypeIndex );
				NumSummaryTextures += TextureType.ExportIndices.Num();
				TextureType.ExportIndices.Empty();
			}

			NumExportTexturesTotal = 0;
			NumExportTexturesAdded = 0;
			for ( INT ExportIndex=0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex )
			{
				UTexture2D* Texture2D = Cast<UTexture2D>(Linker->ExportMap(ExportIndex)._Object);
				if ( Texture2D && !Texture2D->HasAnyFlags(RF_ClassDefaultObject) )
				{
					NumExportTexturesTotal++;
					INT PreAllocateSizeX = 0;
					INT PreAllocateSizeY = 0;
					INT PreAllocateNumMips = 0;
					DWORD TexCreateFlags = 0;
					if ( Texture2D->GetResourceMemSettings(Texture2D->FirstResourceMemMip, PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, TexCreateFlags ) )
					{
						FTextureAllocations::FTextureType* TextureType = TextureAllocations.FindTextureType(PreAllocateSizeX, PreAllocateSizeY, PreAllocateNumMips, Texture2D->Format, TexCreateFlags);
						check( TextureType );
						TextureType->ExportIndices.AddItem( ExportIndex );
						NumExportTexturesAdded++;
					}
				}
			}
			check( NumSummaryTextures == NumExportTexturesAdded );
		}
	}

	Ar << TextureAllocations.TextureTypes;

	TextureAllocations.PendingAllocationSize = 0;
	TextureAllocations.PendingAllocationCount.Reset();

	return Ar;
}

/**
 * Kicks off async memory allocations for all textures that will be loaded from this package.
 */
UBOOL ULinkerLoad::StartTextureAllocation()
{
	DOUBLE StartTime = appSeconds();
	INT NumAllocationsStarted = 0;
	INT NumAllocationsConsidered = 0;

	// Only kick off async allocation if the loader is async.
	UBOOL bIsDone = TRUE;
	if ( bUseTimeLimit && !Summary.TextureAllocations.HaveAllAllocationsBeenConsidered() )
	{
		UBOOL bContinue = TRUE;
		for ( INT TypeIndex=Summary.TextureAllocations.NumTextureTypesConsidered;
			  TypeIndex < Summary.TextureAllocations.TextureTypes.Num() && bContinue;
			  ++TypeIndex )
		{
			FTextureAllocations::FTextureType& TextureType = Summary.TextureAllocations.TextureTypes( TypeIndex );
			for ( INT ResourceIndex=TextureType.NumExportIndicesProcessed; ResourceIndex < TextureType.ExportIndices.Num() && bContinue; ++ResourceIndex )
			{
				INT ExportIndex = TextureType.ExportIndices( ResourceIndex );
				if ( WillTextureBeLoaded( UTexture2D::StaticClass(), ExportIndex ) )
				{
					FTexture2DResourceMem* ResourceMem = UTexture2D::CreateResourceMem(
						TextureType.SizeX,
						TextureType.SizeY,
						TextureType.NumMips,
						(EPixelFormat)TextureType.Format,
						TextureType.TexCreateFlags,
						&Summary.TextureAllocations.PendingAllocationCount );
					if ( ResourceMem )
					{
						TextureType.Allocations.AddItem( ResourceMem );
						Summary.TextureAllocations.PendingAllocationSize += ResourceMem->GetResourceBulkDataSize();
						Summary.TextureAllocations.PendingAllocationCount.Increment();
						NumAllocationsStarted++;
					}
				}

				TextureType.NumExportIndicesProcessed++;
				NumAllocationsConsidered++;

				bContinue = !IsTimeLimitExceeded( TEXT("allocating texture memory") );
			}

			// Have we processed all potential allocations for this texture type yet?
			if ( TextureType.HaveAllAllocationsBeenConsidered() )
			{
				Summary.TextureAllocations.NumTextureTypesConsidered++;
			}
		}
		bIsDone = Summary.TextureAllocations.HaveAllAllocationsBeenConsidered();
	}

	DOUBLE Duration = appSeconds() - StartTime;

	// For profiling:
// 	if ( NumAllocationsStarted )
// 	{
// 		debugf( TEXT("StartTextureAllocation duration: %.3f ms (%d textures allocated, %d textures considered)"), Duration*1000.0, NumAllocationsStarted, NumAllocationsConsidered );
// 	}

	return bIsDone && !IsTimeLimitExceeded( TEXT("kicking off texture allocations") );
}

/**
 * Finds a suitable ResourceMem allocation, removes it from this container and return it to the user.
 *
 * @param SizeX				Width of texture
 * @param SizeY				Height of texture
 * @param NumMips			Number of mips
 * @param Format			Texture format (EPixelFormat)
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 **/
FTexture2DResourceMem* FTextureAllocations::FindAndRemove( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags )
{
	FTexture2DResourceMem* ResourceMem = NULL;
	FTextureType* TextureType = FindTextureType( SizeX, SizeY, NumMips, Format, TexCreateFlags );
	if ( TextureType && TextureType->Allocations.Num() > 0 )
	{
		ResourceMem = TextureType->Allocations(0);
		ResourceMem->FinishAsyncAllocation();
		check( ResourceMem->HasAsyncAllocationCompleted() );
		TextureType->Allocations.RemoveSwap( 0 );
		PendingAllocationSize -= ResourceMem->GetResourceBulkDataSize();
	}
	return ResourceMem;
}

/**
 * Finds a texture type that matches the given specifications.
 *
 * @param SizeX				Width of the largest mip-level stored in the package
 * @param SizeY				Height of the largest mip-level stored in the package
 * @param NumMips			Number of mips
 * @param Format			Texture format (EPixelFormat)
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @return					Matching texture type, or NULL if none was found
 */
FTextureAllocations::FTextureType* FTextureAllocations::FindTextureType( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags )
{
	FTexture2DResourceMem* ResourceMem = NULL;
	const DWORD FlagMask = ~(TexCreate_AllowFailure|TexCreate_DisableAutoDefrag);
	for ( INT TypeIndex=0; TypeIndex < TextureTypes.Num(); ++TypeIndex )
	{
		FTextureType& TextureType = TextureTypes( TypeIndex );
		if ( TextureType.SizeX == SizeX &&
			 TextureType.SizeY == SizeY &&
			 TextureType.NumMips == NumMips &&
			 TextureType.Format == Format &&
			 ((TextureType.TexCreateFlags ^ TexCreateFlags) & FlagMask) == 0 )
		{
			return &TextureType;
		}
	}
	return NULL;
}

/**
 * Adds a dummy export index (-1) for a specified texture type.
 * Creates the texture type entry if needed.
 *
 * @param SizeX				Width of the largest mip-level stored in the package
 * @param SizeY				Height of the largest mip-level stored in the package
 * @param NumMips			Number of mips
 * @param Format			Texture format (EPixelFormat)
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 */
void FTextureAllocations::AddResourceMemInfo( INT SizeX, INT SizeY, INT NumMips, DWORD Format, DWORD TexCreateFlags )
{
	FTextureType* TextureType = FindTextureType( SizeX, SizeY, NumMips, Format, TexCreateFlags );
	if ( TextureType == NULL )
	{
		TextureType = new (TextureTypes) FTextureType( SizeX, SizeY, NumMips, Format, TexCreateFlags );
	}
	TextureType->ExportIndices.AddItem(-1);
}

/**
 * Cancels any pending ResourceMem allocation that hasn't been claimed by a texture yet,
 * just in case there are any mismatches at run-time.
 *
 * @param bCancelEverything		If TRUE, cancels all allocations. If FALSE, only cancels allocations that haven't been completed yet.
 */
void FTextureAllocations::CancelRemainingAllocations( UBOOL bCancelEverything )
{
	INT NumRemainingAllocations = 0;
	INT RemainingBulkSize = 0;
	if ( !HasBeenFullyClaimed() )
	{
		for ( INT TypeIndex=0; TypeIndex < TextureTypes.Num(); ++TypeIndex )
		{
			FTextureAllocations::FTextureType& TextureType = TextureTypes( TypeIndex );
			for ( INT ResourceIndex=0; ResourceIndex < TextureType.Allocations.Num(); ++ResourceIndex )
			{
				FTexture2DResourceMem* ResourceMem = TextureType.Allocations( ResourceIndex );
				INT BulkDataSize = ResourceMem->GetResourceBulkDataSize();
				RemainingBulkSize += BulkDataSize;
				NumRemainingAllocations++;
				if ( bCancelEverything || ResourceMem->HasAsyncAllocationCompleted() == FALSE )
				{
					ResourceMem->CancelAsyncAllocation();
					delete ResourceMem;
					TextureType.Allocations.RemoveSwap( ResourceIndex-- );
					PendingAllocationSize -= BulkDataSize;
				}
			}
		}
	}

	check( HasCompleted() );
	check( !bCancelEverything || HasBeenFullyClaimed() );
}

FTextureAllocations::FTextureAllocations( const FTextureAllocations& Other )
:	TextureTypes( Other.TextureTypes )
,	PendingAllocationCount( Other.PendingAllocationCount.GetValue() )
,	PendingAllocationSize( Other.PendingAllocationSize )
,	NumTextureTypesConsidered( Other.NumTextureTypesConsidered )
{
}

void FTextureAllocations::operator=(const FTextureAllocations& Other)
{
	TextureTypes = Other.TextureTypes;
	PendingAllocationCount.Set( Other.PendingAllocationCount.GetValue() );
	PendingAllocationSize = Other.PendingAllocationSize;
	NumTextureTypesConsidered = Other.NumTextureTypesConsidered;
}

/*-----------------------------------------------------------------------------
	UTexture2D
-----------------------------------------------------------------------------*/

/**
 * Calculates and returns the corresponding ResourceMem parameters for this texture.
 *
 * @param FirstMipIdx		Index of the largest mip-level stored within a seekfree (level) package
 * @param OutSizeX			[out] Width of the stored largest mip-level
 * @param OutSizeY			[out] Height of the stored largest mip-level
 * @param OutNumMips		[out] Number of stored mips
 * @param OutTexCreateFlags	[out] ETextureCreateFlags bit flags
 * @return					TRUE if the texture should use a ResourceMem. If FALSE, none of the out parameters will be filled in.
 */
UBOOL UTexture2D::GetResourceMemSettings(INT FirstMipIdx, INT& OutSizeX, INT& OutSizeY, INT& OutNumMips, DWORD& OutTexCreateFlags)
{
	UTextureCube* OuterCubeMap = Cast<UTextureCube>(GetOuter());
	if ( OuterCubeMap || Format == PF_A1 )
	{
		// special case for cubemaps to always load out of main mem
		// 1 bit textures do not create gpu resources
		return FALSE;
	}

	OutSizeX			= Max<INT>(SizeX >> FirstMipIdx, GPixelFormats[Format].BlockSizeX);
	OutSizeY			= Max<INT>(SizeY >> FirstMipIdx, GPixelFormats[Format].BlockSizeY);
	OutNumMips			= Mips.Num() - FirstMipIdx;
	OutTexCreateFlags	= SRGB ? TexCreate_SRGB : 0;
	if ( MipTailBaseIdx == -1 )
	{
		// if no miptail is available then create the texture without a packed miptail
		OutTexCreateFlags |= TexCreate_NoMipTail;
	}
	return TRUE;
}

/**
 * Creates a platform-specific ResourceMem. If an AsyncCounter is provided, it will allocate asynchronously.
 *
 * @param SizeX				Width of the stored largest mip-level
 * @param SizeY				Height of the stored largest mip-level
 * @param NumMips			Number of stored mips
 * @param TexCreateFlags	ETextureCreateFlags bit flags
 * @param AsyncCounter		If specified, starts an async allocation. If NULL, allocates memory immediately.
 * @return					Platform-specific ResourceMem.
 */
FTexture2DResourceMem* UTexture2D::CreateResourceMem(INT SizeX, INT SizeY, INT NumMips, EPixelFormat Format, DWORD TexCreateFlags, FThreadSafeCounter* AsyncCounter)
{
	FTexture2DResourceMem* ResourceMem = NULL;
#if USE_PS3_RHI
	ResourceMem = new FTexture2DResourceMemPS3(SizeX, SizeY, NumMips, (EPixelFormat)Format, AsyncCounter);
#elif USE_XeD3D_RHI
	ResourceMem = new FXeTexture2DResourceMem(SizeX, SizeY, NumMips, (EPixelFormat)Format, TexCreateFlags, AsyncCounter);
#endif
	return ResourceMem;
}

/**
 * Initialize the GPU resource memory that will be used for the bulk mip data
 * This memory is allocated based on the SizeX,SizeY of the texture and the first mip used
 *
 * @param FirstMipIdx first mip that will be resident
 * @return FTexture2DResourceMem container for the allocated GPU resource mem
 */
FTexture2DResourceMem* UTexture2D::InitResourceMem(INT FirstMipIdx)
{
#if USE_PS3_RHI || USE_XeD3D_RHI
	// initialize if one doesn't already exist
	if( !ResourceMem )
	{
		INT FirstMipSizeX, FirstMipSizeY, NumMips;
		DWORD TexCreateFlags;

		// Is this texture supposed to be using pre-allocated ResourceMems?
		if ( GetResourceMemSettings(FirstMipIdx, FirstMipSizeX, FirstMipSizeY, NumMips, TexCreateFlags ) )
		{
			check( FirstResourceMemMip == FirstMipIdx );

			// Is there a corresponding async texture allocation set up for this type of texture?
			ULinker* Linker = GetLinker();
			if ( Linker )
			{
				ResourceMem = Linker->Summary.TextureAllocations.FindAndRemove( FirstMipSizeX, FirstMipSizeY, NumMips, (EPixelFormat)Format, TexCreateFlags );
			}

			// No async allocation?
			if ( ResourceMem == NULL )
			{
				// Do an immediate allocation.
				ResourceMem = CreateResourceMem( FirstMipSizeX, FirstMipSizeY, NumMips, (EPixelFormat)Format, TexCreateFlags, NULL );
			}

			check( ResourceMem );
		}
	}
#endif

	return ResourceMem;
}

/**
* Special serialize function passing the owning UObject along as required by FUnytpedBulkData
* serialization.
*
* @param	Ar		Archive to serialize with
* @param	Owner	UObject this structure is serialized within
* @param	MipIdx	Current mip being serialized
*/
void FTexture2DMipMap::Serialize( FArchive& Ar, UObject* Owner, INT MipIdx )
{
	Data.Serialize( Ar, Owner, MipIdx );
	Ar << SizeX;
	Ar << SizeY;

#if WITH_SUBSTANCE_AIR
	USubstanceAirTexture2D* AirTexture = Cast<USubstanceAirTexture2D>(Owner);

	if (NULL == AirTexture)
	{
		return;
	}

	if (Ar.IsLoading() && GSubstanceAirTextureReader)
	{
		FString TextureName;
		AirTexture->GetFullName().ToLower().Split(TEXT(" "), NULL, &TextureName);

		unsigned int dataSize = Data.GetBulkDataSize();
		char *dataPtr = (char *)Data.GetBulkData();

		if (dataPtr)
		{
			UBOOL ret = GSubstanceAirTextureReader->getMipMap(
				TCHAR_TO_UTF8(*TextureName),
				SizeX,
				SizeY,
				dataPtr,
				dataSize);

#if WITH_SUBSTANCE_AIR_VERBOSE
			warnf(NAME_Log, TEXT("Substance serializing : %s @ %dx%d ==> %s"), 
				*TextureName,
				SizeX,
				SizeY,
				ret ? TEXT("Found") : TEXT("Not Found"));
#endif // WITH_SUBSTANCE_AIR_VERBOSE
		}
	}
#endif // WITH_SUBSTANCE_AIR
}

/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void UTexture2D::InitializeIntrinsicPropertyValues()
{
	SRGB = TRUE;
	UnpackMin[0] = UnpackMin[1] = UnpackMin[2] = UnpackMin[3] = 0.0f;
	UnpackMax[0] = UnpackMax[1] = UnpackMax[2] = UnpackMax[3] = 1.0f;
}

void UTexture2D::LegacySerialize(FArchive& Ar)
{
	Mips.Serialize( Ar, this );
}

void UTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// This is a workaround for Android to support multiple texture formats
	// UTexture2D expects all data in Mips, but to support other compressed formats the data is kept in multiple caches
	// So depending on the run-time detected format support,
	// serialization occurs into Mips in the appropriate location, otherwise a dummy array is serialized into
#if ANDROID
	// Anything in DummyMips is data for the wrong format on Android and will be ignored
	TIndirectArray<FTexture2DMipMap> DummyMips;

	if (Ar.IsLoading())
	{
		if (appGetAndroidTextureFormat() & TEXSUPPORT_DXT)
		{
			// LegacySerialize is just a call to serialize Mips
			LegacySerialize(Ar);
		}
		else
		{
			DummyMips.Serialize(Ar, this);
		}
	}
	else
	{
		LegacySerialize(Ar);
	}
#else
	LegacySerialize(Ar);
#endif

	// Keep track of the fact that we have been loaded from a persistent archive as it's a prerequisite of
	// being streamable.
	if( Ar.IsLoading() && Ar.IsPersistent() )
	{
		bHasBeenLoadedFromPersistentArchive = TRUE;
	}

	if( Ar.Ver() >= VER_ADDED_TEXTURE_FILECACHE_GUIDS )
	{
		Ar << TextureFileCacheGuid;
	}
	else
	{
		GenerateTextureFileCacheGUID(TRUE);
	}

	if( Ar.Ver() < VER_ADDED_TEXTURE_ORIGINAL_SIZE )
	{
		OriginalSizeX = SizeX;
		OriginalSizeY = SizeY;
	}

	if( Ar.Ver() < VER_ADDED_TEXTURE_INTERNALFORMATLODBIAS )
	{
		if( CompressionSettings == TC_NormalmapUncompressed && (OriginalSizeX != SizeX||OriginalSizeY != SizeY) )
		{
			InternalFormatLODBias = 1;
		}
	}

	// serialize the PVRTC data
	if (Ar.Ver() >= VER_ADDED_CACHED_IPHONE_DATA)
	{
		// Support for multiple texture formats, see above comment
#if ANDROID
		if (Ar.IsLoading())
		{
			if (appGetAndroidTextureFormat() & TEXSUPPORT_PVRTC)
			{
				LegacySerialize(Ar);
			}
			else
			{
				DummyMips.Serialize(Ar, this);
			}
		}
		else
		{
			CachedPVRTCMips.Serialize(Ar, this);
		}
#else
		CachedPVRTCMips.Serialize(Ar, this);
#endif
	}

	if (Ar.Ver() >= VER_VERSION_NUMBER_FIX_FOR_FLASH_TEXTURES)
	{
		Ar << CachedFlashMipsMaxResolution;

		// Support for multiple texture formats, see above comment
#if ANDROID
		if (Ar.IsLoading())
		{
			if (appGetAndroidTextureFormat() & TEXSUPPORT_ATITC)
			{
				LegacySerialize(Ar);
			}
			else
			{
				DummyMips.Serialize(Ar, this);
			}
		}
		else
		{
			CachedATITCMips.Serialize(Ar, this);
		}
#else
		CachedATITCMips.Serialize(Ar, this);
#endif

		CachedFlashMips.Serialize(Ar, this, 0);

		// toss outdated flash mips
		if (Ar.Ver() < VER_FLASH_DXT5_TEXTURE_SUPPORT)
		{
			CachedFlashMips.RemoveBulkData();
		}
	}

	if (Ar.Ver() >= VER_ANDROID_ETC_SEPARATED)
	{
		// Support for multiple texture formats, see above comment
#if ANDROID
		if (Ar.IsLoading())
		{
			if (appGetAndroidTextureFormat() & TEXSUPPORT_ETC  )
			{
				LegacySerialize(Ar);
			}
			else
			{
				DummyMips.Serialize(Ar, this);
			}
		}
		else
		{
			CachedETCMips.Serialize(Ar, this);
		}
#else
		CachedETCMips.Serialize(Ar, this);
#endif
	}

#if MOBILE
	// throw away large mips that were cooked into the package, but are not desired, based on SystemSettings
	// this can happen on, say, mobile devices, where we use one cooked package, but it needs to run on 
	// devices of very different memory profiles. The package must contain mips that don't fit on the lower
	// end devices, so we toss them now
	if (GIsGame && Ar.IsLoading())
	{
		// these groups are done in the respective serialize functions, but they haven't run yet,
		// so mimic the behavior here
		if (IsA(ULightMapTexture2D::StaticClass()))
		{
			LODGroup = TEXTUREGROUP_Lightmap;
		}
		else if (IsA(UShadowMapTexture2D::StaticClass()))
		{
			LODGroup = TEXTUREGROUP_Shadowmap;
		}

		INT NormalEmptyStartIdx = 0;
		INT LODBias = GSystemSettings.TextureLODSettings.CalculateLODBias(this);
		if (LODBias != 0)
		{
			if ((LODGroup == TEXTUREGROUP_WorldNormalMap) || (LODGroup == TEXTUREGROUP_CharacterNormalMap) || (LODGroup == TEXTUREGROUP_WeaponNormalMap) || (LODGroup == TEXTUREGROUP_VehicleNormalMap))
			{
				NormalEmptyStartIdx = LODBias;
			}

			LODBias = Min<INT>(LODBias, Mips.Num()-1);
			for (INT EmptyIdx = 0; EmptyIdx < LODBias; EmptyIdx++)
			{
				Mips(EmptyIdx).Data.RemoveBulkData();
				Mips(EmptyIdx).Data.SetBulkDataFlags(BULKDATA_Unused);
			}
		}

#if WITH_MOBILE_RHI && !ANDROID
		//Throw away ALL normal maps but the very lowest Mip for iPad
		// @todo ib2merge: Could this be done before we even load the mips from the TFC, like we do with LODBias? Can't this actually be done with LODBias in the .ini????
		if (GSystemSettings.MobileFeatureLevel == EPF_LowEndFeatures)
		{
			//HACK - assumes ALL normal maps are in these groups
			if ((LODGroup == TEXTUREGROUP_WorldNormalMap) || (LODGroup == TEXTUREGROUP_CharacterNormalMap) || (LODGroup == TEXTUREGROUP_WeaponNormalMap) || (LODGroup == TEXTUREGROUP_VehicleNormalMap))
			{
				for (INT EmptyIdx = NormalEmptyStartIdx; EmptyIdx < Mips.Num()-1; EmptyIdx++)
				{
					Mips(EmptyIdx).Data.RemoveBulkData();
					Mips(EmptyIdx).Data.SetBulkDataFlags(BULKDATA_Unused);
				}
			}
		}
#endif
	}
#endif

// Fix-up Android values for certain formats
#if ANDROID
	if (Ar.IsLoading())
	{	
		if (!(appGetAndroidTextureFormat() & TEXSUPPORT_DXT))
		{
			// make the texture square (the mips were already squarified in ConditionalCachePVRTCTextures)
			if (Format >= PF_DXT1 && Format <= PF_DXT5)
			{
				SizeX = SizeY = Max(SizeX, SizeY);
			}
		}
		else
		{
			// DXT should not have bForcePVRTC4
			bForcePVRTC4 = false;
		}		
	}
#endif
}
void UTexture2D::PostEditUndo()
{
	FPropertyChangedEvent undo(NULL, FALSE, EPropertyChangeType::Undo);
	PostEditChangeProperty(undo);
}

void UTexture2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if( ( !appIsPowerOfTwo(SizeX) || !appIsPowerOfTwo(SizeY) ) && MipGenSettings != TMGS_NoMipmaps )
	{
		// Force NPT textures to have no mipmaps.
		MipGenSettings = TMGS_NoMipmaps;
		NeverStream = TRUE;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GWorld->Scene)
	{
		GWorld->Scene->UpdateImageReflectionTextureArray(this);
	}

	const UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( !( PropertyThatChanged && ( PropertyThatChanged->GetName() == TEXT("LODGroup") || PropertyThatChanged->GetName() == TEXT("LODBias") ) ) )
	{
		// always clear out the cached iphone mips unless we are just changing the lod group or lod bias
		CachedPVRTCMips.Empty();
		CachedATITCMips.Empty();
		CachedETCMips.Empty();
	}

	// always clear flash
	CachedFlashMips.RemoveBulkData();
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UTexture2D::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	if( Format == PF_A1 )
	{
		// No mips needed for 1 bit textures.  System memory will be used.	
		Mips.Empty();
	}

	// Servers need no textures, everyone else does, so toss
	// the data if the only platform we are keeping is WindowsServer
	if(!(PlatformsToKeep & ~UE3::PLATFORM_WindowsServer))
	{
		Mips.Empty();
	}

	// toss cached texture data if we aren't keeping a platform that needs it
	if (!(PlatformsToKeep & (UE3::PLATFORM_IPhone | UE3::PLATFORM_NGP | UE3::PLATFORM_Android)))
	{
		CachedPVRTCMips.Empty();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_Android))
	{
		CachedATITCMips.Empty();
		CachedETCMips.Empty();
	}
	if (!(PlatformsToKeep & UE3::PLATFORM_Flash))
	{
		CachedFlashMips.RemoveBulkData();
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 *	Gets the average brightness of the texture
 *
 *	@param	bIgnoreTrueBlack		If TRUE, then pixels w/ 0,0,0 rgb values do not contribute.
 *	@param	bUseGrayscale			If TRUE, use gray scale else use the max color component.
 *
 *	@return	FLOAT					The average brightness of the texture
 */
FLOAT UTexture2D::GetAverageBrightness(UBOOL bIgnoreTrueBlack, UBOOL bUseGrayscale)
{
	FLOAT AvgBrightness = -1.0f;
#if !CONSOLE
	TArray<BYTE> RawData;

	// use the source art if it exists
	if ( HasSourceArt() )
	{
		// Decompress source art.
		GetUncompressedSourceArt(RawData);
	}
	else
	{
		debugf(TEXT("No SourceArt available for %s"), *GetPathName());
	}

	if (RawData.Num() > 0)
	{
		DOUBLE PixelSum = 0.0f;
		INT Divisor = SizeX * SizeY;
		FColor* ColorData = (FColor*)(&RawData(0));
		FLinearColor CurrentColor;
		for (INT Y = 0; Y < SizeY; Y++)
		{
			for (INT X = 0; X < SizeX; X++)
			{
				if ((ColorData->R == 0) && (ColorData->G == 0) && (ColorData->B == 0) && bIgnoreTrueBlack)
				{
					ColorData++;
					Divisor--;
					continue;
				}

				if (SRGB == TRUE)
				{
					CurrentColor = FLinearColor(*ColorData);
				}
				else
				{
					CurrentColor.R = FLOAT(ColorData->R) / 255.0f;
					CurrentColor.G = FLOAT(ColorData->G) / 255.0f;
					CurrentColor.B = FLOAT(ColorData->B) / 255.0f;
				}

				if (bUseGrayscale == TRUE)
				{
					PixelSum += CurrentColor.R * 0.30f + CurrentColor.G * 0.59f + CurrentColor.B * 0.11f;
				}
				else
				{
					PixelSum += Max<FLOAT>(CurrentColor.R, Max<FLOAT>(CurrentColor.G, CurrentColor.B));
				}

				ColorData++;
			}
		}
		if (Divisor > 0)
		{
			AvgBrightness = PixelSum / Divisor;
		}
	}
#endif	//#if !CONSOLE
	return AvgBrightness;
}

/**
 * Returns a reference to the global list of streamable textures.
 *
 * @return reference to global list of streamable textures.
 */
TLinkedList<UTexture2D*>*& UTexture2D::GetStreamableList()
{
	return FirstStreamableLink;
}

/**
 * Returns a reference to the current streamable link.
 *
 * @return reference to current streamable link
 */
TLinkedList<UTexture2D*>*& UTexture2D::GetCurrentStreamableLink()
{
	// Use first if current link wasn't set yet or has been reset.
	if( !CurrentStreamableLink )
	{
		CurrentStreamableLink = FirstStreamableLink;
	}
	return CurrentStreamableLink;
}

/**
 * Links texture to streamable list and updates streamable texture count.
 */
void UTexture2D::LinkStreaming()
{
	StreamableTexturesLink = TLinkedList<UTexture2D*>(this);
	StreamableTexturesLink.Link( GetStreamableList() );
	NumStreamableTextures++;

	if ( !GIsUCCMake && IsTemplate() == FALSE )
	{
		GStreamingManager->AddStreamingTexture(this);
	}
}

/**
 * Unlinks texture from streamable list, resets CurrentStreamableLink if it matches
 * StreamableTexturesLink and also updates the streamable texture count.
 */
void UTexture2D::UnlinkStreaming()
{
	if ( !GIsUCCMake && IsTemplate() == FALSE )
	{
		GStreamingManager->RemoveStreamingTexture(this);
	}

	// Reset current streamable link if it equals current texture.
	if( &StreamableTexturesLink == CurrentStreamableLink )
	{
		CurrentStreamableLink = NULL;
	}

	// Only decrease count if texture was linked.
	if( StreamableTexturesLink.IsLinked() )
	{
		NumStreamableTextures--;
	}

	// Unlink from list.
	StreamableTexturesLink.Unlink();
}
	
/**
 * Returns the number of streamable textures, maintained by link/ unlink code
 *
 * @return	Number of streamable textures
 */
INT UTexture2D::GetNumStreamableTextures()
{
	return NumStreamableTextures;
}

/**
 * Cancels any pending texture streaming actions if possible.
 * Returns when no more async loading requests are in flight.
 */
void UTexture2D::CancelPendingTextureStreaming()
{
	for( TObjectIterator<UTexture2D> It; It; ++It )
	{
		UTexture2D* CurrentTexture = *It;
		CurrentTexture->CancelPendingMipChangeRequest();
	}

	FlushResourceStreaming();
}

/**
 * Called after object and all its dependencies have been serialized.
 */
void UTexture2D::PostLoad()
{
#if XBOX
#if !FINAL_RELEASE
	if( Format != PF_A1 )
	{
		// make sure we have at least enough bulk mip data available
		// to load the miptail
		INT FirstAvailableMip=INDEX_NONE;
		for( INT MipIndex=Mips.Num()-1; MipIndex >= 0; MipIndex-- )
		{
			const FTexture2DMipMap& Mip = Mips(MipIndex);
			if( Mip.Data.IsAvailableForUse() )
			{
				FirstAvailableMip = MipIndex;
				break;
			}
		}
		checkf( FirstAvailableMip != INDEX_NONE, TEXT("No mips available: %s"), *GetFullName() );
		checkf( FirstAvailableMip >= MipTailBaseIdx || MipTailBaseIdx >= Mips.Num(), TEXT("Not enough mips (%d:%d) available: %s"), FirstAvailableMip, MipTailBaseIdx, *GetFullName() );
	}	
#endif
#else
	if( !HasAnyFlags(RF_Cooked) )
	{
		// no packed mips on other platforms
		MipTailBaseIdx = Max(0,Mips.Num()-1);
	}
#endif

	// Route postload, which will update bIsStreamable as UTexture::PostLoad calls UpdateResource.
	Super::PostLoad();
}

/**
* Called after object has been duplicated.
*/
void UTexture2D::PostDuplicate()
{
	Super::PostDuplicate();

	// update GUID for new texture 
	GenerateTextureFileCacheGUID(TRUE);
}

/** 
* Generates a GUID for the texture if one doesn't already exist. 
*
* @param bForceGeneration	Whether we should generate a GUID even if it is already valid.
*/
void UTexture2D::GenerateTextureFileCacheGUID(UBOOL bForceGeneration)
{
#if CONSOLE
	TextureFileCacheGuid.Invalidate();
#else
	if( (GIsEditor && !GIsGame) || GIsUCC )
	{
		if( bForceGeneration || !TextureFileCacheGuid.IsValid() )
		{
			TextureFileCacheGuid = appCreateGuid();
		}
	}
#endif
}

/**
 * Creates a new resource for the texture, and updates any cached references to the resource.
 */
void UTexture2D::UpdateResource()
{
	// 1 bit textures are for physical material masks only
	// and should not ever be stored on the GPU.

	if( Format != PF_A1 )
	{
		// Make sure there are no pending requests in flight.
		while( UpdateStreamingStatus() == TRUE )
		{
			// Give up timeslice.
			appSleep(0);
		}

		// Route to super.
		Super::UpdateResource();
	}

}

#if !CONSOLE
/**
 * Changes the linker and linker index to the passed in one. A linker of NULL and linker index of INDEX_NONE
 * indicates that the object is without a linker.
 *
 * @param LinkerLoad	New LinkerLoad object to set
 * @param LinkerIndex	New LinkerIndex to set
 */
void UTexture2D::SetLinker( ULinkerLoad* LinkerLoad, INT LinkerIndex )
{
	// We never change linkers in the case of seekfree loading though will reset them/ set them to NULL
	// and don't want to load the texture data in this case.
	if( GUseSeekFreeLoading )
	{
		// Route the call to change the linker.
		Super::SetLinker( LinkerLoad, LinkerIndex );
	}
	else
	{
		// Only update resource if linker changes.
		UBOOL bRequiresUpdate = FALSE;
		if( LinkerLoad != GetLinker() )
		{
			bRequiresUpdate = TRUE;
		}

		// Route the call to change the linker.
		Super::SetLinker( LinkerLoad, LinkerIndex );

		// Changing the linker requires re-creating the resource to make sure streaming behavior is right.
		if( bRequiresUpdate && !HasAnyFlags( RF_Unreachable | RF_BeginDestroyed | RF_NeedLoad | RF_NeedPostLoad ) )
		{
			// Reset to FALSE as Serialize is being called after SetLinker in the case of it being reloaded from disk
			// and we want this to be FALSE if we are not going to serialize it again.
			bHasBeenLoadedFromPersistentArchive = FALSE;

			// Update the resource.
			UpdateResource();

			// Unlink texture...
			UnlinkStreaming();

			// Can't be streamable as we just changed the linker outside of regular load.
			check( !bIsStreamable );
		}
	}
}
#endif

/**
 * Called after the garbage collection mark phase on unreachable objects.
 */
void UTexture2D::BeginDestroy()
{
	// Route BeginDestroy.
	Super::BeginDestroy();

	// Cancel any in flight IO requests
	CancelPendingMipChangeRequest();

	// Safely unlink texture from list of streamable ones.
	UnlinkStreaming();

	TrackTextureEvent( NULL, this, TRUE, TRUE, FALSE );
}

//@warning:	Do NOT call Compress from within Init as it could cause an infinite recursion.
void UTexture2D::Init(UINT InSizeX,UINT InSizeY,EPixelFormat InFormat)
{
	// Check that the dimensions are powers of two and evenly divisible by the format block size.
//	check(!(InSizeX & (InSizeX - 1)));
//	check(!(InSizeY & (InSizeY - 1)));
	check(!(InSizeX % GPixelFormats[InFormat].BlockSizeX));
	check(!(InSizeY % GPixelFormats[InFormat].BlockSizeY));

	// We need to flush all rendering commands before we can modify a textures' mip array, size or format.
	if( Mips.Num() )
	{
		// Flush rendering commands.
		FlushRenderingCommands();
		// Start with a clean plate.
		Mips.Empty();
	}

	SizeX = InSizeX;
	SizeY = InSizeY;
	OriginalSizeX = InSizeX;
	OriginalSizeY = InSizeY;
	Format = InFormat;

	// System memory data should be empty when initializing the texture
	SystemMemoryData.Empty();

	// Allocate first mipmap.
	FTexture2DMipMap* MipMap = new(Mips) FTexture2DMipMap;

	MipMap->SizeX = SizeX;
	MipMap->SizeY = SizeY;

	SIZE_T ImageSize = CalculateImageBytes(SizeX,SizeY,0,(EPixelFormat)Format);

	MipMap->Data.Lock( LOCK_READ_WRITE );
	MipMap->Data.Realloc( ImageSize );
	MipMap->Data.Unlock();
}

/** 
 * Returns a one line description of an object for viewing in the generic browser
 */
FString UTexture2D::GetDesc()
{
	UINT EffectiveSizeX;
	UINT EffectiveSizeY;

	// platform dependent
	UINT LODBiasRegular = UINT( GSystemSettings.TextureLODSettings.CalculateLODBias(this) );

	GSystemSettings.TextureLODSettings.ComputeInGameMaxResolution(LODBiasRegular, *this, EffectiveSizeX, EffectiveSizeY);

	return FString::Printf( TEXT("%s %dx%d -> %dx%d[%s%s]"), 
		NeverStream ? TEXT("NeverStreamed") : TEXT("Streamed"), 
		SizeX, 
		SizeY, 
		EffectiveSizeX, 
		EffectiveSizeY,
		GPixelFormats[Format].Name, 
		DeferCompression ? TEXT("*") : TEXT(""));
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UTexture2D::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeY );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		if( DeferCompression )
		{
			Description += TEXT( "*" );
		}
		break;
	case 2:
		{
			TArray<FString> TextureGroupNames = FTextureLODSettings::GetTextureGroupNames();
			if( LODGroup < TextureGroupNames.Num() )
			{
				Description = TextureGroupNames(LODGroup);
			}
		}
		break;
	case 3:
		Description = NeverStream ? TEXT( "NeverStreamed" ) : TEXT( "Streamed" );
		break;
	default:
		break;
	}
	return( Description );
}

/**
 * Returns whether the texture is ready for streaming aka whether it has had InitRHI called on it.
 *
 * @return TRUE if initialized and ready for streaming, FALSE otherwise
 */
UBOOL UTexture2D::IsReadyForStreaming()
{
	// A value < 0 indicates that the resource is still in the process of being created 
	// for the first time.
	INT RequestStatus = PendingMipChangeRequestStatus.GetValue();
	return RequestStatus != TexState_InProgress_Initialization;
}

/**
 * Waits until all streaming requests for this texture has been fully processed.
 */
void UTexture2D::WaitForStreaming()
{
	GStreamingManager->UpdateIndividualResource( this );

	// Make sure there are no pending requests in flight.
	while( UpdateStreamingStatus() == TRUE )
	{
		// Give up timeslice.
		appSleep(0);
	}
}

/**
 * Updates the streaming status of the texture and performs finalization when appropriate. The function returns
 * TRUE while there are pending requests in flight and updating needs to continue.
 *
 * @param bWaitForMipFading	Whether to wait for Mip Fading to complete before finalizing.
 * @return					TRUE if there are requests in flight, FALSE otherwise
 */
UBOOL UTexture2D::UpdateStreamingStatus( UBOOL bWaitForMipFading /*= FALSE*/ )
{
	UBOOL	bHasPendingRequestInFlight	= TRUE;
	INT		RequestStatus				= PendingMipChangeRequestStatus.GetValue();

	// if resident and requested mip counts match then no pending request is in flight
	if ( ResidentMips == RequestedMips)
	{
		checkf( RequestStatus == TexState_ReadyFor_Requests || RequestStatus == TexState_InProgress_Initialization, TEXT("RequestStatus=%d"), RequestStatus );
		check( !bHasCancelationPending );
		bHasPendingRequestInFlight = FALSE;
	}
	// Pending request in flight, though we might be able to finish it.
	else
	{
		FTexture2DResource* Texture2DResource = (FTexture2DResource*) Resource;

		// If memory has been allocated, go ahead to the next step and load in the mips.
		if ( RequestStatus == TexState_ReadyFor_Loading )
		{
			Texture2DResource->BeginLoadMipData();
		}
		// Update part of mip change request is done, time to kick off finalization.
		else if( RequestStatus == TexState_ReadyFor_Finalization )
		{
			// Don't finalize if we're currently fading out the mips slowly.
			UBOOL bFinalizeNow;
			EMipFadeSettings MipFadeSetting = (LODGroup == TEXTUREGROUP_Lightmap || LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;
			if ( bWaitForMipFading && RequestedMips < ResidentMips && MipFadeSetting == MipFade_Slow && Texture2DResource->MipBiasFade.IsFading() )
			{
				bFinalizeNow = FALSE;
			}
			else
			{
				bFinalizeNow = TRUE;
			}

			if ( bFinalizeNow || GIsRequestingExit || bHasCancelationPending )
			{
#if STATS
				// Are we measuring streaming latency? (Contains negative timestamp based off GStartTime.)
				if ( Timer < 0.0f )
				{
					Timer = FLOAT(appSeconds() - GStartTime) + Timer;
				}
#endif
				// Finalize mip request, aka unlock textures involved, perform switcheroo and free original one.
				Texture2DResource->BeginFinalizeMipCount();
			}
		}
		// Finalization finished. We're done.
		else if( RequestStatus == TexState_ReadyFor_Requests )
		{
			// We have a cancellation request pending which means we did not change anything.
			FTexture2DResource* Texture2DResource = (FTexture2DResource*) Resource;
			if( bHasCancelationPending || (Texture2DResource && Texture2DResource->DidUpdateMipCountFail()) )
			{
				// Reset requested mip count to resident one as we no longer have a request outstanding.
				RequestedMips = ResidentMips;
				// We're done canceling the request.
				bHasCancelationPending = FALSE;
			}
			// Resident mips now match requested ones.
			else
			{
				ResidentMips = RequestedMips;
			}
			bHasPendingRequestInFlight = FALSE;
		}
	}
	return bHasPendingRequestInFlight;
}

/**
 * Tries to cancel a pending mip change request. Requests cannot be canceled if they are in the
 * finalization phase.
 *
 * @param	TRUE if cancelation was successful, FALSE otherwise
 */
UBOOL UTexture2D::CancelPendingMipChangeRequest()
{
	INT RequestStatus = PendingMipChangeRequestStatus.GetValue();
	// Nothing to do if we're already in the process of canceling the request.
	if( !bHasCancelationPending )
	{
		// We can only cancel textures that either have a pending request in flight or are pending finalization.
		if( RequestStatus >= TexState_ReadyFor_Finalization )
		{
			check(Resource);
			FTexture2DResource* Texture2DResource = (FTexture2DResource*) Resource;

			// Did we shrink in-place?
			if ( Texture2DResource->IsBeingReallocated() && RequestedMips < ResidentMips )
			{
				// Can't cancel. The old mips are gone from memory.
				bHasCancelationPending = FALSE;
			}
			else
			{
				// We now have a cancellation pending!
				bHasCancelationPending = TRUE;

				// Begin async cancellation of current request.
				Texture2DResource->BeginCancelUpdate();
			}
		}
		// Texture is either pending finalization or doesn't have a request in flight.
		else
		{
		}
	}
	return bHasCancelationPending;
}

/**
 * Calculates the size of this texture in bytes if it had MipCount miplevels streamed in.
 *
 * @param	MipCount	Number of mips to calculate size for, counting from the smallest 1x1 mip-level and up.
 * @return	Size of MipCount mips in bytes
 */
INT UTexture2D::CalcTextureMemorySize( INT MipCount ) const
{
	INT Size		= 0;
	// Figure out what the first mip to use is.
	INT FirstMip	= Max( 0, Mips.Num() - MipCount );
	// Iterate over all relevant miplevels and sum up their size.
	for( INT MipIndex=FirstMip; MipIndex<Mips.Num(); MipIndex++ )
	{
		const FTexture2DMipMap& MipMap = Mips(MipIndex);

#if FLASH
		const UINT BPP = (Format == PF_DXT1) ? 4 : 32;
		const UINT MipSize = (MipMap.SizeX * MipMap.SizeY * BPP) / 8;
		Size += MipSize;
#else // #if FLASH
		// The bulk data size matches up with the size in video memory and in the case of consoles even takes
		// alignment restrictions into account as the data is expected to be a 1:1 copy including size.
		Size += MipMap.Data.GetBulkDataSize();
#endif // #if FLASH
	}
	return Size;
}

/**
 * Calculates the size of this texture if it had MipCount miplevels streamed in.
 *
 * @param	MipCount	Which mips to calculate size for.
 * @return	Total size of all specified mips, in bytes
 */
INT UTexture2D::CalcTextureMemorySize( ETextureMipCount MipCount ) const
{
	if ( MipCount == TMC_ResidentMips )
	{
		return CalcTextureMemorySize( ResidentMips );
	}
	else if( MipCount == TMC_AllMipsBiased )
	{
		return CalcTextureMemorySize( Mips.Num() - LODBias );
	}
	else
	{
		return CalcTextureMemorySize( Mips.Num() );
	}
}

/**
 * Returns the size of this texture in bytes on 360 if it had MipCount miplevels streamed in.
 *
 * @param	MipCount	Number of toplevel mips to calculate size for
 * @return	size of top mipcount mips in bytes
 */
INT UTexture2D::Get360Size( INT MipCount ) const
{
#if _WINDOWS && !CONSOLE
	INT Size = 0;
	// Create the RHI texture.
	DWORD TexCreateFlags = SRGB ? TexCreate_SRGB : 0;
	// if no miptail is available then create the texture without a packed miptail
	if( MipTailBaseIdx == -1 )
	{
		TexCreateFlags |= TexCreate_NoMipTail;
	}

	FConsoleSupport* ConsoleSupport = FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(CONSOLESUPPORT_NAME_360);
	if (ConsoleSupport)
	{
		FConsoleTextureCooker* TextureCooker = ConsoleSupport->GetGlobalTextureCooker();
		if (TextureCooker)
		{
			// Figure out what the first mip to use is.
			INT FirstMip = Max( 0, Mips.Num() - MipCount );
			if (FirstMip < Mips.Num())
			{
				const FTexture2DMipMap& MipMap = Mips(FirstMip);
				Size = TextureCooker->GetPlatformTextureSize(Format, MipMap.SizeX, MipMap.SizeY, MipCount, TexCreateFlags);
				UINT UnusedMipTailSize = XeCalcUnusedMipTailSize(SizeX, SizeY, EPixelFormat(Format), MipCount, (TexCreateFlags & TexCreate_NoMipTail) ? FALSE : TRUE );
				Size -= UnusedMipTailSize;
			}
		}
	}
	return Size;
#else
	return 0;
#endif
}

/**
 *	Get the CRC of the source art pixels.
 *
 *	@param	[out]	OutSourceCRC		The CRC value of the source art pixels.
 *
 *	@return			UBOOL				TRUE if successful, FALSE if failed (or no source art)
 */
UBOOL UTexture2D::GetSourceArtCRC(DWORD& OutSourceCRC)
{
	UBOOL bResult = FALSE;
#if !CONSOLE
	TArray<BYTE> RawData;

	// use the source art if it exists
	if ( HasSourceArt() )
	{
		// Decompress source art.
		GetUncompressedSourceArt(RawData);
	}
	else
	{
		debugf(TEXT("No SourceArt available for %s"), *GetPathName());
	}

	if (RawData.Num() > 0)
	{
		OutSourceCRC = appMemCrc((void*)(RawData.GetData()), RawData.Num());
		bResult = TRUE;
	}
#endif	//#if !CONSOLE
	return bResult;
}

/**
 * Determines if the given format is uncompressed and thus doesn't need to store source art.
 *
 * @param	Format	The format of the texture to test for support.
 * @return	UBOOL	TRUE if the format already stores uncompressed source art; FALSE, otherwise.
 */
static UBOOL IsSupportedForUncompressedSourceArt( const EPixelFormat Format )
{
	// Currently, only PF_A8R8G8B8 is supported as an uncompressed format
	// that doesn't store extra source art data. 
	return (Format == PF_A8R8G8B8);
}

/**
 * Returns whether or not the texture has source art at all
 *
 * @return	TRUE if the texture has source art. FALSE, otherwise.
 */
UBOOL UTexture2D::HasSourceArt() const
{ 
	UBOOL bHasSourceArt = FALSE;
	const UBOOL bCanAccessRawDataDirectly = ( IsSupportedForUncompressedSourceArt((EPixelFormat)Format) && (Mips.Num() > 0) );

	if( (SourceArt.GetBulkDataSize() != 0) || bCanAccessRawDataDirectly )
	{
		bHasSourceArt = TRUE;
	}

	return bHasSourceArt; 
}

/**
 * Compresses the source art, if needed
 */
void UTexture2D::CompressSourceArt()
{
#if !CONSOLE && !PLATFORM_MACOSX
	if( HasSourceArt() && bIsSourceArtUncompressed )
	{
		TArray<BYTE> UncompressedData;
		GetUncompressedSourceArt(UncompressedData);
		check( UncompressedData.Num() );

		// PNG Compress by compressing the highest mip.
		FPNGHelper PNG;
		PNG.InitRaw( UncompressedData.GetData(), UncompressedData.Num(), OriginalSizeX, OriginalSizeY );
		const TArray<BYTE>& CompressedData = PNG.GetCompressedData();
		check( CompressedData.Num() );

		// We have compressed source art, now store it.
		SetCompressedSourceArt(CompressedData.GetData(), CompressedData.Num());
	}
#endif
}

/**
 * Returns uncompressed source art.
 *
 * @param	OutSourceArt	[out]A buffer containing uncompressed source art.
 */
void UTexture2D::GetUncompressedSourceArt( TArray<BYTE>& OutSourceArt )
{
	// The texture MUST have source art to get uncompressed source art.
	// Before trying to get uncompressed source art, you should call
	// HasSourceArt() first. 
	check( HasSourceArt() );

#if !CONSOLE && !PLATFORM_MACOSX
	OutSourceArt.Empty();

	// Check if we have existing source art for compressed textures.
	if( SourceArt.GetBulkDataSize() != 0 )
	{
		// If we have compressed source art, we have to decompress it. 
		if( !bIsSourceArtUncompressed )
		{
			// Decompress source art.
			FPNGHelper PNG;
			PNG.InitCompressed(SourceArt.Lock(LOCK_READ_WRITE), SourceArt.GetBulkDataSize(), OriginalSizeX, OriginalSizeY);
			try
			{
				OutSourceArt = PNG.GetRawData();
			}
			catch (...)
			{
				warnf(NAME_Warning, TEXT("---- FAILED PNG DECOMPRESSION: %s"), *GetPathName());
			}
			SourceArt.Unlock();

			// Check to see if source art was saved at the incorrect resolution
			if( PNG.GetPNGHeaderWidth() != OriginalSizeX || PNG.GetPNGHeaderHeight() != OriginalSizeY )
			{
				FString ErrorMsg = FString::Printf(TEXT("The saved source art for %s was corrupted when originally imported. Please reimport this texture from source."), *GetPathName() );
				if ( GIsEditor && !GIsUCC )
				{
					appMsgf(AMT_OK, *ErrorMsg);
				}
				else
				{
					warnf(NAME_Error, *ErrorMsg);
				}
			}
		}
		// Otherwise, we don't have to decompress source art if it's already uncompressed.
		else
		{
			const INT TextureSize = OriginalSizeX * OriginalSizeY * 4;
			OutSourceArt.Add(TextureSize);

			// Provide the raw source art data since it's already uncompressed.
			const void* TexData = SourceArt.Lock(LOCK_READ_ONLY);
			appMemcpy(OutSourceArt.GetData(), TexData, TextureSize);
			SourceArt.Unlock();
		}
	}
	// other wise use the plain raw data if it's in the right format
	else
	{
		// In order to access the raw data directly, the format
		// must be supported internally. 
		check( IsSupportedForUncompressedSourceArt((EPixelFormat)Format) );

		const INT TextureSize = OriginalSizeX * OriginalSizeY * 4;
		OutSourceArt.Add(TextureSize);

		// get highest level mip
		void* TexData = Mips(0).Data.Lock(LOCK_READ_ONLY);
		appMemcpy(OutSourceArt.GetData(), TexData, TextureSize);
		Mips(0).Data.Unlock();
	}
#endif
}

/**
 * Sets the given buffer as the uncompressed source art.
 *
 * @param	UncompressedData	Uncompressed source art data. 
 * @param	DataSize			Size of the UncompressedData.
 */
void UTexture2D::SetUncompressedSourceArt( const void* UncompressedData, INT DataSize )
{
	check( UncompressedData != NULL );

	SourceArt.Lock(LOCK_READ_WRITE);
	void* SourceArtPointer = SourceArt.Realloc( DataSize );
	appMemcpy( SourceArtPointer, UncompressedData, DataSize );
	SourceArt.Unlock();

	bIsSourceArtUncompressed = TRUE;
}

/**
 * Sets the given buffer as the compressed source art.
 *
 * @param	CompressedData		Compressed source art data. 
 * @param	DataSize			Size of the CompressedData.
 *
 * @return	TRUE if the compressed source art was set; FALSE, otherwise.
 */
void UTexture2D::SetCompressedSourceArt( const void* CompressedData, INT DataSize )
{
	check( CompressedData != NULL );

	SourceArt.Lock(LOCK_READ_WRITE);
	void* SourceArtPointer = SourceArt.Realloc( DataSize );
	appMemcpy( SourceArtPointer, CompressedData, DataSize );
	SourceArt.Unlock();

	bIsSourceArtUncompressed = FALSE;
}

/**
 *	See if the source art of the two textures matches...
 *
 *	@param		InTexture		The texture to compare it to
 *
 *	@return		UBOOL			TRUE if they matche, FALSE if not
 */
UBOOL UTexture2D::HasSameSourceArt(UTexture2D* InTexture)
{
	UBOOL bResult = FALSE;
#if !CONSOLE
	TArray<BYTE> RawData1;
	TArray<BYTE> RawData2;
	INT SizeX = 0;
	INT SizeY = 0;

	if ((OriginalSizeX == InTexture->OriginalSizeX) && 
		(OriginalSizeY == InTexture->OriginalSizeY) &&
		(SRGB == InTexture->SRGB))
	{
		// use the source art if it exists
		if ( HasSourceArt() )
		{
			if ( InTexture->HasSourceArt() )
			{
				// Decompress source art.
				GetUncompressedSourceArt(RawData1);

				if (RawData1.Num() > 0)
				{
					InTexture->GetUncompressedSourceArt(RawData2);
				}
			}
			else
			{
				debugf(TEXT("No SourceArt available for %s"), *(InTexture->GetPathName()));
			}
		}
		else
		{
			debugf(TEXT("No SourceArt available for %s"), *GetPathName());
		}
	}

	if ((RawData1.Num() > 0) && (RawData1.Num() == RawData2.Num()))
	{
		if (RawData1 == RawData2)
		{
			bResult = TRUE;
		}
	}
#endif	//#if !CONSOLE
	return bResult;
}

/**
 * Returns if the texture should be automatically biased to -1..1 range
 */
UBOOL UTexture2D::BiasNormalMap() const
{
#if PS3
	return CompressionSettings == TC_Normalmap || CompressionSettings == TC_NormalmapUncompressed;
#else
	return FALSE;
#endif
}


/**
 * return the texture/pixel format that should be used internally for an incoming texture load request, if different onload conversion is required 
 */
EPixelFormat UTexture2D::GetEffectivePixelFormat( const EPixelFormat Format, UBOOL bSRGB, UE3::EPlatformType Platform )
{ 
	if(Platform == UE3::PLATFORM_Unknown)
	{
		Platform = appGetPlatformType();
	}

	// some PC GPUs don't support sRGB read (e.g. AMD DX10 cards on ShaderModel3.0)
	// This solution requires 4x more memory but a lot of PC HW emulate the format anyway
	if( (Platform & UE3::PLATFORM_PC) !=0
	&&	(Format == PF_G8)
	&&	bSRGB)
	{
		return PF_A8R8G8B8;
	}

	return Format;
}


FTextureResource* UTexture2D::CreateResource()
{
	FString Filename	= TEXT("");	
	// This might be a new texture that has never been loaded, a texture which just had it's linker detached
	// as it was renamed or saved or a texture that was re-imported and therefore has a linker but has never
	// been loaded from disk.
	bIsStreamable		= FALSE;

	//Because texture can be referenced by the start up package, GEngine can be NULL and bIsStreamable will be turned on by mistake.
	//This forces it off without exception of what package it is in
	if ( bIsCompositingSource )
	{
		NeverStream = TRUE;
	}

	// We can only stream textures that have been loaded from "disk" (aka a persistent archive).
	if( bHasBeenLoadedFromPersistentArchive 
#if MOBILE
	// Some textures aren't cooked into the TFC in the cooker, so those are handled later
	// with a Mip.Data.IsStoredInSeparateFile() check
	&& TextureFileCacheName != NAME_None
#else
	// Disregard textures that are marked as not being streamable.
	&&	!NeverStream
	// Nothing to stream if we don't have multiple mips.
	&&  Mips.Num() > 1
	// We don't stream UI textures. On cooked builds they will have mips stripped out.
	&&	LODGroup != TEXTUREGROUP_UI 
#endif
	)
	{
		// Use the texture file cache name if it is valid.
		if( TextureFileCacheName != NAME_None )
		{
			bIsStreamable	= TRUE;

			// cache a string version
#if ANDROID
			FString TextureCacheString = TextureFileCacheName.ToString() + appGetAndroidTextureFormatName() + TEXT(".") + GSys->TextureFileCacheExtension;
#else
			FString TextureCacheString = TextureFileCacheName.ToString() + TEXT(".") + GSys->TextureFileCacheExtension;
#endif
			
#if PS3
			if (GDownloadableContent == NULL || !GDownloadableContent->GetDLCTextureCachePath(TextureFileCacheName,Filename))
#else
			UDownloadableContentManager* DlcManager = UGameEngine::GetDLCManager();
			if (DlcManager == NULL || !DlcManager->GetDLCNonPackageFilePath(TextureFileCacheName,Filename))
#endif
			{
				// get cooked directory
				FString CookedPath;
				appGetCookedContentPath(appGetPlatformType(), CookedPath);

				// append the TFC filename
				Filename		= CookedPath + TextureCacheString;

				// On Android the user may have deployed incorrect texture format data to the device
#if ANDROID
				if(GFileManager->FileSize(*Filename) == INDEX_NONE)
				{
					debugf( NAME_Error, TEXT("%s does not exist! Check to make sure a supported texture format has been synced for this device"), *TextureCacheString );
				}
#endif
			}
		}
		// Use the linker's filename if it exists. We can't do this if we're using seekfree loading as the linker might potentially
		// be the seekfree package's linker and hence be the wrong filename.
		else if( GetLinker() && !(GetLinker()->LinkerRoot->PackageFlags & PKG_Cooked))
		{
			bIsStreamable	= TRUE;
			Filename		= GetLinker()->Filename;
		}
		// Look up the filename in the package file cache. We cannot do this in the Editor as the texture might have
		// been newly created and therefore never has been saved. Yet, if it was created in an already existing package
		// FindPackageFile would return TRUE! There is also the edge case of creating a texture in a package that 
		// hasn't been saved in which case FindPackageFile would return FALSE.
		else if( !GIsEditor && GPackageFileCache->FindPackageFile( *GetOutermost()->GetName(), NULL, Filename, NULL ) )
		{
			// Found package file. A case for a streamable texture without a linker attached are objects that were
			// forced into the exports table.
			bIsStreamable	= TRUE;
		}
		// Package file not found.
		else
		{
			// now, check by Guid, in case the package was downloaded (we don't use the Guid above, because when 
			// checking by Guid it must serialize the header from the package, which can be slow)
			FGuid PackageGuid = GetOutermost()->GetGuid();
			if( !GIsEditor && GPackageFileCache->FindPackageFile( *GetOutermost()->GetName(), &PackageGuid, Filename, NULL ) )
			{
				bIsStreamable = TRUE;
			}
			else
			{
				// This should only ever happen in the Editor as the game shouldn't be creating texture at run-time.
				checkf(GIsEditor, TEXT("Cannot create textures at run-time in the game [unable to find %s]"), *GetOutermost()->GetName());
			}
		}
	}

	// Only allow streaming if enabled on the engine level.
	bIsStreamable = bIsStreamable && GUseTextureStreaming;

	if(GetEffectivePixelFormat((EPixelFormat)Format, SRGB) != Format)
	{
		// we don't want to stream texture that require on load conversion
		bIsStreamable = FALSE;
	}

	// number of levels in the packed miptail
	INT NumMipTailLevels = Max(0,Mips.Num() - MipTailBaseIdx);

	// Handle corrupted textures :(
	if( Mips.Num() == 0 )
	{
		debugf( NAME_Error, TEXT("%s contains no miplevels! Please delete."), *GetFullName() );
		ResidentMips	= 0;
		RequestedMips	= 0;
	}
	else
	{
#if !MOBILE
		// Handle streaming textures.
		if( bIsStreamable )
		{
			// Only request lower miplevels and let texture streaming code load the rest.
			RequestedMips	= GMinTextureResidentMipCount;
		}
		// Handle non- streaming textures.
		else
#endif
		{
			// Request all miplevels allowed by device. LOD settings are taken into account below.
			RequestedMips	= GMaxTextureMipCount;
		}

		// Take LOD bias into account.
		INT MipCount = Mips.Num() - GetCachedLODBias();
		RequestedMips	= Min( MipCount, RequestedMips );
		// Make sure that we at least load the mips that reside in the packed miptail
		RequestedMips	= Max( RequestedMips, NumMipTailLevels );
		// should be as big as the mips we have already directly loaded into GPU mem
		if( ResourceMem )
		{	
			RequestedMips = Max( RequestedMips, ResourceMem->GetNumMips() );
		}
		RequestedMips	= Max( RequestedMips, 1 );
		ResidentMips	= RequestedMips;
	}

	if( GUsingMobileRHI )
	{
		for (INT EmptyIdx = 0; EmptyIdx < (Mips.Num() - ResidentMips); EmptyIdx++)
		{
			Mips(EmptyIdx).Data.RemoveBulkData();
		}
	}

	FTexture2DResource* Texture2DResource = NULL;

	// Create and return 2D resource if there are any miplevels.
	if( RequestedMips > 0 )
	{
		Texture2DResource = new FTexture2DResource( this, RequestedMips, Filename );
		// preallocated memory for the UTexture2D resource is now owned by this resource
		// and will be freed by the RHI resource or when the FTexture2DResource is deleted
		ResourceMem = NULL;
	}

	// Unlink and relink if streamable.
	UnlinkStreaming();
	if( bIsStreamable )
	{
		LinkStreaming();
	}

	return Texture2DResource;
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UTexture2D::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		INT ExclusiveResourceSize = 0;

		UBOOL InPool = FALSE;
#if USE_XeD3D_RHI
		//@TODO: Add a cross-platform way to query if a texture is resident in a texture pool
		// Check to see if the texture is in a pool
		if ((Resource != NULL) && IsValidRef(Resource->TextureRHI))
		{
			InPool = GTexturePool.IsTextureMemory(Resource->TextureRHI->BaseAddress);
		}
#endif

		if (!InPool)
		{
			// Not in the pool, add only the instantaneous resident size
			ExclusiveResourceSize += CalcTextureMemorySize(ResidentMips);
		}

		return ExclusiveResourceSize;
	}
	else
	{
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();

		for( INT MipIndex=0; MipIndex<Mips.Num(); MipIndex++ )
		{
			ResourceSize += Mips(MipIndex).Data.GetBulkDataSize();
		}

		return ResourceSize;
	}
}

/**
 * Returns whether miplevels should be forced resident.
 *
 * @return TRUE if either transient or serialized override requests miplevels to be resident, FALSE otherwise
 */
UBOOL UTexture2D::ShouldMipLevelsBeForcedResident() const
{
	if ( bGlobalForceMipLevelsToBeResident || bForceMiplevelsToBeResident )
	{
		return TRUE;
	}
	FLOAT CurrentTime = FLOAT(appSeconds() - GStartTime);
	if ( ForceMipLevelsToBeResidentTimestamp >= CurrentTime )
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * Whether all miplevels of this texture have been fully streamed in, LOD settings permitting.
 */
UBOOL UTexture2D::IsFullyStreamedIn()
{
	// Non-streamable textures are considered to be fully streamed in.
	UBOOL bIsFullyStreamedIn = TRUE;
	if( bIsStreamable )
	{
		// Calculate maximum number of mips potentially being resident based on LOD settings and device max texture count.
		INT MaxResidentMips = Max( 1, Min( Mips.Num() - GetCachedLODBias(), GMaxTextureMipCount ) );
		// >= as LOD settings can change dynamically and we consider a texture that is about to loose miplevels to still
		// be fully streamed.
		bIsFullyStreamedIn = ResidentMips >= MaxResidentMips;
	}
	return bIsFullyStreamedIn;
}

/** 
* script accessible function to create and initialize a new Texture2D with the requested settings 
*/
void UTexture2D::execCreate(FFrame& Stack, RESULT_DECL)
{
	P_GET_INT(InSizeX);
	P_GET_INT(InSizeY);
	P_GET_BYTE_OPTX(InFormat, PF_A8R8G8B8);
	P_FINISH;

	EPixelFormat DesiredFormat = EPixelFormat(InFormat);
	if (InSizeX > 0 && InSizeY > 0 )
	{
		UTexture2D* NewTexture = Cast<UTexture2D>(StaticConstructObject(GetClass(), GetTransientPackage(), NAME_None, RF_Transient));
		if (NewTexture != NULL)
		{
			// Disable compression
			NewTexture->CompressionNone			= TRUE;
			NewTexture->CompressionSettings		= TC_Default;
			NewTexture->MipGenSettings			= TMGS_NoMipmaps;
			NewTexture->CompressionNoAlpha		= TRUE;
			NewTexture->DeferCompression		= FALSE;
			// Untiled format
			NewTexture->bNoTiling				= TRUE;

			NewTexture->Init(InSizeX, InSizeY, DesiredFormat);
		}
		*(UTexture2D**)Result = NewTexture;
	}
	else
	{
		debugf(NAME_Warning, TEXT("Invalid parameters specified for UTexture2D::Create()"));
		*(UTextureRenderTarget2D**)Result = NULL;
	}
}

/** Tells the streaming system that it should force all mip-levels to be resident for a number of seconds. */
void UTexture2D::SetForceMipLevelsToBeResident( FLOAT Seconds, INT CinematicTextureGroups )
{
	DWORD TextureGroupBitfield = (DWORD) CinematicTextureGroups;
	DWORD MyTextureGroup = GBitFlag[LODGroup];
	bUseCinematicMipLevels = (TextureGroupBitfield & MyTextureGroup) ? TRUE : FALSE;
	ForceMipLevelsToBeResidentTimestamp = FLOAT(appSeconds() - GStartTime) + Seconds;
}

#if WITH_EDITOR
/** Recreates system memory data for textures that do not use GPU resources (1 bit textures).  Should be called when data in the top level mip changes **/
void UTexture2D::UpdateSystemMemoryData()
{
	// This function should never be called outside the editor
	check(GIsEditor);
	// System memory data is only used for 1 bit formats
	check(Format == PF_A1);
	
	SystemMemoryData.Empty();

	// Calculate the size of the image in bytes 
	UINT SizeBytes = CalculateImageBytes(SizeX, SizeY, 0, PF_A1);
	// Make sure our array holds enough memory for the data
	SystemMemoryData.Add( SizeBytes );
	// Copy the data from the top level mip
	appMemcpy( &SystemMemoryData(0), (BYTE*)Mips(0).Data.Lock(LOCK_READ_ONLY), SizeBytes );
	Mips(0).Data.Unlock();
}

/**
 *	Asynchronously update a set of regions of a texture with new data.
 *	@param MipIndex - the mip number to update
 *	@param NumRegions - number of regions to update
 *	@param Regions - regions to update
 *	@param SrcPitch - the pitch of the source data in bytes
 *	@param SrcBpp - the size one pixel data in bytes
 *	@param SrcData - the source data
 *  @param bFreeData - if TRUE, the SrcData and Regions pointers will be freed after the update.
 */
void UTexture2D::UpdateTextureRegions( INT MipIndex, UINT NumRegions, FUpdateTextureRegion2D* Regions, UINT SrcPitch, UINT SrcBpp, BYTE* SrcData, UBOOL bFreeData )
{
	if( Resource )
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			INT MipIndex;
			UINT NumRegions;
			FUpdateTextureRegion2D* Regions;
			UINT SrcPitch;
			UINT SrcBpp;
			BYTE* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = (FTexture2DResource*)Resource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTextureRegionsData,
			FUpdateTextureRegionsData*,RegionData,RegionData,
			UBOOL,bFreeData,bFreeData,
		{
			RHIUpdateTexture2D(RegionData->Texture2DResource->GetTexture2DRHI(), RegionData->MipIndex, RegionData->NumRegions, RegionData->Regions, RegionData->SrcPitch, RegionData->SrcBpp, RegionData->SrcData );
			if( bFreeData )
			{
				appFree(RegionData->Regions);
				appFree(RegionData->SrcData);
			}
			delete RegionData;
		});
	}
}


#endif

/*-----------------------------------------------------------------------------
	FTexture2DResource implementation.
-----------------------------------------------------------------------------*/

/**
 * Minimal initialization constructor.
 *
 * @param InOwner			UTexture2D which this FTexture2DResource represents.
 * @param InitialMipCount	Initial number of miplevels to upload to card
 * @param InFilename		Filename to read data from
 */
FTexture2DResource::FTexture2DResource( UTexture2D* InOwner, INT InitialMipCount, const FString& InFilename )
:	Owner( InOwner )
,	ResourceMem( InOwner->ResourceMem )
,	Filename( InFilename )
,	IORequestCount( 0 )
,	bUsingInPlaceRealloc(FALSE)
,	bPrioritizedIORequest(FALSE)
,	NumFailedReallocs(0)
#if STATS
,	TextureSize( 0 )
,	IntermediateTextureSize( 0 )
#if _WINDOWS
,	TextureSize_360( 0 )
,	IntermediateTextureSize_360(0)
#endif
#endif
{
	bIgnoreGammaConversions = !Owner->SRGB;

	// First request to create the resource. Decrement the counter to signal that the resource is not ready
	// for streaming yet.
	if( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Requests )
	{
		Owner->PendingMipChangeRequestStatus.Decrement();
	}
	// This can happen if the resource is re-created without ever having had InitRHI called on it.
	else
	{
		check(Owner->PendingMipChangeRequestStatus.GetValue() == TexState_InProgress_Initialization );
	}

	check(InitialMipCount>0);
	check(ARRAY_COUNT(MipData)>=GMaxTextureMipCount);
	check(InitialMipCount==Owner->ResidentMips);
	check(InitialMipCount==Owner->RequestedMips);

	// Keep track of first miplevel to use.
	FirstMip = Owner->Mips.Num() - InitialMipCount;
	check(FirstMip>=0);
	// texture must be as big as base miptail level
	check(FirstMip<=Owner->MipTailBaseIdx);

	// Can't discard the bulk data memory for image reflection textures, as the texture array may need to access that at any time
	const UBOOL bDiscardInternalCopy = Owner->LODGroup != TEXTUREGROUP_ImageBasedReflection;

	// Retrieve initial bulk data.
	// todo: We could store these readers in a TMap<FString,FArchive*> to avoid reopening it each time. 
	// Doesn't seem to be a bottleneck however, and it potentially crash causing with static pointers
	FArchive* TFCReader = NULL;
	for( INT MipIndex=0; MipIndex<ARRAY_COUNT(MipData); MipIndex++ )
	{
		MipData[MipIndex] = NULL;
		if( MipIndex < Owner->Mips.Num() ) 
		{
			FTexture2DMipMap& Mip = InOwner->Mips(MipIndex);
			if( MipIndex < FirstMip )
			{
				// In the case of seekfree loading we want to make sure that mip data that isn't beeing used doesn't
				// linger around. This can happen if texture resolution is less than what was cooked with, which is
				// common on the PC.
				if( GUseSeekFreeLoading && Mip.Data.IsBulkDataLoaded() )
				{
					// Retrieve internal pointer...
					void* InternalBulkDataMemory = NULL;
					Mip.Data.GetCopy( &InternalBulkDataMemory, bDiscardInternalCopy );
					// ... and free it.
					appFree( InternalBulkDataMemory );
				}
			}
			else
			{
				if( Mip.Data.IsAvailableForUse() )
				{
					if( Mip.Data.IsStoredInSeparateFile() )
					{
#if MOBILE
						// open the TFC file
						if (TFCReader == NULL)
						{
							TFCReader = GFileManager->CreateFileReader(*Filename);
						}
						
						TFCReader->Seek(Mip.Data.GetBulkDataOffsetInFile());

						// is the mip data in the tfc compressed?
						if( Mip.Data.IsStoredCompressedOnDisk() )
						{
							INT CompressedSize = Mip.Data.GetBulkDataSizeOnDisk();
							INT UncompressedSize = Mip.Data.GetBulkDataSize();

							// allocate buffer
							MipData[MipIndex] = appMalloc(UncompressedSize);
							// read it in
							TFCReader->SerializeCompressed(MipData[MipIndex], 0, Mip.Data.GetDecompressionFlags(), FALSE);
						}
						else
						{
							// allocate mip data
							MipData[MipIndex] = appMalloc(Mip.Data.GetBulkDataSize());
							// read it in
							TFCReader->Serialize(MipData[MipIndex], Mip.Data.GetBulkDataSize());
						}
#else
						debugf( NAME_Error, TEXT("Corrupt texture [%s]! Missing bulk data for MipIndex=%d"),*InOwner->GetFullName(),MipIndex );
#endif
					}				
					else			
					{
						// Get copy of data, potentially loading array or using already loaded version.
						Mip.Data.GetCopy( &MipData[MipIndex], bDiscardInternalCopy );	
						check(MipData[MipIndex]);
					}
				}
			}

			if(Owner->Format == PF_G8 &&
				UTexture2D::GetEffectivePixelFormat((EPixelFormat)Owner->Format, Owner->SRGB) == PF_A8R8G8B8)
			{
				// expand 1 byte format to 4 byte by replicating each channel
				check(Mip.Data.GetElementSize() == 1);

				BYTE *OldData = (BYTE *)MipData[MipIndex];	

				if(OldData)
				{	
					DWORD Size = Mip.SizeX * Mip.SizeY;
					MipData[MipIndex] = appMalloc(Size * 4);

					BYTE *Src = OldData;
					DWORD *Dst = (DWORD *)MipData[MipIndex];

					for(DWORD I=0; I<Size; ++I)
					{
						DWORD c = *Src++;
						*Dst++ = (c<<24) | (c<<16) | (c<<8) | c;
					}

					appFree( OldData );
				}
			}
		}
	}
	// close any open streaming file
	delete TFCReader;

	STAT( TextureSize = Owner->CalcTextureMemorySize( InitialMipCount ) );
	STATWIN( TextureSize_360 = Owner->Get360Size(InitialMipCount) );
	STAT( IncMemoryStats( Owner ) );
	STATWIN( IncMemoryStats360( Owner ) );
#if XBOX
	INC_MEMORY_STAT_BY(STAT_TexturePool_PackMipTailSavings,XeCalcUnusedMipTailSize(Owner->SizeX, Owner->SizeY, EPixelFormat(Owner->Format), Owner->Mips.Num(), (Owner->MipTailBaseIdx == -1) ? FALSE : TRUE ));
#endif
}

/**
 * Destructor, freeing MipData in the case of resource being destroyed without ever 
 * having been initialized by the rendering thread via InitRHI.
 */
FTexture2DResource::~FTexture2DResource()
{
	STAT( DecMemoryStats( Owner ) );
	STATWIN( DecMemoryStats360( Owner ) );
#if XBOX
	DEC_MEMORY_STAT_BY(STAT_TexturePool_PackMipTailSavings,XeCalcUnusedMipTailSize(Owner->SizeX, Owner->SizeY, EPixelFormat(Owner->Format), Owner->Mips.Num(), (Owner->MipTailBaseIdx == -1) ? FALSE : TRUE ));
#endif

	// free resource memory that was preallocated
	// The deletion needs to happen in the rendering thread.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		DeleteResourceMem,
		FTexture2DResourceMem*,ResourceMem,ResourceMem,
		{
			delete ResourceMem;
		});

	// Make sure we're not leaking memory if InitRHI has never been called.
	for( INT MipIndex=0; MipIndex<ARRAY_COUNT(MipData); MipIndex++ )
	{
		// free any mip data that was copied 
		if( MipData[MipIndex] )
		{
			appFree( MipData[MipIndex] );
		}
		MipData[MipIndex] = NULL;
	}
}

/**
 * Called when the resource is initialized. This is only called by the rendering thread.
 */
void FTexture2DResource::InitRHI()
{
	FTexture2DScopedDebugInfo ScopedDebugInfo(Owner);

	INC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
	INC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, TextureSize );
	STAT( check( IntermediateTextureSize == 0 ) );
	STATWIN( INC_DWORD_STAT_BY( STAT_XboxTextureMemory, TextureSize_360 ) );
	STATWIN( check( IntermediateTextureSize_360 == 0 ) );

	check( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_InProgress_Initialization );
	UINT SizeX = Owner->Mips(FirstMip).SizeX;
	UINT SizeY = Owner->Mips(FirstMip).SizeY;

	// Create the RHI texture.
	DWORD TexCreateFlags = Owner->SRGB ? TexCreate_SRGB : 0;
	// if no miptail is available then create the texture without a packed miptail
	if( Owner->MipTailBaseIdx == -1 )
	{
		TexCreateFlags |= TexCreate_NoMipTail;
	}
	// disable tiled format if needed
	if( Owner->bNoTiling )
	{
		TexCreateFlags |= TexCreate_NoTiling;
	}
#if PS3
	// PS3 can provide automatic texture biasing for normal maps.
	if( Owner->BiasNormalMap() )
	{
		TexCreateFlags |= TexCreate_BiasNormalMap;
	}
#endif

	EPixelFormat EffectiveFormat = UTexture2D::GetEffectivePixelFormat((EPixelFormat)Owner->Format, Owner->SRGB);
	if ( Owner->bForcePVRTC4 && (GTextureFormatSupport & TEXSUPPORT_PVRTC) )
	{
		EffectiveFormat = PF_DXT5;
	}

	//d3d source can be grabbed from the RHI, but ES2 cannot
	UBOOL bSkipRHITextureCreation = Owner->bIsCompositingSource && GUsingES2RHI;
	if (GIsEditor || (!bSkipRHITextureCreation))
	{
#if FLASH
		if (EffectiveFormat >= PF_DXT1 && EffectiveFormat <= PF_DXT5)
		{
			UINT FirstMipSizeX = Owner->SizeX >> FirstMip;
			UINT FirstMipSizeY = Owner->SizeY >> FirstMip;

			Texture2DRHI	= RHICreateFlashTexture2D( 
			    FirstMipSizeX, 
			    FirstMipSizeY, 
			    EffectiveFormat, 
			    Owner->RequestedMips, 
			    TexCreateFlags, 
			    MipData[FirstMip], 
			    Owner->Mips(FirstMip).Data.GetBulkDataSize() );
		}
		else
#endif
		{
		// create texture with ResourceMem data when available
		Texture2DRHI	= RHICreateTexture2D( SizeX, SizeY, EffectiveFormat, Owner->RequestedMips, TexCreateFlags, ResourceMem );

		if( ResourceMem )
		{
			// when using resource memory the RHI texture has already been initialized with data and won't need to have mips copied
			check(Owner->RequestedMips == ResourceMem->GetNumMips());
			check(SizeX == ResourceMem->GetSizeX() && SizeY == ResourceMem->GetSizeY());
			for( INT MipIndex=0; MipIndex<Owner->Mips.Num(); MipIndex++ )
			{
				MipData[MipIndex] = NULL;
			}
		}
		else
		{
			// Read the resident mip-levels into the RHI texture.
			for( INT MipIndex=FirstMip; MipIndex<Owner->Mips.Num(); MipIndex++ )
			{
				if( MipData[MipIndex] != NULL )
				{
					UINT DestPitch;
					void* TheMipData = RHILockTexture2D( Texture2DRHI, MipIndex - FirstMip, TRUE, DestPitch, FALSE );
					GetData( MipIndex, TheMipData, DestPitch );
					RHIUnlockTexture2D( Texture2DRHI, MipIndex - FirstMip, FALSE );
				}
			}
		}
	}
		// the 2d rhi is also the general rhi
		TextureRHI		= Texture2DRHI;
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		GSystemSettings.TextureLODSettings.GetSamplerFilter( Owner ),
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap,
		(Owner->LODGroup == TEXTUREGROUP_UI) ? ESamplerMipMapLODBias(-Owner->Mips.Num()) : MIPBIAS_None
	);
	SamplerStateRHI = RHICreateSamplerState( SamplerStateInitializer );

	// Set the greyscale format flag appropriately.
	bGreyScaleFormat = (Owner->Format == PF_G8);

	// Update mip-level fading.
	EMipFadeSettings MipFadeSetting = (Owner->LODGroup == TEXTUREGROUP_Lightmap || Owner->LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;
	MipBiasFade.SetNewMipCount( Owner->RequestedMips, Owner->RequestedMips, LastRenderTime, MipFadeSetting );

	// We're done with initialization.
	Owner->PendingMipChangeRequestStatus.Increment();
}

/**
 * Called when the resource is released. This is only called by the rendering thread.
 */
void FTexture2DResource::ReleaseRHI()
{
	// Make sure it's safe to release the texture.
	if( Owner->PendingMipChangeRequestStatus.GetValue() != TexState_ReadyFor_Requests )
	{
		// We'll update the streaming portion until we're done streaming.
		UTexture2D* NonConstOwner = const_cast< UTexture2D* >( Owner );
		while( NonConstOwner->UpdateStreamingStatus() )
		{
			// Give up the timeslice.
			appSleep(0);
		}
	}

	DEC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
	DEC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, TextureSize );
	STAT( check( IntermediateTextureSize == 0 ) );
	STATWIN( DEC_DWORD_STAT_BY( STAT_XboxTextureMemory, TextureSize_360 ) );
	STATWIN( check( IntermediateTextureSize_360 == 0 ) );

	check( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Requests );	
	FTextureResource::ReleaseRHI();
	Texture2DRHI.SafeRelease();

	Owner->PendingMipChangeRequestStatus.Decrement();
}

/** Returns the width of the texture in pixels. */
UINT FTexture2DResource::GetSizeX() const
{
	return Owner->SizeX;
}

/** Returns the height of the texture in pixels. */
UINT FTexture2DResource::GetSizeY() const
{
	return Owner->SizeY;
}

/**
 * Writes the data for a single mip-level into a destination buffer.
 *
 * @param MipIndex		Index of the mip-level to read.
 * @param Dest			Address of the destination buffer to receive the mip-level's data.
 * @param DestPitch		Number of bytes per row
 */
void FTexture2DResource::GetData( UINT MipIndex, void* Dest, UINT DestPitch )
{
	const FTexture2DMipMap& MipMap = Owner->Mips(MipIndex);
	check( MipData[MipIndex] );

#if XBOX || WIIU
	BYTE *Src = (BYTE*) MipData[MipIndex];
	BYTE *Dst = (BYTE*) Dest;
	RHISelectiveCopyMipData(Texture2DRHI, Src, Dst, MipMap.Data.GetBulkDataSize(), MipIndex-FirstMip);
#else
	UINT EffectiveSize = 0;
	EPixelFormat EffectiveFormat = (EPixelFormat)Owner->Format;
	UINT SrcPitch = 0;
	UINT NumRows = 0;
#if PS3
	{
		extern UBOOL RequireLinearTexture(INT Format, DWORD SizeX, DWORD SizeY);
		extern DWORD GetTexturePitch(INT Format, DWORD SizeX, DWORD MipIndex, UBOOL bIsLinear);
		extern DWORD GetMipNumRows(INT Format, DWORD SizeY, DWORD MipIndex);
		UBOOL bIsLinear	= RequireLinearTexture( Owner->Format, Owner->SizeX, Owner->SizeY );
		SrcPitch	= GetTexturePitch( Owner->Format, Owner->SizeX, MipIndex, bIsLinear );
		NumRows	= GetMipNumRows( Owner->Format, Owner->SizeY, MipIndex );
		EffectiveSize = SrcPitch * NumRows;
	}
#else	// PS3
#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		extern UINT GetMipStride(UINT SizeX, EPixelFormat Format, UINT MipIndex);
		extern UINT GetMipNumRows(UINT SizeY, EPixelFormat Format, UINT MipIndex);
		// apply PVRTC4 override
		if ( Owner->bForcePVRTC4 && (GTextureFormatSupport & TEXSUPPORT_PVRTC) )
		{
			EffectiveFormat = PF_DXT5;
		}
		SrcPitch = GetMipStride(Owner->SizeX, EffectiveFormat, MipIndex);
		NumRows = GetMipNumRows(Owner->SizeY, EffectiveFormat, MipIndex);
		EffectiveSize = SrcPitch * NumRows;
	}
	else
#endif	// WITH_MOBILE_RHI
	{
		EffectiveFormat = UTexture2D::GetEffectivePixelFormat((EPixelFormat)Owner->Format, Owner->SRGB);

		UINT BlockSizeX = GPixelFormats[EffectiveFormat].BlockSizeX;		// Block width in pixels
		UINT BlockSizeY = GPixelFormats[EffectiveFormat].BlockSizeY;		// Block height in pixels
		UINT BlockBytes = GPixelFormats[EffectiveFormat].BlockBytes;
		UINT NumColumns = (MipMap.SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
		NumRows    = (MipMap.SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
		SrcPitch   = NumColumns * BlockBytes;						// Num-of bytes per row in the source data
		EffectiveSize = BlockBytes*NumColumns*NumRows;
	}
#endif	// !PS3

#if CONSOLE
	// on console we don't want onload conversions
	checkf(EffectiveSize == (UINT)MipMap.Data.GetBulkDataSize(), 
		TEXT("Texture '%s', mip %d, has a BulkDataSize [%d] that doesn't match calculated size [%d]. Texture size %dx%d, format %d"),
		*Owner->GetPathName(), MipIndex, MipMap.Data.GetBulkDataSize(), EffectiveSize, Owner->SizeX, Owner->SizeY, EffectiveFormat);
#endif

	if ( SrcPitch == DestPitch )
	{
		// Copy data, not taking into account stride!
		appMemcpy( Dest, MipData[MipIndex], EffectiveSize );
	}
	else
	{
		// Copy data, taking the stride into account!
		BYTE *Src = (BYTE*) MipData[MipIndex];
		BYTE *Dst = (BYTE*) Dest;
		UINT NumBytesPerRow = Min<UINT>(SrcPitch, DestPitch);
		for ( UINT Row=0; Row < NumRows; ++Row )
		{
			appMemcpy( Dst, Src, NumBytesPerRow );
			Src += SrcPitch;
			Dst += DestPitch;
		}
		check( (PTRINT(Src) - PTRINT(MipData[MipIndex])) == PTRINT(EffectiveSize) );
	}
#endif	// !XBOX
	
	// If we allow full resets we need to hold onto this data
	if( !GAllowFullRHIReset )
	{
		// Free data retrieved via GetCopy inside constructor.
		if( MipMap.Data.ShouldFreeOnEmpty() )
	    {
		    appFree( MipData[MipIndex] );
	    }
	    MipData[MipIndex] = NULL;
    }
}

/**
 * Called from the game thread to kick off a change in ResidentMips after modifying RequestedMips.
 *
 * @param bShouldPrioritizeAsyncIORequest	- Whether the Async I/O request should have higher priority
 */
void FTexture2DResource::BeginUpdateMipCount( UBOOL bShouldPrioritizeAsyncIORequest )
{
	// Set the state to TexState_InProgress_Allocation.
	check( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Requests );
	Owner->PendingMipChangeRequestStatus.Set( TexState_InProgress_Allocation );

	bPrioritizedIORequest = bShouldPrioritizeAsyncIORequest;
	GStreamMemoryTracker.GameThread_BeginUpdate( *Owner );

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FUpdateMipCountCommand,
		FTexture2DResource*, Texture2DResource, this,
		{
			Texture2DResource->UpdateMipCount( );
		});
}

/**
 * Called from the game thread to kick off async I/O to load in new mips.
 */
void FTexture2DResource::BeginLoadMipData( )
{
	// Set the state to TexState_InProgress_Loading.
	check( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Loading );
	Owner->PendingMipChangeRequestStatus.Set( TexState_InProgress_Loading );

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FUpdateMipCountCommand,
		FTexture2DResource*, Texture2DResource, this,
		{
			Texture2DResource->LoadMipData();
		});
}

/**
 * Called from the game thread to kick off finalization of mip change.
 */
void FTexture2DResource::BeginFinalizeMipCount()
{
	check( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Finalization );
	// Finalization is now in flight.
	Owner->PendingMipChangeRequestStatus.Decrement();

	if( IsInRenderingThread() )
	{
		// We're in the rendering thread so just go ahead and finalize mips
		FinalizeMipCount();				
	}
	else
	{
		// We're in the game thread so enqueue the request
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FFinalineMipCountCommand,
			FTexture2DResource*, Texture2DResource, this,
			{
				Texture2DResource->FinalizeMipCount();
			});
	}
}

/**
 * Called from the game thread to kick off cancellation of async operations for request.
 */
void FTexture2DResource::BeginCancelUpdate()
{
	check( Owner->PendingMipChangeRequestStatus.GetValue() >= TexState_ReadyFor_Finalization );
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FCancelUpdateCommand,
		FTexture2DResource*, Texture2DResource, this,
		{
			Texture2DResource->CancelUpdate();
		});
}

/**
 * Called from the rendering thread to perform the work to kick off a change in ResidentMips.
 */
void FTexture2DResource::UpdateMipCount()
{
	FTexture2DScopedDebugInfo ScopedDebugInfo(Owner);

	SCOPE_CYCLE_COUNTER(STAT_RenderingThreadUpdateTime);

	check(Owner->bIsStreamable);
	check(Owner->PendingMipChangeRequestStatus.GetValue() == TexState_InProgress_Allocation);
	check( IsValidRef(IntermediateTextureRHI) == FALSE );

	FirstMip	= Owner->Mips.Num() - Owner->RequestedMips;
	check(FirstMip>=0);

	UINT SizeX	= Owner->Mips(FirstMip).SizeX;
	UINT SizeY	= Owner->Mips(FirstMip).SizeY;

	// Create the RHI texture.
	DWORD TexCreateFlags = Owner->SRGB ? TexCreate_SRGB : 0;
	TexCreateFlags |= TexCreate_AllowFailure | TexCreate_DisableAutoDefrag;

	// If we've tried X number of times, or a multiple of Y, the try a defrag if we fail this time as well.
	if ( NumFailedReallocs > 0 && (NumFailedReallocs == GDefragmentationRetryCounter || (NumFailedReallocs % GDefragmentationRetryCounterLong) == 0) )
	{
		TexCreateFlags &= ~TexCreate_DisableAutoDefrag;
	}

	// if no miptail is available then create the texture without a packed miptail
	if( Owner->MipTailBaseIdx == -1 )
	{
		TexCreateFlags |= TexCreate_NoMipTail;
	}
	// disable tiled format if needed
	if( Owner->bNoTiling )
	{
		TexCreateFlags |= TexCreate_NoTiling;
	}
#if PS3
	// PS3 can provide automatic texture biasing for normal maps.
	if( Owner->BiasNormalMap() )
	{
		TexCreateFlags |= TexCreate_BiasNormalMap;
	}
#endif

    // First try to create a new texture.
    bUsingInPlaceRealloc = FALSE;

    EPixelFormat EffectiveFormat = UTexture2D::GetEffectivePixelFormat((EPixelFormat)Owner->Format, Owner->SRGB);

    check(EffectiveFormat == (EPixelFormat)Owner->Format);		// we don' want to stream texture that require onload conversion

	// We are going to try to asynchronously allocate the texture.
	Owner->PendingMipChangeRequestStatus.Increment();
	check(Owner->PendingMipChangeRequestStatus.GetValue() == TexState_InProgress_AsyncAllocation);

	// Try to initiate an asynchronous reallocation.
	UBOOL bImmediateReallocation = FALSE;
	IntermediateTextureRHI = RHIAsyncReallocateTexture2D( Texture2DRHI, Owner->RequestedMips, SizeX, SizeY, &Owner->PendingMipChangeRequestStatus );
	bUsingInPlaceRealloc = IsValidRef( IntermediateTextureRHI );

    // Did it fail?
    if ( IsValidRef(IntermediateTextureRHI) == FALSE )
    {
		// Async reallocation has failed.
		Owner->PendingMipChangeRequestStatus.Decrement();

		// Create a complete copy of the texture, using the new mipcount.
		IntermediateTextureRHI = RHICreateTexture2D( SizeX, SizeY, EffectiveFormat, Owner->RequestedMips, TexCreateFlags, NULL );

		// Did it fail?
		if ( IsValidRef(IntermediateTextureRHI) == FALSE )
		{
			// Try to an immediate reallocation.
			IntermediateTextureRHI = RHIReallocateTexture2D( Texture2DRHI, Owner->RequestedMips, SizeX, SizeY );
			bUsingInPlaceRealloc = bImmediateReallocation = IsValidRef( IntermediateTextureRHI );
		}
    }

	// Did we reallocate using RHIReallocateTexture2D()?
	if ( bImmediateReallocation )
	{
		// If so, the allocation is now completed.
		Owner->PendingMipChangeRequestStatus.Decrement();
		check( Owner->PendingMipChangeRequestStatus.GetValue() == TexState_ReadyFor_Loading );
	}

	if ( bUsingInPlaceRealloc )
	{
		if ( Owner->RequestedMips > Owner->ResidentMips )
		{
			INC_DWORD_STAT( STAT_GrowingReallocations );
		}
		else
		{
			INC_DWORD_STAT( STAT_ShrinkingReallocations );
		}
	}
	else if ( IsValidRef(IntermediateTextureRHI) )
	{
		const INT SrcMipOffset = Max( 0, Owner->ResidentMips - Owner->RequestedMips );
		const INT DstMipOffset = Max( 0, Owner->RequestedMips - Owner->ResidentMips );
		INT NumMipTailLevels = Max( 0, Owner->Mips.Num() - Owner->MipTailBaseIdx );
		INT NumSharedMips = Min( Owner->ResidentMips, Owner->RequestedMips ) - NumMipTailLevels + 1;

		SCOPED_DRAW_EVENT(EventUpdateMipCount)(DEC_SCENE_ITEMS,TEXT("UpdateMipCount"));

		// Copy shared miplevels.
		for( INT MipIndex=0; MipIndex < NumSharedMips; MipIndex++ )
		{
			// let platform perform the copy from mip to mip, it may use the PendingMipChangeRequestStatus to sync up copy later;
			// figures out size of memory transfer. Includes alignment mojo on consoles.
			RHICopyMipToMipAsync(
				Texture2DRHI,
				MipIndex + SrcMipOffset,
				IntermediateTextureRHI,
				MipIndex + DstMipOffset,
				Owner->Mips(MipIndex + FirstMip + DstMipOffset).Data.GetBulkDataSize(),
				Owner->PendingMipChangeRequestStatus);
		}

		INC_DWORD_STAT( STAT_FullReallocations );
	}
	else
	{
		// We failed to allocate texture memory. Abort silently.

		// Was it the first attempt that failed?
		if ( NumFailedReallocs == 0 )
		{
			INC_DWORD_STAT( STAT_FailedReallocations );
		}
		NumFailedReallocs++;
	}

	// If async reallocation isn't in progress, go ahead to the next step.
	if ( !bUsingInPlaceRealloc || Owner->PendingMipChangeRequestStatus.GetValue() == TexState_InProgress_Allocation )
	{
		// Set the state to TexState_InProgress_Loading and start loading right away.
		Owner->PendingMipChangeRequestStatus.Set( TexState_InProgress_Loading );
		LoadMipData();
	}
	else
	{
		// Decrement the counter so that when async allocation finishes the game thread will see TexState_ReadyFor_Loading.
		Owner->PendingMipChangeRequestStatus.Decrement();
	}

	// Update the memory tracker.
	GStreamMemoryTracker.RenderThread_Update( *Owner, bUsingInPlaceRealloc, IsValidRef(IntermediateTextureRHI) );
}

/**
 * Called from the rendering thread to start async I/O to load in new mips.
 */
void FTexture2DResource::LoadMipData()
{
	SCOPE_CYCLE_COUNTER(STAT_RenderingThreadUpdateTime);
	check(Owner->bIsStreamable);
	check(Owner->PendingMipChangeRequestStatus.GetValue() == TexState_InProgress_Loading);

	IORequestCount = 0;
	if ( IsValidRef(IntermediateTextureRHI) && !Owner->bHasCancelationPending )
	{
		STAT( IntermediateTextureSize = Owner->CalcTextureMemorySize( Owner->RequestedMips ) );
		STATWIN( IntermediateTextureSize_360 = Owner->Get360Size(Owner->RequestedMips) );

		INC_DWORD_STAT_BY( STAT_TextureMemory, IntermediateTextureSize );
		INC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, IntermediateTextureSize );
		STATWIN( INC_DWORD_STAT_BY( STAT_XboxTextureMemory, IntermediateTextureSize_360 ) );

		// Had this texture previously failed to reallocate?
		if ( NumFailedReallocs > 0 )
		{
			DEC_DWORD_STAT( STAT_FailedReallocations );
		}
		NumFailedReallocs = 0;

		if ( bUsingInPlaceRealloc )
		{
			RHIFinalizeAsyncReallocateTexture2D( IntermediateTextureRHI, TRUE );
		}

		FIOSystem* IO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
		check(IO);

#if WITH_SUBSTANCE_AIR
		UBOOL bLoadSubstance = (GSubstanceAirTextureReader && Cast<USubstanceAirTexture2D>((UObject*)Owner) != NULL) ? TRUE : FALSE;
#endif

		// Read into new miplevels, if any.
		INT FirstSharedMip	= (Owner->RequestedMips - Min(Owner->ResidentMips,Owner->RequestedMips));
		for( INT MipIndex=0; MipIndex<FirstSharedMip; MipIndex++ )
		{
			const FTexture2DMipMap& MipMap = Owner->Mips( MipIndex + FirstMip );

			// Lock new texture.
			UINT DestPitch;
			void* TheMipData = RHILockTexture2D( IntermediateTextureRHI, MipIndex, TRUE, DestPitch, FALSE );

#if PS3
			void* OriginalMipData = NULL;
			UINT BlockSizeX = GPixelFormats[Owner->Format].BlockSizeX;		// Block width in pixels
			UINT BlockSizeY = GPixelFormats[Owner->Format].BlockSizeY;		// Block height in pixels
			UINT BlockBytes = GPixelFormats[Owner->Format].BlockBytes;
			UINT NumColumns = (MipMap.SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
			UINT NumRows    = (MipMap.SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
			UINT SrcPitch   = NumColumns * BlockBytes;						// Num-of bytes per row in the source data

			//@TODO: If the pitch doesn't match, load into temp memory and upload to video memory later
			check( SrcPitch == DestPitch);
#endif

			EAsyncIOPriority AsyncIOPriority = bPrioritizedIORequest ? AIOP_BelowNormal : AIOP_Low;

#if WITH_SUBSTANCE_AIR
			// If a cache reader is available, load with it instead of standard streaming methods
			if (bLoadSubstance)
			{
				FString TextureName;
				Owner->GetFullName().ToLower().Split(TEXT(" "), NULL, &TextureName);

					// Push a new unreduxing job to thread pool
					GThreadPool->AddQueuedWork(new ATCReadWork(
						TextureName,
						MipMap.SizeX,
						MipMap.SizeY,
						TheMipData,
						MipMap.Data.GetBulkDataSize(),
						&Owner->PendingMipChangeRequestStatus
						));

				// Fake an IO Request since we fulfill the request in another way
				IORequestIndices[IORequestCount++] = 0;
			}
			else
#endif

			// Mip data is already loaded
			if( MipMap.Data.IsBulkDataLoaded() && MipMap.Data.GetBulkDataSize() > 0 )
			{
				// Const cast here so that we can pass into the GetCopy function, we will not mutate it since we do not allow GetCopy to discard the internal copy
				FTexture2DMipMap& MutableMipMap = const_cast< FTexture2DMipMap& >( MipMap );

				// Get a copy of the data and do not discard the source.
				MutableMipMap.Data.GetCopy( &TheMipData, FALSE );

				// We unlock here because we will not need to wait for async loading.
				RHIUnlockTexture2D( IntermediateTextureRHI, MipIndex, FALSE );
			}
			// Load and decompress async.
			else if( MipMap.Data.IsStoredCompressedOnDisk() )
			{
			// Pass the request on to the async io manager after increasing the request count. The request count 
			// has been pre-incremented before fielding the update request so we don't have to worry about file
			// I/O immediately completing and the game thread kicking off FinalizeMipCount before this function
			// returns.
			Owner->PendingMipChangeRequestStatus.Increment();

				IORequestIndices[IORequestCount++] = IO->LoadCompressedData( 
					Filename,											// filename
					MipMap.Data.GetBulkDataOffsetInFile(),				// offset
					MipMap.Data.GetBulkDataSizeOnDisk(),				// compressed size
					MipMap.Data.GetBulkDataSize(),						// uncompressed size
					TheMipData,											// dest pointer
					MipMap.Data.GetDecompressionFlags(),				// compressed data format
					&Owner->PendingMipChangeRequestStatus,				// counter to decrement
					AsyncIOPriority										// priority
					);
				check(IORequestIndices[MipIndex]);
			}
			// Load async.
			else
			{
				// Pass the request on to the async io manager after increasing the request count. The request count 
				// has been pre-incremented before fielding the update request so we don't have to worry about file
				// I/O immediately completing and the game thread kicking off FinalizeMipCount before this function
				// returns.	
				Owner->PendingMipChangeRequestStatus.Increment();

				IORequestIndices[IORequestCount++] = IO->LoadData( 
					Filename,											// filename
					MipMap.Data.GetBulkDataOffsetInFile(),				// offset
					MipMap.Data.GetBulkDataSize(),						// size
					TheMipData,											// dest pointer
					&Owner->PendingMipChangeRequestStatus,				// counter to decrement
					AsyncIOPriority										// priority
					);
				check(IORequestIndices[MipIndex]);
			}
		}

		// Are we reducing the mip-count?
		if ( Owner->RequestedMips < Owner->ResidentMips )
		{
			// Set up MipBiasFade to start fading out mip-levels (start at 0, increase mip-bias over time).
			EMipFadeSettings MipFadeSetting = (Owner->LODGroup == TEXTUREGROUP_Lightmap || Owner->LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;
			MipBiasFade.SetNewMipCount( Owner->ResidentMips, Owner->RequestedMips, LastRenderTime, MipFadeSetting );
		}
	}

	// Decrement the state to TexState_ReadyFor_Finalization + NumMipsCurrentLoading.
	Owner->PendingMipChangeRequestStatus.Decrement();
}

/**
 * Called from the rendering thread to finalize a mip change.
 */
void FTexture2DResource::FinalizeMipCount()
{
	SCOPE_CYCLE_COUNTER(STAT_RenderingThreadFinalizeTime);

	check(Owner->bIsStreamable);
	check(Owner->PendingMipChangeRequestStatus.GetValue()==TexState_InProgress_Finalization);

	// Did we succeed to (re)allocate memory for the updated texture?
	if ( IsValidRef(IntermediateTextureRHI) )
	{
		UBOOL bSuccess = FALSE;

		// base index of destination texture's miptail. Skip mips smaller than the base miptail level 
		const INT DstMipTailBaseIdx = Owner->MipTailBaseIdx - (Owner->Mips.Num() - Owner->RequestedMips);
		check(DstMipTailBaseIdx>=0);

		// Base index of source texture's miptail. 
		INT SrcMipTailBaseIdx = Owner->MipTailBaseIdx - (Owner->Mips.Num() - Owner->ResidentMips);
		check(SrcMipTailBaseIdx>=0);

		if ( !bUsingInPlaceRealloc )
		{
			const INT SrcMipOffset = Max( 0, Owner->ResidentMips - Owner->RequestedMips );
			const INT DstMipOffset = Max( 0, Owner->RequestedMips - Owner->ResidentMips );
			INT NumMipTailLevels = Max( 0, Owner->Mips.Num() - Owner->MipTailBaseIdx );
			INT NumSharedMips = Min( Owner->ResidentMips, Owner->RequestedMips ) - NumMipTailLevels + 1;

			// Finalize asynchronous copies for mips that we copied from the old texture resource.
			for( INT MipIndex=0; MipIndex < NumSharedMips; MipIndex++ )
			{
				RHIFinalizeAsyncMipCopy( Texture2DRHI, MipIndex + SrcMipOffset, IntermediateTextureRHI, MipIndex + DstMipOffset);
			}
		}

		// Unlock texture mips that we loaded new data to.
		if ( IORequestCount > 0 )
		{
			const INT NumNewMips = Owner->RequestedMips - Owner->ResidentMips;
			const INT NumNewNonTailMips = Min(NumNewMips,DstMipTailBaseIdx);
			check( IORequestCount == NumNewNonTailMips );
			for(INT MipIndex = 0;MipIndex < NumNewNonTailMips;MipIndex++)
			{
				// Intermediate texture has RequestedMips miplevels, all of which have been locked.
				// DEXTEX: Do not unload as we didn't locked the textures in the first place
				RHIUnlockTexture2D( IntermediateTextureRHI, MipIndex, FALSE );
			}
		}

		// Perform switcheroo if the request hasn't been canceled.
		if( !Owner->bHasCancelationPending )
		{
			bSuccess		= TRUE;
			TextureRHI		= IntermediateTextureRHI;
			Texture2DRHI	= IntermediateTextureRHI;

			// Update mip-level fading.
			EMipFadeSettings MipFadeSetting = (Owner->LODGroup == TEXTUREGROUP_Lightmap || Owner->LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;
			MipBiasFade.SetNewMipCount( Owner->RequestedMips, Owner->RequestedMips, LastRenderTime, MipFadeSetting );

#if STATS
			// Update bandwidth measurements if we've streamed in mip-levels.
			if ( Owner->Timer > 0.0f && IntermediateTextureSize > TextureSize )
			{
				DOUBLE BandwidthSample = DOUBLE(IntermediateTextureSize - TextureSize) / DOUBLE(Owner->Timer);
				DOUBLE TotalBandwidth = FStreamingManagerTexture::BandwidthAverage*FStreamingManagerTexture::NumBandwidthSamples;
				TotalBandwidth -= FStreamingManagerTexture::BandwidthSamples[FStreamingManagerTexture::BandwidthSampleIndex];
				TotalBandwidth += BandwidthSample;
				FStreamingManagerTexture::BandwidthSamples[FStreamingManagerTexture::BandwidthSampleIndex] = BandwidthSample;
 				FStreamingManagerTexture::BandwidthSampleIndex = (FStreamingManagerTexture::BandwidthSampleIndex + 1) % NUM_BANDWIDTHSAMPLES;
 				FStreamingManagerTexture::NumBandwidthSamples = ( FStreamingManagerTexture::NumBandwidthSamples == NUM_BANDWIDTHSAMPLES ) ? FStreamingManagerTexture::NumBandwidthSamples : (FStreamingManagerTexture::NumBandwidthSamples+1);
 				FStreamingManagerTexture::BandwidthAverage = TotalBandwidth / FStreamingManagerTexture::NumBandwidthSamples;
 				FStreamingManagerTexture::BandwidthMaximum = Max<FLOAT>(FStreamingManagerTexture::BandwidthMaximum, BandwidthSample);
				FStreamingManagerTexture::BandwidthMinimum = appIsNearlyZero(FStreamingManagerTexture::BandwidthMinimum) ? BandwidthSample : Min<FLOAT>(FStreamingManagerTexture::BandwidthMinimum, BandwidthSample);
			}
#endif
			DEC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
			DEC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, TextureSize );
			STAT( TextureSize = IntermediateTextureSize );
			STATWIN( DEC_DWORD_STAT_BY( STAT_XboxTextureMemory, TextureSize_360 ) );
			STATWIN( TextureSize_360 = IntermediateTextureSize_360 );
		}
		// Request has been canceled.
		else
		{
			// Update mip-level fading.
			EMipFadeSettings MipFadeSetting = (Owner->LODGroup == TEXTUREGROUP_Lightmap || Owner->LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;
			MipBiasFade.SetNewMipCount( Owner->ResidentMips, Owner->ResidentMips, LastRenderTime, MipFadeSetting );

			DEC_DWORD_STAT_BY( STAT_TextureMemory, IntermediateTextureSize );
			DEC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, IntermediateTextureSize );
			STATWIN( DEC_DWORD_STAT_BY( STAT_XboxTextureMemory, IntermediateTextureSize_360 ) );
		}
		IntermediateTextureRHI.SafeRelease();

		GStreamMemoryTracker.RenderThread_Finalize( *Owner, bUsingInPlaceRealloc, bSuccess );
	}
	else
	{
		// failed
		DEC_DWORD_STAT_BY( STAT_TextureMemory, IntermediateTextureSize );
		DEC_DWORD_STAT_BY( Owner->LODGroup + STAT_TextureGroupFirst, IntermediateTextureSize );
	}

	STAT( IntermediateTextureSize = 0 );
	STATWIN( IntermediateTextureSize_360 = 0 );

	// We're done.
	Owner->PendingMipChangeRequestStatus.Decrement();
}

/**
 * Called from the rendering thread to cancel async operations for request.
 */
void FTexture2DResource::CancelUpdate()
{
	SCOPE_CYCLE_COUNTER(STAT_RenderingThreadUpdateTime);

	// TexState_InProgress_Finalization is valid as the request status gets decremented in the main thread. The actual
	// call to FinalizeMipCount will happen after this one though.
	check(Owner->PendingMipChangeRequestStatus.GetValue()>=TexState_InProgress_Finalization);
	check(Owner->bHasCancelationPending);

	// We only have anything worth cancellation if there are outstanding I/O requests.
	if( IORequestCount )
	{
		// Retrieve IO system
		FIOSystem* IO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
		check(IO);
		// Cancel requests. This only cancels pending requests and not ones currently being fulfilled.
		IO->CancelRequests( IORequestIndices, IORequestCount );
	}

	if ( bUsingInPlaceRealloc && IsValidRef(IntermediateTextureRHI) )
	{
		RHICancelAsyncReallocateTexture2D( IntermediateTextureRHI, FALSE );
	}
}

/**
 *	Tries to reallocate the texture for a new mip count.
 *	@param OldMipCount	- The old mip count we're currently using.
 *	@param NewMipCount	- The new mip count to use.
 */
UBOOL FTexture2DResource::TryReallocate( INT OldMipCount, INT NewMipCount )
{
	check( IsValidRef(IntermediateTextureRHI) == FALSE );

	INT MipIndex = Owner->Mips.Num() - NewMipCount;
	check(MipIndex>=0);
	UINT NewSizeX	= Owner->Mips(MipIndex).SizeX;
	UINT NewSizeY	= Owner->Mips(MipIndex).SizeY;

	FTexture2DRHIRef NewTextureRHI = RHIReallocateTexture2D( Texture2DRHI, NewMipCount, NewSizeX, NewSizeY );
	if ( IsValidRef(NewTextureRHI) )
	{
		Texture2DRHI = NewTextureRHI;
		TextureRHI = NewTextureRHI;

		// Update mip-level fading.
		EMipFadeSettings MipFadeSetting = (Owner->LODGroup == TEXTUREGROUP_Lightmap || Owner->LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;
		MipBiasFade.SetNewMipCount( NewMipCount, NewMipCount, LastRenderTime, MipFadeSetting );

#if STATS
		if ( NewMipCount > OldMipCount )
		{
			INC_DWORD_STAT( STAT_GrowingReallocations );
		}
		else
		{
			INC_DWORD_STAT( STAT_ShrinkingReallocations );
		}
		STAT( INT OldSize = Owner->CalcTextureMemorySize( OldMipCount ) );
		STAT( INT NewSize = Owner->CalcTextureMemorySize( NewMipCount ) );
		DEC_DWORD_STAT_BY( STAT_TextureMemory, OldSize );
		INC_DWORD_STAT_BY( STAT_TextureMemory, NewSize );
		STAT( TextureSize = NewSize );
		STATWIN( INT Old360Size = Owner->Get360Size( OldMipCount ) );
		STATWIN( INT New360Size = Owner->Get360Size( NewMipCount ) );
		STATWIN( DEC_DWORD_STAT_BY( STAT_XboxTextureMemory, Old360Size ) );
		STATWIN( INC_DWORD_STAT_BY( STAT_XboxTextureMemory, New360Size ) );
		STATWIN( TextureSize_360 = New360Size );
#endif

		return TRUE;
	}
	return FALSE;
}


FString FTexture2DResource::GetFriendlyName() const
{
	return Owner->GetPathName();
}

//Returns the raw data for a particular mip level
void* FTexture2DResource::GetRawMipData( UINT MipIndex)
{
	return MipData[MipIndex];
}

void FTexture2DArrayResource::InitRHI()
{
#if PLATFORM_SUPPORTS_D3D10_PLUS && !USE_NULL_RHI
	// Create the RHI texture.
	const DWORD TexCreateFlags = bSRGB ? TexCreate_SRGB : 0;
	FTexture2DArrayRHIRef TextureArray = RHICreateTexture2DArray(SizeX, SizeY, GetNumValidTextures(), Format, NumMips, TexCreateFlags, NULL);
	TextureRHI = TextureArray;

	// Read the mip-levels into the RHI texture.
	INT TextureIndex = 0;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TConstIterator It(CachedData); It; ++It)
	{
		const FTextureArrayDataEntry& CurrentDataEntry = It.Value();
		if (CurrentDataEntry.MipData.Num() > 0)
		{
			check(CurrentDataEntry.MipData.Num() == NumMips);
			for (INT MipIndex = 0; MipIndex < CurrentDataEntry.MipData.Num(); MipIndex++)
			{
				if (CurrentDataEntry.MipData(MipIndex).Data.Num() > 0)
				{
					UINT DestStride;
					void* TheMipData = RHILockTexture2DArray(TextureArray, TextureIndex, MipIndex, TRUE, DestStride, FALSE);
					GetData(CurrentDataEntry, MipIndex, TheMipData, DestStride);
					RHIUnlockTexture2DArray(TextureArray, TextureIndex, MipIndex, FALSE);
				}
			}
			TextureIndex++;
		}
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		Filter,
		AM_Clamp,
		AM_Clamp,
		AM_Clamp
	);
	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
#endif
}

FIncomingTextureArrayDataEntry::FIncomingTextureArrayDataEntry(UTexture2D* InTexture)
{
	// Can only access these UTexture members on the game thread
	checkSlow(IsInGameThread());

	SizeX = InTexture->SizeX;
	SizeY = InTexture->SizeY;
	NumMips = InTexture->Mips.Num();
	LODGroup = (TextureGroup)InTexture->LODGroup;
	Format = (EPixelFormat)InTexture->Format;
	Filter = GSystemSettings.TextureLODSettings.GetSamplerFilter(InTexture);
	bSRGB = InTexture->SRGB;

	MipData.Empty(InTexture->Mips.Num());
	MipData.AddZeroed(InTexture->Mips.Num());
	for (INT MipIndex = 0; MipIndex < InTexture->Mips.Num(); MipIndex++)
	{
		if (MipIndex < InTexture->Mips.Num() && InTexture->Mips(MipIndex).Data.IsAvailableForUse())
		{
			MipData(MipIndex).SizeX = InTexture->Mips(MipIndex).SizeX;
			MipData(MipIndex).SizeY = InTexture->Mips(MipIndex).SizeY;
			if (InTexture->Mips(MipIndex).Data.IsStoredInSeparateFile())
			{
				debugf( NAME_Error, TEXT("Corrupt texture [%s]! Missing bulk data for MipIndex=%d"), *InTexture->GetFullName(), MipIndex );
			}
			else			
			{
				const INT MipDataSize = InTexture->Mips(MipIndex).Data.GetElementCount() * InTexture->Mips(MipIndex).Data.GetElementSize();
				MipData(MipIndex).Data.Empty(MipDataSize);
				MipData(MipIndex).Data.Add(MipDataSize);
				// Get copy of data, potentially loading array or using already loaded version.
				void* MipDataPtr = MipData(MipIndex).Data.GetData();
				InTexture->Mips(MipIndex).Data.GetCopy(&MipDataPtr, FALSE);
			}
		}
	}
}

/** 
 * Adds a texture to the texture array.  
 * This is called on the rendering thread, so it must not dereference NewTexture.
 */
void FTexture2DArrayResource::AddTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry)
{
	UBOOL bValidTexture = FALSE;
	if (CachedData.Num() == 0)
	{
		// Copy the UTexture's parameters so that we won't need to dereference it outside of this function,
		// Since the texture may be deleted outside of this function.
		SizeX = InEntry->SizeX;
		SizeY = InEntry->SizeY;
		NumMips = InEntry->NumMips;
		LODGroup = InEntry->LODGroup;
		Format = InEntry->Format;
		Filter = InEntry->Filter;
		bSRGB = InEntry->bSRGB;

		bValidTexture = TRUE;
	}
	else if (SizeX == InEntry->SizeX
		&& SizeY == InEntry->SizeY
		&& NumMips == InEntry->NumMips
		&& LODGroup == InEntry->LODGroup
		&& Format == InEntry->Format
		&& bSRGB == InEntry->bSRGB)
	{
		bValidTexture = TRUE;
	}

	FTextureArrayDataEntry* FoundEntry = CachedData.Find(NewTexture);
	if (!FoundEntry)
	{
		// Add a new entry for this texture
		FoundEntry = &CachedData.Set(NewTexture, FTextureArrayDataEntry());
	}

	if (bValidTexture && FoundEntry->MipData.Num() == 0)
	{
		FoundEntry->MipData = InEntry->MipData;
		bDirty = TRUE;
	}
	
	FoundEntry->NumRefs++;

	delete InEntry;
}

/** Removes a texture from the texture array, and potentially removes the CachedData entry if the last ref was removed. */
void FTexture2DArrayResource::RemoveTexture2D(const UTexture2D* NewTexture)
{
	FTextureArrayDataEntry* FoundEntry = CachedData.Find(NewTexture);
	if (FoundEntry)
	{
		check(FoundEntry->NumRefs > 0);
		FoundEntry->NumRefs--;
		if (FoundEntry->NumRefs == 0)
		{
			CachedData.Remove(NewTexture);
			bDirty = TRUE;
		}
	}
}

/** Updates a CachedData entry (if one exists for this texture), with a new texture. */
void FTexture2DArrayResource::UpdateTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry)
{
	FTextureArrayDataEntry* FoundEntry = CachedData.Find(NewTexture);
	if (FoundEntry)
	{
		const INT OldNumRefs = FoundEntry->NumRefs;
		FoundEntry->MipData.Empty();
		bDirty = TRUE;
		AddTexture2D(NewTexture, InEntry);
		FoundEntry->NumRefs = OldNumRefs;
	}
}

/** Initializes the texture array resource if needed, and re-initializes if the texture array has been made dirty since the last init. */
void FTexture2DArrayResource::UpdateResource()
{
	if (bDirty)
	{
		if (IsInitialized())
		{
			ReleaseResource();
		}

		if (GetNumValidTextures() > 0)
		{
			InitResource();
		}

		bDirty = FALSE;
	}
}

/** Returns the index of a given texture in the texture array. */
INT FTexture2DArrayResource::GetTextureIndex(const UTexture2D* Texture) const
{
	INT TextureIndex = 0;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TConstIterator It(CachedData); It; ++It)
	{
		if (It.Key() == Texture && It.Value().MipData.Num() > 0)
		{
			return TextureIndex;
		}
		// Don't count invalid (empty mip data) entries toward the index
		if (It.Value().MipData.Num() > 0)
		{
			TextureIndex++;
		}
	}
	return INDEX_NONE;
}

INT FTexture2DArrayResource::GetNumValidTextures() const
{
	INT NumValidTextures = 0;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TConstIterator It(CachedData); It; ++It)
	{
		if (It.Value().MipData.Num() > 0)
		{
			NumValidTextures++;
		}
	}
	return NumValidTextures;
}

/** Prevents reallocation from removals of the texture array until EndPreventReallocation is called. */
void FTexture2DArrayResource::BeginPreventReallocation()
{
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TIterator It(CachedData); It; ++It)
	{
		FTextureArrayDataEntry& CurrentEntry = It.Value();
		CurrentEntry.NumRefs++;
	}
	bPreventingReallocation = TRUE;
}

/** Restores the ability to reallocate the texture array. */
void FTexture2DArrayResource::EndPreventReallocation()
{
	check(bPreventingReallocation);
	bPreventingReallocation = FALSE;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TIterator It(CachedData); It; ++It)
	{
		FTextureArrayDataEntry& CurrentEntry = It.Value();
		CurrentEntry.NumRefs--;
		if (CurrentEntry.NumRefs == 0)
		{
			It.RemoveCurrent();
			bDirty = TRUE;
		}
	}
}

/** Copies data from DataEntry into Dest, taking stride into account. */
void FTexture2DArrayResource::GetData(const FTextureArrayDataEntry& DataEntry, INT MipIndex, void* Dest, UINT DestPitch)
{
	check(DataEntry.MipData(MipIndex).Data.Num() > 0);

	UINT NumRows = 0;
	UINT SrcPitch = 0;
	UINT BlockSizeX = GPixelFormats[Format].BlockSizeX;	// Block width in pixels
	UINT BlockSizeY = GPixelFormats[Format].BlockSizeY;	// Block height in pixels
	UINT BlockBytes = GPixelFormats[Format].BlockBytes;
	UINT NumColumns = (DataEntry.MipData(MipIndex).SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
	NumRows = (DataEntry.MipData(MipIndex).SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
	SrcPitch = NumColumns * BlockBytes;		// Num-of bytes per row in the source data

	if (SrcPitch == DestPitch)
	{
		// Copy data, not taking into account stride!
		appMemcpy(Dest, DataEntry.MipData(MipIndex).Data.GetData(), DataEntry.MipData(MipIndex).Data.Num());
	}
	else
	{
		// Copy data, taking the stride into account!
		BYTE *Src = (BYTE*)DataEntry.MipData(MipIndex).Data.GetData();
		BYTE *Dst = (BYTE*)Dest;
		for (UINT Row = 0; Row < NumRows; ++Row)
		{
			appMemcpy(Dst, Src, SrcPitch);
			Src += SrcPitch;
			Dst += DestPitch;
		}
		check((PTRINT(Src) - PTRINT(DataEntry.MipData(MipIndex).Data.GetData())) == PTRINT(DataEntry.MipData(MipIndex).Data.Num()));
	}
}
