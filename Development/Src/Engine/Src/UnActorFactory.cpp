/*=============================================================================
	UnActorFactory.cpp: 
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAIClasses.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"

IMPLEMENT_CLASS(UActorFactory);
IMPLEMENT_CLASS(UActorFactoryStaticMesh);
IMPLEMENT_CLASS(UActorFactoryLight);
IMPLEMENT_CLASS(UActorFactoryPhysicsAsset);
IMPLEMENT_CLASS(UActorFactoryRigidBody);
IMPLEMENT_CLASS(UActorFactoryMover);
IMPLEMENT_CLASS(UActorFactoryEmitter);
IMPLEMENT_CLASS(UActorFactoryAI);
IMPLEMENT_CLASS(UActorFactoryVehicle);
IMPLEMENT_CLASS(UActorFactorySkeletalMesh);
IMPLEMENT_CLASS(UActorFactoryPlayerStart);
IMPLEMENT_CLASS(UActorFactoryRoute);

/*-----------------------------------------------------------------------------
	UActorFactory
-----------------------------------------------------------------------------*/

AActor* UActorFactory::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	check( NewActorClass );
	check( !(NewActorClass->ClassFlags & CLASS_Abstract) );

	check(Location);

	FRotator NewRotation;
	if(Rotation)
		NewRotation = *Rotation;
	else
		NewRotation = NewActorClass->GetDefaultActor()->Rotation;

	AActor* NewActor = Level->SpawnActor(NewActorClass, NAME_None, *Location, NewRotation);

	return NewActor;
}


/*-----------------------------------------------------------------------------
	UActorFactoryStaticMesh
-----------------------------------------------------------------------------*/

AActor* UActorFactoryStaticMesh::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	if(!StaticMesh)
		return NULL;

	AActor* NewActor = Super::CreateActor(Level, Location, Rotation);
	if(!NewActor)
		return NULL;

	AStaticMeshActor* NewSMActor = CastChecked<AStaticMeshActor>(NewActor);

	// Term Component
	NewSMActor->TermRBPhys();
	NewSMActor->ClearComponents();

	// Change properties
	NewSMActor->StaticMeshComponent->StaticMesh = StaticMesh;
	NewSMActor->DrawScale3D = DrawScale3D;

	// Init Component
	NewSMActor->UpdateComponents();
	NewSMActor->InitRBPhys();

	return NewSMActor;
}

UBOOL UActorFactoryStaticMesh::CanCreateActor() 
{ 
	if(StaticMesh)
		return true;
	else 
		return false;
}

void UActorFactoryStaticMesh::AutoFillFields()
{
	StaticMesh = GSelectionTools.GetTop<UStaticMesh>();
}

FString UActorFactoryStaticMesh::GetMenuName()
{
	if(StaticMesh)
		return FString::Printf( TEXT("%s: %s"), *MenuName, StaticMesh->GetFullName() );
	else
		return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryRigidBody
-----------------------------------------------------------------------------*/

AActor* UActorFactoryRigidBody::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	AActor* NewActor = Super::CreateActor(Level, Location, Rotation);
	if(NewActor)
	{
		AKActor* NewRB = CastChecked<AKActor>(NewActor);

		NewRB->StaticMeshComponent->SetRBLinearVelocity(InitialVelocity);

		if(bStartAwake)
		{
			NewRB->StaticMeshComponent->WakeRigidBody();
		}

		NewRB->bDamageAppliesImpulse = bDamageAppliesImpulse;
	}

	return NewActor;
}


UBOOL UActorFactoryRigidBody::CanCreateActor()
{
	if(StaticMesh && StaticMesh->BodySetup)
		return true;
	else 
		return false;
}

/*-----------------------------------------------------------------------------
	UActorFactoryEmitter
-----------------------------------------------------------------------------*/

AActor* UActorFactoryEmitter::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	if(!ParticleSystem)
		return NULL;

	AActor* NewActor = Super::CreateActor(Level, Location, Rotation);
	if(!NewActor)
		return NULL;

	AEmitter* NewEmitter = CastChecked<AEmitter>(NewActor);

	// Term Component
	NewEmitter->ClearComponents();

	// Change properties
	NewEmitter->ParticleSystemComponent->Template = ParticleSystem;

	// Init Component
	NewEmitter->UpdateComponents();

	return NewEmitter;
}

