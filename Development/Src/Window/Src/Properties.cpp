/*=============================================================================
	Properties.cpp: property editing class implementation.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#include "Engine.h"
#include "Window.h"

#include "..\..\Launch\Resources\resource.h"

// QSort comparator.
IMPLEMENT_COMPARE_POINTER( FTreeItem, Properties, {	return appStricmp( *A->GetCaption(), *B->GetCaption() ); } )

/*-----------------------------------------------------------------------------
	WPropertiesBase.
-----------------------------------------------------------------------------*/

FTreeItem* WPropertiesBase::GetListItem( INT i )
{
	FTreeItem* Result = (FTreeItem*)List.GetItemData(i);
	check(Result);
	return Result;
}

/*-----------------------------------------------------------------------------
	FTreeItem
-----------------------------------------------------------------------------*/

HBRUSH FTreeItem::GetBackgroundBrush( UBOOL Selected )
{
	return Selected ? hBrushCurrent : hBrushGrey197;//hBrushOffWhite;
}

COLORREF FTreeItem::GetTextColor( UBOOL Selected )
{
	//return Selected ? RGB(255,255,255) : RGB(0,0,0);
	return RGB(0,0,0);
}

void FTreeItem::Serialize( FArchive& Ar )
{
	//!!Super::Serialize( Ar );
	for( INT i=0; i<Children.Num(); i++ )
		Children(i)->Serialize( Ar );
}

INT FTreeItem::OnSetCursor()
{
	return 0;
}

void FTreeItem::EmptyChildren()
{
	for( INT i=0; i<Children.Num(); i++ )
		delete Children(i);
	Children.Empty();
}

FRect FTreeItem::GetRect()
{
	return OwnerProperties->List.GetItemRect(OwnerProperties->List.FindItemChecked(this));
}

void FTreeItem::Redraw()
{
	InvalidateRect( OwnerProperties->List, GetRect(), 0 );
	UpdateWindow( OwnerProperties->List );
	if( Parent!=OwnerProperties->GetRoot() )
		Parent->Redraw();
}

void FTreeItem::OnItemSetFocus()
{
	if( Parent && Parent!=OwnerProperties->GetRoot() )
		Parent->OnItemSetFocus();
}

void FTreeItem::OnItemKillFocus( UBOOL Abort )
{
	for( INT i=0; i<Buttons.Num(); i++ )
		delete Buttons(i);
	Buttons.Empty();
	ButtonWidth = 0;
	Redraw();
	if( Parent && Parent!=OwnerProperties->GetRoot() )
		Parent->OnItemKillFocus( Abort );
}

void FTreeItem::AddButton( const TCHAR* Text, FDelegate Action )
{
	FRect Rect=GetRect();
	Rect.Max.X -= ButtonWidth;
	SIZE Size;
	HDC hDC = GetDC(*OwnerProperties);
	GetTextExtentPoint32X( hDC, Text, appStrlen(Text), &Size ); 
	ReleaseDC(*OwnerProperties,hDC);
	INT Width = Size.cx + 2;
	WCoolButton* Button = new WCoolButton(&OwnerProperties->List,0,Action);
	Buttons.AddItem( Button );
	Button->OpenWindow( 1, Rect.Max.X-Width, Rect.Min.Y, Width, Rect.Max.Y-Rect.Min.Y, Text );
	ButtonWidth += Width;
}

void FTreeItem::OnItemLeftMouseDown( FPoint P )
{
	if( Expandable )
		ToggleExpansion();
}

void FTreeItem::OnItemRightMouseDown( FPoint P )
{
}

INT FTreeItem::GetIndent()
{
	INT Result=0;
	for( FTreeItem* Test=Parent; Test!=OwnerProperties->GetRoot(); Test=Test->Parent )
		Result++;
	return Result;
}

INT FTreeItem::GetUnitIndentPixels()
{
	return OwnerProperties->ShowTreeLines ? 12 : 8;
}

INT FTreeItem::GetIndentPixels( UBOOL Text )
{
	return GetUnitIndentPixels()*GetIndent() + 
		(
			Text
			?
				OwnerProperties->ShowTreeLines 
				?
					20
				:
					16 
			:
				0
		);
}

FRect FTreeItem::GetExpanderRect()
{
	return FRect( GetIndentPixels(0) + 4, 4, GetIndentPixels(0) + 13, 13 );
}

UBOOL FTreeItem::GetSelected()
{
	return Selected;
}

void FTreeItem::SetSelected( UBOOL InSelected )
{
	Selected = InSelected;
}

void FTreeItem::DrawTreeLines( HDC hDC, FRect Rect, UBOOL bTopLevelHeader )
{
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, RGB(0,0,0) );

	if( OwnerProperties->ShowTreeLines )
	{
		FTreeItem* Prev = this;
		for( INT i=GetIndent(); i>=0; i-- )
		{
			UBOOL bHeaderItem = (Prev->GetHeight() == 20);

			check(Prev->Parent);
			UBOOL FromAbove = (Prev->Parent->Children.Last()!=Prev || Prev->Parent->Children.Last()==this) && (i!=0 || Prev->Parent->Children(0)!=this);
			UBOOL ToBelow   = (Prev->Parent->Children.Last()!=Prev);
			FPoint P( Rect.Min.X + GetUnitIndentPixels()*i, Rect.Min.Y );
			if( !bHeaderItem )
			{
				if( FromAbove || ToBelow )
					FillRect( hDC, FRect(P+FPoint(8,FromAbove?0:8),P+FPoint(9,ToBelow?GetHeight():8)), hBrushDark );
				if( i==GetIndent() )
					FillRect( hDC, FRect(P+FPoint(8,8),P+FPoint(20,9)), hBrushDark );
			}
			Prev = Prev->Parent;
		}
	}
	if( Expandable )
	{
		FRect R = GetExpanderRect() + GetRect().Min;

		if( !bTopLevelHeader )
		{
			FillRect( hDC, R, hBrushDark );
			R = R + FRect(1,1,-1,-1);
			FillRect( hDC, R, GetBackgroundBrush(0) );
			R = R + FRect(1,1,-1,-1);
		}

		INT XMid = R.Min.X + (R.Width()/2), YMid = R.Min.Y + (R.Height()/2);
		FillRect( hDC, FRect( R.Min.X, YMid, R.Max.X, YMid+1 ), hBrushDark );
		if( !Expanded )
			FillRect( hDC, FRect( XMid, R.Min.Y, XMid+1, R.Max.Y), hBrushDark );
	}
}

void FTreeItem::Collapse()
{
	OwnerProperties->SetItemFocus( 0 );
	INT Index = OwnerProperties->List.FindItemChecked( this );
	INT Count = OwnerProperties->List.GetCount();
	while( Index+1<Count )
	{
		FTreeItem* NextItem = OwnerProperties->GetListItem(Index+1);
		FTreeItem* Check;
		for( Check=NextItem->Parent; Check && Check!=this; Check=Check->Parent );
		if( !Check )
			break;
		NextItem->Expanded = 0;
		OwnerProperties->List.DeleteString( Index+1 );
		Count--;
	}
	Expanded=0;
	OwnerProperties->ResizeList();
}

void FTreeItem::Expand()
{
	if( Sorted )
	{
		Sort<USE_COMPARE_POINTER(FTreeItem,Properties)>( &Children(0), Children.Num() );
	}
	if( this==OwnerProperties->GetRoot() )
	{
		for( INT i=0; i<Children.Num(); i++ )
			OwnerProperties->List.AddItem( Children(i) );
	}
	else
	{
		for( INT i=Children.Num()-1; i>=0; i-- )
			OwnerProperties->List.InsertItemAfter( this, Children(i) );
	}
	OwnerProperties->SetItemFocus( 0 );
	OwnerProperties->ResizeList();
	Expanded=1;
}

void FTreeItem::ToggleExpansion()
{
	if( Expandable )
	{
		OwnerProperties->List.SetRedraw( 0 );
		if( Expanded )
			Collapse();
		else
			Expand();
		OwnerProperties->List.SetRedraw( 1 );
		UpdateWindow( OwnerProperties->List );
	}
	OwnerProperties->SetItemFocus( 1 );
}

