/**
 * MaterialInstanceConstantEditor.cpp: Material instance editor class.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "PropertyWindow.h"
#include "MaterialEditorBase.h"
#include "MaterialEditorToolBar.h"
#include "MaterialInstanceConstantEditor.h"
#include "MaterialEditorPreviewScene.h"
#include "NewMaterialEditor.h"
#include "PropertyWindowManager.h"	// required for access to GPropertyWindowManager


//////////////////////////////////////////////////////////////////////////
// UMaterialEditorInstanceConstant
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS(UMaterialEditorInstanceConstant);

//IMPLEMENT_CLASS(UMaterialEditorInstanceConstantGroup);
IMPLEMENT_CLASS(UDEditorParameterValue);
IMPLEMENT_CLASS(UDEditorVectorParameterValue);
IMPLEMENT_CLASS(UDEditorTextureParameterValue);
IMPLEMENT_CLASS(UDEditorScalarParameterValue);
IMPLEMENT_CLASS(UDEditorStaticSwitchParameterValue);
IMPLEMENT_CLASS(UDEditorStaticComponentMaskParameterValue);
IMPLEMENT_CLASS(UDEditorFontParameterValue);

/**Fix up for deprecated properties*/
void UMaterialEditorInstanceConstant::PostLoad ()
{
	Super::PostLoad();

	if (FlattenedTexture_DEPRECATED != NULL)
	{
		FlattenedTexture_DEPRECATED = NULL;
	}
}

void UMaterialEditorInstanceConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SourceInstance)
	{
		UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
		if(PropertyThatChanged && PropertyThatChanged->GetName()==TEXT("Parent"))
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

		if (GEmulateMobileRendering == TRUE)
		{
			// Update any MIC that have this as their parent
			FMobileEmulationMaterialManager::GetManager()->UpdateMaterialInterface(SourceInstance, FALSE, TRUE);
		}
	}
}
/** 
*  Creates/adds value to group retrieved from parent material . 
*
* @param ParentMaterial		Name of material to search for groups.
* @param ParameterValue		Current data to be grouped
*/
void  UMaterialEditorInstanceConstant::AssignParameterToGroup(UMaterial* ParentMaterial, UDEditorParameterValue * ParameterValue)
{
	check(ParentMaterial);
	check(ParameterValue);

	FName ParameterGroupName;
	ParentMaterial->GetGroupName(ParameterValue->ParameterName, ParameterGroupName);

	if (ParameterGroupName == TEXT("") || ParameterGroupName == TEXT("None"))
	{
		if (bUseOldStyleMICEditorGroups == TRUE)
		{
			if (Cast<UDEditorVectorParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Vector Parameter Values");
			}
			else if (Cast<UDEditorTextureParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Texture Parameter Values");
			}
			else if (Cast<UDEditorScalarParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Scalar Parameter Values");
			}
			else if (Cast<UDEditorStaticSwitchParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Static Switch Parameter Values");
			}
			else if (Cast<UDEditorStaticComponentMaskParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Static Component Mask Parameter Values");
			}
			else if (Cast<UDEditorFontParameterValue>( ParameterValue))
			{
				ParameterGroupName = TEXT("Font Parameter Values");
			}
			else
			{
				ParameterGroupName = TEXT("None");
			}
		}
		else
		{
			ParameterGroupName = TEXT("None");
		}

	}

	FEditorParameterGroup& CurrentGroup = GetParameterGroup(ParameterGroupName, ParameterGroups);
	CurrentGroup.Parameters.AddItem(ParameterValue);
}
/** 
	*  Returns group for parameter. Creates one if needed. 
	*
	* @param	InParameterGroupName	Name to be looked for.
	* @param	InParameterGroups		The array of groups to look in
	*/
FEditorParameterGroup& UMaterialEditorInstanceConstant::GetParameterGroup(FName& InParameterGroupName, TArrayNoInit<struct FEditorParameterGroup>& InParameterGroups)
{	
	if (InParameterGroupName == TEXT(""))
	{
		InParameterGroupName = TEXT("None");
	}

	for (INT i = 0; i < InParameterGroups.Num(); i ++)
	{
		FEditorParameterGroup& Group= InParameterGroups(i);
		if (Group.GroupName == InParameterGroupName)
		{
			return Group;
		}
	}
	
	INT Index = InParameterGroups.AddZeroed(1);
	FEditorParameterGroup& Group= InParameterGroups(Index);
	Group.GroupName = InParameterGroupName;
	return Group;
}

IMPLEMENT_COMPARE_CONSTREF( FEditorParameterGroup, SortGroupsByName, 
{ 
	FString AName = A.GroupName.ToString().ToLower();
	FString BName = B.GroupName.ToString().ToLower();
	if (AName == TEXT("none"))
	{
		return 1;
	}
	if (BName == TEXT("none"))
	{
		return 0;
	}

	return appStricmp( *AName, *BName ); 
})

IMPLEMENT_COMPARE_POINTER( UDEditorParameterValue, SortParametersInGroupsByName, 
{
	FString AName = A->ParameterName.ToString().ToLower();
	FString BName = B->ParameterName.ToString().ToLower();

	return appStricmp( *AName, *BName ); 
} )

