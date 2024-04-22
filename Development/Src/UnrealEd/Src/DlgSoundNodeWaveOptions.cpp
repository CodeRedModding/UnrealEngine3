/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "EngineSoundClasses.h"
#include "DlgSoundNodeWaveOptions.h"
#include "SoundPreviewThread.h"

UBOOL WxDlgSoundNodeWaveOptions::bQualityPreviewerActive = FALSE;

/** 
 * Info used to setup the rows of the sound quality previewer
 */
FPreviewInfo WxDlgSoundNodeWaveOptions::PreviewInfo[] =
{
	FPreviewInfo( 5 ),
	FPreviewInfo( 10 ),
	FPreviewInfo( 15 ),
	FPreviewInfo( 20 ),
	FPreviewInfo( 25 ),
	FPreviewInfo( 30 ),
	FPreviewInfo( 35 ),
	FPreviewInfo( 40 ),
	FPreviewInfo( 50 ),
	FPreviewInfo( 60 ),
	FPreviewInfo( 70 ) 
};

FPreviewInfo::FPreviewInfo( INT Quality )
{
	QualitySetting = Quality;

	OriginalSize = 0;

	DecompressedOggVorbis = NULL;
	DecompressedXMA = NULL;
	DecompressedPS3 = NULL;

	OggVorbisSize = 0;
	XMASize = 0;
	PS3Size = 0;
}

void FPreviewInfo::Cleanup( void )
{
	if( DecompressedOggVorbis )
	{
		appFree( DecompressedOggVorbis );
		DecompressedOggVorbis = NULL;
	}

	if( DecompressedXMA )
	{
		appFree( DecompressedXMA );
		DecompressedXMA = NULL;
	}

	if( DecompressedPS3 )
	{
		appFree( DecompressedPS3 );
		DecompressedPS3 = NULL;
	}

	OriginalSize = 0;
	OggVorbisSize = 0;
	XMASize = 0;
	PS3Size = 0;
}

/** 
 * Sound quality previewer
 * 
 * Converts the sound node wave to the native format of all available platforms at various quality settings
 */
BEGIN_EVENT_TABLE( WxDlgSoundNodeWaveOptions, WxTrackableDialog )
	EVT_BUTTON( ID_DialogSoundNodeWaveOptions_OK, WxDlgSoundNodeWaveOptions::OnOK )
	EVT_BUTTON( ID_DialogSoundNodeWaveOptions_Cancel, WxDlgSoundNodeWaveOptions::OnCancel )
	EVT_LIST_ITEM_ACTIVATED( ID_DialogSoundNodeWaveOptions_List, WxDlgSoundNodeWaveOptions::OnListItemDoubleClicked )
	EVT_LIST_ITEM_SELECTED( ID_DialogSoundNodeWaveOptions_List, WxDlgSoundNodeWaveOptions::OnListItemSingleClicked )
	EVT_TIMER( SOUNDNODEWAVE_TIMER_ID, WxDlgSoundNodeWaveOptions::OnTimerImpulse )
	EVT_CLOSE( WxDlgSoundNodeWaveOptions::OnClose )
END_EVENT_TABLE()

