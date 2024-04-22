/**
 *	LensFlareRendering.cpp: LensFlare rendering functionality.
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "LensFlare.h"
#include "ScenePrivate.h"

#if WITH_REALD
#include "RealD/RealD.h"
#endif

/** Sorting helper */
IMPLEMENT_COMPARE_CONSTREF(FLensFlareElementOrder,LensFlareRendering,{ return A.RayDistance > B.RayDistance ? 1 : -1; });

FLensFlareRenderElement::~FLensFlareRenderElement()
{
	ClearDistribution_Float(LFMaterialIndex);
	ClearDistribution_Float(Scaling);
	ClearDistribution_Vector(AxisScaling);
	ClearDistribution_Float(Rotation);
	ClearDistribution_Vector(Color);
	ClearDistribution_Float(Alpha);
	ClearDistribution_Vector(Offset);
	ClearDistribution_Vector(DistMap_Scale);
	ClearDistribution_Vector(DistMap_Color);
	ClearDistribution_Float(DistMap_Alpha);
}

void FLensFlareRenderElement::CopyFromElement(const FLensFlareElement& InElement, const FLensFlareElementMaterials& InElementMaterials)
{
	check(IsInGameThread());

	RayDistance = InElement.RayDistance;
	bIsEnabled = InElement.bIsEnabled;
	bUseSourceDistance = InElement.bUseSourceDistance;
	bNormalizeRadialDistance = InElement.bNormalizeRadialDistance;
	bModulateColorBySource = InElement.bModulateColorBySource;
	Size = InElement.Size;

	bOrientTowardsSource = InElement.bOrientTowardsSource;

	INT MaterialCount = InElementMaterials.ElementMaterials.Num();
	if (MaterialCount > 0)
	{
		LFMaterials[0].AddZeroed(MaterialCount);
		LFMaterials[1].AddZeroed(MaterialCount);
		for (INT MatIndex = 0; MatIndex < MaterialCount; MatIndex++)
		{
			UMaterialInterface* Mat = InElementMaterials.ElementMaterials(MatIndex);
			if (Mat && Mat->CheckMaterialUsage(MATUSAGE_LensFlare))
			{
				LFMaterials[0](MatIndex) = Mat->GetRenderProxy(FALSE);
				LFMaterials[1](MatIndex) = GIsEditor ? Mat->GetRenderProxy(TRUE) : LFMaterials[0](MatIndex);
			}

			if (LFMaterials[0](MatIndex) == NULL)
			{
				LFMaterials[0](MatIndex) = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
			}
			if (LFMaterials[1](MatIndex) == NULL)
			{
				LFMaterials[1](MatIndex) = GIsEditor ? GEngine->DefaultMaterial->GetRenderProxy(TRUE) : LFMaterials[0](MatIndex);
			}
		}
	}
	
	SetupDistribution_Float(InElement.LFMaterialIndex, LFMaterialIndex);
	SetupDistribution_Float(InElement.Scaling, Scaling);
	SetupDistribution_Vector(InElement.AxisScaling, AxisScaling);
	SetupDistribution_Float(InElement.Rotation, Rotation);
	SetupDistribution_Vector(InElement.Color, Color);
	SetupDistribution_Float(InElement.Alpha, Alpha);
	SetupDistribution_Vector(InElement.Offset, Offset);
	SetupDistribution_Vector(InElement.DistMap_Scale, DistMap_Scale);
	SetupDistribution_Vector(InElement.DistMap_Color, DistMap_Color);
	SetupDistribution_Float(InElement.DistMap_Alpha, DistMap_Alpha);
}

void FLensFlareRenderElement::ClearDistribution_Float(FRawDistributionFloat& Dist)
{
	if (Dist.Distribution != NULL)
	{
		Dist.Distribution->RemoveFromRoot();
		Dist.Distribution = NULL;
	}
}

void FLensFlareRenderElement::ClearDistribution_Vector(FRawDistributionVector& Dist)
{
	if (Dist.Distribution != NULL)
	{
		Dist.Distribution->RemoveFromRoot();
		Dist.Distribution = NULL;
	}
}

void FLensFlareRenderElement::SetupDistribution_Float(const FRawDistributionFloat& SourceDist, FRawDistributionFloat& NewDist)
{
	ClearDistribution_Float(NewDist);
	NewDist = SourceDist;
	if (SourceDist.Distribution != NULL)
	{
		NewDist.Distribution = Cast<UDistributionFloat>(UObject::StaticDuplicateObject(
			SourceDist.Distribution, SourceDist.Distribution, UObject::GetTransientPackage(), TEXT("None")));
		NewDist.Distribution->AddToRoot();
		NewDist.Distribution->bIsDirty = TRUE;
	}
}

void FLensFlareRenderElement::SetupDistribution_Vector(const FRawDistributionVector& SourceDist, FRawDistributionVector& NewDist)
{
	ClearDistribution_Vector(NewDist);
	NewDist = SourceDist;
	if (SourceDist.Distribution != NULL)
	{
		NewDist.Distribution = Cast<UDistributionVector>(UObject::StaticDuplicateObject(
			SourceDist.Distribution, SourceDist.Distribution, UObject::GetTransientPackage(), TEXT("None")));
		NewDist.Distribution->AddToRoot();
		NewDist.Distribution->bIsDirty = TRUE;
	}
}

/**
 *	FLensFlareDynamicData
 */
