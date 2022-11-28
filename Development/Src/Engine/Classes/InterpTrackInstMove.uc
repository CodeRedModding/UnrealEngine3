class InterpTrackInstMove extends InterpTrackInst
	native(Interpolation);

/** 
 * InterpTrackInstMove
 *
 * Created by:	James Golding
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

cpptext
{
	virtual void SaveActorState();
	virtual void RestoreActorState();

	/** Will save the current position of the Actor as the 'initial position', used if MoveFrame == IMF_RelativeToInitial. */
	virtual void InitTrackInst(UInterpTrack* Track);
}

/** Saved position. Used in editor for resetting when quitting Matinee. */
var	vector	ResetLocation;

/** Saved rotation. Used in editor for resetting when quitting Matinee. */
var rotator ResetRotation;

/** Transform of group's actor when sequence was started. This is used to reset sequence and also as basis when using IMF_RelativeToInitial. */
var	matrix	InitialTM;

/** Orientation of group's actor when sequence was started. @see InitialTM */
var quat	InitialQuat;

