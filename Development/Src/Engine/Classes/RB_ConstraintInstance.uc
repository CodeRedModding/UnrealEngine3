/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_ConstraintInstance extends Object
	hidecategories(Object)
	native(Physics);

cpptext
{
	// Object interface
	virtual void FinishDestroy();

	// RB_ConstraintInstance interface

	UBOOL TermConstraint(FRBPhysScene* Scene, UBOOL bFireBrokenEvent);

	void CopyInstanceParamsFrom(class URB_ConstraintInstance* fromInstance);
}

/**
 *	Actor that owns this constraint instance.
 *	Could be a ConstraintActor, or an actor using a PhysicsAsset containing this constraint.
 *	Due to the way the ConstraintInstance pooling works, this MUST BE FIRST PROPERTY OF THE CLASS.
 */
var		const transient Actor				Owner;

/** PrimitiveComponent containing this constraint. */
var		const transient PrimitiveComponent	OwnerComponent;

/**
 *	Indicates position of this constraint within the array in an PhysicsAssetInstance.
 *	Will correspond to RB_ConstraintSetup position in PhysicsAsset.
 */
var		const int					ConstraintIndex;


/** Physics scene index. */
var	native const int			SceneIndex;

/** Whether we are in the hardware or software scene. */
var native const bool			bInHardware;

/** Internal use. Physics-engine representation of this constraint. */
var		const native pointer	ConstraintData{class NxJoint};

var(Linear)		const	bool	bLinearXPositionDrive;
var(Linear)		const	bool	bLinearXVelocityDrive;

var(Linear)		const	bool	bLinearYPositionDrive;
var(Linear)		const	bool	bLinearYVelocityDrive;

var(Linear)		const	bool	bLinearZPositionDrive;
var(Linear)		const	bool	bLinearZVelocityDrive;

var(Linear)		const	vector	LinearPositionTarget;
var(Linear)		const	vector	LinearVelocityTarget;
var(Linear)		const	float	LinearDriveSpring;
var(Linear)		const	float	LinearDriveDamping;
var(Linear)		const	float	LinearDriveForceLimit;

var(Angular)	const	bool	bSwingPositionDrive;
var(Angular)	const	bool	bSwingVelocityDrive;

var(Angular)	const	bool	bTwistPositionDrive;
var(Angular)	const	bool	bTwistVelocityDrive;

var(Angular)	const	bool	bAngularSlerpDrive;

var(Angular)	const	quat	AngularPositionTarget;
var(Angular)	const	vector	AngularVelocityTarget; // Revolutions per second
var(Angular)	const	float	AngularDriveSpring;
var(Angular)	const	float	AngularDriveDamping;
var(Angular)	const	float	AngularDriveForceLimit;

/** Indicates that this constraint has been terminated */
var                     bool    bTerminated;

/** 
 *	If bMakeKinForBody1 is true, this is the kinematic body that is made for Body1. 
 *	Due to the way the ConstraintInstance pooling works, this MUST BE LAST PROPERTY OF THE CLASS.
 */
var	const native private pointer	DummyKinActor;

/** 
 *	Create physics engine constraint.
 *	If bMakeKinForBody1 is TRUE, then a non-colliding kinematic body is created for Body1 and used instead.
 */
final native function			InitConstraint(PrimitiveComponent PrimComp1, PrimitiveComponent PrimComp2, RB_ConstraintSetup Setup, float Scale, Actor InOwner, PrimitiveComponent InPrimComp, bool bMakeKinForBody1);

final native noexport function	TermConstraint();

/** Returns the PhysicsAssetInstance that owns this RB_ConstraintInstance (if there is one) */
final native function PhysicsAssetInstance GetPhysicsAssetInstance();

/** Get the position of this constraint in world space. */
final native function vector GetConstraintLocation();

final native function	SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);
final native function	SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);
final native function	SetAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);
final native function	SetAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);

final native function	SetLinearPositionTarget(vector InPosTarget);
final native function	SetLinearVelocityTarget(vector InVelTarget);
final native function	SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit);

final native function	SetAngularPositionTarget(const out quat InPosTarget);
final native function	SetAngularVelocityTarget(vector InVelTarget);
final native function	SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit);

/** Scale Angular Limit Constraints (as defined in RB_ConstraintSetup) */
final native function	SetAngularDOFLimitScale(float InSwing1LimitScale, float InSwing2LimitScale, float InTwistLimitScale, RB_ConstraintSetup InSetup);

/** Allows you to dynamically change the size of the linear limit 'sphere'. */
final native function	SetLinearLimitSize(float NewLimitSize);

/** If bMakeKinForBody1 was used, this function allows the kinematic body to be moved. */
final native function	MoveKinActorTransform(out matrix NewTM);

defaultproperties
{
	LinearDriveSpring=50.0
	LinearDriveDamping=1.0

	bAngularSlerpDrive=false

	AngularPositionTarget=(W=1.0)

	AngularDriveSpring=50.0
	AngularDriveDamping=1.0
}
