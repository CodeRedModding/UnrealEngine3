/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgSelectGroup.h"

BEGIN_EVENT_TABLE(WxDlgSelectGroup, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgSelectGroup::OnOK )
	EVT_UPDATE_UI( wxID_OK, WxDlgSelectGroup::UpdateUI_OkButton )
END_EVENT_TABLE()

WxDlgSelectGroup::WxDlgSelectGroup( wxWindow* InParent )
:	bShowAllPackages( FALSE ),
	RootPackage( NULL )
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, InParent, TEXT("ID_DLG_SELECTGROUP") );
	check( bSuccess );

	TreeCtrl = wxDynamicCast( FindWindow( XRCID( "IDTC_GROUPS" ) ), wxTreeCtrl );
	check( TreeCtrl != NULL );

	wxCheckBox* ShowAllCheckBox = wxDynamicCast( FindWindow( XRCID( "IDCK_SHOW_ALL" ) ), wxCheckBox );
	check( ShowAllCheckBox != NULL );
	ShowAllCheckBox->SetValue(bShowAllPackages? true: false);

	ADDEVENTHANDLER( XRCID("IDCK_SHOW_ALL"), wxEVT_COMMAND_CHECKBOX_CLICKED , &WxDlgSelectGroup::OnShowAllPackages );

	FLocalizeWindow( this );
}

int WxDlgSelectGroup::ShowModal( FString InPackageName, FString InGroupName )
{
	// Find the package objects

	RootPackage = Cast<UPackage>(GEngine->StaticFindObject( UPackage::StaticClass(), ANY_PACKAGE, *InPackageName ) );

	if( RootPackage == NULL )
	{
		bShowAllPackages = TRUE;
	}

	UpdateTree();

	return wxDialog::ShowModal();
}

/**
* Loads the tree control with a list of groups and/or packages.
*/

void WxDlgSelectGroup::UpdateTree()
{
	UPackage* SearchRoot = RootPackage;
	if( bShowAllPackages )
	{
		SearchRoot = NULL;
	}

	// Load tree control with the groups associated with this package.
	TreeCtrl->DeleteAllItems();

	// Add the search root to the tree
	const wxTreeItemId SearchRootId = TreeCtrl->AddRoot( SearchRoot ? *SearchRoot->GetName() : TEXT("Packages"), -1, -1, new WxTreeObjectWrapper( SearchRoot ) );

	// Create an array of packages that are valid to display in the dialog so that these checks don't have to be made
	// per iteration. Exclude any script or PIE packages, as well as the shader cache and transient packages.
	TArray<UPackage*> ValidPackages;
	const UPackage* TransientPackage = UObject::GetTransientPackage();
	const UPackage* LocalShaderCachePackage = GetLocalShaderCache( GRHIShaderPlatform )->GetOutermost();
	const UPackage* RefShaderCachePackage = GetReferenceShaderCache( GRHIShaderPlatform )->GetOutermost();

	for ( TObjectIterator<UPackage> PkgIter; PkgIter; ++PkgIter )
	{
		UPackage* CurPackage = *PkgIter;
		check( CurPackage );

		const UBOOL bIsScriptOrPIEPkg = CurPackage->RootPackageHasAnyFlags( PKG_ContainsScript | PKG_PlayInEditor );
		const UBOOL bIsShaderPkg = ( CurPackage == LocalShaderCachePackage || CurPackage == RefShaderCachePackage );
		const UBOOL bIsTransientPkg = ( CurPackage == TransientPackage );
		const UBOOL bIsValidPkg = !bIsScriptOrPIEPkg && !bIsShaderPkg && !bIsTransientPkg;

		// Consider a package valid if it isn't a script, PIE, shader, or transient package
		if ( bIsValidPkg )
		{
			ValidPackages.AddItem( CurPackage );
		}
	}

	// Track packages already inserted into the tree inside a mapping of Package to tree ID so parent package lookups are O(1)
	TMap<UObject*, wxTreeItemId> PackagesMap;
	PackagesMap.Set( SearchRoot, SearchRootId );

	// Continue to loop through valid packages as long as a package was added to the tree each pass.
	UBOOL bItemsAdded = TRUE;
	while ( bItemsAdded )
	{
		bItemsAdded = FALSE;
		for ( TArray<UPackage*>::TConstIterator ValidPkgIter( ValidPackages ); ValidPkgIter; ++ValidPkgIter )
		{
			UPackage* CurPackage = *ValidPkgIter;
			check( CurPackage );

			UObject* CurOuter = CurPackage->GetOuter();

			// If this package isn't already in the tree, see if its outer/parent package is
			if ( !PackagesMap.HasKey( CurPackage ) )
			{
				wxTreeItemId* OuterId = PackagesMap.Find( CurOuter );
				
				// If this package's outer is already in the tree, it is now safe to add this package as a child of the outer package in the tree
				if ( OuterId )
				{
					const wxTreeItemId NewId = TreeCtrl->AppendItem( *OuterId, *CurPackage->GetName(), -1, -1, new WxTreeObjectWrapper( CurPackage ) );
					PackagesMap.Set( CurPackage, NewId );
					bItemsAdded = TRUE;
				}
			}
		}
	}
	
	// Iterate over each item in the tree, sorting children for each one
	for ( TMap<UObject*, wxTreeItemId>::TConstIterator PackagesMapIter( PackagesMap ); PackagesMapIter; ++PackagesMapIter )
	{
		TreeCtrl->SortChildren( PackagesMapIter.Value() );
	}
	TreeCtrl->Expand( SearchRootId );

	// If a root package exists, make sure it's visible, expanded, and selected in the tree
	if ( RootPackage )
	{
		wxTreeItemId* RootPackageId = PackagesMap.Find( RootPackage );
		if ( RootPackageId )
		{
			TreeCtrl->Expand( *RootPackageId );
			TreeCtrl->EnsureVisible( *RootPackageId );
			TreeCtrl->SelectItem( *RootPackageId );
			TreeCtrl->SetFocus();
		}
	}
}

