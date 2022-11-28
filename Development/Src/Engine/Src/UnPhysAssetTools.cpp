/*=============================================================================
	UnPhysAsset.cpp: Physics Asset Tools - tools for creating a collection of rigid bodies and joints.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"

static const FLOAT	DefaultPrimSize = 15.0f;
static const FLOAT	MinPrimSize = 0.5f;


///////////////////////////////////////	
//////////// UPhysicsAsset ////////////
///////////////////////////////////////

/** Returns INDEX_NONE if no children or more than 1 child. */
static INT GetChildIndex(INT BoneIndex, USkeletalMesh* skelMesh)
{
	INT ChildIndex = INDEX_NONE;

	for(INT i=0; i<skelMesh->RefSkeleton.Num(); i++)
	{
		INT ParentIndex = skelMesh->RefSkeleton(i).ParentIndex;

		if(ParentIndex == BoneIndex)
		{
			if(ChildIndex != INDEX_NONE)
			{

				return INDEX_NONE; // if we already have a child, this bone has more than one so return INDEX_NONE.
			}
			else
			{
				ChildIndex = i;
			}
		}
	}

	return ChildIndex;
}

static FLOAT CalcBoneInfoMinDimension(const FBoneVertInfo& Info)
{
	FBox BoneBox(0);
	for(INT j=0; j<Info.Positions.Num(); j++)
	{
		BoneBox += Info.Positions(j);
	}

	if(BoneBox.IsValid)
	{
		FVector BoxExtent = BoneBox.GetExtent();
		return BoxExtent.GetMin();
	}
	else
	{
		return 0.f;
	}
}

/**
 * For all bones below the give bone index, find each ones minimum box dimension, and return the maximum over those bones.
 * This is used to decide if we should create physics for a bone even if its small, because there are good-sized bones below it.
 */
static FLOAT GetMaximalMinSizeBelow(INT BoneIndex, USkeletalMesh* SkelMesh, const TArray<FBoneVertInfo>& Infos)
{
	check( Infos.Num() == SkelMesh->RefSkeleton.Num() );

	debugf(TEXT("-------------------------------------------------"));

	FLOAT MaximalMinBoxSize = 0.f;

	// For all bones that are children of the supplied one...
	for(INT i=BoneIndex; i<SkelMesh->RefSkeleton.Num(); i++)
	{
		if( SkelMesh->BoneIsChildOf(i, BoneIndex) )
		{
			FLOAT MinBoneDim = CalcBoneInfoMinDimension( Infos(i) );
			
			debugf( TEXT("Parent: %s Bone: %s Size: %f"), *SkelMesh->RefSkeleton(BoneIndex).Name, *SkelMesh->RefSkeleton(i).Name, MinBoneDim );

			MaximalMinBoxSize = Max(MaximalMinBoxSize, MinBoneDim);
		}
	}

	return MaximalMinBoxSize;
}

/**
 * Given a USkeletalMesh, construct a new PhysicsAsset automatically, using the vertices weighted to each bone to calculate approximate collision geometry.
 * Ball-and-socket joints will be created for every joint by default.
 */