FLensFlareDynamicData::FLensFlareDynamicData(const ULensFlareComponent* InLensFlareComp, FLensFlareSceneProxy* InProxy) :
	VertexData(NULL)
{
	appMemzero(&SourceElement, sizeof(FLensFlareRenderElement));
	appMemzero(&Reflections, sizeof(TArrayNoInit<FLensFlareRenderElement*>));

	if (InLensFlareComp && InLensFlareComp->Template)
	{
		ULensFlare* LensFlare = InLensFlareComp->Template;
		check(LensFlare);

		// Ony bother copying the rest of the data if it is enabled...
		if (LensFlare->SourceElement.bIsEnabled)
		{
			check(InLensFlareComp->Materials.Num() > 0);
			SourceElement.CopyFromElement(LensFlare->SourceElement, InLensFlareComp->Materials(0));
		}

		for (INT ElementIndex = 0; ElementIndex < LensFlare->Reflections.Num(); ElementIndex++)
		{
			if ((LensFlare->Reflections(ElementIndex).bIsEnabled) &&
				(InLensFlareComp->Materials.Num() > (ElementIndex + 1)))
			{
				FLensFlareRenderElement* NewElement = new(Reflections)FLensFlareRenderElement(LensFlare->Reflections(ElementIndex), InLensFlareComp->Materials(ElementIndex + 1));
			}
			else
			{
				FLensFlareRenderElement* NewElement = new(Reflections)FLensFlareRenderElement();
			}
		}

		INT ElementCount = 1 + LensFlare->Reflections.Num();
		VertexData = new FLensFlareVertex[ElementCount * 4];
	}

	SortElements();

	// Create the vertex factory for rendering the lens flares
	VertexFactory = new FLensFlareVertexFactory();
}

FLensFlareDynamicData::~FLensFlareDynamicData()
{
	// Clean up
	delete [] VertexData;
	delete VertexFactory;
	VertexFactory = NULL;
	Reflections.Empty();
}

DWORD FLensFlareDynamicData::GetMemoryFootprint( void ) const
{
	DWORD CalcSize = 
		sizeof(*this) + 
		sizeof(FLensFlareRenderElement) +							// Source
		sizeof(FLensFlareRenderElement) * Reflections.Num() +		// Reflections
		sizeof(FLensFlareVertex)		* (1 + Reflections.Num());	// VertexData

	return CalcSize;
}

void FLensFlareDynamicData::InitializeRenderResources(const ULensFlareComponent* InLensFlareComp, FLensFlareSceneProxy* InProxy)
{
	check(IsInGameThread());
	if (VertexFactory)
	{
		BeginInitResource(VertexFactory);
	}
}

void FLensFlareDynamicData::RenderThread_InitializeRenderResources(FLensFlareSceneProxy* InProxy)
{
	check(IsInRenderingThread());
	if (VertexFactory && (VertexFactory->IsInitialized() == FALSE))
	{
		VertexFactory->InitResource();
	}
}

void FLensFlareDynamicData::ReleaseRenderResources(const ULensFlareComponent* InLensFlareComp, FLensFlareSceneProxy* InProxy)
{
	check(IsInGameThread());
	if (VertexFactory)
	{
		BeginReleaseResource(VertexFactory);
	}
}

void FLensFlareDynamicData::RenderThread_ReleaseRenderResources()
{
	check(IsInRenderingThread());
	if (VertexFactory)
	{
		VertexFactory->ReleaseResource();
	}
}

/**
 *	Render thread only draw call
 *	
 *	@param	Proxy		The scene proxy for the lens flare
 *	@param	PDI			The PrimitiveDrawInterface
 *	@param	View		The SceneView that is being rendered
 *	@param	DPGIndex	The DrawPrimitiveGroup being rendered
 *	@param	Flags		Rendering flags
 */
void FLensFlareDynamicData::Render(FLensFlareSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex, DWORD Flags)
{
	//@todo. Fill in or remove...
}

