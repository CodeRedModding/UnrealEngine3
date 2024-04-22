/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


//-----------------------------------------------------------
//
//-----------------------------------------------------------
class RB_DistanceJointSetup extends RB_ConstraintSetup
    native(Physics);

DefaultProperties
{
    LinearXSetup=(bLimited=1)
    LinearYSetup=(bLimited=1)
    LinearZSetup=(bLimited=1)
    bLinearLimitSoft=false
    bSwingLimited=false
}
