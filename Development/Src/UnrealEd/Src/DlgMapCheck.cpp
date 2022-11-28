
#include "UnrealEd.h"
#include "..\..\Launch\Resources\resource.h"

struct {
	char* Text;
	INT Width;
} GCols[] =
{
	"Actor", 80,
	"Message", 500,
	NULL, -1,
};

WDlgMapCheck::WDlgMapCheck( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog				( TEXT("Check Map"), IDDIALOG_MAP_CHECK, InOwnerWindow )
	,	RefreshButton		( this, IDPB_REFRESH, FDelegate(this, static_cast<TDelegate>( &WDlgMapCheck::OnRefresh ) ) )
	,	CloseButton			( this, IDPB_CLOSE, FDelegate(this, static_cast<TDelegate>( &WDlgMapCheck::OnClose ) ) )
	,	GotoActorButton		( this, IDPB_GOTOACTOR, FDelegate(this,static_cast<TDelegate>( &WDlgMapCheck::OnItemListDblClk ) ) )
	,	DeleteActorButton	( this, IDPB_DELETEACTOR, FDelegate(this,static_cast<TDelegate>( &WDlgMapCheck::OnDeleteActor ) ) )
	,	ItemList			( this, IDLC_ITEMS )
{
	Container = NULL;
}

void WDlgMapCheck::OnInitDialog()
{
	WDialog::OnInitDialog();

	Container = new FContainer();

	// Set up the list view
	LVCOLUMNA lvcol;
	lvcol.mask = LVCF_TEXT | LVCF_WIDTH;

	for( INT x = 0 ; GCols[x].Text ; ++x )
	{
		lvcol.pszText = GCols[x].Text;
		lvcol.cx = GCols[x].Width;

		SendMessageX( ItemList.hWnd, LVM_INSERTCOLUMNA, x, (LPARAM)(const LPLVCOLUMNA)&lvcol );
	}

	himl = ImageList_LoadImage( hInstance, MAKEINTRESOURCE(IDBM_MAP_ERRORS), 16, 0, CLR_DEFAULT, IMAGE_BITMAP, LR_LOADMAP3DCOLORS );
	check(himl);
	ListView_SetImageList( ItemList.hWnd, himl, LVSIL_SMALL );

	ItemList.DblClkDelegate = FDelegate(this, static_cast<TDelegate>( &WDlgMapCheck::OnItemListDblClk ) );

	Anchors.Set( (DWORD)ItemList.hWnd,			FWindowAnchor( hWnd, ItemList.hWnd,			ANCHOR_TL, 4, 4,																	ANCHOR_BR, -8-STANDARD_BUTTON_WIDTH, -STANDARD_BUTTON_HEIGHT-8 ) );
	Anchors.Set( (DWORD)RefreshButton.hWnd,		FWindowAnchor( hWnd, RefreshButton.hWnd,
		ANCHOR_RIGHT|ANCHOR_BOTTOM, (-STANDARD_BUTTON_WIDTH-4)*2, -STANDARD_BUTTON_HEIGHT-4,
		ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_BUTTON_WIDTH, STANDARD_BUTTON_HEIGHT ) );
	Anchors.Set( (DWORD)CloseButton.hWnd,		FWindowAnchor( hWnd, CloseButton.hWnd,		ANCHOR_RIGHT|ANCHOR_BOTTOM, -STANDARD_BUTTON_WIDTH-4, -STANDARD_BUTTON_HEIGHT-4,	ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_BUTTON_WIDTH, STANDARD_BUTTON_HEIGHT ) );

	Anchors.Set( (DWORD)GotoActorButton.hWnd,	FWindowAnchor( hWnd, GotoActorButton.hWnd,	ANCHOR_RIGHT|ANCHOR_TOP, -STANDARD_BUTTON_WIDTH-4, 4,	ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_BUTTON_WIDTH, STANDARD_BUTTON_HEIGHT ) );
	Anchors.Set( (DWORD)DeleteActorButton.hWnd,	FWindowAnchor( hWnd, DeleteActorButton.hWnd,ANCHOR_RIGHT|ANCHOR_TOP, -STANDARD_BUTTON_WIDTH-4, 8+STANDARD_BUTTON_HEIGHT,	ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_BUTTON_WIDTH, STANDARD_BUTTON_HEIGHT ) );

	Container->SetAnchors( &Anchors );

}

void WDlgMapCheck::DoModeless( UBOOL bShow )
{
	_Windows.AddItem( this );
	hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_MAP_ERRORS), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
	if( !hWnd )
		appGetLastError();
	Show( bShow );
}

void WDlgMapCheck::OnDestroy()
{
	WDialog::OnDestroy();
	delete Container;
}

void WDlgMapCheck::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	if( Container ) Container->RefreshControls();
	WDialog::OnSize( Flags, NewX, NewY );
}

void WDlgMapCheck::OnCommand( INT Command )
{
	switch( Command )
	{
		case WM_MC_SHOW:
			Show(1);
			break;

		case WM_MC_HIDE:
			Show(0);
			break;

		case WM_MC_SHOW_COND:
			if( SendMessageX( ItemList.hWnd, LVM_GETITEMCOUNT, 0, 0 ) )
				Show(1);
			break;

		case WM_MC_CLEAR:
			ItemList.Empty();
			break;

		case WM_MC_ADD:
			{
				MAPCHECK* MC = (MAPCHECK*)LastlParam;

				// Add the message to the window.
				LVITEM lvi;
				::ZeroMemory( &lvi, sizeof(lvi));
				lvi.mask = LVIF_TEXT | LVIF_IMAGE;
				lvi.pszText = (LPWSTR) MC->Actor->GetName();
				lvi.iItem = 0;
				lvi.iImage = MC->Type;

				INT idx = SendMessageX( ItemList.hWnd, LVM_INSERTITEM, 0, (LPARAM)(const LPLVITEM)&lvi ); 
				if( idx > -1 )
				{
					::ZeroMemory( &lvi, sizeof(lvi));
					lvi.mask = LVIF_TEXT;
					lvi.pszText = (LPWSTR) *MC->Message;
					lvi.iItem = idx;
					lvi.iSubItem = 1;
					SendMessageX( ItemList.hWnd, LVM_SETITEM, 0, (LPARAM)(const LPLVITEM)&lvi );
				}
			}
			break;

		default:
			WWindow::OnCommand(Command);
			break;
	}

}

void WDlgMapCheck::OnRefresh()
{
	GUnrealEd->Exec( TEXT("MAP CHECK") );
}

void WDlgMapCheck::OnClose()
{
	Show(0);
}

void WDlgMapCheck::OnItemListDblClk()
{
	INT idx = ItemList.GetCurrent();
	if( idx == -1 ) return;	// Couldn't get valid selection

	char ActorName[80] = "";
	LVITEMA lvi;
	::ZeroMemory( &lvi, sizeof(lvi) );
	lvi.mask = LVIF_TEXT;
	lvi.cchTextMax = 80;
	lvi.pszText = ActorName;
	lvi.iItem = idx;

	SendMessageX( ItemList.hWnd, LVM_GETITEMA, 0, (LPARAM)(LPLVITEM)&lvi );

	GUnrealEd->Exec(TEXT("ACTOR SELECT NONE"));
	GUnrealEd->Exec(*FString::Printf(TEXT("CAMERA ALIGN NAME=%s"), ANSI_TO_TCHAR( ActorName ) ) );

}

void WDlgMapCheck::OnDeleteActor()
{
	OnItemListDblClk();
	GUnrealEd->Exec( TEXT("DELETE") );
	OnRefresh();

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/