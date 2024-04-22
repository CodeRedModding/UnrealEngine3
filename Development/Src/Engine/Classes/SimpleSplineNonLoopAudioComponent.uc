/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SimpleSplineNonLoopAudioComponent extends SimpleSplineAudioComponent
	native
	collapsecategories
	hidecategories(Object,ActorComponent)
	dependson(SoundNodeAttenuation)
	editinlinenew;

var( Randomized )		float					DelayMin<ToolTip=The lower bound of the delay in seconds>;
var( Randomized )		float					DelayMax<ToolTip=The upper bound of the delay in seconds>;

var( Randomized )		float					PitchMin<ToolTip=The lower bound of pitch (1.0 is no change)>;
var( Randomized )		float					PitchMax<ToolTip=The upper bound of pitch (1.0 is no change)>;

var( Randomized )		float					VolumeMin<ToolTip=The lower bound of volume (1.0 is no change)>;
var( Randomized )		float					VolumeMax<ToolTip=The upper bound of volume (1.0 is no change)>;

var                     int                     CurrentSlotIndex;
var                     float                   UsedVolumeModulation;
var                     float                   UsedPitchModulation;
var                     float                   NextSoundTime;

cpptext
{
	/**
	 * Reassign all randomized fields
	 */
	void Reshuffle();

	// following methods must call Reshuffle(), because they reset PlaybackTime variable
	virtual void Play( void );
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

protected:
	/**
	 * Inner function, handles single slot in UpdateWaveInstances.
	 */
	virtual void HandleSoundSlot( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const TArray<struct FListener>& InListeners, FSplineSoundSlot& Slot, INT ChildIndex);

}

defaultproperties
{
	DelayMin=0.0
	DelayMax=0.0

	PitchMin=1.0
	PitchMax=1.0

	VolumeMin=1.0
	VolumeMax=1.0

	UsedVolumeModulation=1.0
	UsedPitchModulation=1.0
	NextSoundTime=0.0
	CurrentSlotIndex=-1
}