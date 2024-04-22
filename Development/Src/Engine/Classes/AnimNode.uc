
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimNode extends AnimObject
	native(Anim)
	hidecategories(Object)
	abstract;

/** Enumeration for slider types */
enum ESliderType
{
	ST_1D,
	ST_2D
};

/** Curve Key
 @CurveName	: Morph Target name to blend
 @Weight	: Weight of the Morph Target
**/
struct CurveKey
{
	var name	CurveName;
	var float	Weight;
};

/** This node is considered 'relevant' - that is, has >0 weight in the final blend. */
var	transient const bool				bRelevant;
/** set to TRUE when this node became relevant this round of updates. Will be set to false on the next tick. */
var transient const bool				bJustBecameRelevant;
/** If TRUE, this node will be ticked, even if bPauseAnims is TRUE on the SkelMeshComp. */
var(Performance) bool					bTickDuringPausedAnims;
/** This node is editor only and used for something like placement preview */
var const bool                          bEditorOnly;

/** Used to avoid ticking a node twice if it has multiple parents. */
var	transient const int					NodeTickTag;
/** Initialization tag, for deferred InitAnim. */
var transient const INT                 NodeInitTag;
/** Used to avoid a node triggerring event twice if it has multiple parents. */
var	transient const int					NodeEndEventTick;
/** Index in AnimTick Array. Serialized, because we serialize TickArrayIndex in UAnimTree. */
var const int                           TickArrayIndex;
/** Used to indicate whether the BoneAtom cache for this node is up-to-date or not. */
var transient const int					NodeCachedAtomsTag;

/** Total apparent weight this node has in the final blend of all animations. */
var	const float							NodeTotalWeight;

/** Array of Parent nodes, which in most cases only has 1 element. */
var duplicatetransient Array<AnimNodeBlendBase> ParentNodes;

/** This is the name used to find an AnimNode by name from a tree. */
var() name								NodeName;

/** Temporarily disable caching when calling Super::GetBoneAtoms so it's not done multiple times. */
var const transient bool bDisableCaching;
/** If a node is linked to more than once in the graph, this is a cache of the results, to avoid re-evaluating the results. */
var	transient array<BoneAtom>			CachedBoneAtoms;
/** Num Desired Bones used in CachedBoneAtoms. If we request something different, CachedBoneAtoms array is not going to be valid. */
var transient byte						CachedNumDesiredBones;
/** Cached root motion delta, to avoid recalculating (see above). */
var transient BoneAtom					CachedRootMotionDelta;
/** Cached bool indicating if node supplies root motion, to avoid recalculating (see above). */
var transient int						bCachedHasRootMotion;
/** Cached curve keys to avoid recalculating (see above). */
var transient array<CurveKey>			CachedCurveKeys;

/** used when iterating over nodes via GetNodes() and related functions to skip nodes that have already been processed */
var  transient int SearchTag;

/** Array of blended curve key for editor only **/
var(Morph)	editoronly editconst transient array<CurveKey>	LastUpdatedAnimMorphKeys;

/** Flags to control if Script Events should be called. Note that those will affect performance, so be careful! */
var() bool bCallScriptEventOnInit;
var() bool bCallScriptEventOnBecomeRelevant;
var() bool bCallScriptEventOnCeaseRelevant;

