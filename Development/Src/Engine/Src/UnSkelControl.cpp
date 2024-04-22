/*=============================================================================
	UnSkelControl.cpp: Skeletal mesh bone controllers and IK.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"

IMPLEMENT_CLASS(USkelControlBase)
IMPLEMENT_CLASS(USkelControlSingleBone)
IMPLEMENT_CLASS(USkelControlLookAt)
IMPLEMENT_CLASS(USkelControlSpline)
IMPLEMENT_CLASS(USkelControlLimb)
IMPLEMENT_CLASS(USkelControlFootPlacement)
IMPLEMENT_CLASS(USkelControlWheel)
IMPLEMENT_CLASS(USkelControlHandlebars)
IMPLEMENT_CLASS(USkelControlTrail)
IMPLEMENT_CLASS(USkelControl_CCD_IK)
IMPLEMENT_CLASS(USkelControl_TwistBone)
IMPLEMENT_CLASS(USkelControl_Multiply)

#define CONTROL_DIAMOND_SIZE	(3.f)

// FBoneAtom replica version
static void CopyRotationPart(FBoneAtom& DestBA, const FBoneAtom& SrcBA)
{
	DestBA.SetRotation(SrcBA.GetRotationV());
}

/** Utility function for turning axis indicator enum into direction vector, possibly inverted. */
FVector USkelControlBase::GetAxisDirVector(BYTE InAxis, UBOOL bInvert)
{
	FVector AxisDir;

	if(InAxis == AXIS_X)
	{
		AxisDir = FVector(1,0,0);
	}
	else if(InAxis == AXIS_Y)
	{
		AxisDir =  FVector(0,1,0);
	}
	else
	{
		AxisDir =  FVector(0,0,1);
	}

	if(bInvert)
	{
		AxisDir *= -1.f;
	}

	return AxisDir;
}

/** 
 *	Create a matrix given two arbitrary rows of it.
 *	We generate the missing row using another cross product, but we have to get the order right to avoid changing handedness.
 */
FMatrix USkelControlBase::BuildMatrixFromVectors(BYTE Vec1Axis, const FVector& Vec1, BYTE Vec2Axis, const FVector& Vec2)
{
	check(Vec1 != Vec2);

	FMatrix OutMatrix = FMatrix::Identity;

	if(Vec1Axis == AXIS_X)
	{
		OutMatrix.SetAxis(0, Vec1);

		if(Vec2Axis == AXIS_Y)
		{
			OutMatrix.SetAxis(1, Vec2);
			OutMatrix.SetAxis(2, Vec1 ^ Vec2);
		}
		else // AXIS_Z
		{
			OutMatrix.SetAxis(2, Vec2);
			OutMatrix.SetAxis(1, Vec2 ^ Vec1 );
		}
	}
	else if(Vec1Axis == AXIS_Y)
	{
		OutMatrix.SetAxis(1, Vec1);

		if(Vec2Axis == AXIS_X)
		{
			OutMatrix.SetAxis(0, Vec2);
			OutMatrix.SetAxis(2, Vec2 ^ Vec1);
		}
		else // AXIS_Z
		{
			OutMatrix.SetAxis(2, Vec2);
			OutMatrix.SetAxis(0, Vec1 ^ Vec2 );
		}
	}
	else // AXIS_Z
	{
		OutMatrix.SetAxis(2, Vec1);

		if(Vec2Axis == AXIS_X)
		{
			OutMatrix.SetAxis(0, Vec2);
			OutMatrix.SetAxis(1, Vec1 ^ Vec2);
		}
		else // AXIS_Y
		{
			OutMatrix.SetAxis(1, Vec2);
			OutMatrix.SetAxis(0, Vec2 ^ Vec1 );
		}
	}

	FLOAT Det = OutMatrix.RotDeterminant();
	if( OutMatrix.Determinant() <= 0.f )
	{
		debugf( TEXT("BuildMatrixFromVectors : Bad Determinant (%f)"), Det );
		debugf( TEXT("Vec1: %d (%f %f %f)"), Vec1Axis, Vec1.X, Vec1.Y, Vec1.Z );
		debugf( TEXT("Vec2: %d (%f %f %f)"), Vec2Axis, Vec2.X, Vec2.Y, Vec2.Z );
	}
	//check( OutMatrix.RotDeterminant() > 0.f );

	return OutMatrix;
}


/** Given two unit direction vectors, find the axis and angle between them. */
void USkelControlBase::FindAxisAndAngle(const FVector& A, const FVector& B, FVector& OutAxis, FLOAT& OutAngle)
{
	// Should always have valid input - if we hit these, need fixing at a higher level.
	check(A.Size() > KINDA_SMALL_NUMBER);
	check(B.Size() > KINDA_SMALL_NUMBER);

	// Cross product to find vector perpendicular to both
	OutAxis = A ^ B;

	FLOAT OutAxisSize = OutAxis.Size();

	// If A and B are parallel, just return any axis perp to that direction
	if(OutAxisSize < KINDA_SMALL_NUMBER)
	{
		FVector DummyAxis;
		A.FindBestAxisVectors(OutAxis, DummyAxis);
		OutAxis = OutAxis.SafeNormal();

		// See if A and B are parallel or anti-parallel, and return angle accordingly
		if((A | B) > 0.f)
		{
			OutAngle = 0.f;
		}
		else
		{
			OutAngle = (FLOAT)PI;
		}
		return;
	}

	OutAngle = appAsin( OutAxisSize ); // Will always result in positive OutAngle.
	OutAxis /= OutAxisSize;


	// If dot product is negative, adjust the angle accordingly.
	if((A | B) < 0.f)
	{
		OutAngle = (FLOAT)PI - OutAngle; 
	}

}

/*-----------------------------------------------------------------------------
	USkelControlBase
-----------------------------------------------------------------------------*/


void USkelControlBase::TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp)
{
	// Update SkelComponent pointer.
	SkelComponent = SkelComp;

	if ( bShouldTickInScript )
	{
		eventTickSkelControl(DeltaSeconds, SkelComp);
	}

	if( bShouldTickOwner && SkelComp && SkelComp->GetOwner() )
	{
		SkelComp->GetOwner()->eventTickSkelControl(DeltaSeconds, SkelComp, this);
	}

	// Check if we should set the control's strength to match a set of specific anim nodes.
	if( bSetStrengthFromAnimNode && SkelComp && SkelComp->Animations )
	{
		// if node is node initialized, cache list of nodes
		// in the editor, we do this every frame to catch nodes that have been edited/added/removed
		if( !bInitializedCachedNodeList || (GIsEditor && !GIsGame) )
		{
			bInitializedCachedNodeList = TRUE;

			CachedNodeList.Reset();

			// get all nodes
			TArray<UAnimNode*>	Nodes;
			SkelComp->Animations->GetNodes(Nodes);

			// iterate through list of nodes
			for(INT i=0; i<Nodes.Num(); i++)
			{
				UAnimNode* Node = Nodes(i);

				if( Node && Node->NodeName != NAME_None )
				{
					// iterate through our list of names
					for(INT ANodeNameIdx=0; ANodeNameIdx<StrengthAnimNodeNameList.Num(); ANodeNameIdx++)
					{
						if( Node->NodeName == StrengthAnimNodeNameList(ANodeNameIdx) )
						{
							CachedNodeList.AddItem(Node);
							break;
						}
					}
				}
			}
		}

		FLOAT Strength = 0.f;

		// iterate through list of cached nodes
		for(INT i=0; i<CachedNodeList.Num(); i++)
		{
			const UAnimNode* Node = CachedNodeList(i);
			
			if( Node && Node->bRelevant )
			{
				Strength += Node->NodeTotalWeight;
			}

			/* Here is if we want to match the weight of the most relevant node.
			// if the node's weight is greater, use that
			if( Node && Node->NodeTotalWeight > Strength )
			{
				Strength = Node->NodeTotalWeight;
			}
			*/
		}

		ControlStrength	= Min(Strength, 1.f);
		StrengthTarget	= ControlStrength;
	}

	// MetaData ticking
	// Reset weights once per frame. AnimNodeSequences will do that when relevant, but when they're not, we have to reset the weight.
	if( AnimMetaDataUpdateTag != SkelComp->TickTag )
	{
		AnimMetaDataUpdateTag = SkelComp->TickTag;
		AnimMetadataWeight = 0.f;
	}

	if( BlendTimeToGo > 0.f )
	{
		if( BlendTimeToGo > DeltaSeconds )
		{
			// Amount we want to change ControlStrength by.
			const FLOAT BlendDelta = StrengthTarget - ControlStrength; 

			ControlStrength	+= (BlendDelta / BlendTimeToGo) * DeltaSeconds;
			BlendTimeToGo	-= DeltaSeconds;
		}
		else
		{
			BlendTimeToGo	= 0.f; 
			ControlStrength	= StrengthTarget;
		}

		//debugf(TEXT("%3.2f SkelControl ControlStrength: %f, StrengthTarget: %f, BlendTimeToGo: %f"), GWorld->GetTimeSeconds(), ControlStrength, StrengthTarget, BlendTimeToGo);
	}
}


void USkelControlBase::SetSkelControlActive(UBOOL bInActive)
{
	if( bInActive )
	{
		StrengthTarget	= 1.f;
		BlendTimeToGo	= BlendInTime * Abs(StrengthTarget - ControlStrength);
	}
	else
	{
		StrengthTarget	= 0.f;
		BlendTimeToGo	= BlendOutTime * Abs(StrengthTarget - ControlStrength);
	}

	// If we want this weight NOW - update straight away (dont wait for TickSkelControl).
	if( BlendTimeToGo <= 0.f )
	{
		ControlStrength = StrengthTarget;
		BlendTimeToGo	= 0.f;
	}

	// If desired, send active call to next control in chain.
	if( NextControl && bPropagateSetActive )
	{
		NextControl->SetSkelControlActive(bInActive);
	}
}

/**
 * Set custom strength with optional blend time.
 * @param	NewStrength		Target Strength for this controller.
 * @param	InBlendTime		Time it will take to reach that new strength. (0.f == Instant)
 */
void USkelControlBase::SetSkelControlStrength(FLOAT NewStrength, FLOAT InBlendTime)
{
	StrengthTarget	= Clamp<FLOAT>(NewStrength, 0.f, 1.f);
	BlendTimeToGo	= Max(InBlendTime, 0.f) * Abs(StrengthTarget - ControlStrength);

	// If blend time is zero, apply now and don't wait a frame.
	if( BlendTimeToGo <= 0.f )
	{
		ControlStrength = StrengthTarget;
		BlendTimeToGo	= 0.f;
	}
}


/** 
 * Get Alpha for this control. By default it is ControlStrength. 
 * 0.f means no effect, 1.f means full effect.
 * ControlStrength controls whether or not CalculateNewBoneTransforms() is called.
 * By modifying GetControlAlpha() you can still get CalculateNewBoneTransforms() called
 * but not have the controller's effect applied on the mesh.
 * This is useful for cases where you need to have the skeleton built in mesh space
 * for testing, which is not available in TickSkelControl().
 */
FLOAT USkelControlBase::GetControlAlpha()
{
	return bControlledByAnimMetada ? ControlStrength * GetControlMetadataWeight() : ControlStrength;
}

/** Accessor to get Metadataweight property value. */
FLOAT USkelControlBase::GetControlMetadataWeight() const
{
	return bInvertMetadataWeight ? (1.f - AnimMetadataWeight) : AnimMetadataWeight;
}

void USkelControlBase::HandleControlSliderMove(FLOAT NewSliderValue)
{
	ControlStrength = NewSliderValue;
	StrengthTarget = NewSliderValue;
}

void USkelControlBase::PostLoad()
{
	// If desired, use cubic interpolation for skeletal control, to allow them to blend in more smoothly.
	if( bEnableEaseInOut_DEPRECATED )
	{
		// set the new blend type
		BlendType = ABT_Cubic;
		// clear original flag
		bEnableEaseInOut_DEPRECATED = FALSE;
	}

	Super::PostLoad();
}

void USkelControlBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
#if WITH_EDITORONLY_DATA
	if (Ar.Ver() < VER_DEPRECATED_EDITOR_POSITION)
	{
		if (ControlPosX_DEPRECATED!=-9999999 && ControlPosY_DEPRECATED!=-9999999)
		{
			// update editor variable
			NodePosX = ControlPosX_DEPRECATED; 
			NodePosY = ControlPosY_DEPRECATED; 
			ControlPosX_DEPRECATED = ControlPosX_DEPRECATED = -9999999;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void USkelControlBase::OnPaste()
{
	Super::OnPaste();
}
/*-----------------------------------------------------------------------------
	USkelControlSingleBone
-----------------------------------------------------------------------------*/

void USkelControlSingleBone::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);
	OutBoneIndices.AddItem(BoneIndex);
}

