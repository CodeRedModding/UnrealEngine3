/*=============================================================================
	AmbientSoundSimpleToggleable.h: Native AmbientSoundSimpleToggleable calls
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

protected:

	/**
	 * Check whether the auto-play setting should be ignored, even if enabled. If a sound has been
	 * explicitly toggled off via Kismet action, autoplay should be ignored from that point on.
	 *
	 * @return TRUE if the auto-play setting should be ignored, FALSE otherwise
	 */
	virtual UBOOL ShouldIgnoreAutoPlay() const;

