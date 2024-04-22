/**
 *	LensFlare.cpp: LensFlare implementations.
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "LensFlare.h"

IMPLEMENT_CLASS(ULensFlare);
IMPLEMENT_CLASS(ULensFlareComponent);
IMPLEMENT_CLASS(ALensFlareSource);

/**
 *	LensFlare class
 */
/**
 *	Retrieve the curve objects associated with this element.
 *
 *	@param	OutCurves	The array to fill in with the curve pairs.
 */
void FLensFlareElement::GetCurveObjects(TArray<FLensFlareElementCurvePair>& OutCurves)
{
	FLensFlareElementCurvePair* NewCurve;

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = LFMaterialIndex.Distribution;
	NewCurve->CurveName = FString(TEXT("LFMaterialIndex"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Scaling.Distribution;
	NewCurve->CurveName = FString(TEXT("Scaling"));
	
	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = AxisScaling.Distribution;
	NewCurve->CurveName = FString(TEXT("AxisScaling"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Rotation.Distribution;
	NewCurve->CurveName = FString(TEXT("Rotation"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Color.Distribution;
	NewCurve->CurveName = FString(TEXT("Color"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Alpha.Distribution;
	NewCurve->CurveName = FString(TEXT("Alpha"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = Offset.Distribution;
	NewCurve->CurveName = FString(TEXT("Offset"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = DistMap_Scale.Distribution;
	NewCurve->CurveName = FString(TEXT("DistMap_Scale"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = DistMap_Color.Distribution;
	NewCurve->CurveName = FString(TEXT("DistMap_Color"));

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = DistMap_Alpha.Distribution;
	NewCurve->CurveName = FString(TEXT("DistMap_Alpha"));
}

void FLensFlareElement::DuplicateDistribution_Float(const FRawDistributionFloat& SourceDist, UObject* Outer, FRawDistributionFloat& NewDist)
{
	NewDist = SourceDist;
	if (SourceDist.Distribution != NULL)
	{
		NewDist.Distribution = Cast<UDistributionFloat>(UObject::StaticDuplicateObject(
			SourceDist.Distribution, SourceDist.Distribution, Outer, TEXT("None")));
		NewDist.Distribution->bIsDirty = TRUE;
	}
}

void FLensFlareElement::DuplicateDistribution_Vector(const FRawDistributionVector& SourceDist, UObject* Outer, FRawDistributionVector& NewDist)
{
	NewDist = SourceDist;
	if (SourceDist.Distribution != NULL)
	{
		NewDist.Distribution = Cast<UDistributionVector>(UObject::StaticDuplicateObject(
			SourceDist.Distribution, SourceDist.Distribution, Outer, TEXT("None")));
		NewDist.Distribution->bIsDirty = TRUE;
	}
}

UBOOL FLensFlareElement::DuplicateFromSource(const FLensFlareElement& InSource, UObject* Outer)
{
	ElementName = InSource.ElementName;
	RayDistance =  InSource.RayDistance;

	bIsEnabled = InSource.bIsEnabled;
	bUseSourceDistance = InSource.bUseSourceDistance;
	bNormalizeRadialDistance = InSource.bNormalizeRadialDistance;
	bModulateColorBySource = InSource.bModulateColorBySource;
	Size = InSource.Size;

	bOrientTowardsSource = InSource.bOrientTowardsSource;
	
	LFMaterials.Empty();
	for (INT MatIndex = 0; MatIndex < InSource.LFMaterials.Num(); MatIndex++)
	{
		LFMaterials.AddItem(InSource.LFMaterials(MatIndex));
	}

	// We need to duplicate each distribution...
	DuplicateDistribution_Float(InSource.LFMaterialIndex, Outer, LFMaterialIndex);
	DuplicateDistribution_Float(InSource.Scaling, Outer, Scaling);
	DuplicateDistribution_Vector(InSource.AxisScaling, Outer, AxisScaling);
	DuplicateDistribution_Float(InSource.Rotation, Outer, Rotation);
	DuplicateDistribution_Vector(InSource.Color, Outer, Color);
	DuplicateDistribution_Float(InSource.Alpha, Outer, Alpha);
	DuplicateDistribution_Vector(InSource.Offset, Outer, Offset);
	DuplicateDistribution_Vector(InSource.DistMap_Scale, Outer, DistMap_Scale);
	DuplicateDistribution_Vector(InSource.DistMap_Color, Outer, DistMap_Color);
	DuplicateDistribution_Float(InSource.DistMap_Alpha, Outer, DistMap_Alpha);

	return TRUE;
}

/** 
 *	Retrieve the curve of the given name...
 *
 *	@param	CurveName		The name of the curve to retrieve.
 *
 *	@return	UObject*		The curve, if found; NULL if not.
 */
UObject* FLensFlareElement::GetCurve(FString& CurveName)
{
	if (CurveName != TEXT("ScreenPercentageMap"))
	{
		TArray<FLensFlareElementCurvePair> OutCurves;
		GetCurveObjects(OutCurves);

		for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
		{
			if (OutCurves(CurveIndex).CurveName == CurveName)
			{
				// We found it... return it
				return OutCurves(CurveIndex).CurveObject;
			}
		}
	}
	return NULL;
}

// UObject interface.
/**
 *	Called when a property is about to change in the property window.
 *
 *	@param	PropertyAboutToChange	The property that is about to be modified.
 */
void ULensFlare::PreEditChange(UProperty* PropertyAboutToChange)
{
	ReflectionCount = Reflections.Num();
}

/**
 *	Called when a property has been changed in the property window.
 *
 *	@param	PropertyThatChanged		The property that was modified.
 */
void ULensFlare::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("Reflections")) != NULL)
		{
			INT NewReflectionCount = Reflections.Num();
			if (NewReflectionCount > ReflectionCount)
			{
				// Initialize the new element...
				// It's not guaranteed that the element is at the end though, so find the 'empty' element
				for (INT ElementIndex = 0; ElementIndex < Reflections.Num(); ElementIndex++)
				{
					InitializeElement(ElementIndex);
				}
			}
		}
		else
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("RayDistance")) != NULL)
		{
			// You are NOT allowed to change the ray distance of the source
			if (1)
			{

			}
		}
		else if ((PropertyThatChanged->GetName() == TEXT("OutterCone")) ||
				 (PropertyThatChanged->GetName() == TEXT("InnerCone")) ||
				 (PropertyThatChanged->GetName() == TEXT("Radius")))
		{
			// Update the vizualization components
			for (TObjectIterator<ULensFlareComponent> It; It; ++It)
			{
				ULensFlareComponent* LensFlareComponent = *It;
				if (LensFlareComponent->Template == this)
				{
					LensFlareComponent->InitializeVisualizationData(TRUE);
					if (LensFlareComponent->PreviewOuterCone)
					{
						LensFlareComponent->PreviewOuterCone->bNeedsReattach = TRUE;
					}
					if (LensFlareComponent->PreviewInnerCone)
					{
						LensFlareComponent->PreviewInnerCone->bNeedsReattach = TRUE;
					}
					if (LensFlareComponent->PreviewRadius)
					{
						LensFlareComponent->PreviewRadius->bNeedsReattach = TRUE;
					}
					
					ALensFlareSource* LensFlareActor = Cast<ALensFlareSource>(LensFlareComponent->GetOuter());
					if (LensFlareActor)
					{
						LensFlareActor->ConditionalUpdateComponents();
					}
				}
			}
		}

		MarkPackageDirty();
	}

	for (TObjectIterator<ALensFlareSource> It;It;++It)
	{
		if (It->LensFlareComp && (It->LensFlareComp->Template == this))
		{
			It->ForceUpdateComponents();
		}
	}
}

/**
 *	Called when an object has been loaded...
 */
void ULensFlare::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	ULinkerLoad* LFLinkerLoad = GetLinker();
	if (LFLinkerLoad && (LFLinkerLoad->Ver() < VER_LENSFLARE_SCREENPERCENTAGEMAP_FIXUP))
	{
		// Convert the ScreenPercentageMap to a linear 0..1,0..1 mapping
		UDistributionFloatConstantCurve* NewCurve = ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), this);
		check(NewCurve);
		INT KeyIndex;
		KeyIndex = NewCurve->CreateNewKey(0.0f);
		NewCurve->SetKeyOut(0, KeyIndex, 0.0f);
		NewCurve->SetKeyInterpMode(KeyIndex, CIM_Linear);
		KeyIndex = NewCurve->CreateNewKey(1.0f);
		NewCurve->SetKeyOut(0, KeyIndex, 1.0f);
		NewCurve->SetKeyInterpMode(KeyIndex, CIM_Linear);

		ScreenPercentageMap.Distribution = NewCurve;
	}

	if (GIsEditor)
	{
		UDistributionFloatConstantCurve* CheckCurve = Cast<UDistributionFloatConstantCurve>(ScreenPercentageMap.Distribution);
		if (CheckCurve != NULL)
		{
			if (CheckCurve->GetNumKeys() == 0)
			{
				warnf(NAME_Warning, TEXT("Replacing invalid ScreenPercentageMap on %s"), *GetPathName());
				// Convert the ScreenPercentageMap to a linear 0..1,0..1 mapping
				UDistributionFloatConstantCurve* NewCurve = ConstructObject<UDistributionFloatConstantCurve>(UDistributionFloatConstantCurve::StaticClass(), this);
				check(NewCurve);
				INT KeyIndex;
				KeyIndex = NewCurve->CreateNewKey(0.0f);
				NewCurve->SetKeyOut(0, KeyIndex, 0.0f);
				NewCurve->SetKeyInterpMode(KeyIndex, CIM_Linear);
				KeyIndex = NewCurve->CreateNewKey(1.0f);
				NewCurve->SetKeyOut(0, KeyIndex, 1.0f);
				NewCurve->SetKeyInterpMode(KeyIndex, CIM_Linear);

				ScreenPercentageMap.Distribution = NewCurve;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	//Forcing source into world DPG
	SourceDPG = SDPG_World;
}

// CurveEditor helper interface
/**
 *	Add the curves for the given element to the curve editor.
 *
 *	@param	ElementIndex	The index of the element whose curves should be sent.
 *							-1 indicates the source element.
 *	@param	EdSetup			The curve ed setup for the object.
 */
void ULensFlare::AddElementCurvesToEditor(INT ElementIndex, UInterpCurveEdSetup* EdSetup)
{
	FLensFlareElement* LFElement = NULL;
	if (ElementIndex == -1)
	{
		LFElement = &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		LFElement = &(Reflections(ElementIndex));
	}

	if (LFElement == NULL)
	{
		return;
	}

	TArray<FLensFlareElementCurvePair> OutCurves;

	LFElement->GetCurveObjects(OutCurves);

	for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
	{
		UObject* Distribution = OutCurves(CurveIndex).CurveObject;
		if (Distribution)
		{
			EdSetup->AddCurveToCurrentTab(Distribution, OutCurves(CurveIndex).CurveName, FColor(255,0,0), TRUE, TRUE);
		}
	}
}

/**
 *	Removes the curves for the given element from the curve editor.
 *
 *	@param	ElementIndex	The index of the element whose curves should be sent.
 *							-1 indicates the source element.
 *	@param	EdSetup			The curve ed setup for the object.
 */
void ULensFlare::RemoveElementCurvesFromEditor(INT ElementIndex, UInterpCurveEdSetup* EdSetup)
{
	FLensFlareElement* LFElement = NULL;
	if (ElementIndex == -1)
	{
		LFElement = &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		LFElement = &(Reflections(ElementIndex));
	}

	if (LFElement == NULL)
	{
		return;
	}

	TArray<FLensFlareElementCurvePair> OutCurves;

	LFElement->GetCurveObjects(OutCurves);

	for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
	{
		UObject* Distribution = OutCurves(CurveIndex).CurveObject;
		if (Distribution)
		{
			EdSetup->RemoveCurve(Distribution);
		}
	}
}

/**
 *	Add the curve of the given name from the given element to the curve editor.
 *
 *	@param	ElementIndex	The index of the element of interest.
 *							-1 indicates the source element.
 *	@param	CurveName		The name of the curve to add
 *	@param	EdSetup			The curve ed setup for the object.
 */
void ULensFlare::AddElementCurveToEditor(INT ElementIndex, FString& CurveName, UInterpCurveEdSetup* EdSetup)
{
	FLensFlareElement* LFElement = NULL;
	if (ElementIndex == -1)
	{
		LFElement = &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		LFElement = &(Reflections(ElementIndex));
	}

	if ((LFElement == NULL) && (CurveName != TEXT("ScreenPercentageMap")))
	{
		return;
	}

	TArray<FLensFlareElementCurvePair> OutCurves;

	if (CurveName == TEXT("ScreenPercentageMap"))
	{
		GetCurveObjects(OutCurves);
	}
	else
	{
		LFElement->GetCurveObjects(OutCurves);
	}

	for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
	{
		if (OutCurves(CurveIndex).CurveName == CurveName)
		{
			// We found it... send it to the curve editor
			UObject* Distribution = OutCurves(CurveIndex).CurveObject;
			if (Distribution)
			{
				EdSetup->AddCurveToCurrentTab(Distribution, OutCurves(CurveIndex).CurveName, FColor(255,0,0), TRUE, TRUE);
			}
		}
	}
}

/**
 *	Retrieve the curve of the given name from the given element.
 *
 *	@param	ElementIndex	The index of the element of interest.
 *							-1 indicates the source element.
 *	@param	CurveName		The name of the curve to add
 *	@param	EdSetup			The curve ed setup for the object.
 */
UObject* ULensFlare::GetElementCurve(INT ElementIndex, FString& CurveName)
{
	FLensFlareElement* LFElement = NULL;
	if (ElementIndex == -1)
	{
		LFElement = &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		LFElement = &(Reflections(ElementIndex));
	}

	if ((LFElement == NULL) && (CurveName != TEXT("ScreenPercentageMap")))
	{
		return NULL;
	}

	TArray<FLensFlareElementCurvePair> OutCurves;

	if (CurveName == TEXT("ScreenPercentageMap"))
	{
		GetCurveObjects(OutCurves);
	}
	else
	{
		LFElement->GetCurveObjects(OutCurves);
	}

	for (INT CurveIndex = 0; CurveIndex < OutCurves.Num(); CurveIndex++)
	{
		if (OutCurves(CurveIndex).CurveName == CurveName)
		{
			// We found it... return it
			return OutCurves(CurveIndex).CurveObject;
		}
	}

	return NULL;
}

/**
 *	Retrieve the element at the given index
 *
 *	@param	ElementIndex		The index of the element of interest.
 *								-1 indicates the Source element.
 *	
 *	@return	FLensFlareElement*	Pointer to the element if found.
 *								Otherwise, NULL.
 */
const FLensFlareElement* ULensFlare::GetElement(INT ElementIndex) const
{
	if (ElementIndex == -1)
	{
		return &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		return &Reflections(ElementIndex);
	}

	return NULL;
}

/**
 *	Set the given element to the given enabled state.
 *
 *	@param	ElementIndex	The index of the element of interest.
 *							-1 indicates the Source element.
 *	@param	bInIsEnabled	The enabled state to set it to.
 *
 *	@return UBOOL			TRUE if element was found and bIsEnabled was set to given value.
 */
UBOOL ULensFlare::SetElementEnabled(INT ElementIndex, UBOOL bInIsEnabled)
{
	FLensFlareElement* LFElement = NULL;
	if (ElementIndex == -1)
	{
		LFElement = &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		LFElement = &(Reflections(ElementIndex));
	}

	if (LFElement == NULL)
	{
		return FALSE;
	}

	LFElement->bIsEnabled = bInIsEnabled;
	return TRUE;
}

/** 
 *	Initialize the element at the given index
 *
 *	@param	ElementIndex	The index of the element of interest.
 *							-1 indicates the Source element.
 *
 *	@return	UBOOL			TRUE if successful. FALSE otherwise.
 */
UBOOL ULensFlare::InitializeElement(INT ElementIndex)
{
	FLensFlareElement* LFElement = NULL;
	if (ElementIndex == -1)
	{
		LFElement = &SourceElement;
	}
	else
	if ((ElementIndex >= 0) && (ElementIndex < Reflections.Num()))
	{
		LFElement = &(Reflections(ElementIndex));
	}

	if (LFElement == NULL)
	{
		return FALSE;
	}

	LFElement->bNormalizeRadialDistance = TRUE;

	LFElement->bIsEnabled = TRUE;

	/** Size */
	LFElement->Size = FVector(0.2f, 0.2f, 0.0f);

	UDistributionFloatConstant* DFConst;
	UDistributionVectorConstant* DVConst;

	/** Index of the material to use from the LFMaterial array. */
	LFElement->LFMaterialIndex.Distribution = ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), this);
	DFConst = CastChecked<UDistributionFloatConstant>(LFElement->LFMaterialIndex.Distribution);
	DFConst->Constant = 0.0f;
		 
	/**	Global scaling.	 */
	LFElement->Scaling.Distribution = ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), this);
	DFConst = CastChecked<UDistributionFloatConstant>(LFElement->Scaling.Distribution);
	DFConst->Constant = 1.0f;
	
	/**	Anamorphic scaling.	*/
	LFElement->AxisScaling.Distribution = ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), this);
	DVConst = CastChecked<UDistributionVectorConstant>(LFElement->AxisScaling.Distribution);
	DVConst->Constant = FVector(1.0f);
	
	/** Rotation. */
	LFElement->Rotation.Distribution = ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), this);
	DFConst = CastChecked<UDistributionFloatConstant>(LFElement->Rotation.Distribution);
	DFConst->Constant = 0.0f;

	/** Color. */
	LFElement->Color.Distribution = ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), this);
	DVConst = CastChecked<UDistributionVectorConstant>(LFElement->Color.Distribution);
	DVConst->Constant = FVector(1.0f);
	LFElement->Alpha.Distribution = ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), this);
	DFConst = CastChecked<UDistributionFloatConstant>(LFElement->Alpha.Distribution);
	DFConst->Constant = 1.0f;

	/** Offset. */
	LFElement->Offset.Distribution = ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), this);
	DVConst = CastChecked<UDistributionVectorConstant>(LFElement->Offset.Distribution);
	DVConst->Constant = FVector(0.0f);

	/** Distance to camera scaling. */
	LFElement->DistMap_Scale.Distribution = ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), this);
	DVConst = CastChecked<UDistributionVectorConstant>(LFElement->DistMap_Scale.Distribution);
	DVConst->Constant = FVector(1.0f);
	LFElement->DistMap_Color.Distribution = ConstructObject<UDistributionVectorConstant>(UDistributionVectorConstant::StaticClass(), this);
	DVConst = CastChecked<UDistributionVectorConstant>(LFElement->DistMap_Color.Distribution);
	DVConst->Constant = FVector(1.0f);
	LFElement->DistMap_Alpha.Distribution = ConstructObject<UDistributionFloatConstant>(UDistributionFloatConstant::StaticClass(), this);
	DFConst = CastChecked<UDistributionFloatConstant>(LFElement->DistMap_Alpha.Distribution);
	DFConst->Constant = 1.0f;

	return TRUE;
}

