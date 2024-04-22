/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PropertyWindowManager.h"
#include "PropertyWindow.h"
#include "BusyCursor.h"

IMPLEMENT_CLASS(UPropertyWindowManager);	

///////////////////////////////////////////////////////////////////////////////
//
// InitPropertySubSystem.
//
///////////////////////////////////////////////////////////////////////////////

/**
 * Allocates and initializes GPropertyWindowManager.  Called by WxLaunchApp::OnInit() to initialize.  Not rentrant.
 */
void InitPropertySubSystem()
{
	check( GPropertyWindowManager == NULL );
	GPropertyWindowManager = new UPropertyWindowManager;
	GPropertyWindowManager->AddToRoot();
	const UBOOL bWasInitializationSuccessful = GPropertyWindowManager->Initialize();
	check( bWasInitializationSuccessful );
}

///////////////////////////////////////////////////////////////////////////////
//
// UPropertyWindowManager -- property window manager.
//
///////////////////////////////////////////////////////////////////////////////

UPropertyWindowManager::UPropertyWindowManager()
	:	bInitialized( FALSE ),
		bChangesTopology(FALSE),
		ChangeType(EPropertyChangeType::Unspecified),
		bUseScriptDefinedOrder(FALSE),
		bSeqActInterpsValid(FALSE)
{
	GConfig->GetBool(TEXT("EditorFrame"), TEXT("ShowHiddenProperties"), bShowHiddenProperties, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("EditorFrame"), TEXT("UseScriptDefinedOrder"), bUseScriptDefinedOrder, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("EditorFrame"), TEXT("ShowFriendlyPropertyNames"), bShowFriendlyPropertyNames, GEditorUserSettingsIni);
	GConfig->GetBool(TEXT("EditorFrame"), TEXT("ExpandDistributions"), bExpandDistributions, GEditorUserSettingsIni);

	//force on dx11 features for UDK builds
#if UDK
	GConfig->SetBool(TEXT("UnrealEd.PropertyFilters"), TEXT("bShowD3D11Properties"), TRUE, GEditorUserSettingsIni);
#endif

	GPropertyWindowDataCache = this;
}

/**
 * Loads common bitmaps.  Safely reentrant.
 *
 * @return		TRUE on success, FALSE on fail.
 */
UBOOL UPropertyWindowManager::Initialize()
{
	if ( !bInitialized )
	{
		if ( !ArrowDownB.Load( TEXT("DownArrowLarge.png") ) )							return FALSE;
		if ( !ArrowRightB.Load( TEXT("RightArrowLarge.png") ) )							return FALSE;
		if ( !WhiteArrowDownB.Load( TEXT("DownArrowLargeWhite.png" ) ) )				return FALSE;
		if ( !WhiteArrowRightB.Load( TEXT("RightArrowLargeWhite.png") ) )				return FALSE;
		if ( !TransparentArrowDownB.Load( TEXT("DownArrowLargeTransparent.png") ) )		return FALSE;
		if ( !TransparentArrowRightB.Load( TEXT("RightArrowLargeTransparent.png") ) )	return FALSE;
		if ( !CheckBoxOnB.Load( TEXT("CheckBox_On") ) )									return FALSE;
		if ( !CheckBoxOffB.Load( TEXT("CheckBox_Off") ) )								return FALSE;
		if ( !CheckBoxUnknownB.Load( TEXT("CheckBox_Unknown") ) )						return FALSE;
		if ( !Prop_AddNewItemB.Load( TEXT("Prop_Add") ) )								return FALSE;
		if ( !Prop_RemoveAllItemsFromArrayB.Load( TEXT("Prop_Empty") ) )				return FALSE;
		if ( !Prop_InsertNewItemHereB.Load( TEXT("Prop_Insert") ) )						return FALSE;
		if ( !Prop_DeleteItemB.Load( TEXT("Prop_Delete") ) )							return FALSE;
		if ( !Prop_ShowGenericBrowserB.Load( TEXT("Prop_Browse") ) )					return FALSE;
		if ( !Prop_UseMouseToPickColorB.Load( TEXT("Prop_Pick") ) )						return FALSE;
		if ( !Prop_ClearAllTextB.Load( TEXT("Prop_Clear") ) )							return FALSE;
		if ( !Prop_UseMouseToPickActorB.Load( TEXT("Prop_Find") ) )						return FALSE;
		if ( !Prop_UseCurrentBrowserSelectionB.Load( TEXT("Prop_Use") ) )				return FALSE;
		if ( !Prop_NewObjectB.Load( TEXT("Prop_NewObject") ) )							return FALSE;
		if ( !Prop_DuplicateB.Load( TEXT("Prop_Duplicate") ) )							return FALSE;
		if ( !Prop_ResetToDefaultB.Load( TEXT("Prop_ResetToDefault") ) )				return FALSE;
		if ( !Prop_TextBoxB.Load( TEXT("Prop_TextBox") ) )								return FALSE;
		if ( !Prop_CurveEditB.Load( TEXT("Prop_CurveEd") ) )							return FALSE;

		if ( !FavoritesOnImage.Load( TEXT("Prop_FavoriteOn.png") ) )					return FALSE;
		if ( !FavoritesOffImage.Load( TEXT("Prop_FavoriteOff.png") ) )					return FALSE;

		ensure ((FavoritesOnImage.GetWidth() == FavoritesOffImage.GetWidth()) || (FavoritesOnImage.GetHeight() == FavoritesOffImage.GetHeight()));

		bInitialized = TRUE;
	}

	return TRUE;
}

