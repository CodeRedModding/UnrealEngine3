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
	FBX importer for Unreal Engine 3.
==============================================================================*/

#include "UnrealEd.h"


#if WITH_FBX

#include "EngineAnimClasses.h"
#include "EngineInterpolationClasses.h"
#include "EngineSequenceClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "UnFbxImporter.h"

namespace UnFbx {

/**
 * Retrieves whether there are any unknown camera instances within the FBX document that the camera is not in Unreal scene.
 */
inline UBOOL _HasUnknownCameras( USeqAct_Interp* MatineeSequence, FbxNode* Node, const TCHAR* Name )
{
	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	if (Attr && Attr->GetAttributeType() == FbxNodeAttribute::eCAMERA)
	{
		// If we have a Matinee, try to name-match the node with a Matinee group name
		if( MatineeSequence != NULL && MatineeSequence->InterpData != NULL )
		{
			UInterpGroupInst* GroupInst = MatineeSequence->FindFirstGroupInstByName( FString( Name ) );
			if( GroupInst != NULL )
			{
				AActor * GrActor = GroupInst->GetGroupActor();
				// Make sure we have an actor
				if( GrActor != NULL &&
					GrActor->IsA( ACameraActor::StaticClass() ) )
				{
					// OK, we found an existing camera!
					return FALSE;
				}
			}
		}
		
		// Attempt to name-match the scene node for this camera with one of the actors.
		AActor* Actor = FindObject<AActor>( ANY_PACKAGE, Name );
		if ( Actor == NULL || Actor->bDeleteMe )
		{
			return TRUE;
		}
		else
		{
			// If you trigger this assertion, then you've got a name
			// clash between the FBX file and the UE3 level.
			check( Actor->IsA( ACameraActor::StaticClass() ) );
		}
	}
	
	return FALSE;
}

UBOOL CFbxImporter::HasUnknownCameras( USeqAct_Interp* MatineeSequence ) const
{
	if ( Scene == NULL )
	{
		return FALSE;
	}

	// check recursively
	FbxNode* RootNode = Scene->GetRootNode();
	INT NodeCount = RootNode->GetChildCount();
	for ( INT NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex )
	{
		FbxNode* Node = RootNode->GetChild(NodeIndex);
		if ( _HasUnknownCameras( MatineeSequence, Node, ANSI_TO_TCHAR(Node->GetName()) ) )
		{
			return TRUE;
		}

		// Look through children as well
		INT ChildNodeCount = Node->GetChildCount();
		for( INT ChildIndex = 0; ChildIndex < ChildNodeCount; ++ChildIndex )
		{
			FbxNode* ChildNode = Node->GetChild(ChildIndex);
			if( _HasUnknownCameras( MatineeSequence, ChildNode, ANSI_TO_TCHAR(ChildNode->GetName() ) ) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

UBOOL CFbxImporter::IsNodeAnimated(FbxNode* Node, FbxAnimLayer* AnimLayer)
{
	if (!AnimLayer)
	{
		FbxAnimStack* AnimStack = Scene->GetMember(FBX_TYPE(FbxAnimStack), 0);
		if (!AnimStack) return FALSE;

		AnimLayer = AnimStack->GetMember(FBX_TYPE(FbxAnimLayer), 0);
		if (AnimLayer == NULL) return FALSE;
	}
	
	// verify that the node is animated.
	UBOOL bIsAnimated = FALSE;
	FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);

	// translation animation
	FbxProperty TransProp = Node->LclTranslation;
	for (INT i = 0; i < TransProp.GetSrcObjectCount(FBX_TYPE(FbxAnimCurveNode)); i++)
	{
		FbxAnimCurveNode* CurveNode = FbxCast<FbxAnimCurveNode>(TransProp.GetSrcObject(FbxAnimCurveNode::ClassId, i));
		if (CurveNode && AnimLayer->IsConnectedSrcObject(CurveNode))
		{
			bIsAnimated |= (UBOOL)CurveNode->GetAnimationInterval(AnimTimeSpan);
			break;
		}
	}
	// rotation animation
	FbxProperty RotProp = Node->LclRotation;
	for (INT i = 0; IsAnimated == FALSE && i < RotProp.GetSrcObjectCount(FBX_TYPE(FbxAnimCurveNode)); i++)
	{
		FbxAnimCurveNode* CurveNode = FbxCast<FbxAnimCurveNode>(RotProp.GetSrcObject(FbxAnimCurveNode::ClassId, i));
		if (CurveNode && AnimLayer->IsConnectedSrcObject(CurveNode))
		{
			bIsAnimated |= (UBOOL)CurveNode->GetAnimationInterval(AnimTimeSpan);
		}
	}
	
	return bIsAnimated;
}

/** 
 * Finds a camera in the passed in node or any child nodes 
 * @return NULL if the camera is not found, a valid pointer if it is
 */
static FbxCamera* FindCamera( FbxNode* Parent )
{
	FbxCamera* Camera = Parent->GetCamera();
	if( !Camera )
	{
		INT NodeCount = Parent->GetChildCount();
		for ( INT NodeIndex = 0; NodeIndex < NodeCount && !Camera; ++NodeIndex )
		{
			FbxNode* Child = Parent->GetChild( NodeIndex );
			Camera = Child->GetCamera();
		}
	}

	return Camera;
}

void CFbxImporter::ImportMatineeSequence(USeqAct_Interp* MatineeSequence)
{
	if (Scene == NULL || MatineeSequence == NULL) return;
	
	// merge animation layer at first
	FbxAnimStack* AnimStack = Scene->GetMember(FBX_TYPE(FbxAnimStack), 0);
	if (!AnimStack) return;
		
	MergeAllLayerAnimation(AnimStack, FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()));

	FbxAnimLayer* AnimLayer = AnimStack->GetMember(FBX_TYPE(FbxAnimLayer), 0);
	if (AnimLayer == NULL) return;

	// If the Matinee editor is not open, we need to initialize the sequence.
	UBOOL InitializeMatinee = MatineeSequence->InterpData == NULL;
	if (InitializeMatinee)
	{
		// Force the initialization of the sequence
		// This sets the sequence in edition mode as well?
		MatineeSequence->InitInterp();
	}

	UInterpData* MatineeData = MatineeSequence->InterpData;
	FLOAT InterpLength = -1.0f;

	FbxNode* RootNode = Scene->GetRootNode();
	INT NodeCount = RootNode->GetChildCount();
	for (INT NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
	{
		FbxNode* Node = RootNode->GetChild(NodeIndex);

		AActor* Actor = NULL;

		// First check to see if the scene node name matches a Matinee group name
		UInterpGroupInst* FoundGroupInst =
			MatineeSequence->FindFirstGroupInstByName( FString( Node->GetName() ) );
		if( FoundGroupInst != NULL )
		{
			// OK, we found an actor bound to a Matinee group that matches this scene node name
			Actor = FoundGroupInst->GetGroupActor();
		}


		// Attempt to name-match the scene node with one of the actors.
		if( Actor == NULL )
		{
			Actor = FindObject<AActor>( ANY_PACKAGE, ANSI_TO_TCHAR(Node->GetName()) );
		}


		if ( Actor == NULL || Actor->bDeleteMe )
		{
			FbxCamera* CameraNode = FindCamera(Node);
			if ( bCreateUnknownCameras && CameraNode != NULL )
			{
				Actor = GWorld->SpawnActor( ACameraActor::StaticClass(), ANSI_TO_TCHAR(CameraNode->GetName()) );
			}
			else
			{
				continue;
			}
		}

		UInterpGroupInst* MatineeGroup = MatineeSequence->FindGroupInst(Actor);

		// Before attempting to create/import a movement track: verify that the node is animated.
		UBOOL IsAnimated = IsNodeAnimated(Node, AnimLayer);

		if (IsAnimated)
		{
			if (MatineeGroup == NULL)
			{
				MatineeGroup = CreateMatineeGroup(MatineeSequence, Actor, FString(Node->GetName()));
			}

			FLOAT TimeLength = ImportMatineeActor(Node, MatineeGroup);
			InterpLength = Max(InterpLength, TimeLength);
		}

		// Right now, cameras are the only supported import entity type.
		if (Actor->IsA(ACameraActor::StaticClass()))
		{
			// there is a pivot node between the FbxNode and node attribute
			FbxCamera* Camera = NULL;
			if ((FbxNode*)Node->GetChild(0))
			{
				Camera = ((FbxNode*)Node->GetChild(0))->GetCamera();
			}

			if(NULL == Camera)
			{
				Camera = FindCamera(Node);
			}

			if (Camera)
			{
				if (MatineeGroup == NULL)
				{
					MatineeGroup = CreateMatineeGroup(MatineeSequence, Actor, FString(Node->GetName()));
				}

				ImportCamera((ACameraActor*) Actor, MatineeGroup, Camera);
			}
		}

		if (MatineeGroup != NULL)
		{
			MatineeGroup->Modify();
		}
	}

	MatineeData->InterpLength = (InterpLength < 0.0f) ? 5.0f : InterpLength;
	MatineeSequence->Modify();

	if (InitializeMatinee)
	{
		MatineeSequence->TermInterp();
	}
}

void CFbxImporter::ImportCamera(ACameraActor* Actor, UInterpGroupInst* MatineeGroup, FbxCamera* FbxCamera)
{
	// Get the real camera node that stores customed camera attributes
	// Note: there is a pivot node between the Fbx camera Node and node attribute
	FbxNode* FbxCameraNode = FbxCamera->GetNode()->GetParent();
	// Import the aspect ratio
	Actor->AspectRatio = FbxCamera->FilmAspectRatio.Get(); // Assumes the FBX comes from Unreal or Maya
	ImportAnimatedProperty(&Actor->AspectRatio, TEXT("AspectRatio"), MatineeGroup, 
				Actor->AspectRatio, FbxCameraNode->FindProperty("UE_AspectRatio") );

	// Import the FOVAngle
	this->FbxImportCamera = FbxCamera;
	Actor->FOVAngle = FbxCamera->ComputeFieldOfView(FbxCamera->FocalLength.Get()); // Assumes the FBX comes from Unreal or Maya
	ImportAnimatedProperty(&Actor->FOVAngle, TEXT("FOVAngle"), MatineeGroup, FbxCamera->FocalLength.Get(),
		FbxCamera->FocalLength, TRUE );
	this->FbxImportCamera = NULL;

	// Look for a depth-of-field or motion blur description in the FBX extra information structure.
	if (FbxCamera->UseDepthOfField.Get())
	{
		ImportAnimatedProperty(&Actor->CamOverridePostProcess.DOF_FocusDistance, TEXT("CamOverridePostProcess.DOF_FocusDistance"), MatineeGroup,
				FbxCamera->FocusDistance.Get(), FbxCameraNode->FindProperty("UE_DOF_FocusDistance") );
	}
	else if (FbxCamera->UseMotionBlur.Get())
	{
		ImportAnimatedProperty(&Actor->CamOverridePostProcess.MotionBlur_Amount, TEXT("CamOverridePostProcess.MotionBlur_Amount"), MatineeGroup,
				FbxCamera->MotionBlurIntensity.Get(), FbxCameraNode->FindProperty("UE_MotionBlur_Amount") );
	}
}

void CFbxImporter::ImportAnimatedProperty(FLOAT* Value, const TCHAR* ValueName, UInterpGroupInst* MatineeGroup, const FLOAT FbxValue, FbxProperty FbxProperty, UBOOL IsCameraFoV/*=FALSE*/)
{
	if (Scene == NULL || FbxProperty == NULL || Value == NULL || MatineeGroup == NULL) return;

	// Retrieve the FBX animated element for this value and verify that it contains an animation curve.
	if (!FbxProperty.IsValid() || !FbxProperty.GetFlag(FbxPropertyAttr::eAnimatable))
	{
		return;
	}
	
	// verify the animation curve and it has valid key
	FbxAnimCurveNode* CurveNode = FbxProperty.GetCurveNode();
	if (!CurveNode)
	{
		return;
	}
	FbxAnimCurve* FbxCurve = CurveNode->GetCurve(0U);
	if (!FbxCurve || FbxCurve->KeyGetCount() <= 1)
	{
		return;
	}
	
	*Value = FbxValue;

	// Look for a track for this property in the Matinee group.
	UInterpTrackFloatProp* PropertyTrack = NULL;
	INT TrackCount = MatineeGroup->Group->InterpTracks.Num();
	for (INT TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
	{
		UInterpTrackFloatProp* Track = Cast<UInterpTrackFloatProp>( MatineeGroup->Group->InterpTracks(TrackIndex) );
		if (Track != NULL && Track->PropertyName == ValueName)
		{
			PropertyTrack = Track;
			PropertyTrack->FloatTrack.Reset(); // Remove all the existing keys from this track.
			break;
		}
	}

	// If a track for this property was not found, create one.
	if (PropertyTrack == NULL)
	{
		PropertyTrack = ConstructObject<UInterpTrackFloatProp>( UInterpTrackFloatProp::StaticClass(), MatineeGroup->Group, NAME_None, RF_Transactional );
		MatineeGroup->Group->InterpTracks.AddItem(PropertyTrack);
		UInterpTrackInstFloatProp* PropertyTrackInst = ConstructObject<UInterpTrackInstFloatProp>( UInterpTrackInstFloatProp::StaticClass(), MatineeGroup, NAME_None, RF_Transactional );
		MatineeGroup->TrackInst.AddItem(PropertyTrackInst);
		PropertyTrack->PropertyName = ValueName;
		PropertyTrack->TrackTitle = ValueName;
		PropertyTrackInst->InitTrackInst(PropertyTrack);
	}
	FInterpCurveFloat& Curve = PropertyTrack->FloatTrack;


	INT KeyCount = FbxCurve->KeyGetCount();
	// create each key in the first path
	// for animation curve for all property in one track, they share time and interpolation mode in animation keys
	for (INT KeyIndex = Curve.Points.Num(); KeyIndex < KeyCount; ++KeyIndex)
	{
		FbxAnimCurveKey CurKey = FbxCurve->KeyGet(KeyIndex);

		// Create the curve keys
		FInterpCurvePoint<FLOAT> Key;
		Key.InVal = CurKey.GetTime().GetSecondDouble();

		Key.InterpMode = GetUnrealInterpMode(CurKey);

		// Add this new key to the curve
		Curve.Points.AddItem(Key);
	}

	// Fill in the curve keys with the correct data for this dimension.
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FbxAnimCurveKey CurKey = FbxCurve->KeyGet(KeyIndex);
		FInterpCurvePoint<FLOAT>& UnrealKey = Curve.Points( KeyIndex );
		
		// Prepare the FBX values to import into the track key.
		FLOAT OutVal = (IsCameraFoV && FbxImportCamera)? FbxImportCamera->ComputeFieldOfView(CurKey.GetValue()): CurKey.GetValue();

		FLOAT ArriveTangent = 0.0f;
		FLOAT LeaveTangent = 0.0f;

		// Convert the Bezier control points, if available, into Hermite tangents
		if( CurKey.GetInterpolation() == FbxAnimCurveDef::eInterpolationCubic )
		{
			FLOAT LeftTangent = FbxCurve->KeyGetLeftDerivative(KeyIndex);
			FLOAT RightTangent = FbxCurve->KeyGetRightDerivative(KeyIndex);

			if (KeyIndex > 0)
			{
				ArriveTangent = LeftTangent * (CurKey.GetTime().GetSecondDouble() - FbxCurve->KeyGetTime(KeyIndex-1).GetSecondDouble());
			}

			if (KeyIndex < KeyCount - 1)
			{
				LeaveTangent = RightTangent * (FbxCurve->KeyGetTime(KeyIndex+1).GetSecondDouble() - CurKey.GetTime().GetSecondDouble());
			}
		}

		UnrealKey.OutVal = OutVal;
		UnrealKey.ArriveTangent = ArriveTangent;
		UnrealKey.LeaveTangent = LeaveTangent;
	}
}

UInterpGroupInst* CFbxImporter::CreateMatineeGroup(USeqAct_Interp* MatineeSequence, AActor* Actor, FString GroupName)
{
	// There are no groups for this actor: create the Matinee group data structure.
	UInterpGroup* MatineeGroupData = ConstructObject<UInterpGroup>( UInterpGroup::StaticClass(), MatineeSequence->InterpData, NAME_None, RF_Transactional );
	MatineeGroupData->GroupName = FName( *GroupName );
	MatineeSequence->InterpData->InterpGroups.AddItem(MatineeGroupData);

	// Instantiate the Matinee group data structure.
	UInterpGroupInst* MatineeGroup = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), MatineeSequence, NAME_None, RF_Transactional );
	MatineeSequence->GroupInst.AddItem(MatineeGroup);
	MatineeGroup->InitGroupInst(MatineeGroupData, Actor);
	MatineeGroup->SaveGroupActorState();
	MatineeSequence->UpdateConnectorsFromData();

	// Retrieve the Kismet connector for the new group.
	INT ConnectorIndex = MatineeSequence->FindConnectorIndex(MatineeGroupData->GroupName.ToString(), LOC_VARIABLE);
	FIntPoint ConnectorPos = MatineeSequence->GetConnectionLocation(LOC_VARIABLE, ConnectorIndex);

	// Look for this actor in the Kismet object variables
	USequence* Sequence = MatineeSequence->ParentSequence;
	INT ObjectCount = Sequence->SequenceObjects.Num();
	USeqVar_Object* KismetVariable = NULL;
	for (INT ObjectIndex = 0; ObjectIndex < ObjectCount; ++ObjectIndex)
	{
		USeqVar_Object* Variable = Cast<USeqVar_Object>(Sequence->SequenceObjects(ObjectIndex));
		if (Variable != NULL && Variable->ObjValue == Actor)
		{
			KismetVariable = Variable;
			break;
		}
	}

	if (KismetVariable == NULL)
	{
		// Create the object variable in Kismet.
		KismetVariable = ConstructObject<USeqVar_Object>( USeqVar_Object::StaticClass(), Sequence, NAME_None, RF_Transactional );
		KismetVariable->ObjValue = Actor;
		KismetVariable->ObjPosX = MatineeSequence->ObjPosX + ConnectorIndex * LO_MIN_SHAPE_SIZE * 3 / 2; // ConnectorPos.X is not yet valid. It becomes valid only at the first render.
		KismetVariable->ObjPosY = ConnectorPos.Y + LO_MIN_SHAPE_SIZE * 2;
		Sequence->AddSequenceObject(KismetVariable);
		KismetVariable->OnCreated();
	}

	// Connect this new object variable with the Matinee group data connector.
	FSeqVarLink& VariableConnector = MatineeSequence->VariableLinks(ConnectorIndex);
	VariableConnector.LinkedVariables.AddItem(KismetVariable);
	KismetVariable->OnConnect(MatineeSequence, ConnectorIndex);

	// Dirty the Kismet sequence
	Sequence->Modify();

	return MatineeGroup;
}


/**
 * Imports a FBX scene node into a Matinee actor group.
 */
FLOAT CFbxImporter::ImportMatineeActor(FbxNode* Node, UInterpGroupInst* MatineeGroup)
{
	FName DefaultName(NAME_None);

	if (Scene == NULL || Node == NULL || MatineeGroup == NULL) return -1.0f;

	// Bake the pivots.
	// Based in sample code in kfbxnode.h, re: Pivot Management
	{
		FbxVector4 ZeroVector(0,0,0);
		Node->SetPivotState(FbxNode::eSOURCE_SET, FbxNode::ePIVOT_STATE_ACTIVE);
		Node->SetPivotState(FbxNode::eDESTINATION_SET, FbxNode::ePIVOT_STATE_ACTIVE);

		FbxRotationOrderEnum RotationOrder;
		Node->GetRotationOrder(FbxNode::eSOURCE_SET , RotationOrder);
		Node->SetRotationOrder(FbxNode::eDESTINATION_SET , RotationOrder);

		// For cameras and lights (without targets) let's compensate the postrotation.
		if (Node->GetCamera() || Node->GetLight())
		{
			if (!Node->GetTarget())
			{
				FbxVector4 RotationVector(90, 0, 0);
				if (Node->GetCamera())    
					RotationVector.Set(0, 90, 0);

				FbxAMatrix RotationMtx;
				RotationMtx.SetR(RotationVector);

				FbxVector4 PostRotationVector = Node->GetPostRotation(FbxNode::eSOURCE_SET);

				// Rotation order don't affect post rotation, so just use the default XYZ order
				FbxAMatrix lSourceR;
				lSourceR.SetR(PostRotationVector);

				RotationMtx = lSourceR * RotationMtx;

				PostRotationVector = RotationMtx.GetR();

				Node->SetPostRotation(FbxNode::eSOURCE_SET, PostRotationVector);
			}

			// Point light do not need to be adjusted (since they radiate in all the directions).
			if (Node->GetLight() && Node->GetLight()->LightType.Get() == FbxLight::ePOINT)
			{
				Node->SetPostRotation(FbxNode::eSOURCE_SET, FbxVector4(0,0,0,0));
			}

			// apply Pre rotations only on bones / end of chains
			if(Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSKELETON
				|| (Node->GetMarker() && Node->GetMarker()->GetType() == FbxMarker::eFK_EFFECTOR)
				|| (Node->GetMarker() && Node->GetMarker()->GetType() == FbxMarker::eIK_EFFECTOR))
			{
				Node->SetPreRotation(FbxNode::eDESTINATION_SET, Node->GetPreRotation(FbxNode::eSOURCE_SET));

				// No pivots on bones
				Node->SetRotationPivot(FbxNode::eDESTINATION_SET, ZeroVector);    
				Node->SetScalingPivot(FbxNode::eDESTINATION_SET,  ZeroVector);    
				Node->SetRotationOffset(FbxNode::eDESTINATION_SET,ZeroVector);    
				Node->SetScalingOffset(FbxNode::eDESTINATION_SET, ZeroVector);
			}
			else
			{
				// any other type: no pre-rotation support but...
				Node->SetPreRotation(FbxNode::eDESTINATION_SET, ZeroVector);

				// support for rotation and scaling pivots.
				Node->SetRotationPivot(FbxNode::eDESTINATION_SET, Node->GetRotationPivot(FbxNode::eSOURCE_SET));    
				Node->SetScalingPivot(FbxNode::eDESTINATION_SET,  Node->GetScalingPivot(FbxNode::eSOURCE_SET));    
				// Rotation and scaling offset are supported
				Node->SetRotationOffset(FbxNode::eDESTINATION_SET, Node->GetRotationOffset(FbxNode::eSOURCE_SET));    
				Node->SetScalingOffset(FbxNode::eDESTINATION_SET,  Node->GetScalingOffset(FbxNode::eSOURCE_SET));
				//
				// If we supported scaling pivots, we could simply do:
				// Node->SetRotationPivot(KFbxNode::eDESTINATION_SET, ZeroVector);
				// Node->SetScalingPivot(KFbxNode::eDESTINATION_SET, ZeroVector);
			}
		}

		// Recursively convert the animation data according to pivot settings.
		Node->ConvertPivotAnimationRecursive(
			NULL,																// Use the first animation stack by default
			FbxNode::eDESTINATION_SET,											// Convert from Source set to Destination set
			FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()),	// Resampling frame rate in frames per second
			FALSE);																// Do not apply key reducing filter
	}


	// Search for a Movement track in the Matinee group.
	UInterpTrackMove* MovementTrack = NULL;
	INT TrackCount = MatineeGroup->Group->InterpTracks.Num();
	for (INT TrackIndex = 0; TrackIndex < TrackCount && MovementTrack == NULL; ++TrackIndex)
	{
		MovementTrack = Cast<UInterpTrackMove>( MatineeGroup->Group->InterpTracks(TrackIndex) );
	}

	// Check whether the actor should be pivoted in the FBX document.

	AActor* Actor = MatineeGroup->GetGroupActor();
	check( Actor != NULL ); // would this ever be triggered?

	// Find out whether the FBX node is animated.
	// Bucket the transforms at the same time.
	// The Matinee Movement track can take in a Translation vector
	// and three animated Euler rotation angles.
	FbxAnimStack* AnimStack = Scene->GetMember(FBX_TYPE(FbxAnimStack), 0);
	if (!AnimStack) return -1.0f;

	MergeAllLayerAnimation(AnimStack, FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()));

