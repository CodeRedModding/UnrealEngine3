/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:	
	/** 
	 * UObject interface. 
	 */
	virtual void Serialize( FArchive& Ar );

	/** 
	 * USoundNode interface. 
	 */

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
	virtual UBOOL IsFinished( class UAudioComponent* AudioComponent );

	/** 
	 * Returns the maximum distance this sound can be heard from. Very large for looping sounds as the
	 * player can move into the hearable range during a loop.
	 */
	virtual FLOAT MaxAudibleDistance( FLOAT CurrentMaxDistance ) 
	{ 
		return( WORLD_MAX ); 
	}

	/**
	 * Returns the maximum duration this sound node will play for. 
	 * 
	 * @return maximum duration this sound will play for
	 */
	virtual FLOAT GetDuration( void );

	/** 
	 * Returns whether the sound is looping indefinitely or not.
	 */
	virtual UBOOL IsLoopingIndefinitely( void );

	/** 
 	 * Process this node in the sound cue
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );