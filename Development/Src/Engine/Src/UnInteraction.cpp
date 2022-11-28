/*=============================================================================
	UnInteraction.cpp: See .UC for for info
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Wilcox
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UInteraction);
IMPLEMENT_CLASS(UConsole);

//
//	UInteraction::InputKey
//

struct UInteraction_eventInputKey_Parms
{
	FName		Key;
	EInputEvent	Event;
	FLOAT AmountDepressed;
	BITFIELD	ReturnValue;
};


/**
 * Forwards the input event on to script to process it
 *
 * @param Key The name of the key/button being pressed
 * @param Event The type of key/button event being fired
 * @param AmountDepressed The amount the button/key is being depressed (0.0 to 1.0)
 *
 * @return Whether the event was processed or not
 */
UBOOL UInteraction::InputKey(FName Key,EInputEvent Event,FLOAT AmountDepressed)
{
	UInteraction_eventInputKey_Parms	Parms;
	Parms.Key = Key;
	Parms.Event = Event;
	Parms.AmountDepressed = AmountDepressed;
	Parms.ReturnValue = 0;
	ProcessEvent(FindFunctionChecked(FName(TEXT("InputKey"))),&Parms);

	return Parms.ReturnValue;
}

//
//	UInteraction::InputAxis
//

struct UInteraction_eventInputAxis_Parms
{
	FName		Key;
	FLOAT		Delta;
	FLOAT		DeltaTime;
	BITFIELD	ReturnValue;
};

UBOOL UInteraction::InputAxis(FName Key,FLOAT Delta,FLOAT DeltaTime)
{
	UInteraction_eventInputAxis_Parms	Parms;
	Parms.Key = Key;
	Parms.Delta = Delta;
	Parms.DeltaTime = DeltaTime;
	Parms.ReturnValue = 0;
	ProcessEvent(FindFunctionChecked(FName(TEXT("InputAxis"))),&Parms);

	return Parms.ReturnValue;

}

//
//	UInteraction::InputChar
//

struct UInteraction_eventInputChar_Parms
{
	FString		Unicode;
	BITFIELD	ReturnValue;
};

UBOOL UInteraction::InputChar(TCHAR Character)
{
	UInteraction_eventInputChar_Parms	Parms;
	Parms.Unicode = FString::Printf(TEXT("%c"),Character);
	Parms.ReturnValue = 0;
	ProcessEvent(FindFunctionChecked(FName(TEXT("InputChar"))),&Parms);

	return Parms.ReturnValue;

}

//
//	UInteraction::Tick
//

struct UInteraction_eventTick_Parms
{
	FLOAT	DeltaTime;
};

void UInteraction::Tick(FLOAT DeltaTime)
{
	UInteraction_eventTick_Parms	Parms;
	Parms.DeltaTime = DeltaTime;
	ProcessEvent(FindFunctionChecked(FName(TEXT("Tick"))),&Parms);

}