/** Render the source element. */
void FLensFlareDynamicData::RenderSource(FLensFlareSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex, DWORD Flags)
{
	// Determine the position of the source
	FVector	SourcePosition = Proxy->GetLocalToWorld().GetOrigin();

	FVector CameraUp	= -View->InvViewProjectionMatrix.TransformNormal(FVector(1.0f,0.0f,0.0f)).SafeNormal();
	FVector CameraRight	= -View->InvViewProjectionMatrix.TransformNormal(FVector(0.0f,1.0f,0.0f)).SafeNormal();

	FVector CameraToSource = View->ViewOrigin - SourcePosition;
	FLOAT DistanceToSource = CameraToSource.Size();

	// Determine the position of the source
	FVector	WorldPosition = SourcePosition;
	FVector4 ScreenPosition = View->WorldToScreen(WorldPosition);

	FPlane StartTemp = View->Project(WorldPosition);
	FVector StartLine(StartTemp.X, StartTemp.Y, 0.0f);
	FPlane EndTemp(-StartTemp.X, -StartTemp.Y, StartTemp.Z, 1.0f);
	FVector4 EndLine = View->InvViewProjectionMatrix.TransformFVector4(EndTemp);
	EndLine.X /= EndLine.W;
	EndLine.Y /= EndLine.W;
	EndLine.Z /= EndLine.W;
	EndLine.W = 1.0f;
	FVector2D StartPos = FVector2D(StartLine);
	
	FLOAT LocalAspectRatio = View->SizeX / View->SizeY;
	FVector2D DrawSize;
	FLensFlareElementValues LookupValues;

	FMeshBatch Mesh;
	FMeshBatchElement& BatchElement = Mesh.Elements(0);
	Mesh.UseDynamicData			= TRUE;
	BatchElement.IndexBuffer			= NULL;
	Mesh.VertexFactory			= VertexFactory;
	Mesh.DynamicVertexStride	= sizeof(FLensFlareVertex);
	BatchElement.DynamicIndexData		= NULL;
	BatchElement.DynamicIndexStride		= 0;
	Mesh.LCI					= NULL;
	BatchElement.LocalToWorld			= Proxy->GetLocalToWorld();
	BatchElement.WorldToLocal			= Proxy->GetWorldToLocal();
	BatchElement.FirstIndex				= 0;
	BatchElement.MinVertexIndex			= 0;
	BatchElement.MaxVertexIndex			= 3;
	Mesh.ParticleType			= PET_None;
	Mesh.ReverseCulling			= Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
	Mesh.CastShadow				= Proxy->GetCastShadow();
	Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
	BatchElement.NumPrimitives			= 2;
	Mesh.Type					= PT_TriangleStrip;
	Mesh.bUsePreVertexShaderCulling = FALSE;
	Mesh.PlatformMeshData       = NULL;

	FLensFlareRenderElement* Element;
	FVector ElementPosition;
	FLensFlareVertex* Vertices;
	FLensFlareVertex ConstantVertexData;
	for (INT ElementIndex = 0; ElementIndex < ElementOrder.Num(); ElementIndex++)
	{
		FLensFlareElementOrder& ElementOrderEntry = ElementOrder(ElementIndex);
		if (ElementOrderEntry.ElementIndex >= 0)
		{
			continue;
		}
		else
		{
			Vertices = &(VertexData[0]);
			Element = &SourceElement;
			ElementPosition = StartLine;
		}

		if (Element)
		{
			GetElementValues(ElementPosition, StartLine, View, DistanceToSource, Element, LookupValues, Proxy->IsSelected() && GIsEditor && (View->Family->ShowFlags & SHOW_Selection));

			if (LookupValues.LFMaterial)
			{
				DrawSize = FVector2D(Element->Size) * LookupValues.Scaling;
				
				ConstantVertexData.Position = FVector(0.0f);
				ConstantVertexData.Size.X = DrawSize.X;
				ConstantVertexData.Size.Y = DrawSize.Y;
				ConstantVertexData.Size.Z = LookupValues.AxisScaling.X;
				ConstantVertexData.Size.W = LookupValues.AxisScaling.Y;
				ConstantVertexData.Rotation.X = LookupValues.Rotation;
				ConstantVertexData.Rotation.Y = 0.0f;
				ConstantVertexData.Color = LookupValues.Color;
				if (Element->bModulateColorBySource)
				{
					ConstantVertexData.Color *= Proxy->GetSourceColor();
				}
				ConstantVertexData.RadialDist_SourceRatio_Intensity.X = LookupValues.SourceDistance;
				ConstantVertexData.RadialDist_SourceRatio_Intensity.Y = LookupValues.RadialDistance;
				ConstantVertexData.RadialDist_SourceRatio_Intensity.Z = Element->RayDistance;
				ConstantVertexData.RadialDist_SourceRatio_Intensity.W = Proxy->GetConeStrength();

				memcpy(&(Vertices[0]), &ConstantVertexData, sizeof(FLensFlareVertex));
				memcpy(&(Vertices[1]), &ConstantVertexData, sizeof(FLensFlareVertex));
				memcpy(&(Vertices[2]), &ConstantVertexData, sizeof(FLensFlareVertex));
				memcpy(&(Vertices[3]), &ConstantVertexData, sizeof(FLensFlareVertex));

				// These appear to be clockwise, but the LF vertex shader code flips the right axis.
				// This is due to it being based on the sprite particle code, which also does that...
				Vertices[0].TexCoord = FVector2D(0.0f, 0.0f);
				Vertices[1].TexCoord = FVector2D(0.0f, 1.0f);
				Vertices[2].TexCoord = FVector2D(1.0f, 0.0f);
				Vertices[3].TexCoord = FVector2D(1.0f, 1.0f);

				Mesh.DynamicVertexData		= Vertices;
				Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
				Mesh.MaterialRenderProxy	= LookupValues.LFMaterial;

				DrawRichMesh(
					PDI, 
					Mesh, 
					FLinearColor(1.0f, 0.0f, 0.0f),	//WireframeColor,
					FLinearColor(1.0f, 1.0f, 0.0f),	//LevelColor,
					FLinearColor(1.0f, 1.0f, 1.0f),	//PropertyColor,		
					Proxy->GetPrimitiveSceneInfo(),
					Proxy->GetSelected()
					);
			}
		}
	}
}

