/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef _FONT_PROPERTIES_DLG_H
#define _FONT_PROPERTIES_DLG_H

#include "TrackableWindow.h"

class UTextureFactory;

/**
 * This dialog shows the properties for a font, its texture pages, and allows
 * the importing/exporting of those pages.
 */
class WxFontPropertiesDlg :
	public WxTrackableDialog,
	public FViewportClient,
	public FNotifyHook,
	public FSerializableObject
{
public:
	/**
	 * Creates the property window and child viewport objects
	 */
	WxFontPropertiesDlg();
	
	/**
	 * Cleans up any resources allocated by this instance
	 */
	~WxFontPropertiesDlg();

	/**
	 * Displays the dialog with the various font information supplied
	 *
	 * @param InShow the show/hide flag for the dialog
	 * @param Font the font that is being modified
	 */
	bool Show(const bool InShow,UFont* Font);
private:
	using wxDialog::Show;		// Hide parent implementation
public:

// FViewportClient interface.
	/**
	 * Draws all of the pages of the font in the viewport. Sets the scroll
	 * bars as needed
	 *
	 * @param Viewport the viewport being drawn into
	 * @param RI the render interface to draw with
	 */
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	/**
	 * Handles the key events for the thumbnail viewport. It only supports
	 * scrolling.
	 */
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,
		EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	/**
	 * Updates the scrolling location based on the mouse movement
	 */
	virtual UBOOL InputAxis(FViewport* Viewport,INT ControllerId,FName Key,
		FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad=FALSE);

// FNotify interface.
	/**
	 * Handles an object change notification
	 *
	 * @param Ignored
	 * @param Ignored
	 */
	virtual void NotifyPostChange(void*,UProperty*);

// FSerializableObject interface
	/**
	 * Serializes our object references so they don't go away on us
	 *
	 * @param Ar the archive to serialize to
	 */
	void Serialize(FArchive& Ar);

private:
	/** Wx window ID of preview window, or -1 if it's not visible right now */
	wxWindowID PreviewWindowID;

	/**
	 * The font being manipulated
	 */
	UFont* Font;
	/**
	 * The wrapper window around the viewport
	 */
	WxViewportHolder* ViewportHolder;
	/**
	 * Whether the viewport is currently in the scrolling state or not
	 */
	UBOOL bIsScrolling;
	/**
	 * The scrolling speed for the viewport
	 */
	INT ScrollSpeed;
	/**
	 * The viewport the font information will be drawn in
	 */
	FViewport* Viewport;
	/**
	 * The property window that displays the font properties
	 */
	WxPropertyWindowHost* FontPropertyWindow;
	/**
	 * The property window that displays the properties for the currently selected font texture
	 */
	WxPropertyWindowHost* TexturePropertyWindow;
	/**
	 * The last path exported to or from
	 */
	static FString LastPath;
	/**
	 * Size to pad images by
	 */
	const static INT PadSize = 4;
	/**
	 * So we can enable/disable the button upon selection
	 */
	wxButton* UpdateButton;
	/**
	 * So we can enable/disable the button upon selection
	 */
	wxButton* ExportButton;
	/**
	 * Which font page is currently selected
	 */
	INT CurrentSelectedPage;
	/**
	 * The exporter to use for all font page exporting
	 */
	UTextureExporterTGA* TGAExporter;
	/**
	 * The factory to create updated pages with
	 */
	UTextureFactory* Factory;
	/**
	 * Enum for declaring child ids
	 */
	enum EChildIDs
	{
		ID_UPDATE = 2000,
		ID_UPDATE_ALL,
		ID_EXPORT,
		ID_EXPORT_ALL,
		ID_PREVIEW,
		ID_CLOSE
	};

	/**
	 * Creates all of the child controls for this dialog
	 */
	void CreateControls(void);
	/**
	 * Determines which texture page was selected. Updates the buttons
	 * accordingly.
	 *
	 * @param SelectedObject the newly selected object
	 */
	void UpdateSelectedPage(UObject* SelectedObject);
	/**
	 * Common method for replacing a font page with a new texture
	 *
	 * @param PageNum the font page to replace
	 * @param FileName the file to replace it with
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	UBOOL ImportPage(INT PageNum,const TCHAR* FileName);
	/**
	 * Called when the dialog Close button was selected. Closes the modeless
	 * window
	 *
	 * @param In the event being processed
	 */
	void OnOK(wxCommandEvent& In);
	/**
	 * Called when the dialog is closed.  Closes the modeless window
	 */
	void OnClose( wxCloseEvent& In );
	/**
	 * Updates the currently visible page with a newly imported texture
	 *
	 * @param In the event being processed
	 */
	void OnUpdateFontPage(wxCommandEvent& In);
	/**
	 * Updates all of the font pages by creating known names and
	 * finding those textures in the specified directory
	 *
	 * @param In the event being processed
	 */
	void OnUpdateAllFontPages(wxCommandEvent& In);
	/**
	 * Exports the currently visible page as a TGA file
	 *
	 * @param In the event being processed
	 */
	void OnExport(wxCommandEvent& In);
	/**
	 * Exports all font pages to TGA
	 *
	 * @param In the event being processed
	 */
	void OnExportAll(wxCommandEvent& In);
	/**
	 * Opens the preview dialog which allows the artist to view a string
	 * of text using the font as they modify it
	 *
	 * @param In the event being processed
	 */
	void OnPreview(wxCommandEvent& In);
	/**
	 * Positions the child controls when the size changes
	 *
	 * @param In the size event to process
	 */
	void OnSize(wxSizeEvent& In);
	/**
	 * Updates the viewport with the specified scroll position
	 *
	 * @param InEvent the scroll event to process
	 */
	void OnScroll( wxScrollEvent& InEvent );
	/**
	 * Handles a focus change event
	 *
	 * @param In the event to process
	 */
	void OnFocus(wxFocusEvent& In);

	DECLARE_EVENT_TABLE()

	/**
	 * This dialog allows you to preview text using your modified font
	 */
	class WxFontPreviewDlg :
		public wxDialog,
		public FViewportClient
	{
	public:
		/**
		 * Builds the preview dialog
		 */
		WxFontPreviewDlg(wxWindow* Parent,UFont* Font,FColor ForegroundColor,
			FColor BackgroundColor);

		/**
		 * Cleans up it's viewport
		 */
		~WxFontPreviewDlg(void);

		/**
		 * Gets the colors the user selected
		 */
		FColor GetForegroundColor(void)
		{
			return ForegroundColor;
		}
	
		/**
		 * Gets the colors the user selected
		 */
		FLinearColor GetBackgroundColor(void)
		{
			return BackgroundColor;
		}

// FViewportClient interface.
		/**
		 * Draws all of the pages of the font in the viewport. Sets the scroll
		 * bars as needed
		 *
		 * @param Viewport the viewport being drawn into
		 * @param RI the render interface to draw with
		 */
		virtual void Draw(FViewport* Viewport,FCanvas* Canvas);

	private:
		/**
		 * The color to render the text
		 */
		FColor ForegroundColor;
		/**
		 * The background color to use clear the viewport with
		 */
		FColor BackgroundColor;
		/**
		 * The font being manipulated
		 */
		UFont* Font;
		/**
		 * The wrapper window around the viewport
		 */
		WxViewportHolder* ViewportHolder;
		/**
		 * The viewport the font information will be drawn in
		 */
		FViewport* Viewport;
		/**
		 * The edit window for changing the preview text
		 */
		wxTextCtrl* PreviewTextControl;
		/**
		 * The panel that shows the foreground color
		 */
		wxPanel* ForegroundPanel;
		/**
		 * The panel that shows the background color
		 */
		wxPanel* BackgroundPanel;
		/**
		 * Enum for the child control ids
		 */
		enum EChildControlIDs
		{
			ID_FOREGROUND,
			ID_FOREGROUND_PANEL,
			ID_BACKGROUND,
			ID_BACKGROUND_PANEL,
			ID_CLOSE,
			ID_PREVIEW_EDIT
		};

		/**
		 * Creates all of the child controls
		 */
		void CreateControls(void);

		/**
		 * Called when the dialog is closed
		 */
		void OnClose( wxCloseEvent& In );

		/**
		 * Closes the dialog
		 */
		void OnCloseButton(wxCommandEvent& In);
		/**
		 * Common routine for letting the user select a color choice
		 *
		 * @param Color the current value of the color that is being changed
		 *
		 * @return The newly selected color or the original if no change
		 */
		FColor SelectColor(FColor Color);
		/**
		 * Pops up a color picker and sets the foreground color
		 */
		void OnForeground(wxCommandEvent& In);
		/**
		 * Pops up a color picker and sets the background color
		 */
		void OnBackground(wxCommandEvent& In);
		/**
		 * Updates the displayed text when the text in the control changes
		 */
		void OnTextChange(wxCommandEvent& In);
	
		DECLARE_EVENT_TABLE()
	};
};

#endif
