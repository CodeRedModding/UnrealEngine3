/*=============================================================================
	UnInterpolationHitProxy.h
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding  
=============================================================================*/

struct HInterpTrackKeypointProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpTrackKeypointProxy,HHitProxy);

	class UInterpGroup*		Group;
	INT						TrackIndex;
	INT						KeyIndex;

	HInterpTrackKeypointProxy(class UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex):
		Group(InGroup),
		TrackIndex(InTrackIndex),
		KeyIndex(InKeyIndex)
	{}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

struct HInterpTrackKeyHandleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpTrackKeyHandleProxy,HHitProxy);

	class UInterpGroup*		Group;
	INT						TrackIndex;
	INT						KeyIndex;
	UBOOL					bArriving;

	HInterpTrackKeyHandleProxy(class UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex, UBOOL bInArriving):
		Group(InGroup),
		TrackIndex(InTrackIndex),
		KeyIndex(InKeyIndex),
		bArriving(bInArriving)
	{}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};


class FInterpEdSelKey
{
public:
	FInterpEdSelKey()
	{
		Group = NULL;
		TrackIndex = INDEX_NONE;
		KeyIndex = INDEX_NONE;
		UnsnappedPosition = 0.f;
	}

	FInterpEdSelKey(class UInterpGroup* InGroup, INT InTrack, INT InKey)
	{
		Group = InGroup;
		TrackIndex = InTrack;
		KeyIndex = InKey;
	}

	UBOOL operator==(const FInterpEdSelKey& Other) const
	{
		if(	Group == Other.Group &&
			TrackIndex == Other.TrackIndex &&
			KeyIndex == Other.KeyIndex )
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	class UInterpGroup* Group;
	INT TrackIndex;
	INT KeyIndex;
	FLOAT UnsnappedPosition;
};