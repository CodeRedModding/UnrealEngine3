/*=============================================================================
	UnActorFactory.cpp: 
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAIClasses.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "EngineDecalClasses.h"
#include "EngineMaterialClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "EngineFoliageClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineSequenceClasses.h"
#include "LensFlare.h"
#include "EngineFogVolumeClasses.h"

IMPLEMENT_CLASS(UActorFactory);
IMPLEMENT_CLASS(UActorFactoryActor);
IMPLEMENT_CLASS(UActorFactoryStaticMesh);
IMPLEMENT_CLASS(UActorFactoryFracturedStaticMesh);
IMPLEMENT_CLASS(UActorFactoryLight);
IMPLEMENT_CLASS(UActorFactoryDominantDirectionalLight);
IMPLEMENT_CLASS(UActorFactoryDominantDirectionalLightMovable);
IMPLEMENT_CLASS(UActorFactoryTrigger);
IMPLEMENT_CLASS(UActorFactoryPathNode);
IMPLEMENT_CLASS(UActorFactoryPylon);
IMPLEMENT_CLASS(UActorFactoryPhysicsAsset);
IMPLEMENT_CLASS(UActorFactoryRigidBody);
IMPLEMENT_CLASS(UActorFactoryMover);
IMPLEMENT_CLASS(UActorFactoryEmitter);
IMPLEMENT_CLASS(UActorFactoryAI);
IMPLEMENT_CLASS(UActorFactoryVehicle);
IMPLEMENT_CLASS(UActorFactorySkeletalMesh);
IMPLEMENT_CLASS(UActorFactoryPlayerStart);
IMPLEMENT_CLASS(UActorFactoryDynamicSM);
IMPLEMENT_CLASS(UActorFactoryAmbientSound);
IMPLEMENT_CLASS(UActorFactoryAmbientSoundMovable);
IMPLEMENT_CLASS(UActorFactoryAmbientSoundSimple);
IMPLEMENT_CLASS(UActorFactoryAmbientSoundNonLoop);
IMPLEMENT_CLASS(UActorFactoryAmbientSoundNonLoopingToggleable);
IMPLEMENT_CLASS(UActorFactoryAmbientSoundSimpleToggleable);
IMPLEMENT_CLASS(UActorFactoryDecal);
IMPLEMENT_CLASS(UActorFactoryDecalMovable);
IMPLEMENT_CLASS(UActorFactoryCoverLink);
IMPLEMENT_CLASS(UActorFactoryArchetype);
IMPLEMENT_CLASS(UActorFactoryLensFlare);
IMPLEMENT_CLASS(UActorFactoryFogVolumeConstantDensityInfo);
IMPLEMENT_CLASS(UActorFactoryFogVolumeLinearHalfspaceDensityInfo);
IMPLEMENT_CLASS(UActorFactoryFogVolumeSphericalDensityInfo);
IMPLEMENT_CLASS(UActorFactoryInteractiveFoliage);
IMPLEMENT_CLASS(UActorFactoryApexDestructible);
IMPLEMENT_CLASS(UActorFactoryApexClothing);

/*-----------------------------------------------------------------------------
	UActorFactory
-----------------------------------------------------------------------------*/

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactory::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly )
	{
		// cant create the actor if no asset is assigned to this actor.
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoAssetAssigned");
	}

	return bCanCreate;
}

AActor* UActorFactory::GetDefaultActor()
{
	if ( NewActorClassName != TEXT("") )
	{
		debugf(TEXT("Loading ActorFactory Class %s"), *NewActorClassName);
		NewActorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *NewActorClassName, NULL, LOAD_NoWarn, NULL));
		NewActorClassName = TEXT("");
		if ( NewActorClass == NULL )
		{
			debugf(TEXT("ActorFactory Class LOAD FAILED"));
		}
	}	
	check( NewActorClass );

	// if the default class is requested during gameplay, but it's bNoDelete, replace it with GameplayActorClass
	if (GWorld->HasBegunPlay() && NewActorClass == GetClass()->GetDefaultObject<UActorFactory>()->NewActorClass && NewActorClass->GetDefaultActor()->bNoDelete)
	{
		if ( GameplayActorClass == NULL || GameplayActorClass->GetDefaultActor()->bNoDelete )
		{
			appErrorf(TEXT("Actor factories of type %s cannot be used in-game"), *GetClass()->GetName() );
		}
		NewActorClass = GameplayActorClass;
	}
	check( !(NewActorClass->ClassFlags & CLASS_Abstract) );

	return NewActorClass->GetDefaultActor();
}

AActor* UActorFactory::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* ActorFactoryData )
{
	GetDefaultActor();

	check(Location);

	// Don't try and spawn bStatic things once we have started playing level.
	const UBOOL bBegunPlay = GWorld->HasBegunPlay();
	if( bBegunPlay && (GetDefaultActor()->IsStatic() || GetDefaultActor()->bNoDelete) )
	{
		debugf( TEXT("Cannot spawn class '%s' because it is bStatic/bNoDelete and HasBegunPlay is true."), *NewActorClass->GetName() );
		return NULL;
	}

	FRotator const NewRotation = Rotation ? *Rotation : GetDefaultActor()->Rotation;
	AActor* NewActor = GWorld->SpawnActor(NewActorClass, NAME_None, *Location, NewRotation);

	// Let script modify if it wants
	eventPostCreateActor(NewActor, ActorFactoryData);

	return NewActor;
}

/**
* This will check whether there is enough space to spawn an character.
* Additionally it will check the ActorFactoryData to for any overrides 
* ( e.g. bCheckSpawnCollision )
*
* @return if there is enough space to spawn character at this location
**/
UBOOL UActorFactory::IsEnoughRoomToSpawnPawn( const FVector* const Location, const USeqAct_ActorFactory* const ActorFactoryData ) const
{
	// check that the area around the location is clear of other characters
	UBOOL bHitPawn = FALSE;
	FMemMark Mark( GMainThreadMemStack );
	FCheckResult* checkResult = NULL;
	
	// if we don't have an param data then default to checking for collision	
	if( (ActorFactoryData == NULL) || ActorFactoryData->bCheckSpawnCollision )
	{
		checkResult = GWorld->MultiPointCheck( GMainThreadMemStack, *Location, FVector(36,36,78), TRACE_AllColliding );
	}
	else
	{
		checkResult = GWorld->MultiPointCheck( GMainThreadMemStack, *Location, FVector(36,36,78), TRACE_World );
	}

	for( FCheckResult* testResult = checkResult; testResult != NULL; testResult = testResult->GetNext() )
	{
		if( ( testResult->Actor != NULL )
			&& ( testResult->Actor->IsA( APawn::StaticClass() ))
			)
		{
			bHitPawn = TRUE;
			break;
		}
	}

	Mark.Pop();
	return bHitPawn;
}