void FTreeItem::OnItemDoubleClick()
{
	//ToggleExpansion();
}

BYTE* FTreeItem::GetBase( BYTE* Base )
{
	return Parent ? Parent->GetContents(Base) : NULL;
}

BYTE* FTreeItem::GetContents( BYTE* Base )
{
	return GetBase(Base);
}

BYTE* FTreeItem::GetReadAddress( class FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	return Parent ? Parent->GetReadAddress(Child,RequireSingleSelection) : NULL;
}

void FTreeItem::NotifyPreChange()
{
	if( Parent )
		Parent->NotifyPreChange();
}

void FTreeItem::NotifyPostChange()
{
	if( Parent )
		Parent->NotifyPostChange();
}

void FTreeItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	if( Parent )
		Parent->SetProperty( Child, Value );
}

void FTreeItem::GetStates( TArray<FName>& States )
{
	if( Parent ) Parent->GetStates( States );
}
UBOOL FTreeItem::AcceptFlags( QWORD InFlags )
{
	return Parent ? Parent->AcceptFlags( InFlags ) : 0;
}

void FTreeItem::OnItemHelp()
{
}

void FTreeItem::SetFocusToItem()
{
}

void FTreeItem::SetValue( const TCHAR* Value )
{
}

void FTreeItem::SnoopChar( WWindow* Src, INT Char )
{
	if( Char==13 && Expandable )
		ToggleExpansion();
}

void FTreeItem::SnoopKeyDown( WWindow* Src, INT Char )
{
	if( Char==VK_UP || Char==VK_DOWN )
		PostMessageX( OwnerProperties->List, WM_KEYDOWN, Char, 0 );
	if( Char==9 )
		PostMessageX( OwnerProperties->List, WM_KEYDOWN, (GetKeyState(16)&0x8000)?VK_UP:VK_DOWN, 0 );
}

UObject* FTreeItem::GetParentObject()
{
	return Parent->GetParentObject();
}

/*-----------------------------------------------------------------------------
	FPropertyItem
-----------------------------------------------------------------------------*/

void FPropertyItem::Serialize( FArchive& Ar )
{
	FTreeItem::Serialize( Ar );
	Ar << Property << Name;
}

QWORD FPropertyItem::GetId() const
{
	return Name.GetIndex() + ((QWORD)1<<32);
}

FString FPropertyItem::GetCaption() const
{
	return *Name;
}

INT FPropertyItem::OnSetCursor()
{
	FPoint P = OwnerProperties->GetCursorPos() - GetRect().Min;
	if( Abs(P.X-OwnerProperties->GetDividerWidth())<=2 )
	{ 
		SetCursor(LoadCursorIdX(hInstanceWindow,IDC_SplitWE));
		return 1;
	}
	return 0;
}

void FPropertyItem::OnItemLeftMouseDown( FPoint P )
{
	P = OwnerProperties->GetCursorPos() - GetRect().Min;
	if( Abs(P.X-OwnerProperties->GetDividerWidth())<=2 )
	{
		OwnerProperties->BeginSplitterDrag();
		throw TEXT("NoRoute");
	}
	else FTreeItem::OnItemLeftMouseDown( P );
}

BYTE* FPropertyItem::GetBase( BYTE* Base )
{
	return Parent->GetContents(Base) + _Offset;
}

BYTE* FPropertyItem::GetContents( BYTE* Base )
{
	Base = GetBase(Base);
	UArrayProperty* ArrayProperty;
	if( (ArrayProperty=Cast<UArrayProperty>(Property))!=NULL )
		Base = (BYTE*)((FArray*)Base)->GetData();
	return Base;
}

FString FPropertyItem::GetPropertyText()
{
	FString	Result;
	Property->ExportText( 0, Result, GetReadAddress(this)-Property->Offset, GetReadAddress(this)-Property->Offset, NULL, PPF_Localized );
	return Result;
}

void FPropertyItem::SetValue( const TCHAR* Value )
{
	SetProperty( this, Value );
	ReceiveFromControl();
	Redraw();
}

