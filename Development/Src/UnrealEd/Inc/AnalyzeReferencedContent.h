/**
 * This is a helper class - FaceFX AnimSet Generator
 * To reduce memory overhead, cooker uses this class to create one animset per one persistent map
 * Otherwise, it will load all animsets referenced by the map. 
 * This creates new animset and copy all anims used by the map to the new one.
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _INC_ANALYZEREFERENCEDCONTENT
#define _INC_ANALYZEREFERENCEDCONTENT

#include "SpeedTree.h"
#include "UnTerrain.h"
#include "EngineDecalClasses.h"
#include "UnDecalRenderData.h"

class FPersistentFaceFXAnimSetGenerator
{
public:
	/** Garbage Collection Function */
	typedef void (*_GarbageCollectFn)();

	/** Default Garbage Collection Function */
	static void DefaultGarbageCollectFn()
	{
		UObject::CollectGarbage(RF_Native);
	}

	/** Constructor */
	FPersistentFaceFXAnimSetGenerator()
	{
		FaceFXMangledNameCount = 0;
		bLogPersistentFaceFXGeneration = FALSE;
		bGeneratePersistentMapAnimSet = FALSE;
		bStripAnimSetReferences = FALSE;
		GarbageCollectFn = DefaultGarbageCollectFn;
		CallerCommandlet = NULL;

		GConfig->GetBool(TEXT("Cooker.FaceFXOptions"), TEXT("bGeneratePersistentMapAnimSet"), bGeneratePersistentMapAnimSet, GEditorIni);
		GConfig->GetBool(TEXT("Cooker.FaceFXOptions"), TEXT("bStripAnimSetReferences"), bStripAnimSetReferences, GEditorIni);
	}

	/** Helper structure for tracking real FaceFX animations to the mangled ones */
	struct FMangledFaceFXInfo
	{
		FString FaceFXAnimSet;
		FString OriginalFaceFXGroupName;
		FString OriginalFaceFXAnimName;
		FString MangledFaceFXGroupName;
		FString MangledFaceFXAnimName;

		FMangledFaceFXInfo()
		{
			appMemzero(this, sizeof(FMangledFaceFXInfo));
		}

		FMangledFaceFXInfo(const FMangledFaceFXInfo& Src)
		{
			FaceFXAnimSet = Src.FaceFXAnimSet;
			OriginalFaceFXGroupName = Src.OriginalFaceFXGroupName;
			OriginalFaceFXAnimName = Src.OriginalFaceFXAnimName;
			MangledFaceFXGroupName = Src.MangledFaceFXGroupName;
			MangledFaceFXAnimName = Src.MangledFaceFXAnimName;
		}

		FMangledFaceFXInfo& operator=(const FMangledFaceFXInfo& Src)
		{
			FaceFXAnimSet = Src.FaceFXAnimSet;
			OriginalFaceFXGroupName = Src.OriginalFaceFXGroupName;
			OriginalFaceFXAnimName = Src.OriginalFaceFXAnimName;
			MangledFaceFXGroupName = Src.MangledFaceFXGroupName;
			MangledFaceFXAnimName = Src.MangledFaceFXAnimName;
			return *this;
		}

		UBOOL operator==(const FMangledFaceFXInfo& Src) const
		{
			if ((FaceFXAnimSet == Src.FaceFXAnimSet) && 
				(OriginalFaceFXGroupName == Src.OriginalFaceFXGroupName) && 
				(OriginalFaceFXAnimName == Src.OriginalFaceFXAnimName) && 
				(MangledFaceFXGroupName == Src.MangledFaceFXGroupName) && 
				(MangledFaceFXAnimName == Src.MangledFaceFXAnimName))
			{
				return TRUE;
			}
			return FALSE;
		}

		UBOOL operator!=(const FMangledFaceFXInfo& Src) const
		{
			return !(*this == Src);
		}
	};

	/** Helper typedef for a string to MangledFaceFXInfo map */
	/** AnimSetRef name --> MangledFaceFXInfo */
	typedef TMap<FString, FMangledFaceFXInfo> TMangledFaceFXMap;
	/** Group.Anim name --> TMangledFaceFXMap */
	typedef TMap<FString, TMangledFaceFXMap> TGroupAnimToMangledFaceFXMap;
	/** PMap name --> TGroupAnimToMangledFaceFXMap */
	typedef TMap<FString, TGroupAnimToMangledFaceFXMap> TPMapToGroupAnimLookupMap;

	typedef TArray<TMangledFaceFXMap> TMangledFaceFXMapArray;

public:
	/************************************************************************/
	/*  Main functions                                                      */
	/************************************************************************/
	/**
	 *	Set the PersistentMap info for the generator
	 *
	 *	@param	InPersistentMapInfo	The persistent map info
	 */
	void SetPersistentMapInfo(FPersistentMapInfo* InPersistentMapInfo)
	{
		PersistentMapInfo = InPersistentMapInfo;
	}

	/** 
	*	Generate the persistent level FaceFX AnimSet.
	*
	*	@param	InPackageFile	The name of the package being loaded
	*	@return	UFaceFXAnimSet*	The generated anim set, or NULL if not the persistent map
	*/
	UFaceFXAnimSet* GeneratePersistentMapFaceFXAnimSet(const FFilename& InPackageFile);

	/************************************************************************/
	/*  Utility functions                                                   */
	/************************************************************************/

	/** Set Caller class information */
	void SetCallerInfo( UCommandlet * Commandlet, _GarbageCollectFn GarbageCollectFn = NULL );

	/** Clear Current Persistent Map Cache */
	void ClearCurrentPersistentMapCache();

	/** See/Get if we should Log or not */
	void SetLogPersistentFaceFXGeneration(UBOOL bValue) { bLogPersistentFaceFXGeneration = bValue;}
	const UBOOL GetLogPersistentFaceFXGeneration() { return bLogPersistentFaceFXGeneration;}

	/** See/Get if we should generate persistent map animset or not*/
	void SetGeneratePersistentMapAnimSet(UBOOL bValue) { bGeneratePersistentMapAnimSet = bValue;}
	const UBOOL GetGeneratePersistentMapAnimSet() { return bGeneratePersistentMapAnimSet;}

	/** See/Get if we should strip FaceFX animset references*/
	void SetStripAnimSetReferences(UBOOL bValue) { bStripAnimSetReferences = bValue;}
	const UBOOL GetStripAnimSetReferences() { return bStripAnimSetReferences;}

	/** Current Cache is valid or not */
	UBOOL IsCurrentPMapCacheValid()
	{
		return ( CurrentPMapGroupAnimLookupMap!=NULL && CurrentPersistentMapFaceFXArray!=NULL );
	}

	/** Should we include this to persistent map or not */
	UBOOL ShouldIncludeToPersistentMapFaceFXAnimation(const FString & FaceFXAnimSetPathName)
	{
		// If script referenced, then please do not include
		return ( ScriptReferencedFaceFXAnimSets.Find(FaceFXAnimSetPathName) == NULL );
	}

	/** Setup scripted referenced animset */
	void SetupScriptReferencedFaceFXAnimSets();

	/** Generated MangledFaceFXInfo  */
	void GenerateMangledFaceFXInformation( FString& PersistentLevelName, FMangledFaceFXInfo& PFFXData );
	TMangledFaceFXMap* GetMangledFaceFXMap(	TGroupAnimToMangledFaceFXMap* GroupAnimToFFXMap,
		FString& GroupName, FString& AnimName, UBOOL bCreateIfNotFound);
	FMangledFaceFXInfo* GetMangledFaceFXMap(FPersistentFaceFXAnimSetGenerator::TMangledFaceFXMap* MangledFFXMap,
		FString& PersistentLevelName, FString& AnimSetRefName, FString& GroupName, FString& AnimName, UBOOL bCreateIfNotFound);


	/** Find the GroupName/AnimName from FaceFXAnimSets. If found, return the Index */
	static INT FindSourceAnimSet(TArrayNoInit<class UFaceFXAnimSet*>& FaceFXAnimSets, FString& GroupName, FString& AnimName);
	/** Find MangledFaceFXInfo of input of AnimSetRefName, GroupName, AnimName from GroupAnimToFFXMap */
	static FMangledFaceFXInfo* FindMangledFaceFXInfo( TGroupAnimToMangledFaceFXMap* GroupAnimToFFXMap, FString& AnimSetRefName, FString& GroupName, FString& AnimName);
	/** This is the Cached TArray of information*/
	TArray<FMangledFaceFXInfo>* CurrentPersistentMapFaceFXArray;
	TGroupAnimToMangledFaceFXMap* CurrentPMapGroupAnimLookupMap;
	FFilename CurrentPersistentMapName;
	FFilename AliasedPersistentMapName;

private:
	/** 
	*	Set Current Persistent Map Cache
	*
	*	@param	PersistentMapName	: Set Current Persistent Map Name to process 
	*	@param	FaceFXArray			: FaceFXArray for current persistent map
	*	@param	GroupAnimLookupMap	: GroupAnimLookupMap for current persistent map
	*	return TRUE if succeed 
	*/
	void SetCurrentPersistentMapCache(const FString& PersistentMapName, const FString& AliasedMapName);

	/** 
	*	Check the given map package for persistent level.
	*	If it is, then generate the corresponding FaceFX animation information.
	*
	*	@param	InPackageFile	The name of the package being loaded
	*	return TRUE if succeed 
	*/
	UBOOL SetupPersistentMapFaceFXAnimation(const FFilename& InPackageFile);

	/** 
	*	Generate the persistent level FaceFX AnimSet from Cached value.
	*
	*	@return	UFaceFXAnimSet*	The generated anim set, or NULL if not the persistent map
	*/
	UFaceFXAnimSet* GeneratePersistentMapFaceFXAnimSetFromCurrentPMapCache();

private: 
	/** The persistant map info helper */
	FPersistentMapInfo* PersistentMapInfo;

	/** Script-references FaceFXAnimSets (so they properly cook out) */
	TMap<FString, INT> ScriptReferencedFaceFXAnimSets;

	/** Mapping of Sublevels to their parent Persistent map */
	TPMapToGroupAnimLookupMap PersistentMapToGroupAnimLookupMap;
	TMap<FString, TArray<FMangledFaceFXInfo>> PersistentMapFaceFXArray;

	/** The current mangled name count */
	INT FaceFXMangledNameCount;

	/** The GroupName concatanation to the mangled group name */
	TMap<FString, FString> GroupNameToMangledMap;

	/** This is the CallerCommandlet and GarbageCollect function */
	UCommandlet * CallerCommandlet;
	_GarbageCollectFn GarbageCollectFn;

	/** 
	*	If TRUE, then log out the generation of persistent facefx...
	*	Pass "-LOGPERSISTENTFACEFX" on the commandline
	*/
	UBOOL bLogPersistentFaceFXGeneration;
	/** TRUE to pull FaceFX anims used in level into the Persistent level itself.				*/
	UBOOL bGeneratePersistentMapAnimSet;
	/** TRUE to strip all FaceFX Anim Set references during cook */
	UBOOL bStripAnimSetReferences;

};

class FAnalyzeReferencedContentStat
{
public:
	//@todo. If you add new object types, make sure to update this enumeration
	//		 as well as the optional command line.
	enum EIgnoreObjectFlags
	{
		IGNORE_StaticMesh				= 0x00000001,
		IGNORE_StaticMeshComponent		= 0x00000002,
		IGNORE_StaticMeshActor			= 0x00000004,
		IGNORE_Texture					= 0x00000008,
		IGNORE_Material					= 0x00000010,
		IGNORE_Particle					= 0x00000020,
		IGNORE_Anim						= 0x00000040, // This includes all animsets/animsequences
		IGNORE_LightingOptimization		= 0x00000080,
		IGNORE_SoundCue					= 0x00000100,
		IGNORE_Brush					= 0x00000200,
		IGNORE_Level					= 0x00000400,
		IGNORE_ShadowMap				= 0x00000800,
		IGNORE_SkeletalMesh				= 0x00001000,
		IGNORE_SkeletalMeshComponent	= 0x00002000, 
		IGNORE_FaceFXAnimSet			= 0x00004000,
		IGNORE_Primitive				= 0x00008000,
		IGNORE_SoundNodeWave			= 0x00010000,
	};

