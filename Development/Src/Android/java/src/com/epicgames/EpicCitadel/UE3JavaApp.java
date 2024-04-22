/*=============================================================================
	UnrealEngine3.java: The java main application implementation for Android.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

package com.epicgames.EpicCitadel;

//setup imports
import android.app.Activity;
import android.os.Handler;
import android.os.Bundle;
import android.os.Environment;
import android.view.MotionEvent;
import android.view.KeyEvent;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Build;
import android.content.pm.*;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.widget.VideoView;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Gravity;
import java.util.regex.Pattern;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.SurfaceHolder.Callback;
import android.net.wifi.*;
import java.net.NetworkInterface;
import java.net.InetAddress;
import java.net.SocketException;
import java.util.Enumeration;
import javax.microedition.khronos.opengles.GL10;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGL11;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL11;
import android.text.SpannableString;
import android.text.method.*;
import android.util.Log;
import android.app.ActivityManager;
import android.widget.ImageView;
import android.media.SoundPool;
import android.media.AudioManager;
import android.widget.TextView;
import android.view.inputmethod.InputMethodManager;
import android.content.Context;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.Button;
import android.view.View.OnClickListener;
import android.view.View;
import android.view.LayoutInflater;
import android.graphics.BitmapFactory;
import android.media.MediaPlayer.OnPreparedListener;
import android.media.MediaPlayer.OnVideoSizeChangedListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.PowerManager;
import android.content.res.AssetManager;
import android.telephony.TelephonyManager;
import java.io.File;
import java.util.zip.CheckedInputStream;
import java.util.zip.CRC32;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.InputStreamReader;
import java.io.FileDescriptor;
import android.os.StatFs;
import android.util.DisplayMetrics;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.text.InputType;
import android.text.util.Linkify;
import java.lang.Runtime;
import android.content.res.AssetFileDescriptor;
import android.os.Process;
import android.widget.ProgressBar;
import java.text.DateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import com.google.android.vending.expansion.downloader.DownloaderClientMarshaller;
import com.google.android.vending.expansion.downloader.DownloaderServiceMarshaller;
import android.app.PendingIntent;
import android.content.pm.PackageManager.NameNotFoundException;
import com.google.android.vending.expansion.downloader.Helpers;
import com.google.android.vending.expansion.downloader.IDownloaderClient;
import com.google.android.vending.expansion.downloader.IDownloaderService;
import com.google.android.vending.expansion.downloader.DownloadProgressInfo;
import android.os.Messenger;
import com.google.android.vending.expansion.downloader.IStub;
import android.provider.Settings;
import com.google.android.vending.expansion.downloader.impl.DownloadInfo;
import com.google.android.vending.expansion.downloader.impl.DownloadsDB;
import java.util.Map;
import android.os.Debug.MemoryInfo;
import java.io.RandomAccessFile;
import java.io.BufferedInputStream;
import java.io.FileNotFoundException;
import java.util.UUID;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;
import android.media.MediaMetadataRetriever;
import android.graphics.Bitmap;

////////////////////////////////////
/// Class: 
///    class Logger
///    
/// Description: 
///    General Logging
///    
////////////////////////////////////
class Logger
{	
	private static final String TextTag				= "UE3";
	private static boolean bAllowLogging			= true;
	private static boolean bAllowExceptionLogging	= true;
	public static void SuppressLogs ()
	{
		bAllowLogging = bAllowExceptionLogging = false;
	}
	public static void LogOut(String InLog)
	{
		if( bAllowLogging )
		{
			Log.d( TextTag, InLog );	
		}
	}
	public static void ReportException(String InException, Exception InActualError )
	{
		if( bAllowExceptionLogging )
		{
			Log.d( TextTag, "EXCEPTION: " + InException + " : " + InActualError.getMessage() );	
		}
	}
}


////////////////////////////////////
/// Class: 
///    class UE3JavaApp extends  Activity implements OnCompletionListener, MovieUpdateMessage, IDownloaderClient
///    
/// Description: 
///    
///    
////////////////////////////////////
public class UE3JavaApp	extends		Activity
						implements	OnCompletionListener,
									IDownloaderClient	
{
	//////////////
	// BASE
	//////////////
	// java thread handler
    protected Handler handler			= null;
	// path of game data
	private static String ContentPath	= "UnrealEngine3";
	private boolean bRanInit			= false;
    private boolean paused				= false;
    private Runnable initRunnable		= null;  
	public boolean bAppActive			= true;	

	//////////////
	// OPENGL
	//////////////
	// defines
    private static final int EGL_RENDERABLE_TYPE		= 0x3040;
    private static final int EGL_OPENGL_ES2_BIT			= 0x0004;
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

	// EGL_NV_depth_nonlinear
	private static final int EGL_DEPTH_ENCODING_NV = 0x30E2;
	private static final int EGL_DEPTH_ENCODING_NONE_NV = 0;
	private static final int EGL_DEPTH_ENCODING_NONLINEAR_NV = 0x30E3;

	// state
    private EGL10 egl				= null;
    private GL11 gl					= null;
    private EGLSurface eglSurface	= null;
    private EGLDisplay eglDisplay	= null;
    private EGLContext eglContext	= null;
    private EGLConfig eglConfig		= null;
    private EGLConfigParms eglAttemptedParams = null;

	private final int EGLMinRedBits = 5;
	private final int EGLMinGreenBits = 6;
	private final int EGLMinBlueBits = 5;
	private final int EGLMinAlphaBits = 0;
	private final int EGLMinStencilBits = 0;
	private final int EGLMinDepthBits = 16;	 

	//////////////
	// MEDIA
	//////////////
	// state
	private SoundPool GSoundPool			= null;    
    private Thread MoviePrepareThread		= null;
	private MediaPlayer mediaPlayer			= null;
    private MediaPlayer songPlayer			= null;
	private MediaPlayer pendingSongPlayer	= null;
	private String currentSongName			= null;
	private String pendingSongName			= null;
	private float currentSongVolume			= 1.0f;
    private MediaPlayer voiceOverlayPlayer	= null;
	private SurfaceView	videoView			= null;
	private RelativeLayout videoLayout		= null;
	private boolean bIsMoviePlaying			= false;
	private String movieOverlayMessage		= "";
	private MediaPlayer loadingVideoPlayer	= null;
	private SurfaceView	loadingVideoView	= null;
	private RelativeLayout loadingVideoLayout = null;
	private Thread LoadingMoviePrepareThread = null;
	private AssetFileDescriptor loadingMovieFD = null;
	private boolean bIsLoadingMoviePlaying	= false;

	///////////////////
	// FILE DOWNLOADER
	///////////////////
	private ProgressBar progressBar;

	private IDownloaderService remoteService;
	private IStub downloaderClientStub;

    private TextView statusText;
    private TextView progressFraction;
    private TextView progressPercent;
    private TextView averageSpeed;
    private TextView timeRemaining;
    private TextView exitMessage;

    private View dashboard;
    private View cellMessage;
    private View exitLayout;

    private Button pauseButton;
    private Button wifiSettingsButton;
    private Button exitButton;

    private boolean isDownloadPaused = false;

    private String pathToMainExpansionFile;
    private String pathToPatchExpansionFile;

    private static int numDownloadsRunning = 0;

	private boolean skipDownloader = false;
	private boolean bIsExpansionInAPK = false;

    /**
     *	Helper class used to store some expansion file information, makes checking values
     *	in other parts easier.
     */
    private class XAPKFile 
    {
        public boolean IsMain;
        public int FileVersion;
        public long FileSize;

        XAPKFile()
        {
        	IsMain = true;
        	FileVersion = 1;
        	FileSize = 0;
        }

        XAPKFile(boolean isMain, int fileVersion, long fileSize) 
		{
            IsMain = isMain;
            FileVersion = fileVersion;
            FileSize = fileSize;
        }
    }

    private XAPKFile[] xAPKS;

	//////////////
	// MISC
	//////////////
	// graphics
    private boolean					nativeEGL						= false;    
	private SurfaceView				PrimaryGPUView					= null;
	private float					GScreenScalePercent				= 1.0f;
	private boolean					bFullOpenGLReset				= true;
	public boolean					bWindowHasFocus					= true;
	private	boolean					bFirstSurfaceCreated			= false;
	private boolean					bSurfaceCreatedThisPass			= false;
	private int						surfaceWidth					= 0;
    private int						surfaceHeight					= 0;
	private int						SwapBufferFailureCount			= 0;
	private int						ScreenSizeX						= -1;
	private int						ScreenSizeY						= -1;
	private int						DepthSize						= 24;
	// cached map
	private HashMap<String, String> appLocalValues;    
    // softkeyboard
    private LinearLayout			KeyboardTextLayout				= null;
    private EditText				KeyboardText					= null;
    private boolean					bKeyboardOpen					= false;
	// splash and loading
	private RelativeLayout			SplashLayout					= null;
    private ImageView				SplashScreen					= null;
	private ProgressBar				IndefiniteLoadingBar			= null;
    private ImageView				InstallSplashScreen				= null;
	private SurfaceView				StartupView						= null;
	private TextView				Progress						= null;	
	private RelativeLayout			IndefiniteReloadingBar			= null;
	private boolean					bSplashScreenIsHidden			= false;
	// network
	private Runnable				UpdateNetworkTask				= null;
	public boolean					bIsOnWifi						= false;
	public boolean					bIsFullyConnected				= false;
	public boolean					bIsOnline						= false;
	// wakelock
	private PowerManager.WakeLock	CurrentLock						= null;

	//storage
	private String					ExternalStoragePath				= "";
	private String					ContentExternalStoragePath		= "";
	private String					RootExternalPath				= "";

	// static for accessing in the purchaseObserver
	private static SharedPreferences mPrefs							= null;

	private static final String 	lastWorkingPerformanceKey		= "lw_performance";
	public static final String 		currentPerformanceKey			= "cur_performance";
	private static final String 	lastWorkingResScaleKey			= "lw_resolution_scale";
	public static final String 		currentResScaleKey				= "cur_resolution_scale";

	private static long StartupUsedMem; // used memory in bytes
	private static long StartupFreeMem; // free memory in bytes
	public static long GetStartupUsedMem() { return StartupUsedMem; }
	public static long GetStartupFreeMem() { return StartupFreeMem; }

	// expansion file paths
	private String 					MainExpansionFilePath 			= "";
	private String 					PatchExpansionFilePath 			= "";
	private AssetManager			AssetManagerReference;

	private String					appCommandLine					= "";

	////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////
	// ACTIVITY OVERRIDES
	////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
		super.onCreate(savedInstanceState);
		
		// Suppress java logs in Shipping builds
		if (NativeCallback_IsShippingBuild())
		{
			Logger.SuppressLogs();
		}

		Logger.LogOut("onCreate");

		// Immediately get memory stats
		ActivityManager.MemoryInfo MemInfoActivity = new ActivityManager.MemoryInfo();
		ActivityManager activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
		activityManager.getMemoryInfo( MemInfoActivity );
		StartupFreeMem = MemInfoActivity.availMem;

		MemoryInfo MemInfoDebug = new MemoryInfo();
		android.os.Debug.getMemoryInfo( MemInfoDebug );
		StartupUsedMem = MemInfoDebug.getTotalPss() * 1024;

		// Look in assets for an expansion file if fat Amazon APK
		try 
		{
			// @hack .png extension is used on Amazon data file to prevent it from being compressed in the APK
			String MainExpansionFileAssetPath = "main." + getPackageManager().getPackageInfo(getPackageName(), 0).versionCode + "." + getApplicationContext().getPackageName() + ".obb.png";
			try
			{
				final InputStream TestStream = this.getAssets().open(MainExpansionFileAssetPath);

				if (TestStream != null)
				{
					bIsExpansionInAPK = true;
					skipDownloader = true;
					AssetManagerReference = this.getAssets();
					Logger.LogOut("Found Main Expansion directly in APK, skipping downloader");
					TestStream.close();
				}
			}
			catch (IOException e){Logger.LogOut("Expansions not found to be in APK.");}
		}
		catch (NameNotFoundException e){Logger.LogOut("Could not build Expansion Asset Path");}

		// Get the commandline
		InputStream commandLineFile = null;

		// Load from inside of APK
		try 
		{
			commandLineFile = this.getAssets().open("UE3CommandLine.txt");
		}
		catch (IOException e){commandLineFile = null;}

		// If not shipping load from loose files
		if (!NativeCallback_IsShippingBuild())
		{
			String gameName = NativeCallback_GetGameName();

			try
			{
				commandLineFile = new BufferedInputStream(new FileInputStream("/sdcard/UnrealEngine3/" + gameName + "Game/CookedAndroid/UE3CommandLine.txt"));
			}
			catch (FileNotFoundException e) {commandLineFile = null;}
		}
		
		if (commandLineFile != null)
		{
			try
			{
				StringBuilder commandLineBuilder = new StringBuilder(512);
				int character;
				while ((character = commandLineFile.read()) != -1)
				{
					commandLineBuilder.append((char) character);
				}

				appCommandLine = commandLineBuilder.toString();

				commandLineFile.close();	
			}
			catch (IOException e) {}
		}

		skipDownloader = skipDownloader || appCommandLine.contains(" -skipdownloader");

		boolean bLaunchApplication = true;
		if ( NativeCallback_IsShippingBuild() && !skipDownloader )
		{
			bLaunchApplication = !CanDownloadExpansionFiles();
		}

		if ( bLaunchApplication )
		{
			// NOTE: Do not put any other code here
			// instead place it in AppInitialStartup, which is also called on completion of the expansion downloader
			AppInitialStartup();
		}
    }

	// fixes crash with some sliding keyboard
	@Override
    public void onConfigurationChanged(Configuration newConfig) 
	{
		Logger.LogOut("onConfigurationChanged(navigationHidden): " + ( newConfig.navigationHidden == Configuration.NAVIGATIONHIDDEN_YES ? "Hidden" : "Visible" ) );				
		NativeCallback_KeyPadChange( newConfig.navigationHidden != Configuration.NAVIGATIONHIDDEN_YES );
		super.onConfigurationChanged(newConfig); 
    }

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) 
	{
		Logger.LogOut("onActivityResult");
		super.onActivityResult( requestCode, resultCode, data );		
	}	

	@Override
    protected void onUserLeaveHint()
    {		
		Logger.LogOut("onUserLeaveHint");	
		super.onUserLeaveHint();
        SetInterruption( true );
    }

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		Logger.LogOut("onWindowFocusChanged: " + hasFocus);		
		// remember our focus state
		bWindowHasFocus = hasFocus;
		// only care if its gaining focus, true interruptions are from onpause
		if( hasFocus )
		{
			SetInterruption( false );
		}
	}

	@Override 
	protected void onStart()
	{
		super.onStart();
		Logger.LogOut("onStart");
		if (null != downloaderClientStub) 
        {
        	downloaderClientStub.connect(this);
    	}
	}
	
	@Override
	protected void onStop()
	{
		Logger.LogOut("onStop");
		super.onStop();
		if (null != downloaderClientStub) 
		{
        	downloaderClientStub.disconnect(this);
    	}

		if (mPrefs != null)
		{
			SharedPreferences.Editor editor = mPrefs.edit();
			editor.putInt( lastWorkingPerformanceKey, mPrefs.getInt(currentPerformanceKey, -1) );
			editor.putFloat( lastWorkingResScaleKey, mPrefs.getFloat(currentResScaleKey, 1) );
			editor.commit();
		}

		if (Apsalar.IsApsalarInitialized())
		{
			Apsalar.EndSession();
		}
	}
	
    @Override
    protected void onPause()
    {    
		Logger.LogOut("onPause");	
        super.onPause();	
		bAppActive = false;
        SetInterruption( true );
    }
	
    @Override
    protected void onResume()
    {
		Logger.LogOut("onResume");	
        super.onResume();

		// pass the keyboard state in case it was flipped while we were paused
		NativeCallback_KeyPadChange( getResources().getConfiguration().navigationHidden != Configuration.NAVIGATIONHIDDEN_YES );
		// update network immediately
		UpdateNetworkStatus();
		bAppActive = true;		
        SetInterruption( false );
    }
    
    @Override
    public void onDestroy()
    {
		Logger.LogOut("onDestroy");	
		// if we are fully destroyed, we can forget about the language
		if (mPrefs != null)
		{
			SharedPreferences.Editor ed = mPrefs.edit();
			ed.putString("language", "null");
			ed.commit();		
		}
        super.onDestroy();
        systemCleanup();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        boolean ret = super.onTouchEvent(event);
        
		// ignore touch event is soft keyboard is open
        if( bKeyboardOpen )
        {
			return true;
        }
        
        // true means something was captured, so super takes precidence
        if ( !ret )
        {
			int action		= event.getAction();
			int actionCode	= action & MotionEvent.ACTION_MASK;
			
			if( actionCode == MotionEvent.ACTION_MOVE )
			{		
				// on moves dump them all out	
				for (int pointerIter = 0; pointerIter < event.getPointerCount(); pointerIter++)
				{
					ret |= NativeCallback_InputEvent( MotionEvent.ACTION_MOVE, (int) event.getX( pointerIter ), (int) event.getY( pointerIter ), event.getPointerId( pointerIter ), event.getEventTime() );
				}		
			}
			else
			{			
				// if we get a real action print it out and the owner	
				int OwningIndex		= action >> MotionEvent.ACTION_POINTER_ID_SHIFT;
				int OwningPointer	= event.getPointerId( OwningIndex );
				
				// translate these to the basic codes
				if( actionCode == MotionEvent.ACTION_POINTER_DOWN )
				{
					actionCode = MotionEvent.ACTION_DOWN;
				} 
				else if( actionCode == MotionEvent.ACTION_POINTER_UP ) 
				{
					actionCode = MotionEvent.ACTION_UP;
				}			
							
				// one "special" action at a time
				ret |= NativeCallback_InputEvent( actionCode, (int) event.getX( OwningIndex ), (int) event.getY( OwningIndex ), OwningPointer, event.getEventTime() );				
	        }
        }
             
        //DumpMotionEvent( event );                    
        return ret;
    }

	@Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
		Logger.LogOut( "onKeyDown " + keyCode + event);

		if( keyCode == KeyEvent.KEYCODE_MENU  ||
			keyCode == KeyEvent.KEYCODE_SEARCH ||
			keyCode == KeyEvent.KEYCODE_BACK  )
		{
			return true;
		}
		else
        {
	        return super.onKeyDown(keyCode, event);
	    }
    }
	
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event)
    {
		Logger.LogOut( "onKeyUp " + keyCode + event);

		// menu and search do nothing
		if( keyCode == KeyEvent.KEYCODE_MENU  ||
			keyCode == KeyEvent.KEYCODE_SEARCH  )
		{
			return true;
		}		

		// keyboard returned
		if( bKeyboardOpen && ( keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_BACK ) )
		{			
			JavaCallback_HideKeyBoard( false );
			return true;
		}

		// stop moving playing if back button is pressed
		if( (bIsMoviePlaying || bIsLoadingMoviePlaying) && keyCode == KeyEvent.KEYCODE_BACK )
		{
			MovieError();
			return true;
		}
           
		// if super doesn't handle it let us handle it
        return super.onKeyUp(keyCode, event);
	}


	////////////////////////////////////////////////
	// IDownloaderClient Overrides
	///////////////////////////////////////////////

    /**
     * Sets the state of the various controls based on the progressinfo object
     * sent from the downloader service.
     */
    @Override
    public void onDownloadProgress(DownloadProgressInfo progress) 
    {
    	averageSpeed.setText( getString(R.string.kilobytes_per_second, Helpers.getSpeedString(progress.mCurrentSpeed)) );
        timeRemaining.setText( getString(R.string.time_remaining, Helpers.getTimeRemaining(progress.mTimeRemaining)) );

        progress.mOverallTotal = progress.mOverallTotal;
        progressBar.setMax( (int) (progress.mOverallTotal >> 8) );
        progressBar.setProgress( (int) (progress.mOverallProgress >> 8) );
        progressPercent.setText( Long.toString(progress.mOverallProgress * 100 / progress.mOverallTotal) + "%" );
        progressFraction.setText( Helpers.getDownloadProgressString(progress.mOverallProgress, progress.mOverallTotal) );
    }

    /**
	* Handle event changes during the download process
    */
    @Override
    public void onDownloadStateChanged(int newState) 
    {
    	boolean showDashboard = true;
        boolean showCellMessage = false;
        boolean paused;
        boolean indeterminate;
        switch ( newState ) 
        {
            case IDownloaderClient.STATE_IDLE:
                // STATE_IDLE means the service is listening, so it's
                // safe to start making calls via remoteService.
                Logger.LogOut("State idle");
                paused = false;
                indeterminate = true;
                break;
            case IDownloaderClient.STATE_CONNECTING:
            case IDownloaderClient.STATE_FETCHING_URL:
                Logger.LogOut("State: connecting");
                showDashboard = true;
                paused = false;
                indeterminate = true;
                break;
            case IDownloaderClient.STATE_DOWNLOADING:
                Logger.LogOut("State: downloading");
                paused = false;
                showDashboard = true;
                indeterminate = false;
                break;

            case IDownloaderClient.STATE_FAILED_CANCELED:
            	Logger.LogOut("State: Canceled");
            	paused = true;
                showDashboard = false;
                indeterminate = false;
                break;

            case IDownloaderClient.STATE_FAILED:
            	Logger.LogOut("State: General fail");
            case IDownloaderClient.STATE_FAILED_FETCHING_URL:
            	Logger.LogOut("State: Failed fetching URL");
            	paused = true;
                showDashboard = false;
                indeterminate = false;
                break;
            case IDownloaderClient.STATE_FAILED_UNLICENSED:
                Logger.LogOut("State: Failed, unlicensed");
                paused = true;
                showDashboard = false;
                indeterminate = false;
                break;
            case IDownloaderClient.STATE_PAUSED_NEED_CELLULAR_PERMISSION:
            case IDownloaderClient.STATE_PAUSED_WIFI_DISABLED_NEED_CELLULAR_PERMISSION:
                Logger.LogOut("State: need permissions");
                showDashboard = false;
                paused = true;
                indeterminate = false;
                showCellMessage = true;
                break;

            case IDownloaderClient.STATE_PAUSED_WIFI_DISABLED:
            	showDashboard = false;
                paused = true;
                indeterminate = false;
                showCellMessage = false;
                InitializeForceExitMessage(R.string.Wifi_Disabled);
                break;
            case IDownloaderClient.STATE_PAUSED_NEED_WIFI:
            	Logger.LogOut("Need wifi");
            	showDashboard = false;
                paused = true;
                indeterminate = false;
                showCellMessage = false;
                InitializeForceExitMessage(R.string.Wifi_Disabled);
            	break;

            case IDownloaderClient.STATE_PAUSED_BY_REQUEST:
                Logger.LogOut("State: paused");
                paused = true;
                indeterminate = false;
                break;
            case IDownloaderClient.STATE_PAUSED_ROAMING:
            case IDownloaderClient.STATE_PAUSED_SDCARD_UNAVAILABLE:
                paused = true;
                indeterminate = false;
                break; 
            case IDownloaderClient.STATE_COMPLETED:
                Logger.LogOut("State completed");
                showDashboard = false;
                paused = false;
                indeterminate = false;
                DeleteOldExpansionFiles();
                DecrementDownloadsCounter();
                if ( numDownloadsRunning == 0 )
                	AppInitialStartup();
                return;
            case IDownloaderClient.STATE_FAILED_SDCARD_FULL:
            	showDashboard = false;
                paused = true;
                indeterminate = false;
                showCellMessage = false;
                InitializeForceExitMessage(R.string.Out_Of_Memory);
                break;

            default:
            	Logger.LogOut("State changed");
            	paused = true;
                indeterminate = true;
                showDashboard = true;
        }

        int newDashboardVisibility = showDashboard ? View.VISIBLE : View.GONE;
        if ( dashboard.getVisibility() != newDashboardVisibility ) 
        {
            dashboard.setVisibility(newDashboardVisibility);
        }
        int cellMessageVisibility = showCellMessage ? View.VISIBLE : View.GONE;
        if ( cellMessage.getVisibility() != cellMessageVisibility ) 
        {
            cellMessage.setVisibility(cellMessageVisibility);
        }

        progressBar.setIndeterminate(indeterminate);
        SetButtonPausedState(paused);
    }




	/**
     * Critical implementation detail. In onServiceConnected we create the
     * remote service and marshaler. This is how we pass the client information
     * back to the service so the client can be properly notified of changes. We
     * must do this every time we reconnect to the service.
     */
    @Override
    public void onServiceConnected(Messenger m) 
    {
    	Logger.LogOut("onServiceConnected");
        remoteService = DownloaderServiceMarshaller.CreateProxy(m);
        remoteService.onClientUpdated(downloaderClientStub.getMessenger());
    }



	////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////
	// JNI FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////
    
	/////////////////////
	// NATIVE -> JAVA 
	/////////////////////	
	// AUDIO		
	public void JavaCallback_PlaySong( FileDescriptor SongFD, long SongOffset, long SongLength, String SongName ) { PlaySong( SongFD, SongOffset, SongLength, SongName ); }
	public void JavaCallback_StopSong() {  StopSong(); }
	public int JavaCallback_LoadSoundFile( String SoundPath ) {	return LoadSoundFile( SoundPath ); }
	public void JavaCallback_UnloadSoundID( int SoundID ) { UnloadSoundID( SoundID ); }
	public int JavaCallback_PlaySound( int SoundID, boolean Looping ) { return PlaySound( SoundID, Looping ); }	
	public void JavaCallback_StopSound( int StreamID ) { StopSound( StreamID );	}	
	public void JavaCallback_SetVolume( int StreamID, float Volume ) { SetVolume( StreamID, Volume ); }	    
	public void JavaCallback_UpdateSongPlayer( float DeltaTime ) { UpdateSongPlayer( DeltaTime ); }
	// GRAPHICS
	public boolean JavaCallback_swapBuffers() {	return swapBuffers(); }	
	public boolean JavaCallback_makeCurrent() { return makeCurrent(); }
	public boolean JavaCallback_unMakeCurrent() { return unMakeCurrent(); }
	public boolean JavaCallback_initEGL(EGLConfigParms parms) { return initEGL( parms ); }		
	public void JavaCallback_SetFixedSizeScale( float InScale ) { SetFixedSizeScale( InScale ); }
	// MOVIES
    public void JavaCallback_StartVideo( FileDescriptor MovieFD, long MovieOffset, long MovieLength ) { StartVideo( MovieFD, MovieOffset, MovieLength ); }
	public void JavaCallback_StopVideo() { StopVideo(); }	
	public boolean JavaCallback_IsVideoPlaying() { return bIsMoviePlaying;	}
	public void JavaCallback_VideoAddTextOverlay(String text) { VideoAddTextOverlay(text); }
	// SOFT KEYBOARD
	public void JavaCallback_ShowKeyBoard( String InputText, float posX, float posY, float sizeX, float sizeY, boolean IsPassword ) { ShowKeyBoard( InputText, posX, posY, sizeX, sizeY, IsPassword ); }	
	public void JavaCallback_HideKeyBoard( boolean wasCancelled ) { HideKeyBoard( wasCancelled ); }	
	// MISC
	public void JavaCallback_ShowWebPage( String theURL ) { ShowWebPage( theURL ); }      
    public void JavaCallback_ShutDownApp() { Process.killProcess(Process.myPid()); }
	public boolean JavaCallback_hasAppLocalValue(String key) { return hasAppLocalValue( key ); }
    public String JavaCallback_getAppLocalValue(String key) { return getAppLocalValue( key ); }
    public void JavaCallback_setAppLocalValue(String key, String value) { setAppLocalValue( key, value ); }
    public boolean hasAppLocalValue(String key) { return appLocalValues.containsKey(key); }
    public String getAppLocalValue(String key) { return appLocalValues.get(key); }
    public void setAppLocalValue(String key, String value) { appLocalValues.put(key, value); }
	public void JavaCallback_HideSplash() { HideSplash(); }
	public void JavaCallback_HideReloader() { HideReloader(); }
	public int JavaCallback_GetPerformanceLevel() { return GetPerformanceLevel(); }
	public float JavaCallback_GetResolutionScale() { return GetResolutionScale(); }
	public String JavaCallback_GetMainAPKExpansionName() { return MainExpansionFilePath; }
	public String JavaCallback_GetPatchAPKExpansionName() { return PatchExpansionFilePath; }
	public void JavaCallback_OpenSettingsMenu() { OpenSettingsMenu(); }
	public int JavaCallback_GetDepthSize() { return DepthSize; }
	public boolean JavaCallback_IsExpansionInAPK() { return bIsExpansionInAPK; }
	public AssetManager JavaCallback_GetAssetManager() { return AssetManagerReference; }
	public String JavaCallback_GetAppCommandLine() { return appCommandLine; }
	public String JavaCallback_GetDeviceModel() { return Build.MODEL; }

	// Apsalar
	public void JavaCallback_ApsalarInit() { Apsalar.Init(); }
	public void JavaCallback_ApsalarStartSession(String ApiKey, String ApiSecret) { Apsalar.StartSession(ApiKey, ApiSecret); }
	public void JavaCallback_ApsalarEndSession() { Apsalar.EndSession(); }
	public void JavaCallback_ApsalarEvent(String EventName) { Apsalar.Event(EventName); }
	public void JavaCallback_ApsalarEventParam(String EventName, String ParamName, String ParamValue) { Apsalar.EventParam(EventName, ParamName, ParamValue); }
	public void JavaCallback_ApsalarEventParamArray(String EventName, String[] Params) { Apsalar.EventParamArray(EventName, Params); }
	public void JavaCallback_ApsalarLogEngineData(String EventName, int EngineVersion) { Apsalar.LogEventEngineData(EventName, EngineVersion); }

	/////////////////////
	// JAVA -> NATIVE
	/////////////////////
	public native void		NativeCallback_KeyPadChange( boolean IsVisible );
	public native boolean	NativeCallback_InitEGLCallback();   
    public native boolean	NativeCallback_Initialize( int drawWidth, int drawHeight, float ScreenDiagonal, boolean bFullMultiTouch );
	public native void		NativeCallback_PostInitUpdate( int drawWidth, int drawHeight );	
	public native void		NativeCallback_Cleanup();    
    public native void		NativeCallback_KeyboardFinished( String TextOut );
    public native void		NativeCallback_MovieFinished();
    public native boolean	NativeCallback_SystemStats( long SystemMemory );
    public native boolean	NativeCallback_InterruptionChanged( boolean InterruptionActive );
    public native void		NativeCallback_NetworkUpdate( boolean IsConnected, boolean IsWifiConnected );
    public native void		NativeCallback_LanguageSet(String LocaleSet);
    public native boolean	NativeCallback_InputEvent(int action, int x, int y, int PointerIndex, long TimeStamp);
    public native boolean 	NativeCallback_IsShippingBuild();
	public native String	NativeCallback_ApsalarGetKey();
	public native String	NativeCallback_ApsalarGetSecret();
	public native static String	NativeCallback_PhoneHomeGetURL();
	public native static int	NativeCallback_GetEngineVersion();
	public native int 		NativeCallback_GetPerformanceLevel();
	public native float 	NativeCallback_GetResolutionScale();
	public native static void	NativeCallback_UpdatePerformanceSettings(int PerformanceLevel, float ResolutionScale);
	public native void		NativeCallback_EGLSurfaceRecreated();
	public native String	NativeCallback_GetGameName();

	////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////
	// APP
	////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////
	/// Function: 
	///    CanDownloadExpansionFiles
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - true if we can download files, otherwise false
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Checks whether the correct expansion files exist to be used. If not, starts
	///	   an Intent to download what is needed. After the download finishes the application
	///    will be automatically started from onDownloadStateChanged.
	///    
	////////////////////////////////////
	public boolean CanDownloadExpansionFiles()
	{		
		Logger.LogOut("Check for expansion files");

		final Activity act = this;

		Context ApplicationContext = getApplicationContext();
		ConnectivityManager ConnManager = (ConnectivityManager) ApplicationContext.getSystemService(ApplicationContext.CONNECTIVITY_SERVICE);
		NetworkInfo WifiNetworkInfo = ConnManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
		boolean HaveWifi = WifiNetworkInfo != null ? WifiNetworkInfo.isConnected() : false;
		NetworkInfo MobileNetworkInfo = ConnManager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
		boolean HaveMobileData = MobileNetworkInfo != null ? MobileNetworkInfo.isConnected() : false;
		if ( !HaveWifi && !HaveMobileData )
		{	
			// if we can't get info from the internet, try searhing for existing expansion files and use those
			if (CheckForExistingExpansionFiles())
			{
				return false;
			}
			else
			{
				// if we can't check integrity close down
				new AlertDialog.Builder(act)
					.setMessage( getString(R.string.NoConnectionOrExpansions) )
					.setPositiveButton("Ok",
						new DialogInterface.OnClickListener () {
						public void onClick(DialogInterface i, int a)
						{
							finish();
						}
					}
				)
				.setCancelable(false)
				.show();

				return true;
			}
		}


		try
	    {
	        //Build an Intent to start this activity from the Notification
            Intent launchIntent = UE3JavaApp.this.getIntent();
	        Intent activityLauncher = new Intent(UE3JavaApp.this, UE3JavaApp.this.getClass());
	        activityLauncher.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
	        activityLauncher.setAction(launchIntent.getAction());
	        

	        if ( launchIntent.getCategories() != null )
	        {
	        	for (String category : launchIntent.getCategories())
	        	{
	        		activityLauncher.addCategory(category);
	        	}
	        }

	        PendingIntent pendingIntent = PendingIntent.getActivity(UE3JavaApp.this, 0, 
	        	activityLauncher, PendingIntent.FLAG_UPDATE_CURRENT);

	        // Start the download service (if required)
	        int startResult = DownloaderClientMarshaller.startDownloadServiceIfRequired(this,
	                        pendingIntent, UE3JavaFileDownloader.class);


		    // If download has started, initialize this activity to show download progress
	        if (startResult != DownloaderClientMarshaller.NO_DOWNLOAD_REQUIRED) 
	        {
				PopulateXAPKs();

	        	if ( !IsThereSpaceForExpansionFiles() )
		    	{
		    		InitializeForceExitMessage(R.string.Out_Of_Memory);
		    	}
		    	else
		    	{
	        		InitializeDownloadUI();
	        	}
    			return true;
	        } 
	    }
	    catch (NameNotFoundException e)
	    {
	    	Logger.ReportException("Downloader Client Marshaller", e);
	    }

	    return false;

	}	

	////////////////////////////////////
	/// Function: 
	///    AppInitialStartup
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void AppInitialStartup()
	{		
		Logger.LogOut( "=============================================");
		Logger.LogOut( "=============================================");
		Logger.LogOut( "=============================================");
		Logger.LogOut( "========== UNREAL ENGINE 3 STARTUP ==========");
		Logger.LogOut( "=============================================");
		Logger.LogOut( "=============================================");
		Logger.LogOut( "=============================================");

		// Make sure XAPKs are populated
		PopulateXAPKs();

		if ( NativeCallback_IsShippingBuild() && !skipDownloader )
		{
			// Get the version information from the server and build our expansion file paths
			int MainVersion = 0;
			int PatchVersion = 0;
			if ( xAPKS != null && xAPKS.length > 0 )
			{
	        	MainVersion = xAPKS[0].FileVersion;
			}
			else
			{
				// fall back to APK version (often the same)
				try
				{
					MainVersion = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
				}
				catch (NameNotFoundException e)
				{
					Logger.ReportException("", e);
				}
			}

			if ( xAPKS != null && xAPKS.length > 1 )
			{
	        	PatchVersion = xAPKS[1].FileVersion;
			}
			else
			{
				// fall back to APK version (often the same)
				try
				{
					PatchVersion = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
				}
				catch (NameNotFoundException e)
				{
					Logger.ReportException("", e);
				}
			}


			BuildExpansionFilePaths(MainVersion, PatchVersion);
		}
		else
		{
			try
			{
				int VersionNumber = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
				BuildExpansionFilePaths(VersionNumber, 0);
			}
			catch (NameNotFoundException e)
			{
				Logger.ReportException("", e);
			}
		}

		String DeviceID = ((TelephonyManager)getSystemService(Context.TELEPHONY_SERVICE)).getDeviceId();

		Logger.LogOut( "DeviceID: " + DeviceID);
		Logger.LogOut( "Model: " + Build.MODEL); 
		Logger.LogOut( "PackageName: " + getPackageName() );	
		Logger.LogOut( "Android build version: " +  Integer.parseInt(Build.VERSION.SDK));
	
		handler = new Handler();		
        // startup the checks
		systemStartupCheck();
		// prefers 
		mPrefs = getApplicationContext().getSharedPreferences(getPackageName(), MODE_PRIVATE);

		// Init the class that the analytics will gather information from
		ApplicationInformation.Init(this);

		// send PhoneHome data
		AndroidPhoneHome.SetApplicationActivity(this);
		AndroidPhoneHome.QueueRequest();

		// Set Apsalar + Flurry Contexts
		Apsalar.SetApplicationContext(this);
	}	

	////////////////////////////////////
	/// Function: 
	///    systemStartupCheck
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public boolean systemStartupCheck()
	{
		final Activity act = this;  			

		// allow volume control
		setVolumeControlStream( AudioManager.STREAM_MUSIC );
        // get memory info
        ActivityManager actvityManager = (ActivityManager) this.getSystemService( ACTIVITY_SERVICE );
		ActivityManager.MemoryInfo mInfo = new ActivityManager.MemoryInfo ();
		actvityManager.getMemoryInfo( mInfo );
		// Print to log and read in DDMS
		Logger.LogOut( "Memory:" );
		Logger.LogOut( " - minfo.availMem: " + mInfo.availMem );
		Logger.LogOut( " - minfo.lowMemory: " + mInfo.lowMemory );
		Logger.LogOut( " - minfo.threshold: " + mInfo.threshold );
		bFullOpenGLReset = NativeCallback_SystemStats( mInfo.availMem );		
		
		// original storage
		ExternalStoragePath			= Environment.getExternalStorageDirectory().getAbsolutePath() + "/";
		RootExternalPath			= ExternalStoragePath;
		ContentExternalStoragePath	= ExternalStoragePath + ContentPath + "/";

		// update those base values
		UpdateAppValues();

		Logger.LogOut( "Storage Path: " + ContentExternalStoragePath );
			
		// pass the initial keyboard state
  		NativeCallback_KeyPadChange( getResources().getConfiguration().navigationHidden != Configuration.NAVIGATIONHIDDEN_YES );
			
		// total memory of device
		File MemInfoFile = new File( "proc/meminfo" );

		int TotalMemory = 512;
		if( MemInfoFile.exists() )
		{
			String MemInfoString = ReadMemFileToString( MemInfoFile );
			Logger.LogOut( MemInfoString );

			String delims = "[ ]+";
			String[] tokens = MemInfoString.split(delims);
			if( tokens.length == 3 )
			{
				TotalMemory = Integer.parseInt( tokens[1] ) / 1024;
			}
		}
		else
		{
			Logger.LogOut( "no Proc Meminfo" );
		}

		Logger.LogOut( " TotalMemory " + TotalMemory );
		
		// check for a message box
		if( TotalMemory < 256 + 1 )
		{
			handler.post(new Runnable()
            {
				public void run()
				{
					new AlertDialog.Builder(act)                             
							.setMessage( getString(R.string.MemNotEnough) )
                            .setPositiveButton("Ok",
								new DialogInterface.OnClickListener () {
									public void onClick(DialogInterface i, int a)
                                    {
										systemInit();
                                    }
                                }
                            )
                            .setCancelable(false)
                            .show();
                 }
			});
		}
		else
		{
			systemInit();
		}

		return true;
	}
	
	////////////////////////////////////
	/// Function: 
	///    PreStartup
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void PostStartup()
	{		
		final Activity act = this;  	

		// make sure we even have an sdcard
		if( !Environment.getExternalStorageState().equals( Environment.MEDIA_MOUNTED ) ) 
		{
			handler.post(new Runnable()
			{
				public void run()
				{                         
					// if we can't check integrity close down
					new AlertDialog.Builder(act)
						.setMessage( getString(R.string.NoSDCard) )
						.setPositiveButton("Ok",
							new DialogInterface.OnClickListener () {
							public void onClick(DialogInterface i, int a)
							{
								finish();
							}
						}
					)
					.setCancelable(false)
					.show();
				}
			});
							
			return;
		}

		// available space
		StatFs stat				= new StatFs(Environment.getExternalStorageDirectory().getAbsolutePath());
		stat.restat(Environment.getExternalStorageDirectory().getAbsolutePath());
        long bytesAvailable		= (long)stat.getBlockSize() * (long)stat.getFreeBlocks();
		final int megsAvailable	= (int) ( bytesAvailable / (1 * 1024 * 1024) );
		
		Logger.LogOut( "Avaliable Space(MBs): " + megsAvailable);

		// language
		String locale = getResources().getConfiguration().locale.getLanguage();
		NativeCallback_LanguageSet( locale );		
		// log out some info
		Logger.LogOut( "LanguageText: " + locale );	
				
		// startup splash and loading icon
		SplashLayout = new RelativeLayout( act );
		SplashScreen = new ImageView( act );
		IndefiniteLoadingBar = new ProgressBar( act );		
		
		Bitmap SplashImage = BitmapFactory.decodeResource(getResources(), R.drawable.splash);
		SplashScreen.setImageBitmap( SplashImage );
		
		// Scale image in layout to avoid stretching
		int screenWidth = getWindowManager().getDefaultDisplay().getWidth();
		int screenHeight = getWindowManager().getDefaultDisplay().getHeight();

		float videoRatio = ((float) SplashImage.getWidth()) / SplashImage.getHeight();
		float screenRatio = ((float) screenWidth) / screenHeight;

		int scaledWidth,scaledHeight;

		if (screenRatio > videoRatio)
		{
			scaledWidth = (int) (((float)SplashImage.getWidth() / (float)SplashImage.getHeight()) * (float)screenHeight);
			scaledHeight = screenHeight;
		}
		else
		{
			scaledWidth = screenWidth;
			scaledHeight = (int) (((float)SplashImage.getHeight() / (float)SplashImage.getWidth()) * (float)screenWidth);
		}

		RelativeLayout.LayoutParams SplashParams = new RelativeLayout.LayoutParams(scaledWidth, scaledHeight);
		SplashParams.addRule( RelativeLayout.CENTER_IN_PARENT );
		SplashLayout.addView( SplashScreen, SplashParams ); 

		SplashLayout.addView( IndefiniteLoadingBar, new LayoutParams( 50,50 ) );  
		addContentView( SplashLayout, new LayoutParams( LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT ) ); 

		// TODO: move all of these setup stuff into a new system, similiar to get local app values but only called once
		DisplayMetrics metrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics(metrics);

		// check for actual inches
		float ScreenWidthInches			= ( 1.0f / metrics.xdpi ) * metrics.widthPixels;
		float ScreenHeightInches		= ( 1.0f / metrics.ydpi ) * metrics.heightPixels;
		final float ScreenDiagonal		= (float)Math.sqrt( (float)Math.pow( ScreenWidthInches, 2 ) + (float)Math.pow( ScreenHeightInches, 2 ) );
		Logger.LogOut( "Screen Size(Inches) X: " + ScreenWidthInches + " Y: " + ScreenHeightInches + " Diag: " + ScreenDiagonal );			
		// check for true multi touch
		PackageManager packageM = getPackageManager();
		boolean bTrueMultiTouch = false;//packageM.hasSystemFeature( PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT );

		Logger.LogOut( "FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT " + bTrueMultiTouch );
				
		// finish the startup		
		bRanInit = true;
		if( !NativeCallback_Initialize( surfaceWidth, surfaceHeight, ScreenDiagonal, bTrueMultiTouch ) )
		{
			handler.post(new Runnable()
			{
				public void run()
				{
					new AlertDialog.Builder(act)                             
						.setMessage( getString(R.string.Init_Failed) )
						.setPositiveButton("Ok",
						new DialogInterface.OnClickListener () {
							public void onClick(DialogInterface i, int a)
							{
								finish();
							}
					}
					)
						.setCancelable(false)
						.show();
				}
			});                    
		}  	
	}	
		
	////////////////////////////////////
	/// Function: 
	///    BeginOpenGLStartup
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void BeginOpenGLStartup()
	{
		final Activity act = this;  

		// Setting up layouts and views
        PrimaryGPUView = new SurfaceView(this);
        SurfaceHolder CastGPUHolder = PrimaryGPUView.getHolder();
        CastGPUHolder.setType(SurfaceHolder.SURFACE_TYPE_GPU);

        CastGPUHolder.addCallback(new Callback()
        {
            // @Override
            public void surfaceCreated(SurfaceHolder holder)
            {
		        Logger.LogOut("in surfaceCreated");

				bSurfaceCreatedThisPass = true;

				holder.setType(SurfaceHolder.SURFACE_TYPE_GPU);
						        
                boolean eglInitialized	= true;

				//
                if (eglContext == null)
                {
			        Logger.LogOut("calling initEGLCallback");
                    eglInitialized = NativeCallback_InitEGLCallback();
			        Logger.LogOut("initEGLCallback returned");
                }

                if (!nativeEGL && eglInitialized)
				{
					// make sure we created the surface
					eglInitialized = createEGLSurface(holder);
				}

				// if not first run we need to wake up from interruption
				if( bFirstSurfaceCreated )
				{	
					NativeCallback_EGLSurfaceRecreated();

					// also activate the reloading icon or video since recreating graphics assets can be time consuming
					ShowReloader();
					
					SetInterruption( false );
				}

				// if never had a surface
				if( !bFirstSurfaceCreated )
                {
					bFirstSurfaceCreated = true;

                    if (eglInitialized)
					{
						handler.post(new Runnable()
						{
							public void run()
							{
								PostStartup();
							}
						});
					}
                    else
                    {
                        handler.post(new Runnable()
                        {
                            public void run()
                            {
                                 new AlertDialog.Builder(act)
                                    .setMessage( getString(R.string.OpenGL_Failed) )
                                    .setPositiveButton("Ok",
                                        new DialogInterface.OnClickListener () {
                                            public void onClick(DialogInterface i, int a)
                                            {
                                                finish();
                                            }
                                        }
                                    )
                                    .setCancelable(false)
                                    .show();
                            }
                        });
                    }
                }
            }

            // @Override
            public void surfaceChanged(SurfaceHolder holder, int format,
                    int width, int height)
            {
                Logger.LogOut("Surface changed: " + width + ", " + height);
				// some phones gave it the other way...
				surfaceWidth	= width > height ? width : height;
				surfaceHeight	= width > height ? height : width;
				NativeCallback_PostInitUpdate( surfaceWidth, surfaceHeight );
            }

            // @Override
            public void surfaceDestroyed(SurfaceHolder holder)
            {
				Logger.LogOut("Surface surfaceDestroyed");

				// clean up all of open GL!
				if (!nativeEGL)
				{
					SetInterruption( true );
					// shutdown all of opengl (since context may have chance of being lost anyway)
					cleanupEGL();
				}
            }
        });
		
		setContentView( PrimaryGPUView );
	}
			
	////////////////////////////////////
	/// Function: 
	///    ReadMemFileToString
	///    
	/// Specifiers: 
	///    [static] - 
	///    [public] - 
	///    [String] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	static public String ReadMemFileToString(File aFile) 
	{
		//...checks on aFile are elided
		StringBuilder contents = new StringBuilder();
	    
		try 
		{
			//use buffering, reading one line at a time
			//FileReader always assumes default encoding is OK!
			BufferedReader input =  new BufferedReader(new FileReader(aFile));
			
			String line = null; 
			while (( line = input.readLine()) != null)
			{
				if( line.indexOf("MemTotal") != -1 )
				{
					contents.append(line);
					input.close();
					return contents.toString();
				}
			}			
			
			input.close();			
		}
		catch (IOException ex)
		{
			ex.printStackTrace();
		}
	    
		return contents.toString();
	}

    
	////////////////////////////////////
	/// Function: 
	///    systemInit
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///   System initialization code. Kept separate from the {@link #init()} function so that subclasses
    ///   in their simplest form do not need to call any of the parent class' functions. This to make
    ///   it easier for pure C/C++ application so that these do not need to call java functions from C/C++
    ///   code.
	///    
	////////////////////////////////////
	public boolean systemInit()
    {			       
        final Activity act = this;  	
        
		// init base sound pool
        GSoundPool = new SoundPool( 6, AudioManager.STREAM_MUSIC, 0); 

		// TODO: move all of these setup stuff into a new system, similiar to get local app values but only called once
		DisplayMetrics metrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics(metrics);

		ScreenSizeX = metrics.widthPixels;			
		ScreenSizeY = metrics.heightPixels;
         		
		// honeycomb is 11, Build.VERSION.SDK_INT < 11;
		
		// startup the opengl portion
		BeginOpenGLStartup();
		        
        // network update timer
        UpdateNetworkTask = new Runnable() 
        {
		   public void run() 
		   {
				UpdateNetworkStatus();		     
				handler.postDelayed(this, 1000 );
		   }
		};
		
		// push it out
		handler.post( UpdateNetworkTask );

		PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
		CurrentLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK, "ScreenUp");
		CurrentLock.acquire();
 
        return true;
    }
		
	////////////////////////////////////
	/// Function: 
	///    HaltMedia
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Called when game is backgrounded, stops or pause media
	///    
	////////////////////////////////////
	public void HaltMedia()
	{
		try
		{			
			if( mediaPlayer != null || loadingVideoPlayer != null )
			{
				//stop and report it finished
				MovieError();
			}				
		} 
		catch ( Exception e )
		{			
			Logger.ReportException( "Failed Pause Movie ", e );
		}

		try
		{						
			if( songPlayer != null )
			{
				songPlayer.pause();
			}
		} 
		catch ( Exception e )
		{					
			Logger.ReportException( "Failed HaltMusic ", e );
		}	
	}
		
	////////////////////////////////////
	/// Function: 
	///    RestoreMedia
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void RestoreMedia()
	{
		try
		{						
			if( songPlayer != null )
			{
				songPlayer.start();
			}	
		} 
		catch ( Exception e )
		{					
			Logger.ReportException( "Failed RestoreMusic ", e );
		}	
	}

	// Create an anonymous implementation of OnClickListener
	private OnClickListener mKeyboardAccepted = new OnClickListener() 
	{
		public void onClick(View v) 
		{			
			JavaCallback_HideKeyBoard( false );
		}
	};
	private OnClickListener mKeyboardCancel = new OnClickListener() 
	{
		public void onClick(View v) 
		{
			JavaCallback_HideKeyBoard( true );
		}
	};
	
	////////////////////////////////////
	/// Function: 
	///    SetInterruption
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [boolean PauseGame] - 
	///    [boolean bIgnore] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void SetInterruption( boolean PauseGame )
	{		
		Logger.LogOut( "In SetInterruption " + PauseGame + ":" + paused + ":" + bWindowHasFocus );

		if( PauseGame && !paused )
		{			
			Logger.LogOut( "PAUSING" );
			paused = true;        	      
			HaltMedia();

			//a badness interruption
			if( !NativeCallback_InterruptionChanged( true ) )
			{
				Logger.LogOut( "Bad interruption");
				Process.killProcess(Process.myPid());
			}

			if ( mPrefs != null )
			{
				SharedPreferences.Editor ed = mPrefs.edit();
				ed.putString("language", getResources().getConfiguration().locale.getLanguage());
				ed.commit();
			}
	        
			// no need for the lock
			if( CurrentLock != null )
			{
				CurrentLock.release();
				CurrentLock = null;
			}  
		}

		if( !PauseGame && paused && bWindowHasFocus && eglSurface != null )
		{
			Logger.LogOut( "SetInterruption: " + PauseGame );	

			paused = false; 
        
			// this shouldn't happen, as it'd be resumed
			if( CurrentLock != null )
			{
				CurrentLock.release();
				CurrentLock = null;
			}       
			
			PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
			CurrentLock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK, "DoNotDimScreen");
			CurrentLock.acquire();
			
			RestoreMedia();	

			UpdateAppValues();


			if( !NativeCallback_InterruptionChanged( false ) )
			{
				Logger.LogOut( "Bad interruption");
				Process.killProcess(Process.myPid());
			}

		}
	}		  
    
	
	////////////////////////////////////
	/// Function: 
	///    UpdateAppValues
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void UpdateAppValues()
	{
		if( appLocalValues == null )
		{
			appLocalValues = new HashMap();
		}
		
		appLocalValues.put("STORAGE_ROOT",		ContentExternalStoragePath );				
		appLocalValues.put("BASE_DIR",			ContentPath );
		appLocalValues.put("EXTERNAL_ROOT",		RootExternalPath);//DAS used to be 'ExternalStoragePath' this but this value won't be correct for nondesignated storage 

		String localIp = getLocalIpAddress();
		Logger.LogOut("Local Ip: " + localIp);

		if (localIp == null) 
		{
			Logger.LogOut("Setting ip to empty string");
			appLocalValues.put("LOCAL_IP", "");
		}
		else 
		{
			appLocalValues.put("LOCAL_IP", localIp);
		}
	}

	////////////////////////////////////
	/// Function: 
	///    DumpMotionEvent
	///    
	/// Specifiers: 
	///    [private] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	private void DumpMotionEvent(MotionEvent event) 
    {
	   String names[] = { "DOWN" , "UP" , "MOVE" , "CANCEL" , "OUTSIDE" , "POINTER_DOWN" , "POINTER_UP" , "7?" , "8?" , "9?" };	   
	   StringBuilder sb = new StringBuilder();	   
	   int action = event.getAction();	   	   
	   int actionCode = action & MotionEvent.ACTION_MASK;
	   
	   // translate these to the basic codes
		if( actionCode == MotionEvent.ACTION_POINTER_DOWN )
		{
			actionCode = MotionEvent.ACTION_DOWN;
		} 
		else if( actionCode == MotionEvent.ACTION_POINTER_UP ) 
		{
			actionCode = MotionEvent.ACTION_UP;
		}	
				
	   sb.append("event ACTION_" ).append(names[actionCode]);
	   if (actionCode == MotionEvent.ACTION_DOWN || actionCode == MotionEvent.ACTION_UP) 
	   {
		  sb.append("(pid " ).append( event.getPointerId( action >> MotionEvent.ACTION_POINTER_ID_SHIFT ) );
		  sb.append(")" );
		  
		  Logger.LogOut( sb.toString());
	   }

	   //sb.append("[" );
	   //for (int i = 0; i < event.getPointerCount(); i++)
	   //{
		  //sb.append("#" ).append(i);
		  //sb.append("(pid " ).append(event.getPointerId(i));
		  //sb.append(")=" ).append((int) event.getX(i));
		  //sb.append("," ).append((int) event.getY(i));
		  //if (i + 1 < event.getPointerCount())
		  //{
			 //sb.append(";" );
		  //}
	   //}
	   //sb.append("]" );
	}

	////////////////////////////////////
	/// Class: 
	///    class MoviePreparationThread implements Runnable extends LoggingHelper
	///    
	/// Description: 
	///    Prepare movies to be played on seperate thread to prevent an issue on some phones never playing the movie
	///    
	////////////////////////////////////
	public class MoviePreparationThread implements Runnable 
	{
		private MediaPlayer		OurMediaPlayer;
		private UE3JavaApp		MessageSystem;
		private SurfaceView		VideoView;
		
		// Data source information if mediaplayer fails
		private FileDescriptor MovieFD;
		private long MovieOffset;
		private long MovieLength;

		public MoviePreparationThread(MediaPlayer InMediaPlayer, UE3JavaApp InMessageSystem, SurfaceView InVideoView, FileDescriptor InMovieFD, long InMovieOffset, long InMovieLength) 
		{
			OurMediaPlayer = InMediaPlayer;
			MessageSystem = InMessageSystem;
			VideoView = InVideoView;

			MovieFD = InMovieFD;
			MovieOffset = InMovieOffset;
			MovieLength = InMovieLength;
		}

		public void run() 
		{
			try
			{			
				OurMediaPlayer.setOnPreparedListener(new OnPreparedListener() 
				{ 
					@Override
					public void onPrepared(MediaPlayer mp) 
					{
						// Set SurfaceView size to preserve aspect ratio
						int videoWidth = OurMediaPlayer.getVideoWidth();
						int videoHeight = OurMediaPlayer.getVideoHeight();

						// Media player may be unreliable, try grabbing first frame if 0 dimension video
						if (videoWidth == 0 || videoHeight == 0)
						{
							// MediaMetadataRetriever is available in API 10
							if (Build.VERSION.SDK_INT >= 10)
							{
								MediaMetadataRetriever retriever = new  MediaMetadataRetriever();
								retriever.setDataSource(MovieFD, MovieOffset, MovieLength);
								Bitmap bmp = retriever.getFrameAtTime(0);

								if (bmp != null)
								{
									videoWidth = bmp.getWidth();
									videoHeight = bmp.getHeight();

									bmp.recycle();
								}
							}
						}

						// if we still can't determine the video resolution fall back to filling the screen
						if (videoWidth == 0 || videoHeight == 0)
						{
							videoWidth = MessageSystem.getWindowManager().getDefaultDisplay().getWidth();
							videoHeight = MessageSystem.getWindowManager().getDefaultDisplay().getHeight();
						}

						int screenWidth = MessageSystem.getWindowManager().getDefaultDisplay().getWidth();
						int screenHeight = MessageSystem.getWindowManager().getDefaultDisplay().getHeight();

						float videoRatio = ((float) videoWidth) / videoHeight;
						float screenRatio = ((float) screenWidth) / screenHeight;

						LayoutParams layoutParams = VideoView.getLayoutParams();

						if (screenRatio > videoRatio)
						{
							layoutParams.width = (int) (((float)videoWidth / (float)videoHeight) * (float)screenHeight);
							layoutParams.height = screenHeight;
						}
						else
						{
							layoutParams.width = screenWidth;
							layoutParams.height = (int) (((float)videoHeight / (float)videoWidth) * (float)screenWidth);
						}

						VideoView.setLayoutParams(layoutParams);
						
						mp.start();
					}
				});
			
				OurMediaPlayer.setOnVideoSizeChangedListener(new OnVideoSizeChangedListener() 
				{ 
					@Override
					public void onVideoSizeChanged(MediaPlayer mp, int videoWidth, int videoHeight)
					{
						Logger.LogOut("MediaPlayer.onVideoSizeChanged called width: " + videoWidth + " height: " + videoHeight);

						if (videoWidth == 0 || videoHeight == 0)
						{
							return;
						}

						// Set SurfaceView size to preserve aspect ratio
						int screenWidth = MessageSystem.getWindowManager().getDefaultDisplay().getWidth();
						int screenHeight = MessageSystem.getWindowManager().getDefaultDisplay().getHeight();

						float videoRatio = ((float) videoWidth) / videoHeight;
						float screenRatio = ((float) screenWidth) / screenHeight;

						LayoutParams layoutParams = VideoView.getLayoutParams();

						if (screenRatio > videoRatio)
						{
							layoutParams.width = (int) (((float)videoWidth / (float)videoHeight) * (float)screenHeight);
							layoutParams.height = screenHeight;
						}
						else
						{
							layoutParams.width = screenWidth;
							layoutParams.height = (int) (((float)videoHeight / (float)videoWidth) * (float)screenWidth);
						}

						VideoView.setLayoutParams(layoutParams);
					}
				});

				OurMediaPlayer.prepare();
			} 
			catch ( Exception e )
			{
				Logger.ReportException( "Couldn't start video!!!", e );	
				MessageSystem.MovieError();
			}									
		}
	}
	    
	////////////////////////////////////
	/// Function: 
	///    StartVideo
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void StartVideo( final FileDescriptor MovieFD, final long MovieOffset, final long MovieLength )
	{
		// make sure splash screen is hidden
		// this check is here to minimize gap between hiding splash and playing an intro video
		HideSplash();

		// always force a previous stop
		StopVideo();				

		// output the starting
		Logger.LogOut( "StartVideo Called" );
				
		final UE3JavaApp activity = this;
		
		if( MoviePrepareThread != null )
		{
			try
			{
				MoviePrepareThread.join();
			} 
			catch ( Exception e )
			{
				Logger.LogOut( "EXCEPTION - Couldnt join movie thread!!!" );			
			}	
			
			MoviePrepareThread = null;
		}
				
		bIsMoviePlaying = true;

		handler.post(new Runnable()
        {
			public void run()
            {	           
				if (videoLayout == null)
				{
					LayoutInflater inflater = getLayoutInflater(); 
					videoLayout = (RelativeLayout) inflater.inflate(R.layout.movielayout, null);					
					addContentView( videoLayout, new LayoutParams( LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT ) );
				}
				
				videoView = (SurfaceView) findViewById(R.id.movie_view);
				videoView.setZOrderMediaOverlay( true );
				SurfaceHolder videoHolder = videoView.getHolder();
				videoHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
				videoHolder.addCallback(new Callback()
				{					
					@Override
					public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height)
					{
						Logger.LogOut("StartVideo.surfaceChanged called");
					}
					@Override
					public void surfaceDestroyed(SurfaceHolder surfaceHolder) 
					{
						Logger.LogOut("StartVideo.surfaceDestroyed called");
					}
					@Override
					public void surfaceCreated(SurfaceHolder surfaceHolder)
					{
						Logger.LogOut( "StartVideo.surfaceCreated called");
						if ( videoView != null && mediaPlayer == null && surfaceHolder == videoView.getHolder() )
						{
							try
							{
								mediaPlayer = new MediaPlayer();
								mediaPlayer.setAudioStreamType( android.media.AudioManager.STREAM_MUSIC );
								mediaPlayer.reset();

								// play from SDCard or APK
								mediaPlayer.setDataSource( MovieFD, MovieOffset, MovieLength );		
								mediaPlayer.setDisplay( surfaceHolder );
								mediaPlayer.setOnCompletionListener( activity );
								
								MoviePrepareThread = new Thread(new MoviePreparationThread( mediaPlayer, activity, videoView, MovieFD, MovieOffset, MovieLength ), "MoviePrepareThread");
								MoviePrepareThread.start();	
							} 
							catch ( Exception e )
							{
								MovieError();
								Logger.LogOut( "EXCEPTION - Couldn't start video" );
							}
						}
					}
				});				
				
				TextView overlayTextView = (TextView) findViewById(R.id.overlay_text);
				overlayTextView.setText(movieOverlayMessage);

				Logger.LogOut( "Video triggered" );
			}
		});
	}
			
	public void VideoAddTextOverlay (final String text)
	{
		final Activity activity = this;

		handler.post(new Runnable()
        {
			public void run()
            {
				movieOverlayMessage = text;

				// set the textview now if movie is already playing
				if (videoLayout != null)
				{
					TextView overlayTextView = (TextView) findViewById(R.id.overlay_text);
					overlayTextView.setText(movieOverlayMessage);
				}
			}
		});
	}
				
	////////////////////////////////////
	/// Function: 
	///    StopVideo
	///    
	/// Specifiers: 
	///    [private] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	private void StopVideo()
	{
		bIsMoviePlaying = false;
	
		handler.post(new Runnable()
        {
			public void run()
            {
				Logger.LogOut( "StopVideo called" );
				if ( mediaPlayer != null )
				{
					Logger.LogOut( "Stopping video" );
					mediaPlayer.stop();
					mediaPlayer.release();
					mediaPlayer = null;
				}
				if ( videoView != null )
				{
					ViewGroup viewGroup = (ViewGroup)(videoView.getParent());
					viewGroup.removeView(videoView);
					videoView = null;
				}	
				if ( videoLayout != null)
				{
					ViewGroup viewGroup = (ViewGroup)(videoLayout.getParent());
					viewGroup.removeView(videoLayout);
					videoLayout = null;
				}	
			}				
		});
	}

	////////////////////////////////////
	/// Function: 
	///    MovieError
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void MovieError()
	{
		if (mediaPlayer != null)
		{
			NativeCallback_MovieFinished();
		}
		StopVideo();
		HideReloader();
	}

    
	////////////////////////////////////
	/// Function: 
	///    onCompletion
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Interface callback for when movie is done
	///    
	////////////////////////////////////
	public void onCompletion(MediaPlayer arg0)
	{
        Logger.LogOut( "onCompletion called");       
        NativeCallback_MovieFinished(); 
		StopVideo();
    }
		
	////////////////////////////////////
	/// Function: 
	///    GoToOurApp
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Goto Market based on app package name
	///    
	////////////////////////////////////
	public void GoToOurApp()
	{		
		handler.post(new Runnable()
		{
			public void run()
			{
				Uri uri = Uri.parse( "market://details?id=" + getPackageName() );
				Intent intent = new Intent(Intent.ACTION_VIEW, uri);
				intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
				startActivity(intent);
			}
		});
	}    
	
	////////////////////////////////////
	/// Function: 
	///    isIPAddress
	///    
	/// Specifiers: 
	///    [private] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Verify IPv4 
	///    
	////////////////////////////////////
	private boolean isIPAddress(String str) 
	{  
		Pattern ipPattern = Pattern.compile("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}");  
		return ipPattern.matcher(str).matches();  
	}  

	////////////////////////////////////
	/// Function: 
	///    getLocalIpAddress
	///    
	/// Specifiers: 
	///    [public] - 
	///    [String] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Get a Valid IP Address
	///    
	////////////////////////////////////
	public String getLocalIpAddress() 
	{	
		String OutputIPAddress = null;

		Logger.LogOut( "FINDING VALID IP ADDRESS");

		try 
		{	
			byte[] NullAddress = { 0, 0, 0, 0 };
			InetAddress EmptyAddress =  InetAddress.getByAddress( NullAddress );	
		
			for (Enumeration<NetworkInterface> en = NetworkInterface.getNetworkInterfaces(); en.hasMoreElements();) 
			{
				NetworkInterface intf = en.nextElement();
				Logger.LogOut( "Getting Element");

				for (Enumeration<InetAddress> enumIpAddr = intf.getInetAddresses(); enumIpAddr.hasMoreElements();) 
				{
					InetAddress inetAddress = enumIpAddr.nextElement();

					String CurrentIP = inetAddress.getHostAddress().toString();

					Logger.LogOut( "getLocalIpAddress: " + inetAddress.getHostAddress().toString());

					if (!inetAddress.isLoopbackAddress() && !inetAddress.equals( EmptyAddress ) &&
						 isIPAddress( CurrentIP )  ) 
					{		
						Logger.LogOut( "FOUND VALID IP: " + inetAddress.getHostAddress().toString());
						OutputIPAddress = CurrentIP;
						//return inetAddress.getHostAddress().toString();
					}
				}				
			}
		} 
		catch (Exception ex) 
		{
			System.out.println(ex.toString());
		}

		return OutputIPAddress;
	}
	
	////////////////////////////////////
	/// Function: 
	///    UpdateNetworkStatus
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void UpdateNetworkStatus()
	{
		ConnectivityManager cm = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
		if( cm != null )
		{
			NetworkInfo netInfo = cm.getNetworkInfo( ConnectivityManager.TYPE_WIFI );
			// for laziness reasons they are one in the same now
			if( netInfo != null && netInfo.isConnectedOrConnecting() ) 
			{			
				bIsOnWifi = true;	

				if( netInfo.isConnected() ) 
				{
					bIsFullyConnected = true;
				}
				else
				{
					bIsFullyConnected = false;
				}

				// set wifi to true
				NativeCallback_NetworkUpdate( true, true );
			}
			else
			{
				bIsOnWifi			= false;	
				bIsFullyConnected	= false;

				// set wifi to false
				NativeCallback_NetworkUpdate( false, false );
			}
		}
	}	
	
	////////////////////////////////////
	/// Function: 
	///    ShowWebPage
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void ShowWebPage( String theURL )
	{
		final String FinalURL = theURL;
		
		handler.post(new Runnable()
		{
			public void run()
			{
				Uri uri = Uri.parse(FinalURL);
				Intent intent = new Intent(Intent.ACTION_VIEW, uri);
				intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
				startActivity(intent);
			}
		});
	}    
	
	////////////////////////////////////
	/// Function: 
	///    ShowKeyBoard
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [String InputText] - 
	///    [float posX] - 
	///    [float posY] - 
	///    [float sizeX] - 
	///    [float sizeY] - 
	///    [boolean IsPassword] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void ShowKeyBoard( String InputText, float posX, float posY, float sizeX, float sizeY, boolean IsPassword )
	{
 		Logger.LogOut("ShowKeyBoard");
 		
 		final UE3JavaApp activity		= this;
		final String FinalText			= InputText;
		final boolean FinalIsPassword	= IsPassword;
		final int FinalPosX				= (int)(posX);
		final int FinalPosY				= (int)(posY) - 5;
		final int FinalSizeX			= (int)(sizeX);
		final int FinalSizeY			= (int)(sizeY) + 10;
				
		handler.post(new Runnable()
        {
			public void run()
            {	    
				if( KeyboardTextLayout == null )
				{  	
					KeyboardTextLayout = new LinearLayout( activity );     				
					KeyboardTextLayout.setOrientation( LinearLayout.VERTICAL );
					ViewGroup viewGroup = (ViewGroup)KeyboardTextLayout;					
					KeyboardText = new EditText( activity );
					KeyboardText.setHeight(FinalSizeY - FinalPosY);
					viewGroup.addView(KeyboardText, 0);								
					
					int bottomPadding = surfaceHeight - FinalSizeY - 120;
					KeyboardTextLayout.setPadding(FinalPosX - 10, FinalPosY, 0, bottomPadding);
					addContentView( KeyboardTextLayout, new LayoutParams( FinalSizeX, LayoutParams.FILL_PARENT) );				    
					
					KeyboardText.requestFocus();
					
					InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
					imm.showSoftInput( KeyboardText, InputMethodManager.SHOW_FORCED );
				}
				
				bKeyboardOpen = true;
			}
		});
	}	
		
	////////////////////////////////////
	/// Function: 
	///    HideKeyBoard
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void HideKeyBoard( boolean wasCancelled )
	{
 		Logger.LogOut( "HideKeyBoard"); 		
 		
 		final UE3JavaApp activity = this;
		final boolean finalwasCancelled = wasCancelled;
				
		handler.post(new Runnable()
        {
			public void run()
            {	    
				if( KeyboardTextLayout != null )
				{         
					if( KeyboardText != null && !finalwasCancelled )
					{
						NativeCallback_KeyboardFinished( KeyboardText.getText().toString().trim() );
					}

					// in case its still up
					InputMethodManager mgr = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
					mgr.hideSoftInputFromWindow(KeyboardText.getWindowToken(), 0);

					ViewGroup viewGroup = (ViewGroup)(KeyboardTextLayout.getParent());
					viewGroup.removeView(KeyboardTextLayout);					
					KeyboardTextLayout = null;
					KeyboardText = null;
				}						

				bKeyboardOpen = false;
			}
		});
	}	
		
	////////////////////////////////////
	/// Function: 
	///    HideSplash
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void HideSplash()
	{
		if (bSplashScreenIsHidden)
		{
			return;
		}
		bSplashScreenIsHidden = true;

		handler.post(new Runnable()
        {
			public void run()
            {				
				if ( SplashScreen != null )
				{
					ViewGroup viewGroup = (ViewGroup)(SplashScreen.getParent());
					if( viewGroup != null )
					{
						viewGroup.removeView( SplashScreen );
					}
					SplashScreen = null;
				}	

				if ( IndefiniteLoadingBar != null )
				{
					ViewGroup viewGroup = (ViewGroup)(IndefiniteLoadingBar.getParent());
					if( viewGroup != null )
					{
						viewGroup.removeView( IndefiniteLoadingBar );
					}
					IndefiniteLoadingBar = null;
				}	
				
				if ( SplashLayout != null )
				{
					ViewGroup viewGroup = (ViewGroup)(SplashLayout.getParent());
					if( viewGroup != null )
					{
						viewGroup.removeView( SplashLayout );
					}
					SplashLayout = null;
				}			
			}				
		});
	}

	// Displays either spinning reloading circle or a loading movie from res/raw/loading.mp4
	private void ShowReloader()
	{
		final UE3JavaApp act = this; 

		// first look for a video file in the raw asset folder
		// play if found, else show generic spinner
		if( getResources().getIdentifier("loading", "raw", getPackageName()) != 0 ) 
		{
			if( LoadingMoviePrepareThread != null )
			{
				try
				{
					LoadingMoviePrepareThread.join();
				} 
				catch ( Exception e )
				{
					Logger.LogOut( "EXCEPTION - Couldnt join loading movie thread!!!" );			
				}	
			
				LoadingMoviePrepareThread = null;
			}

			final String uriPath = "android.resource://" + getPackageName() + "/raw/loading";
			final Uri uri = Uri.parse(uriPath);

			bIsLoadingMoviePlaying = true;

			if (videoLayout == null)
			{
				LayoutInflater inflater = getLayoutInflater(); 
				loadingVideoLayout = (RelativeLayout) inflater.inflate(R.layout.reloadermovie, null);					
				addContentView( loadingVideoLayout, new LayoutParams( LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT ) );
			}

			loadingVideoView = (SurfaceView) findViewById(R.id.reloader_movie_view);
			loadingVideoView.setZOrderMediaOverlay( true );
			SurfaceHolder loadingVideoHolder = loadingVideoView.getHolder();
			loadingVideoHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
			loadingVideoHolder.addCallback(new Callback()
			{	
				@Override
				public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height)
				{
					Logger.LogOut("ShowReloader.surfaceChanged called");
				}
				@Override
				public void surfaceDestroyed(SurfaceHolder surfaceHolder) 
				{
					Logger.LogOut("ShowReloader.surfaceDestroyed called");
				}
							
				@Override
				public void surfaceCreated(SurfaceHolder surfaceHolder)
				{
					if ( loadingVideoView != null && loadingVideoPlayer == null && surfaceHolder == loadingVideoView.getHolder() )
					{
						try
						{
							loadingVideoPlayer = new MediaPlayer();
							loadingVideoPlayer.setAudioStreamType( android.media.AudioManager.STREAM_MUSIC );
							loadingVideoPlayer.reset();
							loadingVideoPlayer.setLooping( true );

							// play from SDCard or APK
							if (loadingMovieFD == null)
							{
								loadingMovieFD = getResources().openRawResourceFd(getResources().getIdentifier("loading", "raw", getPackageName()));
							}
							loadingVideoPlayer.setDataSource( loadingMovieFD.getFileDescriptor(), loadingMovieFD.getStartOffset(), loadingMovieFD.getLength() );		
							loadingVideoPlayer.setDisplay( surfaceHolder );
								
							LoadingMoviePrepareThread = new Thread(new MoviePreparationThread( loadingVideoPlayer, act, loadingVideoView, loadingMovieFD.getFileDescriptor(), loadingMovieFD.getStartOffset(), loadingMovieFD.getLength() ), "MoviePrepareThread");
							LoadingMoviePrepareThread.start();								
						} 
						catch ( Exception e )
						{
							Logger.LogOut( "EXCEPTION - Couldn't start video: " + uriPath );
						}
					}
				}
			});
		}
		else
		{
			LayoutInflater inflater = act.getLayoutInflater(); 
			IndefiniteReloadingBar =  (RelativeLayout) inflater.inflate(R.layout.reloader, null);					
			addContentView( IndefiniteReloadingBar, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT) );  
		}
	}

	////////////////////////////////////
	/// Function: 
	///    HideReloader
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void HideReloader()
	{
		handler.post(new Runnable()
        {
			public void run()
            {				
				if ( IndefiniteReloadingBar != null )
				{
					ViewGroup viewGroup = (ViewGroup)(IndefiniteReloadingBar.getParent());
					if( viewGroup != null )
					{
						viewGroup.removeView( IndefiniteReloadingBar );
						IndefiniteReloadingBar = null;
					}
				}	
				
				bIsLoadingMoviePlaying = false;

				if ( loadingVideoPlayer != null )
				{
					loadingVideoPlayer.stop();
					loadingVideoPlayer.release();
					loadingVideoPlayer = null;
				}
				if ( loadingVideoView != null )
				{
					ViewGroup viewGroup = (ViewGroup)(loadingVideoView.getParent());
					viewGroup.removeView(loadingVideoView);
					loadingVideoView = null;
				}	
				if ( loadingVideoLayout != null)
				{
					ViewGroup viewGroup = (ViewGroup)(loadingVideoLayout.getParent());
					viewGroup.removeView(loadingVideoLayout);
					loadingVideoLayout = null;
				}

				if (loadingMovieFD != null)
				{
					try 
					{
						loadingMovieFD.close();
					}
					catch (IOException e) {}
					loadingMovieFD = null;
				}
			}				
		});
	}

	////////////////////////////////////
	/// Function: 
	///    SetFixedSizeScale
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void SetFixedSizeScale( float InScale )
	{
		final float FinalScale = InScale;
	
		GScreenScalePercent = InScale;

		handler.post(new Runnable()
        {
			public void run()
            {	
				SurfaceHolder CastGPUHolder = PrimaryGPUView.getHolder();
				Logger.LogOut( "JavaCallback_SetFixedSizeScale " + (int) ( PrimaryGPUView.getWidth() * FinalScale) + "x" + (int) ( PrimaryGPUView.getHeight() * FinalScale ));		
				CastGPUHolder.setFixedSize( (int) ( PrimaryGPUView.getWidth() * FinalScale), (int) ( PrimaryGPUView.getHeight() * FinalScale ) );	

				// force and update of the view
				ViewGroup viewGroup = (ViewGroup)(PrimaryGPUView.getParent());
				viewGroup.removeView(PrimaryGPUView);		
				setContentView( PrimaryGPUView );
			}				
		});		
	}
	
	////////////////////////////////////
	/// Function: 
	///    LoadSoundFile
	///    
	/// Specifiers: 
	///    [public] - 
	///    [int] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public int LoadSoundFile( String SoundPath )
	{			
		int LoadID = GSoundPool.load( SoundPath, 0 );
		if( LoadID <= 0 )
		{
			Logger.LogOut( "loadSoundFile(failed): " + SoundPath);
		}
		else
		{
			Logger.LogOut( "loadSoundFile(from storage): " + SoundPath);
		}

		return LoadID;
	}
		
	////////////////////////////////////
	/// Function: 
	///    UnloadSoundID
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void UnloadSoundID( int SoundID )
	{
		GSoundPool.unload( SoundID );
	}	
	
	////////////////////////////////////
	/// Function: 
	///    PlaySound
	///    
	/// Specifiers: 
	///    [public] - 
	///    [int] - 
	///    
	/// Parameters: 
	///    [int SoundID] - 
	///    [boolean Looping] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public int PlaySound( int SoundID, boolean Looping )
	{
		// start volume at zero? seems to prevent some issues
		return GSoundPool.play( SoundID, 0, 0, 0, Looping ? -1 : 0, 1 );
	}	
	
	////////////////////////////////////
	/// Function: 
	///    StopSound
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void StopSound( int StreamID )
	{
		GSoundPool.stop( StreamID );
	}	
		
	////////////////////////////////////
	/// Function: 
	///    SetVolume
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [int StreamID] - 
	///    [float Volume] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void SetVolume( int StreamID, float Volume )
	{
		GSoundPool.setVolume ( StreamID, Volume, Volume );
	}
			    
	////////////////////////////////////
	/// Function: 
	///    swapBuffers
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Public functions for EGL calls, available to the native code
	///    
	////////////////////////////////////
	public boolean swapBuffers()
    {	
        if ( eglSurface == null || !egl.eglSwapBuffers(eglDisplay, eglSurface))
        {
			// shutdown if swapbuffering goes down
			if( SwapBufferFailureCount > 10 )
			{
				Process.killProcess(Process.myPid());
			}
			SwapBufferFailureCount++;

			// basic reporting
			if( eglSurface == null )
			{
				Logger.LogOut("swapBuffers: eglSurface is NULL");
				return false;
			}
			else
			{
				Logger.LogOut("swapBuffers: eglSwapBuffers err: " + egl.eglGetError());

				if( egl.eglGetError() == EGL11.EGL_CONTEXT_LOST )
				{				
					Logger.LogOut("swapBuffers: EGL11.EGL_CONTEXT_LOST err: " + egl.eglGetError());					
					Process.killProcess(Process.myPid());
				}
			}

	        return false;
	    }
	    
	    return true;
    }	    
    
	////////////////////////////////////
	/// Function: 
	///    makeCurrent
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public boolean makeCurrent()
    {
		try
		{
			if (eglContext == null)
			{
				Logger.LogOut("eglContext is NULL");
				return false;
			}
			else if (eglSurface == null)
			{
				Logger.LogOut("eglSurface is NULL");
				return false;
			}
			else if (!egl.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
			{
				Logger.LogOut("eglMakeCurrent err: " + egl.eglGetError());

				// shut down the game on context lost
				if( egl.eglGetError() == EGL11.EGL_CONTEXT_LOST )
				{				
					Logger.LogOut("EGL11.EGL_CONTEXT_LOST err: " + egl.eglGetError());
					// kills the app forcefully for now
					Process.killProcess(Process.myPid());
				}

				return false;
			}		
		}
		catch ( Exception e )
		{		
			Logger.LogOut( "Failed makeCurrent with exception:" + e.getMessage());				
			e.printStackTrace();
			Process.killProcess(Process.myPid());
		}	        

		Logger.LogOut("eglMakeCurrent succeeded");
	    
	    return true;
    }
		
    
	////////////////////////////////////
	/// Function: 
	///    unMakeCurrent
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public boolean unMakeCurrent()
    {
		try
		{
			if (!egl.eglMakeCurrent(eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT))
			{
				Logger.LogOut("egl(Un)MakeCurrent err: " + egl.eglGetError());
				return false;
			}
		}
		catch ( Exception e )
		{		
			Logger.LogOut( "Failed unMakeCurrent with exception:" + e.getMessage());				
			e.printStackTrace();
			Process.killProcess(Process.myPid());
		}	

		Logger.LogOut("eglMakeCurrent null succeeded");
	    
	    return true;
    }
	  
	   
    public class EGLConfigParms
    {
		/** Whether this is a valid configuration or not */
		public int validConfig		= 0;
		/** Whether to request a multisample framebuffer */
		public int sampleBuffers	= 0;
		/** The number of bits requested for the red component */
		public int redSize     = 5;
		/** The number of bits requested for the green component */
		public int greenSize   = 6;
		/** The number of bits requested for the blue component */
		public int blueSize    = 5;
		/** The number of bits requested for the alpha component */
		public int alphaSize   = 0;
		/** The number of bits requested for the stencil component */
		public int stencilSize = 0;
		/** The number of bits requested for the depth component */
		public int depthSize   = 16;

		public EGLConfigParms()
		{
		}

		public EGLConfigParms(EGLConfigParms Parms)
		{
	        validConfig = Parms.validConfig;
	        sampleBuffers = Parms.sampleBuffers;
	        redSize = Parms.redSize;
	        greenSize = Parms.greenSize;
	        blueSize = Parms.blueSize;
	        alphaSize = Parms.alphaSize;
	        depthSize = Parms.depthSize;
	        stencilSize = Parms.stencilSize;
		}		
	}
		 
    
	////////////////////////////////////
	/// Function: 
	///    initEGL
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Called to initialize EGL. This function should not be called by the inheriting
    ///   activity, but can be overridden if needed.
	///    
	////////////////////////////////////
	public boolean initEGL(EGLConfigParms parms)
    {
		/** Attributes used when creating the context */
		int[] contextAttrs = null;

		eglAttemptedParams = new EGLConfigParms(parms);

        contextAttrs = new int[]
        {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL10.EGL_NONE
        };
		/** Attributes used when selecting the EGLConfig */
		int[] configAttrs = {
			EGL10.EGL_RED_SIZE, EGLMinRedBits,
			EGL10.EGL_GREEN_SIZE, EGLMinGreenBits,
			EGL10.EGL_BLUE_SIZE, EGLMinBlueBits,
			EGL10.EGL_ALPHA_SIZE, EGLMinAlphaBits,
			EGL10.EGL_STENCIL_SIZE, EGLMinStencilBits,
			EGL10.EGL_DEPTH_SIZE, EGLMinDepthBits,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL10.EGL_CONFIG_CAVEAT, EGL10.EGL_NONE,
			EGL10.EGL_NONE
		};

        egl = (EGL10) EGLContext.getEGL();
        egl.eglGetError();
        eglDisplay = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        Logger.LogOut("eglDisplay: " + eglDisplay + ", err: " + egl.eglGetError());
        int[] version = new int[2];
        boolean ret = egl.eglInitialize(eglDisplay, version);
        Logger.LogOut("EglInitialize returned: " + ret);
        if (!ret)
        {
            return false;
        }
        int eglErr = egl.eglGetError();
        if (eglErr != EGL10.EGL_SUCCESS)
            return false;
        Logger.LogOut("eglInitialize err: " + eglErr);

		// if we are running on a Kindle Fire, our desired framebuffer RGBA values are 5-6-5-0
		// This is to workaround an issue with some colors writing incorrectly to 32 bit buffers on KF
		String Model = ApplicationInformation.GetModel();
		if (Model.equals("Kindle Fire"))
		{
			parms.redSize = 5;
			parms.greenSize = 6;
			parms.blueSize = 5;
			parms.alphaSize = 0;
		}

        Logger.LogOut("Config info (" + parms.redSize
			+ ", " + parms.greenSize 
			+ ", " + parms.blueSize 
			+ ", " + parms.alphaSize 
			+ "), [" + parms.depthSize + ", "
			+ parms.stencilSize + "]");

        final EGLConfig[] config = new EGLConfig[20];
        int num_configs[] = new int[1];
        boolean chooseRet = egl.eglChooseConfig(eglDisplay, configAttrs, config, config.length, num_configs);
        Logger.LogOut("eglChooseConfig ret: " + chooseRet);
        Logger.LogOut("eglChooseConfig err: " + egl.eglGetError());
        Logger.LogOut("eglChooseConfig count: " + num_configs[0]);

		// no configs
		if (num_configs[0] == 0)
			return false;
		
		boolean haveConfig = false;
		
        int score = 1000000;

        int val[] = new int[1];
        for (int i = 0; i < num_configs[0]; i++)
        {
            int currScore = 0;
            int r, g, b, a, d, s;
            egl.eglGetConfigAttrib(eglDisplay, config[i], EGL10.EGL_RED_SIZE, val); r = val[0];
            egl.eglGetConfigAttrib(eglDisplay, config[i], EGL10.EGL_GREEN_SIZE, val); g = val[0];
            egl.eglGetConfigAttrib(eglDisplay, config[i], EGL10.EGL_BLUE_SIZE, val); b = val[0];
            egl.eglGetConfigAttrib(eglDisplay, config[i], EGL10.EGL_ALPHA_SIZE, val); a = val[0];
            egl.eglGetConfigAttrib(eglDisplay, config[i], EGL10.EGL_DEPTH_SIZE, val); d = val[0];
            egl.eglGetConfigAttrib(eglDisplay, config[i], EGL10.EGL_STENCIL_SIZE, val); s = val[0];

			// Optional, Tegra-specific non-linear depth buffer, which allows for much better
			// effective depth range in relatively limited bit-depths (e.g. 16-bit)
			int bNonLinearDepth = 0;
			if (egl.eglGetConfigAttrib(eglDisplay, config[i], EGL_DEPTH_ENCODING_NV, val))
			{
				bNonLinearDepth = (val[0] == EGL_DEPTH_ENCODING_NONLINEAR_NV) ? 1 : 0;
			}

            currScore = (Math.abs(r - parms.redSize) + Math.abs(g - parms.greenSize) + Math.abs(b - parms.blueSize) + Math.abs(a - parms.alphaSize)) << 24;
            currScore += Math.abs(1 - bNonLinearDepth)  << 23; 	// Prefer non-linear depth, if available
            currScore += Math.abs(d - parms.depthSize)  << 16;
            currScore += Math.abs(s - parms.stencilSize) << 8;
            if (currScore < score || !haveConfig)
            {
                Logger.LogOut("--------------------------");
				Logger.LogOut("New config chosen: " + i);

				Logger.LogOut("Config info (" + r + ", " + g + ", " + b + ", " + a + "), [" + d + "(" + bNonLinearDepth + "), " + s + "]");

                //for (int j = 0; j < (configAttrs.length-1)>>1; j++)
                //{
                //    egl.eglGetConfigAttrib(eglDisplay, config[i], configAttrs[j*2], val);
                //   // if (val[0] >= configAttrs[j*2+1])
                //   //     Logger.LogOut("setting " + j + ", matches: " + val[0]);
                //}

                eglConfig	= config[i];
				DepthSize = d;		// store depth/stencil sizes
                haveConfig	= true;
				score		= currScore;
            }
        }
        
        if (!haveConfig)
			return false;
			
        eglContext = egl.eglCreateContext(eglDisplay, eglConfig, EGL10.EGL_NO_CONTEXT, contextAttrs);
        Logger.LogOut("eglCreateContext: " + egl.eglGetError());

        gl = (GL11) eglContext.getGL();

        return true;
    }   
    
	////////////////////////////////////
	/// Function: 
	///    createEGLSurface
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Called to create the EGLSurface to be used for rendering. This function should not be called by the inheriting
    ///    activity, but can be overridden if needed.
	///    
	////////////////////////////////////
	public boolean createEGLSurface(SurfaceHolder surface)
    {
        try
		{
			eglSurface = egl.eglCreateWindowSurface(eglDisplay, eglConfig, surface, null);

			// On some Android devices, eglChooseConfigs will lie about valid configurations (specifically 32-bit color)
			if (egl.eglGetError() == EGL10.EGL_BAD_MATCH)
			{
				Logger.LogOut("eglCreateWindowSurface FAILED, retrying with more restricted context");
				
				// Dump what's already been initialized
				cleanupEGL();

				// Reduce target color down to 565
				eglAttemptedParams.redSize = 5;
				eglAttemptedParams.greenSize = 6;
				eglAttemptedParams.blueSize = 5;
				eglAttemptedParams.alphaSize = 0;
				initEGL(eglAttemptedParams);

				// try again
				eglSurface = egl.eglCreateWindowSurface(eglDisplay, eglConfig, surface, null);
			}
		}
		catch ( Exception e )
		{		
			Logger.LogOut( "Failed createEGLSurface " + e.getMessage());	
			e.printStackTrace();			
			Process.killProcess(Process.myPid());
		}	

        Logger.LogOut("eglSurface: " + eglSurface + ", err: " + egl.eglGetError());
        return (eglSurface != null);
    }

    /**
     * Destroys the EGLSurface used for rendering. This function should not be called by the inheriting
     * activity, but can be overridden if needed.
     */
    public void destroyEGLSurface()
    {
		Logger.LogOut( "Begin destroyEGLSurface" );			

		if( egl != null )
		{
			if (eglDisplay != null && eglSurface != null)
			{
				egl.eglMakeCurrent(eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, eglContext);
			}
			if (eglSurface != null)
			{
				egl.eglDestroySurface(eglDisplay, eglSurface);
			}
		}
        eglSurface = null;

		Logger.LogOut( "End destroyEGLSurface" );			
    }

    /**
     * Called to clean up egl. This function should not be called by the inheriting
     * activity, but can be overridden if needed.
     */
    public void cleanupEGL()
    {
		Logger.LogOut( "Full OpenGL ShutDown!!!" );	

        destroyEGLSurface();
        if (eglDisplay != null)
            egl.eglMakeCurrent(eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);
        if (eglContext != null)
            egl.eglDestroyContext(eglDisplay, eglContext);
        if (eglDisplay != null)
            egl.eglTerminate(eglDisplay);

        eglDisplay = null;
        eglContext = null;
        eglSurface = null;
    }
   	
	////////////////////////////////////////
	//SONGS
	////////////////////////////////////////		
	
	////////////////////////////////////
	/// Function: 
	///    StopSong
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void StopSong()
	{		
		Logger.LogOut( "StopSong" );

		handler.post(new Runnable()
        {
			public void run()
            {
				if( songPlayer != null )
				{				
					try
					{
						if( songPlayer.isPlaying () )
						{
							songPlayer.stop();
						}
					}
					catch ( Exception e )
					{
						Logger.LogOut( "EXCEPTION - song acting up: " + e.getMessage() );
					}	
					
					currentSongName = null;
					songPlayer.release();
					songPlayer = null;
				}	

				// kill any pending song as well
				if( pendingSongPlayer != null )
				{				
					try
					{
						if( pendingSongPlayer.isPlaying () )
						{
							pendingSongPlayer.stop();
						}
					}
					catch ( Exception e )
					{
						Logger.LogOut( "EXCEPTION - song acting up: " + e.getMessage() );
					}	
					
					pendingSongName = null;
					pendingSongPlayer.release();
					pendingSongPlayer = null;
				}	
			}
		});	
	}
	
	////////////////////////////////////
	/// Function: 
	///    PlaySong
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    
	///    
	////////////////////////////////////
	public void PlaySong( final FileDescriptor SongFD, final long SongOffset, final long SongLength, final String SongName )
	{
		// output the starting
		Logger.LogOut( "PlaySong: " + SongName);
	
		handler.post(new Runnable()
        {
			public void run()
            {	            
				try
				{	
					if (songPlayer != null)
					{
						if( songPlayer.isPlaying () )
						{
							// early out if the song is already playing or is already pending
							if ((SongName.equals(currentSongName) && pendingSongPlayer == null) || SongName.equals(pendingSongName))
							{
								return;
							}

							// else set the new track as pending so the current one fades out
							if (pendingSongPlayer != null)
							{
								pendingSongPlayer.release();
							}
							pendingSongPlayer = new MediaPlayer();

							// play from SDCard or APK
							pendingSongPlayer.setDataSource( SongFD, SongOffset, SongLength);
							pendingSongPlayer.prepare();
							pendingSongPlayer.setLooping(true);	
							pendingSongName = SongName;		
							return;
						}	
						else
						{
							songPlayer.release();
						}
					}
											
					songPlayer = new MediaPlayer();

					// play from SDCard or APK
					songPlayer.setDataSource( SongFD, SongOffset, SongLength);
					songPlayer.prepare();
					songPlayer.setLooping(true);			
					songPlayer.start();				
					currentSongVolume = 1.0f;

					currentSongName = SongName;
				} 
				catch ( Exception e )
				{
					Logger.LogOut( "EXCEPTION - Couldn't start song " );
				}				
			}
		});						
	}
	  
	// Handles fading out tracks 
	public void UpdateSongPlayer(final float DeltaTime)
	{
		handler.post(new Runnable()
        {
			public void run()
            {
				// Theres no pending song, so do nothing
				if (pendingSongPlayer == null)
				{
					return;
				}

				try
				{
					// Continue fading out current song if a new one is pending
					if (currentSongVolume > 0.0f)
					{
						currentSongVolume -= DeltaTime;
						songPlayer.setVolume(currentSongVolume, currentSongVolume);
					}
					else
					{
						if (songPlayer.isPlaying())
						{
							songPlayer.stop();
						}

						// else swap the players and reset the current volume
						songPlayer.release();
						songPlayer = pendingSongPlayer;
						currentSongName = pendingSongName;
						pendingSongPlayer = null;
						pendingSongName = null;
						songPlayer.start();
						currentSongVolume = 1.0f;	
					}
				}
				catch ( Exception e )
				{
					Logger.LogOut( "EXCEPTION - song acting up: " + e.getMessage() );
				}
			}
		});
	}

	////////////////////////////////////
	/// Function: 
	///    systemCleanup
	///    
	/// Specifiers: 
	///    [protected] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Called when the Activity is exiting and it is time to cleanup.
    ///    Kept separate from the {@link #cleanup()} function so that subclasses
    ///    in their simplest form do not need to call any of the parent class' functions. This to make
    ///    it easier for pure C/C++ application so that these do not need to call java functions from C/C++
	///    
	////////////////////////////////////
	protected void systemCleanup()
    {
		Logger.LogOut( "*=*=*=*= systemCleanup =*=*=*=*" );
		if (bRanInit)
		{
            NativeCallback_Cleanup();
		}
        if (!nativeEGL)
		{
	        cleanupEGL();
		}		
		Process.killProcess(Process.myPid());
    }

    static
    {
        System.loadLibrary("UnrealEngine3");
    }



	////////////////////////////////////
	/// Function: 
	///    IncrementDownloadsCounter
	///    
	/// Specifiers: 
	///    [public] - 
	///	   [static] - 
	///    [synchronized] -
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Increment the number of files we are downloading. Used to prevent starting the application
	///    multiple times when downloading multiple files.
	///    
	////////////////////////////////////
    public static synchronized void IncrementDownloadsCounter() 
    {
    	++numDownloadsRunning;
    }


	////////////////////////////////////
	/// Function: 
	///    DecrementDownloadsCounter
	///    
	/// Specifiers: 
	///    [private] - 
	///	   [static] - 
	///    [synchronized] -
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Decrement the number of files we are downloading. See IncrementDownloadsCounter for details
	///    
	////////////////////////////////////
    private static synchronized void DecrementDownloadsCounter()
    {
    	numDownloadsRunning--;
    }

	  
	////////////////////////////////////
	/// Function: 
	///    ExpansionFilesDelivered
	///    
	/// Specifiers: 
	///    [private] - 
	///    [boolean] - returns true if all files in xAPK match a file in storage, or if there are no files in xAPKS
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Checks if the apk expansion files exist on disk
	///    
	////////////////////////////////////
    private boolean ExpansionFilesDelivered() 
    {
    	if ( xAPKS == null )
    		return true;

        for (XAPKFile xf : xAPKS) 
        {
            String fileName = Helpers.getExpansionAPKFileName(this, xf.IsMain, xf.FileVersion);
            if (!Helpers.doesFileExist(this, fileName, xf.FileSize, false))
                return false;
        }

        return true;
    }


	////////////////////////////////////
	/// Function: 
	///    SetButtonPausedState
	///    
	/// Specifiers: 
	///    [private] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [boolean] - the paused state to set: true for pause, false for unpause
	///    
	/// Description: 
	///    Sets the paused state for button to provide visual feedback to the user
	///    
	////////////////////////////////////
    private void SetButtonPausedState(boolean paused) 
    {
        isDownloadPaused = paused;
        int stringResourceID = paused ? R.string.Text_Button_Resume : R.string.Text_Button_Pause;
        pauseButton.setText(stringResourceID);
    }

    ////////////////////////////////////
	/// Function: 
	///    DeleteOldExpansionFiles
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Deletes all files except the current main and patch expansion files
	///    
	////////////////////////////////////
	private void DeleteOldExpansionFiles()
	{
		try
		{
			int fileVersion = 0;
			final int versionCode = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
			fileVersion = versionCode;

			String fileNameMain = Helpers.getExpansionAPKFileName(this, true, fileVersion); //get the expansion file name based on the build version of the app.
			String fileNamePatch = Helpers.getExpansionAPKFileName(this, false, fileVersion);

			File newFile = new File(Helpers.generateSaveFileName(this, fileNameMain));
			File[] listFiles = newFile.getParentFile().listFiles();

			for ( File file:listFiles )
			{
			    String name = file.getName();
			    if ( name.startsWith(fileNameMain) || name.startsWith(fileNamePatch) )
			      	continue;

			    file.delete();
			}
		}
		catch (NameNotFoundException e)
		{
			Logger.ReportException("", e);
		}
	}

    ////////////////////////////////////
	/// Function: 
	///    IsThereSpaceForExpansionFiles
	///    
	/// Specifiers: 
	///    [public] - 
	///    [boolean] - returns true if there is enough bytes for the expansion files, or if there are no files that can be read
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Checks if there is enough space for the expansion files
	///    
	////////////////////////////////////    
	public boolean IsThereSpaceForExpansionFiles()
    {
    	if ( xAPKS == null )
    		return true;

	   	long bytesNeeded = 0;
		for ( XAPKFile apkFile : xAPKS )
		{
			 bytesNeeded += apkFile.FileSize;
		}

		String pathToExpansionFiles = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/obb";

		if ( bytesNeeded > Helpers.getAvailableBytes(new File(pathToExpansionFiles)) )
		{
			Logger.LogOut("Out of space, user needs to delete something");
			return false;
		}

		return true;
    }


    ////////////////////////////////////
	/// Function: 
	///    InitializeDownloadUI
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Initializes the view elements used in the download UI, such as the views, buttons, download progress bar, etc.
	///    
	////////////////////////////////////
    public void InitializeDownloadUI()
    {
    	Logger.LogOut("Initialize download UI");
	    downloaderClientStub = DownloaderClientMarshaller.CreateStub(this, UE3JavaFileDownloader.class);
		setContentView(R.layout.main);

        progressBar = (ProgressBar) findViewById(R.id.progressBar);
		statusText = (TextView) findViewById(R.id.statusText);
        progressFraction = (TextView) findViewById(R.id.progressAsFraction);
        progressPercent = (TextView) findViewById(R.id.progressAsPercentage);
        averageSpeed = (TextView) findViewById(R.id.progressAverageSpeed);
        timeRemaining = (TextView) findViewById(R.id.progressTimeRemaining);
        dashboard = findViewById(R.id.downloaderDashboard);
        cellMessage = findViewById(R.id.approveCellular);
        pauseButton = (Button) findViewById(R.id.pauseButton);
        wifiSettingsButton = (Button) findViewById(R.id.wifiSettingsButton);


        pauseButton.setOnClickListener(new View.OnClickListener() 
        {
            @Override
            public void onClick(View view) 
            {
                if (isDownloadPaused) 
                {
                    remoteService.requestContinueDownload();
                } else {
                    remoteService.requestPauseDownload();
                }
                SetButtonPausedState(!isDownloadPaused);
            }
        });

        wifiSettingsButton.setOnClickListener(new View.OnClickListener() 
        {
            @Override
            public void onClick(View v) 
            {
                startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
            }
        });

        Button resumeOnCell = (Button) findViewById(R.id.resumeOverCellular);
        resumeOnCell.setOnClickListener(new View.OnClickListener() 
        {
            @Override
            public void onClick(View view) 
            {
                remoteService.setDownloadFlags(IDownloaderService.FLAGS_DOWNLOAD_OVER_CELLULAR);
                remoteService.requestContinueDownload();
                cellMessage.setVisibility(View.GONE);
            }
        }); 
    }


    ////////////////////////////////////
	/// Function: 
	///    InitializeForceExitMessage
	///    
	/// Specifiers: 
	///    [public] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [int] - id of the message to display
	///    
	/// Description: 
	///    Initializes a message and button to inform the user why the application is forcing
	///	   him/her to exit 
	///    
	////////////////////////////////////
    public void InitializeForceExitMessage(int msgId)
    {
    	if ( dashboard != null )
    		dashboard.setVisibility(View.GONE);

	    exitLayout = findViewById(R.id.exitLayout);
	    exitMessage = (TextView) findViewById(R.id.exitMessage);
	    exitMessage.setText( msgId );
        exitButton = (Button) findViewById(R.id.exitButton);
        exitLayout.setVisibility(View.VISIBLE);


    	exitButton.setOnClickListener(new View.OnClickListener()
        {
        	@Override
        	public void onClick(View v)
        	{
        		systemCleanup();
        	}
        });
    }

    ////////////////////////////////////
	/// Function: 
	///    PopulateXAPKs
	///    
	/// Specifiers: 
	///    [private] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Populates the XAPK values based on information pulled from the google play server
	///    
	////////////////////////////////////
	private void PopulateXAPKs()
	{
		DownloadsDB db = DownloadsDB.getDB( getApplicationContext() );
	    int lastVersionCode = db.getLastCheckedVersionCode();

	    if ( lastVersionCode != -1 ) 
	    {
	        DownloadInfo[] infos = db.getDownloads();
	        if (null != infos) 
	        {
	        	xAPKS = new XAPKFile[ infos.length ];
	           	for ( int i = 0; i < infos.length; ++i )
	            {
	            	XAPKFile newAPK = new XAPKFile();

	            	newAPK.FileSize = infos[i].mTotalBytes;

	            	String fileName = infos[i].mFileName;
	            	if ( fileName.startsWith("main") )
	            	{
	            		newAPK.IsMain = true;
	            		fileName = fileName.substring(5);
	            	}
	            	else if ( fileName.startsWith("patch") )
	            	{
	            		newAPK.IsMain = false;
	            		fileName = fileName.substring(6);
	            	}

	            	int dotIndex = fileName.indexOf('.');
					newAPK.FileVersion = Integer.parseInt( fileName.substring(0, dotIndex) );

					xAPKS[i] = newAPK;
					Logger.LogOut("newest apk: " + newAPK.IsMain + "." + newAPK.FileVersion);
				}
			}
			db.close();	
		}
	}


    ////////////////////////////////////
	/// Function: 
	///    DeleteAllExpansionFiles
	///    
	/// Specifiers: 
	///    [private] - 
	///    [void] - 
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Deletes all expansion files INCLUDING the current main and patch expansion files
	///    
	////////////////////////////////////
	private void DeleteAllExpansionFiles()
	{
		try
		{
			int fileVersion = 0;
			final int versionCode = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
			fileVersion = versionCode;

			String fileNameMain = Helpers.getExpansionAPKFileName(this, true, fileVersion); //get the expansion file name based on the build version of the app.

			File newFile = new File(Helpers.generateSaveFileName(this, fileNameMain));
			File[] listFiles = newFile.getParentFile().listFiles();

			for ( File file:listFiles )
			{
			    file.delete();
			}
		}
		catch (NameNotFoundException e)
		{
			Logger.ReportException("", e);
		}
	}

	private void OpenSettingsMenu()
	{
		InitializeSettingsPreferences();

		Intent settingsActivity = new Intent(getApplicationContext(), UE3JavaPreferences.class);
	    startActivity(settingsActivity);		
	}



    ////////////////////////////////////
	/// Function: 
	///    GetPerformanceLevel
	///    
	/// Specifiers: 
	///    [private] - 
	///    [int] - The user set performance level to use
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Accesses the Shared Preferences to get the user set performance level (or -1 if no preference set)
	///    
	////////////////////////////////////
	private int GetPerformanceLevel()
	{
		if ( mPrefs == null )
		{
			mPrefs = getApplicationContext().getSharedPreferences(getPackageName(), MODE_PRIVATE);
		}

		// returning -1 will cause the native side to determine what performance settings to use
		return mPrefs.getInt( lastWorkingPerformanceKey, -1 );
	}


    ////////////////////////////////////
	/// Function: 
	///    GetResolutionScale
	///    
	/// Specifiers: 
	///    [private] - 
	///    [float] - The user set resolution scale to use
	///    
	/// Parameters: 
	///    [void] - 
	///    
	/// Description: 
	///    Helper function to access shared preferences and return the user set resolution scale
	///    
	////////////////////////////////////
	private float GetResolutionScale()
	{
		if ( mPrefs == null )
		{
			mPrefs = getApplicationContext().getSharedPreferences(getPackageName(), MODE_PRIVATE);	
		}

		return mPrefs.getFloat( lastWorkingResScaleKey, -1 );
	}

	// Initialize the preference values that the settings menu uses, if they aren't already
	private void InitializeSettingsPreferences()
	{
		// if the last working performance key hasn't been set, it will be -1
		if ( mPrefs.getInt(currentPerformanceKey, -1) < 0 ||
			 mPrefs.getFloat(currentResScaleKey, -1) < 0 )
		{
			int PerformanceValue = NativeCallback_GetPerformanceLevel();
			float ResolutionScaleValue = NativeCallback_GetResolutionScale();

			// Map resolution Scale to the discrete Values available
			if (ResolutionScaleValue < 0.6f)
			{
				ResolutionScaleValue = 0.5f;
			}
			else if (ResolutionScaleValue < 0.8f)
			{
				ResolutionScaleValue = 0.75f;
			}
			else
			{
				ResolutionScaleValue = 1.0f;
			}

			SharedPreferences.Editor editor = mPrefs.edit();
			
			editor.putInt( currentPerformanceKey, PerformanceValue );
			editor.putFloat( currentResScaleKey, ResolutionScaleValue );

			// check in case our last working values are valid. not directly affected by settings, but in case
			// of crashing on exit we want to make sure some values exist
			if ( mPrefs.getInt(lastWorkingPerformanceKey, -1) < 0 ||
				 mPrefs.getFloat(lastWorkingResScaleKey, -1) < 0 )
			{
				editor.putInt( lastWorkingPerformanceKey, PerformanceValue );
				editor.putFloat( lastWorkingResScaleKey, ResolutionScaleValue );	
			}

			editor.commit();
		}
	}

	// builds the strings for the main and patch expansion files
	// pass in 0 if the file doesn't exist
	private void BuildExpansionFilePaths(int MainVersion, int PatchVersion)
	{
		if ( MainVersion > 0 && MainExpansionFilePath == "" )
		{
			String PackageName = getApplicationContext().getPackageName();
			String StorageDirectory = Environment.getExternalStorageDirectory().getAbsolutePath()  + "/Android/obb/" + PackageName + "/";

			// if the assets are in the apk they just at the root of the asssets
			if (bIsExpansionInAPK)
			{
				StorageDirectory = "";
				PatchVersion = MainVersion; // will match since combined apk will always upload together
			}
			else if (!NativeCallback_IsShippingBuild() || skipDownloader)
			{
				// If not shipping build revise path to UnrealEngine3 sync directory
				StorageDirectory = Environment.getExternalStorageDirectory().getAbsolutePath()  + "/UnrealEngine3/obb/" + PackageName + "/";
			}

			MainExpansionFilePath = StorageDirectory + "main." + MainVersion + "." + PackageName + ".obb";
			// @hack .png extension is used on Amazon data file to prevent it from being compressed in the APK
			MainExpansionFilePath += bIsExpansionInAPK ? ".png" : "";

			if ( PatchVersion > 0 && PatchExpansionFilePath == "" )
			{
				PatchExpansionFilePath = StorageDirectory + "patch." + PatchVersion + "." + PackageName + ".obb";
				PatchExpansionFilePath += bIsExpansionInAPK ? ".png" : "";
			}
		}

		Logger.LogOut("Main Expansion Path set to: " + MainExpansionFilePath);
		Logger.LogOut("Patch Expansion Path set to: " + PatchExpansionFilePath);
	}

	// Check if any expansion expansion files exist. If so, use their path for the path strings
	private boolean CheckForExistingExpansionFiles()
	{
		String PackageName = getApplicationContext().getPackageName();
		String StorageDirectory = Environment.getExternalStorageDirectory().getAbsolutePath() + "/Android/obb/" + PackageName;

		boolean ExpansionFound = false;

		File ExpansionPathFile = new File(StorageDirectory);
		if ( ExpansionPathFile.exists() )
		{
			File[] ExpansionFiles = ExpansionPathFile.listFiles();
			
			for ( int i = 0; i < ExpansionFiles.length; ++i )
			{
				String FileName = ExpansionFiles[i].getName();
				if ( FileName.startsWith("main.") )
				{
					MainExpansionFilePath = ExpansionFiles[i].getPath();
					ExpansionFound = true;
					continue;
				}

				if ( FileName.startsWith("patch.") )
				{
					PatchExpansionFilePath = ExpansionFiles[i].getPath();
					ExpansionFound = true;
				}
			}
		}
		return ExpansionFound;
	}
}

// Separate class for managing a unique id for the game
class Installation 
{
    private static String sID = null;
    private static final String INSTALLATION = "INSTALLATION";

    public synchronized static String id(Context context) 
    {
        if (sID == null) 
        {  
            File installation = new File(context.getFilesDir(), INSTALLATION);
            try 
            {
                if (!installation.exists())
                {
                    writeInstallationFile(installation);
                }
                sID = readInstallationFile(installation);
            } 
            catch (Exception e) 
            {
                throw new RuntimeException(e);
            }
        }
        return sID;
    }

    private static String readInstallationFile(File installation) throws IOException 
    {
        RandomAccessFile f = new RandomAccessFile(installation, "r");
        byte[] bytes = new byte[(int) f.length()];
        f.readFully(bytes);
        f.close();
        return new String(bytes);
    }

    private static void writeInstallationFile(File installation) throws IOException 
    {
        FileOutputStream out = new FileOutputStream(installation);
        String id = UUID.randomUUID().toString();
        out.write(id.getBytes());
        out.close();
    }
}
