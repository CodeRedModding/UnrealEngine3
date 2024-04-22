/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 *  Audio component used by AmbientSoundSpline. It finds position of sound source (virtual speaker) based on poredefined points list, and listener's position.
 */

class SplineAudioComponent extends AudioComponent
	native
	collapsecategories
	hidecategories(Object,ActorComponent)
	editinlinenew;

struct InterpPointOnSpline
{
	var() vector Position;
	var() float InVal;
	var() float Length;
};

/** The scope of listener. How far points are included while virtual speaker eveluation. */
var(SplineAudioComponent)   float ListenerScopeRadius;

/** While virtual speaker position evaluation, the clolest point index is stored in this field. It is used to Calculate the distance, needed to eval the attenuation. */
var                         int ClosestPointOnSplineIndex;


/** Points used to find virtual speaker's position. They are placed on spline automatically, while spline editing. */
var init                    array< InterpPointOnSpline > Points;

cpptext
{
	/** 
	 * @param InListeners all listeners list
	 * @param ClosestListenerIndexOut through this variable index of the closest listener is returned 
	 * @return Closest RELATIVE location of sound (relative to position of the closest listener). 
	 */
	virtual FVector FindClosestLocation( const TArray<struct FListener>& InListeners, INT& ClosestListenerIndexOut );

	/**
	 * @return  point, that should be used for evaluation distance, between listener, and sound source. That distance is used for attenuation.
	 * The function is needed when the speaker sound's position is estimated from a shape (AmbientSoundSpline)
	 */
	virtual FVector GetPointForDistanceEval();

	/**
	 * The function generates Points.
	 */
	UBOOL SetSplineData(const FInterpCurveVector& SplineData, FLOAT DistanceBetweenPoints);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

public:
	/**
	 *  Math helper function
	 *  @param Points - parray of points
	 *  @param Listener - position of listener
	 *  @param Radius - scope of listener
	 *  @param ClosestPointIndex - out - index of the closest point, if none point is inside the listener's scope -1 is returned
	 *  @return mean virtual speaker position, with respect to distance from listener 
	 */ 
	static FVector FindVirtualSpeakerPosition(const TArray< FInterpPointOnSpline >& Points, FVector Listener, FLOAT Radius, INT * ClosestPointIndex = NULL);
}
defaultproperties
{
	ListenerScopeRadius = 1200;	
}