/** Get the curve objects associated with the LensFlare itself */
void ULensFlare::GetCurveObjects(TArray<FLensFlareElementCurvePair>& OutCurves)
{
	FLensFlareElementCurvePair* NewCurve;

	NewCurve = new(OutCurves)FLensFlareElementCurvePair;
	check(NewCurve);
	NewCurve->CurveObject = ScreenPercentageMap.Distribution;
	NewCurve->CurveName = FString(TEXT("ScreenPercentageMap"));
}

/**
 *	LensFlareComponent class
 */
void ULensFlareComponent::SetSourceColor(FLinearColor InSourceColor)
{
	if (SourceColor != InSourceColor)
	{
		SourceColor = InSourceColor;
		// Need to detach/re-attach
		BeginDeferredReattach();
	}
}

void ULensFlareComponent::SetTemplate(class ULensFlare* NewTemplate, UBOOL bForceSet)
{
	if ((NewTemplate != Template) || bForceSet)
	{
		Template = NewTemplate;
		if (Template)
		{
			OuterCone = Template->OuterCone;
			InnerCone = Template->InnerCone;
			ConeFudgeFactor = Template->ConeFudgeFactor;
			Radius = Template->Radius;
			bUseTrueConeCalculation = Template->bUseTrueConeCalculation;
			MinStrength = Template->MinStrength;
			if (bAutoActivate == TRUE)
			{
				bIsActive = TRUE;
			}
		}

		SetupMaterialsArray(TRUE);

		// Need to detach/re-attach
		BeginDeferredReattach();
	}
}

