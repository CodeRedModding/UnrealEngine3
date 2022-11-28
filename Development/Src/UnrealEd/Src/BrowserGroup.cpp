#include "UnrealEd.h"
#include "..\..\Launch\Resources\resource.h"

// --------------------------------------------------------------
// WBrowserGroup
// --------------------------------------------------------------

#define ID_BG_TOOLBAR	29050
TBBUTTON tbBGButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_GB_NEW_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_GB_DELETE_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 3, IDMN_GB_ADD_TO_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 4, IDMN_GB_DELETE_FROM_GROUP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 5, IDMN_GB_REFRESH, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 6, IDMN_GB_SELECT, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 7, IDMN_GB_DESELECT, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_BG[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("New Group"), IDMN_GB_NEW_GROUP,
	TEXT("Delete"), IDMN_GB_DELETE_GROUP,
	TEXT("Add Selected Actors to Group(s)"), IDMN_GB_ADD_TO_GROUP,
	TEXT("Delete Select Actors from Group(s)"), IDMN_GB_DELETE_FROM_GROUP,
	TEXT("Refresh Group List"), IDMN_GB_REFRESH,
	TEXT("Select Actors in Group(s)"), IDMN_GB_SELECT,
	TEXT("Deselect Actors in Group(s)"), IDMN_GB_DESELECT,
	NULL, 0
};

WBrowserGroup::WBrowserGroup( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
{
	Container = NULL;
	ListGroups = NULL;
	MenuID = IDMENU_BrowserGroup;
	Description = TEXT("Groups");
}

void WBrowserGroup::OpenWindow( UBOOL bChild )
{
	WBrowser::OpenWindow( bChild );
	SetCaption();
}

void WBrowserGroup::UpdateMenu()
{
	HMENU menu = IsDocked() ? GetMenu( OwnerWindow->hWnd ) : GetMenu( hWnd );
	CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );
}

void WBrowserGroup::OnCreate()
{
	WBrowser::OnCreate();

	SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserGroup) );

	Container = new FContainer();

	ListGroups = new WCheckListBox( this, IDLB_GROUPS );
	ListGroups->OpenWindow( 1, 0, 1, 1 );

	hWndToolBar = CreateToolbarEx( 
		hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
		IDB_BrowserGroup_TOOLBAR,
		8,
		hInstance,
		IDB_BrowserGroup_TOOLBAR,
		(LPCTBBUTTON)&tbBGButtons,
		11,
		16,16,
		16,16,
		sizeof(TBBUTTON));
	check(hWndToolBar);

	ToolTipCtrl = new WToolTip(this);
	ToolTipCtrl->OpenWindow();
	for( INT tooltip = 0 ; ToolTips_BG[tooltip].ID > 0 ; ++tooltip )
	{
		// Figure out the rectangle for the toolbar button.
		INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_BG[tooltip].ID, 0 );
		RECT rect;
		SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

		ToolTipCtrl->AddTool( hWndToolBar, ToolTips_BG[tooltip].ToolTip, tooltip, &rect );
	}

	INT Top = 0;
	Anchors.Set( (DWORD)hWndToolBar,		FWindowAnchor( hWnd, hWndToolBar,		ANCHOR_TL, 0, 0,	ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
	Top += STANDARD_TOOLBAR_HEIGHT+4;
	Anchors.Set( (DWORD)ListGroups->hWnd,	FWindowAnchor( hWnd, ListGroups->hWnd,	ANCHOR_TL, 4, Top,	ANCHOR_BR, -4, -4 ) );

	Container->SetAnchors( &Anchors );

	UpdateGroupList();
	PositionChildControls();

}

void WBrowserGroup::UpdateAll()
{
	UpdateGroupList();
	UpdateSelectedGroupList();
}

void WBrowserGroup::OnDestroy()
{
	delete Container;
	delete ListGroups;

	::DestroyWindow( hWndToolBar );
	delete ToolTipCtrl;

	WBrowser::OnDestroy();
}

void WBrowserGroup::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	WBrowser::OnSize(Flags, NewX, NewY);
	PositionChildControls();
	InvalidateRect( hWnd, NULL, FALSE );
	UpdateMenu();
}

// Updates the check status of all the groups, based on the contents of the VisibleGroups
// variable in the LevelInfo.

void WBrowserGroup::GetFromVisibleGroups()
{
	// First set all groups to "off"
	for( INT x = 0 ; x < ListGroups->GetCount() ; ++x )
		ListGroups->SetItemData( x, 0 );

	// Now turn "on" the ones we need to.
	TArray<FString> Array;
	GUnrealEd->Level->GetLevelInfo()->VisibleGroups.ParseIntoArray( TEXT(","), &Array );

	for( INT x = 0 ; x < Array.Num() ; ++x )
	{
		INT Index = ListGroups->FindStringExact( *Array(x) );
		if( Index != LB_ERR )
			ListGroups->SetItemData( Index, 1 );
	}

	UpdateVisibility();
}

// Updates the VisibleGroups variable in the LevelInfo

void WBrowserGroup::SendToVisibleGroups()
{
	FString NewVisibleGroups;

	for( INT x = 0 ; x < ListGroups->GetCount() ; ++x )
	{
		if( (int)ListGroups->GetItemData( x ) )
		{
			if( NewVisibleGroups.Len() )
				NewVisibleGroups += TEXT(",");
			NewVisibleGroups += ListGroups->GetString(x);
		}
	}

	GUnrealEd->Level->GetLevelInfo()->VisibleGroups = NewVisibleGroups;

	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
}

void WBrowserGroup::UpdateGroupList()
{
	// Loop through all the actors in the world and put together a list of unique group names.
	// Actors can belong to multiple groups by separating the group names with semi-colons ("group1;group2")
	TArray<FString> Groups;

	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if(	Actor 
			&& Actor!=GUnrealEd->Level->Brush()
			&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			TArray<FString> Array;
			FString S = *Actor->Group;
			S.ParseIntoArray( TEXT(","), &Array );

			for( INT x = 0 ; x < Array.Num() ; ++x )
			{
				// Only add the group name if it doesn't already exist.
				UBOOL bExists = FALSE;
				for( INT z = 0 ; z < Groups.Num() ; ++z )
				{
					if( Groups(z) == Array(x) )
					{
						bExists = 1;
						break;
					}
				}

				if( !bExists )
				{
					new(Groups)FString( Array(x) );
				}
			}
		}
	}

	// Add the list of unique group names to the group listbox
	ListGroups->Empty();

	for( INT x = 0 ; x < Groups.Num() ; ++x )
	{
		ListGroups->AddString( *Groups(x) );
		ListGroups->SetItemData( ListGroups->FindStringExact( *Groups(x) ), 1 );
	}

	GetFromVisibleGroups();

	ListGroups->SetCurrent( 0, 1 );
}

void WBrowserGroup::PositionChildControls( void )
{
	if( Container ) Container->RefreshControls();
}

// Loops through all actors in the world and updates their visibility based on which groups are checked.

void WBrowserGroup::UpdateVisibility()
{
	FString NewVisibleGroups;

	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);

		if(	Actor 
			&& Actor!=GUnrealEd->Level->Brush()
			&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			Actor->Modify();
			Actor->bHiddenEdGroup = 0;

			TArray<FString> Array;
			FString S = *Actor->Group;
			S.ParseIntoArray( TEXT(","), &Array );

			for( INT x = 0 ; x < Array.Num() ; ++x )
			{
				INT Index = ListGroups->FindStringExact( *Array(x) );
				if( Index != LB_ERR && !(int)ListGroups->GetItemData( Index ) )
				{
					Actor->bHiddenEdGroup = 1;
					Actor->ClearFlags( RF_EdSelected );
					break;
				}
			}
		}
	}

	GCallback->Send( CALLBACK_RedrawAllViewports );
}

void WBrowserGroup::OnCommand( INT Command )
{
	switch( Command ) {

		case IDMN_GB_NEW_GROUP:
			NewGroup();
			break;

		case IDMN_GB_DELETE_GROUP:
			DeleteGroup();
			break;

		case IDMN_GB_ADD_TO_GROUP:
			{
				INT SelCount = ListGroups->GetSelectedCount();
				if( SelCount == LB_ERR )	return;
				int* Buffer = new int[SelCount];
				ListGroups->GetSelectedItems(SelCount, Buffer);
				for( INT s = 0 ; s < SelCount ; ++s )
					AddToGroup(ListGroups->GetString(Buffer[s]));
				delete [] Buffer;
			}
			break;

		case IDMN_GB_DELETE_FROM_GROUP:
			DeleteFromGroup();
			break;

		case IDMN_GB_RENAME_GROUP:
			RenameGroup();
			break;

		case IDMN_GB_REFRESH:
			OnRefreshGroups();
			break;

		case IDMN_GB_SELECT:
			SelectActorsInGroups(1);
			break;

		case IDMN_GB_DESELECT:
			SelectActorsInGroups(0);
			break;

		case WM_WCLB_UPDATE_VISIBILITY:
			UpdateVisibility();
			SendToVisibleGroups();
			break;

		default:
			WBrowser::OnCommand(Command);
			break;
	}
}

