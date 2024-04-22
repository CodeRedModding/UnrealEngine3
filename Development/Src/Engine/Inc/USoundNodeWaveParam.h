/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:	
	/** 
	 * Return the maximum number of child nodes; normally 0 to 2
	 */
	virtual INT GetMaxChildNodes( void )
	{
		return( 1 );
	}

	/** 
	 * Gets the time in seconds of the associated sound data
	 */	
	virtual FLOAT GetDuration( void );

	/** 
	 * USoundNode interface.
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/** 
	 *
	 */
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );

