/*=============================================================================
	UnScript.cpp: UnrealScript engine support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	UnrealScript execution and support code.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

void AActor::execDebugClock( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
}

void AActor::execDebugUnclock( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
}

//
// Generalize animation retrieval to work for both skeletal meshes (animation sits in Actor->SkelAnim->AnimSeqs) and
// classic meshes (Mesh->AnimSeqs) For backwards compatibility....
//

//
// Initialize execution.
//
void AActor::InitExecution()
{
	UObject::InitExecution();

	checkSlow(GetStateFrame());
	checkSlow(GetStateFrame()->Object==this);
	checkSlow(GetLevel()!=NULL);
	checkSlow(GetLevel()->Actors(0)!=NULL);
	checkSlow(GetLevel()->Actors(0)==Level);
	checkSlow(Level!=NULL);

}

/*-----------------------------------------------------------------------------
	Natives.
-----------------------------------------------------------------------------*/

//////////////////////
// Console Commands //
//////////////////////

void AActor::execConsoleCommand( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Command);
	P_GET_UBOOL_OPTX(bWriteToLog,0);
	P_FINISH;

	if( bWriteToLog )
	{
		GetLevel()->Engine->Exec( *Command, *GLog );
		*(FString*)Result = FString(TEXT(""));
	}
	else
	{
		FStringOutputDevice StrOut;
		GetLevel()->Engine->Exec( *Command, StrOut );
		*(FString*)Result = *StrOut;
	}
}

/////////////////////////////
// Log and error functions //
/////////////////////////////

void AActor::execError( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(S);
	P_FINISH;

	Stack.Log( *S );
	GetLevel()->DestroyActor( this );

}

//////////////////////////
// Clientside functions //
//////////////////////////

void APlayerController::execClientTravel( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(URL);
	P_GET_BYTE(TravelType);
	P_GET_UBOOL(bItems);
	P_FINISH;

	// Warn the client.
	eventPreClientTravel();

	// Do the travel.
	GEngine->SetClientTravel( *URL, bItems, (ETravelType)TravelType );

}

void APlayerController::execGetPlayerNetworkAddress( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if( Player && Player->IsA(UNetConnection::StaticClass()) )
		*(FString*)Result = Cast<UNetConnection>(Player)->LowLevelGetRemoteAddress();
	else
		*(FString*)Result = TEXT("");
}

void APlayerController::execGetServerNetworkAddress( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if( GetLevel() && GetLevel()->NetDriver && GetLevel()->NetDriver->ServerConnection )
		*(FString*)Result = GetLevel()->NetDriver->ServerConnection->LowLevelGetRemoteAddress();
	else
		*(FString*)Result = TEXT("");
}

void APlayerController::execCopyToClipboard( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Text);
	P_FINISH;
	appClipboardCopy(*Text);
}

void APlayerController::execPasteFromClipboard( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(Text);
	P_FINISH;
	*(FString*)Result = appClipboardPaste();
}

void ALevelInfo::execGetLocalURL( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(FString*)Result = GetLevel()->URL.String();
}

void ALevelInfo::execIsDemoBuild( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

#if DEMOVERSION
	*(UBOOL*)Result = 1;
#else
	*(UBOOL*)Result = 0;
#endif
}

/** 
 * Returns whether we are running on a console platform or on the PC.
 *
 * @return TRUE if we're on a console, FALSE if we're running on a PC
 */
void ALevelInfo::execIsConsoleBuild( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
#ifdef CONSOLE
	*(UBOOL*)Result = 1;
#else
	*(UBOOL*)Result = 0;
#endif
}

void ALevelInfo::execGetAddressURL( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(FString*)Result = FString::Printf( TEXT("%s:%i"), *GetLevel()->URL.Host, GetLevel()->URL.Port );
}

void ALevelInfo::execIsEntry( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(UBOOL*)Result = 0;

	if(!XLevel || !XLevel->Engine)
		return;

	UGameEngine* GameEngine = Cast<UGameEngine>(XLevel->Engine);

	if( GameEngine &&  ( GameEngine->GLevel == GameEngine->GEntry ) )
		*(UBOOL*)Result = 1;
}

void ALevelInfo::execGetLevelSequence(FFrame &Stack, RESULT_DECL)
{
	P_FINISH;
	*(USequence**)Result = XLevel->GameSequence;
}

////////////////////////////////
// Latent function initiators //
////////////////////////////////

void AActor::execSleep( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(Seconds);
	P_FINISH;

	GetStateFrame()->LatentAction = EPOLL_Sleep;
	LatentFloat  = Seconds;
}

///////////////////////////
// Slow function pollers //
///////////////////////////

void AActor::execPollSleep( FFrame& Stack, RESULT_DECL )
{
	FLOAT DeltaSeconds = *(FLOAT*)Result;
	if( (LatentFloat-=DeltaSeconds) < 0.5 * DeltaSeconds )
	{
		// Awaken.
		GetStateFrame()->LatentAction = 0;
	}
}
IMPLEMENT_FUNCTION( AActor, EPOLL_Sleep, execPollSleep );