void WBrowserGroup::SelectActorsInGroups( UBOOL Select )
{
	INT SelCount = ListGroups->GetSelectedCount();
	if( SelCount == LB_ERR )	return;
	int* Buffer = new int[SelCount];
	ListGroups->GetSelectedItems(SelCount, Buffer);

	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if(	Actor 
			&& Actor!=GUnrealEd->Level->Brush()
			&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			FString GroupName = *Actor->Group;
			TArray<FString> Array;
			GroupName.ParseIntoArray( TEXT(","), &Array );
			for( INT x = 0 ; x < Array.Num() ; ++x )
			{
				INT idx = ListGroups->FindStringExact( *Array(x) );

				for( INT s = 0 ; s < SelCount ; ++s )
					if( idx == Buffer[s] )
						GUnrealEd->SelectActor( GUnrealEd->Level, Actor, Select, 0 );
			}
		}
	}

	delete [] Buffer;
	GCallback->Send( CALLBACK_RedrawAllViewports );
	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );

}

void WBrowserGroup::NewGroup()
{
	if( !NumActorsSelected() )
	{
		appMsgf(0, TEXT("You must have some actors selected to create a new group."));
		return;
	}

	// Generate a suggested group name to use as a default.
	INT x = 1;
	FString DefaultName;
	while(1)
	{
		DefaultName = *FString::Printf(TEXT("Group%d"), x);
		if( ListGroups->FindStringExact( *DefaultName ) == LB_ERR )
			break;
		x++;
	}

	WxDlgGroup dlg;
	if( dlg.ShowModal( 1, DefaultName ) == wxID_OK )
	{
		if( GUnrealEd->Level->GetLevelInfo()->VisibleGroups.Len() )
			GUnrealEd->Level->GetLevelInfo()->VisibleGroups += TEXT(",");
		GUnrealEd->Level->GetLevelInfo()->VisibleGroups += dlg.Name;

		AddToGroup( dlg.Name );
		UpdateGroupList();
	}

}

void WBrowserGroup::DeleteGroup()
{
	INT SelCount = ListGroups->GetSelectedCount();
	if( SelCount == LB_ERR )	return;
	int* Buffer = new int[SelCount];
	ListGroups->GetSelectedItems(SelCount, Buffer);
	for( INT s = 0 ; s < SelCount ; ++s )
	{
		FString DeletedGroup = ListGroups->GetString(Buffer[s]);
		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if(	Actor 
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				FString GroupName = *Actor->Group;
				TArray<FString> Array;
				GroupName.ParseIntoArray( TEXT(","), &Array );
				FString NewGroup;
				for( INT x = 0 ; x < Array.Num() ; ++x )
				{
					if( Array(x) != DeletedGroup )
					{
						if( NewGroup.Len() )
							NewGroup += TEXT(",");
						NewGroup += Array(x);
					}
				}
				if( NewGroup != *Actor->Group )
				{
					Actor->Modify();
					Actor->Group = *NewGroup;
				}
			}
		}
	}
	delete [] Buffer;

	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
	UpdateGroupList();
}

void WBrowserGroup::AddToGroup( FString InGroupName)
{
	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if(	Actor 
			&& GSelectionTools.IsSelected( Actor )
			&& Actor!=GUnrealEd->Level->Brush()
			&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			FString GroupName = *Actor->Group, NewGroupName;

			// Make sure this actor is not already in this group.  If so, don't add it again.
			TArray<FString> Array;
			GroupName.ParseIntoArray( TEXT(","), &Array );
			INT x;
			for( x = 0 ; x < Array.Num() ; ++x )
			{
				if( Array(x) == InGroupName )
					break;
			}

			if( x == Array.Num() )
			{
				// Add the group to the actors group list
				NewGroupName = *FString::Printf(TEXT("%s%s%s"), *GroupName, (GroupName.Len()?TEXT(","):TEXT("")), *InGroupName );
				Actor->Modify();
				Actor->Group = *NewGroupName;
			}
		}
	}

	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
}

