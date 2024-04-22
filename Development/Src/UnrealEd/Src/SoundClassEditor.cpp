/*=============================================================================
	SoundClassEditor.cpp: SoundClass editing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnLinkedObjEditor.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "UnObjectTools.h"
#include "UnLinkedObjDrawUtils.h"
#include "SoundClassEditor.h"
#include "PropertyWindow.h"

void WxSoundClassEditor::OpenNewObjectMenu( void )
{
	WxMBSoundClassEdNewClass Menu( this );
	FTrackPopupMenu Popup( this, &Menu );
	Popup.Show();
}

void WxSoundClassEditor::OpenObjectOptionsMenu( void )
{
	WxMBSoundClassEdClassOptions Menu( this );
	FTrackPopupMenu Popup( this, &Menu );
	Popup.Show();
}

void WxSoundClassEditor::OpenConnectorOptionsMenu( void )
{
	WxMBSoundClassEdConnectorOptions Menu( this );
	FTrackPopupMenu Popup( this, &Menu );
	Popup.Show();
}

void WxSoundClassEditor::DrawObjects( FViewport* Viewport, FCanvas* Canvas )
{
	WxLinkedObjEd::DrawObjects( Viewport, Canvas );
	MasterClass->DrawSoundClasses( AudioDevice, Canvas, SelectedClasses );
}

void WxSoundClassEditor::UpdatePropertyWindow( void )
{
	if( SelectedClasses.Num() )
	{
		ClearWindowList();
		PropertyWindow->SetObjectArray( SelectedClasses, EPropertyWindowFlags::ShouldShowCategories );

		if( SelectedClasses.Num() == 1 )
		{
			FName SoundClassName = SelectedClasses( 0 )->GetFName();
			SetDockingWindowTitle( PropertyWindow, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "PropertiesCaption_F" ), *SoundClassName.ToString() ) ) );
		}
		else
		{
			SetDockingWindowTitle( PropertyWindow, *LocalizeUnrealEd( "PropertiesCaption" ) );
		}
	}

	PropertyWindow->ExpandAllItems();
}

void WxSoundClassEditor::EmptySelection( void )
{
	SelectedClasses.Empty();
}

void WxSoundClassEditor::AddToSelection( UObject* Obj )
{
	check( Obj->IsA( USoundClass::StaticClass() ) );

	if( SelectedClasses.ContainsItem( ( USoundClass* )Obj ) )
	{
		return;
	}

	SelectedClasses.AddItem( ( USoundClass* )Obj );
}

UBOOL WxSoundClassEditor::IsInSelection( UObject* Obj ) const
{
	check( Obj->IsA( USoundClass::StaticClass() ) );

	return( SelectedClasses.ContainsItem( ( USoundClass* )Obj ) );
}

INT WxSoundClassEditor::GetNumSelected( void ) const
{
	return( SelectedClasses.Num() );
}

void WxSoundClassEditor::SetSelectedConnector( FLinkedObjectConnector& Connector )
{
	check( Connector.ConnObj->IsA( USoundClass::StaticClass() ) );

	ConnObj = Connector.ConnObj;
	ConnType = Connector.ConnType;
	ConnIndex = Connector.ConnIndex;
}

FIntPoint WxSoundClassEditor::GetSelectedConnLocation( FCanvas* Canvas )
{
	// If no ConnClass, return origin. This works in the case of connecting a node to the 'root'.
	if( !ConnObj )
	{
		return( FIntPoint( 0, 0 ) );
	}

	USoundClass* ConnClass = CastChecked<USoundClass>( ConnObj );

	FSoundClassEditorData* ConnNodeEdData = MasterClass->EditorData.Find( ConnClass );
	check( ConnNodeEdData );

	return( ConnClass->GetConnectionLocation( Canvas, ConnType, ConnIndex, *ConnNodeEdData ) );
}

void WxSoundClassEditor::MakeConnectionToConnector( FLinkedObjectConnector& Connector )
{
	// Avoid connections to yourself.
	if( Connector.ConnObj == ConnObj )
	{
		return;
	}

	// Normal case - connecting an input of one node to the output of another.
	if( ConnType == LOC_INPUT && Connector.ConnType == LOC_OUTPUT )
	{
		check( Connector.ConnIndex == 0 );

		USoundClass* ConnClass = CastChecked<USoundClass>( ConnObj );
		USoundClass* EndConnNode = CastChecked<USoundClass>( Connector.ConnObj );
		ConnectClasses( ConnClass, ConnIndex, EndConnNode );
	}
	else if( ConnType == LOC_OUTPUT && Connector.ConnType == LOC_INPUT )
	{
		check( ConnIndex == 0 );

		USoundClass* ConnClass = CastChecked<USoundClass>( ConnObj );
		USoundClass* EndConnNode = CastChecked<USoundClass>( Connector.ConnObj );
		ConnectClasses( EndConnNode, Connector.ConnIndex, ConnClass );
	}
}

void WxSoundClassEditor::MakeConnectionToObject( UObject* EndObj )
{
}

/**
 * Called when the user releases the mouse over a link connector and is holding the ALT key.
 * Commonly used as a shortcut to breaking connections.
 *
 * @param	Connector	The connector that was ALT+clicked upon.
 */
