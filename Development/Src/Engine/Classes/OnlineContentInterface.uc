/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides accessors to the platform specific content
 * system (ie downloadable content, etc)
 */
interface OnlineContentInterface
	dependson(OnlineSubsystem);

/**
 * Delegate used in content change (add or deletion) notifications
 * for any user
 */
delegate OnContentChange();

/**
 * Adds the delegate used to notify the gameplay code that (downloaded) content changed
 *
 * @param Content Delegate the delegate to use for notifications
 * @param LocalUserNum whether to watch for changes on a specific slot or all slots
 */
function AddContentChangeDelegate(delegate<OnContentChange> ContentDelegate, optional byte LocalUserNum = 255);

/**
 * Removes the delegate from the set of delegates that are notified
 *
 * @param Content Delegate the delegate to use for notifications
 * @param LocalUserNum whether to watch for changes on a specific slot or all slots
 */
function ClearContentChangeDelegate(delegate<OnContentChange> ContentDelegate, optional byte LocalUserNum = 255);

/**
 * Delegate used when the content read request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadContentComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the content read request has completed
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ReadContentCompleteDelegate the delegate to use for notifications
 */
function AddReadContentComplete(byte LocalUserNum,EOnlineContentType ContentType,delegate<OnReadContentComplete> ReadContentCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the content read request has completed
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ReadContentCompleteDelegate the delegate to use for notifications
 */
function ClearReadContentComplete(byte LocalUserNum,EOnlineContentType ContentType,delegate<OnReadContentComplete> ReadContentCompleteDelegate);

/**
 * Starts an async task that retrieves the list of downloaded/savegame content for the player.
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param DeviceId optional value to restrict the enumeration to a particular device
 *
 * @return true if the read request was issued successfully, false otherwise
 */
function bool ReadContentList(byte LocalUserNum,EOnlineContentType ContentType,optional int DeviceId = -1);

/**
 * Starts an async task that frees any downloaded content resources for that player
 *
 * @param LocalUserNum The user to clear the content list for
 * @param ContentType the type of content being read
 */
function ClearContentList(byte LocalUserNum,EOnlineContentType ContentType);

/**
 * Retrieve the list of content the given user has downloaded or otherwise retrieved
 * to the local console.
 
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ContentList The out array that receives the list of all content
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
function EOnlineEnumerationReadState GetContentList(byte LocalUserNum,EOnlineContentType ContentType,out array<OnlineContent> ContentList);

/**
 * Starts an async task that retrieves the list of downloaded/savegame content for the player across all titles
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param TitleId the title id to filter on. Zero means all titles
 * @param DeviceId optional value to restrict the enumeration to a particular device
 *
 * @return true if the read request was issued successfully, false otherwise
 */
function bool ReadCrossTitleContentList(byte LocalUserNum,EOnlineContentType ContentType,optional int TitleId = 0,optional int DeviceId = -1);

/**
 * Starts an async task that frees any downloaded content resources for that player
 *
 * @param LocalUserNum The user to clear the content list for
 * @param ContentType the type of content being read
 */
function ClearCrossTitleContentList(byte LocalUserNum,EOnlineContentType ContentType);

/**
 * Retrieve the list of content the given user has downloaded or otherwise retrieved
 * to the local console.
 
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ContentList The out array that receives the list of all content
 *
 * @return OERS_Done if the read has completed, otherwise one of the other states
 */
function EOnlineEnumerationReadState GetCrossTitleContentList(byte LocalUserNum,EOnlineContentType ContentType,out array<OnlineCrossTitleContent> ContentList);

/**
 * Delegate used when the content read request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnReadCrossTitleContentComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the content read request has completed
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ReadContentCompleteDelegate the delegate to use for notifications
 */
function AddReadCrossTitleContentCompleteDelegate(byte LocalUserNum,EOnlineContentType ContentType,delegate<OnReadCrossTitleContentComplete> ReadContentCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the content read request has completed
 *
 * @param LocalUserNum The user to read the content list of
 * @param ContentType the type of content being read
 * @param ReadContentCompleteDelegate the delegate to use for notifications
 */
function ClearReadCrossTitleContentCompleteDelegate(byte LocalUserNum,EOnlineContentType ContentType,delegate<OnReadCrossTitleContentComplete> ReadContentCompleteDelegate);

/**
 * Reads a player's cross title save game data from the specified content bundle
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param TitleId the title id the save game is from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 *
 * @return true if the async read was started successfully, false otherwise
 */
function bool ReadCrossTitleSaveGameData(byte LocalUserNum,int DeviceId,int TitleId,string FriendlyName,string FileName,string SaveFileName);

/**
 * Copies a player's cross title save game data from the cached async read data
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param TitleId the title id the save game is from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 * @param bIsValid out value indicating whether the save is corrupt or not
 * @param SaveGameData the array that is filled with the save game data
 *
 * @return true if the async read was started successfully, false otherwise
 */
function bool GetCrossTitleSaveGameData(byte LocalUserNum,int DeviceId,int TitleId,string FriendlyName,string FileName,string SaveFileName,out byte bIsValid,out array<byte> SaveGameData);

/**
 * Delegate used when the cross title content read request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 * @param LocalUserNum the user that was initiating the data read
 * @param DeviceId the device that the read was on
 * @param TitleId the title id the save game is from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 */
delegate OnReadCrossTitleSaveGameDataComplete(bool bWasSuccessful,byte LocalUserNum,int DeviceId,int TitleId,string FriendlyName,string FileName,string SaveFileName);

/**
 * Adds the delegate used to notify the gameplay code that a cross title save game read request has completed
 *
 * @param LocalUserNum The user that was reading a save game
 * @param ReadSaveGameDataCompleteDelegate the delegate to use for notifications
 */
function AddReadCrossTitleSaveGameDataComplete(byte LocalUserNum,delegate<OnReadCrossTitleSaveGameDataComplete> ReadSaveGameDataCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that a cross title save game read request has completed
 *
 * @param LocalUserNum The user that was reading a save game
 * @param ReadSaveGameDataCompleteDelegate the delegate to use for notifications
 */
function ClearReadCrossTitleSaveGameDataComplete(byte LocalUserNum,delegate<OnReadCrossTitleSaveGameDataComplete> ReadSaveGameDataCompleteDelegate);

/**
 * Clears any cached save games
 *
 * @param LocalUserNum the user that is deleting data
 *
 * @return true if the clear succeeded, false otherwise
 */
function bool ClearCrossTitleSaveGames(byte LocalUserNum);

/**
 * Asks the online system for the number of new and total content downloads
 *
 * @param LocalUserNum the user to check the content download availability for
 * @param CategoryMask the bitmask to use to filter content by type
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
function bool QueryAvailableDownloads(byte LocalUserNum,optional int CategoryMask = -1);

/**
 * Called once the download query completes
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
delegate OnQueryAvailableDownloadsComplete(bool bWasSuccessful);

/**
 * Adds the delegate used to notify the gameplay code that the content download query has completed
 *
 * @param LocalUserNum the user to check the content download availability for
 * @param ReadContentCompleteDelegate the delegate to use for notifications
 */
function AddQueryAvailableDownloadsComplete(byte LocalUserNum,delegate<OnQueryAvailableDownloadsComplete> QueryDownloadsDelegate);

/**
 * Clears the delegate used to notify the gameplay code that the content download query has completed
 *
 * @param LocalUserNum the user to check the content download availability for
 * @param ReadContentCompleteDelegate the delegate to use for notifications
 */
function ClearQueryAvailableDownloadsComplete(byte LocalUserNum,delegate<OnQueryAvailableDownloadsComplete> QueryDownloadsDelegate);

/**
 * Returns the number of new and total downloads available for the user
 *
 * @param LocalUserNum the user to check the content download availability for
 * @param NewDownloads out value of the number of new downloads available
 * @param TotalDownloads out value of the number of total downloads available
 */
function GetAvailableDownloadCounts(byte LocalUserNum,out int NewDownloads,out int TotalDownloads);

/**
 * Reads a player's save game data from the specified content bundle
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 *
 * @return true if the async read was started successfully, false otherwise
 */
function bool ReadSaveGameData(byte LocalUserNum,int DeviceId,string FriendlyName,string FileName,string SaveFileName);

/**
 * Copies a player's save game data from the cached async read data
 *
 * @param LocalUserNum the user that is initiating the data read (also used in validating ownership of the data)
 * @param DeviceId the device to read the same game from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 * @param bIsValid out value indicating whether the save is corrupt or not
 * @param SaveGameData the array that is filled with the save game data
 *
 * @return true if the async read was started successfully, false otherwise
 */
function bool GetSaveGameData(byte LocalUserNum,int DeviceId,string FriendlyName,string FileName,string SaveFileName,out byte bIsValid,out array<byte> SaveGameData);

/**
 * Delegate used when the content read request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 * @param LocalUserNum the user that was initiating the data read
 * @param DeviceId the device that the read was on
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file to read from inside of the content package
 * @param SaveFileName the file name of the save game inside the content package
 */
delegate OnReadSaveGameDataComplete(bool bWasSuccessful,byte LocalUserNum,int DeviceId,string FriendlyName,string FileName,string SaveFileName);

/**
 * Adds the delegate used to notify the gameplay code that a save game read request has completed
 *
 * @param LocalUserNum The user that was reading a save game
 * @param ReadSaveGameDataCompleteDelegate the delegate to use for notifications
 */
function AddReadSaveGameDataComplete(byte LocalUserNum,delegate<OnReadSaveGameDataComplete> ReadSaveGameDataCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that a save game read request has completed
 *
 * @param LocalUserNum The user that was reading a save game
 * @param ReadSaveGameDataCompleteDelegate the delegate to use for notifications
 */
function ClearReadSaveGameDataComplete(byte LocalUserNum,delegate<OnReadSaveGameDataComplete> ReadSaveGameDataCompleteDelegate);

/**
 * Writes a player's save game data to the specified content bundle and file
 *
 * @param LocalUserNum the user that is initiating the data write
 * @param DeviceId the device to write the same game to
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file name of the content package
 * @param SaveFileName the file name of the save game inside the content package
 * @param SaveGameData the data to write to the save game file
 *
 * @return true if the async write was started successfully, false otherwise
 */
function bool WriteSaveGameData(byte LocalUserNum,int DeviceId,string FriendlyName,string FileName,string SaveFileName,const out array<byte> SaveGameData);

/**
 * Delegate used when the content write request has completed
 *
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 * @param LocalUserNum the user that was initiating the data write
 * @param DeviceId the device to write the same game to
 * @param FriendlyName the friendly name of the save game
 * @param FileName the file to write to inside of the content package
 * @param SaveGameData the data to write to the save game file
 */
delegate OnWriteSaveGameDataComplete(bool bWasSuccessful,byte LocalUserNum,int DeviceId,string FriendlyName,string FileName,string SaveFileName);

/**
 * Adds the delegate used to notify the gameplay code that a save game write request has completed
 *
 * @param LocalUserNum The user that was writing a save game
 * @param WriteSaveGameDataCompleteDelegate the delegate to use for notifications
 */
function AddWriteSaveGameDataComplete(byte LocalUserNum,delegate<OnWriteSaveGameDataComplete> WriteSaveGameDataCompleteDelegate);

/**
 * Clears the delegate used to notify the gameplay code that a save game write request has completed
 *
 * @param LocalUserNum The user that was writing a save game
 * @param WriteSaveGameDataCompleteDelegate the delegate to use for notifications
 */
function ClearWriteSaveGameDataComplete(byte LocalUserNum,delegate<OnWriteSaveGameDataComplete> WriteSaveGameDataCompleteDelegate);

/**
 * Deletes a player's save game data
 *
 * @param LocalUserNum the user that is deleting data
 * @param DeviceId the device to delete the same game from
 * @param FriendlyName the friendly name of the save game that was returned by enumeration
 * @param FileName the file name of the content package to delete
 *
 * @return true if the delete succeeded, false otherwise
 */
function bool DeleteSaveGame(byte LocalUserNum,int DeviceId,string FriendlyName,string FileName);

/**
 * Clears any cached save games
 *
 * @param LocalUserNum the user that is deleting data
 *
 * @return true if the clear succeeded, false otherwise
 */
function bool ClearSaveGames(byte LocalUserNum);