	INT IgnoreObjects;

	// this holds all of the common data for our structs
	typedef TMap<FString,UINT> PerLevelDataMap;
	struct FAssetStatsBase
	{
		/** Mapping from LevelName to the number of instances of this type in that level */
		PerLevelDataMap LevelNameToInstanceCount;

		/** Maps this static mesh was used in.								*/
		TArray<FString> MapsUsedIn;

		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* This function fills up MapsUsedIn and LevelNameToInstanceCount if bAddPerLevelDataMap is TRUE. 
		*
		* @param	LevelPackage	Level Package this object belongs to
		* @param	bAddPerLevelDataMap	Set this to be TRUE if you'd like to collect this stat per level (in the Level folder)
		* 
		*/
		void AddLevelInfo( UPackage* LevelPackage, UBOOL bAddPerLevelDataMap = FALSE );
	};

	/**
	* Encapsulates gathered stats for a particular UStaticMesh object.
	*/
	struct FStaticMeshStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members. */
		FStaticMeshStats( UStaticMesh* StaticMesh );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
		FString ToCSV( const FString& LevelName ) const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FStaticMeshStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Number of static mesh instances overall.									*/
		INT	NumInstances;
		/** Triangle count of mesh.														*/
		INT	NumTriangles;
		/** Section count of mesh.														*/
		INT NumSections;
		/** Number of convex hulls in the collision geometry of mesh.					*/
		INT NumConvexPrimitives;
		/** Does this static mesh use simple collision */
		INT bUsesSimpleRigidBodyCollision;
		/** Number of sections that have collision enabled                              */
		INT NumElementsWithCollision;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Whether resource is referenced by particle system                           */
		UBOOL bIsReferencedByParticles;
		/** Resource size of static mesh.												*/
		INT	ResourceSize;
		/** Is this mesh scaled non-uniformly in a level								*/
		UBOOL bIsMeshNonUniformlyScaled;
		/** Does this mesh have box collision that should be converted					*/
		UBOOL bShouldConvertBoxColl;
		/** Array of different scales that this mesh is used at							*/
		TArray<FVector> UsedAtScales;
		/** Does this static mesh strip kDOPTree.										*/
		UBOOL bUsesStripkDOPForConsole;
	};

	/**
	* Encapsulates gathered stats for a particular USkeletalMesh object.
	*/
	struct FSkeletalMeshStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members. */
		FSkeletalMeshStats( USkeletalMesh* SkeletalMesh );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
		FString ToCSV( const FString& LevelName ) const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FSkeletalMeshStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Number of Skeletal mesh instances overall.									*/
		INT	NumInstances;
		/** Triangle count of mesh.														*/
		INT	NumTriangles;
		/** Vertex count of mesh.														*/
		INT	NumVertices;
		/** Vertex buffer size of Skeletal mesh.												*/
		INT	VertexMemorySize;
		/** Index buffer size of Skeletal mesh.												*/
		INT	IndexMemorySize;
		/** Rigid vertex count of mesh.													*/
		INT	NumRigidVertices;
		/** Soft vertex count of mesh.													*/
		INT	NumSoftVertices;
		/** Section count of mesh.														*/
		INT NumSections;
		/** Chunk count of mesh.														*/
		INT NumChunks;
		/** Vertice Influences Data of Skeletal mesh.												*/
		INT	VertexInfluencsSize;
		/** Max bone influences of mesh.												*/
		INT MaxBoneInfluences;
		/** Active bone index count of mesh.											*/
		INT NumActiveBoneIndices;
		/** Required bone count of mesh.												*/
		INT NumRequiredBones;
		/** Number of materials applied to the mesh.									*/
		INT NumMaterials;
		/** Does this skeletal mesh use per-poly bone collision							*/
		UBOOL bUsesPerPolyBoneCollision;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Whether resource is referenced by particle system                           */
		UBOOL bIsReferencedByParticles;
		/** Resource size of Skeletal mesh.												*/
		INT	ResourceSize;
	};

	/**
	* Encapsulates gathered stats for a particular UModelComponent, UDecallComponent, UTerrainComponent, USpeedTreeComponent object.
	*/
	struct FPrimitiveStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members. */
		FPrimitiveStats( UModel* Model );
		FPrimitiveStats( UDecalComponent* DecalComponent );
		FPrimitiveStats( ATerrain* Terrain );
		FPrimitiveStats( USpeedTreeComponent * SpeedTreeComponent );
		FPrimitiveStats( USkeletalMesh * SkeletalMesh, FAnalyzeReferencedContentStat::FSkeletalMeshStats * SkeletalMeshStats );
		FPrimitiveStats( UStaticMesh* SkeletalMesh, FAnalyzeReferencedContentStat::FStaticMeshStats * StaticMeshStats );

		UINT	GetNumTrianglesForTerrain( UDecalComponent * DecalComponent, UTerrainComponent * TerrainComponent, UINT DefaultNumTriangle );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
		FString ToCSV( const FString& LevelName ) const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FPrimitiveStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Number of static mesh instances overall.									*/
		INT	NumInstances;
		/** Triangle count of mesh.														*/
		INT	NumTriangles;
		/** Section count of mesh.														*/
		INT NumSections;
		/** Resource size of static mesh.												*/
		INT	ResourceSize;
	};

	/**
	* Encapsulates gathered stats for a particular UShadowMap1D object.
	*/
	struct FShadowMap1DStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FShadowMap1DStats(UShadowMap1D* ShadowMap1D);
		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}
		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Size in Bytes of the resource												*/
		INT ResourceSize;
		/** Number of Vertex samples used for the 1D shadowmap							*/
		INT NumSamples;		
		/** Light that caused the creation of the 1D shadowmap							*/
		FString UsedByLight;
	};

	/**
	* Encapsulates gathered stats for a particular UShadowMap2D object.
	*/
	struct FShadowMap2DStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FShadowMap2DStats(UShadowMap2D* ShadowMap2D);
		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}
		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** The texture which contains the shadow-map data.								*/
		FString ShadowMapTexture2D;
		/** size of shadow map texture X												*/
		INT ShadowMapTexture2DSizeX;
		/** size of shadow map texture Y												*/
		INT ShadowMapTexture2DSizeY;
		/** format of shadow map texture 												*/
		FString ShadowMapTexture2DFormat;
		/** Light that caused the creation of the 1D shadowmap							*/
		FString UsedByLight;
	};	

	/**
	* Encapsulates gathered stats for a particular UTexture object.
	*/
	struct FTextureStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FTextureStats( UTexture* Texture );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FTextureStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Map of materials this textures is being used by.							*/
		TMap<FString,INT> MaterialsUsedBy;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Resource size of texture.													*/
		INT	ResourceSize;
		/** LOD bias.																	*/
		INT	LODBias;
		/** LOD group.																	*/
		INT LODGroup;
		/** Texture pixel format.														*/
		FString Format;
	};

	/**
	* Encapsulates gathered stats for a particular UMaterial object.
	*/
	struct FMaterialStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FMaterialStats( UMaterial* Material );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FMaterialStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Number of BSP surfaces this material is applied to.							*/
		INT	NumBrushesAppliedTo;
		/** Number of static mesh instances this material is applied to.				*/
		INT NumStaticMeshInstancesAppliedTo;
		/** Map of static meshes this material is used by.								*/
		TMap<FString,INT> StaticMeshesAppliedTo;
		/** Number of skeletal mesh instances this material is applied to.				*/
		INT NumSkeletalMeshInstancesAppliedTo;
		/** Map of skeletal meshes this material is used by.							*/
		TMap<FString,INT> SkeletalMeshesAppliedTo;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Array of textures used. Also implies count.									*/
		TArray<FString>	TexturesUsed;
		/** Number of texture samples made to render this material.						*/
		INT NumTextureSamples;

		/** Number of shader description/instruction									*/
		TArray<FString> ShaderDescriptions;
		TArray<INT>		InstructionCounts;

		/** Keeps one list of all unique shader description								*/
		static TArray<FString> ShaderUniqueDescriptions;					
