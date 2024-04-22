/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnrealEdPrivateClasses.h"

#include "PropertyNode.h"
#include "ObjectsPropertyNode.h"
#include "ItemPropertyNode.h"
#include "LightmassPropertyNode.h"
#include "PropertyWindow.h"
#include "PropertyUtils.h"
#include "PropertyWindowManager.h"
#include "ScopedTransaction.h"
#include "ScopedPropertyChange.h"	// required for access to FScopedPropertyChange

//////////////////////////////////////////////////////////////////////////
// WxCustomPropertyItem_LightmassMaterialParameter
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC_CLASS(WxCustomPropertyItem_LightmassMaterialParameter, WxCustomPropertyItem_ConditionalItem);

BEGIN_EVENT_TABLE(WxCustomPropertyItem_LightmassMaterialParameter, WxCustomPropertyItem_ConditionalItem)
	EVT_BUTTON(ID_MATERIALINSTANCE_CONSTANT_EDITOR_RESETTODEFAULT, OnResetToDefault)
	EVT_MENU(ID_PROP_RESET_TO_DEFAULT, WxCustomPropertyItem_LightmassMaterialParameter::OnResetToDefault)
END_EVENT_TABLE()

WxCustomPropertyItem_LightmassMaterialParameter::WxCustomPropertyItem_LightmassMaterialParameter() : 
	WxCustomPropertyItem_ConditionalItem()
{
	ResetToDefault = NULL;
	bAllowEditing = FALSE;
}

/**
 * Initialize this property window.  Must be the first function called after creating the window.
 */
void WxCustomPropertyItem_LightmassMaterialParameter::Create(wxWindow* InParent)
{
	WxCustomPropertyItem_ConditionalItem::Create(InParent);

	// Create a new button and add it to the button array.
	if (ResetToDefault == NULL)
	{
		ResetToDefault = new WxPropertyItemButton(this, ID_MATERIALINSTANCE_CONSTANT_EDITOR_RESETTODEFAULT, GPropertyWindowManager->Prop_ResetToDefaultB);
		check(PropertyNode);
		const INT OldIndentX = PropertyNode->GetIndentX();
		PropertyNode->SetIndentX(OldIndentX + 15 + PROP_ARROW_Width);

		// Generate tooltip text for this button.
		UMaterialInterface* Parent = GetInstanceObjectParent();
		FLightmassParameterizedMaterialSettings* LightmassSettings = GetInstanceLightmassSettings();

		if (Parent && LightmassSettings)
		{
			FString ToolTipText = *LocalizeUnrealEd("PropertyWindow_ResetToDefault");
			FName PropertyName = PropertyStructName;

			if (DisplayName == TEXT("EmissiveBoost"))
			{
				FLOAT OutValue = Parent->GetEmissiveBoost();
				ToolTipText += TEXT(" ");
				ToolTipText += FString::Printf(TEXT("%f"), OutValue);
			}
			else if (DisplayName == TEXT("DiffuseBoost"))
			{
				FLOAT OutValue = Parent->GetDiffuseBoost();
				ToolTipText += TEXT(" ");
				ToolTipText += FString::Printf(TEXT("%f"), OutValue);
			}
			else if (DisplayName == TEXT("SpecularBoost"))
			{
				FLOAT OutValue = Parent->GetSpecularBoost();
				ToolTipText += TEXT(" ");
				ToolTipText += FString::Printf(TEXT("%f"), OutValue);
			}
			else if (DisplayName == TEXT("ExportResolutionScale"))
			{
				FLOAT OutValue = Parent->GetExportResolutionScale();
				ToolTipText += TEXT(" ");
				ToolTipText += FString::Printf(TEXT("%f"), OutValue);
			}
			else if (DisplayName == TEXT("DistanceFieldPenumbraScale"))
			{
				FLOAT OutValue = Parent->GetDistanceFieldPenumbraScale();
				ToolTipText += TEXT(" ");
				ToolTipText += FString::Printf(TEXT("%f"), OutValue);
			}

			ResetToDefault->SetToolTip(*ToolTipText);
		}
		ResetToDefault->Hide();	//start out hidden
	}
}

