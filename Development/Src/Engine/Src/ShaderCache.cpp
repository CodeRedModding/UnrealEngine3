/*=============================================================================
	ShaderCache.cpp: Shader cache implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ShaderCache.h"

IMPLEMENT_CLASS(UShaderCache);

// local and reference shader cache type IDs.
enum EShaderCacheType
{
	SC_Local				= 0,
	SC_Reference			= 1,

	SC_NumShaderCacheTypes	= 2,
};

/**
 * The global caches for non-global shaders.
 * Not handled by GC and code associating objects is responsible for adding them to the root set.
 */
UShaderCache* GShaderCaches[SC_NumShaderCacheTypes][SP_NumPlatforms];

/** The global shader caches for global shaders. */
FShaderCache* GGlobalShaderCaches[SP_NumPlatforms];

UBOOL GSerializingLocalShaderCache = FALSE;

/** Array of compressed shader caches.  This can only be read from / written to on the rendering thread. */
TArray<const FCompressedShaderCodeCache*> GCompressedShaderCaches[SP_NumPlatforms];

// Pull the shader cache stats into this context
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_CompressedShaderMemory);
STAT_MAKE_AVAILABLE_FAST(STAT_ShaderCompression_UncompressedShaderMemory);

FCompressedShaderCodeCache::~FCompressedShaderCodeCache()
{
	// Update stats based on how much shader memory is being freed
	for (TMap<FShaderType*, FTypeSpecificCompressedShaderCode>::TIterator It(ShaderTypeCompressedShaderCode); It; ++It)
	{
		const FShaderType* Type = It.Key();
		if (Type)
		{
			const EMemoryStats MemoryStat = GetMemoryStatType((EShaderFrequency)Type->GetFrequency());
			const FTypeSpecificCompressedShaderCode& CurrentTypeCompressedCode = It.Value();
			for (INT ChunkIndex = 0; ChunkIndex < CurrentTypeCompressedCode.CodeChunks.Num(); ChunkIndex++)
			{
				const FCompressedShaderCodeChunk& CurrentChunk = CurrentTypeCompressedCode.CodeChunks(ChunkIndex);
				DEC_DWORD_STAT_BY(MemoryStat, CurrentChunk.CompressedCode.Num());
				DEC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_CompressedShaderMemory, CurrentChunk.CompressedCode.Num());
			}
		}
	}
}

/** Compresses the compiled bytecode from the input shaders and writes the output to ShaderTypeCompressedShaderCode. */
void FCompressedShaderCodeCache::CompressShaderCode(const TMap<FGuid,FShader*>& Shaders, EShaderPlatform Platform)
{
	ShaderTypeCompressedShaderCode.Empty();
	const UINT CompressedShaderChunkSizeTarget = GetCompressedShaderChunkSizeTarget(Platform);
	TMap<FShaderType*, TArray<TArray<BYTE> > > UncompressedShaderCode;

	for (TMap<FGuid,FShader*>::TConstIterator ShaderIt(Shaders); ShaderIt; ++ShaderIt)
	{
		FShader* CurrentShader = ShaderIt.Value();
		FShaderType* Type = CurrentShader->GetType(); 
		TArray<TArray<BYTE> >* CurrentTypeUncompressedCode = UncompressedShaderCode.Find(Type);
		FTypeSpecificCompressedShaderCode* CurrentTypeCompressedCode = ShaderTypeCompressedShaderCode.Find(Type);

		// If no entry for this shader's type exists, add one
		if (!CurrentTypeUncompressedCode)
		{
			check(!CurrentTypeCompressedCode);
			TArray<TArray<BYTE> > NewUncompressedCode;
			NewUncompressedCode.AddItem(TArray<BYTE>());
			CurrentTypeUncompressedCode = &UncompressedShaderCode.Set(Type, NewUncompressedCode);
			FTypeSpecificCompressedShaderCode NewCompressedCode;
			NewCompressedCode.CodeChunks.AddItem(FCompressedShaderCodeChunk());
			CurrentTypeCompressedCode = &ShaderTypeCompressedShaderCode.Set(Type, NewCompressedCode);
		}

		check(CurrentTypeUncompressedCode && CurrentTypeCompressedCode);
		check(CurrentTypeUncompressedCode->Num() > 0 && CurrentTypeCompressedCode->CodeChunks.Num() > 0);
		check(CurrentShader->GetCode().Num() > 0);
		TArray<BYTE>* CurrentUncompressedCodeChunk = &CurrentTypeUncompressedCode->Last();

		// If the code chunk's size exceeds the size target, create a new chunk
		if (CurrentUncompressedCodeChunk->Num() > 0 
			&& (UINT)(CurrentUncompressedCodeChunk->Num() + CurrentShader->GetCode().Num()) > CompressedShaderChunkSizeTarget)
		{
			CurrentTypeUncompressedCode->AddItem(TArray<BYTE>());
			CurrentUncompressedCodeChunk = &CurrentTypeUncompressedCode->Last();
			CurrentTypeCompressedCode->CodeChunks.AddItem(FCompressedShaderCodeChunk());
		}

		FCompressedShaderCodeChunk& CurrentCompressedCodeChunk = CurrentTypeCompressedCode->CodeChunks.Last();
		// Setup the information needed to decompress the current shader
		CurrentTypeCompressedCode->CompressedShaderInfos.Set(CurrentShader->GetId(), 
			FIndividualCompressedShaderInfo(CurrentTypeCompressedCode->CodeChunks.Num() - 1, CurrentShader->GetCode().Num(), CurrentUncompressedCodeChunk->Num()));
		// Append the current shader's bytecode to the current chunk, which only contains shaders of the same type to get a good compression ratio, 
		// Since shaders of the same type are very similar.
		CurrentUncompressedCodeChunk->Append(CurrentShader->GetCode());
		//@todo - use the max of the compressor being used
		check(CurrentUncompressedCodeChunk->Num() < MaxUncompressedSize);
	}

	const ECompressionFlags ShaderCompressionFlags = GetShaderCompressionFlags(Platform);
	for (TMap<FShaderType*, FTypeSpecificCompressedShaderCode>::TIterator It(ShaderTypeCompressedShaderCode); It; ++It)
	{
		const FShaderType* Type = It.Key();
		FTypeSpecificCompressedShaderCode& CurrentTypeCompressedCode = It.Value();
		TArray<TArray<BYTE> >* CurrentTypeUncompressedCodePtr = UncompressedShaderCode.Find(Type);
		check(CurrentTypeUncompressedCodePtr);
		check(CurrentTypeCompressedCode.CodeChunks.Num() == CurrentTypeUncompressedCodePtr->Num());

		for (INT ChunkIndex = 0; ChunkIndex < CurrentTypeCompressedCode.CodeChunks.Num(); ChunkIndex++)
		{
			TArray<BYTE>& UncompressedChunk = (*CurrentTypeUncompressedCodePtr)(ChunkIndex);
			check(UncompressedChunk.Num() > 0);
			TArray<BYTE> TempCompressionOutput;
			// Compressed output can be larger than the input, so we use temporary storage to hold the compressed output for now
			TempCompressionOutput.Empty(UncompressedChunk.Num() * 4 / 3);
			TempCompressionOutput.Add(UncompressedChunk.Num() * 4 / 3);
			INT CompressedSize = TempCompressionOutput.Num();

			// Compress the code chunk, CompressedSize will contain the compressed code size
			verify(appCompressMemory(
				ShaderCompressionFlags, 
				TempCompressionOutput.GetData(), 
				CompressedSize, 
				UncompressedChunk.GetData(), 
				UncompressedChunk.Num()));

			FCompressedShaderCodeChunk& CompressedChunk = CurrentTypeCompressedCode.CodeChunks(ChunkIndex);
			CompressedChunk.UncompressedSize = UncompressedChunk.Num();
			// Copy to the compressed code chunk now that we know the actual compressed size
			CompressedChunk.CompressedCode.Empty(CompressedSize);
			CompressedChunk.CompressedCode.Add(CompressedSize);
			appMemcpy(CompressedChunk.CompressedCode.GetData(), TempCompressionOutput.GetData(), CompressedSize);

			// Enable to verify that decompressed results match the original, which is useful for debugging compressor bugs
			const UBOOL bTestDecompression = FALSE;
			if (bTestDecompression)
			{
				TArray<BYTE> TempUncompressed;
				TempUncompressed.Empty(UncompressedChunk.Num());
				TempUncompressed.Add(UncompressedChunk.Num());
				verify(appUncompressMemory(
					ShaderCompressionFlags,
					TempUncompressed.GetData(),
					TempUncompressed.Num(),
					CompressedChunk.CompressedCode.GetData(),
					CompressedChunk.CompressedCode.Num()));

				check(TempUncompressed == UncompressedChunk);
			}
		}
	}
}

