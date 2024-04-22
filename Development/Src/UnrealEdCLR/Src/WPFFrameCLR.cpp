/*================================================================================
	WPFFrameCLR.cpp: Code for interfacing C++ with C++/CLI and WPF
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEdCLR.h"

#include "InteropShared.h"
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

//-------------------------------------
//MWPFPanel
//-------------------------------------

/**
 * Panel class to be docked into a WPF Frame
 */
MWPFPanel::MWPFPanel(String^ InXamlFileName)
{
	String^ XamlPathAndFileName = String::Format( "{0}WPF\\Controls\\{1}", CLRTools::ToString( GetEditorResourcesDir() ), InXamlFileName);
	Visual^ InternalVisual = CLRTools::CreateVisualFromXaml(XamlPathAndFileName);

	Content = safe_cast< UIElement^>(InternalVisual);

	ParentFrame = nullptr;
}

//-------------------------------------
//MWPFFrame
//-------------------------------------


/**
 * Constructor
 * Sets up a top-level WPF Window, but does not finalize.  Finalize should be called when all child windows have been appended
 * @param InParentWindow - The parent window
 * @param InSettings - The initialization structure for the window
 * @param InContextName - Used for saving windows layout
 */
MWPFFrame::MWPFFrame(wxWindow* InParentWindow, WPFFrameInitStruct^ InSettings, const FString& InContextName)
{
	ContextName = CLRTools::ToString(InContextName);

	HWND ParentWindowHandle = NULL;

	//Create the wx wrapper
	if (InSettings->bUseWxDialog) 
	{
		WxDialog = new wxDialog();
		WxDialog->Create(InParentWindow, wxID_ANY, TEXT("WPFFrame"), wxDefaultPosition, wxDefaultSize, 0);
		ParentWindowHandle = (HWND)WxDialog->GetHandle();//(HWND)GApp->EditorFrame->GetHandle();
	}
	else
	{
		WxDialog = NULL;
	}

	UBOOL bAllowFrameDraw = FALSE;
	CreateWindow((HWND)ParentWindowHandle,
				TEXT("WPFFrame"), 
				TEXT("EmptyFrame.xaml"), 
				InSettings->PositionX, 
				InSettings->PositionY, 
				0,
				0,
				0,
				InSettings->bShowInTaskBar,
				bAllowFrameDraw,
				InSettings->bForceToFront);

	//init the array for appended panels
	bModalComplete = FALSE;

	//Set the title
	Label^ TitleLabel = safe_cast< Label^ >( LogicalTreeHelper::FindLogicalNode( InteropWindow->RootVisual, "TitleLabel" ) );
	TitleLabel->Content = InSettings->WindowTitle;

	//Setup close button
	Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( InteropWindow->RootVisual, "TitleBarCloseButton" ) );
	CloseButton->Click += gcnew RoutedEventHandler( this, &MWPFFrame::CloseButtonClicked );
	if (!InSettings->bShowCloseButton)
	{
		CloseButton->Visibility = Visibility::Hidden;
	} 
	else
	{
		FakeTitleBarButtonWidth += CloseButton->ActualWidth;
	}

	//Setup Help Button
	Button^ HelpButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( InteropWindow->RootVisual, "TitleBarHelpButton" ) );
	HelpButton->Click += gcnew RoutedEventHandler( this, &MWPFFrame::HelpButtonClicked );
	if (InSettings->WindowHelpURL == nullptr)
	{
		HelpButton->Visibility = Visibility::Hidden;
	}
	else
	{
		HelpURL = InSettings->WindowHelpURL;
		FakeTitleBarButtonWidth += HelpButton->ActualWidth;
	}
	//temporarily disable the help button
	HelpButton->Visibility = Visibility::Hidden;

	EventBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( InteropWindow->RootVisual, "EventBorder" ) );


	//need to listen for windows movement events
	InteropWindow->AddHook(gcnew Interop::HwndSourceHook( this, &MWPFFrame::FrameMessageHookFunction ) );

	//Copy off settings for layout
	bCenterWindow = InSettings->bCenterWindow;
	bResizeable = InSettings->bResizable;
	bUseSaveLayout = InSettings->bUseSaveLayout;
}

/**Virtual Destructor*/
MWPFFrame::~MWPFFrame(void)
{
	if (WxDialog)
	{
		// Save the screen position before we go away
		SaveLayout();

		//shut down window
		WxDialog->Destroy();
		WxDialog = NULL;
	}
}


/** Makes the window modal until the modal has been released */
INT MWPFFrame::SetContentAndShowModal(MWPFPanel^ InPanel, const INT InDefaultDialogResult)
{
	DialogResult = InDefaultDialogResult;

	LayoutWindow(InPanel);

	InPanel->SetParentFrame (this);
	InPanel->PostLayout();

	//Show and set modal
	if (WxDialog)
	{
		GCallbackEvent->Send( CALLBACK_EditorPreModal );
		WxDialog->MakeModal(true);
		WxDialog->ShowModal();
		WxDialog->MakeModal(false);
		GCallbackEvent->Send( CALLBACK_EditorPostModal );
	}
	else
	{
		ShowWindow(TRUE);
		while (!bModalComplete)
		{
			MSG Msg;
			while( PeekMessageW(&Msg,NULL,0,0,PM_REMOVE) )
			{
				TranslateMessage( &Msg );
				DispatchMessageW( &Msg );
			}
		}
	}

	return DialogResult;
}

/** 
 * Shows the window (modeless)
 */