//
// USkelControlSingleBone::ApplySkelControl
//
void USkelControlSingleBone::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	FBoneAtom NewBoneTM = SkelComp->SpaceBases(BoneIndex);

	if( bApplyRotation )
	{
		// SpaceBases are in component space - so we need to calculate the BoneRotationSpace -> Component transform
		FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, BoneRotationSpace, RotationSpaceBoneName);
		ComponentToFrame.RemoveScaling();
		ComponentToFrame.SetOrigin( FVector(0.f) );

		FBoneAtom FrameToComponent = ComponentToFrame.InverseSafe();
		FBoneAtom RotInFrame(BoneRotation, FVector::ZeroVector);
	
		// Add to existing rotation
		FBoneAtom RotInComp;
		if( bAddRotation )
		{
			// See if we should remove the mesh rotation from that delta.
			if( bRemoveMeshRotation && BoneRotationSpace == BCS_WorldSpace )
			{
				FBoneAtom MeshRotationActor = FBoneAtom(SkelComp->Rotation, FVector::ZeroVector).Inverse();
				AActor* Owner = SkelComp->GetOwner();
				FBoneAtom ActorToWorld;
				if ( Owner )
				{
					ActorToWorld.SetMatrix(Owner->LocalToWorld());
				}
				else
				{
					ActorToWorld = FBoneAtom::Identity;
				}

				ActorToWorld.RemoveScaling();
				ActorToWorld.SetOrigin( FVector(0.f) );
				RotInFrame = RotInFrame * (ActorToWorld.Inverse() * MeshRotationActor * ActorToWorld);
			}

			RotInComp = NewBoneTM * (ComponentToFrame * RotInFrame * FrameToComponent);
		}
		// Replace current rotation
		else
		{
			RotInComp = RotInFrame * FrameToComponent;
		}
		RotInComp.SetOrigin( NewBoneTM.GetOrigin() );

		NewBoneTM = RotInComp;
	}

	if(bApplyTranslation)
	{
		// We do not remove scaling here - we want to compensate
		FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, BoneTranslationSpace, TranslationSpaceBoneName);

		// Add to current transform
		if(bAddTranslation)
		{
			FVector TransInComp = ComponentToFrame.InverseSafe().TransformNormal(BoneTranslation);

			FVector NewOrigin = NewBoneTM.GetOrigin() + TransInComp;
			NewBoneTM.SetOrigin(NewOrigin);
		}
		// Replace current translation
		else
		{
			// Translation in the component reference frame
			FVector TransInComp = ComponentToFrame.InverseSafe().TransformFVector(BoneTranslation);

			NewBoneTM.SetOrigin(TransInComp);
		}
	}

	OutBoneTransforms.AddItem(NewBoneTM);
}

INT USkelControlSingleBone::GetWidgetCount()
{
	if(bApplyTranslation)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

FBoneAtom USkelControlSingleBone::GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex)
{
	check(WidgetIndex == 0);
	FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, BoneTranslationSpace, TranslationSpaceBoneName);
	FBoneAtom LocalToWorld = SkelComp->LocalToWorldBoneAtom;
	FBoneAtom FrameToComponent = ComponentToFrame.InverseSafe() * LocalToWorld;
	FrameToComponent.SetOrigin( LocalToWorld.TransformFVector( SkelComp->SpaceBases(BoneIndex).GetOrigin() ) );

	return FrameToComponent;
}

void USkelControlSingleBone::HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec)
{
	check(WidgetIndex == 0);
	BoneTranslation += DragVec;
}

/************************************************************************************
 * USkelControl_TwistBone
 ***********************************************************************************/

#define DEBUG_TWISTBONECONTROLLER 0

void USkelControl_TwistBone::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);
	OutBoneIndices.AddItem(BoneIndex);
}

FQuat USkelControl_TwistBone::ExtractRollAngle(INT BoneIndex, USkeletalMeshComponent* SkelComp)
{
	const FBoneAtom BoneAtom = SkelComp->LocalAtoms(BoneIndex);
	const FBoneAtom BoneAtomRef(SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation, BoneAtom.GetTranslation());

	// find delta angle between the two quaternions X Axis.
	const FVector BoneAtomX = BoneAtom.GetRotation().RotateVector(FVector(1.f,0.f,0.f));
	const FVector BoneAtomRefX = BoneAtomRef.GetRotation().RotateVector(FVector(1.f,0.f,0.f));
	const FQuat LocalToRefQuat = FQuatFindBetween(BoneAtomX, BoneAtomRefX);
	checkSlow( LocalToRefQuat.IsNormalized() );

	// Rotate parent bone atom from position in local space to reference skeleton
	// Since our rotation rotates both vectors with shortest arc
	// we're essentially left with a quaternion that has roll angle difference with reference skeleton version
	const FQuat BoneQuatAligned = LocalToRefQuat * BoneAtom.GetRotation();
	checkSlow( BoneQuatAligned.IsNormalized() );

	// Find that delta roll angle
	const FQuat DeltaRollQuat = (-BoneAtomRef.GetRotation()) * BoneQuatAligned;
	checkSlow( DeltaRollQuat.IsNormalized() );

#if DEBUG_TWISTBONECONTROLLER
	debugf(TEXT("\t ExtractRollAngle, Bone: %s (%d)"), 
		*SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), BoneIndex);
	debugf( TEXT("\t\t Bone Quat: %s, Rot: %s, AxisX: %s"), *BoneAtom.GetRotation().ToString(), *BoneAtom.GetRotation().Rotator().ToString(), *BoneAtomX.ToString() );
	debugf( TEXT("\t\t BoneRef Quat: %s, Rot: %s, AxisX: %s"), *BoneAtomRef.GetRotation().ToString(), *BoneAtomRef.GetRotation().Rotator().ToString(), *BoneAtomRefX.ToString() );
	debugf( TEXT("\t\t LocalToRefQuat Quat: %s, Rot: %s"), *LocalToRefQuat.ToString(), *LocalToRefQuat.Rotator().ToString() );
	
	const FVector BoneQuatAlignedX = BoneAtom.GetRotation().RotateVector(FVector(1.f,0.f,0.f));
	debugf( TEXT("\t\t BoneQuatAligned Quat: %s, Rot: %s, AxisX: %s"), *BoneQuatAligned.ToString(), *BoneQuatAligned.Rotator().ToString(), *BoneQuatAlignedX.ToString() );
	debugf( TEXT("\t\t DeltaRollQuat Quat: %s, Rot: %s"), *DeltaRollQuat.ToString(), *DeltaRollQuat.Rotator().ToString() );

	FBoneAtom BoneAtomAligned(BoneQuatAligned, BoneAtomRef.GetTranslation());
	const FQuat DeltaQuatAligned = FQuatFindBetween(BoneAtomAligned.GetAxis(0), BoneAtomRef.GetAxis(0));
	debugf( TEXT("\t\t DeltaQuatAligned Quat: %s, Rot: %s"), *DeltaQuatAligned.ToString(), *DeltaQuatAligned.Rotator().ToString() );
	FVector DeltaRollAxis;
	FLOAT	DeltaRollAngle;
	DeltaRollQuat.ToAxisAndAngle(DeltaRollAxis, DeltaRollAngle);
	debugf( TEXT("\t\t DeltaRollAxis: %s, DeltaRollAngle: %f"), *DeltaRollAxis.ToString(), DeltaRollAngle );
#endif

	return DeltaRollQuat;
}

//
// USkelControlSingleBone::ApplySkelControl
//
void USkelControl_TwistBone::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const INT SourceBoneIndex = SkelComp->MatchRefBone(SourceBoneName);
	if( SourceBoneIndex == INDEX_NONE )
	{
		return;
	}

#if DEBUG_TWISTBONECONTROLLER
	debugf(TEXT("USkelControl_TwistBone, Bone: %s (%d), SourceBone: %s (%d)"), 
		*SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).Name.ToString(), BoneIndex,
		*SkelComp->SkeletalMesh->RefSkeleton(SourceBoneIndex).Name.ToString(), SourceBoneIndex);
#endif

	// Reference bone
	const FQuat RefQuat = SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation;

	// Find delta roll angle for lower bone.
	FQuat DeltaRollBoneQuat = ExtractRollAngle(SourceBoneIndex, SkelComp);

	// Turn to Axis and Angle
	FVector RollAxis;
	FLOAT	RollAngle;
	DeltaRollBoneQuat.ToAxisAndAngle(RollAxis, RollAngle);

	const FVector DefaultAxis = FVector(1.f,0.f,0.f);

	// See if we need to invert angle.
	if( (RollAxis | DefaultAxis) < 0.f )
	{
		RollAxis = -RollAxis;
		RollAngle = -RollAngle;
	}

	// Make sure it is the shortest angle.
	RollAngle = UnwindHeading(RollAngle);

	// New bone rotation
	FQuat NewQuat = RefQuat * FQuat(RollAxis, RollAngle * TwistAngleScale);
	// Normalize resulting quaternion.
	NewQuat.Normalize();

	// Turn that back into mesh space
	FBoneAtom NewBoneAtom(NewQuat, SkelComp->LocalAtoms(BoneIndex).GetTranslation());
	OutBoneTransforms.AddItem( NewBoneAtom * SkelComp->SpaceBases(SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex) );

#if DEBUG_TWISTBONECONTROLLER
	debugf( TEXT("\t FullDeltaQuat: %s, Rot: %s"), *FullDeltaQuat.ToString(), *FullDeltaQuat.Rotator().ToString() );
	debugf( TEXT("\t RefQuat: %s, Rot: %s"), *RefQuat.ToString(), *RefQuat.Rotator().ToString() );
	debugf( TEXT("\t NewQuat: %s, Rot: %s"), *NewQuat.ToString(), *NewQuat.Rotator().ToString() );
	debugf( TEXT("\t RollAxis: %s, RollAngle: %f"), *RollAxis.ToString(), RollAngle );
	debugf( TEXT("\t NewBoneAtom Quat: %s, Rot: %s, Axis: %s"), *NewBoneAtom.GetRotation().ToString(), *NewBoneAtom.GetRotation().Rotator().ToString(), *NewBoneAtom.GetAxis(0).ToString() );
#endif
}

/*-----------------------------------------------------------------------------
	USkelControlLookAt
-----------------------------------------------------------------------------*/

/** LookAtAlpha allows to cancel head look when going beyond boundaries */
FLOAT USkelControlLookAt::GetControlAlpha()
{
	return bControlledByAnimMetada ? ControlStrength * GetControlMetadataWeight() * LookAtAlpha : ControlStrength * LookAtAlpha;
}

void USkelControlLookAt::SetLookAtAlpha(FLOAT DesiredAlpha, FLOAT DesiredBlendTime)
{
	if( LookAtAlphaTarget != DesiredAlpha )
	{
		LookAtAlphaTarget			= DesiredAlpha;
		LookAtAlphaBlendTimeToGo	= DesiredBlendTime * Abs(LookAtAlphaTarget - LookAtAlpha);
	}
}

/** Interpolate TargetLocation towards DesiredTargetLocation based on TargetLocationInterpSpeed */
void USkelControlLookAt::InterpolateTargetLocation(FLOAT DeltaTime)
{
	AActor* AOwner = SkelComponent ? SkelComponent->GetOwner() : NULL;

	// If controller is not relevant, then we can directly set Target to Desired.
	if( ControlStrength * LookAtAlpha < 0.001f || !SkelComponent || !SkelComponent->bRecentlyRendered )
	{
		// if not relevant, directly set DesiredTargetLocation and let SkelControl interpolation do its thing.
		TargetLocation = DesiredTargetLocation;
		if( AOwner )
		{
			// Delta from Owner Location to Desired Location.
			FVector const WorldDeltaToDesired = DesiredTargetLocation - AOwner->Location;

			// Turn that into Actor Space.
			FRotationMatrix	const RotM(AOwner->Rotation);
			ActorSpaceLookAtTarget = FVector(WorldDeltaToDesired | RotM.GetAxis(0), WorldDeltaToDesired | RotM.GetAxis(1), WorldDeltaToDesired | RotM.GetAxis(2));
		}
		else
		{
			ActorSpaceLookAtTarget = FVector(256,0.f,0.f);
		}
		return;
	}
	
	if( AOwner )
	{
// 		debugf(TEXT("AOwner: %s, TimeSeconds: %f, DeltaTime: %f"), *AOwner->GetFName().ToString(), GWorld->GetWorldInfo()->TimeSeconds, DeltaTime);
		
		// Find our bone index... argh wish there was a better way to know
		if (ControlBoneIndex == INDEX_NONE)
		{
			UAnimTree* const Tree = Cast<UAnimTree>(SkelComponent->Animations);
			if( Tree )
			{
				for(INT i=0; i<SkelComponent->RequiredBones.Num() && ControlBoneIndex == INDEX_NONE; i++)
				{
					const INT BoneIndex = SkelComponent->RequiredBones(i);

					if( (SkelComponent->SkelControlIndex.Num() > 0) && (SkelComponent->SkelControlIndex(BoneIndex) != 255) )
					{
						const INT ControlIndex = SkelComponent->SkelControlIndex(BoneIndex);
						const USkelControlBase* Control = Tree->SkelControlLists(ControlIndex).ControlHead;
						while( Control )
						{
							// we found us... so we found the boneindex... wheee
							if( Control == this )
							{
								ControlBoneIndex = BoneIndex;
								break;
							}
							Control = Control->NextControl;
						}
					}
				}
			}
		}


		// if we can, use actual bone location, rather than Actor Location, which adds an offset from the lookat bone location.
		FVector const OwnerLocation = (ControlBoneIndex != INDEX_NONE) ? SkelComponent->GetBoneMatrix(ControlBoneIndex).GetOrigin() : AOwner->Location;

		// Delta from Owner Location to Desired Location.
		FVector const WorldDeltaToDesired = DesiredTargetLocation - OwnerLocation;
		
		// Turn that into Actor Space.
		FRotationMatrix	const RotM(AOwner->Rotation);
		FVector const ActorSpaceDeltaToDesired = FVector(WorldDeltaToDesired | RotM.GetAxis(0), WorldDeltaToDesired | RotM.GetAxis(1), WorldDeltaToDesired | RotM.GetAxis(2));

		// Separate Distance and Vector, so we can interpolate separately angle and distance.
		FLOAT const LocalDistanceToDesired = ActorSpaceDeltaToDesired.Size();
		FVector const LocalNormalToDesired = ActorSpaceDeltaToDesired.SafeNormal();
		FLOAT LocalDistanceToTarget = ActorSpaceLookAtTarget.Size();
		FVector LocalNormalToTarget = ActorSpaceLookAtTarget.SafeNormal();

// 		debugf(TEXT("\tLocalDistanceToTarget: %f, LocalDistanceToDesired: %f, TargetLocationInterpSpeed: %f"), LocalDistanceToTarget, LocalDistanceToDesired, TargetLocationInterpSpeed);
		// Interp Distance
		LocalDistanceToTarget = FInterpConstantTo(LocalDistanceToTarget, LocalDistanceToDesired, DeltaTime, TargetLocationInterpSpeed * 33.f);

		// Rotate Vectors by Angle Interpolation
		{
			FQuat DeltaQuat = FQuatFindBetween(LocalNormalToTarget, LocalNormalToDesired);

			// Decompose into an axis and angle for rotation
			FVector DeltaAxis = FVector::ZeroVector;
			FLOAT DeltaAngle = 0.f;
			DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAngle);

			// Limit the amount we can rotate.
			FLOAT const MaxRotRadians = TargetLocationInterpSpeed * (PI / 180) * 12.f * DeltaTime;
// 			debugf(TEXT("\tDeltaAngle: %f, MaxRotRadians: %f"), DeltaAngle, MaxRotRadians);
			if( Abs(DeltaAngle) > MaxRotRadians )
			{
				DeltaAngle = Clamp(DeltaAngle, -MaxRotRadians, MaxRotRadians);
				DeltaQuat = FQuat(DeltaAxis, DeltaAngle);
			}

			LocalNormalToTarget = DeltaQuat.RotateVector(LocalNormalToTarget);
		}

		// Result of Local Angle/Distance interpolation.
		ActorSpaceLookAtTarget = LocalNormalToTarget * LocalDistanceToTarget;

		// Turn that back into world space
		TargetLocation = OwnerLocation + ActorSpaceLookAtTarget.X * RotM.GetAxis(0) + ActorSpaceLookAtTarget.Y * RotM.GetAxis(1) + ActorSpaceLookAtTarget.Z * RotM.GetAxis(2);
	}
	// Fallback is world interpolation. But not great because of distances...
	else
	{
		TargetLocation = VInterpTo(TargetLocation, DesiredTargetLocation, DeltaTime, TargetLocationInterpSpeed);
		ActorSpaceLookAtTarget = FVector(256,0.f,0.f);
	}
}

/** Sets DesiredTargetLocation to a new location */
void USkelControlLookAt::SetTargetLocation(FVector NewTargetLocation)
{
	DesiredTargetLocation = NewTargetLocation;

	// Update TargetLocation and ActorSpaceLookAtTarget.
	InterpolateTargetLocation(0.f);
}

UBOOL USkelControlLookAt::CanLookAtPoint(FVector PointLoc, UBOOL bDrawDebugInfo, UBOOL bDebugUsePersistentLines, UBOOL bDebugFlushLinesFirst)
{
	// we need access to the SpaceBases array..
	if( !SkelComponent || (GWorld->GetWorldInfo()->TimeSeconds - SkelComponent->LastRenderTime) > 1.0f )
	{
		debugf(TEXT("USkelControlLookAt::CanLookAtPoint, no SkelComponent, or not rendered recently."));
		return FALSE;
	}

	const UAnimTree* Tree = Cast<UAnimTree>(SkelComponent->Animations);

	if( !Tree )
	{
		debugf(TEXT("USkelControlLookAt::CanLookAtPoint, no AnimTree."));
		return FALSE;
	}

	// Find our bone index... argh wish there was a better way to know
	if (ControlBoneIndex == INDEX_NONE)
	{
		for(INT i=0; i<SkelComponent->RequiredBones.Num() && ControlBoneIndex == INDEX_NONE; i++)
		{
			const INT BoneIndex = SkelComponent->RequiredBones(i);

			if( (SkelComponent->SkelControlIndex.Num() > 0) && (SkelComponent->SkelControlIndex(BoneIndex) != 255) )
			{
				const INT ControlIndex = SkelComponent->SkelControlIndex(BoneIndex);
				const USkelControlBase* Control = Tree->SkelControlLists(ControlIndex).ControlHead;
				while( Control )
				{
					// we found us... so we found the boneindex... wheee
					if( Control == this )
					{
						ControlBoneIndex = BoneIndex;
						break;
					}
					Control = Control->NextControl;
				}
			}
		}
	}

	if( ControlBoneIndex == INDEX_NONE )
	{
		debugf(TEXT("USkelControlLookAt::CanLookAtPoint, BoneIndex not found."));
		return FALSE;
	}

	// Original look dir, not influenced by look at controller.
	FVector OriginalLookDir		= BaseLookDir;
	FVector BonePosCompSpace	= BaseBonePos;

	// If BoneController has not been calculated recently, BaseLookDir and BaseBonePos information is not accurate.
	// So we grab it from the mesh. But it may be affected by other skel controllers...
	// So to be safe, it's better to use SetLookAtAlpha() than SetSkelControlActive(), so the controller always updates BaseLookDir and BaseBonePos.
	if( (GWorld->GetWorldInfo()->TimeSeconds - LastCalcTime) < 1.0f )
	{
		if( bLimitBasedOnRefPose )
		{
			// Calculate transform of bone in ref-pose.
			const INT ParentIndex		= SkelComponent->SkeletalMesh->RefSkeleton(ControlBoneIndex).ParentIndex;
			const FBoneAtom BoneRefPose = FBoneAtom(SkelComponent->SkeletalMesh->RefSkeleton(ControlBoneIndex).BonePos.Orientation, SkelComponent->SkeletalMesh->RefSkeleton(ControlBoneIndex).BonePos.Position) * SkelComponent->SpaceBases(ParentIndex);
			// Calculate ref-pose look dir.
			OriginalLookDir = BoneRefPose.TransformNormal(GetAxisDirVector(LookAtAxis, bInvertLookAtAxis));
			OriginalLookDir = OriginalLookDir.SafeNormal();
		}

		BonePosCompSpace	= SkelComponent->SpaceBases(ControlBoneIndex).GetOrigin();
	}

	// Get target location, in component space.
	const FBoneAtom ComponentToFrame	= SkelComponent->CalcComponentToFrameMatrix(ControlBoneIndex, BCS_WorldSpace, NAME_None);
	const FVector TargetCompSpace	= ComponentToFrame.InverseSafe().TransformFVector(PointLoc);

	// Find direction vector we want to look in - again in component space.
	FVector DesiredLookDir	= (TargetCompSpace - BaseBonePos).SafeNormal();

#if !FINAL_RELEASE
	// Draw debug information if requested
	if( bDrawDebugInfo )
	{
		AActor* Actor = SkelComponent->GetOwner();
	
		if( bDebugFlushLinesFirst )
		{
			Actor->FlushPersistentDebugLines();
		}

		Actor->DrawDebugSphere(PointLoc, 8.f, 8, 255, 000, 000, bDebugUsePersistentLines);

		// Test to make sure i got it correct!
		const FVector& TargetWorldSpace = SkelComponent->LocalToWorld.TransformFVector(TargetCompSpace);
		Actor->DrawDebugSphere(TargetWorldSpace, 4.f, 8, 000, 255, 000, bDebugUsePersistentLines);

		const FVector& OriginWorldSpace = SkelComponent->LocalToWorld.TransformFVector(BonePosCompSpace);
		Actor->DrawDebugSphere(OriginWorldSpace, 8.f, 8, 000, 255, 000, bDebugUsePersistentLines);

		const FVector& BaseLookDirWorldSpace	= SkelComponent->LocalToWorld.TransformNormal(OriginalLookDir).SafeNormal();
		Actor->DrawDebugLine(OriginWorldSpace, OriginWorldSpace + BaseLookDirWorldSpace * 25.f, 000, 255, 000, bDebugUsePersistentLines);

		const FVector& DesiredLookDirWorldSpace	= SkelComponent->LocalToWorld.TransformNormal(DesiredLookDir).SafeNormal();
		Actor->DrawDebugLine(OriginWorldSpace, OriginWorldSpace + DesiredLookDirWorldSpace * 25.f, 255, 000, 000, bDebugUsePersistentLines);

		// Original Look Dir
		//Actor->DrawDebugLine(OriginWorldSpace, BaseLookDirWorldSpace*, 255, 255, 0, bDebugUsePersistentLines);

		const FLOAT MaxAngleRadians = MaxAngle * ((FLOAT)PI/180.f);

		Actor->DrawDebugCone(OriginWorldSpace, BaseLookDirWorldSpace, 64.f, MaxAngleRadians, MaxAngleRadians, 16, FColor(0, 0, 255), bDebugUsePersistentLines);
	}
#endif // #if !FINAL_RELEASE


	return !ApplyLookDirectionLimits(DesiredLookDir, BaseLookDir, ControlBoneIndex, SkelComponent);
}

void USkelControlLookAt::TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp)
{
	Super::TickSkelControl(DeltaSeconds, SkelComp);

	// Interpolate LookAtAlpha
	const FLOAT BlendDelta = LookAtAlphaTarget - LookAtAlpha;
	if( LookAtAlphaBlendTimeToGo > KINDA_SMALL_NUMBER || Abs(BlendDelta) > KINDA_SMALL_NUMBER )
	{
		if( LookAtAlphaBlendTimeToGo <= DeltaSeconds || Abs(BlendDelta) <= KINDA_SMALL_NUMBER )
		{
			LookAtAlpha					= LookAtAlphaTarget;
			LookAtAlphaBlendTimeToGo	= 0.f;
		}
		else
		{
			LookAtAlpha					+= (BlendDelta / LookAtAlphaBlendTimeToGo) * DeltaSeconds;
			LookAtAlphaBlendTimeToGo	-= DeltaSeconds;
		}
	}
	else
	{
		LookAtAlpha					= LookAtAlphaTarget;
		LookAtAlphaBlendTimeToGo	= 0.f;
	}
}


//
// USkelControlLookAt::GetAffectedBones
//
void USkelControlLookAt::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	// Bone is not allowed to rotate, early out
	if( !bAllowRotationX && !bAllowRotationY && !bAllowRotationZ )
	{
		return;
	}


	check(OutBoneIndices.Num() == 0);
	OutBoneIndices.AddItem(BoneIndex);
}

/** 
 * ApplyLookDirectionLimits.  Factored out to allow overriding of limit-enforcing behavior 
 * Returns TRUE if DesiredLookDir was beyond MaxAngle limit.
 */
UBOOL USkelControlLookAt::ApplyLookDirectionLimits(FVector& DesiredLookDir, const FVector &CurrentLookDir, INT BoneIndex, USkeletalMeshComponent* SkelComp)
{
	UBOOL bResult = FALSE;

	// If we have a dead-zone, update the DesiredLookDir.
	FLOAT DeadZoneRadians = 0.f;
	if( DeadZoneAngle > 0.f && !CurrentLookDir.IsNearlyZero() && !DesiredLookDir.IsNearlyZero() ) 
	{
		FVector ErrorAxis;
		FLOAT	ErrorAngle;
		FindAxisAndAngle(CurrentLookDir, DesiredLookDir, ErrorAxis, ErrorAngle);

		DeadZoneRadians = DeadZoneAngle * ((FLOAT)PI/180.f);
		FLOAT NewAngle = ::Max( ErrorAngle - DeadZoneRadians, 0.f );
		FQuat UpdateQuat(ErrorAxis, NewAngle);

		DesiredLookDir = UpdateQuat.RotateVector(CurrentLookDir);
	}

	if( bEnableLimit )
	{
		if( bLimitBasedOnRefPose )
		{
			// Calculate transform of bone in ref-pose.
			const INT ParentIndex		= SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
			// want base ref pose in mesh space. we already have this inverse, so inverse back
			const FBoneAtom BoneRefPose = SkelComp->SkeletalMesh->RefBasesInvMatrix(BoneIndex).Inverse();
			// Calculate ref-pose look dir.
			LimitLookDir = BoneRefPose.TransformNormal(GetAxisDirVector(LookAtAxis, bInvertLookAtAxis));
			LimitLookDir = LimitLookDir.SafeNormal();
		}
		else
		{
			LimitLookDir = CurrentLookDir;
		}

		if( LimitLookDir.IsNearlyZero() || DesiredLookDir.IsNearlyZero() )
		{
			return bResult;
		}

		// Turn into axis and angle.
		FVector ErrorAxis;
		FLOAT	ErrorAngle;
		FindAxisAndAngle(LimitLookDir, DesiredLookDir, ErrorAxis, ErrorAngle);

		// If too great - update.
		const FLOAT MaxAngleRadians = MaxAngle * ((FLOAT)PI/180.f);
		const FLOAT OuterMaxAngleRadians = OuterMaxAngle * ((FLOAT)PI/180.f);

		if( ErrorAngle > MaxAngleRadians )
		{
			// Otherwise, clamp the DesiredLookDir within MaxAngleRadians
			FQuat LookQuat(ErrorAxis, MaxAngleRadians);
			DesiredLookDir = LookQuat.RotateVector(LimitLookDir);

		}

		if( ErrorAngle > OuterMaxAngleRadians )
		{
			bResult = TRUE;

			// Going beyond limit, so cancel out controller
			if( bDisableBeyondLimit )
			{
				if( LookAtAlphaTarget > ZERO_ANIMWEIGHT_THRESH )
				{
					if( bNotifyBeyondLimit && SkelComp->GetOwner() != NULL )
					{
						SkelComp->GetOwner()->eventNotifySkelControlBeyondLimit( this );
					}

					SetLookAtAlpha(0.f, BlendOutTime);
				}
			}
		}
		// Otherwise, if we are within the constraint and dead buffer
		else if( bDisableBeyondLimit && ErrorAngle <= (OuterMaxAngleRadians - DeadZoneRadians) )
		{
			if( LookAtAlphaTarget < 1.f - ZERO_ANIMWEIGHT_THRESH )
			{
				SetLookAtAlpha(1.f, BlendInTime);
			}
		}
	}

	return bResult;
}