void WxSoundClassEditor::AltClickConnector( FLinkedObjectConnector& Connector )
{
	SetSelectedConnector( Connector );

	wxCommandEvent DummyEvent;
	// this works because OnContextBreakLink() is called immediately
	DummyEvent.SetClientData( &Connector ); 
	OnContextBreakLink( DummyEvent );
}

void WxSoundClassEditor::MoveSelectedObjects( INT DeltaX, INT DeltaY )
{
	for( INT i = 0; i < SelectedClasses.Num(); i++ )
	{
		USoundClass* SoundClass = SelectedClasses( i );

		FSoundClassEditorData* EdData = MasterClass->EditorData.Find( SoundClass );
		check( EdData );
		
		EdData->NodePosX += DeltaX;
		EdData->NodePosY += DeltaY;
	}
}

void WxSoundClassEditor::EdHandleKeyInput( FViewport* Viewport, FName Key, EInputEvent Event )
{
	if( Event == IE_Pressed )
	{
		if( Key == KEY_Delete )
		{
			DeleteSelectedClasses();
		}
	}
}

void WxSoundClassEditor::ConnectClasses( USoundClass* ParentClass, INT ChildIndex, USoundClass* ChildClass )
{
	check( ChildIndex >= 0 && ChildIndex < ParentClass->ChildClassNames.Num() );

	if( MasterClass->CheckValidConnection( AudioDevice, ParentClass, ChildClass ) )
	{
		ParentClass->ChildClassNames( ChildIndex ) = ChildClass->GetFName();
		ParentClass->PostEditChange();
	}

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundClassEditor::DeleteSelectedClasses( void )
{
	if( SelectedClasses.Num() > 0 )
	{
		ConnObj = NULL;
		GUnrealEd->GetThumbnailManager()->ClearComponents();

		while( SelectedClasses.Num() )
		{
			USoundClass* DelClass = SelectedClasses( 0 );
			SelectedClasses.Remove( 0 );

			GEditor->GetSelectedObjects()->Deselect( DelClass );
			AudioDevice->RemoveClass( DelClass );

			// Refresh the content browser
			GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectDeleted, DelClass ) );
		}

		// Refresh the sound classes
		AudioDevice->AddClass( NULL );
	}

	UpdatePropertyWindow();
	RefreshViewport();
}

/*-----------------------------------------------------------------------------
	WxSoundClassEditor
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxSoundClassEditor, wxFrame )
	EVT_MENU( IDM_SOUNDCLASS_NEW_CLASS, WxSoundClassEditor::OnContextNewSoundClass )
	EVT_MENU( IDM_SOUNDCLASS_ADD_INPUT, WxSoundClassEditor::OnContextAddInput )
	EVT_MENU( IDM_SOUNDCLASS_DELETE_INPUT, WxSoundClassEditor::OnContextDeleteInput )
	EVT_MENU( IDM_SOUNDCLASS_DELETE_CLASS, WxSoundClassEditor::OnContextDeleteSoundClass )
	EVT_MENU( IDM_SOUNDCLASS_BREAK_LINK, WxSoundClassEditor::OnContextBreakLink )
	EVT_SIZE( WxSoundClassEditor::OnSize )
END_EVENT_TABLE()

WxSoundClassEditor::WxSoundClassEditor( wxWindow* InParent, wxWindowID InID )
	: WxLinkedObjEd( InParent, InID, TEXT( "SoundClassEditor" ) )
{
	// Set the sound cue editor window title to include the sound cue being edited.
	SetTitle( *LocalizeUnrealEd( "SoundClassEditorCaption" ) );

	AudioDevice = GEditor->Client ? GEditor->Client->GetAudioDevice() : NULL;

	MasterClass = AudioDevice->GetSoundClass( ( FName )NAME_Master );
	MasterClass->InitSoundClassEditor( AudioDevice );
}

WxSoundClassEditor::~WxSoundClassEditor( void )
{
	SaveProperties();
}

void WxSoundClassEditor::InitEditor( void )
{
	CreateControls( FALSE );

	LoadProperties();

	// Shift origin so origin is roughly in the middle. Would be nice to use Viewport size, but doesn't seem initialised here...
	LinkedObjVC->Origin2D.X = 150;
	LinkedObjVC->Origin2D.Y = 300;

	BackgroundTexture = LoadObject<UTexture2D>( NULL, TEXT( "EditorMaterials.SoundCueBackground" ), NULL, LOAD_None, NULL );

	ConnObj = NULL;
}

/**
 * Creates the controls for this window
 */
