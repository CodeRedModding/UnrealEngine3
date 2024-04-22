/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SoundMode extends Object
	native( AudioDevice )
	dontsortcategories( SoundMode )
	dependson( AudioDevice, SoundClass )
	hidecategories( object );

struct native AudioEQEffect
{
	/** Start time of effect */
	var	native transient	double	RootTime;

	/** High frequency filter cutoff frequency (Hz) */
	var( HighPass )			float	HFFrequency<ToolTip=High pass cutoff frequency.>;
	/** High frequency gain */
	var( HighPass )			float	HFGain<ToolTip=0.0 is silent, 1.0 is full volume.>;

	/** Middle frequency filter cutoff frequency (Hz) */
	var( BandPass )			float	MFCutoffFrequency<ToolTip=Band pass cutoff frequency.>;
	/** Middle frequency filter bandwidth frequency (Hz) */
	var( BandPass )			float	MFBandwidth<ToolTip=Band pass bandwidth (0.1 to 2.0).>;
	/** Middle frequency filter gain */
	var( BandPass )			float	MFGain<ToolTip=0.0 is silent, 1.0 is full volume.>;

	/** Low frequency filter cutoff frequency (Hz) */
	var( LowPass )			float	LFFrequency<ToolTip=Low pass cutoff frequency.>;			
	/** Low frequency filter gain */
	var( LowPass )			float	LFGain<ToolTip=0.0 is silent, 1.0 is full volume.>;

	structcpptext
	{
		// Cannot use strcutdefaultproperties here as this class is a member of a native class
		FAudioEQEffect( void ) :
			RootTime( 0.0 ),
			HFFrequency( DEFAULT_HIGH_FREQUENCY ),
			HFGain( 1.0f ),
			MFCutoffFrequency( DEFAULT_MID_FREQUENCY ),
			MFBandwidth( 1.0f ),
			MFGain( 1.0f ),
			LFFrequency( DEFAULT_LOW_FREQUENCY ),
			LFGain( 1.0f )
		{
		}

		/** 
		 * Interpolate EQ settings based on time
		 */
		void Interpolate( FLOAT InterpValue, const FAudioEQEffect& Start, const FAudioEQEffect& End );
		
		/** 
		 * Validate all settings are in range
		 */
		void ClampValues( void );
	}
};

/**
 * Elements of data for sound group volume control
 */
struct native SoundClassAdjuster
{
	var()	transient	ESoundClassName	SoundClassName<Tooltip=The sound class this adjuster affects.>;
	var()	editconst	name			SoundClass<Tooltip=(Debug: Should be the same as SoundClassName).>;
	var()				float			VolumeAdjuster<Tooltip=A multiplier applied to the volume.>;
	var()				float			PitchAdjuster<Tooltip=A multiplier applied to the pitch.>;
	var()				bool			bApplyToChildren<Tooltip=Check to apply this adjuster to all children of the sound class.>;
	var()				float			VoiceCenterChannelVolumeAdjuster<Tooltip=A multiplier applied to VoiceCenterChannelVolume.>;

	structdefaultproperties
	{
		SoundClassName="Master"
		SoundClass=Master
		VolumeAdjuster=1
		PitchAdjuster=1
		VoiceCenterChannelVolumeAdjuster=1
		bApplyToChildren=false;
	}
};

/** Whether to apply the EQ effect */
var( EQ )			bool							bApplyEQ<ToolTip=Whether to apply an EQ effect.>;
var( EQ )			AudioEQEffect					EQSettings;

/** Array of changes to be applied to groups */
var( SoundClasses )	array<SoundClassAdjuster>		SoundClassEffects;

var()				float							InitialDelay<ToolTip=Initial delay in seconds before the the mode is applied.>;
var()				float							FadeInTime<ToolTip=Time taken in seconds for the mode to fade in.>;
var()				float							Duration<ToolTip=Duration of mode, negative means it will be applied until another mode is set.>;
var()				float							FadeOutTime<ToolTip=Time taken in seconds for the mode to fade out.>;

defaultproperties
{
	bApplyEQ=FALSE
	InitialDelay=0.0
	Duration=-1.0
	FadeInTime=0.2
	FadeOutTime=0.2
}
