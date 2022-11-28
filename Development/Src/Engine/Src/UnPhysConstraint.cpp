/*=============================================================================
	UnConstraint.cpp: Physics Constraints - rigid-body constraint (joint) related classes.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#ifdef WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ARB_ConstraintActor);

IMPLEMENT_CLASS(URB_ConstraintSetup);
IMPLEMENT_CLASS(URB_BSJointSetup);
IMPLEMENT_CLASS(URB_HingeSetup);
IMPLEMENT_CLASS(URB_PrismaticSetup);
IMPLEMENT_CLASS(URB_SkelJointSetup);

IMPLEMENT_CLASS(URB_ConstraintInstance);

IMPLEMENT_CLASS(URB_ConstraintDrawComponent);

#define MIN_LIN_ERP (0.2f)
#define MIN_ANG_ERP (0.2f)
#define MIN_LIN_CFM (0.0007f)
#define MIN_ANG_CFM (0.0007f)

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// UTILS ///////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

FMatrix FindBodyMatrix(AActor* Actor, FName BoneName)
{
	if(!Actor)
		return FMatrix::Identity;

	if(BoneName == NAME_None)
	{
		FMatrix BodyTM = Actor->LocalToWorld();
		BodyTM.RemoveScaling();
		return BodyTM;
	}

	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Actor->CollisionComponent);
	if(SkelComp)
	{
		INT BoneIndex = SkelComp->MatchRefBone(BoneName);
		if(BoneIndex != INDEX_NONE)
		{	
			FMatrix BodyTM = SkelComp->GetBoneMatrix(BoneIndex);
			BodyTM.RemoveScaling();
			return BodyTM;
		}
	}

	return FMatrix::Identity;
}


FBox FindBodyBox(AActor* Actor, FName BoneName)
{
	if(!Actor)
		return FBox(0);

	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Actor->CollisionComponent);
	if(SkelComp && SkelComp->PhysicsAsset)
	{
		INT BoneIndex = SkelComp->MatchRefBone(BoneName);
		INT BodyIndex = SkelComp->PhysicsAsset->FindBodyIndex(BoneName);
		if(BoneIndex != INDEX_NONE && BodyIndex != INDEX_NONE)
		{	
			FVector TotalScale = Actor->DrawScale * Actor->DrawScale3D.X;

			FMatrix BoneTM = SkelComp->GetBoneMatrix(BoneIndex);
			BoneTM.RemoveScaling();

			return SkelComp->PhysicsAsset->BodySetup(BoneIndex)->AggGeom.CalcAABB(BoneTM, TotalScale.X);
		}
	}
	else
	{
		return Actor->GetComponentsBoundingBox(true);
	}

	return FBox(0);
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// CONSTRAINT ACTOR ////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

// When we move a constraint - we need to update the position/axis held in
// local space (ie. relative to each connected actor)
void ARB_ConstraintActor::PostEditMove()
{
	Super::PostEditMove();

	check(ConstraintSetup);
	check(ConstraintInstance);

	FMatrix a1TM = FindBodyMatrix(ConstraintActor1, ConstraintSetup->ConstraintBone1);
	a1TM.ScaleTranslation( FVector(U2PScale) );

	FMatrix a2TM = FindBodyMatrix(ConstraintActor2, ConstraintSetup->ConstraintBone2);
	a2TM.ScaleTranslation( FVector(U2PScale) );

	// Calculate position/axis from constraint actor.
	FRotationMatrix ConMatrix = FRotationMatrix(Rotation);

	// World ref frame
	FVector wPos, wPri, wOrth;

	wPos = Location * U2PScale;
	wPri = ConMatrix.GetAxis(0);
	wOrth = ConMatrix.GetAxis(1);

	FVector lPos, lPri, lOrth;

	FMatrix a1TMInv = a1TM.Inverse();
	FMatrix a2TMInv = a2TM.Inverse();

	ConstraintSetup->Pos1 = a1TMInv.TransformFVector(wPos);
	ConstraintSetup->PriAxis1 = a1TMInv.TransformNormal(wPri);
	ConstraintSetup->SecAxis1 = a1TMInv.TransformNormal(wOrth);

	ConstraintSetup->Pos2 = a2TMInv.TransformFVector(wPos);
	ConstraintSetup->PriAxis2 = a2TMInv.TransformNormal(wPri);
	ConstraintSetup->SecAxis2 = a2TMInv.TransformNormal(wOrth);

}

void ARB_ConstraintActor::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	if(GIsEditor)
		this->PostEditMove();

}

void ARB_ConstraintActor::CheckForErrors()
{
	Super::CheckForErrors();



}

void ARB_ConstraintActor::InitRBPhys()
{
	// Ensure connected actors have their physics started up.
	if(ConstraintActor1)
		ConstraintActor1->InitRBPhys();

	if(ConstraintActor2)
		ConstraintActor2->InitRBPhys();

	ConstraintInstance->InitConstraint( ConstraintActor1, ConstraintActor2, ConstraintSetup, 1.0f, this );

	SetDisableCollision(bDisableCollision);
}

void ARB_ConstraintActor::TermRBPhys()
{
	ConstraintInstance->TermConstraint();

}

#ifdef WITH_NOVODEX

static NxActor* GetConstraintActor(AActor* Actor, FName BoneName)
{
	if (Actor)
		return Actor->GetNxActor(BoneName);

	return NULL;
}

#endif // WITH_NOVODEX

void ARB_ConstraintActor::SetDisableCollision(UBOOL NewDisableCollision)
{
#ifdef WITH_NOVODEX
	NxActor* FirstActor = GetConstraintActor(ConstraintActor1, ConstraintSetup->ConstraintBone1);
	NxActor* SecondActor = GetConstraintActor(ConstraintActor2, ConstraintSetup->ConstraintBone2);
	
	if (!FirstActor || !SecondActor)
		return;
	
	check(GetLevel());
	check(GetLevel()->NovodexScene);

	NxU32 CurrentFlags = GetLevel()->NovodexScene->getActorPairFlags(*FirstActor, *SecondActor);

	if (bDisableCollision)
	{
		GetLevel()->NovodexScene->setActorPairFlags(*FirstActor, *SecondActor, CurrentFlags | NX_IGNORE_PAIR);
	}
	else
	{
		GetLevel()->NovodexScene->setActorPairFlags(*FirstActor, *SecondActor, CurrentFlags & ~NX_IGNORE_PAIR);
	}
#endif

	bDisableCollision = NewDisableCollision;
}

void ARB_ConstraintActor::execSetDisableCollision( FFrame& Stack, RESULT_DECL )
{
	P_GET_UBOOL(NewDisableCollision);
	P_FINISH;

	SetDisableCollision(NewDisableCollision);

}

//////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// CONSTRAINT SETUP ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

void URB_ConstraintSetup::PostEditChange(UProperty* PropertyThatChanged)
{
	// This is pretty vile, but can't find another way to get and ConstraintActor that may contain this ConstraintSetup
	ULevel* Level = GEngine->GetLevel();

	if(Level)
	{
		for(INT i=0; i<Level->Actors.Num(); i++)
		{
			ARB_ConstraintActor* ConActor = Cast<ARB_ConstraintActor>(Level->Actors(i));
			if(ConActor && ConActor->ConstraintSetup == this)
			{
				ConActor->PostEditChange(NULL);
				return;
			}
		}
	}
}

// Gives it to you in Unreal space.
FMatrix URB_ConstraintSetup::GetRefFrameMatrix(INT BodyIndex)
{
	check(BodyIndex == 0 || BodyIndex == 1);

	FMatrix Result;

	if(BodyIndex == 0)
	{
		Result = FMatrix(PriAxis1, SecAxis1, PriAxis1 ^ SecAxis1, Pos1*P2UScale);
	}
	else
	{
		Result = FMatrix(PriAxis2, SecAxis2, PriAxis2 ^ SecAxis2, Pos2*P2UScale);
	}
	
	// Do a quick check that Primary and Secondary are othogonal...
	FVector ZAxis = Result.GetAxis(2);
	FLOAT Error = Abs( ZAxis.Size() - 1.0f );

	if(Error > 0.01f)
	{
		debugf( TEXT("URB_ConstraintInstance::GetRefFrameMatrix : Pri and Sec for body %d dont seem to be orthogonal (Error=%f)."), BodyIndex, Error);
	}

	return Result;

}

// Pass in reference frame in Unreal scale.
void URB_ConstraintSetup::SetRefFrameMatrix(INT BodyIndex, const FMatrix& RefFrame)
{
	check(BodyIndex == 0 || BodyIndex == 1);

	if(BodyIndex == 0)
	{
		Pos1 = RefFrame.GetOrigin() * U2PScale;
		PriAxis1 = RefFrame.GetAxis(0);
		SecAxis1 = RefFrame.GetAxis(1);
	}
	else
	{
		Pos2 = RefFrame.GetOrigin() * U2PScale;
		PriAxis2 = RefFrame.GetAxis(0);
		SecAxis2 = RefFrame.GetAxis(1);
	}

}

void URB_ConstraintSetup::CopyConstraintGeometryFrom(class URB_ConstraintSetup* fromSetup)
{
	ConstraintBone1 = fromSetup->ConstraintBone1;
	ConstraintBone2 = fromSetup->ConstraintBone2;

	Pos1 = fromSetup->Pos1;
	PriAxis1 = fromSetup->PriAxis1;
	SecAxis1 = fromSetup->SecAxis1;

	Pos2 = fromSetup->Pos2;
	PriAxis2 = fromSetup->PriAxis2;
	SecAxis2 = fromSetup->SecAxis2;
}

void URB_ConstraintSetup::CopyConstraintParamsFrom(class URB_ConstraintSetup* fromSetup)
{
	LinearXSetup = fromSetup->LinearXSetup;
	LinearYSetup = fromSetup->LinearYSetup;
	LinearZSetup = fromSetup->LinearZSetup;

	LinearStiffness = fromSetup->LinearStiffness;
	LinearDamping = fromSetup->LinearDamping;
	bLinearBreakable = fromSetup->bLinearBreakable;
	LinearBreakThreshold = fromSetup->LinearBreakThreshold;

	bSwingLimited = fromSetup->bSwingLimited;
	bTwistLimited = fromSetup->bTwistLimited;

	Swing1LimitAngle = fromSetup->Swing1LimitAngle;
	Swing2LimitAngle = fromSetup->Swing2LimitAngle;
	TwistLimitAngle = fromSetup->TwistLimitAngle;

	AngularStiffness = fromSetup->AngularStiffness;
	AngularDamping = fromSetup->AngularDamping;
	bAngularBreakable = fromSetup->bAngularBreakable;
	AngularBreakThreshold = fromSetup->AngularBreakThreshold;
}


static FString ConstructJointBodyName(AActor* Actor, FName ConstraintBone)
{
	if( Actor )
	{
		if(ConstraintBone == NAME_None)
			return FString( Actor->GetName() );
		else
			return FString::Printf(TEXT("%s.%s"), Actor->GetName(), *ConstraintBone);
	}
	else
	{
		if(ConstraintBone == NAME_None)
			return FString(TEXT("@World"));
		else
			return FString(*ConstraintBone);
	}
}


//////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// CONSTRAINT INSTANCE ////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

#ifdef WITH_NOVODEX
static void InitJointDesc(NxJointDesc& Desc, NxActor* FirstActor, NxActor* SecondActor, URB_ConstraintSetup* Setup, FLOAT Scale)
{
	Desc.jointFlags = NX_JF_COLLISION_ENABLED;

	NxVec3 FirstAnchor = U2NVectorCopy(Setup->Pos1 * Scale);
	NxVec3 FirstAxis = U2NVectorCopy(Setup->PriAxis1);
	NxVec3 FirstNormal = U2NVectorCopy(Setup->SecAxis1);

	if (FirstActor && (!FirstActor->isDynamic() || FirstActor->readBodyFlag(NX_BF_FROZEN)))
	{
		// Transform into world space
		NxMat34 FirstActorPose = FirstActor->getGlobalPose();
		NxVec3 WorldAnchor = FirstActorPose * FirstAnchor;
		NxVec3 WorldAxis = FirstActorPose.M * FirstAxis;
		NxVec3 WorldNormal = FirstActorPose.M * FirstNormal;

		FirstAnchor = WorldAnchor;
		FirstAxis = WorldAxis;
		FirstNormal = WorldNormal;
		FirstActor = NULL;
	}

	NxVec3 SecondAnchor = U2NVectorCopy(Setup->Pos2 * Scale);
	NxVec3 SecondAxis = U2NVectorCopy(Setup->PriAxis2);
	NxVec3 SecondNormal = U2NVectorCopy(Setup->SecAxis2);

	if (SecondActor && (!SecondActor->isDynamic() || SecondActor->readBodyFlag(NX_BF_FROZEN)))
	{
		// Transform into world space
		NxMat34 SecondActorPose = SecondActor->getGlobalPose();
		NxVec3 WorldAnchor = SecondActorPose * SecondAnchor;
		NxVec3 WorldAxis = SecondActorPose.M * SecondAxis;
		NxVec3 WorldNormal = SecondActorPose.M * SecondNormal;

		SecondAnchor = WorldAnchor;
		SecondAxis = WorldAxis;
		SecondNormal = WorldNormal;
		SecondActor = NULL;
	}

	Desc.localAnchor[0] = FirstAnchor;
	Desc.localAnchor[1] = SecondAnchor;

	Desc.localAxis[0] = FirstAxis;
	Desc.localAxis[1] = SecondAxis;

	Desc.localNormal[0] = FirstNormal;
	Desc.localNormal[1] = SecondNormal;

	Desc.actor[0] = FirstActor;
	Desc.actor[1] = SecondActor;
}
#endif // WITH_NOVODEX

template<typename JointDescType> void SetupBreakableJoint(JointDescType& JointDesc, URB_ConstraintSetup* setup)
{
	bool Breakable = false;

	if (setup->bLinearBreakable)
	{
		JointDesc.maxForce = setup->LinearBreakThreshold;
		Breakable = true;
	}

	if (setup->bAngularBreakable)
	{
		JointDesc.maxTorque = setup->AngularBreakThreshold;
		Breakable = true;
	}
}

void URB_ConstraintInstance::InitConstraint(AActor* actor1, AActor* actor2, URB_ConstraintSetup* setup, FLOAT Scale, AActor* inOwner)
{
	Owner = inOwner;

#ifdef WITH_NOVODEX
	check(Owner->GetLevel());
	NxScene* Scene = Owner->GetLevel()->NovodexScene;
	
	if (!Scene)
		return;

	NxActor* FirstActor = GetConstraintActor(actor1, setup->ConstraintBone1);
	NxActor* SecondActor = GetConstraintActor(actor2, setup->ConstraintBone2);

	UBOOL bLockTwist = setup->bTwistLimited && (setup->TwistLimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockSwing1 = setup->bSwingLimited && (setup->Swing1LimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockSwing2 = setup->bSwingLimited && (setup->Swing2LimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockAllSwing = bLockSwing1 && bLockSwing2;
	UBOOL bLinearXLocked = setup->LinearXSetup.bLimited && (setup->LinearXSetup.LimitSize < RB_MinSizeToLockDOF);
	UBOOL bLinearYLocked = setup->LinearYSetup.bLimited && (setup->LinearYSetup.LimitSize < RB_MinSizeToLockDOF);
	UBOOL bLinearZLocked = setup->LinearZSetup.bLimited && (setup->LinearZSetup.LimitSize < RB_MinSizeToLockDOF);

	UBOOL bPrismatic = false;
	NxVec3 PrismaticAxis;
	FLOAT PrismaticLimit = 0.0f;
	if (bLockAllSwing && bLockTwist)
	{
		if (bLinearYLocked && bLinearZLocked)
		{
			PrismaticAxis = NxVec3(1, 0, 0);
			PrismaticLimit = setup->LinearXSetup.LimitSize;
			bPrismatic = true;
		}
		else if (bLinearXLocked && bLinearZLocked)
		{
			PrismaticAxis = NxVec3(0, 1, 0);
			PrismaticLimit = setup->LinearYSetup.LimitSize;
			bPrismatic = true;
		}
		else if (bLinearXLocked && bLinearYLocked)
		{
			PrismaticAxis = NxVec3(0, 0, 1);
			PrismaticLimit = setup->LinearZSetup.LimitSize;
			bPrismatic = true;
		}
	}

	NxJoint* Joint = NULL;
	ConstraintData = (Fpointer) NULL;

	if (bLinearXLocked && bLinearYLocked && bLinearZLocked)
	{
		if (bLockAllSwing)
		{
			NxRevoluteJointDesc Desc;
			InitJointDesc(Desc, FirstActor, SecondActor, setup, Scale);

			Desc.flags = 0;
			Desc.projectionMode = NX_JPM_POINT_MINDIST;
			Desc.projectionDistance = 0.1f;

			if (setup->bTwistLimited)
			{
				FLOAT TwistLimit = 0.0001f;
				Desc.flags = NX_RJF_LIMIT_ENABLED;

				if (!bLockTwist)
				{
					TwistLimit = setup->TwistLimitAngle * (PI/180.0f);
				}

				Desc.limit.low.value = -TwistLimit;
				Desc.limit.high.value = TwistLimit;
			}

			SetupBreakableJoint(Desc, setup);

			if (!Desc.isValid())
			{
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - Invalid revolute joint description, %s"), Owner->GetName());
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - Low twist limit: %f"), Desc.limit.low.value);
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - High twist limit: %f"), Desc.limit.high.value);
				return;
			}

			Joint = Scene->createJoint(Desc);
			check(Joint);
		}
		else
		{
			NxSphericalJointDesc Desc;
			InitJointDesc(Desc, FirstActor, SecondActor, setup, Scale);

			Desc.flags = 0;
			Desc.projectionMode = NX_JPM_POINT_MINDIST;
			Desc.projectionDistance = 0.1f;

			if (setup->bTwistLimited)
			{
				FLOAT TwistLimit = Max<FLOAT>(setup->TwistLimitAngle * (PI/180.0f), 0.0001f);
				Desc.twistLimit.low.value = -TwistLimit;
				Desc.twistLimit.high.value = TwistLimit;
				Desc.flags |= NX_SJF_TWIST_LIMIT_ENABLED;
			}

			if (setup->bSwingLimited)
			{
				// NX_TODO: Change this once Novodex supports 'anisotropic' swing limits
                Desc.swingLimit.value = Max<FLOAT>(setup->Swing1LimitAngle, setup->Swing2LimitAngle) * (PI/180.0f);
				Desc.flags |= NX_SJF_SWING_LIMIT_ENABLED;
			}

			SetupBreakableJoint(Desc, setup);

			if (!Desc.isValid())
			{
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - Invalid spherical joint description, %s"), Owner->GetName());
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - Low twist limit: %f"), Desc.twistLimit.low.value);
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - High twist limit: %f"), Desc.twistLimit.high.value);
				debugf(TEXT("URB_ConstraintInstance::InitConstraint - Swing limit: %f"), Desc.swingLimit.value);
				return;
			}

			Joint = Scene->createJoint(Desc);
			check(Joint);
		}
	}
// NX_TODO: Make prismatic joints not suck. Currently joint offsetting is not working correctly and
// limit planes also seem to interact less than favorably with other constraints, such as mouse constraints.
#if 0
	else if (bLinearYLocked && bLinearZLocked)
	{
		NxPrismaticJointDesc Desc;
		InitJointDesc(Desc, FirstActor, SecondActor, setup, Scale);

		if (!Desc.isValid())
		{
			debugf(TEXT("URB_ConstraintInstance::InitConstraint - Invalid prismatic joint description, %s"), Owner->GetName());
			return;
		}

		Joint = Scene->createJoint(Desc);
		check(Joint);

		NxVec3 LimitBasePoint;
		SecondActor->localToGlobalSpace(Desc.localAnchor[1], LimitBasePoint);

		Joint->setLimitPoint(LimitBasePoint);

		NxVec3 LimitPlaneNormal;
		SecondActor->localToGlobalSpaceDirection(Desc.localAxis[1], LimitPlaneNormal);

		NxVec3 FirstAnchor;
		FirstActor->localToGlobalSpace(Desc.localAnchor[0], FirstAnchor);

		NxVec3 SecondAnchor;
		SecondActor->localToGlobalSpace(Desc.localAnchor[1], SecondAnchor);

		NxVec3 LimitOffset = setup->LinearXSetup.LimitSize * U2PScale * Scale * LimitPlaneNormal;

		if (!Joint->addLimitPlane(LimitPlaneNormal, LimitBasePoint - LimitOffset))
			debugf(TEXT("URB_ConstraintInstance::InitConstraint - Failed to add first limit plane to prismatic joint, %s"), Owner->GetName());
		if (!Joint->addLimitPlane(-LimitPlaneNormal, LimitBasePoint + LimitOffset))
			debugf(TEXT("URB_ConstraintInstance::InitConstraint - Failed to add second limit plane to prismatic joint, %s"), Owner->GetName());
	}
#endif
	else
	{
		debugf(TEXT("URB_ConstraintInstance::InitConstraint - Unsupported joint type, %s"), Owner->GetName());
		return;
	}

	NxReal MaxForce, MaxTorque;
	Joint->getBreakable(MaxForce, MaxTorque);
	//debugf(TEXT("InitConstraint -- MaxForce: %f, MaxTorque: %f"), MaxForce, MaxTorque);	
	//debugf(TEXT("InitConstraint -- Simulation method: %c"), Joint->getMethod() == NxJoint::JM_LAGRANGE ? 'L' : 'R');

	Joint->userData = this;
	ConstraintData = (Fpointer) Joint;
#endif // WITH_NOVODEX
}


// This should destroy any constraint
void URB_ConstraintInstance::TermConstraint()
{
#ifdef WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)	ConstraintData;
	if (!Joint)
		return;

	check(Owner);
	NxScene* Scene = Owner->GetLevel()->NovodexScene;
	if (Scene)
		Scene->releaseJoint(*Joint);

	ConstraintData = NULL;
#endif // WITH_NOVODEX
}