void WBrowserGroup::DeleteFromGroup()
{
	INT SelCount = ListGroups->GetSelectedCount();
	if( SelCount == LB_ERR )	return;
	int* Buffer = new int[SelCount];
	ListGroups->GetSelectedItems(SelCount, Buffer);
	for( INT s = 0 ; s < SelCount ; ++s )
	{
		FString DeletedGroup = ListGroups->GetString(Buffer[s]);

		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if(	Actor 
				&& GSelectionTools.IsSelected( Actor )
				&& Actor!=GUnrealEd->Level->Brush()
				&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
			{
				FString GroupName = *Actor->Group;
				TArray<FString> Array;
				GroupName.ParseIntoArray( TEXT(","), &Array );
				FString NewGroup;
				for( INT x = 0 ; x < Array.Num() ; ++x )
					if( Array(x) != DeletedGroup )
					{
						if( NewGroup.Len() )
							NewGroup += TEXT(",");
						NewGroup += Array(x);
					}
				if( NewGroup != *Actor->Group )
				{
					Actor->Modify();
					Actor->Group = *NewGroup;
				}
			}
		}
	}

	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
	UpdateGroupList();
}

void WBrowserGroup::RenameGroup()
{
	WxDlgGroup dlg;
	INT SelCount = ListGroups->GetSelectedCount();
	if( SelCount == LB_ERR )	return;
	int* Buffer = new int[SelCount];
	ListGroups->GetSelectedItems(SelCount, Buffer);
	for( INT s = 0 ; s < SelCount ; ++s )
	{
		FString Src = ListGroups->GetString(Buffer[s]);
		if( dlg.ShowModal( 0, Src ) == wxID_OK )
			SwapGroupNames( Src, dlg.Name );
	}
	delete [] Buffer;

	UpdateGroupList();
	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );

}

void WBrowserGroup::SwapGroupNames( FString Src, FString Dst )
{
	if( Src == Dst ) return;
	check(Src.Len());
	check(Dst.Len());

	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if(	Actor 
			&& Actor!=GUnrealEd->Level->Brush()
			&& Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			FString GroupName = *Actor->Group, NewGroup;
			TArray<FString> Array;
			GroupName.ParseIntoArray( TEXT(","), &Array );
			for( INT x = 0 ; x < Array.Num() ; ++x )
			{
				FString AddName;
				AddName = Array(x);
				if( Array(x) == Src )
					AddName = Dst;

				if( NewGroup.Len() )
					NewGroup += TEXT(",");
				NewGroup += AddName;
			}

			if( NewGroup != *Actor->Group )
			{
				Actor->Modify();
				Actor->Group = *NewGroup;
			}
		}
	}
}

INT WBrowserGroup::NumActorsSelected()
{
	UINT	NumSelected = 0;
	for(UINT ActorIndex = 0;ActorIndex < (UINT)GEditor->Level->Actors.Num();ActorIndex++)
		if(GEditor->Level->Actors(ActorIndex) && GSelectionTools.IsSelected( GEditor->Level->Actors(ActorIndex) ) )
			NumSelected++;
	return NumSelected;
}

void WBrowserGroup::UpdateSelectedGroupList()
{
	GUnrealEd->Level->GetLevelInfo()->SelectedGroups = TEXT("");
}

void WBrowserGroup::OnNewGroup()
{
	NewGroup();
}

void WBrowserGroup::OnDeleteGroup()
{
	DeleteGroup();
}

void WBrowserGroup::OnAddToGroup()
{
	INT SelCount = ListGroups->GetSelectedCount();
	if( SelCount == LB_ERR )	return;
	int* Buffer = new int[SelCount];
	ListGroups->GetSelectedItems(SelCount, Buffer);
	for( INT s = 0 ; s < SelCount ; ++s )
		AddToGroup(ListGroups->GetString(Buffer[s]));
	delete [] Buffer;
}

void WBrowserGroup::OnDeleteFromGroup()
{
	DeleteFromGroup();
}

void WBrowserGroup::OnRefreshGroups()
{
	UpdateGroupList();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
