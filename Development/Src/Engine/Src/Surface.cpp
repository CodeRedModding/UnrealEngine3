/*=============================================================================
	Surface.cpp: USurface implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(USurface);

void USurface::execGetSurfaceWidth(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(FLOAT*)Result = GetSurfaceWidth();
}

void USurface::execGetSurfaceHeight(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	*(FLOAT*)Result = GetSurfaceHeight();
}