/**
 * Clears references to resources [usually set by the call to AutoFillFields] when the factory has done its work.  The default behavior
 * (which is to call AutoFillFields() with an empty selection set) should be sufficient for most factories, but this method is provided
 * to allow customized behavior.
 */
void UActorFactory::ClearFields()
{
	AutoFillFields(USelection::StaticClass()->GetDefaultObject<USelection>());
}

/*-----------------------------------------------------------------------------
	UActorFactoryStaticMesh
-----------------------------------------------------------------------------*/

AActor* UActorFactoryStaticMesh::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = NULL;
#if WITH_EDITORONLY_DATA
	NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	if( StaticMesh )
	{
		debugf(TEXT("Actor Factory created %s"), *StaticMesh->GetName());
		// Term Component
		NewActor->TermRBPhys(NULL);
		NewActor->ClearComponents();

		// Change properties
		UStaticMeshComponent* StaticMeshComponent = NULL;
		for (INT Idx = 0; Idx < NewActor->Components.Num() && StaticMeshComponent == NULL; Idx++)
		{
			StaticMeshComponent = Cast<UStaticMeshComponent>(NewActor->Components(Idx));
		}

		check(StaticMeshComponent);
		StaticMeshComponent->StaticMesh = StaticMesh;
		StaticMeshComponent->VertexPositionVersionNumber = StaticMesh->VertexPositionVersionNumber;
		NewActor->DrawScale3D = DrawScale3D;

		// Init Component
		NewActor->ConditionalUpdateComponents();
		NewActor->InitRBPhys();

		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);

		if ( StaticMesh->bCanBecomeDynamic )
		{
			// if static to dynamic mesh, only self shadow
			StaticMeshComponent->bSelfShadowOnly = TRUE;

			// wake up when hit by other rigid body
			StaticMeshComponent->bNotifyRigidBodyCollision = TRUE;

			// don't block paths if can be pushed around
			NewActor->bPathColliding = FALSE;
		}
	}
#endif // WITH_EDITORONLY_DATA

	return NewActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryStaticMesh::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	UBOOL bResult = FALSE;
	if ( StaticMesh )
	{
		if ( Cast<UFracturedStaticMesh>(StaticMesh) == NULL )
		{
			bResult = TRUE;
		}
		else
		{
			OutErrorMsg = TEXT("Error_CouldNotCreateActor_FractureStaticMesh");
		}
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoStaticMesh");
	}

	return bResult;
}

void UActorFactoryStaticMesh::AutoFillFields(USelection* Selection)
{
	UStaticMesh* SelectedStaticMesh = Selection->GetTop<UStaticMesh>();
	if ( Cast<UFracturedStaticMesh>(SelectedStaticMesh) == NULL )
	{
		// fractured static mesh actors should not be handled by the standard SMA factory 
		StaticMesh = SelectedStaticMesh;
	}
}

FString UActorFactoryStaticMesh::GetMenuName()
{
	if ( StaticMesh != NULL )
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *StaticMesh->GetPathName() );
	}
	else
	{
		return MenuName;
	}
}


/*-----------------------------------------------------------------------------
	UActorFactoryFracturedStaticMesh
-----------------------------------------------------------------------------*/

AActor* UActorFactoryFracturedStaticMesh::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = NULL;
#if WITH_EDITORONLY_DATA
	NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	if( FracturedStaticMesh )
	{
		// Term Component
		NewActor->TermRBPhys(NULL);
		NewActor->ClearComponents();

		// Change properties
		UFracturedStaticMeshComponent* FracturedStaticMeshComponent = NULL;
		for (INT Idx = 0; Idx < NewActor->Components.Num() && FracturedStaticMeshComponent == NULL; Idx++)
		{
			FracturedStaticMeshComponent = Cast<UFracturedStaticMeshComponent>(NewActor->Components(Idx));
		}

		check(FracturedStaticMeshComponent);
		FracturedStaticMeshComponent->SetStaticMesh(FracturedStaticMesh);
		FracturedStaticMeshComponent->VertexPositionVersionNumber = FracturedStaticMesh->VertexPositionVersionNumber;
		NewActor->DrawScale3D = DrawScale3D;

		// Init Component
		NewActor->ConditionalUpdateComponents();
		NewActor->InitRBPhys();

		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);
	}
#endif // WITH_EDITORONLY_DATA

	return NewActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryFracturedStaticMesh::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{ 
	if ( FracturedStaticMesh != NULL && FracturedStaticMesh->BodySetup )
	{
		return TRUE;
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoStaticMesh");
		return FALSE;
	}
}

void UActorFactoryFracturedStaticMesh::AutoFillFields(USelection* Selection)
{
	FracturedStaticMesh = Selection->GetTop<UFracturedStaticMesh>();
}

FString UActorFactoryFracturedStaticMesh::GetMenuName()
{
	if(FracturedStaticMesh)
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *FracturedStaticMesh->GetPathName() );
	}
	else
	{
		return MenuName;
	}
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryDominantDirectionalLight::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly )
	{
		// No asset is selected for this factory
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_FromAssetOnly");
	}
	else
	{
		const ULevel* pCurrentLevel = GWorld ? GWorld->CurrentLevel : NULL;
		for( TObjectIterator<ADominantDirectionalLight> It; It; ++It )
		{
			ADominantDirectionalLight* CurrentLight = *It;
			// make sure we only check lights in the same world, that have valid light components, and that are enabled
			if (!CurrentLight->IsPendingKill()
				&& CurrentLight->LightComponent
				&& CurrentLight->LightComponent->bEnabled
				&& CurrentLight->GetOutermost()->ContainsMap()
				&& CurrentLight->GetLevel() == pCurrentLevel)		// must come after ContainsMap check
			{
				OutErrorMsg = TEXT("Error_CouldNotCreateActor_AlreadyADominantDirectionalLight");			
				bCanCreate = FALSE;
				break;
			}
		}
	}

	return bCanCreate;
}

/**
 * Returns whether the ActorFactory thinks it could create an Actor with the current settings.
 * Can be used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If TRUE, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	TRUE if the actor can be created with this factory
 */