///////////////
// Collision //
///////////////

void AActor::execSetCollision( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL_OPTX(NewCollideActors,bCollideActors);
	P_GET_UBOOL_OPTX(NewBlockActors,  bBlockActors  );
	P_FINISH;

	SetCollision( NewCollideActors, NewBlockActors );

}

void AActor::execOnlyAffectPawns( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bNewAffectPawns);
	P_FINISH;

	if( bCollideActors && (bNewAffectPawns != bOnlyAffectPawns))
	{
		SetCollision(false, bBlockActors);
		bOnlyAffectPawns = bNewAffectPawns;
		SetCollision(true, bBlockActors);
	}

}

void AActor::execSetCollisionSize( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(NewRadius);
	P_GET_FLOAT(NewHeight);
	P_FINISH;

	SetCollisionSize( NewRadius, NewHeight );

	// Return boolean success or failure.
	*(DWORD*)Result = 1;

}

void AActor::execSetBase( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(AActor,NewBase);
	P_GET_VECTOR_OPTX(NewFloor, FVector(0,0,1) );
	P_GET_OBJECT_OPTX(USkeletalMeshComponent,SkelComp, NULL);
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	SetBase( NewBase, NewFloor, 1, SkelComp, BoneName );
}

void AActor::execSetDrawScale( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(NewScale);
	P_FINISH;

	SetDrawScale(NewScale);

}

void AActor::execSetDrawScale3D( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewScale3D);
	P_FINISH;

	SetDrawScale3D(NewScale3D);

}

void AActor::execSetPrePivot( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewPrePivot);
	P_FINISH;

	SetPrePivot(PrePivot);

}

///////////
// Audio //
///////////

void AActor::execCreateAudioComponent( FFrame& Stack, RESULT_DECL )
{
	// Get parameters.
	P_GET_OBJECT(USoundCue,SoundCue);
	P_GET_UBOOL_OPTX(bPlay,0);
	P_FINISH;

	*(UAudioComponent**)Result = UAudioDevice::CreateComponent( SoundCue, GetLevel(), this, bPlay );

}

//////////////
// Movement //
//////////////

void AActor::execMove( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Delta);
	P_FINISH;

	FCheckResult Hit(1.0f);
	*(DWORD*)Result = GetLevel()->MoveActor( this, Delta, Rotation, Hit );

}

void AActor::execSetLocation( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewLocation);
	P_FINISH;

	*(DWORD*)Result = GetLevel()->FarMoveActor( this, NewLocation );
}

void AActor::execSetRelativeLocation( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewLocation);
	P_FINISH;

	if ( Base )
	{
		if(bHardAttach && !bBlockActors)
		{
			// Remember new relative position
			RelativeLocation = NewLocation;

			// Using this new relative transform, calc new wold transform and update current position to match.
			FMatrix HardRelMatrix = FMatrix(RelativeLocation, RelativeRotation);
			FMatrix BaseTM = FMatrix(Base->Location, Base->Rotation);
			FMatrix NewWorldTM = HardRelMatrix * BaseTM;

			NewLocation = NewWorldTM.GetOrigin();
			*(DWORD*)Result = GetLevel()->FarMoveActor( this, NewLocation,false,false,true );
		}
		else
		{
			NewLocation = Base->Location + FRotationMatrix(Base->Rotation).TransformFVector(NewLocation);
			*(DWORD*)Result = GetLevel()->FarMoveActor( this, NewLocation,false,false,true );
			if ( Base )
				RelativeLocation = Location - Base->Location;
		}
	}

}

void AActor::execSetRotation( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	FCheckResult Hit(1.0f);
	*(DWORD*)Result = GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
}

void AActor::execSetRelativeRotation( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	if ( Base )
	{
		if(bHardAttach && !bBlockActors)
		{
			// We make a new HardRelMatrix using the new rotation and the existing position.
			FMatrix HardRelMatrix = FMatrix(RelativeLocation, NewRotation);
			RelativeLocation = HardRelMatrix.GetOrigin();
			RelativeRotation = HardRelMatrix.Rotator();

			// Work out what the new chile rotation is
			FMatrix BaseTM = FMatrix(Base->Location, Base->Rotation);
			FMatrix NewWorldTM = HardRelMatrix * BaseTM;
			NewRotation = NewWorldTM.Rotator();
		}
		else
		{
			NewRotation = (FRotationMatrix( NewRotation ) * FRotationMatrix( Base->Rotation )).Rotator();
		}
	}
	FCheckResult Hit(1.0f);
	*(DWORD*)Result = GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
}


///////////////
// Relations //
///////////////

void AActor::execSetOwner( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(NewOwner);
	P_FINISH;

	SetOwner( NewOwner );

}

//////////////////
// Line tracing //
//////////////////

