/*=============================================================================
	Lightmass.h: lightmass import/export definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LIGHTMASS_H__
#define __LIGHTMASS_H__

#include "StaticLightingPrivate.h"
#include "SwarmDefines.h"
#include "SwarmInterface.h"
#include "LightmassRender.h"

/** Forward declarations of Lightmass types */
namespace Lightmass
{
	struct FSceneFileHeader;
	struct FDebugLightingInputData;
	struct FMaterialData;
	struct FMaterialElementData;
}

/** Lightmass Exporter class */
class FLightmassExporter
{
public:

	FLightmassExporter();
	virtual ~FLightmassExporter();

	void SetLevelSettings(FLightmassWorldInfoSettings& InLevelSettings)
	{
		LevelSettings = InLevelSettings;
	}

	void SetNumUnusedLocalCores(INT InNumUnusedLocalCores)
	{
		NumUnusedLocalCores = InNumUnusedLocalCores;
	}

	void SetQualityLevel(ELightingBuildQuality InQualityLevel)
	{
		QualityLevel = InQualityLevel;
	}

	void SetLevelName(const FString& InName)
	{
		LevelName = InName;
	}

	void ClearImportanceVolumes()
	{
		ImportanceVolumes.Empty();
	}

	void AddImportanceVolume(const ALightmassImportanceVolume* InImportanceVolume)
	{
		ImportanceVolumes.AddItem(InImportanceVolume->GetComponentsBoundingBox(TRUE));
	}

	const TArray<FBox>& GetImportanceVolumes() const
	{
		return ImportanceVolumes;
	}

	void SetCustomImportanceVolume(const FBox& InCustomImportanceBoundingBox)
	{
		CustomImportanceBoundingBox = InCustomImportanceBoundingBox;
	}

	void SetbVisibleLevelsOnly(const UBOOL InbVisibleLevels)
	{
		bVisibleLevelsOnly = InbVisibleLevels;
	}

	void AddCharacterIndirectDetailVolume(const ALightmassCharacterIndirectDetailVolume* InDetailVolume)
	{
		CharacterIndirectDetailVolumes.AddItem(InDetailVolume->GetComponentsBoundingBox(TRUE));
	}

	void AddMaterial(UMaterialInterface* InMaterialInterface);
	void AddTerrainMaterialResource(FTerrainMaterialResource* InTerrainMaterialResource);
	void AddLight(ULightComponent* Light);

	const FStaticLightingMapping* FindMappingByGuid(FGuid FindGuid) const;

	/** Guids of visibility tasks. */
	TArray<FGuid> VisibilityBucketGuids;

private:

	void WriteToChannel( FGuid& DebugMappingGuid );

	/** Exports visibility Data. */
	void WriteVisibilityData(INT Channel);

	void WriteLights( INT Channel );

	/**
	 * Exports all UModels to secondary, persistent channels
	 */
	void WriteModels();
	
	/**
	 * Exports all UStaticMeshes to secondary, persistent channels
	 */
	void WriteStaticMeshes();

	/**
	 * Exports all ATerrains to secondary, persistent channels
	 */
	void WriteTerrains();

#if WITH_SPEEDTREE
	/**
	 * Exports all USpeedTrees to secondary, persistent channels
	 */
	void WriteSpeedTrees();
#endif	//#if WITH_SPEEDTREE

	void WriteLandscapes();

	/**
	 *	Exports all of the materials to secondary, persistent channels
	 */
	void WriteMaterials();

	void WriteMeshInstances( INT Channel );
	void WriteFluidSurfaceInstances( INT Channel );
#if WITH_SPEEDTREE
	void WriteSpeedTreeMeshInstances( INT Channel );
#endif	//#if WITH_SPEEDTREE
	void WriteLandscapeInstances( INT Channel );

	void WriteMappings( INT Channel );