/** Regenerates the parameter arrays. */
void UMaterialEditorInstanceConstant::RegenerateArrays()
{
	VisibleExpressions.Empty();
	TArray<FEditorParameterGroup> ParameterGroupBackup = ParameterGroups;
	ParameterGroups.Empty();

	if(Parent)
	{	
		// Only operate on base materials
		UMaterial* ParentMaterial = Parent->GetMaterial();
		SourceInstance->UpdateParameterNames();	// Update any parameter names that may have changed.

		WxMaterialEditor::GetVisibleMaterialParameters(ParentMaterial, SourceInstance, VisibleExpressions);

		// Loop through all types of parameters for this material and add them to the parameter arrays.
		TArray<FName> ParameterNames;
		TArray<FGuid> Guids;
		ParentMaterial->GetAllVectorParameterNames(ParameterNames, Guids);

		// Vector Parameters.

		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FGuid ParamGuid = Guids(ParameterIdx);
			if (!VisibleExpressions.ContainsItem(ParamGuid))
			{
				// ignore params that aren't visible because they aren't connected to the actual material
				continue;
			}

			UDEditorVectorParameterValue & ParameterValue = *(ConstructObject<UDEditorVectorParameterValue>(UDEditorVectorParameterValue::StaticClass()));
			FName ParameterName = ParameterNames(ParameterIdx);
			FLinearColor Value;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = ParamGuid;

			if(SourceInstance->GetVectorParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}

			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->VectorParameterValues.Num(); ParameterIdx++)
			{
				FVectorParameterValue& SourceParam = SourceInstance->VectorParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}
		// Scalar Parameters.
		ParentMaterial->GetAllScalarParameterNames(ParameterNames, Guids);
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{		
			FGuid ParamGuid = Guids(ParameterIdx);
			if (!VisibleExpressions.ContainsItem(ParamGuid))
			{
				// ignore params that aren't visible because they aren't connected to the actual material
				continue;
			}

			UDEditorScalarParameterValue & ParameterValue = *(ConstructObject<UDEditorScalarParameterValue>(UDEditorScalarParameterValue::StaticClass()));
			FName ParameterName = ParameterNames(ParameterIdx);
			FLOAT Value;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = ParamGuid;

			if(SourceInstance->GetScalarParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->ScalarParameterValues.Num(); ParameterIdx++)
			{
				FScalarParameterValue& SourceParam = SourceInstance->ScalarParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Texture Parameters.
		ParentMaterial->GetAllTextureParameterNames(ParameterNames, Guids);
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{	
			FGuid ParamGuid = Guids(ParameterIdx);
			if (!VisibleExpressions.ContainsItem(ParamGuid))
			{
				// ignore params that aren't visible because they aren't connected to the actual material
				continue;
			}

			UDEditorTextureParameterValue& ParameterValue = *(ConstructObject<UDEditorTextureParameterValue>(UDEditorTextureParameterValue::StaticClass()));
			FName ParameterName = ParameterNames(ParameterIdx);
			UTexture* Value;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = ParamGuid;

			if(SourceInstance->GetTextureParameterValue(ParameterName, Value))
			{
				ParameterValue.ParameterValue = Value;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->TextureParameterValues.Num(); ParameterIdx++)
			{
				FTextureParameterValue& SourceParam = SourceInstance->TextureParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.ParameterValue = SourceParam.ParameterValue;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Font Parameters.
		ParentMaterial->GetAllFontParameterNames(ParameterNames, Guids);
		for(INT ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FGuid ParamGuid = Guids(ParameterIdx);
			if (!VisibleExpressions.ContainsItem(ParamGuid))
			{
				// ignore params that aren't visible because they aren't connected to the actual material
				continue;
			}

			UDEditorFontParameterValue& ParameterValue = *(ConstructObject<UDEditorFontParameterValue>(UDEditorFontParameterValue::StaticClass()));
			FName ParameterName = ParameterNames(ParameterIdx);
			UFont* FontValue;
			INT FontPage;

			ParameterValue.bOverride = FALSE;
			ParameterValue.ParameterName = ParameterName;
			ParameterValue.ExpressionId = ParamGuid;

			if(SourceInstance->GetFontParameterValue(ParameterName, FontValue,FontPage))
			{
				ParameterValue.FontValue = FontValue;
				ParameterValue.FontPage = FontPage;
			}


			// @todo: This is kind of slow, maybe store these in a map for lookup?
			// See if this keyname exists in the source instance.
			for(INT ParameterIdx=0; ParameterIdx<SourceInstance->FontParameterValues.Num(); ParameterIdx++)
			{
				FFontParameterValue& SourceParam = SourceInstance->FontParameterValues(ParameterIdx);
				if(ParameterName==SourceParam.ParameterName)
				{
					ParameterValue.bOverride = TRUE;
					ParameterValue.FontValue = SourceParam.FontValue;
					ParameterValue.FontPage = SourceParam.FontPage;
				}
			}
			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}

		// Get all static parameters from the source instance.  This will handle inheriting parent values.
		FStaticParameterSet SourceStaticParameters;
		SourceInstance->GetStaticParameterValues(&SourceStaticParameters);

		// Copy Static Switch Parameters
		for(INT ParameterIdx=0; ParameterIdx<SourceStaticParameters.StaticSwitchParameters.Num(); ParameterIdx++)
		{
			FGuid ParamGuid = SourceStaticParameters.StaticSwitchParameters(ParameterIdx).ExpressionGUID;
			if (!VisibleExpressions.ContainsItem(ParamGuid))
			{
				// ignore params that aren't visible because they aren't connected to the actual material
				continue;
			}

			FString ParameterName;
			SourceStaticParameters.StaticSwitchParameters(ParameterIdx).ParameterName.ToString(ParameterName);
			if (ParameterName.InStr(*ULandscapeMaterialInstanceConstant::LandscapeVisibilitySwitchName) == INDEX_NONE )
			{
				FStaticSwitchParameter StaticSwitchParameterValue = FStaticSwitchParameter(SourceStaticParameters.StaticSwitchParameters(ParameterIdx));
				UDEditorStaticSwitchParameterValue* ParameterValue = NULL;

				// Switch parameters maintain their values when interface is rebuilt, this is because switches
				// cause a rebuild on activation, Forcing them back to their original value. This is incorrect.
				for ( INT GroupIdx = 0; !ParameterValue && GroupIdx < ParameterGroupBackup.Num(); GroupIdx++)
				{
					FEditorParameterGroup& Group = ParameterGroupBackup(GroupIdx);
					for (INT PropertyIdx = 0; PropertyIdx < Group.Parameters.Num(); PropertyIdx++)
					{
						UDEditorStaticSwitchParameterValue* Param = Cast<UDEditorStaticSwitchParameterValue>(Group.Parameters(PropertyIdx));
						if (Param && Param->ExpressionId == StaticSwitchParameterValue.ExpressionGUID)
						{
							ParameterValue = Param;
							break;
						}
					}
				}

				// If no existing parameter value is found, Create a new one and initialize form the source.
				if (!ParameterValue)
				{
					ParameterValue = ConstructObject<UDEditorStaticSwitchParameterValue>(UDEditorStaticSwitchParameterValue::StaticClass());

					ParameterValue->ParameterValue = StaticSwitchParameterValue.Value;
					ParameterValue->bOverride = StaticSwitchParameterValue.bOverride;
					ParameterValue->ParameterName = StaticSwitchParameterValue.ParameterName;
					ParameterValue->ExpressionId = StaticSwitchParameterValue.ExpressionGUID;
				}
				
				AssignParameterToGroup(ParentMaterial, ParameterValue);
			}
		}

		// Copy Static Component Mask Parameters

		for(INT ParameterIdx=0; ParameterIdx<SourceStaticParameters.StaticComponentMaskParameters.Num(); ParameterIdx++)
		{
			FStaticComponentMaskParameter StaticComponentMaskParameterValue = FStaticComponentMaskParameter(SourceStaticParameters.StaticComponentMaskParameters(ParameterIdx));

			FGuid ParamGuid = StaticComponentMaskParameterValue.ExpressionGUID;
			if (!VisibleExpressions.ContainsItem(ParamGuid))
			{
				// ignore params that aren't visible because they aren't connected to the actual material
				continue;
			}

			UDEditorStaticComponentMaskParameterValue& ParameterValue = *(ConstructObject<UDEditorStaticComponentMaskParameterValue>(UDEditorStaticComponentMaskParameterValue::StaticClass()));
			ParameterValue.ParameterValue.R = StaticComponentMaskParameterValue.R;
			ParameterValue.ParameterValue.G = StaticComponentMaskParameterValue.G;
			ParameterValue.ParameterValue.B = StaticComponentMaskParameterValue.B;
			ParameterValue.ParameterValue.A = StaticComponentMaskParameterValue.A;
			ParameterValue.bOverride =StaticComponentMaskParameterValue.bOverride;
			ParameterValue.ParameterName =StaticComponentMaskParameterValue.ParameterName;
			ParameterValue.ExpressionId= StaticComponentMaskParameterValue.ExpressionGUID;

			AssignParameterToGroup(ParentMaterial, Cast<UDEditorParameterValue>(&ParameterValue));
		}
	}
	// sort contents of groups
	for(INT ParameterIdx=0; ParameterIdx<ParameterGroups.Num(); ParameterIdx++)
	{
		FEditorParameterGroup & ParamGroup = ParameterGroups(ParameterIdx);
		Sort<USE_COMPARE_POINTER(UDEditorParameterValue, SortParametersInGroupsByName)>( &ParamGroup.Parameters(0), ParamGroup.Parameters.Num() );
	}	
	
	// sort groups itself pushing defaults to end
	Sort<USE_COMPARE_CONSTREF(FEditorParameterGroup, SortGroupsByName)>( ParameterGroups.GetTypedData(), ParameterGroups.Num() );
	TArray<struct FEditorParameterGroup> ParameterDefaultGroups;
	for(INT ParameterIdx=ParameterGroups.Num() - 1; ParameterIdx >= 0; ParameterIdx--)
	{
		FEditorParameterGroup & ParamGroup = ParameterGroups(ParameterIdx);
		if (bUseOldStyleMICEditorGroups == false)
		{			
			if (ParamGroup.GroupName ==  TEXT("None"))
			{
				ParameterDefaultGroups.AddItem(ParamGroup);
				ParameterGroups.Remove(ParameterIdx);
				break;
			}
		}
		else
		{
			if (ParamGroup.GroupName ==  TEXT("Vector Parameter Values") || 
				ParamGroup.GroupName ==  TEXT("Scalar Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Texture Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Static Switch Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Static Component Mask Parameter Values") ||
				ParamGroup.GroupName ==  TEXT("Font Parameter Values"))
			{
				ParameterDefaultGroups.AddItem(ParamGroup);
				ParameterGroups.Remove(ParameterIdx);
			}

		}
	}
	if (ParameterDefaultGroups.Num() >0)
	{
		ParameterGroups.Append(ParameterDefaultGroups);
	}

	//@todo. Make this disable-able in the ini?
	RegenerateMobileArrays();
}

UDEditorVectorParameterValue* MICEd_GetVectorParameter(FName& InParameterName)
{
	UDEditorVectorParameterValue* ParameterValue = ConstructObject<UDEditorVectorParameterValue>(UDEditorVectorParameterValue::StaticClass());
	ParameterValue->ParameterName = InParameterName;
	ParameterValue->ExpressionId = FGuid(0x8000, 0, 0, 0);
	return ParameterValue;
}

UDEditorScalarParameterValue* MICEd_GetScalarParameter(FName& InParameterName)
{
	UDEditorScalarParameterValue* ParameterValue = ConstructObject<UDEditorScalarParameterValue>(UDEditorScalarParameterValue::StaticClass());
	ParameterValue->ParameterName = InParameterName;
	ParameterValue->ExpressionId = FGuid(0x8000, 0, 0, 0);
	return ParameterValue;
}

UDEditorTextureParameterValue* MICEd_GetTextureParameter(FName& InParameterName)
{
	UDEditorTextureParameterValue* ParameterValue = ConstructObject<UDEditorTextureParameterValue>(UDEditorTextureParameterValue::StaticClass());
	ParameterValue->ParameterName = InParameterName;
	ParameterValue->ExpressionId = FGuid(0x8000, 0, 0, 0);
	return ParameterValue;
}

/** Copies the parameter array values back to the source instance. */
void UMaterialEditorInstanceConstant::CopyToSourceInstance()
{
	if(SourceInstance->IsTemplate(RF_ClassDefaultObject) == FALSE )
	{
		SourceInstance->MarkPackageDirty();
		SourceInstance->ClearParameterValues();

		// Scalar Parameters
		for(INT GroupIdx=0; GroupIdx<ParameterGroups.Num(); GroupIdx++)
		{
			FEditorParameterGroup & Group = ParameterGroups(GroupIdx);
			for(INT ParameterIdx=0; ParameterIdx<Group.Parameters.Num(); ParameterIdx++)
			{
				if (Group.Parameters(ParameterIdx) == NULL)
				{
					continue;
				}
				UDEditorScalarParameterValue * ScalarParameterValue = Cast<UDEditorScalarParameterValue>(Group.Parameters(ParameterIdx));
				if (ScalarParameterValue)
				{
					if(ScalarParameterValue->bOverride)
					{
						SourceInstance->SetScalarParameterValue(ScalarParameterValue->ParameterName, ScalarParameterValue->ParameterValue);
						continue;
					}
				}
				UDEditorFontParameterValue * FontParameterValue = Cast<UDEditorFontParameterValue>(Group.Parameters(ParameterIdx));
				if (FontParameterValue)
				{
					if(FontParameterValue->bOverride)
					{
						SourceInstance->SetFontParameterValue(FontParameterValue->ParameterName,FontParameterValue->FontValue,FontParameterValue->FontPage);
						continue;
					}
				}

				UDEditorTextureParameterValue * TextureParameterValue = Cast<UDEditorTextureParameterValue>(Group.Parameters(ParameterIdx));
				if (TextureParameterValue)
				{
					if(TextureParameterValue->bOverride)
					{
						SourceInstance->SetTextureParameterValue(TextureParameterValue->ParameterName, TextureParameterValue->ParameterValue);
						continue;
					}
				}
				UDEditorVectorParameterValue * VectorParameterValue = Cast<UDEditorVectorParameterValue>(Group.Parameters(ParameterIdx));
				if (VectorParameterValue)
				{
					if(VectorParameterValue->bOverride)
					{
						SourceInstance->SetVectorParameterValue(VectorParameterValue->ParameterName, VectorParameterValue->ParameterValue);
						continue;
					}
				}

			}
		}
		CopyStaticParametersToSourceInstance();

		// Copy phys material back to source instance
		SourceInstance->PhysMaterial = PhysMaterial;

		// Copy physical material mask info back to the source instance
		SourceInstance->PhysMaterialMask = PhysicalMaterialMask.PhysMaterialMask;
		SourceInstance->PhysMaterialMaskUVChannel = PhysicalMaterialMask.PhysMaterialMaskUVChannel;
		SourceInstance->BlackPhysicalMaterial = PhysicalMaterialMask.BlackPhysicalMaterial;
		SourceInstance->WhitePhysicalMaterial = PhysicalMaterialMask.WhitePhysicalMaterial;

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
		VisibleExpressions.Empty();
		
		// force refresh of visibility of properties
		if( Parent )
		{
			UMaterial* ParentMaterial = Parent->GetMaterial();
			WxMaterialEditor::GetVisibleMaterialParameters(ParentMaterial, SourceInstance, VisibleExpressions);
		}

		// Copy mobile settings
		CopyMobileParametersToSourceInstance();
	}
}

/** Copies static parameters to the source instance, which will be marked dirty if a compile was necessary */
void UMaterialEditorInstanceConstant::CopyStaticParametersToSourceInstance()
{
	if(SourceInstance->IsTemplate(RF_ClassDefaultObject) == FALSE )
	{
		//build a static parameter set containing all static parameter settings
		FStaticParameterSet StaticParameters;

		for(INT GroupIdx=0; GroupIdx<ParameterGroups.Num(); GroupIdx++)
		{
			FEditorParameterGroup & Group = ParameterGroups(GroupIdx);
			for(INT ParameterIdx=0; ParameterIdx<Group.Parameters.Num(); ParameterIdx++)
			{
				if (Group.Parameters(ParameterIdx) == NULL)
				{
					continue;
				}
				// static switch

				UDEditorStaticSwitchParameterValue * StaticSwitchParameterValue = Cast<UDEditorStaticSwitchParameterValue>(Group.Parameters(ParameterIdx));
				if (StaticSwitchParameterValue)
				{

					UBOOL SwitchValue = StaticSwitchParameterValue->ParameterValue;
					FGuid ExpressionIdValue = StaticSwitchParameterValue->ExpressionId;
					if (!StaticSwitchParameterValue->bOverride)
					{
						if (Parent)
						{
							//use the parent's settings if this parameter is not overridden
							SourceInstance->Parent->GetStaticSwitchParameterValue(StaticSwitchParameterValue->ParameterName, SwitchValue, ExpressionIdValue);
						}
					}
					FStaticSwitchParameter * NewParameter = 
						new(StaticParameters.StaticSwitchParameters) FStaticSwitchParameter(StaticSwitchParameterValue->ParameterName, SwitchValue, StaticSwitchParameterValue->bOverride, ExpressionIdValue);

				}

				// static component mask

				UDEditorStaticComponentMaskParameterValue * StaticComponentMaskParameterValue = Cast<UDEditorStaticComponentMaskParameterValue>(Group.Parameters(ParameterIdx));
				if (StaticComponentMaskParameterValue)
				{
					UBOOL MaskR = StaticComponentMaskParameterValue->ParameterValue.R;
					UBOOL MaskG = StaticComponentMaskParameterValue->ParameterValue.G;
					UBOOL MaskB = StaticComponentMaskParameterValue->ParameterValue.B;
					UBOOL MaskA = StaticComponentMaskParameterValue->ParameterValue.A;
					FGuid ExpressionIdValue = StaticComponentMaskParameterValue->ExpressionId;

					if (!StaticComponentMaskParameterValue->bOverride)
					{
						if (Parent)
						{
							//use the parent's settings if this parameter is not overridden
							SourceInstance->Parent->GetStaticComponentMaskParameterValue(StaticComponentMaskParameterValue->ParameterName, MaskR, MaskG, MaskB, MaskA, ExpressionIdValue);
						}
					}
					FStaticComponentMaskParameter * NewParameter = new(StaticParameters.StaticComponentMaskParameters) 
						FStaticComponentMaskParameter(StaticComponentMaskParameterValue->ParameterName, MaskR, MaskG, MaskB, MaskA, StaticComponentMaskParameterValue->bOverride, ExpressionIdValue);
				}

				// static component mask
				if (Parent)
				{
					UDEditorTextureParameterValue * TextureParameterValue = Cast<UDEditorTextureParameterValue>(Group.Parameters(ParameterIdx));
					if (TextureParameterValue)
					{
						// Try to read the parent's settings of this parameter
						FGuid ExpressionIdValue;
						BYTE CompressionSettings;
						if( SourceInstance->Parent->GetNormalParameterValue(TextureParameterValue->ParameterName, CompressionSettings, ExpressionIdValue) )
						{
							// Succeeded! This texture parameter is a normal map parameter.
							// Use the values set in the editor if we should override.
							if( TextureParameterValue->bOverride && TextureParameterValue->ParameterValue )
							{
								CompressionSettings = TextureParameterValue->ParameterValue->CompressionSettings;
								ExpressionIdValue = TextureParameterValue->ExpressionId;
							}
							FNormalParameter* NewParameter = new(StaticParameters.NormalParameters) 
								FNormalParameter(TextureParameterValue->ParameterName, CompressionSettings, TextureParameterValue->bOverride, ExpressionIdValue);
						}
					}
				}
			}
		}
		if (SourceInstance->SetStaticParameterValues(&StaticParameters))
		{
			//mark the package dirty if a compile was needed
			SourceInstance->MarkPackageDirty();
		}
	}
}


/**  
 * Sets the source instance for this object and regenerates arrays. 
 *
 * @param MaterialInterface		Instance to use as the source for this material editor instance.
 */
void UMaterialEditorInstanceConstant::SetSourceInstance(UMaterialInstanceConstant* MaterialInterface)
{
	check(MaterialInterface);
	SourceInstance = MaterialInterface;
	Parent = SourceInstance->Parent;
	PhysMaterial = SourceInstance->PhysMaterial;

	// Copy physical material mask info from the source instance
	PhysicalMaterialMask.PhysMaterialMask = SourceInstance->PhysMaterialMask;
	PhysicalMaterialMask.PhysMaterialMaskUVChannel = SourceInstance->PhysMaterialMaskUVChannel;
	PhysicalMaterialMask.BlackPhysicalMaterial = SourceInstance->BlackPhysicalMaterial;
	PhysicalMaterialMask.WhitePhysicalMaterial = SourceInstance->WhitePhysicalMaterial;

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

/** Regenerates the mobile parameter arrays. */
void UMaterialEditorInstanceConstant::RegenerateMobileArrays()
{
	MobileParameterGroups.Empty();

	// Make this an ini setting?
	if (TRUE)
	{
		TArray<FName> MobileGroupNames;

		UMaterial::GetMobileParameterGroupNames(MobileGroupNames);
		for (INT GroupIdx = 0; GroupIdx < MobileGroupNames.Num(); GroupIdx++)
		{
			FName GroupName = MobileGroupNames(GroupIdx);
			if (GenerateMobileParameterEntries(GroupName) == FALSE)
			{
				warnf(NAME_Warning, TEXT("Failed to generate mobile parameter entries for %s"), *(GroupName.ToString()));
			}
		}
		VisibleExpressions.AddItem(FGuid(0x8000, 0, 0, 0));
	}
}

/** 
 *	Generate the mobile parameter entries for the given group.
 *
 *	@param	InGroupName		The group to generate
 */
UBOOL UMaterialEditorInstanceConstant::GenerateMobileParameterEntries(FName& InGroupName)
{
	UMaterial* AbsoluteParent = SourceInstance->GetMaterial();
	if (AbsoluteParent != NULL)
	{
		if (AbsoluteParent->IsMobileGroupEnabled(InGroupName) == TRUE)
		{
			FEditorParameterGroup& ParamGroup = GetParameterGroup(InGroupName, MobileParameterGroups);
			// Make sure it's empty...
			ParamGroup.Parameters.Empty();

			TArray<FName> ScalarParameterNames;
			TArray<FName> VectorParameterNames;
			TArray<FName> TextureParameterNames;

			UMaterial::GetMobileScalarParameterNamesForGroup(InGroupName, ScalarParameterNames);
			UMaterial::GetMobileVectorParameterNamesForGroup(InGroupName, VectorParameterNames);
			UMaterial::GetMobileTextureParameterNamesForGroup(InGroupName, TextureParameterNames);

			for (INT ScalarIdx = 0; ScalarIdx < ScalarParameterNames.Num(); ScalarIdx++)
			{
				FName ScalarName = ScalarParameterNames(ScalarIdx);
				UDEditorScalarParameterValue* ScalarParam = MICEd_GetScalarParameter(ScalarName);
				check(ScalarParam);
				FLOAT ScalarValue;
				if (SourceInstance->GetScalarParameterValue(ScalarName, ScalarValue) == TRUE)
				{
					ScalarParam->bOverride = TRUE;
					ScalarParam->ParameterValue = ScalarValue;
				}
				ParamGroup.Parameters.AddItem(ScalarParam);
			}

			for (INT VectorIdx = 0; VectorIdx < VectorParameterNames.Num(); VectorIdx++)
			{
				FName VectorName = VectorParameterNames(VectorIdx);
				UDEditorVectorParameterValue* VectorParam = MICEd_GetVectorParameter(VectorName);
				check(VectorParam);
				FLinearColor VectorValue;
				if (SourceInstance->GetVectorParameterValue(VectorName, VectorValue) == TRUE)
				{
					VectorParam->bOverride = TRUE;
					VectorParam->ParameterValue = VectorValue;
				}
				ParamGroup.Parameters.AddItem(VectorParam);
			}

			for (INT TextureIdx = 0; TextureIdx < TextureParameterNames.Num(); TextureIdx++)
			{
				FName TextureName = TextureParameterNames(TextureIdx);
				UDEditorTextureParameterValue* TextureParam = MICEd_GetTextureParameter(TextureName);
				check(TextureParam);
				UTexture* TextureValue;
				if (SourceInstance->GetTextureParameterValue(TextureName, TextureValue) == TRUE)
				{
					TextureParam->bOverride = TRUE;
					TextureParam->ParameterValue = TextureValue;
				}
				ParamGroup.Parameters.AddItem(TextureParam);
			}
		}

		return TRUE;
	}

	return FALSE;
}

/** Copies the mobile parameter array values back to the source instance. */
void UMaterialEditorInstanceConstant::CopyMobileParametersToSourceInstance()
{
	// Make this an ini setting?
	if (TRUE)
	{
		if(SourceInstance->IsTemplate(RF_ClassDefaultObject) == FALSE )
		{
			SourceInstance->MarkPackageDirty();

			// Copy all the mobile settings to the source
			// We simply place them in the appropriate type just like any other parameter...
			for (INT GroupIdx = 0; GroupIdx < MobileParameterGroups.Num(); GroupIdx++)
			{
				FEditorParameterGroup & Group = MobileParameterGroups(GroupIdx);
				for (INT ParameterIdx = 0; ParameterIdx < Group.Parameters.Num(); ParameterIdx++)
				{
					if (Group.Parameters(ParameterIdx) == NULL)
					{
						continue;
					}

					UDEditorScalarParameterValue * ScalarParameterValue = Cast<UDEditorScalarParameterValue>(Group.Parameters(ParameterIdx));
					if (ScalarParameterValue != NULL)
					{
						if (ScalarParameterValue->bOverride)
						{
							SourceInstance->SetScalarParameterValue(ScalarParameterValue->ParameterName, ScalarParameterValue->ParameterValue);
							continue;
						}
					}

					UDEditorTextureParameterValue * TextureParameterValue = Cast<UDEditorTextureParameterValue>(Group.Parameters(ParameterIdx));
					if (TextureParameterValue != NULL)
					{
						if (TextureParameterValue->bOverride)
						{
							SourceInstance->SetTextureParameterValue(TextureParameterValue->ParameterName, TextureParameterValue->ParameterValue);
							continue;
						}
					}

					UDEditorVectorParameterValue * VectorParameterValue = Cast<UDEditorVectorParameterValue>(Group.Parameters(ParameterIdx));
					if (VectorParameterValue != NULL)
					{
						if (VectorParameterValue->bOverride)
						{
							SourceInstance->SetVectorParameterValue(VectorParameterValue->ParameterName, VectorParameterValue->ParameterValue);
							continue;
						}
					}

				}
			}

			// force refresh of visibility of properties
			VisibleExpressions.AddItem(FGuid(0x8000, 0, 0, 0));

			// Copy all the other non-parameter settings as well so that at 
			// run-time we don't have to find the parent everytime we set parameters up.
			SourceInstance->SetupMobileProperties();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
//	WxMaterialInstanceConstantEditor
//
//////////////////////////////////////////////////////////////////////////

/**
 * wxWidgets Event Table
 */
BEGIN_EVENT_TABLE(WxMaterialInstanceConstantEditor, WxMaterialEditorBase)
	EVT_CLOSE( OnClose )
	EVT_MENU(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SYNCTOGB, OnMenuSyncToGB)
	EVT_MENU(ID_MATERIALINSTANCE_CONSTANT_EDITOR_OPENEDITOR, OnMenuOpenEditor)
	EVT_LIST_ITEM_ACTIVATED(ID_MATERIALINSTANCE_CONSTANT_EDITOR_LIST, OnInheritanceListDoubleClick)
	EVT_LIST_ITEM_RIGHT_CLICK(ID_MATERIALINSTANCE_CONSTANT_EDITOR_LIST, OnInheritanceListRightClick)
	EVT_TOOL(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS, OnShowAllMaterialParameters)
END_EVENT_TABLE()
IMPLEMENT_DYNAMIC_CLASS(WxMaterialInstanceConstantEditor, WxMaterialEditorBase);
WxMaterialInstanceConstantEditor::WxMaterialInstanceConstantEditor() :	WxMaterialEditorBase( NULL, -1, NULL ),   FDockingParent(this)
{}
WxMaterialInstanceConstantEditor::WxMaterialInstanceConstantEditor( wxWindow* Parent, wxWindowID id, UMaterialInterface* InMaterialInterface ) :	
        WxMaterialEditorBase( Parent, id, InMaterialInterface ),   
		FDockingParent(this)
{
	// Set the static mesh editor window title to include the static mesh being edited.
	SetTitle( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("MaterialInstanceEditorCaption_F"), *InMaterialInterface->GetPathName()) ) );

	// Construct a temp holder for our instance parameters.
	UMaterialInstanceConstant* InstanceConstant = Cast<UMaterialInstanceConstant>(InMaterialInterface);
	MaterialEditorInstance = ConstructObject<UMaterialEditorInstanceConstant>(UMaterialEditorInstanceConstant::StaticClass());
	
	UBOOL bTempUseOldStyleMICEditorGroups = TRUE;
	GConfig->GetBool(TEXT("UnrealEd.EditorEngine"), TEXT("UseOldStyleMICEditorGroups"), bTempUseOldStyleMICEditorGroups, GEngineIni);	
	MaterialEditorInstance->bUseOldStyleMICEditorGroups = bTempUseOldStyleMICEditorGroups;
	MaterialEditorInstance->SetSourceInstance(InstanceConstant);
	
	// Create toolbar
	ToolBar = new WxMaterialInstanceConstantEditorToolBar( this, -1 );
	SetToolBar( ToolBar );

	// Create property window
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, this );
	PropertyWindow->SetFlags(EPropertyWindowFlags::SupportsCustomControls, TRUE);

	SetSize(1024,768);

	// Add inheritance list.
	InheritanceList = new WxListView(this, ID_MATERIALINSTANCE_CONSTANT_EDITOR_LIST, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
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

WxMaterialInstanceConstantEditor::~WxMaterialInstanceConstantEditor()
{
	SaveSettings();

	PropertyWindow->SetObject( NULL, EPropertyWindowFlags::NoFlags );
	delete PropertyWindow;
}

void WxMaterialInstanceConstantEditor::OnClose(wxCloseEvent& In)
{
	// flatten the MIC if it was marked dirty while editing the MIC
	GCallbackEvent->Send(CALLBACK_MobileFlattenedTextureUpdate, MaterialEditorInstance->SourceInstance);

	// close the window
	Destroy();
}

void WxMaterialInstanceConstantEditor::Serialize(FArchive& Ar)
{
	WxMaterialEditorBase::Serialize(Ar);

	// Serialize our custom object instance
	Ar<<MaterialEditorInstance;

	// Serialize all parent material instances that are stored in the list.
	Ar<<ParentList;
}

/** Pre edit change notify for properties. */
void WxMaterialInstanceConstantEditor::NotifyPreChange(void* Src, UProperty* PropertyThatChanged)
{
	// If they changed the parent, kill the current object in the property window since we will need to remake it.
	if(PropertyThatChanged->GetName()==TEXT("Parent"))
	{
		// Collapse all of the property arrays, since they will be changing with the new parent.
		PropertyWindow->CollapseItem(TEXT("ParameterGroups"));
		PropertyWindow->CollapseItem(TEXT("MobileParameterGroups"));
	}
}

/** Post edit change notify for properties. */
void WxMaterialInstanceConstantEditor::NotifyPostChange(void* Src, UProperty* PropertyThatChanged)
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

			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface);
			if(MIC)
			{
				MIC->SetParent(NULL);
			}
		}

		RebuildInheritanceList();

		//We've invalidated the tree structure, rebuild it
		PropertyWindow->RequestReconnectToData();
	}

	//rebuild the property window to account for the possibility that the item changed was
	//a static switch

	UObject * PropertyClass = PropertyThatChanged->GetOuter();
	if(PropertyClass && PropertyClass->GetName() == TEXT("DEditorStaticSwitchParameterValue")  && MaterialEditorInstance->Parent )
	{
		TArray<FGuid> PreviousExpressions(MaterialEditorInstance->VisibleExpressions);
		MaterialEditorInstance->VisibleExpressions.Empty();
		WxMaterialEditor::GetVisibleMaterialParameters(MaterialEditorInstance->Parent->GetMaterial(), MaterialEditorInstance->SourceInstance, MaterialEditorInstance->VisibleExpressions);
		MaterialEditorInstance->VisibleExpressions.AddItem(FGuid(0x8000, 0, 0, 0));

		// switch param value can change param visibility - must regen arrays and update property window to force these to display.
		MaterialEditorInstance->RegenerateArrays();
		PropertyWindow->RequestReconnectToData();

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

		// Always force to refresh if static parameter has changed
		PropertyWindow->PostRebuild();		
	}
}

/** Rebuilds the inheritance list for this material instance. */
void WxMaterialInstanceConstantEditor::RebuildInheritanceList()
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
void WxMaterialInstanceConstantEditor::RebuildParentList()
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
void WxMaterialInstanceConstantEditor::RebuildChildList()
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

/**
 * Draws messages on the canvas.
 */
void WxMaterialInstanceConstantEditor::DrawMessages(FViewport* Viewport,FCanvas* Canvas)
{
	Canvas->PushAbsoluteTransform(FMatrix::Identity);
	if (MaterialEditorInstance->Parent)
	{
		const FMaterialResource* MaterialResource = MaterialEditorInstance->SourceInstance->GetMaterialResource();
		const UMaterial* BaseMaterial = MaterialEditorInstance->SourceInstance->GetMaterial();
		INT DrawPositionY = 5;
		if (BaseMaterial && MaterialResource)
		{
			DrawMaterialInfoStrings(Canvas, BaseMaterial, MaterialResource, MaterialResource->GetCompileErrors(), DrawPositionY, TRUE);
		}
	}
	Canvas->PopTransform();
}

/** Saves editor settings. */
void WxMaterialInstanceConstantEditor::SaveSettings()
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
void WxMaterialInstanceConstantEditor::LoadSettings()
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

		// Load the preview scene
		if(PreviewVC->PreviewScene)
		{
			PreviewVC->PreviewScene->LoadSettings(TEXT("MaterialInstanceEditor"));
		}
	}
}

