/*=============================================================================
	PropertyUtils.cpp: Editor property window edit and draw code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PROPERTYUTILS_H__
#define __PROPERTYUTILS_H__

// Forward declarations
class UPropertyInputProxy;
class WxNumericPropertySpinner;
class WxRangedSlider;
class FPropertyNode;
class FObjectPropertyNode;
class WxPropertyControl;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Property drawing
//
///////////////////////////////////////////////////////////////////////////////////////////////////

/*-----------------------------------------------------------------------------
	UPropertyDrawProxy
-----------------------------------------------------------------------------*/

/**
 * Allows customized drawing for properties.
 */
class UPropertyDrawProxy : public UObject
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawProxy,UObject,0,UnrealEd)

	/**
	 * @return Controls whether or not the parent property can expand.
	 */
	virtual UBOOL LetPropertyExpand() const
	{
		return TRUE;
	}

	/**
	 * @return	Returns the height required by the input proxy.
	 */
	virtual INT GetProxyHeight() const;

	/**
	 * Determines if this input proxy supports the specified UProperty.
	 *
	 * @param	InItem			The property window node.
	 * @param	InArrayIdx		The index in the case of array properties.
	 *
	 * @return					TRUE if the draw proxy supports the specified property.
	 */
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
	{ return FALSE; }

	/**
	 * Draws the value of InProperty inside of InRect.
	 *
	 * @param	InDC			The wx device context.
	 * @param	InRect			The rect to draw the value into.
	 * @param	InReadAddress	The address of the property value.
	 * @param	InProperty		The property itself.
	 * @param	InInputProxy	The input proxy for the property.
	 */
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );

	/**
	 * Draws a representation of an unknown value (i.e. when multiple objects have different values in the same field).
	 *
	 * @param	InDC			The wx device context.
	 * @param	InRect			The rect to draw the value into.
	 */
	virtual void DrawUnknown( wxDC* InDC, wxRect InRect );

	/**
	 * Associates this draw proxy with a given control so we can access the
	 * input proxy if needed.
	 * @param	InControl	The property control to associate.
	 */
	virtual void AssociateWithControl(WxPropertyControl* /*InControl*/)
	{ }
};

/*-----------------------------------------------------------------------------
	UPropertyDrawNumeric
-----------------------------------------------------------------------------*/

class UPropertyDrawNumeric : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawNumeric,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawRangedNumeric
-----------------------------------------------------------------------------*/

class UPropertyDrawRangedNumeric : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawRangedNumeric,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawColor
-----------------------------------------------------------------------------*/

class UPropertyDrawColor : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawColor,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawRotation
-----------------------------------------------------------------------------*/

class UPropertyDrawRotation : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawRotation,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawRotationHeader
-----------------------------------------------------------------------------*/

class UPropertyDrawRotationHeader : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawRotationHeader,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawBool
-----------------------------------------------------------------------------*/

class UPropertyDrawBool : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawBool,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
	virtual void DrawUnknown( wxDC* InDC, wxRect InRect );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawArrayHeader
-----------------------------------------------------------------------------*/

class UPropertyDrawArrayHeader : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawArrayHeader,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};


/*-----------------------------------------------------------------------------
UPropertyDrawTextEditBox
-----------------------------------------------------------------------------*/

class UPropertyDrawTextEditBox : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawTextEditBox,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
UPropertyDrawMultilineTextBox
-----------------------------------------------------------------------------*/

class UPropertyDrawMultilineTextBox : public UPropertyDrawProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyDrawMultilineTextBox,UPropertyDrawProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;

	/**
	 * @return	Returns the height required by the draw proxy.
	 */
	virtual INT GetProxyHeight() const;

	/**
	 * Associates this draw proxy with a given control so we can access the
	 * input proxy if needed.
	 *
	 * @param	InControl	The property control to associate.
	 */
	virtual void AssociateWithControl(WxPropertyControl* InControl);

	/**
	 * Used to retrieve the height of the text control in the input proxy.
	 */
	WxPropertyControl* PropertyControl;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Property input
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * An object/addr pair, passed in bulk into UPropertyInputProxy methods.
 */
class FObjectBaseAddress
{
public:
	FObjectBaseAddress()
		:	Object( NULL )
		,	BaseAddress( NULL )
	{}
	FObjectBaseAddress(UObject* InObject, BYTE* InBaseAddress)
		:	Object( InObject )
		,	BaseAddress( InBaseAddress )
	{}

	UObject*	Object;
	BYTE*		BaseAddress;
};

/*-----------------------------------------------------------------------------
WxPropertyItemButton
-----------------------------------------------------------------------------*/
/**
 * Wraps wxBitmapButton and overrides the erase background event to prevent flickering.
 */
class WxPropertyItemButton : public wxBitmapButton
{
public:
	DECLARE_DYNAMIC_CLASS(WxPropertyItemButton);

	/**
	 * Constructor.
	 */
	WxPropertyItemButton();
	/**
	 * Constructor.
	 */
	WxPropertyItemButton(wxWindow *parent,
		wxWindowID id,
		const wxBitmap& bitmap,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxBU_AUTODRAW,
		const wxValidator& validator = wxDefaultValidator,
		const wxString& name = wxButtonNameStr);

	/** Overloads the erase background event to prevent flickering */
	virtual void OnEraseBackground(wxEraseEvent &Event);

	DECLARE_EVENT_TABLE();
};

/*-----------------------------------------------------------------------------
	UPropertyInputProxy
-----------------------------------------------------------------------------*/

/**
 * Handles user input for a property value.
 */
class UPropertyInputProxy : public UObject
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputProxy,UObject,0,UnrealEd)

	UPropertyInputProxy()
		:	ComboBox( NULL ),
			LastResizeRect(0, 0, 0, 0)
	{
	}

	/** Property button enums. */
	enum ePropButton
	{
		PB_Add,
		PB_Empty,
		PB_Insert,
		PB_Delete,
		PB_Browse,
		PB_Clear,
		PB_Find,
		PB_Use,
		PB_NewObject,
		PB_Duplicate,
		PB_TextBox,
		PB_CurveEdit,
	};


protected:
	/** Any little buttons that appear on the right side of a property window item. */
	TArray<WxPropertyItemButton*> Buttons;

	/** Drop-down combo box used for editing certain properties (eg enums). */
	wxComboBox* ComboBox;

	wxRect LastResizeRect;

private:
	//////////////////////////////////////////////////////////////////////////
	// Splitter

	/**
	 * Adds a button to the right side of the item window.
	 *
	 * @param	InItem			The parent node of the new button.
	 * @param	InButton		Specifies the type of button to add.
	 * @param	InRC			The rect to place the button in.
	 */
	void AddButton( WxPropertyControl* InItem, ePropButton InButton, const wxRect& InRC );

	/**
	* Creates any buttons that the property needs.  Empties out all buttons if no property is bound
	* or if the property is const.
	*
	* @param	InItem			The parent node of the new button.
	* @param	InRC			The rect to place the button in.
	*/
	void CreateButtons( WxPropertyControl* InItem, const wxRect& InRC );

	/**
	* Deletes any buttons associated with the property window item.
	*/
	void DeleteButtons();

