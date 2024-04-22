/*=============================================================================
	DlgMapCheck.cpp: UnrealEd dialog for displaying map errors.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgMapCheck.h"
#include "EngineSequenceClasses.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "Kismet.h"

static struct WARNINGBITMAP
{
	FString BitmapName;
	MapCheckType Type;
	MapCheckGroup Group;
	WxBitmap Bitmap;
} WarningBitmaps[] = {
	{ TEXT( "MapCheck_Kismet.png" ),				MCTYPE_NONE,				MCGROUP_KISMET },
	{ TEXT( "MapCheck_CriticalError.png" ),			MCTYPE_CRITICALERROR,		MCGROUP_DEFAULT },
	{ TEXT( "MapCheck_Error.png" ),					MCTYPE_ERROR,				MCGROUP_DEFAULT },
	{ TEXT( "MapCheck_PerformanceWarning.png" ),	MCTYPE_PERFORMANCEWARNING,	MCGROUP_DEFAULT },
	{ TEXT( "MapCheck_Warning.png" ),				MCTYPE_WARNING,				MCGROUP_DEFAULT },
	{ TEXT( "MapCheck_Note.png" ),					MCTYPE_NOTE,				MCGROUP_DEFAULT },
	{ TEXT( "MapCheck_Info.png" ),					MCTYPE_INFO,				MCGROUP_DEFAULT },
};
static INT NumWarningBitmaps = sizeof( WarningBitmaps ) / sizeof( WARNINGBITMAP );

BEGIN_EVENT_TABLE(WxDlgMapCheck, WxTrackableDialog)
	EVT_BUTTON(wxID_CANCEL,						WxDlgMapCheck::OnClose)
	EVT_BUTTON(IDPB_REFRESH,					WxDlgMapCheck::OnRefresh)
	EVT_BUTTON(IDPB_GOTOACTOR,					WxDlgMapCheck::OnGoTo)
	EVT_BUTTON(IDPB_DELETEACTOR,				WxDlgMapCheck::OnDelete)
	EVT_BUTTON(IDPB_GOTOPROPERTIES,				WxDlgMapCheck::OnGotoProperties)
	EVT_BUTTON(IDPB_SHOWHELPPAGE,				WxDlgMapCheck::OnShowHelpPage)
	EVT_BUTTON(IDPB_COPYTOCLIPBOARD,			WxDlgMapCheck::OnCopyToClipboard)
	EVT_LIST_ITEM_ACTIVATED(IDLC_ERRORWARNING,	WxDlgMapCheck::OnItemActivated)

	EVT_LIST_COL_CLICK(IDLC_ERRORWARNING,		WxDlgMapCheck::OnColumnClicked)

	EVT_UPDATE_UI(IDPB_GOTOACTOR,				WxDlgMapCheck::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_DELETEACTOR,				WxDlgMapCheck::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_GOTOPROPERTIES,			WxDlgMapCheck::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_SHOWHELPPAGE,			WxDlgMapCheck::OnUpdateUI)

	EVT_CHECKBOX(IDCB_CRITICALERRORFILTER,		WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_ERRORFILTER,				WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_PERFORMANCEWARNINGFILTER,	WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_WARNINGFILTER,			WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_NOTEFILTER,				WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_INFOFILTER,				WxDlgMapCheck::OnFilter)

	EVT_CHECKBOX(IDCB_KISMETFILTER,				WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_MOBILEPLATFORMFILTER,		WxDlgMapCheck::OnFilter)

	EVT_UPDATE_UI(IDCB_CRITICALERRORFILTER,		WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_ERRORFILTER,				WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_PERFORMANCEWARNINGFILTER,WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_WARNINGFILTER,			WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_NOTEFILTER,				WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_INFOFILTER,				WxDlgMapCheck::OnUpdateFilterUI)

	EVT_UPDATE_UI(IDCB_KISMETFILTER,			WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_MOBILEPLATFORMFILTER,	WxDlgMapCheck::OnUpdateFilterUI)
END_EVENT_TABLE()

WxDlgMapCheck::WxDlgMapCheck(wxWindow* InParent) : 
	WxTrackableDialog(InParent, wxID_ANY, (wxString)*LocalizeUnrealEd("MapCheck"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	DlgPosSizeName(TEXT("DlgMapCheckWithFilters"))
{
}

WxDlgMapCheck::~WxDlgMapCheck()
{
	// Save window position.
	FWindowUtil::SavePosSize(*DlgPosSizeName, this);
}

/**
 *	Initialize the dialog box.
 */