UBOOL UActorFactoryDominantDirectionalLightMovable::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly /*= FALSE*/ )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly )
	{
		// No asset is selected for this factory
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_FromAssetOnly");
	}
	else
	{
		const ULevel* pCurrentLevel = GWorld ? GWorld->CurrentLevel : NULL;
		for( TObjectIterator<ADominantDirectionalLight> It; It; ++It )
		{
			ADominantDirectionalLight* CurrentLight = *It;
			// make sure we only check lights in the same world, that have valid light components, and that are enabled
			if (!CurrentLight->IsPendingKill()
				&& CurrentLight->LightComponent
				&& CurrentLight->LightComponent->bEnabled
				&& CurrentLight->GetOutermost()->ContainsMap()
				&& CurrentLight->GetLevel() == pCurrentLevel)		// must come after ContainsMap check
			{
				OutErrorMsg = TEXT("Error_CouldNotCreateActor_AlreadyADominantDirectionalLight");			
				bCanCreate = FALSE;
				break;
			}
		}
	}

	return bCanCreate;
}

/*-----------------------------------------------------------------------------
	UActorFactoryDynamicSM
-----------------------------------------------------------------------------*/

AActor* UActorFactoryDynamicSM::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	ADynamicSMActor* NewSMActor = CastChecked<ADynamicSMActor>(NewActor);

	if( StaticMesh )
	{
		// Term Component
		NewSMActor->TermRBPhys(NULL);
		NewSMActor->ClearComponents();

		// Change properties
		NewSMActor->StaticMeshComponent->StaticMesh = StaticMesh;
		if (GIsGame)
		{
			NewSMActor->ReplicatedMesh = StaticMesh;
		}
		NewSMActor->StaticMeshComponent->bNotifyRigidBodyCollision = bNotifyRigidBodyCollision;
		NewSMActor->DrawScale3D = DrawScale3D;
		NewSMActor->CollisionType = CollisionType;
		NewSMActor->SetCollisionFromCollisionType();
		NewSMActor->bNoEncroachCheck = bNoEncroachCheck;
		NewSMActor->StaticMeshComponent->bUseCompartment = bUseCompartment;
		NewSMActor->StaticMeshComponent->bCastDynamicShadow = bCastDynamicShadow;

		// Init Component
		NewSMActor->ConditionalUpdateComponents();
		NewSMActor->InitRBPhys();

		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);
	}

	return NewSMActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryDynamicSM::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	if(StaticMesh)
	{
		return TRUE;
	}
	else 
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoStaticMesh");
		return FALSE;
	}
}

void UActorFactoryDynamicSM::AutoFillFields(USelection* Selection)
{
	StaticMesh = Selection->GetTop<UStaticMesh>();
}

FString UActorFactoryDynamicSM::GetMenuName()
{
	if(StaticMesh)
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *StaticMesh->GetPathName() );
	}
	else
	{
		return MenuName;
	}
}

void UActorFactoryDynamicSM::PostLoad()
{
	Super::PostLoad();
}

/*-----------------------------------------------------------------------------
	UActorFactoryRigidBody
-----------------------------------------------------------------------------*/

void UActorFactoryRigidBody::PostLoad()
{
	Super::PostLoad();
}

AActor* UActorFactoryRigidBody::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if(NewActor)
	{
		AKActor* NewRB = CastChecked<AKActor>(NewActor);

		// Find reference frame initial velocity is applied in
		FMatrix VelFrameTM = FMatrix::Identity;
		if(bLocalSpaceInitialVelocity)
		{
			FRotator InitRot(0,0,0);
			if(Rotation)
			{
				InitRot = *Rotation;
			}

			VelFrameTM = FRotationMatrix(InitRot);
		}

		// Calculate initial angular/linear velocity
		FVector TotalInitAngVel = FVector(0,0,0);
		FVector TotalInitLinVel = InitialVelocity;

		// Add contribution from Distributions if present.
		if(AdditionalVelocity)
		{
			TotalInitLinVel += AdditionalVelocity->GetValue();
		}
		
		if(InitialAngularVelocity)
		{
			TotalInitAngVel += InitialAngularVelocity->GetValue();
		}


		FVector InitVel = VelFrameTM.TransformNormal(InitialVelocity);
	
		// Apply initial linear/angular velocity
		NewRB->StaticMeshComponent->SetRBLinearVelocity( VelFrameTM.TransformNormal(TotalInitLinVel) );
		NewRB->StaticMeshComponent->SetRBAngularVelocity( VelFrameTM.TransformNormal(TotalInitAngVel) );
		
		// Wake if desired.
		if(bStartAwake)
		{
			NewRB->StaticMeshComponent->WakeRigidBody();
		}

		NewRB->StaticMeshComponent->SetRBChannel((ERBCollisionChannel)RBChannel);
		NewRB->StaticMeshComponent->SetBlockRigidBody(bBlockRigidBody);

		NewRB->bDamageAppliesImpulse = bDamageAppliesImpulse;
		NewRB->ReplicatedDrawScale3D = NewRB->DrawScale3D * 1000.0f; // avoid effects of vector rounding (@warning: must match script: see PostBeginPlay() and ReplicatedEvent())

		NewRB->bEnableStayUprightSpring = bEnableStayUprightSpring;
		NewRB->StayUprightTorqueFactor = StayUprightTorqueFactor;
		NewRB->StayUprightMaxTorque = StayUprightMaxTorque;
	}

	return NewActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryRigidBody::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	if(StaticMesh && StaticMesh->BodySetup)
	{
		return TRUE;
	}
	else 
	{
		if ( !StaticMesh )
		{
			OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoStaticMesh");
		}
		else if ( !StaticMesh->BodySetup )
		{
			OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoRigidBodySetup");
		}

		return FALSE;
	}
}

/*-----------------------------------------------------------------------------
	UActorFactoryEmitter
-----------------------------------------------------------------------------*/

AActor* UActorFactoryEmitter::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AEmitter* NewEmitter = CastChecked<AEmitter>(NewActor);

	if( ParticleSystem )
	{
		// Term Component
		NewEmitter->ClearComponents();

		// Change properties
		NewEmitter->SetTemplate(ParticleSystem);

		// if we're created by Kismet on the server during gameplay, we need to replicate the emitter
		if (GWorld->HasBegunPlay() && GWorld->GetNetMode() != NM_Client && ActorFactoryData != NULL)
		{
			NewEmitter->RemoteRole = ROLE_SimulatedProxy;
			NewEmitter->bAlwaysRelevant = TRUE;
			NewEmitter->NetUpdateFrequency = 0.1f; // could also set bNetTemporary but LD might further trigger it or something
			// call into gameplay code with template so it can set up replication
			NewEmitter->eventSetTemplate(ParticleSystem, NewEmitter->bDestroyOnSystemFinish);
		}

		// Init Component
		NewEmitter->ConditionalUpdateComponents();
	}

	return NewEmitter;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryEmitter::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	if(ParticleSystem)
	{
		return TRUE;
	}
	else 
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoParticleSystem");
		return FALSE;
	}
}

