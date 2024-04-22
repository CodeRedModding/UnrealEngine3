/*=============================================================================
	AmbientSoundNonLoop.h: Native AmbientSoundNonLoop calls
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

