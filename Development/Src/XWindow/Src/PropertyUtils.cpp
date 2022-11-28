
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	XPropertyDrawProxy
-----------------------------------------------------------------------------*/

void UPropertyDrawProxy::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	FString Str;
	InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized );

	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );

	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

void UPropertyDrawProxy::DrawUnknown( wxDC* InDC, wxRect InRect )
{
	FString Str = TEXT("");
	
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );

	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawNumeric
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawNumeric::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( (InItem->Property->IsA(UFloatProperty::StaticClass())
		||	InItem->Property->IsA(UIntProperty::StaticClass())
		||	(InItem->Property->IsA(UByteProperty::StaticClass()) && Cast<UByteProperty>(InItem->Property)->Enum == NULL))
		&& !( Cast<UStructProperty>(InItem->ParentItem->Property) && Cast<UStructProperty>(InItem->ParentItem->Property)->Struct->GetFName() == NAME_Rotator ) )
	{
		return 1;
	}

	return 0;
}

void UPropertyDrawNumeric::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	// By default, draw the equation.  If it's empty, draw the actual value.

	FString Str = ((UPropertyInputNumeric*)InInputProxy)->Equation;
	if( Str.Len() == 0 )
	{
		InProperty->ExportText( 0, Str, InReadAddress - InProperty->Offset, InReadAddress - InProperty->Offset, NULL, PPF_Localized );
	}

	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );

	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawColor
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawColor::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	return ( Cast<UStructProperty>(InItem->Property) && (Cast<UStructProperty>(InItem->Property)->Struct->GetFName()==NAME_Color || Cast<UStructProperty>(InItem->Property)->Struct->GetFName()==NAME_LinearColor) );
}

void UPropertyDrawColor::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	INT NumButtons = InInputProxy ? InInputProxy->Buttons.Num() : 0;
	DWORD D = (*(DWORD*)InReadAddress);
	FColor	Color;
	if(Cast<UStructProperty>(InProperty)->Struct->GetFName() == NAME_Color)
		Color = *(FColor*)InReadAddress;
	else
		Color = FColor(*(FLinearColor*)InReadAddress);
	wxColour WkColor( Color.R, Color.G, Color.B );
	InDC->SetPen( *wxBLACK_PEN );
	InDC->SetBrush( wxBrush( WkColor, wxSOLID ) );
	InDC->DrawRectangle( InRect.x+4,InRect.y+4, InRect.GetWidth()-(NumButtons*PB_ButtonSz)-12,InRect.GetHeight()-8 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawRotation
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawRotation::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( Cast<UStructProperty>(InItem->ParentItem->Property) && Cast<UStructProperty>(InItem->ParentItem->Property)->Struct->GetFName() == NAME_Rotator )
	{
		return 1;
	}

	return 0;
}

void UPropertyDrawRotation::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	FLOAT Val = *((INT*)InReadAddress);
	Val = 360.f * (Val / 65536.f);

	FString Wk;
	if( ::fabs(Val) > 359.f )
	{
		INT Revolutions = Val / 360.f;
		Val -= Revolutions * 360;
		Wk = FString::Printf( TEXT("%.2f%c %s %d"), Val, 176, (Revolutions < 0)?TEXT("-"):TEXT("+"), abs(Revolutions) );
	}
	else
	{
		Wk = FString::Printf( TEXT("%.2f%c"), Val, 176 );
	}

	// note : The degree symbol is ASCII code 248 (char code 176)
	InDC->DrawText( *Wk, InRect.x,InRect.y+1 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawRotationHeader
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawRotationHeader::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( Cast<UStructProperty>(InItem->Property) && Cast<UStructProperty>(InItem->Property)->Struct->GetFName() == NAME_Rotator )
	{
		return 1;
	}

	return 0;
}

void UPropertyDrawRotationHeader::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	InDC->DrawText( TEXT("..."), InRect.x+2,InRect.y+1 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawBool
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawBool::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	return ( Cast<UBoolProperty>(InItem->Property) != NULL );
}

void UPropertyDrawBool::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	WxMaskedBitmap* bmp = (*(BITFIELD*)InReadAddress & Cast<UBoolProperty>(InProperty)->BitMask) ? &GPropertyWindowManager->CheckBoxOnB : &GPropertyWindowManager->CheckBoxOffB;

	InDC->DrawBitmap( *bmp, InRect.x, InRect.y+((InRect.GetHeight() - 13) / 2), 1 );
}

void UPropertyDrawBool::DrawUnknown( wxDC* InDC, wxRect InRect )
{
	WxMaskedBitmap* bmp = &GPropertyWindowManager->CheckBoxUnknownB;

	InDC->DrawBitmap( *bmp, InRect.x, InRect.y+((InRect.GetHeight() - 13) / 2), 1 );
}

/*-----------------------------------------------------------------------------
	UPropertyDrawArrayHeader
-----------------------------------------------------------------------------*/

UBOOL UPropertyDrawArrayHeader::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	return ( ( InItem->Property->ArrayDim != 1 && InArrayIdx == -1 ) || Cast<UArrayProperty>(InItem->Property) );
}

