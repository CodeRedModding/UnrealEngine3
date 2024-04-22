/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "UnSkeletalRender.h"
#include "UnTerrain.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#include "UnSoftBodySupport.h"
#endif // WITH_NOVODEX

#include "NvApexManager.h"
#if WITH_APEX
#include "NvApexScene.h"
#endif

IMPLEMENT_CLASS(URB_RadialImpulseComponent);
IMPLEMENT_CLASS(URB_Handle);
IMPLEMENT_CLASS(URB_Spring);

/** Util for printing out how long each InitArticulated call takes. */
//#define SHOW_INITARTICULATED_TIME

#if PERF_SHOW_PHYS_INIT_COSTS
extern DOUBLE TotalTerrainTime;
extern DOUBLE TotalCreateActorTime;
extern DOUBLE TotalPerTriStaticMeshTime;
extern DOUBLE TotalInitArticulatedTime;
extern DOUBLE TotalConstructBodyInstanceTime;
extern DOUBLE TotalInitBodyTime;
#endif

#if WITH_NOVODEX
static void AddRadialImpulseToBody(NxActor* Actor, const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
	if (!Actor)
		return;

	NxVec3 nCOMPos = Actor->getCMassGlobalPosition(); // center of mass in world space
	NxVec3 nOrigin = U2NPosition(Origin); // origin of radial impulse, in world space

	NxVec3 nDelta = nCOMPos - nOrigin; // vector from origin to COM
	FLOAT Mag = nDelta.magnitude() * P2UScale; // Distance from COM to origin, in Unreal scale
	
	// If COM is outside radius, do nothing.
	if (Mag > Radius)
		return;

	nDelta.normalize();

	// Scale by U2PScale here, because units are velocity * mass. 
	FLOAT ImpulseMag = Strength * U2PScale;
	if (Falloff == RIF_Linear)
	{
		ImpulseMag *= (1.0f - (Mag / Radius));
	}

	NxVec3 nImpulse = nDelta * ImpulseMag;
	if(bVelChange)
	{
		addForce(Actor,nImpulse,(NX_VELOCITY_CHANGE));
	}
	else
	{
		addForce(Actor,nImpulse,(NX_IMPULSE));
	}
	Actor->wakeUp();
}

static void AddRadialForceToBody(NxActor* Actor, const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff)
{
	if (!Actor)
	{
		return;
	}

	NxVec3 nCOMPos = Actor->getCMassGlobalPosition(); // center of mass in world space
	NxVec3 nOrigin = U2NPosition(Origin); // origin of force, in world space

	NxVec3 nDelta = nCOMPos - nOrigin; // vector from origin to COM
	FLOAT Mag = nDelta.magnitude() * P2UScale; // Distance from COM to origin, in Unreal scale
	// If COM is outside radius, do nothing.
	if (Mag > Radius)
		return;

	nDelta.normalize();

	// If using lienar falloff, scale with distance.
	FLOAT ForceMag = Strength;
	if (Falloff == RIF_Linear)
	{
		ForceMag *= (1.0f - (Mag / Radius));
	}

	// Apply force
	NxVec3 nForce = nDelta * ForceMag;
	addForce(Actor,nForce, NX_FORCE);
}

static void AddForceFieldToBody(NxActor* Actor, FForceApplicator* Applicator)
{
	if (!Actor)
	{
		return;
	}

	NxVec3 nCOMPos = Actor->getCMassGlobalPosition(); // center of mass in world space
	FVector COMPos = N2UPosition(nCOMPos);

	NxVec3 nVelocity = Actor->getLinearVelocity();
	FVector Velocity = N2UVectorCopy(nVelocity);

	FVector ResultForce = FVector(0.0f);
	FVector ResultTorque = FVector(0.0f);

	if( Applicator->ComputeForce(
		&COMPos, sizeof(FVector), 1.0f,
		&Velocity, sizeof(FVector), 1.0f,
		&ResultForce, sizeof(FVector), 1.0f,
		&ResultTorque, sizeof(FVector), 1.0f,
		1, FBox(FVector(-BIG_NUMBER), FVector(BIG_NUMBER)) ))
	{
		Actor->addForce(U2NVectorCopy(ResultForce), NX_FORCE);
		Actor->addTorque(U2NVectorCopy(ResultTorque), NX_FORCE);
	}
}

#if !NX_DISABLE_CLOTH
static void AddForceFieldToCloth(NxCloth* Cloth, FForceApplicator* Applicator, FLOAT ClothForceScale)
{

	if (!Cloth)
	{
		return;
	}

	NxMeshData	meshData	= Cloth->getMeshData();
	INT NumClothVertices = (INT)*meshData.numVerticesPtr;

	if(NumClothVertices <= 0)
	{
		return;
	}

	NxClothMesh* ClothMesh = Cloth->getClothMesh();
	NxScene* ClothScene = &(Cloth->getScene());

	/* Setup buffers */
	BYTE* CurrClothPosition = (BYTE *)meshData.verticesPosBegin;
	INT PositionStride = meshData.verticesPosByteStride;

	// Cloth velocity buffer opt
	FRBPhysScene * Scene = GWorld->RBPhysScene;
	check(Scene);
	check(sizeof(FVector) >= sizeof(NxVec3));
	Scene->ClothVelocityScratch.Reserve( 2*NumClothVertices );
	NxVec3* ClothVelocities = (NxVec3 *)Scene->ClothVelocityScratch.GetTypedData();
	NxVec3* NewClothVelocities = (NxVec3 *)(Scene->ClothVelocityScratch.GetTypedData()+NumClothVertices);

	Cloth->getVelocities(ClothVelocities, sizeof(NxVec3));
	appMemset(NewClothVelocities, 0, NumClothVertices * sizeof(NxVec3));

	//Get timing params
	NxReal dt;
	NxU32 MaxIter;
	NxTimeStepMethod Method;
	NxU32 NumSubSteps;
	
	//use timestep from last frame, is this OK(cloth seems to do this internally)
	ClothScene->getTiming(dt, MaxIter, Method, &NumSubSteps);
	
	//Get bounding box

	NxBounds3 ClothWorldBounds;
	Cloth->getWorldBounds(ClothWorldBounds);
	FBox ClothWorldBox(N2UPosition(ClothWorldBounds.min), N2UPosition(ClothWorldBounds.max));

	//Do the force computation
	UBOOL NonZero = FALSE;

	if( Applicator->ComputeForce(
		(FVector *)CurrClothPosition, PositionStride, P2UScale,
		(FVector *)ClothVelocities, sizeof(FVector), 1.0f,
		(FVector *)NewClothVelocities, sizeof(FVector), ClothForceScale * dt,
		0, 0, 0,
		NumClothVertices, ClothWorldBox ))
	{
		#if SUPPORT_DOUBLE_BUFFERING
		// Combination of getVelocity and setVelocity is problematic when double buffered
		// -> use addForceAtVertex
		if (Scene->UsingBufferedScene)
		{
			for(INT i=0; i<NumClothVertices; i++)
			{
				Cloth->addForceAtVertex(NewClothVelocities[i], i, NX_VELOCITY_CHANGE);
			}
		}
		else
		#endif
		{
			for(INT i=0; i<NumClothVertices; i++)
			{
				NewClothVelocities[i] += ClothVelocities[i];
			}

			Cloth->wakeUp();
			Cloth->setVelocities(NewClothVelocities, sizeof(NxVec3));
		}
	}
}
#endif	// !NX_DISABLE_CLOTH

#endif // WITH_NOVODEX

//////////////// PRIMITIVE COMPONENT ///////////////

void UPrimitiveComponent::WakeRigidBody(FName BoneName)
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor(BoneName);
	if (Actor)
		Actor->wakeUp();
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execWakeRigidBody( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	WakeRigidBody(BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execWakeRigidBody);

void UPrimitiveComponent::PutRigidBodyToSleep(FName BoneName)
{
#if WITH_NOVODEX
	
	NxActor* Actor = GetNxActor(BoneName);
	if (Actor)
	{
		debugfSlow(TEXT("Putting %s to sleep"),*this->GetName());
		Actor->putToSleep();
		if ( Actor->isSleeping() )
		{
			debugfSlow(TEXT("Actor is asleep"));
		}
	}
#endif // WITH_NOVODEX
}
void UPrimitiveComponent::execPutRigidBodyToSleep( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;
	PutRigidBodyToSleep(BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execPutRigidBodyToSleep);


///////

void UPrimitiveComponent::SetBlockRigidBody(UBOOL bNewBlockRigidBody)
{
#if WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if(Actor)
	{
		URB_BodySetup* Setup = GetRBBodySetup();

		// Never allow collision if bNoCollision flag is set in BodySetup.
		if((Setup && Setup->bNoCollision) || !bNewBlockRigidBody)
		{
			Actor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
		}
		else
		{
			Actor->clearActorFlag( NX_AF_DISABLE_COLLISION );
		}
	}
#endif

	BlockRigidBody = bNewBlockRigidBody;
}

void UPrimitiveComponent::execSetBlockRigidBody(FFrame& Stack, RESULT_DECL)
{
	P_GET_UBOOL(bNewBlockRigidBody);
	P_FINISH;
	
	SetBlockRigidBody(bNewBlockRigidBody);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetBlockRigidBody);


/** Indicates  */
void UPrimitiveComponent::SetNotifyRigidBodyCollision(UBOOL bNewNotifyRigidBodyCollision)
{
#if WITH_NOVODEX
	if (BodyInstance != NULL)
	{
		NxActor* nActor = BodyInstance->GetNxActor();
		if (nActor != NULL)
		{
			nActor->setGroup(bNewNotifyRigidBodyCollision ? UNX_GROUP_NOTIFYCOLLIDE : UNX_GROUP_DEFAULT);
		}
	}
#endif

	bNotifyRigidBodyCollision = bNewNotifyRigidBodyCollision;
}

void UPrimitiveComponent::execSetNotifyRigidBodyCollision(FFrame& Stack, RESULT_DECL)
{
	P_GET_UBOOL(bNewNotifyRigidBodyCollision);
	P_FINISH;

	SetNotifyRigidBodyCollision(bNewNotifyRigidBodyCollision);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetNotifyRigidBodyCollision);

void UPrimitiveComponent::execInitRBPhys(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	if (IsAttached() && GetOwner() != NULL)
	{
		if (GetOwner()->Physics == PHYS_RigidBody && GetOwner()->CollisionComponent != this)
		{
			debugf(NAME_Warning, TEXT("InitRBPhys(): Can't init physics for component attached to PHYS_RigidBody Actor unless it is the CollisionComponent"));
		}
		else
		{
			InitComponentRBPhys(GetOwner()->Physics != PHYS_RigidBody);
		}
	}
}

/** 
 *	Changes the current PhysMaterialOverride for this component. 
 *	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its 
 *	surface properties like friction and the damping.
 */
void UPrimitiveComponent::SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial)
{
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call SetPhysMaterialOverride() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		return;
	}
#endif

	// Save ref to PhysicalMaterial
	PhysMaterialOverride = NewPhysMaterial;

#if WITH_NOVODEX
	if( BodyInstance != NULL )
	{
		// Go through the chain of physical materials and update the NxActor
		BodyInstance->UpdatePhysMaterialOverride();
	}
#endif // WITH_NOVODEX
}

/** Script handler for SetPhysMaterialOverride. */
void UPrimitiveComponent::execSetPhysMaterialOverride(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UPhysicalMaterial,NewPhysMaterial);
	P_FINISH;

	SetPhysMaterialOverride(NewPhysMaterial);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetPhysMaterialOverride);

/** 
 *	Used for creating one-way physics interactions.
 *	@see RBDominanceGroup
 */
void UPrimitiveComponent::SetRBDominanceGroup(BYTE InDomGroup)
{
	BYTE UseGroup = Clamp<BYTE>(InDomGroup, 0, 31);

#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
		nActor->setDominanceGroup(UseGroup);
	}
#endif

	RBDominanceGroup = InDomGroup;
}

void UPrimitiveComponent::execSetRBDominanceGroup(FFrame& Stack, RESULT_DECL)
{
	P_GET_BYTE(InDomGroup);
	P_FINISH;

	SetRBDominanceGroup(InDomGroup);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBDominanceGroup);

URB_BodyInstance* UPrimitiveComponent::GetRootBodyInstance()
{
	return (BodyInstance != NULL && BodyInstance->IsValidBodyInstance()) ? BodyInstance : NULL;
}

void UPrimitiveComponent::execGetRootBodyInstance(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	*(URB_BodyInstance**)Result = GetRootBodyInstance();
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execGetRootBodyInstance);

///////

void UPrimitiveComponent::SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if (nActor)
	{
		NxVec3 nNewVel = U2NPosition(NewVel);

		if(bAddToCurrent)
		{
			NxVec3 nOldVel = nActor->getLinearVelocity();
			nNewVel += nOldVel;
		}

		setLinearVelocity(nActor,nNewVel);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execSetRBLinearVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewVel);
	P_GET_UBOOL_OPTX(bAddToCurrent, FALSE);
	P_FINISH;

	SetRBLinearVelocity(NewVel, bAddToCurrent);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBLinearVelocity);

///////

void UPrimitiveComponent::SetRBAngularVelocity(const FVector& NewAngVel, UBOOL bAddToCurrent)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if (nActor)
	{
		NxVec3 nNewAngVel = U2NVectorCopy(NewAngVel);

		if(bAddToCurrent)
		{
			NxVec3 nOldAngVel = nActor->getAngularVelocity();
			nNewAngVel += nOldAngVel;
		}

		nActor->setAngularVelocity(nNewAngVel);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execSetRBAngularVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewAngVel);
	P_GET_UBOOL_OPTX(bAddToCurrent, FALSE);
	P_FINISH;

	SetRBAngularVelocity(NewAngVel, bAddToCurrent);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBAngularVelocity);

//////

void UPrimitiveComponent::RetardRBLinearVelocity(const FVector& RetardDir, FLOAT VelScale)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if (nActor)
	{
		// Ensure within range
		VelScale = Clamp(VelScale, 0.f, 1.f);

		FVector BodyVel = N2UPosition(nActor->getLinearVelocity());
		FLOAT AlongDirMag = BodyVel | RetardDir;

		if(AlongDirMag > 0.f)
		{
			// Subtract component in RetardDir
			BodyVel -= (VelScale * AlongDirMag * RetardDir);
			// And apply to physics body
			setLinearVelocity(nActor, U2NPosition(BodyVel));
		}
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execRetardRBLinearVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(RetardDir);
	P_GET_FLOAT(VelScale);
	P_FINISH;

	RetardRBLinearVelocity(RetardDir, VelScale);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execRetardRBLinearVelocity);


///////

void UPrimitiveComponent::SetRBPosition(const FVector& NewPos, FName BoneName)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor(BoneName);
	if (nActor != NULL)
	{
		nActor->setGlobalPosition(U2NPosition(NewPos));

		// Force a physics update now for the owner, to avoid a 1-frame lag when teleporting before the graphics catches up.
		if (Owner != NULL && Owner->Physics == PHYS_RigidBody)
		{
			Owner->SyncActorToRBPhysics();
		}
	}
#endif
}

void UPrimitiveComponent::SetRBRotation(const FRotator& NewRot, FName BoneName)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor(BoneName);
	if (nActor != NULL)
	{
		nActor->setGlobalOrientationQuat(U2NQuaternion(NewRot.Quaternion()));

		// Force a physics update now for the owner, to avoid a 1-frame lag when teleporting before the graphics catches up.
		if (Owner != NULL && Owner->Physics == PHYS_RigidBody)
		{
			Owner->SyncActorToRBPhysics();
		}
	}
#endif
}

void UPrimitiveComponent::execSetRBPosition( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(NewPos);
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	SetRBPosition(NewPos, BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBPosition);

void UPrimitiveComponent::execSetRBRotation(FFrame& Stack, RESULT_DECL)
{
	P_GET_ROTATOR(NewRot);
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	SetRBRotation(NewRot, BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execSetRBRotation);


///////

UBOOL UPrimitiveComponent::RigidBodyIsAwake(FName BoneName)
{
#if WITH_NOVODEX
	NxActor* Actor = NULL;
	
	Actor = GetNxActor(BoneName);

	if (Actor && !Actor->isSleeping())
		return TRUE;
	else
		return FALSE;
#else // WITH_NOVODEX

	return FALSE;
#endif
}

void UPrimitiveComponent::execRigidBodyIsAwake( FFrame& Stack, RESULT_DECL )
{
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	*(UBOOL*)Result = RigidBodyIsAwake(BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execRigidBodyIsAwake);

///////

void UPrimitiveComponent::AddImpulse(FVector Impulse, FVector Position, FName BoneName, UBOOL bVelChange)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor(BoneName);
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddImpulse() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		NxVec3 nImpulse = U2NPosition(Impulse);

		if (Position.IsZero())
		{
			if(bVelChange)
			{
				addForce(nActor,nImpulse,(NX_VELOCITY_CHANGE));
			}
			else
			{
				addForce(nActor,nImpulse,(NX_IMPULSE));
			}
		}
		else
		{
			NxVec3 nPosition = U2NPosition(Position);

			if(bVelChange)
			{
				nActor->addForceAtPos(nImpulse, nPosition, NX_VELOCITY_CHANGE);
			}
			else
			{
				nActor->addForceAtPos(nImpulse, nPosition, NX_IMPULSE);
			}
		}

		nActor->wakeUp();
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddImpulse( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Impulse);
	P_GET_VECTOR_OPTX(Position, FVector(0,0,0));
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_GET_UBOOL_OPTX(bVelChange, FALSE);
	P_FINISH;

	AddImpulse(Impulse, Position, BoneName, bVelChange);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddImpulse);

void UStaticMeshComponent::AddImpulse(FVector Impulse, FVector Position, FName BoneName, UBOOL bVelChange)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor(BoneName);
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
		Super::AddImpulse(Impulse, Position, BoneName, bVelChange);
	}
	else if ( Owner && Owner->bWorldGeometry && CanBecomeDynamic() )
	{
		Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(this);
		Super::AddImpulse(Impulse, Position, BoneName, bVelChange);
	}
#endif // WITH_NOVODEX
}

void UStaticMeshComponent::AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if( nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
	{
		Super::AddRadialImpulse(Origin, Radius, Strength, Falloff, bVelChange);
	}
	else if ( Owner && Owner->bWorldGeometry && CanBecomeDynamic() )
	{
		Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(this);
		Super::AddRadialImpulse(Origin, Radius, Strength, Falloff, bVelChange);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if( nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddRadialImpulse() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		AddRadialImpulseToBody(nActor, Origin, Radius, Strength, Falloff, bVelChange);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddRadialImpulse( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Origin);
	P_GET_FLOAT(Radius);
	P_GET_FLOAT(Strength);
	P_GET_BYTE(Falloff);
	P_GET_UBOOL_OPTX(bVelChange,FALSE);
	P_FINISH;

	AddRadialImpulse(Origin, Radius, Strength, Falloff, bVelChange);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddRadialImpulse);

void UPrimitiveComponent::AddRadialForce(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if( nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddRadialForce() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		AddRadialForceToBody(nActor, Origin, Radius, Strength, Falloff);
	}
#endif // WITH_NOVODEX
}

void UStaticMeshComponent::AddRadialForce(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();
	if( nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
	{
		Super::AddRadialForce(Origin, Radius, Strength, Falloff);
	}
	else if ( Owner && Owner->bWorldGeometry && CanBecomeDynamic() )
	{
		Cast<AKActorFromStatic>(AKActorFromStatic::StaticClass()->GetDefaultActor())->MakeDynamic(this);
		Super::AddRadialForce(Origin, Radius, Strength, Falloff);
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddRadialForce( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Origin);
	P_GET_FLOAT(Radius);
	P_GET_FLOAT(Strength);
	P_GET_BYTE(Falloff);
	P_FINISH;

	AddRadialForce(Origin, Radius, Strength, Falloff);
}

void UPrimitiveComponent::AddForceField(FForceApplicator* Applicator, const FBox& FieldBoundingBox, UBOOL bApplyToCloth, UBOOL bApplyToRigidBody)
{
	if(bIgnoreForceField)
	{
		return;
	}

#if WITH_NOVODEX

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call AddForceField) on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif

	NxActor* nActor = GetNxActor();
	if( nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) && bApplyToRigidBody)
	{
		AddForceFieldToBody(nActor, Applicator);
	}


#endif // WITH_NOVODEX
}

// ADD FORCE
void UPrimitiveComponent::AddForce(FVector Force, FVector Position, FName BoneName)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor(BoneName);
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddForce() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		NxVec3 nForce = U2NVectorCopy(Force);

		if(Position.IsZero())
		{
			addForce(nActor,nForce);
		}
		else
		{
			NxVec3 nPosition = U2NPosition(Position);
			nActor->addForceAtPos(nForce, nPosition);
		}

		nActor->wakeUp();
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddForce( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Force);
	P_GET_VECTOR_OPTX(Position, FVector(0,0,0));
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	AddForce(Force, Position, BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddForce);

// ADD TORQUE
void UPrimitiveComponent::AddTorque(FVector Torque, FName BoneName)
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor(BoneName);
	if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddTorque() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		NxVec3 nTorque = U2NVectorCopy(Torque);
		nActor->addTorque(nTorque,NX_FORCE);
		nActor->wakeUp();
	}
#endif // WITH_NOVODEX
}

void UPrimitiveComponent::execAddTorque( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Torque);
	P_GET_NAME_OPTX(BoneName, NAME_None);
	P_FINISH;

	AddTorque(Torque, BoneName);
}
IMPLEMENT_FUNCTION(UPrimitiveComponent,INDEX_NONE,execAddTorque);


#if WITH_NOVODEX
NxActor* UPrimitiveComponent::GetNxActor(FName BoneName)
{
	if (BodyInstance)
		return BodyInstance->GetNxActor();
	else
		return NULL;
}

NxActor* UPrimitiveComponent::GetIndexedNxActor(INT BodyIndex)
{
	if (BodyInstance)
		return BodyInstance->GetNxActor();
	else
		return NULL;
}

/** Utility for getting all physics bodies contained within this component. */
void UPrimitiveComponent::GetAllNxActors(TArray<class NxActor*>& OutActors)
{
	NxActor* nActor = GetNxActor();
	if(nActor)
	{
		OutActors.AddItem(nActor);
	}
}

#endif // WITH_NOVODEX

void UPrimitiveComponent::UpdateRBKinematicData()
{
#if WITH_NOVODEX
	NxActor* nActor = GetNxActor();

	if(!nActor || !nActor->isDynamic() || !nActor->readBodyFlag(NX_BF_KINEMATIC) || nActor->readBodyFlag(NX_BF_FROZEN))
	{
		return;
	}
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call UpdateRBKinematicData() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif

	// Synchronize the position and orientation of the rigid body to match the actor.
	FVector FullScale;
	FMatrix PrimTM;
	GetTransformAndScale(PrimTM, FullScale);

#if !FINAL_RELEASE
	const UBOOL bContainsNaN = PrimTM.ContainsNaN();
	if( bContainsNaN == TRUE )
	{
		warnf( TEXT("PrimTM.ContainsNaN(): %s %s"), *Owner->GetName(), *GetName() );
		check(!bContainsNaN && "Check log for the name of the asset having the NaN");
	}
#else
	check(!PrimTM.ContainsNaN());
#endif // FINAL_RELEASE

	NxMat34 nNewPose = U2NTransform(PrimTM);

	// Don't call moveGlobalPose if we are already in the correct pose. 
	// Also check matrix we are passing in is valid.
	NxMat34 nCurrentPose = nActor->getGlobalPose();
	if( !FullScale.IsNearlyZero() && 
		nNewPose.M.determinant() > (FLOAT)KINDA_SMALL_NUMBER && 
		!MatricesAreEqual(nNewPose, nCurrentPose, (FLOAT)KINDA_SMALL_NUMBER) )
	{
		nActor->moveGlobalPose( nNewPose );
	}
#endif // WITH_NOVODEX
}

/** 
 *	Create any physics-engine data needed for this PrimitiveComponent, including creating am RB_BodyInstance if required. 
 *	This will insert the physics object in to the GWorld physics scene. If you wish to add it to a different scene, call InitBody directly.
 */
void UPrimitiveComponent::InitComponentRBPhys(UBOOL bFixed)
{
	if(!GWorld->RBPhysScene || bDisableAllRigidBody)
	{
		return;
	}

	// If we already have a rigid body, do nothing.
	if(!BodyInstance)
	{
	
		URB_BodySetup* BodySetup = GetRBBodySetup();
		if(BodySetup)
		{
			// Create new BodyInstance at given location.
			FVector FullScale;
			FMatrix PrimTM;
			GetTransformAndScale(PrimTM, FullScale);

#if PERF_SHOW_PHYS_INIT_COSTS
			DOUBLE StartConstructInstance = appSeconds();
#endif

			BodyInstance = GWorld->InstanceRBBody(NULL);
			check(BodyInstance);

#if PERF_SHOW_PHYS_INIT_COSTS
			DOUBLE StartInitBody = appSeconds();
#endif

			// Pick the right scene.
			FRBPhysScene* UseScene = GWorld->RBPhysScene;

			// Create the body.
			BodyInstance->InitBody(BodySetup, PrimTM, FullScale, bFixed, this, GWorld->RBPhysScene);

#if PERF_SHOW_PHYS_INIT_COSTS
			TotalConstructBodyInstanceTime += (StartInitBody - StartConstructInstance);
			DOUBLE DeltaConstructTime = (appSeconds() - StartInitBody);
			TotalInitBodyTime += DeltaConstructTime;

			if (FALSE && DeltaConstructTime * 1000.0f > 0.2f)
			{
				debugf(TEXT("UPrimitiveComponent::InitComponentRBPhys %s took %f ms"), *GetFullName(), DeltaConstructTime * 1000.0f);
			}
#endif
		}
	}
}

/** Change the physics information for this UPrimitiveComponent to either allow it to simulate, or to 'lock' it in position. */
void UPrimitiveComponent::SetComponentRBFixed(UBOOL bFixed)
{
	if(BodyInstance)
	{
		BodyInstance->SetFixed(bFixed);
	}
}

