/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ITEM_PROPERTY_NODE_H__
#define __ITEM_PROPERTY_NODE_H__

#include "PropertyNode.h"

/*-----------------------------------------------------------------------------
	WxItemPropertyControl
-----------------------------------------------------------------------------*/
class WxItemPropertyControl : public WxPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxItemPropertyControl);

	/** Make sure the display name is set to NULL */
	WxItemPropertyControl() { DisplayName.Empty();}

	/**
	 * Virtual function to allow generation of display name
	 */
	virtual void InitBeforeCreate(void);
	/**
	 * Initialize this property window.  Must be the first function called after creating the window.
	 */
	virtual void Create(wxWindow* InParent);

	/** Does nothing to avoid flicker when we redraw the screen. */
	void OnEraseBackground( wxEraseEvent& Event )
	{
		// Intentionally do nothing.
	}

	void OnPaint( wxPaintEvent& In );
	void OnInputTextEnter( wxCommandEvent& In );
	void OnInputTextChanged( wxCommandEvent& In );
	void OnInputComboBox( wxCommandEvent& In );

	/**
	 * Renders the expansion arrow next to the property window item.
	 *
	 * @param	RenderDeviceContext		the device context to use for rendering the item name
	 * @param Color		the color to draw the arrow
	 */
	virtual void RenderExpansionArrow( wxBufferedPaintDC& RenderDeviceContext, const wxColour& Color );

	/**
	 * Renders the left side of the property window item.
	 *
	 * @param	RenderDeviceContext		the device context to use for rendering the item name
	 * @param	ClientRect				the bounding region of the property window item
	 */
	virtual void RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect );

	/**
	 * Displays a context menu for this property window node.
	 *
	 * @param	In	the mouse event that caused the context menu to be requested
	 *
	 * @return	TRUE if the context menu was shown.
	 */
	virtual UBOOL ShowPropertyItemContextMenu( wxMouseEvent& In );

	/**
	 * Resets the value of the property associated with this item to its default value, if applicable.
	 */
	virtual void ResetPropertyValueToDefault();

	/**
	 * Sees if the value of this property differs from any of the objects it belongs to (deals with multi-select)
	 */
	UBOOL DiffersFromDefault(UObject* InObject, UProperty* InProperty);

		/**
	 * virtualized to allow custom naming
	 */
	virtual FString GetItemName (void) const;

	/**
	 * virtualized to allow custom naming
	 */
	virtual FString GetDisplayName (void) const;

	/**
	 * Let's derived classes override the display name chosen by default
	 */
	void SetDisplayName(const FString& InName)
	{
		DisplayName = InName;
	}

	/**Returns the rect for favorites toggle*/
	wxRect GetFavoritesRect (void) const;

	/**Returns the rect for expansion arrow*/
	virtual wxRect GetExpansionArrowRect (void) const;

	/**Returns the rect for checkbox used in conditional item property controls.  In the base implementation to keep it close to the favorites rect function*/
	virtual wxRect GetConditionalRect (void) const;

	/**Returns the left side of the title of this property*/
	virtual wxRect GetItemNameRect (const INT Width, const INT Height) const;

	/**
	 * Returns TRUE if MouseX and MouseY fall within the bounding region of the favorites bitmap 
	 */
	UBOOL ClickedFavoritesToggle( INT MouseX, INT MouseY ) const;

	/**
	 * Returns the bitmap to be used as the favorite, null if none
	 */
	const WxBitmap* GetFavoriteBitmap( void ) const;

	/**
	 * Returns the width of the bitmap to be used as the favorite
	 */
	const INT GetFavoriteWidth( void ) const;

	/**Toggles whether or not this value is a favorite or not*/
	void ToggleFavoriteState(void);
	
	/**
	 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
	 * and (if the property window item is expandable) to toggle expansion state.
	 *
	 * @param	Event	the mouse click input that generated the event
	 *
	 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
	 */
	virtual UBOOL ClickedPropertyItem( wxMouseEvent& In );


	DECLARE_EVENT_TABLE();

protected:
	/**
	 * A human readable version of the property name.
	 */
	FString DisplayName;
};
//-----------------------------------------------------------------------------
//FItemPropertyNode - All other non-category/object nodes
//-----------------------------------------------------------------------------
class FItemPropertyNode : public FPropertyNode
{
public:
	FItemPropertyNode(void);
	virtual ~FItemPropertyNode(void);

	/**
	 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
	 *
	 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
	 */
	virtual BYTE* GetValueBaseAddress( BYTE* Base );
	/**
	 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
	 * to dynamic array elements, the pointer returned will be the location for that element's data. 
	 *
	 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
	 */
	virtual BYTE* GetValueAddress( BYTE* Base );

protected :
	/**
	 * Overridden function for special setup
	 */
	virtual void InitExpansionFlags (void);
	/**
	 * Overridden function for Creating Child Nodes
	 */
	virtual void InitChildNodes(void);
	/**
	 * Overridden function for Creating the corresponding wxPropertyControl
	 */
	virtual void CreateInternalWindow(void);
	/**
	 * Function to allow different nodes to add themselves to focus or not
	 */
	virtual void AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& OutVisibleArray);

private:
	/**
	 * Helper function to fix up rotator child properties
	 * @param InStructProperty - the rotator property to be adjusted
	 */
	void InitRotatorStructure(UStructProperty* InStructProperty);

};

/** Stores information about property condition metadata. */
struct FPropertyConditionInfo
{
public:
	/** Address of the target property. */
	BYTE* Address;
	/** Whether the condition should be negated. */
	UBOOL bNegateValue;
};

/**
 * The purpose of this custom property editing control is to provide a method for linking the editability of one property to the value
 * of another property.  Only when the value of the other (known as the "conditional") property matches the required value will this
 * property window item be editable.
 * <p>
 * Currently, only bool properties can be used as the conditional property, and the conditional property must be contained within the same
 * struct as this item's property.
 * <p>
 * This property editing control renders an additional checkbox just to the left of the property name, which is used to control the value
 * of the conditional property.  When this checkbox is disabled, the text for this property editing control is greyed out.
 * <p>
 * It is registered by adding an entry to the CustomPropertyClasses array of the CustomPropertyItemBindings section of the Editor .ini.  Conditionals
 * are specified using property metadata in the format <EditCondition=ConditionalPropertyName>.
 */
class WxCustomPropertyItem_ConditionalItem : public WxItemPropertyControl
{
public:
	DECLARE_DYNAMIC_CLASS(WxCustomPropertyItem_ConditionalItem);

	/**
	 * Returns the property used for determining whether this item's Property can be edited.
	 *
	 * @todo ronp - eventually, allow any type of property by specifying required value in metadata
	 */
	UBoolProperty* GetEditConditionProperty(UBOOL& bNegate) const;

	/**
	 * Finds the property being used to determine whether this item's associated property should be editable/expandable.
	 * If the propery is successfully found, ConditionPropertyAddresses will be filled with the addresses of the conditional properties' values.
	 *
	 * @param	ConditionProperty	receives the value of the property being used to control this item's editability.
	 * @param	ConditionPropertyAddresses	receives the addresses of the actual property values for the conditional property
	 *
	 * @return	TRUE if both the conditional property and property value addresses were successfully determined.
	 */
	UBOOL GetEditConditionPropertyAddress( UBoolProperty*& ConditionProperty, TArray<FPropertyConditionInfo>& ConditionPropertyAddresses );

	/**
	 * Returns TRUE if MouseX and MouseY fall within the bounding region of the checkbox used for displaying the value of this property's edit condition.
	 */
	virtual UBOOL ClickedCheckbox( INT MouseX, INT MouseY ) const;

	/**
	 * Returns true if this item supports display/toggling a check box for it's conditional parent property
	 *
	 * @return True if a check box control is needed for toggling the EditCondition property here
	 */
	virtual UBOOL SupportsEditConditionCheckBox() const;

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

	/**Returns the rect for checkbox used in conditional item property controls.  In the base implementation to keep it close to the favorites rect function*/
	virtual wxRect GetConditionalRect (void) const;

	/**
	 * Renders the left side of the property window item.
	 *
	 * This version is responsible for rendering the checkbox used for toggling whether this property item window should be enabled.
	 *
	 * @param	RenderDeviceContext		the device context to use for rendering the item name
	 * @param	ClientRect				the bounding region of the property window item
	 */
	virtual void RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect );

	/**
	 * Expands child items.  Does nothing if the window is already expanded.
	 */
	virtual void Expand(const TArray<FString>& InFilterStrings);

	/**
	 * Recursively searches through children for a property named PropertyName and expands it.
	 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
	 */
	virtual void ExpandItem( const FString& PropertyName, INT ArrayIndex );

	/**
	 * Expands all items rooted at the property window node.
	 */
	virtual void ExpandAllItems(const TArray<FString>& InFilterStrings);

	/**
	 *	Allows custom hiding of controls if conditions aren't met for conditional items
	 */
	virtual UBOOL IsControlsSuppressed (void)
	{
		if( WxItemPropertyControl::IsControlsSuppressed() )
		{
			return TRUE;
		}
		return !IsConditionMet();
	}

protected:
	/**
	 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
	 * and (if the property window item is expandable) to toggle expansion state.
	 *
	 * @param	Event	the mouse click input that generated the event
	 *
	 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
	 */
	virtual UBOOL ClickedPropertyItem( wxMouseEvent& In );

	/**
	 * Called when property window item receives a left down click.  Derived version must test for double clicking in the check box.
	 * @param InX - Window relative cursor x position
	 * @param InY - Window relative cursor y position
	 */
	virtual void PrivateLeftDoubleClick(const INT InX, const INT InY);

};

#endif // __ITEM_PROPERTY_NODE_H__
