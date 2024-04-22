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
	Main implementation of FbxExporter : export FBX data from Unreal
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX

#include "UnFbxExporter.h"

namespace UnFbx
{

CFbxExporter::CFbxExporter()
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

	DefaultCamera = NULL;
}

CFbxExporter::~CFbxExporter()
{
	if (SdkManager)
	{
		SdkManager->Destroy();
		SdkManager = NULL;
	}
}

CFbxExporter* CFbxExporter::GetInstance()
{
	static CFbxExporter* ExporterInstance = NULL;

	if (ExporterInstance == NULL)
	{
		ExporterInstance = new CFbxExporter();
	}
	return ExporterInstance;
}

void CFbxExporter::CreateDocument()
{
	Scene = FbxScene::Create(SdkManager,"");
	
	// create scene info
	FbxDocumentInfo* SceneInfo = FbxDocumentInfo::Create(SdkManager,"SceneInfo");
	SceneInfo->mTitle = "Unreal Matinee Sequence";
	SceneInfo->mSubject = "Export Unreal Matinee";
	SceneInfo->mComment = "no particular comments required.";

	Scene->SetSceneInfo(SceneInfo);
	
	//FbxScene->GetGlobalSettings().SetOriginalUpAxis(KFbxAxisSystem::Max);
	FbxAxisSystem::eFrontVector FrontVector = (FbxAxisSystem::eFrontVector)-FbxAxisSystem::ParityOdd;
	const FbxAxisSystem UnrealZUp(FbxAxisSystem::ZAxis, FrontVector, FbxAxisSystem::RightHanded);
	Scene->GetGlobalSettings().SetAxisSystem(UnrealZUp);
	Scene->GetGlobalSettings().SetOriginalUpAxis(UnrealZUp);
	// Maya use cm by default
	Scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::cm);
	//FbxScene->GetGlobalSettings().SetOriginalSystemUnit( KFbxSystemUnit::m );
	
	// setup anim stack
	AnimStack = FbxAnimStack::Create(Scene, "Unreal Matinee Take");
	//KFbxSet<KTime>(AnimStack->LocalStart, KTIME_ONE_SECOND);
	AnimStack->Description.Set("Animation Take for Unreal Matinee.");

	// this take contains one base layer. In fact having at least one layer is mandatory.
	AnimLayer = FbxAnimLayer::Create(Scene, "Base Layer");
	AnimStack->AddMember(AnimLayer);
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif

void CFbxExporter::WriteToFile(const TCHAR* Filename)
{
	INT Major, Minor, Revision;
	UBOOL Status = TRUE;

	INT FileFormat = -1;
	bool bEmbedMedia = false;

	// Create an exporter.
	FbxExporter* Exporter = FbxExporter::Create(SdkManager, "");

	// set file format
	if( FileFormat < 0 || FileFormat >= SdkManager->GetIOPluginRegistry()->GetWriterFormatCount() )
	{
		// Write in fall back format if pEmbedMedia is true
		FileFormat = SdkManager->GetIOPluginRegistry()->GetNativeWriterFormat();
	}

	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.

	IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,        true);
	IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,         true);
	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,        bEmbedMedia);
	IOS_REF.SetBoolProp(EXP_FBX_SHAPE,           true);
	IOS_REF.SetBoolProp(EXP_FBX_GOBO,            true);
	IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,       true);
	IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

	// Initialize the exporter by providing a filename.
	if( !Exporter->Initialize(TCHAR_TO_ANSI(Filename), FileFormat, SdkManager->GetIOSettings()) )
	{
		warnf(NAME_Log, TEXT("Call to KFbxExporter::Initialize() failed.\n"));
		warnf(NAME_Log, TEXT("Error returned: %s\n\n"), Exporter->GetLastErrorString());
		return;
	}

	FbxManager::GetFileFormatVersion(Major, Minor, Revision);
	warnf(NAME_Log, TEXT("FBX version number for this version of the FBX SDK is %d.%d.%d\n\n"), Major, Minor, Revision);

	// Export the scene.
	Status = Exporter->Export(Scene); 

	// Destroy the exporter.
	Exporter->Destroy();
	
	CloseDocument();
	
	return;
}

/**
 * Release the FBX scene, releasing its memory.
 */
void CFbxExporter::CloseDocument()
{
	FbxActors.Reset();
	FbxMaterials.Reset();
	FbxNodeNameToIndexMap.Reset();
	
	if (Scene)
	{
		Scene->Destroy();
		Scene = NULL;
	}
}

void CFbxExporter::CreateAnimatableUserProperty(FbxNode* Node, FLOAT Value, const char* Name, const char* Label)
{
	// Add one user property for recording the animation
	FbxProperty IntensityProp = FbxProperty::Create(Node, FbxFloatDT, Name, Label);
	IntensityProp.Set(Value);
	IntensityProp.ModifyFlag(FbxPropertyAttr::eUser, true);
	IntensityProp.ModifyFlag(FbxPropertyAttr::eAnimatable, true);
}

/**
 * Exports the basic scene information to the FBX document.
 */
void CFbxExporter::ExportLevelMesh(ULevel* Level, USeqAct_Interp* MatineeSequence )
{
	if (Level == NULL) return;

	// Exports the level's scene geometry
	// the vertex number of Model must be more than 2 (at least a triangle panel)
	if (Level->Model != NULL && Level->Model->VertexBuffer.Vertices.Num() > 2 && Level->Model->MaterialIndexBuffers.Num() > 0)
	{
		// create a KFbxNode
		FbxNode* Node = FbxNode::Create(Scene,"LevelMesh");

		// set the shading mode to view texture
		Node->SetShadingMode(FbxNode::eTEXTURE_SHADING);
		Node->LclScaling.Set(FbxVector4(1.0, 1.0, 1.0));
		
		Scene->GetRootNode()->AddChild(Node);

		// Export the mesh for the world
		ExportModel(Level->Model, Node, "Level Mesh");
	}

	// Export all the recognized global actors.
	// Right now, this only includes lights.
	INT ActorCount = GWorld->CurrentLevel->Actors.Num();
	for (INT ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		AActor* Actor = GWorld->CurrentLevel->Actors(ActorIndex);
		if (Actor != NULL)
		{
			if (Actor->IsA(ALight::StaticClass()))
			{
				ExportLight((ALight*) Actor, MatineeSequence );
			}
			else if (Actor->IsA(AStaticMeshActor::StaticClass()))
			{
				ExportStaticMesh( Actor, ((AStaticMeshActor*) Actor)->StaticMeshComponent, MatineeSequence );
			}
			else if (Actor->IsA(ADynamicSMActor::StaticClass()))
			{
				ExportStaticMesh( Actor, ((ADynamicSMActor*) Actor)->StaticMeshComponent, MatineeSequence );
			}
			else if (Actor->IsA(ABrush::StaticClass()))
			{
				// All brushes should be included within the world geometry exported above.
				ExportBrush((ABrush*) Actor, FALSE, NULL );
			}
			else if (Actor->IsA(ATerrain::StaticClass()))
			{
				// ExportTerrain?((ATerrain*) Actor);
			}
			else if (Actor->IsA(AEmitter::StaticClass()))
			{
				ExportActor( Actor, MatineeSequence ); // Just export the placement of the particle emitter.
			}
		}
	}
}

