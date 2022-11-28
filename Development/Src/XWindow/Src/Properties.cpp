
#include "XWindowPrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

void InitPropertySubSystem()
{
	GPropertyWindowManager = new UPropertyWindowManager;
	GPropertyWindowManager->AddToRoot();

	GPropertyWindowManager->ArrowDownB.Load( TEXT("DownArrowLarge") );
	GPropertyWindowManager->ArrowRightB.Load( TEXT("RightArrowLarge") );
	GPropertyWindowManager->CheckBoxOnB.Load( TEXT("CheckBox_On") );
	GPropertyWindowManager->CheckBoxOffB.Load( TEXT("CheckBox_Off") );
	GPropertyWindowManager->CheckBoxUnknownB.Load( TEXT("CheckBox_Unknown") );
	GPropertyWindowManager->Prop_AddB.Load( TEXT("Prop_Add") );
	GPropertyWindowManager->Prop_EmptyB.Load( TEXT("Prop_Empty") );
	GPropertyWindowManager->Prop_InsertB.Load( TEXT("Prop_Insert") );
	GPropertyWindowManager->Prop_DeleteB.Load( TEXT("Prop_Delete") );
	GPropertyWindowManager->Prop_BrowseB.Load( TEXT("Prop_Browse") );
	GPropertyWindowManager->Prop_PickB.Load( TEXT("Prop_Pick") );
	GPropertyWindowManager->Prop_ClearB.Load( TEXT("Prop_Clear") );
	GPropertyWindowManager->Prop_FindB.Load( TEXT("Prop_Find") );
	GPropertyWindowManager->Prop_UseB.Load( TEXT("Prop_Use") );
	GPropertyWindowManager->Prop_NewObjectB.Load( TEXT("Prop_NewObject") );
}

/*-----------------------------------------------------------------------------
	WxPropertyWindowToolBar.
-----------------------------------------------------------------------------*/

WxPropertyWindowToolBar::WxPropertyWindowToolBar( wxWindow* InParent, wxWindowID InID )
	: wxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxNO_BORDER | wxTB_3DBUTTONS )
{
	// Bitmaps

	CopyB.Load( TEXT("Copy") );
	LockB.Load( TEXT("Lock") );

	// Set up the ToolBar

	AddSeparator();
	AddCheckTool( ID_LOCK_SELECTIONS, TEXT(""), LockB, LockB, TEXT("Lock Selected Actors?") );
	AddSeparator();
	AddTool( IDM_COPY, TEXT(""), CopyB, TEXT("Copy To Clipboard") );

	Realize();
}

WxPropertyWindowToolBar::~WxPropertyWindowToolBar()
{
}

/*-----------------------------------------------------------------------------
	UPropertyWindowManager
-----------------------------------------------------------------------------*/

UPropertyWindowManager::UPropertyWindowManager()
{
}

UBOOL UPropertyWindowManager::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return 0;
}

void UPropertyWindowManager::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	for( INT p = 0 ; p < PropertyWindows.Num() ; ++p )
	{
		PropertyWindows(p)->Serialize( Ar );
	}
}

void UPropertyWindowManager::Add( WxPropertyWindow* InPropertyWindow )
{
	PropertyWindows.AddItem( InPropertyWindow );
}

void UPropertyWindowManager::Remove( WxPropertyWindow* InPropertyWindow )
{
	PropertyWindows.RemoveItem( InPropertyWindow );
}

IMPLEMENT_CLASS(UPropertyWindowManager);	

/*-----------------------------------------------------------------------------
	WxPropertyWindowFrame
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindowFrame,wxFrame)

BEGIN_EVENT_TABLE( WxPropertyWindowFrame, wxFrame )
	EVT_SIZE( WxPropertyWindowFrame::OnSize )
	EVT_CLOSE( WxPropertyWindowFrame::OnClose )
	EVT_MENU( IDM_COPY, WxPropertyWindowFrame::OnCopy )
	EVT_MENU( ID_LOCK_SELECTIONS, WxPropertyWindowFrame::OnLock )
	EVT_UPDATE_UI( ID_LOCK_SELECTIONS, WxPropertyWindowFrame::UI_Lock )
END_EVENT_TABLE()

WxPropertyWindowFrame::WxPropertyWindowFrame( wxWindow* Parent, wxWindowID id, UBOOL bShowToolBar, FNotifyHook* InNotifyHook )
	: wxFrame( Parent, id, TEXT("Properties"), wxDefaultPosition, wxDefaultSize, (Parent ? wxFRAME_FLOAT_ON_PARENT : wxDIALOG_NO_PARENT ) | wxDEFAULT_FRAME_STYLE | wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR )
{
	bAllowClose = Parent ? FALSE : TRUE;
	PropertyWindow = new WxPropertyWindow( this, InNotifyHook );

	if( bShowToolBar )
	{
		ToolBar = new WxPropertyWindowToolBar( this, -1 );
		SetToolBar( ToolBar );
	}

	bLocked = 0;

	FWindowUtil::LoadPosSize( TEXT("FloatingPropertyWindow"), this, 64,64,350,500 );
}

WxPropertyWindowFrame::~WxPropertyWindowFrame()
{
	FWindowUtil::SavePosSize( TEXT("FloatingPropertyWindow"), this );
}

void WxPropertyWindowFrame::OnClose( wxCloseEvent& In )
{
	if( bAllowClose )
	{
		In.Skip();
	}
	else
	{
		In.Veto();
		Hide();
	}
}

void WxPropertyWindowFrame::OnSize( wxSizeEvent& In )
{
	if( PropertyWindow )
	{
		wxPoint pt = GetClientAreaOrigin();
		wxRect rc = GetClientRect();
		rc.y -= pt.y;

		PropertyWindow->SetSize( rc );
	}
}

void WxPropertyWindowFrame::OnCopy( wxCommandEvent& In )
{
	FStringOutputDevice Ar;

	for( INT o = 0 ; o < PropertyWindow->Root->Objects.Num() ; ++o )
	{
		UObject* obj = PropertyWindow->Root->Objects(o);
		UExporter::ExportToOutputDevice( obj, NULL, Ar, TEXT("T3D"), 0 );
	}

	appClipboardCopy(*Ar);
}

void WxPropertyWindowFrame::OnLock( wxCommandEvent& In )
{
	bLocked = In.IsChecked();
}

void WxPropertyWindowFrame::UI_Lock( wxUpdateUIEvent& In )
{
	In.Check( bLocked == 1 );
}

// Updates the caption of the floating frame based on the objects being edited

void WxPropertyWindowFrame::UpdateTitle()
{
	FString Caption;

	if( !PropertyWindow->Root->BaseClass )
		Caption = LocalizeGeneral("PropNone",TEXT("Window"));
	else if( PropertyWindow->Root->Objects.Num()==1 )
		Caption = FString::Printf( *LocalizeGeneral("PropSingle",TEXT("Window")), PropertyWindow->Root->BaseClass->GetName() );
	else
		Caption = FString::Printf( *LocalizeGeneral("PropMulti",TEXT("Window")), PropertyWindow->Root->BaseClass->GetName(), PropertyWindow->Root->Objects.Num() );

	SetTitle( *Caption );
}

/*-----------------------------------------------------------------------------
	WxPropertyWindow
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow,wxWindow)

BEGIN_EVENT_TABLE( WxPropertyWindow, wxWindow )
	EVT_SIZE( WxPropertyWindow::OnSize )
	EVT_SCROLL( WxPropertyWindow::OnScroll )
	EVT_MOUSEWHEEL( WxPropertyWindow::OnMouseWheel )
END_EVENT_TABLE()

WxPropertyWindow::WxPropertyWindow( wxWindow* InParent, FNotifyHook* InNotifyHook )
	: wxWindow( InParent, -1, wxDefaultPosition, wxDefaultSize, wxCLIP_CHILDREN )
{
	ScrollBar = new wxScrollBar( this, -1, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL );
	Root = new WxPropertyWindow_Objects( this, NULL, NULL, -1, INDEX_NONE );
	SplitterPos = 175;
	LastFocused = NULL;
	NotifyHook = InNotifyHook;
	bDraggingSplitter = 0;
	ThumbPos = 0;
	bShowCategories = 1;

	((UPropertyWindowManager*)GPropertyWindowManager)->Add( this );
}

WxPropertyWindow::~WxPropertyWindow()
{
	((UPropertyWindowManager*)GPropertyWindowManager)->Remove( this );

	Root->RemoveAllObjects();
	Root->Destroy();
}

// Links up the Next/Prev pointers the children (needed for things like keyboard navigation)

void WxPropertyWindow::LinkChildren()
{
	Root->Prev = LinkChildrenForItem( Root );
}

WxPropertyWindow_Base* WxPropertyWindow::LinkChildrenForItem( WxPropertyWindow_Base* InItem )
{
	WxPropertyWindow_Base* Prev = InItem;

	for( INT x = 0 ; x < InItem->ChildItems.Num() ; ++x )
	{
		Prev->Next = InItem->ChildItems(x);
		InItem->ChildItems(x)->Prev = Prev;

		if( InItem->ChildItems(x)->ChildItems.Num() > 0 )
			Prev = LinkChildrenForItem( InItem->ChildItems(x) );
		else
			Prev = InItem->ChildItems(x);
	}

	return Prev;
}

// Moves to the next property item/category

void WxPropertyWindow::MoveToNextItem( WxPropertyWindow_Base* InItem )
{
	if( InItem->Next )
	{
		if( InItem->Next == Root )
			return;

		if( wxDynamicCast( InItem->Next, WxPropertyWindow_Category ) )
			MoveToNextItem( InItem->Next );
		else
			InItem->Next->SetFocus();
	}
}

// Moves to the previous property item/category

void WxPropertyWindow::MoveToPrevItem( WxPropertyWindow_Base* InItem )
{
	if( InItem->Prev )
		if( wxDynamicCast( InItem->Prev, WxPropertyWindow_Category ) )
			MoveToPrevItem( InItem->Prev );
		else
			InItem->Prev->SetFocus();
}

void WxPropertyWindow::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	INT Width = wxSystemSettings::GetMetric( wxSYS_VSCROLL_X );

	Root->SetSize( wxRect( rc.x, rc.y, rc.GetWidth()-Width, rc.GetHeight() ) );
	ScrollBar->SetSize( wxRect( rc.GetWidth()-Width, rc.y, Width, rc.GetHeight() ) );

	PositionChildren();
}

// Updates the scrollbars values

void WxPropertyWindow::UpdateScrollBar()
{
	wxRect rc = GetClientRect();

	ScrollBar->SetScrollbar( ThumbPos, rc.GetHeight(), MaxH, rc.GetHeight() );
}

void WxPropertyWindow::OnScroll( wxScrollEvent& In )
{
	ThumbPos = In.GetPosition();
	PositionChildren();
}

void WxPropertyWindow::OnMouseWheel( wxMouseEvent& In )
{
	ThumbPos -= (In.GetWheelRotation() / In.GetWheelDelta()) * PROP_ItemHeight;
	ThumbPos = Clamp( ThumbPos, 0, MaxH - PROP_ItemHeight );
	PositionChildren();
}

// Positions existing child items so they are in the proper positions, visible/hidden, etc.

void WxPropertyWindow::PositionChildren()
{
	LinkChildren();

	MaxH = PositionChild( Root, 0, -ThumbPos );

	UpdateScrollBar();
}

// Recursive minion of PositionChildren

INT WxPropertyWindow::PositionChild( WxPropertyWindow_Base* InItem, INT InX, INT InY )
{
	wxRect rc = GetClientRect();
	rc.width -= wxSystemSettings::GetMetric( wxSYS_VSCROLL_X );

	INT H = InItem->ChildHeight;

	InX += PROP_Indent;

	if( wxDynamicCast( InItem, WxPropertyWindow_Category ) || wxDynamicCast( InItem, WxPropertyWindow_Objects ) )
		InX -= PROP_Indent;

	// Set position/size of any children this window has

	for( INT x = 0 ; x < InItem->ChildItems.Num() ; ++x )
	{
		UBOOL bShow = IsItemShown( InItem->ChildItems(x) );

		INT WkH = PositionChild( InItem->ChildItems(x), InX, H );
		if( bShow )
			H += WkH;

		InItem->ChildItems(x)->Show( bShow > 0 );
	}

	if( InItem->bExpanded || InItem == Root )
		H += InItem->ChildSpacer;

	// Set pos/size of window

	InItem->IndentX = InX;
	if( wxDynamicCast( InItem, WxPropertyWindow_Category ) || wxDynamicCast( InItem, WxPropertyWindow_Objects ) )
		InItem->SetSize( 0,InY, rc.GetWidth(),H );
	else
		InItem->SetSize( 2,InY, rc.GetWidth()-4,H );

	rc = InItem->GetClientRect();
	InItem->InputProxy->ParentResized( InItem, wxRect( rc.x+SplitterPos,rc.y,rc.width-SplitterPos,PROP_ItemHeight ) );

	return H;
}

// Look up the tree to determine the hide/show status of InItem.

UBOOL WxPropertyWindow::IsItemShown( WxPropertyWindow_Base* InItem )
{
	WxPropertyWindow_Base* Win = wxDynamicCast( InItem->GetParent(), WxPropertyWindow_Base );
	check(Win);

	// If anything above this item is not expanded, that means this item must be hidden

	while( Win != Root )		// Loop until the root object window, at the most
	{
		if( Win->bCanBeExpanded && !Win->bExpanded )
			return 0;

		Win = wxDynamicCast( Win->GetParent(), WxPropertyWindow_Base );
		check(Win);
	}

	return 1;
}

void WxPropertyWindow::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	if( NotifyHook )
		NotifyHook->NotifyPreChange( NULL, PropertyAboutToChange );
}

void WxPropertyWindow::NotifyPostChange(UProperty* PropertyThatChanged)
{
	if( NotifyHook )
		NotifyHook->NotifyPostChange( NULL, PropertyThatChanged );
}

void WxPropertyWindow::SetObject( UObject* InObject, UBOOL InExpandCategories, UBOOL InSorted, UBOOL InShowCategories )
{
	RememberExpandedItems();

	RemoveAllObjects();
	if( InObject )
		Root->AddObject( InObject );

	Finalize( InExpandCategories, InSorted, InShowCategories );
}

void WxPropertyWindow::SetObjectArray( TArray<UObject*>* InObjects, UBOOL InExpandCategories, UBOOL InSorted, UBOOL InShowCategories )
{
	RememberExpandedItems();

	RemoveAllObjects();
	for( INT x = 0 ; x < InObjects->Num() ; ++x )
		Root->AddObject( (*InObjects)(x) );

	Finalize( InExpandCategories, InSorted, InShowCategories );
}

void WxPropertyWindow::GetObjectArray(TArray<UObject*>& Objects)
{
	Objects.Empty();

	Objects = Root->Objects;
}

void WxPropertyWindow::FinalizeValues()
{
	if( LastFocused )
	{
		LastFocused->SetFocus();
		wxSafeYield();
	}
}

void WxPropertyWindow::RemoveAllObjects()
{
	if( LastFocused )
	{
		LastFocused->Refresh();
		LastFocused->InputProxy->SendToObjects( LastFocused );
	}

	Root->RemoveAllObjects();
}

void WxPropertyWindow::Finalize( UBOOL InExpandCategories, UBOOL InSorted, UBOOL InShowCategories )
{
	bShowCategories = InShowCategories;

	Root->bSorted = InSorted;
	Root->Finalize();

	RestoreExpandedItems();

	if( InExpandCategories )
		Root->ExpandCategories();
}

void WxPropertyWindow::Serialize( FArchive& Ar )
{
	Root->Serialize( Ar );
}

void WxPropertyWindow::RestoreExpandedItems()
{
	Root->bExpanded = 0;
	Root->RestoreExpandedItems( &ExpandedItems );
}

void WxPropertyWindow::RememberExpandedItems()
{
	ExpandedItems.Empty();
	Root->bExpanded = 1;
	Root->RememberExpandedItems( &ExpandedItems );
}

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Base
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow_Base,wxWindow)

BEGIN_EVENT_TABLE( WxPropertyWindow_Base, wxWindow )
	EVT_LEFT_DOWN( WxPropertyWindow_Base::OnLeftClick )
	EVT_LEFT_UP( WxPropertyWindow_Base::OnLeftUnclick )
	EVT_LEFT_DCLICK( WxPropertyWindow_Base::OnLeftDoubleClick )
	EVT_SET_FOCUS( WxPropertyWindow_Base::OnSetFocus )
	EVT_COMMAND_RANGE( ID_PROP_CLASSBASE_START, ID_PROP_CLASSBASE_END, wxEVT_COMMAND_MENU_SELECTED, WxPropertyWindow_Base::OnEditInlineNewClass )
	EVT_BUTTON( ID_PROP_PB_ADD, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_EMPTY, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_INSERT, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_DELETE, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_BROWSE, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_PICK, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_CLEAR, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_FIND, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_USE, WxPropertyWindow_Base::OnPropItemButton )
	EVT_BUTTON( ID_PROP_PB_NEWOBJECT, WxPropertyWindow_Base::OnPropItemButton )
	EVT_MOTION( WxPropertyWindow_Base::OnMouseMove )
	EVT_CHAR( WxPropertyWindow_Base::OnChar )
END_EVENT_TABLE()

WxPropertyWindow_Base::WxPropertyWindow_Base( wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx )
		: wxWindow( InParent, -1, wxDefaultPosition, wxDefaultSize )
	,	IndentX( 0 )
	,	Property( InProperty )
	,	PropertyOffset( InPropertyOffset )
	,	ArrayIndex( InArrayIdx )
	,	ParentItem( InParentItem )
	,	bExpanded( 0 )
	,	bCanBeExpanded( 0 )
	,	ChildHeight( PROP_ItemHeight )
	,	ChildSpacer( 0 )
	,	Next( NULL )
	,	Prev( NULL )
	,	PropertyClass( NULL )
	,	RespectEditFlag( 1 )
{
}

void WxPropertyWindow_Base::FinishSetUp( UProperty* InProperty )
{
	BYTE* ReadAddress = GetReadAddress(this,0);

	bEditInlineNotify = (InProperty && (InProperty->PropertyFlags&CPF_EditInlineNotify) && Cast<UObjectProperty>(InProperty));
	bSingleSelectOnly = InProperty && GetReadAddress( this, 1 ) ? 1 : 0;
	bEdFindable = ( InProperty && (InProperty->PropertyFlags&CPF_EdFindable) && Cast<UObjectProperty>(InProperty) && ReadAddress);
	bEditInline = (InProperty && (InProperty->PropertyFlags&CPF_EditInline) && Cast<UObjectProperty>(InProperty) && ReadAddress);
	bEditInlineUse = (InProperty && (InProperty->PropertyFlags&CPF_EditInlineUse) && Cast<UObjectProperty>(InProperty) && ReadAddress);

	// Find and construct proxies for this property

	DrawProxy = NULL;
	InputProxy = NULL;

	if( Property )
	{
		//@todo: Need to find a cleaner solution for guaranteeing the order in which the proxies are tried.
		TArray<UPropertyDrawProxy*> DrawProxies;
		DrawProxies.AddItem( ConstructObject<UPropertyDrawProxy>( UPropertyDrawRotationHeader::StaticClass()) );
		DrawProxies.AddItem( ConstructObject<UPropertyDrawProxy>( UPropertyDrawRotation::StaticClass()		) );
		DrawProxies.AddItem( ConstructObject<UPropertyDrawProxy>( UPropertyDrawNumeric::StaticClass()		) );
		DrawProxies.AddItem( ConstructObject<UPropertyDrawProxy>( UPropertyDrawColor::StaticClass()			) );
		DrawProxies.AddItem( ConstructObject<UPropertyDrawProxy>( UPropertyDrawBool::StaticClass()			) );
		DrawProxies.AddItem( ConstructObject<UPropertyDrawProxy>( UPropertyDrawArrayHeader::StaticClass()	) );

		TArray<UPropertyInputProxy*> InputProxies;
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputArray::StaticClass()		) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputColor::StaticClass()		) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputBool::StaticClass()		) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputArrayItem::StaticClass()	) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputNumeric::StaticClass()	) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputText::StaticClass()		) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputRotation::StaticClass()	) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputEditInline::StaticClass()	) );
		InputProxies.AddItem( ConstructObject<UPropertyInputProxy>( UPropertyInputCombo::StaticClass()		) );

		for( INT i=0; i<DrawProxies.Num(); i++ )
		{
			check(DrawProxies(i));
			if( DrawProxies(i)->Supports( this, ArrayIndex ) )
			{
				DrawProxy = DrawProxies(i);
			}
			else
			{
				DrawProxies(i)->Destroy();
			}
		}

		for( INT i=0; i<InputProxies.Num(); i++ )
		{
			check(InputProxies(i));
			if( InputProxies(i)->Supports( this, ArrayIndex ) )
			{
				InputProxy = InputProxies(i);
			}
			else
			{
				InputProxies(i)->Destroy();
			}
		}
	}
	
	// If we couldn't find a specialized proxies, use the base proxies.

	if( !DrawProxy)		
		DrawProxy	= ConstructObject<UPropertyDrawProxy>( UPropertyDrawProxy::StaticClass() );
	if( !InputProxy)	
		InputProxy	= ConstructObject<UPropertyInputProxy>( UPropertyInputProxy::StaticClass() );

	bSorted = ParentItem ? ParentItem->bSorted : 1;

	SetWindowStyleFlag( wxCLIP_CHILDREN|wxCLIP_SIBLINGS );
}

WxPropertyWindow_Base::~WxPropertyWindow_Base()
{
	DrawProxy->Destroy();
	delete DrawProxy;

	InputProxy->Destroy();
	delete InputProxy;

	WxPropertyWindow* pw = FindPropertyWindow();

	if( pw && pw->LastFocused == this )
		pw->LastFocused = NULL;
}

BYTE* WxPropertyWindow_Base::GetBase( BYTE* Base )
{
	return ParentItem ? ParentItem->GetContents( Base ) : NULL;
}

BYTE* WxPropertyWindow_Base::GetContents( BYTE* Base )
{
	return GetBase( Base );
}

UObject* WxPropertyWindow_Base::GetParentObject()
{
	return ParentItem->GetParentObject();
}

FString WxPropertyWindow_Base::GetPropertyText()
{
	FString	Result;
	BYTE* Data = GetReadAddress( this, bSingleSelectOnly );
	if( Data )
		Property->ExportText( 0, Result, Data - Property->Offset, Data - Property->Offset, NULL, PPF_Localized );
	return Result;
}

// Follows the window chain upwards until it finds the top level window, which is the property window

WxPropertyWindow* WxPropertyWindow_Base::FindPropertyWindow()
{
	wxWindow* Ret = this;

	while( Ret->GetParent() )
	{
		Ret = Ret->GetParent();
		if( wxDynamicCast( Ret, WxPropertyWindow ) )
			return wxDynamicCast( Ret, WxPropertyWindow );
	}

	return NULL;
}

void WxPropertyWindow_Base::OnLeftClick( wxMouseEvent& In )
{
	if( IsOnSplitter( In.GetX() ) )
	{
		// Set up to draw the window splitter
		FindPropertyWindow()->bDraggingSplitter = 1;
		LastX = In.GetX();
		CaptureMouse();
	}
	else
	{
		if( !InputProxy->LeftClick( this, In.GetX(), In.GetY() ) )
		{
			if( bCanBeExpanded )
			{
				// Only expand if clicking on the left side of the splitter
				if( In.GetX() < FindPropertyWindow()->SplitterPos )
				{
					if( bExpanded )
						Collapse();
					else
						Expand();
				}
			}
		}
	}
}

void WxPropertyWindow_Base::DeleteChildItems()
{
	for( INT x = 0 ; x < ChildItems.Num() ; ++x )
		ChildItems(x)->Destroy();

	ChildItems.Empty();

	Next = NULL;
}

void WxPropertyWindow_Base::Expand()
{
	if( bExpanded )
	{
		return;
	}

	bExpanded = 1;

	CreateChildItems();
	FindPropertyWindow()->PositionChildren();

	Refresh();
}

void WxPropertyWindow_Base::Collapse()
{
	if( !bExpanded )
	{
		return;
	}

	bExpanded = 0;

	WxPropertyWindow* PW = FindPropertyWindow();

	DeleteChildItems();

	PW->PositionChildren();
	Refresh();
}

void WxPropertyWindow_Base::OnLeftUnclick( wxMouseEvent& In )
{
	// Stop dragging the splitter

	if( FindPropertyWindow()->bDraggingSplitter )
	{
		FindPropertyWindow()->bDraggingSplitter = 0;
		LastX = 0;
		ReleaseMouse();
		wxSizeEvent se;
		FindPropertyWindow()->OnSize( se );
	}
}

void WxPropertyWindow_Base::OnMouseMove( wxMouseEvent& In )
{
	if( FindPropertyWindow()->bDraggingSplitter )
	{
		FindPropertyWindow()->SplitterPos -= LastX - In.GetX();
		LastX = In.GetX();
		SetCursor( wxCursor( wxCURSOR_SIZEWE ) );
	}

	if( IsOnSplitter( In.GetX() ) )
		SetCursor( wxCursor( wxCURSOR_SIZEWE ) );
	else
		SetCursor( wxCursor( wxCURSOR_ARROW ) );
}

void WxPropertyWindow_Base::OnLeftDoubleClick( wxMouseEvent& In )
{
	FString Value = GetPropertyText();
	UBOOL bForceValue = 0;
	if( !Value.Len() )
	{
		Value = GTrue;
		bForceValue = 1;
	}

	WxPropertyWindow_Objects* objwin = FindObjectItemParent();
	for( INT i = 0 ; i < objwin->Objects.Num() ; i++ )
	{
		InputProxy->DoubleClick( this, objwin->Objects(i), GetBase( (BYTE*)(objwin->Objects(i)) ), *Value, bForceValue );
	}

	Refresh();
}

void WxPropertyWindow_Base::OnSetFocus( wxFocusEvent& In )
{
	WxPropertyWindow* PW = FindPropertyWindow();

	// Update the previously focused item

	if( PW->LastFocused )
	{
		PW->LastFocused->Refresh();
		PW->LastFocused->InputProxy->SendToObjects( PW->LastFocused );
		PW->LastFocused->InputProxy->DeleteControls();
	}

	// Create any input controls this newly focused item needs

	wxRect rc = GetClientRect();
	InputProxy->CreateControls( this, FindObjectItemParent()->BaseClass, wxRect( rc.x+PW->SplitterPos,rc.y,rc.width-PW->SplitterPos,PROP_ItemHeight ), *GetPropertyText() );

	// Misc

	PW->LastFocused = this;
	Refresh();
}

// Creates any child items for any properties within this item

void WxPropertyWindow_Base::CreateChildItems()
{
}

// Creates a path to the current item

FString WxPropertyWindow_Base::GetPath()
{
	FString Name;

	if( ParentItem )
	{
		Name += ParentItem->GetPath();
		Name += TEXT(".");
	}

	if( Property )
		Name += Property->GetName();

	return Name;
}

void WxPropertyWindow_Base::RememberExpandedItems( TArray<FString>* InExpandedItems )
{
	if( bExpanded )
	{
		new( *InExpandedItems )FString( GetPath() );

		for( INT x = 0 ; x < ChildItems.Num() ; ++x )
			ChildItems(x)->RememberExpandedItems( InExpandedItems );
	}
}

void WxPropertyWindow_Base::RestoreExpandedItems( TArray<FString>* InExpandedItems )
{
	if( InExpandedItems->FindItemIndex( GetPath() ) != INDEX_NONE )
	{
		Expand();

		for( INT x = 0 ; x < ChildItems.Num() ; ++x )
			ChildItems(x)->RestoreExpandedItems( InExpandedItems );
	}
}

// Follows the chain of items upwards until it finds the object window that houses this item.

WxPropertyWindow_Objects* WxPropertyWindow_Base::FindObjectItemParent()
{
	wxWindow* Ret = this;

	while( !wxDynamicCast( Ret, WxPropertyWindow_Objects ) )
	{
		Ret = Ret->GetParent();
		if( wxDynamicCast( Ret, WxPropertyWindow_Objects ) )
			break;
	}

	return wxDynamicCast( Ret, WxPropertyWindow_Objects );
}

BYTE* WxPropertyWindow_Base::GetReadAddress( WxPropertyWindow_Base* InItem, UBOOL InRequiresSingleSelection )
{
	return ParentItem ? ParentItem->GetReadAddress( InItem, InRequiresSingleSelection ) : NULL;
}

void WxPropertyWindow_Base::OnPropItemButton( wxCommandEvent& In )
{
	switch( In.GetId() )
	{
		case ID_PROP_PB_ADD:
			InputProxy->ButtonClick( this, PB_Add );
			break;
		case ID_PROP_PB_EMPTY:
			InputProxy->ButtonClick( this, PB_Empty );
			break;
		case ID_PROP_PB_INSERT:
			InputProxy->ButtonClick( this, PB_Insert );
			break;
		case ID_PROP_PB_DELETE:
			InputProxy->ButtonClick( this, PB_Delete );
			break;
		case ID_PROP_PB_BROWSE:
			InputProxy->ButtonClick( this, PB_Browse );
			break;
		case ID_PROP_PB_PICK:
			InputProxy->ButtonClick( this, PB_Pick );
			break;
		case ID_PROP_PB_CLEAR:
			InputProxy->ButtonClick( this, PB_Clear );
			break;
		case ID_PROP_PB_FIND:
			InputProxy->ButtonClick( this, PB_Find );
			break;
		case ID_PROP_PB_USE:
			InputProxy->ButtonClick( this, PB_Use );
			break;
		case ID_PROP_PB_NEWOBJECT:
			InputProxy->ButtonClick( this, PB_NewObject );
			break;
	}
}

void WxPropertyWindow_Base::OnEditInlineNewClass( wxCommandEvent& In )
{
	UClass* NewClass = *ClassMap.Find( In.GetId() );
	if( !NewClass )
		return;

	Collapse();

	UObject* ParentObject = FindObjectItemParent()->GetParentObject();
	WxPropertyWindow* pw = FindPropertyWindow();

	NewObject( NewClass, ParentObject );

	//if( pw->NotifyHook )
	//	pw->NotifyHook->NotifyExec( this, *FString::Printf(TEXT("NEWNEWOBJECT CLASS=%s PARENT=%s PARENTCLASS=%s"), NewClass->GetName(), *ParentObject->GetPathName(), ParentObject->GetClass()->GetName() ) );

	Expand();
}

// Creates a new object (for editinline functionality)

void WxPropertyWindow_Base::NewObject( UClass* InClass, UObject* InParent )
{
	UObject* NewObject = NULL;

	UObject* UseOuter = (InClass->IsChildOf(UComponent::StaticClass()) || InParent->IsA(UClass::StaticClass()) ? InParent : InParent->GetOuter());
	NewObject = GEngine->StaticConstructObject( InClass, UseOuter, NAME_None, RF_Public|((InClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0) );

	if( NewObject )
		InputProxy->SendTextToObjects( this, *NewObject->GetPathName() );
}

// Checks to see if an X position is on top of the property windows splitter bar

UBOOL WxPropertyWindow_Base::IsOnSplitter( INT InX )
{
	return abs( InX - FindPropertyWindow()->SplitterPos ) < 3;
}

// Expands all category items belonging to this window

void WxPropertyWindow_Base::ExpandCategories()
{
	for( INT x = 0 ; x < ChildItems.Num() ; ++x )
		if( wxDynamicCast( ChildItems(x), WxPropertyWindow_Category ) )
			ChildItems(x)->Expand();
}

void WxPropertyWindow_Base::OnChar( wxKeyEvent& In )
{
	WxPropertyWindow* pw = FindPropertyWindow();

	InputProxy->SendToObjects( this );
	if( ParentItem )
		ParentItem->Refresh();

	switch( In.GetKeyCode() )
	{
		case WXK_DOWN:
		{
			pw->MoveToNextItem( this );
		}
		break;

		case WXK_UP:
		{
			pw->MoveToPrevItem( this );
		}
		break;

		case WXK_TAB:
		{
			if( In.ShiftDown() )
				pw->MoveToPrevItem( this );
			else
				pw->MoveToNextItem( this );
		}
		break;

		case WXK_RETURN:
		{
			// Pressing ENTER on a struct header item will toggle it's expanded status\

			if( Cast<UStructProperty>(Property) )
			{
				if( bExpanded )
				{
					Collapse();
				}
				else
				{
					Expand();
					pw->MoveToNextItem( this );
				}
			}
		}
		break;
	}
}

IMPLEMENT_COMPARE_POINTER( WxPropertyWindow_Base, XWindowProperties, { if( wxDynamicCast( A, WxPropertyWindow_Category ) ) return appStricmp( *((WxPropertyWindow_Category*)A)->CategoryName, *((WxPropertyWindow_Category*)B)->CategoryName ); else return appStricmp( A->Property->GetName(), B->Property->GetName() ); } )

void WxPropertyWindow_Base::SortChildren()
{
	if( bSorted )
	{
		Sort<USE_COMPARE_POINTER(WxPropertyWindow_Base,XWindowProperties)>( &ChildItems(0), ChildItems.Num() );
	}
}

void WxPropertyWindow_Base::Serialize( FArchive& Ar )
{
	Ar << InputProxy << DrawProxy;

	for( INT x = 0 ; x < ChildItems.Num() ; ++x )
		ChildItems(x)->Serialize( Ar );
}

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Objects
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow_Objects,WxPropertyWindow_Base)

BEGIN_EVENT_TABLE( WxPropertyWindow_Objects, WxPropertyWindow_Base )
	EVT_PAINT( WxPropertyWindow_Objects::OnPaint )
END_EVENT_TABLE()

WxPropertyWindow_Objects::WxPropertyWindow_Objects( wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx )
	: WxPropertyWindow_Base( InParent, InParentItem, InProperty, InPropertyOffset, InArrayIdx )
	,	BaseClass( NULL )
{
	FinishSetUp( InProperty );

	bSorted = InParentItem ? InParentItem->bSorted : 1;
	ChildHeight = 0;
}

WxPropertyWindow_Objects::~WxPropertyWindow_Objects()
{
}

void WxPropertyWindow_Objects::CreateChildItems()
{
	if( FindPropertyWindow()->bShowCategories )
		ChildHeight = ChildSpacer = 0;
	else
	{
		ChildHeight = ChildSpacer = PROP_CategorySpacer;
		SetWindowStyleFlag( wxCLIP_CHILDREN|wxCLIP_SIBLINGS );
	}

	Collapse();

	if( bEditInlineUse )
	{
		bCanBeExpanded = 0;
		return;
	}

	// Get a list of unique category names

	TArray<FName> Categories;
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
		if( !RespectEditFlag || (It->PropertyFlags&CPF_Edit && BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE) )
			Categories.AddUniqueItem( It->Category );

	// Add the category headers and the child items that belong inside of them

	// !Only show category headers if this is the top level object window (and the parent window allows them)

	if( ParentItem == NULL && FindPropertyWindow()->bShowCategories )
	{
		for( INT x = 0 ; x < Categories.Num() ; ++x )
		{
			WxPropertyWindow_Category* pwc = new WxPropertyWindow_Category( Categories(x), BaseClass, this, this, NULL, OFFSET_None, INDEX_NONE );
			ChildItems.AddItem( pwc );

			if( BaseClass->AutoExpandCategories.FindItemIndex(Categories(x)) != INDEX_NONE )
			{
				pwc->Expand();
			}
		}
	}
	else
	{
		TArray<UProperty*> Properties;

		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
			if( !RespectEditFlag || (It->PropertyFlags&CPF_Edit && BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE) )
				Properties.AddItem( *It );

		for( INT x = 0 ; x < Properties.Num() ; ++x )
			ChildItems.AddItem( new WxPropertyWindow_Item( this, this, Properties(x), Properties(x)->Offset, INDEX_NONE ) );
	}

	SortChildren();

}

// Adds a new object to the list

void WxPropertyWindow_Objects::AddObject( UObject* InObject )
{
	Objects.AddUniqueItem( InObject );
}

// Removes an object from the list

void WxPropertyWindow_Objects::RemoveObject( UObject* InObject )
{
	INT idx = Objects.FindItemIndex( InObject );

	if( idx != INDEX_NONE )
		Objects.Remove( idx, 1 );
	
	if( !Objects.Num() )
	{
		Collapse();
	}
}

// Removes all objects from the list

void WxPropertyWindow_Objects::RemoveAllObjects()
{
	while( Objects.Num() )
		RemoveObject( Objects(0) );
}

// When the object list is finalized, call this function to finish the property window set up

void WxPropertyWindow_Objects::Finalize()
{
	BaseClass = GetBestBaseClass();
	Expand();

	// Only automatically expand and position the child windows if this is the top level object window

	if( this == FindPropertyWindow()->Root )
	{
		bExpanded = 1;
		FindPropertyWindow()->ThumbPos = 0;
		FindPropertyWindow()->PositionChildren();
	}

	WxPropertyWindowFrame* ParentFrame = wxDynamicCast(GetParent()->GetParent(),WxPropertyWindowFrame);
	if( ParentFrame )		ParentFrame->UpdateTitle();
}

// Looks at the Objects array and returns the best base class

UClass* WxPropertyWindow_Objects::GetBestBaseClass()
{
	UClass* BestClass = NULL;

	for( INT x = 0 ; x < Objects.Num() ; ++x )
	{
		check( Objects(x)->GetClass() );

		if( !BestClass )	BestClass = Objects(x)->GetClass();

		while( !Objects(x)->GetClass()->IsChildOf( BestClass ) )
			BestClass = BestClass->GetSuperClass();
	}

	return BestClass;
}

BYTE* WxPropertyWindow_Objects::GetReadAddress( WxPropertyWindow_Base* InItem, UBOOL InRequiresSingleSelection )
{
	if( !Objects.Num() )									return NULL;
	if( !InItem->Property )									return NULL;
	if( InRequiresSingleSelection && Objects.Num() > 1 )	return NULL;

	// If this item is the child of an array, return NULL if there is a different number
	// of items in the array in different objects, when multi-selecting.

	if( Cast<UArrayProperty>(InItem->Property->GetOuter()) )
	{
		INT Num = ((FArray*)InItem->ParentItem->GetBase((BYTE*)Objects(0)))->Num();
		for( INT i = 1 ; i < Objects.Num() ; i++ )
			if( Num != ((FArray*)InItem->ParentItem->GetBase((BYTE*)Objects(i)))->Num() )
				return NULL;
	}

	BYTE* Base = InItem->GetBase( (BYTE*)(Objects(0)) );

	// If the item is an array itself, return NULL if there are a different number of
	// items in the array in different objects, when multi-selecting.

	if( Cast<UArrayProperty>(InItem->Property) )
	{
		INT Num = ((FArray*)InItem->GetBase( (BYTE*)Objects(0) ))->Num();
		for( INT i = 1 ; i < Objects.Num() ; i++ )
			if( Num != ((FArray*)InItem->GetBase((BYTE*)Objects(i)))->Num() )
				return NULL;
	}
	else
	{
		// Checks to see if the value of this property is the same in all selected objects.  If not, it returns NULL.

		for( INT i = 1 ; i < Objects.Num() ; i++ )
			if( !InItem->Property->Identical( Base, InItem->GetBase( (BYTE*)Objects(i) ) ) )
				return NULL;
	}

	// Everything checked out and we have a usable memory address...

	return Base;
}

BYTE* WxPropertyWindow_Objects::GetBase( BYTE* Base )
{
	return Base;
}

UObject* WxPropertyWindow_Objects::GetParentObject()
{
	return Objects(0);
}

void WxPropertyWindow_Objects::Expand()
{
	WxPropertyWindow_Base::Expand();
}

void WxPropertyWindow_Objects::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc( this );

	if( FindPropertyWindow()->bShowCategories || FindPropertyWindow()->Root != this )
	{
		In.Skip();
		return;
	}

	wxRect rc = GetClientRect();

	dc.SetBrush( wxBrush( wxColour(64,64,64), wxSOLID ) );
	dc.SetPen( *wxMEDIUM_GREY_PEN );

	dc.DrawRoundedRectangle( rc.x+1,rc.y+1, rc.width-2,rc.height-2, 5 );
}

FString WxPropertyWindow_Objects::GetPath()
{
	FString Name;

	if( ParentItem )
	{
		Name += ParentItem->GetPath();
		Name += TEXT(".");
	}

	Name += TEXT("Object");

	return Name;
}

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Category
-----------------------------------------------------------------------------*/

