/*=============================================================================
	AudioDevice.h: Native AudioDevice calls
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

protected:
	friend class FSoundSource;

	/** Constructor */
	UAudioDevice( void ) 
	{
	}

	/**
	 * Add any referenced objects that shouldn't be garbage collected
	 */
	virtual void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	/**
	 * Complete the destruction of this class
	 */
	virtual void FinishDestroy( void );

	/**
	 * Handle pausing/unpausing of sources when entering or leaving pause mode
	 */
	void HandlePause( UBOOL bGameTicking );

	/**
	 * Stop sources that are no longer audible
	 */
	void StopSources( TArray<FWaveInstance*>& WaveInstances, INT FirstActiveIndex );

	/**
	 * Start and/or update any sources that have a high enough priority to play
	 */
	virtual void StartSources( TArray<FWaveInstance*>& WaveInstances, INT FirstActiveIndex, UBOOL bGameTicking );

	/**
	 * Lists all the loaded sounds and their memory footprint
	 */
	virtual void ListSounds( const TCHAR* Cmd, FOutputDevice& Ar )
	{
	}

	/**
	 * Sets the 'pause' state of sounds which are always loaded.
	 *
	 * @param	bPaused			Pause sounds if TRUE, play paused sounds if FALSE.
	 */
	virtual void PauseAlwaysLoadedSounds(UBOOL bPaused)
	{
	}

	/**
	 * Check for errors and output a human readable string
	 */
	virtual UBOOL ValidateAPICall( const TCHAR* Function, INT ErrorCode )
	{
		return( TRUE );
	}

	/**
	 * Lists all the playing waveinstances and their associated source
	 */
	void ListWaves( FOutputDevice& Ar );

	/**
	 * Lists a summary of loaded sound collated by class
	 */
	void ListSoundClasses( FOutputDevice& Ar );

	/**
	 * Parses the sound classes and propagates multiplicative properties down the tree.
	 */
	void ParseSoundClasses( void );

	/**
	 * Construct the CurrentSoundClassProperties map
	 *
	 * This contains the original sound class properties propagated properly, and all adjustments due to the sound mode
	 */
	void GetCurrentSoundClassState( void );

	/**
	 * Set the mode for altering sound class properties
	 */
	UBOOL ApplySoundMode( USoundMode* NewMode );

	/**
	 * Sets the sound class adjusters from the current sound mode. 
	 */
	void ApplyClassAdjusters();

	/**
	 * Recursively apply an adjuster to the passed in sound class and all children of the sound class
	 * 
	 * @param InAdjuster		The adjuster to apply
	 * @param InSoundClassName	The name of the sound class to apply the adjuster to.  Also applies to all children of this class
	 */
	void RecursiveApplyAdjuster( const struct FSoundClassAdjuster& InAdjuster, const class FName& InSoundClassName );

	/**
	 * Works out the interp value between source and end
	 */
	FLOAT Interpolate( DOUBLE EndTime );

	/**
	 * Gets the current state of the interior settings
	 */
	void GetCurrentInteriorSettings( void );

	/** 
	 * Apply the interior settings to ambient sounds
	 */
	void ApplyInteriorSettings( INT VolumeIndex, const FInteriorSettings& Settings );

	/**
	 * PostEditChange
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * Serialize
	 */
	virtual void Serialize( FArchive& Ar );

	/**
	 * Platform dependent call to init effect data on a sound source
	 */
	void* InitEffect( FSoundSource* Source );

	/**
 	 * Platform dependent call to update the sound output with new parameters
	 */
	void* UpdateEffect( FSoundSource* Source );

	/**
	 * Platform dependent call to destroy any effect related data
	 */
	void DestroyEffect( FSoundSource* Source );

	/**
	 * Return the pointer to the sound effects handler
	 */
	class FAudioEffectsManager* GetEffects( void )
	{
		return( Effects );
	}

	/** Internal */
	void SortWaveInstances( INT MaxChannels );

	/**
	 * Internal helper function used by ParseSoundClasses to traverse the tree.
	 *
	 * @param CurrentClass			Subtree to deal with
	 * @param ParentProperties		Propagated properties of parent node
	 */
	void RecurseIntoSoundClasses( class USoundClass* CurrentClass, FSoundClassProperties* ParentProperties );

