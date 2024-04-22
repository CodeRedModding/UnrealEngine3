/*=============================================================================
	UnConstraint.cpp: Physics Constraints - rigid-body constraint (joint) related classes.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ARigidBodyBase);
IMPLEMENT_CLASS(ARB_ConstraintActor);

IMPLEMENT_CLASS(URB_ConstraintSetup);
IMPLEMENT_CLASS(URB_BSJointSetup);
IMPLEMENT_CLASS(URB_HingeSetup);
IMPLEMENT_CLASS(URB_PrismaticSetup);
IMPLEMENT_CLASS(URB_SkelJointSetup);
IMPLEMENT_CLASS(URB_PulleyJointSetup);
IMPLEMENT_CLASS(URB_StayUprightSetup);
IMPLEMENT_CLASS(URB_DistanceJointSetup);

IMPLEMENT_CLASS(URB_ConstraintInstance);

IMPLEMENT_CLASS(URB_ConstraintDrawComponent);

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// UTILS ///////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

FMatrix FindBodyMatrix(AActor* Actor, FName BoneName)
{
	if(!Actor)
	{
		return FMatrix::Identity;
	}

	// Jointing to a skeletal mesh component (which isn't using single body physics) - find the bone.
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Actor->CollisionComponent);
	if(SkelComp && !SkelComp->bUseSingleBodyPhysics)
	{
		if(BoneName != NAME_None)
		{
			INT BoneIndex = 0;

			if (BoneName != NAME_None)
				BoneIndex = SkelComp->MatchRefBone(BoneName);

			if(BoneIndex != INDEX_NONE)
			{	
				FMatrix BodyTM = SkelComp->GetBoneMatrix(BoneIndex);
				BodyTM.RemoveScaling();
				return BodyTM;
			}
			else
			{
				debugf( TEXT("FindBodyMatrix : Could not find bone '%s'"), *BoneName.ToString() );
				return FMatrix::Identity;
			}
		}
		else
		{
			debugf( TEXT("FindBodyMatrix : Connecting to SkeletalMesh of Actor '%s', but no BoneName specified"), *Actor->GetName() );
			return FMatrix::Identity;
		}
	}
	// Non skeletal (ie single body) case.
	else
	{
		if(Actor->CollisionComponent)
		{
			FMatrix BodyTM = Actor->CollisionComponent->LocalToWorld;
			BodyTM.RemoveScaling();
			return BodyTM;
		}
		else
		{
			debugf( TEXT("FindBodyMatrix : No CollisionComponent for Actor '%s'."), *Actor->GetName() );
			return FMatrix::Identity;
		}
	}
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
			FVector TotalScale(SkelComp->Scale * SkelComp->Scale3D.X * Actor->DrawScale * Actor->DrawScale3D.X);

			FMatrix BoneTM = SkelComp->GetBoneMatrix(BoneIndex);
			BoneTM.RemoveScaling();

			return SkelComp->PhysicsAsset->BodySetup(BodyIndex)->AggGeom.CalcAABB(BoneTM, TotalScale);
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

/** InitConstraint()
 * - Spawn an RB_ConstraintActor
 * - Fill in the ConstraintActor1 and ConstraintActor2, as well as ConstraintBone1/ConstraintBone2 in the ConstraintSetup if you attaching to bones of a skeletal mesh.
 * - Once everything is in correct locations, call ARB_ConstraintActor::PostEditMove to update the relative reference frame (Pos1, PriAxis1 etc).
 * - Then call InitRBPhys on the ConstraintActor, to start the joint up.
 * - If you want to make it breakable, set bLinearBreakable/ LinearBreakThreshold before calling InitRBPhys.
 */
void ARB_ConstraintActor::InitConstraint( AActor* Actor1, AActor* Actor2, FName Actor1Bone, FName Actor2Bone, FLOAT BreakThreshold)
{
	if (ConstraintSetup != NULL)
	{
		ConstraintActor1 = Actor1;
		ConstraintActor2 = Actor2;
		ConstraintSetup->ConstraintBone1 = Actor1Bone;
		ConstraintSetup->ConstraintBone2 = Actor2Bone;
		if ( BreakThreshold > 0.f )
		{
			ConstraintSetup->bLinearBreakable = true;
			ConstraintSetup->LinearBreakThreshold = BreakThreshold;
		}
		UpdateConstraintFramesFromActor();
		InitRBPhys();
	}
	else
	{
		debugf(NAME_Warning, TEXT("Cannot initialize '%s' because it has no ConstraintSetup"), *GetName());
	}
}

/** Shut down the constraint - breaking it. */
void ARB_ConstraintActor::TermConstraint()
{
	TermRBPhys(NULL);
}

/** 
 *	Update the reference frames held inside the ConstraintSetup that indicate the joint location in the reference frame 
 *	of the two connected Actors. You should call this whenever the constraint or either actor moves, or if you change
 *	the connected actors. THis function does nothing though once the joint has bee initialised (InitRBPhys).
 */
void ARB_ConstraintActor::UpdateConstraintFramesFromActor()
{
	check(ConstraintSetup);
	check(ConstraintInstance);

	FMatrix a1TM = FindBodyMatrix(ConstraintActor1, ConstraintSetup->ConstraintBone1);
	a1TM.ScaleTranslation( FVector(U2PScale,U2PScale,U2PScale) );

	FMatrix a2TM = FindBodyMatrix(ConstraintActor2, ConstraintSetup->ConstraintBone2);
	a2TM.ScaleTranslation( FVector(U2PScale,U2PScale,U2PScale) );

	// Calculate position/axis from constraint actor.
	const FRotationMatrix ConMatrix = FRotationMatrix(Rotation);

	// World ref frame
	const FVector wPos = Location * U2PScale;
	const FVector wPri = ConMatrix.GetAxis(0);
	const FVector wOrth = ConMatrix.GetAxis(1);

	if(bUpdateActor1RefFrame)
	{
		const FMatrix a1TMInv = a1TM.Inverse();
		ConstraintSetup->Pos1 = a1TMInv.TransformFVector(wPos);
		ConstraintSetup->PriAxis1 = a1TMInv.TransformNormal(wPri);
		ConstraintSetup->SecAxis1 = a1TMInv.TransformNormal(wOrth);
	}

	if(bUpdateActor2RefFrame)
	{
		const FMatrix a2TMInv = a2TM.Inverse();
		ConstraintSetup->Pos2 = a2TMInv.TransformFVector(wPos);
		ConstraintSetup->PriAxis2 = a2TMInv.TransformNormal(wPri);
		ConstraintSetup->SecAxis2 = a2TMInv.TransformNormal(wOrth);
	}

	if(PulleyPivotActor1)
	{
		ConstraintSetup->PulleyPivot1 = PulleyPivotActor1->Location;
	}

	if(PulleyPivotActor2)
	{
		ConstraintSetup->PulleyPivot2 = PulleyPivotActor2->Location;
	}

	// Update draw component (and hence scene proxy) to reflect new information
	ForceUpdateComponents(FALSE,FALSE);
}

/** 
 *	Called when we move a constraint to update the position/axis held in local space (ie. relative to each connected actor)
 */
void ARB_ConstraintActor::PostEditMove(UBOOL bFinished)
{
	UpdateConstraintFramesFromActor();
	Super::PostEditMove(bFinished);
}

void ARB_ConstraintActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(GIsEditor)
	{
		PostEditMove( TRUE );
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#if WITH_EDITOR
void ARB_ConstraintActor::CheckForErrors()
{
	Super::CheckForErrors();
	if( ConstraintSetup == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ConstraintSetupNull" ), *GetName() ) ), TEXT( "ConstraintSetupNull" ) );
	}
	if( ConstraintInstance == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_ConstraintInstanceNull" ), *GetName() ) ), TEXT( "ConstraintInstanceNull" ) );
	}

	// Make sure constraint actors are not both NULL.
	if ( ConstraintActor1 == NULL && ConstraintActor2 == NULL )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_BothConstraintsNull" ), *GetName() ) ), TEXT( "BothConstraintsNull" ) );
	}
	else
	{
		// Make sure constraint actors are not both static.
		if ( ConstraintActor1 != NULL && ConstraintActor2 != NULL )
		{
			if ( ConstraintActor1->IsStatic() && ConstraintActor2->IsStatic() )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_BothConstraintsStatic" ), *GetName() ) ), TEXT( "BothConstraintsStatic" ) );
			}
		}
		else
		{
			// At this point, we know one constraint actor is NULL and the other is non-NULL.
			// Check that the non-NULL constraint actor is not static.
			if ( ( ConstraintActor1 == NULL && ConstraintActor2->IsStatic() ) ||
				 ( ConstraintActor2 == NULL && ConstraintActor1->IsStatic() ) )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SingleStaticActor" ), *GetName() ) ), TEXT( "SingleStaticActor" ) );
			}
		}
	}
}
#endif

void ARB_ConstraintActor::InitRBPhys()
{
	// Make sure not setting one of the ConstraintActors to point to myself.
	// If you do that, it goes into an infinite InitRBPhys call loop.
	if(ConstraintActor1 == this)
	{
		ConstraintActor1 = NULL;
	}

	if(ConstraintActor2 == this)
	{
		ConstraintActor2 = NULL;
	}

	// Ensure connected actors have their physics started up.
	UPrimitiveComponent* PrimComp1 = NULL;
	if(ConstraintActor1)
	{
		ConstraintActor1->InitRBPhys();
		PrimComp1 = ConstraintActor1->CollisionComponent;
	}

	UPrimitiveComponent* PrimComp2 = NULL;
	if(ConstraintActor2)
	{
		ConstraintActor2->InitRBPhys();
		PrimComp2 = ConstraintActor2->CollisionComponent;
	}

	if (ConstraintSetup != NULL && (PrimComp1 != NULL || PrimComp2 != NULL))
	{
		ConstraintInstance->InitConstraint(PrimComp1, PrimComp2, ConstraintSetup, 1.0f, this, NULL, FALSE);
		SetDisableCollision(bDisableCollision);
	}
}

/**
 *	Shut down physics for this constraint.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics it the constraint is in that scene.
 */
void ARB_ConstraintActor::TermRBPhys(FRBPhysScene* Scene)
{
	ConstraintInstance->TermConstraint(Scene, FALSE);
}

#if WITH_NOVODEX

static NxActor* GetNxActorFromActor(AActor* Actor, FName BoneName)
{
	if (Actor && Actor->CollisionComponent)
	{
		return Actor->CollisionComponent->GetNxActor(BoneName);
	}

	return NULL;
}

static NxActor* GetNxActorFromComponent(UPrimitiveComponent* PrimComp, FName BoneName)
{
	if (PrimComp)
	{
		return PrimComp->GetNxActor(BoneName);
	}

	return NULL;
}

#endif // WITH_NOVODEX

void ARB_ConstraintActor::SetDisableCollision(UBOOL NewDisableCollision)
{
#if WITH_NOVODEX
	NxActor* FirstActor = GetNxActorFromActor(ConstraintActor1, ConstraintSetup->ConstraintBone1);
	NxActor* SecondActor = GetNxActorFromActor(ConstraintActor2, ConstraintSetup->ConstraintBone2);
	
	if (!FirstActor || !SecondActor)
		return;
	
	// Get the scene actors are in.
	NxScene* NovodexScene = &(FirstActor->getScene());

	// Flip the NX_IGNORE_PAIR flag (without changing anything else).
	NxU32 CurrentFlags = NovodexScene->getActorPairFlags(*FirstActor, *SecondActor);
	if (bDisableCollision)
	{
		NovodexScene->setActorPairFlags(*FirstActor, *SecondActor, CurrentFlags | NX_IGNORE_PAIR);
	}
	else
	{
		NovodexScene->setActorPairFlags(*FirstActor, *SecondActor, CurrentFlags & ~NX_IGNORE_PAIR);
	}
#endif

	bDisableCollision = NewDisableCollision;
}

