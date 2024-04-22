/*
* Copyright 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
  Implementation of animation export related functionality from FbxExporter
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX

#include "UnFbxExporter.h"
#include "EngineAnimClasses.h"

namespace UnFbx
{


void CFbxExporter::ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq,
									 const USkeletalMesh* SkelMesh,
									 TArray<FbxNode*>& BoneNodes,
									 FbxAnimLayer* AnimLayer,
									 FLOAT AnimStartOffset,
									 FLOAT AnimEndOffset,
									 FLOAT AnimPlayRate,
									 FLOAT StartTime,
									 UBOOL bLooping)
{
	const UAnimSet* AnimSet = AnimSeq->GetAnimSet();

	// Add the animation data to the bone nodes
	for(INT BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* CurrentBoneNode = BoneNodes(BoneIndex);

		// Create the AnimCurves
		FbxAnimCurve* Curves[6];
		Curves[0] = CurrentBoneNode->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[1] = CurrentBoneNode->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[2] = CurrentBoneNode->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		Curves[3] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[4] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[5] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		FLOAT AnimTime					= AnimStartOffset;
		FLOAT AnimEndTime				= AnimSeq->SequenceLength - AnimEndOffset;
		const FLOAT AnimTimeIncrement	= AnimSeq->SequenceLength / AnimSeq->NumFrames * AnimPlayRate;

		FbxTime ExportTime;
		ExportTime.SetSecondDouble(StartTime);

		FbxTime ExportTimeIncrement;
		ExportTimeIncrement.SetSecondDouble(AnimTimeIncrement);

		INT BoneTrackIndex = AnimSet->FindTrackWithName(SkelMesh->RefSkeleton(BoneIndex).Name);
		if(BoneTrackIndex == INDEX_NONE)
		{
			// If this sequence does not have a track for the current bone, then skip it
			continue;
		}

		for(INT i = 0; i < 6; ++i)
		{
			Curves[i]->KeyModifyBegin();
		}

		UBOOL bLastKey = FALSE;
		// Step through each frame and add the bone's transformation data
		while (AnimTime < AnimEndTime)
		{
			FBoneAtom BoneAtom;
			AnimSeq->GetBoneAtom(BoneAtom, BoneTrackIndex, AnimTime, bLooping, TRUE);

			FbxVector4 Translation = Converter.ConvertToFbxPos(BoneAtom.GetTranslation());

			FbxVector4 Rotation;
			if(BoneIndex)
			{
				Rotation = Converter.ConvertToFbxAnimRot(BoneAtom.GetRotation());
			}
			else
			{
				Rotation = Converter.ConvertToFbxRot(BoneAtom.GetRotation().Euler());
			}

			INT lKeyIndex;

			AnimTime += AnimTimeIncrement;

			bLastKey = AnimTime >= AnimEndTime;
			for(INT i = 0, j=3; i < 3; ++i, ++j)
			{
				lKeyIndex = Curves[i]->KeyAdd(ExportTime);
				Curves[i]->KeySetValue(lKeyIndex, Translation[i]);
				Curves[i]->KeySetInterpolation(lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic);

				if( bLastKey )
				{
					Curves[i]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
				}

				lKeyIndex = Curves[j]->KeyAdd(ExportTime);
				Curves[j]->KeySetValue(lKeyIndex, Rotation[i]);
				Curves[j]->KeySetInterpolation(lKeyIndex, bLastKey ? FbxAnimCurveDef::eInterpolationConstant : FbxAnimCurveDef::eInterpolationCubic);

				if( bLastKey )
				{
					Curves[j]->KeySetConstantMode( lKeyIndex, FbxAnimCurveDef::eConstantStandard );
				}
			}

			ExportTime += ExportTimeIncrement;
		
		
		}

		for(INT i = 0; i < 6; ++i)
		{
			Curves[i]->KeyModifyEnd();
		}
	}
}


// The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
// will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
// rotation tracks to convert the angles into a more interpolation-friendly format.  
void CFbxExporter::CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer )
{
	// Add the animation data to the bone nodes
	for(INT BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
	{
		FbxNode* CurrentBoneNode = BoneNodes(BoneIndex);

		// Fetch the AnimCurves
		FbxAnimCurve* Curves[3];
		Curves[0] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		Curves[1] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		Curves[2] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		for(INT CurveIndex = 0; CurveIndex < 3; ++CurveIndex)
		{
			FbxAnimCurve* CurrentCurve = Curves[CurveIndex];

			FLOAT CurrentAngleOffset = 0.f;
			for(INT KeyIndex = 1; KeyIndex < CurrentCurve->KeyGetCount(); ++KeyIndex)
			{
				FLOAT PreviousOutVal	= CurrentCurve->KeyGetValue( KeyIndex-1 );
				FLOAT CurrentOutVal		= CurrentCurve->KeyGetValue( KeyIndex );

				FLOAT DeltaAngle = (CurrentOutVal + CurrentAngleOffset) - PreviousOutVal;

				if(DeltaAngle >= 180)
				{
					CurrentAngleOffset -= 360;
				}
				else if(DeltaAngle <= -180)
				{
					CurrentAngleOffset += 360;
				}

				CurrentOutVal += CurrentAngleOffset;

				CurrentCurve->KeySetValue(KeyIndex, CurrentOutVal);
			}
		}
	}
}


void CFbxExporter::ExportAnimSequence( const UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, UBOOL bExportSkelMesh )
{
	if( Scene == NULL || AnimSeq == NULL || SkelMesh == NULL )
	{
 		return;
	}

#if UDK
	// Do not allow content that has been stripped to be exported
	if( AnimSeq->GetOutermost()->PackageFlags & PKG_NoExportAllowed || AnimSeq->RawAnimationData.Num() == 0 
		|| ( bExportSkelMesh && ( SkelMesh->GetOutermost()->PackageFlags & PKG_NoExportAllowed ) ) )
	{
		warnf(NAME_Warning, TEXT("Source data missing for '%s'"), *AnimSeq->GetName());
		appMsgf( AMT_OK, *FString::Printf( *LocalizeUnrealEd("Exporter_Error_SourceDataUnavailable")) );
		return;
	}
#endif

	FbxString NodeName("BaseNode");

	FbxNode* BaseNode = FbxNode::Create(Scene, NodeName);
	Scene->GetRootNode()->AddChild(BaseNode);

	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(*SkelMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);


	// Export the anim sequence
	{
		ExportAnimSequenceToFbx(AnimSeq,
			SkelMesh,
			BoneNodes,
			AnimLayer,
			0.f,		// AnimStartOffset
			0.f,		// AnimEndOffset
			1.f,		// AnimPlayRate
			0.f,		// StartTime
			FALSE);		// bLooping

		CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);
	}


	// Optionally export the mesh
	if(bExportSkelMesh)
	{
		FString MeshName;
		SkelMesh->GetName(MeshName);

		// Add the mesh
		FbxNode* MeshRootNode = CreateMesh(*SkelMesh, *MeshName);
		if(MeshRootNode)
		{
			BaseNode->AddChild(MeshRootNode);
		}

		if(SkeletonRootNode && MeshRootNode)
		{
			// Bind the mesh to the skeleton
			BindMeshToSkeleton(*SkelMesh, MeshRootNode, BoneNodes);

			// Add the bind pose
			CreateBindPose(MeshRootNode);
		}
	}
}


void CFbxExporter::ExportAnimSequencesAsSingle( USkeletalMesh* SkelMesh, const ASkeletalMeshActor* SkelMeshActor, const FString& ExportName, const TArray<UAnimSequence*>& AnimSeqList, const TArray<struct FAnimControlTrackKey>& TrackKeys )
{
	if (Scene == NULL || SkelMesh == NULL || AnimSeqList.Num() == 0 || AnimSeqList.Num() != TrackKeys.Num()) return;

	FbxNode* BaseNode = FbxNode::Create(Scene, Converter.ConvertToFbxString(ExportName));
	Scene->GetRootNode()->AddChild(BaseNode);

	if( SkelMeshActor )
	{
		// Set the default position of the actor on the transforms
		// The UE3 transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		BaseNode->LclTranslation.Set(Converter.ConvertToFbxPos(SkelMeshActor->Location));
		BaseNode->LclRotation.Set(Converter.ConvertToFbxRot(SkelMeshActor->Rotation.Euler()));
		BaseNode->LclScaling.Set(Converter.ConvertToFbxScale(SkelMeshActor->DrawScale * SkelMeshActor->DrawScale3D));

	}

	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(*SkelMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);

	UBOOL bAnyObjectMissingSourceData = FALSE;
	FLOAT ExportStartTime = 0.f;
	for(INT AnimSeqIndex = 0; AnimSeqIndex < AnimSeqList.Num(); ++AnimSeqIndex)
	{
		const UAnimSequence* AnimSeq = AnimSeqList(AnimSeqIndex);
		const FAnimControlTrackKey& TrackKey = TrackKeys(AnimSeqIndex);

		// Shift the anim sequences so the first one is at time zero in the FBX file
		const FLOAT CurrentStartTime = TrackKey.StartTime - ExportStartTime;

#if UDK
		if( AnimSeq->GetOutermost()->PackageFlags & PKG_NoExportAllowed || AnimSeq->RawAnimationData.Num() == 0 )
		{
			bAnyObjectMissingSourceData = TRUE;
			warnf(NAME_Warning, TEXT("Source data missing for '%s'"), *AnimSeq->GetName());
			continue;
		}
#endif

		ExportAnimSequenceToFbx(AnimSeq,
			SkelMesh,
			BoneNodes,
			AnimLayer,
			TrackKey.AnimStartOffset,
			TrackKey.AnimEndOffset,
			TrackKey.AnimPlayRate,
			CurrentStartTime,
			TrackKey.bLooping);
	}

	CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);

	if (bAnyObjectMissingSourceData)
	{
		appMsgf( AMT_OK, *FString::Printf( *LocalizeUnrealEd("Exporter_Error_SourceDataUnavailable")) );
	}

}


/**
 * Exports all the animation sequences part of a single Group in a Matinee sequence
 * as a single animation in the FBX document.  The animation is created by sampling the
 * sequence at 30 updates/second and extracting the resulting bone transforms from the given
 * skeletal mesh
 */
