/*=============================================================================
	DlgLightingResults.cpp: UnrealEd dialog for displaying lighting build 
						    errors and warnings.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgLightingResults.h"
#include "EngineSequenceClasses.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "LevelBrowser.h"
#include "SurfaceIterators.h"

BEGIN_EVENT_TABLE(WxDlgLightingResults, WxTrackableDialog)
	EVT_BUTTON(wxID_CANCEL,						WxDlgLightingResults::OnClose)
	EVT_BUTTON(IDPB_REFRESH,					WxDlgLightingResults::OnRefresh)
	EVT_BUTTON(IDPB_GOTOACTOR,					WxDlgLightingResults::OnGoTo)
	EVT_BUTTON(IDPB_DELETEACTOR,				WxDlgLightingResults::OnDelete)
	EVT_BUTTON(IDPB_GOTOPROPERTIES,				WxDlgLightingResults::OnGotoProperties)
	EVT_BUTTON(IDPB_SHOWHELPPAGE,				WxDlgLightingResults::OnShowHelpPage)
	EVT_BUTTON(IDPB_COPYTOCLIPBOARD,			WxDlgLightingResults::OnCopyToClipboard)
	EVT_LIST_ITEM_ACTIVATED(IDLC_ERRORWARNING,	WxDlgLightingResults::OnItemActivated)

	EVT_LIST_COL_CLICK(IDLC_ERRORWARNING,		WxDlgLightingResults::OnColumnClicked)

	EVT_UPDATE_UI(IDPB_GOTOACTOR,				WxDlgLightingResults::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_DELETEACTOR,				WxDlgLightingResults::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_GOTOPROPERTIES,			WxDlgLightingResults::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_SHOWHELPPAGE,			WxDlgLightingResults::OnUpdateUI)

	EVT_CHECKBOX(IDCB_CRITICALERRORFILTER,		WxDlgLightingResults::OnFilter)
	EVT_CHECKBOX(IDCB_ERRORFILTER,				WxDlgLightingResults::OnFilter)
	EVT_CHECKBOX(IDCB_PERFORMANCEWARNINGFILTER,	WxDlgLightingResults::OnFilter)
	EVT_CHECKBOX(IDCB_WARNINGFILTER,			WxDlgLightingResults::OnFilter)
	EVT_CHECKBOX(IDCB_NOTEFILTER,				WxDlgLightingResults::OnFilter)
	EVT_CHECKBOX(IDCB_INFOFILTER,				WxDlgLightingResults::OnFilter)

	EVT_CHECKBOX(IDCB_KISMETFILTER,				WxDlgLightingResults::OnFilter)
	EVT_CHECKBOX(IDCB_MOBILEPLATFORMFILTER,		WxDlgLightingResults::OnFilter)

	EVT_UPDATE_UI(IDCB_CRITICALERRORFILTER,		WxDlgLightingResults::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_ERRORFILTER,				WxDlgLightingResults::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_PERFORMANCEWARNINGFILTER,WxDlgLightingResults::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_WARNINGFILTER,			WxDlgLightingResults::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_NOTEFILTER,				WxDlgLightingResults::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_INFOFILTER,				WxDlgLightingResults::OnUpdateFilterUI)

	EVT_UPDATE_UI(IDCB_KISMETFILTER,			WxDlgLightingResults::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_MOBILEPLATFORMFILTER,	WxDlgLightingResults::OnUpdateFilterUI)
END_EVENT_TABLE()

static TArray<FCheckErrorWarningInfo> GLightingErrorWarningInfoList;

WxDlgLightingResults::WxDlgLightingResults(wxWindow* InParent) : 
	WxDlgMapCheck(InParent)
{
}

WxDlgLightingResults::~WxDlgLightingResults()
{
}

/**
 *	Initialize the dialog box.
 */
void WxDlgLightingResults::Initialize()
{
	WxDlgMapCheck::Initialize();

	SetTitle((wxString)*LocalizeUnrealEd("LightingResults"));

	// Load window position.
	DlgPosSizeName = TEXT("DlgLightingResultsWithFilters");
	FWindowUtil::LoadPosSize(*DlgPosSizeName, this, 490, 275, 900, 540);

	ErrorWarningInfoList = &GLightingErrorWarningInfoList;

	SortColumn = -1;
}

/**
 * Shows the dialog only if there are warnings or errors to display.
 */
void WxDlgLightingResults::ShowConditionally()
{
	LoadListCtrl();

	for (INT MsgIdx = 0; MsgIdx < ErrorWarningList->GetItemCount(); ++MsgIdx)
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(MsgIdx);
		if ((ItemInfo->Type == MCTYPE_CRITICALERROR) || (ItemInfo->Type == MCTYPE_ERROR) || (ItemInfo->Type == MCTYPE_WARNING))
		{
			Show(true);
			break;
		}
	}
}

/**
 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 * @param	InObject				Object associated with the message; can be NULL.
 * @param	InMessage				The message to display.
 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
 */
void WxDlgLightingResults::AddItem(MapCheckType InType, UObject* InObject, const TCHAR* InMessage, const TCHAR* InUDNPage, MapCheckGroup InGroup)
{
	// Columns are, from left to right: Level/Package, Object, Message.

	FCheckErrorWarningInfo ErrorWarningInfo;

	ErrorWarningInfo.UDNHelpString = InUDNPage;
	ErrorWarningInfo.Object = InObject;
	ErrorWarningInfo.Type = InType;
	ErrorWarningInfo.Group = InGroup;
	ErrorWarningInfo.Message = InMessage;
	ErrorWarningInfo.UDNPage = InUDNPage;

	GLightingErrorWarningInfoList.AddUniqueItem( ErrorWarningInfo );
}

/** Event handler for when the refresh button is clicked on. */
void WxDlgLightingResults::OnRefresh(wxCommandEvent& In)
{
	LoadListCtrl();
}


/** Event handler for when the goto button is clicked on. */
void WxDlgLightingResults::OnGoTo(wxCommandEvent& In)
{
	// Clear existing selections
	GEditor->SelectNone(FALSE, TRUE);

	// Do the base processing
	WxDlgMapCheck::OnGoTo(In);

	// Now, special handle the BSP mappings...
	const INT NumSelected = ErrorWarningList->GetSelectedItemCount();
	if (NumSelected > 0)
	{
		const FScopedBusyCursor BusyCursor;
		TArray<ULightmappedSurfaceCollection*> SelectedSurfaceCollections;
		long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		while (ItemIndex != -1)
		{
			const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
			UObject* Object = ItemInfo->Object;
			if (Object)
			{
				ULightmappedSurfaceCollection* SelectedSurfaceCollection = Cast<ULightmappedSurfaceCollection>(Object);
				if (SelectedSurfaceCollection)
				{
					SelectedSurfaceCollections.AddItem(SelectedSurfaceCollection);
				}
			}
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		}

		// If any surface collections are selected, select them in the editor
		if (SelectedSurfaceCollections.Num() > 0)
		{
			TArray<AActor*> SelectedActors;
			for (INT CollectionIdx = 0; CollectionIdx < SelectedSurfaceCollections.Num(); CollectionIdx++)
			{
				ULightmappedSurfaceCollection* SurfaceCollection = SelectedSurfaceCollections(CollectionIdx);
				if (SurfaceCollection)
				{
					// Select the surfaces in this mapping
					for (INT SurfaceIdx = 0; SurfaceIdx < SurfaceCollection->Surfaces.Num(); SurfaceIdx++)
					{
						INT SurfaceIndex = SurfaceCollection->Surfaces(SurfaceIdx);
						FBspSurf& Surf = SurfaceCollection->SourceModel->Surfs(SurfaceIndex);
						SurfaceCollection->SourceModel->ModifySurf(SurfaceIndex, 0);
						Surf.PolyFlags |= PF_Selected;
						if (Surf.Actor)
						{
							SelectedActors.AddUniqueItem(Surf.Actor);
						}
					}
				}
			}

			// Add the brushes to the selected actors list...
			if (SelectedActors.Num() > 0)
			{
				GEditor->MoveViewportCamerasToActor(SelectedActors, FALSE);
			}

			GEditor->NoteSelectionChange();
		}
	}
}

/** Event handler for when a message is clicked on. */
void WxDlgLightingResults::OnItemActivated(wxListEvent& In)
{
	const long ItemIndex = In.GetIndex();

	const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
	UObject* Obj = ItemInfo->Object;
	ULightmappedSurfaceCollection* SurfaceCollection = Cast<ULightmappedSurfaceCollection>(Obj);
	if (SurfaceCollection)
	{
		// Deselect all selected object...
		GEditor->SelectNone( TRUE, TRUE );

		// Select the surfaces in this mapping
		TArray<AActor*> SelectedActors;
		for (INT SurfaceIdx = 0; SurfaceIdx < SurfaceCollection->Surfaces.Num(); SurfaceIdx++)
		{
			INT SurfaceIndex = SurfaceCollection->Surfaces(SurfaceIdx);
			FBspSurf& Surf = SurfaceCollection->SourceModel->Surfs(SurfaceIndex);
			SurfaceCollection->SourceModel->ModifySurf(SurfaceIndex, 0);
			Surf.PolyFlags |= PF_Selected;
			if (Surf.Actor)
			{
				SelectedActors.AddUniqueItem(Surf.Actor);
			}
		}

		// Add the brushes to the selected actors list...
		if (SelectedActors.Num() > 0)
		{
			GEditor->MoveViewportCamerasToActor(SelectedActors, FALSE);
		}

		GEditor->NoteSelectionChange();

		return;
	}

	WxDlgMapCheck::OnItemActivated(In);
}

/**
 * Dialog that displays the StaticMesh lighting info for the selected level(s).
 */
BEGIN_EVENT_TABLE(WxDlgStaticMeshLightingInfo, WxTrackableDialog)
	// Done via the EventHandler 
END_EVENT_TABLE()

static TArray<FStaticMeshLightingInfo> GStaticMeshLightingInfoList;
WxDlgStaticMeshLightingInfo::ESortMethod GStaticMeshLightingInfo_SortMethod = WxDlgStaticMeshLightingInfo::SORTBY_StaticMesh;
WxDlgStaticMeshLightingInfo::ELevelOptions GStaticMeshLightingInfo_ScanMethod = WxDlgStaticMeshLightingInfo::DLGSMLI_AllLevels;

// NOTE: There is a LOT of similar functionality in this dialog as there is in the DlgMapCheck.
// However, the data stored in the list was different enough to not justify re-working that dialog to support it.