UBOOL UPhysicsAsset::CreateFromSkeletalMesh( USkeletalMesh* skelMesh, FPhysAssetCreateParams& Params )
{
	DefaultSkelMesh = skelMesh;

	// Create an empty default PhysicsInstance
	DefaultInstance = ConstructObject<UPhysicsAssetInstance>( UPhysicsAssetInstance::StaticClass(), this, NAME_None, RF_Transactional );

	// For each bone, get the vertices most firmly attached to it.
	TArray<FBoneVertInfo> Infos;

	if(Params.VertWeight == EVW_DominantWeight)
	{
		skelMesh->CalcBoneVertInfos(Infos, true);
	}
	else
	{
		skelMesh->CalcBoneVertInfos(Infos, false);
	}

	check( Infos.Num() == skelMesh->RefSkeleton.Num() );

	UBOOL bHitRoot = false;

	// Iterate over each graphics bone creating body/joint.
	for(INT i=0; i<skelMesh->RefSkeleton.Num(); i++)
	{
		FName BoneName = skelMesh->RefSkeleton(i).Name;

		INT ParentIndex = INDEX_NONE;
		FName ParentName = NAME_None; 
		INT ParentBodyIndex = INDEX_NONE;

		// If we have already found the 'physics root', we expect a parent.
		if(bHitRoot)
		{
			ParentIndex = skelMesh->RefSkeleton(i).ParentIndex;
			ParentName = skelMesh->RefSkeleton(ParentIndex).Name;
			ParentBodyIndex = FindBodyIndex(ParentName);

			// Ignore bones with no physical parent (except root)
			if(ParentBodyIndex == INDEX_NONE)
			{
				continue;
			}
		}

		// Determine if we should create a physics body for this bone
		UBOOL bMakeBone = false;

		// If we have passed the physics 'root', and this bone has no physical parent, ignore it.
		if( !(bHitRoot && ParentBodyIndex == INDEX_NONE) )
		{
			// If bone is big enough - create physics.
			if( CalcBoneInfoMinDimension( Infos(i) ) > Params.MinBoneSize )
			{
				bMakeBone = true;
			}

			// If its too small, and its not the physics root, and we have set the option, see if it has any large children.
			if( !bMakeBone && bHitRoot && Params.bWalkPastSmall )
			{
				if( GetMaximalMinSizeBelow(i, skelMesh, Infos) > Params.MinBoneSize )
				{
					bMakeBone = true;
				}
			}
		}

		if( bMakeBone )
		{
			// Go ahead and make this bone physical.
			INT NewBodyIndex = CreateNewBody( BoneName );
			URB_BodySetup* bs = BodySetup( NewBodyIndex );
			check(bs->BoneName == BoneName);

			// Fill in collision info for this bone.
			CreateCollisionFromBone(bs, skelMesh, i, Params, Infos);

			// If not root - create joint to parent body.
			if(bHitRoot && Params.bCreateJoints)
			{
				INT NewConstraintIndex = CreateNewConstraint( BoneName );
				URB_ConstraintSetup* cs = ConstraintSetup( NewConstraintIndex );

				// Transform of child from parent is just child ref-pose entry.
				FMatrix RelTM = skelMesh->GetRefPoseMatrix(i);

				// Place joint at origin of child
				cs->ConstraintBone1 = BoneName;
				cs->Pos1 = FVector(0,0,0);
				cs->PriAxis1 = FVector(1,0,0);
				cs->SecAxis1 = FVector(0,1,0);

				cs->ConstraintBone2 = ParentName;
				cs->Pos2 = RelTM.GetOrigin() * U2PScale;
				cs->PriAxis2 = RelTM.GetAxis(0);
				cs->SecAxis2 = RelTM.GetAxis(1);

				// Disable collision between constrained bodies by default.
				URB_BodyInstance* bodyInstance = DefaultInstance->Bodies(NewBodyIndex);
				URB_BodyInstance* parentInstance = DefaultInstance->Bodies(ParentBodyIndex);

				DefaultInstance->DisableCollision(bodyInstance, parentInstance);
			}

			bHitRoot = true;
		}
	}

	return 1;
}


/** 
 * Replaces any collision already in 'bs' with an auto-generated one using params provided.
 */
