/*=============================================================================
	XeAudioDevice.cpp: Unreal XAudio2 Audio interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "Engine.h"

#include <xapobase.h>
#include <xapofx.h>
#include <xaudio2fx.h>

#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioEffect.h"
#include "XAudio2Device.h"
#include "XAudio2Effects.h"

/*-----------------------------------------------------------------------------
	FBandPassFilter
-----------------------------------------------------------------------------*/
class FBandPassFilter
{
private:

	FLOAT Coefficient0;
	FLOAT Coefficient1;
	FLOAT Coefficient2;
	FLOAT Coefficient3;
	FLOAT Coefficient4;

	FLOAT Z0;
	FLOAT Z1;
	FLOAT Y0;
	FLOAT Y1;

	inline static FLOAT CalculateC( FLOAT BandwidthHz, FLOAT SampleRate )
	{
		const FLOAT Angle = PI * ( ( BandwidthHz * 0.5f ) / SampleRate );
		return appTan( Angle - 1.0f ) / appTan( 2.0f * Angle + 1.0f );
	}

	inline static FLOAT CalculateD( FLOAT CenterFrequencyHz, FLOAT SampleRate )
	{
		const FLOAT Angle = 2.0f * PI * CenterFrequencyHz / SampleRate;
		return -appCos( Angle );
	}

public:

	/**
	 * Constructor (default).
	 */
	FBandPassFilter( void )
		:	Coefficient0( 0.0f )
		,	Coefficient1( 0.0f )
		,	Coefficient2( 0.0f )
		,	Coefficient3( 0.0f )
		,	Coefficient4( 0.0f )
		,	Z0( 0.0f )
		,	Z1( 0.0f )
		,	Y0( 0.0f )
		,	Y1( 0.0f )
	{
	}

	inline void Initialize( FLOAT FrequencyHz, FLOAT BandwidthHz, FLOAT SampleRate )
	{
		const FLOAT C = CalculateC( BandwidthHz, SampleRate );
		const FLOAT D = CalculateD( FrequencyHz, SampleRate );

		const FLOAT A0 = 1.0f;
		const FLOAT A1 = D * ( 1.0f - C );
		const FLOAT A2 = -C;
		const FLOAT B0 = 1.0f + C;
		const FLOAT B1 = 0.0f;
		const FLOAT B2 = -B0;

		Coefficient0 = B0 / A0;

		Coefficient1 = +B1 / A0;
		Coefficient2 = +B2 / A0;
		Coefficient3 = -A1 / A0;
		Coefficient4 = -A2 / A0;

		Z0 = Z1 = Y0 = Y1 = 0.0f;
	}

	inline FLOAT Process( FLOAT Sample )
	{
		const FLOAT Y	= Coefficient0 * Sample
						+ Coefficient1 * Z0
						+ Coefficient2 * Z1
						+ Coefficient3 * Y0
						+ Coefficient4 * Y1;
		Z1 = Z0;
		Z0 = Sample;
		Y1 = Y0;
		Y0 = Y;

		return Y;
	}
};

/*-----------------------------------------------------------------------------
	Global utility classes for generating a radio distortion effect.
-----------------------------------------------------------------------------*/
static FBandPassFilter      GFinalBandPassFilter;

/*-----------------------------------------------------------------------------
	FXAudio2RadioEffect. Custom XAPO for radio distortion.
-----------------------------------------------------------------------------*/

#define RADIO_CLASS_ID __declspec( uuid( "{5EB8D611-FF96-429d-8365-2DDF89A7C1CD}" ) ) 

/**
 * Custom XAudio2 Audio Processing Object (XAPO) that distorts 
 * audio samples into having a radio effect applied to them.
 */