void WxDlgMapCheck::Initialize()
{
	wxBoxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		// Warning List
		wxBoxSizer* ListSizer = new wxBoxSizer(wxVERTICAL);
		{
			ErrorWarningList = new wxListCtrl( this, IDLC_ERRORWARNING, wxDefaultPosition, wxSize(350, 200), wxLC_REPORT );
			ListSizer->Add(ErrorWarningList, 1, wxGROW|wxALL, 5);
		}
		MainSizer->Add(ListSizer, 1, wxGROW|wxALL, 5);		

		// Add buttons
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			{
				CheckBox_CriticalError = new wxCheckBox(this, IDCB_CRITICALERRORFILTER, *LocalizeUnrealEd( "MapCheck_Filter_CriticalError" ) );
				ButtonSizer->Add(CheckBox_CriticalError, 0, wxEXPAND|wxALL, 5);

				CheckBox_Error = new wxCheckBox(this, IDCB_ERRORFILTER, *LocalizeUnrealEd( "MapCheck_Filter_Error" ) );
				ButtonSizer->Add(CheckBox_Error, 0, wxEXPAND|wxALL, 5);

				CheckBox_PerformanceWarning = new wxCheckBox(this, IDCB_PERFORMANCEWARNINGFILTER, *LocalizeUnrealEd( "MapCheck_Filter_PerformanceWarning" ) );
				ButtonSizer->Add(CheckBox_PerformanceWarning, 0, wxEXPAND|wxALL, 5);

				CheckBox_Warning = new wxCheckBox(this, IDCB_WARNINGFILTER, *LocalizeUnrealEd( "MapCheck_Filter_Warning" ) );
				ButtonSizer->Add(CheckBox_Warning, 0, wxEXPAND|wxALL, 5);

				CheckBox_Note = new wxCheckBox(this, IDCB_NOTEFILTER, *LocalizeUnrealEd( "MapCheck_Filter_Note" ) );
				ButtonSizer->Add(CheckBox_Note, 0, wxEXPAND|wxALL, 5);

				CheckBox_Info = new wxCheckBox(this, IDCB_INFOFILTER, *LocalizeUnrealEd( "MapCheck_Filter_Info" ) );
				ButtonSizer->Add(CheckBox_Info, 0, wxEXPAND|wxALL, 5);

				CheckBox_Kismet = new wxCheckBox(this, IDCB_KISMETFILTER, *LocalizeUnrealEd( "MapCheck_Filter_Kismet" ) );
				ButtonSizer->Add(CheckBox_Kismet, 0, wxEXPAND|wxALL, 5);

				CheckBox_MobilePlatform = new wxCheckBox(this, IDCB_MOBILEPLATFORMFILTER, *LocalizeUnrealEd( "MapCheck_Filter_MobilePlatform" ) );
				ButtonSizer->Add(CheckBox_MobilePlatform, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				CloseButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd("Close"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(CloseButton, 0, wxEXPAND|wxALL, 5);

				RefreshButton = new wxButton( this, IDPB_REFRESH, *LocalizeUnrealEd("RefreshF5"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(RefreshButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				GotoButton = new wxButton( this, IDPB_GOTOACTOR, *LocalizeUnrealEd("GoTo"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(GotoButton, 0, wxEXPAND|wxALL, 5);

				PropertiesButton = new wxButton( this, IDPB_GOTOPROPERTIES, *LocalizeUnrealEd("Properties"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(PropertiesButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			DeleteButton = new wxButton( this, IDPB_DELETEACTOR, *LocalizeUnrealEd("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonSizer->Add(DeleteButton, 0, wxEXPAND|wxALL, 5);

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				HelpButton = new wxButton(this, IDPB_SHOWHELPPAGE, *LocalizeUnrealEd("UDNWarningErrorHelpF1"));
				ButtonSizer->Add(HelpButton, 0, wxEXPAND|wxALL, 5);
			
				CopyToClipboardButton = new wxButton(this, IDPB_COPYTOCLIPBOARD, *LocalizeUnrealEd("CopyToClipboard") );
				ButtonSizer->Add(CopyToClipboardButton, 0, wxEXPAND|wxALL, 5 );
			}
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_TOP|wxALL, 5);

	}
	SetSizer(MainSizer);

	// Set an accelerator table to handle hotkey presses for the window.
	wxAcceleratorEntry Entries[3];
	Entries[0].Set(wxACCEL_NORMAL,  WXK_F5,			IDPB_REFRESH);
	Entries[1].Set(wxACCEL_NORMAL,  WXK_DELETE,		IDPB_DELETEACTOR);
	Entries[2].Set(wxACCEL_NORMAL,  WXK_F1,			IDPB_SHOWHELPPAGE);

	wxAcceleratorTable AcceleratorTable(3, Entries);
	SetAcceleratorTable(AcceleratorTable);

	// Fill in the columns
	SetupErrorWarningListColumns();

	// Create the ImageList
	ImageList = new wxImageList( 16, 15 );
	for( INT Bitmap = 0; Bitmap < NumWarningBitmaps; Bitmap++ )
	{
		WarningBitmaps[ Bitmap ].Bitmap.Load( *WarningBitmaps[ Bitmap ].BitmapName );
		ImageList->Add( WarningBitmaps[ Bitmap ].Bitmap );
	}
	ErrorWarningList->AssignImageList( ImageList, wxIMAGE_LIST_SMALL );

	// init filter settings
	ListFilter_CriticalError = TRUE;
	ListFilter_Error = TRUE;
	ListFilter_PerformanceWarning = TRUE;
	ListFilter_Warning = TRUE;
	ListFilter_Note = TRUE;
	ListFilter_Info = TRUE;

	ListFilter_Kismet = TRUE;
	ListFilter_MobilePlatform = TRUE;

	OnSetFilterCheckBoxes();

	// Load window position.
	FWindowUtil::LoadPosSize(*DlgPosSizeName, this, 490, 275, 900, 540);

	ErrorWarningInfoList = &GErrorWarningInfoList;

	SortColumn = -1;
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxDlgMapCheck::OnSelected()
{
	if(!IsShown())
	{
		Show();
	}

	WxTrackableDialog::OnSelected();
}

bool WxDlgMapCheck::Show( bool show )
{
	if( show )
	{
		LoadListCtrl();
	}
	const bool bShown = wxDialog::Show(show);
	
	return bShown;
}

/**
 * Shows the dialog only if there are messages to display.
 */
void WxDlgMapCheck::ShowConditionally()
{
	LoadListCtrl();

	if( ErrorWarningList->GetItemCount() > 0 )
	{
		Show( true );
	}
}

/** Clears out the list of messages appearing in the window. */
void WxDlgMapCheck::ClearMessageList()
{
	ErrorWarningList->DeleteAllItems();
	ErrorWarningInfoList->Empty();
	ReferencedObjects.Empty();
}

/**
 * Freezes the message list.
 */
void WxDlgMapCheck::FreezeMessageList()
{
	ErrorWarningList->Freeze();
}

/**
 * Thaws the message list.
 */
void WxDlgMapCheck::ThawMessageList()
{
	ErrorWarningList->Thaw();
}

/**
 * Returns TRUE if the dialog has any map errors, FALSE if not
 *
 * @return TRUE if the dialog has any map errors, FALSE if not
 */
UBOOL WxDlgMapCheck::HasAnyErrors() const
{
	UBOOL bHasErrors = FALSE;
	if ( ErrorWarningInfoList )
	{
		for ( TArray<FCheckErrorWarningInfo>::TConstIterator InfoIter( *ErrorWarningInfoList ); InfoIter; ++InfoIter )
		{
			const FCheckErrorWarningInfo& CurInfo = *InfoIter;
			if ( CurInfo.Type == MCTYPE_ERROR || CurInfo.Type == MCTYPE_CRITICALERROR )
			{
				bHasErrors = TRUE;
				break;
			}
		}
	}
	return bHasErrors;
}

/**
 * Gets a suitable string from the specified MapCheck filter type and group...
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 */
UBOOL WxDlgMapCheck::IsThisVisible( MapCheckType InType, MapCheckGroup InGroup )
{
	UBOOL bIsVisible = TRUE;
	switch( InType )
	{
		case MCTYPE_CRITICALERROR:
		if( !ListFilter_CriticalError )
		{
			bIsVisible = FALSE;
		}
		break;

		case MCTYPE_ERROR:
		if( !ListFilter_Error )
		{
			bIsVisible = FALSE;
		}
		break;

		case MCTYPE_PERFORMANCEWARNING:
		if( !ListFilter_PerformanceWarning )
		{
			bIsVisible = FALSE;
		}
		break;

		case MCTYPE_WARNING:
		if( !ListFilter_Warning )
		{
			bIsVisible = FALSE;
		}
		break;

		case MCTYPE_NOTE:
		if( !ListFilter_Note )
		{
			bIsVisible = FALSE;
		}
		break;

		case MCTYPE_INFO:
		if( !ListFilter_Info )
		{
			bIsVisible = FALSE;
		}
		break;
	}
	if( InGroup & MCGROUP_KISMET )
	{
		if( !ListFilter_Kismet )
		{
			bIsVisible = FALSE;
		}
	}
	if( InGroup & MCGROUP_MOBILEPLATFORM )
	{
		if( !ListFilter_MobilePlatform )
		{
			bIsVisible = FALSE;
		}
	}
	return bIsVisible;
}

/**
 * Gets a suitable string from the specified MapCheck filter type and group...
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 */
FString WxDlgMapCheck::GetErrorTypeString( MapCheckType InType, MapCheckGroup InGroup )
{
	FString ErrorString;
	switch( InType )
	{
		case MCTYPE_CRITICALERROR:
		ErrorString = LocalizeUnrealEd( "MapCheck_Filter_CriticalError" );
		break;

		case MCTYPE_ERROR:
		ErrorString = LocalizeUnrealEd( "MapCheck_Filter_Error" );
		break;

		case MCTYPE_PERFORMANCEWARNING:
		ErrorString = LocalizeUnrealEd( "MapCheck_Filter_PerformanceWarning" );
		break;

		case MCTYPE_WARNING:
		ErrorString = LocalizeUnrealEd( "MapCheck_Filter_Warning" );
		break;

		case MCTYPE_NOTE:
		ErrorString = LocalizeUnrealEd( "MapCheck_Filter_Note" );
		break;

		case MCTYPE_INFO:
		ErrorString = LocalizeUnrealEd( "MapCheck_Filter_Info" );
		break;
	}
	if( InGroup & MCGROUP_KISMET )
	{
		ErrorString += " ";
		ErrorString += LocalizeUnrealEd( "MapCheck_Group_Kismet" );
	}
	if( InGroup & MCGROUP_MOBILEPLATFORM )
	{
		ErrorString += " ";
		ErrorString += LocalizeUnrealEd( "MapCheck_Group_MobilePlatform" );
	}

	return ErrorString;
}

/**
 * Gets a suitable column label from the specified MapCheck filter type and group...
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 */
FString WxDlgMapCheck::GetErrorTypeColumnLabel( MapCheckType InType, MapCheckGroup InGroup )
{
	FString ErrorString;
	switch( InType )
	{
		case MCTYPE_CRITICALERROR:
		ErrorString = LocalizeUnrealEd( "MapCheck_Column_CriticalError" );
		break;

		case MCTYPE_ERROR:
		ErrorString = LocalizeUnrealEd( "MapCheck_Column_Error" );
		break;

		case MCTYPE_PERFORMANCEWARNING:
		ErrorString = LocalizeUnrealEd( "MapCheck_Column_PerformanceWarning" );
		break;

		case MCTYPE_WARNING:
		ErrorString = LocalizeUnrealEd( "MapCheck_Column_Warning" );
		break;

		case MCTYPE_NOTE:
		ErrorString = LocalizeUnrealEd( "MapCheck_Column_Note" );
		break;

		case MCTYPE_INFO:
		ErrorString = LocalizeUnrealEd( "MapCheck_Column_Info" );
		break;
	}
	if( InGroup & MCGROUP_KISMET )
	{
		ErrorString += " ";
		ErrorString += LocalizeUnrealEd( "MapCheck_Group_Kismet" );
	}
	if( InGroup & MCGROUP_MOBILEPLATFORM )
	{
		ErrorString += " ";
		ErrorString += LocalizeUnrealEd( "MapCheck_Group_MobilePlatform" );
	}

	return ErrorString;
}

/**
 * Gets a suitable icon from the specified MapCheck filter type and group...
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 */
INT WxDlgMapCheck::GetIconIndex( MapCheckType InType, MapCheckGroup InGroup )
{
	INT IconIndex = 0;
	for( INT Bitmap = 0; Bitmap < NumWarningBitmaps; Bitmap++ )
	{
		WARNINGBITMAP& WB = WarningBitmaps[ Bitmap ];
		if( WB.Type == MCTYPE_NONE )
		{
			if( InGroup & WB.Group )
			{
				IconIndex = Bitmap;
				break;
			}
		}
		else
		{
			if( WB.Type == InType )
			{
				IconIndex = Bitmap;
				break;
			}
		}
	}
	return IconIndex;
}

/**
 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 * @param	InMessage				The message to display.
 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
 */
void WxDlgMapCheck::AddItem(MapCheckType InType, MapCheckGroup InGroup, UObject* InActor, const TCHAR* InMessage, const TCHAR* InUDNPage)
{
	// Columns are, from left to right: Type, Level, Actor, Message.

	FCheckErrorWarningInfo ErrorWarningInfo;
	
	UObject *NextOuter = InActor;
	while (NextOuter)
	{
		ULevel *OwnerLevel = Cast<ULevel>(NextOuter);
		if (OwnerLevel)
		{
			ErrorWarningInfo.Level = OwnerLevel;
			break;
		}
		NextOuter = NextOuter->GetOuter();
	}

	ErrorWarningInfo.LevelName = TEXT("<None>");
	ErrorWarningInfo.Name = TEXT("<None>");

	if (InActor)
	{
		ErrorWarningInfo.LevelName = GetLevelOrPackageName(InActor);
		ErrorWarningInfo.Name = InActor->GetName();
	}

	ErrorWarningInfo.UDNHelpString = InUDNPage;
	ErrorWarningInfo.Object = InActor;
	ErrorWarningInfo.Type = InType;
	ErrorWarningInfo.Group = InGroup;
	ErrorWarningInfo.Message = InMessage;
	ErrorWarningInfo.UDNPage = InUDNPage;

	GErrorWarningInfoList.AddItem( ErrorWarningInfo );
	FString ErrorString = GetErrorTypeString( InType, InGroup );
	warnf( TEXT( "MapCheckForError: %s %s %s"), *ErrorString, *InActor->GetFullName(), InMessage ); // print out to the log window so you can copy paste
}

/**
* Loads the list control with the contents of the GErrorWarningInfoList array.
*/
void WxDlgMapCheck::LoadListCtrl()
{
	FString ActorName;
	FString LevelName;
	UBOOL bReferencedByKismet = FALSE;

	ErrorWarningList->DeleteAllItems();
	ReferencedObjects.Empty();

	SortErrorWarningInfoList();

	for( int x = 0 ; x < ErrorWarningInfoList->Num() ; ++x )
	{
		FCheckErrorWarningInfo* ewi = &((*ErrorWarningInfoList)( x ));
		
		LevelName = ewi->LevelName;
		ActorName = ewi->Name;
		INT LevelIndex;
		UBOOL ObjectLevelFound = GWorld->Levels.FindItem(ewi->Level, LevelIndex);
		if ( ewi->Object && ObjectLevelFound)
		{
			ULevel* Level = NULL;
			AActor* Actor = Cast<AActor>(ewi->Object);
			if (Actor)
			{
				Level = Actor->GetLevel();
			}

			UBOOL bIsInKismetGroup = ( ewi->Group & MCGROUP_KISMET );
			if( ( Level ) && ( !bIsInKismetGroup ) && ( ewi->Type != MCTYPE_PERFORMANCEWARNING ) )
			{
				// Determine if the actor is referenced by a kismet sequence.
				USequence* RootSeq = GWorld->GetGameSequence( Level );
				if( RootSeq )
				{
					bReferencedByKismet = RootSeq->ReferencesObject(Actor);
				}
			}
		}

		UBOOL bVisible = TRUE;
		if( IsThisVisible( ewi->Type, ewi->Group ) )
		{
			FString ColumnLabel = GetErrorTypeColumnLabel( ewi->Type, ewi->Group );
			INT IconIndex = GetIconIndex( ewi->Type, ewi->Group );
			if( bReferencedByKismet )
			{
				// Force the Kismet icon
				IconIndex = GetIconIndex( MCTYPE_NONE, MCGROUP_KISMET );
			}
			const LONG Index = ErrorWarningList->InsertItem( x, *ColumnLabel, IconIndex );
			ErrorWarningList->SetItem( Index, 1, *LevelName );
			ErrorWarningList->SetItem( Index, 2, *ActorName );
			ErrorWarningList->SetItem( Index, 3, *ewi->Message );
			ErrorWarningList->SetItemPtrData( Index, (PTRINT)ewi );
		}

		if (ewi->Object && ObjectLevelFound)
		{
			ReferencedObjects.AddUniqueItem(ewi->Object);
		}
	}
}

void WxDlgMapCheck::Serialize(FArchive& Ar)
{
	Ar << ReferencedObjects;
}

/**
 * Removes all items from the map check dialog that pertain to the specified object.
 *
 * @param	Object		The object to match when removing items.
 */
void WxDlgMapCheck::RemoveObjectItems(UObject* Object)
{
	// Remove object from the referenced object's array.
	ReferencedObjects.RemoveItem(Object);

	// Loop through all of the items in our warning list and remove them from the list view if their client data matches the actor we are removing,
	// make sure to iterate through the list backwards so we do not modify ids as we are removing items.
	INT InfoListCount = ErrorWarningInfoList->Num();
	for(long InfoItemIdx=InfoListCount-1; InfoItemIdx>=0;InfoItemIdx--)
	{
		const UObject* ItemObject = (*ErrorWarningInfoList)(InfoItemIdx).Object;
		if ( ItemObject == Object )
		{
			ErrorWarningInfoList->Remove((INT)InfoItemIdx);
		}
	}

	INT ListCount = ErrorWarningList->GetItemCount();
	for(long ItemIdx=ListCount-1; ItemIdx>=0;ItemIdx--)
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIdx);
		if ( ItemInfo->Object == Object )
		{
			ErrorWarningList->DeleteItem(ItemIdx);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Event handlers.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Event handler for when the close button is clicked on. */
void WxDlgMapCheck::OnClose(wxCommandEvent& In)
{
	Show(0);
}

/** Event handler for when the refresh button is clicked on. */
void WxDlgMapCheck::OnRefresh(wxCommandEvent& In)
{
	GEditor->Exec( TEXT("MAP CHECK") );
	LoadListCtrl();
}

/** Event handler for when the goto button is clicked on. */
void WxDlgMapCheck::OnGoTo(wxCommandEvent& In)
{
	const INT NumSelected = ErrorWarningList->GetSelectedItemCount();

	if( NumSelected > 0 )
	{
		const FScopedBusyCursor BusyCursor;
		TArray<AActor*> SelectedActors;
		TArray<UPrimitiveComponent*> SelectedComponents;
		TArray<UObject*> SelectedObjects;
		TArray<USequenceObject*> SequenceObjects;
		AZoneInfo* SelectedZone = NULL;

		long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		while( ItemIndex != -1 )
		{
			const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
			UObject* Object = ItemInfo->Object;
			INT LevelIndex;
			if ( Object && GWorld->Levels.FindItem(ItemInfo->Level, LevelIndex))
			{
				AActor* Actor = Cast<AActor>(Object);
				UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);
				USequenceObject* SequenceObject = Cast<USequenceObject>(Object);
				if ( Actor )
				{
					if( Actor->IsA( AZoneInfo::StaticClass() ) )
					{
						SelectedZone = Cast<AZoneInfo>( Actor );
					}
					else
					{
						if( Actor->IsReferencedByKismet( &SequenceObject ) )
						{
							SequenceObjects.AddItem(SequenceObject);
						}
						SelectedActors.AddItem( Actor );
					}
				}
				else if ( SequenceObject)
				{
					SequenceObjects.AddItem(SequenceObject);
				}
				else if (Component)
				{
					SelectedComponents.AddItem(Component);
				}
				else
				{
					SelectedObjects.AddItem(Object);
				}
			}
			
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		}

		// If a zone was selected, open it up to edit...
		if( SelectedZone )
		{
			GUnrealEd->ShowWorldProperties( TRUE, TEXT( "KillZ" ) );
		}

		// If any components are selected, find the actor(s) they are associated with
		if (SelectedComponents.Num() > 0)
		{
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				for (INT CompIdx = 0; CompIdx < ActorIt->AllComponents.Num(); CompIdx++)
				{
					UPrimitiveComponent* CheckPrim = Cast<UPrimitiveComponent>(ActorIt->AllComponents(CompIdx));
					if (CheckPrim)
					{
						INT TempIdx;
						if (SelectedComponents.FindItem(CheckPrim, TempIdx))
						{
							SelectedActors.AddUniqueItem(*ActorIt);
							break;
						}
					}
				}
			}
		}

		// Selecting actors gets priority
		if (SelectedActors.Num() > 0)
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("MapCheckGoto") );
			GEditor->SelectNone( FALSE, TRUE );
			for ( INT ActorIndex = 0 ; ActorIndex < SelectedActors.Num() ; ++ActorIndex )
			{
				AActor* Actor = SelectedActors(ActorIndex);
				GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
			}
			GEditor->NoteSelectionChange();
			GEditor->MoveViewportCamerasToActor( SelectedActors, FALSE );
		}
		else if (SelectedObjects.Num() > 0)
		{
			GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
		}

		if (SequenceObjects.Num() > 0)
		{
			// Only GoTo the first sequence object, even if multiple are selected.
			USequenceObject* SequenceObject = SequenceObjects(0);

			// If no Kismet windows open - open one now.
			if( GApp->KismetWindows.Num() == 0 )
			{
				WxKismet::OpenKismet( NULL, FALSE, GApp->EditorFrame );
			}
			check( GApp->KismetWindows.Num() > 0 );

			//search in each kismet window
			USequence* LevelSequence = SequenceObject->GetRootSequence();
			for ( INT KismetWindowIndex = 0 ; KismetWindowIndex < GApp->KismetWindows.Num() ; ++KismetWindowIndex )
			{
				WxKismet* KismetWindow = GApp->KismetWindows(KismetWindowIndex);

				// Set the workspace of the kismet window to be the sequence for the actor's level.
				KismetWindow->ChangeActiveSequence( SequenceObject->ParentSequence, TRUE );
				KismetWindow->CenterViewOnSeqObj( SequenceObject );
			}
		}

	}
}

/** Event handler for when the delete button is clicked on. */
void WxDlgMapCheck::OnDelete(wxCommandEvent& In)
{
	const FScopedBusyCursor BusyCursor;
	GEditor->Exec(TEXT("ACTOR SELECT NONE"));

	long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	TArray<AActor*> RemoveList;

	while( ItemIndex != -1 )
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
		UObject* Object = ItemInfo->Object;
		INT LevelIndex;
		if (Object && GWorld->Levels.FindItem(ItemInfo->Level, LevelIndex))
		{
			AActor* Actor = Cast<AActor>(Object);
			if ( Actor )
			{
				GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );

				if(Actor != NULL)
				{
					RemoveList.AddUniqueItem(Actor);
				}
			}
		}
		ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}

	GEditor->Exec( TEXT("DELETE") );
	
	// Loop through all of the actors we deleted and remove any items that reference it.
	for(INT RemoveIdx=0; RemoveIdx < RemoveList.Num(); RemoveIdx++)
	{
		// only remove the item if the delete was successful
		AActor* Actor = RemoveList(RemoveIdx);
		if ( Actor->IsPendingKill() )
		{
			RemoveObjectItems( Actor );
		}
	}
}

/** Event handler for when a the properties button is clicked on */
void WxDlgMapCheck::OnGotoProperties(wxCommandEvent& In)
{
	const INT NumSelected = ErrorWarningList->GetSelectedItemCount();

	if( NumSelected > 0 )
	{
		const FScopedBusyCursor BusyCursor;
		TArray< UObject* > SelectedActors;
		TArray<UPrimitiveComponent*> SelectedComponents;

		// Find all actors or components selected in the error list.
		LONG ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		while( ItemIndex != -1 )
		{
			const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
			UObject* Object = ItemInfo->Object;
			INT LevelIndex;
			if (Object && GWorld->Levels.FindItem(ItemInfo->Level, LevelIndex))
			{
				AActor* Actor = Cast<AActor>(Object);
				UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);

				if ( Actor )
				{
					SelectedActors.AddItem( Actor );
				}
				else if ( Component )
				{
					SelectedComponents.AddItem( Component );
				}
		
			}
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		}

		// If any components are selected, find the actor(s) they are associated with
		for (INT ComponentScan = 0; ComponentScan < SelectedComponents.Num(); ++ComponentScan)
		{
			AActor* ParentActor = SelectedComponents(ComponentScan)->GetOwner();
			if (ParentActor)
			{
				SelectedActors.AddUniqueItem( ParentActor );
			}
		}

		if ( SelectedActors.Num() > 0 )
		{
			// Update the property windows and create one if necessary
			GUnrealEd->ShowActorProperties();
			// Only show properties for the actors that were selected in the error list.
			GUnrealEd->UpdatePropertyWindowFromActorList( SelectedActors );
		}
	}
}

/** Event handler for when a message is clicked on. */
void WxDlgMapCheck::OnItemActivated(wxListEvent& In)
{
	const long ItemIndex = In.GetIndex();
	const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
	UObject* Obj = ItemInfo->Object;
	INT LevelIndex;
	// make sure this object is in the currently loaded world
	if (!GWorld->Levels.FindItem(ItemInfo->Level, LevelIndex))
	{
		return;
	}

	AActor* Actor = Cast<AActor>( Obj );
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Obj);
	USequenceObject* SequenceObject = Cast<USequenceObject>(Obj);
	if (Component)
	{
		UModelComponent* ModelComponent = Cast<UModelComponent>(Component);
		if (ModelComponent)
		{
			ModelComponent->SelectAllSurfaces();
		}
		else
		{
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				for (INT CompIdx = 0; CompIdx < ActorIt->AllComponents.Num(); CompIdx++)
				{
					UPrimitiveComponent* CheckPrim = Cast<UPrimitiveComponent>(ActorIt->AllComponents(CompIdx));
					if (CheckPrim)
					{
						if (CheckPrim == Component)
						{
							Actor = *ActorIt;
							break;
						}
					}
				}
			}
		}
	}

	if ( Actor )
	{
		if( Actor->IsA( AZoneInfo::StaticClass() ) )
		{
			GUnrealEd->ShowWorldProperties( TRUE, TEXT( "KillZ" ) );
		}
		else
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("MapCheckGoto") );
			GEditor->SelectNone( TRUE, TRUE );
			GEditor->SelectActor( Actor, TRUE, NULL, TRUE, TRUE );
			GEditor->MoveViewportCamerasToActor( *Actor, FALSE );
		}
	}
	else if (Obj)
	{
		TArray<UObject*> SelectedObjects;
		SelectedObjects.AddItem( Obj );
		GApp->EditorFrame->SyncBrowserToObjects( SelectedObjects );
	}
	
	// If an actor was produced the mad check issue, Check for references in kismet.
	if ( SequenceObject || ( Actor && Actor->IsReferencedByKismet( &SequenceObject ) ) )
	{
		// If no Kismet windows open - open one now.
		if( GApp->KismetWindows.Num() == 0 )
		{
			WxKismet::OpenKismet( NULL, FALSE, GApp->EditorFrame );
		}
		check( GApp->KismetWindows.Num() > 0 );

		//search in each kismet window
		USequence* LevelSequence = SequenceObject->GetRootSequence();
		for ( INT KismetWindowIndex = 0 ; KismetWindowIndex < GApp->KismetWindows.Num() ; ++KismetWindowIndex )
		{
			WxKismet* KismetWindow = GApp->KismetWindows(KismetWindowIndex);

			// Set the workspace of the kismet window to be the sequence for the actor's level.
			KismetWindow->ChangeActiveSequence( LevelSequence, TRUE );

			KismetWindow->CenterViewOnSeqObj( SequenceObject );
		}
	}

}

/** Event handler for when a column is clicked on. */
void WxDlgMapCheck::OnColumnClicked (wxListEvent& In)
{
	SortColumn = In.GetColumn();
	LoadListCtrl();
}

/** Event handler for when the "Show Help" button is clicked on. */
void WxDlgMapCheck::OnShowHelpPage(wxCommandEvent& In)
{
#if UDK
	// UDK Users get sent to the UDK Homepage
	wxLaunchDefaultBrowser(TEXT("http://udn.epicgames.com/Three/DevelopmentKitHome.html"));
#else
#if SHIPPING_PC_GAME
	// End users don't have access to the secure parts of UDN; send them to the UDN homepage.
	wxLaunchDefaultBrowser(TEXT("http://udn.epicgames.com/Main/WebHome.html"));
#else
	// Send developers, who have access to the secure parts of UDN, to the context-specific page.
	const FString BaseURLString = "https://udn.epicgames.com/Three/MapErrors";

	// Loop through all selected items and launch browser pages for each item.
	long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	while( ItemIndex != -1 )
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
		
		if(ItemInfo->UDNHelpString.Len())
		{
			wxLaunchDefaultBrowser(*FString::Printf(TEXT("%s#%s"), *BaseURLString, *ItemInfo->UDNHelpString));
		}
		else
		{
			wxLaunchDefaultBrowser(*BaseURLString);
		}

		ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}
#endif
#endif
}

