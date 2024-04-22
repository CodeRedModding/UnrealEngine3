/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/**
 * This SoundNode uses UEngine::bAllowMatureLanguage to determine whether child nodes
 * that have USoundNodeWave::bMature=TRUE should be played. 
 */
 
class SoundNodeMature extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

defaultproperties
{
}
