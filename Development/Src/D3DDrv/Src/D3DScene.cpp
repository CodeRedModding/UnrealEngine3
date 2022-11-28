/*=============================================================================
	D3DScene.cpp: Unreal Direct3D scene rendering.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	FD3DPrimitiveRenderInterface::FD3DPrimitiveRenderInterface
//

FD3DPrimitiveRenderInterface::FD3DPrimitiveRenderInterface(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass):
	Primitive(InPrimitive),
	Pass(InPass),
	Dirty(0)
{}

//
//	FD3DPrimitiveRenderInterface::GetViewport
//

FChildViewport* FD3DPrimitiveRenderInterface::GetViewport()
{
	return GD3DRenderInterface.CurrentViewport->Viewport;
}

//
//	FD3DPrimitiveRenderInterface::IsHitTesting
//

UBOOL FD3DPrimitiveRenderInterface::IsHitTesting()
{
	return GD3DRenderInterface.HitTesting;
}

//
//	FD3DPrimitiveRenderInterface::DrawWireframe
//

void FD3DPrimitiveRenderInterface::DrawWireframe(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,EWireframeType WireType,FColor Color,UINT FirstIndex,UINT NumPrimitives,UINT MinVertexIndex,UINT MaxVertexIndex,UBOOL DrawVertices)
{
	if(Pass != SP_OpaqueEmissive && Pass != SP_Unlit && Pass != SP_DepthSortedHitTest && Pass != SP_UnlitHitTest)
		return;

	FD3DVertexFactory*		CachedVertexFactory = (FD3DVertexFactory*)GD3DRenderDevice->Resources.GetCachedResource(VertexFactory);
	FD3DIndexBuffer*		CachedIndexBuffer = (FD3DIndexBuffer*)GD3DRenderDevice->Resources.GetCachedResource(IndexBuffer);

	UINT	BaseVertexIndex = CachedVertexFactory->Set(VertexFactory);

	if(GD3DRenderInterface.HitTesting)
	{
		FD3DWireframeHitProxyShader*	CachedShader = (FD3DWireframeHitProxyShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_WireframeHitProxy,GEngine->DefaultMaterial->GetMaterialInterface(0),VertexFactory->Type());
		CachedShader->Set(VertexFactory,GEngine->DefaultMaterial->GetInstanceInterface(),Color);
	}
	else
	{
		FD3DWireframeShader*	CachedShader = (FD3DWireframeShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_Wireframe,GEngine->DefaultMaterial->GetMaterialInterface(0),VertexFactory->Type());
		CachedShader->Set(VertexFactory,GEngine->DefaultMaterial->GetInstanceInterface(),Color);
	}

	GDirect3DDevice->SetIndices(CachedIndexBuffer->IndexBuffer);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	switch(WireType)
	{
	case WT_TriList:
		GDirect3DDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_WIREFRAME);
		GDirect3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,BaseVertexIndex,MinVertexIndex,MaxVertexIndex - MinVertexIndex + 1,FirstIndex,NumPrimitives);
		GDirect3DDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
		break;
	case WT_TriStrip:
		GDirect3DDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_WIREFRAME);
		GDirect3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,BaseVertexIndex,MinVertexIndex,MaxVertexIndex - MinVertexIndex + 1,FirstIndex,NumPrimitives);
		GDirect3DDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
		break;
	default:
		GDirect3DDevice->DrawIndexedPrimitive(D3DPT_LINELIST,BaseVertexIndex,MinVertexIndex,MaxVertexIndex - MinVertexIndex + 1,FirstIndex,NumPrimitives);
		break;
	}

#ifndef XBOX // POINTSIZE isn't supported on Xenon.
	if(DrawVertices)
	{
		FLOAT	PointSize = 3.0f;
		GDirect3DDevice->SetRenderState(D3DRS_POINTSCALEENABLE,FALSE);
		GDirect3DDevice->SetRenderState(D3DRS_POINTSIZE,*(DWORD*)&PointSize);

		GDirect3DDevice->DrawPrimitive(D3DPT_POINTLIST,BaseVertexIndex + MinVertexIndex,MaxVertexIndex - MinVertexIndex + 1);
	}
#endif

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);

	Dirty = 1;
}

//
//	FD3DPrimitiveRenderInterface::DrawSprite
//

void FD3DPrimitiveRenderInterface::DrawSprite(const FVector& Position,FLOAT SizeX,FLOAT SizeY,FTexture2D* Sprite,FColor Color)
{
	if(Pass != SP_OpaqueEmissive && Pass != SP_Unlit && Pass != SP_DepthSortedHitTest && Pass != SP_UnlitHitTest)
		return;

	if(GD3DRenderInterface.HitTesting)
		GD3DRenderDevice->SpriteHitProxyShader->Set(Sprite,Color);
	else
		GD3DRenderDevice->SpriteShader->Set(Sprite,Color);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	FD3DCanvasVertex Vertices[4];

	FVector WorldSpriteX = GD3DSceneRenderer->InvViewProjectionMatrix.TransformNormal(FVector(+1,0,0)).SafeNormal() * SizeX,
			WorldSpriteY = GD3DSceneRenderer->InvViewProjectionMatrix.TransformNormal(FVector(0,-1,0)).SafeNormal() * SizeY;

	Vertices[0].Position = GD3DSceneRenderer->ViewProjectionMatrix.TransformFPlane(FPlane(Position - WorldSpriteX - WorldSpriteY,1));
	Vertices[0].U = 0.0f;
	Vertices[0].V = 0.0f;

	Vertices[1].Position = GD3DSceneRenderer->ViewProjectionMatrix.TransformFPlane(FPlane(Position - WorldSpriteX + WorldSpriteY,1));
	Vertices[1].U = 0.0f;
	Vertices[1].V = 1.0f;

	Vertices[2].Position = GD3DSceneRenderer->ViewProjectionMatrix.TransformFPlane(FPlane(Position + WorldSpriteX + WorldSpriteY,1));
	Vertices[2].U = 1.0f;
	Vertices[2].V = 1.0f;

	Vertices[3].Position = GD3DSceneRenderer->ViewProjectionMatrix.TransformFPlane(FPlane(Position + WorldSpriteX - WorldSpriteY,1));
	Vertices[3].U = 1.0f;
	Vertices[3].V = 0.0f;

	GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
	GDirect3DDevice->DrawPrimitiveUP(
		D3DPT_TRIANGLEFAN,
		2,
		Vertices,
		sizeof(FD3DCanvasVertex)
		);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);

	Dirty = 1;
}

//
//	FD3DPrimitiveRenderInterface::DrawLine
//

void FD3DPrimitiveRenderInterface::DrawLine(const FVector& Start,const FVector& End,FColor Color)
{
	if(Pass != SP_OpaqueEmissive && Pass != SP_Unlit && Pass != SP_DepthSortedHitTest && Pass != SP_UnlitHitTest)
		return;

	if(GD3DRenderInterface.HitTesting)
	{
		FD3DWireframeHitProxyShader*	CachedShader = (FD3DWireframeHitProxyShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_WireframeHitProxy,GEngine->DefaultMaterial->MaterialResources[0],NULL);
		CachedShader->Set(NULL,NULL,Color);
	}
	else
	{
		FD3DWireframeShader*	CachedShader = (FD3DWireframeShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_Wireframe,GEngine->DefaultMaterial->MaterialResources[0],NULL);
		CachedShader->Set(NULL,NULL,Color);
	}

	FD3DCanvasVertex Vertices[2];

	Vertices[0].Position = FPlane(Start,1);
	Vertices[1].Position = FPlane(End,1);

	GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
	GDirect3DDevice->DrawPrimitiveUP(D3DPT_LINELIST,1,Vertices,sizeof(FD3DCanvasVertex));

	Dirty = 1;
}

//
//	FD3DPrimitiveRenderInterface::DrawPoint
//

void FD3DPrimitiveRenderInterface::DrawPoint(const FVector& Position,FColor Color,FLOAT PointSize)
{
	if(Pass != SP_OpaqueEmissive && Pass != SP_Unlit && Pass != SP_DepthSortedHitTest && Pass != SP_UnlitHitTest)
		return;

	if(GD3DRenderInterface.HitTesting)
	{
		FD3DWireframeHitProxyShader*	CachedShader = (FD3DWireframeHitProxyShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_WireframeHitProxy,GEngine->DefaultMaterial->MaterialResources[0],NULL);
		CachedShader->Set(NULL,NULL,Color);
	}
	else
	{
		FD3DWireframeShader*	CachedShader = (FD3DWireframeShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_Wireframe,GEngine->DefaultMaterial->MaterialResources[0],NULL);
		CachedShader->Set(NULL,NULL,Color);
	}
	
	FD3DCanvasVertex Vertices[4];
	FPlane	BasePosition = GD3DSceneRenderer->ViewProjectionMatrix.TransformFPlane(FPlane(Position,1));
	FLOAT	PixelSizeX = PointSize / GD3DSceneRenderer->Context.SizeX,
			PixelSizeY = PointSize / GD3DSceneRenderer->Context.SizeY;

	Vertices[0].Position = GD3DSceneRenderer->InvViewProjectionMatrix.TransformFPlane(BasePosition + FPlane(-PixelSizeX * BasePosition.W,+PixelSizeY * BasePosition.W,0,0));
	Vertices[1].Position = GD3DSceneRenderer->InvViewProjectionMatrix.TransformFPlane(BasePosition + FPlane(-PixelSizeX * BasePosition.W,-PixelSizeY * BasePosition.W,0,0));
	Vertices[2].Position = GD3DSceneRenderer->InvViewProjectionMatrix.TransformFPlane(BasePosition + FPlane(+PixelSizeX * BasePosition.W,-PixelSizeY * BasePosition.W,0,0));
	Vertices[3].Position = GD3DSceneRenderer->InvViewProjectionMatrix.TransformFPlane(BasePosition + FPlane(+PixelSizeX * BasePosition.W,+PixelSizeY * BasePosition.W,0,0));

	GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->GetCanvasVertexDeclaration());
	GDirect3DDevice->DrawPrimitiveUP(
		D3DPT_TRIANGLEFAN,
		2,
		Vertices,
		sizeof(FD3DCanvasVertex)
		);

	Dirty = 1;
}

//
//	FD3DMeshPRI::IsMaterialRelevant
//

UBOOL FD3DMeshPRI::IsMaterialRelevant(FMaterial* Material)
{
	if(GD3DSceneRenderer->View->ViewMode == SVM_LightingOnly)
		Material = GEngine->LightingOnlyMaterial->GetMaterialInterface(0);

	if((Pass == SP_OpaqueLighting || Pass == SP_TranslucentLighting) && Material->Unlit)
		return 0;

	if(Pass == SP_OpaqueEmissive && !(Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || ((Material->BlendMode == BLEND_Translucent || Material->BlendMode == BLEND_Additive) && !Material->Unlit)))
		return 0;

	if((Pass == SP_TranslucentEmissive || Pass == SP_TranslucentLighting) && !((Material->BlendMode == BLEND_Translucent || Material->BlendMode == BLEND_Additive) && !Material->Unlit))
		return 0;

	if(Pass == SP_UnlitTranslucency && !((Material->BlendMode == BLEND_Additive || Material->BlendMode == BLEND_Translucent) && Material->Unlit))
		return 0;

	return 1;
}

//
//	FD3DMeshPRI::DrawMesh
//

void FD3DMeshPRI::DrawMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
{
	if(!IsMaterialRelevant(Material))
		return;

	if(GD3DSceneRenderer->View->ViewMode == SVM_LightingOnly)
		Material = GEngine->LightingOnlyMaterial->GetMaterialInterface(0);

	FD3DVertexFactory*				CachedVertexFactory = (FD3DVertexFactory*)GD3DRenderDevice->Resources.GetCachedResource(VertexFactory);
	TD3DRef<IDirect3DIndexBuffer9>	CachedIndexBuffer = NULL;
	UINT							BaseIndex = 0;

	if(IndexBuffer)
	{
		if(IndexBuffer->Dynamic)
		{
			FD3DDynamicIndexBuffer*	DynamicIndexBuffer = GD3DRenderDevice->GetDynamicIndexBuffer(IndexBuffer->Size,IndexBuffer->Stride);
			CachedIndexBuffer = DynamicIndexBuffer->IndexBuffer;
			BaseIndex = DynamicIndexBuffer->Set(IndexBuffer);
		}
		else
			CachedIndexBuffer = ((FD3DIndexBuffer*)GD3DRenderDevice->Resources.GetCachedResource(IndexBuffer))->IndexBuffer;
	}

	UINT	BaseVertexIndex = CachedVertexFactory->Set(VertexFactory);
	if(CachedIndexBuffer)
		GDirect3DDevice->SetIndices(CachedIndexBuffer);

	for(INT BackFace = 0;BackFace < (Material->TwoSided ? 2 : 1);BackFace++)
	{
		if(Pass != SP_LightFunction)
		{
			if(CullMode == CM_CW)
				GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,BackFace ? D3DCULL_CCW : D3DCULL_CW);
			else if(CullMode == CM_CCW)
				GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,BackFace ? D3DCULL_CCW : D3DCULL_CCW);
		}

		GD3DSceneRenderer->TwoSidedSign = BackFace ? -1.0f : 1.0f;

		Set(VertexFactory,Material,MaterialInstance);

		if(CachedIndexBuffer)
			GDirect3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,BaseVertexIndex,MinVertexIndex,MaxVertexIndex - MinVertexIndex + 1,BaseIndex + FirstIndex,NumTriangles);
		else
			GDirect3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST,BaseVertexIndex + FirstIndex,NumTriangles);

		GD3DStats.MeshTriangles.Value += NumTriangles;
	}

	if(Pass != SP_LightFunction)
		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

	Dirty = 1;

	// When the mesh-edges show flag is present, render the mesh wireframe as well as the filled surfaces.

	if(GD3DSceneRenderer->View->ShowFlags & SHOW_MeshEdges)
		DrawWireframe(
			VertexFactory,
			IndexBuffer,
			WT_TriList,
			FColor(0,0,255),
			FirstIndex,
			NumTriangles,
			MinVertexIndex,
			MaxVertexIndex,
			0);
}

//
//	FD3DMeshLightPRI::Set
//

void FD3DMeshLightPRI::Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	DWORD	Function;
	if(Light->IsA(USpotLightComponent::StaticClass()))
		Function = SF_SpotLight;
	else if(Light->IsA(UPointLightComponent::StaticClass()))
		Function = SF_PointLight;
	else if(Light->IsA(UDirectionalLightComponent::StaticClass()))
		Function = SF_DirectionalLight;
	else
		appErrorf(TEXT("Unrecognized light type: %s"),*Light->GetFullName());

	if((Material->BlendMode == BLEND_Translucent || Material->BlendMode == BLEND_Additive) && Pass == SP_OpaqueLighting)
        Function |= SF_OpaqueLayer;

	if(GD3DRenderDevice->UseFPBlending)
		Function |= SF_UseFPBlending;

	FD3DMeshLightShader*	CachedShader = (FD3DMeshLightShader*)GD3DRenderDevice->MeshLightShaders.GetCachedShader(
																	(ED3DShaderFunction)Function,
																	Material,
																	VertexFactory->Type(),
																	StaticLightingType
																	);
	CachedShader->Set(Primitive,VertexFactory,MaterialInstance,Light,VisibilityTexture);
}

//
//	FD3DPrimitiveInfo::FD3DPrimitiveInfo
//

FD3DPrimitiveInfo::FD3DPrimitiveInfo(UPrimitiveComponent* InPrimitive,const FSceneContext& Context):
	Primitive(InPrimitive)
{
	Visible = Occluded = 0;
	NotOccluded = 1;
	Lit = 1;
	PVI = Primitive->CreateViewInterface(Context);
	LayerMask = PVI->GetLayerMask(Context);
}

//
//	FD3DPrimitiveInfo::IsOccluded
//

UBOOL FD3DPrimitiveInfo::IsOccluded()
{
	if(NotOccluded)
		return 0;

	if(!Occluded)
	{
		check(OcclusionQuery);

		DWORD	NumPixelsVisible = 0;
		HRESULT	QueryResult = OcclusionQuery->GetData(&NumPixelsVisible,sizeof(DWORD),D3DGETDATA_FLUSH);
		if(QueryResult == S_OK)
		{
			if(NumPixelsVisible)
				NotOccluded = 1;
			else
				Occluded = 1;
		}
		else
		{
			GD3DStats.IncompleteOcclusionQueries.Value++;
			return 0;
		}
	}

	if(Occluded)
		GD3DStats.CulledOcclusionQueries.Value++;

	return Occluded;
}

//
//	FD3DVisiblePrimitiveIterator
//

FD3DVisiblePrimitiveIterator::FD3DVisiblePrimitiveIterator(DWORD InLayerMask):
	RemainingLayerMask(InLayerMask),
	FinishedLayerMask(0),
	CurrentSet(NULL)
{
	NextSet();
}

void FD3DVisiblePrimitiveIterator::NextSet()
{
	if(CurrentSet)
	{
		FinishedLayerMask |= CurrentSet->LayerMask;
		RemainingLayerMask &= ~CurrentSet->LayerMask;
	}

	CurrentSet = NULL;
	Index = -1;

	if(RemainingLayerMask)
	{
		for(INT SetIndex = 0;SetIndex < GD3DSceneRenderer->LayerVisibilitySets.Num();SetIndex++)
		{
			if(GD3DSceneRenderer->LayerVisibilitySets(SetIndex).LayerMask & RemainingLayerMask)
			{
				CurrentSet = &GD3DSceneRenderer->LayerVisibilitySets(SetIndex);
				break;
			}
		}

		if(CurrentSet)
			Next();
	}
}

void FD3DVisiblePrimitiveIterator::Next()
{
	check(CurrentSet);
	while(1)
	{
		Index++;

		if(Index >= CurrentSet->Num())
		{
			NextSet();
			return;
		}

		CurrentPrimitiveInfo = &GD3DSceneRenderer->PrimitiveInfoArray((*CurrentSet)(Index));
		if(!(CurrentPrimitiveInfo->LayerMask & FinishedLayerMask))
			break;
	}
}

FD3DPrimitiveInfo* FD3DVisiblePrimitiveIterator::operator->() const
{
	return CurrentPrimitiveInfo;
}

FD3DPrimitiveInfo* FD3DVisiblePrimitiveIterator::operator*() const
{
	return CurrentPrimitiveInfo;
}

FD3DVisiblePrimitiveIterator::operator UBOOL() const
{
	return CurrentSet != NULL;
}

//
//	FShadowProjectionMatrix
//

struct FShadowProjectionMatrix: FMatrix
{
	FShadowProjectionMatrix(FLOAT MinZ,FLOAT MaxZ,const FPlane& WPlane):
		FMatrix(
			FPlane(1,	0,	0,														WPlane.X),
			FPlane(0,	1,	0,														WPlane.Y),
			FPlane(0,	0,	(WPlane.Z * MaxZ + WPlane.W) / (MaxZ - MinZ),			WPlane.Z),
			FPlane(0,	0,	-MinZ * (WPlane.Z * MaxZ + WPlane.W) / (MaxZ - MinZ),	WPlane.W)
			)
	{}
};

FD3DShadowInfo::FD3DShadowInfo(UPrimitiveComponent* InSubjectPrimitive,const FMatrix& WorldToLight,const FVector& FaceDirection,const FSphere& SubjectBoundingSphere,const FPlane& WPlane,FLOAT MinLightZ,FLOAT MaxLightZ,UINT InResolution):
	SubjectPrimitive(InSubjectPrimitive),
	Resolution(InResolution),
	Occluded(0),
	NotOccluded(1)
{
	FVector	XAxis,
			YAxis;
	FaceDirection.FindBestAxisVectors(XAxis,YAxis);
	FMatrix	WorldToFace = WorldToLight * FBasisVectorMatrix(-XAxis,YAxis,FaceDirection.SafeNormal(),FVector(0,0,0));

	FLOAT	MaxShadowW = WorldToFace.TransformFVector(SubjectBoundingSphere).Z + SubjectBoundingSphere.W;
	MinShadowW = Max(MaxShadowW - SubjectBoundingSphere.W * 2,MinLightZ);

	ShadowMatrix = WorldToFace * FShadowProjectionMatrix(MinLightZ,MaxShadowW,WPlane);
	SubjectShadowMatrix = WorldToFace * FShadowProjectionMatrix(MinShadowW,MaxShadowW,WPlane);
	ReceiverShadowMatrix = WorldToFace * FShadowProjectionMatrix(MinShadowW,MaxLightZ,WPlane);

	MaxShadowZ = SubjectShadowMatrix.TransformFVector((FVector)SubjectBoundingSphere + WorldToLight.Inverse().TransformNormal(FaceDirection) * SubjectBoundingSphere.W).Z;

	if(GD3DRenderDevice->EnableOcclusionQuery)
	{
		HRESULT Result = GDirect3DDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION,&OcclusionQuery.Handle);
		if(FAILED(Result))
			appErrorf(TEXT("CreateQuery failed: %s"),*D3DError(Result));
	}

	FConvexVolume::GetViewFrustumBounds(ReceiverShadowMatrix,1,ShadowFrustum);
}

UBOOL FD3DShadowInfo::IsOccluded()
{
	if(NotOccluded)
		return 0;

	if(!Occluded)
	{
		check(OcclusionQuery);

		while(1) // Loop until the occlusion query completes.
		{
			DWORD	NumPixelsVisible = 0;
			HRESULT	QueryResult = OcclusionQuery->GetData(&NumPixelsVisible,sizeof(DWORD),D3DGETDATA_FLUSH);
			if(QueryResult == S_OK)
			{
				if(NumPixelsVisible)
					NotOccluded = 1;
				else
					Occluded = 1;
				break;
			}
		};
	}

	if(Occluded)
	{
		GD3DStats.CulledOcclusionQueries.Value++;
	}

	return Occluded;
}

//
//	FD3DSceneRenderer::FD3DSceneRenderer
//

struct FPrimitiveOrder
{
	INT		PrimitiveInfoIndex;
	FLOAT	Z;
	FPrimitiveOrder(INT InPrimitiveInfoIndex,FLOAT InZ):
		PrimitiveInfoIndex(InPrimitiveInfoIndex),
		Z(InZ)
	{}
};

IMPLEMENT_COMPARE_CONSTREF(FPrimitiveOrder,D3DScene,{ return A.Z < B.Z ? 1 : -1; });
IMPLEMENT_COMPARE_POINTER(UHeightFogComponent,D3DScene,{ return A->Height < B->Height ? 1 : (A->Height > B->Height ? -1 : 0); });

FD3DSceneRenderer::FD3DSceneRenderer(const FSceneContext& InContext):
	Scene(InContext.Scene),
	View(InContext.View),
	Context(InContext),
	LastLightingBuffer(0),
	NewLightingBuffer(0),
	LastLightingLayerFinished(0),
	LastBlurBuffer(0),
	TwoSidedSign(1.0f)
{
	GD3DRenderDevice->CacheBuffers(Max(Context.SizeX,GD3DRenderInterface.CurrentViewport->SizeX),Max(Context.SizeY,GD3DRenderInterface.CurrentViewport->SizeY));

	BEGINCYCLECOUNTER(GD3DStats.SceneSetupTime);

	// Initialize the view transform and frustum.

	ViewProjectionMatrix = View->ViewMatrix * View->ProjectionMatrix;
	InvViewProjectionMatrix = ViewProjectionMatrix.Inverse();
	FConvexVolume::GetViewFrustumBounds(ViewProjectionMatrix,0,ViewFrustum);
	if(View->ProjectionMatrix.M[3][3] < 1.0f)
		CameraPosition = FPlane(View->ViewMatrix.Inverse().GetOrigin(),1);
	else
		CameraPosition = FPlane(View->ViewMatrix.Inverse().TransformNormal(FVector(0,0,-1)).SafeNormal(),0);

	// Create the (initially empty) layer visibility sets.

	FD3DLayerVisibilitySet*	OpaqueVisibilitySet = new(LayerVisibilitySets) FD3DLayerVisibilitySet(PLM_Opaque);
	FD3DLayerVisibilitySet*	TranslucentVisibilitySet = new(LayerVisibilitySets) FD3DLayerVisibilitySet(PLM_Translucent);
	FD3DLayerVisibilitySet*	BackgroundVisibilitySet = new(LayerVisibilitySets) FD3DLayerVisibilitySet(PLM_Background);
	FD3DLayerVisibilitySet*	ForegroundVisibilitySet = new(LayerVisibilitySets) FD3DLayerVisibilitySet(PLM_Foreground);

	// Find visible primitives.

	TArray<UPrimitiveComponent*>	VisiblePrimitives;
	Scene->GetVisiblePrimitives(Context,ViewFrustum,VisiblePrimitives);

	for(INT PrimitiveIndex = 0;PrimitiveIndex < VisiblePrimitives.Num();PrimitiveIndex++)
	{
		if(VisiblePrimitives(PrimitiveIndex)->Visible(View))
		{
			// Create a FD3DPrimitiveInfo for each visible primitive.

			INT					PrimitiveInfoIndex = GetPrimitiveInfoIndex(VisiblePrimitives(PrimitiveIndex));
			FD3DPrimitiveInfo&	PrimitiveInfo = PrimitiveInfoArray(PrimitiveInfoIndex);
			PrimitiveInfo.Visible = 1;

			// Add the primitive to the visibility sets for the layers in it's layer mask.

			for(INT SetIndex = 0;SetIndex < LayerVisibilitySets.Num();SetIndex++)
				if(LayerVisibilitySets(SetIndex).LayerMask & PrimitiveInfo.LayerMask)
					LayerVisibilitySets(SetIndex).AddItem(PrimitiveInfoIndex);

			// Add the primitive's lights to the visible lights array.

			for(INT LightIndex = 0;LightIndex < PrimitiveInfo.Primitive->Lights.Num();LightIndex++)
			{
				ULightComponent* Light = PrimitiveInfo.Primitive->Lights(LightIndex).GetLight();
				FD3DLightInfo* LightInfo = VisibleLights.Find(Light);
				if(!LightInfo)
				{
					VisibleLights.Set(Light,FD3DLightInfo());
				}
			}

			// Create an occlusion query for the primitive.

			if(GD3DRenderDevice->EnableOcclusionQuery)
			{
				HRESULT	Result = GDirect3DDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION,&PrimitiveInfo.OcclusionQuery.Handle);
				if(FAILED(Result))
					appErrorf(TEXT("CreateQuery failed: %s"),*D3DError(Result));
			}
		}
	}

	// Sort the elements of the translucent layer visibility set.

	TArray<FPrimitiveOrder> PrimitiveOrder;
	PrimitiveOrder.Empty(TranslucentVisibilitySet->Num());
	for(INT PrimitiveIndex = 0;PrimitiveIndex < TranslucentVisibilitySet->Num();PrimitiveIndex++)
	{
		INT PrimitiveInfoIndex = (*TranslucentVisibilitySet)(PrimitiveIndex);
		FD3DPrimitiveInfo& PrimitiveInfo = PrimitiveInfoArray(PrimitiveInfoIndex);
		new(PrimitiveOrder) FPrimitiveOrder(PrimitiveInfoIndex,Context.View->Project(PrimitiveInfo.Primitive->Bounds.Origin).Z);
	}
	Sort<USE_COMPARE_CONSTREF(FPrimitiveOrder,D3DScene)>(&PrimitiveOrder(0),PrimitiveOrder.Num());

	TranslucentVisibilitySet->Empty(PrimitiveOrder.Num());
	for(INT PrimitiveIndex = 0;PrimitiveIndex < PrimitiveOrder.Num();PrimitiveIndex++)
	{
		TranslucentVisibilitySet->AddItem(PrimitiveOrder(PrimitiveIndex).PrimitiveInfoIndex);
	}

	// Find possible shadows.

	for(TMap<ULightComponent*,FD3DLightInfo>::TIterator It(VisibleLights);It;++It)
	{
		ULightComponent* Light = It.Key();
		FD3DLightInfo& LightInfo = It.Value();

		if(!Light->CastShadows || !GD3DRenderDevice->UseStaticLighting || !Light->HasStaticShadowing())
			continue;

		for(TDoubleLinkedList<UPrimitiveComponent*>::TIterator It(Light->FirstDynamicPrimitiveLink);It;It.Next())
		{
			if(!It->ShadowParent && It->CastShadow && It->Visible(Context.View) && !It->HasStaticShadowing())
			{
				// Compute the composite bounds of this group of shadow primitives.

				FBoxSphereBounds	Bounds(It->Bounds);

				for(UPrimitiveComponent* ShadowChild = It->FirstShadowChild;ShadowChild;ShadowChild = ShadowChild->NextShadowChild)
					Bounds = Bounds + ShadowChild->Bounds;

				// Determine the amount of shadow buffer resolution to use.

				FPlane	ScreenPosition = Context.View->Project(Bounds.Origin);
				FLOAT	ScreenRadius = Max(
										(FLOAT)Context.SizeX / 2.0f * Context.View->ProjectionMatrix.M[0][0],
										(FLOAT)Context.SizeY / 2.0f * Context.View->ProjectionMatrix.M[1][1]
										) *
										Bounds.SphereRadius /
										Max(ScreenPosition.W,1.0f);
				UINT	ShadowResolution = Clamp<UINT>(ScreenRadius * GD3DRenderDevice->ShadowResolutionScale,GD3DRenderDevice->MinShadowResolution,GD3DRenderDevice->MaxShadowResolution - 2);

				if(Light->IsA(UDirectionalLightComponent::StaticClass()))
				{
					// For sky lights, use an orthographic projection.

					UDirectionalLightComponent*	SkyLight = Cast<UDirectionalLightComponent>(Light);
					new(LightInfo.Shadows) FD3DShadowInfo(
						*It,
						FTranslationMatrix(-Bounds.Origin) *
							FInverseRotationMatrix(FVector(SkyLight->WorldToLight.M[0][2],SkyLight->WorldToLight.M[1][2],SkyLight->WorldToLight.M[2][2]).SafeNormal().Rotation()) *
							FScaleMatrix(FVector(1.0f,1.0f / Bounds.SphereRadius,1.0f / Bounds.SphereRadius)),
						FVector(1,0,0),
						FSphere(Bounds.Origin,Bounds.SphereRadius),
						FPlane(0,0,0,1),
						-HALF_WORLD_MAX,
						HALF_WORLD_MAX,
						ShadowResolution
						);
				}
				else if(Light->IsA(UPointLightComponent::StaticClass()))
				{
					// For point lights, use a perspective projection looking at the primitive from the light position.

					UPointLightComponent*	PointLight = Cast<UPointLightComponent>(Light);
					FVector					LightPosition = PointLight->GetOrigin(),
											LightVector = Bounds.Origin - LightPosition;
					FLOAT					LightDistance = LightVector.Size(),
											SilhouetteRadius = 0.0f;

					if(LightDistance > It->Bounds.SphereRadius)
						SilhouetteRadius = Bounds.SphereRadius * appInvSqrt((LightDistance - Bounds.SphereRadius) * (LightDistance + Bounds.SphereRadius));

					if(LightDistance <= Bounds.SphereRadius || SilhouetteRadius > 1.0f)
					{
						// If the FOV required to capture the entire primitive is above 90 degrees, clamp the FOV at 90 and split the shadow into multiple projections.

						for(UINT ComponentIndex = 0;ComponentIndex < 3;ComponentIndex++)
						{
							for(UINT Sign = 0;Sign < 2;Sign++)
							{
								FVector	FaceDirection(0,0,0);
								FaceDirection[ComponentIndex] = Sign ? 1.0f : -1.0f;

								new(LightInfo.Shadows) FD3DShadowInfo(
									*It,
									FTranslationMatrix(-LightPosition) *
										FInverseRotationMatrix((LightVector / LightDistance).Rotation()),
									FaceDirection,
									FSphere(Bounds.Origin,Bounds.SphereRadius),
									FPlane(0,0,1,0),
									0.1f,
									PointLight->Radius,
									ShadowResolution
									);
							}
						}
					}
					else
					{
						// The primitive fits in a single <90 degree FOV projection.

						new(LightInfo.Shadows) FD3DShadowInfo(
							*It,
							FTranslationMatrix(-LightPosition) *
								FInverseRotationMatrix((LightVector / LightDistance).Rotation()) *
								FScaleMatrix(FVector(1.0f,1.0f / SilhouetteRadius,1.0f / SilhouetteRadius)),
							FVector(1,0,0),
							FSphere(Bounds.Origin,Bounds.SphereRadius),
							FPlane(0,0,1,0),
							0.1f,
							PointLight->Radius,
							ShadowResolution
							);
					}
				}
			}
		}
	}

	if(GD3DRenderDevice->EnableOcclusionQuery)
	{
		// Create an occlusion queries for each possible layer of each translucent primitive.

		TranslucencyLayerQueries.Empty(TranslucentVisibilitySet->Num() * GD3DRenderDevice->MaxTranslucencyLayers);

		for(INT LayerIndex = 0;LayerIndex < GD3DRenderDevice->MaxTranslucencyLayers;LayerIndex++)
		{
			for(INT PrimitiveIndex = 0;PrimitiveIndex < TranslucentVisibilitySet->Num();PrimitiveIndex++)
			{
				TD3DRef<IDirect3DQuery9>	Query;
				HRESULT						Result = GDirect3DDevice->CreateQuery(D3DQUERYTYPE_OCCLUSION,&Query.Handle);
				if(FAILED(Result))
					appErrorf(TEXT("CreateQuery failed: %s"),*D3DError(Result));
				new(TranslucencyLayerQueries) TD3DRef<IDirect3DQuery9>(Query);
			}
		}

		GD3DStats.UniqueOcclusionQueries.Value += TranslucentVisibilitySet->Num() * GD3DRenderDevice->MaxTranslucencyLayers;
	}

	// Count the primitives in each layer.

	GD3DStats.OpaquePrimitives.Value += OpaqueVisibilitySet->Num();
	GD3DStats.TranslucentPrimitives.Value += TranslucentVisibilitySet->Num();
	GD3DStats.BackgroundPrimitives.Value += BackgroundVisibilitySet->Num();
	GD3DStats.ForegroundPrimitives.Value += ForegroundVisibilitySet->Num();

	GVisibilityStats.VisiblePrimitives.Value += OpaqueVisibilitySet->Num();
	GVisibilityStats.VisiblePrimitives.Value += TranslucentVisibilitySet->Num();
	GVisibilityStats.VisiblePrimitives.Value += BackgroundVisibilitySet->Num();
	GVisibilityStats.VisiblePrimitives.Value += ForegroundVisibilitySet->Num();

	// Determine the sky color for the scene.

	SkyColor = FLinearColor::Black;
	for(INT LightIndex = 0;LightIndex < Scene->SkyLights.Num();LightIndex++)
	{
		if(Scene->SkyLights(LightIndex)->bEnabled)
		{
			SkyColor += (FLinearColor)Scene->SkyLights(LightIndex)->Color * Scene->SkyLights(LightIndex)->Brightness;
		}
	}

	// Clear fog constants.

	for(INT LayerIndex = 0;LayerIndex < 4;LayerIndex++)
	{
		FogMinHeight[LayerIndex] = FogMaxHeight[LayerIndex] = FogDistanceScale[LayerIndex] = 0.0f;
		FogInScattering[LayerIndex] = FLinearColor::Black;
	}

	if(View->ShowFlags & SHOW_Fog)
	{
		// Build a list of the first four height fog components in the scene, sorted by descending height.

		UHeightFogComponent* FogComponents[4];
		INT NumFogComponents = Min(Scene->Fogs.Num(),4);
		for(INT FogIndex = 0;FogIndex < NumFogComponents;FogIndex++)
		{
			FogComponents[FogIndex] = Scene->Fogs(FogIndex);
		}
		Sort<USE_COMPARE_POINTER(UHeightFogComponent,D3DScene)>(FogComponents,NumFogComponents);

		// Remap the fog layers into back to front order.

		INT FogLayerMap[4];
		INT NumFogLayers = 0;
		for(INT AscendingFogIndex = NumFogComponents - 1;AscendingFogIndex >= 0;AscendingFogIndex--)
		{
			if(FogComponents[AscendingFogIndex]->Height > CameraPosition.Z)
			{
				for(INT DescendingFogIndex = 0;DescendingFogIndex <= AscendingFogIndex;DescendingFogIndex++)
				{
					FogLayerMap[NumFogLayers++] = DescendingFogIndex;
				}
				break;
			}
			FogLayerMap[NumFogLayers++] = AscendingFogIndex;
		}

		// Calculate the fog constants.

		for(INT LayerIndex = 0;LayerIndex < NumFogLayers;LayerIndex++)
		{
			UHeightFogComponent* FogComponent = FogComponents[FogLayerMap[LayerIndex]];
			FogDistanceScale[LayerIndex] = appLoge(1.0f - FogComponent->Density) / appLoge(2.0f);
			if(FogLayerMap[LayerIndex] + 1 < NumFogLayers)
			{
				FogMinHeight[LayerIndex] = FogComponents[FogLayerMap[LayerIndex] + 1]->Height;
			}
			else
			{
				FogMinHeight[LayerIndex] = -HALF_WORLD_MAX;
			}
			FogMaxHeight[LayerIndex] = FogComponent->Height;
			// This formula is incorrect, but must be used to support legacy content.  The in-scattering color should be computed like this:
			// FogInScattering[LayerIndex] = FLinearColor(FogComponent->LightColor) * (FogComponent->LightBrightness / (appLoge(2.0f) * FogDistanceScale[LayerIndex]));
			FogInScattering[LayerIndex] = FLinearColor(FogComponent->LightColor) * (FogComponent->LightBrightness / appLoge(0.5f));
		}
	}

	ENDCYCLECOUNTER;
}

//
//	FD3DSceneRenderer::~FD3DSceneRenderer
//

FD3DSceneRenderer::~FD3DSceneRenderer()
{
	// Cleanup primitive views.

	for(INT InfoIndex = 0;InfoIndex < PrimitiveInfoArray.Num();InfoIndex++)
		PrimitiveInfoArray(InfoIndex).PVI->FinalizeView();
}

//
//	FD3DSceneRenderer::CreatePrimitiveInfo
//

INT FD3DSceneRenderer::CreatePrimitiveInfo(UPrimitiveComponent* Primitive)
{
	FD3DPrimitiveInfo*	PrimitiveInfo = new(PrimitiveInfoArray) FD3DPrimitiveInfo(Primitive,Context);
	PrimitiveIndexMap.Set(Primitive,PrimitiveInfoArray.Num() - 1);
	return PrimitiveInfoArray.Num() - 1;
}

//
//	FD3DSceneRenderer::GetLastLightingBuffer
//

const TD3DRef<IDirect3DTexture9>& FD3DSceneRenderer::GetLastLightingBuffer()
{
#ifndef XBOX
	return GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetTexture();
#else
	return GD3DRenderDevice->LightingRenderTarget->GetTexture();
#endif
}

//
//	FD3DSceneRenderer::GetNewLightingBuffer
//

const TD3DRef<IDirect3DSurface9>& FD3DSceneRenderer::GetNewLightingBuffer(UBOOL Replace,UBOOL Additive,UBOOL Fullscreen)
{
#ifndef XBOX
	if(LastLightingLayerFinished)
	{
		if(Replace || (Additive && GD3DRenderDevice->UseFPBlending))
			NewLightingBuffer = LastLightingBuffer;
		else
		{
			BEGIND3DCYCLECOUNTER(GD3DStats.LightBufferCopyTime);
			NewLightingBuffer = 1 - LastLightingBuffer;
			if(!Fullscreen)
			{
				GD3DRenderDevice->CopyShader->Set(GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetTexture());
				GD3DRenderInterface.FillSurface(
					GD3DRenderDevice->LightingRenderTargets[NewLightingBuffer]->GetSurface(),
					0,
					0,
					Context.SizeX,
					Context.SizeY,
					0,
					0,
					(FLOAT)Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX,
					(FLOAT)Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY
					);
			}
			ENDD3DCYCLECOUNTER;
		}
		LastLightingLayerFinished = 0;
	}
	return GD3DRenderDevice->LightingRenderTargets[NewLightingBuffer]->GetSurface();
#else
	return GD3DRenderDevice->LightingRenderTarget->GetSurface();
#endif
}

//
//	FD3DSceneRenderer::ClearLightingBufferDepth
//

void FD3DSceneRenderer::ClearLightingBufferDepth(FLOAT DepthValue)
{
	BEGIND3DCYCLECOUNTER(GD3DStats.LightBufferCopyTime);
	FinishLightingLayer();
#ifndef XBOX
	GD3DRenderDevice->CopyFillShader->Set(GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetTexture(),FLinearColor(0,0,0,DepthValue),FPlane(0,0,0,1));
#else
	GD3DRenderDevice->CopyFillShader->Set(GD3DRenderDevice->LightingRenderTarget->GetTexture(),FLinearColor(0,0,0,DepthValue),FPlane(0,0,0,1));
#endif
	GD3DRenderInterface.FillSurface(
		GetNewLightingBuffer(0,0,1),
		0,
		0,
		Context.SizeX,
		Context.SizeY,
		0,
		0,
		(FLOAT)Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX,
		(FLOAT)Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY
		);
	ENDD3DCYCLECOUNTER;
}

//
//	FD3DSceneRenderer::FinishLightingLayer
//

void FD3DSceneRenderer::FinishLightingLayer()
{
#ifndef XBOX
	if(!LastLightingLayerFinished)
	{
		LastLightingLayerFinished = 1;
		LastLightingBuffer = NewLightingBuffer;
	}
#else
	//@todo xenon: either handle stretching in FinishLightingLayer or avoid it in all cases
	check( Context.SizeX == GD3DRenderDevice->BufferSizeX );
	check( Context.SizeY == GD3DRenderDevice->BufferSizeY );
	GD3DRenderDevice->LightingRenderTarget->Resolve();
#endif
}

//
//	FD3DUnlitPRI
//

struct FD3DUnlitPRI: FD3DMeshPRI
{
	// Constructor.

	FD3DUnlitPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass):
		FD3DMeshPRI(InPrimitive,InPass)
	{}

	// FPrimitiveRenderInterface interface.

	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		FD3DMeshPRI::DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
	}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		DWORD	Function = SF_Emissive | SF_OpaqueLayer;
		if(GD3DRenderDevice->UseFPBlending)
			Function |= SF_UseFPBlending;
		FD3DEmissiveShader*	CachedShader = (FD3DEmissiveShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)Function,Material,VertexFactory->Type());
		CachedShader->Set(Primitive,VertexFactory,MaterialInstance);
	}
};

//
//	FD3DDepthOnlyPRI
//

struct FD3DDepthOnlyPRI: FD3DMeshPRI
{
	FD3DDepthOnlyShader* DepthOnlyShader;

	// Constructor.

	FD3DDepthOnlyPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass,FD3DDepthOnlyShader* InDepthOnlyShader):
		FD3DMeshPRI(InPrimitive,InPass),
		DepthOnlyShader(InDepthOnlyShader)
	{}

	// FPrimitiveRenderInterface interface.

	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		if(VertexFactory->Type()->IsA(FLocalVertexFactory::StaticType) && !VertexFactory->Dynamic && !IndexBuffer->Dynamic && Material->BlendMode == BLEND_Opaque)
		{
			FD3DMeshPRI::DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
		}
	}

	virtual void DrawMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		if(VertexFactory->Type()->IsA(FLocalVertexFactory::StaticType) && !VertexFactory->Dynamic && !IndexBuffer->Dynamic && Material->BlendMode == BLEND_Opaque)
		{
			FD3DMeshPRI::DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
		}
	}

	virtual void DrawSprite(const FVector& Position,FLOAT SizeX,FLOAT SizeY,FTexture2D* Sprite,FColor Color) {}
	virtual void DrawWireframe(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,EWireframeType WireType,FColor Color,UINT FirstIndex,UINT NumPrimitives,UINT MinVertexIndex,UINT MaxVertexIndex,UBOOL DrawVertices) {}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		DepthOnlyShader->SetInstance(Primitive,(FLocalVertexFactory*)VertexFactory);
	}
};

//
//	FD3DEmissivePRI
//

struct FD3DEmissivePRI: FD3DMeshPRI
{
	UBOOL	VertexLit;
	UBOOL	HasLitMaterial;

	// Constructor.

	FD3DEmissivePRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass):
		FD3DMeshPRI(InPrimitive,InPass),
		VertexLit(0),
		HasLitMaterial(0)
	{}

	// FPrimitiveRenderInterface interface.

	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		VertexLit = 1;
		FD3DMeshPRI::DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
		VertexLit = 0;
	}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		if(!Material->Unlit)
			HasLitMaterial = 1;

		if(GD3DSceneRenderer->View->ViewMode == SVM_Unlit)
		{
			DWORD	Function = SF_Unlit;
			if(Pass != SP_TranslucentEmissive)
				Function |= SF_OpaqueLayer;

			FD3DUnlitShader*	CachedShader = (FD3DUnlitShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)Function,Material,VertexFactory->Type());
			CachedShader->Set(Primitive,VertexFactory,MaterialInstance);
		}
		else if(GD3DSceneRenderer->View->ViewMode == SVM_LightComplexity)
		{
			DWORD	Function = SF_Emissive;
			if(Pass != SP_TranslucentEmissive)
				Function |= SF_OpaqueLayer;
			if(GD3DRenderDevice->UseFPBlending)
				Function |= SF_UseFPBlending;

			// Count the number of light passes required for this primitive.
			INT NumActiveLights = 0;
			for(INT LightIndex = 0;LightIndex < Primitive->Lights.Num();LightIndex++)
			{
				ULightComponent* Light = Primitive->Lights(LightIndex).GetLight();
				if(Light->bEnabled && Light->Brightness > 0.0f && !Light->IsA(USkyLightComponent::StaticClass()))
				{
					NumActiveLights++;
				}
			}

			// Create a material instance which sets the color based on the number of light passes and GEngine->LightComplexityColors.
			FMaterialInstance SolidColorInstance;
			if(GEngine->LightComplexityColors.Num())
			{
				SolidColorInstance.VectorValueMap.Set(NAME_Color,GEngine->LightComplexityColors(Min(NumActiveLights,GEngine->LightComplexityColors.Num() - 1)));
			}
			else
			{
				SolidColorInstance.VectorValueMap.Set(NAME_Color,FLinearColor::Black);
			}

			// Set the solid color material's emissive shader.
			FD3DEmissiveShader*	CachedShader = (FD3DEmissiveShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)Function,GEngine->SolidColorMaterial->GetMaterialInterface(0),VertexFactory->Type());
			CachedShader->Set(Primitive,VertexFactory,&SolidColorInstance);
		}
		else
		{
			if(VertexLit)
			{
				DWORD	Function = SF_VertexLighting;
				if(Pass != SP_TranslucentEmissive)
					Function |= SF_OpaqueLayer;
				if(GD3DRenderDevice->UseFPBlending)
					Function |= SF_UseFPBlending;

				FD3DVertexLightingShader*	CachedShader = (FD3DVertexLightingShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)Function,Material,VertexFactory->Type());
				CachedShader->Set(Primitive,VertexFactory,MaterialInstance);
			}
			else
			{
				DWORD	Function = SF_Emissive;
				if(Pass != SP_TranslucentEmissive)
					Function |= SF_OpaqueLayer;
				if(GD3DRenderDevice->UseFPBlending)
					Function |= SF_UseFPBlending;

				FD3DEmissiveShader*	CachedShader = (FD3DEmissiveShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)Function,Material,VertexFactory->Type());
				CachedShader->Set(Primitive,VertexFactory,MaterialInstance);
			}
		}
	}
};

//
//	FD3DHitTestingPRI
//

struct FD3DHitTestingPRI: FD3DEmissivePRI
{
	// Constructor.

	FD3DHitTestingPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass):
		FD3DEmissivePRI(InPrimitive,InPass)
	{}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		FD3DHitProxyShader*	CachedShader = (FD3DHitProxyShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_HitProxy,Material,VertexFactory->Type());
		CachedShader->Set(Primitive,VertexFactory,MaterialInstance);
	}

	// FPrimitiveRenderInterface interface.

	virtual void SetHitProxy(HHitProxy* HitProxy) { GD3DRenderInterface.SetHitProxy(HitProxy); }
};

//
//	FD3DTranslucentLayerPRI
//

struct FD3DTranslucentLayerPRI: FD3DMeshPRI
{
	INT		LayerIndex;

	// Constructor.

	FD3DTranslucentLayerPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass,INT InLayerIndex):
		FD3DMeshPRI(InPrimitive,InPass),
		LayerIndex(InLayerIndex)
	{}

	// FPrimitiveRenderInterface interface.

	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		FD3DMeshPRI::DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
	}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		FD3DTranslucentLayerShader*	CachedShader = (FD3DTranslucentLayerShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_TranslucentLayer,Material,VertexFactory->Type());
		CachedShader->Set(Primitive,VertexFactory,MaterialInstance,LayerIndex);
	}
};
//
//	FD3DUnlitTranslucencyPRI
//

struct FD3DUnlitTranslucencyPRI: FD3DMeshPRI
{
	// Constructor.

	FD3DUnlitTranslucencyPRI(UPrimitiveComponent* InPrimitive):
		FD3DMeshPRI(InPrimitive,SP_UnlitTranslucency)
	{}

	// FPrimitiveRenderInterface interface.

	virtual void DrawVertexLitMesh(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		FD3DMeshPRI::DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
	}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		FD3DUnlitTranslucencyShader*	CachedShader = (FD3DUnlitTranslucencyShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_UnlitTranslucency,Material,VertexFactory->Type());
		CachedShader->Set(Primitive,VertexFactory,MaterialInstance);

		if(Material->BlendMode == BLEND_Additive)
		{
			GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
			GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
			GDirect3DDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
			GDirect3DDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ONE);
		}
		else
		{
			GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
			GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
			if(GD3DRenderDevice->UseFPTranslucency)
			{
				GDirect3DDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
				GDirect3DDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ONE);
			}
			else
			{
				GDirect3DDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
				GDirect3DDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);
			}
		}
	}
};

//
//	SetUnlitTarget
//

void SetUnlitTarget()
{
	GDirect3DDevice->SetRenderTarget(0,GD3DSceneRenderer->GetNewLightingBuffer(1));
	GD3DRenderInterface.SetViewport(0,0,GD3DSceneRenderer->Context.SizeX,GD3DSceneRenderer->Context.SizeY);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);
}

//
//	SetFogTarget
//

void SetFogTarget(UBOOL Translucent)
{
	GDirect3DDevice->SetRenderTarget(0,GD3DSceneRenderer->GetNewLightingBuffer(0,0,1));
	GD3DRenderInterface.SetViewport(0,0,GD3DSceneRenderer->Context.SizeX,GD3DSceneRenderer->Context.SizeY);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESS);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,Translucent ? D3DZB_TRUE : D3DZB_FALSE);
}

//
//	SetEmissiveTarget
//

void SetEmissiveTarget(UBOOL Translucent)
{
	GDirect3DDevice->SetRenderTarget(0,GD3DSceneRenderer->GetNewLightingBuffer());
	GD3DRenderInterface.SetViewport(0,0,GD3DSceneRenderer->Context.SizeX,GD3DSceneRenderer->Context.SizeY);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,Translucent ? D3DCMP_GREATER : D3DCMP_LESSEQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
}

//
//	SetTranslucentLayerTarget
//

void SetTranslucentLayerTarget()
{
	GDirect3DDevice->SetRenderTarget(0,GD3DSceneRenderer->GetNewLightingBuffer());
	GD3DRenderInterface.SetViewport(0,0,GD3DSceneRenderer->Context.SizeX,GD3DSceneRenderer->Context.SizeY);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
}

//
//	SetUnlitTranslucencyTarget
//

void SetUnlitTranslucencyTarget()
{
	if(GD3DRenderDevice->UseFPTranslucency)
	{
		GDirect3DDevice->SetRenderTarget(0,GD3DSceneRenderer->GetNewLightingBuffer(0,1));
	}
	else
	{
		GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->LightRenderTarget->GetSurface());
		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_RGBA(0,0,0,255),0,0);
	}

	GD3DRenderInterface.SetViewport(0,0,GD3DSceneRenderer->Context.SizeX,GD3DSceneRenderer->Context.SizeY);

	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,1);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
}

//
//	FD3DCubeHelper - Vertices and indices for a unit cube.  Used for rendering occlusion query bounding boxes.
//

struct FD3DCubeHelper
{
	static FVector				Vertices[8];
	static _WORD				Indices[12 * 3];
	static D3DVERTEXELEMENT9	VertexElement;

	static _WORD VertexIndex(UBOOL X,UBOOL Y,UBOOL Z) { return Z | (2 * Y) | (4 * X); }
};

_WORD FD3DCubeHelper::Indices[] =
{
	FD3DCubeHelper::VertexIndex(0,0,0), FD3DCubeHelper::VertexIndex(0,1,0), FD3DCubeHelper::VertexIndex(0,1,1),
	FD3DCubeHelper::VertexIndex(0,0,0), FD3DCubeHelper::VertexIndex(0,1,1), FD3DCubeHelper::VertexIndex(0,0,1),
	FD3DCubeHelper::VertexIndex(1,0,0), FD3DCubeHelper::VertexIndex(1,0,1), FD3DCubeHelper::VertexIndex(1,1,1),
	FD3DCubeHelper::VertexIndex(1,0,0), FD3DCubeHelper::VertexIndex(1,1,1), FD3DCubeHelper::VertexIndex(1,1,0),
	FD3DCubeHelper::VertexIndex(0,0,0), FD3DCubeHelper::VertexIndex(0,0,1), FD3DCubeHelper::VertexIndex(1,0,1),
	FD3DCubeHelper::VertexIndex(0,0,0), FD3DCubeHelper::VertexIndex(1,0,1), FD3DCubeHelper::VertexIndex(1,0,0),
	FD3DCubeHelper::VertexIndex(0,1,0), FD3DCubeHelper::VertexIndex(1,1,0), FD3DCubeHelper::VertexIndex(1,1,1),
	FD3DCubeHelper::VertexIndex(0,1,0), FD3DCubeHelper::VertexIndex(1,1,1), FD3DCubeHelper::VertexIndex(0,1,1),
	FD3DCubeHelper::VertexIndex(0,0,0), FD3DCubeHelper::VertexIndex(1,0,0), FD3DCubeHelper::VertexIndex(1,1,0),
	FD3DCubeHelper::VertexIndex(0,0,0), FD3DCubeHelper::VertexIndex(1,1,0), FD3DCubeHelper::VertexIndex(0,1,0),
	FD3DCubeHelper::VertexIndex(0,0,1), FD3DCubeHelper::VertexIndex(0,1,1), FD3DCubeHelper::VertexIndex(1,1,1),
	FD3DCubeHelper::VertexIndex(0,0,1), FD3DCubeHelper::VertexIndex(1,1,1), FD3DCubeHelper::VertexIndex(1,0,1),
};

D3DVERTEXELEMENT9 FD3DCubeHelper::VertexElement =
{
	0,						// Stream
	0,						// Offset
	D3DDECLTYPE_FLOAT3,		// Type
	D3DDECLMETHOD_DEFAULT,	// Method
	D3DDECLUSAGE_POSITION,	// Usage
	0						// UsageIndex
};

//
//	FD3DSceneRenderer::RenderLayer
//

UBOOL FD3DSceneRenderer::RenderLayer(UBOOL Translucent)
{
	TArray<ULightComponent*> VisibleLayerLights;

	// Render the emissive primitives.

	BEGIND3DCYCLECOUNTER(GD3DStats.EmissiveRenderTime);

	SetEmissiveTarget(Translucent);

	UBOOL	EmissiveDirty = 0;
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(Translucent ? PLM_Translucent : PLM_DepthSorted);PrimitiveIt;PrimitiveIt.Next())
	{
		if(!PrimitiveIt->IsOccluded())
		{
			FD3DEmissivePRI	EmissivePRI(PrimitiveIt->Primitive,Translucent == 0 ? SP_OpaqueEmissive : SP_TranslucentEmissive);
			PrimitiveIt->PVI->Render(Context,&EmissivePRI);

			if(EmissivePRI.HasLitMaterial)
			{
				for(INT LightIndex = 0;LightIndex < PrimitiveIt->Primitive->Lights.Num();LightIndex++)
					VisibleLayerLights.AddUniqueItem(PrimitiveIt->Primitive->Lights(LightIndex).GetLight());
			}
			else
				PrimitiveIt->Lit = 0;

			EmissiveDirty |= EmissivePRI.Dirty;
		}
	}

	if(EmissiveDirty)
		FinishLightingLayer();
	else
		return 0;

	GD3DStats.SceneLayers.Value++;

	if(GD3DRenderDevice->DumpRenderTargets)
		GD3DRenderDevice->SaveRenderTarget(GetNewLightingBuffer(),TEXT("EmissiveBuffer"));

	ENDD3DCYCLECOUNTER;

	if(View->IsLit())
	{
		// Render primitive lighting.

		for(INT LightIndex = 0;LightIndex < VisibleLayerLights.Num();LightIndex++)
		{
			ULightComponent* Light = VisibleLayerLights(LightIndex);

			if(Light->Brightness == 0.0f || !Light->bEnabled || Light->IsA(USkyLightComponent::StaticClass()))
				continue;

			FD3DLightInfo* LightInfo = VisibleLights.Find(Light);

			check(LightInfo);

			// Write this light to the temporary lighting buffer.

			RenderLight(Light,LightInfo,Translucent);
			FinishLightingLayer();

			if(GD3DRenderDevice->DumpRenderTargets)
			{
#ifndef XBOX
				GD3DRenderDevice->SaveRenderTarget(GetTextureSurface(GetLastLightingBuffer()),TEXT("LightingBuffer"));
				GD3DRenderDevice->SaveRenderTarget(GetTextureSurface(GetLastLightingBuffer()),TEXT("LightingBufferAlpha"),1);
#else
				GD3DRenderDevice->SaveRenderTarget(GetNewLightingBuffer(),TEXT("LightingBuffer"));
				GD3DRenderDevice->SaveRenderTarget(GetNewLightingBuffer(),TEXT("LightingBufferAlpha"),1);
#endif
			}

			GD3DStats.LightPasses.Value++;
		}
	}

	// Compute scene fogging.

	if(View->ProjectionMatrix.M[3][3] < 1.0f && (View->ShowFlags & SHOW_Fog) && !(View->ViewMode & SVM_WireframeMask))
	{
		SetFogTarget(Translucent);
		GD3DRenderDevice->FogShader->Set(GetLastLightingBuffer());
		GD3DRenderInterface.DrawQuad(
			0,
			0,
			Context.SizeX,
			Context.SizeY,
			0,
			0,
			(FLOAT)Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX,
			(FLOAT)Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY
			);
		FinishLightingLayer();
	}

	return 1;
}

//
//	FD3DSceneRenderer::Render
//

void FD3DSceneRenderer::Render()
{
	FD3DSceneViewState*	CachedViewState = View->State ? (FD3DSceneViewState*)GD3DRenderDevice->Resources.GetCachedResource(View->State) : NULL;

	GDirect3DDevice->SetDepthStencilSurface(GD3DRenderDevice->DepthBuffer);

	FLOAT	BufferUL = (FLOAT)Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX,
			BufferVL = (FLOAT)Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY;

	// Clear the lighting buffers to the background color.

	FLinearColor	LinearBackgroundColor(View->BackgroundColor);
	DWORD			BackgroundColorD3D = D3DCOLOR_RGBA(
											Clamp(appTrunc(LinearBackgroundColor.R * 255.0f),0,255),
											Clamp(appTrunc(LinearBackgroundColor.G * 255.0f),0,255),
											Clamp(appTrunc(LinearBackgroundColor.B * 255.0f),0,255),
											0
											);

	GDirect3DDevice->SetRenderTarget(0,GetNewLightingBuffer(0,0,1));
	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,BackgroundColorD3D,0.0f,0);
	FinishLightingLayer();

	GDirect3DDevice->SetRenderTarget(0,GetNewLightingBuffer(0,0,1));
	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,BackgroundColorD3D,1.0f,0);

	// Render background primitives.

	BEGIND3DCYCLECOUNTER(GD3DStats.UnlitRenderTime);

	SetUnlitTarget();

	UBOOL	BackgroundDirty = 0;
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Background);PrimitiveIt;PrimitiveIt.Next())
	{
		FD3DUnlitPRI	BackgroundPRI(PrimitiveIt->Primitive,SP_Unlit);
		PrimitiveIt->PVI->RenderBackground(Context,&BackgroundPRI);
		BackgroundDirty |= BackgroundPRI.Dirty;
	}

	if(BackgroundDirty)
	{
		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,0,1.0f,0);
		ClearLightingBufferDepth(0.0f);
		FinishLightingLayer();
	}

	if(GD3DRenderDevice->DumpRenderTargets)
#ifndef XBOX
		GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetSurface(),TEXT("Background"),0);
#else
		GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTarget->GetSurface(),TEXT("Background"),0);
#endif

	ENDD3DCYCLECOUNTER;

	// Incrementally find the translucent samples that have GD3DRenderDevice->MaxTranslucencyLayers - 1 layers in front of them.

	TD3DRef<IDirect3DQuery9>*	TranslucentLayerQuery = GD3DRenderDevice->EnableOcclusionQuery ? &TranslucencyLayerQueries(0) : NULL;
	INT							MaxVisibleTranslucencyLayer = -1;

	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,0,1.0f,0);

	for(INT LayerIndex = 0;LayerIndex < GD3DRenderDevice->MaxTranslucencyLayers;LayerIndex++)
	{
		SetTranslucentLayerTarget();

		UBOOL	LayerDirty = 0;
		for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Translucent);PrimitiveIt;PrimitiveIt.Next())
		{
			if(GD3DRenderDevice->EnableOcclusionQuery)
				(*TranslucentLayerQuery)->Issue(D3DISSUE_BEGIN);

			FD3DTranslucentLayerPRI	LayerPRI(PrimitiveIt->Primitive,SP_TranslucentEmissive,LayerIndex);
			PrimitiveIt->PVI->Render(Context,&LayerPRI);
			LayerDirty |= LayerPRI.Dirty;

			if(GD3DRenderDevice->EnableOcclusionQuery)
				(*TranslucentLayerQuery)->Issue(D3DISSUE_END);

			TranslucentLayerQuery++;
		}

		if(!LayerDirty)
			break;

		FinishLightingLayer();
		MaxVisibleTranslucencyLayer++;

		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,0,1.0f,0);
		ClearLightingBufferDepth(D3DX_16F_MAX);

		if(GD3DRenderDevice->DumpRenderTargets)
#ifndef XBOX
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetSurface(),TEXT("TranslucencyLayer"),1);
#else
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTarget->GetSurface(),TEXT("TranslucencyLayer"),1);
#endif
	}

	// Render the depth pre-pass.

	BEGIND3DCYCLECOUNTER(GD3DStats.DepthRenderTime);

	GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->LightRenderTarget->GetSurface());
	GD3DRenderInterface.SetViewport(0,0,Context.SizeX,Context.SizeY);

	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);

	FD3DDepthOnlyShader* DepthOnlyShader = (FD3DDepthOnlyShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)SF_DepthOnly,GEngine->DefaultMaterial->GetMaterialInterface(0),&FLocalVertexFactory::StaticType);
	DepthOnlyShader->Set();

	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Opaque);PrimitiveIt;PrimitiveIt.Next())
	{
		if(!CachedViewState || CachedViewState->UnoccludedPrimitives.Find(PrimitiveIt->Primitive) != NULL)
		{
			FD3DDepthOnlyPRI DepthOnlyPRI(PrimitiveIt->Primitive,SP_OpaqueEmissive,DepthOnlyShader);
			PrimitiveIt->PVI->Render(Context,&DepthOnlyPRI);
		}
	}

	ENDD3DCYCLECOUNTER;

	// Perform occlusion queries on the bounding boxes of all visible primitives.

	BEGIND3DCYCLECOUNTER(GD3DStats.OcclusionQueryTime);

	// Disable writing to depth.

	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);

	// Set the vertex declaration for the cube vertices.

	TArray<D3DVERTEXELEMENT9>	CubeVertexElements;
	CubeVertexElements.AddItem(FD3DCubeHelper::VertexElement);
	GDirect3DDevice->SetVertexDeclaration(GD3DRenderDevice->CreateVertexDeclaration(CubeVertexElements));

	// Set the occlusion query shaders.

	GD3DRenderDevice->OcclusionQueryShader->Set();

	// Get the view frustum's near clipping plane.

	FPlane	NearPlane;
	UBOOL	CheckNearPlane = ViewProjectionMatrix.GetFrustumNearPlane(NearPlane);

	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_DepthSorted);PrimitiveIt;PrimitiveIt.Next())
	{
		FBoxSphereBounds Bounds = PrimitiveIt->Primitive->Bounds;

		// Enlarge the bounding box slightly to fix cases where a single BSP occludes itself.
		Bounds.BoxExtent += FVector(1,1,1);

		// Check for bounding boxes intersecting the near clipping plane.
		// If they're clipped away while rendering, the occlusion query may incorrectly say the primitive was occluded.
		// Instead, simply assume these primitives are definitely not occluded.

		if(!GD3DRenderDevice->EnableOcclusionQuery ||
			(CheckNearPlane && Abs(NearPlane.PlaneDot(Bounds.Origin)) < FBoxPushOut(NearPlane,Bounds.BoxExtent)) ||
			(PrimitiveIt->LayerMask & PLM_Foreground))
		{
			GD3DStats.DegenerateOcclusionQueries.Value++;
			PrimitiveIt->NotOccluded = 1;
		}
		else
		{
			// Perform an occlusion query for a single primitive's bounding box.

			FVector	Vertices[8] =
			{
				Bounds.Origin + FVector(-Bounds.BoxExtent.X,-Bounds.BoxExtent.Y,-Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(-Bounds.BoxExtent.X,-Bounds.BoxExtent.Y,+Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(-Bounds.BoxExtent.X,+Bounds.BoxExtent.Y,-Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(-Bounds.BoxExtent.X,+Bounds.BoxExtent.Y,+Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(+Bounds.BoxExtent.X,-Bounds.BoxExtent.Y,-Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(+Bounds.BoxExtent.X,-Bounds.BoxExtent.Y,+Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(+Bounds.BoxExtent.X,+Bounds.BoxExtent.Y,-Bounds.BoxExtent.Z),
				Bounds.Origin + FVector(+Bounds.BoxExtent.X,+Bounds.BoxExtent.Y,+Bounds.BoxExtent.Z)
			};

			PrimitiveIt->OcclusionQuery->Issue(D3DISSUE_BEGIN);
			GDirect3DDevice->DrawIndexedPrimitiveUP(
				D3DPT_TRIANGLELIST,
				0,
				ARRAY_COUNT(Vertices),
				12,
				FD3DCubeHelper::Indices,
				D3DFMT_INDEX16,
				Vertices,
				sizeof(Vertices[0])
				);
			PrimitiveIt->OcclusionQuery->Issue(D3DISSUE_END);
			PrimitiveIt->NotOccluded = 0;
			GD3DStats.UniqueOcclusionQueries.Value++;
		}
	}

	// Perform occlusion queries on the frustums of the shadows.

	for(TMap<ULightComponent*,FD3DLightInfo>::TIterator It(VisibleLights);It;++It)
	{
		ULightComponent* Light = It.Key();
		FD3DLightInfo* LightInfo = &It.Value();

		if(Light->Brightness == 0.0f || !Light->bEnabled || Light->IsA(USkyLightComponent::StaticClass()))
			continue;

		for(INT ShadowIndex = 0;ShadowIndex < LightInfo->Shadows.Num();ShadowIndex++)
		{
			FD3DShadowInfo* ShadowInfo = &LightInfo->Shadows(ShadowIndex);
			if(GD3DRenderDevice->EnableOcclusionQuery && (!CheckNearPlane || !ShadowInfo->ShadowFrustum.IntersectSphere(CameraPosition,NEAR_CLIPPING_PLANE * appSqrt(3.0f))))
			{
				const FMatrix& WorldToFrustum = ShadowInfo->ReceiverShadowMatrix;
				FMatrix FrustumToWorld = WorldToFrustum.Inverse();

				FVector	W = FVector(WorldToFrustum.M[0][3],WorldToFrustum.M[1][3],WorldToFrustum.M[2][3]),
						Z = FVector(WorldToFrustum.M[0][2],WorldToFrustum.M[1][2],WorldToFrustum.M[2][2]);

				FLOAT	MinZ = 0.0f,
						MaxZ = 1.0f,
						MinW = 1.0f,
						MaxW = 1.0f;

				if((Z|W) > DELTA*DELTA)
				{
					MinW = (MinZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MinZ) + WorldToFrustum.M[3][3];
					MaxW = (MaxZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MaxZ) + WorldToFrustum.M[3][3];
				}

				FVector	Vertices[8] =
				{
					FrustumToWorld.TransformFPlane(FPlane(-MinW,-MinW,(MinZ * MinW),MinW)),
					FrustumToWorld.TransformFPlane(FPlane(-MaxW,-MaxW,(MaxZ * MaxW),MaxW)),
					FrustumToWorld.TransformFPlane(FPlane(-MinW,+MinW,(MinZ * MinW),MinW)),
					FrustumToWorld.TransformFPlane(FPlane(-MaxW,+MaxW,(MaxZ * MaxW),MaxW)),
					FrustumToWorld.TransformFPlane(FPlane(+MinW,-MinW,(MinZ * MinW),MinW)),
					FrustumToWorld.TransformFPlane(FPlane(+MaxW,-MaxW,(MaxZ * MaxW),MaxW)),
					FrustumToWorld.TransformFPlane(FPlane(+MinW,+MinW,(MinZ * MinW),MinW)),
					FrustumToWorld.TransformFPlane(FPlane(+MaxW,+MaxW,(MaxZ * MaxW),MaxW)),
				};

				ShadowInfo->OcclusionQuery->Issue(D3DISSUE_BEGIN);
				GDirect3DDevice->DrawIndexedPrimitiveUP(
					D3DPT_TRIANGLELIST,
					0,
					ARRAY_COUNT(Vertices),
					12,
					FD3DCubeHelper::Indices,
					D3DFMT_INDEX16,
					Vertices,
					sizeof(Vertices[0])
					);
				ShadowInfo->OcclusionQuery->Issue(D3DISSUE_END);
				ShadowInfo->NotOccluded = 0;
				GD3DStats.UniqueOcclusionQueries.Value++;
			}
		}
	}

	// Reenable writing to color and depth.

	GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);

	ENDD3DCYCLECOUNTER;

	// Render the opaque layer.

	RenderLayer(0);

	// Check the translucent layer occlusion queries to find the number of visible translucent layers.

	if(GD3DRenderDevice->EnableOcclusionQuery)
	{
		BEGINCYCLECOUNTER(GD3DStats.TranslucencyLayerQueryTime);

		TranslucentLayerQuery = GD3DRenderDevice->EnableOcclusionQuery ? &TranslucencyLayerQueries(0) : NULL;
		for(INT LayerIndex = 0;LayerIndex <= MaxVisibleTranslucencyLayer;LayerIndex++)
		{
			UBOOL	LayerVisible = 0;

			for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Translucent);PrimitiveIt;PrimitiveIt.Next())
			{
				if(!PrimitiveIt->IsOccluded())
				{
					// If this primitive is unoccluded, check the number of pixels returned by it's occlusion query for the current layer.

					while(1) // Loop until the occlusion query completes.
					{
						DWORD	NumPixelsVisible = 0;
						HRESULT	QueryResult = (*TranslucentLayerQuery)->GetData(&NumPixelsVisible,sizeof(DWORD),D3DGETDATA_FLUSH);
						if(QueryResult == S_OK)
						{
							if(NumPixelsVisible)
							{
								// The primitive has pixels visible in this layer, the layer is visible.
								LayerVisible = 1;
							}
							break;
						}
					}
				}
				TranslucentLayerQuery++;
			}

			if(!LayerVisible)
			{
				// No primitives were visible in this translucency layer.
				MaxVisibleTranslucencyLayer = LayerIndex - 1;
				break;
			}
		}

		ENDD3DCYCLECOUNTER;
	}

	// Render the unlit translucent primitives.

	SetUnlitTranslucencyTarget();
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Translucent);PrimitiveIt;PrimitiveIt.Next())
	{
		FD3DUnlitTranslucencyPRI	TranslucencyPRI(PrimitiveIt->Primitive);
		PrimitiveIt->PVI->Render(Context,&TranslucencyPRI);
	}
	if(GD3DRenderDevice->DumpRenderTargets)
	{
		if(GD3DRenderDevice->UseFPTranslucency)
		{
#ifndef XBOX
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetSurface(),TEXT("UnlitTranslucency"),0);
#else
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTarget->GetSurface(),TEXT("UnlitTranslucency"),0);
#endif
		}
		else
		{
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightRenderTarget->GetSurface(),TEXT("UnlitTranslucency"),0);
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightRenderTarget->GetSurface(),TEXT("UnlitTranslucency"),1);
		}
	}

	GDirect3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,0);

	if(GD3DRenderDevice->UseFPTranslucency)
	{
		FinishLightingLayer();
	}
	else
	{
		GD3DRenderDevice->LightRenderTarget->Resolve();
		GD3DRenderDevice->BlendImageShader->Set(GD3DRenderDevice->LightRenderTarget->GetTexture());
#ifndef XBOX
		GD3DRenderInterface.FillSurface(GetTextureSurface(GetLastLightingBuffer()),0,0,Context.SizeX,Context.SizeY,0,0,BufferUL,BufferVL);
#else
		GD3DRenderInterface.FillSurface(GD3DRenderDevice->LightingRenderTarget->GetSurface(),0,0,Context.SizeX,Context.SizeY,0,0,BufferUL,BufferVL);
		GD3DRenderDevice->LightingRenderTarget->Resolve();
#endif
	}

	// Render the translucent layers.

	for(INT LayerIndex = MaxVisibleTranslucencyLayer;LayerIndex >= 0;LayerIndex--)
	{
		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,0,0.0f,0);
		if(!RenderLayer(1))
			break;
	}

	// Render foreground primitives.

	BEGIND3DCYCLECOUNTER(GD3DStats.UnlitRenderTime);

	SetUnlitTarget();

	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,0,1.0f,0);

	UBOOL	ForegroundDirty = 0;
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Foreground);PrimitiveIt;PrimitiveIt.Next())
	{
		FD3DUnlitPRI	ForegroundPRI(PrimitiveIt->Primitive,SP_Unlit);
		PrimitiveIt->PVI->RenderForeground(Context,&ForegroundPRI);
		ForegroundDirty |= ForegroundPRI.Dirty;
	}

	if(Context.View->ShowFlags & SHOW_ShadowFrustums)
	{
		// Render the shadow frustum wireframes in the foreground.

		FD3DUnlitPRI ForegroundPRI(NULL,SP_Unlit);

		for(TMap<ULightComponent*,FD3DLightInfo>::TIterator It(VisibleLights);It;++It)
		{
			ULightComponent* Light = It.Key();
			FD3DLightInfo& LightInfo = It.Value();

			for(INT ShadowIndex = 0;ShadowIndex < LightInfo.Shadows.Num();ShadowIndex++)
			{
				FD3DShadowInfo& ShadowInfo = LightInfo.Shadows(ShadowIndex);
				if(!ShadowInfo.IsOccluded())
				{
					ForegroundPRI.DrawFrustumWireframe(ShadowInfo.ReceiverShadowMatrix,FColor(FGetHSV(ShadowInfo.SubjectPrimitive->GetIndex() & 255,0,255)));
				}
			}
		}
	}

	if(ForegroundDirty)
		FinishLightingLayer();
	if(GD3DRenderDevice->DumpRenderTargets)
#ifndef XBOX
		GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTargets[LastLightingBuffer]->GetSurface(),TEXT("Foreground"));
#else
		GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightingRenderTarget->GetSurface(),TEXT("Foreground"));
#endif

	ENDD3DCYCLECOUNTER;

	// Blur the lighting and add it to the frame buffer.

	BEGIND3DCYCLECOUNTER(GD3DStats.PostProcessTime);

	TD3DRef<IDirect3DTexture9>	ExposureTexture;
	FLOAT						FinalMultiple;

	if(GD3DRenderDevice->UsePostProcessEffects)
	{
		// Clear the blur buffers to black.
		for(UINT BufferIndex = 0;BufferIndex < ARRAY_COUNT(GD3DRenderDevice->BlurRenderTargets);BufferIndex++)
		{	
			GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->BlurRenderTargets[BufferIndex]->GetSurface());
			GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_RGBA(0,0,0,255),0.0f,0);
		}

		UINT	BlurredSceneSizeX = Context.SizeX / GD3DRenderDevice->CachedBlurBufferDivisor,
				BlurredSceneSizeY = Context.SizeY / GD3DRenderDevice->CachedBlurBufferDivisor;
		FLOAT	BlurBufferU = 1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeX,
				BlurBufferV = 1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeY,
				BlurBufferUL = (FLOAT)BlurredSceneSizeX / (FLOAT)GD3DRenderDevice->BlurBufferSizeX,
				BlurBufferVL = (FLOAT)BlurredSceneSizeY / (FLOAT)GD3DRenderDevice->BlurBufferSizeY;

		FVector2D	DownsampleOffsets[16];
		FPlane		DownsampleWeights[16];
		UINT		NumDownSamples;

		if(GD3DRenderDevice->UseFPFiltering)
		{
			NumDownSamples = 0;
			for(UINT Y = 0;Y < GD3DRenderDevice->CachedBlurBufferDivisor;Y += 2)
			{
				for(UINT X = 0;X < GD3DRenderDevice->CachedBlurBufferDivisor;X += 2)
				{
					UINT	XWidth = 1,
							YWidth = 1;
					DownsampleOffsets[NumDownSamples] = FVector2D(X,Y);
					if(X + 1 < GD3DRenderDevice->CachedBlurBufferDivisor)
					{
						DownsampleOffsets[NumDownSamples].X += 0.5f;
						XWidth = 2;
					}
					if(Y + 1 < GD3DRenderDevice->CachedBlurBufferDivisor)
					{
						DownsampleOffsets[NumDownSamples].Y += 0.5f;
						YWidth = 2;
					}
					DownsampleWeights[NumDownSamples] = FPlane(1,1,1,1) * XWidth * YWidth;
					NumDownSamples++;
				}
			}
		}
		else
		{
			for(UINT Y = 0;Y < GD3DRenderDevice->CachedBlurBufferDivisor;Y++)
			{
				for(UINT X = 0;X < GD3DRenderDevice->CachedBlurBufferDivisor;X++)
				{
					DownsampleOffsets[Y * GD3DRenderDevice->CachedBlurBufferDivisor + X] = FVector2D(X,Y);
					DownsampleWeights[Y * GD3DRenderDevice->CachedBlurBufferDivisor + X] = FPlane(1,1,1,1);
				}
			}
			NumDownSamples = Square(GD3DRenderDevice->CachedBlurBufferDivisor);
		}

		FinalMultiple = 1.0f / (1.0f + GD3DRenderDevice->BlurAlpha);

		// Downsample the linear framebuffer.

		LastBlurBuffer = 0;

		GD3DRenderDevice->BlurShaders.GetCachedShader(NumDownSamples)->Set(GetLastLightingBuffer(),DownsampleOffsets,DownsampleWeights,FPlane(1,1,1,1),GD3DRenderDevice->UseFPFiltering);
		GD3DRenderInterface.FillSurface(
			GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(),
			1,
			1,
			BlurredSceneSizeX,
			BlurredSceneSizeY,
			+0.5f / (FLOAT)GD3DRenderDevice->BufferSizeX - 0.5f / (FLOAT)GD3DRenderDevice->BlurBufferSizeX, // Cancel the offset applied in FD3DCanvasVertex::Set.
			+0.5f / (FLOAT)GD3DRenderDevice->BufferSizeY - 0.5f / (FLOAT)GD3DRenderDevice->BlurBufferSizeY,
			BufferUL,
			BufferVL
			);

		if(GD3DRenderDevice->DumpRenderTargets)
			GD3DRenderDevice->SaveRenderTarget( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(), TEXT("BlurBuffer") );

		// Blur the downsampled framebuffer iteratively.

		const FLOAT	BlurAttenuation = GD3DRenderDevice->BlurAttenuation,
					BlurScaleX = 128.0f / BlurredSceneSizeX / View->ProjectionMatrix.M[0][0],
					BlurScaleY = 128.0f / BlurredSceneSizeY / View->ProjectionMatrix.M[1][1];
		FVector2D	BlurOffsets[16];
		FPlane		BlurWeights[16];

		const UINT	NumIterations = 2;
		for(UINT Iteration = 0;Iteration < NumIterations;Iteration++)
		{
			UINT NumSamples = 0;
			if(GD3DRenderDevice->UseFPFiltering)
			{
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex += 2)
				{
					BlurOffsets[NumSamples] = FVector2D(-SampleIndex * appPow(16,Iteration) - 1.0f + 1.0f / (1.0f + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX)),0);
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * (appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX * SampleIndex) + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX * (SampleIndex + 1)));
					NumSamples++;
				}
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex += 2)
				{
					BlurOffsets[NumSamples] = FVector2D(SampleIndex * appPow(16,Iteration) + 1.0f - 1.0f / (1.0f + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX)),0);
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * (appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX * SampleIndex) + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX * (SampleIndex + 1)));
					NumSamples++;
				}
			}
			else
			{
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex++)
				{
					BlurOffsets[NumSamples] = FVector2D(SampleIndex * appPow(16,Iteration),0);
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * appPow(BlurAttenuation,appPow(16,Iteration) * SampleIndex * BlurScaleX);
					NumSamples++;
				}
			}
			GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->Resolve();
			GD3DRenderDevice->BlurShaders.GetCachedShader(NumSamples)->Set(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetTexture(),BlurOffsets,BlurWeights,FPlane(1,1,1,1),GD3DRenderDevice->UseFPFiltering);
			GD3DRenderInterface.FillSurface(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer ? 0 : 1]->GetSurface(),1,1,BlurredSceneSizeX,BlurredSceneSizeY,BlurBufferU,BlurBufferV,BlurBufferUL,BlurBufferVL);
			LastBlurBuffer = LastBlurBuffer ? 0 : 1;
		}

		if(GD3DRenderDevice->DumpRenderTargets)
			GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(),TEXT("BlurBuffer"));

		if(!GD3DRenderDevice->UseFPFiltering)
		{
			for(UINT Iteration = 0;Iteration < NumIterations;Iteration++)
			{
				UINT NumSamples = 0;
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex++)
				{
					BlurOffsets[NumSamples] = FVector2D(-SampleIndex * appPow(16,Iteration),0);
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * appPow(BlurAttenuation,appPow(16,Iteration) * SampleIndex * BlurScaleX);
					NumSamples++;
				}
				GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->Resolve();
				GD3DRenderDevice->BlurShaders.GetCachedShader(NumSamples)->Set(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetTexture(),BlurOffsets,BlurWeights,FPlane(1,1,1,1),GD3DRenderDevice->UseFPFiltering);
				GD3DRenderInterface.FillSurface(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer ? 0 : 1]->GetSurface(),1,1,BlurredSceneSizeX,BlurredSceneSizeY,BlurBufferU,BlurBufferV,BlurBufferUL,BlurBufferVL);
				LastBlurBuffer = LastBlurBuffer ? 0 : 1;
			}

			if(GD3DRenderDevice->DumpRenderTargets)
				GD3DRenderDevice->SaveRenderTarget( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(), TEXT("BlurBuffer") );
		}

		for(UINT Iteration = 0;Iteration < NumIterations;Iteration++)
		{
			UINT NumSamples = 0;
			if(GD3DRenderDevice->UseFPFiltering)
			{
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex += 2)
				{
					BlurOffsets[NumSamples] = FVector2D(0,-SampleIndex * appPow(16,Iteration) - 1.0f + 1.0f / (1.0f + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleY)));
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * (appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX * SampleIndex) + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleY * (SampleIndex + 1)));
					NumSamples++;
				}
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex += 2)
				{
					BlurOffsets[NumSamples] = FVector2D(0,SampleIndex * appPow(16,Iteration) + 1.0f - 1.0f / (1.0f + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleY)));
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * (appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleX * SampleIndex) + appPow(BlurAttenuation,appPow(16,Iteration) * BlurScaleY * (SampleIndex + 1)));
					NumSamples++;
				}
			}
			else
			{
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex++)
				{
					BlurOffsets[NumSamples] = FVector2D(0,SampleIndex * appPow(16,Iteration));
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * appPow(BlurAttenuation,appPow(16,Iteration) * SampleIndex * BlurScaleY);
					NumSamples++;
				}
			}
			GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->Resolve();
			GD3DRenderDevice->BlurShaders.GetCachedShader(NumSamples)->Set(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetTexture(),BlurOffsets,BlurWeights,FPlane(1,1,1,1),GD3DRenderDevice->UseFPFiltering);
			GD3DRenderInterface.FillSurface( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer ? 0 : 1]->GetSurface(), 1, 1, BlurredSceneSizeX, BlurredSceneSizeY, BlurBufferU, BlurBufferV, BlurBufferUL, BlurBufferVL );
			LastBlurBuffer = LastBlurBuffer ? 0 : 1;
		}

		if(GD3DRenderDevice->DumpRenderTargets)
			GD3DRenderDevice->SaveRenderTarget( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(), TEXT("BlurBuffer") );

		if(!GD3DRenderDevice->UseFPFiltering)
		{
			for(UINT Iteration = 0;Iteration < NumIterations;Iteration++)
			{
				UINT NumSamples = 0;
				for(INT SampleIndex = 0;SampleIndex < 16;SampleIndex++)
				{
					BlurOffsets[NumSamples] = FVector2D(0,-SampleIndex * appPow(16,Iteration));
					BlurWeights[NumSamples] = FPlane(1,1,1,1) * appPow(BlurAttenuation,appPow(16,Iteration) * SampleIndex * BlurScaleY);
					NumSamples++;
				}
				GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->Resolve();
				GD3DRenderDevice->BlurShaders.GetCachedShader(NumSamples)->Set(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetTexture(),BlurOffsets,BlurWeights,FPlane(1,1,1,1),GD3DRenderDevice->UseFPFiltering);
				GD3DRenderInterface.FillSurface( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer ? 0 : 1]->GetSurface(), 1, 1, BlurredSceneSizeX, BlurredSceneSizeY, BlurBufferU, BlurBufferV, BlurBufferUL, BlurBufferVL );
				LastBlurBuffer = LastBlurBuffer ? 0 : 1;
			}

			if(GD3DRenderDevice->DumpRenderTargets)
				GD3DRenderDevice->SaveRenderTarget( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(), TEXT("BlurBuffer") );
		}

		// Add the blurred lighting to the lighting buffer.

		GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->Resolve();
		GD3DRenderDevice->AccumulateImageShader->Set(GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetTexture(),FLinearColor::White * GD3DRenderDevice->BlurAlpha / FinalMultiple);
#ifndef XBOX
		GD3DRenderInterface.FillSurface(GetTextureSurface(GetLastLightingBuffer()),0,0,Context.SizeX,Context.SizeY,0,0,BufferUL,BufferVL);
#else
		GD3DRenderInterface.FillSurface(GD3DRenderDevice->LightingRenderTarget->GetSurface(),0,0,Context.SizeX,Context.SizeY,0,0,BufferUL,BufferVL);
		GD3DRenderDevice->LightingRenderTarget->Resolve();
#endif
	}
	else
		FinalMultiple = 1.0f;

	if(GD3DRenderDevice->DumpRenderTargets)
		GD3DRenderDevice->SaveRenderTarget(GetTextureSurface(GetLastLightingBuffer()), TEXT("FinalLightBuffer") );

#ifndef XBOX
	//@hack xenon: don't make use of automatic brightness adjustment, requires render target mojo
	if(View->AutomaticBrightness && CachedViewState)
	{
		// Iteratively downsample the linear framebuffer to a 1x1 texture.

		LastBlurBuffer = 0;
		TD3DRef<IDirect3DTexture9>	DownsampleSourceTexture = GetLastLightingBuffer();
		UINT						DownsampleSourceSizeX = Context.SizeX,
									DownsampleSourceSizeY = Context.SizeY,
									DownsampleBufferSizeX = GD3DRenderDevice->BufferSizeX,
									DownsampleBufferSizeY = GD3DRenderDevice->BufferSizeY;

		while(DownsampleSourceSizeX > 1 || DownsampleSourceSizeY > 1)
		{
			UINT	DestSizeX = Max<UINT>((DownsampleSourceSizeX + 3) / 4,1),
					DestSizeY = Max<UINT>((DownsampleSourceSizeY + 3) / 4,1),
					FilterSizeX = DownsampleSourceSizeX / DestSizeX,
					FilterSizeY = DownsampleSourceSizeY / DestSizeY;

			FVector2D	FilterOffsets[16];
			FPlane		FilterWeights[16];
			for(UINT Y = 0;Y < FilterSizeY;Y++)
			{
				for(UINT X = 0;X < FilterSizeX;X++)
				{
					FilterOffsets[Y * FilterSizeX + X] = FVector2D(X - (FLOAT)FilterSizeX / 2.0f,Y - (FLOAT)FilterSizeY / 2.0f);
					FilterWeights[Y * FilterSizeX + X] = FPlane(1,1,1,1);
				}
			}

			LastBlurBuffer = 1 - LastBlurBuffer;
			GDirect3DDevice->ColorFill( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(), NULL, D3DCOLOR_RGBA(0,0,0,0) );
			GD3DRenderDevice->BlurShaders.GetCachedShader(FilterSizeX * FilterSizeY)->Set(DownsampleSourceTexture,FilterOffsets,FilterWeights,FPlane(1,1,1,1));
			GD3DRenderInterface.FillSurface(
				GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(),
				0,
				0,
				DestSizeX,
				DestSizeY,
				0,
				0,
				(FLOAT)DownsampleSourceSizeX / (FLOAT)DownsampleBufferSizeX,
				(FLOAT)DownsampleSourceSizeY / (FLOAT)DownsampleBufferSizeY
				);

			if(GD3DRenderDevice->DumpRenderTargets)
				GD3DRenderDevice->SaveRenderTarget( GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetSurface(),TEXT("DownsampleBuffer"));

			DownsampleSourceTexture = GD3DRenderDevice->BlurRenderTargets[LastBlurBuffer]->GetTexture();
			DownsampleSourceSizeX = DestSizeX;
			DownsampleSourceSizeY = DestSizeY;
			DownsampleBufferSizeX = GD3DRenderDevice->BlurBufferSizeX;
			DownsampleBufferSizeY = GD3DRenderDevice->BlurBufferSizeY;
		};

		// Calculate the exposure for this frame based on the view parameters, the last frame's exposure, and the time since the last frame.

		DOUBLE	CurrentFrameTime = appSeconds();
		GD3DRenderDevice->ExposureShader->Set(DownsampleSourceTexture,CachedViewState->ExposureTextures[CachedViewState->LastExposureTexture],(CurrentFrameTime - CachedViewState->LastExposureTime) / 3.0f,View->ManualBrightnessScale);
		CachedViewState->LastExposureTexture = 1 - CachedViewState->LastExposureTexture;
		CachedViewState->LastExposureTime = CurrentFrameTime;
		GD3DRenderInterface.FillSurface(GetTextureSurface(CachedViewState->ExposureTextures[CachedViewState->LastExposureTexture]),0,0,1,1,0,0,0,0);
		ExposureTexture = CachedViewState->ExposureTextures[CachedViewState->LastExposureTexture];
	}
	else
	{
		// Put the manual exposure in a texture for the finishImage shader to use.

		GD3DRenderDevice->FillShader->Set(FLinearColor(View->ManualBrightnessScale,0.0f,0.0f,0.0f));
		GD3DRenderInterface.FillSurface( GD3DRenderDevice->ManualExposureRenderTarget->GetSurface(), 0, 0, 1, 1, 0, 0, 0, 0 );
		ExposureTexture = GD3DRenderDevice->ManualExposureRenderTarget->GetTexture();
	}
#endif

	// Gamma correct the light buffer and write it to the backbuffer.

	//GDirect3DDevice->SetRenderState(D3DRS_SRGBWRITEENABLE,TRUE);
	GD3DRenderDevice->FinishImageShader->Set(GetLastLightingBuffer(),ExposureTexture,FLinearColor::White * FinalMultiple,View->OverlayColor);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GD3DRenderInterface.FillSurface(
		GD3DRenderDevice->BackBuffer,
		Context.X,
		Context.Y,
		Context.SizeX,
		Context.SizeY,
		+0.5f / (FLOAT)GD3DRenderDevice->BufferSizeX - 0.5f / (FLOAT)GD3DRenderDevice->DeviceSizeX,
		+0.5f / (FLOAT)GD3DRenderDevice->BufferSizeY - 0.5f / (FLOAT)GD3DRenderDevice->DeviceSizeY,
		BufferUL,
		BufferVL);
	//GDirect3DDevice->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);

	ENDD3DCYCLECOUNTER;

	// Restore state.

	GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->BackBuffer);
	GD3DRenderInterface.SetViewport(0,0,GD3DRenderInterface.CurrentViewport->SizeX,GD3DRenderInterface.CurrentViewport->SizeY);
	GDirect3DDevice->SetDepthStencilSurface(NULL);
	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);

	if(CachedViewState)
	{
		// Remember the primitives which were unoccluded for the next frame's occluders.

		CachedViewState->UnoccludedPrimitives.Empty();
		for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Opaque);PrimitiveIt;PrimitiveIt.Next())
		{
			if(!PrimitiveIt->IsOccluded())
			{
				CachedViewState->UnoccludedPrimitives.Set(PrimitiveIt->Primitive,1);
			}
		}
	}
}

//
//	FD3DSceneRenderer::RenderHitProxies
//

void FD3DSceneRenderer::RenderHitProxies()
{
	GDirect3DDevice->SetDepthStencilSurface(GD3DRenderDevice->DepthBuffer);
	GD3DRenderInterface.SetViewport(Context.X,Context.Y,Context.SizeX,Context.SizeY);
	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,0,1.0f,0);

	// Render background primitives.

	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Background);PrimitiveIt;PrimitiveIt.Next())
	{
		FD3DHitTestingPRI	UnlitHitTestingPRI(PrimitiveIt->Primitive,SP_UnlitHitTest);
		PrimitiveIt->PVI->RenderBackgroundHitTest(Context,&UnlitHitTestingPRI);
	}

	// Render depth-sorted primitives.

	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_DepthSorted);PrimitiveIt;PrimitiveIt.Next())
	{
		FD3DHitTestingPRI	HitTestingPRI(PrimitiveIt->Primitive,SP_DepthSortedHitTest);
		PrimitiveIt->PVI->RenderHitTest(Context,&HitTestingPRI);
	}

	// Render foreground primitives.

	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	for(FD3DVisiblePrimitiveIterator PrimitiveIt(PLM_Foreground);PrimitiveIt;PrimitiveIt.Next())
	{
		FD3DHitTestingPRI	UnlitHitTestingPRI(PrimitiveIt->Primitive,SP_UnlitHitTest);
		PrimitiveIt->PVI->RenderForegroundHitTest(Context,&UnlitHitTestingPRI);
	}
	GD3DRenderInterface.SetHitProxy(NULL);

	// Restore state.

	GD3DRenderInterface.SetViewport(0,0,GD3DRenderInterface.CurrentViewport->SizeX,GD3DRenderInterface.CurrentViewport->SizeY);
	GDirect3DDevice->SetDepthStencilSurface(NULL);
	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_FALSE);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
}
