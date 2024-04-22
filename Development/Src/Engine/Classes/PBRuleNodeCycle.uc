/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeCycle extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object)	
	dependson(ProcBuildingRuleset);

/** Axis to break input scope up along */
var()   EProcBuildingAxis   RepeatAxis;

/** How big each repeat should be */
var()   float               RepeatSize;

/** How big each 'cycle' is (ie how many outputs will be created */
var()   int                 CycleSize;

/** Whether each output should be a fixed size (and hence output a scope from the Remainder output). If FALSE, acts just like Repeat Rule. */
var()   bool                bFixRepeatSize;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);	

	// Editor
	virtual FString GetRuleNodeTitle();	

	// PBRuleNodeCycle
	/** Util to regenerate the outputs, base on CycleSize */
	void UpdateOutputs();
}

	
defaultproperties
{
	RepeatAxis=EPBAxis_Z
	RepeatSize=512
	CycleSize=2

	NextRules.Empty
	NextRules[0]=(LinkName="Remainder")
	NextRules[1]=(LinkName="Step 0")
	NextRules[2]=(LinkName="Step 1")
}