/**
 * Exports the light-specific information for a UE3 light actor.
 */
void CFbxExporter::ExportLight( ALight* Actor, USeqAct_Interp* MatineeSequence )
{
	if (Scene == NULL || Actor == NULL || Actor->LightComponent == NULL) return;

	// Export the basic actor information.
	FbxNode* FbxActor = ExportActor( Actor, MatineeSequence ); // this is the pivot node
	// The real fbx light node
	FbxNode* FbxLightNode = FbxActor->GetParent();

	ULightComponent* BaseLight = Actor->LightComponent;

	FString FbxNodeName = GetActorNodeName(Actor, MatineeSequence);

	// Export the basic light information
	FbxLight* Light = FbxLight::Create(Scene, TCHAR_TO_ANSI(*FbxNodeName));
	Light->Intensity.Set(BaseLight->Brightness * 100);
	Light->Color.Set(Converter.ConvertToFbxColor(BaseLight->LightColor));
	
	// Add one user property for recording the Brightness animation
	CreateAnimatableUserProperty(FbxLightNode, BaseLight->Brightness, "UE_Intensity", "UE_Matinee_Light_Intensity");
	
	// Look for the higher-level light types and determine the lighting method
	if (BaseLight->IsA(UPointLightComponent::StaticClass()))
	{
		UPointLightComponent* PointLight = (UPointLightComponent*) BaseLight;
		if (BaseLight->IsA(USpotLightComponent::StaticClass()))
		{
			USpotLightComponent* SpotLight = (USpotLightComponent*) BaseLight;
			Light->LightType.Set(FbxLight::eSPOT);

			// Export the spot light parameters.
			if (!appIsNearlyZero(SpotLight->InnerConeAngle))
			{
				Light->InnerAngle.Set(SpotLight->InnerConeAngle);
			}
			else // Maya requires a non-zero inner cone angle
			{
				Light->InnerAngle.Set(0.01f);
			}
			Light->OuterAngle.Set(SpotLight->OuterConeAngle);
		}
		else
		{
			Light->LightType.Set(FbxLight::ePOINT);
		}
		
		// Export the point light parameters.
		Light->EnableFarAttenuation.Set(true);
		Light->FarAttenuationEnd.Set(PointLight->Radius);
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxLightNode, PointLight->Radius, "UE_Radius", "UE_Matinee_Light_Radius");
		
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxLightNode, PointLight->FalloffExponent, "UE_FalloffExponent", "UE_Matinee_Light_FalloffExponent");
	}
	else if (BaseLight->IsA(UDirectionalLightComponent::StaticClass()))
	{
		// The directional light has no interesting properties.
		Light->LightType.Set(FbxLight::eDIRECTIONAL);
	}
	
	FbxActor->SetNodeAttribute(Light);
}

void CFbxExporter::ExportCamera( ACameraActor* Actor, USeqAct_Interp* MatineeSequence )
{
	if (Scene == NULL || Actor == NULL) return;

	// Export the basic actor information.
	FbxNode* FbxActor = ExportActor( Actor, MatineeSequence ); // this is the pivot node
	// The real fbx camera node
	FbxNode* FbxCameraNode = FbxActor->GetParent();

	FString FbxNodeName = GetActorNodeName(Actor, NULL);

	// Create a properly-named FBX camera structure and instantiate it in the FBX scene graph
	FbxCamera* Camera = FbxCamera::Create(Scene, TCHAR_TO_ANSI(*FbxNodeName));

	// Export the view area information
	Camera->ProjectionType.Set(FbxCamera::ePERSPECTIVE);
	Camera->SetAspect(FbxCamera::eFIXED_RATIO, Actor->AspectRatio, 1.0f);
	Camera->FilmAspectRatio.Set(Actor->AspectRatio);
	Camera->SetApertureWidth(Actor->AspectRatio * 0.612f); // 0.612f is a magic number from Maya that represents the ApertureHeight
	Camera->SetApertureMode(FbxCamera::eFOCAL_LENGTH);
	Camera->FocalLength.Set(Camera->ComputeFocalLength(Actor->FOVAngle));
	
	// Add one user property for recording the AspectRatio animation
	CreateAnimatableUserProperty(FbxCameraNode, Actor->AspectRatio, "UE_AspectRatio", "UE_Matinee_Camera_AspectRatio");

	// Push the near/far clip planes away, as the UE3 engine uses larger values than the default.
	Camera->SetNearPlane(10.0f);
	Camera->SetFarPlane(100000.0f);

	// Export the post-processing information: only one of depth-of-field or motion blur can be supported in 3dsMax.
	// And Maya only supports some simple depth-of-field effect.
	FPostProcessSettings* PostProcess = &Actor->CamOverridePostProcess;
	if (PostProcess->bEnableDOF)
	{
		// Export the depth-of-field information
		Camera->UseDepthOfField.Set(TRUE);
		
		// 'focal depth' <- 'focus distance'.
		if (PostProcess->DOF_FocusType == FOCUS_Distance)
		{
			Camera->FocusSource.Set(FbxCamera::eSPECIFIC_DISTANCE);
			Camera->FocusDistance.Set(PostProcess->DOF_FocusDistance);
		}
		else if (PostProcess->DOF_FocusType == FOCUS_Position)
		{
			Camera->FocusSource.Set(FbxCamera::eSPECIFIC_DISTANCE);
			Camera->FocusDistance.Set((Actor->Location - PostProcess->DOF_FocusPosition).Size());
		}
		
		// Add one user property for recording the UE_DOF_FocusDistance animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->DOF_FocusDistance, "UE_DOF_FocusDistance", "UE_Matinee_Camear_DOF_FocusDistance");

		// Add one user property for recording the DOF_FocusInnerRadius animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->DOF_FocusInnerRadius, "UE_DOF_FocusInnerRadius", "UE_Matinee_Camear_DOF_FocusInnerRadius");
		
		// Add one user property for recording the DOF_BlurKernelSize animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->DOF_BlurKernelSize, "UE_DOF_BlurKernelSize", "UE_Matinee_Camear_DOF_BlurKernelSize");
	}
	else if (PostProcess->bEnableMotionBlur)
	{
		Camera->UseMotionBlur.Set(TRUE);
		Camera->MotionBlurIntensity.Set(PostProcess->MotionBlur_Amount);
		// Add one user property for recording the MotionBlur_Amount animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->MotionBlur_Amount, "UE_MotionBlur_Amount", "UE_Matinee_Camear_MotionBlur_Amount");
	}
	
	FbxActor->SetNodeAttribute(Camera);

	DefaultCamera = Camera;
}

/**
 * Exports the mesh and the actor information for a UE3 brush actor.
 */
