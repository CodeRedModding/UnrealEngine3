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

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );