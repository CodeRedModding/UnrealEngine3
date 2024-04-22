/*=============================================================================
	UnInterpolationHitProxy.h
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_INTERPOLATIONHITPROXY
#define _INC_INTERPOLATIONHITPROXY

/** Input interface hit proxy */
struct HInterpEdInputInterface : public HHitProxy
{
	FInterpEdInputInterface* ClickedObject;
	FInterpEdInputData InputData;

	DECLARE_HIT_PROXY(HInterpEdInputInterface,HHitProxy);
	HInterpEdInputInterface(FInterpEdInputInterface* InObject, const FInterpEdInputData &InData): HHitProxy(HPP_UI), ClickedObject(InObject), InputData(InData) {}

	/** @return Returns a mouse cursor from the input interface. */
	virtual EMouseCursor GetMouseCursor()
	{
		return ClickedObject->GetMouseCursor(InputData);
	}
};

struct HInterpTrackKeypointProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpTrackKeypointProxy,HHitProxy);

	/** Tracks group */
	class UInterpGroup*		Group;
	/** Track which had a keyframe hit */
	class UInterpTrack*		Track;
	/** Index of hit keyframe */
	INT						KeyIndex;

	HInterpTrackKeypointProxy(class UInterpGroup* InGroup, class UInterpTrack* InTrack, INT InKeyIndex):
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack),
		KeyIndex(InKeyIndex)
	{}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_Cross;
	}
};

/** Hit proxy for keyframes drawn directly subgroups and not tracks. */
struct HInterpTrackSubGroupKeypointProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpTrackSubGroupKeypointProxy,HHitProxy);
	
	/* Parent track of the sub group */
	class UInterpTrack*		Track;
	/* Time of the key that was hit */
	FLOAT					KeyTime;
	/* SubGroup index */
	INT						GroupIndex;

	HInterpTrackSubGroupKeypointProxy(class UInterpTrack* InTrack, FLOAT InKeyTime, INT InGroupIndex ):
	HHitProxy(HPP_UI),
		Track(InTrack),
		KeyTime(InKeyTime),
		GroupIndex(InGroupIndex)
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
		HHitProxy(HPP_UI),
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



#endif
