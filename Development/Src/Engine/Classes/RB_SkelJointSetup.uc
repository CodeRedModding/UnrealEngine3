/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_SkelJointSetup extends RB_ConstraintSetup
	native(Physics);


defaultproperties
{
	bSwingLimited=true
	bTwistLimited=true

	Swing1LimitAngle=45.0
	Swing2LimitAngle=45.0
	TwistLimitAngle=15.0
}
