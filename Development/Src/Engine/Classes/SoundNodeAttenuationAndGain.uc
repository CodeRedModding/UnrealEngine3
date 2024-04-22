/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
/** 
 * Defines how a sounds changes volume with distance to the listener.
 * Allows to more advance distance-volume function modelling.
 * https://udn.epicgames.com/Three/UsingSoundActors
 */ 
 
class SoundNodeAttenuationAndGain extends SoundNode
	DependsOn(SoundNodeAttenuation)
	native( Sound )
	hidecategories( Object )
	editinlinenew;

/* The settings for attenuating. */
var( AttenuationAndGain )		bool					bAttenuate<ToolTip=Enable attenuation via volume>;
var( AttenuationAndGain )		bool					bSpatialize<ToolTip=Enable the source to be positioned in 3D>;
var( AttenuationAndGain )		float					dBAttenuationAtMax<ToolTip=The volume at maximum distance in deciBels>;
var( AttenuationAndGain )		float					OmniRadius<ToolTip=At what distance to start blending the sound from as omnidirectional>;

/** What kind of attenuation model to use */
var( AttenuationAndGain )		SoundDistanceModel		GainDistanceAlgorithm<ToolTip=The type of volume versus distance algorithm to use>;
var( AttenuationAndGain )		SoundDistanceModel		AttenuateDistanceAlgorithm<ToolTip=The type of volume versus distance algorithm to use>;

/** How to calculate the distance from the sound to the listener */
var( AttenuationAndGain )		ESoundDistanceCalc		DistanceType<ToolTip=Special attenuation modes>;

var( AttenuationAndGain )		float					MinimalVolume<ToolTip=Volume level at distance between 0 and RadiusMin>;
var( AttenuationAndGain )		float					RadiusMin<ToolTip=The range at which the sound starts gaining>;
var( AttenuationAndGain )		float					RadiusPeak<ToolTip=The range at which the sound starts attenuating. RadiusPeak must be greater than RadiusMin and lesser than RadiusMax>;
var( AttenuationAndGain )		float					RadiusMax<ToolTip=The range at which the sound has attenuated completely>;

/* The settings for attenuating with a low pass filter. */
var( LowPassFilter )	bool					bAttenuateWithLPF<ToolTip=Enable attenuation via low pass filter>;
var( LowPassFilter )	float					LPFMinimal<ToolTip=LPF level at distance between 0 and RadiusMin>;
var( LowPassFilter )	float					LPFRadiusMin<ToolTip=The range at which to start rising a low passfilter>;
var( LowPassFilter )	float					LPFRadiusPeak<ToolTip=The range at which to start attenuating a low passfilter>;
var( LowPassFilter )	float					LPFRadiusMax<ToolTip=The range at which to apply the maximum amount of low pass filter>;

defaultproperties
{
	bAttenuate=true
	bSpatialize=true
	dBAttenuationAtMax=-60;

	GainDistanceAlgorithm=ATTENUATION_Linear
	AttenuateDistanceAlgorithm=ATTENUATION_Linear
	DistanceType=SOUNDDISTANCE_Normal

	MinimalVolume=0.75
	RadiusMin=400
	RadiusPeak=2000
	RadiusMax=4000
	
	bAttenuateWithLPF=false
	LPFMinimal=0.75
	LPFRadiusMin=400
	LPFRadiusPeak=2000
	LPFRadiusMax=4000
}
