/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlPageFactory.h"
#include "RemoteControlFrame.h"

/**
 * The constructor registers this factory object with the RemoteControl frame.
 */
FRemoteControlPageFactory::FRemoteControlPageFactory()
{
	WxRemoteControlFrame::RegisterPageFactory( this );
}