void WxSoundClassEditor::CreateControls( UBOOL bTreeControl )
{
	WxLinkedObjEd::CreateControls( bTreeControl );

	if (PropertyWindow != NULL)
	{
		SetDockingWindowTitle( PropertyWindow, *FString::Printf(LocalizeSecure(LocalizeUnrealEd( "PropertiesCaption_F" ), *MasterClass->GetPathName()) ) );
	}

	wxMenuBar* MenuBar = new wxMenuBar();
	AppendWindowMenu( MenuBar );
	SetMenuBar(MenuBar);
}

/**
 * Used to serialize any UObjects contained that need to be to kept around.
 *
 * @param Ar The archive to serialize with
 */
void WxSoundClassEditor::Serialize( FArchive& Ar )
{
	WxLinkedObjEd::Serialize( Ar );

	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << SelectedClasses << ConnObj;
	}
}

void WxSoundClassEditor::OnContextNewSoundClass( wxCommandEvent& In )
{
	WxDlgGenericStringEntry Dlg;
	INT Result = Dlg.ShowModal( TEXT( "NewClassName" ), TEXT( "EnterNewName" ), TEXT( "SoundClass" ) );
	if( Result != wxID_OK )
	{
		return;
	}

	FName NewClassName = FName( *Dlg.GetEnteredString() );
	UPackage* Package = MasterClass->GetOutermost();
	USoundClass* NewSoundClass = ConstructObject<USoundClass>( USoundClass::StaticClass(), Package, NewClassName, RF_Public );

	// Create new editor data struct and add to map in SoundCue.
	FSoundClassEditorData NewEdData;
	appMemset( &NewEdData, 0, sizeof( FSoundClassEditorData ) );

	NewEdData.NodePosX = ( LinkedObjVC->NewX - LinkedObjVC->Origin2D.X ) / LinkedObjVC->Zoom2D;
	NewEdData.NodePosY = ( LinkedObjVC->NewY - LinkedObjVC->Origin2D.Y ) / LinkedObjVC->Zoom2D;

	MasterClass->EditorData.Set( NewSoundClass, NewEdData );

	AudioDevice->AddClass( NewSoundClass );
	Package->MarkPackageDirty();

	NewSoundClass->PostEditChange();

	ObjectTools::RefreshResourceType( UGenericBrowserType_SoundCue::StaticClass() );
	ObjectTools::RefreshResourceType( UGenericBrowserType_Sounds::StaticClass() );

	RefreshViewport();
}

