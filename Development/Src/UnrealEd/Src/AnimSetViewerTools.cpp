/*=============================================================================
	AnimSetViewerMain.cpp: AnimSet viewer main
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineAnimClasses.h"
#include "AnimSetViewer.h"
#include "PropertyWindow.h"
#include "DlgGenericComboEntry.h"
#include "DlgRename.h"
#include "DlgRotateAnimSequence.h"
#include "BusyCursor.h"
#include "AnimationUtils.h"
#include "SkelImport.h"
#include "Factories.h"
#include "UnSkelRenderPublic.h"
#include "UnMeshBuild.h"
#include "DlgCheckBoxList.h"

#if WITH_FBX
#include "UnFbxImporter.h"
#include "UnFbxExporter.h"
#endif // WITH_FBX

#if WITH_SIMPLYGON
#include "SkeletalMeshSimplificationWindow.h"
#endif // #if WITH_SIMPLYGON

/** Comparison function for sorting INTs smallest to largest */
IMPLEMENT_COMPARE_CONSTREF( BYTE, AnimSetViewerTools, { return (A - B); } )
IMPLEMENT_COMPARE_CONSTREF( INT, SortINTSAscending, { return (A - B); } )

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Static helpers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	/**
	 * Recompresses the sequences in specified anim set by applying each sequence's compression scheme.
	 * 
	 */
	static void RecompressSet(UAnimSet* AnimSet, USkeletalMesh* SkeletalMesh)
	{
		for( INT SequenceIndex = 0 ; SequenceIndex < AnimSet->Sequences.Num() ; ++SequenceIndex )
		{
			UAnimSequence* Seq = AnimSet->Sequences( SequenceIndex );
			FAnimationUtils::CompressAnimSequence(Seq, SkeletalMesh, FALSE, FALSE);
		}
	}

	/**
	 * Recompresses the specified sequence in anim set by applying the sequence's compression scheme.
	 * 
	 */
	static void RecompressSequence(UAnimSequence* Seq, USkeletalMesh* SkeletalMesh)
	{
		FAnimationUtils::CompressAnimSequence(Seq, SkeletalMesh, FALSE, FALSE);
	}
}

/*
*   Given a skeletal mesh and an array of bone pair "breaks", calculate the vertices affected by such a break
*   @param SkeletalMesh - Mesh of interest
*   @param InfluenceIdx - Vertex influence array index
*   @param BonePairs - array of bone pairs to break
*   @param OutMapping - output of vertex indices affected by the break
*/
void FindBoneWeightsPerVertex(const USkeletalMeshComponent* SkelComp, TArray< TArray<INT> >& BoneVertices)
{
	USkeletalMesh * SkeletalMesh = SkelComp->SkeletalMesh;

	BoneVertices.Empty();

	if( SkeletalMesh->LODModels.IsValidIndex(SkelComp->PredictedLODLevel) == FALSE )
	{
		return;
	}

	const FStaticLODModel* LODModel = &(SkeletalMesh->LODModels(SkelComp->PredictedLODLevel));
	if (LODModel->VertexInfluences.Num() > 0)
	{
		const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel->VertexInfluences(0);
		if (VertexInfluences.Usage == IWU_PartialSwap)
		{
			BoneVertices.AddZeroed( SkeletalMesh->RefSkeleton.Num());
			for(INT ChunkIndex = 0;ChunkIndex < LODModel->Chunks.Num();ChunkIndex++)
			{
				const FSkelMeshChunk& Chunk = LODModel->Chunks(ChunkIndex);

				const INT RigidIndexStart = Chunk.GetRigidVertexBufferIndex();
				for(INT VertIndex=0; VertIndex<Chunk.RigidVertices.Num();VertIndex++)
				{
					const FRigidSkinVertex* RigidVert = &Chunk.RigidVertices(VertIndex);
					INT BoneIndex = Chunk.BoneMap(RigidVert->Bone);
					BoneVertices(BoneIndex).AddItem(RigidIndexStart+VertIndex);
				}

				const INT SoftIndexStart = Chunk.GetSoftVertexBufferIndex();
				for(INT VertIndex=0; VertIndex<Chunk.SoftVertices.Num(); VertIndex++)
				{
					const FSoftSkinVertex* SoftVert = &Chunk.SoftVertices(VertIndex);
					for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
					{
						if( SoftVert->InfluenceWeights[Idx] > 0 )
						{
							INT BoneIndex = Chunk.BoneMap(SoftVert->InfluenceBones[Idx]);
							BoneVertices(BoneIndex).AddItem(SoftIndexStart+VertIndex);
						}
					}
				}
			}
		}
	}
}

/*
 *   Given a skeletal mesh and an array of bone pair "breaks", calculate the vertices affected by such a break
 *   @param SkeletalMesh - Mesh of interest
 *   @param InfluenceIdx - Vertex influence array index
 *   @param BonePairs - array of bone pairs to break
 *   @param OutMapping - output of vertex indices affected by the break
 */
void CalculateBoneWeightInfluences(USkeletalMesh* SkeletalMesh, TArray<FBoneIndexPair>& BonePairs, EBoneBreakOption Option)
{
	// update instance weights for all LODs
	INT NumLODs = SkeletalMesh->LODModels.Num();
	for( INT CurLODIdx=0; CurLODIdx < NumLODs; CurLODIdx++ )
	{
		FStaticLODModel& LODModel = SkeletalMesh->LODModels(CurLODIdx);
		for (INT InfluenceIdx=0; InfluenceIdx < LODModel.VertexInfluences.Num(); InfluenceIdx++)
		{	
			FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(InfluenceIdx);
			if (VertexInfluences.Usage != IWU_PartialSwap)
			{
				continue;
			}

			if( VertexInfluences.Influences.Num() > 0 &&
				VertexInfluences.Influences.Num() == LODModel.NumVertices )
			{
				//Temp container to hold bonepair <-> vert list mapping so we can make one TMap at the end
				TArray< TArray<DWORD> > VertListsPerBonePair;
				VertListsPerBonePair.AddZeroed(BonePairs.Num());

				for( INT ChunkIdx=0; ChunkIdx < LODModel.Chunks.Num(); ChunkIdx++ )
				{
					const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIdx);
					for( UINT VertIndex=Chunk.BaseVertexIndex; VertIndex < (Chunk.BaseVertexIndex + Chunk.GetNumVertices()); VertIndex++ )
					{
						// check default mesh vertex bones to see if they match up with any bone pairs
						const FGPUSkinVertexBase* Vertex = LODModel.VertexBufferGPUSkin.GetVertexPtr(VertIndex);
						const FVertexInfluence & AltVertInfluence = VertexInfluences.Influences(VertIndex);

						for( INT BonePairIdx=0; BonePairIdx < BonePairs.Num();  BonePairIdx++ )
						{
							const FBoneIndexPair& BonePair = BonePairs(BonePairIdx);

							UBOOL bAltBoneMatch[2] = {FALSE, FALSE};
							//assume match if invalid index (ie. no parent)
							UBOOL bCurBoneMatch[2] = 
							{	
								BonePair.BoneIdx[0]==INDEX_NONE && BonePair.BoneIdx[1]!=INDEX_NONE,	
								BonePair.BoneIdx[1]==INDEX_NONE && BonePair.BoneIdx[0]!=INDEX_NONE,	
							};

							// match bone 0
							for( INT Idx=0; Idx < MAX_INFLUENCES && !bCurBoneMatch[0]; Idx++ )
							{
								if( Vertex->InfluenceWeights[Idx] > 0 &&
									Chunk.BoneMap(Vertex->InfluenceBones[Idx]) == BonePair.BoneIdx[0] )
								{
									bCurBoneMatch[0] = TRUE;
									break;
								}
							}

							// match bone 1
							for( INT Idx=0; Idx < MAX_INFLUENCES && !bCurBoneMatch[1]; Idx++ )
							{
								if( Vertex->InfluenceWeights[Idx] > 0 &&
									Chunk.BoneMap(Vertex->InfluenceBones[Idx]) == BonePair.BoneIdx[1] )
								{
									bCurBoneMatch[1] = TRUE;
									break;
								}
							}
				
							// first check if this vert is even valid or not
							// if alter weight has this bone,consider
							for( INT Idx=0; Idx < MAX_INFLUENCES ; Idx++ )
							{
								if( AltVertInfluence.Weights.InfluenceWeights[Idx] > 0 &&
									Chunk.BoneMap(AltVertInfluence.Bones.InfluenceBones[Idx]) == BonePair.BoneIdx[0] )
								{
									bAltBoneMatch[0] = TRUE;
									break;
								}
							}

							// match bone 1
							for( INT Idx=0; Idx < MAX_INFLUENCES; Idx++ )
							{
								if( AltVertInfluence.Weights.InfluenceWeights[Idx] > 0 &&
									Chunk.BoneMap(AltVertInfluence.Bones.InfluenceBones[Idx]) == BonePair.BoneIdx[1] )
								{
									bAltBoneMatch[1] = TRUE;
									break;
								}
							}

							// found if both bones are matched
							if ( Option == BONEBREAK_SoftPreferred )
							{
								if( bCurBoneMatch[0] && bCurBoneMatch[1] )
								{
									check(VertIndex < (UINT)MAXDWORD);
									VertListsPerBonePair(BonePairIdx).AddUniqueItem(VertIndex);
								}
							}
							else if ( Option == BONEBREAK_AutoDetect || Option == BONEBREAK_RigidPreferred )
							{
								// now check if soft has it, but not rigid
								// that means it's broken off rigid
								if ( bCurBoneMatch[0] && !bAltBoneMatch[0] )
								{
									check(VertIndex < (UINT)MAXDWORD);
									VertListsPerBonePair(BonePairIdx).AddUniqueItem(VertIndex);										
								}
								else if ( bCurBoneMatch[1] && !bAltBoneMatch[1] )
								{
									check(VertIndex < (UINT)MAXDWORD);
									VertListsPerBonePair(BonePairIdx).AddUniqueItem(VertIndex);										
								}
								// also if only rigid has it, but not in soft
								// usually that means it's broken further away 
								// from normal blending area
								// this case, we assume rigid is going to take it over
								 else if ( Option == BONEBREAK_RigidPreferred )
								 {
									 if (bAltBoneMatch[0] && !bCurBoneMatch[0] )
									{
										check(VertIndex < (UINT)MAXDWORD);
										VertListsPerBonePair(BonePairIdx).AddUniqueItem(VertIndex);										
									}
									else if ( bAltBoneMatch[1] && !bCurBoneMatch[1] )
									{
										check(VertIndex < (UINT)MAXDWORD);
										VertListsPerBonePair(BonePairIdx).AddUniqueItem(VertIndex);										
									}
								}
							}
						}
					}
				}

				//Put all the buckets into the map
				for (INT i=0; i<BonePairs.Num(); i++)
				{
					VertexInfluences.VertexInfluenceMapping.Remove(BonePairs(i));
					if (VertListsPerBonePair(i).Num() > 0)
					{
						debugf(TEXT("Adding verts for Bone (%d, %d) - # %d "), BonePairs(i).BoneIdx[0], BonePairs(i).BoneIdx[1], VertListsPerBonePair(i).Num());
						VertexInfluences.VertexInfluenceMapping.Set(BonePairs(i), VertListsPerBonePair(i));
					}
				}
			}
		}
	}
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxAnimSetViewer
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WxAnimSetViewer::SetSelectedSkelMesh(USkeletalMesh* InSkelMesh, UBOOL bClearMorphTarget, UBOOL bReselectAnimSet/* = TRUE*/)
{
	check(InSkelMesh);

	// Before we change mesh, clear any component we using for previewing attachments.
	ClearSocketPreviews();

	SelectedSkelMesh = InSkelMesh;
	
	// Set up for undo/redo!
	SelectedSkelMesh->SetFlags(RF_Transactional);

	SkelMeshCombo->Freeze();
	SkelMeshCombo->Clear();
	for(TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* SkelMesh = *It;

		if( !(SkelMesh->GetOutermost()->PackageFlags & PKG_Trash) )
		{
			SkelMeshCombo->Append( *SkelMesh->GetName(), SkelMesh );
		}
	}
	SkelMeshCombo->Thaw();

	// Set combo box to reflect new selection.
	for(UINT i=0; i<SkelMeshCombo->GetCount(); i++)
	{
		if( SkelMeshCombo->GetClientData(i) == SelectedSkelMesh )
		{
			SkelMeshCombo->SetSelection(i); // This won't cause a new IDM_ANIMSET_SKELMESHCOMBO event.
		}
	}

	// turn off alt bone editing mode and toggle off mesh weights
	EnableAltBoneWeighting(FALSE);
	ToggleMeshWeights(FALSE);

	//Remove selected bone manipulation
	PreviewSkelComp->BonesOfInterest.Empty();

	// clear all about vertex info tool
	appMemzero(&VertexInfo, sizeof(VertexInfo));
	UpdateVertexMode();
	
	//Remove bone influences being rendered
	BoneInfluenceLODInfo.Empty(SelectedSkelMesh->LODModels.Num());
	BoneInfluenceLODInfo.AddZeroed(SelectedSkelMesh->LODModels.Num());

	//Remove materials saved from previous mesh
	PreviewSkelComp->SkelMaterials.Empty();

	PreviewSkelComp->SetSkeletalMesh(SelectedSkelMesh);
	PreviewSkelComp->ReleaseApexClothing();
	PreviewSkelComp->InitApexClothing(RBPhysScene);
	PreviewSkelCompRaw->SetSkeletalMesh(SelectedSkelMesh);

	// When changing primary skeletal mesh - reset all extra ones.
	SkelMeshAux1Combo->SetSelection(0);
	PreviewSkelCompAux1->SetSkeletalMesh(NULL);
	PreviewSkelCompAux1->UpdateParentBoneMap();

	SkelMeshAux2Combo->SetSelection(0);
	PreviewSkelCompAux2->SetSkeletalMesh(NULL);
	PreviewSkelCompAux2->UpdateParentBoneMap();

	SkelMeshAux3Combo->SetSelection(0);
	PreviewSkelCompAux3->SetSkeletalMesh(NULL);
	PreviewSkelCompAux3->UpdateParentBoneMap();

	MeshProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
	MeshProps->SetObject( SelectedSkelMesh, EPropertyWindowFlags::ShouldShowCategories );

	// Update the Socket Manager
	UpdateSocketList();
	SetSelectedSocket(NULL);
	RecreateSocketPreviews();

	if( bReselectAnimSet )
	{
		// Try and re-select the select AnimSet with the new mesh.
		SetSelectedAnimSet(SelectedAnimSet, false);
	}

	// Select no morph target.
	if(bClearMorphTarget)
	{
		UBOOL bRefreshMorphSet = SelectedSkelMesh->PreviewMorphSets.Num() >  0 && 
			(SelectedMorphSet==NULL || SelectedSkelMesh->PreviewMorphSets.ContainsItem(SelectedMorphSet)==FALSE);

		// if preview  morph set exists, 
		// and current morphset doens't belong to preview morphsets, and refresh to new item
		if ( bRefreshMorphSet )
		{
			for (INT I=0; I<SelectedSkelMesh->PreviewMorphSets.Num(); ++I)
			{
				SetSelectedMorphSet(SelectedSkelMesh->PreviewMorphSets(I), FALSE);
			}
		}
		else
		{
			SetSelectedMorphSet(NULL, FALSE);
		}
	}

	// Reset LOD to Auto mode.
	PreviewSkelComp->ForcedLodModel = 0;
	PreviewSkelCompRaw->ForcedLodModel = 0;
	MenuBar->Check( IDM_ANIMSET_LOD_AUTO, true );
	ToolBar->ToggleTool( IDM_ANIMSET_LOD_AUTO, true );

	// Update the buttons used for changing the desired LOD.
	UpdateForceLODButtons();
	UpdateAltBoneWeightingMenu();

	// Turn off cloth sim when we change mesh.
	ToolBar->ToggleTool( IDM_ANIMSET_TOGGLECLOTH, false );
	PreviewSkelComp->TermClothSim(NULL);
	PreviewSkelComp->bEnableClothSimulation = FALSE;
	
	// Turn off soft-body sim when we change mesh. Reinitialize data buffers for previewing if we already have generated tetras.
	ToolBar->ToggleTool(IDM_ANIMSET_SOFTBODYTOGGLESIM, FALSE);
	PreviewSkelComp->TermSoftBodySim(NULL);
	PreviewSkelComp->bEnableSoftBodySimulation = FALSE;
	if(InSkelMesh->SoftBodyTetraVertsUnscaled.Num() > 0)
	{
		FlushRenderingCommands();
		PreviewSkelComp->InitSoftBodySimBuffers();
	}
	
	// update chunk list
	UpdateMaterialList();
	UpdateStatusBar();

	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check( ASVPreviewVC );
	ASVPreviewVC->Viewport->Invalidate();


	// Remember we want to use this mesh with the current anim/morph set.
	if(SelectedSkelMesh && SelectedAnimSet)
	{
		FString MeshName = SelectedSkelMesh->GetPathName();
		SelectedAnimSet->PreviewSkelMeshName = FName( *MeshName );
	}

	// Fill Skeleton Tree.
	FillSkeletonTree();

	// Refresh the toolbar chunks and sections controls
	UpdateChunkPreview();
	UpdateSectionPreview();

#if WITH_SIMPLYGON
	SimplificationWindow->UpdateControls();
#endif // #if WITH_SIMPLYGON
}

// If trying to set the AnimSet to one that cannot be played on selected SkelMesh, show message and just select first instead.
void WxAnimSetViewer::SetSelectedAnimSet(UAnimSet* InAnimSet, UBOOL bAutoSelectMesh, UBOOL bRefreshAnimSequenceList/* = TRUE*/)
{
	// Only allow selection of compatible AnimSets. 
	// AnimSetCombo should contain all AnimSets that can played on SelectedSkelMesh.

	// In case we have loaded some new packages, rebuild the list before finding the entry.
	UpdateAnimSetCombo();

	INT AnimSetIndex = INDEX_NONE;
	for(UINT i=0; i<AnimSetCombo->GetCount(); i++)
	{
		if(AnimSetCombo->GetClientData(i) == InAnimSet)
		{
			AnimSetIndex = i;
		}
	}

	// If specified AnimSet is not compatible with current skeleton, show message and pick the first one.
	if(AnimSetIndex == INDEX_NONE)
	{
		if(InAnimSet)
		{
			appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_AnimSetCantBePlayedOnSM"), *InAnimSet->GetName(), *SelectedSkelMesh->GetName()) );
		}

		AnimSetIndex = 0;
	}

	// Handle case where combo is empty - select no AnimSet
	if(AnimSetCombo->GetCount() > 0)
	{
		AnimSetCombo->SetSelection(AnimSetIndex);

		// Assign selected AnimSet
		SelectedAnimSet = (UAnimSet*)AnimSetCombo->GetClientData(AnimSetIndex);

		// Add newly selected AnimSet to the AnimSets array of the preview skeletal mesh.
		PreviewSkelComp->AnimSets.Empty();
		PreviewSkelCompRaw->AnimSets.Empty();
		if (SelectedAnimSet)
		{
			PreviewSkelComp->AnimSets.AddItem(SelectedAnimSet);
			PreviewSkelCompRaw->AnimSets.AddItem(SelectedAnimSet);
		}

		AnimSetProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
		AnimSetProps->SetObject( SelectedAnimSet, EPropertyWindowFlags::ShouldShowCategories );
	}
	else
	{
		SelectedAnimSet = NULL;

		PreviewSkelComp->AnimSets.Empty();
		PreviewSkelCompRaw->AnimSets.Empty();

		AnimSetProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
	}

	if( bRefreshAnimSequenceList )
	{
		// Refresh the animation sequence list.
		UpdateAnimSeqList();

		// Keep the anim seq selection if it is owned by the selected set. However, 
		// reselect the seq because it's not guaranteed to be at the same index. 
		if( SelectedAnimSeq && SelectedAnimSeq->GetOuter() == SelectedAnimSet )
		{
			SetSelectedAnimSequence( SelectedAnimSeq );
		}
		// Select the first sequence (if present) - or none at all.
		else if( AnimSeqList->GetCount() > 0 )
		{
			SetSelectedAnimSequence( (UAnimSequence*)(AnimSeqList->GetClientData(0)) );
		}
		else
		{
			SetSelectedAnimSequence( NULL );
		}
	}

	UpdateStatusBar();

	// The menu title bar displays the full path name of the selected AnimSet
	FString WinTitle;
	if(SelectedAnimSet)
	{
		WinTitle = FString::Printf( LocalizeSecure(LocalizeUnrealEd("AnimSetEditor_F"), *SelectedAnimSet->GetPathName()) );
	}
	else
	{
		WinTitle = FString::Printf( *LocalizeUnrealEd("AnimSetEditor") );
	}

	SetTitle( *WinTitle );


	// See if there is a skeletal mesh we would like to use with this AnimSet
	if( bAutoSelectMesh )
	{
		// See first if current mesh works with new animset.
		// Set a threshold of 50% of tracks have to be matched by the skeletal mesh to be valid.
		const UBOOL bHasValidSkelMesh = (SelectedSkelMesh && SelectedAnimSet && SelectedAnimSet->GetSkeletalMeshMatchRatio(SelectedSkelMesh) >= 0.5f - KINDA_SMALL_NUMBER);

		if( !bHasValidSkelMesh && SelectedAnimSet && SelectedAnimSet->PreviewSkelMeshName != NAME_None)
		{
			USkeletalMesh* PreviewSkelMesh = (USkeletalMesh*)UObject::StaticFindObject( USkeletalMesh::StaticClass(), ANY_PACKAGE, *SelectedAnimSet->PreviewSkelMeshName.ToString() );
			if(PreviewSkelMesh)
			{
				// We don't need to reselect the selected anim set 
				// because we just did that in this function!
				const UBOOL bReselectAnimSet = FALSE;
				SetSelectedSkelMesh(PreviewSkelMesh, TRUE, bReselectAnimSet);
			}
		}
	}

	// make sure that the animset preview is pointing the current mesh, we use this for fx preview*/
	// Remember we want to use this mesh with the current anim/morph set.
	if(SelectedSkelMesh && SelectedAnimSet)
	{
		FString MeshName = SelectedSkelMesh->GetPathName();
		SelectedAnimSet->PreviewSkelMeshName = FName( *MeshName );
	}
}

void WxAnimSetViewer::SetSelectedAnimSequence(UAnimSequence* InAnimSeq)
{
	if(!InAnimSeq)
	{
		PreviewAnimNode->SetAnim(NAME_None);
		PreviewAnimNodeRaw->SetAnim(NAME_None);
		SelectedAnimSeq = PreviewAnimNode->AnimSeq;
		AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
	}
	else
	{
		INT AnimSeqIndex = INDEX_NONE;
		for(UINT i=0; i<AnimSeqList->GetCount(); i++)
		{
			if( AnimSeqList->GetClientData(i) == InAnimSeq )
			{
				AnimSeqIndex = i;
			}
		}		

		if(AnimSeqIndex == INDEX_NONE)
		{
			PreviewAnimNode->SetAnim(NAME_None);
			PreviewAnimNodeRaw->SetAnim(NAME_None);
			SelectedAnimSeq = NULL;
			AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
		}
		else
		{
			AnimSeqList->SetSelection( AnimSeqIndex );
			PreviewAnimNode->SetAnim( InAnimSeq->SequenceName );
			PreviewAnimNodeRaw->SetAnim( InAnimSeq->SequenceName );
			SelectedAnimSeq = InAnimSeq;

			AnimSeqProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
			AnimSeqProps->SetObject( SelectedAnimSeq, EPropertyWindowFlags::ShouldShowCategories );
		}
	}

	// if you set new animsequence, remove any arrow component that was attached to the mesh before
	// this isn't best way to fix this issue, but if anim notify attach anything, 
	// there is no way to clear it, so remove any arrow components that was attached 
	if ( PreviewSkelComp )
	{
		PreviewSkelComp->DetachAnyOf(UArrowComponent::StaticClass());
	}

	if ( PreviewSkelCompRaw )
	{
		PreviewSkelCompRaw->DetachAnyOf(UArrowComponent::StaticClass());
	}

	UpdateStatusBar();
}

/** Set the supplied MorphTargetSet as the selected one. */
void WxAnimSetViewer::SetSelectedMorphSet(UMorphTargetSet* InMorphSet, UBOOL bAutoSelectMesh)
{
	// In case we have loaded some new packages, rebuild the list before finding the entry.
	UpdateMorphSetCombo();

	INT MorphSetIndex = INDEX_NONE;
	for(UINT i=0; i<MorphSetCombo->GetCount(); i++)
	{
		if(MorphSetCombo->GetClientData(i) == InMorphSet)
		{
			MorphSetIndex = i;
		}
	}

	if(MorphSetIndex == INDEX_NONE)
	{
		// set to first one
		MorphSetIndex = 0;
	}

	// Combo should never be empty - there is always a -None- slot.
	if ( MorphSetCombo->GetCount() > 0 )
	{
		MorphSetCombo->SetSelection(MorphSetIndex);

		SelectedMorphSet = (UMorphTargetSet*)MorphSetCombo->GetClientData(MorphSetIndex);

		// for now, just fill up if not exists only
 		if ( SelectedMorphSet->IsValidBaseMesh() == FALSE)
 		{
			// ask if it would be okay to update morph targets
			if ( appMsgf( AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_36") ) == 0 )
			{
				SelectedMorphSet->UpdateMorphTargetsFromBaseMesh();
			}
 		}
		else
		{
			SelectedMorphSet->FillBaseMeshData(TRUE);
		}
		// Note, it may be NULL at this point, because combo contains a 'None' entry.

		// assign MorphTargetSet to array in SkeletalMeshComponent. 
		PreviewSkelComp->MorphSets.Empty();
		PreviewSkelCompRaw->MorphSets.Empty();
		PreviewSkelComp->MorphTargetsQueried.Empty();
		PreviewSkelCompRaw->MorphTargetsQueried.Empty();

		if(SelectedMorphSet)
		{
			PreviewSkelComp->MorphSets.AddItem(SelectedMorphSet);
			PreviewSkelCompRaw->MorphSets.AddItem(SelectedMorphSet);
			PreviewSkelComp->InitMorphTargets();
			PreviewSkelCompRaw->MorphTargetsQueried = PreviewSkelComp->MorphTargetsQueried;	// Copy to prevent spam
			PreviewSkelCompRaw->InitMorphTargets();
		}
	}
	else
	{
		SelectedMorphSet = NULL;

		PreviewSkelComp->MorphSets.Empty();
		PreviewSkelCompRaw->MorphSets.Empty();
		PreviewSkelComp->MorphTargetsQueried.Empty();
		PreviewSkelCompRaw->MorphTargetsQueried.Empty();

		MorphTargetProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
	}

	// Refresh the animation sequence list.
	UpdateMorphTargetList();

	UpdateStatusBar();

	// Set the BaseSkelMesh as the selected skeletal mesh.
	if( bAutoSelectMesh && 
		SelectedMorphSet && 
		SelectedMorphSet->BaseSkelMesh && 
		SelectedMorphSet->BaseSkelMesh != SelectedSkelMesh)
	{
		SetSelectedSkelMesh(SelectedMorphSet->BaseSkelMesh, FALSE);
	}
}

/** Set the selected MorphTarget as the desired. */
void WxAnimSetViewer::UpdateSelectedMorphTargets()
{
	// this is for propery window/and potentially delete operation
	SelectedMorphTargets.Empty();

	if ( SelectedMorphSet )
	{
		for ( INT I=0; I<SelectedMorphSet->Targets.Num(); ++I )
		{
			if ( MorphTargetPanel->IsSelected(I) )
			{
				SelectedMorphTargets.AddItem(SelectedMorphSet->Targets(I));
			}
		}

		if ( SelectedMorphTargets.Num() > 0 )
		{
			MorphTargetProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
			MorphTargetProps->SetObjectArray(SelectedMorphTargets, EPropertyWindowFlags::ShouldShowCategories );
		}
		else
		{
			MorphTargetProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
		}
	}
	else
	{
		PreviewMorphPose->ClearAll();
		MorphTargetProps->SetObject( NULL, EPropertyWindowFlags::ShouldShowCategories );
	}


}


#if WITH_FBX
// TODO: merge this function with ImportPSA()
void WxAnimSetViewer::ImportFbxAnim()
{
	if(!SelectedAnimSet)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_SelectAnimSetForImport"), ANSI_TO_TCHAR("FBX") ));
		return;
	}
	
	// Prevent animations from being imported into a cooked anim set.
	UPackage* Package = SelectedAnimSet->GetOutermost();
	if( Package->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	WxFileDialog ImportFileDialog( this, 
		*LocalizeUnrealEd("ImportFbx_Anim_File"), 
		*(GApp->LastDir[LD_FBX_ANIM]),
		TEXT(""),
		TEXT("FBX files|*.fbx"),
		wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE,
		wxDefaultPosition);

	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		// Remember if this AnimSet was brand new
		const UBOOL bSetWasNew = SelectedAnimSet->TrackBoneNames.Num() == 0 ? TRUE : FALSE;

		wxArrayString ImportFilePaths;
		ImportFileDialog.GetPaths(ImportFilePaths);

		UBOOL bMissingTrackError = FALSE;
		for(UINT FileIndex = 0; FileIndex < ImportFilePaths.Count(); FileIndex++)
		{
			const FFilename Filename( (const TCHAR*) ImportFilePaths[FileIndex] );
			GApp->LastDir[LD_FBX_ANIM] = Filename.GetPath(); // Save path as default for next time.

			UEditorEngine::ImportFbxANIMIntoAnimSet( SelectedAnimSet, *Filename, SelectedSkelMesh, TRUE, bMissingTrackError );

			SelectedAnimSet->MarkPackageDirty();
		}

		// If this set was new - we have just created the bone-track-table.
		// So we now need to check if we can still play it on our selected SkeletalMesh.
		// If we can't, we look for a few one which can play it.
		// If we can't find any mesh to play this new set on, we fail the import and reset the AnimSet.
		if(bSetWasNew)
		{
			if( !SelectedAnimSet->CanPlayOnSkeletalMesh(SelectedSkelMesh) )
			{
				USkeletalMesh* NewSkelMesh = NULL;
				for(TObjectIterator<USkeletalMesh> It; It && !NewSkelMesh; ++It)
				{
					USkeletalMesh* TestSkelMesh = *It;
					if( SelectedAnimSet->CanPlayOnSkeletalMesh(TestSkelMesh) )
					{
						NewSkelMesh = TestSkelMesh;
					}
				}

				if(NewSkelMesh)
				{
					SetSelectedSkelMesh(NewSkelMesh, TRUE);
				}
				else
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_AnimSetCouldNotBePlayed"), ANSI_TO_TCHAR("FBX") ));
					SelectedAnimSet->ResetAnimSet();
					return;
				}
			}
		}

		// Refresh AnimSet combo to show new number of sequences in this AnimSet.
		const INT CurrentSelection = AnimSetCombo->GetSelection();
		UpdateAnimSetCombo();
		AnimSetCombo->SetSelection(CurrentSelection);

		// Refresh AnimSequence list box to show any new anims.
		UpdateAnimSeqList();

		// Reselect current animation sequence. If none selected, pick the first one in the box (if not empty).
		if(SelectedAnimSeq)
		{
			SetSelectedAnimSequence(SelectedAnimSeq);
		}
		else
		{
			if(AnimSeqList->GetCount() > 0)
			{
				SetSelectedAnimSequence( (UAnimSequence*)(AnimSeqList->GetClientData(0)) );
			}
		}

		SelectedAnimSet->MarkPackageDirty();
	}
	return; 	
}


void WxAnimSetViewer::ExportFbxAnim()
{
	if(!SelectedAnimSet || !SelectedAnimSeq)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_SelectAnimSetForExport"), ANSI_TO_TCHAR("FBX") ));
		return;
	}

	FString DefaultFileName = SelectedAnimSeq->SequenceName.GetNameString() + TEXT(".fbx");

	WxFileDialog ExportFileDialog(this, 
		*LocalizeUnrealEd("ExportMatineeAnimTrack"), 
		*(GApp->LastDir[LD_FBX_ANIM]), 
		*DefaultFileName, 
		TEXT("FBX document|*.fbx"),
		wxSAVE | wxOVERWRITE_PROMPT, 
		wxDefaultPosition);

	// Show dialog and execute the import if the user did not cancel out
	if( ExportFileDialog.ShowModal() == wxID_OK )
	{
		// Get the filename from dialog
		wxString ExportFilename = ExportFileDialog.GetPath();
		FFilename FileName = ExportFilename.c_str();
		GApp->LastDir[LD_FBX_ANIM] = FileName.GetPath(); // Save path as default for next time.

		// Ask the user if they would like to include the skeletal mesh with the animation
		WxChoiceDialog SaveMeshDialog(
			*LocalizeUnrealEd( TEXT("AnimSetViewer_ExportMeshWithAnimPrompt") ),
			*LocalizeUnrealEd( TEXT("AnimSetViewer_ExportMeshWithAnim") ),
			WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd( TEXT("GenericDialog_Yes") ), WxChoiceDialogBase::DCT_DefaultAffirmative ),
			WxChoiceDialogBase::Choice( AMT_OKCancel, *LocalizeUnrealEd( TEXT("GenericDialog_No") ), WxChoiceDialogBase::DCT_DefaultCancel )
			);

		SaveMeshDialog.ShowModal();

		BOOL bSaveSkeletalMesh = ( SaveMeshDialog.GetChoice().ReturnCode == AMT_OK );

		UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();

		Exporter->CreateDocument();
		
		Exporter->ExportAnimSequence(SelectedAnimSeq, SelectedSkelMesh, bSaveSkeletalMesh);

		// Save to disk
		Exporter->WriteToFile( ExportFilename.c_str() );
	}
}
#endif // WITH_FBX

void WxAnimSetViewer::ImportPSA()
{
	if(!SelectedAnimSet)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_SelectAnimSetForImport"), ANSI_TO_TCHAR("PSA") ));
		return;
	}

	// Prevent animations from being imported into a cooked anim set.
	UPackage* Package = SelectedAnimSet->GetOutermost();
	if( Package->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	WxFileDialog ImportFileDialog( this, 
		*LocalizeUnrealEd("ImportPSAFile"), 
		*(GApp->LastDir[LD_PSA]),
		TEXT(""),
		TEXT("PSA files|*.psa"),
		wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE,
		wxDefaultPosition);

	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		// Remember if this AnimSet was brand new
		const UBOOL bSetWasNew = SelectedAnimSet->TrackBoneNames.Num() == 0 ? TRUE : FALSE;

		wxArrayString ImportFilePaths;
		ImportFileDialog.GetPaths(ImportFilePaths);

		for(UINT FileIndex = 0; FileIndex < ImportFilePaths.Count(); FileIndex++)
		{
			const FFilename Filename( (const TCHAR*)ImportFilePaths[FileIndex] );
			GApp->LastDir[LD_PSA] = Filename.GetPath(); // Save path as default for next time.

			UEditorEngine::ImportPSAIntoAnimSet( SelectedAnimSet, *Filename, SelectedSkelMesh );

			SelectedAnimSet->MarkPackageDirty();
		}

		// If this set was new - we have just created the bone-track-table.
		// So we now need to check if we can still play it on our selected SkeletalMesh.
		// If we can't, we look for a few one which can play it.
		// If we can't find any mesh to play this new set on, we fail the import and reset the AnimSet.
		if(bSetWasNew)
		{
			if( !SelectedAnimSet->CanPlayOnSkeletalMesh(SelectedSkelMesh) )
			{
				USkeletalMesh* NewSkelMesh = NULL;
				for(TObjectIterator<USkeletalMesh> It; It && !NewSkelMesh; ++It)
				{
					USkeletalMesh* TestSkelMesh = *It;
					if( SelectedAnimSet->CanPlayOnSkeletalMesh(TestSkelMesh) )
					{
						NewSkelMesh = TestSkelMesh;
					}
				}

				if(NewSkelMesh)
				{
					SetSelectedSkelMesh(NewSkelMesh, true);
				}
				else
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_AnimSetCouldNotBePlayed"), ANSI_TO_TCHAR("PSA") ));
					SelectedAnimSet->ResetAnimSet();
					return;
				}
			}
		}

		// Refresh AnimSet combo to show new number of sequences in this AnimSet.
		const INT CurrentSelection = AnimSetCombo->GetSelection();
		UpdateAnimSetCombo();
		AnimSetCombo->SetSelection(CurrentSelection);
			
		// Refresh AnimSequence list box to show any new anims.
		UpdateAnimSeqList();

		// Reselect current animation sequence. If none selected, pick the first one in the box (if not empty).
		if(SelectedAnimSeq)
		{
			SetSelectedAnimSequence(SelectedAnimSeq);
		}
		else
		{
			if(AnimSeqList->GetCount() > 0)
			{
				SetSelectedAnimSequence( (UAnimSequence*)(AnimSeqList->GetClientData(0)) );
			}
		}

		SelectedAnimSet->MarkPackageDirty();
	}
}

/**
* Import the temporary skeletal mesh into the specified LOD of the currently selected skeletal mesh
*
* @param InSkeletalMesh - newly created mesh used as LOD
* @param DesiredLOD - the LOD index to import into. A new LOD entry is created if one doesn't exist
* @return If true, import succeeded
*/
UBOOL WxAnimSetViewer::ImportMeshLOD(USkeletalMesh* InSkeletalMesh, INT DesiredLOD)
{
	check(InSkeletalMesh);
	// Now we copy the base FStaticLODModel from the imported skeletal mesh as the new LOD in the selected mesh.
	check(InSkeletalMesh->LODModels.Num() == 1);

	// Names of root bones must match.
	if(InSkeletalMesh->RefSkeleton(0).Name != SelectedSkelMesh->RefSkeleton(0).Name)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODRootNameIncorrect"), *InSkeletalMesh->RefSkeleton(0).Name.ToString(), *SelectedSkelMesh->RefSkeleton(0).Name.ToString()) );
		return FALSE;
	}

	// We do some checking here that for every bone in the mesh we just imported, it's in our base ref skeleton, and the parent is the same.
	for(INT i=0; i<InSkeletalMesh->RefSkeleton.Num(); i++)
	{
		INT LODBoneIndex = i;
		FName LODBoneName = InSkeletalMesh->RefSkeleton(LODBoneIndex).Name;
		INT BaseBoneIndex = SelectedSkelMesh->MatchRefBone(LODBoneName);
		if( BaseBoneIndex == INDEX_NONE )
		{
			// If we could not find the bone from this LOD in base mesh - we fail.
			appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODBoneDoesNotMatch"), *LODBoneName.ToString(), *SelectedSkelMesh->GetName()) );
			return FALSE;
		}

		if(i>0)
		{
			INT LODParentIndex = InSkeletalMesh->RefSkeleton(LODBoneIndex).ParentIndex;
			FName LODParentName = InSkeletalMesh->RefSkeleton(LODParentIndex).Name;

			INT BaseParentIndex = SelectedSkelMesh->RefSkeleton(BaseBoneIndex).ParentIndex;
			FName BaseParentName = SelectedSkelMesh->RefSkeleton(BaseParentIndex).Name;

			if(LODParentName != BaseParentName)
			{
				// If bone has different parents, display an error and don't allow import.
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODBoneHasIncorrectParent"), *LODBoneName.ToString(), *LODParentName.ToString(), *BaseParentName.ToString()) );
				return FALSE;
			}
		}
	}

	FStaticLODModel& NewLODModel = InSkeletalMesh->LODModels(0);

	// Enforce LODs having only single-influence vertices.
	UBOOL bCheckSingleInfluence;
	GConfig->GetBool( TEXT("AnimSetViewer"), TEXT("CheckSingleInfluenceLOD"), bCheckSingleInfluence, GEditorIni );
	if( bCheckSingleInfluence && 
		DesiredLOD > 0 )
	{
		for(INT ChunkIndex = 0;ChunkIndex < NewLODModel.Chunks.Num();ChunkIndex++)
		{
			if(NewLODModel.Chunks(ChunkIndex).SoftVertices.Num() > 0)
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("LODHasSoftVertices") );
			}
		}
	}

	// If this LOD is going to be the lowest one, we check all bones we have sockets on are present in it.
	if( DesiredLOD == SelectedSkelMesh->LODModels.Num() || 
		DesiredLOD == SelectedSkelMesh->LODModels.Num()-1 )
	{
		for(INT i=0; i<SelectedSkelMesh->Sockets.Num(); i++)
		{
			// Find bone index the socket is attached to.
			USkeletalMeshSocket* Socket = SelectedSkelMesh->Sockets(i);
			INT SocketBoneIndex = InSkeletalMesh->MatchRefBone( Socket->BoneName );

			// If this LOD does not contain the socket bone, abort import.
			if( SocketBoneIndex == INDEX_NONE )
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODMissingSocketBone"), *Socket->BoneName.ToString(), *Socket->SocketName.ToString()) );
				return FALSE;
			}
		}
	}

	// Fix up the ActiveBoneIndices array.
	for(INT i=0; i<NewLODModel.ActiveBoneIndices.Num(); i++)
	{
		INT LODBoneIndex = NewLODModel.ActiveBoneIndices(i);
		FName LODBoneName = InSkeletalMesh->RefSkeleton(LODBoneIndex).Name;
		INT BaseBoneIndex = SelectedSkelMesh->MatchRefBone(LODBoneName);
		NewLODModel.ActiveBoneIndices(i) = BaseBoneIndex;
	}

	// Fix up the chunk BoneMaps.
	for(INT ChunkIndex = 0;ChunkIndex < NewLODModel.Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = NewLODModel.Chunks(ChunkIndex);
		for(INT i=0; i<Chunk.BoneMap.Num(); i++)
		{
			INT LODBoneIndex = Chunk.BoneMap(i);
			FName LODBoneName = InSkeletalMesh->RefSkeleton(LODBoneIndex).Name;
			INT BaseBoneIndex = SelectedSkelMesh->MatchRefBone(LODBoneName);
			Chunk.BoneMap(i) = BaseBoneIndex;
		}
	}

	// Create the RequiredBones array in the LODModel from the ref skeleton.
	for(INT i=0; i<NewLODModel.RequiredBones.Num(); i++)
	{
		FName LODBoneName = InSkeletalMesh->RefSkeleton(NewLODModel.RequiredBones(i)).Name;
		INT BaseBoneIndex = SelectedSkelMesh->MatchRefBone(LODBoneName);
		if(BaseBoneIndex != INDEX_NONE)
		{
			NewLODModel.RequiredBones(i) = BaseBoneIndex;
		}
		else
		{
			NewLODModel.RequiredBones.Remove(i--);
		}
	}

	// Also sort the RequiredBones array to be strictly increasing.
	Sort<USE_COMPARE_CONSTREF(BYTE, AnimSetViewerTools)>( &NewLODModel.RequiredBones(0), NewLODModel.RequiredBones.Num() );

	// To be extra-nice, we apply the difference between the root transform of the meshes to the verts.
	FMatrix LODToBaseTransform = InSkeletalMesh->GetRefPoseMatrix(0).Inverse() * SelectedSkelMesh->GetRefPoseMatrix(0);

	for(INT ChunkIndex = 0;ChunkIndex < NewLODModel.Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = NewLODModel.Chunks(ChunkIndex);
		// Fix up rigid verts.
		for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
		{
			Chunk.RigidVertices(i).Position = LODToBaseTransform.TransformFVector( Chunk.RigidVertices(i).Position );
			Chunk.RigidVertices(i).TangentX = LODToBaseTransform.TransformNormal( Chunk.RigidVertices(i).TangentX );
			Chunk.RigidVertices(i).TangentY = LODToBaseTransform.TransformNormal( Chunk.RigidVertices(i).TangentY );
			Chunk.RigidVertices(i).TangentZ = LODToBaseTransform.TransformNormal( Chunk.RigidVertices(i).TangentZ );
		}

		// Fix up soft verts.
		for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
		{
			Chunk.SoftVertices(i).Position = LODToBaseTransform.TransformFVector( Chunk.SoftVertices(i).Position );
			Chunk.SoftVertices(i).TangentX = LODToBaseTransform.TransformNormal( Chunk.SoftVertices(i).TangentX );
			Chunk.SoftVertices(i).TangentY = LODToBaseTransform.TransformNormal( Chunk.SoftVertices(i).TangentY );
			Chunk.SoftVertices(i).TangentZ = LODToBaseTransform.TransformNormal( Chunk.SoftVertices(i).TangentZ );
		}
	}

	// Shut down the skeletal mesh component that is previewing this mesh.
	{
		FComponentReattachContext ReattachContextPreview(PreviewSkelComp);
		FComponentReattachContext ReattachContextPreviewRaw(PreviewSkelCompRaw);

		// If we want to add this as a new LOD to this mesh - add to LODModels/LODInfo array.
		if(DesiredLOD == SelectedSkelMesh->LODModels.Num())
		{
			new(SelectedSkelMesh->LODModels)FStaticLODModel();

			// Add element to LODInfo array.
			SelectedSkelMesh->LODInfo.AddZeroed();
			check( SelectedSkelMesh->LODInfo.Num() == SelectedSkelMesh->LODModels.Num() );
			SelectedSkelMesh->LODInfo(DesiredLOD) = InSkeletalMesh->LODInfo(0);
		}
		else
		{
			// if it's overwriting existing LOD, need to update section information
			// update to the right # of sections 
			// Set up LODMaterialMap to number of materials in new mesh.
			// InSkeletalMesh->LOD 0 is the newly imported mesh
			FSkeletalMeshLODInfo& LODInfo = SelectedSkelMesh->LODInfo(DesiredLOD);
			FStaticLODModel& LODModel = InSkeletalMesh->LODModels(0);
			// if section # has been changed
			if (LODInfo.bEnableShadowCasting.Num() != LODModel.Sections.Num())
			{
				// Save old information so that I can copy it over
				TArray<UBOOL> OldEnableShadowCasting;
				TArray<FTriangleSortSettings> OldTriangleSortSettings;

				OldEnableShadowCasting = LODInfo.bEnableShadowCasting;
				OldTriangleSortSettings = LODInfo.TriangleSortSettings;

				// resize to the correct number
				LODInfo.bEnableShadowCasting.Empty(LODModel.Sections.Num());
				LODInfo.TriangleSortSettings.Empty(LODModel.Sections.Num());
				// fill up data
				for ( INT SectionIndex = 0 ; SectionIndex < LODModel.Sections.Num() ; ++SectionIndex )
				{
					// if found from previous data, copy over
					if ( SectionIndex < OldEnableShadowCasting.Num() )
					{
						LODInfo.bEnableShadowCasting.AddItem( OldEnableShadowCasting(SectionIndex) );
						LODInfo.TriangleSortSettings.AddItem( OldTriangleSortSettings(SectionIndex) );
					}
					else
					{
						// if not add default data
						LODInfo.bEnableShadowCasting.AddItem( TRUE );
						LODInfo.TriangleSortSettings.AddZeroed();
					}
				}
			}
		}

		// Set up LODMaterialMap to number of materials in new mesh.
		FSkeletalMeshLODInfo& LODInfo = SelectedSkelMesh->LODInfo(DesiredLOD);
		LODInfo.LODMaterialMap.Empty();

		// Now set up the material mapping array.
		for(INT MatIdx = 0; MatIdx < InSkeletalMesh->Materials.Num(); MatIdx++)
		{
			// Try and find the auto-assigned material in the array.
			INT LODMatIndex = INDEX_NONE;
			if (InSkeletalMesh->Materials(MatIdx) != NULL)
			{
				LODMatIndex = SelectedSkelMesh->Materials.FindItemIndex( InSkeletalMesh->Materials(MatIdx) );
			}

			// If we didn't just use the index - but make sure its within range of the Materials array.
			if(LODMatIndex == INDEX_NONE)
			{
				LODMatIndex = ::Clamp(MatIdx, 0, SelectedSkelMesh->Materials.Num() - 1);
			}

			LODInfo.LODMaterialMap.AddItem(LODMatIndex);
		}

		// if new LOD has more material slot, add the extra to main skeletal
		if ( SelectedSkelMesh->Materials.Num() < InSkeletalMesh->Materials.Num() )
		{
			SelectedSkelMesh->Materials.AddZeroed(InSkeletalMesh->Materials.Num()-SelectedSkelMesh->Materials.Num());
		}

		// Release all resources before replacing the model
		SelectedSkelMesh->PreEditChange(NULL);

		// Index buffer will be destroyed when we copy the LOD model so we must copy the index buffer and reinitialize it after the copy
		FMultiSizeIndexContainerData Data;
		NewLODModel.MultiSizeIndexContainer.GetIndexBufferData( Data );

		// Assign new FStaticLODModel to desired slot in selected skeletal mesh.
		SelectedSkelMesh->LODModels(DesiredLOD) = NewLODModel;

		SelectedSkelMesh->LODModels(DesiredLOD).MultiSizeIndexContainer.RebuildIndexBuffer( Data );
		
		// rebuild vertex buffers and reinit RHI resources
		SelectedSkelMesh->PostEditChange();		

		// ReattachContexts go out of scope here, reattaching skel components to the scene.
	}

	// Now that the mesh is back together, recalculate the bone break weighting if we previously had done them
	for (INT LODIdx = 0; LODIdx<PreviewSkelComp->LODInfo.Num(); LODIdx++)
	{
		PreviewSkelComp->ToggleInstanceVertexWeights(FALSE, LODIdx);
	}
	
	if (SelectedSkelMesh->BoneBreakNames.Num() > 0)
	{
		for (BYTE Option=BONEBREAK_SoftPreferred; Option<=BONEBREAK_RigidPreferred; ++Option)
		{
			TArray<FBoneIndexPair> BoneIndexPairs;
			BoneIndexPairs.Empty();
			for (INT BoneNameIdx=0; BoneNameIdx < SelectedSkelMesh->BoneBreakNames.Num(); BoneNameIdx++)
			{
				const FString& BoneName = SelectedSkelMesh->BoneBreakNames(BoneNameIdx);
				if (SelectedSkelMesh->BoneBreakOptions(BoneNameIdx) == Option)
				{
					const FName BoneFName(*BoneName);
					INT BoneIndex = SelectedSkelMesh->MatchRefBone(BoneFName);
					if (BoneIndex != INDEX_NONE)
					{
						PreviewSkelComp->RemoveInstanceVertexWeightBoneParented(BoneFName);
						FBoneIndexPair NewBoneIndexPair;
						NewBoneIndexPair.BoneIdx[0] = BoneIndex;
						NewBoneIndexPair.BoneIdx[1] = SelectedSkelMesh->RefSkeleton(BoneIndex).ParentIndex;
						BoneIndexPairs.AddItem(NewBoneIndexPair);
					}
				}
			}

			if (BoneIndexPairs.Num() > 0)
			{
				//Add the collection of bones to the bone influences list
				CalculateBoneWeightInfluences(SelectedSkelMesh, BoneIndexPairs, (EBoneBreakOption) Option);
			}
		}

		//Reset gore mode state
		EnableAltBoneWeighting(PreviewSkelComp->bDrawBoneInfluences);
	}

	return TRUE;
}


/** Import a .PSK or .FBX file as an LOD of the selected SkeletalMesh. */
void WxAnimSetViewer::ImportMeshLOD()
{
	if(!SelectedSkelMesh)
	{
		return;
	}

	const UPackage* SkelMeshPackage = SelectedSkelMesh->GetOutermost();
	if( SkelMeshPackage->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	FString ExtensionStr;
#if WITH_ACTORX
	ExtensionStr += TEXT("PSK files|*.psk|");
#endif
#if WITH_FBX
	ExtensionStr += TEXT("FBX files|*.fbx|");
#endif
	ExtensionStr += TEXT("All files|*.*");

	// First, display the file open dialog for selecting the .PSK file.
	WxFileDialog ImportFileDialog( this, 
		*LocalizeUnrealEd("ImportMeshLOD"), 
		*(GApp->LastDir[LD_PSA]),
		TEXT(""),
		*ExtensionStr,
		wxOPEN | wxFILE_MUST_EXIST,
		wxDefaultPosition);

	// Only continue if we pressed OK and have only one file selected.
	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		wxArrayString ImportFilePaths;
		ImportFileDialog.GetPaths(ImportFilePaths);

		if(ImportFilePaths.Count() == 0)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("NoFileSelectedForLOD") );
		}
		else if(ImportFilePaths.Count() > 1)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("MultipleFilesSelectedForLOD") );
		}
		else
		{

			FFilename Filename = (const TCHAR*)ImportFilePaths[0];
			GApp->LastDir[LD_PSA] = Filename.GetPath(); // Save path as default for next time.

#if WITH_FBX
			// Check the file extension for PSK/FBX switch. Anything that isn't .FBX is considered a PSK file.
			const FString FileExtension = Filename.GetExtension();
			const UBOOL bIsFBX = appStricmp(*FileExtension, TEXT("FBX")) == 0;

			if (bIsFBX)
			{
				UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();
				// don't import material and animation
				UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();
				ImportOptions->bImportMaterials = FALSE;
				ImportOptions->bImportTextures = FALSE;
				ImportOptions->bImportAnimSet = FALSE;

				if ( !FbxImporter->ImportFromFile( *Filename ) )
				{
					// Log the error message and fail the import.
					debugf(TEXT("FBX file parse failed"));
					//Warn->Log( NAME_Error, FbxImporter->GetErrorMessage() );
				}
				else
				{
					UBOOL bUseLODs = TRUE;
					INT MaxLODLevel = 0;
					TArray< TArray<FbxNode*>* > MeshArray;
					TArray<FString> LODStrings;
					TArray<FbxNode*>* MeshObject = NULL;;

					// Populate the mesh array
					FbxImporter->FillFbxSkelMeshArrayInScene(FbxImporter->Scene->GetRootNode(), MeshArray, FALSE);

					// Nothing found, error out
					if (MeshArray.Num() == 0)
					{
						appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_NoMeshFound") );
						FbxImporter->ReleaseScene();
						return;
					}

					MeshObject = MeshArray(0);
						
					// check if there is LODGroup for this skeletal mesh
					for (INT j = 0; j < MeshObject->Num(); j++)
					{
						FbxNode* Node = (*MeshObject)(j);
						if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
						{
							// get max LODgroup level
							if (MaxLODLevel < (Node->GetChildCount() - 1))
							{
								MaxLODLevel = Node->GetChildCount() - 1;
							}
						}
					}

					// No LODs found, switch to supporting a mesh array containing meshes instead of LODs
					if (MaxLODLevel == 0)
					{
						bUseLODs = FALSE;
						MaxLODLevel = SelectedSkelMesh->LODInfo.Num();
					}

					// Create LOD dropdown strings
					LODStrings.AddZeroed(MaxLODLevel + 1);
					LODStrings(0) = FString::Printf( TEXT("Base") );
					for(INT i = 1; i < MaxLODLevel + 1; i++)
					{
						LODStrings(i) = FString::Printf(TEXT("%d"), i);
					}

					// Display the LOD selection dialog
					WxDlgGenericComboEntry Dlg;
					if( Dlg.ShowModal( TEXT("ChooseLODLevel"), TEXT("LODLevel:"), LODStrings, 0, TRUE ) == wxID_OK )
					{
						INT SelectedLOD = Dlg.GetComboBox().GetSelection();

						if (SelectedLOD > SelectedSkelMesh->LODInfo.Num())
						{
							// Make sure they don't manage to select a bad LOD index
							appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Prompt_InvalidLODIndex"), SelectedLOD));
						}
						else
						{
							// Find the LOD node to import (if using LODs)
							TArray<FbxNode*> SkelMeshNodeArray;
							if (bUseLODs)
							{
								for (INT j = 0; j < MeshObject->Num(); j++)
								{
									FbxNode* Node = (*MeshObject)(j);
									if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
									{
										if (Node->GetChildCount() > SelectedLOD)
										{
											SkelMeshNodeArray.AddItem(Node->GetChild(SelectedLOD));
										}
										else // in less some LODGroups have less level, use the last level
										{
											SkelMeshNodeArray.AddItem(Node->GetChild(Node->GetChildCount() - 1));
										}
									}
									else
									{
										SkelMeshNodeArray.AddItem(Node);
									}
								}
							}

							// Import mesh
							USkeletalMesh* TempSkelMesh = NULL;
							TempSkelMesh = (USkeletalMesh*)FbxImporter->ImportSkeletalMesh(UObject::GetTransientPackage(), bUseLODs? SkelMeshNodeArray: *MeshObject, NAME_None, 0, Filename.GetBaseFilename());

							// Add imported mesh to existing model
							UBOOL bImportSucceeded = FALSE;
							if( TempSkelMesh )
							{
								bImportSucceeded = ImportMeshLOD(TempSkelMesh, SelectedLOD);

								// Update buttons to reflect new LOD.
								UpdateForceLODButtons();

								// Mark package containing skeletal mesh as dirty.
								SelectedSkelMesh->MarkPackageDirty();
							}

							if (bImportSucceeded)
							{
								// Pop up a box saying it worked.
								appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODImportSuccessful"), SelectedLOD) );
							}
							else
							{
								// Import failed
								appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODImportFail"), SelectedLOD) );
							}
						}
					}
				
					// Cleanup
					for (INT i=0; i<MeshArray.Num(); i++)
					{
						delete MeshArray(i);
					}					
				}
				FbxImporter->ReleaseScene();
			}
			else
#endif
			{
#if WITH_ACTORX
			    // Now display combo to choose which LOD to import this mesh as.
			    TArray<FString> LODStrings;
			    LODStrings.AddZeroed( SelectedSkelMesh->LODModels.Num() );
			    for(INT i=0; i<SelectedSkelMesh->LODModels.Num(); i++)
			    {
				    LODStrings(i) = FString::Printf( TEXT("%d"), i+1 );
			    }
    
			    WxDlgGenericComboEntry dlg;
			    if( dlg.ShowModal( TEXT("ChooseLODLevel"), TEXT("LODLevel:"), LODStrings, 0, TRUE ) == wxID_OK )
			    {
				    INT DesiredLOD = dlg.GetComboBox().GetSelection() + 1;
    
				    // If the LOD we want
				    if( DesiredLOD > 0 && DesiredLOD <= SelectedSkelMesh->LODModels.Num() )
				    {
					    // Load the data from the file into a byte array.
					    TArray<BYTE> Data;
					    if( appLoadFileToArray( Data, *Filename ) )
					    {
						    Data.AddItem( 0 );
						    const BYTE* Ptr = &Data( 0 );
    
						    // Use the SkeletalMeshFactory to load this SkeletalMesh into a temporary SkeletalMesh.
						    USkeletalMeshFactory* SkelMeshFact = new USkeletalMeshFactory();
						    USkeletalMesh* TempSkelMesh = (USkeletalMesh*)SkelMeshFact->FactoryCreateBinary( 
							    USkeletalMesh::StaticClass(), UObject::GetTransientPackage(), NAME_None, 0, NULL, NULL, Ptr, Ptr+Data.Num()-1, GWarn );
						    if( TempSkelMesh )
						    {
							    if( ImportMeshLOD(TempSkelMesh,DesiredLOD) )
									{
										// Pop up a box saying it worked.
										appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODImportSuccessful"), DesiredLOD) );
									}
									else
									{
										// Pop up a box saying it failed.
										appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODImportUnsuccessful"), DesiredLOD) );
									}

							    // Update buttons to reflect new LOD.
							    UpdateForceLODButtons();

							    // Mark package containing skeletal mesh as dirty.
							    SelectedSkelMesh->MarkPackageDirty();
						    }
					    }
				    }
			    }
#else
				appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ActorXDeprecated") );
#endif
			}
		}
	}
}

#if WITH_FBX
extern FbxNode* GetFirstFbxMesh(FbxNode* Node, UBOOL bIsSkelMesh);
#endif

/**
* Import a .psk file as a skeletal mesh and copy its bone influence weights 
* to the existing skeletal mesh.  The psk for the base skeletal mesh must also be specified
*/
void WxAnimSetViewer::ImportMeshWeights()
{
	if( !SelectedSkelMesh )
	{
		return;
	}

	const UPackage* SkelMeshPackage = SelectedSkelMesh->GetOutermost();
	if( SkelMeshPackage->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	// make a list of existing LOD levels to choose from
	TArray<FString> LODStrings;
	LODStrings.AddZeroed( SelectedSkelMesh->LODModels.Num() );
	for(INT i=0; i<SelectedSkelMesh->LODModels.Num(); i++)
	{
		LODStrings(i) = FString::Printf( TEXT("%d"), i );
	}

	// show dialog for choosing which LOD level to import into
	WxDlgGenericComboEntry dlg;
	if( dlg.ShowModal( TEXT("ChooseLODLevel"), TEXT("ExistingLODLevel:"), LODStrings, 0, TRUE ) == wxID_OK )
	{
		INT DesiredLOD = dlg.GetComboBox().GetSelection();

		// Only import if the LOD we want exists
		if( SelectedSkelMesh->LODModels.IsValidIndex(DesiredLOD) )
		{
			FFilename BaseMeshFilename, WeightsMeshFilename;
			FString ExtensionStr;
#if WITH_ACTORX
			ExtensionStr += TEXT("PSK files|*.psk|");
#endif
#if WITH_FBX
			ExtensionStr += TEXT("FBX files|*.fbx|");
#endif
			ExtensionStr += TEXT("All files|*.*");

			WxDualFileDialog MeshWeightsDialog(TEXT("ImportMeshWeightsTitle"), TEXT("ImportMeshWeightsFilenameBase"), TEXT("ImportMeshWeightsFilename"), ExtensionStr, TRUE, wxDEFAULT_DIALOG_STYLE);
			if (MeshWeightsDialog.ShowModal() == wxID_OK)
			{
				TArray<FFilename> MeshWeightsFiles;
				MeshWeightsDialog.GetFilenames(MeshWeightsFiles);
				BaseMeshFilename = MeshWeightsFiles(0);
				WeightsMeshFilename = MeshWeightsFiles(1);
			}

			if( BaseMeshFilename != TEXT("") && 
				WeightsMeshFilename != TEXT("") )
			{

				// Now display combo to choose desired import weight usage.
				TArray<FString> UsageStrings;
				UsageStrings.AddZeroed( IWU_Max );
				for(INT i=0; i<IWU_Max; i++)
				{
					UsageStrings(i) = LocalizeUnrealEd(*FString::Printf(TEXT("ImportMeshWeightUsage%d"), i));
				}

				WxDlgGenericComboEntry UsageDlg;
				if( UsageDlg.ShowModal( TEXT("ImportMeshWeightUsageTitle"), TEXT("ImportMeshWeightUsage"), UsageStrings, 0, TRUE ) == wxID_OK )
				{
					EInstanceWeightUsage DesiredUsage = (EInstanceWeightUsage)UsageDlg.GetComboBox().GetSelection();

#if WITH_FBX
					FString FileExtension = WeightsMeshFilename.GetExtension();
					const UBOOL bWeightMeshIsFBX = appStricmp(*FileExtension, TEXT("FBX")) == 0;
					FileExtension = BaseMeshFilename.GetExtension();
					const UBOOL bBaseMeshIsFBX = appStricmp(*FileExtension, TEXT("FBX")) == 0;

					if (bWeightMeshIsFBX != bBaseMeshIsFBX)
					{
						appMsgf( AMT_OK, *LocalizeUnrealEd("TwoFileMustBeSameFormat") );    
					}
					else if (bWeightMeshIsFBX)
					{
						FSkelMeshOptionalImportData MeshImportData;
						USkeletalMesh* TempSkelMesh = NULL;

						UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();

						// set import options
						UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();
						ImportOptions->bImportMaterials = FALSE;
						ImportOptions->bImportTextures = FALSE;
						ImportOptions->bImportRigidAnim = FALSE;
						ImportOptions->bImportAnimSet = FALSE;

						if ( !FbxImporter->ImportFromFile( *WeightsMeshFilename ) )
						{
							// Log the error message and fail the import.
							debugf(TEXT("Mesh loading"));
							//Warn->Log( NAME_Error, FbxImporter->GetErrorMessage() );
						}
						else
						{
							// Log the import message and import the mesh.
							//Warn->Log( FbxImporter->GetErrorMessage() );

							if (SelectedSkelMesh != NULL)
							{

							}
							// import skeletal mesh alternate weights
							FbxImporter->ImportAlterWeightSkelMesh(MeshImportData.RawMeshInfluencesData);
		
							// when doing full swap, within a group of chunks of the same material index, bone map entries must match if they
							// share the same reference skeleton bone index.  At MAX_GPUSKIN_BONES, its possible to fragment in such a way
							// that you can't accommodate all bone entries, this forces some space to do so.
							GConfig->GetInt( TEXT("AnimSetViewer"), TEXT("AltWeightMaxBoneCountPerChunk"), MeshImportData.MaxBoneCountPerChunk, GEditorIni );
							MeshImportData.IntendedUsage = DesiredUsage;

						}
						FbxImporter->ReleaseScene();

						GWarn->BeginSlowTask( TEXT("Importing Mesh Model Weights"), TRUE);

						if ( !FbxImporter->ImportFromFile( *BaseMeshFilename ) )
						{
							// Log the error message and fail the import.
							debugf(TEXT("Mesh loading"));
							//Warn->Log( NAME_Error, FbxImporter->GetErrorMessage() );
						}
						else
						{
							// Log the import message and import the mesh.
							//Warn->Log( FbxImporter->GetErrorMessage() );
							TArray< TArray<FbxNode*>* > MeshArray;
							FbxImporter->FillFbxSkelMeshArrayInScene(FbxImporter->Scene->GetRootNode(), MeshArray, TRUE);

							TempSkelMesh = (USkeletalMesh*)FbxImporter->ImportSkeletalMesh(UObject::GetTransientPackage(), 
								*MeshArray(0), NAME_None, RF_Transient, *BaseMeshFilename, NULL, &MeshImportData);

							for (INT i=0; i<MeshArray.Num(); i++)
							{
								delete MeshArray(i);
							}
						}
						FbxImporter->ReleaseScene();

						if( TempSkelMesh )
						{
							ImportMeshLOD(TempSkelMesh,DesiredLOD);
						}

						GWarn->EndSlowTask();
					}
					else
#endif //WITH_FBX
					{
#if WITH_ACTORX
						// Load the data from the files into byte arrays
						TArray<BYTE> BaseMeshData;
						TArray<BYTE> WeightsMeshData;

						if( appLoadFileToArray(BaseMeshData, *BaseMeshFilename) &&
							appLoadFileToArray(WeightsMeshData, *WeightsMeshFilename) )
						{
							BaseMeshData.AddItem(0);
							WeightsMeshData.AddItem(0);

							const BYTE* BaseMeshDataPtr = &BaseMeshData(0);
							BYTE* WeightsMeshDataPtr = &WeightsMeshData(0);

							// import raw mesh data and use it as optional data for importing mesh weights
							FSkelMeshOptionalImportData MeshImportData;
							MeshImportData.IntendedUsage = DesiredUsage;
							// when doing full swap, within a group of chunks of the same material index, bone map entries must match if they
							// share the same reference skeleton bone index.  At MAX_GPUSKIN_BONES, its possible to fragment in such a way
							// that you can't accommodate all bone entries, this forces some space to do so.
							GConfig->GetInt( TEXT("AnimSetViewer"), TEXT("AltWeightMaxBoneCountPerChunk"), MeshImportData.MaxBoneCountPerChunk, GEditorIni );  
							MeshImportData.RawMeshInfluencesData.ImportFromFile( WeightsMeshDataPtr, WeightsMeshDataPtr + WeightsMeshData.Num()-1 );

							GWarn->BeginSlowTask( TEXT("Importing Mesh Model Weights"), TRUE);

							// Use the SkeletalMeshFactory to load this SkeletalMesh into a temporary SkeletalMesh.
							// pass in the optional mesh weight import data 
							USkeletalMeshFactory* SkelMeshFact = new USkeletalMeshFactory(&MeshImportData);
							USkeletalMesh* TempSkelMesh = (USkeletalMesh*)SkelMeshFact->FactoryCreateBinary( 
								USkeletalMesh::StaticClass(), 
								UObject::GetTransientPackage(), 
								NAME_None, 
								RF_Transient, 
								NULL, 
								NULL, 
								BaseMeshDataPtr, 
								BaseMeshDataPtr + BaseMeshData.Num()-1,
								GWarn 
								);

							if( TempSkelMesh )
							{
								ImportMeshLOD(TempSkelMesh,DesiredLOD);
							}

							GWarn->EndSlowTask();
						}
#else
						GWarn->Logf( *LocalizeUnrealEd("Error_ActorXDeprecated") );
#endif
					}
				}
			}
		}
	}

	UpdateAltBoneWeightingMenu();
}

/**
 * Updates the visibility and collision of the floor
 */
void WxAnimSetViewer::UpdateFloorComponent(void)
{
	FASVViewportClient* ASVPreviewVC = GetPreviewVC();
	check(ASVPreviewVC);
	check(EditorFloorComp);

	// Turn on/off the ground box
	EditorFloorComp->SetHiddenEditor(!ASVPreviewVC->bShowFloor);
	EditorFloorComp->SetBlockRigidBody(ASVPreviewVC->bShowFloor);
}


#if WITH_FBX
extern void FillFbxSkelMeshArray(FbxNode* Node, TArray<FbxNode*>& outSkelMeshArray);

void WxAnimSetViewer::ImportFbxMorphTarget(UBOOL bImportToLOD, UBOOL bUseImportName, FFilename Filename, EMorphImportError &ImportError)
{
	UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();

	if ( !FbxImporter->ImportFromFile( *Filename ) )
	{
		// Log the error message and fail the import.
		debugf(TEXT("Mesh loading"));
		//Warn->Log( NAME_Error, FbxImporter->GetErrorMessage() );
	}
	else
	{
		// Log the import message and import the mesh.
		//Warn->Log( FbxImporter->GetErrorMessage() );

		// get FBX mesh nodes that match the Unreal skeletal mesh
		TArray<FbxNode*> MorphMeshArray;
		FbxImporter->FindFBXMeshesByBone(SelectedMorphSet->BaseSkelMesh, FALSE, MorphMeshArray);
		
		if ( MorphMeshArray.Num() == 0 ) // no FBX mesh found
		{
			ImportError = MorphImport_InvalidMeshFormat;
		}
		else if ( bImportToLOD && 
				  SelectedMorphSet->Targets.Num() > 0  )
		{
			TArray<FbxNode*> FbxNodes;
			INT MorphCount = 0;
			INT LODLevels = 1;
			INT NodeIndex;
			// get morph count in these geometry
			// get max FBX LOD level
			for (NodeIndex = 0; NodeIndex < MorphMeshArray.Num(); NodeIndex++)
			{
				FbxNode* FbxNode = MorphMeshArray(NodeIndex);
				FbxNodeAttribute* Attr = FbxNode->GetNodeAttribute();
				if (Attr)
				{
					FbxMesh* FbxMesh = NULL;
					if (Attr->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
					{
						FbxMesh = FbxNode->GetChild(0)->GetMesh();
						if (FbxNode->GetChildCount() > LODLevels)
						{
							LODLevels = FbxNode->GetChildCount();
						}
					}
					else
					{
						FbxMesh = FbxNode->GetMesh();
					}

					if (FbxMesh)
					{
						MorphCount += FbxMesh->GetShapeCount();
					}
				}
			}

			if (LODLevels <=1 || MorphCount ==0)
			{
				return;
			}

			// create a list of morph targets to display in the dlg
			TArray<UObject*> MorphList;
			for( INT Idx=0; Idx < SelectedMorphSet->Targets.Num(); Idx++)
			{
				MorphList.AddItem(SelectedMorphSet->Targets(Idx));
			}
			// only allow up to same number of LODs as the base skel mesh
			INT MaxLODIdx = SelectedMorphSet->BaseSkelMesh->LODModels.Num()-1;
			check(MaxLODIdx >= 0);

			// loop each FBX LOD level, import morph target for each LOD
			for (INT LODIndex = 1; LODIndex <= LODLevels && LODIndex <= MaxLODIdx; LODIndex++)
			{
				TArray<FbxNode*> FbxNodes;
				// construct FBX mesh nodes that compose the whole skeletal mesh
				for (INT j = 0; j < MorphMeshArray.Num(); j++)
				{
					FbxNode* Node = MorphMeshArray(j);
					if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
					{
						if (Node->GetChildCount() > LODIndex)
						{
							FbxNodes.AddItem(Node->GetChild(LODIndex));
						}
						else // in less some LODGroups have less level, use the last level
						{
							FbxNodes.AddItem(Node->GetChild(Node->GetChildCount() - 1));
						}
					}
					else
					{
						FbxNodes.AddItem(Node);
					}
				}


				TArray<FbxShape*> FbxShapeArray;
				FbxShapeArray.Add(FbxNodes.Num());
				// Initialize the shape array.
				// In the shape array, only one geometry has shape at a time. Other geometries has no shape
				INT NodeIndex;
				for (NodeIndex = 0; NodeIndex < FbxNodes.Num(); NodeIndex++)
				{
					FbxShapeArray(NodeIndex) = NULL;
				}

				// For each morph in FBX geometries, we create one morph target for the Unreal skeletal mesh
				for (NodeIndex = 0; NodeIndex < FbxNodes.Num(); NodeIndex++)
				{
					// reset the shape array
					if (NodeIndex > 0)
					{
						FbxShapeArray(NodeIndex-1) = NULL;
					}
					FbxMesh* Mesh = FbxNodes(NodeIndex)->GetMesh();
					if (Mesh)
					{
						INT BlendShapeDeformerCount = Mesh->GetDeformerCount(FbxDeformer::eBLENDSHAPE);
						for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
						{
							FbxBlendShape* BlendShape = (FbxBlendShape*)Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);

							FString BlendShapeName = ANSI_TO_TCHAR(FbxImporter->MakeName(BlendShape->GetName()));

							INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
							for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
							{
								FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

								if(Channel)
								{
									FString ChannelName = ANSI_TO_TCHAR(FbxImporter->MakeName(Channel->GetName()));

									// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
									if(ChannelName.StartsWith(BlendShapeName))
									{
										ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
									}

									//Find which shape should we use according to the weight.
									INT ShapeCount = Channel->GetTargetShapeCount();
									for(INT ShapeIndex = 0; ShapeIndex<ShapeCount; ++ShapeIndex)
									{
										FbxShape* Shape = Channel->GetTargetShape(ShapeIndex);

										FbxShapeArray(NodeIndex) = Shape;

										FString ShapeName;
										if( ShapeCount > 1 )
										{
											ShapeName = ANSI_TO_TCHAR(FbxImporter->MakeName(Shape->GetName() ) );
										}
										else
										{
											// Maya concatenates the number of the shape to the end of its name, so instead use the name of the channel
											ShapeName = ChannelName; 
										}

										// Show dialog to select morph target that import the LOD morph target to
										WxDlgMorphLODFbxImport Dlg = WxDlgMorphLODFbxImport( this, TCHAR_TO_ANSI(*ShapeName), MorphList, LODIndex );

										if (Dlg.ShowModal() == wxID_OK )
										{
											INT SelectedMorphIndex = Dlg.GetSelectedMorph();
											UMorphTarget* Result = SelectedMorphSet->Targets(SelectedMorphIndex);

											// now we get a shape for whole mesh, import to unreal as a morph target
											USkeletalMesh* TmpSkeletalMesh = (USkeletalMesh*)FbxImporter->ImportSkeletalMesh( UObject::GetTransientPackage(), FbxNodes, NAME_None, 0, 
												Filename.GetBaseFilename(), &FbxShapeArray );


											// Reacquire the mesh.  If it was triangulated by ImportSkeletalMesh our original pointer will be invalid
											Mesh = FbxNodes(NodeIndex)->GetMesh();
											check( Mesh );

											// Reacquire the blendshape after reacquiring the mesh
											BlendShape = (FbxBlendShape*)Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);

											GWarn->BeginSlowTask( TEXT("Generating Morph Model"), 1);

											// convert the morph target mesh to the raw vertex data
											FMorphMeshRawSource TargetMeshRawData( TmpSkeletalMesh );
											FMorphMeshRawSource BaseMeshRawData(SelectedMorphSet->BaseSkelMesh, LODIndex);

											// populate the vertex data for the morph target mesh using its base mesh
											// and the newly imported mesh
											check(Result);
											Result->CreateMorphMeshStreams( BaseMeshRawData, TargetMeshRawData, LODIndex );

											GWarn->EndSlowTask();
										}
									}
								}
							}
						}
					}
				} // for NodeIndex
			}
		}
		else  // import morph target for LOD 0
		{
			TArray<FbxNode*> FbxNodes;
			INT MorphCount = 0;
			INT NodeIndex;
			// get morph count in this geometry
			// get FBX mesh nodes in LOD 0 by expanding LOD group
			for (NodeIndex = 0; NodeIndex < MorphMeshArray.Num(); NodeIndex++)
			{
				FbxNode* FbxNode = MorphMeshArray(NodeIndex);
				FbxNodeAttribute* Attr = FbxNode->GetNodeAttribute();
				if (Attr)
				{
					FbxMesh* FbxMesh = NULL;
					if (Attr->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
					{
						 FbxMesh = FbxNode->GetChild(0)->GetMesh();
						 FbxNodes.AddItem(FbxNode->GetChild(0));
					}
					else
					{
						FbxMesh = FbxNode->GetMesh();
						FbxNodes.AddItem(FbxNode);
					}

					if (FbxMesh)
					{
						MorphCount += FbxMesh->GetShapeCount();
					}
				}
			}

			if (FbxNodes.Num() > 0 && MorphCount > 0)
			{
				FName ShapeName;
				TArray<FbxShape*> FbxShapeArray;
				FbxShapeArray.Add(FbxNodes.Num());
				// Initialize the shape array.
				// In the shape array, only one geometry has shape at a time. Other geometries has no shape
				INT NodeIndex;
				for (NodeIndex = 0; NodeIndex < FbxNodes.Num(); NodeIndex++)
				{
					FbxShapeArray(NodeIndex) = NULL;
				}

				// For each morph in FBX geometries, we create one morph target for the Unreal skeletal mesh
				for (NodeIndex = 0; NodeIndex < FbxNodes.Num(); NodeIndex++)
				{
					// reset the shape array
					if (NodeIndex > 0)
					{
						FbxShapeArray(NodeIndex-1) = NULL;
					}
					FbxMesh* Mesh = FbxNodes(NodeIndex)->GetMesh();
					if (Mesh)
					{
						INT BlendShapeDeformerCount = Mesh->GetDeformerCount(FbxDeformer::eBLENDSHAPE);
						for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
						{
							FbxBlendShape* BlendShape = (FbxBlendShape*)Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);

							FString BlendShapeName = ANSI_TO_TCHAR(FbxImporter->MakeName(BlendShape->GetName()));

							INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
							for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
							{
								FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

								if(Channel)
								{
									//Find which shape should we use according to the weight.
									INT ShapeCount = Channel->GetTargetShapeCount();

									FString ChannelName = ANSI_TO_TCHAR(FbxImporter->MakeName(Channel->GetName()));

									// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
									if(ChannelName.StartsWith(BlendShapeName))
									{
										ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
									}

									for(INT ShapeIndex = 0; ShapeIndex<ShapeCount; ++ShapeIndex)
									{
										FbxShape* Shape = Channel->GetTargetShape(ShapeIndex);
										FbxShapeArray(NodeIndex) = Shape;

										// Maya concatenates the number of the shape to the end of its name, so instead use the name of the channel
										FString ShapeName;

										if (MorphCount == 1 && bUseImportName)
										{
											ShapeName = *Filename.GetBaseFilename();
										}
										else if( ShapeCount > 1 )
										{
											ShapeName = ANSI_TO_TCHAR(FbxImporter->MakeName(Shape->GetName() ) );
										}
										else
										{
											// Maya concatenates the number of the shape to the end of its name, so instead use the name of the channel
											ShapeName = ChannelName;
										}

										UMorphTarget* ExistingTarget = SelectedMorphSet->FindMorphTarget(FName(*ShapeName));

										// handle an existing target by asking the user to replace the existing one
										if( ExistingTarget &&
											appMsgf( AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_29") )!=0 )
										{
											continue;
										}

										// now we get a shape for whole mesh, import to unreal as a morph target
										USkeletalMesh* TmpSkeletalMesh = (USkeletalMesh*)FbxImporter->ImportSkeletalMesh( UObject::GetTransientPackage(), FbxNodes, NAME_None, 0, 
											Filename.GetBaseFilename(), &FbxShapeArray );

										// Reacquire the mesh.  If it was triangulated by ImportSkeletalMesh our original pointer will be invalid
										Mesh = FbxNodes(NodeIndex)->GetMesh();
										check( Mesh );

										// Reacquire the blendshape after reaquiring the mesh
										BlendShape = (FbxBlendShape*)Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);

										GWarn->BeginSlowTask( TEXT("Generating Morph Model"), 1);

										// convert the morph target mesh to the raw vertex data
										FMorphMeshRawSource TargetMeshRawData( TmpSkeletalMesh );
										FMorphMeshRawSource BaseMeshRawData(SelectedMorphSet->BaseSkelMesh, 0);
										UMorphTarget* Result =  NULL;
										// create the new morph target mesh
										if( ExistingTarget)
										{
											Result = ExistingTarget;
										}
										else
										{
											Result = ConstructObject<UMorphTarget>( UMorphTarget::StaticClass(), SelectedMorphSet, FName(*ShapeName) );
											// add it to the set
											SelectedMorphSet->Targets.AddItem( Result );
										}

										// populate the vertex data for the morph target mesh using its base mesh
										// and the newly imported mesh
										check(Result);
										Result->CreateMorphMeshStreams( BaseMeshRawData, TargetMeshRawData, 0 );

										GWarn->EndSlowTask();
									}
								}
							}
						}
					}
				}
			} // for NodeIndex
			else
			{
				// log
				GWarn->Log( NAME_Warning, "no FBX morph target found in LOD 0" );
			}
		}

		DisplayError(ImportError, Filename);
	}
	FbxImporter->ReleaseScene(); 
}
#endif //WITH_FBX

/**
* Displays and handles dialogs necessary for importing 
* a new morph target mesh from file. The new morph
* target is placed in the currently selected MorphTargetSet
*
* @param bImportToLOD - if TRUE then new files will be treated as morph target LODs 
* instead of new morph target resources
*/
void WxAnimSetViewer::ImportMorphTarget(UBOOL bImportToLOD, UBOOL UseImportName)
{
	if( !SelectedMorphSet )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_SelectMorphSetForImport") );
		return;
	}

	// The MorphTargetSet should be bound to a skel mesh on creation
	if( !SelectedMorphSet->BaseSkelMesh )
	{	
		return;
	}

	// Disallow on cooked content.
	const UPackage* MorphSetPackage = SelectedMorphSet->GetOutermost();
	if( MorphSetPackage->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return;
	}

	if (UseImportName && appMsgf( AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_35") )!=0) 
	{
		return;
	}		


	FString ExtensionStr;
#if WITH_ACTORX
	ExtensionStr += TEXT("PSK files|*.psk|");
#endif
#if WITH_FBX
	ExtensionStr += TEXT("FBX files|*.fbx|");
#endif
	ExtensionStr += TEXT("All files|*.*");

	// First, display the file open dialog for selecting the files.
	WxFileDialog ImportFileDialog( this, 
		*LocalizeUnrealEd("ImportMorphTarget"), 
		*(GApp->LastDir[LD_PSA]),
		TEXT(""),
		*ExtensionStr,
		wxOPEN | wxFILE_MUST_EXIST | wxMULTIPLE,
		wxDefaultPosition);
	
	// show dialog and handle OK
	if( ImportFileDialog.ShowModal() == wxID_OK )
	{
		// get list of files from dialog
		wxArrayString ImportFilePaths;
		ImportFileDialog.GetPaths(ImportFilePaths);

		// iterate over all selected files
		for( UINT FileIdx=0; FileIdx < ImportFilePaths.Count(); FileIdx++ )
		{
			// path and filename to import from
			FFilename Filename = (const TCHAR*)ImportFilePaths[FileIdx];
			// Check the file extension for PSK/DAE switch. Anything that isn't .DAE is considered a PSK file.
			const FString FileExtension = Filename.GetExtension();
			// keep track of error reported
			EMorphImportError ImportError=MorphImport_OK;

#if WITH_FBX
			const UBOOL bIsFBX = appStricmp(*FileExtension, TEXT("FBX")) == 0;
			if ( bIsFBX )
			{
				ImportFbxMorphTarget( bImportToLOD,  UseImportName, Filename, ImportError);
			}
			else
#endif
			{
				// handle importing to LOD level of an existing morph target
				if( bImportToLOD &&
					SelectedMorphSet->Targets.Num() > 0 )
				{
					// create a list of morph targets to display in the dlg
					TArray<UObject*> MorphList;
					for( INT Idx=0; Idx < SelectedMorphSet->Targets.Num(); Idx++)
					{
						MorphList.AddItem(SelectedMorphSet->Targets(Idx));
					}
					// only allow up to same number of LODs as the base skel mesh
					INT MaxLODIdx = SelectedMorphSet->BaseSkelMesh->LODModels.Num()-1;
					check(MaxLODIdx >= 0);
					// show dialog for importing an LOD level
					WxDlgMorphLODImport Dlg(this,MorphList,MaxLODIdx,*Filename.GetBaseFilename());
					if( Dlg.ShowModal() == wxID_OK )
					{
						// get user input from dlg
						INT SelectedMorphIdx = Dlg.GetSelectedMorph();
						INT SelectedLODIdx = Dlg.GetLODLevel();

						// import mesh to LOD
#if WITH_ACTORX
						// create the morph target importer
						FMorphTargetBinaryPSKImport MorphImport( SelectedMorphSet->BaseSkelMesh, SelectedLODIdx, GWarn );
						// import the mesh LOD
						if( SelectedMorphSet->Targets.IsValidIndex(SelectedMorphIdx) )
						{
							MorphImport.ImportMorphLODModel(
								SelectedMorphSet->Targets(SelectedMorphIdx),
								*Filename,
								SelectedLODIdx,
								&ImportError
								);			
						}
						else
						{
							ImportError = MorphImport_MissingMorphTarget;
						}
#else
						appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ActorXDeprecated") );
#endif
					}
				}
				// handle importing a new morph target object
				else
				{
					// show dialog to specify the new morph target's name
					WxDlgGenericStringEntry NameDlg;
					if( UseImportName || NameDlg.ShowModal(TEXT("NewMorphTargetName"), TEXT("NewMorphTargetName"), *Filename.GetBaseFilename()) == wxID_OK )
					{
						// create the morph target importer
						FMorphTargetBinaryPSKImport MorphImport( SelectedMorphSet->BaseSkelMesh, 0, GWarn );

						// get the name entered
						FName NewTargetName;
						if ( UseImportName )
						{
							NewTargetName = *Filename.GetBaseFilename();
						}
						else
						{
							NewTargetName = *NameDlg.GetEnteredString();
						}

						// import the mesh
#if WITH_ACTORX
						MorphImport.ImportMorphMeshToSet( 
							SelectedMorphSet, 
							NewTargetName, 
							*Filename,
							FALSE,
							&ImportError );

						// handle an existing target by asking the user to replace the existing one
						if( ImportError == MorphImport_AlreadyExists &&
							(UseImportName || appMsgf( AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_29") )==0) )
						{

							MorphImport.ImportMorphMeshToSet( 
								SelectedMorphSet, 
								NewTargetName, 
								*Filename,
								TRUE,
								&ImportError );
						}	
#else
						GWarn->Logf( *LocalizeUnrealEd("Error_ActorXDeprecated") );
#endif
					}

				}

				// handle errors
				DisplayError(ImportError, Filename);			
			}
		}
	}

	// Refresh combo to show new number of targets in this MorphTargetSet.
	INT CurrentSelection = MorphSetCombo->GetSelection();
	UpdateMorphSetCombo();
	MorphSetCombo->SetSelection(CurrentSelection);

	// Refresh morph target list box to show any new anims.
	UpdateMorphTargetList();

	// Mark as dirty.
	SelectedMorphSet->MarkPackageDirty();
}
/*
 * Reset all morph target weights to be 0 
 */
void WxAnimSetViewer::ResetMorphTargets()
{
	check ( SelectedMorphSet );

	for (INT I=0; I<SelectedMorphSet->Targets.Num(); ++I)
	{
		UpdateMorphTargetWeight(I, 0.f);
	}
}

/*
 * Update Index's morph target to input weight 
 */
void WxAnimSetViewer::UpdateMorphTargetWeight(const INT Index, const FLOAT Weight)
{
	check ( SelectedMorphSet );
	check ( SelectedMorphSet->Targets.Num() > Index );

	const UMorphTarget* Result = SelectedMorphSet->Targets(Index);
	if (Result)
	{
		// If there's weight, add the morph target, otherwise make sure it's removed as a weight of 0 will prevent the animation from using it itself.
		if ( Weight > 0.0f )
		{
			PreviewMorphPose->AddMorphTarget(Result->GetFName(), Weight);
		}
		else
		{
			PreviewMorphPose->RemoveMorphTarget(Result->GetFName());
		}
		PreviewSkelComp->UpdateMorphTargetMaterial(Result, Weight);
		PreviewSkelCompRaw->UpdateMorphTargetMaterial(Result, Weight);
	}
}

void WxAnimSetViewer::DisplayError(EMorphImportError &ImportError, FFilename &Filename)
{
	switch( ImportError )
	{
	    case MorphImport_AlreadyExists:
		    //appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MorphImport_AlreadyExists") );
		    break;
	    case MorphImport_CantLoadFile:
		    appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_MorphImport_CantLoadFile"), *Filename) );
		    break;
	    case MorphImport_InvalidMeshFormat:
		    appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MorphImport_InvalidMeshFormat") );
		    break;
	    case MorphImport_MismatchBaseMesh:
		    appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MorphImport_MismatchBaseMesh") );
		    break;
	    case MorphImport_ReimportBaseMesh:
		    appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MorphImport_ReimportBaseMesh") );
		    break;
	    case MorphImport_MissingMorphTarget:
		    appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MorphImport_MissingMorphTarget") );
		    break;
	    case MorphImport_InvalidLODIndex:
		    appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MorphImport_InvalidLODIndex") );
		    break;
	}
}

/** rename the currently selected morph target */
void WxAnimSetViewer::RenameMorphTarget(const INT Index, const TCHAR * NewName)
{
	check ( SelectedMorphSet );
	check ( SelectedMorphSet->Targets.Num() > Index );

	FString New(NewName);
	New.Trim(); // clear white spaces

	// if same name found, ignore - It's okay to ignore with new system
	// It will ignore until it finds unique name (while typing)
	// when it's unique name, it will be applied
	// If not, it won't be applied
	if( New!=TEXT("") && SelectedMorphSet->FindMorphTarget( FName(NewName) ) == NULL )
	{
		// rename the object
		SelectedMorphSet->Targets(Index)->Rename( NewName );
		// mark as dirty so a resave is required
		SelectedMorphSet->MarkPackageDirty();
	}
}

/** delete the selected morph targets */
void WxAnimSetViewer::DeleteSelectedMorphTarget()
{
	FString NameList;
	for ( INT I=0; I<SelectedMorphTargets.Num(); ++I )
	{
		NameList += SelectedMorphTargets(I)->GetName();
		NameList += TEXT("\n");
	}

	// show confirmation message dialog
	UBOOL bDoDelete = appMsgf( AMT_YesNo, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_27"), *NameList) ) );
	if( bDoDelete )
	{
		for ( INT I=0; I<SelectedMorphTargets.Num(); ++I )
		{
			SelectedMorphSet->Targets.RemoveItem(SelectedMorphTargets(I));
		}

		// Refresh list
		UpdateMorphTargetList();

		// mark as dirty
		SelectedMorphSet->MarkPackageDirty();
	}
}

/** delete the selected morph targets */
void WxAnimSetViewer::RemapVerticesSelectedMorphTarget()
{
	if ( appMsgf( AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_37") ) == 0 )
	{
		// add prompt
		SelectedMorphSet->UpdateMorphTargetsFromBaseMesh();
	}
}

void WxAnimSetViewer::CreateNewAnimSet()
{
	FString Package = TEXT("");
	FString Group = TEXT("");

	if(SelectedSkelMesh)
	{
		// Bit yucky this...
		check( SelectedSkelMesh->GetOuter() );

		// If there are 2 levels above this mesh - top is packages, then group
		if( SelectedSkelMesh->GetOuter()->GetOuter() )
		{
			Group = SelectedSkelMesh->GetOuter()->GetFullGroupName(0);
			Package = SelectedSkelMesh->GetOuter()->GetOuter()->GetName();
		}
		else // Otherwise, just a package.
		{
			Group = TEXT("");
			Package = SelectedSkelMesh->GetOuter()->GetName();
		}
	}

	WxDlgRename RenameDialog;
	RenameDialog.SetTitle( *LocalizeUnrealEd("NewAnimSet") );
	if( RenameDialog.ShowModal( Package, Group, TEXT("NewAnimSet") ) == wxID_OK )
	{
		if( RenameDialog.GetNewName().Len() == 0 || RenameDialog.GetNewPackage().Len() == 0 )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_MustSpecifyNewAnimSetName") );
			return;
		}

		UPackage* Pkg = GEngine->CreatePackage(NULL,*RenameDialog.GetNewPackage());
		if( RenameDialog.GetNewGroup().Len() )
		{
			Pkg = GEngine->CreatePackage(Pkg,*RenameDialog.GetNewGroup());
		}

		UAnimSet* NewAnimSet = ConstructObject<UAnimSet>( UAnimSet::StaticClass(), Pkg, FName( *RenameDialog.GetNewName() ), RF_Public|RF_Standalone );

		// Will update AnimSet list, which will include new AnimSet.
		SetSelectedSkelMesh(SelectedSkelMesh, TRUE);
		SetSelectedAnimSet(NewAnimSet, FALSE);

		SelectedAnimSet->MarkPackageDirty();
	}
}

IMPLEMENT_COMPARE_POINTER( UAnimSequence, AnimSetViewer, { return appStricmp(*A->SequenceName.ToString(),*B->SequenceName.ToString()); } );

/**
 * Populates all the anim sequences in the given anim set to the anim sequence list. 
 *
 * @param	SetToAdd	The anim set containing sequences to add. 
 */
void WxAnimSetViewer::AddAnimSequencesToList( UAnimSet* SetToAdd )
{
	// Add each anim. sequence of the currently selected animset to the listbox if they pass the search filter (or if the filter is empty)
	const FString FilterString = AnimSeqFilter->GetValue().c_str();

	for ( TArray<UAnimSequence*>::TConstIterator AnimSeqIter(SetToAdd->Sequences); AnimSeqIter; ++AnimSeqIter )
	{
		UAnimSequence* CurSeq = *AnimSeqIter;
		const FString SeqString = FString::Printf( TEXT("%s [%d]"), *CurSeq->SequenceName.ToString(), CurSeq->NumFrames );
		if ( FilterString.Len() == 0 || SeqString.InStr( FilterString, FALSE, TRUE ) != INDEX_NONE )
		{
			AnimSeqList->Append(*SeqString, CurSeq);
		}
	}
}

void WxAnimSetViewer::UpdateAnimSeqList()
{
	AnimSeqList->Freeze();
	AnimSeqList->Clear();

	// If this mode is on, it is assumed that all anim sets were 
	// loaded. So, we can just iterate over the anim sets in memory. 
	if(bSearchAllAnimSequences)
	{
		for(TObjectIterator<UAnimSet> It; It; ++It)
		{
			AddAnimSequencesToList(*It);
		}
	}
	else if(SelectedAnimSet)
	{
		AddAnimSequencesToList(SelectedAnimSet);
	}

	AnimSeqList->Thaw();
}

// Note - this will clear the current selection in the combo- must manually select an AnimSet afterwards.
void WxAnimSetViewer::UpdateAnimSetCombo()
{
	AnimSetCombo->Freeze();
	AnimSetCombo->Clear();
	
	FString DefaultAnimSetName(TEXT("_None"));
	UAnimSet *DefaultAnimSet = NULL;
	AnimSetCombo->Append( *DefaultAnimSetName, DefaultAnimSet );

	for(TObjectIterator<UAnimSet> It; It; ++It)
	{
		UAnimSet* ItAnimSet = *It;

		// Add the anim set regardless if it can play on the current skeleton when the user 
		// wants to see all anim sequences. Though, the skeletal mesh will change if the 
		// user selects an anim seq that doesn't work with the current mesh.
		const UBOOL bAddCurrentSet = ( bSearchAllAnimSequences || ItAnimSet->CanPlayOnSkeletalMesh(SelectedSkelMesh) );

		if( bAddCurrentSet )
		{
			FString AnimSetString = FString::Printf( TEXT("%s [%d]"), *ItAnimSet->GetName(), ItAnimSet->Sequences.Num() );
			AnimSetCombo->Append( *AnimSetString, ItAnimSet );
		}
	}
	AnimSetCombo->Thaw();
}

IMPLEMENT_COMPARE_POINTER( UMorphTarget, AnimSetViewer, { return appStricmp(*A->GetName(), *B->GetName()); } );


/** Update the list of MorphTargets in the set. */
void WxAnimSetViewer::UpdateMorphTargetList()
{
	if(!SelectedMorphSet)
	{
		return;
	}

	// make sure morph target is initialized 
	// - this is to fix issue with viewing right after importing
	if ( PreviewMorphPose->SkelComponent )
	{
		// call init to clear the morph target name index
		PreviewMorphPose->SkelComponent->InitMorphTargets();
	}

	// clear all morph weighting before updating list
	PreviewMorphPose->ClearAll();

	// Sort the list
	Sort<USE_COMPARE_POINTER(UMorphTarget,AnimSetViewer)>(&SelectedMorphSet->Targets(0),SelectedMorphSet->Targets.Num());

	// Morph Target Panel responsible for the list
	MorphTargetPanel->LayoutWindows(SelectedMorphSet->Targets);

	// update selection - clear all selection
	UpdateSelectedMorphTargets();
}

/**
 *	Update the list of MorphTargetSets in the combo box.
 *	Note - this will clear the current selection in the combo- must manually select an AnimSet afterwards.
 */
void WxAnimSetViewer::UpdateMorphSetCombo()
{
	MorphSetCombo->Freeze();
	MorphSetCombo->Clear();

	for(TObjectIterator<UMorphTargetSet> It; It; ++It)
	{
		UMorphTargetSet* ItMorphSet = *It;

		FString MorphSetString = FString::Printf( TEXT("%s [%d]"), *ItMorphSet->GetName(), ItMorphSet->Targets.Num() );
		MorphSetCombo->Append( *MorphSetString, ItMorphSet );
	}
	MorphSetCombo->Thaw();
}


// Update the UI to match the current animation state (position, playing, looping)
void WxAnimSetViewer::RefreshPlaybackUI()
{
	// Update scrub bar (can only do if we have an animation selected.
	if(SelectedAnimSeq)
	{
// 		check(PreviewAnimNode->AnimSeq == SelectedAnimSeq);

		if( PreviewAnimNode->AnimSeq == SelectedAnimSeq )
		{
			FLOAT CurrentPos = PreviewAnimNode->CurrentTime / SelectedAnimSeq->SequenceLength;
			TimeSlider->SetValue( appRound(CurrentPos * (FLOAT)ASV_SCRUBRANGE) );
		}
	}

	// Update Play/Stop button
	if(PreviewAnimNode->bPlaying)
	{
		// Only set the bitmap label if we actually are changing state.
		const wxBitmap& CurrentBitmap = PlayButton->GetBitmapLabel();

		if(CurrentBitmap.GetHandle() != StopB.GetHandle())
		{
			PlayButton->SetBitmapLabel( StopB );
			PlayButton->Refresh();
		}
	}
	else
	{
		// Only set the bitmap label if we actually are changing state.
		const wxBitmap& CurrentBitmap = PlayButton->GetBitmapLabel();

		if(CurrentBitmap.GetHandle() != PlayB.GetHandle())
		{
			PlayButton->SetBitmapLabel( PlayB );
			PlayButton->Refresh();
		}
	}


	// Update Loop toggle
	if(PreviewAnimNode->bLooping)
	{
		// Only set the bitmap label if we actually are changing state.
		const wxBitmap& CurrentBitmap = LoopButton->GetBitmapLabel();

		if(CurrentBitmap.GetHandle() != LoopB.GetHandle())
		{
			LoopButton->SetBitmapLabel( LoopB );
			LoopButton->Refresh();
		}
	}
	else
	{
		// Only set the bitmap label if we actually are changing state.
		const wxBitmap& CurrentBitmap = LoopButton->GetBitmapLabel();

		if(CurrentBitmap.GetHandle() != NoLoopB.GetHandle())
		{
			LoopButton->SetBitmapLabel( NoLoopB );
			LoopButton->Refresh();
		}
	}
}

extern FParticleDataManager	GParticleDataManager;

void WxAnimSetViewer::TickViewer(FLOAT DeltaSeconds)
{
	if ((bResampleAnimNotifyData == TRUE) && (SelectedAnimSeq != NULL))
	{
		for (INT NotifyIdx = 0; NotifyIdx < SelectedAnimSeq->Notifies.Num(); NotifyIdx++)
		{
			FAnimNotifyEvent* OwnerEvent = &(SelectedAnimSeq->Notifies(NotifyIdx));
			if (OwnerEvent && OwnerEvent->Notify)
			{
				OwnerEvent->Notify->AnimNotifyEventChanged(PreviewAnimNode, OwnerEvent);
			}
		}
		bResampleAnimNotifyData = FALSE;
	}

	// Tick the PreviewSkelComp to move animation forwards, then Update to update bone locations.
	PreviewSkelComp->TickAnimNodes(DeltaSeconds);
	
	if (PreviewSkelComp->UpdateLODStatus())
	{
		// whenever LOD has changed, update Alternate Bone weighting menu
		UpdateAltBoneWeightingMenu();

		// Update the weights
		FillSkeletonTree();
	}

	// update the instanced influence weights if needed
	for (INT LODIdx=0; LODIdx<PreviewSkelComp->LODInfo.Num(); LODIdx++)
	{
		if( PreviewSkelComp->LODInfo(LODIdx).bNeedsInstanceWeightUpdate )
		{
			PreviewSkelComp->UpdateInstanceVertexWeights(LODIdx);
		}
	}

	PreviewSkelComp->UpdateSkelPose();

	// it's not dup code - it's Raw skeletal
	PreviewSkelCompRaw->TickAnimNodes(DeltaSeconds);
	if (PreviewSkelCompRaw->UpdateLODStatus())
	{
		// whenever LOD has changed, update Alternate Bone weighting menu
		UpdateAltBoneWeightingMenu();
	}
	
	// update the instanced influence weights if needed
	for (INT LODIdx=0; LODIdx<PreviewSkelCompRaw->LODInfo.Num(); LODIdx++)
	{
		if( PreviewSkelCompRaw->LODInfo(LODIdx).bNeedsInstanceWeightUpdate )
		{
			PreviewSkelCompRaw->UpdateInstanceVertexWeights(LODIdx);
		}
	}

	PreviewSkelCompRaw->UpdateSkelPose();

	// Apply wind forces.
	PreviewSkelComp->UpdateClothWindForces(DeltaSeconds);
#if WITH_APEX
	PreviewSkelComp->TickApexClothing(DeltaSeconds);
#endif
	// Update any cloth attachment
	PreviewSkelComp->UpdateFixedClothVerts();

	// Run physics
	TickRBPhysScene(RBPhysScene, DeltaSeconds);
	WaitRBPhysScene(RBPhysScene);

	DeferredRBResourceCleanup(RBPhysScene);

	FASVViewportClient* vc = GetPreviewVC();
	if( NULL != vc->LineBatcher )
	{
		vc->LineBatcher->BatchedLines.Empty(); // remove the novodex debug line draws
#if WITH_NOVODEX
		RBPhysScene->AddNovodexDebugLines( vc->LineBatcher );
		vc->LineBatcher->BeginDeferredReattach();
#endif
	}

	// Update components
	PreviewSkelComp->ConditionalUpdateTransform(FMatrix::Identity);
	PreviewSkelCompRaw->ConditionalUpdateTransform(FMatrix::Identity);
	PreviewSkelCompAux1->ConditionalUpdateTransform(FMatrix::Identity);
	PreviewSkelCompAux2->ConditionalUpdateTransform(FMatrix::Identity);
	PreviewSkelCompAux3->ConditionalUpdateTransform(FMatrix::Identity);

	// Update any attached particle systems for previewing...
	UBOOL bHadPSysComponents = FALSE;
	for (INT AttachmentIndex = 0; AttachmentIndex < PreviewSkelComp->Attachments.Num(); AttachmentIndex++)
	{
		FAttachment& CheckAttachment = PreviewSkelComp->Attachments(AttachmentIndex);
		UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(CheckAttachment.Component);
		if (PSysComp)
		{
			PSysComp->Tick(DeltaSeconds);
			bHadPSysComponents = TRUE;
		}
	}
	if (bHadPSysComponents == TRUE)
	{
		GParticleDataManager.UpdateDynamicData();
	}

	// Move scrubber to reflect current animation position etc.
	RefreshPlaybackUI();
}

void WxAnimSetViewer::UpdateSkelComponents()
{
	FComponentReattachContext ReattachContext1(PreviewSkelComp);
	FComponentReattachContext ReattachContext1Raw(PreviewSkelCompRaw);
	FComponentReattachContext ReattachContext2(PreviewSkelCompAux1);
	FComponentReattachContext ReattachContext3(PreviewSkelCompAux2);
	FComponentReattachContext ReattachContext4(PreviewSkelCompAux3);
}

void WxAnimSetViewer::UpdateForceLODButtons()
{
	if(SelectedSkelMesh && PreviewSkelComp )
	{
		ToolBar->EnableTool( IDM_ANIMSET_LOD_AUTO, !PreviewSkelComp->bDrawBoneInfluences );
		ToolBar->EnableTool( IDM_ANIMSET_LOD_BASE, !PreviewSkelComp->bDrawBoneInfluences );
		ToolBar->EnableTool( IDM_ANIMSET_LOD_1, !PreviewSkelComp->bDrawBoneInfluences && SelectedSkelMesh->LODModels.Num() > 1 );
		ToolBar->EnableTool( IDM_ANIMSET_LOD_2, !PreviewSkelComp->bDrawBoneInfluences && SelectedSkelMesh->LODModels.Num() > 2 );
		ToolBar->EnableTool( IDM_ANIMSET_LOD_3, !PreviewSkelComp->bDrawBoneInfluences && SelectedSkelMesh->LODModels.Num() > 3 );

		MenuBar->Enable( IDM_ANIMSET_LOD_AUTO, !PreviewSkelComp->bDrawBoneInfluences );
		MenuBar->Enable( IDM_ANIMSET_LOD_BASE, !PreviewSkelComp->bDrawBoneInfluences );
		MenuBar->Enable( IDM_ANIMSET_LOD_1, !PreviewSkelComp->bDrawBoneInfluences && SelectedSkelMesh->LODModels.Num() > 1 );
		MenuBar->Enable( IDM_ANIMSET_LOD_2, !PreviewSkelComp->bDrawBoneInfluences && SelectedSkelMesh->LODModels.Num() > 2 );
		MenuBar->Enable( IDM_ANIMSET_LOD_3, !PreviewSkelComp->bDrawBoneInfluences && SelectedSkelMesh->LODModels.Num() > 3 );
	}
}

void WxAnimSetViewer::EmptySelectedSet()
{
	if(SelectedAnimSet)
	{
		UBOOL bDoEmpty = appMsgf( AMT_YesNo, *LocalizeUnrealEd("Prompt_1") );
		if( bDoEmpty )
		{
			SelectedAnimSet->ResetAnimSet();
			UpdateAnimSeqList();
			SetSelectedAnimSequence(NULL);

			SelectedAnimSet->MarkPackageDirty();
		}
	}
}

// Delete Morph Tracks from All Sequences
void WxAnimSetViewer::DeleteMorphTrackFromSelectedSet()
{
	if( SelectedAnimSet )
	{
		TArray<FName> MorphTargetsUsedByAnimSet;

		// Get all the list of morph targt names 
		for ( INT SeqIndex = 0 ; SeqIndex < SelectedAnimSet->Sequences.Num() ; ++SeqIndex )
		{
			for ( INT CurveIdx=0; CurveIdx < SelectedAnimSet->Sequences(SeqIndex)->CurveData.Num(); ++CurveIdx )
			{
				MorphTargetsUsedByAnimSet.AddUniqueItem(SelectedAnimSet->Sequences(SeqIndex)->CurveData(CurveIdx).CurveName);
			}
		}

		// Add to dialog
		WxGenericDlgCheckBoxList<FName> dlg;
		for ( INT I=0; I<MorphTargetsUsedByAnimSet.Num(); ++I )
		{
			dlg.AddCheck(MorphTargetsUsedByAnimSet(I), MorphTargetsUsedByAnimSet(I).ToString(), FALSE);
		}

		// Show and confirm
		FIntPoint WindowSize(300, 500);
		const INT Result = dlg.ShowDialog( *LocalizeUnrealEd("AnimSetViewer_DeleteMorphTracksTitle"), *LocalizeUnrealEd("AnimSetViewer_DeleteMorphTracksContent"), WindowSize, TEXT("DeleteMorphTracksFromAnimSet"));

		if ( Result == wxID_OK )
		{
			TArray<FName> KeysToDelete;
			dlg.GetResults( KeysToDelete, TRUE );
			if ( KeysToDelete.Num() > 0 )
			{
				// Go through all sequence
				for ( INT SeqIndex = 0 ; SeqIndex < SelectedAnimSet->Sequences.Num() ; ++SeqIndex )
				{
					for ( INT CurveIdx=0; CurveIdx < SelectedAnimSet->Sequences(SeqIndex)->CurveData.Num(); ++CurveIdx )
					{
						// if this key is in KeysToDelete
						if (KeysToDelete.ContainsItem(SelectedAnimSet->Sequences(SeqIndex)->CurveData(CurveIdx).CurveName))
						{
							// delete the key
							SelectedAnimSet->Sequences(SeqIndex)->CurveData.Remove(CurveIdx);
							--CurveIdx;
						}
					}
				}

				// Mark this is dirty
				SelectedAnimSet->MarkPackageDirty();
			}

		}
	}
}

void WxAnimSetViewer::DeleteTrackFromSelectedSet()
{
	if( SelectedAnimSet )
	{
		// check dialogue
		WxGenericDlgCheckBoxList<FName> dlg;
		// Make a list of all tracks in the animset.
		for ( INT TrackIndex = 0 ; TrackIndex < SelectedAnimSet->TrackBoneNames.Num() ; ++TrackIndex )
		{
			const FName& TrackName = SelectedAnimSet->TrackBoneNames(TrackIndex);
			dlg.AddCheck(TrackName, TrackName.ToString(), FALSE);
		}

		// Present the user with a list of potential tracks to delete.
		FIntPoint WindowSize(300, 500);
		const INT Result = dlg.ShowDialog( TEXT("Delete Tracks"), TEXT("Delete Tracks From AnimSet"), WindowSize, TEXT("DeleteTracksFromAnimSet"));

		if ( Result == wxID_OK )
		{
			TArray<FName> TracksToDelete;
			dlg.GetResults( TracksToDelete, TRUE );
			UBOOL bDeletedSomething = FALSE;

			for ( INT I=0; I<TracksToDelete.Num(); ++I )
			{
				const FName SelectedTrackName( TracksToDelete(I) );

				const INT SelectedTrackIndex = SelectedAnimSet->TrackBoneNames.FindItemIndex( SelectedTrackName );
				if( SelectedTrackIndex != INDEX_NONE )
				{
					// Eliminate the selected track from all sequences in the set.
					for( INT SequenceIndex = 0 ; SequenceIndex < SelectedAnimSet->Sequences.Num() ; ++SequenceIndex )
					{
						UAnimSequence* Seq = SelectedAnimSet->Sequences( SequenceIndex );
						check( Seq->RawAnimationData.Num() == SelectedAnimSet->TrackBoneNames.Num() );
						Seq->RawAnimationData.Remove( SelectedTrackIndex );

						// Keep AdditiveBasePose in sync.
						if( Seq->bIsAdditive )
						{
							check( Seq->AdditiveBasePose.Num() == SelectedAnimSet->TrackBoneNames.Num() );
							Seq->AdditiveBasePose.Remove( SelectedTrackIndex );
						}
					}

					// Delete corresponding track bone name as well.
					SelectedAnimSet->TrackBoneNames.Remove( SelectedTrackIndex );
					bDeletedSomething = TRUE;
				}
			}

			// If we've actually done something, let others know of the change.
			if( bDeletedSomething )
			{
				// Flush the linkup cache.
				SelectedAnimSet->LinkupCache.Empty();
				SelectedAnimSet->SkelMesh2LinkupCache.Empty();

				// Flag the animset for resaving.
				SelectedAnimSet->MarkPackageDirty();

				// Reinit anim trees so that eg new linkup caches are created.
				for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
				{
					USkeletalMeshComponent* SkelComp = *It;
					if(!SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
					{
						SkelComp->InitAnimTree();
					}
				}

				// Recompress the anim set if compression was applied.
				RecompressSet( SelectedAnimSet, SelectedSkelMesh );
			}
		}
	}
}

/** Copy TranslationBoneNames to new AnimSet. */
void WxAnimSetViewer::CopyTranslationBoneNamesToAnimSet()
{
	TArray<UAnimSet*> SelectedAnimSets;
	if ( GEditor->GetSelectedObjects()->GetSelectedObjects<UAnimSet>( SelectedAnimSets ) > 0)
	{
		GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
	
		for (INT Index = 0; Index < SelectedAnimSets.Num(); Index++)
		{	
			// Get destination AnimSet, and check we have one!
			UAnimSet* DestAnimSet = SelectedAnimSets(Index);
			if( !DestAnimSet )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("NoDestinationAnimSetSelected") );
				continue;
			}

			// Check we're not moving to same AnimSet... duh!
			if( DestAnimSet == SelectedAnimSet )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("AnimSetSelectedIsSourceAnimSet") );
				continue;
			}

			DestAnimSet->UseTranslationBoneNames = SelectedAnimSet->UseTranslationBoneNames;
			DestAnimSet->ForceMeshTranslationBoneNames = SelectedAnimSet->ForceMeshTranslationBoneNames;
			DestAnimSet->MarkPackageDirty();
		}
	}
}


static INT NumTotalTracks = 0;
static INT NumAdditiveTracks = 0;
static INT NumNoTrackData = 0;
static INT NumTotalBones = 0;

static INT NumDeletedTracks = 0;
static INT NumDeletedAdditiveTracks = 0;
static INT NumDeletedRotationOnlyTracks = 0;

#define MAXPOSDIFF			(0.0001f)
#define MAXANGLEDIFF		(0.0003f)
#define TESTMAXPOSDIFF		(0.001f)
#define TESTMAXANGLEDIFF	(0.003f)

void WxAnimSetViewer::AnalyzeAnimSet()
{
	if (!SelectedAnimSet)
	{
		return;
	}

	const INT AnimLinkupIndex = SelectedAnimSet->GetMeshLinkupIndex( SelectedSkelMesh );
	check( AnimLinkupIndex != INDEX_NONE );
	check( AnimLinkupIndex < SelectedAnimSet->LinkupCache.Num() );

	const FAnimSetMeshLinkup& AnimLinkup	= SelectedAnimSet->LinkupCache( AnimLinkupIndex );
	const TArray<FMeshBone>& RefSkel		= SelectedSkelMesh->RefSkeleton;
	check( AnimLinkup.BoneToTrackTable.Num() == RefSkel.Num() );

	TArray<INT> UsedTracksArray;
	UsedTracksArray.AddZeroed( SelectedAnimSet->TrackBoneNames.Num() );
	TArray<INT> DeletedTracksArray;
	DeletedTracksArray.AddZeroed( SelectedAnimSet->TrackBoneNames.Num() );
	TArray<INT> DeletedAdditiveTracksArray;
	DeletedAdditiveTracksArray.AddZeroed( SelectedAnimSet->TrackBoneNames.Num() );
	TArray<FLOAT> TrackLargestTranslationError;
	TArray<FLOAT> TrackLargestRotationError;

	TrackLargestTranslationError.AddZeroed( SelectedAnimSet->TrackBoneNames.Num() );
	TrackLargestRotationError.AddZeroed( SelectedAnimSet->TrackBoneNames.Num() );

	INT BoneIndex;
	for(INT AnimSeqIndex=0; AnimSeqIndex<SelectedAnimSet->Sequences.Num(); AnimSeqIndex++)
	{
		UAnimSequence* AnimSeq = SelectedAnimSet->Sequences(AnimSeqIndex);
		
		// Recompress Raw data just to be safe.
		AnimSeq->CompressRawAnimData();

		const INT NumTracks = AnimSeq->RawAnimationData.Num();
		NumTotalTracks += NumTracks;
		if( AnimSeq->bIsAdditive )
		{
			NumAdditiveTracks += NumTracks;
		}

		NumNoTrackData += (RefSkel.Num() - NumTracks);
		NumTotalBones += RefSkel.Num();

		for(INT TrackIndex=0; TrackIndex<NumTracks; TrackIndex++)
		{
			FRawAnimSequenceTrack& RawTrack = AnimSeq->RawAnimationData(TrackIndex);
			UBOOL bTrackIsNeeded = TRUE;

			if( RawTrack.PosKeys.Num() == 1 && RawTrack.RotKeys.Num() == 1 )
			{
				if( AnimSeq->bIsAdditive )
				{
					FLOAT const TranslationError = RawTrack.PosKeys(0).Size();
					FLOAT const RotationError = FQuatError(FQuat::Identity, RawTrack.RotKeys(0));
					TrackLargestTranslationError(TrackIndex) = Max<FLOAT>(TrackLargestTranslationError(TrackIndex), TranslationError);
					TrackLargestRotationError(TrackIndex) = Max<FLOAT>(TrackLargestRotationError(TrackIndex), RotationError);
					if( TranslationError <= MAXPOSDIFF && RotationError <= MAXANGLEDIFF )
					{
						NumDeletedTracks++;
						NumDeletedAdditiveTracks++;
						DeletedTracksArray(TrackIndex)++;
						DeletedAdditiveTracksArray(TrackIndex)++;
						bTrackIsNeeded = FALSE;
					}
				}
				else
				{
					if( AnimLinkup.BoneToTrackTable.FindItem(TrackIndex, BoneIndex) )
					{
						FVector RefBoneTranslation = RefSkel(BoneIndex).BonePos.Position;
						FQuat RefBoneRotation = RefSkel(BoneIndex).BonePos.Orientation;

						FLOAT const TranslationError = (RawTrack.PosKeys(0) - RefBoneTranslation).Size();
						FLOAT const RotationError = FQuatError(RefBoneRotation, RawTrack.RotKeys(0));

						TrackLargestTranslationError(TrackIndex) = Max<FLOAT>(TrackLargestTranslationError(TrackIndex), TranslationError);
						TrackLargestRotationError(TrackIndex) = Max<FLOAT>(TrackLargestRotationError(TrackIndex), RotationError);

						if( TranslationError <= MAXPOSDIFF && RotationError <= MAXANGLEDIFF )
						{
							NumDeletedTracks++;
							DeletedTracksArray(TrackIndex)++;
							bTrackIsNeeded = FALSE;
						}
						else if( (SelectedAnimSet->ForceMeshTranslationBoneNames.FindItemIndex(RefSkel(BoneIndex).Name) != INDEX_NONE || (SelectedAnimSet->bAnimRotationOnly && SelectedAnimSet->UseTranslationBoneNames.FindItemIndex(RefSkel(BoneIndex).Name) == INDEX_NONE))
							&& RotationError <= MAXANGLEDIFF )
						{
							NumDeletedTracks++;
							NumDeletedRotationOnlyTracks++;
							DeletedTracksArray(TrackIndex)++;
							bTrackIsNeeded = FALSE;
						}
					}
					else
					{
						warnf(TEXT("BoneIndex not found for Track %d (%s)"), TrackIndex, *SelectedAnimSet->TrackBoneNames(TrackIndex).ToString());
					}
				}
			}

			if( bTrackIsNeeded )
			{
				UsedTracksArray(TrackIndex) = 1;
			}
		}
	}

	INT TracksThatCanBeDeletedInSet = 0;
	for(INT TrackIndex=0; TrackIndex<UsedTracksArray.Num(); TrackIndex++)
	{
		if( UsedTracksArray(TrackIndex) == 0 )
		{
			TracksThatCanBeDeletedInSet++;
		}
	}

	debugf(TEXT("Evalutate Anim Tracks... %s"), *SelectedAnimSet->GetFName().ToString());
	debugf(TEXT("\t NumTotalTracks: %d, NumAdditiveTracks: %d, NumNoTrackData: %d"), NumTotalTracks, NumAdditiveTracks, NumNoTrackData);
	debugf(TEXT("\t NumDeletedTracks: %d (%d%%)"), NumDeletedTracks, NumTotalTracks ? (NumDeletedTracks * 100) / NumTotalTracks : 0);
	debugf(TEXT("\t NumDeletedAdditiveTracks: %d (%d%%)"), NumDeletedAdditiveTracks, NumDeletedAdditiveTracks ? (NumDeletedAdditiveTracks * 100) / NumAdditiveTracks : 0);
	debugf(TEXT("\t NumDeletedRotationOnlyTracks: %d (%d%%)"), NumDeletedRotationOnlyTracks, NumTotalTracks ? (NumDeletedRotationOnlyTracks * 100) / NumTotalTracks : 0);
	debugf(TEXT("\t DeletedTracksMemSavings: %d"), NumDeletedTracks * (sizeof(FVector) + sizeof(FQuat)) );
	debugf(TEXT("\t Average BlendTree Gain: %d%%"), NumTotalBones ? 100 * (NumDeletedTracks + NumNoTrackData) / NumTotalBones : 0);
	debugf(TEXT("\t TracksThatCanBeDeletedInSet: %d"), TracksThatCanBeDeletedInSet );
	for(INT TrackIndex=0; TrackIndex<UsedTracksArray.Num(); TrackIndex++)
	{
		if( UsedTracksArray(TrackIndex) == 0 )
		{
			debugf(TEXT("\t\t Track: %3d (%s)"), TrackIndex, *SelectedAnimSet->TrackBoneNames(TrackIndex).ToString() );
		}
	}
	debugf(TEXT("\t NumBones: %d, NumSequences: %d"), RefSkel.Num(), SelectedAnimSet->Sequences.Num());
	for(INT TrackIndex=0; TrackIndex<DeletedTracksArray.Num(); TrackIndex++)
	{
		if( DeletedTracksArray(TrackIndex) > 0 )
		{
			debugf(TEXT("\t\t %3d (%s) \t\tTrack Deleted %4d time(s) (%4d Additive) (%3d%%) MaxTranslationError: %f, MaxRotationError: %f"), 
				TrackIndex, 
				*SelectedAnimSet->TrackBoneNames(TrackIndex).ToString(),
				DeletedTracksArray(TrackIndex), 
				DeletedAdditiveTracksArray(TrackIndex), 
				(100 * DeletedTracksArray(TrackIndex)) / SelectedAnimSet->Sequences.Num(),
				TrackLargestTranslationError(TrackIndex),
				TrackLargestRotationError(TrackIndex) );
		}
	}

	debugf(TEXT("Scanning all tracks..."));
	INT MaxNumInvestigations = 0;
	INT MaxReductionErrors = 0;
	// Spit out tracks that couldn't be deleted but might be good candidates 
	for(INT TrackIndex=0; TrackIndex<SelectedAnimSet->TrackBoneNames.Num(); TrackIndex++)
	{
		debugf(TEXT("\tTrack: %3d (%s)"), TrackIndex, *SelectedAnimSet->TrackBoneNames(TrackIndex).ToString() );

		// Compute Max Translation and Max Rotation Error for this track
		FLOAT MaxTranslationError = 0.f;
		FLOAT MaxRotationError = 0.f;

		INT NumInvestigations = 0;
		INT NumReductionErrors = 0;

		for(INT AnimSeqIndex=0; AnimSeqIndex<SelectedAnimSet->Sequences.Num(); AnimSeqIndex++)
		{
			UAnimSequence* AnimSeq = SelectedAnimSet->Sequences(AnimSeqIndex);
			FRawAnimSequenceTrack& RawTrack = AnimSeq->RawAnimationData(TrackIndex);

			if( !AnimLinkup.BoneToTrackTable.FindItem(TrackIndex, BoneIndex) )
			{
				continue;
			}

			// Compute Max Translation and Max Rotation Error for this track
			FLOAT AnimTranslationError = 0.f;
			FLOAT AnimRotationError = 0.f;

			FVector RefBoneTranslation = RefSkel(BoneIndex).BonePos.Position;
			FQuat RefBoneRotation = RefSkel(BoneIndex).BonePos.Orientation;

			for(INT KeyIdx=0; KeyIdx<RawTrack.PosKeys.Num(); KeyIdx++)
			{
				FLOAT const TranslationError = AnimSeq->bIsAdditive ? RawTrack.PosKeys(KeyIdx).Size() : (RawTrack.PosKeys(KeyIdx) - RefBoneTranslation).Size();
				MaxTranslationError = Max<FLOAT>(MaxTranslationError, TranslationError);
				AnimTranslationError = Max<FLOAT>(AnimTranslationError, TranslationError);
			}
			for(INT KeyIdx=0; KeyIdx<RawTrack.RotKeys.Num(); KeyIdx++)
			{
				FLOAT const RotationError = FQuatError(AnimSeq->bIsAdditive ? FQuat::Identity : RefBoneRotation, RawTrack.RotKeys(KeyIdx));
				MaxRotationError = Max<FLOAT>(MaxRotationError, RotationError);
				AnimRotationError = Max<FLOAT>(AnimRotationError, RotationError);
			}	

			// This is good to be skipped!! Verify that we actually reduced those to 1 frame
			if( AnimTranslationError <= MAXPOSDIFF && AnimRotationError <= MAXANGLEDIFF )
			{
				if( RawTrack.PosKeys.Num() > 1 || RawTrack.RotKeys.Num() > 1 )
				{
					debugf(TEXT("\t\tTrack not correctly reduced!! %s. AnimTranslationError: %f, AnimRotationError: %f, PosKeys: %d, RotKeys: %d"), *AnimSeq->SequenceName.ToString(), AnimTranslationError, AnimRotationError, RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num());
					NumReductionErrors++;
					MaxReductionErrors++;
				}
			}
			else if( AnimTranslationError <= TESTMAXPOSDIFF && AnimRotationError <= TESTMAXANGLEDIFF )
			{
				debugf(TEXT("\t\tInvestigate for %s. AnimTranslationError: %f, AnimRotationError: %f"), *AnimSeq->SequenceName.ToString(), AnimTranslationError, AnimRotationError);
				NumInvestigations++;
				MaxNumInvestigations++;
			}
		}

		debugf(TEXT("\t\t MaxTranslationError: %f, MaxRotationError: %f, NumReductionErrors: %d, NumInvestigations: %d"), MaxTranslationError, MaxRotationError, NumReductionErrors, NumInvestigations);
	}
	debugf(TEXT("MaxNumInvestigations: %d, MaxReductionErrors: %d"), MaxNumInvestigations, MaxReductionErrors);
}

void WxAnimSetViewer::RenameSelectedSeq()
{
	if( SelectedAnimSeq )
	{
		check(SelectedAnimSet);

		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("RenameAnimSequence"), TEXT("NewSequenceName"), *SelectedAnimSeq->SequenceName.ToString() );
		if( Result == wxID_OK )
		{
			const FString NewSeqString = dlg.GetEnteredString();
			const FName NewSeqName =  FName( *NewSeqString );

			// Don't do anything if the user has elected to keep the pre-existing name
			if ( NewSeqName != SelectedAnimSeq->SequenceName )
			{
				// If there is a sequence with that name already, see if we want to over-write it.
				UAnimSequence* FoundSeq = SelectedAnimSet->FindAnimSequence(NewSeqName);
				if(FoundSeq)
				{
					const FString ConfirmMessage = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_2"), *NewSeqName.ToString()) );
					const UBOOL bDoDelete = appMsgf( AMT_YesNo, *ConfirmMessage );
					if( !bDoDelete )
					{
						return;
					}

					SelectedAnimSet->RemoveAnimSequenceFromAnimSet( FoundSeq );
				}

				SelectedAnimSeq->SequenceName = NewSeqName;

				UpdateAnimSeqList();

				// This will reselect this sequence, update the AnimNodeSequence, status bar etc.
				SetSelectedAnimSequence( SelectedAnimSeq );


				SelectedAnimSet->MarkPackageDirty();
			}
		}
	}
}

void WxAnimSetViewer::RemovePrefixFromSequences()
{
	FString Prefix;

	if( SelectedAnimSeq )
	{
		check(SelectedAnimSet);

		WxDlgGenericStringEntry dlg;
		const INT Result = dlg.ShowModal( TEXT("RemovePrefixFromSelectedSequences"), TEXT("PrefixToRemove"), *SelectedAnimSeq->SequenceName.ToString() );

		if( Result == wxID_OK )
		{
			Prefix = dlg.GetEnteredString();
		}
	}

	if ( Prefix.Len() == 0 )
	{
		warnf( NAME_Log, TEXT("Rename failed: Prefix Empty")  );
		return;
	}

	// Get the list of animations selected
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);
	if ( Selection.Count() == 0 )
	{
		warnf( NAME_Log, TEXT("Rename failed: Nothing selected")  );
		return;
	}

	// Build list of animations to delete
	TArray<class UAnimSequence*> Sequences;
	FString SeqListString = TEXT(" ");
	for ( UINT i = 0; i < Selection.Count(); i++ )
	{

		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData( SelIndex );

		if( AnimSeq )
		{
			FString NewName = AnimSeq->SequenceName.ToString();

			if ( NewName.StartsWith(Prefix) )
			{
				NewName = NewName.Mid(Prefix.Len(), NewName.Len() - Prefix.Len());	
				if (NewName.Len() >0)
				{
					warnf( NAME_Log, TEXT("Renaming AnimSequence %s to %s"),*AnimSeq->SequenceName.ToString(), *NewName );
					AnimSeq->SequenceName = FName( *NewName);						
				}
				else
				{
					warnf( NAME_Log, TEXT("Can`t rename %s to empty name"),*AnimSeq->SequenceName.ToString()  );
					appDebugMessagef(*FString::Printf(TEXT("Can`t rename %s to empty name"),*SelectedAnimSeq->SequenceName.ToString() ));
				}
			}
		}
	}

	// Refresh list
	UpdateAnimSeqList();

	SelectedAnimSet->MarkPackageDirty();
}

void WxAnimSetViewer::DeleteSelectedSequence()
{
	// Get the list of animations selected
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);

	// Build list of animations to delete
	TArray<class UAnimSequence*> Sequences;
	for (UINT i=0; i<Selection.Count(); i++)
	{
		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
		if( AnimSeq )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
		}
	}

	TArray<UAnimSequence*> SeqsToRemove;

	// check to see if there is base animation to be deleted, then warn user
	// to see if they still like to delete the base animation
	RemoveAdditiveSequences(Sequences, SeqsToRemove, FALSE, TRUE );

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		return;
	}

	FString SeqListString = TEXT(" ");
	for (INT i=0; i<Sequences.Num(); i++)
	{
		SeqListString = FString::Printf( TEXT("%s\n %s"), *SeqListString, *Sequences(i)->SequenceName.ToString());
	}

	// Pop up a message to make sure the user really wants to do this
	FString ConfirmMessage = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_3"), *SeqListString) );
	UBOOL bDoDelete = appMsgf( AMT_YesNo, *ConfirmMessage );
	if( !bDoDelete )
	{
		return;
	}

	// Now go through all sequences and delete!
	INT LastIndex = INDEX_NONE;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			SelectedAnimSet->RemoveAnimSequenceFromAnimSet(AnimSeq);
		}
	}

	// Refresh list
	UpdateAnimSeqList();

	// Select previous or next animation in the list (if present) - or none at all.
	if( SelectedAnimSet->Sequences.Num() > 0 )
	{
		INT SelectedIndex = Clamp<INT>(LastIndex, 0, SelectedAnimSet->Sequences.Num() - 1);
		SetSelectedAnimSequence( SelectedAnimSet->Sequences(SelectedIndex) );
	}
	else
	{
		SetSelectedAnimSequence( NULL );
	}

	SelectedAnimSet->MarkPackageDirty();
}

/* 
 Remove any of the selected sequences are additive or related to additive sequences and return # of animsequences that are removed from the list
 @param Sequences: (in/out) array of animation sequence to check
 @param SequencesToRemove: (out) array of animations to be removed
 @param bRemoveAdditive: Remove Additive Sequences
 @param bRemoveBase: Remove Base Sequences
 @return : # of animations to be removed
 */
INT WxAnimSetViewer::RemoveAdditiveSequences(TArray<UAnimSequence*>& Sequences, TArray<UAnimSequence*>& SequencesToRemove, const UBOOL bRemoveAdditive, const UBOOL bRemoveBase )
{
	FString AdditiveSequenceNames = TEXT("\n");
	SequencesToRemove.Empty();

	for (INT SeqIdx = Sequences.Num() - 1; SeqIdx >= 0; --SeqIdx)
	{
		UAnimSequence* Sequence = Sequences(SeqIdx);
		// if additive animation to remove and if additive remove
		if (bRemoveAdditive && Sequence->bIsAdditive)
		{
			SequencesToRemove.AddItem(Sequence);
			Sequences.Remove(SeqIdx);
		}
		// if base, then check related additive animsequences
		else if(bRemoveBase && Sequence->RelatedAdditiveAnimSeqs.Num() > 0)
		{
			for ( INT AdditiveSeqID=0; AdditiveSeqID<Sequence->RelatedAdditiveAnimSeqs.Num(); ++AdditiveSeqID )
			{
				AdditiveSequenceNames = FString::Printf(TEXT("%s %s\n"), *AdditiveSequenceNames, *Sequence->RelatedAdditiveAnimSeqs(AdditiveSeqID)->SequenceName.ToString());
			}

			// warn if they would still like to move or delete
			FString WarningMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prompt_MoveOrDeleteAdditiveSequence"), *Sequence->SequenceName.ToString(), *AdditiveSequenceNames));
			const UBOOL bProceed = appMsgf(AMT_YesNo, *WarningMessage);
			if (!bProceed)
			{
				// if not, remove it from the list
				SequencesToRemove.AddItem(Sequence);
				Sequences.Remove(SeqIdx);
			}
		}
	}

	return SequencesToRemove.Num();
}
/* 
 Verify any of the selected sequences are additive or related to additive sequences and see if we would like to continue
 @param Sequences: (in/out) array of animation sequence to check
 @param bMove: TRUE if we're moving
 @return : TRUE if # of Sequences is 0, meaning nothing to be done anymore
 
*/
UBOOL WxAnimSetViewer::VerifyAdditiveSequencesToMove(TArray<UAnimSequence*>& Sequences, UAnimSet* DestAnimSet, UBOOL bMove)
{
	check(DestAnimSet);
	if (SelectedAnimSet->GetOutermost() != DestAnimSet->GetOutermost())
	{
		TArray<UAnimSequence*> SeqsToRemove;

		FString RemovedSequenceNames = TEXT("\n");
		if (RemoveAdditiveSequences(Sequences, SeqsToRemove, TRUE, bMove) > 0)
		{
			for ( INT SeqID=0; SeqID<SeqsToRemove.Num(); ++SeqID )
			{
				RemovedSequenceNames = FString::Printf(TEXT("%s %s\n"), *RemovedSequenceNames, *SeqsToRemove(SeqID)->SequenceName.ToString());
			}

			FString WarningMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("Prompt_CantCopyAdditiveSequence"), *RemovedSequenceNames, *DestAnimSet->GetName()));
			appMsgf(AMT_OK, *WarningMessage);

			if( Sequences.Num() == 0 )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/** Copy animation selection to new animation set. */
void WxAnimSetViewer::CopySelectedSequence()
{
	// Get the list of animations selected
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);

	// Build list of animations to move
	TArray<class UAnimSequence*> Sequences;
	FString SeqListString = TEXT(" ");
	for (UINT i=0; i<Selection.Count(); i++)
	{
		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
		if( AnimSeq )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
			SeqListString = FString::Printf( TEXT("%s\n %s"), *SeqListString, *AnimSeq->SequenceName.ToString());
		}
	}

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		return;
	}

	// Get selected AnimSet, and check we have one!
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
	UAnimSet* DestAnimSet = GEditor->GetSelectedObjects()->GetTop<UAnimSet>();
	if( !DestAnimSet )
	{
		// If there is no Destination, assume dest animset is current animset
		DestAnimSet = SelectedAnimSet;
	}

	// Pop up a message to make sure the user really wants to do this if it's different animset
	if ( DestAnimSet != SelectedAnimSet )
	{
		FString ConfirmMessage = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_34"), *SeqListString, *DestAnimSet->GetName()) );
		UBOOL bProceed = appMsgf( AMT_YesNo, *ConfirmMessage );
		if( !bProceed )
		{
			return;
		}
	}

	// Make sure additive or related additive sequences aren't being copied to a different package
	// Returns false of all sequences have been removed from the selected set
	if ( !VerifyAdditiveSequencesToMove(Sequences, DestAnimSet, FALSE) )
	{
		return;
	}

	// Now go through all sequences and move them!
	INT LastIndex = INDEX_NONE;
	UBOOL bAskAboutReplacing = TRUE;
	UBOOL bDoReplace = FALSE;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			// See if this sequence already exists in destination
			UAnimSequence* DestSeq = DestAnimSet->FindAnimSequence(AnimSeq->SequenceName);
			// Replace Sequence Name when moving to same AnimSet
			UBOOL bReplaceSequenceName = ( DestAnimSet == SelectedAnimSet );
			// New Sequence Name To replace
			FName NewSeqName;
			// If not, create new one now.
			if( !DestSeq )
			{
				DestSeq = ConstructObject<UAnimSequence>( UAnimSequence::StaticClass(), DestAnimSet );
			}
			else
			{
				if (!bReplaceSequenceName && bAskAboutReplacing)
				{
					// if it's different animset, ask if they'd like to replace
					UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Prompt_25"), *AnimSeq->SequenceName.ToString()));
					bDoReplace = MsgResult == ART_Yes || MsgResult == ART_YesAll;
					bAskAboutReplacing = MsgResult == ART_No || MsgResult == ART_Yes;
				}
				
				if( !bDoReplace )
				{
					do 
					{
						// If they do not like to replace, show rename dialogue
						WxDlgGenericStringEntry dlg;
						// rename dialogue
						const INT Result = dlg.ShowModal( TEXT("NewSequenceName"), TEXT("NewSequenceName"), *AnimSeq->SequenceName.ToString() );
						// if Okay
						if( Result == wxID_OK )
						{
							// got new name
							const FString NewSeqString = dlg.GetEnteredString();
							NewSeqName =  FName( *NewSeqString );
							bReplaceSequenceName = TRUE;
							// create new one and assign name, so that when copied it's added to the animset
							DestSeq = ConstructObject<UAnimSequence>( UAnimSequence::StaticClass(), DestAnimSet );
							DestSeq->SequenceName = NewSeqName;
						}
						else
						{
							// clear new seq name
							bReplaceSequenceName = FALSE;
							break;
						}

						// do this until New Seq Name doesn't exist
						if (DestAnimSet->FindAnimSequence(NewSeqName)!=NULL)
						{
							// warn if the name already exists
							appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_39") );
						}
						else
						{
							break;
						}

					} while (1);
				
					// stop asking, just move onto next one
					if ( !bReplaceSequenceName )
					{
						continue; // Move on to next sequence...
					}
				}
			}
			
			// Copy AnimSeq information who belongs to SelectedAnimSet, into DestAnimSeq to be put into DestAnimSet.
			if( !CopyAnimSequence(AnimSeq, DestSeq, SelectedAnimSet, DestAnimSet, SelectedSkelMesh) )
			{		
				// Abort
				return;
			}

			// if Copy succeed, 
			// Copy new name again
			if ( bReplaceSequenceName )
			{
				DestSeq->SequenceName = NewSeqName;
			}
		}
	}

	// if dest animation is same as current animset, please refresh
	if ( DestAnimSet == SelectedAnimSet )
	{
		UpdateAnimSeqList();

		// This will reselect this sequence, update the AnimNodeSequence, status bar etc.
		SetSelectedAnimSequence(SelectedAnimSeq);
	}

	// Mark destination package dirty
	DestAnimSet->MarkPackageDirty();
}


/** Move animation selection to new animation set. */
void WxAnimSetViewer::MoveSelectedSequence()
{
	// Get the list of animations selected
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);

	// Build list of animations to move
	TArray<class UAnimSequence*> Sequences;
	FString SeqListString = TEXT(" ");
	for (UINT i=0; i<Selection.Count(); i++)
	{
		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
		if( AnimSeq )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
			SeqListString = FString::Printf( TEXT("%s\n %s"), *SeqListString, *AnimSeq->SequenceName.ToString());
		}
	}

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		return;
	}

	// Get selected AnimSet, and check we have one!
	GCallbackEvent->Send( CALLBACK_LoadSelectedAssetsIfNeeded );
	UAnimSet* DestAnimSet = GEditor->GetSelectedObjects()->GetTop<UAnimSet>();
	if( !DestAnimSet )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("NoDestinationAnimSetSelected") );
		return;
	}

	// Check we're not moving to same AnimSet... duh!
	if( DestAnimSet == SelectedAnimSet )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("AnimSetSelectedIsSourceAnimSet") );
		return;
	}

	// Pop up a message to make sure the user really wants to do this
	FString ConfirmMessage = FString::Printf( LocalizeSecure(LocalizeUnrealEd("Prompt_32"), *SeqListString, *DestAnimSet->GetName()) );
	UBOOL bDoDelete = appMsgf( AMT_YesNo, *ConfirmMessage );
	if( !bDoDelete )
	{
		return;
	}

	// Make sure additive or related additive sequences aren't being moved to a different package
	// Returns false of all sequences have been removed from the selected set
	if ( !VerifyAdditiveSequencesToMove(Sequences, DestAnimSet, TRUE) )
	{
		return;
	}

	// Now go through all sequences and move them!
	INT LastIndex = INDEX_NONE;
	UBOOL bAskAboutReplacing = TRUE;
	UBOOL bDoReplace = FALSE;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			// See if this sequence already exists in destination
			UAnimSequence* DestSeq = DestAnimSet->FindAnimSequence(AnimSeq->SequenceName);
			// If not, create new one now.
			if( !DestSeq )
			{
				DestSeq = ConstructObject<UAnimSequence>( UAnimSequence::StaticClass(), DestAnimSet );
			}
			else
			{
				if (bAskAboutReplacing)
				{
					// if it's different animset, ask if they'd like to replace
					UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Prompt_25"), *AnimSeq->SequenceName.ToString()));
					bDoReplace = MsgResult == ART_Yes || MsgResult == ART_YesAll;
					bAskAboutReplacing = MsgResult == ART_No || MsgResult == ART_Yes;
				}

				if( !bDoReplace )
				{
					continue; // Move on to next sequence...
				}
			}
			
			// Copy AnimSeq information who belongs to SelectedAnimSet, into DestAnimSeq to be put into DestAnimSet.
			if( CopyAnimSequence(AnimSeq, DestSeq, SelectedAnimSet, DestAnimSet, SelectedSkelMesh) )
			{		
				// now delete AnimSeq from source AnimSet
				SelectedAnimSet->RemoveAnimSequenceFromAnimSet(AnimSeq);
			}
			// Abort
			else
			{
				return;
			}
		}
	}

	// Refresh list
	UpdateAnimSeqList();

	// Select previous or next animation in the list (if present) - or none at all.
	if( SelectedAnimSet->Sequences.Num() > 0 )
	{
		INT SelectedIndex = Clamp<INT>(LastIndex, 0, SelectedAnimSet->Sequences.Num() - 1);
		SetSelectedAnimSequence( SelectedAnimSet->Sequences(SelectedIndex) );
	}
	else
	{
		SetSelectedAnimSequence( NULL );
	}

	SelectedAnimSet->MarkPackageDirty();
	DestAnimSet->MarkPackageDirty();
}

/** 
 * Copy SourceAnimSeq in SourceAnimSet to DestAnimSeq in DestAnimSet.
 * Returns TRUE for success, FALSE for failure.
 */
UBOOL WxAnimSetViewer::CopyAnimSequence(UAnimSequence* SourceAnimSeq, UAnimSequence *DestAnimSeq, UAnimSet* SourceAnimSet, UAnimSet* DestAnimSet, USkeletalMesh* FillInMesh)
{
	// Make sure input is valid
	check( SourceAnimSeq && DestAnimSeq && SourceAnimSet && DestAnimSet && FillInMesh );

	// Attempt to use destination SkeletalMesh if we have any remapping to do.
	if( DestAnimSet->PreviewSkelMeshName != NAME_None )
	{
		USkeletalMesh* DestMesh = LoadObject<USkeletalMesh>(NULL, *DestAnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
		// Success!
		if( DestMesh != NULL )
		{
			FillInMesh = DestMesh;
		}
	}

	// Calculate track mapping from target tracks in DestAnimSet to the source among those we are importing.
	TArray<INT> TrackMap;

	// If Destination is an empty AnimSet, we copy this information from the source animset.
	if( DestAnimSet->TrackBoneNames.Num() == 0)
	{
		DestAnimSet->PreviewSkelMeshName = SourceAnimSet->PreviewSkelMeshName;
		DestAnimSet->TrackBoneNames = SourceAnimSet->TrackBoneNames;
		TrackMap.Add( DestAnimSet->TrackBoneNames.Num() );

		for(INT i=0; i<DestAnimSet->TrackBoneNames.Num(); i++)
		{
			TrackMap(i) = i;
		}
	}
	else
	{
		// Otherwise, ensure right track goes to right place.
		// If we are missing a track, we give a warning and refuse to import into this set.
		TrackMap.Add( DestAnimSet->TrackBoneNames.Num() );

		// For each track in the AnimSet, find its index in the source data
		UBOOL bAskAboutPatching = TRUE;
		UBOOL bDoPatching = FALSE;
		for(INT i=0; i<DestAnimSet->TrackBoneNames.Num(); i++)
		{
			FName TrackName = DestAnimSet->TrackBoneNames(i);
			TrackMap(i) = INDEX_NONE;

			for(INT j=0; j<SourceAnimSet->TrackBoneNames.Num(); j++)
			{
				FName TestName = SourceAnimSet->TrackBoneNames(j);
				if( TestName == TrackName )
				{	
					if( TrackMap(i) != INDEX_NONE )
					{
						debugf( TEXT(" DUPLICATE TRACK IN INCOMING DATA: %s"), *TrackName.ToString() );
					}
					TrackMap(i) = j;
				}
			}

			// If we failed to find a track we need in the imported data - see if we want to patch it using the skeletal mesh ref pose.
			if( TrackMap(i) == INDEX_NONE )
			{				
				if (bAskAboutPatching)
				{
					UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindTrack"), *TrackName.ToString()));
					bDoPatching = MsgResult == ART_Yes || MsgResult == ART_YesAll;
					bAskAboutPatching = MsgResult == ART_No || MsgResult == ART_Yes;
				}

				if( bDoPatching )
				{
					// Check the selected skel mesh has a bone called that. If we can't find it - fail.
					INT PatchBoneIndex = FillInMesh->MatchRefBone(TrackName);
					if( PatchBoneIndex == INDEX_NONE )
					{
						appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_ExtraInfoInAnimFile"), *TrackName.ToString()));
						return FALSE;
					}
				}
				else
				{
					return FALSE;
				}
			}
		}
	}

	// Flag to indicate that the AnimSet has had its TrackBoneNames array changed, and therefore its LinkupCache needs to be flushed.
	UBOOL bAnimSetTrackChanged = FALSE;

	// Now we see if there are any tracks in the incoming data which do not have a use in the DestAnimSet. 
	// These will be thrown away unless we extend the DestAnimSet name table.
	TArray<FName> AnimSetMissingNames;
	TArray<UAnimSequence*> SequencesRequiringRecompression;
	for(INT i=0; i<SourceAnimSet->TrackBoneNames.Num(); i++)
	{
		if( !TrackMap.ContainsItem(i) )
		{
			FName ExtraTrackName = SourceAnimSet->TrackBoneNames(i);
			UBOOL bDoExtension = appMsgf(AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("Error_ExtraInfoInAnimFile"), *ExtraTrackName.ToString()));
			if( bDoExtension )
			{
				INT PatchBoneIndex = FillInMesh->MatchRefBone(ExtraTrackName);

				// If we can't find the extra track in the SkelMesh to create an animation from, warn and fail.
				if( PatchBoneIndex == INDEX_NONE )
				{
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_CouldNotFindPatchBone"), *ExtraTrackName.ToString()));
					return FALSE;
				}
				// If we could, add to all animations in the AnimSet, and add to track table.
				else
				{
					DestAnimSet->TrackBoneNames.AddItem(ExtraTrackName);
					bAnimSetTrackChanged = TRUE;

					// Iterate over all existing sequences in this set and add an extra track to the end.
					for(INT SetAnimIndex=0; SetAnimIndex<DestAnimSet->Sequences.Num(); SetAnimIndex++)
					{
						UAnimSequence* ExtendSeq = DestAnimSet->Sequences(SetAnimIndex);

						// Remove any compression on the sequence so that it will be recomputed with the new track.
						if( ExtendSeq->CompressedTrackOffsets.Num() > 0 )
						{
							ExtendSeq->CompressedTrackOffsets.Empty();
							// Mark the sequence as requiring recompression.
							SequencesRequiringRecompression.AddUniqueItem( ExtendSeq );
						}

						// Add an extra track to the end, based on the ref skeleton.
						ExtendSeq->RawAnimationData.AddZeroed();
						FRawAnimSequenceTrack& RawTrack = ExtendSeq->RawAnimationData( ExtendSeq->RawAnimationData.Num()-1 );

						// Create 1-frame animation from the reference pose of the skeletal mesh.
						// This is basically what the compression does, so should be fine.
						if( ExtendSeq->bIsAdditive )
						{
							RawTrack.PosKeys.AddItem(FVector(0.f));

							FQuat RefOrientation = FQuat::Identity;
							// To emulate ActorX-exported animation quat-flipping, we do it here.
							if( PatchBoneIndex > 0 )
							{
								RefOrientation.W *= -1.f;
							}
							RawTrack.RotKeys.AddItem(RefOrientation);

							// Extend AdditiveBasePose
							const FMeshBone& RefSkelBone = FillInMesh->RefSkeleton(PatchBoneIndex);

							FBoneAtom RefBoneAtom(RefSkelBone.BonePos.Orientation,
								RefSkelBone.BonePos.Position);
							if( PatchBoneIndex > 0)
							{
								RefBoneAtom.FlipSignOfRotationW(); // As above - flip if necessary
							}

							// Save off RefPose into destination AnimSequence
							ExtendSeq->AdditiveBasePose.AddZeroed();
							FRawAnimSequenceTrack& BasePoseTrack = ExtendSeq->AdditiveBasePose( ExtendSeq->AdditiveBasePose.Num()-1 );
							BasePoseTrack.PosKeys.AddItem(RefBoneAtom.GetTranslation());
							BasePoseTrack.RotKeys.AddItem(RefBoneAtom.GetRotation());
						}
						else
						{
							const FVector RefPosition = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
							RawTrack.PosKeys.AddItem(RefPosition);

							FQuat RefOrientation = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
							// To emulate ActorX-exported animation quat-flipping, we do it here.
							if( PatchBoneIndex > 0 )
							{
								RefOrientation.W *= -1.f;
							}
							RawTrack.RotKeys.AddItem(RefOrientation);
						}
					}

					// So now the new incoming track 'i' maps to the last track in the AnimSet.
					TrackMap.AddItem(i);
				}
			}
		}
	}

	// Make sure DestAnimSeq's outer is the DestAnimSet
	if( DestAnimSeq->GetOuter() != DestAnimSet )
	{
		DestAnimSeq->Rename( *DestAnimSeq->GetName(), DestAnimSet );
	}

	// Make sure DestAnimSeq belongs to DestAnimSet
	if( DestAnimSet->Sequences.FindItemIndex(DestAnimSeq) == INDEX_NONE )
	{
		DestAnimSet->Sequences.AddItem( DestAnimSeq );
	}

	// Initialize DestAnimSeq
	DestAnimSeq->RecycleAnimSequence();

	// Copy all parameters from source to destination
	UAnimSequence::CopyAnimSequenceProperties(SourceAnimSeq, DestAnimSeq);

	// Make sure data is zeroed
	DestAnimSeq->RawAnimationData.AddZeroed( DestAnimSet->TrackBoneNames.Num() );
	if( DestAnimSeq->bIsAdditive )
	{
		DestAnimSeq->AdditiveBasePose.AddZeroed( DestAnimSet->TrackBoneNames.Num() );
	}

	// Structure of data is this:
	// RawAnimKeys contains all keys. 
	// Sequence info FirstRawFrame and NumRawFrames indicate full-skel frames (NumPSATracks raw keys). Ie number of elements we need to copy from RawAnimKeys is NumRawFrames * NumPSATracks.

	// Import each track.
	for(INT TrackIdx = 0; TrackIdx < DestAnimSet->TrackBoneNames.Num(); TrackIdx++)
	{
		check( DestAnimSet->TrackBoneNames.Num() == DestAnimSeq->RawAnimationData.Num() );
		FRawAnimSequenceTrack& RawTrack = DestAnimSeq->RawAnimationData(TrackIdx);

		// Find the source track for this one in the AnimSet
		INT SourceTrackIdx = TrackMap( TrackIdx );

		// If bone was not found in incoming data, use SkeletalMesh to create the track.
		if( SourceTrackIdx == INDEX_NONE )
		{
			FName TrackName = DestAnimSet->TrackBoneNames(TrackIdx);
			INT PatchBoneIndex = FillInMesh->MatchRefBone(TrackName);
			check(PatchBoneIndex != INDEX_NONE); // Should have checked for this case above!

			// Create 1-frame animation from the reference pose of the skeletal mesh.
			// This is basically what the compression does, so should be fine.
			if( DestAnimSeq->bIsAdditive )
			{
				RawTrack.PosKeys.AddItem(FVector(0.f));

				FQuat RefOrientation = FQuat::Identity;
				// To emulate ActorX-exported animation quat-flipping, we do it here.
				if( PatchBoneIndex > 0 )
				{
					RefOrientation.W *= -1.f;
				}
				RawTrack.RotKeys.AddItem(RefOrientation);

				// Extend AdditiveBasePose
				const FMeshBone& RefSkelBone = FillInMesh->RefSkeleton(PatchBoneIndex);

				FBoneAtom RefBoneAtom(RefSkelBone.BonePos.Orientation, RefSkelBone.BonePos.Position);
				if( PatchBoneIndex > 0)
				{
					RefBoneAtom.FlipSignOfRotationW(); // As above - flip if necessary
				}

				// Save off RefPose into destination AnimSequence
				DestAnimSeq->AdditiveBasePose(TrackIdx).PosKeys.AddItem(RefBoneAtom.GetTranslation());
				DestAnimSeq->AdditiveBasePose(TrackIdx).RotKeys.AddItem(RefBoneAtom.GetRotation());
			}
			else
			{
				const FVector RefPosition = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Position;
				RawTrack.PosKeys.AddItem(RefPosition);

				FQuat RefOrientation = FillInMesh->RefSkeleton(PatchBoneIndex).BonePos.Orientation;
				// To emulate ActorX-exported animation quat-flipping, we do it here.
				if( PatchBoneIndex > 0 )
				{
					RefOrientation.W *= -1.f;
				}
				RawTrack.RotKeys.AddItem(RefOrientation);
			}
		}
		else
		{
			check(SourceTrackIdx >= 0 && SourceTrackIdx < SourceAnimSet->TrackBoneNames.Num());

			INT SrcTrackIndex = SourceAnimSet->FindTrackWithName(DestAnimSet->TrackBoneNames(TrackIdx));
			if( SrcTrackIndex != INDEX_NONE )
			{
				// Direct copy
				DestAnimSeq->RawAnimationData(TrackIdx) = SourceAnimSeq->RawAnimationData(SrcTrackIndex);

				// Remap the additive ref pose
				if( DestAnimSeq->bIsAdditive )
				{
					DestAnimSeq->AdditiveBasePose(TrackIdx) = SourceAnimSeq->AdditiveBasePose(SrcTrackIndex);
				}
			}
			else
			{
				appMsgf(AMT_OK, *FString::Printf(TEXT("Couldn't find track in source animset. %s"), *DestAnimSet->TrackBoneNames(TrackIdx).ToString()) );
				return FALSE;
			}
		}
	}

	// See if SourceAnimSeq had a compression Scheme
	FAnimationUtils::CompressAnimSequence(DestAnimSeq, NULL, FALSE, FALSE);

	// If we need to, flush the LinkupCache.
	if( bAnimSetTrackChanged )
	{
		DestAnimSet->LinkupCache.Empty();
		DestAnimSet->SkelMesh2LinkupCache.Empty();

		// We need to re-init any skeletal mesh components now, because they might still have references to linkups in this set.
		for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if( !SkelComp->IsPendingKill() && !SkelComp->IsTemplate() )
			{
				SkelComp->InitAnimTree();
			}
		}

		// Recompress any sequences that need it.
		for( INT SequenceIndex = 0 ; SequenceIndex < SequencesRequiringRecompression.Num() ; ++SequenceIndex )
		{
			UAnimSequence* AnimSeq = SequencesRequiringRecompression( SequenceIndex );
			FAnimationUtils::CompressAnimSequence(AnimSeq, NULL, FALSE, FALSE);
		}
	}

	return TRUE;
}

void WxAnimSetViewer::MakeSelectedSequencesAdditive()
{
	// Get the list of selected animations
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);

	// Build list of selected AnimSequences
	TArray<class UAnimSequence*> Sequences;
	FString SeqListString = TEXT(" ");
	for (UINT i=0; i<Selection.Count(); i++)
	{
		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
		if( AnimSeq )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
			SeqListString = FString::Printf( TEXT("%s\n %s"), *SeqListString, *AnimSeq->SequenceName.ToString());
		}
	}

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		return;
	}

	// Now go through all sequences and convert them!
	INT LastIndex = INDEX_NONE;
	UBOOL bAskAboutReplacing = TRUE;
	UBOOL bDoReplace = FALSE;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			FString NewNameString = FString::Printf(TEXT("ADD_%s"), *AnimSeq->SequenceName.ToString());

			// See if this sequence already exists in destination
			UAnimSequence* DestSeq = SelectedAnimSet->FindAnimSequence(FName(*NewNameString));
			UBOOL bDestinationWasCreated = (DestSeq != NULL);
			// If not, create new one now.
			if( DestSeq )
			{
				if (bAskAboutReplacing)
				{
					// if it's different animset, ask if they'd like to replace
					UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Prompt_25"), *NewNameString));
					bDoReplace = MsgResult == ART_Yes || MsgResult == ART_YesAll;
					bAskAboutReplacing = MsgResult == ART_No || MsgResult == ART_Yes;
				}

				if( !bDoReplace )
				{
					continue; // Move on to next sequence...
				}
			}

			// Popup dialog to pick build method and base animation.
			WxConvertToAdditiveDialog ConvertToAdditiveDialog(this, this);
			if( ConvertToAdditiveDialog.ShowModal() != wxID_OK )
			{
				return;
			}

			// Gather results
			UAnimSequence* RefAnimSeq = NULL;
			EConvertToAdditive BuildMethod = ConvertToAdditiveDialog.GetBuildMethod();
			if( BuildMethod != CTA_RefPose )
			{
				RefAnimSeq = ConvertToAdditiveDialog.GetSelectedAnimation();
				if( RefAnimSeq == NULL )
				{
					return; // Abort if animation could not be selected
				}
			}

			if ( !DestSeq )
			{
				DestSeq = ConstructObject<UAnimSequence>(UAnimSequence::StaticClass(), SelectedAnimSet);
				// Add AnimSequence to AnimSet
				SelectedAnimSet->Sequences.AddItem( DestSeq );
			}

			// Convert source UAnimSequence to an additive animation into destination UAnimSequence
			// Using RefAnimSeq as a base, according to BuildMethod.
			FAnimationUtils::ConvertAnimSeqToAdditive(AnimSeq, DestSeq, RefAnimSeq, SelectedSkelMesh, BuildMethod, ConvertToAdditiveDialog.GetLoopingAnim());
		}
	}

	// Refresh list
	UpdateAnimSeqList();
	// Mark Current package as dirty
	SelectedAnimSet->MarkPackageDirty();
}


/** 
 * Rebuild Selected Additive Animations.
 */
void WxAnimSetViewer::RebuildAdditiveAnimation()
{
	// Get the list of selected animations
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);

	// Build list of selected AnimSequences
	TArray<class UAnimSequence*> Sequences;
	FString SeqListString = TEXT(" ");
	for (UINT i=0; i<Selection.Count(); i++)
	{
		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
		// Make sure selected animations are additive.
		if( AnimSeq && AnimSeq->bIsAdditive )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
			SeqListString = FString::Printf( TEXT("%s\n %s"), *SeqListString, *AnimSeq->SequenceName.ToString());
		}
	}

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		appMsgf( AMT_OK, TEXT("No Additive Animations Selected") );
		return;
	}

	// Go through all selected additive animations, and rebuild them.
	TArray<AdditiveAnimRebuildInfo> AdditiveAnimRebuildList;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			FAnimationUtils::GetAdditiveAnimRebuildList(AnimSeq, AdditiveAnimRebuildList);
			FAnimationUtils::RebuildAdditiveAnimations(AdditiveAnimRebuildList);
		}
	}

	// Mark Current package as dirty
	SelectedAnimSet->MarkPackageDirty();
}