/** Render the reflection elements. */
void FLensFlareDynamicData::RenderReflections(FLensFlareSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex, DWORD Flags)
{
	// Determine the position of the source
	FVector	WorldPosition = Proxy->GetLocalToWorld().GetOrigin();
	FVector4 ScreenPosition = View->WorldToScreen(WorldPosition);
	FVector2D PixelPosition;
	View->ScreenToPixel(ScreenPosition, PixelPosition);

	FVector CameraToSource = View->ViewOrigin - WorldPosition;
	FLOAT DistanceToSource = CameraToSource.Size();

	FPlane StartTemp = View->Project(WorldPosition);
	// Draw a line reflected about the center
	if (Proxy->GetRenderDebug() == TRUE)
	{	
		DrawWireStar(PDI, WorldPosition, 25.0f, FColor(255,0,0), SDPG_Foreground);
	}

	FVector StartLine(StartTemp.X, StartTemp.Y, 0.0f);
	FPlane EndTemp(-StartTemp.X, -StartTemp.Y, StartTemp.Z, 1.0f);
	FVector4 EndLine = View->InvViewProjectionMatrix.TransformFVector4(EndTemp);
	EndLine.X /= EndLine.W;
	EndLine.Y /= EndLine.W;
	EndLine.Z /= EndLine.W;
	EndLine.W = 1.0f;
	FVector2D StartPos = FVector2D(StartLine);
	
	if (Proxy->GetRenderDebug() == TRUE)
	{	
		PDI->DrawLine(WorldPosition, EndLine, FLinearColor(1.0f, 1.0f, 0.0f), SDPG_Foreground);
		DrawWireStar(PDI, EndLine, 25.0f, FColor(0,255,0), SDPG_Foreground);
	}

	FLOAT LocalAspectRatio = View->SizeX / View->SizeY;

	FVector2D DrawSize;
	FLensFlareElementValues LookupValues;

	FMeshBatch Mesh;
	FMeshBatchElement& BatchElement = Mesh.Elements(0);
	Mesh.UseDynamicData			= TRUE;
	BatchElement.IndexBuffer			= NULL;
	Mesh.VertexFactory			= VertexFactory;
	Mesh.DynamicVertexStride	= sizeof(FLensFlareVertex);
	BatchElement.DynamicIndexData		= NULL;
	BatchElement.DynamicIndexStride		= 0;
	Mesh.LCI					= NULL;
	BatchElement.LocalToWorld			= FMatrix::Identity;//Proxy->GetLocalToWorld();
	BatchElement.WorldToLocal			= FMatrix::Identity;//Proxy->GetWorldToLocal();
	BatchElement.FirstIndex				= 0;
	BatchElement.MinVertexIndex			= 0;
	BatchElement.MaxVertexIndex			= 3;
	Mesh.ParticleType			= PET_None;
	Mesh.ReverseCulling			= Proxy->GetLocalToWorldDeterminant() < 0.0f ? TRUE : FALSE;
	Mesh.CastShadow				= Proxy->GetCastShadow();
	Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
	BatchElement.NumPrimitives			= 2;
	Mesh.Type					= PT_TriangleStrip;
	Mesh.bUsePreVertexShaderCulling = FALSE;
	Mesh.PlatformMeshData       = NULL;

	FLOAT MobileAlphaCoefficient = 0.0f;
#if WITH_MOBILE_RHI
	if (GUsingMobileRHI )
	{
		MobileAlphaCoefficient = Proxy->GetOcclusionPercentage(*View);
	}
#endif

	FLensFlareRenderElement* Element;
	FVector ElementPosition;
	FLensFlareVertex* Vertices;
	FLensFlareVertex ConstantVertexData;
	for (INT ElementIndex = 0; ElementIndex < ElementOrder.Num(); ElementIndex++)
	{
		FLensFlareElementOrder& ElementOrderEntry = ElementOrder(ElementIndex);
		if (ElementOrderEntry.ElementIndex >= 0)
		{
			Vertices = &(VertexData[4 + ElementOrderEntry.ElementIndex* 4]);
			Element = &Reflections(ElementOrderEntry.ElementIndex);
			if (Element)
			{
				ElementPosition = FVector(
					((StartPos * (1.0f - Element->RayDistance)) +
					(-StartPos * Element->RayDistance)), 0.0f
					);
			}
		}
		else
		{
			// The source is rendered in RenderSource
			continue;
		}

		if (Element && Element->bIsEnabled)
		{
			GetElementValues(ElementPosition, StartLine, View, DistanceToSource, Element, LookupValues, Proxy->IsSelected() && GIsEditor && (View->Family->ShowFlags & SHOW_Selection)); 
			ElementPosition += LookupValues.Offset;
			if (LookupValues.LFMaterial)
			{
				DrawSize = FVector2D(Element->Size) * LookupValues.Scaling;
				
				FVector4 ElementProjection = FVector4(ElementPosition.X, ElementPosition.Y, 0.1f, 1.0f);
				FPlane ElementPosition = View->InvViewProjectionMatrix.TransformFVector4(ElementProjection);
				FVector SpritePosition = FVector(ElementPosition.X / ElementPosition.W,
												 ElementPosition.Y / ElementPosition.W,
												 ElementPosition.Z / ElementPosition.W);
				DrawSize /= ElementPosition.W;

				ConstantVertexData.Position = SpritePosition;
				ConstantVertexData.Size.X = DrawSize.X;
				ConstantVertexData.Size.Y = DrawSize.Y;
				ConstantVertexData.Size.Z = LookupValues.AxisScaling.X;
				ConstantVertexData.Size.W = LookupValues.AxisScaling.Y;
				ConstantVertexData.Rotation.X = LookupValues.Rotation;
				ConstantVertexData.Rotation.Y = 0.0f;
				ConstantVertexData.Color = LookupValues.Color;

#if WITH_MOBILE_RHI
				if (GUsingMobileRHI)
				{
					FLOAT AttenuationCoefficient = Proxy->GetConeStrength();	//based on cone

					//add another attenuation based on distance outside of view.  
					FLOAT ViewHalfSizeX = (View->SizeX * .5f);
					FLOAT ViewHalfSizeY = (View->SizeY * .5f);
					FLOAT DistanceOutsideScreenX = Abs(PixelPosition.X - ViewHalfSizeX)-ViewHalfSizeX;
					FLOAT DistanceOutsideScreenY = Abs(PixelPosition.Y - ViewHalfSizeY)-ViewHalfSizeY;
					if ((DistanceOutsideScreenX > -50.0f) || (DistanceOutsideScreenY > -50.0f))
					{
						//Fade over 50 pixels (hardcoded for now)
						const FLOAT MaxPixelAttenuationDistanceFactor = .02f;	//   1 / 50 pixels
						FLOAT XAttentuationFactor = -DistanceOutsideScreenX*MaxPixelAttenuationDistanceFactor;
						FLOAT YAttentuationFactor = -DistanceOutsideScreenY*MaxPixelAttenuationDistanceFactor;
						FLOAT FinalAttenuationFactor = Min(XAttentuationFactor, YAttentuationFactor);
						FinalAttenuationFactor = Clamp<FLOAT>(FinalAttenuationFactor, 0.0f, 1.0f);
						AttenuationCoefficient *= FinalAttenuationFactor;
					}

					//Assuming additive on mobile.  Translucent will double get double alpha'ed in this case.
					ConstantVertexData.Color *= MobileAlphaCoefficient*AttenuationCoefficient;
				}
#endif

				if (Element->bModulateColorBySource)
				{
					ConstantVertexData.Color *= Proxy->GetSourceColor();
				}
				ConstantVertexData.RadialDist_SourceRatio_Intensity.X = LookupValues.SourceDistance;
				ConstantVertexData.RadialDist_SourceRatio_Intensity.Y = LookupValues.RadialDistance;
				ConstantVertexData.RadialDist_SourceRatio_Intensity.Z = Element->RayDistance;
				ConstantVertexData.RadialDist_SourceRatio_Intensity.W = Proxy->GetConeStrength();

				memcpy(&(Vertices[0]), &ConstantVertexData, sizeof(FLensFlareVertex));
				memcpy(&(Vertices[1]), &ConstantVertexData, sizeof(FLensFlareVertex));
				memcpy(&(Vertices[2]), &ConstantVertexData, sizeof(FLensFlareVertex));
				memcpy(&(Vertices[3]), &ConstantVertexData, sizeof(FLensFlareVertex));

				// These appear to be clockwise, but the LF vertex shader code flips the right axis.
				// This is due to it being based on the sprite particle code, which also does that...
				Vertices[0].TexCoord = FVector2D(0.0f, 0.0f);
				Vertices[1].TexCoord = FVector2D(0.0f, 1.0f);
				Vertices[2].TexCoord = FVector2D(1.0f, 0.0f);
				Vertices[3].TexCoord = FVector2D(1.0f, 1.0f);

				Mesh.DynamicVertexData		= Vertices;
				Mesh.DepthPriorityGroup		= (ESceneDepthPriorityGroup)DPGIndex;
				Mesh.MaterialRenderProxy	= LookupValues.LFMaterial;

				DrawRichMesh(
					PDI, 
					Mesh, 
					FLinearColor(1.0f, 0.0f, 0.0f),	//WireframeColor,
					FLinearColor(1.0f, 1.0f, 0.0f),	//LevelColor,
					FLinearColor(1.0f, 1.0f, 1.0f),	//PropertyColor,		
					Proxy->GetPrimitiveSceneInfo(),
					Proxy->GetSelected()
					);
			}
		}
	}
}