void WxSoundClassEditor::OnContextAddInput( wxCommandEvent& In )
{
	INT NumSelected = SelectedClasses.Num();
	if( NumSelected != 1 )
	{
		return;
	}

	USoundClass* SoundClass = SelectedClasses( 0 );
	SoundClass->ChildClassNames.AddUniqueItem( NAME_None );

	SoundClass->PostEditChange();

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundClassEditor::OnContextDeleteInput( wxCommandEvent& In )
{
	check( ConnType == LOC_INPUT );

	// Can't delete root input!
	if( ConnObj == MasterClass )
	{
		return;
	}

	USoundClass* ConnClass = CastChecked<USoundClass>( ConnObj );
	check( ConnIndex >= 0 && ConnIndex < ConnClass->ChildClassNames.Num() );

	ConnClass->ChildClassNames.Remove( ConnIndex );

	ConnClass->PostEditChange();

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundClassEditor::OnContextDeleteSoundClass( wxCommandEvent& In )
{
	DeleteSelectedClasses();
}

void WxSoundClassEditor::OnContextBreakLink( wxCommandEvent& In )
{
	USoundClass* ConnClass = CastChecked<USoundClass>( ConnObj );

	if( ConnType == LOC_INPUT )
	{
		check( ConnIndex >= 0 && ConnIndex < ConnClass->ChildClassNames.Num() );
		ConnClass->ChildClassNames.Remove( ConnIndex );

		ConnClass->PostEditChange();
	}
	else if( ConnObj != MasterClass && ConnType == LOC_OUTPUT )
	{
		TArray<USoundClass*> ClassCollection;
		MasterClass->EditorData.GenerateKeyArray( ClassCollection );

		UBOOL bFound = FALSE;
		for( INT ClassIndex = 0; ClassIndex < ClassCollection.Num() && !bFound; ++ClassIndex )
		{
			USoundClass *CurrentClass = ClassCollection( ClassIndex );
			for( INT ChildIndex = 0; ChildIndex < CurrentClass->ChildClassNames.Num(); ++ChildIndex )
			{
				FName ChildClassName = CurrentClass->ChildClassNames( ChildIndex );
				USoundClass* ChildClass = AudioDevice->GetSoundClass( ChildClassName );
				if( ChildClass == ConnClass )
				{
					bFound = TRUE;
					CurrentClass->ChildClassNames.Remove( ChildIndex );

					CurrentClass->PostEditChange();
					break;
				}
			}
		}
	}

	UpdatePropertyWindow();
	RefreshViewport();
}

void WxSoundClassEditor::OnSize( wxSizeEvent& In )
{
	RefreshViewport();
	const wxRect rc = GetClientRect();
	::MoveWindow( ( HWND )LinkedObjVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
	In.Skip();
}

/*-----------------------------------------------------------------------------
	WxMBSoundClassEdNewClass.
-----------------------------------------------------------------------------*/

WxMBSoundClassEdNewClass::WxMBSoundClassEdNewClass( WxSoundClassEditor* ClassEditor )
{
	Append( IDM_SOUNDCLASS_NEW_CLASS, *LocalizeUnrealEd( "NewSoundClass" ), TEXT( "" ) );
}

WxMBSoundClassEdNewClass::~WxMBSoundClassEdNewClass( void )
{
}

/*-----------------------------------------------------------------------------
	WxMBSoundClassEdClassOptions.
-----------------------------------------------------------------------------*/

WxMBSoundClassEdClassOptions::WxMBSoundClassEdClassOptions( WxSoundClassEditor* ClassEditor )
{
	INT NumSelected = ClassEditor->SelectedClasses.Num();

	if( NumSelected == 1 )
	{
		// See if we adding another input would exceed max child nodes.
		USoundClass* SoundClass = ClassEditor->SelectedClasses( 0 );
		Append( IDM_SOUNDCLASS_ADD_INPUT, *LocalizeUnrealEd( "AddChildSlot" ), TEXT( "" ) );
	}

	Append( IDM_SOUNDCLASS_DELETE_CLASS, *LocalizeUnrealEd( "DeleteSelectedClasses" ), TEXT( "" ) );
}

WxMBSoundClassEdClassOptions::~WxMBSoundClassEdClassOptions( void )
{
}

/*-----------------------------------------------------------------------------
	WxMBSoundClassEdConnectorOptions.
-----------------------------------------------------------------------------*/

WxMBSoundClassEdConnectorOptions::WxMBSoundClassEdConnectorOptions( WxSoundClassEditor* ClassEditor )
{
	// Only display the 'Break Link' option if there is a link to break!
	UBOOL bHasConnection = false;

	if( ClassEditor->ConnType == LOC_INPUT )
	{
		USoundClass* ConnClass = CastChecked<USoundClass>( ClassEditor->ConnObj );
		if( ConnClass->ChildClassNames.Num() > 0 )
		{
			bHasConnection = true;
		}
	}
	else if( ClassEditor->ConnObj != ClassEditor->MasterClass && ClassEditor->ConnType == LOC_OUTPUT)
	{
		bHasConnection = true;
	}

	if( bHasConnection )
	{
		Append( IDM_SOUNDCLASS_BREAK_LINK, *LocalizeUnrealEd( "BreakLink" ), TEXT( "" ) );
	}
}

WxMBSoundClassEdConnectorOptions::~WxMBSoundClassEdConnectorOptions( void )
{
}