#if 0 
		/** Max depth of dependent texture read chain.									*/
		INT MaxTextureDependencyLength;
		/** RepresentativeInstructionCounts												*/
		INT NumRepresentativeInstructionCounts;
		/** Translucent instruction count.												*/
		INT NumInstructionsTranslucent;
		/** Additive instruction count.													*/
		INT NumInstructionsAdditive;
		/** Modulate instruction count.													*/
		INT NumInstructionsModulate;
		/** Base pass no lightmap instruction count.										*/
		INT NumInstructionsBasePassNoLightmap;
		/** Base pass with vertex lightmap instruction count.							*/
		INT NumInstructionsBasePassAndLightmap;
		/** Point light with shadow map instruction count.								*/
		INT NumInstructionsPointLightWithShadowMap;
#endif
		/** Resource size of all referenced/ used textures.								*/
		INT ResourceSizeOfReferencedTextures;
		/** Blend Mode .								*/
		EBlendMode BlendMode;
		/** Use Shared texture input between diffuse and specular.						*/
		UBOOL UseUniqueSpecularTexture;

		static void SetupShaders();

		/** Shader type for base pass pixel shader (no lightmap).  */
		static FShaderType*	ShaderTypeBasePassNoLightmap;
		/** Shader type for base pass pixel shader (including lightmap). */
		static FShaderType*	ShaderTypeBasePassAndLightmap;
		/** Shader type for point light with shadow map pixel shader. */
		static FShaderType*	ShaderTypePointLightWithShadowMap;
		/** Examine flag - if this is TRUE, then it will compile material to get parameter information*/
		static UBOOL		TestUniqueSpecularTexture;
	};

	/**
	* Encapsulates gathered stats for a particular UParticleSystem object
	*/
	struct FParticleStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FParticleStats( UParticleSystem* ParticleSystem );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const;

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FParticleStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;	
		/** Number of emitters in this system.											*/
		INT NumEmitters;
		/** Combined number of modules in all emitters used.							*/
		INT NumModules;
		/** Combined number of peak particles in system.								*/
		INT NumPeakActiveParticles;
		/** Combined number of collision modules across emitters                        */
		INT NumEmittersUsingCollision;
		/** Combined number of emitters that have active physics                        */
		INT NumEmittersUsingPhysics;
		/** Maximum number of particles drawn per frame                                 */
		INT MaxNumDrawnPerFrame;
		/** Ratio of particles simulated to particles drawn                             */
		FLOAT PeakActiveToMaxDrawnRatio;
		/** This is the size in bytes that this Particle System will use                */
		INT NumBytesUsed;
		/** if any modules/emitters use distortion material */
		UBOOL bUsesDistortionMaterial;
		/** if any modules/emitters use scene color material */
		UBOOL bUsesSceneTextureMaterial;
		/** materials that use distortion */
		TArray<FString> DistortMaterialNames;
		/** materials that use scene color */
		TArray<FString> SceneColorMaterialNames;
		/** If any modules have mesh emitters that have DoCollision == TRUE wich is more than likely bad and perf costing */
		UBOOL bMeshEmitterHasDoCollisions;
		/** If any modules have mesh emitters that have DoCollision == TRUE wich is more than likely bad and perf costing */
		UBOOL bMeshEmitterHasCastShadows;
		/** If the particle system has warm up time greater than N seconds**/
		FLOAT WarmUpTime;
		/** If any of the emitters are PhysX emitters == TRUE */
		UBOOL bHasPhysXEmitters;
	};

	/**
	* Encapsulates gathered textures-->particle systems information for all particle systems
	*/
	struct FTextureToParticleSystemStats : public FAssetStatsBase
	{
		FTextureToParticleSystemStats(UTexture* InTexture);
		void AddParticleSystem(UParticleSystem* InParticleSystem);

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		const INT GetParticleSystemsContainedInCount() const
		{
			return ParticleSystemsContainedIn.Num();
		}

		FString GetParticleSystemContainedIn(INT Index) const
		{
			if ((Index >= 0) && (Index < ParticleSystemsContainedIn.Num()))
			{
				return ParticleSystemsContainedIn(Index);
			}

			return TEXT("*** INVALID ***");
		}

	protected:
		/** Texture name.										*/
		FString TextureName;
		/** Texture size.										*/
		FString TextureSize;
		/** Texture pixel format.								*/
		FString Format;
		TArray<FString> ParticleSystemsContainedIn;
	};

	struct FFaceFXAnimSetStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FFaceFXAnimSetStats( UFaceFXAnimSet* FaceFXAnimSet);

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;

		/** This takes a LevelName and then looks for the number of Instances of this AnimStat used within that level **/
		FString ToCSV( const FString& LevelName ) const;

		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FFaceFXAnimSetStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;	
		/** Size in bytes of this Animset.												*/
		INT	ResourceSize;
		/** Name of Group.																*/
		FString GroupName;
		/** Number of Animations in the Group.											*/
		INT NumberOfAnimations;
	};

	struct FAnimSequenceStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FAnimSequenceStats( UAnimSequence* Sequence );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;

		/** This takes a LevelName and then looks for the number of Instances of this AnimStat used within that level **/
		FString ToCSV( const FString& LevelName ) const;

		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FAnimSequenceStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Animset name.																*/
		FString AnimSetName;
		/** Animation Tag.																*/
		FString AnimTag;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Whether resource is forced to be uncompressed by human action               */
		UBOOL bMarkedAsDoNotOverrideCompression;
		/** Type of compression used on this animation.									*/
		enum AnimationCompressionFormat TranslationFormat;
		/** Type of compression used on this animation.									*/
		enum AnimationCompressionFormat RotationFormat;
		/** Name of compression algo class used. */
		FString CompressionScheme;
		/** Size in bytes of this animation. */
		INT	AnimationResourceSize;
		/** Percentage(0-100%) of compress ratio*/
		INT CompressionRatio;
		/** Total Tracks in this animation. */
		INT	TotalTracks;
		/** Total Tracks with no animated translation. */
		INT NumTransTracksWithOneKey;
		/** Total Tracks with no animated rotation. */
		INT NumRotTracksWithOneKey;
		/** Size in bytes of this animation's track table. */
		INT	TrackTableSize;
		/** total translation keys. */
		INT TotalNumTransKeys;
		/** total rotation keys. */
		INT TotalNumRotKeys;
		/** Average size of a single translation key, in bytes. */
		FLOAT TranslationKeySize;
		/** Average size of a single rotation key, in bytes. */
		FLOAT RotationKeySize;
		/** Size of the overhead that isn't directly key data (includes data that is required to reconstruct keys, so it's not 'waste'), in bytes */
		INT OverheadSize;
		/** Total Frames in this animation. */
		INT	TotalFrames;

		enum EAnimReferenceType
		{
			ART_SkeletalMeshComponent, // Regular SkeletalMeshComponent - mostly from script
			ART_Matinee, // From Matinee, cinematic animations
			ART_Crowd, // From Crowd spawner, expected to be none or very small
		};

		/** Reference Type **/
		EAnimReferenceType ReferenceType;

	};

	struct FLightingOptimizationStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members */
		FLightingOptimizationStats( AStaticMeshActor* StaticMeshActor );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*   Calculate the memory required to light a mesh with given NumVertices using vertex lighting
		*/
		static INT CalculateVertexLightingBytesUsed(INT NumVertices);

		/** Assuming DXT1 lightmaps...
		*   4 bits/pixel * width * height = Highest MIP Level * 1.333 MIP Factor for approx usage for a full mip chain
		*   Either 1 or 3 textures if we're doing simple or directional (3-axis) lightmap
		*   Most lightmaps require a second UV channel which is probably an extra 4 bytes (2 floats compressed to SHORT) 
		*/
		static INT CalculateLightmapLightingBytesUsed(INT Width, INT Height, INT NumVertices, INT UVChannelIndex);

		/** 
		*	For a given list of parameters, compute a full spread of potential savings values using vertex light, or 256, 128, 64, 32 pixel square light maps
		*  @param LMType - Current type of lighting being used
		*  @param NumVertices - Number of vertices in the given mesh
		*  @param Width - Width of current lightmap
		*  @param Height - Height of current lightmap
		*  @param TexCoordIndex - channel index of the uvs currently used for lightmaps
		*  @param LOI - A struct to be filled in by the function with the potential savings
		*/
		static void CalculateLightingOptimizationInfo(ELightMapInteractionType LMType, INT NumVertices, INT Width, INT Height, INT TexCoordIndex, FLightingOptimizationStats& LOStats);

		static const INT NumLightmapTextureSizes = 4;
		static const INT LightMapSizes[NumLightmapTextureSizes];

		/** Name of the Level this StaticMeshActor is on */
		FString						LevelName;
		/** Name of the StaticMeshActor this optimization is for */
		FString						ActorName;
		/** Name of the StaticMesh belonging to the above StaticMeshActor */
		FString						SMName;
		/** Current type of lighting scheme used */
		ELightMapInteractionType    IsType;        
		/** Texture size of the current lighting scheme, if texture, 0 otherwise */
		INT                         TextureSize;   
		/** Amount of memory used by the current lighting scheme */
		INT							CurrentBytesUsed;
		/** Amount of memory savings for each lighting scheme (256,128,64,32 pixel lightmaps + vertex lighting) */
		INT							BytesSaved[NumLightmapTextureSizes + 1];
	};

	/**
	* Encapsulates gathered stats for a particular USoundCue object.
	*/
	struct FSoundCueStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members. */
		FSoundCueStats( USoundCue* SoundCue );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
		FString ToCSV( const FString& LevelName ) const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FSoundCueStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** FaceFX anim  name.															*/
		FString FaceFXAnimName;
		/** FaceFX group name.															*/
		FString FaceFXGroupName;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Resource size of static mesh.												*/
		INT	ResourceSize;
	};


	/**
	* Encapsulates gathered stats for a particular USoundCue object.
	*/
	struct FSoundNodeWaveStats : public FAssetStatsBase
	{
		/** Constructor, initializing all members. */
		FSoundNodeWaveStats( USoundNodeWave* SoundNodeWave );

		/**
		* Stringifies gathered stats in CSV format.
		*
		* @return comma separated list of stats
		*/
		FString ToCSV() const;
		/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
		FString ToCSV( const FString& LevelName ) const;
		/** @return TRUE if this asset type should be logged */
		UBOOL ShouldLogStat() const
		{
			return TRUE;
		}

		/**
		* Returns a header row for CSV
		*
		* @return comma separated header row
		*/
		static FString GetCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary CSV header row
		*/
		static FString GetSummaryCSVHeaderRow();

		/**
		*	This is for summary
		*	@return command separated summary data row
		*/
		static FString ToSummaryCSV(const FString& LevelName, const TMap<FString,FSoundNodeWaveStats>& StatsData);

		/** Resource type.																*/
		FString ResourceType;
		/** Resource name.																*/
		FString ResourceName;
		/** Whether resource is referenced by script.									*/
		UBOOL bIsReferencedByScript;
		/** Resource size of static mesh.												*/
		INT	ResourceSize;
	};

	//@todo: this code is in dire need of refactoring

	/**
	* Retrieves/ creates material stats associated with passed in material.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	Material	Material to retrieve/ create material stats for
	* @return	pointer to material stats associated with material
	*/
	FMaterialStats* GetMaterialStats( UMaterial* Material );

	/**
	* Retrieves/ creates texture stats associated with passed in texture.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	Texture		Texture to retrieve/ create texture stats for
	* @return	pointer to texture stats associated with texture
	*/
	FTextureStats* GetTextureStats( UTexture* Texture );

	/**
	* Retrieves/ creates static mesh stats associated with passed in static mesh.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	StaticMesh	Static mesh to retrieve/ create static mesh stats for
	* @return	pointer to static mesh stats associated with static mesh
	*/
	FStaticMeshStats* GetStaticMeshStats( UStaticMesh* StaticMesh, UPackage* LevelPackage );

	/**
	* Retrieves/ creates static mesh stats associated with passed in primitive stats.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	Object	Object to retrieve/ create primitive stats for
	* @return	pointer to primitive stats associated with object (ModelComponent, DecalComponent, SpeedTreeComponent, TerrainComponent)
	*/
	FPrimitiveStats* GetPrimitiveStats( UObject* Object, UPackage* LevelPackage );

	/**
	* Retrieves/ creates skeletal mesh stats associated with passed in skeletal mesh.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	SkeletalMesh	Skeletal mesh to retrieve/ create skeletal mesh stats for
	* @return	pointer to skeletal mesh stats associated with skeletal mesh
	*/
	FSkeletalMeshStats* GetSkeletalMeshStats( USkeletalMesh* SkeletalMesh, UPackage* LevelPackage );

	/**
	* Retrieves/ creates particle stats associated with passed in particle system.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	ParticleSystem	Particle system to retrieve/ create static mesh stats for
	* @return	pointer to particle system stats associated with static mesh
	*/
	FParticleStats* GetParticleStats( UParticleSystem* ParticleSystem );

	/**
	* Retrieves/creates texture in particle system stats associated with the passed in texture.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	InTexture	The texture to retrieve/create stats for
	* @return	pointer to textureinparticlesystem stats
	*/
	FTextureToParticleSystemStats* GetTextureToParticleSystemStats(UTexture* InTexture);

	/**
	* Retrieves/ creates animation sequence stats associated with passed in animation sequence.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	AnimSequence	Anim sequence to retrieve/ create anim sequence stats for
	* @return	pointer to particle system stats associated with anim sequence
	*/
	FAnimSequenceStats* GetAnimSequenceStats( UAnimSequence* AnimSequence );

	/**
	* Retrieves/ creates animation sequence stats associated with passed in animation sequence.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	FaceFXAnimSet	FaceFXAnimSet to retrieve/ create anim sequence stats for
	* @return	pointer to particle system stats associated with anim sequence
	*/
	FFaceFXAnimSetStats* GetFaceFXAnimSetStats( UFaceFXAnimSet* FaceFXAnimSet );

	/**
	* Retrieves/ creates lighting optimization stats associated with passed in static mesh actor.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	ActorComponent	Actor component to calculate potential light map savings stats for
	* @return	pointer to lighting optimization stats associated with this actor component
	*/
	FLightingOptimizationStats* GetLightingOptimizationStats( AStaticMeshActor* ActorComponent );

	/**
	* Retrieves/ creates sound cue stats associated with passed in sound cue.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
	* @return				pointer to sound cue  stats associated with sound cue  
	*/
	FSoundCueStats* GetSoundCueStats( USoundCue* SoundCue, UPackage* LevelPackage );

	/**
	* Retrieves/ creates sound cue stats associated with passed in sound cue.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
	* @return				pointer to sound cue  stats associated with sound cue  
	*/
	FSoundNodeWaveStats* GetSoundNodeWaveStats( USoundNodeWave* SoundNodeWave, UPackage* LevelPackage );

	/**
	* Retrieves/ creates shadowmap 1D stats associated with passed in shadowmap 1D object.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
	* @return				pointer to sound cue  stats associated with sound cue  
	*/
	FShadowMap1DStats* GetShadowMap1DStats( UShadowMap1D* ShadowMap1D, UPackage* LevelPackage );

	/**
	* Retrieves/ creates shadowmap 2D stats associated with passed in shadowmap 2D object.
	*
	* @warning: returns pointer into TMap, only valid till next time Set is called
	*
	* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
	* @return				pointer to sound cue  stats associated with sound cue  
	*/
	FShadowMap2DStats* GetShadowMap2DStats( UShadowMap2D* ShadowMap2D, UPackage* LevelPackage );

	/** Mapping from a fully qualified resource string (including type) to static mesh stats info.								*/
	TMap<FString,FStaticMeshStats> ResourceNameToStaticMeshStats;
	/** Mapping from a fully qualified resource string (including type) to other primitive stats info - excluding staticmeshes/skeletalmeshes.	*/
	TMap<FString,FPrimitiveStats> ResourceNameToPrimitiveStats;
	/** Mapping from a fully qualified resource string (including type) to skeletal mesh stats info.								*/
	TMap<FString,FSkeletalMeshStats> ResourceNameToSkeletalMeshStats;
	/** Mapping from a fully qualified resource string (including type) to texture stats info.									*/
	TMap<FString,FTextureStats> ResourceNameToTextureStats;
	/** Mapping from a fully qualified resource string (including type) to material stats info.									*/
	TMap<FString,FMaterialStats> ResourceNameToMaterialStats;
	/** Mapping from a fully qualified resource string (including type) to particle stats info.									*/
	TMap<FString,FParticleStats> ResourceNameToParticleStats;
	/** Mapping from a full qualified resource string (including type) to texutreToParticleSystem stats info					*/
	TMap<FString,FTextureToParticleSystemStats> ResourceNameToTextureToParticleSystemStats;
	/** Mapping from a fully qualified resource string (including type) to anim stats info.										*/
	TMap<FString,FAnimSequenceStats> ResourceNameToAnimStats;																
	/** Mapping from a fully qualified resource string (including type) to facefx stats info.									*/
	TMap<FString,FFaceFXAnimSetStats> ResourceNameToFaceFXAnimSetStats;																
	/** Mapping from a fully qualified resource string (including type) to lighting optimization stats info.					*/
	TMap<FString,FLightingOptimizationStats> ResourceNameToLightingStats;
	/** Mapping from a fully qualified resource string (including type) to sound cue stats info.								*/
	TMap<FString,FSoundCueStats> ResourceNameToSoundCueStats;
	/** Mapping from a fully qualified resource string (including type) to sound cue stats info.								*/
	TMap<FString,FSoundNodeWaveStats> ResourceNameToSoundNodeWaveStats;
	/** Mapping from a fully qualified resource string (including type) to shadowmap 1D.										*/
	TMap<FString,FShadowMap1DStats> ResourceNameToShadowMap1DStats;
	/** Mapping from a fully qualified resource string (including type) to shadowmap 2D.										*/
	TMap<FString,FShadowMap2DStats> ResourceNameToShadowMap2DStats;

	void	SetIgnoreObjectFlag( INT IgnoreObjectFlag ) { IgnoreObjects = IgnoreObjectFlag; }
	INT		GetIgnoreObjectFlag()	{ return IgnoreObjects;		}
	UBOOL	InIgnoreObjectFlag( INT IgnoreObjectFlag ) { return (IgnoreObjects & IgnoreObjectFlag); }

	void WriteOutAllAvailableStatData( const FString& CSVDirectory );

	/** This will write out the specified Stats to the AnalyzeReferencedContentCSVs dir **/
	template< typename STAT_TYPE >
	static void WriteOutCSVs( const TMap<FString,STAT_TYPE>& StatsData, const FString& CSVDirectory, const FString& StatsName );

	/** This will write out the summary Stats to the AnalyzeReferencedContentCSVs dir **/
	template< typename STAT_TYPE >
	static void WriteOutSummaryCSVs( const TMap<FString,STAT_TYPE>& StatsData, const FString& CSVDirectory, const FString& StatsName );

	template< typename STAT_TYPE >
	static void WriteOutCSVsPerLevel( const TMap<FString,STAT_TYPE>& StatsData, const FString& CSVDirectory, const FString& StatsName );

	template< typename STAT_TYPE >
	static INT GetTotalCountPerLevel( const TMap<FString,STAT_TYPE>& StatsData, const FString& LevelName );

	void WriteOutSummary(const FString& CSVDirectory);

	// include only map list that has been loaded
	TArray<FString> MapFileList;
};