public:
	/**
	 * Basic initialisation of the platform agnostic layer of the audio system
	 */
	virtual UBOOL Init( void );

	/**
	 * The audio system's main "Tick" function
	 */
	virtual void Update( UBOOL bGameTicking );

	/**
	 * Iterate over the active AudioComponents for wave instances that could be playing
	 */
	INT GetSortedActiveWaveInstances( TArray<FWaveInstance*>& WaveInstances, UBOOL bGameTicking );

	/**
	 * Exec
	 */
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar = *GLog );

	/**
	 * Stop all the audio components and sources attached to the scene. NULL scene means all components.
	 */
	virtual void Flush( FSceneInterface* Scene );

	/**
	 * Precaches the passed in sound node wave object.
	 *
	 * @param	SoundNodeWave	Resource to be precached.
	 */
	virtual void Precache( USoundNodeWave* SoundNodeWave )
	{
	}

	/**
	 * Frees the bulk resource data assocated with this SoundNodeWave.
	 *
	 * @param	SoundNodeWave	wave object to free associated bulk data
	 */
	virtual void FreeResource( USoundNodeWave* SoundNodeWave )
	{
	}

	/**
	 * Stops all game sounds (and possibly UI) sounds
	 *
	 * @param bShouldStopUISounds If TRUE, this function will stop UI sounds as well
	 */
	virtual void StopAllSounds( UBOOL bShouldStopUISounds = FALSE );

	/**
	 * Sets the listener's location and orientation for the viewport
	 */
	void SetListener( INT ViewportIndex, INT MaxViewportIndex, const FVector& Location, const FVector& Up, const FVector& Right, const FVector& Front, UBOOL bUpdateVelocity );

	/**
	 * Starts a transition to new reverb and interior settings
	 *
	 * @param   VolumeIndex			The object index of the volume
	 * @param	ReverbSettings		The reverb settings to use.
	 * @param	InteriorSettings	The interior settings to use.
	 */
	void SetAudioSettings( INT VolumeIndex, const FReverbSettings& ReverbSettings, const FInteriorSettings& InteriorSettings );

	/**
	 * Creates an audio component to handle playing a sound cue
	 */
	static UAudioComponent* CreateComponent( USoundCue* SoundCue, FSceneInterface* Scene, AActor* Actor = NULL, UBOOL Play = TRUE, UBOOL bStopWhenOwnerDestroyed = FALSE, FVector* Location = NULL );

	/**
	 * Adds a component to the audio device
	 */
	void AddComponent( UAudioComponent* AudioComponent );

	/**
	 * Removes an attached audio component
	 */
	void RemoveComponent( UAudioComponent* AudioComponent );

	/** 
	 * Gets the current audio debug state
	 */
	EDebugState GetMixDebugState( void );

	/**
	 * Set up the sound class hierarchy
	 */
	void InitSoundClasses( void );

	/**
	 * Load up all requested sound modes
	 */
	void InitSoundModes( void );

	/** 
	 * Gets a summary of loaded sound collated by class
	 */
	void GetSoundClassInfo( TMap<FName, FAudioClassInfo>& AudioClassInfos );

	/**
	 * Returns the base sound class tree 
	 *
	 * @param	SoundClassName	name of sound class to retrieve
	 * @return	sound class if it exists
	 */
	USoundClass* GetSoundClass( FName SoundClassName );

	/**
	 * Returns the soundclass that contains the current state of the mode system 
	 *
	 * @param	SoundClassName	name of sound class to retrieve
	 * @return	sound class properties if it exists
	 */
	FSoundClassProperties* GetCurrentSoundClass( FName SoundClassName );

	/**
	 * Updates sound class volumes
	 */
	void SetClassVolume( FName Class, FLOAT Volume );

	/**
	 * Checks to see if a coordinate is within a distance of any listener
	 */
	UBOOL LocationIsAudible( FVector Location, FLOAT MaxDistance );

	/**
	 * Gets the name of a sound class given a MenuID
	 */
	FName GetSoundClass( INT ID );

	/**
	 * Add a newly created sound class to the base set
	 */
	void AddClass( USoundClass* SoundClass );

	/**
	 * Removes a sound class
	 */
	void RemoveClass( USoundClass* SoundClass );

	/**
	 * Add a newly created sound mode to the base set
 	 */
	void AddMode( USoundMode* SoundMode );

	/**
	 * Removes a sound mode
	 */
	void RemoveMode( USoundMode* SoundMode );

	/**
	 * Creates a soundcue and PCM data for a the text in SpokenText
	 */ 
	USoundCue* CreateTTSSoundCue( const FString& SpokenText, ETTSSpeaker Speaker );

	/** 
	 * Resets all interpolating values to defaults.
	 */
	void ResetInterpolation( void );

	/**
	 * Enables or Disables the radio effect. 
	 */
	void EnableRadioEffect( UBOOL bEnable = FALSE );

	/** 
	 * Enables or Disables sound spawning.
	 */
	void SetSoundSpawningEnabled( UBOOL bEnable = TRUE);

	friend class FAudioEffectsManager;
	friend class FXAudio2EffectsManager;
	friend class FPS3AudioEffectsManager;

	friend class FXAudio2SoundSource;
	friend class FPS3SoundSource;
