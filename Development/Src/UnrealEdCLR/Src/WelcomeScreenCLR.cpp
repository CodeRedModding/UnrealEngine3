/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEdCLR.h"
#include "WelcomeScreenShared.h"
#include "FileHelpers.h"

#ifdef __cplusplus_cli

#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

/** Panel for the welcome screen */
ref class MWelcomeScreenPanel : public MWPFPanel
{
public:
	/**
	 * Constructor
	 *
	 * @param	InXaml	XAML file to use for the panel
	 */
	MWelcomeScreenPanel( String^ InXaml )
		: MWPFPanel( InXaml )
	{
		// Hook up the close button event
		Button^ CloseWindowButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CloseWindowButton") );
		check( CloseWindowButton != nullptr );
		CloseWindowButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnCloseClicked );

		// Hook up the getting started button event
		Button^ GettingStartedButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "GettingStartedButton") );
		check( GettingStartedButton != nullptr );
		GettingStartedButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnGettingStartedClicked );

		// Hook up the tutorials button event
		Button^ TutorialsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "TutorialsButton") );
		check( TutorialsButton != nullptr );
		TutorialsButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnTutorialsClicked );

		// Hook up the forums button event
		Button^ ForumsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "ForumsButton") );
		check( ForumsButton != nullptr );
		ForumsButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnForumsClicked );

		// Hook up the news button event
		Button^ NewsButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "NewsButton") );
		check( NewsButton != nullptr );
		NewsButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnNewsClicked );

		// Hook up the Facebook button event
		Button^ FacebookButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "FacebookButton") );
		check( FacebookButton != nullptr );
		FacebookButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnFacebookButtonClicked );

		// Hook up the new map button event
		Button^ NewMapButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "NewMapButton") );
		check( NewMapButton != nullptr );
		NewMapButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnNewMapClicked );

		// Hook up the open map button event
		Button^ OpenMapButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OpenMapButton") );
		check( OpenMapButton != nullptr );
		OpenMapButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnOpenMapClicked );

		// Hook up the show at startup check box events
		ShowAtStartupCheckBox = safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( this, "ShowAtStartupCheckBox" ) );
		check( ShowAtStartupCheckBox != nullptr );

		UBOOL bShowAtStartup = TRUE;
		GConfig->GetBool( TEXT("WelcomeScreen"), TEXT("bShowAtStartup"), bShowAtStartup, GEditorUserSettingsIni );
		ShowAtStartupCheckBox->IsChecked = ( bShowAtStartup == TRUE );

		ShowAtStartupCheckBox->Checked += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnStartupCheckBoxToggle );
		ShowAtStartupCheckBox->Unchecked += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnStartupCheckBoxToggle );
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		MWPFPanel::SetParentFrame( InParentFrame );

		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MWelcomeScreenPanel::OnCloseClicked );
	}

private:

	/** Checkbox to allow the user to specify whether to show the window at startup or not */
	CheckBox^ ShowAtStartupCheckBox;

	/** Called when the user clicks the close button */
	void OnCloseClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( 0 );
	}

	/** Called when the user clicks the getting started button */
	void OnGettingStartedClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		const FString URL = TEXT("http://udn.epicgames.com/Three/DevelopmentKitHome.html");
		appLaunchURL( *URL );
	}

	/** Called when the user clicks the tutorials button */
	void OnTutorialsClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		const FString URL = TEXT("http://udn.epicgames.com/Three/VideoTutorials.html");
		appLaunchURL( *URL );
	}

	/** Called when the user clicks the forums button */
	void OnForumsClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		const FString URL = TEXT("http://forums.epicgames.com/forums/366-UDK");
		appLaunchURL( *URL );
	}

	/** Called when the user clicks the news button */
	void OnNewsClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		const FString URL = TEXT("http://www.unrealengine.com/news/category/udk_releases/");
		appLaunchURL( *URL );
	}

	/** Called when the user clicks on the Facebook button */
	void OnFacebookButtonClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		const FString URL = TEXT("http://www.facebook.com/UnrealEngine");
		appLaunchURL( *URL );
	}

	/** Called when the user clicks the new map button */
	void OnNewMapClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		FEditorFileUtils::NewMapInteractive();
	}

	/** Called when the user clicks the open map button */
	void OnOpenMapClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		FEditorFileUtils::LoadMap();
	}
	
	/** Called when the user toggles the "On Startup" checkbox */
	void OnStartupCheckBoxToggle( Object^ Owner, RoutedEventArgs^ Args )
	{
		const UBOOL bIsChecked = ShowAtStartupCheckBox->IsChecked.HasValue && ShowAtStartupCheckBox->IsChecked.Value;
		GConfig->SetBool( TEXT("WelcomeScreen"), TEXT("bShowAtStartup"), bIsChecked, GEditorUserSettingsIni );
	}
};