void UPhysicsAsset::CreateCollisionFromBone( URB_BodySetup* bs, USkeletalMesh* skelMesh, INT BoneIndex, FPhysAssetCreateParams& Params, TArray<FBoneVertInfo>& Infos )
{
	// Empty any existing collision.
	bs->AggGeom.EmptyElements();

	// Calculate orientation of to use for collision primitive.
	FMatrix ElemTM;
	UBOOL bSphyl;

	if(Params.bAlignDownBone)
	{
		INT ChildIndex = GetChildIndex(BoneIndex, skelMesh);
		if(ChildIndex != INDEX_NONE)
		{
			// Get position of child relative to parent.
			FMatrix RelTM = skelMesh->GetRefPoseMatrix(ChildIndex);
			FVector ChildPos = RelTM.GetOrigin();

			// ZAxis for collision geometry lies down axis to child bone.
			FVector ZAxis = ChildPos.SafeNormal();

			// Then we pick X and Y randomly. 
			// JTODO: Should project all the vertices onto ZAxis plane and fit a bounding box using calipers or something...
			FVector XAxis, YAxis;
			ZAxis.FindBestAxisVectors( YAxis, XAxis );

			ElemTM = FMatrix( XAxis, YAxis, ZAxis, FVector(0) );

			bSphyl = true;
		}
		else
		{
			ElemTM = FMatrix::Identity;
			bSphyl = false;
		}
	}
	else
	{
		ElemTM = FMatrix::Identity;
		bSphyl = false;
	}

	// Get the (Unreal scale) bounding box for this bone using the rotation.
	FBoneVertInfo* BoneInfo = &Infos(BoneIndex);
	FBox BoneBox(0);
	for(INT j=0; j<BoneInfo->Positions.Num(); j++)
	{
		BoneBox += ElemTM.InverseTransformFVector( BoneInfo->Positions(j)  );
	}

	FVector BoxCenter(0,0,0), BoxExtent(0,0,0);

	if( BoneBox.IsValid )
		BoneBox.GetCenterAndExtents(BoxCenter, BoxExtent);

	FLOAT MinRad = BoxExtent.GetMin();
	FLOAT MinAllowedSize = Max(Params.MinBoneSize, MinPrimSize);

	// If the primitive is going to be too small - just use some default numbers and let the user tweak.
	if( MinRad < MinAllowedSize )
	{
		BoxExtent = FVector(DefaultPrimSize, DefaultPrimSize, DefaultPrimSize);
	}

	FVector BoneOrigin = ElemTM.TransformFVector( BoxCenter );
	ElemTM.SetOrigin( BoneOrigin );

	if(Params.GeomType == EFG_Box)
	{
		// Add a new box geometry to this body the size of the bounding box.
		int ex = bs->AggGeom.BoxElems.AddZeroed();
		FKBoxElem* be = &bs->AggGeom.BoxElems(ex);

		be->TM = ElemTM;

		be->X = BoxExtent.X * 2.0f * 1.01f; // Side Lengths (add 1% to avoid graphics glitches)
		be->Y = BoxExtent.Y * 2.0f * 1.01f;
		be->Z = BoxExtent.Z * 2.0f * 1.01f;	
	}
	else
	{
		if(bSphyl)
		{
			int sx = bs->AggGeom.SphylElems.AddZeroed();
			FKSphylElem* se = &bs->AggGeom.SphylElems(sx);

			se->TM = ElemTM;

			se->Radius = Max(BoxExtent.X, BoxExtent.Y) * 1.01f;
			se->Length = BoxExtent.Z * 1.01f;
		}
		else
		{
			int sx = bs->AggGeom.SphereElems.AddZeroed();
			FKSphereElem* se = &bs->AggGeom.SphereElems(sx);

			se->TM = ElemTM;

			se->Radius = BoxExtent.GetMax() * 1.01f;
		}
	}

}

/**
 * Does a few things:
 * - add any collision primitives from body2 into body1 (adjusting eaches tm).
 * - reconnect any constraints between 'add body' to 'base body', destroying any between them.
 * - update collision disable table for any pairs including 'add body'
 */
