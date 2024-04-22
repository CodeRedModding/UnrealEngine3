/*=============================================================================
	UnMorph.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineMaterialClasses.h"
#include "UnLinkedObjDrawUtils.h"

IMPLEMENT_CLASS(UMorphTarget)
IMPLEMENT_CLASS(UMorphTargetSet)
IMPLEMENT_CLASS(UMorphWeightSequence)

IMPLEMENT_CLASS(UMorphNodeBase)
IMPLEMENT_CLASS(UMorphNodeWeightBase)
IMPLEMENT_CLASS(UMorphNodeWeight)
IMPLEMENT_CLASS(UMorphNodePose)
IMPLEMENT_CLASS(UMorphNodeMultiPose);
IMPLEMENT_CLASS(UMorphNodeWeightByBoneAngle)
IMPLEMENT_CLASS(UMorphNodeWeightByBoneRotation)


static const FColor MorphConnColor(50,50,100);
static const FColor MorphWeightColor(60,60,90);
/** BackGround color when a node has been deprecated */
static const FColor Morph_DeprecatedBGColor(200,0,0);

/** Color to draw a selected object */
static const FColor Morph_SelectedColor( 255, 255, 0 );

#define ZERO_MORPHWEIGHT_THRESH (0.01f)  // Below this weight threshold, morphs won't be blended in.


//////////////////////////////////////////////////////////////////////////
// UMorphTargetSet
//////////////////////////////////////////////////////////////////////////

FString UMorphTargetSet::GetDesc()
{
	return FString::Printf( TEXT("%d MorphTargets"), Targets.Num() );
}

/** Find a morph target by name in this MorphTargetSet. */ 
UMorphTarget* UMorphTargetSet::FindMorphTarget(FName MorphTargetName)
{
	if(MorphTargetName == NAME_None)
	{
		return NULL;
	}

	for(INT i=0; i<Targets.Num(); i++)
	{
		if( Targets(i)->GetFName() == MorphTargetName )
		{
			return Targets(i);
		}
	}

	return NULL;
}


INT UMorphTargetSet::GetResourceSize()
{
	INT Retval = 0;

	if (!GExclusiveResourceSizeMode)
	{
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();

		// Count up the morph targets in the set
		for( INT i=0; i < Targets.Num(); ++i )
		{
			ResourceSize += Targets(i)->GetResourceSize();
		}

		Retval = ResourceSize;
	}

	return Retval;
}

/** 
* Verify if the current BaseSkelMesh is still valid
* 
* @return TRUE if so. FALSE otherwise. 
*/
UBOOL UMorphTargetSet::IsValidBaseMesh()
{
#if WITH_EDITORONLY_DATA
	// no data to verify. Assume it's still good
	if ( RawWedgePointIndices.Num() == 0 )
	{
		return TRUE;
	}

	// if no basemesh, then return false
	if ( BaseSkelMesh == NULL )
	{
		return FALSE;
	}

	// go through all LODs and find if the information is valid or not
	for( INT LODIndex=0; LODIndex < BaseSkelMesh->LODModels.Num(); ++LODIndex )
	{
		// if not filled up yet, assume it's good
		if ( LODIndex >=  RawWedgePointIndices.Num())
		{
			continue;
		}
		else // I have data now
		{
			FStaticLODModel & LODModel = BaseSkelMesh->LODModels(LODIndex);
			TArray<DWORD> & LODWedgePointIndices = RawWedgePointIndices(LODIndex);

			// not valid if # of elements don't match
			if (LODModel.RawPointIndices.GetElementCount() != LODWedgePointIndices.Num())
			{
				return FALSE;
			}

			// now the size should be same
			check (LODWedgePointIndices.GetAllocatedSize() == LODModel.RawPointIndices.GetBulkDataSize());

			// compare CRC
			DWORD BaseCRC = appMemCrc(LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODModel.RawPointIndices.GetBulkDataSize());
			LODModel.RawPointIndices.Unlock();

			DWORD TargetCRC = appMemCrc(LODWedgePointIndices.GetData(), LODWedgePointIndices.GetAllocatedSize());

			if ( BaseCRC != TargetCRC )
			{
				return FALSE;
			}

			// Sometimes CRC check didn't work. Will do memcmp if that was same. 
			UBOOL Different = appMemcmp(LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODWedgePointIndices.GetData(), LODWedgePointIndices.GetAllocatedSize());
			LODModel.RawPointIndices.Unlock();
			if ( Different )
			{
				return FALSE;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	return TRUE;
}

/** Need to serialize **/
void UMorphTargetSet::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// this is editor only data, so no reason to do anything if not editor
	if (VER_SERIALIZE_MORPHTARGETRAWVERTSINDICES <= Ar.Ver())
	{
		Ar << RawWedgePointIndices;
	}

	if (!GIsEditor || GIsCooking)
	{
		// we don't need this data. throw it out
		RawWedgePointIndices.Empty();
	}
}
/** 
* Update vertex indices for all morph targets from the base mesh
*
*/
void UMorphTargetSet::UpdateMorphTargetsFromBaseMesh()
{
#if WITH_EDITORONLY_DATA
	// go through all targets and 
	for ( INT TargetID = 0; TargetID< Targets.Num(); ++TargetID )
	{	
		if ( Targets(TargetID) )
		{
			// remap vertices
			Targets(TargetID)->RemapVertexIndices(BaseSkelMesh, RawWedgePointIndices);
		}
	}

	FillBaseMeshData(FALSE);

	appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_38") );
#endif // WITH_EDITORONLY_DATA
}

/**
* Refill data assuming current base mesh exactly works
* This is only for compatibility to support old morph targets to re-fill base mesh information 
*/
void UMorphTargetSet::FillBaseMeshData(UBOOL DoNotOverwriteIfExists)
{
#if WITH_EDITORONLY_DATA
	if ( BaseSkelMesh )
	{
		for ( INT LODIndex=0; LODIndex < BaseSkelMesh->LODModels.Num(); ++LODIndex )
		{
			// if not filled up yet, assume it's good
			if ( LODIndex >=  RawWedgePointIndices.Num())
			{
				// make sure the new added data is the index of LOD
				verify(LODIndex == RawWedgePointIndices.AddZeroed());
			}
		
			FStaticLODModel & LODModel = BaseSkelMesh->LODModels(LODIndex);
			TArray<DWORD> & LODWedgePointIndices = RawWedgePointIndices(LODIndex);

			// if it can overwrite existing or LODWedgePointIndices is empty
			if ( !DoNotOverwriteIfExists || LODWedgePointIndices.Num()==0)
			{
				LODWedgePointIndices.Empty( LODModel.RawPointIndices.GetElementCount() );
				LODWedgePointIndices.Add( LODModel.RawPointIndices.GetElementCount() );
				appMemcpy( LODWedgePointIndices.GetData(), LODModel.RawPointIndices.Lock(LOCK_READ_ONLY), LODModel.RawPointIndices.GetBulkDataSize() );
				LODModel.RawPointIndices.Unlock();					
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}
//////////////////////////////////////////////////////////////////////////
// UMorphWeightSequence
//////////////////////////////////////////////////////////////////////////

FString UMorphWeightSequence::GetDesc()
{
	return FString( TEXT("") );
}

//////////////////////////////////////////////////////////////////////////
// UMorphNodeBase
//////////////////////////////////////////////////////////////////////////

void UMorphNodeBase::InitMorphNode(USkeletalMeshComponent* InSkelComp)
{
	// Save a reference to the SkeletalMeshComponent that owns this MorphNode.
	SkelComponent = InSkelComp;
}

void UMorphNodeBase::GetNodes(TArray<UMorphNodeBase*>& OutNodes)
{
	// Add myself to the list.
	OutNodes.AddUniqueItem(this);
}

FIntPoint UMorphNodeBase::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
#if WITH_EDITORONLY_DATA
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, OutDrawY );
	}
#endif // WITH_EDITORONLY_DATA

	return FIntPoint( 0, 0 );
}

void UMorphNodeBase::OnPaste()
{
	Super::OnPaste();
}

//////////////////////////////////////////////////////////////////////////
// UMorphNodeWeightBase
//////////////////////////////////////////////////////////////////////////

void UMorphNodeWeightBase::GetNodes(TArray<UMorphNodeBase*>& OutNodes)
{
	// Add myself to the list.
	OutNodes.AddUniqueItem(this);

	// Iterate over each connector
	for(INT i=0; i<NodeConns.Num(); i++)
	{
		FMorphNodeConn& Conn = NodeConns(i);

		// Iterate over each link from this connector.
		for(INT j=0; j<Conn.ChildNodes.Num(); j++)
		{
			// If there is a child node, call GetNodes on it.
			if( Conn.ChildNodes(j) )
			{
				Conn.ChildNodes(j)->GetNodes(OutNodes);
			}
		}
	}
}

/**
 * Draws this morph node in the AnimTreeEditor.
 *
 * @param	Canvas			The canvas to use.
 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
 */
void UMorphNodeWeightBase::DrawMorphNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes)
{
#if WITH_EDITORONLY_DATA
	// Construct the FLinkedObjDrawInfo for use the linked-obj drawing utils.
	FLinkedObjDrawInfo ObjInfo;

	// AnimTree's don't have an output connector on left
	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("Out"), MorphConnColor ) );

	// Add output for each child.
	for(INT i=0; i<NodeConns.Num(); i++)
	{
		ObjInfo.Outputs.AddItem( FLinkedObjConnInfo(*NodeConns(i).ConnName.ToString(), MorphConnColor) );
	}

	ObjInfo.ObjObject = this;

	// Generate border color
	UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor = bSelected ? Morph_SelectedColor : FColor(0,0,0);

	// Generate name for node. User-give name if entered - otherwise just class name.
	FString NodeTitle;
	FString NodeDesc = GetClass()->GetDescription(); // Need to assign it here, so pointer isn't invalid by the time we draw it.
	if( NodeName != NAME_None )
	{
		NodeTitle = NodeName.ToString();
	}
	else
	{
		NodeTitle = NodeDesc;
	}

	// Use util to draw box with connectors etc.
	const FColor& BackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? Morph_DeprecatedBGColor : MorphWeightColor;
	const FColor FontColor = FColor( 255, 255, 128 );
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *NodeTitle, NULL, FontColor, BorderColor, BackGroundColor, FIntPoint(NodePosX, NodePosY) );

	// Read back draw locations of connectors, so we can draw lines in the correct places.
	OutDrawY = ObjInfo.InputY(0);

	for(INT i=0; i<NodeConns.Num(); i++)
	{
		NodeConns(i).DrawY = ObjInfo.OutputY(i);
	}

	DrawWidth = ObjInfo.DrawWidth;
	DrawHeight = ObjInfo.DrawHeight;

	// If desired, draw a slider for this node.
	if( bDrawSlider )
	{
		const INT SliderDrawY = NodePosY + ObjInfo.DrawHeight;
		const FColor SliderBackGroundColor = (GetClass()->ClassFlags & CLASS_Deprecated) ? Morph_DeprecatedBGColor : FColor(140,140,140);
		FLinkedObjDrawUtils::DrawSlider(Canvas, FIntPoint(NodePosX, SliderDrawY), DrawWidth, BorderColor, SliderBackGroundColor, GetSliderPosition(), FString(TEXT("")), this, 0, FALSE);
	}

	// Iterate over each connector
	for(INT i=0; i<NodeConns.Num(); i++)
	{
		FMorphNodeConn& Conn = NodeConns(i);

		// Iterate over each link from this connector.
		for(INT j=0; j<Conn.ChildNodes.Num(); j++)
		{
			// If there is a child node, call GetNodes on it.
			UMorphNodeBase* ChildNode = Conn.ChildNodes(j);
			if( ChildNode )
			{
				const FIntPoint Start	= GetConnectionLocation(LOC_OUTPUT, i);
				const FIntPoint End		= ChildNode->GetConnectionLocation(LOC_INPUT, 0);
				const FColor& LineColor = ( bSelected || SelectedNodes.ContainsItem( ChildNode ) ) ? Morph_SelectedColor : MorphConnColor;
				
				// Curves
				{
					const FLOAT Tension		= Abs<INT>(Start.X - End.X);
					FLinkedObjDrawUtils::DrawSpline(Canvas, End, -Tension * FVector2D(1,0), Start, -Tension * FVector2D(1,0), LineColor, TRUE);
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

FIntPoint UMorphNodeWeightBase::GetConnectionLocation(INT ConnType, INT ConnIndex)
{
#if WITH_EDITORONLY_DATA
	if(ConnType == LOC_INPUT)
	{
		check(ConnIndex == 0);
		return FIntPoint( NodePosX - LO_CONNECTOR_LENGTH, OutDrawY );
	}
	else if(ConnType == LOC_OUTPUT)
	{
		check( ConnIndex >= 0 && ConnIndex < NodeConns.Num() );
		return FIntPoint( NodePosX + DrawWidth + LO_CONNECTOR_LENGTH, NodeConns(ConnIndex).DrawY );
	}
#endif // WITH_EDITORONLY_DATA

	return FIntPoint( 0, 0 );
}

//////////////////////////////////////////////////////////////////////////
// UMorphNodeWeight
//////////////////////////////////////////////////////////////////////////

void UMorphNodeWeight::SetNodeWeight(FLOAT NewWeight)
{
	NodeWeight = NewWeight;
}

void UMorphNodeWeight::GetActiveMorphs(TArray<FActiveMorph>& OutMorphs)
{
	// If weight is low enough, do nothing (add no morph targets)
	if(NodeWeight < ZERO_MORPHWEIGHT_THRESH)
	{
		return;
	}

	// This node should only have one connector.
	check(NodeConns.Num() == 1);
	FMorphNodeConn& Conn = NodeConns(0);

	// Temp storage.
	TArray<FActiveMorph> TempMorphs;

	// Iterate over each link from this connector.
	for(INT j=0; j<Conn.ChildNodes.Num(); j++)
	{
		// If there is a child node, call GetActiveMorphs on it.
		if( Conn.ChildNodes(j) )
		{
			TempMorphs.Empty();
			Conn.ChildNodes(j)->GetActiveMorphs(TempMorphs);

			// Iterate over each active morph, scaling it by this nodes weight, and adding to output array.
			for(INT k=0; k<TempMorphs.Num(); k++)
			{
				OutMorphs.AddItem( FActiveMorph(TempMorphs(k).Target, TempMorphs(k).Weight * NodeWeight) );
			}
		}
	}
}

FLOAT UMorphNodeWeight::GetSliderPosition()
{
	return NodeWeight;
}

void UMorphNodeWeight::HandleSliderMove(FLOAT NewSliderValue)
{
	NodeWeight = NewSliderValue;
}


//////////////////////////////////////////////////////////////////////////
// UMorphNodePose
//////////////////////////////////////////////////////////////////////////

/** If someone changes name of animation - update to take affect. */
void UMorphNodePose::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SetMorphTarget(MorphName);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** 
 *	Set the MorphTarget to use for this MorphNodePose by name. 
 *	Will find it in the owning SkeletalMeshComponent MorphSets array using FindMorphTarget.
 */
void UMorphNodePose::SetMorphTarget(FName MorphTargetName)
{
	MorphName = MorphTargetName;
	Target = NULL;

	if(MorphTargetName == NAME_None || !SkelComponent)
	{
		return;
	}

	Target = SkelComponent->FindMorphTarget(MorphTargetName);
	if(!Target)
	{
#if !CONSOLE
		warnf(NAME_DevAnim, TEXT("%s - Failed to find MorphTarget '%s' on SkeletalMeshComponent: %s"),
			*GetName(),
			*MorphTargetName.ToString(),
			*SkelComponent->GetFullName()
			);
#endif
	}
}

void UMorphNodePose::GetActiveMorphs(TArray<FActiveMorph>& OutMorphs)
{
	// If we have a target, add it to the array.
	if( Target )
	{
		OutMorphs.AddItem( FActiveMorph(Target, Weight) );
		// @todo see if its already in there.
	}
}

void UMorphNodePose::InitMorphNode(USkeletalMeshComponent* InSkelComp)
{
	Super::InitMorphNode(InSkelComp);

	// On initialize, look up the morph target by the name we have saved.
	SetMorphTarget(MorphName);
}

/** 
 * Draws this morph node in the AnimTreeEditor.
 *
 * @param	Canvas			The canvas to use.
 * @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
 */
void UMorphNodePose::DrawMorphNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes)
{
#if WITH_EDITORONLY_DATA
	FLinkedObjDrawInfo ObjInfo;

	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("Out"), MorphConnColor ) );
	ObjInfo.ObjObject = this;

	UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor =  bSelected ? Morph_SelectedColor : FColor(0,0,0);
	const FColor FontColor = FColor( 255, 255, 128 );
	const FIntPoint Point(NodePosX, NodePosY);
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *MorphName.ToString(), NULL, FontColor, BorderColor, MorphConnColor, Point );

	DrawWidth = ObjInfo.DrawWidth;
	OutDrawY = ObjInfo.InputY(0);
#endif // WITH_EDITORONLY_DATA
}

