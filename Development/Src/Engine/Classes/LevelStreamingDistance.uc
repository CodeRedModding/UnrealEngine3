/**
 * LevelStreamingDistance
 *
 * Distance based streaming implementation.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LevelStreamingDistance extends LevelStreaming
	native;


/** Origin of level used for distance calculation to viewer				*/
var()	vector	Origin;
/** Maximum distance to viewer at which the level still is streamed in	*/
var()	float	MaxDistance;

cpptext
{
	/**
	 * Returns whether this level should be present in memory which in turn tells the 
	 * streaming code to stream it in. Please note that a change in value from FALSE 
	 * to TRUE only tells the streaming code that it needs to START streaming it in 
	 * so the code needs to return TRUE an appropriate amount of time before it is 
	 * needed.
	 *
	 * @param ViewLocation	Location of the viewer
	 * @return TRUE if level should be loaded/ streamed in, FALSE otherwise
	 */
	virtual UBOOL ShouldBeLoaded( const FVector& ViewLocation );
}