void UActorFactoryEmitter::AutoFillFields(USelection* Selection)
{
	ParticleSystem = Selection->GetTop<UParticleSystem>();
}

FString UActorFactoryEmitter::GetMenuName()
{
	if(ParticleSystem)
		return FString::Printf( TEXT("%s: %s"), *MenuName, *ParticleSystem->GetName() );
	else
		return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryPhysicsAsset
-----------------------------------------------------------------------------*/

void UActorFactoryPhysicsAsset::PreSave()
{
	Super::PreSave();
#if WITH_EDITORONLY_DATA
	// Because PhysicsAsset->DefaultSkelMesh is editor only, we need to keep a reference to the SkeletalMesh we want to spawn here.
	if(!IsTemplate() && SkeletalMesh == NULL && PhysicsAsset && PhysicsAsset->DefaultSkelMesh)
	{
		SkeletalMesh = PhysicsAsset->DefaultSkelMesh;
	}
#endif // WITH_EDITORONLY_DATA
}

AActor* UActorFactoryPhysicsAsset::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	if(!PhysicsAsset)
	{
		return NULL;
	}

	// If a specific mesh is supplied, use it. Otherwise, use default from PhysicsAsset.
	USkeletalMesh* UseSkelMesh = SkeletalMesh;
#if WITH_EDITORONLY_DATA
	if(!UseSkelMesh)
	{
		UseSkelMesh = PhysicsAsset->DefaultSkelMesh;
	}
#endif // WITH_EDITORONLY_DATA
	if(!UseSkelMesh)
	{
		return NULL;
	}

	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if(!NewActor)
	{
		return NULL;
	}

	AKAsset* NewAsset = CastChecked<AKAsset>(NewActor);

	// Term Component
	NewAsset->TermRBPhys(NULL);
	NewAsset->ClearComponents();

	// Change properties
	NewAsset->SkeletalMeshComponent->SkeletalMesh = UseSkelMesh;
	if (GIsGame)
	{
		NewAsset->ReplicatedMesh = UseSkelMesh;
		NewAsset->ReplicatedPhysAsset = PhysicsAsset;
	}
	NewAsset->SkeletalMeshComponent->PhysicsAsset = PhysicsAsset;
	NewAsset->SkeletalMeshComponent->bNotifyRigidBodyCollision = bNotifyRigidBodyCollision;
	NewAsset->SkeletalMeshComponent->bUseCompartment = bUseCompartment;
	NewAsset->SkeletalMeshComponent->bCastDynamicShadow = bCastDynamicShadow;
	NewAsset->DrawScale3D = DrawScale3D;

	// Init Component
	NewAsset->ConditionalUpdateComponents();
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

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryPhysicsAsset::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	if(PhysicsAsset)
	{
		return TRUE;
	}
	else 
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoPhysicsAsset");
		return FALSE;
	}
}

void UActorFactoryPhysicsAsset::AutoFillFields(USelection* Selection)
{
	PhysicsAsset = Selection->GetTop<UPhysicsAsset>();
}

FString UActorFactoryPhysicsAsset::GetMenuName()
{
	if(PhysicsAsset)
		return FString::Printf( TEXT("%s: %s"), *MenuName, *PhysicsAsset->GetName() );
	else
		return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAI
-----------------------------------------------------------------------------*/
AActor* UActorFactoryAI::GetDefaultActor()
{
	if ( PawnClass )
	{
		NewActorClass = PawnClass;
	}

	check( NewActorClass );
	check( !(NewActorClass->ClassFlags & CLASS_Abstract) );

	return NewActorClass->GetDefaultActor();
}

AActor* UActorFactoryAI::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	// first create the pawn
	APawn* newPawn = NULL;
	if (PawnClass != NULL)
	{
		// check that the area around the location is clear of other characters
		UBOOL bHitPawn = IsEnoughRoomToSpawnPawn( Location, ActorFactoryData );

		if (!bHitPawn)
		{
			newPawn = (APawn*)Super::CreateActor( Location, Rotation, ActorFactoryData );
			if (newPawn != NULL)
			{
				// create the controller
				if (ControllerClass != NULL)
				{
					// If no pointer for rotation supplied, use default rotation.
					FRotator NewRotation;
					if(Rotation)
						NewRotation = *Rotation;
					else
						NewRotation = ControllerClass->GetDefaultActor()->Rotation;

					check(Location);
					AAIController* newController = (AAIController*)GWorld->SpawnActor(ControllerClass, NAME_None, *Location, NewRotation);
					if (newController != NULL)
					{
						// handle the team assignment
						newController->eventSetTeam(TeamIndex);
						// force the controller to possess, etc
						newController->eventPossess(newPawn, false);


						if (newController && newController->PlayerReplicationInfo && PawnName != TEXT("") )
							newController->PlayerReplicationInfo->eventSetPlayerName(PawnName);
					}
				}
				if (bGiveDefaultInventory && newPawn->WorldInfo->Game != NULL)
				{
					newPawn->WorldInfo->Game->eventAddDefaultInventory(newPawn);
				}
				// create any inventory
				for (INT idx = 0; idx < InventoryList.Num(); idx++)
				{
					newPawn->eventCreateInventory( InventoryList(idx), false  );
				}
			}
		}
	}
	return newPawn;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryAI::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly && !PawnClass )
	{	
		// Actor cant be created if a valid asset hasnt been assigned
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoAssetAssigned");
	}

	return bCanCreate;
}


/*-----------------------------------------------------------------------------
  UActorFactoryActor
-----------------------------------------------------------------------------*/
AActor* UActorFactoryActor::GetDefaultActor()
{
	if ( ActorClass )
	{
		NewActorClass = ActorClass;
	}

	check( NewActorClass );
	check( !(NewActorClass->ClassFlags & CLASS_Abstract) );

	return NewActorClass->GetDefaultActor();
}

