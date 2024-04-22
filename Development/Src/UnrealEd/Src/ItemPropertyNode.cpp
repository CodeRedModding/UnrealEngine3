/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnrealEdPrivateClasses.h"
#include "PropertyNode.h"
#include "ObjectsPropertyNode.h"
#include "ItemPropertyNode.h"
#include "PropertyWindow.h"
#include "PropertyUtils.h"
#include "PropertyWindowManager.h"
#include "ScopedTransaction.h"
#include "ScopedPropertyChange.h"	// required for access to FScopedPropertyChange
#include "MaterialInstanceConstantEditor.h"
#include "MaterialInstanceTimeVaryingEditor.h"


/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectIterator		TPropObjectIterator;

/** Internal convenience type. */
typedef FObjectPropertyNode::TObjectConstIterator	TPropObjectConstIterator;

//-----------------------------------------------------------------------------
//
// FItemPropertyNode
//
//-----------------------------------------------------------------------------
FItemPropertyNode::FItemPropertyNode(void)
: FPropertyNode()
{
}
FItemPropertyNode::~FItemPropertyNode(void)
{
}

/**
 * Calculates the memory address for the data associated with this item's property.  This is typically the value of a UProperty or a UObject address.
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to an FArray*)
 */
BYTE* FItemPropertyNode::GetValueBaseAddress( BYTE* StartAddress )
{
	UArrayProperty* OuterArrayProp = Cast<UArrayProperty>(Property->GetOuter());
	if ( OuterArrayProp != NULL )
	{
		FScriptArray* Array = (FScriptArray*)ParentNode->GetValueBaseAddress(StartAddress);
		if ( Array != NULL && ArrayIndex < Array->Num() )
		{
			return ((BYTE*)Array->GetData()) + PropertyOffset;
		}

		return NULL;
	}
	else
	{
		BYTE* ValueAddress = ParentNode->GetValueAddress(StartAddress);
		if ( ValueAddress != NULL )
		{
			ValueAddress += PropertyOffset;
		}
		return ValueAddress;
	}
}

/**
 * Calculates the memory address for the data associated with this item's value.  For most properties, identical to GetValueBaseAddress.  For items corresponding
 * to dynamic array elements, the pointer returned will be the location for that element's data. 
 *
 * @param	StartAddress	the location to use as the starting point for the calculation; typically the address of the object that contains this property.
 *
 * @return	a pointer to a UProperty value or UObject.  (For dynamic arrays, you'd cast this value to whatever type is the Inner for the dynamic array)
 */
BYTE* FItemPropertyNode::GetValueAddress( BYTE* StartAddress )
{
	BYTE* Result = GetValueBaseAddress(StartAddress);

	UArrayProperty* ArrayProperty;
	if( Result != NULL && (ArrayProperty=Cast<UArrayProperty>(Property))!=NULL )
	{
		Result = (BYTE*)((FScriptArray*)Result)->GetData();
	}

	return Result;
}

/**
 * Overriden function for special setup
 */
void FItemPropertyNode::InitExpansionFlags (void)
{
	check(NodeWindow);
	TArray<BYTE*> Addresses;
	if(	Cast<UStructProperty>(Property,CLASS_IsAUStructProperty)
		||	( Cast<UArrayProperty>(Property) && GetReadAddress(this,FALSE,Addresses) )
		||  HasNodeFlags(EPropertyNodeFlags::EditInline)
		||	( Property->ArrayDim > 1 && ArrayIndex == -1 ) )
	{
		SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, TRUE);
	}

	// set flag
	if ( Property && IsEditConst() )
	{
		// Set Node Flag
		SetNodeFlags(EPropertyNodeFlags::IsRealTime, Property->GetBoolMetaData(TEXT("RealTime")));
	}
}
/**
 * Overriden function for Creating Child Nodes
 */
