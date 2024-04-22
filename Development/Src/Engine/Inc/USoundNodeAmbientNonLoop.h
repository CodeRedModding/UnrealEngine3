/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

protected:
	/** 
	 * UObject interface. 
	 */
	virtual void Serialize( FArchive& Ar );

	/** 
	 * USoundNode interface. 
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished
	 */
	virtual UBOOL NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance );

	/** 
	 * Pick which slot to play next. 
	 */
	INT PickNextSlot( void );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
