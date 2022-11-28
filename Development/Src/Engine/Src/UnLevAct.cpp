/*=============================================================================
	UnLevAct.cpp: Level actor functions
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

#include "EngineSequenceClasses.h"

/*-----------------------------------------------------------------------------
	Level actor management.
-----------------------------------------------------------------------------*/

//
// Create a new actor. Returns the new actor, or NULL if failure.
//
AActor* ULevel::SpawnActor
(
	UClass*			Class,
	FName			InName,
	const FVector&	Location,
	const FRotator&	Rotation,
	AActor*			Template,
	UBOOL			bNoCollisionFail,
	UBOOL			bRemoteOwned,
	AActor*			Owner,
	APawn*			Instigator,
	UBOOL			bNoFail
)
{
	UBOOL	bBegunPlay = Actors.Num() && Cast<ALevelInfo>(Actors(0)) && Cast<ALevelInfo>(Actors(0))->bBegunPlay;

	// Make sure this class is spawnable.
	if( !Class )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because no class was specified") );
		return NULL;
	}
	if( Class->ClassFlags & CLASS_Abstract )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s is abstract"), Class->GetName() );
		return NULL;
	}
	else if( !Class->IsChildOf(AActor::StaticClass()) )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because %s is not an actor class"), Class->GetName() );
		return NULL;
	}
	else if( bBegunPlay && (Class->GetDefaultActor()->bStatic || Class->GetDefaultActor()->bNoDelete) )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s has bStatic or bNoDelete"), Class->GetName() );
		if ( !bNoFail )
			return NULL;		
	}

	// Use class's default actor as a template.
	if( !Template )
		Template = Class->GetDefaultActor();
	check(Template!=NULL);

	FVector NewLocation = Location;
	// Make sure actor will fit at desired location, and adjust location if necessary.
	if( (Template->bCollideWorld || (Template->bCollideWhenPlacing && (GetLevelInfo()->NetMode != NM_Client))) && !bNoCollisionFail )
	{
		if( !FindSpot( Template->GetCylinderExtent(), NewLocation ) )
		{
			debugf( NAME_Warning, TEXT("SpawnActor failed because of collision at the spawn location") );
			return NULL;
		}
	}

	// Add at end of list.
	INT iActor = Actors.Add();
    AActor* Actor = Actors(iActor) = (AActor*)StaticConstructObject( Class, GetOuter(), InName, 0, Template );

	Actor->SetFlags( RF_Transactional );
	for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
		if(Actor->Components(ComponentIndex))
			Actor->Components(ComponentIndex)->SetFlags(RF_Transactional);

	// Detect if the actor's collision component is not in the components array, which is invalid.
	if(Actor->CollisionComponent && Actor->Components.FindItemIndex(Actor->CollisionComponent) == INDEX_NONE)
		appErrorf(TEXT("Spawned actor %s with a collision component %s that is not in the Components array."),*Actor->GetFullName(),*Actor->CollisionComponent->GetFullName());

	// Set base actor properties.
	Actor->Tag			= Class->GetFName();
	Actor->Region		= FPointRegion( GetLevelInfo() );
	Actor->Level		= GetLevelInfo();
	Actor->bTicked		= !Ticked;
	Actor->XLevel		= this;
	Actor->CreationTime = GetLevelInfo()->TimeSeconds;

	// Set network role.
	check(Actor->Role==ROLE_Authority);
	if( bRemoteOwned )
		Exchange( Actor->Role, Actor->RemoteRole );

	// Set the actor's location and rotation.
	Actor->Location = NewLocation;
	Actor->Rotation = Rotation;

	// Initialize the actor's components.
	Actor->UpdateComponents();

	// init actor's physics volume
	Actor->PhysicsVolume = 	GetLevelInfo()->PhysicsVolume;

	// Set owner.
	Actor->SetOwner( Owner );

	// Set instigator
	Actor->Instigator = Instigator;

	Actor->InitRBPhys();

	// Send messages.
	Actor->InitExecution();
	Actor->Spawned();
	if(bBegunPlay)
	{
		Actor->eventPreBeginPlay();
	if( Actor->bDeleteMe && !bNoFail )
			return NULL;
		Actor->eventBeginPlay();
	if( Actor->bDeleteMe && !bNoFail )
			return NULL;
		for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
			if(Actor->Components(ComponentIndex) && Actor->Components(ComponentIndex)->Initialized)
				Actor->Components(ComponentIndex)->BeginPlay();
	}

	// Set the actor's zone.
	Actor->SetZone( iActor==0, 1 );

	// Check for encroachment.
	if( !bNoCollisionFail )
	{
		if( CheckEncroachment( Actor, Actor->Location, Actor->Rotation, 1 ) )
		{
			DestroyActor( Actor );
			return NULL;
		}
	}

	if(bBegunPlay)
	{
		// Send PostBeginPlay.
		Actor->eventPostBeginPlay();
	if( Actor->bDeleteMe && !bNoFail )
			return NULL;

		Actor->PostBeginPlay();

		// Init scripting.
		Actor->eventSetInitialState();

		// Find Base
		if( !Actor->Base && Actor->bCollideWorld && Actor->bShouldBaseAtStartup
			&& ((Actor->Physics == PHYS_None) || (Actor->Physics == PHYS_Rotating)) )
			Actor->FindBase();
	}

	// Success: Return the actor.
	if( InTick )
		NewlySpawned = new(GEngineMem)FActorLink(Actor,NewlySpawned);

	// replicated actors will have postnetbeginplay() called in net code, after initial properties are received
	if ( bBegunPlay && !bRemoteOwned )
		Actor->eventPostNetBeginPlay();

	return Actor;
}

//
// Spawn a brush.
//
ABrush* ULevel::SpawnBrush()
{
	ABrush* Result = (ABrush*)SpawnActor( ABrush::StaticClass() );
	check(Result);

	return Result;
}

/* EditorDestroyActor()
version of DestroyActor() which should be called by the editor
*/
UBOOL ULevel::EditorDestroyActor( AActor* ThisActor )
{
	check(ThisActor);
	check(ThisActor->IsValid());

	if ( (ThisActor->bPathColliding && ThisActor->bBlockActors)
		|| ThisActor->IsA(ANavigationPoint::StaticClass()) )
		GetLevelInfo()->bPathsRebuilt = 0;

	return DestroyActor(ThisActor);
}

