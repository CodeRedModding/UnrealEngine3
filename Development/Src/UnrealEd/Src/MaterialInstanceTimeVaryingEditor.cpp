/**
* MaterialInstanceTimeVaryingEditor.cpp: Material instance editor class.
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "PropertyWindow.h"
#include "MaterialEditorBase.h"
#include "MaterialEditorToolBar.h"
#include "MaterialInstanceTimeVaryingEditor.h"
#include "MaterialEditorPreviewScene.h"
#include "NewMaterialEditor.h"
#include "PropertyWindowManager.h"	// required for access to GPropertyWindowManager
#include "MaterialInstanceTimeVaryingHelpers.h"

//////////////////////////////////////////////////////////////////////////
// UMaterialEditorInstanceTimeVarying
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UMaterialEditorInstanceTimeVarying);

void UMaterialEditorInstanceTimeVarying::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged->GetName()==TEXT("Parent"))
	{
		// If the parent was changed to the source instance, set it to NULL.
		if(Parent==SourceInstance)
		{
			Parent = NULL;
		}

		{
			FGlobalComponentReattachContext RecreateComponents;
			SourceInstance->SetParent(Parent);
			// Fully update static parameters before reattaching the scene's components
			SetSourceInstance(SourceInstance);
		}
	}

	CopyToSourceInstance();

	// Tell our source instance to update itself so the preview updates.
	SourceInstance->PostEditChangeProperty(PropertyChangedEvent);
}

/** Regenerates the parameter arrays. */
void UMaterialEditorInstanceTimeVarying::RegenerateArrays()
{
	// Clear out existing parameters.
	VectorParameterValues.Empty();
	LinearColorParameterValues.Empty();
	ScalarParameterValues.Empty();
	TextureParameterValues.Empty();
	FontParameterValues.Empty();
	StaticSwitchParameterValues.Empty();
	StaticComponentMaskParameterValues.Empty();
	VisibleExpressions.Empty();

	if(Parent)
	{
		TArray<FGuid> Guids;

		// Only operate on base materials
		UMaterial* ParentMaterial = Parent->GetMaterial();
		SourceInstance->UpdateParameterNames();	// Update any parameter names that may have changed.

		// Loop through all types of parameters for this material and add them to the parameter arrays.
		TArray<FName> ParameterNames;

		// Vector Parameters.
		ParentMaterial->GetAllVectorParameterNames(ParameterNames, Guids);
		VectorParameterValues.AddZeroed(ParameterNames.Num());

		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorVectorParameterValueOverTime& ParameterValue = VectorParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			FLinearColor Value;
			FInterpCurveInitVector Values;
			
			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids(ParameterIdx);

			if(SourceInstance->GetVectorCurveParameterValue(ParameterName, Values))
			{
				ParameterValue.ParameterValueCurve = Values;
			}

			if(SourceInstance->GetVectorParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}

			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->VectorParameterValues.Num(); ParameterIdx++)
			{
				FVectorParameterValueOverTime& SourceParam = SourceInstance->VectorParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValueCurve = SourceParam.ParameterValueCurve;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
					
					ParameterValue.bLoop = SourceParam.bLoop;
					ParameterValue.bAutoActivate = SourceParam.bAutoActivate;
					ParameterValue.CycleTime = SourceParam.CycleTime;
					ParameterValue.bNormalizeTime = SourceParam.bNormalizeTime;

					ParameterValue.OffsetTime = SourceParam.OffsetTime;
					ParameterValue.bOffsetFromEnd = SourceParam.bOffsetFromEnd;
				}
			}
		}

		// LinearColor Parameters.
		// These can hook into Vector Parmaeters
		ParentMaterial->GetAllVectorParameterNames(ParameterNames, Guids);
		LinearColorParameterValues.AddZeroed(ParameterNames.Num());

		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorLinearColorParameterValueOverTime& ParameterValue = LinearColorParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			FLinearColor Value;
			FInterpCurveInitLinearColor Values;
			
			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids(ParameterIdx);

			if(SourceInstance->GetLinearColorCurveParameterValue(ParameterName, Values))
			{
				ParameterValue.ParameterValueCurve = Values;
			}

			if(SourceInstance->GetLinearColorParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}

			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->LinearColorParameterValues.Num(); ParameterIdx++)
			{
				FLinearColorParameterValueOverTime& SourceParam = SourceInstance->LinearColorParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValueCurve = SourceParam.ParameterValueCurve;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
					
					ParameterValue.bLoop = SourceParam.bLoop;
					ParameterValue.bAutoActivate = SourceParam.bAutoActivate;
					ParameterValue.CycleTime = SourceParam.CycleTime;
					ParameterValue.bNormalizeTime = SourceParam.bNormalizeTime;

					ParameterValue.OffsetTime = SourceParam.OffsetTime;
					ParameterValue.bOffsetFromEnd = SourceParam.bOffsetFromEnd;
				}
			}

		}

		// Scalar Parameters.
		ParentMaterial->GetAllScalarParameterNames(ParameterNames, Guids);
		ScalarParameterValues.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorScalarParameterValueOverTime& ParameterValue = ScalarParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			FInterpCurveInitFloat Values;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids(ParameterIdx);

			if(SourceInstance->GetScalarCurveParameterValue(ParameterName, Values))
			{
				ParameterValue.ParameterValueCurve = Values;
			}

			FLOAT Value;
			if(SourceInstance->GetScalarParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->ScalarParameterValues.Num(); ParameterIdx++)
			{
				FScalarParameterValueOverTime& SourceParam = SourceInstance->ScalarParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValueCurve = SourceParam.ParameterValueCurve;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;

					ParameterValue.bLoop = SourceParam.bLoop;
					ParameterValue.bAutoActivate = SourceParam.bAutoActivate;
					ParameterValue.CycleTime = SourceParam.CycleTime;
					ParameterValue.bNormalizeTime = SourceParam.bNormalizeTime;

					ParameterValue.OffsetTime = SourceParam.OffsetTime;
					ParameterValue.bOffsetFromEnd = SourceParam.bOffsetFromEnd;
				}
			}
		}


		// Texture Parameters.
		ParentMaterial->GetAllTextureParameterNames(ParameterNames, Guids);
		TextureParameterValues.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorTextureParameterValueOverTime& ParameterValue = TextureParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			UTexture* Value;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids(ParameterIdx);

			if(SourceInstance->GetTextureParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->TextureParameterValues.Num(); ParameterIdx++)
			{
				FTextureParameterValueOverTime& SourceParam = SourceInstance->TextureParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;

					ParameterValue.bLoop = SourceParam.bLoop;
					ParameterValue.bAutoActivate = SourceParam.bAutoActivate;
					ParameterValue.CycleTime = SourceParam.CycleTime;
					ParameterValue.bNormalizeTime = SourceParam.bNormalizeTime;

					ParameterValue.OffsetTime = SourceParam.OffsetTime;
					ParameterValue.bOffsetFromEnd = SourceParam.bOffsetFromEnd;
				}
			}
		}

		// Font Parameters.
		ParentMaterial->GetAllFontParameterNames(ParameterNames, Guids);
		FontParameterValues.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorFontParameterValueOverTime& ParameterValue = FontParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			UFont* FontValue;
			INT FontPage;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = Guids(ParameterIdx);

			if(SourceInstance->GetFontParameterValue(ParameterName, FontValue,FontPage))
			{
				ParameterValue.FontValue = FontValue;
				ParameterValue.FontPage = FontPage;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->FontParameterValues.Num(); ParameterIdx++)
			{
				FFontParameterValueOverTime& SourceParam = SourceInstance->FontParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.FontValue = SourceParam.FontValue;
					ParameterValue.FontPage = SourceParam.FontPage;

					ParameterValue.bLoop = SourceParam.bLoop;
					ParameterValue.bAutoActivate = SourceParam.bAutoActivate;
					ParameterValue.CycleTime = SourceParam.CycleTime;
					ParameterValue.bNormalizeTime = SourceParam.bNormalizeTime;

					ParameterValue.OffsetTime = SourceParam.OffsetTime;
					ParameterValue.bOffsetFromEnd = SourceParam.bOffsetFromEnd;
				}
			}
		}

		// Static Switch Parameters
		ParentMaterial->GetAllStaticSwitchParameterNames(ParameterNames, Guids);
		StaticSwitchParameterValues.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorStaticSwitchParameterValueOverTime& EditorParameter = StaticSwitchParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			UBOOL Value = FALSE;
			FGuid ExpressionId = Guids(ParameterIdx);

			EditorParameter.bOverride = FALSE;
			EditorParameter.ParameterName = ParameterName;
			EditorParameter.ExpressionId = Guids(ParameterIdx);

			//get the settings from the parent in the MITV chain
			if(SourceInstance->Parent->GetStaticSwitchParameterValue(ParameterName, Value, ExpressionId))
			{
				EditorParameter.ParameterValue = Value;
			}
			EditorParameter.ExpressionId = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
			{
				for(INT ParameterIdx = 0; ParameterIdx < SourceInstance->StaticParameters[QualityIndex]->StaticSwitchParameters.Num(); ParameterIdx++)
				{
					const FStaticSwitchParameter& StaticSwitchParam = SourceInstance->StaticParameters[QualityIndex]->StaticSwitchParameters(ParameterIdx);

					if(ParameterName == StaticSwitchParam.ParameterName)
					{
						EditorParameter.bOverride = StaticSwitchParam.bOverride;
						if (StaticSwitchParam.bOverride)
						{
							EditorParameter.ParameterValue = StaticSwitchParam.Value;
						}
					}
				}
			}
		}


		// Static Component Mask Parameters
		ParentMaterial->GetAllStaticComponentMaskParameterNames(ParameterNames, Guids);
		StaticComponentMaskParameterValues.AddZeroed(ParameterNames.Num());
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FEditorStaticComponentMaskParameterValueOverTime& EditorParameter = StaticComponentMaskParameterValues(ParameterIdx);
			FName ParameterName = ParameterNames(ParameterIdx);
			UBOOL R = FALSE;
			UBOOL G = FALSE;
			UBOOL B = FALSE;
			UBOOL A = FALSE;
			FGuid ExpressionId = Guids(ParameterIdx);

			EditorParameter.bOverride = FALSE;
			EditorParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MITV chain
			if(SourceInstance->Parent->GetStaticComponentMaskParameterValue(ParameterName, R, G, B, A, ExpressionId))
			{
				EditorParameter.ParameterValue.R = R;
				EditorParameter.ParameterValue.G = G;
				EditorParameter.ParameterValue.B = B;
				EditorParameter.ParameterValue.A = A;
			}
			EditorParameter.ExpressionId = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for (INT QualityIndex = 0; QualityIndex < MSQ_MAX; QualityIndex++)
			{
				for(INT ParameterIdx = 0; ParameterIdx < SourceInstance->StaticParameters[QualityIndex]->StaticComponentMaskParameters.Num(); ParameterIdx++)
				{
					const FStaticComponentMaskParameter& StaticComponentMaskParam = SourceInstance->StaticParameters[QualityIndex]->StaticComponentMaskParameters(ParameterIdx);

					if(ParameterName == StaticComponentMaskParam.ParameterName)
					{
						EditorParameter.bOverride = StaticComponentMaskParam.bOverride;
						if (StaticComponentMaskParam.bOverride)
						{
							EditorParameter.ParameterValue.R = StaticComponentMaskParam.R;
							EditorParameter.ParameterValue.G = StaticComponentMaskParam.G;
							EditorParameter.ParameterValue.B = StaticComponentMaskParam.B;
							EditorParameter.ParameterValue.A = StaticComponentMaskParam.A;
						}
					}
				}
			}
		}

		WxMaterialEditor::GetVisibleMaterialParameters(ParentMaterial, SourceInstance, VisibleExpressions);
	}
}