UBOOL UActorFactoryEmitter::CanCreateActor() 
{ 
	if(ParticleSystem)
		return true;
	else 
		return false;
}

void UActorFactoryEmitter::AutoFillFields()
{
	ParticleSystem = GSelectionTools.GetTop<UParticleSystem>();
}

FString UActorFactoryEmitter::GetMenuName()
{
	if(ParticleSystem)
		return FString::Printf( TEXT("%s: %s"), *MenuName, ParticleSystem->GetName() );
	else
		return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryPhysicsAsset
-----------------------------------------------------------------------------*/

AActor* UActorFactoryPhysicsAsset::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	if(!PhysicsAsset)
		return NULL;

	// If a specific mesh is supplied, use it. Otherwise, use default from PhysicsAsset.
	USkeletalMesh* UseSkelMesh = SkeletalMesh;
	if(!UseSkelMesh)
		UseSkelMesh = PhysicsAsset->DefaultSkelMesh;
	check(UseSkelMesh);

	AActor* NewActor = Super::CreateActor(Level, Location, Rotation);
	if(!NewActor)
		return NULL;

	AKAsset* NewAsset = CastChecked<AKAsset>(NewActor);

	// Term Component
	NewAsset->TermRBPhys();
	NewAsset->ClearComponents();

	// Change properties
	NewAsset->SkeletalMeshComponent->SkeletalMesh = UseSkelMesh;
	NewAsset->SkeletalMeshComponent->PhysicsAsset = PhysicsAsset;
	NewAsset->DrawScale3D = DrawScale3D;

	// Init Component
	NewAsset->UpdateComponents();
	NewAsset->InitRBPhys();

	// Call other functions
	NewAsset->SkeletalMeshComponent->SetRBLinearVelocity(InitialVelocity);

	if(bStartAwake)
	{
		NewAsset->SkeletalMeshComponent->WakeRigidBody();
	}

	NewAsset->bDamageAppliesImpulse = bDamageAppliesImpulse;
		
	return NewAsset;
}

UBOOL UActorFactoryPhysicsAsset::CanCreateActor() 
{ 
	if(PhysicsAsset)
		return true;
	else 
		return false;
}

void UActorFactoryPhysicsAsset::AutoFillFields()
{
	PhysicsAsset = GSelectionTools.GetTop<UPhysicsAsset>();
}

FString UActorFactoryPhysicsAsset::GetMenuName()
{
	if(PhysicsAsset)
		return FString::Printf( TEXT("%s: %s"), *MenuName, PhysicsAsset->GetName() );
	else
		return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAI
-----------------------------------------------------------------------------*/

AActor* UActorFactoryAI::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	// first create the pawn
	APawn* newPawn = NULL;
	if (PawnClass != NULL)
	{
		// check that the area around the location is clear of other characters
		UBOOL bHitPawn = false;

		FMemMark Mark(GMem);

		FCheckResult* checkResult = Level->MultiPointCheck(GMem, *Location, FVector(36,36,78), NULL, true, false, false);

		for (FCheckResult* testResult = checkResult; testResult != NULL && !bHitPawn; testResult = testResult->GetNext())
		{
			if (testResult->Actor != NULL &&
				testResult->Actor->IsA(APawn::StaticClass()))
			{
				bHitPawn = true;
			}
		}

		Mark.Pop();

		if (!bHitPawn)
		{
			NewActorClass = PawnClass;
			newPawn = (APawn*)Super::CreateActor(Level,Location,Rotation);
			if (newPawn != NULL)
			{
				// create any inventory
				for (INT idx = 0; idx < InventoryList.Num(); idx++)
				{
					newPawn->eventCreateInventory( InventoryList(idx) );
				}
				// create the controller
				if (ControllerClass != NULL)
				{
					NewActorClass = ControllerClass;
					AAIController* newController = (AAIController*)Super::CreateActor(Level,Location,Rotation);
					if (newController != NULL)
					{
						// handle the team assignment
						newController->eventSetTeam(TeamIndex);
						// force the controller to possess, etc
						newController->eventPossess(newPawn);
					}
				}
			}
		}
	}
	return newPawn;
}

/*-----------------------------------------------------------------------------
	UActorFactoryVehicle
-----------------------------------------------------------------------------*/

