/*=============================================================================
	CodeAudioEffects.h: Unreal CoreAudio audio effects interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_COREAUDIOEFFECTS
#define _INC_COREAUDIOEFFECTS

#define REVERB_ENABLED 1
#define EQ_ENABLED 1
#define RADIO_ENABLED 1

/** 
 * CoreAudio effects manager
 */
class FCoreAudioEffectsManager : public FAudioEffectsManager
{
public:
	FCoreAudioEffectsManager( UAudioDevice* InDevice );
	~FCoreAudioEffectsManager( void );

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define a radio effect.
	 * 
	 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
	 */
	virtual void SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters );

private:

	UBOOL bRadioAvailable;

	/** Console variables to tweak the radio effect output */
	IConsoleVariable*	Radio_ChebyshevPowerMultiplier;
	IConsoleVariable*	Radio_ChebyshevPower;
	IConsoleVariable*	Radio_ChebyshevCubedMultiplier;
	IConsoleVariable*	Radio_ChebyshevMultiplier;

	friend class UCoreAudioDevice;
	friend class FCoreAudioSoundSource;
};

#endif