void UPhysicsAsset::WeldBodies(INT BaseBodyIndex, INT AddBodyIndex, USkeletalMeshComponent* SkelComp)
{
	if(BaseBodyIndex == INDEX_NONE || AddBodyIndex == INDEX_NONE)
		return;

	FVector Scale3D = SkelComp->Owner->DrawScale3D * SkelComp->Owner->DrawScale;
	check( Scale3D.IsUniform() );
	FVector InvScale3D( 1.0f/Scale3D.X );

	URB_BodySetup* Body1 = BodySetup(BaseBodyIndex);
	INT Bone1Index = SkelComp->SkeletalMesh->MatchRefBone(Body1->BoneName);
	check(Bone1Index != INDEX_NONE);
	FMatrix Bone1TM = SkelComp->GetBoneMatrix(Bone1Index);
	Bone1TM.RemoveScaling();
	Bone1TM.ScaleTranslation(InvScale3D); // Remove any asset scaling here.
	FMatrix InvBone1TM = Bone1TM.Inverse();

	URB_BodySetup* Body2 = BodySetup(AddBodyIndex);
	INT Bone2Index = SkelComp->SkeletalMesh->MatchRefBone(Body2->BoneName);
	check(Bone2Index != INDEX_NONE);
	FMatrix Bone2TM = SkelComp->GetBoneMatrix(Bone2Index);
	Bone2TM.RemoveScaling();
	Bone2TM.ScaleTranslation(InvScale3D);

	FMatrix Bone2ToBone1TM = Bone2TM * InvBone1TM;

	// First copy all collision info over.
	for(INT i=0; i<Body2->AggGeom.SphereElems.Num(); i++)
	{
		INT NewPrimIndex = Body1->AggGeom.SphereElems.AddItem( Body2->AggGeom.SphereElems(i) );
		Body1->AggGeom.SphereElems(NewPrimIndex).TM = Body2->AggGeom.SphereElems(i).TM * Bone2ToBone1TM; // Make transform relative to body 1 instead of body 2
	}

	for(INT i=0; i<Body2->AggGeom.BoxElems.Num(); i++)
	{
		INT NewPrimIndex = Body1->AggGeom.BoxElems.AddItem( Body2->AggGeom.BoxElems(i) );
		Body1->AggGeom.BoxElems(NewPrimIndex).TM = Body2->AggGeom.BoxElems(i).TM * Bone2ToBone1TM;
	}

	for(INT i=0; i<Body2->AggGeom.SphylElems.Num(); i++)
	{
		INT NewPrimIndex = Body1->AggGeom.SphylElems.AddItem( Body2->AggGeom.SphylElems(i) );
		Body1->AggGeom.SphylElems(NewPrimIndex).TM = Body2->AggGeom.SphylElems(i).TM * Bone2ToBone1TM;
	}

	for(INT i=0; i<Body2->AggGeom.ConvexElems.Num(); i++)
	{
		// No matrix here- we transform all the vertices into the new ref frame instead.
		INT NewPrimIndex = Body1->AggGeom.ConvexElems.AddItem( Body2->AggGeom.ConvexElems(i) );
		FKConvexElem* cElem= &Body1->AggGeom.ConvexElems(NewPrimIndex);

		for(INT j=0; j<cElem->VertexData.Num(); j++)
		{
			cElem->VertexData(j) = Bone2ToBone1TM.TransformFVector( cElem->VertexData(j) );
		}
	}

	// We need to update the collision disable table to shift any pairs that included body2 to include body1 instead.
	// We remove any pairs that include body2 & body1.

	for(INT i=0; i<BodySetup.Num(); i++)
	{
		if(i == AddBodyIndex) 
			continue;

		QWORD Key = RigidBodyIndicesToKey(i, AddBodyIndex);

		if( DefaultInstance->CollisionDisableTable.Find(Key) )
		{
			DefaultInstance->CollisionDisableTable.Remove(Key);

			// Only re-add pair if its not between 'base' and 'add' bodies.
			if(i != BaseBodyIndex)
			{
				QWORD NewKey = RigidBodyIndicesToKey(i, BaseBodyIndex);
				DefaultInstance->CollisionDisableTable.Set(NewKey, 0);
			}
		}
	}

	// Make a sensible guess for the other flags

	Body1->bNoCollision			= Body1->bNoCollision || Body2->bNoCollision;
	Body1->bFixed				= Body1->bFixed || Body2->bFixed;
	Body1->bBlockNonZeroExtent	= Body1->bBlockNonZeroExtent || Body2->bBlockNonZeroExtent;
	Body1->bBlockZeroExtent		= Body1->bBlockZeroExtent || Body2->bBlockZeroExtent;

	// Then deal with any constraints.

	TArray<INT>	Body2Constraints;
	BodyFindConstraints(AddBodyIndex, Body2Constraints);

	while( Body2Constraints.Num() > 0 )
	{
		INT ConstraintIndex = Body2Constraints(0);
		URB_ConstraintSetup* Setup = ConstraintSetup(ConstraintIndex);

		FName OtherBodyName;
		if( Setup->ConstraintBone1 == Body2->BoneName )
			OtherBodyName = Setup->ConstraintBone2;
		else
			OtherBodyName = Setup->ConstraintBone1;

		// If this is a constraint between the two bodies we are welding, we just destroy it.
		if(OtherBodyName == Body1->BoneName)
		{
			DestroyConstraint(ConstraintIndex);
		}
		else // Otherwise, we reconnect it to body1 (the 'base' body) instead of body2 (the 'weldee').
		{
			if(Setup->ConstraintBone2 == Body2->BoneName)
			{
				Setup->ConstraintBone2 = Body1->BoneName;

				FMatrix ConFrame = Setup->GetRefFrameMatrix(1);
				Setup->SetRefFrameMatrix(1, ConFrame * Bone2ToBone1TM);
			}
			else
			{
				Setup->ConstraintBone1 = Body1->BoneName;

				FMatrix ConFrame = Setup->GetRefFrameMatrix(0);
				Setup->SetRefFrameMatrix(0, ConFrame * Bone2ToBone1TM);
			}
		}

		// See if we have any more constraints to body2.
		BodyFindConstraints(AddBodyIndex, Body2Constraints);
	}

	// Finally remove the body
	DestroyBody(AddBodyIndex);

}