/** Decompresses the given shader's code from this compressed cache if possible, and stores the result in UncompressedCode. */
UBOOL FCompressedShaderCodeCache::DecompressShaderCode(const FShader* Shader, const FGuid& Id, EShaderPlatform Platform, TArray<BYTE>& UncompressedCode) const
{
	// Find the compressed code corresponding to this type.  This will be NULL if the shader cache does not contain any shaders of the given type.
	const FTypeSpecificCompressedShaderCode* CompressedShaderCode = ShaderTypeCompressedShaderCode.Find(Shader->GetType());
	if (CompressedShaderCode)
	{
		// Find the information needed to decompress the given shader.  This will be NULL if the shader cache does not contain compressed code for the shader.
		const FIndividualCompressedShaderInfo* ShaderInfoPtr = CompressedShaderCode->CompressedShaderInfos.Find(Id);
		if (ShaderInfoPtr)
		{
			const ECompressionFlags ShaderCompressionFlags = GetShaderCompressionFlags(Platform);
			const FCompressedShaderCodeChunk& CompressedChunk = CompressedShaderCode->CodeChunks(ShaderInfoPtr->ChunkIndex);
			TArray<BYTE> TempUncompressedChunk;
			TempUncompressedChunk.Empty(CompressedChunk.UncompressedSize);
			TempUncompressedChunk.Add(CompressedChunk.UncompressedSize);

			// Decompress the entire chunk, even though we only need to access one shader's code
			verify(appUncompressMemory(
				ShaderCompressionFlags, 
				TempUncompressedChunk.GetData(), 
				CompressedChunk.UncompressedSize, 
				CompressedChunk.CompressedCode.GetData(), 
				CompressedChunk.CompressedCode.Num()));

			// Copy the shader's code from the chunk into UncompressedCode.
			UncompressedCode.Empty(ShaderInfoPtr->UncompressedCodeLength);
			UncompressedCode.Add(ShaderInfoPtr->UncompressedCodeLength);
			appMemcpy(UncompressedCode.GetData(), &TempUncompressedChunk(ShaderInfoPtr->UncompressedCodeOffset), ShaderInfoPtr->UncompressedCodeLength);
			return TRUE;
		}
	}
	return FALSE;
}

/** Returns TRUE if the compressed shader cache contains compressed shader code for the given shader. */
UBOOL FCompressedShaderCodeCache::HasShader(const FShader* Shader) const
{
	const FTypeSpecificCompressedShaderCode* CompressedShaderCode = ShaderTypeCompressedShaderCode.Find(Shader->GetType());
	if (CompressedShaderCode)
	{
		const FIndividualCompressedShaderInfo* ShaderInfoPtr = CompressedShaderCode->CompressedShaderInfos.Find(Shader->GetId());
		if (ShaderInfoPtr)
		{
			return TRUE;
		}
	}
	return FALSE;
}

void FCompressedShaderCodeCache::AddRef()
{
	++NumRefs;
}

void FCompressedShaderCodeCache::Release()
{
	if(--NumRefs == 0)
	{
		// The last reference has been released, so enqueue a command to remove the compressed shader cache from the array used by the rendering thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FRemoveCompressedShaderCache,
			EShaderPlatform,Platform,Platform,
			const FCompressedShaderCodeCache*,CompressedCache,this,
		{
			GCompressedShaderCaches[Platform].RemoveItem(CompressedCache);
		});
	
		// We can't delete the cache at this point because the above rendering command may still be waiting to be processed by the rendering thread
		// Enqueue the deferred deletion of this cache to happen when it is safe
		BeginCleanup(this);
	}
}

/** Called by the deferred cleanup mechanism when it is safe to delete this object. */
void FCompressedShaderCodeCache::FinishCleanup()
{
	delete this;
}

INT FCompressedShaderCodeCache::GetCompressedCodeSize() const
{
	INT CodeSize = 0;
	// Update shader memory stats based on how much compressed shader code we just loaded
	for (TMap<FShaderType*, FTypeSpecificCompressedShaderCode>::TConstIterator It(ShaderTypeCompressedShaderCode); It; ++It)
	{
		const FShaderType* Type = It.Key();
		if (Type)
		{
			const FTypeSpecificCompressedShaderCode& CurrentTypeCompressedCode = It.Value();
			for (INT ChunkIndex = 0; ChunkIndex < CurrentTypeCompressedCode.CodeChunks.Num(); ChunkIndex++)
			{
				const FCompressedShaderCodeChunk& CurrentChunk = CurrentTypeCompressedCode.CodeChunks(ChunkIndex);
				CodeSize += CurrentChunk.GetCompressedCodeSize();				
			}
		}
	}
	return CodeSize;
}

FArchive& operator<<(FArchive& Ar, FCompressedShaderCodeCache& Ref)
{
	Ar << Ref.ShaderTypeCompressedShaderCode;
	if (Ar.IsLoading())
	{
		// Update shader memory stats based on how much compressed shader code we just loaded
		for (TMap<FShaderType*, FTypeSpecificCompressedShaderCode>::TIterator It(Ref.ShaderTypeCompressedShaderCode); It; ++It)
		{
			const FShaderType* Type = It.Key();
			if (Type)
			{
				const EMemoryStats MemoryStat = GetMemoryStatType((EShaderFrequency)Type->GetFrequency());
				const FTypeSpecificCompressedShaderCode& CurrentTypeCompressedCode = It.Value();
				for (INT ChunkIndex = 0; ChunkIndex < CurrentTypeCompressedCode.CodeChunks.Num(); ChunkIndex++)
				{
					const FCompressedShaderCodeChunk& CurrentChunk = CurrentTypeCompressedCode.CodeChunks(ChunkIndex);
					INC_DWORD_STAT_BY(MemoryStat, CurrentChunk.GetCompressedCodeSize());
					INC_DWORD_STAT_BY_FAST(STAT_ShaderCompression_CompressedShaderMemory, CurrentChunk.GetCompressedCodeSize());
				}
			}
		}
	}
	return Ar;
}

/** Loads the shader cache. */
void FShaderCache::Load(FArchive& Ar)
{
	if(Ar.Ver() >= VER_GLOBAL_SHADER_FILE)
	{
		Ar << Platform;
		// SP_PCD3D_SM4 was 4 before VER_REMOVED_SHADER_MODEL_2
		if (Ar.Ver() < VER_REMOVED_SHADER_MODEL_2 && Platform == 4)
		{
			Platform = SP_PCD3D_SM4;
		}
		if (Ar.Ver() < VER_FIXED_AUTO_SHADER_VERSIONING)
		{
			TMap<FShaderType*, DWORD> Dummy;
			Ar << Dummy;
		}
	}

	if (Ar.Ver() >= VER_SHADER_COMPRESSION)
	{
		if (GRHIShaderPlatform == (EShaderPlatform)Platform && UseShaderCompression((EShaderPlatform)Platform))
		{
			CompressedCache = new FCompressedShaderCodeCache((EShaderPlatform)Platform);
			Ar << *CompressedCache;
			if (!CompressedCache->IsEmpty())
			{
				// Add the compressed shader cache to the array used by the rendering thread
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					FSetCompressedShaderCache,
					EShaderPlatform,Platform,(EShaderPlatform)Platform,
					const FCompressedShaderCodeCache*,CompressedCache,CompressedCache,
				{
					GCompressedShaderCaches[Platform].AddItem(CompressedCache);
				});
			}
		}
		else
		{
			FCompressedShaderCodeCache DummyCache((EShaderPlatform)Platform);
			Ar << DummyCache;
		}
	}

	check(Ar.IsLoading());
	TMap<FGuid,FShader*> TempShaders;
	SerializeShaders(TempShaders, Ar);
}

