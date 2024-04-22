/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * An interface for data blog storing withing the context of the CloudSaveSystem
 */
interface CloudSaveSystemDataBlobStoreInterface;

/**
*Delegate signature for callback on GetDataBlob requests.
*/
delegate GetDataBlobCallbackDelegate(bool bWasSuccessful, string StorageID, string BlobName, const out array<byte> DataBlob, string Error);

/**
*Starts an Async request to get a data blob.  All delegates set by SetGetDataBlobCompleteDelegate will be notified on completion
*
**/
function GetDataBlob(string StorageID, string BlobName, delegate<GetDataBlobCallbackDelegate> OnGetDataBlobComplete);

/**
*Delegate signature for callback on SetDataBlob requests.
*/
delegate SetDataBlobCallbackDelegate(bool bWasSucessfull, string StorageID, string BlobName, string Error);

/**
*Starts an Async request to set blob data.  All delegates set by SetSetDataBlobCompleteDelegate will be notified on completion
*/
function SetDataBlob(string StorageID, string BlobName, const out array<byte> DataBlob, delegate<SetDataBlobCallbackDelegate> InSetDataBlobCallback);

/**
*Delegate signature for callback on SetDataBlob requests.
*/
private delegate DeleteDataBlobCallbackDelegate(bool bWasSucessfull, string StorageID, string BlobName, string Error);

/**
*Starts an Async delete of the data blob
*/
function bool DeleteDataBlob(string StorageID, string BlobName, delegate<DeleteDataBlobCallbackDelegate> InDeleteDataBlobCallback);
