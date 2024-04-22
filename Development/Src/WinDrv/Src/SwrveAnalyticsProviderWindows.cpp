/*=============================================================================
	SwrveAnalyticsWindows.cpp: Windows-specific support for SwrveAnalytics API
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "WinDrv.h"
#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#include "HttpImplementationWinInet.h"

#if WITH_UE3_NETWORKING

IMPLEMENT_CLASS(USwrveAnalyticsWindows);

class FSwrveTickerWindows : public FTickableObject
{
	TArray<FHttpResponseWinInet*> ResponseQueue;
public:
	FSwrveTickerWindows()
	{
	}

	/** Checks each response to see if it's ready, then handles delegate calling, etc. */
	virtual void Tick(FLOAT DeltaSeconds)
	{
		for (int i=0;i<ResponseQueue.Num();)
		{
			FHttpResponseWinInet* Response = ResponseQueue(i);
			if (Response->IsReady())
			{
				debugf/*suppressed*/(NAME_DevStats,TEXT("Swrve response for %s. Code: %d. Payload: %s"), *Response->GetURL().GetURL(), Response->GetResponseCode(), *Response->GetPayloadAsString());
				delete Response;
				ResponseQueue.RemoveSwap(i);
			}
			else
			{
				++i;
			}
		}
	}

	/** Conditional tick. */
	virtual UBOOL IsTickable() const
	{
		return ResponseQueue.Num() > 0;
	}

	/** make sure this is always ticking, even while game is paused */
	virtual UBOOL IsTickableWhenPaused() const
	{
		return TRUE;
	}

	void AddResponse(FHttpResponseWinInet* Response)
	{
		if (Response != NULL)
		{
			ResponseQueue.AddItem(Response);
		}
	}
};

static FSwrveTickerWindows GSwrveTicker;

/**
 * Gets the session token from the Swrve singleton.
 */
FString USwrveAnalyticsWindows::GetSessionToken()
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

/** Sends a request to SWRVE (helper func). */
void USwrveAnalyticsWindows::SendToSwrve(const FString& MethodName, const FString& OptionalParams, const FString& Payload)
{
	debugf(NAME_DevStats, TEXT("Swrve Method: %s. Params: %s. Payload:\n%s"), *MethodName, *OptionalParams, *Payload);

	GSwrveTicker.AddResponse(FHttpRequestWinInet().SetHeader(TEXT("Content-Type"), Payload.IsEmpty() ? TEXT("text/plain") : TEXT("application/x-www-form-urlencoded; charset=utf-8")).MakeRequest(FURLWinInet(
		FString::Printf(TEXT("%s%s?session_token=%s&app_version=%d%s%s"),
		*APIServer, *MethodName, *GetSessionToken(), GEngineVersion, OptionalParams.IsEmpty() ? TEXT("") : TEXT("&"), *OptionalParams)
		), Payload.IsEmpty() ? TEXT("GET") : TEXT("POST"), Payload));
}

/**
 * Perform any initialization. Called once after singleton instantiation.
 * Overrides default Swrve analytics behavior if config options are specified
 */
void USwrveAnalyticsWindows::Init()
{	
	Super::Init();

	SessionStartTime = 0;

	// Detect release packaging
	// @todo - find some way to specify this
	UBOOL bIsRelease = FALSE;
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
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows missing %s. No uploads will be processed."), GameIDStr);
	}
	const TCHAR* GameAPIKeyStr = 
		bIsRelease
			? TEXT("GameAPIKeyRelease")
			: bTestAnalytics || bDebugAnalytics
				? TEXT("GameAPIKeyTest") 
				: TEXT("GameAPIKeyDev");
	if (!GConfig->GetString(TEXT("SwrveAnalytics"), GameAPIKeyStr, GameAPIKey, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows missing %s. No uploads will be processed."), GameAPIKeyStr);
	}
	const TCHAR* APIServerStr = 
		bDebugAnalytics
			? TEXT("APIServerDebug") 
			: TEXT("APIServer");
	if (!GConfig->GetString(TEXT("SwrveAnalytics"), APIServerStr, APIServer, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows missing %s. No uploads will be processed."), APIServerStr);
	}
	else
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows.%s = %s."), APIServerStr,*APIServer);
	}
	const TCHAR* ABTestServerStr = TEXT("ABTestServer");
	if (!GConfig->GetString(TEXT("SwrveAnalytics"), ABTestServerStr, ABTestServer, GEngineIni))
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows missing %s. No uploads will be processed."), ABTestServerStr);
	}

	FString UserId;
	if (Parse(appCmdLine(), TEXT("ANALYTICSUSERID="), UserId, FALSE))
	{
		SetUserId(UserId);
	}
}

/** Called when the analytics class is being destroyed (typically at program end) */
void USwrveAnalyticsWindows::BeginDestroy()
{
	Super::BeginDestroy();
}

void USwrveAnalyticsWindows::SetUserId(const FString& NewUserId)
{
	// command-line specified user ID overrides all attempts to reset it.
	FString UserId;
	if (Parse(appCmdLine(), TEXT("ANALYTICSUSERID="), UserId, FALSE))
	{
		Super::SetUserId(UserId);
	}
	else
	{
		Super::SetUserId(NewUserId);
	}
}

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
void USwrveAnalyticsWindows::StartSession()
{
	if (bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows::StartSession called again. Ignoring."));
		return;
	}

	if (UserId.IsEmpty())
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows::StartSession called without a valid UserId. Ignoring."));
		return;
	}

	UAnalyticEventsBase::StartSession();

	// record the session start time.
	time_t TimeVal = time(NULL);
	SessionStartTime = (INT)TimeVal;

	CachedSessionToken = TEXT("");

	debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows::StartSession [%s]"),*GameAPIKey);

	SendToSwrve(TEXT("1/session_start"));

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
void USwrveAnalyticsWindows::EndSession()
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows::EndSession called without BeginSession. Ignoring."));
		return;
	}

	SendToSwrve(TEXT("1/session_end"));

	UAnalyticEventsBase::EndSession();

	debugf(NAME_DevStats,TEXT("USwrveAnalyticsWindows::EndSession"));
}

/** Helper to log any swrve event. Used by all the LogXXX functions. */
void USwrveAnalyticsWindows::SwrveLogEvent(const FString& EventName, const TArray<FEventStringParam>& ParamArray)
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogEvent called outside of session."));
		return;
	}

	// encode params as JSON
	FString EventParams = TEXT("swrve_payload=");
	if (ParamArray.Num() > 0)
	{
		EventParams += TEXT("{");
		for (int Ndx=0;Ndx<ParamArray.Num();++Ndx)
		{
			if (Ndx > 0)
			{
				EventParams += TEXT(",");
			}
			EventParams += FString(TEXT("\"")) + ParamArray(Ndx).ParamName + TEXT("\": \"") + ParamArray(Ndx).ParamValue + TEXT("\"");
			/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveLogEvent %s[Attr%d]:'%s'='%s'"), *EventName,Ndx,*ParamArray(Ndx).ParamName,*ParamArray(Ndx).ParamValue);
		}
		EventParams += TEXT("}");
	}
	else
	{
		/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveLogEvent %s"), *EventName);
	}
	SendToSwrve(TEXT("1/event"), FString::Printf(TEXT("name=%s"), *EventName), EventParams);
}

/**
 * Adds a named event to the session
 *
 * @param EventName unique string for named event
 * @param bTimed if true then event is logged with timing
 */
void USwrveAnalyticsWindows::LogStringEvent(const FString& EventName,UBOOL bTimed)
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
void USwrveAnalyticsWindows::LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed)
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
void USwrveAnalyticsWindows::LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed)
{
	SwrveLogEvent(EventName, ParamArray);
}

/**
 * Adds a named error event with corresponding error message
 *
 * @param ErrorName unique string for error event 
 * @param ErrorMessage message detailing the error encountered
 */
void USwrveAnalyticsWindows::LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage)
{
	TArray<FEventStringParam> ParamArray;
	ParamArray.AddItem(FEventStringParam(TEXT("ErrorMessage"), ErrorMessage));
	SwrveLogEvent(ErrorName, ParamArray);
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsWindows::LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue)
{
	TArray<FEventStringParam> Param;
	Param.AddItem(FEventStringParam(AttributeName, AttributeValue));
	LogUserAttributeUpdateArray(Param);
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsWindows::LogUserAttributeUpdateArray(const TArray<FEventStringParam>& ParamArray)
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogEvent called outside of session."));
		return;
	}
	if (ParamArray.Num() == 0)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogUserAttributeUpdateArray called with no attributes to update."));
		return;
	}

	FString EventParams = TEXT("");
	for (INT Ndx=0;Ndx<ParamArray.Num();++Ndx)
	{
		EventParams += FString(TEXT("&")) + ParamArray(Ndx).ParamName + FString(TEXT("=")) + ParamArray(Ndx).ParamValue;
		/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveUserAttribute[%d]:'%s'='%s'"), Ndx,*ParamArray(Ndx).ParamName,*ParamArray(Ndx).ParamValue);
	}

	SendToSwrve(TEXT("1/user"), EventParams, FString());
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsWindows::LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogEvent called outside of session."));
		return;
	}

	/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveItemPurchase: ItemId='%s', Currency='%s', PerItemCost='%d', ItemQuantity='%d'."), *ItemId, *Currency, PerItemCost, ItemQuantity);

	SendToSwrve(TEXT("1/purchase"), FString::Printf(TEXT("item=%s&cost=%d&quantity=%d&currency=%s"), 
		*ItemId, PerItemCost, ItemQuantity, *Currency)); 
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsWindows::LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogEvent called outside of session."));
		return;
	}

	/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveCurrencyPurchase: GameCurrencyType='%s', GameCurrencyAmount='%d', RealCurrencyType='%s', RealMoneyCost='%f', PaymentProvider='%s'."), *GameCurrencyType, GameCurrencyAmount, *RealCurrencyType, RealMoneyCost, *PaymentProvider);

	SendToSwrve(TEXT("1/buy_in"), FString::Printf(TEXT("cost=%.2f&local_currency=%s&payment_provider=%s&reward_amount=%d&reward_currency=%s"), 
		RealMoneyCost, *RealCurrencyType, *PaymentProvider, GameCurrencyAmount, *GameCurrencyType)); 
}

/** See documentation in UAnalyticEventsBase.uc */
void USwrveAnalyticsWindows::LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	if (!bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("SwrveLogEvent called outside of session."));
		return;
	}

	/*debugfSuppressed*/debugf(NAME_DevStats,TEXT("SwrveCurrencyGiven: GameCurrencyType='%s', GameCurrencyAmount='%d'."), *GameCurrencyType, GameCurrencyAmount);

	SendToSwrve(TEXT("1/currency_given"), FString::Printf(TEXT("given_currency=%s&given_amount=%d"), 
		*GameCurrencyType, GameCurrencyAmount)); 
}

#endif //WITH_UE3_NETWORKING
