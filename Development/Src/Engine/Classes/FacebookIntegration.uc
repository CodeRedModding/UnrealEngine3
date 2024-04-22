/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This is the base class for Facebook integration (each platform has a subclass
 */
class FacebookIntegration extends PlatformInterfaceBase
	native(PlatformInterface)
	config(Engine);



enum EFacebookIntegrationDelegate
{
	FID_AuthorizationComplete,
	FID_FacebookRequestComplete,
	FID_DialogComplete,
	FID_FriendsListComplete,
};


/** The application ID to link to */
var config string AppID;

/** Permissions that are expected by game - see: http://developers.facebook.com/docs/authentication/permissions/ */
var config array<string> Permissions;

/** Username of the current user */
var string Username;

/** Id of the current user */
var string UserId;

/** Access token as retrieved from FB */
var string AccessToken;


/** Structure to hold a Facebook friend */
struct native FacebookFriend
{
	/** The user's display name */
	var string Name;

	/** The user's id, can be used to send messages, etc */
	var string Id;
};

/** The list of friends that is filled out as soon as the user logs on */
var array<FacebookFriend> FriendsList;


/**
 * Perform any needed initialization
 */
native event bool Init();

/**
 * Starts the process of allowing the app to use Facebook
 */
native event bool Authorize();

/**
 * @return true if the app has been authorized by the current user
 */
native event bool IsAuthorized();

/**
 * Kicks off a Facebook GraphAPI request (response will come via delegate)
 *
 * @param GraphRequest The request to make (like "me/groups")
 */
native event FacebookRequest(string GraphRequest);

/**
 * Shows a facebook dialog (ie, posting to wall)
 *
 * @param Action The dialog to open (like "feed")
 * @param KeysAndValues The extra parameters to pass to the dialog (dialog specific). Separate keys and values: < "key1", "value1", "key2", "value2" >
 */
native event FacebookDialog(string Action, array<string> ParamKeysAndValues);

/**
 * Call this to disconnect from Facebook. Next time authorization happens, the auth webpage
 * will be shown again
 */
native event Disconnect();
