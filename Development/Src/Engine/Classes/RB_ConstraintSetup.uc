//=============================================================================
// Complete constraint definition used by rigid body physics.
// 
// Defaults here will give you a ball and socket joint.
// Positions are in Physics scale.
//=============================================================================


class RB_ConstraintSetup extends Object
	hidecategories(Object)
	native(Physics);


var()	const name JointName;


////////// CONSTRAINT GEOMETRY

var() name ConstraintBone1;
var() name ConstraintBone2;

// Body1 ref frame
var vector Pos1;
var vector PriAxis1;
var vector SecAxis1;

// Body2 ref frame
var vector Pos2;
var vector PriAxis2;
var vector SecAxis2;



// LINEAR DOF

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

	structdefaults
	{
		bLimited=1
		LimitSize=0.0
	}
};

// LINEAR DOF

var(Linear)	LinearDOFSetup	LinearXSetup;
var(Linear)	LinearDOFSetup	LinearYSetup;
var(Linear)	LinearDOFSetup	LinearZSetup;

var(Linear)		float		LinearStiffness;
var(Linear)		float		LinearDamping;

var(Linear)		bool		bLinearBreakable;
var(Linear)		float		LinearBreakThreshold;	

// ANGULAR DOF

var(Angular)	bool		bSwingLimited;
var(Angular)	bool		bTwistLimited;

var(Angular)	float		Swing1LimitAngle;	// Used if bSwing1Limited is true. In degrees.
var(Angular)	float		Swing2LimitAngle;	// Used if bSwing2Limited is true. In degrees.
var(Angular)	float		TwistLimitAngle;	// Used if bTwistLimited is true. In degrees.

var(Angular)	float		AngularStiffness;
var(Angular)	float		AngularDamping;

var(Angular)	bool		bAngularBreakable;
var(Angular)	float		AngularBreakThreshold;

cpptext
{
	// UObject interface
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	// Get/SetRefFrameMatrix only used in PhAT
	FMatrix GetRefFrameMatrix(INT BodyIndex);
	void SetRefFrameMatrix(INT BodyIndex, const FMatrix& RefFrame);

	void CopyConstraintGeometryFrom(class URB_ConstraintSetup* fromSetup);
	void CopyConstraintParamsFrom(class URB_ConstraintSetup* fromSetup);

	void DrawConstraint(struct FPrimitiveRenderInterface* PRI, 
		FLOAT Scale, UBOOL bDrawLimits, UBOOL bDrawSelected, UMaterialInstance* LimitMaterial,
		const FMatrix& Con1Frame, const FMatrix& Con2Frame);
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
}