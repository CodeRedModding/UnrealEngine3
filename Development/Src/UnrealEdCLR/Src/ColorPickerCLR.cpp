/*================================================================================
	ColorPickerCLR.cpp: Code for interfacing C++ with C++/CLI and WPF
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEdCLR.h"

#ifdef __cplusplus_cli

#include "InteropShared.h"
#include "ManagedCodeSupportCLR.h"
#include "ColorPickerShared.h"
#include "WPFFrameCLR.h"

using namespace System;
using namespace System::Windows::Shapes;
using namespace System::Windows::Input;


MColorPickerPanel::MColorPickerPanel(const FPickColorStruct& InColorStruct, String^ InXamlName, const UBOOL bInEmbeddedPanel)
	: MWPFPanel(InXamlName)
	, ColorStruct(InColorStruct)
	, bBrightnessSliderCausedValueChange(false)
	, bEmbeddedPanel(bInEmbeddedPanel)
{
	//hook up button events
	Button^ OKButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "OKButton" ) );
	OKButton->Click += gcnew RoutedEventHandler( this, &MColorPickerPanel::OKClicked );

	Button^ CancelButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "CancelButton" ) );
	CancelButton->Click += gcnew RoutedEventHandler( this, &MColorPickerPanel::CancelClicked );

	//hide the ok/close buttons if this is an embedded button
	if (bEmbeddedPanel)
	{
		OKButton->Visibility = System::Windows::Visibility::Collapsed;
		CancelButton->Visibility = System::Windows::Visibility::Collapsed;
	}

	//Button for changing mouse to eye dropper
	Button^ EyeDropperButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode( this, "EyeDropperButton" ) );
	EyeDropperButton->Click += gcnew RoutedEventHandler( this, &MColorPickerPanel::EyeDropperClicked );

	//Hook up mouse events
	MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::OnMouseUp );
	LostMouseCapture += gcnew MouseEventHandler( this, &MColorPickerPanel::OnLostMouseCapture );

	RedSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "RedDragSlider" ) );
	GreenSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "GreenDragSlider" ) );
	BlueSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "BlueDragSlider" ) );
	AlphaSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "AlphaDragSlider" ) );
	HueSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "HueDragSlider" ) );
	SaturationSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "SaturationDragSlider" ) );
	BrightnessSlider = safe_cast< CustomControls::DragSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "BrightnessDragSlider" ) );
	ColorWheel = safe_cast< CustomControls::ColorWheel^ >( LogicalTreeHelper::FindLogicalNode( this, "ColorWheel" ) );
	SimpleSaturationSlider = safe_cast< CustomControls::GradientSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "SimpleSaturationSlider" ) );
	SimpleBrightnessSlider = safe_cast< CustomControls::GradientSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "SimpleBrightnessSlider" ) );
	SimpleAlphaSlider = safe_cast< CustomControls::GradientSlider^ >( LogicalTreeHelper::FindLogicalNode( this, "SimpleAlphaSlider" ) );
	AdvancedVisibilityToggle = safe_cast< System::Windows::Controls::Primitives::ToggleButton^ >( LogicalTreeHelper::FindLogicalNode( this, "AdvancedVisibilityToggle" ) );

	//Set the "next" control properties so that the user can tab from one to the next.
	RedSlider->NextDragSliderControl = GreenSlider;
	GreenSlider->NextDragSliderControl = BlueSlider;
	BlueSlider->NextDragSliderControl = AlphaSlider;
	AlphaSlider->NextDragSliderControl = RedSlider;
	HueSlider->NextDragSliderControl = SaturationSlider;
	SaturationSlider->NextDragSliderControl = BrightnessSlider;
	BrightnessSlider->NextDragSliderControl = HueSlider;
	
	//text version
	HexTextBox = safe_cast< TextBox^ >( LogicalTreeHelper::FindLogicalNode( this, "HexTextBox" ) );
	HexTextBox->KeyUp += gcnew KeyEventHandler( this, &MColorPickerPanel::OnKeyUp);

	//Bind Controls
	UnrealEd::Utils::CreateBinding(RedSlider, CustomControls::DragSlider::ValueProperty, this, "Red" );
	UnrealEd::Utils::CreateBinding(GreenSlider, CustomControls::DragSlider::ValueProperty, this, "Green" );
	UnrealEd::Utils::CreateBinding(BlueSlider, CustomControls::DragSlider::ValueProperty, this, "Blue" );
	UnrealEd::Utils::CreateBinding(AlphaSlider, CustomControls::DragSlider::ValueProperty, this, "Alpha" );
	UnrealEd::Utils::CreateBinding(HueSlider, CustomControls::DragSlider::ValueProperty, this, "Hue" );
	UnrealEd::Utils::CreateBinding(SaturationSlider, CustomControls::DragSlider::ValueProperty, this, "Saturation" );
	UnrealEd::Utils::CreateBinding(BrightnessSlider, CustomControls::DragSlider::ValueProperty, this, "Brightness" );
	
	UnrealEd::Utils::CreateBinding(SimpleSaturationSlider, CustomControls::GradientSlider::ValueProperty, this, "Saturation" );
	UnrealEd::Utils::CreateBinding(SimpleBrightnessSlider, CustomControls::GradientSlider::ValueProperty, this, "Brightness" );
	UnrealEd::Utils::CreateBinding(SimpleAlphaSlider, CustomControls::GradientSlider::ValueProperty, this, "Alpha" );
	
	UnrealEd::Utils::CreateBinding(ColorWheel, CustomControls::ColorWheel::HueProperty, this, "Hue" );
	UnrealEd::Utils::CreateBinding(ColorWheel, CustomControls::ColorWheel::SaturationProperty, this, "Saturation" );
	UnrealEd::Utils::CreateBinding(ColorWheel, CustomControls::ColorWheel::BrightnessProperty, this, "Brightness" );

	


	UnrealEd::Utils::CreateBinding(HexTextBox, TextBox::TextProperty, this, "HexString" );

	// Register for property change callbacks from our properties object
	this->PropertyChanged += gcnew ComponentModel::PropertyChangedEventHandler( this, &MColorPickerPanel::OnColorPickerPropertyChanged );

	//If going to slow, and auto-update is stopped, this event will turn it back on
	RedSlider->MouseUp   += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	GreenSlider->MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	BlueSlider->MouseUp  += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	AlphaSlider->MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	HueSlider->MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	SaturationSlider->MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	BrightnessSlider->MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	ColorWheel->MouseUp += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseUp );
	RedSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	GreenSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	BlueSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	AlphaSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	HueSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	SaturationSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	BrightnessSlider->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );
	ColorWheel->PreviewMouseDown += gcnew MouseButtonEventHandler( this, &MColorPickerPanel::Widget_MouseDown );

	SimpleBrightnessSlider->ValueCommitted += gcnew CustomControls::GradientSlider::ValueCommitted_Handler( this, &MColorPickerPanel::StopDeferringUpdates );
	SimpleSaturationSlider->ValueCommitted += gcnew CustomControls::GradientSlider::ValueCommitted_Handler( this, &MColorPickerPanel::StopDeferringUpdates );
	SimpleAlphaSlider->ValueCommitted += gcnew CustomControls::GradientSlider::ValueCommitted_Handler( this, &MColorPickerPanel::StopDeferringUpdates );

	AdvancedVisibilityToggle->Checked += gcnew RoutedEventHandler( this, &MColorPickerPanel::AdvancedToggled );
	AdvancedVisibilityToggle->Unchecked += gcnew RoutedEventHandler( this, &MColorPickerPanel::AdvancedToggled );

	// Handle changes to the whitepoint
	SimpleBrightnessSlider->WhitepointChanged += gcnew CustomControls::GradientSlider::WhitepointChanged_Handler( this, &MColorPickerPanel::WhitepointChaned );

	bUpdateWhenChanged = TRUE;
	bCaptureColorFromMouse = FALSE;
	bIsMouseButtonDown = FALSE;

	//insert into list of tickable panels
	if (!StaticColorPickerPanelList)
	{
		StaticColorPickerPanelList = gcnew List<MColorPickerPanel^>();
	}
	StaticColorPickerPanelList->Add(this);

	// Should we show the advanced options:
	ReadSettings();
}

//destructor used to remove from the "Tick" array
MColorPickerPanel::~MColorPickerPanel()
{
	//remove from list of tickable panels
	if (StaticColorPickerPanelList!=nullptr)
	{
		StaticColorPickerPanelList->Remove(this);
		if (StaticColorPickerPanelList->Count==0)
		{
			delete StaticColorPickerPanelList;
			StaticColorPickerPanelList = nullptr;
		}
	}
}


/** Updates the window with the new state*/
void MColorPickerPanel::BindData()
{		
	bool bSupportHDR = false;
	if (ColorStruct.FLOATColorArray.Num())
	{
		check(ColorStruct.FLOATColorArray(0));
		FLinearColor FirstColor = *(ColorStruct.FLOATColorArray(0));
		StartColorRed   = FirstColor.R;
		StartColorGreen = FirstColor.G;
		StartColorBlue  = FirstColor.B;
		StartColorAlpha = FirstColor.A;

		bSupportHDR = true;
	}
	if (ColorStruct.PartialFLOATColorArray.Num())
	{
		const FColorChannelStruct& ChannelStruct = ColorStruct.PartialFLOATColorArray(0);
		StartColorRed   = ChannelStruct.Red   ? *(ChannelStruct.Red  ) : 0.0f;
		StartColorGreen = ChannelStruct.Green ? *(ChannelStruct.Green) : 0.0f;
		StartColorBlue  = ChannelStruct.Blue  ? *(ChannelStruct.Blue ) : 0.0f;
		StartColorAlpha = ChannelStruct.Alpha ? *(ChannelStruct.Alpha) : 0.0f;

		bSupportHDR = true;
	}
	else if (ColorStruct.DWORDColorArray.Num())
	{
		check(ColorStruct.DWORDColorArray(0));
		FLinearColor FirstColor = *(ColorStruct.DWORDColorArray(0));
		StartColorRed   = FirstColor.R;
		StartColorGreen = FirstColor.G;
		StartColorBlue  = FirstColor.B;
		StartColorAlpha = FirstColor.A;

		bSupportHDR = false;
	}

	double ColorSliderMin = 0.0;
	double ColorSliderMax = 1.0;
	double ColorValueMin = 0.0;
	double ColorValueMax = 1000.0;
	double ColorIncrement = .01;
	if (ColorStruct.DWORDColorArray.Num() > 0)
	{
		ColorValueMax = 1.0;
	}
	SetSliderRange(RedSlider,   ColorSliderMin, ColorSliderMax, ColorValueMin, ColorValueMax, ColorIncrement);
	SetSliderRange(GreenSlider, ColorSliderMin, ColorSliderMax, ColorValueMin, ColorValueMax, ColorIncrement);
	SetSliderRange(BlueSlider,  ColorSliderMin, ColorSliderMax, ColorValueMin, ColorValueMax, ColorIncrement);
	//Always from 0 to 1
	SetSliderRange(AlphaSlider, ColorSliderMin, ColorSliderMax, ColorSliderMin, ColorSliderMax, ColorIncrement);

	SetSliderRange(HueSlider, 0.0, 360.0, 0.0, 360.0, 1.0);
	SetSliderRange(SaturationSlider, 0.0, 1.0, 0.0, 1.0, .01);
	SetSliderRange(BrightnessSlider, 0.0, 1.0, 0.0, 1.0, .01);

	//disable update during the initial setting
	bUpdateWhenChanged = FALSE;

	//assign first color with pushing data to color pointers
	Red   = StartColorRed;
	Green = StartColorGreen;
	Blue  = StartColorBlue;
	Alpha = StartColorAlpha;

	//Setup color previews
	System::Windows::Shapes::Rectangle^ OldColorRect = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "OldColorRectNoAlpha" ) );
	OldColorRect->Fill = GetHDRColorGradient(1.0f, Red, Green, Blue, true);

	System::Windows::Shapes::Rectangle^ OldColorRectNoAlpha = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "OldColorRect" ) );
	OldColorRectNoAlpha->Fill = GetHDRColorGradient(Alpha, Red, Green, Blue, false);

	UpdateOriginalColorPreview();

	//Update brightness slider
	UpdateSimpleSliders();

	// Adjust the fancy brightness slider as a function 
	SimpleBrightnessSlider->IsVariableRange = bSupportHDR;
	SimpleBrightnessSlider->Whitepoint = 1;
	if (bSupportHDR)
	{
		SimpleBrightnessSlider->AdjustRangeToValue();
	}
	
	//re-enable dynamic updating
	bUpdateWhenChanged = TRUE;

	//Update preview
	UpdateColorPreview();
}