void ULensFlareComponent::SetIsActive(UBOOL bInIsActive)
{
	if (bIsActive != bInIsActive)
	{
		bIsActive = bInIsActive;
		FLensFlareSceneProxy* SceneProxy = (FLensFlareSceneProxy*)Scene_GetProxyFromInfo(SceneInfo);
		if (SceneProxy)
		{
			SceneProxy->SetIsActive(bIsActive);
		}
	}
}

// UObject interface
void ULensFlareComponent::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseResourcesFence = new FRenderCommandFence();
	check(ReleaseResourcesFence);
	ReleaseResourcesFence->BeginFence();
}

UBOOL ULensFlareComponent::IsReadyForFinishDestroy()
{
	UBOOL bSuperIsReady = Super::IsReadyForFinishDestroy();

	UBOOL bIsReady = TRUE;
	if (ReleaseResourcesFence)
	{
		bIsReady = (ReleaseResourcesFence->GetNumPendingFences() == 0);
	}
	return (bSuperIsReady && bIsReady);
}

void ULensFlareComponent::FinishDestroy()
{
	if (ReleaseResourcesFence)
	{
		delete ReleaseResourcesFence;
		ReleaseResourcesFence = FALSE;
	}
	Super::FinishDestroy();
}

void ULensFlareComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
}

void ULensFlareComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SetupMaterialsArray(TRUE);
	BeginDeferredReattach();
}

void ULensFlareComponent::PostLoad()
{
	// initialize to force immediate test
	NextTraceTime = -1.0f * appFrand();
	Super::PostLoad();
}

// UActorComponent interface.
void ULensFlareComponent::Attach()
{
	SetupMaterialsArray(FALSE);

	Super::Attach();

	InitializeVisualizationData(FALSE);
}

// UPrimitiveComponent interface
void ULensFlareComponent::UpdateBounds()
{
	if (Template && (Template->bUseFixedRelativeBoundingBox == TRUE))
	{
		// Use hardcoded relative bounding box from template.
		Template->FixedRelativeBoundingBox.IsValid = TRUE;
		Bounds	= FBoxSphereBounds(Template->FixedRelativeBoundingBox.TransformBy(LocalToWorld));
	}
	else
	{
		Super::UpdateBounds();
	}

	// Send the results over to the render thread!
	FLensFlareSceneProxy* SceneProxy = (FLensFlareSceneProxy*)Scene_GetProxyFromInfo(SceneInfo);
	if (SceneProxy)
	{
		SceneProxy->UpdateOcclusionBounds(Bounds);
	}
}

void ULensFlareComponent::Tick(FLOAT DeltaTime)
{
	Super::Tick(DeltaTime);

#if WITH_MOBILE_RHI	
	if( GUsingMobileRHI && GSystemSettings.bAllowLensFlares && SceneInfo )
	{
		// Handle large quantums
		NextTraceTime -= Min( 0.1f, DeltaTime );

		if ( NextTraceTime < 0.f)
		{
			UBOOL bWasOccluded = FALSE;
			bVisibleForMobile = TRUE;
			AWorldInfo* Info = GWorld->GetWorldInfo();
			if (Info)
			{
				for (AController* Controller = Info->ControllerList; Controller != NULL; Controller = Controller->NextController)
				{
					APlayerController* PlayerController = Cast<APlayerController>(Controller);
					if (PlayerController && PlayerController->IsLocalPlayerController())
					{
						FVector POVLoc;
						FRotator POVRotation;
						PlayerController->eventGetPlayerViewPoint( POVLoc, POVRotation );
						FVector POVDir = POVRotation.Vector();

						AActor* LensFlareActor = GetOwner();
						check(LensFlareActor);

						FVector POVToLensFlareDir = LensFlareActor->Location - POVLoc;
						POVToLensFlareDir.Normalize();

						if ( (POVDir | POVToLensFlareDir ) < 0.7f )
						{
							// not visible because behind or out of FOV of player
							bVisibleForMobile = FALSE;
						}
						else
						{
							const FVector BoundsOffset = -POVToLensFlareDir * Bounds.SphereRadius;

							//Make sure there is no actor between the camera and the flare
							FCheckResult Hit(1.f);
							GWorld->SingleLineCheck(Hit, NULL, LensFlareActor->Location + BoundsOffset, POVLoc, TRACE_World | TRACE_ComplexCollision | TRACE_StopAtAnyHit);
							bWasOccluded = Hit.Actor != NULL;
							bVisibleForMobile = !bWasOccluded;
						}

						//assuming only one player controller for mobile
						break;
					}
				}
			}

			if( bVisibleForMobile || !bWasOccluded )
			{
				// For flares that are already visible, or were frustum culled, trace again soon so they'll appear
				// quickly when the view rotation changes or when they become occluded
				NextTraceTime += 0.5f;
			}
			else
			{
				// For flares that are not visible, we can be a little bit late on making them visible, to save
				// having to do line checks frequently.
				NextTraceTime += 1.5f;
			}
		}

		//send the delta down to the render proxy
		FLensFlareSceneProxy* SceneProxy = (FLensFlareSceneProxy*)Scene_GetProxyFromInfo(SceneInfo);
		if (SceneProxy)
		{
			//using Delta Time * Fudge Constant, so it should fully transition over a second/Fudge Constant
			FLOAT OcclusionChangeSpeed = 2.5f;
			FLOAT DeltaOcclusion = (bVisibleForMobile ? DeltaTime : -DeltaTime)*OcclusionChangeSpeed;
			SceneProxy->ChangeMobileOcclusionPercentage(DeltaOcclusion);
		}
	}
#endif
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void ULensFlareComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	if( Template )
	{
		for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
		{
			const FLensFlareElementMaterials& InnerElement = Materials(ElementIdx);
			for (INT InnerIdx = 0; InnerIdx < InnerElement.ElementMaterials.Num(); InnerIdx++)
			{
				OutMaterials.AddItem(InnerElement.ElementMaterials(InnerIdx));
			}
		}
	}
}

