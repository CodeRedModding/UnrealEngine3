/*=============================================================================
	HIDControllers.h: Unreal Mac platform interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __HIDCONTROLLERS_H__
#define __HIDCONTROLLERS_H__

// Gameplay controller (joystick/gamepad) defintions and declarations


// Same as in Windows, for compatibility
enum EJoystickType
{
	JOYSTICK_None,
	JOYSTICK_PS2_Old_Converter,
	JOYSTICK_PS2_New_Converter,
	JOYSTICK_X360,
	JOYSTICK_Xbox_Type_S,
	/**Game Caster Virtual Camera*/
	JOYSTICK_GameCaster,
};

#define MAX_CONTROLLERS		4	// At most 4 shown as detected at the same time, the rest is ignored.

struct FButtonInfo
{
	// UnrealEngine button name. Those can be different for different controller types
	FName	Name;

	// Button pressed/released on latest update.
	UBOOL	Value;

	// Button pressed/released on update before that.
	UBOOL	LastValue;

	// Next time a IE_Repeat event should be generated for each button
	DOUBLE	NextRepeatTime;

	// Handle to the way of getting current values.
	void*	HIDElement;
};

struct FAxisInfo
{
	// UnrealEngine axis name. Those can be different for different controller types
	FName	Name;

	// Axis value from last update.
	LONG	Value;

	// Minimal and maximal observed value for this axis.
	LONG	MinValue;
	LONG	MaxValue;

	// Controller-specific adjustment of value.
	float	MinRange;
	float	MaxRange;
	float	DeadZone;

	// Handle to the way of getting current values.
	void*	HIDElement;
};

// Information about a single joystick/gamepad, analogous to what Windows client stores
struct FControllerInfo
{
	// Device ID. Kept after device disconnection to allow detecting it's the same device and connecting it to where it was connected last time.
	long VendorID;
	long ProductID;
	char SerialNumber[32];

	// The joystick's type. This is used to tweak button mappings and axis ranges for specific controller models. I'll try to add as many as I can here.
	EJoystickType ControllerType;

	// Controller Unreal ID, passed to viewport client along with commands. Those are generally 0, 1, 2 or 3, taken from struct position and unchanged.
	INT ControllerID;

	TArray<FButtonInfo>	Buttons;
	TArray<FAxisInfo>	Axes;

	// Pointer to HID structure associated with the controller. If zero, nothing is connected. Refreshed with HID device matching/removal callbacks.
	void* HIDDevice;
};

class HIDControllerInfo
{
public:
	FControllerInfo		Controllers[MAX_CONTROLLERS];

	// This will update all connected controller axis and button states to current values
	void RefreshStates( void );

	// Publis destructor so compiler won't protest
	virtual ~HIDControllerInfo( void );

	// Access function
	static HIDControllerInfo* GetControllerSet( void );

private:

	// Singleton constructor, so we can just call Update and get state, and it'll get initialized exactly when we need it
	static HIDControllerInfo* GHIDControllerSingleton;

	// Pointer to HID structure associated with the controller set. If zero, nothing is connected. Set up in constructor/destructor.
	void* HIDManager;

	HIDControllerInfo( void );
};

#endif