/** Saves the shader cache. */
void FShaderCache::Save(FArchive& Ar,const TMap<FGuid,FShader*>& Shaders,UBOOL bSavingCookedPackage)
{
	Ar << Platform;

	FCompressedShaderCodeCache SavingCompressedCache((EShaderPlatform)Platform);

	// Compress the shader cache if we are cooking for a platform which won't ever save shaders,
	if (GIsCooking && (GCookingTarget & UE3::PLATFORM_Console)
		// And shader compression is enabled for that platform,
		&& UseShaderCompression((EShaderPlatform)Platform)
		// And we're saving a shader cache in a cooked package (don't want to compress for Local/Ref shader caches),
		&& bSavingCookedPackage
		// And this is the final save call during cooking, whose results are actually written to disk.
		&& Ar.IsFinalPackageSave())
	{
		SavingCompressedCache.CompressShaderCode(Shaders, (EShaderPlatform)Platform);
	}

	Ar << SavingCompressedCache;

	check(Ar.IsSaving());
	SerializeShaders(Shaders, Ar);
}

/**
 * Constructor.
 *
 * @param	InPlatform	Platform this shader cache is for.
 */
UShaderCache::UShaderCache( EShaderPlatform InPlatform )
:	FShaderCache(InPlatform)
,	bDirty(FALSE)
{}

/**
 * Flushes the shader map for a material from the cache.
 * @param StaticParameters - The static parameter set identifying the material
 * @param Platform Platform to flush.
 */
void UShaderCache::FlushId(const FStaticParameterSet& StaticParameters, EShaderPlatform Platform)
{
	UShaderCache* ShaderCache = GShaderCaches[SC_Local][Platform];
	if (ShaderCache)
	{
		// Remove the shaders cached for this material ID from the shader cache.
		ShaderCache->MaterialShaderMap.Remove(StaticParameters);
		// make sure the reference in the map is removed
		ShaderCache->MaterialShaderMap.Shrink();
		ShaderCache->bDirty = TRUE;
	}
}

/**
* Flushes the shader map for a material from all caches.
* @param StaticParameters - The static parameter set identifying the material
*/
void UShaderCache::FlushId(const class FStaticParameterSet& StaticParameters)
{
	for( INT PlatformIndex=0; PlatformIndex<SP_NumPlatforms; PlatformIndex++ )
	{
		UShaderCache::FlushId( StaticParameters, (EShaderPlatform)PlatformIndex );
	}
}

/**
 * Combines OtherCache's shaders with this one.  
 * OtherCache has priority and will overwrite any existing shaders, otherwise they will just be added.
 *
 * @param OtherCache	Shader cache to merge
 */
void UShaderCache::Merge(UShaderCache* OtherCache)
{
	check(OtherCache && Platform == OtherCache->Platform);

	//copy over all the material shader maps, overwriting existing ones with incoming ones when necessary
	for( TMap<FStaticParameterSet,TRefCountPtr<FMaterialShaderMap> >::TIterator MaterialIt(OtherCache->MaterialShaderMap); MaterialIt; ++MaterialIt )
	{
		TRefCountPtr<FMaterialShaderMap>& CurrentMaterialShaderMap = MaterialIt.Value();
		check(CurrentMaterialShaderMap->GetMaterialId() == MaterialIt.Key());
		AddMaterialShaderMap(CurrentMaterialShaderMap);
	}
}

/**
 * Adds a material shader map to the cache fragment.
 */
void UShaderCache::AddMaterialShaderMap(FMaterialShaderMap* InMaterialShaderMap)
{
	MaterialShaderMap.Set(InMaterialShaderMap->GetMaterialId(),InMaterialShaderMap);
	bDirty = TRUE;
}

void UShaderCache::FinishDestroy()
{
	for( INT TypeIndex=0; TypeIndex<SC_NumShaderCacheTypes; TypeIndex++ )
	{
		for( INT PlatformIndex=0; PlatformIndex<SP_NumPlatforms; PlatformIndex++ )
		{
			if( GShaderCaches[TypeIndex][PlatformIndex] == this )
			{
				// The shader cache is a root object, but it will still be purged on exit.  Make sure there isn't a dangling reference to it.
				GShaderCaches[TypeIndex][PlatformIndex] = NULL;
			}
		}
	}

	Super::FinishDestroy();
}

IMPLEMENT_COMPARE_CONSTREF(FStaticParameterSet,SortMaterialsByStaticParamSet,
{
	for ( INT i = 0; i < 4; i++ )
	{
		if ( A.BaseMaterialId[i] > B.BaseMaterialId[i] )
		{
			return 1;
		}
		else if ( A.BaseMaterialId[i] < B.BaseMaterialId[i] )
		{
			return -1;
		}
	}

	if (A.StaticSwitchParameters.Num() > B.StaticSwitchParameters.Num())
	{
		return 1;
	}
	else if (A.StaticSwitchParameters.Num() < B.StaticSwitchParameters.Num())
	{
		return -1;
	}

	for (INT SwitchIndex = 0; SwitchIndex < A.StaticSwitchParameters.Num(); SwitchIndex++)
	{
		const FStaticSwitchParameter &SwitchA = A.StaticSwitchParameters(SwitchIndex);
		const FStaticSwitchParameter &SwitchB = B.StaticSwitchParameters(SwitchIndex);

		if (SwitchA.ParameterName.ToString() != SwitchB.ParameterName.ToString())
		{
			return (SwitchA.ParameterName.ToString() > SwitchB.ParameterName.ToString()) * 2 - 1;
		} 

		if (SwitchA.Value != SwitchB.Value) { return (SwitchA.Value > SwitchB.Value) * 2 - 1; } 
	}

	if (A.StaticComponentMaskParameters.Num() > B.StaticComponentMaskParameters.Num())
	{
		return 1;
	}
	else if (A.StaticComponentMaskParameters.Num() < B.StaticComponentMaskParameters.Num())
	{
		return -1;
	}

	for (INT MaskIndex = 0; MaskIndex < A.StaticComponentMaskParameters.Num(); MaskIndex++)
	{
		const FStaticComponentMaskParameter &MaskA = A.StaticComponentMaskParameters(MaskIndex);
		const FStaticComponentMaskParameter &MaskB = B.StaticComponentMaskParameters(MaskIndex);

		if (MaskA.ParameterName.ToString() != MaskB.ParameterName.ToString())
		{
			return (MaskA.ParameterName.ToString() > MaskB.ParameterName.ToString()) * 2 - 1;
		} 

		if (MaskA.R != MaskB.R) { return (MaskA.R > MaskB.R) * 2 - 1; } 
		if (MaskA.G != MaskB.G) { return (MaskA.G > MaskB.G) * 2 - 1; } 
		if (MaskA.B != MaskB.B) { return (MaskA.B > MaskB.B) * 2 - 1; } 
		if (MaskA.A != MaskB.A) { return (MaskA.A > MaskB.A) * 2 - 1; } 
	}

	if (A.NormalParameters.Num() > B.NormalParameters.Num())
	{
		return 1;
	}
	else if (A.NormalParameters.Num() < B.NormalParameters.Num())
	{
		return -1;
	}

	for (INT MaskIndex = 0; MaskIndex < A.NormalParameters.Num(); MaskIndex++)
	{
		const FNormalParameter &NormalA = A.NormalParameters(MaskIndex);
		const FNormalParameter &NormalB = B.NormalParameters(MaskIndex);

		if (NormalA.ParameterName.ToString() != NormalB.ParameterName.ToString())
		{
			return (NormalA.ParameterName.ToString() > NormalB.ParameterName.ToString()) * 2 - 1;
		} 

		if (NormalA.CompressionSettings != NormalB.CompressionSettings) { return (NormalA.CompressionSettings > NormalB.CompressionSettings) * 2 - 1; } 
	}

	if (A.TerrainLayerWeightParameters.Num() > B.TerrainLayerWeightParameters.Num())
	{
		return 1;
	}
	else if (A.TerrainLayerWeightParameters.Num() < B.TerrainLayerWeightParameters.Num())
	{
		return -1;
	}

	for (INT MaskIndex = 0; MaskIndex < A.TerrainLayerWeightParameters.Num(); MaskIndex++)
	{
		const FStaticTerrainLayerWeightParameter &TerrainLayerWeightA = A.TerrainLayerWeightParameters(MaskIndex);
		const FStaticTerrainLayerWeightParameter &TerrainLayerWeightB = B.TerrainLayerWeightParameters(MaskIndex);

		if (TerrainLayerWeightA.ParameterName.ToString() != TerrainLayerWeightB.ParameterName.ToString())
		{
			return (TerrainLayerWeightA.ParameterName.ToString() > TerrainLayerWeightB.ParameterName.ToString()) * 2 - 1;
		} 

		if (TerrainLayerWeightA.WeightmapIndex != TerrainLayerWeightB.WeightmapIndex) { return (TerrainLayerWeightA.WeightmapIndex > TerrainLayerWeightB.WeightmapIndex) * 2 - 1; } 
	}

	return 0;
})

