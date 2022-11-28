/*=============================================================================
	UnDebugToolExec.cpp: Game debug tool implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/ 

#include "XWindowPrivate.h"
#include "DebugToolExec.h"

/**
 * Brings up a property window to edit the passed in object.
 *
 * @param Object	property to edit
 */
void FDebugToolExec::EditObject( UObject* Object )
{
	//@warning @todo @fixme: EditObject isn't aware of lifetime of UObject so it might be editing a dangling pointer!
	// This means the user is currently responsible for avoiding crashes!
	WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame( NULL, -1, 1 );	
	Properties->PropertyWindow->SetObject( Object, 0, 1, 1 );
	Properties->Show();
}

/**
 * Exec handler, parsing the passed in command
 *
 * @param Cmd	Command to parse
 * @param Ar	output device used for logging
 */
UBOOL FDebugToolExec::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Edits the class defaults.
	if( ParseCommand(&Cmd,TEXT("EDITDEFAULT")) )
	{
		UClass* Class;
		if( ParseObject<UClass>( Cmd, TEXT("CLASS="), Class, ANY_PACKAGE ) )
		{
			EditObject( Class->GetDefaultObject() );
		}
		else 
		{
			Ar.Logf( TEXT("Missing class") );
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
				if (!It->IsPendingKill())
				{
					foundObj = *It;
				}
			}
		}
		// Search by name.
		else
		{
			FName searchName;
			if (Parse(Cmd, TEXT("NAME="), searchName))
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
		}
		// Bring up an property editing window for the found object.
		if (foundObj != NULL)
		{
			EditObject(foundObj);
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

		// Search by class.
		if( ParseObject<UClass>( Cmd, TEXT("CLASS="), Class, ANY_PACKAGE ) && Class->IsChildOf(AActor::StaticClass()) )
		{
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			
			// Look for the closest actor of this class to the player.
			AActor* Player  = GameEngine && GameEngine->Players.Num() ? GameEngine->Players(0)->Actor : NULL;
			FLOAT   MinDist = FLT_MAX;
			for( TObjectIterator<AActor> It; It; ++It )
			{
				FLOAT Dist = Player ? FDist(It->Location,Player->Location) : 0.0;
				if
				(	( !Player || It->GetLevel()==Player->GetLevel() )
				&&	!It->bDeleteMe
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
				for( TObjectIterator<AActor> It; It; ++It )
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
			EditObject( Found );
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

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
