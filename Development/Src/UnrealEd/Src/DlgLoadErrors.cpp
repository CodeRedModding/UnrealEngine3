
#include "UnrealEd.h"
#include "..\..\Launch\Resources\resource.h"

WDlgLoadErrors::WDlgLoadErrors( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("Load Errors"), IDDIALOG_LOAD_ERRORS, InOwnerWindow )
	,	OKButton		( this, IDOK, FDelegate(this, static_cast<TDelegate>( &WDlgLoadErrors::EndDialogTrue ) ) )
	,	PackageList		( this, IDLB_PACKAGES )
	,	ResourceList	( this, IDLB_RESOURCES )
{
}

void WDlgLoadErrors::OnInitDialog()
{
	WDialog::OnInitDialog();

}

void WDlgLoadErrors::DoModeless( UBOOL bShow )
{
	_Windows.AddItem( this );
	hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_LOAD_ERRORS), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
	if( !hWnd )
		appGetLastError();
	Show( bShow );
}

void WDlgLoadErrors::OnClose()
{
	Show(0);
}

void WDlgLoadErrors::Refresh()
{
	PackageList.Empty();
	ResourceList.Empty();

	for( INT x = 0 ; x < GEdLoadErrors.Num() ; ++x )
	{
		if( GEdLoadErrors(x).Type == FEdLoadError::TYPE_FILE )
			PackageList.AddString( *GEdLoadErrors(x).Desc );
		else
			ResourceList.AddString( *GEdLoadErrors(x).Desc );
	}

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/