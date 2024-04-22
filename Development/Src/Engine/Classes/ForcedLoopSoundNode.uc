/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Fake SoundNode used by SimpleSplineAudioComponent only to force endless loop.
 * Should not be used anywhere else.
 */
class ForcedLoopSoundNode extends SoundNode
	native( Sound );

cpptext
{
	virtual UBOOL NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance );
	virtual FLOAT GetDuration( );
	/** 
	 * Returns the maximum distance this sound can be heard from. Very large for looping sounds as the
	 * player can move into the hearable range during a loop.
	 */
	virtual FLOAT MaxAudibleDistance( FLOAT CurrentMaxDistance );
}