void FItemPropertyNode::InitChildNodes(void)
{
	//NOTE - this is only turned off as to not invalidate child object nodes.
	UProperty* Property = GetProperty();
	UStructProperty* StructProperty = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty);
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
	UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property,CLASS_IsAUObjectProperty);

	if( Property->ArrayDim > 1 && ArrayIndex == -1 )
	{
		// Expand array.
		SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);
		for( INT i = 0 ; i < Property->ArrayDim ; i++ )
		{
			FItemPropertyNode* NewItemNode = new FItemPropertyNode;//;//CreatePropertyItem(Property,i,this);
			WxItemPropertyControl* pwi = CreatePropertyItem(Property, i);
			NewItemNode->InitNode(pwi, this, TopPropertyWindow, Property, i*Property->ElementSize, i);
			ChildNodes.AddItem(NewItemNode);
		}
	}
	else if( ArrayProperty )
	{
		// Expand array.
		SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);

		FScriptArray* Array = NULL;
		TArray<BYTE*> Addresses;
		if ( GetReadAddress( this, HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses ) )
		{
			Array = (FScriptArray*)Addresses(0);
		}

		if( Array )
		{
			for( INT i = 0 ; i < Array->Num() ; i++ )
			{
				FItemPropertyNode* NewItemNode = new FItemPropertyNode;//;//CreatePropertyItem(ArrayProperty,i,this);
				WxItemPropertyControl* pwi = CreatePropertyItem(ArrayProperty, i);
				NewItemNode->InitNode(pwi, this, TopPropertyWindow, ArrayProperty->Inner, i*ArrayProperty->Inner->ElementSize, i);
				ChildNodes.AddItem(NewItemNode);
			}
		}
	}
	else if( StructProperty )
	{
		//force colors to not alphabetize
		if (StructProperty->Struct->GetFName()==NAME_Color || StructProperty->Struct->GetFName()==NAME_LinearColor)
		{
			SetNodeFlags(EPropertyNodeFlags::SortChildren, FALSE);
		}
		//force rotations to not alphabetize
		if (StructProperty->Struct->GetFName()==NAME_Rotator)
		{
			InitRotatorStructure(StructProperty);
		}

		const UBOOL bShouldShowNonEditable = TopPropertyWindow->HasFlags(EPropertyWindowFlags::ShouldShowNonEditable);
		// Expand struct.
		for( TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It )
		{
			UProperty* StructMember = *It;
			if( bShouldShowNonEditable || (StructMember->PropertyFlags&CPF_Edit) )
			{
				FItemPropertyNode* NewItemNode = new FItemPropertyNode;//;//CreatePropertyItem(StructMember,INDEX_NONE,this);
				WxItemPropertyControl* pwi = CreatePropertyItem(StructMember, INDEX_NONE);
				NewItemNode->InitNode(pwi, this, TopPropertyWindow, StructMember, StructMember->Offset, INDEX_NONE);
				ChildNodes.AddItem(NewItemNode);

				if (GPropertyWindowManager->GetExpandDistributions() == FALSE)
				{
					// auto-expand distribution structs
					if ( Cast<UComponentProperty>(StructMember) != NULL )
					{
						const FName StructName = StructProperty->Struct->GetFName();
						if (StructName == NAME_RawDistributionFloat || StructName == NAME_RawDistributionVector)
						{
							NewItemNode->SetNodeFlags(EPropertyNodeFlags::Expanded, TRUE);
						}
					}
				}
			}
		}
	}
	else if( ObjectProperty || Property->IsA(UInterfaceProperty::StaticClass()))
	{
		BYTE* ReadValue = NULL;

		TArray<BYTE*> Addresses;
		if( GetReadAddress( this, HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses, FALSE ) )
		{
			// We've got some addresses, and we know they're all NULL or non-NULL.
			// Have a peek at the first one, and only build an objects node if we've got addresses.
			UObject* obj = *(UObject**)Addresses(0);
			if( obj )
			{
				//verify it's not above in the hierarchy somewhere
				FObjectPropertyNode* ParentObjectNode = FindObjectItemParent();
				while (ParentObjectNode)
				{
					for ( TPropObjectIterator Itor( ParentObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
					{
						if (*Itor == obj)
						{
							SetNodeFlags(EPropertyNodeFlags::NoChildrenDueToCircularReference, TRUE);
							//stop the circular loop!!!
							return;
						}
					}
					FPropertyNode* UpwardTravesalNode = ParentObjectNode->GetParentNode();
					ParentObjectNode = (UpwardTravesalNode==NULL) ? NULL : UpwardTravesalNode->FindObjectItemParent();
				}

				WxObjectsPropertyControl* pwo = new WxObjectsPropertyControl;
				FObjectPropertyNode* NewObjectNode = new FObjectPropertyNode;
				for ( INT i = 0 ; i < Addresses.Num() ; ++i )
				{
					NewObjectNode->AddObject( *(UObject**) Addresses(i) );
				}
				NewObjectNode->InitNode(pwo, this, TopPropertyWindow, Property, PROP_OFFSET_None, INDEX_NONE);
				ChildNodes.AddItem(NewObjectNode);
			}
		}
	}

	//needs to be after all the children are created
	if (GPropertyWindowManager->GetExpandDistributions() == TRUE)
	{
		// auto-expand distribution structs
		if (Property->IsA(UStructProperty::StaticClass()))
		{
			FName StructName = ((UStructProperty*)Property)->Struct->GetFName();
			if (StructName == NAME_RawDistributionFloat || StructName == NAME_RawDistributionVector)
			{
				const UBOOL bExpand = TRUE;
				const UBOOL bRecurse = TRUE;
				SetExpanded(bExpand, bRecurse);
			}
		}
	}
}


/**
 * Overriden function for Creating the corresponding wxPropertyWindow_Base
 */
void FItemPropertyNode::CreateInternalWindow(void)
{
	NodeWindow->Create(GetParentNodeWindow());
}
/**
 * Function to allow different nodes to add themselves to focus or not
 */
void FItemPropertyNode::AddSelfToVisibleArray(OUT TArray<WxPropertyControl*>& OutVisibleArray)
{
	OutVisibleArray.AddItem(GetNodeWindow());
}

/**
 * Helper function to fix up rotator child properties
 * @param InStructProperty - the rotator property to be adjusted
 */
void FItemPropertyNode::InitRotatorStructure(UStructProperty* InStructProperty)
{
	//must be a valid property
	check(InStructProperty);

	//force rotator properties to x-axis, y-axis, z-axis ordering (roll, pitch, yaw)
	// Expand struct.
	for( TFieldIterator<UProperty> It(InStructProperty->Struct); It; ++It )
	{
		UProperty* StructMember = *It;
		if (StructMember->GetName() == TEXT("Roll"))
		{
			StructMember->SetMetaData(TEXT("OrderIndex"), TEXT("0"));
		}
		else if (StructMember->GetName() == TEXT("Pitch"))
		{
			StructMember->SetMetaData(TEXT("OrderIndex"), TEXT("1"));
		} 
		else if (StructMember->GetName() == TEXT("Yaw"))
		{
			StructMember->SetMetaData(TEXT("OrderIndex"), TEXT("2"));
		}
	}

	SetNodeFlags(EPropertyNodeFlags::SortChildren | EPropertyNodeFlags::IsForceScriptOrder, TRUE);
}


/*-----------------------------------------------------------------------------
	WxItemPropertyControl
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS( WxItemPropertyControl, WxPropertyControl )

BEGIN_EVENT_TABLE( WxItemPropertyControl, WxPropertyControl )
	EVT_PAINT( WxItemPropertyControl::OnPaint )
	EVT_ERASE_BACKGROUND( WxItemPropertyControl::OnEraseBackground )
	EVT_TEXT_ENTER( ID_PROPWINDOW_EDIT, WxItemPropertyControl::OnInputTextEnter )
	EVT_TEXT( ID_PROPWINDOW_EDIT, WxItemPropertyControl::OnInputTextChanged )
	EVT_COMBOBOX( ID_PROPWINDOW_COMBOBOX, WxItemPropertyControl::OnInputComboBox )
	EVT_TEXT_ENTER( ID_PROPWINDOW_COMBOBOX, WxItemPropertyControl::OnInputComboBox )
END_EVENT_TABLE()

/**
 * Virtual function to allow generation of display name
 */
void WxItemPropertyControl::InitBeforeCreate(void)
{
	// Clean up the property name
	{
		//only create the display name if it hasn't been created before.
		if (DisplayName.Len() == 0)
		{
			UProperty* Property = GetProperty();
			UBOOL bIsBoolProperty = ConstCast<UBoolProperty>(Property) != NULL;
			
			FString PropertyDisplayName = Property->GetName();
			SanitizePropertyDisplayName( PropertyDisplayName, bIsBoolProperty );

			DisplayName = *PropertyDisplayName;
		}
	}
}

/**
 * Initialize this property window.  Must be the first function called after creating the window.
 */
void WxItemPropertyControl::Create(wxWindow* InParent)
{
	WxPropertyControl::Create( InParent);

	FinishSetup();

	// Let the input/draw proxy override whether or not the property can expand.
	if(HasNodeFlags(EPropertyNodeFlags::CanBeExpanded) && InputProxy != NULL && DrawProxy != NULL)
	{
		if(InputProxy->LetPropertyExpand() == FALSE || DrawProxy->LetPropertyExpand() == FALSE )
		{
			PropertyNode->SetNodeFlags(EPropertyNodeFlags::CanBeExpanded, FALSE);
		}
	}


}

/**
 * Displays a context menu for this property window node.
 *
 * @param	In	the mouse event that caused the context menu to be requested
 *
 * @return	TRUE if the context menu was shown.
 */
UBOOL WxItemPropertyControl::ShowPropertyItemContextMenu( wxMouseEvent& In )
{
	check(PropertyNode);
	UProperty* Property = GetProperty();

	UBOOL bResult = FALSE;
	
	//if (Property->HasAnyPropertyFlags(CPF_EditConst | CPF_EditFixedSize))
	//{
	//	return bResult;//no other options in the context menu, just bail.
	//}

	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	WxPropertyWindowHost* HostWindow = MainWindow->GetParentHostWindow();

	UBOOL bAnyDifferentThanDefault = FALSE;
	UBOOL bAnyPasteable = FALSE;

	INT NumFocusWindows = HostWindow->NumActiveFocusWindows();
	for (int FocusWindowIndex = 0; FocusWindowIndex < NumFocusWindows; ++FocusWindowIndex)
	{
		FPropertyNode* FocusNode = HostWindow->GetFocusNode(FocusWindowIndex);
		if (FocusNode)
		{
			FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();
			for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
			{
				if ( !MainWindow->HasFlags(EPropertyWindowFlags::ReadOnly) && Property != NULL && !Property->HasAnyPropertyFlags(CPF_EditConst) )
				{
					bAnyPasteable = TRUE;

					FPropertyItemValueDataTracker ValueTracker(this, *Itor);
					if ( ValueTracker.HasDefaultValue() && DiffersFromDefault(*Itor, Property) )
					{
						bAnyDifferentThanDefault = TRUE;
					}
				}
			}
		}
	}

	const INT InX = In.GetX();
	if ( InX >= 0 && InX < MainWindow->GetSplitterPos() )
	{
		wxMenu ContextMenu;
		if ( bAnyDifferentThanDefault )
		{
			ContextMenu.Append(ID_PROP_RESET_TO_DEFAULT, *LocalizeUnrealEd(TEXT("PropertyWindow_ResetToDefault")));
		}
		if (NumFocusWindows > 0)
		{
			ContextMenu.Append(ID_UI_PROPERTYWINDOW_COPY, *LocalizeUnrealEd(TEXT("PropertyWindow_CopySelectedProperties")));
		}
		if (bAnyPasteable)
		{
			ContextMenu.Append(ID_UI_PROPERTYWINDOW_PASTE, *LocalizeUnrealEd(TEXT("PropertyWindow_PasteProperties")));
		}

		if (ContextMenu.GetMenuItemCount() > 0)
		{
			FTrackPopupMenu tpm(this, &ContextMenu);
			tpm.Show();
			bResult = TRUE;
		}
	}

	return bResult;
}

void WxItemPropertyControl::OnPaint( wxPaintEvent& In )
{
	check (PropertyNode);
	//Make sure the window is at least showing before we draw
	if (!PropertyNode->IsPartiallyUnclipped())
	{
		return;
	}

	UProperty* Property = GetProperty();

	wxBufferedPaintDC dc( this );
	const wxRect rc = PropertyNode->GetClippedClientRect();


	// Colors will be derived from the base color.
	wxColour BaseColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	//change color to more green
	if (PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsSeenDueToFiltering)) 
	{
		BaseColor = wxColour( Max(BaseColor.Red()-32, 0), BaseColor.Green(), Max(BaseColor.Blue()-32, 0) );
	} 

	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	WxPropertyWindowHost* HostWindow = MainWindow->GetParentHostWindow();
	check(HostWindow);

	const INT SplitterPos = MainWindow->GetSplitterPos()-2;
	UBOOL bIsEditConst = PropertyNode->IsEditConst();
	// If this is the focused item, draw a greenish selection background.
	UBOOL bIsFocused = HostWindow->IsWindowInActiveFocus(this);
	const INT ItemHeight = GetPropertyHeight();

	// Clear background
	//dc.SetBackground( wxBrush( wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE), wxSOLID ) );
	//dc.Clear();
	dc.SetBrush( wxBrush( BaseColor, wxSOLID ) );
	dc.SetPen( wxPen( BaseColor, 1 /* width */, wxSOLID ) );
	dc.DrawRectangle( rc);//wxRect( rc.x, rc.y, SplitterPos, ItemHeight ) );

	
	//Get the address earlier, so we can early out if the data has been stomped on by import
	BYTE* ValueAddress = NULL;
	TArray<BYTE*> Addresses;
	if (PropertyNode->GetReadAddress( PropertyNode, PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses, FALSE, TRUE ) )
	{
		ValueAddress = Addresses(0);
	}

	if( bIsFocused )
	{
		const wxColour HighlightClr = wxColour( BaseColor.Red(), 255, BaseColor.Blue() );
		dc.SetBrush( wxBrush( HighlightClr, wxSOLID ) );
		dc.SetPen( wxPen( HighlightClr, 1 /* width */, wxSOLID ) );
		if ( bIsEditConst )
		{
			// only highlight left of the splitter for edit const
			dc.DrawRectangle( wxRect( rc.x, rc.y, SplitterPos, ItemHeight ) );
		}
		else
		{
			// highlight the entire area
			dc.DrawRectangle( wxRect( rc.x, rc.y, rc.GetWidth(), ItemHeight ) );
		}
	}
	// if this is edit const then change the background color
	if ( bIsEditConst && ! PropertyNode->HasNodeFlags(EPropertyNodeFlags::ForceEditConstDisabledStyle) )
	{
		const wxColour HighlightClr = wxColour( BaseColor.Red() - 32, BaseColor.Green() - 32, BaseColor.Blue() - 32 );
		dc.SetBrush( wxBrush( HighlightClr, wxSOLID ) );
		dc.SetPen( wxPen( HighlightClr, 1 /* width */, wxSOLID ) );
		if ( bIsFocused )
		{
			// only highlight right of the splitter if focused
			dc.DrawRectangle( wxRect( SplitterPos, rc.y, rc.GetWidth() - SplitterPos, ItemHeight ) );
		}
		else
		{
			// highlight the entire area
			wxRect ExpansionRect = GetExpansionArrowRect();
			dc.DrawRectangle( wxRect( ExpansionRect.GetRight()-2, rc.y, rc.GetWidth(), ItemHeight ) );
		}
	}

	// Set the font.
	dc.SetFont( *wxNORMAL_FONT );

	// Draw an expansion arrow on the left if this node can be expanded AND it has at least one child (otherwise there is no point in drawing the arrow)
	UBOOL bSuppressExpansion = FALSE;
	if (Property && Cast<UArrayProperty>(Property))
	{
		TArray<BYTE*> Addresses;
		if (PropertyNode->GetReadAddress( PropertyNode, PropertyNode->HasNodeFlags(EPropertyNodeFlags::SingleSelectOnly), Addresses, FALSE, TRUE ) )
		{
			BYTE* ValueAddress = Addresses(0);
			bSuppressExpansion = ((FScriptArray*)ValueAddress)->Num() == 0;
		}
	}
	if ( !bSuppressExpansion )
	{
		RenderExpansionArrow( dc, BaseColor );
	}

	// Vertical splitter line
	{
		const wxColour DarkerColor( BaseColor.Red()-64, BaseColor.Green()-64, BaseColor.Blue()-64 );
		dc.SetPen( wxPen( DarkerColor, 1, wxSOLID ) );
		dc.DrawLine( SplitterPos,0, SplitterPos,rc.GetHeight() );
	}

	
	// Draw the horizontal splitter line
	if( GPropertyWindowManager->GetShowHorizontalDividers() )
	{
		dc.SetPen( wxPen( wxColour(BaseColor.Red()-25,BaseColor.Green()-25, BaseColor.Blue()-25), 1, wxSOLID ) );
		dc.DrawLine( 0, 0, rc.GetWidth(), 0 );						// horizontal
	}

	///////////////////////////////////////////////////////////////////////////
	//
	// LEFT SIDE OF SPLITTER
	//
	///////////////////////////////////////////////////////////////////////////

	// Mask out the region left of the splitter.
	dc.SetClippingRegion( rc.x,rc.y, SplitterPos-2,rc.GetHeight() );

	// Check to see if the property value differs from default.
	UBOOL bDiffersFromDefault = FALSE;
	if (ValueAddress)
	{
		bDiffersFromDefault = PropertyNode->GetDiffersFromDefault();
	}
	// If the property differs from the default, draw the text in bold.
	if ( bDiffersFromDefault )
	{
		wxFont Font = dc.GetFont();
		Font.SetWeight( wxBOLD );
		dc.SetFont( Font );
	}

	// Draw the text
	//ensure text defaults to black
	dc.SetTextForeground(wxColour(0,0,0));
	const wxRect NameRect = GetClientRect();
	RenderItemName( dc, NameRect );

	///////////////////////////////////////////////////////////////////////////
	//
	// RIGHT SIDE OF SPLITTER
	//
	///////////////////////////////////////////////////////////////////////////

	// Set up a proper clipping area and tell the draw proxy to draw the value

	dc.SetClippingRegion( SplitterPos+2,rc.y, rc.GetWidth()-(SplitterPos+2),rc.GetHeight() );


	const wxRect ValueRect( SplitterPos+2,					// x
							0,								// y
							rc.GetWidth()-(SplitterPos+2),	// width
							ItemHeight );					// height

	if( ValueAddress )
	{
		DrawProxy->Draw( &dc, ValueRect, ValueAddress, Property, InputProxy );
	}
	else
	{
		DrawProxy->DrawUnknown( &dc, ValueRect );
	}

	dc.DestroyClippingRegion();

	// Clean up

	dc.SetBrush( wxNullBrush );
	dc.SetPen( wxNullPen );
}

