/*=============================================================================
	GameStatsVisualizer.cpp: Browser window for working with game stats files
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnrealEdGameStatsClasses.h"
#include "GameStatsBrowser.h"
#include "GameStatsDatabaseTypes.h"

IMPLEMENT_CLASS(UGameStatsVisualizer);
IMPLEMENT_CLASS(UBasicStatsVisualizer);
IMPLEMENT_CLASS(UPlayerMovementVisualizer);
IMPLEMENT_CLASS(UHeatmapVisualizer);
IMPLEMENT_CLASS(UPerformanceVisualizer);
IMPLEMENT_CLASS(UGenericParamlistVisualizer);


/*-----------------------------------------------------------------------------
WxVisualizerOptionsDialog.
-----------------------------------------------------------------------------*/



BEGIN_EVENT_TABLE(WxVisualizerOptionsDialog, wxDialog)
	EVT_CLOSE( WxVisualizerOptionsDialog::OnClose )
	EVT_BUTTON( wxID_OK, WxVisualizerOptionsDialog::OnOK )
END_EVENT_TABLE()

WxVisualizerOptionsDialog::~WxVisualizerOptionsDialog() 
{
	//Clear the visualizer's pointer so BeginDestroy doesn't try to delete also
	if (Visualizer && Visualizer->OptionsDialog == this)
	{
		Visualizer->OptionsDialog = NULL;
	}
}