	void WriteBaseMeshInstanceData( INT Channel, INT MeshIndex, const class FStaticLightingMesh* Mesh, TArray<Lightmass::FMaterialElementData>& MaterialElementData );
	void WriteBaseMappingData( INT Channel, const class FStaticLightingMapping* Mapping );
	void WriteBaseVertexMappingData( INT Channel, const class FStaticLightingVertexMapping* VertexMapping );
	void WriteBaseTextureMappingData( INT Channel, const class FStaticLightingTextureMapping* TextureMapping );
	void WriteVertexData( INT Channel, const struct FStaticLightingVertex* Vertex );
	void WriteTerrainMapping( INT Channel, const class FTerrainComponentStaticLighting* TerrainMapping );
	void WriteFluidMapping( INT Channel, const class FFluidSurfaceStaticLightingTextureMapping* FluidMapping );
#if WITH_SPEEDTREE
	void WriteSpeedTreeMapping( INT Channel, const class FSpeedTreeStaticLightingMapping* SpeedTreeMapping );
#endif	//#if WITH_SPEEDTREE
	void WriteLandscapeMapping( INT Channel, const class FLandscapeStaticLightingTextureMapping* LandscapeMapping );

	/** Finds the GUID of the mapping that is being debugged. */
	UBOOL FindDebugMapping(FGuid& DebugMappingGuid);

	/** Fills out the Scene's settings, read from the engine ini. */
	void WriteSceneSettings( Lightmass::FSceneFileHeader& Scene );

	/** Fills InputData with debug information */
	void WriteDebugInput( Lightmass::FDebugLightingInputData& InputData, FGuid& DebugMappingGuid );

	TMap<const FStaticLightingMesh*,INT> MeshToIndexMap;

	//
	NSwarm::FSwarmInterface&	Swarm;
	UBOOL						bSwarmConnectionIsValid;
	FGuid						SceneGuid;
	FString						ChannelName;

	FBox						CustomImportanceBoundingBox;

	TArray<FBox>				ImportanceVolumes;
	TArray<FBox>				CharacterIndirectDetailVolumes;

	FLightmassWorldInfoSettings LevelSettings;
	/** The number of local cores to leave unused */
	INT NumUnusedLocalCores;
	/** The quality level of the lighting build */
	ELightingBuildQuality QualityLevel;

	FString LevelName;

	UBOOL bVisibleLevelsOnly;

	// lights objects
	TLookupMap<const class UDirectionalLightComponent*> DirectionalLights;
	TLookupMap<const class UPointLightComponent*> PointLights;
	TLookupMap<const class USpotLightComponent*> SpotLights;
	TLookupMap<const class USkyLightComponent*> SkyLights;

	// BSP mappings
	TLookupMap<class FBSPSurfaceStaticLighting*> BSPSurfaceMappings;
	TLookupMap<const class UModel*> Models;
	
	// static mesh mappings
	TLookupMap<const class FStaticMeshStaticLightingMesh*> StaticMeshLightingMeshes;
	TLookupMap<class FStaticMeshStaticLightingTextureMapping*> StaticMeshTextureMappings;
	TLookupMap<class FStaticMeshStaticLightingVertexMapping*> StaticMeshVertexMappings;	
	TLookupMap<const class UStaticMesh*> StaticMeshes;
	
	// Terrain mappings
	TLookupMap<class FTerrainComponentStaticLighting*> TerrainMappings;
	TLookupMap<class ATerrain*> Terrains;
	
	// Fluid surface mappings
	TLookupMap<const class FFluidSurfaceStaticLightingMesh*> FluidSurfaceLightingMeshes;
	TLookupMap<class FFluidSurfaceStaticLightingTextureMapping*> FluidSurfaceTextureMappings;

#if WITH_SPEEDTREE
	// Speedtree mappings
	TLookupMap<const class FSpeedTreeStaticLightingMesh*> SpeedTreeLightingMeshes;
	TLookupMap<class FSpeedTreeStaticLightingMapping*> SpeedTreeMappings;
	TLookupMap<const class USpeedTree*> SpeedTrees;
#endif	//#if WITH_SPEEDTREE

	// Landscape
	TLookupMap<const class FLandscapeStaticLightingMesh*> LandscapeLightingMeshes;
	TLookupMap<class FLandscapeStaticLightingTextureMapping*> LandscapeTextureMappings;

	// materials
	TLookupMap<class UMaterialInterface*> Materials;
	TLookupMap<class FTerrainMaterialResource*> TerrainMaterialResources;

