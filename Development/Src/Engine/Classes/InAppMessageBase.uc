/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for In App Message integration (each platform has a subclass)
 */
class InAppMessageBase extends PlatformInterfaceBase
	native(PlatformInterface)
	config(Engine);

enum EInAppMessageInterfaceDelegate
{
	IAMD_InAppSMSUIComplete,
	IAMD_InAppEmailComplete,
};

/**
 * Perform any needed initialization
 */
native event Init();

/**
 * Kicks off an in app SMS, using the platform to show the UI. If this returns FALSE system was unable to do this
 * 
 * @param InitialMessage [optional] Initial message to show
 * 
 * @return TRUE if a UI was displayed for the user to interact with, and a IAMD_InAppSMSUIComplete will be sent when done
 */
native event bool ShowInAppSMSUI(optional string InitialMessage);

/**
 * Kicks off a in app email, using the platform to show the UI. If this returns FALSE system was unable to do this
 * 
 * @param InitialMessage [optional] Initial message to show
 * 
 * @return TRUE if a UI was displayed for the user to interact with, and a IAMD_InAppEmailComplete will be sent when done
 */
native event bool ShowInAppEmailUI(optional string InitialSubject, optional string InitialMessage);
