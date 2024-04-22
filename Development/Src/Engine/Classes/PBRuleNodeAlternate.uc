/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeAlternate extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object)	
	dependson(ProcBuildingRuleset);

/** Axis that repeating should be done along */
var()   EProcBuildingAxis   RepeatAxis;
/** Fixed size of A parts (usually columnes) */
var()   float               ASize;
/** Maximum size of stretchable part between As, before inserting another A and B */
var()   float               BMaxSize;
/** If TRUE, pattern will be BABAB instead of ABABA */
var()   bool                bInvertPatternOrder;
/** If TRUE, A and B will be the same size. BMaxSize controls this size, and defines how many meshes will be inserted. */
var()   bool                bEqualSizeAB;

cpptext
{
	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
}

	
defaultproperties
{
	ASize=512.0
	BMaxSize=0.0
	
	NextRules[0]=(LinkName="A")
	NextRules[1]=(LinkName="B")	
}