	/** Exporting progress bar maximum value. */
	INT		TotalProgress;
	/** Exporting progress bar current value. */
	INT		CurrentProgress;

	/** The material renderers */
	FLightmassMaterialRenderer MaterialRenderer;
	FLightmassTerrainMaterialRenderer TerrainMaterialRenderer;

	/** Friends */
	friend class FBSPSurfaceStaticLighting;
	friend class FStaticMeshStaticLightingMesh;
	friend class FStaticMeshStaticLightingTextureMapping;
	friend class FStaticMeshStaticLightingVertexMapping;
	friend class FTerrainComponentStaticLighting;
	friend class FLightmassProcessor;
	friend class FFluidSurfaceStaticLightingMesh;
	friend class FFluidSurfaceStaticLightingTextureMapping;
#if WITH_SPEEDTREE
	friend class FSpeedTreeStaticLightingMesh;
	friend class FSpeedTreeStaticLightingMapping;
#endif	//#if WITH_SPEEDTREE
	friend class FFracturedStaticLightingMesh;
	friend class FLandscapeStaticLightingMesh;
	friend class FLandscapeStaticLightingTextureMapping;
};

/** Lightmass Importer class */
class FLightmassImporter
{
};

/** Thread-safe single-linked list (lock-free). */
template<typename ElementType>
class TListThreadSafe
{	public:

	/** Initialization constructor. */
	TListThreadSafe():
		FirstElement(NULL)
	{}

	/**
	 * Adds an element to the list.
	 * @param Element	Newly allocated and initialized list element to add.
	 */
	void AddElement(TList<ElementType>* Element)
	{
		// Link the element at the beginning of the list.
		TList<ElementType>* LocalFirstElement;
		do 
		{
			LocalFirstElement = FirstElement;
			Element->Next = LocalFirstElement;
		}
		while(appInterlockedCompareExchangePointer((void**)&FirstElement,Element,LocalFirstElement) != LocalFirstElement);
	}

	/**
	 * Clears the list and returns the elements.
	 * @return	List of all current elements. The original list is cleared. Make sure to delete all elements when you're done with them!
	 */
	TList<ElementType>* ExtractAll()
	{
		// Atomically read the complete list and clear the shared head pointer.
		TList<ElementType>* LocalFirstElement;
		do
		{
			LocalFirstElement = FirstElement;
		}
		while(appInterlockedCompareExchangePointer((void**)&FirstElement,NULL,LocalFirstElement) != LocalFirstElement);
		return LocalFirstElement;
	}

	/**
	 *	Clears the list.
	 */
	void Clear()
	{
		while (FirstElement)
		{
			// Atomically read the complete list and clear the shared head pointer.
			TList<ElementType>* Element = ExtractAll();

			// Delete all elements in the local list.
			while (Element)
			{
				TList<ElementType>* NextElement = Element->Next;
				delete Element;
				Element = NextElement;
			};
		};
	}

private:

	TList<ElementType>* FirstElement;
};

/** Stores the data for a visibility cell imported from Lightmass before compression. */
class FUncompressedPrecomputedVisibilityCell
{
public:
	FBox Bounds;
	/** Precomputed visibility data, the bits are indexed by VisibilityId of a primitive component. */
	TArray<BYTE> VisibilityData;
};

/** Lightmass Processor class */
class FLightmassProcessor
{
public:
	/** 
	 * Constructor
	 * 
	 * @param bInDumpBinaryResults TRUE if it should dump out raw binary lighting data to disk
	 */
	FLightmassProcessor(const FStaticLightingSystem& InSystem, UBOOL bInDumpBinaryResults, UBOOL bInOnlyBuildVisibility, UBOOL bInOnlyBuildVisibleLevels);

	~FLightmassProcessor();

	/** Retrieve an exporter for the given channel name */
	FLightmassExporter* GetLightmassExporter();

	/** Is the connection to Swarm valid? */
	UBOOL IsSwarmConnectionIsValid() const
	{
		return bSwarmConnectionIsValid;
	}