/************************************************************************************
* UMorphNodeWeightByBoneAngle
***********************************************************************************/

/** Utility function to get an axis from a given matrix, using an EAxis enum */
static FVector GetMatrixAxis(FMatrix& Matrix, BYTE Axis)
{
	if( Axis == AXIS_X )
	{
		return Matrix.GetAxis(0);
	}
	else if( Axis == AXIS_Y )
	{
		return Matrix.GetAxis(1);
	}

	return Matrix.GetAxis(2);
}

static FVector GetBoneAtomAxis(FBoneAtom& BoneAtom, BYTE Axis)
{
	if( Axis == AXIS_X )
	{
		return BoneAtom.GetAxis(0);
	}
	else if( Axis == AXIS_Y )
	{
		return BoneAtom.GetAxis(1);
	}

	return BoneAtom.GetAxis(2);
}

/** Get bone's axis vector using existing SpaceBases array. */
static FVector GetBoneAxis(USkeletalMeshComponent* SkelComponent, const INT BoneIndex, const BYTE Axis, const UBOOL bInvert)
{
	INT MatrixAxis;

	// Convert Axis enum to matrix row.
	if( Axis == AXIS_X )
	{
		MatrixAxis = 0;
	}
	else if( Axis == AXIS_Y )
	{
		MatrixAxis = 1;
	}
	else
	{
		MatrixAxis = 2;
	}

	// Should we invert?
	if( bInvert )
	{
		return (SkelComponent->GetBoneMatrix(BoneIndex).GetAxis(MatrixAxis).SafeNormal() * -1.f);
	}

	return SkelComponent->GetBoneMatrix(BoneIndex).GetAxis(MatrixAxis).SafeNormal();
}