/** 
 * Event handler for when the "Copy to Clipboard" button is clicked on. Copies the contents
 * of the dialog to the clipboard. If no items are selected, the entire contents are copied.
 * If any items are selected, only selected items are copied.
 */
void WxDlgMapCheck::OnCopyToClipboard(wxCommandEvent& In)
{
	if ( ErrorWarningList->GetItemCount() > 0 )
	{
		// Determine the search flags to use while iterating through the list elements. If anything at all is selected,
		// only consider selected elements; otherwise copy all of the elements to the clipboard.
		const INT SearchState = ( ErrorWarningList->GetSelectedItemCount() > 0 ) ? wxLIST_STATE_SELECTED : wxLIST_STATE_DONTCARE;
		LONG ItemIndex = wxNOT_FOUND;
		ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, SearchState );
		
		// Store an array of the maximum width of each column (saved as # of characters) in order to nicely format
		// each column into the clipboard later.
		const INT ColumnCount = ErrorWarningList->GetColumnCount();
		TArray<INT> MaxColumnWidths( ColumnCount );

		// Initialize the maximum columns widths to zero
		for ( INT ColIndex = 0; ColIndex < MaxColumnWidths.Num(); ++ColIndex )
		{
			MaxColumnWidths(ColIndex) = 0;
		}

		// The current version of wxWidgets appears to only have an awkward way of retrieving data from the list via a
		// query item. The newest version (2.9.?) adds in the ability to query text by column, but for now, data must be queried
		// repeatedly.
		wxListItem QueryItem;
		QueryItem.SetMask( wxLIST_MASK_TEXT );

		// Iterate over each relevant row, storing each string in each column. Ideally this could be done all in place on one
		// FString, but to ensure nice formatting, the maximum length of each column needs to be determined. Rather than iterate
		// twice, just store the strings off in a nested TArray for convenience.
		TArray< TArray<FString> > RowStrings;
		while ( ItemIndex != wxNOT_FOUND )
		{
			const INT RowIndex = RowStrings.AddZeroed();
			TArray<FString>& CurRowStrings = RowStrings(RowIndex);

			// Set the query item to represent the current row
			QueryItem.SetId( ItemIndex );

			// Query for the string in each column of the current row
			for ( INT ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex )
			{
				// Set the current column in the query, then ask for the string for this column
				QueryItem.SetColumn( ColumnIndex );
				ErrorWarningList->GetItem( QueryItem );

				const wxString& QueryItemText = QueryItem.GetText();
				const INT QueryItemTexLen = QueryItemText.Len();

				CurRowStrings.AddItem( FString( QueryItemText ) );

				// Check if the current column string is the new maximum width for this column index
				if ( QueryItemTexLen > MaxColumnWidths(ColumnIndex) )
				{
					MaxColumnWidths(ColumnIndex) = QueryItemTexLen;
				}
			}

			// Retrieve the next row's index
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, SearchState );
		}

		FString ErrorTextToCopy;

		// Iterate over every relevant string, formatting them nicely into one large FString to copy to the clipboard
		for ( TArray< TArray<FString> >::TConstIterator RowStringIter( RowStrings ); RowStringIter; ++RowStringIter )
		{
			const TArray<FString>& CurRowStrings = *RowStringIter;
			check( CurRowStrings.Num() == ColumnCount );

			for ( TArray<FString>::TConstIterator ColumnIter( CurRowStrings ); ColumnIter; ++ColumnIter )
			{
				// Format the string such that it takes up space equal to the maximum width of its respective column and
				// is left-justified
				if ( ColumnIter.GetIndex() < ColumnCount - 1 )
				{
					ErrorTextToCopy += FString::Printf( TEXT("%-*s"), MaxColumnWidths( ColumnIter.GetIndex() ), **ColumnIter );
					ErrorTextToCopy += TEXT("\t");
				}
				// In the case of the last column, just append the text, it doesn't need any special formatting
				else
				{
					ErrorTextToCopy += *ColumnIter;
				}
			}
			ErrorTextToCopy += TEXT("\r\n");
		}
		appClipboardCopy( *ErrorTextToCopy );
	}
}