/** Syncs the GB to the selected parent in the inheritance list. */
void WxMaterialInstanceConstantEditor::SyncSelectedParentToGB()
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
void WxMaterialInstanceConstantEditor::OpenSelectedParentEditor()
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
			else if(SelectedMaterialInstance->IsA(UMaterialInstanceConstant::StaticClass()))
			{
				// Show material instance editor
				UMaterialInstanceConstant* MaterialInstanceConstant = Cast<UMaterialInstanceConstant>(SelectedMaterialInstance);
				wxFrame* MaterialInstanceEditor = new WxMaterialInstanceConstantEditor( (wxWindow*)GApp->EditorFrame,-1, MaterialInstanceConstant );
				MaterialInstanceEditor->SetSize(1024,768);
				MaterialInstanceEditor->Show();
			}
		}
	}
}

/** Event handler for when the user wants to sync the GB to the currently selected parent. */
void WxMaterialInstanceConstantEditor::OnMenuSyncToGB(wxCommandEvent &Event)
{
	SyncSelectedParentToGB();
}

/** Event handler for when the user wants to open the editor for the selected parent material. */
void WxMaterialInstanceConstantEditor::OnMenuOpenEditor(wxCommandEvent &Event)
{
	OpenSelectedParentEditor();
}

/** Double click handler for the inheritance list. */
void WxMaterialInstanceConstantEditor::OnInheritanceListDoubleClick(wxListEvent &ListEvent)
{
	OpenSelectedParentEditor();
}

