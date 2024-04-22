/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:
	virtual void PostLoad();
	/** 
	 * USoundNode interface. 
	 */

	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished 
	 */
	virtual UBOOL NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance )
	{
		return( FALSE );
	}

	/**
	 * Returns whether the node is finished after having been notified of buffer
	 * being finished.
	 *
	 * @param	AudioComponent	Audio component containing payload data
	 * @return	TRUE if finished, FALSE otherwise.
	 */
	virtual UBOOL IsFinished( class UAudioComponent* /*Unused*/)
	{
		return( TRUE ); 
	}

	/** 
	 * Returns the maximum distance this sound can be heard from.
	 *
	 * @param	CurrentMaxDistance	The max audible distance of all parent nodes
	 * @return	FLOAT of the greater of this node's max audible distance and its parent node's max audible distance
	 */
	virtual FLOAT MaxAudibleDistance( FLOAT CurrentMaxDistance ) 
	{ 
		return( CurrentMaxDistance ); 
	}

	/** 
	 * Returns the maximum duration this sound node will play for. 
	 *
	 * @return	FLOAT of number of seconds this sound will play for. INDEFINITELY_LOOPING_DURATION means forever.
	 */
	virtual FLOAT GetDuration( void );

	/** 
	 * Check whether to apply the radio filter
	 */
	UBOOL ApplyRadioFilter( UAudioDevice* AudioDevice, UAudioComponent* AudioComponent );

	/** 
	 * Returns whether the sound is looping indefinitely or not.
	 */
	virtual UBOOL IsLoopingIndefinitely( void ) 
	{ 
		return( FALSE ); 
	}

	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/**
	 * Returns an array of active nodes
	 */
	virtual void GetNodes( class UAudioComponent* AudioComponent, TArray<USoundNode*>& SoundNodes );

	/**
	 * Like above but returns ALL (not just active) nodes.
	 */
	virtual void GetAllNodes( TArray<USoundNode*>& SoundNodes ); 

	/**
	 * Returns the maximum number of child nodes this node can possibly have
	 */
	virtual INT GetMaxChildNodes( void ) 
	{ 
		return( 1 ); 
	}

	/**
	 * Tool drawing
	 */
	virtual void DrawSoundNode( FCanvas* Canvas, const struct FSoundNodeEditorData& EdData, UBOOL bSelected );
	virtual FIntPoint GetConnectionLocation( FCanvas* Canvas, INT ConnType, INT ConnIndex, const struct FSoundNodeEditorData& EdData );

	/**
	 * Helper function to reset bFinished on wave instances this node has been notified of being finished.
	 *
	 * @param	AudioComponent	Audio component this node is used in.
	 */
	void ResetWaveInstances( UAudioComponent* AudioComponent );

	/** 
	 * Editor interface. 
	 */

	/** Get the name of the class used to help out when handling events in UnrealEd.
	 * @return	String name of the helper class.
	 */
	virtual const FString GetEdHelperClassName( void ) const
	{
		return( FString( TEXT( "UnrealEd.SoundNodeHelper" ) ) );
	}

	/**
	 * Called by the Sound Cue Editor for nodes which allow children.  The default behaviour is to
	 * attach a single connector. Dervied classes can override to eg add multiple connectors.
	 */
	virtual void CreateStartingConnectors( void );
	virtual void InsertChildNode( INT Index );
	virtual void RemoveChildNode( INT Index );

	/**
	 * Called when a property value from a member struct or array has been changed in the editor.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);	

	/**
	 * Calculate the attenuated volume using the available data
	 */
	static void CalculateAttenuatedVolume( UAudioComponent* AudioComponent, const BYTE DistanceModel, const FLOAT Distance, const FLOAT UsedMinRadius, const FLOAT UsedMaxRadius, const FLOAT dBAttenuationAtMax );

	/**
	 * Calculate the high shelf filter value
	 */	
	static void CalculateLPFComponent( UAudioComponent* AudioComponent, const FLOAT Distance, const FLOAT UsedLPFMinRadius, const FLOAT UsedLPFMaxRadius );

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );
