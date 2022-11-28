/*=============================================================================
	UnSkeletalAnim.h: Unreal skeletal animation related classes
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Raw keyframe data for one track.
struct FRawAnimSequenceTrack
{
	// Each array will either contain NumKey elements or 1 element. If 1 element, its assumed to be constant over enture sequence.

	TArray<FVector>		PosKeys;
	TArray<FQuat>		RotKeys;
	TArray<FLOAT>		KeyTimes; // Key times are in seconds.

	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& T)
	{	
		return	Ar << T.PosKeys << T.RotKeys << T.KeyTimes;
	}
};