//
// USkelControlLookAt::CalculateNewBoneTransforms
//
void USkelControlLookAt::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// Bone transform in mesh space
	FBoneAtom NewBoneTM = SkelComp->SpaceBases(BoneIndex);

	// Find the base look direction vector.
	BaseLookDir		= SkelComp->SpaceBases(BoneIndex).TransformNormal( GetAxisDirVector(LookAtAxis, bInvertLookAtAxis) ).SafeNormal();
	// Get bone position (will be in component space).
	BaseBonePos		= SkelComp->SpaceBases(BoneIndex).GetOrigin();
	// Keep track of when BaseLookDir was last updated.
	LastCalcTime	= GWorld->GetWorldInfo()->TimeSeconds;

	// Get target location, in component space.
	const FBoneAtom ComponentToFrame	= SkelComp->CalcComponentToFrameMatrix(BoneIndex, TargetLocationSpace, TargetSpaceBoneName);
	const FVector TargetCompSpace	= ComponentToFrame.InverseSafe().TransformFVector(TargetLocation);

	// Find direction vector we want to look in - again in component space.
	FVector DesiredLookDir	= (TargetCompSpace - BaseBonePos).SafeNormal();

	// Use limits to update DesiredLookDir if desired.
	// We also use these to play with the 'LookAtAlpha'
	ApplyLookDirectionLimits(DesiredLookDir, BaseLookDir, BoneIndex, SkelComp);

//	CanLookAtPoint(DesiredTargetLocation, TRUE, TRUE, TRUE);

	// Below we not touching LookAtAlpha anymore. So if it's zero, there's no point going further.
	if( GetControlAlpha() < ZERO_ANIMWEIGHT_THRESH )
	{
		return;
	} 

	// If we are not defining an 'up' axis as well, we calculate the minimum rotation needed to point the axis in 
	// the right direction. This is nice because we still have some animation acting on the roll of the bone.
	if( !bDefineUpAxis )
	{
		// Calculate a quaternion that gets us from our current rotation to the desired one.
		const FQuat DeltaLookQuat = FQuatFindBetween(BaseLookDir, DesiredLookDir);
		const FBoneAtom DeltaLookTM(DeltaLookQuat, FVector(0.f) );

		NewBoneTM.SetOrigin( FVector(0.f) );
		NewBoneTM = NewBoneTM * DeltaLookTM;
		NewBoneTM.SetOrigin( BaseBonePos );
	}
	// If we are defining an 'up' axis as well, we can calculate the entire bone transform explicitly.
	else
	{
		if( UpAxis == LookAtAxis )
		{
			debugf( TEXT("USkelControlLookAt (%s): UpAxis and LookAtAxis cannot be the same."), *ControlName.ToString() );
		}

		// Invert look at direction if desired.
		if( bInvertLookAtAxis )
		{
			DesiredLookDir *= -1.f;
		}

		// Calculate 'world up' (+Z) in the component ref frame.
		const FVector UpCompSpace = SkelComp->LocalToWorld.InverseTransformNormal( FVector(0,0,1) );

		// Then calculate our desired up vector using 2 cross products. Probably a more elegant way to do this...
		FVector TempRight = DesiredLookDir ^ UpCompSpace;

		// Handle case when DesiredLookDir ~= (0,0,1) ie looking straight up in this mode.
		if( TempRight.IsNearlyZero() )
		{
			TempRight = FVector(0,1,0);
		}
		else
		{
			TempRight.Normalize();
		}

		FVector DesiredUpDir = TempRight ^ DesiredLookDir;

		// Reverse direction if desired.
		if( bInvertUpAxis )
		{
			DesiredUpDir *= -1.f;
		}

		// Do some sanity checking on vectors before using them to generate vectors.
		if((DesiredLookDir != DesiredUpDir) && (DesiredLookDir | DesiredUpDir) < 0.1f)
		{
			// Then build the bone matrix. 
			NewBoneTM = BuildMatrixFromVectors(LookAtAxis, DesiredLookDir, UpAxis, DesiredUpDir);
			NewBoneTM.SetOrigin( BaseBonePos );
		}
	}

	// See if we should do some per rotation axis filtering
	if( bEnableLimit || !bAllowRotationX || !bAllowRotationY || !bAllowRotationZ )
	{
		FBoneAtom ComponentToAllowFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, AllowRotationSpace, AllowRotationOtherBoneName);
		ComponentToAllowFrame.RemoveScaling();
		FQuat CompToFrameQuat(ComponentToAllowFrame.GetRotation());

		FBoneAtom CurrentBoneTransformNoScale = SkelComp->SpaceBases(BoneIndex);
		CurrentBoneTransformNoScale.RemoveScaling();
		FQuat CurrentQuatRot = CompToFrameQuat * CurrentBoneTransformNoScale.GetRotation();
		FQuat DesiredQuatRot = CompToFrameQuat * NewBoneTM.GetRotation();

		FQuat DeltaQuat = DesiredQuatRot * (-CurrentQuatRot);

		FRotator DeltaRot = DeltaQuat.Rotator();

		// Filter out any of the Roll (X), Pitch (Y), Yaw (Z) in bone relative space
		if( !bAllowRotationX )
		{
			DeltaRot.Roll	= 0;
		}
		else
		{
			DeltaRot.Roll = Clamp(DeltaRot.Roll, (INT)(RotationAngleRangeX.X / 360.0 * 65536), (INT)(RotationAngleRangeX.Y / 360.0 * 65536));
		}
		if( !bAllowRotationY )
		{
			DeltaRot.Pitch	= 0;
		}
		else
		{
			DeltaRot.Pitch = Clamp(DeltaRot.Pitch, (INT)(RotationAngleRangeY.X / 360.0 * 65536), (INT)(RotationAngleRangeY.Y / 360.0 * 65536));
		}
		if( !bAllowRotationZ )
		{
			DeltaRot.Yaw	= 0;
		}
		else
		{
			DeltaRot.Yaw = Clamp(DeltaRot.Yaw, (INT)(RotationAngleRangeZ.X / 360.0 * 65536), (INT)(RotationAngleRangeZ.Y / 360.0 * 65536));
		}

		// Find new desired rotation
		DesiredQuatRot = DeltaRot.Quaternion() * CurrentQuatRot;

		FQuat NewQuat = (-CompToFrameQuat) * DesiredQuatRot;
		NewQuat.Normalize();

		// Turn into new bone position.
		NewBoneTM = FBoneAtom(NewQuat, BaseBonePos);
	}

	OutBoneTransforms.AddItem(NewBoneTM);
}


FBoneAtom USkelControlLookAt::GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex)
{
	check(WidgetIndex == 0);
	FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, TargetLocationSpace, TargetSpaceBoneName);
	FBoneAtom LocalToWorld = SkelComp->LocalToWorldBoneAtom;
	FVector WorldLookTarget =  LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(TargetLocation) );

	FBoneAtom FrameToComponent = ComponentToFrame.InverseSafe() * LocalToWorld;
	FrameToComponent.SetOrigin(WorldLookTarget);

	return FrameToComponent;
}

void USkelControlLookAt::HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec)
{
	check(WidgetIndex == 0);
	TargetLocation += DragVec;
}

void USkelControlLookAt::DrawSkelControl3D(const FSceneView* View, FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, INT BoneIndex)
{
	const FBoneAtom ComponentToFrame	= SkelComp->CalcComponentToFrameMatrix(BoneIndex, TargetLocationSpace, TargetSpaceBoneName);
	const FVector WorldLookTarget	= SkelComp->LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(TargetLocation) );
	const FMatrix TargetTM			= FTranslationMatrix(WorldLookTarget);
	DrawWireDiamond(PDI,TargetTM, CONTROL_DIAMOND_SIZE, FColor(128,255,255), SDPG_Foreground);

	if( bEnableLimit && bShowLimit && SkelComp->SkeletalMesh )
	{
		// Calculate transform for cone.
		FVector YAxis, ZAxis;
		LimitLookDir.FindBestAxisVectors(YAxis, ZAxis);
		const FVector	ConeOrigin		= SkelComp->SpaceBases(BoneIndex).GetOrigin();
		const FLOAT		MaxAngleRadians = MaxAngle * ((FLOAT)PI/180.f);
		const FMatrix	ConeToWorld		= FScaleMatrix(FVector(30.f)) * FMatrix(LimitLookDir, YAxis, ZAxis, ConeOrigin) * SkelComp->LocalToWorld;

		UMaterialInterface* LimitMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_None, NULL);

		DrawCone(PDI,ConeToWorld, MaxAngleRadians, MaxAngleRadians, 40, TRUE, FColor(64,255,64), LimitMaterial->GetRenderProxy(FALSE), SDPG_World);
	}
}

/** Set lookat control to point at target location */
void USkelControlLookAt::SetControlTargetLocation(const FVector& InTargetLocation)
{
	TargetLocation = InTargetLocation;
}


/*-----------------------------------------------------------------------------
	USkelControlSpline
-----------------------------------------------------------------------------*/

//
// USkelControlSpline::GetAffectedBones
//
void USkelControlSpline::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);

	if(SplineLength < 2)
	{
		return;
	}

	// The incoming BoneIndex is the 'end' of the spline chain. We need to find the 'start' by walking SplineLength bones up hierarchy.
	// Fail if we walk past the root bone.

	INT WalkBoneIndex = BoneIndex;
	OutBoneIndices.Add(SplineLength); // Allocate output array of bone indices.
	OutBoneIndices(SplineLength-1) = BoneIndex;

	for(INT i=1; i<SplineLength; i++)
	{
		INT OutTransformIndex = SplineLength-(i+1);

		// If we are at the root but still need to move up, chain is too long, so clear the OutBoneIndices array and give up here.
		if(WalkBoneIndex == 0)
		{
			debugf( TEXT("USkelControlSpline : Spling passes root bone of skeleton.") );
			OutBoneIndices.Empty();
			return;
		}
		else
		{
			// Get parent bone.
			WalkBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(WalkBoneIndex).ParentIndex;

			// Insert indices at the start of array, so that parents are before children in the array.
			OutBoneIndices(OutTransformIndex) = WalkBoneIndex;
		}
	}
}