/**Enables/Disables the panel with the dangerous connections*/
void MColorPickerPanel::SetEnabled(const UBOOL bInEnable)
{
	Panel^ DataPanel = safe_cast< Panel^ >( LogicalTreeHelper::FindLogicalNode( this, "DataPanel" ) );
	DataPanel->IsEnabled = bInEnable ? true : false;
}


/**
 * Callback when the parent frame is set to hook up custom events to its widgets
 */
void MColorPickerPanel::SetParentFrame (MWPFFrame^ InParentFrame)
{
	MWPFPanel::SetParentFrame(InParentFrame);
	
	//Make sure to treat the close as a cancel
	Button^ CloseButton = safe_cast< Button^ >( LogicalTreeHelper::FindLogicalNode(InParentFrame->GetRootVisual(), "TitleBarCloseButton" ) );
	CloseButton->Click += gcnew RoutedEventHandler( this, &MColorPickerPanel::CancelClicked );
}

/**Static function to get the number of color picker panels that currently exist*/
UINT MColorPickerPanel::GetNumColorPickerPanels (void)
{
	if (StaticColorPickerPanelList)
	{
		return StaticColorPickerPanelList->Count;
	}
	return 0;
}

/**Static function to get the number of color picker panels that currently exist*/
MColorPickerPanel^ MColorPickerPanel::GetStaticColorPicker (const UINT InIndex)
{
	//if this fails, GetNumColorPickerPanels should have been tested first
	check(StaticColorPickerPanelList);
	check(IsWithin<UINT>(InIndex, 0, StaticColorPickerPanelList->Count));
	return StaticColorPickerPanelList[InIndex];
}

