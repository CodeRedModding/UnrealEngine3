/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "Factories.h"
#include "FontPropertiesDlg.h"
#include "PropertyWindow.h"

BEGIN_EVENT_TABLE(WxFontPropertiesDlg, WxTrackableDialog)
	EVT_SIZE( WxFontPropertiesDlg::OnSize )
	EVT_SCROLL( WxFontPropertiesDlg::OnScroll )
	EVT_SET_FOCUS( WxFontPropertiesDlg::OnFocus )
	EVT_CLOSE( WxFontPropertiesDlg::OnClose )
	// Button events
	EVT_BUTTON( ID_CLOSE, WxFontPropertiesDlg::OnOK )
	EVT_BUTTON( ID_UPDATE, WxFontPropertiesDlg::OnUpdateFontPage )
	EVT_BUTTON( ID_UPDATE_ALL, WxFontPropertiesDlg::OnUpdateAllFontPages )
	EVT_BUTTON( ID_EXPORT, WxFontPropertiesDlg::OnExport )
	EVT_BUTTON( ID_EXPORT_ALL, WxFontPropertiesDlg::OnExportAll )
	EVT_BUTTON( ID_PREVIEW, WxFontPropertiesDlg::OnPreview )
END_EVENT_TABLE()

FString WxFontPropertiesDlg::LastPath;

/**
 * Creates the property window and child viewport objects
 */
WxFontPropertiesDlg::WxFontPropertiesDlg() :
	WxTrackableDialog(NULL,-1,wxString(TEXT("FontProperties"))),
	PreviewWindowID( wxID_NONE ),
	Viewport(NULL),
	FontPropertyWindow(NULL),
	TexturePropertyWindow(NULL),
	ViewportHolder(NULL),
	bIsScrolling(FALSE),
	Font(NULL),
	UpdateButton(NULL),
	ExportButton(NULL),
	CurrentSelectedPage(-1),
	TGAExporter(NULL),
	Factory(NULL)
{
	// Create all of our child windows
	CreateControls();
	// Create a TGA exporter
	TGAExporter = ConstructObject<UTextureExporterTGA>(UTextureExporterTGA::StaticClass());
	// And our importer
	Factory = ConstructObject<UTextureFactory>(UTextureFactory::StaticClass());
	// Set the defaults
	Factory->Blending = BLEND_Opaque;
	Factory->LightingModel = MLM_Unlit;
	Factory->bDeferCompression = TRUE;
}

/**
 * Creates all of the child controls for this dialog
 */
