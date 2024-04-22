/*=============================================================================
	CodeAudioEffects.cpp: Unreal CoreAudio audio effects interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "Engine.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioDecompress.h"
#include "UnAudioEffect.h"
#include "CoreAudioDevice.h"
#include "CoreAudioEffects.h"

extern FCoreAudioSoundSource *GAudioChannels[MAX_AUDIOCHANNELS + 1];

extern UBOOL MacLoadRadioEffectComponent();

/*------------------------------------------------------------------------------------
	FCoreAudioEffectsManager.
------------------------------------------------------------------------------------*/

/**
 * Init all sound effect related code
 */
FCoreAudioEffectsManager::FCoreAudioEffectsManager( UAudioDevice* InDevice )
	: FAudioEffectsManager( InDevice )
{
#if RADIO_ENABLED
	bRadioAvailable = MacLoadRadioEffectComponent();
#endif

	Radio_ChebyshevPowerMultiplier = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevPowerMultiplier" ), 2.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
	Radio_ChebyshevPower = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevPower" ), 5.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
	Radio_ChebyshevMultiplier = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevMultiplier" ), 3.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
	Radio_ChebyshevCubedMultiplier = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevCubedMultiplier" ), 5.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
}

FCoreAudioEffectsManager::~FCoreAudioEffectsManager()
{
}

/** 
 * Calls the platform specific code to set the parameters that define reverb
 */
void FCoreAudioEffectsManager::SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters )
{
	FLOAT DryWetMix = ReverbEffectParameters.Gain * 100.0f;		// 0.0-100.0, 100.0
	FLOAT SmallLargeMix = ReverbEffectParameters.GainHF * 100.0f;	// 0.0-100.0, 50.0
	FLOAT PreDelay = 0.025f;									// 0.001->0.03, 0.025
	FLOAT ModulationRate = 1.0f;								// 0.001->2.0, 1.0
	FLOAT ModulationDepth = 0.2f;								// 0.0 -> 1.0, 0.2
	FLOAT FilterGain = Clamp<FLOAT>( VolumeToDeciBels( ReverbEffectParameters.Volume ), -18.0f, 18.0f );	// -18.0 -> +18.0, 0.0
	
	FLOAT LargeDecay = Clamp<FLOAT>( ( ReverbEffectParameters.DecayTime - 1.0f ) * 0.25f, 0.0f, 1.0f );
	FLOAT SmallDecay = Clamp<FLOAT>( LargeDecay * ReverbEffectParameters.DecayHFRatio * 0.5f, 0.0f, 1.0f );
	
	FLOAT SmallSize = Max<FLOAT>( SmallDecay * 0.05f, 0.001f );			// 0.0001->0.05, 0.0048
	FLOAT SmallDensity = ReverbEffectParameters.ReflectionsGain;		// 0->1, 0.28
	FLOAT SmallBrightness = Max<FLOAT>(ReverbEffectParameters.Diffusion, 0.1f);	// 0.1->1, 0.96
	FLOAT SmallDelayRange = ReverbEffectParameters.ReflectionsDelay;	// 0->1 0.5
	
	FLOAT LargeSize = Max<FLOAT>( LargeDecay * 0.15f, 0.005f );			// 0.005->0.15, 0.04
	FLOAT LargeDelay = Max<FLOAT>( ReverbEffectParameters.LateDelay, 0.001 );	// 0.001->0.1, 0.035
	FLOAT LargeDensity = ReverbEffectParameters.LateGain;		// 0->1, 0.82
	FLOAT LargeDelayRange = 0.3f;								// 0->1, 0.3
	FLOAT LargeBrightness = Max<FLOAT>(ReverbEffectParameters.Density, 0.1f);	// 0.1->1, 0.49

	for( DWORD Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		FCoreAudioSoundSource *Source = GAudioChannels[Index];
		if( Source && Source->ReverbUnit )
		{
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_DryWetMix, kAudioUnitScope_Global, 0, DryWetMix, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallLargeMix, kAudioUnitScope_Global, 0, SmallLargeMix, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_PreDelay, kAudioUnitScope_Global, 0, PreDelay, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_ModulationRate, kAudioUnitScope_Global, 0, ModulationRate, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_ModulationDepth, kAudioUnitScope_Global, 0, ModulationDepth, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_FilterFrequency, kAudioUnitScope_Global, 0, DEFAULT_HIGH_FREQUENCY, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_FilterGain, kAudioUnitScope_Global, 0, FilterGain, 0);
			
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallSize, kAudioUnitScope_Global, 0, SmallSize, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallDensity, kAudioUnitScope_Global, 0, SmallDensity, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallBrightness, kAudioUnitScope_Global, 0, SmallBrightness, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallDelayRange, kAudioUnitScope_Global, 0, SmallDelayRange, 0);

			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeSize, kAudioUnitScope_Global, 0, LargeSize, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeDelay, kAudioUnitScope_Global, 0, LargeDelay, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeDensity, kAudioUnitScope_Global, 0, LargeDensity, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeDelayRange, kAudioUnitScope_Global, 0, LargeDelayRange, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeBrightness, kAudioUnitScope_Global, 0, LargeBrightness, 0);
		}
	}
}

/** 
 * Calls the platform specific code to set the parameters that define EQ
 */
void FCoreAudioEffectsManager::SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters )
{
	FLOAT LowGain = VolumeToDeciBels(EQEffectParameters.LFGain);
	FLOAT CenterGain = VolumeToDeciBels(EQEffectParameters.MFGain);
	FLOAT HighGain = VolumeToDeciBels(EQEffectParameters.HFGain);

	for( DWORD Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		FCoreAudioSoundSource *Source = GAudioChannels[Index];
		if( Source && Source->EQUnit )
		{
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_LowFrequency, kAudioUnitScope_Global, 0, EQEffectParameters.LFFrequency, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_LowGain, kAudioUnitScope_Global, 0, LowGain, 0 );

			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterFreq1, kAudioUnitScope_Global, 0, (EQEffectParameters.MFCutoffFrequency - EQEffectParameters.LFFrequency) / 2.0f, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterGain1, kAudioUnitScope_Global, 0, CenterGain, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_Bandwidth1, kAudioUnitScope_Global, 0, EQEffectParameters.MFBandwidth, 0 );
			
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterFreq2, kAudioUnitScope_Global, 0, EQEffectParameters.MFCutoffFrequency, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterGain2, kAudioUnitScope_Global, 0, CenterGain, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_Bandwidth2, kAudioUnitScope_Global, 0, EQEffectParameters.MFBandwidth, 0 );

			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterFreq3, kAudioUnitScope_Global, 0, (EQEffectParameters.HFFrequency - EQEffectParameters.MFCutoffFrequency) / 2.0f, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterGain3, kAudioUnitScope_Global, 0, CenterGain, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_Bandwidth3, kAudioUnitScope_Global, 0, EQEffectParameters.MFBandwidth, 0 );

			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_HighFrequency, kAudioUnitScope_Global, 0, EQEffectParameters.HFFrequency, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_HighGain, kAudioUnitScope_Global, 0, HighGain, 0 );
		}
	}
}

/** 
 * Calls the platform specific code to set the parameters that define a radio effect.
 * 
 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
 */
void FCoreAudioEffectsManager::SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters )
{
	enum ERadioEffectParams
	{
		RadioParam_ChebyshevPowerMultiplier,
		RadioParam_ChebyshevPower,
		RadioParam_ChebyshevMultiplier,
		RadioParam_ChebyshevCubedMultiplier
	};

	const FLOAT ChebyshevPowerMultiplier = Radio_ChebyshevPowerMultiplier->GetFloat();
	const FLOAT ChebyshevPower = Radio_ChebyshevPower->GetFloat();
	const FLOAT ChebyshevMultiplier = Radio_ChebyshevMultiplier->GetFloat();
	const FLOAT ChebyshevCubedMultiplier = Radio_ChebyshevCubedMultiplier->GetFloat();

	for( DWORD Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		FCoreAudioSoundSource *Source = GAudioChannels[Index];
		if( Source && Source->RadioUnit )
		{
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevPowerMultiplier, kAudioUnitScope_Global, 0, ChebyshevPowerMultiplier, 0 );
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevPower, kAudioUnitScope_Global, 0, ChebyshevPower, 0 );
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevMultiplier, kAudioUnitScope_Global, 0, ChebyshevMultiplier, 0 );
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevCubedMultiplier, kAudioUnitScope_Global, 0, ChebyshevCubedMultiplier, 0 );
		}
	}
}