/** 
* Called each frame.  Will get the color under the mouse cursor if we are currently capturing color from the mouse 
*/
void MColorPickerPanel::Tick( )
{
	if (bCaptureColorFromMouse)
	{
		//get full screen DC
		HDC tempDC = GetDC( NULL );
		int SupportsColorManagement = GetDeviceCaps(tempDC, COLORMGMTCAPS);
		if (SupportsColorManagement != CM_NONE)
		{
			POINT CursorPos;
			GetCursorPos(&CursorPos);
			COLORREF CapturedColor = GetPixel(tempDC, CursorPos.x, CursorPos.y);
			//Get the gamma color from the pixel
			FColor TempGammaColor(GetRValue(CapturedColor), GetGValue(CapturedColor), GetBValue(CapturedColor));
			//Convert to linear space
			FLinearColor NewCapturedLinearColor = TempGammaColor;

			//Assign into color values
			Red = NewCapturedLinearColor.R;
			Green = NewCapturedLinearColor.G;
			Blue = NewCapturedLinearColor.B;
			Alpha = 1.0f;
		}
		//release full screen DC
		ReleaseDC( NULL, tempDC );
	}
}

//Helper function to convert to WPF Color
Color MColorPickerPanel::GetBrushColor(const FLOAT InAlphaPercent, const FLOAT InRed, const FLOAT InGreen, const FLOAT InBlue, const UBOOL bUseSrgb)
{
	float ClampedRed;
	float ClampedGreen;
	float ClampedBlue;
	float ClampedAlpha;

	if (bUseSrgb)
	{
		ClampedRed = Clamp<FLOAT>(appPow(InRed,1.0f / 2.2f)*255.0, 0, 255.0);
		ClampedGreen = Clamp<FLOAT>(appPow(InGreen,1.0f / 2.2f)*255.0, 0, 255.0);
		ClampedBlue = Clamp<FLOAT>(appPow(InBlue,1.0f / 2.2f)*255.0, 0, 255.0);
		ClampedAlpha = Clamp<FLOAT>(InAlphaPercent*255.0, 0, 255.0);
	}
	else
	{
		ClampedRed = Clamp<FLOAT>(InRed*255.0, 0, 255.0);
		ClampedGreen = Clamp<FLOAT>(InGreen*255.0, 0, 255.0);
		ClampedBlue = Clamp<FLOAT>(InBlue*255.0, 0, 255.0);
		ClampedAlpha = Clamp<FLOAT>(InAlphaPercent*255.0, 0, 255.0);
	}

	return Color::FromArgb( appTrunc(ClampedAlpha), appTrunc(ClampedRed), appTrunc(ClampedGreen), appTrunc(ClampedBlue) );
}

