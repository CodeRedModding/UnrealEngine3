/*=============================================================================
	SwrveAnalyticsIPhone.mm: IPhone specific support for SwrveAnalytics API
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"

#if WITH_SWRVE
#import "Swrve.h"
#import "IPhoneObjCWrapper.h"
#import "NSStringExtensions.h"
#import <sys/sysctl.h>
#import <UIKit/UIDevice.h>

Swrve* GSwrveSingleton;

/**
 * StoreKit subclass of generic MicroTrans API
 */
class USwrveAnalyticsIPhone : public UAnalyticEventsBase
{
	DECLARE_CLASS_INTRINSIC(USwrveAnalyticsIPhone, UAnalyticEventsBase, 0, IPhoneDrv)
public:
	// UAnalyticEventsBase interface
	virtual void Init();
	virtual void BeginDestroy();
	virtual void SetUserId(const FString& NewUserId);
	virtual void StartSession();
    virtual void EndSession();
    virtual void LogStringEvent(const FString& EventName,UBOOL bTimed);
    virtual void LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed);
    virtual void LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed);
    virtual void LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage);
	virtual void LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue);
	virtual void LogUserAttributeUpdateArray(const TArray<FEventStringParam>& ParamArray);
	virtual void LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity);
	virtual void LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider);
	virtual void LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount);
	virtual void SendCachedEvents();

private:
	/** code to generate a Swrve-compatible session token. Not needed currently but left here if we need it in the future. */
	FString GetSessionToken();
	/** Swrve Game ID Number - Get from your account manager */
	INT GameID;
	/** Swrve Game API Key - Get from your account manager */
	FString GameAPIKey;
	/** Swrve API Server - should be http://api.swrve.com/ */
	FString APIServer;
	/** Swrve A/B test Server - should be http://abtest.swrve.com/ */
	FString ABTestServer;
	/** Time when the session began (to generate session token) */
	INT SessionStartTime;
	/** Cached session token so we don't regenerate every time. */
	FString CachedSessionToken;
};

/** Saves any pending swrve events to disk if necessary. Can be called when the application goes to the background. */
void FlushSwrveEventsToDiskIfNecessary()
{
	if (GSwrveSingleton != NULL)
	{
		swrve_save_events_to_disk(GSwrveSingleton);
	}
}

/** Class that manages checking if the responses are ready and calling the delegate on them. */
class FSwrveTicker : FTickableObject
{
	DOUBLE NextFlushTime;
	DOUBLE TimeBetweenFlushesSec;
	FString UserId;
public:
	FSwrveTicker()
	{
		TimeBetweenFlushesSec = 15.0;
		// don't initialize here because GCurrentTime doesn't have the huge offset applied to it yet. Wait until Tick() is called.
		NextFlushTime = 0.0;
	}

	/** Checks each response to see if it's ready, then handles delegate calling, etc. */
	virtual void Tick(FLOAT DeltaSeconds)
	{
		// initialize and force an initial flush (in case any events were left over last run)
		if (NextFlushTime == 0.0)
		{
			NextFlushTime = GCurrentTime;
		}

		if (GCurrentTime >= NextFlushTime)
		{
			// see if we have valid values to send the event with.
			if (GSwrveSingleton != NULL && !UserId.IsEmpty())
			{
				debugf(NAME_DevStats, TEXT("Flushing any pending swrve events."));
				swrve_send_queued_events(GSwrveSingleton, (CFStringRef)[NSString stringWithFString:UserId]);
				// Comment this out as we're using RunLoops now. If we weren't we'd need to use the below code to schedule this on the main thread's run loop.
				//// capture the NSString so we don't try to reference the UObject instance in the block below
				//NSString* UserIdNS = [NSString stringWithFString:UserId];
				//// must dispatch this call because the API assumes a run loop in the current thread but we don't have one.
				//dispatch_async(dispatch_get_main_queue(), ^{ swrve_send_queued_events(GSwrveSingleton, (CFStringRef)UserIdNS); });
			}
			else
			{
				debugf(NAME_DevStats, TEXT("Tried to flush Swrve events, but Swrve Singletone is NULL (%p) or UserId is empty (%s). Skipping this flush."), GSwrveSingleton, *UserId);
			}
			// always set the next flush time.
			// Don't just increment by the time interval, as we could have gone to sleep and be thousands of flushes behind!
			NextFlushTime = GCurrentTime + TimeBetweenFlushesSec;
		}
	}

