/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeQuad extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);

/** Material to apply to created quad. */
var()   MaterialInterface   Material;
/** How large each repeat of the texture is allowed to be along X. */
var()   float               RepeatMaxSizeX;
/** How large each repeat of the texture is allowed to be along Z. */
var()   float               RepeatMaxSizeZ;
/** Resolution of lightmap on this quad */
var()   int                 QuadLightmapRes;
/** Amount to offset this quad along Y */
var()	float				YOffset;
/** If TRUE, UV range will just be 0-1, and not repeating based on RepeatMaxSize */
var()	bool				bDisableMaterialRepeat;

cpptext
{
	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
	virtual FColor GetRuleNodeTitleColor();	
}

defaultproperties
{
	NextRules.Empty // leaf node
	
	RepeatMaxSizeX=512.0
	RepeatMaxSizeZ=512.0
	
	QuadLightmapRes=32
}