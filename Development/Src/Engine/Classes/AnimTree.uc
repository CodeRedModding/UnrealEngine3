
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class AnimTree extends AnimNodeBlendBase
	native(Anim)
	hidecategories(Object);

/** Anim Tree template we were created from, or None if we are a template */
var()   const	AnimTree	AnimTreeTemplate;

/** Enable pooling for this AnimTree. This will pool freed copies of this AnimTree for later reuse */
var()   bool    bEnablePooling;

/** Definition of a group of AnimNodeSequences */
struct native AnimGroup
{
	/** Cached array of AnimNodeSequence nodes to update. */
	var	transient	const	Array<AnimNodeSequence> SeqNodes;
	/** Master node for synchronization. (Highest weight of the group) */
	var transient	const	AnimNodeSequence		SynchMaster;
	/** Master node for notifications. (Highest weight of the group) */
	var transient	const	AnimNodeSequence		NotifyMaster;
	/* Name of group. */
	var()			const	Name					GroupName;
	/** Rate Scale */
	var()			const	float					RateScale;
	/** Tracked Synch Position */
	var				const	float					SynchPctPosition;

	structdefaultproperties
	{
		RateScale=1.f
	}
};

/** List of animations groups */
var()	Array<AnimGroup>	AnimGroups;

/**
 * Skeleton Branches that should be composed first.
 * This is to solve Controllers relying on bones to be updated before them.
 */
var deprecated  Array<Name>	PrioritizedSkelBranches;

var()         Array<Name> ComposePrePassBoneNames;
var()         Array<Name> ComposePostPassBoneNames;

struct native SkelControlListHead
{
	/** Name of bone that this list of SkelControls will be executed after. */
	var	name			BoneName;

	/** First Control in the linked list of SkelControls to execute. */
	var editinline export SkelControlBase	ControlHead;

	/** For editor use. */
	var editoronly int				DrawY;
};

/** Root of tree of MorphNodes. */
var		editinline export array<MorphNodeBase>			RootMorphNodes;

/** Array of lists of SkelControls. Each list is executed after the bone specified using BoneName is updated with animation data. */
var		editinline export array<SkelControlListHead>	SkelControlLists;

/** Array for storing pose when bUseSavedPose is TRUE */
var		array<BoneAtom>			SavedPose;

/** If TRUE, AnimTree will always just return cached pose, rather than evaluating entire anim tree. */
var		bool					bUseSavedPose;

////// AnimTree Editor support

/** Y position of MorphNode input on AnimTree. */
var		editoronly int				MorphConnDrawY;

/** Used to avoid editing the same AnimTree in multiple AnimTreeEditors at the same time. */
var		editoronly transient bool	bBeingEdited;

/** Play rate used when previewing animations */
var()	editoronly float				PreviewPlayRate;

// DEPRECATED Preview data, editor only
var deprecated  editoronly SkeletalMesh		        PreviewSkelMesh;
var	deprecated  editoronly SkeletalMesh			    SocketSkelMesh;
var	deprecated  editoronly StaticMesh			    SocketStaticMesh;
var deprecated  editoronly Name					    SocketName;
var	deprecated  editoronly array<AnimSet>		    PreviewAnimSets;
var	deprecated  editoronly array<MorphTargetSet>	PreviewMorphSets;

/** Structure to hold a preview mesh, and a name for quick selection from within the editor */
struct native PreviewSkelMeshStruct
{
	/** Display name in combo box */
	var() Name                  DisplayName;
	/** Preview Skeletal Mesh */
	var() SkeletalMesh          PreviewSkelMesh;
	/** MorphTargetSets used when previewing this AnimTree in the AnimTreeEditor. */
	var() Array<MorphTargetSet>	PreviewMorphSets;
};
/** SkeletalMesh used when previewing this AnimTree in the AnimTreeEditor. */
var()   editoronly Array<PreviewSkelMeshStruct>    PreviewMeshList;
var     editoronly INT                             PreviewMeshIndex;