/** Event handler for when wx wants to update UI elements. */
void WxDlgMapCheck::OnUpdateUI(wxUpdateUIEvent& In)
{
	switch(In.GetId())
	{
		// Disable these buttons if nothing is selected.
	case IDPB_SHOWHELPPAGE:case IDPB_GOTOACTOR: case IDPB_DELETEACTOR: case IDPB_GOTOPROPERTIES:
		{
			const UBOOL bItemSelected = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED ) != -1;
			In.Enable(bItemSelected == TRUE);
		}
		break;
	}
}

/** Event handler for when the user changes the check status of the filtering categories */
void WxDlgMapCheck::OnFilter(wxCommandEvent& In)
{
	ListFilter_CriticalError = CheckBox_CriticalError->IsChecked();
	ListFilter_Error = CheckBox_Error->IsChecked();
	ListFilter_PerformanceWarning = CheckBox_PerformanceWarning->IsChecked();
	ListFilter_Warning = CheckBox_Warning->IsChecked();
	ListFilter_Note = CheckBox_Note->IsChecked();
	ListFilter_Info = CheckBox_Info->IsChecked();

	ListFilter_Kismet = CheckBox_Kismet->IsChecked();
	ListFilter_MobilePlatform = CheckBox_MobilePlatform->IsChecked();

	LoadListCtrl();
}

