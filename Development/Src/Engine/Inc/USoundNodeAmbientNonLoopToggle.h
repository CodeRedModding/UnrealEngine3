/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

protected:
	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished
	 */
	virtual UBOOL NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