//////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// CONSTRAINT SETUP ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

void URB_ConstraintSetup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// This is pretty vile, but can't find another way to get and ConstraintActor that may contain this ConstraintSetup
	for( FActorIterator It; It; ++ It )
	{
		ARB_ConstraintActor* ConActor = Cast<ARB_ConstraintActor>(*It);
		if(ConActor && ConActor->ConstraintSetup == this)
		{
			ConActor->PostEditChange();
			return;
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
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

	bLinearLimitSoft = fromSetup->bLinearLimitSoft;

	LinearLimitStiffness = fromSetup->LinearLimitStiffness;
	LinearLimitDamping = fromSetup->LinearLimitDamping;

	bLinearBreakable = fromSetup->bLinearBreakable;
	LinearBreakThreshold = fromSetup->LinearBreakThreshold;

	bSwingLimited = fromSetup->bSwingLimited;
	bTwistLimited = fromSetup->bTwistLimited;

	bSwingLimitSoft = fromSetup->bSwingLimitSoft;
	bTwistLimitSoft = fromSetup->bTwistLimitSoft;

	Swing1LimitAngle = fromSetup->Swing1LimitAngle;
	Swing2LimitAngle = fromSetup->Swing2LimitAngle;
	TwistLimitAngle = fromSetup->TwistLimitAngle;

	SwingLimitStiffness = fromSetup->SwingLimitStiffness;
	SwingLimitDamping = fromSetup->SwingLimitDamping;

	TwistLimitStiffness = fromSetup->TwistLimitStiffness;
	TwistLimitDamping = fromSetup->TwistLimitDamping;

	bAngularBreakable = fromSetup->bAngularBreakable;
	AngularBreakThreshold = fromSetup->AngularBreakThreshold;

	bIsPulley = fromSetup->bIsPulley;
	bMaintainMinDistance = fromSetup->bMaintainMinDistance;
	PulleyRatio = fromSetup->PulleyRatio;
}


static FString ConstructJointBodyName(AActor* Actor, FName ConstraintBone)
{
	if( Actor )
	{
		if(ConstraintBone == NAME_None)
			return Actor->GetName();
		else
			return FString::Printf(TEXT("%s.%s"), *Actor->GetName(), *ConstraintBone.ToString());
	}
	else
	{
		if(ConstraintBone == NAME_None)
			return FString(TEXT("@World"));
		else
			return ConstraintBone.ToString();
	}
}


//////////////////////////////////////////////////////////////////////////////////////
///////////////////////////// CONSTRAINT INSTANCE ////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

#if WITH_NOVODEX

static void InitJointDesc(NxJointDesc& Desc, NxActor* FirstActor, NxActor* SecondActor, URB_ConstraintSetup* Setup, FLOAT Scale)
{
	Desc.jointFlags = NX_JF_COLLISION_ENABLED;
	#if !FINAL_RELEASE
	Desc.jointFlags |= NX_JF_VISUALIZATION;
	#endif

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

	// Because Novodex keeps limits/axes locked in the first body reference frame, whereas Unreal keeps them in the second body reference frame,
	// we have to flip the bodies here.
	Desc.localAnchor[1] = FirstAnchor;
	Desc.localAnchor[0] = SecondAnchor;

	Desc.localAxis[1] = FirstAxis;
	Desc.localAxis[0] = SecondAxis;

	Desc.localNormal[1] = FirstNormal;
	Desc.localNormal[0] = SecondNormal;

	Desc.actor[1] = FirstActor;
	Desc.actor[0] = SecondActor;
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

/** Handy macro for setting BIT of VAR based on the UBOOL CONDITION */
#define SET_DRIVE_PARAM(VAR, CONDITION, BIT)   (VAR) = (CONDITION) ? ((VAR) | (BIT)) : ((VAR) & ~(BIT))


/** 
 *	Create physics engine constraint.
 */
void URB_ConstraintInstance::InitConstraint(UPrimitiveComponent* PrimComp1, UPrimitiveComponent* PrimComp2, class URB_ConstraintSetup* Setup, FLOAT Scale, AActor* InOwner, UPrimitiveComponent* InPrimComp, UBOOL bMakeKinForBody1)
{
	// if there's already a constraint, get rid of it first
	if (ConstraintData != NULL)
	{
		TermConstraint(NULL, FALSE);
	}

	Owner = InOwner;
	OwnerComponent = InPrimComp;

#if WITH_NOVODEX
	NxActor* FirstActor = GetNxActorFromComponent(PrimComp1, Setup->ConstraintBone1);
	NxActor* SecondActor = GetNxActorFromComponent(PrimComp2, Setup->ConstraintBone2);

	// Do not create joint if there are no actors to connect.
	if(!FirstActor && !SecondActor)
	{
		return;
	}

	// Create kinematic proxy at location of first actor, and use that instead.
	if(bMakeKinForBody1)
	{
		FMatrix KinActorTM;
		NxScene* KinActorScene;
		// If we have an actor, use its transform
		if(FirstActor)
		{
			KinActorTM = N2UTransform(FirstActor->getGlobalPose());
			KinActorScene = &(FirstActor->getScene());
		}
		// Otherwise, create with identity
		else
		{
			KinActorTM = FMatrix::Identity;
			KinActorScene = &(SecondActor->getScene());
		}

		FirstActor = CreateDummyKinActor(KinActorScene, KinActorTM);

		// Keep pointer to this kinematic actor.
		DummyKinActor = FirstActor;
	}

	// Get scene from the actors we want to connect.
	NxScene* Scene = NULL;

	// make sure actors are in same scene
	NxScene* Scene1 = NULL;
	NxScene* Scene2 = NULL;

	if(FirstActor)
	{
		Scene1 = &(FirstActor->getScene());
	}

	if(SecondActor)
	{
		Scene2 = &(SecondActor->getScene());
	}

	// make sure actors are in same scene
	if(Scene1 && Scene2 && Scene1 != Scene2)
	{
		debugf( TEXT("Attempting to create a joint between actors in two different scenes.  No joint created.") );
		return;
	}

	Scene = Scene1 ? Scene1 : Scene2;

	check(Scene);

	NxJoint* Joint = NULL;
	ConstraintData = NULL;

	if(Setup->bIsPulley)
	{
		NxPulleyJointDesc Desc;
		InitJointDesc(Desc, FirstActor, SecondActor, Setup, Scale);
		
		// Copy properties from setup.
		Desc.flags = 0;
		if(Setup->bMaintainMinDistance)
		{
			Desc.flags = NX_PJF_IS_RIGID;
		}

		// Because Novodex bodies are opposite order to Unreal bodies, need to flip this here.
		Desc.pulley[1] = U2NPosition(Setup->PulleyPivot1);
		Desc.pulley[0] = U2NPosition(Setup->PulleyPivot2);

		Desc.ratio = Setup->PulleyRatio;

		// Calculate the distance so the current state is the rest state.
		NxReal CurrentDistance = 0.f;

		if(FirstActor)
		{
			NxVec3 FirstAnchor = U2NVectorCopy(Setup->Pos1 * Scale);
			NxMat34 FirstActorPose = FirstActor->getGlobalPose();
			NxVec3 WorldAnchor = FirstActorPose * FirstAnchor;
			CurrentDistance += (WorldAnchor - Desc.pulley[1]).magnitude();
		}

		if(SecondActor)
		{
			NxVec3 SecondAnchor = U2NVectorCopy(Setup->Pos2 * Scale);
			NxMat34 SecondActorPose = SecondActor->getGlobalPose();
			NxVec3 WorldAnchor = SecondActorPose * SecondAnchor;
			CurrentDistance += (WorldAnchor - Desc.pulley[0]).magnitude() * Setup->PulleyRatio;
		}

		Desc.distance = CurrentDistance;
		//Desc.distance = 0.f;

		SetupBreakableJoint(Desc, Setup);

		// Finally actually create the joint
		Joint = Scene->createJoint(Desc);
		check(Joint);
	}
	else
	{
		NxD6JointDesc Desc;
		InitJointDesc(Desc, FirstActor, SecondActor, Setup, Scale);

		Desc.flags = 0;

		if( Setup->bEnableProjection )
		{
			Desc.projectionMode = NX_JPM_POINT_MINDIST;
			Desc.projectionDistance = 0.1f; // Linear projection when error > 0.1
			Desc.projectionAngle = 10.f * ((FLOAT)PI/180.0f); // Angular projection when error > 10 degrees
		}

		/////////////// TWIST LIMIT
		if(Setup->bTwistLimited)
		{
			Desc.twistMotion = (Setup->TwistLimitAngle < RB_MinAngleToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;

			FLOAT TwistLimitRad = Setup->TwistLimitAngle * (PI/180.0f);
			Desc.twistLimit.high.value = TwistLimitRad;
			Desc.twistLimit.low.value = -TwistLimitRad;

			if(Setup->bTwistLimitSoft)
			{
				Desc.twistLimit.high.spring = Setup->TwistLimitStiffness;
				Desc.twistLimit.low.spring = Setup->TwistLimitStiffness;

				Desc.twistLimit.high.damping = Setup->TwistLimitDamping;
				Desc.twistLimit.low.damping = Setup->TwistLimitDamping;
			}
		}

		/////////////// SWING LIMIT
		if(Setup->bSwingLimited)
		{
			// Novodex swing directions are different from Unreal's - so change here.
			Desc.swing2Motion = (Setup->Swing1LimitAngle < RB_MinAngleToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;
			Desc.swing1Motion = (Setup->Swing2LimitAngle < RB_MinAngleToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;

			Desc.swing2Limit.value = Setup->Swing1LimitAngle * (PI/180.0f);
			Desc.swing1Limit.value = Setup->Swing2LimitAngle * (PI/180.0f);

			if(Setup->bSwingLimitSoft)
			{
				Desc.swing2Limit.spring = Setup->SwingLimitStiffness;
				Desc.swing1Limit.spring = Setup->SwingLimitStiffness;
				Desc.swing2Limit.damping = Setup->SwingLimitDamping;
				Desc.swing1Limit.damping = Setup->SwingLimitDamping;
			}
		}

		/////////////// LINEAR LIMIT
		FLOAT MaxLinearLimit = 0.f;
		if(Setup->LinearXSetup.bLimited)
		{
			Desc.xMotion = (Setup->LinearXSetup.LimitSize < RB_MinSizeToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;
			MaxLinearLimit = ::Max(MaxLinearLimit, Setup->LinearXSetup.LimitSize);
		}

		if(Setup->LinearYSetup.bLimited)
		{
			Desc.yMotion = (Setup->LinearYSetup.LimitSize < RB_MinSizeToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;
			MaxLinearLimit = ::Max(MaxLinearLimit, Setup->LinearYSetup.LimitSize);
		}

		if(Setup->LinearZSetup.bLimited)
		{
			Desc.zMotion = (Setup->LinearZSetup.LimitSize < RB_MinSizeToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;
			MaxLinearLimit = ::Max(MaxLinearLimit, Setup->LinearZSetup.LimitSize);
		}

		Desc.linearLimit.value = MaxLinearLimit * U2PScale;

		if(Setup->bLinearLimitSoft)
		{
			Desc.linearLimit.spring = Setup->LinearLimitStiffness;
			Desc.linearLimit.damping = Setup->LinearLimitDamping;
		}

		///////// DRIVE

#if 1
		// Turn on 'slerp drive' if desired.
		// Can't use slerp drive if any angular axis is locked.
		if( bAngularSlerpDrive &&
			Desc.swing1Motion != NX_D6JOINT_MOTION_LOCKED &&
			Desc.swing2Motion != NX_D6JOINT_MOTION_LOCKED &&
			Desc.twistMotion != NX_D6JOINT_MOTION_LOCKED )
		{
			Desc.flags = Desc.flags | NX_D6JOINT_SLERP_DRIVE;
		}
#endif

		// Setup for breakable
		SetupBreakableJoint(Desc, Setup);

		// Check description is ok.
		if (!Desc.isValid())
		{
			FName OwnerName = NAME_None;
			if(Owner)
			{
				OwnerName = Owner->GetFName();
			}

			debugf(TEXT("URB_ConstraintInstance::InitConstraint - Invalid 6DOF joint description (%s - %s)"), *OwnerName.ToString(), *Setup->JointName.ToString());
			return;
		}

		Joint = Scene->createJoint(Desc);
		check(Joint);
	}

	NxReal MaxForce, MaxTorque;
	Joint->getBreakable(MaxForce, MaxTorque);
	//debugf(TEXT("InitConstraint -- MaxForce: %f, MaxTorque: %f"), MaxForce, MaxTorque);	
	//debugf(TEXT("InitConstraint -- Simulation method: %c"), Joint->getMethod() == NxJoint::JM_LAGRANGE ? 'L' : 'R');

	Joint->userData = this;

	// Remember reference to scene index.
	FRBPhysScene* RBScene = (FRBPhysScene*)Scene->userData;
	SceneIndex = RBScene->NovodexSceneIndex;

	ConstraintData = Joint;

	// Make initial calls to constraint motor stuff.
	SetLinearPositionTarget(LinearPositionTarget);
	SetLinearVelocityTarget(LinearVelocityTarget);

	SetAngularPositionTarget(AngularPositionTarget);
	SetAngularVelocityTarget(AngularVelocityTarget);


	NxD6Joint* D6Joint = Joint->isD6Joint();
	if(D6Joint)
	{
		NxD6JointDesc Desc;
		D6Joint->saveToDesc(Desc);

		SET_DRIVE_PARAM(Desc.xDrive.driveType, bLinearXPositionDrive, NX_D6JOINT_DRIVE_POSITION);
		SET_DRIVE_PARAM(Desc.yDrive.driveType, bLinearYPositionDrive, NX_D6JOINT_DRIVE_POSITION);
		SET_DRIVE_PARAM(Desc.zDrive.driveType, bLinearZPositionDrive, NX_D6JOINT_DRIVE_POSITION);

		SET_DRIVE_PARAM(Desc.xDrive.driveType, bLinearXVelocityDrive, NX_D6JOINT_DRIVE_VELOCITY);
		SET_DRIVE_PARAM(Desc.yDrive.driveType, bLinearYVelocityDrive, NX_D6JOINT_DRIVE_VELOCITY);
		SET_DRIVE_PARAM(Desc.zDrive.driveType, bLinearZVelocityDrive, NX_D6JOINT_DRIVE_VELOCITY);

		SET_DRIVE_PARAM(Desc.swingDrive.driveType, bSwingPositionDrive, NX_D6JOINT_DRIVE_POSITION);
		SET_DRIVE_PARAM(Desc.twistDrive.driveType, bTwistPositionDrive, NX_D6JOINT_DRIVE_POSITION);
		SET_DRIVE_PARAM(Desc.slerpDrive.driveType, bSwingPositionDrive && bTwistPositionDrive, NX_D6JOINT_DRIVE_POSITION);

		SET_DRIVE_PARAM(Desc.swingDrive.driveType, bSwingVelocityDrive, NX_D6JOINT_DRIVE_VELOCITY);
		SET_DRIVE_PARAM(Desc.twistDrive.driveType, bTwistVelocityDrive, NX_D6JOINT_DRIVE_VELOCITY);
		SET_DRIVE_PARAM(Desc.slerpDrive.driveType, bSwingVelocityDrive && bTwistVelocityDrive, NX_D6JOINT_DRIVE_VELOCITY);

		// If the Outer of this constraint instance is a physics asset instance, we take into account the overall motor scaling from there.
		UPhysicsAssetInstance* Inst = GetPhysicsAssetInstance();
		FLOAT SpringScale = 1.f;
		FLOAT DampScale = 1.f;
		FLOAT ForceScale = 1.f;
		if(Inst)
		{
			SpringScale = Inst->LinearSpringScale;
			DampScale = Inst->LinearDampingScale;
			ForceScale = Inst->LinearForceLimitScale;
		}

		Desc.xDrive.spring		= Desc.yDrive.spring		= Desc.zDrive.spring		= LinearDriveSpring * SpringScale;
		Desc.xDrive.damping		= Desc.yDrive.damping		= Desc.zDrive.damping		= LinearDriveDamping * DampScale;
		FLOAT LinearForceLimit = LinearDriveForceLimit * ForceScale;
		Desc.xDrive.forceLimit	= Desc.yDrive.forceLimit	= Desc.zDrive.forceLimit	= LinearForceLimit > 0.f ? LinearForceLimit : FLT_MAX;

		// Grab angular scaling
		SpringScale = 1.f;
		DampScale = 1.f;
		ForceScale = 1.f;
		if(Inst)
		{
			SpringScale = Inst->AngularSpringScale;
			DampScale = Inst->AngularDampingScale;
			ForceScale = Inst->AngularForceLimitScale;
		}

		Desc.swingDrive.spring		= Desc.twistDrive.spring		= Desc.slerpDrive.spring		= AngularDriveSpring * SpringScale;
		Desc.swingDrive.damping		= Desc.twistDrive.damping		= Desc.slerpDrive.damping		= AngularDriveDamping * DampScale;
		FLOAT AngularForceLimit = AngularDriveForceLimit * ForceScale;
		Desc.swingDrive.forceLimit	= Desc.twistDrive.forceLimit	= Desc.slerpDrive.forceLimit	= AngularForceLimit > 0.f ? AngularForceLimit : FLT_MAX;

		D6Joint->loadFromDesc(Desc);
	}
#endif // WITH_NOVODEX
}


/**
 *	Clean up the physics engine info for this instance.
 *	If Scene is NULL, it will always shut down physics. If an RBPhysScene is passed in, it will only shut down physics if the constraint is in that scene. 
 *	Returns TRUE if physics was shut down, and FALSE otherwise.
 */
UBOOL URB_ConstraintInstance::TermConstraint(FRBPhysScene* Scene, UBOOL bFireBrokenEvent)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)	ConstraintData;
	if (!Joint)
	{
		return TRUE;
	}

	// If this joint is in the scene we want (or we passed in NULL), kill it.
	if(Scene == NULL || SceneIndex == Scene->NovodexSceneIndex)
	{
		// use correct scene
		NxScenePair* nPair = GetNovodexScenePairFromIndex(SceneIndex);
		if(nPair)
		{
			NxScene* nScene = nPair->PrimaryScene;
			if(nScene)
			{
				// If desired, fire the 'constraint has broken' event. This should only really happen when calling TermConstraint from script.
				if(bFireBrokenEvent)
				{
					URB_ConstraintInstance* Inst = (URB_ConstraintInstance*)(Joint->userData);
					if(Inst)
					{
						USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Inst->OwnerComponent);
						if(SkelComp)
						{	
							check(SkelComp->PhysicsAssetInstance);
							check(SkelComp->PhysicsAsset);
							check(SkelComp->PhysicsAssetInstance->Constraints.Num() == SkelComp->PhysicsAsset->ConstraintSetup.Num());
							check(Inst->ConstraintIndex < SkelComp->PhysicsAsset->ConstraintSetup.Num());

							if( Inst->Owner && !Inst->Owner->bPendingDelete && !Inst->Owner->IsPendingKill() )
							{
								URB_ConstraintSetup* Setup = SkelComp->PhysicsAsset->ConstraintSetup(Inst->ConstraintIndex);
								// maybe the constraint name  (add to TTP)
								const FVector& ConstrainLocation = Inst->GetConstraintLocation();
								Inst->Owner->eventConstraintBrokenNotify( Inst->Owner, Setup, Inst );
							}
						}
					}
				}

				if (!GIsEditor)
				{
					DeferredReleaseNxJoint(Joint, TRUE);
				}
				else
				{
					nScene->releaseJoint(*Joint);
				}
				NxActor* KinActor = (NxActor*)DummyKinActor;
				if(KinActor)
				{
					DestroyDummyKinActor(KinActor);
					KinActor = NULL;
				}
			}
		}

		ConstraintData = NULL;

		bTerminated = TRUE;

		// Indicate joint was terminated.
		return TRUE;
	}
#endif // WITH_NOVODEX

	return FALSE;
}

void URB_ConstraintInstance::FinishDestroy()
{
	// Clean up physics-engine constrint stuff when instance is GC'd
	TermConstraint(NULL, FALSE);

	Super::FinishDestroy();
}

void URB_ConstraintInstance::execTermConstraint( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

	TermConstraint(NULL, TRUE);
}

void URB_ConstraintInstance::CopyInstanceParamsFrom(class URB_ConstraintInstance* fromInstance)
{
	bLinearXPositionDrive = fromInstance->bLinearXPositionDrive;
	bLinearXVelocityDrive = fromInstance->bLinearXVelocityDrive;

	bLinearYPositionDrive = fromInstance->bLinearYPositionDrive;
	bLinearYVelocityDrive = fromInstance->bLinearYVelocityDrive;

	bLinearZPositionDrive = fromInstance->bLinearZPositionDrive;
	bLinearZVelocityDrive = fromInstance->bLinearZVelocityDrive;

	LinearPositionTarget = fromInstance->LinearPositionTarget;
	LinearVelocityTarget = fromInstance->LinearVelocityTarget;
	LinearDriveSpring = fromInstance->LinearDriveSpring;
	LinearDriveDamping = fromInstance->LinearDriveDamping;
	LinearDriveForceLimit = fromInstance->LinearDriveForceLimit;

	bSwingPositionDrive = fromInstance->bSwingPositionDrive;
	bSwingVelocityDrive = fromInstance->bSwingVelocityDrive;

	bTwistPositionDrive = fromInstance->bTwistPositionDrive;
	bTwistVelocityDrive = fromInstance->bTwistVelocityDrive;

	bAngularSlerpDrive = fromInstance->bAngularSlerpDrive;

	AngularPositionTarget = fromInstance->AngularPositionTarget;
	AngularVelocityTarget = fromInstance->AngularVelocityTarget;
	AngularDriveSpring = fromInstance->AngularDriveSpring;
	AngularDriveDamping = fromInstance->AngularDriveDamping;
	AngularDriveForceLimit = fromInstance->AngularDriveForceLimit;
}

/** Returns the PhysicsAssetInstance that owns this RB_ConstraintInstance (if there is one) */
UPhysicsAssetInstance* URB_ConstraintInstance::GetPhysicsAssetInstance()
{
	USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(OwnerComponent);
	if(SkelComp)
	{
		return SkelComp->PhysicsAssetInstance;
	}

	return NULL;
}

/** Get the position of this constraint in world space. */
FVector URB_ConstraintInstance::GetConstraintLocation()
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)	ConstraintData;
	if (!Joint)
	{
		return FVector(0,0,0);
	}

	NxVec3 nJointPos = Joint->getGlobalAnchor();

	return N2UPosition(nJointPos);
#else
	return FVector(0,0,0);
#endif
}

/** Function for turning linear position drive on and off. */
void URB_ConstraintInstance::SetLinearPositionDrive(UBOOL bEnableXDrive, UBOOL bEnableYDrive, UBOOL bEnableZDrive)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint && Joint->getState() != NX_JS_BROKEN)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			SET_DRIVE_PARAM(Desc.xDrive.driveType, bEnableXDrive, NX_D6JOINT_DRIVE_POSITION);
			SET_DRIVE_PARAM(Desc.yDrive.driveType, bEnableYDrive, NX_D6JOINT_DRIVE_POSITION);
			SET_DRIVE_PARAM(Desc.zDrive.driveType, bEnableZDrive, NX_D6JOINT_DRIVE_POSITION);

			D6Joint->loadFromDesc(Desc);
		}
	}
