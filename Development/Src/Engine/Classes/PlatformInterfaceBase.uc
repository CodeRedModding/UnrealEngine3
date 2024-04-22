/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for the various platform interface classes and has support
 * for a generic delegate system, as well has having subclasses determine if they
 * should register for a tick.
 * 
 */
 
class PlatformInterfaceBase extends Object
	native(PlatformInterface)
	transient;

cpptext
{
	/**
	 * C++ interface to get the singleton
	 */
	static UCloudStorageBase* GetCloudStorageInterfaceSingleton();
	static UCloudStorageBase* GetLocalStorageInterfaceSingleton();
	static UFacebookIntegration* GetFacebookIntegrationSingleton();
	static UInGameAdManager* GetInGameAdManagerSingleton();
	static UMicroTransactionBase* GetMicroTransactionInterfaceSingleton();
	static UAnalyticEventsBase* GetAnalyticEventsInterfaceSingleton();
	static UTwitterIntegrationBase* GetTwitterIntegrationSingleton();
	static UAppNotificationsBase* GetAppNotificationsInterfaceSingleton();
	static UInAppMessageBase* GetInAppMessageInterfaceSingleton();

	/**
	 * Check for certain exec commands that map to the various subclasses (it will only
	 * get/create the singleton if the first bit of the exec command matches a one of 
	 * the special strings, like "ad" for ad manager)
	 */
	static UBOOL StaticExec(const TCHAR* Cmd, FOutputDevice& Ar);

	/**
	 * Determines if there are any delegates of the given type on this platform interface object.
	 * This is useful to skip a bunch of FPlatformInterfaceDelegateResult if there is no
	 * one even listening!
	 *
	 * @param DelegateType The type of delegate to look up delegates for
	 *
	 * @return TRUE if there are any delegates set of the given type
	 */
	UBOOL HasDelegates(INT DelegateType);
};

/** An enum for the types of data used in a PlatformInterfaceData struct, below */
enum EPlatformInterfaceDataType
{
	PIDT_None,		// no data type specified
	PIDT_Int,
	PIDT_Float,
	PIDT_String,
	PIDT_Object,
	PIDT_Custom,	// a custom type where more than one value may be filled out
};

/** 
 * Struct that encompasses the most common types of data. This is the data payload
 * of PlatformInterfaceDelegateResult
 */
struct native PlatformInterfaceData
{
	/** An optional tag for this data */
	var name DataName;

	/** Specifies which value is valid for this structure */
	var	EPlatformInterfaceDataType Type;

	/** Various typed result values */
	var	int IntValue;
	var	float FloatValue;
	var	init string StringValue;
	var	init string StringValue2;
	var	Object ObjectValue;
};

/** Generic structure for returning most any kind of data from C++ to delegate functions */
struct native PlatformInterfaceDelegateResult
{
	/** This is always usable, no matter the type */
	var	bool bSuccessful;

	/** The result actual data */
	var PlatformInterfaceData Data;
};

/**
 * Helper struct, since UnrealScript doesn't allow arrays of arrays, but
 * arrays of structs of arrays are okay.
 */
struct native DelegateArray
{
	var array< delegate<PlatformInterfaceDelegate> > Delegates;
};

/** Array of delegate arrays. Only add and remove via helper functions, and call via the helper delegate call function */
var array<DelegateArray> AllDelegates;

/** Generic platform interface delegate signature */
delegate PlatformInterfaceDelegate(const out PlatformInterfaceDelegateResult Result);

/**
 * Call all the delegates currently set for the given delegate type with the given data
 *
 * @param DelegateType Which set of delegates to call (this is defined in the subclass of PlatformInterfaceBase)
 * @param Result Data to pass to each delegate
 */
native function CallDelegates(int DelegateType, out PlatformInterfaceDelegateResult DelegateResult);

/** @return the CloudStorage singleton object */
native static function CloudStorageBase GetCloudStorageInterface();

/** @return the LocalStorage singleton object */
native static function CloudStorageBase GetLocalStorageInterface();

/** @return the Facebook singleton object */
native static function FacebookIntegration GetFacebookIntegration();

/** @return the AdManager singleton object */
native static function InGameAdManager GetInGameAdManager();

/** @return the MicroTransaction singleton object */
native static function MicroTransactionBase GetMicroTransactionInterface();

/** @return the AnalyticsEvents singleton object */
native static function AnalyticEventsBase GetAnalyticEventsInterface();

/** @return the TwitterIntegration singleton object */
native static function TwitterIntegrationBase GetTwitterIntegration();

/** @return the AppNotificationsBase singleton object */
native static function AppNotificationsBase GetAppNotificationsInterface();

/** @return the InAppMessageBase singleton object */
native static function InAppMessageBase GetInAppMessageInterface();

/**
 * Adds a typed delegate (the value of the type is subclass dependent, make an enum per subclass)
 *
 * @param InDelegate the delegate to use for notifications
 */
function AddDelegate(int DelegateType, delegate<PlatformInterfaceDelegate> InDelegate)
{
	if (AllDelegates.length < DelegateType + 1)
	{
		AllDelegates.length = DelegateType + 1;
	}
	// Add this delegate to the array if not already present
	if (AllDelegates[DelegateType].Delegates.Find(InDelegate) == INDEX_NONE)
	{
		AllDelegates[DelegateType].Delegates.AddItem(InDelegate);
	}
}

/**
 * Removes a delegate from the list of listeners
 *
 * @param InDelegate the delegate to use for notifications
 */
function ClearDelegate(int DelegateType, delegate<PlatformInterfaceDelegate> InDelegate)
{
	local int RemoveIndex;

	if (DelegateType < AllDelegates.length)
	{
		// Remove this delegate from the array if found
		RemoveIndex = AllDelegates[DelegateType].Delegates.Find(InDelegate);
		if (RemoveIndex != INDEX_NONE)
		{
			AllDelegates[DelegateType].Delegates.Remove(RemoveIndex,1);
		}
	}
}