/** Event handler for when the user right clicks on the inheritance list. */
void WxMaterialInstanceConstantEditor::OnInheritanceListRightClick(wxListEvent &ListEvent)
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

			ContextMenu->Append(ID_MATERIALINSTANCE_CONSTANT_EDITOR_OPENEDITOR, *Label);
		}

		ContextMenu->Append(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SYNCTOGB, *LocalizeUnrealEd("SyncContentBrowser"));

		PopupMenu(ContextMenu);
	}
}

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxMaterialInstanceConstantEditor::GetDockingParentName() const
{
	return TEXT("MaterialInstanceEditor");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxMaterialInstanceConstantEditor::GetDockingParentVersion() const
{
	return 1;
}

/** Event handler for when the user wants to toggle showing all material parameters. */
void WxMaterialInstanceConstantEditor::OnShowAllMaterialParameters(wxCommandEvent &Event)
{
	PropertyWindow->RequestMainWindowTakeFocus();
	PropertyWindow->PostRebuild();
}

UObject* WxMaterialInstanceConstantEditor::GetSyncObject()
{
	if (MaterialEditorInstance)
	{
		return MaterialEditorInstance->SourceInstance;
	}
	return NULL;
}

FObjectPropertyNode_ParameterGroup::FObjectPropertyNode_ParameterGroup(void)
: FObjectPropertyNode()
{
}
FObjectPropertyNode_ParameterGroup::~FObjectPropertyNode_ParameterGroup(void)
{
}

void FObjectPropertyNode_ParameterGroup::InitChildNodes(void)
{
	
	check(TopPropertyWindow);
	//if the categories draw the borders OR the object is in the middle of the hierarchy (no need for a border at all)
	if( HasNodeFlags(EPropertyNodeFlags::ShowCategories) || (ParentNode != NULL))
	{
		ChildHeight = ChildSpacer = 0;
	}
	else
	{
		ChildHeight = ChildSpacer = PROP_CategorySpacer;
	}


	UDEditorParameterValue * DEditorParameterValue= NULL;
	for( INT i = 0; i < GetNumObjects(); ++i )
	{
		UObject* TempObject = GetObject( i );
		if( TempObject )
		{
			DEditorParameterValue= Cast<UDEditorParameterValue>( TempObject);
			if (DEditorParameterValue)
			{
				break;
			}
		}
	}
	if (DEditorParameterValue)
	{
		const UBOOL bShouldShowNonEditable = TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable);
		// Iterate over all fields, creating items.
		for( TFieldIterator<UProperty> It(BaseClass); It; ++It )
		{
			//if( bShouldShowNonEditable || (It->PropertyFlags&CPF_Edit )
			{
				UProperty* CurProp = *It;
				FItemPropertyNode* NewItemNode = new FItemPropertyNode;
				WxItemPropertyControl* pwi = CreatePropertyItem(CurProp);

				// skip other properties then ParameterValue 
				if(DEditorParameterValue)
				{
					FName ParameterValueName(TEXT("ParameterValue"));
					if (ParameterValueName != CurProp->GetFName())
					{
						continue;
					}
				}

				WxCustomPropertyItem_MIC_Parameter* PropertyWindowItem =NULL;
				if (DEditorParameterValue)
				{
					PropertyWindowItem = wxDynamicCast(pwi, WxCustomPropertyItem_MIC_Parameter);
					if (PropertyWindowItem)
					{
						PropertyWindowItem->PropertyStructName=DEditorParameterValue->ParameterName;
						PropertyWindowItem->SetDisplayName(DEditorParameterValue->ParameterName.ToString());
						PropertyWindowItem->ExpressionId = DEditorParameterValue->ExpressionId;

					}
				}
				NewItemNode->InitNode(pwi, this, TopPropertyWindow, CurProp, CurProp->Offset, INDEX_NONE);

				ChildNodes.AddItem(NewItemNode);
				// // disable not visible parameters instead of hiding it
				if (PropertyWindowItem )
				{
					UMaterialEditorInstanceConstant* MaterialInterface = PropertyWindowItem->GetInstanceObject();
					if(MaterialInterface && MaterialInterface->VisibleExpressions.ContainsItem(DEditorParameterValue->ExpressionId))
					{
						PropertyWindowItem->bDisabled = FALSE;
						NewItemNode->SetNodeFlags(EPropertyNodeFlags::ForceEditConstDisabledStyle, FALSE);
						//NewItemNode->SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, FALSE);
					}
					else
					{
						PropertyWindowItem->bDisabled = TRUE;
						NewItemNode->SetNodeFlags(EPropertyNodeFlags::ForceEditConstDisabledStyle, TRUE);
						//NewItemNode->SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, TRUE);
					}
				}
			}
		}
	}
}

