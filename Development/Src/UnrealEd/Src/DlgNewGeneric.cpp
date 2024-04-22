/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "..\..\Launch\Resources\resource.h"
#include "PropertyWindow.h"
#include "UnObjectTools.h"
#include "UnPackageTools.h"

/*-----------------------------------------------------------------------------
	WxDlgNewGeneric.
-----------------------------------------------------------------------------*/
/**
 * Adds the specific font controls to the dialog
 */
void WxDlgNewGeneric::AppendFontControls(void)
{
	FontSizer = new wxBoxSizer(wxHORIZONTAL);
	FactorySizer->Add(FontSizer, 0, wxGROW|wxALL, 5);

	ChooseFontButton = new wxButton( this, ID_CHOOSE_FONT, *LocalizeUnrealEd( "ChooseFont" ), wxDefaultPosition, wxDefaultSize, 0 );
	FontSizer->Add(ChooseFontButton, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	FontStaticText = new wxStaticText( this, wxID_STATIC, TEXT("This is some sample text"), wxDefaultPosition, wxDefaultSize, 0 );
	FontSizer->Add(FontStaticText, 1, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

	FontSizer->Layout();
	FactorySizer->Layout();
	// Add the size to the overall dialog size
	wxSize FontSize = FontSizer->GetSize();
	wxSize OurSize = GetSize();
	SetSize(wxSize(OurSize.GetWidth(),OurSize.GetHeight() + FontSize.GetHeight()));
}

/**
 * Removes the font controls from the dialog
 */
void WxDlgNewGeneric::RemoveFontControls(void)
{
	if (FontSizer != NULL)
	{
		// Shrink the dialog size first
		wxSize FontSize = FontSizer->GetSize();
		wxSize OurSize = GetSize();
		SetSize(wxSize(OurSize.GetWidth(),OurSize.GetHeight() - FontSize.GetHeight()));
		// Now remove us from the dialog
		FactorySizer->Detach(FontSizer);
		delete FontStaticText;
		FontStaticText = NULL;
		delete ChooseFontButton;
		ChooseFontButton = NULL;
		delete FontSizer;
		FontSizer = NULL;
		// Force a layout
		FactorySizer->Layout();
	}
}

/**
 * Lets the user select the font that is going to be imported
 */
void WxDlgNewGeneric::OnChooseFont(wxCommandEvent& In)
{
	wxFontDialog Dialog(this);
	// Show the dialog to let them choose the font
	if (Dialog.ShowModal() == wxID_OK)
	{
		wxFontData RetData = Dialog.GetFontData();
		// Get the font they selected
		wxFont Font = RetData.GetChosenFont();
		// Set the static text to use this font
		FontStaticText->SetFont(Font);
		// Force a visual refresh
		FontSizer->Layout();
		// Now update the factory with the latest values
		UTrueTypeFontFactory* TTFactory = Cast<UTrueTypeFontFactory>(Factory);
		if (TTFactory != NULL)
		{
			if( TTFactory->ImportOptions == NULL )
			{
				TTFactory->SetupFontImportOptions();
			}
			TTFactory->ImportOptions->Data.FontName = (const TCHAR*)Font.GetFaceName();
			TTFactory->ImportOptions->Data.Height = Font.GetPointSize();
			TTFactory->ImportOptions->Data.bEnableUnderline = Font.GetUnderlined();
			TTFactory->ImportOptions->Data.bEnableItalic = Font.GetStyle() == wxITALIC;
			TTFactory->ImportOptions->Data.bEnableBold = ( Font.GetWeight() == wxBOLD );
		}
		// Update the changed properties
		PropertyWindow->SetObject( NULL, EPropertyWindowFlags::Sorted);
		PropertyWindow->SetObject( TTFactory, EPropertyWindowFlags::Sorted );
		PropertyWindow->ExpandItem( "ImportOptions" );
		PropertyWindow->ExpandItem( "Data" );
		PropertyWindow->Raise();
	}
}

void WxDlgNewGeneric::OnFactorySelChange(wxCommandEvent& In)
{
	UClass* Class = (UClass*)FactoryCombo->GetClientData( FactoryCombo->GetSelection() );

	Factory = ConstructObject<UFactory>( Class );

	PropertyWindow->SetObject( Factory, EPropertyWindowFlags::Sorted );
	PropertyWindow->Raise();

	// Always attempt to remove the font controls, as a navigation from a
	// font factory to another will otherwise cause the controls to be created twice
	RemoveFontControls();

	// Append our font controls if this is a font factory
	if (Factory && Factory->GetClass()->IsChildOf(UFontFactory::StaticClass()))
	{
		// For fonts, we cheat and display the ImportOptions sub-object instead of the root factory object
		UTrueTypeFontFactory* FontFactory = Cast<UTrueTypeFontFactory>( Factory );
		check( FontFactory != NULL );
		FontFactory->SetupFontImportOptions();
		PropertyWindow->SetObject( FontFactory, EPropertyWindowFlags::Sorted );
		PropertyWindow->ExpandItem( "ImportOptions" );
		PropertyWindow->ExpandItem( "Data" );

		AppendFontControls();
	}
	Refresh();
}

BEGIN_EVENT_TABLE(WxDlgNewGeneric, wxDialog)
	EVT_COMBOBOX( IDCB_FACTORY, WxDlgNewGeneric::OnFactorySelChange )
	EVT_BUTTON( ID_CHOOSE_FONT, WxDlgNewGeneric::OnChooseFont )
END_EVENT_TABLE()

/**
 * Creates the dialog box for choosing a factory and creating a new resource
 */
WxDlgNewGeneric::WxDlgNewGeneric() :
	wxDialog(NULL,-1,wxString(TEXT("New")))
{
	CreatedObject = NULL;
	FontStaticText = NULL;
	ChooseFontButton = NULL;
	FontSizer = NULL;
	// Set our initial size
	SetSize(500,500);
    wxFlexGridSizer* ItemFlexGridSizer2 = new wxFlexGridSizer(1, 2, 0, 0);
    ItemFlexGridSizer2->AddGrowableRow(0);
    ItemFlexGridSizer2->AddGrowableCol(0);
    SetSizer(ItemFlexGridSizer2);

    wxBoxSizer* ItemBoxSizer3 = new wxBoxSizer(wxVERTICAL);
    ItemFlexGridSizer2->Add(ItemBoxSizer3, 1, wxGROW|wxGROW|wxALL, 5);

    wxStaticBox* ItemStaticBoxSizer4Static = new wxStaticBox(this, wxID_ANY, TEXT("Info"));
    wxStaticBoxSizer* ItemStaticBoxSizer4 = new wxStaticBoxSizer(ItemStaticBoxSizer4Static, wxHORIZONTAL);
    ItemBoxSizer3->Add(ItemStaticBoxSizer4, 0, wxGROW|wxALL, 5);

	PGNSizer = new wxBoxSizer(wxHORIZONTAL);
	PGNCtrl = new WxPkgGrpNameCtrl( this, ID_PKGGRPNAME, PGNSizer, TRUE );
	PGNCtrl->SetSizer(PGNSizer);
	PGNCtrl->Show();
	PGNCtrl->SetAutoLayout(true);
    ItemStaticBoxSizer4->Add(PGNCtrl, 1, wxGROW, 5);

    wxStaticBox* ItemStaticBoxSizer6Static = new wxStaticBox(this, wxID_ANY, TEXT("Factory"));
    FactorySizer = new wxStaticBoxSizer(ItemStaticBoxSizer6Static, wxVERTICAL);
    ItemBoxSizer3->Add(FactorySizer, 1, wxGROW|wxALL, 5);

    wxBoxSizer* ItemBoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
    FactorySizer->Add(ItemBoxSizer7, 0, wxGROW|wxALL, 5);

    wxStaticText* ItemStaticText8 = new wxStaticText( this, wxID_STATIC, TEXT("Factory"), wxDefaultPosition, wxDefaultSize, 0 );
    ItemBoxSizer7->Add(ItemStaticText8, 0, wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

    wxString* ItemComboBox9Strings = NULL;
	FactoryCombo = new WxComboBox( this, IDCB_FACTORY, TEXT(""), wxDefaultPosition, wxSize(200, -1), 0, ItemComboBox9Strings, wxCB_READONLY | wxCB_SORT );
    ItemBoxSizer7->Add(FactoryCombo, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticBox* ItemStaticBoxSizer10Static = new wxStaticBox(this, wxID_ANY, TEXT("Options"));
    wxStaticBoxSizer* ItemStaticBoxSizer10 = new wxStaticBoxSizer(ItemStaticBoxSizer10Static, wxVERTICAL);
    FactorySizer->Add(ItemStaticBoxSizer10, 1, wxGROW|wxALL, 5);


    wxBoxSizer* ItemBoxSizer15 = new wxBoxSizer(wxVERTICAL);
    ItemFlexGridSizer2->Add(ItemBoxSizer15, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxButton* ItemButton16 = new wxButton( this, wxID_OK, TEXT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    ItemButton16->SetDefault();
    ItemBoxSizer15->Add(ItemButton16, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxButton* ItemButton17 = new wxButton( this, wxID_CANCEL, TEXT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    ItemBoxSizer15->Add(ItemButton17, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	Factory = NULL;

	FLocalizeWindow( this );

	//Must be after flocalize to maintain search string
	// Insert the property window inside the options
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, GUnrealEd );
    ItemStaticBoxSizer10->Add(PropertyWindow, 1, wxGROW|wxALL, 5);
}

// DefaultFactoryClass is an optional parameter indicating the default factory to select in combo box
int WxDlgNewGeneric::ShowModal(const FString& InPackage, const FString& InGroup, UClass* DefaultFactoryClass, TArray< UGenericBrowserType* >* InBrowsableObjectTypeList )
{
	Package = InPackage;
	Group = InGroup;
	
	if( InBrowsableObjectTypeList != NULL )
	{
		BrowsableObjectTypeList = *InBrowsableObjectTypeList;
	}

	PGNCtrl->PkgCombo->SetValue( *Package );
	PGNCtrl->GrpEdit->SetValue( *Group );

	// Find the factories that create new objects and add to combo box
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(UFactory::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			UClass* FactoryClass = *It;
			Factory = ConstructObject<UFactory>( FactoryClass );
			UFactory* DefFactory = (UFactory*)FactoryClass->GetDefaultObject();
			if ( Factory->bCreateNew && Factory->ValidForCurrentGame() )
			{
				FactoryCombo->Append( *DefFactory->Description, FactoryClass );
			}
		}
	}

	Factory = NULL;


	// Select the incoming 'default factory class' in our list if we can find it
	INT DefIndex = INDEX_NONE;
	if( DefaultFactoryClass != NULL )
	{
		UFactory* DefaultFactoryClassDefFactory = ( UFactory* )DefaultFactoryClass->GetDefaultObject();
		if( DefaultFactoryClassDefFactory != NULL )
		{
			DefIndex = FactoryCombo->FindString( *DefaultFactoryClassDefFactory->Description );
		}
	}

	// If no default was specified, we just select the first one.
	if(DefIndex == INDEX_NONE)
	{
		DefIndex = 0;
	}

	FactoryCombo->SetSelection( DefIndex );	

	wxCommandEvent DummyEvent;
	OnFactorySelChange( DummyEvent );

	PGNCtrl->NameEdit->SetFocus();

	return wxDialog::ShowModal();
}

// Fetches the package and outermost package, given the name/group, returns false if failed
bool WxDlgNewGeneric::GetPackages( const FString& rPackageIn, const FString& rGroupIn, UPackage*& prPkgOut, UPackage*& prOtmPkgOut)
{
	// Find (or create!) the desired package for this object
	UPackage* Pkg = UObject::CreatePackage(NULL,*rPackageIn);
	if( rGroupIn.Len() )
	{
		Pkg = GEngine->CreatePackage(Pkg,*rGroupIn);
	}

	// Disallow creating new objects in cooked packages.
	UPackage* OutermostPkg = Pkg->GetOutermost();
	if( OutermostPkg->PackageFlags & PKG_Cooked )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
		return FALSE;
	}

	// Handle fully loading packages before creating new objects.
	TArray<UPackage*> TopLevelPackages;
	TopLevelPackages.AddItem( OutermostPkg );
	if( !PackageTools::HandleFullyLoadingPackages( TopLevelPackages, TEXT("CreateANewObject") ) )
	{
		// User aborted.
		return FALSE;
	}

	prPkgOut = Pkg;
	prOtmPkgOut = OutermostPkg;
	return TRUE;
}

bool WxDlgNewGeneric::Validate()
{
	// Make sure the property window has applied all outstanding changes
	if( PropertyWindow != NULL )
	{
		PropertyWindow->FlushLastFocused();
		PropertyWindow->ClearLastFocused();
	}

	Package = PGNCtrl->PkgCombo->GetValue();
	Group	= PGNCtrl->GrpEdit->GetValue();
	Name	= PGNCtrl->NameEdit->GetValue();

	FString	QualifiedName = Package + TEXT(".");
	if( Group.Len() > 0 )
	{
		QualifiedName += Group + TEXT(".");
	}
	QualifiedName += Name;

	// @todo: These 'reason' messages are not localized strings!
	FString Reason;
	if (!FIsValidObjectName( *Name, Reason )
	||	!FIsValidGroupName( *Package, Reason )
	||	!FIsValidGroupName( *Group, Reason, TRUE))
	{
		appMsgf( AMT_OK, *Reason );
		return FALSE;
	}

	// Fetch the working packages
	UPackage* Pkg = NULL;
	UPackage* OutermostPkg = NULL;
	if ( !GetPackages( Package, Group, Pkg, OutermostPkg ) )
	{
		return FALSE;
	}

	// We need to test again after fully loading.
	if (!FIsValidObjectName( *Name, Reason )
	||	!FIsValidGroupName( *Package, Reason )
	||	!FIsValidGroupName( *Group, Reason, TRUE ) )
	{
		appMsgf( AMT_OK, *Reason );
		return FALSE;
	}

	// Check for an existing object
	UObject* ExistingObject = UObject::StaticFindObject( UObject::StaticClass(), ANY_PACKAGE, *QualifiedName );
	if( ExistingObject != NULL )
	{
		// Object already exists in either the specified package or another package.  Check to see if the user wants
		// to replace the object.
		UBOOL bWantReplace =
			appMsgf(
				AMT_YesNo,
				LocalizeSecure(
					LocalizeUnrealEd( "ReplaceExistingObjectInPackage_F" ),
					*Name,
					*ExistingObject->GetClass()->GetName(),
					*Package ) );
		
		if( bWantReplace )
		{
			// if the existing object is the same class, we just replace it without changing its address.
			// otherwise, we'll need to delete it first
			if ( ExistingObject->GetClass() != Factory->GetSupportedClass() )
			{
				// Replacing an object.  Here we go!
				TArray< UObject* > ObjectsToDelete;
				ObjectsToDelete.AddItem( ExistingObject );
				
				// Delete the existing object
				INT NumObjectsDeleted = 0;
				check( BrowsableObjectTypeList.Num() > 0 );
				NumObjectsDeleted = ObjectTools::DeleteObjects( ObjectsToDelete, BrowsableObjectTypeList );
				
				if( NumObjectsDeleted == 0 || !FIsUniqueObjectName( *QualifiedName, ANY_PACKAGE, Reason ) )
				{
					// Original object couldn't be deleted
					return FALSE;
				}

				// Fetch the working packages again (incase they've modified due to the above cleanup)
				if ( !GetPackages( Package, Group, Pkg, OutermostPkg ) )
				{
					return FALSE;
				}
			}
		}
		else
		{
			// User chose not to replace the object; they'll need to enter a new name
			return FALSE;
		}
	}

	UObject* NewObj = Factory->FactoryCreateNew( Factory->GetSupportedClass(), Pkg, FName( *Name ), RF_Public|RF_Standalone, NULL, GWarn );
	if( NewObj )
	{
		CreatedObject = NewObj;
		// Set the new objects as the sole selection.
		USelection* SelectionSet = GEditor->GetSelectedObjects();
		SelectionSet->DeselectAll();
		SelectionSet->Select( NewObj );

		// Refresh
		{
			const DWORD UpdateMask = CBR_ObjectCreated|CBR_SyncAssetView;
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, NewObj));
		}

		// Mark the package dirty...
		OutermostPkg->MarkPackageDirty();
	}

	return TRUE;
}

/**
 * Since we create a UFactory object, it needs to be serialized
 *
 * @param Ar The archive to serialize with
 */
void WxDlgNewGeneric::Serialize(FArchive& Ar)
{
	Ar << Factory;
	Ar << BrowsableObjectTypeList;
}
