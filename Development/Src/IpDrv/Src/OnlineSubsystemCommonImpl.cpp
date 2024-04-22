/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(UOnlineSubsystemCommonImpl);

/**
 * Determine if the player is registered in the specified session
 *
 * @param PlayerId the player to check if in session or not
 * @return TRUE if the player is a registrant in the session
 */
UBOOL UOnlineSubsystemCommonImpl::IsPlayerInSession(FName SessionName,FUniqueNetId PlayerID)
{
	UBOOL bResult = FALSE;
	FNamedSession* Session = GetNamedSession(SessionName);
	if (Session != NULL)
	{
		FOnlineRegistrant Registrant(PlayerID);
		const UBOOL bIsSessionOwner = Session->GameSettings != NULL && Session->GameSettings->OwningPlayerId == PlayerID;
		if (bIsSessionOwner ||
			Session->Registrants.ContainsItem(Registrant))
		{
			bResult = TRUE;
		}
	}
	return bResult;
}

#endif	//#if WITH_UE3_NETWORKING
