/*=============================================================================
	ImageReflectionRendering.cpp: Implementation for rendering image based reflections.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "EngineMeshClasses.h"
#include "ImageUtils.h"
#include "SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "BokehDOF.h"

UBOOL GRenderDynamicReflectionShadowing = TRUE;
UBOOL GBlurDynamicReflectionShadowing = TRUE;

/** Note - image reflection shader recompile is required to propagate changes to this. */
UBOOL GDownsampleStaticReflectionShadowing = TRUE;

IMPLEMENT_CLASS(AImageReflection);
IMPLEMENT_CLASS(AImageReflectionSceneCapture);
IMPLEMENT_CLASS(UDEPRECATED_ImageReflectionComponent);
IMPLEMENT_CLASS(UImageBasedReflectionComponent);
IMPLEMENT_CLASS(AImageReflectionShadowPlane);
IMPLEMENT_CLASS(UImageReflectionShadowPlaneComponent);


void AImageReflection::PostLoad()
{
	Super::PostLoad();
	if (ReflectionComponent_DEPRECATED)
	{
		ImageReflectionComponent->ReflectionTexture = ReflectionComponent_DEPRECATED->ReflectionTexture;
	}
}

void AImageReflectionSceneCapture::PostDuplicate()
{
	Super::PostDuplicate();
	// Don't copy the generated texture reference
	ImageReflectionComponent->ReflectionTexture = NULL;
}

// World size of the EditorMeshes.TexPropPlane that is used for previewing
const FLOAT PlaneSize = 321.0f;

/** Renders the scene to a texture for a AImageReflectionSceneCapture. */
void GenerateImageReflectionTexture(AImageReflectionSceneCapture* Reflection, UTextureRenderTarget2D* TempRenderTarget)
{
	FSceneViewFamilyContext ViewFamily(
		TempRenderTarget->GameThread_GetRenderTargetResource(),
		GWorld->Scene,
		// Dynamic shadows currently don't work in ortho projections
		(SHOW_ViewMode_Lit|(SHOW_DefaultGame & ~SHOW_ViewMode_Mask)) & ~(SHOW_PostProcess | SHOW_DynamicShadows | SHOW_LOD | SHOW_Fog | SHOW_Selection),
		GCurrentTime - GStartTime,
		GDeltaTime,
		GCurrentTime - GStartTime);

	ViewFamily.bClearScene = TRUE;

	FVector4 OverrideLODViewOrigin(0, 0, 0, 1.0f);

	// Transform positions into the local space of the mesh
	FMatrix ViewMatrix = Reflection->WorldToLocal();

	// Orient the view to look straight at the preview plane's front face
	ViewMatrix = ViewMatrix * FMatrix(
		FPlane(0,	0,	-1,	0),
		FPlane(-1,	0,	0,	0),
		FPlane(0,	1,	0,	0),
		FPlane(0,	0,	0,	1));

	// Remove the scaling along the depth of the image reflection that is included in LocalToWorld so that we can specify a world space depth range
	const FLOAT EffectiveZRange = Reflection->DepthRange / (Reflection->DrawScale * Reflection->DrawScale3D.X);

	const FMatrix ProjectionMatrix = FOrthoMatrix(
		PlaneSize / 2.0f,
		PlaneSize / 2.0f,
		0.5f / EffectiveZRange,
		EffectiveZRange
		);

	TSet<UPrimitiveComponent*> HiddenPrimitives;
	FSceneView* NewView = new FSceneView(
		&ViewFamily,
		NULL,
		-1,
		NULL,
		NULL,
		NULL,
		GEngine->GetWorldPostProcessChain(),
		NULL,
		NULL,
		0,
		0,
		(FLOAT)TempRenderTarget->SizeX,
		(FLOAT)TempRenderTarget->SizeY,
		ViewMatrix,
		ProjectionMatrix,
		FLinearColor::Black,
		FLinearColor(0,0,0,0),
		FLinearColor::White,
		HiddenPrimitives,
		FRenderingPerformanceOverrides(E_ForceInit),
		1.0f,
		TRUE,					// Treat this as a fresh frame, ignore any potential historical data
		FTemporalAAParameters()
#if !CONSOLE
		,1
		,OverrideLODViewOrigin // we need to override this since we are using an ortho view, the normal checks in SceneRendering will fail
#endif
		);

	// Disable specular, otherwise we would be capturing image based reflections from the previous lighting rebuild
	NewView->SpecularOverrideParameter = FVector4(0,0,0,0);

	ViewFamily.Views.Empty();
	ViewFamily.Views.AddItem(NewView);

	// Render the scene
	FCanvas Canvas(TempRenderTarget->GameThread_GetRenderTargetResource(), NULL);
	BeginRenderingViewFamily(&Canvas, &ViewFamily);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FResolveCommand,
		FTextureRenderTargetResource*, RTResource, TempRenderTarget->GameThread_GetRenderTargetResource(),
	{
		// copy the results of the scene rendering from the target surface to its texture
		RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());
	});

	// Wait until rendering is complete before accessing the render target
	FlushRenderingCommands();

	FRenderTarget* RenderTarget = TempRenderTarget->GameThread_GetRenderTargetResource();

	TArray<FFloat16Color> OutputBuffer;
	// Read the data back to the CPU
	verify(RenderTarget->ReadFloat16Pixels(OutputBuffer));

	// Create a new texture
	UTexture2D* FinalTexture = CastChecked<UTexture2D>(UObject::StaticConstructObject(UTexture2D::StaticClass(), Reflection->GetOutermost(), Reflection->GetFName(), 0));
	FinalTexture->Init(TempRenderTarget->SizeX, TempRenderTarget->SizeY, PF_A8R8G8B8);

	// Create base mip for the texture we created.
	FColor* MipData = (FColor*)FinalTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
	for (INT y = 0; y < TempRenderTarget->SizeY; y++)
	{
		FColor* DestPtr = &MipData[(TempRenderTarget->SizeY - 1 - y) * TempRenderTarget->SizeX];
		FFloat16Color* SrcPtr = &OutputBuffer((TempRenderTarget->SizeY - 1 - y) * TempRenderTarget->SizeX);
		for (INT x = 0; x < TempRenderTarget->SizeX; x++)
		{
			FLinearColor CurrentColor = FLinearColor(*SrcPtr);
			// Scale down by ColorRange before gamma correction and quantizing so that we can store colors outside the [0, 1] range (at the cost of decreased color precision)
			CurrentColor.R /= Reflection->ColorRange;
			CurrentColor.G /= Reflection->ColorRange;
			CurrentColor.B /= Reflection->ColorRange;
			//@todo - mask based on depth
			CurrentColor.A = CurrentColor.GetLuminance() > .001f ? 1.0f : 0.0f;
			*DestPtr = CurrentColor.ToFColor(TRUE);
			DestPtr++;
			SrcPtr++;
		}
	}
	FinalTexture->Mips(0).Data.Unlock();

	// Set compression options.
	FinalTexture->SRGB = TRUE;
	FinalTexture->CompressionSettings = TC_Default;
	FinalTexture->DeferCompression= FALSE;
	FinalTexture->LODGroup = TEXTUREGROUP_ImageBasedReflection;

	FinalTexture->PostEditChange();

	// Update the image reflection with the newly generated texture
	Reflection->ImageReflectionComponent->ReflectionTexture = FinalTexture;

	Reflection->MarkPackageDirty();
	Reflection->ForceUpdateComponents(FALSE, FALSE);
}

