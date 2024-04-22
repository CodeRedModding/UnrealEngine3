/*=============================================================================
	UnMiscDeclarations.h: Misc Unreal Engine class declarations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef _UNMISC_DECLARATIONS_H_
#define _UNMISC_DECLARATIONS_H_

/**
 * Raw keyframe data for one track.  Each array will contain either NumFrames elements or 1 element.
 * One element is used as a simple compression scheme where if all keys are the same, they'll be
 * reduced to 1 key that is constant over the entire sequence.
 *
 * @warning: manually mirrored in AnimSequence.uc due to dual native/ script serialization.
 */
struct FRawAnimSequenceTrack
{
	/** Position keys. */
	TArray<FVector> PosKeys;
	/** Rotation keys. */
	TArray<FQuat> RotKeys;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FRawAnimSequenceTrack& T )
	{
		T.PosKeys.BulkSerialize(Ar);
		T.RotKeys.BulkSerialize(Ar);

		// Support for legacy content.
		if( Ar.IsLoading() && Ar.Ver() < VER_RAW_ANIMDATA_REDUX )
		{
			TArray<FLOAT> KeyTimes;
			KeyTimes.BulkSerialize(Ar);
		}

		return Ar;
	}
};

#endif //_UNMISC_DECLARATIONS_H_