/**
 * Returns a list of all material shader maps residing in this shader cache.	 
 */
TArray<TRefCountPtr<FMaterialShaderMap> > UShaderCache::GetMaterialShaderMap() const
{
	TArray<TRefCountPtr<FMaterialShaderMap> > MaterialShaderMapArray;
	for( TMap<FStaticParameterSet,TRefCountPtr<FMaterialShaderMap> >::TConstIterator It(MaterialShaderMap); It; ++It )
	{
		MaterialShaderMapArray.AddItem( It.Value() );
	}
	return MaterialShaderMapArray;
}

/**
 *	Removes all entries other than the given ones
 *
 *	@param	InValidList		Array of valid FStaticParameterSet IDs (the list of IDs for materials that were saved)
 */
INT UShaderCache::CleanupCacheEntries(TArray<FStaticParameterSet>& InMaterialIDList)
{
	INT RemovedCount = 0;

	// Iterate over all the entries in the material shader map, taking note of 
	// which entries are not found in the given ID list.
	for (TMap<FStaticParameterSet,TRefCountPtr<class FMaterialShaderMap> >::TIterator MapIt(MaterialShaderMap); MapIt; ++MapIt)
	{
		FStaticParameterSet& ParamSet = MapIt.Key();
		TRefCountPtr<class FMaterialShaderMap>& MapEntry = MapIt.Value();
		check(ParamSet == MapEntry->GetMaterialId());
		INT FoundIndex = InMaterialIDList.FindItemIndex(ParamSet);
		if (FoundIndex == INDEX_NONE)
		{
			// Remove this entry...
			MaterialShaderMap.Remove(ParamSet);
			RemovedCount++;
		}
	}

	if (RemovedCount > 0)
	{
		MaterialShaderMap.Compact();
	}

	return RemovedCount;
}

/**
 * Presave function. Gets called once before an object gets serialized for saving.  Sorts the MaterialShaderMap
 * maps so that shader cache serialization is deterministic.
 */
void UShaderCache::PreSave()
{
	Super::PreSave();
#if WITH_EDITORONLY_DATA
	MaterialShaderMap.KeySort<COMPARE_CONSTREF_CLASS(FStaticParameterSet,SortMaterialsByStaticParamSet)>();
#endif // WITH_EDITORONLY_DATA
}

void UShaderCache::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.IsSaving())
	{
		Save(Ar);

		// Mark the cache as not dirty.
		bDirty = FALSE;
	}
	else if(Ar.IsLoading())
	{
		Load(Ar, (this->GetFlags() & RF_DisregardForGC) != 0);
	}

	if( Ar.IsCountingMemory() )
	{
		MaterialShaderMap.CountBytes( Ar );
		for( TMap<FStaticParameterSet,TRefCountPtr<class FMaterialShaderMap> >::TIterator It(MaterialShaderMap); It; ++It )
		{
			It.Value()->Serialize( Ar );
		}
	}
}

/** Keeps track of accumulative time spent in UShaderCache::Load. */
DOUBLE GShaderCacheLoadTime = 0;

void UShaderCache::Load(FArchive& Ar, UBOOL bIsAlwaysLoaded)
{
	DOUBLE StartTime = appSeconds();
	SCOPE_SECONDS_COUNTER(GShaderCacheLoadTime);

	if (Ar.Ver() < VER_SHADER_CACHE_PRIORITY)
	{
		ShaderCachePriority = 0;
	}
	else
	{
		Ar << ShaderCachePriority;
	}


	if(Ar.Ver() < VER_GLOBAL_SHADER_FILE)
	{
		Ar << Platform;
		TMap<FShaderType*, DWORD> Dummy;
		Ar << Dummy;
		TMap<FVertexFactoryType*, DWORD> Dummy2;
		Ar << Dummy2;
	}

	// Serialize the shaders.
	FShaderCache::Load(Ar);

	if (CompressedCache)
	{
		CompressedCache->CacheName = GetPathName();
		CompressedCache->ShaderCachePriority = ShaderCachePriority;
		CompressedCache->bIsAlwaysLoaded = bIsAlwaysLoaded;
	}

	if(Ar.Ver() >= VER_GLOBAL_SHADER_FILE && Ar.Ver() < VER_FIXED_AUTO_SHADER_VERSIONING)
	{
		TMap<FVertexFactoryType*, DWORD> Dummy;
		Ar << Dummy;
	}

	TArray<FString> OutdatedVFTypes;
	INT NumMaterialShaderMaps = 0;
	INT NumRedundantMaterialShaderMaps = 0;
	Ar << NumMaterialShaderMaps;

	// One cannot delete these right away, as they are used in a many-to-one relationship on load
	TArray<FMaterialShaderMap*> DeferredDeleteMaterialShaderMaps;

	// Load the material shader indices in the cache.
	for( INT MaterialIndex=0; MaterialIndex<NumMaterialShaderMaps; MaterialIndex++ )
	{
		FStaticParameterSet StaticParameters;
		StaticParameters.Serialize(Ar);

		INT ShaderMapVersion = 0;
		INT ShaderMapLicenseeVersion = 0;
		if (Ar.Ver() >= VER_UNIFORMEXPRESSION_TEXTUREINDEX)
		{
			Ar << ShaderMapVersion << ShaderMapLicenseeVersion;
		}

		// Deserialize the offset of the next material.
		INT SkipOffset = 0;
		Ar << SkipOffset;

		FMaterialShaderMap* ExistingMaterialShaderIndex = FMaterialShaderMap::FindId(StaticParameters,(EShaderPlatform)Platform);

		if (ShaderMapVersion < VER_MIN_MATERIALSHADERMAP || ShaderMapLicenseeVersion < LICENSEE_VER_MIN_MATERIALSHADERMAP)
		{
			++NumRedundantMaterialShaderMaps;
			Ar.Seek(SkipOffset);
		}
#define MERGE_COMPRESSED_SHADERS (1)
		else if (MERGE_COMPRESSED_SHADERS && CONSOLE && 
			ExistingMaterialShaderIndex &&
			ExistingMaterialShaderIndex->CompressedCache &&
			ExistingMaterialShaderIndex->CompressedCache->bIsAlwaysLoaded == FALSE &&
			ExistingMaterialShaderIndex->CompressedCache->ShaderCachePriority >= CompressedCache->ShaderCachePriority
			)
		{
			++NumRedundantMaterialShaderMaps;

			// Deserialize the material shader map.
			FMaterialShaderMap* MaterialShaderIndex = new FMaterialShaderMap();
			MaterialShaderIndex->Serialize(Ar);

			// One cannot delete these right away, as they are used in a many-to-one relationship on load
			DeferredDeleteMaterialShaderMaps.AddItem(MaterialShaderIndex);

			// Add a reference to the shader map from the cache. This ensures that the shader map isn't deleted between 
			// the cache being deserialized and PostLoad being called on materials in the same package.
			MaterialShaderMap.Set(StaticParameters,ExistingMaterialShaderIndex);

			// Determine if all of the shaders in this shader map have their compressed code in CompressedCache
			// This can return FALSE if a shader in ExistingMaterialShaderIndex has been recompiled before getting stored in the current shader cache,
			// But the package containing ExistingMaterialShaderIndex was not recooked.
			if (ExistingMaterialShaderIndex->AddGuidAliases(MaterialShaderIndex))
			{
				// Switch the existing shader map to referencing the new compressed shader cache
				// This way when shader maps are shared between multiple streaming levels, only the most recent compressed shader caches will be kept
				ExistingMaterialShaderIndex->CompressedCache = CompressedCache;
			}
			else
			{
				warnf( 
					TEXT("Compressed shader cache %s didn't contain code for material %s!  Can't reuse this cache, shader memory will be higher until a full recook."),
					*CompressedCache->CacheName,
					*MaterialShaderIndex->GetFriendlyName());
			}
		}
		else if (ExistingMaterialShaderIndex && CONSOLE)
		{
			++NumRedundantMaterialShaderMaps;
			
			// If a shader map with the same ID is already resident, skip this one.
			// On PC we want to still deserialize the shader map in case it is more complete than the existing one.
			Ar.Seek(SkipOffset);

			// Add a reference to the shader map from the cache. This ensures that the shader map isn't deleted between 
			// the cache being deserialized and PostLoad being called on materials in the same package.
			MaterialShaderMap.Set(StaticParameters,ExistingMaterialShaderIndex);

			// @todo, code in this if can disappear if the MERGE_COMPRESSED_SHADERS is always on
			if (ExistingMaterialShaderIndex->CompressedCache &&
				ExistingMaterialShaderIndex->CompressedCache->bIsAlwaysLoaded == FALSE)
			{

				INT PreviousShaderCachePriority = ExistingMaterialShaderIndex->CompressedCache->ShaderCachePriority;
				if (PreviousShaderCachePriority >= CompressedCache->ShaderCachePriority)
				{
					// Determine if all of the shaders in this shader map have their compressed code in CompressedCache
					// This can return FALSE if a shader in ExistingMaterialShaderIndex has been recompiled before getting stored in the current shader cache,
					// But the package containing ExistingMaterialShaderIndex was not recooked.
					if (ExistingMaterialShaderIndex->IsCompressedShaderCacheComplete(CompressedCache))
					{
						// Switch the existing shader map to referencing the new compressed shader cache
						// This way when shader maps are shared between multiple streaming levels, only the most recent compressed shader caches will be kept
						ExistingMaterialShaderIndex->CompressedCache = CompressedCache;
					}
				}
			}
		}
		else
		{
			// Deserialize the material shader map.
			FMaterialShaderMap* MaterialShaderIndex = new FMaterialShaderMap();
			MaterialShaderIndex->Serialize(Ar);

			if(ExistingMaterialShaderIndex)
			{
				// merge the new material shader map into the existing one
				ExistingMaterialShaderIndex->Merge(MaterialShaderIndex);
				// One cannot delete these right away, as they are used in a many-to-one relationship on load
				DeferredDeleteMaterialShaderMaps.AddItem(MaterialShaderIndex);
			}
			else
			{
				if (MaterialShaderIndex->IsUniformExpressionSetValid())
				{
					// Verify that all of the shaders in this shader map have their compressed code in CompressedCache
					check(MaterialShaderIndex->IsCompressedShaderCacheComplete(CompressedCache));

					// Setup the material shader map to reference the compressed cache that it needs
					MaterialShaderIndex->CompressedCache = CompressedCache;
					MaterialShaderIndex->Register();
					ExistingMaterialShaderIndex = MaterialShaderIndex;
				}
				else
				{
					warnf(TEXT("Shader map %s had an invalid uniform expression set and was discarded!  This most likely indicates a bug in cooking, and the default material will be used instead."), 
						*MaterialShaderIndex->GetFriendlyName());
				}
			}

			if (ExistingMaterialShaderIndex)
			{
				// Add a reference to the shader map from the cache. This ensures that the shader map isn't deleted between 
				// the cache being deserialized and PostLoad being called on materials in the same package.
				MaterialShaderMap.Set(StaticParameters,ExistingMaterialShaderIndex);
			}
		}
	}
	// One cannot delete these right away, as they are used in a many-to-one relationship on load
	for (INT Index = 0; Index < DeferredDeleteMaterialShaderMaps.Num() ; Index++ )
	{
		delete DeferredDeleteMaterialShaderMaps(Index);
	}
	DeferredDeleteMaterialShaderMaps.Empty();

	if(Ar.Ver() < VER_GLOBAL_SHADER_FILE)
	{
		// Ignore legacy global shader maps saved in package shader caches.
		TShaderMap<FGlobalShaderType>* NewGlobalShaderMap = new TShaderMap<FGlobalShaderType>();
		NewGlobalShaderMap->Serialize(Ar);
	}

	// Log which types were skipped/flushed because their source files have changed
	// Note that these aren't always accurate, for example if the local shader cache is older 
	// than the ref then these are expected to be out of date.
	if (ShouldReloadChangedShaders() && GSerializingLocalShaderCache)
	{
		INT NumOutdatedVFTypes = OutdatedVFTypes.Num();
		if (NumOutdatedVFTypes > 0)
		{
			warnf(NAME_DevShaders, TEXT("Skipped %i outdated FVertexFactoryTypes:"), NumOutdatedVFTypes);
			for (INT TypeIndex = 0; TypeIndex < NumOutdatedVFTypes; TypeIndex++)
			{	
				warnf(NAME_DevShaders, TEXT("	%s"), *OutdatedVFTypes(TypeIndex));
			}
		}
	}
	
	// Log some cache stats.
	if( NumMaterialShaderMaps )
	{
		debugf(NAME_DevShaders,TEXT("Shader cache for %s contains %u materials (%u redundant)"),*GetOutermost()->GetName(),NumMaterialShaderMaps,NumRedundantMaterialShaderMaps);
	}
	// diagnose a performance problem with MT cooking
	if (appStristr(appCmdLine(), TEXT("CookPackages")) != NULL)
	{
		debugf(TEXT("Loading Shader Cache %s in %fs"),*Ar.GetLinker()->Filename,FLOAT(appSeconds() - StartTime));
	}
}