/** 
 * Copies the parameter array values back to the source instance. 
 *
 */
void UMaterialEditorInstanceTimeVarying::CopyToSourceInstance()
{
	SourceInstance->MarkPackageDirty();
	SourceInstance->ClearParameterValues();

	// Scalar Parameters
	for(INT ParameterIdx=0; ParameterIdx<ScalarParameterValues.Num(); ParameterIdx++)
	{
		FEditorScalarParameterValueOverTime& ParameterValue = ScalarParameterValues(ParameterIdx);

		//debugf( TEXT( "CopyToSourceInstance: %s"), *ParameterValue.ParameterName.ToString() );

		if(ParameterValue.bOverride)
		{
			SourceInstance->SetScalarCurveParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValueCurve);
			SourceInstance->SetScalarParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValue);

			UpdateParameterValueOverTimeValues<UMaterialInstanceTimeVarying, FScalarParameterValueOverTime>( SourceInstance, ParameterValue.ParameterName, ParameterValue.bLoop, ParameterValue.bAutoActivate, ParameterValue.CycleTime, ParameterValue.bNormalizeTime, ParameterValue.OffsetTime, ParameterValue.bOffsetFromEnd );
		}
	}

	// Texture Parameters
	for(INT ParameterIdx=0; ParameterIdx<TextureParameterValues.Num(); ParameterIdx++)
	{
		FEditorTextureParameterValueOverTime& ParameterValue = TextureParameterValues(ParameterIdx);

		if(ParameterValue.bOverride)
		{
			SourceInstance->SetTextureParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValue);

			UpdateParameterValueOverTimeValues<UMaterialInstanceTimeVarying, FTextureParameterValueOverTime>( SourceInstance, ParameterValue.ParameterName, ParameterValue.bLoop, ParameterValue.bAutoActivate, ParameterValue.CycleTime, ParameterValue.bNormalizeTime, ParameterValue.OffsetTime, ParameterValue.bOffsetFromEnd );
		}
	}

	// Font Parameters
	for(INT ParameterIdx=0; ParameterIdx<FontParameterValues.Num(); ParameterIdx++)
	{
		FEditorFontParameterValueOverTime& ParameterValue = FontParameterValues(ParameterIdx);

		if(ParameterValue.bOverride)
		{
			SourceInstance->SetFontParameterValue(ParameterValue.ParameterName,ParameterValue.FontValue,ParameterValue.FontPage);

			UpdateParameterValueOverTimeValues<UMaterialInstanceTimeVarying, FFontParameterValueOverTime>( SourceInstance, ParameterValue.ParameterName, ParameterValue.bLoop, ParameterValue.bAutoActivate, ParameterValue.CycleTime, ParameterValue.bNormalizeTime, ParameterValue.OffsetTime, ParameterValue.bOffsetFromEnd );
		}
	}

	// Vector Parameters
	for(INT ParameterIdx=0; ParameterIdx<VectorParameterValues.Num(); ParameterIdx++)
	{
		FEditorVectorParameterValueOverTime& ParameterValue = VectorParameterValues(ParameterIdx);

		if(ParameterValue.bOverride)
		{
			SourceInstance->SetVectorCurveParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValueCurve);
			SourceInstance->SetVectorParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValue);

			UpdateParameterValueOverTimeValues<UMaterialInstanceTimeVarying, FVectorParameterValueOverTime>( SourceInstance, ParameterValue.ParameterName, ParameterValue.bLoop, ParameterValue.bAutoActivate, ParameterValue.CycleTime, ParameterValue.bNormalizeTime, ParameterValue.OffsetTime, ParameterValue.bOffsetFromEnd );
		}
	}

	// LinearColor (RGBA Vector) Parameters
	for(INT ParameterIdx=0; ParameterIdx<VectorParameterValues.Num(); ParameterIdx++)
	{
		FEditorLinearColorParameterValueOverTime& ParameterValue = LinearColorParameterValues(ParameterIdx);

		if(ParameterValue.bOverride)
		{
			SourceInstance->SetLinearColorCurveParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValueCurve);
			SourceInstance->SetLinearColorParameterValue(ParameterValue.ParameterName, ParameterValue.ParameterValue);

			UpdateParameterValueOverTimeValues<UMaterialInstanceTimeVarying, FLinearColorParameterValueOverTime>( SourceInstance, ParameterValue.ParameterName, ParameterValue.bLoop, ParameterValue.bAutoActivate, ParameterValue.CycleTime, ParameterValue.bNormalizeTime, ParameterValue.OffsetTime, ParameterValue.bOffsetFromEnd );

			SourceInstance->CheckForVectorParameterConflicts(ParameterValue.ParameterName);
		}
	}

	

	CopyStaticParametersToSourceInstance();

	// Copy phys material back to source instance
	SourceInstance->PhysMaterial = PhysMaterial;

	// Copy phys material mask information back to the source instance.
	SourceInstance->PhysMaterialMask = PhysicalMaterialMask.PhysMaterialMask;
	SourceInstance->PhysMaterialMaskUVChannel = PhysicalMaterialMask.PhysMaterialMaskUVChannel;
	SourceInstance->BlackPhysicalMaterial = PhysicalMaterialMask.BlackPhysicalMaterial;
	SourceInstance->WhitePhysicalMaterial = PhysicalMaterialMask.WhitePhysicalMaterial;

	SourceInstance->bAutoActivateAll = bAutoActivateAll;      

	// Copy the Lightmass settings...
	SourceInstance->SetOverrideCastShadowAsMasked(LightmassSettings.CastShadowAsMasked.bOverride);
	SourceInstance->SetCastShadowAsMasked(LightmassSettings.CastShadowAsMasked.ParameterValue);
	SourceInstance->SetOverrideEmissiveBoost(LightmassSettings.EmissiveBoost.bOverride);
	SourceInstance->SetEmissiveBoost(LightmassSettings.EmissiveBoost.ParameterValue);
	SourceInstance->SetOverrideDiffuseBoost(LightmassSettings.DiffuseBoost.bOverride);
	SourceInstance->SetDiffuseBoost(LightmassSettings.DiffuseBoost.ParameterValue);
	SourceInstance->SetOverrideSpecularBoost(LightmassSettings.SpecularBoost.bOverride);
	SourceInstance->SetSpecularBoost(LightmassSettings.SpecularBoost.ParameterValue);
	SourceInstance->SetOverrideExportResolutionScale(LightmassSettings.ExportResolutionScale.bOverride);
	SourceInstance->SetExportResolutionScale(LightmassSettings.ExportResolutionScale.ParameterValue);
	SourceInstance->SetOverrideDistanceFieldPenumbraScale(LightmassSettings.DistanceFieldPenumbraScale.bOverride);
	SourceInstance->SetDistanceFieldPenumbraScale(LightmassSettings.DistanceFieldPenumbraScale.ParameterValue);

	// Update object references and parameter names.
	SourceInstance->UpdateParameterNames();
}

