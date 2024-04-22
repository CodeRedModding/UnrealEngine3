/*=============================================================================
	UnContentCookers.h: Content cooker helper objects.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Cooking.
-----------------------------------------------------------------------------*/


/**
 * Helper structure containing information needed for separating bulk data from it's archive
 */
struct FCookedBulkDataInfo
{
	/** From last saving or StoreInSeparateFile call: Serialized flags for bulk data									*/
	DWORD	SavedBulkDataFlags;
	/** From last saving or StoreInSeparateFile call: Number of elements in bulk data array								*/
	INT		SavedElementCount;
	/** From last saving or StoreInSeparateFile call: Offset of bulk data into file or INDEX_NONE if no association		*/
	INT		SavedBulkDataOffsetInFile;
	/** From last saving or StoreInSeparateFile call: Size of bulk data on disk or INDEX_NONE if no association			*/
	INT		SavedBulkDataSizeOnDisk;
	/** Name of texture file cache if saved in one.																		*/
	FName	TextureFileCacheName;

	/**
	 * Serialize operator
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Info	Structure to serialize
	 * @return	Ar after having been used for serialization
	 */
	friend FArchive& operator<<( FArchive& Ar, FCookedBulkDataInfo& Info )
    {
        Ar << Info.SavedBulkDataFlags;
		Ar << Info.SavedElementCount;
		Ar << Info.SavedBulkDataOffsetInFile;
		Ar << Info.SavedBulkDataSizeOnDisk;
		Ar << Info.TextureFileCacheName;
		return Ar;
    }
};

/**
* Helper structure containing information needed to track updated texture file cache entries 
*/
struct FCookedTextureFileCacheInfo
{
	/** 
	* Constructor 
	*/
	FCookedTextureFileCacheInfo() 
		:	TextureFileCacheGuid(0,0,0,0)
		,	TextureFileCacheName(NAME_None)
		,	LastSaved(0)
	{
	}

	/**
	* Serialize operator
	*
	* @param	Ar		Archive to serialize with
	* @param	Info	Structure to serialize
	* @return	Ar after having been used for serialization
	*/
	friend FArchive& operator<<( FArchive& Ar, FCookedTextureFileCacheInfo& Info )
	{
		Ar << Info.TextureFileCacheGuid;
		Ar << Info.TextureFileCacheName;

		Ar << Info.LastSaved;
		return Ar;
	}

	/** Unique texture ID to determine if texture file cache bulk data needs an update */
	FGuid TextureFileCacheGuid;
	/** Name of texture file cache archive used */
	FName TextureFileCacheName;
	/** Time the texture entry was last saved into the TFC */
	DOUBLE LastSaved;
};

/**
 *	Structure for tracking TFC entries for post-cook verification
 */
enum ETextureFileCacheType
{
	TFCT_Base = 0,
	TFCT_Character,
	TFCT_Lighting
};

struct FTextureFileCacheEntry
{
	/** The Type of TFC the texture is in */
	ETextureFileCacheType TFCType;
	/** The path name of the texture... */
	FString TextureName;
	/** The mip of the texture... */
	INT MipIndex;
	/** The GUID of the texture, the file cache name and the last time it was saved. */
	//FCookedTextureFileCacheInfo TFCInfo;
	/** The offset of the entry */
	INT OffsetInCache;
	/** The size of the entry */
	INT SizeInCache;
	/** The flags of the entry */
	INT FlagsInCache;
	/** The flags of the entry */
	INT ElementCountInCache;

	FTextureFileCacheEntry() :
		  TFCType(TFCT_Base)
		, TextureName(TEXT(""))
		, MipIndex(-1)
		, OffsetInCache(-1)
		, SizeInCache(-1)
		, FlagsInCache(-1)
		, ElementCountInCache(-1)
	{
	}

	FTextureFileCacheEntry(const FTextureFileCacheEntry& Src) :
		  TFCType(Src.TFCType)
		, TextureName(Src.TextureName)
		, MipIndex(Src.MipIndex)
		, OffsetInCache(Src.OffsetInCache)
		, SizeInCache(Src.SizeInCache)
		, FlagsInCache(Src.FlagsInCache)
		, ElementCountInCache(Src.ElementCountInCache)
	{
	}

	static UBOOL EntriesOverlap(FTextureFileCacheEntry& EntryA, FTextureFileCacheEntry& EntryB)
	{
		if (EntryA.TFCType != EntryB.TFCType)
		{
			return FALSE;
		}

		if (EntryA.TextureName == EntryB.TextureName)
		{
			if (EntryA.OffsetInCache == EntryB.OffsetInCache)
			{
				if (EntryA.SizeInCache == EntryB.SizeInCache)
				{
					return FALSE;
				}
			}
		}

		if (((EntryA.OffsetInCache >= EntryB.OffsetInCache) && 
			(EntryA.OffsetInCache < (EntryB.OffsetInCache + EntryB.SizeInCache))) ||
			((EntryB.OffsetInCache >= EntryA.OffsetInCache) &&
			(EntryB.OffsetInCache < (EntryA.OffsetInCache + EntryA.SizeInCache))))
		{
			return TRUE;
		}

		return FALSE;
	}
};

/**
 * Texture usage information container used to determine memory usage of textures on DVD.
 */
struct FCookedTextureUsageInfo
{
	/** List of packages this texture's low mips are cooked into.					*/
	TSet<FString>	PackageNames;
	/** Format of texture.															*/
	BYTE			Format;
	/** Texture LOD group.															*/
	BYTE			LODGroup;
	/** Width of texture.															*/
	INT				SizeX;
	/** Height of texture.															*/
	INT				SizeY;
	/** 
	 * Size of higher mips stored just once. Does not include storage alignment 
	 * waste and compression in e.g. the TFC case.
	 */
	INT				StoredOnceMipSize;
	/** 
	 * Size of lower mips duplicated into packages. This is before compression and
	 * the size is not a total but rather for a single package. Multiply by 
	 * PackageNames.Num() for total.												
	 */
	INT				DuplicatedMipSize;