/** 
* Updates the node's weight based on the angle between the given 2 bones.
* It then scales its children morph targets by this weight and returns them.
*/
void UMorphNodeWeightByBoneAngle::GetActiveMorphs(TArray<FActiveMorph>& OutMorphs)
{
	if( !SkelComponent )
	{
		return;
	}

	// Make sure we have valid bone names
	const INT BaseBoneIndex		= SkelComponent->MatchRefBone(BaseBoneName);
	const INT AngleBoneIndex	= SkelComponent->MatchRefBone(AngleBoneName);

	if( BaseBoneIndex == INDEX_NONE || AngleBoneIndex == INDEX_NONE || 
		SkelComponent->SpaceBases.Num() <= BaseBoneIndex ||
		SkelComponent->SpaceBases.Num() <= AngleBoneIndex )
	{
		return;
	}

	// Figure out node's weight based on angle between 2 bones.
	const FVector BaseBoneDir	= GetBoneAxis(SkelComponent, BaseBoneIndex, BaseBoneAxis, bInvertBaseBoneAxis);
	const FVector AngleBoneDir	= GetBoneAxis(SkelComponent, AngleBoneIndex, AngleBoneAxis, bInvertAngleBoneAxis);

	// Figure out angle in degrees between the 2 bones.
	const FLOAT DotProduct	= Clamp<FLOAT>(AngleBoneDir | BaseBoneDir, -1.f, 1.f);
	const FLOAT RadAngle	= appAcos(DotProduct);
	Angle = RadAngle * 180.f / PI;

	// Figure out where we are in the Angle to Weight array.
	INT ArrayIndex = 0;
	const INT WeightArraySize = WeightArray.Num();
	while( ArrayIndex < WeightArraySize && WeightArray(ArrayIndex).Angle < Angle )
	{
		ArrayIndex++;
	}

	// Handle going beyond array size, or if array is empty.
	if( ArrayIndex >= WeightArraySize )
	{
		NodeWeight = (WeightArraySize > 0) ? WeightArray(WeightArraySize-1).TargetWeight : 0.f;
	}
	// If we're in between 2 valid key angles, then perform linear interpolation in between these.
	else if( ArrayIndex > 0 && 
		WeightArray(ArrayIndex).Angle > Angle && 
		WeightArray(ArrayIndex).Angle > WeightArray(ArrayIndex-1).Angle )
	{
		const FLOAT Alpha = (Angle - WeightArray(ArrayIndex-1).Angle) / (WeightArray(ArrayIndex).Angle - WeightArray(ArrayIndex-1).Angle);
		NodeWeight = Lerp(WeightArray(ArrayIndex-1).TargetWeight, WeightArray(ArrayIndex).TargetWeight, Alpha);
	}
	// Otherwise, just return the current angle's corresponding weight
	else
	{
		NodeWeight = WeightArray(ArrayIndex).TargetWeight;
	}

	// Support for Material Parameters
	if( bControlMaterialParameter )
	{
		UMaterialInterface* MaterialInterface = SkelComponent->GetMaterial(MaterialSlotId);

		// See if we need to update the MaterialInstanceConstant reference
		if( MaterialInterface != MaterialInstanceConstant )
		{
			MaterialInstanceConstant = NULL;
			if( MaterialInterface && MaterialInterface->IsA(UMaterialInstanceConstant::StaticClass()) )
			{
				MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInterface);
			}

			if( !MaterialInstanceConstant && SkelComponent->SkeletalMesh )
			{
				if( MaterialSlotId < SkelComponent->SkeletalMesh->Materials.Num() && SkelComponent->SkeletalMesh->Materials(MaterialSlotId) )
				{
					if( SkelComponent->bDisableFaceFXMaterialInstanceCreation )
					{
						debugf(TEXT("UGearMorph_WeightByBoneAngle: WARNING Unable to create MaterialInstanceConstant because bDisableFaceFXMaterialInstanceCreation is true!"));
					}
					else
					{
						UMaterialInstanceConstant* NewMaterialInstanceConstant = CastChecked<UMaterialInstanceConstant>( UObject::StaticConstructObject(UMaterialInstanceConstant::StaticClass(), SkelComponent) );
						NewMaterialInstanceConstant->SetParent(SkelComponent->SkeletalMesh->Materials(MaterialSlotId));
						SkelComponent->SetMaterial(MaterialSlotId, NewMaterialInstanceConstant);
						MaterialInstanceConstant = NewMaterialInstanceConstant;
					}
				}
			}
		}

		// Set Scalar parameter value
		if( MaterialInstanceConstant )
		{
			MaterialInstanceConstant->SetScalarParameterValue(ScalarParameterName, NodeWeight);
		}
	}

	// If node weight is irrelevant, do nothing.
	if( NodeWeight < ZERO_ANIMWEIGHT_THRESH )
	{
		return;
	}

	// This node should only have one connector.
	check(NodeConns.Num() == 1);
	FMorphNodeConn& Conn = NodeConns(0);

	// Temp storage.
	TArray<FActiveMorph> TempMorphs;

	// Iterate over each link from this connector.
	for(INT j=0; j<Conn.ChildNodes.Num(); j++)
	{
		// If there is a child node, call GetActiveMorphs on it.
		if( Conn.ChildNodes(j) )
		{
			TempMorphs.Empty();
			Conn.ChildNodes(j)->GetActiveMorphs(TempMorphs);

			// Iterate over each active morph, scaling it by this node's weight, and adding to output array.
			for(INT k=0; k<TempMorphs.Num(); k++)
			{
				OutMorphs.AddItem( FActiveMorph(TempMorphs(k).Target, TempMorphs(k).Weight * NodeWeight) );
			}
		}
	}
}