/** Copies static parameters to the source instance, which will be marked dirty if a compile was necessary */
void UMaterialEditorInstanceTimeVarying::CopyStaticParametersToSourceInstance()
{
	//build a static parameter set containing all static parameter settings
	FStaticParameterSet StaticParameters;

	// Static Switch Parameters
	for(INT ParameterIdx = 0; ParameterIdx < StaticSwitchParameterValues.Num(); ParameterIdx++)
	{
		FEditorStaticSwitchParameterValueOverTime& EditorParameter = StaticSwitchParameterValues(ParameterIdx);
		UBOOL SwitchValue = EditorParameter.ParameterValue;
		FGuid ExpressionIdValue = EditorParameter.ExpressionId;
		if (!EditorParameter.bOverride)
		{
			if (Parent)
			{
				//use the parent's settings if this parameter is not overridden
				SourceInstance->Parent->GetStaticSwitchParameterValue(EditorParameter.ParameterName, SwitchValue, ExpressionIdValue);
			}
		}
		FStaticSwitchParameter * NewParameter = 
			new(StaticParameters.StaticSwitchParameters) FStaticSwitchParameter(EditorParameter.ParameterName, SwitchValue, EditorParameter.bOverride, ExpressionIdValue);
	}

	// Static Component Mask Parameters
	for(INT ParameterIdx = 0; ParameterIdx < StaticComponentMaskParameterValues.Num(); ParameterIdx++)
	{
		FEditorStaticComponentMaskParameterValueOverTime& EditorParameter = StaticComponentMaskParameterValues(ParameterIdx);
		UBOOL MaskR = EditorParameter.ParameterValue.R;
		UBOOL MaskG = EditorParameter.ParameterValue.G;
		UBOOL MaskB = EditorParameter.ParameterValue.B;
		UBOOL MaskA = EditorParameter.ParameterValue.A;
		FGuid ExpressionIdValue = EditorParameter.ExpressionId;

		if (!EditorParameter.bOverride)
		{
			if (Parent)
			{
				//use the parent's settings if this parameter is not overridden
				SourceInstance->Parent->GetStaticComponentMaskParameterValue(EditorParameter.ParameterName, MaskR, MaskG, MaskB, MaskA, ExpressionIdValue);
			}
		}
		FStaticComponentMaskParameter * NewParameter = new(StaticParameters.StaticComponentMaskParameters) 
			FStaticComponentMaskParameter(EditorParameter.ParameterName, MaskR, MaskG, MaskB, MaskA, EditorParameter.bOverride, ExpressionIdValue);
	}

	if (SourceInstance->SetStaticParameterValues(&StaticParameters))
	{
		//mark the package dirty if a compile was needed
		SourceInstance->MarkPackageDirty();
	}
}


/**  
 * Sets the source instance for this object and regenerates arrays. 
 *
 * @param MaterialInterface		Instance to use as the source for this material editor instance.
 */
void UMaterialEditorInstanceTimeVarying::SetSourceInstance(UMaterialInstanceTimeVarying* MaterialInterface)
{
	check(MaterialInterface);
	SourceInstance = MaterialInterface;
	Parent = SourceInstance->Parent;
	PhysMaterial = SourceInstance->PhysMaterial;

	// Copy phys material mask information from the source instance.
	PhysicalMaterialMask.PhysMaterialMask = SourceInstance->PhysMaterialMask;
	PhysicalMaterialMask.PhysMaterialMaskUVChannel = SourceInstance->PhysMaterialMaskUVChannel;
	PhysicalMaterialMask.BlackPhysicalMaterial = SourceInstance->BlackPhysicalMaterial;
	PhysicalMaterialMask.WhitePhysicalMaterial = SourceInstance->WhitePhysicalMaterial;

	bAutoActivateAll = SourceInstance->bAutoActivateAll;
	// Copy the Lightmass settings...
	LightmassSettings.CastShadowAsMasked.bOverride = SourceInstance->GetOverrideCastShadowAsMasked();
	LightmassSettings.CastShadowAsMasked.ParameterValue = SourceInstance->GetCastShadowAsMasked();
	LightmassSettings.EmissiveBoost.bOverride = SourceInstance->GetOverrideEmissiveBoost();
	LightmassSettings.EmissiveBoost.ParameterValue = SourceInstance->GetEmissiveBoost();
	LightmassSettings.DiffuseBoost.bOverride = SourceInstance->GetOverrideDiffuseBoost();
	LightmassSettings.DiffuseBoost.ParameterValue = SourceInstance->GetDiffuseBoost();
	LightmassSettings.SpecularBoost.bOverride = SourceInstance->GetOverrideSpecularBoost();
	LightmassSettings.SpecularBoost.ParameterValue = SourceInstance->GetSpecularBoost();
	LightmassSettings.ExportResolutionScale.bOverride = SourceInstance->GetOverrideExportResolutionScale();
	LightmassSettings.ExportResolutionScale.ParameterValue = SourceInstance->GetExportResolutionScale();
	LightmassSettings.DistanceFieldPenumbraScale.bOverride = SourceInstance->GetOverrideDistanceFieldPenumbraScale();
	LightmassSettings.DistanceFieldPenumbraScale.ParameterValue = SourceInstance->GetDistanceFieldPenumbraScale();

	RegenerateArrays();

	//propagate changes to the base material so the instance will be updated if it has a static permutation resource
	CopyStaticParametersToSourceInstance();
	SourceInstance->UpdateStaticPermutation();
}


//////////////////////////////////////////////////////////////////////////
// WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC_CLASS(WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter, WxCustomPropertyItem_ConditionalItem);

BEGIN_EVENT_TABLE(WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter, WxCustomPropertyItem_ConditionalItem)
	EVT_BUTTON(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_RESETTODEFAULT, OnResetToDefault)
	EVT_MENU(ID_PROP_RESET_TO_DEFAULT, WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::OnResetToDefault)
END_EVENT_TABLE()

WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter() : 
WxCustomPropertyItem_ConditionalItem()
{
	ResetToDefault = NULL;
	bAllowEditing = FALSE;
}

/**
 * Initialize this property window.  Must be the first function called after creating the window.
 */
void WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::Create(wxWindow* InParent)
{
	WxCustomPropertyItem_ConditionalItem::Create(InParent);

	// Create a new button and add it to the button array.
	if(ResetToDefault==NULL)
	{
		ResetToDefault = new wxBitmapButton( this, ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_RESETTODEFAULT, GPropertyWindowManager->Prop_ResetToDefaultB );
		INT OldIndentX = PropertyNode->GetIndentX();
		PropertyNode->SetIndentX(OldIndentX + 15 + PROP_ARROW_Width);

		// Generate tooltip text for this button.
		UMaterialEditorInstanceTimeVarying* Instance = GetInstanceObject();

		if(Instance && Instance->Parent)
		{
			FString ToolTipText = *LocalizeUnrealEd("PropertyWindow_ResetToDefault");
			FName PropertyName = PropertyStructName;

			FName ScalarArrayName(TEXT("ScalarParameterValues"));
			FName TextureArrayName(TEXT("TextureParameterValues"));
			FName FontArrayName(TEXT("FontParameterValues"));
			FName VectorArrayName(TEXT("VectorParameterValues"));
			FName StaticSwitchArrayName(TEXT("StaticSwitchParameterValues"));
			FName StaticComponentMaskArrayName(TEXT("StaticComponentMaskParameterValues"));

			FName TempDisplayName(*DisplayName);
			if(PropertyName==ScalarArrayName)
			{
				FLOAT OutValue;
				if(Instance->Parent->GetScalarParameterValue(TempDisplayName, OutValue))
				{
					ToolTipText += TEXT(" ");
					ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceFloatValue_F"), OutValue));
				}
			}
			else if(PropertyName==TextureArrayName)
			{
				UTexture* OutValue;
				if(Instance->Parent->GetTextureParameterValue(TempDisplayName, OutValue))
				{
					if(OutValue)
					{
						ToolTipText += TEXT(" ");
						ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceTextureValue_F"), *OutValue->GetName()));
					}
				}				
			}
			else if(PropertyName==FontArrayName)
			{
				UFont* OutFontValue;
				INT OutFontPage;
				if(Instance->Parent->GetFontParameterValue(TempDisplayName, OutFontValue,OutFontPage))
				{
					if(OutFontValue)
					{
						ToolTipText += TEXT(" ");
						ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceFontValue_F"), *OutFontValue->GetName(),OutFontPage));
					}
				}				
			}
			else if(PropertyName==VectorArrayName)
			{
				FLinearColor OutValue;
				if(Instance->Parent->GetVectorParameterValue(TempDisplayName, OutValue))
				{
					ToolTipText += TEXT(" ");
					ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceVectorValue_F"), OutValue.R, OutValue.G, OutValue.B, OutValue.A));
				}				
			}
			else if(PropertyName==StaticSwitchArrayName)
			{
				UBOOL OutValue;
				FGuid TempGuid(0,0,0,0);
				if(Instance->Parent->GetStaticSwitchParameterValue(TempDisplayName, OutValue, TempGuid))
				{
					ToolTipText += TEXT(" ");
					ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceStaticSwitchValue_F"), (INT)OutValue));
				}				
			}
			else if(PropertyName==StaticComponentMaskArrayName)
			{
				UBOOL OutValue[4];
				FGuid TempGuid(0,0,0,0);

				if(Instance->Parent->GetStaticComponentMaskParameterValue(TempDisplayName, OutValue[0], OutValue[1], OutValue[2], OutValue[3], TempGuid))
				{
					ToolTipText += TEXT(" ");
					ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceStaticComponentMaskValue_F"), (INT)OutValue[0], (INT)OutValue[1], (INT)OutValue[2], (INT)OutValue[3]));
				}				
			}

			ResetToDefault->SetToolTip(*ToolTipText);
		}
	}
}

/**
 * Returns TRUE if MouseX and MouseY fall within the bounding region of the checkbox used for displaying the value of this property's edit condition.
 */
UBOOL WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::ClickedCheckbox( INT MouseX, INT MouseY ) const
{
	if( SupportsEditConditionCheckBox() )
	{
		//Disabling check box
		check(PropertyNode);
		wxRect ConditionalRect = GetConditionalRect();

		if (ConditionalRect.Contains(MouseX, MouseY))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Toggles the value of the property being used as the condition for editing this property.
 *
 * @return	the new value of the condition (i.e. TRUE if the condition is now TRUE)
 */
UBOOL WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::ToggleConditionValue()
{	
	UMaterialEditorInstanceTimeVarying* Instance = GetInstanceObject();

	if(Instance)
	{
		FName PropertyName = PropertyStructName;
		FName ScalarArrayName(TEXT("ScalarParameterValues"));
		FName TextureArrayName(TEXT("TextureParameterValues"));
		FName FontArrayName(TEXT("FontParameterValues"));
		FName VectorArrayName(TEXT("VectorParameterValues"));
		FName StaticSwitchArrayName(TEXT("StaticSwitchParameterValues"));
		FName StaticComponentMaskArrayName(TEXT("StaticComponentMaskParameterValues"));

		if(PropertyName==ScalarArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->ScalarParameterValues.Num();ParamIdx++)
			{
				FEditorScalarParameterValueOverTime& Param = Instance->ScalarParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					Param.bOverride = !Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==TextureArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->TextureParameterValues.Num();ParamIdx++)
			{
				FEditorTextureParameterValueOverTime& Param = Instance->TextureParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					Param.bOverride = !Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==FontArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->FontParameterValues.Num();ParamIdx++)
			{
				FEditorFontParameterValueOverTime& Param = Instance->FontParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					Param.bOverride = !Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==VectorArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->VectorParameterValues.Num();ParamIdx++)
			{
				FEditorVectorParameterValueOverTime& Param = Instance->VectorParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					Param.bOverride = !Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==StaticSwitchArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->StaticSwitchParameterValues.Num();ParamIdx++)
			{
				FEditorStaticSwitchParameterValueOverTime& Param = Instance->StaticSwitchParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					Param.bOverride = !Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==StaticComponentMaskArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->StaticComponentMaskParameterValues.Num();ParamIdx++)
			{
				FEditorStaticComponentMaskParameterValueOverTime& Param = Instance->StaticComponentMaskParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					Param.bOverride = !Param.bOverride;
					break;
				}
			}
		}

		// Notify the instance that we modified an override so it needs to update itself.
		FPropertyChangedEvent PropertyEvent(GetProperty());
		Instance->PostEditChangeProperty(PropertyEvent);
	}

	// Always allow editing even if we aren't overriding values.
 	return TRUE;
}


/**
 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
 * associated property should be allowed.
 */
UBOOL WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::IsOverridden()
{
	UMaterialEditorInstanceTimeVarying* Instance = GetInstanceObject();

	if(Instance)
	{
		FName PropertyName = PropertyStructName;
		FName ScalarArrayName(TEXT("ScalarParameterValues"));
		FName TextureArrayName(TEXT("TextureParameterValues"));
		FName FontArrayName(TEXT("FontParameterValues"));
		FName VectorArrayName(TEXT("VectorParameterValues"));
		FName StaticSwitchArrayName(TEXT("StaticSwitchParameterValues"));
		FName StaticComponentMaskArrayName(TEXT("StaticComponentMaskParameterValues"));

		if(PropertyName==ScalarArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->ScalarParameterValues.Num();ParamIdx++)
			{
				FEditorScalarParameterValueOverTime& Param = Instance->ScalarParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					bAllowEditing = Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==TextureArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->TextureParameterValues.Num();ParamIdx++)
			{
				FEditorTextureParameterValueOverTime& Param = Instance->TextureParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					bAllowEditing = Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==FontArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->FontParameterValues.Num();ParamIdx++)
			{
				FEditorFontParameterValueOverTime& Param = Instance->FontParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					bAllowEditing = Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==VectorArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->VectorParameterValues.Num();ParamIdx++)
			{
				FEditorVectorParameterValueOverTime& Param = Instance->VectorParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					bAllowEditing = Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==StaticSwitchArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->StaticSwitchParameterValues.Num();ParamIdx++)
			{
				FEditorStaticSwitchParameterValueOverTime& Param = Instance->StaticSwitchParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					bAllowEditing = Param.bOverride;
					break;
				}
			}
		}
		else if(PropertyName==StaticComponentMaskArrayName)
		{
			for(INT ParamIdx=0; ParamIdx<Instance->StaticComponentMaskParameterValues.Num();ParamIdx++)
			{
				FEditorStaticComponentMaskParameterValueOverTime& Param = Instance->StaticComponentMaskParameterValues(ParamIdx);

				if(Param.ParameterName.ToString() == DisplayName)
				{
					bAllowEditing = Param.bOverride;
					break;
				}
			}
		}
	}

	return bAllowEditing;
}


/**
 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
 * associated property should be allowed.
 */
UBOOL WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::IsConditionMet()
{
	return TRUE;
}

/** @return Returns the instance object this property is associated with. */
UMaterialEditorInstanceTimeVarying* WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::GetInstanceObject()
{
	FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
	UMaterialEditorInstanceTimeVarying* MaterialInterface = NULL;

	if(ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			MaterialInterface = Cast<UMaterialEditorInstanceTimeVarying>(*It);
			break;
		}
	}

	return MaterialInterface;
}

/**
 * Renders the left side of the property window item.
 *
 * This version is responsible for rendering the checkbox used for toggling whether this property item window should be enabled.
 *
 * @param	RenderDeviceContext		the device context to use for rendering the item name
 * @param	ClientRect				the bounding region of the property window item
 */
void WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect )
{
	const UBOOL bItemEnabled = IsOverridden();

	// determine which checkbox image to render
	const WxMaskedBitmap& bmp = bItemEnabled
		? GPropertyWindowManager->CheckBoxOnB
		: GPropertyWindowManager->CheckBoxOffB;

	// render the checkbox bitmap
	wxRect ConditionalRect = GetConditionalRect();
	// render the checkbox bitmap
	RenderDeviceContext.DrawBitmap( bmp, ConditionalRect.GetLeft(), ConditionalRect.GetTop(), 1 );
	ResetToDefault->SetSize(ConditionalRect.GetLeft()-PROP_Indent, ConditionalRect.GetTop(), PROP_Indent, PROP_Indent);

	INT W, H;
	RenderDeviceContext.GetTextExtent( *DisplayName, &W, &H );
	wxRect TitleRect = GetItemNameRect(W, H);
	RenderDeviceContext.DrawText( *DisplayName, TitleRect.GetLeft(), TitleRect.GetTop() );

	RenderDeviceContext.DestroyClippingRegion();
}

/** Reset to default button event. */
void WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::OnResetToDefault(wxCommandEvent &Event)
{
	UMaterialEditorInstanceTimeVarying* Instance = GetInstanceObject();

	if(Instance && Instance->Parent)
	{
		FName PropertyName = PropertyStructName;

		FName ScalarArrayName(TEXT("ScalarParameterValues"));
		FName TextureArrayName(TEXT("TextureParameterValues"));
		FName FontArrayName(TEXT("FontParameterValues"));
		FName VectorArrayName(TEXT("VectorParameterValues"));
		FName StaticSwitchArrayName(TEXT("StaticSwitchParameterValues"));
		FName StaticComponentMaskArrayName(TEXT("StaticComponentMaskParameterValues"));

		FName TempDisplayName(*DisplayName);
		if(PropertyName==ScalarArrayName)
		{
			FInterpCurveInitFloat OutValues;
			if(Instance->Parent->GetScalarCurveParameterValue(TempDisplayName, OutValues))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->ScalarParameterValues.Num(); PropertyIdx++)
				{
					FEditorScalarParameterValueOverTime& Value = Instance->ScalarParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValueCurve = OutValues;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}

			FLOAT OutValue;
			if(Instance->Parent->GetScalarParameterValue(TempDisplayName, OutValue))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->ScalarParameterValues.Num(); PropertyIdx++)
				{
					FEditorScalarParameterValueOverTime& Value = Instance->ScalarParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}
		}
		else if(PropertyName==TextureArrayName)
		{
			UTexture* OutValue;
			if(Instance->Parent->GetTextureParameterValue(TempDisplayName, OutValue))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->TextureParameterValues.Num(); PropertyIdx++)
				{
					FEditorTextureParameterValueOverTime& Value = Instance->TextureParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}				
		}
		else if(PropertyName==FontArrayName)
		{
			UFont* OutFontValue;
			INT OutFontPage;
			if(Instance->Parent->GetFontParameterValue(TempDisplayName, OutFontValue,OutFontPage))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->FontParameterValues.Num(); PropertyIdx++)
				{
					FEditorFontParameterValueOverTime& Value = Instance->FontParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.FontValue = OutFontValue;
						Value.FontPage = OutFontPage;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}				
		}
		else if(PropertyName==VectorArrayName)
		{
			FInterpCurveInitVector OutValues;
			if(Instance->Parent->GetVectorCurveParameterValue(TempDisplayName, OutValues))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->VectorParameterValues.Num(); PropertyIdx++)
				{
					FEditorVectorParameterValueOverTime& Value = Instance->VectorParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValueCurve = OutValues;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}

			FLinearColor OutValue;
			if(Instance->Parent->GetVectorParameterValue(TempDisplayName, OutValue))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->VectorParameterValues.Num(); PropertyIdx++)
				{
					FEditorVectorParameterValueOverTime& Value = Instance->VectorParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}				
		}
		else if(PropertyName==StaticSwitchArrayName)
		{
			UBOOL OutValue;
			FGuid TempGuid(0,0,0,0);
			if(Instance->Parent->GetStaticSwitchParameterValue(TempDisplayName, OutValue, TempGuid))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->StaticSwitchParameterValues.Num(); PropertyIdx++)
				{
					FEditorStaticSwitchParameterValueOverTime& Value = Instance->StaticSwitchParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}				
		}
		else if(PropertyName==StaticComponentMaskArrayName)
		{
			UBOOL OutValue[4];
			FGuid TempGuid(0,0,0,0);

			if(Instance->Parent->GetStaticComponentMaskParameterValue(TempDisplayName, OutValue[0], OutValue[1], OutValue[2], OutValue[3], TempGuid))
			{
				for(INT PropertyIdx=0; PropertyIdx<Instance->StaticComponentMaskParameterValues.Num(); PropertyIdx++)
				{
					FEditorStaticComponentMaskParameterValueOverTime& Value = Instance->StaticComponentMaskParameterValues(PropertyIdx);
					if(Value.ParameterName == TempDisplayName)
					{
						Value.ParameterValue.R = OutValue[0];
						Value.ParameterValue.G = OutValue[1];
						Value.ParameterValue.B = OutValue[2];
						Value.ParameterValue.A = OutValue[3];
						Instance->CopyToSourceInstance();
						break;
					}
				}
			}				
		}

		// Rebuild property window to update the values.
		GetPropertyWindow()->Rebuild();
		GetPropertyWindow()->RequestMainWindowTakeFocus();
		check(PropertyNode);
		PropertyNode->InvalidateChildControls();
 	}
}

/**
* Overriden function to allow hiding when not referenced.
*/
UBOOL WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::IsDerivedForcedHide (void) const
{
	const WxPropertyWindowHost* PropertyWindowHost = GetPropertyWindow()->GetParentHostWindow();
	check(PropertyWindowHost);
	const wxWindow *HostParent   = PropertyWindowHost->GetParent();
	const WxMaterialInstanceTimeVaryingEditor *Win = wxDynamicCast( HostParent, WxMaterialInstanceTimeVaryingEditor );
	UBOOL ForceHide = TRUE;

	// When property window is floating (not docked) the parent is an wxAuiFloatingFrame.
	// We must get at the Editor via the docking system:
	//                      wxAuiFloatingFrame -> wxAuiManager -> ManagedWindow
	if (Win == NULL)
	{
		wxAuiFloatingFrame *OwnerFloatingFrame = wxDynamicCast( HostParent, wxAuiFloatingFrame );
		check(OwnerFloatingFrame);
		wxAuiManager *AuiManager = OwnerFloatingFrame->GetOwnerManager();
		Win = wxDynamicCast(AuiManager->GetManagedWindow(), WxMaterialInstanceTimeVaryingEditor);
	}
	check(Win);

	UMaterialEditorInstanceTimeVarying* MaterialInterface = Win->MaterialEditorInstance;
	check(MaterialInterface);

	if(Win->ToolBar->GetToolState(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS) || MaterialInterface->VisibleExpressions.ContainsItem(ExpressionId))
	{
		ForceHide = FALSE;
	}

	return ForceHide;
}

/**
 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
 * and (if the property window item is expandable) to toggle expansion state.
 *
 * @param	Event	the mouse click input that generated the event
 *
 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
 */
UBOOL WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::ClickedPropertyItem( wxMouseEvent& Event )
{
	FPropertyNode* PropertyNode	= GetPropertyNode();
	UProperty* NodeProperty = GetProperty();

	UBOOL bShouldGainFocus = TRUE;

	// if this property is edit-const, it can't be changed
	// or if we couldn't find a valid condition property, also use the base version
	if ( NodeProperty == NULL || PropertyNode->IsEditConst() )
	{
		bShouldGainFocus = WxItemPropertyControl::ClickedPropertyItem(Event);
	}

	// if they clicked on the checkbox, toggle the edit condition
	else if ( ClickedCheckbox(Event.GetX(), Event.GetY()) )
	{
		
		NotifyPreChange(NodeProperty);
		bShouldGainFocus = !PropertyNode->HasNodeFlags(EPropertyNodeFlags::CanBeExpanded);
		if ( ToggleConditionValue() == FALSE )
		{
			bShouldGainFocus = FALSE;

			// if we just disabled the condition which allows us to edit this control
			// collapse the item if this is an expandable item
			if ( HasNodeFlags(EPropertyNodeFlags::CanBeExpanded) )
			{
				const UBOOL bExpand = FALSE;
				const UBOOL bRecurse = FALSE;
				PropertyNode->SetExpanded(bExpand, bRecurse);
			}
		}

		
		// Note the current property window so that CALLBACK_ObjectPropertyChanged
		// doesn't destroy the window out from under us.
		WxPropertyWindow* MainWindow = GetPropertyWindow();
		check(MainWindow);
		MainWindow->ChangeActiveCallbackCount(1);

		const UBOOL bTopologyChange = FALSE;
		FPropertyChangedEvent ChangeEvent(NodeProperty, bTopologyChange);
		NotifyPostChange(ChangeEvent);

		// Unset, effectively making this property window updatable by CALLBACK_ObjectPropertyChanged.
		MainWindow->ChangeActiveCallbackCount(-1);
	}
	// if the condition for editing this control has been met (i.e. the checkbox is checked), pass the event back to the base version, which will do the right thing
	// based on where the user clicked
	else if ( IsConditionMet() )
	{
		bShouldGainFocus = WxItemPropertyControl::ClickedPropertyItem(Event);
	}
	else
	{
		// the condition is false, so this control isn't allowed to do anything - swallow the event.
		bShouldGainFocus = FALSE;
	}

	return bShouldGainFocus;
}