//
// Destroy an actor.
// Returns 1 if destroyed, 0 if it couldn't be destroyed.
//
// What this routine does:
// * Remove the actor from the actor list.
// * Generally cleans up the engine's internal state.
//
// What this routine does not do, but is done in ULevel::Tick instead:
// * Removing references to this actor from all other actors.
// * Killing the actor resource.
//
// This routine is set up so that no problems occur even if the actor
// being destroyed inside its recursion stack.
//
UBOOL ULevel::DestroyActor( AActor* ThisActor, UBOOL bNetForce, UBOOL bDeferRefCleanup )
{
	check(ThisActor);
	check(ThisActor->IsValid());
	//debugf( NAME_Log, "Destroy %s", ThisActor->GetClass()->GetName() );

	// check for a destroyed sequence event
	USeqEvent_Destroyed *destroyedEvent = Cast<USeqEvent_Destroyed>(ThisActor->GetEventOfClass(USeqEvent_Destroyed::StaticClass()));
	if (destroyedEvent != NULL)
	{
		destroyedEvent->CheckActivate(ThisActor,ThisActor);
	}

	// In-game deletion rules.
	if( GetLevelInfo()->bBegunPlay )
	{
		// Can't kill bStatic and bNoDelete actors during play.
		if( ThisActor->bStatic || ThisActor->bNoDelete )
			return 0;

		// If already on list to be deleted, pretend the call was successful.
		if( ThisActor->bDeleteMe )
			return 1;

		// Can't kill if wrong role.
		if( ThisActor->Role!=ROLE_Authority && !bNetForce && !ThisActor->bNetTemporary )
			return 0;

		// Don't destroy player actors.
		APlayerController* PC = ThisActor->GetAPlayerController();
		if ( PC )
		{
				UNetConnection* C = Cast<UNetConnection>(PC->Player);
			if( C )
			{	
				if( C->Channels[0] && C->State!=USOCK_Closed )
				{
					PC->bPendingDestroy = true;
					C->Channels[0]->Close();
				}
					return 0;
				}
		}
	}

	// Get index.
	INT iActor = GetActorIndex( ThisActor );
	Actors.ModifyItem( iActor );
	ThisActor->Modify();
	ThisActor->bPendingDelete = true;

	// Send EndState notification.
	if( ThisActor->GetStateFrame() && ThisActor->GetStateFrame()->StateNode )
	{
		ThisActor->eventEndState();
		if( ThisActor->bDeleteMe )
			return 1;
	}
	// Tell this actor it's about to be destroyed.
	ThisActor->eventDestroyed();
	ThisActor->PostScriptDestroyed();
	// Remove from base.
	if( ThisActor->Base )
	{
		ThisActor->SetBase( NULL );
		if( ThisActor->bDeleteMe )
			return 1;
	}
	if( ThisActor->Attached.Num() > 0 )
		for( INT i=0; i<ThisActor->Attached.Num(); i++ )
			if ( ThisActor->Attached(i) )
				ThisActor->Attached(i)->SetBase( NULL );
	if( ThisActor->bDeleteMe )
		return 1;

	// Clean up all touching actors.
	INT iTemp = 0;
	for ( INT i=0; i<ThisActor->Touching.Num(); i++ )
		if ( ThisActor->Touching(i) && ThisActor->Touching(i)->Touching.FindItem(ThisActor, iTemp) )
		{
			ThisActor->EndTouch( ThisActor->Touching(i), 1 );
			i--;
			if( ThisActor->bDeleteMe )
			{
				return 1;
			}
		}

	// If this actor has an owner, notify it that it has lost a child.
	if( ThisActor->Owner )
	{
		ThisActor->Owner->eventLostChild( ThisActor );
		if( ThisActor->bDeleteMe )
			return 1;
	}
	// Notify net players that this guy has been destroyed.
	if( NetDriver )
		NetDriver->NotifyActorDestroyed( ThisActor );

	// Remove the actor from the actor list.
	check(Actors(iActor)==ThisActor);
	Actors(iActor) = NULL;
	ThisActor->bDeleteMe = 1;

	// Clean up the actor's components.
	ThisActor->ClearComponents();
	if(!GetLevelInfo()->bBegunPlay)
		ThisActor->InvalidateLightingCache();

	// Do object destroy.
	ThisActor->ConditionalDestroy();

	// Cleanup.
	if( GetLevelInfo()->bBegunPlay )
	{
		// During play, just add to delete-list list and destroy when level is unlocked.
		ThisActor->Deleted = FirstDeleted;
		FirstDeleted       = ThisActor;
	}
	else
	{
		if(!bDeferRefCleanup)
		{
			// Remove all references to this actor now.
			// In the editor we never actually destroy the object, as we may want to undo the delete.
			for( TObjectIterator<UObject> It; It; ++It )
			{
				UObject* Obj = *It;
				Obj->GetClass()->CleanupDestroyed( (BYTE*)Obj, Obj );
			}
		}
	}
	// Return success.
	return 1;
}

//
// Compact the actor list.
//
void ULevel::CompactActors()
{
	INT c = iFirstDynamicActor;
	for( INT i=iFirstDynamicActor; i<Actors.Num(); i++ )
	{
		if( Actors(i) )
		{
			if( !Actors(i)->bDeleteMe )
			{
				if(c != i)
					Actors.ModifyItem(c);

				Actors(c++) = Actors(i);
			}
			else
				debugf( TEXT("Undeleted %s"), *Actors(i)->GetFullName() );
		}
	}
	if( c != Actors.Num() )
		Actors.Remove( c, Actors.Num()-c );
}

//
// Cleanup destroyed actors.
// During gameplay, called in ULevel::Unlock.
// During editing, called after each actor is deleted.
//
void ULevel::CleanupDestroyed( UBOOL bForce )
{
	// Pack actor list.
	if( GetLevelInfo()->bBegunPlay && !bForce )
		CompactActors();

	// If nothing deleted, exit.
	if( !FirstDeleted )
		return;

	// Don't do anything unless a bunch of actors are in line to be destroyed.
	if( !bForce )
    {
		INT c=0;
		for( AActor* A=FirstDeleted; A; A=A->Deleted )
			c++;
		if( c<128 )
		return;
    }
	// Remove all references to actors tagged for deletion.
	for( INT iActor=0; iActor<Actors.Num(); iActor++ )
	{
		AActor* Actor = Actors(iActor);
		if( Actor )
		{
			// Would be nice to say if(!Actor->bStatic), but we can't count on it.
			checkSlow(!Actor->bDeleteMe);
			Actor->GetClass()->CleanupDestroyed( (BYTE*)Actor, Actor );
		}
	}
	// If editor, let garbage collector destroy objects.
	if( !GetLevelInfo()->bBegunPlay )
		return;

	TArray<UComponent*>	DestroyedComponents;
	for(TObjectIterator<UComponent> It;It;++It)
		if(It->IsPendingKill() && It->IsIn(GetOuter()))
			DestroyedComponents.AddItem(*It);
	while(DestroyedComponents.Num())
	{
		for(UINT ComponentIndex = 0;ComponentIndex < (UINT)DestroyedComponents.Num();ComponentIndex++)
		{
			UBOOL	HasChildren = 0;
			for(UINT OtherComponentIndex = 0;OtherComponentIndex < (UINT)DestroyedComponents.Num();OtherComponentIndex++)
			{
				if(DestroyedComponents(OtherComponentIndex)->GetOuter() == DestroyedComponents(ComponentIndex))
				{
					HasChildren = 1;
					break;
				}
			}

			if(!HasChildren)
			{
				delete DestroyedComponents(ComponentIndex);
				DestroyedComponents.Remove(ComponentIndex--);
			}
		}
	};

	// clean up any references in the scripting system
	if (GameSequence != NULL)
	{
		// build a list of all actors ready to be deleted
		TArray<UObject*> deletedActors;
		for (AActor *actor = FirstDeleted; actor != NULL; actor = actor->Deleted)
		{
			deletedActors.AddItem(actor);
		}
		if (deletedActors.Num() > 0)
		{
			FArchiveReplaceObjectRef replaceAr(GameSequence,&deletedActors);
		}
	}

	while( FirstDeleted!=NULL )
	{
		// Physically destroy the actor-to-delete.
		AActor* ActorToKill = FirstDeleted;
		FirstDeleted        = FirstDeleted->Deleted;

		check(ActorToKill->bDeleteMe);

		// Destroy the actor.
		delete ActorToKill;
	}
}

/*-----------------------------------------------------------------------------
	Player spawning.
-----------------------------------------------------------------------------*/

