/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PROPERTYWINDOWMANAGER_H__
#define __PROPERTYWINDOWMANAGER_H__

#include "Bitmaps.h"

// Forward declarations.
class WxPropertyWindow;


/**
 * Allocates and initializes GPropertyWindowManager.  Called by WxLaunchApp::OnInit() to initialize.  Not rentrant.
 */
void InitPropertySubSystem();

/**
 * A container for all property windows.  Manages property window serialization.
 */
class UPropertyWindowManager : public UObject, public FPropertyWindowDataCache
{
	DECLARE_CLASS_INTRINSIC(UPropertyWindowManager,UObject,CLASS_Transient|0,UnrealEd);

	UPropertyWindowManager();

	/**
	 * Loads common bitmaps.  Safely reentrant.
	 *
	 * @return		TRUE on success, FALSE on fail.
	 */
	UBOOL Initialize();

	/**
	 * Property window management.  Registers a property window with the manager.
	 *
	 * @param	InPropertyWindow		The property window to add to managing control.  Must be non-NULL.
	 */
	void RegisterWindow(WxPropertyWindow* InPropertyWindow);

	/**
	 * Property window management.  Unregisters a property window from the manager.
	 *
	 * @param	InPropertyWindow		The property window to remove from managing control.
	 */
	void UnregisterWindow(WxPropertyWindow* InPropertyWindow);

	/**
	 * Serializes all managed property windows to the specified archive.
	 *
	 * @param		Ar		The archive to read/write.
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	void AddReferencedObjects(TArray<UObject*>& ObjectArray);

	/**
	 * Dissociates all set objects and hides windows.
	 */
	void ClearReferencesAndHide();

	/**
	 * Dissociates all property windows from the specified objects
	 */
	void ClearAllThatContainObjects(const TArray< UObject* >& InObjects);

	/** The list of active property windows. */
	TArray<WxPropertyWindow*> PropertyWindows;

	/** Accessor for bShowAllItemButtons */
	UBOOL GetShowHiddenProperties() const
	{
		return bShowHiddenProperties;
	}
	/** Accessor for bUseScriptDefinedOrder*/
	UBOOL GetUseScriptDefinedOrder() const
	{
		return bUseScriptDefinedOrder;
	}	

	/** Accessor for bShowHorizontalDividers*/
	UBOOL GetShowHorizontalDividers()
	{
		GConfig->GetBool(TEXT("PropertyWindow"), TEXT("ShowHorizontalDividers"), bShowHorizontalDividers, GEditorUserSettingsIni);
		return bShowHorizontalDividers;
	}

	/** Accessor for bShowFriendlyPropertyNames */
	UBOOL GetShowFriendlyPropertyNames() const
	{
		return bShowFriendlyPropertyNames;
	}

	/** Accessor for bExpandDistributions */
	UBOOL GetExpandDistributions() const
	{
		return bExpandDistributions;
	}

	/**
	 * Toggles whether or not the buttons for each property item are shown
	 * regardless of whether or not they currently have focus.
	 *
	 * @param	bShowButtons	TRUE if all buttons are to be shown.
	 */
	void SetShowHiddenProperties(const UBOOL bInShowHiddenProperties);

	/**
	 * Sets whether to use the script defined order or to use the class order
	 *
	 * @param	bInUseScriptDefinedOrder	TRUE if property windows should use the order of variables in the .uc file.
	 */
	void SetUseScriptDefinedOrder(const UBOOL bInUseScriptDefinedOrder);

	/**
	 * Sets whether to show horizontal dividers
	 *
	 * @param	bInUseScriptDefinedOrder	TRUE if property windows should show horizontal dividers
	 */
	void SetShowHorizontalDividers(const UBOOL bInShowHorizontalDividers);

	/**
	 * Toggles whether or not property names should be nicer looking (spaces, ? for boolean, etc)
	 * @param	bShowFriendlyPropertyNames	TRUE if friendly names are desired.
	 */
	void UPropertyWindowManager::SetShowFriendlyNames(const UBOOL bInShowFriendlyPropertyNames);

	/**
	 * Toggles whether or not distributions are auto-expanded in the property window.
	 *
	 * @param	bInExpandDistributions	TRUE if distributions should be expanded
	 */ 
	void SetExpandDistributions(const UBOOL bInExpandDistributions);

	/**
	 * Gets the name of all nodes in the property tree that should be expanded (saved between runs)
	 * @param InClassName - The class name (context) requested
	 * @param OutExpandedNodeNames - The list of fully qualified property nodes to keep open
	 */
	void GetExpandedItems(const FString& InClassName, OUT TArray<FString>& OutExpandedNodeNames);

	/**
	 * Sets the name of all nodes in the property tree that should be expanded (saved between runs)
	 * @param InClassName - The class name (context) requested
	 * @param InExpandedNodeNames - The list of fully qualified property nodes to keep open
	 */
	void SetExpandedItems(const FString& InClassName, const TArray<FString>& InExpandedNodeNames);

	/**Helper function to clear all cached lists for property window refreshing*/
	void ClearPropertyWindowCachedData();

	/**Retrieves a current list of all USeqAct_Interps for use in property window updating */
	virtual const TArray<UObject*>& GetEditedSeqActInterps (void);

public:
	// @todo DB: create a solution for dynamically binding bitmaps.

	// Bitmaps used by all property windows.
	WxBitmap	TransparentArrowDownB;
	WxBitmap	TransparentArrowRightB;
	WxBitmap	ArrowDownB;
	WxBitmap	ArrowRightB;
	WxBitmap	WhiteArrowDownB;
	WxBitmap	WhiteArrowRightB;
	WxMaskedBitmap	CheckBoxOnB;
	WxMaskedBitmap	CheckBoxOffB;
	WxMaskedBitmap	CheckBoxUnknownB;
	WxMaskedBitmap	Prop_AddNewItemB;
	WxMaskedBitmap	Prop_RemoveAllItemsFromArrayB;
	WxMaskedBitmap	Prop_InsertNewItemHereB;
	WxMaskedBitmap	Prop_DeleteItemB;
	WxMaskedBitmap	Prop_ShowGenericBrowserB;
	WxMaskedBitmap	Prop_UseMouseToPickColorB;
	WxMaskedBitmap	Prop_ClearAllTextB;
	WxMaskedBitmap	Prop_UseMouseToPickActorB;
	WxMaskedBitmap	Prop_UseCurrentBrowserSelectionB;
	WxMaskedBitmap	Prop_NewObjectB;
	WxMaskedBitmap	Prop_DuplicateB;
	WxMaskedBitmap	Prop_ResetToDefaultB;
	WxMaskedBitmap	Prop_TextBoxB;
	WxMaskedBitmap	Prop_CurveEditB;

	/**Bitmaps for favorites*/
	WxBitmap FavoritesOnImage;
	WxBitmap FavoritesOffImage;

	/** Used during callbacks, if TRUE the topology has changed and all related property windows should be fully reconnected*/
	UBOOL bChangesTopology;

	/** Used during callbacks, represents the type of change that's taking place*/
	EPropertyChangeType::Type ChangeType;

private:
	/** Indicates whether or not the manager has been initialized ie Initialize() was successfully called. */
	UBOOL			bInitialized;

	/** Flag indicating whether each property item should display its buttons regardless of whether or not it currently has focus. */
	UBOOL bShowHiddenProperties;

	/** Flag indicating whether to use the order defined by the script rather than the order in the class */
	UBOOL bUseScriptDefinedOrder;

	/** Flag indicating whether to show the horizontal dividers between properties */
	UBOOL bShowHorizontalDividers;

	/** Flag indicating whether each property item should display a more spaced out legible name. */
	UBOOL bShowFriendlyPropertyNames;
	
	/** If TRUE, then expand distributions in property windows */
	UBOOL bExpandDistributions;


	/** TRUE if the following array of SeqAct_Interps is valid */
	UBOOL bSeqActInterpsValid;
	/** Temporary cached array of seq act interps for use during property window refreshing */
	TArray<UObject*> CachedEditedSeqActInterps;


};

#endif // __PROPERTYWINDOWMANAGER_H__
