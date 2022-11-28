
#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	WxDlgAbout.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgAbout, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgAbout::OnOK )
END_EVENT_TABLE()

WxDlgAbout::WxDlgAbout()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_ABOUT") );

	Bitmap.LoadLiteral( TEXT("Splash\\EdSplash") );
	BitmapStatic = (wxStaticBitmap*)FindWindow( XRCID( "IDSC_BITMAP" ) );
	BitmapStatic->SetBitmap( Bitmap );
	VersionStatic = (wxStaticText*)FindWindow( XRCID( "IDSC_VERSION_NUMBER" ) );
	VersionStatic->SetLabel( *FString::Printf( TEXT("%d"), GEngineVersion ) );

	FWindowUtil::LoadPosSize( TEXT("DlgAbout"), this );
}

WxDlgAbout::~WxDlgAbout()
{
	FWindowUtil::SavePosSize( TEXT("DlgAbout"), this );
}

void WxDlgAbout::OnOK( wxCommandEvent& In )
{
	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	WxDlgRename.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgRename, wxDialog)
END_EVENT_TABLE()

WxDlgRename::WxDlgRename()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_RENAME") );

	PackageEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_PACKAGE" ) );
	GroupEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_GROUP" ) );
	NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );

	FWindowUtil::LoadPosSize( TEXT("DlgRename"), this );
}

WxDlgRename::~WxDlgRename()
{
	FWindowUtil::SavePosSize( TEXT("DlgRename"), this );
}

int WxDlgRename::ShowModal( FString InPackage, FString InGroup, FString InName )
{
	OldPackage = NewPackage = InPackage;
	OldGroup = NewGroup = InGroup;
	OldName = NewName = InName;

	PackageEdit->SetValue( *InPackage );
	GroupEdit->SetValue( *InGroup );
	NameEdit->SetValue( *InName );

	return wxDialog::ShowModal();
}

bool WxDlgRename::Validate()
{
	NewPackage = PackageEdit->GetValue();
	NewGroup = GroupEdit->GetValue();
	NewName = NameEdit->GetValue();

	FString Reason;
	if( !FIsValidObjectName( *NewName, Reason ) )
	{
		appMsgf( 0, *Reason );
		return 0;
	}

	return 1;
}