	FbxAnimLayer* AnimLayer = AnimStack->GetMember(FBX_TYPE(FbxAnimLayer), 0);
	if (AnimLayer == NULL) return -1.0f;
	
	UBOOL bNodeAnimated = IsNodeAnimated(Node, AnimLayer);
	UBOOL ForceImportSampling = FALSE;

	// Add a Movement track if the node is animated and the group does not already have one.
	if (MovementTrack == NULL && bNodeAnimated)
	{
		MovementTrack = ConstructObject<UInterpTrackMove>( UInterpTrackMove::StaticClass(), MatineeGroup->Group, NAME_None, RF_Transactional );
		MatineeGroup->Group->InterpTracks.AddItem(MovementTrack);
		UInterpTrackInstMove* MovementTrackInst = ConstructObject<UInterpTrackInstMove>( UInterpTrackInstMove::StaticClass(), MatineeGroup, NAME_None, RF_Transactional );
		MatineeGroup->TrackInst.AddItem(MovementTrackInst);
		MovementTrackInst->InitTrackInst(MovementTrack);
	}

	// List of casted subtracks in this movement track.
	TArray< UInterpTrackMoveAxis* > SubTracks;

	// Remove all the keys in the Movement track
	if (MovementTrack != NULL)
	{
		MovementTrack->PosTrack.Reset();
		MovementTrack->EulerTrack.Reset();
		MovementTrack->LookupTrack.Points.Reset();

		if( MovementTrack->SubTracks.Num() > 0 )
		{
			for( INT SubTrackIndex = 0; SubTrackIndex < MovementTrack->SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrackMoveAxis* SubTrack = CastChecked<UInterpTrackMoveAxis>( MovementTrack->SubTracks( SubTrackIndex ) );
				SubTrack->FloatTrack.Reset();
				SubTrack->LookupTrack.Points.Reset();
				SubTracks.AddItem( SubTrack );
			}
		}
	}