class RADIO_CLASS_ID FXAudio2RadioEffect : public CXAPOParametersBase
{
public:
	/**
	 * Constructor. 
	 *
	 * @param	InitialSampleRate	The sample rate to process the audio.
	 */
	FXAudio2RadioEffect( FLOAT InitialSampleRate )
		:	CXAPOParametersBase( &Registration, ( BYTE* )Parameters, sizeof( FAudioRadioEffect ), FALSE )
		,	SampleRate( InitialSampleRate )
	{
		Radio_ChebyshevPowerMultiplier = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevPowerMultiplier" ), 2.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
		Radio_ChebyshevPower = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevPower" ), 5.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
		Radio_ChebyshevMultiplier = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevMultiplier" ), 3.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );
		Radio_ChebyshevCubedMultiplier = GConsoleManager->RegisterConsoleVariable( TEXT( "Radio_ChebyshevCubedMultiplier" ), 5.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default );

		// Initialize the global audio processing helper classes
		GFinalBandPassFilter.Initialize( 2000.0f, 400.0f, SampleRate );

		// Setup default values for the parameters to initialize the rest of the global 
		// audio processing helper classes. See FXAudio2RadioEffect::OnSetParameters().
		FAudioRadioEffect DefaultParameters;
		SetParameters( &DefaultParameters, sizeof( DefaultParameters ) );
	}

	/**
	 * Copies the wave format of the audio for reference.
	 *
	 * @note	Called by XAudio2 to lock the input and output configurations of an XAPO allowing 
	 *			it to do any final initialization before Process is called on the real-time thread.
	 */
	STDMETHOD( LockForProcess )(	UINT32 InputLockedParameterCount,
									const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters,
									UINT32 OutputLockedParameterCount,
									const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters )
	{
		// Try to lock the process on the base class before attempting to do any initialization here. 
		HRESULT Result = CXAPOParametersBase::LockForProcess(	InputLockedParameterCount,
													  			pInputLockedParameters,
																OutputLockedParameterCount,
																pOutputLockedParameters );
		// Process couldn't be locked. ABORT! 
		if( FAILED( Result ) )
		{
			return Result;
		}

		// Store the wave format locally on this effect to use in the Process() function.
		appMemcpy( &WaveFormat, pInputLockedParameters[0].pFormat, sizeof( WAVEFORMATEX ) );
		return S_OK;
	}

	/**
	 * Adds a radio distortion to the input buffer if the radio effect is enabled. 
	 *
	 * @param	InputProcessParameterCount	Number of elements in pInputProcessParameters.
	 * @param	pInputProcessParameters		Input buffer containing audio samples. 
	 * @param	OutputProcessParameterCount	Number of elements in pOutputProcessParameters. 
	 * @param	pOutputProcessParameters	Output buffer, which is not touched by this xAPO. 
	 * @param	IsEnabled					TRUE if this effect is enabled; FALSE, otherwise. 
	 */
	STDMETHOD_( void, Process )(	UINT32 InputProcessParameterCount,
									const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters,
									UINT32 OutputProcessParameterCount,
									XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters,
									BOOL IsEnabled )
	{
		// Verify several condition based on the registration 
		// properties we used to create the class. 
		check( IsLocked() );
		check( InputProcessParameterCount == 1 );
		check( OutputProcessParameterCount == 1 );
		check( pInputProcessParameters[0].pBuffer == pOutputProcessParameters[0].pBuffer );

		// Check the global volume multiplier because this effect 
		// will continue to play if the editor loses focus.
		if( IsEnabled && GVolumeMultiplier != 0.0f )
		{
			FAudioRadioEffect* RadioParameters = ( FAudioRadioEffect* )BeginProcess();
			check( RadioParameters );

			// The total sample size must account for multiple channels. 
			const UINT SampleSize = pInputProcessParameters[0].ValidFrameCount * WaveFormat.nChannels;

			const FLOAT ChebyshevPowerMultiplier = Radio_ChebyshevPowerMultiplier->GetFloat();
			const FLOAT ChebyshevPower = Radio_ChebyshevPower->GetFloat();
			const FLOAT ChebyshevCubedMultiplier = Radio_ChebyshevCubedMultiplier->GetFloat();
			const FLOAT ChebyshevMultiplier = Radio_ChebyshevMultiplier->GetFloat();

			// If we have a silent buffer, then allocate the samples. Then, set the sample values 
			// to zero to avoid getting NANs. Otherwise, the user may hear an annoying popping noise.
			if( pInputProcessParameters[0].BufferFlags == XAPO_BUFFER_VALID )
			{
				FLOAT* SampleData = ( FLOAT* )pInputProcessParameters[0].pBuffer;

				// Process each sample one at a time
				for( UINT SampleIndex = 0; SampleIndex < SampleSize; ++SampleIndex )
				{
					FLOAT Sample = SampleData[SampleIndex];

					// Early-out of processing if the sample is zero because a zero sample 
					// will still create some static even if no audio is playing.
					if( Sample == 0.0f )
					{
						continue;
					}

					// Waveshape it
					const FLOAT SampleCubed = Sample * Sample * Sample;
					Sample = ( ChebyshevPowerMultiplier * appPow( Sample, ChebyshevPower ) ) - ( ChebyshevCubedMultiplier * SampleCubed ) + ( ChebyshevMultiplier * Sample );

					// Again with the bandpass
					Sample = GFinalBandPassFilter.Process( Sample );

					// Store the value after done processing
					SampleData[SampleIndex] = Sample;
				}

				EndProcess();
			}
		}
	}

protected:

	/**
	 * Reinitializes the global helper sample 
	 * 
	 * @note	Called whenever SetParameters() is called on this XAPO. 
	 */
	void OnSetParameters( const void* Parameters, UINT32 ParameterSize )
	{
		// The given parameter must be a FAudioRadioEffect struct.
		check( ParameterSize == sizeof( FAudioRadioEffect ) );
		
		FAudioRadioEffect& RadioParams = *( FAudioRadioEffect* )Parameters;
	}

private:

	/** Ring buffer needed by CXAPOParametersBase */
	FAudioRadioEffect						Parameters[3];

	/** Format of the audio we're processing. */
	WAVEFORMATEX							WaveFormat;

	/** The sample rate to process the audio samples. */
	const FLOAT								SampleRate;

	/** Console variables to tweak the output */
	IConsoleVariable*						Radio_ChebyshevPowerMultiplier;
	IConsoleVariable*						Radio_ChebyshevPower;
	IConsoleVariable*						Radio_ChebyshevCubedMultiplier;
	IConsoleVariable*						Radio_ChebyshevMultiplier;

	/** Registration properties defining this radio effect class. */
	static XAPO_REGISTRATION_PROPERTIES		Registration;
};

/** Define the registration properties for the radio effect. */
XAPO_REGISTRATION_PROPERTIES FXAudio2RadioEffect::Registration =
{
	__uuidof( FXAudio2RadioEffect ),
	TEXT( "FXAudio2RadioEffect" ), 
	TEXT( "Copyright 2011 Epic Games, Inc. All Rights Reserved." ),
	1, 0,
	XAPO_FLAG_INPLACE_REQUIRED	| XAPO_FLAG_CHANNELS_MUST_MATCH
								| XAPO_FLAG_FRAMERATE_MUST_MATCH
								| XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH
								| XAPO_FLAG_BUFFERCOUNT_MUST_MATCH
								| XAPO_FLAG_INPLACE_SUPPORTED,
	1, 1, 1, 1
};

/*------------------------------------------------------------------------------------
	FXeAudioEffectsManager.
------------------------------------------------------------------------------------*/

/** 
 * Create voices that pipe the dry or EQ'd sound to the master output
 */