/** A scene proxy used to preview the image reflection actor.  The actual reflection is not rendered through this proxy. */
class FImageReflectionPreviewSceneProxy : public FStaticMeshSceneProxy
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FImageReflectionPreviewSceneProxy(const UImageBasedReflectionComponent* Component) :
		FStaticMeshSceneProxy(Component),
		ReflectionTexture(Component->ReflectionTexture),
		ReflectionColor(Component->ReflectionColor * Component->ReflectionColor.A)
	{
		check(ReflectionTexture);
		AImageReflectionSceneCapture* CaptureOwner = Cast<AImageReflectionSceneCapture>(Component->GetOwner());
		bDrawDepthRange = CaptureOwner != NULL;
		if (CaptureOwner)
		{
			// Remove the scaling inherent in the LocalToWorld transform so artists can provide DepthRange in world space
			DepthRange = CaptureOwner->DepthRange / (CaptureOwner->DrawScale * CaptureOwner->DrawScale3D.X);
			ReflectionColor *= CaptureOwner->ColorRange;
		}
		else
		{
			DepthRange = 0;
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Relevance = FStaticMeshSceneProxy::GetViewRelevance(View);
		if (bDrawDepthRange)
		{
			// Force DrawDynamicElements to always be called so we can draw the depth range
			Relevance.bDynamicRelevance = TRUE;
			Relevance.bStaticRelevance = FALSE;
		}
		return Relevance;
	}


	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		FStaticMeshSceneProxy::DrawDynamicElements(PDI, View, DPGIndex, Flags);

		// If selected, draw a wireframe box visualizing the depth range
		if (bDrawDepthRange 
			&& (View->Family->ShowFlags & SHOW_StaticMeshes)
			&& GetDepthPriorityGroup(View) == DPGIndex
			&& AllowDebugViewmodes()
			&& IsSelected())
		{
			const FVector Extent = FVector(DepthRange, PlaneSize / 2.0f, PlaneSize / 2.0f);
			const FVector Corner0 = LocalToWorld.TransformFVector(FVector(-Extent.X, -Extent.Y, -Extent.Z));
			const FVector Corner1 = LocalToWorld.TransformFVector(FVector(Extent.X, -Extent.Y, -Extent.Z));
			const FVector Corner2 = LocalToWorld.TransformFVector(FVector(-Extent.X, Extent.Y, -Extent.Z));
			const FVector Corner3 = LocalToWorld.TransformFVector(FVector(Extent.X, Extent.Y, -Extent.Z));
			const FVector Corner4 = LocalToWorld.TransformFVector(FVector(-Extent.X, -Extent.Y, Extent.Z));
			const FVector Corner5 = LocalToWorld.TransformFVector(FVector(Extent.X, -Extent.Y, Extent.Z));
			const FVector Corner6 = LocalToWorld.TransformFVector(FVector(-Extent.X, Extent.Y, Extent.Z));
			const FVector Corner7 = LocalToWorld.TransformFVector(FVector(Extent.X, Extent.Y, Extent.Z));

			PDI->DrawLine(Corner0, Corner1, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner1, Corner3, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner3, Corner2, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner2, Corner0, WireframeColor, DPGIndex);

			PDI->DrawLine(Corner4, Corner5, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner5, Corner7, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner7, Corner6, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner6, Corner4, WireframeColor, DPGIndex);

			PDI->DrawLine(Corner0, Corner4, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner1, Corner5, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner2, Corner6, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner3, Corner7, WireframeColor, DPGIndex);
		}
	}


	virtual UBOOL GetMeshElement(INT LODIndex,INT ElementIndex,INT FragmentIndex,BYTE InDepthPriorityGroup,const FMatrix& WorldToLocal, FMeshBatch& OutMeshElement, const UBOOL bUseSelectedMaterial, const UBOOL bUseHoveredMaterial) const
	{
		const UBOOL bShouldRender = FStaticMeshSceneProxy::GetMeshElement(LODIndex, ElementIndex, FragmentIndex, InDepthPriorityGroup, WorldToLocal, OutMeshElement, bUseSelectedMaterial, bUseHoveredMaterial);
		if (ReflectionTexture->Resource)
		{
			// Wrap the material with a FTexturedMaterialRenderProxy which has hooks to override certain parameters with our per-instance data
			OverrideMaterialProxy = FTexturedMaterialRenderProxy(OutMeshElement.MaterialRenderProxy, ReflectionTexture->Resource, ReflectionColor);
		}
		OutMeshElement.MaterialRenderProxy = &OverrideMaterialProxy;
		return bShouldRender && ReflectionTexture->Resource;
	}

protected:

	UBOOL bDrawDepthRange;
	FLOAT DepthRange;
	UTexture2D* ReflectionTexture;
	FLinearColor ReflectionColor;
	mutable FTexturedMaterialRenderProxy OverrideMaterialProxy;
};

void UImageBasedReflectionComponent::Attach()
{
	Super::Attach();

	// Add the image reflection to the scene if it is valid
	if (ReflectionTexture && ReflectionTexture->LODGroup == TEXTUREGROUP_ImageBasedReflection)
	{
		FLOAT ColorRange = 1.0f;
		AImageReflectionSceneCapture* CaptureOwner = Cast<AImageReflectionSceneCapture>(GetOwner());
		if (CaptureOwner)
		{
			ColorRange = CaptureOwner->ColorRange;
		}
		Scene->AddImageReflection(this, ReflectionTexture, 1.0f, ReflectionColor * ReflectionColor.A * ColorRange * Scale, bTwoSided, bEnabled);
	}
}

void UImageBasedReflectionComponent::UpdateTransform()
{
	Super::UpdateTransform();

	if (ReflectionTexture && ReflectionTexture->LODGroup == TEXTUREGROUP_ImageBasedReflection)
	{
		FLOAT ColorRange = 1.0f;
		AImageReflectionSceneCapture* CaptureOwner = Cast<AImageReflectionSceneCapture>(GetOwner());
		if (CaptureOwner)
		{
			ColorRange = CaptureOwner->ColorRange;
		}
		Scene->UpdateImageReflection(this, ReflectionTexture, 1.0f, ReflectionColor * ReflectionColor.A * ColorRange * Scale, bTwoSided, bEnabled);
	}
}

void UImageBasedReflectionComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach(bWillReattach);

	Scene->RemoveImageReflection(this);
}

void UImageBasedReflectionComponent::SetEnabled(UBOOL bSetEnabled)
{
	if(bEnabled != bSetEnabled)
	{
		// Update bEnabled, and begin a deferred component reattach.
		bEnabled = bSetEnabled;
		BeginDeferredUpdateTransform();
	}
}

void UImageBasedReflectionComponent::UpdateImageReflectionParameters()
{
	BeginDeferredUpdateTransform();
}

FPrimitiveSceneProxy* UImageBasedReflectionComponent::CreateSceneProxy()
{
	return new FImageReflectionPreviewSceneProxy(this);
}

void UImageBasedReflectionComponent::PostLoad()
{
	Super::PostLoad();
}

void UImageBasedReflectionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName() == TEXT("ReflectionTexture")
			&& ReflectionTexture)
		{
			for (TObjectIterator<UImageBasedReflectionComponent> ReflectionIt; ReflectionIt; ++ReflectionIt)
			{
				UImageBasedReflectionComponent* Reflection = *ReflectionIt;
				const UBOOL bReflectionIsInWorld = Reflection->GetOwner() && GWorld->ContainsActor(Reflection->GetOwner());
				if (bReflectionIsInWorld
					&& Reflection->ReflectionTexture
					&& Reflection->bEnabled
					&& (Reflection->ReflectionTexture->SizeX != ReflectionTexture->SizeX
					|| Reflection->ReflectionTexture->SizeY != ReflectionTexture->SizeY
					|| Reflection->ReflectionTexture->Mips.Num() != ReflectionTexture->Mips.Num()
					|| Reflection->ReflectionTexture->LODGroup != ReflectionTexture->LODGroup
					|| Reflection->ReflectionTexture->Format != ReflectionTexture->Format
					|| Reflection->ReflectionTexture->SRGB != ReflectionTexture->SRGB))
				{
					//@todo - render an indicator that the texture is invalid, currently it just disappears
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_ReflectionTextureDoesntMatch"), *Reflection->GetOwner()->GetName()));
					break;
				}
			}

			if (ReflectionTexture->LODGroup != TEXTUREGROUP_ImageBasedReflection)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Error_ReflectionTextureInvalid"));
			}
		}
	}
}

void UImageReflectionShadowPlaneComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	//get the plane height
	FLOAT W = ParentToWorld.GetOrigin().Z;
	//get the plane normal
	FVector4 T = ParentToWorld.TransformNormal(FVector(0.0f,0.0f,1.0f));
	FVector PlaneNormal = FVector(T);
	PlaneNormal.Normalize();
	ReflectionPlane = FPlane(PlaneNormal, W);
}

void UImageReflectionShadowPlaneComponent::Attach()
{
	Super::Attach();

	if (bEnabled)
	{
		Scene->AddImageReflectionShadowPlane(this, ReflectionPlane);
	}
}

void UImageReflectionShadowPlaneComponent::UpdateTransform()
{
	Super::UpdateTransform();

	Scene->RemoveImageReflectionShadowPlane(this);

	if (bEnabled)
	{
		Scene->AddImageReflectionShadowPlane(this, ReflectionPlane);
	}
}

void UImageReflectionShadowPlaneComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach(bWillReattach);

	Scene->RemoveImageReflectionShadowPlane(this);
}

void UImageReflectionShadowPlaneComponent::SetEnabled(UBOOL bSetEnabled)
{
	if(bEnabled != bSetEnabled)
	{
		// Update bEnabled, and begin a deferred component reattach.
		bEnabled = bSetEnabled;
		BeginDeferredReattach();
	}
}

/*-----------------------------------------------------------------------------
	FImageReflectionSceneInfo
-----------------------------------------------------------------------------*/
FImageReflectionSceneInfo::FImageReflectionSceneInfo(const UActorComponent* InComponent, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bInEnabled) : 
	ReflectionTexture(InReflectionTexture),
	ReflectionColor(InReflectionColor),
	bTwoSided(bInTwoSided),
	bEnabled(bInEnabled)
{
	const ULightComponent* LightComponent = ConstCast<ULightComponent>(InComponent);
	bLightReflection = LightComponent != NULL;

	check(InComponent && (InReflectionTexture || bLightReflection));
	check(InComponent->GetOwner() || LightComponent);

	if (LightComponent)
	{
		ReflectionPlane = FPlane(0, 0, 0, 0);
		ReflectionOrigin = LightComponent->GetPosition();
		ReflectionXAxisAndYScale = FVector4(0, 1, 0, 1);
	}
	else
	{
		const FVector DrawScale3D = InComponent->GetOwner()->DrawScale * InComponent->GetOwner()->DrawScale3D * ReflectionScale;
		const FMatrix LocalToWorld = InComponent->GetOwner()->LocalToWorld();
		FVector ForwardVector(1.0f,0.0f,0.0f);
		FVector RightVector(0.0f,-1.0f,0.0f);

		const FVector4 PlaneNormal = LocalToWorld.TransformNormal(ForwardVector);
		const FVector Origin = LocalToWorld.GetOrigin();

		// Normalize the plane
		ReflectionPlane = FPlane(Origin, FVector(PlaneNormal).SafeNormal());
		ReflectionOrigin = Origin;
		const FVector ReflectionXAxis = LocalToWorld.TransformNormal(RightVector);
		// Include the owner's draw scale in the axes
		ReflectionXAxisAndYScale = ReflectionXAxis.SafeNormal() / (PlaneSize * DrawScale3D.Y);
		ReflectionXAxisAndYScale.W = DrawScale3D.Y / DrawScale3D.Z;
	}
}

/*-----------------------------------------------------------------------------
	FReflectionMaskVertexShader
-----------------------------------------------------------------------------*/

/** Vertex shader used to render meshes into a planar reflection for shadowing. */
class FReflectionMaskVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FReflectionMaskVertexShader,MeshMaterial);
public:
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile for the default material, since we're only handling opaque materials
		return Material->IsSpecialEngineMaterial() && Platform == SP_PCD3D_SM5;
	}

	FReflectionMaskVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FShader(Initializer),
		VertexFactoryParameters(Initializer.VertexFactoryType, Initializer.ParameterMap)
	{
		MirrorPlaneParameter.Bind(Initializer.ParameterMap, TEXT("MirrorPlane"), TRUE);
	}

	FReflectionMaskVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MirrorPlaneParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory, const FMaterialRenderProxy* MaterialRenderProxy, const FViewInfo& View, const FPlane& InMirrorPlane)
	{
		VertexFactoryParameters.Set(this, VertexFactory, View);

		SetShaderValue(GetVertexShader(), MirrorPlaneParameter, InMirrorPlane);
	}

	void SetMesh(const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, BatchElementIndex, View);
	}

private:

	FVertexFactoryVSParameterRef VertexFactoryParameters;
	FShaderParameter MirrorPlaneParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FReflectionMaskVertexShader,TEXT("ImageReflectionMeshShader"),TEXT("ReflectionMaskVertexMain"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
	FReflectionMaskPixelShader
-----------------------------------------------------------------------------*/

/** Pixel shader used to render meshes into a planar reflection for shadowing. */
class FReflectionMaskPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FReflectionMaskPixelShader,MeshMaterial);
public:
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile for the default material, since we're only handling opaque materials
		return Material->IsSpecialEngineMaterial() && Platform == SP_PCD3D_SM5;
	}

	FReflectionMaskPixelShader() {}

	FReflectionMaskPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		InvResolutionXParameter.Bind(Initializer.ParameterMap, TEXT("InvResolutionX"), TRUE);
		MirrorPlaneParameter.Bind(Initializer.ParameterMap, TEXT("MirrorPlane"), TRUE);
		ReflectedCameraPositionParameter.Bind(Initializer.ParameterMap, TEXT("ReflectedCameraPosition"), TRUE);
	}

	void SetParameters(const FViewInfo& View, const FPlane& InMirrorPlane, const FVector& InReflectedCameraPosition)
	{
		DeferredParameters.Set(View, this);
		SetShaderValue(GetPixelShader(), InvResolutionXParameter, 2.0f / View.RenderTargetSizeX);
		SetShaderValue(GetPixelShader(), MirrorPlaneParameter, InMirrorPlane);
		SetShaderValue(GetPixelShader(), ReflectedCameraPositionParameter, InReflectedCameraPosition);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << InvResolutionXParameter;
		Ar << MirrorPlaneParameter;
		Ar << ReflectedCameraPositionParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter InvResolutionXParameter;
	FShaderParameter MirrorPlaneParameter;
	FShaderParameter ReflectedCameraPositionParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FReflectionMaskPixelShader,TEXT("ImageReflectionMeshShader"),TEXT("ReflectionMaskPixelMain"),SF_Pixel,0,0);

/** Drawing policy for rendering dynamic meshes into a planar reflection, used for shadowing image reflections. */
class FReflectionMaskDrawingPolicy : public FMeshDrawingPolicy
{
public:

	FReflectionMaskDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FPlane& MirrorPlane,
		const FVector& ReflectedCameraPosition,
		const FPlane& TranslatedMirrorPlane
		);

	// FMeshDrawingPolicy interface.
	UBOOL Matches(const FReflectionMaskDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) 
			&& VertexShader == Other.VertexShader 
			&& PixelShader == Other.PixelShader;
	}

	void DrawShared(const FViewInfo& View, FBoundShaderStateRHIRef ShaderState) const;

	void SetMeshRenderState(
		const FViewInfo& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	friend INT Compare(const FReflectionMaskDrawingPolicy& A,const FReflectionMaskDrawingPolicy& B);

	/** Indicates whether the primitive has moved since last frame. */
	static UBOOL HasMoved(const FPrimitiveSceneInfo* PrimitiveSceneInfo);

private:

	FReflectionMaskVertexShader* VertexShader;
	FReflectionMaskPixelShader* PixelShader;
	FPlane MirrorPlane;
	FVector ReflectedCameraPosition;
	FPlane TranslatedMirrorPlane;
};

/** Drawing policy factory for rendering dynamic meshes into a planar reflection, used for shadowing image reflections. */
class FReflectionMaskDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };

	struct ContextType
	{
		ContextType(const FPlane& InMirrorPlane, const FVector& InReflectedCameraPosition, const FPlane& InTranslatedMirrorPlane) :
			MirrorPlane(InMirrorPlane),
			ReflectedCameraPosition(InReflectedCameraPosition),
			TranslatedMirrorPlane(InTranslatedMirrorPlane)
		{}

		const FPlane& MirrorPlane;
		const FVector& ReflectedCameraPosition;
		const FPlane& TranslatedMirrorPlane;
	};

	static UBOOL DrawDynamicMesh(	
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);

	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy);
};

FReflectionMaskDrawingPolicy::FReflectionMaskDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FPlane& InMirrorPlane,
	const FVector& InReflectedCameraPosition,
	const FPlane& InTranslatedMirrorPlane
	) :	
	FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, *InMaterialRenderProxy->GetMaterial(), FALSE, TRUE),
	MirrorPlane(InMirrorPlane),
	ReflectedCameraPosition(InReflectedCameraPosition),
	TranslatedMirrorPlane(InTranslatedMirrorPlane)
{
	const FMaterial* MaterialResource = InMaterialRenderProxy->GetMaterial();
	VertexShader = MaterialResource->GetShader<FReflectionMaskVertexShader>(InVertexFactory->GetType());
	PixelShader = MaterialResource->GetShader<FReflectionMaskPixelShader>(InVertexFactory->GetType());
}