void AActor::execTrace( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_GET_UBOOL_OPTX(bTraceActors,bCollideActors);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_GET_STRUCT_REF(FTraceHitInfo, HitInfo);
	P_GET_UBOOL_OPTX(bTraceWater,false);
	P_FINISH;

	// Trace the line.
	FCheckResult Hit(1.0f);
	DWORD TraceFlags;
	if( bTraceActors )
		TraceFlags = TRACE_ProjTargets;
	else
		TraceFlags = TRACE_World;

	if( HitInfo )
		TraceFlags |= TRACE_Material;

	if ( bTraceWater )
		TraceFlags |= TRACE_Water;

	AActor *TraceActor = this;
	AController *C = Cast<AController>(this);
	if ( C && C->Pawn )
		TraceActor = C->Pawn;
	GetLevel()->SingleLineCheck( Hit, TraceActor, TraceEnd, TraceStart, TraceFlags, TraceExtent );

	*(AActor**)Result = Hit.Actor;
	*HitLocation      = Hit.Location;
	*HitNormal        = Hit.Normal;

	if(HitInfo)
	{
		HitInfo->Material = Hit.Material ? Hit.Material->GetMaterial() : NULL;
		HitInfo->Item = Hit.Item;
		HitInfo->BoneName = Hit.BoneName;
		HitInfo->HitComponent = Hit.Component;
	}
}

/** Run a line check against just this PrimitiveComponent. Return TRUE if we hit. */
void AActor::execTraceComponent( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_OBJECT(UPrimitiveComponent, InComponent);
	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_GET_STRUCT_REF(FTraceHitInfo, HitInfo);
	P_FINISH;

	FCheckResult Hit(1.0f);
	UBOOL bNoHit = InComponent->LineCheck(Hit, TraceEnd, TraceStart, TraceExtent, TRACE_AllBlocking);

	*HitLocation      = Hit.Location;
	*HitNormal        = Hit.Normal;

	if(HitInfo)
	{
		HitInfo->Material = Hit.Material ? Hit.Material->GetMaterial() : NULL;
		HitInfo->Item = Hit.Item;
		HitInfo->BoneName = Hit.BoneName;
		HitInfo->HitComponent = Hit.Component;
	}

	*(DWORD*)Result = !bNoHit;
}

void AActor::execFastTrace( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_FINISH;

	// Trace the line.
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TRACE_World|TRACE_StopAtFirstHit );

	*(DWORD*)Result = !Hit.Actor;
}

///////////////////////
// Spawn and Destroy //
///////////////////////

void AActor::execSpawn( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,SpawnClass);
	P_GET_OBJECT_OPTX(AActor,SpawnOwner,NULL);
	P_GET_NAME_OPTX(SpawnName,NAME_None);
	P_GET_VECTOR_OPTX(SpawnLocation,Location);
	P_GET_ROTATOR_OPTX(SpawnRotation,Rotation);
	P_FINISH;

	// Spawn and return actor.
	AActor* Spawned = SpawnClass ? GetLevel()->SpawnActor
	(
		SpawnClass,
		NAME_None,
		SpawnLocation,
		SpawnRotation,
		NULL,
		0,
		0,
		SpawnOwner,
		Instigator
	) : NULL;
	if( Spawned && (SpawnName != NAME_None) )
		Spawned->Tag = SpawnName;
	*(AActor**)Result = Spawned;
}

void AActor::execDestroy( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	// scripting support, fire off destroyed event
	//TODO:	
	*(DWORD*)Result = GetLevel()->DestroyActor( this );
}

////////////
// Timing //
////////////

void AActor::execSetTimer( FFrame& Stack, RESULT_DECL )
{
	// read function parms off the stack
	P_GET_FLOAT(inRate);
	P_GET_UBOOL_OPTX(inbLoop,0);
	P_GET_NAME_OPTX(inFuncName,NAME_Timer);
	P_FINISH;
	// search for an existing timer first
	UBOOL bFoundEntry = 0;
	for (INT idx = 0; idx < Timers.Num() && !bFoundEntry; idx++)
	{
		// if matching function,
		if (Timers(idx).FuncName == inFuncName)
		{
			bFoundEntry = 1;
			// if given a 0.f rate, disable the timer
			if (inRate == 0.f)
			{
				Timers.Remove(idx--,1);
			}
			// otherwise update with new rate
			else
			{
				Timers(idx).bLoop = inbLoop;
				Timers(idx).Rate = inRate;
				Timers(idx).Count = 0.f;
			}
		}
	}
	// if no timer was found, add a new one
	if (!bFoundEntry)
	{
#ifdef _DEBUG
		// search for the function and assert that it exists
		UFunction *newFunc = FindFunctionChecked(inFuncName);
		newFunc = NULL;
#endif
		INT idx = Timers.AddZeroed();
		Timers(idx).FuncName = inFuncName;
		Timers(idx).bLoop = inbLoop;
		Timers(idx).Rate = inRate;
		Timers(idx).Count = 0.f;
	}
}

void AActor::execClearTimer(FFrame &Stack, RESULT_DECL)
{
	P_GET_NAME_OPTX(inFuncName,NAME_Timer);
	P_FINISH;
	for (INT idx = 0; idx < Timers.Num(); idx++)
	{
		// if matching function,
		if (Timers(idx).FuncName == inFuncName)
		{
			Timers.Remove(idx--,1);
		}
	}
}

