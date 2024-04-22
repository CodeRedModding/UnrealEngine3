/**
 * This class manages the waveform data for a forcefeedback device,
 * specifically for the xbox gamepads.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ForceFeedbackWaveform extends Object
	native
	editinlinenew;


/** The type of function that generates the waveform sample */
enum EWaveformFunction
{
	WF_Constant,
	WF_LinearIncreasing,
	WF_LinearDecreasing,
	WF_Sin0to90,
	WF_Sin90to180,
	WF_Sin0to180,
	WF_Noise
};

/** Holds a single sample's information */
struct native WaveformSample
{
	/**
	 * Use a byte with a range of 0 to 100 to represent the percentage of
	 * "on". This cuts the data needed to store the waveforms in half.
	 */
	var() byte LeftAmplitude;
	var() byte RightAmplitude;
	/** For function generated samples, the type of function */
	var() EWaveformFunction LeftFunction;
	var() EWaveformFunction RightFunction;
	/** The amount of time this sample plays */
	var() float Duration;

	structcpptext
	{
		friend FArchive& operator<<(FArchive& Ar,FWaveformSample& A)
		{	
			return	Ar << A.LeftAmplitude << A.RightAmplitude << A.LeftFunction
				<< A.RightFunction << A.Duration;
		}
	}
};

/** Whether this waveform should be looping or not */
var() bool bIsLooping;

/** The list of samples that make up this waveform */
var() array<WaveformSample> Samples;

/** The distance at which the waveform starts to falloff in strength */
var() float WaveformFalloffStartDistance;

/** The distance at which the waveform is no longer felt */
var() float MaxWaveformDistance;

cpptext
{
	void Serialize( FArchive& Ar )
	{
		Super::Serialize(Ar);
		if (Ar.IsLoading() && Ar.Ver() < VER_FORCEFEEDBACKWAVERFORM_NOEXPORT_CHANGE)
		{
			INT TempIsLooping = 0;
			Ar << TempIsLooping;
			bIsLooping = (TempIsLooping == 0) ? FALSE : TRUE;
			Ar << Samples;
		}
	}
}
