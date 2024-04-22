/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


#ifndef __LIGHTMASS_PROPERTY_NODE_H__
#define __LIGHTMASS_PROPERTY_NODE_H__

#include "ItemPropertyNode.h"

//
//	Lightmass Custom Properties
//
/**
 * Custom property window class for displaying material instance parameters.
 */
class WxCustomPropertyItem_LightmassMaterialParameter : public WxCustomPropertyItem_ConditionalItem
{
public:
	DECLARE_DYNAMIC_CLASS(WxCustomPropertyItem_LightmassMaterialParameter);

	/** Whether or not to allow editing of properties. */
	UBOOL bAllowEditing;

	/** Name of the struct that holds this property. */
	FName PropertyStructName;

	// Constructor
	WxCustomPropertyItem_LightmassMaterialParameter();

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
	 * Toggles the value of the property being used as the condition for editing this property.
	 *
	 * @return	the new value of the condition (i.e. TRUE if the condition is now TRUE)
	 */
	virtual UBOOL ToggleConditionValue();

	/**
	 * Returns TRUE if MouseX and MouseY fall within the bounding region of the checkbox used for displaying the value of this property's edit condition.
	 */
	virtual UBOOL ClickedCheckbox( INT MouseX, INT MouseY ) const;

	/**
	 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
	 * associated property should be allowed.
	 */
	virtual UBOOL IsConditionMet();

	/**
	 * @return TRUE if the property is overridden, FALSE otherwise.
	 */
	virtual UBOOL IsOverridden();

	/** @return	The parent of the instance object this property is associated with. */
	class UMaterialInterface* GetInstanceObjectParent();
	/** @return	The Lightmass settings for the instance object this property is associated with. */
	struct FLightmassParameterizedMaterialSettings* GetInstanceLightmassSettings();

	/** Call PostEditChange on the instance object this property is associated with. */
	void InstanceObjectPostEditChange();
	/** Call CopyToSourceInstance on the instance object this property is associated with. */
	void InstanceObjectCopyToSource();

	/**
	 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
	 * and (if the property window item is expandable) to toggle expansion state.
	 *
	 * @param	Event	the mouse click input that generated the event
	 *
	 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
	 */
	UBOOL ClickedPropertyItem( wxMouseEvent& Event );

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
	 * Overriden to allow custom naming
	 */
	virtual FString GetDisplayName (void) const { return DisplayName; }
private:

	/** Reset to default button. */
	class WxPropertyItemButton* ResetToDefault;

	DECLARE_EVENT_TABLE()
	
};

/**
 * Custom property window item class for displaying material instance parameter arrays unwrapped.
 */
class WxPropertyWindow_LightmassMaterialParameters : public WxItemPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyWindow_LightmassMaterialParameters);

	/**
	 * Called by InitChildNodes(), creates any child items for any properties within this item.
	 * @param InFilterString - List of strings that must be in the property name in order to display
	 *					Empty InFilterStrings means display as normal
	 *					All strings must match in order to be accepted by the filter
	 */
	virtual void InitWindowDefinedChildNodes();
};
/**
 * Custom property window item class for displaying Lightmass level settings in the WorldProperties.
 */
class WxCustomPropertyItem_LightmassWorldInfoSettingsParameter : public WxItemPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxCustomPropertyItem_LightmassWorldInfoSettingsParameter);

	/**
	 * Called by InitChildNodes(), creates any child items for any properties within this item.
	 * @param InFilterString - List of strings that must be in the property name in order to display
	 *					Empty InFilterStrings means display as normal
	 *					All strings must match in order to be accepted by the filter
	 */
	virtual void InitWindowDefinedChildNodes();
};

#endif // __LIGHTMASS_PROPERTY_NODE_H__