/**
 *	Retrieve the various values for the given element
 *
 *	@param	ScreenPosition		The position of the element in screen space
 *	@param	SourcePosition		The position of the source in screen space
 *	@param	View				The SceneView being rendered
 *	@param	Element				The element of interest
 *	@param	Values				The values to fill in
 *
 *	@return	UBOOL				TRUE if successful
 */
UBOOL FLensFlareDynamicData::GetElementValues(FVector& ScreenPosition, FVector& SourcePosition, const FSceneView* View, 
	FLOAT DistanceToSource, FLensFlareRenderElement* Element, FLensFlareElementValues& Values, const UBOOL bSelected)
{
	check(Element);

	// The lookup value will be in the range of [0..~1.4] with 
	// 0.0 being right on top of the source (looking directly at the source)
	// 1.0 being on the opposite edge of the screen horizontally or vertically
	// 1.4 being on the opposite edge of the screen diagonally
	FLOAT LookupValue;

	Values.RadialDistance = FVector2D(ScreenPosition.X, ScreenPosition.Y).Size();
	if (Element->bNormalizeRadialDistance)
	{
		// The center point is at 0,0.
		// We need to extend a line through the screen position to the edge of the screen
		// and determine the length, then normal the RadialDistance with that value.
		FVector EdgePosition;
		if (Abs(ScreenPosition.X) > Abs(ScreenPosition.Y))
		{
			EdgePosition.X = 1.0f;
			EdgePosition.Y = Abs(ScreenPosition.Y / ScreenPosition.X);
		}
		else
		{
			EdgePosition.Y = 1.0f;
			EdgePosition.X = Abs(ScreenPosition.X / ScreenPosition.Y);
		}

		Values.RadialDistance /= FVector2D(EdgePosition.X, EdgePosition.Y).Size();
	}
	
	FVector2D SourceToScreen = FVector2D(SourcePosition.X - ScreenPosition.X, SourcePosition.Y - ScreenPosition.Y);
	FVector2D SourceTemp = SourceToScreen;
	SourceTemp /= 2.0f;
	Values.SourceDistance = SourceTemp.Size();
	if (Element->bUseSourceDistance)
	{
		LookupValue = Values.SourceDistance;
	}
	else
	{
		LookupValue = Values.RadialDistance;
	}

	FVector DistScale_Scale = Element->DistMap_Scale.GetValue(DistanceToSource);
	FVector DistScale_Color = Element->DistMap_Color.GetValue(DistanceToSource);
	FLOAT DistScale_Alpha = Element->DistMap_Alpha.GetValue(DistanceToSource);

	INT MaterialIndex = appTrunc(Element->LFMaterialIndex.GetValue(LookupValue));
	if ((MaterialIndex >= 0) && (MaterialIndex < Element->LFMaterials[0].Num()))
	{
		Values.LFMaterial = Element->LFMaterials[bSelected](MaterialIndex);
	}
	else
	{
		Values.LFMaterial = Element->LFMaterials[bSelected](0);
	}

	Values.Scaling = Element->Scaling.GetValue(LookupValue);
	Values.AxisScaling = Element->AxisScaling.GetValue(LookupValue) * DistScale_Scale;
	FLOAT OrientRotation = 0.0f;
	if (Element->bOrientTowardsSource == TRUE)
	{
		FVector2D UpValue(0.0f, 1.0f);
		SourceToScreen.Normalize();
		UpValue.Normalize();
		FLOAT DotProd = SourceToScreen | UpValue;
		OrientRotation = appAcos(DotProd);
		if (ScreenPosition.X > SourcePosition.X)
		{
			OrientRotation *= -1.0f;
		}
	}
	Values.Rotation = OrientRotation + Element->Rotation.GetValue(LookupValue);
	FVector Color = Element->Color.GetValue(LookupValue) * DistScale_Color;
	FLOAT Alpha = Element->Alpha.GetValue(LookupValue) * DistScale_Alpha;
	Values.Color = FLinearColor(Color.X, Color.Y, Color.Z, Alpha);
	Values.Offset = Element->Offset.GetValue(LookupValue);

	return FALSE;
}

