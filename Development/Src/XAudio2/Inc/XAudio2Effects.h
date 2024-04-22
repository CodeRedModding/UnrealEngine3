/*=============================================================================
	XAudio2Effects.h: Unreal XAudio2 audio effects interface object.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_XAUDIO2EFFECTS
#define _INC_XAUDIO2EFFECTS

/** 
 * XAudio2 effects manager
 */
class FXAudio2EffectsManager : public FAudioEffectsManager
{
public:
	FXAudio2EffectsManager( UAudioDevice* InDevice );
	~FXAudio2EffectsManager( void );

	/** 
     * Create voices that pipe the dry or EQ'd sound to the master output
	 */
	UBOOL CreateEQPremasterVoices( class UXAudio2Device* XAudio2Device );

	/** 
     * Create a voice that pipes the reverb sounds to the premastering voices
	 */
	UBOOL CreateReverbVoice( class UXAudio2Device* XAudio2Device );

	/** 
     * Create a voice that pipes the radio sounds to the master output
	 *
	 * @param	XAudio2Device	The audio device used by the engine.
	 */
	UBOOL CreateRadioVoice( class UXAudio2Device* XAudio2Device );

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters( const FAudioEQEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define a radio effect.
	 * 
	 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
	 */
	virtual void SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters );

private:
	/** 
	 * Creates the submix voice that handles the reverb effect
	 */
	UBOOL CreateEffectsSubmixVoices( void );

	/** Reverb effect */
	IUnknown*					ReverbEffect;
	/** EQ effect */
	IUnknown*					EQEffect;
	/** Radio effect */
	IUnknown*					RadioEffect;

	/** For receiving 6 channels of audio that will have no EQ applied */
	IXAudio2SubmixVoice*		DryPremasterVoice;
	/** For receiving 6 channels of audio that can have EQ applied */
	IXAudio2SubmixVoice*		EQPremasterVoice;
	/** For receiving audio that will have reverb applied */
	IXAudio2SubmixVoice*		ReverbEffectVoice;
	/** For receiving audio that will have radio effect applied */
	IXAudio2SubmixVoice*		RadioEffectVoice;

	friend class UXAudio2Device;
	friend class FXAudio2SoundSource;
};

#endif
// end
