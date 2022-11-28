/*=============================================================================
	UnPhysAsset.cpp: Physics Asset - code for managing articulated assemblies of rigid bodies.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

IMPLEMENT_CLASS(UPhysicsAsset);
IMPLEMENT_CLASS(URB_BodySetup);

IMPLEMENT_CLASS(UPhysicsAssetInstance);
IMPLEMENT_CLASS(URB_BodyInstance);

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX


///////////////////////////////////////	
//////////// UPhysicsAsset ////////////
///////////////////////////////////////

UBOOL UPhysicsAsset::LineCheck(FCheckResult& Result, class USkeletalMeshComponent* SkelComp, const FVector& Start, const FVector& End, const FVector& Extent)
{
	if( !Extent.IsZero() )
		return 1;

	FVector Scale3D = SkelComp->Owner->DrawScale * SkelComp->Owner->DrawScale3D;
	if( !Scale3D.IsUniform() )
	{
		debugf( TEXT("UPhysicsAsset::LineCheck : Non-uniform scale factor.") );
		return 1;
	}

	UBOOL bIsZeroExtent = Extent.IsZero();

	Result.Item = INDEX_NONE;
	Result.Time = 1.0f;
	Result.BoneName = NAME_None;
	Result.Component = NULL;

	FCheckResult TempResult;

	for(INT i=0; i<BodySetup.Num(); i++)
	{
		URB_BodySetup* bs = BodySetup(i);

		if( (bIsZeroExtent && !bs->bBlockZeroExtent) || (!bIsZeroExtent && !bs->bBlockNonZeroExtent) )
			continue;

		INT BoneIndex = SkelComp->MatchRefBone(bs->BoneName);
		check(BoneIndex != INDEX_NONE);

		FMatrix WorldBoneTM = SkelComp->GetBoneMatrix(BoneIndex);
		WorldBoneTM.RemoveScaling();

		TempResult.Time = 1.0f;

		bs->AggGeom.LineCheck(TempResult, WorldBoneTM, Scale3D, End, Start, Extent);

		if(TempResult.Time < Result.Time)
		{
			Result = TempResult;
			Result.Item = i;
			Result.BoneName = bs->BoneName;
			Result.Component = SkelComp;
			Result.Actor = SkelComp->Owner;
		}
	}

	if(Result.Time < 1.0f)
		return 0;
	else
		return 1;
}


FBox UPhysicsAsset::CalcAABB(USkeletalMeshComponent* SkelComp)
{
	FBox Box(0);

	FVector Scale3D = SkelComp->Owner->DrawScale * SkelComp->Owner->DrawScale3D;
	if( Scale3D.IsUniform() )
	{
		for(INT i=0; i<BodySetup.Num(); i++)
		{
			URB_BodySetup* bs = BodySetup(i);

			INT BoneIndex = SkelComp->MatchRefBone(bs->BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				FMatrix WorldBoneTM = SkelComp->GetBoneMatrix(BoneIndex);
				WorldBoneTM.RemoveScaling();

				Box += bs->AggGeom.CalcAABB( WorldBoneTM, Scale3D.X );
			}
		}
	}
	else
	{
		debugf( TEXT("UPhysicsAsset::CalcAABB : Non-uniform scale factor.") );
	}

	if(!Box.IsValid)
	{
		Box = FBox( SkelComp->LocalToWorld.GetOrigin(), SkelComp->LocalToWorld.GetOrigin() );
	}

	return Box;
}

// Find the index of the physics bone that is controlling this graphics bone.
INT	UPhysicsAsset::FindControllingBodyIndex(class USkeletalMesh* skelMesh, INT StartBoneIndex)
{
	INT BoneIndex = StartBoneIndex;
	while(1)
	{
		FName BoneName = skelMesh->RefSkeleton(BoneIndex).Name;
		INT BodyIndex = FindBodyIndex(BoneName);

		if(BodyIndex != INDEX_NONE)
			return BodyIndex;

		INT ParentBoneIndex = skelMesh->RefSkeleton(BoneIndex).ParentIndex;

		if(ParentBoneIndex == BoneIndex)
			return INDEX_NONE;

		BoneIndex = ParentBoneIndex;
	}

	return INDEX_NONE; // Shouldn't reach here.
}


// TODO: These do a nasty linear search. Replace with TMap!!!
INT UPhysicsAsset::FindBodyIndex(FName bodyName)
{
	check( BodySetup.Num() == DefaultInstance->Bodies.Num() );

	for(INT i=0; i<BodySetup.Num(); i++)
	{
		if( BodySetup(i)->BoneName == bodyName )
			return i;
	}

	return INDEX_NONE;
}

INT UPhysicsAsset::FindConstraintIndex(FName constraintName)
{
	check( ConstraintSetup.Num() == DefaultInstance->Constraints.Num() );

	for(INT i=0; i<ConstraintSetup.Num(); i++)
	{
		if( ConstraintSetup(i)->JointName == constraintName )
			return i;
	}

	return INDEX_NONE;
}

void UPhysicsAsset::UpdateBodyIndices()
{
	for(INT i=0; i<DefaultInstance->Bodies.Num(); i++)
	{
		DefaultInstance->Bodies(i)->BodyIndex = i;
	}
}


// Find all the constraints that are connected to a particular body.
void UPhysicsAsset::BodyFindConstraints(INT bodyIndex, TArray<INT>& constraints)
{
	constraints.Empty();
	FName bodyName = BodySetup(bodyIndex)->BoneName;

	for(INT i=0; i<ConstraintSetup.Num(); i++)
	{
		if( ConstraintSetup(i)->ConstraintBone1 == bodyName || ConstraintSetup(i)->ConstraintBone2 == bodyName )
			constraints.AddItem(i);
	}
}

void UPhysicsAsset::ClearShapeCaches()
{
	for(INT i=0; i<BodySetup.Num(); i++)
	{
		BodySetup(i)->ClearShapeCache();
	}
}



///////////////////////////////////////
/////////// URB_BodySetup /////////////
///////////////////////////////////////

void URB_BodySetup::CopyBodyPropertiesFrom(class URB_BodySetup* fromSetup)
{
	CopyMeshPropsFrom(fromSetup);

	PhysicalMaterial = fromSetup->PhysicalMaterial;
	COMNudge = fromSetup->COMNudge;
	bFixed = fromSetup->bFixed;
	bNoCollision = fromSetup->bNoCollision;
}

// Cleanup all collision geometries currently in this StaticMesh. Must be called before the StaticMesh is destroyed.
void URB_BodySetup::ClearShapeCache()
{
#ifdef WITH_NOVODEX
	for (INT i = 0; i < CollisionGeom.Num(); i++)
	{
		NxActorDesc* ActorDesc = (NxActorDesc*) CollisionGeom(i);

		if (ActorDesc)
		{
			delete ActorDesc;
			CollisionGeom(i) = NULL;
		}
	}

	CollisionGeom.Empty();
	CollisionGeomScale3D.Empty();
#endif // WITH_NOVODEX
}

void URB_BodySetup::Destroy()
{
	Super::Destroy();
	ClearShapeCache();
}

///////////////////////////////////////
/////////// URB_BodyInstance //////////
///////////////////////////////////////

// Transform is in Unreal scale.
void URB_BodyInstance::InitBody(URB_BodySetup* setup, const FMatrix& transform, const FVector& Scale3D, UBOOL bFixed, UPrimitiveComponent* PrimComp)
{
	Owner = PrimComp->Owner;
	check(Owner);

#ifdef WITH_NOVODEX
	if (BodyData)
		return;

	if (!Owner || !Owner->GetLevel() || !Owner->GetLevel()->NovodexScene)
		return;

	// Try to find a shape with the matching scale
	NxArray<NxShapeDesc*>* Aggregate = NULL;
	for (int i = 0; i < setup->CollisionGeomScale3D.Num(); i++)
	{
		// Found a shape with the right scale
		if ((setup->CollisionGeomScale3D(i) - Scale3D).IsNearlyZero())
		{
			Aggregate = &((NxActorDesc*) setup->CollisionGeom(i))->shapes;
		}
	}

	// If no shape was found, create a new one and place it in the list
	if (!Aggregate)
	{
		// Instance a Novodex collision shape based on the stored unreal collision data.
		NxActorDesc* GeomActorDesc = NULL;
		const TCHAR* DebugName = (setup->BoneName != NAME_None) ? *setup->BoneName : Owner->GetName();

		GeomActorDesc = setup->AggGeom.InstanceNovodexGeom(Scale3D, DebugName);
		if (GeomActorDesc)
		{
			Aggregate = &GeomActorDesc->shapes;
		}

		if (Aggregate)
		{
			setup->CollisionGeomScale3D.AddItem(Scale3D);
			setup->CollisionGeom.AddItem(GeomActorDesc);
		}
		else
		{
			debugf(TEXT("URB_BodyInstance::InitBody : Could not create new Shape: %s"), *setup->BoneName);
			return;
		}
	}

	// Find the PhysicalMaterial to use for this instance.
	UClass* PhysMatClass = NULL;
	if( PrimComp->PhysicalMaterialOverride )
	{
		PhysMatClass = PrimComp->PhysicalMaterialOverride;
	}
	else if( setup->PhysicalMaterial )
	{
		PhysMatClass = setup->PhysicalMaterial;
	}
	else // If no physical material is specified, just use the base class.
	{
		PhysMatClass = UPhysicalMaterial::StaticClass();
	}
	check( PhysMatClass );
	check( PhysMatClass->IsChildOf(UPhysicalMaterial::StaticClass()) );
	NxMaterialIndex MatIndex = GEngine->FindMaterialIndex( PhysMatClass );

	// Create ActorDesc description, and copy list of primitives from stored geometry.
	NxActorDesc ActorDesc;

	for(UINT i=0; i<Aggregate->size(); i++)
	{
		ActorDesc.shapes.push_back( (*Aggregate)[i] );

		// Set the material to the one specified in the PhysicalMaterial before creating this NxActor instance.
		ActorDesc.shapes[i]->materialIndex = MatIndex;
	}

	NxMat34 nTM = U2NTransform(transform);

	UPhysicalMaterial* PhysMat = (UPhysicalMaterial*)PhysMatClass->GetDefaultObject();

	ActorDesc.density = PhysMat->Density;
	ActorDesc.globalPose = nTM;
	ActorDesc.userData = PrimComp;

	if(setup->bNoCollision || !PrimComp->BlockRigidBody)
	{
		ActorDesc.flags = NX_AF_DISABLE_COLLISION;
	}

	UBOOL bBodyIsDynamic = false;

	// Now fill in dynamics paramters.
	// If owner is static, don't create any dynamics properties for this Actor.
	NxBodyDesc BodyDesc;

	if(!Owner->bStatic)
	{
		// Set the damping properties from the PhysicalMaterial
		BodyDesc.angularDamping = PhysMat->AngularDamping;
		BodyDesc.linearDamping = PhysMat->LinearDamping;

		// Inherit linear velocity from Unreal Actor.
		FVector uLinVel = Owner->Velocity;

		// Set kinematic flag if body is not currently dynamics.
		if(setup->bFixed || bFixed)
		{
			BodyDesc.flags |= NX_BF_KINEMATIC;
			//uLinVel += transform.TransformNormal(setup->ApparentVelocity);
		}
		else
		{
			bBodyIsDynamic = true;
		}

		// Set linear velocity of body.
		BodyDesc.linearVelocity = U2NPosition(uLinVel);
		
		// Assign Body Description to Actor Description
		ActorDesc.body = &BodyDesc;
	}

	// Give the actor a chance to modify the NxActorDesc before we create the NxActor with it
	if(PrimComp->Owner)
	{
		PrimComp->Owner->ModifyNxActorDesc(ActorDesc);
	}

	if (!ActorDesc.isValid())
	{
		debugf(TEXT("URB_BodyInstance::InitBody - Error, rigid body description invalid, %s"), *setup->BoneName);
		return;
	}

	NxActor* Actor = Owner->GetLevel()->NovodexScene->createActor(ActorDesc);
	check(Actor);

	if(!Owner->bStatic)
	{
		check( Actor->isDynamic() );

		// Adjust mass scaling to avoid big differences between big and small objects.
		FLOAT OldMass = Actor->getMass();
		FLOAT NewMass = appPow(OldMass, 0.75f);
		//debugf( TEXT("OldMass: %f NewMass: %f"), OldMass, NewMass );

		// Apply user-defined mass scaling.
		NewMass *= Clamp<FLOAT>(setup->MassScale, 0.01f, 100.0f);

		FLOAT MassRatio = NewMass/OldMass;
		NxVec3 InertiaTensor = Actor->getMassSpaceInertiaTensor();
		Actor->setMassSpaceInertiaTensor(InertiaTensor * MassRatio);
		Actor->setMass(NewMass);

		// Apply the COMNudge
		NxVec3 nCOMNudge = U2NPosition(setup->COMNudge);

		NxVec3 nCOMPos = Actor->getCMassLocalPosition();
		Actor->setCMassOffsetLocalPosition(nCOMPos + nCOMNudge);

		if(setup->bFixed || bFixed)
		{
			Actor->clearBodyFlag( NX_BF_KINEMATIC );
			Actor->raiseBodyFlag( NX_BF_KINEMATIC );
		}
	}

	// Bodies should start out sleeping.
	Actor->putToSleep();

	// Store pointer to Novodex data in RB_BodyInstance
	BodyData = (Fpointer)Actor;

	// Store pointer to owning Componet in NxActor's UserData;
	Actor->userData = PrimComp;

	// If dynamics, add the primitive component to the level's set of physical primitive components.
	// JTODO: Add all dynamics PrimComponents, not just the ones that respond to wind!
	if(bBodyIsDynamic && PhysMat->WindResponse > 0.f)
	{
		ULevel* Level = Owner->GetLevel();

		// Each component should only be added once.
		Level->PhysPrimComponents.AddUniqueItem(PrimComp);
	}

	// After starting up the physics, let the Actor do anything else it might want.
	if(PrimComp->Owner)
	{
		PrimComp->Owner->PostInitRigidBody(Actor);
	}
#endif // WITH_NOVODEX
}

void URB_BodyInstance::TermBody()
{
#ifdef WITH_NOVODEX
	NxActor* Actor = (NxActor*) BodyData;
	if (!Actor || !Owner || !Owner->GetLevel() || !Owner->GetLevel()->NovodexScene)
		return;

	Owner->GetLevel()->NovodexScene->releaseActor(*Actor);
	BodyData = NULL;
#endif // WITH_NOVODEX
}

void URB_BodyInstance::SetFixed(UBOOL bNewFixed)
{
#ifdef WITH_NOVODEX
	NxActor* Actor = (NxActor*)BodyData;
	if(Actor && Actor->isDynamic())
	{
		// If we want it fixed, and it is currently not kinematic
		if(bNewFixed && !Actor->readBodyFlag(NX_BF_KINEMATIC))
		{
			Actor->raiseBodyFlag(NX_BF_KINEMATIC);
		}
		// If want to stop it being fixed, and it is currently kinematic
		else if(!bNewFixed && Actor->readBodyFlag(NX_BF_KINEMATIC))
		{
			Actor->clearBodyFlag(NX_BF_KINEMATIC);

			// Should not need to do this, but moveGlobalPose does not currently update velocity correctly so we are not using it.
			if(Owner)
			{
				Actor->setLinearVelocity( U2NPosition(Owner->Velocity) );
			}
		}
	}
#endif // WITH_NOVODEX
}


FMatrix URB_BodyInstance::GetUnrealWorldTM()
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	check(Actor);
	check(Actor->getNbShapes() > 0);

	NxMat34 nTM = Actor->getGlobalPose();
	FMatrix uTM = N2UTransform(nTM);

	return uTM;
#endif // WITH_NOVODEX

	return FMatrix::Identity;
}


FVector URB_BodyInstance::GetUnrealWorldVelocity()
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	check(Actor);

	NxVec3 nVelocity = Actor->getLinearVelocity();
	FVector uVelocity = N2UPosition(nVelocity);

	return uVelocity;
#endif // WITH_NOVODEX

	return FVector(0.f);
}

void URB_BodyInstance::DrawCOMPosition( FPrimitiveRenderInterface* PRI, FLOAT COMRenderSize, const FColor& COMRenderColor )
{
#ifdef WITH_NOVODEX
	NxActor* Actor = GetNxActor();
	if(Actor)
	{
		NxVec3 nCOMPos = Actor->getCMassGlobalPosition();
		FVector COMPos = N2UPosition(nCOMPos);

		PRI->DrawWireStar( COMPos, COMRenderSize, COMRenderColor);
	}
#endif
}

#ifdef WITH_NOVODEX
class NxActor* URB_BodyInstance::GetNxActor()
{
	if (!BodyData)
		return NULL;

	NxActor* Actor = (NxActor*) BodyData;
	return Actor;
}
#endif // WITH_NOVODEX

///////////////////////////////////////
//////// UPhysicsAssetInstance ////////
///////////////////////////////////////

void UPhysicsAssetInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CollisionDisableTable;
}

void UPhysicsAssetInstance::EnableCollision(class URB_BodyInstance* BodyA, class URB_BodyInstance* BodyB)
{
	if(BodyA == BodyB)
		return;

	QWORD Key = RigidBodyIndicesToKey(BodyA->BodyIndex, BodyB->BodyIndex);

	// If its not in table - do nothing
	if( !CollisionDisableTable.Find(Key) )
		return;

	CollisionDisableTable.Remove(Key);

#ifdef WITH_NOVODEX
	NxActor* ActorA = BodyA->GetNxActor();
	NxActor* ActorB = BodyB->GetNxActor();

	if (ActorA && ActorB)
	{
		NxU32 CurrentFlags = Owner->GetLevel()->NovodexScene->getActorPairFlags(*ActorA, *ActorB);
		Owner->GetLevel()->NovodexScene->setActorPairFlags(*ActorA, *ActorB, CurrentFlags & ~NX_IGNORE_PAIR);
	}
#endif // WITH_NOVODEX
}

void UPhysicsAssetInstance::DisableCollision(class URB_BodyInstance* BodyA, class URB_BodyInstance* BodyB)
{
	if(BodyA == BodyB)
		return;

	QWORD Key = RigidBodyIndicesToKey(BodyA->BodyIndex, BodyB->BodyIndex);

	// If its already in the disable table - do nothing
	if( CollisionDisableTable.Find(Key) )
		return;

	CollisionDisableTable.Set(Key, 0);

#ifdef WITH_NOVODEX
	NxActor* ActorA = BodyA->GetNxActor();
	NxActor* ActorB = BodyB->GetNxActor();

	if (ActorA && ActorB)
	{
		NxU32 CurrentFlags = Owner->GetLevel()->NovodexScene->getActorPairFlags(*ActorA, *ActorB);
		Owner->GetLevel()->NovodexScene->setActorPairFlags(*ActorA, *ActorB, CurrentFlags | NX_IGNORE_PAIR);
	}
#endif // WITH_NOVODEX
}

// Called to actually start up the physics of
void UPhysicsAssetInstance::InitInstance(USkeletalMeshComponent* SkelComp, class UPhysicsAsset* PhysAsset)
{
	check(SkelComp->Owner);
	Owner = SkelComp->Owner;

	if(!SkelComp->SkeletalMesh)
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : SkeletalMeshComponent has no SkeletalMesh: %s"), Owner->GetName());
		return;
	}

	FVector Scale3D = Owner->DrawScale * Owner->DrawScale3D;
	if( !Scale3D.IsUniform() )
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Actor has non-uniform scaling: %s"), Owner->GetName());
		return;
	}
	FLOAT Scale = Scale3D.X;

	if( Bodies.Num() != PhysAsset->BodySetup.Num() )
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Asset/AssetInstance Body Count Mismatch (%d/%d) : %s"), PhysAsset->BodySetup.Num(), Bodies.Num(), Owner->GetName());
		return;
	}

	if( Constraints.Num() != PhysAsset->ConstraintSetup.Num() )
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Asset/AssetInstance Counstraint Count Mismatch (%d/%d) : %s"), PhysAsset->ConstraintSetup.Num(), Constraints.Num(), Owner->GetName());
		return;
	}

	// Find root physics body
	USkeletalMesh* SkelMesh = SkelComp->SkeletalMesh;
	RootBodyIndex = INDEX_NONE;
	for(INT i=0; i<SkelMesh->RefSkeleton.Num() && RootBodyIndex == INDEX_NONE; i++)
	{
		INT BodyInstIndex = PhysAsset->FindBodyIndex( SkelMesh->RefSkeleton(i).Name );
		if(BodyInstIndex != INDEX_NONE)
		{
			RootBodyIndex = BodyInstIndex;
		}
	}

	if(RootBodyIndex == INDEX_NONE)
	{
		debugf(TEXT("UPhysicsAssetInstance::InitInstance : Could not find root physics body: %s"), Owner->GetName());
		return;
	}

	// Create all the bodies.
	for(INT i=0; i<Bodies.Num(); i++)
	{
		check( Bodies(i) );

		// Get transform of bone by name.
		INT BoneIndex = SkelComp->MatchRefBone( PhysAsset->BodySetup(i)->BoneName );
		if(BoneIndex == INDEX_NONE)
		{
			debugf(TEXT("UPhysicsAssetInstance::InitInstance : Could not find bone %s in mesh %s"), 
				*(PhysAsset->BodySetup(i)->BoneName), SkelComp->SkeletalMesh->GetName() );
			continue;
		}

		FMatrix BoneTM = SkelComp->GetBoneMatrix( BoneIndex );
		BoneTM.RemoveScaling();

		// Create physics body instance.
		Bodies(i)->InitBody( PhysAsset->BodySetup(i), BoneTM, Scale3D, false, SkelComp);
	}

	// Create all the constraints.
	for(INT i=0; i<Constraints.Num(); i++)
	{
		check( Constraints(i) );

		Constraints(i)->InitConstraint( Owner, Owner, PhysAsset->ConstraintSetup(i), Scale, Owner );
	}

	// Fill in collision disable table information.
	for(INT i=1; i<Bodies.Num(); i++)
	{
		for(INT j=0; j<i; j++)
		{
			QWORD Key = RigidBodyIndicesToKey(j,i);
			if( CollisionDisableTable.Find(Key) )
			{
#ifdef WITH_NOVODEX
				NxActor* ActorA = Bodies(i)->GetNxActor();
				NxActor* ActorB = Bodies(j)->GetNxActor();

				if (ActorA && ActorB)
				{
					NxU32 CurrentFlags = Owner->GetLevel()->NovodexScene->getActorPairFlags(*ActorA, *ActorB);
					Owner->GetLevel()->NovodexScene->setActorPairFlags(*ActorA, *ActorB, CurrentFlags | NX_IGNORE_PAIR);
				}
#endif // WITH_NOVODEX
			}
		}
	}	

}

void UPhysicsAssetInstance::TermInstance()
{
	for(INT i=0; i<Constraints.Num(); i++)
	{
		check( Constraints(i) );
		Constraints(i)->TermConstraint();
		delete Constraints(i);
		Constraints(i) = NULL;
	}

	for(INT i=0; i<Bodies.Num(); i++)
	{
		check( Bodies(i) );
		Bodies(i)->TermBody();
		delete Bodies(i);
		Bodies(i) = NULL;
	}

}