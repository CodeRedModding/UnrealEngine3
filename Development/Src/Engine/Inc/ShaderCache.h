/*=============================================================================
	ShaderCache.h: Shader cache definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SHADERCACHE_H__
#define __SHADERCACHE_H__

#include "ShaderManager.h"

/** Information needed to decompress a single shader. */
class FIndividualCompressedShaderInfo
{
public:
	FIndividualCompressedShaderInfo() {}

	FIndividualCompressedShaderInfo(INT InChunkIndex, INT InUncompressedCodeLength, INT InUncompressedCodeOffset) :
		ChunkIndex(InChunkIndex),
		UncompressedCodeLength(InUncompressedCodeLength),
		UncompressedCodeOffset(InUncompressedCodeOffset)
	{
		check(InChunkIndex < USHRT_MAX);
		check(InUncompressedCodeLength < USHRT_MAX);
	}

	friend FArchive& operator<<(FArchive& Ar,FIndividualCompressedShaderInfo& Ref)
	{
		Ar << Ref.ChunkIndex;
		Ar << Ref.UncompressedCodeOffset;
		Ar << Ref.UncompressedCodeLength;
		return Ar;
	}

private:
	/** Index into FTypeSpecificCompressedShaderCode::CodeChunks that this shader's code is in. */
	WORD ChunkIndex;

	/** Length of this shader's uncompressed code. */
	WORD UncompressedCodeLength;

	/** Index of this shader's code into the code chunks uncompressed code. */
	INT UncompressedCodeOffset;

	friend class FCompressedShaderCodeCache;
};

/** Combined code of multiple shaders that were compressed together. */
class FCompressedShaderCodeChunk
{
public:
	FCompressedShaderCodeChunk() :
		UncompressedSize(INDEX_NONE)
	{}

	friend FArchive& operator<<(FArchive& Ar,FCompressedShaderCodeChunk& Ref)
	{
		Ar << Ref.UncompressedSize;
		Ar << Ref.CompressedCode;
		return Ar;
	}

	INT GetCompressedCodeSize() const { return CompressedCode.GetAllocatedSize(); }

private:
	/** Size of the uncompressed chunk. */
	INT UncompressedSize;

	/** Combined compressed bytecode. */
	TArray<BYTE> CompressedCode;

	friend class FCompressedShaderCodeCache;
};

/** Compressed shader code specific to a single shader type. */
class FTypeSpecificCompressedShaderCode
{
public:
	friend FArchive& operator<<(FArchive& Ar,FTypeSpecificCompressedShaderCode& Ref)
	{
		Ar << Ref.CompressedShaderInfos;
		Ar << Ref.CodeChunks;
		return Ar;
	}

private:
	/** Map from shader guid to the information required to decompress that shader. */
	TMap<FGuid, FIndividualCompressedShaderInfo> CompressedShaderInfos;

	/** Code chunks for this shader type that were split apart due to size limits. */
	TArray<FCompressedShaderCodeChunk> CodeChunks;

	friend class FCompressedShaderCodeCache;
	friend FArchive& operator<<(FArchive& Ar,class FCompressedShaderCodeCache& Ref);
};

/** Compressed shader code that corresponds to a single FShaderCache. */
class FCompressedShaderCodeCache : public FDeferredCleanupInterface
{
public:
	FCompressedShaderCodeCache(EShaderPlatform InPlatform) : 
		bIsAlwaysLoaded(FALSE),
		NumRefs(0),
		Platform(InPlatform)
	{}
	~FCompressedShaderCodeCache();

	/** Compresses the compiled bytecode from the input shaders and writes the output to ShaderTypeCompressedShaderCode. */
	void CompressShaderCode(const TMap<FGuid,FShader*>& Shaders, EShaderPlatform Platform);

	/** Decompresses the given shader's code from this compressed cache if possible, and stores the result in UncompressedCode. */
	UBOOL DecompressShaderCode(const FShader* Shader, const FGuid& Id, EShaderPlatform Platform, TArray<BYTE>& UncompressedCode) const;

