/*=============================================================================
	StaticLightingExport.cpp: Static lighting export implementations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

#include "UnStaticMeshLight.h"
#include "UnModelLight.h"
#include "TerrainLight.h"
#include "EngineFluidClasses.h"
#include "FluidSurfaceLight.h"
#include "EngineFoliageClasses.h"
#include "SpeedTreeStaticLighting.h"
#include "UnFracturedStaticMesh.h"
#include "LandscapeLight.h"

#if WITH_MANAGED_CODE
#include "Lightmass.h"
#endif // WITH_MANAGED_CODE

/** 
* Export static lighting mapping instance data to an exporter 
* @param Exporter - export interface to process static lighting data
**/
void FBSPSurfaceStaticLighting::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->BSPSurfaceMappings.AddItem(this);

	// remember all the models used by all the BSP mappings
	Exporter->Models.AddItem(Model);

	if (Model)
	{
		// get all the materials used in the node group
		for (INT NodeIndex = 0; NodeIndex < NodeGroup->Nodes.Num(); NodeIndex++)
		{
			FBspSurf& Surf = Model->Surfs(Model->Nodes(NodeGroup->Nodes(NodeIndex)).iSurf);

			UMaterialInterface* Material = Surf.Material;
			if (Material)
			{
				Exporter->AddMaterial(Material);
			}
		}
	}

	for( INT LightIdx=0; LightIdx < NodeGroup->RelevantLights.Num(); LightIdx++ )
	{
		ULightComponent* Light = NodeGroup->RelevantLights(LightIdx);
		if( Light )
		{
			Exporter->AddLight(Light);
		}
	}
#endif // WITH_MANAGED_CODE
}

/**
 *	@return	UOject*		The object that is mapped by this mapping
 */
UObject* FBSPSurfaceStaticLighting::GetMappedObject() const
{
#if WITH_MANAGED_CODE
	//@todo. THIS WILL SCREW UP IF CALLED MORE THAN ONE TIME!!!!
	// Create a collection object to allow selection of the surfaces in this mapping
	ULightmappedSurfaceCollection* MappedObject = CastChecked<ULightmappedSurfaceCollection>(
		UObject::StaticConstructObject(ULightmappedSurfaceCollection::StaticClass()));
	// Set the owner model
	MappedObject->SourceModel = Model;
	// Fill in the surface index array
	for (INT NodeIndex = 0; NodeIndex < NodeGroup->Nodes.Num(); NodeIndex++)
	{
		MappedObject->Surfaces.AddItem(Model->Nodes(NodeGroup->Nodes(NodeIndex)).iSurf);
	}
	return MappedObject;
#else
	return NULL;
#endif // WITH_MANAGED_CODE
}

/** 
* Export static lighting mesh instance data to an exporter 
* @param Exporter - export interface to process static lighting data
**/
void FStaticMeshStaticLightingMesh::ExportMeshInstance(class FLightmassExporter* Exporter) const
{
#if WITH_MANAGED_CODE
	Exporter->StaticMeshLightingMeshes.AddItem(this);	

	for( INT LightIdx=0; LightIdx < RelevantLights.Num(); LightIdx++ )
	{
		ULightComponent* Light = RelevantLights(LightIdx);
		if( Light )
		{
			Exporter->AddLight(Light);
		}
	}

	// Add the UStaticMesh and materials to the exporter...
	if( StaticMesh )
	{
		Exporter->StaticMeshes.AddItem(StaticMesh);
		if( Primitive )
		{	
			for( INT LODIndex = 0; LODIndex < StaticMesh->LODModels.Num(); ++LODIndex )
			{
				const FStaticMeshRenderData& LODRenderData = StaticMesh->LODModels(LODIndex);
				for( INT ElementIndex = 0; ElementIndex < LODRenderData.Elements.Num(); ++ElementIndex )
				{
					const FStaticMeshElement& Element = LODRenderData.Elements( ElementIndex );
					UMaterialInterface* Material = Primitive->GetMaterial( Element.MaterialIndex, LODIndex );
					Exporter->AddMaterial(Material);
				}
			}
		}
	}
#endif // WITH_MANAGED_CODE
}

/** 
* Export static lighting mapping instance data to an exporter 
* @param Exporter - export interface to process static lighting data
**/
void FStaticMeshStaticLightingTextureMapping::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->StaticMeshTextureMappings.AddItem(this);	
#endif // WITH_MANAGED_CODE
}

/** 
* Export static lighting mapping instance data to an exporter 
* @param Exporter - export interface to process static lighting data
**/
void FStaticMeshStaticLightingVertexMapping::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->StaticMeshVertexMappings.AddItem(this);
#endif // WITH_MANAGED_CODE
}


#if !CONSOLE
/** 
 * Export static lighting mapping instance data to an exporter 
 * @param Exporter - export interface to process static lighting data
 **/