/** previewing of socket */
struct native PreviewSocketStruct
{
	/** Preview Name for quick selection */
	var() Name          DisplayName;
	/** Name of socket to use */
	var() Name          SocketName;
	/** Attached preview skeletal mesh */
	var() SkeletalMesh  PreviewSkelMesh;
	/** Attached preview staticmesh */
	var() StaticMesh    PreviewStaticMesh;
};
var()   editoronly  Array<PreviewSocketStruct>  PreviewSocketList;
var     editoronly  INT                         PreviewSocketIndex;

/** AnimSets used when previewing this AnimTree in the AnimTreeEditor. */
struct native PreviewAnimSetsStruct
{
	var() Name            DisplayName;
	var() Array<AnimSet>  PreviewAnimSets;
};
var()   editoronly  Array<PreviewAnimSetsStruct>    PreviewAnimSetList;
var     editoronly  INT                             PreviewAnimSetListIndex;

/** Preview AnimSet Index in the list - used to assign/preview for AnimSequenceNode **/
var     editoronly  INT                             PreviewAnimSetIndex;

/** Saved position of camera used for previewing skeletal mesh in AnimTreeEditor. */
var		editoronly vector		PreviewCamPos;

/** Saved orientation of camera used for previewing skeletal mesh in AnimTreeEditor. */
var		editoronly rotator		PreviewCamRot;

/** Saved position of floor mesh used for in AnimTreeEditor preview window. */
var		editoronly vector		PreviewFloorPos;

/** Saved yaw rotation of floor mesh used for in AnimTreeEditor preview window. */
var		editoronly int			PreviewFloorYaw;

var     editoronly array<AnimNodeFrame>     AnimNodeFrames;

////// End AnimTree Editor support

/** Keep track if ParentNodeArray has been built. Needs to happen in editor. Otherwise, we have to build it at runtime. */
var duplicatetransient BOOL bParentNodeArrayBuilt;
/** Copy of the AnimTickArray, to be serialized, and not rebuilt at run time. */
var duplicatetransient Array<AnimNode> AnimTickArray;
/** This is to notify (depending on version) to rebuild animtree. This information is saved during Serialize, and it will be used in InitAnimTree **/
/** was going to make this transient, but because of cooking problem, changing this to non-transient. Once tree is converted, it will be saved **/
var bool bRebuildAnimTickArray;