/**
 * Toggles the value of the property being used as the condition for editing this property.
 *
 * @return	the new value of the condition (i.e. TRUE if the condition is now TRUE)
 */
UBOOL WxCustomPropertyItem_LightmassMaterialParameter::ToggleConditionValue()
{	
	FLightmassParameterizedMaterialSettings* LightmassSettings = GetInstanceLightmassSettings();

	if (LightmassSettings)
	{
		if (DisplayName == TEXT("EmissiveBoost"))
		{
			LightmassSettings->EmissiveBoost.bOverride = !LightmassSettings->EmissiveBoost.bOverride;
		}
		else if (DisplayName == TEXT("DiffuseBoost"))
		{
			LightmassSettings->DiffuseBoost.bOverride = !LightmassSettings->DiffuseBoost.bOverride;
		}
		else if (DisplayName == TEXT("SpecularBoost"))
		{
			LightmassSettings->SpecularBoost.bOverride = !LightmassSettings->SpecularBoost.bOverride;
		}
		else if (DisplayName == TEXT("ExportResolutionScale"))
		{
			LightmassSettings->ExportResolutionScale.bOverride = !LightmassSettings->ExportResolutionScale.bOverride;
		}
		else if (DisplayName == TEXT("DistanceFieldPenumbraScale"))
		{
			LightmassSettings->DistanceFieldPenumbraScale.bOverride = !LightmassSettings->DistanceFieldPenumbraScale.bOverride;
		}

		// Notify the instance that we modified an override so it needs to update itself.
		InstanceObjectPostEditChange();
	}

	// Always allow editing even if we aren't overriding values.
	return TRUE;
}

/**
 * Returns TRUE if MouseX and MouseY fall within the bounding region of the checkbox used for displaying the value of this property's edit condition.
 */	
UBOOL WxCustomPropertyItem_LightmassMaterialParameter::ClickedCheckbox( INT MouseX, INT MouseY ) const
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
 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
 * associated property should be allowed.
 */
UBOOL WxCustomPropertyItem_LightmassMaterialParameter::IsOverridden()
{
	FLightmassParameterizedMaterialSettings* LightmassSettings = GetInstanceLightmassSettings();

	if (LightmassSettings)
	{
		if (DisplayName == TEXT("EmissiveBoost"))
		{
			bAllowEditing = LightmassSettings->EmissiveBoost.bOverride;
		}
		else if (DisplayName == TEXT("DiffuseBoost"))
		{
			bAllowEditing = LightmassSettings->DiffuseBoost.bOverride;
		}
		else if (DisplayName == TEXT("SpecularBoost"))
		{
			bAllowEditing = LightmassSettings->SpecularBoost.bOverride;
		}
		else if (DisplayName == TEXT("ExportResolutionScale"))
		{
			bAllowEditing = LightmassSettings->ExportResolutionScale.bOverride;
		}
		else if (DisplayName == TEXT("DistanceFieldPenumbraScale"))
		{
			bAllowEditing = LightmassSettings->DistanceFieldPenumbraScale.bOverride;
		}
	}

	return bAllowEditing;
}

/**
 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
 * associated property should be allowed.
 */
UBOOL WxCustomPropertyItem_LightmassMaterialParameter::IsConditionMet()
{
	return TRUE;
}

