/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __COLOR_PICKER_SHARED_H__
#define __COLOR_PICKER_SHARED_H__

/**General function for choosing a dword color.  Implementation can be wx or WPF*/
ColorPickerConstants::ColorPickerResults PickColorWPF(const FPickColorStruct& ColorStruct);

/** Called from UUnrealEngine::Tick to tick any open managed color picker windows */
void TickColorPickerWPF();

/**
 * If the color picker is bound to this window in some way, unbind immediately or the color picker will be in a bad state
 * @param InWindowToUnbindFrom - Window that is being shut down that could leave the color picker in a bad state
 */
void UnBindColorPickers(wxWindow* InWindowToUnbindFrom);

/**
 * If the color picker is bound to this window in some way, unbind immediately or the color picker will be in a bad state
 * @param InPropertyNodeToUnbindFrom - Property Node to unbind from (in case there is a local rebuild in the property tree)
 */
void UnBindColorPickers(FPropertyNode* InPropertyNodeToUnbindFrom);

/**
 * If the color picker is bound to a particular object that is being destroyed, unbind immediately or the color picker will be in a bad state
 * @param InObject - UObject that is being deleted that could leave the color picker in a bad state
 */
void UnBindColorPickers(UObject* InObject);

/**
 * Shuts down global pointers before shut down
 */
void CloseColorPickers();


#ifdef __cplusplus_cli


#include "UnrealEdCLR.h"
#include "ManagedCodeSupportCLR.h"
#include "WPFFrameCLR.h"

using namespace System::Windows::Input;

delegate void ColorPickerPropertyChangeFunction();

/**
 * A color picker panel that can be summoned within a floating dialog.
 */
