/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeSize extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);

/** Axis to check size of */
var()   EProcBuildingAxis   SizeAxis;

/** If size if less than this, fire < output, otherwise fire >= */
var()   float               DecisionSize;

/** If TRUE, uses the size of the entire building face, rather than just the area passed in to this rule */
var()	bool				bUseTopLevelScopeSize;

cpptext
{
	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
}

	
defaultproperties
{
	DecisionSize=512.0
	
	NextRules[0]=(LinkName="Less")
	NextRules[1]=(LinkName="Greater/Equal")
}