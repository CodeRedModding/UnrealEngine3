/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DebugToolExec.h"
#include "PropertyWindow.h"

/**
 * Brings up a property window to edit the passed in object.
 *
 * @param Object	property to edit
 * @param bShouldShowNonEditable	whether to show properties that are normally not editable under "None"
 */
void FDebugToolExec::EditObject(UObject* Object, UBOOL bShouldShowNonEditable)
{
#if FINAL_RELEASE || SHIPPING_PC_GAME
	// the effects of this cannot be easily reversed, so prevent the user from playing network games without restarting to avoid potential exploits
	GDisallowNetworkTravel = TRUE;
#endif

	// Only allow EditObject if we're using wxWidgets.
	extern UBOOL GUsewxWindows;
	if( GUsewxWindows )
	{
		//@warning @todo @fixme: EditObject isn't aware of lifetime of UObject so it might be editing a dangling pointer!
		// This means the user is currently responsible for avoiding crashes!
		WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame;
		Properties->Create( NULL, -1);

		// Disallow closing so that closing the EditActor property window doesn't terminate the application.
		Properties->DisallowClose();

		Properties->SetObject(Object, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories | (bShouldShowNonEditable ? EPropertyWindowFlags::ShouldShowNonEditable : 0));
		Properties->Show();

		if (GIsPlayInEditorWorld)
		{
			PIEFrames.AddItem(Properties);
		}
	}
	else
	{
		debugf(TEXT("You can only edit objects after using -wxwindows on the command-line to start the game.")); 
	}
}

/**
 * Exec handler, parsing the passed in command
 *
 * @param Cmd	Command to parse
 * @param Ar	output device used for logging
 */
UBOOL FDebugToolExec::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// these commands are only allowed in standalone games
#if FINAL_RELEASE || SHIPPING_PC_GAME
	if (GWorld->GetNetMode() != NM_Standalone || (Cast<UGameEngine>(GEngine) != NULL && ((UGameEngine*)GEngine)->GPendingLevel != NULL))
	{
		return 0;
	}
	// Edits the class defaults.
	else