//
// Spawn an actor for gameplay.
//
struct FAcceptInfo
{
	AActor*			Actor;
	FString			Name;
	TArray<FString> Parms;
	FAcceptInfo( AActor* InActor, const TCHAR* InName )
	: Actor( InActor ), Name( InName ), Parms()
	{}
};
APlayerController* ULevel::SpawnPlayActor( UPlayer* Player, ENetRole RemoteRole, const FURL& URL, FString& Error )
{
	Error=TEXT("");

	// Get package map.
	UPackageMap*    PackageMap = NULL;
	UNetConnection* Conn       = Cast<UNetConnection>( Player );
	if( Conn )
		PackageMap = Conn->PackageMap;

	// Make the option string.
	TCHAR Options[1024]=TEXT("");
	for( INT i=0; i<URL.Op.Num(); i++ )
	{
		appStrcat( Options, TEXT("?") );
		appStrcat( Options, *URL.Op(i) );
	}

	if ( appStricmp(GetOuter()->GetName(),TEXT("Entry"))==0 )
	{
		// look for existing playercontroller, and return it
		for ( INT i=0; i<Actors.Num(); i++ )
			if ( Actors(i) && !Actors(i)->bDeleteMe && Actors(i)->GetAPlayerController() )
				return Actors(i)->GetAPlayerController();
	}

	// Tell UnrealScript to log in.
	INT SavedActorCount = Actors.Num();//oldver: Login should say whether to accept inventory.
	APlayerController* Actor = GetLevelInfo()->Game->eventLogin( *URL.Portal, Options, Error );
	if( !Actor )
	{
		debugf( NAME_Warning, TEXT("Login failed: %s"), *Error);
		return NULL;
	}

	UBOOL AcceptInventory = (SavedActorCount!=Actors.Num());//oldver: Hack, accepts inventory iff actor was spawned.

	// Possess the newly-spawned player.
	Actor->SetPlayer( Player );
	//debugf(TEXT("%s got player %s"),Actor->GetName(), Player->GetName());
	Actor->Role       = ROLE_Authority;
	Actor->RemoteRole = RemoteRole;
	GetLevelInfo()->Game->eventPostLogin( Actor );

	if ( Actor->Pawn && !Actor->Level->Game->bIsSaveGame )
	{
		Actor->eventTravelPreAccept();
		Actor->Pawn->eventTravelPreAccept();

		// Any saved items?
		const TCHAR* Str = NULL;
		if( AcceptInventory )
		{
			const TCHAR* PlayerName = URL.GetOption( TEXT("NAME="), *FURL::DefaultName );
			if( PlayerName )
			{
				FString* FoundItems = TravelInfo.Find( PlayerName );
				if( FoundItems )
					Str = **FoundItems;
			}
			if( !Str && GetLevelInfo()->NetMode==NM_Standalone )
			{
				TMap<FString,FString>::TIterator It(TravelInfo);
				if( It )
					Str = *It.Value();
			}
		}

		// Handle inventory items.
		TCHAR ClassName[256], ActorName[256];
		TArray<FAcceptInfo> Accepted;
		while( Str && Parse(Str,TEXT("CLASS="),ClassName,ARRAY_COUNT(ClassName)) && Parse(Str,TEXT("NAME="),ActorName,ARRAY_COUNT(ActorName)) )
		{
			// Load class.
			debugf( TEXT("Incoming travelling actor of class %s"), ClassName );//!!xyzzy
			FAcceptInfo* Accept=NULL;
			AActor* Spawned=NULL;
			UClass* Class=StaticLoadClass( AActor::StaticClass(), NULL, ClassName, NULL, LOAD_NoWarn|LOAD_AllowDll, PackageMap );
			if( !Class )
			{
				debugf( NAME_Log, TEXT("SpawnPlayActor: Cannot accept travelling class '%s'"), ClassName );
			}
			else if( Class->IsChildOf(APawn::StaticClass()) )
			{
				Accept = new(Accepted)FAcceptInfo(Actor->Pawn,ActorName);
			}
			else if( (Spawned=SpawnActor( Class, NAME_None, Actor->Pawn->Location, Actor->Pawn->Rotation, NULL, 1,0,Actor->Pawn ))==NULL )
			{
				debugf( NAME_Log, TEXT("SpawnPlayActor: Failed to spawn travelling class '%s'"), ClassName );
			}
			else
			{
				debugf( NAME_Log, TEXT("SpawnPlayActor: Spawned travelling actor") );
				Accept = new(Accepted)FAcceptInfo(Spawned,ActorName);
			}

			// Save properties.
			TCHAR Buffer[256];
			ParseLine(&Str,Buffer,ARRAY_COUNT(Buffer),1);
			ParseLine(&Str,Buffer,ARRAY_COUNT(Buffer),1);
			while( ParseLine(&Str,Buffer,ARRAY_COUNT(Buffer),1) && appStrcmp(Buffer,TEXT("}"))!=0 )
				if( Accept )
					new(Accept->Parms)FString(Buffer);
		}

		// Import properties.
		for( INT i=0; i<Accepted.Num(); i++ )
		{
			// Parse all properties.
			for( INT j=0; j<Accepted(i).Parms.Num(); j++ )
			{
				const TCHAR* Ptr = *Accepted(i).Parms(j);
				while( *Ptr==' ' )
					Ptr++;
				TCHAR VarName[256], *VarEnd=VarName;
				while( appIsAlnum(*Ptr) || *Ptr=='_' )
					*VarEnd++ = *Ptr++;
				*VarEnd=0;
				INT Element=0;
				if( *Ptr=='[' )
				{
					Element=appAtoi(++Ptr);
					while( appIsDigit(*Ptr) )
						Ptr++;
					if( *Ptr++!=']' )
						continue;
				}
				if( *Ptr++!='=' )
					continue;
				for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Accepted(i).Actor->GetClass()); It; ++It )
				{
					if
					(	(It->PropertyFlags & CPF_Travel)
					&&	appStricmp( It->GetName(), VarName )==0
					&&	Element<It->ArrayDim )
					{
						// Import the property.
						BYTE* Data = (BYTE*)Accepted(i).Actor + It->Offset + Element*It->ElementSize;
						UObjectProperty* Ref = Cast<UObjectProperty>( *It );
						if( Ref && Ref->PropertyClass->IsChildOf(AActor::StaticClass()) )
						{
							for( INT k=0; k<Accepted.Num(); k++ )
							{
								if( Accepted(k).Name==Ptr )
								{
									*(UObject**)Data = Accepted(k).Actor;
									break;
								}
							}
						}
						else It->ImportText( Ptr, Data, PPF_Localized, Accepted(i).Actor );
					}
				}
			}
		}

		// Call travel-acceptance functions in reverse order to avoid inventory flipping.
		for( INT i=Accepted.Num()-1; i>=0; i-- )
			Accepted(i).Actor->eventTravelPreAccept();
		GetLevelInfo()->Game->eventAcceptInventory( Actor->Pawn );
		for( INT i=Accepted.Num()-1; i>=0; i-- )
			Accepted(i).Actor->eventTravelPostAccept();
		Actor->eventTravelPostAccept();
	}

	return Actor;
}

/*-----------------------------------------------------------------------------
	Level actor moving/placing.
-----------------------------------------------------------------------------*/

// FindSpot()
// Find a suitable nearby location to place a collision box.
// No suitable location will ever be found if Location is not a valid point inside the level