void UShaderCache::Save(FArchive& Ar)
{
	Ar << ShaderCachePriority;

	// Make sure deferred shader compiling is completed before saving shader caches
	GShaderCompilingThreadManager->FinishDeferredCompilation();
	// the reference shader should never be saved (when it's being created, it's actually the local shader cache for that run of the commandlet)
	// check(this != GShaderCaches[SC_Reference][Platform]);

	// Find the shaders used by materials in the cache.
	TMap<FGuid,FShader*> Shaders;

	for( TMap<FStaticParameterSet,TRefCountPtr<FMaterialShaderMap> >::TIterator MaterialIt(MaterialShaderMap); MaterialIt; ++MaterialIt )
	{
		const TRefCountPtr<FMaterialShaderMap>& ShaderMap = MaterialIt.Value();
		ShaderMap->GetShaderList(Shaders);
	}

	// Serialize the shaders.
	FShaderCache::Save(Ar,Shaders,RootPackageHasAnyFlags(PKG_Cooked));

	// Save the material shader indices in the cache.
	INT NumMaterialShaderMaps = MaterialShaderMap.Num();
	Ar << NumMaterialShaderMaps;
	for( TMap<FStaticParameterSet,TRefCountPtr<FMaterialShaderMap> >::TIterator MaterialIt(MaterialShaderMap); MaterialIt; ++MaterialIt )
	{
		// Serialize the material static parameter set separate, so at load time we can see whether this is a redundant material shader map without fully
		// deserializing it.
		FStaticParameterSet StaticParameters = MaterialIt.Key();
		StaticParameters.Serialize(Ar);

		INT ShaderMapVersion = Ar.Ver();
		INT ShaderMapLicenseeVersion = Ar.LicenseeVer();
		Ar << ShaderMapVersion << ShaderMapLicenseeVersion;

		// Write a placeholder value for the skip offset.
		INT SkipOffset = Ar.Tell();
		Ar << SkipOffset;

		// Serialize the material shader map.
		MaterialIt.Value()->Serialize(Ar);

		// Write the actual offset of the end of the material shader map data over the placeholder value written above.
		INT EndOffset = Ar.Tell();
		Ar.Seek(SkipOffset);
		Ar << EndOffset;
		Ar.Seek(EndOffset);
	}
}

/**
 * Loads the reference and local shader caches for the passed in platform
 *
 * @param	Platform	Platform to load shader caches for.
 */