	/** Conditional tick. */
	virtual UBOOL IsTickable() const
	{
		return GSwrveSingleton != NULL;
	}

	/** make sure this is always ticking, even while game is paused */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	/** set the event flush interval to send to Swrve web service */
	void SetFlushInterval(DOUBLE TimeSec)
	{
		// clamp to 0.1 sec.
		if (TimeSec < 0.1)
		{
			warnf(TEXT("Swrve flush interval cannot be less than 0.1. Clamping %f."), (FLOAT)TimeSec);
		}
		TimeSec = Max(0.1, TimeSec);
		TimeBetweenFlushesSec = TimeSec;
	}

	/** Set the unique user ID for Swrve. */
	void SetUserId(const FString& InUserId)
	{
		UserId = InUserId;
	}
};

// Global instance of the HttpTicker for iPhone.
static FSwrveTicker GSwrveTicker;


IMPLEMENT_CLASS(USwrveAnalyticsIPhone);
void AutoInitializeRegistrantsSwrveAnalyticsIPhone( INT& Lookup )
{
	USwrveAnalyticsIPhone::StaticClass();
}

/**
 * Perform any initialization. Called once after singleton instantiation.
 * Overrides default Swrve analytics behavior if config options are specified
 */
void USwrveAnalyticsIPhone::Init()
{	
	Super::Init();

	SessionStartTime = 0;

	// Detect release packaging
	UBOOL bIsRelease = IPhoneIsPackagedForDistribution();
	UBOOL bTestAnalytics = ParseParam(appCmdLine(), TEXT("TESTANALYTICS"));
	UBOOL bDebugAnalytics = ParseParam(appCmdLine(), TEXT("DEBUGANALYTICS"));

	// Allow one ApiKey for development, and one for production
	const TCHAR* GameIDStr = 
		bIsRelease
			? TEXT("GameIDRelease")
			: bTestAnalytics || bDebugAnalytics
				? TEXT("GameIDTest") 
				: TEXT("GameIDDev");
	if (!GConfig->GetInt(TEXT("SwrveAnalytics"), GameIDStr, GameID, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone missing %s. No uploads will be processed."), GameIDStr);
	}
	const TCHAR* GameAPIKeyStr = 
		bIsRelease
			? TEXT("GameAPIKeyRelease")
			: bTestAnalytics || bDebugAnalytics 
				? TEXT("GameAPIKeyTest") 
				: TEXT("GameAPIKeyDev");
	if (!GConfig->GetString(TEXT("SwrveAnalytics"), GameAPIKeyStr, GameAPIKey, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone missing %s. No uploads will be processed."), GameAPIKeyStr);
	}
	const TCHAR* APIServerStr = 
		bDebugAnalytics
			? TEXT("APIServerDebug") 
			: TEXT("APIServer");
	if (!GConfig->GetString(TEXT("SwrveAnalytics"), APIServerStr, APIServer, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone missing %s. No uploads will be processed."), APIServerStr);
	}
	else
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone.%s = %s."), APIServerStr,*APIServer);
	}
	const TCHAR* ABTestServerStr = TEXT("ABTestServer");
	if (!GConfig->GetString(TEXT("SwrveAnalytics"), ABTestServerStr, ABTestServer, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone missing %s. No uploads will be processed."), ABTestServerStr);
	}

	// Set the flush interval with the global ticker
	FLOAT EventFlushIntervalSec = 0.0f;
	if (GConfig->GetFloat(TEXT("SwrveAnalytics"), TEXT("EventFlushIntervalSec"), EventFlushIntervalSec, GEngineIni))
	{
		GSwrveTicker.SetFlushInterval((DOUBLE)EventFlushIntervalSec);
	}

	FString UserId;
	if (Parse(appCmdLine(), TEXT("ANALYTICSUSERID="), UserId, FALSE))
	{
		SetUserId(UserId);
	}

	NSString* GameAPIKeyNSStr = [NSString stringWithFString:GameAPIKey];
	NSString* APIServerNSStr = [NSString stringWithFString:APIServer];
	NSString* ABTestServerNSStr = [NSString stringWithFString:ABTestServer];

	// These paths are arbitrary, but swrve must be able to write to them.
	// Swrve recommends placing them in the Caches folder.
	NSString *LibraryPath = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
	NSString *EventCacheFile = [LibraryPath stringByAppendingPathComponent:@"SwrveEventCache.txt"];
	NSString *ABTestCacheFile = [LibraryPath stringByAppendingPathComponent:@"SwrveABTestCache.txt"];
	NSString *InstallTimeCacheFile = [LibraryPath stringByAppendingPathComponent:@"SwrveInstallTimeCache.txt"];

	GSwrveSingleton = swrve_init(
		kCFAllocatorDefault, 
		GameID, 
		(CFStringRef)GameAPIKeyNSStr, 
		(CFStringRef)APIServerNSStr, 
		(CFStringRef)ABTestServerNSStr, 
		(CFStringRef)EventCacheFile, 
		(CFStringRef)ABTestCacheFile, 
		(CFStringRef)InstallTimeCacheFile);

	checkf(GSwrveSingleton != NULL, TEXT("call to swrve_init failed"));
}

/** Called when the analytics class is being destroyed (typically at program end) */
void USwrveAnalyticsIPhone::BeginDestroy()
{
	Super::BeginDestroy();
	if (GSwrveSingleton != NULL)
	{
		swrve_close(GSwrveSingleton);
		GSwrveSingleton = NULL;
	}
}

void USwrveAnalyticsIPhone::SetUserId(const FString& NewUserId)
{
	// command-line specified user ID overrides all attempts to reset it.
	FString UserId;
	if (Parse(appCmdLine(), TEXT("ANALYTICSUSERID="), UserId, FALSE))
	{
		Super::SetUserId(UserId);
		GSwrveTicker.SetUserId(UserId);
	}
	else
	{
		Super::SetUserId(NewUserId);
		GSwrveTicker.SetUserId(NewUserId);
	}
}

extern int CheckHeader();
extern NSString* ScanForDLC();

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
void USwrveAnalyticsIPhone::StartSession()
{
	if (bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone::StartSession called again. Ignoring."));
		return;
	}

	UAnalyticEventsBase::StartSession();

	debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone::StartSession [%s]"),*GameAPIKey);

	swrve_session_start(GSwrveSingleton);

	// record the session start time.
	timeval time;
	gettimeofday(&time, NULL);
	SessionStartTime = (INT)time.tv_sec;

	CachedSessionToken = TEXT("");

	//@todo send these as user attributes
	// get the device hardware type
	//size_t hwtype_size;
	//sysctlbyname("hw.machine", NULL, &hwtype_size, NULL, 0);
	//char* hwtype = (char*)malloc(hwtype_size+1);
	//hwtype[hwtype_size] = '\0';
	//sysctlbyname("hw.machine", hwtype, &hwtype_size, NULL, 0);

	//[Swrve event:@"EngineData" withArgs:[NSDictionary dictionaryWithObjectsAndKeys:
	//	[NSString stringWithFormat:@"%d", CheckHeader()], @"DevBit", 
	//	[[UIDevice currentDevice] model], @"DeviceModel", 
	//	[NSString stringWithFormat:@"%s",hwtype], @"DeviceType", 
	//	[[UIDevice currentDevice] systemVersion], @"OSVersion", 
	//	[NSString stringWithFormat:@"%d", IPhoneLoadUserSettingU64("IPhoneHome::NumCrashes")], @"NumCrashes",
	//	[NSString stringWithFormat:@"%d", IPhoneLoadUserSettingU64("IPhoneHome::NumMemoryWarnings")], @"NumMemoryWarnings",
	//	[[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicPackagingMode"], @"PackageMode", 
	//	ScanForDLC(), @"PackageHash", 
	//	[NSString stringWithFormat:@"%d", GEngineVersion], @"EngineVersion", 
	//	[[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicAppVersion"], @"AppVersion", 
	//	nil]];

	//extern int CheckJailbreakBySandbox();
	//extern int CheckJailbreakBySysctl();

	//// Send jailbreak data to our analytics provider
	//[Swrve event:@"EngineData2" withArgs:[NSDictionary dictionaryWithObjectsAndKeys:
	//	[NSString stringWithFormat:@"%d", CheckJailbreakBySandbox()], @"Cf", 
	//	[NSString stringWithFormat:@"%d", CheckJailbreakBySysctl()], @"Cp", 
	//	nil]];

	//free(hwtype);
}

/**
 * End capturing stats and queue the upload 
 */
void USwrveAnalyticsIPhone::EndSession()
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone::EndSession called without BeginSession. Ignoring."));
		return;
	}
	UAnalyticEventsBase::EndSession();

	swrve_session_end(GSwrveSingleton);

	debugf(NAME_DevStats,TEXT("USwrveAnalyticsIPhone::EndSession"));
}

/**
 * Gets the session token from the Swrve singleton.
 */
FString USwrveAnalyticsIPhone::GetSessionToken()
{
	// early out if there is no session.
	if (!bSessionInProgress)
	{
		return FString();
	}

	// return the cache if possible
	if (!CachedSessionToken.IsEmpty())
	{
		return CachedSessionToken;
	}

	// validate required information.
	if (GameID == 0 || GameAPIKey.IsEmpty() || UserId.IsEmpty())
	{
		debugf(NAME_DevStats,TEXT("Cannot generate a Swrve session token because the GameID is zero (%d), the GameAPIKey is empty (%s), or the UserId is empty (%s)."), GameID, *GameAPIKey, *UserId);
		return FString();
	}
	extern FString MD5HashAnsiString(const TCHAR*);

	FString SessionHash = MD5HashAnsiString(*FString::Printf(TEXT("%s%d%s"), *UserId, SessionStartTime, *GameAPIKey));
	CachedSessionToken = FString::Printf(TEXT("%d=%s=%d=%s"), GameID, *UserId, SessionStartTime, *SessionHash);

	return CachedSessionToken;
}

/** Helper to log any swrve event. Used by all the LogXXX functions. */
void SwrveLogEvent(const FString& EventName, const TArray<FEventStringParam>& ParamArray)
{
	if (GSwrveSingleton == NULL)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogEvent called when Swrve singleton has not been initialized."));
		return;
	}

	NSString* EventNameNS = [NSString stringWithFString:EventName];
	// encode params as JSON
	FString EventParams = TEXT("");
	if (ParamArray.Num() > 0)
	{
		EventParams += TEXT("{");
		for (int Ndx=0;Ndx<ParamArray.Num();++Ndx)
		{
			if (Ndx > 0)
			{
				EventParams += TEXT(",");
			}
			EventParams += FString(TEXT("\"")) + ParamArray(Ndx).ParamName + TEXT("\":\"") + ParamArray(Ndx).ParamValue + TEXT("\"");
			/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveLogEvent %s[Attr%d]:'%s'='%s'"), *EventName,Ndx,*ParamArray(Ndx).ParamName,*ParamArray(Ndx).ParamValue);
		}
		EventParams += TEXT("}");
	}
	else
	{
		/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveLogEvent %s"), *EventName);
	}
	NSString* EventParamsNS = [NSString stringWithFString:EventParams];
	if (swrve_event(GSwrveSingleton, (CFStringRef)EventNameNS, (CFStringRef)EventParamsNS) != SWRVE_SUCCESS)
	{
		debugf(NAME_DevStats, TEXT("SwrveLogEvent: Failed to send event with name: %s"), *EventName);
	}
}