#endif

	bLinearXPositionDrive = bEnableXDrive;
	bLinearYPositionDrive = bEnableYDrive;
	bLinearZPositionDrive = bEnableZDrive;
}


/** Function for turning linear velocity drive on and off. */
void URB_ConstraintInstance::SetLinearVelocityDrive(UBOOL bEnableXDrive, UBOOL bEnableYDrive, UBOOL bEnableZDrive)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint && Joint->getState() != NX_JS_BROKEN)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			SET_DRIVE_PARAM(Desc.xDrive.driveType, bEnableXDrive, NX_D6JOINT_DRIVE_VELOCITY);
			SET_DRIVE_PARAM(Desc.yDrive.driveType, bEnableYDrive, NX_D6JOINT_DRIVE_VELOCITY);
			SET_DRIVE_PARAM(Desc.zDrive.driveType, bEnableZDrive, NX_D6JOINT_DRIVE_VELOCITY);

			D6Joint->loadFromDesc(Desc);
		}
	}
#endif

	bLinearXVelocityDrive = bEnableXDrive;
	bLinearYVelocityDrive = bEnableYDrive;
	bLinearZVelocityDrive = bEnableZDrive;
}


/** Function for turning angular position drive on and off. */
void URB_ConstraintInstance::SetAngularPositionDrive(UBOOL bEnableSwingDrive, UBOOL bEnableTwistDrive)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint && Joint->getState() != NX_JS_BROKEN)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			SET_DRIVE_PARAM(Desc.swingDrive.driveType, bEnableSwingDrive, NX_D6JOINT_DRIVE_POSITION);
			SET_DRIVE_PARAM(Desc.twistDrive.driveType, bEnableTwistDrive, NX_D6JOINT_DRIVE_POSITION);

			// If both drives are on, turn slerp drive on (in case its being used)
			// It's ok to have both drives on - it will only use one type of drive, based on the NX_D6JOINT_SLERP_DRIVE flag.
			SET_DRIVE_PARAM(Desc.slerpDrive.driveType, bEnableSwingDrive && bEnableTwistDrive, NX_D6JOINT_DRIVE_POSITION);

			D6Joint->loadFromDesc(Desc);
		}
	}