public:

	/**
	 * @return	Returns the height required by the input proxy.
	 */
	virtual INT GetProxyHeight() const;

	/**
	 * @return Controls whether or not the parent property can expand.
	 */
	virtual UBOOL LetPropertyExpand() const
	{
		return TRUE;
	}

	/**
	 * @return		The number of buttons currently active on this control.
	 */
	INT GetNumButtons() const		{ return Buttons.Num(); }

	/**
	 * Creates any controls that are needed to edit the property.  Derived classes that implement
	 * this method should call up to their parent.
	 *
	 * @param	InItem			The parent node of the new controls.
	 * @param	InBaseClass		The leafest common base class of the objects being edited.
	 * @param	InRC			The rect to place the controls in.
	 * @param	InValue			The current property value.
	 */
	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue )
	{
		CreateButtons( InItem, InRC );
	}

	/**
	 * Deletes any controls which were created for editing.  Baseclass behaviour is to delete
	 * all buttons.  Derived classes that implement this method should call up to their parent.
	 */
	virtual void DeleteControls()
	{
		DeleteButtons();
	}

	/**
	 * Determines if this input proxy supports the specified UProperty.
	 *
	 * @param	InItem			The property window node.
	 * @param	InArrayIdx		The index in the case of array properties.
	 *
	 * @return					TRUE if the input proxy supports the property.
	 */
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const
	{
		check( 0 );
		return 0;
	}

	/**
	 * Allow ignoring of destruction and recreation of controls when there is no focus change.
	 */
	virtual UBOOL IgnoreDoubleSetFocus()
	{
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	// Event handlers

	/**
	 * Allows a customized response to the set focus event of the property item.
	 * @param	InItem			The property window node.
	 */
	virtual void OnSetFocus(WxPropertyControl* InItem) {}

	/**
	* Allows a customized response to the kill focus event of the property item.
	* @param	InItem			The property window node.
	*/
	virtual void OnKillFocus(WxPropertyControl* InItem) {}

	/**
	 * Allows a customized response to a user double click on the item in the property window.
	 *
	 * @param	InItem			The property window node.
	 * @param	InObjects		A collection of objects and addresses from which to export a value if bForceValue is FALSE.
	 * @param	InValue			If bForceValue is TRUE, specifies the new property value.
	 * @param	bForceValue		If TRUE, use the value specified with InValue.  Otherwise, export from the property at InReadAddress.
	 */
	virtual void DoubleClick(WxPropertyControl* InItem, const TArray<FObjectBaseAddress>& InObjects, const TCHAR* InValue, UBOOL bForceValue)
	{}

	/**
	 * Allows special actions on left mouse clicks.  Return TRUE to stop normal processing.
	 *
	 * @param	InItem			The property window node.
	 * @param	InX				The cursor X position associated with the click event.
	 * @param	InY				The cursor Y position associated with the click event.
	 */
	virtual UBOOL LeftClick( WxPropertyControl* InItem, INT InX, INT InY )
	{ return 0; }

	/**
	 * Allows special actions on left mouse clicks.  Return TRUE to stop normal processing.
	 *
	 * @param	InItem			The property window node.
	 * @param	InX				The cursor X position associated with the click event.
	 * @param	InY				The cursor Y position associated with the click event.
	 */
	virtual UBOOL LeftUnclick( WxPropertyControl* InItem, INT InX, INT InY )
	{ return 0; }

	/**
	 * Allows special actions on right mouse clicks.  Return TRUE to stop normal processing.
	 *
	 * @param	InItem			The property window node.
	 * @param	InX				The cursor X position associated with the click event.
	 * @param	InY				The cursor Y position associated with the click event.
	 */
	virtual UBOOL RightClick( WxPropertyControl* InItem, INT InX, INT InY )
	{ return 0; }

	/**
	 * Sends the current value in the input control to the selected objects.
	 *
	 * @param	InItem		The property window node containing the property which will receive the new value.
	 */
	virtual void SendToObjects( WxPropertyControl* InItem )
	{}

	/**
	 * Sends a text string to the selected objects.
	 *
	 * @param	InItem		The property window node containing the property which will receive the new value.
	 * @param	InText		The text value to send.
	 */
	void SendTextToObjects( WxPropertyControl* InItem, const FString& InText );

	/**
	 * Allows the created controls to react to the parent window being resized.  Baseclass
	 * behaviour is to resize any buttons on the control.  Derived classes that implement
	 * this method should to call up to their parent.
	 *
	 * @param	InItem			The parent node that was resized.
	 * @param	InRC			The window rect.
	 */
	virtual void ParentResized( WxPropertyControl* InItem, const wxRect& InRC );

	/**
	 * Handles button clicks from within the item.
	 *
	 * @param	InItem			The parent node that received the click.
	 * @param	InButton		The type of button that was clicked.
	 */
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton )
	{}

	/**
	 * Updates the control value with the property value.
	 *
	 * @param	InProperty		The property from which to export.
	 * @param	InData			The parent object read address.
	 */
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData )
	{}

	/**
	 * Sends a text value to the property item.  Also handles all notifications of this value changing.
	 */
	void ImportText(WxPropertyControl* InItem,
					UProperty* InProperty,
					const TCHAR* InBuffer,
					BYTE* InData,
					UObject* InParent);

	void ImportText(WxPropertyControl* InItem,
					const TArray<FObjectBaseAddress>& InObjects,
					const FString& InValue);

	void ImportText(WxPropertyControl* InItem,
					const TArray<FObjectBaseAddress>& InObjects,
					const TArray<FString>& InValues);

	/**
	* This notification occurs when the child controls are just about to be hidden or shown.
	*
	* @param	bShow	whether the child controls have just been shown.
	*/
	virtual void NotifyHideShowChildren(UBOOL bShow) {}

