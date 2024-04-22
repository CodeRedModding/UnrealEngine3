/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines how a sounds changes volume with distance to the listener
 */ 
 
class SoundNodeAttenuation extends SoundNode
	native( Sound )
	hidecategories( Object )
	dontsortcategories( Attenuation, LowPassFilter )
	editinlinenew;

enum SoundDistanceModel
{
	ATTENUATION_Linear,
	ATTENUATION_Logarithmic,
	ATTENUATION_Inverse,
	ATTENUATION_LogReverse,
	ATTENUATION_NaturalSound
};

enum ESoundDistanceCalc
{
	SOUNDDISTANCE_Normal,
	SOUNDDISTANCE_InfiniteXYPlane,
	SOUNDDISTANCE_InfiniteXZPlane,
	SOUNDDISTANCE_InfiniteYZPlane,
};

/* The settings for attenuating. */
var( Attenuation )		bool					bAttenuate<ToolTip=Enable attenuation via volume>;
var( Attenuation )		bool					bSpatialize<ToolTip=Enable the source to be positioned in 3D>;
var( Attenuation )		float					dBAttenuationAtMax<ToolTip=The volume at maximum distance in deciBels>;
var( Attenuation )		float					OmniRadius<ToolTip=At what distance to start blending the sound from as omnidirectional>;

/** What kind of attenuation model to use */
var( Attenuation )		SoundDistanceModel		DistanceAlgorithm<ToolTip=The type of volume versus distance algorithm to use>;

/** How to calculate the distance from the sound to the listener */
var( Attenuation )		ESoundDistanceCalc		DistanceType<ToolTip=Special attenuation modes>;

var( Attenuation )		float					RadiusMin<ToolTip=The range at which the sound starts attenuating>;
var( Attenuation )		float					RadiusMax<ToolTip=The range at which the sound has attenuated completely>;

/* The settings for attenuating with a low pass filter. */
var( LowPassFilter )	bool					bAttenuateWithLPF<ToolTip=Enable attenuation via low pass filter>;
var( LowPassFilter )	float					LPFRadiusMin<ToolTip=The range at which to start applying a low passfilter>;
var( LowPassFilter )	float					LPFRadiusMax<ToolTip=The range at which to apply the maximum amount of low pass filter>;

defaultproperties
{
	bAttenuate=true
	bSpatialize=true
	dBAttenuationAtMax=-60;
	DistanceAlgorithm=ATTENUATION_Linear
	RadiusMin=400
	RadiusMax=4000
	
	LPFRadiusMin=3000
	LPFRadiusMax=6000
}