/** 
 *	Setup the Materials array for the lens flare component.
 *	
 *	@param	bForceReset		If TRUE, reset the array and refill it from the template.
 */
void ULensFlareComponent::SetupMaterialsArray(UBOOL bForceReset)
{
	if (bForceReset == TRUE)
	{
		Materials.Empty();
	}

	if (Template && (Materials.Num() == 0))
	{
		for (INT ElementIdx = -1; ElementIdx < Template->Reflections.Num(); ElementIdx++)
		{
			const FLensFlareElement* Element = Template->GetElement(ElementIdx);
			INT ArrayIndex = ElementIdx + 1;
			INT NewIndex = Materials.AddZeroed();
			check(ArrayIndex == NewIndex);
			if ((Element->bIsEnabled == TRUE) && (Element->LFMaterials.Num() > 0))
			{
				FLensFlareElementMaterials& InnerMaterials = Materials(NewIndex);
				for (INT ElementMaterialIdx = 0; ElementMaterialIdx < Element->LFMaterials.Num(); ElementMaterialIdx++)
				{
					InnerMaterials.ElementMaterials.AddItem(Element->LFMaterials(ElementMaterialIdx));
				}
			}
		}
	}
}

INT ULensFlareComponent::GetNumElements() const
{
	// count up materials we're using from all sources
	INT Total = 0;
	if (Template != NULL)
	{
		if (Materials.Num() > 0)
		{
			for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
			{
				const FLensFlareElementMaterials& InnerMaterials = Materials(ElementIdx);
				Total += InnerMaterials.ElementMaterials.Num();
			}
		}
		else
		{
			const FLensFlareElement& Element = Template->SourceElement;
			if (Element.bIsEnabled)
			{
				Total += Element.LFMaterials.Num();
			}

			// Get the materials from each reflection element
			for (INT ElementIdx = 0; ElementIdx < Template->Reflections.Num(); ElementIdx++)
			{
				const FLensFlareElement& Element = Template->Reflections(ElementIdx);
				if (Element.bIsEnabled)
				{
					Total += Element.LFMaterials.Num();
				}
			}
		}
	}
	return Total;
}