protected:
	/**
	 * Called when the "Use selected" property item button is clicked.
	 */
	void OnUseSelected(WxPropertyControl* InItem);

	/**
	 * Recurse up to the next object node, adding all array indices into a map according to their property name
	 * @param ArrayIndexMap - for the current object, what properties use which array offsets
	 * @param InNode - node to start adding array offsets for.  This function will move upward until it gets to an object node
	 */
	void GenerateArrayIndexMapToObjectNode(TMap<FString,INT>& OutArrayIndexMap, FPropertyNode* InNode);
};

/*-----------------------------------------------------------------------------
	UPropertyInputArray
-----------------------------------------------------------------------------*/

class UPropertyInputArray : public UPropertyInputProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputArray,UPropertyInputProxy,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton );

};

/*-----------------------------------------------------------------------------
	UPropertyInputArrayItemBase
-----------------------------------------------------------------------------*/

/**
 * Baseclass for input proxies for types that can exist in an array.
 */
class UPropertyInputArrayItemBase : public UPropertyInputProxy
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputArrayItemBase,UPropertyInputProxy,0,UnrealEd)

	/**
	 * Implements handling of buttons common to array items.
	 */
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton );

	/**
	 * Wrapper method for determining whether a class is valid for use by this property item input proxy.
	 *
	 * @param	InItem			the property window item that contains this proxy.
	 * @param	CheckClass		the class to verify
	 * @param	bAllowAbstract	TRUE if abstract classes are allowed
	 *
	 * @return	TRUE if CheckClass is valid to be used by this input proxy
	 */
	virtual UBOOL IsClassAllowed( WxPropertyControl* InItem,  UClass* CheckClass, UBOOL bAllowAbstract ) const;

};

/*-----------------------------------------------------------------------------
	UPropertyInputBool
-----------------------------------------------------------------------------*/

class UPropertyInputBool : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputBool,UPropertyInputArrayItemBase,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void DoubleClick(WxPropertyControl* InItem, const TArray<FObjectBaseAddress>& InObjects, const TCHAR* InValue, UBOOL bForceValue);
	virtual UBOOL LeftClick( WxPropertyControl* InItem, INT InX, INT InY );

};

/*-----------------------------------------------------------------------------
	UPropertyInputColor
-----------------------------------------------------------------------------*/

class UPropertyInputColor : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputColor,UPropertyInputArrayItemBase,0,UnrealEd)

	/**
	 * Generates a text string based from the color passed in, based on the color type of the property passed in.
	 *
	 * @param InItem	Property that we are basing our string type off of.
	 * @param InColor	Color to use to generate the string.
	 * @param OutString Output for generated string.
	 */
	static void GenerateColorString( const WxPropertyControl* InItem, const FColor &InColor, FString& OutString );

	/**
	 * This should bring up the color picker
	 *
	 * @param	InItem			The property window node.
	 * @param	InX				The cursor X position associated with the click event.
	 * @param	InY				The cursor Y position associated with the click event.
	 */
	virtual UBOOL LeftUnclick( WxPropertyControl* InItem, INT InX, INT InY );

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
};

/*-----------------------------------------------------------------------------
	UPropertyInputArrayItem
-----------------------------------------------------------------------------*/

class UPropertyInputArrayItem : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputArrayItem,UPropertyInputArrayItemBase,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputText
-----------------------------------------------------------------------------*/

class UPropertyInputText : public UPropertyInputArrayItemBase
{
	/**
	 * This class wraps WxTextCtrl and overrides the erase background event to prevent flickering.
	 */
	class WxPropertyItemTextCtrl : public WxTextCtrl
	{
	public:
		DECLARE_DYNAMIC_CLASS(WxPropertyItemTextCtrl);

		/**
		 * Constructor.
		 */
		WxPropertyItemTextCtrl();
		/**
		 * Constructor.
		 */
		WxPropertyItemTextCtrl( wxWindow* parent,
			wxWindowID id,
			const wxString& value = TEXT(""),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = 0 );

		/**
		 * Event handler for the focus event. This function maintains proper selection state within the parent property window.
		 *
		 * @param	Event	Information about the event.
		 */
		virtual void OnChangeFocus(wxFocusEvent &Event);

		/**
		 * Event handler for the kill focus event. This function brings the cursor back to index 0 within the text control.
		 *
		 * @param	Event	Information about the event.
		 */
		virtual void OnKillFocus(wxFocusEvent& Event);

