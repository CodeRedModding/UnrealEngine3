
#include "UnrealEd.h"
#include "UnTerrain.h"

/*-----------------------------------------------------------------------------
	WxWizardPageSimple.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxWizardPageSimple, wxWizardPageSimple)
END_EVENT_TABLE()

WxWizardPageSimple::WxWizardPageSimple( wxWizard* InParent, wxWizardPage* InPrev, wxWizardPage* InNext, TCHAR* InTemplateName )
	: wxWizardPageSimple( InParent, InPrev, InNext )
{
	Panel = (wxPanel*)wxXmlResource::Get()->LoadPanel( this, InTemplateName );
	check(Panel);			// Panel resource wasn't found!
	Panel->Fit();
}

WxWizardPageSimple::~WxWizardPageSimple()
{
}

UBOOL WxWizardPageSimple::InputIsValid()
{
	return 1;
}

/*-----------------------------------------------------------------------------
	WxWizard_NewTerrain.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE(WxWizard_NewTerrain, wxWizard)
END_EVENT_TABLE()

WxWizard_NewTerrain::WxWizard_NewTerrain( wxWindow* InParent, int InID )
	: wxWizard( InParent, InID, TEXT("New Terrain Wizard"), GApp->EditorFrame->WizardB )
{
	Page1 = new WxNewTerrain_Page1( this );
	Page2 = new WxNewTerrain_Page2( this, Page1 );

	Page1->SetNext( Page2 );

	SetPageSize( wxSize( 350, 150 ) );
}

WxWizard_NewTerrain::~WxWizard_NewTerrain()
{
}

UBOOL WxWizard_NewTerrain::RunWizard()
{
	if( wxWizard::RunWizard( Page1 ) )
	{
		return Exec();
	}

	return 0;
}

UBOOL WxWizard_NewTerrain::Exec()
{
	// Validate user input

	if( !Page1->InputIsValid()
		|| !Page2->InputIsValid() )
	{
		appMsgf( 0, TEXT("Input is invalid.") );
		return 0;
	}

	// Create the new terrain and set it up

	FVector Location( appAtof( Page1->XText->GetValue().c_str() ), appAtof( Page1->YText->GetValue().c_str() ), appAtof( Page1->ZText->GetValue().c_str() ) );
	wxSize Patches( appAtoi( Page1->XPatchesText->GetValue().c_str() ), appAtoi( Page1->YPatchesText->GetValue().c_str() ) );
	UTerrainLayerSetup* tls = (UTerrainLayerSetup*)Page2->LayerSetupCombo->GetClientData( Page2->LayerSetupCombo->GetSelection() );

	ATerrain* terrain = Cast<ATerrain>( GEditor->GetLevel()->SpawnActor( ATerrain::StaticClass(), NAME_None, Location ) );

	terrain->NumPatchesX = Patches.GetX();
	terrain->NumPatchesY = Patches.GetY();

	if( tls != NULL )
	{
		terrain->Layers.AddZeroed();
		terrain->Layers(0).Setup = tls;
		terrain->Layers(0).AlphaMapIndex = -1;
	}

	terrain->PreEditChange();
	terrain->PostEditChange(NULL);

	GEditor->RedrawLevel( GEditor->Level );

	return 1;
}

/*-----------------------------------------------------------------------------
	WxWizard_Page1.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxNewTerrain_Page1, WxWizardPageSimple )
END_EVENT_TABLE()

WxNewTerrain_Page1::WxNewTerrain_Page1( wxWizard* InParent, wxWizardPage* InPrev, wxWizardPage* InNext )
	: WxWizardPageSimple( InParent, InPrev, InNext, TEXT("ID_PAGE_NEWTERRAIN_1") )
{
	XText = wxDynamicCast( FindWindow( XRCID( "IDEC_X" ) ), wxTextCtrl );
	YText = wxDynamicCast( FindWindow( XRCID( "IDEC_Y" ) ), wxTextCtrl );
	ZText = wxDynamicCast( FindWindow( XRCID( "IDEC_Z" ) ), wxTextCtrl );
	XPatchesText = wxDynamicCast( FindWindow( XRCID( "IDEC_X_PATCHES" ) ), wxTextCtrl );
	YPatchesText = wxDynamicCast( FindWindow( XRCID( "IDEC_Y_PATCHES" ) ), wxTextCtrl );

	XText->SetValue( TEXT("0.0") );
	YText->SetValue( TEXT("0.0") );
	ZText->SetValue( TEXT("0.0") );

	XPatchesText->SetValue( TEXT("16") );
	YPatchesText->SetValue( TEXT("16") );
}

WxNewTerrain_Page1::~WxNewTerrain_Page1()
{
}

/*-----------------------------------------------------------------------------
	WxNewTerrain_Page2.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxNewTerrain_Page2, WxWizardPageSimple )
END_EVENT_TABLE()

WxNewTerrain_Page2::WxNewTerrain_Page2( wxWizard* InParent, wxWizardPage* InPrev, wxWizardPage* InNext )
	: WxWizardPageSimple( InParent, InPrev, InNext, TEXT("ID_PAGE_NEWTERRAIN_2") )
{
	LayerSetupCombo = wxDynamicCast( FindWindow( XRCID( "IDCB_LAYER_SETUP" ) ), wxComboBox );

	LayerSetupCombo->Append( TEXT("None"), (void*)NULL );

	for( TObjectIterator<UObject> It ; It ; ++It )
	{
		if( It->IsA( UTerrainLayerSetup::StaticClass() ) )
		{
			LayerSetupCombo->Append( It->GetName(), (void*)(*It) );
		}
	}

	LayerSetupCombo->SetSelection( 0 );
}

WxNewTerrain_Page2::~WxNewTerrain_Page2()
{
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