// CheckSlice() used by FindSpot()
UBOOL ULevel::CheckSlice( FVector& Location, const FVector& Extent, INT& bKeepTrying )
{
	FCheckResult Hit(1.f);
	FVector SliceExtent = Extent;
	SliceExtent.Z = 1.f;
	bKeepTrying = 0;

	if( !EncroachingWorldGeometry( Hit,Location, SliceExtent, GetLevelInfo() ) )
	{
		// trace down to find floor
		FVector Down = FVector(0.f,0.f,Extent.Z);
		SingleLineCheck( Hit, NULL, Location - 2.f*Down, Location, TRACE_World, SliceExtent );
		FVector FloorNormal = Hit.Normal;
		if ( !Hit.Actor || (Hit.Time > 0.5f) )
		{
			// assume ceiling was causing problem
			if ( !Hit.Actor )
				Location = Location - Down;
			else
				Location = Location - (2.f*Hit.Time-1.f) * Down + FVector(0.f,0.f,1.f);

			if( !EncroachingWorldGeometry( Hit,Location, Extent, GetLevelInfo() ) )
			{
				// push back up to ceiling, and return
				SingleLineCheck( Hit, NULL, Location + Down, Location, TRACE_World, Extent );
				if ( Hit.Actor )
					Location = Hit.Location;
				return true;
			}
			else
			{
				// push out from floor, try to fit
				FloorNormal.Z = 0.f;
				Location = Location + FloorNormal * Extent.X;
				return ( !EncroachingWorldGeometry( Hit,Location, Extent, GetLevelInfo() ) );
			}
		}
		else
		{
			// assume Floor was causing problem
			Location = Location + (0.5f-Hit.Time) * 2.f*Down + FVector(0.f,0.f,1.f);
			if( !EncroachingWorldGeometry( Hit,Location, Extent, GetLevelInfo() ) )
				return true;
			else
			{
				// push out from floor, try to fit
				FloorNormal.Z = 0.f;
				Location = Location + FloorNormal * Extent.X;
				return ( !EncroachingWorldGeometry( Hit,Location, Extent, GetLevelInfo() ) );
			}
		}
	}
	bKeepTrying = 1;
	return false;
}

UBOOL ULevel::FindSpot(	const FVector& Extent, FVector& Location )
{
	FCheckResult Hit(1.f);

	// check if fits at desired location
	if( !EncroachingWorldGeometry( Hit,Location, Extent, GetLevelInfo() ) )
		return true;
	if( Extent==FVector(0.f,0.f,0.f) )
		return false;

	FVector StartLoc = Location;

	// Check if slice fits
	INT bKeepTrying = 1;
	if ( CheckSlice(Location,Extent,bKeepTrying) )
		return true;
	else if ( !bKeepTrying )
		return false;

	// Try to fit half-slices
	Location = StartLoc;
	FVector SliceExtent = 0.5f * Extent;
	SliceExtent.Z = 1.f;
	INT NumFit = 0;
	for (INT i=-1;i<2;i+=2)
		for (INT j=-1;j<2;j+=2)
			if ( NumFit < 2 )
			{
				FVector SliceOffset = FVector(0.55f*Extent.X*i, 0.55f*Extent.Y*j, 0.f);
				if ( !EncroachingWorldGeometry(Hit,StartLoc+SliceOffset, SliceExtent, GetLevelInfo()) )
				{
					NumFit++;
					Location += 1.1f * SliceOffset;
				}
			}
	if ( NumFit == 0 )
		return false;

	// find full-sized slice to check
	if ( NumFit == 1 )
		Location = 2.f * Location - StartLoc;
	SingleLineCheck( Hit, NULL, Location, StartLoc, TRACE_World );
	if ( Hit.Actor )
		return false;

	if ( !EncroachingWorldGeometry(Hit,Location, Extent, GetLevelInfo()) )
	{
		// adjust toward center
		SingleLineCheck( Hit, NULL, StartLoc + 0.2f * (StartLoc - Location), Location, TRACE_World, Extent );
		if ( Hit.Actor )
			Location = Hit.Location;
		return true;
	}
	return false;
}

//
// Try to place an actor that has moved a long way.  This is for
// moving actors through teleporters, adding them to levels, and
// starting them out in levels.  The results of this function is independent
// of the actor's current location and rotation.
//
// If the actor doesn't fit exactly in the location specified, tries
// to slightly move it out of walls and such.
//
// Returns 1 if the actor has been successfully moved, or 0 if it couldn't fit.
//
// Updates the actor's Zone and PhysicsVolume.
//
UBOOL ULevel::FarMoveActor( AActor* Actor, const FVector& DestLocation, UBOOL test, UBOOL bNoCheck, UBOOL bAttachedMove )
{
	check(Actor!=NULL);
	if( (Actor->bStatic || !Actor->bMovable) && GetLevelInfo()->bBegunPlay )
		return 0;
	if ( test && (Actor->Location == DestLocation) )
		return 1;

    FVector prevLocation = Actor->Location;
	FVector newLocation = DestLocation;
	int result = 1;

	if (!bNoCheck && (Actor->bCollideWorld || (Actor->bCollideWhenPlacing && (GetLevelInfo()->NetMode != NM_Client))) )
		result = FindSpot( Actor->GetCylinderExtent(), newLocation );

	if (result && !test && !bNoCheck)
		result = !CheckEncroachment( Actor, newLocation, Actor->Rotation, 1);
	
    if( prevLocation != Actor->Location && !test ) // CheckEncroachment moved this actor (teleported), we're done
    {
        // todo: ensure the actor was placed back into the collision hash
        //debugf(TEXT("CheckEncroachment moved this actor, we're done!"));
        //farMoveStackCnt--;
        return result;
    }
	
	if( result )
	{
		//move based actors and remove base unles this farmove was done as a test
		if ( !test )
		{
			Actor->bJustTeleported = true;
			if ( !bAttachedMove )
				Actor->SetBase(NULL);
			for ( INT i=0; i<Actor->Attached.Num(); i++ )
				if ( Actor->Attached(i) )
				{
					FarMoveActor(Actor->Attached(i),
						newLocation + Actor->Attached(i)->Location - Actor->Location,false,bNoCheck,true);
				}
		}

		Actor->Location = newLocation;
	}

	// FIXME - setzone fast if actor attached to bone

	// Set the zone after moving, so that if a PhysicsVolumeChange or ActorEntered/ActorEntered message
	// tries to move the actor, the hashing will be correct.
	if( result )
		Actor->SetZone( test,0 );

	Actor->UpdateComponents();

	return result;
}