/** Draw bone axices on viewport when node is selected */
void UMorphNodeWeightByBoneAngle::Render(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	// Make sure we have valid bone names
	const INT BaseBoneIndex		= SkelComponent->MatchRefBone(BaseBoneName);
	const INT AngleBoneIndex	= SkelComponent->MatchRefBone(AngleBoneName);

	if( BaseBoneIndex == INDEX_NONE || AngleBoneIndex == INDEX_NONE || 
		SkelComponent->SpaceBases.Num() <= BaseBoneIndex ||
		SkelComponent->SpaceBases.Num() <= AngleBoneIndex )
	{
		return;
	}

	FStaticLODModel& LODModel = SkelComponent->SkeletalMesh->LODModels(SkelComponent->PredictedLODLevel);
	for(INT i=0; i<LODModel.RequiredBones.Num(); i++)
	{
		const INT BoneIndex = LODModel.RequiredBones(i);

		if( BoneIndex == BaseBoneIndex || BoneIndex == AngleBoneIndex )
		{
			const FVector	LocalBonePos	= SkelComponent->SpaceBases(BoneIndex).GetOrigin();
			const FVector	BonePos			= SkelComponent->LocalToWorld.TransformFVector(LocalBonePos);

			// Draw coord system at each bone
			PDI->DrawLine( BonePos, SkelComponent->LocalToWorld.TransformFVector( LocalBonePos + 3.75f * SkelComponent->SpaceBases(BoneIndex).GetAxis(0) ), FColor(255,0,0), SDPG_Foreground );
			PDI->DrawLine( BonePos, SkelComponent->LocalToWorld.TransformFVector( LocalBonePos + 3.75f * SkelComponent->SpaceBases(BoneIndex).GetAxis(1) ), FColor(0,255,0), SDPG_Foreground );
			PDI->DrawLine( BonePos, SkelComponent->LocalToWorld.TransformFVector( LocalBonePos + 3.75f * SkelComponent->SpaceBases(BoneIndex).GetAxis(2) ), FColor(0,0,255), SDPG_Foreground );

			// Draw axis considered for angle
			if( BoneIndex == BaseBoneIndex )
			{
				const FLOAT		Length = SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position.Size() * (bInvertBaseBoneAxis ? -1.f : 1.f);
				const FVector	Extent = GetBoneAtomAxis(SkelComponent->SpaceBases(BoneIndex), BaseBoneAxis) * Length;
				PDI->DrawLine(BonePos, SkelComponent->LocalToWorld.TransformFVector(LocalBonePos + Extent), FColor(255,255,255), SDPG_Foreground);
			}
			else if( BoneIndex == AngleBoneIndex )
			{
				const FLOAT		Length = SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position.Size() * (bInvertAngleBoneAxis ? -1.f : 1.f);
				const FVector	Extent = GetBoneAtomAxis(SkelComponent->SpaceBases(BoneIndex), AngleBoneAxis) * Length;
				PDI->DrawLine(BonePos, SkelComponent->LocalToWorld.TransformFVector(LocalBonePos + Extent), FColor(255,255,255), SDPG_Foreground);
			}
		}
	}
}


/** Draw angle between bones and node's weight on viewport */
void UMorphNodeWeightByBoneAngle::Draw(FViewport* Viewport, FCanvas* Canvas, const FSceneView* View)
{
	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	// Make sure we have valid bone names
	const INT BaseBoneIndex		= SkelComponent->MatchRefBone(BaseBoneName);
	const INT AngleBoneIndex	= SkelComponent->MatchRefBone(AngleBoneName);

	if( BaseBoneIndex == INDEX_NONE || AngleBoneIndex == INDEX_NONE || 
		SkelComponent->SpaceBases.Num() <= BaseBoneIndex ||
		SkelComponent->SpaceBases.Num() <= AngleBoneIndex )
	{
		return;
	}

	const INT HalfX = Viewport->GetSizeX()/2;
	const INT HalfY = Viewport->GetSizeY()/2;

	FStaticLODModel& LODModel = SkelComponent->SkeletalMesh->LODModels(SkelComponent->PredictedLODLevel);
	for(INT i=0; i<LODModel.RequiredBones.Num(); i++)
	{
		const INT BoneIndex = LODModel.RequiredBones(i);
		if( BoneIndex == AngleBoneIndex )
		{
			const FVector	LocalBonePos	= SkelComponent->SpaceBases(BoneIndex).GetOrigin();
			const FVector	BonePos			= SkelComponent->LocalToWorld.TransformFVector(LocalBonePos);
			const FPlane	proj			= View->Project(BonePos);

			if( proj.W > 0.f )
			{
				const INT XPos = appTrunc(HalfX + ( HalfX * proj.X ));
				const INT YPos = appTrunc(HalfY + ( HalfY * (proj.Y * -1) ));

				const FString BoneString	= FString::Printf( TEXT("Angle: %3.0f, Weight %1.2f"), Angle, NodeWeight);

				DrawString(Canvas, XPos, YPos, *BoneString, GEngine->SmallFont, FColor(255,255,255));
			}
		}
	}
}

/************************************************************************************
 * UMorphNodeWeightByBoneRotation
 ***********************************************************************************/

/** Helper function to get rotation axises from a Quaternion */
static FVector GetQuatAxis(const FQuat& Quat, BYTE Axis, bool bInvertAxis)
{
	FVector Result;
	switch( Axis )
	{
		default:
		case AXIS_X : Result = Quat.GetAxisX(); break;
		case AXIS_Y : Result = Quat.GetAxisY(); break;
		case AXIS_Z : Result = Quat.GetAxisZ(); break;
	}

	if( bInvertAxis )
	{
		Result *= -1.f;
	}

	return Result;
}

/** Draw bone axises on viewport when node is selected */
void UMorphNodeWeightByBoneRotation::Render(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	// Make sure we have valid bone names
	const INT BoneIndex	= SkelComponent->MatchRefBone(BoneName);
	if( BoneIndex == INDEX_NONE || SkelComponent->SpaceBases.Num() <= BoneIndex  )
	{
		return;
	}
	
	// Rotate parent bone atom from position in local space to reference skeleton
	// Since our rotation rotates both vectors with shortest arc
	// we're essentially left with a quaternion that has roll angle difference with reference skeleton version
	const FQuat BoneQuatAligned = GetAlignedQuat(BoneIndex);

	const INT ParentBoneIndex = SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
	const FBoneAtom	RefBoneAtom(SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation, SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position);

	DrawDebugCoordSystem(PDI, FColor(255,0,0), RefBoneAtom, SkelComponent->SpaceBases(ParentBoneIndex));

	const FBoneAtom	AlignedBoneAtom(BoneQuatAligned, SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Position);
	DrawDebugCoordSystem(PDI, FColor(0,255,0), AlignedBoneAtom, SkelComponent->SpaceBases(ParentBoneIndex));
}

