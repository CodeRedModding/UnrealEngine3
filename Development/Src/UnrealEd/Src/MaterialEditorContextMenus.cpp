/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineMaterialClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "MaterialEditorContextMenus.h"
#include "NewMaterialEditor.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Static helpers.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

/**
 * Appends entries to the specified menu for creating new material expressions.
 *
 * @param	Menu			The menu to append items to.
 * @param	MaterialEditor	The material editor the menu appears in.
 */
IMPLEMENT_COMPARE_CONSTREF( FString, MaterialEditorContextMenus, { return appStricmp(*A,*B); } );
static void AppendNewShaderExpressionsToMenu(wxMenu* Menu, WxMaterialEditor* MaterialEditor)
{
	check( MaterialEditor->bMaterialExpressionClassesInitialized );

	INT ID = IDM_NEW_SHADER_EXPRESSION_START;

	if (MaterialEditor->bUseUnsortedMenus == TRUE)
	{
		// Create a menu based on the sorted list.
		for( INT x = 0; x < MaterialEditor->MaterialExpressionClasses.Num(); ++x, ID++ )
		{
			UClass* MaterialExpressionClass = MaterialEditor->MaterialExpressionClasses(x);
			if (IsAllowedExpressionType(MaterialExpressionClass, MaterialEditor->EditMaterialFunction != NULL))
			{
				Menu->Append( ID,
					*FString::Printf(LocalizeSecure(LocalizeUnrealEd("NewF"),
					*FString(*MaterialExpressionClass->GetName()).Mid(appStrlen(TEXT("MaterialExpression"))))),
					TEXT("")
					);
			}
		}
	}
	else
	{
		// Add the Favorites sub-menu
		if (MaterialEditor->FavoriteExpressionClasses.Num() > 0)
		{
			// Search for all modules of this type
			TMap<INT, FString> ExpressionsMap;
			for (INT FavIndex = 0; FavIndex < MaterialEditor->FavoriteExpressionClasses.Num(); FavIndex++, ID++)
			{
				if (IsAllowedExpressionType(MaterialEditor->FavoriteExpressionClasses(FavIndex), MaterialEditor->EditMaterialFunction != NULL))
				{
					ExpressionsMap.Set( ID, FString(*MaterialEditor->FavoriteExpressionClasses(FavIndex)->GetName()).Mid(appStrlen(TEXT("MaterialExpression"))) );
				}
			}

			// Add the 'label'
			if( ExpressionsMap.Num() > 0 )
			{
				// Sort by name
				ExpressionsMap.ValueSort<COMPARE_CONSTREF_CLASS(FString,MaterialEditorContextMenus)>();

				wxMenu* pkNewMenu = new wxMenu();
				for( TMap<INT, FString>::TIterator It(ExpressionsMap); It; ++It )
				{
					pkNewMenu->Append(It.Key(),
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("NewF"),
						*It.Value())),
						TEXT("")
						);
				}
				Menu->Append(-1, TEXT("Favorites"), pkNewMenu);
				Menu->AppendSeparator();
			}
		}

		// Add each category
		if (MaterialEditor->CategorizedMaterialExpressionClasses.Num() > 0)
		{
			for (INT CategoryIndex = 0; CategoryIndex < MaterialEditor->CategorizedMaterialExpressionClasses.Num(); CategoryIndex++)
			{
				FCategorizedMaterialExpressionNode* CategoryNode = &(MaterialEditor->CategorizedMaterialExpressionClasses(CategoryIndex));
				
				// Search for all modules of this type
				TMap<INT, FString> ExpressionsMap;
				for (INT SubIndex = 0; SubIndex < CategoryNode->MaterialExpressionClasses.Num(); SubIndex++, ID++)
				{
					if (IsAllowedExpressionType(CategoryNode->MaterialExpressionClasses(SubIndex), MaterialEditor->EditMaterialFunction != NULL))
					{
						ExpressionsMap.Set( ID, FString(*CategoryNode->MaterialExpressionClasses(SubIndex)->GetName()).Mid(appStrlen(TEXT("MaterialExpression"))) );
					}
				}

				// Add the 'label'
				if( ExpressionsMap.Num() > 0 )
				{
					// Sort by name
					ExpressionsMap.ValueSort<COMPARE_CONSTREF_CLASS(FString,MaterialEditorContextMenus)>();

					wxMenu* pkNewMenu = new wxMenu();
					for( TMap<INT, FString>::TIterator It(ExpressionsMap); It; ++It )
					{
						pkNewMenu->Append(It.Key(),
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("NewF"),
							*It.Value())),
							TEXT("")
							);
					}
					Menu->Append(-1, *(CategoryNode->CategoryName), pkNewMenu);
				}
			}
		}

		// Unassigned entries...
		if (MaterialEditor->UnassignedExpressionClasses.Num() > 0)
		{
			// Search for all modules of this type
			TMap<INT, FString> ExpressionsMap;
			for (INT UnassignedIndex = 0; UnassignedIndex < MaterialEditor->UnassignedExpressionClasses.Num(); UnassignedIndex++, ID++)
			{
				if (IsAllowedExpressionType(MaterialEditor->UnassignedExpressionClasses(UnassignedIndex), MaterialEditor->EditMaterialFunction != NULL))
				{
					ExpressionsMap.Set( ID, FString(*MaterialEditor->UnassignedExpressionClasses(UnassignedIndex)->GetName()).Mid(appStrlen(TEXT("MaterialExpression"))) );
				}
			}

			// Add the 'label'
			if( ExpressionsMap.Num() > 0 )
			{
				// Sort by name
				ExpressionsMap.ValueSort<COMPARE_CONSTREF_CLASS(FString,MaterialEditorContextMenus)>();

				for( TMap<INT, FString>::TIterator It(ExpressionsMap); It; ++It )
				{
					Menu->Append(It.Key(),
						*FString::Printf(LocalizeSecure(LocalizeUnrealEd("NewF"),
						*It.Value())),
						TEXT("")
						);
				}
			}
		}
	}
	checkMsg( ID <= IDM_NEW_SHADER_EXPRESSION_END, "Insuffcient number of material expression IDs" );
}

} // namespace

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMaterialEditorContextMenu_NewNode
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Presented when the user right-clicks on an empty region of the material editor viewport.
 */
