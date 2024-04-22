/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeExtractTopBottom extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object)	
	dependson(ProcBuildingRuleset);


var()   float   ExtractTopZ;
var()   float   ExtractNotTopZ;

var()   float   ExtractBottomZ;
var()   float   ExtractNotBottomZ;

cpptext
{
	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	virtual class UPBRuleNodeCorner* GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
}

	
defaultproperties
{
	ExtractTopZ=512.0
	ExtractNotTopZ=0.0
	
	ExtractBottomZ=512.0
	ExtractNotBottomZ=0.0

	NextRules[0]=(LinkName="Top")
	NextRules[1]=(LinkName="Not Top")
	NextRules[2]=(LinkName="Mid")
	NextRules[3]=(LinkName="Bottom")	
	NextRules[4]=(LinkName="Not Bottom")	
}