	void SetUseDeterministicLighting(UBOOL bInUseDeterministicLighting)
	{
		bUseDeterministicLighting = bInUseDeterministicLighting;
	}

	void SetImportCompletedMappingsImmediately(UBOOL bInImportCompletedMappingsImmediately)
	{
		bImportCompletedMappingsImmediately = bInImportCompletedMappingsImmediately;
	}

	void SetProcessCompletedMappingsImmediately(UBOOL bInProcessCompletedMappingsImmediately)
	{
		bProcessCompletedMappingsImmediately = bInProcessCompletedMappingsImmediately;
	}

	UBOOL	Run();

	/** Defined the period during which Job APIs can be used, including opening and using Job Channels */
	UBOOL	OpenJob();
	UBOOL	CloseJob();

	FString GetMappingFileExtension(const FStaticLightingMapping* InMapping);

	/** Returns the Lightmass statistics. */
	const FLightmassStatistics& GetStatistics() const
	{
		return Statistics;
	}

	/**
	 * Import the mapping specified by a Guid.
	 *	@param	MappingGuid				Guid that identifies a mapping
	 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
	 *									If FALSE, store it off for later processing
	 **/
	void	ImportMapping( const FGuid& MappingGuid, UBOOL bProcessImmediately );

	/**
	 * Process the mapping specified by a Guid.
	 * @param MappingGuid	Guid that identifies a mapping
	 **/
	void	ProcessMapping( const FGuid& MappingGuid );

	/**
	 * Process any available mappings.
	 * Used when running w/ bProcessCompletedMappingsImmediately == TRUE
	 *
	 *	@param	bProcessAllAvailable	If TRUE, process ALL available mappings.
	 **/
	void	ProcessAvailableMappings(UBOOL bProcessAllAvailable);

protected:
	enum StaticLightingType
	{
		SLT_Texture,		// FStaticLightingTextureMapping
		SLT_Vertex,			// FStaticLightingVertexMapping
	};

	struct FTextureMappingImportHelper;
	struct FVertexMappingImportHelper;

	/**
	 *	Helper struct for importing mappings
	 */
	struct FMappingImportHelper
	{
		/** The type of lighting mapping */
		StaticLightingType	Type;
		/** The mapping guid read in */
		FGuid				MappingGuid;
		/** The execution time this mapping took */
		DOUBLE				ExecutionTime;
		/** Whether the mapping has been processed yet */
		UBOOL				bProcessed;

		FMappingImportHelper()
		{
			Type = SLT_Texture;
			ExecutionTime = 0.0;
			bProcessed = FALSE;
		}

		FMappingImportHelper(const FMappingImportHelper& InHelper)
		{
			Type = InHelper.Type;
			MappingGuid = InHelper.MappingGuid;
			ExecutionTime = InHelper.ExecutionTime;
			bProcessed = InHelper.bProcessed;
		}

		virtual ~FMappingImportHelper()
		{
		}

		virtual FTextureMappingImportHelper* GetTextureMappingHelper()
		{
			return NULL;
		}

		virtual FVertexMappingImportHelper* GetVertexMappingHelper()
		{
			return NULL;
		}
	};

	/**
	 *	Helper struct for importing texture mappings
	 */
	struct FTextureMappingImportHelper : public FMappingImportHelper
	{
		/** The texture mapping being imported */
		FStaticLightingTextureMapping* TextureMapping;
		/** The imported quantized lightmap data */
		FQuantizedLightmapData* QuantizedData;
		/** The percentage of unmapped texels */
		FLOAT UnmappedTexelsPercentage;
		/** Number of shadow maps to import */
		INT NumShadowMaps;
		INT NumSignedDistanceFieldShadowMaps;
		/** The imported shadow map data */
		TMap<ULightComponent*,FShadowMapData2D*> ShadowMapData;

		FTextureMappingImportHelper()
		{
			TextureMapping = NULL;
			QuantizedData = NULL;
			UnmappedTexelsPercentage = 0.0f;
			NumShadowMaps = 0;
			NumSignedDistanceFieldShadowMaps = 0;
			Type = SLT_Texture;
		}

