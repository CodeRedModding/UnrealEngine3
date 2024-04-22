/**
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "GameFramework.h"


IMPLEMENT_CLASS(USeqAct_ModifyProperty)
IMPLEMENT_CLASS(USeqAct_ControlGameMovie)


/**
* Grabs the list of all attached Objects, and then attempts to import any specified property
* values for each one.
*/
void USeqAct_ModifyProperty::Activated()
{
	if (Properties.Num() > 0 && Targets.Num() > 0)
	{
		// for each Object found,
		for (INT Idx = 0; Idx < Targets.Num(); Idx++)
		{
			UObject *Obj = Targets(Idx);
			if (Obj != NULL)
			{
				// iterate through each property
				for (INT propIdx = 0; propIdx < Properties.Num(); propIdx++)
				{
					if (Properties(propIdx).bModifyProperty)
					{
						// find the property field
						UProperty *prop = Cast<UProperty>(Obj->FindObjectField(Properties(propIdx).PropertyName,1));
						if (prop != NULL)
						{
							debugf(TEXT("Applying property %s for object %s"),*prop->GetName(),*Obj->GetName());
							// import the property text for the new Object
							prop->ImportText(*(Properties(propIdx).PropertyValue),(BYTE*)Obj + prop->Offset,0,NULL);
						}
						else
						{
							debugf(TEXT("failed to find property \"%s\" in %s"), *Properties(propIdx).PropertyName.ToString(), *Obj->GetName());
							// auto-add the pawn if property wasn't found on the controller
							if (Cast<AController>(Obj) != NULL)
							{
								Targets.AddUniqueItem(Cast<AController>(Obj)->Pawn);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		debugf(TEXT("no properties/targets %d"),Targets.Num());
	}
}

#if WITH_EDITOR
void USeqAct_ModifyProperty::CheckForErrors()
{
	Super::CheckForErrors();

	if (GWarn != NULL && GWarn->MapCheck_IsActive())
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ModifyPropertyPrototypingOnly" ), *GetPathName() ) ), TEXT( "ModifyPropertyPrototypingOnly" ) );
	}
}
#endif


void USeqAct_ControlGameMovie::Activated()
{
	if (InputLinks(0).bHasImpulse)
	{
//		debugf(TEXT("PLAY *****************************************************    USeqAct_ControlGameMovie =%s    *****************************************************"), 			*MovieName );

		// inform all clients
		UBOOL bFoundLocalPlayer = FALSE;
		for (AController* C = GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			AGamePlayerController* PC = Cast<AGamePlayerController>(C);
			if (PC != NULL)
			{
				bFoundLocalPlayer = bFoundLocalPlayer || PC->IsLocalPlayerController();
				PC->eventClientPlayMovie(MovieName, StartOfRenderingMovieFrame, EndOfRenderingMovieFrame,FALSE,TRUE,TRUE);
			}
		}

		// play it locally if no local player controller (e.g. dedicated server)
		if( !bFoundLocalPlayer &&
			GFullScreenMovie )
		{
			// No local players, and we don't allow clients to pause movies
			UINT MovieFlags = MM_PlayOnceFromStream | MF_OnlyBackButtonSkipsMovie;

			//GameThreadPlayMovie->GameThreadSetMovieHidden
			GFullScreenMovie->GameThreadPlayMovie( ( EMovieMode )MovieFlags, *MovieName, 0, StartOfRenderingMovieFrame, EndOfRenderingMovieFrame );
		}
	}
	else
	{
		//		debugf(TEXT("STOP *****************************************************    USeqAct_ControlGameMovie =%s    *****************************************************"), 			*MovieName );

		// inform all clients
		UBOOL bFoundLocalPlayer = FALSE;
		for (AController* C = GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			AGamePlayerController* PC = Cast<AGamePlayerController>(C);
			if (PC != NULL)
			{
				bFoundLocalPlayer = bFoundLocalPlayer || PC->IsLocalPlayerController();
				PC->eventClientStopMovie(0, FALSE, FALSE, FALSE);
			}
		}
		// stop it locally if no local player controller (e.g. dedicated server)
		if( !bFoundLocalPlayer &&
			GFullScreenMovie )
		{
			GFullScreenMovie->GameThreadStopMovie();
		}
	}

	OutputLinks(0).ActivateOutputLink();
}

UBOOL USeqAct_ControlGameMovie::UpdateOp(FLOAT DeltaTime)
{
	UBOOL bMovieComplete = FALSE;
	check(GFullScreenMovie);

	FString LastMovieName = GFullScreenMovie->GameThreadGetLastMovieName();
	// strip extension off the filename
	MovieName = FFilename(MovieName).GetBaseFilename();

	// If a name was specified, but that is not what is playing, say we are done
	if( (MovieName != TEXT("") && MovieName != LastMovieName) ||
		// If we are playing a movie we are interested in, and its done
		GFullScreenMovie->GameThreadIsMovieFinished(*MovieName) )
	{
		bMovieComplete = TRUE;
	}

	// activate the output if the movie finished
	if (bMovieComplete)
	{
		OutputLinks(1).ActivateOutputLink();
		// make sure the movie is stopped for everyone
		for (AController* C = GetWorldInfo()->ControllerList; C != NULL; C = C->NextController)
		{
			AGamePlayerController* PC = Cast<AGamePlayerController>(C);
			if (PC != NULL && !PC->IsLocalPlayerController())
			{
				PC->eventClientStopMovie(0, FALSE, FALSE, FALSE);
			}
		}
	}

	// stop ticking if we are done with the movie
	return bMovieComplete;
}