/** 
 *	Destroy any physics-engine data held in the RB_BodyInstance for this PrimitiveComponent. 
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene. 
 */
void UPrimitiveComponent::TermComponentRBPhys(FRBPhysScene* InScene)
{
	if(BodyInstance)
	{
		// We tell the BodyInstance to shut down the physics-engine data.
		UBOOL bTerminated = BodyInstance->TermBody(InScene);

		if(bTerminated)
		{
			// Return BodyInstance to pool and stop referencing it
			GWorld->ReturnRBBody(BodyInstance);
			BodyInstance = NULL;
		}
	}
}

//////////////// SKELETAL MESH COMPONENT ///////////////

#if WITH_NOVODEX
/** 
 *	Get a pointer to the Novodex NxActor object associated with this SkeletalMeshComponent. 
 *	In the case of an articulated object, BoneName indicates which bone is wanted.
 *	If this skeletal component is running single-body physics and does not have a PhysicsAssetInstance, 
 *	then this function calls Super::GetNxActor, which will return the single NxActor associated in the BodyInstance.
 */
NxActor* USkeletalMeshComponent::GetNxActor(FName BoneName)
{
	if (PhysicsAssetInstance)
	{
		if (PhysicsAssetInstance->Bodies.Num() == 0)
			return NULL;

		check(PhysicsAsset);

		URB_BodyInstance* BodyInstance = NULL;

		if (BoneName == NAME_None)
		{
			if (PhysicsAssetInstance->RootBodyIndex < 0)
			{	// It would be nice if we could print this once per skeletal-mesh component resource but that seems to 
				// be too difficult to be worthwhile at the moment.  Hopefully my printing the information at
				// initialization-time will be sufficient.  I'll remove the commented-out code soon, but
				// it's useful enough to be worth putting in Perforce at least.
// 				debugf(NAME_Error,
// 					TEXT("Physics Asset Instance on (%s)->(%s).\n\tSkeletal Mesh: %s\n\tPhysicsAsset: %s%s%s"),
// 						*Owner->GetName(), *GetName(),
// 						SkeletalMesh ? *SkeletalMesh->GetName() : TEXT("<NONE>"),
// 						PhysicsAsset ? *PhysicsAsset->GetName() : TEXT("<NONE>"),
// 						TEXT("\n\tNo root body found, cannot fetch Novodex NxActor."),
// 						TEXT("\n\tCheck whether physics and skeletal mesh match names.")
// 					  );
				return NULL;
			}

			// if they didn't specify, return the root body
			BodyInstance = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex);
		}
		else
		{
			INT BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			if (BodyIndex == INDEX_NONE)
			{
				debugf(TEXT("USkeletalMeshComponent::GetNxActor() : Could not find bone: %s"), *BoneName.ToString());
				return NULL;
			}
			BodyInstance = PhysicsAssetInstance->Bodies(BodyIndex);
		}

		check(BodyInstance);
		return BodyInstance->GetNxActor();
	}
	else
	{
		return Super::GetNxActor(BoneName);
	}
}

NxActor* USkeletalMeshComponent::GetIndexedNxActor(INT BodyIndex)
{
	if (PhysicsAssetInstance)
	{
		if (PhysicsAssetInstance->Bodies.Num() == 0)
			return NULL;

		check(PhysicsAsset);

		URB_BodyInstance* BodyInstance = NULL;

		if (BodyIndex == INDEX_NONE)
		{
			// if they didn't specify, return the root body
			BodyInstance = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex);
		}
		else
		{
			BodyInstance = PhysicsAssetInstance->Bodies(BodyIndex);
		}

		check(BodyInstance);
		return BodyInstance->GetNxActor();
	}
	else
	{
		return Super::GetIndexedNxActor(BodyIndex);
	}
}

/** Utility for getting all physics bodies contained within this component. */
void USkeletalMeshComponent::GetAllNxActors(TArray<class NxActor*>& OutActors)
{
	if(PhysicsAssetInstance)
	{
		for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
		{
			NxActor* nActor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
			if(nActor)
			{
				OutActors.AddItem(nActor);
			}
		}
	}
}

FVector USkeletalMeshComponent::NxGetPointVelocity(FVector LocationInWorldSpace)
{
	NxActor* nActor = GetNxActor();
	FVector PointVel(0.f);
	if (nActor)
	{
		PointVel = N2UPosition( nActor->getPointVelocity( U2NVectorCopy(LocationInWorldSpace )) );
	}

	return PointVel;
}

#endif // WITH_NOVODEX


/**
 *	Changes the rigid-body channel that this object is defined in.
 */
void UPrimitiveComponent::SetRBChannel(ERBCollisionChannel Channel)
{
	RBChannel = Channel;

	UpdatePhysicsToRBChannels();
}

/** 
 *	Changes a member of the RBCollideWithChannels container for this PrimitiveComponent.
 */
void UPrimitiveComponent::SetRBCollidesWithChannel(ERBCollisionChannel Channel, UBOOL bNewCollides)
{
	RBCollideWithChannels.SetChannel(Channel, bNewCollides);

	UpdatePhysicsToRBChannels();
}

/** 
 *	Changes the RBCollideWithChannels container for this PrimitiveComponent.
 */
void UPrimitiveComponent::SetRBCollisionChannels(FRBCollisionChannelContainer Channels)
{
	RBCollideWithChannels = Channels;

	UpdatePhysicsToRBChannels();
}

/** Internal function that updates physics objects to match the RBChannel/RBCollidesWithChannel info. */
void UPrimitiveComponent::UpdatePhysicsToRBChannels()
{
#if WITH_NOVODEX
	NxGroupsMask NewMask = CreateGroupsMask(RBChannel, &RBCollideWithChannels);

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call UpdatePhysicsToRBChannels() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif // !FINAL_RELEASE

	NxActor* nActor = GetNxActor();
	if(nActor)
	{
		// Iterate over each shape, setting object.
		INT NumShapes = nActor->getNbShapes();
		NxShape *const * Shapes = nActor->getShapes();
		for(INT i=0; i<NumShapes; i++)
		{
			NxShape* nShape = Shapes[i];
			if(nShape->getType() != NX_SHAPE_WHEEL)
			{
				nShape->setGroupsMask(NewMask);
			}
		}
	}

#endif // WITH_NOVODEX
}

/** Internal function that updates physics objects to match the RBChannel/RBCollidesWithChannel info. */
void USkeletalMeshComponent::UpdatePhysicsToRBChannels()
{
	if(bUseSingleBodyPhysics)
	{
		Super::UpdatePhysicsToRBChannels();
		return;
	}

#if WITH_NOVODEX
	NxGroupsMask NewMask = CreateGroupsMask(RBChannel, &RBCollideWithChannels);

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call UpdatePhysicsToRBChannels() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif // !FINAL_RELEASE
	
	if (PhysicsAssetInstance != NULL)
	{
		// Iterate over each bone/body.
		for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* nActor = BodyInstance->GetNxActor();
			if (nActor)
			{
				// Iterate over each shape, setting object.
				INT NumShapes = nActor->getNbShapes();
				NxShape *const * Shapes = nActor->getShapes();
				for(INT j=0; j<NumShapes; j++)
				{
					NxShape* nShape = Shapes[j];
					if(nShape->getType() != NX_SHAPE_WHEEL)
					{
						nShape->setGroupsMask(NewMask);
					}
				}	
			}
		}
	}
#endif // WITH_NOVODEX
}

/** Script version of SetRBCollidesWithChannel */
void UPrimitiveComponent::execSetRBCollidesWithChannel( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE(Channel);
	P_GET_UBOOL(bNewCollides);
	P_FINISH;

	SetRBCollidesWithChannel((ERBCollisionChannel)Channel, bNewCollides);
}

/** Script version of SetRBCollisionChannels */
void UPrimitiveComponent::execSetRBCollisionChannels( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FRBCollisionChannelContainer, Channels);
	P_FINISH;

	SetRBCollisionChannels(Channels);
}

/** Script version of SetRBChannel */
void UPrimitiveComponent::execSetRBChannel( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE(Channel);
	P_FINISH;

	SetRBChannel((ERBCollisionChannel)Channel);
}

/** Force the physics of this skeletal mesh to start simulating. Will do nothing if this object is fixed though. */
void USkeletalMeshComponent::WakeRigidBody(FName BoneName)
{
#if WITH_NOVODEX
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call WakeRigidBody() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif
	if (BoneName == NAME_None && PhysicsAssetInstance)
	{
		check(PhysicsAsset);

		for (int i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* Actor = BodyInstance->GetNxActor();
			if (Actor)
				Actor->wakeUp();
		}
	}
	else
	{
		NxActor* Actor = GetNxActor(BoneName);
		if (Actor)
		{
			Actor->wakeUp();
		}
	}
#endif // WITH_NOVODEX
}

/** Force the physics of this skeletal mesh to start sleeping. Will do nothing if this object is fixed though. */
void USkeletalMeshComponent::PutRigidBodyToSleep(FName BoneName)
{
#if WITH_NOVODEX
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call PutRigidBodyToSleep() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif
	if (BoneName == NAME_None && PhysicsAssetInstance)
	{
		check(PhysicsAsset);

		for (int i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* Actor = BodyInstance->GetNxActor();
			if (Actor)
				Actor->putToSleep();
		}
	}
	else
	{
		NxActor* Actor = GetNxActor(BoneName);
		if (Actor)
		{
			Actor->putToSleep();
		}
	}
#endif // WITH_NOVODEX
}

/** If NAME_None is specified, return TRUE is ANY body is awake. */
UBOOL USkeletalMeshComponent::RigidBodyIsAwake( FName BoneName )
{
	if(bUseSingleBodyPhysics || BoneName != NAME_None)
	{
		return Super::RigidBodyIsAwake(BoneName);
	}

	UBOOL bAwake = FALSE;

#if WITH_NOVODEX
	// If we have a PhysicsAssetInstance..
	if(PhysicsAssetInstance)
	{
		// ..iterate over each body to find any that are awak
		for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BI = PhysicsAssetInstance->Bodies(i);
			check(BI);
			NxActor* nActor = BI->GetNxActor();
			if(nActor && !nActor->isSleeping())
			{
				// Found an awake one - so mesh is considered 'awake'
				bAwake = TRUE;
				continue;
			}
		}
	}
#endif // WITH_NOVODEX

	return bAwake;
}


/** 
 *	Allows you to change whether or not the rigid-body physics of this skeletal mesh will collide with other rigid bodies.
 *	If you set bBlockRigidBody to false, this body will pass through other bodies (including the world) and they will pass through it.
 */
void USkeletalMeshComponent::SetBlockRigidBody(UBOOL bNewBlockRigidBody)
{
#if WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
		check(PhysicsAsset);

#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call SetBlockRigidBody() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		for (int i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* Actor = BodyInstance->GetNxActor();
			if (Actor)
			{
				URB_BodySetup* Setup = PhysicsAsset->BodySetup(i);

				// Never allow collision if bNoCollision flag is set in BodySetup.
				if(Setup->bNoCollision || !bNewBlockRigidBody || bNotUpdatingKinematicDueToDistance)
				{
					Actor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
				}
				else
				{
					Actor->clearActorFlag( NX_AF_DISABLE_COLLISION );
				}			
			}
		}
	}
#endif

	BlockRigidBody = bNewBlockRigidBody;
}

void USkeletalMeshComponent::SetNotifyRigidBodyCollision(UBOOL bNewNotifyRigidBodyCollision)
{
#if WITH_NOVODEX
	if(PhysicsAssetInstance != NULL)
	{
		// Iterate over each bone/body.
		for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			checkSlow(BodyInstance != NULL);

			NxActor* nActor = BodyInstance->GetNxActor();
			if (nActor != NULL)
			{
				nActor->setGroup(bNewNotifyRigidBodyCollision ? UNX_GROUP_NOTIFYCOLLIDE : UNX_GROUP_DEFAULT);
			}
		}
	}
#endif

	bNotifyRigidBodyCollision = bNewNotifyRigidBodyCollision;
}

/** Changes the current PhysMaterialOverride for this component. */
void USkeletalMeshComponent::SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial)
{
	// Single-body case - just use PrimComp code.
	if(bUseSingleBodyPhysics)
	{
		UPrimitiveComponent::SetPhysMaterialOverride(NewPhysMaterial);
		return;
	}

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call SetPhysMaterialOverride() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		return;
	}
#endif

	// Save ref to PhysicalMaterial
	PhysMaterialOverride = NewPhysMaterial;

#if WITH_NOVODEX
	if( PhysicsAssetInstance != NULL )
	{
		// For each body in the physics asset
		for( INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++ )
		{
			URB_BodyInstance* BI = PhysicsAssetInstance->Bodies(i);
			// Go through the chain of physical materials and update the NxActor
			BI->UpdatePhysMaterialOverride();
		}
	}
#endif // WITH_NOVODEX
}

/** 
 *	Used for creating one-way physics interactions.
 *	@see RBDominanceGroup
 */
void USkeletalMeshComponent::SetRBDominanceGroup(BYTE InDomGroup)
{
	// Handle single-body case (eg vehicles)
	if(bUseSingleBodyPhysics)
	{
		Super::SetRBDominanceGroup(InDomGroup);
		return;
	}

	BYTE UseGroup = Clamp<BYTE>(InDomGroup, 0, 31);

#if WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
		for(INT BodyIndex=0; BodyIndex<PhysicsAssetInstance->Bodies.Num(); BodyIndex++)
		{
			URB_BodyInstance* BI = PhysicsAssetInstance->Bodies(BodyIndex);
			NxActor* nActor = BI->GetNxActor();
			if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
			{
				nActor->setDominanceGroup(UseGroup);
			}
		}
	}
#endif

	RBDominanceGroup = InDomGroup;
}

URB_BodyInstance* USkeletalMeshComponent::GetRootBodyInstance()
{
	if (bUseSingleBodyPhysics)
	{
		return Super::GetRootBodyInstance();
	}
	else if ( PhysicsAssetInstance != NULL && PhysicsAssetInstance->Bodies.IsValidIndex(PhysicsAssetInstance->RootBodyIndex) &&
				PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex)->IsValidBodyInstance() )
	{
		return PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex);
	}
	else
	{
		return NULL;
	}
}