/**
 * Adds a WPF callback delegate
 */
void MColorPickerPanel::AddCallbackDelegate(ColorPickerPropertyChangeFunction^ InPropertyChangeCallback)
{
	PropertyChangeCallback += InPropertyChangeCallback;
}

/** Called when the settings of the dialog are to be accepted*/
void MColorPickerPanel::OKClicked( Object^ Owner, RoutedEventArgs^ Args )
{
	ParentFrame->Close(ColorPickerConstants::ColorAccepted);
}

/** Called when the settings of the dialog are to be ignored*/
void MColorPickerPanel::CancelClicked( Object^ Owner, RoutedEventArgs^ Args )
{
	//don't update until completely done
	bUpdateWhenChanged = FALSE;
	Red   = StartColorRed;
	Green = StartColorGreen;
	Blue  = StartColorBlue;
	Alpha = StartColorAlpha;
	bUpdateWhenChanged = TRUE;

	UBOOL bStopUpdateBasedOnPerf = FALSE;
	PushColorToDataPtrs(bStopUpdateBasedOnPerf);

	ParentFrame->Close(ColorPickerConstants::ColorRejected);
}

/** Called when the settings of the dialog are to be accepted*/
void MColorPickerPanel::EyeDropperClicked( Object^ Owner, RoutedEventArgs^ Args )
{
	// change the cursor to a crosshair
	this->Cursor = ((TextBlock^)ColorWheel->FindResource("curEyeDropper"))->Cursor;
	bCaptureColorFromMouse = TRUE;
	this->CaptureMouse();
}	

/** Called when a color picker window property is changed */
void MColorPickerPanel::OnColorPickerPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args )
{
	UBOOL bStopUpdateBasedOnPerf = TRUE;

	static UBOOL bPerformingUpdate = FALSE;
	if (!bPerformingUpdate)
	{
		//make sure there is no ping ponging back and forth.  Lock out all other incoming messages
		bPerformingUpdate = TRUE;

		if (String::Compare(Args->PropertyName, "HexString")==0)
		{
			//hex string changed. push value to RGB, and HSV
			ConvertHexStringToRGBA();
			ConvertRGBToHSV();
		}
		else if ((String::Compare(Args->PropertyName, "Hue")==0) || (String::Compare(Args->PropertyName, "Saturation")==0) || (String::Compare(Args->PropertyName, "Brightness")==0))
		{
			//hsv changed.  push value to RGB, and HSV
			ConvertHSVToRGB();
			ConvertRGBAToHexString();
		}
		else
		{
			//rgba has changed.  Push values to HSV and hex
			ConvertRGBToHSV();
			ConvertRGBAToHexString();
		}

		//update the brightness slider so that it ranges from black to a fullbright version of currently selected Hue/Saturation
		UpdateSimpleSliders();

		//reset to allow subsequent updates to make it through 
		bPerformingUpdate = FALSE;

		//Push the internal data to the live data
		PushColorToDataPtrs(bStopUpdateBasedOnPerf);

		// Make sure the color previews are up to datetesting if a window has capture wpf
		UpdateColorPreview();
	}
}

//Mouse Events for releasing capture
void MColorPickerPanel::OnMouseUp( Object^ Sender, MouseButtonEventArgs^ e )
{
	RelinquishCaptureColorFromMouse();
}

/**If we were in capture mode but lost focus, then reclaim mouse capture
 * Without this, when alt tabbing back to the editor, the color capture only works in the color picker window and not the whole desktop
 */
void MColorPickerPanel::OnLostMouseCapture (Object^ Owner, MouseEventArgs^ Args)
{
	RelinquishCaptureColorFromMouse();
}

/**Shared function to relinquish capture color from mouse*/
void MColorPickerPanel::RelinquishCaptureColorFromMouse (void)
{
	if (bCaptureColorFromMouse)
	{			
		// change the cursor back to default
		this->Cursor = Cursors::Arrow;
		bCaptureColorFromMouse = FALSE;
		this->ReleaseMouseCapture();

		//realtime updates are now allowed again
		bUpdateWhenChanged = TRUE;

		UBOOL bStopUpdateBasedOnPerf = FALSE;
		PushColorToDataPtrs(bStopUpdateBasedOnPerf);
	}
}

/**Add ability to commit text ctrls on "ENTER" */
void MColorPickerPanel::OnKeyUp (Object^ Owner, KeyEventArgs^ Args)
{
	if (Args->Key == Key::Return)
	{
		TextBox^ ChangedTextBox = safe_cast< TextBox^ >(Args->Source);
		if (ChangedTextBox != nullptr)
		{
			ChangedTextBox->GetBindingExpression(ChangedTextBox->TextProperty)->UpdateSource();
		}
	}
}

/** Called when mouse button is pressed over the widget */
void MColorPickerPanel::Widget_MouseDown( Object^ Sender, MouseButtonEventArgs^ e )
{
	bIsMouseButtonDown = TRUE;
}

/** Called when mouse button is released over the widget */
void MColorPickerPanel::Widget_MouseUp( Object^ Sender, MouseButtonEventArgs^ e )
{
	bIsMouseButtonDown = FALSE;
	StopDeferringUpdates();
}

