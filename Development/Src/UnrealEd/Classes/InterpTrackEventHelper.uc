/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class InterpTrackEventHelper extends InterpTrackHelper
	native;

cpptext
{
	/** Checks track-dependent criteria prior to adding a new keyframe.
	* Responsible for any message-boxes or dialogs for selecting key-specific parameters.
	* Optionally creates/references a key-specific data object to be used in PostCreateKeyframe.
	*
	* @param Track		Pointer to the currently selected track.
	* @param KeyTime	The time that this Key becomes active.
	* @return	Returns true if this key can be created and false if some criteria is not met (i.e. No related item selected in browser).
	*/
	virtual	UBOOL PreCreateKeyframe( UInterpTrack *Track, FLOAT KeyTime ) const;

	/** Uses the key-specific data object from PreCreateKeyframe to initialize the newly added key.
	*
	* @param Track		Pointer to the currently selected track.
	* @param KeyIndex	The index of the keyframe that as just added.  This is the index returned by AddKeyframe.
	*/
	virtual void  PostCreateKeyframe( UInterpTrack *Track, INT KeyIndex ) const;
}
