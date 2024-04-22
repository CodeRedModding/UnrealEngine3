/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __HardwareSurveyShared_h__
#define __HardwareSurveyShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app



#ifdef __cplusplus_cli


// ... MANAGED ONLY definitions go here ...


#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else


/** Performs the hardware survey and dumps the output into an unmanaged byte array for subsequent HTTP upload. */
void PerformHardwareSurveyCLR(TArray<BYTE>& OutPayload);

/** performs the hardware survey and dumps it to disk in the log dir. used for debugging/development */
void PerformHardwareSurveyDumpCLR();



#endif	// __HardwareSurveyShared_h__