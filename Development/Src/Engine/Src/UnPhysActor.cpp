/*=============================================================================
	UnPhysActor.cpp: Actor-related rigid body physics functions.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 


#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnTerrain.h"

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#ifdef WITH_NOVODEX
static FLOAT AxisSpringScale = 100.0f;
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(UKMeshProps);	
IMPLEMENT_CLASS(AKActor);
IMPLEMENT_CLASS(AKAsset);
IMPLEMENT_CLASS(UPhysicalMaterial);
IMPLEMENT_CLASS(ARB_Thruster);
IMPLEMENT_CLASS(ARB_LineImpulseActor);

//////////////// ACTOR ///////////////

void AActor::InitRBPhys()
{
	if(bDeleteMe) 
		return;	

	ULevel* level = GetLevel();
	if(!level)
		return;

#ifdef WITH_NOVODEX
	if (!level->NovodexScene)
		return;
#endif // WITH_NOVODEX

	if(Physics == PHYS_Articulated)
	{
		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(CollisionComponent);
		if(SkelComp)
			SkelComp->InitArticulated();
		return;
	}

	// Iterate over all prim components in this actor, creating collision geometry for each one.
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>( Components(ComponentIndex) );

		if(!PrimComp)
			continue;

		// If we already have a rigid body, do nothing.
		if(PrimComp->BodyInstance)
			continue;

		// If physics is PHYS_RigidBody, we only create a rigid body for the CollisionComponent.
		if(Physics == PHYS_RigidBody && PrimComp != CollisionComponent)
			continue;

		URB_BodySetup* BodySetup = PrimComp->FindRBBodySetup();
		if(!BodySetup)
			continue;

		FVector FullScale = DrawScale * DrawScale3D;
		UBOOL bFixBody;
		if(Physics == PHYS_RigidBody)
			bFixBody = false;
		else
			bFixBody = true;

		// Create new BodyInstance at given location.
		FMatrix PrimTM = PrimComp->LocalToWorld;
		PrimTM.RemoveScaling();

		PrimComp->BodyInstance = ConstructObject<URB_BodyInstance>( URB_BodyInstance::StaticClass(), GetLevel(), NAME_None, RF_Transactional );
		PrimComp->BodyInstance->InitBody(BodySetup, PrimTM, FullScale, bFixBody, PrimComp);
	}

}

void AActor::TermRBPhys()
{
	ULevel* level = GetLevel();

	if(Physics == PHYS_Articulated)
	{
		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(CollisionComponent);
		if(SkelComp)
			SkelComp->TermArticulated();
		return;
	}

	// Iterate over all prim components in this actor, creating collision geometry for each one.
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>( Components(ComponentIndex) );

		if(PrimComp && PrimComp->BodyInstance)
		{
			PrimComp->BodyInstance->TermBody();
			delete PrimComp->BodyInstance;
			PrimComp->BodyInstance = NULL;
		}
	}

}

#ifdef WITH_NOVODEX
NxActor* AActor::GetNxActor(FName BoneName)
{
	if (CollisionComponent)
		return CollisionComponent->GetNxActor(BoneName);
	else
		return NULL;
}
#endif // WITH_NOVODEX

/** 
 *	Update the Actor to match the rigid-body physics of its CollisionComponent (if there are any). 
 */
void AActor::SyncActorToRBPhysics()
{
	if(!CollisionComponent)
	{
		debugf(TEXT("AActor::SyncActorToRBPhysics : No CollisionComponent."));
		return;
	}

	if(!CollisionComponent->BodyInstance)
	{
		debugf(TEXT("AActor::SyncActorToRBPhysics : No BodyInstance in CollisionComponent."));
		return;
	}

	// Get transform we want to get this Component to be at. 
	FMatrix ComponentTM = CollisionComponent->BodyInstance->GetUnrealWorldTM();

	// Now we have to work out where to put the Actor to achieve this.

	// First find the current Actor-to-Component transform
	FMatrix ActorTM = LocalToWorld();
	FMatrix RelTM = CollisionComponent->LocalToWorld * ActorTM.Inverse();
	
	// Then apply the inverse of this to the new Component TM to get the new Actor TM.
	FMatrix NewTM = RelTM.Inverse() * ComponentTM;

	FVector NewLocation = NewTM.GetOrigin();
	FVector MoveBy = NewLocation - Location;

	FRotator NewRotation = NewTM.Rotator();

	FCheckResult Hit(1.0f);
	GetLevel()->MoveActor(this, MoveBy, NewRotation, Hit);

	// Update actor Velocity variable to that of rigid body. Might be used by other gameplay stuff.
	URB_BodyInstance* BodyInst = CollisionComponent->BodyInstance;

	BodyInst->Velocity = BodyInst->GetUnrealWorldVelocity();
	Velocity = BodyInst->Velocity;
}


void AActor::physRigidBody(FLOAT DeltaTime)
{
	check(Physics == PHYS_RigidBody);

	SyncActorToRBPhysics();

#ifdef WITH_NOVODEX
	// If we are not in the default physics volume, apply a force to the rigid body.
	APhysicsVolume* DefVol = GetLevel()->GetLevelInfo()->GetDefaultPhysicsVolume();
	if(PhysicsVolume != DefVol)
	{
		NxVec3 nDeltaGrav = U2NPosition(PhysicsVolume->Gravity - DefVol->Gravity);

		if(CollisionComponent)
		{
			NxActor* nActor = CollisionComponent->GetNxActor();
			if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
			{
				nActor->addForce( nActor->getMass() * nDeltaGrav );
			}
		}
	}
#endif
}

void AActor::physArticulated(FLOAT DeltaTime)
{
#ifdef WITH_NOVODEX
	check(Physics == PHYS_Articulated);

	if(!CollisionComponent)
	{
		debugf(TEXT("AActor::physArticulated (%s) : No CollisionComponent."), GetName());
		return;
	}

	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(CollisionComponent);
	if(!SkelComp)
	{
		debugf(TEXT("AActor::physArticulated (%s) : CollisionComponent is not a SkeletalMeshComponent."), GetName());
		return;
	}

	// If there is no asset instance, we can't update from anything.
	UPhysicsAssetInstance* AssetInst = SkelComp->PhysicsAssetInstance;
	if(!AssetInst)
	{
		debugf(TEXT("AActor::physArticulated (%s) : No PhysicsAssetInstance."), GetName());
		return;
	}

	if(AssetInst->RootBodyIndex == INDEX_NONE)
	{
		debugf(TEXT("AActor::physArticulated (%s) : Invalid RootBodyIndex."), GetName());
		return;
	}

	// In PHYS_Articulated, the root physics bone is placed at the origin of the skel component. So we work out the trans.
	URB_BodyInstance* BodyInst = AssetInst->Bodies(AssetInst->RootBodyIndex);
	check(BodyInst);

	if( !BodyInst->GetNxActor() )
	{
		debugf(TEXT("AActor::physArticulated (%s) : Root bone has no NxActor."), GetName());
		return;
	}

	FMatrix RootBoneTM = BodyInst->GetUnrealWorldTM();
	FMatrix NewActorTM( RootBoneTM.GetOrigin(), FRotator(0, 0, 0) );

	FVector NewLocation = NewActorTM.GetOrigin();
	FVector MoveBy = NewLocation - Location;

	FRotator NewRotation = NewActorTM.Rotator();

	FCheckResult Hit(1.0f);
	GetLevel()->MoveActor(this, MoveBy, NewRotation, Hit);

	// Update velocity of actor to that of the body we are following.
	Velocity = BodyInst->GetUnrealWorldVelocity();

	// Update per bone velocities as well.
	for (UINT Index = 0; Index < UINT(AssetInst->Bodies.Num()); Index++)
	{
		URB_BodyInstance* BoneBodyInst = AssetInst->Bodies(Index);
		BoneBodyInst->Velocity = BoneBodyInst->GetUnrealWorldVelocity();
	}

	// If we are not in the default physics volume, apply a force to each rigid body.
	APhysicsVolume* DefVol = GetLevel()->GetLevelInfo()->GetDefaultPhysicsVolume();
	if(PhysicsVolume != DefVol)
	{
		NxVec3 nDeltaGrav = U2NPosition(PhysicsVolume->Gravity - DefVol->Gravity);

		for(INT i=0; i<AssetInst->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInst = AssetInst->Bodies(i);
			check(BodyInst);

			NxActor* nActor = BodyInst->GetNxActor();
			if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
			{
				nActor->addForce( nActor->getMass() * nDeltaGrav );
			}
		}
	}
#endif
}