void FReflectionMaskDrawingPolicy::DrawShared(const FViewInfo& View, FBoundShaderStateRHIRef ShaderState) const
{
	RHISetBoundShaderState(ShaderState);

	VertexShader->SetParameters(VertexFactory, MaterialRenderProxy, View, TranslatedMirrorPlane);
	PixelShader->SetParameters(View, MirrorPlane, ReflectedCameraPosition);

	// Set the shared mesh resources.
	FMeshDrawingPolicy::DrawShared(&View);
}

void FReflectionMaskDrawingPolicy::SetMeshRenderState(
	const FViewInfo& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	VertexShader->SetMesh(Mesh, BatchElementIndex, View);

	FMeshDrawingPolicy::SetMeshRenderState(View, PrimitiveSceneInfo, Mesh, BatchElementIndex, bBackFace, ElementData);
}

/** 
 * Create bound shader state using the vertex decl from the mesh draw policy
 * as well as the shaders needed to draw the mesh
 * @param DynamicStride - optional stride for dynamic vertex data
 * @return new bound shader state object
 */
FBoundShaderStateRHIRef FReflectionMaskDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
	if (DynamicStride)
	{
		StreamStrides[0] = DynamicStride;
	}

	return RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShader->GetPixelShader(), EGST_None);	
}

INT Compare(const FReflectionMaskDrawingPolicy& A, const FReflectionMaskDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	return 0;
}

UBOOL FReflectionMaskDrawingPolicyFactory::DrawDynamicMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	EBlendMode BlendMode = Material->GetBlendMode();
	// Only add primitives with opaque or masked materials, unless they have a decal material.
	if (Mesh.CastShadow
		&& (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked) 
		&& !Material->IsDecalMaterial())
	{
		// Override with the default material.
		// Masked and two-sided materials are not handled separately, but the artifacts are normally not severe.
		FReflectionMaskDrawingPolicy DrawingPolicy(Mesh.VertexFactory, GEngine->DefaultMaterial->GetRenderProxy(FALSE), DrawingContext.MirrorPlane, DrawingContext.ReflectedCameraPosition, DrawingContext.TranslatedMirrorPlane);
		DrawingPolicy.DrawShared(View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
		for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(View, PrimitiveSceneInfo, Mesh, BatchElementIndex, bBackFace, FMeshDrawingPolicy::ElementDataType());
			DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
		}
		return TRUE;
	}
	return FALSE;
}

UBOOL FReflectionMaskDrawingPolicyFactory::IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
{
	// Ignore primitives with translucent materials
	return IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode());
}

/** Geometry shader that generates quads which blur the reflection mask by scattering the pixel's mask to its neighbors. */
class FMaskBlurGeometryShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMaskBlurGeometryShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	/** Default constructor. */
	FMaskBlurGeometryShader() {}

	/** Initialization constructor. */
	FMaskBlurGeometryShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ReflectionMaskTextureParameter.Bind(Initializer.ParameterMap, TEXT("ReflectionMaskTexture"), TRUE);
		SceneCoordinate1ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate1ScaleBias"),TRUE);
		SceneCoordinate2ScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinate2ScaleBias"),TRUE);
		ArraySettingsParameter.Bind(Initializer.ParameterMap,TEXT("ArraySettings"),TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
#if PLATFORM_SUPPORTS_D3D10_PLUS
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		const UINT HalfSizeX = BufferSizeX / 2;
		const UINT HalfSizeY = BufferSizeY / 2;

		const INT HalfTargetSizeX = View.RenderTargetSizeX / 2;
		const INT HalfTargetSizeY = View.RenderTargetSizeY / 2;

		const FGeometryShaderRHIRef& ShaderRHI = GetGeometryShader();

		SetTextureParameter(
			ShaderRHI,
			ReflectionMaskTextureParameter,
			TStaticSamplerState<SF_Point,AM_Border,AM_Border,AM_Border>::GetRHI(),
			GSceneRenderTargets.GetTranslucencyBufferTexture());

		SetShaderValue(
			ShaderRHI,
			SceneCoordinate1ScaleBiasParameter,
			FVector4(
			2.0f / HalfTargetSizeX,
			-2.0f / HalfTargetSizeY,
			-GPixelCenterOffset / HalfSizeX - 1.0f,
			GPixelCenterOffset / HalfSizeY + 1.0f));

		UINT NumPrimitivesInX = HalfTargetSizeX;

		SetShaderValue(
			ShaderRHI,
			SceneCoordinate2ScaleBiasParameter,
			FVector4(
			1.0f / HalfSizeX,
			1.0f / HalfSizeY,
			0.5f / HalfSizeX,
			0.5f / HalfSizeY));

		SetShaderValue(
			ShaderRHI,
			ArraySettingsParameter,
			FVector4(
			NumPrimitivesInX,
			0.0f,
			0.0f,
			0.0f));
#endif
	}

	// FShader interface.
	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ReflectionMaskTextureParameter << SceneCoordinate1ScaleBiasParameter << SceneCoordinate2ScaleBiasParameter << ArraySettingsParameter;
		return bShaderHasOutdatedParameters;
	}

private: 

	FShaderResourceParameter ReflectionMaskTextureParameter;
	FShaderParameter SceneCoordinate1ScaleBiasParameter;
	FShaderParameter SceneCoordinate2ScaleBiasParameter;
	FShaderParameter ArraySettingsParameter;
};

IMPLEMENT_SHADER_TYPE(,FMaskBlurGeometryShader,TEXT("ImageReflectionShader"),TEXT("MaskBlurGeometryShader"),SF_Geometry,0,0);

/** Pixel shader that blurs the reflection mask by scattering the pixel's mask to its neighbors. */
class FMaskBlurPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMaskBlurPixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	FMaskBlurPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	FMaskBlurPixelShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FMaskBlurPixelShader,TEXT("ImageReflectionShader"),TEXT("MaskBlurPixelShader"),SF_Pixel,0,0);

FGlobalBoundShaderState MaskBlurBoundShaderState;

/** Shader parameters needed to render image based reflections. */
class FImageReflectionShaderParameters
{
public:

	const static UINT MaxNumImageReflections;
	const static UINT MaxNumLightReflections;

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FImageReflectionShaderParameters& P);

	/** Sets the shader parameters needed for image based reflections. */
	void Set(const FPixelShaderRHIParamRef PixelShaderRHI, const FSceneView& View, FScene& Scene, const FReflectionPlanarShadowInfo* ShadowInfo) const;

private:

	/** Parameters used for rendering image based reflections. */
	FShaderParameter NumActiveReflectionsParameter;
	FShaderParameter ImageReflectionPlaneParameter;
	FShaderParameter ImageReflectionOriginParameter;
	FShaderParameter ImageReflectionXAxisParameter;
	FShaderParameter ImageReflectionColorParameter;
	FShaderParameter LightReflectionOriginParameter;
	FShaderParameter LightReflectionColorParameter;
	FShaderResourceParameter ImageReflectionTextureParameter;
	FShaderResourceParameter ImageReflectionSamplerParameter;
	FShaderResourceParameter StaticShadowingTextureParameter0;
	FShaderResourceParameter StaticShadowingTextureParameter1;
	FShaderResourceParameter ReflectionMaskTextureParameter;
	FShaderParameter MirrorPlaneParameter;
	FShaderParameter VolumeMinParameter;
	FShaderParameter VolumeSizeParameter;
	FShaderResourceParameter DistanceFieldTextureParameter;
	FShaderResourceParameter DistanceFieldSamplerParameter;
	FShaderResourceParameter EnvironmentTextureParameter;
	FShaderResourceParameter EnvironmentSamplerParameter;
	FShaderParameter EnvironmentColorParameter;
	FShaderParameter ViewProjectionMatrixParameter;
	FShaderParameter TexelSizesParameter;
	FShaderParameter PixelSizesParameter;
};

