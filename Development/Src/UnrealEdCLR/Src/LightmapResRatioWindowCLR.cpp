/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEdCLR.h"

#include "ManagedCodeSupportCLR.h"
#include "LightmapResRatioWindowShared.h"
#include "WPFWindowWrapperCLR.h"

#include "ConvertersCLR.h"

#pragma unmanaged
#include "LightmapResRatioAdjust.h"
#pragma managed

/**
 * Light map resolution window control (managed)
 */
ref class MLightmapResRatioWindow
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
	UBOOL InitLightmapResRatioWindow( const HWND InParentWindowHandle )
	{
		String^ WindowTitle = CLRTools::LocalizeString( "LightmapResRatioWindow_WindowTitle" );
		String^ WPFXamlFileName = "LightmapResRatioWindow.xaml";

		// We draw our own title bar so tell the window about it's height
		const int FakeTitleBarHeight = 28;
		const UBOOL bIsTopMost = FALSE;

		// If we don't have an initial position yet then default to centering the new window
		bool bCenterWindow = (FLightmapResRatioAdjustSettings::Get().WindowPositionX == -1) || 
			(FLightmapResRatioAdjustSettings::Get().WindowPositionY == -1);

		// Call parent implementation's init function to create the actual window
		if (!InitWindow( InParentWindowHandle,
						 WindowTitle,
						 WPFXamlFileName,
 						 FLightmapResRatioAdjustSettings::Get().WindowPositionX,
 						 FLightmapResRatioAdjustSettings::Get().WindowPositionY,
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

		// Hook up the primitives
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PrimitiveStaticMeshesCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "PrimitiveStaticMeshes" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PrimitiveBSPSurfacesCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "PrimitiveBSPSurfaces" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PrimitiveTerrainsCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "PrimitiveTerrains" );
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "PrimitiveFluidSurfacesCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "PrimitiveFluidSurfaces" );

		// Hook up the LevelSelect properties
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "LevelSelectRadio_Current" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsLevelSelectCurrent" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "LevelSelectRadio_Selected" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsLevelSelectSelected" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< UnrealEd::BindableRadioButton^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "LevelSelectRadio_AllLoaded" ) ),
			UnrealEd::BindableRadioButton::IsActuallyCheckedProperty, this, "IsLevelSelectAllLoaded" );

		// Hook up the selectobjects only
 		UnrealEd::Utils::CreateBinding(
			safe_cast< CheckBox^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "SelectedObjectsOnlyCheckBox" ) ),
			CheckBox::IsCheckedProperty, this, "SelectedObjectsOnly" );

		// Hook up the dragsliders
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "RatioDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "RatioValue" );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "StaticMeshMinDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MinStaticMeshes", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "StaticMeshMaxDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MaxStaticMeshes", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BSPMinDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MinBSPSurfaces", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "BSPMaxDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MaxBSPSurfaces", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "TerrainMinDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MinTerrains", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "TerrainMaxDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MaxTerrains", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FluidMinDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MinFluidSurfaces", gcnew RoundedIntToDoubleConverter() );
		UnrealEd::Utils::CreateBinding(
			safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( RootVisual, "FluidMaxDragSlider" ) ),
			CustomControls::DragSlider::ValueProperty, this, "MaxFluidSurfaces", gcnew RoundedIntToDoubleConverter() );

		// Hook up the buttons
		Button^ ApplyRatioButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode(RootVisual, "RatioApplyButton" ) );
		ApplyRatioButton->Click += gcnew RoutedEventHandler( this, &MLightmapResRatioWindow::ApplyRatioButton_Click);
		Button^ CloseButton = safe_cast< Button^ >(LogicalTreeHelper::FindLogicalNode(RootVisual, "RatioCloseButton" ) );
		CloseButton->Click += gcnew RoutedEventHandler( this, &MLightmapResRatioWindow::CloseButton_Click);

		// Register for property change callbacks from our properties object
		this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MLightmapResRatioWindow::OnLightmapResRatioPropertyChanged );

		// Show the window!
		ShowWindow( true );

		return TRUE;
	}

protected:

	/** Called when a property is changed */
	void OnLightmapResRatioPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
	{
		// ...
	}

	/** Called when the apply button is clicked */
	void ApplyRatioButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		FLightmapResRatioAdjustSettings::ApplyRatioAdjustment();
	}

	/** Called when the close button is clicked */
	void CloseButton_Click(Object^ Owner, RoutedEventArgs^ Args)
	{
		ShowWindow(false);
	}

