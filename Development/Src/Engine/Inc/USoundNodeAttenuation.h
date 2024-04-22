/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:
	/** 
	 * USoundNode interface. 
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/**
	 * Used to calculate the maximum radius of the owning cue
	 */
	virtual FLOAT MaxAudibleDistance( FLOAT CurrentMaxDistance ) 
	{ 
		return( ::Max<FLOAT>( CurrentMaxDistance, RadiusMax ) ); 
	}

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
