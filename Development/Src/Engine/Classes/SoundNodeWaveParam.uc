/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/** 
 * Sound node that takes a runtime parameter for the wave to play
 */
 
class SoundNodeWaveParam extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;

/** The name of the wave parameter to use to look up the SoundNodeWave we should play */
var()	name				WaveParameterName;