	/** Returns TRUE if the compressed shader cache contains compressed shader code for the given shader. */
	UBOOL HasShader(const FShader* Shader) const;
	UBOOL IsEmpty() const { return ShaderTypeCompressedShaderCode.Num() == 0; }
	friend FArchive& operator<<(FArchive& Ar,FCompressedShaderCodeCache& Ref);

	// Reference counting.
	void AddRef();
	void Release();

	/** Called by the deferred cleanup mechanism when it is safe to delete this object. */
	virtual void FinishCleanup();

	/** Path name of the UShaderCache this belongs to, only used for debugging. */
	FString CacheName;

	/** Shader Cache priority based on the the shader cache's life span. Lower means it's a longer life span */
	INT ShaderCachePriority;

	/** Flag if this compressed shader cache was loaded by an always loaded cache */
	UBOOL bIsAlwaysLoaded;

	/** Returns the current reference count.  This method should only be used for debug printing. */
	UINT GetRefCount() const { return NumRefs; }

	/** Returns the Compressed Code Size used by this Cache */
	INT GetCompressedCodeSize() const;

private:
	/** The number of references to this cache. */
	mutable UINT NumRefs;

	EShaderPlatform Platform;

	/** Map from shader type to the arrays of compressed shader code for each shader type. */
	TMap<FShaderType*, FTypeSpecificCompressedShaderCode> ShaderTypeCompressedShaderCode;
};

/** A collection of persistent shaders. */
class FShaderCache
{
public:

	/** Initialization constructor. */
	FShaderCache(EShaderPlatform InPlatform = SP_PCD3D_SM3)
	:	Platform(InPlatform)
	{}

	/** Loads the shader cache. */
	void Load(FArchive& Ar);
	
	/** Saves the shader cache. */
	void Save(FArchive& Ar,const TMap<FGuid,FShader*>& Shaders,UBOOL bSavingCookedPackage);

protected:

	/** 
	 * Reference to the compressed shader cache corresponding to this shader cache.
	 * With seekfree loading on consoles, shader caches cooked into seekfree packages will be GC'ed shortly after the package is loaded,
	 * But the FCompressedShaderCodeCache will not be deleted as it will still be referenced by whichever FMaterialShaderMaps came from that shader cache.
	 * If this is the global shader cache, this is the only reference to the global FCompressedShaderCodeCache.
	 */
	TRefCountPtr<FCompressedShaderCodeCache> CompressedCache;

	/** Platform this shader cache is for. */
	BYTE Platform;
};

/** A collection of persistent shaders and material shader maps. */
class UShaderCache : public UObject, public FShaderCache
{
	DECLARE_CLASS_INTRINSIC(UShaderCache,UObject,0,Engine);
	NO_DEFAULT_CONSTRUCTOR(UShaderCache);
public:

	/**
	 * Flushes the shader map for a material from the platform cache.
	 * @param StaticParameters - The static parameter set identifying the material
	 * @param Platform Platform to flush.
	 */
	static void FlushId(const class FStaticParameterSet& StaticParameters, EShaderPlatform Platform);

	/**
	 * Flushes the shader map for a material from all caches.
	 * @param StaticParameters - The static parameter set identifying the material
	 */
	static void FlushId(const class FStaticParameterSet& StaticParameters);

	/**
	 * Combines OtherCache's shaders with this one.  
	 * OtherCache has priority and will overwrite any existing shaders, otherwise they will just be added.
	 *
	 * @param OtherCache	Shader cache to merge
	 */
	void Merge(UShaderCache* OtherCache);

	/**
	 * Adds a material shader map to the cache fragment.
	 *
	 * @param MaterialShaderIndex - The shader map for the material.
	 */
	void AddMaterialShaderMap(class FMaterialShaderMap* MaterialShaderMap);

	/**
	 * Constructor.
	 * @param	Platform	Platform this shader cache is for.
	 */
	UShaderCache( EShaderPlatform Platform );

