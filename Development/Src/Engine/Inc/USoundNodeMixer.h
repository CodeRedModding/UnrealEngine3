/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:
	// USoundNode interface.

	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
	virtual INT GetMaxChildNodes( void ) 
	{ 
		return( -1 ); 
	}

	/**
	 * Mixers have two connectors by default.
	 */
	virtual void CreateStartingConnectors( void );

	/**
	 * Overloaded to add an entry to InputVolume.
	 */
	virtual void InsertChildNode( INT Index );

	/**
	 * Overloaded to remove an entry from InputVolume.
	 */
	virtual void RemoveChildNode( INT Index );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );