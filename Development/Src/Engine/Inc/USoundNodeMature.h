/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:
	// USoundNode interface.
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/**
	 * Mature nodes have two connectors by default.
	 */
	virtual void CreateStartingConnectors( void );

	virtual INT GetMaxChildNodes( void );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );

	/**
	 * Called after object and all its dependencies have been serialized.
	 */
	virtual void PostLoad();