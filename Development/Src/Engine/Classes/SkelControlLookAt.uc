/**
 *	Controller that rotates a single bone to 'look at' a given target.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SkelControlLookAt extends SkelControlBase
	native(Anim);

 
cpptext
{
	// USkelControlBase interface
	/** LookAtAlpha allows to cancel head look when going beyond boundaries */
	virtual	FLOAT	GetControlAlpha();
	virtual void	TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp);
	virtual void	GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices);
	virtual void	CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms);	
	
	virtual INT		GetWidgetCount() { return 1; }
	virtual FBoneAtom GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex);
	virtual void	HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec);
	virtual void	DrawSkelControl3D(const FSceneView* View, FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, INT BoneIndex);

	virtual void SetControlTargetLocation(const FVector& InTargetLocation);

protected:
	virtual UBOOL	ApplyLookDirectionLimits(FVector& DesiredLookDir, const FVector &CurrentLookDir, INT BoneIndex, USkeletalMeshComponent* SkelComp);

public:
}

/** Position in world space that this bone is looking at. */
var(LookAt)		vector				TargetLocation;

/** Reference frame that TargetLocation is defined in. */
var(LookAt)		EBoneControlSpace	TargetLocationSpace;

/** Name of bone used if TargetLocationSpace is BCS_OtherBoneSpace. */
var(LookAt)		name				TargetSpaceBoneName;

/** Axis of the controlled bone that you wish to point at the TargetLocation. */
var(LookAt)		EAxis				LookAtAxis;

/** Whether to invert the LookAtAxis, so it points away from the TargetLocation. */
var(LookAt)		bool				bInvertLookAtAxis;

/** If you want to also define which axis should try to point 'up' (world +Z). */
var(LookAt)		bool				bDefineUpAxis;

/** Axis of bone to point upwards. Cannot be the same as LookAtAxis. */
var(LookAt)		EAxis				UpAxis;

/** Whether to invert the UpAxis, so it points down instead. */
var(LookAt)		bool				bInvertUpAxis;

/** Interpolation speed for TargetLocation to DesiredTargetLocation */
var(LookAt)		float				TargetLocationInterpSpeed;

/** Interpolation target for TargetLocation */
var vector DesiredTargetLocation;
/** Actor Space Look At Normal. So interpolation done is relative to the owner */
var protected const transient vector ActorSpaceLookAtTarget;

/** If true, only allow a certain adjustment from the reference pose of the bone. */
var(Limit)		bool				bEnableLimit;
/** By default limit is based on ref pose of the character */
var(Limit)		bool				bLimitBasedOnRefPose;
/** Interp back to zero strength if limit surpassed */
var(Limit)		bool				bDisableBeyondLimit;
/** Call event to notify owner of limit break */
var(Limit)		bool				bNotifyBeyondLimit;

/** If true, draw a cone in the editor to indicate the maximum allowed movement of the bone. */
var(Limit)		bool				bShowLimit;

/** The maximum rotation applied from the reference pose of the bone, in degrees. */
var(Limit)		float				MaxAngle;

/** The outer maximum rotation applied from the reference pose of the bone, in degrees, that allows bone to stay in the MaxAngle */
var(Limit)		float				OuterMaxAngle;

/** Allowed error between the current look direction and the desired look direction. */
var(Limit)		float				DeadZoneAngle;

/** Per rotation axis filtering */
var(Limit)		bool				bAllowRotationX;
var(Limit)		bool				bAllowRotationY;
var(Limit)		bool				bAllowRotationZ;

/** Per rotation axis range of motion in degrees, relative to reference pose of the bone. First (negative) value is lowest relative angle, second is highest. */
var(Limit)      Vector2D            RotationAngleRangeX;
var(Limit)      Vector2D            RotationAngleRangeY;
var(Limit)      Vector2D            RotationAngleRangeZ;

/** Rotation space to check limits within */
var(Limit)		EBoneControlSpace	AllowRotationSpace;
var(Limit)		Name				AllowRotationOtherBoneName;

/** LookAtAlpha allows to cancel head look when going beyond boundaries */
var const transient	float	LookAtAlpha;
var const transient float	LookAtAlphaTarget;
var const transient float	LookAtAlphaBlendTimeToGo;

/** internal, used to draw base orientation for limits */
var const transient Vector	LimitLookDir;
/** Internal, base look dir, without skel controller's influence. */
var const transient Vector	BaseLookDir;
/** Internal, base bone position in component space. */
var const transient Vector	BaseBonePos;
/** 
 * Keep track of when the controller was last calculated.
 * We need this to make sure BaseLookDir is accurate, when using CanLookAt().
 */
var const transient float	LastCalcTime;

/** Cached control bone index - which one index is this skelcontrol for **/
var const transient INT     ControlBoneIndex;

/** Sets DesiredTargetLocation to a new location */
final virtual native function SetTargetLocation(Vector NewTargetLocation);

/** Interpolate TargetLocation towards DesiredTargetLocation based on TargetLocationInterpSpeed */
final native function InterpolateTargetLocation(float DeltaTime);

/** 
 * Set LookAtAlpha. 
 * Allows the controller to blend in/out while still being processed.
 * CalculateNewBoneTransforms() is still called when ControllerStength > 0 and LookAtAlpha <= 0
 */
final native function SetLookAtAlpha(float DesiredAlpha, float DesiredBlendTime);

/** 
 * returns TRUE if PointLoc is within cone of vision of look at controller. 
 * This requires the mesh to be rendered, SpaceBases has to be up to date.
 * @param	PointLoc		Point in world space.
 * @param	bDrawDebugInfo	if true, debug information will be drawn on hud.
 */
final native function bool CanLookAtPoint(vector PointLoc, optional bool bDrawDebugInfo, optional bool bDebugUsePersistentLines, optional bool bDebugFlushLinesFirst);

defaultproperties
{
	ActorSpaceLookAtTarget=(X=256.f)  // Looking straight ahead is default.
	LookAtAxis=AXIS_X
	UpAxis=AXIS_Z
	bShowLimit=TRUE
	bLimitBasedOnRefPose=TRUE

	TargetLocationInterpSpeed=10.f
	BlendInTime=0.33f
	BlendOutTime=0.33f

	LookAtAlphaTarget=1.f
	LookAtAlpha=1.f

	bAllowRotationX=TRUE
	bAllowRotationY=TRUE
	bAllowRotationZ=TRUE
	AllowRotationSpace=BCS_BoneSpace

	RotationAngleRangeX=(X=-90.f, Y=90.f)
	RotationAngleRangeY=(X=-90.f, Y=90.f)
	RotationAngleRangeZ=(X=-90.f, Y=90.f)

	// default outer max angle
	OuterMaxAngle=90
	bIgnoreWhenNotRendered=TRUE
	ControlBoneIndex = INDEX_NONE
}