/**
 * Renders the expansion arrow next to the property window item.
 *
 * @param	RenderDeviceContext		the device context to use for rendering the item name
 * @param Color		the color to draw the arrow
 */
void WxItemPropertyControl::RenderExpansionArrow( wxBufferedPaintDC& RenderDeviceContext, const wxColour& Color )
{
	check(PropertyNode);

	//only draw arrows if not a zero length array
	if(PropertyNode->HasNodeFlags(EPropertyNodeFlags::CanBeExpanded))
	{
		UBOOL bHasChildren = (PropertyNode->GetNumChildNodes()>0);
		UBOOL bMightHaveChildrenWhenOpened = PropertyNode->HasNodeFlags(EPropertyNodeFlags::InitChildrenOnDemand) && !PropertyNode->HasNodeFlags(EPropertyNodeFlags::HasEverBeenExpanded);

		if(bHasChildren || bMightHaveChildrenWhenOpened)
		{
			//set up as solid or transparent
			WxPropertyWindow* MainWindow = GetPropertyWindow();
			check(MainWindow);

			UBOOL bNormalExpansion = MainWindow->IsNormalExpansionAllowed(PropertyNode);
			INT BrushAndPenStyle = bNormalExpansion ? wxSOLID : wxTRANSPARENT;
			RenderDeviceContext.SetBrush( wxBrush( Color, BrushAndPenStyle ) );
			RenderDeviceContext.SetPen( wxPen( Color, 1, BrushAndPenStyle ) );

			if (PropertyNode->GetMaxChildDepthAllowed() != 0)
			{
				RenderDeviceContext.SetTextForeground(wxColour(0,0,0));
			}
			else
			{
				//set the arrow color to be red as the data has been clipped
				RenderDeviceContext.SetTextForeground(wxColour(255,0,0));
				bNormalExpansion = FALSE;
			}

			const UBOOL bExpanded = PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded);
			wxBitmap* ArrowBitmap = NULL;
			if (bNormalExpansion)
			{
				ArrowBitmap = bExpanded ? &(GPropertyWindowManager->ArrowDownB): &(GPropertyWindowManager->ArrowRightB);
			} 
			else 
			{
				ArrowBitmap = bExpanded ? &(GPropertyWindowManager->TransparentArrowDownB): &(GPropertyWindowManager->TransparentArrowRightB);
			}
			check(ArrowBitmap);

			// Draw the down or right arrow, depending on whether the node is expanded.
			const wxRect ArrowRect = GetExpansionArrowRect();
			RenderDeviceContext.DrawBitmap( *ArrowBitmap, ArrowRect.GetLeft(), ArrowRect.GetTop(), bNormalExpansion ? true : false);
		}
	}
}