WxDlgSoundNodeWaveOptions::WxDlgSoundNodeWaveOptions( wxWindow* InParent, USoundNodeWave* Wave ) : 
    WxTrackableDialog( InParent, wxID_ANY, TEXT( "DlgSoundNodeWaveOptions" ), wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU )
	, SoundNode( Wave )
	, ThreadTimer( this, SOUNDNODEWAVE_TIMER_ID )
	, CurrentIndex( 0 )			
{
	bQualityPreviewerActive = TRUE;

	wxBoxSizer* MainSizer = NULL;
	wxBoxSizer* HorizontalSizer = NULL;
	wxBoxSizer* ButtonSizer = NULL;
	bTaskEnded = FALSE;
	
	// Setup dialog layout
	MainSizer = new wxBoxSizer( wxVERTICAL );
	// List control and OK/Cancel buttons
	HorizontalSizer = new wxBoxSizer( wxHORIZONTAL );

	// OK button
	ButtonSizer = new wxBoxSizer( wxVERTICAL );

	// List
	ListOptions = new wxListCtrl( this, ID_DialogSoundNodeWaveOptions_List, wxDefaultPosition, wxDefaultSize, wxLC_REPORT );
	HorizontalSizer->Add( ListOptions, 1, wxEXPAND | wxALL, 5 );
	
	// "OK"
	ButtonOK = new wxButton( this, ID_DialogSoundNodeWaveOptions_OK, TEXT( "OK" ) );
	ButtonSizer->Add( ButtonOK, 0, wxEXPAND | wxALL, 5 );

	// "Cancel"
	ButtonCancel = new wxButton( this, ID_DialogSoundNodeWaveOptions_Cancel, TEXT( "Cancel" ) );
	ButtonSizer->Add( ButtonCancel, 0, wxEXPAND | wxALL, 5 );

	// default state
	ButtonOK->Enable( TRUE );
	
	// Update Sizer
	HorizontalSizer->Add( ButtonSizer, 0, wxALL, 5 );
	MainSizer->Add( HorizontalSizer, 1, wxEXPAND | wxALL, 5 );
	SetSizer( MainSizer );
	
	FLocalizeWindow( this );
	
	// cache the title, so we can use this for progress
	OriginalDialogTitle = GetLabel().c_str();

	// Load the window position
	FWindowUtil::LoadPosSize( TEXT( "DlgSoundNodeWaveOptions" ), this, -1, -1, 900, 300 );
	Layout();

	StopPreviewSound();

	SoundPreviewThreadRunnable = NULL;
	SoundPreviewThread = NULL;
	/** Raise event every 100ms	*/
	ThreadTimer.Start( 100 ); 

	// Compress all the sounds at the varying compression levels
	CreateCompressedWaves();

	// Feed the data into the dialog
	RefreshOptionsList();

	Show( true );
}

WxDlgSoundNodeWaveOptions::~WxDlgSoundNodeWaveOptions( void )
{
	StopPreviewSound();
	
	// Save out the window's 
	FWindowUtil::SavePosSize( TEXT( "DlgSoundNodeWaveOptions" ), this );
}

/** 
 * Frees all the allocated assets 
 */
void WxDlgSoundNodeWaveOptions::Cleanup( void )
{
	if (!bTaskEnded)
	{
		SetLabel(*FString::Printf(TEXT("%s   (Canceling conversion...)"), *OriginalDialogTitle));
	}

	SoundPreviewThreadRunnable->Stop();
	SoundPreviewThread->WaitForCompletion();
	GThreadFactory->Destroy( SoundPreviewThread );
	SoundPreviewThread = NULL;
	delete SoundPreviewThreadRunnable;
	SoundPreviewThreadRunnable = NULL;

	SetLabel(*OriginalDialogTitle);

	StopPreviewSound();

	SoundNode->RawPCMData = NULL;
	SoundNode->DecompressionType = DTYPE_Setup;

	for( INT i = 0; i < sizeof( PreviewInfo ) / sizeof( FPreviewInfo ); i++ )
	{
		PreviewInfo[i].Cleanup();
	}

	Show( false );
	bQualityPreviewerActive = FALSE;

	// Refresh
	const DWORD UpdateMask = CBR_UpdatePackageListUI|CBR_UpdateAssetListUI;
	GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, SoundNode));
}

/**
 * Used to serialize any UObjects contained that need to be to kept around.
 *
 * @param Ar The archive to serialize with
 */
void WxDlgSoundNodeWaveOptions::Serialize( FArchive& Ar )
{
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << SoundNode;
	}
}

/** 
 * Converts the raw wave data using all the quality settings in the PreviewInfo table
 */
void WxDlgSoundNodeWaveOptions::CreateCompressedWaves( void )
{
	OriginalQuality = SoundNode->CompressionQuality;
	INT Count = sizeof( PreviewInfo ) / sizeof( FPreviewInfo );

	SoundPreviewThreadRunnable = new FSoundPreviewThread( Count, SoundNode, PreviewInfo );

	EThreadPriority SoundPreviewThreadPrio = TPri_Normal;
    SoundPreviewThread = GThreadFactory->CreateThread( SoundPreviewThreadRunnable, TEXT( "SoundPrieviewThread" ), 0, 0, 0, SoundPreviewThreadPrio );
}

/** 
 * Refreshes the options list. 
 */
