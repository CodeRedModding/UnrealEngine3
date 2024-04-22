/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class MultiCueSplineAudioComponent extends SplineAudioComponent
	native
	collapsecategories
	hidecategories(Object,ActorComponent)
	dependson(SoundNodeAttenuation)
	editinlinenew;

struct native MultiCueSplineSoundSlot
{
	var()	SoundCue	    SoundCue;
	var()	float			PitchScale;
	var()	float			VolumeScale;
	var()	int			    StartPoint;
	var()	int			    EndPoint;

	// To remember where the volumes are interpolating to and from
	var native const	    double			LastUpdateTime;
	var	native const	    float			SourceInteriorVolume;
	var	native const	    float			SourceInteriorLPF;
	var	native const	    float			CurrentInteriorVolume;
	var	native const	    float			CurrentInteriorLPF;

	//
	var bool                bPlaying;

	structdefaultproperties
	{
		PitchScale=1.0
		VolumeScale=1.0
		StartPoint=-1
		EndPoint=-1

		SourceInteriorVolume=1.0
		SourceInteriorLPF=1.0
		CurrentInteriorVolume=1.0
		CurrentInteriorLPF=1.0
		bPlaying=false
	}

	structcpptext
	{
		FMultiCueSplineSoundSlot()
		{
			SoundCue = NULL;
			PitchScale = 1.0f;
			VolumeScale = 1.0f;
			StartPoint = -1;
			EndPoint = -1;

			LastUpdateTime = 0.0;
			SourceInteriorVolume = 1.0f;
			SourceInteriorLPF = 1.0f;
			CurrentInteriorVolume = 1.0f;
			CurrentInteriorLPF = 1.0f;
			bPlaying=FALSE;
		}
	}
};

var( Sounds ) init	    array<MultiCueSplineSoundSlot>	SoundSlots<ToolTip=Sounds to play>;

var                     int                             CurrentSlotIndex;

cpptext
{
	/**
	 * Dissociates component from audio device and deletes wave instances.
	 */
	virtual void Cleanup( void );
	virtual void Play( void );
	virtual void Stop( void );

	virtual void UpdateWaveInstances( UAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const TArray<struct FListener>& InListeners, FLOAT DeltaTime );

	/** 
	 * @param InListeners all listeners list
	 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
	 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
	 */
	virtual FVector FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut );

	FLOAT GetDuration( );

	
	/**
	 *  Math helper function
	 *  @param Points - parray of points
	 *  @param Listener - position of listener
	 *  @param Radius - scope of listener
	 *  @param Slot - info about the sound range along spline
	 *  @param OutScaledDistance - out - distance , that should be used for attenuation calculation
	 *  @return mean virtual speaker position, with respect to distance from listener 
	 */ 
	static FVector FindVirtualSpeakerScaledPosition( const TArray< FInterpCurveVector::FPointOnSpline >& Points, FVector Listener, FLOAT Radius, const FMultiCueSplineSoundSlot& Slot, FLOAT& OutScaledDistance, INT& OutClosestPointOnSplineIndex );


}