cpptext
{
	UAnimNode * GetAnimNode() { return this;}
	// UAnimNode interface

	/** Do any initialisation, and then call InitAnim on all children. Should not discard any existing anim state though. */
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	/** Deferred Initialization, called only when the node is relevant in the tree. */
	virtual void DeferredInitAnim() {}
	/** Call DeferredInitAnim() if the node required it. Recurses through the Tree. Increase UAnimNode::CurrentSeachTag before calling. */
	virtual void CallDeferredInitAnim();

	/** AnimSets have been updated, update all animations */
	virtual void AnimSetsUpdated() {}

	/**
	 *	Called just after a node has been copied from its AnimTreeTemplate version.
	 *	This is called on the copy, and SourceNode is the node within the AnimTreeTemplate.
	 */
	virtual void PostAnimNodeInstance(UAnimNode* SourceNode, TMap<UAnimNode*,UAnimNode*>& SrcToDestNodeMap) {}

	/** 
	 *  Called when we need to reset our values to the source. This is called on a node that already has all pointers set up correctly
	 */
	virtual void ResetAnimNodeToSource(UAnimNode *SourceNode);

	/**
	 *	Update this node, then call TickAnim on all children.
	 *	@param DeltaSeconds		Amount of time to advance this node.
	 *	@param TotalWeight		The eventual weight that this node will have in the final blend. This is the multiplication of weights of all nodes above this one.
	 */
	virtual	void	TickAnim(FLOAT DeltaSeconds) {}

	/** Parent node is requesting a blend out. Give node a chance to delay that. */
	virtual UBOOL	CanBlendOutFrom() { return TRUE; }

	/** parent node is requesting a blend in. Give node a chance to delay that. */
	virtual UBOOL	CanBlendTo() { return TRUE; }

	/** 
	 * Add this node and all children to array. Node are added so a parent is always before its children in the array.
	 * @param bForceTraversal	Disables optimization when calling this from the root. (precached AnimTickArray returned).
	 */
	void GetNodes(TArray<UAnimNode*>& Nodes, bool bForceTraversal=FALSE);

	/** Add this node and all children of the specified class to array. Node are added so a parent is always before its children in the array. */
	void GetNodesByClass(TArray<class UAnimNode*>& Nodes, class UClass* BaseClass);

	/** Return an array with all UAnimNodeSequence childs, including this node. */
	void GetAnimSeqNodes(TArray<UAnimNodeSequence*>& Nodes, FName InSynchGroupName=NAME_None);

	virtual void BuildParentNodesArray();
	/** Used for building array of AnimNodes in 'tick' order - that is, all parents of a node are added to array before it. */
	virtual void BuildTickArray(TArray<UAnimNode*>& OutTickArray) {}

	/**
	 *	Get the local transform for each bone. If a blend, will recursively ask children and blend etc.
	 *	DesiredBones should be in strictly increasing order.
	 */
	virtual void GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);

	/**
	 *	Will copy the cached results into the OutAtoms array if they are up to date and return TRUE
	 *	If cache is not up to date, does nothing and retuns FALSE.
	 */
	virtual UBOOL GetCachedResults(FBoneAtomArray& OutAtoms, FBoneAtom& OutRootMotionDelta, INT& bOutHasRootMotion, FCurveKeyArray& OutCurveKeys, INT NumDesiredBones);

	/** Save the supplied array of BoneAtoms in the CachedBoneAtoms. */
    virtual UBOOL ShouldSaveCachedResults();
	void SaveCachedResults(const FBoneAtomArray& NewAtoms, const FBoneAtom& NewRootMotionDelta, INT bNewHasRootMotion, const FCurveKeyArray& NewCurveKeys, INT NumDesiredBones);

	/** 
	 *	Whether we should keep the cached result for the next frame or not 
	 *	This is to avoid keeping cached result once it ticks. 
	 *	It will release cache result if this returns FALSE
	 **/
    virtual UBOOL ShouldKeepCachedResult() { return FALSE; }

	/** 
	 * Clear Cached Result 
	 **/
	virtual void ClearCachedResult();

	/** Get notification that this node has become relevant for the final blend. ie TotalWeight is now > 0 */
	virtual void OnBecomeRelevant();

	/** Get notification that this node is no longer relevant for the final blend. ie TotalWeight is now == 0 */
	virtual void OnCeaseRelevant();

	/** Utility for counting the number of parents of this node that have been ticked. */
	UBOOL WereAllParentsTicked() const;

	/** Returns TRUE if this node is a child of Node */
	UBOOL IsChildOf(UAnimNode* Node);

	/** Returns TRUE if this node is a child of Node */
	UBOOL IsChildOf_Internal(UAnimNode* Node);

	/** Optimisation way to see if this is a UAnimTree */
	virtual UAnimTree* GetAnimTree() { return NULL; }

	virtual void SetAnim( FName SequenceName ) {}
	virtual void SetPosition( FLOAT NewTime, UBOOL bFireNotifies ) {}

	/** Override these functions to disable loading of editor only nodes */
	virtual UBOOL NeedsLoadForClient() const;
	virtual UBOOL NeedsLoadForServer() const;

	/// ANIMTREE EDITOR

	/**
	 * Draws this node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight) { DrawAnimNode(Canvas, SelectedNodes, bShowWeight); }
	
	/**
	 * Draws this anim node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawAnimNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight) {}

	/** Return title to display for this Node in the AnimTree editor. */
	virtual FString GetNodeTitle() { return TEXT(""); }

	/** For editor use. */
	virtual FIntPoint GetConnectionLocation(INT ConnType, int ConnIndex);

	/** Return the number of sliders */
	virtual INT GetNumSliders() const { return 0; }

	/** Return the slider type of slider Index */
	virtual ESliderType GetSliderType(INT InIndex) const { return ST_1D; }

	/** Return current position of slider for this node in the AnimTreeEditor. Return value should be within 0.0 to 1.0 range. */
	virtual FLOAT GetSliderPosition(INT SliderIndex, INT ValueIndex) { return 0.f; }

	/** Called when slider is moved in the AnimTreeEditor. NewSliderValue is in range 0.0 to 1.0. */
	virtual void HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue) {}

	/** Get the number to draw under the slider to show the current value being previewed. */
	virtual FString GetSliderDrawValue(INT SliderIndex) { return FString(TEXT("")); }

	/** internal code for GetNodes(); should only be called from GetNodes() or from the GetNodesInternal() of this node's parent */
	virtual void GetNodesInternal(TArray<UAnimNode*>& Nodes);

	/** Called after (copy/)pasted - reset values or re-link if needed**/
	virtual void OnPaste();

	// STATIC ANIMTREE UTILS

	/** flag to prevent calling GetNodesInternal() from anywhere besides GetNodes() or another GetNodesInternal(), since
	 * we can't make it private/protected because UAnimNodeBlendBase needs to be able to call it on its children
	 */
	static UBOOL bNodeSearching;
	/** current tag value used for SearchTag on nodes being iterated over. Incremented every time a new search is started */
	static INT CurrentSearchTag;
	/** Array to keep track of those nodes requiring an actual clear of the cache */
	static TArray<UAnimNode*> NodesRequiringCacheClear;

	/**
	 * Fills the Atoms array with the specified skeletal mesh reference pose.
	 *
	 * @param Atoms				[out] Output array of relative bone transforms. Must be the same length as RefSkel when calling function.
	 * @param DesiredBones		Indices of bones we want to modify. Parents must occur before children.
	 * @param RefSkel			Input reference skeleton to create atoms from.
	 */
	static void FillWithRefPose(TArray<FBoneAtom>& Atoms, const TArray<BYTE>& DesiredBones, const TArray<struct FMeshBone>& RefSkel);
	static void FillWithRefPose(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, const TArray<struct FMeshBone>& RefSkel);

	/** Utility for taking an array of bone indices and ensuring that all parents are present (ie. all bones between those in the array and the root are present). */
	static void EnsureParentsPresent( TArray<BYTE>& BoneIndices, USkeletalMesh* SkelMesh );

	/** Utility functions to ease off Casting */
	virtual class UAnimNodeSlot* GetAnimNodeSlot() { return NULL; }
	virtual class UAnimNodeAimOffset* GetAnimNodeAimOffset() { return NULL; }
	virtual class UAnimNodeSequence* GetAnimNodeSequence() { return NULL; }
}

/** Called from InitAnim. Allows initialization of script-side properties of this node. */
event OnInit();
/** Get notification that this node has become relevant for the final blend. ie TotalWeight is now > 0 */
event OnBecomeRelevant();
/** Get notification that this node is no longer relevant for the final blend. ie TotalWeight is now == 0 */
event OnCeaseRelevant();

/**
 * Find an Animation Node in the Animation Tree whose NodeName matches InNodeName.
 * Will search this node and all below it.
 * Warning: The search is O(n^2), so for large AnimTrees, cache result.
 */
native final function AnimNode FindAnimNode(name InNodeName);

native function PlayAnim(bool bLoop = false, float Rate = 1.0f, float StartTime = 0.0f);
native function StopAnim();
// calls PlayAnim with the current settings
native function ReplayAnim();	

