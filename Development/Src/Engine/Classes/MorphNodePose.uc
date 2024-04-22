/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MorphNodePose extends MorphNodeBase
	native(Anim)
	hidecategories(Object);

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// MorphNodeBase interface
	virtual void GetActiveMorphs(TArray<FActiveMorph>& OutMorphs);
	virtual void InitMorphNode(USkeletalMeshComponent* InSkelComp);

	/** 
	 * Draws this morph node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 */	
	virtual void DrawMorphNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes);
}

/** Cached pointer to actual MorphTarget object. */
var		transient MorphTarget	Target;

/** 
 *	Name of morph target to use for this pose node. 
 *	Actual MorphTarget is looked up by name in the MorphSets array in the SkeletalMeshComponent.
 */
var()	name					MorphName;
 
/** default weight is 1.f. But it can be scaled for tweaking. */
var()	float					Weight;
 
/** 
 *	Set the MorphTarget to use for this MorphNodePose by name. 
 *	Will find it in the owning SkeletalMeshComponent MorphSets array using FindMorphTarget.
 */
native final function SetMorphTarget(Name MorphTargetName);

defaultproperties
{
	Weight=1.f
}