/** @return	The parent of the instance object this property is associated with. */
UMaterialInterface* WxCustomPropertyItem_LightmassMaterialParameter::GetInstanceObjectParent()
{
	check(PropertyNode);
	FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
	UMaterialEditorInstanceConstant* MIConstant = NULL;
	UMaterialEditorInstanceTimeVarying* MITimeVarying = NULL;

	if (ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			MIConstant = Cast<UMaterialEditorInstanceConstant>(*It);
			MITimeVarying = Cast<UMaterialEditorInstanceTimeVarying>(*It);
			break;
		}
	}

	if (MIConstant)
	{
		return MIConstant->Parent;
	}
	if (MITimeVarying)
	{
		return MITimeVarying->Parent;
	}
	return NULL;
}

/** @return	The Lightmass settings for the instance object this property is associated with. */
FLightmassParameterizedMaterialSettings* WxCustomPropertyItem_LightmassMaterialParameter::GetInstanceLightmassSettings()
{
	check(PropertyNode);
	FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
	UMaterialEditorInstanceConstant* MIConstant = NULL;
	UMaterialEditorInstanceTimeVarying* MITimeVarying = NULL;

	if (ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			MIConstant = Cast<UMaterialEditorInstanceConstant>(*It);
			MITimeVarying = Cast<UMaterialEditorInstanceTimeVarying>(*It);
			break;
		}
	}

	if (MIConstant)
	{
		return &(MIConstant->LightmassSettings);
	}
	if (MITimeVarying)
	{
		return &(MITimeVarying->LightmassSettings);
	}
	return NULL;
}

/** Call PostEditChange on the instance object this property is associated with. */
void WxCustomPropertyItem_LightmassMaterialParameter::InstanceObjectPostEditChange()
{
	check(PropertyNode);
	FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
	UMaterialEditorInstanceConstant* MIConstant = NULL;
	UMaterialEditorInstanceTimeVarying* MITimeVarying = NULL;

	if (ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			MIConstant = Cast<UMaterialEditorInstanceConstant>(*It);
			MITimeVarying = Cast<UMaterialEditorInstanceTimeVarying>(*It);
			break;
		}
	}

	FPropertyChangedEvent PropertyEvent(PropertyNode->GetProperty());
	if (MIConstant)
	{
		MIConstant->PostEditChangeProperty(PropertyEvent);
	}
	if (MITimeVarying)
	{
		MITimeVarying->PostEditChangeProperty(PropertyEvent);
	}
}

/** Call CopyToSourceInstance on the instance object this property is associated with. */
void WxCustomPropertyItem_LightmassMaterialParameter::InstanceObjectCopyToSource()
{
	check(PropertyNode);
	FObjectPropertyNode* ItemParent = PropertyNode->FindObjectItemParent();
	UMaterialEditorInstanceConstant* MIConstant = NULL;
	UMaterialEditorInstanceTimeVarying* MITimeVarying = NULL;

	if (ItemParent)
	{
		for(FObjectPropertyNode::TObjectIterator It(ItemParent->ObjectIterator()); It; ++It)
		{
			MIConstant = Cast<UMaterialEditorInstanceConstant>(*It);
			MITimeVarying = Cast<UMaterialEditorInstanceTimeVarying>(*It);
			break;
		}
	}

	if (MIConstant)
	{
		MIConstant->CopyToSourceInstance();
	}
	if (MITimeVarying)
	{
		MITimeVarying->CopyToSourceInstance();
	}
}

/**
 * Renders the left side of the property window item.
 *
 * This version is responsible for rendering the checkbox used for toggling whether this property item window should be enabled.
 *
 * @param	RenderDeviceContext		the device context to use for rendering the item name
 * @param	ClientRect				the bounding region of the property window item
 */