	FCookedTextureUsageInfo()
	:	Format( PF_Unknown )
	,	LODGroup( TEXTUREGROUP_MAX )
	,	SizeX( 0 )
	,	SizeY( 0 )
	,	StoredOnceMipSize( 0 )
	,	DuplicatedMipSize( 0 )
	{}

	/**
	 * Serialize operator
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Info	Structure to serialize
	 * @return	Ar after having been used for serialization
	 */
	friend FArchive& operator<<( FArchive& Ar, FCookedTextureUsageInfo& Info )
	{
		Ar << Info.PackageNames << Info.Format << Info.LODGroup << Info.SizeX << Info.SizeY;
		Ar << Info.StoredOnceMipSize << Info.DuplicatedMipSize;
		return Ar;
	}
};

/**
 *	Helper structure for tracking information about force-cooked objects
 */
struct FForceCookedInfo
{
	/** 
	 *	List of content items that should be tagged as already cooked
	 *	Note that these items may/may not actually be loaded at the time.
	 *	TMap for quicker look-up than array searching
	 */
	TMap<FString, UBOOL>	CookedContentList;

	/**
	 * Serialize operator
	 *
	 * @param	Ar		Archive to serialize with
	 * @param	Info	Structure to serialize
	 * @return	Ar		after having been used for serialization
	 */
	friend FArchive& operator<<(FArchive& Ar, FForceCookedInfo& Info)
	{
		Ar << Info.CookedContentList;
		return Ar;
	}
};

/** Stores information about shader files that is used to detect shader file changes for cooking. */
struct FPersistentShaderData
{
public:

	/** 
	 * Updates the persistent data's shader hashes.  
	 * Returns TRUE if the new hashes were different than the existing persistent data's hashes.
	 */
	UBOOL UpdateShaderHashes(const TMap<FString, FSHAHash>& InShaderTypeToSHA, const TMap<FString, FSHAHash>& InVertexFactoryTypeToSHA)
	{
		const UBOOL bNewHashesDifferent = LegacyCompareNotEqual(InShaderTypeToSHA, ShaderTypeToSHA) ||  LegacyCompareNotEqual(InVertexFactoryTypeToSHA, VertexFactoryTypeToSHA);
		if (bNewHashesDifferent)
		{
			ShaderTypeToSHA = InShaderTypeToSHA;
			VertexFactoryTypeToSHA = InVertexFactoryTypeToSHA;
		}
		return bNewHashesDifferent;
	}

	virtual void Serialize(FArchive& Ar)
	{		
		Ar << ShaderTypeToSHA;
		Ar << VertexFactoryTypeToSHA;
	}

	/** Loads the persistent shader data from disk, if the file exists. */
	void LoadFromDisk(const FString& Filename)
	{
		FArchive* PersistentShaderDataFile = GFileManager->CreateFileReader(*Filename);
		if (PersistentShaderDataFile)
		{
			Serialize(*PersistentShaderDataFile);
			delete PersistentShaderDataFile;
		}
	}

	/** Saves the persistent shader data to disk. */
	void SaveToDisk(const FString& Filename)
	{
		FArchive* PersistentShaderDataFile = GFileManager->CreateFileWriter(*Filename);
		Serialize(*PersistentShaderDataFile);
		delete PersistentShaderDataFile;
	}

private:

	/** Map from shader type name to SHA key of the shader file, used to detect when to recook due to shader changes. */
	TMap<FString, FSHAHash> ShaderTypeToSHA;

	/** Map from vertex factory type name to SHA key of the vertex factory file, used to detect when to recook due to VF changes. */
	TMap<FString, FSHAHash> VertexFactoryTypeToSHA;
};

extern DOUBLE		GShaderCacheLoadTime;
extern DOUBLE		GRHIShaderCompileTime_Total;
extern DOUBLE		GRHIShaderCompileTime_PS3;
extern DOUBLE		GRHIShaderCompileTime_XBOXD3D;
extern DOUBLE		GRHIShaderCompileTime_NGP;
extern DOUBLE		GRHIShaderCompileTime_WIIU;
extern DOUBLE		GRHIShaderCompileTime_PCD3D_SM3;
extern DOUBLE		GRHIShaderCompileTime_PCD3D_SM5;
extern DOUBLE		GRHIShaderCompileTime_PCOGL;

// Compression stats.
extern DOUBLE		GCompressorTime;
extern QWORD		GCompressorSrcBytes;
extern QWORD		GCompressorDstBytes;
extern DOUBLE		GArchiveSerializedCompressedSavingTime;

/** FCookStats
* A container for the cooker stats to facilitate transfer to the parent process
* Several assumptions are made that this is POD and never persists
**/
struct FCookStats
{
	DOUBLE	TotalTime;
	DOUBLE	AlwaysLoadedProcessingTime;
	DOUBLE	ContentProcessingTime;
	DOUBLE	CreateIniFilesTime;
	DOUBLE	LoadSectionPackagesTime;
	DOUBLE	LoadNativePackagesTime;
	DOUBLE	LoadDependenciesTime;
	DOUBLE	LoadPackagesTime;
	DOUBLE	LoadPerMapPackagesTime;
	DOUBLE	LoadCombinedStartupPackagesTime;
	DOUBLE	CheckDependentPackagesTime;
	DOUBLE	ExternGShaderCacheLoadTime;
	DOUBLE	SaveShaderCacheTime;
	DOUBLE	CopyShaderCacheTime;
	DOUBLE	ExternGRHIShaderCompileTime_Total;
	DOUBLE	ExternGRHIShaderCompileTime_PS3;
	DOUBLE	ExternGRHIShaderCompileTime_XBOXD3D;
	DOUBLE	ExternGRHIShaderCompileTime_NGP;
	DOUBLE	ExternGRHIShaderCompileTime_WIIU;
	DOUBLE	ExternGRHIShaderCompileTime_PCD3D_SM3;
	DOUBLE	ExternGRHIShaderCompileTime_PCD3D_SM5;
	DOUBLE	ExternGRHIShaderCompileTime_PCOGL;

