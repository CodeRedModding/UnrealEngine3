/*=============================================================================
	AmbientSound.h: Native AmbientSound calls
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

protected:
	// AActor interface.
	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	/**
	 * Starts audio playback if wanted.
	 */
	virtual void UpdateComponentsInternal( UBOOL bCollisionUpdate = FALSE );

	/**
	 * Check whether the auto-play setting should be ignored, even if enabled. By default,
	 * this returns TRUE if the Ambient Sound's level is hidden in the editor, though subclasses may implement custom behavior.
	 *
	 * @return TRUE if the auto-play setting should be ignored, FALSE otherwise
	 */
	virtual UBOOL ShouldIgnoreAutoPlay() const;