AActor* UActorFactoryActor::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	// first create the pawn
	AActor* NewActor = NULL;

	if (ActorClass != NULL)
	{
		// check that the area around the location is clear of other characters
		UBOOL bHitPawn = IsEnoughRoomToSpawnPawn( Location, ActorFactoryData );

		if( !bHitPawn )
		{
			NewActor = (AActor*)Super::CreateActor( Location, Rotation, ActorFactoryData );
		}
	}
	return NewActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryActor::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly && !ActorClass )
	{
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoAssetAssigned");
	}

	return bCanCreate;
}


/*-----------------------------------------------------------------------------
	UActorFactoryVehicle
-----------------------------------------------------------------------------*/
AActor* UActorFactoryVehicle::GetDefaultActor()
{
	if ( VehicleClass )
	{
		NewActorClass = VehicleClass;
	}

	check( NewActorClass );
	check( !(NewActorClass->ClassFlags & CLASS_Abstract) );

	return NewActorClass->GetDefaultActor();
}

AActor* UActorFactoryVehicle::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	// first create the pawn
	AVehicle* NewVehicle = NULL;

	if (VehicleClass != NULL)
	{
		// check that the area around the location is clear of other characters
		UBOOL bHitPawn = IsEnoughRoomToSpawnPawn( Location, ActorFactoryData );

		if( !bHitPawn )
		{
			NewVehicle = (AVehicle*)Super::CreateActor( Location, Rotation, ActorFactoryData );
		}
	}
	return NewVehicle;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryVehicle::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly && !VehicleClass )
	{
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoAssetAssigned");
	}
	return bCanCreate;
}
/*-----------------------------------------------------------------------------
	UActorFactorySkeletalMesh
-----------------------------------------------------------------------------*/

AActor* UActorFactorySkeletalMesh::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if(!NewActor)
	{
		return NULL;
	}

	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);

	if( SkeletalMesh )
	{
		// Term Component
		NewSMActor->ClearComponents();

		// Change properties
		NewSMActor->SkeletalMeshComponent->SkeletalMesh = SkeletalMesh;
		if (GIsGame)
		{
			NewSMActor->ReplicatedMesh = SkeletalMesh;
		}
		if(AnimSet)
		{
			NewSMActor->SkeletalMeshComponent->AnimSets.AddItem( AnimSet );
		}

		UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(NewSMActor->SkeletalMeshComponent->Animations);
		if(SeqNode)
		{
			SeqNode->AnimSeqName = AnimSequenceName;

			if (AnimSequenceName != NAME_None)
			{
				SeqNode->PlayAnim(TRUE);
			}
		}

		// Init Component
		NewSMActor->ConditionalUpdateComponents();

		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);
	}

	return NewSMActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactorySkeletalMesh::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	if(SkeletalMesh)
	{	
		return TRUE;
	}
	else 
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoSkellMesh");
		return FALSE;
	}
}

void UActorFactorySkeletalMesh::AutoFillFields(USelection* Selection)
{
	SkeletalMesh = Selection->GetTop<USkeletalMesh>();
}

FString UActorFactorySkeletalMesh::GetMenuName()
{
	if(SkeletalMesh)
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *SkeletalMesh->GetPathName() );
	}
	else
	{
		return MenuName;
	}
}

/*-----------------------------------------------------------------------------
	UActorFactoryAmbientSound
-----------------------------------------------------------------------------*/

void UActorFactoryAmbientSound::SetSoundCue( AAmbientSound* NewSound )
{
	if( AmbientSoundCue )
	{
		// Term Component
		NewSound->ClearComponents();

		// Change properties
		NewSound->AudioComponent->SoundCue = AmbientSoundCue;

		// Init Component
		NewSound->ConditionalUpdateComponents();

		// propagate the actor
		GObjectPropagator->PropagateActor( NewSound );
	}
}

AActor* UActorFactoryAmbientSound::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AAmbientSound* NewSound = CastChecked<AAmbientSound>( NewActor );
	SetSoundCue( NewSound );

	return NewSound;
}

void UActorFactoryAmbientSound::AutoFillFields( USelection* Selection )
{
	AmbientSoundCue = Selection->GetTop<USoundCue>();
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryAmbientSound::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreate = TRUE;
	if( bFromAssetOnly && !AmbientSoundCue )
	{
		bCanCreate = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoAssetAssigned");
	}
	return bCanCreate;
}

FString UActorFactoryAmbientSound::GetMenuName( void )
{
	if( AmbientSoundCue )
	{
		return FString::Printf( TEXT( "%s: %s" ), *MenuName, *AmbientSoundCue->GetName() );
	}

	return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAmbientSoundMovable
-----------------------------------------------------------------------------*/

AActor* UActorFactoryAmbientSoundMovable::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AAmbientSoundMovable* NewSound = CastChecked<AAmbientSoundMovable>( NewActor );
	SetSoundCue( NewSound );

	return NewSound;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAmbientSoundSimple
-----------------------------------------------------------------------------*/

void UActorFactoryAmbientSoundSimple::SetSoundSlot( AAmbientSoundSimple* NewSound )
{
	if( SoundNodeWave )
	{
		// Term Component
		NewSound->ClearComponents();

		// Change properties
		FAmbientSoundSlot SoundSlot;
		SoundSlot.Wave = SoundNodeWave;

		NewSound->AmbientProperties->SoundSlots.AddItem( SoundSlot );

		// Init Component
		NewSound->ConditionalUpdateComponents();

		// propagate the actor
		GObjectPropagator->PropagateActor( NewSound );
	}
}

AActor* UActorFactoryAmbientSoundSimple::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AAmbientSoundSimple* NewSound = CastChecked<AAmbientSoundSimple>( NewActor );
	SetSoundSlot( NewSound );

	return NewSound;
}

void UActorFactoryAmbientSoundSimple::AutoFillFields( USelection* Selection )
{
	SoundNodeWave = Selection->GetTop<USoundNodeWave>();
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryAmbientSoundSimple::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	UBOOL bCanCreateActor = TRUE;
	if( bFromAssetOnly && !SoundNodeWave )
	{
		bCanCreateActor = FALSE;
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoAssetAssigned");
	}
	return bCanCreateActor;
}

FString UActorFactoryAmbientSoundSimple::GetMenuName( void )
{
	if( SoundNodeWave )
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *SoundNodeWave->GetName() );
	}

	return MenuName;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAmbientSoundNonLoop
-----------------------------------------------------------------------------*/

AActor* UActorFactoryAmbientSoundNonLoop::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AAmbientSoundSimple* NewSound = CastChecked<AAmbientSoundNonLoop>( NewActor );
	SetSoundSlot( NewSound );

	return NewSound;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAmbientSoundNonLoopingToggleable
-----------------------------------------------------------------------------*/

AActor* UActorFactoryAmbientSoundNonLoopingToggleable::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AAmbientSoundSimple* NewSound = CastChecked<AAmbientSoundNonLoopingToggleable>( NewActor );
	SetSoundSlot( NewSound );

	return NewSound;
}

/*-----------------------------------------------------------------------------
	UActorFactoryAmbientSimpleToggleable
-----------------------------------------------------------------------------*/

AActor* UActorFactoryAmbientSoundSimpleToggleable::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AAmbientSoundSimpleToggleable* NewSound = CastChecked<AAmbientSoundSimpleToggleable>( NewActor );
	SetSoundSlot( NewSound );

	return NewSound;
}

/*-----------------------------------------------------------------------------
	UActorFactoryDecal
-----------------------------------------------------------------------------*/

/**
 * @return	TRUE if the specified material instance is a valid decal material.
 */
static inline UBOOL IsValidDecalMaterial(UMaterialInterface* MaterialInterface)
{
	if (MaterialInterface)
	{
		// Require that the selected material has a decal usage
		UMaterial* BaseSelectedMaterial = MaterialInterface->GetMaterial();
		if (BaseSelectedMaterial->GetUsageByFlag(MATUSAGE_Decals))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Called to create an actor at the supplied location/rotation
 *
 * @param	Location			Location to create the actor at
 * @param	Rotation			Rotation to create the actor with
 * @param	ActorFactoryData	Kismet object which spawns actors, could potentially have settings to use/override
 *
 * @return	The newly created actor, NULL if it could not be created
 */
AActor* UActorFactoryDecal::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor;

	if ( Rotation )
	{
		// Assuming the rotation is a surface orientation, we hand in the inverted rotation 
		// to orient the decal towards the surface.
		const FRotator InvertedRotation( (-Rotation->Vector()).Rotation() );
		if ( Location )
		{
			// A location was specified; back the decal off the surface a bit.
			const FVector OffsetLocation( *Location + Rotation->Vector() );
			NewActor = Super::CreateActor( &OffsetLocation, &InvertedRotation, ActorFactoryData );
		}
		else
		{
			NewActor = Super::CreateActor( NULL, &InvertedRotation, ActorFactoryData );
		}
	}
	else
	{
		// No rotation was specified, so we can't orient the decal or back it off the surface.
		NewActor = Super::CreateActor( Location, NULL, ActorFactoryData );
	}

	if( !NewActor )
	{
		return NULL;
	}

	ADecalActorBase* NewDecalActor = CastChecked<ADecalActorBase>( NewActor );

	if( NewDecalActor && 
		IsValidDecalMaterial( DecalMaterial ) )
	{
		// Term Component
		NewDecalActor->ClearComponents();

		// Change properties
		NewDecalActor->Decal->SetDecalMaterial(DecalMaterial);

		// Call PostEditMove to force the decal actor's position/orientation on the decal component.
		//NewDecalActor->PostEditMove( TRUE );

		// Init Component
		NewDecalActor->ConditionalUpdateComponents();

		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);
	}

	return NewDecalActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryDecal::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{
	UBOOL bResult = IsValidDecalMaterial( DecalMaterial );

	// Fill in error code if there isn't a valid decal material
	if ( !bResult )
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoDecalMaterial");
	}

	return bResult;
}

/**
 * Fill the data fields of this actor with the current selection
 *
 * @param	Selection	Selection to use to fill this actor's data fields with
 */
void UActorFactoryDecal::AutoFillFields(USelection* Selection)
{
	// use selected decal material
	if( Selection )
	{
		for( FSelectionIterator It( *Selection ); It; ++It )
		{
			UMaterialInterface* Mat = Cast<UMaterialInterface>(*It);
			if (IsValidDecalMaterial(Mat))
			{
				DecalMaterial = Mat;
				break;
			}
		}
	}
}

/**
 * Clears references to resources [usually set by the call to AutoFillFields] when the factory has done its work.  The default behavior
 * (which is to call AutoFillFields() with an empty selection set) should be sufficient for most factories, but this method is provided
 * to allow customized behavior.
 */
void UActorFactoryDecal::ClearFields()
{
	DecalMaterial = NULL;
}

/**
 * Returns the name this factory should show up as in a context-sensitive menu
 *
 * @return	Name this factory should show up as in a menu
 */
FString UActorFactoryDecal::GetMenuName()
{
	if( IsValidDecalMaterial( DecalMaterial ) )
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *DecalMaterial->GetPathName() );
	}
	else
	{
		return MenuName;
	}
}

/*-----------------------------------------------------------------------------
	UActorFactoryArchetype
-----------------------------------------------------------------------------*/

AActor* UActorFactoryArchetype::GetDefaultActor()
{
	return ArchetypeActor;
}

AActor* UActorFactoryArchetype::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	check(Location);

	// Invalid if there is no Archetype, or  the Archetype is not one...
	if(!ArchetypeActor || !ArchetypeActor->HasAnyFlags(RF_ArchetypeObject))
	{
		return NULL;
	}

	UClass* NewClass = ArchetypeActor->GetClass();

	FRotator NewRotation;
	if(Rotation)
		NewRotation = *Rotation;
	else
		NewRotation = NewClass->GetDefaultActor()->Rotation;

	AActor* NewActor = GWorld->SpawnActor(NewClass, NAME_None, *Location, NewRotation, ArchetypeActor);

	return NewActor;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryArchetype::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly ) 
{ 
	if(ArchetypeActor && ArchetypeActor->HasAnyFlags(RF_ArchetypeObject))
	{
		return TRUE;
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoArchetype");
		return FALSE;
	}
}

void UActorFactoryArchetype::AutoFillFields(USelection* Selection)
{
	ArchetypeActor = NULL;

	for( USelection::TObjectIterator It( Selection->ObjectItor() ) ; It && !ArchetypeActor; ++It )
	{
		UObject* Object = *It;
		AActor* Actor = Cast<AActor>(Object);
		if(Actor && Actor->HasAnyFlags(RF_ArchetypeObject) )
		{
			ArchetypeActor = Actor;
		}
	}
}

FString UActorFactoryArchetype::GetMenuName()
{
	if(ArchetypeActor)
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *ArchetypeActor->GetFullName() );
	}
	else
	{
		return MenuName;
	}
}