void USkeletalMeshComponent::SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent)
{
#if WITH_NOVODEX
	if(bUseSingleBodyPhysics)
	{
		Super::SetRBLinearVelocity(NewVel, bAddToCurrent);
	}
	else if(PhysicsAssetInstance)
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call SetRBLinearVelocity() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* nActor = BodyInstance->GetNxActor();
			if (nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
			{
				NxVec3 nNewVel = U2NPosition(NewVel);

				if(bAddToCurrent)
				{
					NxVec3 nOldVel = nActor->getLinearVelocity();
					nNewVel += nOldVel;
				}

				setLinearVelocity(nActor,nNewVel);
			}
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::SetRBAngularVelocity(const FVector& NewAngVel, UBOOL bAddToCurrent)
{
#if WITH_NOVODEX
	if(bUseSingleBodyPhysics)
	{
		Super::SetRBAngularVelocity(NewAngVel, bAddToCurrent);
	}
	else if(PhysicsAssetInstance)
	{
		check(PhysicsAsset);

#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call SetRBAngularVelocity() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		// Find the root actor. We use its location as the center of the rotation.
		NxActor* nRootActor = NULL; // Find correct root actor below only if we have a valid index.

		if (PhysicsAssetInstance->RootBodyIndex < 0) // This can happen if no root body was found, such as when the skeletal mesh and physics don't match.
		{
			// Ideally we'd only print this once per SkeletalMeshComponent (or even per component with a particular resource pair if possible).
			// Failing that, this is still OK unless it starts spamming us a lot, in which case we may have to remove it.
			debugf(NAME_Error,
				TEXT("Physics Asset Instance on (%s)->(%s).\n\tSkeletal Mesh: %s\n\tPhysicsAsset: %s%s%s"),
					*Owner->GetName(), *GetName(),
					SkeletalMesh ? *SkeletalMesh->GetName() : TEXT("<NONE>"),
					PhysicsAsset ? *PhysicsAsset->GetName() : TEXT("<NONE>"),
					TEXT("\n\tNo root body found; can't SetRBAngularVelocity."),
					TEXT("\n\tCheck whether physics and skeletal mesh match names.")
					);
		}
		else
		{
			URB_BodyInstance* RootBodyInst = PhysicsAssetInstance->Bodies( PhysicsAssetInstance->RootBodyIndex );
			nRootActor = RootBodyInst->GetNxActor();
		}

		if(nRootActor)
		{
			NxVec3 nRootPos = nRootActor->getGlobalPosition();

			// Iterate over each bone, updating its velocity
			for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
			{
				URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
				check(BodyInstance);

				NxActor* nActor = BodyInstance->GetNxActor();
				if (nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
				{
					NxVec3 nNewAngVel = U2NVectorCopy(NewAngVel);

					// Calculate the linear velocity necessary on this body to get the whole asset to rotate around its root position.
					NxVec3 nActorCOMPos = nActor->getCMassGlobalPosition();
					NxVec3 nCOMRelPos = nActorCOMPos - nRootPos;
					NxVec3 nRotVel = nNewAngVel.cross(nCOMRelPos);

					if(bAddToCurrent)
					{
						NxVec3 nOldAngVel = nActor->getAngularVelocity();
						nNewAngVel += nOldAngVel;

						NxVec3 nOldLinVel = nActor->getLinearVelocity();
						nRotVel += nOldLinVel;
					}

					nActor->setAngularVelocity(nNewAngVel);
					setLinearVelocity(nActor,nRotVel);
				}
			}
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::RetardRBLinearVelocity(const FVector& RetardDir, FLOAT VelScale)
{
#if WITH_NOVODEX
	if(bUseSingleBodyPhysics)
	{
		Super::RetardRBLinearVelocity(RetardDir, VelScale);
	}
	else if(PhysicsAssetInstance)
	{
		check(PhysicsAsset);

#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call RetardRBLinearVelocity() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif

		// Ensure within range
		VelScale = Clamp(VelScale, 0.f, 1.f);

		// Iterate over each bone, updating its velocity
		for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInstance = PhysicsAssetInstance->Bodies(i);
			check(BodyInstance);

			NxActor* nActor = BodyInstance->GetNxActor();
			if (nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC))
			{
				FVector BodyVel = N2UPosition(nActor->getLinearVelocity());
				FLOAT AlongDirMag = BodyVel | RetardDir;

				if(AlongDirMag > 0.f)
				{
					// Subtract component in RetardDir
					BodyVel -= (VelScale * AlongDirMag * RetardDir);
					// And apply to physics body
					setLinearVelocity(nActor,U2NPosition(BodyVel));
				}
			}
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::SetRBPosition(const FVector& NewPos, FName BoneName)
{
	if (BoneName != NAME_None || bUseSingleBodyPhysics)
	{
		Super::SetRBPosition(NewPos, BoneName);
	}
	else if (PhysicsAssetInstance == NULL)
	{
		debugf(NAME_Warning, TEXT("USkeletalMeshComponent::SetRBPosition(): no PhysicsAssetInstance for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else
	{
#if WITH_NOVODEX
		// calculate the deltas to get the root body to NewPos
		URB_BodyInstance* RootBodyInst = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex);
		if(RootBodyInst->IsValidBodyInstance())
		{
			FMatrix RootBodyMatrix = RootBodyInst->GetUnrealWorldTM();
			FVector DeltaLoc = NewPos - RootBodyMatrix.GetOrigin();
			// move the root body
			NxActor* RootActor = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex)->GetNxActor();
			if (RootActor != NULL)
			{
				RootActor->setGlobalPosition(U2NPosition(NewPos));
#if DO_CHECK
				FVector RelativeVector = (PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex)->GetUnrealWorldTM().GetOrigin() - NewPos);
				check(RelativeVector.SizeSquared() < 1.f);
#endif
			}

			// apply the delta to all the other bodies
			for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
			{
				if (i != PhysicsAssetInstance->RootBodyIndex)
				{
					NxActor* BodyActor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
					if (BodyActor != NULL)
					{
						FMatrix BodyMatrix = PhysicsAssetInstance->Bodies(i)->GetUnrealWorldTM();
						BodyActor->setGlobalPosition(U2NPosition(BodyMatrix.GetOrigin() + DeltaLoc));
					}
				}
			}

			// Force a physics update now for the owner, to avoid a 1-frame lag when teleporting before the graphics catches up.
			if (Owner != NULL && Owner->Physics == PHYS_RigidBody)
			{
				Owner->SyncActorToRBPhysics();
			}
		}
#endif
	}
}

void USkeletalMeshComponent::SetRBRotation(const FRotator& NewRot, FName BoneName)
{
	if (BoneName != NAME_None || bUseSingleBodyPhysics)
	{
		Super::SetRBRotation(NewRot, BoneName);
	}
	else if (PhysicsAssetInstance == NULL)
	{
		debugf(NAME_Warning, TEXT("USkeletalMeshComponent::SetRBRotation(): no PhysicsAssetInstance for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL"));
	}
	else
	{
#if WITH_NOVODEX
		// calculate the deltas to get the root body to NewRot
		URB_BodyInstance* RootBodyInst = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex);
		if(RootBodyInst->IsValidBodyInstance())
		{
			FMatrix RootBodyMatrix = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex)->GetUnrealWorldTM();
			FRotator DeltaRot = NewRot - RootBodyMatrix.Rotator();
			// move the root body
			NxActor* RootActor = PhysicsAssetInstance->Bodies(PhysicsAssetInstance->RootBodyIndex)->GetNxActor();
			if (RootActor != NULL)
			{
				RootActor->setGlobalOrientationQuat(U2NQuaternion(NewRot.Quaternion()));
			}

			// apply the delta to all the other bodies
			for (INT i = 0; i < PhysicsAssetInstance->Bodies.Num(); i++)
			{
				if (i != PhysicsAssetInstance->RootBodyIndex)
				{
					NxActor* BodyActor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
					if (BodyActor != NULL)
					{
						FMatrix BodyMatrix = PhysicsAssetInstance->Bodies(i)->GetUnrealWorldTM();
						BodyActor->setGlobalOrientationQuat(U2NQuaternion((BodyMatrix.Rotator() + DeltaRot).Quaternion()));
					}
				}
			}

			// Force a physics update now for the owner, to avoid a 1-frame lag when teleporting before the graphics catches up.
			if (Owner != NULL && Owner->Physics == PHYS_RigidBody)
			{
				Owner->SyncActorToRBPhysics();
			}
		}
#endif
	}
}

/** Update bEnableFullAnimWeightBodies Flag - it does not turn off if already on **/
void USkeletalMeshComponent::UpdateFullAnimWeightBodiesFlag()
{
	// If EnableFullAnimWeightBodies is not set
	// If it's true, do not unset it. Some code might like to keep that way
	if ( bEnableFullAnimWeightBodies == FALSE )
	{
		if (PhysicsAsset && PhysicsAssetInstance)
		{
			for ( INT Idx=0; Idx<PhysicsAsset->BodySetup.Num(); ++Idx )
			{
				//  and if Always Full Anim Weights is set in the body
				URB_BodySetup* bs = PhysicsAsset->BodySetup(Idx);
				URB_BodyInstance* bi = PhysicsAssetInstance->Bodies(Idx);

				if(bi->IsValidBodyInstance())
				{
					if ( bs->bAlwaysFullAnimWeight || bi->bInstanceAlwaysFullAnimWeight ) 
					{
						// set it
						bEnableFullAnimWeightBodies = TRUE;
						break;
					}
				}
			}
		}
	}
}

// Start up actual articulated physics for this 
void USkeletalMeshComponent::InitArticulated(UBOOL bFixed)
{
#if WITH_NOVODEX

	// we don't want skelmesh physx assets initialized in the editor (used to get short circuited by lack of RBPhysScene, but we have one in the editor now)
	if( !GIsGame )
	{
		return;
	}
	if(!PhysicsAsset)
	{
		if ( SkeletalMesh )
			debugf(NAME_Warning, TEXT("USkeletalMeshComponent::InitArticulated : No PhysicsAsset defined for %s with skeletalmesh %s"), *GetPathName(), *SkeletalMesh->GetPathName());
		else
			debugf(NAME_Warning, TEXT("USkeletalMeshComponent::InitArticulated : No PhysicsAsset defined for %s"), *GetPathName());
		return;
	}


	// If we dont already have an instance for this asset, initialise it now.
	if(!PhysicsAssetInstance)
	{
#ifdef SHOW_INITARTICULATED_TIME
		DOUBLE ConstructStart = appSeconds();
#endif

		// Construct a new PhysicsAssetInstance object
		PhysicsAssetInstance = ConstructObject<UPhysicsAssetInstance>(UPhysicsAssetInstance::StaticClass(), GWorld, NAME_None, RF_Public);

		// Copy properties from default PAI
		PhysicsAssetInstance->LinearSpringScale			= PhysicsAsset->DefaultInstance->LinearSpringScale;
		PhysicsAssetInstance->LinearDampingScale		= PhysicsAsset->DefaultInstance->LinearDampingScale;
		PhysicsAssetInstance->LinearForceLimitScale		= PhysicsAsset->DefaultInstance->LinearForceLimitScale;

		PhysicsAssetInstance->AngularSpringScale		= PhysicsAsset->DefaultInstance->AngularSpringScale;
		PhysicsAssetInstance->AngularDampingScale		= PhysicsAsset->DefaultInstance->AngularDampingScale;
		PhysicsAssetInstance->AngularForceLimitScale	= PhysicsAsset->DefaultInstance->AngularForceLimitScale;

		PhysicsAssetInstance->CollisionDisableTable = PhysicsAsset->DefaultInstance->CollisionDisableTable;

		PhysicsAssetInstance->bInitBodies				= PhysicsAsset->DefaultInstance->bInitBodies;

		// Create bodies from pool
		INT NumBodies = PhysicsAsset->DefaultInstance->Bodies.Num();
		PhysicsAssetInstance->Bodies.AddZeroed(NumBodies);
		for(INT i=0; i<NumBodies; i++)
		{
			PhysicsAssetInstance->Bodies(i) = GWorld->InstanceRBBody( PhysicsAsset->DefaultInstance->Bodies(i) );
		}

		// Create constraints from pool
		INT NumConstraints = PhysicsAsset->DefaultInstance->Constraints.Num();
		PhysicsAssetInstance->Constraints.AddZeroed(NumConstraints);
		for(INT i=0; i<NumConstraints; i++)
		{
			PhysicsAssetInstance->Constraints(i) = GWorld->InstanceRBConstraint( PhysicsAsset->DefaultInstance->Constraints(i) );
		}

#ifdef SHOW_INITARTICULATED_TIME
		DOUBLE ConstructEnd = appSeconds();
		DOUBLE InitStart = appSeconds();
#endif

		// Pick the right scene.
		FRBPhysScene* UseScene = GWorld->RBPhysScene;

		PhysicsAssetInstance->InitInstance(this, PhysicsAsset, bFixed, UseScene);

		// Update Flag
		UpdateFullAnimWeightBodiesFlag();

#ifdef SHOW_INITARTICULATED_TIME
		DOUBLE InitEnd = appSeconds();
		debugf( TEXT("InitArticulated (%s - %s) - Construct: %2.3fms Init: %2.3fms"), 
			*GetPathName(), 
			*(SkeletalMesh->GetName()), 
			1000.f*(ConstructEnd-ConstructStart), 
			1000.f*(InitEnd-InitStart) );
#endif
	}

#endif // WITH_NOVODEX
}

/** 
 *	Turn off all physics and remove the instance. 
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics if the asset is in that scene. 
 */
void USkeletalMeshComponent::TermArticulated(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	if(!PhysicsAssetInstance)
	{
		return;
	}

	// TermInstance will return Constraints and Bodies to the pool.
	UBOOL bTerminated = PhysicsAssetInstance->TermInstance(Scene);
	if(bTerminated)
	{
		PhysicsAssetInstance = NULL;
	}
#endif // WITH_NOVODEX
}

/** In the case of a skeletal mesh, we want to update all the kinematic bones in the skeleton to match the graphics locations. */
void USkeletalMeshComponent::UpdateRBKinematicData()
{
	// Single body case - treat it like a regular one-body component
	if(bUseSingleBodyPhysics)
	{
		Super::UpdateRBKinematicData();
	}
}

/** Simple, CPU evaluation of a vertex's skinned position (returned in component space) */
FVector USkeletalMeshComponent::GetSkinnedVertexPosition(INT VertexIndex) const
{
	FVector SkinnedPos(0,0,0);

	// Fail if no mesh
	if(!SkeletalMesh)
	{
		return SkinnedPos;
	}

	FStaticLODModel& Model = SkeletalMesh->LODModels(0);

	const USkeletalMeshComponent* BaseComponent = ParentAnimComponent ? ParentAnimComponent : this;

	// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
	INT ChunkIndex;
	INT VertIndex;
	UBOOL bSoftVertex;
	Model.GetChunkAndSkinType(VertexIndex, ChunkIndex, VertIndex, bSoftVertex);

	check(ChunkIndex < Model.Chunks.Num());
	const FSkelMeshChunk& Chunk = Model.Chunks(ChunkIndex);


	const INT RigidInfluenceIndex = SkinningTools::GetRigidInfluenceIndex();

	// Do soft skinning for this vertex.
	if(bSoftVertex)
	{
		const FGPUSkinVertexBase* SrcSoftVertex = Model.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetSoftVertexBufferIndex()+VertIndex);

#if !__INTEL_BYTE_ORDER__
		// BYTE[] elements in LOD.VertexBufferGPUSkin have been swapped for VET_UBYTE4 vertex stream use
		for(INT InfluenceIndex = MAX_INFLUENCES-1;InfluenceIndex >=  MAX_INFLUENCES-Chunk.MaxBoneInfluences;InfluenceIndex--)
#else
		for(INT InfluenceIndex = 0;InfluenceIndex < Chunk.MaxBoneInfluences;InfluenceIndex++)
#endif
		{
			BYTE BoneIndex = Chunk.BoneMap(SrcSoftVertex->InfluenceBones[InfluenceIndex]);
			if(ParentAnimComponent)
			{		
				check(ParentBoneMap.Num() == SkeletalMesh->RefSkeleton.Num());
				BoneIndex = ParentBoneMap(BoneIndex);
			}

			const FLOAT		Weight = (FLOAT)SrcSoftVertex->InfluenceWeights[InfluenceIndex] / 255.0f;
			const FBoneAtom	RefToLocal = SkeletalMesh->RefBasesInvMatrix(BoneIndex) * BaseComponent->SpaceBases(BoneIndex);

			SkinnedPos += RefToLocal.TransformFVector(Model.VertexBufferGPUSkin.GetVertexPosition(SrcSoftVertex)) * Weight;
		}
	}
	// Do rigid (one-influence) skinning for this vertex.
	else
	{
		const FGPUSkinVertexBase* SrcRigidVertex = Model.VertexBufferGPUSkin.GetVertexPtr(Chunk.GetRigidVertexBufferIndex()+VertIndex);

		BYTE BoneIndex = Chunk.BoneMap(SrcRigidVertex->InfluenceBones[RigidInfluenceIndex]);
		if(ParentAnimComponent)
		{
			check(ParentBoneMap.Num() == SkeletalMesh->RefSkeleton.Num());
			BoneIndex = ParentBoneMap(BoneIndex);
		}

		const FBoneAtom	RefToLocal = SkeletalMesh->RefBasesInvMatrix(BoneIndex) * BaseComponent->SpaceBases(BoneIndex);

		SkinnedPos = RefToLocal.TransformFVector(Model.VertexBufferGPUSkin.GetVertexPosition(SrcRigidVertex));
	}

	return SkinnedPos;
}

void USkeletalMeshComponent::UpdateFixedClothVerts()
{
#if WITH_APEX
	BOOL bIsNewClothing = false;
	FIApexClothing *Clothing = GetApexClothing();
	// If clothing is visible (or we are not freezing it when not rendered) call SyncTransforms
	if ( Clothing && (Clothing->IsVisible(TRUE) || !bAutoFreezeApexClothingWhenNotRendered) && (!PhysicsAssetInstance || PhysicsWeight == 0) )
	{
		// Clothing needs up to date transform 
		FMatrix ParentTransform = FMatrix::Identity;
		if( Owner )
		{
			// We have a special case for when its attached to another SkelComp.
			if( AttachedToSkelComponent )
			{
				ParentTransform = AttachedToSkelComponent->CalcAttachedSkelCompMatrix(this);
			}
			else
			{
				ParentTransform = Owner->LocalToWorld();
			}

			if (ParentAnimComponent && bTransformFromAnimParent)
			{
				ParentTransform = ParentAnimComponent->CalcCurrentLocalToWorld(ParentTransform);
				Super::SetParentToWorld(ParentTransform);
			}
			else
			{
				// Update CachedParentToWorld
				SetParentToWorld(ParentTransform);
			}

			// Updates LocalToWorld matrix based on CachedParentToWorld
			SetTransformedToWorld();
		}		
		TArray<FMatrix> WorldBoneTMs;
		physx::PxU32 BoneCount;
		const physx::PxU32 *BonesUsed = Clothing->GetBonesUsed(BoneCount);
		WorldBoneTMs.Add(BoneCount);
		for(physx::PxU32 Index=0; Index<BoneCount; Index++)
		{
			INT BoneIdx = (INT)BonesUsed[Index];
			WorldBoneTMs(Index) = GetBoneMatrix(BoneIdx);
			WorldBoneTMs(Index).ScaleTranslation( FVector(U2PScale) );
		}
		FMatrix LocalToWorldPhysX = LocalToWorld;
		LocalToWorldPhysX.ScaleTranslation( FVector(U2PScale) );
		Clothing->SyncTransforms(WorldBoneTMs.Num(), (const physx::PxF32*)WorldBoneTMs.GetData(), sizeof(FMatrix), LocalToWorldPhysX);
		//debugf(TEXT("SyncTransforms '%s' (UpdateFixedClothVerts)"), *GetPathName());
	}
#endif

#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	// Do nothing if no cloth.
	// Also, only do cloth stuff at highest LOD.
	// @todo: turn off cloth sim when at lower LODs.
	if( !ClothSim 
	||	!SkeletalMesh 
	||	!MeshObject 
	||	PredictedLODLevel > 0 
	||	bAttachClothVertsToBaseBody
	// Only perform work if we're moving/ animating or doing it for the first frame.
	||	(bUpdatedFixedClothVerts && bIsClothOnStaticObject) )
	{
		return;
	}

	NxCloth* nCloth = (NxCloth*)ClothSim;

	INT StartVert = 0;
	TArray<NxVec3> AnimatedPos;

	if(SkeletalMesh->bLimitClothToAnimMesh)
	{
		INT NumSimVerts = nCloth->getNumberOfParticles();
		AnimatedPos.AddZeroed(NumSimVerts);
	}
	else
	{
		StartVert = SkeletalMesh->NumFreeClothVerts;
	}

	// All the verts from NumFreeClothVerts to the end should be 'fixed'.
	check(SkeletalMesh->NumFreeClothVerts <= SkeletalMesh->ClothToGraphicsVertMap.Num());
	for(INT ClothVertIdx=StartVert; ClothVertIdx<SkeletalMesh->ClothToGraphicsVertMap.Num(); ClothVertIdx++)
	{
		// Find the index of the graphics vertex that corresponds to this cloth vertex
		INT GraphicsIndex = SkeletalMesh->ClothToGraphicsVertMap(ClothVertIdx);
		FVector SkinnedPos = GetSkinnedVertexPosition(GraphicsIndex);
		FVector WorldPos = LocalToWorld.TransformFVector(SkinnedPos); // Transform into world space

		// Find sim 
		INT SimVertIdx = ClothVertIdx;
		if(SkeletalMesh->ClothWeldingMap.Num() > 0)
		{
			SimVertIdx = SkeletalMesh->ClothWeldingMap(ClothVertIdx);
		}

		// If we are limiting all verts to a distance of the animated position, set all constaint positions
		if(SkeletalMesh->bLimitClothToAnimMesh)
		{
			AnimatedPos(SimVertIdx) = U2NPosition(WorldPos);
		}
		// If we are only updating 'fixed' verts, move it now
		else
		{
			nCloth->attachVertexToGlobalPosition(SimVertIdx, U2NPosition(WorldPos)); 
		}
	}

	if(SkeletalMesh->bLimitClothToAnimMesh)
	{
		nCloth->setConstrainPositions(AnimatedPos.GetData(), sizeof(NxVec3));
	}

	bUpdatedFixedClothVerts = TRUE;
#endif // WITH_NOVODEX && !NX_DISABLE_CLOTH
}


#if WITH_NOVODEX 
/** Util to clamp a vector to a box. */
static void ClampVectorToBox(NxVec3& Vec, const FVector& Min, const FVector& Max)
{
	Vec.x = Clamp<FLOAT>(Vec.x, Min.X, Max.X);
	Vec.y = Clamp<FLOAT>(Vec.y, Min.Y, Max.Y);
	Vec.z = Clamp<FLOAT>(Vec.z, Min.Z, Max.Z);
}
#endif // #if WITH_NOVODEX 

/** Update forces applied to each cloth particle based on the ClothWind parameter. */
void USkeletalMeshComponent::UpdateClothWindForces(FLOAT DeltaSeconds)
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	// Do nothing if no cloth enabled or no wind.
	NxCloth* nCloth = (NxCloth*)ClothSim;
	if(!ClothSim || bClothFrozen || nCloth == NULL || nCloth->isSleeping())
	{
		return;
	}

	// check to see if the cloth location has exceeded the reset distance
	if (LastClothLocation != FVector(0.f) && (LastClothLocation - Owner->Location).SizeSquared() > (MinDistanceForClothReset * MinDistanceForClothReset))
	{
		ResetClothVertsToRefPose();
	}
	LastClothLocation = (Owner) ? Owner->Location : FVector(0.f,0.f,0.f);

	// If we want to modify particle velocities.
	if(!ClothWind.IsZero() || bClothBaseVelClamp || bClothPositionalDampening )
	{
		// Find the 'base-most' velocity - keep walking up as long as they are not static actors.
		FVector BaseVel(0,0,0);
		AActor* VelActor = Owner;
		while(VelActor && !VelActor->IsStatic())
		{
			BaseVel = VelActor->Velocity;
			VelActor = VelActor->Base;
		}

		FVector MinClothVel = (BaseVel - ClothBaseVelClampRange) * U2PScale;
		FVector MaxClothVel = (BaseVel + ClothBaseVelClampRange) * U2PScale;

		// Compute relative dampening box around owner
		FVector OwnerLoc = (Owner != NULL) ? Owner->Location : FVector(0);
		FRotationMatrix OwnerRot = (Owner != NULL) ? Owner->Rotation : FRotator(0,0,0);
		FVector HMin = OwnerRot.TransformNormal( MinPosDampRange );
		FVector HMax = OwnerRot.TransformNormal( MaxPosDampRange );
		FVector PosDampBoxCenter = OwnerLoc + ((HMax - HMin)*0.5f);
		FVector PosDampBoxExtent = ((HMax + HMin)*0.5f);
		UBOOL bDoDampBoxChecks = bClothPositionalDampening && !PosDampBoxExtent.IsZero();
/*
		if( Owner )
		{
			Owner->DrawDebugBox( PosDampBoxCenter, PosDampBoxExtent, 255, 0, 0, FALSE );
		}
*/

		// Check data all looks good, and get number of particles.
#if !FINAL_RELEASE
		if(SkeletalMesh->bEnableClothTearing && (SkeletalMesh->ClothWeldingMap.Num() == 0))
		{
			//With cloth tearing the cloth can have less particles than the size of the array.
			check( nCloth->getNumberOfParticles() == NumClothMeshVerts );
			check( ClothMeshNormalData.Num() == ClothMeshPosData.Num() );
		}
		else if (SkeletalMesh->ClothWeldingMap.Num() == 0)
		{
			check( nCloth->getNumberOfParticles() == ClothMeshPosData.Num() );
			check( ClothMeshNormalData.Num() == ClothMeshPosData.Num() );
		}
		else
		{
			// Check consistency for NxCloth output buffers
			check( nCloth->getNumberOfParticles() == ClothMeshWeldedPosData.Num() );
			check( ClothMeshWeldedNormalData.Num() == ClothMeshWeldedPosData.Num() );

			// check consistency of Welding map
			check (SkeletalMesh->ClothWeldingMap.Num() == ClothMeshPosData.Num() );
			check( SkeletalMesh->ClothWeldingDomain == ClothMeshWeldedPosData.Num() );
		}
#endif
		//check (NumClothMeshVerts == nCloth->getNumberOfParticles() );

		// First, read current velocities from each particle.
		TArray<NxVec3> ParticleVels;
		TArray<NxVec3> ParticleLocs;
		INT NumClothParticles = nCloth->getNumberOfParticles();
		ParticleVels.Add(NumClothParticles);
		ParticleLocs.Add(NumClothParticles);

		nCloth->getVelocities(ParticleVels.GetData());
		nCloth->getPositions(ParticleLocs.GetData());

		// Get the scene the cloth is in
		NxScene & nScene = nCloth->getScene();
		FRBPhysScene* RBScene = (FRBPhysScene*)nScene.userData;
		check(RBScene);

		FVector Wind = ClothWind;
		if(bClothWindRelativeToOwner)
		{
			Wind = OwnerRot.TransformNormal( ClothWind );
		}

		// Then update velocity based on dot product between wind vector and surface normal for that particle
		for(INT i=0; i<NumClothParticles; i++)
		{
			FLOAT WindDot = (Wind | ClothMeshNormalData(i));
			NxVec3 WindForce = U2NVectorCopy(WindDot * ClothMeshNormalData(i) * 0.01f);
			#if SUPPORT_DOUBLE_BUFFERING
			NxVec3 OldVel;
			if (RBScene->UsingBufferedScene)
			{
				OldVel = ParticleVels(i);
			}
			#endif
			ParticleVels(i) += WindForce;

			// If desired, clamp velocity
			if(bClothBaseVelClamp)
			{		
				ClampVectorToBox(ParticleVels(i), MinClothVel, MaxClothVel);
			}
			else if(bClothBaseVelInterp)
			{
				FVector DeltaVel = N2UPosition(ParticleVels(i)) - BaseVel;

				FVector NewVel = Lerp( N2UPosition(ParticleVels(i)), BaseVel, ClothBaseVelClampRange * DeltaSeconds );
				ParticleVels(i) = U2NPosition(NewVel);
			}

			// Do positional box dampening 
			if( bDoDampBoxChecks )
			{
				FVector ParticleLoc = N2UPosition( ParticleLocs(i) );
				FVector DampScale(-1);
				FVector Pct(-1);

				if( PosDampBoxExtent.X != 0.f )
				{
					if( (ParticleLoc.X > PosDampBoxCenter.X && ParticleVels(i).x > 0) || 
						(ParticleLoc.X < PosDampBoxCenter.X && ParticleVels(i).x < 0) )
					{
						Pct.X = Clamp<FLOAT>( Abs<FLOAT>(ParticleLoc.X-PosDampBoxCenter.X) / PosDampBoxExtent.X, 0.f, 1.f );
						DampScale.X = Lerp<FLOAT>( MinPosDampScale.X, MaxPosDampScale.X, Pct.X );
						ParticleVels(i) *= DampScale.X;
					}
				}
				if( PosDampBoxExtent.Y != 0.f )
				{
					if( (ParticleLoc.Y > PosDampBoxCenter.Y && ParticleVels(i).y > 0) || 
						(ParticleLoc.Y < PosDampBoxCenter.Y && ParticleVels(i).y < 0) )
					{
						Pct.Y = Clamp<FLOAT>( Abs<FLOAT>(ParticleLoc.Y-PosDampBoxCenter.Y) / PosDampBoxExtent.Y, 0.f, 1.f );
						DampScale.Y = Lerp<FLOAT>( MinPosDampScale.Y, MaxPosDampScale.Y, Pct.Y );
						ParticleVels(i) *= DampScale.Y;
					}
				}
				if( PosDampBoxExtent.Z != 0.f )
				{
					if( (ParticleLoc.Z > PosDampBoxCenter.Z && ParticleVels(i).z > 0) || 
						(ParticleLoc.Z < PosDampBoxCenter.Z && ParticleVels(i).z < 0) )
					{
						Pct.Z = Clamp<FLOAT>( Abs<FLOAT>(ParticleLoc.Z-PosDampBoxCenter.Z) / PosDampBoxExtent.Z, 0.f, 1.f );
						DampScale.Z = Lerp<FLOAT>( MinPosDampScale.Z, MaxPosDampScale.Z, Pct.Z );
						ParticleVels(i) *= DampScale.Z;
					}
				}
			}

			
			#if SUPPORT_DOUBLE_BUFFERING
			if (RBScene->UsingBufferedScene)
			{
				// Compute how much we have clamped and reduce the velocity change to keep velocity small enough
				NxVec3 Clamped = OldVel - ParticleVels(i);
				WindForce -= Clamped;
				// Combination of getVelocity and setVelocity is problematic when double buffered
				// -> use addForceAtVertex
				nCloth->addForceAtVertex(WindForce, i, NX_VELOCITY_CHANGE);
			}
			#endif
		}

		// Update particle velocities.
		#if SUPPORT_DOUBLE_BUFFERING
		if (!RBScene->UsingBufferedScene)
		#endif
		{
			// Combination of getVelocity and setVelocity is problematic when double buffered
			nCloth->setVelocities(ParticleVels.GetData());
		}
	}
#endif
}

/** Forces apex clothing to use 'teleport and reset' for the next update */
void USkeletalMeshComponent::ForceApexClothingTeleportAndReset()
{
#if WITH_APEX
	FIApexClothing *Clothing = GetApexClothing();
	if(Clothing != NULL)
	{
		Clothing->ForceNextUpdateTeleportAndReset();
	}
#endif
}

/** Script exposed ForceApexClothingTeleportAndReset */
void USkeletalMeshComponent::execForceApexClothingTeleportAndReset(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	ForceApexClothingTeleportAndReset();
}

/** Forces apex clothing to use 'teleport' for the next update */
void USkeletalMeshComponent::ForceApexClothingTeleport()
{
#if WITH_APEX
	FIApexClothing *Clothing = GetApexClothing();
	if(Clothing != NULL)
	{
		Clothing->ForceNextUpdateTeleport();
	}
#endif
}

/** Script exposed ForceApexClothingTeleport */
void USkeletalMeshComponent::execForceApexClothingTeleport(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	ForceApexClothingTeleport();
}

/** Move all vertices in the cloth to the reference pose and zero their velocity. */
void USkeletalMeshComponent::ResetClothVertsToRefPose()
{
	// Don't allow this on tearable cloth
	if(!SkeletalMesh || !ClothSim || SkeletalMesh->bEnableClothTearing)
	{
		return;
	}

#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;

	// Build vertex buffer with _all_ verts in skeletal mesh.
	FStaticLODModel& Model = SkeletalMesh->LODModels(0);
	TArray<NxVec3>	ClothVerts;

	if (SkeletalMesh->ClothWeldingMap.Num() > 0)
	{
		if( (SkeletalMesh->ClothWeldingMap.Num() != SkeletalMesh->ClothToGraphicsVertMap.Num()) )
		{
			debugf(TEXT("ResetClothVertsToRefPose (%s) : SkeletalMesh->ClothWeldingMap.Num() != SkeletalMesh->ClothToGraphicsVertMap.Num()"), *SkeletalMesh->GetName());
			return;
		}

		if( (SkeletalMesh->ClothWeldingDomain != NumClothMeshVerts) )
		{
			debugf(TEXT("ResetClothVertsToRefPose (%s) : SkeletalMesh->ClothWeldingDomain != NumClothMeshVerts"), *SkeletalMesh->GetName());
			return;
		}
	}
	else
	{
		if(NumClothMeshVerts != SkeletalMesh->ClothToGraphicsVertMap.Num())
		{
			debugf(TEXT("ResetClothVertsToRefPose (%s) : NumClothMeshVerts != SkeletalMesh->ClothToGraphicsVertMap.Num()"), *SkeletalMesh->GetName());
			return;
		}
	}

	ClothVerts.Add(NumClothMeshVerts);

	for(INT i=0; i<NumClothMeshVerts; i++)
	{
		// Find the index of the graphics vertex that corresponds to this cloth vertex
		INT GraphicsIndex = SkeletalMesh->ClothToGraphicsVertMap(i);

		// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
		INT ChunkIndex;
		INT VertIndex;
		UBOOL bSoftVertex;
		Model.GetChunkAndSkinType(GraphicsIndex, ChunkIndex, VertIndex, bSoftVertex);

		check(ChunkIndex < Model.Chunks.Num());
		const FSkelMeshChunk& Chunk = Model.Chunks(ChunkIndex);

		// Find component-space position of vertex in ref pose.
		FVector RefPosePos(0,0,0);
		if(bSoftVertex)
		{
			RefPosePos = Model.VertexBufferGPUSkin.GetVertexPosition(Chunk.GetSoftVertexBufferIndex()+VertIndex);
		}
		else
		{
			RefPosePos = Model.VertexBufferGPUSkin.GetVertexPosition(Chunk.GetRigidVertexBufferIndex()+VertIndex);
		}

		// Put world-space position into array
		INT Destination = i;
		if (SkeletalMesh->ClothWeldingMap.Num() > 0)
		{
			Destination = SkeletalMesh->ClothWeldingMap(i);
		}
		ClothVerts(Destination) = U2NPosition( LocalToWorld.TransformFVector(RefPosePos) );
	}

	// Move all points in cloth to reference pose.
	nCloth->setPositions(ClothVerts.GetData());

	// Also zero out velocities.
	TArray<NxVec3> ClothVels;
	ClothVels.AddZeroed(ClothVerts.Num());
	nCloth->setVelocities(ClothVels.GetData());
#endif
}

/** Script handler for ResetClothVertsToRefPose. */
void USkeletalMeshComponent::execResetClothVertsToRefPose( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	ResetClothVertsToRefPose();
}

//Some get*() APIs
FLOAT USkeletalMeshComponent::GetClothAttachmentResponseCoefficient()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getAttachmentResponseCoefficient();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothAttachmentTearFactor()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getAttachmentTearFactor();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothBendingStiffness()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getBendingStiffness();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothCollisionResponseCoefficient()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getCollisionResponseCoefficient();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothDampingCoefficient()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getDampingCoefficient();
#else
	return -1;
#endif
}
INT USkeletalMeshComponent::GetClothFlags()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getFlags();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothFriction()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getFriction();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothPressure()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getPressure();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothSleepLinearVelocity()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getSleepLinearVelocity();
#else
	return -1;
#endif
}
INT USkeletalMeshComponent::GetClothSolverIterations()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getSolverIterations();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothStretchingStiffness()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getStretchingStiffness();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothTearFactor()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getTearFactor();
#else
	return -1;
#endif
}
FLOAT USkeletalMeshComponent::GetClothThickness()
{
	if(!SkeletalMesh || !ClothSim)
	{
		return -1;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	return nCloth->getThickness();
#else
	return -1;
#endif
}


//some set*() APIs
void USkeletalMeshComponent::SetClothAttachmentResponseCoefficient(FLOAT ClothAttachmentResponseCoefficient)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setAttachmentResponseCoefficient(ClothAttachmentResponseCoefficient);
#endif
}
void USkeletalMeshComponent::SetClothAttachmentTearFactor(FLOAT ClothAttachmentTearFactor)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setAttachmentTearFactor(ClothAttachmentTearFactor);
#endif
}
void USkeletalMeshComponent::SetClothBendingStiffness(FLOAT ClothBendingStiffness)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setBendingStiffness(ClothBendingStiffness);
#endif
}
void USkeletalMeshComponent::SetClothCollisionResponseCoefficient(FLOAT ClothCollisionResponseCoefficient)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setCollisionResponseCoefficient(ClothCollisionResponseCoefficient);
#endif
}
void USkeletalMeshComponent::SetClothDampingCoefficient(FLOAT ClothDampingCoefficient)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setDampingCoefficient(ClothDampingCoefficient);
#endif
}
void USkeletalMeshComponent::SetClothFlags(INT ClothFlags)
{
	if(!SkeletalMesh || !ClothSim || ClothFlags<0)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setFlags(ClothFlags);
#endif
}
void USkeletalMeshComponent::SetClothFriction(FLOAT ClothFriction)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setFriction(ClothFriction);
#endif
}
void USkeletalMeshComponent::SetClothPressure(FLOAT ClothPressure)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setPressure(ClothPressure);
#endif
}
void USkeletalMeshComponent::SetClothSleepLinearVelocity(FLOAT ClothSleepLinearVelocity)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setSleepLinearVelocity(ClothSleepLinearVelocity);
#endif
}
void USkeletalMeshComponent::SetClothSolverIterations(INT ClothSolverIterations)
{
	if(!SkeletalMesh || !ClothSim || ClothSolverIterations<1)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setSolverIterations(ClothSolverIterations);
#endif
}
void USkeletalMeshComponent::SetClothStretchingStiffness(FLOAT ClothStretchingStiffness)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setStretchingStiffness(ClothStretchingStiffness);
#endif
}
void USkeletalMeshComponent::SetClothTearFactor(FLOAT ClothTearFactor)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setTearFactor(ClothTearFactor);
#endif
}
void USkeletalMeshComponent::SetClothThickness(FLOAT ClothThickness)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	nCloth->setThickness(ClothThickness);
#endif
}
//Other APIs
void USkeletalMeshComponent::SetClothSleep(UBOOL IfClothSleep)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	if(IfClothSleep){nCloth->putToSleep();}
	else{nCloth->wakeUp();}
