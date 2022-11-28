/*=============================================================================
	UnBuild.h: Unreal build settings.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _HEADER_UNBUILD_H_
#define _HEADER_UNBUILD_H_

#ifdef FINAL_RELEASE
	#define DO_CHECK	0
	#define STATS		0
#else
	#define DO_CHECK	1
	#define STATS		1
#endif

#define DEMOVERSION		0

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