/**Returns the rect for checkbox used in conditional item property controls.*/
wxRect  WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter::GetConditionalRect (void) const
{
	wxRect ConditionalRect = WxCustomPropertyItem_ConditionalItem::GetConditionalRect();
	//Make room for reset to defaults
	ConditionalRect.x += PROP_Indent;
	return ConditionalRect;
}

//////////////////////////////////////////////////////////////////////////
// WxPropertyWindow_MaterialInstanceTimeVaryingParameters
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow_MaterialInstanceTimeVaryingParameters, WxItemPropertyControl);

// Called by Expand(), creates any child items for any properties within this item.
void WxPropertyWindow_MaterialInstanceTimeVaryingParameters::InitWindowDefinedChildNodes(void)
{
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	check(PropertyNode);
	UProperty* Property = GetProperty();
	check(Property);

	FName PropertyName = Property->GetFName();
	UStructProperty* StructProperty = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty);
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
	UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property,CLASS_IsAUObjectProperty);

	// Copy IsSorted() flag from parent.  Default sorted to TRUE if no parent exists.

	PropertyNode->SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);
	if( Property->ArrayDim > 1 && PropertyNode->GetArrayIndex() == -1 )
	{
		for( INT i = 0 ; i < Property->ArrayDim ; i++ )
		{
			FItemPropertyNode* NewTreeItemNode = new FItemPropertyNode;//;//CreatePropertyItem(ArrayProperty,i,this);
			WxItemPropertyControl* pwi = PropertyNode->CreatePropertyItem(Property,i);
			NewTreeItemNode->InitNode(pwi, PropertyNode, MainWindow, Property, i*Property->ElementSize, i);
			PropertyNode->AddChildNode(NewTreeItemNode);
		}
	}
	else if( ArrayProperty )
	{
		FScriptArray* Array = NULL;
		TArray<BYTE*> Addresses;
		if ( PropertyNode->GetReadAddress( PropertyNode, PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
		{
			Array = (FScriptArray*)Addresses(0);
		}

		FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();
		UMaterialEditorInstanceTimeVarying* MaterialInterface = NULL;
		UMaterial* Material = NULL;


		if(ObjectNode)
		{
			for(FObjectPropertyNode::TObjectIterator It(ObjectNode->ObjectIterator()); It; ++It)
			{
				MaterialInterface = Cast<UMaterialEditorInstanceTimeVarying>(*It);
				Material = MaterialInterface->SourceInstance->GetMaterial();
				break;
			}
		}

		if( Array && Material )
		{
			FName ParameterValueName(TEXT("ParameterValue"));
			FName ScalarArrayName(TEXT("ScalarParameterValues"));
			FName TextureArrayName(TEXT("TextureParameterValues"));
			FName FontArrayName(TEXT("FontParameterValues"));
			FName VectorArrayName(TEXT("VectorParameterValues"));
			FName StaticSwitchArrayName(TEXT("StaticSwitchParameterValues"));
			FName StaticComponentMaskArrayName(TEXT("StaticComponentMaskParameterValues"));

			// Make sure that the inner of this array is a material instance parameter struct.
			UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProperty->Inner);
		
			if(StructProperty)
			{	
				// Iterate over all possible fields of this struct until we find the value field, we want to combine
				// the name and value of the parameter into one property window item.  We do this by adding a item for the value
				// and overriding the name of the item using the name from the parameter.
				for( TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It )
				{
					UProperty* StructMember = *It;
					if( MainWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable) || (StructMember->PropertyFlags&CPF_Edit) )
					{
						// Loop through all elements of this array and add properties for each one.
						for( INT ArrayIdx = 0 ; ArrayIdx < Array->Num() ; ArrayIdx++ )
						{	

							WxItemPropertyControl* pwi = PropertyNode->CreatePropertyItem(StructMember,INDEX_NONE);
							WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter* PropertyWindowItem = wxDynamicCast(pwi, WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter);

							if( PropertyWindowItem == NULL )
							{
								debugf( TEXT( "PropertyName was NULL: %s "), *StructMember->GetName() );
								continue;
							}

							if(StructMember->GetFName() == ParameterValueName)
							{
								// Find a name for the parameter property we are adding.
								FName OverrideName = NAME_None;
								BYTE* ElementData = ((BYTE*)Array->GetData())+ArrayIdx*ArrayProperty->Inner->ElementSize;

								//debugf( TEXT( "PropertyName is: %s "), *PropertyName.ToString() );

								if(PropertyName==ScalarArrayName)
								{
									FEditorScalarParameterValue* Param = (FEditorScalarParameterValue*)(ElementData);
									OverrideName = (Param->ParameterName);
									PropertyWindowItem->ExpressionId = Param->ExpressionId;
								}
								else if(PropertyName==TextureArrayName)
								{
									FEditorTextureParameterValue* Param = (FEditorTextureParameterValue*)(ElementData);
									OverrideName = (Param->ParameterName);
									PropertyWindowItem->ExpressionId = Param->ExpressionId;
								}
								else if(PropertyName==FontArrayName)
								{
									FEditorFontParameterValue* Param = (FEditorFontParameterValue*)(ElementData);
									OverrideName = (Param->ParameterName);
									PropertyWindowItem->ExpressionId = Param->ExpressionId;
								}
								else if(PropertyName==VectorArrayName)
								{
									FEditorVectorParameterValue* Param = (FEditorVectorParameterValue*)(ElementData);
									OverrideName = (Param->ParameterName);
									PropertyWindowItem->ExpressionId = Param->ExpressionId;
								}
								else if(PropertyName==StaticSwitchArrayName)
								{
									FEditorStaticSwitchParameterValue* Param = (FEditorStaticSwitchParameterValue*)(ElementData);
									OverrideName = (Param->ParameterName);
									PropertyWindowItem->ExpressionId = Param->ExpressionId;
								}
								else if(PropertyName==StaticComponentMaskArrayName)
								{
									FEditorStaticComponentMaskParameterValue* Param = (FEditorStaticComponentMaskParameterValue*)(ElementData);
									OverrideName = (Param->ParameterName);
									PropertyWindowItem->ExpressionId = Param->ExpressionId;
								}

								WxMaterialInstanceTimeVaryingEditor *Win = wxDynamicCast(this->GetPropertyWindow()->GetGrandParent()->GetParent(), WxMaterialInstanceTimeVaryingEditor);
								check(Win);
								
								//All windows need to get created at start up
								//if(Win->ToolBar->GetToolState(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS) || MaterialInterface->VisibleExpressions.ContainsItem(ExpressionId))
								{
									// Add the property.
									PropertyWindowItem->PropertyStructName = PropertyName;
									PropertyWindowItem->SetDisplayName(OverrideName.ToString());

									FItemPropertyNode* NewTreeItemNode = new FItemPropertyNode;//;//CreatePropertyItem(ArrayProperty,i,this);
									NewTreeItemNode->InitNode(pwi, PropertyNode, MainWindow, StructMember, ArrayIdx*ArrayProperty->Inner->ElementSize+StructMember->Offset, ArrayIdx);
									PropertyNode->AddChildNode(NewTreeItemNode);
								}
							}
						}
					}
				}
			}
		}
	}
	PropertyNode->SetNodeFlags(EPropertyNodeFlags::SortChildren, TRUE);
}


//////////////////////////////////////////////////////////////////////////
//
//	WxMaterialInstanceTimeVaryingEditor
//
//////////////////////////////////////////////////////////////////////////

/**
 * wxWidgets Event Table
 */
BEGIN_EVENT_TABLE(WxMaterialInstanceTimeVaryingEditor, WxMaterialEditorBase)
	EVT_MENU(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_SYNCTOGB, OnMenuSyncToGB)
	EVT_MENU(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_OPENEDITOR, OnMenuOpenEditor)
	EVT_LIST_ITEM_ACTIVATED(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_LIST, OnInheritanceListDoubleClick)
	EVT_LIST_ITEM_RIGHT_CLICK(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_LIST, OnInheritanceListRightClick)
	EVT_TOOL(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS, OnShowAllMaterialParameters)
END_EVENT_TABLE()