void UPropertyDrawArrayHeader::Draw( wxDC* InDC, wxRect InRect, BYTE* InReadAddress, UProperty* InProperty, INT InArrayIdx, UPropertyInputProxy* InInputProxy )
{
	FString Str = TEXT("...");
	wxCoord W, H;
	InDC->GetTextExtent( *Str, &W, &H );
	InDC->DrawText( *Str, InRect.x, InRect.y+((InRect.GetHeight() - H) / 2) );
}

/*-----------------------------------------------------------------------------
	UPropertyInputProxy
-----------------------------------------------------------------------------*/

void UPropertyInputProxy::DeleteButtons()
{
	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		Buttons(x)->Destroy();
	}

	Buttons.Empty();
}

void UPropertyInputProxy::AddButton( WxPropertyWindow_Base* InItem, ePropButton InButton, wxRect InRC )
{
	WxMaskedBitmap* bmp = NULL;
	FString ToolTip;
	INT ID = 0;

	switch( InButton )
	{
		case PB_Add:
			bmp = &GPropertyWindowManager->Prop_AddB;
			ToolTip = TEXT("Add New Item");
			ID = ID_PROP_PB_ADD;
			break;

		case PB_Empty:
			bmp = &GPropertyWindowManager->Prop_EmptyB;
			ToolTip = TEXT("Remove All Items From Array");
			ID = ID_PROP_PB_EMPTY;
			break;

		case PB_Insert:
			bmp = &GPropertyWindowManager->Prop_InsertB;
			ToolTip = TEXT("Insert New Item Here");
			ID = ID_PROP_PB_INSERT;
			break;

		case PB_Delete:
			bmp = &GPropertyWindowManager->Prop_DeleteB;
			ToolTip = TEXT("Delete Item");
			ID = ID_PROP_PB_DELETE;
			break;

		case PB_Browse:
			bmp = &GPropertyWindowManager->Prop_BrowseB;
			ToolTip = TEXT("Show Generic Browser");
			ID = ID_PROP_PB_BROWSE;
			break;

		case PB_Pick:
			bmp = &GPropertyWindowManager->Prop_PickB;
			ToolTip = TEXT("Use Mouse To Pick Color From Viewport");
			ID = ID_PROP_PB_PICK;
			break;

		case PB_Clear:
			bmp = &GPropertyWindowManager->Prop_ClearB;
			ToolTip = TEXT("Clear All Text");
			ID = ID_PROP_PB_CLEAR;
			break;

		case PB_Find:
			bmp = &GPropertyWindowManager->Prop_FindB;
			ToolTip = TEXT("Use Mouse To Pick Actor From Viewport");
			ID = ID_PROP_PB_FIND;
			break;

		case PB_Use:
			bmp = &GPropertyWindowManager->Prop_UseB;
			ToolTip = TEXT("Use Current Selection In Browser");
			ID = ID_PROP_PB_USE;
			break;

		case PB_NewObject:
			bmp = &GPropertyWindowManager->Prop_NewObjectB;
			ToolTip = TEXT("Create A New Object");
			ID = ID_PROP_PB_NEWOBJECT;
			break;

		default:
			check(0);	// Unknown button type
			break;
	}

	wxRect itemrect = InItem->GetClientRect();

	wxBitmapButton* button = new wxBitmapButton( InItem, ID, *bmp );
	button->SetToolTip( *ToolTip );
	Buttons.AddItem( button );

	ParentResized( InItem, InRC );
}

void UPropertyInputProxy::CreateButtons( WxPropertyWindow_Base* InItem, wxRect InRC )
{
	Buttons.Empty();

	if( !InItem->Property )
	{
		return;
	}

	// If property is const, don't create any buttons

	if( InItem->Property->PropertyFlags&CPF_EditConst || InItem->Property->PropertyFlags&CPF_EditConstArray )
	{
		return;
	}

	if( Cast<UArrayProperty>(InItem->Property->GetOuter()) 
			&& ((((UArrayProperty*)InItem->Property->GetOuter())->PropertyFlags & CPF_EditConst)
			|| (((UArrayProperty*)InItem->Property->GetOuter())->PropertyFlags & CPF_EditConstArray) ) )
	{
		return;
	}
	
	if( InItem->Property->IsA(UArrayProperty::StaticClass() ) )
	{
		if( InItem->bSingleSelectOnly )
		{
			AddButton( InItem, PB_Add, InRC );
			AddButton( InItem, PB_Empty, InRC );
		}
	}
	if( Cast<UArrayProperty>(InItem->Property->GetOuter()) )
	{
		if( InItem->bSingleSelectOnly )
		{
			AddButton( InItem, PB_Insert, InRC );
			AddButton( InItem, PB_Delete, InRC );
		}

		// If this is coming from an array property and we are using a combobox for editing, we don't want any other buttons beyond these two

		if( ComboBox != NULL )
		{
			return;
		}
	}
	if( InItem->Property->IsA(UStructProperty::StaticClass()) && (Cast<UStructProperty>(InItem->Property)->Struct->GetFName() == NAME_Color || Cast<UStructProperty>(InItem->Property)->Struct->GetFName() == NAME_LinearColor) )
	{
		// Color.
		AddButton( InItem, PB_Browse, InRC );
		//AddButton( InItem, PB_Pick, InRC );

	}
	if( InItem->Property->IsA(UObjectProperty::StaticClass()) )
	{
		if( InItem->bEdFindable )
		{
			if( !(InItem->Property->PropertyFlags & CPF_NoClear) )
			{
				AddButton( InItem, PB_Clear, InRC );
			}
			AddButton( InItem, PB_Find, InRC );
			AddButton( InItem, PB_Use, InRC );
		}
		else if( InItem->bEditInline )
		{
			AddButton( InItem, PB_NewObject, InRC );
			if( !(InItem->Property->PropertyFlags & CPF_NoClear) )
			{
				AddButton( InItem, PB_Clear, InRC );
			}
		}
		else
		{
			// Class.

			if( Cast<UClassProperty>( InItem->Property ) == NULL )
			{
				if( !(InItem->Property->PropertyFlags & CPF_NoClear) )
				{
					AddButton( InItem, PB_Clear, InRC );
					AddButton( InItem, PB_Browse, InRC );
				}
				if( !InItem->bEditInline || InItem->bEditInlineUse )
				{
					AddButton( InItem, PB_Use, InRC );
				}
			}
		}
	}
}

