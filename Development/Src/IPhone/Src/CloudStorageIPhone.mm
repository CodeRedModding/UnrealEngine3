/*=============================================================================
	IPhoneFacebook.cpp: IPhone specific Facebook integration.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EnginePlatformInterfaceClasses.h"
#import "IPhoneAppDelegate.h"
#import "IPhoneAsyncTask.h"
#include "AES.h"
#include "IPhoneObjCWrapper.h"

// only allow this intrinsic class if we have icloud support on the device (OS 5 or later)
#if WITH_IOS_5

/** Generic UIDocument subclass for storing data the game has created */
@interface FCloudDocument : UIDocument
{
	/** This is the data the game has created and will be saved to disk/cloud, or has been loaded from it */
    NSData* GameData;

	/** When handling conflicts, this is the FileVersion helper for this document (this is only used during conflict resolution, not for normal document loading) */
	NSFileVersion* ConflictVersion;

	/** If YES, then the next time the document is saved, use Create instead of Overwrite */
	BOOL bFirstSave;
};

@property (retain) NSData* Data;
@property (retain) NSFileVersion* ConflictVersion;
@property (assign, nonatomic) BOOL bFirstSave;
@end


@implementation FCloudDocument
@synthesize Data, ConflictVersion, bFirstSave;

- (id) initWithFileURL:(NSURL*)InURL
{
    self = [super initWithFileURL:InURL];

    return self;
}

- (void)dealloc
{
	self.Data = nil;
	self.ConflictVersion = nil;

	[super dealloc];
}

- (id)contentsForType:(NSString *)typeName error:(NSError **)outError
{
	// copy the data
	NSMutableData* EncryptedBuffer = [NSMutableData dataWithData:self.Data];
	
	// pad it to a multiple of 16
	INT OldLength = [EncryptedBuffer length];
	INT NewLength = Align(OldLength, 16);
	[EncryptedBuffer increaseLengthBy:NewLength - OldLength];

	// encrypt in place
	appEncryptData((BYTE*)[EncryptedBuffer bytes], [EncryptedBuffer length]);

    return EncryptedBuffer;
}

- (BOOL)loadFromContents:(id)contents ofType:(NSString *)typeName error:(NSError **)outError
{
	// copy the data
	NSData* DecryptedBuffer = contents;
	DecryptedBuffer = [NSMutableData dataWithData:DecryptedBuffer];

	// decrypt in place
	appDecryptData((BYTE*)[DecryptedBuffer bytes], [DecryptedBuffer length]);

	self.Data = DecryptedBuffer;
    return YES;
}


@end






@interface FCloudSupport : NSObject
{
	id CloudObserver;
}

/**
 * Notification called when a cloud key/value has changed
 */
- (void)CloudKeyValueChanged:(NSNotification*)Notification;

@end


/**
 * iCloud fallback for the Cloud interface that uses local storage
 */
class UCloudStorageBaseIPhone : public UCloudStorageBase
{
	DECLARE_CLASS_INTRINSIC(UCloudStorageBaseIPhone, UCloudStorageBase, 0, IPhoneDrv)

	/**
	 * Initiate reading a key/value pair from NSUserDefaults. 
	 * 
	 * @param KeyName String name of the key to retrieve
	 * @param Type Type of data to retrieve 
	 * @param Value The resulting value of the key we just read (supports multiple types of value)
	 *
	 * @return True if successful
	 */
	virtual UBOOL ReadKeyValue(const FString& KeyName, BYTE Type, struct FPlatformInterfaceDelegateResult& Value)
	{
		FString CloudKeyName = FString::Printf(TEXT("CLOUD::%s"), *KeyName);

		ANSICHAR ValString[256];
		IPhoneLoadUserSetting(TCHAR_TO_ANSI(*CloudKeyName), ValString, ARRAY_COUNT(ValString) - 1);

		// fill out the result value
		Value.bSuccessful = TRUE;
		Value.Data.Type = Type;
		Value.Data.DataName = FName(*KeyName);
		switch (Type)
		{
			case PIDT_Int:
				Value.Data.IntValue = (INT)strtol(ValString, NULL, 0);
				break;
			case PIDT_Float:
				Value.Data.FloatValue = (FLOAT)strtod(ValString, NULL);
				break;
			case PIDT_String:
				Value.Data.StringValue = FString(ValString);
				break;
			case PIDT_Object:
				{
					debugf(TEXT("CONVERT NSDATA TO UOBJECT HERE"));
				}
				break;
		};

		return TRUE;
	}

	/**
	 * Write a key/value pair to NSUserDefaults
	 *
	 * @param KeyName String name of the key to write
	 * @param Value The type and value to write
	 *
	 * @return True if successful
	 */
	virtual UBOOL WriteKeyValue(const FString& KeyName, const FPlatformInterfaceData& Value)
	{
		FString CloudKeyName = FString::Printf(TEXT("CLOUD::%s"), *KeyName);

		FString ValString;
		switch (Value.Type)
		{
			case PIDT_Int:
				ValString = FString::Printf(TEXT("%i"), Value.IntValue);
				break;
			case PIDT_Float:
				ValString = FString::Printf(TEXT("%i"), Value.FloatValue);
				break;
			case PIDT_String:
				ValString = Value.StringValue;
				break;
			case PIDT_Object:
				{
					debugf(TEXT("CONVERT NSDATA TO UOBJECT HERE"));
				}
				break;
		};

		IPhoneSaveUserSetting(TCHAR_TO_ANSI(*CloudKeyName), TCHAR_TO_ANSI(*ValString));

		return TRUE;
	}
};


