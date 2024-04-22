/**
 * LevelStreamingPersistent
 *
 * @documentation
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LevelStreamingPersistent extends LevelStreaming
	transient
	native;

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
	virtual UBOOL ShouldBeLoaded( const FVector& ViewLocation )
	{
		return TRUE;
	}

	/**
	 * Returns whether this level should be visible/ associated with the world if it is
	 * loaded.
	 * 
	 * @param ViewLocation	Location of the viewer
	 * @return TRUE
	 */
	virtual UBOOL ShouldBeVisible( const FVector& ViewLocation )
	{
		return TRUE;
	}
}