void MWPFFrame::SetContentAndShow(MWPFPanel^ InPanel)
{
	LayoutWindow(InPanel);

	InPanel->SetParentFrame (this);
	InPanel->PostLayout();

	//Show and set modal
	if (WxDialog)
	{
		WxDialog->Show();
	} 
	else
	{
		ShowWindow(TRUE);
	}
}

/** 
 * Shows the window (modeless)
 */
void MWPFFrame::SetContentAndShowComposite(ContentControl^ InControl, List<MWPFPanel^>^ InPaneList)
{
	check(InPaneList != nullptr);

	LayoutWindow(InControl);

	for (INT i = 0; i < InPaneList->Count; ++i)
	{
		MWPFPanel^ TempPanel = InPaneList[i];
		TempPanel->SetParentFrame (this);
		TempPanel->PostLayout();
	}

	//Show and set modal
	if (WxDialog)
	{
		WxDialog->Show();
	} 
	else
	{
		ShowWindow(TRUE);
	}
}

/** Used to close based on child panel buttons */
void MWPFFrame::Close(const INT InDialogResult)
{
	SaveLayout();

	ShowWindow(FALSE);
	bModalComplete = TRUE;
	DialogResult = InDialogResult;

	if (WxDialog != NULL)
	{
		if (WxDialog->IsModal())
		{
			WxDialog->EndModal(DialogResult);
		}
		WxDialog->Hide();
	}
}

/**Brings wx dialog to the front*/
void MWPFFrame::Raise (void)
{
	if (WxDialog)
	{
		WxDialog->Raise();
	}
}

/**
 * Forces a save of the current window layout
 */
void MWPFFrame::SaveLayout (void)
{
	//to ensure close getting called twice (in the case of a panel listening to the close button)
	if (bUseSaveLayout && WxDialog && WxDialog->IsShown())
	{
		//Save position of window.
		FWindowUtil::SavePosSize( CLRTools::ToFString(ContextName), WxDialog);
	}
}


/** Called when the dialog is being cancelled*/
void MWPFFrame::CloseButtonClicked( Object^ Owner, RoutedEventArgs^ Args )
{
	Close(DialogResult);
}

/** Called when the help button is clicked*/
void MWPFFrame::HelpButtonClicked(Object^ Owner, RoutedEventArgs^ Args)
{
	//otherwise this button should be inaccessible
	check(HelpURL);

	appLaunchURL(*CLRTools::ToFString(HelpURL));
}

/**
 * Preps the window to be shown or shown modal
 */
void MWPFFrame::LayoutWindow(ContentControl^ InPanel)
{
	//set content
	Border^ ContentBorder = safe_cast< Border^ >( LogicalTreeHelper::FindLogicalNode( InteropWindow->RootVisual, "ContentBorder" ) );
	ContentBorder->Child = InPanel;

	//layout the window
	MWPFWindowWrapper::FinalizeLayout(bCenterWindow ? true : false);
	ShowWindow(TRUE);

	Panel^ TitlePanel = safe_cast< Panel^ >( LogicalTreeHelper::FindLogicalNode( InteropWindow->RootVisual, "TitlePanel" ) );
	FakeTitleBarHeight = TitlePanel->ActualHeight;

	if (WxDialog)
	{
		//Match the wx window in size and position
		RECT rc;
		GetWindowRect(( HWND )InteropWindow->Handle.ToPointer(), &rc);
		//save off default width and height
		INT TempWidth  = rc.right - rc.left;
		INT TempHeight = rc.bottom - rc.top;
		WxDialog->SetSize(rc.left, rc.top, TempWidth, TempHeight);

		//Load position of window.
		if (bUseSaveLayout)
		{
			FWindowUtil::LoadPosSize( CLRTools::ToFString(ContextName), WxDialog);
		}
		//if the position is overriden in the file, use the new values (otherwise they will be the defaults)
		rc.left = WxDialog->GetPosition().x;
		rc.top  = WxDialog->GetPosition().y;
		if (bResizeable)
		{
			//use the values saved off in the ini file
			TempWidth  = WxDialog->GetSize().GetWidth();
			TempHeight = WxDialog->GetSize().GetHeight();
		}
		rc.right  = rc.left + TempWidth;
		rc.bottom = rc.top + TempHeight;

		SetWindowPos(static_cast<HWND>(InteropWindow->Handle.ToPointer()), NULL,  rc.left, rc.top, TempWidth, TempHeight, SWP_NOZORDER);
	}
}

/** Called when the HwndSource receives a windows message */
IntPtr MWPFFrame::FrameMessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled )
{
	OutHandled = false;
	int RetVal = 0;
	
	//only adjust the dialog if it exists
	if( WxDialog )
	{
		if( ( Msg == WM_WINDOWPOSCHANGED ) || ( Msg == WM_WINDOWPOSCHANGING ) )
		{
			RECT rc;
			GetWindowRect(( HWND )InteropWindow->Handle.ToPointer(), &rc);

			wxRect CompareRect = WxDialog->GetRect();
			INT WxLeft   = CompareRect.GetLeft();
			INT WxTop    = CompareRect.GetTop();
			INT WxRight  = WxLeft + CompareRect.GetWidth();
			INT WxBottom = WxTop + CompareRect.GetHeight();

			if ((WxLeft != rc.left) || (WxTop != rc.top) || (WxRight != rc.right) || (WxBottom != rc.bottom))
			{
				WxDialog->SetSize(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
				Raise();
			}
			OutHandled = true;
		}
		if( Msg == WM_MOUSEACTIVATE )
		{
			Raise();
		}
	}

	return (IntPtr)RetVal;
}