void AActor::execIsTimerActive(FFrame &Stack, RESULT_DECL)
{
	P_GET_NAME_OPTX(inFuncName,NAME_Timer);
	P_FINISH;
	*(UBOOL*)Result = 0;
	for (INT idx = 0; idx < Timers.Num(); idx++)
	{
		if (Timers(idx).FuncName == inFuncName)
		{
			*(UBOOL*)Result = 1;
			break;
		}
	}
}

void AActor::execGetTimerCount(FFrame &Stack, RESULT_DECL)
{
	P_GET_NAME_OPTX(inFuncName,NAME_Timer);
	P_FINISH;
	*(FLOAT*)Result = -1.f;
	for (INT idx = 0; idx < Timers.Num(); idx++)
	{
		if (Timers(idx).FuncName == inFuncName)
		{
			*(FLOAT*)Result = Timers(idx).Count;
			break;
		}
	}
}

/*-----------------------------------------------------------------------------
	Native iterator functions.
-----------------------------------------------------------------------------*/

void AActor::execAllActors( FFrame& Stack, RESULT_DECL )
{
	// Get the parms.
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_NAME_OPTX(TagName,NAME_None);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && 
                !TestActor->bDeleteMe &&
                TestActor->IsA(BaseClass) && 
                (TagName==NAME_None || TestActor->Tag==TagName) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execDynamicActors( FFrame& Stack, RESULT_DECL )
{
	// Get the parms.
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_NAME_OPTX(TagName,NAME_None);
	P_FINISH;
	
	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor = GetLevel()->iFirstDynamicActor;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && 
                !TestActor->bDeleteMe &&
                TestActor->IsA(BaseClass) && 
                (TagName==NAME_None || TestActor->Tag==TagName) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execChildActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && 
                !TestActor->bDeleteMe &&
                TestActor->IsA(BaseClass) && 
                TestActor->IsOwnedBy( this ) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execBasedActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iBased=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		for( iBased; iBased<Attached.Num() && *OutActor==NULL; iBased++ )
		{
			AActor* TestActor = Attached(iBased);
			if(	TestActor &&
                !TestActor->bDeleteMe &&
                TestActor->IsA(BaseClass))
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execTouchingActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iTouching=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		for( iTouching; iTouching<Touching.Num() && *OutActor==NULL; iTouching++ )
		{
			AActor* TestActor = Touching(iTouching);
			if(	TestActor &&
                !TestActor->bDeleteMe &&
                TestActor->IsA(BaseClass))
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execTraceActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(End);
	P_GET_VECTOR_OPTX(Start,Location);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_FINISH;

	FMemMark Mark(GMem);
	BaseClass         = BaseClass ? BaseClass : AActor::StaticClass();
	FCheckResult* Hit = GetLevel()->MultiLineCheck( GMem, End, Start, TraceExtent, Level, TRACE_AllColliding, this );

	PRE_ITERATOR;
		if( Hit )
		{
			if ( Hit->Actor && 
                 !Hit->Actor->bDeleteMe &&
                 Hit->Actor->IsA(BaseClass))
			{
				*OutActor    = Hit->Actor;
				*HitLocation = Hit->Location;
				*HitNormal   = Hit->Normal;
			}
			Hit = Hit->GetNext();
		}
		else
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			*OutActor = NULL;
			break;
		}
	POST_ITERATOR;
	Mark.Pop();

}

void AActor::execVisibleActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT_OPTX(Radius,0.0f);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;
	FCheckResult Hit(1.f);

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
			&& !TestActor->bHidden
            &&  !TestActor->bDeleteMe
			&&	TestActor->IsA(BaseClass)
			&&	(Radius==0.0f || (TestActor->Location-TraceLocation).SizeSquared() < Square(Radius)) )
			{
				GetLevel()->SingleLineCheck( Hit, this, TestActor->Location, TraceLocation, TRACE_World|TRACE_StopAtFirstHit );
				if ( !Hit.Actor || (Hit.Actor == TestActor) )
					*OutActor = TestActor;
			}
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execVisibleCollidingActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_GET_UBOOL_OPTX(bIgnoreHidden, 0);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	FMemMark Mark(GMem);
	FCheckResult* Link = GetLevel()->Hash->ActorRadiusCheck( GMem, TraceLocation, Radius );
	
	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		FCheckResult Hit(1.f);

		if ( Link )
		{
			while ( Link )
			{
				if( !Link->Actor ||  
					!Link->Actor->bCollideActors ||	
					Link->Actor->bDeleteMe ||
					!Link->Actor->IsA(BaseClass) ||
					(bIgnoreHidden && Link->Actor->bHidden) )
				{
					Link = Link->GetNext();
				}
				else
				{
					GetLevel()->SingleLineCheck( Hit, this, Link->Actor->Location, TraceLocation, TRACE_World );
					if ( Hit.Actor && (Hit.Actor != Link->Actor) )
					{
						Link = Link->GetNext();
					}
					else
					{
						break;
					}
				}
			}

			if ( Link )
			{
				*OutActor = Link->Actor;
				Link = Link->GetNext();
			}
		}
		if ( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	Mark.Pop();
}

void AActor::execCollidingActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	FMemMark Mark(GMem);
	FCheckResult* Link=GetLevel()->Hash->ActorRadiusCheck( GMem, TraceLocation, Radius );
	
	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		if ( Link )
		{
			while ( Link )
			{
				if( !Link->Actor ||
					!Link->Actor->bCollideActors ||	
					Link->Actor->bDeleteMe ||
					!Link->Actor->IsA(BaseClass) )
				{
					Link = Link->GetNext();
				}
				else
					break;
			}

			if ( Link )
			{
				*OutActor = Link->Actor;
				Link=Link->GetNext();
			}
		}
		if ( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	Mark.Pop();
}

void AActor::execOverlappingActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_GET_UBOOL_OPTX(bIgnoreHidden, 0);
	P_FINISH;

	FBox SphereBox( TraceLocation - FVector(Radius), TraceLocation + FVector(Radius) );
	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	FMemMark Mark(GMem);
	FCheckResult* Link = GetLevel()->Hash->ActorOverlapCheck(GMem, Owner, SphereBox, false);

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		if ( Link )
		{
			while ( Link )
			{
				if( !Link->Actor
                    || Link->Actor->bDeleteMe
					|| !Link->Actor->IsA(BaseClass)
					|| (bIgnoreHidden && Link->Actor->bHidden) )
				{
					Link = Link->GetNext();
				}
				else
				{
					break;
				}
			}

			if ( Link )
			{
				*OutActor = Link->Actor;
				Link=Link->GetNext();
			}
		}
		if ( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	Mark.Pop();
}

/* execInventoryActors
	Iterator for InventoryManager
	Note: Watch out for Iterator being used to remove Inventory items!
*/
void AInventoryManager::execInventoryActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AInventory::StaticClass();
	AInventory	*InvItem;
	InvItem = InventoryChain;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		INT InventoryCount = 0;
		while ( InvItem )
		{	
			if ( InvItem->IsA(BaseClass) )
			{
				*OutActor	= InvItem;
				InvItem		= InvItem->Inventory; // Jump to next InvItem in case OutActor is removed from inventory, for next iteration
				break;
			}
			// limit inventory checked, since temporary loops in linked list may sometimes be created on network clients while link pointers are being replicated
			InventoryCount++;
			if ( InventoryCount > 100 )
				break;
			InvItem	= InvItem->Inventory;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

/**
 iterator execLocalPlayerControllers()
 returns all locally rendered/controlled player controllers (typically 1 per client, unless split screen)
*/
void AActor::execLocalPlayerControllers( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	INT iPlayers=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		for( iPlayers; iPlayers<GEngine->Players.Num() && *OutActor==NULL; iPlayers++ )
		{
			if ( GEngine->Players(iPlayers) )
				*OutActor = GEngine->Players(iPlayers)->Actor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

void AActor::execContainsPoint( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Spot);
	P_FINISH;

	UBOOL HaveHit = 0;
	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)this->Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(this->Components(ComponentIndex));

		if(primComp && primComp->ShouldCollide())
		{
			FCheckResult Hit(0);
			HaveHit = ( primComp->PointCheck( Hit, Spot, FVector(0,0,0) ) == 0 );
			if(HaveHit)
			{
				*(DWORD*)Result = 1;
				return;
			}
		}
	}	

	*(DWORD*)Result = 0;

}

void AActor::execTouchingActor( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(A);
	P_FINISH;

	*(DWORD*)Result = this->IsOverlapping(A);

}

void AActor::execGetComponentsBoundingBox( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT_REF(FBox, ActorBox);
	P_FINISH;

	*ActorBox = GetComponentsBoundingBox();

}

void AActor::execGetBoundingCylinder( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT_REF(CollisionRadius);
	P_GET_FLOAT_REF(CollisionHeight);
	P_FINISH;

	this->GetBoundingCylinder(*CollisionRadius, *CollisionHeight);

}

void AZoneInfo::execZoneActors( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
            &&  !TestActor->bDeleteMe
			&&	TestActor->IsA(BaseClass)
			&&	TestActor->IsInZone(this) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

}

/*-----------------------------------------------------------------------------
	Script processing function.
-----------------------------------------------------------------------------*/

void AActor::ExecutingBadStateCode(FFrame& Stack) 
{
	debugf( NAME_Critical, TEXT("%s - Unknown code token %02X - attempt to recover by setting bBadStateCode for actors"), GetName(), Stack.Code[-1] );
	debugf(TEXT("Problem is in %s possibly because of state change to %s during execution of subexpression"), GetFullName(), GetStateFrame()->StateNode->GetFullName());
	bBadStateCode = true;
}


//
// Execute the state code of the actor.
//
void AActor::ProcessState( FLOAT DeltaSeconds )
{
	if
	(	GetStateFrame()
	&&	GetStateFrame()->Code
	&&	(Role>=ROLE_Authority || (GetStateFrame()->StateNode->StateFlags & STATE_Simulated))
	&&	!IsPendingKill() )
	{
		UState* StartStateNode = GetStateFrame()->StateNode;
		UState* OldStateNode = GetStateFrame()->StateNode;

		// If a latent action is in progress, update it.
		if( GetStateFrame()->LatentAction )
			(this->*GNatives[GetStateFrame()->LatentAction])( *GetStateFrame(), (BYTE*)&DeltaSeconds );

		// Execute code.
		INT NumStates=0;
		INT NumCode=0;
		while( !bDeleteMe && GetStateFrame()->Code && !GetStateFrame()->LatentAction && !bBadStateCode )
		{
			BYTE Buffer[MAX_CONST_SIZE];
			GetStateFrame()->Step( this, Buffer );
			if ( NumCode++ > 1000000 )
			{
					bBadStateCode = true;
					GetStateFrame()->Code = NULL;
					debugf(NAME_Critical, TEXT("%s Infinite loop in state code - possibly because of state change to %s during execution of subexpression"), GetFullName(), GetStateFrame()->StateNode->GetFullName());
			} 
			if ( !bBadStateCode )
			{
				if( GetStateFrame()->StateNode!=OldStateNode )
				{
						StartStateNode = OldStateNode;
					OldStateNode = GetStateFrame()->StateNode;
					if( ++NumStates > 4 )
					{
						//GetStateFrame().Logf( "Pause going from %s to %s", xx, yy );
						break;
					}
				}
			}
			else
			{
				// Actor can try to recover from state code stack getting screwed up by state change during state code execution
				eventRecoverFromBadStateCode();
				break;
			}
		}
	}
}

//
// Internal RPC calling.
//
static inline void InternalProcessRemoteFunction
(
	AActor*			Actor,
	UNetConnection*	Connection,
	UFunction*		Function,
	void*			Parms,
	FFrame*			Stack,
	UBOOL			IsServer
)
{
	// Make sure this function exists for both parties.
	FClassNetCache* ClassCache = Connection->PackageMap->GetClassNetCache( Actor->GetClass() );
	if( !ClassCache )
		return;
	FFieldNetCache* FieldCache = ClassCache->GetFromField( Function );
	if( !FieldCache )
		return;

	// Get the actor channel.
	UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
	if( !Ch )
	{
		if( IsServer )
			Ch = (UActorChannel *)Connection->CreateChannel( CHTYPE_Actor, 1 );
		if( !Ch )
			return;
		if( IsServer )
			Ch->SetChannelActor( Actor );
	}

	// Make sure initial channel-opening replication has taken place.
	if( Ch->OpenPacketId==INDEX_NONE )
	{
		if( !IsServer )
			return;
		Ch->ReplicateActor();
	}

	// Form the RPC preamble.
	FOutBunch Bunch( Ch, 0 );
	//debugf(TEXT("   Call %s"),Function->GetFullName());
	Bunch.WriteInt( FieldCache->FieldNetIndex, ClassCache->GetMaxIndex() );

	// Form the RPC parameters.
	if( Stack )
	{
		appMemzero( Parms, Function->ParmsSize );
        for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
			Stack->Step( Stack->Object, (BYTE*)Parms + It->Offset );
		checkSlow(*Stack->Code==EX_EndFunctionParms);
	}
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
	{
		if( Connection->PackageMap->ObjectToIndex(*It)!=INDEX_NONE )
		{
			UBOOL Send = 1;
			if( !Cast<UBoolProperty>(*It,CLASS_IsAUBoolProperty) )
			{
				Send = !It->Matches(Parms,NULL,0);
				Bunch.WriteBit( Send );
			}
			if( Send )
				It->NetSerializeItem( Bunch, Connection->PackageMap, (BYTE*)Parms + It->Offset );
		}
	}

	// Reliability.
	//warning: RPC's might overflow, preventing reliable functions from getting thorough.
	if( Function->FunctionFlags & FUNC_NetReliable )
		Bunch.bReliable = 1;

	// Send the bunch.
	if( Bunch.IsError() )
		debugf( NAME_DevNetTraffic, TEXT("RPC bunch overflowed") );
	else
	if( Ch->Closing )
		debugf( NAME_DevNetTraffic, TEXT("RPC bunch on closing channel") );
	else
	{
//		debugf( NAME_DevNetTraffic, TEXT("      Sent RPC: %s::%s"), Actor->GetName(), Function->GetName() );
		Ch->SendBunch( &Bunch, 1 );
	}

}

//
// Return whether a function should be executed remotely.
//
UBOOL AActor::ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	// Quick reject.
	if( (Function->FunctionFlags & FUNC_Static) || bDeleteMe )
		return 0;
	UBOOL Absorb = (Role<=ROLE_SimulatedProxy) && !(Function->FunctionFlags & (FUNC_Simulated | FUNC_Native));
	if( Level->NetMode==NM_Standalone )
		return 0;
	if( !(Function->FunctionFlags & FUNC_Net) )
		return Absorb;

	// Check if the actor can potentially call remote functions.
	// FIXME - cleanup finding top - or make function
    APlayerController* Top = GetTopPlayerController();//Cast<APlayerController>(GetTopOwner());
	UNetConnection* ClientConnection = NULL;
	if
	(	(Role==ROLE_Authority)
	&&	(Top==NULL || (ClientConnection=Cast<UNetConnection>(Top->Player))==NULL) )
		return Absorb;

	// See if UnrealScript replication condition is met.
	while( Function->GetSuperFunction() )
		Function = Function->GetSuperFunction();
	UBOOL Val=0;
	FFrame( this, Function->GetOwnerClass(), Function->RepOffset, NULL ).Step( this, &Val );
	if( !Val )
		return Absorb;

	// Get the connection.
	UBOOL           IsServer   = Level->NetMode==NM_DedicatedServer || Level->NetMode==NM_ListenServer;
	UNetConnection* Connection = IsServer ? ClientConnection : GetLevel()->NetDriver->ServerConnection;
	if ( !Connection )
		return 1;

	// If saturated and function is unimportant, skip it.
	if( !(Function->FunctionFlags & FUNC_NetReliable) && !Connection->IsNetReady(0) )
		return 1;

	// Send function data to remote.
	InternalProcessRemoteFunction( this, Connection, Function, Parms, Stack, IsServer );
	return 1;

}

