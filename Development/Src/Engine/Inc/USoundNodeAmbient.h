/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

protected:
	/** 
	 * USoundNode interface. 
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );

	/**
	 * Like above but returns ALL (not just active) nodes.
	 */
	virtual void GetAllNodes( TArray<USoundNode*>& SoundNodes ); 

	/**
	 * Used to calculate the maximum radius of the owning cue
	 */
	virtual FLOAT MaxAudibleDistance( FLOAT CurrentMaxDistance ) 
	{ 
		return( ::Max<FLOAT>( CurrentMaxDistance, RadiusMax ) ); 
	}

	virtual INT GetMaxChildNodes( void ) 
	{ 
		return( 0 ); 
	}

	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished 
	 */
	virtual UBOOL NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance );

	/**
	 * We're looping indefinitely so we're never finished.
	 *
	 * @param	AudioComponent	Audio component containing payload data
	 * @return	FALSE
	 */
	virtual UBOOL IsFinished( class UAudioComponent* /*Unused*/ ) 
	{ 
		return( FALSE ); 
	}

	/** 
	 * Gets the time in seconds of ambient sound - in this case INDEFINITELY_LOOPING_DURATION
	 */	
	virtual FLOAT GetDuration( void )
	{
		return( INDEFINITELY_LOOPING_DURATION );
	}

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
