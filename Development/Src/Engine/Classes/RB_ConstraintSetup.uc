//=============================================================================
// Complete constraint definition used by rigid body physics.
// 
// Defaults here will give you a ball and socket joint.
// Positions are in Physics scale.
// When adding stuff here, make sure to update URB_ConstraintSetup::CopyConstraintParamsFrom
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


class RB_ConstraintSetup extends Object
	hidecategories(Object)
	native(Physics);

/** Name of bone that this joint is associated with. */
var()	const name JointName;

///////////////////////////// CONSTRAINT GEOMETRY

/** 
 *	Name of first bone (body) that this constraint is connecting. 
 *	This will be the 'child' bone in a PhysicsAsset.
 */
var() name ConstraintBone1;

/** 
 *	Name of second bone (body) that this constraint is connecting. 
 *	This will be the 'parent' bone in a PhysicsAset.
 */
var() name ConstraintBone2;

///////////////////////////// Body1 ref frame

/** Location of constraint in Body1 reference frame. Physics scale. */
var vector Pos1;

/** Primary (twist) axis in Body1 reference frame. */
var vector PriAxis1;

/** Seconday axis in Body1 reference frame. Orthogonal to PriAxis1. */
var vector SecAxis1;

///////////////////////////// Body2 ref frame

/** Location of constraint in Body2 reference frame. Physics scale. */
var vector Pos2;

/** Primary (twist) axis in Body2 reference frame. */
var vector PriAxis2;

/** Seconday axis in Body2 reference frame. Orthogonal to PriAxis2. */
var vector SecAxis2;

// Pulley info
var	vector	PulleyPivot1;
var	vector	PulleyPivot2;

/** 
 * If distance error between bodies exceeds 0.1 units, or rotation error exceeds 10 degrees, body will be projected to fix this.
 * For example a chain spinning too fast will have its elements appear detached due to velocity, this will project all bodies so they still appear attached to each other. 
 */
var()	bool	bEnableProjection;

///////////////////////////// LINEAR DOF

/** 
 *	Struct specying one Linear Degree Of Freedom for this constraint.
 *	Defaults to a ball-and-socket joint.
 */
struct native LinearDOFSetup
{
	/** Whether this DOF has any limit on it. */
	var() byte			bLimited;

	/** 
	 *	'Half-length' of limit gap. Can shift it by fiddling Pos1/2.
	 *	A size of 0.0 results in 'locking' the linear DOF.
	 */
	var() float			LimitSize; 

	structdefaultproperties
	{
		bLimited=1
		LimitSize=0.0
	}
};

// LINEAR DOF

var(Linear)	LinearDOFSetup	LinearXSetup;
var(Linear)	LinearDOFSetup	LinearYSetup;
var(Linear)	LinearDOFSetup	LinearZSetup;

var(Linear)		bool		bLinearLimitSoft;

var(Linear)		float		LinearLimitStiffness;
var(Linear)		float		LinearLimitDamping;

var(Linear)		bool		bLinearBreakable;
var(Linear)		float		LinearBreakThreshold;	

// ANGULAR DOF

var(Angular)	bool		bSwingLimited;
var(Angular)	bool		bTwistLimited;

var(Angular)	bool		bSwingLimitSoft;
var(Angular)	bool		bTwistLimitSoft;

var(Angular)	float		Swing1LimitAngle;	// Used if bSwing1Limited is true. In degrees.
var(Angular)	float		Swing2LimitAngle;	// Used if bSwing2Limited is true. In degrees.
var(Angular)	float		TwistLimitAngle;	// Used if bTwistLimited is true. In degrees.

var(Angular)	float		SwingLimitStiffness;
var(Angular)	float		SwingLimitDamping;

var(Angular)	float		TwistLimitStiffness;
var(Angular)	float		TwistLimitDamping;

var(Angular)	bool		bAngularBreakable;
var(Angular)	float		AngularBreakThreshold;

// PULLEY

var(Pulley)		bool		bIsPulley;
var(Pulley)		bool		bMaintainMinDistance;
var(Pulley)		float		PulleyRatio;

cpptext
{
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// Get/SetRefFrameMatrix only used in PhAT
	FMatrix GetRefFrameMatrix(INT BodyIndex);
	void SetRefFrameMatrix(INT BodyIndex, const FMatrix& RefFrame);

	void CopyConstraintGeometryFrom(class URB_ConstraintSetup* fromSetup);
	void CopyConstraintParamsFrom(class URB_ConstraintSetup* fromSetup);

	void DrawConstraint(class FPrimitiveDrawInterface* PDI, 
		FLOAT Scale, FLOAT LimitDrawScale, UBOOL bDrawLimits, UBOOL bDrawSelected, UMaterialInterface* LimitMaterial,
		const FMatrix& Con1Frame, const FMatrix& Con2Frame, UBOOL bDrawAsPoint);
}

defaultproperties
{
	Pos1=(X=0,Y=0,Z=0)
	PriAxis1=(X=1,Y=0,Z=0)
	SecAxis1=(X=0,Y=1,Z=0)

	Pos2=(X=0,Y=0,Z=0)
	PriAxis2=(X=1,Y=0,Z=0)
	SecAxis2=(X=0,Y=1,Z=0)

	LinearBreakThreshold=300.0
	AngularBreakThreshold=500.0
	
	PulleyRatio=1.0
}