UMaterialInterface* ULensFlareComponent::GetMaterial(INT ElementIndex)
{
	return GetElementMaterial(ElementIndex);
}

void ULensFlareComponent::SetMaterial(INT ElementIndex,UMaterialInterface* Material)
{
	SetElementMaterial(ElementIndex, Material);
}

UMaterialInterface* ULensFlareComponent::GetElementMaterial(INT MaterialIndex) const
{
	// match the passed in index to the complete list of materials we are using
	INT Count = INDEX_NONE;
	if (Template != NULL)
	{
		if (Materials.Num() > 0)
		{
			for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
			{
				const FLensFlareElementMaterials& InnerMaterials = Materials(ElementIdx);
				for (INT InnerIdx = 0; InnerIdx < InnerMaterials.ElementMaterials.Num(); InnerIdx++)
				{
					if (++Count == MaterialIndex)
					{
						return InnerMaterials.ElementMaterials(InnerIdx);
					}
				}
			}
		}
		else
		{
			const FLensFlareElement& Element = Template->SourceElement;
			if (Element.bIsEnabled)
			{
				for (INT MatIdx = 0; MatIdx < Element.LFMaterials.Num(); MatIdx++)
				{
					if (++Count == MaterialIndex)
					{
						return Element.LFMaterials(MatIdx);
					}
				}
			}

			for (INT ElementIdx = 0; ElementIdx < Template->Reflections.Num(); ElementIdx++)
			{
				const FLensFlareElement& Element = Template->Reflections(ElementIdx);
				if (Element.bIsEnabled)
				{
					for (INT MatIdx = 0; MatIdx < Element.LFMaterials.Num(); MatIdx++)
					{
						if (++Count == MaterialIndex)
						{
							return Element.LFMaterials(MatIdx);
						}
					}
				}
			}
		}
	}

	return NULL;
}

