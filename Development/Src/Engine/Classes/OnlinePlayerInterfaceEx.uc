/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides extended player functionality not supported by
 * all platforms. The OnlineSubsystem will return NULL when requesting this
 * interface on a platform where it is not supporeted.
 */
interface OnlinePlayerInterfaceEx dependson(OnlineSubsystem);

/**
 * Displays the UI that allows a player to give feedback on another player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player having feedback given for
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowFeedbackUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the gamer card UI for the specified player
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player to show the gamer card of
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowGamerCardUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the messages UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowMessagesUI(byte LocalUserNum);

/**
 * Displays the achievements UI for a player
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowAchievementsUI(byte LocalUserNum);

/**
 * Displays the invite ui
 *
 * @param LocalUserNum the local user sending the invite
 * @param InviteText the string to prefill the UI with
 */
function bool ShowInviteUI(byte LocalUserNum,optional string InviteText);

/**
 * Displays the marketplace UI for content
 *
 * @param LocalUserNum the local user viewing available content
 * @param CategoryMask the bitmask to use to filter content by type
 * @param OfferId a specific offer that you want shown
 */
function bool ShowContentMarketplaceUI(byte LocalUserNum,optional int CategoryMask = -1,optional int OfferId);

/**
 * Displays the marketplace UI for memberships
 *
 * @param LocalUserNum the local user viewing available memberships
 */
function bool ShowMembershipMarketplaceUI(byte LocalUserNum);

/**
 * Displays the UI that allows the user to choose which device to save content to
 *
 * @param LocalUserNum the controller number of the associated user
 * @param SizeNeeded the size of the data to be saved in bytes
 * @param bManageStorage whether to allow the user to manage their storage or not
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowDeviceSelectionUI(byte LocalUserNum,int SizeNeeded,optional bool bManageStorage);

/**
 * Delegate used when the device selection request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnDeviceSelectionComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the user has completed
 * their device selection
 *
 * @param DeviceDelegate the delegate to use for notifications
 */
function AddDeviceSelectionDoneDelegate(byte LocalUserNum,delegate<OnDeviceSelectionComplete> DeviceDelegate);

/**
 * Removes the specified delegate from the list of callbacks
 *
 * @param DeviceDelegate the delegate to use for notifications
 */
function ClearDeviceSelectionDoneDelegate(byte LocalUserNum,delegate<OnDeviceSelectionComplete> DeviceDelegate);

/**
 * Fetches the results of the device selection
 *
 * @param LocalUserNum the player to check the results for
 * @param DeviceName out param that gets a copy of the string
 *
 * @return the ID of the device that was selected
 * NOTE: Zero means the user hasn't selected one
 */
function int GetDeviceSelectionResults(byte LocalUserNum,out string DeviceName);

/**
 * Checks the device id to determine if it is still valid (could be removed) and/or
 * if there is enough space on the specified device
 *
 * @param DeviceId the device to check
 * @param SizeNeeded the amount of space requested
 *
 * @return true if valid, false otherwise
 */
function bool IsDeviceValid(int DeviceId,optional int SizeNeeded);

/**
 * Unlocks a gamer picture for the local user
 *
 * @param LocalUserNum the user to unlock the picture for
 * @param PictureId the id of the picture to unlock
 */
function bool UnlockGamerPicture(byte LocalUserNum,int PictureId);

/**
 * Called when an external change to player profile data has occured
 */
delegate OnProfileDataChanged();

/**
 * Sets the delegate used to notify the gameplay code that someone has changed their profile data externally
 *
 * @param LocalUserNum the user the delegate is interested in
 * @param ProfileDataChangedDelegate the delegate to use for notifications
 */
function AddProfileDataChangedDelegate(byte LocalUserNum,delegate<OnProfileDataChanged> ProfileDataChangedDelegate);

/**
 * Clears the delegate used to notify the gameplay code that someone has changed their profile data externally
 *
 * @param LocalUserNum the user the delegate is interested in
 * @param ProfileDataChangedDelegate the delegate to use for notifications
 */
function ClearProfileDataChangedDelegate(byte LocalUserNum,delegate<OnProfileDataChanged> ProfileDataChangedDelegate);

/**
 * Displays the UI that shows a user's list of friends
 *
 * @param LocalUserNum the controller number of the associated user
 * @param PlayerId the id of the player being invited
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowFriendsInviteUI(byte LocalUserNum,UniqueNetId PlayerId);

/**
 * Displays the UI that shows the player list
 *
 * @param LocalUserNum the controller number of the associated user
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowPlayersUI(byte LocalUserNum);

/**
 * Shows a custom players UI for the specified list of players
 *
 * @param LocalUserNum the controller number of the associated user
 * @param Players the list of players to show in the custom UI
 * @param Title the title to use for the UI
 * @param Description the text to show at the top of the UI
 *
 * @return TRUE if it was able to show the UI, FALSE if it failed
 */
function bool ShowCustomPlayersUI(byte LocalUserNum,const out array<UniqueNetId> Players,string Title,string Description);

/**
 * Unlocks an avatar award for the local user
 *
 * @param LocalUserNum the user to unlock the avatar item for
 * @param AvatarItemId the id of the avatar item to unlock
 */
function bool UnlockAvatarAward(byte LocalUserNum,int AvatarItemId);

/**
 * Reads the online profile settings for a given user and title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 * @param ProfileSettings the object to copy the results to and contains the list of items to read
 *
 * @return true if the call succeeds, false otherwise
 */
function bool ReadCrossTitleProfileSettings(byte LocalUserNum,int TitleId,OnlineProfileSettings ProfileSettings);

/**
 * Delegate used when the last read profile settings request has completed
 *
 * @param LocalUserNum the controller index of the player who's read just completed
 * @param TitleId the title that the profile settings were read for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadCrossTitleProfileSettingsComplete(byte LocalUserNum,int TitleId,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that the last read request has completed
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to use for notifications
 */
function AddReadCrossTitleProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnReadCrossTitleProfileSettingsComplete> ReadProfileSettingsCompleteDelegate);

/**
 * Searches the existing set of delegates for the one specified and removes it
 * from the list
 *
 * @param LocalUserNum which user to watch for read complete notifications
 * @param ReadProfileSettingsCompleteDelegate the delegate to find and clear
 */
function ClearReadCrossTitleProfileSettingsCompleteDelegate(byte LocalUserNum,delegate<OnReadCrossTitleProfileSettingsComplete> ReadProfileSettingsCompleteDelegate);

/**
 * Returns the online profile settings for a given user and title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 *
 * @return the profile settings object
 */
function OnlineProfileSettings GetCrossTitleProfileSettings(byte LocalUserNum,int TitleId);

/**
 * Removes a cached entry of a profile for the specified title id
 *
 * @param LocalUserNum the user that we are reading the data for
 * @param TitleId the title that the profile settings are being read for
 */
function ClearCrossTitleProfileSettings(byte LocalUserNum,int TitleId);

/**
 * Shows a dialog with the message pre-populated in it
 *
 * @param LocalUserNum the user sending the message
 * @param Recipients the list of people to send the message to
 * @param MessageTitle the title of the message being sent
 * @param NonEditableMessage the portion of the message that the user cannot edit
 * @param EditableMessage the portion of the message the user can edit
 *
 * @return true if successful, false otherwise
 */
function bool ShowCustomMessageUI(byte LocalUserNum,const out array<UniqueNetId> Recipients,string MessageTitle,string NonEditableMessage,optional string EditableMessage);	