IMPLEMENT_DYNAMIC_CLASS(WxPropertyWindow_Category,WxPropertyWindow_Base)

BEGIN_EVENT_TABLE( WxPropertyWindow_Category, WxPropertyWindow_Base )
	EVT_LEFT_DOWN( WxPropertyWindow_Category::OnLeftClick )
	EVT_PAINT( WxPropertyWindow_Category::OnPaint )
	EVT_MOTION( WxPropertyWindow_Category::OnMouseMove )
END_EVENT_TABLE()

WxPropertyWindow_Category::WxPropertyWindow_Category( FName InCategoryName, UClass* InBaseClass, wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx )
	: WxPropertyWindow_Base( InParent, InParentItem, InProperty, InPropertyOffset, InArrayIdx )
	,	CategoryName( InCategoryName )
{
	FinishSetUp( InProperty );

	ChildHeight = PROP_CategoryHeight;
	ChildSpacer = PROP_CategorySpacer;

	SetWindowStyleFlag( wxCLIP_CHILDREN|wxCLIP_SIBLINGS );
}

WxPropertyWindow_Category::~WxPropertyWindow_Category()
{
}

void WxPropertyWindow_Category::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc( this );

	wxRect rc = GetClientRect();

	dc.SetFont( *wxNORMAL_FONT );

	dc.SetBrush( wxBrush( wxColour(64,64,64), wxSOLID ) );
	dc.SetPen( *wxMEDIUM_GREY_PEN );
	wxFont Font = dc.GetFont();
	Font.SetWeight( wxBOLD );
	dc.SetFont( Font );

	// Only top level categories have the round edged borders

	if( GetParent() == FindPropertyWindow()->Root )
		dc.DrawRoundedRectangle( rc.x+1,rc.y+1, rc.width-2,rc.height-2, 5 );

	dc.SetTextForeground( *wxWHITE );
	dc.DrawText( *CategoryName, rc.x+4+ARROW_Width,rc.y+3 );
}

