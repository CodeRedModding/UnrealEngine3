/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class AudioDevice extends Subsystem
	config( engine )
	native( AudioDevice )
	dependson( SoundClass )
	transient;

/** 
 * Filled out with entries from DefaultEngine.ini
 */
enum ESoundClassName
{
	Master
};

/** 
 * Debug state of the audio system
 */
enum EDebugState
{
	// No debug sounds
	DEBUGSTATE_None,
	// No reverb sounds
	DEBUGSTATE_IsolateDryAudio,
	// Only reverb sounds
	DEBUGSTATE_IsolateReverb,
	// Force LPF on all sources
	DEBUGSTATE_TestLPF,
	// Bleed stereo sounds fully to the rear speakers
	DEBUGSTATE_TestStereoBleed,
	// Bleed all sounds to the LFE speaker
	DEBUGSTATE_TestLFEBleed,
	// Disable any LPF filter effects
	DEBUGSTATE_DisableLPF,
	// Disable any radio filter effects
	DEBUGSTATE_DisableRadio,
};

/**
 * The different voices available for TTS
 */
enum ETTSSpeaker
{
	TTSSPEAKER_Paul,
	TTSSPEAKER_Harry,
	TTSSPEAKER_Frank,
	TTSSPEAKER_Dennis,
	TTSSPEAKER_Kit,
	TTSSPEAKER_Betty,
	TTSSPEAKER_Ursula,
	TTSSPEAKER_Rita,
	TTSSPEAKER_Wendy,
};

/** 
 * Defines the properties of the listener
 */
struct native Listener
{
	var const PortalVolume PortalVolume;
	var vector Location;
	var vector Up;
	var vector Right;
	var vector Front;
	var vector Velocity;
};

/** 
 * Structure for collating info about sound classes
 */
struct native AudioClassInfo 
{
	var const int NumResident;
	var const int SizeResident;
	var const int NumRealTime;
	var const int SizeRealTime;
};

/** The maximum number of concurrent audible sounds */
var		config const	int								MaxChannels;
/** The amount of memory to reserve for always resident sounds */
var		config const	int								CommonAudioPoolSize;
/** Low pass filter OneOverQ value */
var		config const	float							LowPassFilterResonance;
/** Sound duration in seconds below which sounds are entirely expanded to PCM at load time in the Editor. */
var		config const	float							MinCompressedDurationEditor;
/** Sound duration in seconds below which sounds are entirely expanded to PCM at load time in the Game. */
var		config const	float							MinCompressedDurationGame;

/** Sound node wave to use for the radio chirp in sound */
var		config const	string							ChirpInSoundNodeWaveName;
var			   const	SoundNodeWave					ChirpInSoundNodeWave;
/** Sound node wave to use for the radio chirp out sound */
var		config const	string							ChirpOutSoundNodeWaveName;
var			   const	SoundNodeWave					ChirpOutSoundNodeWave;

/** Pointer to permanent memory allocation stack. */
var		native const	pointer							CommonAudioPool;
/** Available size in permanent memory stack */
var		native const	int								CommonAudioPoolFreeBytes;

var		transient const	array<AudioComponent>			AudioComponents;
var		native const	array<pointer>					Sources{FSoundSource};
var		native const	array<pointer>					FreeSources{FSoundSource};
var		native const	Map{FWaveInstance*, FSoundSource*}	WaveInstanceSourceMap;

var		native const	bool							bGameWasTicking;

var		native const	array<Listener>					Listeners;
var		native const	QWORD							CurrentTick;

/** Map of available sound classes */
var()					Map{FName, class USoundClass*}	SoundClasses;

/** Source, current and destination properties of all sound classes */
var						Map{FName, struct FSoundClassProperties}	SourceSoundClasses;
var						Map{FName, struct FSoundClassProperties}	CurrentSoundClasses;
var						Map{FName, struct FSoundClassProperties}	DestinationSoundClasses;

/** Map of available sound modes */
var		native const	Map{FName, class USoundMode*}	SoundModes;

/** Interface to audio effects processing */
var		native const	pointer							Effects{class FAudioEffectsManager};

var		native const	name							BaseSoundModeName;
var		native const	SoundMode						CurrentMode;
var		native const	double							SoundModeStartTime;
var		native const	double							SoundModeFadeInStartTime;
var		native const	double							SoundModeFadeInEndTime;
var		native const	double							SoundModeEndTime;

/** The index of the volume the listener resides in */
var		native const	int								ListenerVolumeIndex;
var		native const	InteriorSettings				ListenerInteriorSettings;

/** The times of interior volumes fading in and out */
var		native const	double							InteriorStartTime;
var		native const	double							InteriorEndTime;
var		native const	double							ExteriorEndTime;
var		native const	double							InteriorLPFEndTime;
var		native const	double							ExteriorLPFEndTime;

var		native const	float							InteriorVolumeInterp;
var		native const	float							InteriorLPFInterp;
var		native const	float							ExteriorVolumeInterp;
var		native const	float							ExteriorLPFInterp;

/** An AudioComponent to play test sounds on */
var			   const	AudioComponent					TestAudioComponent;

/** Interface to text to speech processor */
var		native const	pointer							TextToSpeech{class FTextToSpeech};

/** The debug state of the audio device */
var		native const	EDebugState						DebugState;

/** transient master volume multiplier that can be modified at runtime without affecting user settings automatically reset to 1.0 on level change */
var		transient		float							TransientMasterVolume;

/** Timestamp of the last update */
var     transient       float                           LastUpdateTime;

/** flag to stop audio component spawning, used on map change */
var     transient		bool							bSoundSpawningEnabled;

/**
 * Sets a new sound mode and applies it to all appropriate sound classes
 */
native final function bool SetSoundMode( name NewMode );

/** Find SoundClass given a Name */
native final function SoundClass FindSoundClass( Name SoundClassName );

defaultproperties
{
	TransientMasterVolume=1.0
}