/**
 * Takes selected AnimSequences, pops up a list of additive animations to choose from, and adds it.
 */
void WxAnimSetViewer::AddAdditiveAnimationToSelectedSequence(UBOOL bPerformSubtraction)
{
	// Get the list of selected animations
	wxArrayInt Selection;
	AnimSeqList->GetSelections(Selection);

	// Build list of selected AnimSequences
	TArray<class UAnimSequence*> Sequences;
	FString SeqListString = TEXT(" ");
	for (UINT i=0; i<Selection.Count(); i++)
	{
		INT SelIndex = Selection.Item(i);

		UAnimSequence* AnimSeq = (UAnimSequence*)AnimSeqList->GetClientData(SelIndex);
		if( AnimSeq )
		{
			check( AnimSeq->GetOuter() == SelectedAnimSet );
			check( SelectedAnimSet->Sequences.ContainsItem(AnimSeq) );

			// Checks out, so add item...
			Sequences.AddItem(AnimSeq);
			SeqListString = FString::Printf( TEXT("%s\n %s"), *SeqListString, *AnimSeq->SequenceName.ToString());
		}
	}

	// If no sequences found, just abort
	if( Sequences.Num() == 0 )
	{
		appMsgf( AMT_OK, TEXT("No Animations Selected") );
		return;
	}

	// Now go through all sequences
	INT LastIndex = INDEX_NONE;
	UBOOL bAskAboutReplacing = TRUE;
	UBOOL bDoReplace = FALSE;
	for(INT i=0; i<Sequences.Num(); i++)
	{
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{

			// Pop up dialog to pick additive animation to add, and destination animation name.
			WxAddAdditiveAnimDialog PickAdditiveDialog(*AnimSeq->SequenceName.ToString(), bPerformSubtraction ? *LocalizeUnrealEd("AAD_SubtractAdditiveAnimation") : *LocalizeUnrealEd("AAD_AddAdditiveAnimation"), this, this);
			if( PickAdditiveDialog.ShowModal() != wxID_OK )
			{
				// Abort if OK is not selected
				return;
			}

			FName DestinationName = PickAdditiveDialog.GetDestinationAnimationName();
			UAnimSequence* AdditiveAnimSeq = PickAdditiveDialog.GetSelectedAnimation();

			// Just making sure we have an additive animation to add.
			if( AdditiveAnimSeq == NULL )
			{
				appMsgf( AMT_OK, TEXT("No Additive Animation Selected") );
				return;
			}

			// See if this sequence already exists in destination
			UAnimSequence* DestSeq = SelectedAnimSet->FindAnimSequence(DestinationName);
			UBOOL bDestinationWasCreated = (DestSeq != NULL);

			// If not, create new one now.
			if( !DestSeq )
			{
				DestSeq = ConstructObject<UAnimSequence>(UAnimSequence::StaticClass(), SelectedAnimSet);
				// Add AnimSequence to AnimSet
				SelectedAnimSet->Sequences.AddItem( DestSeq );
			}
			else
			{
				if (bAskAboutReplacing)
				{
					// if it's different animset, ask if they'd like to replace
					UINT MsgResult = appMsgf(AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeUnrealEd("Prompt_25"), *DestinationName.ToString()));
					bDoReplace = MsgResult == ART_Yes || MsgResult == ART_YesAll;
					bAskAboutReplacing = MsgResult == ART_No || MsgResult == ART_Yes;
				}

				if( !bDoReplace )
				{
					continue; // Move on to next sequence...
				}
			}


			// Add additive animation
			if( !AddAdditiveAnimation(AnimSeq, AdditiveAnimSeq, DestSeq, SelectedSkelMesh, DestinationName, PickAdditiveDialog.GetBuildMethod(), bPerformSubtraction) )
			{
				// If destination AnimSequence was just created, delete it. Don't let zombie sequences lie in package.
				if( bDestinationWasCreated )
				{
					SelectedAnimSet->RemoveAnimSequenceFromAnimSet(DestSeq);
				}
				return;
			}
		}
	}

	// Refresh list
	UpdateAnimSeqList();
	// Mark Current package as dirty
	SelectedAnimSet->MarkPackageDirty();
}

UBOOL WxAnimSetViewer::AddAdditiveAnimation
(
	UAnimSequence* SourceAnimSeq,
	UAnimSequence* AdditiveAnimSeq,
	UAnimSequence* DestAnimSeq, 
	USkeletalMesh* SkelMesh, 
	FName AnimationName,
	EAddAdditive BuildMethod,
	UBOOL bPerformSubtraction
)
{
	// Make sure all anim sequences belong to the same AnimSet, this makes our life easier as they share the same track mapping.
	check(SourceAnimSeq->GetOuter() == DestAnimSeq->GetOuter());
	check(SourceAnimSeq->GetOuter() == AdditiveAnimSeq->GetOuter());
	
	// Make sure the additive animation is additive. :)
	check( AdditiveAnimSeq->bIsAdditive );

	// AnimSet those animsequences belong to
	UAnimSet* AnimSet = SourceAnimSeq->GetAnimSet();

	// If we're not overwriting source, we need to do some clean up and set up.
	if( DestAnimSeq != SourceAnimSeq )
	{
		// Make sure destination is setup correctly
		DestAnimSeq->RecycleAnimSequence();

		// Copy properties of Source into Dest.
		UAnimSequence::CopyAnimSequenceProperties(SourceAnimSeq, DestAnimSeq);

		// Dest won't have anything to do with Source anymore, so clear all additive animation references.
		DestAnimSeq->ClearAdditiveAnimReferences();

		// New name
		DestAnimSeq->SequenceName = AnimationName;

		// Copy animation data from source
		DestAnimSeq->RawAnimationData = SourceAnimSeq->RawAnimationData;
		// Copy additive base pose if source animation is additive	
		if( SourceAnimSeq->bIsAdditive )
		{
			DestAnimSeq->AdditiveBasePose = SourceAnimSeq->AdditiveBasePose;
		}
	}

	// Verify that number of tracks are matching.
	check( (AnimSet->TrackBoneNames.Num() == SourceAnimSeq->RawAnimationData.Num()) && (AnimSet->TrackBoneNames.Num() == DestAnimSeq->RawAnimationData.Num()) );
	check( AnimSet->TrackBoneNames.Num() == AdditiveAnimSeq->RawAnimationData.Num() );

	// If destination is a single frame, but additive is not, extend Destination to match length of Additive.
	if( DestAnimSeq->NumFrames == 1 && AdditiveAnimSeq->NumFrames > 1 )
	{
		DestAnimSeq->NumFrames = AdditiveAnimSeq->NumFrames;
		DestAnimSeq->SequenceLength = AdditiveAnimSeq->SequenceLength;
		for(INT TrackIdx=0; TrackIdx<AnimSet->TrackBoneNames.Num(); TrackIdx++)
		{
			FRawAnimSequenceTrack& DestRawTrack = DestAnimSeq->RawAnimationData(TrackIdx);
			FRawAnimSequenceTrack& AdditiveRawTrack = AdditiveAnimSeq->RawAnimationData(TrackIdx);
			// We extend destination's position and rotation keys below to match additive.
			// When doing the actual animation addition. So no need to do it here.
		}
	}

	// If we have to scale source to additive, then we need to add some keyframes to destination
	if( BuildMethod == EAA_ScaleSourceToAdditive 
		// Only do it if Destination has less frames than additive. We automatically scale additive to match source.
		&& DestAnimSeq->NumFrames < AdditiveAnimSeq->NumFrames )
	{
		const INT MaxNumKeys = AdditiveAnimSeq->NumFrames;
		FBoneAtom BoneAtom = FBoneAtom::Identity;

		// Got through all tracks in Destination. And scale all keys to additive animation length.
		for(INT TrackIdx=0; TrackIdx<AnimSet->TrackBoneNames.Num(); TrackIdx++)
		{
			FRawAnimSequenceTrack& AdditiveRawTrack = AdditiveAnimSeq->RawAnimationData(TrackIdx);
			FRawAnimSequenceTrack NewDestRawTrack;

			NewDestRawTrack.PosKeys.Add(MaxNumKeys);
			NewDestRawTrack.RotKeys.Add(MaxNumKeys);

			for(INT KeyIdx=0; KeyIdx<MaxNumKeys; KeyIdx++)
			{
				AdditiveAnimSeq->GetBoneAtom(BoneAtom, TrackIdx, ((FLOAT)KeyIdx * AdditiveAnimSeq->SequenceLength) / (FLOAT)(MaxNumKeys-1), FALSE, TRUE);
				NewDestRawTrack.PosKeys(KeyIdx) = BoneAtom.GetTranslation();
				NewDestRawTrack.RotKeys(KeyIdx) = BoneAtom.GetRotation();
			}

			// Copy new track
			DestAnimSeq->RawAnimationData(TrackIdx) = NewDestRawTrack;
		}

		DestAnimSeq->NumFrames = AdditiveAnimSeq->NumFrames;
		DestAnimSeq->SequenceLength = AdditiveAnimSeq->SequenceLength;
	}

	// Now add additive animation to destination.
	for(INT TrackIdx=0; TrackIdx<AnimSet->TrackBoneNames.Num(); TrackIdx++)
	{
		// Figure out which bone this track is mapped to
		const INT BoneIndex = SkelMesh->MatchRefBone(AnimSet->TrackBoneNames(TrackIdx));

		FRawAnimSequenceTrack& DestRawTrack = DestAnimSeq->RawAnimationData(TrackIdx);
		FRawAnimSequenceTrack& AdditiveRawTrack = AdditiveAnimSeq->RawAnimationData(TrackIdx);

		FBoneAtom AdditiveBoneAtom = FBoneAtom::Identity;

		// Here we work from destination's frames, and scale additive animation to that.
		// If we need to do it differently, earlier, we scale modify source data.
		const INT MaxNumKeys = DestAnimSeq->NumFrames;

		if( MaxNumKeys > 1 )
		{
			// if we have animated additive animation, but not base animation, then we need to add more keys there to match.
			// Because with the addition of both, we'll now have something animated.

			// Do translation here
			if( DestRawTrack.PosKeys.Num() == 1 )
			{
				DestRawTrack.PosKeys.Add(MaxNumKeys-1);
				for(INT KeyIdx=1; KeyIdx<MaxNumKeys; KeyIdx++)
				{
					DestRawTrack.PosKeys(KeyIdx) = DestRawTrack.PosKeys(0);
				}
			}

			// And rotation now
			if( DestRawTrack.RotKeys.Num() == 1 )
			{
				DestRawTrack.RotKeys.Add(MaxNumKeys-1);
				for(INT KeyIdx=1; KeyIdx<MaxNumKeys; KeyIdx++)
				{
					DestRawTrack.RotKeys(KeyIdx) = DestRawTrack.RotKeys(0);
				}
			}
		}

		// Go through all keys, and perform addition.
		for(INT KeyIdx=0; KeyIdx<MaxNumKeys; KeyIdx++)
		{
			// If we have the same number of keys, we can directly use those
			if( DestAnimSeq->NumFrames == AdditiveAnimSeq->NumFrames )
			{
				AdditiveBoneAtom.SetTranslation( AdditiveRawTrack.PosKeys( KeyIdx < AdditiveRawTrack.PosKeys.Num() ? KeyIdx : 0 ) );
				AdditiveBoneAtom.SetRotation( AdditiveRawTrack.RotKeys( KeyIdx < AdditiveRawTrack.RotKeys.Num() ? KeyIdx : 0 ) );
			}
			// Otherwise we have to scale the animation
			else
			{
				AdditiveAnimSeq->GetBoneAtom(AdditiveBoneAtom, TrackIdx, ((FLOAT)KeyIdx * AdditiveAnimSeq->SequenceLength) / (FLOAT)(MaxNumKeys-1), FALSE, TRUE);
			}

			// Invert for subtraction
			if( bPerformSubtraction )
			{
				AdditiveBoneAtom.SetTranslation(-AdditiveBoneAtom.GetTranslation());
				AdditiveBoneAtom.SetRotation(-AdditiveBoneAtom.GetRotation());
			}

			// Translation
			DestRawTrack.PosKeys(KeyIdx) += AdditiveBoneAtom.GetTranslation();

			// Rotation

			// For rotation part. We have this annoying thing to work around...
			// See UAnimNodeSequence::GetAnimationPose()
			// Addition with "quaternion fix for ActorX exported quaternions". Then revert back.
			if( BoneIndex > 0 || BoneIndex == INDEX_NONE )
			{
				DestRawTrack.RotKeys(KeyIdx).W *= -1.f;
				AdditiveBoneAtom.FlipSignOfRotationW();
			}

			// Addition.
			DestRawTrack.RotKeys(KeyIdx) = AdditiveBoneAtom.GetRotation() * DestRawTrack.RotKeys(KeyIdx);
				
			// Convert back to non "quaternion fix for ActorX exported quaternions".
			if( BoneIndex > 0 || BoneIndex == INDEX_NONE )
			{
				DestRawTrack.RotKeys(KeyIdx).W *= -1.f;
				AdditiveBoneAtom.FlipSignOfRotationW();
			}

			// Normalize resulting quaternion.
			DestRawTrack.RotKeys(KeyIdx).Normalize();
		}
	}

	// Compress Raw Anim Data.
	DestAnimSeq->CompressRawAnimData();

	// See if SourceAnimSeq had a compression Scheme
	FAnimationUtils::CompressAnimSequence(DestAnimSeq, NULL, FALSE, FALSE);

	return TRUE;
}

/**
 * Pop up a dialog asking for axis and angle (in debgrees), and apply that rotation to all keys in selected sequence.
 * Basically 
 */
void WxAnimSetViewer::SequenceApplyRotation()
{
	if(SelectedAnimSeq)
	{
		WxDlgRotateAnimSeq dlg;
		INT Result = dlg.ShowModal();
		if( Result == wxID_OK )
		{
			// Angle (in radians) to rotate AnimSequence by
			FLOAT RotAng = dlg.Degrees * (PI/180.f);

			// Axis to rotate AnimSequence about.
			FVector RotAxis;
			if( dlg.Axis == AXIS_X )
			{
				RotAxis = FVector(1.f, 0.f, 0.f);
			}
			else if( dlg.Axis == AXIS_Y )
			{
				RotAxis = FVector(0.f, 1.f, 0.0f);
			}
			else if( dlg.Axis == AXIS_Z )
			{
				RotAxis = FVector(0.f, 0.f, 1.0f);
			}
			else
			{
				check(0);
			}

			// Make transformation matrix out of rotation (via quaternion)
			const FQuat RotQuat( RotAxis, RotAng );

			// Hmm.. animations don't have any idea of hierarchy, so we just rotate the first track. Can we be sure track 0 is the root bone?
			FRawAnimSequenceTrack& RawTrack = SelectedAnimSeq->RawAnimationData(0);
			for( INT r = 0 ; r < RawTrack.RotKeys.Num() ; ++r )
			{
				FQuat& Quat = RawTrack.RotKeys(r);
				Quat = RotQuat * Quat;
				Quat.Normalize();
			}
			for( INT p = 0 ; p < RawTrack.PosKeys.Num() ; ++p)
			{
				RawTrack.PosKeys(p) = RotQuat.RotateVector(RawTrack.PosKeys(p));
			}

			RecompressSequence(SelectedAnimSeq, SelectedSkelMesh);

			SelectedAnimSeq->MarkPackageDirty();
		}
	}
}

void WxAnimSetViewer::SequenceReZeroToCurrent()
{
	if( SelectedAnimSeq )
	{
		// Find vector that would translate current root bone location onto origin.
		FVector ApplyTranslation = -1.f * PreviewSkelComp->SpaceBases(0).GetOrigin();

		// Convert into world space and eliminate 'z' translation. Don't want to move character into ground.
		FVector WorldApplyTranslation = PreviewSkelComp->LocalToWorld.TransformNormal(ApplyTranslation);
		WorldApplyTranslation.Z = 0.f;
		ApplyTranslation = PreviewSkelComp->LocalToWorld.InverseTransformNormal(WorldApplyTranslation);

		// As above, animations don't have any idea of hierarchy, so we don't know for sure if track 0 is the root bone's track.
		FRawAnimSequenceTrack& RawTrack = SelectedAnimSeq->RawAnimationData(0);
		for(INT i=0; i<RawTrack.PosKeys.Num(); i++)
		{
			RawTrack.PosKeys(i) += ApplyTranslation;
		}

		RecompressSequence(SelectedAnimSeq, SelectedSkelMesh);
		SelectedAnimSeq->MarkPackageDirty();
	}
}

/**
 * Crop a sequence either before or after the current position. This is made slightly more complicated due to the basic compression
 * we do where tracks which had all identical key frames are reduced to just 1 frame.
 * 
 * @param bFromStart Should we remove the sequence before or after the selected position.
 */
void WxAnimSetViewer::SequenceCrop(UBOOL bFromStart)
{
	if(SelectedAnimSeq)
	{
		// Can't crop cooked animations.
		const UPackage* SeqPackage = SelectedAnimSeq->GetOutermost();
		if( SeqPackage->PackageFlags & PKG_Cooked )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
			return;
		}

		// Crop the raw anim data.
		SelectedAnimSeq->CropRawAnimData( PreviewAnimNode->CurrentTime, bFromStart );
		// Recompress animation from Raw.
		FAnimationUtils::CompressAnimSequence(SelectedAnimSeq, NULL, FALSE, FALSE);

		if(bFromStart)
		{
			PreviewAnimNode->CurrentTime = 0.f;
		}
		else
		{		
			PreviewAnimNode->CurrentTime = SelectedAnimSeq->SequenceLength;
		}

		UpdateAnimSeqList();
		SetSelectedAnimSequence(SelectedAnimSeq);
	}
}

/** Tool for updating the bounding sphere/box of the selected skeletal mesh. Shouldn't generally be needed, except for fixing stuff up possibly. */
void WxAnimSetViewer::UpdateMeshBounds()
{
	if(SelectedSkelMesh)
	{
		FBox BoundingBox(0);
		check(SelectedSkelMesh->LODModels.Num() > 0);
		FStaticLODModel& LODModel = SelectedSkelMesh->LODModels(0);

		for(INT ChunkIndex = 0;ChunkIndex < LODModel.Chunks.Num();ChunkIndex++)
		{
			const FSkelMeshChunk& Chunk = LODModel.Chunks(ChunkIndex);
			for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
			{
				BoundingBox += Chunk.RigidVertices(i).Position;
			}
			for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
			{
				BoundingBox += Chunk.SoftVertices(i).Position;
			}
		}

		const FBox Temp			= BoundingBox;
		const FVector MidMesh	= 0.5f*(Temp.Min + Temp.Max);
		BoundingBox.Min			= Temp.Min + 1.0f*(Temp.Min - MidMesh);
		BoundingBox.Max			= Temp.Max + 1.0f*(Temp.Max - MidMesh);

		// Tuck up the bottom as this rarely extends lower than a reference pose's (e.g. having its feet on the floor).
		BoundingBox.Min.Z	= Temp.Min.Z + 0.1f*(Temp.Min.Z - MidMesh.Z);
		SelectedSkelMesh->Bounds = FBoxSphereBounds(BoundingBox);

		SelectedSkelMesh->MarkPackageDirty();
	}
}

/** Update the external force on the cloth sim. */
void WxAnimSetViewer::UpdateClothWind()
{
	const FVector WindDir = WindRot.Vector();
	PreviewSkelComp->ClothWind = WindDir * WindStrength;
	// Wind Parameters for ApexClothing
#if WITH_APEX
	PreviewSkelComp->WindVelocity = WindDir * WindStrength;
	PreviewSkelComp->WindVelocityBlendTime = WindVelocityBlendTime;
#endif
}

/**
 * Refreshes the chunk preview toolbar combo box w/ a listing of chunks belonging to the current LOD
 */
void WxAnimSetViewer::UpdateChunkPreview()
{
	ToolBar->ChunkComboBox->Clear();
	ToolBar->ChunkComboBox->Append(*LocalizeUnrealEd("AnimSetViewer_ChunksAll"));

	INT LodModel = 0;
	INT ForcedLodModel = PreviewSkelComp->ForcedLodModel;
	if (ForcedLodModel > 0)
	{
		LodModel = Clamp( ForcedLodModel - 1, 0, PreviewSkelComp->SkeletalMesh->LODModels.Num() - 1 );
	}

	INT NumChunks = SelectedSkelMesh->LODModels( LodModel ).Chunks.Num();
	FString ChunkNum;
	for (INT ChunkIdx = 0; ChunkIdx < NumChunks; ++ChunkIdx)
	{
		ChunkNum = FString::Printf(TEXT("%d"), ChunkIdx);
		ToolBar->ChunkComboBox->Append(*ChunkNum);
	}
	ToolBar->ChunkComboBox->SetSelection(0);
	PreviewChunk(-1);
}

/**
 * Sets the index of the chunk to preview
 */
void WxAnimSetViewer::PreviewChunk(INT ChunkIdx)
{
	PreviewSkelComp->ChunkIndexPreview = ChunkIdx;
	PreviewSkelComp->BeginDeferredReattach(); // Force regeneration of the render proxy
}

/**
 * Refreshes the section preview toolbar combo box w/ a listing of sections belonging to the current LOD
 */
