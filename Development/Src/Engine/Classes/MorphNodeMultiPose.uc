/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MorphNodeMultiPose extends MorphNodeBase
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
	
	/**
	 * If exists, it returns Index in the array
	 * Returns -1 if fails
	 */
	INT ExistsIn(const FName & InName);
	INT ExistsIn(const UMorphTarget * InTarget);	
	
	/**
	 * Get weights if exists. Since weight list is editable, there is no guarantee this would exists. 
	 */
	FLOAT GetMorphTargetWeight(INT Index)
	{
		return ( Weights.Num() > Index )? Weights(Index):0.f;
	};
	
	/**
	 * Refresh all morph target information from internal data(name/weights)
	 */
	 void RefreshMorphTargets();
	 
	/** 
	 *	Clear all names and weights - deletes all data 
	 */
	void ClearAll();
}

/** Cached pointer to actual MorphTarget object. */
var		transient array<MorphTarget>	Targets;

/** 
 *	Name of morph target to use for this pose node. 
 *	Actual MorphTarget is looked up by name in the MorphSets array in the SkeletalMeshComponent.
 */
var()	array<name>					MorphNames;
 
/** default weight is 1.f. But it can be scaled for tweaking. */
var()	array<float>					Weights;
 
/** 
 *	Add the MorphTarget to use for this MorphNodeMultiPose by name. 
 *	Will find it in the owning SkeletalMeshComponent MorphSets array using FindMorphTarget.
 */
native final function bool AddMorphTarget(Name MorphTargetName, optional float InWeight = 1.0f);

/** 
 *	Remove the MorphTarget from using for this MorphNodeMultiPose by name. 
 *	Will find it in the owning SkeletalMeshComponent MorphSets array using FindMorphTarget.
 */
native final function RemoveMorphTarget(Name MorphTargetName);

/** 
 *	Update weight of the morph target 
 */
native final function bool UpdateMorphTarget(MorphTarget Target, float InWeight);

defaultproperties
{
}