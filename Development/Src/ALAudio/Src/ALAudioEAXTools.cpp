/*=============================================================================
	ALAudioEAXTools.cpp: Unreal OpenAL Audio EAX tools.
	Copyright 1999-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
		(taken out of the EAX 3.0 SDK)
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "ALAudioPrivate.h"

#define OPENAL
//@warning: relies on modified eax.h
#include <eax.h>


/*------------------------------------------------------------------------------------
	EAX3 morphing.
------------------------------------------------------------------------------------*/


// Function prototypes used by EAX3ListenerInterpolate
static void Clamp(EAXVECTOR *eaxVector);
static bool CheckEAX3LP(LPEAXLISTENERPROPERTIES lpEAX3LP);


/***********************************************************************************************\
*
* Definition of the EAXMorph function - EAX3ListenerInterpolate
*
\***********************************************************************************************/

/*
	EAX3ListenerInterpolate
	lpStart			- Initial EAX 3 Listener parameters
	lpFinish		- Final EAX 3 Listener parameters
	flRatio			- Ratio Destination : Source (0.0 == Source, 1.0 == Destination)
	lpResult		- Interpolated EAX 3 Listener parameters
	bCheckValues	- Check EAX 3.0 parameters are in range, default = false (no checking)
*/
bool EAX3ListenerInterpolate(LPEAXLISTENERPROPERTIES lpStart, LPEAXLISTENERPROPERTIES lpFinish,
						float flRatio, LPEAXLISTENERPROPERTIES lpResult, bool bCheckValues)
{
	EAXVECTOR StartVector, FinalVector;

	float flInvRatio;

	if (bCheckValues)
	{
		if (!CheckEAX3LP(lpStart))
			return false;

		if (!CheckEAX3LP(lpFinish))
			return false;
	}

	if (flRatio >= 1.0f)
	{
		memcpy(lpResult, lpFinish, sizeof(EAXLISTENERPROPERTIES));
		return true;
	}
	else if (flRatio <= 0.0f)
	{
		memcpy(lpResult, lpStart, sizeof(EAXLISTENERPROPERTIES));
		return true;
	}

	flInvRatio = (1.0f - flRatio);

	// Environment
	lpResult->ulEnvironment = 26;	// (UNDEFINED environment)

	// Environment Size
	if (lpStart->flEnvironmentSize == lpFinish->flEnvironmentSize)
		lpResult->flEnvironmentSize = lpStart->flEnvironmentSize;
	else
		lpResult->flEnvironmentSize = (float)exp( (log(lpStart->flEnvironmentSize) * flInvRatio) + (log(lpFinish->flEnvironmentSize) * flRatio) );
	
	// Environment Diffusion
	if (lpStart->flEnvironmentDiffusion == lpFinish->flEnvironmentDiffusion)
		lpResult->flEnvironmentDiffusion = lpStart->flEnvironmentDiffusion;
	else
		lpResult->flEnvironmentDiffusion = (lpStart->flEnvironmentDiffusion * flInvRatio) + (lpFinish->flEnvironmentDiffusion * flRatio);
	
	// Room
	if (lpStart->lRoom == lpFinish->lRoom)
		lpResult->lRoom = lpStart->lRoom;
	else
		lpResult->lRoom = (int)( ((float)lpStart->lRoom * flInvRatio) + ((float)lpFinish->lRoom * flRatio) );
	
	// Room HF
	if (lpStart->lRoomHF == lpFinish->lRoomHF)
		lpResult->lRoomHF = lpStart->lRoomHF;
	else
		lpResult->lRoomHF = (int)( ((float)lpStart->lRoomHF * flInvRatio) + ((float)lpFinish->lRoomHF * flRatio) );
	
	// Room LF
	if (lpStart->lRoomLF == lpFinish->lRoomLF)
		lpResult->lRoomLF = lpStart->lRoomLF;
	else
		lpResult->lRoomLF = (int)( ((float)lpStart->lRoomLF * flInvRatio) + ((float)lpFinish->lRoomLF * flRatio) );
	
	// Decay Time
	if (lpStart->flDecayTime == lpFinish->flDecayTime)
		lpResult->flDecayTime = lpStart->flDecayTime;
	else
		lpResult->flDecayTime = (float)exp( (log(lpStart->flDecayTime) * flInvRatio) + (log(lpFinish->flDecayTime) * flRatio) );
	
	// Decay HF Ratio
	if (lpStart->flDecayHFRatio == lpFinish->flDecayHFRatio)
		lpResult->flDecayHFRatio = lpStart->flDecayHFRatio;
	else
		lpResult->flDecayHFRatio = (float)exp( (log(lpStart->flDecayHFRatio) * flInvRatio) + (log(lpFinish->flDecayHFRatio) * flRatio) );
	
	// Decay LF Ratio
	if (lpStart->flDecayLFRatio == lpFinish->flDecayLFRatio)
		lpResult->flDecayLFRatio = lpStart->flDecayLFRatio;
	else
		lpResult->flDecayLFRatio = (float)exp( (log(lpStart->flDecayLFRatio) * flInvRatio) + (log(lpFinish->flDecayLFRatio) * flRatio) );
	
	// Reflections
	if (lpStart->lReflections == lpFinish->lReflections)
		lpResult->lReflections = lpStart->lReflections;
	else
		lpResult->lReflections = (int)( ((float)lpStart->lReflections * flInvRatio) + ((float)lpFinish->lReflections * flRatio) );
	
	// Reflections Delay
	if (lpStart->flReflectionsDelay == lpFinish->flReflectionsDelay)
		lpResult->flReflectionsDelay = lpStart->flReflectionsDelay;
	else
		lpResult->flReflectionsDelay = (float)exp( (log(lpStart->flReflectionsDelay+0.0001) * flInvRatio) + (log(lpFinish->flReflectionsDelay+0.0001) * flRatio) );

	// Reflections Pan

	// To interpolate the vector correctly we need to ensure that both the initial and final vectors vectors are clamped to a length of 1.0f
	StartVector = lpStart->vReflectionsPan;
	FinalVector = lpFinish->vReflectionsPan;

	Clamp(&StartVector);
	Clamp(&FinalVector);

	if (lpStart->vReflectionsPan.x == lpFinish->vReflectionsPan.x)
		lpResult->vReflectionsPan.x = lpStart->vReflectionsPan.x;
	else
		lpResult->vReflectionsPan.x = FinalVector.x + (flInvRatio * (StartVector.x - FinalVector.x));
	
	if (lpStart->vReflectionsPan.y == lpFinish->vReflectionsPan.y)
		lpResult->vReflectionsPan.y = lpStart->vReflectionsPan.y;
	else
		lpResult->vReflectionsPan.y = FinalVector.y + (flInvRatio * (StartVector.y - FinalVector.y));
	
	if (lpStart->vReflectionsPan.z == lpFinish->vReflectionsPan.z)
		lpResult->vReflectionsPan.z = lpStart->vReflectionsPan.z;
	else
		lpResult->vReflectionsPan.z = FinalVector.z + (flInvRatio * (StartVector.z - FinalVector.z));
	
	// Reverb
	if (lpStart->lReverb == lpFinish->lReverb)
		lpResult->lReverb = lpStart->lReverb;
	else
		lpResult->lReverb = (int)( ((float)lpStart->lReverb * flInvRatio) + ((float)lpFinish->lReverb * flRatio) );
	
	// Reverb Delay
	if (lpStart->flReverbDelay == lpFinish->flReverbDelay)
		lpResult->flReverbDelay = lpStart->flReverbDelay;
	else
		lpResult->flReverbDelay = (float)exp( (log(lpStart->flReverbDelay+0.0001) * flInvRatio) + (log(lpFinish->flReverbDelay+0.0001) * flRatio) );
	
	// Reverb Pan

	// To interpolate the vector correctly we need to ensure that both the initial and final vectors are clamped to a length of 1.0f	
	StartVector = lpStart->vReverbPan;
	FinalVector = lpFinish->vReverbPan;

	Clamp(&StartVector);
	Clamp(&FinalVector);

	if (lpStart->vReverbPan.x == lpFinish->vReverbPan.x)
		lpResult->vReverbPan.x = lpStart->vReverbPan.x;
	else
		lpResult->vReverbPan.x = FinalVector.x + (flInvRatio * (StartVector.x - FinalVector.x));
	
	if (lpStart->vReverbPan.y == lpFinish->vReverbPan.y)
		lpResult->vReverbPan.y = lpStart->vReverbPan.y;
	else
		lpResult->vReverbPan.y = FinalVector.y + (flInvRatio * (StartVector.y - FinalVector.y));
	
	if (lpStart->vReverbPan.z == lpFinish->vReverbPan.z)
		lpResult->vReverbPan.z = lpStart->vReverbPan.z;
	else
		lpResult->vReverbPan.z = FinalVector.z + (flInvRatio * (StartVector.z - FinalVector.z));
	
	// Echo Time
	if (lpStart->flEchoTime == lpFinish->flEchoTime)
		lpResult->flEchoTime = lpStart->flEchoTime;
	else
		lpResult->flEchoTime = (float)exp( (log(lpStart->flEchoTime) * flInvRatio) + (log(lpFinish->flEchoTime) * flRatio) );
	
	// Echo Depth
	if (lpStart->flEchoDepth == lpFinish->flEchoDepth)
		lpResult->flEchoDepth = lpStart->flEchoDepth;
	else
		lpResult->flEchoDepth = (lpStart->flEchoDepth * flInvRatio) + (lpFinish->flEchoDepth * flRatio);

	// Modulation Time
	if (lpStart->flModulationTime == lpFinish->flModulationTime)
		lpResult->flModulationTime = lpStart->flModulationTime;
	else
		lpResult->flModulationTime = (float)exp( (log(lpStart->flModulationTime) * flInvRatio) + (log(lpFinish->flModulationTime) * flRatio) );
	
	// Modulation Depth
	if (lpStart->flModulationDepth == lpFinish->flModulationDepth)
		lpResult->flModulationDepth = lpStart->flModulationDepth;
	else
		lpResult->flModulationDepth = (lpStart->flModulationDepth * flInvRatio) + (lpFinish->flModulationDepth * flRatio);
	
	// Air Absorption HF
	if (lpStart->flAirAbsorptionHF == lpFinish->flAirAbsorptionHF)
		lpResult->flAirAbsorptionHF = lpStart->flAirAbsorptionHF;
	else
		lpResult->flAirAbsorptionHF = (lpStart->flAirAbsorptionHF * flInvRatio) + (lpFinish->flAirAbsorptionHF * flRatio);
	
	// HF Reference
	if (lpStart->flHFReference == lpFinish->flHFReference)
		lpResult->flHFReference = lpStart->flHFReference;
	else
		lpResult->flHFReference = (float)exp( (log(lpStart->flHFReference) * flInvRatio) + (log(lpFinish->flHFReference) * flRatio) );
	
	// LF Reference
	if (lpStart->flLFReference == lpFinish->flLFReference)
		lpResult->flLFReference = lpStart->flLFReference;
	else
		lpResult->flLFReference = (float)exp( (log(lpStart->flLFReference) * flInvRatio) + (log(lpFinish->flLFReference) * flRatio) );
	
	// Room Rolloff Factor
	if (lpStart->flRoomRolloffFactor == lpFinish->flRoomRolloffFactor)
		lpResult->flRoomRolloffFactor = lpStart->flRoomRolloffFactor;
	else
		lpResult->flRoomRolloffFactor = (lpStart->flRoomRolloffFactor * flInvRatio) + (lpFinish->flRoomRolloffFactor * flRatio);
	
	// Flags
	lpResult->ulFlags = (lpStart->ulFlags & lpFinish->ulFlags);

	// Clamp Delays
	if (lpResult->flReflectionsDelay > EAXLISTENER_MAXREFLECTIONSDELAY)
		lpResult->flReflectionsDelay = EAXLISTENER_MAXREFLECTIONSDELAY;

	if (lpResult->flReverbDelay > EAXLISTENER_MAXREVERBDELAY)
		lpResult->flReverbDelay = EAXLISTENER_MAXREVERBDELAY;

	return true;
}


