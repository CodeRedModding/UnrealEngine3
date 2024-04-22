/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_Sound extends AnimNotify
	native(Anim);

var()	SoundCue	SoundCue;
var()	bool		bFollowActor;
var()	Name		BoneName;
var()	bool		bIgnoreIfActorHidden;

/** This is the percent to play this Sound.  Defaults to 100% (aka 1.0f) **/
var()   float       PercentToPlay;
var()   float		VolumeMultiplier;
var()	float		PitchMultiplier;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );

	virtual FString GetEditorComment() { return TEXT("Snd"); }
}

defaultproperties
{
	PercentToPlay=1.0f
	VolumeMultiplier=1.f
	PitchMultiplier=1.f
	bFollowActor=TRUE

	NotifyColor=(R=200,G=200,B=255)
}