void ULensFlareComponent::SetElementMaterial(INT MaterialIndex, UMaterialInterface* InMaterial)
{
	UBOOL bNeedsReattach = FALSE;
	INT Count = INDEX_NONE;
	if (Template != NULL)
	{
		if (Materials.Num() > 0)
		{
			for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
			{
				FLensFlareElementMaterials& InnerMaterials = Materials(ElementIdx);
				for (INT InnerIdx = 0; InnerIdx < InnerMaterials.ElementMaterials.Num(); InnerIdx++)
				{
					if (++Count == MaterialIndex)
					{
						InnerMaterials.ElementMaterials(InnerIdx) = InMaterial;
						bNeedsReattach = TRUE;
						break;
					}
				}
			}
		}
		if (bNeedsReattach == TRUE)
		{
			BeginDeferredReattach();
		}
	}
}

/** Returns true if the prim is using a material with unlit distortion */
UBOOL ULensFlareComponent::HasUnlitDistortion() const
{
	if (Template == NULL)
	{
		return FALSE;
	}

	UBOOL bHasDistortion = FALSE;

	for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
	{
		const FLensFlareElementMaterials& InnerElement = Materials(ElementIdx);
		for (INT InnerIdx = 0; InnerIdx < InnerElement.ElementMaterials.Num(); InnerIdx++)
		{
			if (InnerElement.ElementMaterials(InnerIdx))
			{
				UMaterial* Material = InnerElement.ElementMaterials(InnerIdx)->GetMaterial();
				if (Material && (Material->HasDistortion() == TRUE))
				{
					bHasDistortion = TRUE;
					break;
				}
			}
		}
	}

	return bHasDistortion;
}

/** Returns true if the prim is using a material with unlit translucency */
UBOOL ULensFlareComponent::HasUnlitTranslucency() const
{
	if (Template == NULL)
	{
		return FALSE;
	}

	UBOOL bHasTranslucency = FALSE;
	for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
	{
		const FLensFlareElementMaterials& InnerElement = Materials(ElementIdx);
		for (INT InnerIdx = 0; InnerIdx < InnerElement.ElementMaterials.Num(); InnerIdx++)
		{
			if (InnerElement.ElementMaterials(InnerIdx))
			{
				UMaterial* Material = InnerElement.ElementMaterials(InnerIdx)->GetMaterial();
				if (Material && (Material->LightingModel == MLM_Unlit))
				{
					// check for unlit translucency
					if (IsTranslucentBlendMode((EBlendMode)Material->BlendMode))
					{
						bHasTranslucency = TRUE;
						break;
					}
				}
			}
		}
	}

	return bHasTranslucency;
}

/** Returns true if the prim is using a material with lit translucency */
UBOOL ULensFlareComponent::HasLitTranslucency() const
{
	if (Template == NULL)
	{
		return FALSE;
	}

	UBOOL bHasTranslucency = FALSE;
	for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
	{
		const FLensFlareElementMaterials& InnerElement = Materials(ElementIdx);
		for (INT InnerIdx = 0; InnerIdx < InnerElement.ElementMaterials.Num(); InnerIdx++)
		{
			if (InnerElement.ElementMaterials(InnerIdx))
			{
				UMaterial* Material = InnerElement.ElementMaterials(InnerIdx)->GetMaterial();
				if (Material && (Material->LightingModel == MLM_Phong))
				{
					// check for unlit translucency
					if (IsTranslucentBlendMode((EBlendMode)Material->BlendMode))
					{
						bHasTranslucency = TRUE;
						break;
					}
				}
			}
		}
	}

	return bHasTranslucency;
}