//
// Tries to move the actor by a movement vector.  If no collision occurs, this function
// just does a Location+=Move.
//
// Assumes that the actor's Location is valid and that the actor
// does fit in its current Location. Assumes that the level's
// Dynamics member is locked, which will always be the case during
// a call to ULevel::Tick; if not locked, no actor-actor collision
// checking is performed.
//
// If bCollideWorld, checks collision with the world.
//
// For every actor-actor collision pair:
//
// If both have bCollideActors and bBlocksActors, performs collision
//    rebound, and dispatches Touch messages to touched-and-rebounded
//    actors.
//
// If both have bCollideActors but either one doesn't have bBlocksActors,
//    checks collision with other actors (but lets this actor
//    interpenetrate), and dispatches Touch and UnTouch messages.
//
// Returns 1 if some movement occured, 0 if no movement occured.
//
// Updates actor's Zone and PhysicsVolume.
//
// If Test = 1 (default 0), do not send notifications.
//
UBOOL ULevel::MoveActor
(
	AActor*			Actor,
	const FVector&	Delta,
	const FRotator&	NewRotation,
	FCheckResult&	Hit,
	UBOOL			bTest,
	UBOOL			bIgnorePawns,
	UBOOL			bIgnoreBases,
	UBOOL			bNoFail
)
{
	check(Actor!=NULL);
	if( (Actor->bStatic || !Actor->bMovable) && GetLevelInfo()->bBegunPlay )
		return 0;

	UBOOL bRelevantAttachments = (Actor->Attached.Num() != 0);
	UBOOL bNoDelta = Delta.IsZero();

	if ( bRelevantAttachments
		&& ( (bNoDelta && (!Actor->CollisionComponent || Cast<UCylinderComponent>(Actor->CollisionComponent))) 
			|| (!Actor->bCollideActors && !Actor->bCollideWorld))  )
	{
		bRelevantAttachments = false;
		for ( INT i=0; i<Actor->Attached.Num(); i++ )
			if ( Actor->Attached(i) )
			{
				bRelevantAttachments = true;
				break;
			}
	}

	// Skip if no vector.
	if( bNoDelta )
	{
		if( NewRotation==Actor->Rotation )
		{
			return 1;
		}
		if( !bRelevantAttachments && (!Actor->CollisionComponent || Cast<UCylinderComponent>(Actor->CollisionComponent)) )
		{
			Actor->Rotation = NewRotation;
			if ( !bTest )
			{
				Actor->UpdateRelativeRotation();
			Actor->UpdateComponents();
			}
			return 1;
		}
	}
	if ( !bRelevantAttachments && (!Actor->CollisionComponent || (!Actor->bCollideActors && !Actor->bCollideWorld)) )
	{
		Actor->Location += Delta;
		Actor->UpdateComponents();
		if ( NewRotation == Actor->Rotation )
			return 1;
		Actor->Rotation = NewRotation;
		if ( !bTest )
		{
			Actor->UpdateRelativeRotation();
		}
		return 1;
	}

	// Set up.

	FMemMark Mark(GMem);
	Hit = FCheckResult(1.f);
	FLOAT DeltaSize;
	FVector DeltaDir;
	if( Delta.IsNearlyZero() )
	{
		DeltaSize = 0;
		DeltaDir = Delta;
	}
	else
	{
		DeltaSize = Delta.Size();
		DeltaDir       = Delta/DeltaSize;
	}
	FLOAT TestAdjust	   = 2.f;
	FVector TestDelta      = Delta + TestAdjust * DeltaDir;
	INT     MaybeTouched   = 0;
	FCheckResult* FirstHit = NULL;

	UBOOL doEncroachTouch = 1;

	// Perform movement collision checking if needed for this actor.
	if((Actor->bCollideActors || Actor->bCollideWorld) &&
		Actor->CollisionComponent &&
		!Actor->IsEncroacher() &&
		(DeltaSize != 0.f) )
	{
		doEncroachTouch = 0;

		// Check collision along the line.
		DWORD TraceFlags = 0;
		if ( Actor->bCollideActors )
		{
			if ( !bIgnorePawns )
				TraceFlags |= TRACE_Pawns;
			TraceFlags |= TRACE_Others | TRACE_Volumes;
		}
		if ( Actor->bCollideWorld )
			TraceFlags |= TRACE_World;

		FVector ColCenter;

		if(Actor->CollisionComponent->IsValidComponent())
		{
			check(Actor->CollisionComponent->Initialized);
			ColCenter = Actor->CollisionComponent->Bounds.Origin;
		}
		else
			ColCenter = Actor->Location;

		FirstHit = MultiLineCheck
		(
			GMem,
			ColCenter + TestDelta,
			ColCenter,
			Actor->GetCylinderExtent(),
			Actor->bCollideWorld  ? GetLevelInfo() : NULL,
			TraceFlags,
			Actor
		);

		// Handle first blocking actor.
		if( Actor->bCollideWorld || Actor->bBlockActors )
		{
			for( FCheckResult* Test=FirstHit; Test; Test=Test->GetNext() )
			{
				if( (!bIgnoreBases || !Actor->IsBasedOn(Test->Actor)) && !Test->Actor->IsBasedOn(Actor) )
				{
					MaybeTouched = 1;
					if( Actor->IsBlockedBy(Test->Actor,Test->Component) )
					{
						Hit = *Test;
						break;
					}
				}
			}
		}
	}

	// Attenuate movement.
	FVector FinalDelta = Delta;
	if( Hit.Time < 1.f && !bNoFail )
	{
		// Fix up delta, given that TestDelta = Delta + TestAdjust.
		FLOAT FinalDeltaSize = (DeltaSize + TestAdjust) * Hit.Time;
		if ( FinalDeltaSize <= TestAdjust)
		{
			FinalDelta = FVector(0.f,0.f,0.f);
			Hit.Time = 0.f;
		}
		else
		{
			FinalDelta = TestDelta * Hit.Time - TestAdjust * DeltaDir;
			Hit.Time   = (FinalDeltaSize - TestAdjust) / DeltaSize;
		}
	}

	// Abort if encroachment declined.
	if( !bTest && !bNoFail && Actor->IsEncroacher() && CheckEncroachment( Actor, Actor->Location + FinalDelta, NewRotation, doEncroachTouch ) )
	{
		Mark.Pop();
		return 0;
	}

	// Move the based actors (after encroachment checking).
	Actor->Location += FinalDelta;
	FRotator OldRotation = Actor->Rotation;
	Actor->Rotation  = NewRotation;

	if( (Actor->Attached.Num() > 0) && !bTest )
	{
		// Move base.
		FRotator ReducedRotation(0,0,0);

		FMatrix OldMatrix = FRotationMatrix( OldRotation ).Transpose();
		FMatrix NewMatrix = FRotationMatrix( NewRotation );
		
		if( OldRotation != Actor->Rotation )
		{
			ReducedRotation = FRotator( ReduceAngle(Actor->Rotation.Pitch)	- ReduceAngle(OldRotation.Pitch),
										ReduceAngle(Actor->Rotation.Yaw)	- ReduceAngle(OldRotation.Yaw)	,
										ReduceAngle(Actor->Rotation.Roll)	- ReduceAngle(OldRotation.Roll) );
		}

		// Calculate new transform matrix of base actor (ignoring scale).
		FMatrix BaseTM = FMatrix(Actor->Location, Actor->Rotation);

		for( INT i=0; i<Actor->Attached.Num(); i++ )
		{
			// For non-skeletal based actors. Skeletal-based actors are handled inside USkeletalMeshComponent::Update
			AActor* Other = Actor->Attached(i);
			if ( Other && !Other->BaseSkelComponent )
			{
				FVector   RotMotion( 0, 0, 0 );
				FCheckResult OtherHit(1.f);

				if( Other->Physics == PHYS_Interpolating || (Other->bHardAttach && !Other->bBlockActors) )
				{
					FMatrix HardRelMatrix = FMatrix(Other->RelativeLocation, Other->RelativeRotation);
					FMatrix NewWorldTM = HardRelMatrix * BaseTM;
					FVector NewWorldPos = NewWorldTM.GetOrigin();
					FRotator NewWorldRot = NewWorldTM.Rotator();
					MoveActor( Other, NewWorldPos - Other->Location, NewWorldRot, OtherHit, 0, 0, 1 );
				}
				else
				{
					FRotator finalRotation = Other->Rotation + ReducedRotation;

					if( OldRotation != Actor->Rotation )
					{
						// update player view rotation
						APawn *P = Other->GetAPawn();
						FLOAT ControllerRoll = 0;
						if ( P && P->Controller )
							{
								ControllerRoll = P->Controller->Rotation.Roll;
							P->Controller->Rotation += ReducedRotation;
							}

							// If its a pawn, and its not a crawler, remove roll.
							if( P && !P->bCrawler )
							{
								finalRotation.Roll = Other->Rotation.Roll;
								if( P->Controller )
									P->Controller->Rotation.Roll = ControllerRoll;
							}
						
						// Handle rotation-induced motion.
						RotMotion = NewMatrix.TransformFVector( OldMatrix.TransformFVector( Other->RelativeLocation ) ) - Other->RelativeLocation;
					}
				
				    // move attached actor
					MoveActor( Other, FinalDelta + RotMotion, finalRotation, OtherHit, 0, 0, 1 );
				}

				if ( !bNoFail && Other->IsBlockedBy(Actor,Actor->CollisionComponent) )
				{
					// check if encroached
					UBOOL bStillEncroaching = Other->IsOverlapping(Actor);

					// if encroachment declined, move back to old location
					if ( bStillEncroaching && Actor->eventEncroachingOn(Other) )
					{
						Actor->Location -= FinalDelta;
						Actor->Rotation = OldRotation;
						for( INT j=0; j<Actor->Attached.Num(); j++ )
							if ( Actor->Attached(j) )
								MoveActor( Actor->Attached(j), -1.f * FinalDelta, Actor->Attached(j)->Rotation, OtherHit, 0, 0, 1 );
						Mark.Pop();
						return 0;
					}
				}
			}
		}
	}

	// Update the location.

	// update relative location of this actor
	if ( !bTest && !Actor->bHardAttach && Actor->Physics != PHYS_Interpolating )
	{
		if ( Actor->Base )
			Actor->RelativeLocation = Actor->Location - Actor->Base->Location;
		if ( OldRotation != Actor->Rotation )
			Actor->UpdateRelativeRotation();
	}

	// Handle bump and touch notifications.
	if( !bTest )
	{
		// Notify first bumped actor unless it's the level or the actor's base.
		if( Hit.Actor && !Hit.Actor->bWorldGeometry && !Actor->IsBasedOn(Hit.Actor) )
		{
			// Notify both actors of the bump.
			Hit.Actor->NotifyBump(Actor, Hit.Normal);
			Actor->NotifyBump(Hit.Actor, Hit.Normal);
		}

		// Handle Touch notifications.
		if( MaybeTouched || !Actor->bBlockActors )
			for( FCheckResult* Test=FirstHit; Test && Test->Time<Hit.Time; Test=Test->GetNext() )
			{
				if ( (!bIgnoreBases || !Actor->IsBasedOn(Test->Actor)) &&
					(!Actor->IsBlockedBy(Test->Actor,Test->Component)) && Actor != Test->Actor)
					Actor->BeginTouch( Test->Actor, Test->Location, Test->Normal );
			}

		// UnTouch notifications.
		for( int i=0; i<Actor->Touching.Num(); )
		{
			if( Actor->Touching(i) && !Actor->IsOverlapping(Actor->Touching(i)) )
				Actor->EndTouch( Actor->Touching(i), 0 );
			else
				i++;
		}
	}
	// Set actor zone.
	Actor->SetZone( bTest,0 );
	Mark.Pop();

	// Update render data.
	Actor->UpdateComponents();

	// Return whether we moved at all.

	return Hit.Time>0.f;
}