/*-----------------------------------------------------------------------------
	WxDlgPackageGroupName.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgPackageGroupName, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgPackageGroupName::OnOK )
END_EVENT_TABLE()

WxDlgPackageGroupName::WxDlgPackageGroupName()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_RENAME") );

	PackageEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_PACKAGE" ) );
	GroupEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_GROUP" ) );
	NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );

	FWindowUtil::LoadPosSize( TEXT("DlgPackageGroupName"), this );
}

WxDlgPackageGroupName::~WxDlgPackageGroupName()
{
	FWindowUtil::SavePosSize( TEXT("DlgPackageGroupName"), this );
}

int WxDlgPackageGroupName::ShowModal( FString InPackage, FString InGroup, FString InName )
{
	Package = InPackage;
	Group = InGroup;
	Name = InName;

	PackageEdit->SetValue( *InPackage );
	GroupEdit->SetValue( *InGroup );
	NameEdit->SetValue( *InName );

	return wxDialog::ShowModal();
}

void WxDlgPackageGroupName::OnOK( wxCommandEvent& In )
{
	Package = PackageEdit->GetValue();
	Group = GroupEdit->GetValue();
	Name = NameEdit->GetValue();

	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	WxDlgAddSpecial.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgAddSpecial, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgAddSpecial::OnOK )
END_EVENT_TABLE()

WxDlgAddSpecial::WxDlgAddSpecial()
	: wxDialog()
{
	wxXmlResource* GlobalResource = wxXmlResource::Get();
	GlobalResource->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_ADDSPECIAL") );

	PrefabsCombo = (wxComboBox*)FindWindow( XRCID( "IDCB_PREFABS" ) );
	AntiPortalCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_ANTIPORTAL" ) );
	PortalCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_PORTAL" ) );
	MirrorCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_MIRROR" ) );
	InvisibleCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_INVISIBLE" ) );
	TwoSidedCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_TWOSIDED" ) );
	SolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_SOLID" ) );
	SemiSolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_SEMISOLID" ) );
	NonSolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_NONSOLID" ) );

	PrefabsCombo->Append( TEXT("Invisible Collision Hull") );
	PrefabsCombo->Append( TEXT("Zone Portal") );
	PrefabsCombo->Append( TEXT("Anti-Portal") );
	PrefabsCombo->Append( TEXT("Regular Brush") );
	PrefabsCombo->Append( TEXT("Semi Solid Brush") );
	PrefabsCombo->Append( TEXT("Non Solid Brush") );
	PrefabsCombo->SetSelection( 1 );
    wxCommandEvent Event;
	OnPrefabsSelChange( Event );

	ADDEVENTHANDLER( XRCID("IDCB_PREFABS"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxDlgAddSpecial::OnPrefabsSelChange );

	FWindowUtil::LoadPosSize( TEXT("DlgAddSpecial"), this );
}

WxDlgAddSpecial::~WxDlgAddSpecial()
{
	FWindowUtil::SavePosSize( TEXT("DlgAddSpecial"), this );
}

void WxDlgAddSpecial::OnOK( wxCommandEvent& In )
{
	INT Flags = 0;

	if( MirrorCheck->GetValue() )		Flags |= PF_Mirrored;
	if( PortalCheck->GetValue() )		Flags |= PF_Portal;
	if( InvisibleCheck->GetValue() )	Flags |= PF_Invisible;
	if( TwoSidedCheck->GetValue() )		Flags |= PF_TwoSided;
	if( SemiSolidRadio->GetValue() )	Flags |= PF_Semisolid;
	if( NonSolidRadio->GetValue() )		Flags |= PF_NotSolid;

	GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH ADD FLAGS=%d"), Flags));

	wxDialog::OnOK( In );
}

void WxDlgAddSpecial::OnPrefabsSelChange( wxCommandEvent& In )
{
	AntiPortalCheck->SetValue( 0 );
	MirrorCheck->SetValue( 0 );
	PortalCheck->SetValue( 0 );
	InvisibleCheck->SetValue( 0 );
	TwoSidedCheck->SetValue( 0 );

	SolidRadio->SetValue( 0 );
	SemiSolidRadio->SetValue( 0 );
	NonSolidRadio->SetValue( 0 );

	switch( PrefabsCombo->GetSelection() )
	{
		case 0:	// Invisible Collision Hull
			InvisibleCheck->SetValue( 1 );
			SemiSolidRadio->SetValue( 1 );
			break;

		case 1:	// Zone Portal
			PortalCheck->SetValue( 1 );
			InvisibleCheck->SetValue( 1 );
			NonSolidRadio->SetValue( 1 );
			break;

		case 2:	// Anti Portal
			AntiPortalCheck->SetValue( 1 );
			InvisibleCheck->SetValue( 1 );
			NonSolidRadio->SetValue( 1 );
			break;

		case 3:	// Regular Brush
			SolidRadio->SetValue( 1 );
			break;

		case 4:	// Semisolid brush
			SemiSolidRadio->SetValue( 1 );
			break;

		case 5:	// Nonsolid brush
			NonSolidRadio->SetValue( 1 );
			break;
	}
}

/*-----------------------------------------------------------------------------
	WxDlgImportBrush.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgImportBrush, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgImportBrush::OnOK )
END_EVENT_TABLE()

WxDlgImportBrush::WxDlgImportBrush()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_IMPORTBRUSH") );

	MergeFacesCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_MERGEFACES" ) );
	SolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_SOLID" ) );
	NonSolidRadio = (wxRadioButton*)FindWindow( XRCID( "IDRB_NONSOLID" ) );

	SolidRadio->SetValue( 1 );

	FWindowUtil::LoadPosSize( TEXT("DlgImportBrush"), this );
}

WxDlgImportBrush::~WxDlgImportBrush()
{
	FWindowUtil::SavePosSize( TEXT("DlgImportBrush"), this );
}

void WxDlgImportBrush::OnOK( wxCommandEvent& In )
{
	GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH IMPORT FILE=\"%s\" MERGE=%d FLAGS=%d"),
		*Filename,
		MergeFacesCheck->GetValue(),
		NonSolidRadio->GetValue() ? PF_NotSolid : 0) );

	GUnrealEd->Level->Brush()->Brush->BuildBound();

	wxDialog::OnOK( In );
}

int WxDlgImportBrush::ShowModal( FString InFilename )
{
	Filename = InFilename;

	return wxDialog::ShowModal();
}

/*-----------------------------------------------------------------------------
	WxDlgImportBase.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgImportBase, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgImportBase::OnOK )
END_EVENT_TABLE()

WxDlgImportBase::WxDlgImportBase()
	: wxDialog()
{
	check(0);	// wrong ctor
}

WxDlgImportBase::WxDlgImportBase( FString InResourceName )
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, *InResourceName );

	FilenameStatic = (wxStaticText*)FindWindow( XRCID( "IDSC_FILENAME" ) );
	PackageText = (wxTextCtrl*)FindWindow( XRCID( "IDEC_PACKAGE" ) );
	GroupText = (wxTextCtrl*)FindWindow( XRCID( "IDEC_GROUP" ) );
	NameText = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );

	ADDEVENTHANDLER( XRCID("IDPB_SKIP"), wxEVT_COMMAND_BUTTON_CLICKED, &WxDlgImportBase::OnSkip );
	ADDEVENTHANDLER( XRCID("IDPB_OKALL"), wxEVT_COMMAND_BUTTON_CLICKED, &WxDlgImportBase::OnOKAll );
}

WxDlgImportBase::~WxDlgImportBase()
{
}

int WxDlgImportBase::ShowModal( FString InPackage, FString InGroup, TArray<FFilename>* InFilenames )
{
	Package = InPackage;
	Group = InGroup;
	Filenames = InFilenames;

	bOKToAll = 0;
	iCurrentFilename = -1;

	PackageText->SetValue( *Package );
	GroupText->SetValue( *Group );

	SetNextFilename();

	return wxDialog::ShowModal();
}

void WxDlgImportBase::SetNextFilename()
{
	iCurrentFilename++;
	if( iCurrentFilename == Filenames->Num() )
	{
		EndModal(wxID_OK);
		return;
	}

	if( bOKToAll )
	{
		RefreshName();
		GetDataFromUser();
		ImportFile( (*Filenames)(iCurrentFilename) );
		SetNextFilename();
		return;
	};

	RefreshName();

}

void WxDlgImportBase::RefreshName()
{
	FFilename fn = (*Filenames)(iCurrentFilename);
	FilenameStatic->SetLabel( *fn );
	NameText->SetValue( *fn.GetBaseFilename() );

}

UBOOL WxDlgImportBase::GetDataFromUser( void )
{
	Package = PackageText->GetValue();
	Group = GroupText->GetValue();
	Name = NameText->GetValue();

	if( !Package.Len() || !Name.Len() )
	{
		appMsgf( 0, TEXT("Invalid input.") );
		return 0;
	}

	return 1;

}

void WxDlgImportBase::ImportFile( FString Filename )
{
}

void WxDlgImportBase::OnOK( wxCommandEvent& In )
{
	if( GetDataFromUser() )
	{
		ImportFile( (*Filenames)(iCurrentFilename) );
		SetNextFilename();
	}
}

void WxDlgImportBase::OnOKAll( wxCommandEvent& In )
{
	if( GetDataFromUser() )
	{
		bOKToAll = 1;
		ImportFile( (*Filenames)(iCurrentFilename) );		
		SetNextFilename();
	}

}

void WxDlgImportBase::OnSkip( wxCommandEvent& In )
{
	if( GetDataFromUser() )
		SetNextFilename();

}

/*-----------------------------------------------------------------------------
	WxDlgGroup.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGroup, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgGroup::OnOK )
END_EVENT_TABLE()

WxDlgGroup::WxDlgGroup()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GROUP") );

	NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );

	FWindowUtil::LoadPosSize( TEXT("DlgGroup"), this );
}

WxDlgGroup::~WxDlgGroup()
{
	FWindowUtil::SavePosSize( TEXT("DlgGroup"), this );
}

int WxDlgGroup::ShowModal( UBOOL InNew, FString InName )
{
	Name = InName;

	if( InNew )
		SetTitle( TEXT("New Group") );
	else
		SetTitle( TEXT("Rename Group") );

	NameEdit->SetValue( *Name );

	return wxDialog::ShowModal();
}

void WxDlgGroup::OnOK( wxCommandEvent& In )
{
	Name = NameEdit->GetValue();

	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	WxDlgNewGeneric.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgNewGeneric, wxDialog)
END_EVENT_TABLE()

WxDlgNewGeneric::WxDlgNewGeneric()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GENERIC_NEW") );

	PackageEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_PACKAGE" ) );
	GroupEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_GROUP" ) );
	NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );
	FactoryCombo = (wxComboBox*)FindWindow( XRCID( "IDCB_FACTORY" ) );

	// Replace the placeholder window with a property window
	wxPanel* PropsPanel = (wxPanel*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );

	PropsPanelSizer = new wxBoxSizer(wxVERTICAL);
	PropsPanel->SetSizer(PropsPanelSizer);
	PropsPanel->SetAutoLayout(true);

	wxRect rc = PropsPanel->GetRect();

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	PropertyWindow->SetSize( rc );

	PropsPanel->Refresh();

	ADDEVENTHANDLER( XRCID("IDCB_FACTORY"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxDlgNewGeneric::OnFactorySelChange );

	Factory = NULL;

	FWindowUtil::LoadPosSize( TEXT("DlgNewGeneric"), this );
}

WxDlgNewGeneric::~WxDlgNewGeneric()
{
	FWindowUtil::SavePosSize( TEXT("DlgNewGeneric"), this );

	delete Factory;
}

// DefaultFactoryClass is an optional parameter indicating the default factory to select in combo box
int WxDlgNewGeneric::ShowModal( FString InPackage, FString InGroup, UClass* DefaultFactoryClass )
{
	Package = InPackage;
	Group = InGroup;

	PackageEdit->SetValue( *Package );
	GroupEdit->SetValue( *Group );

	// Find the factories that create new objects and add to combo box

	INT i=0;
	INT DefIndex = INDEX_NONE;
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		if( It->IsChildOf(UFactory::StaticClass()) && !(It->ClassFlags & CLASS_Abstract) )
		{
			UClass* FactoryClass = *It;
			Factory = ConstructObject<UFactory>( FactoryClass );
			UFactory* DefFactory = (UFactory*)FactoryClass->GetDefaultObject();
			if( Factory->bCreateNew )
			{
				FactoryCombo->Append( *DefFactory->Description, FactoryClass );

				// If this is the item we want as the default, remember the index.
				if(FactoryClass && DefaultFactoryClass == FactoryClass)
				{
					DefIndex = i;
				}

				i++;
			}
			delete Factory;
		}
	}

	Factory = NULL;

	// If noe default was specified, we just select the first one.
	if(DefIndex == INDEX_NONE)
	{
		DefIndex = 0;
	}

	FactoryCombo->SetSelection( DefIndex );	

	OnFactorySelChange( wxCommandEvent() );

	NameEdit->SetFocus();

	return wxDialog::ShowModal();
}

bool WxDlgNewGeneric::Validate()
{
	Package = PackageEdit->GetValue();
	Group	= GroupEdit->GetValue();
	Name	= NameEdit->GetValue();

	FString	QualifiedName;;
	if( Group.Len() )
	{
		QualifiedName = Package + TEXT(".") + Group + TEXT(".") + Name;
	}
	else
	{
		QualifiedName = Package + TEXT(".") + Name;
	}
	
	FString Reason;
	if( !FIsValidObjectName( *Name, Reason ) || !FIsValidObjectName( *Package, Reason ) || !FIsValidObjectName( *Group, Reason ) || !FIsUniqueObjectName( *QualifiedName, Reason ) )
	{
		appMsgf( 0, *Reason );
		return 0;
	}

	UPackage* Pkg = GEngine->CreatePackage(NULL,*Package);
	if( Group.Len() )
		Pkg = GEngine->CreatePackage(Pkg,*Group);

	Pkg->bDirty = 1;

	Factory->FactoryCreateNew( Factory->SupportedClass, Pkg, FName( *Name ), RF_Public|RF_Standalone, NULL, GWarn );

	return 1;
}

void WxDlgNewGeneric::OnFactorySelChange( wxCommandEvent& In )
{
	UClass* Class = (UClass*)FactoryCombo->GetClientData( FactoryCombo->GetSelection() );

	delete Factory;
	Factory = ConstructObject<UFactory>( Class );

	PropertyWindow->SetObject( Factory, 1,1,0 );
	PropertyWindow->Raise();
	PropsPanelSizer->Layout();
	Refresh();
}

/*-----------------------------------------------------------------------------
	WxDlgImportGeneric.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgImportGeneric, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgImportGeneric::OnOK )
END_EVENT_TABLE()

WxDlgImportGeneric::WxDlgImportGeneric()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GENERIC_IMPORT") );

	PackageEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_PACKAGE" ) );
	GroupEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_GROUP" ) );
	NameEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );
	
	// Replace the placeholder window with a property window
	wxPanel* PropsPanel = (wxPanel*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );

	PropsPanelSizer = new wxBoxSizer(wxVERTICAL);
	PropsPanel->SetSizer(PropsPanelSizer);
	PropsPanel->SetAutoLayout(true);

	wxRect rc = PropsPanel->GetRect();

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	PropertyWindow->SetSize( rc );

	PropsPanel->Refresh();

	ADDEVENTHANDLER( XRCID("ID_OK_ALL"), wxEVT_COMMAND_BUTTON_CLICKED , &WxDlgImportGeneric::OnOKAll );

	Factory = NULL;

	FWindowUtil::LoadPosSize( TEXT("DlgImportGeneric"), this );
}

WxDlgImportGeneric::~WxDlgImportGeneric()
{
	FWindowUtil::SavePosSize( TEXT("DlgImportGeneric"), this );
}

int WxDlgImportGeneric::ShowModal( FFilename InFilename, FString InPackage, FString InGroup, UClass* InClass, UFactory* InFactory )
{
	Filename = InFilename;
	Package = InPackage;
	Group = InGroup;
	Factory = InFactory;
	Class = InClass;

	PackageEdit->SetValue( *Package );
	GroupEdit->SetValue( *Group );
	NameEdit->SetValue( *InFilename.GetBaseFilename() );

	if( Factory->bOKToAll )
	{
		DoImport( 1 );
		return wxID_OK;
	}

	PropertyWindow->SetObject( Factory, 1,1,0 );
	PropertyWindow->Raise();
	PropsPanelSizer->Layout();
	Refresh();

	return wxDialog::ShowModal();
}

void WxDlgImportGeneric::OnOK( wxCommandEvent& In )
{
	DoImport( 0 );
}

void WxDlgImportGeneric::OnOKAll( wxCommandEvent& In )
{
	Factory->bOKToAll = 1;
	DoImport( 1 );
}

void WxDlgImportGeneric::DoImport( UBOOL bOKToAll )
{
	Package = PackageEdit->GetValue();
	Group = GroupEdit->GetValue();
	Name = NameEdit->GetValue();

	FString Reason;
	if( !FIsValidObjectName( *Name, Reason ) )
	{
		appMsgf( 0, *Reason );
		return;
	}

	UPackage* Pkg = GEngine->CreatePackage(NULL,*Package);
	if( Group.Len() )
		Pkg = GEngine->CreatePackage(Pkg,*Group);

	Pkg->bDirty = 1;

	UFactory::StaticImportObject( GEditor->Level, Class, Pkg, FName( *Name ), RF_Public|RF_Standalone, *Filename, NULL, Factory );

	EndModal( wxID_OK );
}

/*-----------------------------------------------------------------------------
	WxDlgExportGeneric.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgExportGeneric, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgExportGeneric::OnOK )
END_EVENT_TABLE()

WxDlgExportGeneric::WxDlgExportGeneric()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GENERIC_EXPORT") );

	NameText = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );

	// Replace the placeholder window with a property window

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show(0);

	Exporter = NULL;

	FWindowUtil::LoadPosSize( TEXT("DlgExportGeneric"), this );
}

WxDlgExportGeneric::~WxDlgExportGeneric()
{
	FWindowUtil::SavePosSize( TEXT("DlgExportGeneric"), this );
}

int WxDlgExportGeneric::ShowModal( FFilename InFilename, UObject* InObject, UExporter* InExporter )
{
	Filename = InFilename;
	Exporter = InExporter;
	Object = InObject;

	NameText->SetLabel( *Object->GetFullName() );
	NameText->SetEditable(false);

	PropertyWindow->SetObject( Exporter, 1,1,0 );
	Refresh();

	return wxDialog::ShowModal();
}

void WxDlgExportGeneric::OnOK( wxCommandEvent& In )
{
	DoExport();
}

void WxDlgExportGeneric::DoExport()
{
	UExporter::ExportToFile( Object, Exporter, *Filename, 0 );

	EndModal( wxID_OK );
}

/*-----------------------------------------------------------------------------
	WxDlgTextureProperties.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgTextureProperties, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgTextureProperties::OnOK )
	EVT_SIZE( WxDlgTextureProperties::OnSize )
END_EVENT_TABLE()

WxDlgTextureProperties::WxDlgTextureProperties()
	: wxDialog()
	, Viewport( NULL)
{
	PropertyWindow = NULL;

	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_TEXTURE_PROPERTIES") );

	// Property window for editing the texture

	PropertyWindow = new WxPropertyWindow( this, this );

	wxWindow* win = FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show();

	// Viewport to show the texture in

	ViewportHolder = new EViewportHolder( (wxWindow*)this, -1, 0 );
	win = (wxWindow*)FindWindow( XRCID( "ID_VIEWPORT_WINDOW" ) );
	rc = win->GetRect();
	Viewport = GEngine->Client->CreateWindowChildViewport( this, (HWND)ViewportHolder->GetHandle(), rc.width, rc.height );
	Viewport->CaptureJoystickInput(false);
	ViewportHolder->SetViewport( Viewport );
	ViewportHolder->SetSize( rc );

	FWindowUtil::LoadPosSize( TEXT("DlgTextureProperties"), this );
}

WxDlgTextureProperties::~WxDlgTextureProperties()
{
	FWindowUtil::SavePosSize( TEXT("DlgTextureProperties"), this );

	GEngine->Client->CloseViewport(Viewport); 
	Viewport = NULL;
	delete PropertyWindow;
}

bool WxDlgTextureProperties::Show( bool InShow, UTexture* InTexture )
{
	Texture = InTexture;

	PropertyWindow->SetObject( Texture, 1,1,0 );


	SetTitle( *FString::Printf( TEXT("Texture Properties - %s ( %s )"), *Texture->GetPathName(), *Texture->GetDesc() ) );

	return wxDialog::Show(InShow);
}

void WxDlgTextureProperties::OnOK( wxCommandEvent& In )
{
	Destroy();
}

void WxDlgTextureProperties::OnSize( wxSizeEvent& In )
{
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	if( win && PropertyWindow )
	{
		wxRect rc = win->GetRect();
		PropertyWindow->SetSize( rc );
	}

	In.Skip();
}

void WxDlgTextureProperties::Draw(FChildViewport* Viewport,FRenderInterface* RI)
{
	RI->Clear( FColor(0,0,0) );
	Texture->DrawThumbnail( TPT_Plane, 0, 0, Viewport, RI, -1, 0, 1, 0 );
}

void WxDlgTextureProperties::NotifyDestroy( void* Src )
{
	if( Src == PropertyWindow )
		PropertyWindow = NULL;
}

void WxDlgTextureProperties::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{

}

void WxDlgTextureProperties::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	Viewport->Invalidate();
}

void WxDlgTextureProperties::NotifyExec( void* Src, const TCHAR* Cmd )
{

}

/*-----------------------------------------------------------------------------
	WxDlgBrushBuilder.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgBrushBuilder, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgBrushBuilder::OnOK )
	EVT_BUTTON( wxID_CLOSE, WxDlgBrushBuilder::OnClose )
END_EVENT_TABLE()

WxDlgBrushBuilder::WxDlgBrushBuilder()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_BRUSH_BUILDER") );

	// Replace the placeholder window with a property window

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show(0);

	FWindowUtil::LoadPosSize( TEXT("DlgBrushBuilder"), this );
}

WxDlgBrushBuilder::~WxDlgBrushBuilder()
{
	FWindowUtil::SavePosSize( TEXT("DlgBrushBuilder"), this );
	delete PropertyWindow;
}

UBOOL WxDlgBrushBuilder::Show( UBrushBuilder* InBrushBuilder, UBOOL InShow )
{
	BrushBuilder = InBrushBuilder;

	PropertyWindow->SetObject( BrushBuilder, 1,0,0 );

	SetTitle( *FString::Printf( TEXT("Brush Builder - %s"), *BrushBuilder->ToolTip ) );

    return wxDialog::Show( InShow ? true : false );
}

void WxDlgBrushBuilder::OnOK( wxCommandEvent& In )
{
	PropertyWindow->FinalizeValues();
	BrushBuilder->eventBuild();
	GEditorModeTools.GetCurrentMode()->MapChangeNotify();
}

void WxDlgBrushBuilder::OnClose( wxCommandEvent& In )
{
	Destroy();
}

/*-----------------------------------------------------------------------------
	WxDlgGenericOptions.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGenericOptions, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgGenericOptions::OnOK )
END_EVENT_TABLE()

WxDlgGenericOptions::WxDlgGenericOptions()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GENERIC_OPTIONS") );

	// Replace the placeholder window with a property window

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show(0);

	FWindowUtil::LoadPosSize( TEXT("DlgGenericOptions"), this );
}

WxDlgGenericOptions::~WxDlgGenericOptions()
{
	FWindowUtil::SavePosSize( TEXT("DlgGenericOptions"), this );
	delete PropertyWindow;
}

UBOOL WxDlgGenericOptions::ShowModal( UObject* InObject, FString InCaption )
{
	Object = InObject;

	PropertyWindow->SetObject( Object, 1,0,0 );

	SetTitle( *InCaption );

	return wxDialog::ShowModal();
}

void WxDlgGenericOptions::OnOK( wxCommandEvent& In )
{
	PropertyWindow->FinalizeValues();
	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	WxDlgGenericStringEntry.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGenericStringEntry, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgGenericStringEntry::OnOK )
END_EVENT_TABLE()

WxDlgGenericStringEntry::WxDlgGenericStringEntry()
: wxDialog( )
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_GENERICSTRINGENTRY") );

	StringEntry = (wxTextCtrl*)FindWindow( XRCID( "IDEC_STRINGENTRY" ) );
	StringCaption = (wxStaticText*)FindWindow( XRCID( "IDEC_STRINGCAPTION" ) );

	ADDEVENTHANDLER( XRCID("IDEC_STRINGENTRY"), wxEVT_COMMAND_TEXT_ENTER, &WxDlgGenericStringEntry::OnOK );

	EnteredString = FString( TEXT("") );

	FWindowUtil::LoadPosSize( TEXT("DlgGenericStringEntry"), this );
}

WxDlgGenericStringEntry::~WxDlgGenericStringEntry()
{
	FWindowUtil::SavePosSize( TEXT("DlgGenericStringEntry"), this );
}

int WxDlgGenericStringEntry::ShowModal(const TCHAR* DialogTitle, const TCHAR* Caption, const TCHAR* DefaultString)
{
	SetTitle( DialogTitle );

	StringCaption->SetLabel( Caption );
	StringEntry->SetValue( DefaultString );

	return wxDialog::ShowModal();
}

void WxDlgGenericStringEntry::OnOK( wxCommandEvent& In )
{
	EnteredString = StringEntry->GetValue();

	wxDialog::OnOK( In );
}


/*-----------------------------------------------------------------------------
	WxDlgGenericComboEntry.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgGenericComboEntry, wxDialog)
EVT_BUTTON( wxID_OK, WxDlgGenericComboEntry::OnOK )
END_EVENT_TABLE()

WxDlgGenericComboEntry::WxDlgGenericComboEntry()
: wxDialog( )
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_GENERICCOMBOENTRY") );

	ComboBox = (wxComboBox*)FindWindow( XRCID( "IDEC_COMBOENTRY" ) );
	ComboCaption = (wxStaticText*)FindWindow( XRCID( "IDEC_COMBOCAPTION" ) );
}

WxDlgGenericComboEntry::~WxDlgGenericComboEntry()
{
}

int WxDlgGenericComboEntry::ShowModal(const TCHAR* InDialogTitle, const TCHAR* InComboCaption, TArray<FString>& InComboOptions)
{
	SetTitle( InDialogTitle );

	ComboCaption->SetLabel( InComboCaption );

	for(INT i=0; i<InComboOptions.Num(); i++)
	{
		ComboBox->Append( *InComboOptions(i) );
	}

	ComboBox->SetSelection(0);

	return wxDialog::ShowModal();
}

void WxDlgGenericComboEntry::OnOK( wxCommandEvent& In )
{
	SelectedString = ComboBox->GetValue();

	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	WxDlgActorFactory.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgActorFactory, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgActorFactory::OnOK )
END_EVENT_TABLE()

WxDlgActorFactory::WxDlgActorFactory()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_ACTOR_FACTORY") );

	NameText = (wxStaticText*)FindWindow( XRCID( "IDEC_NAME" ) );

	// Replace the placeholder window with a property window

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show(0);

	Factory = NULL;

	FWindowUtil::LoadPosSize( TEXT("DlgActorFactory"), this );
}

WxDlgActorFactory::~WxDlgActorFactory()
{
	FWindowUtil::SavePosSize( TEXT("DlgActorFactory"), this );
}

int WxDlgActorFactory::ShowModal( UActorFactory* InFactory )
{
	Factory = InFactory;

	PropertyWindow->SetObject( Factory, 1,1,0 );

	NameText->SetLabel( *(Factory->MenuName) );
	//NameText->SetEditable(false);

	return wxDialog::ShowModal();
}

void WxDlgActorFactory::OnOK( wxCommandEvent& In )
{
	wxDialog::OnOK( In );
}

/*-----------------------------------------------------------------------------
	XDlgSurfPropPage1.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxSurfPropPage1, wxPanel)
END_EVENT_TABLE()

WxSurfPropPage1::WxSurfPropPage1( wxWindow* InParent )
	: wxPanel( InParent )
{
	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, TEXT("ID_SURFPROP_PANROTSCALE") );
	Panel->Fit();

	SimpleCB = (wxComboBox*)FindWindow( XRCID( "IDCB_SIMPLE" ) );
	CustomULabel = (wxStaticText*)FindWindow( XRCID( "IDSC_CUSTOM_U" ) );
	CustomVLabel = (wxStaticText*)FindWindow( XRCID( "IDSC_CUSTOM_V" ) );
	CustomUEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_CUSTOM_U" ) );
	CustomVEdit = (wxTextCtrl*)FindWindow( XRCID( "IDEC_CUSTOM_V" ) );
	SimpleScaleButton = (wxRadioButton*)FindWindow( XRCID( "IDRB_SIMPLE" ) );
	RelativeCheck = (wxCheckBox*)FindWindow( XRCID( "IDCK_RELATIVE" ) );
	LightMapResCombo = (wxComboBox*)FindWindow( XRCID( "IDCB_LIGHTMAPRES" ) );

	CustomULabel->Disable();
	CustomVLabel->Disable();
	CustomUEdit->Disable();
	CustomVEdit->Disable();
	SimpleScaleButton->SetValue( 1 );

	ADDEVENTHANDLER( XRCID("IDPB_U_1"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnU1 );
	ADDEVENTHANDLER( XRCID("IDPB_U_4"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnU4 );
	ADDEVENTHANDLER( XRCID("IDPB_U_16"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnU16 );
	ADDEVENTHANDLER( XRCID("IDPB_U_64"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnU64 );
	ADDEVENTHANDLER( XRCID("IDPB_V_1"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnV1 );
	ADDEVENTHANDLER( XRCID("IDPB_V_4"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnV4 );
	ADDEVENTHANDLER( XRCID("IDPB_V_16"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnV16 );
	ADDEVENTHANDLER( XRCID("IDPB_V_64"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnV64 );
	ADDEVENTHANDLER( XRCID("IDPB_ROT_45"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnRot45 );
	ADDEVENTHANDLER( XRCID("IDPB_ROT_90"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnRot90 );
	ADDEVENTHANDLER( XRCID("IDPB_FLIP_U"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnFlipU );
	ADDEVENTHANDLER( XRCID("IDPB_FLIP_V"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnFlipV );
	ADDEVENTHANDLER( XRCID("IDPB_APPLY"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage1::OnApply );
	ADDEVENTHANDLER( XRCID("IDRB_SIMPLE"), wxEVT_COMMAND_RADIOBUTTON_SELECTED, &WxSurfPropPage1::OnScaleSimple );
	ADDEVENTHANDLER( XRCID("IDRB_CUSTOM"), wxEVT_COMMAND_RADIOBUTTON_SELECTED, &WxSurfPropPage1::OnScaleCustom );
	ADDEVENTHANDLER( XRCID("IDCB_LIGHTMAPRES"), wxEVT_COMMAND_COMBOBOX_SELECTED, &WxSurfPropPage1::OnLightMapResSelChange );
}

WxSurfPropPage1::~WxSurfPropPage1()
{
}

void WxSurfPropPage1::Pan( INT InU, INT InV )
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXPAN U=%f V=%f"), InU * Mod, InV * Mod ) );
}

void WxSurfPropPage1::Scale( FLOAT InScaleU, FLOAT InScaleV, UBOOL InRelative )
{
	if( InScaleU == 0.f )
	{
		InScaleU = 1.f;
	}
	if( InScaleV == 0.f )
	{
		InScaleV = 1.f;
	}

	InScaleU = 1.0f / InScaleU;
	InScaleV = 1.0f / InScaleV;

	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXSCALE %s UU=%f VV=%f"), InRelative?TEXT("RELATIVE"):TEXT(""), InScaleU, InScaleV ) );
}

void WxSurfPropPage1::RefreshPage()
{
	// Find the lightmap scale on the selected surfaces.

	FLOAT	LightMapScale = 0.0f;
	UBOOL	ValidScale = 0;

	for(INT SurfaceIndex = 0;SurfaceIndex < GUnrealEd->Level->Model->Surfs.Num();SurfaceIndex++)
	{
		FBspSurf&	Surf =  GUnrealEd->Level->Model->Surfs(SurfaceIndex);

		if(Surf.PolyFlags & PF_Selected)
		{
			if(!ValidScale)
			{
				LightMapScale = Surf.LightMapScale;
				ValidScale = 1;
			}
			else if(LightMapScale != Surf.LightMapScale)
			{
				ValidScale = 0;
				break;
			}
		}
	}

	// Select the appropriate scale.

	INT		ScaleItem = INDEX_NONE;

	if(ValidScale)
	{
		FString	ScaleString = FString::Printf(TEXT("%.1f"),LightMapScale);

		ScaleItem = LightMapResCombo->FindString(*ScaleString);

		if(ScaleItem == INDEX_NONE)
		{
			ScaleItem = LightMapResCombo->GetCount();
			LightMapResCombo->Append( *ScaleString );
		}
	}

	LightMapResCombo->SetSelection( ScaleItem );
}

void WxSurfPropPage1::OnU1( wxCommandEvent& In )
{
	Pan( 1, 0 );
}

void WxSurfPropPage1::OnU4( wxCommandEvent& In )
{
	Pan( 4, 0 );
}

void WxSurfPropPage1::OnU16( wxCommandEvent& In )
{
	Pan( 16, 0 );
}

void WxSurfPropPage1::OnU64( wxCommandEvent& In )
{
	Pan( 64, 0 );
}

void WxSurfPropPage1::OnV1( wxCommandEvent& In )
{
	Pan( 0, 1 );
}

void WxSurfPropPage1::OnV4( wxCommandEvent& In )
{
	Pan( 0, 4 );
}

void WxSurfPropPage1::OnV16( wxCommandEvent& In )
{
	Pan( 0, 16 );
}

void WxSurfPropPage1::OnV64( wxCommandEvent& In )
{
	Pan( 0, 64 );
}

void WxSurfPropPage1::OnFlipU( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY TEXMULT UU=-1 VV=1") );
}

void WxSurfPropPage1::OnFlipV( wxCommandEvent& In )
{
	GUnrealEd->Exec( TEXT("POLY TEXMULT UU=1 VV=-1") );
}

void WxSurfPropPage1::OnRot45( wxCommandEvent& In )
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	FLOAT UU = 1.0f / appSqrt(2.f);
	FLOAT VV = 1.0f / appSqrt(2.f);
	FLOAT UV = (1.0f / appSqrt(2.f)) * Mod;
	FLOAT VU = -(1.0f / appSqrt(2.f)) * Mod;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
}

void WxSurfPropPage1::OnRot90( wxCommandEvent& In )
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	FLOAT UU = 0;
	FLOAT VV = 0;
	FLOAT UV = 1 * Mod;
	FLOAT VU = -1 * Mod;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
}

void WxSurfPropPage1::OnApply( wxCommandEvent& In )
{
	FLOAT UScale, VScale;

	if( SimpleScaleButton->GetValue() )
	{
		UScale = VScale = appAtof( SimpleCB->GetValue() );
	}
	else
	{
		UScale = appAtof( CustomUEdit->GetValue() );
		VScale = appAtof( CustomVEdit->GetValue() );
	}

	UBOOL bRelative = RelativeCheck->GetValue();

	Scale( UScale, VScale, bRelative );
}

void WxSurfPropPage1::OnScaleSimple( wxCommandEvent& In )
{
	CustomULabel->Disable();
	CustomVLabel->Disable();
	CustomUEdit->Disable();
	CustomVEdit->Disable();
	SimpleCB->Enable();
}

void WxSurfPropPage1::OnScaleCustom( wxCommandEvent& In )
{
	CustomULabel->Enable();
	CustomVLabel->Enable();
	CustomUEdit->Enable();
	CustomVEdit->Enable();
	SimpleCB->Disable();
}

void WxSurfPropPage1::OnLightMapResSelChange( wxCommandEvent& In )
{
	FLOAT	LightMapScale = appAtof(LightMapResCombo->GetString(LightMapResCombo->GetSelection()));

	for(INT SurfaceIndex = 0;SurfaceIndex < GUnrealEd->Level->Model->Surfs.Num();SurfaceIndex++)
	{
		FBspSurf&	Surf = GUnrealEd->Level->Model->Surfs(SurfaceIndex);

		if(Surf.PolyFlags & PF_Selected)
		{
			Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly).LightMapScale = LightMapScale;
			Surf.LightMapScale = LightMapScale;
		}
	}
}

/*-----------------------------------------------------------------------------
	WxSurfPropPage2.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxSurfPropPage2, wxPanel)
END_EVENT_TABLE()

WxSurfPropPage2::WxSurfPropPage2( wxWindow* InParent )
	: wxPanel( InParent )
{
	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, TEXT("ID_SURFPROP_ALIGN") );
	Panel->Fit();

	AlignList = (wxListBox*)FindWindow( XRCID( "IDLB_ALIGNMENT" ) );

	ADDEVENTHANDLER( XRCID("IDLB_ALIGNMENT"), wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, &WxSurfPropPage2::OnApply );
	ADDEVENTHANDLER( XRCID("IDLB_ALIGNMENT"), wxEVT_COMMAND_LISTBOX_SELECTED, &WxSurfPropPage2::OnAlignSelChange );
	ADDEVENTHANDLER( XRCID("IDPB_APPLY"), wxEVT_COMMAND_BUTTON_CLICKED, &WxSurfPropPage2::OnApply );

	// Initialize controls.
	for( INT x = 0 ; x < GTexAlignTools.Aligners.Num() ; ++x )
		AlignList->Append( *GTexAlignTools.Aligners(x)->Desc );
	AlignList->SetSelection( 0 );

	PropertyWindow = new WxPropertyWindow( this, GUnrealEd );
	wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
	wxRect rc = win->GetRect();
	PropertyWindow->SetSize( rc );
	win->Show(0);
}

WxSurfPropPage2::~WxSurfPropPage2()
{
}

void WxSurfPropPage2::RefreshPage()
{
	PropertyWindow->SetObject( GTexAlignTools.Aligners( AlignList->GetSelection() ), 1,0,0 );
}

void WxSurfPropPage2::OnAlignSelChange( wxCommandEvent& In )
{
	RefreshPage();
}

void WxSurfPropPage2::OnApply( wxCommandEvent& In )
{
	GTexAlignTools.Aligners( AlignList->GetSelection() )->Align( TEXALIGN_None, GUnrealEd->Level->Model );
}

/*-----------------------------------------------------------------------------
	WxDlgSurfaceProperties.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxDlgSurfaceProperties, wxDialog)
END_EVENT_TABLE()

WxDlgSurfaceProperties::WxDlgSurfaceProperties()
	: wxDialog()
{
	wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_SURFACEPROPERTIES") );

	Notebook = (wxNotebook*)FindWindow( XRCID( "IDNB_NOTEBOOK" ) );

	Page1 = new WxSurfPropPage1( this );
	Page2 = new WxSurfPropPage2( this );

	Notebook->AddPage( Page1, TEXT("Rot/Pan/Scale"), 1 );
	Notebook->AddPage( Page2, TEXT("Alignment"), 0 );

	FWindowUtil::LoadPosSize( TEXT("SurfaceProperties"), this );

	Notebook->SetPageSize( wxSize( 500, 300 ) );
	SetSize( 520, 360 );

	RefreshPages();
}

WxDlgSurfaceProperties::~WxDlgSurfaceProperties()
{
	FWindowUtil::SavePosSize( TEXT("SurfaceProperties"), this );
}

void WxDlgSurfaceProperties::RefreshPages()
{
	Page1->RefreshPage();
	Page2->RefreshPage();
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