BEGIN_COMMANDLET(AnalyzeReferencedContent,UnrealEd)

void StaticInitialize();

/**
* Handles encountered object, routing to various sub handlers.
*
* @param	Object			Object to handle
* @param	LevelPackage	Currently loaded level package, can be NULL if not a level
* @param	bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleObject( UObject* Object, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in static mesh.
*
* @param StaticMesh	StaticMesh to gather stats for.
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleStaticMesh( UStaticMesh* StaticMesh, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in static mesh component.
*
* @param StaticMeshComponent	StaticMeshComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleStaticMeshComponent( UStaticMeshComponent* StaticMeshComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles special case for stats for passed in static mesh component who is part of a ParticleSystemComponent
*
* @param ParticleSystemComponent	ParticleSystemComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleStaticMeshOnAParticleSystemComponent( UParticleSystemComponent* ParticleSystemComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in skeletal mesh.
*
* @param SkeletalMesh	SkeletalMesh to gather stats for.
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleSkeletalMesh( USkeletalMesh* SkeletalMesh, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in skeletal mesh component.
*
* @param SkeletalMeshComponent	SkeletalMeshComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleSkeletalMeshComponentForSMC( USkeletalMeshComponent* SkeletalMeshComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in ModelComponent.
*
* @param ModelComponent	ModelComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleModelComponent( UModelComponent* ModelComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in DecalComponent.
*
* @param DecalComponent	DecalComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleDecalComponent( UDecalComponent* DecalComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in TerrainComponent.
*
* @param TerrainComponent	TerrainComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleTerrainComponent( UTerrainComponent* TerrainComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in SpeedTreeComponent.
*
* @param SpeedTreeComponent	SpeedTreeComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleSpeedTreeComponent( USpeedTreeComponent* SpeedTreeComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in material.
*
* @param Material		Material to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleMaterial( UMaterial* Material, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in texture.
*
* @param Texture		Texture to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleTexture( UTexture* Texture, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in brush.
*
* @param Brush			Brush to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleBrush( ABrush* Brush, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in particle system.
*
* @param ParticleSystem	Particle system to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleParticleSystem( UParticleSystem* ParticleSystem, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/////////////////////////////
// ANIM DATA
/////////////////////////////
/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSequence		AnimSequence to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
* @param ReferenceType : EAnimReferencetype 0-Skeletalmeshcomponent, 1-Matinee, 2-Flocking
*/
void HandleAnimSetInternal( UAnimSet* AnimSet, UPackage* LevelPackage, UBOOL bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType ReferenceType );

