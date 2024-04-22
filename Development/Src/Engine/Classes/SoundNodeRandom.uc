/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Selects sounds from a random set
 */
  
class SoundNodeRandom extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

var()		editfixedsize	array<float>		Weights;

/** If greater than 0, then upon each level load such a number of inputs will be randomly selected
 *  and the rest will be removed. This can be used to cut down the memory usage of large randomizing
 *  cues.
 */
var()						int					PreselectAtLevelLoad;

/** 
 * Determines whether or not this SoundNodeRandom should randomize with or without
 * replacement.  
 *
 * WithoutReplacement means that only nodes left will be valid for 
 * selection.  So with that, you are guarenteed to have only one occurrence of the
 * sound played until all of the other sounds in the set have all been played.
 *
 * WithReplacement means that a node will be chosen and then placed back into the set.
 * So one could play the same sound over and over if the probabilities don't go your way :-)
 */
var() 						bool				bRandomizeWithoutReplacement;

/**
 * Internal state of which sounds have been played.  This is only used at runtime
 * to keep track of which sounds have been played
 */
var 		transient		array<bool>			HasBeenUsed;

/** Counter var so we don't have to count all of the used sounds each time we choose a sound **/
var 		transient		int					NumRandomUsed;

defaultproperties
{
	bRandomizeWithoutReplacement=TRUE
	NumRandomUsed=0
}