		/**
		 * Event handler for the left mouse button down event. This handler is needed because when the text control is clicked and it doesn't have focus it cannot
		 * select all of the text in the control due to the fact that the control thinks it is in the middle of a drag/selection operation. This only applies when
		 * all property item buttons are being rendered because that's the only time when all text control's are created and visible.
		 */
		virtual void OnLeftMouseDown(wxMouseEvent &Event);

		/**
		 * Event handler for the select all text event.
		 *
		 * @param	Event	Information about the event.
		 */
		virtual void OnSelectAllText(wxCommandEvent &Event);

		/**
		 * This event handler sets the default cursor when the mouse cursor enters the text box. This prevents the resize cursor from being permanent until you move the mouse outside of the text box.
		 *
		 * @param	Event	Information about the event.
		 */
		virtual void OnMouseEnter(wxMouseEvent &Event);

		/**
		 * Handles key down events.
		 *
		 * @param	Event	Information about the event.
		 */
		virtual void OnChar( wxKeyEvent& Event );

		DECLARE_EVENT_TABLE();
	};

	DECLARE_CLASS_INTRINSIC(UPropertyInputText,UPropertyInputArrayItemBase,0,UnrealEd)

	UPropertyInputText()
		:	TextCtrl( NULL ),
			PropertyPanel( NULL )
	{}

	/**
	 * Clears any selection in the text control.
	 */
	void ClearTextSelection();

	WxPropertyItemTextCtrl* TextCtrl;

protected:
	wxPanel* PropertyPanel;
	wxSizer* PropertySizer;

	INT MaxRowCount;
	

public:
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual void SendToObjects( WxPropertyControl* InItem );
	virtual void ParentResized( WxPropertyControl* InItem, const wxRect& InRC );
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton );
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData );

	/**
	* Allows a customized response to the set focus event of the property item.
	* @param	InItem			The property window node.
	*/
	virtual void OnSetFocus(WxPropertyControl* InItem);

	/**
	* Allows a customized response to the kill focus event of the property item.
	* @param	InItem			The property window node.
	*/
	virtual void OnKillFocus(WxPropertyControl* InItem);

	/**
	 * @return	Returns the height required by the input proxy.
	 */
	virtual INT GetProxyHeight() const;

};


/*-----------------------------------------------------------------------------
	UPropertyInputNumeric
-----------------------------------------------------------------------------*/

class UPropertyInputNumeric : public UPropertyInputText
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputNumeric,UPropertyInputText,0,UnrealEd)

	FString Equation;

	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void SendToObjects( WxPropertyControl* InItem );
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData );

	/**
	 * @return Returns the numeric value of the property.
	 */
	virtual FLOAT GetValue();

	/**
	 * Gets the values for each of the objects that a property item is affecting.
	 * @param	InItem		Property item to get objects from.
	 * @param	Values		Array of FLOATs to store values.
	 */
	virtual void GetValues(WxPropertyControl* InItem, TArray<FLOAT> &Values);

	/**
	 * Updates the numeric value of the property.
	 * @param	Value	New Value for the property.
	 */
	virtual void SetValue(FLOAT NewValue, WxPropertyControl* InItem);

	/**
	 * Sends the new values to all objects.
	 * @param NewValues		New values for the property.
	 */
	virtual void SetValues(const TArray<FLOAT>& NewValues, WxPropertyControl* InItem);

protected:
	/** Spinner for numeric properties. */
	WxNumericPropertySpinner* SpinButton;
};

/*-----------------------------------------------------------------------------
	UPropertyInputInteger
-----------------------------------------------------------------------------*/

class UPropertyInputInteger : public UPropertyInputNumeric
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputInteger,UPropertyInputNumeric,0,UnrealEd)

	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void SendToObjects( WxPropertyControl* InItem );

	/**
	* Updates the numeric value of the property.
	* @param	Value	New Value for the property.
	*/
	virtual void SetValue(FLOAT NewValue, WxPropertyControl* InItem);

	/**
	 * Sends the new values to all objects.
	 * @param NewValues		New values for the property.
	 */
	virtual void SetValues(const TArray<FLOAT>& NewValues, WxPropertyControl* InItem);