IMPLEMENT_DYNAMIC_CLASS(WxCustomPropertyItem_MIC_Parameter, WxCustomPropertyItem_ConditionalItem);

BEGIN_EVENT_TABLE(WxCustomPropertyItem_MIC_Parameter, WxCustomPropertyItem_ConditionalItem)
EVT_BUTTON(ID_MATERIALINSTANCE_CONSTANT_EDITOR_RESETTODEFAULT, OnResetToDefault)
EVT_MENU(ID_PROP_RESET_TO_DEFAULT, WxCustomPropertyItem_MIC_Parameter::OnResetToDefault)
END_EVENT_TABLE()

/** Reset to default button event. */
void WxCustomPropertyItem_MIC_Parameter::OnResetToDefault(wxCommandEvent &Event)
{
	UMaterialEditorInstanceConstant* Instance = GetInstanceObject();


	if(Instance && Instance->Parent)
	{
		FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();

		UDEditorParameterValue * TempObject =NULL;
		if(ItemParent)
		{
			for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
			{
				TempObject = Cast<UDEditorParameterValue>( *It);
				break;
			}
		}
		if (TempObject)
		{
			FName PropertyName = PropertyStructName;
			FName TempDisplayName(*DisplayName);

			UDEditorScalarParameterValue * ScalarParameterValue = Cast<UDEditorScalarParameterValue>(TempObject);
			if (ScalarParameterValue)
			{
				FLOAT OutValue;
				if(Instance->Parent->GetScalarParameterValue(PropertyName, OutValue))
				{
					if(ScalarParameterValue->ParameterName == TempDisplayName)
					{
						ScalarParameterValue->ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
					}
				}
			}
			UDEditorFontParameterValue * FontParameterValue = Cast<UDEditorFontParameterValue>(TempObject);
			if (FontParameterValue)
			{
				UFont* OutFontValue;
				INT OutFontPage;
				if(Instance->Parent->GetFontParameterValue(TempDisplayName, OutFontValue,OutFontPage))
				{
					if(FontParameterValue->ParameterName == TempDisplayName)
					{
						FontParameterValue->FontValue = OutFontValue;
						FontParameterValue->FontPage = OutFontPage;
						Instance->CopyToSourceInstance();
					}
				}
			}

			UDEditorTextureParameterValue * TextureParameterValue = Cast<UDEditorTextureParameterValue>(TempObject);
			if (TextureParameterValue)
			{
				UTexture* OutValue;
				if(Instance->Parent->GetTextureParameterValue(PropertyName, OutValue))
				{
					if(TextureParameterValue->ParameterName == TempDisplayName)
					{
						TextureParameterValue->ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
					}
				}
			}
			UDEditorVectorParameterValue * VectorParameterValue = Cast<UDEditorVectorParameterValue>(TempObject);
			if (VectorParameterValue)
			{
				FLinearColor OutValue;
				if(Instance->Parent->GetVectorParameterValue(PropertyName, OutValue))
				{
					if(VectorParameterValue->ParameterName == TempDisplayName)
					{
						VectorParameterValue->ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
					}
				}
			}
			UDEditorStaticSwitchParameterValue * StaticSwitchParameterValue = Cast<UDEditorStaticSwitchParameterValue>(TempObject);
			if (StaticSwitchParameterValue)
			{
				UBOOL OutValue;
				FGuid TempGuid(0,0,0,0);
				if(Instance->Parent->GetStaticSwitchParameterValue(TempDisplayName, OutValue, TempGuid))
				{
					if(StaticSwitchParameterValue->ParameterName == TempDisplayName)
					{
						StaticSwitchParameterValue->ParameterValue = OutValue;
						Instance->CopyToSourceInstance();
					}
				}
			}
			UDEditorStaticComponentMaskParameterValue * StaticComponentMaskParameterValue = Cast<UDEditorStaticComponentMaskParameterValue>(TempObject);
			if (StaticSwitchParameterValue)
			{
				UBOOL OutValue[4];
				FGuid TempGuid(0,0,0,0);

				if(Instance->Parent->GetStaticComponentMaskParameterValue(TempDisplayName, OutValue[0], OutValue[1], OutValue[2], OutValue[3], TempGuid))
				{
					if(StaticSwitchParameterValue->ParameterName == TempDisplayName)
					{
						StaticComponentMaskParameterValue->ParameterValue.R = OutValue[0];
						StaticComponentMaskParameterValue->ParameterValue.G = OutValue[1];
						StaticComponentMaskParameterValue->ParameterValue.B = OutValue[2];
						StaticComponentMaskParameterValue->ParameterValue.A = OutValue[3];
						Instance->CopyToSourceInstance();
					}
				}
			}
		}

		// Rebuild property window to update the values.
		GetPropertyWindow()->Rebuild();
		GetPropertyWindow()->RequestMainWindowTakeFocus();
		check(PropertyNode);
		PropertyNode->InvalidateChildControls();
		FPropertyChangedEvent ChangeEvent(PropertyNode->GetProperty());
		NotifyPostChange(ChangeEvent);
	}
}
UBOOL WxCustomPropertyItem_MIC_Parameter::ToggleConditionValue()
{	
	UMaterialEditorInstanceConstant* Instance = GetInstanceObject();
	if(Instance)
	{
		FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
		UMaterialEditorInstanceConstant* MaterialInterface = NULL;
		UDEditorParameterValue * TempObject =NULL;
		if(ItemParent)
		{
			for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
			{
				TempObject = Cast<UDEditorParameterValue>( *It);
				break;
			}
		}
		if (TempObject)
		{
			TempObject->bOverride=!TempObject->bOverride;
			// Notify the instance that we modified an override so it needs to update itself.
			FPropertyNode* PropertyNode = GetPropertyNode();
			FPropertyChangedEvent PropertyEvent(PropertyNode->GetProperty());
			Instance->PostEditChangeProperty(PropertyEvent);
		}
	}
	// Always allow editing even if we aren't overriding values.
	return TRUE;
}
/**
* Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
* associated property should be allowed.
*/
UBOOL WxCustomPropertyItem_MIC_Parameter::IsConditionMet()
{
	return TRUE;
}
/**
* Overriden function to allow hiding when not referenced.
*/
UBOOL WxCustomPropertyItem_MIC_Parameter::IsDerivedForcedHide (void) const
{
	const WxPropertyWindowHost* PropertyWindowHost = GetPropertyWindow()->GetParentHostWindow();
	check(PropertyWindowHost);
	const wxWindow *HostParent   = PropertyWindowHost->GetParent();
	const WxMaterialInstanceConstantEditor *Win = wxDynamicCast( HostParent, WxMaterialInstanceConstantEditor );
	UBOOL ForceHide = TRUE;

	// When property window is floating (not docked) the parent is an wxAuiFloatingFrame.
	// We must get at the Editor via the docking system:
	//                      wxAuiFloatingFrame -> wxAuiManager -> ManagedWindow
	if (Win == NULL)
	{
		wxAuiFloatingFrame *OwnerFloatingFrame = wxDynamicCast( HostParent, wxAuiFloatingFrame );
		check(OwnerFloatingFrame);
		wxAuiManager *AuiManager = OwnerFloatingFrame->GetOwnerManager();
		Win = wxDynamicCast(AuiManager->GetManagedWindow(), WxMaterialInstanceConstantEditor);
	}
	check(Win);

	UMaterialEditorInstanceConstant* MaterialInterface = Win->MaterialEditorInstance;
	check(MaterialInterface);

	if(Win->ToolBar->GetToolState(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS) || MaterialInterface->VisibleExpressions.ContainsItem(ExpressionId))
	{
		ForceHide = FALSE;
	}

	return ForceHide;
}
UBOOL WxCustomPropertyItem_MIC_Parameter::IsControlsSuppressed (void)
{
	const WxPropertyWindowHost* PropertyWindowHost = GetPropertyWindow()->GetParentHostWindow();
	check(PropertyWindowHost);
	const wxWindow *HostParent   = PropertyWindowHost->GetParent();
	const WxMaterialInstanceConstantEditor *Win = wxDynamicCast( HostParent, WxMaterialInstanceConstantEditor );
	UBOOL bDisableCtrl = FALSE;

	// When property window is floating (not docked) the parent is an wxAuiFloatingFrame.
	// We must get at the Editor via the docking system:
	//                      wxAuiFloatingFrame -> wxAuiManager -> ManagedWindow
	if (Win == NULL)
	{
		wxAuiFloatingFrame *OwnerFloatingFrame = wxDynamicCast( HostParent, wxAuiFloatingFrame );
		check(OwnerFloatingFrame);
		wxAuiManager *AuiManager = OwnerFloatingFrame->GetOwnerManager();
		Win = wxDynamicCast(AuiManager->GetManagedWindow(), WxMaterialInstanceConstantEditor);
	}
	check(Win);

	UMaterialEditorInstanceConstant* MaterialInterface = Win->MaterialEditorInstance;
	check(MaterialInterface);

	if(Win->ToolBar->GetToolState(ID_MATERIALINSTANCE_CONSTANT_EDITOR_SHOWALLPARAMETERS) && MaterialInterface->VisibleExpressions.ContainsItem(ExpressionId) == FALSE)
	{
		bDisableCtrl = TRUE;
	}
	FPropertyNode* PropertyNode = GetPropertyNode();
	bDisabled=FALSE;

	PropertyNode->SetNodeFlags(EPropertyNodeFlags::ForceEditConstDisabledStyle, FALSE);
	//PropertyNode->SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, FALSE);
	if (bDisableCtrl)
	{
		PropertyNode->SetNodeFlags(EPropertyNodeFlags::ForceEditConstDisabledStyle, TRUE);
		//PropertyNode->SetNodeFlags(EPropertyNodeFlags::IsPropertyEditConst, TRUE);
		bDisabled=TRUE;
	}

	return FALSE;
}