#endif
}
void USkeletalMeshComponent::SetClothPosition(const FVector& ClothOffSet)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	int ClothVerNumber = nCloth->getNumberOfParticles();
	NxVec3* CurClothPositions;
	CurClothPositions = new NxVec3[ClothVerNumber];
	nCloth->getPositions(CurClothPositions);
	NxVec3 OffSet = U2NPosition(ClothOffSet);
	for(int i = 0; i < ClothVerNumber; i ++){
		CurClothPositions[i]+=OffSet;
	}
	nCloth->setPositions(CurClothPositions);
	delete[] CurClothPositions;
#endif
}
void USkeletalMeshComponent::SetClothVelocity(const FVector& VelocityOffSet)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	int ClothVerNumber = nCloth->getNumberOfParticles();
	NxVec3* CurClothVelocities;
	CurClothVelocities = new NxVec3[ClothVerNumber];
	nCloth->getVelocities(CurClothVelocities);
	NxVec3 OffSet = U2NPosition(VelocityOffSet);
	for(int i = 0; i < ClothVerNumber; i ++){
		CurClothVelocities[i]+=OffSet;
	}
	nCloth->setVelocities(CurClothVelocities);
	delete[] CurClothVelocities;
#endif
}
//Attachment API
void USkeletalMeshComponent::AttachClothToCollidingShapes(UBOOL AttatchTwoWay, UBOOL AttachTearable)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	if (AttatchTwoWay && AttachTearable){
		nCloth->attachToCollidingShapes(NX_CLOTH_ATTACHMENT_TEARABLE|NX_CLOTH_ATTACHMENT_TWOWAY);	
	}
	else if(AttatchTwoWay){
		nCloth->attachToCollidingShapes(NX_CLOTH_ATTACHMENT_TWOWAY);
	}
	else if(AttachTearable){
		nCloth->attachToCollidingShapes(NX_CLOTH_ATTACHMENT_TEARABLE);
	}
	else{
		nCloth->attachToCollidingShapes(0);	
	}
#endif
}
//ValidBounds API
void USkeletalMeshComponent::EnableClothValidBounds(UBOOL IfEnableClothValidBounds)
{
	if(!SkeletalMesh || !ClothSim)
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	NxCloth* nCloth = (NxCloth*)ClothSim;
	NxU32 ClothFlags = nCloth->getFlags();
	if(IfEnableClothValidBounds){ClothFlags |= NX_CLF_VALIDBOUNDS;}
	else{ClothFlags &= ~NX_CLF_VALIDBOUNDS;}
	nCloth->setFlags(ClothFlags);
#endif
}
void USkeletalMeshComponent::SetClothValidBounds(const FVector& ClothValidBoundsMin, const FVector& ClothValidBoundsMax)
{
	{
		if(!SkeletalMesh || !ClothSim)
		{
			return;
		}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
		NxCloth* nCloth = (NxCloth*)ClothSim;
		NxBounds3 ClothValidBounds;
		ClothValidBounds.include(U2NTransform(LocalToWorld).t + U2NPosition(ClothValidBoundsMin));
		ClothValidBounds.include(U2NTransform(LocalToWorld).t + U2NPosition(ClothValidBoundsMax));
		nCloth->setValidBounds(ClothValidBounds);
#endif
	}
}

//--------------------------------------------------------------------------------
//Some get*() APIs
void USkeletalMeshComponent::execGetClothAttachmentResponseCoefficient( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothAttachmentResponseCoefficient();
}
void USkeletalMeshComponent::execGetClothAttachmentTearFactor( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothAttachmentTearFactor();
}
void USkeletalMeshComponent::execGetClothBendingStiffness( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothBendingStiffness();
}
void USkeletalMeshComponent::execGetClothCollisionResponseCoefficient( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothCollisionResponseCoefficient();
}
void USkeletalMeshComponent::execGetClothDampingCoefficient( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothDampingCoefficient();
}
void USkeletalMeshComponent::execGetClothFlags( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(INT*)Result = GetClothFlags();
}
void USkeletalMeshComponent::execGetClothFriction( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothFriction();
}
void USkeletalMeshComponent::execGetClothPressure( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothPressure();
}
void USkeletalMeshComponent::execGetClothSleepLinearVelocity( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothSleepLinearVelocity();
}
void USkeletalMeshComponent::execGetClothSolverIterations( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(INT*)Result = GetClothSolverIterations();
}
void USkeletalMeshComponent::execGetClothStretchingStiffness( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothStretchingStiffness();
}
void USkeletalMeshComponent::execGetClothTearFactor( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothTearFactor();
}
void USkeletalMeshComponent::execGetClothThickness( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	*(FLOAT*)Result = GetClothThickness();
}
//some set*() APIs
void USkeletalMeshComponent::execSetClothAttachmentResponseCoefficient( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothAttachmentResponseCoefficient);
	P_FINISH;

	SetClothAttachmentResponseCoefficient(ClothAttachmentResponseCoefficient);
}
void USkeletalMeshComponent::execSetClothAttachmentTearFactor( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothAttachmentTearFactor);
	P_FINISH;

	SetClothAttachmentTearFactor(ClothAttachmentTearFactor);
}
void USkeletalMeshComponent::execSetClothBendingStiffness( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothBendingStiffness);
	P_FINISH;

	SetClothBendingStiffness(ClothBendingStiffness);
}
void USkeletalMeshComponent::execSetClothCollisionResponseCoefficient( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothCollisionResponseCoefficient);
	P_FINISH;

	SetClothCollisionResponseCoefficient(ClothCollisionResponseCoefficient);
}
void USkeletalMeshComponent::execSetClothDampingCoefficient( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothDampingCoefficient);
	P_FINISH;

	SetClothDampingCoefficient(ClothDampingCoefficient);
}
void USkeletalMeshComponent::execSetClothFlags( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(ClothFlags);
	P_FINISH;

	SetClothFlags(ClothFlags);
}
void USkeletalMeshComponent::execSetClothFriction( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothFriction);
	P_FINISH;

	SetClothFriction(ClothFriction);
}
void USkeletalMeshComponent::execSetClothPressure( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothPressure);
	P_FINISH;

	SetClothPressure(ClothPressure);
}
void USkeletalMeshComponent::execSetClothSleepLinearVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothSleepLinearVelocity);
	P_FINISH;

	SetClothSleepLinearVelocity(ClothSleepLinearVelocity);
}
void USkeletalMeshComponent::execSetClothSolverIterations( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(ClothSolverIterations);
	P_FINISH;

	SetClothSolverIterations(ClothSolverIterations);
}
void USkeletalMeshComponent::execSetClothStretchingStiffness( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothStretchingStiffness);
	P_FINISH;

	SetClothStretchingStiffness(ClothStretchingStiffness);
}
void USkeletalMeshComponent::execSetClothTearFactor( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothTearFactor);
	P_FINISH;

	SetClothTearFactor(ClothTearFactor);
}
void USkeletalMeshComponent::execSetClothThickness( FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(ClothThickness);
	P_FINISH;

	SetClothThickness(ClothThickness);
}
//Other APIs
void USkeletalMeshComponent::execSetClothSleep( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(IfClothSleep);
	P_FINISH;

	SetClothSleep(IfClothSleep);
}
void USkeletalMeshComponent::execSetClothPosition( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(ClothOffSet);
	P_FINISH;

	SetClothPosition(ClothOffSet);
}
void USkeletalMeshComponent::execSetClothVelocity( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(VelocityOffSet);
	P_FINISH;

	SetClothVelocity(VelocityOffSet);
}
//Attachment API
void USkeletalMeshComponent::execAttachClothToCollidingShapes( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(AttatchTwoWay);
	P_GET_UBOOL(AttachTearable);
	P_FINISH;

	AttachClothToCollidingShapes(AttatchTwoWay, AttachTearable);
}
//ValidBounds APIs
void USkeletalMeshComponent::execEnableClothValidBounds( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(IfEnableClothValidBounds);
	P_FINISH;

	EnableClothValidBounds(IfEnableClothValidBounds);		
}
void USkeletalMeshComponent::execSetClothValidBounds( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(ClothValidBoundsMin);
	P_GET_VECTOR(ClothValidBoundsMax);
	P_FINISH;

	SetClothValidBounds(ClothValidBoundsMin, ClothValidBoundsMax);
}

/** USkeletalMeshComponent version uses Super::AddImpulse to apply impulse to rigid body, then looks to see if cloth/soft body needs impulses applied. */
void USkeletalMeshComponent::AddImpulse(FVector Impulse, FVector Position, FName BoneName, UBOOL bVelChange)
{
	Super::AddImpulse(Impulse, Position, BoneName, bVelChange);

#if WITH_NOVODEX
	// Don't support adding forces at COM for cloth/soft-body.
	if(Position.IsZero())
	{
		return;
	}

	NxVec3 nImpulse = U2NPosition(Impulse);
	NxVec3 nPosition = U2NPosition(Position);
	NxForceMode ForceMode = bVelChange ? NX_VELOCITY_CHANGE : NX_IMPULSE;

#  if !NX_DISABLE_CLOTH
	if(ClothSim && bEnableClothSimulation)
	{
		NxCloth* Cloth = (NxCloth *)ClothSim;

		NxRay nWorldRay;
		NxVec3 nHit;
		NxU32 nVertexId;

		FVector ImpulseDir = Impulse.SafeNormal();
		nWorldRay.orig = U2NPosition(Position - (10.f*ImpulseDir));
		nWorldRay.dir = U2NVectorCopy(ImpulseDir);

		// TODO: Avoid extra ray cast just to find closest vert
		if(Cloth->raycast(nWorldRay, nHit, nVertexId))
		{
			Cloth->addForceAtVertex(nImpulse * ClothImpulseScale, nVertexId, ForceMode);
		}
	}
#  endif // !NX_DISABLE_CLOTH

#  if !NX_DISABLE_SOFTBODY
	if(SoftBodySim && bEnableSoftBodySimulation)
	{
		NxSoftBody* SoftBody = (NxSoftBody *)SoftBodySim;

		NxRay nWorldRay;
		NxVec3 nHit;
		NxU32 nVertexId;

		FVector ImpulseDir = Impulse.SafeNormal();
		nWorldRay.orig = U2NPosition(Position - (10.f*ImpulseDir));
		nWorldRay.dir = U2NVectorCopy(ImpulseDir);

		// TODO: Avoid extra ray cast just to find closest vert
		if(SoftBody->raycast(nWorldRay, nHit, nVertexId))
		{
			SoftBody->addForceAtVertex(nImpulse * SoftBodyImpulseScale, nVertexId, ForceMode);
		}
	}
#  endif // !NX_DISABLE_SOFTBODY
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

	if(bUseSingleBodyPhysics)
	{
		Super::AddRadialImpulse(Origin, Radius, Strength, Falloff, bVelChange);
		return;
	}

#if WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddRadialImpulse() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
		{
			NxActor* nActor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
			if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
			{
				AddRadialImpulseToBody(nActor, Origin, Radius, Strength, Falloff, bVelChange);
			}
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::AddRadialForce(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

	if(bUseSingleBodyPhysics)
	{
		Super::AddRadialForce(Origin, Radius, Strength, Falloff);
		return;
	}

#if WITH_NOVODEX
	if(PhysicsAssetInstance)
	{
#if !defined(FINAL_RELEASE) || !CONSOLE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call AddRadialForce() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
		{
			NxActor* nActor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
			if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
			{
				AddRadialForceToBody(nActor, Origin, Radius, Strength, Falloff);
			}
		}
	}
#endif // WITH_NOVODEX
}

void USkeletalMeshComponent::AddForceField(FForceApplicator* Applicator, const FBox& FieldBoundingBox, UBOOL bApplyToCloth, UBOOL bApplyToRigidBody)
{
	if(bIgnoreForceField)
	{
		return;
	}

	if(bUseSingleBodyPhysics)
	{
		Super::AddForceField(Applicator, FieldBoundingBox, bApplyToCloth, bApplyToRigidBody);
		return;
	}

#if WITH_NOVODEX

	if((PhysicsAssetInstance == NULL) && (ClothSim == NULL))
		return;

#if !defined(FINAL_RELEASE) || !defined(CONSOLE)
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call AddForceField() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif

	if(PhysicsAssetInstance && bApplyToRigidBody)
	{
		for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
		{
			NxActor* nActor = PhysicsAssetInstance->Bodies(i)->GetNxActor();
			if(nActor && nActor->isDynamic() && !nActor->readBodyFlag(NX_BF_KINEMATIC) )
			{
				AddForceFieldToBody(nActor, Applicator);
			}
		}
	}

#if !NX_DISABLE_CLOTH
	// Don't apply force to frozen cloth
	if((ClothSim != NULL) && bEnableClothSimulation && bApplyToCloth && !bClothFrozen && (ClothForceScale > 0))
	{
		if (SkeletalMesh->bClothMetal && (PhysicsAssetInstance != NULL) && (PhysicsAssetInstance->Bodies.Num() == 1))
		{
			NxActor* nActor = PhysicsAssetInstance->Bodies(0)->GetNxActor();
			AddForceFieldToBody(nActor, Applicator);
		}
		else
		{
			NxCloth* nCloth = (NxCloth *)ClothSim;
			AddForceFieldToCloth(nCloth, Applicator, ClothForceScale);
		}
	}
#endif	// !NX_DISABLE_CLOTH

#endif // WITH_NOVODEX
}

/** 
 *	This is used in the single-body case in PrimitiveComponent::InitComponentRBPhys. 
 *	Return the BodySetup of the root bone of the mesh.
 */
URB_BodySetup* USkeletalMeshComponent::GetRBBodySetup()
{
	if(SkeletalMesh && PhysicsAsset)
	{
		for(INT i=0; i<SkeletalMesh->RefSkeleton.Num(); i++)
		{
			INT BodyIndex = PhysicsAsset->FindBodyIndex( SkeletalMesh->RefSkeleton(i).Name );
			if(BodyIndex != INDEX_NONE)
			{
				return PhysicsAsset->BodySetup(BodyIndex);
			}
		}
	}	

	return NULL;
}


/** 
 *	When initialising physics for a SkeletalMeshComponent, either call the PrimitiveComponent version of function
 *	which handles single-body case (eg. a vehicle) or InitArticulated, which handles the multi-body (eg. ragdoll) case.
 *	Then iterate over all attached components initialising them.
 */
void USkeletalMeshComponent::InitComponentRBPhys(UBOOL bFixed)
{
	if(!GWorld->RBPhysScene || bDisableAllRigidBody)
	{
		return;
	}

#if WITH_APEX
	// Don't simulate clothing in the editor outside PIE
	if(!bSkipInitClothing && GIsGame)
	{
		InitApexClothing(GWorld->RBPhysScene);
	}
#endif

	bSkelCompFixed = bFixed;

	if(bUseSingleBodyPhysics)
	{
		Super::InitComponentRBPhys(bSkelCompFixed);
	}
	else
	{
		if(bHasPhysicsAssetInstance && SkeletalMesh)
		{
#if PERF_SHOW_PHYS_INIT_COSTS
			DOUBLE StartInitArticulated = appSeconds();
#endif

			InitArticulated(bSkelCompFixed);

#if PERF_SHOW_PHYS_INIT_COSTS
			TotalInitArticulatedTime += (appSeconds() - StartInitArticulated);
#endif
		}
	}

	if(!ClothSim)
	{
		// Make sure we start empty, even if no physics are run.
		ClothMeshIndexData.Empty();

		// Init cloth if desired
		if(bEnableClothSimulation)
		{
#if !FINAL_RELEASE	
			if( GetOwner()->bHidden )
			{
				debugf(NAME_Warning,TEXT("Creating a hidden cloth: %s"),*(GetOwner()->GetName()));
			}
#endif // !FINAL_RELEASE

			FRBPhysScene* UseScene = GWorld->RBPhysScene;
			InitClothSim(UseScene);
		}

		if (SkeletalMesh && SkeletalMesh->bClothMetal)
		{
			InitClothMetal();
		}
	}

	// Init SoftBody if desired
	if(bEnableSoftBodySimulation)
	{
		FRBPhysScene* UseScene = GWorld->RBPhysScene;
		InitSoftBodySim(UseScene);
	}

	// Iterate over attached components, calling InitComponentRBPhys on each.
	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		FAttachment& Attachment = Attachments(AttachmentIndex);
		if(Attachment.Component && Attachment.Component->IsAttached())
		{
			Attachment.Component->InitComponentRBPhys(bSkelCompFixed);
		}
	}
}

/** 
 *	Change the physics information for this component to either allow it to simulate, or to 'lock' it in position. 
 *	If single-body case, routes function to PrimitiveComponent version.
 */
void USkeletalMeshComponent::SetComponentRBFixed(UBOOL bFixed)
{
	bSkelCompFixed = bFixed;

	if(bUseSingleBodyPhysics)
	{
		Super::SetComponentRBFixed(bFixed);
	}
	else
	{
		// If we have a PhysicsAssetInstance now, iterate over the bodies.
		if(PhysicsAssetInstance)
		{
			check(PhysicsAsset);
			check(PhysicsAssetInstance->Bodies.Num() == PhysicsAsset->BodySetup.Num());

			for(INT i=0; i<PhysicsAssetInstance->Bodies.Num(); i++)
			{
				URB_BodySetup* Setup = PhysicsAsset->BodySetup(i);
				URB_BodyInstance* Instance = PhysicsAssetInstance->Bodies(i);

				// We need to look at the Setup, because if we are unfixing in general, we still don't
				// want to 'unfix' things that are marked fixed in the PhysicsAsset
				Instance->SetFixed(bFixed || Setup->bFixed);
			}
		}
	}
}

/** 
 *	When terminating physics for this SkeletalMeshComponent, first terminated any articulated physics. 
 *	Then call TermComponentRBPhys on all attached Components. 
 *	This calls the PrimitiveComponent::TermComponentRBPhys which will clear any BodyInstance present (eg with a vehicle).
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene. 
 */
void USkeletalMeshComponent::TermComponentRBPhys(FRBPhysScene* InScene)
{
#if WITH_APEX
		ReleaseApexClothing(); // release any previously allocated apex clothing
#endif
	Super::TermComponentRBPhys(InScene);

	TermArticulated(InScene);

	// Iterate over attached components.
	for(UINT AttachmentIndex = 0;AttachmentIndex < (UINT)Attachments.Num();AttachmentIndex++)
	{
		FAttachment& Attachment = Attachments(AttachmentIndex);
		if(Attachment.Component)
		{
			Attachment.Component->TermComponentRBPhys(InScene);
		}
	}

	// Term cloth
	TermClothSim(InScene);
	TermSoftBodySim(InScene);
}

/**
 *	Set value of bHasPhysicsAssetInstance flag.
 *	Will create/destroy PhysicsAssetInstance as desired.
 */
void USkeletalMeshComponent::SetHasPhysicsAssetInstance(UBOOL bHasInstance, UBOOL bUseCurrentPosition)
{
	// Turning it from not having asset instance to having one.
	if(bHasInstance && !bHasPhysicsAssetInstance)
	{
		if (!bUseCurrentPosition)
		{
			UpdateSkelPose();
		}
		InitArticulated(bSkelCompFixed);
	}
	// Turning it from have an instance to not having one.
	else if(!bHasInstance && bHasPhysicsAssetInstance)
	{
		TermArticulated(NULL);
	}

	bHasPhysicsAssetInstance = bHasInstance;
}

/** Script version of SetHasPhysicsAssetInstance */
void USkeletalMeshComponent::execSetHasPhysicsAssetInstance( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bHasInstance);
	P_GET_UBOOL_OPTX(bForceReInit, FALSE);
	P_FINISH;
	
	SetHasPhysicsAssetInstance(bHasInstance, bForceReInit);
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execSetHasPhysicsAssetInstance);

