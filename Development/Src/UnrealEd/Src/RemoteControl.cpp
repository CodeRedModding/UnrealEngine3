/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlPageFactory.h"
#include "RemoteControlActorsPage.h"
#include "RemoteControlRenderPage.h"
#include "RemoteControlStatsPage.h"
#include "RemoteControl.h"

/**
 * Registers factories for the standard RemoteControl pages.
 */
void RegisterCoreRemoteControlPages()
{
	static TRemoteControlPageFactory<WxRemoteControlRenderPage>	Obj1;
	static TRemoteControlPageFactory<WxRemoteControlActorsPage>	Obj0;
	static TRemoteControlPageFactory<WxRemoteControlStatsPage>	Obj2;
}