void UMorphNodeWeightByBoneRotation::DrawDebugCoordSystem(FPrimitiveDrawInterface* PDI, FColor Color, const FBoneAtom& LocalBoneAtom, const FBoneAtom& ParentBoneTransform)
{
	const FBoneAtom BoneTransform	= LocalBoneAtom * ParentBoneTransform;
	const FVector	BonePosLocal	= BoneTransform.GetOrigin();
	const FVector	BonePos			= SkelComponent->LocalToWorld.TransformFVector(BonePosLocal);

	// Draw coord system at each bone
	PDI->DrawLine( BonePos, SkelComponent->LocalToWorld.TransformFVector( BonePosLocal + 3.75f * BoneTransform.GetAxis(0) ), Color, SDPG_Foreground );
	PDI->DrawLine( BonePos, SkelComponent->LocalToWorld.TransformFVector( BonePosLocal + 3.75f * BoneTransform.GetAxis(1) ), Color, SDPG_Foreground );
	PDI->DrawLine( BonePos, SkelComponent->LocalToWorld.TransformFVector( BonePosLocal + 3.75f * BoneTransform.GetAxis(2) ), Color, SDPG_Foreground );
}

FQuat UMorphNodeWeightByBoneRotation::GetAlignedQuat(INT BoneIndex)
{
	// Current Bone Atom in local space.
	const FBoneAtom BoneAtom = SkelComponent->LocalAtoms(BoneIndex);
	// Reference Bone Rotation
	const FQuat RefQuat = SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation;

	// find delta angle between the two quaternions X Axis.
	const FVector BoneAtomAxis = GetQuatAxis(BoneAtom.GetRotation(), BoneAxis, bInvertBoneAxis);
	const FVector BoneAtomRefAxis = GetQuatAxis(RefQuat, BoneAxis, bInvertBoneAxis);
	const FQuat LocalToRefQuat = FQuatFindBetween(BoneAtomAxis, BoneAtomRefAxis);

	// Rotate parent bone atom from position in local space to reference skeleton
	// Since our rotation rotates both vectors with shortest arc
	// we're essentially left with a quaternion that has roll angle difference with reference skeleton version
	const FQuat BoneQuatAligned = LocalToRefQuat * BoneAtom.GetRotation();
	checkSlow( BoneQuatAligned.IsNormalized() );

#if FALSE
	// make sure we have actually aligned those two axises with each other.
	FVector DeltaAxis = BoneAtomRefAxis - GetQuatAxis(BoneQuatAligned, BoneAxis, bInvertBoneAxis);
	if( !DeltaAxis.IsNearlyZero(0.01f) )
	{
		// Useful information when debugging.
		FMatrix RefMat = FBoneAtom(RefQuat.Rotator(), FVector(0.f)).ToMatrix();
		FMatrix BoneMat = FBoneAtom(BoneAtom.GetRotation().Rotator(), FVector(0.f)).ToMatrix();
		FMatrix AlignedMat = FBoneAtom(BoneQuatAligned.Rotator(), FVector(0.f)).ToMatrix();
		FMatrix DeltaMat = FBoneAtom(LocalToRefQuat.Rotator(), FVector(0.f)).ToMatrix();
		FVector AlignedAxis = GetQuatAxis(BoneQuatAligned, BoneAxis, bInvertBoneAxis);
	}
#endif

	return BoneQuatAligned;
}

/** 
 * Updates the node's weight based on the angle between the given 2 bones.
 * It then scales its children morph targets by this weight and returns them.
 */
