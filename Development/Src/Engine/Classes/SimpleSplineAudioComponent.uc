/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 *  Audio component used by AmbientSoundSimpleSpline. It finds position of sound source (virtual speaker) based on poredefined points list, and listener's position. 
 *  Moreover range of sound source along spline is considered while sound source position evaluation.
 */

class SimpleSplineAudioComponent extends SplineAudioComponent
	native
	collapsecategories
	hidecategories(Object,ActorComponent)
	dependson(SoundNodeAttenuation)
	editinlinenew;

struct native SplineSoundSlot
{
	var()	SoundNodeWave	Wave;
	var()	float			PitchScale;
	var()	float			VolumeScale;

	/** 
	 *  Indexes define range of sound sorce along spline.
	 */
	var()	int			    StartPoint;
	var()	int			    EndPoint;
	var()	float	        Weight;

	// To remember where the volumes are interpolating to and from
	var native const	    double			LastUpdateTime;
	var	native const	    float			SourceInteriorVolume;
	var	native const	    float			SourceInteriorLPF;
	var	native const	    float			CurrentInteriorVolume;
	var	native const	    float			CurrentInteriorLPF;
	

	structdefaultproperties
	{
		PitchScale=1.0
		VolumeScale=1.0
		StartPoint=-1
		EndPoint=-1
		Weight=1.0

		SourceInteriorVolume=1.0
		SourceInteriorLPF=1.0
		CurrentInteriorVolume=1.0
		CurrentInteriorLPF=1.0
	}

	structcpptext
	{
		FSplineSoundSlot()
		{
			Wave = NULL;
			PitchScale = 1.0f;
			VolumeScale = 1.0f;
			StartPoint = -1;
			EndPoint = -1;
			Weight = 1.0f;

			LastUpdateTime = 0.0;
			SourceInteriorVolume = 1.0f;
			SourceInteriorLPF = 1.0f;
			CurrentInteriorVolume = 1.0f;
			CurrentInteriorLPF = 1.0f;
		}
	}
};

/** The settings for attenuating with a low pass filter. */
var( LowPassFilter )	bool					bAttenuateWithLPF<ToolTip=Enable attenuation via low pass filter>;
var( LowPassFilter )	float					LPFRadiusMin<ToolTip=The range at which to start applying a low passfilter>;
var( LowPassFilter )	float					LPFRadiusMax<ToolTip=The range at which to apply the maximum amount of low pass filter>;

var( Attenuation )		float					dBAttenuationAtMax<ToolTip=The volume at maximum distance in deciBels>;

/** HACK, value is used only to edit OmniRadius */ 
var( Attenuation )		float					FlattenAttenuationRadius<ToolTip=At what distance to start blending the sound from as omnidirectional>;

/** What kind of attenuation model to use */
var( Attenuation )		SoundDistanceModel		DistanceAlgorithm<ToolTip=The type of volume versus distance algorithm to use>;

var( Attenuation )		float					RadiusMin<ToolTip=The range at which the sound starts attenuating>;
var( Attenuation )		float					RadiusMax<ToolTip=The range at which the sound has attenuated completely>;

var( Sounds ) init	    array<SplineSoundSlot>	SoundSlots<ToolTip=Sounds to play>;

/** A SoundNode needed for endless loop  */
var                     SoundNode	            NotifyBufferFinishedHook;

cpptext
{
	/**
	 * Dissociates component from audio device and deletes wave instances.
	 */
	virtual void Cleanup( void );
	virtual void UpdateWaveInstances( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const TArray<struct FListener>& InListeners, FLOAT DeltaTime );

	/** 
	 * @param InListeners all listeners list
	 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
	 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
	 */
	virtual FVector FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut );

	/**
	 * @return  point, that should be used for evaluation distance, between listener, and sound source. That distance is used for attenuation.
	 * The function is needed when the speaker sound's position is estimated from a shape (AmbientSoundSpline)
	 */
	virtual FVector GetPointForDistanceEval();

	/**
	 *  Math helper function
	 *  @param Points - parray of points
	 *  @param Listener - position of listener
	 *  @param Radius - scope of listener
	 *  @param Slot - info about the sound range along spline
	 *  @param OutScaledDistance - out - distance , that should be used for attenuation calculation
	 *  @return mean virtual speaker position, with respect to distance from listener 
	 */ 
	static FVector FindVirtualSpeakerScaledPosition( const TArray< FInterpCurveVector::FPointOnSpline >& Points, FVector Listener, FLOAT Radius, const FSplineSoundSlot& Slot, FLOAT& OutScaledDistance, INT& OutClosestPointOnSplineIndex );

protected:

	/**
	 * Inner function, handles single slot in UpdateWaveInstances.
	 */
	virtual void HandleSoundSlot( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const TArray<struct FListener>& InListeners, FSplineSoundSlot& Slot, INT ChildIndex);
}


defaultproperties
{
	dBAttenuationAtMax=-60

	FlattenAttenuationRadius = 800

	DistanceAlgorithm=ATTENUATION_Linear

	RadiusMin=200
	RadiusMax=1200
	
	LPFRadiusMin=3000
	LPFRadiusMax=6000

	Begin Object Class=ForcedLoopSoundNode Name=ForcedLoopSoundNode0
	End Object

	Begin Object Class=SoundCue Name=SoundCue0
		SoundClass=Ambient
		Duration=10000.0
		FirstNode=ForcedLoopSoundNode0 
	End Object

	SoundCue=SoundCue0
	NotifyBufferFinishedHook=ForcedLoopSoundNode0
	CueFirstNode=ForcedLoopSoundNode0
}