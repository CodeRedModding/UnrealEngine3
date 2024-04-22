/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_HingeSetup extends RB_ConstraintSetup
	native(Physics);


defaultproperties
{
	bSwingLimited=true
	bTwistLimited=false

	Swing1LimitAngle=0.0
	Swing2LimitAngle=0.0
	TwistLimitAngle=45.0
}