void UMorphNodeWeightByBoneRotation::GetActiveMorphs(TArray<FActiveMorph>& OutMorphs)
{
	if( !SkelComponent )
	{
		return;
	}

	// Make sure we have valid bone names
	const INT BoneIndex	= SkelComponent->MatchRefBone(BoneName);
	if( BoneIndex == INDEX_NONE || SkelComponent->SpaceBases.Num() <= BoneIndex  )
	{
		return;
	}
	
	// Rotate parent bone atom from position in local space to reference skeleton
	// Since our rotation rotates both vectors with shortest arc
	// we're essentially left with a quaternion that has roll angle difference with reference skeleton version
	const FQuat BoneQuatAligned = GetAlignedQuat(BoneIndex);
	checkSlow( BoneQuatAligned.IsNormalized() );

	// Reference Bone Rotation
	const FQuat RefQuat = SkelComponent->SkeletalMesh->RefSkeleton(BoneIndex).BonePos.Orientation;

	// Find that delta angle between animated pose and Ref Pose.
	FQuat DeltaQuat = (-RefQuat) * BoneQuatAligned;
	checkSlow( DeltaQuat.IsNormalized() );

	// Turn to Axis and Angle
	FVector Axis;
	DeltaQuat.ToAxisAndAngle(Axis, Angle);

	FVector DefaultAxis;
	switch( BoneAxis )
	{
		default:
		case AXIS_X : DefaultAxis = FVector(1.f,0.f,0.f);
		case AXIS_Y : DefaultAxis = FVector(0.f,1.f,0.f);
		case AXIS_Z : DefaultAxis = FVector(0.f,0.f,1.f);
	}

	if( bInvertBoneAxis )
	{
		DefaultAxis *= -1.f;
	}

	// See if we need to invert angle.
	if( (Axis | DefaultAxis) < 0.f )
	{
		Angle = -Angle;
	}

	// Make sure it is the shortest angle
	Angle = UnwindHeading(Angle);

	// Figure out the weight of the morph target
	// Go through all the defined angles and their corresponding weight
	// Find upper and lower bounds.
	FVector2D AngleRange(-BIG_NUMBER,+BIG_NUMBER);
	INT	IndexLow = INDEX_NONE;
	INT IndexHigh = INDEX_NONE;

	for(INT i=0; i<WeightArray.Num(); i++)
	{
		const FLOAT DeltaAngle = UnwindHeading( (WeightArray(i).Angle * PI / 180.f) - Angle );
		if( DeltaAngle >= 0.f )
		{
			if( DeltaAngle < AngleRange.Y )
			{
				AngleRange.Y = DeltaAngle;
				IndexHigh = i;
			}
		}
		else // if( DeltaAngle < 0.f )
		{
			if( DeltaAngle > AngleRange.X )
			{
				AngleRange.X = DeltaAngle;
				IndexLow = i;
			}
		}
	}

	// Make sure we have valid results.
	if( IndexLow == INDEX_NONE && IndexHigh == INDEX_NONE )
	{
		NodeWeight = 0.f;
	}
	else if( IndexLow == INDEX_NONE  )
	{
		NodeWeight = WeightArray(IndexHigh).TargetWeight;
	}
	else if( IndexHigh == INDEX_NONE )
	{
		NodeWeight = WeightArray(IndexLow).TargetWeight;
	}
	else
	{
		// Set weights on two animations we're blending
		const FLOAT TotalDist = AngleRange.Y - AngleRange.X;
		const FLOAT LowerWeight = WeightArray(IndexLow).TargetWeight * (TotalDist + AngleRange.X) / TotalDist;
		const FLOAT HigherWeight = WeightArray(IndexHigh).TargetWeight * (TotalDist - AngleRange.Y) / TotalDist;
		NodeWeight = LowerWeight + HigherWeight;
	}

	// Support for Material Parameters
	if( bControlMaterialParameter )
	{
		UMaterialInterface* MaterialInterface = SkelComponent->GetMaterial(MaterialSlotId);

		// See if we need to update the MaterialInstanceConstant reference
		if( MaterialInterface != MaterialInstanceConstant )
		{
			MaterialInstanceConstant = NULL;
			if( MaterialInterface && MaterialInterface->IsA(UMaterialInstanceConstant::StaticClass()) )
			{
				MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(MaterialInterface);
			}

			if( !MaterialInstanceConstant && SkelComponent->SkeletalMesh )
			{
				if( MaterialSlotId < SkelComponent->SkeletalMesh->Materials.Num() && SkelComponent->SkeletalMesh->Materials(MaterialSlotId) )
				{
					if( SkelComponent->bDisableFaceFXMaterialInstanceCreation )
					{
						debugf(TEXT("UGearMorph_WeightByBoneAngle: WARNING Unable to create MaterialInstanceConstant because bDisableFaceFXMaterialInstanceCreation is true!"));
					}
					else
					{
						UMaterialInstanceConstant* NewMaterialInstanceConstant = CastChecked<UMaterialInstanceConstant>( UObject::StaticConstructObject(UMaterialInstanceConstant::StaticClass(), SkelComponent) );
						NewMaterialInstanceConstant->SetParent(SkelComponent->SkeletalMesh->Materials(MaterialSlotId));
						INT NumMaterials = SkelComponent->Materials.Num();
						if( NumMaterials <= MaterialSlotId )
						{
							SkelComponent->Materials.AddZeroed(MaterialSlotId + 1 - NumMaterials);
						}
						SkelComponent->Materials(MaterialSlotId) = NewMaterialInstanceConstant;
						MaterialInstanceConstant = NewMaterialInstanceConstant;
					}
				}
			}
		}

		// Set Scalar parameter value
		if( MaterialInstanceConstant )
		{
			MaterialInstanceConstant->SetScalarParameterValue(ScalarParameterName, NodeWeight);
		}
	}

	// If node weight is irrelevant, do nothing.
	if( NodeWeight < ZERO_ANIMWEIGHT_THRESH )
	{
		return;
	}

	// This node should only have one connector.
	check(NodeConns.Num() == 1);
	FMorphNodeConn& Conn = NodeConns(0);

	// Temp storage.
	TArray<FActiveMorph> TempMorphs;

	// Iterate over each link from this connector.
	for(INT j=0; j<Conn.ChildNodes.Num(); j++)
	{
		// If there is a child node, call GetActiveMorphs on it.
		if( Conn.ChildNodes(j) )
		{
			TempMorphs.Empty();
			Conn.ChildNodes(j)->GetActiveMorphs(TempMorphs);

			// Iterate over each active morph, scaling it by this node's weight, and adding to output array.
			for(INT k=0; k<TempMorphs.Num(); k++)
			{
				OutMorphs.AddItem( FActiveMorph(TempMorphs(k).Target, TempMorphs(k).Weight * NodeWeight) );
			}
		}
	}
}

