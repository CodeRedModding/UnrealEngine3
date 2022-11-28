/*=============================================================================
	D3DSceneLight.cpp: Unreal Direct3D scene light rendering.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	FD3DShadowVolumeSRI
//

struct FD3DShadowVolumeSRI: FShadowRenderInterface
{
	FSceneView*			View;
	FScene*				Scene;
	ULightComponent*	Light;

	// Constructor.

	FD3DShadowVolumeSRI(FD3DSceneRenderer* SceneRenderer,ULightComponent* InLight):
		Scene(SceneRenderer->Scene),
		View(SceneRenderer->View),
		Light(InLight)
	{}

	// FPrimitiveRenderInterface interface.

	virtual void DrawShadowVolume(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,EShadowStencilMode StencilMode)
	{
		FD3DVertexFactory*				CachedVertexFactory = (FD3DVertexFactory*)GD3DRenderDevice->Resources.GetCachedResource(VertexFactory);
		TD3DRef<IDirect3DIndexBuffer9>	CachedIndexBuffer = NULL;
		UINT							BaseIndex = 0;

		switch(StencilMode)
		{
		case SSM_ZFail:
			GDirect3DDevice->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
			GDirect3DDevice->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_INCR);
			GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILPASS,D3DSTENCILOP_KEEP);
			GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL,D3DSTENCILOP_DECR);
			break;
		case SSM_ZPass:
			GDirect3DDevice->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_DECR);
			GDirect3DDevice->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);
			GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILPASS,D3DSTENCILOP_INCR);
			GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL,D3DSTENCILOP_KEEP);
			break;
		default:
			appErrorf(TEXT("Unknown shadow stencil mode: %x"),(UINT)StencilMode);
		};

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
		GD3DRenderDevice->ShadowVolumeShader->Set(VertexFactory,Light);

		if(CachedIndexBuffer)
		{
			GDirect3DDevice->SetIndices(CachedIndexBuffer);
			GDirect3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,BaseVertexIndex,MinVertexIndex,MaxVertexIndex - MinVertexIndex + 1,BaseIndex + FirstIndex,NumTriangles);
		}
		else
			GDirect3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST,BaseVertexIndex + FirstIndex,NumTriangles);

	}
};

//
//	FD3DStaticLightPRI
//

struct FD3DStaticLightPRI: FStaticLightPrimitiveRenderInterface
{
	UPrimitiveComponent*	Primitive;
	ED3DScenePass			Pass;
	ULightComponent*		Light;

	// Constructor.

	FD3DStaticLightPRI(UPrimitiveComponent* InPrimitive,ED3DScenePass InPass,ULightComponent* InLight):
		Primitive(InPrimitive),
		Pass(InPass),
		Light(InLight)
	{}

	// DrawVisibilityTexture

	virtual void DrawVisibilityTexture(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,FTexture2D* VisibilityTexture,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		FD3DMeshLightPRI	LightPRI(Primitive,Pass,Light,SLT_TextureMask,(FD3DTexture2D*)GD3DRenderDevice->Resources.GetCachedResource(VisibilityTexture));
		LightPRI.DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
	}

	// DrawVisibilityVertexMask

	virtual void DrawVisibilityVertexMask(FVertexFactory* VertexFactory,FIndexBuffer* IndexBuffer,FMaterial* Material,FMaterialInstance* MaterialInstance,UINT FirstIndex,UINT NumTriangles,UINT MinVertexIndex,UINT MaxVertexIndex,ECullMode CullMode)
	{
		FD3DMeshLightPRI	LightPRI(Primitive,Pass,Light,SLT_VertexMask);
		LightPRI.DrawMesh(VertexFactory,IndexBuffer,Material,MaterialInstance,FirstIndex,NumTriangles,MinVertexIndex,MaxVertexIndex,CullMode);
	}
};

//
//	FD3DShadowDepthPRI
//

struct FD3DShadowDepthPRI: FD3DMeshPRI
{
	FD3DShadowInfo* ShadowInfo;
	UBOOL Pre;

	// Constructor.

	FD3DShadowDepthPRI(UPrimitiveComponent* InPrimitive,FD3DShadowInfo* InShadowInfo,UBOOL InPre):
		FD3DMeshPRI(InPrimitive,SP_ShadowDepth),
		ShadowInfo(InShadowInfo),
		Pre(InPre)
	{}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		DWORD Function = SF_ShadowDepth;
		if(GD3DRenderDevice->UseFPBlending)
			Function |= SF_UseFPBlending;
		FD3DShadowDepthShader*	CachedShader = (FD3DShadowDepthShader*)GD3DRenderDevice->MeshShaders.GetCachedShader((ED3DShaderFunction)Function,Material,VertexFactory->Type());
		CachedShader->Set(Primitive,VertexFactory,MaterialInstance,ShadowInfo->SubjectShadowMatrix,Pre ? ShadowInfo->ShadowMatrix : ShadowInfo->SubjectShadowMatrix,ShadowInfo->MaxShadowZ);
	}
};

//
//	FD3DShadowDepthTestPRI
//

struct FD3DShadowDepthTestPRI: FD3DMeshPRI
{
	FD3DShadowInfo* ShadowInfo;
	UBOOL DirectionalLight;
	UBOOL Pre;

	// Constructor.

	FD3DShadowDepthTestPRI(UPrimitiveComponent* InPrimitive,FD3DShadowInfo* InShadowInfo,UBOOL InDirectionalLight,UBOOL InPre):
		FD3DMeshPRI(InPrimitive,SP_LightFunction),
		ShadowInfo(InShadowInfo),
		DirectionalLight(InDirectionalLight),
		Pre(InPre)
	{}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		if(GD3DRenderDevice->UseHWShadowMaps)
		{
			FD3DNVShadowDepthTestShader*	CachedShader = (FD3DNVShadowDepthTestShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_NVShadowDepthTest,GEngine->DefaultMaterial->MaterialResources[0],VertexFactory->Type());
			CachedShader->Set(Primitive,VertexFactory,MaterialInstance,ShadowInfo->SubjectShadowMatrix,ShadowInfo->MaxShadowZ,DirectionalLight,Pre ? ShadowInfo->Resolution / 2 : ShadowInfo->Resolution);
		}
		else
		{
			FD3DShadowDepthTestShader*	CachedShader = (FD3DShadowDepthTestShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_ShadowDepthTest,GEngine->DefaultMaterial->MaterialResources[0],VertexFactory->Type());
			CachedShader->Set(Primitive,VertexFactory,MaterialInstance,ShadowInfo->SubjectShadowMatrix,ShadowInfo->MaxShadowZ,DirectionalLight,Pre ? ShadowInfo->Resolution / 2 : ShadowInfo->Resolution);
		}
	}
};

//
//	FD3DLightFunctionPRI
//

struct FD3DLightFunctionPRI: FD3DMeshPRI
{
	ULightComponent*	LightComponent;

	// Constructor.

	FD3DLightFunctionPRI(UPrimitiveComponent* InPrimitive,ULightComponent* InLightComponent):
		FD3DMeshPRI(InPrimitive,SP_LightFunction),
		LightComponent(InLightComponent)
	{}

	// FD3DMeshPRI interface.

	virtual void Set(FVertexFactory* VertexFactory,FMaterial* Material,FMaterialInstance* MaterialInstance)
	{
		// Determine the world to light transform.

		FD3DLightFunctionShader*	CachedShader = (FD3DLightFunctionShader*)GD3DRenderDevice->MeshShaders.GetCachedShader(SF_LightFunction,LightComponent->Function->SourceMaterial->GetMaterialInterface(0),VertexFactory->Type());
		CachedShader->Set(Primitive,VertexFactory,LightComponent->Function->SourceMaterial->GetInstanceInterface(),LightComponent->WorldToLight * FScaleMatrix(FVector(LightComponent->Function->Scale)).Inverse());
	}
};

//
//	SetShadowRenderTarget - Used to prepare to render the contents of a shadow depth buffer.
//

static void SetShadowRenderTarget(UINT ShadowResolution)
{
	GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->ShadowDepthRenderTarget->GetSurface());
	GDirect3DDevice->SetDepthStencilSurface(GD3DRenderDevice->ShadowDepthBuffer->GetSurface());
	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,0xffffffff,1.0f,0);
	GD3DRenderInterface.SetViewport(1,1,ShadowResolution,ShadowResolution);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
}

static void ResolveShadowRenderTarget()
{
	GD3DRenderDevice->ShadowDepthRenderTarget->Resolve();
}

//
//	SetShadowTestTarget - Used to prepare to perform depth testing against a shadow depth buffer.
//

static void SetShadowTestTarget(const FSceneContext& Context,UBOOL Inverted)
{
	GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->LightRenderTarget->GetSurface());
	GDirect3DDevice->SetDepthStencilSurface(GD3DRenderDevice->DepthBuffer);
	GD3DRenderInterface.SetViewport(0,0,Context.SizeX,Context.SizeY);
	GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
	GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_DESTCOLOR);
	GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ZERO);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);

	if(Inverted)
	{
		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);
		GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_GREATEREQUAL);
	}
	else
	{
		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
		GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	}
}

static void ResolveShadowTestTarget()
{
	GD3DRenderDevice->LightRenderTarget->Resolve();
}

//
//	FD3DSceneRenderer::RenderShadowDepthBuffer
//

void FD3DSceneRenderer::RenderShadowDepthBuffer(ULightComponent* Light,FD3DShadowInfo* ShadowInfo,DWORD LayerMask)
{
	GD3DStats.ShadowFrustums.Value++;

	// The calls to GetPrimitiveInfo below may change the pointer of the FD3DPrimitiveInfo for the shadow primitive,
	// so reference it by index instead.

	INT	PrimitiveInfoIndex = GetPrimitiveInfoIndex(ShadowInfo->SubjectPrimitive);

	// Render the shadow subject into the shadow depth buffer.

	BEGIND3DCYCLECOUNTER(GD3DStats.ShadowDepthTime);
	SetShadowRenderTarget(ShadowInfo->Resolution);
	{
		FD3DShadowDepthPRI		ShadowDepthPRI(ShadowInfo->SubjectPrimitive,ShadowInfo,0);
		PrimitiveInfoArray(PrimitiveInfoIndex).PVI->RenderShadowDepth(Context,&ShadowDepthPRI);
	}
	for(UPrimitiveComponent* ShadowChild = ShadowInfo->SubjectPrimitive->FirstShadowChild;ShadowChild;ShadowChild = ShadowChild->NextShadowChild)
	{
		FD3DShadowDepthPRI		ShadowDepthPRI(ShadowChild,ShadowInfo,0);
		GetPrimitiveInfo(ShadowChild).PVI->RenderShadowDepth(Context,&ShadowDepthPRI);
	}
	ResolveShadowRenderTarget();
	if(GD3DRenderDevice->DumpRenderTargets)
		GD3DRenderDevice->SaveRenderTarget( GD3DRenderDevice->ShadowDepthRenderTarget->GetSurface(),TEXT("ShadowDepthTexture"));
	ENDD3DCYCLECOUNTER;

	// Project the shadow depth buffer onto the scene.

	BEGIND3DCYCLECOUNTER(GD3DStats.ShadowDepthTestTime);

	if(GD3DRenderDevice->UseHWShadowMaps)
		GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );

	FD3DShadowDepthTestPRI	ShadowDepthTestPRI(NULL,ShadowInfo,Light->IsA(USkyLightComponent::StaticClass()),0);
	if(ShadowInfo->ShadowFrustum.IntersectSphere(CameraPosition,NEAR_CLIPPING_PLANE * appSqrt(3.0f)))
		SetShadowTestTarget(Context,1);
	else
		SetShadowTestTarget(Context,0);
	ShadowDepthTestPRI.DrawFrustumBounds(ShadowInfo->ReceiverShadowMatrix,GEngine->DefaultMaterial->GetMaterialInterface(0),GEngine->DefaultMaterial->GetInstanceInterface());
	ENDD3DCYCLECOUNTER;

	// Don't render static objects shadowing the subject if the subject isn't visible or is occluded.

	if(!PrimitiveInfoArray(PrimitiveInfoIndex).Visible || PrimitiveInfoArray(PrimitiveInfoIndex).IsOccluded() || !(PrimitiveInfoArray(PrimitiveInfoIndex).LayerMask & LayerMask))
	{
		UBOOL	ChildVisible = 0;
		for(UPrimitiveComponent* ShadowChild = ShadowInfo->SubjectPrimitive->FirstShadowChild;ShadowChild;ShadowChild = ShadowChild->NextShadowChild)
		{
			FD3DPrimitiveInfo&	ChildPrimitiveInfo = GetPrimitiveInfo(ShadowChild);
			if(ChildPrimitiveInfo.Visible && !ChildPrimitiveInfo.IsOccluded() && (ChildPrimitiveInfo.LayerMask & LayerMask))
			{
				ChildVisible = 1;
				break;
			}
		}
		if(!ChildVisible)
			return;
	}

	// Render objects potentially shadowing the subject into the shadow depth buffer.

	BEGIND3DCYCLECOUNTER(GD3DStats.ShadowDepthTime);
	SetShadowRenderTarget(ShadowInfo->Resolution / 2);
	FConvexVolume	PreViewFrustum;
	FConvexVolume::GetViewFrustumBounds(ShadowInfo->ShadowMatrix,1,PreViewFrustum);
	for(TDoubleLinkedList<FStaticLightPrimitive*>::TIterator It(Light->FirstStaticPrimitiveLink);It;It.Next())
	{
		UPrimitiveComponent* Primitive = It->GetSource();
		if(PreViewFrustum.IntersectBox(Primitive->Bounds.Origin,Primitive->Bounds.BoxExtent) && Primitive->CastShadow && Primitive->Visible(Context.View))
		{
			FD3DShadowDepthPRI	ShadowDepthPRI(Primitive,ShadowInfo,1);
			GetPrimitiveInfo(Primitive).PVI->RenderShadowDepth(Context,&ShadowDepthPRI);
		}
	}
	ResolveShadowRenderTarget();
	if(GD3DRenderDevice->DumpRenderTargets)
		GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->ShadowDepthRenderTarget->GetSurface(),TEXT("PreShadowDepthTexture"));
	ENDD3DCYCLECOUNTER;

	if(GD3DRenderDevice->UseHWShadowMaps)
		GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );

	// Project the shadow depth buffer onto the subject.

	BEGIND3DCYCLECOUNTER(GD3DStats.ShadowDepthTestTime);
	SetShadowTestTarget(Context,0);
	if(PrimitiveInfoArray(PrimitiveInfoIndex).Visible && !PrimitiveInfoArray(PrimitiveInfoIndex).IsOccluded() && (PrimitiveInfoArray(PrimitiveInfoIndex).LayerMask & LayerMask))
	{
		FD3DShadowDepthTestPRI	ShadowDepthTestPRI(ShadowInfo->SubjectPrimitive,ShadowInfo,Light->IsA(UDirectionalLightComponent::StaticClass()),1);
		PrimitiveInfoArray(PrimitiveInfoIndex).PVI->Render(Context,&ShadowDepthTestPRI);
	}
	for(UPrimitiveComponent* ShadowChild = ShadowInfo->SubjectPrimitive->FirstShadowChild;ShadowChild;ShadowChild = ShadowChild->NextShadowChild)
	{
		FD3DPrimitiveInfo&	ChildPrimitiveInfo = GetPrimitiveInfo(ShadowChild);
		if(ChildPrimitiveInfo.Visible && !ChildPrimitiveInfo.IsOccluded() && (ChildPrimitiveInfo.LayerMask & LayerMask))
		{
			FD3DShadowDepthTestPRI	ShadowDepthTestPRI(ChildPrimitiveInfo.Primitive,ShadowInfo,Light->IsA(UDirectionalLightComponent::StaticClass()),1);
			ChildPrimitiveInfo.PVI->Render(Context,&ShadowDepthTestPRI);
		}
	}
	ENDD3DCYCLECOUNTER;
}

//
//	SetLightingTarget
//

void SetLightingTarget(FD3DSceneRenderer* SceneRenderer,UBOOL Translucent)
{
	GDirect3DDevice->SetRenderTarget(0,SceneRenderer->GetNewLightingBuffer(0,1));
	GDirect3DDevice->SetDepthStencilSurface(GD3DRenderDevice->DepthBuffer);
	GD3DRenderInterface.SetViewport(0,0,SceneRenderer->Context.SizeX,SceneRenderer->Context.SizeY);

	if(GD3DRenderDevice->UseFPBlending && !Translucent)
	{
		GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,1);
		GDirect3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
		GDirect3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);

		GDirect3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,1);
		GDirect3DDevice->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ZERO);
		GDirect3DDevice->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_ONE);
	}
	else
		GDirect3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,0);

	GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
	GDirect3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_EQUAL);
	GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
}

//
//	FD3DSceneRenderer::RenderLight
//

void FD3DSceneRenderer::RenderLight(ULightComponent* Light,FD3DLightInfo* LightInfo,UBOOL Translucent)
{
	DWORD LayerMask = Translucent ? PLM_Translucent : PLM_DepthSorted;

	// Calculate a scissor rectangle for the light's radius.

	UPointLightComponent*	PointLight = Cast<UPointLightComponent>(Light);
	FLOAT					PointLightMinZ = 0.0f,
							PointLightMaxZ = 0.0f;
	if(PointLight)
	{
		FPlane	ScreenPosition = Context.View->Project(PointLight->GetOrigin());
		if(ScreenPosition.W > PointLight->Radius + DELTA)
		{
			FLOAT	SilhouetteRadius = PointLight->Radius * appInvSqrt((ScreenPosition.W - PointLight->Radius) * (ScreenPosition.W + PointLight->Radius)),
					SizeX = Context.View->ProjectionMatrix.M[0][0] * SilhouetteRadius,
					SizeY = Context.View->ProjectionMatrix.M[1][1] * SilhouetteRadius;

			RECT	ScissorRect;
			ScissorRect.left =		Clamp<INT>(appFloor((ScreenPosition.X - SizeX + 1) * (FLOAT)Context.SizeX / +2.0f),Context.X,Context.X + Context.SizeX);
			ScissorRect.top =		Clamp<INT>(appFloor((ScreenPosition.Y + SizeY - 1) * (FLOAT)Context.SizeY / -2.0f),Context.Y,Context.Y + Context.SizeY);
			ScissorRect.right =		Clamp<INT>(appCeil( (ScreenPosition.X + SizeX + 1) * (FLOAT)Context.SizeX / +2.0f),Context.X,Context.X + Context.SizeX);
			ScissorRect.bottom =	Clamp<INT>(appCeil( (ScreenPosition.Y - SizeY - 1) * (FLOAT)Context.SizeY / -2.0f),Context.Y,Context.Y + Context.SizeY);

			// Early out if scissor volume is empty.
			if( ScissorRect.left == ScissorRect.right || ScissorRect.bottom == ScissorRect.top )
				return;

			GDirect3DDevice->SetScissorRect(&ScissorRect);
			GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,1);
		}
	}

	GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->LightRenderTarget->GetSurface());
	GDirect3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,0xffffffff,1.0f,0);

	if(Light->CastShadows)
	{	
		if(GD3DRenderDevice->UseStaticLighting && Light->HasStaticShadowing())
		{
			BEGIND3DCYCLECOUNTER(GD3DStats.ShadowRenderTime);

			for(INT ShadowIndex = 0;ShadowIndex < LightInfo->Shadows.Num();ShadowIndex++)
			{
				FD3DShadowInfo* ShadowInfo = &LightInfo->Shadows(ShadowIndex);
				if(!ShadowInfo->IsOccluded())
				{
					RenderShadowDepthBuffer(Light,ShadowInfo,LayerMask);
				}
			}

			ENDD3DCYCLECOUNTER;
		}

		// Render the shadow volumes to the stencil buffer.
#ifndef XBOX
		//@todo xenon: the below currently locks the GPU with the August XDK. Easiest way to reproduce is firing the gun in WarGame.
		BEGIND3DCYCLECOUNTER(GD3DStats.ShadowRenderTime);

		GDirect3DDevice->SetRenderTarget(0,GD3DRenderDevice->LightRenderTarget->GetSurface());
		GD3DRenderInterface.SetViewport(0,0,Context.SizeX,Context.SizeY);

		GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESS);
		GDirect3DDevice->Clear(0,NULL,D3DCLEAR_STENCIL,D3DCOLOR_RGBA(0,0,0,0),1.0f,0);
		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
		GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE,TRUE);
		GDirect3DDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,TRUE);
		GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE,0);
		GDirect3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,0);
		GDirect3DDevice->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_ALWAYS);
		GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILFUNC,D3DCMP_ALWAYS);

		FD3DShadowVolumeSRI	ShadowSRI(this,Light);

		if(!GD3DRenderDevice->UseStaticLighting)
		{
			for(TDoubleLinkedList<FStaticLightPrimitive*>::TIterator It(Light->FirstStaticPrimitiveLink);It;It.Next())
			{
				UPrimitiveComponent*	Primitive = It->GetSource();
				if(Primitive->CastShadow && Primitive->Visible(Context.View))
					GetPrimitiveInfo(Primitive).PVI->RenderShadowVolume(Context,&ShadowSRI,Light);
			}
		}

		for(TDoubleLinkedList<UPrimitiveComponent*>::TIterator It(Light->FirstDynamicPrimitiveLink);It;It.Next())
		{
			if(It->CastShadow && It->Visible(Context.View) && !(GD3DRenderDevice->UseStaticLighting && Light->HasStaticShadowing() && !It->HasStaticShadowing()))
			{
				GetPrimitiveInfo(*It).PVI->RenderShadowVolume(Context,&ShadowSRI,Light);
			}
		}

		GDirect3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
		GDirect3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		GDirect3DDevice->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
		GDirect3DDevice->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);
		GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILPASS,D3DSTENCILOP_KEEP);
		GDirect3DDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL,D3DSTENCILOP_KEEP);
		GDirect3DDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,FALSE);

		GDirect3DDevice->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL);
		GDirect3DDevice->SetRenderState(D3DRS_STENCILREF,0);

		ENDD3DCYCLECOUNTER;
#endif
	}

	if(Light->Function && Light->Function->SourceMaterial)
	{
		SetShadowTestTarget(Context,1);

		FD3DLightFunctionPRI	LightFunctionPRI(NULL,Light);
		LightFunctionPRI.DrawBox(FMatrix::Identity,FVector(WORLD_MAX,WORLD_MAX,WORLD_MAX),GEngine->DefaultMaterial->GetMaterialInterface(0),GEngine->DefaultMaterial->GetInstanceInterface());
	}

	ResolveShadowTestTarget();

	if(GD3DRenderDevice->DumpRenderTargets)
		GD3DRenderDevice->SaveRenderTarget(GD3DRenderDevice->LightRenderTarget->GetSurface(),TEXT("LightBuffer"));

	BEGIND3DCYCLECOUNTER(GD3DStats.LightRenderTime);

	SetLightingTarget(this,Translucent);

	if(Light->CastShadows && GD3DRenderDevice->UseFPBlending)
		GDirect3DDevice->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_INCR);

	if(GD3DRenderDevice->UseStaticLighting)
	{
		// Render the statically lit primitives.

		for(TDoubleLinkedList<FStaticLightPrimitive*>::TIterator It(Light->FirstStaticPrimitiveLink);It;It.Next())
		{
			UPrimitiveComponent*	Primitive = It->GetSource();
			FD3DPrimitiveInfo*		PrimitiveInfo = GetVisiblePrimitiveInfo(Primitive);
			if(PrimitiveInfo && (PrimitiveInfo->LayerMask & LayerMask) && !PrimitiveInfo->IsOccluded())
			{
				FD3DStaticLightPRI	StaticLightPRI(PrimitiveInfo->Primitive,Translucent == 0 ? SP_OpaqueLighting : SP_TranslucentLighting,Light);
				It->Render(Context,PrimitiveInfo->PVI,&StaticLightPRI);
			}
		}
	}
	else
	{
		// Render the statically lit primitives using dynamic lighting.

		for(TDoubleLinkedList<FStaticLightPrimitive*>::TIterator It(Light->FirstStaticPrimitiveLink);It;It.Next())
		{
			UPrimitiveComponent*	Primitive = It->GetSource();
			FD3DPrimitiveInfo*		PrimitiveInfo = GetVisiblePrimitiveInfo(Primitive);
			if(PrimitiveInfo && (PrimitiveInfo->LayerMask & LayerMask) && !PrimitiveInfo->IsOccluded())
			{
				FD3DMeshLightPRI	LightPRI(Primitive,Translucent == 0 ? SP_OpaqueLighting : SP_TranslucentLighting,Light,SLT_Dynamic);
				PrimitiveInfo->PVI->Render(Context,&LightPRI);
			}
		}
	}

	// Render the dynamically lit primitives.

	for(TDoubleLinkedList<UPrimitiveComponent*>::TIterator It(Light->FirstDynamicPrimitiveLink);It;It.Next())
	{
		FD3DPrimitiveInfo*		PrimitiveInfo = GetVisiblePrimitiveInfo(*It);
		if(PrimitiveInfo && (PrimitiveInfo->LayerMask & LayerMask) && !PrimitiveInfo->IsOccluded())
		{
			FD3DMeshLightPRI	LightPRI(*It,Translucent == 0 ? SP_OpaqueLighting : SP_TranslucentLighting,Light,SLT_Dynamic);
			PrimitiveInfo->PVI->Render(Context,&LightPRI);
		}
	}

	// Restore state.

	GDirect3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,0);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE);
	GDirect3DDevice->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
	GDirect3DDevice->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);

	ENDD3DCYCLECOUNTER;

}