void CFbxExporter::ExportBrush(ABrush* Actor, UModel* InModel, UBOOL bConvertToStaticMesh )
{
	if (Scene == NULL || Actor == NULL || Actor->BrushComponent == NULL) return;

	if (!bConvertToStaticMesh)
	{
		// Retrieve the information structures, verifying the integrity of the data.
		UModel* Model = Actor->BrushComponent->Brush;

		if (Model == NULL || Model->VertexBuffer.Vertices.Num() < 3 || Model->MaterialIndexBuffers.Num() == 0) return;

		// Create the FBX actor, the FBX geometry and instantiate it.
		FbxNode* FbxActor = ExportActor( Actor, NULL );
		Scene->GetRootNode()->AddChild(FbxActor);

		// Export the mesh information
		ExportModel(Model, FbxActor, TCHAR_TO_ANSI(*Actor->GetName()));
	}
	else
	{
		// Convert the brush to a static mesh then export that static mesh
		TArray<FStaticMeshTriangle>	Triangles;
		TArray<FStaticMeshElement>	Materials;

		GetBrushTriangles( Triangles, Materials, Actor, InModel ? InModel : Actor->Brush );

		if( Triangles.Num() )
		{
			UStaticMesh* NewMesh = CreateStaticMesh(Triangles,Materials,UObject::GetTransientPackage(),Actor->GetPureName());

			ExportStaticMesh(NewMesh);
		}
	}
}

void CFbxExporter::ExportModel(UModel* Model, FbxNode* Node, const char* Name)
{
	//INT VertexCount = Model->VertexBuffer.Vertices.Num();
	INT MaterialCount = Model->MaterialIndexBuffers.Num();

	const FLOAT BiasedHalfWorldExtent = HALF_WORLD_MAX * 0.95f;

	// Create the mesh and three data sources for the vertex positions, normals and texture coordinates.
	FbxMesh* Mesh = FbxMesh::Create(Scene, Name);
	
	// Create control points.
	UINT VertCount(Model->VertexBuffer.Vertices.Num());
	Mesh->InitControlPoints(VertCount);
	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	
	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}
	
	// We want to have one normal for each vertex (or control point),
	// so we set the mapping mode to eBY_CONTROL_POINT.
	FbxLayerElementNormal* LayerElementNormal= FbxLayerElementNormal::Create(Mesh, "");

	LayerElementNormal->SetMappingMode(FbxLayerElement::eBY_CONTROL_POINT);

	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDIRECT);
	
	// Create UV for Diffuse channel.
	FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, "DiffuseUV");
	UVDiffuseLayer->SetMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
	UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eDIRECT);
	Layer->SetUVs(UVDiffuseLayer, FbxLayerElement::eDIFFUSE_TEXTURES);
	
	for (UINT VertexIdx = 0; VertexIdx < VertCount; ++VertexIdx)
	{
		FModelVertex& Vertex = Model->VertexBuffer.Vertices(VertexIdx);
		FVector Normal = (FVector) Vertex.TangentZ;

		// If the vertex is outside of the world extent, snap it to the origin.  The faces associated with
		// these vertices will be removed before exporting.  We leave the snapped vertex in the buffer so
		// we won't have to deal with reindexing everything.
		FVector FinalVertexPos = Vertex.Position;
		if( Abs( Vertex.Position.X ) > BiasedHalfWorldExtent ||
			Abs( Vertex.Position.Y ) > BiasedHalfWorldExtent ||
			Abs( Vertex.Position.Z ) > BiasedHalfWorldExtent )
		{
			FinalVertexPos = FVector( 0.0f, 0.0f, 0.0f );
		}

		ControlPoints[VertexIdx] = FbxVector4(FinalVertexPos.X, -FinalVertexPos.Y, FinalVertexPos.Z);
		FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxAMatrix NodeMatrix;
		FbxVector4 Trans = Node->LclTranslation.Get();
		NodeMatrix.SetT(FbxVector4(Trans[0], Trans[1], Trans[2]));
		FbxVector4 Rot = Node->LclRotation.Get();
		NodeMatrix.SetR(FbxVector4(Rot[0], Rot[1], Rot[2]));
		NodeMatrix.SetS(Node->LclScaling.Get());
		FbxNormal = NodeMatrix.MultT(FbxNormal);
		FbxNormal.Normalize();
		LayerElementNormal->GetDirectArray().Add(FbxNormal);
		
		// update the index array of the UVs that map the texture to the face
		UVDiffuseLayer->GetDirectArray().Add(FbxVector2(Vertex.TexCoord.X, -Vertex.TexCoord.Y));
	}
	
	Layer->SetNormals(LayerElementNormal);
	Layer->SetUVs(UVDiffuseLayer);
	
	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eBY_POLYGON);
	MatLayer->SetReferenceMode(FbxLayerElement::eINDEX_TO_DIRECT);
	Layer->SetMaterials(MatLayer);
	
	// Create the materials and the per-material tesselation structures.
	TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator MaterialIterator(Model->MaterialIndexBuffers);
	for (; MaterialIterator; ++MaterialIterator)
	{
		UMaterialInterface* MaterialInterface = MaterialIterator.Key();
		FRawIndexBuffer16or32& IndexBuffer = *MaterialIterator.Value();
		INT IndexCount = IndexBuffer.Indices.Num();
		if (IndexCount < 3) continue;
		
		// Are NULL materials okay?
		INT MaterialIndex = -1;
		FbxSurfaceMaterial* FbxMaterial;
		if (MaterialInterface != NULL && MaterialInterface->GetMaterial() != NULL)
		{
			FbxMaterial = ExportMaterial(MaterialInterface->GetMaterial());
		}
		else
		{
			// Set default material
			FbxMaterial = CreateDefaultMaterial();
		}
		MaterialIndex = Node->AddMaterial(FbxMaterial);

		// Create the Fbx polygons set.

		// Retrieve and fill in the index buffer.
		const INT TriangleCount = IndexCount / 3;
		for( INT TriangleIdx = 0; TriangleIdx < TriangleCount; ++TriangleIdx )
		{
			UBOOL bSkipTriangle = FALSE;

			for( INT IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
			{
				// Skip triangles that belong to BSP geometry close to the world extent, since its probably
				// the automatically-added-brush for new levels.  The vertices will be left in the buffer (unreferenced)
				FVector VertexPos = Model->VertexBuffer.Vertices( IndexBuffer.Indices( TriangleIdx * 3 + IndexIdx ) ).Position;
				if( Abs( VertexPos.X ) > BiasedHalfWorldExtent ||
					Abs( VertexPos.Y ) > BiasedHalfWorldExtent ||
					Abs( VertexPos.Z ) > BiasedHalfWorldExtent )
				{
					bSkipTriangle = TRUE;
					break;
				}
			}

			if( !bSkipTriangle )
			{
				// all faces of the cube have the same texture
				Mesh->BeginPolygon(MaterialIndex);
				for( INT IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
				{
					// Control point index
					Mesh->AddPolygon(IndexBuffer.Indices( TriangleIdx * 3 + IndexIdx ));

				}
				Mesh->EndPolygon ();
			}
		}
	}
	
	Node->SetNodeAttribute(Mesh);
}

void CFbxExporter::ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, USeqAct_Interp* MatineeSequence )
{
	if (Scene == NULL || Actor == NULL || StaticMeshComponent == NULL) return;

	// Retrieve the static mesh rendering information at the correct LOD level.
	UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
	if (StaticMesh == NULL || StaticMesh->LODModels.Num() == 0) return;
	INT LODIndex = StaticMeshComponent->ForcedLodModel;
	if (LODIndex >= StaticMesh->LODModels.Num())
	{
		LODIndex = StaticMesh->LODModels.Num() - 1;
	}
	FStaticMeshRenderData* RenderMesh = StaticMesh->GetLODForExport(LODIndex);
	if (RenderMesh)
	{
		FString FbxNodeName = GetActorNodeName(Actor, MatineeSequence);

		FColorVertexBuffer* ColorBuffer = NULL;

		if (LODIndex < StaticMeshComponent->LODData.Num())
		{
			ColorBuffer = StaticMeshComponent->LODData(LODIndex).OverrideVertexColors;
		}

		FbxNode* FbxActor = ExportActor( Actor, MatineeSequence );
		ExportStaticMeshToFbx(*RenderMesh, *FbxNodeName, FbxActor, -1, ColorBuffer);
	}
}


