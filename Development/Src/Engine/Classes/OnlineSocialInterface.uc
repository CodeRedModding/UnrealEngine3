/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides functions for posting to and querying capabilities for social networking sites
 */
interface OnlineSocialInterface dependson(OnlineSubsystem);

/**
 * Queries the social networking features that the title is allowed to use.
 *
 * @return true if the async task was successfully started, false otherwise
 */
function bool QuerySocialPostPrivileges();

/**
 * Delegate used in notifying the UI/game that querying for social privileges completed
 *
 * @param bWasSuccessful true if the query completed ok, false otherwise
 * @param PostPrivileges struct containing the supported flags for enabled social features
 */
delegate OnQuerySocialPostPrivilegesCompleted(bool bWasSuccessful,SocialPostPrivileges PostPrivileges);

/**
 * Sets the delegate used to notify the gameplay code that social post privileges query has completed
 *
 * @param PostPrivilegesDelegate the delegate to use for notifications
 */
function AddQuerySocialPostPrivilegesCompleted(delegate<OnQuerySocialPostPrivilegesCompleted> PostPrivilegesDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param PostPrivilegesDelegate the delegate to use for notifications
 */
function ClearQuerySocialPostPrivilegesCompleted(delegate<OnQuerySocialPostPrivilegesCompleted> PostPrivilegesDelegate);

/**
 * Posts an image to a social network site
 *
 * @param LocalUserNum local user that the image is being posted for
 * @param PostImageInfo contains the description info needed to post the image
 * @param FullImage byte array containing the image to be uploaded
 *
 * @return true if the async task was successfully started, false otherwise
 */
function bool PostImage(byte LocalUserNum,const out SocialPostImageInfo PostImageInfo, const array<byte> FullImage);

/**
 * Delegate used in notifying the UI/game that posting a social image has completed
 *
 * @param LocalUserNum local user that the image is being posted for
 * @param bWasSuccessful true if the query completed ok, false otherwise
 */
delegate OnPostImageCompleted(byte LocalUserNum,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that social image post has completed
 *
 * @param LocalUserNum local user that the image is being posted for
 * @param PostImageDelegate the delegate to use for notifications
 */
function AddPostImageCompleted(byte LocalUserNum,delegate<OnPostImageCompleted> PostImageDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum local user that the image is being posted for
 * @param PostImageDelegate the delegate to use for notifications
 */
function ClearPostImageCompleted(byte LocalUserNum,delegate<OnPostImageCompleted> PostImageDelegate);

/**
 * Posts an image link to a social network site
 *
 * @param LocalUserNum local user that the image link is being posted for
 * @param PostLinkInfo contains the description info needed to post the image link 
 *
 * @return true if the async task was successfully started, false otherwise
 */
function bool PostLink(byte LocalUserNum,const out SocialPostLinkInfo PostLinkInfo);

/**
 * Delegate used in notifying the UI/game that posting a social image link has completed
 *
 * @param LocalUserNum local user that the image link is being posted for
 * @param bWasSuccessful true if the query completed ok, false otherwise
 */
delegate OnPostLinkCompleted(byte LocalUserNum,bool bWasSuccessful);

/**
 * Sets the delegate used to notify the gameplay code that social image link post has completed
 *
 * @param LocalUserNum local user that the image link is being posted for
 * @param PostLinkDelegate the delegate to use for notifications
 */
function AddPostLinkCompleted(byte LocalUserNum,delegate<OnPostLinkCompleted> PostLinkDelegate);

/**
 * Removes the specified delegate from the notification list
 *
 * @param LocalUserNum local user that the image link is being posted for
 * @param PostLinkDelegate the delegate to use for notifications
 */
function ClearPostLinkCompleted(byte LocalUserNum,delegate<OnPostLinkCompleted> PostLinkDelegate);

