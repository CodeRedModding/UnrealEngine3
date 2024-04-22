
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNodeBlendBase extends AnimNode
	native(Anim)
	hidecategories(Object)
	abstract;

/** Link to a child AnimNode. */
struct native AnimBlendChild
{
	/** Name of link. */
	var()					Name		Name;
	/** Child AnimNode. */
	var	editinline export	AnimNode	Anim;
	/** Weight with which this child will be blended in. Sum of all weights in the Children array must be 1.0 */
	var						float		Weight;
	/** Weight used for blending. See AnimBlendType. */
	var const transient		float		BlendWeight;
	/**
	 * Whether this child's skeleton should be mirrored.
	 * Do not use this lightly, mirroring is rather expensive.
	 * So minimize the number of times mirroring is done in the tree.
	 */
	var						bool		bMirrorSkeleton;
	/** Is Children Additive Animation. */
	var						bool		bIsAdditive;
	/** For editor use. */
	var	editoronly			int			DrawY;
};

/** Array of children AnimNodes. These will be blended together and the results returned by GetBoneAtoms. */
var	editfixedsize editinline export	array<AnimBlendChild>		Children;

/** Whether children connectors (ie elements of the Children array) may be added/removed. */
var						bool						bFixNumChildren;
/** Type of animation blending. Affects how the weight interpolates. */
var()	AlphaBlendType	BlendType;

cpptext
{
	/** Call DeferredInitAnim() if the node required it. Recurses through the Tree. Increase UAnimNode::CurrentSeachTag before calling. */
	virtual void CallDeferredInitAnim();

	// UAnimNode interface
	virtual	void TickAnim(FLOAT DeltaSeconds);

	virtual void BuildParentNodesArray();
	virtual void BuildTickArray(TArray<UAnimNode*>& OutTickArray);

	FORCEINLINE FLOAT	GetBlendWeight(FLOAT ChildWeight);
	FORCEINLINE void	SetBlendTypeWeights();
	virtual void GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);
	virtual void GetChildBoneAtoms( INT ChildIdx, FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys );

	/**
	 * Get mirrored bone atoms from desired child index.
	 * Bones are mirrored using the SkelMirrorTable.
	 */
	void GetMirroredBoneAtoms(FBoneAtomArray& Atoms, INT ChildIndex, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);

	/**
	 * Draws this node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawAnimNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight);
	virtual FString GetNodeTitle();

	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);
	virtual INT Extend2DSlider(FCanvas* Canvas, const FIntPoint &SliderPos, INT SliderWidth, UBOOL bAABBLiesWithinViewport, INT LoSliderHandleHeight) { return 0; }

	/** For debugging. Return the sum of the weights of all children nodes. Should always be 1.0. */
	FLOAT GetChildWeightTotal();

	/** Notification to this blend that a child UAnimNodeSequence has reached the end and stopped playing. Not called if child has bLooping set to true or if user calls StopAnim. */
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child, FLOAT PlayedTime, FLOAT ExcessTime);

	/** A child connector has been added */
	virtual void	OnAddChild(INT ChildNum);
	/** A child connector has been removed */
	virtual void	OnRemoveChild(INT ChildNum);
	/** A child Anim has been modified*/
	virtual void    OnChildAnimChange(INT ChildNum){};

	/** Rename all child nodes upon Add/Remove, so they match their position in the array. */
	virtual void	RenameChildConnectors();

	/** internal code for GetNodes(); should only be called from GetNodes() or from the GetNodesInternal() of this node's parent */
	virtual void GetNodesInternal(TArray<UAnimNode*>& Nodes);
	
	/** Called after (copy/)pasted - reset values or re-link if needed**/
	virtual void OnPaste();	

	/** 
	 * Resolve conflicts for blend curve weights if same morph target exists 
	 *
	 * @param	InChildrenCurveKeys	Array of curve keys for children. The index should match up with Children.
	 * @param	OutCurveKeys		Result output after blending is resolved
	 * 
	 * @return	Number of new addition to OutCurveKeys
	 */
	virtual INT BlendCurveWeights(const FArrayCurveKeyArray& InChildrenCurveKeys, FCurveKeyArray& OutCurveKeys);

protected:
	/** 
	 * Update Child Weight : Make sure childIndex isn't OOB
	 */
	virtual void UpdateChildWeight(INT ChildIndex);
}

native function PlayAnim(bool bLoop = false, float Rate = 1.0f, float StartTime = 0.0f);
native function StopAnim();
// calls PlayAnim with the current settings
native function ReplayAnim();

defaultproperties
{
	BlendType=ABT_Linear
}
