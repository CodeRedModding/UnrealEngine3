/*=============================================================================
	AndroidApsalar.h: Android specific support for Apsalar Analytics API
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"

#if WITH_APSALAR
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
class UApsalarAnalyticsAndroid : public UAnalyticEventsBase
{
	DECLARE_CLASS_INTRINSIC(UApsalarAnalyticsAndroid, UAnalyticEventsBase, 0, AndroidDrv)

	// UAnalyticEventsBase interface
	virtual void Init();
	virtual void StartSession();
	virtual void EndSession();
	virtual void LogStringEvent(const FString& EventName,UBOOL bTimed);
	virtual void LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed);
	virtual void LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed);
	virtual void LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage);

	/** Key assigned to the current app */
	FString ApiKey;
	/** Key assigned to the current app */
	FString ApiSecret;

	/** tracks if the session has already been started once (call reStartSession if so). */
	UBOOL bHasBeenStarted;
};
#endif // WITH_APSALAR