/**
 *	UActorFactoryLensFlare
 */
    //## BEGIN PROPS ActorFactoryLensFlare
//    class ULensFlare* LensFlareObject;
    //## END PROPS ActorFactoryLensFlare
AActor* UActorFactoryLensFlare::CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	ALensFlareSource* NewLensFlare = CastChecked<ALensFlareSource>(NewActor);

	if( LensFlareObject )
	{
		// Term Component
		NewLensFlare->ClearComponents();

		// Change properties
		NewLensFlare->RemoteRole = ROLE_None;
		NewLensFlare->bAlwaysRelevant = FALSE;
		NewLensFlare->NetUpdateFrequency = 0.0f; // could also set bNetTemporary but LD might further trigger it or something
		NewLensFlare->SetTemplate(LensFlareObject);

		// Initialize the visualization components here prior to the creation of their scene proxys
		if (NewLensFlare->LensFlareComp)
		{
			NewLensFlare->LensFlareComp->InitializeVisualizationData(TRUE);
		}

		// Init Component
		NewLensFlare->ConditionalUpdateComponents();
	}

	return NewLensFlare;
}

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryLensFlare::CanCreateActor( FString& OutErrorMsg, UBOOL bFromAssetOnly )
{
	if(LensFlareObject)
	{
		return TRUE;
	}
	else 
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoLensFlare");
		return FALSE;
	}
}

void UActorFactoryLensFlare::AutoFillFields(class USelection* Selection)
{
	LensFlareObject = Selection->GetTop<ULensFlare>();
}

FString UActorFactoryLensFlare::GetMenuName()
{
	if (LensFlareObject)
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *LensFlareObject->GetName() );
	}
	else
	{
		return MenuName;
	}
}

/**
 * @return	TRUE if the specified material instance is a valid fog material.
 */
static inline UBOOL IsValidFogMaterial(UMaterialInterface* MaterialInterface)
{
	if (MaterialInterface)
	{
		// Require that the selected material has a fog usage
		UMaterial* BaseSelectedMaterial = MaterialInterface->GetMaterial();
		if (BaseSelectedMaterial->GetUsageByFlag(MATUSAGE_FogVolumes))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*
 * 
 */
static void SetupFogVolumeActor(AActor* NewActor, UMaterialInterface* SelectedMaterial)
{
	AFogVolumeDensityInfo* NewFogVolume = CastChecked<AFogVolumeDensityInfo>(NewActor);

	// Find an unused name in the level
	INT NameIndex = 0;
	UObject* ExistingObject = NULL;
	FString PotentialMIName;
	do 
	{
		PotentialMIName = FString::Printf(TEXT("FogVolumeMI_%i"), NameIndex);
		ExistingObject = FindObject<UObject>(NewFogVolume->GetOutermost(), *PotentialMIName);
		NameIndex++;
	} 
	while (ExistingObject != NULL);

	// Create a new material instance, whose parent is the selected material in the generic browser, or EngineMaterials.FogVolumeMaterial if nothing is selected
	UMaterialInstanceConstant* NewMI = 
		ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), NewFogVolume->GetOutermost(), *PotentialMIName, RF_Transactional);
	NewMI->MarkPackageDirty(TRUE);

	// If a valid material was selected in the generic browser, use that as the parent, otherwise use the default fog volume material
	if (IsValidFogMaterial(SelectedMaterial))
	{
		NewMI->SetParent(SelectedMaterial);
	}
	else
	{
		UMaterialInterface* DefaultFogVolumeMaterial = 
			(UMaterialInterface*)UObject::StaticLoadObject( UMaterialInterface::StaticClass(),NULL,TEXT("EngineMaterials.FogVolumeMaterial"),NULL,LOAD_None,NULL );
		NewMI->SetParent(DefaultFogVolumeMaterial);
	}

	// Set the new material instance on the fog volume
	NewFogVolume->DensityComponent->FogMaterial = NewMI;
}

/**
*	UActorFactoryFogVolumeConstantDensityInfo
*/

/**
 * If the ActorFactory thinks it could create an Actor with the current settings.
 * Can Used to determine if we should add to context menu or if the factory can be used for drag and drop.
 *
 * @param	OutErrorMsg		Receives localized error string name if returning FALSE.
 * @param	bFromAssetOnly	If true, the actor factory will check that a valid asset has been assigned from selection.  If the factory always requires an asset to be selected, this param does not matter
 * @return	True if the actor can be created with this factory
 */
UBOOL UActorFactoryFogVolumeConstantDensityInfo::CanCreateActor( FString& OutErrorMsg, UBOOL bForAssetOnly )
{
	UBOOL bCanCreate = FALSE;

	if ( (SelectedMaterial || bNothingSelected) && !bForAssetOnly )
	{
		bCanCreate = TRUE;
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_SelectedItemIsNotFogVolumeMaterial");
	}

	return bCanCreate;
}

AActor* UActorFactoryFogVolumeConstantDensityInfo::CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	SetupFogVolumeActor(NewActor, SelectedMaterial);
	
	return NewActor;
}

void UActorFactoryFogVolumeConstantDensityInfo::AutoFillFields(class USelection* Selection)
{
	SelectedMaterial = Selection->GetTop<UMaterialInterface>();

	if (!IsValidFogMaterial(SelectedMaterial))
	{
		SelectedMaterial = NULL;
	}

	bNothingSelected = Selection->GetTop<UObject>() == NULL || Selection->GetTop<UClass>();
}

/**
*	UActorFactoryFogVolumeSphericalDensityInfo
*/
AActor* UActorFactoryFogVolumeSphericalDensityInfo::CreateActor( const FVector* const Location, const FRotator* const Rotation, const class USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	AFogVolumeDensityInfo* NewFogVolume = CastChecked<AFogVolumeDensityInfo>(NewActor);

	if (NewFogVolume->AutomaticMeshComponent && NewFogVolume->AutomaticMeshComponent->StaticMesh)
	{
		UFogVolumeSphericalDensityComponent* SphericalComponent = CastChecked<UFogVolumeSphericalDensityComponent>(NewFogVolume->DensityComponent);
		const FLOAT ComponentBoundsRadius = NewFogVolume->AutomaticMeshComponent->StaticMesh->Bounds.SphereRadius;
		// Set the AutomaticMeshComponent's scale so that it will always bound the fog volume tightly
		NewFogVolume->AutomaticMeshComponent->Scale = (600.0f + 5.0f) / ComponentBoundsRadius;
	}

	return NewActor;
}


/*-----------------------------------------------------------------------------
UActorFactoryApexDestructible
-----------------------------------------------------------------------------*/