INT UPhysicsAsset::CreateNewConstraint(FName constraintName, URB_ConstraintSetup* constraintSetup)
{
	// constraintClass must be a subclass of URB_ConstraintSetup
	check( ConstraintSetup.Num() == DefaultInstance->Constraints.Num() );

	INT constraintIndex = FindConstraintIndex(constraintName);
	if(constraintIndex != INDEX_NONE)
		return constraintIndex;


	URB_ConstraintSetup* newConstraintSetup = ConstructObject<URB_ConstraintSetup>( URB_ConstraintSetup::StaticClass(), this, NAME_None, RF_Transactional );

	if(constraintSetup)
		newConstraintSetup->CopyConstraintParamsFrom( constraintSetup );

	INT ConstraintSetupIndex = ConstraintSetup.AddItem( newConstraintSetup );
	newConstraintSetup->JointName = constraintName;


	URB_ConstraintInstance* newConstraintInstance = ConstructObject<URB_ConstraintInstance>( URB_ConstraintInstance::StaticClass(), DefaultInstance, NAME_None, RF_Transactional );
	INT ConstraintInstanceIndex = DefaultInstance->Constraints.AddItem( newConstraintInstance );

	check(ConstraintSetupIndex == ConstraintInstanceIndex);

	return ConstraintSetupIndex;

}

void UPhysicsAsset::DestroyConstraint(INT constraintIndex)
{
	ConstraintSetup.Remove(constraintIndex);
	DefaultInstance->Constraints.Remove(constraintIndex);

}

// Create a new URB_BodySetup and default URB_BodyInstance if there is not one for this body already.
// Returns the Index for this body.
INT UPhysicsAsset::CreateNewBody(FName bodyName)
{
	check( BodySetup.Num() == DefaultInstance->Bodies.Num() );

	INT bodyIndex = FindBodyIndex(bodyName);
	if(bodyIndex != INDEX_NONE)
		return bodyIndex; // if we already have one for this name - just return that.

	URB_BodySetup* newBodySetup = ConstructObject<URB_BodySetup>( URB_BodySetup::StaticClass(), this, NAME_None, RF_Transactional );
	INT BodySetupIndex = BodySetup.AddItem( newBodySetup );
	newBodySetup->BoneName = bodyName;

	URB_BodyInstance* newBodyInstance = ConstructObject<URB_BodyInstance>( URB_BodyInstance::StaticClass(), DefaultInstance, NAME_None, RF_Transactional );
	INT BodyInstanceIndex = DefaultInstance->Bodies.AddItem( newBodyInstance );

	check(BodySetupIndex == BodyInstanceIndex);

	UpdateBodyIndices();

	// Return index of new body.
	return BodySetupIndex;

}

