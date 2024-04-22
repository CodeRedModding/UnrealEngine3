/*=============================================================================
	UnUIStrings.cpp: UI structs, utility, and helper class implementations for rendering and formatting strings
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// engine classes
#include "EnginePrivate.h"

// widgets and supporting UI classes
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "UnUIMarkupResolver.h"
#include "Localization.h"

/**
 * Allows access to UObject::GetLanguage() from modules which aren't linked against Core
 */
const TCHAR* appGetCurrentLanguage()
{
	return UObject::GetLanguage();
}

DECLARE_CYCLE_STAT(TEXT("ParseString Time"),STAT_UIParseString,STATGROUP_UI);