INT FindMaterialIndex(TArray<FStaticMeshElement>& Materials, UMaterialInterface* Material)
{
	for(INT MaterialIndex = 0;MaterialIndex < Materials.Num();MaterialIndex++)
	{
		if(Materials(MaterialIndex).Material == Material)
		{
			return MaterialIndex;
		}
	}

	const INT NewMaterialIndex = Materials.Num();
	new(Materials) FStaticMeshElement(Material,NewMaterialIndex);

	return NewMaterialIndex;
}

struct FBSPExportData
{
	TArray<FStaticMeshTriangle> Triangles;
	TArray<FStaticMeshElement> Materials;
};

void CFbxExporter::ExportBSP( UModel* Model, UBOOL bSelectedOnly )
{
	TMap< ABrush*, FBSPExportData > BrushToTriMap;

	TArray<UMaterialInterface*> AllMaterials;

	for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Model->Nodes(NodeIndex);
		FBspSurf& Surf = Model->Surfs(Node.iSurf);
		
		ABrush* BrushActor = Surf.Actor;

		if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly || BrushActor->IsSelected() )
		{
			FPoly Poly;
			GEditor->polyFindMaster( Model, Node.iSurf, Poly );

			FBSPExportData& Data = BrushToTriMap.FindOrAdd( Surf.Actor );

			TArray<FStaticMeshTriangle>& Triangles = Data.Triangles;
			TArray<FStaticMeshElement>& Materials = Data.Materials;

			UMaterialInterface*	Material = Poly.Material;

			AllMaterials.AddUniqueItem( Material );

			// Find a material index for this polygon.
			INT	MaterialIndex = FindMaterialIndex(Materials,Material);

			const FVector& TextureBase = Model->Points(Surf.pBase);
			const FVector& TextureX = Model->Vectors(Surf.vTextureU);
			const FVector& TextureY = Model->Vectors(Surf.vTextureV);
			const FVector& Normal = Model->Vectors(Surf.vNormal);

			for(INT StartVertexIndex = 1;StartVertexIndex < Node.NumVertices-1;StartVertexIndex++)
			{
				// These map the node's vertices to the 3 triangle indices to triangulate the convex polygon.
				INT TriVertIndices[3] = {	Node.iVertPool + StartVertexIndex + 1,
											Node.iVertPool + StartVertexIndex,
											Node.iVertPool };

				FStaticMeshTriangle*	Triangle = new(Triangles) FStaticMeshTriangle;

				Triangle->MaterialIndex = MaterialIndex;
				Triangle->FragmentIndex = 0;

				Triangle->SmoothingMask = 1<<(Node.iSurf%32);
				Triangle->NumUVs = 2;

				Triangle->bExplicitNormals = TRUE;

				for(UINT TriVertexIndex = 0; TriVertexIndex < 3; TriVertexIndex++)
				{
					const FVert& Vert = Model->Verts(TriVertIndices[TriVertexIndex]);
					const FVector& Vertex = Model->Points(Vert.pVertex);

					FLOAT U = ((Vertex - TextureBase) | TextureX) / 128.0f;
					FLOAT V = ((Vertex - TextureBase) | TextureY) / 128.0f;

					Triangle->Vertices[TriVertexIndex] = Vertex;
					Triangle->UVs[TriVertexIndex][0] = FVector2D( U, V );
					Triangle->UVs[TriVertexIndex][1] = Vert.ShadowTexCoord;
					Triangle->Colors[TriVertexIndex] = FColor(255,255,255,255);
					Triangle->TangentZ[TriVertexIndex] = Normal;
				}
			}
		}
	}

	// Export each mesh
	for( TMap< ABrush*, FBSPExportData >::TIterator It(BrushToTriMap); It; ++It )
	{
		FBSPExportData& Data = It.Value();
		if( Data.Triangles.Num() )
		{
			UStaticMesh* NewMesh = CreateStaticMesh( Data.Triangles, Data.Materials, UObject::GetTransientPackage(), It.Key()->GetFName() );

			ExportStaticMesh( NewMesh, &AllMaterials );
		}
	}
}

void CFbxExporter::ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>* MaterialOrder )
{
	if (Scene == NULL || StaticMesh == NULL) return;
	FString MeshName;
	StaticMesh->GetName(MeshName);
	FStaticMeshRenderData* RenderMesh = StaticMesh->GetLODForExport(0);
	if (RenderMesh)
	{
		FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*MeshName));
		Scene->GetRootNode()->AddChild(MeshNode);
		ExportStaticMeshToFbx(*RenderMesh, *MeshName, MeshNode, -1, NULL, MaterialOrder );
	}
}

void CFbxExporter::ExportStaticMeshLightMap( UStaticMesh* StaticMesh, INT LODIndex, INT UVChannel )
{
	if (Scene == NULL || StaticMesh == NULL) return;

	FString MeshName;
	StaticMesh->GetName(MeshName);
	FStaticMeshRenderData* RenderMesh = StaticMesh->GetLODForExport(LODIndex);
	if (RenderMesh)
	{
		FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*MeshName));
		Scene->GetRootNode()->AddChild(MeshNode);
		ExportStaticMeshToFbx(*RenderMesh, *MeshName, MeshNode, UVChannel);
	}
}

void CFbxExporter::ExportSkeletalMesh( USkeletalMesh* SkeletalMesh )
{
	if (Scene == NULL || SkeletalMesh == NULL) return;

	FString MeshName;
	SkeletalMesh->GetName(MeshName);

	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);

	ExportSkeletalMeshToFbx(*SkeletalMesh, *MeshName, MeshNode);
}

void CFbxExporter::ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent )
{
	if (Scene == NULL || Actor == NULL || SkeletalMeshComponent == NULL) return;

	// Retrieve the skeletal mesh rendering information.
	USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;

	FString FbxNodeName = GetActorNodeName(Actor, NULL);

	FbxNode* FbxActorNode = ExportActor( Actor, NULL );
	ExportSkeletalMeshToFbx(*SkeletalMesh, *FbxNodeName, FbxActorNode);
}

FbxSurfaceMaterial* CFbxExporter::CreateDefaultMaterial()
{
	FbxSurfaceMaterial* FbxMaterial = Scene->GetMaterial("Fbx Default Material");
	
	if (!FbxMaterial)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, "Fbx Default Material");
		((FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(FbxDouble3(0.72, 0.72, 0.72));
	}
	
	return FbxMaterial;
}

