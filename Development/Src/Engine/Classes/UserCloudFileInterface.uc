/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides access to per user cloud file storage
 */
interface UserCloudFileInterface;

/**
 * Copies the file data into the specified buffer for the specified file
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
function bool GetFileContents(string UserId,string FileName,out array<byte> FileContents);

/**
 * Empties the set of downloaded files if possible (no async tasks outstanding)
 *
 * @param UserId User owning the storage
 *
 * @return true if they could be deleted, false if they could not
 */
function bool ClearFiles(string UserId);

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param UserId User owning the storage
 * @param FileName the name of the file to remove from the cache
 *
 * @return true if it could be deleted, false if it could not
 */
function bool ClearFile(string UserId,string FileName);

/**
 * Delegate fired when the list of files has been returned from the network store
 *
 * @param bWasSuccessful whether the file list was successful or not
 * @param UserId User owning the storage
 *
 */
delegate OnEnumerateUserFilesComplete(bool bWasSuccessful,string UserId);

/**
 * Requests a list of available User files from the network store
 *
 * @param UserId User owning the storage
 *
 */
function EnumerateUserFiles(string UserId);

/**
 * Adds the delegate to the list to be notified when all files have been enumerated
 *
 * @param EnumerateUserFileCompleteDelegate the delegate to add
 *
 */
function AddEnumerateUserFileCompleteDelegate(delegate<OnEnumerateUserFilesComplete> EnumerateUserFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param EnumerateUserFileCompleteDelegate the delegate to remove
 *
 */
function ClearEnumerateUserFileCompleteDelegate(delegate<OnEnumerateUserFilesComplete> EnumerateUserFileCompleteDelegate);

/**
 * Returns the list of User files that was returned by the network store
 *
 * @param UserId User owning the storage
 * @param UserFiles out array of file metadata
 *
 */
function GetUserFileList(string UserId,out array<EmsFile> UserFiles);

/**
 * Delegate fired when a user file read from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 *
 */
delegate OnReadUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Starts an asynchronous read of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool ReadUserFile(string UserId,string FileName);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadUserFileCompleteDelegate the delegate to add
 */
function AddReadUserFileCompleteDelegate(delegate<OnReadUserFileComplete> ReadUserFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ReadUserFileCompleteDelegate the delegate to remove
 */
function ClearReadUserFileCompleteDelegate(delegate<OnReadUserFileComplete> ReadUserFileCompleteDelegate);

/**
 * Delegate fired when a user file write to the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file Write was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 *
 */
delegate OnWriteUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Starts an asynchronous write of the specified user file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToWrite the name of the file to write
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool WriteUserFile(string UserId,string FileName,const out array<byte> FileContents);

/**
 * Adds the delegate to the list to be notified when a requested file has been written
 *
 * @param WriteUserFileCompleteDelegate the delegate to add
 */
function AddWriteUserFileCompleteDelegate(delegate<OnWriteUserFileComplete> WriteUserFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param WriteUserFileCompleteDelegate the delegate to remove
 */
function ClearWriteUserFileCompleteDelegate(delegate<OnWriteUserFileComplete> WriteUserFileCompleteDelegate);

/**
 * Delegate fired when a user file delete from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param UserId User owning the storage
 * @param FileName the name of the file this was for
 */
delegate OnDeleteUserFileComplete(bool bWasSuccessful,string UserId,string FileName);

/**
 * Starts an asynchronous delete of the specified user file from the network platform's file store
 *
 * @param UserId User owning the storage
 * @param FileToRead the name of the file to read
 * @param bShouldCloudDelete whether to delete the file from the cloud
 * @param bShouldLocallyDelete whether to delete the file locally
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool DeleteUserFile(string UserId,string FileName,bool bShouldCloudDelete,bool bShouldLocallyDelete);

/**
 * Adds the delegate to the list to be notified when a requested file has been deleted
 *
 * @param DeleteUserFileCompleteDelegate the delegate to add
 */
function AddDeleteUserFileCompleteDelegate(delegate<OnDeleteUserFileComplete> DeleteUserFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param DeleteUserFileCompleteDelegate the delegate to remove
 */
function ClearDeleteUserFileCompleteDelegate(delegate<OnDeleteUserFileComplete> DeleteUserFileCompleteDelegate);

/** clears all delegates for e.g. end of level cleanup */
function ClearAllDelegates();