static void LoadShaderCaches( EShaderPlatform Platform )
{
#if !MOBILE
	// If we're building the reference shader cache, only load the reference shader cache.
	const UBOOL bLoadOnlyReferenceShaderCache = ParseParam(appCmdLine(), TEXT("refcache"));
	
	if (appGetPlatformType() & UE3::PLATFORM_WindowsServer) 
	{
		//Don't load shaders at all for dedicated servers
		return;
	}

	for( INT TypeIndex=0; TypeIndex<SC_NumShaderCacheTypes; TypeIndex++ )
	{
		UShaderCache*& ShaderCacheRef = GShaderCaches[TypeIndex][Platform];
		//check for reentry
		check(ShaderCacheRef == NULL);

		// If desired, skip loading the local shader cache.
		if(TypeIndex == SC_Local && bLoadOnlyReferenceShaderCache)
		{
			continue;
		}

		// mark that we are serializing the local shader cache, so we can load the global shaders
		GSerializingLocalShaderCache = (TypeIndex == SC_Local);
		
		// by default, we have no shadercache
		ShaderCacheRef = NULL;

		// only look for the shader cache object if the package exists (avoids a throw inside the code)
		FFilename PackageName = (TypeIndex == SC_Local ? GetLocalShaderCacheFilename(Platform) : GetReferenceShaderCacheFilename(Platform));
		FString Filename;
		if (GPackageFileCache->FindPackageFile(*PackageName, NULL, Filename))
		{
			// if another instance is writing the shader cache while we are reading, then opening will fail, and 
			// that's not good, so retry until we can open it
#if !CONSOLE
			// this "lock" will make sure that another process can't be writing to the package before we actually 
			// read from it with LoadPackage, etc below
			FArchive* ReaderLock = NULL;
			// try to open the shader cache for 
			DOUBLE StartTime = appSeconds();
			const DOUBLE ShaderRetryDelaySeconds = 15.0;

			// try until we can read the file, or until ShaderRetryDelaySeconds has passed
			while (ReaderLock == NULL && appSeconds() - StartTime < ShaderRetryDelaySeconds)
			{
				ReaderLock = GFileManager->CreateFileReader(*Filename);
				if(!ReaderLock)
				{
					// delay a bit
					appSleep(1.0f);
				}
			}
#endif

			UBOOL bSkipLoading = FALSE;
			if( TypeIndex == SC_Local && (CONSOLE || !GUseSeekFreeLoading))
			{
				// This function is being called during script compilation, which is why we need to use LOAD_FindIfFail.
				UObject::BeginLoad();
				ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_NoWarn | LOAD_FindIfFail, NULL, NULL );
				UObject::EndLoad();
				// Skip loading the local shader cache if it was built with an old version.
				const INT MaxVersionDifference = 10;
				// Skip loading the local shader cache if the version # is less than a given threshold
				const INT MinShaderCacheVersion = VER_PARTICLE_LOD_DISTANCE_FIXUP;
				if( Linker && 
					((GEngineVersion - Linker->Summary.EngineVersion) > MaxVersionDifference ||
					Linker->Ver() < MinShaderCacheVersion) )
				{
					bSkipLoading = TRUE;
				}
			}

			// Skip loading the shader cache if wanted.
			if( !bSkipLoading )
			{
#if !CONSOLE
				// if we are seekfree loading and we are in here, we are a PC game which doesn't cook the shadercaches into each package, 
				// but we can't call LoadPackage normally because we are going to be inside loading Engine.u at this point, so the 
				// ResetLoaders at the end of the LoadPackage (in the seekfree case) will fail because the objects in the shader cache 
				// package won't get EndLoad called on them because we are already inside an EndLoad (for Engine.u).
				// Instead, we use LoadObject, and we will ResetLoaders on the package later in the startup process. We need to disable 
				// seekfree temporarily because LoadObject doesn't work in the seekfree case if the object isn't already loaded.
				if (GUseSeekFreeLoading)
				{
					// unset GUseSeekFreeLoading so that the LoadObject will work
					GUseSeekFreeLoading = FALSE;
					ShaderCacheRef = LoadObject<UShaderCache>(NULL, *(PackageName.GetBaseFilename() + TEXT(".CacheObject")), NULL, LOAD_NoWarn, NULL );
					GUseSeekFreeLoading = TRUE;
				}
				else
#endif
				{
					// This function could be called during script compilation, which is why we need to use LOAD_FindIfFail.
					UPackage* ShaderCachePackage = UObject::LoadPackage( NULL, *Filename, LOAD_NoWarn | LOAD_FindIfFail );
					if( ShaderCachePackage )
					{
						ShaderCacheRef = FindObject<UShaderCache>( ShaderCachePackage, TEXT("CacheObject") );
					}
				}
			}

#if !CONSOLE
			delete ReaderLock;
#endif
		}

		if(!ShaderCacheRef)
		{
			// if we didn't find the local shader cache, create it. if we don't find the refshadercache, that's okay, just leave it be
			if (TypeIndex == SC_Local || bLoadOnlyReferenceShaderCache)
			{
				// If the local shader cache couldn't be loaded, create an empty cache.
				FString LocalShaderCacheName = FString(TEXT("LocalShaderCache-")) + ShaderPlatformToText(Platform);
				ShaderCacheRef = new(UObject::CreatePackage(NULL,*LocalShaderCacheName),TEXT("CacheObject")) UShaderCache(Platform);
				ShaderCacheRef->MarkPackageDirty(FALSE);
			}
		}
		// if we found it, make sure it's loaded
		else
		{
			// if this function was inside a BeginLoad()/EndLoad(), then the LoadObject above didn't actually serialize it, this will
			ULinkerLoad* Linker = ShaderCacheRef->GetLinker();
			
			Linker->Preload(ShaderCacheRef);


			// Make sure we don't hold onto huge decompression buffers via a FArchiveAsync object
			if ((Linker->Loader != NULL) && !GUseSeekFreeLoading)
			{
				Linker->Loader->FlushCache();
			}
		}

		if (ShaderCacheRef)
		{
			// make sure it's not GC'd
			ShaderCacheRef->AddToRoot();
		}

		GSerializingLocalShaderCache = FALSE;
	}

	// If we only loaded the reference shader cache, use it in place of the local shader cache.
	if(bLoadOnlyReferenceShaderCache)
	{
		GShaderCaches[SC_Local][Platform] = GShaderCaches[SC_Reference][Platform];
	}
#endif
}

/** Archive used when saving shaders, which generates data used to detect serialization mismatches on load. */
class FShaderSaveArchive : public FArchiveProxy
{
public:

	FShaderSaveArchive(FArchive& Archive, TArray<WORD>& InSerializations) : 
		FArchiveProxy(Archive),
		NextSerialization(0),
		Serializations(InSerializations)
	{
		OriginalPosition = Archive.Tell();
	}

	virtual ~FShaderSaveArchive()
	{
		// Seek back to the original archive position so we can undo any serializations that went through this archive
		InnerArchive.Seek(OriginalPosition);
	}

	virtual void Serialize( void* V, INT Length )
	{
		check(Length < USHRT_MAX);
		if (NextSerialization < Serializations.Num())
		{
			// We are no longer appending (due to a seek), make sure writes match up in size with what's already been written
			check(Length == Serializations(NextSerialization));
		}
		else
		{
			// Appending to the archive, track the size of this serialization
			Serializations.AddItem(Length);
		}
		NextSerialization++;
		
		FArchiveProxy::Serialize(V, Length);
	}

	virtual void Seek( INT InPos )
	{
		INT Offset = InPos - Tell();
		if (Offset <= 0)
		{
			// We're seeking backward, walk backward through the serialization history while updating NextSerialization
			while (Offset < 0)
			{
				// Not supporting seeking outside of the history tracked by this FShaderSaveArchive
				check(NextSerialization > 0);
				Offset += Serializations(NextSerialization - 1);
				NextSerialization--;
			}
		}
		else
		{
			// We're seeking forward, walk forward through the serialization history while updating NextSerialization
			while (Offset > 0)
			{
				// Not supporting seeking past the front most serialization in the history
				check(NextSerialization - 1 < Serializations.Num());
				Offset -= Serializations(NextSerialization - 1);
				NextSerialization++;
			}
			NextSerialization++;
		}
		check(Offset == 0);
		
		FArchiveProxy::Seek(InPos);
	}

	/** Next index into Serializations. */
	INT NextSerialization;
	/** Tracks the lengths of serializations that have gone through this archive. */
	TArray<WORD>& Serializations;

private:
	/** Stored off position of the original archive we are wrapping. */
	INT OriginalPosition;
};

/** Archive used when loading shaders which detects serialization mismatches. */
class FShaderLoadArchive : public FArchiveProxy
{
public:

	FShaderLoadArchive(FArchive& Archive, const TArray<WORD>& InPastSerializations, UBOOL bInEnableAutomaticVersioning) : 
		FArchiveProxy(Archive),
		NextSerialization(0),
		bMismatch(FALSE),
		PastSerializations(InPastSerializations),
		bEnableAutomaticVersioning(bInEnableAutomaticVersioning)
	{
	}

	virtual void Serialize( void* V, INT Length )
	{
		check(Length < USHRT_MAX);

		if (NextSerialization >= PastSerializations.Num() || PastSerializations(NextSerialization) != Length)
		{
			// There has been a serialization mismatch if we are trying to serialize more times than the stored history,
			// Or if we are trying to serialize a different size.
			bMismatch = TRUE;
		}

		// If there has been a mismatch, we can't forward the serialization to the underlying archive
		if (bMismatch && bEnableAutomaticVersioning)
		{
			checkSlow(IsLoading());
			// Try to initialize the value to something safe.  This is not robust but works in most cases.
			appMemzero(V, Length);
		}
		else
		{
			FArchiveProxy::Serialize(V, Length);
		}
		NextSerialization++;
	}