/** Commit any changes to the color value and stop deferring color changes */
void MColorPickerPanel::StopDeferringUpdates()
{
	//realtime updates are now allowed again
	bUpdateWhenChanged = TRUE;

	UBOOL bStopUpdateBasedOnPerf = FALSE;
	PushColorToDataPtrs(bStopUpdateBasedOnPerf);
}

/** The whitepoint of a brightness slider changed; update its background */
void MColorPickerPanel::WhitepointChaned( double NewValue )
{
	UpdateSimpleSliders();
}


/**
 * Iterates 
 * @param bStopUpdateBasedOnPerf - If true, the real time update will stop if it takes too long.  Should be false on the mouse up event
 */
void MColorPickerPanel::PushColorToDataPtrs(const UBOOL bStopUpdateBasedOnPerf)
{
	//if we are allowing time-outs (actively dragging on the widget) and it has been requested to DISABLE real-time updates
	if (bStopUpdateBasedOnPerf && ColorStruct.bSendEventsOnlyOnMouseUp && bIsMouseButtonDown)
	{
		bUpdateWhenChanged = FALSE;
	}

	//if we're still allowed to update on the fly because performance is 
	if (bUpdateWhenChanged)
	{
		DOUBLE StartUpdateTime = appSeconds();

		//Tell property window we're about to make changes
		if (ColorStruct.PropertyWindow)
		{
			ColorStruct.PropertyWindow->ChangeActiveCallbackCount(1);
		}

		//cause the parent object to outdate itself
		for (INT ObjectIndex = 0; ObjectIndex < ColorStruct.ParentObjects.Num(); ++ObjectIndex)
		{
			//all provided parent objects MUST be valid
			UObject* ParentObject = ColorStruct.ParentObjects(ObjectIndex);
			check(ParentObject);

			if (ColorStruct.PropertyWindow && ColorStruct.PropertyNode)
			{
				WxPropertyControl* PropertyControl = ColorStruct.PropertyNode->GetNodeWindow();
				UProperty* PropertyToChange = ColorStruct.PropertyNode->GetProperty();
				ColorStruct.PropertyWindow->NotifyPreChange(PropertyControl, PropertyToChange, ParentObject);
			}
			else
			{
				ParentObject->PreEditChange(NULL);
			}
		}

		//push dword colors
		for (INT i = 0; i < ColorStruct.DWORDColorArray.Num(); ++i)
		{
			FColor* TempColor = ColorStruct.DWORDColorArray(i);
			check(TempColor);
			FLinearColor TempLinearColor(Red, Green, Blue, Alpha);
			*TempColor = TempLinearColor;
		}
		//push float colors
		for (INT i = 0; i < ColorStruct.FLOATColorArray.Num(); ++i)
		{
			FLinearColor* TempColor = ColorStruct.FLOATColorArray(i);
			check(TempColor);
			*TempColor = FLinearColor(Red, Green, Blue, Alpha);
		}
		//push partial float colors
		for (INT i = 0; i < ColorStruct.PartialFLOATColorArray.Num(); ++i)
		{
			FColorChannelStruct ChannelStruct = ColorStruct.PartialFLOATColorArray(i);
			if (ChannelStruct.Red)
			{
				(*ChannelStruct.Red) = Red;
			}
			if (ChannelStruct.Green)
			{
				(*ChannelStruct.Green) = Green;
			}
			if (ChannelStruct.Blue)
			{
				(*ChannelStruct.Blue) = Blue;
			}
			if (ChannelStruct.Alpha)
			{
				(*ChannelStruct.Alpha) = Alpha;
			}
		}

		//cause the parent object to outdate itself
		for (INT ObjectIndex = 0; ObjectIndex < ColorStruct.ParentObjects.Num(); ++ObjectIndex)
		{
			//all provided parent objects MUST be valid
			UObject* ParentObject = ColorStruct.ParentObjects(ObjectIndex);
			check(ParentObject);

			if (ColorStruct.PropertyWindow && ColorStruct.PropertyNode)
			{
				WxPropertyControl* PropertyControl = ColorStruct.PropertyNode->GetNodeWindow();
				UProperty* PropertyThatChanged = ColorStruct.PropertyNode->GetProperty();
				const UBOOL bChangesTopology = FALSE;
				FPropertyChangedEvent ChangeEvent(PropertyThatChanged, bChangesTopology);
				ColorStruct.PropertyWindow->NotifyPostChange(PropertyControl, ChangeEvent);
			}
			else
			{
				ParentObject->PostEditChange();
			}
		}

		//Tell property window we're done making changes
		if (ColorStruct.PropertyWindow)
		{
			ColorStruct.PropertyWindow->ChangeActiveCallbackCount(-1);
			ColorStruct.PropertyWindow->RefreshVisibleWindows();
		}

		// Workaround required to refresh item when "View->PropertyWindowOptions->Show All Property Item Buttons" is true
		if (ColorStruct.PropertyNode && ColorStruct.PropertyWindow)
		{
			if (ColorStruct.PropertyNode->HasNodeFlags(EPropertyNodeFlags::Expanded))
			{
				ColorStruct.PropertyWindow->RebuildSubTree(ColorStruct.PropertyNode);
				const UBOOL bExpand = TRUE;
				const UBOOL bRecurse = FALSE;
				ColorStruct.PropertyNode->SetExpanded(bExpand, bRecurse);
			}
		}

		//force 3d viewports to redraw
		GCallbackEvent->Send(CALLBACK_RedrawAllViewports);
		GCallbackEvent->Send(CALLBACK_ColorPickerChanged);

		for (INT WindowIndex = 0; WindowIndex < ColorStruct.RefreshWindows.Num(); ++WindowIndex)
		{
			wxWindow* CurrentRefreshWindow = ColorStruct.RefreshWindows(WindowIndex);
			check( CurrentRefreshWindow );

			//walk up and make sure parent windows redraw themselves
			while(CurrentRefreshWindow)
			{
				CurrentRefreshWindow->Refresh();
				//force the refresh to happen immediately as the normal loop isn't being called
				if (ColorStruct.bModal)
				{
					CurrentRefreshWindow->Update();
				}
				CurrentRefreshWindow = CurrentRefreshWindow->GetParent();
			}
		}

		//push to wpf windows
		if (PropertyChangeCallback)
		{
			PropertyChangeCallback();
		}

		DOUBLE EndUpdateTime = appSeconds();
		if (bStopUpdateBasedOnPerf && (EndUpdateTime - StartUpdateTime >= ColorPickerConstants::MaxAllowedUpdateEventTime))
		{
			bUpdateWhenChanged = FALSE;
		}
	}
}

