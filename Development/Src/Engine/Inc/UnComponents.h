/*=============================================================================
	UnComponents.h: Forward declarations of object components of actors
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge
=============================================================================*/

class APlayerController;

enum ETestMoveResult
{
	TESTMOVE_Stopped = 0,
	TESTMOVE_Moved = 1,
	TESTMOVE_Fell = 2,
	TESTMOVE_HitGoal = 5,
};

