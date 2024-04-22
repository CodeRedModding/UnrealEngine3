/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "ALAudioPrivate.h"

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsALAudio( INT& Lookup )
{
	UALAudioDevice::StaticClass();
}

/**
 * Auto generates names.
 */
void AutoRegisterNamesALAudio()
{
}

