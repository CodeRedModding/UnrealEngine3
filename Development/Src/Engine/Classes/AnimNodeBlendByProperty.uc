/**
 * AnimNodeBlendByProperty.uc
 * Looks at a specific property of the Pawn and will blend between two inputs based on its value
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeBlendByProperty extends AnimNodeBlendList
		native(Anim);

/** Property Name to look up */
var()	name		        PropertyName;
/** If Property should be looked up on the Owner's base instead of the Owner. */
var()   bool                bUseOwnersBase;
/** Name of cached property. Used to detect changes and invalidating the cached property.	*/
var		transient name		CachedPropertyName;
/** Cached property object pointer. Avoids slow FindField on a per tick basis, and cast. */
var const transient native Pointer  CachedFloatProperty{UFloatProperty};
var const transient native Pointer  CachedBoolProperty{UBoolProperty};
var const transient native Pointer  CachedByteProperty{UByteProperty};
/** Force an update on the node */
var const transient bool bForceUpdate;
/** Track Owner changes */
var     transient Actor     CachedOwner;

var()	float		BlendTime;
var()	float		FloatPropMin;
var()	float		FloatPropMax;

/** Use BlendToChild1Time/BlendToChild2Time instead of BlendTime? */
var()	bool		bUseSpecificBlendTimes;
var() 	float		BlendToChild1Time;
var()	float		BlendToChild2Time;

/** When moving the slider, keep nodes with same property in sync. */
var(Editor)	bool	bSynchronizeNodesInEditor;

cpptext
{
	virtual void InitAnim(USkeletalMeshComponent* MeshComp, UAnimNodeBlendBase* Parent);
	virtual	void TickAnim(FLOAT DeltaSeconds);
	virtual FString GetNodeTitle();
	virtual void HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue);
	/** Parent node is requesting a blend out. Give node a chance to delay that. */
	virtual UBOOL	CanBlendOutFrom();
	/** parent node is requesting a blend in. Give node a chance to delay that. */
	virtual UBOOL	CanBlendTo();
}

defaultproperties
{
	Children(0)=(Name="Child1")
	Children(1)=(Name="Child2")

	bSynchronizeNodesInEditor=TRUE
	bFixNumChildren=FALSE
	bForceChildFullWeightWhenBecomingRelevant=FALSE

	BlendTime=0.1

	BlendToChild1Time=0.1
	BlendToChild2Time=0.1

	FloatPropMin=0.0
	FloatPropMax=1.0
}
