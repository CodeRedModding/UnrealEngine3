/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _MACTARGET_H_
#define _MACTARGET_H_

#include "MacSocket.h"

/// Representation of a single UE3 instance running on PC
class CMacTarget : public CTarget
{
public:
	/** Version of iOS */
	wstring OSVersion;

	/** The callback for crash notifications */
	CrashCallbackPtr CrashCallback;

	/** If TRUE, the app has crashed and we are collecting a callstack */
	bool bCollectingCallstack;
	/** The callstack text lines */
	wstring CrashCallstackBuffer;
	/** The callstack entries for capturing crashes */
	vector<DWORD> CrashCallstack;
	/** The final human-readable callstack */
	wstring FinalCallstackBuffer;
	/** The build configuration executing on the device */
	wstring BuildConfigurationString;

	CMacTarget(const sockaddr_in* InRemoteAddress, FMacSocket* InTCPClient, FMacSocket* InUDPClient);
	virtual ~CMacTarget();

	bool ShouldSendTTY();

	/** Report the crash to CrashReporter */
	void ReportCrash();
	/** Get a human readable callstack from the backtrace */
	bool GetHumanReadableCallstack();
};

#endif
