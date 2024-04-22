/**
 * LevelStreamingAlwaysLoaded
 *
 * @documentation
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LevelStreamingAlwaysLoaded extends LevelStreaming
	native;

/** Determines whether or not his Always loaded level is one that was auto created for a Procedural Building LOD level. **/
var() bool bIsProceduralBuildingLODLevel;

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
	* @return TRUE
	*/
	virtual UBOOL ShouldBeLoaded( const FVector& ViewLocation );
}