UBOOL FXAudio2EffectsManager::CreateEQPremasterVoices( UXAudio2Device* XAudio2Device )
{
	DWORD SampleRate = XAudio2Device->DeviceDetails.OutputFormat.Format.nSamplesPerSec;

	// Create the EQ effect
	if( !AudioDevice->ValidateAPICall( TEXT( "CreateFX (EQ)" ), 
		CreateFX( __uuidof( FXEQ ), &EQEffect ) ) )
	{
		return( FALSE );
	}

	XAUDIO2_EFFECT_DESCRIPTOR EQEffects[] =
	{
		{ EQEffect, TRUE, SPEAKER_COUNT }
	};

	XAUDIO2_EFFECT_CHAIN EQEffectChain =
	{
		1, EQEffects
	};

	if( !AudioDevice->ValidateAPICall( TEXT( "CreateSubmixVoice (EQPremaster)" ), 
		XAudio2Device->XAudio2->CreateSubmixVoice( &EQPremasterVoice, SPEAKER_COUNT, SampleRate, 0, STAGE_EQPREMASTER, NULL, &EQEffectChain ) ) )
	{
		return( FALSE );
	}

	if( !AudioDevice->ValidateAPICall( TEXT( "CreateSubmixVoice (DryPremaster)" ), 
		XAudio2Device->XAudio2->CreateSubmixVoice( &DryPremasterVoice, SPEAKER_COUNT, SampleRate, 0, STAGE_EQPREMASTER, NULL, NULL ) ) )
	{
		return( FALSE );
	}

	// Set the output matrix catering for a potential downmix
	const DWORD NumChannels = XAudio2Device->DeviceDetails.OutputFormat.Format.nChannels;
	UXAudio2Device::GetOutputMatrix( XAudio2Device->DeviceDetails.OutputFormat.dwChannelMask, NumChannels );

	if( !AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (EQPremaster)" ), 
		EQPremasterVoice->SetOutputMatrix( NULL, SPEAKER_COUNT, NumChannels, UXAudio2Device::OutputMixMatrix ) ) )
	{
		return( FALSE );
	}

	if( !AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (DryPremaster)" ), 
		DryPremasterVoice->SetOutputMatrix( NULL, SPEAKER_COUNT, NumChannels, UXAudio2Device::OutputMixMatrix ) ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

/** 
 * Create a voice that pipes the reverb sounds to the premastering voices
 */
UBOOL FXAudio2EffectsManager::CreateReverbVoice( UXAudio2Device* XAudio2Device )
{
	UINT32 Flags;

	DWORD SampleRate = XAudio2Device->DeviceDetails.OutputFormat.Format.nSamplesPerSec;
	Flags = 0;		// XAUDIO2FX_DEBUG

	// Create the reverb effect
	if( !AudioDevice->ValidateAPICall( TEXT( "CreateReverbEffect" ), 
		XAudio2CreateReverb( &ReverbEffect, Flags ) ) )
	{
		return( FALSE );
	}

	XAUDIO2_EFFECT_DESCRIPTOR ReverbEffects[] =
	{
		{ ReverbEffect, TRUE, 2 }
	};

	XAUDIO2_EFFECT_CHAIN ReverbEffectChain =
	{
		1, ReverbEffects
	};

	XAUDIO2_SEND_DESCRIPTOR SendList[] = 
	{
		{ 0, DryPremasterVoice }
	};

	const XAUDIO2_VOICE_SENDS ReverbSends = 
	{
		1, SendList
	};

	if( !AudioDevice->ValidateAPICall( TEXT( "CreateSubmixVoice (Reverb)" ), 
		XAudio2Device->XAudio2->CreateSubmixVoice( &ReverbEffectVoice, 2, SampleRate, 0, STAGE_REVERB, &ReverbSends, &ReverbEffectChain ) ) )
	{
		return( FALSE );
	}

	const FLOAT OutputMatrix[SPEAKER_COUNT * 2] = 
	{ 
		1.0f, 0.0f,
		0.0f, 1.0f, 
		0.7f, 0.7f, 
		0.0f, 0.0f, 
		1.0f, 0.0f, 
		0.0f, 1.0f 
	};

	if( !AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (Reverb)" ), 
		ReverbEffectVoice->SetOutputMatrix( DryPremasterVoice, 2, SPEAKER_COUNT, OutputMatrix ) ) )
	{
		return( FALSE );
	}

	return( TRUE );
}

/** 
 * Create a voice that pipes the radio sounds to the master output.
 *
 * @param	XAudio2Device	The audio device used by the engine.
 */