UBOOL WxCustomPropertyItem_MIC_Parameter::ClickedPropertyItem( wxMouseEvent& Event )
{
	FPropertyNode* PropertyNode = GetPropertyNode();
	UProperty* NodeProperty = GetProperty();
	check(PropertyNode);

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
		bShouldGainFocus = !HasNodeFlags(EPropertyNodeFlags::CanBeExpanded);
		if ( ToggleConditionValue() == FALSE )
		{
			bShouldGainFocus = FALSE;

			// if we just disabled the condition which allows us to edit this control
			// collapse the item if this is an expandable item
			const UBOOL bExpand   = FALSE;
			const UBOOL bRecurse  = FALSE;
			PropertyNode->SetExpanded(bExpand, bRecurse);
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
	bShouldGainFocus = TRUE;
	return bShouldGainFocus;
}
/**Returns the rect for checkbox used in conditional item property controls.*/
wxRect WxCustomPropertyItem_MIC_Parameter::GetConditionalRect (void) const
{
	wxRect ConditionalRect = WxCustomPropertyItem_ConditionalItem::GetConditionalRect();
	//Make room for reset to defaults
	ConditionalRect.x += PROP_Indent;
	return ConditionalRect;
}
UBOOL WxCustomPropertyItem_MIC_Parameter::ClickedCheckbox( INT MouseX, INT MouseY ) const
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
void WxCustomPropertyItem_MIC_Parameter::Create(wxWindow* InParent)
{
	WxCustomPropertyItem_ConditionalItem::Create(InParent);

	// Set the tooltip for this custom property if we do not already have one
	if(GetToolTip() == NULL)
	{
		UMaterialEditorInstanceConstant* Instance = GetInstanceObject();
		if(Instance && Instance->Parent)
		{
			FName TempDisplayName(*DisplayName);
			FString ToolTipText;
			// Use the Description set in the Material editor
			if(Instance->Parent->GetParameterDesc(TempDisplayName, ToolTipText))
			{
				SetToolTip(*ToolTipText);
			}
			else if (Instance->Parent->IsA(UMaterialInstanceConstant::StaticClass()))
			{
				// Support for material instances that inherit from other material instances
				UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Instance->Parent);
				while (MIC && MIC->Parent)
				{
					if(MIC->Parent->GetParameterDesc(TempDisplayName, ToolTipText))
					{
						SetToolTip(*ToolTipText);
						break;
					}
					else if (MIC->Parent->IsA(UMaterialInstanceConstant::StaticClass()))
					{
						MIC = Cast<UMaterialInstanceConstant>(MIC->Parent);
					}
					else
					{
						break;
					}
				}
			}
		}
	}
	// Create a new button and add it to the button array.
	if(ResetToDefault==NULL)
	{
		ResetToDefault = new WxPropertyItemButton( this, ID_MATERIALINSTANCE_CONSTANT_EDITOR_RESETTODEFAULT, GPropertyWindowManager->Prop_ResetToDefaultB );
		INT OldIndentX = PropertyNode->GetIndentX();
		PropertyNode->SetIndentX(OldIndentX + 15 + PROP_ARROW_Width);

		// Generate tooltip text for this button.
		UMaterialEditorInstanceConstant* Instance = GetInstanceObject();

		if(Instance && Instance->Parent)
		{
			FString ToolTipText = *LocalizeUnrealEd("PropertyWindow_ResetToDefault");
			FName PropertyName = PropertyStructName;

			FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
			UDEditorParameterValue * TempObject =NULL;

			if(ItemParent)
			{
				for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
				{
					TempObject = Cast<UDEditorParameterValue>( *It);
					break;
				}
			}
			if (TempObject)
			{
				FName PropertyName = PropertyStructName;
				FName TempDisplayName(*DisplayName);

				UDEditorScalarParameterValue * ScalarParameterValue = Cast<UDEditorScalarParameterValue>(TempObject);
				if (ScalarParameterValue)
				{
					FLOAT OutValue;
					if(Instance->Parent->GetScalarParameterValue(TempDisplayName, OutValue))
					{
						ToolTipText += TEXT(" ");
						ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceFloatValue_F"), OutValue));
					}
				}
				UDEditorFontParameterValue * FontParameterValue = Cast<UDEditorFontParameterValue>(TempObject);
				if (FontParameterValue)
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

				UDEditorTextureParameterValue * TextureParameterValue = Cast<UDEditorTextureParameterValue>(TempObject);
				if (TextureParameterValue)
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
				UDEditorVectorParameterValue * VectorParameterValue = Cast<UDEditorVectorParameterValue>(TempObject);
				if (VectorParameterValue)
				{
					FLinearColor OutValue;
					if(Instance->Parent->GetVectorParameterValue(TempDisplayName, OutValue))
					{
						ToolTipText += TEXT(" ");
						ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceVectorValue_F"), OutValue.R, OutValue.G, OutValue.B, OutValue.A));
					}
				}
				UDEditorStaticSwitchParameterValue * StaticSwitchParameterValue = Cast<UDEditorStaticSwitchParameterValue>(TempObject);
				if (StaticSwitchParameterValue)
				{
					UBOOL OutValue;
					FGuid TempGuid(0,0,0,0);
					if(Instance->Parent->GetStaticSwitchParameterValue(TempDisplayName, OutValue, TempGuid))
					{
						ToolTipText += TEXT(" ");
						ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceStaticSwitchValue_F"), (INT)OutValue));
					}
				}
				UDEditorStaticComponentMaskParameterValue * StaticComponentMaskParameterValue = Cast<UDEditorStaticComponentMaskParameterValue>(TempObject);
				if (StaticSwitchParameterValue)
				{
					UBOOL OutValue[4];
					FGuid TempGuid(0,0,0,0);

					if(Instance->Parent->GetStaticComponentMaskParameterValue(TempDisplayName, OutValue[0], OutValue[1], OutValue[2], OutValue[3], TempGuid))
					{
						ToolTipText += TEXT(" ");
						ToolTipText += FString::Printf(LocalizeSecure(LocalizeUnrealEd("MaterialInstanceStaticComponentMaskValue_F"), (INT)OutValue[0], (INT)OutValue[1], (INT)OutValue[2], (INT)OutValue[3]));
					}
				}
			}
			ResetToDefault->SetToolTip(*ToolTipText);
		}
	}
}
FString WxCustomPropertyItem_MIC_Parameter::GetDisplayName () const {
	return PropertyStructName.ToString();
}
WxCustomPropertyItem_MIC_Parameter::WxCustomPropertyItem_MIC_Parameter() : 
WxCustomPropertyItem_ConditionalItem()
{
	ResetToDefault = NULL;
	bAllowEditing = FALSE;
	bDisabled =FALSE;
}
/** @return Returns the instance object this property is associated with. */
UMaterialEditorInstanceConstant* WxCustomPropertyItem_MIC_Parameter::GetInstanceObject()
{
	FPropertyNode* PropertyNodeA = PropertyNode->GetParentNode()->GetParentNode();
	FObjectPropertyNode* ItemParent = PropertyNodeA->FindObjectItemParent();
	//FObjectPropertyNode* ItemParent = ItemParentA->FindObjectItemParent();
	UMaterialEditorInstanceConstant* MaterialInterface = NULL;

	if(ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			MaterialInterface = Cast<UMaterialEditorInstanceConstant>(*It);
			break;
		}
	}

	return MaterialInterface;
}
UBOOL WxCustomPropertyItem_MIC_Parameter::IsOverridden()
{

	UMaterialEditorInstanceConstant* Instance = GetInstanceObject();

	FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
	UMaterialEditorInstanceConstant* MaterialInterface = NULL;
	UDEditorParameterValue * TempObject =NULL;
	if(ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			TempObject = Cast<UDEditorParameterValue>( *It);
			break;
		}
	}
	if (TempObject)
	{
		bAllowEditing = TempObject->bOverride;

	}

	return bAllowEditing;
}
void WxCustomPropertyItem_MIC_Parameter::RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect )
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