WxDlgStaticMeshLightingInfo::WxDlgStaticMeshLightingInfo(wxWindow* InParent) : 
	WxTrackableDialog(InParent, wxID_ANY, (wxString)*LocalizeUnrealEd("StaticMeshLightingInfo"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	// Create and layout the controls
	wxBoxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		// Warning List
		wxBoxSizer* ListSizer = new wxBoxSizer(wxVERTICAL);
		{
			StaticMeshList = new wxListCtrl( this, wxID_ANY, wxDefaultPosition, wxSize(350, 200), wxLC_REPORT );
			ListSizer->Add(StaticMeshList, 1, wxGROW|wxALL, 5);
		}
		MainSizer->Add(ListSizer, 1, wxGROW|wxALL, 5);		

		// Add buttons
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			{
				CloseButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd("Close"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(CloseButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				RescanAllLevelsButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("RescanAllLevels"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(RescanAllLevelsButton, 0, wxEXPAND|wxALL, 5);
				RescanSelectedLevelsButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("RescanSelectedLevels"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(RescanSelectedLevelsButton, 0, wxEXPAND|wxALL, 5);
				RescanCurrentLevelButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("RescanCurrentLevel"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(RescanCurrentLevelButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				GotoButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("GoTo"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(GotoButton, 0, wxEXPAND|wxALL, 5);

				SyncButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("Sync"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SyncButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				SwapButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("Swap"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SwapButton, 0, wxEXPAND|wxALL, 5);
				SwapExButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("SwapEx"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SwapExButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				SetToVertexButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("SetToVertexMapping"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SetToVertexButton, 0, wxEXPAND|wxALL, 5);
				SetToTextureButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("SetToTextureMapping"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SetToTextureButton, 0, wxEXPAND|wxALL, 5);
				SetToTextureExButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("SetToTextureMappingEx"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SetToTextureExButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			// These buttons will be hidden but are required for accelerator table... (ie hot-keys)
			{
				SelectAllButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("SelectAll"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SelectAllButton, 0, wxEXPAND|wxALL, 5);
				UndoButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("SelectAll"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(UndoButton, 0, wxEXPAND|wxALL, 5);
				CopyToClipboardButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("CopyToClipboard"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(CopyToClipboardButton, 0, wxEXPAND|wxALL, 5);
			}
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_TOP|wxALL, 5);

	}
	SetSizer(MainSizer);

	// Setup the button events
	// Use the event handler and the Id of the button of interest
	wxEvtHandler* EvtHandler = GetEventHandler();
	EvtHandler->Connect(CloseButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnClose));
	EvtHandler->Connect(RescanAllLevelsButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnRescan));
	EvtHandler->Connect(RescanSelectedLevelsButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnRescan));
	EvtHandler->Connect(RescanCurrentLevelButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnRescan));
	EvtHandler->Connect(GotoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnGoTo));
	EvtHandler->Connect(SyncButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSync));
	EvtHandler->Connect(SwapButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSwap));
	EvtHandler->Connect(SwapExButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSwapEx));
	EvtHandler->Connect(SetToVertexButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSetToVertex));
	EvtHandler->Connect(SetToTextureButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSetToTexture));
	EvtHandler->Connect(SetToTextureExButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSetToTextureEx));
	EvtHandler->Connect(StaticMeshList->GetId(), wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler(WxDlgStaticMeshLightingInfo::OnItemActivated));
	EvtHandler->Connect(StaticMeshList->GetId(), wxEVT_COMMAND_LIST_COL_CLICK, wxListEventHandler(WxDlgStaticMeshLightingInfo::OnListColumnClick));
	EvtHandler->Connect(SelectAllButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnSelectAll));
	EvtHandler->Connect(UndoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnUndo));
	EvtHandler->Connect(CopyToClipboardButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgStaticMeshLightingInfo::OnLog));

 	// Set an accelerator table to handle hotkey presses for the window.
 	wxAcceleratorEntry Entries[3];
	Entries[0].Set(wxACCEL_CTRL,	67,			CopyToClipboardButton->GetId());	// Ctrl-C
	Entries[1].Set(wxACCEL_CTRL,	65,			SelectAllButton->GetId());			// Ctrl-A
	Entries[2].Set(wxACCEL_CTRL,	90,			UndoButton->GetId());				// Ctrl-Z
 	wxAcceleratorTable AcceleratorTable(3, Entries);
 	SetAcceleratorTable(AcceleratorTable);

	SelectAllButton->Hide();
	UndoButton->Hide();
	CopyToClipboardButton->Hide();

	// Fill in the columns
	SetupListColumns();

	// Load window position.
	FWindowUtil::LoadPosSize(TEXT("DlgStaticMeshLightingInfo2"), this, -1, -1, 1400, 500);

	StaticMeshInfoList = &GStaticMeshLightingInfoList;
}

WxDlgStaticMeshLightingInfo::~WxDlgStaticMeshLightingInfo()
{
	// Save window position.
	FWindowUtil::SavePosSize(TEXT("DlgStaticMeshLightingInfo2"), this);
}

/** Called when the window has been selected from within the ctrl + tab dialog. */
void WxDlgStaticMeshLightingInfo::OnSelected()
{
	if(!IsShown())
	{
		Show();
	}

	WxTrackableDialog::OnSelected();
}

/**
 *	Show the dialog.
 *
 *	@param	show	If TRUE show the dialog, FALSE hide it.
 *
 *	@return	bool	
 */
bool WxDlgStaticMeshLightingInfo::Show( bool show )
{
	if( show )
	{
		LoadListCtrl();
	}
	else
	{
		ClearMessageList();
	}

	return wxDialog::Show(show);
}

/** Shows the dialog only if there are messages to display. */
void WxDlgStaticMeshLightingInfo::ShowConditionally()
{
	LoadListCtrl();

	if (StaticMeshList->GetItemCount() > 0)
	{
		Show( true );
	}
}

/** Clears out the list of messages appearing in the window. */
void WxDlgStaticMeshLightingInfo::ClearMessageList()
{
	StaticMeshList->DeleteAllItems();
	StaticMeshInfoList->Empty();
	ReferencedObjects.Empty();
}

/** Freezes the message list. */
void WxDlgStaticMeshLightingInfo::FreezeMessageList()
{
	StaticMeshList->Freeze();
}

/** Thaws the message list. */
void WxDlgStaticMeshLightingInfo::ThawMessageList()
{
	StaticMeshList->Thaw();
}

/** 
 *	Adds a message to the map check dialog, to be displayed when the dialog is shown.
 *
 *	@param	InActor							StaticMeshActor associated with the message
 *	@param	InStaticMeshComponent			StaticMeshComponent associated with the message
 *	@param	InStaticMesh					The source StaticMesh that is related to this info.
 *	@param	bInTextureMapping				If TRUE, the object is currently using Texture mapping.
 *	@param	InStaticLightingResolution		The static lighting resolution used to estimate texutre mapping.
 *	@param	InLightMapLightCount			The number of lights generating light maps on the primitive.
 *	@param	InTextureLightMapMemoryUsage	Estimated memory usage in bytes for light map texel data.
 *	@param	InVertexLightMapMemoryUsage		Estimated memory usage in bytes for light map vertex data.
 *	@param	InShadowMapLightCount			The number of lights generating shadow maps on the primtive.
 *	@param	InTextureShadowMapMemoryUsage	Estimated memory usage in bytes for shadow map texel data.
 *	@param	InVertexShadowMapMemoryUsage	Estimated memory usage in bytes for shadow map vertex data.
 *	@param	bInHasLightmapTexCoords			If TRUE if the mesh has the proper UV channels.
 */
void WxDlgStaticMeshLightingInfo::AddItem(AActor* InActor, UStaticMeshComponent* InStaticMeshComponent, 
	UStaticMesh* InStaticMesh, UBOOL bInTextureMapping, INT InStaticLightingResolution, 
	INT InLightMapLightCount, INT InTextureLightMapMemoryUsage, INT InVertexLightMapMemoryUsage, 
	INT InShadowMapLightCount, INT InTextureShadowMapMemoryUsage, INT InVertexShadowMapMemoryUsage, 
	UBOOL bInHasLightmapTexCoords)
{
	// Columns are, from left to right: 
	//	Level, actor, static mesh, mapping type, Has Lightmap UVs, static lighting resolution, texture light map, vertex light map, texture shadow map, vertex shadow map
	FStaticMeshLightingInfo Info;

	Info.StaticMeshActor = InActor;
	Info.StaticMeshComponent = InStaticMeshComponent;
	Info.StaticMesh = InStaticMesh;
	Info.bTextureMapping = bInTextureMapping;
	Info.StaticLightingResolution = InStaticLightingResolution;
	Info.LightMapLightCount = InLightMapLightCount;
	Info.TextureLightMapMemoryUsage = InTextureLightMapMemoryUsage;
	Info.VertexLightMapMemoryUsage = InVertexLightMapMemoryUsage;
	Info.ShadowMapLightCount = InShadowMapLightCount;
	Info.TextureShadowMapMemoryUsage = InTextureShadowMapMemoryUsage;
	Info.VertexShadowMapMemoryUsage = InVertexShadowMapMemoryUsage;
	Info.bHasLightmapTexCoords = bInHasLightmapTexCoords;

	WxDlgStaticMeshLightingInfo* StaticMeshLightingInfoWindow = GApp->GetDlgStaticMeshLightingInfo();
	check(StaticMeshLightingInfoWindow);
	if (Info.StaticMeshActor)
	{
		StaticMeshLightingInfoWindow->ReferencedObjects.AddUniqueItem(Info.StaticMeshActor);
	}
	if (Info.StaticMeshComponent)
	{
		StaticMeshLightingInfoWindow->ReferencedObjects.AddUniqueItem(Info.StaticMeshComponent);
	}
	if (Info.StaticMesh)
	{
		StaticMeshLightingInfoWindow->ReferencedObjects.AddUniqueItem(Info.StaticMesh);
	}
	GStaticMeshLightingInfoList.AddUniqueItem(Info);
}

/** 
 *	Adds the given static mesh lighting info to the list.
 *
 *	@param	InSMLightingInfo				The lighting info to add
 */
void WxDlgStaticMeshLightingInfo::AddItem(FStaticMeshLightingInfo& InSMLightingInfo)
{
	GStaticMeshLightingInfoList.AddUniqueItem(InSMLightingInfo);
	if (InSMLightingInfo.StaticMeshActor)
	{
		ReferencedObjects.AddUniqueItem(InSMLightingInfo.StaticMeshActor);
	}
	if (InSMLightingInfo.StaticMeshComponent)
	{
		ReferencedObjects.AddUniqueItem(InSMLightingInfo.StaticMeshComponent);
	}
	if (InSMLightingInfo.StaticMesh)
	{
		ReferencedObjects.AddUniqueItem(InSMLightingInfo.StaticMesh);
	}
}

/** 
 *	Scans the level(s) for static meshes and fills in the information.
 *
 *	@param	InLevelOptions					What level(s) to scan.
 */