	virtual void Seek( INT InPos )
	{
		if (!bEnableAutomaticVersioning)
		{
			// Always forward the seek if automatic versioning is disabled
			FArchiveProxy::Seek(InPos);
		}
		else if (!bMismatch)
		{
			INT Offset = InPos - Tell();
			if (Offset <= 0)
			{
				// We're seeking backward, walk backward through the serialization history while updating NextSerialization
				while (Offset < 0)
				{
					check(NextSerialization > 0);
					Offset += PastSerializations(NextSerialization - 1);
					NextSerialization--;
				}
			}
			else
			{
				// We're seeking forward, walk forward through the serialization history while updating NextSerialization
				while (Offset > 0)
				{
					check(NextSerialization - 1 < PastSerializations.Num());
					Offset -= PastSerializations(NextSerialization - 1);
					NextSerialization++;
				}
				NextSerialization++;
			}
			check(Offset == 0);

			FArchiveProxy::Seek(InPos);
		}
	}

	UBOOL HadSerializationMismatch() const
	{
		// Report a mismatch if one was detected during serialization,
		// Or if we haven't serialized the same number of times during loading as we did during saving
		return (bMismatch || NextSerialization != PastSerializations.Num()) && bEnableAutomaticVersioning;
	}

private:
	INT NextSerialization;
	UBOOL bMismatch;
	// Sizes of serializations that occurred during saving
	const TArray<WORD>& PastSerializations;
	// Whether to throw away shaders with serialization mismatches
	UBOOL bEnableAutomaticVersioning;
};

/**
 *	Save the shaders that are mapped for a material
 *
 *	@param	InShaders				The shaders to serialize.
 *	@param	Ar						The archive to serialize them to.
 */
void SerializeShaders(const TMap<FGuid,FShader*>& InShaders, FArchive& Ar)
{
	// Whether to generate or use automatic shader versioning data
	UBOOL bSerializeAutomaticVersioningData = FALSE;
#if !CONSOLE
	// We can be serializing cooked data when cooking or when script patching, so handle those cases
	if (!((GIsCooking && !(GCookingTarget & UE3::PLATFORM_PC) || SUPPORTS_SCRIPTPATCH_CREATION)
		&& Ar.ContainsCookedData()))
	{
		// Only use automatic versioning on PC, but not when saving shaders into a cooked package for a non-PC platform
		bSerializeAutomaticVersioningData = TRUE;
	}
#endif

	if (Ar.IsSaving())
	{
		INT NumShaders = InShaders.Num();
		Ar << NumShaders;
		for (TMap<FGuid,FShader*>::TConstIterator ShaderIt(InShaders); ShaderIt; ++ShaderIt)
		{
			FShader* Shader = ShaderIt.Value();
			// Serialize the shader type and ID separately, so at load time we can see whether 
			// this is a redundant shader without fully deserializing it.
			FShaderType* ShaderType = Shader->GetType();
			FGuid ShaderId = Shader->GetId();
			Ar << ShaderType << ShaderId;

			// Serialize the hash of the shader's source files that it was compiled with,
			// So we can skip the shader if its source files have changed.
			FSHAHash Hash = Shader->GetHash();
			Ar << Hash;
			
			// Write a placeholder value for the skip offset.
			INT SkipOffset = Ar.Tell();
			Ar << SkipOffset;

			TArray<WORD> Serializations;
			INT FirstSaveSize = 0;
			if (bSerializeAutomaticVersioningData)
			{
				const INT ShaderSerializeBegin = Ar.Tell();
				// Wrap the archive with a FShaderSaveArchive which tracks serializations
				FShaderSaveArchive SaveArchive(Ar, Serializations);
				// Serialize the shader once, generating a serialization history.
				// The FShaderSaveArchive dtor will seek Ar back to the original position so the results of this serialization will be overwritten.
				Shader->Serialize(SaveArchive);
				FirstSaveSize = SaveArchive.Tell() - ShaderSerializeBegin;
			}
			// Store the serialization history, so we can detect mismatches on load
			Ar << Serializations;

			const INT ShaderSerializeBegin = Ar.Tell();

			// Serialize the shader.
			Shader->Serialize(Ar);

			const INT ShaderSerializationEnd = Ar.Tell();
			// Verify that the amount serialized while generating the serialization history matches the actual serialization
			check(!bSerializeAutomaticVersioningData || FirstSaveSize == ShaderSerializationEnd - ShaderSerializeBegin);
			
			// Write the actual offset of the end of the shader data over the placeholder value written above.
			INT EndOffset = Ar.Tell();
			Ar.Seek(SkipOffset);
			Ar << EndOffset;
			Ar.Seek(EndOffset);
		}
	}
	else
	if (Ar.IsLoading())
	{
		// Serialize the shaders themselves...
		INT NumShaders = 0;
		INT NumLegacyShaders = 0;
		INT NumRedundantShaders = 0;
		TArray<FString> OutdatedShaderTypes;

		Ar << NumShaders;

		// Load the shaders in the cache.
		for (INT ShaderIndex = 0; ShaderIndex < NumShaders; ShaderIndex++)
		{
			// Deserialize the shader type and shader ID.
			FShaderType* ShaderType = NULL;
			FGuid ShaderId;
			Ar << ShaderType << ShaderId;

			FSHAHash SavedHash;
			if (Ar.Ver() < VER_FIXED_AUTO_SHADER_VERSIONING)
			{
				appMemzero(&SavedHash.Hash, sizeof(SavedHash.Hash));
			}
			else
			{
				// Load the hash of the shader's files that it was compiled with
				Ar << SavedHash;
			}

			// Deserialize the offset of the next shader.
			INT SkipOffset = 0;
			Ar << SkipOffset;

			if (!ShaderType)
			{
				// If the shader type doesn't exist anymore, skip the shader.
				Ar.Seek(SkipOffset);
				NumLegacyShaders++;
			}
			else
			{
				// Get the current hash of the shader's source files
				const FSHAHash& CurrentHash = ShaderType->GetSourceHash();
				
				FShader* Shader = ShaderType->FindShaderById(ShaderId);
				if (Shader)
				{
					// If a shader with the same type and ID is already resident, skip this shader.
					Ar.Seek(SkipOffset);
					NumRedundantShaders++;
				}
				else if (ShouldReloadChangedShaders() && SavedHash != CurrentHash)
				{
					// If the shader has changed since it was last compiled, skip it.
					Ar.Seek(SkipOffset);
					NumLegacyShaders++;
					OutdatedShaderTypes.AddUniqueItem(FString(ShaderType->GetName()));
				}
				else if ((Ar.Ver() < ShaderType->GetMinPackageVersion()) || (Ar.LicenseeVer() < ShaderType->GetMinLicenseePackageVersion()))
				{
					// If the shader type's serialization is compatible with the version the shader was saved in, skip it.
					Ar.Seek(SkipOffset);
					NumLegacyShaders++;
				}
				else
				{
					// Create a new instance of the shader type.
					Shader = ShaderType->ConstructForDeserialization();

					TArray<WORD> Serializations;
					if (Ar.Ver() >= VER_FIXED_AUTO_SHADER_VERSIONING)
					{
						// Load the serialization history of the shader
						Ar << Serializations;
					}

					// Wrap Ar with an archive that can detect serialization mismatches
					FShaderLoadArchive LoadArchive(Ar, Serializations, bSerializeAutomaticVersioningData);

					// Deserialize the shader into the new instance.
					const UBOOL bShaderHasOutdatedParameters = Shader->Serialize(LoadArchive);

					if (LoadArchive.HadSerializationMismatch() || bShaderHasOutdatedParameters)
					{
						// Remove all references to the shader and delete it since it has outdated parameters.
						ShaderType->DeregisterShader(Shader);
						delete Shader;
						Ar.Seek(SkipOffset);
						NumLegacyShaders++;
						OutdatedShaderTypes.AddUniqueItem(FString(ShaderType->GetName()));
					}
					else
					{
						// If this happens it probably indicates a bug in the automatic shader versioning
						checkf(Ar.Tell() == SkipOffset, 
							TEXT("Deserialized the wrong amount for shader %s!  Expected archive position %i, got position %i\n"), 
							ShaderType->GetName(), 
							SkipOffset, 
							Ar.Tell()
							);
					}
				}
			}
		}
		
		if (ShouldReloadChangedShaders())
		{
			const INT NumOutdatedShaderTypes = OutdatedShaderTypes.Num();
			if (NumOutdatedShaderTypes > 0)
			{
				warnf(NAME_DevShaders, TEXT("Skipped %i outdated FShaderTypes from %s shader cache:"), 
					NumOutdatedShaderTypes,
					GSerializingLocalShaderCache ? TEXT("Local") : TEXT("Ref"));
				for (INT TypeIndex = 0; TypeIndex < NumOutdatedShaderTypes; TypeIndex++)
				{	
					warnf(NAME_DevShaders, TEXT("	%s"), *OutdatedShaderTypes(TypeIndex));
				}
			}
		}

		// Log some cache stats.
		if( NumShaders )
		{
			debugf(NAME_DevShaders,TEXT("... Loaded %u shaders (%u legacy, %u redundant)"),NumShaders,NumLegacyShaders,NumRedundantShaders);
		}
	}
}

