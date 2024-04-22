/*=============================================================================
	ApsalarAnalyticsIPhone.mm: IPhone specific support for Apsalar Analytics API
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"

#if WITH_APSALAR
#import "Apsalar.h"
#import "IPhoneObjCWrapper.h"
#include <sys/sysctl.h>

#if 0
	#define debugfStatsSlow debugf
#else
	#define debugfStatsSlow debugfSuppressed
#endif

void WarnIfEventNameIsTooLong(const FString& EventName)
{
	if (EventName.Len() > 32)
	{
		warnf(TEXT("Apsalar event name is too long: %s and will be truncated by Apsalar. 32 character max limit."), *EventName);
	}
}

/**
 * StoreKit subclass of generic MicroTrans API
 */
class UApsalarAnalyticsIPhone : public UAnalyticEventsBase
{
	DECLARE_CLASS_INTRINSIC(UApsalarAnalyticsIPhone, UAnalyticEventsBase, 0, IPhoneDrv)

	// UAnalyticEventsBase interface
	virtual void Init();
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

	/** Maximum buffer to allocate for apsalar uploads */
	INT MaxBufferSize;
	/** Key assigned to the current app */
	FString ApiKey;
	/** Key assigned to the current app */
	FString ApiSecret;
	
	/** tracks if the session has already been started once (call reStartSession if so). */
	UBOOL bHasBeenStarted;
};

IMPLEMENT_CLASS(UApsalarAnalyticsIPhone);
void AutoInitializeRegistrantsApsalarAnalyticsIPhone( INT& Lookup )
{
	UApsalarAnalyticsIPhone::StaticClass();
}

/**
 * Perform any initialization. Called once after singleton instantiation.
 * Overrides default Apsalar analytics behavior if config options are specified
 */
void UApsalarAnalyticsIPhone::Init()
{	
	bHasBeenStarted = FALSE;
	if (GConfig->GetInt(TEXT("IPhoneDrv.ApsalarAnalyticsIPhone"), TEXT("MaxBufferSize"), MaxBufferSize, GEngineIni) &&
		MaxBufferSize > 0)
	{
		[Apsalar setBufferLimit:MaxBufferSize];
	}
	// Detect release packaging
	UBOOL bIsRelease = IPhoneIsPackagedForDistribution();

	DEFINE_ANALYTICS_CONFIG;

	FAnalyticsConfig* Config = AnalyticsConfig.Find(GGameName);
	// Don't let UDKGame set the config value via hard-coding on iOS, so licensees can still configure their own.
	// Android Epic Citadel uses UDK Game, so we have to do this.
	if (Config && appStricmp(GGameName, TEXT("UDK")) != 0 )
	{
		// Allow one ApiKey for development, and one for production
		ApiKey = bIsRelease ? Config->ApsalarAPIKeyRelease : Config->ApsalarAPIKeyDev;
		ApiSecret = bIsRelease ? Config->ApsalarSecretRelease : Config->ApsalarSecretDev;
	}
	else
	{
		// Allow one ApiKey for development, and one for production
		const TCHAR* ApiKeyStr = bIsRelease ? TEXT("ApiKeyRelease") : TEXT("ApiKeyDev");
		if (!GConfig->GetString(TEXT("IPhoneDrv.ApsalarAnalyticsIPhone"), ApiKeyStr, ApiKey, GEngineIni))
		{
			debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone missing %s. No uploads will be processed."), ApiKeyStr);
		}
		const TCHAR* ApiSecretStr = bIsRelease ? TEXT("ApiSecretRelease") : TEXT("ApiSecretDev");
		if (!GConfig->GetString(TEXT("IPhoneDrv.ApsalarAnalyticsIPhone"), ApiSecretStr, ApiSecret, GEngineIni))
		{
			debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone missing %s. No uploads will be processed."), ApiSecretStr);
		}
	}
	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::Init called, APIKey [%s]"), *ApiKey);
}

extern int CheckHeader();
extern NSString* ScanForDLC();

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
void UApsalarAnalyticsIPhone::StartSession()
{
	if (bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::StartSession called again. Ignoring."));
		return;
	}

	UAnalyticEventsBase::StartSession();

	NSString* ApiKeyStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ApiKey) encoding:NSUTF8StringEncoding];
	NSString* ApiSecretStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ApiSecret) encoding:NSUTF8StringEncoding];

	if (!bHasBeenStarted)
	{
		bHasBeenStarted = TRUE;
	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::StartSession [%s]"),*ApiKey);

	[Apsalar startSession:ApiKeyStr withKey:ApiSecretStr];

	// get the device hardware type
	size_t hwtype_size;
	sysctlbyname("hw.machine", NULL, &hwtype_size, NULL, 0);
	char* hwtype = (char*)malloc(hwtype_size+1);
	hwtype[hwtype_size] = '\0';
	sysctlbyname("hw.machine", hwtype, &hwtype_size, NULL, 0);

	[Apsalar event:@"EngineData" withArgs:[NSDictionary dictionaryWithObjectsAndKeys:
		[NSString stringWithFormat:@"%d", CheckHeader()], @"DevBit", 
		[[UIDevice currentDevice] model], @"DeviceModel", 
		[NSString stringWithFormat:@"%s",hwtype], @"DeviceType", 
		[[UIDevice currentDevice] systemVersion], @"OSVersion", 
		[NSString stringWithFormat:@"%d", IPhoneLoadUserSettingU64("IPhoneHome::NumCrashes")], @"NumCrashes",
		[NSString stringWithFormat:@"%d", IPhoneLoadUserSettingU64("IPhoneHome::NumMemoryWarnings")], @"NumMemoryWarnings",
		[[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicPackagingMode"], @"PackageMode", 
		ScanForDLC(), @"PackageHash", 
		[NSString stringWithFormat:@"%d", GEngineVersion], @"EngineVersion", 
		[[[NSBundle mainBundle] infoDictionary] objectForKey: @"EpicAppVersion"], @"AppVersion", 
		nil]];

		extern int CheckJailbreakBySandbox();
		extern int CheckJailbreakBySysctl();

		// Send jailbreak data to our analytics provider
		[Apsalar event:@"EngineData2" withArgs:[NSDictionary dictionaryWithObjectsAndKeys:
			[NSString stringWithFormat:@"%d", CheckJailbreakBySandbox()], @"Cf", 
			[NSString stringWithFormat:@"%d", CheckJailbreakBySysctl()], @"Cp", 
			nil]];

	free(hwtype);
}
	else
	{
		debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::RestartSession [%s]"),*ApiKey);
		[Apsalar reStartSession:ApiKeyStr withKey:ApiSecretStr];
	}
}

/**
 * End capturing stats and queue the upload 
 */
void UApsalarAnalyticsIPhone::EndSession()
{
	UAnalyticEventsBase::EndSession();
	
	[Apsalar endSession];

	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::EndSession"));
}

/**
 * Adds a named event to the session
 *
 * @param EventName unique string for named event
 * @param bTimed if true then event is logged with timing
 */
void UApsalarAnalyticsIPhone::LogStringEvent(const FString& EventName,UBOOL bTimed)
{
	debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::LogStringEvent [%s]"),*EventName);
	WarnIfEventNameIsTooLong(EventName);

 	NSString* EventNameStr = [NSString stringWithCString:TCHAR_TO_UTF8(*EventName) encoding:NSUTF8StringEncoding];
 	[Apsalar event:EventNameStr];
}

/**
 * Adds a named event to the session with a single parameter/value
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 * @param bTimed if true then event is logged with timing
 */
void UApsalarAnalyticsIPhone::LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed)
{ 
	// can't do this at startup for some reason, have to wait for a bit.
	static bool bLoggedApsalarID = false;
	if (!bLoggedApsalarID)
	{
		bLoggedApsalarID = true;
		debugf(TEXT("ApsalarID: %s"), *FString([Apsalar apsalarID]));
	}

	debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::LogStringEventParam [%s %s %s]"),
		*EventName,*ParamName,*ParamValue);
	WarnIfEventNameIsTooLong(EventName);

	NSString* EventNameStr = [NSString stringWithCString:TCHAR_TO_UTF8(*EventName) encoding:NSUTF8StringEncoding];
	NSString* ParamNameStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ParamName) encoding:NSUTF8StringEncoding];
	NSString* ParamValueStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ParamValue) encoding:NSUTF8StringEncoding];
	NSDictionary* EventParamDict = [NSDictionary dictionaryWithObject:ParamValueStr forKey:ParamNameStr];
	[Apsalar event:EventNameStr withArgs:EventParamDict];
}

/**
 * Adds a named event to the session with an array of parameter/values
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs
 * @param bTimed if true then event is logged with timing
 */
void UApsalarAnalyticsIPhone::LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed)
{
	WarnIfEventNameIsTooLong(EventName);
	NSString* EventNameStr = [NSString stringWithCString:TCHAR_TO_UTF8(*EventName) encoding:NSUTF8StringEncoding];
	if (ParamArray.Num() > 0)
	{
		NSDictionary* EventParamDict = [NSMutableDictionary dictionaryWithCapacity:ParamArray.Num()];
		for	(INT Idx=0; Idx < ParamArray.Num(); Idx++)
		{
			debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::LogStringEventParamArray [%s %d][%s %s]"),
				*EventName,Idx,*ParamArray(Idx).ParamName,*ParamArray(Idx).ParamValue);

			NSString* ParamNameStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ParamArray(Idx).ParamName) encoding:NSUTF8StringEncoding];
			NSString* ParamValueStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ParamArray(Idx).ParamValue) encoding:NSUTF8StringEncoding];
			[EventParamDict setValue:ParamValueStr forKey:ParamNameStr];
		}
		[Apsalar event:EventNameStr withArgs:EventParamDict];
	}
	else
	{
		[Apsalar event:EventNameStr];	
	}
}

/**
 * Adds a named error event with corresponding error message
 *
 * @param ErrorName unique string for error event 
 * @param ErrorMessage message detailing the error encountered
 */
void UApsalarAnalyticsIPhone::LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage)
{
	debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsIPhone::LogErrorMessage [%s %s]"),*ErrorName,*ErrorMessage);
	WarnIfEventNameIsTooLong(ErrorName);

	NSString* EventNameStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ErrorName) encoding:NSUTF8StringEncoding];
	NSString* ParamValueStr = [NSString stringWithCString:TCHAR_TO_UTF8(*ErrorMessage) encoding:NSUTF8StringEncoding];
	NSDictionary* EventParamDict = [NSDictionary dictionaryWithObject:ParamValueStr forKey:@"ErrorMessage"];
	[Apsalar event:EventNameStr withArgs:EventParamDict];
}

/** See documentation in UAnalyticEventsBase.uc */
void UApsalarAnalyticsIPhone::LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue)
{
	TArray<FEventStringParam> Param;
	Param.AddItem(FEventStringParam(AttributeName, AttributeValue));
	LogUserAttributeUpdateArray(Param);
}

/** See documentation in UAnalyticEventsBase.uc */
void UApsalarAnalyticsIPhone::LogUserAttributeUpdateArray(const TArray<FEventStringParam>& ParamArray)
{
	LogStringEventParamArray(TEXT("User Attribute"), ParamArray, FALSE);
}

/** See documentation in UAnalyticEventsBase.uc */
void UApsalarAnalyticsIPhone::LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	TArray<FEventStringParam> Params;
	Params.AddItem(FEventStringParam(TEXT("ItemId"), ItemId));
	Params.AddItem(FEventStringParam(TEXT("Currency"), Currency));
	Params.AddItem(FEventStringParam(TEXT("PerItemCost"), FString::Printf(TEXT("%d"), PerItemCost)));
	Params.AddItem(FEventStringParam(TEXT("ItemQuantity"), FString::Printf(TEXT("%d"), ItemQuantity)));
	LogStringEventParamArray(TEXT("Item Purchase"), Params, FALSE);
}

/** See documentation in UAnalyticEventsBase.uc */
void UApsalarAnalyticsIPhone::LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	TArray<FEventStringParam> Params;
	Params.AddItem(FEventStringParam(TEXT("GameCurrencyType"), GameCurrencyType));
	Params.AddItem(FEventStringParam(TEXT("GameCurrencyAmount"), FString::Printf(TEXT("%d"), GameCurrencyAmount)));
	Params.AddItem(FEventStringParam(TEXT("RealCurrencyType"), RealCurrencyType));
	Params.AddItem(FEventStringParam(TEXT("RealMoneyCost"), FString::Printf(TEXT("%f"), RealMoneyCost)));
	Params.AddItem(FEventStringParam(TEXT("PaymentProvider"), PaymentProvider));
	LogStringEventParamArray(TEXT("Currency Purchase"), Params, FALSE);
}

/** See documentation in UAnalyticEventsBase.uc */
void UApsalarAnalyticsIPhone::LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	TArray<FEventStringParam> Params;
	Params.AddItem(FEventStringParam(TEXT("GameCurrencyType"), GameCurrencyType));
	Params.AddItem(FEventStringParam(TEXT("GameCurrencyAmount"), FString::Printf(TEXT("%d"), GameCurrencyAmount)));
	LogStringEventParamArray(TEXT("Currency Given"), Params, FALSE);
}

#endif //WITH_APSALAR