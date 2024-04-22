/*=============================================================================
	BinkHeaders.h: Bink specific include headers
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef HEADERS_BINK_H
#define HEADERS_BINK_H

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#define RAD_NO_LOWERCASE_TYPES

// When compiling UDK for consoles, don't use special UDK Bink version (we don't have any).
#include "bink.h"

#undef BinkOpen
#undef BinkDoFrame
RADEXPFUNC HBINK RADEXPLINK BinkOpen(char const * name,U32 flags);
RADEXPFUNC S32  RADEXPLINK BinkDoFrame(HBINK bnk);

#include "BinkTextures.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#endif //HEADERS_BINK_H