private:
	/**
	 * Clamps a passed in value to the bounds of the metadata of the object.  
	 * Supports "ClampMin" (Numeric), "ClampMax" (Numeric), and "ArrayClamp".  Note, ArrayClamp must reference an array with the same outer so it can be found
	 * @param InItem - The property window that we are trying to clamp the value of
	 * @param InValue - The requested value to set this property to prior to clamping
	 * @return The clamped value to assign directly into the property by ImportText
	 */
	INT ClampValueFromMetaData(WxPropertyControl* InItem, INT InValue);
	
	/**
	 * Gets the max valid index for a array property of an object
	 * @param InObjectNode - The parent of the variable being clamped
	 * @param InArrayName - The array name we're hoping to clamp to the extents of
	 * @return LastValidEntry in the array (if the array is found)
	 */
	INT GetArrayPropertyLastValidIndex(FObjectPropertyNode* InObjectNode, const FString& InArrayName);
};


/*-----------------------------------------------------------------------------
	UPropertyInputRangedNumeric
-----------------------------------------------------------------------------*/

class UPropertyInputRangedNumeric : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputRangedNumeric,UPropertyInputArrayItemBase,0,UnrealEd)

	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void SendToObjects( WxPropertyControl* InItem );

	/**
	 * @return Returns the numeric value of the property.
	 */
	virtual FLOAT GetValue();

	/**
	 * Gets the values for each of the objects that a property item is affecting.
	 * @param	InItem		Property item to get objects from.
	 * @param	Values		Array of FLOATs to store values.
	 */
	virtual void GetValues(WxPropertyControl* InItem, TArray<FLOAT> &Values);

	/**
	 * Updates the numeric value of the property.
	 * @param	Value	New Value for the property.
	 */
	virtual void SetValue(const FString& NewValue, WxPropertyControl* InItem);

	/**
	 * Custom resizing to keep slider size in sync with parent
	 */
	virtual void ParentResized( WxPropertyControl* InItem, const wxRect& InRC );

	/**
	 * Allows a customized response to the set focus event of the property item.
	 * @param	InItem			The property window node.
	 */
	virtual void OnSetFocus(WxPropertyControl* InItem);

	/**
	* Allows a customized response to the kill focus event of the property item.
	* @param	InItem			The property window node.
	*/
	virtual void OnKillFocus(WxPropertyControl* InItem);

	/**
	 * Updates the control value with the property value.
	 *
	 * @param	InProperty		The property from which to export.
	 * @param	InData			The parent object read address.
	 */
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData );

protected:
	/** Ranged Slider for numeric properties. */
	WxRangedSlider* Slider;

};

/*-----------------------------------------------------------------------------
	UPropertyInputRotation
-----------------------------------------------------------------------------*/

class UPropertyInputRotation : public UPropertyInputNumeric
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputRotation,UPropertyInputNumeric,0,UnrealEd)

	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void SendToObjects( WxPropertyControl* InItem );

	/**
	 * @return Returns the numeric value of the property.
	 */
	virtual FLOAT GetValue();

	/**
	 * Gets the values for each of the objects that a property item is affecting.
	 * @param	InItem		Property item to get objects from.
	 * @param	Values		Array of FLOATs to store values.
	 */
	virtual void GetValues(WxPropertyControl* InItem, TArray<FLOAT> &Values);


	/**
	 * Updates the numeric value of the property.
	 * @param	Value	New Value for the property.
	 */
	virtual void SetValue(FLOAT NewValue, WxPropertyControl* InItem);

	/**
	 * Sends the new values to all objects.
	 * @param NewValues		New values for the property.
	 */
	virtual void SetValues(const TArray<FLOAT>& NewValues, WxPropertyControl* InItem);

};

/*-----------------------------------------------------------------------------
	UPropertyInputCombo
-----------------------------------------------------------------------------*/

class UPropertyInputCombo : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputCombo,UPropertyInputArrayItemBase,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual void SendToObjects( WxPropertyControl* InItem );
	virtual void ParentResized( WxPropertyControl* InItem, const wxRect& InRC );
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData );

private:
	/**
	 * Shared function to get the Rect that we want to set this combo to from the parent window size
	 * @param InRect - Rect of the parent control
	 * @return Desired combo rect
	 */
	wxRect GetComboRect(const wxRect& InRect);

	/**
	 * Fills the combobox with the contents of the enumeration
	 * @param InComboBox - the combo box to fill in with strings
	 * @param InEnum - the enumeration to fill the combo box with
	 */
	FString FillComboFromEnum (wxComboBox* InComboBox, UEnum* InEnum, const FString& InDefaultValue);

	/** Fills the combobox with the contents of dynamic list provided by owner object */
	FString FillComboFromDynamicList(WxPropertyControl* InItem, const FString& InDefaultValue);

	/**
	* Allows a customized response to the set focus event of the property item.
	* @param	InItem			The property window node.
	*/
	virtual void OnSetFocus(WxPropertyControl* InItem);

	/**
	 * Indicates that this combo box's values are friendly names for the real values; currently only used for enum drop-downs.
	 */
	UBOOL bUsesAlternateDisplayValues;

	/**
	 * Indicates that this combo box contains the "Multiple values" string
	 */
	UBOOL bContainsMultipleValueString;
};
/*-----------------------------------------------------------------------------
* UPropertyInput_DynamicCombo - editable combobox that could be filled on the fly
* for now it is used to select groups names for Group in Material Eeditor
-----------------------------------------------------------------------------*/
class UPropertyInput_DynamicCombo : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInput_DynamicCombo,UPropertyInputArrayItemBase,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual void SendToObjects( WxPropertyControl* InItem );
	virtual void ParentResized( WxPropertyControl* InItem, const wxRect& InRC );
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData );
private:
	/**
	* Shared function to get the Rect that we want to set this combo to from the parent window size
	* @param InRect - Rect of the parent control
	* @return Desired combo rect
	*/
	wxRect GetComboRect(const wxRect& InRect);

	/**
	* Fills the combobox with the contents of material editor group names
	* @param InComboBox - the combo box to fill in with strings
	* @param InItem - node to get retrieve all information
	* @param InDefaultValue - legacy default value
	*/
	FString FillComboFromMaterialEditor (wxComboBox* InComboBox, WxPropertyControl* InItem, const FString& InDefaultValue);


	/**
	* Indicates that this combo box's values are friendly names for the real values; currently only used for enum drop-downs.
	*/
	UBOOL bUsesAlternateDisplayValues;
};
/*-----------------------------------------------------------------------------
	UPropertyInputEditInline
-----------------------------------------------------------------------------*/

class UPropertyInputEditInline : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputEditInline,UPropertyInputArrayItemBase,0,UnrealEd)

	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton );

	/**
	 * Wrapper method for determining whether a class is valid for use by this property item input proxy.
	 *
	 * @param	InItem			the property window item that contains this proxy.
	 * @param	CheckClass		the class to verify
	 * @param	bAllowAbstract	TRUE if abstract classes are allowed
	 *
	 * @return	TRUE if CheckClass is valid to be used by this input proxy
	 */
	virtual UBOOL IsClassAllowed( WxPropertyControl* InItem,  UClass* CheckClass, UBOOL bAllowAbstract ) const;

};


/*-----------------------------------------------------------------------------
UPropertyInputTextEditBox
-----------------------------------------------------------------------------*/

class UPropertyInputTextEditBox : public UPropertyInputArrayItemBase
{
	DECLARE_CLASS_INTRINSIC(UPropertyInputTextEditBox,UPropertyInputArrayItemBase,0,UnrealEd)
	
	UPropertyInputTextEditBox()
	:	DlgTextEditBox( NULL )
	,	TextButton(NULL)
	{}

	FString InitialValue;
	class WxDlgPropertyTextEditBox* DlgTextEditBox;
	wxButton* TextButton;

public:
	virtual UBOOL Supports( const FPropertyNode* InTreeNode, INT InArrayIdx ) const;
	virtual void ButtonClick( WxPropertyControl* InItem, ePropButton InButton );
	virtual void CreateControls( WxPropertyControl* InItem, UClass* InBaseClass, const wxRect& InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual void ParentResized( WxPropertyControl* InItem, const wxRect& InRC );
	virtual void SendToObjects( WxPropertyControl* InItem );
	virtual void RefreshControlValue( UProperty* InProperty, BYTE* InData );
	virtual void NotifyHideShowChildren(UBOOL bShow);

	/**
	* Veto destruction and recreation of controls when there is no focus change.
	*/
	virtual UBOOL IgnoreDoubleSetFocus()
	{
		return TRUE;
	}

};


#endif // __PROPERTYUTILS_H__
