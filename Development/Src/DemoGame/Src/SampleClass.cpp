/*=============================================================================
	SampleClass.cpp
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	This is a minimal example of how to mix UnrealScript and C++ code
	within a class (defined by SampleClass.uc and SampleClass.cpp),
	for a package defined by DemoGame.u.
=============================================================================*/

// Includes.
#include "DemoGame.h"

/*-----------------------------------------------------------------------------
	ASampleClass class implementation.
-----------------------------------------------------------------------------*/

// C++ implementation of "SampleNativeFunction".
void ASampleClass::execSampleNativeFunction( FFrame& Stack, RESULT_DECL )
{
	// Write a message to the log.
	debugf(TEXT("Entered C++ SampleNativeFunction"));

	// Parse the UnrealScript parameters.
	P_GET_INT(Integer);		// Parse an integer parameter.
	P_GET_STR(String);		// Parse a string parameter.
	P_GET_VECTOR(Vector);	// Parse a vector parameter.
	P_FINISH;				// You must call me when finished parsing all parameters.

	// Generate a message as an FString.
	// This accesses both UnrealScript function parameters (like "String"), and UnrealScript member variables (like "MyBool").
	FString Msg = FString::Printf(TEXT("In C++ SampleNativeFunction (Integer=%i,String=%s,Bool=%i)"),Integer,*String,MyBool);

	// Call some UnrealScript functions from C++.
    debugf(TEXT("Message from C++: %s"),Msg);
	this->eventSampleEvent(Integer);
}

// Register ASampleClass.
// If you forget this, you get a VC++ linker error like:
// SampleClass.obj : error LNK2001: unresolved external symbol "private: static class UClass  ASampleClass::PrivateStaticClass" (?PrivateStaticClass@ASampleClass@@0VUClass@@A)
IMPLEMENT_CLASS(ASampleClass);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