IMPLEMENT_CLASS(UPropertyDrawProxy_ParameterGroup);
void UPropertyDrawProxy_ParameterGroup::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy )
{
	FString Str ;
	UStructProperty * StructProp = Cast<UStructProperty>(InProperty);
	if ( StructProp != NULL  )
	{
		FEditorParameterGroup* Param = (FEditorParameterGroup*)((BYTE*)InReadAddress);
		if (Param)
		{

			TArrayNoInit<class UDEditorParameterValue*> & ParametersGR = Param->Parameters;
			INT UsedParams=0;
			for (INT k=0; k < ParametersGR.Num(); k++)
			{
				UDEditorParameterValue * Ad  = ParametersGR(k);
				if (Ad && Ad->bOverride)
				{
					UsedParams++;
				}

			}
			Str = FString::Printf(TEXT("%s (%s %d/%d)"), *LocalizeUnrealEd( TEXT("ArrayPropertyHeader") ), *LocalizeUnrealEd( TEXT("Overriding") ), UsedParams,  ParametersGR.Num());
		}


	}
	else if ( Cast<UArrayProperty>(InProperty) != NULL )
	{
		Str = FString::Printf(TEXT("%s (%d)"), *LocalizeUnrealEd( TEXT("ArrayPropertyHeader") ), ((FScriptArray*)InReadAddress)->Num());
	}
	else
	{
		Str = *LocalizeUnrealEd( TEXT("ArrayPropertyHeader") );
	}

	// . . . and draw it.
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}
UBOOL UPropertyDrawProxy_ParameterGroup::Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
{

	return TRUE;

}
//////////////////////////////////////////////////////////////////////////
// WxCustomPropertyItem_ParameterGroup
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC_CLASS(WxCustomPropertyItem_ParameterGroup, WxItemPropertyControl);

