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
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	virtual INT GetMaxChildNodes( void ) 
	{ 
		return( -1 );  
	}

	/**
	 * DistanceCrossFades have two connectors by default.
	 */
	virtual void CreateStartingConnectors( void );

	virtual void InsertChildNode( INT Index );
	virtual void RemoveChildNode( INT Index );

	virtual FLOAT MaxAudibleDistance( FLOAT CurrentMaxDistance );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
