/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides extended player functionality not supported by
 * all platforms. The OnlineSubsystem will return NULL when requesting this
 * interface on a platform where it is not supporeted.
 */
interface OnlinePartyChatInterface dependson(OnlineSubsystem);

/**
 * Sends an invite to everyone in the existing party session
 *
 * @param LocalUserNum the user to sending the invites
 *
 * @return true if it was able to send them, false otherwise
 */
function bool SendPartyGameInvites(byte LocalUserNum);

/**
 * Called when the async invite send has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnSendPartyGameInvitesComplete(bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to sending the invites
 * @param SendPartyGameInvitesCompleteDelegate the delegate to use for notifications
 */
function AddSendPartyGameInvitesCompleteDelegate(byte LocalUserNum,delegate<OnSendPartyGameInvitesComplete> SendPartyGameInvitesCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to sending the invites
 * @param SendPartyGameInvitesCompleteDelegate the delegate to use for notifications
 */
function ClearSendPartyGameInvitesCompleteDelegate(byte LocalUserNum,delegate<OnSendPartyGameInvitesComplete> SendPartyGameInvitesCompleteDelegate);

/**
 * Gets the party member information from the platform, including the application specific data
 *
 * @param PartyMembers the array to be filled out of party member information
 *
 * @return true if the call could populate the array, false otherwise
 */
function bool GetPartyMembersInformation(out array<OnlinePartyMember> PartyMembers);

/**
 * Gets the individual party member's information from the platform, including the application specific data
 *
 * @param MemberId the id of the party member to lookup
 * @param PartyMember out value where the data is copied to
 *
 * @return true if the call found the player, false otherwise
 */
function bool GetPartyMemberInformation(UniqueNetId MemberId,out OnlinePartyMember PartyMember);

/**
 * Called when a player has joined or left your party chat
 *
 * @param bJoinedOrLeft true if the player joined, false if they left
 * @param PlayerName the name of the player that was affected
 * @param PlayerId the net id of the player that left
 */
delegate OnPartyMemberListChanged(bool bJoinedOrLeft,string PlayerName,UniqueNetId PlayerId);

/**
 * Sets the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMemberListChangedDelegate the delegate to use for notifications
 */
function AddPartyMemberListChangedDelegate(byte LocalUserNum,delegate<OnPartyMemberListChanged> PartyMemberListChangedDelegate);

/**
 * Clears the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMemberListChangedDelegate the delegate to use for notifications
 */
function ClearPartyMemberListChangedDelegate(byte LocalUserNum,delegate<OnPartyMemberListChanged> PartyMemberListChangedDelegate);

/**
 * Called when a player has joined or left your party chat
 *
 * @param PlayerName the name of the player that was affected
 * @param PlayerId the net id of the player that had data change
 * @param CustomData1 the first 4 bytes of the custom data
 * @param CustomData2 the second 4 bytes of the custom data
 * @param CustomData3 the third 4 bytes of the custom data
 * @param CustomData4 the fourth 4 bytes of the custom data
 */
delegate OnPartyMembersInfoChanged(string PlayerName,UniqueNetId PlayerId,int CustomData1,int CustomData2,int CustomData3,int CustomData4);

/**
 * Sets the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMembersInfoChangedDelegate the delegate to use for notifications
 */
function AddPartyMembersInfoChangedDelegate(byte LocalUserNum,delegate<OnPartyMembersInfoChanged> PartyMembersInfoChangedDelegate);

/**
 * Clears the delegate used to notify the gameplay code that async task has completed
 *
 * @param LocalUserNum the user to listening for party chat notifications
 * @param PartyMembersInfoChangedDelegate the delegate to use for notifications
 */
function ClearPartyMembersInfoChangedDelegate(byte LocalUserNum,delegate<OnPartyMembersInfoChanged> PartyMembersInfoChangedDelegate);

/**
 * Sets a party member's application specific data
 *
 * @param LocalUserNum the user that you are setting the data for
 * @param Data1 the first 4 bytes of custom data
 * @param Data2 the second 4 bytes of custom data
 * @param Data3 the third 4 bytes of custom data
 * @param Data4 the fourth 4 bytes of custom data
 *
 * @return true if the data could be set, false otherwise
 */
function bool SetPartyMemberCustomData(byte LocalUserNum,int Data1,int Data2,int Data3,int Data4);

/**
 * Determines the amount of data that has been sent in the last second
 *
 * @return >= 0 if able to get the bandwidth used over the last second, < 0 upon an error
 */
function int GetPartyBandwidth();

/**
 * Opens the party UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
function bool ShowPartyUI(byte LocalUserNum);

/**
 * Opens the voice channel UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
function bool ShowVoiceChannelUI(byte LocalUserNum);

/**
 * Opens the community sessions UI for the user
 *
 * @param LocalUserNum the user requesting the UI
 */
function bool ShowCommunitySessionsUI(byte LocalUserNum);

/**
 * Checks for the specified player being in a party chat
 *
 * @param LocalUserNum the user that you are setting the data for
 *
 * @return true if there is a party chat in progress, false otherwise
 */
function bool IsInPartyChat(byte LocalUserNum);