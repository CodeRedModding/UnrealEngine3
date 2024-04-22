/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _INC_ALAUDIO
#define _INC_ALAUDIO

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "Core.h"
#include "Engine.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "UnAudioEffect.h"

/*-----------------------------------------------------------------------------
	Audio public includes.
-----------------------------------------------------------------------------*/

#include "ALAudioDevice.h"

#if _WINDOWS

// OpenAL function prototypes.
#define AL_EXT( name, strname ) extern UBOOL SUPPORTS##name;
#define AL_PROC( name, strname, ret, func, parms ) extern ret ( CDECL * func ) parms;
#include "ALFuncs.h"
#undef AL_EXT
#undef AL_PROC

#endif

#endif

