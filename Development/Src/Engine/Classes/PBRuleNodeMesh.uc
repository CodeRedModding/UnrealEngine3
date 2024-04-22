/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeMesh extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);

/** Stores all the options that can be applied to one section of a mesh */
struct native BuildingMatOverrides
{
	/** Array of materials, one of which will be selected for a certain section */
	var()	array<MaterialInterface>	MaterialOptions;
};

/** Information about one mesh used as part of the building construction */
struct native BuildingMeshInfo
{
	/** Actual mesh to use */
	var()   StaticMesh  Mesh;
	/** Defined X length of mesh, when used in building */
	var()   float       DimX;
	/** Defined Z length of mesh, when used in building */
	var()   float       DimZ;
	/** Chance of this building mesh being picked */
	var()   float       Chance;	
	/** Optional translation applied to to mesh */
	var()   DistributionVector      Translation;
	/** Optional rotation (in degrees) applied to to mesh */	
	var()   DistributionVector      Rotation;
	/** If TRUE, the Translation specified is scaled by any scaling applied to the mesh */
	var()   bool        bMeshScaleTranslation;
	
	/** If TRUE, use OverriddenLightMapRes instead of resolution set on the mesh. */
	var()   bool        bOverrideMeshLightMapRes;
	/** Resolution to use for lighting on this mesh, if bOverrideMeshLightMapRes is TRUE. */
	var()   int	        OverriddenMeshLightMapRes;
	
	/** DEPRECATED */
	var   array<MaterialInterface>    MaterialOverrides;

	/** Specifies options for overriding material on each section of the mesh  */
	var()	array<BuildingMatOverrides>		SectionOverrides;
	
	structdefaultproperties
	{
		DimX=512.0
		DimZ=512.0
		Chance=1.0
		OverriddenMeshLightMapRes=32
	}
	
	structcpptext
	{
		FBuildingMeshInfo() {}
		FBuildingMeshInfo(EEventParm)
		{
			appMemzero(this, sizeof(FBuildingMeshInfo));
		}
		void InitToDefaults()
		{
			appMemzero(this, sizeof(FBuildingMeshInfo));
			DimX=512.f;
			DimZ=512.f;
			Chance=1.f;
			OverriddenMeshLightMapRes=32;
		}
		FBuildingMeshInfo(ENativeConstructor)
		{
			InitToDefaults();
		}	

		/** Get a set of overrides, one for each section, by picking from each SectionOverrides struct. CAn be random, or first for each section */
		TArray<UMaterialInterface*> GetMaterialOverrides(UBOOL bRandom) const;
	}
};

/** Set of meshes to pick from. */
var()   array<BuildingMeshInfo>     BuildingMeshes;

/** Mesh to use if this scope if partially occluded. If a mesh is not specified, will just use one of the BuildingMeshes array.  */
var()   BuildingMeshInfo            PartialOccludedBuildingMesh;

/** If TRUE, will test region is not occluded (or is partially occluded) before placing mesh. */
var()   bool                        bDoOcclusionTest;

/** If TRUE, this mesh will block all, including players */
var()   bool                        bBlockAll;

cpptext
{
	virtual void PostLoad();

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
	virtual FColor GetRuleNodeTitleColor();

	/** Allows custom visualization drawing*/
	virtual FIntPoint GetVisualizationSize(void);
	/**
	 * Custom visualization that can be specified per node
	 */
	virtual void DrawVisualization(FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, const FIntPoint& InDrawPosition);
	

private:
	/**
	 * Render function that retrieves the thumbnail from the mesh and draws it in the grid
	 */
	void DrawPreviewMesh (FLinkedObjectDrawHelper* InHelper, FViewport* Viewport, FCanvas* Canvas, const FBuildingMeshInfo& MeshInfo, const FIntPoint& InDrawPosition, const INT InRow, const INT InCol, const FColor& BorderColor);

	// PBRuleNodeMesh	
}

/** Util to pick a random building mesh from the BuildingMeshes array, using the Chance specified */
native function int PickRandomBuildingMesh();	

defaultproperties
{
	NextRules.Empty // leaf node

	bDoOcclusionTest=TRUE
}