AActor* UActorFactoryVehicle::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	// first create the pawn
	AVehicle* NewVehicle = NULL;

	if( NewVehicle != NULL )
	{
		// check that the area around the location is clear of other characters
		UBOOL bHitPawn = false;

		FMemMark Mark(GMem);

		FCheckResult* checkResult = Level->MultiPointCheck(GMem, *Location, FVector(36,36,78), NULL, true, false, false);

		for (FCheckResult* testResult = checkResult; testResult != NULL && !bHitPawn; testResult = testResult->GetNext())
		{
			if (testResult->Actor != NULL &&
				testResult->Actor->IsA(APawn::StaticClass()))
			{
				bHitPawn = true;
			}
		}

		Mark.Pop();

		if( !bHitPawn )
		{
			NewActorClass = VehicleClass;
			NewVehicle = (AVehicle*)Super::CreateActor(Level, Location, Rotation);
		}
	}
	return NewVehicle;
}

/*-----------------------------------------------------------------------------
	UActorFactorySkeletalMesh
-----------------------------------------------------------------------------*/

AActor* UActorFactorySkeletalMesh::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	if(!SkeletalMesh)
		return NULL;

	AActor* NewActor = Super::CreateActor(Level, Location, Rotation);
	if(!NewActor)
		return NULL;

	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);

	// Term Component
	NewSMActor->ClearComponents();

	// Change properties
	NewSMActor->SkeletalMeshComponent->SkeletalMesh = SkeletalMesh;
	if(AnimSet)
	{
		NewSMActor->SkeletalMeshComponent->AnimSets.AddItem( AnimSet );
	}

	UAnimNodeSequence* SeqNode = CastChecked<UAnimNodeSequence>(NewSMActor->SkeletalMeshComponent->Animations);
	SeqNode->AnimSeqName = AnimSequenceName;

	// Init Component
	NewSMActor->UpdateComponents();

	return NewSMActor;
}

UBOOL UActorFactorySkeletalMesh::CanCreateActor() 
{ 
	if(SkeletalMesh)
		return true;
	else 
		return false;
}

void UActorFactorySkeletalMesh::AutoFillFields()
{
	SkeletalMesh = GSelectionTools.GetTop<USkeletalMesh>();
}

FString UActorFactorySkeletalMesh::GetMenuName()
{
	if(SkeletalMesh)
		return FString::Printf( TEXT("%s: %s"), *MenuName, SkeletalMesh->GetFullName() );
	else
		return MenuName;
}


/*-----------------------------------------------------------------------------
	UActorFactoryRoute
-----------------------------------------------------------------------------*/

/**
 * Looks at the list of selected objects and builds a list of navigation points in order
 * to be used as the initial contents of the route.
 */
AActor* UActorFactoryRoute::CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation)
{
	ARoute *route = NULL;
	// build a list of navigation points currently selected
	TArray<ANavigationPoint*> navList;
	for (INT idx = 0; idx < GSelectionTools.SelectedObjects.Num(); idx++)
	{
		debugf(TEXT("Selected object %s"),GSelectionTools.SelectedObjects(idx)->GetName());
		ANavigationPoint *nav = Cast<ANavigationPoint>(GSelectionTools.SelectedObjects(idx));
		if (nav != NULL)
		{
			navList.AddItem(nav);
		}
	}
	if (navList.Num() > 0)
	{
		// create the route
		NewActorClass = ARoute::StaticClass();
		route = Cast<ARoute>(Super::CreateActor(Level, Location, Rotation));
		if (route != NULL)
		{
			// and copy the move list
			route->MoveList.AddZeroed(navList.Num());
			for (INT idx = 0; idx < navList.Num(); idx++)
			{
				route->MoveList(idx) = navList(idx);
			}
		}
		else
		{
			debugf(TEXT("Failed to create new route"));
		}
	}
	else
	{
		debugf(TEXT("No selected navigation points"));
	}
	return route;
}

UBOOL UActorFactoryRoute::CanCreateActor()
{
	// build a list of navigation points currently selected
	TArray<ANavigationPoint*> navList;
	for (INT idx = 0; idx < GSelectionTools.SelectedObjects.Num(); idx++)
	{
		debugf(TEXT("Selected object %s"),GSelectionTools.SelectedObjects(idx)->GetName());
		ANavigationPoint *nav = Cast<ANavigationPoint>(GSelectionTools.SelectedObjects(idx));
		if (nav != NULL)
		{
			navList.AddItem(nav);
		}
	}
	return (navList.Num() > 0);
}