/** Event handler for when the filtering check boxes need updating/redrawing */
void WxDlgMapCheck::OnUpdateFilterUI(wxUpdateUIEvent& In)
{
	OnSetFilterCheckBoxes();
}

/** Set filtering checkbox states based on the ListFilter_ member variables */
void WxDlgMapCheck::OnSetFilterCheckBoxes()
{
	CheckBox_CriticalError->SetValue( TRUE == ListFilter_CriticalError );
	CheckBox_Error->SetValue( TRUE == ListFilter_Error );
	CheckBox_PerformanceWarning->SetValue( TRUE == ListFilter_PerformanceWarning );
	CheckBox_Warning->SetValue( TRUE == ListFilter_Warning );
	CheckBox_Note->SetValue( TRUE == ListFilter_Note );
	CheckBox_Info->SetValue( TRUE == ListFilter_Info );

	CheckBox_Kismet->SetValue( TRUE == ListFilter_Kismet );
	CheckBox_MobilePlatform->SetValue( TRUE == ListFilter_MobilePlatform );
}

/** Set up the columns for the ErrorWarningList control. */
void WxDlgMapCheck::SetupErrorWarningListColumns()
{
	if (ErrorWarningList)
	{
		// Columns are, from left to right: Type, Level, Actor, Message.
		ErrorWarningList->InsertColumn( 0, *LocalizeUnrealEd("ErrorListItemType"), wxLIST_FORMAT_LEFT, 100 );
		ErrorWarningList->InsertColumn( 1, *LocalizeUnrealEd("Level"), wxLIST_FORMAT_LEFT, 100 );
		ErrorWarningList->InsertColumn( 2, *LocalizeUnrealEd("Actor"), wxLIST_FORMAT_LEFT, 100 );
		ErrorWarningList->InsertColumn( 3, *LocalizeUnrealEd("Message"), wxLIST_FORMAT_LEFT, 9999 );
	}
}

