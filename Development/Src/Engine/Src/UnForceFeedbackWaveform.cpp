/*=============================================================================
	UnForceFeedbackWaveform.cpp: Contains the platform independent forcefeedback
	code.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Joe Graf
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UForceFeedbackWaveform);
IMPLEMENT_CLASS(UForceFeedbackManager);

/**
 * Updates which waveform sample to use
 *
 * @param flDelta The amount of time that has elapsed
 */
void UForceFeedbackManager::UpdateWaveformData(FLOAT flDelta)
{
	// Check to see if this waveform sample has expired or not
	if (ElapsedTime + flDelta < FFWaveform->Samples(CurrentSample).Duration)
	{
		ElapsedTime += flDelta;
	}
	else
	{
		// Figure out if there is a tiny bit of time that has overlapped
		// into the next sample
		ElapsedTime = (ElapsedTime + flDelta) -
			FFWaveform->Samples(CurrentSample).Duration;
		CurrentSample++;
		// Check for a completed waveform
		if (CurrentSample == FFWaveform->Samples.Num())
		{
			// If this is a non-looping waveform zero it out
			if (FFWaveform->bIsLooping == 0)
			{
				// This waveform is done playing
				FFWaveform = NULL;
			}
			// This waveform loops until stopped (vehicles most likely)
			else
			{
				CurrentSample = 0;
				ElapsedTime = 0;
			}
		}
	}
}