#endif // #ifdef __cplusplus_cli

// Initialize static instance to NULL
FWelcomeScreen* FWelcomeScreen::Instance = NULL;

/**
 * Returns whether or not the welcome screen should be displayed at startup (based on user preference)
 *
 * @return	TRUE if the welcome screen should be displayed at startup; FALSE otherwise
 */
UBOOL FWelcomeScreen::ShouldDisplayWelcomeScreenAtStartup()
{
	UBOOL bDisplayWelcomeScreen = FALSE;

#ifdef __cplusplus_cli
	GConfig->GetBool( TEXT("WelcomeScreen"), TEXT("bShowAtStartup"), bDisplayWelcomeScreen, GEditorUserSettingsIni );
#endif // #ifdef __cplusplus_cli

	return bDisplayWelcomeScreen;
}

/** Display the welcome screen */
void FWelcomeScreen::DisplayWelcomeScreen()
{
#ifdef __cplusplus_cli
	FWelcomeScreen& Instance = GetInternalInstance();
	Instance.WelcomeScreenFrame->SetContentAndShow( Instance.WelcomeScreenPanel );
#endif // #ifdef __cplusplus_cli
}

/** Shut down the welcome screen singleton */
void FWelcomeScreen::Shutdown()
{
	delete Instance;
	Instance = NULL;
}

/** Constructor */
FWelcomeScreen::FWelcomeScreen()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );

#ifdef __cplusplus_cli
	// Initialize settings for the WPF frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
	Settings->WindowTitle = CLRTools::ToString( *LocalizeUnrealEd("WelcomeScreen_Title") );
	Settings->bCenterWindow = TRUE;
	Settings->bUseSaveLayout = FALSE;

	// Construct a WPF frame for the welcome screen
	WelcomeScreenFrame = gcnew MWPFFrame( NULL, Settings, TEXT("WelcomeScreen") );
	check( WelcomeScreenFrame );

	// Construct a custom welcome screen panel
	WelcomeScreenPanel = gcnew MWelcomeScreenPanel( CLRTools::ToString( TEXT("WelcomeScreen.xaml") ) );
	check( WelcomeScreenPanel );

	delete Settings;
#endif // #ifdef __cplusplus_cli
}

/** Destructor */
FWelcomeScreen::~FWelcomeScreen()
{
	// Unregister global callbacks
	GCallbackEvent->UnregisterAll( this );

	delete WelcomeScreenPanel;
	delete WelcomeScreenFrame;
	WelcomeScreenPanel = NULL;
	WelcomeScreenFrame = NULL;
}

/**
 * Return internal singleton instance of the class
 *
 * @return	Reference to the internal singleton instance of the class
 */
FWelcomeScreen& FWelcomeScreen::GetInternalInstance()
{
	if ( Instance == NULL )
	{
		Instance = new FWelcomeScreen();
	}
	check( Instance );
	return *Instance;
}

/** Override from FCallbackEventDevice to handle events */
void FWelcomeScreen::Send( ECallbackEventType Event )
{
	switch ( Event )
	{
		case CALLBACK_EditorPreModal:
			WelcomeScreenFrame->EnableWindow( false );
			break;

		case CALLBACK_EditorPostModal:
			WelcomeScreenFrame->EnableWindow( true );
			break;
	}
}