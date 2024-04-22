/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class PBRuleNodeTransform extends PBRuleNodeBase
	native(ProcBuilding)
	collapsecategories
	hidecategories(Object);

/** Translation applied to to scope */
var()   DistributionVector      Translation;

/** Rotation (in degrees) applied to to scope */	
var()   DistributionVector      Rotation;

/** Scaling applied to to scope */
var()   DistributionVector      Scale;

cpptext
{
	// PBRuleNodeBase interface
	virtual void ProcessScope(FPBScope2D& InScope, INT TopLevelScopeIndex, AProcBuilding* BaseBuilding, AProcBuilding* ScopeBuilding, UStaticMeshComponent* LODParent);
}

defaultproperties
{

}