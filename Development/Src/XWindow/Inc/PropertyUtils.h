/*=============================================================================
	PropertyUtils.h: Classes that support the property window

	Revision history:
		* Created by Warren Marshall

	Make sure to update WxPropertyWindow_Base when adding new input and/ or 
	draw proxies.
=============================================================================*/


#define PB_ButtonSz		15

enum ePropButton
{
	PB_Add,
	PB_Empty,
	PB_Insert,
	PB_Delete,
	PB_Browse,
	PB_Pick,
	PB_Clear,
	PB_Find,
	PB_Use,
	PB_NewObject,
};

class UPropertyInputProxy;

/*-----------------------------------------------------------------------------
	UPropertyDrawProxy

	Allows customized drawing for properties
-----------------------------------------------------------------------------*/

class UPropertyDrawProxy : public UObject
{
	DECLARE_CLASS(UPropertyDrawProxy,UObject,0,UnrealEd)

	UPropertyDrawProxy() {}

	// Determines if this input proxy supports the specified UProperty
	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx ) { return 0; }

	// Draws the value of InProperty inside of InRect
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );

	// Draws a representation of an unknown value (i.e. when multiple objects have different values in the same field)
	virtual void DrawUnknown( wxDC* InDC, wxRect InRect );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawNumeric
-----------------------------------------------------------------------------*/

class UPropertyDrawNumeric : public UPropertyDrawProxy
{
	DECLARE_CLASS(UPropertyDrawNumeric,UPropertyDrawProxy,0,UnrealEd)

	UPropertyDrawNumeric() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawColor
-----------------------------------------------------------------------------*/

class UPropertyDrawColor : public UPropertyDrawProxy
{
	DECLARE_CLASS(UPropertyDrawColor,UPropertyDrawProxy,0,UnrealEd)

	UPropertyDrawColor() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawRotation
-----------------------------------------------------------------------------*/

class UPropertyDrawRotation : public UPropertyDrawProxy
{
	DECLARE_CLASS(UPropertyDrawRotation,UPropertyDrawProxy,0,UnrealEd)

	UPropertyDrawRotation() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawRotationHeader
-----------------------------------------------------------------------------*/

class UPropertyDrawRotationHeader : public UPropertyDrawProxy
{
	DECLARE_CLASS(UPropertyDrawRotationHeader,UPropertyDrawProxy,0,UnrealEd)

	UPropertyDrawRotationHeader() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawBool
-----------------------------------------------------------------------------*/

class UPropertyDrawBool : public UPropertyDrawProxy
{
	DECLARE_CLASS(UPropertyDrawBool,UPropertyDrawProxy,0,UnrealEd)

	UPropertyDrawBool() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );
	virtual void DrawUnknown( wxDC* InDC, wxRect InRect );
};

/*-----------------------------------------------------------------------------
	UPropertyDrawArrayHeader
-----------------------------------------------------------------------------*/

class UPropertyDrawArrayHeader : public UPropertyDrawProxy
{
	DECLARE_CLASS(UPropertyDrawArrayHeader,UPropertyDrawProxy,0,UnrealEd)

	UPropertyDrawArrayHeader() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy );
};

/*-----------------------------------------------------------------------------
	UPropertyInputProxy

	Handles user input for a property value
-----------------------------------------------------------------------------*/

class UPropertyInputProxy : public UObject
{
	DECLARE_CLASS(UPropertyInputProxy,UObject,0,UnrealEd)

	UPropertyInputProxy()
	{
		TextCtrl = NULL;
		ComboBox = NULL;
	}

	TArray<wxBitmapButton*> Buttons;		// Any little buttons that appear on the right side of a property window item
	WxTextCtrl* TextCtrl;
	wxComboBox* ComboBox;

	// Adds a button to the right side of the item window
	void AddButton( WxPropertyWindow_Base* InItem, ePropButton InButton, wxRect InRC );

	// Sends a text value to the property item.  Also handles all notifications of this value changing.
	void ImportText( WxPropertyWindow* InPropWindow, UProperty* InProperty, const TCHAR* InBuffer, BYTE* InData, UObject* InParent );

	// Determines if this input proxy supports the specified UProperty
	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx ) { return 0; }

