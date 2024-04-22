/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "StartPageShared.h"


/**
 * Start Page window control (managed)
 */
ref class MStartPageControl 
{
public:

	/** Constructor */
	MStartPageControl()
	{
		String^ URLString = "http://www.unrealengine.com/udk/documentation/";
		MainURI = gcnew Uri(URLString);

		//failed to load, go to a file on disk
		String^ EngineDir = CLRTools::ToString(*GFileManager->ConvertToAbsolutePath(*appEngineDir()));
		String^ RelativePath = "EditorResources\\UDKOffline.html";
		String^ BackupFile = EngineDir + RelativePath;
		BackupURI = gcnew Uri(BackupFile);

		CurrentURI = MainURI;

		//assume an empty load is a success (at start up).  First, view will rebuild the browser
		bDefaultLoadComplete = TRUE;
		//haven't gotten focus yet
		bFirstFocus = TRUE;
	}

	/** Destructor (deterministic; effectively IDisposable.Dispose() to the CLR) */
	~MStartPageControl()
	{
		DisposeOfInteropWindow();
	}

protected:
	/** Finalizer (non-deterministic destructor) */
	!MStartPageControl()
	{
	}

	/** Clean up the interop window if it still exists */
	void DisposeOfInteropWindow()
	{
		// Dispose of the window.  This also cancels our various event handlers and message hooks.
		if( InteropWindow != nullptr )
		{
			delete InteropWindow;
			InteropWindow = nullptr;
		}
	}

public:

	/**
	 * Initialize the start page
	 *
	 * @param	InParent				Parent window (or NULL if we're not parented.)
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitStartPage( WxStartPageHost* InParent, const HWND InParentWindowHandle )
	{
		//ParentStartPageWindow = InParent;

		// Setup WPF window to be created as a child of the browser window
		Interop::HwndSourceParameters sourceParams( "StartPageHost" );
		sourceParams.PositionX = 0;
		sourceParams.PositionY = 0;
		sourceParams.ParentWindow = (IntPtr)InParentWindowHandle;
		sourceParams.WindowStyle = (WS_VISIBLE | WS_CHILD);
		
		InteropWindow = gcnew Interop::HwndSource(sourceParams);
		InteropWindow->SizeToContent = SizeToContent::Manual;

		// Need to make sure any faulty WPF methods are hooked as soon as possible after a WPF window
		// has been created (it loads DLLs internally, which can't be hooked until after creation)
#if WITH_EASYHOOK
		WxUnrealEdApp::InstallHooksWPF();
#endif

		// Attach our message hook routine.  This is only for catching messages that do not
		// have WPF equivalents.  The WPF messages can be handled through the HwndSource
		// object itself!
		InteropWindow->AddHook(
			gcnew Interop::HwndSourceHook( this, &MStartPageControl::MessageHookFunction ) );

		// Show the window!
		const HWND WPFWindowHandle = ( HWND )InteropWindow->Handle.ToPointer();
		::ShowWindow( WPFWindowHandle, SW_SHOW );

		return TRUE;
	}

	/** Deals with parent resizing*/
	void Resize(HWND hWndParent, int x, int y, int Width, int Height)
	{
		SetWindowPos(static_cast<HWND>(InteropWindow->Handle.ToPointer()), NULL, 0, 0, Width, Height, SWP_NOMOVE | SWP_NOZORDER);
	}

	/**
	 * Propagates focus to the WPF control.
	 */
	void SetFocus()
	{
		if (bFirstFocus)
		{
			UBOOL bBuildNewBrowser = TRUE;
			RebuildWebBrowserControl(bBuildNewBrowser);
			bFirstFocus = FALSE;
		}

		if ( InteropWindow != nullptr )
		{
			WebBrowserCtrl->Focus();
		}
	}

	/** Called when the URL begins loading*/
	void OnURIBeginLoad( Object^ Owner, System::Windows::Navigation::NavigatingCancelEventArgs^ Args )
	{
		FString MainURIString = CLRTools::ToFString(MainURI->AbsoluteUri);
		FString BackupURIString = CLRTools::ToFString(BackupURI->AbsoluteUri);
		FString RequestedURIString = CLRTools::ToFString(Args->Uri->AbsoluteUri);
		FString CurrentURIString = CLRTools::ToFString(CurrentURI->AbsoluteUri);
		UBOOL bStartFromEnd = FALSE;
		UBOOL bIgnoreCase = TRUE;

		//if we're trying to load one of the main pages or a related error
		INT MainStringFoundIndex = RequestedURIString.InStr(MainURIString, bStartFromEnd, bIgnoreCase);
		UBOOL bContainsMainString   = (MainStringFoundIndex!=INDEX_NONE);
		UBOOL bContainsBackupString = (RequestedURIString.InStr(BackupURIString, bStartFromEnd, bIgnoreCase)!=INDEX_NONE);
		if (bContainsMainString || bContainsBackupString)
		{
			//Special case when including links within the same page
			INT RequestedPoundFoundIndex = RequestedURIString.InStr(TEXT("#"), bStartFromEnd, bIgnoreCase);
			INT CurrentPoundFoundIndex = CurrentURIString.InStr(TEXT("#"), bStartFromEnd, bIgnoreCase);
			if ((RequestedPoundFoundIndex != INDEX_NONE) && (CurrentPoundFoundIndex != INDEX_NONE))
			{
				if (RequestedURIString.Left(RequestedPoundFoundIndex) == CurrentURIString.Left(CurrentPoundFoundIndex))
				{
					//we're already on this page.  Don't reload the browser
					//Args->Cancel = true;
					return;
				}
			}

			//this isn't an error page, but the correct URL to load!
			if (MainStringFoundIndex == 0)
			{
				UBOOL bBuildNewBrowser = (CurrentURI == BackupURI);
				Args->Cancel = bBuildNewBrowser ? true : false;
				CurrentURI = Args->Uri;
				RebuildWebBrowserControl(bBuildNewBrowser);
			}
			//else did we try to load and fail?
			else if (Args->Uri != CurrentURI)
			{
				//force this through by setting default load complete to true
				bDefaultLoadComplete = TRUE;
				CurrentURI = BackupURI;
				UBOOL bBuildNewBrowser = TRUE;
				Args->Cancel = bBuildNewBrowser ? true : false;
				RebuildWebBrowserControl(bBuildNewBrowser);
			}
		}
		else
		{
			INT MainPrefixFoundIndex = RequestedURIString.InStr("http://udk.com", bStartFromEnd, bIgnoreCase);
			//MUST be at the very beginning (to ensure it's not a "cannot find" error)
			UBOOL bContainsMainPrefix   = (MainPrefixFoundIndex==0);

			INT DocumentationFoundIndex = RequestedURIString.InStr("documentation", bStartFromEnd, bIgnoreCase);
			UBOOL bContainsDocumentation   = (DocumentationFoundIndex!=INDEX_NONE);

			//is this a request for a localized start page
			if (bContainsMainPrefix && bContainsDocumentation)
			{
				if (CurrentURI != Args->Uri)
				{
					CurrentURI = Args->Uri;
				}
			}
			else
			{
				//loading an external page
				Uri^ RequestedURI = Args->Uri;
				Args->Cancel = TRUE;

				FString URIToLaunch = CLRTools::ToFString(RequestedURI->AbsoluteUri);
				appLaunchURL(*URIToLaunch);
			}
		}
	}

	/** Called when the URL begins loading*/
	void OnURIBeginDownload (Object^ Owner, System::Windows::Navigation::NavigationEventArgs^ Args )
	{
	}

	/** Called when the URL finishes loading*/
	void OnURILoadCompleted( Object^ Owner, System::Windows::Navigation::NavigationEventArgs^ Args )
	{
		//we made our first attempt.  They can navigate from here.
		bDefaultLoadComplete = TRUE;
	}

	/** Called when the HwndSource receives a windows message */
	IntPtr MessageHookFunction( IntPtr HWnd, int Msg, IntPtr WParam, IntPtr LParam, bool% OutHandled )
	{
		IntPtr Result = (IntPtr)0;
		OutHandled = false;

		if( Msg == WM_GETDLGCODE )
		{
			OutHandled = true;

			// This tells Windows (and Wx) that we'll need keyboard events for this control
			Result = IntPtr( DLGC_WANTALLKEYS );
		}


		// Tablet PC software (e.g. Tablet PC Input Panel) sends WM_GETOBJECT events which
		// trigger a massive performance bug in WPF (http://wpf.codeplex.com/Thread/View.aspx?ThreadId=41964)
		// We'll intercept this message and skip it.
		if( Msg == WM_GETOBJECT )
		{
			OutHandled = true;
			Result = (IntPtr)0;
		}

		return Result;
	}

protected:
	void RebuildWebBrowserControl(UBOOL bDestroyPreviousBrowser)
	{
		//if we'd hit steady state (start-up OR browsed to page)
		if (bDefaultLoadComplete)
		{
			UBOOL bOldBrowserExisted = WebBrowserCtrl != nullptr;

			//if there was never a browser before or we are trying to reload the MAIN uri.  make a new browser
			if ((!bOldBrowserExisted) || (bDestroyPreviousBrowser))
			{
				//create web control and tether to panel
				WebBrowserCtrl = gcnew System::Windows::Controls::WebBrowser();
				WebBrowserCtrl->Navigating    += gcnew System::Windows::Navigation::NavigatingCancelEventHandler( this, &MStartPageControl::OnURIBeginLoad );
				WebBrowserCtrl->Navigated     += gcnew System::Windows::Navigation::NavigatedEventHandler( this, &MStartPageControl::OnURIBeginDownload );
				WebBrowserCtrl->LoadCompleted += gcnew System::Windows::Navigation::LoadCompletedEventHandler( this, &MStartPageControl::OnURILoadCompleted );

				//WebBrowserPanel->Children->Add(WebBrowserCtrl);
				check(InteropWindow);
				InteropWindow->RootVisual = WebBrowserCtrl;
			} 

			//just let the event go through our browser, in the case of first time loading OR just connecting to the internet and hitting refresh.
			bDefaultLoadComplete = FALSE;

			//WebBrowserCtrl->NavigateToString(TEXT(" "));
			WebBrowserCtrl->Navigate(CurrentURI);
		}
	}

	/** The actual WPF control that is being hosted */
	System::Windows::Controls::WebBrowser^ WebBrowserCtrl;

	/** WPF interop wrapper around the HWND and WPF controls */
	Interop::HwndSource^ InteropWindow;

	/** URI to request*/
	Uri^ MainURI;
	/** Backup URI to attempt */
	Uri^ BackupURI;
	/** URI we are currently attempting to laod*/
	Uri^CurrentURI;
	//Allows further navigating after initial load via external browser
	UBOOL bDefaultLoadComplete;
	//Rebuild the webbrowser on first focus
	UBOOL bFirstFocus;
};