	DOUBLE	CleanupMaterialsTime;
	DOUBLE  CleanupShaderCacheTime;
	DOUBLE	CookTime;
	DOUBLE	CookPhysicsTime;
	DOUBLE	CookTextureTime;
	DOUBLE	CookSoundTime;
	DOUBLE	CookSoundCueTime;
	DOUBLE	LocSoundTime;
	DOUBLE	CookMovieTime;
	DOUBLE	CookStripTime;
	DOUBLE	CookSkeletalMeshTime;
	DOUBLE	CookStaticMeshTime;
	DOUBLE	CookParticleSystemTime;
	DOUBLE	CookParticleSystemDuplicateRemovalTime;
	DOUBLE	CookLandscapeTime;

	DOUBLE	PackageSaveTime;
	DOUBLE	PackageLocTime;

	DOUBLE	PrepareForSavingTime;
	DOUBLE	PrepareForSavingTextureTime;
	DOUBLE	PrepareForSavingTerrainTime;
	DOUBLE	PrepareForSavingMaterialTime;
	DOUBLE	PrepareForSavingMaterialInstanceTime;
	DOUBLE	PrepareForSavingStaticMeshTime;

	DOUBLE	CollectGarbageAndVerifyTime;

	DOUBLE	PrePackageIterationTime;
	DOUBLE	PackageIterationTime;
	DOUBLE	PrepPackageTime;
	DOUBLE	TagCookedStartupTime;
	DOUBLE	MarkCookedStartupTime;
	DOUBLE	InitializePackageTime;
	DOUBLE	FinalizePackageTime;
	DOUBLE	PostPackageIterationTime;

	DOUBLE	PersistentFaceFXTime;
	DOUBLE	PersistentFaceFXDeterminationTime;
	DOUBLE	PersistentFaceFXGenerationTime;

	/** Total size, in bytes, of raw texture data that would have been saved to the TFC but was already there */
	QWORD	TFCTextureAlreadySaved;
	/** Total size, in bytes, of raw texture data that was saved to the TFC */
	QWORD	TFCTextureSaved;

	// Compression stats.
	DOUBLE	ExternGCompressorTime;
	QWORD	ExternGCompressorSrcBytes;
	QWORD	ExternGCompressorDstBytes;
	DOUBLE	ExternGArchiveSerializedCompressedSavingTime;