/**
 * Renders the left side of the property window item.
 *
 * @param	RenderDeviceContext		the device context to use for rendering the item name
 * @param	ClientRect				the bounding region of the property window item
 */
void WxItemPropertyControl::RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect )
{
	check(PropertyNode);

	// Get the additional offset we need based on how the window was created
	const WxPropertyWindowHost* PropertyWindowHost = GetPropertyWindow()->GetParentHostWindow();
	check(PropertyWindowHost);
	const INT Offset = PropertyWindowHost->GetPropOffset();

	//if favorites are enabled
	const WxBitmap* FavoritesBmp = GetFavoriteBitmap();
	if ( FavoritesBmp )
	{
		// render the favorites bitmap
		wxRect FavoritesRect = GetFavoritesRect();
		RenderDeviceContext.DrawBitmap( *FavoritesBmp, FavoritesRect.GetLeft(), FavoritesRect.GetTop(), 1 );
	}

	INT W, H;
	FString LeftText = GetItemName();
	RenderDeviceContext.GetTextExtent( *LeftText, &W, &H );
	wxRect NameRect = GetItemNameRect(W, H);

	RenderDeviceContext.DrawText( *LeftText, NameRect.GetLeft(), NameRect.GetTop() );

	RenderDeviceContext.DestroyClippingRegion();
}


void WxItemPropertyControl::OnInputTextChanged( wxCommandEvent& In )
{
	Refresh();
}

void WxItemPropertyControl::OnInputTextEnter( wxCommandEvent& In )
{
	check(PropertyNode);
	FPropertyNode* ParentNode = PropertyNode->GetParentNode();
	WxPropertyControl* ParentWindow = ParentNode->GetNodeWindow();

	InputProxy->SendToObjects( this );
	ParentWindow->Refresh();
	check( Cast<UPropertyInputText>(InputProxy) );
    static_cast<UPropertyInputText*>( InputProxy )->ClearTextSelection();

	Refresh();
}

void WxItemPropertyControl::OnInputComboBox( wxCommandEvent& In )
{
	InputProxy->SendToObjects( this );
	Refresh();
}



//@todo ronp - fix support for component properties (CopyCompleteValue() doesn't do the right thing).
void WxPropertyControl::OnContext_ResetToDefaults( wxCommandEvent& Event )
{
	ResetPropertyValueToDefault();

	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->RebuildSubTree(PropertyNode);
}

/**
 * Sees if the value of this property differs from any of the objects it belongs to (deals with multi-select)
 */
UBOOL WxItemPropertyControl::DiffersFromDefault(UObject* InObject, UProperty* InProperty)
{
	check( InObject );
	check( InProperty );
	check(PropertyNode);

	UBOOL bDiffersFromDefault = FALSE;

	// special case for Object class - no defaults to compare against
	if ( InObject != UObject::StaticClass() && InObject != UObject::StaticClass()->GetDefaultObject() )
	{
		FPropertyItemValueDataTracker ValueTracker(this, InObject);
		if ( ValueTracker.HasDefaultValue() )
		{
			//////////////////////////
			// Check the property against its default.
			// If the property is an object property, we have to take special measures.
			UArrayProperty* OuterArrayProperty = Cast<UArrayProperty>(InProperty->GetOuter());
			if ( OuterArrayProperty != NULL )
			{
				FScriptArray* ValueArray = (FScriptArray*)ValueTracker.PropertyValueBaseAddress;
				FScriptArray* DefaultArray = (FScriptArray*)ValueTracker.PropertyDefaultBaseAddress;

				// make sure we're not trying to compare against an element that doesn't exist
				if ( DefaultArray != NULL && PropertyNode->GetArrayIndex() >= DefaultArray->Num() )
				{
					bDiffersFromDefault = TRUE;
				}
			}

			// The property is a simple field.  Compare it against the enclosing object's default for that property.
			if ( !bDiffersFromDefault )
			{
				DWORD PortFlags = 0;
				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(InProperty);
				// Use PPF_DeepComparison for component objects
				if (ObjectProperty && ObjectProperty->PropertyClass && ObjectProperty->PropertyClass->IsChildOf(UComponent::StaticClass()))
				{
					PortFlags |= PPF_DeepComparison;
				}
				// Use PPF_DeltaComparison for instanced objects
				else if (InProperty->ContainsInstancedObjectProperty())
				{
					PortFlags |= PPF_DeltaComparison;
				}

				if ( ValueTracker.PropertyValueAddress == NULL || ValueTracker.PropertyDefaultAddress == NULL )
				{
					// if either are NULL, we had a dynamic array somewhere in our parent chain and the array doesn't
					// have enough elements in either the default or the object
					bDiffersFromDefault = TRUE;
				}
				else if ( PropertyNode->GetArrayIndex() == INDEX_NONE && InProperty->ArrayDim > 1 )
				{
					for ( INT Idx = 0; !bDiffersFromDefault && Idx < InProperty->ArrayDim; Idx++ )
					{
						bDiffersFromDefault = !InProperty->Identical(
							ValueTracker.PropertyValueAddress + Idx * InProperty->ElementSize,
							ValueTracker.PropertyDefaultAddress + Idx * InProperty->ElementSize,
							PortFlags
							);
					}
				}
				else
				{
					bDiffersFromDefault = !InProperty->Identical(
						ValueTracker.PropertyValueAddress,
						ValueTracker.PropertyDefaultAddress,
						PortFlags
						);
				}
			}
		}
	}

	return bDiffersFromDefault;
}

