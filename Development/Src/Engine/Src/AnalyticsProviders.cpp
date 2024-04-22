/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EnginePlatformInterfaceClasses.h"

IMPLEMENT_CLASS(UMultiProviderAnalytics);

/**
 * Set the UserID for use with analytics.
 */
void UMultiProviderAnalytics::SetUserId(const FString& NewUserId)
{
	Super::SetUserId(NewUserId);

	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->SetUserId(NewUserId);
		}
	}
}


/**
 * Start capturing stats for upload 
 */
void UMultiProviderAnalytics::StartSession()
{
	Super::StartSession();

	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->StartSession();
		}
	}
}
    
/**
 * End capturing stats and queue the upload 
 */
void UMultiProviderAnalytics::EndSession()
{
	Super::EndSession();

	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->EndSession();
		}
	}
}

/**
 * Adds a named event to the session
 *
 * @param EventName unique string for named event
 * @param bTimed if true then event is logged with timing
 */
void UMultiProviderAnalytics::LogStringEvent(const FString& EventName, UBOOL bTimed)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogStringEvent(EventName,bTimed);
		}
	}	
}

/**
 * Ends a timed string event
 *
 * @param EventName unique string for named event
 */
void UMultiProviderAnalytics::EndStringEvent(const FString& EventName)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->EndStringEvent(EventName);
		}
	}
}

/**
 * Adds a named event to the session with a single parameter/value
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 * @param bTimed if true then event is logged with timing
 */
void UMultiProviderAnalytics::LogStringEventParam(const FString& EventName, const FString& ParamName, const FString& ParamValue, UBOOL bTimed)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogStringEventParam(EventName,ParamName,ParamValue,bTimed);
		}
	}
}

/**
 * Ends a timed event with a single parameter/value.  Param values are updated for ended event.
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 */
void UMultiProviderAnalytics::EndStringEventParam(const FString& EventName, const FString& ParamName, const FString& ParamValue)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->EndStringEventParam(EventName,ParamName,ParamValue);
		}
	}	
}

/**
 * Adds a named event to the session with an array of parameter/values
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs
 * @param bTimed if true then event is logged with timing
 */
void UMultiProviderAnalytics::LogStringEventParamArray(const FString& EventName, const TArray<struct FEventStringParam>& ParamArray, UBOOL bTimed)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogStringEventParamArray(EventName,ParamArray,bTimed);
		}
	}	
}

/**
 * Ends a timed event with an array of parameter/values. Param values are updated for ended event unless array is empty
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs. If array is empty ending the event wont update values
 */
void UMultiProviderAnalytics::EndStringEventParamArray(const FString& EventName, const TArray<struct FEventStringParam>& ParamArray)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->EndStringEventParamArray(EventName,ParamArray);
		}
	}	
}

/**
 * Adds a named error event with corresponding error message
 *
 * @param ErrorName unique string for error event 
 * @param ErrorMessage message detailing the error encountered
 */
void UMultiProviderAnalytics::LogErrorMessage(const FString& ErrorName, const FString& ErrorMessage)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogErrorMessage(ErrorName,ErrorMessage);
		}
	}	
}

/**
 * Update a single user attribute.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param AttributeName - the name of the attribute
 * @param AttributeValue - the value of the attribute.
 */
void UMultiProviderAnalytics::LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogUserAttributeUpdate(AttributeName, AttributeValue);
		}
	}	
}

/**
 * Update an array of user attributes.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param AttributeArray - the array of attribute name/values to set.
 */
void UMultiProviderAnalytics::LogUserAttributeUpdateArray(const TArray<FEventStringParam>& AttributeArray)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogUserAttributeUpdateArray(AttributeArray);
		}
	}	
}

/**
 * Record an in-game purchase of a an item.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param ItemId - the ID of the item, should be registered with the provider first.
 * @param Currency - the currency of the purchase (ie, Gold, Coins, etc), should be registered with the provider first.
 * @param PerItemCost - the cost of one item in the currency given.
 * @param ItemQuantity - the number of Items purchased.
 */
void UMultiProviderAnalytics::LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogItemPurchaseEvent(ItemId, Currency, PerItemCost, ItemQuantity);
		}
	}	
}

/**
 * Record a purchase of in-game currency using real-world money.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param GameCurrencyType - type of in game currency purchased, should be registered with the provider first.
 * @param GameCurrencyAmount - amount of in game currency purchased.
 * @param RealCurrencyType - real-world currency type (like a 3-character ISO 4217 currency code, but provider dependent).
 * @param RealMoneyCost - cost of the currency in real world money, expressed in RealCurrencyType units.
 * @param PaymentProvider - Provider who brokered the transaction. Generally arbitrary, but examples are PayPal, Facebook Credits, App Store, etc.
 */
void UMultiProviderAnalytics::LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogCurrencyPurchaseEvent(GameCurrencyType, GameCurrencyAmount, RealCurrencyType, RealMoneyCost, PaymentProvider);
		}
	}	
}

/**
 * Record a gift of in-game currency from the game itself.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param GameCurrencyType - type of in game currency given, should be registered with the provider first.
 * @param GameCurrencyAmount - amount of in game currency given.
 */
void UMultiProviderAnalytics::LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount)
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->LogCurrencyGivenEvent(GameCurrencyType, GameCurrencyAmount);
		}
	}	
}

/**
 * Flush any cached events to the analytics provider.
 *
 * Note that not all providers support explicitly sending any cached events. In this case this method
 * does nothing.
 */
void UMultiProviderAnalytics::SendCachedEvents()
{
	for (INT Idx=0; Idx < AnalyticsProviders.Num(); Idx++)
	{
		if (AnalyticsProviders(Idx) != NULL)
		{
			AnalyticsProviders(Idx)->SendCachedEvents();
		}
	}	
}