void UPropertyInputProxy::ParentResized( WxPropertyWindow_Base* InItem, wxRect InRC )
{
	INT XPos = InRC.x + InRC.GetWidth() - PB_ButtonSz;
	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		Buttons(x)->SetSize( XPos,0,PB_ButtonSz,PB_ButtonSz );
		XPos -= PB_ButtonSz;
	}
}

void UPropertyInputProxy::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
}

void UPropertyInputProxy::ImportText( WxPropertyWindow* InPropWindow, UProperty* InProperty, const TCHAR* InBuffer, BYTE* InData, UObject* InParent )
{
	InPropWindow->NotifyPreChange(InProperty);
	InParent->PreEditChange();
	InProperty->ImportText( InBuffer, InData, PPF_Localized, InParent );
	InParent->PostEditChange(InProperty);
	InPropWindow->NotifyPostChange(InProperty);

	InParent->MarkPackageDirty();
}

void UPropertyInputProxy::SendTextToObjects( WxPropertyWindow_Base* InItem, FString InText )
{
	WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();

	// If more than one object is selected, an empty field indicates their values for this property differ.
	// Don't send it to the objects value in this case (if we did, they would all get set to None which isn't good)

	if( objwin->Objects.Num() > 1 && !InText.Len() )
		return;

	for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
	{
		ImportText( InItem->FindPropertyWindow(), InItem->Property, *InText, InItem->GetBase((BYTE*)objwin->Objects(i)), objwin->Objects(i) );
		objwin->Objects(i)->MarkPackageDirty();
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputBool
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputBool::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	return ( Cast<UBoolProperty>(InItem->Property) != NULL );
}

void UPropertyInputBool::DoubleClick( WxPropertyWindow_Base* InItem, UObject* InObject, BYTE* InBaseAddress, const TCHAR* InValue, UBOOL bForceValue )
{
	FString Result;

	if( bForceValue )
		Result = InValue;
	else
	{
		InItem->Property->ExportTextItem( Result, InBaseAddress, NULL, NULL, 0 );
	
		if( Result == TEXT("True") || Result == GTrue )
			Result = TEXT("False");
		else
			Result = TEXT("True");
	}

	ImportText( InItem->FindPropertyWindow(), InItem->Property, *Result, InBaseAddress, InObject );
}

UBOOL UPropertyInputBool::LeftClick( WxPropertyWindow_Base* InItem, INT InX, INT InY )
{
	// If clicking on top of the checkbox graphic, toggle it's value and return

	WxPropertyWindow* pw = InItem->FindPropertyWindow();

	InX -= pw->SplitterPos;
	if( InX > 0 && InX < GPropertyWindowManager->CheckBoxOnB.GetWidth() )
	{
		FString Value = InItem->GetPropertyText();
		UBOOL bForceValue = 0;
		if( !Value.Len() )
		{
			Value = GTrue;
			bForceValue = 1;
		}

		WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
		for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
		{
			DoubleClick( InItem, objwin->Objects(i), InItem->GetBase( (BYTE*)(objwin->Objects(i)) ), *Value, bForceValue );
		}

		InItem->Refresh();

		return 1;
	}

	return 0;
}

/*-----------------------------------------------------------------------------
	UPropertyInputColor
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputColor::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	return ( Cast<UStructProperty>(InItem->Property) && (Cast<UStructProperty>(InItem->Property)->Struct->GetFName()==NAME_Color || Cast<UStructProperty>(InItem->Property)->Struct->GetFName()==NAME_LinearColor) );
}

void UPropertyInputColor::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
	FColor	PrevColor;

	if(Cast<UStructProperty>(InItem->Property)->Struct->GetFName() == NAME_Color)
		PrevColor = *(FColor*)InItem->GetReadAddress(InItem,InItem->bSingleSelectOnly);
	else
		PrevColor = FColor(*(FLinearColor*)InItem->GetReadAddress(InItem,InItem->bSingleSelectOnly));

	switch( InButton )
	{
		case PB_Browse:
			wxColourData cd;
			cd.SetChooseFull(1);
			cd.SetColour(wxColour(PrevColor.R,PrevColor.G,PrevColor.B));
			WxDlgColor dlg;
			dlg.Create( InItem, &cd );
			if( dlg.ShowModal() == wxID_OK )
			{
				wxColour clr = dlg.GetColourData().GetColour();
				WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
				FString Value;
				if(Cast<UStructProperty>(InItem->Property)->Struct->GetFName() == NAME_Color)
					Value = FString::Printf( TEXT("(R=%i,G=%i,B=%i)"), clr.Red(), clr.Green(), clr.Blue() );
				else
					Value = FString::Printf( TEXT("(R=%1.5f,G=%f,B=%f)"), (FLOAT)clr.Red() / 255.0f, (FLOAT)clr.Green() / 255.0f, (FLOAT)clr.Blue() / 255.0f );
				for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
				{
					ImportText( InItem->FindPropertyWindow(), InItem->Property, *Value, InItem->GetBase((BYTE*)objwin->Objects(i)), objwin->Objects(i) );
				}
			}
			InItem->Refresh();
			break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputArray
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputArray::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	return ( ( InItem->Property->ArrayDim != 1 && InArrayIdx == -1 ) || Cast<UArrayProperty>(InItem->Property) );
}

void UPropertyInputArray::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Empty:
		{
			WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
			BYTE* Addr = InItem->GetReadAddress( InItem, InItem->bSingleSelectOnly );
			if( Addr )
			{
				UArrayProperty* Array = CastChecked<UArrayProperty>( InItem->Property );

				InItem->FindPropertyWindow()->NotifyPreChange(NULL);

				InItem->Collapse();
				InItem->FindPropertyWindow()->PositionChildren();
				((FArray*)Addr)->Empty( Array->Inner->ElementSize );
				InItem->Expand();

				InItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}
		}
		break;

		case PB_Add:
		{
			WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
			BYTE* Addr = InItem->GetReadAddress( InItem, InItem->bSingleSelectOnly );
			if( Addr )
			{
				UArrayProperty* Array = CastChecked<UArrayProperty>( InItem->Property );

				INT ArrayIndex = ((FArray*)Addr)->AddZeroed( Array->Inner->ElementSize, 1 );

				// Use struct defaults.
				UStructProperty* StructProperty = Cast<UStructProperty>(Array->Inner);
				if( StructProperty )
				{
					check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == Array->Inner->ElementSize );
					BYTE* Dest = (BYTE*)((FArray*)Addr)->GetData() + ArrayIndex * Array->Inner->ElementSize;
					appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
				}
			
				InItem->FindPropertyWindow()->NotifyPreChange(NULL);
				InItem->Collapse();
				InItem->FindPropertyWindow()->PositionChildren();
				InItem->Expand();
				InItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}
		}
		break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputArrayItem
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputArrayItem::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	return !Cast<UClassProperty>(InItem->Property) && Cast<UArrayProperty>(InItem->Property->GetOuter()) && InItem->bSingleSelectOnly && !(Cast<UArrayProperty>(InItem->Property->GetOuter())->PropertyFlags&CPF_EditConstArray);
}

void UPropertyInputArrayItem::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Insert:
		{
			FArray* Addr = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				BYTE* Dest = (BYTE*)Addr->GetData() + InItem->ArrayIndex*InItem->Property->GetSize();

				Addr->Insert( InItem->ArrayIndex, 1, InItem->Property->GetSize() );	
				appMemzero( Dest, InItem->Property->GetSize() );

				// Use struct defaults.
				UStructProperty* StructProperty	= Cast<UStructProperty>( InItem->Property );
				if( StructProperty )
				{
					check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == InItem->Property->GetSize() );
					appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
				}

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}

		}
		break;

		case PB_Delete:
		{
			FArray* Addr  = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				Addr->Remove( InItem->ArrayIndex, 1, InItem->Property->GetSize() );

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}
		}
		break;

		case PB_Browse:
			GCallback->Send( CALLBACK_ShowDockableWindow, DWT_GENERIC_BROWSER );
			break;

		case PB_Clear:
			SendTextToObjects( InItem, TEXT("None") );
			InItem->Refresh();
			break;

		case PB_Use:
		{
			UObjectProperty* objprop = Cast<UObjectProperty>( InItem->Property );
			if( objprop )
				if( GSelectionTools.GetTop( objprop->PropertyClass ) )
					SendTextToObjects( InItem, *GSelectionTools.GetTop( objprop->PropertyClass )->GetPathName() );

			InItem->Refresh();
		}
		break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputText
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputText::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	if(	!InItem->bEditInline
			&&	( (InItem->Property->IsA(UNameProperty::StaticClass()) && InItem->Property->GetFName() != NAME_InitialState)
			||	InItem->Property->IsA(UStrProperty::StaticClass())
			||	InItem->Property->IsA(UObjectProperty::StaticClass()) )
			&&	!InItem->Property->IsA(UComponentProperty::StaticClass()) )
		return 1;

	return 0;
}

void UPropertyInputText::CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue )
{
	UPropertyInputProxy::CreateControls( InItem, InBaseClass, InRC, InValue );

	TextCtrl = new WxTextCtrl;
	TextCtrl->Create( InItem, ID_PROPWINDOW_EDIT, InValue );
	TextCtrl->SetFocus();
	TextCtrl->SetSelection(-1,-1);
	//TextCtrl->SetDropTarget( new WxDropTarget_PropWindowObjectItem( InItem-> ) );

	ParentResized( InItem, InRC );
}

void UPropertyInputText::DeleteControls()
{
	UPropertyInputProxy::DeleteControls();

	if( TextCtrl )
	{
		TextCtrl->Destroy();
	}

	TextCtrl = NULL;
}

void UPropertyInputText::SendToObjects( WxPropertyWindow_Base* InItem )
{
	if( !TextCtrl )
		return;

	WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
	WxPropertyWindow* pw = InItem->FindPropertyWindow();

	FString Value = TextCtrl->GetValue();
	if( objwin->Objects.Num() == 1 || Value.Len() )
	{
		for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
		{
			ImportText( pw, InItem->Property, *Value, InItem->GetBase((BYTE*)objwin->Objects(i)), objwin->Objects(i) );
		}
	}
}

void UPropertyInputText::ParentResized( WxPropertyWindow_Base* InItem, wxRect InRC )
{
	UPropertyInputProxy::ParentResized( InItem, InRC );

	if( TextCtrl )
		TextCtrl->SetSize( InRC.x-4,-2,InRC.GetWidth()+6-(Buttons.Num()*PB_ButtonSz),InRC.GetHeight()+4 );
}

void UPropertyInputText::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Browse:
			//WDM GApp->EditorFrame->DockingRoot->ShowDockableWindow( DWT_GENERIC_BROWSER );
			break;

		case PB_Clear:
			TextCtrl->SetValue( TEXT("") );
			break;

		case PB_Use:
		{
			UObjectProperty* objprop = Cast<UObjectProperty>( InItem->Property );
			if( objprop )
			{
				if( GSelectionTools.GetTop( objprop->PropertyClass ) )
				{
					TextCtrl->SetValue( *GSelectionTools.GetTop( objprop->PropertyClass )->GetPathName() );

					WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
					for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
						ImportText( InItem->FindPropertyWindow(), InItem->Property, TextCtrl->GetValue().c_str(), InItem->GetReadAddress( InItem, InItem->bSingleSelectOnly ), objwin->Objects(i));
				}
			}
		}
		break;

		case PB_Insert:
		{
			FArray* Addr = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				BYTE* Dest = (BYTE*)Addr->GetData() + InItem->ArrayIndex*InItem->Property->GetSize();

				Addr->Insert( InItem->ArrayIndex, 1, InItem->Property->GetSize() );	
				appMemzero( Dest, InItem->Property->GetSize() );

				// Use struct defaults.
				UStructProperty* StructProperty	= Cast<UStructProperty>( InItem->Property );
				if( StructProperty )
				{
					check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == InItem->Property->GetSize() );
					appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
				}

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}

		}
		break;

		case PB_Delete:
		{
			FArray* Addr  = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				Addr->Remove( InItem->ArrayIndex, 1, InItem->Property->GetSize() );

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}
		}
		break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputRotation
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputRotation::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	if( Cast<UStructProperty>(InItem->ParentItem->Property) && Cast<UStructProperty>(InItem->ParentItem->Property)->Struct->GetFName() == NAME_Rotator )
	{
		return 1;
	}

	return 0;
}

void UPropertyInputRotation::CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue )
{
	FString Result = InValue;

	FLOAT Val = appAtof( InValue );
	Val = 360.f * (Val / 65536.f);

	FString Wk;
	if( ::fabs(Val) > 359.f )
	{
		INT Revolutions = Val / 360.f;
		Val -= Revolutions * 360;
		Wk = FString::Printf( TEXT("%.2f%c %s %d"), Val, 176, (Revolutions < 0)?TEXT("-"):TEXT("+"), abs(Revolutions) );
	}
	else
	{
		Wk = FString::Printf( TEXT("%.2f%c"), Val, 176 );
	}

	Result = Wk;

	UPropertyInputText::CreateControls( InItem, InBaseClass, InRC, *Result );
}

void UPropertyInputRotation::SendToObjects( WxPropertyWindow_Base* InItem )
{
	if( !TextCtrl )
		return;

	WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
	WxPropertyWindow* pw = InItem->FindPropertyWindow();

	FString Value = TextCtrl->GetValue();

	if( objwin->Objects.Num() == 1 || Value.Len() )
	{
		// Parse the input back from the control (i.e. "80.0?- 2")

		FLOAT Val;

		TArray<FString> Split;
		if( Value.ParseIntoArray( TEXT(" "), &Split ) == 3 )
		{
			INT Sign = (Split(1) == TEXT("+")) ? +1 : -1;
			INT Revolutions = appAtof( *Split(2) );

			Val = appAtof( *Split(0) ) + (360 * (Revolutions * Sign));
		}
		else
		{
			Val = appAtof( *Value );
		}

		// Convert back to Unreal units

		//debugf( TEXT("Rotation : %1.1f"), Val );
		Val = 65536.f * (Val / 360.f);
		Value = *FString::Printf( TEXT("%f"), Val );
		//debugf( TEXT("Unreal   : %s"), *Value );

		for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
		{
			ImportText( pw, InItem->Property, *Value, InItem->GetBase((BYTE*)objwin->Objects(i)), objwin->Objects(i) );
		}
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputNumeric (with support for equations)
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputNumeric::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	if(	!InItem->bEditInline
		&& ( InItem->Property->IsA(UFloatProperty::StaticClass())
		||	InItem->Property->IsA(UIntProperty::StaticClass())
		||	(InItem->Property->IsA(UByteProperty::StaticClass()) && Cast<UByteProperty>(InItem->Property)->Enum == NULL) ) )
	{
		return 1;
	}

	return 0;
}

void UPropertyInputNumeric::CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue )
{
	// Set the equation initially to be the same as the numeric value in the property

	if( Equation.Len() == 0 )
	{
		Equation = InValue;
	}

	UPropertyInputText::CreateControls( InItem, InBaseClass, InRC, *Equation );
}

void UPropertyInputNumeric::SendToObjects( WxPropertyWindow_Base* InItem )
{
	if( !TextCtrl )
		return;

	WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
	WxPropertyWindow* pw = InItem->FindPropertyWindow();

	Equation = TextCtrl->GetValue();

	if( objwin->Objects.Num() == 1 || Equation.Len() )
	{
		// Solve for the equation value and pass that value back to the property

		FLOAT Result;
		Eval( Equation, &Result );
		FString Value = *FString::Printf( TEXT("%f"), Result );

		for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
		{
			ImportText( pw, InItem->Property, *Value, InItem->GetBase((BYTE*)objwin->Objects(i)), objwin->Objects(i) );
		}
	}
}

/**
 * Evaluates a numerical equation.
 *
 * Operators and precedence: 1:+- 2:/% 3:* 4:^ 5:&|
 * Unary: -
 * Types: Numbers (0-9.), Hex ($0-$f)
 * Grouping: ( )
 *
 * @param	Str		String containing the equation
 * @param	pResult	Pointer to storage for the result
 *
 * @return	1 if successful, 0 if equation fails
 */

UBOOL UPropertyInputNumeric::Eval( FString Str, FLOAT* pResult )
{
	FLOAT Result;

	// Check for a matching number of brackets right up front.
	INT Brackets = 0;
	for( INT x = 0 ; x < Str.Len() ; x++ )
	{
		if( Str.Mid(x,1) == TEXT("(") )
		{
			Brackets++;
		}
		if( Str.Mid(x,1) == TEXT(")") )
		{
			Brackets--;
		}
	}

	if( Brackets != 0 )
	{
		debugf(TEXT("Expression Error : Mismatched brackets"));
		Result = 0;
	}

	else
	{
		if( !SubEval( &Str, &Result, 0 ) )
		{
			debugf(TEXT("Expression Error : Error in expression"));
			Result = 0;
		}
	}

	*pResult = Result;

	return 1;
}

UBOOL UPropertyInputNumeric::SubEval( FString* pStr, FLOAT* pResult, INT Prec )
{
	FString c;
	FLOAT V, W, N;

	V = W = N = 0.0f;

	c = GrabChar(pStr);

	if( (c >= TEXT("0") && c <= TEXT("9")) || c == TEXT(".") )	// Number
	{
		V = 0;
		while(c >= TEXT("0") && c <= TEXT("9"))
		{
			V = V * 10 + Val(c);
			c = GrabChar(pStr);
		}

		if( c == TEXT(".") )
		{
			N = 0.1f;
			c = GrabChar(pStr);

			while(c >= TEXT("0") && c <= TEXT("9"))
			{
				V = V + N * Val(c);
				N = N / 10.0f;
				c = GrabChar(pStr);
			}
		}
	}
	else if( c == TEXT("("))									// Opening parenthesis
	{
		if( !SubEval(pStr, &V, 0) )
		{
			return 0;
		}
		c = GrabChar(pStr);
	}
	else if( c == TEXT("-") )									// Negation
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}
		V = -V;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("+"))									// Positive
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}
		c = GrabChar(pStr);
	}
	else if( c == TEXT("@") )									// Square root
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}

		if( V < 0 )
		{
			debugf(TEXT("Expression Error : Can't take square root of negative number"));
			return 0;
		}
		else
		{
			V = appSqrt(V);
		}

		c = GrabChar(pStr);
	}
	else														// Error
	{
		debugf(TEXT("Expression Error : No value recognized"));
		return 0;
	}
