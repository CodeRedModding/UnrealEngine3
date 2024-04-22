/*=============================================================================
 IPhoneObjCWrapper.h: iPhone wrapper for making ObjC calls from C++ code
 Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/


#ifndef __IPHONE_OBJC_WRAPPER_H__
#define __IPHONE_OBJC_WRAPPER_H__

#import <stdint.h>

#define IPHONE_PATH_MAX 1024

// Create a special build for Apple's battery test lab. Also change the flag in SwordGui.uci
#define APPLE_BATTERY_TEST_BUILD 0

/**
 * Possible iOS devices. Must match the IOSDeviceNames array!
 */
enum EIOSDevice
{
	IOS_IPhone3GS,
	IOS_IPhone4,
	IOS_IPhone4S,
	IOS_IPhone5,
	IOS_IPad,
	IOS_IPodTouch4,
	IOS_IPodTouch5,
	IOS_IPad2,
	IOS_IPad3,
	IOS_IPad4,
	IOS_IPadMini,
	IOS_Unknown,
};

/** If set to something other than IOS_Unknown, allows overriding the device type for testing purposes. */
extern EIOSDevice GOverrideIOSDevice;

/** If set to something other than 0.0f, allows overriding the device iOS version for testing purposes. */
extern float GOverrideIOSVersion;

/**
 * Get the path to the .app where file loading occurs
 * 
 * @param AppDir [out] Return path for the application directory that is the root of file loading
 * @param MaxLen Size of AppDir buffer
 */
void IPhoneGetApplicationDirectory( char *AppDir, int MaxLen );

/**
 * Get the path to the user document directory where file saving occurs
 * 
 * @param DocDir [out] Return path for the application directory that is the root of file saving
 * @param MaxLen Size of DocDir buffer
 */
void IPhoneGetDocumentDirectory( char *DocDir, int MaxLen );

/**
 * Creates a directory (must be in the Documents directory
 *
 * @param Directory Path to create
 * @param bMakeTree If true, it will create intermediate directory
 *
 * @return true if successful)
 */
bool IPhoneCreateDirectory(char* Directory, bool bMakeTree);

/**
 * Retrieve current memory information (for just this task)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetTaskMemoryInfo(uint64_t& ResidentSize, uint64_t& VirtualSize);

/**
 * Retrieve current memory information (for the entire device, not limited to our process)
 *
 * @param FreeMemory Amount of free memory in bytes
 * @param UsedMemory Amount of used memory in bytes
 */
void IPhoneGetPhysicalMemoryInfo( uint64_t & FreeMemory, uint64_t & UsedMemory );

/**
 * Enables or disables the view autorotation when the user rotates the view
 */
void IPhoneSetRotationEnabled(int bEnabled);

/**
 * Launch a URL for the given Tag
 *
 * @param Tag String describing what URL to launch
 * @param bProcessRedirectsLocally If TRUE, redirects are loaded and processed before launching the external app. Note that this can add a delay before switching apps, but it will reduce flicker/app swaps if the URL is actually a redirect
 */
void IPhoneLaunchURL(const char* LaunchURL, UBOOL bProcessRedirectsLocally);

/**
 * Save a key/value string pair to the user's settings
 *
 * @param Key Name of the setting
 * @param Value Value to save to the setting
 */
void IPhoneSaveUserSetting(const char* Key, const char* Value);

/**
 * Load a value from the user's settings for the given Key
 *
 * @param Key Name of the setting
 * @param Value [out] String to put the loaded value into
 * @param MaxValueLen Size of the OutValue string
 */
void IPhoneLoadUserSetting(const char* Key, char* OutValue, int MaxValueLen);

/**
 * Convenience wrapper around IPhoneLoadUserSetting for integers
 * NOTE: strtoull returns 0 if it can't parse the int (this will be the default when we first load)
 * 
 * @param Name Name of the setting
 * @return Setting value as uint64_t
 */
uint64_t IPhoneLoadUserSettingU64(const char* Name);

/**
 * Convenience wrapper around IPhoneLoadUserSetting for integers
 * NOTE: strtoull returns 0 if it can't parse the int (this will be the default when we first load)
 * 
 * @param Name Name of the setting
 * @param Min Lower clamp value
 * @param Max Upper clamp value
 * @return Setting value as uint64_t
 */
uint64_t IPhoneLoadUserSettingU64Clamped(const char* Name, uint64_t Min, uint64_t Max);

/**
 * Convenience wrapper around IPhoneSaveUserSetting for integers
 * 
 * @param Name The name of the setting
 * @param Value The new setting value
 */
void IPhoneSaveUserSettingU64(const char* Name, uint64_t Value);

/**
 * Convenience wrapper around IPhoneLoadUserSettingU64 and IPhoneSaveUserSettingU64
 *
 * @param Name The name of the setting
 * @param By How much to increment the setting by
 */
uint64_t IPhoneIncrementUserSettingU64(const char* Name, uint64_t By = 1);

/**
 * Scales the volume for mp3s
 *
 * @param VolumeMultiplier Amount to scale the volume (0.0 - 1.0)
 */
void IPhoneScaleMusicVolume(const char* VolumeMultiplier);

/**
 * Plays an mp3 in hardware
 *
 * @param SongName Name of the mp3 to play, WITHOUT path or extension info
 */