/** Draw angle between bones and node's weight on viewport */
void UMorphNodeWeightByBoneRotation::Draw(FViewport* Viewport, FCanvas* Canvas, const FSceneView* View)
{
	if( !SkelComponent || !SkelComponent->SkeletalMesh )
	{
		return;
	}

	// Make sure we have valid bone names
	const INT AngleBoneIndex	= SkelComponent->MatchRefBone(BoneName);

	if( AngleBoneIndex == INDEX_NONE || SkelComponent->SpaceBases.Num() <= AngleBoneIndex )
	{
		return;
	}

	const INT HalfX = Viewport->GetSizeX()/2;
	const INT HalfY = Viewport->GetSizeY()/2;

	FStaticLODModel& LODModel = SkelComponent->SkeletalMesh->LODModels(SkelComponent->PredictedLODLevel);
	for(INT i=0; i<LODModel.RequiredBones.Num(); i++)
	{
		const INT BoneIndex = LODModel.RequiredBones(i);
		if( BoneIndex == AngleBoneIndex )
		{
			const FVector	LocalBonePos	= SkelComponent->SpaceBases(BoneIndex).GetOrigin();
			const FVector	BonePos			= SkelComponent->LocalToWorld.TransformFVector(LocalBonePos);
			const FPlane	proj			= View->Project(BonePos);

			if( proj.W > 0.f )
			{
				const INT XPos = appTrunc(HalfX + ( HalfX * proj.X ));
				const INT YPos = appTrunc(HalfY + ( HalfY * (proj.Y * -1) ));

				const FString BoneString = FString::Printf( TEXT("Angle: %3.0f, Weight %1.2f"), Angle * 180.f / PI, NodeWeight);
				DrawString(Canvas, XPos, YPos, *BoneString, GEngine->SmallFont, FColor(255,255,255));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UMorphNodeMultiPose
//////////////////////////////////////////////////////////////////////////

/** If someone changes name of animation - update to take affect. */
void UMorphNodeMultiPose::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshMorphTargets();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** 
*	Add the MorphTarget to use for this MorphNodeMultiPose by name. 
*	Will find it in the owning SkeletalMeshComponent MorphSets array using FindMorphTarget.
*/
UBOOL UMorphNodeMultiPose::AddMorphTarget(FName MorphTargetName, FLOAT InWeight)
{
	check (Targets.Num() == MorphNames.Num());

	if(MorphTargetName == NAME_None || !SkelComponent)
	{
		return FALSE;
	}

	// If exists, just update weight
	INT Exists = ExistsIn(MorphTargetName);
	if ( Exists >= 0 )
	{
		UpdateMorphTarget( Targets(Exists), InWeight );
		return TRUE;
	}

	// Look for target. If found, add it
	UMorphTarget * Found = SkelComponent->FindMorphTarget(MorphTargetName);
	if(Found)
	{
		MorphNames.AddItem(MorphTargetName);
		Targets.AddItem(Found);
	}
	else
	{
		warnf(NAME_DevAnim, TEXT("%s - Failed to find MorphTarget '%s' on SkeletalMeshComponent: %s"),
			*GetName(),
			*MorphTargetName.ToString(),
			*SkelComponent->GetFullName()
			);

		return FALSE;
	}

	return TRUE;
}

/** Returns active morphs */
void UMorphNodeMultiPose::GetActiveMorphs(TArray<FActiveMorph>& OutMorphs)
{
	// If we have a target, add it to the array.
	for ( INT I=0; I<Targets.Num(); ++I )
	{
		if ( Targets(I) )
		{
			OutMorphs.AddItem( FActiveMorph(Targets(I), GetMorphTargetWeight(I)) );
		}
	}
}

void UMorphNodeMultiPose::InitMorphNode(USkeletalMeshComponent* InSkelComp)
{
	Super::InitMorphNode(InSkelComp);

	// On initialize, look up the morph target by the name we have saved.
	RefreshMorphTargets();
}

/** 
* Draws this morph node in the AnimTreeEditor.
*
* @param	Canvas			The canvas to use.
* @param	SelectedNodes	Reference to array of all currently selected nodes, potentially including this node
*/
void UMorphNodeMultiPose::DrawMorphNode(FCanvas* Canvas, const TArray<UAnimObject*>& SelectedNodes)
{
#if WITH_EDITORONLY_DATA
	// TODO: ls - This is just displaying first one for now. 
	FLinkedObjDrawInfo ObjInfo;

	ObjInfo.Inputs.AddItem( FLinkedObjConnInfo(TEXT("Out"), MorphConnColor ) );
	ObjInfo.ObjObject = this;

	FName MorphName = (MorphNames.Num()>0)? MorphNames(0):NAME_None;
	const UBOOL bSelected = SelectedNodes.ContainsItem( this );
	const FColor& BorderColor = bSelected ? Morph_SelectedColor : FColor(0,0,0);
	const FColor FontColor = FColor( 255, 255, 128 );
	const FIntPoint Point(NodePosX, NodePosY);
	FLinkedObjDrawUtils::DrawLinkedObj( Canvas, ObjInfo, *MorphName.ToString(), NULL, FontColor, BorderColor, MorphConnColor, Point );

	DrawWidth = ObjInfo.DrawWidth;
	OutDrawY = ObjInfo.InputY(0);
#endif // WITH_EDITORONLY_DATA
}

/** 
*	Remove the MorphTarget from using for this MorphNodeMultiPose by name. 
*	Will find it in the owning SkeletalMeshComponent MorphSets array using FindMorphTarget.
*/
void UMorphNodeMultiPose::RemoveMorphTarget(FName MorphTargetName)
{
	check ( Targets.Num() == MorphNames.Num() );

	INT Exists = ExistsIn(MorphTargetName);
	if ( Exists >= 0 ) 
	{
		Targets.Remove( Exists );
		MorphNames.Remove(Exists);

		if (Weights.Num() > Exists)
		{
			Weights.Remove( Exists );
		}
	}
}

/** 
*	Update weight of the morph target 
*/
UBOOL UMorphNodeMultiPose::UpdateMorphTarget(class UMorphTarget* Target,FLOAT InWeight)
{
	if ( Target )
	{
		INT Exists = ExistsIn(Target);
		if ( Exists >= 0 )
		{
			if (Weights.Num() > Exists)
			{
				Weights(Exists) = InWeight;
			}
			else
			{
				// Need to add extra array to fill up
				INT Num = Weights.Num();
				INT NumToAdd = (Exists + 1) - Weights.Num();
				Weights.Add(NumToAdd);

				check (Weights.Num()-1 == Exists);

				for (INT I=Num; I<Weights.Num()-1; ++I)
				{
					// default weight
					Weights(I) = 0.f;
				}

				Weights(Exists) = InWeight;
			}

			return TRUE;
		}
	}

	return FALSE;
}

/** 
*	Clear all names and weights
*/
void UMorphNodeMultiPose::ClearAll()
{
	MorphNames.Empty();
	Weights.Empty();
	Targets.Empty();
}

/**
* If exists, it returns Index in the array
* Returns -1 if fails
*/
INT UMorphNodeMultiPose::ExistsIn(const FName & InName)
{
	for ( INT I=0; I<MorphNames.Num(); ++I )
	{
		if ( MorphNames(I) == InName )
		{
			return I;
		}
	}

	return -1;
}

/**
* If exists, it returns Index in the array
* Returns -1 if fails
*/
INT UMorphNodeMultiPose::ExistsIn(const UMorphTarget * InTarget)
{
	for ( INT I=0; I<Targets.Num(); ++I )
	{
		if ( Targets(I) == InTarget )
		{
			return I;
		}
	}

	return -1;

}

/**
* Refresh all morph target information from internal data(name/weights)
*/
void UMorphNodeMultiPose::RefreshMorphTargets()
{
	if( !SkelComponent || MorphNames.Num() <= 0 )
	{
		return;
	}

	// clears targets and refresh all data
	Targets.Empty();
	Targets.Add(MorphNames.Num());

	for( INT I=0; I<MorphNames.Num(); ++I )
	{
		if( MorphNames(I) == NAME_None )
		{
			Targets(I) = NULL;
			continue;
		}

		// Look for target. If found, add it
		UMorphTarget * Found = SkelComponent->FindMorphTarget(MorphNames(I));
		if(Found)
		{
			Targets(I) = Found;
		}
		else
		{
			warnf(NAME_DevAnim, TEXT("%s - Failed to find MorphTarget '%s' on SkeletalMeshComponent: %s"),
				*GetName(),
				*MorphNames(I).ToString(),
				*SkelComponent->GetFullName()
				);

			Targets(I) = NULL;
		}
	}
}