PrecLoop:
	if( c == TEXT("") )
	{
		*pResult = V;
		return 1;
	}
	else if( c == TEXT(")") )
	{
		*pStr = TEXT(")") + *pStr;
		*pResult = V;
		return 1;
	}
	else if( c == TEXT("+") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 2) )
			{
				V = V + W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
		}
	}
	else if( c == TEXT("-") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 2) )
			{
				V = V - W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
		}
	}
	else if( c == TEXT("/") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					debugf(TEXT("Expression Error : Division by zero isn't allowed"));
					return 0;
				}
				else
				{
					V = V / W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
		}
	}
	else if( c == TEXT("%") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					debugf(TEXT("Expression Error : Modulo zero isn't allowed"));
					return 0;
				}
				else
				{
					V = (INT)V % (INT)W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
		}
	}
	else if( c == TEXT("*") )
	{
		if( Prec > 3 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 4) )
			{
				V = V * W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
		}
	}
	else
	{
		debugf(TEXT("Expression Error : Unrecognized Operator"));
	}

	*pResult = V;
	return 1;
}

FString UPropertyInputNumeric::GrabChar( FString* pStr )
{
	FString GrabChar;
	if( pStr->Len() )
	{
		do
		{		
			GrabChar = pStr->Left(1);
			*pStr = pStr->Mid(1);
		} while( GrabChar == TEXT(" ") );
	}
	else
	{
		GrabChar = TEXT("");
	}

	return GrabChar;
}

