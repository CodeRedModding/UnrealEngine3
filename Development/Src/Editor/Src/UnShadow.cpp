/*=============================================================================
	UnLight.cpp: Bsp light mesh illumination builder code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"

extern FRebuildTools GRebuildTools;

//
//	UEditorEngine::shadowIlluminateBsp
//	Raytracing entry point.
//

void UEditorEngine::shadowIlluminateBsp(ULevel* Level)
{
	DOUBLE	StartTime = appSeconds();

	// Prepare actors for raytracing.

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if( Actor )
			Actor->PreRaytrace();
	}

	// Illuminate the level.

	for( INT ComponentIndex = 0 ; ComponentIndex < Level->ModelComponents.Num() ; ComponentIndex++ )
		Level->ModelComponents(ComponentIndex)->CacheLighting();

	// Illuminate actors.

	GWarn->BeginSlowTask(TEXT("Illuminating actors"),1);

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if( Actor )
		{
			// Calculate lighting for the actor.

			Level->Actors(ActorIndex)->CacheLighting();

			Actor->PostRaytrace();
		}
		
		GWarn->StatusUpdatef(ActorIndex,Level->Actors.Num(),TEXT("Illuminating actors"));
	}
	
	GWarn->EndSlowTask();

	debugf(TEXT("Illumination: %f seconds"),appSeconds() - StartTime);

}

/*---------------------------------------------------------------------------------------
   The End
---------------------------------------------------------------------------------------*/