/**
 * iOS subclass of generic Cloud Storage API for using local storage alongside iCloud
 */
class ULocalStorageIPhone : public UCloudStorageBaseIPhone
{
	DECLARE_CLASS_INTRINSIC(ULocalStorageIPhone, UCloudStorageBaseIPhone, 0, IPhoneDrv)
	
	virtual void Init()
	{
		// Do not call super class, which marks files as needing to upgrade to cloud storage
	}
};


FPlatformInterfaceDelegateResult HACK_Result;
INT NumPendingWrites = 0;

// Used to block re-entry into QueryForCloudDocuments, as that can cause problems
UBOOL bQueryInProgress = FALSE;

/**
 * iCloud subclass of generic Cloud Storage API
 */
class UCloudStorageIPhone : public UCloudStorageBaseIPhone
{
	DECLARE_CLASS_INTRINSIC(UCloudStorageIPhone, UCloudStorageBaseIPhone, 0, IPhoneDrv)

	/**
	 * Perform any initialization
	 */
	virtual void Init()
	{
		// create some arrays
		Documents = [[NSMutableArray alloc] initWithCapacity:0];
		PendingConflicts = [[NSMutableArray alloc] initWithCapacity:0];
		ConflictVersions = [[NSMutableArray alloc] initWithCapacity:0];

		// cloud support object (this will leak, that's okay)
		CloudSupport = [[FCloudSupport alloc] init];

		// listen for changes
		[[NSNotificationCenter defaultCenter] addObserver:CloudSupport
													selector:@selector(CloudKeyValueChanged:)
														name:NSUbiquitousKeyValueStoreDidChangeExternallyNotification
													object:nil];

		[[NSNotificationCenter defaultCenter] addObserverForName:UIDocumentStateChangedNotification
															object:nil
															queue:nil
														usingBlock:^(NSNotification* Notification)
			{
				// if the document is undergoing conflict resolution, 
				FCloudDocument* UpdatedDoc = Notification.object;
				if (UpdatedDoc.ConflictVersion != nil)
				{
					return;
				}

				NSLog(@"Document [%@] changed state to %d\n", UpdatedDoc.fileURL, UpdatedDoc.documentState);
				if (UpdatedDoc.documentState & UIDocumentStateInConflict)
				{
					// @todo: check what thread this is running, do we need the synchronize?
					@synchronized(PendingConflicts)
					{
						// don't hadd a document multiple times
						if ([PendingConflicts containsObject:UpdatedDoc] == NO)
						{
							[PendingConflicts addObject:UpdatedDoc];
							
							if ([PendingConflicts count] == 1)
							{
								// kick off processing a conflict if we just added the first one
								ProcessNextConflict();
							}
						}
					}

				}
			}];

		// kick off a synchronization operation to get latest values
		[[NSUbiquitousKeyValueStore defaultStore] synchronize];

		// kick off the document synchronization (which can sometimes take several seconds)
		QueryForCloudDocuments();
	}

	void ClearExistingDocuments()
	{
		// cache the old ones to close down
		for (UIDocument* Doc in Documents)
		{
 			// close and release when done
 			[Doc closeWithCompletionHandler:^(BOOL bSuccess)
 				{
 				}];
		}
		
		// done with the array
		[Documents removeAllObjects];
	}

	/**
	 * @return True if we are actually using local cloud storage emulation
	 */
	virtual UBOOL IsUsingLocalStorage()
	{
		return FALSE;
	}

	/**
	 * Initiate reading a key/value pair from cloud storage. A CSD_KeyValueReadComplete
	 * delegate will be called when it completes
	 * 
	 * @param KeyName String name of the key to retrieve
	 * @param Type Type of data to retrieve 
	 * @param SerializedObj If you are reading an object (PIDT_Object), it will de-serialize the binary blob into this object
	 *
	 * @return True if successful
	 */
	virtual UBOOL ReadKeyValue(const FString& KeyName, BYTE Type, struct FPlatformInterfaceDelegateResult& Value)
	{
		NSUbiquitousKeyValueStore* Store = [NSUbiquitousKeyValueStore defaultStore];

		// convert key name to an NSString (we should extend NSString with a from FString function)
		NSString* KeyStr = [NSString stringWithCString:TCHAR_TO_UTF8(*KeyName) encoding:NSUTF8StringEncoding];

		Value.bSuccessful = TRUE;
		Value.Data.Type = Type;
		Value.Data.DataName = FName(*KeyName);

		switch (Type)
		{
			case PIDT_Int:
				// chop down to 32-bit for script access
				Value.Data.IntValue = (INT)[Store longLongForKey:KeyStr];
				break;
			case PIDT_Float:
				// chop down to 32-bit for script access
				Value.Data.FloatValue = (FLOAT)[Store doubleForKey:KeyStr];
				break;
			case PIDT_String:
				// convert NSString to FString
				Value.Data.StringValue = FString([Store stringForKey:KeyStr]);
				break;
			case PIDT_Object:
				{
					//NSData* Data = [Store dataForKey:KeyStr];
					debugf(TEXT("CONVERT NSDATA TO UOBJECT HERE"));
				}
				break;
		};

		if (!bSuppressDelegateCalls)
		{
			// tell script we've read the data
			CallDelegates(CSD_KeyValueReadComplete, Value);
		}

		return TRUE;
	}

	/**
	 * Write a key/value pair to the cloud
	 *
	 * @param KeyName String name of the key to write
	 * @param Value The type and value to write
	 *
	 * @return True if successful
	 */
	virtual UBOOL WriteKeyValue(const FString& KeyName, const FPlatformInterfaceData& Value)
	{
		//Write the kvs value to the NSUserDefaults for conflict resolution
		//to allow local data to be used when resolving against cloud stored values
		Super::WriteKeyValue(KeyName, Value);

		NSUbiquitousKeyValueStore* Store = [NSUbiquitousKeyValueStore defaultStore];

		// convert key name to an NSString (we should extend NSString with a from FString function)
		NSString* KeyStr = [NSString stringWithCString:TCHAR_TO_UTF8(*KeyName) encoding:NSUTF8StringEncoding];

		switch (Value.Type)
		{
			case PIDT_Int:
				[Store setLongLong:Value.IntValue forKey:KeyStr];
				break;
			case PIDT_Float:
				[Store setDouble:Value.FloatValue forKey:KeyStr];
				break;
			case PIDT_String:
				// convert NSString to FString
				[Store setString:[NSString stringWithCString:TCHAR_TO_UTF8(*Value.StringValue) encoding:NSUTF8StringEncoding] forKey:KeyStr];
				break;
			case PIDT_Object:
				{
//					NSData* Data = [Store dataForKey:KeyStr];
					debugf(TEXT("CONVERT NSDATA TO UOBJECT HERE"));
				}
				break;
		};

		// publish immediately
		if ([Store synchronize] == NO)
		{
			// if the synchronize failed, then we know the write as failed, and we won't call the delegates
			return FALSE;
		}

		if (!bSuppressDelegateCalls)
		{
			FPlatformInterfaceDelegateResult Result(EC_EventParm);
			Result.bSuccessful = TRUE;
			Result.Data = Value;
		
			// tell script we've written the data, successfully
			CallDelegates(CSD_KeyValueWriteComplete, Result);
		}

		return TRUE;
	}




	/**
	 * Kick off an async query of documents that exist in the cloud. If any documents have
	 * already been retrieved, this will flush those documents, and refresh the set. A
	 * CSD_DocumentQueryComplete delegate will be called when it completes  (if this 
	 * function returns true). Then use GetNumCloudDocuments() and GetCloudDocumentName() 
	 * to get the information about any existing documents.
	 *
	 * @return True if successful
	 */
	virtual UBOOL QueryForCloudDocuments()
	{
		// Don't allow new queries while one is already in progress
		if( bQueryInProgress )
		{
			debugf(TEXT("Query already in progress"));
			return FALSE;
		}
		bQueryInProgress = TRUE;

		// the query needs a run loop, so do it on main thread
		[CloudSupport performSelectorOnMainThread:@selector(MainThreadQueryForDocuments) withObject:nil waitUntilDone:NO];

		return TRUE;
	}