WxMaterialInstanceTimeVaryingEditor::WxMaterialInstanceTimeVaryingEditor( wxWindow* Parent, wxWindowID id, UMaterialInterface* InMaterialInterface ) :	
        WxMaterialEditorBase( Parent, id, InMaterialInterface ),   
		FDockingParent(this)
{
	// Set the static mesh editor window title to include the static mesh being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("MaterialInstanceEditorCaption_F"), *InMaterialInterface->GetPathName()) ) );

	// Construct a temp holder for our instance parameters.
	UMaterialInstanceTimeVarying* InstanceTimeVarying = Cast<UMaterialInstanceTimeVarying>(InMaterialInterface);
	MaterialEditorInstance = ConstructObject<UMaterialEditorInstanceTimeVarying>(UMaterialEditorInstanceTimeVarying::StaticClass());
	MaterialEditorInstance->SetSourceInstance(InstanceTimeVarying);
	
	// Create toolbar
	ToolBar = new WxMaterialInstanceTimeVaryingEditorToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Create property window
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, this );
	PropertyWindow->SetFlags(EPropertyWindowFlags::SupportsCustomControls, TRUE);

	SetSize(1024,768);

	// Add inheritance list.
	InheritanceList = new WxListView(this, ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_LIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
	RebuildInheritanceList();

	// Add docking windows.
	{
		AddDockingWindow(PropertyWindow, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("PropertiesCaption_F")), *MaterialInterface->GetPathName())), *LocalizeUnrealEd(TEXT("Properties")));
		AddDockingWindow(InheritanceList, FDockingParent::DH_Left, *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("MaterialInstanceParent_F")), *MaterialInterface->GetPathName())), *LocalizeUnrealEd(TEXT("MaterialInstanceParent")));

		SetDockHostSize(FDockingParent::DH_Left, 300);

		AddDockingWindow( ( wxWindow* )PreviewWin, FDockingParent::DH_None, NULL );

		// Try to load a existing layout for the docking windows.
		LoadDockingLayout();
	}

	// Add docking menu
	wxMenuBar* MenuBar = new wxMenuBar();
	AppendWindowMenu(MenuBar);
	SetMenuBar(MenuBar);

	// Load editor settings.
	LoadSettings();

	// Set the preview mesh for the material.  This call must occur after the toolbar is initialized.
	if ( !SetPreviewMesh( *InMaterialInterface->PreviewMesh ) )
	{
		// The material preview mesh couldn't be found or isn't loaded.  Default to our primitive type.
		SetPrimitivePreview();
	}

	//move this lower, so the layout would already be established.  Otherwise windows will not clip to the parent window properly
	PropertyWindow->SetObject( MaterialEditorInstance, EPropertyWindowFlags::Sorted );
}

WxMaterialInstanceTimeVaryingEditor::~WxMaterialInstanceTimeVaryingEditor()
{
	SaveSettings();

	PropertyWindow->SetObject( NULL, EPropertyWindowFlags::NoFlags );
	delete PropertyWindow;
}

void WxMaterialInstanceTimeVaryingEditor::Serialize(FArchive& Ar)
{
	WxMaterialEditorBase::Serialize(Ar);

	// Serialize our custom object instance
	Ar<<MaterialEditorInstance;

	// Serialize all parent material instances that are stored in the list.
	Ar<<ParentList;
}

/** Pre edit change notify for properties. */
void WxMaterialInstanceTimeVaryingEditor::NotifyPreChange(void* Src, UProperty* PropertyThatChanged)
{
	// If they changed the parent, kill the current object in the property window since we will need to remake it.
	if(PropertyThatChanged->GetName()==TEXT("Parent"))
	{
		// Collapse all of the property arrays, since they will be changing with the new parent.
		PropertyWindow->CollapseItem(TEXT("VectorParameterValues"));
		PropertyWindow->CollapseItem(TEXT("ScalarParameterValues"));
		PropertyWindow->CollapseItem(TEXT("TextureParameterValues"));
		PropertyWindow->CollapseItem(TEXT("FontParameterValues"));
	}
}

/** Post edit change notify for properties. */
void WxMaterialInstanceTimeVaryingEditor::NotifyPostChange(void* Src, UProperty* PropertyThatChanged)
{
	// Update the preview window when the user changes a property.
	RefreshPreviewViewport();

	// If they changed the parent, regenerate the parent list.
	if(PropertyThatChanged->GetName()==TEXT("Parent"))
	{
		UBOOL bSetEmptyParent = FALSE;

		// Check to make sure they didnt set the parent to themselves.
		if(MaterialEditorInstance->Parent==MaterialInterface)
		{
			bSetEmptyParent = TRUE;
		}

		if (bSetEmptyParent)
		{
			MaterialEditorInstance->Parent = NULL;

			UMaterialInstanceTimeVarying* MITV = Cast<UMaterialInstanceTimeVarying>(MaterialInterface);
			if(MITV)
			{
				MITV->SetParent(NULL);
			}
		}

		RebuildInheritanceList();

		//We've invalidated the tree structure, rebuild it
		PropertyWindow->RequestReconnectToData();
	}

	//rebuild the property window to account for the possibility that the item changed was
	//a static switch
	if(PropertyThatChanged->Category == FName(TEXT("EditorStaticSwitchParameterValueOverTime")))
	{
		TArray<FGuid> PreviousExpressions(MaterialEditorInstance->VisibleExpressions);
		MaterialEditorInstance->VisibleExpressions.Empty();
		WxMaterialEditor::GetVisibleMaterialParameters(MaterialEditorInstance->Parent->GetMaterial(), MaterialEditorInstance->SourceInstance, MaterialEditorInstance->VisibleExpressions);
		
		// If there are any child materials, make sure they are refreshed
		if ( !bChildListBuilt )
		{
			RebuildChildList();
		}
		for ( INT i = 0; i < ChildList.Num(); i++ )
		{
			UMaterialInstance* pChild = ChildList( i );
			if ( pChild )
			{
				pChild->InitStaticPermutation();
			}
		}

		//check to see if it was the override button that was clicked or the value of the static switch
		//by comparing the values of the previous and current visible expression lists
		UBOOL bHasChanged = PreviousExpressions.Num() != MaterialEditorInstance->VisibleExpressions.Num();

		if(!bHasChanged)
		{
			for(INT Index = 0; Index < PreviousExpressions.Num(); ++Index)
			{
				if(PreviousExpressions(Index) != MaterialEditorInstance->VisibleExpressions(Index))
				{
					bHasChanged = TRUE;
					break;
				}
			}
		}

		if(bHasChanged)
		{
			PropertyWindow->PostRebuild();
		}
	}
}

/** Rebuilds the inheritance list for this material instance. */
void WxMaterialInstanceTimeVaryingEditor::RebuildInheritanceList()
{
	InheritanceList->DeleteAllColumns();
	InheritanceList->DeleteAllItems();
	ParentList.Empty();
	ChildList.Empty();
	bChildListBuilt = FALSE;

	InheritanceList->Freeze();
	{
		InheritanceList->InsertColumn(0, *LocalizeUnrealEd("Parent"));
		InheritanceList->InsertColumn(1, *LocalizeUnrealEd("Name"));

		// Travel up the parent chain for this material instance until we reach the root material.
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
		if(MaterialInstance)
		{
			long CurrentIdx = InheritanceList->InsertItem(0, *MaterialInstance->GetName());
			wxFont CurrentFont = InheritanceList->GetItemFont(CurrentIdx);
			CurrentFont.SetWeight(wxFONTWEIGHT_BOLD);
			InheritanceList->SetItemFont(CurrentIdx, CurrentFont);
			InheritanceList->SetItem(CurrentIdx, 1, *MaterialInstance->GetName());

			// Add all the parents
			RebuildParentList();

			// Add the parents to the inheritance list
			for ( INT i = ParentList.Num() - 2; i >= 0; i-- )
			{
				UMaterialInterface* pParent = ParentList( i );
				if ( pParent )
				{
					long ItemIdx = InheritanceList->InsertItem(0, *(pParent->GetName()));
					InheritanceList->SetItem(ItemIdx, 1, *pParent->GetName());
				}
			}

			// Add all children
			// RebuildChildList();

			// Loop through all the items and set their first column.
			INT NumItems = InheritanceList->GetItemCount();
			for(INT ItemIdx=0; ItemIdx<NumItems; ItemIdx++)
			{
				if(ItemIdx==0)
				{
					InheritanceList->SetItem(ItemIdx, 0, *LocalizeUnrealEd("Material"));
				}
				else
				{
					if(ItemIdx < NumItems - 1)
					{
						InheritanceList->SetItem(ItemIdx,0,
							*FString::Printf(TEXT("%s %i"), *LocalizeUnrealEd("Parent"), NumItems-1-ItemIdx));
					}
					else
					{
						InheritanceList->SetItem(ItemIdx, 0, *LocalizeUnrealEd("Current"));
					}
				}
			}
		}

		// Autosize columns
		InheritanceList->SetColumnWidth(0, wxLIST_AUTOSIZE);
		InheritanceList->SetColumnWidth(1, wxLIST_AUTOSIZE);
	}
	InheritanceList->Thaw();
}

