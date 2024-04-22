/*=============================================================================
	HardwareSurvey.cpp: Hardware Survey posting functionality.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnIpDrv.h"
#if WITH_MANAGED_CODE
	// Have to include editor file here because there is no EngineCLR project.
	#include "HardwareSurveyShared.h"
#endif
#include "HTTPDownload.h"

/** The thread that will runn the UDK hardware survey if necessary. */
FRunnableThread* GUDKHardwareSurveyThread = NULL;

/** Indicates if the UDK hardware survey thread is running. Done in a global so these internal classes don't have to be exposed. */
volatile INT GUDKHardwareSurveyThreadRunning = 0;

volatile INT GUDKHardwareSurveySucceeded = 0;

#if UDK && WITH_UE3_NETWORKING && _WINDOWS && WITH_MANAGED_CODE

// URL that surveys are sent to.
// Internally, we send it to "et.epicgames.com". If you set up your own service, define it's URL here.
#if !defined(PHONE_HOME_URL)
#define PHONE_HOME_URL TEXT("tempuri.org")
#endif

/** Contains options for performing the UDK hardware survey */
struct FUDKHardwareSurveyOptions
{
public:
	/** Initialize basic types */
	FUDKHardwareSurveyOptions()
		: bHardwareSurveyUseCompression(FALSE)
	{}

	/** The URL of the survey server. */
	FString HardwareSurveyAddress;

	/** The timeout of the upload in seconds. */
	FLOAT HardwareSurveyTimeout;

	/** Whether to compress the survey data. */
	UBOOL bHardwareSurveyUseCompression;
};

/** Runnable that will execute the UDK hardware survey in a child thread. */
class FUDKHardwareSurveyer : public FRunnable
{
	/** Options to control the survey execution. */
	FUDKHardwareSurveyOptions Options;

	/** HTTP Poster that does the upload. */
	FHttpDownloadString* HttpPoster;

	/** Set when the thread needs to terminate early */
	volatile INT bShouldQuit;

	/** Survey start time (for timing) */
	DOUBLE SurveyStartTime;

public:

	/** ctor. Passed config options for how to run the survey. */
	FUDKHardwareSurveyer(const FUDKHardwareSurveyOptions& InOptions)
		: Options(InOptions)
		, HttpPoster(NULL)
		, bShouldQuit(0)
		, SurveyStartTime(0.0)
	{
	}

