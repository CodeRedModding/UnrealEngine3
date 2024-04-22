/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeVariation extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);
	
/** If TRUE, choose output based on variation of scope to left of this one, rather than this one. */
var()	bool	bVariationOfScopeOnLeft;

cpptext
{
	// PBRuleNodeVariation interface
	virtual void RegenVariationOutputs(UProcBuildingRuleset* Ruleset);
	INT GetVariationOutputIndex(AProcBuilding* BaseBuilding, INT TopLevelScopeIndex);

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);	
	virtual class UPBRuleNodeCorner* GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex);

	// Editor
	virtual FString GetRuleNodeTitle();	
	virtual void RuleNodeCreated(UProcBuildingRuleset* Ruleset);
}


defaultproperties
{
	NextRules[0]=(LinkName="Default")
}