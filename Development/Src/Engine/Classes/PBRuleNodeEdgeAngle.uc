/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeEdgeAngle extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);

/** Enum to indicate the various edges of a scope */
enum EProcBuildingEdge
{
	EPBE_Top,
	EPBE_Bottom,
	EPBE_Left,
	EPBE_Right
};

/** Edge of scope that we want to look at angle of */
var()   EProcBuildingEdge   Edge;

/** Struct containing info about each decision angle */
struct native RBEdgeAngleInfo
{
	/** Angle (in degrees)  */
	var()   float    Angle;
	
	structdefaultproperties
	{
		Angle=0.0
	}
};

/** Set of angles of edge connection, each corresponds to an output of this node */
var()   array<RBEdgeAngleInfo>  Angles;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	
	// Editor
	virtual FString GetRuleNodeTitle();	
	virtual FString GetRuleNodeOutputName(INT ConnIndex);
	
	
	// PBRuleNodeEdgeAngle interface

	/** Update the NextRules array based on the Angles array */
	void UpdateRuleConnectors();
}

	
defaultproperties
{
	Edge=EPBE_Left
		
	NextRules.Empty
}