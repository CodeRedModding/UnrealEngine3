/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlRenderPage.h"
#include "RemoteControlGame.h"
#include "BusyCursor.h"

/** Global controlling whether to show the FPS counter */
extern UBOOL GShowFpsCounter;

BEGIN_EVENT_TABLE(WxRemoteControlRenderPage, WxRemoteControlPage)
	EVT_MENU( IDM_VIEWPORT_SHOW_STATICMESHES, WxRemoteControlRenderPage::OnShowStaticMeshes )
	EVT_MENU( IDM_VIEWPORT_SHOW_TERRAIN, WxRemoteControlRenderPage::OnShowTerrain )
	EVT_MENU( IDM_VIEWPORT_SHOW_BSP, WxRemoteControlRenderPage::OnShowBSP )
	EVT_MENU( IDM_VIEWPORT_SHOW_BSPSPLIT, WxRemoteControlRenderPage::OnShowBSPSplit )
	EVT_MENU( IDM_VIEWPORT_SHOW_COLLISION, WxRemoteControlRenderPage::OnShowCollision )
	EVT_MENU( IDM_VIEWPORT_SHOW_BOUNDS, WxRemoteControlRenderPage::OnShowBounds )
	EVT_MENU( IDM_VIEWPORT_SHOW_MESHEDGES, WxRemoteControlRenderPage::OnShowMeshEdges )
	EVT_MENU( IDM_VIEWPORT_SHOW_VERTEXCOLORS, WxRemoteControlRenderPage::OnShowVertexColors )
	EVT_MENU( IDM_VIEWPORT_SHOW_SCENE_CAPTURE_UPDATES, WxRemoteControlRenderPage::OnShowSceneCaptureUpdates )
	EVT_MENU( IDM_VIEWPORT_SHOW_SHADOWFRUSTUMS, WxRemoteControlRenderPage::OnShowShadowFrustums )
	EVT_MENU( IDM_VIEWPORT_SHOW_HITPROXIES, WxRemoteControlRenderPage::OnShowHitProxies )
	EVT_MENU( IDM_VIEWPORT_SHOW_FOG, WxRemoteControlRenderPage::OnShowFog )
	EVT_MENU( IDM_VIEWPORT_SHOW_PARTICLES, WxRemoteControlRenderPage::OnShowParticles )
	EVT_MENU( ID_REMOTECONTROL_RENDERPAGE_SHOW_CONSTRAINTS, WxRemoteControlRenderPage::OnShowConstraints )
	EVT_MENU( ID_REMOTECONTROL_RENDERPAGE_SHOW_TERRAINPATCHES, WxRemoteControlRenderPage::OnShowTerrainPatches )
	EVT_MENU( ID_REMOTECONTROL_RENDERPAGE_SHOW_SKELMESHES, WxRemoteControlRenderPage::OnShowSkeletalMeshes )
    EVT_MENU( ID_REMOTECONTROL_RENDERPAGE_SHOW_SKIN, WxRemoteControlRenderPage::OnShowSkin )
    EVT_MENU( ID_REMOTECONTROL_RENDERPAGE_SHOW_BONES, WxRemoteControlRenderPage::OnShowBones )
	EVT_MENU( IDM_VIEWPORT_SHOW_DECALS, WxRemoteControlRenderPage::OnShowDecals )
	EVT_MENU( IDM_VIEWPORT_SHOW_DECALINFO, WxRemoteControlRenderPage::OnShowDecalInfo )
	EVT_MENU( IDM_VIEWPORT_SHOW_LEVELCOLORATION, WxRemoteControlRenderPage::OnShowLevelColoration )
	EVT_MENU( IDM_VIEWPORT_SHOW_SPRITES, WxRemoteControlRenderPage::OnShowSprites )
	EVT_MENU( IDM_VIEWPORT_SHOW_CONSTRAINTS, WxRemoteControlRenderPage::OnShowConstraints )
END_EVENT_TABLE()

