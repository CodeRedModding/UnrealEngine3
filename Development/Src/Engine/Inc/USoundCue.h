/*=============================================================================
	SoundCue.h: Native SoundCue calls
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

protected:
	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	/** 
	 * Remap sound locations through portals
	 */
	FVector RemapLocationThroughPortals( const FVector& SourceLocation, const FVector& ListenerLocation );

	/** 
	 * Calculate the maximum audible distance accounting for every node
	 */
	void CalculateMaxAudibleDistance( void );

public:

	/** 
	* Populate the enum using the serialised fname
	*/
	void Fixup( void );

	/**
	* Called when a property value from a member struct or array has been changed in the editor.
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UObject interface.
	virtual void Serialize( FArchive& Ar );

	/**
	 * @return		Sum of the size of waves referenced by this cue.
	 */
	virtual INT GetResourceSize( void );

	/**
	 *	@param		Platform		EPlatformType indicating the platform of interest...
	 *
	 *	@return		Sum of the size of waves referenced by this cue for the given platform.
	 */
	virtual INT GetResourceSize( UE3::EPlatformType Platform );

	/**
	 * Returns a description of this object that can be used as extra information in list views.
	 */
	virtual FString GetDesc( void );

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/** 
	 * Does a simple range check to all listeners to test hearability
	 */
	UBOOL IsAudibleSimple( FVector* Location );

	/**
	 * Checks to see if a location is audible
	 */
	UBOOL IsAudible( const FVector& SourceLocation, const FVector& ListenerLocation, AActor* SourceActor, INT& bIsOccluded, UBOOL bCheckOcclusion );

	/**
	 * Recursively finds all Nodes in the Tree
	 */
	void RecursiveFindAllNodes( USoundNode* Node, TArray<class USoundNode*>& OutNodes );

	/**
	 * Recursively finds sound nodes of type T
	 */
	template<typename T> void RecursiveFindNode( USoundNode* Node, TArray<T*>& OutNodes );

	/**
	 * Instantiate certain functions to work around a linker issue
	 */
	void RecursiveFindMixer( USoundNode* Node, TArray<class USoundNodeMixer*> &OutNodes );
	void RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes );

#if WITH_EDITOR
	/** 
	 * Makes sure ogg vorbis data is available for all sound nodes in this cue by converting on demand
	 */
	UBOOL ValidateData( void );
#endif

	/** 
	 * Draw a sound cue in the sound cue editor
	 */
	void DrawCue( FCanvas* Canvas, TArray<USoundNode *>& SelectedNodes );