cpptext
{
	virtual void    PostLoad();
	virtual void	PostAnimNodeInstance(UAnimNode* SourceNode, TMap<UAnimNode*,UAnimNode*>& SrcToDestNodeMap);
	virtual void    ResetAnimNodeToSource(UAnimNode *SourceNode);

	virtual void	InitAnim(USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent);
	virtual void	GetBoneAtoms(FBoneAtomArray& Atoms, const TArray<BYTE>& DesiredBones, FBoneAtom& RootMotionDelta, INT& bHasRootMotion, FCurveKeyArray& CurveKeys);

	/** Optimisation way to see if this is a UAnimTree */
	virtual UAnimTree* GetAnimTree() { return this; }

	/** Remove from Sync Group **/
	void            RemoveFromSyncGroup(UAnimNodeSequence * SeqNode);
	void            SyncGroupPreTickUpdate();
	void			UpdateAnimNodeSeqGroups(FLOAT DeltaSeconds);
	void			PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** Get all SkelControls within this AnimTree. */
	void			GetSkelControls(TArray<USkelControlBase*>& OutControls);

	/** Get all MorphNodes within this AnimTree. */
	void			GetMorphNodes(TArray<class UMorphNodeBase*>& OutNodes);

	/** Call InitMorph on all morph nodes attached to the tree. */
	void			InitTreeMorphNodes(USkeletalMeshComponent* InSkelComp);

	/** Calls GetActiveMorphs on each element of the RootMorphNodes array. */
	void			GetTreeActiveMorphs(TArray<FActiveMorph>& OutMorphs);

	/** Make a copy of entire tree, including all AnimNodes and SkelControls. 
	 *  @param  NewTreeOuter    Component that will hold this 
	 *  @param  bAcceptPooled   If true, function is allowed to return pooled anim tree*/
	UAnimTree*		CopyAnimTree(UObject* NewTreeOuter, UBOOL bAcceptPooled = FALSE);

	/** Returns this Anim Tree to the global pool, if it is safe to do so */
	void            ReturnToPool();

	/** Utility function to return the size of the AnimTree pool */
	static INT      GetPoolSize();

	/** Utility function for copying a selection of nodes, maintaining any references between them, but leaving any external references. */
	static void		CopyAnimNodes(const TArray<class UAnimNode*>& SrcNodes, UObject* NewOuter, TArray<class UAnimNode*>& DestNodes, TMap<class UAnimNode*,class UAnimNode*>& SrcToDestNodeMap);

	/** Utility function for copying a selection of controls, maintaining any references between them, but leaving any external references. */
	static void		CopySkelControls(const TArray<class USkelControlBase*>& SrcControls, UObject* NewOuter, TArray<class USkelControlBase*>& DestControls, TMap<class USkelControlBase*,class USkelControlBase*>& SrcToDestControlMap);

	/** Utility function for copying a selection of morph nodes, maintaining any references between them, but leaving any external references. */
	static void		CopyMorphNodes(const TArray<class UMorphNodeBase*>& SrcNodes, UObject* NewOuter, TArray<class UMorphNodeBase*>& DestNodes, TMap<class UMorphNodeBase*,class UMorphNodeBase*>& SrcToDestNodeMap);


	// UAnimNode interface
	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);

	/**
	 * Draws this node in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawAnimNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight);

	/**
	* This returns total size for this animtree including animnodes, skelcontrols, morphnodes
	* This is only for ListAnimTress to get idea of how much size each tree cost in run-time
	* Do not use this in GetResourceSize as it will be calculate multiple times of same data
	*/
	INT GetTotalNodeBytes();

	/** 
	 *  Custom Serialize function
	 *  This function will save some information we can use in InitAnimTree
	 */
	void Serialize( FArchive& Ar );
}


native final function	SkelControlBase		FindSkelControl(name InControlName);

native final function	MorphNodeBase		FindMorphNode(name InNodeName);

/**
 *	When passing in TRUE, will cause tree to evaluate and then save resulting pose. From then on will continue to use that saved pose instead of re-evaluating the tree
 *	This feature is turned off when the SkeletalMesh changes
 */
native final function						SetUseSavedPose(bool bUseSaved);

//
// Anim Groups
//

/** Add a node to an existing anim group */
native final function bool				SetAnimGroupForNode(AnimNodeSequence SeqNode, Name GroupName, optional bool bCreateIfNotFound);
/** Returns the master node driving synchronization for this group. */
native final function AnimNodeSequence	GetGroupSynchMaster(Name GroupName);
/** Returns the master node driving notifications for this group. */
native final function AnimNodeSequence	GetGroupNotifyMaster(Name GroupName);
/** Force a group at a relative position. */
native final function					ForceGroupRelativePosition(Name GroupName, FLOAT RelativePosition);
/** Get the relative position of a group. */
native final function float				GetGroupRelativePosition(Name GroupName);
/** Adjust the Rate Scale of a group */
native final function					SetGroupRateScale(Name GroupName, FLOAT NewRateScale);
/** Get the Rate Scale of a group */
native final function float				GetGroupRateScale(Name GroupName);
/**
 * Returns the index in the AnimGroups list of a given GroupName.
 * If group cannot be found, then INDEX_NONE will be returned.
 */
native final function INT				GetGroupIndex(Name GroupName);

defaultproperties
{
	Children(0)=(Name="Child",Weight=1.0)
	bFixNumChildren=TRUE
	PreviewPlayRate=1.f
}
