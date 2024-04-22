/*=============================================================================
	MaterialInstanceTimeVaryingEditor.h: Material instance editor class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _MATERIAL_INSTANCE_TIME_VARYING_EDITOR_H_
#define _MATERIAL_INSTANCE_TIME_VARYING_EDITOR_H_

#include "ItemPropertyNode.h"
#include "PropertyWindow.h"
#include "MaterialEditorBase.h"

// Forward declarations.
class WxPropertyWindow;
class WxMaterialInstanceTimeVaryingEditorToolBar;

/**
 * Custom property window class for displaying material instance parameters.
 */
class WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter : public WxCustomPropertyItem_ConditionalItem
{
public:
	DECLARE_DYNAMIC_CLASS(WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter);

	/** Whether or not to allow editing of properties. */
	UBOOL bAllowEditing;

	/** Name of the struct that holds this property. */
	FName PropertyStructName;

	/** Save the id for future visibility checks */
	FGuid ExpressionId;

	// Constructor
	WxCustomPropertyItem_MaterialInstanceTimeVaryingParameter();

	/**
	* Initialize this property window.  Must be the first function called after creating the window.
	*/
	virtual void Create(wxWindow* InParent);

	/**
	 * Returns true if this item supports display/toggling a check box for it's conditional parent property
	 *
	 * @return True if a check box control is needed for toggling the EditCondition property here
	 */
	virtual UBOOL SupportsEditConditionCheckBox() const
	{
		return TRUE;
	}

	/**
	 * Returns TRUE if MouseX and MouseY fall within the bounding region of the checkbox used for displaying the value of this property's edit condition.
	 */
	virtual UBOOL ClickedCheckbox( INT MouseX, INT MouseY ) const;

	/**
	 * Toggles the value of the property being used as the condition for editing this property.
	 *
	 * @return	the new value of the condition (i.e. TRUE if the condition is now TRUE)
	 */
	virtual UBOOL ToggleConditionValue();

	/**
	 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
	 * associated property should be allowed.
	 */
	virtual UBOOL IsConditionMet();

	/**
	 * @return TRUE if the property is overridden, FALSE otherwise.
	 */
	virtual UBOOL IsOverridden();

	/** @return Returns the instance object this property is associated with. */
	UMaterialEditorInstanceTimeVarying* GetInstanceObject();

	/**
	 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
	 * and (if the property window item is expandable) to toggle expansion state.
	 *
	 * @param	Event	the mouse click input that generated the event
	 *
	 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
	 */
	UBOOL ClickedPropertyItem( wxMouseEvent& Event );

	/**Returns the rect for checkbox used in conditional item property controls.*/
	virtual wxRect GetConditionalRect (void) const;

	/**
	 * Renders the left side of the property window item.
	 *
	 * This version is responsible for rendering the checkbox used for toggling whether this property item window should be enabled,
	 * as well as setting the position and size of the Reset to Default button. 
	 *
	 * @param	RenderDeviceContext		the device context to use for rendering the item name
	 * @param	ClientRect				the bounding region of the property window item
	 */
	virtual void RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect );

	/** Reset to default button event. */
	virtual void OnResetToDefault(wxCommandEvent &Event);

	/**
	* Overriden function to allow hiding when not referenced.
	*/
	virtual UBOOL IsDerivedForcedHide (void) const;
	/**
	* Overriden to allow custom naming
	*/
	virtual FString GetDisplayName (void) const { return DisplayName; }

private:

	/** Reset to default button. */
	wxBitmapButton* ResetToDefault;

	DECLARE_EVENT_TABLE()
	
};

/**
 * Custom property window item class for displaying material instance parameter arrays unwrapped.
 */
class WxPropertyWindow_MaterialInstanceTimeVaryingParameters : public WxItemPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow_MaterialInstanceTimeVaryingParameters);

	virtual void InitWindowDefinedChildNodes(void);
private:
};


/**
 * Main material instance editor window class.
 */
class WxMaterialInstanceTimeVaryingEditor : public WxMaterialEditorBase, public FSerializableObject, public FDockingParent, FNotifyHook
{
public:

	WxPropertyWindowHost* PropertyWindow;						/** Property window to display instance parameters. */
	UMaterialEditorInstanceTimeVarying* MaterialEditorInstance;	/** Object that stores all of the possible parameters we can edit. */
	WxListView* InheritanceList;								/** List of the inheritance chain for this material instance. */
	TArray<UMaterialInterface*> ParentList;						/** List of parents used to populate the inheritance list chain. */
	TArray<UMaterialInstance*> ChildList;						/** List of children used to populate the inheritance list chain. */
	UBOOL bChildListBuilt;										/** Flag to indicate whether the child list has attempted to be built */

	/** The material editor's toolbar. */
	WxMaterialInstanceTimeVaryingEditorToolBar*		ToolBar;

	WxMaterialInstanceTimeVaryingEditor(wxWindow* parent, wxWindowID id, UMaterialInterface* InMaterialInterface);
	virtual ~WxMaterialInstanceTimeVaryingEditor();

	virtual void Serialize(FArchive& Ar);

	/** Pre edit change notify for properties. */
	void NotifyPreChange(void* Src, UProperty* PropertyThatChanged);

	/** Post edit change notify for properties. */
	void NotifyPostChange(void* Src, UProperty* PropertyThatChanged);

	/** Rebuilds the inheritance list for this material instance. */
	void RebuildInheritanceList();

	/** Rebuilds the parent list for this material instance. */
	void RebuildParentList();

	/** Rebuilds the child list for this material instance. */
	void RebuildChildList();

protected:

	/** Saves editor settings. */
	void SaveSettings();

	/** Loads editor settings. */
	void LoadSettings();

	/** Syncs the GB to the selected parent in the inheritance list. */
	void SyncSelectedParentToGB();

	/** Opens the editor for the selected parent. */
	void OpenSelectedParentEditor();

	/** Event handler for when the user wants to sync the GB to the currently selected parent. */
	void OnMenuSyncToGB(wxCommandEvent &Event);

	/** Event handler for when the user wants to open the editor for the selected parent material. */
	void OnMenuOpenEditor(wxCommandEvent &Event);

	/** Double click handler for the inheritance list. */
	void OnInheritanceListDoubleClick(wxListEvent &ListEvent);

	/** Event handler for when the user right clicks on the inheritance list. */
	void OnInheritanceListRightClick(wxListEvent &ListEvent);

	/** Event handler for when the user wants to toggle showing all material parameters. */
	void OnShowAllMaterialParameters(wxCommandEvent &Event);

	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

	virtual UObject* GetSyncObject();

private:
	DECLARE_EVENT_TABLE()
};

#endif // _MATERIAL_INSTANCE_TIME_VARYING_EDITOR_H_