FbxDouble3 SetMaterialComponent(FColorMaterialInput& MatInput)
{
	FColor FinalColor;
	
	if (MatInput.Expression)
	{
		if (Cast<UMaterialExpressionConstant>(MatInput.Expression))
		{
			UMaterialExpressionConstant* Expr = Cast<UMaterialExpressionConstant>(MatInput.Expression);
			FinalColor = FColor(Expr->R);
		}
		else if (Cast<UMaterialExpressionVectorParameter>(MatInput.Expression))
		{
			UMaterialExpressionVectorParameter* Expr = Cast<UMaterialExpressionVectorParameter>(MatInput.Expression);
			FinalColor = Expr->DefaultValue;
		}
		else if (Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant3Vector* Expr = Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = Expr->B;
		}
		else if (Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant4Vector* Expr = Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = Expr->B;
			//FinalColor.A = Expr->A;
		}
		else if (Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant2Vector* Expr = Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = 0;
		}
		else
		{
			FinalColor.R = MatInput.Constant.R / 128.0;
			FinalColor.G = MatInput.Constant.G / 128.0;
			FinalColor.B = MatInput.Constant.B / 128.0;
		}
	}
	else
	{
		FinalColor.R = MatInput.Constant.R / 128.0;
		FinalColor.G = MatInput.Constant.G / 128.0;
		FinalColor.B = MatInput.Constant.B / 128.0;
	}
	
	return FbxDouble3(FinalColor.R, FinalColor.G, FinalColor.B);
}

/**
* Exports the profile_COMMON information for a UE3 material.
*/
FbxSurfaceMaterial* CFbxExporter::ExportMaterial(UMaterial* Material)
{
	if (Scene == NULL || Material == NULL) return NULL;
	
	// Verify that this material has not already been exported:
	if (FbxMaterials.Find(Material))
	{
		return *FbxMaterials.Find(Material);
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = NULL;
	
	// Set the lighting model
	if (Material->LightingModel == MLM_Phong || Material->LightingModel == MLM_Custom || Material->LightingModel == MLM_SHPRT)
	{
		FbxMaterial = FbxSurfacePhong::Create(Scene, TCHAR_TO_ANSI(*Material->GetName()));
		((FbxSurfacePhong*)FbxMaterial)->Specular.Set(SetMaterialComponent(Material->SpecularColor));
		((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else if (Material->LightingModel == MLM_NonDirectional)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, TCHAR_TO_ANSI(*Material->GetName()));
	}
	else // if (Material->LightingModel == MLM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, TCHAR_TO_ANSI(*Material->GetName()));
	}
	
	((FbxSurfaceLambert*)FbxMaterial)->Emissive.Set(SetMaterialComponent(Material->EmissiveColor));
	((FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(SetMaterialComponent(Material->DiffuseColor));
	((FbxSurfaceLambert*)FbxMaterial)->TransparencyFactor.Set(Material->Opacity.Constant);

	// Fill in the profile_COMMON effect with the UE3 material information.
	// TODO: Look for textures/constants in the Material expressions...
	
	FbxMaterials.Set(Material, FbxMaterial);
	
	return FbxMaterial;
}


/**
 * Exports the given Matinee sequence information into a FBX document.
 */
void CFbxExporter::ExportMatinee(USeqAct_Interp* MatineeSequence)
{
	if (MatineeSequence == NULL || Scene == NULL) return;

	// If the Matinee editor is not open, we need to initialize the sequence.
	UBOOL InitializeMatinee = MatineeSequence->InterpData == NULL;
	if (InitializeMatinee)
	{
		MatineeSequence->InitInterp();
	}

	// Iterate over the Matinee data groups and export the known tracks
	INT GroupCount = MatineeSequence->GroupInst.Num();
	for (INT GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		UInterpGroupInst* Group = MatineeSequence->GroupInst(GroupIndex);
		AActor* Actor = Group->GetGroupActor();
		if (Group->Group == NULL || Actor == NULL) continue;

		// Look for the class-type of the actor.
		if (Actor->IsA(ACameraActor::StaticClass()))
		{
			ExportCamera( (ACameraActor*) Actor, MatineeSequence );
		}

		FbxNode* FbxActor = ExportActor( Actor, MatineeSequence );

		// Look for the tracks that we currently support
		INT TrackCount = Min(Group->TrackInst.Num(), Group->Group->InterpTracks.Num());
		for (INT TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
		{
			UInterpTrackInst* TrackInst = Group->TrackInst(TrackIndex);
			UInterpTrack* Track = Group->Group->InterpTracks(TrackIndex);
			if (TrackInst->IsA(UInterpTrackInstMove::StaticClass()) && Track->IsA(UInterpTrackMove::StaticClass()))
			{
				UInterpTrackInstMove* MoveTrackInst = (UInterpTrackInstMove*) TrackInst;
				UInterpTrackMove* MoveTrack = (UInterpTrackMove*) Track;
				ExportMatineeTrackMove(FbxActor, MoveTrackInst, MoveTrack, MatineeSequence->InterpData->InterpLength);
			}
			else if (TrackInst->IsA(UInterpTrackInstFloatProp::StaticClass()) && Track->IsA(UInterpTrackFloatProp::StaticClass()))
			{
				UInterpTrackInstFloatProp* PropertyTrackInst = (UInterpTrackInstFloatProp*) TrackInst;
				UInterpTrackFloatProp* PropertyTrack = (UInterpTrackFloatProp*) Track;
				ExportMatineeTrackFloatProp(FbxActor, PropertyTrack);
			}
		}
	}

	if (InitializeMatinee)
	{
		MatineeSequence->TermInterp();
	}

	DefaultCamera = NULL;
}


/**
 * Exports a scene node with the placement indicated by a given UE3 actor.
 * This scene node will always have two transformations: one translation vector and one Euler rotation.
 */
FbxNode* CFbxExporter::ExportActor(AActor* Actor, USeqAct_Interp* MatineeSequence )
{
	// Verify that this actor isn't already exported, create a structure for it
	// and buffer it.
	FbxNode* ActorNode = FindActor(Actor);
	if (ActorNode == NULL)
	{
		FString FbxNodeName = GetActorNodeName(Actor, MatineeSequence);

		// See if a node with this name was already found
		// if so add and increment the number on the end of it
		// this seems to be what Collada does internally
		INT *NodeIndex = FbxNodeNameToIndexMap.Find( FbxNodeName );
		if( NodeIndex )
		{
			FbxNodeName = FString::Printf( TEXT("%s%d"), *FbxNodeName, *NodeIndex );
			++(*NodeIndex);
		}
		else
		{
			FbxNodeNameToIndexMap.Set( FbxNodeName, 1 );	
		}

		ActorNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*FbxNodeName));
		Scene->GetRootNode()->AddChild(ActorNode);

		FbxActors.Set(Actor, ActorNode);

		// Set the default position of the actor on the transforms
		// The UE3 transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		ActorNode->LclTranslation.Set(Converter.ConvertToFbxPos(Actor->Location));
		ActorNode->LclRotation.Set(Converter.ConvertToFbxRot(Actor->Rotation.Euler()));
		ActorNode->LclScaling.Set(Converter.ConvertToFbxScale(Actor->DrawScale * Actor->DrawScale3D));
	
		// For cameras and lights: always add a Y-pivot rotation to get the correct coordinate system.
		if (Actor->IsA(ACameraActor::StaticClass()) || Actor->IsA(ALight::StaticClass()))
		{
			FString FbxPivotNodeName = GetActorNodeName(Actor, NULL);

			if (FbxPivotNodeName == FbxNodeName)
			{
				FbxPivotNodeName += ANSI_TO_TCHAR("_pivot");
			}

			FbxNode* PivotNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*FbxPivotNodeName));
			PivotNode->LclRotation.Set(FbxVector4(90, 0, -90));

			if (Actor->IsA(ACameraActor::StaticClass()))
			{
				PivotNode->SetPostRotation(FbxNode::eSOURCE_SET, FbxVector4(0, -90, 0));
			}
			else if (Actor->IsA(ALight::StaticClass()))
			{
				PivotNode->SetPostRotation(FbxNode::eSOURCE_SET, FbxVector4(-90, 0, 0));
			}
			ActorNode->AddChild(PivotNode);

			ActorNode = PivotNode;
		}
	}

	return ActorNode;
}

