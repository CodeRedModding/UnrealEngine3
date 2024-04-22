/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

public:	
	/** 
	* USoundNode interface. 
	*/
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/** 
	* Used to create a unique string to identify unique nodes
	*/
	virtual FString GetUniqueString( void );

	/** 
	* Returns the maximum duration this sound node will play for. 
	*
	* @return	FLOAT of number of seconds this sound will play for. INDEFINITELY_LOOPING_DURATION means forever.
	*/
	virtual FLOAT GetDuration( void );

	/**
	* Called when a property on this object has been modified externally
	*
	* @param PropertyThatChanged the property that was modified
	*/
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent);