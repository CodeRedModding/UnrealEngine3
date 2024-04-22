/*
* Copyright 2009 - 2010 Autodesk, Inc.  All Rights Reserved.
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
	Main implementation of CFbxImporter : import FBX data to Unreal
=============================================================================*/

#include "UnrealEd.h"
#include "FFeedbackContextEditor.h"

#if WITH_FBX

#include "Factories.h"
#include "Engine.h"

#include "SkelImport.h"
#include "EnginePrefabClasses.h"
#include "EngineAnimClasses.h"
#include "UnFbxImporter.h"

namespace UnFbx
{

//-------------------------------------------------------------------------
// Memory management callback functions used by the FBX SDK
//-------------------------------------------------------------------------
#if defined(_DEBUG)

void* MyMalloc(size_t pSize, int, const char*, int)       
{
	return malloc(pSize);
}

void* MyCalloc(size_t pCount,size_t pSize, int, const char*, int)
{
	return calloc(pCount, pSize);
}

void* MyRealloc(void* pData, size_t pSize, int, const char*, int)
{
	return realloc(pData, pSize);
}

void  MyFree(void* pData, int)
{
	free(pData);
}

size_t MyMsize(void* pData, int)
{
	return _msize(pData);
}

#else

void* MyMalloc(size_t pSize)       
{
	return malloc(pSize);
}

void* MyCalloc(size_t pCount,size_t pSize)
{
	return calloc(pCount, pSize);
}

void* MyRealloc(void* pData, size_t pSize)
{
	return realloc(pData, pSize);
}

void  MyFree(void* pData)
{
	free(pData);
}

size_t MyMsize(void* pData)
{
	return _msize(pData);
}

#endif // defined(_DEBUG)


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
CFbxImporter::CFbxImporter()
	: bFirstMesh(TRUE)
{
	// Specify global memory handler callbacks to be used by the FBX SDK
	FbxSetMallocHandler(	&MyMalloc);
	FbxSetCallocHandler(	&MyCalloc);
	FbxSetReallocHandler(	&MyRealloc);
	FbxSetFreeHandler(		&MyFree);
	FbxSetMSizeHandler(		&MyMsize);

	// Create the SdkManager
	SdkManager = FbxManager::Create();
	
	// create an IOSettings object
	FbxIOSettings * ios = FbxIOSettings::Create(SdkManager, IOSROOT );
	SdkManager->SetIOSettings(ios);

	// Create the geometry converter
	GeometryConverter = new FbxGeometryConverter(SdkManager);
	Scene = NULL;
	
	ImportOptions = new FBXImportOptions();
	
	CurPhase = NOTSTARTED;

	FbxImportCamera = NULL;
}
	
//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
CFbxImporter::~CFbxImporter()
{
	CleanUp();
}

const FLOAT CFbxImporter::SCALE_TOLERANCE = 0.000001;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
CFbxImporter* CFbxImporter::GetInstance()
{
	static CFbxImporter* ImporterInstance = NULL;
	
	if (ImporterInstance == NULL)
	{
		ImporterInstance = new CFbxImporter();
	}
	return ImporterInstance;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void CFbxImporter::CleanUp()
{
	ReleaseScene();
	
	if (SdkManager)
	{
		SdkManager->Destroy();
	}
	SdkManager = NULL;
	delete GeometryConverter;
	GeometryConverter = NULL;
	delete ImportOptions;
	ImportOptions = NULL;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void CFbxImporter::ReleaseScene()
{
	if (Importer)
	{
		Importer->Destroy();
		Importer = NULL;
	}
	
	if (Scene)
	{
		Scene->Destroy();
		Scene = NULL;
	}
	
	// reset
	CollisionModels.Clear();
	SkelMeshToMorphMap.Reset();
	SkelMeshToMorphMap.Shrink();
	CurPhase = NOTSTARTED;
	bFirstMesh = TRUE;
}

FBXImportOptions* UnFbx::CFbxImporter::GetImportOptions()
{
	return ImportOptions;
}

INT CFbxImporter::DetectDeformer(const FFilename& InFilename)
{
	INT Result = 0;
	FString Filename = InFilename;
	
	if (OpenFile(Filename, TRUE))
	{
		FbxStatistics Statistics;
		Importer->GetStatistics(&Statistics);
		INT ItemIndex;
		FbxString ItemName;
		INT ItemCount;
		for ( ItemIndex = 0; ItemIndex < Statistics.GetNbItems(); ItemIndex++ )
		{
			Statistics.GetItemPair(ItemIndex, ItemName, ItemCount);
			if ( ItemName == "Deformer" && ItemCount > 0 )
			{
				Result = 1;
				break;
			}
		}
		Importer->Destroy();
		Importer = NULL;
		CurPhase = NOTSTARTED;
	}
	else
	{
		Result = -1;
	}
	
	return Result; 
}

UBOOL CFbxImporter::GetSceneInfo(FString Filename, FbxSceneInfo& SceneInfo)
{
	UBOOL Result = TRUE;
	
	//if we're riding on top of an existing task, save off the previous task...
	const TCHAR *StatusMessage = TEXT("Parse FBX file to get scene info");
	UBOOL bPushedSlowTask = FALSE;
	if (GIsSlowTask)
	{
		GWarn->PushStatus();
		GWarn->StatusUpdatef(0,100,StatusMessage);
		bPushedSlowTask = TRUE;
	}
	else
	{
		GWarn->BeginSlowTask( StatusMessage, TRUE );
	}
	
	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(Filename, FALSE))
		{
			Result = FALSE;
			break;
		}
		GWarn->UpdateProgress( 40, 100 );
		//pass through
	case FILEOPENED:
		if (!ImportFile(Filename))
		{
			Result = FALSE;
			break;
		}
		GWarn->UpdateProgress( 90, 100 );
		//pass through
	case IMPORTED:
	
	default:
		break;
	}
	
	if (Result)
	{
		FbxTimeSpan GlobalTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
		
		TArray<FbxNode*> LinkNodes;
		
		SceneInfo.TotalMaterialNum = Scene->GetMaterialCount();
		SceneInfo.TotalTextureNum = Scene->GetTextureCount();
		SceneInfo.TotalGeometryNum = 0;
		SceneInfo.NonSkinnedMeshNum = 0;
		SceneInfo.SkinnedMeshNum = 0;
		for ( INT GeometryIndex = 0; GeometryIndex < Scene->GetGeometryCount(); GeometryIndex++ )
		{
			FbxGeometry * Geometry = Scene->GetGeometry(GeometryIndex);
			
			if (Geometry->GetAttributeType() == FbxNodeAttribute::eMESH)
			{
				FbxNode* GeoNode = Geometry->GetNode();
				UBOOL bIsLinkNode = FALSE;

				// check if this geometry node is used as link
				for ( INT i = 0; i < LinkNodes.Num(); i++ )
				{
					if ( GeoNode == LinkNodes(i))
					{
						bIsLinkNode = TRUE;
						break;
					}
				}
				// if the geometry node is used as link, ignore it
				if (bIsLinkNode)
				{
					continue;
				}

				SceneInfo.TotalGeometryNum++;
				
				FbxMesh* Mesh = (FbxMesh*)Geometry;
				SceneInfo.MeshInfo.Add();
				FbxMeshInfo& MeshInfo = SceneInfo.MeshInfo.Last();
				MeshInfo.Name = MakeName(GeoNode->GetName());
				MeshInfo.bTriangulated = Mesh->IsTriangleMesh();
				MeshInfo.MaterialNum = GeoNode->GetMaterialCount();
				MeshInfo.FaceNum = Mesh->GetPolygonCount();
				MeshInfo.VertexNum = Mesh->GetControlPointsCount();
				
				// LOD info
				MeshInfo.LODGroup = NULL;
				FbxNode* ParentNode = GeoNode->GetParent();
				if ( ParentNode->GetNodeAttribute() && ParentNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
				{
					FbxNodeAttribute* LODGroup = ParentNode->GetNodeAttribute();
					MeshInfo.LODGroup = MakeName(ParentNode->GetName());
					for (INT LODIndex = 0; LODIndex < ParentNode->GetChildCount(); LODIndex++)
					{
						if ( GeoNode == ParentNode->GetChild(LODIndex))
						{
							MeshInfo.LODLevel = LODIndex;
							break;
						}
					}
				}
				
				// skeletal mesh
				if (Mesh->GetDeformerCount(FbxDeformer::eSKIN) > 0)
				{
					SceneInfo.SkinnedMeshNum++;
					MeshInfo.bIsSkelMesh = TRUE;
					MeshInfo.MorphNum = Mesh->GetShapeCount();
					// skeleton root
					FbxSkin* Skin = (FbxSkin*)Mesh->GetDeformer(0, FbxDeformer::eSKIN);
					FbxNode* Link = Skin->GetCluster(0)->GetLink();
					while (Link->GetParent() && Link->GetParent()->GetSkeleton())
					{
						Link = Link->GetParent();
					}
					MeshInfo.SkeletonRoot = MakeName(Link->GetName());
					MeshInfo.SkeletonElemNum = Link->GetChildCount(true);
					
					FbxTimeSpan AnimTimeSpan(FBXSDK_TIME_INFINITE,FBXSDK_TIME_MINUS_INFINITE);
					Link->GetAnimationInterval(AnimTimeSpan);
					GlobalTimeSpan.UnionAssignment(AnimTimeSpan);
				}
				else
				{
					SceneInfo.NonSkinnedMeshNum++;
					MeshInfo.bIsSkelMesh = FALSE;
					MeshInfo.SkeletonRoot = NULL;
				}
			}
		}
		
		// TODO: display multiple anim stack
		SceneInfo.TakeName = NULL;
		for( INT AnimStackIndex = 0; AnimStackIndex < Scene->GetSrcObjectCount(FbxAnimStack::ClassId); AnimStackIndex++ )
		{
			FbxAnimStack* CurAnimStack = FbxCast<FbxAnimStack>(Scene->GetSrcObject(FbxAnimStack::ClassId, 0));
			// TODO: skip empty anim stack
			const char* AnimStackName = CurAnimStack->GetName();
			SceneInfo.TakeName = new char[strlen(AnimStackName) + 1];
			strcpy_s(SceneInfo.TakeName, strlen(AnimStackName) + 1, AnimStackName);
		}
		SceneInfo.FrameRate = FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode());
		
		if ( GlobalTimeSpan.GetDirection() == FBXSDK_TIME_FORWARD)
		{
			SceneInfo.TotalTime = (GlobalTimeSpan.GetDuration().GetMilliSeconds())/1000.f * SceneInfo.FrameRate;
		}
		else
		{
			SceneInfo.TotalTime = 0;
		}
	}
	
	//if we're riding on top of an existing task, reload the old task
	if (bPushedSlowTask)
	{
		GWarn->PopStatus();
	}
	else
	{
		GWarn->EndSlowTask();
	}
	return Result;
}


UBOOL CFbxImporter::OpenFile(FString Filename, UBOOL bParseStatistics)
{
	UBOOL Result = TRUE;
	
	if (CurPhase != NOTSTARTED)
	{
		// something went wrong
		return FALSE;
	}

	INT SDKMajor,  SDKMinor,  SDKRevision;

	// Create an importer.
	Importer = FbxImporter::Create(SdkManager,"");

	// Get the version number of the FBX files generated by the
	// version of FBX SDK that you are using.
	FbxManager::GetFileFormatVersion(SDKMajor, SDKMinor, SDKRevision);

	// Initialize the importer by providing a filename.
	if (bParseStatistics)
	{
		Importer->ParseForStatistics(true);
	}
	
	const UBOOL bImportStatus = Importer->Initialize(TCHAR_TO_ANSI(*Filename));

	if( !bImportStatus )  // Problem with the file to be imported
	{
		warnf(NAME_Error,TEXT("Call to KFbxImporter::Initialize() failed."));
		warnf(TEXT("Error returned: %s"), ANSI_TO_TCHAR(Importer->GetLastErrorString()));

		if (Importer->GetLastErrorID() ==
			FbxIO::eFileVersionNotSupportedYet ||
			Importer->GetLastErrorID() ==
			FbxIO::eFileVersionNotSupportedAnymore)
		{
			warnf(TEXT("FBX version number for this FBX SDK is %d.%d.%d"),
				SDKMajor, SDKMinor, SDKRevision);
		}

		return FALSE;
	}

	// Skip the version check if we are just parsing for information.
	if( !bParseStatistics )
	{
		INT FileMajor,  FileMinor,  FileRevision;
		Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

		INT FileVersion = (FileMajor << 16 | FileMinor << 8 | FileRevision);
		INT SDKVersion = (SDKMajor << 16 | SDKMinor << 8 | SDKRevision);

		if( FileVersion != SDKVersion )
		{

			// Appending the SDK version to the config key causes the warning to automatically reappear even if previously suppressed when the SDK version we use changes. 
			FString ConfigStr = FString::Printf( TEXT("Warning_OutOfDateFBX_%d"), SDKVersion );

			FString FileVerStr = FString::Printf( TEXT("%d.%d.%d"), FileMajor, FileMinor, FileRevision );
			FString SDKVerStr  = FString::Printf( TEXT("%d.%d.%d"), SDKMajor, SDKMinor, SDKRevision );

			FString WarningText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT("Warning_OutOfDateFBX") ), *FileVerStr, *SDKVerStr ) );

			WxSuppressableWarningDialog VersionWarning( 
				WarningText,
				LocalizeUnrealEd( "Warning_IncompatibleFBX_Title" ),
				ConfigStr );

			VersionWarning.ShowModal();
		}
	}

	CurPhase = FILEOPENED;
	// Destroy the importer
	//Importer->Destroy();

	return Result;
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif

UBOOL CFbxImporter::ImportFile(FString Filename)
{
	UBOOL Result = TRUE;
	
	UBOOL bStatus;
	
	FFilename FilePath(Filename);
	FileBasePath = FilePath.GetPath();

	// Create the Scene
	Scene = FbxScene::Create(SdkManager,"");
	warnf(TEXT("Loading FBX Scene from %s"), *Filename);

	INT FileMajor, FileMinor, FileRevision;

	IOS_REF.SetBoolProp(IMP_FBX_MATERIAL,		true);
	IOS_REF.SetBoolProp(IMP_FBX_TEXTURE,		 true);
	IOS_REF.SetBoolProp(IMP_FBX_LINK,			true);
	IOS_REF.SetBoolProp(IMP_FBX_SHAPE,		   true);
	IOS_REF.SetBoolProp(IMP_FBX_GOBO,			true);
	IOS_REF.SetBoolProp(IMP_FBX_ANIMATION,	   true);
	IOS_REF.SetBoolProp(IMP_SKINS,			   true);
	IOS_REF.SetBoolProp(IMP_DEFORMATION,		 true);
	IOS_REF.SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
	IOS_REF.SetBoolProp(IMP_TAKE,				true);

	// Import the scene.
	bStatus = Importer->Import(Scene);

	// Get the version number of the FBX file format.
	Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

	// output result
	if(bStatus)
	{
		warnf(TEXT("FBX Scene Loaded Succesfully"));
		CurPhase = IMPORTED;
	}
	else
	{
		ErrorMessage = ANSI_TO_TCHAR(Importer->GetLastErrorString());
		warnf(NAME_Error,TEXT("FBX Scene Loading Failed : %s"), Importer->GetLastErrorString());
		CleanUp();
		Result = FALSE;
		CurPhase = NOTSTARTED;
	}
	
	Importer->Destroy();
	Importer = NULL;
	
	return Result;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
UBOOL CFbxImporter::ImportFromFile(const TCHAR* Filename)
{
	UBOOL Result = TRUE;
	// Converts the FBX data to Z-up, X-forward, Y-left.  Unreal is the same except with Y-right, 
	// but the conversion to left-handed coordinates is not working properly
	FbxAxisSystem::eFrontVector FrontVector = (FbxAxisSystem::eFrontVector)-FbxAxisSystem::ParityOdd;
	const FbxAxisSystem UnrealZUp(FbxAxisSystem::ZAxis, FrontVector, FbxAxisSystem::RightHanded);

	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(FString(Filename), FALSE))
		{
			Result = FALSE;
			break;
		}
	case FILEOPENED:
		if (!ImportFile(FString(Filename)))
		{
			Result = FALSE;
			CurPhase = NOTSTARTED;
			break;
		}
	case IMPORTED:
		// convert axis to Z-up
		FbxRootNodeUtility::RemoveAllFbxRoots( Scene );
		UnrealZUp.ConvertScene( Scene );

		// Convert the scene's units to what is used in this program, if needed.
		// The base unit used in both FBX and Unreal is centimeters.  So unless the units 
		// are already in centimeters (ie: scalefactor 1.0) then it needs to be converted
		//if( FbxScene->GetGlobalSettings().GetSystemUnit().GetScaleFactor() != 1.0 )
		//{
		//	KFbxSystemUnit::cm.ConvertScene( FbxScene );
		//}


		// convert name to unreal-supported format
		// actually, crashes...
		//KFbxSceneRenamer renamer(FbxScene);
		//renamer.ResolveNameClashing(false,false,true,true,true,KString(),"TestBen",false,false);
		
	default:
		break;
	}
	
	return Result;
}

ANSICHAR* CFbxImporter::MakeName(const ANSICHAR* Name)
{
	int SpecialChars[] = {'.', ',', '/', '`', '%'};
	ANSICHAR* TmpName = new ANSICHAR[strlen(Name)+1];
	int len = strlen(Name);
	strcpy_s(TmpName, strlen(Name) + 1, Name);

	for ( INT i = 0; i < 5; i++ )
	{
		ANSICHAR* CharPtr = TmpName;
		while ( (CharPtr = strchr(CharPtr,SpecialChars[i])) != NULL )
		{
			CharPtr[0] = '_';
		}
	}

	// Remove namespaces
	ANSICHAR* NewName;
	NewName = strchr (TmpName, ':');
	  
	// there may be multiple namespace, so find the last ':'
	while (NewName && strchr(NewName + 1, ':'))
	{
		NewName = strchr(NewName + 1, ':');
	}

	if (NewName)
	{
		return NewName + 1;
	}

	return TmpName;
}

FName CFbxImporter::MakeNameForMesh(FString InName, FbxObject* FbxObject)
{
	FName OutputName;

	// "Name" field can't be empty
	if (ImportOptions->bUsedAsFullName && InName != FString("None"))
	{
		OutputName = *InName;
	}
	else
	{
		char Name[512];
		int SpecialChars[] = {'.', ',', '/', '`', '%'};
		sprintf_s(Name,512,"%s",FbxObject->GetName());

		for ( INT i = 0; i < 5; i++ )
		{
			char* CharPtr = Name;
			while ( (CharPtr = strchr(CharPtr,SpecialChars[i])) != NULL )
			{
				CharPtr[0] = '_';
			}
		}

		// for mesh, replace ':' with '_' because Unreal doesn't support ':' in mesh name
		char* NewName = NULL;
		NewName = strchr (Name, ':');

		if (NewName)
		{
			char* Tmp;
			Tmp = NewName;
			while (Tmp)
			{

				// Always remove namespaces
				NewName = Tmp + 1;
				
				// there may be multiple namespace, so find the last ':'
				Tmp = strchr(NewName + 1, ':');
			}
		}
		else
		{
			NewName = Name;
		}

		if ( InName == FString("None"))
		{
			OutputName = FName( *FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(NewName )) );
		}
		else
		{
			OutputName = FName( *FString::Printf(TEXT("%s_%s"), *InName,ANSI_TO_TCHAR(NewName)) );
		}
	}
	
	return OutputName;
}

FbxAMatrix CFbxImporter::ComputeTotalMatrix(FbxNode* Node)
{
	FbxAMatrix Geometry;
	FbxVector4 Translation, Rotation, Scaling;
	Translation = Node->GetGeometricTranslation(FbxNode::eSOURCE_SET);
	Rotation = Node->GetGeometricRotation(FbxNode::eSOURCE_SET);
	Scaling = Node->GetGeometricScaling(FbxNode::eSOURCE_SET);
	Geometry.SetT(Translation);
	Geometry.SetR(Rotation);
	Geometry.SetS(Scaling);

	//For Single Matrix situation, obtain transfrom matrix from eDESTINATION_SET, which include pivot offsets and pre/post rotations.
	FbxAMatrix& GlobalTransform = Scene->GetEvaluator()->GetNodeGlobalTransform(Node);
	
	FbxAMatrix TotalMatrix;
	TotalMatrix = GlobalTransform * Geometry;

	return TotalMatrix;
}

UBOOL CFbxImporter::IsOddNegativeScale(FbxAMatrix& TotalMatrix)
{
	FbxVector4 Scale = TotalMatrix.GetS();
	INT NegativeNum = 0;

	if (Scale[0] < 0) NegativeNum++;
	if (Scale[1] < 0) NegativeNum++;
	if (Scale[2] < 0) NegativeNum++;

	return NegativeNum == 1 || NegativeNum == 3;
}

/**
* Recursively get skeletal mesh count
*
* @param Node Root node to find skeletal meshes
* @return INT skeletal mesh count
*/
INT GetFbxSkeletalMeshCount(FbxNode* Node)
{
	INT SkeletalMeshCount = 0;
	if (Node->GetMesh() && (Node->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN)>0))
	{
		SkeletalMeshCount = 1;
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		SkeletalMeshCount += GetFbxSkeletalMeshCount(Node->GetChild(ChildIndex));
	}

	return SkeletalMeshCount;
}

/**
* Get mesh count (including static mesh and skeletal mesh, except collision models) and find collision models
*
* @param Node Root node to find meshes
* @return INT mesh count
*/
INT CFbxImporter::GetFbxMeshCount(FbxNode* Node)
{
	INT MeshCount = 0;
	if (Node->GetMesh())
	{
		if (!FillCollisionModelList(Node))
		{
			MeshCount = 1;
		}
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		MeshCount += GetFbxMeshCount(Node->GetChild(ChildIndex));
	}

	return MeshCount;
}

/**
* Get all Fbx mesh objects
*
* @param Node Root node to find meshes
* @param outMeshArray return Fbx meshes
*/
void CFbxImporter::FillFbxMeshArray(FbxNode* Node, TArray<FbxNode*>& outMeshArray, UnFbx::CFbxImporter* FbxImporter)
{
	if (Node->GetMesh())
	{
		if (!FbxImporter->FillCollisionModelList(Node))
		{ 
			outMeshArray.AddItem(Node);
		}
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FillFbxMeshArray(Node->GetChild(ChildIndex), outMeshArray, FbxImporter);
	}
}

/**
* Get all Fbx skeletal mesh objects
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes
*/
void FillFbxSkelMeshArray(FbxNode* Node, TArray<FbxNode*>& outSkelMeshArray)
{
	if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN) > 0 )
	{
		outSkelMeshArray.AddItem(Node);
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FillFbxSkelMeshArray(Node->GetChild(ChildIndex), outSkelMeshArray);
	}
}

void CFbxImporter::RecursiveFixSkeleton(FbxNode* Node, TArray<FbxNode*> &SkelMeshes, UBOOL bImportNestedMeshes )
{
	for (INT i = 0; i < Node->GetChildCount(); i++)
	{
		RecursiveFixSkeleton(Node->GetChild(i), SkelMeshes, bImportNestedMeshes );
	}

	FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	if ( Attr && (Attr->GetAttributeType() == FbxNodeAttribute::eMESH || Attr->GetAttributeType() == FbxNodeAttribute::eNULL ) )
	{
		if( bImportNestedMeshes  && Attr->GetAttributeType() == FbxNodeAttribute::eMESH )
		{
			// for leaf mesh, keep them as mesh
			INT ChildCount = Node->GetChildCount();
			INT ChildIndex;
			for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++)
			{
				FbxNode* Child = Node->GetChild(ChildIndex);
				if (Child->GetMesh() == NULL)
				{
					break;
				}
			}

			if (ChildIndex != ChildCount)
			{
				//replace with skeleton
				FbxSkeleton* lSkeleton = FbxSkeleton::Create(SdkManager,"");
				Node->SetNodeAttribute(lSkeleton);
				lSkeleton->SetSkeletonType(FbxSkeleton::eLIMB_NODE);
			}
			else // this mesh may be not in skeleton mesh list. If not, add it.
			{
				INT MeshIndex;
				for (MeshIndex = 0; MeshIndex < SkelMeshes.Num(); MeshIndex++)
				{
					if (Node == SkelMeshes(MeshIndex))
					{
						break;
					}
				}

				if (MeshIndex == SkelMeshes.Num()) // add this mesh
				{
					SkelMeshes.AddItem(Node);
				}
			}
		}
		else
		{
			//replace with skeleton
			FbxSkeleton* lSkeleton = FbxSkeleton::Create(SdkManager,"");
			Node->SetNodeAttribute(lSkeleton);
			lSkeleton->SetSkeletonType(FbxSkeleton::eLIMB_NODE);
		}
	}
}

FbxNode* CFbxImporter::GetRootSkeleton(FbxNode* Link)
{
	FbxNode* RootBone = Link;

	// get Unreal skeleton root
	// mesh and dummy are used as bone if they are in the skeleton hierarchy
	while (RootBone->GetParent())
	{
        const ANSICHAR* Name = RootBone->GetName();
        const ANSICHAR* PName = RootBone->GetParent() ? RootBone->GetParent()->GetName() : "";
       	FbxMesh* Mesh = RootBone->GetParent() ? RootBone->GetParent()->GetMesh() : NULL;

		FbxNodeAttribute* Attr = RootBone->GetParent()->GetNodeAttribute();
		if (Attr && 
			(Attr->GetAttributeType() == FbxNodeAttribute::eMESH || 
			 Attr->GetAttributeType() == FbxNodeAttribute::eNULL ||
			 Attr->GetAttributeType() == FbxNodeAttribute::eSKELETON) &&
			RootBone->GetParent() != Scene->GetRootNode())
		{
			// in some case, skeletal mesh can be ancestor of bones
			// this avoids this situation
			if (Attr->GetAttributeType() == FbxNodeAttribute::eMESH )
			{
				FbxMesh* Mesh = (FbxMesh*)Attr;
				if (Mesh->GetDeformerCount(FbxDeformer::eSKIN) > 0)
				{
					break;
				}
			}

			RootBone = RootBone->GetParent();
		}
		else
		{
			break;
		}
	}

	return RootBone;
}

/**
* Get all Fbx skeletal mesh objects which are grouped by skeleton they bind to
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
* @param SkeletonArray
* @param ExpandLOD flag of expanding LOD to get each mesh
*/
void CFbxImporter::RecursiveFindFbxSkelMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, UBOOL ExpandLOD)
{
	FbxNode* SkelMeshNode = NULL;
	FbxNode* NodeToAdd = Node;

    if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN) > 0 )
	{
		SkelMeshNode = Node;
	}
	else if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
	{
		// for LODgroup, add the LODgroup to OutSkelMeshArray according to the skeleton that the first child bind to
		SkelMeshNode = Node->GetChild(0);
		// check if the first child is skeletal mesh
		if (!(SkelMeshNode->GetMesh() && SkelMeshNode->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN) > 0))
		{
			SkelMeshNode = NULL;
		}
		else if (ExpandLOD)
		{
			// if ExpandLOD is TRUE, only add the first LODGroup level node
			NodeToAdd = SkelMeshNode;
		}
		// else NodeToAdd = Node;
	}

	if (SkelMeshNode)
	{
		// find root skeleton

		check(SkelMeshNode->GetMesh() != NULL);
		const int fbxDeformerCount = SkelMeshNode->GetMesh()->GetDeformerCount();
		FbxSkin* Deformer = static_cast<FbxSkin*>( SkelMeshNode->GetMesh()->GetDeformer(0, FbxDeformer::eSKIN) );
		
		if (Deformer != NULL)
		{
			FbxNode* Link = Deformer->GetCluster(0)->GetLink(); //Get the bone influences by this first cluster
			Link = GetRootSkeleton(Link); // Get the skeleton root itself

			INT i;
			for (i = 0; i < SkeletonArray.Num(); i++)
			{
				if ( Link == SkeletonArray(i) )
				{
					// append to existed outSkelMeshArray element
					TArray<FbxNode*>* TempArray = outSkelMeshArray(i);
					TempArray->AddItem(NodeToAdd);
					break;
				}
			}

			// if there is no outSkelMeshArray element that is bind to this skeleton
			// create new element for outSkelMeshArray
			if ( i == SkeletonArray.Num() )
			{
				TArray<FbxNode*>* TempArray = new TArray<FbxNode*>();
				TempArray->AddItem(NodeToAdd);
				outSkelMeshArray.AddItem(TempArray);
				SkeletonArray.AddItem(Link);
			}
		}
	}
	else
	{
		INT ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			RecursiveFindFbxSkelMesh(Node->GetChild(ChildIndex), outSkelMeshArray, SkeletonArray, ExpandLOD);
		}
	}
}

void CFbxImporter::RecursiveFindRigidMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, UBOOL ExpandLOD)
{
	UBOOL RigidNodeFound = FALSE;
	FbxNode* RigidMeshNode = NULL;

	if (Node->GetMesh())
	{
		// ignore skeletal mesh
		if (Node->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN) == 0 )
		{
			RigidMeshNode = Node;
			RigidNodeFound = TRUE;
		}
	}
	else if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
	{
		// for LODgroup, add the LODgroup to OutSkelMeshArray according to the skeleton that the first child bind to
		FbxNode* FirstLOD = Node->GetChild(0);
		// check if the first child is skeletal mesh
		if (FirstLOD->GetMesh())
		{
			if (FirstLOD->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN) == 0 )
			{
				RigidNodeFound = TRUE;
			}
		}

		if (RigidNodeFound)
		{
			if (ExpandLOD)
			{
				RigidMeshNode = FirstLOD;
			}
			else
			{
				RigidMeshNode = Node;
			}

		}
	}

	if (RigidMeshNode)
	{
		// find root skeleton
		FbxNode* Link = GetRootSkeleton(RigidMeshNode);

		INT i;
		for (i = 0; i < SkeletonArray.Num(); i++)
		{
			if ( Link == SkeletonArray(i))
			{
				// append to existed outSkelMeshArray element
				TArray<FbxNode*>* TempArray = outSkelMeshArray(i);
				TempArray->AddItem(RigidMeshNode);
				break;
			}
		}

		// if there is no outSkelMeshArray element that is bind to this skeleton
		// create new element for outSkelMeshArray
		if ( i == SkeletonArray.Num() )
		{
			TArray<FbxNode*>* TempArray = new TArray<FbxNode*>();
			TempArray->AddItem(RigidMeshNode);
			outSkelMeshArray.AddItem(TempArray);
			SkeletonArray.AddItem(Link);
		}
	}

	// for LODGroup, we will not deep in.
	if (!(Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP))
	{
		INT ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			RecursiveFindRigidMesh(Node->GetChild(ChildIndex), outSkelMeshArray, SkeletonArray, ExpandLOD);
		}
	}
}

/**
* Get all Fbx skeletal mesh objects in the scene. these meshes are grouped by skeleton they bind to
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
*/
void CFbxImporter::FillFbxSkelMeshArrayInScene(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, UBOOL ExpandLOD)
{
	TArray<FbxNode*> SkeletonArray;

	// a) find skeletal meshes
	
	RecursiveFindFbxSkelMesh(Node, outSkelMeshArray, SkeletonArray, ExpandLOD);
	// for skeletal mesh, we convert the skeleton system to skeleton
	// in less we recognize bone mesh as rigid mesh if they are textured
	for ( INT SkelIndex = 0; SkelIndex < SkeletonArray.Num(); SkelIndex++)
	{
		RecursiveFixSkeleton(SkeletonArray(SkelIndex), *outSkelMeshArray(SkelIndex), ImportOptions->bImportMeshesInBoneHierarchy );
	}

	SkeletonArray.Empty();
	// b) find rigid mesh
	
	// for rigid meshes, we don't convert to bone
	if (ImportOptions->bImportRigidAnim)
	{
		RecursiveFindRigidMesh(Node, outSkelMeshArray, SkeletonArray, ExpandLOD);
	}
}

