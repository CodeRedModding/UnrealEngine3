/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_PlayFaceFXAnim extends SequenceAction
	native(Sequence);

/** Reference to FaceFX AnimSet package the animation is in */
var()	FaceFXAnimSet	FaceFXAnimSetRef;

/**
 *	Name of group within the FaceFXAsset to find the animation in. Case sensitive.
 */
var()	string			FaceFXGroupName;

/** 
 *	Name of FaceFX animation within the specified group to play. Case sensitive.
 */
var()	string			FaceFXAnimName;

/** The SoundCue to play with this FaceFX. **/
var() SoundCue SoundCueToPlay;

defaultproperties
{
	ObjName="Play FaceFX Anim"
	ObjCategory="Sound"

	InputLinks(0)=(LinkDesc="Play")
}