System::Windows::Media::RadialGradientBrush^ MColorPickerPanel::GetHDRColorGradient(const float InAlpha, const float InRed, const float InGreen, const float InBlue, bool bRightHalf)
{
	System::Windows::Media::RadialGradientBrush^ GradientBrush;
	GradientBrush = gcnew System::Windows::Media::RadialGradientBrush();

	GradientBrush->RadiusX = 1.0;
	GradientBrush->RadiusY = 0.5;

	if (bRightHalf)
	{
		GradientBrush->GradientOrigin = System::Windows::Point(1.0, 0.5);
		GradientBrush->Center = System::Windows::Point(1.0, 0.5);
	}
	else
	{
		GradientBrush->GradientOrigin = System::Windows::Point(0.0, 0.5);
		GradientBrush->Center = System::Windows::Point(0.0, 0.5);
	}

	//add the first key frame being
	GradientBrush->GradientStops->Add( gcnew GradientStop( GetBrushColor(InAlpha, InRed, InGreen, InBlue, ColorStruct.bUseSrgb), 0.0 ) );

	List<float>^ ValuesOverOne = gcnew List<float>();
	if (InRed > 1.0)
	{
		ValuesOverOne->Add(InRed);
	}
	if (InGreen > 1.0)
	{
		ValuesOverOne->Add(InGreen);
	}
	if (InBlue > 1.0)
	{
		ValuesOverOne->Add(InBlue);
	}
	ValuesOverOne->Sort();
	//now that all colors have been added, if there is no valid max, add one
	if (ValuesOverOne->Count == 0)
	{
		ValuesOverOne->Add(1.0f);
	}

	for (INT i = 0; i < ValuesOverOne->Count; ++i)
	{
		FLOAT OneOverValue = 1.0f / ValuesOverOne[i];
		FLOAT Percent = ValuesOverOne[i] / ValuesOverOne[ValuesOverOne->Count - 1];
		GradientBrush->GradientStops->Add( gcnew GradientStop( GetBrushColor(InAlpha, InRed*OneOverValue, InGreen*OneOverValue, InBlue*OneOverValue, ColorStruct.bUseSrgb), Percent ) );
	}

	return GradientBrush;
}

/**
 * Updates the preview button
 */
void MColorPickerPanel::UpdateColorPreview()
{
	System::Windows::Shapes::Rectangle^ NewColorRect = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "NewColorRectNoAlpha" ) );
	NewColorRect->Fill = GetHDRColorGradient(1.0f, Red, Green, Blue, true);

	System::Windows::Shapes::Rectangle^ NewColorRectNoAlpha = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "NewColorRect" ) );
	NewColorRectNoAlpha->Fill = GetHDRColorGradient(Alpha, Red, Green, Blue, false);

	//if this is embedded, keep the "original and new" colors the same since there is only an "active color"
	if (bEmbeddedPanel)
	{
		UpdateOriginalColorPreview();
	}
}

/**
* Updates the preview color (original color)
 */
void MColorPickerPanel::UpdateOriginalColorPreview()
{
	//Setup color previews
	System::Windows::Shapes::Rectangle^ OldColorRect = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "OldColorRectNoAlpha" ) );
	OldColorRect->Fill = gcnew SolidColorBrush( GetBrushColor(1.0f, Red, Green, Blue, ColorStruct.bUseSrgb) );

	System::Windows::Shapes::Rectangle^ OldColorRectNoAlpha = safe_cast< System::Windows::Shapes::Rectangle^ >( LogicalTreeHelper::FindLogicalNode( this, "OldColorRect" ) );
	OldColorRectNoAlpha->Fill = gcnew SolidColorBrush( GetBrushColor(Alpha, Red, Green, Blue, ColorStruct.bUseSrgb) );
}