#endif

	bSwingPositionDrive = bEnableSwingDrive;
	bTwistPositionDrive = bEnableTwistDrive;
}

/** Function for turning angular velocity drive on and off. */
void URB_ConstraintInstance::SetAngularVelocityDrive(UBOOL bEnableSwingDrive, UBOOL bEnableTwistDrive)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint && Joint->getState() != NX_JS_BROKEN)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			SET_DRIVE_PARAM(Desc.swingDrive.driveType, bEnableSwingDrive, NX_D6JOINT_DRIVE_VELOCITY);
			SET_DRIVE_PARAM(Desc.twistDrive.driveType, bEnableTwistDrive, NX_D6JOINT_DRIVE_VELOCITY);

			// If both drives are on, turn slerp drive on (in case its being used)
			SET_DRIVE_PARAM(Desc.slerpDrive.driveType, bEnableSwingDrive && bEnableTwistDrive, NX_D6JOINT_DRIVE_VELOCITY);

			D6Joint->loadFromDesc(Desc);
		}
	}
#endif

	bSwingVelocityDrive = bEnableSwingDrive;
	bTwistVelocityDrive = bEnableTwistDrive;
}

/** Function for setting linear position target. */
void URB_ConstraintInstance::SetLinearPositionTarget(FVector InPosTarget)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxVec3 nPos = U2NPosition(InPosTarget);
			D6Joint->setDrivePosition(nPos);
		}
	}