/**
 * Returns the reference shader cache for the passed in platform
 *
 * @param	Platform	Platform to return reference shader cache for.
 * @return	The reference shader cache for the passed in platform
 */
UShaderCache* GetReferenceShaderCache( EShaderPlatform Platform )
{
	// make sure shader caches are loaded
	if( !GShaderCaches[SC_Local][Platform] )
	{
		LoadShaderCaches( Platform );
	}

	return GShaderCaches[SC_Reference][Platform];
}

/**
 * Returns the local shader cache for the passed in platform
 *
 * @param	Platform	Platform to return local shader cache for.
 * @return	The local shader cache for the passed in platform
 */
UShaderCache* GetLocalShaderCache( EShaderPlatform Platform )
{
	if( !GShaderCaches[SC_Local][Platform] )
	{
		LoadShaderCaches( Platform );
	}

	return GShaderCaches[SC_Local][Platform];
}

/**
 * Returns the global shader cache for the passed in platform
 *
 * @param	Platform	Platform to return global shader cache for.
 * @return	The global shader cache for the passed in platform
 */
FShaderCache* GetGlobalShaderCache( EShaderPlatform Platform )
{
	if(!GGlobalShaderCaches[Platform])
	{
		GGlobalShaderCaches[Platform] = new FShaderCache(Platform);
	}
	return GGlobalShaderCaches[Platform];
}

/**
 * Saves the local shader cache for the passed in platform.
 *
 * @param	Platform	Platform to save shader cache for.
 * @param	OverrideCacheFilename If non-NULL, then the shader cache will be saved to the given path
 */
void SaveLocalShaderCache(EShaderPlatform Platform, const TCHAR* OverrideCacheFilename)
{
#if !SHIPPING_PC_GAME
	// Only save the shader cache for the first instance running.
	if( GIsFirstInstance || OverrideCacheFilename )  // if folks are overriding, they know what they are doing and can allow multiple saves
	{
#endif

		UShaderCache* ShaderCache = GShaderCaches[SC_Local][Platform];
		if( ShaderCache && ShaderCache->IsDirty())
		{
			// Reset the LinkerLoads for all shader caches, since we may be saving the local shader cache over the refshadercache file.
			for(INT TypeIndex = 0;TypeIndex < SC_NumShaderCacheTypes;TypeIndex++)
			{
				if(GShaderCaches[TypeIndex][Platform])
				{
					UObject::ResetLoaders(GShaderCaches[TypeIndex][Platform]);
				}
			}

			UPackage* ShaderCachePackage = ShaderCache->GetOutermost();
			// The shader cache isn't network serializable
			if (ParseParam(appCmdLine(), TEXT("MTCHILD")))
			{
				// mt children should not save the shader cache compressed as it just wastes time
				ShaderCachePackage->PackageFlags |= PKG_ServerSideOnly;
				ShaderCachePackage->PackageFlags &= ~PKG_StoreCompressed;
			}
			else
			{
				ShaderCachePackage->PackageFlags |= PKG_ServerSideOnly | PKG_StoreCompressed;
			}

			if( OverrideCacheFilename )
			{
				debugf(TEXT("saving '%s'"),OverrideCacheFilename);
				UObject::SavePackage(ShaderCachePackage, ShaderCache, 0, OverrideCacheFilename, GWarn, NULL, FALSE, TRUE, SAVE_NoError);
			}
			else
			{
				UObject::SavePackage(ShaderCachePackage, ShaderCache, 0, *GetLocalShaderCacheFilename(Platform), GWarn, NULL, FALSE, TRUE, SAVE_NoError);
			}

			// mark it as clean, as its been saved!
			ShaderCache->MarkClean();

			// release memory held by cached shader files
			FlushShaderFileCache();
		}

#if !SHIPPING_PC_GAME
	}
	else
	{
		// Only warn once.
		static UBOOL bAlreadyWarned = FALSE;
		if( !bAlreadyWarned )
		{
			bAlreadyWarned = TRUE;
			debugf( NAME_Warning, TEXT("Skipping saving the shader cache as another instance of the game is running.") );
		}
	}
#endif
}

/**
 * Saves all local shader caches.
 */
void SaveLocalShaderCaches()
{
	// Don't save shader caches when cooking dedicated servers
	if (GCookingTarget & UE3::PLATFORM_WindowsServer)
	{
		return;
	}

	// make sure the PC shader caches are always saved with PC compression when cooking for ES2 platforms 
	// (which don't have their own shader cache)
	ECompressionFlags SavedCompressionType = GBaseCompressionMethod;
	if (GIsCooking) // && (GCookingTarget & UE3::PLATFORM_OpenGLES2))
	{
		GBaseCompressionMethod = COMPRESS_DefaultPC;
	}

	for( INT PlatformIndex=0; PlatformIndex<SP_NumPlatforms; PlatformIndex++ )
	{
		SaveLocalShaderCache( (EShaderPlatform)PlatformIndex );
	}

	// restore the compression type 
	GBaseCompressionMethod = SavedCompressionType;
}

FString GetShaderCacheFilename(const TCHAR* Name, const TCHAR* Extension, EShaderPlatform Platform)
{
	UBOOL bUseCookedPath = GUseSeekFreeLoading;
#if CONSOLE
	bUseCookedPath = TRUE;
#endif
	if(bUseCookedPath)
	{
		// shader cache is in the cooked directory
		FString CookedDirName;
		appGetCookedContentPath(appGetPlatformType(), CookedDirName);

		return FString::Printf(
			TEXT("%s%s-%s.%s"), 
			*CookedDirName,
			Name,
			ShaderPlatformToText(Platform),
			Extension
			);
	}
	else
	{
		// find the first entry in the Paths array that contains the GameDir (so we don't find an ..\Engine path)
		for (INT PathIndex = 0; PathIndex < GSys->Paths.Num(); PathIndex++)
		{
			// Swap Windows directory separators with platform-dependent ones.
			const FString Path(GSys->Paths(PathIndex).Replace(TEXT("\\"), PATH_SEPARATOR));
			// does the path contain GameDir? (..\ExampleGame\)
			if (Path.InStr(appGameDir(), FALSE, TRUE) != -1)
			{
				// if so, use it
				return FString::Printf(TEXT("%s%s%s-%s.%s"), 
					*GSys->Paths(PathIndex),
					PATH_SEPARATOR,
					Name,
					ShaderPlatformToText(Platform),
					Extension
					);
			}
		}

		checkf(FALSE, TEXT("When making the ShaderCache filename, failed to find a GSys->Path containing %s"), *appGameDir());
		return TEXT("");
	}
}

FString GetLocalShaderCacheFilename( EShaderPlatform Platform )
{
	return GetShaderCacheFilename(TEXT("LocalShaderCache"), TEXT("upk"), Platform);
}

FString GetReferenceShaderCacheFilename( EShaderPlatform Platform )
{
	return GetShaderCacheFilename(TEXT("RefShaderCache"), TEXT("upk"), Platform);
}

FString GetGlobalShaderCacheFilename(EShaderPlatform Platform)
{
	// platforms with flattened materials don't have a shader cache (yet), so we just
	// reuse the SM3 shader cache, because the code will crash in other places if
	// no shader cache is loaded
	if( GUsingES2RHI )
	{
		return GetShaderCacheFilename(TEXT("GlobalShaderCache"),TEXT("bin"), SP_PCD3D_SM3);
	}
	return GetShaderCacheFilename(TEXT("GlobalShaderCache"),TEXT("bin"),Platform);
}
