/*=============================================================================
	UnrealJavaApsalar.java: The java wrapper for access to the Apsalar Android library
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

package com.epicgames.EpicCitadel;

//setup imports
import android.app.Activity;
import android.content.Context;
import java.lang.reflect.Method;

class Apsalar
{
	static Activity ApplicationContext;

	// Function Pointers
	static Class ApsalarClass;
	// Logging
	static Method Method_StartSession;
	static Method Method_EndSession;
	static Method Method_Event;
	static Method Method_EventParamArray;

	// Apsalar state flags
	static boolean ApsalarInitialized = false;
	public static boolean IsApsalarInitialized() { return ApsalarInitialized; }
	static boolean ExceptionMessageThrown = false;

	// Called during Android initialization so that Apsalar can know the application context
	public static void SetApplicationContext(Activity ApplicationActivity)
	{
		ApplicationContext = ApplicationActivity;
	}

	public static void Init ()
	{
		try 
		{
			ApsalarClass = Class.forName("com.apsalar.sdk.Apsalar");

			Method_StartSession = ApsalarClass.getDeclaredMethod("startSession", Activity.class, String.class, String.class);
			Method_EndSession = ApsalarClass.getDeclaredMethod("endSession");
			Method_Event = ApsalarClass.getDeclaredMethod("event", String.class);
			Method_EventParamArray = ApsalarClass.getDeclaredMethod("event", String.class, Object[].class);

			ApsalarInitialized = true;
		}
		catch (Exception e) { if (!ExceptionMessageThrown) { Logger.LogOut("EXCEPTION thrown in UE3JavaApsalar"); ExceptionMessageThrown = true; } }
	}

	public static void StartSession(String ApiKey, String ApiSecret)
	{
		try
		{
			Method_StartSession.invoke(null, ApplicationContext, ApiKey, ApiSecret);
		}
		catch (Exception e) { if (!ExceptionMessageThrown) { Logger.LogOut("EXCEPTION thrown in UE3JavaApsalar"); ExceptionMessageThrown = true; } }
	}

	public static void EndSession()
	{
		try
		{
			Method_EndSession.invoke(null);
		}
		catch (Exception e) { if (!ExceptionMessageThrown) { Logger.LogOut("EXCEPTION thrown in UE3JavaApsalar"); ExceptionMessageThrown = true; } } 
	}

	public static void Event(String EventName)
	{
		try
		{
			Method_Event.invoke(null, EventName);
		}
		catch (Exception e) { if (!ExceptionMessageThrown) { Logger.LogOut("EXCEPTION thrown in UE3JavaApsalar"); ExceptionMessageThrown = true; } }
	}

	public static void EventParam(String EventName, String Name, String Value)
	{
		try
		{
			Method_EventParamArray.invoke(null, EventName, new String[] {Name, Value});
		}
		catch (Exception e) { if (!ExceptionMessageThrown) { Logger.LogOut("EXCEPTION thrown in UE3JavaApsalar"); ExceptionMessageThrown = true; } }
	}

	public static void EventParamArray(String EventName, String[] Params)
	{
		try
		{
			Method_EventParamArray.invoke(null, EventName, Params);
		}
		catch (Exception e) { if (!ExceptionMessageThrown) { Logger.LogOut("EXCEPTION thrown in UE3JavaApsalar"); ExceptionMessageThrown = true; } }
	}

	public static void LogEventEngineData(String EventName, int EngineVersion)
	{
		String[] Params = new String[18];

		Params[0] = "DevBit";
		Params[1] = Integer.toString(ApplicationInformation.CheckHeader());

		Params[2] = "DeviceModel";
		Params[3] = ApplicationInformation.GetModel();

		Params[4] = "DeviceType";
		Params[5] = ApplicationInformation.GetDeviceType();

		Params[6] = "NumCrashes";
		Params[7] = Long.toString(ApplicationInformation.GetNumCrashes());

		Params[8] = "NumMemoryWarnings";
		Params[9] = Long.toString(ApplicationInformation.GetNumMemoryWarnings());

		Params[10] = "PackageHash";
		Params[11] = ApplicationInformation.ScanForDLC();

		Params[12] = "EngineVersion";
		Params[13] = Integer.toString(EngineVersion);

		Params[14] = "AppVersion";
		Params[15] = ApplicationInformation.GetAppVersion();

		Params[16] = "OSVersion";
		Params[17] = ApplicationInformation.GetOSVersion();

		Apsalar.EventParamArray(EventName, Params);
	}
}