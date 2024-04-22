/*=============================================================================
	MacCrashDefines.h: Unreal definitions for Mac crash reporting.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UE3_MACCRASHDEFINES_H__
#define __UE3_MACCRASHDEFINES_H__

#define MacSysCrashToken			"UE3SYS:CRASH"
#define MacSysEndCrashToken			"UE3SYS:ENDCRASH"
#define MacSysCrashString			TEXT("UE3SYS:CRASH: %s")
#define MacSysGameString			TEXT("UE3SYS:GAME: %s")
#define MacSysEngineVersionString	TEXT("UE3SYS:ENGINEVERSION: %d")
#define MacSysChangelistString		TEXT("UE3SYS:CHANGELIST: %d")
#define MacSysConfigurationString	TEXT("UE3SYS:CONFIGURATION: %s")
#define MacSysCallstackStartString	TEXT("UE3SYS:CALLSTACKSTART: %d")
#define MacSysCallstackEntryString	TEXT("UE3SYS:CALLSTACK: 0x%08x")
#define MacSysCallstackEndString	TEXT("UE3SYS:CALLSTACKEND")
#define MacSysEndCrashString		TEXT("UE3SYS:ENDCRASH")

#endif	//__UE3_MACCRASHDEFINES_H__