/*-----------------------------------------------------------------------------
	GameInfo
-----------------------------------------------------------------------------*/

//
// Network
//
void AGameInfo::execGetNetworkNumber( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	*(FString*)Result = XLevel->NetDriver ? XLevel->NetDriver->LowLevelGetNetworkNumber() : FString(TEXT(""));

}

//
// Deathmessage parsing.
//
void AGameInfo::execParseKillMessage( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(KillerName);
	P_GET_STR(VictimName);
	P_GET_STR(KillMessage);
	P_FINISH;

	FString Message, Temp;
	INT Offset;

	Temp = KillMessage;

	Offset = Temp.InStr(TEXT("%k"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += KillerName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
		Temp = Message;
	}

	Offset = Temp.InStr(TEXT("%o"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += VictimName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
	}

	*(FString*)Result = Message;

}

// Color functions
#define P_GET_COLOR(var)            P_GET_STRUCT(FColor,var)

void AActor::execMultiply_ColorFloat( FFrame& Stack, RESULT_DECL )
{
	P_GET_COLOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	A.R = (BYTE) (A.R * B);
	A.G = (BYTE) (A.G * B);
	A.B = (BYTE) (A.B * B);
	*(FColor*)Result = A;

}	

void AActor::execMultiply_FloatColor( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT (A);
	P_GET_COLOR(B);
	P_FINISH;

	B.R = (BYTE) (B.R * A);
	B.G = (BYTE) (B.G * A);
	B.B = (BYTE) (B.B * A);
	*(FColor*)Result = B;

}	

void AActor::execAdd_ColorColor( FFrame& Stack, RESULT_DECL )
{
	P_GET_COLOR(A);
	P_GET_COLOR(B);
	P_FINISH;

	A.R = A.R + B.R;
	A.G = A.G + B.G;
	A.B = A.B + B.B;
	*(FColor*)Result = A;

}

void AActor::execSubtract_ColorColor( FFrame& Stack, RESULT_DECL )
{
	P_GET_COLOR(A);
	P_GET_COLOR(B);
	P_FINISH;

	A.R = A.R - B.R;
	A.G = A.G - B.G;
	A.B = A.B - B.B;
	*(FColor*)Result = A;

}

void AVolume::execEncompasses( FFrame& Stack, RESULT_DECL )
{
	P_GET_ACTOR(InActor);
	P_FINISH;

	*(DWORD*)Result = Encompasses(InActor->Location);
}

void AHUD::execDraw3DLine( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Start);
	P_GET_VECTOR(End);
	P_GET_COLOR(LineColor);
	P_FINISH;

	XLevel->LineBatcher->DrawLine(Start,End,LineColor);

}

//
//	AActor::execDrawDebugLine
//

void AActor::execDrawDebugLine(FFrame& Stack,RESULT_DECL)
{
	P_GET_VECTOR(LineStart);
	P_GET_VECTOR(LineEnd);
	P_GET_BYTE(R);
	P_GET_BYTE(G);
	P_GET_BYTE(B);
	P_FINISH;

	GetLevel()->LineBatcher->DrawLine(LineStart, LineEnd, FColor(R, G, B));
}

/** Draw a persistent line */
void AActor::execDrawPersistentDebugLine(FFrame& Stack,RESULT_DECL)
{
	P_GET_VECTOR(LineStart);
	P_GET_VECTOR(LineEnd);
	P_GET_BYTE(R);
	P_GET_BYTE(G);
	P_GET_BYTE(B);
	P_FINISH;

	GetLevel()->PersistentLineBatcher->DrawLine(LineStart, LineEnd, FColor(R, G, B));
}

/** Flush persistent lines */
void AActor::execFlushPersistentDebugLines(FFrame& Stack,RESULT_DECL)
{
	P_FINISH;
	GetLevel()->PersistentLineBatcher->BatchedLines.Empty();
}

/** Add a data point to a line on the global debug chart. */
void AActor::execChartData( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(DataName);
	P_GET_FLOAT(DataValue);
    P_FINISH;

	ChartData(DataName, DataValue);
}

/*---------------------------------------------------------------------------------------
	CD Key Validation: MasterServer -> GameServer -> Client -> GameServer -> MasterServer
---------------------------------------------------------------------------------------*/

//
// Request a CD Key challenge response from the client.
//
void APlayerController::execClientValidate(FFrame& Stack,RESULT_DECL)
{
	P_GET_STR( Challenge );
	P_FINISH;
//	eventServerValidationResponse( GetCDKeyHash(),  GetCDKeyResponse(*Challenge) );
}

//
// CD Key Challenge response received from the client.
//
void APlayerController::execServerValidationResponse(FFrame& Stack,RESULT_DECL)
{
	P_GET_STR( CDKeyHash );
	P_GET_STR( Response );
	P_FINISH;
	UNetConnection* Conn = Cast<UNetConnection>(Player);
	if( Conn )
	{
		// Store it in the connection.  The uplink will pick this up.
		Conn->CDKeyHash     = CDKeyHash;
		Conn->CDKeyResponse = Response;
	}
}

void AActor::execClock(FFrame& Stack,RESULT_DECL)
{
	P_GET_FLOAT_REF( Time );
	P_FINISH;
	clock(*Time);
}

void AActor::execUnClock(FFrame& Stack,RESULT_DECL)
{
	P_GET_FLOAT_REF( Time );
	P_FINISH;
	unclock(*Time);
	*Time = *Time * GSecondsPerCycle * 1000.f;
}

/* 
 // ======================================================================================
// AddComponent
// Add an ActorComponent to this Actor
// input:  NewComponent - ActorComponent to add
//         bDuplicate - whether to add the passed in ActorComponent (if false), or add a duplicate 
                        component of the same class
 // ======================================================================================
*/
void AActor::execAddComponent( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UActorComponent,NewComponent);
	P_GET_UBOOL(bDuplicate);
	P_FINISH;

	if ( bDuplicate )
	{
		UActorComponent* DuplicateComponent = ConstructObject<UActorComponent>(NewComponent->GetClass(),this);
		Components.AddItem(DuplicateComponent);
		*(UActorComponent**)Result = DuplicateComponent;
	}
	else
	{
		Components.AddItem(NewComponent);
		*(UActorComponent**)Result = NewComponent;
	}
	UpdateComponents();
}

