/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * This interface provides accessors to the platform specific tile file downloading
 */
interface OnlineTitleFileInterface;

/**
 * Delegate fired when a file read from the network platform's title specific storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnReadTitleFileComplete(bool bWasSuccessful,string FileName);

/**
 * Starts an asynchronous read of the specified file from the network platform's
 * title specific file store
 *
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool ReadTitleFile(string FileToRead);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadTitleFileCompleteDelegate the delegate to add
 */
function AddReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ReadTitleFileCompleteDelegate the delegate to remove
 */
function ClearReadTitleFileCompleteDelegate(delegate<OnReadTitleFileComplete> ReadTitleFileCompleteDelegate);

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
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
function bool ClearDownloadedFiles();

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
function bool ClearDownloadedFile(string FileName);

/**
 * Async call to request a list of files (returned as string) from EMS
 */
function RequestTitleFileList();

/**
 * Delegate fired when the request for a list of files completes
 *
 * @param bWasSuccessful whether the request completed successfully
 * @param ResultStr contains the list of files and associated meta data
 */
delegate OnRequestTitleFileListComplete(bool bWasSuccessful, string ResultStr);

/**
 * Adds the delegate to the list to be notified when the list of requested files has been received
 *
 * @param RequestTitleFileListDelegate the delegate to add
 */
function AddRequestTitleFileListCompleteDelegate(delegate<OnRequestTitleFileListComplete> RequestTitleFileListDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param RequestTitleFileListDelegate the delegate to remove
 */
function ClearRequestTitleFileListCompleteDelegate(delegate<OnRequestTitleFileListComplete> RequestTitleFileListDelegate);