AActor* UActorFactoryApexDestructible::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}

	if( DestructibleAsset )
	{
		// Term Component
		NewActor->TermRBPhys(NULL);
		NewActor->ClearComponents();

		// Change properties
		UApexStaticDestructibleComponent* ApexStaticDestructibleComponent = NULL;
		for( INT Index = 0; Index < NewActor->Components.Num() && ApexStaticDestructibleComponent == NULL; ++Index )
		{
			ApexStaticDestructibleComponent = Cast<UApexStaticDestructibleComponent>(NewActor->Components(Index));
		}
		check(ApexStaticDestructibleComponent);
		//		ApexStaticDestructibleComponent->WireframeColor = FColor( 255, 128, 0, 255 );
		ApexStaticDestructibleComponent->Asset = DestructibleAsset;

		//		UApexDynamicDestructibleComponent* ApexDynamicDestructibleComponent = NULL;
		//		for( INT Index = 0; Index < NewActor->Components.Num() && ApexDynamicDestructibleComponent == NULL; ++Index )
		//		{
		//			ApexDynamicDestructibleComponent = Cast<UApexDynamicDestructibleComponent>(NewActor->Components(Index));
		//		}
		//		check(ApexDynamicDestructibleComponent);
		//		ApexDynamicDestructibleComponent->WireframeColor = FColor( 255, 128, 0, 255 );
		//		ApexDynamicDestructibleComponent->Asset = DestructibleAsset;

		// Set ApexDestructibleActor-specific fields
		AApexDestructibleActor * DestructibleActor = Cast<AApexDestructibleActor>(NewActor);
		check( DestructibleActor != NULL );

		DestructibleActor->LoadEditorParametersFromAsset();
		if( (DestructibleAsset != NULL) )
		{
			// Make sure the asset is up to date first
			if( DestructibleAsset->FractureMaterials.Num() != DestructibleActor->FractureMaterials.Num() )
			{
				// Fix up FractureMaterials if the number of fracture levels has changed
				DestructibleActor->FractureMaterials.Empty();
				for( INT Depth = 0; Depth < DestructibleAsset->FractureMaterials.Num(); ++Depth)
				{
					DestructibleActor->FractureMaterials.AddItem( DestructibleAsset->FractureMaterials(Depth) );
				}
			}
		}
		DestructibleActor->CacheFractureEffects();

		// Init Component
		if( NewActor->CollisionComponent )
		{
			NewActor->CollisionComponent->SetRBCollisionChannels( CollideWithChannels );
			NewActor->CollisionComponent->SetRBChannel( (ERBCollisionChannel)RBChannel );
		}
		NewActor->ConditionalUpdateComponents();
		NewActor->InitRBPhys();

		// Set the destructible actor to dynamic initially
		if( bStartAwake )
		{
			//Only the first param is meaningful
			Cast<AApexDestructibleActor>(NewActor)->setPhysics(PHYS_RigidBody, NULL, FVector(0,0,0));
		}

		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);
	}

	return NewActor;
}

UBOOL UActorFactoryApexDestructible::CanCreateActor(FString& OutErrorMsg, UBOOL bFromAssetOnly) 
{ 
	if(DestructibleAsset)
	{
		return true;
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoDestructibleAsset");
		return false;
	}
}

void UActorFactoryApexDestructible::AutoFillFields(USelection* Selection)
{
	DestructibleAsset = Selection->GetTop<UApexDestructibleAsset>();
}

FString UActorFactoryApexDestructible::GetMenuName()
{
	if(DestructibleAsset)
	{
		return FString::Printf( TEXT("%s: %s"), *MenuName, *DestructibleAsset->GetPathName() );
	}
	else
	{
		return MenuName;
	}
}

/*-----------------------------------------------------------------------------
UActorFactoryApexClothing
-----------------------------------------------------------------------------*/

AActor* UActorFactoryApexClothing::CreateActor( const FVector* const Location, const FRotator* const Rotation, const USeqAct_ActorFactory* const ActorFactoryData )
{
	AActor* NewActor = Super::CreateActor( Location, Rotation, ActorFactoryData );
	if( !NewActor )
	{
		return NULL;
	}
	ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>(NewActor);
	check(NewSMActor->SkeletalMeshComponent != NULL);
	//We have at least one clothing asset
	if( ClothingAssets.Num() > 0 && ClothingAssets(0) != NULL )
	{
		if(NewSMActor->SkeletalMeshComponent->SkeletalMesh)
		{
			NewSMActor->SkeletalMeshComponent->SkeletalMesh->ClothingAssets = ClothingAssets;
		}

		NewSMActor->SkeletalMeshComponent->ApexClothingRBChannel = ClothingRBChannel;
		NewSMActor->SkeletalMeshComponent->ApexClothingRBCollideWithChannels = ClothingRBCollideWithChannels;
		//Initialize 
		NewSMActor->SkeletalMeshComponent->InitComponentRBPhys(true);
		
		// propagate the actor
		GObjectPropagator->PropagateActor(NewActor);
	}
	return NewSMActor;
}

UBOOL UActorFactoryApexClothing::CanCreateActor(FString& OutErrorMsg, UBOOL bFromAssetOnly) 
{ 
	//We have at least one asset
	if((SkeletalMesh != NULL) && (ClothingAssets.Num() > 0) && (ClothingAssets(0) != NULL))
	{
		return TRUE;
	}
	else
	{
		OutErrorMsg = TEXT("Error_CouldNotCreateActor_NoClothingAsset");
		return FALSE;
	}
}
void UActorFactoryApexClothing::AutoFillFields(USelection* Selection)
{
	UApexClothingAsset* ClothingAsset = Selection->GetTop<UApexClothingAsset>();
	if( ClothingAsset != NULL )
	{
		ClothingAssets.AddUniqueItem(ClothingAsset); 
	}
}
FString UActorFactoryApexClothing::GetMenuName()
{
	if(ClothingAssets.Num() == 1)
	{
		UApexClothingAsset* pa = ClothingAssets(0);
		return FString::Printf( TEXT("%s: %s"), *MenuName, *pa->GetFullName() );
	}
	else
	{
		return MenuName;
	}
}

/** Sets up the Fog Volume Actor with the default Fog Material if it is not already made*/
void AFogVolumeDensityInfo::SetupDefaultFogVolume()
{
	if(DensityComponent && DensityComponent->FogMaterial == NULL)
	{
		SetupFogVolumeActor(this, NULL);
	}
}