/**
 * Resets the value of the property associated with this item to its default value, if applicable.
 */
void WxItemPropertyControl::ResetPropertyValueToDefault()
{
	check(PropertyNode);
	UProperty* Property = GetProperty();
	check(Property);

	// Note the current property window so that CALLBACK_ObjectPropertyChanged
	// doesn't destroy the window out from under us.
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->ChangeActiveCallbackCount(1);

	// The property is a simple field.  Compare it against the enclosing object's default for that property.
	////////////////
	FReloadObjectArc* OldMemoryArchive = GMemoryArchive;
	FArchetypePropagationArc* PropagationArchive;
	GMemoryArchive = PropagationArchive = new FArchetypePropagationArc();

	UBOOL bNotfiedPreChange = FALSE;

	// Get an iterator for the enclosing objects.
	FObjectPropertyNode* ObjectNode = PropertyNode->FindObjectItemParent();
	for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
	{
		UObject* Object = *Itor;

		// special case for UObject class - it has no defaults
		if ( Object != UObject::StaticClass() && Object != UObject::StaticClass()->GetDefaultObject() )
		{
			FPropertyItemValueDataTracker ValueTracker(this, Object);
			if ( ValueTracker.HasDefaultValue() )
			{
				// If the object we are modifying is in the PIE world, than make the PIE world the active
				// GWorld.  Assumes all objects managed by this property window belong to the same world.
				UWorld* OldGWorld = NULL;
				if ( GUnrealEd && GUnrealEd->PlayWorld && Object->IsIn(GUnrealEd->PlayWorld))
				{
					OldGWorld = SetPlayInEditorWorld(GUnrealEd->PlayWorld);
				}

				GMemoryArchive->ActivateWriter();

				NotifyPreChange(Property, ValueTracker.PropertyValueRoot.OwnerObject);
				bNotfiedPreChange = TRUE;

				if ( ValueTracker.PropertyDefaultAddress != NULL )
				{
					UObject* RootObject = ValueTracker.GetTopLevelObject();

					FPropertyItemComponentCollector ComponentCollector(ValueTracker);
					
					// dynamic arrays are the only property type that do not support CopySingleValue correctly due to the fact that they cannot
					// be used in a static array
					UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
					if ( ArrayProp != NULL )
					{
						Property->CopyCompleteValue(ValueTracker.PropertyValueAddress, ValueTracker.PropertyDefaultAddress, RootObject, ValueTracker.PropertyValueRoot.OwnerObject);
					}
					else
					{
						if ( PropertyNode->GetArrayIndex() == INDEX_NONE && Property->ArrayDim > 1 )
						{
							Property->CopyCompleteValue(ValueTracker.PropertyValueAddress, ValueTracker.PropertyDefaultAddress, RootObject, ValueTracker.PropertyValueRoot.OwnerObject);
						}
						else
						{
							Property->CopySingleValue(ValueTracker.PropertyValueAddress, ValueTracker.PropertyDefaultAddress, RootObject, ValueTracker.PropertyValueRoot.OwnerObject);
						}
					}

					if ( ComponentCollector.Components.Num() > 0 )
					{
						TMap<UComponent*,UComponent*> ReplaceMap;
						FPropertyItemComponentCollector DefaultComponentCollector(ValueTracker);
						for ( INT CompIndex = 0; CompIndex < ComponentCollector.Components.Num(); CompIndex++ )
						{
							UComponent* Component = ComponentCollector.Components(CompIndex);
							if ( DefaultComponentCollector.Components.HasKey(Component->GetArchetype<UComponent>()) )
							{
								ReplaceMap.Set(Component, Component->GetArchetype<UComponent>());
							}
							else
							{
								ReplaceMap.Set(Component, DefaultComponentCollector.Components(CompIndex));
							}
						}

// 		UObject* InSearchObject,
// 		const TMap<T*,T*>& inReplacementMap,
// 		UBOOL bNullPrivateRefs,
// 		UBOOL bIgnoreOuterRef,
// 		UBOOL bIgnoreArchetypeRef,
// 		UBOOL bDelayStart=FALSE
						FArchiveReplaceObjectRef<UComponent> ReplaceAr(RootObject, ReplaceMap, FALSE, TRUE, TRUE);

						FObjectInstancingGraph InstanceGraph(RootObject);

						TArray<UObject*> Subobjects;
						FArchiveObjectReferenceCollector Collector(
							&Subobjects,	//	InObjectArray
							RootObject,		//	LimitOuter
							FALSE,			//	bRequireDirectOuter
							TRUE,			//	bIgnoreArchetypes
							TRUE,			//	bSerializeRecursively
							FALSE			//	bShouldIgnoreTransient
							);

						RootObject->Serialize(Collector);
						for ( INT ObjIndex = 0; ObjIndex < Subobjects.Num(); ObjIndex++ )
						{
							UObject* SubObj = Subobjects(ObjIndex);
							InstanceGraph.AddObjectPair(SubObj);

							UComponent* Comp = Cast<UComponent>(SubObj);
							if ( Comp != NULL )
							{
								InstanceGraph.AddComponentPair(Comp->GetArchetype<UComponent>(), Comp);
							}
						}

						RootObject->InstanceComponentTemplates(&InstanceGraph);
					}
				}
				else
				{
					Property->ClearValue(ValueTracker.PropertyValueAddress);
				}

				if (( InputProxy != NULL ) && PropertyNode->HasNodeFlags(EPropertyNodeFlags::ChildControlsCreated))
				{
					InputProxy->RefreshControlValue(Property, ValueTracker.PropertyValueAddress);
				}

				GMemoryArchive->ActivateReader();
				//assume reset to default, can change topology
				const UBOOL bTopologyChange = TRUE;
				// Call PostEditChange on all objects.
				FPropertyChangedEvent ChangeEvent(Property, bTopologyChange);
				NotifyPostChange( ChangeEvent );

				if (OldGWorld)
				{
					// restore the original (editor) GWorld
					RestoreEditorWorld( OldGWorld );
				}
			}
		}
	}

	if ( bNotfiedPreChange )
	{
		PropertyNode->FastRefreshEntireTree();
	}

	// if GMemoryArchive is still pointing to the one we created, restore it to the previous value
	if ( GMemoryArchive == PropagationArchive )
	{
		GMemoryArchive = OldMemoryArchive;
	}

	// clean up the FArchetypePropagationArc we created
	delete PropagationArchive;
	PropagationArchive = NULL;

	// Unset, effectively making this property window updatable by CALLBACK_ObjectPropertyChanged.
	MainWindow->ChangeActiveCallbackCount(-1);
}