void WxCustomPropertyItem_LightmassMaterialParameter::RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect )
{
	check(PropertyNode);
	const INT IndentX = PropertyNode->GetIndentX();
	const UBOOL bItemEnabled = IsOverridden();

	// determine which checkbox image to render
	const WxMaskedBitmap& bmp = bItemEnabled
		? GPropertyWindowManager->CheckBoxOnB
		: GPropertyWindowManager->CheckBoxOffB;

	// render the checkbox bitmap
	wxRect ConditionalRect = GetConditionalRect();

	RenderDeviceContext.DrawBitmap( bmp, ConditionalRect.GetLeft(), ConditionalRect.GetTop(), 1 );
	ResetToDefault->SetSize(ConditionalRect.GetLeft()-PROP_Indent, ConditionalRect.GetTop(), PROP_Indent, PROP_Indent);

	INT W, H;
	RenderDeviceContext.GetTextExtent( *DisplayName, &W, &H );
	wxRect TitleRect = GetItemNameRect(W, H);
	RenderDeviceContext.DrawText( *DisplayName, TitleRect.GetLeft(), TitleRect.GetTop() );

	RenderDeviceContext.DestroyClippingRegion();
}

/** Reset to default button event. */
void WxCustomPropertyItem_LightmassMaterialParameter::OnResetToDefault(wxCommandEvent &Event)
{
	UMaterialInterface* Parent = GetInstanceObjectParent();
	FLightmassParameterizedMaterialSettings* LightmassSettings = GetInstanceLightmassSettings();

	if (Parent && LightmassSettings)
	{
 		if (DisplayName == TEXT("EmissiveBoost"))
 		{
 			LightmassSettings->EmissiveBoost.ParameterValue = Parent->GetEmissiveBoost();
 			InstanceObjectCopyToSource();
 		}
 		else if (DisplayName == TEXT("DiffuseBoost"))
 		{
 			LightmassSettings->DiffuseBoost.ParameterValue = Parent->GetDiffuseBoost();
 			InstanceObjectCopyToSource();
 		}
 		else if (DisplayName == TEXT("SpecularBoost"))
 		{
 			LightmassSettings->SpecularBoost.ParameterValue = Parent->GetSpecularBoost();
 			InstanceObjectCopyToSource();
 		}
 		else if (DisplayName == TEXT("ExportResolutionScale"))
 		{
 			LightmassSettings->ExportResolutionScale.ParameterValue = Parent->GetExportResolutionScale();
 			InstanceObjectCopyToSource();
 		}
		else if (DisplayName == TEXT("DistanceFieldPenumbraScale"))
		{
			LightmassSettings->DistanceFieldPenumbraScale.ParameterValue = Parent->GetDistanceFieldPenumbraScale();
			InstanceObjectCopyToSource();
		}
 
		// Rebuild property window to update the values.
		GetPropertyWindow()->Rebuild();
		GetPropertyWindow()->RequestMainWindowTakeFocus();
		check(PropertyNode);
		PropertyNode->InvalidateChildControls();
	}
}

/**
 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
 * and (if the property window item is expandable) to toggle expansion state.
 *
 * @param	Event	the mouse click input that generated the event
 *
 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
 */