UBOOL FXAudio2EffectsManager::CreateRadioVoice( class UXAudio2Device* XAudio2Device )
{
	// Grab the sample rate, which is needed to configure the radio distortion effect settings.
	const DWORD SampleRate = XAudio2Device->DeviceDetails.OutputFormat.Format.nSamplesPerSec;
	
	// Create the custom XAPO radio distortion effect
	FXAudio2RadioEffect* NewRadioEffect = new FXAudio2RadioEffect( SampleRate );
	NewRadioEffect->Initialize( NULL, 0 );
	RadioEffect = ( IXAPO* )NewRadioEffect;

	// Define the effect chain that will be applied to 
	// the submix voice dedicated to radio distortion.
	const UBOOL bRadioEffectEnabled = TRUE;
	const INT	OutputChannelCount	= SPEAKER_COUNT;

	XAUDIO2_EFFECT_DESCRIPTOR RadioEffects[] =
	{
		{ RadioEffect, bRadioEffectEnabled, OutputChannelCount }
	};

	XAUDIO2_EFFECT_CHAIN RadioEffectChain =
	{
		1, RadioEffects
	};

	// Finally, create the submix voice that holds the radio effect. Sounds (source voices) 
	// will be piped to this submix voice to receive radio distortion. 
	if( !AudioDevice->ValidateAPICall( TEXT( "CreateSubmixVoice (Radio)" ), 
		XAudio2Device->XAudio2->CreateSubmixVoice( &RadioEffectVoice, OutputChannelCount, SampleRate, 0, STAGE_RADIO, NULL, &RadioEffectChain ) ) )
	{
		return FALSE;
	}

	const DWORD NumChannels = XAudio2Device->DeviceDetails.OutputFormat.Format.nChannels;
	UXAudio2Device::GetOutputMatrix( XAudio2Device->DeviceDetails.OutputFormat.dwChannelMask, NumChannels );

	// Designate the radio-distorted audio to route to the master voice.
	if( !AudioDevice->ValidateAPICall( TEXT( "SetOutputMatrix (Radio)" ), 
		RadioEffectVoice->SetOutputMatrix( NULL, SPEAKER_COUNT, NumChannels, UXAudio2Device::OutputMixMatrix ) ) )
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Init all sound effect related code
 */
FXAudio2EffectsManager::FXAudio2EffectsManager( UAudioDevice* InDevice )
	: FAudioEffectsManager( InDevice )
{
	UXAudio2Device* XAudio2Device = ( UXAudio2Device* )AudioDevice;
	
	check( MIN_FILTER_GAIN >= FXEQ_MIN_GAIN );
	check( MAX_FILTER_GAIN <= FXEQ_MAX_GAIN );
	check( MIN_FILTER_FREQUENCY >= FXEQ_MIN_FREQUENCY_CENTER );
	check( MAX_FILTER_FREQUENCY <= FXEQ_MAX_FREQUENCY_CENTER );

	ReverbEffect = NULL;
	EQEffect = NULL;
	RadioEffect = NULL;

	DryPremasterVoice = NULL;
	EQPremasterVoice = NULL;
	ReverbEffectVoice = NULL;
	RadioEffectVoice = NULL;

	// Create premaster voices for EQ and dry passes
	CreateEQPremasterVoices( XAudio2Device );

	// Create reverb voice 
	CreateReverbVoice( XAudio2Device );

	// Create radio voice
	CreateRadioVoice( XAudio2Device );
}

/**
 * Clean up
 */
FXAudio2EffectsManager::~FXAudio2EffectsManager( void )
{
	if( RadioEffectVoice )
	{
		RadioEffectVoice->DestroyVoice();
	}

	if( ReverbEffectVoice )
	{
		ReverbEffectVoice->DestroyVoice();
	}

	if( DryPremasterVoice )
	{
		DryPremasterVoice->DestroyVoice();
	}

	if( EQPremasterVoice )
	{
		EQPremasterVoice->DestroyVoice();
	}

	if( RadioEffect )
	{
		RadioEffect->Release();
	}

	if( ReverbEffect )
	{
		ReverbEffect->Release();
	}

	if( EQEffect )
	{
		EQEffect->Release();
	}
}

/**
 * Applies the generic reverb parameters to the XAudio2 hardware
 */
void FXAudio2EffectsManager::SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters )
{
	if( ReverbEffectVoice != NULL )
	{
		XAUDIO2FX_REVERB_I3DL2_PARAMETERS ReverbParameters;
		XAUDIO2FX_REVERB_PARAMETERS NativeParameters;

		ReverbParameters.WetDryMix = 100.0f;
		ReverbParameters.Room = VolumeToMilliBels( ReverbEffectParameters.Volume * ReverbEffectParameters.Gain, 0 );
		ReverbParameters.RoomHF = VolumeToMilliBels( ReverbEffectParameters.GainHF, -45 );
		ReverbParameters.RoomRolloffFactor = ReverbEffectParameters.RoomRolloffFactor;
		ReverbParameters.DecayTime = ReverbEffectParameters.DecayTime;
		ReverbParameters.DecayHFRatio = ReverbEffectParameters.DecayHFRatio;
		ReverbParameters.Reflections = VolumeToMilliBels( ReverbEffectParameters.ReflectionsGain, 1000 );
		ReverbParameters.ReflectionsDelay = ReverbEffectParameters.ReflectionsDelay;
		ReverbParameters.Reverb = VolumeToMilliBels( ReverbEffectParameters.LateGain, 2000 );
		ReverbParameters.ReverbDelay = ReverbEffectParameters.LateDelay;
		ReverbParameters.Diffusion = ReverbEffectParameters.Diffusion * 100.0f;
		ReverbParameters.Density = ReverbEffectParameters.Density * 100.0f;
		ReverbParameters.HFReference = DEFAULT_HIGH_FREQUENCY;

		ReverbConvertI3DL2ToNative( &ReverbParameters, &NativeParameters );

		AudioDevice->ValidateAPICall( TEXT( "SetEffectParameters (Reverb)" ), 
			ReverbEffectVoice->SetEffectParameters( 0, &NativeParameters, sizeof( NativeParameters ) ) );
	}
}

