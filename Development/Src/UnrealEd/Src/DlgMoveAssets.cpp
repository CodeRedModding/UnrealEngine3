/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgMoveAssets.h"
#include "DlgSelectGroup.h"

IMPLEMENT_COMPARE_CONSTREF( FString, DlgMoveAssets, { return appStricmp(*A,*B); } );
IMPLEMENT_COMPARE_CONSTREF( FClassMoveInfo, DlgMoveAssets, { return appStricmp(*A.ClassName,*B.ClassName); } );

/** @return		Index into ClassRelocation of the specified class, or INDEX_NONE if not found. */
static INT FindRelocationIndex(const TCHAR* InClassName)
{
	for( INT ClassIndex = 0 ; ClassIndex < GUnrealEd->ClassRelocationInfo.Num() ; ++ClassIndex )
	{
		if ( GUnrealEd->ClassRelocationInfo(ClassIndex).ClassName == InClassName )
		{
			return ClassIndex;
		}
	}
	return INDEX_NONE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	WxDlgCreateMoveInfo
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Dialog for creating new class-specific asset move information. */
class WxDlgCreateMoveInfo : public wxDialog
{
public:
	WxDlgCreateMoveInfo()	
	{
		const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_CreateClassMoveInfo") );
		check( bSuccess );

		ClassCombo = (wxComboBox*)FindWindow( XRCID( "ID_ClassCombo" ) );
		check( ClassCombo );
		PackageCombo = (wxComboBox*)FindWindow( XRCID( "ID_PackageCombo" ) );
		check( PackageCombo );
		GroupTextCtrl = (wxTextCtrl*)FindWindow( XRCID( "ID_GroupTextCtrl" ) );
		check( GroupTextCtrl );

		FWindowUtil::LoadPosSize( TEXT("DlgCreateClassMoveInfo"), this, -1, -1, 363, 170 );
		FLocalizeWindow( this );
	}

	virtual ~WxDlgCreateMoveInfo()
	{
		FWindowUtil::SavePosSize( TEXT("DlgCreateClassMoveInfo"), this );
	}

	int ShowModal()
	{
		// Create the list of available resource types.
		TArray<FString> ResourceNames;
		for( TObjectIterator<UClass> ItC ; ItC ; ++ItC )
		{
			const UBOOL bIsAllType = (*ItC == UGenericBrowserType_All::StaticClass());
			const UBOOL bIsCustomType = (*ItC == UGenericBrowserType_Custom::StaticClass());
			if( !bIsAllType && !bIsCustomType && ItC->IsChildOf(UGenericBrowserType::StaticClass()) && !(ItC->ClassFlags&CLASS_Abstract) )
			{
				const FString ClassName = ItC->GetName();
				if ( ClassName.StartsWith(TEXT("GenericBrowserType_")) )
				{
					ResourceNames.AddItem( ClassName.Mid(19) );
				}
				else
				{
					ResourceNames.AddItem( ClassName );
				}
			}
		}

		if ( ResourceNames.Num() > 0 )
		{
			Sort<USE_COMPARE_CONSTREF(FString,DlgMoveAssets)>( ResourceNames.GetTypedData(), ResourceNames.Num() );
			ClassCombo->Freeze();
			ClassCombo->Clear();
			for ( INT ClassIndex = 0 ; ClassIndex < ResourceNames.Num() ; ++ClassIndex )
			{
				ClassCombo->Append( *ResourceNames(ClassIndex) );
			}
			ClassCombo->SetSelection(0);
			ClassCombo->Thaw();
		}
		return wxDialog::ShowModal();
	}

	virtual bool Validate()
	{
		const FString TempFinalClassName = ClassCombo->GetValue().c_str();
		const FString TempFinalPackageName = PackageCombo->GetValue().c_str();
		const FString TempFinalGroupName = GroupTextCtrl->GetValue().c_str();

		FString Reason;
		// Verify the package name is empty or a valid name.
		if ( !FIsValidGroupName(*TempFinalPackageName, Reason) )
		{
			appMsgf( AMT_OK, *Reason );
			return 0;
		}
		// Verify the group name is empty or a valid name.
		if ( !FIsValidGroupName(*TempFinalGroupName, Reason, TRUE) )
		{
			appMsgf( AMT_OK, *Reason );
			return 0;
		}

		FinalClassName = TempFinalClassName;
		FinalPackageName = TempFinalPackageName;
		FinalGroupName = TempFinalGroupName;

		return 1;
	}

	wxComboBox* ClassCombo;
	wxComboBox* PackageCombo;
	wxTextCtrl* GroupTextCtrl;

	FString FinalClassName;
	FString FinalPackageName;
	FString FinalGroupName;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	WxPkgGrpNameTxtCtrl
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * A control extending WxPkgGrpNameCtrl designed to accept
 * texture path as well as package/group(s)/name from the user.
 */
class WxPkgGrpNameTxtCtrl : public WxPkgGrpNameCtrl
{
public:
	WxPkgGrpNameTxtCtrl( wxWindow* InParent, wxWindowID InID, wxSizer* InParentSizer);

	/** Fills the output list with the set of active class relocation infos. */
	void GetActiveClassRelocationInfo(TArray<FClassMoveInfo>& OutActiveClassRelocationInfo) const
	{
		check( MoveInfoCheckList );
		OutActiveClassRelocationInfo.Empty();
		for ( UINT Index = 0 ; Index < MoveInfoCheckList->GetCount() ; ++Index )
		{
			// If the list item is checked . . .
			if ( MoveInfoCheckList->IsChecked(Index) )
			{
				// Use the class name to index into the global relocation list.
				const FString ClassName( MoveInfoCheckList->GetString(Index) );
				const INT RelocationIndex = FindRelocationIndex( *ClassName );
				if ( RelocationIndex != INDEX_NONE )
				{
					const FClassMoveInfo& SrcCMI = GUnrealEd->ClassRelocationInfo(RelocationIndex);
					OutActiveClassRelocationInfo.AddItem( SrcCMI );
				}
			}
		}
	}

	void SetOKToAllButton(wxButton* InButton)
	{
		ButtonOKToAll = InButton;
		SetOKToAllState();
	}

	void SetTreatNameAsSuffix(UBOOL bEnableTreatNameAsSuffix)
	{
		TreatNameAsSuffixCheck->SetValue( bEnableTreatNameAsSuffix ? true : false );
		OnTreatNameAsSuffixCheckChanged();
	}

	wxStaticText *IncludeRefsLabel;
	wxCheckBox *IncludeRefsCheck;
	wxStaticText *TreatNameAsSuffixLabel;
	wxCheckBox *TreatNameAsSuffixCheck;
	wxCheckBox *SavePackagesCheck;
	wxCheckBox *CheckoutPackagesCheck;
	wxButton* ButtonOKToAll;

	wxStaticText* MoveInfoLabel;
	wxCheckListBox* MoveInfoCheckList;
	WxBitmapButton* CreateNewMoveInfoButton;
	wxStaticText* NewPackageLabel;
	wxComboBox* NewPackageCombo;
	wxStaticText* NewGroupLabel;
	wxTextCtrl* NewGroupEdit;
	WxBitmapButton* NewGroupBrowserButton;

private:
	/** Populates the checklist with class move info. */
	void UpdateMoveInfoCheckList()
	{
		Sort<USE_COMPARE_CONSTREF(FClassMoveInfo,DlgMoveAssets)>( GUnrealEd->ClassRelocationInfo.GetTypedData(), GUnrealEd->ClassRelocationInfo.Num() );

		MoveInfoCheckList->Freeze();
		MoveInfoCheckList->Clear();
		{
			for ( INT Index = 0 ; Index < GUnrealEd->ClassRelocationInfo.Num() ; ++Index )
			{
				const INT NewIndex = MoveInfoCheckList->Append( *GUnrealEd->ClassRelocationInfo(Index).ClassName );
				if ( GUnrealEd->ClassRelocationInfo(Index).bActive )
				{
					MoveInfoCheckList->Check( NewIndex );
				}
			}
		}
		MoveInfoCheckList->Thaw();
	}

	void OnMoveInfoBrowse( wxCommandEvent& In )
	{
		const FString PkgName = NewPackageCombo->GetValue().c_str();
		const FString GroupName = NewGroupEdit->GetValue().c_str();

		WxDlgSelectGroup Dlg( this );
		if( Dlg.ShowModal( PkgName, GroupName ) == wxID_OK )
		{
			NewPackageCombo->SetValue( *Dlg.Package );
			NewGroupEdit->SetValue( *Dlg.Group );
		}
	}

	void OnIncludeRefsCheck(wxCommandEvent& In)
	{
		const bool bIsChecked = IncludeRefsCheck->IsChecked();
		MoveInfoLabel->Enable( bIsChecked );
		MoveInfoCheckList->Enable( bIsChecked );
		CreateNewMoveInfoButton->Enable( bIsChecked );

		// Disable manual 'Name as suffix?' toggling.
		TreatNameAsSuffixLabel->Enable( !bIsChecked );
		TreatNameAsSuffixCheck->Enable( !bIsChecked );

		if ( !bIsChecked )
		{
			// The "Include References?" checkbox isn't ticked; disable all controls.
			ClearSelectedMoveInfo();
		}
		else
		{
			// If we're including references, force the name field to be treated as a suffix.
			SetTreatNameAsSuffix( TRUE );

			// The "Include References?" checkbox is ticked; enable controls if
			// something is selected in the class checklist.
			const INT SelectedIndex = MoveInfoCheckList->GetSelection();
			if ( SelectedIndex == wxNOT_FOUND )
			{
				ClearSelectedMoveInfo();
			}
			else
			{
				SetSelectedMoveInfo( SelectedIndex );
			}
		}
	}

	void SetOKToAllState()
	{
		if ( ButtonOKToAll )
		{
			ButtonOKToAll->Enable( true );
		}
	}

	void OnTreatNameAsSuffixCheckChanged()
	{
		SetOKToAllState();

		const bool bEnableTreatNameAsSuffix = TreatNameAsSuffixCheck->IsChecked();
		if ( NameLabel && NameEdit )
		{
			// Set the name label based on whether the name is a suffix or a full name.
			const FString NewNameLabelString =
				bEnableTreatNameAsSuffix ? 
				*LocalizeUnrealEd("NameSuffix"):
				*LocalizeUnrealEd("NewName");
			NameLabel->SetLabel( *NewNameLabelString );

			if ( bEnableTreatNameAsSuffix )
			{
				NameEdit->SetValue( TEXT("_Dup") );
			}
		}
	}

	void OnTreatNameAsSuffixCheck(wxCommandEvent& In)
	{
		OnTreatNameAsSuffixCheckChanged();
	}

	void OnCreateNewMoveInfo(wxCommandEvent& In)
	{
		WxDlgCreateMoveInfo Dlg;
		if( Dlg.ShowModal() == wxID_OK )
		{
			INT NewSelectionIndex = INDEX_NONE;
			const INT RelocationIndex = FindRelocationIndex( *Dlg.FinalClassName );
			if ( RelocationIndex != INDEX_NONE )
			{
				const FString ClassAlreadyExists = FString::Printf(LocalizeSecure(LocalizeUnrealEd("ClassMoveInfoAlreadyExistsQ"),*Dlg.FinalClassName));
				if ( appMsgf(AMT_YesNo,*ClassAlreadyExists) )
				{
					// User approved the class info update.
					NewSelectionIndex = RelocationIndex;
					FClassMoveInfo& CMI = GUnrealEd->ClassRelocationInfo(RelocationIndex);
					CMI.PackageName = Dlg.FinalPackageName;
					CMI.GroupName = Dlg.FinalGroupName;
				}
			}
			else
			{
				// The class is new -- add it!
				FClassMoveInfo CMI(EC_EventParm);
				CMI.ClassName = Dlg.FinalClassName;
				CMI.PackageName = Dlg.FinalPackageName;
				CMI.GroupName = Dlg.FinalGroupName;
				CMI.bActive = TRUE;
				GUnrealEd->ClassRelocationInfo.AddItem( CMI );

				MoveInfoCheckList->Freeze();
				NewSelectionIndex = MoveInfoCheckList->Append( *Dlg.FinalClassName );
				MoveInfoCheckList->Check( NewSelectionIndex, true );
				MoveInfoCheckList->Thaw();
			}

			// Select the class entry that was updated/modified.
			if ( NewSelectionIndex >= 0 )
			{
				SetSelectedMoveInfo( NewSelectionIndex );
			}
		}
	}

	void OnMoveInfoListChecked(wxCommandEvent& In)
	{
		const INT Index = In.GetSelection();
		GUnrealEd->ClassRelocationInfo(Index).bActive = MoveInfoCheckList->IsChecked(Index);
		SetSelectedMoveInfo( Index );
	}

	void OnMoveInfoListSelected(wxCommandEvent& In)
	{
		const INT Index= In.GetSelection();
		SetSelectedMoveInfo( Index );
	}

	void OnMoveInfoPackageComboText(wxCommandEvent& In)
	{
		const INT SelectedMoveInfoIndex = MoveInfoCheckList->GetSelection();
		GUnrealEd->ClassRelocationInfo(SelectedMoveInfoIndex).PackageName = NewPackageCombo->GetValue();
	}

	void OnMoveInfoGroupEditText(wxCommandEvent& In)
	{
		const INT SelectedMoveInfoIndex = MoveInfoCheckList->GetSelection();
		GUnrealEd->ClassRelocationInfo(SelectedMoveInfoIndex).GroupName = NewGroupEdit->GetValue();
	}

	void ClearSelectedMoveInfo()
	{
		MoveInfoCheckList->Select( wxNOT_FOUND );
		NewPackageLabel->Enable( false );
		NewPackageCombo->Enable( false );
		NewGroupLabel->Enable( false );
		NewGroupEdit->Enable( false );
		NewGroupBrowserButton->Enable( false );
	}

	void SetSelectedMoveInfo(INT Index)
	{
		MoveInfoCheckList->Select( Index );
		const INT RelocationIndex = FindRelocationIndex( MoveInfoCheckList->GetString(Index) );
		check( RelocationIndex != INDEX_NONE );
		NewPackageLabel->Enable( true );
		NewPackageCombo->SetValue( *GUnrealEd->ClassRelocationInfo(Index).PackageName );
		NewPackageCombo->Enable( true );
		NewGroupLabel->Enable( true );
		NewGroupEdit->SetValue( *GUnrealEd->ClassRelocationInfo(Index).GroupName );
		NewGroupEdit->Enable( true );
		NewGroupBrowserButton->Enable( true );
	}

	DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE( WxPkgGrpNameTxtCtrl, WxPkgGrpNameCtrl )
	EVT_CHECKLISTBOX(ID_AssetClassMoveInfo, WxPkgGrpNameTxtCtrl::OnMoveInfoListChecked)
	EVT_LISTBOX(ID_AssetClassMoveInfo, WxPkgGrpNameTxtCtrl::OnMoveInfoListSelected)
	EVT_TEXT(ID_ClassMovePackageCombo, WxPkgGrpNameTxtCtrl::OnMoveInfoPackageComboText)
	EVT_TEXT(ID_ClassMoveGroupEdit, WxPkgGrpNameTxtCtrl::OnMoveInfoGroupEditText)
	EVT_BUTTON(ID_CreateNewMoveInfo, WxPkgGrpNameTxtCtrl::OnCreateNewMoveInfo)
	EVT_BUTTON(ID_ClassMoveGroupBrowse, WxPkgGrpNameTxtCtrl::OnMoveInfoBrowse)
	EVT_CHECKBOX(ID_IncludedRefsCheck, WxPkgGrpNameTxtCtrl::OnIncludeRefsCheck)
	EVT_CHECKBOX(ID_TreatNameAsSuffixCheck, WxPkgGrpNameTxtCtrl::OnTreatNameAsSuffixCheck)
END_EVENT_TABLE()

WxPkgGrpNameTxtCtrl::WxPkgGrpNameTxtCtrl(wxWindow* InParent, wxWindowID InID, wxSizer* InParentSizer)
	:	WxPkgGrpNameCtrl( InParent, InID, InParentSizer, TRUE )
	,	ButtonOKToAll( NULL )
{
	FlexGridSizer->Add( 5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// "Treat name as suffix?" checkbox
	TreatNameAsSuffixLabel = new wxStaticText( this, wxID_STATIC, TEXT("EnableNameSuffixQ"), wxDefaultPosition, wxDefaultSize, 0 );
	FlexGridSizer->Add( TreatNameAsSuffixLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5 );

	TreatNameAsSuffixCheck = new wxCheckBox( this, ID_TreatNameAsSuffixCheck, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	FlexGridSizer->Add( TreatNameAsSuffixCheck, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );

	FlexGridSizer->Add( 5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// "Save packages?" checkbox
	wxStaticText* SavePacakgesLabel = new wxStaticText( this, wxID_STATIC, TEXT("SavePackagesQ"), wxDefaultPosition, wxDefaultSize, 0 );
	FlexGridSizer->Add( SavePacakgesLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5 );

	SavePackagesCheck = new wxCheckBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	FlexGridSizer->Add( SavePackagesCheck, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );

	FlexGridSizer->Add( 5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// "Checkout packages?" checkbox
	wxStaticText* CheckoutPacakgesLabel = new wxStaticText( this, wxID_STATIC, TEXT("CheckoutPackagesQ"), wxDefaultPosition, wxDefaultSize, 0 );
	FlexGridSizer->Add( CheckoutPacakgesLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5 );

	CheckoutPackagesCheck = new wxCheckBox( this, -1, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	FlexGridSizer->Add( CheckoutPackagesCheck, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );

	FlexGridSizer->Add( 5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// "Include refs?" checkbox
	IncludeRefsLabel = new wxStaticText( this, wxID_STATIC, TEXT("References"), wxDefaultPosition, wxDefaultSize, 0 );
	FlexGridSizer->Add( IncludeRefsLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5 );

	IncludeRefsCheck = new wxCheckBox( this, ID_IncludedRefsCheck, TEXT(""), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
	FlexGridSizer->Add( IncludeRefsCheck, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );

	FlexGridSizer->Add( 5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// Add the checklist.
	MoveInfoLabel = new wxStaticText( this, wxID_STATIC, TEXT("ClassRelocation"), wxDefaultPosition, wxDefaultSize, 0 );
	MoveInfoLabel->Enable(false);
	FlexGridSizer->Add(MoveInfoLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	MoveInfoCheckList = new wxCheckListBox(this, ID_AssetClassMoveInfo, wxDefaultPosition, wxDefaultSize, 0, NULL, wxLB_SINGLE);
	MoveInfoCheckList->Enable(false);
	UpdateMoveInfoCheckList();
	FlexGridSizer->Add( MoveInfoCheckList, 1, wxEXPAND|wxALL, 5 );

	CreateNewMoveInfoButton = new WxBitmapButton( this, ID_CreateNewMoveInfo, BrowseB, wxDefaultPosition, wxSize(24, 24) );
	CreateNewMoveInfoButton->Enable(false);
	FlexGridSizer->Add( CreateNewMoveInfoButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// Add the package target.
	{
		NewPackageLabel = new wxStaticText( this, wxID_STATIC, TEXT("TargetPackage"), wxDefaultPosition, wxDefaultSize, 0 );
		NewPackageLabel->Enable(false);
		FlexGridSizer->Add(NewPackageLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

		NewPackageCombo = new wxComboBox( this, ID_ClassMovePackageCombo, TEXT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN|wxCB_SORT );
		NewPackageCombo->Enable(false);
		FlexGridSizer->Add( NewPackageCombo, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );
		TArray<UPackage*> Packages;
		GEditor->GetPackageList( &Packages, NULL );
		for( INT PackageIndex = 0 ; PackageIndex < Packages.Num() ; ++PackageIndex )
		{
			NewPackageCombo->Append( *Packages(PackageIndex)->GetName(), Packages(PackageIndex) );
		}
	}

	// Add the group target.
	FlexGridSizer->Add( 5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// Texture Group edit box and browse button
	NewGroupLabel = new wxStaticText( this, wxID_STATIC, TEXT("TargetGroup"), wxDefaultPosition, wxDefaultSize, 0 );
	NewGroupLabel->Enable(false);
	FlexGridSizer->Add( NewGroupLabel, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5 );

	NewGroupEdit = new wxTextCtrl( this, ID_ClassMoveGroupEdit, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
	NewGroupEdit->Enable(false);
	FlexGridSizer->Add( NewGroupEdit, 0, wxALIGN_LEFT|wxGROW|wxALL, 5 );

	NewGroupBrowserButton = new WxBitmapButton( this, ID_ClassMoveGroupBrowse, BrowseB, wxDefaultPosition, wxSize(24, 24) );
	NewGroupBrowserButton->Enable(false);
	FlexGridSizer->Add( NewGroupBrowserButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	// Finally, localize the window.
	FLocalizeWindow( this );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	WxDlgMoveAssets
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE( WxDlgMoveAssets, wxDialog )
	EVT_BUTTON(ID_OK_ALL, WxDlgMoveAssets::OnOKToAll)
END_EVENT_TABLE()

WxDlgMoveAssets::WxDlgMoveAssets()
	:	wxDialog( GApp->EditorFrame, -1, *LocalizeUnrealEd("MoveWithReferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER )
	,	bIncludeRefs( FALSE )
	,	bTreatNameAsSuffix( FALSE )
	,	bCheckoutPackages( FALSE )
	,	bSavePackages( FALSE )
{
	wxBoxSizer* UberSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		wxStaticBoxSizer* InfoBoxSizer = new wxStaticBoxSizer(wxVERTICAL, this, TEXT("Info"));
		{
			PGNCtrl = new WxPkgGrpNameTxtCtrl( this, -1, NULL );
			InfoBoxSizer->Add( PGNCtrl, 1, wxEXPAND|wxALL, 5 );
			//PGNCtrl->Show();
		}
		UberSizer->Add( InfoBoxSizer, 1, wxEXPAND | wxALL, 5 );

		// OK/Cancel Buttons
		wxSizer *ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			wxButton* ButtonOK = new wxButton(this, wxID_OK, *LocalizeUnrealEd("OK"));
			ButtonOK->SetDefault();
			ButtonSizer->Add(ButtonOK, 0, wxEXPAND | wxALL, 5);

			wxButton* ButtonOKToAll = new wxButton(this, ID_OK_ALL, *LocalizeUnrealEd("OKToAll"));
			ButtonSizer->Add(ButtonOKToAll, 0, wxEXPAND | wxALL, 5);
			PGNCtrl->SetOKToAllButton( ButtonOKToAll );

			wxButton* ButtonCancel = new wxButton(this, wxID_CANCEL, *LocalizeUnrealEd("Cancel"));
			ButtonSizer->Add(ButtonCancel, 0, wxEXPAND | wxALL, 5);
		}
		UberSizer->Add( ButtonSizer, 0, wxALL, 5);

	}

	SetSizer( UberSizer );
	SetAutoLayout( true );

	FWindowUtil::LoadPosSize( TEXT("DlgMove"), this, -1, -1, 497, 408 );
}

WxDlgMoveAssets::~WxDlgMoveAssets()
{
	FWindowUtil::SavePosSize( TEXT("DlgMove"), this );
}

int WxDlgMoveAssets::ShowModal( const FString& InPackage, const FString& InGroup, const FString& InName, UBOOL bAllowPackageRename )
{
	NewPackage = InPackage;
	NewGroup = InGroup;
	NewName = InName;

	PGNCtrl->PkgCombo->SetValue( *InPackage );
	PGNCtrl->GrpEdit->SetValue( *InGroup );
	PGNCtrl->NameEdit->SetValue( *InName );
	PGNCtrl->EnablePackageCombo( bAllowPackageRename );

	return wxDialog::ShowModal();
}

/** Sets whether or not the 'name' field can be used as a name suffix. */
void WxDlgMoveAssets::ConfigureNameField(UBOOL bEnableTreatNameAsSuffix)
{
	if ( PGNCtrl )
	{
		PGNCtrl->SetTreatNameAsSuffix( bEnableTreatNameAsSuffix );
	}
}

bool WxDlgMoveAssets::Validate()
{
	NewPackage = PGNCtrl->PkgCombo->GetValue();
	NewGroup = PGNCtrl->GrpEdit->GetValue();
	NewName = PGNCtrl->NameEdit->GetValue();

	bIncludeRefs = PGNCtrl->IncludeRefsCheck->GetValue();
	bTreatNameAsSuffix = PGNCtrl->TreatNameAsSuffixCheck->GetValue();
	bSavePackages = PGNCtrl->SavePackagesCheck->GetValue();
	bCheckoutPackages = PGNCtrl->CheckoutPackagesCheck->GetValue();

	FString Reason;
	if( !FIsValidGroupName(*NewPackage, Reason)
	||	!FIsValidGroupName(*NewGroup, Reason, TRUE)
	||	!FIsValidObjectName( *NewName, Reason ) )
	{
		appMsgf( AMT_OK, *Reason );
		return 0;
	}

	return 1;
}

/**
 * Determines the depth in the class heirarchy of a class from a named class.
 *
 * @return	Class distance (0 match, 1 direct child, 2 grandchild, etc), or -1 if not of the named type.
 */
static INT DistanceFromClass(UClass* Class, const TCHAR* ParentClassName)
{
	INT Dist = 0;
	for (UClass* TempClass = Class; TempClass != NULL; TempClass = TempClass->GetSuperClass())
	{
		if( TempClass->GetName() == ParentClassName )
		{
			return Dist;
		}
		++Dist;
	}
	return -1;
}

/**
 * Determines a class-specific target package and group for the specified object.
 */
void WxDlgMoveAssets::DetermineClassPackageAndGroup(const UObject* InObject,
													FString& OutPackage,
													FString& OutGroup) const
{
	check( PGNCtrl );

	TArray<FClassMoveInfo> ActiveClassRelocationInfo;
	PGNCtrl->GetActiveClassRelocationInfo( ActiveClassRelocationInfo );

	// Compute the 'closest' parent class in the object's inheritance tree.
	INT MinClassDist = INT_MAX;
	INT MinClassIndex = INDEX_NONE;
	for( INT ClassIndex = 0 ; ClassIndex < ActiveClassRelocationInfo.Num() ; ++ClassIndex )
	{
		const FClassMoveInfo& CurClassRelocation = ActiveClassRelocationInfo(ClassIndex);
		const INT ClassDist = DistanceFromClass( InObject->GetClass(), *CurClassRelocation.ClassName );
		if ( ClassDist != -1 && MinClassDist > ClassDist )
		{
			MinClassDist = ClassDist;
			MinClassIndex = ClassIndex;
		}
	}

	if ( MinClassIndex != INDEX_NONE )
	{
		OutPackage = ActiveClassRelocationInfo(MinClassIndex).PackageName;
		OutGroup = ActiveClassRelocationInfo(MinClassIndex).GroupName;
	}
	else
	{
		// Return empty strings to indicate that no class-specific targets were specified for this type.
		OutPackage = TEXT("");
		OutGroup= TEXT("");
	}
}
void WxDlgMoveAssets::OnOKToAll(wxCommandEvent& In)
{
	if ( Validate() )
	{
		EndModal( ID_OK_ALL );
	}
	else
	{
		EndModal( wxID_CANCEL );
	}
}
