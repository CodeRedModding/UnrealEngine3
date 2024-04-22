/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackLinearColorPropHelper extends InterpTrackVectorPropHelper
	native;

cpptext
{
	/**
	 * Checks track-dependent criteria prior to adding a new track.
	 * Responsible for any message-boxes or dialogs for selecting track-specific parameters.
	 * Called on default object.
	 *
	 * @param Group The group that this track is being added to
	 * @param	Trackdef Pointer to default object for this UInterpTrackClass.
	 * @param	bDuplicatingTrack Whether we are duplicating this track or creating a new one from scratch.
	 * @param bAllowPrompts When TRUE, we'll prompt for more information from the user with a dialog box if we need to
	 * @return Returns true if this track can be created and false if some criteria is not met (i.e. A named property is already controlled for this group).
	 */
	virtual	UBOOL PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const;

	/**
	 * Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
	 *
	 * @param Track				Pointer to the track that was just created.
	 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
	 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
	 */
	virtual void  PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const;
}
