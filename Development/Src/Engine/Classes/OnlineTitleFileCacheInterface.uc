/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides accessors to the platform specific tile file caching
 */
interface OnlineTitleFileCacheInterface;

/**
 * Starts an asynchronous read of the specified file from the local cache
 *
 * @param FileName the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool LoadTitleFile(string FileName);

/**
 * Delegate fired when a file read from the local cache is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnLoadTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param LoadCompleteDelegate the delegate to add
 */
function AddLoadTitleFileCompleteDelegate(delegate<OnLoadTitleFileComplete> LoadCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param LoadCompleteDelegate the delegate to remove
 */
function ClearLoadTitleFileCompleteDelegate(delegate<OnLoadTitleFileComplete> LoadCompleteDelegate);

/**
 * Starts an asynchronous write of the specified file to disk
 *
 * @param FileName the name of the file to save
 * @param LogicalName the name to associate with the physical filename
 * @param FileContents the buffer to write data from
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool SaveTitleFile(string FileName,string LogicalName,array<byte> FileContents);

/**
 * Delegate fired when a file save to the local cache is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnSaveTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Adds the delegate to the list to be notified when a requested file has been saved
 *
 * @param SaveCompleteDelegate the delegate to add
 */
function AddSaveTitleFileCompleteDelegate(delegate<OnSaveTitleFileComplete> SaveCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param SaveCompleteDelegate the delegate to remove
 */
function ClearSaveTitleFileCompleteDelegate(delegate<OnSaveTitleFileComplete> SaveCompleteDelegate);

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
function bool GetTitleFileContents(string FileName,out array<byte> FileContents);

/**
 * Determines the async state of the tile file read operation
 *
 * @param FileName the name of the file to check on
 *
 * @return the async state of the file read
 */
function EOnlineEnumerationReadState GetTitleFileState(string FileName);

/**
 * Determines the hash of the tile file that was read
 *
 * @param FileName the name of the file to check on
 *
 * @return the hash string for the file
 */
function string GetTitleFileHash(string FileName);

/**
 * Determines the hash of the tile file that was read
 *
 * @param FileName the name of the file to check on
 *
 * @return the logical name of the for the given physical filename
 */
function string GetTitleFileLogicalName(string FileName);

/**
 * Empties the set of cached files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
function bool ClearCachedFiles();

/**
 * Empties the cached data for this file if it is not being loaded/saved currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be cleared, false if it could not
 */
function bool ClearCachedFile(string FileName);

/**
 * Deletes the set of title files from disc
 *
 * @param MaxAgeSeconds if > 0 then any files older than max seconds are deleted, if == 0 then all files are deleted
 * @return true if they could be deleted, false if they could not
 */
function bool DeleteTitleFiles(float MaxAgeSeconds);

/**
 * Deletes a single file from disc
 *
 * @param FileName the name of the file to delete
 *
 * @return true if it could be deleted, false if it could not
 */
function bool DeleteTitleFile(string FileName);