	/**
	* This thread-able runnable performs the hardware survey collection 
	* and posts to a configurable web service provider.
	* 
	* Collection requires managed code support.
	*
	* @return True if initialization was successful, false otherwise
	*/
	virtual UBOOL Init(void)
	{
		SurveyStartTime = appSeconds();

		// mark the thread as running
		appInterlockedIncrement(&GUDKHardwareSurveyThreadRunning);

		// Make the managed call.
		TArray<BYTE> UncompressedBuffer;
		PerformHardwareSurveyCLR(UncompressedBuffer);

		BOOL Result = FALSE;

		// Build an url from the string
		FURL Url(NULL,*Options.HardwareSurveyAddress,TRAVEL_Absolute);
		FResolveInfo* ResolveInfo = NULL;
		// See if we need to resolve this string
		UBOOL bIsValidIp = FInternetIpAddr::IsValidIp(*Url.Host);
		if (bIsValidIp == FALSE)
		{
			// Allocate a platform specific resolver and pass that in
			ResolveInfo = GSocketSubsystem->GetHostByName(TCHAR_TO_ANSI(*Url.Host));
		}
		// Create a new posting object
		HttpPoster = new FHttpDownloadString(FALSE,
			Options.HardwareSurveyTimeout,
			FString::Printf(TEXT("UDKVer=%d"), GEngineVersion),
			ResolveInfo,
			HRT_Post);

		const INT UNCOMPRESSED_HEADER_SIZE = 4;
		const INT COMPRESSED_HEADER_SIZE = 8;

		// Determine whether to send as text or compressed
		if (Options.bHardwareSurveyUseCompression)
		{
			TArray<BYTE> CompressedBuffer;
			const INT UncompressedBufferSize = UncompressedBuffer.Num();
			INT CompressedBufferSize = UncompressedBufferSize + COMPRESSED_HEADER_SIZE;
			// Now size the buffer leaving space for the header
			CompressedBuffer.Empty(CompressedBufferSize);
			CompressedBuffer.Add(CompressedBufferSize);
			// Now compress the buffer into our destination skipping the header
			verify(appCompressMemory(ECompressionFlags(COMPRESS_ZLIB | COMPRESS_BiasSpeed),
				&CompressedBuffer(COMPRESSED_HEADER_SIZE),
				CompressedBufferSize,
				(void*)UncompressedBuffer.GetTypedData(),
				UncompressedBufferSize));
			// Finally, write the header information into the buffer
			CompressedBuffer(0) = 0x4D;	// M
			CompressedBuffer(1) = 0x43;	// C
			CompressedBuffer(2) = 0x50; // P
			CompressedBuffer(3) = 0x01; // Compressed flag
			CompressedBuffer(4) = (UncompressedBufferSize & 0xFF000000) >> 24;
			CompressedBuffer(5) = (UncompressedBufferSize & 0x00FF0000) >> 16;
			CompressedBuffer(6) = (UncompressedBufferSize & 0x0000FF00) >> 8;
			CompressedBuffer(7) = UncompressedBufferSize & 0xFF;
			// Copy the payload into the object for sending
			HttpPoster->CopyPayload(CompressedBuffer.GetTypedData(),CompressedBufferSize + COMPRESSED_HEADER_SIZE);
		}
		else
		{
			TArray<BYTE> FinalBuffer;
			const INT UncompressedBufferSize = UncompressedBuffer.Num();
			INT FinalBufferSize = UncompressedBufferSize + UNCOMPRESSED_HEADER_SIZE;
			// Now size the buffer leaving space for the header
			FinalBuffer.Empty(FinalBufferSize);
			FinalBuffer.Add(FinalBufferSize);
			FinalBuffer(0) = 0x4D;	// M
			FinalBuffer(1) = 0x43;	// C
			FinalBuffer(2) = 0x50;  // P
			FinalBuffer(3) = 0x00;
			appMemcpy(&FinalBuffer(4), &UncompressedBuffer(0), UncompressedBufferSize);
			HttpPoster->CopyPayload(FinalBuffer.GetTypedData(),FinalBufferSize);
		}

		// Start the download task
		HttpPoster->DownloadUrl(Url);
		return TRUE;
	}

	/**
	* Keep ticking the HTTP request until it finishes, errors, or times out.
	*
	* @return The exit code of the runnable object
	*/
	virtual DWORD Run(void)
	{
		DOUBLE LastTime = appSeconds();
		while (!bShouldQuit)
		{
			DOUBLE CurTime = appSeconds();

			HttpPoster->Tick((FLOAT)(CurTime - LastTime));
			EHTTPState PosterState = HttpPoster->GetHttpState();
			if (PosterState == HTTP_Error)
			{
				debugf(TEXT("Failed to upload UDK hardware survey to %s in %.2f seconds."), *Options.HardwareSurveyAddress, appSeconds()-SurveyStartTime);
				break;
			}
			if (PosterState == HTTP_Closed)
			{
				appInterlockedIncrement(&GUDKHardwareSurveySucceeded);
				debugf(TEXT("Successfully upload UDK hardware survey to %s in %.2f seconds."), *Options.HardwareSurveyAddress, appSeconds()-SurveyStartTime);
				break;
			}
			appSleep(0.25f);
			LastTime = CurTime;
		}
		if (bShouldQuit)
		{
			// return an error code if we are requested to terminate early
			debugf(TEXT("UDK hardware survey thread requested to shut down early. Quitting."));
		}
		// signal that the thread finished.
		appInterlockedDecrement(&GUDKHardwareSurveyThreadRunning);
		// return success if the HTTP state is closed.
		return HttpPoster->GetHttpState() == HTTP_Closed ? 0 : 1;
	}