void WxDlgSoundNodeWaveOptions::RefreshOptionsList( void )
{
	FString Temp;

	ListOptions->ClearAll();
	
	ListOptions->InsertColumn( 0, TEXT( "Quality" ), wxLIST_FORMAT_CENTRE, 60 );
	ListOptions->InsertColumn( 1, TEXT( "Original DataSize(Kb)" ), wxLIST_FORMAT_RIGHT, 120 );
	ListOptions->InsertColumn( 2, TEXT( "Vorbis DataSize(Kb)" ), wxLIST_FORMAT_RIGHT, 120 );
	ListOptions->InsertColumn( 3, TEXT( "XMA DataSize(Kb)" ), wxLIST_FORMAT_RIGHT, 120 );
	ListOptions->InsertColumn( 4, TEXT( "PS3 DataSize(Kb)" ), wxLIST_FORMAT_RIGHT, 120 );

	for( INT i = 0; i < sizeof( PreviewInfo ) / sizeof( FPreviewInfo ); i++ )
	{
		Temp = FString::Printf( TEXT( "%d" ), PreviewInfo[i].QualitySetting );
		long ItemIdx = ListOptions->InsertItem( i, *Temp );
		ListOptions->SetItem( ItemIdx, 0, *Temp );

		if( PreviewInfo[i].OriginalSize == 0 )
		{
			Temp = FString::Printf( TEXT( "%.2f (0.0%%)" ), PreviewInfo[i].OriginalSize / 1024.0f );
			ListOptions->SetItem( ItemIdx, 1, *Temp );

			Temp = FString::Printf( TEXT( "%.2f (0.0%%)" ), PreviewInfo[i].OggVorbisSize / 1024.0f );
			ListOptions->SetItem( ItemIdx, 2, *Temp );

			Temp = FString::Printf( TEXT( "%.2f (0.0%%)" ), PreviewInfo[i].XMASize / 1024.0f );
			ListOptions->SetItem( ItemIdx, 3, *Temp );

			Temp = FString::Printf( TEXT( "%.2f (0.0%%)" ), PreviewInfo[i].PS3Size / 1024.0f );
			ListOptions->SetItem( ItemIdx, 4, *Temp );	
		}
		else
		{
			Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].OriginalSize / 1024.0f, PreviewInfo[i].OriginalSize * 100.0f / PreviewInfo[i].OriginalSize );
			ListOptions->SetItem( ItemIdx, 1, *Temp );

			Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].OggVorbisSize / 1024.0f, PreviewInfo[i].OggVorbisSize * 100.0f / PreviewInfo[i].OriginalSize );
			ListOptions->SetItem( ItemIdx, 2, *Temp );

			Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].XMASize / 1024.0f, PreviewInfo[i].XMASize * 100.0f / PreviewInfo[i].OriginalSize );
			ListOptions->SetItem( ItemIdx, 3, *Temp );

			Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].PS3Size / 1024.0f, PreviewInfo[i].PS3Size * 100.0f / PreviewInfo[i].OriginalSize );
			ListOptions->SetItem( ItemIdx, 4, *Temp );	
		}
	}

	ItemIndex = -1;
}

/** 
 * Starts playing the sound preview. 
 */
void WxDlgSoundNodeWaveOptions::PlayPreviewSound( void )
{
	// As this sound now has RawPCMData, this will set it to DTYPE_Preview
	SoundNode->DecompressionType = DTYPE_Setup;

	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, SoundNode );
	if( AudioComponent )
	{
		AudioComponent->Stop();

		AudioComponent->bUseOwnerLocation = FALSE;
		AudioComponent->bAutoDestroy = FALSE;
		AudioComponent->Location = FVector( 0.0f, 0.0f, 0.0f );
		AudioComponent->bIsUISound = TRUE;
		AudioComponent->bAllowSpatialization = FALSE;
		AudioComponent->bReverb = FALSE;
		AudioComponent->bCenterChannelOnly = FALSE;

		AudioComponent->Play();	
	}
}

/** 
 * Stops the sound preview if it is playing. 
 */
void WxDlgSoundNodeWaveOptions::StopPreviewSound( void )
{
	UAudioComponent* AudioComponent = GEditor->GetPreviewAudioComponent( NULL, NULL );
	if( AudioComponent )
	{
		AudioComponent->Stop();
	}
}

/**
 * Callback for when the user clicks OK.
 */
void WxDlgSoundNodeWaveOptions::OnOK( wxCommandEvent& In )
{
	if( ItemIndex > -1 )
	{
		SoundNode->CompressionQuality = PreviewInfo[ItemIndex].QualitySetting;
		SoundNode->MarkPackageDirty();
	}
	else
	{
		SoundNode->CompressionQuality = OriginalQuality;
	}

	Cleanup();
}

/**
 * Callback for when the user clicks Cancel.
 */
void WxDlgSoundNodeWaveOptions::OnCancel( wxCommandEvent& In )
{
	SoundNode->CompressionQuality = OriginalQuality;
	Cleanup();
}

/**
 * Callback for when the user clicks the close button.
 */