WxMaterialEditorContextMenu_NewNode::WxMaterialEditorContextMenu_NewNode(WxMaterialEditor* MaterialEditor)
{
	// New expressions.
	AppendNewShaderExpressionsToMenu( this, MaterialEditor );

	// New comment.
	AppendSeparator();
	Append( ID_MATERIALEDITOR_NEW_COMMENT, *LocalizeUnrealEd("NewComment"), TEXT("") );

	// Paste here.
	AppendSeparator();
	Append( ID_MATERIALEDITOR_PASTE_HERE, *LocalizeUnrealEd("PasteHere"), TEXT("") );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMaterialEditorContextMenu_NodeOptions
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Presented when the user right-clicks on a material expression node in the material editor.
 */
WxMaterialEditorContextMenu_NodeOptions::WxMaterialEditorContextMenu_NodeOptions(WxMaterialEditor* MaterialEditor)
{
	// Are any expressions selected?
	if( MaterialEditor->SelectedExpressions.Num() > 0 )
	{
		// Add a 'Convert To Parameter' option for convertible types
		UBOOL bFoundConstant = FALSE;
		for (INT ExpressionIndex = 0; ExpressionIndex < MaterialEditor->SelectedExpressions.Num(); ++ExpressionIndex)
		{
			UMaterialExpression* Expression = MaterialEditor->SelectedExpressions(ExpressionIndex);
			if (Expression->IsA(UMaterialExpressionConstant::StaticClass())
				|| Expression->IsA(UMaterialExpressionConstant2Vector::StaticClass())
				|| Expression->IsA(UMaterialExpressionConstant3Vector::StaticClass())
				|| Expression->IsA(UMaterialExpressionConstant4Vector::StaticClass())
				|| Expression->IsA(UMaterialExpressionTextureSample::StaticClass())
				|| Expression->IsA(UMaterialExpressionComponentMask::StaticClass()))
			{
				bFoundConstant = TRUE;
				break;
			}
		}

		if (bFoundConstant && MaterialEditor->EditMaterialFunction == NULL)
		{
			Append( ID_MATERIALEDITOR_CONVERT_TO_PARAMETER, *LocalizeUnrealEd("ConvertToParameter"), TEXT("") );
			AppendSeparator();
		}

		Append( ID_MATERIALEDITOR_SELECT_DOWNSTREAM_NODES, *LocalizeUnrealEd("SelectDownstream"), TEXT("") );
		Append( ID_MATERIALEDITOR_SELECT_UPSTREAM_NODES, *LocalizeUnrealEd("SelectUpstream"), TEXT("") );
		AppendSeparator();

		// Present "Use Current Texture" if a texture is selected in the generic browser and at
		// least on Texture Sample node is selected in the material editor.
		for ( INT ExpressionIndex = 0 ; ExpressionIndex < MaterialEditor->SelectedExpressions.Num() ; ++ExpressionIndex )
		{
			UMaterialExpression* Expression = MaterialEditor->SelectedExpressions(ExpressionIndex);
			if ( Expression->IsA(UMaterialExpressionTextureSample::StaticClass()) )
			{
				//AppendSeparator();
				Append( IDM_USE_CURRENT_TEXTURE, *LocalizeUnrealEd("UseCurrentTexture"), *LocalizeUnrealEd("UseCurrentTexture") );
				AppendSeparator();
				break;
			}
		}
	}

	// Duplicate selected.
	const INT NumSelected = MaterialEditor->GetNumSelected();
	if( NumSelected == 1 )
	{
		Append( ID_MATERIALEDITOR_DUPLICATE_NODES, *LocalizeUnrealEd("DuplicateSelectedItem"), TEXT("") );
	}
	else
	{
		Append( ID_MATERIALEDITOR_DUPLICATE_NODES, *LocalizeUnrealEd("DuplicateSelectedItems"), TEXT("") );
	}
	AppendSeparator();

	// Break all links
	Append( ID_MATERIALEDITOR_BREAK_ALL_LINKS, *LocalizeUnrealEd("BreakAllLinks"), TEXT("") );
	AppendSeparator();

	// Delete selected.
	if( NumSelected == 1 )
	{
		Append( ID_MATERIALEDITOR_DELETE_NODE, *LocalizeUnrealEd("DeleteSelectedItem"), TEXT("") );
	}
	else
	{
		Append( ID_MATERIALEDITOR_DELETE_NODE, *LocalizeUnrealEd("DeleteSelectedItems"), TEXT("") );
	}

	// Handle the favorites options
	if( MaterialEditor->SelectedExpressions.Num() == 1 )
	{
		AppendSeparator();
		UMaterialExpression* Expression = MaterialEditor->SelectedExpressions(0);
		if (MaterialEditor->IsMaterialExpressionInFavorites(Expression))
		{
			Append( ID_MATERIALEDITOR_REMOVE_FROM_FAVORITES, *LocalizeUnrealEd("RemoveFromFavorites"), TEXT("") );
		}
		else
		{
			Append( ID_MATERIALEDITOR_ADD_TO_FAVORITES, *LocalizeUnrealEd("AddToFavorites"), TEXT("") );
		}
	}

	if( MaterialEditor->SelectedExpressions.Num() == 1 )
	{
		// Add a preview node option if only one node is selected
		AppendSeparator();
		if( MaterialEditor->PreviewExpression == MaterialEditor->SelectedExpressions(0) )
		{
			// If we are already previewing the selected node, the menu option should tell the user that this will stop previewing
			Append( ID_MATERIALEDITOR_PREVIEW_NODE, *LocalizeUnrealEd("MaterialEditor_PreviewNodeStop"), TEXT("") );
		}
		else
		{
			// The menu option should tell the user this node will be previewed.
			Append( ID_MATERIALEDITOR_PREVIEW_NODE, *LocalizeUnrealEd("MaterialEditor_PreviewNodeStart"), TEXT("") );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMaterialEditorContextMenu_ConnectorOptions
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Presented when the user right-clicks on an object connector in the material editor viewport.
 */
WxMaterialEditorContextMenu_ConnectorOptions::WxMaterialEditorContextMenu_ConnectorOptions(WxMaterialEditor* MaterialEditor)
{
	UMaterialExpression* MaterialExpression = Cast<UMaterialExpression>( MaterialEditor->ConnObj );
	UMaterial* MaterialNode = Cast<UMaterial>( MaterialEditor->ConnObj );

	UBOOL bHasExpressionConnection = FALSE;
	UBOOL bHasMaterialConnection = FALSE;

	if( MaterialEditor->ConnType == LOC_OUTPUT ) // Items on left side of node.
	{
		if ( MaterialExpression )
		{
			bHasExpressionConnection = TRUE;
		}
		if ( MaterialNode )
		{
			bHasMaterialConnection = TRUE;
		}
	}
	else if ( MaterialEditor->ConnType == LOC_INPUT ) // Items on right side of node.
	{
		// Only expressions can have connectors on the left side.
		check( MaterialExpression );
		bHasExpressionConnection = TRUE;
	}

	// Break link.
	if( bHasExpressionConnection || bHasMaterialConnection )
	{
		Append( ID_MATERIALEDITOR_BREAK_LINK, *LocalizeUnrealEd("BreakLink"), TEXT("") );

		// add menu items to expression output for material connection
		if ( MaterialEditor->ConnType == LOC_INPUT ) // left side
		{
			AppendSeparator();

			// If we are editing a material function, display options to connect to function outputs
			if (MaterialEditor->EditMaterialFunction)
			{
				INT FunctionOutputIndex = 0;
				const INT NumFunctionOutputsSupported = ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputEnd - ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputBegin;
				for (INT ExpressionIndex = 0; ExpressionIndex < MaterialEditor->Material->Expressions.Num() && FunctionOutputIndex < NumFunctionOutputsSupported; ExpressionIndex++)
				{
					UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(MaterialEditor->Material->Expressions(ExpressionIndex));
					if (FunctionOutput)
					{
						Append( ID_MATERIALEDITOR_CONNECT_TO_FunctionOutputBegin + FunctionOutputIndex, 
							*FString::Printf(LocalizeSecure(LocalizeUnrealEd("ConnectToFunctionOutput"), *FunctionOutput->OutputName)), TEXT("") );
						FunctionOutputIndex++;
					}
				}
			}
			else
			{
				Append( ID_MATERIALEDITOR_CONNECT_TO_DiffuseColor, *LocalizeUnrealEd("ConnectToDiffuseColor"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_DiffusePower, *LocalizeUnrealEd("ConnectToDiffusePower"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_EmissiveColor, *LocalizeUnrealEd("ConnectToEmissiveColor"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_SpecularColor, *LocalizeUnrealEd("ConnectToSpecularColor"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_SpecularPower, *LocalizeUnrealEd("ConnectToSpecularPower"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_Opacity, *LocalizeUnrealEd("ConnectToOpacity"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_OpacityMask, *LocalizeUnrealEd("ConnectToOpacityMask"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_Distortion, *LocalizeUnrealEd("ConnectToDistortion"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_TransmissionMask, *LocalizeUnrealEd("ConnectToTransmissionMask"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_TransmissionColor, *LocalizeUnrealEd("ConnectToTransmissionColor"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_Normal, *LocalizeUnrealEd("ConnectToNormal"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_CustomLighting, *LocalizeUnrealEd("ConnectToCustomLighting"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_CustomLightingDiffuse, *LocalizeUnrealEd("ConnectToCustomLightingDiffuse"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_AnisotropicDirection, *LocalizeUnrealEd("ConnectToAnisotropicDirection"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_WorldPositionOffset, *LocalizeUnrealEd("ConnectToWorldPositionOffset"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_WorldDisplacement, *LocalizeUnrealEd("ConnectToWorldDisplacement"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_TessellationMultiplier, *LocalizeUnrealEd("ConnectToTessellationMultiplier"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_SubsurfaceInscatteringColor, *LocalizeUnrealEd("ConnectToSubsurfaceInscatteringColor"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_SubsurfaceAbsorptionColor, *LocalizeUnrealEd("ConnectToSubsurfaceAbsorptionColor"), TEXT("") );
				Append( ID_MATERIALEDITOR_CONNECT_TO_SubsurfaceScatteringRadius, *LocalizeUnrealEd("ConnectToSubsurfaceScatteringRadius"), TEXT("") );
			}
		}
	}

	// If an input connect was clicked on, append options to create new shader expressions.
	if(MaterialEditor->ConnObj)
	{
		if ( MaterialEditor->ConnType == LOC_OUTPUT )
		{
			AppendSeparator();
			wxMenu* NewConnectorMenu = new wxMenu();
			AppendNewShaderExpressionsToMenu( NewConnectorMenu, MaterialEditor );
			Append(wxID_ANY, *LocalizeUnrealEd("AddNewMaterialExpression"), NewConnectorMenu);
		}
	}
}