/**
 * Applies the generic EQ parameters to the XAudio2 hardware
 */
void FXAudio2EffectsManager::SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters )
{
	FXEQ_PARAMETERS NativeParameters;

	NativeParameters.FrequencyCenter0 = EQEffectParameters.LFFrequency;
	NativeParameters.Gain0 = EQEffectParameters.LFGain;
	NativeParameters.Bandwidth0 = FXEQ_DEFAULT_BANDWIDTH;

	NativeParameters.FrequencyCenter1 = EQEffectParameters.MFCutoffFrequency;
	NativeParameters.Gain1 = EQEffectParameters.MFGain;
	NativeParameters.Bandwidth1 = EQEffectParameters.MFBandwidth;

	NativeParameters.FrequencyCenter2 = EQEffectParameters.HFFrequency;
	NativeParameters.Gain2 = EQEffectParameters.HFGain;
	NativeParameters.Bandwidth2 = FXEQ_DEFAULT_BANDWIDTH;

	NativeParameters.FrequencyCenter3 = FXEQ_DEFAULT_FREQUENCY_CENTER_3;
	NativeParameters.Gain3 = FXEQ_DEFAULT_GAIN;
	NativeParameters.Bandwidth3 = FXEQ_DEFAULT_BANDWIDTH;

	AudioDevice->ValidateAPICall( TEXT( "SetEffectParameters (EQ)" ), 
		EQPremasterVoice->SetEffectParameters( 0, &NativeParameters, sizeof( NativeParameters ) ) );
}

/** 
 * Calls the platform specific code to set the parameters that define a radio effect.
 * 
 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
 */
void FXAudio2EffectsManager::SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters ) 
{
	AudioDevice->ValidateAPICall( TEXT( "SetEffectParameters (Radio)" ), 
		RadioEffectVoice->SetEffectParameters( 0, &RadioEffectParameters, sizeof( RadioEffectParameters ) ) );
}

// end