/**
 *	Sort the contained elements along the ray
 */
void FLensFlareDynamicData::SortElements()
{
	//selected and unselected should have the same length
	const UBOOL bSelected = FALSE;
	ElementOrder.Empty();
	// Put the source in first...
	if ((SourceElement.LFMaterials[bSelected].Num() > 0) && (SourceElement.LFMaterials[bSelected](0)))
	{
		new(ElementOrder)FLensFlareElementOrder(-1, SourceElement.RayDistance);
	}

	for (INT ElementIndex = 0; ElementIndex < Reflections.Num(); ElementIndex++)
	{
		FLensFlareRenderElement* Element = &Reflections(ElementIndex);
		if (Element && (Element->LFMaterials[bSelected].Num() > 0))
		{
			new(ElementOrder)FLensFlareElementOrder(ElementIndex, Element->RayDistance);
		}
	}
	
	Sort<USE_COMPARE_CONSTREF(FLensFlareElementOrder,LensFlareRendering)>(&(ElementOrder(0)),ElementOrder.Num());
}

//
//	Scene Proxies
//
/** Initialization constructor. */
FLensFlareSceneProxy::FLensFlareSceneProxy(const ULensFlareComponent* Component) :
	  FPrimitiveSceneProxy(Component, Component->Template ? Component->Template->GetFName() : NAME_None)
	, FPrimitiveSceneProxyOcclusionTracker(Component)
	, Owner(Component->GetOwner())
	, bSelected(Component->IsOwnerSelected())
	, bIsActive(Component->bIsActive)
	, CullDistance(Component->CachedMaxDrawDistance > 0 ? Component->CachedMaxDrawDistance : WORLD_MAX)
	, bCastShadow(Component->CastShadow)
	, bHasTranslucency(Component->HasUnlitTranslucency())
	, bHasUnlitTranslucency(Component->HasUnlitTranslucency())
	, bHasSeparateTranslucency(Component->HasSeparateTranslucency())
	, bHasLitTranslucency(Component->HasLitTranslucency())
	, bHasDistortion(Component->HasUnlitDistortion())
	, bUsesSceneColor(bHasTranslucency && Component->UsesSceneColor())
	, bUseTrueConeCalculation(Component->bUseTrueConeCalculation)
	, OuterCone(Component->OuterCone)
	, InnerCone(Component->InnerCone)
	, ConeFudgeFactor(Component->ConeFudgeFactor)
	, Radius(Component->Radius)
	, ConeStrength(1.0f)
	, MinStrength(Component->MinStrength)
	, MobileOcclusionPercentage(0.0f)
	, SourceColor(Component->SourceColor)
	, DynamicData(NULL)
#if WITH_READ
	, StereoCoveragePercentage(0.0f)
#endif
{
	bRequiresOcclusionForCorrectness = TRUE;

	check(IsInGameThread());
	if (Component->Template == NULL)
	{
		return;
	}

	SourceDPG = Component->Template->SourceDPG;
	ReflectionsDPG = Component->Template->ReflectionsDPG;
	bRenderDebug = Component->Template->bRenderDebugLines;

	StaticDepthPriorityGroup = SourceDPG;

	ScreenPercentageMap = &(Component->Template->ScreenPercentageMap);

	static const FLOAT BoundsOffset = 1.0f;
	static const FLOAT BoundsScale = 1.1f;
	static const FLOAT BoundsScaledOffset = BoundsOffset * BoundsScale;
	static const FVector BoundsScaledOffsetVector = FVector(1,1,1) * BoundsScaledOffset;
	OcclusionBounds = FBoxSphereBounds(
		Component->Bounds.Origin,
		Component->Bounds.BoxExtent * BoundsScale + BoundsScaledOffsetVector,
		Component->Bounds.SphereRadius * BoundsScale + BoundsScaledOffset
		);

	// Make a local copy of the template elements...
	DynamicData = new FLensFlareDynamicData(Component, this);
	check(DynamicData);
	if (DynamicData)
	{
		DynamicData->InitializeRenderResources(NULL, this);
	}
}

FLensFlareSceneProxy::~FLensFlareSceneProxy()
{
	if (DynamicData)
	{
		check(IsInRenderingThread());
		DynamicData->RenderThread_ReleaseRenderResources();
	}
	delete DynamicData;
	DynamicData = NULL;
}

/**
 * Draws the primitive's static elements.  This is called from the game thread once when the scene proxy is created.
 * The static elements will only be rendered if GetViewRelevance declares static relevance.
 * Called in the game thread.
 * @param PDI - The interface which receives the primitive elements.
 */
void FLensFlareSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{

}

/** 
 * Draw the scene proxy as a dynamic element
 *
 * @param	PDI - draw interface to render to
 * @param	View - current view
 * @param	DPGIndex - current depth priority 
 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
 */
void FLensFlareSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	if ((bIsActive == TRUE) && (View->Family->ShowFlags & SHOW_LensFlares))
	{
		if (DynamicData != NULL)
		{
#if WITH_REALD
			//If RealD stereo is enabled we only want update occlusion for the first view
			if (!RealD::IsStereoEnabled() || (View && (View->X == 0.0)))
#endif
			{
				// Update the occlusion data every frame
				if (UpdateAndRenderOcclusionData(PDI, View, DPGIndex, Flags) == FALSE)
				{
					return;
				}
			}

			// Check if the flare is relevant, and if so, render it.
			if (CheckViewStatus(View) == FALSE)
			{
				return;
			}

#if WITH_MOBILE_RHI
			if (GUsingMobileRHI)
			{
				//don't waste the draws when the lens flare is disabled
				if (MobileOcclusionPercentage <= 0.0f)
				{
					return;
				}
			}
#endif

			// Set the occlusion value
			if (DPGIndex == SourceDPG)
			{
				// Render the source in the dynamic data
				DynamicData->RenderSource(this, PDI, View, DPGIndex, Flags);
			}
			
			if (DPGIndex == ReflectionsDPG)
			{

				// Render each reflection in the dynamic data
				DynamicData->RenderReflections(this, PDI, View, DPGIndex, Flags);
			}
		}

		RenderBounds(PDI, DPGIndex, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, !Owner || Owner->IsSelected());
	}
}

FPrimitiveViewRelevance FLensFlareSceneProxy::GetViewRelevance(const FSceneView* View)
{
	FPrimitiveViewRelevance Result;

	const EShowFlags ShowFlags = View->Family->ShowFlags;
	if (IsShown(View) && (ShowFlags & SHOW_LensFlares))
	{
		Result.bDynamicRelevance = TRUE;
		Result.bStaticRelevance = FALSE;

		Result.SetDPG(SourceDPG, TRUE);
		Result.SetDPG(ReflectionsDPG, TRUE);

		if (!(View->Family->ShowFlags & SHOW_Wireframe) && (View->Family->ShowFlags & SHOW_Materials))
		{
			Result.bTranslucentRelevance = bHasUnlitTranslucency || bHasLitTranslucency;
			Result.bDistortionRelevance = bHasDistortion;
			Result.bUsesSceneColor = bUsesSceneColor;
		}
		SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
		if (View->Family->ShowFlags & SHOW_Bounds)
		{
			Result.bOpaqueRelevance = TRUE;
		}

		Result.bSeparateTranslucencyRelevance = bHasSeparateTranslucency;
	}

	// Lens flares never cast shadows...
	Result.bShadowRelevance = FALSE;

	return Result;
}

/**
 *	Set the lens flare active or not...
 *	@param	bInIsActive		The active state to set the LF to
 */
void FLensFlareSceneProxy::SetIsActive(UBOOL bInIsActive)
{
	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		LensFlareSetIsActiveCommand,
		FLensFlareSceneProxy*, SceneProxy, this,
		UBOOL, bInIsActive, bInIsActive,
	{
		SceneProxy->SetIsActive_RenderThread(bInIsActive);
	}
	);
}

void FLensFlareSceneProxy::ChangeMobileOcclusionPercentage(const FLOAT DeltaPercent)
{
	// queue a call to update this data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		LensFlareChangeMobileOcclusionPercentage,
		FLensFlareSceneProxy*, SceneProxy, this,
		FLOAT, DeltaPercent, DeltaPercent,
	{
		SceneProxy->MobileOcclusionPercentage += DeltaPercent;
		SceneProxy->MobileOcclusionPercentage = Clamp<FLOAT>(SceneProxy->MobileOcclusionPercentage, 0.0, 1.0f);
	}
	);
}


/**
 *	Render-thread side of the SetIsActive function
 *
 *	@param	bInIsActive		The active state to set the LF to
 */
void FLensFlareSceneProxy::SetIsActive_RenderThread(UBOOL bInIsActive)
{
	bIsActive = bInIsActive;
}

/** 
 *	Get the results of the last frames occlusion and kick off the one for the next frame
 *
 *	@param	PDI - draw interface to render to
 *	@param	View - current view
 *	@param	DPGIndex - current depth priority 
 *	@param	Flags - optional set of flags from EDrawDynamicElementFlags
 */