	FLOAT TimeLength = -1.0f;

	// Fill in the Movement track with the FBX keys
	if (bNodeAnimated)
	{
		// Check: The position and rotation tracks must have the same number of keys, the same key timings and
		// the same segment interpolation types.
		FbxAnimCurve *TransCurves[6], *RealCurves[6];
		
		TransCurves[0] = Node->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		TransCurves[1] = Node->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		TransCurves[2] = Node->LclTranslation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

		TransCurves[3] = Node->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
		TransCurves[4] = Node->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
		TransCurves[5] = Node->LclRotation.GetCurve<FbxAnimCurve>(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
		// remove empty curves
		INT CurveIndex;
		INT RealCurveNum = 0;
		for (CurveIndex = 0; CurveIndex < 6; CurveIndex++)
		{
			if (TransCurves[CurveIndex] && TransCurves[CurveIndex]->KeyGetCount() > 1)
			{
				RealCurves[RealCurveNum++] = TransCurves[CurveIndex];
			}
		}
		
		UBOOL bResample = FALSE;
		if (RealCurveNum > 1)
		{
			INT KeyCount = RealCurves[0]->KeyGetCount();
			// check key count of all curves
			for (CurveIndex = 1; CurveIndex < RealCurveNum; CurveIndex++)
			{
				if (KeyCount != RealCurves[CurveIndex]->KeyGetCount())
				{
					bResample = TRUE;
					break;
				}
			}
			// check key time for each key
			for (INT KeyIndex = 0; !bResample && KeyIndex < KeyCount; KeyIndex++)
			{
				FbxTime KeyTime = RealCurves[0]->KeyGetTime(KeyIndex);
				FbxAnimCurveDef::InterpolationType Interpolation = RealCurves[0]->KeyGetInterpolation(KeyIndex);
				//KFbxAnimCurveDef::ETangentMode Tangent = RealCurves[0]->KeyGetTangentMode(KeyIndex);
				
				for (CurveIndex = 1; CurveIndex < RealCurveNum; CurveIndex++)
				{
					if (KeyTime != RealCurves[CurveIndex]->KeyGetTime(KeyIndex) ||
						Interpolation != RealCurves[CurveIndex]->KeyGetInterpolation(KeyIndex) ) // ||
						//Tangent != RealCurves[CurveIndex]->KeyGetTangentMode(KeyIndex))
					{
						bResample = TRUE;
						break;
					}
				}
			}
			
			if (bResample)
			{
				// Get the re-sample time span
				FbxTime Start, Stop;
				Start = RealCurves[0]->KeyGetTime(0);
				Stop = RealCurves[0]->KeyGetTime(RealCurves[0]->KeyGetCount() - 1);
				for (CurveIndex = 1; CurveIndex < RealCurveNum; CurveIndex++)
				{
					if (Start > RealCurves[CurveIndex]->KeyGetTime(0))
					{
						Start = RealCurves[CurveIndex]->KeyGetTime(0);
					}
					
					if (Stop < RealCurves[CurveIndex]->KeyGetTime(RealCurves[CurveIndex]->KeyGetCount() - 1))
					{
						Stop = RealCurves[CurveIndex]->KeyGetTime(RealCurves[CurveIndex]->KeyGetCount() - 1);
					}
				}
				
				DOUBLE ResampleRate;
				ResampleRate = FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode());
				FbxTime FramePeriod;
				FramePeriod.SetSecondDouble(1.0 / ResampleRate);
				
				for (CurveIndex = 0; CurveIndex < 6; CurveIndex++)
				{
					UBOOL bRemoveConstantKey = FALSE;
					// for the constant animation curve, the key may be not in the resample time range,
					// so we need to remove the constant key after resample,
					// otherwise there must be one more key
					if (TransCurves[CurveIndex]->KeyGetCount() == 1 && TransCurves[CurveIndex]->KeyGetTime(0) < Start)
					{
						bRemoveConstantKey = TRUE;
					}

					// only re-sample from Start to Stop
					FbxAnimCurveFilterResample CurveResampler;
					CurveResampler.SetPeriodTime(FramePeriod);
					CurveResampler.SetStartTime(Start);
					CurveResampler.SetStopTime(Stop);
					CurveResampler.SetKeysOnFrame(true);
					CurveResampler.Apply(*TransCurves[CurveIndex]);

					// remove the key that is not in the resample time range
					// the constant key always at the time 0, so it is OK to remove the first key
					if (bRemoveConstantKey)
					{
						TransCurves[CurveIndex]->KeyRemove(0);
					}
				}

			}
			
		}


		FbxAMatrix Matrix		= ComputeTotalMatrix(Node);
		FbxVector4 DefaultPos	= Node->LclTranslation.Get();
		FbxVector4 DefaultRot	= Node->LclRotation.Get();

		FbxAMatrix FbxCamToUnrealRHMtx, InvFbxCamToUnrealRHMtx;
		FbxAMatrix UnrealRHToUnrealLH, InUnrealRHToUnrealLH;

		Actor->SetLocation( FVector( -DefaultPos[1], -DefaultPos[0], DefaultPos[2] ) );
	
		UBOOL bIsCamera = FALSE;
		if (!Node->GetCamera())
		{
			Actor->SetRotation( FRotator::MakeFromEuler( FVector( DefaultRot[0], -DefaultRot[1], -DefaultRot[2] ) ) );
		}
		else
		{
			// Note: the camera rotations contain rotations from the Fbx Camera to the converted
			// right-hand Unreal coordinates.  So we must negate the Fbx Camera -> Unreal WorldRH, then convert
			// the remaining rotation to left-handed coordinates

			// Describing coordinate systems as <Up, Forward, Left>:
			// Fbx Camera:					< Y, -Z, -X>
			// Unreal Right-handed World:	< Z, -Y,  X>
			// Unreal Left-handed World:	< Z,  X, -Y>

			FbxAMatrix DefaultRotMtx;
			DefaultRotMtx.SetR(DefaultRot);

			FbxCamToUnrealRHMtx[0] = FbxVector4(-1.f,  0.f,  0.f, 0.f );
			FbxCamToUnrealRHMtx[1] = FbxVector4( 0.f,  0.f,  1.f, 0.f );
			FbxCamToUnrealRHMtx[2] = FbxVector4( 0.f,  1.f,  0.f, 0.f );
			InvFbxCamToUnrealRHMtx = FbxCamToUnrealRHMtx.Inverse();

			UnrealRHToUnrealLH[0] = FbxVector4( 0.f,  1.f,  0.f, 0.f );
			UnrealRHToUnrealLH[1] = FbxVector4( 1.f,  0.f,  0.f, 0.f );
			UnrealRHToUnrealLH[2] = FbxVector4( 0.f,  0.f,  1.f, 0.f );
			InUnrealRHToUnrealLH = UnrealRHToUnrealLH.Inverse();

			// Remove the FbxCamera's local to world rotation
			FbxAMatrix UnrealCameraRotationMtx		= DefaultRotMtx * InvFbxCamToUnrealRHMtx;

			// Convert the remaining rotation into world space
			UnrealCameraRotationMtx					= UnrealRHToUnrealLH * UnrealCameraRotationMtx * InUnrealRHToUnrealLH;

			FbxVector4 UnrealCameraRotationEuler	= UnrealCameraRotationMtx.GetR();

			Actor->SetRotation( FRotator::MakeFromEuler( FVector( UnrealCameraRotationEuler[0], UnrealCameraRotationEuler[1], UnrealCameraRotationEuler[2] ) ) );
			bIsCamera = TRUE;
		}

		if( MovementTrack->SubTracks.Num() > 0 )
		{
			check (bIsCamera == FALSE);
			ImportMoveSubTrack(TransCurves[0], 0, SubTracks(0), 0, FALSE, RealCurves[0], DefaultPos[0]);
			ImportMoveSubTrack(TransCurves[1], 1, SubTracks(1), 1, TRUE, RealCurves[0], DefaultPos[1]);
			ImportMoveSubTrack(TransCurves[2], 2, SubTracks(2), 2, FALSE, RealCurves[0], DefaultPos[2]);
			ImportMoveSubTrack(TransCurves[3], 3, SubTracks(3), 0, FALSE, RealCurves[0], DefaultRot[0]);
			ImportMoveSubTrack(TransCurves[4], 4, SubTracks(4), 1, TRUE, RealCurves[0], DefaultRot[1]);
			ImportMoveSubTrack(TransCurves[5], 5, SubTracks(5), 2, TRUE, RealCurves[0], DefaultRot[2]);

			for( INT SubTrackIndex = 0; SubTrackIndex < SubTracks.Num(); ++SubTrackIndex )
			{
				UInterpTrackMoveAxis* SubTrack = SubTracks( SubTrackIndex );
				// Generate empty look-up keys.
				INT KeyIndex;
				for ( KeyIndex = 0; KeyIndex < SubTrack->FloatTrack.Points.Num(); ++KeyIndex )
				{
					SubTrack->LookupTrack.AddPoint( SubTrack->FloatTrack.Points( KeyIndex).InVal, DefaultName );
				}
			}

			FLOAT StartTime;
			// Scale the track timing to ensure that it is large enough
			MovementTrack->GetTimeRange( StartTime, TimeLength );
		}
		else
		{
			ImportMatineeAnimated(TransCurves[0], MovementTrack->PosTrack, 1, TRUE, RealCurves[0], DefaultPos[0]);
			ImportMatineeAnimated(TransCurves[1], MovementTrack->PosTrack, 0, TRUE, RealCurves[0], DefaultPos[1]);
			ImportMatineeAnimated(TransCurves[2], MovementTrack->PosTrack, 2, FALSE, RealCurves[0], DefaultPos[2]);

			if (bIsCamera)
			{
				// Import the rotation data unmodified
				ImportMatineeAnimated(TransCurves[3], MovementTrack->EulerTrack, 0, FALSE, RealCurves[0], DefaultRot[0]);
				ImportMatineeAnimated(TransCurves[4], MovementTrack->EulerTrack, 1, FALSE, RealCurves[0], DefaultRot[1]);
				ImportMatineeAnimated(TransCurves[5], MovementTrack->EulerTrack, 2, FALSE, RealCurves[0], DefaultRot[2]);

				// Once the individual Euler channels are imported, then convert the rotation into Unreal coords
				for(INT PointIndex = 0; PointIndex < MovementTrack->EulerTrack.Points.Num(); ++PointIndex)
				{
					FInterpCurvePoint<FVector>& CurveKey = MovementTrack->EulerTrack.Points( PointIndex );

					FbxAMatrix CurveMatrix;
					CurveMatrix.SetR( FbxVector4(CurveKey.OutVal.X, CurveKey.OutVal.Y, CurveKey.OutVal.Z) );

					// Remove the FbxCamera's local to world rotation
					FbxAMatrix UnrealCameraRotationMtx		= CurveMatrix * InvFbxCamToUnrealRHMtx;

					// Convert the remaining rotation into world space
					UnrealCameraRotationMtx					= UnrealRHToUnrealLH * UnrealCameraRotationMtx * InUnrealRHToUnrealLH;

					FbxVector4 UnrealCameraRotationEuler	= UnrealCameraRotationMtx.GetR();
					CurveKey.OutVal.X = UnrealCameraRotationEuler[0];
					CurveKey.OutVal.Y = UnrealCameraRotationEuler[1];
					CurveKey.OutVal.Z = UnrealCameraRotationEuler[2];
				}

				
				// The FInterpCurve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
				// will cause the camera to rotate all the way around through 0 degrees.  So here we make a second pass over the 
				// Euler track to convert the angles into a more interpolation-friendly format.  
				FLOAT CurrentAngleOffset[3] = { 0.f, 0.f, 0.f };
				for(INT PointIndex = 1; PointIndex < MovementTrack->EulerTrack.Points.Num(); ++PointIndex)
				{
					const FInterpCurvePoint<FVector>& CurveKeyPrev	= MovementTrack->EulerTrack.Points( PointIndex-1 );
					FInterpCurvePoint<FVector>& CurveKey			= MovementTrack->EulerTrack.Points( PointIndex );
					
					FVector PreviousOutVal	= CurveKeyPrev.OutVal;
					FVector CurrentOutVal	= CurveKey.OutVal;

					for(INT AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
					{
						FLOAT DeltaAngle = (CurrentOutVal[AxisIndex] + CurrentAngleOffset[AxisIndex]) - PreviousOutVal[AxisIndex];

						if(DeltaAngle >= 180)
						{
							CurrentAngleOffset[AxisIndex] -= 360;
						}
						else if(DeltaAngle <= -180)
						{
							CurrentAngleOffset[AxisIndex] += 360;
						}

						CurrentOutVal[AxisIndex] += CurrentAngleOffset[AxisIndex];
					}

					CurveKey.OutVal = CurrentOutVal;
				}

				// Recalculate the tangents using the new data
				MovementTrack->EulerTrack.AutoSetTangents();
			}
			else
			{
				ImportMatineeAnimated(TransCurves[3], MovementTrack->EulerTrack, 0, FALSE, RealCurves[0], DefaultRot[0]);
				ImportMatineeAnimated(TransCurves[4], MovementTrack->EulerTrack, 1, TRUE, RealCurves[0], DefaultRot[1]);
				ImportMatineeAnimated(TransCurves[5], MovementTrack->EulerTrack, 2, TRUE, RealCurves[0], DefaultRot[2]);
			}

			// Generate empty look-up keys.
			INT KeyIndex;
			for ( KeyIndex = 0; KeyIndex < RealCurves[0]->KeyGetCount(); ++KeyIndex )
			{
				MovementTrack->LookupTrack.AddPoint( (FLOAT)RealCurves[0]->KeyGet(KeyIndex).GetTime().GetSecondDouble(), DefaultName );
			}

			// Scale the track timing to ensure that it is large enough
			TimeLength = (FLOAT)RealCurves[0]->KeyGet(KeyIndex - 1).GetTime().GetSecondDouble();
		}

	}

	// Inform the engine and UI that the tracks have been modified.
	if (MovementTrack != NULL)
	{
		MovementTrack->Modify();
	}
	MatineeGroup->Modify();
	
	return TimeLength;
}


BYTE CFbxImporter::GetUnrealInterpMode(FbxAnimCurveKey FbxKey)
{
	BYTE Mode = CIM_CurveUser;
	// Convert the interpolation type from FBX to Unreal.
	switch( FbxKey.GetInterpolation() )
	{
		case FbxAnimCurveDef::eInterpolationCubic:
		{
			switch (FbxKey.GetTangentMode())
			{
				// Auto tangents will now be imported as user tangents to allow the
				// user to modify them without inadvertently resetting other tangents
// 				case KFbxAnimCurveDef::eTANGENT_AUTO:
// 					if ((KFbxAnimCurveDef::eTANGENT_GENERIC_CLAMP & FbxKey.GetTangentMode(true)))
// 					{
// 						Mode = CIM_CurveAutoClamped;
// 					}
// 					else
// 					{
// 						Mode = CIM_CurveAuto;
// 					}
// 					break;
				case FbxAnimCurveDef::eTangentBreak:
					Mode = CIM_CurveBreak;
					break;
				case FbxAnimCurveDef::eTangentAuto:
				case FbxAnimCurveDef::eTangentUser:
				case FbxAnimCurveDef::eTangentTCB:
					Mode = CIM_CurveUser;
					break;
				default:
					break;
			}
			break;
		}

		case FbxAnimCurveDef::eInterpolationConstant:
			if (FbxKey.GetTangentMode() != (FbxAnimCurveDef::TangentMode)FbxAnimCurveDef::eConstantStandard)
			{
				// warning not support
				;
			}
			Mode = CIM_Constant;
			break;

		case FbxAnimCurveDef::eInterpolationLinear:
			Mode = CIM_Linear;
			break;
	}
	return Mode;
}

void CFbxImporter::ImportMoveSubTrack( FbxAnimCurve* FbxCurve, INT FbxDimension, UInterpTrackMoveAxis* SubTrack, INT CurveIndex, UBOOL bNegative, FbxAnimCurve* RealCurve, FLOAT DefaultVal )
{
	if (CurveIndex >= 3) return;

	FInterpCurveFloat& Curve = SubTrack->FloatTrack;
	// the FBX curve has no valid keys, so fake the Unreal Matinee curve
	if (FbxCurve == NULL || FbxCurve->KeyGetCount() < 2)
	{
		INT KeyIndex;
		for ( KeyIndex = Curve.Points.Num(); KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			FLOAT Time = (FLOAT)RealCurve->KeyGet(KeyIndex).GetTime().GetSecondDouble();
			// Create the curve keys
			FInterpCurvePoint<FLOAT> Key;
			Key.InVal = Time;
			Key.InterpMode = GetUnrealInterpMode(RealCurve->KeyGet(KeyIndex));

			Curve.Points.AddItem(Key);
		}

		for ( KeyIndex = 0; KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			FInterpCurvePoint<FLOAT>& Key = Curve.Points( KeyIndex );
			Key.OutVal = DefaultVal;
			Key.ArriveTangent = 0;
			Key.LeaveTangent = 0;
		}
	}
	else
	{
		INT KeyCount = (INT) FbxCurve->KeyGetCount();

		for (INT KeyIndex = Curve.Points.Num(); KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );

			// Create the curve keys
			FInterpCurvePoint<FLOAT> Key;
			Key.InVal = CurKey.GetTime().GetSecondDouble();

			Key.InterpMode = GetUnrealInterpMode(CurKey);

			// Add this new key to the curve
			Curve.Points.AddItem(Key);
		}

		// Fill in the curve keys with the correct data for this dimension.
		for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );
			FInterpCurvePoint<FLOAT>& UnrealKey = Curve.Points( KeyIndex );