void FPropertyItem::Draw( HDC hDC )
{
	FRect Rect = GetRect();
	FString	Str;

	// Draw background.
	FillRect( hDC, Rect, hBrushGrey160 ); 

	// Draw left background.
	FRect LeftRect=Rect;
	LeftRect.Max.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, LeftRect+FRect(0,1-GetSelected(),-1,0), GetBackgroundBrush(GetSelected()) );
	LeftRect.Min.X += GetIndentPixels(1);

	// Draw tree lines.
	DrawTreeLines( hDC, Rect, 0 );

	// Setup text.
	SetBkMode( hDC, TRANSPARENT );

	// Draw left text.
	if( ArrayIndex==-1 )
		Str = *Name;
	else
	{
		Str = FString::Printf(TEXT("[%i]"),ArrayIndex);
	}
	SetTextColor( hDC, GetTextColor(GetSelected()) );
	DrawTextExX( hDC, (LPWSTR)*Str, Str.Len(), LeftRect + FRect(0,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	if( GetSelected() )	// cheap bold effect
		DrawTextExX( hDC, (LPWSTR)*Str, Str.Len(), LeftRect + FRect(1,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );

	// Draw right background.
	FRect RightRect = Rect;
	RightRect.Min.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, RightRect+FRect(0,1,0,0), GetBackgroundBrush(0) );

	// Draw right text.
	RightRect.Max.X -= ButtonWidth;
	BYTE* ReadValue = GetReadAddress( this );
	SetTextColor( hDC, GetTextColor(0) );
	if( (Property->ArrayDim!=1 && ArrayIndex==-1) || Cast<UArrayProperty>(Property) )
	{
		// Array expander.
		Str = TEXT("...");
		DrawTextExX( hDC, (LPWSTR)*Str, Str.Len(), RightRect + FRect(4,0,0,1), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	}
	else if( ReadValue && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct->GetFName()==NAME_Color )
	{
		// Color.
		FillRect( hDC, RightRect + FRect(4,4,-4,-4), hBrushBlack ); 
		DWORD D = (*(DWORD*)ReadValue); //!! 
		DWORD TempColor = ((D&0xff)<<16) + (D&0xff00) + ((D&0xff0000)>>16);
		HBRUSH hBrush = CreateSolidBrush(COLORREF(TempColor));
		FillRect( hDC, RightRect + FRect(5,5,-5,-5), hBrush ); 
		DeleteObject( hBrush );
	}
	else if( ReadValue )
	{
		// Text.
		Str = GetPropertyText();
		if(  Cast<UFloatProperty>(Property) || Cast<UIntProperty>(Property) || 
			(Cast<UByteProperty>(Property) && !Cast<UByteProperty>(Property)->Enum) )
		{
			if( !Equation.Len() )
				Equation = Str;
			Str = Equation;
			DrawTextExX( hDC, (LPWSTR)*Str, Str.Len(), RightRect + FRect(4,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
		}
		else
			DrawTextExX( hDC, (LPWSTR)*Str, Str.Len(), RightRect + FRect(4,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	}

}

INT FPropertyItem::GetHeight()
{
	return 16;
}

void FPropertyItem::SetFocusToItem()
{
	if( EditControl )
		SetFocus( *EditControl );
	else if( TrackControl )
		SetFocus( *TrackControl );
	else if( ComboControl )
		SetFocus( *ComboControl );
}

void FPropertyItem::OnItemDoubleClick()
{
	Advance();
	FTreeItem::OnItemDoubleClick();
}

void FPropertyItem::OnItemSetFocus()
{
	check(!EditControl);
	check(!TrackControl);
	FTreeItem::OnItemSetFocus();
	if
	(	(Property->ArrayDim==1 || ArrayIndex!=-1)
	&&	!(Property->PropertyFlags & CPF_EditConst) )
	{
		if( Property->IsA(UByteProperty::StaticClass()) && !Cast<UByteProperty>(Property)->Enum )
		{
			// Slider.
			FRect Rect = GetRect();
			Rect.Min.X = 28+OwnerProperties->GetDividerWidth();
			Rect.Min.Y--;
			TrackControl = new WTrackBar( &OwnerProperties->List );
			TrackControl->ThumbTrackDelegate    = FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnTrackBarThumbTrack ) );
			TrackControl->ThumbPositionDelegate = FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnTrackBarThumbPosition ) );
			TrackControl->OpenWindow( 0 );
			TrackControl->SetTicFreq( 32 );
			TrackControl->SetRange( 0, 255 );
			TrackControl->MoveWindow( Rect, 0 );
		}
		if
		(	(Property->IsA(UBoolProperty::StaticClass()))
		||	(Property->IsA(UByteProperty::StaticClass()) && Cast<UByteProperty>(Property)->Enum)
		||	(Property->IsA(UNameProperty::StaticClass()) && Name==NAME_InitialState)
		||  (Cast<UClassProperty>(Property)) )
		{
			// Combo box.
			FRect Rect = GetRect() + FRect(0,0,-1,-1);
			Rect.Min.X = OwnerProperties->GetDividerWidth();

			HolderControl = new WLabel( &OwnerProperties->List );
			HolderControl->Snoop = this;
			HolderControl->OpenWindow( 0 );
			FRect HolderRect = Rect.Right(20) + FRect(0,0,0,1);
			HolderControl->MoveWindow( HolderRect, 0 );

			Rect = Rect + FRect(-2,-6,-2,0);

			ComboControl = new WComboBox( HolderControl );
			ComboControl->Snoop = this;
			ComboControl->SelectionEndOkDelegate     = FDelegate(this, static_cast<TDelegate>( &FPropertyItem::ComboSelectionEndOk ));
			ComboControl->SelectionEndCancelDelegate = FDelegate(this, static_cast<TDelegate>( &FPropertyItem::ComboSelectionEndCancel ));
			ComboControl->OpenWindow( 0, Cast<UClassProperty>(Property) ? 1 : 0 );
			ComboControl->MoveWindow( Rect-HolderRect.Min, 1 );

			if( Property->IsA(UBoolProperty::StaticClass()) )
			{
				ComboControl->AddString( GFalse );
				ComboControl->AddString( GTrue );
			}
			else if( Property->IsA(UByteProperty::StaticClass()) )
			{
				for( INT i=0; i<Cast<UByteProperty>(Property)->Enum->Names.Num(); i++ )
					ComboControl->AddString( *Cast<UByteProperty>(Property)->Enum->Names(i) );
			}
			else if( Property->IsA(UNameProperty::StaticClass()) && Name==NAME_InitialState )
			{
				TArray<FName> States;
				GetStates( States );
				ComboControl->AddString( *FName(NAME_None) );
				for( INT i=0; i<States.Num(); i++ )
					ComboControl->AddString( *States(i) );
			}
			else if( Cast<UClassProperty>(Property) )			
			{
				UClassProperty* ClassProp = CastChecked<UClassProperty>(Property);
				ComboControl->AddString( TEXT("None") );
				for( TObjectIterator<UClass> It; It; ++It )
					if( It->IsChildOf(ClassProp->MetaClass) && !(It->ClassFlags&CLASS_Hidden) && !(It->ClassFlags&CLASS_Abstract) )
						ComboControl->AddString( It->GetName() );
				goto SkipTheRest;
			}
		}
		if( Property->IsA(UArrayProperty::StaticClass()) )
		{
			if( SingleSelect && !(Property->PropertyFlags&CPF_EditConstArray) )
			{
				AddButton( *LocalizeGeneral("AddButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnArrayAdd ) ) );
				AddButton( *LocalizeGeneral("EmptyButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnArrayEmpty ) ) );
			}
		}
		if( Cast<UArrayProperty>(Property->GetOuter()) )
		{
			if( SingleSelect && !(Cast<UArrayProperty>(Property->GetOuter())->PropertyFlags&CPF_EditConstArray) )
			{
				AddButton( *LocalizeGeneral("InsertButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnArrayInsert ) ) );
				AddButton( *LocalizeGeneral("DeleteButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnArrayDelete ) ) );
			}
		}
		if( Property->IsA(UStructProperty::StaticClass()) && appStricmp(Cast<UStructProperty>(Property)->Struct->GetName(),TEXT("Color"))==0 )
		{
			// Color.
			AddButton( *LocalizeGeneral("BrowseButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnChooseColorButton ) ) );
			AddButton( *LocalizeGeneral("PickButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnPickColorButton ) ) );

		}
		if( Property->IsA(UObjectProperty::StaticClass()) )
		{
			if( EdFindable )
			{
				if( !(Property->PropertyFlags & CPF_NoClear) )
					AddButton( *LocalizeGeneral("ClearButton", TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnClearButton ) ) );
				AddButton( *LocalizeGeneral("FindButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnFindButton ) ) );
				AddButton( *LocalizeGeneral("UseButton",   TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnUseCurrentButton ) ) );
			}
			else
			{
				// Class.
				if( !(Property->PropertyFlags & CPF_NoClear) )
					AddButton( *LocalizeGeneral("BrowseButton",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnBrowseButton ) ) );
				if( !EditInline || EditInlineUse )
					AddButton( *LocalizeGeneral("UseButton",   TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnUseCurrentButton ) ) );
				if( !(Property->PropertyFlags & CPF_NoClear) )
					AddButton( *LocalizeGeneral("ClearButton", TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FPropertyItem::OnClearButton ) ) );
			}
		}
		if
		(	(Property->IsA(UFloatProperty ::StaticClass()))
		||	(Property->IsA(UIntProperty   ::StaticClass()))
		||	(Property->IsA(UNameProperty  ::StaticClass()) && Name!=NAME_InitialState)
		||	(Property->IsA(UStrProperty   ::StaticClass()))
		||	(Property->IsA(UObjectProperty::StaticClass()))
		||	(Property->IsA(UByteProperty  ::StaticClass()) && Cast<UByteProperty>(Property)->Enum==NULL) )
		{
			// Edit control.
			FRect Rect = GetRect();
			Rect.Min.X = 1+OwnerProperties->GetDividerWidth();
			Rect.Min.Y--;
			if( Property->IsA(UByteProperty::StaticClass()) )
				Rect.Max.X = Rect.Min.X + 28;
			else
				Rect.Max.X -= ButtonWidth;
			EditControl = new WEdit( &OwnerProperties->List );
			EditControl->Snoop = this;
			EditControl->OpenWindow( 0, 0, 0 );
			EditControl->MoveWindow( Rect+FRect(0,1,0,1), 0 );
		}
		SkipTheRest:
		ReceiveFromControl();
		Redraw();
		if( EditControl )
			EditControl->Show(1);
		if( TrackControl )
			TrackControl->Show(1);
		if( ComboControl )
			ComboControl->Show(1);
		if( HolderControl )
			HolderControl->Show(1);
		SetFocusToItem();
	}
}

void FPropertyItem::OnItemKillFocus( UBOOL Abort )
{
	if( !Abort )
		SendToControl();
	if( EditControl )
		delete EditControl;
	EditControl=NULL;
	if( TrackControl )
		delete TrackControl;
	TrackControl=NULL;
	if( ComboControl )
		delete ComboControl;
	ComboControl=NULL;
	if( HolderControl )
		delete HolderControl;
	HolderControl=NULL;
	FTreeItem::OnItemKillFocus( Abort );
}

void FPropertyItem::Collapse()
{
	FTreeItem::Collapse();
	EmptyChildren();
}

// FControlSnoop interface.
void FPropertyItem::SnoopChar( WWindow* Src, INT Char )
{
	if( Char==13 )
	{
		if( EditInline )
		{
			FTreeItem::SnoopKeyDown( Src, 9 );
			return;
		}			
		Advance();
	}
	else if( Char==27 )
		ReceiveFromControl();
	FTreeItem::SnoopChar( Src, Char );
}
void FPropertyItem::ComboSelectionEndCancel()
{
	ReceiveFromControl();
}

void FPropertyItem::ComboSelectionEndOk()
{
	ComboChanged=1;
	SendToControl();
	ReceiveFromControl();
	Redraw();
}

void FPropertyItem::OnTrackBarThumbTrack()
{
	if( TrackControl && EditControl )
	{
		TCHAR Tmp[256];
		appSprintf( Tmp, TEXT("%i"), TrackControl->GetPos() );
		EditControl->SetText( Tmp );
		EditControl->SetModify( 1 );
		UpdateWindow( *EditControl );
	}
}

void FPropertyItem::OnTrackBarThumbPosition()
{
	OnTrackBarThumbTrack();
	SendToControl();
	if( EditControl )
	{
		SetFocus( *EditControl );
		EditControl->SetSelection( 0, EditControl->GetText().Len() );
		Redraw();
	}
}

void FPropertyItem::OnChooseColorButton()
{
	union _FColor {
		struct{BYTE B,G,R,A;}; 
		DWORD DWColor;
	} ColorValue;
	DWORD* ReadValue = (DWORD*)GetReadAddress( this );
	ColorValue.DWColor = ReadValue ? *ReadValue : 0;

	CHOOSECOLORA cc;
	static COLORREF acrCustClr[16];
	appMemzero( &cc, sizeof(cc) );
	cc.lStructSize  = sizeof(cc);
	cc.hwndOwner    = OwnerProperties->List;
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult    = RGB(ColorValue.R, ColorValue.G, ColorValue.B);
	cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
	if( ChooseColorA(&cc)==TRUE )
	{
		TCHAR Str[256];
		appSprintf( Str, TEXT("(R=%i,G=%i,B=%i)"), GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult) );
		SetValue( Str );
		InvalidateRect( OwnerProperties->List, NULL, 0 );
		UpdateWindow( OwnerProperties->List );
	}
	Collapse();
	Redraw();
}

void FPropertyItem::OnArrayAdd()
{
	// Only works with single selection.
	BYTE* Addr = GetReadAddress( this, 1 );
	if( Addr )
	{
		UArrayProperty* Array = CastChecked<UArrayProperty>( Property );
		NotifyPreChange();
		Collapse();
	
		INT ArrayIndex = ((FArray*)Addr)->AddZeroed( Array->Inner->ElementSize, 1 );

		// Use struct defaults.
		UStructProperty* StructProperty = Cast<UStructProperty>(Array->Inner);
		if( StructProperty )
		{
			check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == Array->Inner->ElementSize );
			BYTE* Dest = (BYTE*)((FArray*)Addr)->GetData() + ArrayIndex * Array->Inner->ElementSize;
			appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
		}
	
		Expand();
		NotifyPostChange();

		// Activate the newly added item
		OwnerProperties->List.SetCurrent( OwnerProperties->List.FindItem(this) + ((FArray*)Addr)->Num() );
		if( Children.Num() && Children(Children.Num()-1)->Expandable )
			Children(Children.Num()-1)->Expand();
		OwnerProperties->SetItemFocus(1);
	}
}

void FPropertyItem::OnArrayEmpty()
{
	// Only works with single selection.
	BYTE* Addr = GetReadAddress( this, 1 );
	if( Addr )
	{
		UArrayProperty* Array = CastChecked<UArrayProperty>( Property );
		NotifyPreChange();
		Collapse();
		((FArray*)Addr)->Empty( Array->Inner->ElementSize );
		Expand();
		NotifyPostChange();
	}
}

void FPropertyItem::OnArrayInsert()
{
	FArray* Addr  = (FArray*)GetReadAddress((FPropertyItem*)Parent, 1);
	if( Addr )
	{
		BYTE* Dest = (BYTE*)Addr->GetData() + ArrayIndex*Property->GetSize();

		Addr->Insert( ArrayIndex, 1, Property->GetSize() );	
		appMemzero( Dest, Property->GetSize() );

		// Use struct defaults.
		UStructProperty* StructProperty	= Cast<UStructProperty>( Property );
		if( StructProperty )
		{
			check( Align(StructProperty->Struct->Defaults.Num(),StructProperty->Struct->MinAlignment) == Property->GetSize() );
			appMemcpy( Dest, &StructProperty->Struct->Defaults(0), StructProperty->Struct->Defaults.Num() );
		}

		FTreeItem* P = Parent;
		P->NotifyPreChange();
		P->Collapse();
		P->Expand();
		P->NotifyPostChange();
	}
}

void FPropertyItem::OnArrayDelete()
{
	FArray* Addr  = (FArray*)GetReadAddress((FPropertyItem*)Parent, 1);
	if( Addr )
	{
		// 'this' gets deleted by Collapse() so we have to remember these values
		FTreeItem* LocalParent		= Parent;
		UProperty* LocalProperty	= Property;
		INT LocalArrayIndex			= ArrayIndex;
		LocalParent->NotifyPreChange();
		LocalParent->Collapse();
		Addr->Remove( LocalArrayIndex, 1, LocalProperty->GetSize() );
		LocalParent->Expand();
		LocalParent->NotifyPostChange();
	}
}

void FPropertyItem::OnBrowseButton()
{
	UObjectProperty* ReferenceProperty = CastChecked<UObjectProperty>(Property);
	TCHAR Temp[256];
	appSprintf( Temp, TEXT("BROWSECLASS CLASS=%s"), ReferenceProperty->PropertyClass ? ReferenceProperty->PropertyClass->GetName() : TEXT("Texture") );
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, Temp );
	Redraw();
}

void FPropertyItem::OnUseCurrentButton()
{
	UObjectProperty* ObjectProperty = CastChecked<UObjectProperty>(Property);
	check(ObjectProperty->PropertyClass);
	SetValue(*GSelectionTools.GetTop(ObjectProperty->PropertyClass)->GetPathName());
	Redraw();
	if( EditInline )
	{
		Collapse();
		Expand();
		if(Children.Num())
			Children(0)->Expand();
	}
}

void FPropertyItem::OnPickColorButton()
{
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, TEXT("USECOLOR") );
	Collapse();
	Redraw();
}

void FPropertyItem::OnFindButton()
{
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, TEXT("FINDACTOR") );
	Collapse();
	Redraw();
}

void FPropertyItem::OnClearButton()
{
	SetValue( TEXT("None") );
	Redraw();
	if( EditInline )
	{
		Collapse();
		Expand();
	}
}

void FPropertyItem::Advance()
{
	if( ComboControl && ComboControl->GetCurrent()>=0 )
	{
		ComboControl->SetCurrent( (ComboControl->GetCurrent()+1) % ComboControl->GetCount() );
		ComboChanged=1;
	}
	SendToControl();
	ReceiveFromControl();
	Redraw();
}

void FPropertyItem::SendToControl()
{
	if( EditControl )
	{
		if( EditControl->GetModify() )
		{
			SetValue( *EditControl->GetText() );
			if( EditInline )
				Collapse();
		}
	}
	else if( ComboControl )
	{
		if( ComboChanged )
			SetValue( *ComboControl->GetString(ComboControl->GetCurrent()) );
		ComboChanged=0;
	}
}

void FPropertyItem::ReceiveFromControl()
{
	ComboChanged=0;
	BYTE* ReadValue = GetReadAddress( this );
	if( EditControl )
	{
		FString	Str;
		if( ReadValue )
			Str = GetPropertyText();

		if(  Cast<UFloatProperty>(Property) || Cast<UIntProperty>(Property) || 
			(Cast<UByteProperty>(Property) && !Cast<UByteProperty>(Property)->Enum) )
		{
			EditControl->SetText( *Equation );
			EditControl->SetSelection( 0, Equation.Len() );
		}
		else
		{
			EditControl->SetText( *Str );
			EditControl->SetSelection( 0, Str.Len() );
		}
	}
	if( TrackControl )
	{
		if( ReadValue )
			TrackControl->SetPos( *(BYTE*)ReadValue );
	}
	if( ComboControl )
	{
		UBoolProperty* BoolProperty;
		if( (BoolProperty=Cast<UBoolProperty>(Property))!=NULL )
		{
			ComboControl->SetCurrent( ReadValue ? (*(BITFIELD*)ReadValue&BoolProperty->BitMask)!=0 : -1 );
		}
		else if( Property->IsA(UByteProperty::StaticClass()) )
		{
			ComboControl->SetCurrent( ReadValue ? *(BYTE*)ReadValue : -1 );
		}
		else if( Property->IsA(UNameProperty::StaticClass()) && Name==NAME_InitialState )
		{
			INT Index=ReadValue ? ComboControl->FindString( **(FName*)ReadValue ) : 0;
			ComboControl->SetCurrent( Index>=0 ? Index : 0 );
		}
		ComboChanged=0;
	}
}

/*-----------------------------------------------------------------------------
	FHeaderItem
-----------------------------------------------------------------------------*/

void FHeaderItem::Draw( HDC hDC )
{
	FRect Rect = GetRect();
	UBOOL bTopLevelHeader = !GetIndent();

	// Draw background.
	FillRect( hDC, Rect, GetBackgroundBrush(0) ); 
	Rect = Rect + FRect(1+GetIndentPixels(0),1,-1,-1);
	FillRect( hDC, Rect, bTopLevelHeader ? hBrushBlack : hBrushGrey160 ); 
	Rect = Rect + FRect(1,1,-1,-1);
	FillRect( hDC, Rect, bTopLevelHeader ? hBrushGrey180 : hBrushGrey );
	Rect = Rect + FRect(1,1,-1,-1);

	DrawTreeLines( hDC, GetRect(), bTopLevelHeader );

	// Prep text.
	SetTextColor( hDC, GetTextColor(GetSelected()) );
	SetBkMode( hDC, TRANSPARENT );
	
	// Draw name.
	FString C = GetCaption();

	Rect = Rect + FRect(18,0,-18,0);
	if( !appStricmp( UObject::GetLanguage(), TEXT("jpt") ) )
		TextOutW( hDC, Rect.Min.X, Rect.Min.Y,  const_cast<TCHAR*>(*C), C.Len() );
	else
		DrawTextExX( hDC, const_cast<TCHAR*>(*C), C.Len(), Rect, DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
}

INT FHeaderItem::GetHeight()
{
	return 20;
}

/*-----------------------------------------------------------------------------
	FCategoryItem
-----------------------------------------------------------------------------*/

void FCategoryItem::Serialize( FArchive& Ar )
{
	FHeaderItem::Serialize( Ar );
	Ar << Category << BaseClass;
}

QWORD FCategoryItem::GetId() const
{
	return Category.GetIndex() + ((QWORD)2<<32);
}

FString FCategoryItem::GetCaption() const
{
	return *Category;
}

void FCategoryItem::Expand()
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
		if( It->Category==Category && AcceptFlags(It->PropertyFlags) )
			Children.AddItem( new FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
	FTreeItem::Expand();
}

void FCategoryItem::Collapse()
{
	FTreeItem::Collapse();
	EmptyChildren();
}

/*-----------------------------------------------------------------------------
	FNewObjectItem
-----------------------------------------------------------------------------*/

// Item to create a new object for inline editing.

IMPLEMENT_COMPARE_POINTER( UObject, Properties, { return appStrcmp( B->GetName(), A->GetName() ); } )

class FNewObjectItem : public FHeaderItem
{
public:
	// Variables.
	UObjectProperty*	Property;
	WComboBox*			ComboControl;
	WLabel*				HolderControl;
	FString				NewClass;

	static FString		LastUsedClass;

	// Constructors.
	FNewObjectItem()
	{}
	FNewObjectItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UObjectProperty* InProperty )
	:	FHeaderItem( InOwnerProperties, InParent, 0 )
	,	Property( InProperty )
	,	ComboControl	( NULL )
	,	HolderControl	( NULL )
	{
		UBOOL FoundLastUsed = 0;
		UBOOL FoundBaseClass = 0;

		// If the property class is abstract or not marked to be constructed with the New button, find a subclass which can be.
		if( (Property->PropertyClass->ClassFlags&CLASS_Abstract) || !(Property->PropertyClass->ClassFlags&CLASS_EditInlineNew) )
		{
			TArray<UObject*> Temp;
			for( TObjectIterator<UClass> It; It; ++It )
				if( It->IsChildOf(Property->PropertyClass) && (It->ClassFlags&CLASS_EditInlineNew) && !(It->ClassFlags & CLASS_Hidden) && !(It->ClassFlags&CLASS_Abstract) )
					Temp.AddItem(*It);

			Sort<USE_COMPARE_POINTER(UObject,Properties)>( &Temp(0), Temp.Num() );		
			for( INT i=0;i<Temp.Num();i++ )
			{
				if( !FoundBaseClass )
				{
					NewClass = Temp(i)->GetName();
					FoundBaseClass = 1;
				}
				if( LastUsedClass == Temp(i)->GetName() )
					FoundLastUsed = 1;
			}
		}
		if( FoundLastUsed )
			NewClass = LastUsedClass;
		else
		if( !FoundBaseClass )
			NewClass = Property->PropertyClass->GetName();
	}

	void OnItemSetFocus();
	void OnItemKillFocus( UBOOL Abort );
	void ComboSelectionEndCancel();
	void ComboSelectionEndOk();
	void OnNew();
	void Draw( HDC hDC );
	FString GetCaption() const;
	QWORD GetId() const;
};

FString	FNewObjectItem::LastUsedClass;


// Remember the last class new'd
void FNewObjectItem::OnItemSetFocus()
{
	FHeaderItem::OnItemSetFocus();

	// New button.
	AddButton( *LocalizeGeneral("New",TEXT("Window")), FDelegate(this, static_cast<TDelegate>( &FNewObjectItem::OnNew ) ) );

	// Combo box.
	FRect Rect = GetRect() + FRect(0,0,-ButtonWidth-1,-1);
	Rect.Min.X = OwnerProperties->GetDividerWidth();

	HolderControl = new WLabel( &OwnerProperties->List );
	HolderControl->Snoop = this;
	HolderControl->OpenWindow( 0 );
	FRect HolderRect = Rect.Right(20) + FRect(0,0,0,1);
	HolderControl->MoveWindow( HolderRect, 0 );

	Rect = Rect + FRect(-2,-6,-2,0);

	ComboControl = new WComboBox( HolderControl );
	ComboControl->Snoop = this;
	ComboControl->SelectionEndOkDelegate     = FDelegate(this, static_cast<TDelegate>( &FNewObjectItem::ComboSelectionEndOk ) );
	ComboControl->SelectionEndCancelDelegate = FDelegate(this, static_cast<TDelegate>( &FNewObjectItem::ComboSelectionEndCancel ) );
	ComboControl->OpenWindow( 0, 1 );
	ComboControl->MoveWindow( Rect-HolderRect.Min, 1 );

	for( TObjectIterator<UClass> It; It; ++It )
		if( It->IsChildOf(Property->PropertyClass) && (It->ClassFlags&CLASS_EditInlineNew) && !(It->ClassFlags & CLASS_Hidden) && !(It->ClassFlags&CLASS_Abstract) )
			ComboControl->AddString( It->GetName() );	

	INT Index = ComboControl->FindString( *NewClass );
	ComboControl->SetCurrent( Index>=0 ? Index : 0 );

	ComboControl->Show(1);
	HolderControl->Show(1);
	SetFocus( *ComboControl );
}

void FNewObjectItem::OnItemKillFocus( UBOOL Abort )
{
	if( !Abort )
		NewClass = ComboControl->GetString(ComboControl->GetCurrent());
	if( ComboControl )
		delete ComboControl;
	ComboControl=NULL;
	if( HolderControl )
		delete HolderControl;
	HolderControl=NULL;
	FHeaderItem::OnItemKillFocus( Abort );
}

void FNewObjectItem::ComboSelectionEndCancel()
{
	INT Index=ComboControl->FindString( *NewClass );
	ComboControl->SetCurrent( Index>=0 ? Index : 0 );
}

void FNewObjectItem::ComboSelectionEndOk()
{
	NewClass = ComboControl->GetString(ComboControl->GetCurrent());
	LastUsedClass = NewClass;
	Redraw();
}

void FNewObjectItem::OnNew()
{
	UObject* ParentObject = GetParentObject();
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( Parent, *FString::Printf(TEXT("NEWOBJECT CLASS=%s PARENT=%s PARENTCLASS=%s"), *NewClass, *ParentObject->GetPathName(), ParentObject->GetClass()->GetName() ) );
	FTreeItem* P = Parent;
	P->Collapse();
	P->Expand();
	P->Children(0)->Expand();
}

void FNewObjectItem::Draw( HDC hDC )
{
	FRect Rect = GetRect();

	// Draw background.
	FillRect( hDC, Rect, hBrushGrey160 ); 

	// Draw left background.
	FRect LeftRect=Rect;
	LeftRect.Max.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, LeftRect+FRect(0,1-GetSelected(),-1,0), GetBackgroundBrush(GetSelected()) );
	LeftRect.Min.X += GetIndentPixels(1);

	// Draw tree lines.
	DrawTreeLines( hDC, Rect, 0 );

	// Setup text.
	SetBkMode( hDC, TRANSPARENT );

	// Draw left text.
	FString Str = LocalizeGeneral("New",TEXT("Window"));
	SetTextColor( hDC, GetTextColor(GetSelected()) );
	DrawTextExX( hDC, (TCHAR *)(*Str), Str.Len(), LeftRect + FRect(0,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );

	// Draw right background.
	FRect RightRect = Rect;
	RightRect.Min.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, RightRect+FRect(0,1,0,0), GetBackgroundBrush(0) );

	// Draw right text.
	RightRect.Max.X -= ButtonWidth;
	SetTextColor( hDC, GetTextColor(0) );
	DrawTextExX( hDC, (TCHAR *)(*NewClass), NewClass.Len(), RightRect + FRect(4,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );

}

FString FNewObjectItem::GetCaption() const
{ 
	return TEXT("");
}

QWORD FNewObjectItem::GetId() const
{
	return (INT)this + ((QWORD)4<<32);
}

/*-----------------------------------------------------------------------------
	WProperties
-----------------------------------------------------------------------------*/

void WProperties::Serialize( FArchive& Ar )
{
	WPropertiesBase::Serialize( Ar );
	GetRoot()->Serialize( Ar );
}

void WProperties::DoDestroy()
{
	PropertiesWindows.RemoveItem( this );
	WWindow::DoDestroy();
}

INT WProperties::OnSetCursor()
{
	if( ::IsWindow(List.hWnd) && List.GetCount() )
	{
		FPoint P = GetCursorPos();
		INT Index = List.ItemFromPoint( P );
		return Index>0 ? GetListItem( Index )->OnSetCursor() : 0;
	}
	return 0;
}

void WProperties::OnDestroy()
{
	WWindow::OnDestroy();
	_DeleteWindows.AddItem( this );
}

void WProperties::OpenChildWindow( INT InControlId )
{
	HWND hWndParent = InControlId ? GetDlgItem(OwnerWindow->hWnd,InControlId) : OwnerWindow->hWnd;
	check(hWndParent);
	FRect R;
	::GetClientRect( hWndParent, R );
	PerformCreateWindowEx
	(
		0, NULL, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0, 0, R.Width(), R.Height(),
		hWndParent, NULL, hInstance
	);
	List.OpenWindow( 1, 0, 0, 1 );
	Show(1);
}

void WProperties::OpenChildWindow( HWND hWndParent )
{
	FRect R;
	::GetClientRect( hWndParent, R );
	PerformCreateWindowEx
	(
		0, NULL, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0, 0, R.Width(), R.Height(),
		hWndParent, NULL, hInstance
	);
	List.OpenWindow( 1, 0, 0, 1 );
	Show(1);
}

void WProperties::OpenWindow( HWND hWndParent )
{
	PerformCreateWindowEx
	(
		WS_EX_TOOLWINDOW,
		*GetRoot()->GetCaption(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		256+64+32,
		512,
		hWndParent ? hWndParent : OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	List.OpenWindow( 1, 1, 0, 1 );
}

void WProperties::OnActivate( UBOOL Active )
{
	if( Active==1 )
	{
		SetFocus( List );
		if( !FocusItem )
			SetItemFocus( 1 );
	}
	else
	{
		SetItemFocus( 0 );
	}
}

void WProperties::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	WWindow::OnSize( Flags, NewX, NewY );
	if( List.hWnd )
	{
		SetItemFocus( 0 );
		InvalidateRect( List, NULL, FALSE );
		ResizeList();
		SetItemFocus( 1 );
	}
}

void WProperties::OnPaint()
{
	if( GetUpdateRect( *this, NULL, 0 ) )
	{
		PAINTSTRUCT PS;
		HDC   hDC      = BeginPaint( *this, &PS );
		FRect Rect     = GetClientRect();
		FRect ListRect = List.GetClientRect();
		Rect.Min.Y     = ListRect.Max.Y;
		FillRect( hDC, Rect, (HBRUSH)(COLOR_BTNFACE+1) ); 
		EndPaint( *this, &PS );
	}
}

void WProperties::OnListDoubleClick()
{
	if( FocusItem )
		FocusItem->OnItemDoubleClick();
}

void WProperties::OnListSelectionChange()
{
	SetItemFocus( 1 );
}

void WProperties::SnoopLeftMouseDown( WWindow* Src, FPoint P )
{
	if( Src==&List )
	{
		INT Index = List.ItemFromPoint( P );
		if( Index>=0 )
		{
			List.SetCurrent( Index, 0 );
			FTreeItem* Item = GetListItem( Index );
			Item->OnItemLeftMouseDown( P-Item->GetRect().Min );
		}
	}
}

void WProperties::SnoopRightMouseDown( WWindow* Src, FPoint P )
{
	if( Src==&List )
	{
		INT Index = List.ItemFromPoint( P );
		if( Index>=0 )
		{
			List.SetCurrent( Index, 0 );
			FTreeItem* Item = GetListItem( Index );
			Item->OnItemRightMouseDown( P-Item->GetRect().Min );
		}
	}
}

void WProperties::SnoopChar( WWindow* Src, INT Char )
{
	if( FocusItem )
		FocusItem->SnoopChar( Src, Char );
}

void WProperties::SnoopKeyDown( WWindow* Src, INT Char )
{
	if( Char==9 )
		PostMessageX( List, WM_KEYDOWN, (GetKeyState(16)&0x8000)?VK_UP:VK_DOWN, 0 );
	WPropertiesBase::SnoopKeyDown( Src, Char );
}

INT WProperties::GetDividerWidth()
{
	return DividerWidth;
}

void WProperties::BeginSplitterDrag()
{
	SetItemFocus( NULL );
	DragInterceptor            = new WDragInterceptor( this, FPoint(0,INDEX_NONE), GetClientRect(), FPoint(3,3) );	
	DragInterceptor->DragPos   = FPoint(GetDividerWidth(),GetCursorPos().Y);
	DragInterceptor->DragClamp = FRect(GetClientRect().Inner(FPoint(64,0)));
	DragInterceptor->OpenWindow();
}

void WProperties::OnFinishSplitterDrag( WDragInterceptor* Drag, UBOOL Success )
{
	if( Success )
	{
		DividerWidth += Drag->DragPos.X - Drag->DragStart.X;
		if( PersistentName!=NAME_None )
			GConfig->SetInt( TEXT("WindowPositions"), *(FString(*PersistentName)+TEXT(".Split")), DividerWidth, GEditorIni );
		InvalidateRect( *this, NULL, 1 );
		InvalidateRect( List, NULL, 1 );
		UpdateWindow( *this );
	}
	DragInterceptor = NULL;
}

void WProperties::SetValue( const TCHAR* Value )
{
	if( FocusItem )
		FocusItem->SetValue( Value );
}

void WProperties::SetItemFocus( UBOOL FocusCurrent )
{
	if( FocusItem )
		FocusItem->OnItemKillFocus( 0 );
	FocusItem = NULL;
	if( FocusCurrent && List.GetCount()>0 )
		FocusItem = GetListItem( List.GetCurrent() );
	if( FocusItem )
		FocusItem->OnItemSetFocus();
}

void WProperties::ResizeList()
{
	FRect ClientRect = GetClientRect();
	FRect R(0,0,0,4);//!!why?
	for( INT i=List.GetCount()-1; i>=0; i-- )
		R.Max.Y += List.GetItemHeight( i );
	AdjustWindowRect( R, GetWindowLongX(List,GWL_STYLE), 0 );
	List.MoveWindow( FRect(0,0,ClientRect.Width(),Min(ClientRect.Height(),R.Height())), 1 );
}

void WProperties::ForceRefresh()
{
	if( !bAllowForceRefresh ) return;

	// Disable editing.
	SetItemFocus( 0 );

	// Remember which items were expanded.
	if( List.GetCount() )
	{
		Remembered.Empty();
		SavedTop     = GetListItem(List.GetTop())->GetId();
		SavedCurrent = List.GetSelected(List.GetCurrent()) ? GetListItem(List.GetCurrent())->GetId() : 0;
		for( INT i=0; i<List.GetCount(); i++ )
		{
			FTreeItem* Item = GetListItem(i);
			if( Item->Expanded )
			{
				Remembered.AddItem( Item->GetId() );
			}
		}
	}

	// Empty it and add root items.
	List.Empty();
	GetRoot()->EmptyChildren();
	GetRoot()->Expanded=0;
	GetRoot()->Expand();

	// Restore expansion state of the items.
	INT CurrentIndex=-1, TopIndex=-1;
	for( INT i=0; i<List.GetCount(); i++ )
	{
		FTreeItem* Item = GetListItem(i);
		QWORD      Id   = Item->GetId();
		if( Item->Expandable && !Item->Expanded )
		{
			for( INT j=0; j<Remembered.Num(); j++ )
				if( Remembered(j)==Id )
				{
					Item->Expand();
					break;
				}
		}
		if( Id==SavedTop     ) TopIndex     = i;
		if( Id==SavedCurrent ) CurrentIndex = i;
	}

	// Adjust list size.
	ResizeList();

	// Set indices.
	if( TopIndex>=0 ) List.SetTop( TopIndex );
	if( CurrentIndex>=0 ) List.SetCurrent( CurrentIndex, 1 );

}

void WProperties::ExpandAll()
{
	for( INT i=0; i<List.GetCount(); i++ )
	{
		FTreeItem* Item = GetListItem(i);

		if( Item->Expandable && !Item->Expanded )
			Item->Expand();
	}
}

/*-----------------------------------------------------------------------------
	FPropertyItemBase
-----------------------------------------------------------------------------*/

void FPropertyItemBase::Serialize( FArchive& Ar )
{
	FHeaderItem::Serialize( Ar );
	Ar << BaseClass;
}

UBOOL FPropertyItemBase::AcceptFlags( QWORD InFlags )
{
	return (InFlags&FlagMask)==FlagMask;
}

void FPropertyItemBase::GetStates( TArray<FName>& States )
{
	if( BaseClass )
		for( TFieldIterator<UState> StateIt(BaseClass); StateIt; ++StateIt )
			if( StateIt->StateFlags & STATE_Editable )
				States.AddUniqueItem( StateIt->GetFName() );
}

void FPropertyItemBase::Collapse()
{
	FTreeItem::Collapse();
	EmptyChildren();
}

FString FPropertyItemBase::GetCaption() const
{
	return Caption;
}

QWORD FPropertyItemBase::GetId() const
{
	return (QWORD)BaseClass + (QWORD)4;
}

/*-----------------------------------------------------------------------------
	FObjectsItem
-----------------------------------------------------------------------------*/

void FObjectsItem::Serialize( FArchive& Ar )
{
	FPropertyItemBase::Serialize( Ar );
	Ar << _Objects;
}

BYTE* FObjectsItem::GetBase( BYTE* Base )
{
	return Base;
}

BYTE* FObjectsItem::GetReadAddress( FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	if( !_Objects.Num() )
		return NULL;
	if( RequireSingleSelection && _Objects.Num() > 1 )
		return NULL;

	// If this item is the child of an array, return NULL if there is a different number
	// of items in the array in different objects, when multi-selecting.
	if( Cast<UArrayProperty>(Child->Property->GetOuter()) )
	{
		INT Num0 = ((FArray*)Child->Parent->GetBase((BYTE*)_Objects(0)))->Num();
		for( INT i=1; i<_Objects.Num(); i++ )
			if( Num0 != ((FArray*)Child->Parent->GetBase((BYTE*)_Objects(i)))->Num() )
				return NULL;
	}

	BYTE* Base0 = Child->GetBase((BYTE*)_Objects(0));

	// If the item is an array itself, return NULL if there are a different number of
	// items in the array in different objects, when multi-selecting.
	if( Cast<UArrayProperty>(Child->Property) )
	{
		INT Num0 = ((FArray*)Child->GetBase((BYTE*)_Objects(0)))->Num();
		for( INT i=1; i<_Objects.Num(); i++ )
			if( Num0 != ((FArray*)Child->GetBase((BYTE*)_Objects(i)))->Num() )
				return NULL;
	}
	else
	{
		for( INT i=1; i<_Objects.Num(); i++ )
			if( !Child->Property->Identical( Base0, Child->GetBase((BYTE*)_Objects(i)) ) )
				return NULL;
	}
	return Base0;
}

void FObjectsItem::NotifyPreChange()
{
	for( INT i=0; i<_Objects.Num(); i++ )
		_Objects(i)->PreEditChange();
	if( NotifyParent )
		Parent->NotifyPreChange();
}

void FObjectsItem::NotifyPostChange()
{
	for( INT i=0; i<_Objects.Num(); i++ )
		_Objects(i)->PostEditChange(NULL);
	if( NotifyParent )
		Parent->NotifyPostChange();
}

void FObjectsItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyPreChange( OwnerProperties, NULL );
	if( NotifyParent )
		Parent->NotifyPreChange();
	for( INT i=0; i<_Objects.Num(); i++ )
	{
		_Objects(i)->PreEditChange();
		if(  Cast<UFloatProperty>(Child->Property) || Cast<UIntProperty>(Child->Property) || 
			(Cast<UByteProperty>(Child->Property) && !Cast<UByteProperty>(Child->Property)->Enum) )
		{
			FLOAT Result;
			FString Wk = Child->Equation = Value;
			if( Eval( Wk, &Result ) )
				Child->Property->ImportText( *FString::Printf(TEXT("%f"),Result), Child->GetBase((BYTE*)_Objects(i)), PPF_Localized, _Objects(i) );
			else
			{
				Child->Equation = Value;
				Child->Property->ImportText( Value, Child->GetBase((BYTE*)_Objects(i)), PPF_Localized, _Objects(i) );
			}
		}
		else
			Child->Property->ImportText( Value, Child->GetBase((BYTE*)_Objects(i)), PPF_Localized, _Objects(i) );
		_Objects(i)->PostEditChange(NULL);
	}
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyPostChange( OwnerProperties, NULL );
	if( NotifyParent )
		Parent->NotifyPostChange();
}

// Evaluate a numerical expression.
// Returns 1 if ok, 0 if error.
// Sets Result, or 0.0 if error.
//
// Operators and precedence: 1:+- 2:/% 3:* 4:^ 5:&|
// Unary: -
// Types: Numbers (0-9.), Hex ($0-$f)
// Grouping: ( )
//
UBOOL FObjectsItem::Eval( FString Str, FLOAT* pResult )
{
	FLOAT Result;

	// Check for a matching number of brackets right up front.
	INT Brackets = 0;
	for( INT x = 0 ; x < Str.Len() ; x++ )
	{
		if( Str.Mid(x,1) == TEXT("(") )	Brackets++;
		if( Str.Mid(x,1) == TEXT(")") )	Brackets--;
	}
	if( Brackets != 0 )
	{
		debugf(TEXT("Expression Error : Mismatched brackets"));
		Result = 0;
	}
	else
		if( !SubEval( &Str, &Result, 0 ) )
		{
			debugf(TEXT("Expression Error : Error in expression"));
			Result = 0;
		}

	*pResult = Result;

	return 1;
}

UBOOL FObjectsItem::SubEval( FString* pStr, FLOAT* pResult, INT Prec )
{
	FString c;
	FLOAT V, W, N;

	V = W = N = 0.0f;

	c = GrabChar(pStr);

	if((c >= TEXT("0") && c <= TEXT("9")) || c == TEXT(".")) // Number
	{
		V = 0;
		while(c >= TEXT("0") && c <= TEXT("9"))
		{
			V = V * 10 + Val(c);
			c = GrabChar(pStr);
		}
		if(c == TEXT("."))
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
	else if( c == TEXT("(")) // Opening parenthesis
	{
		if( !SubEval(pStr, &V, 0) )
			return 0;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("-") ) // Negation
	{
		if( !SubEval(pStr, &V, 1000) )
			return 0;
		V = -V;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("+")) // Positive
	{
		if( !SubEval(pStr, &V, 1000) )
			return 0;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("@") ) // Square root
	{
		if( !SubEval(pStr, &V, 1000) )
			return 0;
		if( V < 0 )
		{
			debugf(TEXT("Expression Error : Can't take square root of negative number"));
			return 0;
		}
		else
			V = appSqrt(V);
		c = GrabChar(pStr);
	}
	else // Error
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
			if( SubEval(pStr, &W, 2) )
			{
				V = V + W;
				c = GrabChar(pStr);
				goto PrecLoop;
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
			if( SubEval(pStr, &W, 2) )
			{
				V = V - W;
				c = GrabChar(pStr);
				goto PrecLoop;
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
	else if( c == TEXT("%") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else 
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
	else if( c == TEXT("*") )
	{
		if( Prec > 3 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
			if( SubEval(pStr, &W, 4) )
			{
				V = V * W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
	}
	else
	{
		debugf(TEXT("Expression Error : Unrecognized Operator"));
	}

	*pResult = V;
	return 1;
}

void FObjectsItem::Expand()
{
	UBOOL OldSorted = Sorted;
	if( ByCategory )
	{
		if( BaseClass && (BaseClass->ClassFlags&CLASS_CollapseCategories) )
		{
			// Sort by category, but don't show the category headers.
			Sorted = 0;
			TArray<FName> Categories;
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
				if( BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE )
					Categories.AddUniqueItem( It->Category );

			for( INT i=0; i<Categories.Num(); i++ )
				for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
					if( It->Category==Categories(i) && AcceptFlags(It->PropertyFlags) )
						Children.AddItem( new FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
		}
		else
		{
			// Expand to show categories.
			TArray<FName> Categories;
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
				if( AcceptFlags( It->PropertyFlags ) && BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE )
					Categories.AddUniqueItem( It->Category );
			for( INT i=0; i<Categories.Num(); i++ )
				Children.AddItem( new FCategoryItem(OwnerProperties,this,BaseClass,Categories(i),1) );
		}
	}
	else
	{
		// Expand to show individual items.
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
			if( AcceptFlags(It->PropertyFlags)
			&&	BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE
			&&	It->GetOwnerClass()!=UObject::StaticClass() //hack for ufactory display!!
			)
				Children.AddItem( new FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
	}
	FTreeItem::Expand();
	Sorted = OldSorted;
}

FString FObjectsItem::GrabChar( FString* pStr )
{
	FString GrabChar;
	if( pStr->Len() )
		do {
			GrabChar = pStr->Left(1);
			*pStr = pStr->Mid(1);
		} while( GrabChar == TEXT(" ") );
	else
		GrabChar = TEXT("");

	return GrabChar;
}

// Converts a string to it's numeric equivalent, ignoring whitespace.
// "123  45" - becomes 12,345
FLOAT FObjectsItem::Val( FString Value )
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
			if( Char != TEXT(" ") )
				break;
	}

	return RetValue;
}

FString FObjectsItem::GetCaption() const
{
	if( Caption.Len() )
		return Caption;			
	else if( !BaseClass )
		return LocalizeGeneral("PropNone",TEXT("Window"));
	else if( _Objects.Num()==1 )
		return FString::Printf( *LocalizeGeneral("PropSingle",TEXT("Window")), BaseClass->GetName() );
	else
		return FString::Printf( *LocalizeGeneral("PropMulti",TEXT("Window")), BaseClass->GetName(), _Objects.Num() );

}

void FObjectsItem::SetObjects( UObject** InObjects, INT Count )
{
	// Disable editing, to prevent crash due to edit-in-progress after empty objects list.
	OwnerProperties->SetItemFocus( 0 );

	// Add objects and find lowest common base class.
	_Objects.Empty();
	BaseClass = NULL;
	for( INT i=0; i<Count; i++ )
	{
		if( InObjects[i] )
		{
			check(InObjects[i]->GetClass());
			if( BaseClass==NULL )	
				BaseClass=InObjects[i]->GetClass();
			while( !InObjects[i]->GetClass()->IsChildOf(BaseClass) )
				BaseClass = BaseClass->GetSuperClass();
			_Objects.AddItem( InObjects[i] );
		}
	}

	// Automatically title the window.
	OwnerProperties->SetText( *GetCaption() );

	// Refresh all properties.
	if( Expanded || this==OwnerProperties->GetRoot() )
		OwnerProperties->ForceRefresh();

}

UObject* FObjectsItem::GetParentObject()
{
	return _Objects(0);
}


/*-----------------------------------------------------------------------------
	WObjectProperties
-----------------------------------------------------------------------------*/

FTreeItem* WObjectProperties::GetRoot()
{
	return &Root;
}

void WObjectProperties::Show( UBOOL Show )
{
	WProperties::Show(Show);
	// Prevents the property windows from being hidden behind other windows.
	if( bShow )
		BringWindowToTop( hWnd );
}

/*-----------------------------------------------------------------------------
	FClassItem
-----------------------------------------------------------------------------*/

BYTE* FClassItem::GetBase( BYTE* Base )
{
	return Base;
}

// FTreeItem interface.
BYTE* FClassItem::GetReadAddress( class FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	return Child->GetBase(&BaseClass->Defaults(0));
}

void FClassItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	Child->Property->ImportText( Value, GetReadAddress(Child), PPF_Localized, BaseClass );
}

void FClassItem::Expand()
{
	TArray<FName> Categories;
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
		if( AcceptFlags( It->PropertyFlags ) )
			Categories.AddUniqueItem( It->Category );
	for( INT i=0; i<Categories.Num(); i++ )
		Children.AddItem( new FCategoryItem(OwnerProperties,this,BaseClass,Categories(i),1) );
	FTreeItem::Expand();
}

UObject* FClassItem::GetParentObject()
{
	return BaseClass;
}
/*-----------------------------------------------------------------------------
	WClassProperties
-----------------------------------------------------------------------------*/

FTreeItem* WClassProperties::GetRoot()
{
	return &Root;
}

/*-----------------------------------------------------------------------------
	FPropertyItem.
-----------------------------------------------------------------------------*/

void FPropertyItem::Expand()
{
	UStructProperty* StructProperty;
	UArrayProperty* ArrayProperty;
	UObjectProperty* ObjectProperty;
	if( Property->ArrayDim>1 && ArrayIndex==-1 )
	{
		// Expand array.
		Sorted=0;
		for( INT i=0; i<Property->ArrayDim; i++ )
			Children.AddItem( new FPropertyItem( OwnerProperties, this, Property, Name, i*Property->ElementSize, i, 666 ) );
	}
	else if( (ArrayProperty=Cast<UArrayProperty>(Property))!=NULL )
	{
		// Expand array.
		Sorted=0;
		FArray* Array = (FArray*)GetReadAddress(this);
		if( Array )
			for( INT i=0; i<Array->Num(); i++ )
				Children.AddItem( new FPropertyItem( OwnerProperties, this, ArrayProperty->Inner, Name, i*ArrayProperty->Inner->ElementSize, i, 666 ) );
	}
	else if( (StructProperty=Cast<UStructProperty>(Property))!=NULL )
	{
		// Expand struct.
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(StructProperty->Struct); It; ++It )
			if( AcceptFlags( It->PropertyFlags ) )
				Children.AddItem( new FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
	}
	else if((ObjectProperty=Cast<UObjectProperty>(Property))!=NULL )
	{
		BYTE* ReadValue = GetReadAddress( this );
		UObject* O = *(UObject**)ReadValue;
		if( O )
		{
			FObjectsItem* ObjectsItem = new FObjectsItem( OwnerProperties, this, CPF_Edit, *O->GetFullName(), 1, EditInlineNotify );
			ObjectsItem->SetObjects( &O, 1 );
			Children.AddItem( ObjectsItem );
		}
		else
			Children.AddItem( new FNewObjectItem( OwnerProperties, this, ObjectProperty ) );
	}
	FTreeItem::Expand();
}




/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

