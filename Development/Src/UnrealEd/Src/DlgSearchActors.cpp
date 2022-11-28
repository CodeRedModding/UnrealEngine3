
#include "UnrealEd.h"
#include "..\..\Launch\Resources\resource.h"


WDlgSearchActors::WDlgSearchActors( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("Search for Actors"), IDDIALOG_SEARCH, InOwnerWindow )
	,	CloseButton		( this, IDPB_CLOSE,			FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnClose ) ) )
	,	GotoActorButton	( this, IDPB_GOTOACTOR,		FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnActorListDblClick ) ) )
	,	DeleteActorButton( this, IDPB_DELETEACTOR,	FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnDeleteActor ) ) )
	,	NameEdit		( this, IDEC_NAME )
	,	EventEdit		( this, IDEC_EVENT )
	,	TagEdit			( this, IDEC_TAG )
	,	ActorList		( this, IDLB_NAMES )
{
}

void WDlgSearchActors::OnInitDialog()
{
	WDialog::OnInitDialog();
	ActorList.DoubleClickDelegate = FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnActorListDblClick ) );
	NameEdit.ChangeDelegate = FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnNameEditChange ) );
	EventEdit.ChangeDelegate = FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnEventEditChange ) );
	TagEdit.ChangeDelegate = FDelegate(this, static_cast<TDelegate>( &WDlgSearchActors::OnTagEditChange ) );
	RefreshActorList();
}

void WDlgSearchActors::OnShowWindow( UBOOL bShow )
{
	WWindow::OnShowWindow( bShow );
	RefreshActorList();
}

void WDlgSearchActors::DoModeless( UBOOL bShow )
{
	_Windows.AddItem( this );
	hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_SEARCH), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
	if( !hWnd )
		appGetLastError();
	Show( bShow );
}

void WDlgSearchActors::RefreshActorList( void )
{
	ActorList.Empty();
	//LockWindowUpdate( ActorList.hWnd );
	//HWND hwndFocus = ::GetFocus();
	FString Name, Event, Tag;

	Name = NameEdit.GetText();
	Event = EventEdit.GetText();
	Tag = TagEdit.GetText();

	if( GUnrealEd
			&& GUnrealEd->Level )
	{
		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* pActor = GUnrealEd->Level->Actors(i);
			if( pActor )
			{
				FString ActorName = pActor->GetName(),
					ActorTag = *(pActor->Tag);
				if( Name != ActorName.Left( Name.Len() ) )
					continue;
				if( Tag.Len() && Tag != ActorTag.Left( Tag.Len() ) )
					continue;

				ActorList.AddString( pActor->GetName() );
			}
		}
	}

	//LockWindowUpdate( NULL );
	//::SetFocus( hwndFocus );
}

void WDlgSearchActors::OnClose()
{
	Show(0);
}

void WDlgSearchActors::OnActorListDblClick()
{
	GUnrealEd->SelectNone( GUnrealEd->Level, 0 );
	GUnrealEd->Exec( *FString::Printf(TEXT("CAMERA ALIGN NAME=%s"), *(ActorList.GetString( ActorList.GetCurrent()) ) ) );
	GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
}

void WDlgSearchActors::OnDeleteActor()
{
	OnActorListDblClick();
	GUnrealEd->Exec( TEXT("DELETE") );
	RefreshActorList();
}

void WDlgSearchActors::OnNameEditChange()
{
	RefreshActorList();
}

void WDlgSearchActors::OnEventEditChange()
{
	RefreshActorList();
}

void WDlgSearchActors::OnTagEditChange()
{
	RefreshActorList();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