bool WxVisualizerOptionsDialog::Create(UGameStatsVisualizer* InVisualizer)
{
	Visualizer = InVisualizer;
	if (Visualizer != NULL)
	{
		//Generate a title bar localization string for the dialog
		FString VisualizerTitleBarStr = Visualizer->GetClass()->GetName();
		const INT DotPos = VisualizerTitleBarStr.InStr( TEXT(".") );
		if (DotPos != -1)
		{
			VisualizerTitleBarStr = VisualizerTitleBarStr.Right( (VisualizerTitleBarStr.Len() - DotPos) - 1);
		}

		VisualizerTitleBarStr += TEXT("_TitleBar");

		if (wxDialog::Create(Visualizer->Parent, wxID_ANY, wxString(*VisualizerTitleBarStr), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE, wxDialogNameStr))
		{

			// Load the window hierarchy from the .xrc file
			wxPanel* OptionsPanel = wxXmlResource::Get()->LoadPanel(this, *(Visualizer->OptionsDialogName));
			if (OptionsPanel)
			{
				INT Width = 0;
				INT Height = 0;
				OptionsPanel->GetSize(&Width, &Height);

				wxBoxSizer* TopmostSizer = new wxBoxSizer(wxVERTICAL);
				if (TopmostSizer)
				{		 
					SetSizer(TopmostSizer);
					wxSizer* ButtonSizer = CreateButtonSizer(wxOK);

					//Add an OK button and the custom panel to the dialog
					TopmostSizer->Add(OptionsPanel, 1, wxEXPAND|wxALIGN_CENTER|wxALL, 5);
					TopmostSizer->Add(ButtonSizer, 0, wxALIGN_CENTER|wxALL, 5);

					TopmostSizer->SetMinSize(Width, Height);
					TopmostSizer->Fit(this);
				}
			}

			// Localize all of the static text in the loaded window
			FLocalizeWindow( this, FALSE, TRUE );
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Callback when the OK button is clicked, shuts down the options panel
 * @param In - Properties of the event triggered
 */
void WxVisualizerOptionsDialog::OnOK( wxCommandEvent& In )
{
	//We hide because the stats browser has logic for calling destroy appropriately
	Hide();
}

/**
 * Callback when the window is closed, shuts down the options panel
 * @param In - Properties of the event triggered
 */
void WxVisualizerOptionsDialog::OnClose(wxCloseEvent& In)
{
	//We hide because the stats browser has logic for calling destroy appropriately
	Hide();
}


BEGIN_EVENT_TABLE(WxHeatmapOptionsDialog, WxVisualizerOptionsDialog)
END_EVENT_TABLE()

bool WxHeatmapOptionsDialog::Create(UGameStatsVisualizer* InVisualizer)
{
	bool bSuccess = WxVisualizerOptionsDialog::Create(InVisualizer);
	if (bSuccess)
	{
		UHeatmapVisualizer* HeatmapVis = Cast<UHeatmapVisualizer>(Visualizer);

		wxSlider* MaxDensitySlider = static_cast< wxSlider* >(
			FindWindow( TEXT( "ID_MAXDENSITY" ) ) );
		check( MaxDensitySlider != NULL );

		MaxDensitySlider->SetRange(HeatmapVis->MinDensity + 1, HeatmapVis->MaxDensity);
		MaxDensitySlider->SetValue(HeatmapVis->CurrentMaxDensity);

		wxSlider* MinDensitySlider = static_cast< wxSlider* >(
			FindWindow( TEXT( "ID_MINDENSITY" ) ) );
		check( MinDensitySlider != NULL );

		MinDensitySlider->SetRange(HeatmapVis->MinDensity, HeatmapVis->CurrentMaxDensity - 1);
		MinDensitySlider->SetValue(HeatmapVis->CurrentMinDensity);

		wxSlider* HeatRadiusSlider = static_cast< wxSlider* >(
			FindWindow( TEXT( "ID_HEATRADIUS" ) ) );
		check( HeatRadiusSlider != NULL );
		HeatRadiusSlider->SetValue(HeatmapVis->HeatRadius);

		wxSlider* PixelDensitySlider = static_cast< wxSlider* >(
			FindWindow( TEXT( "ID_PIXELDENSITY" ) ) );
		check( PixelDensitySlider != NULL );
		PixelDensitySlider->SetValue(HeatmapVis->NumUnrealUnitsPerPixel);

		#undef AddWxEventHandler
		#undef AddWxScrollEventHandler
		#define AddWxEventHandler( id, event, func )\
		{\
			wxEvtHandler* eh = GetEventHandler();\
			eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxCommandEventFunction)(func) );\
		}

		#define AddWxScrollEventHandler( id, event, func )\
		{\
			wxEvtHandler* eh = GetEventHandler();\
			eh->Connect( id, event, (wxObjectEventFunction)(wxEventFunction)(wxScrollEventFunction)(func) );\
		}

		AddWxScrollEventHandler( XRCID( "ID_MINDENSITY" ), wxEVT_SCROLL_CHANGED, &WxHeatmapOptionsDialog::OnMinDensityFilterUpdated );
		AddWxScrollEventHandler( XRCID( "ID_MAXDENSITY" ), wxEVT_SCROLL_CHANGED, &WxHeatmapOptionsDialog::OnMaxDensityFilterUpdated );
		AddWxScrollEventHandler( XRCID( "ID_HEATRADIUS" ), wxEVT_SCROLL_CHANGED, &WxHeatmapOptionsDialog::OnHeatRadiusUpdated );
		AddWxScrollEventHandler( XRCID( "ID_PIXELDENSITY" ), wxEVT_SCROLL_CHANGED, &WxHeatmapOptionsDialog::OnPixelDensityUpdated );

		AddWxScrollEventHandler( XRCID( "ID_MINDENSITY" ), wxEVT_SCROLL_THUMBTRACK, &WxHeatmapOptionsDialog::OnMinDensityFilterUpdated );
		AddWxScrollEventHandler( XRCID( "ID_MAXDENSITY" ), wxEVT_SCROLL_THUMBTRACK, &WxHeatmapOptionsDialog::OnMaxDensityFilterUpdated );
		AddWxScrollEventHandler( XRCID( "ID_HEATRADIUS" ), wxEVT_SCROLL_THUMBTRACK, &WxHeatmapOptionsDialog::OnHeatRadiusUpdated );
		AddWxScrollEventHandler( XRCID( "ID_PIXELDENSITY" ), wxEVT_SCROLL_THUMBTRACK, &WxHeatmapOptionsDialog::OnPixelDensityUpdated );

		AddWxEventHandler( XRCID( "ID_HEATMAPSCREENSHOT" ), wxEVT_COMMAND_BUTTON_CLICKED, &WxHeatmapOptionsDialog::OnScreenshotClicked );
	}

	return bSuccess;
}

/**
 * Callback when the min density slider is manipulated, regenerates the heatmap texture with new scaling
 * @param In - Properties of the event triggered
 */
void WxHeatmapOptionsDialog::OnMinDensityFilterUpdated( wxScrollEvent& In )
{
	wxSlider* MinDensitySlider = static_cast< wxSlider* >(
		FindWindow( TEXT( "ID_MINDENSITY" ) ) );
	check( MinDensitySlider != NULL );

	UHeatmapVisualizer* HeatmapVis = Cast<UHeatmapVisualizer>(Visualizer);

	//Regenerate the heatmap
	HeatmapVis->UpdateDensityMapping(MinDensitySlider->GetValue(), HeatmapVis->CurrentMaxDensity);
	HeatmapVis->CreateHeatmapTexture();

	//Redraw the viewports
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/**
 * Callback when the max density slider is manipulated, regenerates the heatmap texture with new scaling
 * @param In - Properties of the event triggered
 */
void WxHeatmapOptionsDialog::OnMaxDensityFilterUpdated( wxScrollEvent& In )
{
	wxSlider* MaxDensitySlider = static_cast< wxSlider* >(
		FindWindow( TEXT( "ID_MAXDENSITY" ) ) );
	check( MaxDensitySlider != NULL );

	UHeatmapVisualizer* HeatmapVis = Cast<UHeatmapVisualizer>(Visualizer);

	//Regenerate the heatmap
	HeatmapVis->UpdateDensityMapping(HeatmapVis->CurrentMinDensity, MaxDensitySlider->GetValue());
	HeatmapVis->CreateHeatmapTexture();

	//Redraw the viewports
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/**
 * Callback when the heat radius slider is manipulated, regenerates the heatmap texture with new scaling
 * @param In - Properties of the event triggered
 */
void WxHeatmapOptionsDialog::OnHeatRadiusUpdated( wxScrollEvent& In )
{
	wxSlider* HeatRadiusSlider = static_cast< wxSlider* >(
		FindWindow( TEXT( "ID_HEATRADIUS" ) ) );
	check( HeatRadiusSlider != NULL );

	UHeatmapVisualizer* HeatmapVis = Cast<UHeatmapVisualizer>(Visualizer);
	HeatmapVis->HeatRadius = HeatRadiusSlider->GetValue();

	//Regenerate the heatmap
	HeatmapVis->CreateHeatmapGrid();
	HeatmapVis->UpdateDensityMapping(HeatmapVis->CurrentMinDensity, HeatmapVis->CurrentMaxDensity);
	HeatmapVis->CreateHeatmapTexture();

	//Redraw the viewports
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/**
 * Callback when the pixel density slider is manipulated, regenerates the heatmap texture with new scaling
 * @param In - Properties of the event triggered
 */
void WxHeatmapOptionsDialog::OnPixelDensityUpdated( wxScrollEvent& In )
{
	wxSlider* PixelDensitySlider = static_cast< wxSlider* >(
		FindWindow( TEXT( "ID_PIXELDENSITY" ) ) );
	check( PixelDensitySlider != NULL );

	UHeatmapVisualizer* HeatmapVis = Cast<UHeatmapVisualizer>(Visualizer);
	
	// Remove old padding to the texture
	const FVector OldWorldOffset(5.0f*HeatmapVis->NumUnrealUnitsPerPixel, 5.0f*HeatmapVis->NumUnrealUnitsPerPixel, 0.f);
	HeatmapVis->WorldMinPos -= (OldWorldOffset * -1.f);
	HeatmapVis->WorldMaxPos -= OldWorldOffset;

	// Add new padding to the texture
	HeatmapVis->NumUnrealUnitsPerPixel = PixelDensitySlider->GetValue();

	const FVector NewWorldOffset(5.0f*HeatmapVis->NumUnrealUnitsPerPixel, 5.0f*HeatmapVis->NumUnrealUnitsPerPixel, 0.f);
	HeatmapVis->WorldMinPos += (NewWorldOffset * -1.f);
	HeatmapVis->WorldMaxPos += NewWorldOffset;

	//Regenerate the heatmap
	HeatmapVis->CreateHeatmapGrid();
	HeatmapVis->UpdateDensityMapping(HeatmapVis->CurrentMinDensity, HeatmapVis->CurrentMaxDensity);
	HeatmapVis->CreateHeatmapTexture();

	//Redraw the viewports
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}

/**
* Callback when user pushes the take screenshot button
* @param In - Properties of the event triggered
*/
void WxHeatmapOptionsDialog::OnScreenshotClicked(wxCommandEvent& In)
{		
	//Right now this code assumes the upper left (first) viewport is the one with the topdown heatmap
	//The GIsTiledScreenshot code turns the flag off after the first viewport drawing to get to it
	//@TODO - need to make sure the viewport is the correct one, but it breaks certain abstractions 
	FlushRenderingCommands();
	GIsTiledScreenshot = TRUE;
	GScreenshotResolutionMultiplier = 2;
	GScreenshotResolutionMultiplier = Clamp<INT>( GScreenshotResolutionMultiplier, 2, 128 );
	GScreenshotMargin = Clamp<INT>(64, 0, 320);

	//Trigger a render call
	GCallbackEvent->Send( CALLBACK_RedrawAllViewports );
}


#define NUM_GAME_EVENT_COLORS (32)
static const FColor GameEventColors[NUM_GAME_EVENT_COLORS] = 
{
	FColor(255, 0, 0),		//red
	FColor(0, 0, 255),		//blue
	FColor(0, 255, 0),		//green
	FColor(139, 0, 255),	//violet
	FColor(255, 127, 0),	//orange
	FColor(255, 223, 0),	//golden yellow
	FColor(175, 175, 175),	//light grey
	FColor(148, 0, 211),	//dark purple
	FColor(153, 101, 21),	//golden brown
	FColor(248, 131, 121),	//coral
	FColor(0, 123, 167),	//cerulean
	FColor(75, 83, 32),		//army green
	FColor(245, 222, 179),	//wheat
	FColor(204, 85, 0),		//burnt orange
	FColor(0, 128, 128),	//teal
	FColor(154, 205, 50),	//yellow green
	FColor(70, 70, 70),		//grey
	FColor(153, 102, 204),	//Amethyst
	FColor(123, 63, 0),		//chocolate
	FColor(146, 0, 10),		//sangria
	FColor(175, 238, 238),	//pale blue
	FColor(120, 134, 107),	//camouflage green
	FColor(255, 117, 24),	//pumpkin
	FColor(251, 206, 177),	//apricot
	FColor(170, 152, 169),	//rose quartz
	FColor(0, 0, 139),		//dark blue
	FColor(191, 0, 255),	//electric purple
    FColor(175, 64, 53),	//medium carmine
	FColor(220,  20,  60),	//crimson
	FColor(0, 71, 171),		//cobalt
	FColor(127, 255, 0),	//chartreuse
	FColor(0, 255, 255)		//Aqua
};

FColor GetColorByPlayerIndex(const INT PlayerIndex)
{
	return GameEventColors[PlayerIndex % NUM_GAME_EVENT_COLORS]; 
}

/**
 * Given a chance to initialize 
 * Iterates over the draw properties array to associate textures with sprite names
 */
void UBasicStatsVisualizer::Init()
{
	for (INT DrawIdx = 0; DrawIdx < DrawingProperties.Num(); DrawIdx++)
	{
		/*const*/ FStatDrawingProperties& DrawProp = DrawingProperties(DrawIdx);
		if (DrawProp.StatSprite == NULL && DrawProp.SpriteName.Len() > 0)
		{
			// Try to load the specified class
			DrawProp.StatSprite = LoadObject<UTexture2D>(NULL, *DrawProp.SpriteName, NULL, LOAD_None, NULL);
		}
	}
}

/** Reset the visualizer to initial state */
void UBasicStatsVisualizer::Reset() 
{
	BasicEntries.Empty();
	PlayerEntries.Empty();
	PlayerPlayerEntries.Empty();
	PlayerTargetEntries.Empty();
}

/** Called before any database entries are given to the visualizer */
void UBasicStatsVisualizer::BeginVisiting()
{
	Reset();
}

/** Called at the end of database entry traversal, returns success or failure */
UBOOL UBasicStatsVisualizer::EndVisiting()
{
	//Did we keep any of the data we saw?
	return GetVisualizationSetCount() > 0 ? TRUE : FALSE;
}

/** Returns the number of data points the visualizer is actively working with */
INT UBasicStatsVisualizer::GetVisualizationSetCount() const
{
	 return BasicEntries.Num() + PlayerEntries.Num() + PlayerPlayerEntries.Num() + PlayerTargetEntries.Num();
}

/** 
 *	Retrieve some metadata about an event
 * @param EventIndex - some visualizer relative index about the data to get metadata about
 * @param MetadataString - return string containing information about the event requested
 */
void UBasicStatsVisualizer::GetMetadata(INT EventIndex, FString& MetadataString)
{						 
	MetadataString.Empty();

	//Figure out what we hit in the proxy
	INT NumBasicEvents = BasicEntries.Num();
	INT NumPlayerEvents = PlayerEntries.Num();
	INT NumPlayerPlayerEvents = PlayerPlayerEntries.Num();
	INT NumPlayerTargetEvents = PlayerTargetEntries.Num();

	//Index is just addition of previous array sizes to create a unique index
	//so we have to unroll it
	FVector EventPosition;
	FRotator EventRotation;
	if (EventIndex < NumBasicEvents)
	{
		const FBasicStatEntry& Entry = BasicEntries(EventIndex);
		MetadataString = FString::Printf(TEXT("Event[%d]: %s\nTime:%.2f"), Entry.EventID, *Entry.EventName, Entry.EventTime);
	}
	else if (EventIndex < NumBasicEvents + NumPlayerEvents)
	{
		const FPlayerEntry& Entry = PlayerEntries(EventIndex - NumBasicEvents);
		MetadataString = FString::Printf(TEXT("Event[%d]: %s\nTime:%.2f\nPlayer:%s"), Entry.EventID, *Entry.EventName, Entry.EventTime, *Entry.PlayerName);
		if (Entry.WeaponName.Len() > 0)
		{
			MetadataString += FString::Printf(TEXT("\nWeapon:%s"), *Entry.WeaponName);
		}
	}
	else if (EventIndex < NumBasicEvents + NumPlayerEvents + NumPlayerPlayerEvents)
	{
		const FPlayerPlayerEntry& Entry = PlayerPlayerEntries(EventIndex - NumBasicEvents - NumPlayerEvents);
		MetadataString = FString::Printf(TEXT("Event[%d]: %s\nTime:%.2f\nPlayer1:%s\nPlayer2:%s"), Entry.EventID, *Entry.EventName, Entry.EventTime, *Entry.Player1Name, *Entry.Player2Name);
	}
	else if (EventIndex < NumBasicEvents + NumPlayerEvents + NumPlayerPlayerEvents + NumPlayerTargetEvents)
	{
		const FPlayerTargetEntry& Entry = PlayerTargetEntries(EventIndex - NumBasicEvents - NumPlayerEvents - NumPlayerPlayerEvents);
		MetadataString = FString::Printf(TEXT("Event[%d]: %s\nTime:%.2f\nPlayer:%s\nTarget:%s\nDamageType: %s"), Entry.EventID, *Entry.EventName, Entry.EventTime, *Entry.PlayerName, *Entry.TargetName, *Entry.DamageType);
		if (Entry.KillType.Len() > 0)
		{
			MetadataString += FString::Printf(TEXT("\nKillType: %s"), *Entry.KillType);
		}
	}
}

/** Called when a hitproxy belonging to this visualizer is triggered */
void UBasicStatsVisualizer::HandleHitProxy(HGameStatsHitProxy* HitProxy)
{
	//Figure out what we hit in the proxy
	INT NumBasicEvents = BasicEntries.Num();
	INT NumPlayerEvents = PlayerEntries.Num();
	INT NumPlayerPlayerEvents = PlayerPlayerEntries.Num();
			 
	//Index is just addition of previous array sizes to create a unique index
	//so we have to unroll it
	FVector EventPosition;
	FRotator EventRotation;
	if (HitProxy->StatIndex < NumBasicEvents)
	{
		const FBasicStatEntry& Entry = BasicEntries(HitProxy->StatIndex);
		EventPosition = Entry.Location;
		EventRotation = Entry.Rotation;
	}
	else if (HitProxy->StatIndex < NumBasicEvents + NumPlayerEvents)
	{
		const FPlayerEntry& Entry = PlayerEntries(HitProxy->StatIndex - NumBasicEvents);
		EventPosition = Entry.Location;
		EventRotation = Entry.Rotation;
	}
	else if (HitProxy->StatIndex < NumBasicEvents + NumPlayerEvents + NumPlayerPlayerEvents)
	{
		const FPlayerPlayerEntry& Entry = PlayerPlayerEntries(HitProxy->StatIndex - NumBasicEvents - NumPlayerEvents);
		EventPosition = Entry.Location;
		EventRotation = Entry.Rotation;
	}
	else
	{
		const FPlayerTargetEntry& Entry = PlayerTargetEntries(HitProxy->StatIndex - NumBasicEvents - NumPlayerEvents - NumPlayerPlayerEvents);
		EventPosition = Entry.Location;
		EventRotation = Entry.Rotation;
	}

	InvalidateViewportsForEvent(EventPosition,EventRotation);
}

/**
 * Invalidates all viewports for the specified location/rotation
 *
 * @param Location the location to invalidate
 * @param Rotation the rotation to invalidate
 */
void UGameStatsVisualizer::InvalidateViewportsForEvent(const FVector& Location,const FRotator& Rotation)
{
	// Iterate over each viewport, updating view if its perspective.
	INT ViewportCount = GApp->EditorFrame->ViewportConfigData->GetViewportCount();
	for(INT i=0; i<ViewportCount; i++)
	{
		FEditorLevelViewportClient* LevelVC = GApp->EditorFrame->ViewportConfigData->AccessViewport(i).ViewportWindow;
		if(LevelVC && LevelVC->ViewportType == LVT_Perspective)
		{
			LevelVC->ViewLocation = Location;
			LevelVC->ViewRotation = Rotation;
			LevelVC->Viewport->Invalidate();
		}
	}
}

/** 
 * Visualizes all stats in a very basic way (sprite at a location with an orientation arrow typically)
 * @param View - the view being drawn in
 * @param PDI - draw interface for primitives
 * @param ViewportType - type of viewport being draw (perspective, ortho)
 */
void UBasicStatsVisualizer::Visualize(const FSceneView* View, FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType)
{
	BYTE DepthPriority = (ViewportType == LVT_Perspective) ? SDPG_World : SDPG_Foreground;

	//Sprite size scale divisor done once outside of all for-loops
	FLOAT ScaleDivisor = ( 4 / (FLOAT)View->SizeX / View->ProjectionMatrix.M[0][0] );

	//Real basic entries (colored sprite at position)
	INT NumBasicEvents = BasicEntries.Num();
	for (INT EventIdx = 0; EventIdx < NumBasicEvents; EventIdx++)
	{
		const FBasicStatEntry& Entry = BasicEntries(EventIdx);

		const FStatDrawingProperties& DrawingProps = GetDrawingProperties(Entry.EventID);
		const FLinearColor SpriteColor(DrawingProps.StatColor);
		const FLOAT SpriteSize = DrawingProps.Size;
		const FPlane ProjPoint = View->Project( Entry.Location );
		if (ProjPoint.W > 0.0f)
		{
			const FLOAT Scale = ProjPoint.W * ScaleDivisor;

			PDI->SetHitProxy( new HGameStatsHitProxy(this, EventIdx) );
			PDI->DrawSprite( Entry.Location, SpriteSize * Scale, SpriteSize * Scale, DrawingProps.StatSprite->Resource, SpriteColor, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}
	}

	//Real basic player entries (colored sprite with orientation at a position)
	INT NumPlayerEvents = PlayerEntries.Num();
	for (INT EventIdx = 0; EventIdx < NumPlayerEvents; EventIdx++)
	{
		const FPlayerEntry& Entry = PlayerEntries(EventIdx);

		const FStatDrawingProperties& DrawingProps = GetDrawingProperties(Entry.EventID);

		const FLOAT SpriteSize = DrawingProps.Size;
		const FPlane PlayerProjPoint = View->Project( Entry.Location );
		if (PlayerProjPoint.W > 0.0f)
		{
			const FLOAT Scale = PlayerProjPoint.W * ScaleDivisor;

			const FColor PlayerColor = GetColorByPlayerIndex(Entry.PlayerIndex);

			PDI->SetHitProxy( new HGameStatsHitProxy(this, NumBasicEvents + EventIdx) );
			PDI->DrawSprite( Entry.Location, SpriteSize * Scale, SpriteSize * Scale, DrawingProps.StatSprite->Resource, PlayerColor, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );

			FScaleRotationTranslationMatrix ArrowToWorld(FVector(1.0f, 1.0f, 1.0f), Entry.Rotation, Entry.Location);
			FLOAT ArrowLength = 40.0f;
			FLOAT ArrowSize = 3.0f;
			DrawDirectionalArrow(PDI, ArrowToWorld, PlayerColor, ArrowLength, ArrowSize, DepthPriority);
		}
	}

	//Player player entries draw one colored sprite at the location of interaction with player orientations drawn as arrows
	INT NumPlayerPlayerEvents = PlayerPlayerEntries.Num();
	for (INT EventIdx = 0; EventIdx < NumPlayerPlayerEvents; EventIdx++)
	{
		const FPlayerPlayerEntry& Entry = PlayerPlayerEntries(EventIdx);

		const FStatDrawingProperties& DrawingProps = GetDrawingProperties(Entry.EventID);
		const FLOAT SpriteSize = DrawingProps.Size;

		const FColor Player1Color = GetColorByPlayerIndex(Entry.Player1Index);
		const FPlane Player1ProjPoint = View->Project( Entry.Location );
		if (Player1ProjPoint.W > 0.0f)
		{
			const FLOAT Scale = Player1ProjPoint.W * ScaleDivisor;

			PDI->SetHitProxy( new HGameStatsHitProxy(this, NumBasicEvents + NumPlayerEvents + EventIdx) );
			PDI->DrawSprite( Entry.Location, SpriteSize * Scale, SpriteSize * Scale, DrawingProps.StatSprite->Resource, Player1Color, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}

		const FColor Player2Color = GetColorByPlayerIndex(Entry.Player2Index);
		const FPlane Player2ProjPoint = View->Project( Entry.Player2Location );
		if (Player2ProjPoint.W > 0.0f)
		{
			const FLOAT Scale = Player2ProjPoint.W * ScaleDivisor;

			PDI->SetHitProxy( new HGameStatsHitProxy(this, NumBasicEvents + NumPlayerEvents + EventIdx) );
			PDI->DrawSprite( Entry.Player2Location, SpriteSize * Scale, SpriteSize * Scale, DrawingProps.StatSprite->Resource, Player2Color, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}

		const FLOAT ArrowLength = 40.0f;
		const FLOAT ArrowSize = 3.0f;

		FScaleRotationTranslationMatrix Player1Orientation(FVector(1.0f, 1.0f, 1.0f), Entry.Rotation, Entry.Location);
		DrawDirectionalArrow(PDI, Player1Orientation, Player1Color, ArrowLength, ArrowSize, DepthPriority);

		FScaleRotationTranslationMatrix Player2Orientation(FVector(1.0f, 1.0f, 1.0f), Entry.Rotation2, Entry.Player2Location);
		DrawDirectionalArrow(PDI, Player2Orientation, Player2Color, ArrowLength, ArrowSize, DepthPriority);
	}

	//Player target entries draw one colored sprite at each player position with an arrow indicating player->target relationship
	INT NumPlayerTargetEvents = PlayerTargetEntries.Num();
	for (INT EventIdx = 0; EventIdx < NumPlayerTargetEvents; EventIdx++)
	{
		const FPlayerTargetEntry& Entry = PlayerTargetEntries(EventIdx);

		const FStatDrawingProperties& DrawingProps = GetDrawingProperties(Entry.EventID);

		const FColor PlayerColor = GetColorByPlayerIndex(Entry.PlayerIndex);
		const FLOAT SpriteSize = DrawingProps.Size;
		const FPlane PlayerProjPoint = View->Project( Entry.Location );
		if (PlayerProjPoint.W > 0.0f)
		{
			const FLOAT Scale = PlayerProjPoint.W * ScaleDivisor;

			PDI->SetHitProxy( new HGameStatsHitProxy(this, NumBasicEvents + NumPlayerEvents + NumPlayerPlayerEvents + EventIdx) );
			PDI->DrawSprite( Entry.Location, SpriteSize * Scale, SpriteSize * Scale, DrawingProps.StatSprite->Resource, PlayerColor, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}

		const FPlane TargetProjPoint = View->Project( Entry.TargetLocation );
		if (TargetProjPoint.W > 0.0f)
		{
			const FLOAT Scale = TargetProjPoint.W * ScaleDivisor;
			const FColor TargetColor = GetColorByPlayerIndex(Entry.TargetIndex);

			PDI->SetHitProxy( new HGameStatsHitProxy(this, NumBasicEvents + NumPlayerEvents + NumPlayerPlayerEvents + EventIdx) );
			PDI->DrawSprite( Entry.TargetLocation, SpriteSize * Scale, SpriteSize * Scale, DrawingProps.StatSprite->Resource, TargetColor, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}
		
		FVector PlayerToTargetVec(Entry.TargetLocation - Entry.Location);
		FScaleRotationTranslationMatrix ArrowToWorld(FVector(1.0f, 1.0f, 1.0f), PlayerToTargetVec.Rotation(), Entry.Location);
		FLOAT ArrowLength = PlayerToTargetVec.Size();
		FLOAT ArrowSize = 3.0f;
		DrawDirectionalArrow(PDI, ArrowToWorld, PlayerColor, ArrowLength, ArrowSize, DepthPriority);
	}
}

void UBasicStatsVisualizer::VisualizeCanvas(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType)
{
}

/** 
 * Return the drawing properties defined for the given EventID 
 * @param EventID - EventID to get the drawing property for
 */
const FStatDrawingProperties& UBasicStatsVisualizer::GetDrawingProperties(INT EventID)
{
	INT PropIndex = 0;

	for (INT DrawIdx = 0; DrawIdx < DrawingProperties.Num(); DrawIdx++)
	{
		/*const*/ FStatDrawingProperties& DrawProp = DrawingProperties(DrawIdx);
		if (DrawProp.EventID == EventID)
		{
			PropIndex = DrawIdx;
			break;
		}
	}

	return DrawingProperties(PropIndex);
}

void UBasicStatsVisualizer::Visit(GameStringEntry* Entry)
{
   /* Do nothing */
}

void UBasicStatsVisualizer::Visit(GameIntEntry* Entry)
{
   /* Do nothing */
}

void UBasicStatsVisualizer::Visit(TeamIntEntry* Entry)
{
   /* Do nothing */
}

/** 
 * Copy any basic game position event into a basic player entry for visualization
 * @param Entry - game position event that occurred
 */
void UBasicStatsVisualizer::Visit(GamePositionEntry* Entry)
{
	if (Entry != NULL)
	{
		FBasicStatEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;

		BasicEntries.AddItem(NewEntry);
	}
}


/** 
 * Copy any player string event into a basic player entry for visualization
 * @param Entry - Player string event that occurred
 */
void UBasicStatsVisualizer::Visit(PlayerStringEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;
		
		PlayerEntries.AddItem(NewEntry);
	}
}

/** 
 * Copy any player int event into a basic player entry for visualization
 * @param Entry - Player int event that occurred
 */
void UBasicStatsVisualizer::Visit(PlayerIntEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;

		PlayerEntries.AddItem(NewEntry);
	}
}

/** 
 * Copy any player float event into a basic player entry for visualization
 * @param Entry - Player float event that occurred
 */
void UBasicStatsVisualizer::Visit(PlayerFloatEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;

		PlayerEntries.AddItem(NewEntry);
	}
}

void UBasicStatsVisualizer::Visit(PlayerLoginEntry* Entry)
{
   /* Do nothing */
}

/** 
 * Copy any player spawn event into a basic player entry for visualization
 * @param Entry - Player spawn event that occurred
 */
void UBasicStatsVisualizer::Visit(PlayerSpawnEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;
		
		PlayerEntries.AddItem(NewEntry);
	}
}

/** 
 * Create enough data to generate a player -> target sprite pairing
 * @param Entry - Player kill event that occurred
 */
void UBasicStatsVisualizer::Visit(PlayerKillDeathEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerTargetEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;
		NewEntry.TargetLocation = Entry->Target.Location;
		NewEntry.TargetRotation = Entry->Target.Rotation;

		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.PlayerName = Entry->PlayerName;

		NewEntry.TargetIndex = Entry->Target.PlayerIndex;
		NewEntry.TargetName = Entry->Target.PlayerName;

		NewEntry.KillType = Entry->KillTypeString;
		NewEntry.DamageType = Entry->DamageClassName;

		PlayerTargetEntries.AddItem(NewEntry);
	}
}

/** 
 * Create enough data to generate a player - player sprite pairing
 * @param Entry - Player event that occurred
 */
void UBasicStatsVisualizer::Visit(PlayerPlayerEntry * Entry)
{
	if (Entry != NULL)
	{
		FPlayerPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;
		NewEntry.Player2Location = Entry->Target.Location;
		NewEntry.Rotation2 = Entry->Target.Rotation;

		NewEntry.Player1Index = Entry->PlayerIndex;
		NewEntry.Player1Name = Entry->PlayerName;

		NewEntry.Player2Index = Entry->Target.PlayerIndex;
		NewEntry.Player2Name = Entry->Target.PlayerName;

		PlayerPlayerEntries.AddItem(NewEntry);
	}					
}

/** 
 * Copy any player weapon event into a basic player entry for visualization
 * @param Entry - Player weapon event that occurred
 */
void UBasicStatsVisualizer::Visit(WeaponEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.WeaponName = Entry->WeaponClassName;

		PlayerEntries.AddItem(NewEntry);
	}
}

/** 
 * Copy any player damage event into a basic player entry for visualization
 * @param Entry - Player damage event that occurred
 */
void UBasicStatsVisualizer::Visit(DamageEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerTargetEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.DamageType = Entry->DamageClassName;
		NewEntry.TargetIndex = Entry->Target.PlayerIndex;
		NewEntry.TargetName = Entry->Target.PlayerName;
		NewEntry.TargetLocation = Entry->Target.Location;
		NewEntry.TargetRotation = Entry->Target.Rotation;

		PlayerTargetEntries.AddItem(NewEntry);
	}
}

/** 
 * Copy any player projectile event into a basic player entry for visualization
 * @param Entry - Player projectile event that occurred
 */
void UBasicStatsVisualizer::Visit(ProjectileIntEntry* Entry)
{
	if (Entry != NULL)
	{
		FPlayerEntry NewEntry(EC_EventParm);
		NewEntry.EventID = Entry->EventID;
		NewEntry.EventName = Entry->EventName;
		NewEntry.EventTime = Entry->EventTime;
		NewEntry.PlayerIndex = Entry->PlayerIndex;
		NewEntry.PlayerName = Entry->PlayerName;
		NewEntry.Location = Entry->Location;
		NewEntry.Rotation = Entry->Rotation;

		PlayerEntries.AddItem(NewEntry);
	}
}	  



/************************************************************************/
/*   Movement visualizer                                                */
/************************************************************************/

/** Compare two times, returns >0 if TimeA > TimeB, 0 if TimeA==TimeB, <0 if TimeA < TimeB */
QSORT_RETURN TimeCompare(FPosEntry* PosA, FPosEntry* PosB)
{
	return (PosA->Time > PosB->Time) ? 1 : (PosA->Time < PosB->Time) ? -1 : 0; 
}

/** Given a chance to initialize */
void UPlayerMovementVisualizer::Init()
{
	for (INT DrawIdx = 0; DrawIdx < DrawingProperties.Num(); DrawIdx++)
	{
		/*const*/ FPlayerMovementStatDrawingProperties& DrawProp = DrawingProperties(DrawIdx);
		if (DrawProp.StatSprite == NULL && DrawProp.SpriteName.Len() > 0)
		{
			// Try to load the specified class
			DrawProp.StatSprite = LoadObject<UTexture2D>(NULL, *DrawProp.SpriteName, NULL, LOAD_None, NULL);
		}
	}

	Reset();
}

/** Reset the visualizer to initial state */
void UPlayerMovementVisualizer::Reset() 
{
	//We iterate to clear out the specific data, leaving the player sprites intact
	//since we don't always have a spawn event within our dataset (need a better way)
	for (INT PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		FPlayerMovement& Player = Players(PlayerIdx);
		Player.Segments.Empty();
		Player.TempPositions.Empty();
	}
}

/** Called before any database entries are given to the visualizer */
void UPlayerMovementVisualizer::BeginVisiting()
{
	Reset();
}

/** Called at the end of database entry traversal, returns success or failure */
UBOOL UPlayerMovementVisualizer::EndVisiting()
{
	//Sort all the individual segment positions by time
	for (INT PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		FPlayerMovement& Player = Players(PlayerIdx);
		if (Player.TempPositions.Num() > 0)
		{
			//Sort all the gathered data
			appQsort(&Player.TempPositions(0), Player.TempPositions.Num(), sizeof(FPosEntry), (QSORT_COMPARE)TimeCompare); 

			const FPosEntry& FirstPos = Player.TempPositions(0);

			//Add the first movement segment entry and position
			FMovementSegment FirstMovementSegment(EC_EventParm);
			FirstMovementSegment.Positions.AddItem(FirstPos);
			Player.Segments.AddItem(FirstMovementSegment);

			FMovementSegment* CurrentMovementSegment = &Player.Segments.Top();

			FLOAT LastPointTime = FirstPos.Time; 
			for (INT PosIter=1; PosIter<Player.TempPositions.Num(); PosIter++)
			{
				FPosEntry& NewPoint = Player.TempPositions(PosIter);

				//Get the last entry for comparison
				FPosEntry& LastPos = CurrentMovementSegment->Positions.Top();

				//check for "teleport" in time/space
				FLOAT DistToPoint = (LastPos.Position - NewPoint.Position).Size();
				FLOAT TimeDelta = NewPoint.Time - LastPointTime;

				//check closeness to last point (only keep outside of some delta)
				static FLOAT CloseDistThreshold = 5.0f;
				static FLOAT TimeThreshold = 3.0f;
				static FLOAT FarDistThreshold = 1000.0f;
				UBOOL bIsSamePoint = DistToPoint < CloseDistThreshold; 
				UBOOL bNeedNewSegment = (TimeDelta > TimeThreshold) || (DistToPoint > FarDistThreshold);

				//First make sure its a new point
				if (bIsSamePoint == FALSE)
				{
					//Only create a new segment if we've warped or time since last point was large
					if (bNeedNewSegment)
					{
						FMovementSegment NewSegment(EC_EventParm);
						NewSegment.Positions.Push(NewPoint);
						Player.Segments.Push(NewSegment);
						CurrentMovementSegment = &Player.Segments.Top();
					}
					else
					{
						//Add the new point to old segment
						CurrentMovementSegment->Positions.Push(NewPoint);
					}
				}
				//else absorb point

				//Save the last point's time regardless of its use (prevents making new segments unnecessarily)
				LastPointTime = NewPoint.Time;
			}

			Player.TempPositions.Empty();
		}
	}

	return Players.Num() > 0;
}

/** Returns the number of data points the visualizer is actively working with */
INT UPlayerMovementVisualizer::GetVisualizationSetCount() const
{
	INT TotalDataPoints = 0;
	for (INT PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		const TArray<FMovementSegment>& Segments = Players(PlayerIdx).Segments;
		for (INT SegmentIdx = 0; SegmentIdx < Segments.Num(); SegmentIdx++)
		{
			TotalDataPoints += Segments(SegmentIdx).Positions.Num();
		}
	}
	return TotalDataPoints;
}

/** 
 *	Retrieve some metadata about an event
 * @param EventIndex - some visualizer relative index about the data to get metadata about
 * @param MetadataString - return string containing information about the event requested
 */
void UPlayerMovementVisualizer::GetMetadata(INT EventIndex, FString& MetadataString)
{	   
	MetadataString.Empty();
	if (Players.IsValidIndex(EventIndex))
	{
		const FPlayerMovement& Player = Players(EventIndex);
		MetadataString = FString::Printf(TEXT("%s\n"), *Player.PlayerName);
	}
}

/** 
 * Return the drawing properties defined for the given player 
 * @param PawnClassName - Name of the pawn spawned
 */
const FPlayerMovementStatDrawingProperties& UPlayerMovementVisualizer::GetDrawingProperties(const FString& PawnClassName)
{
	INT PropIndex = 0;

	//First entry is fallback
	for (INT DrawIdx = 1; DrawIdx < DrawingProperties.Num(); DrawIdx++)
	{
		/*const*/ FPlayerMovementStatDrawingProperties& DrawProp = DrawingProperties(DrawIdx);
		if (PawnClassName.InStr(DrawProp.PawnClassName, FALSE, TRUE) >= 0)
		{
			PropIndex = DrawIdx;
			break;
		}
	}

	return DrawingProperties(PropIndex);
}

/** Called when a hitproxy belonging to this visualizer is triggered */
void UPlayerMovementVisualizer::HandleHitProxy(HGameStatsHitProxy* HitProxy)
{

}

/** 
 * Draws all players with unique color within the given time period
 * taking into account time/space jumps
 * @param View - the view being drawn in
 * @param PDI - draw interface for primitives
 */
void UPlayerMovementVisualizer::Visualize(const FSceneView* View, FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType)
{
	BYTE DepthPriority = (ViewportType == LVT_Perspective) ? SDPG_World : SDPG_Foreground;

	//Sprite size scale divisor done once outside of all for-loops
	FLOAT ScaleDivisor = ( 4 / (FLOAT)View->SizeX / View->ProjectionMatrix.M[0][0] );

	//for each player
	for (INT PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		const FPlayerMovement& Player = Players(PlayerIdx);

		const FColor& PlayerColor = GetColorByPlayerIndex(Player.PlayerIndex);

		if (Player.Segments.Num() > 0)
		{
			//Draw the "head"
			if (Player.StatSprite != NULL)
			{
				const FMovementSegment& NewestMovementSegment = Player.Segments.Top();
				const FPosEntry& NewestPosEntry = NewestMovementSegment.Positions.Top();
				const FPlane ProjPoint = View->Project(NewestPosEntry.Position);
				if (ProjPoint.W > 0.0f)
				{
					const FLOAT Scale = ProjPoint.W * ScaleDivisor;
					const FLOAT SpriteSize = 8.0f;
					PDI->SetHitProxy( new HGameStatsHitProxy(this, PlayerIdx) );
					PDI->DrawSprite( NewestPosEntry.Position, SpriteSize * Scale, SpriteSize * Scale, Player.StatSprite->Resource, PlayerColor, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
					PDI->SetHitProxy( NULL );
				}
			}

			//for each segment
			INT NumSegments = Player.Segments.Num();
			for (INT SegmentIdx = 0; SegmentIdx < NumSegments; SegmentIdx++)
			{
				const FMovementSegment& CurrentMovementSegment = Player.Segments(SegmentIdx);
				//const FLOAT MinTime = CurrentMovementSegment.Positions(0).Time;
				//const FLOAT MaxTime = CurrentMovementSegment.Positions.Top().Time;

				//draw a line segment
				INT NumPositions = CurrentMovementSegment.Positions.Num();
				for (INT PosIdx = 1; PosIdx < NumPositions; PosIdx++)
				{
					const FPosEntry& PrevEntry = CurrentMovementSegment.Positions(PosIdx - 1);
					const FPosEntry& PosEntry = CurrentMovementSegment.Positions(PosIdx);

					FLinearColor LineColor(PlayerColor);
					//LineColor.Desaturate(1.0f - ((PrevEntry.Time - MinTime) / (MaxTime - MinTime)));
					FVector PlayerToTargetVec(PosEntry.Position - PrevEntry.Position);
					FScaleRotationTranslationMatrix SegDirMatrix(FVector(1.0f, 1.0f, 1.0f), PlayerToTargetVec.Rotation(), PrevEntry.Position);
					FLOAT ArrowLength = PlayerToTargetVec.Size();
					FLOAT ArrowSize = 3.0f;
					DrawDirectionalArrow(PDI, SegDirMatrix, LineColor, ArrowLength, ArrowSize, DepthPriority);

					FScaleRotationTranslationMatrix ArrowToWorld(FVector(1.0f, 1.0f, 1.0f), PrevEntry.Rotation, PrevEntry.Position);
					ArrowLength = 40.0f;
					ArrowSize = 3.0f;
					DrawDirectionalArrow(PDI, ArrowToWorld, FLinearColor::White, ArrowLength, ArrowSize, DepthPriority);
				}
			}
		}
	}
}

/** Create or find a given player entry by index */
FPlayerMovement& UPlayerMovementVisualizer::CreateOrFindPlayerEntry(INT PlayerIndex, const FString& PlayerName)
{
	//Get/allocate the unique player
	INT ThisPlayerIdx = INDEX_NONE;
	for (INT PlayerIdx = 0; PlayerIdx < Players.Num(); PlayerIdx++)
	{
		if (Players(PlayerIdx).PlayerIndex == PlayerIndex)
		{
			ThisPlayerIdx = PlayerIdx;
			break;
		}
	}

	if (ThisPlayerIdx == INDEX_NONE)
	{
		//Create a new player entry
		FPlayerMovement Player(EC_EventParm);
		Player.PlayerIndex = PlayerIndex;
		Player.PlayerName = PlayerName;

		ThisPlayerIdx = Players.AddItem(Player);
	}

	return Players(ThisPlayerIdx); 
}

/** Player locations during the game are stored as PlayerIntEntries so we only really need to process these */
/** ASSUMPTION: At the moment the DB returns results in a time monotonic order, but this is a bad assumption, should really collect/sort/process */
void UPlayerMovementVisualizer::Visit(PlayerIntEntry* Entry)
{
	//Only care about movement atm
	if (Entry != NULL && Entry->PlayerIndex != INDEX_NONE && Entry->EventID == UCONST_GAMEEVENT_PLAYER_LOCATION_POLL)
	{
		//Get/allocate the unique player
		FPlayerMovement& Player = CreateOrFindPlayerEntry(Entry->PlayerIndex, Entry->PlayerName);

		//Fill out the position info
		FPosEntry PosEntry(EC_EventParm);
		PosEntry.Time = Entry->EventTime;
		PosEntry.Position = Entry->Location;
		PosEntry.Rotation = Entry->Rotation;

		//Add it to the array
		Player.TempPositions.AddItem(PosEntry);
	}
}

/** Player spawns reveal the pawn class in use so we can choose a sprite */
void UPlayerMovementVisualizer::Visit(class PlayerSpawnEntry* Entry)
{
	//Only care about movement atm
	if (Entry != NULL && Entry->PlayerIndex != INDEX_NONE)
	{
		FPlayerMovement& Player = CreateOrFindPlayerEntry(Entry->PlayerIndex, Entry->PlayerName);

		const FPlayerMovementStatDrawingProperties& DrawProps = GetDrawingProperties(Entry->PawnClassName);
	   
		//Set the sprite (@TODO - each spawn could be a different pawn class, do we care?)
		Player.StatSprite = DrawProps.StatSprite; 

		//Fill out the position info
		FPosEntry PosEntry(EC_EventParm);
		PosEntry.Time = Entry->EventTime;
		PosEntry.Position = Entry->Location;
		PosEntry.Rotation = Entry->Rotation;

		//Add it to the array
		Player.TempPositions.AddItem(PosEntry);
	}
}

/************************************************************************/
/* Generic param list visualizer                                        */
/************************************************************************/
/** Given a chance to initialize */
void UGenericParamlistVisualizer::Init()
{
	Reset();
}

/** Reset the visualizer to initial state */
void UGenericParamlistVisualizer::Reset() 
{
	DrawAtoms.Empty();
}

/** Called before any database entries are given to the visualizer */
void UGenericParamlistVisualizer::BeginVisiting()
{
	Reset();
}

/** Called at the end of database entry traversal, returns success or failure */
UBOOL UGenericParamlistVisualizer::EndVisiting()
{
	return TRUE;
}

/** Returns the number of data points the visualizer is actively working with */
INT UGenericParamlistVisualizer::GetVisualizationSetCount() const
{
	return DrawAtoms.Num();
}

/** 
 *	Retrieve some metadata about an event
 * @param EventIndex - some visualizer relative index about the data to get metadata about
 * @param MetadataString - return string containing information about the event requested
 */
void UGenericParamlistVisualizer::GetMetadata(INT EventIndex, FString& MetadataString)
{	   
	MetadataString.Empty();
	if (DrawAtoms.IsValidIndex(EventIndex))
	{
		const FDrawAtom& Atom = DrawAtoms(EventIndex);
		MetadataString = FString::Printf(TEXT("%s\n%s"), *Atom.ShortName,*Atom.LongName);
	}
}

/** Called when a hitproxy belonging to this visualizer is triggered */
void UGenericParamlistVisualizer::HandleHitProxy(HGameStatsHitProxy* HitProxy)
{

}

/** 
 * Draws all players with unique color within the given time period
 * taking into account time/space jumps
 * @param View - the view being drawn in
 * @param PDI - draw interface for primitives
 */
extern void DrawWireBox(class FPrimitiveDrawInterface* PDI,const FBox& Box,FColor Color,BYTE DepthPriority);
void UGenericParamlistVisualizer::Visualize(const FSceneView* View, FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType)
{
	BYTE DepthPriority = (ViewportType == LVT_Perspective) ? SDPG_World : SDPG_Foreground;

	//Sprite size scale divisor done once outside of all for-loops
	FLOAT ScaleDivisor = ( 4 / (FLOAT)View->SizeX / View->ProjectionMatrix.M[0][0] );

	//for each DrawAtom
	for (INT AtomIdx = 0; AtomIdx< DrawAtoms.Num(); ++AtomIdx)
	{
		const FDrawAtom& Atom = DrawAtoms(AtomIdx);
		
		// a sprite!
		const FPlane ProjPoint = View->Project(Atom.Loc);
		if (Atom.Sprite != NULL && ProjPoint.W > 0.0f)
		{
			const FLOAT Scale = ProjPoint.W * ScaleDivisor;
			const FLOAT SpriteSize = 8.0f;
			PDI->SetHitProxy( new HGameStatsHitProxy(this, AtomIdx) );
			PDI->DrawSprite( Atom.Loc, SpriteSize * Scale, SpriteSize * Scale, Atom.Sprite->Resource, Atom.Color, DepthPriority, 0.0, 0.0, 0.0, 0.0 );
			PDI->SetHitProxy( NULL );
		}

		// lines
		for(INT LineIdx=0;LineIdx<Atom.Lines.Num();++LineIdx)
		{
			const FLine& Line = Atom.Lines(LineIdx);
			PDI->DrawLine(Line.LineStart,Line.LineEnd,Line.LineColor,DepthPriority,Line.Thickness);
		}

		// boxes 
		for(INT BoxIdx=0;BoxIdx<Atom.Boxes.Num();++BoxIdx)
		{
			const FDrawBox& Box = Atom.Boxes(BoxIdx);
			FBox TempBox = FBox::BuildAABB(Box.BoxLoc,Box.Extent);
			DrawWireBox(PDI,TempBox,Box.BoxColor,DepthPriority);
		}
	}
}


void UGenericParamlistVisualizer::Visit(GenericParamListEntry* Entry)
{
	FString Name;
	FVector VecData0(0.f),VecData1(0.f),VecData2(0.f);

	if(Entry->GetNamedParamData<FString>(FName(TEXT("Name")),Name) && Entry->GetNamedParamData<FVector>(FName(TEXT("BaseLocation")),VecData0))
	{
		
		// overall stuff
		FDrawAtom NewAtom(EC_EventParm);
		NewAtom.Loc = VecData0;
		NewAtom.ShortName=Name;
		Entry->GetNamedParamData<FString>(FName(TEXT("Text")),NewAtom.LongName);
		NewAtom.Color = FLinearColor(1.0f,1.0f,1.0f);
		if( Entry->GetNamedParamData<FVector>(FName(TEXT("Color")),VecData0) )
		{
			NewAtom.Color = FLinearColor(VecData0.X,VecData0.Y,VecData0.Z);
		}

		// sprite
		FString SpriteName;
		if( Entry->GetNamedParamData<FString>(FName(TEXT("Sprite")),SpriteName))
		{
			NewAtom.Sprite = LoadObject<UTexture2D>(NULL, *SpriteName, NULL, LOAD_None, NULL);
		}

		// lines
		UBOOL bLine = Entry->GetNamedParamData<FVector>(FName(TEXT("LineStart")),VecData0) && Entry->GetNamedParamData<FVector>(FName(TEXT("LineEnd")),VecData1);
		if(bLine)
		{
			FLine NewLine(EC_EventParm);
			NewLine.LineStart = VecData0;
			NewLine.LineEnd = VecData1;
			NewLine.LineColor = FLinearColor(1.0,0.0,0.0,1.0f);
			if( Entry->GetNamedParamData<FVector>(FName(TEXT("LineColor")),VecData2) )
			{
				NewLine.LineColor = FLinearColor(VecData2.X,VecData2.Y,VecData2.Z,1.0f);
			}
			NewLine.Thickness = 0.0f;
			Entry->GetNamedParamData<FLOAT>(FName(TEXT("LineThickness")),NewLine.Thickness);
			NewAtom.Lines.AddItem(NewLine);
		}

		// boxes
		UBOOL bBox = Entry->GetNamedParamData<FVector>(FName(TEXT("BoxLoc")),VecData0) && Entry->GetNamedParamData<FVector>(FName(TEXT("BoxExtent")),VecData1);
		if(bBox)
		{
			FDrawBox Box(EC_EventParm);
			Box.BoxLoc = VecData0;
			Box.Extent = VecData1;
			Box.BoxColor = FColor(255,0,0);
			if( Entry->GetNamedParamData<FVector>(FName(TEXT("BoxColor")),VecData2) )
			{
				Box.BoxColor = FLinearColor(VecData2.X,VecData2.Y,VecData2.Z,1.0f).ToFColor(FALSE);
			}
			NewAtom.Boxes.AddItem(Box);
		}

		DrawAtoms.AddItem(NewAtom);

	}


}
/************************************************************************/
/*   Heatmap  visualizer                                                */
/************************************************************************/

#define DEFAULT_HEATMAP_PT_STRENGTH 8.0f

/** Given a chance to initialize */
void UHeatmapVisualizer::Init()
{
	//Create a texture to hold the heatmap
	OverlayTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass());
	OverlayTexture->LODGroup = TEXTUREGROUP_UI;
	OverlayTexture->AddressX = TA_Clamp;
	OverlayTexture->AddressY = TA_Clamp;
	OverlayTexture->Init(TextureXSize, TextureYSize, PF_G8);

	// Create the material instance.
	HeatmapMaterial = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass());
	HeatmapMaterial->SetParent(GEngine->HeatmapMaterial);
	HeatmapMaterial->SetTextureParameterValue(TEXT("HeatmapTexture"), OverlayTexture);
}

/** Reset the visualizer to initial state */
void UHeatmapVisualizer::Reset() 
{
	//Reset the world bounds
	WorldMinPos.Set(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	WorldMaxPos.Set(-HALF_WORLD_MAX, -HALF_WORLD_MAX, -HALF_WORLD_MAX);

	//Empty the previous positions
	HeatmapPositions.Empty();
}

/**
 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
 * asynchronous cleanup process.
 */
void UHeatmapVisualizer::BeginDestroy()
{
	//Delete the options dialog if we created one
	if (OptionsDialog) 
	{
		OptionsDialog->Destroy();
		OptionsDialog = NULL;
	}

	UGameStatsVisualizer::BeginDestroy();
}

/** 
 * Returns a dialog box with options related to the visualizer
 * @return NULL if no options for this visualizer, else pointer to dialog
 */
WxVisualizerOptionsDialog* UHeatmapVisualizer::GetOptionsDialog()
{
	if (OptionsDialog == NULL)
	{
		OptionsDialog = new WxHeatmapOptionsDialog();
		if (OptionsDialog == NULL || OptionsDialog->Create(this) == FALSE)
		{
			OptionsDialog = NULL;
		}
	}

	return OptionsDialog; 
}

/** Called before any database entries are given to the visualizer */
void UHeatmapVisualizer::BeginVisiting()
{
	Reset();
}

/* 
 *   Draws a strength value, attenuated by distance from center, at a given grid location and radius
 * @param Grid - array of grid points to draw within
 * @param Strength - strength of the initial center point
 * @param CenterX - center point X
 * @param CenterY - center point Y
 * @param Radius - radius to draw
 * @param MaxX - bounds in X of the grid
 * @param MaxY - bounds in Y of the grid
 */
static void DrawLocation(TArray<FLOAT>& Grid, FLOAT Strength, INT CenterX, INT CenterY, INT Radius, INT MaxX, INT MaxY)
{
	for (INT dx=-Radius; dx<=Radius; dx++)
	{
		INT PosX = CenterX + dx;
		if (PosX >= 0 && PosX < MaxX)
		{
			for (INT dy=-Radius; dy<=Radius; dy++)
			{
				INT PosY = CenterY + dy;

				if (PosY >= 0 && PosY < MaxY)
				{
					INT TexelIdx = INT(PosY * MaxX + PosX);
					FLOAT Dist = Abs(dx) + Abs(dy);
					Grid(TexelIdx) += (Dist == 0) ? Strength : (Strength / Dist);
				}
			}
		}
	}
}

/** Called at the end of database entry traversal, returns success or failure */
UBOOL UHeatmapVisualizer::EndVisiting()
{
	if (HeatmapMaterial && OverlayTexture && HeatmapPositions.Num() > 0)
	{
		// Provide some padding to the texture to prevent heat radius clipping
		const FVector WorldOffset(5.0f*NumUnrealUnitsPerPixel, 5.0f*NumUnrealUnitsPerPixel, 0.f);
		WorldMinPos += (WorldOffset * -1.f);
		WorldMaxPos += WorldOffset;

		//Create the texture first time
		CreateHeatmapGrid();
		UpdateDensityMapping(-1, -1);
		CreateHeatmapTexture();
		return TRUE;
	}
	else if (OverlayTexture)
	{
		// Clear out the texture on failure
		INT TextureDataSize = OverlayTexture->Mips(0).Data.GetBulkDataSize();
		BYTE* TextureData = (BYTE*)OverlayTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
		appMemzero(TextureData, TextureDataSize);
		OverlayTexture->Mips(0).Data.Unlock();
		OverlayTexture->UpdateResource();
	}

	return FALSE;
}

/** Returns the number of data points the visualizer is actively working with */
INT UHeatmapVisualizer::GetVisualizationSetCount() const
{
	return HeatmapPositions.Num();
}

void UHeatmapVisualizer::UpdateDensityMapping(INT NewMinDensity, INT NewMaxDensity)
{
	//get min/max values for normalization
	if (NewMinDensity == -1 && NewMaxDensity == -1)
	{
		FLOAT TempMinDensity = 1e10;
		FLOAT TempMaxDensity = -1e10;
		for (INT PosIdx=0; PosIdx<PositionGrid.Num(); PosIdx++)
		{
			FLOAT Value = PositionGrid(PosIdx);
			if (Value > 0)
			{
				if (Value < TempMinDensity) 
				{
					TempMinDensity = Value;
				}
				if (Value > TempMaxDensity)
				{
					TempMaxDensity = Value;
				}
			}
		}

		MinDensity = appTrunc(TempMinDensity);
		MaxDensity = appTrunc(TempMaxDensity);
		if (CurrentMinDensity > 0 && CurrentMaxDensity > 0)
		{
			CurrentMinDensity = Clamp(CurrentMinDensity, 0, MaxDensity - 1);
			CurrentMaxDensity = Clamp(CurrentMaxDensity, CurrentMinDensity + 1, MaxDensity);
		}
		else
		{
			//Set the default to [min, .95 * max] to get a good range of color
			CurrentMinDensity = MinDensity;
			CurrentMaxDensity = appTrunc(0.95f * (FLOAT)MaxDensity);
		}
	}
	else
	{
		CurrentMinDensity = Clamp(NewMinDensity, 0, MaxDensity - 1);
		CurrentMaxDensity = Clamp(NewMaxDensity, MinDensity + 1, MaxDensity);
	}

	if (OptionsDialog != NULL)
	{
		//Update the options dialog to reflect the current state of the heatmap
		WxHeatmapOptionsDialog* Options = static_cast<WxHeatmapOptionsDialog*>(OptionsDialog);

		wxSlider* MinDensitySlider = static_cast< wxSlider* >(
			Options->FindWindow( TEXT( "ID_MINDENSITY" ) ) );
		check( MinDensitySlider != NULL );

		MinDensitySlider->SetRange(0, CurrentMaxDensity - 1);
		MinDensitySlider->SetValue(CurrentMinDensity);

		wxSlider* MaxDensitySlider = static_cast< wxSlider* >(
			Options->FindWindow( TEXT( "ID_MAXDENSITY" ) ) );
		check( MaxDensitySlider != NULL );

		MaxDensitySlider->SetRange(CurrentMinDensity + 1, MaxDensity);
		MaxDensitySlider->SetValue(CurrentMaxDensity);
	}
}
			  
/*
 *   Calculate the positional heatmap values from all the points in the data set
 */
void UHeatmapVisualizer::CreateHeatmapGrid()
{
	FLOAT WorldLengthX = WorldMaxPos.X - WorldMinPos.X;
	FLOAT WorldLengthY = WorldMaxPos.Y - WorldMinPos.Y;

	// Recalculate an aspect correct texture for rendering
	TextureXSize = Min<INT>(Max<INT>((INT)((WorldLengthX + 0.5f) / NumUnrealUnitsPerPixel), 1), 2048);
	TextureYSize = Min<INT>(Max<INT>((INT)((WorldLengthY + 0.5f) / NumUnrealUnitsPerPixel), 1), 2048);
	
	PositionGrid.Empty(TextureXSize * TextureYSize);
	PositionGrid.AddZeroed(TextureXSize * TextureYSize);
	for (INT PosIdx=0; PosIdx<HeatmapPositions.Num(); PosIdx++)
	{
		const FHeatMapPosEntry& Entry = HeatmapPositions(PosIdx);

		INT TexelX = ((Entry.Position.X - WorldMinPos.X) / WorldLengthX) * TextureXSize;
		INT TexelY = ((Entry.Position.Y - WorldMinPos.Y) / WorldLengthY) * TextureYSize;

		DrawLocation(PositionGrid, Entry.Strength, TexelX, TexelY, HeatRadius, TextureXSize, TextureYSize);
	}
}

/**
 * Runs through the data and creates a heatmap texture, normalizing values
 */
void UHeatmapVisualizer::CreateHeatmapTexture()
{
	check(OverlayTexture);
	OverlayTexture->Init(TextureXSize, TextureYSize, PF_G8);

	// Setup the texture and allocate the first mip level's worth of data
	INT TextureDataSize = OverlayTexture->Mips(0).Data.GetBulkDataSize();
	BYTE* TextureData = (BYTE*)OverlayTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);

	// Expand the range to encompass 0..1 for [MinVal, MaxVal]
	check(PositionGrid.Num() == TextureDataSize);
	FLOAT Modifier = 255.0f / (FLOAT)(CurrentMaxDensity - CurrentMinDensity);
	for (INT PosIdx=0; PosIdx<TextureDataSize; PosIdx++)
	{
		TextureData[PosIdx] = Min(255, Max(0, appFloor((FLOAT)(PositionGrid(PosIdx) - CurrentMinDensity) * Modifier + 0.5f)));
	}

	OverlayTexture->Mips(0).Data.Unlock();
	OverlayTexture->UpdateResource();
}

/** 
 *	Retrieve some metadata about an event
 * @param EventIndex - some visualizer relative index about the data to get metadata about
 * @param MetadataString - return string containing information about the event requested
 */
void UHeatmapVisualizer::GetMetadata(INT EventIndex, FString& MetadataString)
{

}

/** Called when a hitproxy belonging to this visualizer is triggered */
void UHeatmapVisualizer::HandleHitProxy(HGameStatsHitProxy* HitProxy)
{

}

/** 
 * Draws all players with unique color within the given time period
 * taking into account time/space jumps
 * @param View - the view being drawn in
 * @param PDI - draw interface for primitives
 * @param ViewportType - type of viewport being draw (perspective, ortho)
 */
void UHeatmapVisualizer::Visualize(const FSceneView* View, FPrimitiveDrawInterface* PDI, ELevelViewportType ViewportType)
{
}

/** 
 * Draw your stuff as a canvas overlay 
 * @param ViewportClient - viewport client currently drawing
 * @param View - the view being drawn in
 * @param Canvas - overlay canvas
 * @param ViewportType - type of viewport being draw (perspective, ortho)
 */
void UHeatmapVisualizer::VisualizeCanvas(FEditorLevelViewportClient* ViewportClient, const FSceneView* View, FCanvas* Canvas, ELevelViewportType ViewportType)
{
	if (!Canvas->IsHitTesting() && OverlayTexture && HeatmapPositions.Num() > 0)
	{
		//Converts the world space coordinates into screen space [0,0] [ViewX, ViewY]
		FVector2D MinScreen;
		View->ScreenToPixel(View->WorldToScreen(WorldMinPos),MinScreen);

		FVector2D MaxScreen;
		View->ScreenToPixel(View->WorldToScreen(WorldMaxPos),MaxScreen);

		//@TODO - Shader way to get this done much faster
		//Get RenderT   utexturerendertarget2d
		//Create RenderT(RGBA)
		//Set RenderT
		//Clear RenderT(black/alpha)
		//DrawPrim Points
		//Set old rendertarget back on canvas

		//Feed new texture to blur
		//DrawBlur	  ????
		//Draw new blur result to created rendertarget
		//Resolve to texture

		//Feed newest texture into heatmap material
		
		// Draw the heatmap on the canvas
		VisualizeCanvas(Canvas, MinScreen, MaxScreen);
	}
}

/*
 *   Actual DrawTile call to the canvas, using min/max screen values to properly position the texture
 * @param Canvas - canvas to draw to
 * @param MinScreen - WorldMin position of the heatmap, converted to screen space
 * @param MaxScreen - WorldMax position of the heatmap, converted to screen space
 */
void UHeatmapVisualizer::VisualizeCanvas(FCanvas* Canvas, const FVector2D& MinScreen, const FVector2D& MaxScreen)
{
	if (OverlayTexture && HeatmapPositions.Num() > 0)
	{
		//Need to recompute what is min,max in screen space
		FLOAT MinX = Min(MinScreen.X, MaxScreen.X);
		FLOAT MinY = Min(MinScreen.Y, MaxScreen.Y);

		FLOAT MaxX = Max(MinScreen.X, MaxScreen.X);
		FLOAT MaxY = Max(MinScreen.Y, MaxScreen.Y);

		DrawTile(Canvas, MinX, MinY, MaxX - MinX, MaxY - MinY, 0, 0, 1, 1, HeatmapMaterial->GetRenderProxy(0)); 
	}
}

void UHeatmapVisualizer::AddNewPoint(const FVector& Pt, FLOAT Time, FLOAT Strength)
{
	FHeatMapPosEntry LocEntry;

	if (!Pt.ContainsNaN())
	{
		LocEntry.Position = Pt;
		LocEntry.Time = Time;
		LocEntry.Strength = Strength;
		HeatmapPositions.AddItem(LocEntry);

		//Setup bounds for the data
		WorldMinPos.X = Min(WorldMinPos.X, LocEntry.Position.X);
		WorldMaxPos.X = Max(WorldMaxPos.X, LocEntry.Position.X);

		WorldMinPos.Y = Min(WorldMinPos.Y, LocEntry.Position.Y);
		WorldMaxPos.Y = Max(WorldMaxPos.Y, LocEntry.Position.Y);
	}
}

/** Game positions are relevant to heatmap */
void UHeatmapVisualizer::Visit(GamePositionEntry* Entry)
{
	FLOAT Strength = DEFAULT_HEATMAP_PT_STRENGTH;
	switch (Entry->EventID)
	{
	case UCONST_GAMEEVENT_GPUFRAMETIME_POLL:
	case UCONST_GAMEEVENT_GAMETHREAD_POLL:
	case UCONST_GAMEEVENT_RENDERTHREAD_POLL:
	case UCONST_GAMEEVENT_FRAMETIME_POLL:
		break;
	default:
		AddNewPoint(Entry->Location,Entry->EventTime, Strength);
		break;
	}
}

/** Player locations during the game are stored as PlayerIntEntries so we only really need to process these */
void UHeatmapVisualizer::Visit(PlayerIntEntry* Entry)
{
	//Only care about movement atm
	if (Entry != NULL && Entry->PlayerIndex != INDEX_NONE && Entry->EventID == UCONST_GAMEEVENT_PLAYER_LOCATION_POLL)
	{
		AddNewPoint(Entry->Location, Entry->EventTime, DEFAULT_HEATMAP_PT_STRENGTH);
	}
}

/** Player kills are relevant to heatmap */
void UHeatmapVisualizer::Visit(PlayerKillDeathEntry* Entry)
{
	AddNewPoint(Entry->Location, Entry->EventTime, DEFAULT_HEATMAP_PT_STRENGTH);
}

/** Player spawns are relevant to heatmap */
void UHeatmapVisualizer::Visit(PlayerSpawnEntry* Entry)
{
	AddNewPoint(Entry->Location, Entry->EventTime, DEFAULT_HEATMAP_PT_STRENGTH);
}

/** Player spawns are relevant to heatmap */
void UHeatmapVisualizer::Visit(ProjectileIntEntry* Entry)
{
	AddNewPoint(Entry->Location, Entry->EventTime, DEFAULT_HEATMAP_PT_STRENGTH);
}

void UHeatmapVisualizer::Visit(GenericParamListEntry* Entry)
{
	FVector Vec(0.f);
	if(Entry->GetNamedParamData<FVector>(TEXT("HeatmapPoint"),Vec))
	{
		AddNewPoint(Vec, Entry->EventTime, DEFAULT_HEATMAP_PT_STRENGTH);
	}
}

/************************************************************************/
/*  UPerformanceVisualizer                                              */
/************************************************************************/

/** Reset the visualizer to initial state */
void UPerformanceVisualizer::Reset()
{
	Super::Reset();
	GridPositionHitCounts.Empty();
	GridPositionMaxValues.Empty();
	GridPositionSums.Empty();
}

/** Called before any database entries are given to the visualizer */
void UPerformanceVisualizer::BeginVisiting()
{
	Super::BeginVisiting();
}

/** Called at the end of database entry traversal, returns success or failure */
UBOOL UPerformanceVisualizer::EndVisiting()
{
	return Super::EndVisiting();
}

/** Game locations during the game are stored as GamePositionEntries */
void UPerformanceVisualizer::Visit(class GamePositionEntry* Entry)
{
	switch (Entry->EventID)
	{
	case UCONST_GAMEEVENT_GPUFRAMETIME_POLL:
	case UCONST_GAMEEVENT_RENDERTHREAD_POLL:
	case UCONST_GAMEEVENT_GAMETHREAD_POLL:
	case UCONST_GAMEEVENT_FRAMETIME_POLL:
		{
			AddNewPoint(Entry->Location, Entry->EventTime, Entry->Value);
			break;
		}
	default:
		break;
	}
}

/* 
 * Draws a strength value, attenuated by distance from center, at a given grid location and radius
 * will only set a maximum value per cell
 * @param Grid - array of grid points to draw within
 * @param Strength - strength of the initial center point
 * @param CenterX - center point X
 * @param CenterY - center point Y
 * @param Radius - radius to draw
 * @param MaxX - bounds in X of the grid
 * @param MaxY - bounds in Y of the grid
 */
static void DrawLocationMax(TArray<FLOAT>& Grid, FLOAT Strength, INT CenterX, INT CenterY, INT Radius, INT MaxX, INT MaxY)
{
	for (INT dx=-Radius; dx<=Radius; dx++)
	{
		INT PosX = CenterX + dx;
		if (PosX >= 0 && PosX < MaxX)
		{
			for (INT dy=-Radius; dy<=Radius; dy++)
			{
				INT PosY = CenterY + dy;

				if (PosY >= 0 && PosY < MaxY)
				{
					INT TexelIdx = INT(PosY * MaxX + PosX);
					FLOAT Dist = Abs(dx) + Abs(dy);
					Grid(TexelIdx) = Max<FLOAT>((Dist == 0) ? Strength : (Strength / Dist), Grid(TexelIdx));
				}
			}
		}
	}
}

/*
 *   Calculate the positional heatmap values from all the points in the data set
 */
void UPerformanceVisualizer::CreateHeatmapGrid()
{
	FLOAT WorldLengthX = WorldMaxPos.X - WorldMinPos.X;
	FLOAT WorldLengthY = WorldMaxPos.Y - WorldMinPos.Y;

	// Recalculate an aspect correct texture for rendering
	TextureXSize = Min<INT>(Max<INT>((INT)((WorldLengthX + 0.5f) / NumUnrealUnitsPerPixel), 1), 2048);
	TextureYSize = Min<INT>(Max<INT>((INT)((WorldLengthY + 0.5f) / NumUnrealUnitsPerPixel), 1), 2048);
	
// 	GridPositionHitCounts.Empty(TextureXSize * TextureYSize);
// 	GridPositionHitCounts.AddZeroed(TextureXSize * TextureYSize);
// 	GridPositionSums.Empty(TextureXSize * TextureYSize);
// 	GridPositionSums.AddZeroed(TextureXSize * TextureYSize);
	GridPositionMaxValues.Empty(TextureXSize * TextureYSize);
	GridPositionMaxValues.AddZeroed(TextureXSize * TextureYSize);
	for (INT PosIdx=0; PosIdx<HeatmapPositions.Num(); PosIdx++)
	{
		const FHeatMapPosEntry& Entry = HeatmapPositions(PosIdx);

		INT TexelX = ((Entry.Position.X - WorldMinPos.X) / WorldLengthX) * TextureXSize;
		INT TexelY = ((Entry.Position.Y - WorldMinPos.Y) / WorldLengthY) * TextureYSize;

		const INT GridPosition = TexelX * TextureYSize + TexelY;

		// Increase hit count in this position
// 		GridPositionHitCounts(GridPosition) = GridPositionHitCounts(GridPosition) + 1;
		// Sum values (with hitcount would be avg)
// 		GridPositionSums(GridPosition) = GridPositionSums(GridPosition) + Entry.Strength;
		// Record Max value
		if (GridPositionMaxValues(GridPosition) < Entry.Strength)
		{
			GridPositionMaxValues(GridPosition) = Entry.Strength;
		}
	}

	PositionGrid.Empty(TextureXSize * TextureYSize);
	PositionGrid.AddZeroed(TextureXSize * TextureYSize);
	for (INT PosIdx=0; PosIdx<GridPositionMaxValues.Num(); PosIdx++)
	{
		INT TexelX = PosIdx / TextureYSize;
		INT TexelY = PosIdx % TextureYSize;

		DrawLocationMax(PositionGrid, GridPositionMaxValues(PosIdx), TexelX, TexelY, HeatRadius, TextureXSize, TextureYSize);
	}
}
