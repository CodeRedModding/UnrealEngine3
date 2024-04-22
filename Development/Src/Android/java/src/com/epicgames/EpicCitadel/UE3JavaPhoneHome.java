/*=============================================================================
	UE3JavaPhoneHome.java Android class for phone home interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

package com.epicgames.EpicCitadel;

//setup imports
import android.app.Activity;
import android.content.SharedPreferences;
import java.util.Date;

import java.net.URL;
import java.net.URLConnection;
import java.net.MalformedURLException;
import java.io.IOException;

import java.lang.Object;

import java.net.HttpURLConnection;
import java.io.BufferedOutputStream;
import java.io.OutputStream;
import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;

import android.os.SystemClock;
import android.view.Display;
import android.graphics.Point;
import android.content.Context;
import android.view.WindowManager;
import java.util.Locale;
import android.os.Build;
import android.os.Build.VERSION;
import android.telephony.TelephonyManager;
import android.provider.Settings;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileFilter;
import java.util.regex.Pattern;
import android.content.pm.PackageManager.NameNotFoundException;
import java.lang.System;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.lang.StringBuilder;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;
import java.net.URLEncoder;
import java.io.UnsupportedEncodingException;

class AndroidPhoneHome
{
	public static SharedPreferences SharedPreferences; 
	public static SharedPreferences.Editor SharedPreferencesEditor;
	public static boolean bIsPhoneHomeInFlight = false;
	static Activity ApplicationActivity;
	static Context ApplicationContext;
	
	static AndroidPhoneHome Aph;

	private String AppPayload;
	private String GamePayload;
	private String GameName;
	private String ReplyBuffer;

	private long CollectStart;
	private boolean WiFiConn;
	private boolean bCompleted;

	private final int PhoneHomeXMLVer = 4;

	// This should be the first function called on AndroidPhoneHome, most other
	// functions rely on having the Activity and SharedPreferences
	public static void SetApplicationActivity(Activity CurrentActivity)
	{
		ApplicationActivity = CurrentActivity;
		ApplicationContext = ApplicationActivity.getApplicationContext();

		SharedPreferences = ApplicationActivity.getPreferences(0);
		SharedPreferencesEditor = SharedPreferences.edit();
	}

	public static boolean ShouldPhoneHome ()
	{
		long Now = System.currentTimeMillis() / 1000;

		if ( SharedPreferences == null )
		{
			Logger.LogOut("Need to call SetApplicationActivity first!");
			return false;
		}

		long LastSuccess = SharedPreferences.getLong("AndroidPhoneHome::LastSuccess", 0);
		long LastFailure = SharedPreferences.getLong("AndroidPhoneHome::LastFailure", 0);

		// make sure at least 1 day has passed since last_success
		if (LastSuccess != 0 && Now - LastSuccess < 60*60*24)
		{
			return false;
		}

		// make sure at least 5 minutes have passed since last_failure
		if (LastFailure != 0 && Now - LastFailure < 60*5)
		{
			return false;
		}
		return true;
	}

	// Entry point for starting a phone home request
	public static void QueueRequest ()
	{
		// if there's one in flight, never phone home
		// NOTE: this isn't intended to be MT-safe as queueRequest makes no such guarantee either
		// if (bIsPhoneHomeInFlight)
		// {
		// 	return;
		// }

		// // enforce throttling
		// if (!AndroidPhoneHome.ShouldPhoneHome())
		// {
		// 	return;
		// }

		// mark that a phone home is in flight because we may get called again while async callbacks are processing
		bIsPhoneHomeInFlight = true;

		Aph = new AndroidPhoneHome();
		Aph.Init();

		// send the payload
		// NOTE: this completes in several asynchronous callback steps
		Aph.CollectPayload();
	}

	// This should be called from the main (UI) thread
	public void DidSucceed ()
	{
		// if, somehow, this gets called after we got our data, ignore it
		if (bCompleted)
		{
			return;
		}
		bCompleted = true;

		// reset our "since last upload" stats
		SharedPreferencesEditor.putLong("AndroidPhoneHome::NumSurveyFailsSinceLast", 0);

		// record last upload time
		SharedPreferencesEditor.putLong("AndroidPhoneHome::LastSuccess", System.currentTimeMillis() / 1000 );

		// reset our "since last submission stats"
		SharedPreferencesEditor.putLong("AndroidPhoneHome::NumInvocations", 0);
		SharedPreferencesEditor.putLong("AndroidPhoneHome::NumCrashes", 0);
		SharedPreferencesEditor.putLong("AndroidPhoneHome::NumMemoryWarnings", 0);
		SharedPreferencesEditor.putLong("AndroidPhoneHome::AppPlaytimeSecs", 0);

		SharedPreferencesEditor.commit();

		bIsPhoneHomeInFlight = false;
	}

	// This should be called in the main (UI) thread
	public void DidFail ()
	{
		// if, somehow, this gets called after we got our data, ignore it
		if (bCompleted)
		{
			return;
		}
		bCompleted = true;

		IncrementPreferenceLong( "AndroidPhoneHome::TotalSurveyFails" );
		IncrementPreferenceLong( "AndroidPhoneHome::NumSurveyFailsSinceLast" );

		// record last failure time so we don't retry to soon
		SharedPreferencesEditor.putLong( "Android::LastFailure", System.currentTimeMillis() / 1000 );
		SharedPreferencesEditor.commit();

		// notify the UPhoneHome object
		// NOTE: needs to be called in the game thread.
		bIsPhoneHomeInFlight = false;
	}


	// Puts the payloads together into one string along with final packaging, creates and formats the http url request, tries sending
	// the payload and gets the server response
	private void DoSendPayload ()
	{
		// check for an encryption segment with encryption mode = decrypted, or no encryption segment at all
		int DevResult = ApplicationInformation.CheckHeader(); // 0 is valid, 1 is pirated, 2 is missing / unknown LC

		// combine the payloads
		String Payload = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
		Payload += "<phone-home ver=\"" + PhoneHomeXMLVer + "\" dev=\"" + DevResult + "\">\n";
		Payload += AppPayload;
		Payload += GamePayload;


		// end timing and record
		double dts = 1e-9 * ((double)(System.nanoTime() - CollectStart));
		Payload += "<meta collect-time-secs=\"" + dts + "\"/>\n";

		// terminate and convert to data
		Payload += "\n</phone-home>";
		byte[] Data = null;
		try
		{
			Data = Payload.getBytes("UTF8");
		}
		catch (Exception e)
		{
			Logger.ReportException("Getting bytes of data", e);
			DidFail();
			return;
		}

		Logger.LogOut("Phone Home Payload: \n" + Payload);

		String PhoneHomeURLBase = UE3JavaApp.NativeCallback_PhoneHomeGetURL();
		if ( PhoneHomeURLBase.length() == 0 )
		{
			// if the url hasn't been specified then exit
			return;
		}

		String PhoneHomeURLStr = "http://" + PhoneHomeURLBase + "/PhoneHome-1?uid=" + GetUIDHashValue("MD5") + 
			"&game=";

		if ( WiFiConn )
		{
			try
			{
				PhoneHomeURLStr += URLEncoder.encode("&wwan=1", "utf-8");
			}
			catch (UnsupportedEncodingException e)
			{
				Logger.ReportException("Unsupported encoding", e);
			}
		}

		URL PhoneHomeUrl = null;
		try
		{
			PhoneHomeUrl = new URL(PhoneHomeURLStr);
		}
		catch (MalformedURLException e)
		{
			Logger.ReportException("Getting URL", e);
			DidFail();
			return;
		}

		
		// assume it's a failure and record the time now so we don't retry to soon (but don't increment failure count)
		// it's possible we'll quit before the delegate tells us we've sent but it will still go through
		SharedPreferencesEditor.putLong( "AndroidPhoneHome::LastFailure", System.currentTimeMillis() / 1000 );
		SharedPreferencesEditor.commit();

		// create a POST HTTP request and send it
		HttpURLConnection Conn = null;
		try
		{
			Conn = (HttpURLConnection) PhoneHomeUrl.openConnection();
		}
		catch (IOException e)
		{
			Logger.ReportException("Opening url connection", e);
			DidFail();
			return;
		}

		try
		{
			// populate fields and try sending
			Conn.setDoOutput(true); // Set to POST
			Conn.setDoInput(true);
			Conn.setFixedLengthStreamingMode( Data.length );
			Conn.setRequestMethod("POST");
			Conn.setRequestProperty( "Content-Type", "application/xml" );
			Conn.setRequestProperty( "Content-Length", String.valueOf(Data.length) );
			
			OutputStream OutStream = new BufferedOutputStream( Conn.getOutputStream() );
			OutStream.write( Data );
			OutStream.flush();
			OutStream.close();

			int ResponseCode = Conn.getResponseCode();
			String ResponseMessage = Conn.getResponseMessage();
			Logger.LogOut("Response code: " + ResponseCode);
			Logger.LogOut("Response message: " + ResponseMessage);
			// Handle response code here
			
			// Get data from server
			InputStream InStream = new BufferedInputStream( Conn.getInputStream() );
			int ReadSize = 1024;
			byte[] Buffer = new byte[ReadSize];
			int BytesRead = ReadSize;

			while ( BytesRead == ReadSize )
			{
				BytesRead = InStream.read( Buffer, 0, ReadSize );
				if ( BytesRead > 0 )
				{
					ReplyBuffer += new String( Buffer, 0, BytesRead );
				}
			}
			InStream.close();

			Logger.LogOut("ReplyBuffer: " + ReplyBuffer);
		}
		catch (IOException e)
		{
			Logger.ReportException("Opening stream", e);
			DidFail();
			return;
		}
		finally
		{
			Conn.disconnect();
		}

		DidSucceed();
	}

	// Gather and package the info for th Game payload
	public void CollectStats ()
	{
		GameName = ApplicationContext.getPackageName();
		if (GameName == null)
		{
			GameName = "unknown";
		}

		Context ApplicationContext = ApplicationActivity.getApplicationContext();
		String AppVersionString = ApplicationInformation.GetAppVersion(); 

		int EngineVersion = UE3JavaApp.NativeCallback_GetEngineVersion();

		// No Piracy checks on Android Currently
		String DLCHash = ApplicationInformation.ScanForDLC();
		int NumJailBrokenFilesFound = 0;
		int NumJailBrokenProcessesFound = 0;

		// report the game name and engine version
		GamePayload += "<game name=\"" + GameName  + "\" engver=\"" + EngineVersion + "\" appver=\"" + AppVersionString + "\" dlc=\"" + 
			DLCHash + "\" cf=\"" + NumJailBrokenFilesFound + "\" cp=\"" + NumJailBrokenProcessesFound + "\">\n";
		
		// implement game-specifc stats here (UPhoneHome)

		GamePayload += "</game>\n";

		DoSendPayload();
	}

	// Gather and package the info for the App payload 
	public void ReachabilityDone ()
	{
		// add stats about upload connectivity
		long TotalSurveys 				= SharedPreferences.getLong( "AndroidPhoneHome::TotalSurveys", 0 ); 
		long TotalSurveyFails 			= SharedPreferences.getLong( "AndroidPhoneHome::TotalSurveyFails", 0 );	
		long TotalSurveyFailsSinceLast 	= SharedPreferences.getLong( "AndroidPhoneHome::NumSurveyFailsSinceLast", 0 );
		AppPayload += "<upload total-attempts=\"" + TotalSurveys + "\" total-failures=\"" + TotalSurveyFails + "\" recent-failures=\"" + TotalSurveyFailsSinceLast + "\"/>\n";

		// get other device info (os, device type, etc)
		String Hardware 	= ApplicationInformation.GetDeviceType();
		String Model 		= ApplicationInformation.GetModel();
		int Version 		= Build.VERSION.SDK_INT;
		String MD5Value 	= GetUIDHashValue("MD5");
		String SHA1Value 	= GetUIDHashValue("SHA-1");
		AppPayload += "<device type=\"" + Hardware + "\" model=\"" + Model + "\" os-ver=\"" + Version + "\" md5=\"" + MD5Value + "\" sha1=\"" + SHA1Value + "\"/>\n";

		// get locale (language settings) info
		String LocaleCountryCode 	= Locale.getDefault().getCountry();
		String LocalelanguageCode 	= Locale.getDefault().getLanguage();
		String LocaleVariantCode 	= Locale.getDefault().getVariant();
		AppPayload += "<locale country=\"" + LocaleCountryCode + "\" language=\"" + LocalelanguageCode + "\" variant=\"" + LocaleVariantCode + "\"/>\n";

		// Screen dimensions
		WindowManager WinManager = (WindowManager) ApplicationActivity.getApplicationContext().getSystemService(Context.WINDOW_SERVICE);
		Display DisplayMain = WinManager.getDefaultDisplay();
		int Width = DisplayMain.getWidth();
		int Height = DisplayMain.getHeight();
		AppPayload += "<screen width=\"" + Width + "\" height=\"" + Height + "\"/>\n";

		// System info
		File MemInfoFile = new File( "proc/meminfo" );

		String MemInfoString = UE3JavaApp.ReadMemFileToString( MemInfoFile );

		String delims = "[ ]+";
		String[] tokens = MemInfoString.split(delims);
		int PhysicalMemory = 512;
		if( tokens.length == 3 )
		{
			PhysicalMemory = Integer.parseInt( tokens[1] ) / 1024;
		}
		AppPayload += "<processInfo processors=\"" + GetNumCores() + "\" uptime=\"" + (SystemClock.elapsedRealtime() / 1000) + "\" physmem=\"" + PhysicalMemory + "\"/>\n";

		// report the memory we had to work with when we started
		AppPayload += "<startmem free-bytes=\"" + UE3JavaApp.GetStartupFreeMem() + "\" used-bytes=\"" + UE3JavaApp.GetStartupUsedMem() + "\"/>\n";

		// report application stats
		long NumInvocations		= SharedPreferences.getLong( "AndroidPhoneHome::NumInvocations", 0 );
		long NumCrashes			= ApplicationInformation.GetNumCrashes();
		long NumMemoryWarnings	= ApplicationInformation.GetNumMemoryWarnings();
		long AppPlaytimeSecs	= SharedPreferences.getLong( "AndroidPhoneHome::AppPlaytimeSecs", 0 );
		AppPayload += "<app invokes=\"" + NumInvocations + "\" crashes=\"" + NumCrashes + "\" memwarns=\"" + NumMemoryWarnings + "\" playtime-secs=\"" + AppPlaytimeSecs + "\"/>\n"; 

		CollectStats();
	}

	// Kicks off the async calls to collect and send the payload
	// This is called from QueueRequest
	private void CollectPayload ()
	{
		// begin timing
		CollectStart = System.nanoTime(); 

		IncrementPreferenceLong("AndroidPhoneHome::TotalSurveys");

		// kick off check for WiFi connection
		WiFiConn = false;

		// Start the task to collect and send data
		Logger.LogOut("Start phone home collection");
		
		// Fire off a thread to do some work that we shouldn't do directly in the UI thread
        Thread PayloadThread = new Thread() {
            public void run() {
                ConnectivityManager ConnManager = (ConnectivityManager) ApplicationContext.getSystemService(ApplicationContext.CONNECTIVITY_SERVICE);
				WiFiConn = ConnManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI).isConnected();			
				Aph.ReachabilityDone();
            }
        };
		PayloadThread.setPriority(Thread.MIN_PRIORITY);
        PayloadThread.start();
	}

	public void Init ()
	{
		bCompleted 			= false;
		AppPayload 			= "";
		GamePayload 		= "";
		ReplyBuffer			= "";
		GameName			= "";
	}

	public void IncrementPreferenceLong(String PreferenceName)
	{
		long currentValue = SharedPreferences.getLong(PreferenceName, 0);
		SharedPreferencesEditor.putLong(PreferenceName, currentValue + 1);
		SharedPreferencesEditor.commit();
	}

	private int GetNumCores() 
	{
	    //Private Class to display only CPU devices in the directory listing
	    class CpuFilter implements FileFilter 
	    {
	        @Override
	        public boolean accept(File pathname) 
	        {
	            //Check if filename is "cpu", followed by a single digit number
	            if(Pattern.matches("cpu[0-9]", pathname.getName())) 
	            {
	                return true;
	            }
	            return false;
	        } 
	    }

	    try 
	    {
	        //Get directory containing CPU info
	        File dir = new File("/sys/devices/system/cpu/");
	        //Filter to only list the devices we care about
	        File[] files = dir.listFiles(new CpuFilter());
	        //Return the number of cores (virtual CPU devices)
	        return files.length;
	    } 
	    catch(Exception e) 
	    {
	        //Default to return 1 core
	        return 1;
	    }
	}

	private String GetUIDHashValue(String Algorithm)
	{
		// Build Unique Device ID
		String Uid = Settings.Secure.getString(ApplicationContext.getContentResolver(),Settings.Secure.ANDROID_ID); // ANDROID_ID should be unique on most devices
		Uid += ((TelephonyManager)ApplicationContext.getSystemService(ApplicationContext.TELEPHONY_SERVICE)).getDeviceId(); // Will ge unique on phones but not tablets
		Uid += Build.SERIAL; // Should be unique on most non-phones

		String FinalHashValue = "";	
		MessageDigest Digest;
		try
		{
			Digest = MessageDigest.getInstance(Algorithm);
			Digest.reset();
			Digest.update( Uid.getBytes() );
			byte[] HashValue = Digest.digest();

			// Convert hash value to proper format
			StringBuilder sb = new StringBuilder();
			for ( int i = 0; i < HashValue.length; ++i )
				sb.append( Integer.toHexString(0x100 + (HashValue[i] & 0xff)).substring(1) );

			FinalHashValue = sb.toString();
		}
		catch (NoSuchAlgorithmException e)
		{
			Logger.ReportException("Couldn't find algorithm", e);
		}

		return FinalHashValue;
	}
}

