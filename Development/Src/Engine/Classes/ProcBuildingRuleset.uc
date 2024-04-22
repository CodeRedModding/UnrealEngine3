/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ProcBuildingRuleset extends Object
	hidecategories(Object)	
	native(ProcBuilding);

/** Pointer to first rule to execute */	
var     instanced PBRuleNodeBase    RootRule;

/** Used to avoid editing the same ruleset in multiple Facade windows at the same time. */
var		editoronly transient bool   bBeingEdited;

/** Material applied to roof surface by default (can be overridden) */
var(Roof)   MaterialInterface       DefaultRoofMaterial;

/** Material applied to floor surface by default (can be overridden) */
var(Floor)   MaterialInterface      DefaultFloorMaterial;

/** Material applied to non-rectangular surfaces by default (can be overridden) */
var()       MaterialInterface       DefaultNonRectWallMaterial;

/** Offset applied to floor poly if at very top of overall building */
var(Roof)   float                   RoofZOffset;

/** Offset applied to roof poly if not at very top of overall building */
var(Roof)   float                   NotRoofZOffset;

/** Offset applied to floor poly if at very bottom of overall building */
var(Floor)  float	                FloorZOffset;

/** Offset applied to floor poly if not at very bottom of overall building */
var(Floor)  float                   NotFloorZOffset;

/** Amount to 'pull in' vertices of the generated roof poly. */
var(Roof)   float                   RoofPolyInset;

/** Amount to 'pull in' vertices of the generated floor poly. */
var(Floor)  float                   FloorPolyInset;

/** Amount of specular to apply to low LOD building material */
var(LOD)    float                   BuildingLODSpecular;

/** How much to raise top of scopes that meet the roof of the building, forming a short wall around the roof. */
var(Roof)   float                   RoofEdgeScopeRaise;

/** Cubemap texture to use for the LOD version of the building. */
var(LOD)   Texture                  LODCubemap;

/** Whether to have any 'interior' texture on the LOD building windows */
var(LOD)   bool                     bEnableInteriorTexture;

/** Texture to use for 'interior' of LOD building windows */
var(LOD)   Texture                  InteriorTexture;

/** If TRUE, roof only displays in when building drops to low-detail version */
var(LOD)	bool	                bLODOnlyRoof;

/** Struct contain information about 'variations' supported within this ruleset */
struct native PBVariationInfo
{
	/** Name of this variation */
	var()	Name	VariationName;

	/** If TRUE, meshes are placed on top of simple face poly, rather than making hole for meshes. */
	var()	bool	bMeshOnTopOfFacePoly;
};

/** Array of 'variations' supported within this ruleset */
var()	array<PBVariationInfo>		Variations;

/** Struct holding a defined 'swatch' of parameters that can be selected by name */
struct native PBParamSwatch
{
	/** Name of this swatch */
	var()   name    SwatchName;

	/** Set of parameters that should be applied when this swatch is selected for a building */
	var()   array<PBMaterialParam>  Params;
};

/** Pre-defined, names 'swatches' (or sets) of parameters that can be selected on a building */
var()   array<PBParamSwatch>        ParamSwatches;

/** If TRUE, then pick a random swatch if a building is currently None (or not supported by current ruleset) */
var()   bool                        bPickRandomSwatch;

/** Array of comment nodes (for drawing comment boxes) - not connected, so need this so they are serialized. */
var		editoronly array<PBRuleNodeComment>		Comments;

enum EProcBuildingAxis
{
	EPBAxis_X,
	EPBAxis_Z
};

cpptext
{
	/** Get the top- or bottom-most corner node in this ruleset. */
	UPBRuleNodeCorner* GetRulesetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex);

	/** Returns all rulesets ref'd by this ruleset (via SubRuleset node) */
	void GetReferencedRulesets(TArray<UProcBuildingRuleset*>& OutRulesets);

	/** Pick a random swatch name from this ruleset  */
	FName GetRandomSwatchName();

	/** Get the index of a swatch in the ParamSwatches array (INDEX_NONE if not present)  */
	INT GetSwatchIndexFromName(FName SearchName);
}

defaultproperties
{
	BuildingLODSpecular=2.0
	RoofEdgeScopeRaise=0.0

	bEnableInteriorTexture=TRUE
}