void AActor::NotifyBump(AActor *Other, const FVector &HitNormal)
{
	eventBump(Other, HitNormal);
}

void APawn::NotifyBump(AActor *Other, const FVector &HitNormal)
{
	if ( !Controller || !Controller->eventNotifyBump(Other, HitNormal) )
		eventBump(Other, HitNormal);
}
/*-----------------------------------------------------------------------------
	Encroachment.
-----------------------------------------------------------------------------*/

//
// Check whether Actor is encroaching other actors after a move, and return
// 0 to ok the move, or 1 to abort it.
//
UBOOL ULevel::CheckEncroachment
(
	AActor*			Actor,
	FVector			TestLocation,
	FRotator		TestRotation,
	UBOOL			bTouchNotify
)
{
	check(Actor);

	// If this actor doesn't need encroachment checking, allow the move.
	if( !Actor->bCollideActors &&
		!Actor->bBlockActors &&
		!Actor->IsEncroacher() )
		return 0;

	// set up matrices for calculating movement caused by mover rotation
	FMatrix WorldToLocal, TestLocalToWorld;
	if ( Actor->IsEncroacher() )
	{
		WorldToLocal = Actor->WorldToLocal();
		FVector RealLocation = Actor->Location;
		FRotator RealRotation = Actor->Rotation;
		Actor->Location = TestLocation;
		Actor->Rotation = TestRotation;
		TestLocalToWorld = Actor->LocalToWorld();
		Actor->Location = RealLocation;
		Actor->Rotation = RealRotation;
	}

	// Query the mover about what he wants to do with the actors he is encroaching.
	FMemMark Mark(GMem);
	FCheckResult* FirstHit = Hash ? Hash->ActorEncroachmentCheck( GMem, Actor, TestLocation, TestRotation, TRACE_AllColliding ) : NULL;	
	for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
	{
		if
		(	Test->Actor!=Actor
		&&	Test->Actor!=GetLevelInfo()
		&&  !Test->Actor->IsBasedOn(Actor)
		&&	Actor->IsBlockedBy( Test->Actor, Test->Component ) )
		{
			UBOOL bStillEncroaching = true;
			// Actors can be pushed by movers or karma stuff.
			if ( Actor->IsEncroacher() && !Test->Actor->IsEncroacher() )
			{
				// check if mover can safely push encroached actor
				// Move test actor away from mover
				FVector MoveDir = TestLocation - Actor->Location;
				FVector OldLoc = Test->Actor->Location;
				FVector Dest = Test->Actor->Location + MoveDir;
				if ( TestRotation != Actor->Rotation )
				{
					FVector TestLocalLoc = WorldToLocal.TransformFVector(Test->Actor->Location);
					// multiply X 1.5 to account for max bounding box center to colliding edge dist change
					MoveDir += 1.5f * (TestLocalToWorld.TransformFVector(TestLocalLoc) - Test->Actor->Location);
				}
				Test->Actor->moveSmooth(MoveDir);

				// see if mover still encroaches test actor
				// Save actor's location and rotation.
				Exchange( TestLocation, Actor->Location );
				Exchange( TestRotation, Actor->Rotation );

				FCheckResult TestHit(1.f);
				bStillEncroaching = Test->Actor->IsOverlapping(Actor, &TestHit);

				// Restore actor's location and rotation.
				Exchange( TestLocation, Actor->Location );
				Exchange( TestRotation, Actor->Rotation );

				if ( !bStillEncroaching && (MoveDir.SizeSquared() > 4.f) ) //push test actor back toward brush
				{
					FVector realLoc = Actor->Location;
					Actor->Location = TestLocation;
					MoveActor( Test->Actor, -0.5f * MoveDir, Test->Actor->Rotation, TestHit );
					Actor->Location = realLoc;
				}
			}
			if ( bStillEncroaching && Actor->eventEncroachingOn(Test->Actor) )
			{
				Mark.Pop();
				return 1;
			}
			else 
				Actor->eventRanInto(Test->Actor);
		}
	}

	// If bTouchNotify, send Touch and UnTouch notifies.
	if( bTouchNotify )
	{
		// UnTouch notifications.
		for( int i=0; i<Actor->Touching.Num(); )
		{
			if( Actor->Touching(i) && !Actor->IsOverlapping(Actor->Touching(i)) )
				Actor->EndTouch( Actor->Touching(i), 0 );
			else
				i++;
		}
	}

	// Notify the encroached actors but not the level.
	for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
		if
		(	Test->Actor!=Actor
		&&  !Test->Actor->IsBasedOn(Actor)
		&&	Test->Actor!=GetLevelInfo() )
		{
			if( Actor->IsBlockedBy(Test->Actor,Test->Component) )
			{
				Test->Actor->eventEncroachedBy(Actor);
			}
			else if( bTouchNotify )
			{
				// Make sure Test->Location is not Zero, if that's the case, use TestLocation
				FVector	HitLocation = Test->Location.IsZero() ? TestLocation : Test->Location;

				// Make sure we have a valid Normal
				FVector NormalDir = Test->Normal.IsZero() ? (TestLocation - Actor->Location) : Test->Normal;
				if( !NormalDir.IsZero() )
				{
					NormalDir.Normalize();
				}
				else
				{
					NormalDir = TestLocation - Test->Actor->Location;
					NormalDir.Normalize();
				}
				Actor->BeginTouch( Test->Actor, HitLocation, NormalDir );
			}
		}
							
	Mark.Pop();

	// Ok the move.
	return 0;
}