		FTextureMappingImportHelper(const FTextureMappingImportHelper& InHelper)
		:	FMappingImportHelper(InHelper)
		{
			TextureMapping = InHelper.TextureMapping;
			QuantizedData = InHelper.QuantizedData;
			UnmappedTexelsPercentage = InHelper.UnmappedTexelsPercentage;
			NumShadowMaps = InHelper.NumShadowMaps;
			NumSignedDistanceFieldShadowMaps = InHelper.NumSignedDistanceFieldShadowMaps;
			ShadowMapData = InHelper.ShadowMapData;
		}

		virtual ~FTextureMappingImportHelper()
		{
			// Note: TextureMapping and QuantizeData are handled elsewhere.
		}

		virtual FTextureMappingImportHelper* GetTextureMappingHelper()
		{
			return this;
		}
	};

	/**
	 *	Helper struct for importing vertex mappings
	 */
	struct FVertexMappingImportHelper : public FMappingImportHelper
	{
		/** The vertex mapping being imported */
		FStaticLightingVertexMapping* VertexMapping;
		/** The imported quantized light map data */
		FQuantizedLightmapData* QuantizedData;
		/** Number of shadow maps to import */
		INT NumShadowMaps;
		/** The imported shadow map data */
		TMap<ULightComponent*,FShadowMapData1D*> ShadowMapData;

		FVertexMappingImportHelper()
		{
			VertexMapping = NULL;
			QuantizedData = NULL;
			NumShadowMaps = 0;
			Type = SLT_Vertex;
		}

		FVertexMappingImportHelper(const FVertexMappingImportHelper& InHelper)
		:	FMappingImportHelper(InHelper)
		{
			VertexMapping = InHelper.VertexMapping;
			QuantizedData = InHelper.QuantizedData;
			NumShadowMaps = InHelper.NumShadowMaps;
			ShadowMapData = InHelper.ShadowMapData;
		}

		virtual ~FVertexMappingImportHelper()
		{
			// Note: VertexMapping and QuantizeData are handled elsewhere.
		}

		virtual FVertexMappingImportHelper* GetVertexMappingHelper()
		{
			return this;
		}
	};

	UBOOL AddDominantShadowTask(ULightComponent* Light);

	/**
	 *	Retrieve the light for the given Guid
	 *
	 *	@param	LightGuid			The guid of the light we are looking for
	 *
	 *	@return	ULightComponent*	The corresponding light component.
	 *								NULL if not found.
	 */
	ULightComponent* FindLight(FGuid& LightGuid);

	/**
	 *	Retrieve the static mehs for the given Guid
	 *
	 *	@param	Guid				The guid of the static mesh we are looking for
	 *
	 *	@return	UStaticMesh*		The corresponding static mesh.
	 *								NULL if not found.
	 */
	UStaticMesh* FindStaticMesh(FGuid& Guid);