/**
 * Adds a named event to the session
 *
 * @param EventName unique string for named event
 * @param bTimed if true then event is logged with timing
 */
void USwrveAnalyticsIPhone::LogStringEvent(const FString& EventName,UBOOL bTimed)
{
	SwrveLogEvent(EventName, TArray<FEventStringParam>());
}

/**
 * Adds a named event to the session with a single parameter/value
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 * @param bTimed if true then event is logged with timing
 */
void USwrveAnalyticsIPhone::LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed)
{ 
	TArray<FEventStringParam> ParamArray;
	ParamArray.AddItem(FEventStringParam(ParamName, ParamValue));
	SwrveLogEvent(EventName, ParamArray);
}

/**
 * Adds a named event to the session with an array of parameter/values
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs
 * @param bTimed if true then event is logged with timing
 */
void USwrveAnalyticsIPhone::LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed)
{
	SwrveLogEvent(EventName, ParamArray);
}

/**
 * Adds a named error event with corresponding error message
 *
 * @param ErrorName unique string for error event 
 * @param ErrorMessage message detailing the error encountered
 */
void USwrveAnalyticsIPhone::LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage)
{
	TArray<FEventStringParam> ParamArray;
	ParamArray.AddItem(FEventStringParam(TEXT("ErrorMessage"), ErrorMessage));
	SwrveLogEvent(ErrorName, ParamArray);
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsIPhone::LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue)
{
	TArray<FEventStringParam> Param;
	Param.AddItem(FEventStringParam(AttributeName, AttributeValue));
	LogUserAttributeUpdateArray(Param);
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsIPhone::LogUserAttributeUpdateArray(const TArray<FEventStringParam>& ParamArray)
{
	if (GSwrveSingleton == NULL)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogUserAttributeUpdateArray called when Swrve singleton has not been initialized."));
		return;
	}
	if (ParamArray.Num() == 0)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogUserAttributeUpdateArray called with no attributes to update."));
		return;
	}

	// encode params as JSON
	FString EventParams = TEXT("{");
	for (int Ndx=0;Ndx<ParamArray.Num();++Ndx)
	{
		if (Ndx > 0)
		{
			EventParams += TEXT(",");
		}
		EventParams += FString(TEXT("\"")) + ParamArray(Ndx).ParamName + TEXT("\":\"") + ParamArray(Ndx).ParamValue + TEXT("\"");
		/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveUserAttribute[%d]:'%s'='%s'"), Ndx,*ParamArray(Ndx).ParamName,*ParamArray(Ndx).ParamValue);
	}
	EventParams += TEXT("}");

	NSString* EventParamsNS = [NSString stringWithFString:EventParams];
	if (swrve_user_update(GSwrveSingleton, (CFStringRef)EventParamsNS) != SWRVE_SUCCESS)
	{
		debugf(NAME_DevStats, TEXT("SwrveLogUserAttributeUpdateArray: Failed to update user attributes."));
	}
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsIPhone::LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	if (GSwrveSingleton == NULL)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogItemPurchaseEvent called when Swrve singleton has not been initialized."));
		return;
	}

	/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveItemPurchase: ItemId='%s', Currency='%s', PerItemCost='%d', ItemQuantity='%d'."), *ItemId, *Currency, PerItemCost, ItemQuantity);

	if (swrve_purchase(GSwrveSingleton, 
		(CFStringRef)[NSString stringWithFString:ItemId], 
		(CFStringRef)[NSString stringWithFString:Currency], 
		PerItemCost, 
		ItemQuantity) != SWRVE_SUCCESS)
	{
		debugf(NAME_DevStats, TEXT("SwrveLogItemPurchaseEvent: Failed to record purchase."));
	}
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsIPhone::LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	if (GSwrveSingleton == NULL)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogCurrencyPurchaseEvent called when Swrve singleton has not been initialized."));
		return;
	}

	/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveCurrencyPurchase: GameCurrencyType='%s', GameCurrencyAmount='%d', RealCurrencyType='%s', RealMoneyCost='%f', PaymentProvider='%s'."), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider);

	if (swrve_buy_in(GSwrveSingleton, 
		(CFStringRef)[NSString stringWithFString:GameCurrencyType], 
		GameCurrencyAmount,
		(DOUBLE)RealMoneyCost,
		(CFStringRef)[NSString stringWithFString:RealCurrencyType]) != SWRVE_SUCCESS)
	{
		debugf(NAME_DevStats, TEXT("SwrveLogCurrencyPurchaseEvent: Failed to record purchase."));
	}
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsIPhone::LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	if (GSwrveSingleton == NULL)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogCurrencyGivenEvent called when Swrve singleton has not been initialized."));
		return;
	}

	/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveCurrencyGiven: GameCurrencyType='%s', GameCurrencyAmount='%d'."), *GameCurrencyType, GameCurrencyAmount);

	if (swrve_currency_given(GSwrveSingleton, 
		(CFStringRef)[NSString stringWithFString:GameCurrencyType], 
		(DOUBLE)GameCurrencyAmount) != SWRVE_SUCCESS)
	{
		debugf(NAME_DevStats, TEXT("SwrveLogCurrencyGivenEvent: Failed to record purchase."));
	}
}

void USwrveAnalyticsIPhone::SendCachedEvents()
{
	if (GSwrveSingleton == NULL)
	{
		debugf(NAME_DevStats,TEXT("SwrveSendCachedEvents called when Swrve singleton has not been initialized."));
		return;
	}

	debugf(NAME_DevStats, TEXT("Flushing any pending swrve events by request."));
	swrve_send_queued_events(GSwrveSingleton, (CFStringRef)[NSString stringWithFString:UserId]);
}

#endif //WITH_SWRVE