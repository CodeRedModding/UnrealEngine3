/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * An interface for data blob storing withing the context of the CloudSaveSystem
 */
class UserCloudFileCloudSaveSystemDataBlobStore extends Object
	implements(CloudSaveSystemDataBlobStoreInterface)
	dependson(CloudSaveSystemDataBlobStoreInterface)
	dependson(UserCloudFileInterface);

//==============================================================================
var private transient UserCloudFileInterface UserCloudFile;
var private transient delegate<GetDataBlobCallbackDelegate> GetDataBlobCallback;
var private transient delegate<SetDataBlobCallbackDelegate> SetDataBlobCallback;
var private transient delegate<DeleteDataBlobCallbackDelegate> DeleteDataBlobCallback;
//==============================================================================
delegate GetDataBlobCallbackDelegate(bool bWasSuccessful, string StorageID, string BlobName, out array<byte> DataBlob, string Error);
delegate SetDataBlobCallbackDelegate(bool bWasSucessfull, string StorageID, string BlobName, string Error);
private delegate DeleteDataBlobCallbackDelegate(bool bWasSucessfull, string StorageID, string BlobName, string Error);
//==============================================================================

final function Init(UserCloudFileInterface InUserCloudFile)
{
	UserCloudFile = InUserCloudFile;
}

function GetDataBlob(string StorageID, string BlobName, delegate<GetDataBlobCallbackDelegate> InGetDataBlobCallback)
{
	local bool ErrorOccured;
	local string Error;
	local array<Byte> EmptyBuffer;

	ErrorOccured = true;

	if (UserCloudFile == None)
	{
		Error = "GetDataBlob::UserCloudFileInterface instance is null";
	}
	else if (GetDataBlobCallback != None)
	{
		Error = "GetDataBlob::GetDataBlob operation already active";
	}
	else if (InGetDataBlobCallback == None)
	{
		Error = "GetDataBlob::InGetDataBlobCallback cannot be none";
	}
	else
	{
		UserCloudFile.AddReadUserFileCompleteDelegate(OnReadUserFileComplete);
		GetDataBlobCallback = InGetDataBlobCallback;
		if (UserCloudFile.ReadUserFile(StorageID, BlobName))
		{
			ErrorOccured = false;
		}
		else
		{
			Error = "GetDataBlob::Unknown error starting read of user file from cloud";
			GetDataBlobCallback = None;
			UserCloudFile.ClearReadUserFileCompleteDelegate(OnReadUserFileComplete);
		}

	}

	if (ErrorOccured && InGetDataBlobCallback != None)
	{
		InGetDataBlobCallback(false/*bWasSuccessful*/, StorageID, BlobName, EmptyBuffer, Error);
	}
}

final private function OnReadUserFileComplete(bool bWasSuccessful,string UserId,string FileName)
{
	local delegate<GetDataBlobCallbackDelegate> Callback;
	local array<byte> FileContents;
	local bool LocalResult;

	UserCloudFile.ClearReadUserFileCompleteDelegate(OnReadUserFileComplete);

	LocalResult = bWasSuccessful;
	if (bWasSuccessful)
	{
		if(!UserCloudFile.GetFileContents(UserId, FileName, FileContents))
		{
			LocalResult = false;
		}
	}

	if (GetDataBlobCallback != None)
	{
		Callback = GetDataBlobCallback;
		GetDataBlobCallback = None;
		Callback(LocalResult, UserId, FileName, FileContents, "");
	}
}

function SetDataBlob(string StorageID, string BlobName, const out array<byte> DataBlob, delegate<SetDataBlobCallbackDelegate> InSetDataBlobCallback)
{
	local bool ErrorOccured;
	local string Error;

	ErrorOccured = true;
	if (UserCloudFile == None)
	{
		Error = "SetDataBlob::UserCloudFileInterface instance is null";
	}
	else if (SetDataBlobCallback != None)
	{
		Error = "SetDataBlob::SetDataBlob operation already active";
	}
	else if (InSetDataBlobCallback == None)
	{
		Error="SetDataBlob::InSetDataBlobCallback cannot be none";
	}
	else
	{
		UserCloudFile.AddWriteUserFileCompleteDelegate(OnWriteUserFileComplete);
		SetDataBlobCallback = InSetDataBlobCallback;
		if (UserCloudFile.WriteUserFile(StorageID, BlobName, DataBlob))
		{
			ErrorOccured = false;
		}
		else
		{
			Error="SetDataBlob::Unknown error starting write of user file to cloud";
			SetDataBlobCallback = None;
			UserCloudFile.ClearWriteUserFileCompleteDelegate(OnWriteUserFileComplete);
		}

	}

	if (ErrorOccured && InSetDataBlobCallback != None)
	{
		InSetDataBlobCallback(false/*bWasSuccessful*/, StorageID, BlobName, Error);
	}
}

final private function OnWriteUserFileComplete(bool bWasSuccessful,string UserId,string FileName)
{
	local delegate<SetDataBlobCallbackDelegate> Callback;

	UserCloudFile.ClearWriteUserFileCompleteDelegate(OnWriteUserFileComplete);

	if(SetDataBlobCallback != None)
	{
		Callback = SetDataBlobCallback;
		SetDataBlobCallback = None;
		Callback(bWasSuccessful, UserId, FileName, "");
	}
}

function bool DeleteDataBlob(string StorageID, string BlobName, delegate<DeleteDataBlobCallbackDelegate> InDeleteDataBlobCallback)
{
	local bool RValue;
	RValue = false;

	if (UserCloudFile != None && DeleteDataBlobCallback == None) 
	{
		UserCloudFile.AddDeleteUserFileCompleteDelegate(OnDeleteUserFileComplete);
		DeleteDataBlobCallback = InDeleteDataBlobCallback;
		RValue = UserCloudFile.DeleteUserFile(StorageID, BlobName, true/*ShouldCloudDelete*/, true/*ShouldLocallyDelete*/);
		if (!RValue)
		{
			DeleteDataBlobCallback = None;
			UserCloudFile.ClearDeleteUserFileCompleteDelegate(OnDeleteUserFileComplete);
		}
	}

	return RValue;
}

private function OnDeleteUserFileComplete(bool bWasSuccessful,string UserId,string FileName)
{
	local delegate<DeleteDataBlobCallbackDelegate> Callback;

	UserCloudFile.ClearDeleteUserFileCompleteDelegate(OnDeleteUserFileComplete);

	if (DeleteDataBlobCallback != None)
	{
		Callback = DeleteDataBlobCallback;
		DeleteDataBlobCallback = None;
		Callback(bWasSuccessful, UserId, FileName, "");
	}
}