/** Initialise cloth simulation for this component, using information and simulation parameters in the SkeletalMesh. */
void USkeletalMeshComponent::InitClothSim(FRBPhysScene* Scene)
{
	if( !SkeletalMesh )
	{
		return;
	}
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	// Need a mesh and a scene. Also, do nothing if we already have a sim
	if( !Scene || ClothSim)
	{
		return;
	}

#if DEDICATED_SERVER
	//Vertex buffers have been removed, so this is not supported @TODO JM Investigate collision issues with cloth
	return; 
#endif

	// Get overall scaling factor
	FVector TotalScale = Scale * Scale3D;
	if (Owner != NULL)
	{
		TotalScale *= Owner->DrawScale3D * Owner->DrawScale;
	}

	if(!TotalScale.IsUniform())
	{
		debugf( TEXT("InitClothSim : Only supported on uniformly scaled meshes.") );
		return;
	}

	// Ask the SkeletalMesh for the ClothMesh - it keeps them cached at different scales.
	NxClothMesh* ClothMesh = SkeletalMesh->GetClothMeshForScale(TotalScale.X);
	if(ClothMesh)
	{
		FrozenLocalToWorldPos = LocalToWorld.GetOrigin();
		FrozenLocalToWorldRot = LocalToWorld.Rotator();
		
		NxClothDesc ClothDesc;
		ClothDesc.clothMesh = ClothMesh;

		// Set up buffers for where to put updated mesh data.
		FStaticLODModel* LODModel = &(SkeletalMesh->LODModels(0));

		ClothDynamicBlendWeight = ClothBlendWeight;

		INT ClothMeshPosSize = SkeletalMesh->ClothToGraphicsVertMap.Num();
		INT ClothMeshIndexSize = SkeletalMesh->ClothIndexBuffer.Num();

		//Add reserve indices/vertices to accomodate tearing.
		//Only enable tearing when we are not welding.
		if(SkeletalMesh->bEnableClothTearing && (SkeletalMesh->ClothWeldingMap.Num() == 0))
		{
			ClothMeshPosSize += SkeletalMesh->ClothTearReserve;

			ClothMeshParentData.Empty();
			ClothMeshParentData.AddZeroed(ClothMeshIndexSize);
		}

		// Size output arrays to correct sizes.
		ClothMeshPosData.Empty();
		ClothMeshPosData.AddZeroed(ClothMeshPosSize);

		ClothMeshNormalData.Empty();
		ClothMeshNormalData.AddZeroed(ClothMeshPosSize);

		ClothMeshIndexData.Empty();
		ClothMeshIndexData.AddZeroed(ClothMeshIndexSize);

		// Make sure the index buffer starts out initialized and valid!
		for(INT i=0; i<SkeletalMesh->ClothIndexBuffer.Num(); i++)
		{
			ClothMeshIndexData(i) = SkeletalMesh->ClothIndexBuffer(i);
		}

		//Make sure the position buffer starts out initialized
		SkeletalMesh->ComputeClothSectionVertices(ClothMeshPosData, TotalScale.X, TRUE);

		// Fill in output description

		if (SkeletalMesh->ClothWeldingMap.Num() == 0)
		{
			ClothDesc.meshData.verticesPosBegin = ClothMeshPosData.GetData();
			ClothDesc.meshData.verticesPosByteStride = sizeof(FVector);
			ClothDesc.meshData.verticesNormalBegin = ClothMeshNormalData.GetData();
			ClothDesc.meshData.verticesNormalByteStride = sizeof(FVector);
			ClothDesc.meshData.indicesBegin = ClothMeshIndexData.GetData();
			ClothDesc.meshData.indicesByteStride = sizeof(INT);

			ClothDesc.meshData.maxVertices = ClothMeshPosSize;
			ClothDesc.meshData.maxIndices = ClothMeshIndexSize;

			if(SkeletalMesh->bEnableClothTearing)
			{
				ClothDesc.meshData.parentIndicesBegin = ClothMeshParentData.GetData();
				ClothDesc.meshData.parentIndicesByteStride = sizeof(INT);
				ClothDesc.meshData.maxParentIndices = ClothMeshIndexSize;
				ClothDesc.meshData.numParentIndicesPtr = (NxU32 *)&NumClothMeshParentIndices;
			}
		}
		else
		{
			check(SkeletalMesh->ClothWeldingDomain > 0);

			INT ClothMeshWeldedPosSize = SkeletalMesh->ClothWeldingDomain;
			INT ClothMeshWeldedIndexSize = SkeletalMesh->ClothWeldedIndices.Num();
			// Allocate alternate buffers at right size
			
			ClothMeshWeldedPosData.Empty();
			ClothMeshWeldedPosData.AddZeroed(ClothMeshWeldedPosSize);

			ClothMeshWeldedNormalData.Empty();
			ClothMeshWeldedNormalData.AddZeroed(ClothMeshWeldedPosSize);

			ClothMeshWeldedIndexData.Empty();
			ClothMeshWeldedIndexData.AddZeroed(ClothMeshWeldedIndexSize);

			//Make sure the welded position buffer starts out initialized
			SkeletalMesh->ComputeClothSectionVertices(ClothMeshPosData, TotalScale.X, FALSE);

			//Make sure the welded index buffer starts out initialized;
			for(INT i=0; i<SkeletalMesh->ClothWeldedIndices.Num(); i++)
			{
				ClothMeshWeldedIndexData(i) = SkeletalMesh->ClothWeldedIndices(i);
			}

			ClothDesc.meshData.verticesPosBegin = ClothMeshWeldedPosData.GetData();;
			ClothDesc.meshData.verticesPosByteStride = sizeof(FVector);
			ClothDesc.meshData.verticesNormalBegin = ClothMeshWeldedNormalData.GetData();
			ClothDesc.meshData.verticesNormalByteStride = sizeof(FVector);
			ClothDesc.meshData.indicesBegin = ClothMeshWeldedIndexData.GetData();
			ClothDesc.meshData.indicesByteStride = sizeof(INT);
			ClothDesc.meshData.maxVertices = ClothMeshWeldedPosData.Num();
			ClothDesc.meshData.maxIndices = ClothMeshWeldedIndexData.Num();
		}

		ClothDesc.meshData.numVerticesPtr = (NxU32*)(&NumClothMeshVerts);
		ClothDesc.meshData.numIndicesPtr = (NxU32*)(&NumClothMeshIndices);
		ClothDesc.meshData.dirtyBufferFlagsPtr = (NxU32*)(&ClothDirtyBufferFlag);

		// Can only set density at creation time.
		ClothDesc.density = SkeletalMesh->ClothDensity;

		// Enable 'hard stretch limit'
		if(SkeletalMesh->bHardStretchLimit)
		{
#if !FINAL_RELEASE
			if(!SkeletalMesh->bLimitClothToAnimMesh)
			{
				debugf(TEXT("WARNING: (%s) Cloth using bHardStretchLimit, but bLimitClothToAnimMesh is FALSE."), *GetPathName());
			}
#endif

			ClothDesc.flags  |= NX_CLF_HARD_STRETCH_LIMITATION;
			ClothDesc.hardStretchLimitationFactor = SkeletalMesh->HardStretchLimitFactor;
		}

		// Set physics collision mask on cloth
		NxGroupsMask ClothMask = CreateGroupsMask(ClothRBChannel, &ClothRBCollideWithChannels);
		ClothDesc.groupsMask = ClothMask;

		// Set position of cloth in space.
		FMatrix SkelMeshCompTM = LocalToWorld;
		SkelMeshCompTM.RemoveScaling();

		if (SkeletalMesh->bClothMetal)
		{
			// Compute the bounding box and it's center
			// Note: relies on correctly initialized position buffer.
			
			NxBounds3 Bounds;
			Bounds.setEmpty();

			for(INT i=0; i<ClothMeshPosData.Num(); i++)
			{
				Bounds.include( U2NVectorCopy(ClothMeshPosData(i)) );
			}

			NxVec3 Center;
			Bounds.getCenter(Center);

			// This is a workaround for metal cloth
			// This makes sure that when the metal cloth is attached, the bounding box of the cloth is exactly centered and thus does not
			// shift the metal cloth anymore!
			ClothDesc.globalPose.M.id();
			ClothDesc.globalPose.t = -Center;
		}
		else
		{
			ClothDesc.globalPose = U2NTransform(SkelMeshCompTM);
		}

		// Get the physics scene.
		NxScene* NovodexScene = Scene->GetNovodexPrimaryScene();
		check(NovodexScene);

#if !FINAL_RELEASE
		ClothDesc.flags |= NX_CLF_VISUALIZATION;
#endif 

		// Dont try and use compartment in editor tools!
		if(bClothUseCompartment && (!GIsEditor || GIsGame))
		{
			ClothDesc.compartment = Scene->GetNovodexClothCompartment();
			if(IsPhysXHardwarePresent())
			{
				ClothDesc.flags |= NX_CLF_HARDWARE;
			}
		}
		else if( IsPhysXHardwarePresent() )
		{
			debugf( NAME_PerfWarning, TEXT("bClothUseCompartment not set, no HW acceleration. SkeletalMeshComponent: %s, SkeletalMesh: %s"), *GetName(), *SkeletalMesh->GetName() );
		}

		ClothDesc.relativeGridSpacing = SkeletalMesh->ClothRelativeGridSpacing;

		// Cloth tearing must be enabled before cloth creation.
		if(SkeletalMesh->bEnableClothTearing && (SkeletalMesh->ClothWeldingMap.Num() == 0))
		{
			ClothDesc.flags |= NX_CLF_TEARABLE;
		}

		// Actually create cloth object.
		// just in case physics is still running... make sure we stall.
		WaitForNovodexScene(*NovodexScene);
		NxCloth* NewCloth = NovodexScene->createCloth( ClothDesc );
		NumClothMeshVerts = NewCloth->getNumberOfParticles();

		// Save pointer to cloth object
		ClothSim = NewCloth;

		EnableClothValidBounds(SkeletalMesh->bEnableValidBounds);
		SetClothValidBounds(SkeletalMesh->ValidBoundsMin,SkeletalMesh->ValidBoundsMax);
		if(SkeletalMesh->bEnableClothTearing && !SkeletalMesh->bEnableValidBounds)
		{
			debugf(NAME_PerfWarning,TEXT("bEnableValidBounds not set while bEnableClothTearing is set. Possible performance issue.  SkeletalMeshComponent: %s, SkeletalMesh: %s"), *GetName(), *SkeletalMesh->GetName() );
		}

		// Save index of scene we put it into
		SceneIndex = Scene->NovodexSceneIndex;

		// Push params into this instance of the cloth.
		UpdateClothParams();
		SetClothExternalForce(ClothExternalForce);
		SetClothFrozen(bClothFrozen);

		// Initialise fixed verts.
		UpdateFixedClothVerts();

		InitClothBreakableAttachments();

		if(!bClothAwakeOnStartup)
		{
			NewCloth->putToSleep();
		}
	}
#endif
}

/** Attach breakable cloth vertex attachment to physics asset. */
void USkeletalMeshComponent::InitClothBreakableAttachments()
{  
#if WITH_NOVODEX && !NX_DISABLE_CLOTH

	if((SkeletalMesh == NULL) || (PhysicsAsset == NULL) || (PhysicsAssetInstance == NULL))
	{
		return;
	}

	const TArray<FClothSpecialBoneInfo>& SpecialBones = SkeletalMesh->ClothSpecialBones;

	if((ClothSim == NULL) || (SpecialBones.Num() <= 0))
	{
		return;
	}

	NxCloth* nCloth = (NxCloth *)ClothSim;

	//This should happen before tearing can change the number of vertices.
	check( NumClothMeshVerts == nCloth->getNumberOfParticles() );
	NxVec3* nClothPositions = (NxVec3 *)appMalloc(sizeof(NxVec3) * NumClothMeshVerts);

	//get the positions directly from the particles as we have not simulated yet(and the mesh data is not valid).
	nCloth->getPositions(nClothPositions, sizeof(NxVec3));

	for(INT i=0; i<SpecialBones.Num(); i++)
	{
		// Decide on attachment type.

		UBOOL BreakableAttach = FALSE;

		if(SpecialBones(i).BoneType == CLOTHBONE_BreakableAttachment)
		{
			BreakableAttach = TRUE;
		}
		else if(SpecialBones(i).BoneType == CLOTHBONE_Fixed)
		{
			BreakableAttach = FALSE;
		}
		else
		{
			continue;
		}


		INT BoneIndex = MatchRefBone(SpecialBones(i).BoneName);
		
		//attach to the bones actor in the PhysicsAsset.
		if(BoneIndex != INDEX_NONE)
		{
			check(BoneIndex < 255);

			INT BodyIndex = PhysicsAsset->FindControllingBodyIndex(SkeletalMesh, BoneIndex);
			if(BodyIndex != INDEX_NONE)
			{
				NxActor* nActor = PhysicsAssetInstance->Bodies(BodyIndex)->GetNxActor();


				//Just use first shape(should we choose more carefully? all shapes should be rigid in a compound...)

				if(nActor != NULL && (nActor->getNbShapes() > 0))
				{

					for(INT j=0; j<SpecialBones(i).AttachedVertexIndices.Num(); j++)
					{
						INT VertexIndex = SpecialBones(i).AttachedVertexIndices(j);

						NxShape* nShape = nActor->getShapes()[0];
						NxMat34 nShapePose = nShape->getGlobalPose();
							
						NxVec3 nLocalPos;
						nShapePose.multiplyByInverseRT(nClothPositions[VertexIndex], nLocalPos);
						
						// just in case physics is still running... make sure we stall.
						NxScene &nScene = nCloth->getScene();
						WaitForNovodexScene(nScene);
						
						if(BreakableAttach)
						{
							nCloth->attachVertexToShape(VertexIndex, nShape, nLocalPos, NX_CLOTH_ATTACHMENT_TEARABLE);
						}
						else
						{
							nCloth->attachVertexToShape(VertexIndex, nShape, nLocalPos, 0);
						}
					}
				}
			}
		}
	}
	appFree(nClothPositions);
	nClothPositions = NULL;

#endif // WITH_NOVODEX && !NX_DISABLE_CLOTH
}


/** Stop cloth simulation and clean up simulation objects. */
void USkeletalMeshComponent::TermClothSim(FRBPhysScene* Scene)
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	// Nothing to do if no simulation object
	if(!ClothSim)
	{
		return;
	}

	// If in right scene, or no scene supplied, clean up.
	if(Scene == NULL || SceneIndex == Scene->NovodexSceneIndex)
	{
		// This will return NULL if this scene has already been shut down. 
		// If it has - do nothing - cloth will have been cleaned up with the scene.
		NxScene* NovodexScene = GetNovodexPrimarySceneFromIndex(SceneIndex);
		if(NovodexScene)
		{
			NxCloth* nCloth = (NxCloth*)ClothSim;
			#if SUPPORT_DOUBLE_BUFFERING
			if (Scene == NULL || Scene->UsingBufferedScene)
			#endif
			{
				// just in case physics is still running...
				// Set the NxMeshData pointers to NULL so PhysX will not try to access the released buffers
				NxMeshData NullMeshData;
				static_cast<NxCloth*>(ClothSim)->setMeshData(NullMeshData);

				GNovodexPendingKillCloths.AddItem(nCloth);
			}
			#if SUPPORT_DOUBLE_BUFFERING
			else
			{
				NovodexScene->releaseCloth(*nCloth);
			}
			#endif

			ClothSim = NULL;
		}
	}
#endif
}

void USkeletalMeshComponent::InitClothMetal()
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if (!bEnableClothSimulation)
	{
		debugf( TEXT("Warning: bClothMetal is set but bEnableClothSimulation is not set for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else if (ClothSim == NULL)
	{
		debugf( TEXT("Warning: bClothMetal is set but no Cloth has been generated for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else if (!bHasPhysicsAssetInstance)
	{
		debugf( TEXT("Warning: bClothMetal is set but bHasPhysicsAssetInstance is not set for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else if (PhysicsAssetInstance == NULL)
	{
		debugf( TEXT("Warning: bClothMetal is set but it has no PhysicsAssetInstance for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else if (PhysicsAssetInstance->Bodies.Num() == 0)
	{
		debugf( TEXT("Warning: bClothMetal is set but no bodies have been created for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else if (PhysicsAssetInstance->Bodies.Num() > 1)
	{
		debugf( TEXT("Warning: bClothMetal is set but more than one body has been created for %s with skeletalmesh: %s"), *GetFullName(), SkeletalMesh?*SkeletalMesh->GetFullName():TEXT("NULL") );
	}
	else
	{
		UBOOL InvalidActor = FALSE;
		NxCloth* nCloth = (NxCloth*)ClothSim;
		NxActor* nActor = PhysicsAssetInstance->Bodies(0)->GetNxActor();

		// reset the mass density based?
		NxReal Volume = 0;
		for (UINT i = 0; i < nActor->getNbShapes(); i++)
		{
			NxShape* nShape = nActor->getShapes()[i];
			switch(nShape->getType())
			{
			case NX_SHAPE_BOX:
				{
					NxBoxShape* nBox = nShape->isBox();
					NxVec3 D = nBox->getDimensions();
					Volume += D.x * D.y * D.z;
				} break;
			case NX_SHAPE_SPHERE:
				{
					NxSphereShape* nSphere = nShape->isSphere();
					NxReal R = nSphere->getRadius();
					Volume += 4.f / 3.f * NxPi * R * R * R;
				} break;
			case NX_SHAPE_CAPSULE:
				{
					NxCapsuleShape* nCapsule = nShape->isCapsule();
					NxReal R = nCapsule->getRadius();
					// Sphere part
					Volume += 4.f / 3.f * NxPi * R * R * R;
					// Cylinder part
					Volume += 2 * NxPi * R * R * nCapsule->getHeight();
				} break;
			case NX_SHAPE_CONVEX:
				{
					debugf( TEXT("Warning: Metal Cloth can not be used in conjunction with Convex shapes") );
					InvalidActor = TRUE;
				} break;
			case NX_SHAPE_MESH:
				{
					debugf( TEXT("Warning: Metal Cloth can not be used in conjunction with Mesh shapes") );
					InvalidActor = TRUE;
				} break;
			case NX_SHAPE_HEIGHTFIELD:
				{
					debugf( TEXT("Warning: Metal Cloth can not be used in conjunction with Heightfield shapes") );
					InvalidActor = TRUE;
				} break;
			case NX_SHAPE_WHEEL:
				{
					debugf( TEXT("Warning: Metal Cloth can not be used in conjunction with Wheel shapes") );
					InvalidActor = TRUE;
				} break;
			default:
				{
					debugf( TEXT("Warning: Metal Cloth can not be used in conjunction with shapes of unknown type") );
					InvalidActor = TRUE;
				} break;
			}

		}

		if (!InvalidActor && Volume <= 0)
		{
			debugf( TEXT("Warning: Metal Cloth can not be created in conjunction with actor of 0 volume") );
			InvalidActor = TRUE;
		}

		if (!InvalidActor)
		{
			check(Volume > 0);
			
			// just in case physics is still running, make sure we stall until its done.
			NxScene &nScene = nCloth->getScene();
			WaitForNovodexScene(nScene);
			
			NxReal Density = nActor->getMass() / Volume;
			//actor->updateMassFromShapes(density, 0);
			nActor->updateMassFromShapes(nCloth->getDensity(), 0);

			nCloth->attachToCore(nActor, SkeletalMesh->ClothMetalImpulseThreshold, SkeletalMesh->ClothMetalPenetrationDepth, SkeletalMesh->ClothMetalMaxDeformationDistance);
		}
	}
#endif
}

/** Update params of the this components internal cloth sim from the SkeletalMesh properties. */
void USkeletalMeshComponent::UpdateClothParams()
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if(!ClothSim || !SkeletalMesh)
	{
		return;
	}

	NxCloth* nCloth = (NxCloth*)ClothSim;

	nCloth->setThickness(SkeletalMesh->ClothThickness * U2PScale);
	nCloth->setBendingStiffness( ::Clamp(SkeletalMesh->ClothBendStiffness, 0.f, 1.f) );
	nCloth->setStretchingStiffness( ::Clamp(SkeletalMesh->ClothStretchStiffness, 0.f, 1.f) );
	nCloth->setDampingCoefficient( ::Clamp(SkeletalMesh->ClothDamping,0.f,1.f) );
	nCloth->setSolverIterations(SkeletalMesh->ClothIterations);
	nCloth->setHierarchicalSolverIterations(SkeletalMesh->ClothHierarchicalIterations);
	nCloth->setFriction(SkeletalMesh->ClothFriction);

	nCloth->setSleepLinearVelocity(SkeletalMesh->ClothSleepLinearVelocity);
	nCloth->setPressure(::Max(SkeletalMesh->ClothPressure, 0.0f));
	nCloth->setCollisionResponseCoefficient(::Max(SkeletalMesh->ClothCollisionResponseCoefficient, 0.0f));
	nCloth->setAttachmentResponseCoefficient(::Max(SkeletalMesh->ClothAttachmentResponseCoefficient, 0.0f));


	if(ClothAttachmentTearFactor >= 0.0f)
	{
		nCloth->setAttachmentTearFactor(::Max(ClothAttachmentTearFactor, 1.0f));
	}
	else
	{
		nCloth->setAttachmentTearFactor(::Max(SkeletalMesh->ClothAttachmentTearFactor, 1.0f));
	}



	NxU32 Flags = nCloth->getFlags();
	if( SkeletalMesh->bEnableClothOrthoBendConstraints ) 
	{
		Flags |= NX_CLF_BENDING | NX_CLF_BENDING_ORTHO;
	}
	else if( SkeletalMesh->bEnableClothBendConstraints )
	{
		Flags |= NX_CLF_BENDING;
	}
	else
	{
		Flags &= ~ (NX_CLF_BENDING | NX_CLF_BENDING_ORTHO);
	}

	nCloth->setTearFactor(SkeletalMesh->ClothTearFactor);

	if(SkeletalMesh->bEnableClothTearing && (SkeletalMesh->ClothWeldingMap.Num() == 0))
	{
		Flags |= NX_CLF_TEARABLE;
	}
	else
	{
		Flags &= ~NX_CLF_TEARABLE;
	}
	
	if( SkeletalMesh->bEnableClothDamping )
	{
		Flags |= NX_CLF_DAMPING;
	}
	else
	{
		Flags &= ~NX_CLF_DAMPING;
	}

	if( bDisableClothCollision )
	{
		Flags |= NX_CLF_DISABLE_COLLISION;
	}
	else
	{
		Flags &= ~NX_CLF_DISABLE_COLLISION;
	}

	if( SkeletalMesh->bUseClothCOMDamping )
	{
		Flags |= NX_CLF_COMDAMPING;
	}
	else
	{
		Flags &= ~NX_CLF_COMDAMPING;
	}

	// Enables/Disables cloth internal pressure.
	if( SkeletalMesh->bEnableClothPressure )
	{
		Flags |= NX_CLF_PRESSURE;
	}
	else
	{
		Flags &= ~NX_CLF_PRESSURE;
	}
	
	// Enables/Disables cloth self collision.
	if( SkeletalMesh->bEnableClothSelfCollision )
	{
		Flags |= NX_CLF_SELFCOLLISION;
	}
	else
	{
		Flags &= ~NX_CLF_SELFCOLLISION;
	}
	
	// Enables/Disables cloth vs rigid-body two-way collision
	if( SkeletalMesh->bEnableClothTwoWayCollision )
	{
		Flags |= NX_CLF_COLLISION_TWOWAY;
	}
	else
	{
		Flags &= ~NX_CLF_COLLISION_TWOWAY;
	}

	nCloth->setFlags( Flags );

	// Set up cloth vert movement limits
	if(SkeletalMesh->bLimitClothToAnimMesh)
	{
		TArray<NxClothConstrainCoefficients> ClothCoeffs;

		INT NumSimVerts = nCloth->getNumberOfParticles();
		ClothCoeffs.AddZeroed(NumSimVerts);

		NxReal MaxAllowedDist = SkeletalMesh->ClothToAnimMeshMaxDist * U2PScale;

		for(INT ClothVertIdx=0; ClothVertIdx<SkeletalMesh->ClothToGraphicsVertMap.Num(); ClothVertIdx++)
		{
			UBOOL bFreeVert = (ClothVertIdx < SkeletalMesh->NumFreeClothVerts);

			INT SimVertIdx = ClothVertIdx;
			if(SkeletalMesh->ClothWeldingMap.Num() > 0)
			{
				SimVertIdx = SkeletalMesh->ClothWeldingMap(ClothVertIdx);
			}

			// If less than NumFreeClothVerts, its 'free', so we just limit distance. If greater - distance is zero (locked)
			ClothCoeffs(SimVertIdx).maxDistance = bFreeVert ? MaxAllowedDist : 0.f;

			// If desired, scale the distance we limit the cloth vertex to per-vertex (from info in skel mesh)
			if(SkeletalMesh->ClothMovementScale.Num() > 0)
			{
				check(SkeletalMesh->ClothMovementScale.Num() == SkeletalMesh->ClothToGraphicsVertMap.Num());
				ClothCoeffs(SimVertIdx).maxDistance *= SkeletalMesh->ClothMovementScale(ClothVertIdx);
			}
		}

		nCloth->setConstrainCoefficients(ClothCoeffs.GetData(), sizeof(NxClothConstrainCoefficients));
	}
	else
	{
		// JTODO: Is it possible to turn constraints off again?
	}
#endif
}

/** Script version of UpdateClothParams. */
void USkeletalMeshComponent::execUpdateClothParams( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	UpdateClothParams();
}

/** 
 *	Update the external force applied to all vertices in the simulated cloth. Can be used for wind etc.
 *	Will be applied until changed.
 */
void USkeletalMeshComponent::SetClothExternalForce(const FVector& InForce)
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if(ClothSim)
	{
		NxCloth* nCloth = (NxCloth*)ClothSim;
		nCloth->setExternalAcceleration( U2NPosition(InForce) );
	}
#endif

	ClothExternalForce = InForce;
}

/** Script version of SetClothExternalForce. */
void USkeletalMeshComponent::execSetClothExternalForce( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(InForce);
	P_FINISH;

	SetClothExternalForce(InForce);
}

void USkeletalMeshComponent::UpdateClothBounds()
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH

	if(ClothSim != NULL)
	{
		NxCloth* nCloth = (NxCloth *)ClothSim;
		
		/* Note: getWorldBounds() does not include the cloth thickness in the calculation. */
		/* TODO: We need to expand the bounds to also encompass the collision obb when active */
		
		NxBounds3 nClothBounds;
		nCloth->getWorldBounds(nClothBounds);

		FLOAT Thickness = nCloth->getThickness();
		Thickness *= P2UScale;

		FBox ClothBox = FBox( N2UPosition(nClothBounds.min), N2UPosition(nClothBounds.max) );
		ClothBox = ClothBox.ExpandBy(Thickness);		

		//FBox ClothBox = FBox(FVector(-WORLD_MAX), FVector(WORLD_MAX));
		/*Decide if this mesh is only cloth(in which case we can ignore the bones)*/

		if( (SkeletalMesh != NULL) && SkeletalMesh->IsOnlyClothMesh())
			Bounds = FBoxSphereBounds(ClothBox);
		else
			Bounds = Bounds + FBoxSphereBounds(ClothBox);
	}

#endif

//TODO: Move this to UpdateSoftBodyBounds() or rename
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY

	if(SoftBodySim != NULL)
	{
		NxSoftBody* nSoftBody = (NxSoftBody *)SoftBodySim;

		NxBounds3 nSoftBodyBounds;
		nSoftBody->getWorldBounds(nSoftBodyBounds);

		FBox SoftBodyBox = FBox(N2UPosition(nSoftBodyBounds.min), N2UPosition(nSoftBodyBounds.max) );

		Bounds = Bounds + FBoxSphereBounds(SoftBodyBox);
	}
#endif //WITH_NOVODEX && !NX_DISABLE_SOFTBODY

}

/** Attach/detach verts from physics body that this components actor is attached to. */
void USkeletalMeshComponent::SetAttachClothVertsToBaseBody(UBOOL bAttachVerts)
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if(ClothSim)
	{
		NxCloth* nCloth = (NxCloth*)ClothSim;
	
		if(bAttachVerts)
		{
			// First get the body that this Owner is attached to.
			NxActor* nBaseActor = NULL;
			AActor* OwnerActor = GetOwner();
			if(OwnerActor && OwnerActor->Base)
			{
				if(OwnerActor->BaseSkelComponent && OwnerActor->BaseBoneName != NAME_None)
				{
					nBaseActor = OwnerActor->BaseSkelComponent->GetNxActor(OwnerActor->BaseBoneName);
				}
				else
				{
					if(OwnerActor->Base->CollisionComponent)
					{
						nBaseActor = OwnerActor->Base->CollisionComponent->GetNxActor();
					}
				}
			}

			// Check they are in the same scene.
			if(nBaseActor && (&(nBaseActor->getScene()) != &(nCloth->getScene())))
			{
				debugf(TEXT("SetAttachClothVertsToBaseBody : Cloth and body not in same scene (%s)"), *GetPathName());
				nBaseActor = NULL;
			}

			// If we got a body, attach verts.
			if(nBaseActor)
			{
				check(nBaseActor->getNbShapes() > 0);
				NxShape* const* nShapes = nBaseActor->getShapes();
				NxShape* nShape = nShapes[0];
				check(nShape);

				FStaticLODModel& Model = SkeletalMesh->LODModels(0);
				NxMat34 nShapeToWorld = nShape->getGlobalPose();
				NxMat34 nWorldToShape;
				nShapeToWorld.getInverse(nWorldToShape);

				for(INT i=SkeletalMesh->NumFreeClothVerts; i<SkeletalMesh->ClothToGraphicsVertMap.Num(); i++)
				{
					// Find the index of the graphics vertex that corresponds to this cloth vertex
					INT GraphicsIndex = SkeletalMesh->ClothToGraphicsVertMap(i);

					// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
					INT ChunkIndex;
					INT VertIndex;
					UBOOL bSoftVertex;
					Model.GetChunkAndSkinType(GraphicsIndex, ChunkIndex, VertIndex, bSoftVertex);

					check(ChunkIndex < Model.Chunks.Num());
					const FSkelMeshChunk& Chunk = Model.Chunks(ChunkIndex);

					FVector RefPosePosWorld(0,0,0);
					if(bSoftVertex)
					{
						RefPosePosWorld = LocalToWorld.TransformFVector(Model.VertexBufferGPUSkin.GetVertexPosition(Chunk.GetSoftVertexBufferIndex()+VertIndex));
					}
					else
					{
						RefPosePosWorld = LocalToWorld.TransformFVector(Model.VertexBufferGPUSkin.GetVertexPosition(Chunk.GetRigidVertexBufferIndex()+VertIndex));
					}

					NxVec3 nClothVertShapeSpace = nWorldToShape * U2NPosition(RefPosePosWorld);

					// just in case physics is still running, make sure we stall until its done.
					NxScene &nScene = nCloth->getScene();
					WaitForNovodexScene(nScene);

					nCloth->freeVertex(i); // Ensure vertex is free before attaching
					nCloth->attachVertexToShape(i, nShape, nClothVertShapeSpace, 0);
				}

				bAttachClothVertsToBaseBody = TRUE;
			}
		}
		else
		{
			// Free all fixed vertices.
			for(INT i=SkeletalMesh->NumFreeClothVerts; i<SkeletalMesh->ClothToGraphicsVertMap.Num(); i++)
			{
				nCloth->freeVertex(i);
			}

			// Indicate verts are not pinned.
			bAttachClothVertsToBaseBody = FALSE;
		}
	}
	else if (!bAttachVerts)
	{
		bAttachClothVertsToBaseBody = FALSE;
	}
#endif
}

/** Script version of SetAttachClothVertsToBaseBone */
void USkeletalMeshComponent::execSetAttachClothVertsToBaseBody( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bAttachVerts);
	P_FINISH;

	SetAttachClothVertsToBaseBody(bAttachVerts);
}

/** Toggle active simulation of cloth. Cheaper than doing SetEnableClothSimulation, and keeps its shape while frozen. */
void USkeletalMeshComponent::SetClothFrozen(UBOOL bNewFrozen)
{
	// When unfreezing (untorn) cloth, first teleport vertices to ref pose if the LocalToWorld transform
	// has changed.
	if(!bClothFrozen && bNewFrozen)
	{
		FrozenLocalToWorldPos = LocalToWorld.GetOrigin();
		FrozenLocalToWorldRot = LocalToWorld.Rotator();
	}
	else if(bClothFrozen && !bNewFrozen && SkeletalMesh && !SkeletalMesh->bEnableClothTearing && 
		    (LocalToWorld.GetOrigin() != FrozenLocalToWorldPos || LocalToWorld.Rotator() != FrozenLocalToWorldRot) )
	{
		ResetClothVertsToRefPose();
	}

	bClothFrozen = bNewFrozen;

#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	if(ClothSim)
	{
		NxCloth* nCloth = (NxCloth*)ClothSim;
		NxU32 Flags = nCloth->getFlags();

		if(bClothFrozen)
		{
			Flags |= NX_CLF_STATIC;
		}
		else
		{
			Flags &= ~NX_CLF_STATIC;
		}

		nCloth->setFlags(Flags);
	}
#endif
}

/** Script version of SetClothFrozen. */
void USkeletalMeshComponent::execSetClothFrozen( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(bNewFrozen);
	P_FINISH;

	SetClothFrozen(bNewFrozen);
}



/**
 * Saves the skeletal component's current AnimSets to a temporary buffer.  You can restore them later by calling
 * RestoreSavedAnimSets().  This is the C++ version of the method.  The script version just calls this one.
 */
void USkeletalMeshComponent::SaveAnimSets()
{
	// if it does have cached value, restore to default first
	if ( bValidTemporarySavedAnimSets==TRUE )
	{
		RestoreSavedAnimSets();
	}

	if( TemporarySavedAnimSets.Num() == 0 )
	{
		TemporarySavedAnimSets.Empty();
		TemporarySavedAnimSets = AnimSets;
	}

	bValidTemporarySavedAnimSets = TRUE;
}



/**
 * Restores saved AnimSets to the master list of AnimSets and clears the temporary saved list of AnimSets.  This
 * is the C++ version of the method.  The script version just calls this one.
 */
void USkeletalMeshComponent::RestoreSavedAnimSets()
{
	if (bValidTemporarySavedAnimSets == TRUE)
	{
		AnimSets.Empty();
		if( TemporarySavedAnimSets.Num() > 0 )
		{
			AnimSets = TemporarySavedAnimSets;
			TemporarySavedAnimSets.Empty();
		}
		bValidTemporarySavedAnimSets = FALSE;
	}
}



/**
 * Saves the skeletal component's current AnimSets to a temporary buffer.  You can restore them later by calling
 * RestoreSavedAnimSets().  This is the script version of the method which wraps the C++ version.
 */
void USkeletalMeshComponent::execSaveAnimSets( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	SaveAnimSets();
}
	


/**
 * Restores saved AnimSets to the master list of AnimSets and clears the temporary saved list of AnimSets.  This
 * is the script version of the method which wraps the C++ version.
 */
void USkeletalMeshComponent::execRestoreSavedAnimSets( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	RestoreSavedAnimSets();
}

/** 
 *	Hides the specified bone.  Currently this just enforces a scale of 0 for the hidden bones. 
 *	@param	bTermBodiesBelow	Terminates physics for physics bodies below this bone in the hierarchy
 */
void USkeletalMeshComponent::HideBone( INT BoneIndex, EPhysBodyOp PhysBodyOption)
{
	if ( BoneIndex != INDEX_NONE )
	{
		LocalAtoms( BoneIndex ).SetScale(0.0f);
		BoneVisibilityStates( BoneIndex ) = BVS_ExplicitlyHidden;
		RebuildVisibilityArray();
		bRequiredBonesUpToDate = FALSE;

		if( PhysBodyOption!=PBO_None && PhysicsAssetInstance )
		{
			FName HideBoneName = SkeletalMesh->RefSkeleton(BoneIndex).Name;
			if ( PhysBodyOption == PBO_Term )
			{
				PhysicsAssetInstance->TermBodiesBelow(HideBoneName, this);
			}
			else if ( PhysBodyOption == PBO_Disable )
			{
				// Disable collision
				PhysicsAssetInstance->EnableCollisionBodiesBelow(FALSE, HideBoneName, this);
			}
		}
	}
	else
	{
		debugf(TEXT("HideBone: Invalid BoneIndex"));
	}
}

/** Unhides the specified bone. */
void USkeletalMeshComponent::UnHideBone( INT BoneIndex )
{
	if ( BoneIndex != INDEX_NONE )
	{
		LocalAtoms( BoneIndex ).SetScale(1.0f);

		//@TODO: If unhiding the child of a still hidden bone (coming in, BoneVisibilityStates(RefSkel(BoneIndex).ParentIndex) != BVS_Visible),
		// should we be re-enabling collision bodies?
		// Setting visible to true here is OK in either case as it will be reset to BVS_HiddenByParent in RecalcRequiredBones later if needed.
		BoneVisibilityStates( BoneIndex ) = BVS_Visible;
		RebuildVisibilityArray();
		bRequiredBonesUpToDate = FALSE;

		if( PhysicsAssetInstance )
		{
			FName HideBoneName = SkeletalMesh->RefSkeleton(BoneIndex).Name;
			// It's okay to turn this on for terminated bodies
			// It won't do any if BodyData isn't found
			PhysicsAssetInstance->EnableCollisionBodiesBelow(TRUE, HideBoneName, this);
		}
	}
	else
	{
		debugf(TEXT("UnHideBone: Invalid BoneIndex"));
	}
}

/** Determines if the specified bone is hidden. */
UBOOL USkeletalMeshComponent::IsBoneHidden( INT BoneIndex )
{
	if ( BoneIndex != INDEX_NONE )
	{
		return BoneVisibilityStates( BoneIndex ) != BVS_Visible;
	}
	return FALSE;
}

/** Hide bones by name and disable collision of the bodies that attach to the bonename
 * @param BoneName: bone name to hide - it will hide all children
 */
void USkeletalMeshComponent::HideBoneByName( FName BoneName, EPhysBodyOp PhysBodyOption )
{
	// Find appropriate BoneIndex
	INT BoneIndex = MatchRefBone(BoneName);
	if ( BoneIndex != INDEX_NONE )
	{
		HideBone(BoneIndex, PhysBodyOption);
	}
}

/** Unhide bones by name and enable collision of the bodies that attach to the bonename
 * @param BoneName: bone name to hide - it will hide all children
 */
void USkeletalMeshComponent::UnHideBoneByName( FName BoneName )
{
	INT BoneIndex = MatchRefBone(BoneName);
	if ( BoneIndex != INDEX_NONE )
	{
		UnHideBone(BoneIndex);
	}
}

FKCachedConvexData* USkeletalMeshComponent::GetBoneCachedPhysConvexData(const FVector& InScale3D, const FName& BoneName)
{
#if WITH_NOVODEX
	if(BoneName != NAME_None && PhysicsAsset)
	{
		// See if the body setup itself has data cooked for that scale.

		URB_BodySetup* BS = 0;
		
		// Find the specified body setup
		for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
		{
			if( PhysicsAsset->BodySetup(i)->BoneName == BoneName )
			{
				BS = PhysicsAsset->BodySetup(i);
				break;
			}
		}

		if( bUsePrecookedPhysData && 
			BS && 
			BS->PreCachedPhysDataVersion == GCurrentCachedPhysDataVersion &&
			BS->PreCachedPhysScale.Num() == BS->PreCachedPhysData.Num() )
		{
			for(INT i=0; i<BS->PreCachedPhysScale.Num(); i++)
			{
				if((BS->PreCachedPhysScale(i) - InScale3D).IsNearlyZero())
				{
					return &BS->PreCachedPhysData(i);
				}
			}
		}

		//
		// At this point the static mesh version looks for the cooked data in ULevel.
		// We are not saving any cooked data for bones in ULevel presently.
		//

	}
#endif // WITH_NOVODEX

	return NULL;
}

void USkeletalMeshComponent::execUpdateMeshForBrokenConstraints( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	UpdateMeshForBrokenConstraints();
}
IMPLEMENT_FUNCTION(USkeletalMeshComponent,INDEX_NONE,execUpdateMeshForBrokenConstraints);

#if 0 && !FINAL_RELEASE
#define DEBUGBROKENCONSTRAINTUPDATE(x) { ##x }
#else
#define DEBUGBROKENCONSTRAINTUPDATE(x)
#endif

void USkeletalMeshComponent::UpdateMeshForBrokenConstraints()
{
	// Needs to have a SkeletalMeshComponent, with a SkeletalMesh, PhysicsAsset and PhysicsAssetInstance.
	if( !SkeletalMesh || !PhysicsAsset || !PhysicsAssetInstance )
	{
		return;
	}

	DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("%3.3f UpdateMeshForBrokenConstraints"), GWorld->GetTimeSeconds());)

	// Iterate through list of constraints in the physics asset
	for(INT ConstraintInstIndex = 0; ConstraintInstIndex < PhysicsAssetInstance->Constraints.Num(); ConstraintInstIndex++)
	{
		// See if we can find a constraint that has been terminated (broken)
		URB_ConstraintInstance* ConstraintInst = PhysicsAssetInstance->Constraints(ConstraintInstIndex);
		if( ConstraintInst && ConstraintInst->bTerminated )
		{
			// Get the associated joint bone index.
			INT JointBoneIndex = MatchRefBone(PhysicsAsset->ConstraintSetup(ConstraintInstIndex)->JointName);
			if( JointBoneIndex == INDEX_NONE )
			{
				continue;
			}

			DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("  Found Broken Constraint: (%d) %s"), JointBoneIndex, *PhysicsAsset->ConstraintSetup(ConstraintInstIndex)->JointName.ToString());)

			// Get child bodies of this joint
			for(INT BodySetupIndex = 0; BodySetupIndex < PhysicsAsset->BodySetup.Num(); BodySetupIndex++)
			{
				URB_BodySetup* BodySetup = PhysicsAsset->BodySetup(BodySetupIndex);
				INT BoneIndex = MatchRefBone(BodySetup->BoneName);
				if( BoneIndex != INDEX_NONE && 
					(BoneIndex == JointBoneIndex || SkeletalMesh->BoneIsChildOf(BoneIndex, JointBoneIndex)) )
				{
					DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("    Found Child Bone: (%d) %s"), BoneIndex, *BodySetup->BoneName.ToString());)

					URB_BodyInstance* ChildBodyInst = PhysicsAssetInstance->Bodies(BodySetupIndex);
					if( ChildBodyInst )
					{
						// Unfix Body so, it is purely physical, not kinematic.
						if( ChildBodyInst->IsFixed() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("      Unfixing body."));)
							ChildBodyInst->SetFixed(FALSE);
						}
						// Turn off Bone Springs
						if( ChildBodyInst->bEnableBoneSpringLinear || ChildBodyInst->bEnableBoneSpringAngular )
						{
							DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("      Turning off bone spring."));)
							FMatrix DummyMatrix;
							ChildBodyInst->EnableBoneSpring(FALSE, FALSE, DummyMatrix);
						}
					}

					URB_ConstraintInstance* ChildConstraintInst = PhysicsAssetInstance->FindConstraintInstance(BodySetup->BoneName, PhysicsAsset);
					if( ChildConstraintInst )
					{
						if( ChildConstraintInst->bLinearXPositionDrive || ChildConstraintInst->bLinearYPositionDrive || ChildConstraintInst->bLinearZPositionDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("      Turning off LinearPositionDrive."));)
							ChildConstraintInst->SetLinearPositionDrive(FALSE, FALSE, FALSE);
						}
						if( ChildConstraintInst->bLinearXVelocityDrive || ChildConstraintInst->bLinearYVelocityDrive || ChildConstraintInst->bLinearZVelocityDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("      Turning off LinearVelocityDrive."));)
							ChildConstraintInst->SetLinearVelocityDrive(FALSE, FALSE, FALSE);
						}
						if( ChildConstraintInst->bSwingPositionDrive || ChildConstraintInst->bTwistPositionDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("      Turning off AngularPositionDrive."));)
							ChildConstraintInst->SetAngularPositionDrive(FALSE, FALSE);
						}
						if( ChildConstraintInst->bSwingVelocityDrive || ChildConstraintInst->bTwistVelocityDrive )
						{
							DEBUGBROKENCONSTRAINTUPDATE(debugf(TEXT("      Turning off AngularVelocityDrive."));)
							ChildConstraintInst->SetAngularVelocityDrive(FALSE, FALSE);
						}
					}
				}
			}
		}
	}
}

