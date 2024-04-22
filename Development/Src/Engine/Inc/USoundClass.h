/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

protected:

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);


	// UObject interface.
	virtual void Serialize( FArchive& Ar );

	/**
	 * Returns a description of this object that can be used as extra information in list views.
	 */
	virtual FString GetDesc( void );

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

public:
	/**
	 * Called when a property value from a member struct or array has been changed in the editor.
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** 
	 * Gets the parameters for the sound mode
	 */
	void Interpolate( FLOAT InterpValue, FSoundClassProperties* Current, FSoundClassProperties* Start, FSoundClassProperties* End );

	/** 
	 * Returns TRUE if the child sound class exists in the tree 
	 */
	UBOOL RecurseCheckChild( UAudioDevice* AudioDevice, FName ChildSoundClassName );

	/** 
	 * Validates a connection, returns TRUE if it is allowed
	 */
	UBOOL CheckValidConnection( UAudioDevice* AudioDevice, USoundClass* ParentSoundClass, USoundClass* ChildSoundClass );

	/**
	 * Sets up stub editor data for any sound class that doesn't have any
	 */
	void InitSoundClassEditor( UAudioDevice* AudioDevice );

	/** 
	 * Draws the sound class hierarchy in the editor
	 */
	void DrawSoundClasses( UAudioDevice* AudioDevice, FCanvas* Canvas, TArray<USoundClass*>& SelectedNodes );

	/** 
	 * Draws a sound class in the sound class editor
	 */
	void DrawSoundClass( FCanvas* Canvas, const FSoundClassEditorData& EdData, UBOOL bSelected );

	/** 
	 * Calculates where to draw the connectors for this class in the sound class editor window
	 */
	FIntPoint GetConnectionLocation( FCanvas* Canvas, INT ConnType, INT ConnIndex, const FSoundClassEditorData& EdData );

