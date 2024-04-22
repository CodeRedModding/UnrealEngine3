/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgOpenPackages.h"

PackageTreePath::PackageTreePath( FString InPath )
{
	Path = InPath;
}

BEGIN_EVENT_TABLE(WxDlgOpenPackages, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgOpenPackages::OnOK )
END_EVENT_TABLE()

WxDlgOpenPackages::WxDlgOpenPackages()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_OPENPACKAGES") );
	check( bSuccess );

	FString ContentDir = GApp->LastDir[LD_GENERIC_OPEN];
	FString UserContentDir = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*ContentDir));

	PackageTreeCtrl = (wxTreeCtrl*)FindWindow( XRCID( "IDTC_PACKAGES" ) );
	check( PackageTreeCtrl != NULL );

	PackageTreeCtrl->AddRoot( TEXT("Content") );

	TArray<FString> PackageFilenames;
	appFindFilesInDirectory(PackageFilenames, *UserContentDir, TRUE, FALSE);

	// @todo - do we need to sort the filenames here or is it safe to assume the file system/manager already did it?

	for( INT p = 0 ; p < PackageFilenames.Num() ; ++p )
	{
		FString PkgName = *PackageFilenames(p);
		TArray<FString> chunks;

		PkgName.ParseIntoArray( &chunks, TEXT("\\"), TRUE );

		// Find the "content" chunk as that marks the start of the package tree

		int x = 0;
		UBOOL bFoundContent = FALSE;
		FString PathBase = TEXT("");

		for( x = 0 ; x < chunks.Num() ; ++x )
		{
			if( PathBase.Len() )
			{
				PathBase += TEXT("\\");
			}
			PathBase += chunks(x);

			if( chunks(x) == TEXT("Content") )
			{
				bFoundContent = TRUE;
				break;
			}
		}
		
		if( bFoundContent )
		{
			wxTreeItemId ParentID = PackageTreeCtrl->GetRootItem();
			FString FullPath = PathBase;

			x++;
			for( ; x < chunks.Num() ; ++x )
			{
				wxTreeItemIdValue cookie;
				wxTreeItemId child;

				FullPath += TEXT("\\");
				FullPath += chunks(x);

				child = PackageTreeCtrl->GetFirstChild( ParentID, cookie );

				while( child.IsOk() ) 
				{
					if( PackageTreeCtrl->GetItemText(child) == *chunks(x) )
					{
						break;
					}

					child = PackageTreeCtrl->GetNextChild( ParentID, cookie );
				}

				if( !child.IsOk() )
				{
					child = PackageTreeCtrl->AppendItem( ParentID, *chunks(x), -1, -1, new PackageTreePath(FullPath) );
				}

				ParentID = child;
			}
		}
	}

	PackageTreeCtrl->Expand( PackageTreeCtrl->GetRootItem() );

	FLocalizeWindow( this );
}

WxDlgOpenPackages::~WxDlgOpenPackages()
{
	FWindowUtil::SavePosSize( TEXT("DlgOpenPackages"), this );
}

void WxDlgOpenPackages::OnOK( wxCommandEvent& In )
{
	SelectedItems.Empty();

	wxArrayTreeItemIds tids;
	PackageTreeCtrl->GetSelections( tids );

	for( UINT x = 0 ; x < tids.GetCount() ; ++x )
	{
		if( PackageTreeCtrl->IsSelected( tids[x] ) && PackageTreeCtrl->GetItemData( tids[x] ) )
		{
			PackageTreePath* Path = (PackageTreePath*)PackageTreeCtrl->GetItemData( tids[x] );
			if( Path->Path.Len() )
			{
				SelectedItems.AddItem( Path->Path );
			}
		}
	}

	EndModal( wxID_OK );
}