void WxDlgSelectGroup::OnOK( wxCommandEvent& In )
{
	WxTreeObjectWrapper* ItemData = static_cast<WxTreeObjectWrapper*>( TreeCtrl->GetItemData( TreeCtrl->GetSelection() ) );

	if( ItemData )
	{
		UPackage* pkg = ItemData->GetObjectChecked<UPackage>();
		FString Prefix;

		Group = TEXT("");

		while( pkg->GetOuter() )
		{
			Prefix = pkg->GetName();
			if( Group.Len() )
			{
				Prefix += TEXT(".");
			}

			Group = Prefix + Group;

			pkg = Cast<UPackage>( pkg->GetOuter() );
		}

		Package = pkg->GetName();
	}
	wxDialog::AcceptAndClose();
}

void WxDlgSelectGroup::OnShowAllPackages( wxCommandEvent& In )
{
	bShowAllPackages = In.IsChecked();

	UpdateTree();
}

/**
 * Method automatically called by wxWidgets to update the UI for the OK button. Specifically
 * disables the button whenever the user has no selection or an invalid selection.
 *
 * @param	In	Event automatically generated by wxWidgets to update the UI
 */
void WxDlgSelectGroup::UpdateUI_OkButton( wxUpdateUIEvent& In )
{
	// Disable the OK button unless the user has a valid selection, otherwise they risk a crash/undesired behavior
	UBOOL bEnableOkButton = FALSE;
	if ( TreeCtrl )
	{
		const wxTreeItemId& CurSelectedItem = TreeCtrl->GetSelection();
		if ( CurSelectedItem.IsOk() )
		{
			WxTreeObjectWrapper* CurSelectedItemData = static_cast<WxTreeObjectWrapper*>( TreeCtrl->GetItemData( CurSelectedItem ) );
			if ( CurSelectedItemData && CurSelectedItemData->GetObject<UPackage>() )
			{
				bEnableOkButton = TRUE;
			}
		}
	}
	In.Enable( bEnableOkButton == TRUE );
}
