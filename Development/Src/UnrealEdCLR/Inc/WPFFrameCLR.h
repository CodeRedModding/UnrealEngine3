/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __WPF_FRAME_H__
#define __WPF_FRAME_H__


#ifdef _MSC_VER
	#pragma once
#endif

#ifndef __cplusplus_cli
	#error "This file must be compiled as managed code using the /clr compiler option."
#endif

#include "WPFWindowWrapperCLR.h"

using namespace System;
using namespace System::Windows::Controls;

/**
 * Helper structure to pass in initialization parameters
 */
ref struct WPFFrameInitStruct
{
	WPFFrameInitStruct()
	{
		PositionX = 0;
		PositionY = 0;

		bCenterWindow = TRUE;
		bShowInTaskBar = FALSE;
		bShowCloseButton = TRUE;
		bResizable = FALSE;
		bForceToFront = FALSE;
		bUseWxDialog = TRUE;
		bUseSaveLayout = TRUE;
	}

	/**The String that will show up in the WPFFrame Title Bar.  Needs to be localized*/
	String^ WindowTitle;
	/**When specified, will cause a "Help" icon to show up in the WPF Frame Title Bar.  When clicked appLaunchURL will bring up the appropriate help page.*/
	/**Presently disabled*/
	String^ WindowHelpURL;

	/**X Position of the window.  
	 * The first run, this will be used unless bCenterWindow is enabled.
	 * If saving layout is enabled, this will disregarded after the first run
	 */
	INT PositionX;
	/**Y Position of the window.  
	 * The first run, this will be used unless bCenterWindow is enabled.
	 * If saving layout is enabled, this will disregarded after the first run
	 */
	INT PositionY;

	/**Should the window be centered upon first layout
	 * If saving layout is enabled, this will be disregarded after first run
	 */
	UBOOL bCenterWindow;
	/**Whether this window should have a entry in the taskbar or just be childed to the app*/
	UBOOL bShowInTaskBar;

	/**Should the WPFFrame Title bar's close button be visible.*/
	UBOOL bShowCloseButton;
	/**Should the WPFFrame be resizeable by the user.*/
	UBOOL bResizable;
	/**Should this window always show up on top*/
	UBOOL bForceToFront;
	//should be true for all editor tasks, but NOT true for commandlets (perforce user login window)
	UBOOL bUseWxDialog;
	/**Whether or not to auto-save/load the layout of the WPF Frame*/
	UBOOL bUseSaveLayout;
};

/** Forward declaration for Panel Parenting */
ref class MWPFFrame;

/**
 * Panel class to be docked into a WPF Frame
 */
ref class MWPFPanel : public ContentControl, public ComponentModel::INotifyPropertyChanged
{
public:
	MWPFPanel(String^ XamlFileName);

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
	virtual event ComponentModel::PropertyChangedEventHandler^ PropertyChanged;

	/**
	 * Function to set the parent frame when needed (should only be for dialogs)
	 * Virtual so panels can listen for the frames events (close, etc)
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame)
	{
		ParentFrame = InParentFrame;
	}

	/**
	 * Gets the associated parent frame
	 * Virtual so panels can listen for the frames events (close, etc)
	 */
	MWPFFrame^ GetParentFrame (void)
	{
		return ParentFrame;
	}

	//for custom work that can't be until the layout is complete
	virtual void PostLayout (void) {}

	/** Refresh all properties */
	void RefreshAllProperties()
	{
		// Pass null here which tells WPF that any or all properties may have changed
		OnPropertyChanged( nullptr );
	}


	/** Called when a property has changed */
	virtual void OnPropertyChanged( String^ Info )
	{
		PropertyChanged( this, gcnew ComponentModel::PropertyChangedEventArgs( Info ) );
	}

protected:
	MWPFFrame^ ParentFrame;
};

//forward declaration
class WxWPFFrame;
/**
 * Generic frame class for WPF top-level windows
 */

ref class MWPFFrame : public MWPFWindowWrapper
{
public:
	/**
	 * Sets up a top-level WPF Window, but does not finalize.  Finalize should be called when all child windows have been appended
	 * @param InParentWindow - The parent window
	 * @param InSettings - The initialization structure for the window
	 * @param InContextName - Used for saving windows layout
	 */
	MWPFFrame(wxWindow* InParentWindow, WPFFrameInitStruct^ InSettings, const FString& InContextName);

	/** Virtual Destructor*/
	virtual ~MWPFFrame(void);

	/** 
	 * Makes the window modal until the modal has been released 
	 * @return - The return code from the dialog (typically given by the child panel that has closed the window
	 */
	INT SetContentAndShowModal(MWPFPanel^ InPanel, const INT InDefaultDialogResult);

	/** 
	 * Shows the window (modeless)
	 */
	void SetContentAndShow(MWPFPanel^ InPanel);

	/** 
	 * Shows the window (modeless)
	 */
	void SetContentAndShowComposite(ContentControl^ InControl, List<MWPFPanel^>^ InPaneList);

	/** Used to close based on child panel buttons */
	void Close(const INT InDialogResult);

	/**Brings wx dialog to the front*/
	void Raise (void);
	/**
	 * Forces a save of the current window layout
	 */
	void SaveLayout (void);

	/** Accessor to top level event border that will catch all events from child windows if needed */
	Border^ GetEventBorder (void) { return EventBorder; }

	/** Called when the dialog is being canceled*/
	void CloseButtonClicked(Object^ Owner, RoutedEventArgs^ Args);

	/** Called when the help button is clicked*/
	void HelpButtonClicked(Object^ Owner, RoutedEventArgs^ Args);

private:

	/**
	 * Preps the window to be shown or shown modal
	 */
	void LayoutWindow(ContentControl^ InPanel);

	/** Called when the HwndSource receives a windows message */
	IntPtr FrameMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled );

	/** If true, this frame should be shown modally*/
	UBOOL bModalComplete;
	/** The return value set by a content panel */
	INT DialogResult;

	/**Border that can be a catch all for events in all child windows*/
	Border^ EventBorder;

	/**URL to launch when the "help" button is clicked*/
	String^ HelpURL;
	/**Name used for saving and loading the position of the window*/
	String^ ContextName;

	//Wrapper WxDialog used for making windows modal (to enforce modal with the rest of the editor)
	wxDialog* WxDialog;

	/**Should the window be centered upon first layout
	 * If saving layout is enabled, this will be disregarded after first run
	 */
	UBOOL bCenterWindow;
	/**Should the WPFFrame be resizable by the user.*/
	UBOOL bResizeable;
	/**Whether or not to auto-save/load the layout of the WPF Frame*/
	UBOOL bUseSaveLayout;
};

#endif // __WPF_FRAME_H__

