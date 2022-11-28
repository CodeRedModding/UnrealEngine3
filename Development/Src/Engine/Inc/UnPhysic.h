/*=============================================================================
	UnPhysic.h: Physics definition.
	Copyright 2000-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge
=============================================================================*/

const float MAXSTEPSIDEZ = 0.08f;	// maximum z value for step side normal (if more, then treat as unclimbable)
// range of acceptable distances for pawn cylinder to float above floor when walking
const float MINFLOORDIST = 1.9f;
const float MAXFLOORDIST = 2.4f;
const float CYLINDERREPULSION = 2.2f;	// amount taken off trace dist for cylinders


