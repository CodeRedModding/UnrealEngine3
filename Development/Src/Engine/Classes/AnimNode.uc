class AnimNode extends Object
	native(Anim)
	hidecategories(Object)
	abstract;

struct BoneAtom
{
	var	quat	Rotation;
	var	vector	Translation;
	var vector	Scale;
};

/** SkeletalMeshComponent that this animation blend tree is feeding. */
var transient SkeletalMeshComponent		SkelComponent;

/** Parent node of this AnimNode in the blend tree. */
var transient AnimNodeBlendBase			ParentNode;

/** This is the name used to find an AnimNode by name from a tree. */
var() name								NodeName;

/** For debugging. Small icon texture drawn for this node when viewing tree in-game. */
var texture2D							DebugIcon;


/** For editor use. */
var float								DebugHeight;


/** For editor use. */
var	int									NodePosX;

/** For editor use. */
var int									NodePosY;

/** For editor use. */
var int									OutDrawY;


cpptext
{
	// UObject interface
	virtual void Destroy();

	// UAnimNode interface

	/** Do any initialisation, and then call InitAnim on all children. Should not discard any existing anim state though. */
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );

	/** 
	 *	Update this node, then call TickAnim on all children. 
	 *	@param DeltaSeconds		Amount of time to advance this node.
	 *	@param TotalWeight		The eventual weight that this node will have in the final blend. This is the multiplication of weights of all nodes above this one.
	 */
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight ) {}

	/** Add this node and all children to array. Node are added so a parent is always before its children in the array. */
	virtual void GetNodes( TArray<UAnimNode*>& Nodes );

	/** Get the local transform for each bone. If a blend, will recursively ask children and blend etc. */
	virtual void GetBoneAtoms( TArray<FBoneAtom>& Atoms );

	/** Get notification that this node has just become active ie. now has weight of more than 0.0. Note, this is only weight within parent blend, not overall. */
	virtual void OnBecomeActive();

	/** Get notification that this node's parent has just become active. */
	virtual void OnParentBecomeActive();

	/** Get notification that this node is stopped being active ie. now has weight of 0.0. See OnBecomeActive. */
	virtual void OnCeaseActive();

	/** Get notification that this node's parent has stopped being active */
	virtual void OnParentCeaseActive();

	/** Delete all children AnimNode Objects. */
	virtual void DestroyChildren() {}

	/** Find a node whose NodeName matches InNodeName. Will search this node and all below it. */
	UAnimNode* FindAnimNode(FName InNodeName);


	virtual void SetAnim( FName SequenceName ) {}
	virtual void PlayAnim( UBOOL bLoop=false, FLOAT Rate=1.0f, FLOAT StartTime=0.0f ) {}
	virtual void StopAnim() {}
	virtual void SetPosition( FLOAT NewTime ) {}

	/// DEBUGGING

	/** Draw debug info about this node. Will call this on its children. */
	virtual void DrawDebug(FRenderInterface* RI, FLOAT ParentPosX, FLOAT ParentPosY, FLOAT PosX, FLOAT PosY, FLOAT Weight);

	/** Recalculate heights used when displaying debug tree view. */
	virtual FLOAT CalcDebugHeight();


	/// ANIMTREE EDITOR

	/** For editor use. */
	virtual void DrawAnimNode(FRenderInterface* RI, UBOOL bSelected) {}

	/** For editor use. */
	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);


	// STATIC ANIMTREE UTILS

	/** Take the parent-space bone transform atoms and back into mesh-space transforms. Need RefSkel for parent info. */
	static void ComposeSkeleton( TArray<FBoneAtom>& LocalTransforms, TArray<struct FMeshBone>& RefSkel, TArray<FMatrix>& MeshTransforms, USkeletalMeshComponent* meshComp);

	/** Use supplied reference skeleton pose information to fill the in the array of FBoneAtoms with a 'default' pose. */
	static void FillWithRefPose( TArray<FBoneAtom>& Atoms, TArray<struct FMeshBone>& RefSkel );
}

/** Script version of OnBecomeActive. */
event OnBecomeActive();	

/** Script version of OnCeaseActive. */
event OnCeaseActive();

/** Called from InitAnim. Allows initialisation of script-side properties of this node. */
event OnInit();

/** Find a node whose NodeName matches InNodeName. Will search this node and all below it. */
native final function AnimNode FindAnimNode(name InNodeName);

defaultproperties
{
	DebugIcon=Texture2D'EngineResources.AnimBlendIcon'
}
