/*=============================================================================
	PostWindowsApi.h: The post-include part of UE3's Win32 API wrapper.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#pragma pack(pop)

// Undo any Windows defines.
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef CDECL
#undef PF_MAX
#undef PlaySound
#undef DrawText

// Redefine CDECL to our version of the #define.  <AJS> Is this really necessary?
#define CDECL	    __cdecl					/* Standard C function */