public:

	/** INotifyPropertyChanged: Exposed event from INotifyPropertyChanged::PropertyChanged */
	virtual event ComponentModel::PropertyChangedEventHandler^ PropertyChanged;

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

	/** Static meshes property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(PrimitiveStaticMeshes, FLightmapResRatioAdjustSettings::Get().bStaticMeshes);
	/** BSP surfaces property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(PrimitiveBSPSurfaces, FLightmapResRatioAdjustSettings::Get().bBSPSurfaces);
	/** Terrains property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(PrimitiveTerrains, FLightmapResRatioAdjustSettings::Get().bTerrains);
	/** Fluid surfaces property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(PrimitiveFluidSurfaces, FLightmapResRatioAdjustSettings::Get().bFluidSurfaces);
	/** Radio button properties for level options */
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsLevelSelectCurrent, ELightmapResRatioAdjustLevels::Options, FLightmapResRatioAdjustSettings::Get().LevelOptions, ELightmapResRatioAdjustLevels::Current );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsLevelSelectSelected, ELightmapResRatioAdjustLevels::Options, FLightmapResRatioAdjustSettings::Get().LevelOptions, ELightmapResRatioAdjustLevels::Selected );
	DECLARE_ENUM_NOTIFY_PROPERTY( bool, IsLevelSelectAllLoaded, ELightmapResRatioAdjustLevels::Options, FLightmapResRatioAdjustSettings::Get().LevelOptions, ELightmapResRatioAdjustLevels::AllLoaded );
	/** Selected objects only property */
	DECLARE_MAPPED_NOTIFY_BOOL_PROPERTY(SelectedObjectsOnly, FLightmapResRatioAdjustSettings::Get().bSelectedObjectsOnly);
	/** Ratio property */
	DECLARE_MAPPED_NOTIFY_PROPERTY(double,	RatioValue,			FLOAT,	FLightmapResRatioAdjustSettings::Get().Ratio);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MinStaticMeshes,	INT,	FLightmapResRatioAdjustSettings::Get().Min_StaticMeshes);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MaxStaticMeshes,	INT,	FLightmapResRatioAdjustSettings::Get().Max_StaticMeshes);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MinBSPSurfaces,		INT,	FLightmapResRatioAdjustSettings::Get().Min_BSPSurfaces);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MaxBSPSurfaces,		INT,	FLightmapResRatioAdjustSettings::Get().Max_BSPSurfaces);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MinTerrains,		INT,	FLightmapResRatioAdjustSettings::Get().Min_Terrains);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MaxTerrains,		INT,	FLightmapResRatioAdjustSettings::Get().Max_Terrains);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MinFluidSurfaces,	INT,	FLightmapResRatioAdjustSettings::Get().Min_FluidSurfaces);
	DECLARE_MAPPED_NOTIFY_PROPERTY(int,		MaxFluidSurfaces,	INT,	FLightmapResRatioAdjustSettings::Get().Max_FluidSurfaces);
};

/** Static: Allocate and initialize window */
FLightmapResRatioWindow* FLightmapResRatioWindow::CreateLightmapResRatioWindow( const HWND InParentWindowHandle )
{
	FLightmapResRatioWindow* NewLightmapResRatioWindow = new FLightmapResRatioWindow();
	if (!NewLightmapResRatioWindow->InitLightmapResRatioWindow(InParentWindowHandle))
	{
		delete NewLightmapResRatioWindow;
		return NULL;
	}
	return NewLightmapResRatioWindow;
}

/** Constructor */
FLightmapResRatioWindow::FLightmapResRatioWindow()
{
	// Register to find out about other windows going modal
	GCallbackEvent->Register( CALLBACK_EditorPreModal, this );
	GCallbackEvent->Register( CALLBACK_EditorPostModal, this );
}

/** Destructor */
FLightmapResRatioWindow::~FLightmapResRatioWindow()
{
	// Unregister callbacks
	GCallbackEvent->UnregisterAll( this );


	// @todo WPF: This is probably redundant, but I'm still not sure if AutoGCRoot destructor will get
	//   called when native code destroys an object that has a non-virtual (or no) destructor

	// Dispose of WindowControl
	WindowControl.reset();
}

/** Initialize the window */
UBOOL FLightmapResRatioWindow::InitLightmapResRatioWindow(const HWND InParentWindowHandle)
{
	WindowControl = gcnew MLightmapResRatioWindow();
	UBOOL bSuccess = WindowControl->InitLightmapResRatioWindow(InParentWindowHandle);
	return bSuccess;
}

/** 
 *	Show the window
 *
 *	@param	bShow		If TRUE, show the window
 *						If FALSE, hide it
 */
void FLightmapResRatioWindow::ShowWindow(UBOOL bShow)
{
	WindowControl->ShowWindow(bShow ? true : false);
	if (bShow == false)
	{
		SaveWindowSettings();
	}
}

/** Refresh all properties */
void FLightmapResRatioWindow::RefreshAllProperties()
{
	WindowControl->RefreshAllProperties();
}

/** Saves window settings to the settings structure */
void FLightmapResRatioWindow::SaveWindowSettings()
{
	Point^ WindowPos = WindowControl->GetRootVisual()->PointToScreen( Point( 0, 0 ) );

	// Store the window's current position
	FLightmapResRatioAdjustSettings::Get().WindowPositionX = WindowPos->X;
	FLightmapResRatioAdjustSettings::Get().WindowPositionY = WindowPos->Y;
}

/** Returns true if the mouse cursor is over the window */
UBOOL FLightmapResRatioWindow::IsMouseOverWindow()
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
void FLightmapResRatioWindow::Send( ECallbackEventType Event )
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