FbxNode* CFbxImporter::FindFBXMeshesByBone(USkeletalMesh* FillInMesh, UBOOL bExpandLOD, TArray<FbxNode*>& OutFBXMeshNodeArray)
{
	// get the root bone of Unreal skeletal mesh
	FMeshBone& Bone = FillInMesh->RefSkeleton(0);
	FString BoneNameString = Bone.Name.ToString();

	// we do not need to check if the skeleton root node is a skeleton
	// because the animation may be a rigid animation
	FbxNode* SkeletonRoot = NULL;

	// find the FBX skeleton node according to name
	SkeletonRoot = Scene->FindNodeByName(TCHAR_TO_ANSI(*BoneNameString));

	// SinceFBX bone names are changed on import, it's possible that the 
	// bone name in the engine doesn't match that of the one in the FBX file and
	// would not be found by FindNodeByName().  So apply the same changes to the 
	// names of the nodes before checking them against the name of the Unreal bone
	if (!SkeletonRoot)
	{
		ANSICHAR TmpBoneName[64];

		for (INT NodeIndex = 0; NodeIndex < Scene->GetNodeCount(); NodeIndex++)
		{
			FbxNode* FbxNode = Scene->GetNode(NodeIndex);

			strcpy_s(TmpBoneName, 64, MakeName(FbxNode->GetName()));
			FString FbxBoneName = FSkeletalMeshBinaryImport::FixupBoneName(TmpBoneName);

			if (FbxBoneName == BoneNameString)
			{
				SkeletonRoot = FbxNode;
				break;
			}
		}
	}


	// return if do not find matched FBX skeleton
	if (!SkeletonRoot)
	{
		return NULL;
	}
	

	// Get Mesh nodes array that bind to the skeleton system
	// 1, get all skeltal meshes in the FBX file
	TArray< TArray<FbxNode*>* > SkelMeshArray;
	FillFbxSkelMeshArrayInScene(Scene->GetRootNode(), SkelMeshArray, FALSE);

	// 2, then get skeletal meshes that bind to this skeleton
	for (INT SkelMeshIndex = 0; SkelMeshIndex < SkelMeshArray.Num(); SkelMeshIndex++)
	{
		FbxNode* MeshNode;
		{
			FbxNode* Node = (*SkelMeshArray(SkelMeshIndex))(0);
			if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
			{
				MeshNode = Node->GetChild(0);
			}
			else
			{
				MeshNode = Node;
			}
		}
		
		// 3, get the root bone that the mesh bind to
		FbxSkin* Deformer = (FbxSkin*)MeshNode->GetMesh()->GetDeformer(0, FbxDeformer::eSKIN);
		FbxNode* Link = Deformer->GetCluster(0)->GetLink();
		Link = GetRootSkeleton(Link);
		// 4, fill in the mesh node
		if (Link == SkeletonRoot)
		{
			// copy meshes
			if (bExpandLOD)
			{
				TArray<FbxNode*> SkelMeshes = 	*SkelMeshArray(SkelMeshIndex);
				for (INT NodeIndex = 0; NodeIndex < SkelMeshes.Num(); NodeIndex++)
				{
					FbxNode* Node = SkelMeshes(NodeIndex);
					if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
					{
						OutFBXMeshNodeArray.AddItem(Node->GetChild(0));
					}
					else
					{
						OutFBXMeshNodeArray.AddItem(Node);
					}
				}
			}
			else
			{
				OutFBXMeshNodeArray.Append(*SkelMeshArray(SkelMeshIndex));
			}
			break;
		}
	}

	for (INT i = 0; i < SkelMeshArray.Num(); i++)
	{
		delete SkelMeshArray(i);
	}

	return SkeletonRoot;
}

/**
* Get the first Fbx mesh node.
*
* @param Node Root node
* @param bIsSkelMesh if we want a skeletal mesh
* @return KFbxNode* the node containing the first mesh
*/
FbxNode* GetFirstFbxMesh(FbxNode* Node, UBOOL bIsSkelMesh)
{
	if (Node->GetMesh())
	{
		if (bIsSkelMesh)
		{
			if (Node->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN)>0)
			{
				return Node;
			}
		}
		else
		{
			return Node;
		}
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FbxNode* FirstMesh;
		FirstMesh = GetFirstFbxMesh(Node->GetChild(ChildIndex), bIsSkelMesh);

		if (FirstMesh)
		{
			return FirstMesh;
		}
	}

	return NULL;
}

void CFbxImporter::CheckSmoothingInfo(FbxMesh* FbxMesh)
{
	if (FbxMesh && bFirstMesh)
	{
		bFirstMesh = FALSE;	 // don't check again
		
		FbxLayer* LayerSmoothing = FbxMesh->GetLayer(0, FbxLayerElement::eSMOOTHING);
		if (!LayerSmoothing)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_NoSmoothgroupForFBXScene"));
		}
	}
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
FbxNode* CFbxImporter::RetrieveObjectFromName(const TCHAR* ObjectName, FbxNode* Root)
{
	FbxNode* Result = NULL;
	
	if ( Scene != NULL )
	{
		if (Root == NULL)
		{
			Root = Scene->GetRootNode();
		}

		for (INT ChildIndex=0;ChildIndex<Root->GetChildCount() && !Result;++ChildIndex)
		{
			FbxNode* Node = Root->GetChild(ChildIndex);
			FbxMesh* FbxMesh = Node->GetMesh();
			if (FbxMesh && 0 == appStrcmp(ObjectName,ANSI_TO_TCHAR(Node->GetName())))
			{
				Result = Node;
			}
			else
			{
				Result = RetrieveObjectFromName(ObjectName,Node);
			}
		}
	}
	return Result;
}

} // namespace UnFbx

#endif //WITH_FBX
