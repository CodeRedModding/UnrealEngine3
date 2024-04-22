/*=============================================================================
	ES2RHIDebug.h: OpenGL ES 2.0 RHI debugging declarations.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ES2RHIDEBUG_H__
#define __ES2RHIDEBUG_H__

#if WITH_ES2_RHI

/** Enabling this will allow three-finger-taps to cycle debug render modes (see below) */
#define ENABLE_THREE_TOUCH_MODES !FINAL_RELEASE && 0


/** Three-finger touch render modes for testing disabling various rendering portions to test speed */
enum EThreeTouchMode
{
	ThreeTouchMode_None,
	ThreeTouchMode_NullContext,
	ThreeTouchMode_TinyViewport,
	ThreeTouchMode_SingleTriangle,
	ThreeTouchMode_NoSwap,
	ThreeTouchMode_Max
};

/** The current debug mode we are in */
extern EThreeTouchMode GThreeTouchMode;


/** Runtime render control variables */
extern UBOOL GMobilePrepass;


// GLCHECK - Macro to easily print out error as they occur

#if FLASH
	#define GLCHECK(x) FES2RenderManager::RHI##x
#elif !DEBUG
	#define GLCHECK(x) x
#else
	// set this to 1 to crash on the first error
	#if 0
		#define GLCHECK_ACTION appErrorf
	#else
		#define GLCHECK_ACTION debugf
	#endif

	#define GLCHECK(x) x; { GLint Err = glGetError(); if (Err != 0) { GLCHECK_ACTION(TEXT("(%s:%d) %s got error %d"), ANSI_TO_TCHAR(__FILE__), __LINE__, ANSI_TO_TCHAR( #x ), Err); } }
#endif

/**
 * Print out information about a GL object (mostly for shader compiling output)
 *
 * @param Obj GL object (shader most likely)
 * @param Description Readable text to describe the object
 * @return Response from appMsgf if there was a failure, or -1 if no issues
 */
INT PrintInfoLog(GLuint Obj, const TCHAR* Description);

#endif

#endif // __ES2RHIDEBUG_H__
