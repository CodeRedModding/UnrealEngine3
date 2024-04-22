/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
// This class of blend node will ramp the 'active' child up to 1.0

class AnimNodeBlendList extends AnimNodeBlendBase
	native(Anim)
	hidecategories(Object);

/** Array of target weights for each child. Size must be the same as the Children array. */
var		array<float>		TargetWeight;

/** How long before current blend is complete (ie. active child reaches 100%) */
var		float				BlendTimeToGo;

/** Child currently active - that is, at or ramping up to 100%. */
var		INT					ActiveChildIndex;

/** Call play anim when active child is changed */
var() bool	bPlayActiveChild;

/**
 * If TRUE (Default), then when the node becomes relevant, the Active Child will be forced to full weight.
 * This is a general optimization, as multiple nodes tend to change state at the same time, this will
 * reduce the maximum number of blends and animation decompression done at the same time.
 * Setting it to FALSE, will let the node interpolate animation normally.
 */
var(Performance) bool	bForceChildFullWeightWhenBecomingRelevant;

/**
 * if TRUE, do not blend when the Skeletal Mesh is not visible.
 * Optimization to save on blending time when meshes are not rendered.
 * Instant switch instead.
 */
var(Performance) bool	bSkipBlendWhenNotRendered;

/** slider position, for animtree editor */
var const	float	SliderPosition;

/** ActiveChildIndex for use in editor only, to debug transitions */
var() editoronly INT EditorActiveChildIndex;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// AnimNode interface
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	virtual void ResetAnimNodeToSource(UAnimNode *SourceNode);
	virtual	void TickAnim(FLOAT DeltaSeconds);

	// AnimTree editor interface
	virtual INT GetNumSliders() const { return 1; }
	virtual FLOAT GetSliderPosition(INT SliderIndex, INT ValueIndex);
	virtual void HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue);
	virtual FString GetSliderDrawValue(INT SliderIndex);

	// AnimNodeBlendBase interface
	virtual void OnAddChild(INT ChildNum);
	virtual void OnRemoveChild(INT ChildNum);

	// AnimNodeBlendList interface
	/** Called after (copy/)pasted - reset values or re-link if needed**/
	virtual void OnPaste();	

}

native function SetActiveChild( INT ChildIndex, FLOAT BlendTime );

defaultproperties
{
	bSkipBlendWhenNotRendered=TRUE
	bForceChildFullWeightWhenBecomingRelevant=TRUE
	Children(0)=(Name="Child1")
	bFixNumChildren=FALSE

	CategoryDesc = "BlendBy"
}