/** Vertex shader used to render deferred image reflections. */
class FImageReflectionVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FImageReflectionVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("NUM_IMAGE_REFLECTIONS"),*appItoa(FImageReflectionShaderParameters::MaxNumImageReflections));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHT_REFLECTIONS"),*appItoa(FImageReflectionShaderParameters::MaxNumLightReflections));
	}

	FImageReflectionVertexShader()	{}
	FImageReflectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(const FViewInfo& View)
	{
		DeferredParameters.Set(View, this);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredVertexShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(,FImageReflectionVertexShader,TEXT("ImageReflectionShader"),TEXT("VertexMain"),SF_Vertex,0,0);

/** 
 * A pixel shader used to render half resolution static reflection shadowing.
 * Two versions - one that is used with MSAA and one that is not.
 */
template<UBOOL bSupportMSAA>
class TReflectionStaticShadowingPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TReflectionStaticShadowingPixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("IMAGE_REFLECTION_MSAA"),bSupportMSAA ? TEXT("1") : TEXT("0"));
	}

	TReflectionStaticShadowingPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ReflectionParameters.Bind(Initializer.ParameterMap);
	}

	TReflectionStaticShadowingPixelShader()
	{
	}

	void SetParameters(const FSceneView& View, FScene& Scene, const FReflectionPlanarShadowInfo* ShadowInfo)
	{
		ReflectionParameters.Set(GetPixelShader(), View, Scene, ShadowInfo);
		DeferredParameters.Set(View, this);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ReflectionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FImageReflectionShaderParameters ReflectionParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TReflectionStaticShadowingPixelShader<TRUE>,TEXT("ImageReflectionShader"),TEXT("StaticShadowPixelMain"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(template<>,TReflectionStaticShadowingPixelShader<FALSE>,TEXT("ImageReflectionShader"),TEXT("StaticShadowPixelMain"),SF_Pixel,0,0);

/** 
 * A pixel shader used to render deferred image reflections. 
 * Two versions - one that supports reading and shading the MSAA samples, and one that does not.
 */
template<UBOOL bSupportMSAA>
class TImageReflectionPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TImageReflectionPixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("NUM_IMAGE_REFLECTIONS"),*appItoa(FImageReflectionShaderParameters::MaxNumImageReflections));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHT_REFLECTIONS"),*appItoa(FImageReflectionShaderParameters::MaxNumLightReflections));
		OutEnvironment.Definitions.Set(TEXT("IMAGE_REFLECTION_MSAA"),bSupportMSAA ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("DOWNSAMPLE_STATIC_SHADOWING"),GDownsampleStaticReflectionShadowing ? TEXT("1") : TEXT("0"));
	}

	TImageReflectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ReflectionParameters.Bind(Initializer.ParameterMap);
	}

	TImageReflectionPixelShader()
	{
	}

	void SetParameters(const FSceneView& View, FScene& Scene, const FReflectionPlanarShadowInfo* ShadowInfo)
	{
		ReflectionParameters.Set(GetPixelShader(), View, Scene, ShadowInfo);
		DeferredParameters.Set(View, this);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ReflectionParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FImageReflectionShaderParameters ReflectionParameters;
};

