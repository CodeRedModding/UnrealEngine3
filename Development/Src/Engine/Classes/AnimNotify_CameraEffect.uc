/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_CameraEffect extends AnimNotify
	native(Anim);

/** The effect to play non the camera **/
var() class<EmitterCameraLensEffectBase> CameraLensEffect;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
	virtual FString GetEditorComment() { return TEXT("CameraEffect"); }
}

defaultproperties
{

}
