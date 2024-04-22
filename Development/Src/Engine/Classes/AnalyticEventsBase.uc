/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for per-platform support for uploading analytics events
 */
 
class AnalyticEventsBase extends PlatformInterfaceBase
	native(PlatformInterface)
	config(Engine);

/** Named param/value string pairing */
struct native EventStringParam
{
	/** parameter name */
	var string ParamName;
	/** parameter value */
	var string ParamValue;

	structcpptext
	{
		FEventStringParam(const FString& InNameStr,const FString& InValueStr)
		{
			appMemzero(this, sizeof(FEventStringParam));
			ParamName = InNameStr;
			ParamValue = InValueStr;
		}
		FEventStringParam(EEventParm)
		{
			appMemzero(this, sizeof(FEventStringParam));
		}
	}
};

/** TRUE if a session has been started and is currently in progress */
var const bool bSessionInProgress;

/**
 * TRUE if the analytics session should be auto started and stopped on application
 * startup. This makes it easier, but some providers may require additional information
 * before a session can be started.
 * 
 * For example, Swrve requires a UserId to be set before a session can be started.
 * Since this may come from an outside source (backend server), it might not be possible
 * to start the analytics session immediately.
 * 
 * If this value is false, StartSession and EndSession must be called manually by the
 * game code at some point.
 */
var config bool bAutoStartSession;

/**
 * When the app is paused (backgrounded on iOS) for more than the specified number
 * of seconds, it signals supporting analytics providers to restart the session.
 * Idea is that if a user backgrounds the app for a brief period, the session should
 * not end, but if they don't come back to the app for a period of time, it should
 * constitute a new session.
 * 
 * A value of zero means the session should not be restarted when paused for any length of time.
 * 
 * Most analytics providers have a built-in timeout for a session that cannot 
 * be overridden, but it is usually quite long (ie, Swrve times out after 60 minutes).
 * 
 * This is currently only supported on iOS.
 */
var config int SessionPauseThresholdSec;

/** stores the UserId if one has been provided. See SetUserID for details. */
var const string UserId;

/** 
 * @return TRUE if a session has been started and is currently in progress 
 */
function bool IsSessionInProgress()
{
	return bSessionInProgress;
}

/**
 * Perform any initialization. Called once after singleton instantiation
 */
native event Init();

/**
 * Set the UserID for use with analytics. Some providers require a unique ID
 * to be provided when supplying events, and some providers create their own.
 * If you are using a provider that requires you to supply the ID, use this
 * method to set it. It is probably best for online games to use the McpId
 */
native event SetUserId(string NewUserId);

/**
 * Start capturing stats for upload 
 */
native event StartSession();

/**
 * End capturing stats and queue the upload 
 */
native event EndSession();

/**
 * Adds a named event to the session
 *
 * @param EventName unique string for named event
 * @param bTimed if true then event is logged with timing
 */
native event LogStringEvent(string EventName, bool bTimed);

/**
 * Ends a timed string event
 *
 * @param EventName unique string for named event
 */
native event EndStringEvent(string EventName);

/**
 * Adds a named event to the session with a single parameter/value
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 * @param bTimed if true then event is logged with timing
 */
native event LogStringEventParam(string EventName, string ParamName, string ParamValue, bool bTimed);

/**
 * Ends a timed event with a single parameter/value.  Param values are updated for ended event.
 *
 * @param EventName unique string for named 
 * @param ParamName parameter name for the event
 * @param ParamValue parameter value for the event
 */
native event EndStringEventParam(string EventName, string ParamName, string ParamValue);

/**
 * Adds a named event to the session with an array of parameter/values
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs
 * @param bTimed if true then event is logged with timing
 */
native event LogStringEventParamArray(string EventName, array<EventStringParam> ParamArray, bool bTimed);

/**
 * Ends a timed event with an array of parameter/values. Param values are updated for ended event unless array is empty
 *
 * @param EventName unique string for named 
 * @param ParamArray array of parameter name/value pairs. If array is empty ending the event wont update values
 */
native event EndStringEventParamArray(string EventName, array<EventStringParam> ParamArray);

/**
 * Adds a named error event with corresponding error message
 *
 * @param ErrorName unique string for error event 
 * @param ErrorMessage message detailing the error encountered
 */
native event LogErrorMessage(string ErrorName, string ErrorMessage);

/**
 * Update a single user attribute.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param AttributeName - the name of the attribute
 * @param AttributeValue - the value of the attribute.
 */
native event LogUserAttributeUpdate(string AttributeName, string AttributeValue);

/**
 * Update an array of user attributes.
 * 
 * Note that not all providers support user attributes. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param AttributeArray - the array of attribute name/values to set.
 */
native event LogUserAttributeUpdateArray(array<EventStringParam> AttributeArray);

/**
 * Record an in-game purchase of a an item.
 * 
 * Note that not all providers support item purchase events. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param ItemId - the ID of the item, should be registered with the provider first.
 * @param Currency - the currency of the purchase (ie, Gold, Coins, etc), should be registered with the provider first.
 * @param PerItemCost - the cost of one item in the currency given.
 * @param ItemQuantity - the number of Items purchased.
 */
native event LogItemPurchaseEvent(string ItemId, string Currency, int PerItemCost, int ItemQuantity);

/**
 * Record a purchase of in-game currency using real-world money.
 * 
 * Note that not all providers support currency events. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param GameCurrencyType - type of in game currency purchased, should be registered with the provider first.
 * @param GameCurrencyAmount - amount of in game currency purchased.
 * @param RealCurrencyType - real-world currency type (like a 3-character ISO 4217 currency code, but provider dependent).
 * @param RealMoneyCost - cost of the currency in real world money, expressed in RealCurrencyType units.
 * @param PaymentProvider - Provider who brokered the transaction. Generally arbitrary, but examples are PayPal, Facebook Credits, App Store, etc.
 */
native event LogCurrencyPurchaseEvent(string GameCurrencyType, int GameCurrencyAmount, string RealCurrencyType, float RealMoneyCost, string PaymentProvider);

/**
 * Record a gift of in-game currency from the game itself.
 * 
 * Note that not all providers support currency events. In this case this method
 * is equivalent to sending a regular event.
 * 
 * @param GameCurrencyType - type of in game currency given, should be registered with the provider first.
 * @param GameCurrencyAmount - amount of in game currency given.
 */
native event LogCurrencyGivenEvent(string GameCurrencyType, int GameCurrencyAmount);

/**
 * Flush any cached events to the analytics provider.
 *
 * Note that not all providers support explicitly sending any cached events. In this case this method
 * does nothing.
 */
native event SendCachedEvents();