/*
	CheckEAX3LP
	Checks that the parameters in the EAX 3 Listener Properties structure are in-range
*/
bool CheckEAX3LP(LPEAXLISTENERPROPERTIES lpEAX3LP)
{
	if ( (lpEAX3LP->lRoom < EAXLISTENER_MINROOM) || (lpEAX3LP->lRoom > EAXLISTENER_MAXROOM) )
		return false;

	if ( (lpEAX3LP->lRoomHF < EAXLISTENER_MINROOMHF) || (lpEAX3LP->lRoomHF > EAXLISTENER_MAXROOMHF) )
		return false;

	if ( (lpEAX3LP->lRoomLF < EAXLISTENER_MINROOMLF) || (lpEAX3LP->lRoomLF > EAXLISTENER_MAXROOMLF) )
		return false;

	if ( (lpEAX3LP->ulEnvironment < EAXLISTENER_MINENVIRONMENT) || (lpEAX3LP->ulEnvironment > EAXLISTENER_MAXENVIRONMENT) )
		return false;

	if ( (lpEAX3LP->flEnvironmentSize < EAXLISTENER_MINENVIRONMENTSIZE) || (lpEAX3LP->flEnvironmentSize > EAXLISTENER_MAXENVIRONMENTSIZE) )
		return false;

	if ( (lpEAX3LP->flEnvironmentDiffusion < EAXLISTENER_MINENVIRONMENTDIFFUSION) || (lpEAX3LP->flEnvironmentDiffusion > EAXLISTENER_MAXENVIRONMENTDIFFUSION) )
		return false;

	if ( (lpEAX3LP->flDecayTime < EAXLISTENER_MINDECAYTIME) || (lpEAX3LP->flDecayTime > EAXLISTENER_MAXDECAYTIME) )
		return false;

	if ( (lpEAX3LP->flDecayHFRatio < EAXLISTENER_MINDECAYHFRATIO) || (lpEAX3LP->flDecayHFRatio > EAXLISTENER_MAXDECAYHFRATIO) )
		return false;

	if ( (lpEAX3LP->flDecayLFRatio < EAXLISTENER_MINDECAYLFRATIO) || (lpEAX3LP->flDecayLFRatio > EAXLISTENER_MAXDECAYLFRATIO) )
		return false;

	if ( (lpEAX3LP->lReflections < EAXLISTENER_MINREFLECTIONS) || (lpEAX3LP->lReflections > EAXLISTENER_MAXREFLECTIONS) )
		return false;

	if ( (lpEAX3LP->flReflectionsDelay < EAXLISTENER_MINREFLECTIONSDELAY) || (lpEAX3LP->flReflectionsDelay > EAXLISTENER_MAXREFLECTIONSDELAY) )
		return false;

	if ( (lpEAX3LP->lReverb < EAXLISTENER_MINREVERB) || (lpEAX3LP->lReverb > EAXLISTENER_MAXREVERB) )
		return false;

	if ( (lpEAX3LP->flReverbDelay < EAXLISTENER_MINREVERBDELAY) || (lpEAX3LP->flReverbDelay > EAXLISTENER_MAXREVERBDELAY) )
		return false;

	if ( (lpEAX3LP->flEchoTime < EAXLISTENER_MINECHOTIME) || (lpEAX3LP->flEchoTime > EAXLISTENER_MAXECHOTIME) )
		return false;

	if ( (lpEAX3LP->flEchoDepth < EAXLISTENER_MINECHODEPTH) || (lpEAX3LP->flEchoDepth > EAXLISTENER_MAXECHODEPTH) )
		return false;

	if ( (lpEAX3LP->flModulationTime < EAXLISTENER_MINMODULATIONTIME) || (lpEAX3LP->flModulationTime > EAXLISTENER_MAXMODULATIONTIME) )
		return false;

	if ( (lpEAX3LP->flModulationDepth < EAXLISTENER_MINMODULATIONDEPTH) || (lpEAX3LP->flModulationDepth > EAXLISTENER_MAXMODULATIONDEPTH) )
		return false;

	if ( (lpEAX3LP->flAirAbsorptionHF < EAXLISTENER_MINAIRABSORPTIONHF) || (lpEAX3LP->flAirAbsorptionHF > EAXLISTENER_MAXAIRABSORPTIONHF) )
		return false;

	if ( (lpEAX3LP->flHFReference < EAXLISTENER_MINHFREFERENCE) || (lpEAX3LP->flHFReference > EAXLISTENER_MAXHFREFERENCE) )
		return false;

	if ( (lpEAX3LP->flLFReference < EAXLISTENER_MINLFREFERENCE) || (lpEAX3LP->flLFReference > EAXLISTENER_MAXLFREFERENCE) )
		return false;

	if ( (lpEAX3LP->flRoomRolloffFactor < EAXLISTENER_MINROOMROLLOFFFACTOR) || (lpEAX3LP->flRoomRolloffFactor > EAXLISTENER_MAXROOMROLLOFFFACTOR) )
		return false;

	if (lpEAX3LP->ulFlags & EAXLISTENERFLAGS_RESERVED)
		return false;

	return true;
}

/*
	Clamp
	Clamps the length of the vector to 1.0f
*/
void Clamp(EAXVECTOR *eaxVector)
{
	float flMagnitude;
	float flInvMagnitude;

	flMagnitude = (float)sqrt((eaxVector->x*eaxVector->x) + (eaxVector->y*eaxVector->y) + (eaxVector->z*eaxVector->z));

	if (flMagnitude <= 1.0f)
		return;

	flInvMagnitude = 1.0f / flMagnitude;

	eaxVector->x *= flInvMagnitude;
	eaxVector->y *= flInvMagnitude;
	eaxVector->z *= flInvMagnitude;
}


/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