//
// USkelControlSpline::CalculateNewBoneTransforms
//
void USkelControlSpline::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// Because we do not modify the start or end bone, with a chain less than 2 - nothing is changed!
	if(SplineLength < 2)
	{
		return;
	}

	// We should have checked this is a valid chain in GetAffectedBones, so can assume its ok here.

	UBOOL bPastRoot = false;
	INT StartBoneIndex = BoneIndex;
	for(INT i=0; i<SplineLength; i++)
	{
		if (StartBoneIndex == 0)
		{
			// if this happens, GetAffectedBone clears out, but somehow between update(when you just connect), this still comes through
			// but to avoid crash, return here
			return;
		}
		StartBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(StartBoneIndex).ParentIndex;
	}

	FVector StartBonePos = SkelComp->SpaceBases(StartBoneIndex).GetOrigin();
	FVector StartAxisDir = GetAxisDirVector(SplineBoneAxis, bInvertSplineBoneAxis);
	FVector StartBoneTangent = StartSplineTension * SkelComp->SpaceBases(StartBoneIndex).TransformNormal(StartAxisDir);

	FVector EndBonePos = SkelComp->SpaceBases(BoneIndex).GetOrigin();
	FVector EndAxisDir = GetAxisDirVector(SplineBoneAxis, bInvertSplineBoneAxis);
	FVector EndBoneTangent = EndSplineTension * SkelComp->SpaceBases(BoneIndex).TransformNormal(EndAxisDir);

	// Allocate array for output transforms. Final bone transform is not modified by this controlled, so can just copy that straight.
	OutBoneTransforms.Add(SplineLength);
	OutBoneTransforms( SplineLength-1 ) = SkelComp->SpaceBases(BoneIndex);

	INT ModifyBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
	for(INT i=1; i<SplineLength; i++)
	{
		INT OutTransformIndex = SplineLength-(i+1);
		FLOAT Alpha = 1.f - ((FLOAT)i/(FLOAT)SplineLength);

		// Calculate the position for this point on the curve.
		FVector NewBonePos = CubicInterp(StartBonePos, StartBoneTangent, EndBonePos, EndBoneTangent, Alpha);

		// Option that points bones in spline along the spline.
		if(BoneRotMode == SCR_AlongSpline)
		{
			FVector NewBoneDir = CubicInterpDerivative(StartBonePos, StartBoneTangent, EndBonePos, EndBoneTangent, Alpha);
			UBOOL bNonZero = NewBoneDir.Normalize();

			// Only try and correct direction if we get a non-zero tangent.
			if(bNonZero)
			{
				// Calculate the direction that bone is currently pointing.
				FVector CurrentBoneDir = SkelComp->SpaceBases(ModifyBoneIndex).TransformNormal( GetAxisDirVector(SplineBoneAxis, bInvertSplineBoneAxis) );
				CurrentBoneDir = CurrentBoneDir.SafeNormal();

				// Calculate a quaternion that gets us from our current rotation to the desired one.
				FQuat DeltaLookQuat = FQuatFindBetween(CurrentBoneDir, NewBoneDir);
				FBoneAtom DeltaLookTM( DeltaLookQuat, FVector(0.f) );
				// Apply to the current bone transform.
				OutBoneTransforms(OutTransformIndex) = SkelComp->SpaceBases(ModifyBoneIndex);
				OutBoneTransforms(OutTransformIndex).SetOrigin( FVector(0.f) );
				OutBoneTransforms(OutTransformIndex) = OutBoneTransforms(OutTransformIndex) * DeltaLookTM;
			}
		}
		// Option that interpolates the rotation of the bone between the start and end rotation.
		else if(SCR_Interpolate)
		{
			FQuat StartBoneQuat = SkelComp->SpaceBases(StartBoneIndex).GetRotation();
			FQuat EndBoneQuat = SkelComp->SpaceBases(BoneIndex).GetRotation();

			FQuat NewBoneQuat = SlerpQuat( StartBoneQuat, EndBoneQuat, Alpha );

			OutBoneTransforms(OutTransformIndex) = FBoneAtom( NewBoneQuat, FVector(0.f) );
		}

		OutBoneTransforms(OutTransformIndex).SetOrigin(NewBonePos);

		ModifyBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(ModifyBoneIndex).ParentIndex;
	}
}

/*-----------------------------------------------------------------------------
	USkelControlLimb
-----------------------------------------------------------------------------*/

//
// USkelControlLimb::GetAffectedBones
//
void USkelControlLimb::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);

	// Get indices of the lower and upper limb bones.
	UBOOL bInvalidLimb = FALSE;
	if( BoneIndex == 0 )
	{
		bInvalidLimb = TRUE;
	}

	const INT LowerLimbIndex = SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
	if( LowerLimbIndex == 0 )
	{
		bInvalidLimb = TRUE;
	}

	const INT UpperLimbIndex = SkelComp->SkeletalMesh->RefSkeleton(LowerLimbIndex).ParentIndex;

	// If we walked past the root, this controlled is invalid, so return no affected bones.
	if( bInvalidLimb )
	{
		debugf( TEXT("USkelControlLimb : Cannot find 2 bones above controlled bone. Too close to root.") );
		return;
	}
	else
	{
		OutBoneIndices.Add(3);
		OutBoneIndices(0) = UpperLimbIndex;
		OutBoneIndices(1) = LowerLimbIndex;
		OutBoneIndices(2) = BoneIndex;
	}
}


//
// USkelControlSpline::CalculateNewBoneTransforms
//
void USkelControlLimb::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);
	OutBoneTransforms.Add(3); // Allocate space for bone transforms.

	// First get indices of the lower and upper limb bones. We should have checked this in GetAffectedBones so can assume its ok now.

	check(BoneIndex != 0);
	const INT LowerLimbIndex = SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
	check(LowerLimbIndex != 0);
	const INT UpperLimbIndex = SkelComp->SkeletalMesh->RefSkeleton(LowerLimbIndex).ParentIndex;

	// If we have enough bones to work on (ie at least 2 bones between controlled hand bone and the root) continue.

	// Get current position of root of limb.
	// All position are in Component space.
	const FVector RootPos			= SkelComp->SpaceBases(UpperLimbIndex).GetOrigin();
	const FVector InitialJointPos	= SkelComp->SpaceBases(LowerLimbIndex).GetOrigin();
	const FVector InitialEndPos		= SkelComp->SpaceBases(BoneIndex).GetOrigin();

	// If desired, calc the initial relative transform between the end effector bone and its parent.
	FBoneAtom EffectorRelTM = FBoneAtom::Identity;
	if( bMaintainEffectorRelRot ) 
	{
		EffectorRelTM = SkelComp->SpaceBases(BoneIndex) * SkelComp->SpaceBases(LowerLimbIndex).InverseSafe();
	}

	FBoneAtom DesiredComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, EffectorLocationSpace, EffectorSpaceBoneName);
	// Get desired position of effector.
	FVector DesiredPos = DesiredComponentToFrame.InverseSafe().TransformFVector(EffectorLocation);

	FVector DesiredDelta = DesiredPos - RootPos;
	FLOAT DesiredLength = DesiredDelta.Size();

	// Check to handle case where DesiredPos is the same as RootPos.
	FVector			DesiredDir;
	if( DesiredLength < (FLOAT)KINDA_SMALL_NUMBER )
	{
		DesiredLength	= (FLOAT)KINDA_SMALL_NUMBER;
		DesiredDir		= FVector(1,0,0);
	}
	else
	{
		DesiredDir		= DesiredDelta/DesiredLength;
	}

	// Fix up desired position to take into account bone scaling.
	// Find BoneScaling currently used. We don't support non uniform scaling.
	// TODO: This is overhead for BoneAtom. Fix this with just getting scale. 
	INT	LocalBoneIdx = BoneIndex;
	FLOAT BoneScaling = SkelComp->SpaceBases(UpperLimbIndex).GetMaximumAxisScale();

	if( BoneScaling > KINDA_SMALL_NUMBER && Abs(1.f - BoneScaling) > KINDA_SMALL_NUMBER )
	{
		DesiredLength	= DesiredLength / BoneScaling;
		DesiredDelta	= DesiredDir * DesiredLength;
		DesiredPos		= RootPos + DesiredDelta;
	}

	// Get joint target (used for defining plane that joint should be in).
	FBoneAtom JointTargetComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, JointTargetLocationSpace, JointTargetSpaceBoneName);
	FVector	JointTargetPos = JointTargetComponentToFrame.InverseSafe().TransformFVector(JointTargetLocation);

	FVector JointTargetDelta = JointTargetPos - RootPos;
	FLOAT JointTargetLength = JointTargetDelta.Size();
	FVector JointPlaneNormal, JointBendDir;

	// Same check as above, to cover case when JointTarget position is the same as RootPos.
	if( JointTargetLength < (FLOAT)KINDA_SMALL_NUMBER )
	{
		JointBendDir		= FVector(0,1,0);
		JointPlaneNormal	= FVector(0,0,1);
	}
	else
	{
		JointPlaneNormal = DesiredDir ^ JointTargetDelta;

		// If we are trying to point the limb in the same direction that we are supposed to displace the joint in, 
		// we have to just pick 2 random vector perp to DesiredDir and each other.
		if( JointPlaneNormal.Size() < (FLOAT)KINDA_SMALL_NUMBER )
		{
			DesiredDir.FindBestAxisVectors(JointPlaneNormal, JointBendDir);
		}
		else
		{
			JointPlaneNormal.Normalize();

			// Find the final member of the reference frame by removing any component of JointTargetDelta along DesiredDir.
			// This should never leave a zero vector, because we've checked DesiredDir and JointTargetDelta are not parallel.
			JointBendDir = JointTargetDelta - ((JointTargetDelta | DesiredDir) * DesiredDir);
			JointBendDir.Normalize();
		}
	}

	// Find lengths of upper and lower limb in the ref skeleton.
	// Use actual sizes instead of ref skeleton, so we take into account translation and scaling from other bone controllers.
	FLOAT LowerLimbLength = SkelComp->LocalAtoms(BoneIndex).GetTranslation().Size();			// SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position.Size();
	FLOAT UpperLimbLength = SkelComp->LocalAtoms(LowerLimbIndex).GetTranslation().Size();		// SkelComp->SkeletalMesh->RefSkeleton(LowerLimbIndex).BonePos.Position.Size();
	FLOAT MaxLimbLength	= LowerLimbLength + UpperLimbLength;

	if( bAllowStretching )
	{
		const FLOAT ScaleRange = StretchLimits.Y - StretchLimits.X;
		if( ScaleRange > KINDA_SMALL_NUMBER && MaxLimbLength > KINDA_SMALL_NUMBER )
		{
			const FLOAT ReachRatio = DesiredLength / MaxLimbLength;
			const FLOAT ScalingFactor = (StretchLimits.Y - 1.f) * Clamp<FLOAT>((ReachRatio - StretchLimits.X) / ScaleRange, 0.f, 1.f);
			if( ScalingFactor > KINDA_SMALL_NUMBER )
			{
				LowerLimbLength *= (1.f + ScalingFactor);
				UpperLimbLength *= (1.f + ScalingFactor);
				MaxLimbLength	*= (1.f + ScalingFactor);

				// Scale Roll Bone if needed.
				if( StretchRollBoneName != NAME_None )
				{	
					INT RollBoneIndex = SkelComp->MatchRefBone(StretchRollBoneName);
					if( RollBoneIndex != INDEX_NONE )
					{
						SkelComp->LocalAtoms(RollBoneIndex).SetTranslation(SkelComp->LocalAtoms(RollBoneIndex).GetTranslation() * (1.f + ScalingFactor * GetControlAlpha()));
					}
				}
			}
		}
	}

	FVector OutEndPos	= DesiredPos;
	FVector OutJointPos = InitialJointPos;

	// If we are trying to reach a goal beyond the length of the limb, clamp it to something solvable and extend limb fully.
	if( DesiredLength > MaxLimbLength )
	{
		OutEndPos	= RootPos + (MaxLimbLength * DesiredDir);
		OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
	}
	else
	{
		// So we have a triangle we know the side lengths of. We can work out the angle between DesiredDir and the direction of the upper limb
		// using the sin rule:
		const FLOAT TwoAB = 2.f * UpperLimbLength * DesiredLength;

		const FLOAT CosAngle = (TwoAB != 0.f) ? ((UpperLimbLength*UpperLimbLength) + (DesiredLength*DesiredLength) - (LowerLimbLength*LowerLimbLength)) / TwoAB : 0.f;

		// If CosAngle is less than 0, the upper arm actually points the opposite way to DesiredDir, so we handle that.
		const UBOOL bReverseUpperBone = (CosAngle < 0.f);

		// If CosAngle is greater than 1.f, the triangle could not be made - we cannot reach the target.
		// We just have the two limbs double back on themselves, and EndPos will not equal the desired EffectorLocation.
		if( CosAngle > 1.f || CosAngle < -1.f )
		{
			// Because we want the effector to be a positive distance down DesiredDir, we go back by the smaller section.
			if( UpperLimbLength > LowerLimbLength )
			{
				OutJointPos = RootPos + (UpperLimbLength * DesiredDir);
				OutEndPos	= OutJointPos - (LowerLimbLength * DesiredDir);
			}
			else
			{
				OutJointPos = RootPos - (UpperLimbLength * DesiredDir);
				OutEndPos	= OutJointPos + (LowerLimbLength * DesiredDir);
			}
		}
		else
		{
			// Angle between upper limb and DesiredDir
			const FLOAT Angle = appAcos(CosAngle);

			// Now we calculate the distance of the joint from the root -> effector line.
			// This forms a right-angle triangle, with the upper limb as the hypotenuse.
			const FLOAT JointLineDist = UpperLimbLength * appSin(Angle);

			// And the final side of that triangle - distance along DesiredDir of perpendicular.
			// ProjJointDistSqr can't be neg, because JointLineDist must be <= UpperLimbLength because appSin(Angle) is <= 1.
			const FLOAT ProjJointDistSqr	= (UpperLimbLength*UpperLimbLength) - (JointLineDist*JointLineDist);
			// although this shouldn't be ever negative, sometimes Xbox release produces -0.f, causing ProjJointDist to be NaN
			// so now I branch it. 						
			FLOAT		ProjJointDist = (ProjJointDistSqr>0.f)? appSqrt(ProjJointDistSqr) : 0.f;
			if( bReverseUpperBone )
			{
				ProjJointDist *= -1.f;
			}

			// So now we can work out where to put the joint!
			OutJointPos = RootPos + (ProjJointDist * DesiredDir) + (JointLineDist * JointBendDir);
		}
	}

	// If we have an offset on the Elbow bone, apply it now.
	if( !JointOffset.IsZero() )
	{
		// Get joint offset in component space
		FBoneAtom JointOffsetComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, JointOffsetSpace, JointOffsetBoneName);
		FVector	LocalJointOffset = JointOffsetComponentToFrame.InverseSafe().TransformNormal(JointOffset);
		// offset our joint location based on that
		OutJointPos += LocalJointOffset;

		// Reupdate JointPlaneNormal if we have to
		if( !bRotateJoint )
		{
			JointTargetDelta = OutJointPos - RootPos;
			JointTargetLength = JointTargetDelta.Size();

			// Same check as above, to cover case when JointTarget position is the same as RootPos.
			if( JointTargetLength < (FLOAT)KINDA_SMALL_NUMBER )
			{
				JointBendDir		= FVector(0,1,0);
				JointPlaneNormal	= FVector(0,0,1);
			}
			else
			{
				JointPlaneNormal = DesiredDir ^ JointTargetDelta;

				// If we are trying to point the limb in the same direction that we are supposed to displace the joint in, 
				// we have to just pick 2 random vector perp to DesiredDir and each other.
				if( JointPlaneNormal.Size() < (FLOAT)KINDA_SMALL_NUMBER )
				{
					DesiredDir.FindBestAxisVectors(JointPlaneNormal, JointBendDir);
				}
				else
				{
					JointPlaneNormal.Normalize();

					// Find the final member of the reference frame by removing any component of JointTargetDelta along DesiredDir.
					// This should never leave a zero vector, because we've checked DesiredDir and JointTargetDelta are not parallel.
					JointBendDir = JointTargetDelta - ((JointTargetDelta | DesiredDir) * DesiredDir);
					JointBendDir.Normalize();
				}
			}
		}

		// Scale Roll Bone if needed.
		if( StretchRollBoneName != NAME_None )
		{	
			INT RollBoneIndex = SkelComp->MatchRefBone(StretchRollBoneName);
			if( RollBoneIndex != INDEX_NONE )
			{
				FLOAT const ScaleValue = (OutJointPos - OutEndPos).Size() / LowerLimbLength;
				SkelComp->LocalAtoms(RollBoneIndex).SetTranslation(SkelComp->LocalAtoms(RollBoneIndex).GetTranslation() * (1.f + (ScaleValue - 1.f) * GetControlAlpha()));
			}
		}
	}

	// Experiment to not create a matrix from scratch but just rotate the existing joint
	if( bRotateJoint )
	{
		// Update transform for upper bone.
		{
			OutBoneTransforms(0) = SkelComp->SpaceBases(UpperLimbIndex);

			// Get difference in direction for old and new joint orientations
			FVector const OldDir = (InitialJointPos - RootPos).SafeNormal();
			FVector const NewDir = (OutJointPos - RootPos).SafeNormal();
			// That was done in Component space, so turn that into local space.
			// Find Delta Rotation take takes us from Old to New dir
			FQuat const DeltaRotation = FQuatFindBetween(OldDir, NewDir);
			// Rotate our Joint quaternion by this delta rotation
			OutBoneTransforms(0).SetRotation( DeltaRotation * OutBoneTransforms(0).GetRotation() );
			// And put joint where it should be.
			OutBoneTransforms(0).SetTranslation( RootPos );
		}

		// Update transform for lower bone.
		{
			OutBoneTransforms(1) = SkelComp->SpaceBases(LowerLimbIndex);

			// Get difference in direction for old and new joint orientations
			FVector const OldDir = (InitialEndPos - InitialJointPos).SafeNormal();
			FVector const NewDir = (OutEndPos - OutJointPos).SafeNormal();
			// That was done in Component space, so turn that into local space.
			FVector const OldDirLocal = OutBoneTransforms(1).InverseTransformNormal(OldDir);
			FVector const NewDirLocal = OutBoneTransforms(1).InverseTransformNormal(NewDir);
			// Find Delta Rotation take takes us from Old to New dir
			FQuat const DeltaRotation = FQuatFindBetween(OldDir, NewDir);
			// Rotate our Joint quaternion by this delta rotation
			OutBoneTransforms(1).SetRotation( DeltaRotation * OutBoneTransforms(1).GetRotation() );
			// And put joint where it should be.
			OutBoneTransforms(1).SetTranslation( OutJointPos );
		}
	}
	else
	{
		// Update transform for upper bone.
		FVector GraphicJointDir = JointPlaneNormal;
		if( bInvertJointAxis )
		{
			GraphicJointDir *= -1.f;
		}

		FVector UpperLimbDir = (OutJointPos - RootPos).SafeNormal();
		if( bInvertBoneAxis )
		{
			UpperLimbDir *= -1.f;
		}

		// Do some sanity checking, then use vectors to build upper limb matrix
		if( !UpperLimbDir.IsNearlyZero() && (UpperLimbDir != GraphicJointDir) && (UpperLimbDir | GraphicJointDir) < 0.1f )
		{
			FMatrix UpperLimbTM = BuildMatrixFromVectors(BoneAxis, UpperLimbDir, JointAxis, GraphicJointDir);
			UpperLimbTM.SetOrigin(RootPos);
			OutBoneTransforms(0).SetMatrix(UpperLimbTM);
		}
		else
		{
			OutBoneTransforms(0) = SkelComp->SpaceBases(UpperLimbIndex);
		}

		// Update transform for lower bone.
		FVector LowerLimbDir = (OutEndPos - OutJointPos).SafeNormal();
		if( bInvertBoneAxis )
		{
			LowerLimbDir *= -1.f;
		}

		// Do some sanity checking, then use vectors to build lower limb matrix
		if( !LowerLimbDir.IsNearlyZero() &&
			(LowerLimbDir != GraphicJointDir) && (LowerLimbDir | GraphicJointDir) < 0.1f )
		{
			FMatrix LowerLimbTM = BuildMatrixFromVectors(BoneAxis, LowerLimbDir, JointAxis, GraphicJointDir);
			LowerLimbTM.SetOrigin(OutJointPos);
			OutBoneTransforms(1).SetMatrix(LowerLimbTM);
		}
		else
		{
			OutBoneTransforms(1) = SkelComp->SpaceBases(LowerLimbIndex);
		}
	}

