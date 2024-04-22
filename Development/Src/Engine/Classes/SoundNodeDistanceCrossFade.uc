/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * SoundNodeDistanceCrossFade
 * 
 * This node's purpose is to play different sounds based on the distance to the listener.  
 * The node mixes between the N different sounds which are valid for the distance.  One should
 * think of a SoundNodeDistanceCrossFade as Mixer node which determines the set of nodes to
 * "mix in" based on their distance to the sound.
 * 
 * Example:
 * You have a gun that plays a fire sound.  At long distances you want a different sound than
 * if you were up close.   So you use a SoundNodeDistanceCrossFade which will calculate the distance
 * a listener is from the sound and play either:  short distance, long distance, mix of short and long sounds.
 *
 * A SoundNodeDistanceCrossFade differs from an SoundNodeAttenuation in that any sound is only going
 * be played if it is within the MinRadius and MaxRadius.  So if you want the short distance sound to be 
 * heard by people close to it, the MinRadius should probably be 0
 *
 * The volume curve for a SoundNodeDistanceCrossFade will look like this:
 *
 *                          Volume (of the input) 
 *    FadeInDistance.Max --> _________________ <-- FadeOutDistance.Min
 *                          /                 \
 *                         /                   \
 *                        /                     \
 * FadeInDistance.Min -->/                       \ <-- FadeOutDistance.Max
 */

class SoundNodeDistanceCrossFade extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

struct native DistanceDatum
{
	/** 
	 * The FadeInDistance at which to start hearing this sound.  If you want to hear the sound 
	 * up close then setting this to 0 might be a good option.
	 */
	var()					float					FadeInDistanceStart<ToolTip=The distance at which this sound starts fading in>;
	var()					float					FadeInDistanceEnd<ToolTip=The distance at which this sound has faded in completely>;

	/**
	 * The FadeOutDistance is where hearing this sound will end.
	 */
	var()					float					FadeOutDistanceStart<ToolTip=The distance at which this sound starts fading out>;
	var()					float					FadeOutDistanceEnd<ToolTip=The distance at which this sound is no longer audible>;

	/** The volume for which this Input should be played **/
	var()					float					Volume;

	var			deprecated	rawdistributionfloat	FadeInDistance;
	var			deprecated	rawdistributionfloat	FadeOutDistance;

	structdefaultproperties
	{
		Volume=1.0f
	}
};

/**
 * Each input needs to have the correct data filled in so the SoundNodeDistanceCrossFade is able
 * to determine which sounds to play
 */
var() export editfixedsize array<DistanceDatum>	CrossFadeInput;

defaultproperties
{
}