/** Static: List of currently-active Start Page instances */
TArray< FStartPage* > FStartPage::StartPageInstances;

/** Static: Allocate and initialize start page*/
FStartPage* FStartPage::CreateStartPage( WxStartPageHost* InParent, const HWND InParentWindowHandle )
{
	FStartPage* NewStartPage = new FStartPage();

	if( !NewStartPage->InitStartPage( InParent, InParentWindowHandle ) )
	{
		delete NewStartPage;
		return NULL;
	}

	return NewStartPage;
}

/**
 * Initialize the start page
 *
 * @param	InParent				Parent window (or NULL if we're not parented.)
 * @param	InParentWindowHandle	Parent window handle
 *
 * @return	TRUE if successful
 */
UBOOL FStartPage::InitStartPage( WxStartPageHost* InParent, const HWND InParentWindowHandle )
{
	WindowControl = gcnew MStartPageControl( );
	MStartPageControl^ MyWindowControl = WindowControl;
	if( MyWindowControl == nullptr )
	{
		return FALSE;
	}

	UBOOL bSuccess = MyWindowControl->InitStartPage( InParent, InParentWindowHandle );

	return bSuccess;
}



/** Constructor */
FStartPage::FStartPage()
{
	// Add to list of instances
	StartPageInstances.AddItem( this );
}



/** Destructor */
FStartPage::~FStartPage()
{
	// Update singleton
	StartPageInstances.RemoveItem( this );
}

/** Resize the window */
void FStartPage::Resize(HWND hWndParent, int x, int y, int Width, int Height)
{
	WindowControl->Resize(hWndParent, x, y, Width, Height);
}

/**
 * Propagates focus to the WPF control.
 */
void FStartPage::SetFocus()
{
	MStartPageControl^ MyWindowControl = WindowControl;
	if ( MyWindowControl != nullptr )
	{
		MyWindowControl->SetFocus();
	}
}

// EOF



