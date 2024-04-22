/*=============================================================================
	UnCodecs.h: Movie codec definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef CODECS_H
#define CODECS_H

#ifndef USE_BINK_CODEC
#error UnBuild.h must be included before including this file (UnCodecs.h).
#endif

#if USE_BINK_CODEC
#include "../Bink/Src/UnCodecBink.h"
#else
class UCodecMovieBink : public UCodecMovie
{
	DECLARE_CLASS_INTRINSIC(UCodecMovieBink,UCodecMovie,0|CLASS_Transient,Engine)
};
#endif //USE_BINK_CODEC

#endif //CODECS_H