	/** constructor, assumes POD data and clears with a memZero */
	FCookStats()
	{
		appMemzero(this,sizeof(FCookStats));
	}
	/** copy constructor, assumes POD data and copies with a memcpy */
	FCookStats(const FCookStats &Other)
	{
		appMemcpy(this,&Other,sizeof(FCookStats));
	}
	/** op =, assumes POD data and copies with a memcpy */
	void operator=(const FCookStats &Other)
	{
		appMemcpy(this,&Other,sizeof(FCookStats));
	}
	/** accumulates all individual stats */
	void operator+=(const FCookStats &Other)
	{
		TotalTime += Other.TotalTime;
		AlwaysLoadedProcessingTime += Other.AlwaysLoadedProcessingTime;
		ContentProcessingTime += Other.ContentProcessingTime;
		CreateIniFilesTime += Other.CreateIniFilesTime;
		LoadSectionPackagesTime += Other.LoadSectionPackagesTime;
		LoadNativePackagesTime += Other.LoadNativePackagesTime;
		LoadDependenciesTime += Other.LoadDependenciesTime;
		LoadPackagesTime += Other.LoadPackagesTime;
		LoadPerMapPackagesTime += Other.LoadPerMapPackagesTime;
		LoadCombinedStartupPackagesTime += Other.LoadCombinedStartupPackagesTime;
		CheckDependentPackagesTime += Other.CheckDependentPackagesTime;
		ExternGShaderCacheLoadTime += Other.ExternGShaderCacheLoadTime;
		SaveShaderCacheTime += Other.SaveShaderCacheTime;
		CopyShaderCacheTime += Other.CopyShaderCacheTime;
		ExternGRHIShaderCompileTime_Total += Other.ExternGRHIShaderCompileTime_Total;
		ExternGRHIShaderCompileTime_PS3 += Other.ExternGRHIShaderCompileTime_PS3;
		ExternGRHIShaderCompileTime_XBOXD3D += Other.ExternGRHIShaderCompileTime_XBOXD3D;
		ExternGRHIShaderCompileTime_NGP += Other.ExternGRHIShaderCompileTime_NGP;
		ExternGRHIShaderCompileTime_PCD3D_SM3 += Other.ExternGRHIShaderCompileTime_PCD3D_SM3;
		ExternGRHIShaderCompileTime_PCD3D_SM5 += Other.ExternGRHIShaderCompileTime_PCD3D_SM5;
		ExternGRHIShaderCompileTime_PCOGL += Other.ExternGRHIShaderCompileTime_PCOGL;
		CleanupMaterialsTime += Other.CleanupMaterialsTime;
		CleanupShaderCacheTime += Other.CleanupShaderCacheTime;
		CookTime += Other.CookTime;
		CookPhysicsTime += Other.CookPhysicsTime;
		CookTextureTime += Other.CookTextureTime;
		CookSoundTime += Other.CookSoundTime;
		CookSoundCueTime += Other.CookSoundCueTime;
		LocSoundTime += Other.LocSoundTime;
		CookMovieTime += Other.CookMovieTime;
		CookStripTime += Other.CookStripTime;
		CookSkeletalMeshTime += Other.CookSkeletalMeshTime;
		CookStaticMeshTime += Other.CookStaticMeshTime;
		CookParticleSystemTime += Other.CookParticleSystemTime;
		CookParticleSystemDuplicateRemovalTime += Other.CookParticleSystemDuplicateRemovalTime;
		PackageSaveTime += Other.PackageSaveTime;
		PackageLocTime += Other.PackageLocTime;
		PrepareForSavingTime += Other.PrepareForSavingTime;
		PrepareForSavingTextureTime += Other.PrepareForSavingTextureTime;
		PrepareForSavingTerrainTime += Other.PrepareForSavingTerrainTime;
		PrepareForSavingMaterialTime += Other.PrepareForSavingMaterialTime;
		PrepareForSavingMaterialInstanceTime += Other.PrepareForSavingMaterialInstanceTime;
		PrepareForSavingStaticMeshTime += Other.PrepareForSavingStaticMeshTime;
		CollectGarbageAndVerifyTime += Other.CollectGarbageAndVerifyTime;
		PrePackageIterationTime += Other.PrePackageIterationTime;
		PackageIterationTime += Other.PackageIterationTime;
		PrepPackageTime += Other.PrepPackageTime;
		TagCookedStartupTime += Other.TagCookedStartupTime;
		MarkCookedStartupTime += Other.MarkCookedStartupTime;
		InitializePackageTime += Other.InitializePackageTime;
		FinalizePackageTime += Other.FinalizePackageTime;
		PostPackageIterationTime += Other.PostPackageIterationTime;
		PersistentFaceFXTime += Other.PersistentFaceFXTime;
		PersistentFaceFXDeterminationTime += Other.PersistentFaceFXDeterminationTime;
		PersistentFaceFXGenerationTime += Other.PersistentFaceFXGenerationTime;
		TFCTextureAlreadySaved += Other.TFCTextureAlreadySaved;
		TFCTextureSaved += Other.TFCTextureSaved;
		ExternGCompressorTime += Other.ExternGCompressorTime;
		ExternGCompressorSrcBytes += Other.ExternGCompressorSrcBytes;
		ExternGCompressorDstBytes += Other.ExternGCompressorDstBytes;
		ExternGArchiveSerializedCompressedSavingTime += Other.ExternGArchiveSerializedCompressedSavingTime;
	}
	void LoadExternalStats()
	{
		ExternGShaderCacheLoadTime = GShaderCacheLoadTime;
		ExternGRHIShaderCompileTime_Total = GRHIShaderCompileTime_Total;
		ExternGRHIShaderCompileTime_PS3 = GRHIShaderCompileTime_PS3;
		ExternGRHIShaderCompileTime_XBOXD3D = GRHIShaderCompileTime_XBOXD3D;
		ExternGRHIShaderCompileTime_NGP = GRHIShaderCompileTime_NGP;
		ExternGRHIShaderCompileTime_PCD3D_SM3 = GRHIShaderCompileTime_PCD3D_SM3;
		ExternGRHIShaderCompileTime_PCD3D_SM5 = GRHIShaderCompileTime_PCD3D_SM5;
		ExternGRHIShaderCompileTime_PCOGL = GRHIShaderCompileTime_PCOGL;

		// Compression stats.
		ExternGCompressorTime = GCompressorTime;
		ExternGCompressorSrcBytes = GCompressorSrcBytes;
		ExternGCompressorDstBytes = GCompressorDstBytes;
		ExternGArchiveSerializedCompressedSavingTime = GArchiveSerializedCompressedSavingTime;

	}
	/**
	 * Serialize function.
	 *
	 * @param	Ar	Archive to serialize with.
	 */
	void Serialize(FArchive& Ar)
	{
		Ar <<  TotalTime;
		Ar <<  AlwaysLoadedProcessingTime;
		Ar <<  ContentProcessingTime;
		Ar <<  CreateIniFilesTime;
		Ar <<  LoadSectionPackagesTime;
		Ar <<  LoadNativePackagesTime;
		Ar <<  LoadDependenciesTime;
		Ar <<  LoadPackagesTime;
		Ar <<  LoadPerMapPackagesTime;
		Ar <<  LoadCombinedStartupPackagesTime;
		Ar <<  CheckDependentPackagesTime;
		Ar <<  ExternGShaderCacheLoadTime;
		Ar <<  SaveShaderCacheTime;
		Ar <<  CopyShaderCacheTime;
		Ar <<  ExternGRHIShaderCompileTime_Total;
		Ar <<  ExternGRHIShaderCompileTime_PS3;
		Ar <<  ExternGRHIShaderCompileTime_XBOXD3D;
		Ar <<  ExternGRHIShaderCompileTime_NGP;
		Ar <<  ExternGRHIShaderCompileTime_PCD3D_SM3;
		Ar <<  ExternGRHIShaderCompileTime_PCD3D_SM5;
		Ar <<  ExternGRHIShaderCompileTime_PCOGL;
		Ar <<  CleanupMaterialsTime;
		Ar <<  CleanupShaderCacheTime;
		Ar <<  CookTime;
		Ar <<  CookPhysicsTime;
		Ar <<  CookTextureTime;
		Ar <<  CookSoundTime;
		Ar <<  CookSoundCueTime;
		Ar <<  LocSoundTime;
		Ar <<  CookMovieTime;
		Ar <<  CookStripTime;
		Ar <<  CookSkeletalMeshTime;
		Ar <<  CookStaticMeshTime;
		Ar <<  CookParticleSystemTime;
		Ar <<  CookParticleSystemDuplicateRemovalTime;
		Ar <<  PackageSaveTime;
		Ar <<  PackageLocTime;
		Ar <<  PrepareForSavingTime;
		Ar <<  PrepareForSavingTextureTime;
		Ar <<  PrepareForSavingTerrainTime;
		Ar <<  PrepareForSavingMaterialTime;
		Ar <<  PrepareForSavingMaterialInstanceTime;
		Ar <<  PrepareForSavingStaticMeshTime;
		Ar <<  CollectGarbageAndVerifyTime;
		Ar <<  PrePackageIterationTime;
		Ar <<  PackageIterationTime;
		Ar <<  PrepPackageTime;
		Ar <<  TagCookedStartupTime;
		Ar <<  MarkCookedStartupTime;
		Ar <<  InitializePackageTime;
		Ar <<  FinalizePackageTime;
		Ar <<  PostPackageIterationTime;
		Ar <<  PersistentFaceFXTime;
		Ar <<  PersistentFaceFXDeterminationTime;
		Ar <<  PersistentFaceFXGenerationTime;
		Ar <<  TFCTextureAlreadySaved;
		Ar <<  TFCTextureSaved;
		Ar <<  ExternGCompressorTime;
		Ar <<  ExternGCompressorSrcBytes;
		Ar <<  ExternGCompressorDstBytes;
		Ar <<  ExternGArchiveSerializedCompressedSavingTime;
	}
};


