/*=============================================================================
	Exporter.h: Lightmass solver exporter class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#pragma once

#include "LMCore.h"

namespace Lightmass
{
	class FStaticLightingTextureMappingResult;
	class FStaticLightingVertexMappingResult;

	//@todo - need to pass to Serialization
	class FLightmassSolverExporter
	{
	public:

		/**
		 * Constructor
		 * @param InSwarm Wrapper object around the Swarm interface
		 * @param bInDumpTextures If TRUE, the 2d lightmap exporter will dump out textures
		 */
		FLightmassSolverExporter( class FLightmassSwarm* InSwarm, const FScene& InScene, UBOOL bInDumpTextures );
		~FLightmassSolverExporter();

		class FLightmassSwarm* GetSwarm();

		/**
		 * Send complete lighting data to UE3
		 *
		 * @param LightingData - Object containing the computed data
		 */
		void ExportResults(struct FVertexMappingStaticLightingData& LightingData, UBOOL bUseUniqueChannel) const;
		void ExportResults(struct FTextureMappingStaticLightingData& LightingData, UBOOL bUseUniqueChannel) const;
		void ExportResults(const struct FPrecomputedVisibilityData& TaskData) const;

		/**
		 * Used when exporting multiple mappings into a single file
		 */
		INT BeginExportResults(struct FVertexMappingStaticLightingData& LightingData, UINT NumMappings) const;
		INT BeginExportResults(struct FTextureMappingStaticLightingData& LightingData, UINT NumMappings) const;
		void EndExportResults() const;

		/** Exports volume lighting samples to UE3. */
		void ExportVolumeLightingSamples(
			UBOOL bExportVolumeLightingDebugOutput,
			const struct FVolumeLightingDebugOutput& DebugOutput,
			const FVector4& VolumeCenter, 
			const FVector4& VolumeExtent, 
			const TMap<INT,TArray<class FVolumeLightingSample> >& VolumeSamples) const;

		/** Exports dominant shadow information to UE3. */
		void ExportDominantShadowInfo(const FGuid& LightGuid, const class FDominantLightShadowInfo& DominantLightShadowInfo) const;

		/** 
		 * Exports information about mesh area lights back to UE3, 
		 * So that UE3 can create dynamic lights to approximate the mesh area light's influence on dynamic objects.
		 */
		void ExportMeshAreaLightData(const class TIndirectArray<FMeshAreaLight>& MeshAreaLights, FLOAT MeshAreaLightGeneratedDynamicLightSurfaceOffset) const;

		/** Exports the volume distance field. */
		void ExportVolumeDistanceField(INT VolumeSizeX, INT VolumeSizeY, INT VolumeSizeZ, FLOAT VolumeMaxDistance, const FBox& DistanceFieldVolumeBounds, const TArray<FColor>& VolumeDistanceField) const;

		/** Creates a new channel and exports everything in DebugOutput. */
		void ExportDebugInfo(const struct FDebugLightingOutput& DebugOutput) const;

	private:
		class FLightmassSwarm*	Swarm;
		const class FScene& Scene;

		/** TRUE if the exporter should dump out textures to disk for previewing */
		UBOOL bDumpTextures;

		/** Writes a TArray to the channel on the top of the Swarm stack. */
		template<class T>
		void WriteArray(const TArray<T>& Array) const;
	};

}	//Lightmass