void WxDlgSoundNodeWaveOptions::OnClose( wxCloseEvent& In )
{
	SoundNode->CompressionQuality = OriginalQuality;
	Cleanup();
}

/**
 * Callback for when the user single clicks an item on the list.
 */
void WxDlgSoundNodeWaveOptions::OnListItemSingleClicked( wxListEvent& In )
{
	ItemIndex = In.GetIndex();
}

/**
 * Callback for when the user double clicks an item on the list.
 */
void WxDlgSoundNodeWaveOptions::OnListItemDoubleClicked( wxListEvent& In )
{
	ItemIndex = In.GetIndex();

	wxPoint MouseLocalPosition = ScreenToClient( ::wxGetMousePosition() );
	
	INT ColumnNumber = CalculateColumnNumber( MouseLocalPosition );

	if( ItemIndex > CurrentIndex ) 
	{
		return;
	}
	
	if( ColumnNumber == 2 && PreviewInfo[ItemIndex].DecompressedOggVorbis ) 
	{
		SoundNode->RawPCMData = PreviewInfo[ItemIndex].DecompressedOggVorbis;
		PlayPreviewSound();
	}
	else if( ColumnNumber == 3 && PreviewInfo[ItemIndex].DecompressedXMA )
	{
		SoundNode->RawPCMData = PreviewInfo[ItemIndex].DecompressedXMA;
		PlayPreviewSound();
	}
	else if( ColumnNumber == 4 && PreviewInfo[ItemIndex].DecompressedPS3 )
	{
		SoundNode->RawPCMData = PreviewInfo[ItemIndex].DecompressedPS3;
		PlayPreviewSound();
	}
}

/**
 * Calculate column number from window local mouse position.
 */
INT WxDlgSoundNodeWaveOptions::CalculateColumnNumber( wxPoint Position )
{
	INT AccumulatedWidth = 0;
	INT ColumnNumber = -1;

	for( INT i = 0; i < ListOptions->GetColumnCount(); i++ )
	{
		AccumulatedWidth += ListOptions->GetColumnWidth( i );
		
		if( Position.x < AccumulatedWidth )
		{
			ColumnNumber = i;
			break;
		}
	}
	return( ColumnNumber );
}

/**
 * Called on every timer impulse, call RefreshOptionsList() when thread job finished.
 */
void WxDlgSoundNodeWaveOptions::OnTimerImpulse( wxTimerEvent &Event )
{
	if( SoundPreviewThreadRunnable )
	{
		static INT NumDots = 0;
		NumDots++;
		if (NumDots == 4)
		{
			NumDots = 0;
		}

		SetLabel(*FString::Printf(TEXT("%s   (Converting sounds %d / %d.%c%c%c)"), *OriginalDialogTitle, CurrentIndex + 1, SoundPreviewThreadRunnable->GetCount(),
			NumDots > 0 ? '.' : ' ', NumDots > 1 ? '.' : ' ', NumDots > 2 ? '.' : ' '));

		if( CurrentIndex < SoundPreviewThreadRunnable->GetIndex() )
		{
			CurrentIndex = SoundPreviewThreadRunnable->GetIndex();
			UpdateOptionsList();
			
			if( CurrentIndex == SoundPreviewThreadRunnable->GetCount() )
			{
				SetLabel(*OriginalDialogTitle);
				bTaskEnded = TRUE;
			}
		}
	}
}

/** 
 * Updates the options list. 
 */
void WxDlgSoundNodeWaveOptions::UpdateOptionsList( void )
{
	FString Temp;

	for( INT i = 0; i < CurrentIndex; i++ )
	{
		Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].OriginalSize / 1024.0f, PreviewInfo[i].OriginalSize * 100.0f / PreviewInfo[i].OriginalSize );
		ListOptions->SetItem( i, 1, *Temp );

		Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].OggVorbisSize / 1024.0f, PreviewInfo[i].OggVorbisSize * 100.0f / PreviewInfo[i].OriginalSize );
		ListOptions->SetItem( i, 2, *Temp );

		Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].XMASize / 1024.0f, PreviewInfo[i].XMASize * 100.0f / PreviewInfo[i].OriginalSize );
		ListOptions->SetItem( i, 3, *Temp );

		Temp = FString::Printf( TEXT( "%.2f (%.1f%%)" ), PreviewInfo[i].PS3Size / 1024.0f, PreviewInfo[i].PS3Size * 100.0f / PreviewInfo[i].OriginalSize );
		ListOptions->SetItem( i, 4, *Temp );	
	}
}

// end