/**
 * virtualized to allow custom naming
 */
FString WxItemPropertyControl::GetItemName (void) const
{
	check(PropertyNode);
	if( PropertyNode->GetArrayIndex()==-1 )
	{
		return GetDisplayName();
	}
	else
	{
		const UProperty* Property = GetProperty();
		if ( Property == NULL || Property->ArraySizeEnum == NULL )
		{
			return FString::Printf(TEXT("[%i]"), PropertyNode->GetArrayIndex() );
		}
		else
		{
			return FString::Printf(TEXT("[%s]"), *Property->ArraySizeEnum->GetEnum(PropertyNode->GetArrayIndex()).ToString());
		}
	}
}

/**
 * virtualized to allow custom naming
 */
FString WxItemPropertyControl::GetDisplayName (void) const
{
	const UProperty* Property = GetProperty();
	check(Property);
	
	//if in "readable display name mode" return that
	if (GPropertyWindowManager->GetShowFriendlyPropertyNames())
	{
		//if there is a localized version, return that
		FString LocalizedName = LocalizeProperty( *(Property->GetFullGroupName(TRUE)), *(Property->GetName()) );	
		if (LocalizedName.Len() > 0)
		{
			return LocalizedName;
		}
		else
		{
			FString FinalDisplayName = GetProperty()->GetMetaData(TEXT("DisplayName"));
			if ( FinalDisplayName.Len() == 0 )
			{
				FinalDisplayName = DisplayName;
			}
			return FinalDisplayName;
		}
	}

	return Property->GetName();
}

/**Returns the rect for favorites toggle*/
wxRect WxItemPropertyControl::GetFavoritesRect (void) const
{
	check(PropertyNode);

	const wxRect rc = GetClientRect();
	wxRect FavoritesRect (rc.x, rc.y, PROP_Indent, rc.GetBottom() - rc.GetTop());
	return FavoritesRect;

	//RenderDeviceContext.DrawBitmap( *FavoritesBmp, FavoritesRect.GetLeft(), FavoritesRect.GetTop() + ((PROP_DefaultItemHeight - FavoritesBmp->GetHeight()) / 2), 1 );
}

/**Returns the rect for expansion arrow*/
wxRect WxItemPropertyControl::GetExpansionArrowRect (void) const
{
	check(PropertyNode);

	const WxPropertyWindowHost* PropertyWindowHost = GetPropertyWindow()->GetParentHostWindow();
	check(PropertyWindowHost);

	//if favorites are enabled
	INT IndentX = PropertyNode->GetIndentX();

	const INT Offset = PropertyWindowHost->GetPropOffset();
	const wxRect rc = PropertyNode->GetClippedClientRect();
	const wxRect ExpansionRect( rc.x + IndentX - Offset + GetFavoriteWidth(), ((PROP_DefaultItemHeight - GPropertyWindowManager->ArrowDownB.GetHeight())/2), PROP_Indent, rc.height );
	return ExpansionRect;
	//RenderDeviceContext.DrawBitmap( *ArrowBitmap, IndentX - Offset, wkrc.y+((PROP_DefaultItemHeight - GPropertyWindowManager->ArrowDownB.GetHeight())/2), bNormalExpansion ? true : false);
}

/**Returns the rect for checkbox used in conditional item property controls.  In the base implementation to keep it close to the favorites rect function*/
wxRect WxItemPropertyControl::GetConditionalRect (void) const
{
	check(PropertyNode);

	wxRect ArrowRect = GetExpansionArrowRect();

	const wxRect rc = PropertyNode->GetClippedClientRect();
	//default to No width.  Handled in derived implementation
	const wxRect ConditionalRect( ArrowRect.GetRight(), ArrowRect.GetTop(), 0, rc.height );

	return ConditionalRect;
}

/**Returns the left side of the title of this property*/
wxRect WxItemPropertyControl::GetItemNameRect (const INT Width, const INT Height) const
{
	check(PropertyNode);

	wxRect ConditionalRect = GetConditionalRect();

	const wxRect rc = PropertyNode->GetClippedClientRect();
	const wxRect NameRect( ConditionalRect.GetRight(), ((PROP_DefaultItemHeight - Height) / 2), rc.width - ConditionalRect.GetRight(), rc.height );
	return NameRect;
}

/**
 * Returns TRUE if MouseX and MouseY fall within the bounding region of the favorites bitmap 
 */