void IPhonePlaySong(const char* SongName);

/**
 * Stops any current mp3 playing
 */
void IPhoneStopSong();

/**
 * Pauses the hardware mp3 stream
 */
void IPhonePauseSong();

/**
 * Resumes a paused song on the hardware mp3 stream
 */
void IPhoneResumeSong();

/**
 * Resumes the previous song from the point in playback where it was paused before the current song started playing
 */
void IPhoneResumePreviousSong();

/**
 * Disables looping of music device, play song will reset it back to looping by default
 */
void IPhoneDisableSongLooping();

/**
 * Returns the OS version of the device
 */
float IPhoneGetOSVersion();

/**
 * Returns the push notification token for the device + application
 */
void IPhoneGetDevicePushNotificationToken( char *DevicePushNotificationToken, int MaxLen );

/**
 * @return How much to scale globally scale UI elements in the X direction 
 * (useful for hi res screens, iPhone4, etc)
 */
float IPhoneGetGlobalUIScaleX();

/**
 * @return How much to scale globally scale UI elements in the Y direction 
 * (useful for hi res screens, iPhone4, etc)
 */
float IPhoneGetGlobalUIScaleY();

/**
 * @return the type of device we are currently running on
 */
EIOSDevice IPhoneGetDeviceType();

/**
 * @return the device type as a string
 */
FString IPhoneGetDeviceTypeString( EIOSDevice DeviceType=IPhoneGetDeviceType() );

/**
 * @return the number of cores on this device
 */
int IPhoneGetNumCores();

/**
 * @return the orientation of the UI on the device
 */
int IPhoneGetUIOrientation();

#if WITH_GAMECENTER

/**
 * Starts GameCenter (can be disabled in a .ini for UDK users)
 * (note, this is defined in GameCenter.mm)
 */
void IPhoneStartGameCenter();

/**
 * Check to see if GameCenter is available (so we can run on pre-4.0 devices)
 */
bool IPhoneDynamicCheckForGameCenter();

#endif

/**
 * Check to see if OS supports achievement banners
 */
bool IPhoneCheckAchievementBannerSupported();

/**
 * Displays a console interface on device
 */
void IPhoneShowConsole();

/**
* Shows a keyboard and allows user to input text in device
*/
void IPhoneGetUserInput(const char* Title, const char* InitialString, const char* ExecResponseFunction, const char* CancelResponseFunction, const char* CharacterLimit);

/**
* Shows a keyboard and allows user to input multi line text in device
*/
void IPhoneGetUserInputMulti(const char* Title, const char* InitialString, const char* ExecRespone);

/**
 * Shows an alert, blocks until user selects a choice, and then returns the index of the 
 * choice the user has chosen
 *
 * @param Title		The title of the message box
 * @param Message	The text to display
 * @param Button0	Label for Button0
 * @param Button1	Label for Button1 (or NULL for no Button1)
 * @param Button2	Label for Button2 (or NULL for no Button2)
 *
 * @return 0, 1 or 2 depending on which button was clicked
 */
int IPhoneShowBlockingAlert(const char* Title, const char* Message, const char* Button0, const char* Button1=0, const char* Button2=0);

/**
 * Gets the language the user has selected
 *
 * @param Language String to receive the Language into
 * @param MaxLen Size of the Language string
 */
void IPhoneGetUserLanguage(char* Language, int MaxLen);

/**
 * Enables or disables the sleep functionality of the device
 */
void IPhoneSetSleepEnabled(bool bIsSleepEnabled);

/**
 * Retrieves the string value for the given key in the application's bundle (ie Info.plist)
 *
 * @param Key The key to look up
 * @param Value A buffer to fill in with the value
 * @param MaxLen Size of the Value string
 *
 * @return true if Key was found in the bundle, and it was had a string value to return
 */
bool IPhoneGetBundleStringValue(const char* Key, char* Value, int MaxLen);

/**
 * Will show an iAd on the top or bottom of screen, on top of the GL view (doesn't resize
 * the view)
 * 
 * @param bShowOnBottomOfScreen If true, the iAd will be shown at the bottom of the screen, top otherwise
 */
void IPhoneShowAdBanner(bool bShowOnBottomOfScreen);

/**
 * Hides the iAd banner shows with IPhoneShowAdBanner. Will force close the ad if it's open
 */
void IPhoneHideAdBanner();

/**
 * Forces closed any displayed ad. Can lead to loss of revenue
 */
void IPhoneCloseAd();

/**
 * @return true if the phone currently has a net connection
 */
bool IPhoneIsNetConnected();

/**
 * Enables or disables the sleep functionality of the device
 */
void IPhoneSetSleepEnabled(bool bIsSleepEnabled);

/**
 * Reads the mac address for the device
 *
 * @return the MAC address as a string (or empty if MAC address could not be read).
 */
FString IPhoneGetMacAddress();

/**
 * Returns whether the build is packaged as Distribution (ie, shipping build)
 */
bool IPhoneIsPackagedForDistribution();

/**
*Returns the size of the back buffer alloced through OpenGL
*/
#if USE_DETAILED_IPHONE_MEM_TRACKING
UINT GetIPhoneOpenGLBackBufferSize();
#endif

#endif
