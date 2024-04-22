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

	// Editor interface.
	virtual void InsertChildNode( INT Index );
	virtual void RemoveChildNode( INT Index );

	// USoundNodeRandom interface
	void FixWeightsArray( void );
	void FixHasBeenUsedArray( void );

	/**
	 * Called after object and all its dependencies have been serialized.
	 */
	virtual void PostLoad();

	/**
	 * Called by the Sound Cue Editor for nodes which allow children.  The default behaviour is to
	 * attach a single connector. Dervied classes can override to e.g. add multiple connectors.
	 */
	virtual void CreateStartingConnectors( void );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );

