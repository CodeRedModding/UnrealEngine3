/*=============================================================================
 	AndroidApsalar.cpp: Android Apsalar definitions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "Core.h"

#if WITH_APSALAR
#include <jni.h>

#include "AndroidApsalar.h"
#include "AndroidJNI.h"

#if 0
	#define debugfStatsSlow debugf
#else
	#define debugfStatsSlow debugfSuppressed
#endif

IMPLEMENT_CLASS(UApsalarAnalyticsAndroid);
void AutoInitializeRegistrantsApsalarAnalyticsAndroid( INT& Lookup )
{
	UApsalarAnalyticsAndroid::StaticClass();
}

/**
 * Perform any initialization. Called once after singleton instantiation.
 * Overrides default Apsalar analytics behavior if config options are specified
 */
void UApsalarAnalyticsAndroid::Init()
{	
	CallJava_ApsalarInit();

	// On Android packaging mode correlates to running installed
#if FINAL_RELEASE
	// shipping PC game 
	UBOOL bIsRelease = TRUE;
#else
	UBOOL bIsRelease = ParseParam(appCmdLine(),TEXT("installed"));
#endif

	DEFINE_ANALYTICS_CONFIG;
	FAnalyticsConfig* Config = AnalyticsConfig.Find(GGameName);
	if (Config)
	{
		// Allow one ApiKey for development, and one for production
		ApiKey = bIsRelease ? Config->ApsalarAPIKeyRelease : Config->ApsalarAPIKeyDev;
		ApiSecret = bIsRelease ? Config->ApsalarSecretRelease : Config->ApsalarSecretDev;
	}
	else
	{
		// Allow one ApiKey for development, and one for production
		const TCHAR* ApiKeyStr = bIsRelease ? TEXT("ApiKeyRelease") : TEXT("ApiKeyDev");
		if (!GConfig->GetString(TEXT("AndroidDrv.ApsalarAnalyticsAndroid"), ApiKeyStr, ApiKey, GEngineIni))
		{
			debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid missing %s. No uploads will be processed."), ApiKeyStr);
		}
		const TCHAR* ApiSecretStr = bIsRelease ? TEXT("ApiSecretRelease") : TEXT("ApiSecretDev");
		if (!GConfig->GetString(TEXT("AndroidDrv.ApsalarAnalyticsAndroid"), ApiSecretStr, ApiSecret, GEngineIni))
		{
			debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid missing %s. No uploads will be processed."), ApiSecretStr);
		}
	}

	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::Init called, APIKey [%s]"), *ApiKey);

	bHasBeenStarted = FALSE;
}

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
void UApsalarAnalyticsAndroid::StartSession()
{
	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::StartSession()"));

	if (bSessionInProgress)
	{
		debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::StartSession called again. Ignoring."));
		return;
	}

	UAnalyticEventsBase::StartSession();

	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::StartSession [%s]"),*ApiKey);

	CallJava_ApsalarStartSession(*ApiKey, *ApiSecret);

	// Java side will collect and send data 
	CallJava_ApsalarLogEngineData(TEXT("EngineData"));
}

/**
 * End capturing stats and queue the upload 
 */
void UApsalarAnalyticsAndroid::EndSession()
{
	UAnalyticEventsBase::EndSession();

	// Actual Call to Apsalar EndSession has been moved to Android's onStop

	debugf(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::EndSession"));
}

/**
 * Adds a named event to the session
 *
 * @param EventName unique string for named event
 * @param bTimed if true then event is logged with timing
 */
void UApsalarAnalyticsAndroid::LogStringEvent(const FString& EventName,UBOOL bTimed)
{
	debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::LogStringEvent [%s]"),*EventName);
	WarnIfEventNameIsTooLong(EventName);

	CallJava_ApsalarLogStringEvent(*EventName);
}

/**
 * Adds a named event to the session with a single parameter/value
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 * @param bTimed if true then event is logged with timing
 */
void UApsalarAnalyticsAndroid::LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed)
{
	// can't do this at startup for some reason, have to wait for a bit.
	static bool bLoggedApsalarID = false;
	if (!bLoggedApsalarID)
	{
		bLoggedApsalarID = true;
		//debugf(TEXT("ApsalarID: %s"), *FString([Apsalar apsalarID]));
	}

	debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::LogStringEventParam [%s %s %s]"),
		*EventName,*ParamName,*ParamValue);
	WarnIfEventNameIsTooLong(EventName);

	CallJava_ApsalarLogStringEventParam(*EventName, *ParamName, *ParamValue);
}

 /**
 * Adds a named event to the session with an array of parameter/values
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs
 * @param bTimed if true then event is logged with timing
 */
void UApsalarAnalyticsAndroid::LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed)
{
	WarnIfEventNameIsTooLong(EventName);

	if (ParamArray.Num() > 0)
	{
		CallJava_ApsalarLogStringEventParamArray(*EventName, ParamArray);
	}
	else
	{
		CallJava_ApsalarLogStringEvent(*EventName);
	}
}

 /**
 * Adds a named error event with corresponding error message
 *
 * @param ErrorName unique string for error event 
 * @param ErrorMessage message detailing the error encountered
 */
void UApsalarAnalyticsAndroid::LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage)
{
	debugfStatsSlow(NAME_DevStats,TEXT("UApsalarAnalyticsAndroid::LogErrorMessage [%s %s]"),*ErrorName,*ErrorMessage);

	CallJava_ApsalarLogStringEventParam(*ErrorName, TEXT("ErrorMessage"), *ErrorMessage);
}

#endif // WITH_APSALAR
