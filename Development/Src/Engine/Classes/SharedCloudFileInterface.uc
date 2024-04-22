/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * Provides the interface for sharing files already on the cloud with other users
 */
interface SharedCloudFileInterface;

/**
 * Copies the shared data into the specified buffer for the specified file
 *
 * @param SharedHandle the name of the file to read
 * @param FileContents the out buffer to copy the data into
 *
 * @return true if the data was copied, false otherwise
 */
function bool GetSharedFileContents(string SharedHandle,out array<byte> FileContents);

/**
 * Empties the set of all downloaded files if possible (no async tasks outstanding)
 *
 * @return true if they could be deleted, false if they could not
 */
function bool ClearSharedFiles();

/**
 * Empties the cached data for this file if it is not being downloaded currently
 *
 * @param SharedHandle the name of the file to read
 *
 * @return true if it could be deleted, false if it could not
 */
function bool ClearSharedFile(string SharedHandle);

/**
 * Delegate fired when a shared file read from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 */
delegate OnReadSharedFileComplete(bool bWasSuccessful,string SharedHandle);

/**
 * Starts an asynchronous read of the specified shared file from the network platform's file store
 *
 * @param SharedHandle the name of the file to read
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool ReadSharedFile(string SharedHandle);

/**
 * Adds the delegate to the list to be notified when a requested file has been read
 *
 * @param ReadSharedFileCompleteDelegate the delegate to add
 */
function AddReadSharedFileCompleteDelegate(delegate<OnReadSharedFileComplete> ReadSharedFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param ReadSharedFileCompleteDelegate the delegate to remove
 */
function ClearReadSharedFileCompleteDelegate(delegate<OnReadSharedFileComplete> ReadSharedFileCompleteDelegate);

/**
 * Delegate fired when a shared file write to the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file Write was successful or not
 * @param UserId User owning the storage
 * @param Filename the name of the file this was for
 * @param SharedHandle the handle to the shared file, may be platform dependent
 */
delegate OnWriteSharedFileComplete(bool bWasSuccessful,string UserId,string Filename,string SharedHandle);

/**
 * Starts an asynchronous write of the specified shared file to the network platform's file store
 *
 * @param UserId User owning the storage
 * @param Filename the name of the file to write
 * @param Contents data to write to the file
 *
 * @return true if the calls starts successfully, false otherwise
 */
function bool WriteSharedFile(string UserId,string Filename,const out array<byte> Contents);

/**
 * Adds the delegate to the list to be notified when a requested file has been written
 *
 * @param WriteSharedFileCompleteDelegate the delegate to add
 */
function AddWriteSharedFileCompleteDelegate(delegate<OnWriteSharedFileComplete> WriteSharedFileCompleteDelegate);

/**
 * Removes the delegate from the notify list
 *
 * @param WriteSharedFileCompleteDelegate the delegate to remove
 */
function ClearWriteSharedFileCompleteDelegate(delegate<OnWriteSharedFileComplete> WriteSharedFileCompleteDelegate);
