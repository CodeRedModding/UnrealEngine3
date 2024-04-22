/*=============================================================================
	IPhoneCrashDefines.h: Unreal definitions for iPhone crash reporting.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UE3_IPHONECRASHDEFINES_H__
#define __UE3_IPHONECRASHDEFINES_H__

#define iPhoneSysCrashToken				"UE3SYS:CRASH"
#define iPhoneSysEndCrashToken			"UE3SYS:ENDCRASH"
#define iPhoneSysCrashString			TEXT("UE3SYS:CRASH: %s")
#define iPhoneSysGameString				TEXT("UE3SYS:GAME: %s")
#define iPhoneSysEngineVersionString	TEXT("UE3SYS:ENGINEVERSION: %d")
#define iPhoneSysChangelistString		TEXT("UE3SYS:CHANGELIST: %d")
#define iPhoneSysConfigurationString	TEXT("UE3SYS:CONFIGURATION: %s")
#define iPhoneSysCallstackStartString	TEXT("UE3SYS:CALLSTACKSTART: %d")
#define iPhoneSysCallstackEntryString	TEXT("UE3SYS:CALLSTACK: 0x%08x")
#define iPhoneSysCallstackEndString		TEXT("UE3SYS:CALLSTACKEND")
#define iPhoneSysEndCrashString			TEXT("UE3SYS:ENDCRASH")

#endif	//__UE3_IPHONECRASHDEFINES_H__