void WxAnimSetViewer::UpdateSectionPreview()
{
	ToolBar->SectionComboBox->Clear();
	ToolBar->SectionComboBox->Append(*LocalizeUnrealEd("AnimSetViewer_SectionsAll"));

	INT LodModel = 0;
	INT ForcedLodModel = PreviewSkelComp->ForcedLodModel;
	if (ForcedLodModel > 0)
	{
		LodModel = Clamp( ForcedLodModel - 1, 0, PreviewSkelComp->SkeletalMesh->LODModels.Num() - 1 );
	}

	INT NumSections = SelectedSkelMesh->LODModels( LodModel ).Sections.Num();
	FString SectionNum;
	for (INT SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
	{
		SectionNum = FString::Printf(TEXT("%d"), SectionIdx);
		ToolBar->SectionComboBox->Append(*SectionNum);
	}
	ToolBar->SectionComboBox->SetSelection(0);
	PreviewSection(-1);
}

/**
 * Sets the index of the section to preview
 */
void WxAnimSetViewer::PreviewSection(INT SectionIdx)
{
	PreviewSkelComp->SectionIndexPreview = SectionIdx;
	PreviewSkelComp->BeginDeferredReattach(); // Force regeneration of the render proxy
}

// Skeleton Tree Manager

/** Posts an event for regenerating the Skeleton Tree control. */
void WxAnimSetViewer::FillSkeletonTree()
{
	wxCommandEvent Event;
	Event.SetEventObject(this);
	Event.SetEventType(IDM_ANIMSET_FILLSKELETONTREE);
	GetEventHandler()->AddPendingEvent(Event);
}
																									   
static wxTreeItemId AnimSetViewer_CreateTreeItemAndParents(INT BoneIndex, TArray<FMeshBone>& Skeleton, const TArray<BYTE>& RequiredBones, wxTreeCtrl* TreeCtrl, TMap<FName,wxTreeItemId>& BoneTreeItemMap)
{
	wxTreeItemId* existingItem = BoneTreeItemMap.Find( Skeleton(BoneIndex).Name );
	if( existingItem != NULL )
	{
		return *existingItem;
	}

	FMeshBone& Bone = Skeleton(BoneIndex);
	wxTreeItemId newItem;
	if( BoneIndex == 0 )
	{
		newItem = TreeCtrl->AddRoot( *FString::Printf(TEXT("%s (%d)"), *Bone.Name.ToString(), BoneIndex) );
	}
	else
	{						  
		wxTreeItemId parentItem = AnimSetViewer_CreateTreeItemAndParents(Bone.ParentIndex, Skeleton, RequiredBones, TreeCtrl, BoneTreeItemMap);
		newItem = TreeCtrl->AppendItem( parentItem, *FString::Printf(TEXT("%s (%d)"), *Bone.Name.ToString(), BoneIndex) );
		TreeCtrl->Expand( parentItem );
	}   

	// Set the bone name bold if in use
	TreeCtrl->SetItemBold(newItem, RequiredBones.ContainsItem(BoneIndex) ? true : false);

	BoneTreeItemMap.Set(Bone.Name, newItem);

	return newItem;
}

/**
 * Event handler for regenerating the tree view data.
 *
 * @param	In	Information about the event.
 */
void WxAnimSetViewer::OnFillSkeletonTree(wxCommandEvent &In)
{
	// We don't have a skeletal mesh, skip update.
	if( !SelectedSkelMesh )
	{
		return;
	}

	SkeletonTreeCtrl->Freeze();
	{
		SkeletonTreeCtrl->DeleteAllItems();

		SkeletonTreeItemBoneIndexMap.Empty();

		TMap<FName,wxTreeItemId>	BoneTreeItemMap;

		TArray<BYTE> RequiredBones;
		if (PreviewSkelComp && SelectedSkelMesh)
		{
			if (SelectedSkelMesh->LODModels.IsValidIndex(PreviewSkelComp->PredictedLODLevel))
			{
				const FStaticLODModel& LODModel = SelectedSkelMesh->LODModels(PreviewSkelComp->PredictedLODLevel);
				const FSkelMeshComponentLODInfo& LODInfo = PreviewSkelComp->LODInfo(PreviewSkelComp->PredictedLODLevel);
				if (bPreviewInstanceWeights && LODInfo.InstanceWeightUsage == IWU_FullSwap && LODModel.VertexInfluences.Num() > 0)
				{
					RequiredBones = LODModel.VertexInfluences(0).RequiredBones;
				}
				else
				{
					RequiredBones = LODModel.RequiredBones;
				}
			}
		}

		// Fill Tree with all bones...
		for(INT BoneIndex=0; BoneIndex<SelectedSkelMesh->RefSkeleton.Num(); BoneIndex++)
		{
			wxTreeItemId newItem = AnimSetViewer_CreateTreeItemAndParents(BoneIndex, SelectedSkelMesh->RefSkeleton, RequiredBones, SkeletonTreeCtrl, BoneTreeItemMap);
			
			// Mapping between bone index and tree item.
			SkeletonTreeItemBoneIndexMap.Set(newItem, BoneIndex);
		}
	}
	SkeletonTreeCtrl->Thaw();
}

void WxAnimSetViewer::OnSkeletonTreeItemRightClick(wxTreeEvent& In)
{
	// Pop up menu
	WxSkeletonTreePopUpMenu Menu( this );
	FTrackPopupMenu TrackMenu( this, &Menu );
	TrackMenu.Show();
}

/*
*  Update the bones of interest in the viewer for blendweight viewing/bone manipulation
*  @param NewBoneIndex - bone clicked on in the tree view (since GetSelections isn't always accurate at time of calling)
*/
void WxAnimSetViewer::UpdateBoneManipulationControl(INT NewBoneIndex)
{
	if( PreviewSkelComp == NULL)
	{
		return;
	}

	//Empty out the last stuff (clear selected socket/last bone selected)
	SetSelectedSocket(NULL);
	PreviewSkelComp->BonesOfInterest.Empty();

	//Zero out the skel control since we're switching bones
	FSkelControlListHead& SkelControlList = PreviewAnimTree->SkelControlLists(0);
	USkelControlSingleBone* SkelControl = Cast<USkelControlSingleBone>(SkelControlList.ControlHead);
	SkelControl->BoneRotation = FRotator(0,0,0);
	SkelControl->BoneTranslation = FVector::ZeroVector;

	//@HACK Add the contribution from the event system (since GetSelections isn't always accurate)
	if (NewBoneIndex != INDEX_NONE)
	{
		PreviewSkelComp->BonesOfInterest.AddItem(NewBoneIndex);
	}

	wxArrayTreeItemIds SelectionSet;
	INT NumSelections = SkeletonTreeCtrl->GetSelections(SelectionSet);

	// Build a list of selected bones.
	for(INT SelectionIdx=0; SelectionIdx<NumSelections; SelectionIdx++)
	{
		wxTreeItemId SelectionItemId = SelectionSet[SelectionIdx];

		const INT* BoneIdxPtr = SkeletonTreeItemBoneIndexMap.Find( SelectionItemId );
		PreviewSkelComp->BonesOfInterest.AddUniqueItem(*BoneIdxPtr);
	}

	//Only handle bone manipulation one at a time
	if (PreviewSkelComp->BonesOfInterest.Num() == 1)
	{
		INT BoneIndex = PreviewSkelComp->BonesOfInterest(0);
		if(PreviewSkelComp->SkeletalMesh && (BoneIndex >= 0) && (BoneIndex < PreviewSkelComp->SkeletalMesh->RefSkeleton.Num()))
		{
			//Update who this skel control is operating on
			SkelControlList.BoneName = SelectedSkelMesh->RefSkeleton(BoneIndex).Name;
		}
	}

	if (PreviewSkelComp->ColorRenderMode==ESCRM_BoneWeights)
	{
		//Reattaching transfers the bone index info to the render thread
		FComponentReattachContext PreviewSkelReattach(PreviewSkelComp);
	}

	//Always invalidate the anim tree and viewport
	PreviewSkelComp->InitSkelControls();
	PreviewWindow->ASVPreviewVC->Invalidate();
}

/*
 * Get a list of vertex indices that reference equivalent vertex positions in the skeletal mesh
 * @param LODIdx - mesh LOD to compare against
 * @param InfluenceIdx - influence track to compare against
 * @param VertCheckIndex - vertex index to check against
 * @param EquivalentVertices - array of vertex indices to fill in
 * @return Count of vertices found to be equivalent
 */
INT WxAnimSetViewer::GetEquivalentVertices(INT LODIdx, INT InfluenceIdx, INT VertCheckIndex, TArray<INT>& EquivalentVertices)
{
	EquivalentVertices.Empty();

	//Iterate over all vertices in the mesh looking for equivalent vertex positions
	if (PreviewSkelComp->SkeletalMesh->LODModels.IsValidIndex(LODIdx))
	{
		FStaticLODModel& LODModel = PreviewSkelComp->SkeletalMesh->LODModels(LODIdx);
		if (LODModel.VertexInfluences.IsValidIndex(InfluenceIdx))
		{
			const INT NumVertices = LODModel.VertexBufferGPUSkin.GetNumVertices();
			const FVector VertexToCheck = LODModel.VertexBufferGPUSkin.GetVertexPosition(VertCheckIndex);
			for (INT VertIdx=0; VertIdx<NumVertices; VertIdx++)
			{
				const FVector OtherVertex = LODModel.VertexBufferGPUSkin.GetVertexPosition(VertIdx);
				if (PointsEqual(VertexToCheck, OtherVertex) && VertCheckIndex != VertIdx)
				{
					EquivalentVertices.AddUniqueItem(VertIdx);
				}
			}
		}
	}

	return EquivalentVertices.Num();
}
									 
/*
 *   Calculate the bone to vert list mapping and clear out related data
 * @param SelectedSkelMesh - mesh of interest
 * @param BoneVertices - 2D array to hold the bone index -> vert list mapping
 */
void WxAnimSetViewer::InitBoneWeightInfluenceData(const USkeletalMeshComponent* SkelComp)
{
	// Grow/shrink tool array
	INT NumLODs = SkelComp->SkeletalMesh->LODModels.Num();
	if (NumLODs != BoneInfluenceLODInfo.Num())
	{
		BoneInfluenceLODInfo.Empty(NumLODs);
		BoneInfluenceLODInfo.AddZeroed(NumLODs);
	}

	// Initialize the tool data
	for (INT LODIdx=0; LODIdx<NumLODs; LODIdx++)
	{
		FLODInfluenceInfo& LODInfluenceInfo = BoneInfluenceLODInfo(LODIdx);
		//Setup the mapping of bones to the verts they influence
		FindBoneWeightsPerVertex(SkelComp, LODInfluenceInfo.BoneVertices);
		//Clear out the other related containers
		LODInfluenceInfo.InfluencedBoneVerts.Empty();
		LODInfluenceInfo.NonInfluencedBoneVerts.Empty();
	}
}

/*
 *   Update the state of a single vertex in the skeletal mesh
 * @param LODIdx - LOD this vertex applies to 
 * @param InfluenceIdx - Influence track to swap
 * @param VertIdx - Vertex Index to swap
 * @param bContributingInfluence - whether to swap in (TRUE) or swap back (FALSE)
 */
void WxAnimSetViewer::UpdateInfluenceWeight(INT LODIdx, INT InfluenceIdx, INT VertIndex, UBOOL bContributingInfluence)
{
	if (PreviewSkelComp->SkeletalMesh->LODModels.IsValidIndex(LODIdx))
	{
		FStaticLODModel& LODModel = PreviewSkelComp->SkeletalMesh->LODModels(LODIdx);
		FLODInfluenceInfo& LODInfluenceInfo = BoneInfluenceLODInfo(LODIdx); 
		if (LODModel.VertexInfluences.IsValidIndex(InfluenceIdx) && LODModel.VertexInfluences(InfluenceIdx).Usage == IWU_PartialSwap)
		{
			for (INT BoneIdx=0; BoneIdx < PreviewSkelComp->BonesOfInterest.Num(); BoneIdx++)
			{
				FBoneIndexPair BonePair;
				BonePair.BoneIdx[0] = PreviewSkelComp->BonesOfInterest(BoneIdx);
				BonePair.BoneIdx[1] = PreviewSkelComp->SkeletalMesh->RefSkeleton(BonePair.BoneIdx[0]).ParentIndex;
				if (bContributingInfluence)
				{
					ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
						RemoveInfluenceWeightCommand, 
						FSkeletalMeshVertexInfluences*, VertexInfluence, &LODModel.VertexInfluences(InfluenceIdx), 
						const FBoneIndexPair, BonePair, BonePair, 
						INT, VertIndex, VertIndex,
					{
						//Remove it
					
						TArray<DWORD>* VertList = VertexInfluence->VertexInfluenceMapping.Find(BonePair);
						if (VertList)
						{
							VertList->RemoveItem((DWORD)VertIndex);
							if (VertList->Num() <= 0)
							{	
								VertexInfluence->VertexInfluenceMapping.Remove(BonePair);
							}
						}
					});

					LODInfluenceInfo.InfluencedBoneVerts.RemoveKey(VertIndex);

					const TArray<INT>& AllVertsThisBone = LODInfluenceInfo.BoneVertices(BonePair.BoneIdx[0]);
					const TArray<INT>& AllVertsParentBone = LODInfluenceInfo.BoneVertices(BonePair.BoneIdx[1]);
					//Only add it to the NonInfluenced array if its part of this bone pair
					if (AllVertsParentBone.FindItemIndex(VertIndex) != INDEX_NONE || AllVertsThisBone.FindItemIndex(VertIndex) != INDEX_NONE)
					{
						LODInfluenceInfo.NonInfluencedBoneVerts.Add(VertIndex);
					}
				}
				else
				{	
					ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
						AddInfluenceWeightCommand, 
						FSkeletalMeshVertexInfluences*, VertexInfluence, &LODModel.VertexInfluences(InfluenceIdx), 
						const FBoneIndexPair, BonePair, BonePair, 
						INT, VertIndex, VertIndex,
					{
						//Add it
						TArray<DWORD>* VertList = VertexInfluence->VertexInfluenceMapping.Find(BonePair);
						if (VertList)
						{
							VertList->AddUniqueItem((DWORD)VertIndex);
						}
						else
						{
							TArray<DWORD> NewVertList;
							NewVertList.AddItem(VertIndex);
							VertexInfluence->VertexInfluenceMapping.Set(BonePair, NewVertList);
						}
					});

					LODInfluenceInfo.InfluencedBoneVerts.Add(VertIndex);
					LODInfluenceInfo.NonInfluencedBoneVerts.RemoveKey(VertIndex);
				}
			}

			//Sort the vertices for the draw call later (depends on ascending order)
			if (LODInfluenceInfo.InfluencedBoneVerts.Num() > 0)
			{
				LODInfluenceInfo.InfluencedBoneVerts.Sort<COMPARE_CONSTREF_CLASS(INT,SortINTSAscending)>();
			}

			//Sort the vertices for the draw call later (depends on ascending order)
			if (LODInfluenceInfo.NonInfluencedBoneVerts.Num() > 0)
			{
				LODInfluenceInfo.NonInfluencedBoneVerts.Sort<COMPARE_CONSTREF_CLASS(INT,SortINTSAscending)>();
			}

			PreviewSkelComp->BeginDeferredReattach();
		}
	}
}

/*
 * Update the bone influence weights on the skeletal mesh for bone pairs specified
 * @BonesOfInterest - array of bone indices to update
 */
void WxAnimSetViewer::UpdateInfluenceWeights(const TArray<INT>& BonesOfInterest)
{
	TArray<FBonePair> BonePairs;
	TArray<FBoneIndexPair> BoneIndexPairs;

	for (INT i=0; i<BonesOfInterest.Num(); i++)
	{
		const INT BoneIndex = BonesOfInterest(i);

		FBoneIndexPair* NewBoneIndexPair = new(BoneIndexPairs) FBoneIndexPair;
		NewBoneIndexPair->BoneIdx[0] = BoneIndex;
		NewBoneIndexPair->BoneIdx[1] = PreviewSkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;

		FBonePair* NewBonePair = new(BonePairs) FBonePair;
		NewBonePair->Bones[0] = PreviewSkelComp->SkeletalMesh->RefSkeleton(BoneIndex).Name;
		NewBonePair->Bones[1] = PreviewSkelComp->SkeletalMesh->RefSkeleton(NewBoneIndexPair->BoneIdx[1]).Name;
	}

	const INT NumLODModels = PreviewSkelComp->SkeletalMesh->LODModels.Num();
	for (INT LODIdx=0; LODIdx<NumLODModels; LODIdx++)
	{
		FLODInfluenceInfo& LODInfluenceInfo = BoneInfluenceLODInfo(LODIdx);

		LODInfluenceInfo.InfluencedBoneVerts.Empty();
		LODInfluenceInfo.NonInfluencedBoneVerts.Empty();
		if (BonePairs.Num() > 0)
		{
			// enable usage of instanced weights
			PreviewSkelComp->UpdateInstanceVertexWeightBones(BonePairs);

			// grab the values stored for display in the editor
			for (INT LODModelIdx = 0; LODModelIdx < NumLODModels; LODModelIdx++)
			{
				const FStaticLODModel& LODModel = PreviewSkelComp->SkeletalMesh->LODModels(LODModelIdx);
				if (LODModel.VertexInfluences.Num() > 0)
				{
					const FSkeletalMeshVertexInfluences& VertexInfluences = LODModel.VertexInfluences(0);
					if (VertexInfluences.Usage == IWU_FullSwap)
					{
						continue;
					}

					for (INT BoneIdx = 0; BoneIdx<BoneIndexPairs.Num(); BoneIdx++)
					{
						const FBoneIndexPair& BoneIndexPair = BoneIndexPairs(BoneIdx);

						//Get all the vertices weighted by this bone and remove the ones found above
						const TArray<INT>& AllVertsThisBone = LODInfluenceInfo.BoneVertices(BoneIndexPair.BoneIdx[0]);
						for (INT i=0; i<AllVertsThisBone.Num(); i++)
						{
							LODInfluenceInfo.NonInfluencedBoneVerts.Add(AllVertsThisBone(i));
						}

						const TArray<INT>& AllVertsParentBone = LODInfluenceInfo.BoneVertices(BoneIndexPair.BoneIdx[1]);
						for (INT i=0; i<AllVertsParentBone.Num(); i++)
						{
							LODInfluenceInfo.NonInfluencedBoneVerts.Add(AllVertsParentBone(i));
						}

						//Find all bones part of the influence swap
						
						const TArray<DWORD>* VertIndices = VertexInfluences.VertexInfluenceMapping.Find(BoneIndexPair); 
						if (VertIndices)
						{
							INT NumVertIndices = VertIndices->Num();
							for (INT i=0; i < NumVertIndices; i++)
							{
								LODInfluenceInfo.InfluencedBoneVerts.Add((*VertIndices)(i));
							}

							for (INT i=0; i < NumVertIndices; i++)
							{
								LODInfluenceInfo.NonInfluencedBoneVerts.RemoveKey((*VertIndices)(i));
							}
						}
					}
				}
			}

			//Sort the vertices for the draw call later (depends on ascending order)
			if (LODInfluenceInfo.InfluencedBoneVerts.Num() > 0)
			{
				LODInfluenceInfo.InfluencedBoneVerts.Sort<COMPARE_CONSTREF_CLASS(INT,SortINTSAscending)>();
			}

			//Sort the vertices for the draw call later (depends on ascending order)
			if (LODInfluenceInfo.NonInfluencedBoneVerts.Num() > 0)
			{
				LODInfluenceInfo.NonInfluencedBoneVerts.Sort<COMPARE_CONSTREF_CLASS(INT,SortINTSAscending)>();
			}
		}
	}
}

/*
 * Callback when the skeleton tree control items are selected/deselected
 * @param In - Event parameter data
 */
void WxAnimSetViewer::OnSkeletonTreeSelectionChange(wxTreeEvent& In)
{
	wxTreeItemId SelectionItemId = In.GetItem();
	wxTreeItemId OldSelectionItemId = In.GetOldItem();

	const INT* BoneIdxPtr = SkeletonTreeItemBoneIndexMap.Find( SelectionItemId );
	const INT* OldBoneIdxPtr = SkeletonTreeItemBoneIndexMap.Find( OldSelectionItemId );
	INT NewBoneIdx = INDEX_NONE;
	if (BoneIdxPtr)
	{
		NewBoneIdx = *BoneIdxPtr;
	}

   UpdateBoneManipulationControl(NewBoneIdx);

   if (PreviewSkelComp->bDrawBoneInfluences)
   {
	  UpdateInfluenceWeights(PreviewSkelComp->BonesOfInterest);
	  PreviewSkelComp->BeginDeferredReattach();
   }
}

void WxAnimSetViewer::OnSkeletonTreeMenuHandleCommand(wxCommandEvent &In)
{
	if( !SkeletonTreeCtrl || !SelectedSkelMesh )
	{
		return;
	}

	wxArrayTreeItemIds SelectionSet;
	INT NumSelections = SkeletonTreeCtrl->GetSelections(SelectionSet);
	if( NumSelections == 0 )
	{
		return;
	}

	// Id of selection in popup menu.
	INT Command = In.GetId();
	debugf(TEXT("OnSkeletonTreeMenuHandleCommand. Command: %d"), Command);

	TArray<BYTE> BoneSelectionMask;
	BoneSelectionMask.AddZeroed(SelectedSkelMesh->RefSkeleton.Num());

	//Empty the blend weight bones of interest
	PreviewSkelComp->BonesOfInterest.Empty();

	// Build a list of selected bones.
	debugf(TEXT("  BoneSelectionMask building."));
	for(INT SelectionIdx=0; SelectionIdx<NumSelections; SelectionIdx++)
	{
		wxTreeItemId SelectionItemId = SelectionSet[SelectionIdx];

		const INT* BoneIdxPtr = SkeletonTreeItemBoneIndexMap.Find( SelectionItemId );
		INT BoneIndex = *BoneIdxPtr;

		// Flag bone as affected
		BoneSelectionMask(BoneIndex) = 1;
		PreviewSkelComp->BonesOfInterest.AddItem(*BoneIdxPtr);
		debugf(TEXT("    Bone: %s flagged as selected."), *SelectedSkelMesh->RefSkeleton(BoneIndex).Name.ToString());
	}

	// If we select child bones as well, then add those to the selection mask.
	if( Command == IDM_ANIMSET_SKELETONTREE_SHOWCHILDBONE || Command == IDM_ANIMSET_SKELETONTREE_HIDECHILDBONE || Command == IDM_ANIMSET_SKELETONTREE_SETCHILDBONECOLOR )
	{
		debugf(TEXT("  Adding children to selection mask."));
		// Skip root bone, he can't be a child.
		for(INT BoneIndex=1; BoneIndex<BoneSelectionMask.Num(); BoneIndex++)
		{
			BoneSelectionMask(BoneIndex) |= BoneSelectionMask(SelectedSkelMesh->RefSkeleton(BoneIndex).ParentIndex);
		}
	}

	if ( Command == IDM_ANIMSET_SKELETONTREE_SHOWBLENDWEIGHTS)
	{
		// clear all about vertex info tool
		appMemzero(&VertexInfo, sizeof(VertexInfo));

		if ( In.IsChecked() )
		{
			VertexInfo.ColorOption = ESCRM_BoneWeights;
		}

		UpdateVertexMode();
	}
	else if ( Command == IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS || Command == IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_AUTODETECT ||
		Command == IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_RIGIDPREFERRED )
	{
		PreviewSkelComp->SkeletalMesh->PreEditChange(NULL);
		EBoneBreakOption Option = BONEBREAK_SoftPreferred;
		if (Command == IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_AUTODETECT)
		{
			Option = BONEBREAK_AutoDetect;
		}
		else if (Command == IDM_ANIMSET_SKELETONTREE_CALCULATEBONEBREAKS_RIGIDPREFERRED)
		{
			Option = BONEBREAK_RigidPreferred;
		}

		if (BeginTransaction(*LocalizeUnrealEd("CalculateBoneBreaks")))
		{
			PreviewSkelComp->SkeletalMesh->Modify();

			TArray<FBoneIndexPair> BoneIndexPairs;
			for (INT i=0; i<PreviewSkelComp->BonesOfInterest.Num(); i++)
			{
				const INT BoneIndex = PreviewSkelComp->BonesOfInterest(i);

				FBoneIndexPair* NewBoneIndexPair = new(BoneIndexPairs) FBoneIndexPair;
				NewBoneIndexPair->BoneIdx[0] = BoneIndex;
				NewBoneIndexPair->BoneIdx[1] = PreviewSkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;

				const FString& BoneName = PreviewSkelComp->SkeletalMesh->RefSkeleton(BoneIndex).Name.ToString();
				INT AddedIndex = PreviewSkelComp->SkeletalMesh->BoneBreakNames.AddUniqueItem(BoneName);
				if (PreviewSkelComp->SkeletalMesh->BoneBreakOptions.Num() <= AddedIndex)
				{
					PreviewSkelComp->SkeletalMesh->BoneBreakOptions.InsertZeroed(AddedIndex);
				}
				PreviewSkelComp->SkeletalMesh->BoneBreakOptions(AddedIndex) = Option;
			}

			if (BoneIndexPairs.Num() > 0)
			{
				//Add the collection of bones to the bone influences list
				CalculateBoneWeightInfluences(PreviewSkelComp->SkeletalMesh, BoneIndexPairs, Option);

				if (PreviewSkelComp->bDrawBoneInfluences)
				{
					//Setup the editor to visualize the change
					UpdateInfluenceWeights(PreviewSkelComp->BonesOfInterest);
				}

				PreviewSkelComp->SkeletalMesh->MarkPackageDirty();
				FComponentReattachContext PreviewSkelReattach(PreviewSkelComp);
				PreviewWindow->ASVPreviewVC->Invalidate();
			}

			EndTransaction(*LocalizeUnrealEd("CalculateBoneBreaks"));
		}

		PreviewSkelComp->SkeletalMesh->PostEditChange();
	}
	else if ( Command == IDM_ANIMSET_SKELETONTREE_DELETEBONEBREAK )
	{
		PreviewSkelComp->SkeletalMesh->PreEditChange(NULL);

		if (BeginTransaction(*LocalizeUnrealEd("DeleteBoneBreaks")))
		{
			PreviewSkelComp->SkeletalMesh->Modify();

			TArray<FBoneIndexPair> BoneIndexPairs;
			for (INT i=0; i<PreviewSkelComp->BonesOfInterest.Num(); i++)
			{
				const INT BoneIndex = PreviewSkelComp->BonesOfInterest(i);

				FBoneIndexPair* NewBoneIndexPair = new(BoneIndexPairs) FBoneIndexPair;
				NewBoneIndexPair->BoneIdx[0] = BoneIndex;
				NewBoneIndexPair->BoneIdx[1] = PreviewSkelComp->SkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;

				const FString& BoneName = PreviewSkelComp->SkeletalMesh->RefSkeleton(BoneIndex).Name.ToString();
				INT IndexToRemove = PreviewSkelComp->SkeletalMesh->BoneBreakNames.RemoveItem(BoneName);
				if (IndexToRemove >= 0)
				{
					PreviewSkelComp->SkeletalMesh->BoneBreakOptions.Remove(IndexToRemove);
				}
			}


			// go through all LOD
			INT NumLODs = PreviewSkelComp->SkeletalMesh->LODModels.Num();
			for( INT CurLODIdx=0; CurLODIdx < NumLODs; CurLODIdx++ )
			{
				FStaticLODModel& LODModel = PreviewSkelComp->SkeletalMesh->LODModels(CurLODIdx);
				for ( INT InfluenceID=0; InfluenceID < LODModel.VertexInfluences.Num(); ++InfluenceID )
				{
					for ( INT BonePairID = 0; BonePairID < BoneIndexPairs.Num(); ++BonePairID )
					{
						LODModel.VertexInfluences(InfluenceID).VertexInfluenceMapping.Remove(BoneIndexPairs(BonePairID));
					}
				}

				BoneInfluenceLODInfo(CurLODIdx).InfluencedBoneVerts.Empty();
				BoneInfluenceLODInfo(CurLODIdx).NonInfluencedBoneVerts.Empty();
			}

			PreviewSkelComp->SkeletalMesh->MarkPackageDirty();
			FComponentReattachContext PreviewSkelReattach(PreviewSkelComp);
			PreviewWindow->ASVPreviewVC->Invalidate();

			EndTransaction(*LocalizeUnrealEd("DeleteBoneBreaks"));
		}

		PreviewSkelComp->SkeletalMesh->PostEditChange();
	}
	else if ( Command == IDM_ANIMSET_SKELETONTREE_RESETBONEBREAKS )
	{
		PreviewSkelComp->SkeletalMesh->PreEditChange(NULL);

		if (BeginTransaction(*LocalizeUnrealEd("ResetBoneBreaks")))
		{
			PreviewSkelComp->SkeletalMesh->Modify();

			// go through all LOD
			INT NumLODs = PreviewSkelComp->SkeletalMesh->LODModels.Num();
			for( INT CurLODIdx=0; CurLODIdx < NumLODs; CurLODIdx++ )
			{
				FStaticLODModel& LODModel = PreviewSkelComp->SkeletalMesh->LODModels(CurLODIdx);
				for ( INT InfluenceID=0; InfluenceID < LODModel.VertexInfluences.Num(); ++InfluenceID )
				{
					LODModel.VertexInfluences(InfluenceID).VertexInfluenceMapping.Empty();
				}

				BoneInfluenceLODInfo(CurLODIdx).InfluencedBoneVerts.Empty();
				BoneInfluenceLODInfo(CurLODIdx).NonInfluencedBoneVerts.Empty();
			}

			PreviewSkelComp->SkeletalMesh->BoneBreakNames.Empty();
			PreviewSkelComp->SkeletalMesh->BoneBreakOptions.Empty();

			PreviewSkelComp->SkeletalMesh->MarkPackageDirty();
			FComponentReattachContext PreviewSkelReattach(PreviewSkelComp);
			PreviewWindow->ASVPreviewVC->Invalidate();

			EndTransaction(*LocalizeUnrealEd("ResetBoneBreaks"));
		}

		PreviewSkelComp->SkeletalMesh->PostEditChange();
	}	
	else
	{
		debugf(TEXT("  Apply changes."));
		// Go through bone list and apply changes.
		FColor NewBoneColor;
		FString BoneNames;
		UBOOL bFirstBone = TRUE;
		for(INT BoneIndex=0; BoneIndex<BoneSelectionMask.Num(); BoneIndex++)
		{
			// Skip bones which were not selected.
			if( BoneSelectionMask(BoneIndex) == 0 )
			{
				continue;
			}

			debugf(TEXT("    Processing bone: %s."), *SelectedSkelMesh->RefSkeleton(BoneIndex).Name.ToString());

			FMeshBone& Bone = SelectedSkelMesh->RefSkeleton(BoneIndex);
			if( Command == IDM_ANIMSET_SKELETONTREE_SHOWBONE || Command == IDM_ANIMSET_SKELETONTREE_SHOWCHILDBONE )
			{
				Bone.BoneColor.A = 255;
			}
			else if( Command == IDM_ANIMSET_SKELETONTREE_HIDEBONE || Command == IDM_ANIMSET_SKELETONTREE_HIDECHILDBONE )
			{
				Bone.BoneColor.A = 0;
			}
			else if( Command == IDM_ANIMSET_SKELETONTREE_SETBONECOLOR || Command == IDM_ANIMSET_SKELETONTREE_SETCHILDBONECOLOR )
			{
				if ( bFirstBone )
				{
					FPickColorStruct PickColorStruct;
					PickColorStruct.RefreshWindows.AddItem(this);
					PickColorStruct.ParentObjects.AddItem(SelectedSkelMesh);
					PickColorStruct.DWORDColorArray.AddItem(&(NewBoneColor));
					PickColorStruct.bModal = TRUE;

					if ( PickColor(PickColorStruct) != ColorPickerConstants::ColorAccepted )
					{
						break;
					}
				}

				// Don't touch Alpha as that is used to toggle per bone drawing.
				Bone.BoneColor.R = NewBoneColor.R;
				Bone.BoneColor.G = NewBoneColor.G;
				Bone.BoneColor.B = NewBoneColor.B;
			}
			else if( Command == IDM_ANIMSET_SKELETONTREE_COPYBONENAME )
			{
				BoneNames += FString::Printf( TEXT("%s\r\n"), *Bone.Name.ToString());
			}

			bFirstBone = FALSE;
		}

		if( Command == IDM_ANIMSET_SKELETONTREE_COPYBONENAME )
		{
			appClipboardCopy( *BoneNames );
		}
	}
}