#if !FINAL_RELEASE && !SHIPPING_PC_GAME
	check(!OutBoneTransforms(0).ContainsNaN());
	check(!OutBoneTransforms(1).ContainsNaN());
	check(!SkelComp->SpaceBases(BoneIndex).ContainsNaN());
	check(OutBoneTransforms(0).IsRotationNormalized());
	check(OutBoneTransforms(1).IsRotationNormalized());
#endif

	// Update transform for end bone.
	if( bTakeRotationFromEffectorSpace )
	{
		OutBoneTransforms(2) = SkelComp->SpaceBases(BoneIndex);
		DesiredComponentToFrame.RemoveScaling();
		FBoneAtom FrameToComponent = DesiredComponentToFrame.InverseSafe();
		CopyRotationPart(OutBoneTransforms(2), FrameToComponent);
#if !FINAL_RELEASE && !SHIPPING_PC_GAME
		check(!DesiredComponentToFrame.ContainsNaN());
		check(!FrameToComponent.ContainsNaN());
		check(!OutBoneTransforms(2).ContainsNaN());
#endif
	}
	else if( bMaintainEffectorRelRot )
	{
		OutBoneTransforms(2) = EffectorRelTM * OutBoneTransforms(1);
#if !FINAL_RELEASE && !SHIPPING_PC_GAME
		check(!EffectorRelTM.ContainsNaN());
		check(!OutBoneTransforms(2).ContainsNaN());
#endif
	}
	else
	{
		OutBoneTransforms(2) = SkelComp->SpaceBases(BoneIndex);
#if !FINAL_RELEASE && !SHIPPING_PC_GAME
		check(!OutBoneTransforms(2).ContainsNaN());
#endif
	}
	OutBoneTransforms(2).SetOrigin(OutEndPos);

#if !FINAL_RELEASE && !SHIPPING_PC_GAME
	check(!OutBoneTransforms(2).ContainsNaN());
#endif
}

INT USkelControlLimb::GetWidgetCount()
{
	return 2;
}

FBoneAtom USkelControlLimb::GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex)
{
	check(WidgetIndex < 2);

	FBoneAtom ComponentToFrame;
	FVector ComponentLoc;
	if(WidgetIndex == 0)
	{
		ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, EffectorLocationSpace, EffectorSpaceBoneName);
		ComponentLoc = SkelComp->LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(EffectorLocation) );
	}
	else
	{
		ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, JointTargetLocationSpace, JointTargetSpaceBoneName);
		ComponentLoc = SkelComp->LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(JointTargetLocation) );
	}

	FBoneAtom FrameToComponent = ComponentToFrame.InverseSafe() * SkelComp->LocalToWorldBoneAtom;
	FrameToComponent.SetOrigin(ComponentLoc);

	return FrameToComponent;
}

void USkelControlLimb::HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec)
{
	check(WidgetIndex < 2);

	if(WidgetIndex == 0)
	{
		EffectorLocation += DragVec;
	}
	else
	{
		JointTargetLocation += DragVec;
	}
}

void USkelControlLimb::DrawSkelControl3D(const FSceneView* View, FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelComp, INT BoneIndex)
{
	FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, EffectorLocationSpace, EffectorSpaceBoneName);
	FVector ComponentLoc = SkelComp->LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(EffectorLocation) );
	FMatrix DiamondTM = FTranslationMatrix(ComponentLoc);
	DrawWireDiamond(PDI, DiamondTM, CONTROL_DIAMOND_SIZE, FColor(128,128,255), SDPG_Foreground );

	ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, JointTargetLocationSpace, JointTargetSpaceBoneName);
	ComponentLoc = SkelComp->LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(JointTargetLocation) );
	DiamondTM = FTranslationMatrix(ComponentLoc);
	DrawWireDiamond(PDI, DiamondTM, CONTROL_DIAMOND_SIZE, FColor(255,128,128), SDPG_Foreground );
}


/*-----------------------------------------------------------------------------
	USkelControlFootPlacement
-----------------------------------------------------------------------------*/
//
// USkelControlFootPlacement::CalculateNewBoneTransforms
//
void USkelControlFootPlacement::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	// First get indices of the lower and upper limb bones. 
	// We should have checked this in GetAffectedBones so can assume its ok now.

	check(BoneIndex != 0);
	INT LowerLimbIndex = SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
	check(LowerLimbIndex != 0);
	INT UpperLimbIndex = SkelComp->SkeletalMesh->RefSkeleton(LowerLimbIndex).ParentIndex;

	// Find the root and end position in world space.
	FVector RootPos = SkelComp->SpaceBases(UpperLimbIndex).GetOrigin();
	FVector WorldRootPos = SkelComp->LocalToWorld.TransformFVector(RootPos);

	FVector EndPos = SkelComp->SpaceBases(BoneIndex).GetOrigin();
	FVector WorldEndPos = SkelComp->LocalToWorld.TransformFVector(EndPos);

	FVector LegDelta = WorldEndPos - WorldRootPos;
	FVector LegDir = LegDelta.SafeNormal();

	// We do a hack here - extend length of line by 100 - to get around Unreals nasty line check fudging.
	FVector CheckEndPos = WorldEndPos + (100.f + FootOffset + MaxDownAdjustment) * LegDir;

	FVector HitLocation, HitNormal;
	UBOOL bHit = SkelComp->LegLineCheck( WorldRootPos, CheckEndPos, HitLocation, HitNormal);

	FLOAT LegAdjust = 0.f;
	if(bHit)
	{
		// Find how much we are adjusting the foot up or down. Postive is down, negative is up.
		LegAdjust = ((HitLocation - WorldEndPos) | LegDir);

		// Reject hits in the 100-unit dead region.
		if( bHit && LegAdjust > (FootOffset + MaxDownAdjustment) )
		{
			bHit = false;
		}
	}

	FVector DesiredPos;
	if(bHit)
	{
		LegAdjust -= FootOffset;

		// Clamp LegAdjust between MaxUp/DownAdjustment.
		LegAdjust = ::Clamp(LegAdjust, -MaxUpAdjustment, MaxDownAdjustment);

		// If bOnlyEnableForUpAdjustment is true, do nothing if we are not adjusting the leg up.
		if(bOnlyEnableForUpAdjustment && LegAdjust >= 0.f)
		{
			return;
		}

		// ..and calculate EffectorLocation.
		DesiredPos = WorldEndPos + (LegAdjust * LegDir);
	}
	else
	{
		if(bOnlyEnableForUpAdjustment)
		{
			return;
		}	

		// No hit - we reach as far as MaxDownAdjustment will allow.
		DesiredPos = WorldEndPos + (MaxDownAdjustment * LegDir);
	}

	EffectorLocation = DesiredPos;
	EffectorLocationSpace = BCS_WorldSpace;

	Super::CalculateNewBoneTransforms(BoneIndex, SkelComp, OutBoneTransforms);

	check(OutBoneTransforms.Num() == 3);

	// OutBoneTransforms(2) will be the transform for the foot here.
	FVector FootPos = ( OutBoneTransforms(2) * SkelComp->LocalToWorldBoneAtom).GetOrigin();	
	
	// Now we orient the foot if desired, and its sufficiently close to our desired position.
	if(bOrientFootToGround && bHit && ((FootPos - DesiredPos).Size() < 1.0f) && !HitNormal.IsZero())
	{
		check(!OutBoneTransforms(2).ContainsNaN());

		// Find reference frame we are trying to orient to the ground. Its the foot bone transform, with the FootRotOffset applied.
		FBoneAtom BoneRefMatrix = FBoneAtom(FootRotOffset, FVector::ZeroVector) * OutBoneTransforms(2);

		// Find the 'up' vector of that reference frame, in component space.
		FVector CurrentFootDir = BoneRefMatrix.TransformNormal( GetAxisDirVector(FootUpAxis, bInvertFootUpAxis) );
		CurrentFootDir = CurrentFootDir.SafeNormal();	

		// Transform hit normal into component space.
		FVector NormalCompSpace = SkelComp->LocalToWorld.InverseTransformNormal(HitNormal).SafeNormal();

		// Calculate a quaternion that gets us from our current rotation to the desired one.
		FQuat DeltaFootQuat = FQuatFindBetween(CurrentFootDir, NormalCompSpace);
		FBoneAtom TempFootTM( DeltaFootQuat, FVector(0.f) );
		check(!TempFootTM.ContainsNaN());

		// Limit the maximum amount we are going to correct by to 'MaxFootOrientAdjust'
		FLOAT MaxFootOrientRad = MaxFootOrientAdjust * ((FLOAT)PI/180.f);
		FVector DeltaFootAxis;
		FLOAT DeltaFootAng;
		DeltaFootQuat.ToAxisAndAngle(DeltaFootAxis, DeltaFootAng);
		DeltaFootAng = ::Clamp(DeltaFootAng, -MaxFootOrientRad, MaxFootOrientRad);
		DeltaFootQuat = FQuat(DeltaFootAxis, DeltaFootAng);

		// Convert from quaternion to rotation matrix.
		FBoneAtom DeltaFootTM( DeltaFootQuat, FVector(0.f) );
		check(!DeltaFootTM.ContainsNaN());

		// Apply rotation matrix to current foot matrix.
		FVector FootBonePos = OutBoneTransforms(2).GetOrigin();
		OutBoneTransforms(2).SetOrigin( FVector(0.f) );
		OutBoneTransforms(2) = OutBoneTransforms(2) * DeltaFootTM;
		OutBoneTransforms(2).SetOrigin( FootBonePos );
		check(!OutBoneTransforms(2).ContainsNaN());
	}
}

