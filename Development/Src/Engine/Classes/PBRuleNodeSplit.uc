/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeSplit extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object)	
	dependson(ProcBuildingRuleset);


var()   EProcBuildingAxis   SplitAxis;

struct native RBSplitInfo
{
	var()   bool    bFixSize;
	var()   float   FixedSize;
	var()   float   ExpandRatio;
	var()   name    SplitName;
	
	structdefaultproperties
	{
		bFixSize=false
		FixedSize=512.0
		ExpandRatio=1.0
	}
};

var()   array<RBSplitInfo>  SplitSetup;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
	virtual FString GetRuleNodeOutputName(INT ConnIndex);
		
	
	// PBRuleNodeSplit interface

	/** Util to output array of size SplitSetup.Num(), indicating how to split a certain size using the splitting rules */
	TArray<FLOAT> CalcSplitSizes(FLOAT TotalSize);

	/** Update the NextRules array based on the RBSplitInfo array */
	void UpdateRuleConnectors();
}

	
defaultproperties
{
	SplitAxis=EPBAxis_Z
	
	NextRules.Empty
	NextRules[0]=(LinkName="Next")
	NextRules[1]=(LinkName="0")
}