/** 
 *	Get the level/package name for the given object.
 *
 *	@param	InObject	The object to retrieve the level/package name for.
 *
 *	@return	FString		The name of the level/package.
 */
FString WxDlgMapCheck::GetLevelOrPackageName(UObject* InObject)
{
	AActor* Actor = Cast<AActor>(InObject);
	if (Actor)
	{
		return Actor->GetLevel()->GetOutermost()->GetName();
	}

	return InObject->GetOutermost()->GetName();
}

/** Need a way to get at the virtual GetLevelOrPackageName */
WxDlgMapCheck* DlgMapCheckForCheck = NULL;

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckType,
{ 
	INT result = A.Type - B.Type;
	if (0 == result)
	{
		result = appStricmp(*A.Message, *B.Message);
	}
	return result;
});
/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckLevelName,
{ 
	FString LevelNameA;
	FString LevelNameB;

	if ( A.Object )
	{
		LevelNameA = DlgMapCheckForCheck->GetLevelOrPackageName(A.Object);
	}
	if ( B.Object )
	{
		LevelNameB = DlgMapCheckForCheck->GetLevelOrPackageName(B.Object);
	}
	INT result = appStricmp(*LevelNameA, *LevelNameB);
	if (0 == result)
	{
		result = appStricmp(*A.Message, *B.Message);
	}
	return result;
});

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckObjectName,
{ 
	FString ObjectNameA;
	FString ObjectNameB;

	if ( A.Object )
	{
		ObjectNameA = A.Object->GetName();
	}
	if ( B.Object )
	{
		ObjectNameB = B.Object->GetName();
	}
	INT result = appStricmp(*ObjectNameA,*ObjectNameB);
	if (0 == result)
	{
		result = appStricmp(*A.Message, *B.Message);
	}
	return result;
});

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckMessage,{ return appStricmp(*A.Message, *B.Message); });

/** Sort based on active column. */
void WxDlgMapCheck::SortErrorWarningInfoList()
{
	DlgMapCheckForCheck = this;
	switch (SortColumn)
	{
		//Level Name
		case 1:
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckLevelName)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
		//Object Name
		case 2:
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckObjectName)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
		//Error Message
		case 3:
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckMessage)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
		case 0:
			// fall through
		default:
			// Sort the error warning info by the TYPE (lowest to highest)
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckType)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
	}
	DlgMapCheckForCheck = NULL;
}