UBOOL WxCustomPropertyItem_LightmassMaterialParameter::ClickedPropertyItem( wxMouseEvent& Event )
{
	UBOOL bShouldGainFocus = TRUE;

	check(PropertyNode);
	UProperty* Property = PropertyNode->GetProperty();

	// if this property is edit-const, it can't be changed
	// or if we couldn't find a valid condition property, also use the base version
	if ( Property == NULL || PropertyNode->IsEditConst() )
	{
		bShouldGainFocus = WxItemPropertyControl::ClickedPropertyItem(Event);
	}

	// if they clicked on the checkbox, toggle the edit condition
	else if ( ClickedCheckbox(Event.GetX(), Event.GetY()) )
	{
		
		NotifyPreChange(Property);
		bShouldGainFocus = !HasNodeFlags(EPropertyNodeFlags::CanBeExpanded);
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

		WxPropertyControl* RefreshWindow = NULL;
		FPropertyNode* ParentNode = PropertyNode->GetParentNode();
		if ( !HasNodeFlags(EPropertyNodeFlags::CanBeExpanded) && ParentNode != NULL )
		{
			RefreshWindow = ParentNode->GetNodeWindow();
		}
		else
		{
			RefreshWindow = this;
		}
		check(RefreshWindow);
		RefreshWindow->Refresh();


		// Note the current property window so that CALLBACK_ObjectPropertyChanged
		// doesn't destroy the window out from under us.
		WxPropertyWindow* MainWindow = GetPropertyWindow();
		check(MainWindow);
		MainWindow->ChangeActiveCallbackCount(1);

		const UBOOL bTopologyChange = FALSE;
		FPropertyChangedEvent ChangeEvent(Property, bTopologyChange);
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

/**
 * Custom property window item class for displaying material instance parameter arrays unwrapped.
 */
IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow_LightmassMaterialParameters, WxItemPropertyControl);

/**
 * Called by Expand(), creates any child items for any properties within this item.
 * @param InFilterString - List of strings that must be in the property name in order to display
 *					Empty InFilterStrings means display as normal
 *					All strings must match in order to be accepted by the filter
 */
void WxPropertyWindow_LightmassMaterialParameters::InitWindowDefinedChildNodes()
{
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	check(PropertyNode);
	UProperty* Property = GetProperty();
	check(Property);

	FName PropertyName = Property->GetFName();
	UStructProperty* StructProperty = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty);

	PropertyNode->SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);
	if (StructProperty)
	{
		// Expand struct.
		for (TFieldIterator<UProperty> ParentIt(StructProperty->Struct); ParentIt; ++ParentIt)
		{
			UStructProperty* StructParent = Cast<UStructProperty>(*ParentIt);
			if (StructParent)
			{
				for( TFieldIterator<UProperty> It(StructParent->Struct); It; ++It )
				{
					UProperty* StructMember = *It;
					if( MainWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable) || (StructMember->PropertyFlags&CPF_Edit) )
					{

						WxItemPropertyControl* pwi = PropertyNode->CreatePropertyItem(StructMember,INDEX_NONE);
						WxCustomPropertyItem_LightmassMaterialParameter* PropertyWindowItem = wxDynamicCast(pwi, WxCustomPropertyItem_LightmassMaterialParameter);

						if (PropertyWindowItem == NULL)
						{
							continue;
						}

						// Find a name for the parameter property we are adding.
						const FName OverrideName = StructParent->GetFName();

						// Add the property.
						PropertyWindowItem->PropertyStructName = StructProperty->GetFName();
						PropertyWindowItem->SetDisplayName(OverrideName.ToString());


						FItemPropertyNode* NewTreeItemNode = new FItemPropertyNode;//;//CreatePropertyItem(ArrayProperty,i,this);
						NewTreeItemNode->InitNode(pwi, PropertyNode, MainWindow, StructMember, StructParent->Offset+StructMember->Offset, INDEX_NONE);
						PropertyNode->AddChildNode(NewTreeItemNode);
					}
				}
			}
		}
	}
	else
	{
		WxItemPropertyControl::InitWindowDefinedChildNodes();
	}
	PropertyNode->SetNodeFlags(EPropertyNodeFlags::SortChildren, TRUE);
}

/**
 * Custom property window item class for displaying Lightmass level settings in the WorldProperties.
 */
IMPLEMENT_DYNAMIC_CLASS(WxCustomPropertyItem_LightmassWorldInfoSettingsParameter, WxItemPropertyControl);

/**
 * Called by Expand(), creates any child items for any properties within this item.
 * @param InFilterString - List of strings that must be in the property name in order to display
 *					Empty InFilterStrings means display as normal
 *					All strings must match in order to be accepted by the filter
 */

void WxCustomPropertyItem_LightmassWorldInfoSettingsParameter::InitWindowDefinedChildNodes()
{
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	check(PropertyNode);
	UProperty* Property = GetProperty();
	check(Property);


	FName PropertyName = Property->GetFName();
	UStructProperty* StructProperty = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty);

	WxItemPropertyControl::InitWindowDefinedChildNodes();
}