/**
 *  Retrieve various actor metrics depending on the provided type.  All of
 *  these will total the values for this component.
 *
 *  @param MetricsType The type of metric to calculate.
 *
 *  METRICS_VERTS    - Get the number of vertices.
 *  METRICS_TRIS     - Get the number of triangles.
 *  METRICS_SECTIONS - Get the number of sections.
 *
 *  @return INT The total of the given type for this component.
 */
INT USkeletalMeshComponent::GetActorMetrics(EActorMetricsType MetricsType)
{
	if(SkeletalMesh != NULL)
	{
		FStaticLODModel& LODModel = SkeletalMesh->LODModels( PredictedLODLevel );

		if(MetricsType == METRICS_VERTS)
		{
			return LODModel.NumVertices;
		}
		else if(MetricsType == METRICS_TRIS)
		{
			return LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Num() / 3;
		}
	}

	return 0;
}

//////////////// STATIC MESH COMPONENT ///////////////


/** Return the BodySetup to use for the physics of this component. Simply return the BodySetup in the StaticMesh. */
URB_BodySetup* UStaticMeshComponent::GetRBBodySetup()
{
	if(StaticMesh)
	{
		return StaticMesh->BodySetup;
	}

	return NULL;
}

void UStaticMeshComponent::CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount)
{
	check(StaticMesh);

	StaticMesh->CookPhysConvexDataForScale(Level, TotalScale3D, GetOwner(), TriByteCount, TriMeshCount, HullByteCount, HullCount);
}


/** Returns the cached pre-cooked convex mesh to use for the physics for this static mesh (if there is any). */
FKCachedConvexData* UStaticMeshComponent::GetCachedPhysConvexData(const FVector& InScale3D)
{
#if WITH_NOVODEX
	if(StaticMesh)
	{
#if WITH_NOVODEX
		// See if the body setup itself has data cooked for that scale.
		URB_BodySetup* BS = StaticMesh->BodySetup;
		if( bUsePrecookedPhysData && 
			BS && 
			BS->PreCachedPhysDataVersion == GCurrentCachedPhysDataVersion &&
			BS->PreCachedPhysScale.Num() == BS->PreCachedPhysData.Num() )
		{
			for(INT i=0; i<BS->PreCachedPhysScale.Num(); i++)
			{
				if((BS->PreCachedPhysScale(i) - InScale3D).IsNearlyZero())
				{
					return &BS->PreCachedPhysData(i);
				}
			}
		}
#endif // WITH_NOVODEX
		// If not - look in level cooked data cache.
		if(Owner)
		{
			// Query the cooked geom cache in the level for this mesh at this scale.
			ULevel* Level = Owner->GetLevel();
			check(Level);
			return Level->FindPhysStaticMeshCachedData(StaticMesh, InScale3D);
		}
	}
#endif // WITH_NOVODEX

	return NULL;
}


/** Create physics representation for this static mesh. */
void UStaticMeshComponent::InitComponentRBPhys(UBOOL bFixed)
{
#if WITH_NOVODEX
	// If no StaticMesh, or no physics scene, or we already have a BodyInstance, do nothing.
	if( !StaticMesh || !GWorld->RBPhysScene || BodyInstance || bDisableAllRigidBody)
	{
		return;
	}

	// Optimisation here. If we do not wish to block rigid bodies, and we are static so that fact cannot change, do nothing.
	if( Owner && Owner->IsStatic() && !BlockRigidBody)
	{
		return;
	}

	// If we are using 'simple' collision (ie collision from the RB_BodySetup) then we can use the base PrimitiveComponent implementation.
	// If there is no RB_BodySetup we do NOT fall back to per-triangle because of its cost.
	if(StaticMesh->UseSimpleRigidBodyCollision)
	{
		Super::InitComponentRBPhys(bFixed);
	}
	// If we are not using the BodySetup, then we need to create a Novodex tri-mesh shape based on the StaticMesh and instance it in the scene.
	else
	{
#if PERF_SHOW_PHYS_INIT_COSTS
		DOUBLE StartPerTriStaticMesh = appSeconds();
#endif

		// novodex is creating collision data for this static mesh.  For things like caves this is fine
		// if you see LOTS of these then one should consider setting the various UseSimple____ collision flags
		// If you see these during gameplay (e.g. when ever you shoot and your projectile's mesh doesn't
		// have the correct flags set, then that should be rectified asap)
#if LOOKING_FOR_PERF_ISSUES
		debugf(NAME_PerfWarning, TEXT("UStaticMeshComponent::InitComponentRBPhys: %s %s"), *StaticMesh->GetName(), *GetOwner()->GetName() );
#endif

		if(!bFixed)
		{
			debugf( TEXT("InitComponentRBPhys : Trying to make a non-fixed per-triangle StaticMesh: Name: %s Owner: %s"), *StaticMesh->GetName(), *GetOwner()->GetName() );
			return;
		}

		// Get scale factor for this mesh.
		FMatrix StaticMeshCompTM;
		FVector TotalScale;
		GetTransformAndScale(StaticMeshCompTM, TotalScale);

		// Now find the NxTriangleMesh shape to use.
		NxTriangleMesh* nTriMesh = NULL;

		// using this expression multiple times
		UBOOL bStatic = Owner && Owner->IsStatic();

		// moving BodyInstance creation outside of loop (below)
		// Then create an RB_BodyInstance for this terrain component and store a pointer to the NxActor in it.
		BodyInstance = GWorld->InstanceRBBody(NULL);
		BodyInstance->BodyData = NULL;
		BodyInstance->OwnerComponent = this;
		BodyInstance->SceneIndex = 	GWorld->RBPhysScene->NovodexSceneIndex;

		// First, check the cache of actual NxTriangleMeshs in the StaticMesh.
		// use SW mesh (index 0) to determine if work has been done
		check( StaticMesh->PhysMesh.Num() == StaticMesh->PhysMeshScale3D.Num() );
		for (INT i=0; i < StaticMesh->PhysMeshScale3D.Num() && !nTriMesh; i++)
		{
			// Found a mesh with the right scale
			if ((StaticMesh->PhysMeshScale3D(i) - TotalScale).IsNearlyZero())
			{
				nTriMesh = (NxTriangleMesh*)StaticMesh->PhysMesh(i);

				// If there is a NULL entry in the cache - go no further.
				if(!nTriMesh)
				{
					GWorld->ReturnRBBody(BodyInstance);
					BodyInstance = NULL;				
					return;
				}
			}
		}

		// If this TriangleMesh has not already been created, create it now.
		if(!nTriMesh)
		{
			// Check level cache for ready cooked data for this scale.
			FKCachedPerTriData* CachedTriData = NULL;
			if(Owner)
			{
				ULevel* Level = Owner->GetLevel();
				CachedTriData = Level->FindPhysPerTriStaticMeshCachedData(StaticMesh, TotalScale);

#if XBOX
				if(CachedTriData)
				{
					check( GetCookedPhysDataEndianess(CachedTriData->CachedPerTriData) != CPDE_LittleEndian );
				}
#endif
			}

			// If no cached data, cook it now
			FKCachedPerTriData TempTriData;
			if(!CachedTriData)
			{
				//On iphone, temporarily do all cooking on device (lacking PhysX extensions)
#if CONSOLE && !IPHONE && !ANDROID && !NGP && !PLATFORM_MACOSX
				// Never cook tri-mesh on console (sloow) - just output a warning
				debugf( TEXT("No Cached Per-Tri StaticMesh Physics Data Found Or Out Of Date (%s) (Owner: %s) - Aborting on console!"), *StaticMesh->GetName(), *Owner->GetName() );
				GWorld->ReturnRBBody(BodyInstance);
				BodyInstance = NULL;				
				return;
#else
				debugf( TEXT("No Cached Per-Tri StaticMesh Physics Data Found Or Out Of Date (%s) (Owner: %s) - Cooking Now."), *StaticMesh->GetName(), *Owner->GetName() );
				MakeCachedPerTriMeshDataForStaticMesh(&TempTriData, StaticMesh, TotalScale, NULL);
				CachedTriData = &TempTriData;
#endif
			}
			check(CachedTriData);

			// Create Novodex mesh from that info.
			FNxMemoryBuffer Buffer(&CachedTriData->CachedPerTriData);
			nTriMesh = GNovodexSDK->createTriangleMesh(Buffer);

			if(!nTriMesh)
			{
				debugf( TEXT("No mesh was created (%s) (Owner: %s)."), *StaticMesh->GetName(), *Owner->GetName() );
				GWorld->ReturnRBBody(BodyInstance);
				BodyInstance = NULL;
				return;
			}
			else
			{
				SetNxTriMeshRefCount(nTriMesh, DelayNxMeshDestruction);
				GNumPhysXTriMeshes++;
			}

			// Add to cache in StaticMesh
			StaticMesh->PhysMesh.AddItem(nTriMesh);

			StaticMesh->PhysMeshScale3D.AddItem(TotalScale);
		}

		// Get the physical material to use for the mesh.
		// TODO: Support per-triangle materials in this case.
		check(GEngine->DefaultPhysMaterial);
		UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
		if( PhysMaterialOverride )
		{
			PhysMat = PhysMaterialOverride;
		}
		else if(StaticMesh->BodySetup && StaticMesh->BodySetup->PhysMaterial)
		{
			PhysMat = StaticMesh->BodySetup->PhysMaterial;
		}

		NxTriangleMeshShapeDesc StaticMeshShapeDesc;
		StaticMeshShapeDesc.meshData = nTriMesh;

		// Only use Mesh Paging on HW RB compartments
		FRBPhysScene* UseScene = GWorld->RBPhysScene;
		NxCompartment *RBCompartment = UseScene->GetNovodexRigidBodyCompartment();
		if(RBCompartment && RBCompartment->getDeviceCode() != NX_DC_CPU)
		{
			StaticMeshShapeDesc.meshPagingMode = NX_MESH_PAGING_AUTO;
		}

		StaticMeshShapeDesc.meshFlags = 0;
		StaticMeshShapeDesc.materialIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( PhysMat );
		StaticMeshShapeDesc.groupsMask = CreateGroupsMask(RBChannel, &RBCollideWithChannels);

		NxMat34 nStaticMeshCompTM = U2NTransform(StaticMeshCompTM);

		// Create actor description and instance it.
		NxActorDesc StaticMeshActorDesc;
		StaticMeshActorDesc.shapes.pushBack(&StaticMeshShapeDesc);
		StaticMeshActorDesc.globalPose = nStaticMeshCompTM;
		StaticMeshActorDesc.density = 1.f;
		
		NxCompartment *Compartment = GWorld->RBPhysScene->GetNovodexRigidBodyCompartment();
		UBOOL bUsingCompartment    = bUseCompartment && !bStatic && Compartment;
		if(bUsingCompartment)
		{
			StaticMeshActorDesc.compartment = Compartment;
		}


		// If we are not blocking rigid bodies, disable collision.
		if(!BlockRigidBody)
		{
			StaticMeshActorDesc.flags = NX_AF_DISABLE_COLLISION;
		}

		// Create the actual NxActor using the mesh collision shape.
		NxScene* NovodexScene  = GWorld->RBPhysScene->GetNovodexPrimaryScene();
		check(NovodexScene);

#if PERF_SHOW_PHYS_INIT_COSTS
		DOUBLE StartCreate = appSeconds();
#endif
		NxActor* nStaticMeshActor = NovodexScene->createActor(StaticMeshActorDesc);
#if PERF_SHOW_PHYS_INIT_COSTS
		TotalCreateActorTime += (appSeconds() - StartCreate);
#endif

		if(nStaticMeshActor)
		{
			// Then create an RB_BodyInstance for this terrain component and store a pointer to the NxActor in it.
			BodyInstance->BodyData = (FPointer)nStaticMeshActor;
			nStaticMeshActor->userData = BodyInstance;
		}
		else
		{
			debugf(TEXT("UStaticMeshComponent::InitComponentRBPhys : Could not create NxActor: %s"), *StaticMesh->GetName());
		}

#if PERF_SHOW_PHYS_INIT_COSTS
		TotalPerTriStaticMeshTime += (appSeconds() - StartPerTriStaticMesh);
#endif
	}
#endif // WITH_NOVODEX
}