/**
 * Serialized container class used for mapping a unique name to bulk data info.
 */
class UPersistentCookerData : public UObject
{
	DECLARE_CLASS_INTRINSIC(UPersistentCookerData,UObject,0,UnrealEd);

	/**
	 * Create an instance of this class given a filename. First try to load from disk and if not found
	 * will construct object and store the filename for later use during saving.
	 *
	 * @param	Filename					Filename to use for serialization
	 * @param	bCreateIfNotFoundOnDisk		If FALSE, don't create if couldn't be found; return NULL.
	 * @return								instance of the container associated with the filename
	 */
	static UPersistentCookerData* CreateInstance( const TCHAR* Filename, UBOOL bCreateIfNotFoundOnDisk );
	
	/**
	 * Saves the data to disk.
	 */
	virtual void SaveToDisk();

	/**
	 * Serialize function.
	 *
	 * @param	Ar	Archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Stores the bulk data info after creating a unique name out of the object and name of bulk data
	 *
	 * @param	Object					Object used to create unique name
	 * @param	BulkDataName			Unique name of bulk data within object, e.g. MipLevel_3
	 * @param	CookedBulkDataInfo		Data to store
	 */
	void SetBulkDataInfo( UObject* Object, const TCHAR* BulkDataName, const FCookedBulkDataInfo& CookedBulkDataInfo );
	
	/**
	 * Retrieves previously stored bulk data info data for object/ bulk data array name combination.
	 *
	 * @param	Object					Object used to create unique name
	 * @param	BulkDataName			Unique name of bulk data within object, e.g. MipLevel_3
	 * @return	cooked bulk data info if found, NULL otherwise
	 */
	FCookedBulkDataInfo* GetBulkDataInfo( UObject* Object, const TCHAR* BulkDataName );

	/**
	 * Gathers bulk data infos of objects in passed in Outer.
	 *
	 * @param	Outer					Outer to use for object traversal when looking for bulk data
	 * @param	AndroidTextureFormat 	On Android we will need to know which texture formats to use, if empty, it assumes platform is not Android
	 */
	void GatherCookedBulkDataInfos( UObject* Outer, DWORD AndroidTextureFormat = 0x0);

	/**
	 * Helper function for GatherCookedBulkDataInfo to operate on mip data
	 * 
	 * @param	Texture2D		the Texture to operate on
	 * @param	Mips			Reference to the actual collection of generated mips
	 * @param	TextureFormat	Which format is the mip data is in
	 */
	void GatherCookedMipDataInfos( UTexture2D* Texture2D, TIndirectArray<FTexture2DMipMap> &Mips, ETextureFormatSupport TextureFormat = TEXSUPPORT_DXT );

	/**
	 * Stores any script SHA data that was saved during this session
	 *
	 * @param	PackageName Name of the package that has been saved
	 */
	void CacheScriptSHAData( const TCHAR* PackageName );

	/**
	 * Dumps all bulk info data to the log.
	 */
	void DumpBulkDataInfos();

	/**
	 * Stores texture file cache entry info for the given object
	 *
	 * @param	Object					Object used to create unique name
	 * @param	CookedBulkDataInfo		Data to store
	 */
	void SetTextureFileCacheEntryInfo( UObject* Object, const FCookedTextureFileCacheInfo& CookedTextureFileCacheInfo );

	/**
	 * Retrieves texture file cache entry info data for given object
	 *
	 * @param	Object					Object used to create unique name
	 * @return	texture file cache info entry if found, NULL otherwise
	 */
	FCookedTextureFileCacheInfo* GetTextureFileCacheEntryInfo( UObject* Object );

	/**
	 * Updates texture file cache entry infos by saving TFC file timestamps for each entry
	 *
	 * @param	TextureCacheNameToFilenameMap	Map from texture file cache name to its archive filename
	 */
	void UpdateCookedTextureFileCacheEntryInfos( const TMap<FName,FString> TextureCacheNameToFilenameMap );

	/**
	 * Adds usage entry for texture 
	 *
	 * @param	Package				Package the texture wants to be serialized to.
	 * @param	Texture				Texture that is being used
	 * @param	StoreOnceMipSize	Size in bytes for mips stored just once in special file
	 * @param	DuplicatedMipSize	Size in bytes of mips that are duplicated for every usage
	 * @param	SizeX				Width taking cooked LOD bias into account
	 * @param	SizeY				Height taking cooked LOD bias into account
	 */
	void AddTextureUsageInfo( UPackage* Package, UTexture2D* Texture, INT StoreOnceMipSize, INT DuplicatedMipSize, INT SizeX, INT SizeY ); 

	/**
	 * Retrieves the file time if the file exists, otherwise looks at the cached data.
	 * This is used to retrieve file time of files we don't save out like empty destination files.
	 * 
	 * @param	Filename	Filename to look file age up
	 * @return	Timestamp of file in seconds
	 */
	DOUBLE GetFileTime( const TCHAR* Filename );

	/**
	 * Sets the time of the passed in file.
	 *
	 * @param	Filename		Filename to set file age
	 * @param	FileTime	Time to set
	 */
	void SetFileTime( const TCHAR* Filename, DOUBLE FileTime );

	/**
	 * Retrieves the cooked version of a previously cooked file.
	 * This is used to retrieve the version so we don't have to open the package to get it
	 * 
	 * @param	Filename	Filename to look version up
	 * @return	Cooked version of the file, or 0 if it doesn't exist
	 */
	INT GetFileCookedVersion( const TCHAR* Filename );

