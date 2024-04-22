/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROL_H__
#define __REMOTECONTROL_H__

#ifndef USING_REMOTECONTROL
#if !CONSOLE && WITH_EDITOR && !FINAL_RELEASE
#define	USING_REMOTECONTROL 1
#else
#define USING_REMOTECONTROL 0
#endif
#endif

#if USING_REMOTECONTROL
#include "RemoteControlExec.h"
/**
 * Registers factories for the standard RemoteControl pages.
 */
void RegisterCoreRemoteControlPages();
#endif

#endif // __REMOTECONTROL_H__
