/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeCorner extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);

/** Amount to split off left (and maybe right) of supplied scope, based on left/right edge angles */
var()   float   CornerSize;


/** Struct containing info about each decision angle */
struct native RBCornerAngleInfo
{
	/** Angle (in degrees)  */
	var()   float   Angle;

	/** If non-zero, overrides the base CornerSize for this particular angle */
	var()	float	CornerSize;
	
	structdefaultproperties
	{
		Angle=0.0
		CornerSize=0.0
	}
};

/** Set of angles of corner angle, each corresponds to an output of this node */
var()   array<RBCornerAngleInfo>    Angles;

/** Angle in degrees at which point surfaces are considered co-planar, and corner mesh is not added */
var()   float                       FlatThreshold;

/** If TRUE, no space left or mesh added in concave corners. */
var()   bool                        bNoMeshForConcaveCorners;

/** How to adjust the roof/floor poly to fit with this corner mesh */
var()   EPBCornerType               CornerType;

/** How far from start of curve mesh region to actually chamfer/round adjust roff poly corner */
var()	float						CornerShapeOffset;

/** If CornerType is set to EPBC_Round, how many tesselation steps to take around the corner */
var()   int                         RoundTesselation;

/** Controls the curvature when using EPBC_Round- essentially 'pulls' the tangent handles further. */
var()   float                       RoundCurvature;

/** 
 *	If TRUE, look at face on the right to see how much gap to leave on right edge. If FALSE, just use CornerSize on left and right side 
 *	Note, this only works when rulesets have the same corner size down the entire height - when looking at adjacent face, the top-left corner size is used.
 */
var()	bool						bUseAdjacentRulesetForRightGap;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
	virtual class UPBRuleNodeCorner* GetCornerNode(UBOOL bTop, AProcBuilding* BaseBuilding, INT TopLevelScopeIndex);

	// Editor
	virtual FString GetRuleNodeTitle();	
	virtual FColor GetRuleNodeTitleColor();
	virtual void RuleNodeCreated(UProcBuildingRuleset* Ruleset);

	/** Update the NextRules array based on the Angles array */
	void UpdateRuleConnectors();	

	/** For a given angle, return the size that this corner node will use on the left of the face */
	FLOAT GetCornerSizeForAngle(FLOAT EdgeAngle);
}

	
defaultproperties
{
	CornerSize=256.0
	FlatThreshold=5.0	

	Angles[0]=(Angle=90.0)
	Angles[1]=(Angle=-90.0)

	CornerType=EPBC_Default
	RoundTesselation=4
	RoundCurvature=1.0
}