	/**
	 * Sets the version of the passed in file.
	 *
	 * @param	Filename		Filename to set version
	 * @param	CookedVersion	Version to set
	 */
	void SetFileCookedVersion( const TCHAR* Filename, INT CookedVersion );

	/**
	 * Retrieves the previously cooked texture formats
	 * 
	 * @param	TextureFormats	The formats to check against
	 * @return	Whether or not TextureFormats matches the saved flags
	 */
	UBOOL UPersistentCookerData::AreTextureFormatsCooked( DWORD TextureFormats );

	/**
	 * Sets the cooked texture formats
	 *
	 * @param	TextureFormats	The formats that have been cooked
	 */
	void UPersistentCookerData::SetCookedTextureFormats( DWORD TextureFormats);

	/**
	 * Sets the texture file cache waste in bytes.
	 *
	 * @param	NewWaste	New waste to set.
	 */
	void SetTextureFileCacheWaste( QWORD NewWaste )
	{
		TextureFileCacheWaste = NewWaste;
	}

	/**
	 * Returns current waste of texture file cache.
	 *
	 * @return waste of texture file cache in bytes.
	 */
	QWORD GetTextureFileCacheWaste()
	{
		return TextureFileCacheWaste;
	}

	/** Sets the path this CookerData object will be saved to. */
	void SetFilename(const TCHAR* InFilename)
	{
		Filename = InFilename;
	}

	/**
	 * Copy the required information from another PCD over the top of our information
	 * Assumed to be used by children cook process 
	 * @param Other PCD containing data to copy
	 */
	virtual void CopyInfo(UPersistentCookerData* Other);

	/**
	 * Copy the cooked always loaded information from another PCD over the top of our information
	 *
	 * @param Other PCD containing always loaded data to copy
	 */
	virtual void CopyCookedStartupObjects(UPersistentCookerData* Other);

	/**
	 * Copy the TFC information from another PCD over the top of our information
	 *
	 * @param Other PCD containing TFC data to copy
	 */
	virtual void CopyTFCInfo(UPersistentCookerData* Other);

	/**
	 * Check the TFC information to see if we need to sync with the parent
	 *
	 * @return TRUE if we need to sync with the parent process
	 */
	virtual UBOOL UPersistentCookerData::NeedsTFCSync();

	/**
	 * Merge another persistent cooker data object into this one and merge TFC
	 *
	 * @param Commendlet Commandlet object that is used to get archive for the base texture file cache
	 * @param OtherDirectory Directory used to find any TextureFileCache's used by the Other cooker data
	 * @param ChildIndex index of the child process we are merging
	 */
	virtual void Merge(UPersistentCookerData* Other, class UCookPackagesCommandlet* Commandlet, const TCHAR* OtherDirectory, INT ChildIndex);

	/**
	 *	Copy errors and warnings from GWarn into the PCD for sending to parent
	 */
	void PackWarningsAndErrorsForParent();

	/**
	 *	Re-emit warnings and errors from child
	 *
	 * @param Prefix  purely for display, prefix for errors and warnings
	 */
	void TelegraphWarningsAndErrorsFromChild(const TCHAR *Prefix);

	/**
	 *	Accessor for CookStats going to parent process or coming from child process
	 *
	 * @return Reference to the embeded cook stats
	 */
	FCookStats& GetCookStats()
	{
		return CookStats;
	}


	/**
	 *	Retrieve the CookedPrefixCommonInfo for the given common package name
	 *
	 *	@param	InCommonName			The name of the common package of interest
	 *	@param	bInCreateIfNotFound		If TRUE, a new info set will be created for the common package if not found
	 *
	 *	@return	FForceCookedInfo*		Pointer to the info if found, NULL if not
	 */
	FForceCookedInfo* GetCookedPrefixCommonInfo(const FString& InCommonName, UBOOL bInCreateIfNotFound = FALSE);

	/**
	 *	Retrieve the forced object list for the given PMap name
	 *
	 *	@param	InPMapName				The name of the PMap of interest
	 *	@param	bInCreateIfNotFound		If TRUE, a new info set will be created for the PMap if not found
	 *
	 *	@return	FForceCookedInfo*		Pointer to the info if found, NULL if not
	 */
	FForceCookedInfo* GetPMapForcedObjectInfo(const FString& InPMapName, UBOOL bInCreateIfNotFound = FALSE);

	/**
	 * @return the FilenameToScriptSHA map
	 */
	const TMap<FString,TArray<BYTE> >& GetFilenameToScriptSHA() const
	{
		return FilenameToScriptSHA;
	}

	/**
	 * Set minimal save mode
	 * @param bInMinimalSave value to set the minimal save flag to
	 */
	void SetMinimalSave(UBOOL bInMinimalSave)
	{
		bMinimalSave = bInMinimalSave;
	}

	/** 
	 *	Get the number of entries in the CookedStartupObjects map
	 *
	 *	@return	INT		The number of entries
	 */
	INT GetCookedStartupObjectsEntryCount()
	{
		return CookedStartupObjects.Num();
	}

	/** 
	 *	Get the map of objects cooked into the given startup (always loaded) package.
	 *
	 *	@param	InPackageName			The name of the always loaded package
	 *	@param	bInCreateIfMissing		If TRUE and the mapping isn't found, create one for it
	 *
	 *	@return	TMap<FString,FString>*	The mapping
	 */
	TMap<FString,FString>* GetCookedAlwaysLoadedMapping(const FString& InPackageName, UBOOL bInCreateIfMissing);

	/** 
	 *	Get the map of objects cooked into the given startup (always loaded) package.
	 *
	 *	@param	InPackageName			The name of the always loaded package
	 *	@param	bInCreateIfMissing		If TRUE and the mapping isn't found, create one for it
	 *
	 *	@return	TMap<FString,FString>*	The mapping
	 */
	TMap<FString,FString>* GetCookedAlwaysLoadedLocMapping(const FString& InPackageName, const FString& InLanguage, UBOOL bInCreateIfMissing);

protected:
	/** Objects cooked into startup (always loaded) packages. Package --> Map of objects cooked into it */
	TMap<FString,TMap<FString,FString> > CookedStartupObjects;
	