#endif

	LinearPositionTarget = InPosTarget;
}

/** Function for setting linear velocity target. */
void URB_ConstraintInstance::SetLinearVelocityTarget(FVector InVelTarget)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxVec3 nVel = U2NPosition(InVelTarget);
			D6Joint->setDriveLinearVelocity(nVel);
		}
	}
#endif

	LinearVelocityTarget = InVelTarget;
}

/** Function for setting angular motor parameters. */
void URB_ConstraintInstance::SetLinearDriveParams(FLOAT InSpring, FLOAT InDamping, FLOAT InForceLimit)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint && Joint->getState() != NX_JS_BROKEN)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			// If the Outer of this constraint instance is a physics asset instance, we take into account the
			// overall motor scaling from there.
			UPhysicsAssetInstance* Inst = GetPhysicsAssetInstance();
			FLOAT SpringScale = 1.f;
			FLOAT DampScale = 1.f;
			FLOAT ForceScale = 1.f;
			if(Inst)
			{
				SpringScale = Inst->LinearSpringScale;
				DampScale = Inst->LinearDampingScale;
				ForceScale = Inst->LinearForceLimitScale;
			}

			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			Desc.xDrive.spring		= Desc.yDrive.spring		= Desc.zDrive.spring		= InSpring * SpringScale;
			Desc.xDrive.damping		= Desc.yDrive.damping		= Desc.zDrive.damping		= InDamping * DampScale;
			FLOAT LinearForceLimit = InForceLimit * ForceScale;
			Desc.xDrive.forceLimit	= Desc.yDrive.forceLimit	= Desc.zDrive.forceLimit	= LinearForceLimit > 0.f ? LinearForceLimit : FLT_MAX;

			D6Joint->loadFromDesc(Desc);
		}
	}