	/**
	 * Check if there are local files on disk. These would come from systems that allow both
	 * cloud files and local files (e.g. iOS users not signed into iCloud).
	 * HandleLocalDocument() will be called for each file this function finds, which you can
	 * override to handle each document.
	 *
	 * @return True if there were any documents
	 */
	virtual UBOOL UpgradeLocalStorageToCloud(const TScriptInterface<class ICloudStorageUpgradeHelper>& UpgradeHelper, UBOOL bForceSearchAgain)
	{
		// Check if we have already upgraded the local system files
		FPlatformInterfaceDelegateResult LocalValue;
		FString NeedsUpgradeKeyName = TEXT("UpgradeKey");
		if( !bForceSearchAgain && Super::ReadKeyValue(NeedsUpgradeKeyName, PIDT_Int, LocalValue) )
		{
			if( LocalValue.Data.IntValue == 0 )
			{
				// We've already done this, no need to do it again
				return FALSE;
			}
		}

		// make sure we don't send any delegate calls while performing some cloud upgrading
		bSuppressDelegateCalls = TRUE;

		Super::QueryForCloudDocuments();
		
		// if we have any legacy saves, we need to upgrade them 
		INT NumLegacySaves = Super::GetNumCloudDocuments();
		if (NumLegacySaves > 0)
		{
			// @todo: Show an alert message about what we are doing!
			debugf(TEXT("Converting %d legacy saves to iCloud saves!"), NumLegacySaves);
			for (INT SaveIndex = 0; SaveIndex < NumLegacySaves; SaveIndex++)
			{
				FString DocName = Super::GetCloudDocumentName(SaveIndex);
				INT bShouldCreateCloudDoc = TRUE;
				INT bShouldDeleteLocalFile = TRUE;
				UpgradeHelper->eventHandleLocalDocument(DocName, bShouldCreateCloudDoc, bShouldDeleteLocalFile);
				if (bShouldCreateCloudDoc == TRUE)
				{
					// read the document
					if (Super::ReadCloudDocument(SaveIndex, FALSE))
					{
						// get the bytes
						TArray<BYTE> SaveData;
						Super::ParseDocumentAsBytes(SaveIndex, SaveData, FALSE);

						// create a cloud document
						INT CloudIndex = CreateCloudDocument(DocName);
						SaveDocumentWithBytes(CloudIndex, SaveData);
						WriteCloudDocument(CloudIndex);
					}
				}

				if (bShouldDeleteLocalFile == TRUE)
				{
					// TODO delete the local file
					//Super::DeleteCloudDocument(SaveIndex);
				}
			}
		}

		// move over existing key/value pairs as well (read from an ini file)
		TArray<FString> CloudKeys;
		GConfig->GetArray( TEXT("CloudStorage.CloudUpgradeKeys"), TEXT("CloudKey"), CloudKeys, GEngineIni );
		UpgradeHelper->eventGetCloudUpgradeKeys(CloudKeys);

		// iterate over all the keys specified in the ini file, and write them out to the cloud
		FPlatformInterfaceData CloudValue;
		for(INT i=0; i<CloudKeys.Num(); i++)
		{
			FString& Entry = CloudKeys(i);

			// Trim whitespace at the beginning.
			Entry = Entry.Trim();
			// Remove brackets.
			Entry = Entry.Replace( TEXT("("), TEXT("") );
			Entry = Entry.Replace( TEXT(")"), TEXT("") );

			// Parse the key name
			FString CloudKeyName;
			Parse( *Entry, TEXT("KeyName="), CloudKeyName );

			// Parse the key type
			CloudValue.Type = PIDT_String;
			FString CloudKeyType;
			if( Parse( *Entry, TEXT("KeyType="), CloudKeyType ) )
			{
				if( CloudKeyType == TEXT("PIDT_Int") )
					CloudValue.Type = PIDT_Int;
				else if( CloudKeyType == TEXT("PIDT_Float") )
					CloudValue.Type = PIDT_Float;
				else if( CloudKeyType == TEXT("PIDT_Object") )
					CloudValue.Type = PIDT_Object;
				else
					CloudValue.Type = PIDT_String;
			}

			// read from local storage
			if( Super::ReadKeyValue(CloudKeyName, CloudValue.Type, LocalValue) )
			{
				// fill in the correct value based on type
				switch( CloudValue.Type )
				{
					case PIDT_Int:
						CloudValue.IntValue = LocalValue.Data.IntValue;
						break;
					case PIDT_Float:
						CloudValue.FloatValue = LocalValue.Data.FloatValue;
						break;
					case PIDT_String:
						CloudValue.StringValue = LocalValue.Data.StringValue;
						break;
					case PIDT_Object:
						CloudValue.ObjectValue = LocalValue.Data.ObjectValue;
						break;
				}

				INT bShouldMoveToCloud = TRUE;
				INT bShouldDeleteLocalValue = FALSE;
				UpgradeHelper->eventHandleLocalKeyValue(CloudKeyName, CloudValue, bShouldMoveToCloud, bShouldDeleteLocalValue);

				// write to the cloud
				if( bShouldMoveToCloud == TRUE )
				{
					WriteKeyValue(CloudKeyName, CloudValue);
				}

				// Destroy local value
				if( bShouldDeleteLocalValue == TRUE )
				{
					// TODO - actually remove the key/value
					CloudValue.IntValue = 0;
					CloudValue.FloatValue = 0;
					CloudValue.StringValue = TEXT("");
					CloudValue.ObjectValue = NULL;
					Super::WriteKeyValue(CloudKeyName, CloudValue);
				}
			}
		}

		// Make a note on the local system that we don't need to do this again
		FPlatformInterfaceData NeedsUpgradeValue;
		NeedsUpgradeValue.IntValue = 0;
		NeedsUpgradeValue.Type = PIDT_Int;
		Super::WriteKeyValue(NeedsUpgradeKeyName, NeedsUpgradeValue);

		// and back to normal
		bSuppressDelegateCalls = FALSE;

		return NumLegacySaves > 0;
	}

	/**
	 * @return the number of documents that are known to exist in the cloud
	 */
	virtual INT GetNumCloudDocuments(UBOOL bIsForConflict)
	{
		// which array are we looking up in?
		NSMutableArray* DocArray = bIsForConflict ? ConflictVersions : Documents;

		return [DocArray count];
	}

	/**
	 * @return the name of the given cloud by index (up to GetNumCloudDocuments() - 1)
	 */
	virtual FString GetCloudDocumentName(INT Index)
	{
		// verify the input
		if (Index < 0 || Index >= [Documents count] || [Documents objectAtIndex:Index] == nil)
		{
			return FString(TEXT(""));
		}

		// pull apart the URL to get the filename
		FCloudDocument* Doc = [Documents objectAtIndex:Index];
  		return FString([Doc.fileURL lastPathComponent]);
	}

	/**
	 * Create a new document in the cloud (uninitialized, unsaved, use the Write/Save functions)
	 *
	 * @param Filename Filename for the cloud document (with any extension you want)
	 * 
	 * @return Index of the new document, or -1 on failure
	 */
	INT CreateCloudDocument(const FString& Filename)
	{
		// calculate the directory to save cloud documents to
		NSFileManager* FM = [NSFileManager defaultManager];

		// using nil for the ubiquity identifier will make it look in the .plist/entitlements, and use the first document id
		NSURL* CloudURL = [FM URLForUbiquityContainerIdentifier:nil];
		if (CloudURL == nil)
		{
			debugf(TEXT("Unable to get URLForUbiquityContainerIdentifier for specified CloudContainerID"));
			return -1;
		}

		// we always want to save into the Documents subdirectory, so that iCloud can display it individually
		NSURL* DocURL = [CloudURL URLByAppendingPathComponent:@"Documents"];
		
		// make sure the Documents directory exists
		if (![FM fileExistsAtPath:[DocURL path]])
		{
			[FM createDirectoryAtURL:DocURL withIntermediateDirectories:YES attributes:nil error:nil];
		}

		// make the final URL for the cloud doc
		NSURL* FullURL = [DocURL URLByAppendingPathComponent:[NSString stringWithCString:TCHAR_TO_UTF8(*Filename) encoding:NSUTF8StringEncoding]];

		// create a new empty doc
		FCloudDocument* NewDoc = [[FCloudDocument alloc] initWithFileURL:FullURL];
NSLog(@"Creating document at %@", FullURL);
		// this is a new document, next save is a create, not overwrite
		NewDoc.bFirstSave = YES;

		// add the document to the list, and return it's index
		[Documents addObject:NewDoc];
		// array is now the owner
		[NewDoc release];

		// return the index (one less than size)
		return [Documents count] - 1;
	}