/**
 * Exports the Matinee movement track into the FBX animation library.
 */
void CFbxExporter::ExportMatineeTrackMove(FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, FLOAT InterpLength)
{
	if (FbxActor == NULL || MoveTrack == NULL) return;
	
	// For the Y and Z angular rotations, we need to invert the relative animation frames,
	// While keeping the standard angles constant.

	if (MoveTrack != NULL)
	{
		FbxAnimLayer* BaseLayer = (FbxAnimLayer*)AnimStack->GetMember(FBX_TYPE(FbxAnimLayer), 0);
		FbxAnimCurve* Curve;

		UBOOL bPosCurve = TRUE;
		if( MoveTrack->SubTracks.Num() == 0 )
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, FALSE, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = FALSE;

			Curve = FbxActor->LclRotation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, TRUE, InterpLength);
		}
		else
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(0)), MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(1)), MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(2)), MoveTrackInst, bPosCurve, 2, FALSE, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = FALSE;

			Curve = FbxActor->LclRotation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(3)), MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(4)), MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve<FbxAnimCurve>(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(5)), MoveTrackInst, bPosCurve, 2, TRUE, InterpLength);
		}
	}
}

/**
 * Exports the Matinee float property track into the FBX animation library.
 */
void CFbxExporter::ExportMatineeTrackFloatProp(FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack)
{
	if (FbxActor == NULL || PropTrack == NULL) return;
	
	FbxNodeAttribute* FbxNodeAttr = NULL;
	// camera and light is appended on the fbx pivot node
	if( FbxActor->GetChild(0) )
	{
		FbxNodeAttr = ((FbxNode*)FbxActor->GetChild(0))->GetNodeAttribute();

		if (FbxNodeAttr == NULL) return;
	}
	
	FbxProperty Property;
	FString PropertyName = PropTrack->PropertyName.GetNameString();
	UBOOL IsFoV = FALSE;
	// most properties are created as user property, only FOV of camera in FBX supports animation
	if (PropertyName == "Brightness")
	{
		Property = FbxActor->FindProperty("UE_Intensity", false);
	}
	else if (PropertyName == "FalloffExponent")
	{
		Property = FbxActor->FindProperty("UE_FalloffExponent", false);
	}
	else if (PropertyName == "Radius")
	{
		Property = FbxActor->FindProperty("UE_Radius", false);
	}
	else if (PropertyName == "FOVAngle" && FbxNodeAttr )
	{
		Property = ((FbxCamera*)FbxNodeAttr)->FocalLength;
		IsFoV = TRUE;
	}
	else if (PropertyName == "AspectRatio")
	{
		Property = FbxActor->FindProperty("UE_AspectRatio", false);
	}
	else if (PropertyName == "DOF_FocusDistance")
	{
		Property = FbxActor->FindProperty("UE_DOF_FocusDistance", false);
	}
	else if (PropertyName == "DOF_FocusInnerRadius")
	{
		Property = FbxActor->FindProperty("UE_DOF_FocusInnerRadius", false);
	}
	else if (PropertyName == "DOF_BlurKernelSize")
	{
		Property = FbxActor->FindProperty("UE_DOF_BlurKernelSize", false);
	}
	else if (PropertyName == "MotionBlur_Amount")
	{
		Property = FbxActor->FindProperty("UE_MotionBlur_Amount", false);
	}

	if (Property != NULL)
	{
		ExportAnimatedFloat(&Property, &PropTrack->FloatTrack, IsFoV);
	}
}

void ConvertInterpToFBX(BYTE UnrealInterpMode, FbxAnimCurveDef::InterpolationType& Interpolation, FbxAnimCurveDef::TangentMode& Tangent)
{
	switch(UnrealInterpMode)
	{
	case CIM_Linear:
		Interpolation = FbxAnimCurveDef::eInterpolationLinear;
		Tangent = FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveAuto:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = FbxAnimCurveDef::eTangentAuto;
		break;
	case CIM_Constant:
		Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		Tangent = (FbxAnimCurveDef::TangentMode)FbxAnimCurveDef::eConstantStandard;
		break;
	case CIM_CurveUser:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveBreak:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (FbxAnimCurveDef::TangentMode) FbxAnimCurveDef::eTangentBreak;
		break;
	case CIM_CurveAutoClamped:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (FbxAnimCurveDef::TangentMode) (FbxAnimCurveDef::eTangentAuto | FbxAnimCurveDef::eTangentGenericClamp);
		break;
	case CIM_Unknown:  // ???
		FbxAnimCurveDef::InterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::TangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		break;
	}
}


// float-float comparison that allows for a certain error in the floating point values
// due to floating-point operations never being exact.
static bool IsEquivalent(FLOAT a, FLOAT b, FLOAT Tolerance = KINDA_SMALL_NUMBER)
{
	return (a - b) > -Tolerance && (a - b) < Tolerance;
}

// Set the default FPS to 30 because the SetupMatinee MEL script sets up Maya this way.
const FLOAT CFbxExporter::BakeTransformsFPS = 30;

/**
 * Exports a given interpolation curve into the FBX animation curve.
 */
