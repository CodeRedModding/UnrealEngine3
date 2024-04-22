/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class RB_StayUprightSetup extends RB_ConstraintSetup
    native(Physics);

DefaultProperties
{
    LinearXSetup=(bLimited=0)
    LinearYSetup=(bLimited=0)
    LinearZSetup=(bLimited=0)
    bLinearLimitSoft=true
    bSwingLimited=true
    bSwingLimitSoft=true
}
