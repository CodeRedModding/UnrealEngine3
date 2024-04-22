/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class InterpTrackVectorPropHelper extends InterpTrackHelper
	native;

cpptext
{
	/**
	 * Pops up a dialog letting user choose between a set of properties, then checks to see if that property has been bound to yet.
	 *
	 * @param PropNames		Possible property names to select from.
	 *
	 * @return TRUE if the property selected was acceptable, FALSE otherwise.
	 */
	virtual UBOOL ChooseProperty(TArray<FName> &PropNames) const;

	/** Checks track-dependent criteria prior to adding a new track.
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

	/** Uses the track-specific data object from PreCreateTrack to initialize the newly added Track.
	 * @param Track				Pointer to the track that was just created.
	 * @param bDuplicatingTrack	Whether we are duplicating this track or creating a new one from scratch.
	 * @param TrackIndex			The index of the Track that as just added.  This is the index returned by InterpTracks.AddItem.
	 */
	virtual void  PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const;
}
