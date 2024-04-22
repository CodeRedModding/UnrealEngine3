/*=============================================================================
	WindFacebook.cpp: Windows specific Facebook integration.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include <wininet.h>
#include "WinDrv.h"
#include "EnginePlatformInterfaceClasses.h"
#include "WinDrvClasses.h"
#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UFacebookWindows);

UBOOL UFacebookWindows::Init()
{
	return TRUE;
}

void UFacebookWindows::BeginDestroy()
{
	Super::BeginDestroy();
}

UBOOL UFacebookWindows::Authorize()
{
	// make sure we have an appid set
	if (AppID == TEXT(""))
	{
		debugf(TEXT("No AppID set in your engine .ini file!"));
		return FALSE;
	}

	ChildProcHandle = appCreateProc(TEXT("..\\UnrealAuthTool.exe"), *FString::Printf(TEXT("-appid %s"), *AppID));

	return ChildProcHandle != NULL;
}

UBOOL UFacebookWindows::IsAuthorized()
{
	return AccessToken != TEXT("");
}

void UFacebookWindows::Disconnect()
{
	AccessToken = TEXT("");
}

void UFacebookWindows::ProcessFacebookRequest(const FString& Payload, INT ResponseCode)
{
	// tell the game we finished
	FPlatformInterfaceDelegateResult Result;
	Result.bSuccessful = ResponseCode == 200;
	Result.Data.Type = PIDT_String;
	Result.Data.StringValue = Payload;

	UPlatformInterfaceBase::GetFacebookIntegrationSingleton()->CallDelegates(FID_FacebookRequestComplete, Result);
}

/**
 * Pure virtual that must be overloaded by the inheriting class. It will
 * be called from within UnLevTic.cpp after ticking all actors or from
 * the rendering thread (depending on bIsRenderingThreadObject)
 *
 * @param DeltaTime	Game time passed since the last call.
 */
void UFacebookWindows::Tick( FLOAT DeltaTime )
{
	if (ChildProcHandle)
	{
		// is it done yet?
		if (appIsProcRunning(ChildProcHandle) == FALSE)
		{
			FPlatformInterfaceDelegateResult Result;

			// read in the clipboard
			FString Contents(appClipboardPaste());
			TArray<FString> Lines;
			Contents.ParseIntoArray(&Lines, TEXT("\r\n"), TRUE);

			// auth token is first line, or "error"
			if (Lines.Num() == 0 || Lines(0) == "error")
			{
				Result.bSuccessful = FALSE;
				Result.Data.Type = PIDT_String;
				Result.Data.StringValue = Contents;
				debugf(TEXT("Failed to read response from external Facebook Auth process (passed via clipboard). Returned data:"));
				for (int Ndx=0;Ndx<Lines.Num();++Ndx)
				{
					debugf(TEXT("    %s"),*Lines(Ndx));
				}
				AccessToken = TEXT("");
				// tell script code
				CallDelegates(FID_AuthorizationComplete, Result);
			}
			else
			{
				// store the auth token for future FB requests
				AccessToken = Lines(0);
				Result.bSuccessful = TRUE;
				// Kick off request to get info about user (FID_AuthorizationComplete called when this is done)
				eventRequestFacebookMeInfo();
			}

			ChildProcHandle = NULL;
		}
	}
}

/**
 * Pure virtual that must be overloaded by the inheriting class. It is
 * used to determine whether an object is ready to be ticked. This is 
 * required for example for all UObject derived classes as they might be
 * loaded async and therefore won't be ready immediately.
 *
 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
 */
UBOOL UFacebookWindows::IsTickable() const
{
	return ChildProcHandle != NULL;
}
#endif