void UStaticMeshComponent::TermComponentRBPhys(FRBPhysScene* InScene)
{
	Super::TermComponentRBPhys(InScene);

}


void UStaticMesh::ClearPhysMeshCache()
{
#if WITH_NOVODEX
	// When we destroy the StaticMesh, add any Novodex meshes that were created for it to the 'pending destroy' list.
	// They should not be being used by anything if the StaticMesh is being GC'd

	for(INT i=0; i<PhysMesh.Num(); i++)
	{
		NxTriangleMesh* nTriMesh = (NxTriangleMesh*)PhysMesh(i);
		// Don't add if NULL.
		if(nTriMesh)
		{
			GNovodexPendingKillTriMesh.AddItem(nTriMesh);
		}
	}
	PhysMesh.Empty();
	PhysMeshScale3D.Empty();
#endif
}

/** Disables physics collision between a specific pair of meshes. */
void UStaticMeshComponent::DisableRBCollisionWithSMC(UPrimitiveComponent* OtherSMC, UBOOL bDisabled)
{
	if(!OtherSMC || OtherSMC == this || !this->BodyInstance || !OtherSMC->BodyInstance)
		return;

#if WITH_NOVODEX
	// Grab physics bodies
	NxActor* ActorA = this->BodyInstance->GetNxActor();
	NxActor* ActorB = OtherSMC->BodyInstance->GetNxActor();

	if (ActorA && ActorB)
	{
		NxScene* NovodexScene = &(ActorA->getScene());
		// Set or unset the 'ignore pair' flag
		NxU32 CurrentFlags = NovodexScene->getActorPairFlags(*ActorA, *ActorB);
		NxU32 NewFlags = (bDisabled) ? (CurrentFlags | NX_IGNORE_PAIR) : (CurrentFlags & ~NX_IGNORE_PAIR);
		// Apply to this pair.
		NovodexScene->setActorPairFlags(*ActorA, *ActorB, NewFlags);
	}
#endif // WITH_NOVODEX
}

/** Script version of DisableRBCollisionWithSMC. */
void UStaticMeshComponent::execDisableRBCollisionWithSMC(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UPrimitiveComponent, OtherSMC);
	P_GET_UBOOL(bDisabled);
	P_FINISH;

	DisableRBCollisionWithSMC(OtherSMC, bDisabled);
}

//////////////// BRUSH COMPONENT ///////////////

void UBrushComponent::BuildSimpleBrushCollision()
{
	if(!Owner)
	{
		warnf( TEXT("BuildSimpleBrushCollision: BrushComponent with no Owner!") );
		return;
	}

	// Get the prepivot from the Actor owner (if there is one).
	FVector PrePivot = Owner->PrePivot;

	// Convert collision model into convex hulls.
	BrushAggGeom.~FKAggregateGeom();
	appMemzero(&BrushAggGeom,sizeof(BrushAggGeom));
	KModelToHulls( &BrushAggGeom, Brush );
}

void UBrushComponent::BuildPhysBrushData()
{
	FVector TotalScale = Scale * Scale3D;
	if (Owner)
	{
		TotalScale *= (Owner->DrawScale * Owner->DrawScale3D);
	}

	// Clear out the cached data.
	CachedPhysBrushData.CachedConvexElements.Empty();

	// Do not cache PhysX geometry for volumes that don't need it
	if( !bDisableAllRigidBody )
	{
		MakeCachedConvexDataForAggGeom(&CachedPhysBrushData, BrushAggGeom.ConvexElems, TotalScale, *GetName() );
	
#if WITH_NOVODEX
		// Update cached data version
		CachedPhysBrushDataVersion = GCurrentCachedPhysDataVersion;
#endif
	}
}

void UBrushComponent::InitComponentRBPhys(UBOOL bFixed)
{
#if WITH_NOVODEX
	const UBOOL bCanCollide = (BrushPhysDesc != NULL) || (CachedPhysBrushData.CachedConvexElements.Num() > 0) || (Brush != NULL);
	// If no Brush, or no physics scene, or we already have a BodyInstance, do nothing.
	if(!bCanCollide || !GWorld->RBPhysScene || BodyInstance || bDisableAllRigidBody)
	{
		return;
	}

	UBOOL bWaterVolume = FALSE;

	FVector TotalScale = Scale * Scale3D;
	if (Owner)
	{
		TotalScale *= (Owner->DrawScale * Owner->DrawScale3D);

        // WaterVolumes are special cased so they only collide with Hover wheels 
        APhysicsVolume* P = Cast<APhysicsVolume>(Owner);
        if (P && P->bWaterVolume)
        {
            bWaterVolume = TRUE;
        }

		// Optimisation here. If we do not wish to block rigid bodies, and we are static so that fact cannot change, do nothing.
		if(Owner->IsStatic() && !BlockRigidBody && !bWaterVolume)
		{
			return;
		}
	}

	if(!bFixed)
	{
		debugf( TEXT("InitComponentRBPhys : Trying to make a non-fixed BrushComponent!") );
	}

	// If we don't have a cached set of Novodex shape descriptions, create them now
	if(!BrushPhysDesc)
	{
		// If we don't have cooked data, cook now and warn.
		if( CachedPhysBrushData.CachedConvexElements.Num() == 0 || 
			CachedPhysBrushDataVersion != GCurrentCachedPhysDataVersion ||
			!bUsePrecookedPhysData)
		{
			checkMsg(Brush != NULL, "Cannot recook brush data that has already been cooked (the model has been stripped out)");

			debugf( TEXT("No Cached Brush Physics Data Found Or Out Of Date (%s) (Owner: %s) - Cooking Now."), *Brush->GetName(), *Owner->GetName() );

			BuildSimpleBrushCollision();
			BuildPhysBrushData();
		}

		// Only continue if we got some valid hulls for this model.
		if(CachedPhysBrushData.CachedConvexElements.Num() > 0)
		{
			BrushPhysDesc = BrushAggGeom.InstanceNovodexGeom( TotalScale, &CachedPhysBrushData, FALSE, *GetFullName() );
		}

		// We don't need the cached physics data any more, so clear it
		CachedPhysBrushData.CachedConvexElements.Empty();
	}

	// If we have the shape descriptions, use them to create an Actor.
	if(BrushPhysDesc)
	{
		// Make transform for this static mesh component
		FMatrix CompTM = LocalToWorld;
		CompTM.RemoveScaling();
		NxMat34 nCompTM = U2NTransform(CompTM);

		// Create actor description and instance it.
		NxActorDesc BrushActorDesc;
		BrushActorDesc.globalPose = nCompTM;
		BrushActorDesc.density = 1.f;

		// Get the physical material to use for the model.
		check(GEngine->DefaultPhysMaterial);
		UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
		if( PhysMaterialOverride )
		{
			PhysMat = PhysMaterialOverride;
		}
		NxMaterialIndex MatIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( PhysMat );

		// Set to special group if its a water volume.
		NxGroupsMask GroupsMask = CreateGroupsMask(RBChannel, &RBCollideWithChannels);
		if(bFluidDrain && !BlockRigidBody)
		{
			GroupsMask = CreateGroupsMask(RBCC_FluidDrain, &RBCollideWithChannels);
		}

		UBOOL bStatic = Owner && Owner->IsStatic();
		if(bUseCompartment && !bStatic)
		{
			BrushActorDesc.compartment = GWorld->RBPhysScene->GetNovodexRigidBodyCompartment();
		}

		// Use the shapes descriptions from the cached actor desc.
		for(UINT i=0; i<BrushPhysDesc->shapes.size(); i++)
		{
#ifndef NX_DISABLE_FLUIDS
			// Novodex fluids
			if( bFluidDrain )
			{
				BrushPhysDesc->shapes[i]->shapeFlags |= NX_SF_FLUID_DRAIN;
			}
			else
			{
				BrushPhysDesc->shapes[i]->shapeFlags &= ~(NxU32)NX_SF_FLUID_DRAIN;
			}
			if( bFluidTwoWay )
			{
				BrushPhysDesc->shapes[i]->shapeFlags |= NX_SF_FLUID_TWOWAY;
			}
			else
			{
				BrushPhysDesc->shapes[i]->shapeFlags &= ~(NxU32)NX_SF_FLUID_TWOWAY;
			}
#endif

			BrushActorDesc.shapes.push_back( BrushPhysDesc->shapes[i] );

			// Set the material to the one specified in the PhysicalMaterial before creating this NxActor instance.
			BrushActorDesc.shapes[i]->materialIndex = MatIndex;

			// Assign collision group to each shape.
			BrushActorDesc.shapes[i]->groupsMask = GroupsMask;
		}

		// Is possible for brushes to be kinematic (eg dynamic blocking volume).
		NxBodyDesc BodyDesc;
		if(!bStatic)
		{
			BodyDesc.flags |= NX_BF_KINEMATIC;
			BrushActorDesc.body = &BodyDesc;
		}

		// Handle BlockRigidBody flag.
		if(!BlockRigidBody && !bWaterVolume && !bFluidDrain)
		{
			BrushActorDesc.flags = NX_AF_DISABLE_COLLISION;
		}

		// moving BodyInstance creation outside of loop (below)
		BodyInstance = GWorld->InstanceRBBody(NULL);
		BodyInstance->BodyData = NULL;
		BodyInstance->OwnerComponent = this;
		BodyInstance->SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;

		// Create the actual NxActor using the mesh collision shape.
		NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
		check(NovodexScene);

#if PERF_SHOW_PHYS_INIT_COSTS
		DOUBLE StartCreate = appSeconds();
#endif
		NxActor* nBrushActor = NovodexScene->createActor(BrushActorDesc);
#if PERF_SHOW_PHYS_INIT_COSTS
		TotalCreateActorTime += (appSeconds() - StartCreate);
#endif

		if(nBrushActor)
		{
			BodyInstance->BodyData = (FPointer)nBrushActor;
			nBrushActor->userData = BodyInstance;
		}
		else
		{
			debugf(TEXT("UBrushComponent::InitComponentRBPhys : Could not create NxActor: %s"), *Brush->GetName());
		}
	}
#endif // WITH_NOVODEX
}

/** We keep the convex shapes around until actual GC, in case we hide and then reshow the level before GC. */
void UBrushComponent::FinishDestroy()
{
#if WITH_NOVODEX
	if(BrushPhysDesc)
	{
		// Add any convex shapes being used to the 'pending destroy' list.
		// We can't destroy these now, because during GC we might hit this while some things are still using it.
		for(UINT j=0; j<BrushPhysDesc->shapes.size(); j++)
		{
			NxShapeDesc* ShapeDesc = BrushPhysDesc->shapes[j];
			if(ShapeDesc->getType() == NX_SHAPE_CONVEX)
			{
				NxConvexShapeDesc* ConvexDesc = (NxConvexShapeDesc*)ShapeDesc;
				GNovodexPendingKillConvex.AddItem( ConvexDesc->meshData );
			}
			delete ShapeDesc;
		}

		// Delete ActorDesc itself
		delete BrushPhysDesc;
		BrushPhysDesc = NULL;
	}
#endif // WITH_NOVODEX
	Super::FinishDestroy();
}

//////////////// TERRAIN COMPONENT ///////////////
#if WITH_NOVODEX
/** Contact modification to remove contacts generated when object is underground. */
bool FNxModifyContact::onContactConstraint(NxU32& changeFlags, 
	const NxShape* shape0, 
	const NxShape* shape1, 
	const NxU32 featureIndex0, 
	const NxU32 featureIndex1,
	NxContactCallbackData& data)
{
	const NxHeightFieldShape* HFShape = NULL;
	const NxShape* OtherShape = NULL;
	NxU32 HFFeatureIndex = (NxU32)INDEX_NONE;

	// Find which shape is the heightfield, and which is the 'other'.
	if( shape0->isHeightField() )
	{
		HFShape = static_cast<const NxHeightFieldShape*>(shape0);
		HFFeatureIndex = featureIndex0;
		OtherShape = shape1;
	}
	else if( shape1->isHeightField() )
	{
		HFShape = static_cast<const NxHeightFieldShape*>(shape1);
		HFFeatureIndex = featureIndex1;
		OtherShape = shape0;
	}

	// if this is with a terrain...
	if( HFShape )
	{
		check(OtherShape);
	
		// Check we are colliding with an actual triangle
		if(HFFeatureIndex != 0xffffffff)
		{
			NxTriangle WorldTri;
			NxU32 Flags;
			HFShape->getTriangle(WorldTri, NULL, &Flags, HFFeatureIndex);

			// Get top of bounding box of object.
			NxBounds3 Bounds;
			OtherShape->getWorldBounds(Bounds);

			// Get bottom of triangle that was used to generate this contact.
			FLOAT BoundsTop = Bounds.max.z;
			FLOAT TriBottom = ::Min3<FLOAT>(WorldTri.verts[0].z, WorldTri.verts[1].z, WorldTri.verts[2].z);

			if(BoundsTop < TriBottom)
			{
#if 0
				GWorld->LineBatcher->DrawLine( N2UPosition(WorldTri.verts[0]), N2UPosition(WorldTri.verts[1]), FColor(0,0,255), SDPG_World );
				GWorld->LineBatcher->DrawLine( N2UPosition(WorldTri.verts[1]), N2UPosition(WorldTri.verts[2]), FColor(0,0,255), SDPG_World );
				GWorld->LineBatcher->DrawLine( N2UPosition(WorldTri.verts[2]), N2UPosition(WorldTri.verts[0]), FColor(0,0,255), SDPG_World );
#endif

				return false;
			}
#if 0
			else
			{
				GWorld->LineBatcher->DrawLine( N2UPosition(WorldTri.verts[0]), N2UPosition(WorldTri.verts[1]), FColor(255,0,0), SDPG_World );
				GWorld->LineBatcher->DrawLine( N2UPosition(WorldTri.verts[1]), N2UPosition(WorldTri.verts[2]), FColor(255,0,0), SDPG_World );
				GWorld->LineBatcher->DrawLine( N2UPosition(WorldTri.verts[2]), N2UPosition(WorldTri.verts[0]), FColor(255,0,0), SDPG_World );
			}
#endif
		}
	}

	return true;
}
#endif // WITH_NOVODEX


/** Create a Novodex triangle mesh actor from this terrain component for colliding physics objects against. */
void UTerrainComponent::InitComponentRBPhys(UBOOL bFixed)
{
	// If no physics scene, or we already have a BodyInstance, do nothing.
	if( !BlockRigidBody || !GWorld->RBPhysScene || BodyInstance || bDisableAllRigidBody )
	{
		return;
	}

#if WITH_NOVODEX
	ATerrain* Terrain = CastChecked<ATerrain>(Owner);

	if(!bFixed)
	{
		debugf( TEXT("InitComponentRBPhys : Trying to make a non-fixed Terrain!") );
	}
	// Bail out if collision for the terrain isn't valid
	if (CollisionVertices.Num() == 0)
	{
		warnf(NAME_Warning,TEXT("Terrain isn't up to date, no Novodex collision data!"));
		return;
	}

#if PERF_SHOW_PHYS_INIT_COSTS
	DOUBLE StartTerrain = appSeconds();
#endif


	// Make transform for this terrain component NxActor
	FMatrix TerrainCompTM = LocalToWorld;
	TerrainCompTM.RemoveScaling();

	FVector TerrainY = TerrainCompTM.GetAxis(1);
	FVector TerrainZ = TerrainCompTM.GetAxis(2);
	TerrainCompTM.SetAxis(2, -TerrainY);
	TerrainCompTM.SetAxis(1, TerrainZ);

	NxMat34 nTerrainCompTM = U2NTransform(TerrainCompTM);

	check(GEngine->DefaultPhysMaterial);

	// Create an RB_BodyInstance for this terrain component and store a pointer to the NxActor in it.
	BodyInstance = GWorld->InstanceRBBody(NULL);
	BodyInstance->BodyData = NULL;
	BodyInstance->OwnerComponent = this;
	BodyInstance->SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;

	FVector TotalScale3D = (Terrain->DrawScale * Terrain->DrawScale3D);

	// Doesn't really matter what index this is - as long as its not the default.
	INT DefPhysMaterialIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( GEngine->DefaultPhysMaterial );
	INT HoleMaterial = DefPhysMaterialIndex + 1;

	// If we have not created a heightfield yet - do it now.
	if(!RBHeightfield)
	{
		// ROW = X
		// COLUMN = Y
		INT CollisionNumCols = TrueSectionSizeY + 1;
		INT CollisionNumRows = TrueSectionSizeX + 1;

		TArray<NxHeightFieldSample> Samples;
		Samples.AddZeroed(CollisionNumCols * CollisionNumRows);

		for(INT RowIndex = 0; RowIndex < CollisionNumRows; RowIndex++)
		{
			for(INT ColIndex = 0; ColIndex < CollisionNumCols; ColIndex++)
			{
				INT SampleIndex = (RowIndex * CollisionNumCols) + ColIndex;
				NxHeightFieldSample& Sample = Samples(SampleIndex);

				INT GlobalPatchX = SectionBaseX + RowIndex;
				INT GlobalPatchY = SectionBaseY + ColIndex;

				const FTerrainPatch& Patch = Terrain->GetPatch(GlobalPatchX, GlobalPatchY);
				FVector Vertex = Terrain->GetCollisionVertex(Patch, GlobalPatchX, GlobalPatchY, 0, 0, 1);
				FLOAT NormalizedHeight = Vertex.Z * 0.5f * TERRAIN_ZSCALE;
				NxI16 Height = ::Clamp<NxI16>(appRound(NormalizedHeight * 32767.f), -32766, 32766);
				Sample.height = Height;

				// Assign material to sample
				// JTODO: Per-triangle materials for terrain...
				INT SampleMaterial = DefPhysMaterialIndex;

				// See if quad is a hole. Use special material to indicate that if so.
				if (Terrain->IsTerrainQuadVisible(
					GlobalPatchX - (GlobalPatchX % Terrain->MaxTesselationLevel), 
					GlobalPatchY - (GlobalPatchY % Terrain->MaxTesselationLevel)) == FALSE)
				{
					SampleMaterial = HoleMaterial;
				}
				if (Terrain->IsTerrainQuadFlipped(GlobalPatchX, GlobalPatchY) == FALSE)
				{
					Sample.tessFlag = NX_HF_0TH_VERTEX_SHARED;
				}
				else
				{
					Sample.tessFlag = 0;
				}

				Sample.materialIndex0 = SampleMaterial;
				Sample.materialIndex1 = SampleMaterial;
			}
		}

		NxHeightFieldDesc HFDesc;
		HFDesc.nbColumns		= CollisionNumCols;
		HFDesc.nbRows			= CollisionNumRows;
		HFDesc.samples			= Samples.GetData();
		HFDesc.sampleStride		= sizeof(NxU32);
		HFDesc.flags			= NX_HF_NO_BOUNDARY_EDGES;

		NxHeightField* nHeightfield = GNovodexSDK->createHeightField(HFDesc);
		RBHeightfield = nHeightfield;
	}
	check(RBHeightfield);

	NxHeightFieldShapeDesc TerrainShapeDesc;
	TerrainShapeDesc.heightField	= (NxHeightField*)RBHeightfield;
	TerrainShapeDesc.shapeFlags		= NX_SF_FEATURE_INDICES | NX_SF_VISUALIZATION;
	TerrainShapeDesc.heightScale	= TotalScale3D.Z * U2PScale * TERRAIN_ZSCALE;
	TerrainShapeDesc.rowScale		= (TotalScale3D.X * U2PScale);
	TerrainShapeDesc.columnScale	= (-TotalScale3D.Y * U2PScale);
	TerrainShapeDesc.meshFlags		= 0;
	TerrainShapeDesc.materialIndexHighBits = 0;
	TerrainShapeDesc.holeMaterial	= HoleMaterial;
	TerrainShapeDesc.groupsMask		= CreateGroupsMask(RBCC_Default, NULL);

	// JTODO: Per-triangle materials for terrain...

	// Create actor description and instance it.
	NxActorDesc TerrainActorDesc;
	TerrainActorDesc.shapes.pushBack(&TerrainShapeDesc);
	TerrainActorDesc.globalPose = nTerrainCompTM;

	// Create the actual NxActor using the mesh collision shape.
	NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	check(NovodexScene);

#if PERF_SHOW_PHYS_INIT_COSTS
	DOUBLE StartCreate = appSeconds();
#endif
	NxActor* TerrainActor = NovodexScene->createActor(TerrainActorDesc);
#if PERF_SHOW_PHYS_INIT_COSTS
	TotalCreateActorTime += (appSeconds() - StartCreate);
#endif

	if(TerrainActor)
	{
		BodyInstance->BodyData = (FPointer)TerrainActor;
		TerrainActor->userData = BodyInstance;

		// If desired, we use a contact modify callback to toss contacts when object is below visible terrain
		if(Terrain->bAllowRigidBodyUnderneath)
		{
			// We need to modify contacts with terrain to remove contacts when actor is
			TerrainActor->setGroup(UNX_GROUP_MODIFYCONTACT);
		}

	}
	else
	{
		debugf(TEXT("UTerrainComponent::InitComponentRBPhys : Could not create NxActor: %s"), *Terrain->GetName());
	}
#endif

#if PERF_SHOW_PHYS_INIT_COSTS
	TotalTerrainTime += (appSeconds() - StartTerrain);
#endif
}

void UTerrainComponent::FinishDestroy()
{
#if WITH_NOVODEX
	// Free the heightfield data.
	if(RBHeightfield)
	{
		NxHeightField* HF = (NxHeightField*)RBHeightfield;
		GNovodexPendingKillHeightfield.AddItem(HF);
		RBHeightfield = NULL;
	}
#endif

	Super::FinishDestroy();
}

UBOOL UPrimitiveComponent::IsAlwaysStatic()
{
	return GetOwner() && GetOwner()->IsStatic();
}

UBOOL UStaticMeshComponent::IsAlwaysStatic()
{
	return GetOwner() && GetOwner()->IsStatic() && !CanBecomeDynamic();
}

/**
 *  Retrieve various actor metrics depending on the provided type.  All of
 *  these will total the values for this component.
 *
 *  @param MetricsType The type of metric to calculate.
 *
 *  METRICS_VERTS    - Get the number of vertices.
 *  METRICS_TRIS     - Get the number of triangles.
 *  METRICS_SECTIONS - Get the number of sections.
 *
 *  @return INT The total of the given type for this component.
 */