WxCustomPropertyItem_ParameterGroup::WxCustomPropertyItem_ParameterGroup() : 
WxItemPropertyControl()
{

}

void WxCustomPropertyItem_ParameterGroup::Create(wxWindow* InParent)
{
	//DisplayName =TEXT("DUPA");
	//WxPropertyControl::Create( InParent);
	WxItemPropertyControl::Create(InParent);

	//// create custom draw proxy to display statistic for used parameters
	UClass* DrawProxyClass = UObject::StaticLoadClass( UPropertyDrawProxy_ParameterGroup::StaticClass(), NULL, TEXT("UnrealEd.PropertyDrawProxy_ParameterGroup"), NULL, LOAD_None, NULL );
	DrawProxy	= ConstructObject<UPropertyDrawProxy>( DrawProxyClass );


}
FString WxCustomPropertyItem_ParameterGroup::GetItemName () const {
	check(PropertyNode);
	if( PropertyNode->GetArrayIndex()==-1 )
	{
		return GetDisplayName();
	}
	else
	{
		// when we have array index we need to diplay name of group instead
		return PropertyStructName.ToString();
	}	
}
FString WxCustomPropertyItem_ParameterGroup::GetDisplayName () const {
	return DisplayName;
}
void WxCustomPropertyItem_ParameterGroup::OnPaint(wxPaintEvent& In)
{
	WxItemPropertyControl::OnPaint( In );
}
void WxCustomPropertyItem_ParameterGroup::InitWindowDefinedChildNodes(void)
{

	check(PropertyNode);
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	UProperty* Property = GetProperty();
	check(Property);

	FPropertyNode*		aa =	GetPropertyNode();

	FName PropertyName = Property->GetFName();
	UStructProperty* StructProperty = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty);
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
	UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property,CLASS_IsAUObjectProperty);


	// Expand array.
	PropertyNode->SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);
	//PropertyNode->SetNodeFlags(EPropertyNodeFlags::ShowCategories, TRUE);
	if( Property->ArrayDim > 1 && PropertyNode->GetArrayIndex() == -1 )
	{
		for( INT i = 0 ; i < Property->ArrayDim ; i++ )
		{
			FItemPropertyNode* NewItemNode = new FItemPropertyNode;//;//CreatePropertyItem(ArrayProperty,i,this);
			WxItemPropertyControl* pwi = PropertyNode->CreatePropertyItem(Property,i);
			NewItemNode->InitNode(pwi, PropertyNode, MainWindow, Property, i*Property->ElementSize, i);
			PropertyNode->AddChildNode(NewItemNode);
		}
	}
	else if( StructProperty )
	{

		TArray<BYTE*> Addresses;

		if ( PropertyNode->GetReadAddress( PropertyNode, PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
		{

			// add  display name instead of array index from 
			FEditorParameterGroup* Param = (FEditorParameterGroup*)((BYTE*)Addresses(0));
			PropertyStructName = (Param->GroupName);


			FName ParameterValueName(TEXT("ParameterValue"));
			TArrayNoInit<class UDEditorParameterValue*> & ParametersGR = Param->Parameters;

			for (INT k=0; k < ParametersGR.Num(); k++)
			{
				UDEditorParameterValue * DEditorParameterValue =  ParametersGR(k);
				if (DEditorParameterValue == NULL )
				{
					continue;
				}


				// we use custom display of objects instead of nested arrays of   

				WxObjectsPropertyControl* pwo = new WxObjectsPropertyControl;
				FObjectPropertyNode_ParameterGroup* NewObjectNode = new FObjectPropertyNode_ParameterGroup;

				NewObjectNode->AddObject( DEditorParameterValue);
				NewObjectNode->InitNode(pwo, PropertyNode, MainWindow, Property, PROP_OFFSET_None, INDEX_NONE, TRUE);
				
				PropertyNode->AddChildNode(NewObjectNode);

			}


		}


	}
}