#endif

	LinearDriveSpring = InSpring;
	LinearDriveDamping = InDamping;
	LinearDriveForceLimit = InForceLimit;
}

/** Function for setting target angular position. */
void URB_ConstraintInstance::SetAngularPositionTarget(const FQuat& InPosTarget)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxQuat nQuat = U2NQuaternion(InPosTarget);
			D6Joint->setDriveOrientation(nQuat);
		}
	}
#endif

	AngularPositionTarget = InPosTarget;
}


/** Function for setting target angular velocity. */
void URB_ConstraintInstance::SetAngularVelocityTarget(FVector InVelTarget)
{
	// If settings are the same, don't do anything.
	if( AngularVelocityTarget == InVelTarget )
	{
		return;
	}

#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			NxVec3 nAngVel = U2NVectorCopy(InVelTarget * 2 * (FLOAT)PI); // Convert from revs per second to radians
			D6Joint->setDriveAngularVelocity(nAngVel);
		}
	}
#endif

	AngularVelocityTarget = InVelTarget;
}

/** Function for setting angular motor parameters. */
void URB_ConstraintInstance::SetAngularDriveParams(FLOAT InSpring, FLOAT InDamping, FLOAT InForceLimit)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if (Joint && Joint->getState() != NX_JS_BROKEN)
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if(D6Joint)
		{
			// If the Outer of this constraint instance is a physics asset instance, we take into account the
			// overall motor scaling from there.
			UPhysicsAssetInstance* Inst = GetPhysicsAssetInstance();
			FLOAT SpringScale = 1.f;
			FLOAT DampScale = 1.f;
			FLOAT ForceScale = 1.f;
			if(Inst)
			{
				SpringScale = Inst->AngularSpringScale;
				DampScale = Inst->AngularDampingScale;
				ForceScale = Inst->AngularForceLimitScale;
			}

			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			Desc.swingDrive.spring		= Desc.twistDrive.spring		= Desc.slerpDrive.spring		= InSpring * SpringScale;
			Desc.swingDrive.damping		= Desc.twistDrive.damping		= Desc.slerpDrive.damping		= InDamping * DampScale;
			FLOAT AngularForceLimit = InForceLimit * ForceScale;
			Desc.swingDrive.forceLimit	= Desc.twistDrive.forceLimit	= Desc.slerpDrive.forceLimit	= AngularForceLimit > 0.f ? AngularForceLimit : FLT_MAX;

			D6Joint->loadFromDesc(Desc);
		}
	}
