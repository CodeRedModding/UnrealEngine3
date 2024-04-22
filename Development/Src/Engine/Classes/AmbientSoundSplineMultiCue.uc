/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 


class AmbientSoundSplineMultiCue extends AmbientSoundSpline
	AutoExpandCategories( AmbientSoundSpline )
	native( Sound );

/** Index of currently edited sound-slot */
var(AmbientSoundSpline) editoronly int EditedSlot<ToolTip=Index of currently edited slot.>;

cpptext
{
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EditorResources.AmbientSoundIcons.S_Ambient_Sound_Simple'
		Scale=0.25
	End Object

	Components.Remove( AudioComponent1 )

	Begin Object Class=MultiCueSplineAudioComponent Name=AudioComponent2
		PreviewSoundRadius=DrawSoundRadius0
		bAutoPlay=false
		bStopWhenOwnerDestroyed=true
		bShouldRemainActiveIfDropped=true
	End Object
	AudioComponent=AudioComponent2
	Components.Add(AudioComponent2)	
}
