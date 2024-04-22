/*=============================================================================
	UnParams.cpp: Functions to help parse commands.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

	What's happening: When the Visual Basic level editor is being used,
	this code exchanges messages with Visual Basic.  This lets Visual Basic
	affect the world, and it gives us a way of sending world information back
	to Visual Basic.
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	Getters.
	All of these functions return 1 if the appropriate item was
	fetched, or 0 if not.
-----------------------------------------------------------------------------*/

//
// Get a floating-point vector (X=, Y=, Z=).
//
UBOOL GetFVECTOR( const TCHAR* Stream, FVector& Value )
{
	INT NumVects = 0;

	Value = FVector(0,0,0);

	// Support for old format.
	NumVects += Parse( Stream, TEXT("X="), Value.X );
	NumVects += Parse( Stream, TEXT("Y="), Value.Y );
	NumVects += Parse( Stream, TEXT("Z="), Value.Z );

	// New format.
	if( NumVects == 0 )
	{
		Value.X = appAtof(Stream);
		Stream = appStrchr(Stream,',');
		if( !Stream )
		{
			return 0;
		}

		Stream++;
		Value.Y = appAtof(Stream);
		Stream = appStrchr(Stream,',');
		if( !Stream )
		{
			return 0;
		}

		Stream++;
		Value.Z = appAtof(Stream);

		NumVects=3;
	}

	return NumVects==3;
}

/**
 * Get a floating-point vector (X Y Z)
 *
 * @param The stream which has the vector in it
 * @param this is an out param which will have the FVector
 *
 * @return this will return the current location in the stream after having processed the Vector out of it
 **/
const TCHAR* GetFVECTORSpaceDelimited( const TCHAR* Stream, FVector& Value )
{
	if( Stream == NULL )
	{
		return NULL;
	}

	Value = FVector(0,0,0);


	Value.X = appAtof(Stream);
	//warnf( TEXT("Value.X %f"), Value.X );
	Stream = appStrchr(Stream,' ');
	if( !Stream )
	{
		return Stream;
	}

	Stream++;
	Value.Y = appAtof(Stream);
	//warnf( TEXT("Value.Y %f"), Value.Y );
	Stream = appStrchr(Stream,' ');
	if( !Stream )
	{
		return Stream;
	}

	Stream++;
	Value.Z = appAtof(Stream);
	//warnf( TEXT("Value.Z %f"), Value.Z );
	
	return Stream;
}


//
// Get a string enclosed in parenthesis.
//
UBOOL GetSUBSTRING
(
	const TCHAR*	Stream, 
	const TCHAR*	Match,
	TCHAR*			Value,
	INT				MaxLen
)
{
	const TCHAR* Found = appStrfind(Stream,Match);
	const TCHAR* Start;

	if( Found == NULL ) return FALSE; // didn't match.

	Start = Found + appStrlen(Match);
	if( *Start != '(' )
		return FALSE;

	appStrncpy( Value, Start+1, MaxLen );
	TCHAR* Temp=appStrchr( Value, ')' );
	if( Temp )
		*Temp=0;

	return TRUE;
}

//
// Get a floating-point vector (X=, Y=, Z=).
//
UBOOL GetFVECTOR
(
	const TCHAR*	Stream, 
	const TCHAR*	Match, 
	FVector&		Value
)
{
	TCHAR Temp[80];
	if (!GetSUBSTRING(Stream,Match,Temp,80)) return FALSE;
	return GetFVECTOR(Temp,Value);

}

//
// Get a set of rotations (PITCH=, YAW=, ROLL=), return whether anything got parsed.
//
UBOOL GetFROTATOR
(
	const TCHAR*	Stream, 
	FRotator&		Rotation,
	INT				ScaleFactor
)
{
	FLOAT	Temp=0.0;
	INT 	N = 0;

	Rotation = FRotator( 0, 0, 0 );

	// Old format.
	if( Parse(Stream,TEXT("PITCH="),Temp) ) {Rotation.Pitch = Temp * ScaleFactor; N++;}
	if( Parse(Stream,TEXT("YAW="),  Temp) ) {Rotation.Yaw   = Temp * ScaleFactor; N++;}
	if( Parse(Stream,TEXT("ROLL="), Temp) ) {Rotation.Roll  = Temp * ScaleFactor; N++;}

	// New format.
	if( N == 0 )
	{
		Rotation.Pitch = appAtof(Stream) * ScaleFactor;
		Stream = appStrchr(Stream,',');
		if( !Stream )
		{
			return FALSE;
		}

		Rotation.Yaw = appAtof(++Stream) * ScaleFactor;
		Stream = appStrchr(Stream,',');
		if( !Stream )
		{
			return FALSE;
		}

		Rotation.Roll = appAtof(++Stream) * ScaleFactor;
		return TRUE;
	}

	return (N > 0);
}


/**
 * Get an int based FRotator (X Y Z)
 *
 * @param The stream which has the rotator in it
 * @param this is an out param which will have the FRotator
 *
 * @return this will return the current location in the stream after having processed the rotator out of it
 **/
const TCHAR* GetFROTATORSpaceDelimited
(
	const TCHAR*	Stream, 
	FRotator&		Rotation,
	INT				ScaleFactor
)
{
	if( Stream == NULL )
	{
		return NULL;
	}

	Rotation = FRotator( 0, 0, 0 );


	Rotation.Pitch = appAtoi(Stream) * ScaleFactor;
	//warnf( TEXT("Rotation.Pitch %d"), Rotation.Pitch );
	Stream = appStrchr(Stream,' ');
	if( !Stream )
	{
		return Stream;
	}

	Rotation.Yaw = appAtoi(++Stream) * ScaleFactor;
	//warnf( TEXT("Rotation.Yaw %d"), Rotation.Yaw );
	Stream = appStrchr(Stream,' ');
	if( !Stream )
	{
		return Stream;
	}

	Rotation.Roll = appAtoi(++Stream) * ScaleFactor;
	//warnf( TEXT("Rotation.Roll %d"), Rotation.Roll );


	return Stream;
}


//
// Get a rotation value, return whether anything got parsed.
//
UBOOL GetFROTATOR
(
	const TCHAR*	Stream, 
	const TCHAR*	Match, 
	FRotator&		Value,
	INT				ScaleFactor
)
{
	TCHAR Temp[80];
	if (!GetSUBSTRING(Stream,Match,Temp,80)) return FALSE;
	return GetFROTATOR(Temp,Value,ScaleFactor);

}

//
// Gets a "BEGIN" string.  Returns 1 if gotten, 0 if not.
// If not gotten, doesn't affect anything.
//
UBOOL GetBEGIN( const TCHAR** Stream, const TCHAR* Match )
{
	const TCHAR* Original = *Stream;
	if( ParseCommand( Stream, TEXT("BEGIN") ) && ParseCommand( Stream, Match ) )
		return TRUE;
	*Stream = Original;
	return FALSE;

}

//
// Gets an "END" string.  Returns 1 if gotten, 0 if not.
// If not gotten, doesn't affect anything.
//
UBOOL GetEND( const TCHAR** Stream, const TCHAR* Match )
{
	const TCHAR* Original = *Stream;
	if (ParseCommand (Stream,TEXT("END")) && ParseCommand (Stream,Match)) return 1; // Gotten.
	*Stream = Original;
	return FALSE;

}

//
// Output a vector.
//
TCHAR* SetFVECTOR( TCHAR* Dest, const FVector* FVector )
{
	appSprintf( Dest, TEXT("%+013.6f,%+013.6f,%+013.6f"), FVector->X, FVector->Y, FVector->Z );
	return Dest;
}