	/**
	* This is called if a thread is requested to terminate early
	*/
	virtual void Stop(void)
	{
		appInterlockedIncrement(&bShouldQuit);
	}

	/**
	* Called in the context of the aggregating thread to perform any cleanup.
	*/
	virtual void Exit(void)
	{
		delete HttpPoster;
		HttpPoster = NULL;
	}

	/**
	* Destructor
	*/
	virtual ~FUDKHardwareSurveyer()
	{
	}
};

/** 
  * Does the actual hardware survey and upload. 
  * Retrieves the web URL to hit, the timeout, and compression flags from config.
  * Spawns a thread in "fire and forget" mode to perform the survey. It should
  * not crash the game if the survey upload fails.
  */
static UBOOL UploadHardwareSurvey()
{
	FUDKHardwareSurveyOptions Options;

	// Hardcode the configuration for sending the upload.
	Options.HardwareSurveyAddress = FString::Printf(TEXT("http://%s/PostUDKSurveyHandler.ashx"), PHONE_HOME_URL);
	Options.HardwareSurveyTimeout = 10.0f;
	Options.bHardwareSurveyUseCompression = TRUE;

	// Create the thread to do the work and let it go. We don't care if it ultimately fails.
	GUDKHardwareSurveyThread = GThreadFactory->CreateThread(new FUDKHardwareSurveyer(Options), TEXT("UDKHardwareSurveyer"), FALSE, TRUE);

	return TRUE;
}

/** 
  * @return the current day of the year to pseudo-human readable form (unique day since 0 AD as INT).
  */
static UINT GetCurrentUniqueDayOfYear()
{
	INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	appUtcTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
	return (UINT)Year * 10000 + (UINT)Month * 100 + (UINT)Day;
}

/**
* Upload hardware survey the first time the program is run or when updated.
* Checks config/opt-out clauses, etc to determine if it needs to do it, else simply returns.
*/
void UploadHardwareSurveyIfNecessary()
{
	// only upload the hardware survey once per day, measured in UTC.
	INT HardwareSurveyVersion = -1;
	UINT HardwareSurveyDate = 0;
	UINT CurrentUniqueDayOfYear = GetCurrentUniqueDayOfYear();

	// perform the survey if we haven't done it before, or we did it for a different engine version.
	const TCHAR* HardwareSurveySectionStr = TEXT("HardwareSurvey");
	const TCHAR* LastSurveyVersionStr = TEXT("LastSurveyVersion");
	const TCHAR* LastSurveyDateStr = TEXT("LastSurveyDate");

	// dump the survey if that's all that is requested.
	if (ParseParam( appCmdLine(), TEXT("DUMPUDKSURVEY") ))
	{
		PerformHardwareSurveyDumpCLR();
	}
	
	if (
		!GConfig->GetInt( HardwareSurveySectionStr, LastSurveyVersionStr, HardwareSurveyVersion, GEngineIni ) || HardwareSurveyVersion != GEngineVersion ||
		!GConfig->GetInt( HardwareSurveySectionStr, LastSurveyDateStr, (INT&)HardwareSurveyDate, GEngineIni ) || HardwareSurveyDate < CurrentUniqueDayOfYear
		)
	{
		debugf(TEXT("Last hardware survey: Ver=%d, Date=%d. Uploading again."), HardwareSurveyVersion, HardwareSurveyDate);

		//perform the actual upload
		if (UploadHardwareSurvey())
		{
			// if successful, set the ini indicating that the survey has been done for this engine version.
			GConfig->SetInt( HardwareSurveySectionStr, LastSurveyVersionStr, GEngineVersion, GEngineIni );
			GConfig->SetInt( HardwareSurveySectionStr, LastSurveyDateStr, CurrentUniqueDayOfYear, GEngineIni );
			GConfig->Flush( FALSE, GEngineIni );
		}
	}
}


#else

/** Don't do hardware survey if we are not UDK. */
void UploadHardwareSurveyIfNecessary()
{
}

#endif