void CFbxExporter::ExportAnimatedVector(FbxAnimCurve* FbxCurve, const char* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, UBOOL bPosCurve, INT CurveIndex, UBOOL bNegative, FLOAT InterpLength)
{
	if (Scene == NULL) return;
	
	FInterpCurveVector* Curve = bPosCurve ? &MoveTrack->PosTrack : &MoveTrack->EulerTrack;

	if (Curve == NULL || CurveIndex >= 3) return;

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	INT KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) + Curve->Points.Num() : Curve->Points.Num();

	// Write out the key times from the UE3 curve to the FBX curve.
	TArray<FLOAT> KeyTimes;
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		FLOAT KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Curve->Points(KeyIndex).InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes(KeyIndex-1) + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes(KeyIndex-1) + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.AddItem(KeyTime);
	}

	// Write out the key values from the UE3 curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			// If we are baking trnasforms, ask the movement track what are transforms are at the given time.
			if( bBakeKeys )
			{
				MoveTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes(KeyIndex), KeyPosition, KeyRotation);
			}
			// Else, this information is already present in the position and rotation tracks stored on the movement track.
			else
			{
				KeyPosition = MoveTrack->PosTrack.Points(KeyIndex).OutVal;
				KeyRotation = FRotator( FQuat::MakeFromEuler(MoveTrack->EulerTrack.Points(KeyIndex).OutVal) );
			}

			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			MoveTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		FLOAT KeyTime = KeyTimes(KeyIndex);
		FLOAT OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		FLOAT FbxKeyValue = bNegative ? -OutValue : OutValue;
		
		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);
		

		FbxAnimCurveDef::InterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::TangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		
		if( !bBakeKeys )
		{
			ConvertInterpToFBX(Curve->Points(KeyIndex).InterpMode, Interpolation, Tangent);
		}

		if (bBakeKeys || Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			FInterpCurvePoint<FVector>& Key = Curve->Points(KeyIndex);

			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( (Interpolation == FbxAnimCurveDef::eInterpolationCubic) )
			{
				FLOAT OutTangentValue = (CurveIndex == 0) ? Key.LeaveTangent.X : (CurveIndex == 1) ? Key.LeaveTangent.Y : Key.LeaveTangent.Z;
				FLOAT OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes(KeyIndex + 1) - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				FLOAT OutTangentY = OutTangentValue / 3.0f;
				FLOAT RightTangent =  OutTangentY / OutTangentX ;
				
				FLOAT NextLeftTangent = 0;
				
				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<FVector>& NextKey = Curve->Points(KeyIndex + 1);
					FLOAT NextInTangentValue = (CurveIndex == 0) ? NextKey.ArriveTangent.X : (CurveIndex == 1) ? NextKey.ArriveTangent.Y : NextKey.ArriveTangent.Z;
					FLOAT NextInTangentX;
					NextInTangentX = (KeyTimes(KeyIndex + 1) - KeyTimes(KeyIndex)) / 3.0f;
					FLOAT NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void CFbxExporter::ExportMoveSubTrack(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, UBOOL bPosCurve, INT CurveIndex, UBOOL bNegative, FLOAT InterpLength)
{
	if (Scene == NULL || FbxCurve == NULL) return;

	FInterpCurveFloat* Curve = &SubTrack->FloatTrack;
	UInterpTrackMove* ParentTrack = CastChecked<UInterpTrackMove>( SubTrack->GetOuter() );

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	INT KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) + Curve->Points.Num() : Curve->Points.Num();

	// Write out the key times from the UE3 curve to the FBX curve.
	TArray<FLOAT> KeyTimes;
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		FLOAT KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes(KeyIndex-1) + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes(KeyIndex-1) + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.AddItem(KeyTime);
	}

	// Write out the key values from the UE3 curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			ParentTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes(KeyIndex), KeyPosition, KeyRotation);
		
			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			ParentTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		FLOAT KeyTime = KeyTimes(KeyIndex);
		FLOAT OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		FLOAT FbxKeyValue = bNegative ? -OutValue : OutValue;

		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);

		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);

		FbxAnimCurveDef::InterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::TangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);

		if (bBakeKeys || Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( (Interpolation == FbxAnimCurveDef::eInterpolationCubic) )
			{
				FLOAT OutTangentValue = Key.LeaveTangent;
				FLOAT OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes(KeyIndex + 1) - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				FLOAT OutTangentY = OutTangentValue / 3.0f;
				FLOAT RightTangent =  OutTangentY / OutTangentX ;

				FLOAT NextLeftTangent = 0;

				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<FLOAT>& NextKey = Curve->Points(KeyIndex + 1);
					FLOAT NextInTangentValue =  Key.LeaveTangent;
					FLOAT NextInTangentX;
					NextInTangentX = (KeyTimes(KeyIndex + 1) - KeyTimes(KeyIndex)) / 3.0f;
					FLOAT NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void CFbxExporter::ExportAnimatedFloat(FbxProperty* FbxProperty, FInterpCurveFloat* Curve, UBOOL IsCameraFoV)
{
	if (FbxProperty == NULL || Curve == NULL) return;

	// do not export an empty anim curve
	if (Curve->Points.Num() == 0) return;

	FbxAnimCurve* AnimCurve = FbxAnimCurve::Create(Scene, "");
	FbxAnimCurveNode* CurveNode = FbxProperty->GetCurveNode(true);
	if (!CurveNode)
	{
		return;
	}
	CurveNode->SetChannelValue<double>(0U, Curve->Points(0).OutVal);
	CurveNode->ConnectToChannel(AnimCurve, 0U);

	// Write out the key times from the UE3 curve to the FBX curve.
	INT KeyCount = Curve->Points.Num();
	TArray<FLOAT> KeyTimes;
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		FLOAT KeyTime = Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes(KeyIndex-1) + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes(KeyIndex-1) + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.AddItem(KeyTime);
	}

	// Write out the key values from the UE3 curve to the FBX curve.
	AnimCurve->KeyModifyBegin();
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);
		FLOAT KeyTime = KeyTimes(KeyIndex);
		
		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = AnimCurve->KeyAdd(Time);
		float OutVal = (IsCameraFoV && DefaultCamera)? DefaultCamera->ComputeFocalLength(Key.OutVal): (float)Key.OutVal;

		FbxAnimCurveDef::InterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::TangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);
		
		if (Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			AnimCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves.
			FLOAT OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes(KeyIndex + 1) - KeyTime) / 3.0f : 0.333f;
			FLOAT OutTangentY = Key.LeaveTangent / 3.0f;
			FLOAT RightTangent =  OutTangentY / OutTangentX ;

			FLOAT NextLeftTangent = 0;

			if (KeyIndex < KeyCount - 1)
			{
				FInterpCurvePoint<FLOAT>& NextKey = Curve->Points(KeyIndex + 1);
				FLOAT NextInTangentX;
				NextInTangentX = (KeyTimes(KeyIndex + 1) - KeyTimes(KeyIndex)) / 3.0f;
				FLOAT NextInTangentY = NextKey.ArriveTangent / 3.0f;
				NextLeftTangent =  NextInTangentY / NextInTangentX ;
			}

			AnimCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent, RightTangent, NextLeftTangent );

		}
	}
	AnimCurve->KeyModifyEnd();
}

/**
 * Finds the given UE3 actor in the already-exported list of structures
 */
FbxNode* CFbxExporter::FindActor(AActor* Actor)
{
	if (FbxActors.Find(Actor))
	{
		return *FbxActors.Find(Actor);
	}
	else
	{
		return NULL;
	}
}

/**
 * Exports a static mesh
 * @param RenderMesh	The static mesh render data to export
 * @param MeshName		The name of the mesh for the FBX file
 * @param FbxActor		The fbx node representing the mesh
 * @param LightmapUVChannel Optional UV channel to export
 * @param ColorBuffer	Vertex color overrides to export
 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
 */