void WxFontPropertiesDlg::CreateControls(void)
{
	SetSize(wxSize(800,600));

	wxBoxSizer* ItemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
	SetSizer(ItemBoxSizer2);

	wxBoxSizer* ItemBoxSizer3 = new wxBoxSizer(wxHORIZONTAL);
	ItemBoxSizer2->Add(ItemBoxSizer3, 1, wxGROW|wxALL, 5);

	wxStaticBox* ItemStaticBoxSizer4Static = new wxStaticBox(this, wxID_ANY, TEXT("FontPages"));
	wxStaticBoxSizer* ItemStaticBoxSizer4 = new wxStaticBoxSizer(ItemStaticBoxSizer4Static, wxHORIZONTAL);
	ItemBoxSizer3->Add(ItemStaticBoxSizer4, 0, wxGROW|wxALL, 5);

	// Viewport to show the font pages in
	ViewportHolder = new WxViewportHolder((wxWindow*)this,-1,TRUE);
	Viewport = GEngine->Client->CreateWindowChildViewport(this,(HWND)ViewportHolder->GetHandle());
	Viewport->CaptureJoystickInput(FALSE);
	ViewportHolder->SetViewport(Viewport);
	ViewportHolder->SetSize( wxSize(256 + 16 + (2 * PadSize), -1) );
	ItemStaticBoxSizer4->Add(ViewportHolder, 0, wxGROW|wxALL, 5);

	wxBoxSizer* ItemBoxSizer6 = new wxBoxSizer(wxVERTICAL);
	ItemStaticBoxSizer4->Add(ItemBoxSizer6, 0, wxALIGN_TOP|wxALL, 5);

	UpdateButton = new wxButton( this, ID_UPDATE, TEXT("Update"), wxDefaultPosition, wxDefaultSize, 0 );
	UpdateButton->Enable(FALSE);
	ItemBoxSizer6->Add(UpdateButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxButton* UpdateAll = new wxButton( this, ID_UPDATE_ALL, TEXT("UpdateAll"), wxDefaultPosition, wxDefaultSize, 0 );
	ItemBoxSizer6->Add(UpdateAll, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	ExportButton = new wxButton( this, ID_EXPORT, TEXT("Export"), wxDefaultPosition, wxDefaultSize, 0 );
	ExportButton->Enable(FALSE);
	ItemBoxSizer6->Add(ExportButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxButton* ItemButton9 = new wxButton( this, ID_EXPORT_ALL, TEXT("ExportAll"), wxDefaultPosition, wxDefaultSize, 0 );
	ItemBoxSizer6->Add(ItemButton9, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxButton* ItemButton10 = new wxButton( this, ID_PREVIEW, TEXT("Preview"), wxDefaultPosition, wxDefaultSize, 0 );
	// Not implemented yet
	//	ItemButton10->Enable(FALSE);
	ItemBoxSizer6->Add(ItemButton10, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

	wxBoxSizer* ItemBoxSizer13 = new wxBoxSizer(wxHORIZONTAL);
	ItemBoxSizer2->Add(ItemBoxSizer13, 0, wxALIGN_RIGHT|wxALL, 5);

	wxButton* ItemButton14 = new wxButton( this, ID_CLOSE, TEXT("Close"), wxDefaultPosition, wxDefaultSize, 0 );
	ItemBoxSizer13->Add(ItemButton14, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

	// Localize all of the windows.  Done before property window as to not corrupt the search panel
	FLocalizeWindow(this);

	wxBoxSizer* PropertiesContainerItemStaticBoxSizer = new wxBoxSizer(wxVERTICAL);
	ItemBoxSizer3->Add(PropertiesContainerItemStaticBoxSizer, 1, wxGROW|wxALL, 0);
	{
		wxStaticBox* FontPropertiesItemStaticBoxSizerStatic = new wxStaticBox(this, wxID_ANY, TEXT("FontProperties"));
		wxStaticBoxSizer* FontPropertiesItemStaticBoxSizer = new wxStaticBoxSizer(FontPropertiesItemStaticBoxSizerStatic, wxHORIZONTAL);
		PropertiesContainerItemStaticBoxSizer->Add(FontPropertiesItemStaticBoxSizer, 1, wxGROW|wxALL, 5);
		{
			// Add our font property window to the right panel
			FontPropertyWindow = new WxPropertyWindowHost;
			FontPropertyWindow->Create( this, this );
			FontPropertiesItemStaticBoxSizer->Add(FontPropertyWindow, 1, wxGROW|wxALL, 5);
		}

		wxStaticBox* TexturePropertiesItemStaticBoxSizerStatic = new wxStaticBox(this, wxID_ANY, TEXT("FontProperties_SelectedTextureProperties"));
		wxStaticBoxSizer* TexturePropertiesItemStaticBoxSizer = new wxStaticBoxSizer(TexturePropertiesItemStaticBoxSizerStatic, wxHORIZONTAL);
		PropertiesContainerItemStaticBoxSizer->Add(TexturePropertiesItemStaticBoxSizer, 1, wxGROW|wxALL, 5);
		{
			// Add our texture property window to the right panel
			TexturePropertyWindow = new WxPropertyWindowHost;
			TexturePropertyWindow->Create( this, this );
			TexturePropertiesItemStaticBoxSizer->Add(TexturePropertyWindow, 1, wxGROW|wxALL, 5);
		}
	}

}

/**
 * Cleans up any resources allocated by this instance
 */
WxFontPropertiesDlg::~WxFontPropertiesDlg()
{
	// Clean up preview window if that's currently visible
	if( PreviewWindowID != wxID_NONE )
	{
		WxFontPreviewDlg* PreviewWindow = static_cast< WxFontPreviewDlg* >( FindWindow( PreviewWindowID ) );
		if( PreviewWindow != NULL )
		{
			PreviewWindow->Destroy();
			PreviewWindowID = wxID_NONE;
		}
	}

	// Clean up the viewport
	GEngine->Client->CloseViewport(Viewport); 
	Viewport = NULL;
}

/**
 * Serializes our object references so they don't go away on us
 *
 * @param Ar the archive to serialize to
 */
void WxFontPropertiesDlg::Serialize(FArchive& Ar)
{
	Ar << Font << TGAExporter << Factory;
}

/**
 * Displays the dialog with the various font information supplied
 *
 * @param InShow the show/hide flag for the dialog
 * @param Font the font that is being modified
 */
bool WxFontPropertiesDlg::Show(bool InShow,UFont* InFont)
{
	// Assign which font we are working on
	Font = InFont;

	// Set the object that the property window is displaying
	FontPropertyWindow->SetObject(Font, EPropertyWindowFlags::NoFlags);

	// Now show the dialog
	return wxDialog::Show(InShow);
}

/**
 * Called when the dialog Close button was selected. Closes the modeless
 * window
 *
 * @param In the event being processed
 */
void WxFontPropertiesDlg::OnOK(wxCommandEvent& In)
{
	Destroy();
}

/**
 * Called when the dialog is closed
 */
void WxFontPropertiesDlg::OnClose( wxCloseEvent& In )
{
	// We're being closed
	Destroy();
}


/**
 * Updates the viewport with the specified scroll position
 *
 * @param InEvent the scroll event to process
 */
void WxFontPropertiesDlg::OnScroll( wxScrollEvent& InEvent )
{
	ViewportHolder->ScrollViewport(InEvent.GetPosition(),FALSE);
}

/**
 * Positions the child controls when the size changes
 *
 * @param In the size event to process
 */
void WxFontPropertiesDlg::OnSize(wxSizeEvent& In)
{
	if (Viewport != NULL)
	{
		// Force a redraw
		Viewport->Invalidate();
	}
}

/**
 * Common method for replacing a font page with a new texture
 *
 * @param PageNum the font page to replace
 * @param FileName the file to replace it with
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL WxFontPropertiesDlg::ImportPage(INT PageNum,const TCHAR* FileName)
{
	// Assume we failed
	UBOOL bSuccess = FALSE;
	TArray<BYTE> Data;
	// Read the file into an array
	if (appLoadFileToArray(Data,FileName))
	{
		// Make a const pointer for the API to be happy
		const BYTE* DataPtr = &Data(0);
		// Create the new texture
		// note RF_Public because font textures can be referenced directly by material expressions
		UTexture2D* NewPage = (UTexture2D*)Factory->FactoryCreateBinary(
			UTexture2D::StaticClass(),Font,NAME_None,RF_Public,NULL,TEXT("TGA"),
			DataPtr,DataPtr + Data.Num(),GWarn);
		if (NewPage != NULL)
		{
			UTexture2D* Texture = Font->Textures(PageNum);
			// Make sure the sizes are the same
			if (Texture->SizeX == NewPage->SizeX &&
				Texture->SizeY == NewPage->SizeY)
			{
				// Set the new texture's settings to match the old texture
				NewPage->CompressionNoAlpha = Texture->CompressionNoAlpha;
				NewPage->CompressionNone = Texture->CompressionNone;
				NewPage->MipGenSettings = Texture->MipGenSettings;
				NewPage->CompressionFullDynamicRange = Texture->CompressionFullDynamicRange;
				NewPage->CompressionNoAlpha = Texture->CompressionNoAlpha;
				NewPage->NeverStream = Texture->NeverStream;
				NewPage->CompressionSettings = Texture->CompressionSettings;
				NewPage->Filter = Texture->Filter;
				// Now compress the texture
				NewPage->Compress();
				// Replace the existing texture with the new one
				Font->Textures(PageNum) = NewPage;
				// Dirty the font's package and refresh the content browser to indicate the font's package needs to be saved post-update
				Font->MarkPackageDirty();
				GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageListUI|CBR_UpdateAssetListUI, Font));
			}
			else
			{
				FString Error = FString::Printf(
					TEXT("The updated image (%s) does not match the original's size"),
					FileName);
				// Tell the user the sizes mismatch
				wxMessageBox(wxString(*Error),
					wxString(TEXT("Texture Size Error")),wxICON_ERROR);
			}
			bSuccess = TRUE;
		}
	}
	return bSuccess;
}

/**
 * Updates the currently visible page with a newly imported texture
 *
 * @param In the event being processed
 */
void WxFontPropertiesDlg::OnUpdateFontPage(wxCommandEvent& In)
{
	if (CurrentSelectedPage > -1)
	{
		WxFileDialog OpenFileDialog(this, 
			*LocalizeUnrealEd("Import"),
			*LastPath,TEXT(""),
			TEXT("TGA Files (*.tga)|*.tga"),
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition);
		if (OpenFileDialog.ShowModal() == wxID_OK)
		{
			LastPath = OpenFileDialog.GetDirectory();
			// Use the common routine for importing the texture
			if (ImportPage(CurrentSelectedPage,OpenFileDialog.GetPath()) == FALSE)
			{
				FString Error = FString::Printf(TEXT("Failed to update the font page (%d) with texture (%s)"),
					CurrentSelectedPage,(const TCHAR*)OpenFileDialog.GetPath());
				// Show an error to the user
				wxMessageBox(wxString(*Error),
					wxString(TEXT("Font Page Import Error")),wxICON_ERROR);
			}
		}
		Viewport->Invalidate();
	}
}

/**
 * Updates all of the font pages by creating known names and
 * finding those textures in the specified directory
 *
 * @param In the event being processed
 */
void WxFontPropertiesDlg::OnUpdateAllFontPages(wxCommandEvent& In)
{
	// Open dialog so user can chose which directory to export to
	wxDirDialog ChooseDirDialog(this, 
		*FString::Printf( LocalizeSecure(LocalizeUnrealEd("Save_F"),*Font->GetName())),
		*LastPath);
	if (ChooseDirDialog.ShowModal() == wxID_OK)
	{
		LastPath = ChooseDirDialog.GetPath();
		// Try to import each file into the corresponding page
		for (INT Index = 0; Index < Font->Textures.Num(); Index++)
		{
			// Create a name for the file based off of the font name and
			// page number
			FString FileName = FString::Printf(TEXT("%s\\%s_Page_%d.tga"),
				*LastPath,*Font->GetName(),Index);
			if (ImportPage(Index,*FileName) == FALSE)
			{
				FString Error = FString::Printf(TEXT("Failed to update the font page (%d) with texture (%s)"),
					Index,*FileName);
				// Show an error to the user
				wxMessageBox(wxString(*Error),
					wxString(TEXT("Font Page Import Error")),wxICON_ERROR);
			}
		}
	}
	Viewport->Invalidate();
}

/**
 * Exports the currently visible page as a TGA file
 *
 * @param In the event being processed
 */
void WxFontPropertiesDlg::OnExport(wxCommandEvent& In)
{
	if (CurrentSelectedPage > -1)
	{
		// Open dialog so user can chose which directory to export to
		wxDirDialog ChooseDirDialog(this, 
			*FString::Printf( LocalizeSecure(LocalizeUnrealEd("Save_F"),*Font->GetName())),
			*LastPath);
		if (ChooseDirDialog.ShowModal() == wxID_OK)
		{
			LastPath = ChooseDirDialog.GetPath();
			// Create a name for the file based off of the font name and
			// page number
			FString FileName = FString::Printf(TEXT("%s\\%s_Page_%d.tga"),
				*LastPath,*Font->GetName(),CurrentSelectedPage);
			// Create that file with the texture data
			UExporter::ExportToFile(Font->Textures(CurrentSelectedPage),
				TGAExporter,*FileName,FALSE);
		}
		Viewport->Invalidate();
	}
}

/**
 * Exports all font pages to TGA
 *
 * @param In the event being processed
 */
void WxFontPropertiesDlg::OnExportAll(wxCommandEvent& In)
{
	// Open dialog so user can chose which directory to export to
	wxDirDialog ChooseDirDialog(this, 
		*FString::Printf( LocalizeSecure(LocalizeUnrealEd("Save_F"),*Font->GetName())),
		*LastPath);
	if (ChooseDirDialog.ShowModal() == wxID_OK)
	{
		LastPath = ChooseDirDialog.GetPath();
		// Loop through exporting each file to the specified directory
		for (INT Index = 0; Index < Font->Textures.Num(); Index++)
		{
			// Create a name for the file based off of the font name and
			// page number
			FString FileName = FString::Printf(TEXT("%s\\%s_Page_%d.tga"),
				*LastPath,*Font->GetName(),Index);
			// Create that file with the texture data
			UExporter::ExportToFile(Font->Textures(Index),TGAExporter,
				*FileName,FALSE);
		}
	}
}


/**
 * Opens the preview dialog which allows the artist to view a string
 * of text using the font as they modify it
 *
 * @param In the event being processed
 */
void WxFontPropertiesDlg::OnPreview(wxCommandEvent& In)
{
	WxFontPreviewDlg* PreviewWindow = NULL;
	if( PreviewWindowID != wxID_NONE )
	{
		// Grab existing window
		PreviewWindow = static_cast< WxFontPreviewDlg* >( FindWindow( PreviewWindowID ) );
		if( PreviewWindow != NULL )
		{
			// Preview window for this font is already visible, so activate it
			PreviewWindow->SetFocus();
		}
		else
		{
			// Window doesn't exist.  User probably closed it.
			PreviewWindowID = wxID_NONE;
		}
	}

	if( PreviewWindow == NULL )
	{
		// Preview window isn't visible yet, so go ahead and create it
		WxFontPreviewDlg* PreviewWindow = new WxFontPreviewDlg( this, Font, FColor(255,255,255), FColor(0,0,0) );
		PreviewWindow->Show();

		// Store preview window ID so we can close the window later if we need to
		PreviewWindowID = PreviewWindow->GetId();
	}
}

/**
 * Draws all of the pages of the font in the viewport. Sets the scroll
 * bars as needed
 *
 * @param Viewport the viewport being drawn into
 * @param RI the render interface to draw with
 */
void WxFontPropertiesDlg::Draw(FViewport* Viewport,FCanvas* Canvas)
{
	Clear(Canvas,FColor(0,0,0));
	INT TotalHeight = 0;
	INT ScrollBarPos = ViewportHolder->GetScrollThumbPos();
	// Get the location of the scrollbar
	INT YPos = PadSize - ScrollBarPos;
	INT LastDrawnYPos = YPos;
	INT XPos = PadSize;
	// Loop through the pages drawing them if they are visible
	for (INT Index = 0; Index < Font->Textures.Num(); Index++)
	{
		UTexture* Texture = Font->Textures(Index);
		// Make sure it's a valid texture page. Could be null if the user
		// is editing things
		if (Texture != NULL)
		{
			// Get the rendering info for this object
			FThumbnailRenderingInfo* RenderInfo =
				GUnrealEd->GetThumbnailManager()->GetRenderingInfo(Texture);
			// If there is an object configured to handle it, draw the thumbnail
			if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
			{
				DWORD Width, Height;
				// Figure out the size we need
				RenderInfo->Renderer->GetThumbnailSize(Texture,TPT_Plane,1.f,
					Width,Height);
				// Scale the height by the width to match our viewport
				if (Width > 256)
				{
					Height *= 256;
					Height /= Width;
					Width = 256;
				}
				// Don't draw if we are outside of our range
				if (YPos + Height + PadSize >= 0 && YPos <= (INT)Viewport->GetSizeY())
				{
					// If hit testing, draw a tile instead
					if (Canvas->IsHitTesting())
					{
						Canvas->SetHitProxy(new HObject(Texture));
						// Draw a simple tile
						DrawTile(Canvas,XPos,YPos,Width,Height,0.0f,0.0f,1.f,1.f,
							FLinearColor::White,GEditor->BkgndHi->Resource);
						Canvas->SetHitProxy(NULL);
					}
					// Otherwise draw the font texture
					else
					{
						// Draw a selected background
						if (Texture->IsSelected())
						{
							DrawTile(Canvas,XPos - PadSize,YPos - PadSize,
								Width + (PadSize * 2),Height + (PadSize * 2),
								0.0f,0.0f,1.f,1.f,FLinearColor::White,
								GEditor->BkgndHi->Resource);
						}

						// Draw the font texture (with alpha blending enabled)
						DrawTile(Canvas,XPos,YPos,Width,Height,0.f,0.f,1.f,1.f,FLinearColor::White,
							Texture->Resource,TRUE);
					}
				}
				// Update our total height and current draw position
				YPos += Height + PadSize;
				TotalHeight += Height + PadSize;
			}
		}
	}
	// Make room for the bottom selected edge
	TotalHeight += PadSize;
	// Update where the scroll bar should be
	ViewportHolder->UpdateScrollBar(ScrollBarPos,TotalHeight);
	// Determine our scroll speed
	ScrollSpeed = TotalHeight / (Font->Textures.Num() * 2);
}
/**
 * Handles the key events for the thumbnail viewport. It only supports
 * scrolling.
 */
UBOOL WxFontPropertiesDlg::InputKey(FViewport* Viewport,INT,FName Key,
	EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	// Hide and lock mouse cursor if we're capturing mouse input
	Viewport->ShowCursor( !Viewport->HasMouseCapture() );
	Viewport->LockMouseToWindow( Viewport->HasMouseCapture() );

	// If we are scrolling and the we've released the button, stop scrolling
	if (bIsScrolling == TRUE && Key == KEY_LeftMouseButton && Event == IE_Released)
	{
		bIsScrolling = FALSE;
		Viewport->Invalidate();
	}
	// Skip this if we are scrolling
	else if (bIsScrolling == FALSE)
	{
		if((Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton) && 
			Event == IE_Pressed)
		{
			const INT HitX = Viewport->GetMouseX();
			const INT HitY = Viewport->GetMouseY();
			// See if we hit something
			HHitProxy* HitResult = Viewport->GetHitProxy(HitX,HitY);
			if (HitResult)
			{
				if (HitResult->IsA(HObject::StaticGetType()))
				{
					// Get the object that was hit
					UObject* HitObject = ((HObject*)HitResult)->Object;
					if (HitObject)
					{
						// Turn off all others and set it as selected
						if ( HitObject->IsA(AActor::StaticClass()) )
						{
							warnf(TEXT("WxFontPropertiesDlg::InputKey : selecting actor!"));
							GEditor->GetSelectedActors()->DeselectAll();
							GEditor->GetSelectedActors()->Select(HitObject);
						}
						else
						{
							debugf(TEXT("WxFontPropertiesDlg::InputKey : selecting object!"));
							GEditor->GetSelectedObjects()->DeselectAll();
							GEditor->GetSelectedObjects()->Select(HitObject);
						}
						// Update our internal state for selected page
						// buttons, etc.
						UpdateSelectedPage(HitObject);
					}
				}
			}
			bIsScrolling = TRUE;
			// Force a redraw
			Viewport->Invalidate();
			Viewport->InvalidateDisplay();
		}
		// Did they scroll using the mouse wheel?
		else if (Key == KEY_MouseScrollUp)
		{
			ViewportHolder->ScrollViewport(-ScrollSpeed);
		}
		// Did they scroll using the mouse wheel?
		else if (Key == KEY_MouseScrollDown)
		{
			ViewportHolder->ScrollViewport(ScrollSpeed);
		}
	}
	return TRUE;
}

/**
 * Updates the scrolling location based on the mouse movement
 */
UBOOL WxFontPropertiesDlg::InputAxis(FViewport* Viewport,INT,FName Key,
	FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad)
{
	// If we are scrolling, update the scroll position based upon the movement
	if (bIsScrolling == TRUE)
	{
		const FLOAT GRAB_SCROLL_SPEED = 5.0f;
		ViewportHolder->ScrollViewport(-appTrunc(Delta * GRAB_SCROLL_SPEED));
	}
	return TRUE;
}


/**
 * Handles an object change notification. Forces a redraw
 *
 * @param Ignored
 * @param Ignored
 */
void WxFontPropertiesDlg::NotifyPostChange(void*,UProperty*)
{
	Viewport->Invalidate();
}

/**
 * Determines which texture page was selected. Updates the buttons
 * accordingly.
 *
 * @param SelectedObject the newly selected object
 */
void WxFontPropertiesDlg::UpdateSelectedPage(UObject* SelectedObject)
{
	// Default to non-selected
	CurrentSelectedPage = -1;

	// Search through the font's texture array seeing if this is a match
	for (INT Index = 0;	Index < Font->Textures.Num() && CurrentSelectedPage == -1; Index++)
	{
		// See if the pointers match
		if (Font->Textures(Index) == SelectedObject)
		{
			CurrentSelectedPage = Index;
			break;
		}
	}

	// Update the texture property window
	if( TexturePropertyWindow != NULL )
	{
		TexturePropertyWindow->SetObject(
			( CurrentSelectedPage > -1 ) ? Font->Textures( CurrentSelectedPage ) : NULL,
			EPropertyWindowFlags::Sorted);
	}

	// Now update our buttons
	UpdateButton->Enable(CurrentSelectedPage > -1);
	ExportButton->Enable(CurrentSelectedPage > -1);
}

/**
 * Handles a focus change event
 *
 * @param In the event to process
 */
void WxFontPropertiesDlg::OnFocus(wxFocusEvent& In)
{
	if (Font != NULL)
	{
		// Just update our state
		UpdateSelectedPage( GEditor->GetSelectedObjects()->GetTop<UTexture>() );
	}
}

BEGIN_EVENT_TABLE(WxFontPropertiesDlg::WxFontPreviewDlg, wxDialog)
	EVT_CLOSE( WxFontPropertiesDlg::OnClose )
	// Button events
	EVT_BUTTON( ID_CLOSE, WxFontPropertiesDlg::WxFontPreviewDlg::OnCloseButton )
	EVT_BUTTON( ID_FOREGROUND, WxFontPropertiesDlg::WxFontPreviewDlg::OnForeground )
	EVT_BUTTON( ID_BACKGROUND, WxFontPropertiesDlg::WxFontPreviewDlg::OnBackground )
	// Events as they type
	EVT_TEXT( ID_PREVIEW_EDIT, WxFontPropertiesDlg::WxFontPreviewDlg::OnTextChange )
END_EVENT_TABLE()

/**
 * Builds the preview dialog
 *
 * @param Parent the parent window of the dialog
 * @param InFont the font to render the text with in the preview window
 * @param InForegroundColor the text color
 * @param InBackgroundColor the color to clear the preview to
 */
WxFontPropertiesDlg::WxFontPreviewDlg::WxFontPreviewDlg(wxWindow* Parent,
	UFont* InFont,FColor InForegroundColor,FColor InBackgroundColor) :
	wxDialog(Parent,-1,wxString(TEXT("FontPreview_WindowCaption"))),
	Font(InFont),
	ForegroundColor(InForegroundColor),
	BackgroundColor(InBackgroundColor)
{
	CreateControls();

	// Localize all of the windows
	FLocalizeWindow( this );
}

/**
 * Cleans up the allocated resources
 */
WxFontPropertiesDlg::WxFontPreviewDlg::~WxFontPreviewDlg(void)
{
	// Clean up the viewport
	GEngine->Client->CloseViewport(Viewport); 
	Viewport = NULL;
}

/**
 * Creates all of the child controls
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::CreateControls(void)
{
	// Fixed window size in pixels
	SetSize( wxSize( 400, 300 ) );

	// Vertical sizer for Preview border and Close button
    wxBoxSizer* ItemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    SetSizer( ItemBoxSizer2 );

	{
		wxStaticBox* ItemStaticBoxSizer3Static = new wxStaticBox(this, wxID_ANY, TEXT( "FontPreview_PreviewBorder" ) );
		{
			wxStaticBoxSizer* ItemStaticBoxSizer3 = new wxStaticBoxSizer(ItemStaticBoxSizer3Static, wxHORIZONTAL);
			ItemBoxSizer2->Add(ItemStaticBoxSizer3, 1, wxGROW|wxALL, 5);

			{
				wxBoxSizer* ItemBoxSizer4 = new wxBoxSizer(wxVERTICAL);
				ItemStaticBoxSizer3->Add(ItemBoxSizer4, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);

				{
					// Viewport to show the text rendered using the font
					ViewportHolder = new WxViewportHolder((wxWindow*)this,-1,FALSE);
					Viewport = GEngine->Client->CreateWindowChildViewport(this,(HWND)ViewportHolder->GetHandle());
					Viewport->CaptureJoystickInput(FALSE);
					ViewportHolder->SetViewport(Viewport);
					ViewportHolder->SetSize( wxSize(-1,112) );
  					ItemBoxSizer4->Add(ViewportHolder, 1, wxGROW|wxALL, 5);

					PreviewTextControl = new wxTextCtrl( this, ID_PREVIEW_EDIT,
						TEXT( "FontPreview_DefaultText" ),
						wxDefaultPosition, wxDefaultSize, 0 );
					ItemBoxSizer4->Add(PreviewTextControl, 0, wxGROW|wxALL, 5);


					wxBoxSizer* ItemBoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
					ItemBoxSizer4->Add(ItemBoxSizer7, 0, wxALIGN_LEFT|wxALL, 5);

					{
						wxButton* ItemButton8 = new wxButton( this, ID_FOREGROUND, TEXT("FontPreview_ForegroundButton"), wxDefaultPosition, wxDefaultSize, 0 );
						ItemBoxSizer7->Add(ItemButton8, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);


						ForegroundPanel = new wxPanel( this, ID_FOREGROUND_PANEL, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
						// Make the text match the specified color
						ForegroundPanel->SetBackgroundColour(wxColour(ForegroundColor.R,ForegroundColor.G,ForegroundColor.B));
						ItemBoxSizer7->Add(ForegroundPanel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);


						wxButton* ItemButton9 = new wxButton( this, ID_BACKGROUND, TEXT("FontPreview_BackgroundButton"), wxDefaultPosition, wxDefaultSize, 0 );
						ItemBoxSizer7->Add(ItemButton9, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);


						BackgroundPanel = new wxPanel( this, ID_BACKGROUND_PANEL, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxTAB_TRAVERSAL );
						// Make the text match the specified color
						BackgroundPanel->SetBackgroundColour(wxColour(BackgroundColor.R,BackgroundColor.G,BackgroundColor.B));
						ItemBoxSizer7->Add(BackgroundPanel, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
					}
				}
			}
		}

		wxBoxSizer* ItemBoxSizer10 = new wxBoxSizer(wxHORIZONTAL);
		ItemBoxSizer2->Add(ItemBoxSizer10, 0, wxALIGN_RIGHT|wxALL, 5);

		{
			wxButton* ItemButton11 = new wxButton( this, ID_CLOSE, TEXT( "FontPreview_CloseButton" ), wxDefaultPosition, wxDefaultSize, 0 );
			ItemBoxSizer10->Add(ItemButton11, 1, wxALIGN_CENTER_VERTICAL|wxALL, 5);
		}
	}
}


/**
 * Called when the dialog is closed
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::OnClose( wxCloseEvent& In )
{
	// We're being closed
	Destroy();
}


/**
 * Ends the dialog
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::OnCloseButton(wxCommandEvent& In)
{
	Destroy();
}

/**
 * Common routine for letting the user select a color choice
 *
 * @param Color the current value of the color that is being changed
 *
 * @return The newly selected color or the original if no change
 */
FColor WxFontPropertiesDlg::WxFontPreviewDlg::SelectColor(FColor Color)
{
	wxColourData Data;
	Data.SetChooseFull(TRUE);
	// Set all 16 custom colors to our color
	for (INT Index = 0; Index < 16; Index++)
	{
		Data.SetCustomColour(Index,
			wxColour(Color.R,Color.G,Color.B));
	}
	// Create the dialog with our custom color set
	wxColourDialog Dialog(this,&Data);
	if (Dialog.ShowModal() == wxID_OK)
	{
		// Get the data that was changed
		wxColourData OutData = Dialog.GetColourData();
		// Now get the resultant color
		wxColour OutColor = OutData.GetColour();
		// Convert to our data type
		Color.R = OutColor.Red();
		Color.G = OutColor.Green();
		Color.B = OutColor.Blue();
	}
	return Color;
}

/**
 * Pops up a color picker and sets the foreground color
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::OnForeground(wxCommandEvent& In)
{
	// Pick a new color
	ForegroundColor = SelectColor(ForegroundColor);
	// And set the panel's with that color
	ForegroundPanel->SetBackgroundColour(
		wxColour(ForegroundColor.R,ForegroundColor.G,ForegroundColor.B));
	ForegroundPanel->Refresh();
	// Force a redraw
	Viewport->Invalidate();
}

/**
 * Pops up a color picker and sets the background color
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::OnBackground(wxCommandEvent& In)
{
	// Show the color picker dialog
	BackgroundColor = SelectColor(BackgroundColor);
	// Set the window's color to the background color
	BackgroundPanel->SetBackgroundColour(
		wxColour(BackgroundColor.R,BackgroundColor.G,BackgroundColor.B));
	BackgroundPanel->Refresh();
	// Force a redraw
	Viewport->Invalidate();
}

/**
 * Updates the displayed text when the text in the control changes
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::OnTextChange(wxCommandEvent& In)
{
	if (Viewport != NULL)
	{
		// Force a redraw
		Viewport->Invalidate();
	}
}

/**
 * Draws the preview text in the viewport using the specified font
 *
 * @param Viewport the viewport being drawn into
 * @param RI the render interface to draw with
 */
void WxFontPropertiesDlg::WxFontPreviewDlg::Draw(FViewport* Viewport,
	FCanvas* Canvas)
{
	// Erase with our background color
	Clear(Canvas,BackgroundColor);
	// And draw the text with the foreground color
	DrawString(Canvas,PadSize,PadSize,(const TCHAR*)PreviewTextControl->GetValue(),
		Font,FLinearColor(ForegroundColor));
}