/**
 * Converts a string to it's numeric equivalent, ignoring whitespace.
 * "123  45" - becomes 12,345
 *
 * @param	Value	The string to convert
 *
 * @return	The converted value
 */

FLOAT UPropertyInputNumeric::Val( FString Value )
{
	FLOAT RetValue = 0;

	for( INT x = 0 ; x < Value.Len() ; x++ )
	{
		FString Char = Value.Mid(x, 1);

		if( Char >= TEXT("0") && Char <= TEXT("9") )
		{
			RetValue *= 10;
			RetValue += appAtoi( *Char );
		}
		else 
		{
			if( Char != TEXT(" ") )
			{
				break;
			}
		}
	}

	return RetValue;
}

/*-----------------------------------------------------------------------------
	UPropertyInputCombo
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputCombo::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;

	if(	((InItem->Property->IsA(UByteProperty::StaticClass()) && Cast<UByteProperty>(InItem->Property)->Enum)
			||	(InItem->Property->IsA(UNameProperty::StaticClass()) && InItem->Property->GetFName() == NAME_InitialState)
			||  (Cast<UClassProperty>(InItem->Property)))
			&&	( ( InArrayIdx == -1 && InItem->Property->ArrayDim == 1 ) || ( InArrayIdx > -1 && InItem->Property->ArrayDim > 0 ) ) )
		return 1;

	return 0;
}

void UPropertyInputCombo::CreateControls( WxPropertyWindow_Base* InItem, UClass* InBaseClass, wxRect InRC, const TCHAR* InValue )
{
	long style;

	if (InItem->Property->IsA(UByteProperty::StaticClass()))
		style = wxCB_READONLY;
	else
		style = wxCB_READONLY|wxCB_SORT;

	ComboBox = new wxComboBox( InItem, ID_PROPWINDOW_COMBOBOX, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, style );

	if( InItem->Property->IsA(UByteProperty::StaticClass()) )
	{
		for( INT i=0; i<Cast<UByteProperty>(InItem->Property)->Enum->Names.Num(); i++ )
			ComboBox->Append( *Cast<UByteProperty>(InItem->Property)->Enum->Names(i) );
	}
	else if( InItem->Property->IsA(UNameProperty::StaticClass()) && InItem->Property->GetFName() == NAME_InitialState )
	{
		TArray<FName> States;

		if( InBaseClass )
			for( TFieldIterator<UState> StateIt( InBaseClass ) ; StateIt ; ++StateIt )
				if( StateIt->StateFlags & STATE_Editable )
					States.AddUniqueItem( StateIt->GetFName() );

		ComboBox->Append( *FName(NAME_None) );
		for( INT i = 0 ; i < States.Num() ; i++ )
			ComboBox->Append( *States(i) );
	}
	else if( Cast<UClassProperty>(InItem->Property) )			
	{
		UClassProperty* ClassProp = CastChecked<UClassProperty>(InItem->Property);
		ComboBox->Append( TEXT("None") );
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if( It->IsChildOf( ClassProp->MetaClass ) && !(It->ClassFlags&CLASS_Hidden) && !(It->ClassFlags&CLASS_Abstract) && !(It->ClassFlags&CLASS_HideDropDown) )
			{
				ComboBox->Append( It->GetName() );
			}
		}
	}

	// This is hacky and terrible, but I don't see a way around it.

	FString Wk = InValue;
	if( Wk.Left( 6 ) == TEXT("Class'") )
	{
		Wk = Wk.Mid( 6, Wk.Len()-7 );
		Wk = Wk.Right( Wk.Len() - Wk.InStr( ".", 1 ) - 1 );
	}

	INT Index = InItem->GetReadAddress( InItem, InItem->bSingleSelectOnly ) ? ComboBox->FindString( *Wk ) : 0;
	ComboBox->Select( Index >= 0 ? Index : 0 );

	UPropertyInputProxy::CreateControls( InItem, InBaseClass, InRC, InValue );

	ParentResized( InItem, InRC );
}

void UPropertyInputCombo::DeleteControls()
{
	UPropertyInputProxy::DeleteControls();

	if( ComboBox )
		ComboBox->Destroy();

	ComboBox = NULL;
}

void UPropertyInputCombo::SendToObjects( WxPropertyWindow_Base* InItem )
{
	if( !ComboBox )
		return;

	FString Value = ComboBox->GetStringSelection();

	WxPropertyWindow_Objects* objwin = InItem->FindObjectItemParent();
	for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
	{
		ImportText( InItem->FindPropertyWindow(), InItem->Property, *Value, InItem->GetBase((BYTE*)objwin->Objects(i)), objwin->Objects(i) );
	}
}

void UPropertyInputCombo::ParentResized( WxPropertyWindow_Base* InItem, wxRect InRC )
{
	UPropertyInputProxy::ParentResized( InItem, InRC );

	if( ComboBox )
		ComboBox->SetSize( InRC.x-4,-3,InRC.GetWidth()+6-(Buttons.Num()*PB_ButtonSz),InRC.GetHeight()+4 );
}

void UPropertyInputCombo::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Insert:
		{
			FArray* Addr = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				BYTE* Dest = (BYTE*)Addr->GetData() + InItem->ArrayIndex*InItem->Property->GetSize();

				Addr->Insert( InItem->ArrayIndex, 1, InItem->Property->GetSize() );	
				appMemzero( Dest, InItem->Property->GetSize() );

				// Use struct defaults.
				UStructProperty* StructProperty	= Cast<UStructProperty>( InItem->Property );
				if( StructProperty )
				{
					check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == InItem->Property->GetSize() );
					appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
				}

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}

		}
		break;

		case PB_Delete:
		{
			FArray* Addr  = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				Addr->Remove( InItem->ArrayIndex, 1, InItem->Property->GetSize() );

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}
		}
		break;
	}
}

/*-----------------------------------------------------------------------------
	UPropertyInputEditInline
-----------------------------------------------------------------------------*/