	//@todo. Return error codes for these???
	/**
	 *	Import light map data from the given channel.
	 *
	 *	@param	Channel				The channel to import from.
	 *	@param	QuantizedData		The quantized lightmap data to fill in
	 *	@param	UncompressedSize	Size the data will be after uncompressing it (if compressed)
	 *	@param	CompressedSize		Size of the source data if compressed
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportLightMapData1DData(INT Channel, FQuantizedLightmapData* QuantizedData, INT UncompressedSize, INT CompressedSize);

	/**
	 *	Import light map data from the given channel.
	 *
	 *	@param	Channel				The channel to import from.
	 *	@param	QuantizedData		The quantized lightmap data to fill in
	 *	@param	UncompressedSize	Size the data will be after uncompressing it (if compressed)
	 *	@param	CompressedSize		Size of the source data if compressed
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportLightMapData2DData(INT Channel, FQuantizedLightmapData* QuantizedData, INT UncompressedSize, INT CompressedSize);

	/**
	 *	Import shadow map data from the given channel.
	 *
	 *	@param	Channel				The channel to import from.
	 *	@param	OutShadowMapData	The light component --> shadow map data mapping that should be filled in.
	 *	@param	ShadowMapCount		Number of shadow maps to import
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportShadowMapData1D(INT Channel, TMap<ULightComponent*,FShadowMapData1D*>& OutShadowMapData, INT ShadowMapCount);
	/**
	 *	Import shadow map data from the given channel.
	 *
	 *	@param	Channel				The channel to import from.
	 *	@param	OutShadowMapData	The light component --> shadow map data mapping that should be filled in.
	 *	@param	ShadowMapCount		Number of shadow maps to import
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportShadowMapData2D(INT Channel, TMap<ULightComponent*,FShadowMapData2D*>& OutShadowMapData, INT ShadowMapCount);
	/**
	 *	Import signed distance field shadow map data from the given channel.
	 *
	 *	@param	Channel				The channel to import from.
	 *	@param	OutShadowMapData	The light component --> shadow map data mapping that should be filled in.
	 *	@param	ShadowMapCount		Number of shadow maps to import
	 *
	 *	@return	UBOOL				TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportSignedDistanceFieldShadowMapData2D(INT Channel, TMap<ULightComponent*,FShadowMapData2D*>& OutShadowMapData, INT ShadowMapCount);


	/**
	 *	Import a complete vertex mapping....
	 *
	 *	@param	Channel			The channel to import from.
	 *	@param	VMImport		The vertex mapping information that will be imported.
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportVertexMapping(INT Channel, FVertexMappingImportHelper& VMImport);

	/**
	 *	Import a complete texture mapping....
	 *
	 *	@param	Channel			The channel to import from.
	 *	@param	TMImport		The texture mapping information that will be imported.
	 *
	 *	@return	UBOOL			TRUE if successful, FALSE otherwise.
	 */
	UBOOL ImportTextureMapping(INT Channel, FTextureMappingImportHelper& TMImport);

	//
	FLightmassExporter* Exporter;
	FLightmassImporter* Importer;
	const FStaticLightingSystem& System;

	NSwarm::FSwarmInterface&	Swarm;
	UBOOL						bSwarmConnectionIsValid;
	/** Whether lightmass has completed the job successfully. */
	UBOOL						bProcessingSuccessful;
	/** Whether lightmass has completed the job with a failure. */
	UBOOL						bProcessingFailed;
	/** Whether lightmass has received a quit message from Swarm. */
	UBOOL						bQuitReceived;
	/** Number of completed tasks, as reported from Swarm. */
	INT							NumCompletedTasks;
	/** Whether Lightmass is currently running. */
	UBOOL						bRunningLightmass;
	/** Lightmass statistics*/
	FLightmassStatistics		Statistics;

	/** If TRUE, only visibility will be rebuilt. */
	UBOOL bOnlyBuildVisibility;
	/** If TRUE, only visible levels lighting will be rebuilt. */
	UBOOL bOnlyBuildVisibleLevels;
	/** If TRUE, this will dump out raw binary lighting data to disk */
	UBOOL bDumpBinaryResults;
	/** If TRUE, ensure the processing order... */
	UBOOL bUseDeterministicLighting;
	/** If TRUE, and in Deterministic mode, mappings will be imported but not processed as they are completed... */
	UBOOL bImportCompletedMappingsImmediately;
	/** If TRUE, and bImportCompletedMappingsImmediately is TRUE, process mappings as they become available. */
	UBOOL bProcessCompletedMappingsImmediately;

	/** The index of the next mapping to process when available. */
	INT MappingToProcessIndex;
	/** The number of available mappings to process before yielding back to the importing. */
	static INT MaxProcessAvailableCount;

	/** Dominant lights which created a dominant shadow task. */
	TMap<FGuid, ULightComponent*> DominantShadowTaskLights;

	/** Imported visibility cells, one array per visibility task. */
	TArray<TArray<FUncompressedPrecomputedVisibilityCell> > CompletedPrecomputedVisibilityCells;

	/** BSP mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FBSPSurfaceStaticLighting*>					PendingBSPMappings;
	/** Texture mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FStaticMeshStaticLightingTextureMapping*>	PendingTextureMappings;
	/** Terrain mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FTerrainComponentStaticLighting*>			PendingTerrainMappings;
	/** Fluid mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FFluidSurfaceStaticLightingTextureMapping*>	PendingFluidMappings;
	/** Vertex mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FStaticMeshStaticLightingVertexMapping*>	PendingVertexMappings;
#if WITH_SPEEDTREE
	/** SpeedTree mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FSpeedTreeStaticLightingMapping*>			PendingSpeedTreeMappings;
#endif	//#if WITH_SPEEDTREE
	/** Landscape mappings that are not completed by Lightmass yet. */
	TMap<FGuid, FLandscapeStaticLightingTextureMapping*>	PendingLandscapeMappings;