void WxPropertyWindow_Category::CreateChildItems()
{
	TArray<UProperty*> Properties;

	// The parent of a category window has to be an object window.

	WxPropertyWindow_Objects* itemobj = FindObjectItemParent();

	// Get a list of properties that are in the same category

	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(itemobj->BaseClass); It; ++It )
		if( !RespectEditFlag || (It->Category == CategoryName && (It->PropertyFlags&CPF_Edit)) )
		{
			Properties.AddItem( *It );
		}

	for( INT x = 0 ; x < Properties.Num() ; ++x )
		ChildItems.AddItem( new WxPropertyWindow_Item( this, this, Properties(x), Properties(x)->Offset, INDEX_NONE ) );

	SortChildren();
}

void WxPropertyWindow_Category::OnLeftClick( wxMouseEvent& In )
{
	if( bExpanded )
		Collapse();
	else
		Expand();
}

void WxPropertyWindow_Category::OnMouseMove( wxMouseEvent& In )
{
	// Blocks message from the base class
	In.Skip();
}

FString WxPropertyWindow_Category::GetPath()
{
	FString Name;

	if( ParentItem )
	{
		Name += ParentItem->GetPath();
		Name += TEXT(".");
	}

	Name += *CategoryName;

	return Name;
}

/*-----------------------------------------------------------------------------
	WxPropertyWindow_Item
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxPropertyWindow_Item, WxPropertyWindow_Base )
	EVT_PAINT( WxPropertyWindow_Item::OnPaint )
	EVT_TEXT_ENTER( ID_PROPWINDOW_EDIT, WxPropertyWindow_Item::OnInputTextEnter )
	EVT_TEXT( ID_PROPWINDOW_EDIT, WxPropertyWindow_Item::OnInputTextChanged )
	EVT_COMBOBOX( ID_PROPWINDOW_COMBOBOX, WxPropertyWindow_Item::OnInputComboBox )
END_EVENT_TABLE()

WxPropertyWindow_Item::WxPropertyWindow_Item( wxWindow* InParent, WxPropertyWindow_Base* InParentItem, UProperty* InProperty, INT InPropertyOffset, INT InArrayIdx )
	: WxPropertyWindow_Base( InParent, InParentItem, InProperty, InPropertyOffset, InArrayIdx )
{
	FinishSetUp( InProperty );

	bCanBeExpanded = 0;

	if(	Cast<UStructProperty>(InProperty)
			||	( Cast<UArrayProperty>(InProperty) && GetReadAddress(this,0) )
			||  bEditInline
			||	( InProperty->ArrayDim > 1 && InArrayIdx == -1 ) )
		bCanBeExpanded = 1;
}

WxPropertyWindow_Item::~WxPropertyWindow_Item()
{
}

void WxPropertyWindow_Item::OnPaint( wxPaintEvent& In )
{
	wxPaintDC dc( this );

	wxColour basecolor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
	wxColour lightcolor( basecolor.Red()-32, basecolor.Green()-32, basecolor.Blue()-32 );
	wxColour lightercolor( basecolor.Red()-24, basecolor.Green()-24, basecolor.Blue()-24 );
	INT SplitterPos = FindPropertyWindow()->SplitterPos-2;

	wxRect rc = GetClientRect();

	if( FindPropertyWindow()->LastFocused == this )
	{
		wxColour HighlightClr = wxColour( basecolor.Red(), 255, basecolor.Blue() );
		dc.SetBrush( wxBrush( HighlightClr, wxSOLID ) );
		dc.SetPen( wxPen( HighlightClr, 1, wxSOLID ) );
		dc.DrawRectangle( wxRect( rc.x, rc.y, rc.GetWidth(), PROP_ItemHeight ) );
	}

	dc.SetFont( *wxNORMAL_FONT );

	if( bCanBeExpanded )
	{
		dc.SetTextBackground( lightcolor );

		dc.SetBrush( wxBrush( lightcolor, wxSOLID ) );
		dc.SetPen( wxPen( lightcolor, 1, wxSOLID ) );
		wxRect wkrc( rc.x + IndentX, rc.y, rc.width - IndentX, rc.height );
		dc.DrawRectangle( wkrc );

		dc.DrawBitmap( bExpanded ? GPropertyWindowManager->ArrowDownB : GPropertyWindowManager->ArrowRightB, IndentX,wkrc.y+((PROP_ItemHeight - GPropertyWindowManager->ArrowDownB.GetHeight())/2), 1 );
	}

	// Thin line on top to act like separator

	dc.SetPen( wxPen( lightercolor, 1, wxSOLID ) );
	dc.DrawLine( 0,0, rc.GetWidth(),0 );
	dc.DrawLine( SplitterPos,0, SplitterPos,rc.GetHeight() );

	//
	// LEFT SIDE OF SPLITTER
	//

	dc.SetClippingRegion( rc.x,rc.y, SplitterPos-2,rc.GetHeight() );

	// If this item has focus, draw the text in bold

	if( FindPropertyWindow()->LastFocused == this )
	{
		wxFont Font = dc.GetFont();
		Font.SetWeight( wxBOLD );
		dc.SetFont( Font );
	}

	// Draw the text

	FString LeftText;
	if( ArrayIndex==-1 )
		LeftText = *Property->GetFName();
	else
		LeftText = *FString::Printf(TEXT("[%i]"), ArrayIndex );

	INT W, H, YOffset;
	dc.GetTextExtent( *LeftText, &W, &H );
	YOffset = (PROP_ItemHeight - H) / 2;
	dc.DrawText( *LeftText, rc.x+IndentX+( bCanBeExpanded ? ARROW_Width : 0 ),rc.y+YOffset );

	dc.DestroyClippingRegion();

	//
	// RIGHT SIDE OF SPLITTER
	//

	// Set up a proper clipping area and tell the draw proxy to draw the value

	dc.SetClippingRegion( SplitterPos+2,rc.y, rc.GetWidth()-(SplitterPos+2),rc.GetHeight() );

	BYTE* ValueAddress = GetReadAddress( this, bSingleSelectOnly );
	wxRect ValueRect( SplitterPos+2,0, rc.GetWidth()-(SplitterPos+2),PROP_ItemHeight );

	if( ValueAddress )
		DrawProxy->Draw( &dc, ValueRect, ValueAddress, Property, ArrayIndex, InputProxy );
	else
		DrawProxy->DrawUnknown( &dc, ValueRect );

	dc.DestroyClippingRegion();

	// Clean up

	dc.SetBrush( wxNullBrush );
	dc.SetPen( wxNullPen );
}

void WxPropertyWindow_Item::OnInputTextChanged( wxCommandEvent& In )
{
	Refresh();
}

void WxPropertyWindow_Item::OnInputTextEnter( wxCommandEvent& In )
{
	InputProxy->SendToObjects( this );
	FindPropertyWindow()->MoveToNextItem( this );
	ParentItem->Refresh();
	Refresh();
}

void WxPropertyWindow_Item::OnInputComboBox( wxCommandEvent& In )
{
	InputProxy->SendToObjects( this );
	Refresh();
}

void WxPropertyWindow_Item::CreateChildItems()
{
	UStructProperty* StructProperty = Cast<UStructProperty>(Property);
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
	UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);

	bSorted = ParentItem ? ParentItem->bSorted : 1;

	if( Property->ArrayDim > 1 && ArrayIndex == -1 )
	{
		// Expand array.
		bSorted = 0;
		for( INT i = 0 ; i < Property->ArrayDim ; i++ )
			ChildItems.AddItem( new WxPropertyWindow_Item( this, this, Property, i*Property->ElementSize, i ) );
	}
	else if( ArrayProperty )
	{
		// Expand array.
		bSorted = 0;
		FArray* Array = (FArray*)GetReadAddress( this, bSingleSelectOnly );
		if( Array )
			for( INT i = 0 ; i < Array->Num() ; i++ )
			{
				ChildItems.AddItem( new WxPropertyWindow_Item( this, this, ArrayProperty->Inner, i*ArrayProperty->Inner->ElementSize, i ) );
			}
	}
	else if( StructProperty )
	{
		// Expand struct.
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(StructProperty->Struct); It; ++It )
			if( !RespectEditFlag || (It->PropertyFlags&CPF_Edit) )
				ChildItems.AddItem( new WxPropertyWindow_Item( this, this, *It, It->Offset, INDEX_NONE ) );
	}
	else if( ObjectProperty )
	{
		BYTE* ReadValue = GetReadAddress( this, bSingleSelectOnly );
		if( ReadValue )
		{
			UObject* obj = *(UObject**)ReadValue;
			if( obj )
			{
				INT idx = ChildItems.AddItem( new WxPropertyWindow_Objects( this, this, NULL, OFFSET_None, INDEX_NONE ) );
				((WxPropertyWindow_Objects*)ChildItems(idx))->AddObject( obj );
				((WxPropertyWindow_Objects*)ChildItems(idx))->Finalize();
			}
			else
			{
				PropertyClass = ObjectProperty->PropertyClass;
				//check(0);	// Handle this!
				//Children.AddItem( new(TEXT("FNewObjectItem"))FNewObjectItem( OwnerProperties, this, ObjectProperty ) );
			}
		}
	}

	SortChildren();
}

BYTE* WxPropertyWindow_Item::GetBase( BYTE* Base )
{
	return ParentItem->GetContents(Base) + PropertyOffset;
}

BYTE* WxPropertyWindow_Item::GetContents( BYTE* Base )
{
	Base = GetBase(Base);
	UArrayProperty* ArrayProperty;
	if( (ArrayProperty=Cast<UArrayProperty>(Property))!=NULL )
		Base = (BYTE*)((FArray*)Base)->GetData();
	return Base;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