void CFbxExporter::ExportMatineeGroup(class USeqAct_Interp* MatineeSequence, USkeletalMeshComponent* SkeletalMeshComponent)
{
	static const FLOAT SamplingRate = 1.f / 30.f;

	FLOAT MatineeLength = MatineeSequence->InterpData->InterpLength;

	if (Scene == NULL || MatineeSequence == NULL || SkeletalMeshComponent == NULL || MatineeLength == 0 || SkeletalMeshComponent->SkeletalMesh == NULL )
	{
		return;
	}

	FbxString NodeName("MatineeSequence");

	FbxNode* BaseNode = FbxNode::Create(Scene, NodeName);
	Scene->GetRootNode()->AddChild(BaseNode);
	AActor* Owner = SkeletalMeshComponent->GetOwner();
	if( Owner )
	{
		// Set the default position of the actor on the transforms
		// The UE3 transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		BaseNode->LclTranslation.Set(Converter.ConvertToFbxPos(Owner->Location));
		BaseNode->LclRotation.Set(Converter.ConvertToFbxRot( ( FRotationMatrix( Owner->Rotation ) * FRotationMatrix( SkeletalMeshComponent->SkeletalMesh->RotOrigin ) ).Rotator().Euler() ) );
		BaseNode->LclScaling.Set(Converter.ConvertToFbxScale(Owner->DrawScale * Owner->DrawScale3D));
	}


	// Create the Skeleton
	TArray<FbxNode*> BoneNodes;
	FbxNode* SkeletonRootNode = CreateSkeleton(*SkeletalMeshComponent->SkeletalMesh, BoneNodes);
	BaseNode->AddChild(SkeletonRootNode);

	FLOAT SampleTime;
	for(SampleTime = 0.f; SampleTime <= MatineeLength; SampleTime += SamplingRate)
	{
		// This will call UpdateSkelPose on the skeletal mesh component to move bones based on animations in the matinee group
		MatineeSequence->UpdateInterp( SampleTime, TRUE );

		FbxTime ExportTime;
		ExportTime.SetSecondDouble(SampleTime);

		// Add the animation data to the bone nodes
		for(INT BoneIndex = 0; BoneIndex < BoneNodes.Num(); ++BoneIndex)
		{
			FName BoneName = SkeletalMeshComponent->SkeletalMesh->RefSkeleton(BoneIndex).Name;
			FbxNode* CurrentBoneNode = BoneNodes(BoneIndex);

			// Create the AnimCurves
			FbxAnimCurve* Curves[6];
			Curves[0] = CurrentBoneNode->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			Curves[1] = CurrentBoneNode->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			Curves[2] = CurrentBoneNode->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			Curves[3] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			Curves[4] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			Curves[5] = CurrentBoneNode->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			for(INT i = 0; i < 6; ++i)
			{
				Curves[i]->KeyModifyBegin();
			}

			FBoneAtom BoneAtom = SkeletalMeshComponent->LocalAtoms(BoneIndex);
			FbxVector4 Translation = Converter.ConvertToFbxPos(BoneAtom.GetOrigin());
			FbxVector4 Rotation = Converter.ConvertToFbxRot(BoneAtom.GetRotation().Euler());

			int lKeyIndex;

			for(INT i = 0, j=3; i < 3; ++i, ++j)
			{
				lKeyIndex = Curves[i]->KeyAdd(ExportTime);
				Curves[i]->KeySetValue(lKeyIndex, Translation[i]);
				Curves[i]->KeySetInterpolation(lKeyIndex, FbxAnimCurveDef::eInterpolationCubic);

				lKeyIndex = Curves[j]->KeyAdd(ExportTime);
				Curves[j]->KeySetValue(lKeyIndex, Rotation[i]);
				Curves[j]->KeySetInterpolation(lKeyIndex, FbxAnimCurveDef::eInterpolationCubic);
			}

			for(INT i = 0; i < 6; ++i)
			{
				Curves[i]->KeyModifyEnd();
			}
		}
	}

	CorrectAnimTrackInterpolation(BoneNodes, AnimLayer);
}


} // namespace UnFbx

#endif //WITH_FBX