#endif
	if( ParseCommand(&Cmd,TEXT("EDITDEFAULT")) )
	{
		// not allowed in the editor as this command can have far reaching effects such as impacting serialization
		if (!GIsEditor)
		{
			UClass* Class = NULL;
			if( ParseObject<UClass>( Cmd, TEXT("CLASS="), Class, ANY_PACKAGE ) == FALSE )
			{
				TCHAR ClassName[256];
				if ( ParseToken(Cmd,ClassName,ARRAY_COUNT(ClassName), 1) )
				{
					Class = FindObject<UClass>( ANY_PACKAGE, ClassName);
				}
			}

			if (Class)
			{
				EditObject(Class->GetDefaultObject(), TRUE);
			}
			else
			{
				Ar.Logf( TEXT("Missing class") );
			}
		}
		return 1;
	}
	else if (ParseCommand(&Cmd,TEXT("EDITOBJECT")))
	{
		UClass* searchClass = NULL;
		UObject* foundObj = NULL;
		// Search by class.
		if (ParseObject<UClass>(Cmd, TEXT("CLASS="), searchClass, ANY_PACKAGE))
		{
			// pick the first valid object
			for (FObjectIterator It(searchClass); It && foundObj == NULL; ++It) 
			{
				if (!It->IsPendingKill() && !It->IsTemplate())
				{
					foundObj = *It;
				}
			}
		}
		// Search by name.
		else
		{
			FName searchName;
			FString SearchPathName;
			if ( Parse(Cmd, TEXT("NAME="), searchName) )
			{
				// Look for actor by name.
				for( TObjectIterator<UObject> It; It && foundObj == NULL; ++It )
				{
					if (It->GetFName() == searchName) 
					{
						foundObj = *It;
					}
				}
			}
			else if ( ParseToken(Cmd,SearchPathName, TRUE) )
			{
				foundObj = FindObject<UObject>(ANY_PACKAGE,*SearchPathName);
			}
		}

		// Bring up an property editing window for the found object.
		if (foundObj != NULL)
		{
			// not allowed in the editor unless it is a PIE object as this command can have far reaching effects such as impacting serialization
			if (!GIsEditor || ((!foundObj->IsTemplate() && (foundObj->GetOutermost()->PackageFlags & PKG_PlayInEditor))))
			{
				EditObject(foundObj, TRUE);
			}
		}
		else
		{
			Ar.Logf(TEXT("Target not found"));
		}
		return 1;
	}
	else if (ParseCommand(&Cmd,TEXT("EDITARCHETYPE")))
	{
		UObject* foundObj = NULL;
		// require fully qualified path name
		FString SearchPathName;
		if (ParseToken(Cmd, SearchPathName, TRUE))
		{
			foundObj = FindObject<UObject>(ANY_PACKAGE,*SearchPathName);
		}

		// Bring up an property editing window for the found object.
		if (foundObj != NULL)
		{
			// not allowed in the editor unless it is a PIE object as this command can have far reaching effects such as impacting serialization
			if (!GIsEditor || ((!foundObj->IsTemplate() && (foundObj->GetOutermost()->PackageFlags & PKG_PlayInEditor))))
			{
				EditObject(foundObj, FALSE);
			}
		}
		else
		{
			Ar.Logf(TEXT("Target not found"));
		}
		return 1;
	}
	// Edits an objects properties or copies them to the clipboard.
	else if( ParseCommand(&Cmd,TEXT("EDITACTOR")) )
	{
		UClass*		Class = NULL;
		AActor*		Found = NULL;

		if (ParseCommand(&Cmd, TEXT("TRACE")))
		{
			AActor* Player  = GEngine && GEngine->GamePlayers.Num() ? GEngine->GamePlayers(0)->Actor : NULL;
			APlayerController* PC = Cast<APlayerController>(Player);
			if (PC != NULL)
			{
				// Do a trace in the player's facing direction and edit anything that's hit.
				FVector PlayerLocation;
				FRotator PlayerRotation;
				PC->eventGetPlayerViewPoint(PlayerLocation, PlayerRotation);
				FCheckResult Hit(1.0f);
				// Prevent the trace intersecting with the player's pawn.
				if( PC->Pawn )
				{
					Player = PC->Pawn;
				}
				GWorld->SingleLineCheck(Hit, Player, PlayerLocation + PlayerRotation.Vector() * 10000, PlayerLocation, TRACE_SingleResult | TRACE_Actors);
				Found = Hit.Actor;
			}
		}
		// Search by class.
		else if( ParseObject<UClass>( Cmd, TEXT("CLASS="), Class, ANY_PACKAGE ) && Class->IsChildOf(AActor::StaticClass()) )
		{
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			
			// Look for the closest actor of this class to the player.
			AActor* Player  = GameEngine && GameEngine->GamePlayers.Num() ? GameEngine->GamePlayers(0)->Actor : NULL;
			APlayerController* PC = Cast<APlayerController>(Player);
			FVector PlayerLocation(0.0f);
			if (PC != NULL)
			{
				FRotator DummyRotation;
				PC->eventGetPlayerViewPoint(PlayerLocation, DummyRotation);
			}

			FLOAT   MinDist = FLT_MAX;
			for( FActorIterator It; It; ++It )
			{
				FLOAT Dist = Player ? FDist(It->Location, PlayerLocation) : 0.0;
				if
				(	!It->bDeleteMe
				&&	It->IsA(Class)
				&&	Dist < MinDist
				)
				{
					MinDist = Dist;
					Found   = *It;
				}
			}
		}
		// Search by name.
		else
		{
			FName ActorName;
			if( Parse( Cmd, TEXT("NAME="), ActorName ) )
			{
				// Look for actor by name.
				for( FActorIterator It; It; ++It )
				{
					if( It->GetFName() == ActorName )
					{
						Found = *It;
						break;
					}
				}
			}
		}

		// Bring up an property editing window for the found object.
		if( Found )
		{
			// not allowed in the editor unless it is a PIE object as this command can have far reaching effects such as impacting serialization
			if (!GIsEditor || ((!Found->IsTemplate() && (Found->GetOutermost()->PackageFlags & PKG_PlayInEditor))))
			{
				EditObject(Found, TRUE);
			}
		}
		else
		{
			Ar.Logf( TEXT("Target not found") );
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Special UnrealEd cleanup function for cleaning up after a Play In Editor session
 */
void FDebugToolExec::CleanUpAfterPIE()
{
	// destroy all PIE editobject frames
	for (INT FrameIndex = 0; FrameIndex < PIEFrames.Num(); FrameIndex++)
	{
		delete PIEFrames(FrameIndex);
	}

	// clear the array
	PIEFrames.Empty();
}