/**
 * Property window management.  Registers a property window with the manager.
 *
 * @param	InPropertyWindow		The property window to add to managing control.  Must be non-NULL.
 */
void UPropertyWindowManager::RegisterWindow(WxPropertyWindow* InPropertyWindow)
{
	check( InPropertyWindow );
	PropertyWindows.AddItem( InPropertyWindow );
}

/**
 * Property window management.  Unregisters a property window from the manager.
 *
 * @param	InPropertyWindow		The property window to remove from managing control.
 */
void UPropertyWindowManager::UnregisterWindow(WxPropertyWindow* InPropertyWindow)
{
	PropertyWindows.RemoveItem( InPropertyWindow );
}

/**
 * Dissociates all set objects and hides windows.
 */
void UPropertyWindowManager::ClearReferencesAndHide()
{
	for( INT WindowIndex=0; WindowIndex<PropertyWindows.Num(); WindowIndex++ )
	{
		// Only hide if we are flagged as being able to be hidden.
		WxPropertyWindow* PropertyWindow = PropertyWindows(WindowIndex);
		check(PropertyWindow);

		const UBOOL bHideable = PropertyWindow->HasFlags(EPropertyWindowFlags::CanBeHiddenByPropertyWindowManager);
		
		WxPropertyWindowHost* HostWindow = PropertyWindow->GetParentHostWindow();
		check(HostWindow);
		UBOOL bIsLocked = HostWindow->IsLocked();
		if( bHideable && !bIsLocked)
		{
			PropertyWindow->ClearIfFromMapPackage();
		}
	}
}


/**
 * Dissociates all property windows from the specified objects
 */
void UPropertyWindowManager::ClearAllThatContainObjects(const TArray< UObject* >& InObjects)
{
	for( INT WindowIndex=0; WindowIndex<PropertyWindows.Num(); WindowIndex++ )
	{
		//if the property window contains this object, it's not safe anymore.  Clear it out.
		WxPropertyWindow* PropertyWindow = PropertyWindows(WindowIndex);
		check(PropertyWindow);
		for (INT i = 0; i < InObjects.Num(); ++i)
		{
			UObject* TestObject = InObjects(i);
			if (TestObject && PropertyWindow->ContainsObject(TestObject))
			{
				PropertyWindow->SetObject(NULL, EPropertyWindowFlags::NoFlags);
			}
		}
	}
}


/**
 * Serializes all managed property windows to the specified archive.
 *
 * @param		Ar		The archive to read/write.
 */
void UPropertyWindowManager::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	for( INT WindowIndex = 0 ; WindowIndex < PropertyWindows.Num() ; ++WindowIndex )
	{
		WxPropertyWindow* PropertyWindow = PropertyWindows(WindowIndex);
		PropertyWindow->Serialize( Ar );
	}
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UPropertyWindowManager::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects( ObjectArray );
	
	// Collect object references...
	TArray<UObject*> CollectedReferences;
	FArchiveObjectReferenceCollector ObjectReferenceCollector( &CollectedReferences );
	Serialize( ObjectReferenceCollector );
	
	// ... and add them.
	for( INT ObjectIndex=0; ObjectIndex<CollectedReferences.Num(); ObjectIndex++ )
	{
		UObject* Object = CollectedReferences(ObjectIndex);
		AddReferencedObject( ObjectArray, Object );
	}
}

/**
* Toggles whether or not hidden properties are shown
*
* @param	bInShowHiddenProperties	TRUE if all hidden properties are to be shown
*/
void UPropertyWindowManager::SetShowHiddenProperties(const UBOOL bInShowHiddenProperties)
{
	if(bShowHiddenProperties == bInShowHiddenProperties)
	{
		return;
	}

	bShowHiddenProperties = bInShowHiddenProperties;

	wxCommandEvent Event;
	Event.SetEventType(ID_TOGGLE_SHOW_HIDDEN_PROPERTIES);

	FScopedBusyCursor Cursor;
	for(int Index = 0; Index < PropertyWindows.Num(); ++Index)
	{
		Event.SetEventObject(PropertyWindows(Index));
		PropertyWindows(Index)->GetEventHandler()->ProcessEvent(Event);
	}
}

/**
 * Sets whether to use the script defined order or to use the class order
 *
 * @param	bInUseScriptDefinedOrder	TRUE if property windows should use the order of variables in the .uc file.
 */