UBOOL WxItemPropertyControl::ClickedFavoritesToggle( INT MouseX, INT MouseY ) const
{
	check(PropertyNode);

	WxPropertyWindow* PropertyWindow = PropertyNode->GetMainWindow();
	if (PropertyWindow && PropertyWindow->IsFavoritesFeatureEnabled() && !PropertyNode->IsChildOfFavorite())
	{
		//Disabling check box
		const wxRect FavoritesRect = GetFavoritesRect();
		if (FavoritesRect.Contains(MouseX, MouseY))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Returns the bitmap to be used as the favorite, null if none
 */
const WxBitmap* WxItemPropertyControl::GetFavoriteBitmap( void ) const
{
	check(PropertyNode);

	const WxPropertyWindow* PropertyWindow = GetPropertyWindow();
	if (PropertyWindow && PropertyWindow->IsFavoritesFeatureEnabled() && !PropertyNode->IsChildOfFavorite())
	{
		//is this item set to be a favorite?
		UBOOL bIsItemFavorite = PropertyNode->HasNodeFlags(EPropertyNodeFlags::IsFavorite);

		// determine which bitmap image to render
		return bIsItemFavorite ? &GPropertyWindowManager->FavoritesOnImage : &GPropertyWindowManager->FavoritesOffImage;
	}

	return NULL;
}

/**
 * Returns the width of the bitmap to be used as the favorite
 */
const INT WxItemPropertyControl::GetFavoriteWidth( void ) const
{
	check(PropertyNode);

	// isn't being drawn in favorites split
	if ( PropertyNode->GetFavoriteIndex() == PROP_Default_Favorite_Index )
	{
		const WxBitmap* FavoritesBmp = GetFavoriteBitmap();
		if ( FavoritesBmp )
		{
			// Move the indent to take this into account
			return FavoritesBmp->GetWidth();
		}
	}
	return 0;
}

/**Toggles whether or not this value is a favorite or not*/
void WxItemPropertyControl::ToggleFavoriteState(void)
{
	check(PropertyNode);
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->SetFavorite(PropertyNode, !HasNodeFlags(EPropertyNodeFlags::IsFavorite));
}

/**
 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
 * and (if the property window item is expandable) to toggle expansion state.
 *
 * @param	Event	the mouse click input that generated the event
 *
 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
 */
UBOOL WxItemPropertyControl::ClickedPropertyItem( wxMouseEvent& In )
{
	// if they clicked on the checkbox, toggle the edit condition
	if ( ClickedFavoritesToggle(In.GetX(), In.GetY()) )
	{
		ToggleFavoriteState();
	}
	else
	{
		return WxPropertyControl::ClickedPropertyItem(In);
	}
	return FALSE;
}


/* ==========================================================================================================
	WxCustomPropertyItem_ConditionalItem
========================================================================================================== */
IMPLEMENT_DYNAMIC_CLASS(WxCustomPropertyItem_ConditionalItem,WxItemPropertyControl);

/**
 * Returns the property used for determining whether this item's Property can be edited.
 *
 * @todo ronp - eventually, allow any type of property by specifying required value in metadata
 */
UBoolProperty* WxCustomPropertyItem_ConditionalItem::GetEditConditionProperty(UBOOL& bNegate) const
{
	UBoolProperty* EditConditionProperty = NULL;
	bNegate = FALSE;
	const UProperty* Property = GetProperty();
	if ( Property != NULL )
	{
		// find the name of the property that should be used to determine whether this property should be editable
		FString ConditionPropertyName = Property->GetMetaData(TEXT("EditCondition"));

		// Support negated edit conditions whose syntax is !BoolProperty
		if (ConditionPropertyName.StartsWith(FString(TEXT("!"))))
		{
			bNegate = TRUE;
			// Chop off the negation from the property name
			ConditionPropertyName = ConditionPropertyName.Right(ConditionPropertyName.Len() - 1);
		}

		// for now, only support boolean conditions, and only allow use of another property within the same struct as the conditional property
		if ( ConditionPropertyName.Len() > 0 && ConditionPropertyName.InStr(TEXT(".")) == INDEX_NONE )
		{
			UStruct* Scope = Property->GetOwnerStruct();
			EditConditionProperty = FindField<UBoolProperty>(Scope, *ConditionPropertyName);
		}
	}
	return EditConditionProperty;
}

/**
 * Finds the property being used to determine whether this item's associated property should be editable/expandable.
 * If the propery is successfully found, ConditionPropertyAddresses will be filled with the addresses of the conditional properties' values.
 *
 * @param	ConditionProperty	receives the value of the property being used to control this item's editability.
 * @param	ConditionPropertyAddresses	receives the addresses of the actual property values for the conditional property
 *
 * @return	TRUE if both the conditional property and property value addresses were successfully determined.
 */
UBOOL WxCustomPropertyItem_ConditionalItem::GetEditConditionPropertyAddress( UBoolProperty*& ConditionProperty, TArray<FPropertyConditionInfo>& ConditionPropertyAddresses )
{
	UBOOL bResult = FALSE;
	UBOOL bNegate = FALSE;
	UBoolProperty* EditConditionProperty = GetEditConditionProperty(bNegate);
	if ( EditConditionProperty != NULL )
	{
		check(PropertyNode);
		FPropertyNode* ParentNode = PropertyNode->GetParentNode();
		check(ParentNode);

		UProperty* Property = PropertyNode->GetProperty();
		if (Property)
		{
			UBOOL bStaticArray = (Property->ArrayDim > 1) && (PropertyNode->GetArrayIndex() != INDEX_NONE);
			if (bStaticArray)
			{
				//in the case of conditional static arrays, we have to go up one more level to get the proper parent struct.
				ParentNode = ParentNode->GetParentNode();
				check(ParentNode);
			}
		}


		FObjectPropertyNode* ObjectNode = ParentNode->FindObjectItemParent();
		for ( TPropObjectIterator Itor( ObjectNode->ObjectIterator() ) ; Itor ; ++Itor )
		{
			UObject* Obj = *Itor;

			// Get the address corresponding to the base of this property (i.e. if a struct property, set BaseOffset to the address of value for the whole struct)
			BYTE* BaseOffset = ParentNode->GetValueAddress((BYTE*)Obj);
			check(BaseOffset != NULL);

			FPropertyConditionInfo NewCondition;
			// now calculate the address of the property value being used as the condition and add it to the array.
			NewCondition.Address = BaseOffset + EditConditionProperty->Offset;
			NewCondition.bNegateValue = bNegate;
			ConditionPropertyAddresses.AddItem(NewCondition);
			bResult = TRUE;
		}
	}

	if ( bResult )
	{
		// set the output variable
		ConditionProperty = EditConditionProperty;
	}

	return bResult;
}

/**
 * Returns TRUE if the value of the conditional property matches the value required.  Indicates whether editing or otherwise interacting with this item's
 * associated property should be allowed.
 */
UBOOL WxCustomPropertyItem_ConditionalItem::IsConditionMet()
{
	UBOOL bResult = FALSE;

	UBoolProperty* ConditionProperty;
	TArray<FPropertyConditionInfo> ConditionValues;

	// get the property we're going to use as the condition for allowing edits, as well as the addresses of the values for that property in all selected objects
	if ( GetEditConditionPropertyAddress(ConditionProperty, ConditionValues) )
	{
		UBOOL bAllConditionsMet = TRUE;
		
		for ( INT ValueIdx = 0; bAllConditionsMet && ValueIdx < ConditionValues.Num(); ValueIdx++ )
		{
			BYTE* ValueAddr = ConditionValues(ValueIdx).Address;
			if (ConditionValues(ValueIdx).bNegateValue)
			{
				bAllConditionsMet = (*(BITFIELD*)ValueAddr & ConditionProperty->BitMask) == 0;
			}
			else
			{
				bAllConditionsMet = (*(BITFIELD*)ValueAddr & ConditionProperty->BitMask) != 0;
			}
		}

		bResult = bAllConditionsMet;
	}

	return bResult;
}

/**Returns the left side of the title of this property*/
wxRect WxCustomPropertyItem_ConditionalItem::GetConditionalRect (void) const
{
	wxRect ConditionalRect = WxItemPropertyControl::GetConditionalRect();

	if( SupportsEditConditionCheckBox() )
	{
		//make room for the conditional item
		ConditionalRect.SetWidth(PROP_Indent);
	}
	
	return ConditionalRect;
}

/**
 * Returns TRUE if MouseX and MouseY fall within the bounding region of the checkbox used for displaying the value of this property's edit condition.
 */
UBOOL WxCustomPropertyItem_ConditionalItem::ClickedCheckbox( INT MouseX, INT MouseY ) const
{
	if( SupportsEditConditionCheckBox() )
	{
		//Disabling check box
		wxRect ConditionalRect = GetConditionalRect();
		if (ConditionalRect.Contains(MouseX, MouseY))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Toggles the value of the property being used as the condition for editing this property.
 *
 * @return	the new value of the condition (i.e. TRUE if the condition is now TRUE)
 */
UBOOL WxCustomPropertyItem_ConditionalItem::ToggleConditionValue()
{
	UBOOL bNewValue = FALSE;

	UBoolProperty* ConditionProperty;
	TArray<FPropertyConditionInfo> ConditionValues;

	// get the property we're going to use as the condition for allowing edits, as well as the addresses of the values for that property in all selected objects
	if ( GetEditConditionPropertyAddress(ConditionProperty, ConditionValues) )
	{
		bNewValue = !IsConditionMet();
		for ( INT ValueIdx = 0; ValueIdx < ConditionValues.Num(); ValueIdx++ )
		{
			BYTE* ValueAddr = ConditionValues(ValueIdx).Address;
			if ( XOR(bNewValue, ConditionValues(ValueIdx).bNegateValue) )
			{
				*(BITFIELD*)ValueAddr |= ConditionProperty->BitMask;
			}
			else
			{
				*(BITFIELD*)ValueAddr &= ~ConditionProperty->BitMask;
			}
		}
	}

	return bNewValue;
}



/**
 * Returns true if this item supports display/toggling a check box for it's conditional parent property
 *
 * @return True if a check box control is needed for toggling the EditCondition property here
 */
UBOOL WxCustomPropertyItem_ConditionalItem::SupportsEditConditionCheckBox() const
{
	// Only draw the check box for toggling the EditCondition parent property if that property isn't
	// already exposed for editing.  If that property is already shown in the window then it doesn't
	// make sense to draw it again.
	UBOOL bIsConditionalPropertyVisible = FALSE;
	{
		UBOOL bNegateValue = FALSE;
		UBoolProperty* ConditionalProperty = const_cast<UBoolProperty*>( GetEditConditionProperty(bNegateValue) );
		if( ConditionalProperty != NULL )
		{
			if( ConditionalProperty->HasAllPropertyFlags( CPF_Edit ) ||
				ConditionalProperty->HasAnyPropertyFlags( CPF_EditConst ))
			{
				// Conditionally-dependent property is already exposed for editing, so no need to draw another
				// check box next to this property's label
				bIsConditionalPropertyVisible = TRUE;
			}
		}
	}

	return !bIsConditionalPropertyVisible;
}

/**
 * Renders the left side of the property window item.
 *
 * This version is responsible for rendering the checkbox used for toggling whether this property item window should be enabled.
 *
 * @param	RenderDeviceContext		the device context to use for rendering the item name
 * @param	ClientRect				the bounding region of the property window item
 */
void WxCustomPropertyItem_ConditionalItem::RenderItemName( wxBufferedPaintDC& RenderDeviceContext, const wxRect& ClientRect )
{
	// Only draw the check box for toggling the EditCondition parent property if that property isn't
	// already exposed for editing.  If that property is already shown in the window then it doesn't
	// make sense to draw it again.
	UBOOL bIsConditionalPropertyVisible = FALSE;
	{
		bIsConditionalPropertyVisible = !SupportsEditConditionCheckBox();
	}

	const UBOOL bItemEnabled = IsConditionMet();

	if( !bIsConditionalPropertyVisible )
	{
		// determine which checkbox image to render
 		const WxMaskedBitmap& bmp = bItemEnabled
			? GPropertyWindowManager->CheckBoxOnB
			: GPropertyWindowManager->CheckBoxOffB;

		// render the checkbox bitmap
		wxRect ConditionalRect = GetConditionalRect();
		RenderDeviceContext.DrawBitmap( bmp, ConditionalRect.GetLeft(), ConditionalRect.GetTop() + ((PROP_DefaultItemHeight - bmp.GetHeight()) / 2), 1 );
	}

	// now render the text itself; if the item is disabled, change the font color to light grey
	if ( !bItemEnabled )
	{
		RenderDeviceContext.SetTextForeground(wxColour( 128, 128, 128));
	}

	WxItemPropertyControl::RenderItemName(RenderDeviceContext, ClientRect);
}

/**
 * Called when an property window item receives a left-mouse-button press which wasn't handled by the input proxy.  Typical response is to gain focus
 * and (if the property window item is expandable) to toggle expansion state.
 *
 * @param	Event	the mouse click input that generated the event
 *
 * @return	TRUE if this property window item should gain focus as a result of this mouse input event.
 */
UBOOL WxCustomPropertyItem_ConditionalItem::ClickedPropertyItem( wxMouseEvent& Event )
{
	check(PropertyNode);
	UBOOL bShouldGainFocus = TRUE;

	UProperty* Property = GetProperty();

	// Note the current property window so that CALLBACK_ObjectPropertyChanged
	// doesn't destroy the window out from under us.
	WxPropertyWindow* MainWindow = GetPropertyWindow();
	check(MainWindow);
	MainWindow->ChangeActiveCallbackCount(1);

	// if this property is edit-const, it can't be changed
	// or if we couldn't find a valid condition property, also use the base version
	UBOOL bNegateValue = FALSE;
	if ( Property == NULL || PropertyNode->IsEditConst() || GetEditConditionProperty(bNegateValue) == NULL )
	{
		bShouldGainFocus = WxItemPropertyControl::ClickedPropertyItem(Event);
	}
	// if they clicked on the checkbox, toggle the edit condition
	else if ( ClickedCheckbox(Event.GetX(), Event.GetY()) )
	{
		NotifyPreChange(Property);
		const UBOOL bCanBeExpanded = PropertyNode->HasNodeFlags(EPropertyNodeFlags::CanBeExpanded);
		bShouldGainFocus = !bCanBeExpanded;
		if ( ToggleConditionValue() == FALSE )
		{
			bShouldGainFocus = FALSE;

			// if we just disabled the condition which allows us to edit this control
			// collapse the item if this is an expandable item
			if (bCanBeExpanded)
			{
				const UBOOL bExpand = FALSE;
				const UBOOL bRecurse = FALSE;
				PropertyNode->SetExpanded(bExpand, bRecurse);
			}

			WxPropertyWindow* MainWindow = GetPropertyWindow();
			check(MainWindow);
			MainWindow->RequestMainWindowTakeFocus();	//re-assign focus, to get a focus kill.
		}
		const UBOOL bTopologyChange = TRUE;
		FPropertyChangedEvent ChangeEvent(Property, bTopologyChange);
		NotifyPostChange(ChangeEvent);

		WxPropertyControl* RefreshWindow = NULL;
		FPropertyNode* ParentNode = PropertyNode->GetParentNode();
		if ( !bCanBeExpanded && ParentNode != NULL )
		{
			RefreshWindow = ParentNode->GetNodeWindow();
		}
		else
		{
			RefreshWindow = this;
		}
		check(RefreshWindow);
		RefreshWindow->Refresh();
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

	// Unset, effectively making this property window updatable by CALLBACK_ObjectPropertyChanged.
	MainWindow->ChangeActiveCallbackCount(-1);

	return bShouldGainFocus;
}

/**
 * Called when property window item receives a left down click.  Derived version must test for double clicking in the check box.
 * @param InX - Window relative cursor x position
 * @param InY - Window relative cursor y position
 */
void WxCustomPropertyItem_ConditionalItem::PrivateLeftDoubleClick(const INT InX, const INT InY)
{
	if ( ClickedCheckbox(InX, InY) )
	{
		return;
	}
	else
	{
		// Don't allow double click to change the property's value unless our enable condition is met
		if( IsConditionMet() )
		{
			WxItemPropertyControl::PrivateLeftDoubleClick(InX, InY);
		}
	}
}


/**
 * Expands child items.  Does nothing if the window is already expanded.
 */
void WxCustomPropertyItem_ConditionalItem::Expand(const TArray<FString>& InFilterStrings)
{
	if ( IsConditionMet() )
	{
		check(PropertyNode);
		const UBOOL bExpand = TRUE;
		const UBOOL bRecurse = FALSE;
		PropertyNode->SetExpanded(bExpand, bRecurse);
	}
}

/**
 * Recursively searches through children for a property named PropertyName and expands it.
 * If it's a UArrayProperty, the propery's ArrayIndex'th item is also expanded.
 */
void WxCustomPropertyItem_ConditionalItem::ExpandItem( const FString& PropertyName, INT ArrayIndex )
{
	if ( IsConditionMet() )
	{
		check(PropertyNode);
		const UBOOL bExpand = TRUE;
		PropertyNode->SetExpandedItem(PropertyName, ArrayIndex, bExpand);
	}
}

/**
 * Expands all items rooted at the property window node.
 */
void WxCustomPropertyItem_ConditionalItem::ExpandAllItems(const TArray<FString>& InFilterStrings)
{
	if ( IsConditionMet() )
	{
		check(PropertyNode);
		const UBOOL bExpand = TRUE;
		const UBOOL bRecurse = TRUE;
		PropertyNode->SetExpanded(bExpand, bRecurse);
	}
}