/*
 // ======================================================================================
 // ClampRotation
 // Clamps the given rotation accoring to the base and the limits passed
 // input:	Rotator RotToLimit - Rotation to limit
 //			Rotator Base	   - Base reference for limiting
 //			Rotator Limits	   - Component wise limits
 // output: out RotToLimit	   - Actual limited rotator
 //			return BOOL	true if within limits, false otherwise
 // effects:  Alters the passed in Rotation by reference and calls event OverRotated
 //			  when limit has been exceeded
 // notes:	  It cannot change const Rotations passed in (ie Actor::Rotation) so you need to
 //			  make a copy first
 // ======================================================================================
*/
void AActor::execClampRotation( FFrame& Stack, RESULT_DECL )
{
	P_GET_ROTATOR_REF(out_RotToLimit)
	P_GET_ROTATOR(rBase);
	P_GET_ROTATOR(rUpperLimits);
	P_GET_ROTATOR(rLowerLimits);
	P_FINISH;

	FRotator rOriginal;
	FRotator rAdjusted;

	rOriginal = *out_RotToLimit;
	rOriginal = rOriginal.Normalize();
	rAdjusted = rOriginal;

	rBase = rBase.Normalize();

	rAdjusted = (rAdjusted-rBase).Normalize();

	if( rUpperLimits.Pitch >= 0 ) 
	{
		rAdjusted.Pitch = Min( rAdjusted.Pitch,  rUpperLimits.Pitch	);
	}
	if( rLowerLimits.Pitch >= 0 ) 
	{
		rAdjusted.Pitch = Max( rAdjusted.Pitch, -rLowerLimits.Pitch	);
	}
		
	if( rUpperLimits.Yaw >= 0 )
	{
		rAdjusted.Yaw   = Min( rAdjusted.Yaw,    rUpperLimits.Yaw	);
	}
	if( rLowerLimits.Yaw >= 0 )
	{
		rAdjusted.Yaw   = Max( rAdjusted.Yaw,   -rLowerLimits.Yaw	);
	}
		
	if( rUpperLimits.Roll >= 0 )
	{
		rAdjusted.Roll  = Min( rAdjusted.Roll,   rUpperLimits.Roll	);
	}
	if( rLowerLimits.Roll >= 0 )
	{
		rAdjusted.Roll  = Max( rAdjusted.Roll,  -rLowerLimits.Roll	);
	}

	rAdjusted = (rAdjusted + rBase).Normalize();

	*out_RotToLimit = rAdjusted;
	if( *out_RotToLimit != rOriginal ) 
	{
		eventOverRotated( rOriginal, *out_RotToLimit );
		*(UBOOL*)Result = 0;
	}
	else 
	{
		*(UBOOL*)Result = 1;
	}
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