/*-----------------------------------------------------------------------------
	SinglePointCheck.
-----------------------------------------------------------------------------*/

//
// Check for nearest hit.
// Return 1 if no hit, 0 if hit.
//
UBOOL ULevel::SinglePointCheck
(
	FCheckResult&	Hit,
	const FVector&	Location,
	const FVector&	Extent,
	ALevelInfo*		Level,
	UBOOL			bActors
)
{
	FMemMark Mark(GMem);
	FCheckResult* Hits = MultiPointCheck( GMem, Location, Extent, Level, bActors );
	if( !Hits )
	{
		Mark.Pop();
		return 1;
	}
	Hit = *Hits;
	for( Hits = Hits->GetNext(); Hits!=NULL; Hits = Hits->GetNext() )
		if( (Hits->Location-Location).SizeSquared() < (Hit.Location-Location).SizeSquared() )
			Hit = *Hits;
	Mark.Pop();
	return 0;
}

/*
  EncroachingWorldGeometry
  return true if Extent encroaches on level, terrain, or bWorldGeometry actors
*/
UBOOL ULevel::EncroachingWorldGeometry
(
	FCheckResult&	Hit,
	const FVector&	Location,
	const FVector&	Extent,
	ALevelInfo*		Level
)
{
	FMemMark Mark(GMem);
	FCheckResult* Hits = MultiPointCheck( GMem, Location, Extent, Level, true, true, true );
	if ( !Hits )
	{
		Mark.Pop();
		return false;
	}
	Hit = *Hits;
	Mark.Pop();
	return true;
}

/*-----------------------------------------------------------------------------
	MultiPointCheck.
-----------------------------------------------------------------------------*/

FCheckResult* ULevel::MultiPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, ALevelInfo* Level, UBOOL bActors, UBOOL bOnlyWorldGeometry, UBOOL bSingleResult )
{
	FCheckResult* Result=NULL;

	if(this->bShowPointChecks)
	{
		// Draw box showing extent of point check.
		LineBatcher->DrawWireBox(FBox(Location-Extent, Location+Extent), FColor(0, 128, 255));
	}

	// Check with level.
	if( Level )
	{
		FCheckResult TestHit(1.f);
		if( Level->GetLevel()->Model->PointCheck( TestHit, NULL, Location, Extent )==0 )
		{
			// Hit.
			TestHit.GetNext() = Result;
			Result            = new(Mem)FCheckResult(TestHit);
			Result->Actor     = Level;
			if ( bSingleResult )
			{
				return Result;
			}
		}
	}

	// Check with actors.
	if( bActors && Hash )
		Result = Hash->ActorPointCheck( Mem, Location, Extent,
							bOnlyWorldGeometry ? TRACE_World : TRACE_AllColliding,
							bSingleResult );

	return Result;
}

/*-----------------------------------------------------------------------------
	SingleLineCheck.
-----------------------------------------------------------------------------*/

//
// Trace a line and return the first hit actor (Actor->bWorldGeometry means hit the world geomtry).
//
UBOOL ULevel::SingleLineCheck
(
	FCheckResult&	Hit,
	AActor*			SourceActor,
	const FVector&	End,
	const FVector&	Start,
	DWORD           TraceFlags,
	const FVector&	Extent
)
{
	// Get list of hit actors.
	FMemMark Mark(GMem);

	TraceFlags = TraceFlags | TRACE_SingleResult;
	FCheckResult* FirstHit = MultiLineCheck
	(
		GMem,
		End,
		Start,
		Extent,
		(TraceFlags&TRACE_Level) ? GetLevelInfo() : NULL,
		TraceFlags,
		SourceActor
	);

	if( FirstHit )
	{
		Hit = *FirstHit;
	}
	else
	{
		Hit.Time = 1.f;
		Hit.Actor = NULL;
	}

	Mark.Pop();
	return FirstHit==NULL;
}

/*-----------------------------------------------------------------------------
	MultiLineCheck.
-----------------------------------------------------------------------------*/

FCheckResult* ULevel::MultiLineCheck
(
	FMemStack&		Mem,
	const FVector&	End,
	const FVector&	Start,
	const FVector&	Extent,
	ALevelInfo*		LevelInfo,
	DWORD			TraceFlags,
	AActor*			SourceActor
)
{
	INT NumHits=0;
	FCheckResult Hits[64];

	// Draw line that we are checking, and box showing extent at end of line, if non-zero
	if(this->bShowLineChecks && Extent.IsZero())
	{
		LineBatcher->DrawLine(Start, End, FColor(0, 255, 128));
		
	}
	else if(this->bShowExtentLineChecks && !Extent.IsZero())
	{
		LineBatcher->DrawLine(Start, End, FColor(0, 255, 255));
		LineBatcher->DrawWireBox(FBox(End-Extent, End+Extent), FColor(0, 255, 255));
	}

	FLOAT Dilation = 1.f;
	FVector NewEnd = End;

	// Check for collision with the level, and cull by the end point for speed.
	INT WorldNum = 0;
	if( (TraceFlags & TRACE_Level) && LevelInfo && LevelInfo->GetLevel()->Model->LineCheck( Hits[NumHits], NULL, End, Start, Extent, TraceFlags )==0 )
	{
		Hits[NumHits].Actor = LevelInfo;
		FLOAT Dist = (Hits[NumHits].Location - Start).Size();
		Hits[NumHits].Time *= Dilation;
		Dilation = ::Min(1.f, Hits[NumHits].Time * (Dist + 5)/(Dist+0.0001f));
		NewEnd = Start + (End - Start) * Dilation;
		WorldNum = NumHits;
		NumHits++;
	}
	if ( NumHits && (TraceFlags & TRACE_StopAtFirstHit) )
		goto SortList;

	// Check with actors.
	if( (TraceFlags & TRACE_Hash) && Hash )
	{
		for( FCheckResult* Link=Hash->ActorLineCheck( Mem, NewEnd, Start, Extent, TraceFlags, SourceActor ); Link && NumHits<ARRAY_COUNT(Hits); Link=Link->GetNext() )
		{
			Link->Time *= Dilation;
			Hits[NumHits++] = *Link;
		}
	}
	// Sort the list.
SortList:
	FCheckResult* Result = NULL;
	if( NumHits )
	{
		appQsort( Hits, NumHits, sizeof(Hits[0]), (QSORT_COMPARE)CompareHits );
		Result = new(Mem,NumHits)FCheckResult;
		for( INT i=0; i<NumHits; i++ )
		{
			Result[i]      = Hits[i];
			Result[i].Next = (i+1<NumHits) ? &Result[i+1] : NULL;
		}
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	ULevel zone functions.
-----------------------------------------------------------------------------*/

//
// Figure out which zone an actor is in, update the actor's iZone,
// and notify the actor of the zone change.  Skips the zone notification
// if the zone hasn't changed.
//

void AActor::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
		return;

	// If refreshing, init the actor's current zone.
	if( bForceRefresh )
	{
		// Init the actor's zone.
		Region = FPointRegion(Level);
	}

	// Find zone based on actor's location and see if it has changed.
	FPointRegion	NewRegion = GetLevel()->Model->PointRegion( Level, Location );

	if( NewRegion.Zone!=Region.Zone )
	{
		// Notify old zone info of actor leaving.
		if( !bTest )
		{
			Region.Zone->eventActorLeaving(this);
			eventZoneChange( NewRegion.Zone );
		}
		Region = NewRegion;
		if( !bTest )
			Region.Zone->eventActorEntered(this);
	}
	else Region = NewRegion;

	// update physics volume
	APhysicsVolume *NewVolume = Level->GetPhysicsVolume(Location,this,bCollideActors && !bTest && !bForceRefresh);
	if ( !bTest )
	{
		if ( NewVolume != PhysicsVolume )
		{
			if ( PhysicsVolume )
			{
				PhysicsVolume->eventActorLeavingVolume(this);
				eventPhysicsVolumeChange(NewVolume);
			}
			PhysicsVolume = NewVolume;
			PhysicsVolume->eventActorEnteredVolume(this);
		}
	}
	else
		PhysicsVolume = NewVolume;
	checkSlow(Region.Zone!=NULL);
}

void APhysicsVolume::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
		return;

	// If refreshing, init the actor's current zone.
	if( bForceRefresh )
	{
		// Init the actor's zone.
		Region = FPointRegion(Level);
	}

	// Find zone based on actor's location and see if it has changed.
	FPointRegion	NewRegion = GetLevel()->Model->PointRegion( Level, Location );

	if( NewRegion.Zone!=Region.Zone )
	{
		// Notify old zone info of actor leaving.
		if( !bTest )
		{
			Region.Zone->eventActorLeaving(this);
			eventZoneChange( NewRegion.Zone );
		}
		Region = NewRegion;
		if( !bTest )
			Region.Zone->eventActorEntered(this);
	}
	else Region = NewRegion;

	PhysicsVolume = this;
	checkSlow(Region.Zone!=NULL);
}