void WxDlgStaticMeshLightingInfo::ScanStaticMeshLightingInfo(ELevelOptions InLevelOptions)
{
	WxDlgStaticMeshLightingInfo* StaticMeshLightingInfoWindow = GApp->GetDlgStaticMeshLightingInfo();
	check(StaticMeshLightingInfoWindow);

	GStaticMeshLightingInfo_ScanMethod = InLevelOptions;

	/** The levels we are gathering information for. */
	TArray<ULevel*> Levels;

	// Clear the AllLights array!
	StaticMeshLightingInfoWindow->AllLights.Empty();

	// Fill the light list
	for (TObjectIterator<ULightComponent> LightIt; LightIt; ++LightIt)
	{
		ULightComponent* const Light = *LightIt;
		const UBOOL bLightIsInWorld = Light->GetOwner() && GWorld->ContainsActor(Light->GetOwner());
		if (bLightIsInWorld && (Light->HasStaticShadowing() || Light->HasStaticLighting()))
		{
			// Add the light to the system's list of lights in the world.
			WxDlgStaticMeshLightingInfo* StaticMeshLightingInfo = GApp->GetDlgStaticMeshLightingInfo();
			check(StaticMeshLightingInfo);
			StaticMeshLightingInfo->AllLights.AddItem(Light);
		}
	}

	switch (InLevelOptions)
	{
	case DLGSMLI_CurrentLevel:
		{
			check(GWorld);
			Levels.AddItem(GWorld->CurrentLevel);
		}
		break;
	case DLGSMLI_SelectedLevels:
		{
			WxLevelBrowser* LevelBrowser = GUnrealEd->GetBrowser<WxLevelBrowser>(TEXT("LevelBrowser"));
			if (LevelBrowser != NULL)
			{
				// Assemble an ignore list from the levels that are currently selected in the level browser.
				for (WxLevelBrowser::TSelectedLevelItemIterator It(LevelBrowser->SelectedLevelItemIterator()); It; ++It)
				{
					if( It->IsLevel() )
					{
						ULevel* Level = It->GetLevel();
						if (Level)
						{
							Levels.AddUniqueItem(Level);
						}
					}
				}

				if (Levels.Num() == 0)
				{
					// Fall to the current level...
					check(GWorld);
					Levels.AddUniqueItem(GWorld->CurrentLevel);
				}
			}
		}
		break;
	case DLGSMLI_AllLevels:
		{
			if (GWorld != NULL)
			{
				// Add main level.
				Levels.AddUniqueItem(GWorld->PersistentLevel);

				// Add secondary levels.
				AWorldInfo*	WorldInfo = GWorld->GetWorldInfo();
				for (INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex)
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
					if (StreamingLevel && StreamingLevel->LoadedLevel)
					{
						Levels.AddUniqueItem(StreamingLevel->LoadedLevel);
					}
				}
			}
		}
		break;
	}

	if (Levels.Num() > 0)
	{
		// Iterate over static mesh components in the list of levels...
		for (TObjectIterator<UStaticMeshComponent> SMCIt; SMCIt; ++SMCIt)
		{
			UStaticMeshComponent* SMComponent = *SMCIt;
			AActor* Owner = NULL;

			// Check all actors and see if it has the component in its AllComponents array.
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				for (INT CompIdx = 0; CompIdx < ActorIt->AllComponents.Num(); CompIdx++)
				{
					UPrimitiveComponent* CheckPrim = Cast<UPrimitiveComponent>(ActorIt->AllComponents(CompIdx));
					if (CheckPrim == SMComponent)
					{
						Owner = *ActorIt;
						break;
					}
				}
			}

			if (Owner)
			{
				INT Dummy;
				ULevel* CheckLevel = Owner->GetLevel();
				if ((CheckLevel != NULL) && (Levels.FindItem(CheckLevel, Dummy) == TRUE))
				{
					FStaticMeshLightingInfo TempSMLInfo;
					WxDlgStaticMeshLightingInfo* StaticMeshLightingInfo = GApp->GetDlgStaticMeshLightingInfo();
					check(StaticMeshLightingInfo);

					if (StaticMeshLightingInfo->FillStaticMeshLightingInfo(SMComponent, Owner, TempSMLInfo) == TRUE)
					{
						StaticMeshLightingInfo->AddItem(TempSMLInfo);
					}
				}
			}
		}
	}
	else
	{
		appMsgf(AMT_OK, *LocalizeUnrealEd("Error_NoLevelsForRescan"));
	}
}

/**
 *	Serialize the referenced assets to prevent GC.
 *
 *	@param	Ar		The archive to serialize to.
 */
void WxDlgStaticMeshLightingInfo::Serialize(FArchive& Ar)
{
	Ar << ReferencedObjects;
}

/** Loads the list control with the contents of the GErrorWarningInfoList array. */
void WxDlgStaticMeshLightingInfo::LoadListCtrl()
{
	StaticMeshList->DeleteAllItems();
	UpdateTotalsEntry();
	for (int Idx = 0; Idx < StaticMeshInfoList->Num() ; Idx++)
	{
		FillListEntry(Idx);
	}
}

/** Event handler for when the close button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnClose(wxCommandEvent& In)
{
	Show(0);
}

/** Event handler for when the rescan button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnRescan(wxCommandEvent& In)
{
	ClearMessageList();

	if (In.GetId() == RescanAllLevelsButton->GetId())
	{
		LastLevelScan = DLGSMLI_AllLevels;
	}
	else
	if (In.GetId() == RescanSelectedLevelsButton->GetId())
	{
		LastLevelScan = DLGSMLI_SelectedLevels;
	}
	else
	//if (In.GetId() == RescanCurrentLevelButton->GetId())
	{
		LastLevelScan = DLGSMLI_CurrentLevel;
	}
	WxDlgStaticMeshLightingInfo::ScanStaticMeshLightingInfo(LastLevelScan);

	SortList(GStaticMeshLightingInfo_SortMethod);
	LoadListCtrl();
}

/** Event handler for when the goto button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnGoTo(wxCommandEvent& In)
{
	const INT NumSelected = StaticMeshList->GetSelectedItemCount();
	if (NumSelected > 0)
	{
		const FScopedBusyCursor BusyCursor;
		TArray<AActor*> SelectedActors;

		// Get the list of StaticMeshActors for all selected entries in the list
		long ItemIndex = StaticMeshList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		while (ItemIndex != -1)
		{
			FStaticMeshLightingInfo* smli = (FStaticMeshLightingInfo*)(StaticMeshList->GetItemData(ItemIndex));
			if (smli && smli->StaticMeshActor)
			{
				SelectedActors.AddItem(smli->StaticMeshActor);
			}
			ItemIndex = StaticMeshList->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		}

		// Select them in the level(s)
		if (SelectedActors.Num() > 0)
		{
			const FScopedTransaction Transaction(*LocalizeUnrealEd("StaticMeshLightingInfoGoto"));
			GEditor->SelectNone(FALSE, TRUE);
			for (INT ActorIndex = 0; ActorIndex < SelectedActors.Num(); ++ActorIndex)
			{
				GEditor->SelectActor(SelectedActors(ActorIndex), TRUE, NULL, FALSE, TRUE);
			}
			GEditor->NoteSelectionChange();
			GEditor->MoveViewportCamerasToActor(SelectedActors, FALSE);
		}
	}
}

/** Event handler for when the sync button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnSync(wxCommandEvent& In)
{
	const INT NumSelected = StaticMeshList->GetSelectedItemCount();
	if (NumSelected > 0)
	{
		const FScopedBusyCursor BusyCursor;
		TArray<UObject*> SelectedObjects;
		long ItemIndex = StaticMeshList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		while (ItemIndex != -1)
		{
			FStaticMeshLightingInfo* smli = (FStaticMeshLightingInfo*)(StaticMeshList->GetItemData(ItemIndex));
			if (smli && smli->StaticMesh)
			{
				SelectedObjects.AddItem(smli->StaticMesh);
			}
			ItemIndex = StaticMeshList->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		}

		if (SelectedObjects.Num() > 0)
		{
			GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
		}
	}
}

/** Event handler for when the swap button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnSwap(wxCommandEvent& In)
{
	SwapMappingMethodOnSelectedComponents(0);
}

/** Event handler for when the swap ex button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnSwapEx(wxCommandEvent& In)
{
	INT NewResolution = GetUserSetStaticLightmapResolution();
	if (NewResolution != -1)
	{
		SwapMappingMethodOnSelectedComponents(NewResolution);
	}
}

/** Event handler for when the SetToVertex button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnSetToVertex(wxCommandEvent& In)
{
	SetMappingMethodOnSelectedComponents(FALSE, 0);
}

/** Event handler for when the SetToTexture button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnSetToTexture(wxCommandEvent& In)
{
	SetMappingMethodOnSelectedComponents(TRUE, 0);
}

/** Event handler for when the SetToTexture button is clicked on. */
void WxDlgStaticMeshLightingInfo::OnSetToTextureEx(wxCommandEvent& In)
{
	INT NewResolution = GetUserSetStaticLightmapResolution();
	if (NewResolution != -1)
	{
		SetMappingMethodOnSelectedComponents(TRUE, NewResolution);
	}
}
	
/** Event handler for when a message is clicked on. */
void WxDlgStaticMeshLightingInfo::OnItemActivated(wxListEvent& In)
{
	const long ItemIndex = In.GetIndex();
	FStaticMeshLightingInfo* smli = (FStaticMeshLightingInfo*)(StaticMeshList->GetItemData(ItemIndex));
	if (smli)
	{
		if (smli->StaticMeshActor)
		{
			const FScopedTransaction Transaction(*LocalizeUnrealEd("StaticMeshLightingInfoGoto"));
			GEditor->SelectNone(TRUE, TRUE);
			GEditor->SelectActor(smli->StaticMeshActor, TRUE, NULL, TRUE, TRUE);
			GEditor->MoveViewportCamerasToActor(*(smli->StaticMeshActor), FALSE);
		}
	}
}

/** Event handler for when a column header is clicked on. */
void WxDlgStaticMeshLightingInfo::OnListColumnClick(wxListEvent& In)
{
	// Sort by the selected column
	SortList((ESortMethod)(In.m_col));
	LoadListCtrl();
}

