/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEdCLR.h"
#include "AboutScreenShared.h"
#include "FileHelpers.h"

#ifdef __cplusplus_cli
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

using namespace System::Windows::Media::Imaging;

/** Panel for the about screen */
ref class MAboutScreenPanel : public MWPFPanel
{
public:
	/** Name of the currently running editor */
	String^ AppName;

	/**
	 * Constructor
	 *
	 * @param	InXaml	XAML file to use for the panel
	 */
	MAboutScreenPanel( String^ InXaml )
		: MWPFPanel( InXaml )
	{
		// Hook up the close button event
		Button^ CloseWindowButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CloseWindowButton") );
		check( CloseWindowButton != nullptr );
		CloseWindowButton->Click += gcnew RoutedEventHandler( this, &MAboutScreenPanel::OnCloseClicked );

		// Hook up the Facebook button event
		Button^ FacebookButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "FacebookButton") );
		check( FacebookButton != nullptr );
		FacebookButton->Click += gcnew RoutedEventHandler( this, &MAboutScreenPanel::OnFacebookButtonClicked );

		FString SplashPath;
		if(appGetSplashPath(TEXT("PC\\EdSplash.bmp"), SplashPath))
		{
			Image^ AboutScreenSplash = safe_cast< Image^ >( LogicalTreeHelper::FindLogicalNode( this, "AboutScreenSplash") );
			check( AboutScreenSplash != nullptr );
	
			AboutScreenSplash->Source = gcnew BitmapImage( gcnew Uri(CLRTools::ToString(appConvertRelativePathToFull(SplashPath)), UriKind::Absolute) );
		}
		
#if UDK
		AppName = CLRTools::ToString(LocalizeUnrealEd( "UDKTitle" ));
#else
		FString GameName = GConfig->GetStr(TEXT("URL"), TEXT("GameName"), GEngineIni);
		AppName = CLRTools::ToString(FString::Printf( LocalizeSecure( LocalizeUnrealEd( "UnrealEdTitle_F" ), *GameName )));
#endif

		TextBlock^ AboutScreenTitle = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( this, "AboutScreenTitle") );
		check( AboutScreenTitle != nullptr);
		AboutScreenTitle->Text = CLRTools::ToString( *FString::Printf(LocalizeSecure(LocalizeUnrealEd("UnrealEdVersionTitle"), *CLRTools::ToFString(AppName))) );

		TextBlock^ AboutScreenVersion = safe_cast< TextBlock^ >( LogicalTreeHelper::FindLogicalNode( this, "AboutScreenVersion") );
		check( AboutScreenVersion != nullptr);
		AboutScreenVersion->Text = CLRTools::ToString( *FString::Printf(LocalizeSecure(LocalizeUnrealEd("UnrealEdVersion"), GEngineVersion, GBuiltFromChangeList)) );
	}

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame( MWPFFrame^ InParentFrame ) override
	{
		MWPFPanel::SetParentFrame( InParentFrame );

		Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MAboutScreenPanel::OnCloseClicked );
	}

private:
	/** Called when the user clicks the close button */
	void OnCloseClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		ParentFrame->Close( 0 );
	}

	/** Called when the user clicks on the Facebook button */
	void OnFacebookButtonClicked( Object^ Owner, RoutedEventArgs^ Args )
	{
		const FString URL = TEXT("http://www.facebook.com/UnrealEngine");
		appLaunchURL( *URL );
	}
};

#endif // #ifdef __cplusplus_cli

// Initialize static instance to NULL
FAboutScreen* FAboutScreen::Instance = NULL;

/** Display the about screen */
void FAboutScreen::DisplayAboutScreen()
{
#ifdef __cplusplus_cli
	FAboutScreen& Instance = GetInternalInstance();
	Instance.AboutScreenFrame->SetContentAndShow( Instance.AboutScreenPanel );
#endif // #ifdef __cplusplus_cli
}

/** Shut down the about screen singleton */
void FAboutScreen::Shutdown()
{
	delete Instance;
	Instance = NULL;
}

/** Constructor */
FAboutScreen::FAboutScreen()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );

#ifdef __cplusplus_cli
	// Construct a custom about screen panel
	AboutScreenPanel = gcnew MAboutScreenPanel( CLRTools::ToString( TEXT("AboutScreen.xaml") ) );
	check( AboutScreenPanel );

	// Initialize settings for the WPF frame
	WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;

	Settings->bCenterWindow = TRUE;
	Settings->bUseSaveLayout = FALSE;

	FString WindowLabel = Localize( TEXT("UnrealEd"), TEXT("AboutUnrealEd"), GPackage, NULL);
	if(WindowLabel.Len() > 0)
	{
		Settings->WindowTitle = CLRTools::ToString(*FString::Printf(LocalizeSecure(WindowLabel, *CLRTools::ToFString(AboutScreenPanel->AppName))));
	}

	// Construct a WPF frame for the about screen
	AboutScreenFrame = gcnew MWPFFrame( NULL, Settings, TEXT("AboutScreen") );
	check( AboutScreenFrame );

	delete Settings;
#endif // #ifdef __cplusplus_cli
}

/** Destructor */
FAboutScreen::~FAboutScreen()
{
	// Unregister global callbacks
	GCallbackEvent->UnregisterAll( this );

	delete AboutScreenPanel;
	delete AboutScreenFrame;
	AboutScreenPanel = NULL;
	AboutScreenFrame = NULL;
}

/**
 * Return internal singleton instance of the class
 *
 * @return	Reference to the internal singleton instance of the class
 */
FAboutScreen& FAboutScreen::GetInternalInstance()
{
	if ( Instance == NULL )
	{
		Instance = new FAboutScreen();
	}
	check( Instance );
	return *Instance;
}

/** Override from FCallbackEventDevice to handle events */
void FAboutScreen::Send( ECallbackEventType Event )
{
	switch ( Event )
	{
		case CALLBACK_EditorPreModal:
			AboutScreenFrame->EnableWindow( false );
			break;

		case CALLBACK_EditorPostModal:
			AboutScreenFrame->EnableWindow( true );
			break;
	}
}