void UPropertyWindowManager::SetUseScriptDefinedOrder(const UBOOL bInUseScriptDefinedOrder)
{
	if(bUseScriptDefinedOrder == bInUseScriptDefinedOrder)
	{
		return;
	}
	bUseScriptDefinedOrder = bInUseScriptDefinedOrder;

	GConfig->SetBool(TEXT("EditorFrame"), TEXT("UseScriptDefinedOrder"), bUseScriptDefinedOrder, GEditorUserSettingsIni);

	for(int Index = 0; Index < PropertyWindows.Num(); ++Index)
	{
		//to update sorting etc.
		PropertyWindows(Index)->RequestReconnectToData();
	}

}

/**
 * Sets whether to show horizontal dividers
 *
 * @param	bInShowHorizontalDividers	TRUE if property windows should show horizontal dividers
 */
void UPropertyWindowManager::SetShowHorizontalDividers(const UBOOL bInShowHorizontalDividers)
{
	if(bShowHorizontalDividers == bInShowHorizontalDividers)
	{
		return;
	}
	bShowHorizontalDividers = bInShowHorizontalDividers;

	GConfig->SetBool(TEXT("EditorFrame"), TEXT("ShowHorizontalDividers"), bShowHorizontalDividers, GEditorUserSettingsIni);

	for(int Index = 0; Index < PropertyWindows.Num(); ++Index)
	{
		//to update sorting etc.
		PropertyWindows(Index)->RequestReconnectToData();
	}
}


/**
* Toggles whether or not property names should be nicer looking (spaces, ? for boolean, etc)
* @param	bShowFriendlyPropertyNames	TRUE if friendly names are desired.
*/
void UPropertyWindowManager::SetShowFriendlyNames(const UBOOL bInShowFriendlyPropertyNames)
{
	if(bShowFriendlyPropertyNames == bInShowFriendlyPropertyNames)
	{
		return;
	}
	bShowFriendlyPropertyNames = bInShowFriendlyPropertyNames;

	GConfig->SetBool(TEXT("EditorFrame"), TEXT("ShowFriendlyPropertyNames"), bShowFriendlyPropertyNames, GEditorUserSettingsIni);

	for(int Index = 0; Index < PropertyWindows.Num(); ++Index)
	{
		//to update sorting etc.
		PropertyWindows(Index)->RequestReconnectToData();
	}
}


/**
 * Toggles whether or not distributions are auto-expanded in the property window.
 *
 * @param	bInExpandDistributions	TRUE if distributions should be expanded
 */ 
void UPropertyWindowManager::SetExpandDistributions(const UBOOL bInExpandDistributions)
{
	if (bExpandDistributions != bInExpandDistributions)
	{
		bExpandDistributions = bInExpandDistributions;

		FScopedBusyCursor Cursor;
		for(int Index = 0; Index < PropertyWindows.Num(); ++Index)
		{
			PropertyWindows(Index)->Rebuild();
		}
	}
}


/**
 * Gets the name of all nodes in the property tree that should be expanded (saved between runs)
 * @param InClassName - The class name (context) requested
 * @param OutExpandedNodeNames - The list of fully qualified property nodes to keep open
 */
void UPropertyWindowManager::GetExpandedItems(const FString& InClassName, OUT TArray<FString>& OutExpandedNodeNames)
{
	TArray<FString> ExpandedNames;
	GConfig->GetSingleLineArray(TEXT("PropertyWindowExpansion"), *InClassName, OutExpandedNodeNames, GEditorUserSettingsIni);
}

/**
 * Sets the name of all nodes in the property tree that should be expanded (saved between runs)
 * @param InClassName - The class name (context) requested
 * @param InExpandedNodeNames - The list of fully qualified property nodes to keep open
 */
void UPropertyWindowManager::SetExpandedItems(const FString& InClassName, const TArray<FString>& InExpandedNodeNames)
{
	GConfig->SetSingleLineArray(TEXT("PropertyWindowExpansion"), *InClassName, InExpandedNodeNames, GEditorUserSettingsIni);
}

/**Helper function to clear all cached lists for property window refreshing*/
void UPropertyWindowManager::ClearPropertyWindowCachedData()
{
	bSeqActInterpsValid = FALSE;
	CachedEditedSeqActInterps.Empty();
}

/**Helper function to retrieve a current list of all USeqAct_Interps for use in property window updating */
const TArray<UObject*>& UPropertyWindowManager::GetEditedSeqActInterps (void)
{
	if (!bSeqActInterpsValid)
	{
		bSeqActInterpsValid = TRUE;

		for( TObjectIterator<USeqAct_Interp> It; It; ++It )
		{
			USeqAct_Interp *Interp = *It;
			if ( Interp && Interp->bIsBeingEdited )
			{
				CachedEditedSeqActInterps.AddItem(Interp);
			}
		}
	}

	return CachedEditedSeqActInterps;
}