/*-----------------------------------------------------------------------------
	USkelControlWheel
-----------------------------------------------------------------------------*/

//
// USkelControlWheel::UpdateWheelControl
//
void USkelControlWheel::UpdateWheelControl( FLOAT InDisplacement, FLOAT InRoll, FLOAT InSteering )
{
	WheelDisplacement = InDisplacement;
	WheelRoll = InRoll;
	WheelSteering = InSteering;
}

//
// USkelControlWheel::CalculateNewBoneTransforms
//
void USkelControlWheel::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	FVector TotalScale = SkelComp->Scale * SkelComp->Scale3D;
	if( SkelComp->GetOwner() != NULL )
	{
		TotalScale *= SkelComp->GetOwner()->DrawScale * SkelComp->GetOwner()->DrawScale3D;
	}

	if( TotalScale.X != 0.f )
	{
		FLOAT RenderDisplacement = ::Min(WheelDisplacement/TotalScale.X, WheelMaxRenderDisplacement);
		BoneTranslation = RenderDisplacement * FVector(0,0,1);
	}

	FVector RollAxis = GetAxisDirVector(WheelRollAxis, bInvertWheelRoll);
	FVector SteerAxis = GetAxisDirVector(WheelSteeringAxis, bInvertWheelSteering);

	FQuat TotalRot = FQuat(SteerAxis, WheelSteering * ((FLOAT)PI/180.f)) * FQuat(RollAxis, WheelRoll * ((FLOAT)PI/180.f));
	BoneRotation = FRotator(TotalRot);//FRotationMatrix(TotalRot).Rotator();

	Super::CalculateNewBoneTransforms(BoneIndex, SkelComp, OutBoneTransforms);
}

/*-----------------------------------------------------------------------------
USkelControlHandlebars
-----------------------------------------------------------------------------*/

//
// USkelControlHandlebars::CalculateNewBoneTransforms
//
void USkelControlHandlebars::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	if (SteerWheelBoneIndex == INDEX_NONE)
	{
	   	SteerWheelBoneIndex = SkelComp->MatchRefBone(WheelBoneName);
	}

	if (SteerWheelBoneIndex != INDEX_NONE)
	{
		INT WheelRightAxis = 0;
		if (WheelRollAxis == AXIS_X)
		{
			WheelRightAxis = 0;
		}
		else if (WheelRollAxis == AXIS_Y)
		{
			WheelRightAxis = 1;
		}
		else
		{
			WheelRightAxis = 2;
		}

		//Get the wheel's "right" vector
		const FBoneAtom WheelMatrix = SkelComp->SpaceBases(SteerWheelBoneIndex);
		const FVector SteerRightAxis = WheelMatrix.GetAxis(WheelRightAxis).SafeNormal();

		//Calculate the steering angle (based off of X Axis being the front 0 degrees)
		FLOAT Angle = appAtan2(SteerRightAxis.Y, SteerRightAxis.X);

		//Forward angle is 90 off right steering
		FLOAT RotAngle = (PI * 0.5f) - Angle;
		const FVector HandleBarRotateAxis = GetAxisDirVector(HandlebarRotateAxis, bInvertRotation);
		const FQuat TotalRot(HandleBarRotateAxis, RotAngle);
		BoneRotation = FRotator(TotalRot);
	}

	Super::CalculateNewBoneTransforms(BoneIndex, SkelComp, OutBoneTransforms);
}

void USkelControlHandlebars::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( GIsEditor &&
		PropertyThatChanged &&
		PropertyThatChanged->GetFName() == FName(TEXT("WheelBoneName")) )
	{
		//Reset the cached bone index
		SteerWheelBoneIndex = INDEX_NONE;
		BoneRotation = FRotator(0,0,0);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	USkelControlTrail
-----------------------------------------------------------------------------*/

void USkelControlTrail::TickSkelControl(FLOAT DeltaSeconds, USkeletalMeshComponent* SkelComp)
{
	Super::TickSkelControl(DeltaSeconds, SkelComp);

	ThisTimstep = DeltaSeconds;

	// Force a reset of trail after control strength has gone to zero
	if(ControlStrength < KINDA_SMALL_NUMBER)
	{
		bHadValidStrength = FALSE;
	}
}

void USkelControlTrail::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);

	if( ChainLength < 2 )
	{
		return;
	}

	// The incoming BoneIndex is the 'end' of the spline chain. We need to find the 'start' by walking SplineLength bones up hierarchy.
	// Fail if we walk past the root bone.

	INT WalkBoneIndex = BoneIndex;
	OutBoneIndices.Add(ChainLength); // Allocate output array of bone indices.
	OutBoneIndices(ChainLength-1) = BoneIndex;

	for(INT i=1; i<ChainLength; i++)
	{
		INT OutTransformIndex = ChainLength-(i+1);

		// If we are at the root but still need to move up, chain is too long, so clear the OutBoneIndices array and give up here.
		if(WalkBoneIndex == 0)
		{
			debugf( TEXT("UWarSkelCtrl_Trail : Spling passes root bone of skeleton.") );
			OutBoneIndices.Empty();
			return;
		}
		else
		{
			// Get parent bone.
			WalkBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(WalkBoneIndex).ParentIndex;

			// Insert indices at the start of array, so that parents are before children in the array.
			OutBoneIndices(OutTransformIndex) = WalkBoneIndex;
		}
	}
}

void USkelControlTrail::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	if( ChainLength < 2 )
	{
		return;
	}

	OutBoneTransforms.Add(ChainLength);

	// Build array of bone indices - starting at highest bone and running down to end of chain (where controller is)
	// Same code as in GetAffectedBones above!
	TArray<INT> ChainBoneIndices;
	INT WalkBoneIndex = BoneIndex;
	ChainBoneIndices.Add(ChainLength); // Allocate output array of bone indices.
	ChainBoneIndices(ChainLength-1) = BoneIndex;
	for(INT i=1; i<ChainLength; i++)
	{
		check(WalkBoneIndex != 0);

		// Get parent bone.
		WalkBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(WalkBoneIndex).ParentIndex;

		// Insert indices at the start of array, so that parents are before children in the array.
		INT OutTransformIndex = ChainLength-(i+1);
		ChainBoneIndices(OutTransformIndex) = WalkBoneIndex;
	}

	// If we have >0 this frame, but didn't last time, record positions of all the bones.
	// Also do this if number has changed or array is zero.
	UBOOL bHasValidStrength = (ControlStrength > 0.f);
	if(TrailBoneLocations.Num() != ChainLength || (bHasValidStrength && !bHadValidStrength))
	{
		TrailBoneLocations.Empty();
		TrailBoneLocations.Add(ChainLength);

		for(INT i=0; i<ChainBoneIndices.Num(); i++)
		{
			INT ChildIndex = ChainBoneIndices(i);
			TrailBoneLocations(i) = SkelComp->SpaceBases(ChildIndex).GetOrigin();
		}
		OldLocalToWorld = SkelComp->LocalToWorld;
	}
	bHadValidStrength = bHasValidStrength;

	// transform between last frame and now.
	FMatrix OldToNewTM = OldLocalToWorld * SkelComp->LocalToWorld.Inverse();

	// Add fake velocity if present to all but root bone
	if(!FakeVelocity.IsZero())
	{
		FVector FakeMovement = -FakeVelocity * ThisTimstep;

		// If desired, treat velocity as being in actor space, so transform into world
		if(bActorSpaceFakeVel && SkelComp->GetOwner())
		{
			const FBoneAtom ActorToWorld(SkelComp->GetOwner()->Rotation, SkelComp->GetOwner()->Location);
			FakeMovement = ActorToWorld.TransformNormal(FakeMovement);
		}

		// Then transform from world into component space
		FakeMovement = SkelComp->LocalToWorld.InverseTransformNormal(FakeMovement);

		// Then add to each bone
		for(INT i=1; i<TrailBoneLocations.Num(); i++)
		{
			TrailBoneLocations(i) += FakeMovement;
		}
	}

	// Root bone of trail is not modified.
	INT RootIndex = ChainBoneIndices(0); 
	OutBoneTransforms(0)	= SkelComp->SpaceBases(RootIndex); // Local space matrix
	TrailBoneLocations(0)	= OutBoneTransforms(0).GetOrigin(); 

	// Starting one below head of chain, move bones.
	for(INT i=1; i<ChainBoneIndices.Num(); i++)
	{
		// Parent bone position in component space.
		INT ParentIndex = ChainBoneIndices(i-1);
		FVector ParentPos = TrailBoneLocations(i-1);
		FVector ParentAnimPos = SkelComp->SpaceBases(ParentIndex).GetOrigin();

		// Child bone position in component space.
		INT ChildIndex = ChainBoneIndices(i);
		FVector ChildPos = OldToNewTM.TransformFVector(TrailBoneLocations(i)); // move from 'last frames component' frame to 'this frames component' frame
		FVector ChildAnimPos = SkelComp->SpaceBases(ChildIndex).GetOrigin();

		// Desired parent->child offset.
		FVector TargetDelta = (ChildAnimPos - ParentAnimPos);

		// Desired child position.
		FVector ChildTarget = ParentPos + TargetDelta;

		// Find vector from child to target
		FVector Error = ChildTarget - ChildPos;

		// Calculate how much to push the child towards its target
		FLOAT Correction = Clamp(ThisTimstep * TrailRelaxation, 0.f, 1.f);
		//FLOAT Correction = Clamp(TrailRelaxation, 0.f, 1.f);

		// Scale correction vector and apply to get new world-space child position.
		TrailBoneLocations(i) = ChildPos + (Error * Correction);

		// If desired, prevent bones stretching too far.
		if(bLimitStretch)
		{
			FLOAT RefPoseLength = TargetDelta.Size();
			FVector CurrentDelta = TrailBoneLocations(i) - TrailBoneLocations(i-1);
			FLOAT CurrentLength = CurrentDelta.Size();

			// If we are too far - cut it back (just project towards parent particle).
			if( (CurrentLength - RefPoseLength > StretchLimit) && CurrentLength > SMALL_NUMBER )
			{
				FVector CurrentDir = CurrentDelta / CurrentLength;
				TrailBoneLocations(i) = TrailBoneLocations(i-1) + (CurrentDir * (RefPoseLength + StretchLimit));
			}
		}

		// Modify child matrix
		OutBoneTransforms(i) = SkelComp->SpaceBases(ChildIndex);
		OutBoneTransforms(i).SetOrigin( TrailBoneLocations(i) );

		// Modify rotation of parent matrix to point at this one.

		// Calculate the direction that parent bone is currently pointing.
		FVector CurrentBoneDir = OutBoneTransforms(i-1).TransformNormal( GetAxisDirVector(ChainBoneAxis, bInvertChainBoneAxis) );
		CurrentBoneDir = CurrentBoneDir.SafeNormal();

		// Calculate vector from parent to child.
		FVector NewBoneDir = (OutBoneTransforms(i).GetOrigin() - OutBoneTransforms(i-1).GetOrigin()).SafeNormal();

		// Calculate a quaternion that gets us from our current rotation to the desired one.
		FQuat DeltaLookQuat = FQuatFindBetween(CurrentBoneDir, NewBoneDir);
		FBoneAtom DeltaTM( DeltaLookQuat, FVector(0.f) );

		// Apply to the current parent bone transform.
		FBoneAtom TmpMatrix = FBoneAtom::Identity;
		CopyRotationPart(TmpMatrix, OutBoneTransforms(i-1));
		TmpMatrix = TmpMatrix * DeltaTM;
		CopyRotationPart(OutBoneTransforms(i-1), TmpMatrix);
	}

	// For the last bone in the chain, use the rotation from the bone above it.
	CopyRotationPart(OutBoneTransforms(ChainLength-1), OutBoneTransforms(ChainLength-2));

	// Update OldLocalToWorld
	OldLocalToWorld = SkelComp->LocalToWorld;
}