UBOOL FLensFlareSceneProxy::UpdateAndRenderOcclusionData(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{
	FSceneViewState* State = (FSceneViewState*)(View->State);
	if (State != NULL)
	{
		FCoverageInfo* Coverage = CoverageMap.Find(State);
		if (Coverage == NULL)
		{
			FCoverageInfo TempInfo;
			CoverageMap.Set(State, TempInfo);
			Coverage = CoverageMap.Find(State);
		}
		check(Coverage);
		CoveragePercentage = Coverage->UnmappedPercentage;
		Coverage->Percentage = Coverage->UnmappedPercentage;
		FLOAT PreviousValue = CoveragePercentage;
		if (FPrimitiveSceneProxyOcclusionTracker::UpdateAndRenderOcclusionData(PrimitiveSceneInfo->Component, PDI, View, DPGIndex, Flags) == TRUE)
		{
#if WITH_MOBILE_RHI
			if (GUsingMobileRHI)
			{
				MobileOcclusionPercentage = Min(MobileOcclusionPercentage, 1.0f - RHIGetMobilePercentColorFade());
				CoveragePercentage = MobileOcclusionPercentage;
				Coverage->Percentage = MobileOcclusionPercentage;
				//Ignore user mapping for mobile
				Coverage->UnmappedPercentage = CoveragePercentage;
			}
			else
#endif
			{
				// Map it using the user-provided percentage mapping
				Coverage->UnmappedPercentage = Coverage->Percentage;
				if (ScreenPercentageMap)
				{
					CoveragePercentage = ScreenPercentageMap->GetValue(CoveragePercentage);
					Coverage->Percentage = CoveragePercentage;
				}
			}

//			if (GIsGame == TRUE)
//			{
//				AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
//				if (WorldInfo)
//				{
//					FString ErrorMessage = 
//						FString::Printf(TEXT("LensFlare: %uI64: %uI64: %5.2f, %5.2f %5.2f %5.2f"), 
//						QWORD(PTRINT(this)), QWORD(PTRINT(State)),
//						PreviousValue, CoveragePercentage, Coverage->Percentage, Coverage->UnmappedPercentage);
//					FColor ErrorColor(255,0,0);
//					QWORD MsgID = (QWORD)((PTRINT)this) + (QWORD)((PTRINT)State);
//					WorldInfo->AddOnScreenDebugMessage(MsgID, 5.0f, ErrorColor,ErrorMessage);
//				}
//			}

#if WITH_REALD
			//cache off the occlusion percentage for use in both views
			StereoCoveragePercentage = CoveragePercentage;
#endif

			return TRUE;
		}
	}

	return FALSE;
}

/** 
 *	Check if the flare is relevant for this view
 *
 *	@param	View - current view
 */
UBOOL FLensFlareSceneProxy::CheckViewStatus(const FSceneView* View)
{
	UBOOL bResult = TRUE;

	// Determine the line from the world to the camera
	FVector	LFToView = LocalToWorld.GetOrigin() - View->ViewOrigin;
	// Get the lens flare facing direction and the camera facing direction (in world-space)
	FVector ViewDirection = View->ViewMatrix.Inverse().TransformNormal(FVector(0.0f, 0.0f, 1.0f));
	// Normalize them just in case
	ViewDirection.Normalize();
	// Normalize the LF to View directional vector
	FVector LFToViewVector = LFToView;
	LFToViewVector.Normalize();

	// Calculate the dot product for both the source and the camera
	// If it is behind the view, don't draw it!
	FLOAT CameraDot = ViewDirection | LFToViewVector;
	if (CameraDot <= 0.0f)
	{
		return FALSE;
	}

	if ((OuterCone != 0.0f) || (Radius != 0.0f))
	{
		UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );
		if (bIsPerspective == FALSE)
		{
			ConeStrength = 1.0f;
			return TRUE;
		}

		// Quick radius check... see if within range
		if (Radius != 0.0f)
		{
			FLOAT Distance = LFToView.Size();
			if (Distance > Radius)
			{
				ConeStrength = 0.0f;
				return FALSE;
			}
			else
			{
				ConeStrength = 1.0f;
			}
		}

		// More complex checks for cone
		if (OuterCone != 0.0f)
		{
			// Get the lens flare facing direction and the camera facing direction (in world-space)
			FVector LFDirection = LocalToWorld.GetAxis(0);

			// Normalize them just in case
			LFDirection.Normalize();

			// Calculate the dot product for both the source and the camera
			FLOAT SourceDot = LFDirection | -LFToViewVector;

			FVector SourceCross = LFDirection ^ -LFToViewVector;
			FVector CameraCross = ViewDirection ^ LFToViewVector;
			UBOOL bSourceAngleNegative = (SourceCross.Z < 0.0f) ? TRUE : FALSE;
			UBOOL bCameraAngleNegative = (CameraCross.Z < 0.0f) ? TRUE : FALSE;

			// Determine the angle between them
			FLOAT ViewAngle = ViewDirection | LFDirection;

			FLOAT SourceAngleDeg = appAcos(SourceDot) * 180.0f / PI;
			FLOAT CameraAngleDeg = appAcos(CameraDot) * 180.0f / PI;
			FLOAT TotalAngle = 
				(bSourceAngleNegative ? -SourceAngleDeg : SourceAngleDeg) + 
				(bCameraAngleNegative ? -CameraAngleDeg : CameraAngleDeg);

			if (bUseTrueConeCalculation == true)
			{
				if (Abs(TotalAngle) <= InnerCone)
				{
					ConeStrength = 1.0f;
					bResult = TRUE;
				}
				else
				{
					if (Abs(TotalAngle) <= OuterCone)
					{
						ConeStrength = (1.0f - (((1.0f-MinStrength)*((Abs(TotalAngle) - InnerCone) / (OuterCone - InnerCone))+MinStrength)))+MinStrength;
					}
					else
					{
						// Outside the cone!
						ConeStrength = MinStrength;
						bResult = MinStrength != 0.0f;
					}
				}
			}
			else
			{
				if (Abs(SourceAngleDeg) > 90.0f)
				{
					// Behind the source... don't bother rendering.
					ConeStrength = 0.0f;
					bResult = FALSE;
				}

				FLOAT ClampedInnerConeAngle = Clamp(InnerCone,0.0f,89.99f);
				FLOAT ClampedOuterConeAngle = Clamp(OuterCone,ClampedInnerConeAngle + 0.001f,89.99f + 0.001f);

				FLOAT FudgedTotalAngle = TotalAngle * ConeFudgeFactor;

				if (Abs(FudgedTotalAngle) <= ClampedInnerConeAngle)
				{
					ConeStrength = 1.0f;
				}
				else
				if (Abs(FudgedTotalAngle) <= ClampedOuterConeAngle)
				{
					ConeStrength = 1.0f - (Abs(FudgedTotalAngle) - ClampedInnerConeAngle) / (ClampedOuterConeAngle - ClampedInnerConeAngle);
				}
				else
				{
					// Outside the cone!
					ConeStrength = 0.0f;
					bResult = FALSE;
				}
			}
		}
	}
	else
	{
		// No cone active
		ConeStrength = 1.0f;
	}

	return bResult;
}
