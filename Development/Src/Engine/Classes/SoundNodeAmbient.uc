/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines the parameters for an in world looping ambient sound e.g. a wind sound
 */
 
class SoundNodeAmbient extends SoundNode
	native( Sound )
	hidecategories( Object )
	AutoExpandCategories( Attenuation, LowPassFilter, Modulation, Sounds, Spatialization )
	DontSortCategories( Attenuation, LowPassFilter, Modulation, Sounds, Spatialization )
	dependson( SoundNodeAttenuation )
	editinlinenew;

struct native AmbientSoundSlot
{
	var()	SoundNodeWave	Wave;
	var()	float			PitchScale;
	var()	float			VolumeScale;
	var()	float			Weight;

	structdefaultproperties
	{
		PitchScale=1.0
		VolumeScale=1.0
		Weight=1.0
	}
	
	structcpptext
	{
		FAmbientSoundSlot( void )
		{
			PitchScale = 1.0f;
			VolumeScale = 1.0f;
			Weight = 1.0f;
		}
	}
};

/* The settings for attenuating. */
var( Attenuation )		bool					bAttenuate<ToolTip=Enable attenuation via volume>;
var( Attenuation )		bool					bSpatialize<ToolTip=Enable the source to be positioned in 3D>;
var( Attenuation )		float					dBAttenuationAtMax<ToolTip=The volume at maximum distance in deciBels>;

/** What kind of attenuation model to use */
var( Attenuation )		SoundDistanceModel		DistanceModel<ToolTip=The type of volume versus distance algorithm to use>;

var( Attenuation )		float					RadiusMin<ToolTip=The range at which the sound starts attenuating>;
var( Attenuation )		float					RadiusMax<ToolTip=The range at which the sound has attenuated completely>;

/* The settings for attenuating with a low pass filter. */
var( LowPassFilter )	bool					bAttenuateWithLPF<ToolTip=Enable attenuation via low pass filter>;
var( LowPassFilter )	float					LPFRadiusMin<ToolTip=The range at which to start applying a low passfilter>;
var( LowPassFilter )	float					LPFRadiusMax<ToolTip=The range at which to apply the maximum amount of low pass filter>;

var( Modulation )		float					PitchMin<ToolTip=The lower bound of pitch (1.0 is no change)>;
var( Modulation )		float					PitchMax<ToolTip=The upper bound of pitch (1.0 is no change)>;

var( Modulation )		float					VolumeMin<ToolTip=The lower bound of volume (1.0 is no change)>;
var( Modulation )		float					VolumeMax<ToolTip=The upper bound of volume (1.0 is no change)>;

var( Sounds )			array<AmbientSoundSlot>	SoundSlots<ToolTip=Sounds to play>;

defaultproperties
{
	bAttenuate=true
	bSpatialize=true
	dBAttenuationAtMax=-60
	RadiusMin=2000
	RadiusMax=5000
	DistanceModel=ATTENUATION_Linear
	
	LPFRadiusMin=3500
	LPFRadiusMax=7000

	VolumeMin=0.7
	VolumeMax=0.7
	PitchMin=1.0
	PitchMax=1.0
}