WxRemoteControlRenderPage::WxRemoteControlRenderPage(FRemoteControlGame *InGame, wxNotebook *InNotebook)
    :	WxRemoteControlPage(InGame)
	,	bShowSkin(TRUE)
	,	bShowBones(FALSE)
{
	// Get the current display info.
	UINT CurWidth;
	UINT CurHeight;
	UBOOL bFullscreen;
	GetGame()->GetDisplayInfo( CurWidth, CurHeight, bFullscreen );
	const FString StrCurRes = FString::Printf(TEXT("%d x %d%s"), CurWidth, CurHeight, bFullscreen?TEXT(" Fullscreen"):TEXT(""));

	const bool bSuccess = wxXmlResource::Get()->LoadPanel(this, InNotebook, TEXT("ID_RENDER_PAGE"));
	check( bSuccess );

	// Assemble the "Open Level" control.
	wxBitmapButton *BitmapFileButton;
	BindControl( BitmapFileButton, XRCID("ID_FILE_BUTTON") );

	WxBitmap FileBitmap;
	const UBOOL bFolderOpenLoadSuccessful = FileBitmap.Load(TEXT("FolderOpen"));
	check( bFolderOpenLoadSuccessful );
	BitmapFileButton->SetBitmapLabel( FileBitmap );
	ADDEVENTHANDLER(XRCID("ID_FILE_BUTTON"), wxEVT_COMMAND_BUTTON_CLICKED, &WxRemoteControlRenderPage::OnOpenLevel);

	// Only enable the "Open Level" button if we're not in the editor.
	const UBOOL bInEditor = GetGame()->IsInEditor();
	BitmapFileButton->Enable( !bInEditor );

	// Assemble game resolution controls.
	BindControl(GameResolutionChoice, XRCID("ID_GAME_RESOLUTION"));

	GameResolutionChoice->Append( *StrCurRes );

	// Present non-fullscreen options.
	if ( !bFullscreen )
	{
		if ( CurWidth != 640 || CurHeight != 480 )
		{
			GameResolutionChoice->Append( TEXT("640 x 480") );
		}
		if ( CurWidth != 800 || CurHeight != 600 )
		{
			GameResolutionChoice->Append( TEXT("800 x 600") );
		}
		// Standard 360 TV mode
		if ( CurWidth != 960 || CurHeight != 720 )
		{
			GameResolutionChoice->Append( TEXT("960 x 720") );
		}
		if ( CurWidth != 1024 || CurHeight != 768 )
		{
			GameResolutionChoice->Append( TEXT("1024 x 768") );
		}
		// G4W aspect 16:10 (1152x720)
		if ( CurWidth != 1152 || CurHeight != 720 )
		{
			GameResolutionChoice->Append( TEXT("1152 x 720") );
		}
		// Standard 360 wide screen mode
		if ( CurWidth != 1280 || CurHeight != 720 )
		{
			GameResolutionChoice->Append( TEXT("1280 x 720") );
		}
		if ( CurWidth != 1280 || CurHeight != 1024 )
		{
			GameResolutionChoice->Append( TEXT("1280 x 1024") );
		}
		if ( CurWidth != 1600 || CurHeight != 1200 )
		{
			GameResolutionChoice->Append( TEXT("1600 x 1200") );
		}
		if ( CurWidth != 1680 || CurHeight != 1050 )
		{
			GameResolutionChoice->Append( TEXT("1680 x 1050") );
		}
		if ( CurWidth != 1920 || CurHeight != 1080 )
		{
			GameResolutionChoice->Append( TEXT("1920 x 1080") );
		}
		if ( CurWidth != 1920 || CurHeight != 1200 )
		{
			GameResolutionChoice->Append( TEXT("1920 x 1200") );
		}
	}

	// Present fullscreen options -- can't go fullscreen in the editor.
	if( !GetGame()->IsInEditor() )
	{
		GameResolutionChoice->Append( TEXT("640 x 480 Fullscreen") );
		GameResolutionChoice->Append( TEXT("800 x 600 Fullscreen") );
		GameResolutionChoice->Append( TEXT("960 x 720 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1024 x 768 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1280 x 720 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1152 x 720 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1280 x 1024 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1600 x 1200 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1680 x 1050 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1920 x 1080 Fullscreen") );
		GameResolutionChoice->Append( TEXT("1920 x 1200 Fullscreen") );
	}
	GameResolutionChoice->Append(TEXT("Custom..."));

	ADDEVENTHANDLER(XRCID("ID_GAME_RESOLUTION"), wxEVT_COMMAND_CHOICE_SELECTED, &WxRemoteControlRenderPage::OnGameResolutionChoice);

	// Max texture size
	BindControl(MaxTextureSizeChoice, XRCID("ID_MAX_TEXTURE_SIZE"));
	SetupResolutionChoiceUI(*MaxTextureSizeChoice, 32, 4096);
	ADDEVENTHANDLER(XRCID("ID_MAX_TEXTURE_SIZE"), wxEVT_COMMAND_CHOICE_SELECTED, &WxRemoteControlRenderPage::OnMaxTextureSizeChoice);

	// Max shadow map size
	BindControl(MaxShadowSizeChoice, XRCID("ID_MAX_SHADOW_SIZE"));

	INT MinShadowMapSize = 0;
	//const UBOOL bGotIntProperty = GetGame()->GetObjectIntProperty(MinShadowMapSize, TEXT("Engine"), TEXT("MinShadowResolution"), TEXT(""));
	//check( bGotIntProperty );
	INT MaxShadowMapSize = 0;
	//const UBOOL bGotIntProperty2 = GetGame()->GetObjectIntProperty(MaxShadowMapSize, TEXT("Engine"), TEXT("MaxShadowResolution"), TEXT(""));
	//check( bGotIntProperty2 );
	SetupResolutionChoiceUI( *MaxShadowSizeChoice, MinShadowMapSize, MaxShadowMapSize );

	ADDEVENTHANDLER(XRCID("ID_MAX_SHADOW_SIZE"), wxEVT_COMMAND_CHOICE_SELECTED, &WxRemoteControlRenderPage::OnMaxShadowSizeChoice);

	// Enable/disable post process effects
	BindControl(UsePostProcessChoice, XRCID("ID_POST_PROCESS_EFFECTS"));
	SetupBooleanChoiceUI(*UsePostProcessChoice);
	ADDEVENTHANDLER(XRCID("ID_POST_PROCESS_EFFECTS"), wxEVT_COMMAND_CHOICE_SELECTED, &WxRemoteControlRenderPage::OnPostProcessChoice);

	BindControl(ViewmodeChoice, XRCID("ID_VIEWMODE"));
	ViewmodeChoice->Append(TEXT("Wireframe"));
	ViewmodeChoice->Append(TEXT("BrushWireframe"));
	ViewmodeChoice->Append(TEXT("Unlit"));
	ViewmodeChoice->Append(TEXT("Lit"));
	ViewmodeChoice->Append(TEXT("LightingOnly"));
	ViewmodeChoice->Append(TEXT("LightComplexity"));
	ViewmodeChoice->Append(TEXT("ShaderComplexity"));

	ADDEVENTHANDLER(XRCID("ID_VIEWMODE"), wxEVT_COMMAND_CHOICE_SELECTED, &WxRemoteControlRenderPage::OnViewmodeChoice);

	wxBitmapButton *BitmapButton;
	BindControl( BitmapButton, XRCID("ID_SHOW_FLAGS") );
	WxBitmap bitmap;

	const UBOOL bDownArrowLargeLoadSuccessful = bitmap.Load(TEXT("DownArrowLarge.png"));
	check( bDownArrowLargeLoadSuccessful );

	BitmapButton->SetBitmapLabel(bitmap);
	ADDEVENTHANDLER(XRCID("ID_SHOW_FLAGS"), wxEVT_COMMAND_BUTTON_CLICKED, &WxRemoteControlRenderPage::OnShowFlags);

	// Slomo text control.
	BindControl(SlomoTextCtrl, XRCID("ID_SLOMO_TEXTCTRL"));
	ADDEVENTHANDLER(XRCID("ID_SLOMO_TEXTCTRL"), wxEVT_COMMAND_TEXT_ENTER, &WxRemoteControlRenderPage::OnApplySlomo);

	// FOV text control.
	BindControl(FOVTextCtrl, XRCID("ID_FOV_TEXTCTRL"));
	ADDEVENTHANDLER(XRCID("ID_FOV_TEXTCTRL"), wxEVT_COMMAND_TEXT_ENTER, &WxRemoteControlRenderPage::OnApplyFOV);

	// DynamicShadows check box.
	BindControl(DynamicShadowsCheckBox, XRCID("ID_DYNAMIC_SHADOWS"));
	ADDEVENTHANDLER(XRCID("ID_DYNAMIC_SHADOWS"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxRemoteControlRenderPage::OnDynamicShadows);

	// ShowHUD check box.
	BindControl(ShowHUDCheckBox, XRCID("ID_SHOW_HUD"));
	ADDEVENTHANDLER(XRCID("ID_SHOW_HUD"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxRemoteControlRenderPage::OnShowHUD);

	// PlayersOnly check box.
	BindControl(PlayersOnlyCheckBox, XRCID("ID_PLAYERS_ONLY"));
	ADDEVENTHANDLER(XRCID("ID_PLAYERS_ONLY"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxRemoteControlRenderPage::OnPlayersOnly);

	// Core stats
	BindControl(FramesPerSecondCheckBox, XRCID("ID_FRAMES_PER_SECOND"));
	ADDEVENTHANDLER(XRCID("ID_FRAMES_PER_SECOND"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxRemoteControlRenderPage::OnFPSClicked);

	BindControl(D3DSceneCheckBox, XRCID("ID_D3DSCENE"));
	ADDEVENTHANDLER(XRCID("ID_D3DSCENE"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxRemoteControlRenderPage::OnD3DSceneClicked);

	BindControl(MemoryCheckBox, XRCID("ID_MEMORY"));
	ADDEVENTHANDLER(XRCID("ID_MEMORY"), wxEVT_COMMAND_CHECKBOX_CLICKED, &WxRemoteControlRenderPage::OnMemoryClicked);

	// Size the window.
	GetSizer()->Fit(this);
	GetSizer()->SetSizeHints(this);
}

/**
 * Return's the page's title, displayed on the notebook tab.
 */
const TCHAR *WxRemoteControlRenderPage::GetPageTitle() const
{
	return TEXT("Rendering");
}

/**
 * Refreshes page contents.
 */
void WxRemoteControlRenderPage::RefreshPage(UBOOL bForce)
{
	// Get the current display info.
	UINT CurWidth, CurHeight;
	UBOOL bFullscreen;
	GetGame()->GetDisplayInfo( CurWidth, CurHeight, bFullscreen );

	// Convert res info to string and update controls.
	const FString StrCurRes = FString::Printf(TEXT("%d x %d%s"), CurWidth, CurHeight, bFullscreen?TEXT(" Fullscreen"):TEXT(""));
	if( GameResolutionChoice->FindString( *StrCurRes ) != wxNOT_FOUND )
	{
		GameResolutionChoice->SetStringSelection( *StrCurRes );
	}
	else
	{
		GameResolutionChoice->Append( *StrCurRes );
		GameResolutionChoice->SetStringSelection( *StrCurRes );
	}

	// texture size
	UpdateResolutionChoiceUI(*MaxTextureSizeChoice, TEXT("D3DRenderDevice"), TEXT("UserMaxTextureSize"));

	// max shadow resolution
	UpdateResolutionChoiceUI(*MaxShadowSizeChoice, TEXT("D3DRenderDevice"), TEXT("MaxShadowResolution"));

	// post process effects
	UpdateBooleanChoiceUI(*UsePostProcessChoice, TEXT("D3DRenderDevice"), TEXT("bUsePostProcessEffects"));

	// Figure out which viewmode is currently running.
	EShowFlags ViewModeShowFlags = (GetGame()->GetLocalPlayer()->ViewportClient->ShowFlags&SHOW_ViewMode_Mask);
	if( ViewModeShowFlags == SHOW_ViewMode_Wireframe )
	{
		ViewmodeChoice->SetStringSelection(TEXT("Wireframe"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_BrushWireframe )
	{
		ViewmodeChoice->SetStringSelection(TEXT("BrushWireframe"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_Unlit)
	{
		ViewmodeChoice->SetStringSelection(TEXT("Unlit"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LightingOnly)
	{
		ViewmodeChoice->SetStringSelection(TEXT("LightingOnly"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LightComplexity)
	{
		ViewmodeChoice->SetStringSelection(TEXT("LightComplexity"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_TextureDensity)
	{
		ViewmodeChoice->SetStringSelection(TEXT("TextureDensity"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_ShaderComplexity)
	{
		ViewmodeChoice->SetStringSelection(TEXT("ShaderComplexity"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LightMapDensity)
	{
		ViewmodeChoice->SetStringSelection(TEXT("LightMapDensity"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_LitLightmapDensity)
	{
		ViewmodeChoice->SetStringSelection(TEXT("LitLightMapDensity"));
	}
	else if( ViewModeShowFlags == SHOW_ViewMode_Lit)
	{
		ViewmodeChoice->SetStringSelection(TEXT("Lit"));
	}
	else
	{
		ViewmodeChoice->SetStringSelection(TEXT("Lit"));
	}

	UpdateSlomoTextCtrl();
	UpdateFOVTextCtrl();
	UpdateDynamicShadowsCheckBox();
	UpdateShowHUDCheckBox();
	UpdatePlayersOnlyCheckBox();

	// Core stats.
	FramesPerSecondCheckBox->SetValue(GShowFpsCounter == TRUE);
	D3DSceneCheckBox->SetValue(FALSE);
#if STATS
	FStatGroup* MemGroup = GStatManager.GetGroup(STATGROUP_Memory);
	MemoryCheckBox->SetValue(MemGroup->bShowGroup == TRUE);
#endif
}

/**
 * Helper function for refreshing the value of DynamicShadows check box.
 */
void WxRemoteControlRenderPage::UpdateDynamicShadowsCheckBox()
{
	const EShowFlags ShowFlags = GetGame()->GetShowFlags();
	const UBOOL bShowDynamicShadows = (ShowFlags&SHOW_DynamicShadows) != 0;
	DynamicShadowsCheckBox->SetValue( bShowDynamicShadows == TRUE );
}

/**
 * Helper function for refreshing the value of ShowHUD check box.
 */
void WxRemoteControlRenderPage::UpdateShowHUDCheckBox()
{
	UBOOL bShowHUD = FALSE;

	ULocalPlayer* LocalPlayer = GetGame()->GetLocalPlayer();
	if ( LocalPlayer )
	{
		if ( LocalPlayer->Actor && LocalPlayer->Actor->myHUD )
		{
			bShowHUD = LocalPlayer->Actor->myHUD->bShowHUD;
		}
	}

	ShowHUDCheckBox->SetValue( bShowHUD == TRUE );
}

/**
* Helper function for refreshing the value of PlayersOnly check box.
*/
void WxRemoteControlRenderPage::UpdatePlayersOnlyCheckBox()
{
	const UBOOL bPlayersOnly = GetGame()->GetWorld()->GetWorldInfo()->bPlayersOnly;
	PlayersOnlyCheckBox->SetValue( bPlayersOnly == TRUE );
}

/**
 * Helper function for refreshing the value of Slomo text field.
 */
void WxRemoteControlRenderPage::UpdateSlomoTextCtrl()
{
	const FLOAT TimeDilation = GetGame()->GetWorld()->GetWorldInfo()->TimeDilation;
	const FString NewValue( FString::Printf(TEXT("%f"), TimeDilation) );
	SlomoTextCtrl->SetValue( *NewValue );
}

/**
 * Helper function for refreshing the value of FOV text field.
 */
void WxRemoteControlRenderPage::UpdateFOVTextCtrl()
{
	ULocalPlayer* LocalPlayer = GetGame()->GetLocalPlayer();
	if ( LocalPlayer )
	{
		AActor* Player = LocalPlayer->Actor;
		APlayerController* PC = Cast<APlayerController>( Player );
		if ( PC )
		{
			FLOAT FOV;
			if ( PC->PlayerCamera )
			{
				FOV = PC->PlayerCamera->LockedFOV;
			}
			else
			{
				FOV = PC->DesiredFOV;
			}

			const FString NewValue( FString::Printf(TEXT("%f"), FOV) );
			FOVTextCtrl->SetValue( *NewValue );
		}
	}	
}

/**
 * Event handler - called when the user changes the game resolution.
 */
void WxRemoteControlRenderPage::OnGameResolutionChoice(wxCommandEvent &In)
{
	const FScopedBusyCursor BusyCursor;

	// Extract the selected resolution.
	wxString str = GameResolutionChoice->GetStringSelection().GetData();
	
	unsigned long width, height;		
	UBOOL bFullscreen;

	if( str.CompareTo(TEXT("Custom...")) == 0 )
	{
		UINT Width, Height;
		UBOOL FullScreen;

		// query the game for the current display information
		GetGame()->GetDisplayInfo(Width, Height, FullScreen);
		width = Width;
		height = Height;
		bFullscreen = FullScreen;

		// create a dialog to get user input for the new resolution
		wxString resstr = TEXT("Resolution");
		wxDialog * pResolutionDialog = new wxDialog(this, 
													-1, 
													resstr,
													wxPoint(-1,-1),
													wxSize(250,150)
													);

		wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
		pResolutionDialog->SetSizer(itemBoxSizer2);

		wxStaticBox* itemStaticBoxSizer3Static = new wxStaticBox(pResolutionDialog, ID_REMOTECONTROL_RENDERPAGE_RESOLUTION_CONTROLS_SIZER, TEXT(""));
		wxStaticBoxSizer* itemStaticBoxSizer3 = new wxStaticBoxSizer(itemStaticBoxSizer3Static, wxVERTICAL);
		itemBoxSizer2->Add(itemStaticBoxSizer3, 0, wxGROW, 5);

		wxGridSizer* itemGridSizer4 = new wxGridSizer(2, 2, 0, 0);
		itemStaticBoxSizer3->Add(itemGridSizer4, 0, wxALIGN_LEFT|wxALL, 3);

		wxCheckBox* itemCheckBox6 = NULL;
		// do not put fullscreen checkbox if using editor
		if(!GetGame()->IsInEditor())
		{
			// create spacer
			wxStaticText* itemStaticText5 = new wxStaticText( pResolutionDialog, wxID_STATIC, TEXT(""), wxDefaultPosition, wxDefaultSize, 0 );
			itemGridSizer4->Add(itemStaticText5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

			// create checkbox for fullscreen
			itemCheckBox6 = new wxCheckBox( pResolutionDialog, ID_REMOTECONTROL_RENDERPAGE_RESOLUTION_FULLSCREENCHKBOX, TEXT("Fullscreen"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE );
			itemGridSizer4->Add(itemCheckBox6, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);
		}
		// create text input control for width entry
		wxTextCtrl* itemTextCtrl7 = new wxTextCtrl( pResolutionDialog, ID_REMOTECONTROL_RENDERPAGE_RESOLUTION_WIDTHCTRL, TEXT("Width"), wxDefaultPosition, wxDefaultSize, 0 );
		itemGridSizer4->Add(itemTextCtrl7, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		// create text input control for height entry
		wxTextCtrl* itemTextCtrl8 = new wxTextCtrl( pResolutionDialog, ID_REMOTECONTROL_RENDERPAGE_RESOLUTION_HEIGHTCTRL, TEXT("Height"), wxDefaultPosition, wxDefaultSize, 0 );
		itemGridSizer4->Add(itemTextCtrl8, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		wxButton* itemButton9 = new wxButton( pResolutionDialog, wxID_OK, TEXT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
		itemGridSizer4->Add(itemButton9, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		wxButton* itemButton10 = new wxButton( pResolutionDialog, wxID_CANCEL, TEXT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
		itemGridSizer4->Add(itemButton10, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

		// initialize fields with current values
		if(!GetGame()->IsInEditor())
		{
			check(itemCheckBox6);
			itemCheckBox6->SetValue(bFullscreen == TRUE);
		}
		wxString widthValue, heightValue;
		widthValue.Printf(TEXT("%i"),width);
		heightValue.Printf(TEXT("%i"),height);
		itemTextCtrl7->SetValue(widthValue);
		itemTextCtrl8->SetValue(heightValue);

		// Show the dialog and get the new resolution from the user.
		const int ReturnValue = pResolutionDialog->ShowModal();
		if( ReturnValue == wxID_OK )
		{
			if(!GetGame()->IsInEditor())
			{
				bFullscreen = itemCheckBox6->GetValue();
			}
			else
			{
				bFullscreen = FALSE;
			}
			widthValue = itemTextCtrl7->GetValue();
			heightValue = itemTextCtrl8->GetValue();
			
			// non-number values entered will set the variable to zero, which works out because in the setres
			// call later a zero width or height will pass through without attempting to set the resolution.
			widthValue.ToULong(&width);
			heightValue.ToULong(&height);
		}

		delete pResolutionDialog;
	}
	else
	{
		int i = str.Find(' ');
		check(i != -1);
		
		wxString token = str.Left((size_t)i);

			
		token.ToULong(&width);

		str = str.Mid((size_t)(i+1));
		check(str.GetChar(0) == 'x');
		// skip x and space
		str = str.Mid(2);
		
		i = str.Find(' ');
			
		if(i == -1)
		{
			// no fullscreen
			bFullscreen = FALSE;
			token = str;
		}
		else
		{
			// yes fullscreen
			token = str.Left((size_t)i);
			bFullscreen = TRUE;
		}
		token.ToULong(&height);
	}
	wxString StrCommand;
	StrCommand.Printf(TEXT("setres %dx%d%s"), width, height, bFullscreen?TEXT("f"):TEXT(""));

    
	GetGame()->ExecConsoleCommand( StrCommand.GetData() );
	GetGame()->RepositionRemoteControl();
}

/**
 * Event handler - called when user changes post process effects setting.
 */
void WxRemoteControlRenderPage::OnPostProcessChoice(wxCommandEvent &In)
{
	SetBooleanPropertyFromChoiceUI(*UsePostProcessChoice, TEXT("D3DRenderDevice"), TEXT("bUsePostProcessEffects"));
}

/**
 * Called when user changes the max texture size.
 */
void WxRemoteControlRenderPage::OnMaxTextureSizeChoice(wxCommandEvent &In)
{
	const FScopedBusyCursor BusyCursor;
	SetResolutionPropertyFromChoiceUI(*MaxTextureSizeChoice, TEXT("D3DRenderDevice"), TEXT("UserMaxTextureSize"));
	GetGame()->ExecConsoleCommand(TEXT("Flush"));
}

/**
 * Called when user changes the max shadow size.
 */
void WxRemoteControlRenderPage::OnMaxShadowSizeChoice(wxCommandEvent &In)
{
	SetResolutionPropertyFromChoiceUI(*MaxShadowSizeChoice, TEXT("D3DRenderDevice"), TEXT("MaxShadowResolution"));
}

/**
 * Called when user changes the viewmode.
 */
void WxRemoteControlRenderPage::OnViewmodeChoice(wxCommandEvent &In)
{
	const wxString Str = ViewmodeChoice->GetStringSelection();
	const FString StrCommand = FString::Printf(TEXT("viewmode %s"), Str.GetData());
	GetGame()->ExecConsoleCommand( *StrCommand );
}

/**
 * Called when user clicks on the "Open Map" button.
 */
void WxRemoteControlRenderPage::OnOpenLevel(wxCommandEvent &In)
{
	const FString MapExtension = GetGame()->GetMapExtension();
	const FString MapFilter = FString::Printf(TEXT("%s files (*.%s)|*.%s"), *(MapExtension.ToUpper()), *MapExtension, *MapExtension);

	// open a file dialog to get the level name from the user
	WxFileDialog FileDialog(this,
							TEXT("Choose a file"),
							TEXT(""),
							TEXT(""),
							*MapFilter,
							wxOPEN,
							wxDefaultPosition
							);

	const INT DialogResult = FileDialog.ShowModal();
	if( DialogResult == wxID_OK )
	{
		const wxString Filename = FileDialog.GetFilename();
		const FString StrCommand = FString::Printf(TEXT("open %s"), Filename.GetData());
		GetGame()->ExecConsoleCommand( *StrCommand );
	}
}

/**
 * Called when user clicks on the showflags drop-down arrow.
 */
void WxRemoteControlRenderPage::OnShowFlags(wxCommandEvent &In)
{
	wxMenu* menu = new wxMenu( TEXT("SHOWFLAGS"));

	menu->AppendCheckItem( ID_REMOTECONTROL_RENDERPAGE_SHOW_BONES, TEXT("Bones") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_BOUNDS, TEXT("Bounds") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_BSP, TEXT("BSP") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_BSPSPLIT, TEXT("BSP Split") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_COLLISION, TEXT("Collision"));
	menu->AppendCheckItem( ID_REMOTECONTROL_RENDERPAGE_SHOW_CONSTRAINTS, TEXT("Constraints") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_DECALS, TEXT("Decals") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_DECALINFO, TEXT("Decal Info") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_FOG, TEXT("Fog") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_HITPROXIES, TEXT("Hit Proxies") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_LEVELCOLORATION, TEXT("Level Coloration") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_MESHEDGES, TEXT("Mesh Edges") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_PARTICLES, TEXT("Particles") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_VERTEXCOLORS, TEXT("Vertex Colors")  );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_SCENE_CAPTURE_UPDATES, TEXT("Scene Capture Portals")  );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_SHADOWFRUSTUMS, TEXT("Shadow Frustums") );
	menu->AppendCheckItem( ID_REMOTECONTROL_RENDERPAGE_SHOW_SKELMESHES, TEXT("Skeletal Meshes") );
	menu->AppendCheckItem( ID_REMOTECONTROL_RENDERPAGE_SHOW_SKIN, TEXT("Skin") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_SPRITES, TEXT("Sprites") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_STATICMESHES, TEXT("Static Meshes") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_TERRAIN, TEXT("Terrain") );
	menu->AppendCheckItem( ID_REMOTECONTROL_RENDERPAGE_SHOW_TERRAINPATCHES, TEXT("Terrain Patches") );
	menu->AppendCheckItem( IDM_VIEWPORT_SHOW_CONSTRAINTS, TEXT("Constraints") );

	const EShowFlags ShowFlags = GetGame()->GetShowFlags();

	menu->Check( IDM_VIEWPORT_SHOW_STATICMESHES, (ShowFlags&SHOW_StaticMeshes) != 0 );
	menu->Check( ID_REMOTECONTROL_RENDERPAGE_SHOW_SKELMESHES, (ShowFlags&SHOW_SkeletalMeshes)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_TERRAIN, (ShowFlags&SHOW_Terrain)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_BSP, (ShowFlags&SHOW_BSP)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_BSPSPLIT, (ShowFlags&SHOW_BSPSplit)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_COLLISION, (ShowFlags&SHOW_Collision)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_BOUNDS, (ShowFlags&SHOW_Bounds)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_MESHEDGES, (ShowFlags&SHOW_MeshEdges)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_VERTEXCOLORS, (ShowFlags & SHOW_VertexColors)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_SCENE_CAPTURE_UPDATES, (ShowFlags & SHOW_SceneCaptureUpdates)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_HITPROXIES, (ShowFlags & SHOW_HitProxies)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_SHADOWFRUSTUMS, (ShowFlags & SHOW_ShadowFrustums)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_FOG, (ShowFlags & SHOW_Fog)!= 0 );
	menu->Check( IDM_VIEWPORT_SHOW_PARTICLES, (ShowFlags & SHOW_Particles)!= 0 );
	menu->Check( ID_REMOTECONTROL_RENDERPAGE_SHOW_CONSTRAINTS, (ShowFlags &SHOW_Constraints)!= 0  );
	menu->Check( ID_REMOTECONTROL_RENDERPAGE_SHOW_TERRAINPATCHES, (ShowFlags &SHOW_TerrainPatches )!= 0 );
    menu->Check( ID_REMOTECONTROL_RENDERPAGE_SHOW_BONES, bShowBones == TRUE );
    menu->Check( ID_REMOTECONTROL_RENDERPAGE_SHOW_SKIN, bShowSkin == TRUE );
	menu->Check( IDM_VIEWPORT_SHOW_DECALS, (ShowFlags&SHOW_Decals) != 0 );
	menu->Check( IDM_VIEWPORT_SHOW_DECALINFO, (ShowFlags&SHOW_DecalInfo) != 0 );
	menu->Check( IDM_VIEWPORT_SHOW_LEVELCOLORATION, (ShowFlags&SHOW_LevelColoration) != 0 );
	menu->Check( IDM_VIEWPORT_SHOW_SPRITES, (ShowFlags&SHOW_Sprites) != 0 );
	menu->Check( IDM_VIEWPORT_SHOW_CONSTRAINTS, (ShowFlags&SHOW_Constraints) != 0 );

	FTrackPopupMenu tpm( this, menu );
	tpm.Show();
	delete menu;
}

/**
 * Called when a user clicks on stat FPS.
 */
void WxRemoteControlRenderPage::OnFPSClicked(wxCommandEvent &In)
{
	GShowFpsCounter ^= TRUE;
	FramesPerSecondCheckBox->SetValue( GShowFpsCounter == TRUE );
}

/**
 * Called when a user clicks on stat D3DScene.
 */
void WxRemoteControlRenderPage::OnD3DSceneClicked(wxCommandEvent &In)
{
	D3DSceneCheckBox->SetValue(FALSE);
}

/**
 * Called when a user clicks on stat Memory.
 */
void WxRemoteControlRenderPage::OnMemoryClicked(wxCommandEvent &In)
{
	GetGame()->ToggleStat(TEXT("Memory"));
#if STATS
	FStatGroup* MemGroup = GStatManager.GetGroup(STATGROUP_Memory);
	MemoryCheckBox->SetValue(MemGroup->bShowGroup == TRUE);
#endif
}

/**
 * Called when a user presses enter in the Slomo text field.
 */
void WxRemoteControlRenderPage::OnApplySlomo(wxCommandEvent &In)
{
	const FString StrCommand( FString::Printf(TEXT("SLOMO %f"), appAtof( SlomoTextCtrl->GetValue() ) ) );
	GetGame()->ExecConsoleCommand( *StrCommand );
	UpdateSlomoTextCtrl();
}

/**
 * Called when a user presses enter in the FOV text field.
 */
void WxRemoteControlRenderPage::OnApplyFOV(wxCommandEvent &In)
{
	const FString StrCommand( FString::Printf(TEXT("FOV %f"), appAtof( FOVTextCtrl->GetValue() ) ) );
	GetGame()->ExecConsoleCommand( *StrCommand );
	UpdateFOVTextCtrl();
}

/**
 * Called when a user clicks on the DynamicShadows check box.
 */
void WxRemoteControlRenderPage::OnDynamicShadows(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("SHOW DYNAMICSHADOWS"));
	UpdateDynamicShadowsCheckBox();
}

/**
 * Called when a user clicks on the ShowHUD check box.
 */
void WxRemoteControlRenderPage::OnShowHUD(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("TOGGLEHUD"));
	UpdateShowHUDCheckBox();
}

/**
 * Called when a user clicks on the PlayersOnly check box.
 */
void WxRemoteControlRenderPage::OnPlayersOnly(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("PLAYERSONLY"));
	UpdatePlayersOnlyCheckBox();
}

/**
    toggle show flag - static meshes
*/
void WxRemoteControlRenderPage::OnShowStaticMeshes(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show STATICMESHES"));
}

/**
    toggle show flag - terrain
*/
void WxRemoteControlRenderPage::OnShowTerrain(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show TERRAIN"));
}

/**
    toggle show flag - bsp
*/
void WxRemoteControlRenderPage::OnShowBSP(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show BSP"));
}

/**
toggle show flag - bsp split
*/
void WxRemoteControlRenderPage::OnShowBSPSplit(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show BSPSPLIT"));
}

/**
    toggle show flag - collision
*/
void WxRemoteControlRenderPage::OnShowCollision(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show COLLISION"));
}

/**
    toggle show flag - bounds
*/
void WxRemoteControlRenderPage::OnShowBounds(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show BOUNDS"));
}

/**
    toggle show flag - mesh edges
*/
void WxRemoteControlRenderPage::OnShowMeshEdges(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show MESHEDGES"));
}

/**
    toggle show flag - vertex colors
*/
void WxRemoteControlRenderPage::OnShowVertexColors(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show VERTEXCOLORS"));
}


/**
toggle show flag - scene capture updates
*/
void WxRemoteControlRenderPage::OnShowSceneCaptureUpdates (wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show SCENECAPTURE"));
}


/**
    toggle show flag - zone colors
*/
void WxRemoteControlRenderPage::OnShowShadowFrustums(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show SHADOWFRUSTUMS"));
}

/**
    toggle show flag - hit proxies
*/
void WxRemoteControlRenderPage::OnShowHitProxies(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show HITPROXIES"));
}

/**
    toggle show flag - fog
*/
void WxRemoteControlRenderPage::OnShowFog(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show FOG"));
}


/**
    toggle show flag - mesh particles
*/
void WxRemoteControlRenderPage::OnShowParticles(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show PARTICLES"));
}


/**
    toggle show flag - constraints
*/
void WxRemoteControlRenderPage::OnShowConstraints(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show CONSTRAINTS"));
}

/**
    toggle show flag - terrain patches
*/
void WxRemoteControlRenderPage::OnShowTerrainPatches(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show TERRAINPATCHES"));
}

/**
    toggle show flag - skeletal meshes
*/
void WxRemoteControlRenderPage::OnShowSkeletalMeshes(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show SKELMESHES"));
}

/**
 * Toggle show bones.
 */
void WxRemoteControlRenderPage::OnShowBones(wxCommandEvent &In)
{
	bShowBones ^= TRUE;

    for( TObjectIterator<UObject> It ; It ; ++It )
	{
		UObject *Object = *It;
        USkeletalMeshComponent *SMComponent = Cast<USkeletalMeshComponent>( Object );
        if ( SMComponent )
		{
            SMComponent->bDisplayBones = bShowBones;
		}
    }
}

/**
 * Toggle show skin.
 */
void WxRemoteControlRenderPage::OnShowSkin(wxCommandEvent &In)
{
	bShowSkin ^= TRUE;

    for( TObjectIterator<UObject> It ; It ; ++It )
	{
		UObject *Object = *It;
        USkeletalMeshComponent *SMComponent = Cast<USkeletalMeshComponent>( Object );
        if ( SMComponent )
		{
            SMComponent->bHideSkin = !bShowSkin;
		}
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WxRemoteControlRenderPage::OnShowDecals(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show DECALS"));
}

void WxRemoteControlRenderPage::OnShowDecalInfo(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show DECALINFO"));
}

void WxRemoteControlRenderPage::OnShowLevelColoration(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show LEVELCOLORATION"));
}

void WxRemoteControlRenderPage::OnShowSprites(wxCommandEvent &In)
{
	GetGame()->ExecConsoleCommand(TEXT("show SPRITES"));
}
