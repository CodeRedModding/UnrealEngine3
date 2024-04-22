/*=============================================================================
	HIDControllers.cpp: Unreal Mac platform interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//#include "Engine.h"
//#include "MacDrv.h"
#include "Engine.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include "HIDControllers.h"

// ======================================================================
// Specific controller configurations - static data
// ======================================================================

struct ControllerButtonConfiguration
{
	uint32_t	HIDUsage;
	FName*		UnrealButtonName;
};

struct ControllerAxisConfiguration
{
	uint32_t	HIDUsage;
	float		MinRange;
	float		MaxRange;
	float		DeadZone;
	FName*		UnrealAxisName;
};

struct ControllerConfiguration
{
	long VendorID;
	long ProductID;
	long NumberOfAxes;
	long NumberOfButtons;

	EJoystickType Type;

	struct ControllerButtonConfiguration* Buttons;
	struct ControllerAxisConfiguration* Axes;

	bool IsTheDevice( long VendorID, long ProductID, long NumberOfAxes, long NumberOfButtons )
	{
		return ( ( this->VendorID == VendorID ) && ( this->ProductID == ProductID ) && ( this->NumberOfAxes == NumberOfAxes ) && ( this->NumberOfButtons == NumberOfButtons ) );
	}
};

ControllerButtonConfiguration Buttons_XBox360Controller[] = {

	{ 1, &KEY_XboxTypeS_A },
	{ 2, &KEY_XboxTypeS_B },
	{ 3, &KEY_XboxTypeS_X },
	{ 4, &KEY_XboxTypeS_Y },
	{ 5, &KEY_XboxTypeS_LeftShoulder },
	{ 6, &KEY_XboxTypeS_RightShoulder },
	{ 7, &KEY_XboxTypeS_Start },
	{ 8, &KEY_XboxTypeS_Back },
	{ 9, &KEY_XboxTypeS_LeftThumbstick },
	{ 10, &KEY_XboxTypeS_RightThumbstick },
	{ 12, &KEY_XboxTypeS_DPad_Up },
	{ 13, &KEY_XboxTypeS_DPad_Down },
	{ 14, &KEY_XboxTypeS_DPad_Left },
	{ 15, &KEY_XboxTypeS_DPad_Right },
	{ (uint32_t)-1, 0 }
};

ControllerAxisConfiguration Axes_XBox360Controller[] = {

	{ kHIDUsage_GD_X, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_LeftX },
	{ kHIDUsage_GD_Y, 1.0f, -1.0f, 0.1f, &KEY_XboxTypeS_LeftY },
	{ kHIDUsage_GD_Rx, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_RightX },
	{ kHIDUsage_GD_Ry, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_RightY },
	{ kHIDUsage_GD_Z, 0.0f, 1.0f, 0.0f, &KEY_XboxTypeS_LeftTriggerAxis },
	{ kHIDUsage_GD_Rz, 0.0f, 1.0f, 0.0f, &KEY_XboxTypeS_RightTriggerAxis },
	{ (uint32_t)-1, 0.0f, 0.0f, 0.0f, 0 }
};

ControllerConfiguration Config_XBox360Controller = {
	1118,
	654,
	6,
	15,
	JOYSTICK_X360,
	Buttons_XBox360Controller,
	Axes_XBox360Controller
};

ControllerConfiguration Config_XBox360WirelessController = {
	1118,
	655,
	6,
	15,
	JOYSTICK_X360,
	Buttons_XBox360Controller,
	Axes_XBox360Controller
};

ControllerButtonConfiguration Buttons_GenericController[] = {

	{ 1, &KEY_XboxTypeS_A },
	{ 2, &KEY_XboxTypeS_B },
	{ 3, &KEY_XboxTypeS_X },
	{ 4, &KEY_XboxTypeS_Y },
	{ 5, &KEY_XboxTypeS_LeftShoulder },
	{ 6, &KEY_XboxTypeS_RightShoulder },
	{ 7, &KEY_XboxTypeS_Start },
	{ 8, &KEY_XboxTypeS_Back },
	{ 9, &KEY_XboxTypeS_LeftThumbstick },
	{ 10, &KEY_XboxTypeS_RightThumbstick },
	{ 12, &KEY_XboxTypeS_DPad_Up },
	{ 13, &KEY_XboxTypeS_DPad_Down },
	{ 14, &KEY_XboxTypeS_DPad_Left },
	{ 15, &KEY_XboxTypeS_DPad_Right },
	{ (uint32_t)-1, 0 }
};

ControllerAxisConfiguration Axes_GenericController[] = {

	{ kHIDUsage_GD_X, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_LeftX },
	{ kHIDUsage_GD_Y, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_LeftY },
	{ kHIDUsage_GD_Rx, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_RightX },
	{ kHIDUsage_GD_Ry, -1.0f, 1.0f, 0.1f, &KEY_XboxTypeS_RightY },
	{ kHIDUsage_GD_Z, 0.0f, 1.0f, 0.0f, &KEY_XboxTypeS_LeftTriggerAxis },
	{ kHIDUsage_GD_Rz, 0.0f, 1.0f, 0.0f, &KEY_XboxTypeS_RightTriggerAxis },
	{ (uint32_t)-1, 0.0f, 0.0f, 0.0f, 0 }
};

ControllerConfiguration Config_GenericController = {
	-1,
	-1,
	-1,
	-1,
	JOYSTICK_None,
	Buttons_GenericController,
	Axes_GenericController
};

// ======================================================================
// HID helper functions
// ======================================================================

const char* GetElementTypeName( IOHIDElementType ElementType )
{
	switch( ElementType )
	{
	case kIOHIDElementTypeInput_Misc:		return "Input_Misc";
	case kIOHIDElementTypeInput_Button:		return "Input_Button";
	case kIOHIDElementTypeInput_Axis:		return "Input_Axis";
	case kIOHIDElementTypeInput_ScanCodes:	return "Input_ScanCodes";
	case kIOHIDElementTypeOutput:			return "Output";
	case kIOHIDElementTypeFeature:			return "Feature";
	case kIOHIDElementTypeCollection:		return "Collection";
	default:								return "(unknown element type)";
	}
}

const char* GetUsagePageName( uint32_t UsagePage )
{
	switch( UsagePage )
	{
	case kHIDPage_Undefined:			return "Undefined";
	case kHIDPage_GenericDesktop:		return "GenericDesktop";
	case kHIDPage_Simulation:			return "Simulation";
	case kHIDPage_VR:					return "VR";
	case kHIDPage_Sport:				return "Sport";
	case kHIDPage_Game:					return "Game";
	case kHIDPage_KeyboardOrKeypad:		return "Keyboard";
	case kHIDPage_LEDs:					return "LED";
	case kHIDPage_Button:				return "Button";
	case kHIDPage_Ordinal:				return "Ordinal";
	case kHIDPage_Telephony:			return "Telephony";
	case kHIDPage_Consumer:				return "Consumer";
	case kHIDPage_Digitizer:			return "Digitizer";
	case kHIDPage_PID:					return "PID";
	case kHIDPage_Unicode:				return "Unicode";
	case kHIDPage_AlphanumericDisplay:	return "AlphanumericDisplay";
	case kHIDPage_PowerDevice:			return "PowerDevice";
	case kHIDPage_BatterySystem:		return "BatterySystem";
	case kHIDPage_BarCodeScanner:		return "BarCodeScanner";
	case kHIDPage_WeighingDevice:		return "WeighingDevice";
	case kHIDPage_MagneticStripeReader:	return "MagneticStripeReader";
	case kHIDPage_CameraControl:		return "CameraControl";
	case kHIDPage_Arcade:				return "Arcade";
	default:							return "(unknown usage page)";
	}
}

void DumpElementInfo( IOHIDElementRef ElementRef )
{
	{
		char NameBuffer[256] = { 0 };
		CFStringRef ElementName = IOHIDElementGetName( ElementRef );
		if( ElementName && CFStringGetCString( ElementName, NameBuffer, sizeof( NameBuffer ), kCFStringEncodingUTF8 ) )
		{
			debugf( TEXT("HID Device Element: '%s'"), NameBuffer );
		}
		else
		{
			debugf( TEXT("HID Device Element: (no name)") );
		}
	}

	IOHIDElementRef ParentElementRef = IOHIDElementGetParent( ElementRef );
	IOHIDElementCookie Cookie = IOHIDElementGetCookie( ElementRef );
	IOHIDElementType ElementType = IOHIDElementGetType( ElementRef );
	FString ElementTypeName = GetElementTypeName( ElementType );
	IOHIDElementCollectionType CollectionType = IOHIDElementGetCollectionType( ElementRef );
	uint32_t UsagePage = IOHIDElementGetUsagePage( ElementRef );
	uint32_t Usage = IOHIDElementGetUsage( ElementRef );
	FString UsagePageName = GetUsagePageName( UsagePage );
	debugf( TEXT("\tParent: %p, Cookie: %p, Type: %s, CollectionType: %p, Usage Page: %s, Usage: %p"),
		ParentElementRef, Cookie, *ElementTypeName, CollectionType, *UsagePageName, Usage );

	uint32_t Unit = IOHIDElementGetUnit( ElementRef );
	uint32_t UnitExp = IOHIDElementGetUnitExponent( ElementRef );
	CFIndex LogicalMin = IOHIDElementGetLogicalMin( ElementRef );
	CFIndex LogicalMax = IOHIDElementGetLogicalMax( ElementRef );
	CFIndex PhysicalMin = IOHIDElementGetPhysicalMin( ElementRef );
	CFIndex PhysicalMax = IOHIDElementGetPhysicalMax( ElementRef );
	debugf( TEXT("\tUnit: %u, Unite Exp: %u, Logical Min: %d, Logical Max: %d, Physical Min: %d, Physical Max: %d"),
		Unit, UnitExp, LogicalMin, LogicalMax, PhysicalMin, PhysicalMax );
	
	Boolean IsVirtual = IOHIDElementIsVirtual( ElementRef );
	Boolean IsRelative = IOHIDElementIsRelative( ElementRef );
	Boolean IsWrapping = IOHIDElementIsWrapping( ElementRef );
	Boolean IsArray = IOHIDElementIsArray( ElementRef );
	Boolean IsNonLinear = IOHIDElementIsNonLinear( ElementRef );
	Boolean HasPreferredState = IOHIDElementHasPreferredState( ElementRef );
	Boolean HasNullState = IOHIDElementHasNullState( ElementRef );
	FString StrTrue = "True";
	FString StrFalse = "False";
	debugf( TEXT("\tVirtual: %s, Relative: %s, Wrapping: %s, Array: %s, Non-linear: %s, Has Preferred State: %s, Has Null State: %s"),
		IsVirtual ? *StrTrue : *StrFalse, IsRelative ? *StrTrue : *StrFalse, IsWrapping ? *StrTrue : *StrFalse, IsArray ? *StrTrue : *StrFalse,
		IsNonLinear ? *StrTrue : *StrFalse, HasPreferredState ? *StrTrue : *StrFalse, HasNullState ? *StrTrue : *StrFalse );
}

static CFMutableDictionaryRef CreateDeviceMatchingDictionary( UInt32 UsagePage, UInt32 Usage )
{
	// Create a dictionary to add usage page/usages to
	CFMutableDictionaryRef Dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	if( !Dict )
	{
		return 0;
	}

	// Add key for device type to refine the matching dictionary.
	CFNumberRef PageCFNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &UsagePage );
	if( !PageCFNumberRef )
	{
		CFRelease( Dict );
		return 0;
	}

	CFDictionarySetValue( Dict, CFSTR( kIOHIDDeviceUsagePageKey ), PageCFNumberRef );
	CFRelease( PageCFNumberRef );

	// Note: the usage is only valid if the usage page is also defined
	CFNumberRef UsageCFNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &Usage );
	if( !UsageCFNumberRef )
	{
		CFRelease( Dict );
		return 0;
	}

	CFDictionarySetValue( Dict, CFSTR( kIOHIDDeviceUsageKey ), UsageCFNumberRef );
	CFRelease( UsageCFNumberRef );

    return Dict;
}

static void Handle_DeviceMatchingCallback( void* Context, IOReturn Result, void* Sender, IOHIDDeviceRef DeviceRef )
{
	HIDControllerInfo* HID = (HIDControllerInfo*)Context;

	// Get identifying information for the device
	long VendorID = 0;
	{
		CFTypeRef Property = IOHIDDeviceGetProperty( DeviceRef, CFSTR( kIOHIDVendorIDKey ) );
		if( Property && ( CFNumberGetTypeID() == CFGetTypeID( Property ) ) )
		{
			CFNumberGetValue( (CFNumberRef)Property, kCFNumberSInt32Type, &VendorID );
		}
	}

	long ProductID = 0;
	{
		CFTypeRef Property = IOHIDDeviceGetProperty( DeviceRef, CFSTR( kIOHIDProductIDKey ) );
		if( Property && ( CFNumberGetTypeID() == CFGetTypeID( Property ) ) )
		{
			CFNumberGetValue( (CFNumberRef)Property, kCFNumberSInt32Type, &ProductID );
		}
	}

	char SerialNumber[256];
	{
		bzero( SerialNumber, 32 );
		CFStringRef SerialNumberStr = (CFStringRef)IOHIDDeviceGetProperty( DeviceRef, CFSTR( kIOHIDSerialNumberKey ) );
		if( SerialNumberStr )
		{
			check( CFStringGetLength( SerialNumberStr ) < 256 );
			Boolean Result = CFStringGetCString( SerialNumberStr, SerialNumber, 256, kCFStringEncodingUTF8 );
			check( Result == true );
		}
	}

	// Iterate over unconnected controller devices to see if there's one with identifying information matching the new one. If there is,
	// connect it as the one and return.
	int UnconnectedDeviceIndex = -1;
	for( int i = 0; i < MAX_CONTROLLERS; ++i )
	{
		FControllerInfo* ControllerInfo = &HID->Controllers[i];
		if( ControllerInfo->HIDDevice )
		{
			continue;
		}

		if( ( ControllerInfo->VendorID == VendorID ) && ( ControllerInfo->ProductID == ProductID ) && !memcmp( SerialNumber, ControllerInfo->SerialNumber, 32 ) )
		{
			UnconnectedDeviceIndex = i;
			break;
		}

		if( UnconnectedDeviceIndex == -1 )
		{
			UnconnectedDeviceIndex = i;
		}
	}

	if( UnconnectedDeviceIndex == -1 )
	{
		return;	// there's no place in the public controller set for a new device. Ignoring it.
	}

	// Open device and associate it with specific controller position
	{
		IOReturn Result = IOHIDDeviceOpen( DeviceRef, kIOHIDOptionsTypeNone );
		if( Result != kIOReturnSuccess )
		{
			return;	// couldn't open the device
		}
	}

	// Analyze device to determine what type it is. You can use number of buttons and axes reported and manufacturer ID for that.
	{
		FControllerInfo* ControllerInfo = &HID->Controllers[UnconnectedDeviceIndex];

		// Get all elements from the device
		CFArrayRef Elements = IOHIDDeviceCopyMatchingElements( DeviceRef, NULL, 0 );
		if( !Elements )
		{
			IOHIDDeviceClose( DeviceRef, kIOHIDOptionsTypeNone );
			return;
		}

		CFIndex ElementsCount = CFArrayGetCount( Elements );

		int NumberOfAxes = 0;
		int NumberOfButtons = 0;

		for( CFIndex ElementIndex = 0; ElementIndex < ElementsCount; ++ElementIndex )
		{
			IOHIDElementRef Element = (IOHIDElementRef)CFArrayGetValueAtIndex( Elements, ElementIndex );
			IOHIDElementType ElementType = IOHIDElementGetType( Element );
			uint32_t UsagePage = IOHIDElementGetUsagePage( Element );

			if( ( ElementType == kIOHIDElementTypeInput_Button ) && ( UsagePage == kHIDPage_Button ) )
			{
				++NumberOfButtons;
			}
			else if( ( ( ElementType == kIOHIDElementTypeInput_Axis ) || ( ElementType == kIOHIDElementTypeInput_Misc ) ) && ( UsagePage == kHIDPage_GenericDesktop ) )
			{
				++NumberOfAxes;
			}
		}

		ControllerInfo->Buttons.Empty();
		ControllerInfo->Axes.Empty();

		debugf( TEXT("Detected HID device. Vendor ID is: %d. Product Id is: %d. Number of axes: %d. Number of buttons: %d"), VendorID, ProductID, NumberOfAxes, NumberOfButtons );

		// Compare this information to known gamepads
		ControllerConfiguration* Config = &Config_GenericController;
		if( Config_XBox360Controller.IsTheDevice( VendorID, ProductID, NumberOfAxes, NumberOfButtons ) )
			Config = &Config_XBox360Controller;
		if( Config_XBox360WirelessController.IsTheDevice( VendorID, ProductID, NumberOfAxes, NumberOfButtons ) )
			Config = &Config_XBox360WirelessController;

		// Now configure the device of detected type, or use a safe generic configuration.
		ControllerInfo->ControllerType = Config->Type;
		for( CFIndex ElementIndex = 0; ElementIndex < ElementsCount; ++ElementIndex )
		{
			IOHIDElementRef Element = (IOHIDElementRef)CFArrayGetValueAtIndex( Elements, ElementIndex );

			DumpElementInfo( Element );

			IOHIDElementType ElementType = IOHIDElementGetType( Element );
			uint32_t UsagePage = IOHIDElementGetUsagePage( Element );
			uint32_t Usage = IOHIDElementGetUsage( Element );

			if( ( ElementType == kIOHIDElementTypeInput_Button ) && ( UsagePage == kHIDPage_Button ) )
			{
				// We're interested in buttons...
				FButtonInfo Button;
				Button.Value = FALSE;		// Set them up as not pressed even if they are, so we'll be able to fill NextRepeatTime properly in Update with given time.
				Button.LastValue = FALSE;
				Button.NextRepeatTime = 0.f;	// doesnt really matter, will be set up on RefreshState()
				Button.HIDElement = (void*)Element;

				bool Valid = false;
				ControllerButtonConfiguration* Buttons = Config->Buttons;
				while( Buttons->HIDUsage != (uint32_t)-1 )
				{
					if( Buttons->HIDUsage == Usage )
					{
						Button.Name = *Buttons->UnrealButtonName;
						Valid = true;
						break;
					}
					++Buttons;
				}

				if( Valid )
				{
					ControllerInfo->Buttons.Push( Button );
				}
			}
			else if( ( ( ElementType == kIOHIDElementTypeInput_Axis ) || ( ElementType == kIOHIDElementTypeInput_Misc ) ) && ( UsagePage == kHIDPage_GenericDesktop ) )
			{
				// ... and axes
				IOHIDValueRef Value;
				IOHIDDeviceGetValue( DeviceRef, Element, &Value );

				FAxisInfo Axis;
				Axis.Value = IOHIDValueGetIntegerValue( Value );
				Axis.MinValue = IOHIDElementGetPhysicalMin( Element );
				Axis.MaxValue = IOHIDElementGetPhysicalMax( Element );
				Axis.HIDElement = (void*)Element;
				
				bool Valid = false;
				ControllerAxisConfiguration* Axes = Config->Axes;
				while( Axes->HIDUsage != (uint32_t)-1 )
				{
					if( Axes->HIDUsage == Usage )
					{
						Axis.Name = *Axes->UnrealAxisName;
						Axis.MinRange = Axes->MinRange;
						Axis.MaxRange = Axes->MaxRange;
						Axis.DeadZone = Axes->DeadZone;
						Valid = true;
						break;
					}
					++Axes;
				}

				if( Valid )
				{
					ControllerInfo->Axes.Push( Axis );
				}
			}
		}

		ControllerInfo->HIDDevice = (void*)DeviceRef;
	}
}

static void Handle_DeviceRemovalCallback( void* Context, IOReturn Result, void* Sender, IOHIDDeviceRef DeviceRef )
{
	HIDControllerInfo* HID = (HIDControllerInfo*)Context;

	// Iterate over unconnected controller devices to see if there's one with identifying information matching the disconnecting one. If there is,
	// disconnect it and return.
	for( int i = 0; i < MAX_CONTROLLERS; ++i )
	{
		FControllerInfo* ControllerInfo = &HID->Controllers[i];
		if( ControllerInfo->HIDDevice == DeviceRef )
		{
//			IOHIDDeviceClose( DeviceRef, kIOHIDOptionsTypeNone );	// tested - don't call it, this crashes the game. Apparently it's close already and it's jut a notification.
			ControllerInfo->HIDDevice = 0;	// forget about the device
			break;
		}
	}
}

// ======================================================================
// Main class functions
// ======================================================================

HIDControllerInfo* HIDControllerInfo::GHIDControllerSingleton = 0;

HIDControllerInfo::HIDControllerInfo( void )
{
	for( int i = 0; i < MAX_CONTROLLERS; ++i )
	{
		bzero( &Controllers[i], sizeof(FControllerInfo) );	// this sets them to default - unconfigured, unconnected.
		Controllers[i].ControllerID = i;
	}

	// Init HID Manager
	IOHIDManagerRef IOHIDManager = IOHIDManagerCreate( kCFAllocatorDefault, 0L );
	if( !IOHIDManager )
	{
		return;	// This will cause all subsequent calls to return information that nothing's connected
	}

	// Set HID Manager to detect devices of two distinct types: Gamepads and joysticks
	CFMutableArrayRef MatchingArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if( !MatchingArray )
	{
		CFRelease( IOHIDManager );
		HIDManager = 0;
		return;
	}

	{
		CFDictionaryRef MatchingJoysticks = CreateDeviceMatchingDictionary( kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick );
		if( !MatchingJoysticks )
		{
			CFRelease( MatchingArray );
			CFRelease( IOHIDManager );
			HIDManager = 0;
			return;
		}
		CFArrayAppendValue( MatchingArray, MatchingJoysticks );
		CFRelease( MatchingJoysticks );
	}

	{
		CFDictionaryRef MatchingGamepads = CreateDeviceMatchingDictionary( kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad );
		if( !MatchingGamepads )
		{
			CFRelease( MatchingArray );
			CFRelease( IOHIDManager );
			HIDManager = 0;
			return;
		}
		CFArrayAppendValue( MatchingArray, MatchingGamepads );
		CFRelease( MatchingGamepads );
	}

	IOHIDManagerSetDeviceMatchingMultiple( IOHIDManager, MatchingArray );
	CFRelease( MatchingArray );

	// Setup HID Manager's add/remove devices callbacks
	IOHIDManagerRegisterDeviceMatchingCallback( IOHIDManager, Handle_DeviceMatchingCallback, this );
	IOHIDManagerRegisterDeviceRemovalCallback( IOHIDManager, Handle_DeviceRemovalCallback, this );

	// Add HID Manager to run loop
	IOHIDManagerScheduleWithRunLoop( IOHIDManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );

	// Open HID Manager - this will cause add device callbacks for all presently connected devices
	IOReturn Result = IOHIDManagerOpen( IOHIDManager, kIOHIDOptionsTypeNone );
	if( Result != kIOReturnSuccess )
	{
		IOHIDManagerUnscheduleFromRunLoop( IOHIDManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
		IOHIDManagerRegisterDeviceMatchingCallback( IOHIDManager, NULL, NULL );
		IOHIDManagerRegisterDeviceRemovalCallback( IOHIDManager, NULL, NULL );
		CFRelease( IOHIDManager );
		HIDManager = 0;
		return;
	}

	HIDManager = (void*)IOHIDManager;
}

HIDControllerInfo::~HIDControllerInfo( void )
{
	if( HIDManager )
	{
		IOHIDManagerRef IOHIDManager = (IOHIDManagerRef)HIDManager;

		// Unschedule HID Manager from run loop
		IOHIDManagerUnscheduleFromRunLoop( IOHIDManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );

		// Reset HID Manager's add/remove devices callbacks
		IOHIDManagerRegisterDeviceMatchingCallback( IOHIDManager, NULL, NULL );
		IOHIDManagerRegisterDeviceRemovalCallback( IOHIDManager, NULL, NULL );

		// Close and release HID Manager. We forget about it even if close fails.
		IOHIDManagerClose( IOHIDManager, 0L );
		CFRelease( IOHIDManager );
		HIDManager = 0;
	}
}

HIDControllerInfo* HIDControllerInfo::GetControllerSet( void )
{
	if( !GHIDControllerSingleton )
		GHIDControllerSingleton = new HIDControllerInfo;
	return GHIDControllerSingleton;
}

void HIDControllerInfo::RefreshStates( void )
{
	if( !HIDManager )
		return;

	// For each public controller devices, if the device is connected, get values of all its registered buttons and axis.
	for( int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLERS; ++ControllerIndex )
	{
		FControllerInfo* ControllerInfo = &Controllers[ControllerIndex];
		if( !ControllerInfo->HIDDevice )
		{
			continue;	// unconnected controller
		}

		IOHIDDeviceRef DeviceRef = (IOHIDDeviceRef)ControllerInfo->HIDDevice;

		// Get current value of all axes
		for( int AxisIndex = 0; AxisIndex < ControllerInfo->Axes.Num(); ++AxisIndex )
		{
			FAxisInfo& Axis = ControllerInfo->Axes(AxisIndex);

			IOHIDValueRef Value;
			IOReturn Result = IOHIDDeviceGetValue( DeviceRef, (IOHIDElementRef)Axis.HIDElement, &Value );
			if( Result == kIOReturnSuccess )
			{
				Axis.Value = IOHIDValueGetIntegerValue( Value );
				if( Axis.MinValue > Axis.Value )
				{
					Axis.MinValue = Axis.Value;
				}
				if( Axis.MaxValue < Axis.Value )
				{
					Axis.MaxValue = Axis.Value;
				}

				// Cut out the dead zone, if applicable
				if( Axis.DeadZone > 0.f )
				{
					if( Axis.Value < 0 )
					{
						if( fabs((float)Axis.Value/Axis.MinValue) < Axis.DeadZone )
						{
							Axis.Value = 0.f;
						}
					}
					else
					{
						if( fabs((float)Axis.Value/Axis.MaxValue) < Axis.DeadZone )
						{
							Axis.Value = 0.f;
						}
					}
				}

//				debugf( TEXT("Axis %d has min %d < val %d < max %d"), AxisIndex, Axis.MinValue, Axis.Value, Axis.MaxValue );
			}
		}

		// Get current value of all buttons
		for( int ButtonIndex = 0; ButtonIndex < ControllerInfo->Buttons.Num(); ++ButtonIndex )
		{
			FButtonInfo& Button = ControllerInfo->Buttons(ButtonIndex);

			IOHIDValueRef Value;
			IOReturn Result = IOHIDDeviceGetValue( DeviceRef, (IOHIDElementRef)Button.HIDElement, &Value );
			if( Result == kIOReturnSuccess )
			{
				bool Pressed = ( IOHIDValueGetIntegerValue( Value ) > 0 );
				Button.Value = Pressed;
/*
				if( Pressed )
				{
					uint32_t Usage = IOHIDElementGetUsage( Button.HIDElement );
					debugf( TEXT("Button %d has changed state"), Usage );
				}
*/
			}
		}
	}
}