ref class MColorPickerPanel : public MWPFPanel
{
public:
	MColorPickerPanel(const FPickColorStruct& InColorStruct, String^ InXamlName, const UBOOL bInEmbeddedPanel);

	//destructor used to remove from the "Tick" array
	~MColorPickerPanel();

	/** Updates the window with the new state*/
	void BindData();

	/**Enables/Disables the panel with the dangerous connections*/
	void SetEnabled(const UBOOL bInEnable);

	/**
	 * Callback when the parent frame is set to hook up custom events to its widgets
	 */
	virtual void SetParentFrame (MWPFFrame^ InParentFrame) override;
	
	/**Static function to get the number of color picker panels that currently exist*/
	static UINT GetNumColorPickerPanels (void);

	/**Static function to get the number of color picker panels that currently exist*/
	static MColorPickerPanel^ GetStaticColorPicker (const UINT InIndex);

	/** 
	* Called each frame.  Will get the color under the mouse cursor if we are currently capturing color from the mouse 
	*/
	void Tick();

	//Helper function to convert to WPF Color
	static Color GetBrushColor(const FLOAT InAlphaPercent, const FLOAT InRed, const FLOAT InGreen, const FLOAT InBlue, UBOOL bUseSrgb);

	/**
	 * Adds a WPF callback delegate
	 */
	void AddCallbackDelegate(ColorPickerPropertyChangeFunction^ InPropertyChangeCallback);

protected:

	/** Called when the settings of the dialog are to be accepted*/
	void OKClicked( Object^ Owner, RoutedEventArgs^ Args );

	/** Called when the settings of the dialog are to be ignored*/
	void CancelClicked( Object^ Owner, RoutedEventArgs^ Args );

	/** Called when the settings of the dialog are to be accepted*/
	void EyeDropperClicked( Object^ Owner, RoutedEventArgs^ Args );

	/** Called when a color picker window property is changed */
	void OnColorPickerPropertyChanged( Object^ Owner, ComponentModel::PropertyChangedEventArgs^ Args );

	//Mouse Events for releasing capture
	void OnMouseUp( Object^ Sender, MouseButtonEventArgs^ e );

	/**If we were in capture mode but lost focus, then reclaim mouse capture
	 * Without this, when alt tabbing back to the editor, the color capture only works in the color picker window and not the whole desktop
	 */
	void OnLostMouseCapture (Object^ Owner, MouseEventArgs^ Args);

	/**Shared function to relinquish capture color from mouse*/
	void RelinquishCaptureColorFromMouse (void);

	/**Add ability to commit text ctrls on "ENTER" */
	void OnKeyUp (Object^ Owner, KeyEventArgs^ Args);
	
	/** Called when mouse button is pressed over the widget */
	void Widget_MouseDown( Object^ Sender, MouseButtonEventArgs^ e );

	/** Called when mouse button is released over the widget */
	void Widget_MouseUp( Object^ Sender, MouseButtonEventArgs^ e );

	/** Commit any changes to the color value and stop deferring color changes */
	void StopDeferringUpdates();

	/** The whitepoint of a brightness slider changed; update its background */
	void WhitepointChaned( double NewValue );

	/**
	 * Iterates 
	 * @param bStopUpdateBasedOnPerf - If true, the real time update will stop if it takes too long.  Should be false on the mouse up event
	 */
	void PushColorToDataPtrs(const UBOOL bStopUpdateBasedOnPerf);
	
	/** Makes a radial brush based on the input color (HDR complient) */
	System::Windows::Media::RadialGradientBrush^ GetHDRColorGradient(const float InAlpha, const float InRed, const float InGreen, const float InBlue, bool bRightHalf);

	/**
	 * Updates the preview button
	 */
	void UpdateColorPreview();

	/**
	* Updates the preview color (original color)
	 */
	void UpdateOriginalColorPreview();

	void UpdateSimpleSliders();
	
	void SetSliderRange(CustomControls::DragSlider^ InSlider, double InSliderMin, double InSliderMax, double InValueMin, double InValueMax, double InIncrement);

private:

	/** Called when mouse button is released over the widget */
	void AdvancedToggled( Object^ Sender, RoutedEventArgs^ e );


	/** Load the ColorPicker UI settings */
	void ReadSettings();

	/** Save the ColorPicker UI settings */
	void SaveSettings();

	//HexString has changed.  Convert to Red, Green, Blue, and Alpha
	void ConvertHexStringToRGBA(void);

	//RGB has changed.  Convert to HexString
	void ConvertRGBAToHexString(void);

	//HSV has changed.  Convert to Red, Green, Blue, and Alpha
	void ConvertHSVToRGB(void);

	//Red, Green, Blue has changed.  Convert to HSV
	void ConvertRGBToHSV(void);	

	/** Internal widgets to save having to get in multiple places*/
	CustomControls::DragSlider^ RedSlider;
	CustomControls::DragSlider^ GreenSlider;
	CustomControls::DragSlider^ BlueSlider;
	CustomControls::DragSlider^ AlphaSlider;
	CustomControls::DragSlider^ HueSlider;
	CustomControls::DragSlider^ SaturationSlider;
	CustomControls::DragSlider^ BrightnessSlider;
	TextBox^ HexTextBox;
	CustomControls::ColorWheel^ ColorWheel;
	CustomControls::GradientSlider^ SimpleBrightnessSlider;
	CustomControls::GradientSlider^ SimpleSaturationSlider;
	CustomControls::GradientSlider^ SimpleAlphaSlider;
	System::Windows::Controls::Primitives::ToggleButton^ AdvancedVisibilityToggle;

	//r, g, b, a
	double StartColorRed;
	double StartColorGreen;
	double StartColorBlue;
	double StartColorAlpha;

	//static list of active color picker panels
	static List<MColorPickerPanel^>^ StaticColorPickerPanelList;

	// Are the current interactions caused by the brightness slider?
	bool bBrightnessSliderCausedValueChange;

	//whether or not this color picker is embedded inside another panel
	UBOOL bEmbeddedPanel;

	// Bindable properties
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Red);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Green);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Blue);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Alpha);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( String^, HexString);

	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Hue);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Saturation);
	DECLARE_SIMPLE_NOTIFY_PROPERTY_AND_VALUE( FLOAT, Brightness);


	UBOOL bUpdateWhenChanged;
	UBOOL bCaptureColorFromMouse;
	UBOOL bIsMouseButtonDown;

	ColorPickerPropertyChangeFunction^ PropertyChangeCallback;

	const FPickColorStruct& ColorStruct;
};

#endif //__cplusplus_cli

#endif // __COLOR_PICKER_SHARED_H__