void MColorPickerPanel::UpdateSimpleSliders()
{
	FLinearColor FullSaturationColor = FLinearColor(Hue, 1.0, Brightness).HSVToLinearRGB();
	FLinearColor NoSaturationColor = FLinearColor(Hue, 0.0, Brightness).HSVToLinearRGB();

	System::Windows::Media::LinearGradientBrush^ SaturationSliderBackground = gcnew System::Windows::Media::LinearGradientBrush();
	SaturationSliderBackground->StartPoint = Point(0,0);
	SaturationSliderBackground->EndPoint = Point(1,0);
	SaturationSliderBackground->GradientStops->Add( gcnew GradientStop( GetBrushColor(1, NoSaturationColor.R, NoSaturationColor.G, NoSaturationColor.B, ColorStruct.bUseSrgb), 0.0 ) );
	SaturationSliderBackground->GradientStops->Add( gcnew GradientStop( GetBrushColor(1, FullSaturationColor.R, FullSaturationColor.G, FullSaturationColor.B, ColorStruct.bUseSrgb), 1.0 ) );
	SimpleSaturationSlider->Background = SaturationSliderBackground;

	FLinearColor FullbrightColor = FLinearColor(Hue, Saturation, 1.0).HSVToLinearRGB();

	System::Windows::Media::LinearGradientBrush^ BrightnessSliderBackground = gcnew System::Windows::Media::LinearGradientBrush();
	BrightnessSliderBackground->StartPoint = Point(0,0);
	BrightnessSliderBackground->EndPoint = Point(SimpleBrightnessSlider->Whitepoint,0);
	BrightnessSliderBackground->GradientStops->Add( gcnew GradientStop( GetBrushColor(1, 0, 0, 0, ColorStruct.bUseSrgb), 0.0 ) );
	BrightnessSliderBackground->GradientStops->Add( gcnew GradientStop( GetBrushColor(1, FullbrightColor.R, FullbrightColor.G, FullbrightColor.B, ColorStruct.bUseSrgb), 1.0 ) );
	SimpleBrightnessSlider->Background = BrightnessSliderBackground;

	System::Windows::Media::LinearGradientBrush^ AlphaSliderBackground = gcnew System::Windows::Media::LinearGradientBrush();
	AlphaSliderBackground->StartPoint = Point(0,0);
	AlphaSliderBackground->EndPoint = Point(1,0);
	AlphaSliderBackground->GradientStops->Add( gcnew GradientStop( GetBrushColor(0, 1, 1, 1, ColorStruct.bUseSrgb), 0.0 ) );
	AlphaSliderBackground->GradientStops->Add( gcnew GradientStop( GetBrushColor(1, 1, 1, 1, ColorStruct.bUseSrgb), 1.0 ) );
	SimpleAlphaSlider->Background = AlphaSliderBackground;

}

void MColorPickerPanel::SetSliderRange(CustomControls::DragSlider^ InSlider, double InSliderMin, double InSliderMax, double InValueMin, double InValueMax, double InIncrement)
{
	InSlider->SliderMin = InSliderMin;
	InSlider->Minimum   = InValueMin;
	InSlider->SliderMax = InSliderMax;
	InSlider->Maximum   = InValueMax;
	InSlider->ValuesPerDragPixel = InIncrement;
}


/** Called when mouse button is released over the widget */
void MColorPickerPanel::AdvancedToggled( Object^ Sender, RoutedEventArgs^ e )
{
	SaveSettings();
	e->Handled = true;
}


/** Load the ColorPicker UI settings */
void MColorPickerPanel::ReadSettings()
{
	UBOOL TmpValue;
	UBOOL ConfigValueFound = GConfig->GetBool(
		TEXT("ColorPickerUI"),
		TEXT("bShowAdvanced"),
		TmpValue,
		GEditorUserSettingsIni);

	if (ConfigValueFound)
	{
		AdvancedVisibilityToggle->IsChecked = (TmpValue == TRUE);
	}
	else
	{
		AdvancedVisibilityToggle->IsChecked = false;
	}
}

/** Save the ColorPicker UI settings */
void MColorPickerPanel::SaveSettings()
{
	GConfig->SetBool(
		TEXT("ColorPickerUI"),
		TEXT("bShowAdvanced"),
		(AdvancedVisibilityToggle->IsChecked.HasValue && AdvancedVisibilityToggle->IsChecked.Value == true),
		GEditorUserSettingsIni);
}

//HexString has changed.  Convert to Red, Green, Blue, and Alpha
void MColorPickerPanel::ConvertHexStringToRGBA(void)
{
	FColor TempColor;
	try
	{
		TempColor.DWColor() = System::Convert::ToUInt32(HexString, 16);
		FLinearColor TempLinearColor = TempColor;
		Red = TempLinearColor.R;
		Green = TempLinearColor.G;
		Blue = TempLinearColor.B;
		Alpha = TempLinearColor.A;

		//set border to standard "ok" color
	}
	catch( ... )
	{
		//set border color to "error" color
	}
}

//RGB has changed.  Convert to HexString
void MColorPickerPanel::ConvertRGBAToHexString(void)
{
	FColor TempColor = FLinearColor(Red, Green, Blue, Alpha);
	HexString = CLRTools::ToString(*FString::Printf( TEXT("%08X"), TempColor.DWColor()));
}

//HSV has changed.  Convert to Red, Green, Blue, and Alpha
void MColorPickerPanel::ConvertHSVToRGB(void)
{
	FLinearColor HSVColor(Hue, Saturation, Brightness);
	FLinearColor RGBColor = HSVColor.HSVToLinearRGB();

	Red = RGBColor.R;
	Green = RGBColor.G;
	Blue = RGBColor.B;
}

//Red, Green, Blue has changed.  Convert to HSV
void MColorPickerPanel::ConvertRGBToHSV(void)
{
	FLinearColor RGBColor(Red, Green, Blue);
	FLinearColor HSVColor = RGBColor.LinearRGBToHSV();

	Hue = HSVColor.R;
	Saturation = HSVColor.G;
	Brightness = HSVColor.B;
}

/**Global pointer to re-usable Color Picker Frame*/
GCRoot(MWPFFrame^) GColorPickerFrame;
/**Global pointer to re-usable Color Picker Panel*/
GCRoot(MColorPickerPanel^) GColorPickerPanel;
/**Reserved for modeless color pickers.  Do not use*/
FPickColorStruct GPickColorStruct;

#endif



