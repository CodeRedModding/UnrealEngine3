/*=============================================================================
	UnAsyncWork.cpp: Implementations of queued work classes
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"


/*-----------------------------------------------------------------------------
	FAsyncVerify.
-----------------------------------------------------------------------------*/

/**
 * Performs the async hash verification
 */
void FAsyncSHAVerify::DoWork()
{
	// default to success
	UBOOL bFailedHashLookup = FALSE;

	debugfSuppressed( NAME_DevSHA, TEXT("FAsyncSHAVerify running for hash [%s]"),*Pathname );

	// if we stored a filename to use to get the hash, get it now
	if (Pathname.Len() > 0)
	{
		// lookup the hash for the file. 
		if (FSHA1::GetFileSHAHash(*Pathname, Hash) == FALSE)
		{
			// if it couldn't be found, then we don't calculate the hash, and we "succeed" since there's no
			// hash to check against
			bFailedHashLookup = TRUE;
		}
	}

	UBOOL bFailed;

	// if we have a valid hash, check it
	if (!bFailedHashLookup)
	{
		BYTE CompareHash[20];
		// hash the buffer (finally)
		FSHA1::HashBuffer(Buffer, BufferSize, CompareHash);
		
		// make sure it matches
		bFailed = appMemcmp(Hash, CompareHash, sizeof(Hash)) != 0;
	}
	else
	{
#if SHIPPING_PC_GAME
		// if it's an error if the hash is unfound, then mark the failure. This is only done for the PC as that is an easier binary to hack
		bFailed = bIsUnfoundHashAnError;
#else
		bFailed = FALSE; 
#endif
	}

	// delete the buffer if we should, now that we are done it
	if (bShouldDeleteBuffer)
	{
		appFree(Buffer);
	}

	// if we failed, then call the failure callback
	if (bFailed)
	{
		appOnFailSHAVerification(*Pathname, bFailedHashLookup);
	}
}