void AActor::UpdateRBPhysKinematicData()
{
#ifdef WITH_NOVODEX
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>( Components(ComponentIndex) );
		if(PrimComp)
			PrimComp->UpdateRBKinematicData();
	}
#endif // WITH_NOVODEX
}

///////////////// TERRAIN /////////////////

/** For a given TerrainComponent, generate a vertex buffer of vertices in 'component' (local) space. */
static void GetTerrainVerts( ATerrain* Terrain, UTerrainComponent* TerrainComp, TArray<FVector>& TerrainVerts )
{
	FVector TerrainScale = Terrain->DrawScale * Terrain->DrawScale3D;

	INT NumVertsX = TerrainComp->SectionSizeX + 1;
	INT NumVertsY = TerrainComp->SectionSizeY + 1;
	INT NumVerts = NumVertsX * NumVertsY;

	for(INT VertY = 0; VertY < TerrainComp->SectionSizeY + 1; VertY++)
	{
		for(INT VertX = 0; VertX < TerrainComp->SectionSizeX + 1; VertX++)
		{
			FVector LocalV;
			LocalV.X = VertX; // In local space, each patch is of size '1.0'. Draw scale then makes it larger.
			LocalV.Y = VertY;

			FLOAT TerrainHeight = Terrain->Height( TerrainComp->SectionBaseX + VertX, TerrainComp->SectionBaseY + VertY );
			LocalV.Z = (-32768.0f + TerrainHeight) * TERRAIN_ZSCALE;

			TerrainVerts.AddItem( LocalV * (U2PScale * TerrainScale) );
		}
	}

}

static void GetTerrainIndices( UTerrainComponent* TerrainComp, TArray<INT>& TerrainIndices )
{
	INT NumVertsX = TerrainComp->SectionSizeX + 1;

	for(INT QuadY = 0; QuadY < TerrainComp->SectionSizeY; QuadY++)
	{
		for(INT QuadX = 0; QuadX < TerrainComp->SectionSizeX; QuadX++)
		{
			TerrainIndices.AddItem( ((QuadY+0) * NumVertsX) + (QuadX+0) );
			TerrainIndices.AddItem( ((QuadY+0) * NumVertsX) + (QuadX+1) );
			TerrainIndices.AddItem( ((QuadY+1) * NumVertsX) + (QuadX+1) );

			TerrainIndices.AddItem( ((QuadY+0) * NumVertsX) + (QuadX+0) );
			TerrainIndices.AddItem( ((QuadY+1) * NumVertsX) + (QuadX+1) );
			TerrainIndices.AddItem( ((QuadY+1) * NumVertsX) + (QuadX+0) );
		}
	}
}

/**
 *	Special version of InitRBPhys that creates Rigid Body collision for each terrain component. 
 *	We assume terrain is never going have physics (!).
 */
