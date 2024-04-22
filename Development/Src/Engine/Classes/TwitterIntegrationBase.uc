/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for Twitter integration (each platform has a subclass)
 */
class TwitterIntegrationBase extends PlatformInterfaceBase
	native(PlatformInterface)
	config(Engine);


/** The possible twitter request methods */
enum ETwitterRequestMethod
{
	TRM_Get,
	TRM_Post,
	TRM_Delete,
};

enum ETwitterIntegrationDelegate
{
	TID_AuthorizeComplete,
	TID_TweetUIComplete,
	TID_RequestComplete,
};


/**
 * Perform any needed initialization
 */
native event Init();

/**
 * @return true if the user is allowed to use the Tweet UI
 */
native event bool CanShowTweetUI();

/**
 * Kicks off a tweet, using the platform to show the UI. If this returns FALSE, or you are on a platform that doesn't support the UI,
 * you can use the TwitterRequest method to perform a manual tweet using the Twitter API
 *
 * @param InitialMessage [optional] Initial message to show
 * @param URL [optional] URL to attach to the tweet
 * @param Picture [optional] Name of a picture (stored locally, platform subclass will do the searching for it) to add to the tweet
 *
 * @return TRUE if a UI was displayed for the user to interact with, and a TID_TweetUIComplete will be sent
 */
native event bool ShowTweetUI(optional string InitialMessage, optional string URL, optional string Picture);

/**
 * Starts the process of authorizing the local user(s). When TID_AuthorizeComplete is called, then GetNumAccounts() 
 * will return a valid number of accounts
 *
 * @return TRUE if the authorization process started, and TID_AuthorizeComplete delegates will be called
 */
native event bool AuthorizeAccounts();

/**
 * @return The number of accounts that were authorized
 */
native event int GetNumAccounts();

/**
 * @return the display name of the given Twitter account
 */
native event string GetAccountName(int AccountIndex);

/**
 * @return the id of the given Twitter account
 */
native event string GetAccountId(int AccountIndex);

/**
 * Kicks off a generic twitter request
 *
 * @param URL The URL for the twitter request
 * @param KeysAndValues The extra parameters to pass to the request (request specific). Separate keys and values: < "key1", "value1", "key2", "value2" >
 * @param Method The method for this request (get, post, delete)
 * @param AccountIndex A user index if an account is needed, or -1 if an account isn't needed for the request
 *
 * @return TRUE the request was sent off, and a TID_RequestComplete
 */
native event bool TwitterRequest(string URL, array<string> ParamKeysAndValues, ETwitterRequestMethod RequestMethod, int AccountIndex);