	/**
	 * Reads a document into memory (or whatever is needed so that the ParseDocumentAs* functions
	 * operate synchronously without stalling the game). A CSD_DocumentReadComplete delegate
	 * will be called (if this function returns true).
	 *
	 * @param Index index of the document to read
	 *
	 * @param True if successful
	 */
	virtual UBOOL ReadCloudDocument(INT Index, UBOOL bIsForConflict)
	{
		// which array are we looking up in?
		NSMutableArray* DocArray = bIsForConflict ? ConflictVersions : Documents;

		// verify the input
		if (Index >= [DocArray count] || [DocArray objectAtIndex:Index] == nil)
		{
			return FALSE;
		}

		FCloudDocument* Doc = [DocArray objectAtIndex:Index];
		[Doc openWithCompletionHandler:^(BOOL bSuccess)
			{
				if (!bSuppressDelegateCalls)
				{
					IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
					AsyncTask.GameThreadCallback = ^ UBOOL (void)
					{
						// fire the delegate call now that the document is ready for use
						HACK_Result = FPlatformInterfaceDelegateResult(EC_EventParm);
						HACK_Result.bSuccessful = bSuccess;
						// which document is this?
						HACK_Result.Data.Type = PIDT_Int;
						HACK_Result.Data.IntValue = Index;
						CallDelegates(CSD_DocumentReadComplete, HACK_Result);
						return TRUE;
					};
					[AsyncTask FinishedTask];
				}
			}];
		
		return TRUE;
	}

	/**
	 * Once a document has been read in, use this to return a string representing the 
	 * entire document. This should only be used if SaveDocumentWithString was used to
	 * generate the document.
	 *
	 * @param Index index of the document to read
	 *
	 * @param The document as a string. It will be empty if anything went wrong.
	 */
	virtual FString ParseDocumentAsString(INT Index, UBOOL bIsForConflict)
	{
		// which array are we looking up in?
		NSMutableArray* DocArray = bIsForConflict ? ConflictVersions : Documents;

		// verify the input
		if (Index >= [DocArray count] || [DocArray objectAtIndex:Index] == nil)
		{
			return FString(TEXT(""));
		}

		// make sure the document has been read in already
		FCloudDocument* Doc = [DocArray objectAtIndex:Index];
		if (Doc.Data == nil)
		{
			return FString(TEXT(""));
		}

		// convert to a string and return it
		NSString* DocumentString = [[[NSString alloc] initWithData:Doc.Data encoding:NSUTF8StringEncoding] autorelease];
		return FString(DocumentString);
	}

	/**
	 * Once a document has been read in, use this to return a string representing the 
	 * entire document. This should only be used if SaveDocumentWithString was used to
	 * generate the document.
	 *
	 * @param Index index of the document to read
	 * @param ByteData The array of bytes to be filled out
	 *
	 * @return The bytes from the file. It will be empty if anything went wrong
	 */
	void ParseDocumentAsBytes(INT Index, TArray<BYTE>& ByteData, UBOOL bIsForConflict)
	{
		// make sure a clean slate
		ByteData.Empty();

		// which array are we looking up in?
		NSMutableArray* DocArray = bIsForConflict ? ConflictVersions : Documents;

		// verify the input
		if (Index >= [DocArray count] || [DocArray objectAtIndex:Index] == nil)
		{
			return;
		}
		
		// make sure the document has been read in already
		FCloudDocument* Doc = [DocArray objectAtIndex:Index];
		if (Doc.Data == nil)
		{
			return;
		}

		// allocate space
		ByteData.Add([Doc.Data length]);

		// copy from document to UE3-usable array
		appMemcpy(ByteData.GetData(), [Doc.Data bytes], ByteData.Num());
	}

	/**
	 * Writes a document that has been already "saved" using the SaveDocumentWith* functions.
	 * A CSD_DocumentWriteComplete delegate will be called (if this function returns true).
	 *
	 * @param Index index of the document to read
	 *
	 * @param True if successful
	 */
	virtual UBOOL WriteCloudDocument(INT Index)
	{
		// verify the input
		if (Index < 0 || Index >= [Documents count] || [Documents objectAtIndex:Index] == nil)
		{
			return FALSE;
		}

		// make sure the block below uses the current value, not the value when the block runs
		UBOOL bStackSuppressDelegateCalls = bSuppressDelegateCalls;

		NumPendingWrites++;

		FCloudDocument* Doc = [Documents objectAtIndex:Index];
		UIDocumentSaveOperation Op = Doc.bFirstSave ? UIDocumentSaveForCreating : UIDocumentSaveForOverwriting;
		[Doc saveToURL:Doc.fileURL forSaveOperation:Op completionHandler:^(BOOL bSuccess)
			{
NSLog(@"Document was written out success = %d", bSuccess);
				// we are no longer saving the first time
				Doc.bFirstSave = NO;

				NumPendingWrites--;

				if (!bStackSuppressDelegateCalls)
				{
					IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
					AsyncTask.GameThreadCallback = ^ UBOOL (void)
					{
						// fire the delegate call now that the document is ready for use
						HACK_Result = FPlatformInterfaceDelegateResult(EC_EventParm);
						HACK_Result.bSuccessful = bSuccess;
						// which document is this?
						HACK_Result.Data.Type = PIDT_Int;
						HACK_Result.Data.IntValue = Index;
						CallDelegates(CSD_DocumentWriteComplete, HACK_Result);
						return TRUE;
					};
					[AsyncTask FinishedTask];
				}
			}];
		
		return TRUE;
	}