#endif

	AngularDriveSpring = InSpring;
	AngularDriveDamping = InDamping;
	AngularDriveForceLimit = InForceLimit;
}


/** Scale Angular Limit Constraints (as defined in RB_ConstraintSetup) */
void URB_ConstraintInstance::SetAngularDOFLimitScale(FLOAT InSwing1LimitScale, FLOAT InSwing2LimitScale, FLOAT InTwistLimitScale, class URB_ConstraintSetup* InSetup)
{
	if( !InSetup )
	{
		debugf(TEXT("SetAngularDOFLimitScale, No RB_ConstraintSetup!"));
		return;
	}

#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if( Joint && Joint->getState() != NX_JS_BROKEN )
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if( D6Joint )
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);

			if ( InSetup->bSwingLimited )
			{
				// Novodex swing directions are different from Unreal's - so change here.
				Desc.swing2Motion		= (InSetup->Swing1LimitAngle * InSwing1LimitScale < RB_MinAngleToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;
				Desc.swing1Motion		= (InSetup->Swing2LimitAngle * InSwing2LimitScale< RB_MinAngleToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;

				Desc.swing2Limit.value	= InSetup->Swing1LimitAngle * InSwing1LimitScale * (PI/180.0f);
				Desc.swing1Limit.value	= InSetup->Swing2LimitAngle * InSwing2LimitScale * (PI/180.0f);
			}

			if ( InSetup->bTwistLimited )
			{
				Desc.twistMotion			= (InSetup->TwistLimitAngle * InTwistLimitScale < RB_MinAngleToLockDOF) ? NX_D6JOINT_MOTION_LOCKED : NX_D6JOINT_MOTION_LIMITED;
				const FLOAT TwistLimitRad	= InSetup->TwistLimitAngle * InTwistLimitScale * (PI/180.0f);
				Desc.twistLimit.high.value	= TwistLimitRad;
				Desc.twistLimit.low.value	= -TwistLimitRad;
			}
			D6Joint->loadFromDesc(Desc);
		}
	}
#endif
}

/** Allows you to dynamically change the size of the linear limit 'sphere'. */
void URB_ConstraintInstance::SetLinearLimitSize(float NewLimitSize)
{
#if WITH_NOVODEX
	NxJoint* Joint = (NxJoint*)ConstraintData;
	if( Joint && Joint->getState() != NX_JS_BROKEN )
	{
		NxD6Joint* D6Joint = Joint->isD6Joint();
		if( D6Joint )
		{
			NxD6JointDesc Desc;
			D6Joint->saveToDesc(Desc);
			Desc.linearLimit.value = NewLimitSize * U2PScale;
			D6Joint->loadFromDesc(Desc);
		}
	}
#endif
}

/** If bMakeKinForBody1 was used, this function allows the kinematic body to be moved. */
void URB_ConstraintInstance::MoveKinActorTransform(FMatrix& NewTM)
{
#if WITH_NOVODEX
	if(DummyKinActor)
	{
		NxActor* nActor = (NxActor*)DummyKinActor;
		NewTM.RemoveScaling();
		check(!NewTM.ContainsNaN());
		NxMat34 nNewPose = U2NTransform(NewTM);

		// Don't call moveGlobalPose if we are already in the correct pose. 
		// Also check matrix we are passing in is valid.
		NxMat34 nCurrentPose = nActor->getGlobalPose();
		if( nNewPose.M.determinant() > (FLOAT)KINDA_SMALL_NUMBER && 
			!MatricesAreEqual(nNewPose, nCurrentPose, (FLOAT)KINDA_SMALL_NUMBER) )
		{
			nActor->moveGlobalPose(nNewPose);
		}
	}
#endif
}