/**General function for choosing a dword color.  Implementation can be wx or WPF*/
ColorPickerConstants::ColorPickerResults PickColorWPF(const FPickColorStruct& ColorStruct)
{
	ColorPickerConstants::ColorPickerResults Result= ColorPickerConstants::ColorRejected;
#ifdef __cplusplus_cli
	//guard to verify that we're not in game using edit actor
	if (GIsEditor)
	{
		WPFFrameInitStruct^ Settings = gcnew WPFFrameInitStruct;
		if (!GColorPickerFrame)
		{
			Settings->WindowTitle = CLRTools::ToString(*LocalizeUnrealEd("ColorPicker_Title"));
			Settings->WindowHelpURL = CLRTools::ToString(TEXT("https://udn.epicgames.com/Three/ColorPicker"));

			GColorPickerFrame = gcnew MWPFFrame(GApp->EditorFrame, Settings, TEXT("ColorPicker"));
			//No need to append, as the constructor does that automatically
			const UBOOL bEmbeddedPanel = FALSE;
			
			//No need to append, as the constructor does that automatically
			GColorPickerPanel = gcnew MColorPickerPanel(GPickColorStruct, CLRTools::ToString(TEXT("ColorPickerWindow.xaml")), bEmbeddedPanel);
		}
		else
		{
			GColorPickerFrame->SaveLayout();
		}

		check(GColorPickerFrame);
		check(GColorPickerPanel);

		GColorPickerFrame->Raise();

		GPickColorStruct = ColorStruct;
		GColorPickerPanel->BindData();
		//turn the window on (in case it has been disabled)
		GColorPickerPanel->SetEnabled(TRUE);

		if (ColorStruct.bModal)
		{
			Result = (ColorPickerConstants::ColorPickerResults)GColorPickerFrame->SetContentAndShowModal(GColorPickerPanel, ColorPickerConstants::ColorRejected);
			CloseColorPickers();
		}
		else
		{
			GColorPickerFrame->SetContentAndShow(GColorPickerPanel);
		}

		delete Settings;
	}
#endif
	return Result;
}

/** Called from UUnrealEngine::Tick to tick any open managed color picker windows */
void TickColorPickerWPF()
{
#ifdef __cplusplus_cli
	for (UINT i = 0; i < MColorPickerPanel::GetNumColorPickerPanels(); ++i)
	{
		MColorPickerPanel^ TempPanel = MColorPickerPanel::GetStaticColorPicker(i);
		MWPFFrame^ TempFrame = TempPanel->GetParentFrame();
		// Only tick if there is a valid color picker frame and panel and the window frame is visible
		if (TempPanel && TempFrame && IsWindowVisible( TempFrame->GetWindowHandle()))
		{
			TempPanel->Tick();
		}
	}
#endif
}
/**
 * If the color picker is bound to this window in some way, unbind immediately or the color picker will be in a bad state
 * @param InWindowToUnbindFrom - Window that is being shut down that could leave the color picker in a bad state
 */
void UnBindColorPickers(wxWindow* InWindowToUnbindFrom)
{
#ifdef __cplusplus_cli
	//if a color picker has been summoned already
	if (GColorPickerFrame && GColorPickerPanel)
	{
		if ((GPickColorStruct.PropertyWindow == InWindowToUnbindFrom) || (GPickColorStruct.RefreshWindows.ContainsItem(InWindowToUnbindFrom)))
		{
			//reset the global picker struct to a new empty one
			GPickColorStruct = FPickColorStruct();
			//rebind to empty data
			GColorPickerPanel->BindData();
			//this window is no longer pointing at live data
			GColorPickerPanel->SetEnabled(FALSE);

			//Close for now until we have better disable visuals
			CloseColorPickers();
		}
	}
#endif
}

/**
 * If the color picker is bound to this window in some way, unbind immediately or the color picker will be in a bad state
 * @param InPropertyNodeToUnbindFrom - Property Node to unbind from (in case there is a local rebuild in the property tree)
 */
void UnBindColorPickers(FPropertyNode* InPropertyNodeToUnbindFrom)
{
#ifdef __cplusplus_cli
	//if a color picker has been summoned already
	if (GColorPickerFrame && GColorPickerPanel)
	{
		if (GPickColorStruct.PropertyNode== InPropertyNodeToUnbindFrom)
		{
			//reset the global picker struct to a new empty one
			GPickColorStruct = FPickColorStruct();
			//rebind to empty data
			GColorPickerPanel->BindData();
			//this window is no longer pointing at live data
			GColorPickerPanel->SetEnabled(FALSE);

			//Close for now until we have better disable visuals
			CloseColorPickers();
		}
	}
#endif
}

/**
 * If the color picker is bound to a particular object that is being destroyed, unbind immediately or the color picker will be in a bad state
 * @param InObject - UObject that is being deleted that could leave the color picker in a bad state
 */
void UnBindColorPickers(UObject* InObject)
{
#ifdef __cplusplus_cli
	//if a color picker has been summoned already
	if (GColorPickerFrame && GColorPickerPanel)
	{
		if (GPickColorStruct.ParentObjects.ContainsItem(InObject))
		{
			//reset the global picker struct to a new empty one
			GPickColorStruct = FPickColorStruct();
			//rebind to empty data
			GColorPickerPanel->BindData();
			//this window is no longer pointing at live data
			GColorPickerPanel->SetEnabled(FALSE);

			//Close for now until we have better disable visuals
			CloseColorPickers();
		}
	}
#endif
}


/**
 * Shuts down global pointers before shut down
 */
void CloseColorPickers()
{
#ifdef __cplusplus_cli
	//if a color picker has been summoned already
	if (GColorPickerFrame && GColorPickerPanel)
	{
		delete GColorPickerPanel;
		delete GColorPickerFrame;
		GColorPickerPanel = NULL;
		GColorPickerFrame = NULL;
	}
#endif
}


