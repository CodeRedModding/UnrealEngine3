/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_ForceField extends AnimNotify
	native(Anim);


/** Type of Forcefield **/
var() instanced NxForceFieldComponent ForceFieldComponent;

/** If this ForceField system should be attached to the location.**/
var() bool bAttach;

/** The socketname in which to attach the ForceField.  Looks for a socket name first then bone name **/
var() name SocketName;

/** The bone name in which to attach the ForceField. Looks for a socket name first then bone name **/
var() name BoneName;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
	virtual FString GetEditorComment() { return TEXT("ForceField"); }
}

defaultproperties
{
}