void FTerrainComponentStaticLighting::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->TerrainMappings.AddItem(this);
	Exporter->Terrains.AddItem(Terrain);

	if (Terrain)
	{
		TArrayNoInit<FTerrainMaterialResource*>& TerrainMaterials = Terrain->GetCachedTerrainMaterials();
		for (INT MtrlIdx = 0; MtrlIdx < TerrainMaterials.Num(); MtrlIdx++)
		{
			Exporter->AddTerrainMaterialResource(TerrainMaterials(MtrlIdx));
		}
	}

	for (INT LightIdx=0; LightIdx < RelevantLights.Num(); LightIdx++ )
	{
		ULightComponent* Light = RelevantLights(LightIdx);
		if( Light )
		{
			Exporter->AddLight(Light);
		}
	}
#endif // WITH_MANAGED_CODE
}
#endif	//#if !CONSOLE

//
//	Fluid surface
//
/** 
 * Export static lighting mesh instance data to an exporter 
 * @param Exporter - export interface to process static lighting data
 **/
void FFluidSurfaceStaticLightingMesh::ExportMeshInstance(class FLightmassExporter* Exporter) const
{
#if WITH_MANAGED_CODE
	Exporter->FluidSurfaceLightingMeshes.AddItem(this);	

	if (Component && Component->FluidMaterial)
	{
		Exporter->AddMaterial(Component->FluidMaterial);
	}

	for( INT LightIdx=0; LightIdx < RelevantLights.Num(); LightIdx++ )
	{
		ULightComponent* Light = RelevantLights(LightIdx);
		if( Light )
		{
			Exporter->AddLight(Light);
		}
	}
#endif // WITH_MANAGED_CODE
}

/** 
 * Export static lighting mapping instance data to an exporter 
 * @param Exporter - export interface to process static lighting data
 **/
void FFluidSurfaceStaticLightingTextureMapping::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->FluidSurfaceTextureMappings.AddItem(this);	
#endif // WITH_MANAGED_CODE
}

#if WITH_SPEEDTREE
/** 
 * Export static lighting mesh instance data to an exporter 
 * @param Exporter - export interface to process static lighting data
 **/
void FSpeedTreeStaticLightingMesh::ExportMeshInstance(class FLightmassExporter* Exporter) const
{
#if WITH_MANAGED_CODE
	Exporter->SpeedTreeLightingMeshes.AddItem(this);
	
	for( INT LightIdx=0; LightIdx < RelevantLights.Num(); LightIdx++ )
	{
		ULightComponent* Light = RelevantLights(LightIdx);
		if( Light )
		{
			Exporter->AddLight(Light);
		}
	}

	USpeedTreeComponent* SpeedTreeComp = ComponentStaticLighting->GetComponent();
	USpeedTree* SpeedTree = SpeedTreeComp ? SpeedTreeComp->SpeedTree : NULL;
	if (SpeedTree)
	{
		Exporter->SpeedTrees.AddItem(SpeedTree);

		for (BYTE MeshTypeIdx = STMT_MinMinusOne + 1; MeshTypeIdx < STMT_Max; MeshTypeIdx++)
		{
			Exporter->AddMaterial(SpeedTreeComp->GetMaterial(MeshTypeIdx));
		}
	}
#endif // WITH_MANAGED_CODE
}

/** 
 * Export static lighting mapping instance data to an exporter 
 * @param Exporter - export interface to process static lighting data
 **/
void FSpeedTreeStaticLightingMapping::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->SpeedTreeMappings.AddItem(this);
#endif // WITH_MANAGED_CODE
}

#endif	//#if WITH_SPEEDTREE


//
//	Landscape
//
/** 
* Export static lighting mesh instance data to an exporter 
* @param Exporter - export interface to process static lighting data
**/
void FLandscapeStaticLightingMesh::ExportMeshInstance(class FLightmassExporter* Exporter) const
{
#if WITH_MANAGED_CODE
	Exporter->LandscapeLightingMeshes.AddItem(this);	

	if (LandscapeComponent && LandscapeComponent->MaterialInstance)
	{
		Exporter->AddMaterial(LandscapeComponent->MaterialInstance);
	}

	for( INT LightIdx=0; LightIdx < RelevantLights.Num(); LightIdx++ )
	{
		ULightComponent* Light = RelevantLights(LightIdx);
		if( Light )
		{
			Exporter->AddLight(Light);
		}
	}
#endif // WITH_MANAGED_CODE
}

/** 
* Export static lighting mapping instance data to an exporter 
* @param Exporter - export interface to process static lighting data
**/
void FLandscapeStaticLightingTextureMapping::ExportMapping(class FLightmassExporter* Exporter)
{
#if WITH_MANAGED_CODE
	Exporter->LandscapeTextureMappings.AddItem(this);	
#endif // WITH_MANAGED_CODE
}