/** Implement a version that supports MSAA, and one that does not. */
IMPLEMENT_SHADER_TYPE(template<>,TImageReflectionPixelShader<TRUE>,TEXT("ImageReflectionShader"),TEXT("PixelMain"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(template<>,TImageReflectionPixelShader<FALSE>,TEXT("ImageReflectionShader"),TEXT("PixelMain"),SF_Pixel,0,0);

class FImageReflectionPerSamplePixelShader : public TImageReflectionPixelShader<TRUE>
{
	DECLARE_SHADER_TYPE(FImageReflectionPerSamplePixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TImageReflectionPixelShader<TRUE>::ShouldCache(Platform);
	}

	FImageReflectionPerSamplePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	TImageReflectionPixelShader<TRUE>(Initializer)
	{}

	FImageReflectionPerSamplePixelShader()
	{}
};

IMPLEMENT_SHADER_TYPE(,FImageReflectionPerSamplePixelShader,TEXT("ImageReflectionShader"),TEXT("SampleMain"),SF_Pixel,0,0);

FGlobalBoundShaderState ImageReflectionStaticShadowingMSAABoundShaderState;
FGlobalBoundShaderState ImageReflectionStaticShadowingNoMSAABoundShaderState;
FGlobalBoundShaderState ImageReflectionBoundStateMSAAFirstPass;
FGlobalBoundShaderState ImageReflectionBoundStateMSAASecondPass;
FGlobalBoundShaderState ImageReflectionBoundStateNoMSAA;

void FImageReflectionShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	NumActiveReflectionsParameter.Bind(ParameterMap,TEXT("NumReflectionsAndTextureResolution"),TRUE);
	ImageReflectionPlaneParameter.Bind(ParameterMap,TEXT("ImageReflectionPlane"),TRUE);
	ImageReflectionOriginParameter.Bind(ParameterMap,TEXT("ImageReflectionOrigin"),TRUE);
	ImageReflectionXAxisParameter.Bind(ParameterMap,TEXT("ReflectionXAxis"),TRUE);
	ImageReflectionColorParameter.Bind(ParameterMap,TEXT("ReflectionColor"),TRUE);
	LightReflectionOriginParameter.Bind(ParameterMap,TEXT("LightReflectionOrigin"),TRUE);
	LightReflectionColorParameter.Bind(ParameterMap,TEXT("LightReflectionColor"),TRUE);
	ImageReflectionTextureParameter.Bind(ParameterMap,TEXT("ImageReflectionTexture"),TRUE);
	ImageReflectionSamplerParameter.Bind(ParameterMap,TEXT("ImageReflectionSampler"),TRUE);
	StaticShadowingTextureParameter0.Bind(ParameterMap,TEXT("StaticShadowingTexture0"),TRUE);
	StaticShadowingTextureParameter1.Bind(ParameterMap,TEXT("StaticShadowingTexture1"),TRUE);
	ReflectionMaskTextureParameter.Bind(ParameterMap,TEXT("ReflectionMaskTexture"),TRUE);
	MirrorPlaneParameter.Bind(ParameterMap,TEXT("MirrorPlane"),TRUE);

	VolumeMinParameter.Bind(ParameterMap,TEXT("VolumeMinAndMaxDistance"),TRUE);
	VolumeSizeParameter.Bind(ParameterMap,TEXT("VolumeSizeAndbCalculateShadowing"),TRUE);
	DistanceFieldTextureParameter.Bind(ParameterMap,TEXT("DistanceFieldTexture"),TRUE);
	DistanceFieldSamplerParameter.Bind(ParameterMap,TEXT("DistanceFieldSampler"),TRUE);

	EnvironmentTextureParameter.Bind(ParameterMap,TEXT("EnvironmentTexture"),TRUE);
	EnvironmentSamplerParameter.Bind(ParameterMap,TEXT("EnvironmentSampler"),TRUE);
	EnvironmentColorParameter.Bind(ParameterMap,TEXT("EnvironmentColor"),TRUE);
	ViewProjectionMatrixParameter.Bind(ParameterMap,TEXT("ViewProjectionMatrix"),TRUE);
	TexelSizesParameter.Bind(ParameterMap,TEXT("TexelSizes"),TRUE);
	PixelSizesParameter.Bind(ParameterMap,TEXT("PixelSizes"),TRUE);
}

FArchive& operator<<(FArchive& Ar,FImageReflectionShaderParameters& Parameters)
{
	Ar << Parameters.NumActiveReflectionsParameter;
	Ar << Parameters.ImageReflectionPlaneParameter;
	Ar << Parameters.ImageReflectionOriginParameter;
	Ar << Parameters.ImageReflectionXAxisParameter;
	Ar << Parameters.ImageReflectionColorParameter;
	Ar << Parameters.LightReflectionOriginParameter;
	Ar << Parameters.LightReflectionColorParameter;
	Ar << Parameters.ImageReflectionTextureParameter;
	Ar << Parameters.ImageReflectionSamplerParameter;
	Ar << Parameters.StaticShadowingTextureParameter0;
	Ar << Parameters.StaticShadowingTextureParameter1;
	Ar << Parameters.ReflectionMaskTextureParameter;
	Ar << Parameters.MirrorPlaneParameter;
	Ar << Parameters.VolumeMinParameter;
	Ar << Parameters.VolumeSizeParameter;
	Ar << Parameters.DistanceFieldTextureParameter;
	Ar << Parameters.DistanceFieldSamplerParameter;
	Ar << Parameters.EnvironmentTextureParameter;
	Ar << Parameters.EnvironmentSamplerParameter;
	Ar << Parameters.EnvironmentColorParameter;
	Ar << Parameters.ViewProjectionMatrixParameter;
	Ar << Parameters.TexelSizesParameter;
	Ar << Parameters.PixelSizesParameter;
	return Ar;
}

/** 
 * Number of simultaneous image reflections supported. 
 * This is limited by constant buffer size since the image reflection instance data takes a lot of constants to upload.
 */
const UINT FImageReflectionShaderParameters::MaxNumImageReflections = 85;

/** 
 * Number of simultaneous light reflections supported. 
 * This is limited by constant buffer size since the image reflection instance data takes a lot of constants to upload.
 */
const UINT FImageReflectionShaderParameters::MaxNumLightReflections = 85;

/** Sets the shader parameters needed for image based reflections. */
void FImageReflectionShaderParameters::Set(const FPixelShaderRHIParamRef PixelShaderRHI, const FSceneView& View, FScene& Scene, const FReflectionPlanarShadowInfo* ShadowInfo) const
{
#if PLATFORM_SUPPORTS_D3D10_PLUS
	if ((NumActiveReflectionsParameter.IsBound() || ImageReflectionTextureParameter.IsBound() || ImageReflectionPlaneParameter.IsBound())
		&& GRHIShaderPlatform == SP_PCD3D_SM5)
	{
		FVector4 ImageReflectionPlanes[MaxNumImageReflections];
		FVector4 ImageReflectionOrigins[MaxNumImageReflections];
		FVector4 ImageReflectionXAxes[MaxNumImageReflections];
		FVector4 ImageReflectionColors[MaxNumImageReflections];
		FVector4 LightReflectionOrigins[MaxNumLightReflections];
		FVector4 LightReflectionColors[MaxNumLightReflections];

		if (ImageReflectionSamplerParameter.IsBound())
		{
			// Set the sampler state used for sampling image reflection textures
			// Forces anisotropic filtering with lots of samples and border color clamping with a border alpha of 0 (transparent)
			RHISetSamplerStateOnly(
				PixelShaderRHI, 
				ImageReflectionSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_AnisotropicLinear,AM_Border,AM_Border,AM_Clamp,MIPBIAS_None,16,0>::GetRHI());
		}

		if (ShadowInfo)
		{
			SetTextureParameter(
				PixelShaderRHI, 
				ReflectionMaskTextureParameter, 
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), 
				GBlurDynamicReflectionShadowing ? GSceneRenderTargets.GetBokehDOFTexture() : GSceneRenderTargets.GetTranslucencyBufferTexture()
				);

			SetPixelShaderValue(
				PixelShaderRHI, 
				MirrorPlaneParameter, 
				ShadowInfo->MirrorPlane);
		}
		else
		{
			SetTextureParameter(
				PixelShaderRHI, 
				ReflectionMaskTextureParameter, 
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), 
				GBlackTexture->TextureRHI
				);

			SetPixelShaderValue(
				PixelShaderRHI, 
				MirrorPlaneParameter, 
				FPlane(0, 0, 1, WORLD_MAX));
		}

		SetTextureParameter(
			PixelShaderRHI, 
			StaticShadowingTextureParameter0, 
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), 
			GSceneRenderTargets.GetTranslucencyBufferTexture()
			);

		SetTextureParameter(
			PixelShaderRHI, 
			StaticShadowingTextureParameter1, 
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), 
			GSceneRenderTargets.GetHalfResPostProcessTexture()
			);

		TArray<FImageReflectionSceneInfo*> ImageReflections;
		Scene.ImageReflections.GenerateValueArray(ImageReflections);

		INT ValidImageReflectionIndex = 0;
		INT ValidLightReflectionIndex = 0;
		for (INT ReflectionIndex = 0; ReflectionIndex < ImageReflections.Num(); ReflectionIndex++)
		{
			const FImageReflectionSceneInfo* ImageReflection = ImageReflections(ReflectionIndex);
			if (ImageReflection->bLightReflection)
			{
				if (ValidLightReflectionIndex < MaxNumLightReflections)
				{
					LightReflectionOrigins[ValidLightReflectionIndex] = FVector4(ImageReflection->ReflectionOrigin, 0);
					LightReflectionColors[ValidLightReflectionIndex] = FVector4(ImageReflection->ReflectionColor);
					ValidLightReflectionIndex++;
				}
			}
			else
			{
				const INT TextureIndex = Scene.ImageReflectionTextureArray.GetTextureIndex(ImageReflection->ReflectionTexture);
				// Only use reflections with a valid texture resource
				if (TextureIndex != INDEX_NONE 
					&& ValidImageReflectionIndex < MaxNumImageReflections
					&& ImageReflection->bEnabled)
				{
					ImageReflectionPlanes[ValidImageReflectionIndex] = FVector4(ImageReflection->ReflectionPlane, ImageReflection->ReflectionPlane.W);
					ImageReflectionOrigins[ValidImageReflectionIndex] = FVector4(ImageReflection->ReflectionOrigin, TextureIndex);
					ImageReflectionXAxes[ValidImageReflectionIndex] = ImageReflection->ReflectionXAxisAndYScale;
					ImageReflectionColors[ValidImageReflectionIndex] = FVector4(FVector4(ImageReflection->ReflectionColor), ImageReflection->bTwoSided ? 1.0f : 0.0f);

					ValidImageReflectionIndex++;
				}
			}
		}

		const FLOAT NumActiveImageReflectionsFloat = ValidImageReflectionIndex;
		const FLOAT NumActiveLightReflectionsFloat = ValidLightReflectionIndex;
		// Set the arrays of reflection quad data
		// Anisotropy value in w
		SetPixelShaderValue(PixelShaderRHI, NumActiveReflectionsParameter, FVector4(NumActiveImageReflectionsFloat, NumActiveLightReflectionsFloat, Scene.ImageReflectionTextureArray.GetSizeX(), 16.0f));
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionPlaneParameter, &ImageReflectionPlanes, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionOriginParameter, &ImageReflectionOrigins, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionXAxisParameter, &ImageReflectionXAxes, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionColorParameter, &ImageReflectionColors, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, LightReflectionOriginParameter, &LightReflectionOrigins, ValidLightReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, LightReflectionColorParameter, &LightReflectionColors, ValidLightReflectionIndex);

		if (ImageReflectionTextureParameter.IsBound())
		{
			if (ValidImageReflectionIndex > 0 && Scene.ImageReflectionTextureArray.IsInitialized())
			{
				check(Scene.ImageReflectionTextureArray.TextureRHI);
				RHISetTextureParameter(
					PixelShaderRHI, 
					ImageReflectionTextureParameter.GetBaseIndex(), 
					Scene.ImageReflectionTextureArray.TextureRHI);
			}
			else
			{
				RHISetTextureParameter(
					PixelShaderRHI, 
					ImageReflectionTextureParameter.GetBaseIndex(), 
					GBlackArrayTexture->TextureRHI);
			}
		}

		SetPixelShaderValue(PixelShaderRHI, VolumeMinParameter, FVector4(Scene.VolumeDistanceFieldBox.Min, Scene.VolumeDistanceFieldMaxDistance));
		const FLOAT CalculateVolumeShadowing = Scene.PrecomputedDistanceFieldVolumeTexture.TextureRHI && GSystemSettings.bAllowImageReflectionShadowing ? 1.0f : 0.0f;
		SetPixelShaderValue(PixelShaderRHI, VolumeSizeParameter, FVector4(Scene.VolumeDistanceFieldBox.Max - Scene.VolumeDistanceFieldBox.Min, CalculateVolumeShadowing));

		if (EnvironmentSamplerParameter.IsBound())
		{
			RHISetSamplerStateOnly(
				PixelShaderRHI, 
				EnvironmentSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_Trilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI());
		}

		if (EnvironmentTextureParameter.IsBound())
		{
			if (Scene.ImageReflectionEnvironmentTexture
				&& Scene.ImageReflectionEnvironmentTexture->Resource->TextureRHI)
			{
				// Update last render time so it will be streamed correctly
				Scene.ImageReflectionEnvironmentTexture->Resource->LastRenderTime = GCurrentTime;
				RHISetTextureParameter(
					PixelShaderRHI, 
					EnvironmentTextureParameter.GetBaseIndex(), 
					Scene.ImageReflectionEnvironmentTexture->Resource->TextureRHI);
			}
			else
			{
				RHISetTextureParameter(
					PixelShaderRHI, 
					EnvironmentTextureParameter.GetBaseIndex(), 
					GBlackTexture->TextureRHI);
			}
		}

		SetPixelShaderValue(PixelShaderRHI, EnvironmentColorParameter, FVector4(Scene.EnvironmentColor, Scene.EnvironmentRotation * PI / 180.0f));

		if (DistanceFieldTextureParameter.IsBound() 
			&& Scene.PrecomputedDistanceFieldVolumeTexture.IsInitialized()
			&& Scene.PrecomputedDistanceFieldVolumeTexture.TextureRHI)
		{
			RHISetTextureParameter(
				PixelShaderRHI, 
				DistanceFieldTextureParameter.GetBaseIndex(), 
				Scene.PrecomputedDistanceFieldVolumeTexture.TextureRHI);
		}

		if (DistanceFieldSamplerParameter.IsBound())
		{
			RHISetSamplerStateOnly(
				PixelShaderRHI, 
				DistanceFieldSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_Trilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		}

		SetPixelShaderValue(PixelShaderRHI, ViewProjectionMatrixParameter, View.ViewProjectionMatrix);

		SetPixelShaderValue(PixelShaderRHI, TexelSizesParameter, FVector4(
			1.0f / GSceneRenderTargets.GetBufferSizeX(), 
			1.0f / GSceneRenderTargets.GetBufferSizeY(),
			1.0f / (GSceneRenderTargets.GetBufferSizeX() / 2),
			1.0f / (GSceneRenderTargets.GetBufferSizeY() / 2)));

		SetPixelShaderValue(PixelShaderRHI, PixelSizesParameter, FVector4(
			2.0f / View.RenderTargetSizeX, 
			2.0f / View.RenderTargetSizeY,
			2.0f / (View.RenderTargetSizeX / 2),
			2.0f / (View.RenderTargetSizeY / 2)));
	}