	/** Language extension to objects cooked into startup (always loaded) packages. LANG --> Package --> Map of objects cooked into it */
	TMap<FString,TMap<FString,TMap<FString,FString> > > CookedStartupObjectsLoc;

	/** Set of materials that have already been put into an always loaded shader cache.			*/
	TSet<FString> AlreadyHandledStartupMaterials;
	/** Set of material instances that have already been put into an always loaded shader cache. */
	TSet<FString> AlreadyHandledStartupMaterialInstances;

	/** Map from unique name to bulk data info data */
	TMap<FString,FCookedBulkDataInfo> CookedBulkDataInfoMap;

	/** Map from unique texture name to TFC entry info */
	TMap<FString,FCookedTextureFileCacheInfo> CookedTextureFileCacheInfoMap;

	/** Map from filename to file time. */
	TMap<FString,DOUBLE> FilenameToTimeMap;

	/** Map from filename to cooked package version. */
	TMap<FString,INT> FilenameToCookedVersion;

	/** Mapping from fully qualified unique texture name to usage status information */
	TMap<FString,FCookedTextureUsageInfo> TextureUsageInfos;

	/** Mapping from map-prefix common packages to the items they contain. */
	TMap<FString,FForceCookedInfo> CookedPrefixCommonInfoMap;

	/** Mapping from PMap package to the forced items they contain */
	TMap<FString, FForceCookedInfo> PMapForcedObjectsMap;

	/** Map from filename to 20 byte SHA key for script code in package */
	TMap<FString,TArray<BYTE> > FilenameToScriptSHA;

	/** Conduit for child cook warnings to get to the parent */
	TArray<FString> ChildCookWarnings;

	/** Conduit for child cook errors to get to the parent */
	TArray<FString> ChildCookErrors;

	/** Keeps track of how many warnings we have already sent to the parent */
	INT LastSentWarning;
	/** Keeps track of how many errors we have already sent to the parent */
	INT LastSentError;

	/** If true, only save minimal information needed for child<->parent communication */
	UBOOL bMinimalSave;

	/** 
	 * Amount of waste in bytes due to using texture file cache. Can accumulate over time due to the way textures 
	 * are updated in the file cache as we can only replace ones with the same size. An easy fix is to perform a full
	 * recook.
	 */
	QWORD TextureFileCacheWaste;

	/** Which texture formats have been cooked previously */
	DWORD CookedTextureFormats;

	/** Filename to use for serialization. */
	FString	Filename;

	/** Conduit for child cook stats to get to the parent */
	FCookStats CookStats;

	friend class UCookPackagesCommandlet;
	friend class UAnalyzeCookedTextureUsageCommandlet;
	friend class UAnalyzeCookedTextureSingleUsageCommandlet;
	friend class UAnalyzeCookedTextureDXT5UsageCommandlet;
	friend class WxTextureStatsBrowser;
};

/**
 * Helper structure encapsulating mapping from src file to cooked data filename.
 */
struct FPackageCookerInfo
{
	/** Src filename.									*/
	FFilename SrcFilename;
	/** Cooked dst filename.							*/
	FFilename DstFilename;
	/** Whether this package should be made seek free.	*/
	UBOOL bShouldBeSeekFree;
	/** Whether this package is a native script file.	*/
	UBOOL bIsNativeScriptFile;
	/** Whether this packages is a startup package.		*/
	UBOOL bIsCombinedStartupPackage;
	/** Whether this packages is standalone seekfree.	*/
	UBOOL bIsStandaloneSeekFreePackage;
	/** Whether or not to just load the package during cooking, don't save it */
	UBOOL bShouldOnlyLoad;
	/** Whether this package is a non-native script package. */
	UBOOL bIsNonNativeScriptFile;
    /** Other required packages (GFx)                   */
    UBOOL bIsOtherRequired;

	/** Empty constructor */
	FPackageCookerInfo()
		:	bShouldBeSeekFree( FALSE )
		,	bIsNativeScriptFile( FALSE )
		,	bIsCombinedStartupPackage( FALSE )
		,	bIsStandaloneSeekFreePackage( FALSE )
		,	bShouldOnlyLoad( FALSE )
		,   bIsNonNativeScriptFile( FALSE )
		,	bIsOtherRequired( FALSE )
	{

	}

	/** Constructor, initializing member variables with passed in ones */
	FPackageCookerInfo( const TCHAR* InSrcFilename, const TCHAR* InDstFilename, UBOOL InbShouldBeSeekFree, UBOOL InbIsNativeScriptFile, UBOOL InbIsCombinedStartupPackage, UBOOL InbIsStandaloneSeekFreePackage, UBOOL InbIsNonNativeScriptFile, UBOOL InbIsOtherRequired = FALSE)
	:	SrcFilename( InSrcFilename )
	,	DstFilename( InDstFilename )
	,	bShouldBeSeekFree( InbShouldBeSeekFree )
	,	bIsNativeScriptFile( InbIsNativeScriptFile )
	,	bIsCombinedStartupPackage( InbIsCombinedStartupPackage )
	,	bIsStandaloneSeekFreePackage( InbIsStandaloneSeekFreePackage )
	,	bShouldOnlyLoad( FALSE )
	,   bIsNonNativeScriptFile( InbIsNonNativeScriptFile )
    ,	bIsOtherRequired( InbIsOtherRequired )
	{}

	/**
	 * Serialize function.
	 *
	 * @param	Ar	Archive to serialize with.
	 */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << SrcFilename;
		Ar << DstFilename;
		Ar << bShouldBeSeekFree;
		Ar << bIsNativeScriptFile;
		Ar << bIsCombinedStartupPackage;
		Ar << bIsStandaloneSeekFreePackage;
		Ar << bShouldOnlyLoad;
		Ar << bIsNonNativeScriptFile;
        Ar << bIsOtherRequired;
	}

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar, FPackageCookerInfo& CookerInfo)
	{
		return Ar 
			<< CookerInfo.SrcFilename
			<< CookerInfo.DstFilename
			<< CookerInfo.bShouldBeSeekFree
			<< CookerInfo.bIsNativeScriptFile
			<< CookerInfo.bIsCombinedStartupPackage
			<< CookerInfo.bIsStandaloneSeekFreePackage
			<< CookerInfo.bShouldOnlyLoad
			<< CookerInfo.bIsNonNativeScriptFile
            << CookerInfo.bIsOtherRequired
            ;
	}
};