	// UObject interface.
	virtual void FinishDestroy();
	virtual void PreSave();
	virtual void Serialize(FArchive& Ar);

	// Accessors.
	UBOOL IsDirty() const 
	{ 
		return bDirty; 
	}

	/**
	 * @return	TRUE if there are no shader maps in this cache, FALSE otherwise
	 */
	UBOOL IsEmpty() const
	{
		return MaterialShaderMap.Num() == 0;
	}

	// set the package as clean
	void MarkClean() 
	{
		bDirty = FALSE; 
	}
	// set the package as dirty
	void MarkDirty() 
	{ 
		bDirty = TRUE; 
	}
	// Sets the priority on this shader cache
	void SetPriority(INT Priority)
	{
		ShaderCachePriority = Priority;
	}

	/**
	 * Returns a list of all material shader maps residing in this shader cache.	 
	 */
	TArray<TRefCountPtr<class FMaterialShaderMap> > GetMaterialShaderMap() const;

	/**
	 *	Removes all entries other than the given ones
	 *
	 *	@param	InValidList		Array of valid FStaticParameterSet IDs (the list of IDs for materials that were saved)
	 */
	INT CleanupCacheEntries(TArray<FStaticParameterSet>& InValidList);

private:

	/** Map from material static parameter set to shader map. */
	TMap<FStaticParameterSet,TRefCountPtr<class FMaterialShaderMap> > MaterialShaderMap;

	/**Shader map priority based on shader cache life span. Lower means it's a longer life span*/
	INT ShaderCachePriority;

	/** Whether shader cache has been modified since the last save.	*/
	UBOOL bDirty;

	/** Loads the shader cache. */
	void Load(FArchive& Ar, UBOOL bIsAlwaysLoaded);
	
	/** Saves the shader cache. */
	void Save(FArchive& Ar);
};

/** Array of compressed shader caches.  This can only be read from / written to on the rendering thread. */
extern TArray<const FCompressedShaderCodeCache*> GCompressedShaderCaches[SP_NumPlatforms];

/**
 *	Save the shaders that are mapped for a material
 *
 *	@param	InShaders				The shaders to serialize.
 *	@param	Ar						The archive to serialize them to.
 */
extern void SerializeShaders(const TMap<FGuid,FShader*>& InShaders, FArchive& Ar);

/**
 * Returns the reference shader cache for the passed in platform
 *
 * @param	Platform	Platform to return reference shader cache for.
 * @return	The reference shader cache for the passed in platform
 */
extern UShaderCache* GetReferenceShaderCache( EShaderPlatform Platform );

/**
 * Returns the local shader cache for the passed in platform
 *
 * @param	Platform	Platform to return local shader cache for.
 * @return	The local shader cache for the passed in platform
 */
extern UShaderCache* GetLocalShaderCache( EShaderPlatform Platform );

/**
 * Returns the global shader cache for the passed in platform
 *
 * @param	Platform	Platform to return global shader cache for.
 * @return	The global shader cache for the passed in platform
 */
extern FShaderCache* GetGlobalShaderCache( EShaderPlatform Platform );

/**
 * Saves the local shader cache for the passed in platform.
 *
 * @param	Platform	Platform to save shader cache for.
 * @param	OverrideCacheFilename If non-NULL, then the shader cache will be saved to the given path
 */
extern void SaveLocalShaderCache( EShaderPlatform Platform, const TCHAR* OverrideCacheFilename=NULL );

/** Saves all local shader caches. */
extern void SaveLocalShaderCaches();

/** @return The filename for the local shader cache file on the specified platform. */
extern FString GetLocalShaderCacheFilename( EShaderPlatform Platform );

/** @return The filename for the reference shader cache file on the specified platform. */
extern FString GetReferenceShaderCacheFilename( EShaderPlatform Platform );

/** @return The filename for the global shader cache on the specified platform. */
extern FString GetGlobalShaderCacheFilename(EShaderPlatform Platform);

#endif
