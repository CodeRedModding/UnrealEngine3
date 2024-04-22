/*=============================================================================
	UnPhysic.h: Physics definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// MAGIC NUMBERS
const FLOAT MAXSTEPSIDEZ = 0.08f;	// maximum z value for step side normal (if more, then treat as unclimbable)

// range of acceptable distances for pawn cylinder to float above floor when walking
const FLOAT MINFLOORDIST = 1.9f;
const FLOAT MAXFLOORDIST = 2.4f;
const FLOAT CYLINDERREPULSION = 2.2f;	// amount taken off trace dist for cylinders

const FLOAT MAXSTEPHEIGHTFUDGE = 2.f;	
const FLOAT SLOWVELOCITYSQUARED = 100.f;// velocity threshold (used for deciding to stop)
const FLOAT SHORTTRACETESTDIST = 100.f;
const FLOAT LADDEROUTPUSH = 3.f;
const FLOAT FASTWALKSPEED = 100.f;		// ~4.5 MPH
const FLOAT SWIMBOBSPEED = -80.f;
const FLOAT MINSTEPSIZESQUARED = 144.f;

