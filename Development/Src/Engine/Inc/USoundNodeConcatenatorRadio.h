/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:
	// USoundNode interface.
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	virtual INT GetMaxChildNodes( void ) 
	{ 
		return( -1 ); 
	}	

	/** 
	 * Returns TRUE if the radio chirp sound should be played
	 */
	UBOOL ApplyChirpSound( UAudioDevice* AudioDevice, UAudioComponent* AudioComponent, USoundNodeWave* Chirp );

	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished 
	 */
	virtual UBOOL NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance );

	/**
	 * Returns whether the node is finished after having been notified of buffer
	 * being finished.
	 *
	 * @param	AudioComponent	Audio component containing payload data
	 * @return	TRUE if finished, FALSE otherwise.
	 */
	UBOOL IsFinished( class UAudioComponent* AudioComponent );

	/**
	 * Returns the maximum duration this sound node will play for. 
	 * 
	 * @return maximum duration this sound will play for
	 */
	virtual FLOAT GetDuration( void );

	/**
	 * Concatenators have two connectors by default.
	 */
	virtual void CreateStartingConnectors( void );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
