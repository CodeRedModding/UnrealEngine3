/*=============================================================================
	UnParams.cpp: Functions to help parse commands.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	What's happening: When the Visual Basic level editor is being used,
	this code exchanges messages with Visual Basic.  This lets Visual Basic
	affect the world, and it gives us a way of sending world information back
	to Visual Basic.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"

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
	int NumVects = 0;

	Value = FVector(0,0,0);

	// Support for old format.
	NumVects += Parse( Stream, TEXT("X="), Value.X );
	NumVects += Parse( Stream, TEXT("Y="), Value.Y );
	NumVects += Parse( Stream, TEXT("Z="), Value.Z );

	// New format.
	if( NumVects==0 )
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

	if( Found == NULL ) return 0; // didn't match.

	Start = Found + appStrlen(Match);
	if( *Start != '(' )
		return 0;

	appStrncpy( Value, Start+1, MaxLen );
	TCHAR* Temp=appStrchr( Value, ')' );
	if( Temp )
		*Temp=0;

	return 1;
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
	if (!GetSUBSTRING(Stream,Match,Temp,80)) return 0;
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
	int 	N = 0;

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
			return 0;

		Rotation.Yaw = appAtof(++Stream) * ScaleFactor;
		Stream = appStrchr(Stream,',');
		if( !Stream )
			return 0;

		Rotation.Roll = appAtof(++Stream) * ScaleFactor;
		return 1;
	}
	return (N > 0);
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
	if (!GetSUBSTRING(Stream,Match,Temp,80)) return 0;
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
		return 1;
	*Stream = Original;
	return 0;

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
	return 0;

}

//
// Output a vector.
//
TCHAR* SetFVECTOR( TCHAR* Dest, const FVector* FVector )
{
	appSprintf( Dest, TEXT("%+013.6f,%+013.6f,%+013.6f"), FVector->X, FVector->Y, FVector->Z );
	return Dest;
}

//
// Get a floating-point scale value.
//
UBOOL GetFSCALE( const TCHAR* Stream, FScale& Scale )
{
	if
	(	!GetFVECTOR( Stream, Scale.Scale )
	||	!Parse( Stream, TEXT("S="), Scale.SheerRate )
	||	!Parse( Stream, TEXT("AXIS="), Scale.SheerAxis ) )
		return 0;
	return 1;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