void ATerrain::InitRBPhys()
{
#ifdef WITH_NOVODEX
	DOUBLE StartTime = appSeconds();
		
	for(INT i=0; i<TerrainComponents.Num(); i++)
	{
		UTerrainComponent* TerrainComp = TerrainComponents(i); 

		// Make transform for this terrain component NxActor
		FMatrix TerrainCompTM = TerrainComp->LocalToWorld;
		TerrainCompTM.RemoveScaling();

		NxMat34 nTerrainCompTM = U2NTransform(TerrainCompTM);

		// Make vertex/index buffer for terrain component.
		TArray<FVector> TerrainVerts;
		GetTerrainVerts( this, TerrainComp, TerrainVerts );

		TArray<INT> TerrainIndices;
		GetTerrainIndices( TerrainComp, TerrainIndices );

		// Create Novodex mesh descriptor and fill it in.
		NxTriangleMeshDesc TerrainDesc;

		TerrainDesc.numVertices = TerrainVerts.Num();
		TerrainDesc.pointStrideBytes = sizeof(FVector);
		TerrainDesc.points = TerrainVerts.GetData();

		TerrainDesc.numTriangles = TerrainIndices.Num()/3;
		TerrainDesc.triangleStrideBytes = 3*sizeof(INT);
		TerrainDesc.triangles = TerrainIndices.GetData();

		TerrainDesc.flags = 0;

		// Create Novodex mesh from that info.
		NxTriangleMesh* TerrainTriMesh = GNovodexSDK->createTriangleMesh(TerrainDesc);

		NxTriangleMeshShapeDesc TerrainShapeDesc;
		TerrainShapeDesc.meshData = TerrainTriMesh;
		TerrainShapeDesc.meshFlags = 0;
		TerrainShapeDesc.materialIndex = GEngine->FindMaterialIndex( UPhysicalMaterial::StaticClass() );

		// JTODO: Per-triangle materials for terrain...

		// Create actor description and instance it.
		NxActorDesc TerrainActorDesc;
		TerrainActorDesc.shapes.pushBack(&TerrainShapeDesc);
		TerrainActorDesc.globalPose = nTerrainCompTM;

		// Create the actual NxActor using the mesh collision shape.
		NxActor* TerrainActor = GetLevel()->NovodexScene->createActor(TerrainActorDesc);

		// Then create an RB_BodyInstance for this terrain component and store a pointer to the NxActor in it.
		TerrainComp->BodyInstance = ConstructObject<URB_BodyInstance>( URB_BodyInstance::StaticClass(), GetLevel(), NAME_None, RF_Transactional );

		TerrainComp->BodyInstance->BodyData = (Fpointer)TerrainActor;
	}

	DOUBLE TotalTime = appSeconds() - StartTime;
	debugf( TEXT("Novodex Terrain Creation (%s): %f ms"), GetName(), TotalTime * 1000.f );
#endif // WITH_NOVODEX
}

void ATerrain::TermRBPhys()
{
	for(INT i = 0; i < TerrainComponents.Num(); i++)
	{
		UTerrainComponent* TerrainComp = TerrainComponents(i);

		if(TerrainComp && TerrainComp->BodyInstance)
		{
			TerrainComp->BodyInstance->TermBody();
			delete TerrainComp->BodyInstance;
			TerrainComp->BodyInstance = NULL;
		}
	}
}

///////////////// PAWN /////////////////

// Create/destroy phantom that is used to push rigid body actors around.
void APawn::SetPushesRigidBodies( UBOOL NewPushes )
{
	bPushesRigidBodies = NewPushes;
}

void APawn::execSetPushesRigidBodies( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(NewPushes);
	P_FINISH;

	SetPushesRigidBodies(NewPushes);
}

void APawn::InitRBPhys()
{
	Super::InitRBPhys();
	SetPushesRigidBodies(bPushesRigidBodies);
}

void APawn::TermRBPhys()
{
	Super::TermRBPhys();
	SetPushesRigidBodies(false);
}

