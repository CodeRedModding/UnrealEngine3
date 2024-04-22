/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PhATSkeletalMeshComponent extends SkeletalMeshComponent
	native;

cpptext
{
	// UPrimitiveComponent interface.
	virtual void Render(const FSceneView* View, class FPrimitiveDrawInterface* PDI);
	virtual void RenderHitTest(const FSceneView* View,class FPrimitiveDrawInterface* PDI);

	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	// PhATSkeletalMeshComponent interface
	void RenderAssetTools(const FSceneView* View, class FPrimitiveDrawInterface* PDI, UBOOL bHitTest);
	void DrawHierarchy(FPrimitiveDrawInterface* PDI, UBOOL bAnimSkel);
}

var transient native const pointer	PhATPtr;

/** Mesh-space matrices showing state of just animation (ie before physics) - useful for debugging! */
var transient native const array<AnimNode.BoneAtom>	AnimationSpaceBases;

defaultproperties
{
	Begin Object Class=AnimNodeSequence Name=AnimNodeSeq0
		bLooping=true
	End Object
	Animations=AnimNodeSeq0

	bHasPhysicsAssetInstance=false
	bUpdateKinematicBonesFromAnimation=true
	bUpdateJointsFromAnimation=true
	ForcedLodModel=1
	PhysicsWeight=1.0

	RBCollideWithChannels=(Default=TRUE)
}