void UPhysicsAsset::DestroyBody(INT bodyIndex)
{
	// First we must correct the CollisionDisableTable.
	// All elements which refer to bodyIndex are removed.
	// All elements which refer to a body with index >bodyIndex are adjusted. 

	TMap<QWORD, UBOOL> NewCDT;
	for(INT i=1; i<BodySetup.Num(); i++)
	{
		for(INT j=0; j<i; j++)
		{
			QWORD Key = RigidBodyIndicesToKey(j,i);

			// If there was an entry for this pair, and it doesn't refer to the removed body, we need to add it to the new CDT.
			if( DefaultInstance->CollisionDisableTable.Find(Key) )
			{
				if(i != bodyIndex && j != bodyIndex)
				{
					INT NewI = (i > bodyIndex) ? i-1 : i;
					INT NewJ = (j > bodyIndex) ? j-1 : j;

					QWORD NewKey = RigidBodyIndicesToKey(NewJ, NewI);

					NewCDT.Set(NewKey, 0);
				}
			}
		}
	}

	DefaultInstance->CollisionDisableTable = NewCDT;

	// Now remove any constraints that were attached to this body.
	// This is a bit yuck and slow...
	TArray<INT> Constraints;
	BodyFindConstraints(bodyIndex, Constraints);

	while(Constraints.Num() > 0)
	{
		DestroyConstraint( Constraints(0) );
		BodyFindConstraints(bodyIndex, Constraints);
	}

	// Remove pointer from array. Actual objects will be garbage collected.
	BodySetup.Remove(bodyIndex);
	DefaultInstance->Bodies.Remove(bodyIndex);

	// Update body indices.
	UpdateBodyIndices();
}

// Ensure 'Outers' of objects are correct.
void UPhysicsAsset::FixOuters()
{
	check(DefaultInstance);
	check(BodySetup.Num() == DefaultInstance->Bodies.Num());
	check(ConstraintSetup.Num() == DefaultInstance->Constraints.Num());

	UBOOL bChangedOuter = false;

	if( DefaultInstance->GetOuter() != this )
	{
		DefaultInstance->Rename( DefaultInstance->GetName(), this );
		bChangedOuter = true;
	}

	for(INT i=0; i<BodySetup.Num(); i++)
	{
		if(BodySetup(i)->GetOuter() != this)
		{
			BodySetup(i)->Rename( BodySetup(i)->GetName(), this );
			bChangedOuter = true;
		}

		if(DefaultInstance->Bodies(i)->GetOuter() != DefaultInstance)
		{
			DefaultInstance->Bodies(i)->Rename( DefaultInstance->Bodies(i)->GetName(), DefaultInstance );
			bChangedOuter = true;
		}
	}

	for(INT i=0; i<ConstraintSetup.Num(); i++)
	{
		if(ConstraintSetup(i)->GetOuter() != this)
		{
			ConstraintSetup(i)->Rename( ConstraintSetup(i)->GetName(), this );
			bChangedOuter = true;
		}

		if(DefaultInstance->Constraints(i)->GetOuter() != DefaultInstance)
		{
			DefaultInstance->Constraints(i)->Rename( DefaultInstance->Constraints(i)->GetName(), DefaultInstance );
			bChangedOuter = true;
		}
	}

	if(bChangedOuter)
	{
		debugf( TEXT("Fixed Outers for PhysicsAsset: %s"), GetName() );
		MarkPackageDirty();
	}
}


///// THUMBNAIL SUPPORT //////

FString UPhysicsAsset::GetDesc()
{
	return FString::Printf( TEXT("%d Bodies, %d Constraints"), BodySetup.Num(), ConstraintSetup.Num() );

}

void UPhysicsAsset::DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_PhysAsset") ));

	InRI->DrawTile
		(
		InX,
		InY,
		InFixedSz ? InFixedSz : Icon->SizeX*InZoom,
		InFixedSz ? InFixedSz : Icon->SizeY*InZoom,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		FLinearColor::White,
		Icon
		);

}

FThumbnailDesc UPhysicsAsset::GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticFindObject( UTexture2D::StaticClass(), ANY_PACKAGE, TEXT("EngineResources.UnrealEdIcon_PhysAsset") ));
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : Icon->SizeX*InZoom;
	td.Height = InFixedSz ? InFixedSz : Icon->SizeY*InZoom;
	return td;

}

INT UPhysicsAsset::GetThumbnailLabels( TArray<FString>* InLabels )
{
	InLabels->Empty();

	new( *InLabels )FString( GetName() );
	new( *InLabels )FString( FString::Printf( TEXT("%d Bodies, %d Constraints"), BodySetup.Num(), ConstraintSetup.Num() ) );

	return InLabels->Num();

}