			// Prepare the FBX values to import into the track key.
			// Convert the Bezier control points, if available, into Hermite tangents
			FLOAT OutVal = bNegative ? -CurKey.GetValue() : CurKey.GetValue();

			FLOAT ArriveTangent = 0.0f;
			FLOAT LeaveTangent = 0.0f;

			if( CurKey.GetInterpolation() == FbxAnimCurveDef::eInterpolationCubic )
			{
				ArriveTangent = bNegative? -FbxCurve->KeyGetLeftDerivative(KeyIndex): FbxCurve->KeyGetLeftDerivative(KeyIndex);
				LeaveTangent = bNegative? -FbxCurve->KeyGetRightDerivative(KeyIndex): FbxCurve->KeyGetRightDerivative(KeyIndex);
			}

			// Fill in the track key with the prepared values
			UnrealKey.OutVal = OutVal;
			UnrealKey.ArriveTangent = ArriveTangent;
			UnrealKey.LeaveTangent = LeaveTangent;
		}
	}
}

void CFbxImporter::ImportMatineeAnimated(FbxAnimCurve* FbxCurve, FInterpCurveVector& Curve, INT CurveIndex, UBOOL bNegative, FbxAnimCurve* RealCurve, FLOAT DefaultVal)
{
	if (CurveIndex >= 3) return;
	
	// the FBX curve has no valid keys, so fake the Unreal Matinee curve
	if (FbxCurve == NULL || FbxCurve->KeyGetCount() < 2)
	{
		INT KeyIndex;
		for ( KeyIndex = Curve.Points.Num(); KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			FLOAT Time = (FLOAT)RealCurve->KeyGet(KeyIndex).GetTime().GetSecondDouble();
			// Create the curve keys
			FInterpCurvePoint<FVector> Key;
			Key.InVal = Time;
			Key.InterpMode = GetUnrealInterpMode(RealCurve->KeyGet(KeyIndex));
			
			Curve.Points.AddItem(Key);
		}
		
		for ( KeyIndex = 0; KeyIndex < RealCurve->KeyGetCount(); ++KeyIndex )
		{
			FInterpCurvePoint<FVector>& Key = Curve.Points( KeyIndex );
			switch (CurveIndex)
			{
			case 0:
				Key.OutVal.X = DefaultVal;
				Key.ArriveTangent.X = 0;
				Key.LeaveTangent.X = 0;
				break;
			case 1:
				Key.OutVal.Y = DefaultVal;
				Key.ArriveTangent.Y = 0;
				Key.LeaveTangent.Y = 0;
				break;
			case 2:
			default:
				Key.OutVal.Z = DefaultVal;
				Key.ArriveTangent.Z = 0;
				Key.LeaveTangent.Z = 0;
				break;
			}
		}
	}
	else
	{
		INT KeyCount = (INT) FbxCurve->KeyGetCount();
		
		for (INT KeyIndex = Curve.Points.Num(); KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );

			// Create the curve keys
			FInterpCurvePoint<FVector> Key;
			Key.InVal = CurKey.GetTime().GetSecondDouble();

			Key.InterpMode = GetUnrealInterpMode(CurKey);

			// Add this new key to the curve
			Curve.Points.AddItem(Key);
		}

		// Fill in the curve keys with the correct data for this dimension.
		for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey CurKey = FbxCurve->KeyGet( KeyIndex );
			FInterpCurvePoint<FVector>& UnrealKey = Curve.Points( KeyIndex );
			
			// Prepare the FBX values to import into the track key.
			// Convert the Bezier control points, if available, into Hermite tangents
			FLOAT OutVal = ( bNegative ? -CurKey.GetValue() : CurKey.GetValue() );

			FLOAT ArriveTangent = 0.0f;
			FLOAT LeaveTangent = 0.0f;
			
			if( CurKey.GetInterpolation() == FbxAnimCurveDef::eInterpolationCubic )
			{
				ArriveTangent = bNegative? -FbxCurve->KeyGetLeftDerivative(KeyIndex): FbxCurve->KeyGetLeftDerivative(KeyIndex);
				LeaveTangent = bNegative? -FbxCurve->KeyGetRightDerivative(KeyIndex): FbxCurve->KeyGetRightDerivative(KeyIndex);
			}

			// Fill in the track key with the prepared values
			switch (CurveIndex)
			{
			case 0:
				UnrealKey.OutVal.X = OutVal;
				UnrealKey.ArriveTangent.X = ArriveTangent;
				UnrealKey.LeaveTangent.X = LeaveTangent;
				break;
			case 1:
				UnrealKey.OutVal.Y = OutVal;
				UnrealKey.ArriveTangent.Y = ArriveTangent;
				UnrealKey.LeaveTangent.Y = LeaveTangent;
				break;
			case 2:
			default:
				UnrealKey.OutVal.Z = OutVal;
				UnrealKey.ArriveTangent.Z = ArriveTangent;
				UnrealKey.LeaveTangent.Z = LeaveTangent;
				break;
			}
		}
	}
}

} // namespace UnFBX


#endif	// WITH_FBX


