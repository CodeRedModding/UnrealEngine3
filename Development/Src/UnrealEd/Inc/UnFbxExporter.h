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

/*============================================================================
					FBX exporter for Unreal Engine 3
=============================================================================*/
#if WITH_FBX

#ifndef __UNFBXEXPORTER_H__
#define __UNFBXEXPORTER_H__

#include "Factories.h"
#include "MatineeExporter.h"
#include "UnFbxImporter.h"

namespace UnFbx
{

/**
 * Main FBX Exporter class.
 */
class CFbxExporter  : public MatineeExporter
{
public:
	/**
	 * Returns the exporter singleton. It will be created on the first request.
	 */
	static CFbxExporter* GetInstance();
	
	/**
	 * Creates and readies an empty document for export.
	 */
	virtual void CreateDocument();
	
	/**
	 * Closes the FBX document, releasing its memory.
	 */
	virtual void CloseDocument();
	
	/**
	 * Writes the FBX document to disk and releases it by calling the CloseDocument() function.
	 */
	virtual void WriteToFile(const TCHAR* Filename);
	
	/**
	 * Exports the light-specific information for a UE3 light actor.
	 */
	virtual void ExportLight( ALight* Actor, USeqAct_Interp* MatineeSequence );

	/**
	 * Exports the camera-specific information for a UE3 camera actor.
	 */
	virtual void ExportCamera( ACameraActor* Actor, USeqAct_Interp* MatineeSequence );

	/**
	 * Exports the mesh and the actor information for a UE3 brush actor.
	 */
	virtual void ExportBrush(ABrush* Actor, UModel* InModel, UBOOL bConvertToStaticMesh );

	/**
	 * Exports the basic scene information to the FBX document.
	 */
	virtual void ExportLevelMesh( ULevel* Level, USeqAct_Interp* MatineeSequence );

	/**
	 * Exports the given Matinee sequence information into a FBX document.
	 */
	virtual void ExportMatinee(class USeqAct_Interp* MatineeSequence);

	/**
	 * Exports all the animation sequences part of a single Group in a Matinee sequence
	 * as a single animation in the FBX document.  The animation is created by sampling the
	 * sequence at 30 updates/second and extracting the resulting bone transforms from the given
	 * skeletal mesh
	 * @param MatineeSequence The Matinee Sequence containing the group to export
	 * @param SkeletalMeshComponent The Skeletal mesh that the animations from the Matinee group are applied to
	 */
	virtual void ExportMatineeGroup(class USeqAct_Interp* MatineeSequence, USkeletalMeshComponent* SkeletalMeshComponent);

	/**
	 * Exports the mesh and the actor information for a UE3 static mesh actor.
	 */
	virtual void ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, USeqAct_Interp* MatineeSequence );

	/**
	 * Exports a UE3 static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MaterialOrder	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	virtual void ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>* MaterialOrder = NULL );

	/**
	 * Exports BSP
	 * @param Model			 The model with BSP to export
	 * @param bSelectedOnly  TRUE to export only selected surfaces (or brushes)
	 */
	virtual void ExportBSP( UModel* Model, UBOOL bSelectedOnly );

	/**
	 * Exports a UE3 static mesh light map
	 */
	virtual void ExportStaticMeshLightMap( UStaticMesh* StaticMesh, INT LODIndex, INT UVChannel );

	/**
	 * Exports a UE3 skeletal mesh
	 */
	virtual void ExportSkeletalMesh( USkeletalMesh* SkeletalMesh );

	/**
	 * Exports the mesh and the actor information for a UE3 skeletal mesh actor.
	 */
	virtual void ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent );

	/**
	 * Exports a single UAnimSequence, and optionally a skeletal mesh
	 */
	void ExportAnimSequence( const UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, UBOOL bExportSkelMesh );

	/**
	 * Exports the list of UAnimSequences as a single animation based on the settings in the TrackKeys
	 */
	void ExportAnimSequencesAsSingle( USkeletalMesh* SkelMesh, const ASkeletalMeshActor* SkelMeshActor, const FString& ExportName, const TArray<UAnimSequence*>& AnimSeqList, const TArray<struct FAnimControlTrackKey>& TrackKeys );

private:
	CFbxExporter();
	~CFbxExporter();
	
	FbxManager* SdkManager;
	FbxScene* Scene;
	FbxAnimStack* AnimStack;
	FbxAnimLayer* AnimLayer;
	FbxCamera* DefaultCamera;
	
	CBasicDataConverter Converter;
	
	TMap<FString,INT> FbxNodeNameToIndexMap;
	TMap<AActor*, FbxNode*> FbxActors;
	TMap<UMaterial*, FbxSurfaceMaterial*> FbxMaterials;
	
	/** The frames-per-second (FPS) used when baking transforms */
	static const FLOAT BakeTransformsFPS;
	
	
	void ExportModel(UModel* Model, FbxNode* Node, const char* Name);
	
	/**
	 * Exports the basic information about a UE3 actor and buffers it.
	 * This function creates one FBX node for the actor with its placement.
	 */
	FbxNode* ExportActor(AActor* Actor, USeqAct_Interp* MatineeSequence );
	
	/**
	 * Exports a static mesh
	 * @param RenderMesh	The static mesh render data to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 * @param LightmapUVChannel Optional UV channel to export
	 * @param ColorBuffer	Vertex color overrides to export
	 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	FbxNode* ExportStaticMeshToFbx(FStaticMeshRenderData& RenderMesh, const TCHAR* MeshName, FbxNode* FbxActor, INT LightmapUVChannel = -1, FColorVertexBuffer* ColorBuffer = NULL, const TArray<UMaterialInterface*>* MaterialOrderOverride = NULL );

	/**
	 * Adds FBX skeleton nodes to the FbxScene based on the skeleton in the given USkeletalMesh, and fills
	 * the given array with the nodes created
	 */
	FbxNode* CreateSkeleton(const USkeletalMesh& SkelMesh, TArray<FbxNode*>& BoneNodes);

	/**
	 * Adds an Fbx Mesh to the FBX scene based on the data in the given FStaticLODModel
	 */
	FbxNode* CreateMesh(const USkeletalMesh& SkelMesh, const TCHAR* MeshName);

	/**
	 * Adds Fbx Clusters necessary to skin a skeletal mesh to the bones in the BoneNodes list
	 */
	void BindMeshToSkeleton(const USkeletalMesh& SkelMesh, FbxNode* MeshRootNode, TArray<FbxNode*>& BoneNodes);

	/**
	 * Add a bind pose to the scene based on the FbxMesh and skinning settings of the given node
	 */
	void CreateBindPose(FbxNode* MeshRootNode);

	/**
	 * Add the given skeletal mesh to the Fbx scene in preparation for exporting.  Makes all new nodes a child of the given node
	 */
	void ExportSkeletalMeshToFbx(const USkeletalMesh& SkelMesh, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	 * Add the given animation sequence as rotation and translation tracks to the given list of bone nodes
	 */
	void ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer,
		FLOAT AnimStartOffset, FLOAT AnimEndOffset, FLOAT AnimPlayRate, FLOAT StartTime, UBOOL bLooping);

	/** 
	 * The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
	 * will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
	 * rotation tracks to convert the angles into a more interpolation-friendly format.  
	 */
	void CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer );

	/**
	 * Exports the Matinee movement track into the FBX animation stack.
	 */
	void ExportMatineeTrackMove(FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, FLOAT InterpLength);

	/**
	 * Exports the Matinee float property track into the FBX animation stack.
	 */
	void ExportMatineeTrackFloatProp(FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack);

	/**
	 * Exports a given interpolation curve into the FBX animation curve.
	 */
	void ExportAnimatedVector(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, UBOOL bPosCurve, INT CurveIndex, UBOOL bNegative, FLOAT InterpLength);
	
	/**
	 * Exports a movement subtrack to an FBX curve
	 */
	void ExportMoveSubTrack(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, UBOOL bPosCurve, INT CurveIndex, UBOOL bNegative, FLOAT InterpLength);
	
	void ExportAnimatedFloat(FbxProperty* FbxProperty, FInterpCurveFloat* Curve, UBOOL IsCameraFoV);

	/**
	 * Finds the given UE3 actor in the already-exported list of structures
	 * @return KFbxNode* the FBX node created from the UE3 actor
	 */
	FbxNode* FindActor(AActor* Actor);
	
	/**
	 * Exports the profile_COMMON information for a UE3 material.
	 */
	FbxSurfaceMaterial* ExportMaterial(UMaterial* Material);
	
	FbxSurfaceMaterial* CreateDefaultMaterial();
	
	/**
	 * Create user property in Fbx Node.
	 * Some Unreal animatable property can't be animated in FBX property. So create user property to record the animation of property.
	 *
	 * @param Node  FBX Node the property append to.
	 * @param Value Property value.
	 * @param Name  Property name.
	 * @param Label Property label.
	 */
	void CreateAnimatableUserProperty(FbxNode* Node, FLOAT Value, const char* Name, const char* Label);
};



} // namespace UnFbx



#endif // __UNFBXEXPORTER_H__

#endif // WITH_FBX
