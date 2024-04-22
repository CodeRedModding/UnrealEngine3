/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ToggleConstraintDrive extends SequenceAction;

/** If TRUE, when the Enable Drive input is fired, will enable angular position drive on the attached constraint actor */
var()	bool	bEnableAngularPositionDrive;
/** If TRUE, when the Enable Drive input is fired, will enable angular velocity drive on the attached constraint actor */
var()	bool	bEnableAngularVelocityDrive;

/** If TRUE, when the Enable Drive input is fired, will enable linear position drive on the attached constraint actor */
var()	bool	bEnableLinearPositionDrive;
/** If TRUE, when the Enable Drive input is fired, will enable linear velocity drive on the attached constraint actor */
var()	bool	bEnableLinearvelocityDrive;

defaultproperties
{
	ObjName="Toggle Constraint Drive"
	ObjCategory="Physics"

	InputLinks(0)=(LinkDesc="Enable Drive")
	InputLinks(1)=(LinkDesc="Disable All Drive")	
}