#endif
}

/** Creates planar reflection shadows for this frame. */
void FSceneRenderer::CreatePlanarReflectionShadows()
{
	if (GRHIShaderPlatform == SP_PCD3D_SM5 && GRenderDynamicReflectionShadowing)
	{
		//@todo - handle multiple views
		const FViewInfo& View = Views(0);

		for (TMap<const UActorComponent*, FPlane>::TConstIterator It(Scene->ImageReflectionShadowPlanes); It; ++It)
		{
			FReflectionPlanarShadowInfo NewShadowInfo;
			NewShadowInfo.MirrorPlane = It.Value();

			// FMirrorMatrix wants the flipped plane for some reason
			const FMirrorMatrix MirrorMatrix(NewShadowInfo.MirrorPlane * -1);
			const FMatrix MirrorViewProjectionMatrix = (MirrorMatrix * View.ViewProjectionMatrix);
			GetViewFrustumBounds(NewShadowInfo.ViewFrustum, MirrorViewProjectionMatrix, FALSE);
			PlanarReflectionShadows.AddItem(NewShadowInfo);
			break;
		}
	}
}

/** Renders deferred image reflections. */
UBOOL FSceneRenderer::RenderImageReflections(UINT DPGIndex)
{
#if PLATFORM_SUPPORTS_D3D10_PLUS
	if(
		( DPGIndex == SDPG_World )
		&& ( GRHIShaderPlatform == SP_PCD3D_SM5 )
		&& ( GSystemSettings.bAllowImageReflections )
		&& ( ( ViewFamily.ShowFlags & SHOW_ImageReflections ) != 0 )
		&& ( ViewFamily.ShouldPostProcess() )
	)
	{
		Scene->ImageReflectionTextureArray.UpdateResource();

		UBOOL bAnyReflectionsToRender = Scene->ImageReflectionTextureArray.IsInitialized() || Scene->ImageReflectionEnvironmentTexture;
		for (TMap<const UActorComponent*, FImageReflectionSceneInfo*>::TConstIterator ReflectionIt(Scene->ImageReflections); ReflectionIt; ++ReflectionIt)
		{
			const FImageReflectionSceneInfo* ImageReflection = ReflectionIt.Value();
			bAnyReflectionsToRender = bAnyReflectionsToRender 
				|| ImageReflection->bLightReflection
				|| Scene->ImageReflectionTextureArray.GetTextureIndex(ImageReflection->ReflectionTexture) != INDEX_NONE;
		}

		if (bAnyReflectionsToRender)
		{
			SCOPED_DRAW_EVENT(EventRenderDeferredReflections)(DEC_SCENE_ITEMS,TEXT("Deferred Reflections"));

			const FReflectionPlanarShadowInfo* PlanarShadowInfo = NULL;
			if (PlanarReflectionShadows.Num() > 0)
			{
				{
					PlanarShadowInfo = &PlanarReflectionShadows(0);
					// FMirrorMatrix wants the flipped plane for some reason
					const FMirrorMatrix MirrorMatrix(PlanarShadowInfo->MirrorPlane * -1);

					SCOPED_DRAW_EVENT(EventRenderReflectionMask)(DEC_SCENE_ITEMS,TEXT("Dynamic Planar Shadow masks"));

					RHISetDepthState(TStaticDepthState<TRUE, CF_LessEqual>::GetRHI());
					RHISetBlendState(TStaticBlendState<>::GetRHI());

					RHISetRenderTarget(GSceneRenderTargets.GetTranslucencyBufferSurface(), GSceneRenderTargets.GetReflectionSmallDepthSurface());

					RHIClear(TRUE, FLinearColor(0,0,0,0), TRUE, 1.0f, FALSE, 0);

					for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						FViewInfo& View = Views(ViewIndex);

						const FVector ReflectedCameraPosition = MirrorMatrix.TransformFVector(View.ViewOrigin);
		
						// Translate the mirror plane to be in the offsetted world space used in vertex shaders
						const FPlane TranslatedMirrorPlane(PlanarShadowInfo->MirrorPlane, PlanarShadowInfo->MirrorPlane.W + ((FVector)PlanarShadowInfo->MirrorPlane | View.PreViewTranslation));
		
						// Set the device viewport for the view
						RHISetViewport(View.RenderTargetX / 2, View.RenderTargetY / 2, 0.0f, View.RenderTargetX / 2 + View.RenderTargetSizeX / 2, View.RenderTargetY / 2 + View.RenderTargetSizeY / 2, 1.0f);

						const FMirrorMatrix TranslatedMirrorMatrix(TranslatedMirrorPlane * -1);
						// Apply the mirror matrix to world space positions
						// This renders a planar reflection by mirroring the world around the reflection plane, instead of mirroring the camera
						const FMatrix MirrorViewProjectionMatrix = TranslatedMirrorMatrix * View.TranslatedViewProjectionMatrix;
						RHISetViewParametersWithOverrides(View, MirrorViewProjectionMatrix, View.DiffuseOverrideParameter, View.SpecularOverrideParameter);
						RHISetMobileHeightFogParams(View.HeightFogParams);
						
						TDynamicPrimitiveDrawer<FReflectionMaskDrawingPolicyFactory> CurrentDPGDrawer(
							&View, 
							DPGIndex, 
							FReflectionMaskDrawingPolicyFactory::ContextType(PlanarShadowInfo->MirrorPlane, ReflectedCameraPosition, TranslatedMirrorPlane), 
							TRUE);

						// Render visible dynamic meshes for this planar reflection shadow
						for (INT PrimitiveIndex = 0; PrimitiveIndex < PlanarShadowInfo->VisibleDynamicPrimitives.Num(); PrimitiveIndex++)
						{
							const FPrimitiveSceneInfo* PrimitiveSceneInfo = PlanarShadowInfo->VisibleDynamicPrimitives(PrimitiveIndex);
							FPrimitiveViewRelevance PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

							if (!PrimitiveViewRelevance.bInitializedThisFrame)
							{
								// Calculate the relevance if it was not cached from the main views
								PrimitiveViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
							}
		
							// Only render if visible
							if(PrimitiveViewRelevance.GetDPG(DPGIndex) 
								// Used to determine whether object is movable or not
								&& !PrimitiveSceneInfo->bStaticShadowing 
								// Only render shadow casters
								&& PrimitiveViewRelevance.bShadowRelevance
								// Skip translucent objects
								&& PrimitiveViewRelevance.bOpaqueRelevance)
							{
								CurrentDPGDrawer.SetPrimitive(PrimitiveSceneInfo);
								PrimitiveSceneInfo->Proxy->DrawDynamicElements(
									&CurrentDPGDrawer,
									&View,
									DPGIndex
									);
							}
						}
					}

					RHICopyToResolveTarget(GSceneRenderTargets.GetTranslucencyBufferSurface(), FALSE, FResolveParams());
				}

				if (GBlurDynamicReflectionShadowing)
				{
					SCOPED_DRAW_EVENT(EventBlurReflectionMask)(DEC_SCENE_ITEMS,TEXT("Blur Dynamic masks"));

					// No depth tests
					RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());

					// Use additive blending, since we are going to accumulate quads
					RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

					// No backface culling
					RHISetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

					RHISetRenderTarget(GSceneRenderTargets.GetBokehDOFSurface(), FSurfaceRHIRef());

					RHIClear(TRUE, FLinearColor(0,0,0,0), FALSE, 0, FALSE, 0);

					for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						FViewInfo& View = Views(ViewIndex);

						// Set the device viewport for the view.
						RHISetViewport(View.RenderTargetX / 2, View.RenderTargetY / 2, 0.0f, View.RenderTargetX / 2 + View.RenderTargetSizeX / 2, View.RenderTargetY / 2 + View.RenderTargetSizeY / 2, 1.0f);
						RHISetViewParameters(View);
						RHISetMobileHeightFogParams(View.HeightFogParams);

						TShaderMapRef<FScreenVertexShader> VertexShader(GetGlobalShaderMap());
						TShaderMapRef<FMaskBlurGeometryShader> GeometryShader(GetGlobalShaderMap());
						TShaderMapRef<FMaskBlurPixelShader> PixelShader(GetGlobalShaderMap());

						SetGlobalBoundShaderState(
							MaskBlurBoundShaderState,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FFilterVertex),
							*GeometryShader
							);

						GeometryShader->SetParameters(View);
		
						// Using a stride of 0 so the one vertex gets repeated
						UINT Stride = 0;
						UINT NumVerticesPerInstance = 3;

						// Reuse the bokeh DOF vertex buffer which just has one vertex
						RHISetStreamSource(0, FBokehDOFRenderer::VertexBuffer.VertexBufferRHI, Stride, 0, FALSE, NumVerticesPerInstance, 1);

						const UINT HalfSizeX = View.RenderTargetSizeX / 2;
						const UINT HalfSizeY = View.RenderTargetSizeY / 2;

						UINT NumPrimitivesInX = HalfSizeX;
						UINT NumPrimitivesInY = HalfSizeY;

						UINT BatchVertexIndex = 0;

						// Render one primitive per half res pixel, the geometry shader will create a quad out of it and size it based on the blur kernel radius
						UINT NumPrimitivesInBatch = NumPrimitivesInX * NumPrimitivesInY;
						RHIDrawPrimitive(PT_TriangleList, BatchVertexIndex, NumPrimitivesInBatch);

						RHICopyToResolveTarget(GSceneRenderTargets.GetBokehDOFSurface(), FALSE, FResolveParams());
					}
				}
			}

			{
				// No depth tests
				RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());

				// No backface culling
				RHISetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

				if (GDownsampleStaticReflectionShadowing)
				{
					SCOPED_DRAW_EVENT(EventRenderIRShadowing)(DEC_SCENE_ITEMS,TEXT("Static shadowing"));

					RHISetBlendState(TStaticBlendState<>::GetRHI());

					RHISetRenderTarget(GSceneRenderTargets.GetTranslucencyBufferSurface(), FSurfaceRHIRef());
					RHISetMRTRenderTarget(GSceneRenderTargets.GetHalfResPostProcessSurface(), 1);
					RHISetMRTColorWriteEnable(TRUE, 1);

					for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						FViewInfo& View = Views(ViewIndex);

						// Set the device viewport for the view.
						RHISetViewport(View.RenderTargetX / 2, View.RenderTargetY / 2, 0.0f, View.RenderTargetX / 2 + View.RenderTargetSizeX / 2, View.RenderTargetY / 2 + View.RenderTargetSizeY / 2, 1.0f);
						RHISetViewParameters(View);
						RHISetMobileHeightFogParams(View.HeightFogParams);

						TShaderMapRef<FImageReflectionVertexShader> VertexShader(GetGlobalShaderMap());
						VertexShader->SetParameters(View);

						if (GSystemSettings.UsesMSAA())
						{
							TShaderMapRef<TReflectionStaticShadowingPixelShader<TRUE> > PixelShader(GetGlobalShaderMap());
							SetGlobalBoundShaderState(
								ImageReflectionStaticShadowingMSAABoundShaderState,
								GFilterVertexDeclaration.VertexDeclarationRHI,
								*VertexShader,
								*PixelShader,
								sizeof(FFilterVertex)
								);

							PixelShader->SetParameters(View, *Scene, PlanarShadowInfo);
						}
						else
						{
							TShaderMapRef<TReflectionStaticShadowingPixelShader<FALSE> > PixelShader(GetGlobalShaderMap());
							SetGlobalBoundShaderState(
								ImageReflectionStaticShadowingNoMSAABoundShaderState,
								GFilterVertexDeclaration.VertexDeclarationRHI,
								*VertexShader,
								*PixelShader,
								sizeof(FFilterVertex)
								);

							PixelShader->SetParameters(View, *Scene, PlanarShadowInfo);
						}
					
						// Render static reflection shadowing at half resolution
						DrawDenormalizedQuad( 
							View.RenderTargetX / 2, View.RenderTargetY / 2, 
							View.RenderTargetSizeX / 2, View.RenderTargetSizeY / 2,
							View.RenderTargetX, View.RenderTargetY, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetSizeX / 2, View.RenderTargetSizeY / 2,
							GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
					}

					RHICopyToResolveTarget(GSceneRenderTargets.GetTranslucencyBufferSurface(), FALSE, FResolveParams());
					RHICopyToResolveTarget(GSceneRenderTargets.GetHalfResPostProcessSurface(), FALSE, FResolveParams());

					RHISetMRTRenderTarget(FSurfaceRHIRef(), 1);
					RHISetMRTColorWriteEnable(FALSE, 1);
				}

				// Bind scene color
				GSceneRenderTargets.BeginRenderingSceneColor();

				// Use additive blending for color, and keep the destination alpha.
				RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());

				if (GSystemSettings.UsesMSAA())
				{
					// Clear stencil to 0
					RHIClear(FALSE, FLinearColor(0,0,0,0), FALSE, 0, TRUE, 0);
				}

				for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					FViewInfo& View = Views(ViewIndex);

					// Set the device viewport for the view.
					RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
					RHISetViewParameters(View);
					RHISetMobileHeightFogParams(View.HeightFogParams);

					TShaderMapRef<FImageReflectionVertexShader> VertexShader(GetGlobalShaderMap());
					VertexShader->SetParameters(View);

					if (GSystemSettings.UsesMSAA())
					{
						SCOPED_DRAW_EVENT(EventRenderImageReflections)(DEC_SCENE_ITEMS,TEXT("Image Reflections"));

						// Set stencil to one.
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff,1
							>::GetRHI());

						TShaderMapRef<TImageReflectionPixelShader<TRUE> > PixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							ImageReflectionBoundStateMSAAFirstPass,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FFilterVertex)
							);

						PixelShader->SetParameters(View, *Scene, PlanarShadowInfo);

						DrawDenormalizedQuad( 
							View.RenderTargetX, View.RenderTargetY, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetX, View.RenderTargetY, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());


						// Pass if 0
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0,0
							>::GetRHI());

						TShaderMapRef<FImageReflectionPerSamplePixelShader> SamplePixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							ImageReflectionBoundStateMSAASecondPass,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*SamplePixelShader,
							sizeof(FFilterVertex)
							);

						SamplePixelShader->SetParameters(View, *Scene, PlanarShadowInfo);
					}
					else
					{
						TShaderMapRef<TImageReflectionPixelShader<FALSE> > PixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							ImageReflectionBoundStateNoMSAA,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FFilterVertex)
							);

						PixelShader->SetParameters(View, *Scene, PlanarShadowInfo);
					}

					SCOPED_DRAW_EVENT(EventRenderPerSample)(DEC_SCENE_ITEMS,(GSystemSettings.UsesMSAA() ? TEXT("PerSample IR") : TEXT("Image Reflections")));

					DrawDenormalizedQuad( 
						View.RenderTargetX, View.RenderTargetY, 
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						View.RenderTargetX, View.RenderTargetY, 
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
				}

				// Restore default stencil state
				RHISetStencilState(TStaticStencilState<>::GetRHI());

				GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
			}
			return TRUE;
		}
	}
#endif

	return FALSE;
}