/** Rebuilds the parent list for this material instance. */
void WxMaterialInstanceTimeVaryingEditor::RebuildParentList()
{
	ParentList.Empty();

	// Travel up the parent chain for this material instance until we reach the root material.
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
	if(MaterialInstance)
	{
		// Add all parents
		UMaterialInstance* Instance = MaterialInstance;
		ParentList.AddItem(Instance);

		UMaterialInterface* Parent = Instance->Parent;
		while(Parent && Parent != Instance)
		{
			ParentList.InsertItem(Parent,0);

			// If the parent is a material then break.
			Instance = Cast<UMaterialInstance>(Parent);

			if(Instance)
			{
				Parent = Instance->Parent;
			}
			else
			{
				break;
			}
		}
	}
}

/** Rebuilds the child list for this material instance. */
void WxMaterialInstanceTimeVaryingEditor::RebuildChildList()
{
	ChildList.Empty();

	// Travel up the parent chain for this material instance until we reach the root material.
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(MaterialInterface);
	if(MaterialInstance)
	{
		// Add all children
		for( TObjectIterator<UMaterialInstance> It; It; ++It )
		{
			UMaterialInstance* Instance = *It;
			if ( Instance != MaterialInstance )
			{
				UMaterialInstance* Parent = Cast<UMaterialInstance>(Instance->Parent);
				while(Parent && Parent != Instance)
				{
					if ( Parent == MaterialInstance )
					{
						ChildList.AddItem( Instance );
						break;
					}
					else
					{
						Instance = Parent;
						Parent = Cast<UMaterialInstance>(Instance->Parent);
					}
				}
			}
		}
	}

	bChildListBuilt = TRUE;
}

/** Saves editor settings. */
void WxMaterialInstanceTimeVaryingEditor::SaveSettings()
{
	SaveDockingLayout();

	// Save the preview scene
	check(PreviewVC);
	check(PreviewVC->PreviewScene);
	PreviewVC->PreviewScene->SaveSettings(TEXT("MaterialInstanceEditor"));	
		
	// Save window position/size.
	FWindowUtil::SavePosSize( TEXT("MaterialInstanceEditor"), this );

	GConfig->SetBool(TEXT("MaterialInstanceEditor"), TEXT("bShowGrid"), bShowGrid, GEditorUserSettingsIni);
	GConfig->SetBool(TEXT("MaterialInstanceEditor"), TEXT("bDrawGrid"), PreviewVC->IsRealtime(), GEditorUserSettingsIni);
	GConfig->SetInt(TEXT("MaterialInstanceEditor"), TEXT("PrimType"), PreviewPrimType, GEditorUserSettingsIni);
}

/** Loads editor settings. */
void WxMaterialInstanceTimeVaryingEditor::LoadSettings()
{
	// Load the desired window position from .ini file.
	FWindowUtil::LoadPosSize( TEXT("MaterialInstanceEditor"), this, 64,64,800,450 );

	UBOOL bRealtime=FALSE;
	GConfig->GetBool(TEXT("MaterialInstanceEditor"), TEXT("bShowGrid"), bShowGrid, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("MaterialInstanceEditor"), TEXT("bDrawGrid"), bRealtime, GEditorUserSettingsIni);

	INT PrimType;
	if(GConfig->GetInt(TEXT("MaterialInstanceEditor"), TEXT("PrimType"), PrimType, GEditorUserSettingsIni))
	{
		PreviewPrimType = (EThumbnailPrimType)PrimType;
	}
	else
	{
		PreviewPrimType = TPT_Sphere;
	}

	if(PreviewVC)
	{
		PreviewVC->SetShowGrid(bShowGrid);
		PreviewVC->SetRealtime(bRealtime);

		if(PreviewVC->PreviewScene)
		{
			// Load the preview scene
			PreviewVC->PreviewScene->LoadSettings(TEXT("MaterialInstanceEditor"));
		}
	}
}

/** Syncs the GB to the selected parent in the inheritance list. */
void WxMaterialInstanceTimeVaryingEditor::SyncSelectedParentToGB()
{
	INT SelectedItem = (INT)InheritanceList->GetFirstSelected();
	if(ParentList.IsValidIndex(SelectedItem))
	{
		UMaterialInterface* SelectedMaterialInstance = ParentList(SelectedItem);
		TArray<UObject*> Objects;

		Objects.AddItem(SelectedMaterialInstance);
		GApp->EditorFrame->SyncBrowserToObjects(Objects);
	}
}

/** Opens the editor for the selected parent. */
void WxMaterialInstanceTimeVaryingEditor::OpenSelectedParentEditor()
{
	INT SelectedItem = (INT)InheritanceList->GetFirstSelected();
	if(ParentList.IsValidIndex(SelectedItem))
	{
		UMaterialInterface* SelectedMaterialInstance = ParentList(SelectedItem);

		// See if its a material or material instance constant.  Don't do anything if the user chose the current material instance.
		if(MaterialInterface!=SelectedMaterialInstance)
		{
			if(SelectedMaterialInstance->IsA(UMaterial::StaticClass()))
			{
				// Show material editor
				UMaterial* Material = Cast<UMaterial>(SelectedMaterialInstance);
				OpenMaterialInMaterialEditor(Material);
			}
			else if(SelectedMaterialInstance->IsA(UMaterialInstanceTimeVarying::StaticClass()))
			{
				// Show material instance editor
				UMaterialInstanceTimeVarying* MaterialInstanceTimeVarying = Cast<UMaterialInstanceTimeVarying>(SelectedMaterialInstance);
				wxFrame* MaterialInstanceEditor = new WxMaterialInstanceTimeVaryingEditor( (wxWindow*)GApp->EditorFrame,-1, MaterialInstanceTimeVarying );
				MaterialInstanceEditor->SetSize(1024,768);
				MaterialInstanceEditor->Show();
			}
		}
	}
}

/** Event handler for when the user wants to sync the GB to the currently selected parent. */
void WxMaterialInstanceTimeVaryingEditor::OnMenuSyncToGB(wxCommandEvent &Event)
{
	SyncSelectedParentToGB();
}

/** Event handler for when the user wants to open the editor for the selected parent material. */
void WxMaterialInstanceTimeVaryingEditor::OnMenuOpenEditor(wxCommandEvent &Event)
{
	OpenSelectedParentEditor();
}

/** Double click handler for the inheritance list. */
void WxMaterialInstanceTimeVaryingEditor::OnInheritanceListDoubleClick(wxListEvent &ListEvent)
{
	OpenSelectedParentEditor();
}

/** Event handler for when the user right clicks on the inheritance list. */
void WxMaterialInstanceTimeVaryingEditor::OnInheritanceListRightClick(wxListEvent &ListEvent)
{
	INT SelectedItem = (INT)InheritanceList->GetFirstSelected();
	if(ParentList.IsValidIndex(SelectedItem))
	{
		UMaterialInterface* SelectedMaterialInstance = ParentList(SelectedItem);

		wxMenu* ContextMenu = new wxMenu();

		if(SelectedMaterialInstance != MaterialInterface)
		{
			FString Label;

			if(SelectedMaterialInstance->IsA(UMaterial::StaticClass()))
			{
				Label = LocalizeUnrealEd("MaterialEditor");
			}
			else
			{
				Label = LocalizeUnrealEd("MaterialInstanceEditor");
			}

			ContextMenu->Append(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_OPENEDITOR, *Label);
		}

		ContextMenu->Append(ID_MATERIALINSTANCE_TIME_VARYING_EDITOR_SYNCTOGB, *LocalizeUnrealEd("SyncContentBrowser"));

		PopupMenu(ContextMenu);
	}
}

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxMaterialInstanceTimeVaryingEditor::GetDockingParentName() const
{
	return TEXT("MaterialInstanceEditor");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxMaterialInstanceTimeVaryingEditor::GetDockingParentVersion() const
{
	return 1;
}

/** Event handler for when the user wants to toggle showing all material parameters. */
void WxMaterialInstanceTimeVaryingEditor::OnShowAllMaterialParameters(wxCommandEvent &Event)
{
	PropertyWindow->RequestMainWindowTakeFocus();
	PropertyWindow->PostRebuild();
}

UObject* WxMaterialInstanceTimeVaryingEditor::GetSyncObject()
{
	if (MaterialEditorInstance)
	{
		return MaterialEditorInstance->SourceInstance;
	}
	return NULL;
}