INT UStaticMeshComponent::GetActorMetrics(EActorMetricsType MetricsType)
{
	if(StaticMesh != NULL)
	{
		check(PreviousLODLevel >= 0 && PreviousLODLevel < StaticMesh->LODModels.Num());
		FStaticMeshRenderData* LODModel = &StaticMesh->LODModels(PreviousLODLevel);
		check(LODModel);

		if(MetricsType == METRICS_VERTS)
		{
			return LODModel->NumVertices;
		}
		else if(MetricsType == METRICS_TRIS)
		{
			return LODModel->IndexBuffer.Indices.Num() / 3;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////
// URB_RadialImpulseComponent //////////////////////////////////////
////////////////////////////////////////////////////////////////////
void URB_RadialImpulseComponent::Attach()
{
	Super::Attach();

	if(PreviewSphere)
		PreviewSphere->SphereRadius = ImpulseRadius;
}

/** Update the component's bounds */
void URB_RadialImpulseComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds(FVector(0,0,0), FVector(ImpulseRadius), ImpulseRadius).TransformBy(LocalToWorld);
}

void URB_RadialImpulseComponent::FireImpulse( FVector Origin )
{
	if( !Owner )
	{
		return;
	}

	FMemMark Mark(GMainThreadMemStack);

	FCheckResult* first = GWorld->Hash->ActorOverlapCheck(GMainThreadMemStack, Owner, Origin, ImpulseRadius);
	for(FCheckResult* result = first; result; result=result->GetNext())
	{
		UPrimitiveComponent* PokeComp = result->Component;
		if(PokeComp && !PokeComp->IsAlwaysStatic() )
		{
			PokeComp->AddRadialImpulse( Origin, ImpulseRadius, ImpulseStrength, ImpulseFalloff, bVelChange );
		}

		// See if we want to fracture stuff.
		if(bCauseFracture)
		{
			AFracturedStaticMeshActor* FracActor = Cast<AFracturedStaticMeshActor>(result->Actor);
			if(FracActor)
			{
				const UBOOL bWantPhysChunksAndParticles = TRUE;
				FracActor->eventBreakOffPartsInRadius(Origin, ImpulseRadius, ImpulseStrength, bWantPhysChunksAndParticles);
			}
		}
	}

	Mark.Pop();
}

////////////////////////////////////////////////////////////////////
// URB_Handle //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void URB_Handle::Attach()
{
	Super::Attach();

	HandleData = NULL;
}


void URB_Handle::Detach( UBOOL bWillReattach )
{
	if(GrabbedComponent)
		ReleaseComponent();

#if WITH_NOVODEX
	if(HandleData)
	{
		check(KinActorData);

#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call RB_Handle::Detach() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		// use correct scene
		NxScenePair* nPair =  GetNovodexScenePairFromIndex(SceneIndex);
		if(nPair)
		{
			NxScene* NovodexScene = nPair->PrimaryScene;
			if(NovodexScene)
			{
				// destroy joint
				NxJoint* Joint = HandleData;
				NovodexScene->releaseJoint(*Joint);

				// Destroy temporary actor.
				NxActor* KinActor = (NxActor*)KinActorData;
				NovodexScene->releaseActor(*KinActor);
			}
		}
		HandleData = NULL;
		KinActorData = NULL;
	}
#endif

	Super::Detach( bWillReattach );
}

/** 
 *	When terminating rigid-body physics for RB_Handle, release anything we are currently holding. Will destroy constraint. 
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene. 
 */
void URB_Handle::TermComponentRBPhys(FRBPhysScene* InScene)
{
#if WITH_NOVODEX
	if(HandleData)
	{
		// If it matches the one we are shutting down, release.
		if(InScene == NULL || SceneIndex == InScene->NovodexSceneIndex)
		{
			ReleaseComponent();
		}
	}
#endif // WITH_NOVODEX
}

/////////////// GRAB

void URB_Handle::GrabComponent(UPrimitiveComponent* InComponent, FName InBoneName, FVector Location, UBOOL bConstrainRotation)
{
	bInterpolating = FALSE;

	// If we are already holding something - drop it first.
	if(GrabbedComponent != NULL)
		ReleaseComponent();

	if(!InComponent)
		return;

#if WITH_NOVODEX
	// Get the NxActor that we want to grab.
	NxActor* Actor = InComponent->GetNxActor(InBoneName);
	if (!Actor || !Actor->isDynamic())
		return;

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call GrabComponent() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif

	// Get the scene the NxActor we want to grab is in.
	NxScene* Scene = &(Actor->getScene());
	check(Scene);

	// Get transform of actor we are grabbing
	NxMat34 GlobalPose = Actor->getGlobalPose();
	// set it's location so we don't need another "Tick" call to have it right
	this->Location = N2UPosition(GlobalPose.t);

	// If we don't already have a handle - make one now.
	if (!HandleData)
	{
		// Calculate handle position (Novodex scale) in world and local space.
		NxVec3 nHandlePos = U2NPosition(Location);
		NxVec3 nLocalHandlePos = GlobalPose % nHandlePos;


		// Create kinematic actor we are going to create joint with. This will be moved around with calls to SetLocation/SetRotation.
		NxActorDesc KinActorDesc;
		KinActorDesc.globalPose = GlobalPose;
		KinActorDesc.globalPose.t = nHandlePos;
		KinActorDesc.density = 1.f;
		KinActorDesc.flags |= NX_AF_DISABLE_COLLISION;
		KinActorDesc.compartment = Actor->getCompartment();

		NxSphereShapeDesc KinActorShapeDesc;
		KinActorShapeDesc.radius = 1.f;
		// Using this mechanism to ignore collision instead of NX_AF_DISABLE_COLLISION becuase its supported by hardware.
		//KinActorShapeDesc.group = UNX_COLGROUP_IGNORE_ALL;
		KinActorDesc.shapes.push_back(&KinActorShapeDesc);

		NxBodyDesc KinBodyDesc;
		KinBodyDesc.flags |= NX_BF_KINEMATIC;
		KinActorDesc.body = &KinBodyDesc;

		NxActor* KinActor = Scene->createActor(KinActorDesc);

		// No bodyinstance
		KinActor->userData = NULL;

		// Save reference to the kinematic actor.
		KinActorData = KinActor;

		// Make description for D6 joint
		NxD6JointDesc PosJointDesc;

		PosJointDesc.actor[0] = Actor;
		PosJointDesc.actor[1] = KinActor;

		PosJointDesc.localAnchor[0] = nLocalHandlePos;
		PosJointDesc.localAnchor[1] = NxVec3(0,0,0);

		PosJointDesc.projectionMode = NX_JPM_NONE;

		// hardware scene support: using D6 joint instead of spherical
		PosJointDesc.xMotion = NX_D6JOINT_MOTION_FREE;
		PosJointDesc.yMotion = NX_D6JOINT_MOTION_FREE;
		PosJointDesc.zMotion = NX_D6JOINT_MOTION_FREE;
		PosJointDesc.xDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
		PosJointDesc.xDrive.forceLimit = FLT_MAX;
		PosJointDesc.xDrive.spring = LinearStiffness * LinearStiffnessScale3D.X;
		PosJointDesc.xDrive.damping = LinearDamping * LinearDampingScale3D.X;
		PosJointDesc.yDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
		PosJointDesc.yDrive.forceLimit = FLT_MAX;
		PosJointDesc.yDrive.spring = LinearStiffness * LinearStiffnessScale3D.Y;
		PosJointDesc.yDrive.damping = LinearDamping * LinearDampingScale3D.Y;
		PosJointDesc.zDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
		PosJointDesc.zDrive.forceLimit = FLT_MAX;
		PosJointDesc.zDrive.spring = LinearStiffness * LinearStiffnessScale3D.Z;
		PosJointDesc.zDrive.damping = LinearDamping * LinearDampingScale3D.Z;

		PosJointDesc.drivePosition.set(0.0f, 0.0f, 0.0f);

		PosJointDesc.twistMotion = NX_D6JOINT_MOTION_FREE;
		PosJointDesc.swing1Motion = NX_D6JOINT_MOTION_FREE;
		PosJointDesc.swing2Motion = NX_D6JOINT_MOTION_FREE;

		bRotationConstrained = bConstrainRotation;
		if (bRotationConstrained)
		{
			PosJointDesc.twistDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
			PosJointDesc.twistDrive.spring = AngularStiffness;
			PosJointDesc.twistDrive.damping = AngularDamping;
			PosJointDesc.swingDrive.driveType = NX_D6JOINT_DRIVE_POSITION;
			PosJointDesc.swingDrive.spring = AngularStiffness;
			PosJointDesc.swingDrive.damping = AngularDamping;
			PosJointDesc.setGlobalAxis(NxVec3(0,0,1));
			PosJointDesc.driveOrientation = NxQuat(NxVec3(0,0,0),1);
		}

		// Finally actually create the joint.
		NxJoint* NewJoint = Scene->createJoint(PosJointDesc);
		
		if(NewJoint)
		{
			// No constraint instance
			NewJoint->userData = NULL;
			HandleData = NewJoint;
			// Remember the scene index that the handle joint/actor are in.
			FRBPhysScene* RBScene = (FRBPhysScene*)Scene->userData;
			SceneIndex = RBScene->NovodexSceneIndex;
		}
		else
		{
			HandleData = 0;
		}
	}

#endif // WITH_NOVODEX

	GrabbedComponent = InComponent;
	GrabbedBoneName = InBoneName;
}

/////////////// RELEASE

// APE_TODO: When an actor is destroyed, use AgRB::getJoints or something to find RB_Handles and release them first.

void URB_Handle::ReleaseComponent()
{
	bInterpolating = FALSE;

#if WITH_NOVODEX
	if(GrabbedComponent)
	{
		if(HandleData)
		{
			check(KinActorData);

#if !FINAL_RELEASE
			// Check to see if this physics call is illegal during this tick group
			if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
			{
				debugf(NAME_Error,TEXT("Can't call ReleaseComponent() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
			}
#endif

			// use correct scene
			NxScene* NovodexScene = GetNovodexPrimarySceneFromIndex(SceneIndex);
			if(NovodexScene)
			{
				// Destroy joint.
				NxJoint* Joint = HandleData;
				NovodexScene->releaseJoint(*Joint);

				// Destroy temporary actor.
				NxActor* KinActor = (NxActor*)KinActorData;
				NovodexScene->releaseActor(*KinActor);
			}
			KinActorData = NULL;
			HandleData = NULL;
		}

		bRotationConstrained = FALSE;

		GrabbedComponent = NULL;
		GrabbedBoneName = NAME_None;
	}
#endif // WITH_NOVODEX
}

/////////////// SETLOCATION

void URB_Handle::SetLocation(FVector NewLocation)
{
	if(!KinActorData)
		return;

#if WITH_NOVODEX
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call SetLocation() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif
	Location = NewLocation;
	NxActor* KinActor = (NxActor*)KinActorData;
	NxVec3 nNewLocation = U2NPosition(NewLocation);
	NxVec3 nCurrentLocation = KinActor->getGlobalPosition();

	// Don't call moveGlobalPosition if it hasn't changed - that will stop bodies from going to sleep.
	if((nNewLocation - nCurrentLocation).magnitudeSquared() > 0.01f*0.01f)
	{
		KinActor->moveGlobalPosition(nNewLocation);
	}
#endif // WITH_NOVODEX
}

/////////////// SETSMOOTHLOCATION

void URB_Handle::SetSmoothLocation(FVector NewLocation, FLOAT MoveTime)
{
	Destination = NewLocation;
	bInterpolating = TRUE;
	StepSize = (Destination - Location)/MoveTime;
}

void URB_Handle::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

	if ( bInterpolating )
	{
		FVector UpdatedLocation = Location + StepSize*DeltaTime;
		if ( ((Destination - UpdatedLocation) | (Destination - Location)) <= 0.f )
		{
			UpdatedLocation = Destination;
			bInterpolating = FALSE;
		}
		SetLocation(UpdatedLocation);
	}
}

void URB_Handle::UpdateSmoothLocation(FVector const& NewLocation)
{
	// ignore if not interpolating
	if (bInterpolating)
	{
		FLOAT RemainingMoveTime = (Destination - Location).Size() / StepSize.Size();
		SetSmoothLocation(NewLocation, RemainingMoveTime);
	}
}

/////////////// SETORIENTATION

void URB_Handle::SetOrientation(const FQuat & NewOrientation)
{
	if(!KinActorData)
		return;

#if WITH_NOVODEX
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call SetOrientation() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif
	NxActor* KinActor = (NxActor*)KinActorData;
	NxQuat nNewOrientation = U2NQuaternion(NewOrientation);
	NxQuat nCurrentOrientation = KinActor->getGlobalOrientationQuat();

	if(Abs(nNewOrientation.dot(nCurrentOrientation)) < (1.f - KINDA_SMALL_NUMBER))
	{
		KinActor->moveGlobalOrientationQuat(nNewOrientation);
	}
#endif // WITH_NOVODEX

}

/////////////// GETORIENTATION

FQuat URB_Handle::GetOrientation()
{
	FQuat Result(0.f,0.f,0.f,0.f);
	if(!KinActorData)
		return Result;

#if WITH_NOVODEX

	NxActor* KinActor = (NxActor*)KinActorData;
	if( KinActor )
	{
		Result = N2UQuaternion( KinActor->getGlobalOrientationQuat() );
	}
#endif // WITH_NOVODEX
	return Result;
}

////////////////////////////////////////////////////////////////////
// URB_Spring //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

/** 
 *	Create a spring between the 2 supplied Components (and possibly bones within those Components in the case of a SkeletalMesh).
 *
 *	@param	InComponent1	Component to attach to one end of the spring.
 *	@param	InBoneName1		If InComponent1 is a SkeletalMeshComponent, this specified the bone within the mesh to attach the spring to.
 *	@param	Position1		Position (in world space) to attach spring to body of InComponent1.
 *	@param	InComponent2	Component to attach other end of spring to.
 *	@param	InBoneName2		If InComponent2 is a SkeletalMeshComponent, this specified the bone within the mesh to attach the spring to.
 *	@param	Position2		Position (in world space) to attach spring to body of InComponent2.
 */
void URB_Spring::SetComponents(UPrimitiveComponent* InComponent1, FName InBoneName1, FVector Position1, UPrimitiveComponent* InComponent2, FName InBoneName2, FVector Position2)
{
#if WITH_NOVODEX
	if(!Owner)
	{
		return;
	}

#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call URB_Spring::SetComponents() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif

	Clear();

	Component1 = InComponent1;
	BoneName1 = InBoneName1;
	Component2 = InComponent2;
	BoneName2 = InBoneName2;

	MinBodyMass = BIG_NUMBER;
	NxActor* nActor1 = NULL;
	if(InComponent1)
	{
		nActor1 = InComponent1->GetNxActor(InBoneName1);
		if(nActor1 && nActor1->isDynamic())
		{
			MinBodyMass = ::Min<FLOAT>(MinBodyMass, nActor1->getMass());
		}
	}

	NxActor* nActor2 = NULL;
	if(InComponent2)
	{
		nActor2 = InComponent2->GetNxActor(InBoneName2);
		if(nActor2 && nActor2->isDynamic())
		{
			MinBodyMass = ::Min<FLOAT>(MinBodyMass, nActor2->getMass());
		}
	}

	// Can't make spring between 2 NULL actors.
	if(!nActor1 && !nActor2)
	{
		return;
	}

	// At least one actor must be dynamic.
	UBOOL bHaveDynamic = ((nActor1 && nActor1->isDynamic()) || (nActor2 && nActor2->isDynamic()));
	if(!bHaveDynamic)
	{
		return;
	}

	// Get the NxScene from one of the NxActors.
	NxScene* Scene = NULL;
	if(nActor1)
	{
		Scene = &(nActor1->getScene());
	}
	else
	{
		Scene = &(nActor2->getScene());
	}
	check(Scene);

	// Should have got a sensible mass from at least one 
	check(MinBodyMass < BIG_NUMBER);

	//debugf( TEXT("MinBodyMass: %f"), MinBodyMass );

	// Create Novodex spring and assign Novodex NxActors.
	NxSpringAndDamperEffectorDesc SpringDesc;
	NxSpringAndDamperEffector* Spring = Scene->createSpringAndDamperEffector(SpringDesc);

	NxVec3 nPos1 = U2NPosition(Position1);
	NxVec3 nPos2 = U2NPosition(Position2);

	Spring->setBodies(nActor1, nPos1, nActor2, nPos2);

	// Save pointer to Novodex spring structure.
	SpringData = (void*)Spring;

	// Remember scene index.
	FRBPhysScene* RBScene = (FRBPhysScene*)Scene->userData;
	SceneIndex = RBScene->NovodexSceneIndex;

	// Reset spring timer.
	TimeSinceActivation = 0.f;


	FLOAT UseSpringForce = SpringMaxForce;
	// If enabled, calculate 
	if(bEnableForceMassRatio)
	{
		UseSpringForce = ::Min<FLOAT>(UseSpringForce, MaxForceMassRatio * MinBodyMass);
	}
	// Scale time time-based factor
	UseSpringForce *= SpringMaxForceTimeScale.Eval( TimeSinceActivation, 1.f );

	Spring->setLinearSpring(0.f, 0.f, SpringSaturateDist, 0.f, UseSpringForce);
	Spring->setLinearDamper(-DampSaturateVel, DampSaturateVel, DampMaxForce, DampMaxForce);

	if(nActor1)
	{
		nActor1->wakeUp();
	}

	if(nActor2)
	{
		nActor2->wakeUp();
	}
#endif
}

/** 
 *	Release any spring that exists. 
 */
void URB_Spring::Clear()
{
#if WITH_NOVODEX
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call URB_Spring::Clear() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif
	// Turning off the spring may have some affect, so wake the bodies now.
	if(Component1)
	{
		Component1->WakeRigidBody(BoneName1);
	}

	if(Component2)
	{
		Component2->WakeRigidBody(BoneName2);
	}

	// Clear previously sprung components.
	Component1 = NULL;
	BoneName1 = NAME_None;
	Component2 = NULL;
	BoneName2 = NAME_None;

	if(!Owner)
	{
		return;
	}

	if(SpringData)
	{
		// use correct scene
		NxScene* Scene = GetNovodexPrimarySceneFromIndex(SceneIndex);
		if(Scene)
		{
			NxSpringAndDamperEffector* Spring = (NxSpringAndDamperEffector*)SpringData;
			Scene->releaseEffector(*Spring);
		}
		SpringData = NULL;
	}
#endif
}

/** 
 *	Each tick, update TimeSinceActivation and if there is a spring running update the parameters from the RB_Spring.
 *	This allows SpringMaxForceTimeScale to modify the strength of the spring over its lifetime.
 */
void URB_Spring::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceActivation += DeltaTime;

#if WITH_NOVODEX
	if(SpringData)
	{
#if !FINAL_RELEASE
		// Check to see if this physics call is illegal during this tick group
		if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
		{
			debugf(NAME_Error,TEXT("Can't call physics functions on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
		}
#endif
		NxSpringAndDamperEffector* Spring = (NxSpringAndDamperEffector*)SpringData;

		FLOAT UseSpringForce = SpringMaxForce;
		// If enabled, calculate 
		if(bEnableForceMassRatio)
		{
			UseSpringForce = ::Min<FLOAT>(UseSpringForce, MaxForceMassRatio * MinBodyMass);
		}
		// Scale time time-based factor
		UseSpringForce *= SpringMaxForceTimeScale.Eval( TimeSinceActivation, 1.f );

		Spring->setLinearSpring(0.f, 0.f, SpringSaturateDist, 0.f, UseSpringForce);
		Spring->setLinearDamper(-DampSaturateVel, DampSaturateVel, DampMaxForce, DampMaxForce);
	}
#endif

	// Ensure both bodies stay awake during spring activity.
	if(Component1)
	{
		Component1->WakeRigidBody(BoneName1);
	}

	if(Component2)
	{
		Component2->WakeRigidBody(BoneName2);
	}
}

/** 
 *	Terminate physics for this component by releasing anything we are holding. 
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the physics is in that scene. 
 */
void URB_Spring::TermComponentRBPhys(FRBPhysScene* InScene)
{
#if WITH_NOVODEX
	if(SpringData)
	{
		// If this spring is in the scene we want, clear it.
		if(InScene == NULL || SceneIndex == InScene->NovodexSceneIndex)
		{
			Clear();
		}
	}
#endif // WITH_NOVODEX
}

void UInstancedStaticMeshComponent::InitComponentRBPhys(UBOOL bFixed)
{
	// TODO: Decide if we need physics support for ISMCs, totally disabled for now for performance
	return;
}

IMPLEMENT_CLASS(AKActorFromStatic);

/**
  * Move the StaticMeshComponent back to its original actor owner, and fix its position again.
  */
void AKActorFromStatic::MakeStatic()
{
	// Lighting
	if (StaticMeshComponent->LightEnvironment)
	{
		UDynamicLightEnvironmentComponent* DynamicLightEnvironment = CastChecked<UDynamicLightEnvironmentComponent>(StaticMeshComponent->LightEnvironment);
		// Make the DLE static so it won't do any more lighting updates
		DynamicLightEnvironment->bDynamic = FALSE;
		// Attach it to the original static mesh since this KActor will be GC'ed
		MyStaticMeshActor->AttachComponent(DynamicLightEnvironment);
		LightEnvironment = NULL;
	}
	StaticMeshComponent->LightingChannels.Dynamic = FALSE;

	// Physics and collision
	StaticMeshComponent->TermComponentRBPhys(NULL);
	MyStaticMeshActor->AttachComponent(StaticMeshComponent);
	StaticMeshComponent->InitComponentRBPhys(TRUE);

	// Position
	FMatrix ActorLocalToWorld = LocalToWorld();
	StaticMeshComponent->ConditionalUpdateTransform(ActorLocalToWorld);
}

/** Disable precomputed lighting when become dynamic */
void AKActorFromStatic::DisablePrecomputedLighting()
{
	if ( StaticMeshComponent )
	{
		DetachComponent(StaticMeshComponent);
		StaticMeshComponent->bUsePrecomputedShadows = FALSE;
		AttachComponent(StaticMeshComponent);
	}
}

/**
  * Move StaticMeshComponent MovableMesh to a KActorFromStatic
  */
AKActorFromStatic* AKActorFromStatic::MakeDynamic(UStaticMeshComponent* MovableMesh)
{
	if ( GIsEditor && !GIsGame )
	{
		return NULL;
	}

	// make sure we have a valid mesh that is currently attached to a staticmeshactor
	if ( !MovableMesh || !MovableMesh->GetOwner() || !MovableMesh->StaticMesh  || !MovableMesh->GetOwner()->IsA(AStaticMeshActorBase::StaticClass()) )
	{
		return NULL;
	}

	// warn if no rigid bodyinstance associated with mesh and return
	if( !MovableMesh->BodyInstance || !MovableMesh->BodyInstance->IsValidBodyInstance() )
	{
		debugf( NAME_Error, TEXT("AKActorFromStatic::MakeDynamic: Invalid or Missing BodyInstance in CollisionComponent for: %s  Mesh: %s"), *MovableMesh->GetName(), *MovableMesh->StaticMesh->GetName() );
		return NULL;
	}

	AKActorFromStatic* MyKActor = Cast<AKActorFromStatic>(GWorld->SpawnActor(AKActorFromStatic::StaticClass(), NAME_None, 
		MovableMesh->LocalToWorld.GetOrigin(), MovableMesh->LocalToWorld.Rotator(), 
		NULL, TRUE, FALSE, MovableMesh->GetOwner()));

	if ( !MyKActor )
	{
		return NULL;
	}

	// Set position
	MyKActor->MyStaticMeshActor = MovableMesh->GetOwner();
	FMatrix ActorLocalToWorld = MyKActor->LocalToWorld();

	// StaticMeshActors may have scale applied (as opposed to static mesh collections)
	AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(MovableMesh->GetOwner());
	if ( MeshActor )
	{
		// make sure drawscale is all in component (already done for static mesh collections)
		FVector ActorScale3D = MyKActor->MyStaticMeshActor->DrawScale * MyKActor->MyStaticMeshActor->DrawScale3D;
		MovableMesh->Scale3D *= ActorScale3D;
		MeshActor->DrawScale = 1.f;
		MeshActor->DrawScale3D = FVector(1.f, 1.f, 1.f);
		FVector RecipScale( 1.f/ActorScale3D.X, 1.f/ActorScale3D.Y, 1.f/ActorScale3D.Z );

		ActorLocalToWorld.M[0][0] *= RecipScale.X;
		ActorLocalToWorld.M[0][1] *= RecipScale.X;
		ActorLocalToWorld.M[0][2] *= RecipScale.X;

		ActorLocalToWorld.M[1][0] *= RecipScale.Y;
		ActorLocalToWorld.M[1][1] *= RecipScale.Y;
		ActorLocalToWorld.M[1][2] *= RecipScale.Y;

		ActorLocalToWorld.M[2][0] *= RecipScale.Z;
		ActorLocalToWorld.M[2][1] *= RecipScale.Z;
		ActorLocalToWorld.M[2][2] *= RecipScale.Z;
	}
	AStaticMeshCollectionActor* CollectionActor = Cast<AStaticMeshCollectionActor>(MovableMesh->GetOwner());
	if (CollectionActor != NULL)
	{
		// Clear the translation & rotation on the component
		MovableMesh->Translation = FVector(0.0f);
		MovableMesh->Rotation = FRotator(0,0,0);
	}

	MovableMesh->ConditionalUpdateTransform(ActorLocalToWorld);

	// Set Lighting
	AActor* MovableMeshOwner = MovableMesh->GetOwner();
	if (MovableMeshOwner)
	{
		// Detach before we start changing lighting settings, since we will need to reattach to propagate them anyway
		MovableMeshOwner->DetachComponent(MovableMesh);
	}

	// Disable precomputed lighting so that the component will throw away its lightmaps on the next attach
	UBOOL bAlreadyInitialized = FALSE;
	if (MovableMesh->LightEnvironment)
	{
		// The movable mesh component already had a light environment, reuse it
		UDynamicLightEnvironmentComponent* DynamicLightEnvironment = CastChecked<UDynamicLightEnvironmentComponent>(MovableMesh->LightEnvironment);
		// Make sure the light environment is dynamic so it captures new lighting state
		DynamicLightEnvironment->bDynamic = TRUE;
		MyKActor->AttachComponent(DynamicLightEnvironment);
		bAlreadyInitialized = TRUE;
	}
	else
	{
		// Create a new light environment to light the movable mesh component
		// Use the mesh component's owner as the owner for this new DLE, since this AKActorFromStatic will be GC'ed once it stops moving
		UDynamicLightEnvironmentComponent* DynamicLightEnvironment = ConstructObject<UDynamicLightEnvironmentComponent>( UDynamicLightEnvironmentComponent::StaticClass(), MovableMeshOwner );
		// Won't be able to cast shadows once the component becomes static so no point in casting shadows while moving
		DynamicLightEnvironment->bCastShadows = FALSE;
		// Force line checks to dominant lights, which is necessary for correct shadowing since dynamic shadows are disabled
		DynamicLightEnvironment->bForceCompositeAllLights = TRUE;
		// Use active components for bounds calculations
		// Need to use this method instead of the default since the default takes the owner's position into account
		DynamicLightEnvironment->BoundsMethod = DLEB_ActiveComponents;
		// The light environment is going to be moving so update lighting more often
		DynamicLightEnvironment->MinTimeBetweenFullUpdates = .5f;
		MovableMesh->SetLightEnvironment(DynamicLightEnvironment);
		MyKActor->AttachComponent(DynamicLightEnvironment);
		MyKActor->LightEnvironment = DynamicLightEnvironment;
	}

	MovableMesh->LightingChannels.Dynamic = TRUE;

	// Set Physics
	MovableMesh->TermComponentRBPhys(NULL);
	MyKActor->AttachComponent(MovableMesh);
	MyKActor->StaticMeshComponent = MovableMesh;
	MyKActor->CollisionComponent = MovableMesh;
	MovableMesh->InitComponentRBPhys(FALSE);
	MovableMesh->SetRBChannel(RBCC_GameplayPhysics);
	MovableMesh->SetRBCollidesWithChannel(RBCC_EffectPhysics, TRUE);
	MovableMesh->SetRBCollidesWithChannel(RBCC_Default, TRUE);
	MovableMesh->SetRBCollidesWithChannel(RBCC_GameplayPhysics, TRUE);
	MovableMesh->SetRBCollidesWithChannel(RBCC_BlockingVolume, TRUE);

	/** FIXMESTEVE - do we really want this behavior?
	if ( !bAlreadyInitialized )
	{
		// look for blocking volumes overlapping this mesh, and disable collision with it
		FMemMark Mark(GMainThreadMemStack);
		FLOAT BoundingRadius = 0.f;
		FLOAT BoundingHeight = 0.f;
		MyKActor->GetBoundingCylinder(BoundingRadius, BoundingHeight);
		FCheckResult* Link = GWorld->Hash->ActorOverlapCheck(GMainThreadMemStack, Owner, MyKActor->Location, ::Min(BoundingRadius, BoundingHeight));
		for ( Link; Link!=NULL; Link=Link->GetNext() )
		{
			if( Link->Actor
				&& !Link->Actor->bDeleteMe
				&& Link->Actor->CollisionComponent )
			{
				ABlockingVolume* BV = Cast<ABlockingVolume>(Link->Actor);
				if ( BV )
				{
					MovableMesh->DisableRBCollisionWithSMC( BV->CollisionComponent, TRUE );
				}
			}
		}
		Mark.Pop();
	}
	*/
	MovableMesh->WakeRigidBody();

	MovableMesh->SetBlockRigidBody(TRUE);

	// OK, so not really spawned by kismet, but need to do some initialization that was skipped in PostBeginPlay()
	MyKActor->eventSpawnedByKismet();

	return MyKActor;
}

