/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeRandom extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);
	

/** How many outputs are created for this node */
var()   INT     NumOutputs;
/** Min number of the outputs will be executed */
var()   INT     MinNumExecuted;
/** Max number of the outputs will be executed */
var()   INT     MaxNumExecuted;	

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
}


defaultproperties
{
	NumOutputs=2
	MinNumExecuted=1
	MaxNumExecuted=1
	
	NextRules.Empty
	NextRules[0]=(LinkName="0")
	NextRules[1]=(LinkName="1")	
}