void APawn::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
		return;

	// If refreshing, init the actor's current zone.
	if( bForceRefresh )
	{
		// Init the actor's zone.
		Region = FPointRegion(Level);
	}

	// Find zone based on actor's location and see if it has changed.
	FPointRegion NewRegion = GetLevel()->Model->PointRegion( Level, Location );

	if( NewRegion.Zone!=Region.Zone )
	{
		// Notify old zone info of player leaving.
		if( !bTest )
		{
			Region.Zone->eventActorLeaving(this);
			eventZoneChange( NewRegion.Zone );
		}
		Region = NewRegion;
		if( !bTest )
			Region.Zone->eventActorEntered(this);
	}
	else Region = NewRegion;

	// update physics volume
	APhysicsVolume *NewVolume = Level->GetPhysicsVolume(Location,this,bCollideActors && !bTest && !bForceRefresh);
	APhysicsVolume *NewHeadVolume = Level->GetPhysicsVolume(Location + FVector(0,0,BaseEyeHeight),this,bCollideActors && !bTest && !bForceRefresh);
	if ( NewVolume != PhysicsVolume )
	{
		if ( !bTest )
		{
			if ( PhysicsVolume )
			{
				PhysicsVolume->eventPawnLeavingVolume(this);
				eventPhysicsVolumeChange(NewVolume);
			}
			if ( Controller )
				Controller->eventNotifyPhysicsVolumeChange( NewVolume );
		}
		PhysicsVolume = NewVolume;
		if ( !bTest )
			PhysicsVolume->eventPawnEnteredVolume(this);
	}
	if ( NewHeadVolume != HeadVolume )
	{
		if ( !bTest && (!Controller || !Controller->eventNotifyHeadVolumeChange(NewHeadVolume)) )
			eventHeadVolumeChange(NewHeadVolume);
		HeadVolume = NewHeadVolume;
	}
	checkSlow(PhysicsVolume);
	checkSlow(Region.Zone!=NULL);
}

void ALevelInfo::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	if( bDeleteMe )
		return;

	// handle levelinfo specially.
	Region = FPointRegion( Level );
}

// init actor volumes
void AVolume::SetVolumes(const TArray<AVolume*>& Volumes)
{
}
void ALevelInfo::SetVolumes(const TArray<AVolume*>& Volumes)
{
}
void AActor::SetVolumes(const TArray<AVolume*>& Volumes)
{
	for( INT i=0; i<Volumes.Num(); i++ )
	{
		AVolume*		V = Volumes(i);
		APhysicsVolume*	P = Cast<APhysicsVolume>(V);
		if( (bCollideActors && V->bCollideActors || P) && V->Encompasses(Location) )
		{
			if( bCollideActors && V->bCollideActors )
			{
				V->Touching.AddItem(this);
				Touching.AddItem(V);
			}
			if( P && (P->Priority > PhysicsVolume->Priority) )
			{
				PhysicsVolume = P;
			}
		}
	}

}
void AActor::SetVolumes()
{
	for( INT i=0; i<GetLevel()->Actors.Num(); i++ )
	{
		AVolume*		V = Cast<AVolume>(GetLevel()->Actors(i));
		APhysicsVolume*	P = Cast<APhysicsVolume>(V);
		if( V && (bCollideActors && V->bCollideActors || P) && V->Encompasses(Location) )
		{
			if( bCollideActors && V->bCollideActors )
			{
				V->Touching.AddItem(this);
				Touching.AddItem(V);
			}
			if( P && (P->Priority > PhysicsVolume->Priority) )
			{
				PhysicsVolume = P;
			}
		}
	}
}

// Allow actors to initialize themselves on the C++ side
void AActor::PostBeginPlay()
{
}

void AVolume::SetVolumes()
{
}

void ALevelInfo::SetVolumes()
{
}

APhysicsVolume* ALevelInfo::GetDefaultPhysicsVolume()
{
	if ( !PhysicsVolume )
	{
		PhysicsVolume = Cast<APhysicsVolume>(GetLevel()->SpawnActor(ADefaultPhysicsVolume::StaticClass()));
		check(PhysicsVolume);
		PhysicsVolume->Priority = -1000000;
		PhysicsVolume->bNoDelete = true;
	}
	return PhysicsVolume;
}

APhysicsVolume* ALevelInfo::GetPhysicsVolume(FVector Loc, AActor* A, UBOOL bUseTouch)
{
	APhysicsVolume *NewVolume = Level->GetDefaultPhysicsVolume();

	if ( bUseTouch && A )
	{
		for ( INT i=0; i<A->Touching.Num(); i++ )
		{
			APhysicsVolume *V = Cast<APhysicsVolume>(A->Touching(i));
			if ( V && (V->Priority > NewVolume->Priority) && (V->Encompasses(Loc) || V->bPhysicsOnContact) )
				NewVolume = V;
		}
	}
	else if ( GetLevel()->Hash )
				{
		FMemMark Mark(GMem);

		for( FCheckResult* Link=GetLevel()->Hash->ActorPointCheck( GMem, Loc, FVector(0.f,0.f,0.f), TRACE_Volumes, 0); Link; Link=Link->GetNext() )
		{
			APhysicsVolume *V = Cast<APhysicsVolume>(Link->Actor);
			if ( V && (V->Priority > NewVolume->Priority) )
				NewVolume = V;
	}
		Mark.Pop();
	}
	return NewVolume;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