	/**
	 * Checks whether there are any pending writes.
	 * Sometimes cloud services can get frazzled if you call ReadCloudDocument while still writing.
	 */
	virtual UBOOL IsStillWritingFiles()
	{
		return NumPendingWrites == 0;
	}

	/**
	 * Prepare a document for writing to the cloud with a string as input data. This is
	 * synchronous
	 *
	 * @param Index index of the document to save
	 * @param StringData The data to put into the document
	 *
	 * @param True if successful
	 */
	virtual UBOOL SaveDocumentWithString(INT Index, const FString& StringData)
	{
		// verify the input
		if (Index < 0 || Index >= [Documents count] || [Documents objectAtIndex:Index] == nil)
		{
			return FALSE;
		}

		// this will be saved to disk later with WriteCloudDocument
		FCloudDocument* Doc = [Documents objectAtIndex:Index];

		// convert string to data
		NSString* Str = [NSString stringWithCString:TCHAR_TO_UTF8(*StringData) encoding:NSUTF8StringEncoding];
		Doc.Data = [Str dataUsingEncoding:NSUTF8StringEncoding];

		return TRUE;
	}


	/**
	 * Prepare a document for writing to the cloud with an array of bytes as input data. This is
	 * synchronous
	 *
	 * @param Index index of the document to save
	 * @param ByteData The array of generic bytes to put into the document
	 *
	 * @param True if successful
	 */
	UBOOL SaveDocumentWithBytes(INT Index, const TArray<BYTE>& ByteData)
	{
		// verify the input
		if (Index < 0 || Index >= [Documents count] || [Documents objectAtIndex:Index] == nil)
		{
			return FALSE;
		}

		// this will be saved to disk later with WriteCloudDocument
		FCloudDocument* Doc = [Documents objectAtIndex:Index];

		// convert TArray of bytes to NSData
		Doc.Data = [NSData dataWithBytes:ByteData.GetData() length:ByteData.Num()];

		return TRUE;
	}

	/**
	 * If there was a conflict notification, this will simply tell the cloud interface
	 * to choose the most recently modified version, and toss any others
	 */
	UBOOL ResolveConflictWithNewestDocument()
	{
		// make sure there's a conflict
		if ([PendingConflicts count] == 0)
		{
			return FALSE;
		}

		// get the current document with conflicts
		FCloudDocument* DocumentWithConflict = [PendingConflicts objectAtIndex:0];

		// the newest document is the current version
		NSError* FileError = nil;
		UBOOL bWasError = FALSE;
		if (![NSFileVersion removeOtherVersionsOfItemAtURL:DocumentWithConflict.fileURL error:&FileError])
		{
			bWasError = TRUE;
			debugf(TEXT("There was an error removing other versions of the document"));
			NSLog(@"Error = %@", FileError);
		}

		DocumentWithConflict.ConflictVersion.resolved = YES;

		// move on to the next conflict (if any)
		@synchronized(PendingConflicts)
		{
			[PendingConflicts removeObjectAtIndex:0];
			ProcessNextConflict();
		}

		return !bWasError;
	}

	/**
	 * If there was a conflict notification, this will tell the cloud interface
	 * to choose the version with a given Index to be the master version, and to
	 * toss any others
	 *
	 * @param Index Conflict version index
	 */
	UBOOL ResolveConflictWithVersionIndex(INT Index)
	{
		// make sure there's a conflict
		if ([PendingConflicts count] == 0)
		{
			return FALSE;
		}

		// get the relevant document/version
		FCloudDocument* DocumentWithConflict = [PendingConflicts objectAtIndex:0];
		FCloudDocument* ConflictDocument = [ConflictVersions objectAtIndex:Index];
		NSFileVersion* NewVersion = ConflictDocument.ConflictVersion;

		NSError* FileError;
		UBOOL bWasError = FALSE;

		// replace the document with the version specified by user
		if ([NewVersion.URL isEqual:DocumentWithConflict.fileURL] == NO)
		{
			if (![NewVersion replaceItemAtURL:DocumentWithConflict.fileURL options:0 error:&FileError])
			{
				bWasError = TRUE;
				debugf(TEXT("There was an error replacing item"));
			}
		}

		// now get rid of everything else
		if (![NSFileVersion removeOtherVersionsOfItemAtURL:DocumentWithConflict.fileURL error:&FileError])
		{
			bWasError = TRUE;
			debugf(TEXT("There was an error removing other versions of the document"));
		}
		
		// move on to the next conflict (if any)
		@synchronized(PendingConflicts)
		{
			[PendingConflicts removeObjectAtIndex:0];
			ProcessNextConflict();
		}

		return !bWasError;

	}

	/**
	 * Takes ownership of the Doc and put it into the DOcuments array (Doc will be
	 * released)
	 */
	void AddDocument(FCloudDocument* Doc)
	{
		[Documents addObject:Doc];
		[Doc release];
	}


