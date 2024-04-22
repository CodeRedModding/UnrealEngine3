/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "LightingToolsWindowShared.h"
#include "WPFWindowWrapperCLR.h"

#include "ConvertersCLR.h"

#pragma unmanaged
#include "LightingTools.h"
#pragma managed

/**
 * Lighting tools window control (managed)
 */
ref class MLightingToolsWindow
	: public MWPFWindowWrapper,
	  public ComponentModel::INotifyPropertyChanged
{

public:

	/**
	 * Initialize the window
	 *
	 * @param	InParentWindowHandle	Parent window handle
	 *
	 * @return	TRUE if successful
	 */
	UBOOL InitLightingToolsWindow( FLightingToolsWindow* InToolsWindow, const HWND InParentWindowHandle )
	{
		ToolsWindow = InToolsWindow;
		String^ WindowTitle = CLRTools::LocalizeString( "LightingToolsWindow_WindowTitle" );
		String^ WPFXamlFileName = "LightingToolsWindow.xaml";

		// We draw our own title bar so tell the window about it's height
		const int FakeTitleBarHeight = 28;
		const UBOOL bIsTopMost = FALSE;

		// If we don't have an initial position yet then default to centering the new window
		bool bCenterWindow = (FLightingToolsSettings::Get().WindowPositionX == -1) || 
			(FLightingToolsSettings::Get().WindowPositionY == -1);

		// Call parent implementation's init function to create the actual window
		if (!InitWindow( InParentWindowHandle,
						 WindowTitle,
						 WPFXamlFileName,
 						 FLightingToolsSettings::Get().WindowPositionX,
 						 FLightingToolsSettings::Get().WindowPositionY,
						 bCenterWindow,
						 FakeTitleBarHeight,
						 bIsTopMost) )
		{
			return FALSE;
		}

		// Setup bindings
		Visual^ RootVisual = InteropWindow->RootVisual;

		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>(RootVisual);
		WindowContentElement->DataContext = this;

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ShowBoundsCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ShowBounds" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ShowShadowTracesCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ShowShadowTraces" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ShowDirectCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ShowDirect" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ShowIndirectCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ShowIndirect" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ShowIndirectSamplesCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ShowSamples" );

		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "ShowDominantLightsCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "ShowDominantLights" );
 		
		// Register for property change callbacks from our properties object
		this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MLightingToolsWindow::OnLightingToolsPropertyChanged );

		// Hook up the buttons
		Button^ CloseButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode(RootVisual, "LightingToolsCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MLightingToolsWindow::CloseButton_Click);

		// Show the window!
		ShowWindow( true );

		return TRUE;
	}

protected:

	/** Called when a property is changed */
	void OnLightingToolsPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
	{
		FLightingToolsSettings::ApplyToggle();
	}

	/** Called when the close button is clicked */
	void CloseButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		FLightingToolsSettings::Reset();
		ToolsWindow->ShowWindow(false);
	}

public:

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
	virtual event ComponentModel::PropertyChangedEventHandler^ PropertyChanged;

	FLightingToolsWindow* ToolsWindow;

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

	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(ShowBounds, FLightingToolsSettings::Get().bShowLightingBounds);
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(ShowShadowTraces, FLightingToolsSettings::Get().bShowShadowTraces);
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(ShowDirect, FLightingToolsSettings::Get().bShowDirectOnly);
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(ShowIndirect, FLightingToolsSettings::Get().bShowIndirectOnly);
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(ShowSamples, FLightingToolsSettings::Get().bShowIndirectSamples);
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(ShowDominantLights, FLightingToolsSettings::Get().bShowAffectingDominantLights);
};

/** Static: Allocate and initialize window */
FLightingToolsWindow* FLightingToolsWindow::CreateLightingToolsWindow( const HWND InParentWindowHandle )
{
	FLightingToolsWindow* NewLightingToolsWindow = new FLightingToolsWindow();
	if (!NewLightingToolsWindow->InitLightingToolsWindow(InParentWindowHandle))
	{
		delete NewLightingToolsWindow;
		return NULL;
	}
	return NewLightingToolsWindow;
}

/** Constructor */
FLightingToolsWindow::FLightingToolsWindow()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );
}

/** Destructor */
FLightingToolsWindow::~FLightingToolsWindow()
{
	// Unregister callbacks
	GCallbackEvent->UnregisterAll( this );


	// @todo WPF: This is probably redundant, but I'm still not sure if AutoGCRoot destructor will get
	//   called when native code destroys an object that has a non-virtual (or no) destructor

	// Dispose of WindowControl
	WindowControl.reset();
}

/** Initialize the window */
UBOOL FLightingToolsWindow::InitLightingToolsWindow(const HWND InParentWindowHandle)
{
	WindowControl = gcnew MLightingToolsWindow();
	GConfig->GetInt(TEXT("LightingToolsWindow"), TEXT("PosX"), FLightingToolsSettings::Get().WindowPositionX, GEditorUserSettingsIni);
	GConfig->GetInt(TEXT("LightingToolsWindow"), TEXT("PosY"), FLightingToolsSettings::Get().WindowPositionY, GEditorUserSettingsIni);
	UBOOL bSuccess = WindowControl->InitLightingToolsWindow(this, InParentWindowHandle);
	return bSuccess;
}

/** 
 *	Show the window
 *
 *	@param	bShow		If TRUE, show the window
 *						If FALSE, hide it
 */
void FLightingToolsWindow::ShowWindow(UBOOL bShow)
{
	WindowControl->ShowWindow(bShow ? true : false);
	if (bShow)
	{
		FLightingToolsSettings::Init();
	}
	else
	{
		SaveWindowSettings();
	}
}

/** Refresh all properties */
void FLightingToolsWindow::RefreshAllProperties()
{
	WindowControl->RefreshAllProperties();
}

/** Saves window settings to the settings structure */
void FLightingToolsWindow::SaveWindowSettings()
{
	Point^ WindowPos = WindowControl->GetRootVisual()->PointToScreen( Point( 0, 0 ) );

	// Store the window's current position
	FLightingToolsSettings::Get().WindowPositionX = WindowPos->X;
	FLightingToolsSettings::Get().WindowPositionY = WindowPos->Y;

	GConfig->SetInt(TEXT("LightingToolsWindow"), TEXT("PosX"), WindowPos->X, GEditorUserSettingsIni);
	GConfig->SetInt(TEXT("LightingToolsWindow"), TEXT("PosY"), WindowPos->Y, GEditorUserSettingsIni);
}

/** Returns true if the mouse cursor is over the window */
UBOOL FLightingToolsWindow::IsMouseOverWindow()
{
	if( WindowControl.get() != nullptr )
	{
		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>( WindowControl->GetRootVisual() );
		if( WindowContentElement->IsMouseOver )
		{
			return TRUE;
		}
	}

	return FALSE;
}


/** FCallbackEventDevice: Called when a parameterless global event we've registered for is fired */
void FLightingToolsWindow::Send( ECallbackEventType Event )
{
	if( WindowControl.get() != nullptr )
	{
		FrameworkElement^ WindowContentElement = safe_cast<FrameworkElement^>( WindowControl->GetRootVisual() );

		switch ( Event )
		{
			case CALLBACK_EditorPreModal:
				WindowContentElement->IsEnabled = false;
				break;

			case CALLBACK_EditorPostModal:
				WindowContentElement->IsEnabled = true;
				break;
		}
	}
}
