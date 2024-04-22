/**
 *	Abstract base class for a skeletal controller.
 *	A SkelControl is a module that can modify the position or orientation of a set of bones in a skeletal mesh in some programmtic way.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SkelControlBase extends AnimObject
	hidecategories(Object)
	native(Anim)
	abstract;


 /** Enum for controlling which reference frame a controller is applied in. */
enum EBoneControlSpace
{
	/** Set absolute position of bone in world space. */
	BCS_WorldSpace,

	/** Set position of bone in Actor's reference frame. */
	BCS_ActorSpace,

	/** Set position of bone in SkeletalMeshComponent's reference frame. */
	BCS_ComponentSpace,

	/** Set position of bone relative to parent bone. */
	BCS_ParentBoneSpace,

	/** Set position of bone in its own reference frame. */
	BCS_BoneSpace,

	/** Set position of bone in the reference frame of another bone. */
	BCS_OtherBoneSpace,

	/** Set position of socket/bone in Owner's Base Mesh. This is useful when this IK has to attach to the base mesh's specific location. */
	BCS_BaseMeshSpace,
};




/** Name used to identify this SkelControl. */
var(Controller)	name			ControlName;

/**
 *	Used to control how much affect this SkelControl has.
 *	1.0 means fully active, 0.0 means have no affect.
 *	Exactly how the control ramps up depends on the specific control type.
 */
var(Controller)	float			ControlStrength;

/** When calling SetActive passing in 'true', indicates how many seconds to take to reach a ControlStrength of 1.0. */
var(Controller)	float			BlendInTime;

/** When calling SetActive passing in 'false', indicates how many seconds to take to reach a ControlStrength of 0.0. */
var(Controller)	float			BlendOutTime;

/** BlendType - Easy in/out */
var(Controller) AlphaBlendType	BlendType;

/** If TRUE, this controller will be applied AFTER physics has been run, as part of the process of blending physics into the graphics skeleton. */
var(Controller) bool			bPostPhysicsController;

/** This is the slider bare at the bottom of the control.  Strength towards which we are currently ramping. This is used in the anim tree.*/
var				float			StrengthTarget;

/** Amount of time left in the currently active blend. */
var transient				float			BlendTimeToGo;

/** If true, Strength will be the same as given AnimNode(s). This is to make transitions easier between nodes and Controllers. */
var(Controller)	bool			bSetStrengthFromAnimNode;
/** List of AnimNode names, to get Strength from */
var(Controller)	Array<Name>		StrengthAnimNodeNameList;
/** Cached list of nodes to get strength from */
var	transient	Array<AnimNode>	CachedNodeList;
var transient	bool			bInitializedCachedNodeList;

//
// AnimMetadata Control
//
/** if TRUE, weight default to 0, and metadata in animation will enable the node based on animation weight. */
var(Controller) bool    bControlledByAnimMetada;
/** If TRUE, weight defaults to 1, and metadata will set that to zero, disabling the node */
var(Controller) bool    bInvertMetadataWeight;
/** Node weight when controlled by AnimMetadata */
var const transient float   AnimMetadataWeight;
/** Tag to update MetaData */
var	const transient	INT AnimMetaDataUpdateTag;

/** If true, calling SetSkelControlActive on this node will call SetSkelControlActive on the next one in the chain as well. */
var(Controller)	bool			bPropagateSetActive;

/** This scaling is applied to the bone that this control is acting upon. */
var(Controller) float			BoneScale;

/**
 *	Used to ensure we don't tick this SkelControl twice, if used in multiple different control chains.
 *	Compared against the SkeletalMeshComponent TickTag.
 */
var	transient			int				ControlTickTag;

/**
 *	whether this control should be ignored if the SkeletalMeshComponent being composed hasn't been rendered recently
 *
 *	@note this can be forced by the SkeletalMeshComponent's bIgnoreControllersWhenNotRendered flag
 */
var(Performance)			bool			bIgnoreWhenNotRendered;

/** Whether this skeletal controller should call the script TickSkelControl() event when ticked */
var bool bShouldTickInScript;

/** Call SkelControlTick() on Owner */
var bool bShouldTickOwner;

/** If true, when skel control becomes active it uses a curve rather than a linear blend to blend in more smoothly. */
/** This will change to BlendType in general **/
var() editconst deprecated bool			bEnableEaseInOut;

/** If the LOD of this skeletal mesh is at or above this LOD, then this SkelControl will not be applied. */
var(Performance)	int				IgnoreAtOrAboveLOD;

/** Next SkelControl in the linked list. */
var				SkelControlBase	NextControl;


/** Used by editor. */
var	deprecated	int				ControlPosX;

/** Used by editor. */
var	deprecated	int				ControlPosY;


cpptext
{
	USkelControlBase * GetSkelControlBase() { return this; }
	/**
	*	Called from the SkeletalMeshComponent Tick function, to allow SkelControls to do any time-based update,
	*	such as adjusting their current blend amount.
	*/
	virtual void TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp);

	/**
	*	Get the array of bones that this controller affects. Must be in hierarchy order, that is, parents before children.
	*/
	virtual void GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices) {}

	/**
	*	Calculate the new component-space transforms for the affected bones.
	*	The output array OutBoneTransforms must correspond to the OutBoneIndices array returned above (be in the same order etc).
	*/
	virtual void CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms) {}

	/**
	*	Allows you to modify the scaling of all affected bones.
	*	The output array OutBoneScales must correspond to the OutBoneIndices array returned above (be in the same order etc).
	*/
	virtual void CalculateNewBoneScales(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FLOAT>& OutBoneScales) {}

	/** Allows you to modify the scaling of the controller bone. */
	virtual FLOAT GetBoneScale(INT BoneIndex, USkeletalMeshComponent* SkelComp) { return BoneScale; }

	// UObject functions
	virtual void PostLoad();
	virtual void Serialize(FArchive& Ar);

	// UTILS

	/** Utility function for turning axis indicator enum into direction vector, possibly inverted. */
	static FVector GetAxisDirVector(BYTE InAxis, UBOOL bInvert);

	/**
	*	Create a matrix given two arbitrary rows of it.
	*	We generate the missing row using another cross product, but we have to get the order right to avoid changing handedness.
	*/
	static FMatrix BuildMatrixFromVectors(BYTE Vec1Axis, const FVector& Vec1, BYTE Vec2Axis, const FVector& Vec2);

	/** Given two unit direction vectors, find the axis and angle between them. */
	static void FindAxisAndAngle(const FVector& A, const FVector& B, FVector& OutAxis, FLOAT& OutAngle);

	// ANIMTREE EDITOR SUPPORT

	/**
	 * Draws this SkelControl in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 * @param	bShowWeight		If TRUE, show the global percentage weight of this node, if applicable.
	 */
	virtual void DrawNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes, UBOOL bShowWeight) { DrawSkelControl(Canvas, SelectedNodes); }
	
	/**
	 * Draws this SkelControl in the AnimTreeEditor.
	 *
	 * @param	Canvas			The canvas to use.
	 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
	 */
	virtual void DrawSkelControl(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes);

	/** For editor use. */
	FIntPoint GetConnectionLocation(INT ConnType);

	/** If we should draw manipulation widget in 3D viewport. */
	virtual INT GetWidgetCount() { return 0; }

	/** The transform to use when drawing the widget, in world space. */
	virtual FBoneAtom GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex) { return FMatrix::Identity; }

	/** Update properties of this controller based on the widget being manipulated in the 3D viewport. */
	virtual void HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec) {}

	/** Extra function for drawing special debug info in the 3D viewport if desired. */
	virtual void DrawSkelControl3D(const FSceneView* View, FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, INT BoneIndex) {}

	/** Called when slider is moved in the AnimTreeEditor. NewSliderValue is in range 0.0 to 1.0. */
	void HandleControlSliderMove(FLOAT NewSliderValue);

	/**
	* Get Alpha for this control. By default it is ControlStrength.
	* 0.f means no effect, 1.f means full effect.
	* ControlStrength controls whether or not CalculateNewBoneTransforms() is called.
	* By modifying GetControlAlpha() you can still get CalculateNewBoneTransforms() called
	* but not have the controller's effect applied on the mesh.
	* This is useful for cases where you need to have the skeleton built in mesh space
	* for testing, which is not available in TickSkelControl().
	*/
	virtual FLOAT	GetControlAlpha();

	/** Used by high level systems to assign a 'target' for a generic control. One example is the ControlTargets array in SkeletalMeshActor. */
	virtual void SetControlTargetLocation(const FVector& TargetLocation) {}
	/** Called after (copy/)pasted - reset values or re-link if needed**/
	virtual void OnPaste();
}


/**
 *	Toggle the active state of the SkeControl.
 *	If passing in true, will take BlendInTime to reach a ControlStrength of 1.0.
 *	If passing in false, will take BlendOutTime to reach a ControlStrength of 0.0.
 */
native final function SetSkelControlActive(bool bInActive);

/**
 * Set custom strength with optional blend time.
 * @param	NewStrength		Target Strength for this controller.
 * @param	InBlendTime		Time it will take to reach that new strength. (0.f == Instant)
 */
native final function SetSkelControlStrength(float NewStrength, float InBlendTime);

/** 
  *  Called every tick if bShouldTickInScript is true 
  */
event TickSkelControl(float DeltaTime, SkeletalMeshComponent SkelComp);

/** Accessor to get Metadataweight property value. */
native final function float GetControlMetadataWeight() const;

defaultproperties
{
	BoneScale=1.0
	ControlStrength=1.0
	StrengthTarget=1.0
	BlendInTime=0.2
	BlendOutTime=0.2
	IgnoreAtOrAboveLOD=1000
	bShouldTickInScript=FALSE
	bShouldTickOwner=FALSE
}
