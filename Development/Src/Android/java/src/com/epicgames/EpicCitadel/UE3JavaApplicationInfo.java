/*=============================================================================
	UE3JavaApplicationInfo.java Android class for phone home interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

package com.epicgames.EpicCitadel;

//setup imports
import android.app.Activity;
import android.content.SharedPreferences;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION;
import android.content.pm.PackageManager.NameNotFoundException;


class ApplicationInformation
{
	public static SharedPreferences SharedPreferences; 
	static Activity ApplicationActivity;
	static Context ApplicationContext;

	public static void Init(Activity InActivity)
	{
		ApplicationActivity = InActivity;
		ApplicationContext = InActivity.getApplicationContext();

		SharedPreferences = ApplicationActivity.getPreferences(0);
	}

	public static int CheckHeader()
	{
		// 0 is valid, 1 is pirated, 2 is missing / unknown LC
		// Not currently checking for pirated info on Android so return valid
		return 0;
	}

	// Static helper functions used for this and other classes
	public static String GetModel()
	{
		return Build.MODEL;
	}

	public static String GetDeviceType()
	{
		return Build.HARDWARE;
	}

	public static String GetOSVersion()
	{
		return Integer.toString(Build.VERSION.SDK_INT);
	}

	public static long GetNumCrashes()
	{
		return SharedPreferences.getLong( "AndroidPhoneHome::NumCrashes", 0 );
	}

	public static long GetNumMemoryWarnings()
	{
		return SharedPreferences.getLong( "AndroidPhoneHome::NumMemoryWarnings", 0 );
	}

	public static String GetAppVersion()
	{
		String AppVersionString = "";
		try
		{
			AppVersionString = ApplicationContext.getPackageManager().getPackageInfo(ApplicationContext.getPackageName(), 0).versionName.toString();
		}
		catch (NameNotFoundException e)
		{
			Logger.ReportException("Getting app version", e);
		}
		return AppVersionString;
	}

	public static String ScanForDLC()
	{
		// No Piracy checks on Android Currently
		return "";
	}	
}