/** Event handler for SelectAll button. */
void WxDlgStaticMeshLightingInfo::OnSelectAll(wxCommandEvent& In)
{
	for (INT SelIdx = 0; SelIdx < StaticMeshInfoList->Num() + 1; SelIdx++)
	{
		StaticMeshList->SetItemState(SelIdx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
}

/** Event handler for undo. */
void WxDlgStaticMeshLightingInfo::OnUndo(wxCommandEvent& In)
{
	GEditor->UndoTransaction();
	ClearMessageList();
	ScanStaticMeshLightingInfo(GStaticMeshLightingInfo_ScanMethod);
	SortList(GStaticMeshLightingInfo_SortMethod);
	LoadListCtrl();
}

/** Event handler for logging selected entries. */
void WxDlgStaticMeshLightingInfo::OnLog(wxCommandEvent& In)
{
	FString CopyString;

	CopyString = FString::Printf(
		TEXT("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s"), 
		*LocalizeUnrealEd("Level"),
		*LocalizeUnrealEd("Actor"),
		*LocalizeUnrealEd("StaticMesh"),
		*LocalizeUnrealEd("MappingType"),
		*LocalizeUnrealEd("HasLightmapUVs"),
		*LocalizeUnrealEd("SMLIResolution"),
		*LocalizeUnrealEd("TexutreLightMap_Bytes"),
		*LocalizeUnrealEd("VertexLightMap_Bytes"),
		*LocalizeUnrealEd("NumLightMapLights"),
		*LocalizeUnrealEd("TextureShadowMap_Bytes"),
		*LocalizeUnrealEd("VertexShadowMap_Bytes"),
		*LocalizeUnrealEd("NumShadowMapLights"));
	debugf(*CopyString);
	CopyString = TEXT("");

	long ItemIndex = StaticMeshList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	while (ItemIndex != -1)
	{
		FString TempString(TEXT(""));
		wxListItem TempItem;
		TempItem.SetId(ItemIndex);
		TempItem.SetMask(wxLIST_MASK_TEXT);
		for (INT ColIdx = SORTBY_Level; ColIdx <= SORTBY_NumShadowMapLights; ColIdx++)
		{
			TempItem.SetColumn(ColIdx);
			StaticMeshList->GetItem(TempItem);
			TempString += TempItem.GetText();
			if (ColIdx < SORTBY_NumShadowMapLights)
			{
				TempString += TEXT(",");
			}
		}
		CopyString += TEXT("\n");
		CopyString += TempString;
		debugf(*TempString);
		ItemIndex = StaticMeshList->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}
	appClipboardCopy(*CopyString);
}

/** Event handler for when wx wants to update UI elements. */
void WxDlgStaticMeshLightingInfo::OnUpdateUI(wxUpdateUIEvent& In)
{
}

/** Set up the columns for the ErrorWarningList control. */
void WxDlgStaticMeshLightingInfo::SetupListColumns()
{
	if (StaticMeshList)
	{
		// Columns are, from left to right: 
		//	Level, actor, static mesh, mapping type, Has Lightmap UVs, static lighting resolution, texture light map, vertex light map, texture shadow map, vertex shadow map

		// THIS MUST BE THE REVERSE ORDER OF THE ESortMethod ENUMERATION IN THE CLASS!
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("NumShadowMapLights"),		wxLIST_FORMAT_LEFT, 125);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("VertexShadowMap_Bytes"),		wxLIST_FORMAT_LEFT, 150);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("TextureShadowMap_Bytes"),	wxLIST_FORMAT_LEFT, 150);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("NumLightMapLights"),			wxLIST_FORMAT_LEFT, 125);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("VertexLightMap_Bytes"),		wxLIST_FORMAT_LEFT, 125);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("TexutreLightMap_Bytes"),		wxLIST_FORMAT_LEFT, 125);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("SMLIResolution"),			wxLIST_FORMAT_LEFT,  75);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("HasLightmapUVs"),			wxLIST_FORMAT_LEFT, 100);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("MappingType"),				wxLIST_FORMAT_LEFT,  90);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("StaticMesh"),				wxLIST_FORMAT_LEFT, 135);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("Actor"),						wxLIST_FORMAT_LEFT, 125);
		StaticMeshList->InsertColumn(0, *LocalizeUnrealEd("Level"),						wxLIST_FORMAT_LEFT, 150);
	}
}

/** 
 *	Get the level/package name for the given object.
 *
 *	@param	InObject	The object to retrieve the level/package name for.
 *
 *	@return	FString		The name of the level/package.
 */
FString WxDlgStaticMeshLightingInfo::GetLevelOrPackageName(UObject* InObject)
{
	AActor* Actor = Cast<AActor>(InObject);
	if (Actor)
	{
		return Actor->GetLevel()->GetOutermost()->GetName();
	}

	return InObject->GetOutermost()->GetName();
}

/** 
 *	Sets the lighting information for the given StaticMeshComponent up.
 *
 *	@param	InSMComponent		The static mesh component to setup the lighting info for.
 *	@param	InSMActor			The static mesh actor that 'owns' the component.
 *	@param	OutSMLightingINfo	The StaticMeshLightingInfo to fill in.
 *	
 *	@return	UBOOL				TRUE if it was successful; FALSE if not
 */
UBOOL WxDlgStaticMeshLightingInfo::FillStaticMeshLightingInfo(UStaticMeshComponent* InSMComponent, 
	AActor* InSMActor, FStaticMeshLightingInfo& OutSMLightingInfo)
{
	check(InSMComponent);

	// 'Initialize it'
	OutSMLightingInfo.StaticMeshActor = InSMActor;
	OutSMLightingInfo.StaticMeshComponent = InSMComponent;
	OutSMLightingInfo.StaticMesh = InSMComponent->StaticMesh;
	OutSMLightingInfo.bTextureMapping = FALSE;
	OutSMLightingInfo.StaticLightingResolution = 0;
	OutSMLightingInfo.LightMapLightCount = 0;
	OutSMLightingInfo.TextureLightMapMemoryUsage = 0;
	OutSMLightingInfo.VertexLightMapMemoryUsage = 0;
	OutSMLightingInfo.ShadowMapLightCount = 0;
	OutSMLightingInfo.TextureShadowMapMemoryUsage = 0;
	OutSMLightingInfo.VertexShadowMapMemoryUsage = 0;
	OutSMLightingInfo.bHasLightmapTexCoords = FALSE;

	if (InSMComponent->GetEstimatedLightAndShadowMapMemoryUsage(
		OutSMLightingInfo.TextureLightMapMemoryUsage, OutSMLightingInfo.TextureShadowMapMemoryUsage,
		OutSMLightingInfo.VertexLightMapMemoryUsage, OutSMLightingInfo.VertexShadowMapMemoryUsage,
		OutSMLightingInfo.StaticLightingResolution, OutSMLightingInfo.bTextureMapping, 
		OutSMLightingInfo.bHasLightmapTexCoords) == TRUE)
	{
		// Find the lights relevant to the primitive.
		TArray<ULightComponent*> LightMapRelevantLights;
		TArray<ULightComponent*> ShadowMapRelevantLights;
		for (INT LightIndex = 0; LightIndex < AllLights.Num(); LightIndex++)
		{
			ULightComponent* Light = AllLights(LightIndex);
			// Only add enabled lights or lights that can potentially be enabled at runtime (toggleable)
			if ((Light->bEnabled || !Light->UseDirectLightMap) && Light->AffectsPrimitive(InSMComponent, TRUE))
			{
				// Check whether the light should use a light-map or shadow-map.
				const UBOOL bUseStaticLighting = Light->UseStaticLighting(InSMComponent->bForceDirectLightMap);
				if (bUseStaticLighting)
				{
					LightMapRelevantLights.AddItem(Light);
				}
				// only allow for shadow maps if shadow casting is enabled
				else if (Light->CastShadows && Light->CastStaticShadows)
				{
					ShadowMapRelevantLights.AddItem(Light);
				}
			}
		}

		OutSMLightingInfo.LightMapLightCount = LightMapRelevantLights.Num();
		OutSMLightingInfo.ShadowMapLightCount = ShadowMapRelevantLights.Num();

		return TRUE;
	}

	return FALSE;
}

/** Sets the given entry in the list. */
void WxDlgStaticMeshLightingInfo::FillListEntry(INT InIndex)
{
	FStaticMeshLightingInfo* smli = &((*StaticMeshInfoList)(InIndex));

	//	Level, actor, static mesh, mapping type, Has Lightmap UVs, static lighting resolution, texture light map, vertex light map, texture shadow map, vertex shadow map
	FString LevelName(TEXT("<None>"));
	FString ActorName(TEXT("<None>"));
	FString StaticMeshName(TEXT("<None>"));
	FString CurrentMapping;
	FString HasLightmapUVs;
	FString StaticLightingResolution;
	FString TextureLightMap;
	FString VertexLightMap;
	FString NumLightMapLights;
	FString TextureShadowMap;
	FString VertexShadowMap;
	FString NumShadowMapLights;

	if (smli->StaticMeshComponent && smli->StaticMesh && smli->StaticMeshActor)
	{
		LevelName = GetLevelOrPackageName(smli->StaticMeshActor);
		ActorName = smli->StaticMeshActor->GetName();
		StaticMeshName = smli->StaticMesh->GetPathName();
	}
	
	CurrentMapping = (smli->bTextureMapping == TRUE) ? TEXT("Texture") : TEXT("Vertex");
	HasLightmapUVs = (smli->bHasLightmapTexCoords == TRUE) ? TEXT("TRUE") : TEXT("FALSE");
	StaticLightingResolution = FString::Printf(TEXT("%d"), smli->StaticLightingResolution);
	TextureLightMap = FString::Printf(TEXT("%d"), 
		(smli->LightMapLightCount > 0) ? smli->TextureLightMapMemoryUsage : 0);
	VertexLightMap = FString::Printf(TEXT("%d"), 
		(smli->LightMapLightCount > 0) ? smli->VertexLightMapMemoryUsage : 0);
	NumLightMapLights = FString::Printf(TEXT("%d"), smli->LightMapLightCount);
	TextureShadowMap = FString::Printf(TEXT("%d"), smli->TextureShadowMapMemoryUsage * smli->ShadowMapLightCount);
	VertexShadowMap = FString::Printf(TEXT("%d"), smli->VertexShadowMapMemoryUsage * smli->ShadowMapLightCount);
	NumShadowMapLights = FString::Printf(TEXT("%d"), smli->ShadowMapLightCount);

	// Leave the '0' entry for the totals...
	INT InsertIndex = InIndex + 1;
	INT CurrentCount = StaticMeshList->GetItemCount();
	if (CurrentCount <= InsertIndex)
	{
		INT AddCount = InsertIndex - CurrentCount + 1;
		for (INT AddIdx = 0; AddIdx < AddCount; AddIdx++)
		{
			StaticMeshList->InsertItem(AddIdx + CurrentCount, *LevelName);
		}
	}
	StaticMeshList->SetItem(InsertIndex, SORTBY_Level,						*LevelName);
	StaticMeshList->SetItem(InsertIndex, SORTBY_Actor,						*ActorName);
	StaticMeshList->SetItem(InsertIndex, SORTBY_StaticMesh,					*StaticMeshName);
	StaticMeshList->SetItem(InsertIndex, SORTBY_MappingType,				*CurrentMapping);
	StaticMeshList->SetItem(InsertIndex, SORTBY_HasLightmapUVs,				*HasLightmapUVs);
	StaticMeshList->SetItem(InsertIndex, SORTBY_StaticLightingResolution,	*StaticLightingResolution);
	StaticMeshList->SetItem(InsertIndex, SORTBY_TextureLightMap,			*TextureLightMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_VertexLightMap,				*VertexLightMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_NumLightMapLights,			*NumLightMapLights);
	StaticMeshList->SetItem(InsertIndex, SORTBY_TextureShadowMap,			*TextureShadowMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_VertexShadowMap,			*VertexShadowMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_NumShadowMapLights,			*NumShadowMapLights);
	StaticMeshList->SetItemPtrData(InsertIndex, (PTRINT)(smli));
}

