class InterpTrackInstFloatProp extends InterpTrackInst
	native(Interpolation);

/** 
 * InterpTrackInstFloatProp
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

cpptext
{
	virtual void SaveActorState();
	virtual void RestoreActorState();

	virtual void InitTrackInst(UInterpTrack* Track);
}

/** Pointer to float property in TrackObject. */
var	pointer		FloatProp; 

/** Saved value for restoring state when exiting Matinee. */
var	float		ResetFloat;