/** Returns true if the prim is using a material with separate translucency */
UBOOL ULensFlareComponent::HasSeparateTranslucency() const
{
	if (Template == NULL)
	{
		return FALSE;
	}

	for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
	{
		const FLensFlareElementMaterials& InnerElement = Materials(ElementIdx);
		for (INT InnerIdx = 0; InnerIdx < InnerElement.ElementMaterials.Num(); InnerIdx++)
		{
			if (InnerElement.ElementMaterials(InnerIdx))
			{
				UMaterial* Material = InnerElement.ElementMaterials(InnerIdx)->GetMaterial();
				if (Material && Material->EnableSeparateTranslucency)
				{
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

/**
 * Returns true if the prim is using a material that samples the scene color texture.
 * If true then these primitives are drawn after all other translucency
 */
UBOOL ULensFlareComponent::UsesSceneColor() const
{
	if (Template == NULL)
	{
		return FALSE;
	}

	UBOOL bUsesSceneColor = FALSE;
	for (INT ElementIdx = 0; ElementIdx < Materials.Num(); ElementIdx++)
	{
		const FLensFlareElementMaterials& InnerElement = Materials(ElementIdx);
		for (INT InnerIdx = 0; InnerIdx < InnerElement.ElementMaterials.Num(); InnerIdx++)
		{
			if (InnerElement.ElementMaterials(InnerIdx))
			{
				UMaterial* Material = InnerElement.ElementMaterials(InnerIdx)->GetMaterial();
				if (Material && (Material->UsesSceneColor() == TRUE))
				{
					bUsesSceneColor = TRUE;
					break;
				}
			}
		}
	}

	return bUsesSceneColor;
}

/** 
 * Initialize the draw data that gets used when creating the visualization scene proxys.
 *
 * @param bUseTemplate		If true, will initialize with the data found in the lens flare template object.
 */
void ULensFlareComponent::InitializeVisualizationData(UBOOL bUseTemplate)
{
	if (GIsEditor && (!bUseTemplate || Template))
	{
		if (PreviewInnerCone)
		{
			PreviewInnerCone->ConeRadius = bUseTemplate? Template->Radius: Radius;
			PreviewInnerCone->ConeAngle = bUseTemplate? Template->InnerCone: InnerCone;
			PreviewInnerCone->Translation = Translation;
		}

		if (PreviewOuterCone)
		{
			PreviewOuterCone->ConeRadius = bUseTemplate? Template->Radius: Radius;
			PreviewOuterCone->ConeAngle = bUseTemplate? Template->OuterCone: OuterCone;
			PreviewOuterCone->Translation = Translation;
		}

		if (PreviewRadius)
		{
			PreviewRadius->SphereRadius = bUseTemplate? Template->Radius: Radius;
			PreviewRadius->Translation = Translation;
		}
	}
}

FPrimitiveSceneProxy* ULensFlareComponent::CreateSceneProxy()
{
	if (Template)
	{
		DepthPriorityGroup = Template->ReflectionsDPG;
		OuterCone = Template->OuterCone;
		InnerCone = Template->InnerCone;
		ConeFudgeFactor = Template->ConeFudgeFactor;
		Radius = Template->Radius;
		bUseTrueConeCalculation = Template->bUseTrueConeCalculation;
		MinStrength = Template->MinStrength;
		if (bAutoActivate == TRUE)
		{
			bIsActive = TRUE;
		}

		SetupMaterialsArray(FALSE);
	}

	if (GSystemSettings.bAllowLensFlares && (DetailMode <= GSystemSettings.DetailMode))
	{
		FLensFlareSceneProxy* LFSceneProxy = new FLensFlareSceneProxy(this);
		check(LFSceneProxy);
		return LFSceneProxy;
	}

	return NULL;
}

// InstanceParameters interface
void ULensFlareComponent::AutoPopulateInstanceProperties()
{
}

/**
 *	LensFlareSource class
 */
/**
 *	Set the template of the LensFlareComponent.
 *
 *	@param	NewTemplate		The new template to use for the lens flare.
 */
void ALensFlareSource::SetTemplate(class ULensFlare* NewTemplate)
{
	if (LensFlareComp)
	{
		FComponentReattachContext ReattachContext(LensFlareComp);
		LensFlareComp->SetTemplate(NewTemplate);
	}
}

void ALensFlareSource::AutoPopulateInstanceProperties()
{
}

// AActor interface.
/**
 * Function that gets called from within Map_Check to allow this actor to check itself
 * for any potential errors and register them with map check dialog.
 */
#if WITH_EDITOR
void ALensFlareSource::CheckForErrors()
{
	Super::CheckForErrors();

	// LensFlareSources placed in a level should have a non-NULL LensFlareComponent.
	UObject* Outer = GetOuter();
	if (Cast<ULevel>(Outer))
	{
		if (LensFlareComp == NULL)
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_LensFlareComponentNull" ), *GetName() ) ), TEXT( "LensFlareComponentNull" ) );
		}
	}
}
#endif
