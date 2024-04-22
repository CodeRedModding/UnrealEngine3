/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _IPHONETARGET_H_
#define _IPHONETARGET_H_

#include "IPhoneSocket.h"

/// Representation of a single UE3 instance running on PC
class CIPhoneTarget : public CTarget
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

	CIPhoneTarget(const sockaddr_in* InRemoteAddress, FIPhoneSocket* InTCPClient, FIPhoneSocket* InUDPClient);
	virtual ~CIPhoneTarget();

	bool ShouldSendTTY();

	/** Report the crash to CrashReporter */
	void ReportCrash();
	/** Get a human readable callstack from the backtrace */
	bool GetHumanReadableCallstack();
};

#endif