void APawn::execInitRagdoll( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	if(bDeleteMe)
	{
		debugf( TEXT("InitRagdoll: Pawn (%s) is deleted!"), GetName() );
		return;
	}

	if(!Mesh || !Mesh->PhysicsAsset)
	{
		*(DWORD*)Result = 0;
	}
	else
	{
		if(Mesh->Owner != this)
		{
			debugf( TEXT("InitRagdoll: SkeletalMeshComponent.Owner (%x) is not the Pawn (%x)"), Mesh->Owner, this );
			return;
			
		}

		//@fixme - should ragdolls really ignore out of world?
		bIgnoreOutOfWorld = 1;

		// initialize physics/etc
		CollisionComponent = Mesh;
		Mesh->InitArticulated();
		Mesh->PhysicsWeight = 1.0f;
		Mesh->bIgnoreControllers=true;
		Mesh->UpdateSpaceBases();
		setPhysics(PHYS_Articulated);
		Mesh->WakeRigidBody();

		*(DWORD*)Result = 1;
	}
}

///////////////// KACTOR /////////////////

static FRigidBodyState SavedRBState;

void AKActor::PreNetReceive()
{
	SavedRBState = RBState;
}

void AKActor::PostNetReceive()
{
#if 0
	// If its the replicated RB state that has changed.
	if( Role != ROLE_Authority &&
		SavedRBState.Position != RBstate.Position ||
		SavedRBState.Quaternion != RBstate.Quaternion ||
		SavedRBState.LinVel != RBstate.LinVel ||
		SavedRBState.AngVel != RBstate.AngVel )
	{

	}
#endif
}

///////////////// RB_THRUSTER /////////////////

UBOOL ARB_Thruster::Tick( FLOAT DeltaTime, ELevelTick TickType )
{
	UBOOL bTicked = Super::Tick(DeltaTime, TickType);
	if(bTicked)
	{
		// Applied force to the base, so if we don't have one, do nothing.
		if(bThrustEnabled && Base)
		{
			FMatrix ActorTM = LocalToWorld();
			FVector WorldForce = ThrustStrength * ActorTM.TransformNormal( FVector(-1,0,0) );

			// Skeletal case.
			if(BaseSkelComponent)
			{
				BaseSkelComponent->AddForce(WorldForce, Location, BaseBoneName);
			}
			// Single-body case.
			else if(Base->CollisionComponent)
			{
				Base->CollisionComponent->AddForce(WorldForce, Location);
			}
		}
	}
	return bTicked;
}

///////////////// RB_LINEIMPULSEACTOR /////////////////

/** Used to keep the arrow graphic updated with the impulse line check length. */
void ARB_LineImpulseActor::PostEditChange(UProperty* PropetyThatChanged)
{
	Super::PostEditChange(PropetyThatChanged);

	Arrow->ArrowSize = ImpulseRange/48.f;
}

/** Do a line check and apply an impulse to anything we hit. */
void ARB_LineImpulseActor::FireLineImpulse()
{
	FMatrix ActorTM = LocalToWorld();
	FVector ImpulseDir = ActorTM.TransformNormal( FVector(1,0,0) );

	// Do line check, take the first result and apply impulse to it.
	if(bStopAtFirstHit)
	{
		FCheckResult Hit(1.f);
		UBOOL bHit = !GetLevel()->SingleLineCheck( Hit, this, Location + (ImpulseRange * ImpulseDir), Location, TRACE_Actors, FVector(0.f) );
		if(bHit)
		{
			check(Hit.Component);
			Hit.Component->AddImpulse( ImpulseDir * ImpulseStrength, Hit.Location, Hit.BoneName, bVelChange );
		}
	}
	// Do the line check, find all Actors along length of line, and apply impulse to all of them.
	else
	{
		FMemMark Mark(GMem);
		FCheckResult* FirstHit = GetLevel()->MultiLineCheck
			(
			GMem,
			Location + (ImpulseRange * ImpulseDir),
			Location,
			FVector(0,0,0),
			Level,
			TRACE_Actors,
			this
			);

		// Iterate over each thing we hit, adding an impulse to the components we hit.
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			check(Check->Component);
			Check->Component->AddImpulse( ImpulseDir * ImpulseStrength, Check->Location, Check->BoneName, bVelChange );
		}

		Mark.Pop();
	}
}

/** UnrealScript exposed version of FireLineImpulse function. */
void ARB_LineImpulseActor::execFireLineImpulse( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	FireLineImpulse();
}