UBOOL UPropertyInputEditInline::Supports( WxPropertyWindow_Base* InItem, INT InArrayIdx )
{
	if( InItem->Property->PropertyFlags&CPF_EditConst )
		return 0;
	
	return InItem->bEditInline;
}

void UPropertyInputEditInline::ButtonClick( WxPropertyWindow_Base* InItem, ePropButton InButton )
{
	switch( InButton )
	{
		case PB_Insert:
		{
			FArray* Addr = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				BYTE* Dest = (BYTE*)Addr->GetData() + InItem->ArrayIndex*InItem->Property->GetSize();

				Addr->Insert( InItem->ArrayIndex, 1, InItem->Property->GetSize() );	
				appMemzero( Dest, InItem->Property->GetSize() );

				// Use struct defaults.
				UStructProperty* StructProperty	= Cast<UStructProperty>( InItem->Property );
				if( StructProperty )
				{
					check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == InItem->Property->GetSize() );
					appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
				}

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}

		}
		break;

		case PB_Delete:
		{
			FArray* Addr  = (FArray*)InItem->ParentItem->GetReadAddress( InItem->ParentItem, InItem->ParentItem->bSingleSelectOnly );
			if( Addr )
			{
				Addr->Remove( InItem->ArrayIndex, 1, InItem->Property->GetSize() );

				WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

				SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
				SaveParentItem->Collapse();
				SaveParentItem->Expand();
				SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
			}
		}
		break;

		case PB_NewObject:
		{
			UObjectProperty* objprop = Cast<UObjectProperty>( InItem->Property );

			wxMenu* Menu = new wxMenu();

			// Create a popup menu containing the classes that can be created for this property

			INT id = ID_PROP_CLASSBASE_START;
			InItem->ClassMap.Empty();
			for( TObjectIterator<UClass> It ; It ; ++It )
				if( It->IsChildOf(objprop->PropertyClass) && (It->ClassFlags&CLASS_EditInlineNew) && !(It->ClassFlags & CLASS_Hidden) && !(It->ClassFlags&CLASS_Abstract) )
				{
					InItem->ClassMap.Set( id, *It );
					Menu->Append( id, It->GetName() );
					id++;
				}

			// Show the menu to the user, sending messages to InItem

			FTrackPopupMenu tpm( InItem, Menu );
			tpm.Show();
		}
		break;

		case PB_Clear:
		{
			WxPropertyWindow_Base* SaveParentItem = InItem->ParentItem;

			SaveParentItem->FindPropertyWindow()->NotifyPreChange(NULL);
			SendTextToObjects( InItem, TEXT("None") );
			InItem->Collapse();
			SaveParentItem->FindPropertyWindow()->NotifyPostChange(NULL);
		}
		break;
	}
}

/*-----------------------------------------------------------------------------
	Class implementations.
	
	Done here to control the order they are iterated later on.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UPropertyDrawArrayHeader);
IMPLEMENT_CLASS(UPropertyDrawProxy);
IMPLEMENT_CLASS(UPropertyDrawNumeric);
IMPLEMENT_CLASS(UPropertyDrawColor);
IMPLEMENT_CLASS(UPropertyDrawRotation);
IMPLEMENT_CLASS(UPropertyDrawRotationHeader);
IMPLEMENT_CLASS(UPropertyDrawBool);

IMPLEMENT_CLASS(UPropertyInputProxy);
IMPLEMENT_CLASS(UPropertyInputArray);
IMPLEMENT_CLASS(UPropertyInputColor);
IMPLEMENT_CLASS(UPropertyInputBool);
IMPLEMENT_CLASS(UPropertyInputArrayItem);
IMPLEMENT_CLASS(UPropertyInputNumeric);
IMPLEMENT_CLASS(UPropertyInputText);
IMPLEMENT_CLASS(UPropertyInputRotation);
IMPLEMENT_CLASS(UPropertyInputEditInline);
IMPLEMENT_CLASS(UPropertyInputCombo);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