	// Allows a customized response to a user double click on the item in the property window
	virtual void DoubleClick( WxPropertyWindow_Base* InItem, UObject* InObject, BYTE* InReadAddress, const TCHAR* InValue, UBOOL bForceValue ) {}

	// Allows special actions on left mouse clicks.  Return TRUE to stop normal processing.
	virtual UBOOL LeftClick( WxPropertyWindow_Base* InItem, INT InX, INT InY ) { return 0; }

	// Creates any controls that are needed to edit the property
	virtual void CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue )
	{
		CreateButtons( InItem, InRC );
	}

	// Creates any buttons that the property needs
	virtual void CreateButtons( WxPropertyWindow_Base* InItem, wxRect InRC );

	// Deletes any controls which were created for editing
	virtual void DeleteControls()
	{
		DeleteButtons();
	}

	// Deletes any buttons that were created
	virtual void DeleteButtons();

	// Sends the current value in the input control to the selected objects
	virtual void SendToObjects( WxPropertyWindow_Base* InItem ) {}

	// Sends a text string to the selected objects
	virtual void SendTextToObjects( WxPropertyWindow_Base* InItem, FString InText );

	// Allows the created controls to react to the parent window being resized
	virtual void ParentResized( WxPropertyWindow_Base* InItem, wxRect InRC );

	// Handles button clicks from within the item
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputBool
-----------------------------------------------------------------------------*/

class UPropertyInputBool : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputBool,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputBool() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void DoubleClick( WxPropertyWindow_Base* InItem, UObject* InObject, BYTE* InReadAddress, const TCHAR* InValue, UBOOL bForceValue );
	virtual UBOOL LeftClick( WxPropertyWindow_Base* InItem, INT InX, INT InY );
};

/*-----------------------------------------------------------------------------
	UPropertyInputColor
-----------------------------------------------------------------------------*/

class UPropertyInputColor : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputColor,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputColor() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputArray
-----------------------------------------------------------------------------*/

class UPropertyInputArray : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputArray,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputArray() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputArrayItem
-----------------------------------------------------------------------------*/

class UPropertyInputArrayItem : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputArrayItem,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputArrayItem() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputText
-----------------------------------------------------------------------------*/

class UPropertyInputText : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputText,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputText() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual void SendToObjects( WxPropertyWindow_Base* InItem );
	virtual void ParentResized( WxPropertyWindow_Base* InItem, wxRect InRC );
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputRotation
-----------------------------------------------------------------------------*/

class UPropertyInputRotation : public UPropertyInputText
{
	DECLARE_CLASS(UPropertyInputRotation,UPropertyInputText,0,UnrealEd)

	UPropertyInputRotation() {}

	virtual void CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue );
	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void SendToObjects( WxPropertyWindow_Base* InItem );
};

/*-----------------------------------------------------------------------------
	UPropertyInputNumeric
-----------------------------------------------------------------------------*/

class UPropertyInputNumeric : public UPropertyInputText
{
	DECLARE_CLASS(UPropertyInputNumeric,UPropertyInputText,0,UnrealEd)

	UPropertyInputNumeric() {}

	FString Equation;

	virtual void CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue );
	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void SendToObjects( WxPropertyWindow_Base* InItem );

	UBOOL Eval( FString Str, FLOAT* pResult );
	UBOOL SubEval( FString* pStr, FLOAT* pResult, INT Prec );
	FString GrabChar( FString* pStr );
	FLOAT Val( FString Value );
};

/*-----------------------------------------------------------------------------
	UPropertyInputCombo
-----------------------------------------------------------------------------*/

class UPropertyInputCombo : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputCombo,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputCombo() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue );
	virtual void DeleteControls();
	virtual void SendToObjects( WxPropertyWindow_Base* InItem );
	virtual void ParentResized( WxPropertyWindow_Base* InItem, wxRect InRC );
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	UPropertyInputEditInline
-----------------------------------------------------------------------------*/

class UPropertyInputEditInline : public UPropertyInputProxy
{
	DECLARE_CLASS(UPropertyInputEditInline,UPropertyInputProxy,0,UnrealEd)

	UPropertyInputEditInline() {}

	virtual UBOOL Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx );
	virtual void ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
