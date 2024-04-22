/*
* Copyright 2008-2009 Autodesk, Inc.  All Rights Reserved.
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

#include "UnrealEd.h"
#include "Factories.h"
#include "BusyCursor.h"

#if WITH_FBX

#include "UnFbxImporter.h"

#endif


IMPLEMENT_CLASS(UFbxImportUI);

UFbxFactory::UFbxFactory()
{
	bEditorImport = true;
	bText = false;
	
	ImportUI = ConstructObject<UFbxImportUI>( UFbxImportUI::StaticClass(), this, NAME_None, RF_Public | RF_ClassDefaultObject); //RF_NeedLoad);
	ImportUI->LoadConfig();
}

void UFbxFactory::StaticConstructor()
{
	new(GetClass(),TEXT("Import Options"),RF_Public)	UObjectProperty(CPP_PROPERTY(ImportUI),TEXT(""),CPF_Edit | CPF_EditInline | CPF_Config | CPF_NoClear, UFbxImportUI::StaticClass());
	new(GetClass()->HideCategories) FName(NAME_Object);
}

/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void UFbxFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass = NULL;

	new(Formats)FString(TEXT("fbx;FBX meshes and skeletal meshes"));

	bCreateNew	= FALSE;
	bText		= FALSE;

	bEditorImport = TRUE;
}

UClass* UFbxFactory::ResolveSupportedClass()
{
	UClass* ImportClass = NULL;

	if (ImportUI->MeshTypeToImport )
	{
		ImportClass = USkeletalMesh::StaticClass();
	}
	else
	{
		ImportClass = UStaticMesh::StaticClass();
	}

	return ImportClass;
}


#if WITH_FBX

UBOOL UFbxFactory::DetectImportType(const FFilename& InFilename)
{
	UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();
	INT ImportType = FbxImporter->DetectDeformer(InFilename);
	if ( ImportType == -1)
	{
		return FALSE;
	}
	else
	{
		ImportUI->MeshTypeToImport = ImportType;
	}
	
	return TRUE;
}


UObject* UFbxFactory::ImportANode(void* VoidFbxImporter, void* VoidNode, UBOOL bDoCreateActors, UObject* InParent, FName InName, EObjectFlags Flags, INT& NodeIndex, INT Total, UObject* InMesh, int LODIndex)
{
	UnFbx::CFbxImporter* FbxImporter = (UnFbx::CFbxImporter*)VoidFbxImporter;
	FbxNode* Node = (FbxNode*)VoidNode;

	UObject* NewObject = NULL;
	FName OutputName = FbxImporter->MakeNameForMesh(InName.ToString(), Node);
	
	{
		// skip collision models
		FbxString* NodeName = new FbxString(Node->GetName());
		if ( NodeName->Find("UCX") == 0 || NodeName->Find("MCDCX") == 0 ||
			 NodeName->Find("UBX") == 0 || NodeName->Find("USP") == 0 )
		{
			return NULL;
		}

		NewObject = FbxImporter->ImportStaticMesh( InParent, Node, OutputName, Flags, Cast<UStaticMesh>(InMesh), LODIndex );
	}

	if (NewObject)
	{
		NodeIndex++;
		GWarn->StatusUpdatef( NodeIndex, Total, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Importingf"), NodeIndex, Total)) );
	}

	return NewObject;
}

#endif	// WITH_FBX


UObject* UFbxFactory::FactoryCreateBinary
(
 UClass*			Class,
 UObject*			InParent,
 FName				Name,
 EObjectFlags		Flags,
 UObject*			Context,
 const TCHAR*		Type,
 const BYTE*&		Buffer,
 const BYTE*		BufferEnd,
 FFeedbackContext*	Warn
 )
{
	ImportUI->SaveConfig();
	
	UObject* NewObject = NULL;

#if WITH_FBX

	UnFbx::CFbxImporter* FbxImporter = UnFbx::CFbxImporter::GetInstance();
	
	// load options
	struct UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();
	
	ImportOptions->bImportMaterials = ImportUI->bImportMaterials;
	ImportOptions->bToSeparateGroup = ImportUI->bAutoCreateGroups;
	ImportOptions->bInvertNormalMap = ImportUI->bInvertNormalMaps;
	ImportOptions->bImportTextures = ImportUI->bImportTextures;
    ImportOptions->bOverrideTangents = ImportUI->bOverrideTangents;
	ImportOptions->bUsedAsFullName = ImportUI->bOverrideFullName;
	ImportOptions->bImportAnimSet = ImportUI->bImportAnimations;
	ImportOptions->bResample = ImportUI->bResampleAnimations;
	ImportOptions->bImportMorph = ImportUI->bImportMorphTargets;
	ImportOptions->bImportRigidAnim = ImportUI->bImportRigidAnimation;
	ImportOptions->bUseT0AsRefPose = ImportUI->bUseT0AsRefPose;
	ImportOptions->bSplitNonMatchingTriangles = ImportUI->bSplitNonMatchingTriangles;
	ImportOptions->bCombineToSingle = ImportUI->bCombineMeshes;
	ImportOptions->bReplaceVertexColors = ImportUI->bReplaceVertexColors;
	ImportOptions->bExplicitNormals = ImportUI->bExplicitNormals;
	ImportOptions->bRemoveDegenerates = ImportUI->bRemoveDegenerates;
	ImportOptions->bImportMeshesInBoneHierarchy = ImportUI->bImportMeshesInBoneHierarchy;
	ImportOptions->bOneConvexHullPerUCX = ImportUI->bOneConvexHullPerUCX;

	Warn->BeginSlowTask( TEXT("Importing FBX mesh"), TRUE );
	if ( !FbxImporter->ImportFromFile( *UFactory::CurrentFilename ) )
	{
		// Log the error message and fail the import.
		Warn->Log( NAME_Error, FbxImporter->GetErrorMessage() );
	}
	else
	{
		// Log the import message and import the mesh.
		const TCHAR* errorMessage = FbxImporter->GetErrorMessage();
		if (errorMessage[0] != NULL)
		{
			Warn->Log( errorMessage );
		}

		FbxNode* RootNodeToImport = NULL;
		RootNodeToImport = FbxImporter->Scene->GetRootNode();

		INT InterestingNodeCount = 1;
		TArray< TArray<FbxNode*>* > SkelMeshArray;

		if (ImportUI->MeshTypeToImport == 1)
		{
			//FbxImporter->ReplaceOrFindNullsUsedAsLinks(RootNodeToImport);
			FbxImporter->FillFbxSkelMeshArrayInScene(RootNodeToImport, SkelMeshArray, FALSE);
			InterestingNodeCount = SkelMeshArray.Num();
		}
		else 
		{
			if ( !ImportUI->bCombineMeshes )
			{
				InterestingNodeCount = FbxImporter->GetFbxMeshCount(RootNodeToImport);
			} 
		}
		
		if (InterestingNodeCount > 1)
		{
			// the option only works when there are only one asset
			ImportOptions->bUsedAsFullName = FALSE;
		}

		const FFilename Filename( UFactory::CurrentFilename );
		if (RootNodeToImport && InterestingNodeCount > 0)
		{  
			INT NodeIndex = 0;

			if ( ImportUI->MeshTypeToImport == 0 )  // static mesh
			{
				if (ImportUI->bCombineMeshes)
				{
					TArray<FbxNode*> FbxMeshArray;
					FbxImporter->FillFbxMeshArray(RootNodeToImport, FbxMeshArray, FbxImporter);
					if (FbxMeshArray.Num() > 0)
					{
						NewObject = FbxImporter->ImportStaticMeshAsSingle(InParent, FbxMeshArray, Name, Flags, NULL, 0);
					}
				}
				else
				{
					NewObject = RecursiveImportNode(FbxImporter,RootNodeToImport,InterestingNodeCount>1,InParent,Name,Flags,NodeIndex,InterestingNodeCount);
				}
			}
			else // skeletal mesh
			{
				for (INT i = 0; i < SkelMeshArray.Num(); i++)
				{
					TArray<FbxNode*> NodeArray = *SkelMeshArray(i);
					
					// check if there is LODGroup for this skeletal mesh
					INT MaxLODLevel = 1;
					for (INT j = 0; j < NodeArray.Num(); j++)
					{
						FbxNode* Node = NodeArray(j);
						if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
						{
							// get max LODgroup level
							if (MaxLODLevel < Node->GetChildCount())
							{
								MaxLODLevel = Node->GetChildCount();
							}
						}
					}
					
					INT LODIndex;
					for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
					{
						if ( !ImportUI->bImportMeshLODs && LODIndex > 0) // not import LOD if UI option is OFF
						{
							break;
						}
						
						TArray<FbxNode*> SkelMeshNodeArray;
						for (INT j = 0; j < NodeArray.Num(); j++)
						{
							FbxNode* Node = NodeArray(j);
							if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
							{
								if (Node->GetChildCount() > LODIndex)
								{
									SkelMeshNodeArray.AddItem(Node->GetChild(LODIndex));
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
						
						if (LODIndex == 0)
						{
							FName OutputName = FbxImporter->MakeNameForMesh(Name.ToString(), SkelMeshNodeArray(0));

							NewObject = FbxImporter->ImportSkeletalMesh( InParent, SkelMeshNodeArray, OutputName, Flags, Filename.GetBaseFilename() );
						}
						else if (NewObject) // the base skeletal mesh is imported successfully
						{
							USkeletalMesh* BaseSkeletalMesh = Cast<USkeletalMesh>(NewObject);
							UObject *LODObject = FbxImporter->ImportSkeletalMesh( UObject::GetTransientPackage(), SkelMeshNodeArray, NAME_None, 0, Filename.GetBaseFilename() );
							FbxImporter->ImportSkeletalMeshLOD( Cast<USkeletalMesh>(LODObject), BaseSkeletalMesh, LODIndex);
							
							// Set LOD Model's DisplayFactor
							/*
							fbxDistance Threshold;

							if ( LodGroup->GetThreshold(i-1, Threshold) )
							{
							BaseSkeletalMesh->LODInfo(i).DisplayFactor = (FLOAT)((Threshold.value() - LodGroup->MinDistance.Get()) /
							(LodGroup->MaxDistance.Get() - LodGroup->MinDistance.Get()));
							}
							*/
							BaseSkeletalMesh->LODInfo(LODIndex).DisplayFactor = 1.0 / MaxLODLevel * LODIndex;
						}
						
						// import morph target
						if ( NewObject && ImportUI->bImportMorphTargets)
						{
							// Disable material importing when importing morph targets
							BITFIELD bImportMaterials = ImportOptions->bImportMaterials;
							ImportOptions->bImportMaterials = 0;

							FbxImporter->ImportFbxMorphTarget(SkelMeshNodeArray, Cast<USkeletalMesh>(NewObject), Filename, LODIndex);
							
							ImportOptions->bImportMaterials = bImportMaterials;
						}
					}
					
					if (NewObject)
					{
						NodeIndex++;
						GWarn->StatusUpdatef( NodeIndex, SkelMeshArray.Num(), *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Importingf"), NodeIndex, SkelMeshArray.Num())) );
					}
				}
				
				for (INT i = 0; i < SkelMeshArray.Num(); i++)
				{
					delete SkelMeshArray(i);
				}
			}
		}
	}

	FbxImporter->ReleaseScene();
	Warn->EndSlowTask();

#endif	// WITH_FBX

	return NewObject;
}


#if WITH_FBX

UObject* UFbxFactory::RecursiveImportNode(void* VoidFbxImporter, void* VoidNode, UBOOL bDoCreateActors, UObject* InParent, FName InName, EObjectFlags Flags, INT& NodeIndex, INT Total)
{
	UObject* NewObject = NULL;

	FbxNode* Node = (FbxNode*)VoidNode;
	if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
	{
		// import base mesh
		NewObject = ImportANode(VoidFbxImporter, Node->GetChild(0), bDoCreateActors, InParent, InName, Flags, NodeIndex, Total);
		if (NewObject && ImportUI->bImportMeshLODs)
		{
			// import LOD meshes
			for (INT LODIndex = 1; LODIndex < Node->GetChildCount(); LODIndex++)
			{
				FbxNode* ChildNode = Node->GetChild(LODIndex);
				ImportANode(VoidFbxImporter, ChildNode, false, InParent, InName, Flags, NodeIndex, Total, NewObject, LODIndex);
			}
		}
	}
	else
	{
		if (Node->GetMesh())
		{
			NewObject = ImportANode(VoidFbxImporter, Node, bDoCreateActors, InParent, InName, Flags, NodeIndex, Total);
		}
		
		for (INT ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			UObject* SubObject = RecursiveImportNode(VoidFbxImporter,Node->GetChild(ChildIndex),bDoCreateActors,InParent,InName,Flags,NodeIndex,Total);
			if (NewObject==NULL)
			{
				NewObject = SubObject;
			}
		}
	}

	return NewObject;
}


#endif	// WITH_FBX


IMPLEMENT_CLASS(UFbxFactory);