	/** Mappings that are completed by Lightmass. */
	TListThreadSafe<FGuid>									CompletedMappingTasks;

	/** Positive if the volume sample task is complete. */
	static volatile INT										VolumeSampleTaskCompleted;

	/** List of completed visibility tasks. */
	TListThreadSafe<FGuid>									CompletedVisibilityTasks;

	/** Positive if the mesh area light data task is complete. */
	static volatile INT										MeshAreaLightDataTaskCompleted;

	/** Positive if the volume distance field task is complete. */
	static volatile INT										VolumeDistanceFieldTaskCompleted;

	/** Dominant shadow tasks that are completed by Lightmass. */
	TListThreadSafe<FGuid>									CompletedDominantShadowTasks;

	/** Mappings that have been imported but not processed. */
	TMap<FGuid, FMappingImportHelper*>						ImportedMappings;

	/** Guid of the mapping that is being debugged. */
	FGuid DebugMappingGuid;

	/**
	 *	Import all mappings that have been completed so far.
	 *
	 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
	 *									If FALSE, store it off for later processing
	 **/
	void	ImportMappings(UBOOL bProcessImmediately);

	/** Imports volume lighting samples from Lightmass and adds them to the appropriate levels. */
	void	ImportVolumeSamples();

	/** Imports precomputed visibility */
	void	ImportPrecomputedVisibility();

	/** Processes the imported visibility, compresses it and applies it to the current level. */
	void	ApplyPrecomputedVisibility();
	
	/** Imports dominant shadow information from Lightmass. */
	void	ImportDominantShadowInfo();

	/** Imports data from Lightmass about the mesh area lights generated for the scene, and creates AGeneratedMeshAreaLight's for them. */
	void	ImportMeshAreaLightData();

	/** Imports the volume distance field from Lightmass. */
	void	ImportVolumeDistanceFieldData();

	/** Determines whether the specified mapping is a texture mapping */
	UBOOL	IsStaticLightingTextureMapping( const FGuid& MappingGuid );
	/** Determines whether the specified mapping is a vertex mapping */
	UBOOL	IsStaticLightingVertexMapping( const FGuid& MappingGuid );

	/** Gets the texture mapping for the specified GUID */
	FStaticLightingTextureMapping*	GetStaticLightingTextureMapping( const FGuid& MappingGuid );
	/** Gets the vertex mapping for the specified GUID */
	FStaticLightingVertexMapping*	GetStaticLightingVertexMapping( const FGuid& MappingGuid );

	/**
	 *	Import the texture mapping 
	 *	@param	TextureMapping			The mapping being imported.
	 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
	 *									If FALSE, store it off for later processing
	 */
	void	ImportStaticLightingTextureMapping( const FGuid& MappingGuid, UBOOL bProcessImmediately );

	/**
	 *	Import the vertex mapping 
	 *	@param	VertexMapping			The mapping being imported.
	 *	@param	bProcessImmediately		If TRUE, immediately process the mapping
	 *									If FALSE, store it off for later processing
	 */
	void	ImportStaticLightingVertexMapping( const FGuid& MappingGuid, UBOOL bProcessImmediately );

	/** Reads in a TArray from the given channel. */
	template<class T>
	void ReadArray(INT Channel, TArray<T>& Array);

	/** Fills out GDebugStaticLightingInfo with the output from Lightmass */
	void	ImportDebugOutput();

	/**
	 * Swarm callback function.
	 * @param CallbackMessage	Incoming message sent from Swarm
	 * @param CallbackData		Type-casted pointer to the FLightmassProcessor
	 **/
	static void SwarmCallback( NSwarm::FMessage* CallbackMessage, void* CallbackData );
};

#endif //__LIGHTMASS_H__