FbxNode* CFbxExporter::ExportStaticMeshToFbx(FStaticMeshRenderData& RenderMesh, const TCHAR* MeshName, FbxNode* FbxActor, INT LightmapUVChannel, FColorVertexBuffer* ColorBuffer, const TArray<UMaterialInterface*>* MaterialOrderOverride )
{
	// Verify the integrity of the static mesh.
	if (RenderMesh.VertexBuffer.GetNumVertices() == 0) return NULL;
	if (RenderMesh.Elements.Num() == 0) return NULL;

	FbxMesh* Mesh = FbxMesh::Create(Scene, TCHAR_TO_ANSI(MeshName));

	// Create and fill in the vertex position data source.
	// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
	const INT VertexCount = RenderMesh.VertexBuffer.GetNumVertices();
	
	Mesh->InitControlPoints(VertexCount); //TmpControlPoints.Num());
	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for (INT PosIndex = 0; PosIndex < VertexCount; ++PosIndex)
	{
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(PosIndex); //TmpControlPoints(PosIndex);
		ControlPoints[PosIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
	}
	
	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	FbxLayerElementNormal* LayerElementNormal= FbxLayerElementNormal::Create(Mesh, "");

	LayerElementNormal->SetMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDIRECT);
	for (INT NormalIndex = 0; NormalIndex < VertexCount; ++NormalIndex)
	{
		FVector Normal = (FVector) (RenderMesh.VertexBuffer.VertexTangentZ(NormalIndex));
		FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxNormal.Normalize();
		LayerElementNormal->GetDirectArray().Add(FbxNormal);
	}
	Layer->SetNormals(LayerElementNormal);

	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	INT TexCoordSourceCount = (LightmapUVChannel == -1)? RenderMesh.VertexBuffer.GetNumTexCoords(): LightmapUVChannel + 1;
	INT TexCoordSourceIndex = (LightmapUVChannel == -1)? 0: LightmapUVChannel;
	TCHAR UVChannelName[32];
	for (; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex)
	{
		FbxLayer* Layer = (LightmapUVChannel == -1)? Mesh->GetLayer(TexCoordSourceIndex): Mesh->GetLayer(0);
		if (Layer == NULL)
		{
			Mesh->CreateLayer();
			Layer = (LightmapUVChannel == -1)? Mesh->GetLayer(TexCoordSourceIndex): Mesh->GetLayer(0);
		}

		if ((LightmapUVChannel >= 0) || ((LightmapUVChannel == -1) && (TexCoordSourceIndex == 1)))
		{
			appSprintf(UVChannelName, TEXT("LightMapUV"));
		}
		else
		{
			appSprintf(UVChannelName, TEXT("DiffuseUV"));
		}			
		
		FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, TCHAR_TO_ANSI(UVChannelName));
		UVDiffuseLayer->SetMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
		UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eDIRECT);
		
		// Create the texture coordinate data source.
		for (INT TexCoordIndex = 0; TexCoordIndex < VertexCount; ++TexCoordIndex)
		{
			const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(TexCoordIndex, TexCoordSourceIndex);
			UVDiffuseLayer->GetDirectArray().Add(FbxVector2(TexCoord.X, -TexCoord.Y + 1.0));
		}
		
		Layer->SetUVs(UVDiffuseLayer, FbxLayerElement::eDIFFUSE_TEXTURES);
	}
	
	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eBY_POLYGON);
	MatLayer->SetReferenceMode(FbxLayerElement::eINDEX_TO_DIRECT);
	Layer->SetMaterials(MatLayer);
	
	// Create the per-material polygons sets.
	INT PolygonsCount = RenderMesh.Elements.Num();
	// Keep track of the number of tri's we export
	INT AccountedTriangles = 0;
	for (INT PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FStaticMeshElement& Polygons = RenderMesh.Elements(PolygonsIndex);

		FbxSurfaceMaterial* FbxMaterial = Polygons.Material ? ExportMaterial(Polygons.Material->GetMaterial()) : NULL;
		if (!FbxMaterial)
		{
			FbxMaterial = CreateDefaultMaterial();
		}
		INT MatIndex = FbxActor->AddMaterial(FbxMaterial);
		
		// Determine the actual material index
		INT ActualIndex = MatIndex;

		if( MaterialOrderOverride )
		{
			ActualIndex = MaterialOrderOverride->FindItemIndex( Polygons.Material );
		}
		// Static meshes contain one triangle list per element.
		// [GLAFORTE] Could it occasionally contain triangle strips? How do I know?
		INT TriangleCount = Polygons.NumTriangles;
		
		// Copy over the index buffer into the FBX polygons set.
		for (INT TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			Mesh->BeginPolygon(ActualIndex);
			for (INT PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				//Mesh->AddPolygon(ControlPointMap(RenderMesh.IndexBuffer.Indices(Polygons.FirstIndex + IndexIndex*3 + PointIndex)));
				Mesh->AddPolygon(RenderMesh.IndexBuffer.Indices(Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)));
			}
			Mesh->EndPolygon();
		}

		AccountedTriangles += TriangleCount;
	}

	// Throw a warning if this is a lightmap export and the exported poly count does not match the raw triangle data count
	if (LightmapUVChannel != -1 && AccountedTriangles != RenderMesh.RawTriangles.GetElementCount())
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("StaticMeshEditor_LightmapExportFewerTriangles"));
	}

	// Create and fill in the smoothing data source.
	FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create(Mesh, "");
	SmoothingInfo->SetMappingMode(FbxLayerElement::eBY_POLYGON);
	SmoothingInfo->SetReferenceMode(FbxLayerElement::eDIRECT);
	FbxLayerElementArrayTemplate<int>& SmoothingArray = SmoothingInfo->GetDirectArray();
	Layer->SetSmoothing(SmoothingInfo);

	INT TriangleCount = RenderMesh.RawTriangles.GetElementCount();
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock(LOCK_READ_ONLY);
	for( INT TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
	{
		FStaticMeshTriangle* Triangle = (RawTriangleData++);
		
		SmoothingArray.Add(Triangle->SmoothingMask);
	}
	RenderMesh.RawTriangles.Unlock();

	// Create and fill in the vertex color data source.
	FColorVertexBuffer* ColorBufferToUse = ColorBuffer? ColorBuffer: &RenderMesh.ColorVertexBuffer;
	INT ColorVertexCount = ColorBufferToUse->GetNumVertices();
	
	// Only export vertex colors if they exist
	if (ColorVertexCount > 0)
	{
		FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create(Mesh, "");
		VertexColor->SetMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
		VertexColor->SetReferenceMode(FbxLayerElement::eDIRECT);
		FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		Layer->SetVertexColors(VertexColor);

		for (INT VertIndex = 0; VertIndex < VertexCount; ++VertIndex)
		{
			FLinearColor VertColor(1.0f, 1.0f, 1.0f);
			
			if (VertIndex < ColorVertexCount)
			{
				VertColor = ColorBufferToUse->VertexColor(VertIndex).ReinterpretAsLinear();
			}

			VertexColorArray.Add( FbxColor(VertColor.R, VertColor.G, VertColor.B, VertColor.A ));
		}
	}

	FbxActor->SetNodeAttribute(Mesh);

	return FbxActor;
}


} // namespace UnFbx

#endif //WITH_FBX