/** Updates the 'totals' entry of the list. */
void WxDlgStaticMeshLightingInfo::UpdateTotalsEntry()
{
	//	Level, actor, static mesh, mapping type, Has Lightmap UVs, static lighting resolution, texture light map, vertex light map, texture shadow map, vertex shadow map
	FString LevelName(TEXT(""));
	FString ActorName(TEXT(""));
	FString StaticMeshName(TEXT(""));
	FString CurrentMapping(TEXT(""));
	FString HasLightmapUVs(TEXT(""));
	FString StaticLightingResolution(TEXT(""));
	FString TextureLightMap(TEXT(""));
	FString VertexLightMap(TEXT(""));
	FString NumLightMapLights(TEXT(""));
	FString TextureShadowMap(TEXT(""));
	FString VertexShadowMap(TEXT(""));
	FString NumShadowMapLights(TEXT(""));

	INT TextureLightMap_Total = 0;
	INT VertexLightMap_Total = 0;
	INT TextureShadowMap_Total = 0;
	INT VertexShadowMap_Total = 0;

	for (INT SMLIIdx = 0; SMLIIdx < StaticMeshInfoList->Num(); SMLIIdx++)
	{
		FStaticMeshLightingInfo* smli = &((*StaticMeshInfoList)(SMLIIdx));
		if (smli)
		{
			if (smli->bTextureMapping == TRUE)
			{
				TextureLightMap_Total += (smli->LightMapLightCount > 0) ? smli->TextureLightMapMemoryUsage : 0;
				TextureShadowMap_Total += smli->TextureShadowMapMemoryUsage * smli->ShadowMapLightCount;
			}
			else
			{
				VertexLightMap_Total += (smli->LightMapLightCount > 0) ? smli->VertexLightMapMemoryUsage : 0;
				VertexShadowMap_Total += smli->VertexShadowMapMemoryUsage * smli->ShadowMapLightCount;
			}
		}
	}

	TextureLightMap = FString::Printf(TEXT("%d"), TextureLightMap_Total);
	VertexLightMap = FString::Printf(TEXT("%d"), VertexLightMap_Total);
	TextureShadowMap = FString::Printf(TEXT("%d"), TextureShadowMap_Total);
	VertexShadowMap = FString::Printf(TEXT("%d"), VertexShadowMap_Total);

	// Leave the '0' entry for the totals...
	INT InsertIndex = 0;
	INT CurrentCount = StaticMeshList->GetItemCount();
	if (CurrentCount <= InsertIndex)
	{
		INT AddCount = InsertIndex - CurrentCount + 1;
		for (INT AddIdx = 0; AddIdx < AddCount; AddIdx++)
		{
			StaticMeshList->InsertItem(AddIdx + CurrentCount, *LevelName);
		}
	}
	StaticMeshList->SetItem(InsertIndex, SORTBY_Level,						*LevelName);
	StaticMeshList->SetItem(InsertIndex, SORTBY_Actor,						*ActorName);
	StaticMeshList->SetItem(InsertIndex, SORTBY_StaticMesh,					*StaticMeshName);
	StaticMeshList->SetItem(InsertIndex, SORTBY_MappingType,				*CurrentMapping);
	StaticMeshList->SetItem(InsertIndex, SORTBY_HasLightmapUVs,				*HasLightmapUVs);
	StaticMeshList->SetItem(InsertIndex, SORTBY_StaticLightingResolution,	*StaticLightingResolution);
	StaticMeshList->SetItem(InsertIndex, SORTBY_TextureLightMap,			*TextureLightMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_VertexLightMap,				*VertexLightMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_NumLightMapLights,			*NumLightMapLights);
	StaticMeshList->SetItem(InsertIndex, SORTBY_TextureShadowMap,			*TextureShadowMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_VertexShadowMap,			*VertexShadowMap);
	StaticMeshList->SetItem(InsertIndex, SORTBY_NumShadowMapLights,			*NumShadowMapLights);
	StaticMeshList->SetItemPtrData(InsertIndex, NULL);
}

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF( \
	FStaticMeshLightingInfo, \
	DlgLightingResults, \
	{	\
		FString ActorName_A(TEXT("<None>"));	\
		FString ActorName_B(TEXT("<None>"));	\
		if (A.StaticMeshActor)	ActorName_A = A.StaticMeshActor->GetName();	\
		if (B.StaticMeshActor)	ActorName_B = B.StaticMeshActor->GetName();	\
		switch (GStaticMeshLightingInfo_SortMethod)	\
		{	\
		case WxDlgStaticMeshLightingInfo::SORTBY_Level:							\
			{	\
				FString LevelName_A(TEXT("<None>"));	\
				FString LevelName_B(TEXT("<None>"));	\
				if (A.StaticMeshActor)	LevelName_A = A.StaticMeshActor->GetLevel()->GetName();	\
				if (B.StaticMeshActor)	LevelName_B = B.StaticMeshActor->GetLevel()->GetName();	\
				if (LevelName_A != LevelName_B)	\
				{	\
					return appStrcmp(*LevelName_A, *LevelName_B);	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_Actor:							\
			{	\
				return appStrcmp(*ActorName_A, *ActorName_B);	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_StaticMesh:					\
			{	\
				FString StaticMeshName_A(TEXT("<None>"));	\
				FString StaticMeshName_B(TEXT("<None>"));	\
				if (A.StaticMesh)	StaticMeshName_A = A.StaticMesh->GetPathName();	\
				if (B.StaticMesh)	StaticMeshName_B = B.StaticMesh->GetPathName();	\
				if (StaticMeshName_A != StaticMeshName_B)	\
				{	\
					return appStrcmp(*StaticMeshName_A, *StaticMeshName_B);	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_MappingType:					\
			{	\
				FString CurrentMapping_A = (A.bTextureMapping == TRUE) ? TEXT("Texture") : TEXT("Vertex");	\
				FString CurrentMapping_B = (B.bTextureMapping == TRUE) ? TEXT("Texture") : TEXT("Vertex");	\
				if (CurrentMapping_A != CurrentMapping_B)	\
				{	\
					return appStrcmp(*CurrentMapping_A, *CurrentMapping_B);	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_HasLightmapUVs:				\
			{	\
				FString HasLightmapUVs_A = (A.bHasLightmapTexCoords == TRUE) ? TEXT("TRUE") : TEXT("FALSE");	\
				FString HasLightmapUVs_B = (B.bHasLightmapTexCoords == TRUE) ? TEXT("TRUE") : TEXT("FALSE");	\
				if (HasLightmapUVs_A != HasLightmapUVs_B)	\
				{	\
					return appStrcmp(*HasLightmapUVs_A, *HasLightmapUVs_B);	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_StaticLightingResolution:		\
			{	\
				if (A.StaticLightingResolution != B.StaticLightingResolution)	\
				{	\
					return A.StaticLightingResolution < B.StaticLightingResolution ? 1 : -1;		\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_TextureLightMap:				\
			{	\
				INT Value_A = (A.LightMapLightCount > 0) ? A.TextureLightMapMemoryUsage : 0;	\
				INT Value_B = (B.LightMapLightCount > 0) ? B.TextureLightMapMemoryUsage : 0;	\
				if (Value_A != Value_B)	\
				{	\
					return Value_A < Value_B ? 1 : -1;	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_VertexLightMap:				\
			{	\
				INT Value_A = (A.LightMapLightCount > 0) ? A.VertexLightMapMemoryUsage : 0;	\
				INT Value_B = (B.LightMapLightCount > 0) ? B.VertexLightMapMemoryUsage : 0;	\
				if (Value_A != Value_B)	\
				{	\
					return Value_A < Value_B ? 1 : -1;	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_NumLightMapLights:				\
			{	\
				if (A.LightMapLightCount != B.LightMapLightCount)	\
				{	\
					return A.LightMapLightCount < B.LightMapLightCount ? 1 : -1;	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_TextureShadowMap:				\
			{	\
				INT Value_A = A.TextureShadowMapMemoryUsage * A.ShadowMapLightCount;	\
				INT Value_B = B.TextureShadowMapMemoryUsage * B.ShadowMapLightCount;	\
				if (Value_A != Value_B)	\
				{	\
					return Value_A < Value_B ? 1 : -1;	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_VertexShadowMap:				\
			{	\
				INT Value_A = A.VertexShadowMapMemoryUsage * A.ShadowMapLightCount;	\
				INT Value_B = B.VertexShadowMapMemoryUsage * B.ShadowMapLightCount;	\
				if (Value_A != Value_B)	\
				{	\
					return Value_A < Value_B ? 1 : -1;	\
				}	\
			}	\
		case WxDlgStaticMeshLightingInfo::SORTBY_NumShadowMapLights:			\
			{
				if (A.ShadowMapLightCount != B.ShadowMapLightCount)	\
				{	\
					return A.ShadowMapLightCount < B.ShadowMapLightCount ? 1 : -1;	\
				}	\
			}
		}	\
		return appStrcmp(*ActorName_A, *ActorName_B);	\
	}	\
	);

void WxDlgStaticMeshLightingInfo::SortList(ESortMethod InSortMethod)
{
	//@todo. Support 'double' selection of sorting column. 
	// Ie, if they click Texture map usage - the first time sort highest to lowest, the second (in a row) lowest to highest
	GStaticMeshLightingInfo_SortMethod = InSortMethod;
	Sort<USE_COMPARE_CONSTREF(FStaticMeshLightingInfo,DlgLightingResults)>(StaticMeshInfoList->GetData(),StaticMeshInfoList->Num());
}

/**
 *	Prompt the user for the static lightmap resolution
 *
 *	@return	INT		The desired resolution
 *					-1 indicates the user cancelled the dialog
 */
INT WxDlgStaticMeshLightingInfo::GetUserSetStaticLightmapResolution()
{
	INT DefaultRes = 0;
	verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), DefaultRes, GLightmassIni));

	WxDlgGenericStringEntry Dialog;
	INT Result = Dialog.ShowModal(
		TEXT("LSMI_GetResolutionTitle"), 
		TEXT(""),
		*FString::Printf(TEXT("%d"), DefaultRes));
	if (Result == wxID_OK)
	{
		INT NewResolution = appAtoi(*Dialog.GetEnteredString());
		if (NewResolution >= 0)
		{
			NewResolution = Max(NewResolution + 3 & ~3,4);
			return NewResolution;
		}
	}

	return -1;
}

/**
 *	Get the selected meshes, as well as an array of the selected indices...
 *
 *	@param	OutSelectedObjects		The array of selected objects to fill in.
 *	@param	OutSelectedIndices		The array of selected indices to fill in.
 */
void WxDlgStaticMeshLightingInfo::GetSelectedMeshes(TArray<FStaticMeshLightingInfo*>& OutSelectedObjects, TArray<long>& OutSelectedIndices)
{
	long ItemIndex = StaticMeshList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	while (ItemIndex != -1)
	{
		OutSelectedIndices.AddItem(ItemIndex);
		FStaticMeshLightingInfo* smli = (FStaticMeshLightingInfo*)(StaticMeshList->GetItemData(ItemIndex));
		if (smli && smli->StaticMeshComponent)
		{
			OutSelectedObjects.AddItem(smli);
		}
		ItemIndex = StaticMeshList->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}
}

/** 
 *	Swap the selected entries, using the given resolution.
 *
 *	@param	InStaticLightingResolution		== 0 to use the values already set.
 *											!= 0 to force all to the given value.
 */
void WxDlgStaticMeshLightingInfo::SwapMappingMethodOnSelectedComponents(INT InStaticLightingResolution)
{
 	const INT NumSelected = StaticMeshList->GetSelectedItemCount();
 	if (NumSelected > 0)
 	{
		const FScopedBusyCursor BusyCursor;
		TArray<long> SelectedIndices;
		TArray<FStaticMeshLightingInfo*> SelectedObjects;
		GetSelectedMeshes(SelectedObjects, SelectedIndices);
		if (SelectedObjects.Num() > 0)
		{
			const FScopedTransaction Transaction(*LocalizeUnrealEd("StaticMeshLightingInfoSwap"));

			UBOOL bReload = FALSE;
			for (INT CompIdx = 0; CompIdx < SelectedObjects.Num(); CompIdx++)
			{
				FStaticMeshLightingInfo* smli = SelectedObjects(CompIdx);

				if (smli->StaticMeshActor)
				{
					smli->StaticMeshActor->Modify();
				}
				smli->StaticMeshComponent->Modify();
				smli->StaticMeshComponent->SetStaticLightingMapping(!(smli->bTextureMapping), InStaticLightingResolution);
				smli->StaticMeshComponent->InvalidateLightingCache();
				if (smli->StaticMeshActor)
				{
					smli->StaticMeshActor->ReattachComponent(smli->StaticMeshComponent);
				}
				if (FillStaticMeshLightingInfo(smli->StaticMeshComponent, smli->StaticMeshActor, *smli) == TRUE)
				{
					bReload = TRUE;
				}
			}

			if (bReload)
			{
				for (INT SelIdx = 0; SelIdx < SelectedIndices.Num(); SelIdx++)
				{
					long Index = SelectedIndices(SelIdx);
					if (Index > 0)
					{
						FillListEntry(Index - 1);
						StaticMeshList->SetItemState(Index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
						StaticMeshList->RefreshItem(Index);
					}
				}
				UpdateTotalsEntry();
				GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
			}
		}
	}
}

/** 
 *	Set all the entries to the given mapping type, using the given resolution (if applicable)
 *
 *	@param	bInTextureMapping				TRUE if all selects components should be set to texture mapping.
 *											FALSE if all selects components should be set to vertex mapping.
 *	@param	InStaticLightingResolution		== 0 to use the values already set.
 *											!= 0 to force all to the given value.
 *											Ignored if setting to vertex mapping
 */
void WxDlgStaticMeshLightingInfo::SetMappingMethodOnSelectedComponents(UBOOL bInTextureMapping, INT InStaticLightingResolution)
{
 	const INT NumSelected = StaticMeshList->GetSelectedItemCount();
 	if (NumSelected > 0)
 	{
		const FScopedBusyCursor BusyCursor;
		TArray<long> SelectedIndices;
		TArray<FStaticMeshLightingInfo*> SelectedObjects;
		GetSelectedMeshes(SelectedObjects, SelectedIndices);
		if (SelectedObjects.Num() > 0)
		{
			const FScopedTransaction Transaction(*LocalizeUnrealEd("StaticMeshLightingInfoSet"));

			UBOOL bReload = FALSE;
			for (INT CompIdx = 0; CompIdx < SelectedObjects.Num(); CompIdx++)
			{
				FStaticMeshLightingInfo* smli = SelectedObjects(CompIdx);
				if (smli->StaticMeshActor)
				{
					smli->StaticMeshActor->Modify();
				}
				smli->StaticMeshComponent->Modify();
				smli->StaticMeshComponent->SetStaticLightingMapping(bInTextureMapping, InStaticLightingResolution);
				smli->StaticMeshComponent->InvalidateLightingCache();
				if (smli->StaticMeshActor)
				{
					smli->StaticMeshActor->ReattachComponent(smli->StaticMeshComponent);
				}
				if (FillStaticMeshLightingInfo(smli->StaticMeshComponent, smli->StaticMeshActor, *smli) == TRUE)
				{
					bReload = TRUE;
				}
			}

			if (bReload)
			{
				for (INT SelIdx = 0; SelIdx < SelectedIndices.Num(); SelIdx++)
				{
					long Index = SelectedIndices(SelIdx);
					if (Index > 0)
					{
						FillListEntry(Index - 1);
						StaticMeshList->SetItemState(Index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
						StaticMeshList->RefreshItem(Index);
					}
				}
				UpdateTotalsEntry();
				GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
			}
		}
	}
}


/** Custom dynamic list control for lighting results */
wxString WxLightingListCtrl::OnGetItemText(long item, long column) const
{
	check(LightingBuildInfoList);
	check(IsWithin<INT>(item, 0, LightingBuildInfoList->Num()));

	const FLightingBuildInfoEntry& lbie = ((*LightingBuildInfoList)(item));

	UBOOL bIsBSP = FALSE;
	if (lbie.Object)
	{
		bIsBSP = lbie.Object->IsA(ULightmappedSurfaceCollection::StaticClass());
	}

	FString Result;
	switch (column)
	{
		case WxDlgLightingBuildInfo::SORTBY_Level:
			Result = TEXT("<None>");
			if (lbie.Object)
			{
				Result = WxDlgLightingBuildInfo::GetLevelOrPackageName(lbie.Object);
			}
			break;
		case WxDlgLightingBuildInfo::SORTBY_Object:
			Result = TEXT("<None>");
			if (lbie.Object)
			{
				Result = WxDlgLightingBuildInfo::GetObjectDisplayName(lbie.Object, FALSE);
			}
			break;
		case WxDlgLightingBuildInfo::SORTBY_Timing:
			Result = FString::Printf(TEXT("%5.3f %%"), (lbie.LightingTime / TotalTime) * 100.0f);;
			break;
		case WxDlgLightingBuildInfo::SORTBY_TotalMemory:
			if ((lbie.TotalTexelMemory == -1) || (bIsBSP == TRUE))
			{
				Result = TEXT("---");
			}
			else
			{
				// Display the memory in kbytes
				Result = FString::Printf(TEXT("%8.2f"), FLOAT(lbie.TotalTexelMemory) / 1024.0f);
			}
			break;
		case WxDlgLightingBuildInfo::SORTBY_UnmappedMemory:
			if ((lbie.UnmappedTexelsMemory == -1) || (bIsBSP == TRUE))
			{
				Result = TEXT("---");
			}
			else
			{
				// Display the memory in kbytes
				Result = FString::Printf(TEXT("%8.2f"), FLOAT(lbie.UnmappedTexelsMemory) / 1024.0f);
			}
			break;
		case WxDlgLightingBuildInfo::SORTBY_UnmappedTexels:
			if ((lbie.UnmappedTexelsPercentage == -1.0f) || (bIsBSP == TRUE))
			{
				Result = TEXT("---");
			}
			else
			{
				Result = FString::Printf(TEXT("%4.1f"), lbie.UnmappedTexelsPercentage * 100.0f);
			}
			break;
	};
	return *Result;
}

/////
/**
 * Dialog that displays the lighting build info.
 */
BEGIN_EVENT_TABLE(WxDlgLightingBuildInfo, WxTrackableDialog)
	// Done via the EventHandler 
END_EVENT_TABLE()

/**
 * Dialog that displays the lighting build info for the selected level(s).
 */
static TArray<FLightingBuildInfoEntry> GLightingBuildInfoList;
WxDlgLightingBuildInfo::ESortMethod GLightingBuildInfo_SortMethod = WxDlgLightingBuildInfo::SORTBY_Timing;

// NOTE: There is a LOT of similar functionality in this dialog as there is in the DlgMapCheck.
// However, the data stored in the list was different enough to not justify re-working that dialog to support it.
WxDlgLightingBuildInfo::WxDlgLightingBuildInfo(wxWindow* InParent) :
	WxTrackableDialog(InParent, wxID_ANY, (wxString)*LocalizeUnrealEd("LightingBuildInfo"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	// Create and layout the controls
	wxBoxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		// Warning List
		wxBoxSizer* ListSizer = new wxBoxSizer(wxVERTICAL);
		{
			LightingBuildListCtrl = new WxLightingListCtrl();
			LightingBuildListCtrl->Create( this, wxID_ANY, wxDefaultPosition, wxSize(350, 200), wxLC_REPORT | wxLC_VIRTUAL);
			ListSizer->Add(LightingBuildListCtrl, 1, wxGROW|wxALL, 5);
		}
		MainSizer->Add(ListSizer, 1, wxGROW|wxALL, 5);		

		// Add buttons
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			{
				CloseButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd("Close"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(CloseButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				GotoButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("GoTo"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(GotoButton, 0, wxEXPAND|wxALL, 5);

				SyncButton = new wxButton( this, wxID_ANY, *LocalizeUnrealEd("Sync"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(SyncButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			// Memory amount displays...
			{
				wxBoxSizer* MemorySizer = new wxBoxSizer(wxVERTICAL);
				{
					TotalTexelMemoryLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("LBI_TotalTexelMemoryLabel"));
					MemorySizer->Add(TotalTexelMemoryLabel, 0, wxEXPAND|wxALL, 5);
					TotalTexelMemoryDisplay = new wxStaticText(this, wxID_ANY, TEXT(""));
					MemorySizer->Add(TotalTexelMemoryDisplay, 0, wxEXPAND|wxALL, 5);
					TotalUnmappedTexelMemoryLabel = new wxStaticText(this, wxID_ANY, *LocalizeUnrealEd("LBI_TotalUnmappedTexelMemoryLabel"));
					MemorySizer->Add(TotalUnmappedTexelMemoryLabel, 0, wxEXPAND|wxALL, 5);
					TotalUnmappedTexelMemoryDisplay = new wxStaticText(this, wxID_ANY, TEXT(""));
					MemorySizer->Add(TotalUnmappedTexelMemoryDisplay, 0, wxEXPAND|wxALL, 5);
					TotalUnmappedTexelMemoryPercentageDisplay = new wxStaticText(this, wxID_ANY, TEXT(""));
					MemorySizer->Add(TotalUnmappedTexelMemoryPercentageDisplay, 0, wxEXPAND|wxALL, 5);
				}
				ButtonSizer->Add(MemorySizer, 0, wxALIGN_BOTTOM|wxALL, 5);
			}
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_TOP|wxALL, 5);

	}
	SetSizer(MainSizer);

	// Setup the button events
	// Use the event handler and the Id of the button of interest
	wxEvtHandler* EvtHandler = GetEventHandler();
	EvtHandler->Connect(CloseButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgLightingBuildInfo::OnClose));
	EvtHandler->Connect(GotoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgLightingBuildInfo::OnGoTo));
	EvtHandler->Connect(SyncButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(WxDlgLightingBuildInfo::OnSync));
	EvtHandler->Connect(LightingBuildListCtrl->GetId(), wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler(WxDlgLightingBuildInfo::OnItemActivated));
	EvtHandler->Connect(LightingBuildListCtrl->GetId(), wxEVT_COMMAND_LIST_COL_CLICK, wxListEventHandler(WxDlgLightingBuildInfo::OnListColumnClick));

	// Fill in the columns
	SetupListColumns();

	// Load window position.
	FWindowUtil::LoadPosSize(TEXT("DlgLightingBuildInfo2"), this, -1, -1, 960, 500);

	LightingBuildInfoList = &GLightingBuildInfoList;
}

WxDlgLightingBuildInfo::~WxDlgLightingBuildInfo()
{
	// Save window position.
	FWindowUtil::SavePosSize(TEXT("DlgLightingBuildInfo2"), this);
}

/** Called when the window has been selected from within the ctrl + tab dialog. */
void WxDlgLightingBuildInfo::OnSelected()
{
	WxTrackableDialog::OnSelected();
}

/** Shows the dialog only if there are messages to display. */
void WxDlgLightingBuildInfo::ShowConditionally()
{
	LoadListCtrl();
	if (LightingBuildListCtrl->GetItemCount() > 0)
	{
		Show( true );
	}
}

/** Clears out the list of messages appearing in the window. */
void WxDlgLightingBuildInfo::ClearMessageList()
{
	LightingBuildListCtrl->DeleteAllItems();
	LightingBuildInfoList->Empty();
	ReferencedObjects.Empty();
}

/** Freezes the message list. */
void WxDlgLightingBuildInfo::FreezeMessageList()
{
	LightingBuildListCtrl->Freeze();
}

/** Thaws the message list. */
void WxDlgLightingBuildInfo::ThawMessageList()
{
	LightingBuildListCtrl->Thaw();
}

/** 
 *	Adds a message to the dialog, to be displayed when the dialog is shown.
 *
 *	@param	InObject						The object associated with the message
 *	@param	InLightingTime					The percentage of lighting time the object took.
 *	@param	InUnmappedTexelsPercentage		The percentage of unmapped texels this object has.
 *	@param	InUnmappedTexelsMemory			The amount of memory consumed by unmapped texels of this object.
 *	@param	InTotalTexelMemory				The memory consumed by all texels for this object.
 */
void WxDlgLightingBuildInfo::AddItem(UObject* InObject, DOUBLE InLightingTime, FLOAT InUnmappedTexelsPercentage, 
	INT InUnmappedTexelsMemory, INT InTotalTexelMemory)
{
	// Columns are, from left to right: 
	//	Level, object, percentage time, percentage unmapped
	FLightingBuildInfoEntry Info;

	Info.Object = InObject;
	Info.LightingTime = InLightingTime;
	Info.UnmappedTexelsPercentage = InUnmappedTexelsPercentage;
	Info.UnmappedTexelsMemory = InUnmappedTexelsMemory;
	Info.TotalTexelMemory = InTotalTexelMemory;

	if (Info.Object)
	{
		WxDlgLightingBuildInfo* LightingBuildInfo = GApp->GetDlgLightingBuildInfo();
		check(LightingBuildInfo);
		LightingBuildInfo->ReferencedObjects.AddUniqueItem(Info.Object);
	}
	GLightingBuildInfoList.AddUniqueItem(Info);
}

/** 
 *	Adds the given static mesh lighting info to the list.
 *
 *	@param	InLightingBuildInfo		The lighting build info to add
 */
void WxDlgLightingBuildInfo::AddItem(FLightingBuildInfoEntry& InLightingBuildInfo)
{
	GLightingBuildInfoList.AddUniqueItem(InLightingBuildInfo);
	if (InLightingBuildInfo.Object)
	{
		ReferencedObjects.AddUniqueItem(InLightingBuildInfo.Object);
	}
}

/**
 *	Serialize the referenced assets to prevent GC.
 *
 *	@param	Ar		The archive to serialize to.
 */
void WxDlgLightingBuildInfo::Serialize(FArchive& Ar)
{
	Ar << ReferencedObjects;
}

/** Loads the list control with the contents of the GErrorWarningInfoList array. */
void WxDlgLightingBuildInfo::LoadListCtrl()
{
	DOUBLE StartTime = appSeconds();
	LightingBuildListCtrl->SetItemCount(LightingBuildInfoList->Num());

	// Pre calculate Total Time
	DOUBLE TotalExecutionTime = 0.0;
	TrackingTotalMemory = 0;
	TrackingTotalUnmappedMemory = 0;

	for (INT Idx = 0; Idx < LightingBuildInfoList->Num(); Idx++)
	{
		const FLightingBuildInfoEntry& lbie = ((*LightingBuildInfoList)(Idx));
		TotalExecutionTime += lbie.LightingTime;

		UBOOL bIsBSP = FALSE;
		if (lbie.Object)
		{
			bIsBSP = lbie.Object->IsA(ULightmappedSurfaceCollection::StaticClass());
		}
		if (!((lbie.TotalTexelMemory == -1) || (bIsBSP == TRUE)))
		{
			TrackingTotalMemory += lbie.TotalTexelMemory;
		}
		if (!((lbie.UnmappedTexelsMemory == -1) || (bIsBSP == TRUE)))
		{
			TrackingTotalUnmappedMemory += lbie.UnmappedTexelsMemory;
		}
	}

	//cache off data into the text ctrl
	LightingBuildListCtrl->SetLightingBuildInfoList (LightingBuildInfoList);
	LightingBuildListCtrl->SetTotalTime(TotalExecutionTime);

	FString DisplayString;
	DisplayString = FString::Printf(TEXT("%.2f (MB)"), FLOAT(TrackingTotalMemory) / (1024.0f * 1024.0f));
	TotalTexelMemoryDisplay->SetLabel(*DisplayString);
	DisplayString = FString::Printf(TEXT("%.2f (MB)"), FLOAT(TrackingTotalUnmappedMemory) / (1024.0f * 1024.0f));
	TotalUnmappedTexelMemoryDisplay->SetLabel(*DisplayString);
	DisplayString = FString::Printf(TEXT("%.2f %%"), FLOAT(TrackingTotalUnmappedMemory) / FLOAT(TrackingTotalMemory) * 100.0f);
	TotalUnmappedTexelMemoryPercentageDisplay->SetLabel(*DisplayString);

	DOUBLE EndTime = appSeconds();
	debugf(NAME_PerfEvent, TEXT("LIGHTING BUILD INFO TIME LoadListCtrl: %f"), EndTime-StartTime);
}

/**
 *	Show the dialog.
 *
 *	@param	show	If TRUE show the dialog, FALSE hide it.
 *
 *	@return	bool	
 */
bool WxDlgLightingBuildInfo::Show( bool show )
{
	if (show)
	{
		LoadListCtrl();
	}
	else
	{
		ClearMessageList();
	}

	return wxDialog::Show(show);
}

/** Event handler for when the refresh button is clicked on. */
void WxDlgLightingBuildInfo::OnRefresh(wxCommandEvent& In)
{
	SortList(GLightingBuildInfo_SortMethod);
	LoadListCtrl();
}

/** Event handler for when the close button is clicked on. */
void WxDlgLightingBuildInfo::OnClose(wxCommandEvent& In)
{
	Show(0);
}

/** Event handler for when the goto button is clicked on. */
void WxDlgLightingBuildInfo::OnGoTo(wxCommandEvent& In)
{
	//@todo. Lots of duplicate code in here from MapCheck...
	// Make a singe Column-based info dialog to avoid these sorts of things!

	// Clear existing selections
	GEditor->SelectNone(FALSE, TRUE);

	const INT NumSelected = LightingBuildListCtrl->GetSelectedItemCount();
	if (NumSelected > 0)
	{
		const FScopedBusyCursor BusyCursor;
		TArray<AActor*> SelectedActors;
		TArray<UPrimitiveComponent*> SelectedComponents;
		TArray<UObject*> SelectedObjects;
		TArray<ULightmappedSurfaceCollection*> SelectedSurfaceCollections;

		long ItemIndex = LightingBuildListCtrl->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		while (ItemIndex != -1)
		{
			check(IsWithin<INT>(ItemIndex, 0, LightingBuildInfoList->Num()));
			FLightingBuildInfoEntry* lbie = &((*LightingBuildInfoList)(ItemIndex));//(FLightingBuildInfoEntry*)LightingBuildListCtrl->GetItemData(ItemIndex);
			if (lbie)
			{
				UObject* Object = lbie->Object;
				if (Object)
				{
					AActor* Actor = Cast<AActor>(Object);
					UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);
					ULightmappedSurfaceCollection* SelectedSurfaceCollection = Cast<ULightmappedSurfaceCollection>(Object);

					if (Actor)
					{
						SelectedActors.AddItem( Actor );
					}
					else if (Component)
					{
						SelectedComponents.AddItem(Component);
					}
					else if (SelectedSurfaceCollection)
					{
						SelectedSurfaceCollections.AddItem(SelectedSurfaceCollection);
					}
					else
					{
						SelectedObjects.AddItem(Object);
					}
				}
				ItemIndex = LightingBuildListCtrl->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
			}
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

		// If any surface collections are selected, select them in the editor
		if (SelectedSurfaceCollections.Num() > 0)
		{
			for (INT CollectionIdx = 0; CollectionIdx < SelectedSurfaceCollections.Num(); CollectionIdx++)
			{
				ULightmappedSurfaceCollection* SurfaceCollection = SelectedSurfaceCollections(CollectionIdx);
				if (SurfaceCollection)
				{
					// Select the surfaces in this mapping
					for (INT SurfaceIdx = 0; SurfaceIdx < SurfaceCollection->Surfaces.Num(); SurfaceIdx++)
					{
						INT SurfaceIndex = SurfaceCollection->Surfaces(SurfaceIdx);
						FBspSurf& Surf = SurfaceCollection->SourceModel->Surfs(SurfaceIndex);
						SurfaceCollection->SourceModel->ModifySurf(SurfaceIndex, 0);
						Surf.PolyFlags |= PF_Selected;
						if (Surf.Actor)
						{
							SelectedActors.AddUniqueItem(Surf.Actor);
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
		else
		if (SelectedObjects.Num() > 0)
		{
			GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
		}
	}
}

/** Event handler for when the sync button is clicked on. */
void WxDlgLightingBuildInfo::OnSync(wxCommandEvent& In)
{
	const INT NumSelected = LightingBuildListCtrl->GetSelectedItemCount();
	if (NumSelected > 0)
	{
		const FScopedBusyCursor BusyCursor;
		TArray<UObject*> SelectedObjects;
		long ItemIndex = LightingBuildListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		while (ItemIndex != -1)
		{
			check(IsWithin<INT>(ItemIndex, 0, LightingBuildInfoList->Num()));
			FLightingBuildInfoEntry* lbie = &((*LightingBuildInfoList)(ItemIndex));//(FLightingBuildInfoEntry*)(LightingBuildListCtrl->GetItemData(ItemIndex));
			if (lbie && lbie->Object)
			{
				UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(lbie->Object);

				if (SMComp && SMComp->StaticMesh)
				{
					SelectedObjects.AddItem(SMComp->StaticMesh);
				}
			}
			ItemIndex = LightingBuildListCtrl->GetNextItem(ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		}

		if (SelectedObjects.Num() > 0)
		{
			GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
		}
	}
}

/** Event handler for when a message is clicked on. */
void WxDlgLightingBuildInfo::OnItemActivated(wxListEvent& In)
{
	const long ItemIndex = In.GetIndex();

	check(IsWithin<INT>(ItemIndex, 0, LightingBuildInfoList->Num()));
	FLightingBuildInfoEntry* lbie = &((*LightingBuildInfoList)(ItemIndex));//(FLightingBuildInfoEntry*)LightingBuildListCtrl->GetItemData(ItemIndex);
	if (lbie == NULL)
	{
		return;
	}

	UObject* Obj = lbie->Object;
	ULightmappedSurfaceCollection* SurfaceCollection = Cast<ULightmappedSurfaceCollection>(Obj);
	if (SurfaceCollection)
	{
		// Deselect all selected object...
		GEditor->SelectNone( TRUE, TRUE );

		// Select the surfaces in this mapping
		TArray<AActor*> SelectedActors;
		for (INT SurfaceIdx = 0; SurfaceIdx < SurfaceCollection->Surfaces.Num(); SurfaceIdx++)
		{
			INT SurfaceIndex = SurfaceCollection->Surfaces(SurfaceIdx);
			FBspSurf& Surf = SurfaceCollection->SourceModel->Surfs(SurfaceIndex);
			SurfaceCollection->SourceModel->ModifySurf(SurfaceIndex, 0);
			Surf.PolyFlags |= PF_Selected;
			if (Surf.Actor)
			{
				SelectedActors.AddUniqueItem(Surf.Actor);
			}
		}

		// Add the brushes to the selected actors list...
		if (SelectedActors.Num() > 0)
		{
			GEditor->MoveViewportCamerasToActor(SelectedActors, FALSE);
		}

		GEditor->NoteSelectionChange();

		return;
	}

	AActor* Actor = Cast<AActor>(Obj);
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Obj);
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

	if (Actor)
	{
//		const FScopedTransaction Transaction(*LocalizeUnrealEd("MapCheckGoto") );
		GEditor->SelectNone( TRUE, TRUE );
		GEditor->SelectActor( Actor, TRUE, NULL, TRUE, TRUE );
		GEditor->MoveViewportCamerasToActor( *Actor, FALSE );
	}
	else
	if (Obj)
	{
		TArray<UObject*> SelectedObjects;
		SelectedObjects.AddItem(Obj);
		GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
	}
}

/** Event handler for when a column header is clicked on. */
void WxDlgLightingBuildInfo::OnListColumnClick(wxListEvent& In)
{
	// Sort by the selected column
	SortList((ESortMethod)(In.m_col));
	LoadListCtrl();
}

/** Set up the columns for the ErrorWarningList control. */
void WxDlgLightingBuildInfo::SetupListColumns()
{
	if (LightingBuildListCtrl)
	{
		// Columns are, from left to right: 
		//	Level, actor, static mesh, mapping type, Has Lightmap UVs, static lighting resolution, texture light map, vertex light map, texture shadow map, vertex shadow map

		// THIS MUST BE THE REVERSE ORDER OF THE ESortMethod ENUMERATION IN THE CLASS!
		LightingBuildListCtrl->InsertColumn(0, *LocalizeUnrealEd("PercentageOfUnmappedTexels"),	wxLIST_FORMAT_CENTER,	125);
		LightingBuildListCtrl->InsertColumn(0, *LocalizeUnrealEd("UnmappedTexelsMemory"),		wxLIST_FORMAT_CENTER,	175);
		LightingBuildListCtrl->InsertColumn(0, *LocalizeUnrealEd("TotalTexelMemory"),			wxLIST_FORMAT_CENTER,	175);
		LightingBuildListCtrl->InsertColumn(0, *LocalizeUnrealEd("PercentageOfLightingTime"),	wxLIST_FORMAT_CENTER,	125);
		LightingBuildListCtrl->InsertColumn(0, *LocalizeUnrealEd("Object"),						wxLIST_FORMAT_LEFT,		200);
		LightingBuildListCtrl->InsertColumn(0, *LocalizeUnrealEd("Level/Package"),				wxLIST_FORMAT_LEFT,		150);
	}
}

/** 
 *	Get the level/package name for the given object.
 *
 *	@param	InObject	The object to retrieve the level/package name for.
 *
 *	@return	FString		The name of the level/package.
 */
FString WxDlgLightingBuildInfo::GetLevelOrPackageName(UObject* InObject)
{
	ULightmappedSurfaceCollection* SurfaceCollection = Cast<ULightmappedSurfaceCollection>(InObject);
	if (SurfaceCollection && SurfaceCollection->SourceModel)
	{
		return SurfaceCollection->SourceModel->GetOutermost()->GetName();
	}

	AActor* Actor = Cast<AActor>(InObject);
	if (Actor)
	{
		return Actor->GetLevel()->GetOutermost()->GetName();
	}

	return InObject->GetOutermost()->GetName();
}

/** 
 *	Get the display name for the given object.
 *
 *	@param	InObject	The object to retrieve the name for.
 *	@param	bFullPath	If TRUE, return the full path name.
 *
 *	@return	FString		The display name of the object.
 */
FString WxDlgLightingBuildInfo::GetObjectDisplayName(UObject* InObject, UBOOL bFullPath)
{
	if (InObject == NULL)
	{
		return TEXT("");
	}

	AActor* Actor = NULL;

	// Is this an object held by an actor?
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(InObject);
	if (PrimitiveComponent)
	{
		if (PrimitiveComponent->IsAttached() == TRUE)
		{
			Actor = Cast<AActor>(PrimitiveComponent->GetOwner());
		}
		if (Actor == NULL)
		{
			Actor = Cast<AActor>(PrimitiveComponent->GetOuter());
		}
		if (Actor == NULL)
		{
			//@todo. Can iterate over all actors and find the component in its AllComponents array...
		}
	}

	if (Actor)
	{
		return bFullPath ? Actor->GetPathName() : Actor->GetName();
	}

	return bFullPath ? InObject->GetPathName() : InObject->GetName();
}

/** Compare routine used by sort for LightingBuildInfo. */
INT DlgLightingBuildInfo_Sort(const FLightingBuildInfoEntry& A, const FLightingBuildInfoEntry& B)
{
	UBOOL bNumericSort = TRUE;
	UBOOL bDisregardBSP = TRUE;
	FLOAT Value_A = -1.0f;
	FLOAT Value_B = -1.0f;

	switch (GLightingBuildInfo_SortMethod)
	{
	case WxDlgLightingBuildInfo::SORTBY_Timing:
		{
			Value_A = A.LightingTime;
			Value_B = B.LightingTime;
			bDisregardBSP = FALSE;
		}
		break;
	case WxDlgLightingBuildInfo::SORTBY_TotalMemory:
		{
			Value_A = FLOAT(A.TotalTexelMemory);
			Value_B = FLOAT(B.TotalTexelMemory);
		}
		break;
	case WxDlgLightingBuildInfo::SORTBY_UnmappedMemory:
		{
			Value_A = FLOAT(A.UnmappedTexelsMemory);
			Value_B = FLOAT(B.UnmappedTexelsMemory);
		}
		break;
	case WxDlgLightingBuildInfo::SORTBY_UnmappedTexels:
		{
			Value_A = A.UnmappedTexelsPercentage;
			Value_B = B.UnmappedTexelsPercentage;
		}
		break;
	case WxDlgLightingBuildInfo::SORTBY_Level:
		{
			FString LevelName_A = WxDlgLightingBuildInfo::GetLevelOrPackageName(A.Object);
			FString LevelName_B = WxDlgLightingBuildInfo::GetLevelOrPackageName(B.Object);
			if (LevelName_A != LevelName_B)
			{
				return appStrcmp(*LevelName_A, *LevelName_B);
			}
			bNumericSort = FALSE;
		}
		break;
	default:
		{
			bNumericSort = FALSE;
		}
		break;
	}

	if (bNumericSort == TRUE)
	{
		UBOOL bIsBSP_A = A.Object->IsA(ULightmappedSurfaceCollection::StaticClass());
		UBOOL bIsBSP_B = B.Object->IsA(ULightmappedSurfaceCollection::StaticClass());
		FLOAT Test_A = (bDisregardBSP && bIsBSP_A) ? -1.0f : Value_A;
		FLOAT Test_B = (bDisregardBSP && bIsBSP_B) ? -1.0f : Value_B;
		if (Test_A != Test_B)
		{
			if ((Test_A >= 0.0f) && (Test_B >= 0.0f))
			{
				return (Test_A < Test_B) ? 1 : -1;
			}
			else if (Test_A >= 0.0f)
			{
				return -1;
			}
			else if (Test_B >= 0.0f)
			{
				return 1;
			}
			//just do not swap
			return 0;
		}
	}

	FString ObjectPathName_A = WxDlgLightingBuildInfo::GetObjectDisplayName(A.Object, FALSE);
	FString ObjectPathName_B = WxDlgLightingBuildInfo::GetObjectDisplayName(B.Object, FALSE);

	// Default sorting is the full path name of the object...
	return appStrcmp(*ObjectPathName_A, *ObjectPathName_B);
}

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FLightingBuildInfoEntry,DlgLightingResults,{ return DlgLightingBuildInfo_Sort(A, B); });

/** Sort the list by the given method. */
void WxDlgLightingBuildInfo::SortList(ESortMethod InSortMethod)
{
	//@todo. Support 'double' selection of sorting column. 
	// Ie, if they click Texture map usage - the first time sort highest to lowest, the second (in a row) lowest to highest
	GLightingBuildInfo_SortMethod = InSortMethod;
	Sort<USE_COMPARE_CONSTREF(FLightingBuildInfoEntry,DlgLightingResults)>(LightingBuildInfoList->GetData(),LightingBuildInfoList->Num());
	LightingBuildListCtrl->Refresh();
}