/************************************************************************************
 * USkelControl_CCD_IK
 ***********************************************************************************/

void USkelControl_CCD_IK::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);

	// Make sure we have at least two bones
	if( NumBones < 2 )
	{
		return;
	}

	OutBoneIndices.Add(NumBones);

	INT WalkBoneIndex = BoneIndex;
	for(INT i=NumBones-1; i>=0; i--)
	{
		if( WalkBoneIndex == 0 )
		{
			debugf( TEXT("USkelControl_CCD_IK : Spline passes root bone of skeleton.") );
			OutBoneIndices.Reset();
			return;
		}

		// Look up table for bone indices
		OutBoneIndices(i) = WalkBoneIndex;
		WalkBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(WalkBoneIndex).ParentIndex;
	}
}

/**
 * CCD IK or "Cyclic-Coordinate Descent Inverse kinematics"
 */
void USkelControl_CCD_IK::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	FLOAT ChainLength = 0.f;
	TArray<INT>			BoneIndices;
	FBoneAtomArray	LocalAtoms;
	INT CurrentBoneIndex = BoneIndex;

	// Keep constraints up to date
	if( AngleConstraint.Num() != NumBones )
	{
		AngleConstraint.Reset();
		AngleConstraint.AddZeroed(NumBones);
	}

	BoneIndices.Add(NumBones);
	LocalAtoms.Add(NumBones);
	OutBoneTransforms.Add(NumBones);
	for(INT i=NumBones-1; i>=0; i--)
	{
		// Look up table for bone indices
		BoneIndices(i) = CurrentBoneIndex;
		// Copy local transforms
		LocalAtoms(i) = SkelComp->LocalAtoms(CurrentBoneIndex);
		// Calculate size of bone chain
		ChainLength += LocalAtoms(i).GetTranslation().Size();
		// Copy space bases into output transforms.
		OutBoneTransforms(i) = SkelComp->SpaceBases(CurrentBoneIndex);

		CurrentBoneIndex = SkelComp->SkeletalMesh->RefSkeleton(CurrentBoneIndex).ParentIndex;
	}

	// Add Effector translation size as well.
	ChainLength += EffectorTranslationFromBone.Size();

	// Setting IK location
	FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, EffectorLocationSpace, EffectorSpaceBoneName);
	// Translation in the component reference frame.
	FVector IKTargetLocation = ComponentToFrame.InverseSafe().TransformFVector(EffectorLocation);

	// Start Bone position.
	FVector StartBonePos = OutBoneTransforms(0).GetOrigin();
	
	// Make sure we're not over extending the chain.
	FVector	DesiredDelta = IKTargetLocation - StartBonePos;
	FLOAT DesiredLength = DesiredDelta.Size();

	// Check to handle case where IKTargetLocation is the same as StartBonePos.
	FVector DesiredDir;
	if( DesiredLength < Precision )
	{
		DesiredLength	= Precision;
		DesiredDir		= FVector(1,0,0);
	}
	else
	{
		DesiredDir		= DesiredDelta/DesiredLength;
	}

	// Make sure we're not over extending the chain.
	if( DesiredLength > ChainLength - Precision )
	{
		DesiredLength = ChainLength - Precision;
		DesiredDelta = DesiredDir * DesiredLength;
		IKTargetLocation = StartBonePos + DesiredDelta;
	}

	// Start with End of Chain bone.
	INT MaxIterations = MaxPerBoneIterations * (NumBones - 1);
	INT Index = bStartFromTail ? 1 : NumBones - 1;
	INT BoneCount = 1;
	IterationsCount = 0;
	FLOAT DistToTargetSq;
	do 
	{
		// End of current bone chain
		FVector ChainEndPos = (FBoneAtom(FQuat::Identity, EffectorTranslationFromBone) * OutBoneTransforms(NumBones-1)).GetOrigin();
		FVector ChainEndToTarget = IKTargetLocation - ChainEndPos;
		DistToTargetSq = ChainEndToTarget.SizeSquared();

		if( DistToTargetSq > Precision )
		{
			// Current Bone Start Position (Head of the Bone)
			FVector BoneStartPosition	= OutBoneTransforms(Index-1).GetOrigin();
			FVector BoneToEnd			= ChainEndPos - BoneStartPosition;
			FVector BoneToTarget		= IKTargetLocation - BoneStartPosition;
			FVector BoneToEndDir		= BoneToEnd.SafeNormal();
			FVector BoneToTargetDir		= BoneToTarget.SafeNormal();

			// Make sure we have a valid setup to work with.
			if( (!BoneToEndDir.IsZero() && !BoneToTargetDir.IsZero()) && 
				// If bones are parallel, then no rotation is going to happen, so just skip to next bone.
				(!bNoTurnOptimization || (BoneToEndDir | BoneToTargetDir) < (1.f - SMALL_NUMBER)) )
			{
				FVector RotationAxis;
				FLOAT	RotationAngle;
				FindAxisAndAngle(BoneToEndDir, BoneToTargetDir, RotationAxis, RotationAngle);

				// Max turn steps.
				if( MaxAngleSteps > 0.f && RotationAngle > MaxAngleSteps )
				{
					RotationAngle = MaxAngleSteps;
				}

				FQuat RotationQuat(RotationAxis, RotationAngle);

				OutBoneTransforms(Index-1).SetOrigin(FVector(0.f));
				OutBoneTransforms(Index-1) = OutBoneTransforms(Index-1) * FBoneAtom(RotationQuat, FVector(0.f));
				OutBoneTransforms(Index-1).SetOrigin(BoneStartPosition);
				check(!OutBoneTransforms(Index-1).ContainsNaN());

				// Update local transform for this bone
				FBoneAtom Parent;
				if( Index >= 2 )
				{
					Parent = OutBoneTransforms(Index-2);
				}
				else
				{
					INT ParentBoneIndex = SkelComp->SkeletalMesh->RefSkeleton( BoneIndices(Index-1) ).ParentIndex;
					if( ParentBoneIndex == 0 )
					{
						Parent = FBoneAtom::Identity;
					}
					else
					{
						Parent = SkelComp->SpaceBases(ParentBoneIndex);
					}
				}

				LocalAtoms(Index - 1) = OutBoneTransforms(Index-1) * Parent.InverseSafe();
				// Apply angle constraint
				if( AngleConstraint(Index-1) > 0.f )
				{
					LocalAtoms(Index-1).GetRotation().ToAxisAndAngle(RotationAxis, RotationAngle);

					// If we're beyond constraint, enforce limits.
					if( RotationAngle > AngleConstraint(Index-1) )
					{
						RotationAngle = AngleConstraint(Index-1);
						LocalAtoms(Index-1).SetRotation(FQuat(RotationAxis, RotationAngle));
						OutBoneTransforms(Index-1) = LocalAtoms(Index-1) * Parent;
					}
				}

				// Update world transforms for children if needed
				for(INT i=Index; i<NumBones; i++)
				{
					OutBoneTransforms(i) = LocalAtoms(i) * OutBoneTransforms(i-1);
					OutBoneTransforms(i).RemoveScaling();
				}
			}
		}

		// Handle looping through bones
		BoneCount++;
		if( bStartFromTail )
		{
			if( ++Index >= NumBones )
			{
				Index = 1;
				BoneCount = 1;
			}
		}
		else
		{
			if( --Index < 1 )
			{
				Index = NumBones - 1;
				BoneCount = 1;
			}
		}

	} while( IterationsCount++ < MaxIterations && DistToTargetSq > Precision );
}

INT USkelControl_CCD_IK::GetWidgetCount()
{
	return 1;
}

FBoneAtom USkelControl_CCD_IK::GetWidgetTM(INT WidgetIndex, USkeletalMeshComponent* SkelComp, INT BoneIndex)
{
	check(WidgetIndex == 0);
	FBoneAtom ComponentToFrame = SkelComp->CalcComponentToFrameMatrix(BoneIndex, EffectorLocationSpace, EffectorSpaceBoneName);
	FBoneAtom LocalToWorld(SkelComp->LocalToWorld);
	FBoneAtom FrameToComponent = ComponentToFrame.InverseSafe() * LocalToWorld;
	FrameToComponent.SetOrigin( LocalToWorld.TransformFVector( ComponentToFrame.InverseSafe().TransformFVector(EffectorLocation) ) );

	return FrameToComponent;
}

void USkelControl_CCD_IK::HandleWidgetDrag(INT WidgetIndex, const FVector& DragVec)
{
	check(WidgetIndex == 0);
	EffectorLocation += DragVec;
}

/************************************************************************************
 * USkelControl_Multiply
 ***********************************************************************************/

void USkelControl_Multiply::GetAffectedBones(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<INT>& OutBoneIndices)
{
	check(OutBoneIndices.Num() == 0);
	OutBoneIndices.AddItem(BoneIndex);
}

//
// USkelControlSingleBone::ApplySkelControl
//
void USkelControl_Multiply::CalculateNewBoneTransforms(INT BoneIndex, USkeletalMeshComponent* SkelComp, TArray<FBoneAtom>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// Reference bone
	const FQuat RefQuat = SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation;
	// Find delta angle for the bone.
	FQuat DeltaBoneQuat = ExtractAngle(BoneIndex, SkelComp);

	// Turn to Axis and Angle
	FVector DeltaAxis, RefAxis;
	FLOAT	DeltaAngle, RefAngle;
	// get axis and angle
	DeltaBoneQuat.ToAxisAndAngle(DeltaAxis, DeltaAngle);
	RefQuat.ToAxisAndAngle(RefAxis, RefAngle);
	
	const FVector DefaultAxis = RefAxis;

	// make sure we're in the same direction with ref pose axis
	// See if we need to invert angle.
	if( (DeltaAxis | DefaultAxis) < 0.f )
	{
		DeltaAxis = -DeltaAxis;
		DeltaAngle = -DeltaAngle;
	}

	// Make sure it is the shortest angle.
	DeltaAngle = UnwindHeading(DeltaAngle);

	// New bone rotation
	FQuat NewQuat = RefQuat * FQuat(DeltaAxis, DeltaAngle * Multiplier);
	// Normalize resulting quaternion.
	NewQuat.Normalize();

	// Turn that back into mesh space
	FBoneAtom NewBoneAtom(NewQuat, SkelComp->LocalAtoms(BoneIndex).GetTranslation());
	OutBoneTransforms.AddItem( NewBoneAtom * SkelComp->SpaceBases(SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex) );

#if 0
	const FQuat OriginalQuat = SkelComp->LocalAtoms(BoneIndex).GetRotation();
	debugf( TEXT("\t RefQuat: %s, Rot: %s"), *RefQuat.ToString(), *RefQuat.Rotator().ToString() );
	debugf( TEXT("\t DeltaBoneQuat: %s, Rot: %s"), *DeltaBoneQuat.ToString(), *DeltaBoneQuat.Rotator().ToString() );
	debugf( TEXT("\t NewQuat: %s, Rot: %s"), *NewQuat.ToString(), *NewQuat.Rotator().ToString() );
	debugf( TEXT("\t OriginalQuat: %s, Rot: %s"), *OriginalQuat.ToString(), *OriginalQuat.Rotator().ToString() );
#endif
}

/** Get Delta Angle between ref pose and animated pose **/
FQuat USkelControl_Multiply::ExtractAngle(INT BoneIndex, USkeletalMeshComponent* SkelComp)
{
	const FBoneAtom BoneAtom = SkelComp->LocalAtoms(BoneIndex);
	const FBoneAtom BoneAtomRef(SkelComp->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation, BoneAtom.GetTranslation());

	// find delta angle between the two quaternions X Axis.
	FQuat AnimQuat = BoneAtom.GetRotation();
	FQuat RefQuat = BoneAtomRef.GetRotation();
	// Delta = R(inverse)*OriginalQuat
	FQuat DeltaQuat = RefQuat.Inverse()*AnimQuat;
	return DeltaQuat;
}