	/**
	 * Look on the local disk for any saves that may have been saved with iOS 4 previously, and 
	 * upgrade to cloud documents if so
	 */
	void CheckForLegacySaves()
	{
		// make sure we don't send any delegate calls while performing some cloud upgrading
		bSuppressDelegateCalls = TRUE;

		Super::QueryForCloudDocuments();
		
		// if we have any legacy saves, we need to upgrade them 
		INT NumLegacySaves = Super::GetNumCloudDocuments();
		if (NumLegacySaves > 0)
		{
			// @todo: Show an alert message about what we are doing!
			debugf(TEXT("Converting %d legacy saves to iCloud saves!"), NumLegacySaves);
			for (INT SaveIndex = 0; SaveIndex < NumLegacySaves; SaveIndex++)
			{
				// read the document
				if (Super::ReadCloudDocument(SaveIndex, FALSE))
				{
					// get the bytes
					TArray<BYTE> SaveData;
					Super::ParseDocumentAsBytes(SaveIndex, SaveData, FALSE);

					// create a cloud document
					INT CloudIndex = CreateCloudDocument(Super::GetCloudDocumentName(SaveIndex));
					SaveDocumentWithBytes(CloudIndex, SaveData);
					WriteCloudDocument(CloudIndex);
				}
			}
		}

		// and back to normal
		bSuppressDelegateCalls = FALSE;
	}

private:
	/**
	 * Starts working on the next (if any) document conflict
	 */
	void ProcessNextConflict()
	{
		// make sure there's something to do
		if ([PendingConflicts count] == 0)
		{
			return;
		}

		// empty out any stale versions from a previous conflict
		[ConflictVersions removeAllObjects];

		// get the document to process
		FCloudDocument* DocumentWithConflict = [PendingConflicts objectAtIndex:0];

		// get current version and the conflicting versions
		NSFileVersion* CurrentVersion = [NSFileVersion currentVersionOfItemAtURL:DocumentWithConflict.fileURL];
		NSArray* FileVersions = [NSFileVersion unresolvedConflictVersionsOfItemAtURL:DocumentWithConflict.fileURL];

		checkf([CurrentVersion.URL isEqual:DocumentWithConflict.fileURL], TEXT("Assumption about URLs failed"));
		
		// make a new document for the current version, even though this is kind of a waste
		// @todo: Just put DocumentWithConflict into ConflictVersions?
		FCloudDocument* CurrentDocument = [[FCloudDocument alloc] initWithFileURL:CurrentVersion.URL];
		// remember the NSFileVersion object for this
		CurrentDocument.ConflictVersion = CurrentVersion;
		[ConflictVersions addObject:CurrentDocument];

		// now make documents for all the conflicting versions
		for (NSFileVersion* ConflictVersion in FileVersions)
		{
			FCloudDocument* ConflictDocument = [[FCloudDocument alloc] initWithFileURL:ConflictVersion.URL];
			// remember the NSFileVersion object for this
			ConflictDocument.ConflictVersion = ConflictVersion;
			[ConflictVersions addObject:ConflictDocument];
		}
debugf(TEXT("For this conflict, there are %d total conflicting versions"), [ConflictVersions count]);
		// process on game thread
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			if (!UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->bSuppressDelegateCalls)
			{
				// tell script code there's a conflict
				HACK_Result = FPlatformInterfaceDelegateResult(EC_EventParm);
				HACK_Result.bSuccessful = TRUE;
				HACK_Result.Data.Type = PIDT_Int;
				HACK_Result.Data.IntValue = [Documents indexOfObject:DocumentWithConflict];
				UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->CallDelegates(CSD_DocumentConflictDetected, HACK_Result);
			}

			return TRUE;
		};
		[AsyncTask FinishedTask];
	}


	/** Obj-C helper object (retained) */
	FCloudSupport* CloudSupport;

	/** Set of documents living in the cloud */
	NSMutableArray* Documents;

	/** Pending conflict. The 0th one is the current one being processed */
	NSMutableArray* PendingConflicts;

	/** Parallel array to Pending Conflicts, of NSFileVersions */
	NSMutableArray* ConflictVersions;
};






@implementation FCloudSupport

/**
 * Notification called when a cloud key/value has changed
 */
- (void)CloudKeyValueChanged:(NSNotification*)Notification
{
	if (!UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->bSuppressDelegateCalls &&
		UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->HasDelegates(CSD_ValueChanged))
	{
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			FPlatformInterfaceDelegateResult Result(EC_EventParm);
			Result.bSuccessful = TRUE;
			Result.Data.DataName = FName(TEXT("ChangedValue"));
			Result.Data.Type = PIDT_String;

			// send a notification for each value that changed
			NSArray* Array = [Notification.userInfo objectForKey:NSUbiquitousKeyValueStoreChangedKeysKey];
			for (NSString* Str in Array)
			{
				// set the name of the key
				Result.Data.StringValue = FString(Str);

				// call the delegates
				UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->CallDelegates(CSD_ValueChanged, Result);
			}
			return TRUE;
		};
		[AsyncTask FinishedTask];
	}
}

- (void)DocumentStateChanged:(NSNotification*)Notification
{
	// if the document is undergoing conflict resolution, 
	FCloudDocument* UpdatedDoc = Notification.object;
	if (UpdatedDoc.ConflictVersion != nil)
	{
		return;
	}

	NSLog(@"Document [%@] changed state to %d\n", UpdatedDoc.fileURL, UpdatedDoc.documentState);
	if (!UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->bSuppressDelegateCalls)
	{
		IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];
		AsyncTask.GameThreadCallback = ^ UBOOL (void)
		{
			if (UpdatedDoc.documentState == UIDocumentStateInConflict)
			{
				// tell script code there's a conflict
				FPlatformInterfaceDelegateResult Result(EC_EventParm);
				Result.bSuccessful = TRUE;
				UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->CallDelegates(CSD_DocumentConflictDetected, Result);
			}
			return TRUE;
		};
		[AsyncTask FinishedTask];
	}
}

- (void)ProcessResults:(NSMetadataQuery*)Query
{
	NSMetadataQuery* InternalQuery = Query;
	// game thread needs to update the documents array
	IPhoneAsyncTask* AsyncTask = [[IPhoneAsyncTask alloc] init];

	AsyncTask.GameThreadCallback = ^ UBOOL (void)
	{
		INT NumCloudDocuments = [InternalQuery resultCount];
		debugf(TEXT("Back on main thread, %d results"), NumCloudDocuments);
			
		UCloudStorageIPhone* CloudStorage = (UCloudStorageIPhone*)UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton();
		
		// clean up the old documents
		CloudStorage->ClearExistingDocuments();

		// if we get here, and there were no cloud saves, then it's possible this is the first time we launched with iOS5
		// call a function, in the game thread, to check the fallback (Super) for local documents, to see if this device
		// was launched in iOS4 previously, and has save games that should be migrated to the cloud
		if (NumCloudDocuments == 0)
		{
			CloudStorage->CheckForLegacySaves();
		}

		for (INT ResultIndex = 0; ResultIndex < NumCloudDocuments; ResultIndex++)
		{
			// a result from the query is an NSMetaDataItem
			NSMetadataItem* Result = [InternalQuery resultAtIndex:ResultIndex];
			NSURL* ResultURL = [Result valueForAttribute:NSMetadataItemURLKey];

			// create a document object (doesn't read it in yet)
			FCloudDocument* Doc = [[FCloudDocument alloc] initWithFileURL:ResultURL];
			if (Doc)
			{
				// cache the document, note there's a TMap with retained NSObject's here
				CloudStorage->AddDocument(Doc);
			}
		}

		// look at results
		[InternalQuery release];

		// Allow queries again
		bQueryInProgress = FALSE;

		if (!UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->bSuppressDelegateCalls)
		{
			FPlatformInterfaceDelegateResult Result(EC_EventParm);
			Result.bSuccessful = TRUE;
			UPlatformInterfaceBase::GetCloudStorageInterfaceSingleton()->CallDelegates(CSD_DocumentQueryComplete, Result);
		}
		return TRUE;
	};
	[AsyncTask FinishedTask];
}

- (void)MainThreadQueryForDocuments
{
	// workaround the _OBJ_CLASS_$_NSMetadataQuery on older OSs
	Class MetadataClass = NSClassFromString(@"NSMetadataQuery");
	NSMetadataQuery* Query = [[MetadataClass alloc] init];

	CloudObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSMetadataQueryDidFinishGatheringNotification
															    object:Query
																 queue:nil
															usingBlock:^(NSNotification* Notification)
	{
		debugf(TEXT("Querying finished!"));

		// dont let the query update while processing (according to docs)
		[Query disableUpdates];
	
		debugf(TEXT(" %d results"), [Query resultCount]);

		// no longer need to listen for notifications
		[[NSNotificationCenter defaultCenter] removeObserver:CloudObserver
														name:NSMetadataQueryDidFinishGatheringNotification 
													  object:Query];
		
		[self ProcessResults:Query];
	}];

	// look for all cloud documents (object is released in the block)
	debugf(TEXT("Kicking off cloud doc query!"));
	[Query setSearchScopes:[NSArray arrayWithObjects:NSMetadataQueryUbiquitousDocumentsScope, nil]];
	[Query setPredicate:[NSPredicate predicateWithFormat:@"%K like '*'", NSMetadataItemFSNameKey]];
	[Query startQuery];

}

@end

#endif // WITH_IOS_5




#if WITH_IOS_5
IMPLEMENT_CLASS(UCloudStorageIPhone);
#endif

IMPLEMENT_CLASS(UCloudStorageBaseIPhone);
IMPLEMENT_CLASS(ULocalStorageIPhone);

void AutoInitializeRegistrantsIPhoneCloudStorage( INT& Lookup )
{
	UCloudStorageBaseIPhone::StaticClass();
	ULocalStorageIPhone::StaticClass();

#if WITH_IOS_5
	
	// is the user running an OS with iCloud capability?
	if (NSClassFromString(@"NSMetadataQuery") == nil)
	{
		debugf(TEXT("OS not iCloud capable!"));
		return;
	}

	// does the user have iCloud enabled? (this function checks for iCloud being enabled and usable, we don't care about the actual URL)

	if ([[NSFileManager defaultManager] URLForUbiquityContainerIdentifier:nil] == nil)
	{
		debugf(TEXT("iCloud disabled!"));
		return;
	}

	// look for commandline override
	if(ParseParam(appCmdLine(), TEXT("noicloud")) == TRUE)
	{
		debugf(TEXT("iCloud disabled from command line!"));
		return;
	}
		
	// don't even add this class into the object system if iCLoud isn't supported (this will make 
	// GetCloudStorageSingleton return the base CloudStorageBase class

	UCloudStorageIPhone::StaticClass();
#endif
}

