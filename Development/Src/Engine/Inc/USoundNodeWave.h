/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

public:	
	/** 
	 * UObject interface. 
	 */
	virtual void Serialize( FArchive& Ar );

	/** 
	 * Frees up all the resources allocated in this class
	 */
	void FreeResources( void );

	/**
	 * Returns whether the resource is ready to have finish destroy routed.
	 *
	 * @return	TRUE if ready for deletion, FALSE otherwise.
	 */
	UBOOL IsReadyForFinishDestroy();

	/**
	 * Frees the sound resource data.
	 */
	virtual void FinishDestroy( void );

	/**
	 * Outside the Editor, uploads resource to audio device and performs general PostLoad work.
	 *
	 * This function is being called after all objects referenced by this object have been serialized.
	 */
	virtual void PostLoad( void );

	/** 
	 * Invalidate compressed data
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** 
	 * Copy the compressed audio data from the bulk data
	 */
	void InitAudioResource( FByteBulkData& CompressedData );

	/** 
	 * Remove the compressed audio data associated with the passed in wave
	 */
	void RemoveAudioResource( void );

	/** 
	 * USoundNode interface.
	 */
	virtual void ParseNodes( UAudioDevice* AudioDevice, USoundNode* Parent, INT ChildIndex, class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );

	/** 
	 * Find an existing waveinstance attached to this audio component (if any)
	 */
	FWaveInstance* FindWaveInstance( UAudioComponent* AudioComponent, QWORD ParentGUID );

	/** 
	 * Handle any special requirements when the sound starts (e.g. subtitles)
	 */
	FWaveInstance* HandleStart( UAudioComponent* AudioComponent, QWORD ParentGUID );

	/** 
	 * SoundNodeWaves are always leaf nodes
	 */
	virtual INT GetMaxChildNodes( void ) 
	{ 
		return( 0 );  
	}

	/** 
	 * Gets the time in seconds of the associated sound data
	 */	
	virtual FLOAT GetDuration( void );

	/** 
	 * Prints the subtitle associated with the SoundNodeWave to the console
	 */
	void LogSubtitle( FOutputDevice& Ar );

	/** 
	 * Get the name of the class used to help out when handling events in UnrealEd.
	 * @return	String name of the helper class.
	 */
	virtual const FString GetEdHelperClassName( void ) const
	{
		return FString( TEXT( "UnrealEd.SoundNodeWaveHelper" ) );
	}

	/**
	 * Returns whether this wave file is a localized resource.
	 *
	 * @return TRUE if it is a localized resource, FALSE otherwise.
	 */
	virtual UBOOL IsLocalizedResource( void );

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize( void );

	/**
	 *	@param		Platform		EPlatformType indicating the platform of interest...
	 *
	 *	@return		Sum of the size of waves referenced by this cue for the given platform.
	 */
	virtual INT GetResourceSize( UE3::EPlatformType Platform );

	/** 
	 * Returns the name of the exporter factory used to export this object
	 * Used when multiple factories have the same extension
	 */
	virtual FName GetExporterName( void );

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc( void );

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString( void );

#if WITH_EDITOR
	/** 
 	 * Makes sure ogg vorbis data is available for this sound node by converting on demand
	 */
	UBOOL ValidateData( void );
#endif