/**
 *	Helper class to handle game-specific cooking. Each game can 
 *	subclass this to handle special-case cooking needs.
 */
class FGameCookerHelperBase
{
public:

	/**
	* Initialize the cooker helpr and process any command line params
	*
	*	@param	Commandlet		The cookpackages commandlet being run
	*	@param	Tokens			Command line tokens parsed from app
	*	@param	Switches		Command line switches parsed from app
	*/
	virtual void Init(
		class UCookPackagesCommandlet* Commandlet, 
		const TArray<FString>& Tokens, 
		const TArray<FString>& Switches )
	{}

	/**
	 *	Create an instance of the persistent cooker data given a filename. 
	 *	First try to load from disk and if not found will construct object and store the 
	 *	filename for later use during saving.
	 *
	 *	The cooker will call this first, and if it returns NULL, it will use the standard
	 *		UPersistentCookerData::CreateInstance function. 
	 *	(They are static hence the need for this)
	 *
	 * @param	Filename					Filename to use for serialization
	 * @param	bCreateIfNotFoundOnDisk		If FALSE, don't create if couldn't be found; return NULL.
	 * @return								instance of the container associated with the filename
	 */
	virtual UPersistentCookerData* CreateInstance( const TCHAR* Filename, UBOOL bCreateIfNotFoundOnDisk );

	/** 
	 *	Generate the package list that is specific for the game being cooked.
	 *
	 *	@param	Commandlet							The cookpackages commandlet being run
	 *	@param	Platform							The platform being cooked for
	 *	@param	ShaderPlatform						The shader platform being cooked for
	 *	@param	NotRequiredFilenamePairs			The package lists being filled in...
	 *	@param	RegularFilenamePairs				""
	 *	@param	MapFilenamePairs					""
	 *	@param	ScriptFilenamePairs					""
	 *	@param	StartupFilenamePairs				""
	 *	@param	StandaloneSeekfreeFilenamePairs		""
	 *	
	 *	@return	UBOOL		TRUE if successful, FALSE is something went wrong.
	 */
	virtual UBOOL GeneratePackageList( 
		class UCookPackagesCommandlet* Commandlet, 
		UE3::EPlatformType Platform,
		EShaderPlatform ShaderPlatform,
		TArray<FPackageCookerInfo>& NotRequiredFilenamePairs,
		TArray<FPackageCookerInfo>& RegularFilenamePairs,
		TArray<FPackageCookerInfo>& MapFilenamePairs,
		TArray<FPackageCookerInfo>& ScriptFilenamePairs,
		TArray<FPackageCookerInfo>& StartupFilenamePairs,
		TArray<FPackageCookerInfo>& StandaloneSeekfreeFilenamePairs)
	{
		return TRUE;
	}

	/**
	 * Cooks passed in object if it hasn't been already.
	 *
	 *	@param	Commandlet					The cookpackages commandlet being run
	 *	@param	Package						Package going to be saved
	 *	@param	Object						Object to cook
	 *	@param	bIsSavedInSeekFreePackage	Whether object is going to be saved into a seekfree package
	 *
	 *	@return	UBOOL						TRUE if the object should continue the 'normal' cooking operations.
	 *										FALSE if the object should not be processed any further.
	 */
	virtual UBOOL CookObject( class UCookPackagesCommandlet* Commandlet, UPackage* Package, 
		UObject* Object, UBOOL bIsSavedInSeekFreePackage )
	{
		return TRUE;
	}

	/** 
	 *	LoadPackageForCookingCallback
	 *	This function will be called in LoadPackageForCooking, allowing the cooker
	 *	helper to handle the package creation as they wish.
	 *
	 *	@param	Commandlet		The cookpackages commandlet being run
	 *	@param	Filename		The name of the package to load.
	 *
	 *	@return	UPackage*		The package generated/loaded
	 *							NULL if the commandlet should load the package normally.
	 */
	virtual UPackage* LoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, const TCHAR* Filename)
	{
		return NULL;
	}

	/** 
	 *	PostLoadPackageForCookingCallback
	 *	This function will be called in LoadPackageForCooking, prior to any
	 *	operations occurring on the contents...
	 *
	 *	@param	Commandlet	The cookpackages commandlet being run
	 *	@param	Package		The package just loaded.
	 *
	 *	@return	UBOOL		TRUE if the package should be processed further.
	 *						FALSE if the cook of this package should be aborted.
	 */
	virtual UBOOL PostLoadPackageForCookingCallback(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
	{
		return TRUE;
	}

	/**
	 *	Clean up the kismet for the given level...
	 *	Remove 'danglers' - sequences that don't actually hook up to anything, etc.
	 *
	 *	@param	Commandlet	The cookpackages commandlet being run
	 *	@param	Package		The package being cooked.
	 */
	virtual void CleanupKismet(class UCookPackagesCommandlet* Commandlet, UPackage* InPackage)
	{
	}

	/**
	 *	Return TRUE if the sound cue should be ignored when generating persistent FaceFX list.
	 *
	 *	@param	InSoundCue		The sound cue of interest
	 *
	 *	@return	UBOOL			TRUE if the sound cue should be ignored, FALSE if not
	 */
	virtual UBOOL ShouldSoundCueBeIgnoredForPersistentFaceFX(class UCookPackagesCommandlet* Commandlet, const USoundCue* InSoundCue)
	{
		return FALSE;
	}

	/**
	 *	Dump out stats specific to the game cooker helper.
	 */
	virtual void DumpStats() {};
};



