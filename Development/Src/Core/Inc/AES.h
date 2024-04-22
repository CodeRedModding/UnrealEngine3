/*=============================================================================
	AES.h: Handling of Advanced Encryption Standard
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __AES_H__
#define __AES_H__

#define AES_BLOCK_SIZE				16

// If the AES key is set to this value, a runtime error will be thrown

#define INVALID_AES_KEY	"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
/**
 * AES key for decrypting coalesced ini files
 * 32 arbitrary bytes to make a 256 bit key
 *
 * Licensees should set this to their own value, IT SHOULD BE 32 RANDOM CHARACTERS!!!!
 */
#ifndef AES_KEY
#define AES_KEY	INVALID_AES_KEY
#endif

void appEncryptData( BYTE *Contents, DWORD FileSize );
void appDecryptData( BYTE *Contents, DWORD FileSize );

#endif