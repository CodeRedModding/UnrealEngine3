class AnimNodeBlendBase extends AnimNode
	native(Anim)
	hidecategories(Object)
	abstract;

cpptext
{
	// UAnimNode interface
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );
	virtual void GetNodes( TArray<UAnimNode*>& Nodes );
	virtual void GetBoneAtoms( TArray<FBoneAtom>& Atoms );
	virtual void DestroyChildren();

	virtual void DrawDebug(FRenderInterface* RI, FLOAT ParentPosX, FLOAT ParentPosY, FLOAT PosX, FLOAT PosY, FLOAT Weight);
	virtual FLOAT CalcDebugHeight();

	virtual void DrawAnimNode(FRenderInterface* RI, UBOOL bSelected);
	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);

	// UAnimNodeBlendBase interface



	/** Return the current weight of the supplied child. Returns 0.0 if this node is part of this blend. */
	FLOAT GetChildWeight(UAnimNode* Child);

	/** For debugging. Return the sum of the weights of all children nodes. Should always be 1.0. */
	FLOAT GetChildWeightTotal();

	/** Notification to this blend that a child UAnimNodeSequence has reached the end and stopped playing. Not called if child has bLooping set to true or if user calls StopAnim. */
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);

	/** Blends need to pass OnXXXXActive events to their children */

	virtual void OnBecomeActive();
	virtual void OnCeaseActive();
}

/** Link to a child AnimNode. */
struct native AnimBlendChild
{
	/** Name of link. */
	var() Name		Name;

	/** Child AnimNode. */
	var() editinline export AnimNode	Anim;

	/** Weight with which this child will be blended in. Sum of all weights in the Children array must be 1.0 */
	var float		Weight;

	/** Whether this child is currently active (ie. has a weight of > 0.0). */
	var int			bActive;

	/** For editor use. */
	var int			DrawY;
};

/** Array of children AnimNodes. These will be blended together and the results returned by GetBoneAtoms. */
var()	editinline export array<AnimBlendChild>		Children;

/** Whether children connectors (ie elements of the Children array) may be added/removed. */
var		bool					bFixNumChildren;

/** For editor use. */
var		int						DrawWidth;