/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSequence		AnimSequence to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
* @param ReferenceType : EAnimReferenceType 0-Skeletalmeshcomponent, 1-Matinee, 2-Flocking
*/
void HandleAnimSequenceInternal( UAnimSequence* AnimSequence, UPackage* LevelPackage, UBOOL bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType ReferenceType );

/**
* Handles gathering stats for passed in animation sequence.
*
* @param InterpTrackAnimControl		InterpTrackAnimControl to gather stats for
* @param LevelPackage				Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced		Whether object is handled because there is a script reference
*/
void HandleInterpTrackAnimControl( UInterpTrackAnimControl* InterpTrackAnimControl, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSet	InterpTrackAnimControl to gather stats for
* @param LevelPackage				Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced		Whether object is handled because there is a script reference
*/
void HandleInterpGroup( UInterpGroup* InterpGroup, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in skeletal mesh component.
*
* @param SkeletalMeshComponent	SkeletalMeshComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleSkeletalMeshComponentForAnim( USkeletalMeshComponent* SkeletalMeshComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/////////////////////////////
// FACEFX
/////////////////////////////
/**
* Handles gathering stats for passed in FaceFxAnimSet.
*
* @param FaceFXAnimSet				FaceFXAnimSet to gather stats for
* @param LevelPackage				Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced		Whether object is handled because there is a script reference
*/
void HandleFaceFXAnimSet( UFaceFXAnimSet* FaceFXAnimSet, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in level.
*
* @param Level				Level to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleLevel( ULevel* Level, UPackage* LevelPackage, UBOOL bIsScriptReferenced );
/**
* Handles gathering stats for passed in static actor component.
*
* @param Level				Level to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void HandleStaticMeshActor( AStaticMeshActor* ActorComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in sound cue.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void HandleSoundCue( USoundCue* SoundCue, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in sound cue.
*
* @param SoundNodeWave			SoundNodeWave to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void HandleSoundNodeWave( USoundNodeWave* SoundNodeWave, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in shadow map 1D.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void HandleShadowMap1D( UShadowMap1D* ShadowMap1D, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

/**
* Handles gathering stats for passed in shadow map 2D.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void HandleShadowMap2D( UShadowMap2D* ShadowMap2D, UPackage* LevelPackage, UBOOL bIsScriptReferenced );

UBOOL IsInVisibleLevel( UObject* Object );

TArray<UPackage *> CurrentLevels;

/** Referenced Content Stat Class															*/
FAnalyzeReferencedContentStat	ReferencedContentStat;

/** List of persistent map - this is only used for persistent FaceFXAnimSet for console		*/
TArray<FString> PersistentMapList;
INT FillPersistentMapList( const TArray<FString>& MapList );

UBOOL HandlePersistentFaceFXAnimSet( const TArray<FString>& MapList, UBOOL bEnableLog = FALSE );

FPersistentFaceFXAnimSetGenerator	PersistentFaceFXAnimSetGenerator;
FPersistentMapInfo					AnalyzeReferencedContent_PersistentMapInfoHelper;


/**
 * This will write out the currently referenced content and then empty out the "persistent" datastructures.
 **/
void WriteOutAndEmptyCurrentlyReferencedContent();

UBOOL UsingPersistentFaceFXAnimSetGenerator()
{
	// if not windows, and if flag is set
	// for now do not use this
	return FALSE;
	//return (Platform != UE3::PLATFORM_Windows && PersistentFaceFXAnimSetGenerator.GetGeneratePersistentMapAnimSet());
}

/** Time the commandlet was started.  This is used in the filename of all of the .csv files. **/
FString CurrentTime;

UBOOL bEmulateCooking;

void CreateCustomEngine();

END_COMMANDLET // UAnalyzeReferencedContentCommandlet



// List of Classes
// ReferencedObject
struct FObjectReferenceData
{
	UClass *	ReferencedClass;
	INT			NumberOfReferenced;
	INT			ObjectSize;
};

// Actor Referenced Information
struct FClassReferenceData
{
	/** Referenced Class to Ref Data */
	TMap<UClass*, FObjectReferenceData> ObjectRefData;

	/** Memory Diff by SpawnActor */
	INT				MemorySize;
};

/* This is object diff class
 * It saves current object list and diff with other object list class to create new object list class
 */
class FObjectList
{
public:
	// construct the list
	FObjectList();
	FObjectList(UClass * ClassType);
	FObjectList operator-(const FObjectList& OtherObjectList);

	TArray<UObject *> ObjectList;

	INT GetTotalSize();

	void RemoveObject(UObject * Object);
	static INT GetObjectSize(UObject * Object);
};

BEGIN_COMMANDLET(AnalyzeReferencedObject,UnrealEd)

void StaticInitialize();

// ReferenceClassList
TArray<UClass*> ReferencedClassList;

struct FClassID
{
	UClass *	Class;
	INT			SpawnIdx;/* 0 is the first, and 2 being the last */

	FClassID(UClass *InClass, INT InSpawnIdx)
		:	Class(InClass),
			SpawnIdx(InSpawnIdx){}

	UBOOL operator==(const FClassID & Other) const
	{
		return (Class == Other.Class && SpawnIdx == Other.SpawnIdx);
	}
};

friend FORCEINLINE DWORD GetTypeHash( const FClassID& ClassId )
{
	return PointerHash(ClassId.Class, ClassId.SpawnIdx);
}

/** Referencer Class to ReferenceData */
TMap<FClassID, FClassReferenceData> ReferenceData;

// Collect data to format
struct FReferenceData
{
	INT ResourceSize;
	INT ReferenceCount;

	FReferenceData() : ReferenceCount(0), ResourceSize(0) {}
	FReferenceData(INT InResourceSize, INT InReferenceCount)
		:ResourceSize(InResourceSize), ReferenceCount(InReferenceCount) {}
};

void WriteOutAllAvailableData(const FString& LogDirectory);
void WriteOutAllocationData(const FString& FileName);
void WriteOutReferenceCount(const FString& FileName, const TMap<UClass *, TArray<FReferenceData>>& OutputData);
void WriteOutResourceSize(const FString& FileName, const TMap<UClass *, TArray<FReferenceData>>& OutputData);
void CollectAllRowData(TMap<UClass *, TArray<FReferenceData>>& OutputData);

AActor * ExtractActorReferencedObjectList(UClass * Class, INT SpawnIndex);
UBOOL ExtractReferenceData(UClass* Class);

END_COMMANDLET

#endif //_INC_ANALYZEREFERENCEDCONTENT