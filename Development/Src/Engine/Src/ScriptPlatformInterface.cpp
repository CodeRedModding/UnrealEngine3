/*=============================================================================
	ScriptPlatformInterface.cpp: Base functionality for the various script accessible platform-interface code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineGameEngineClasses.h"
#include "EnginePlatformInterfaceClasses.h"
#include "AES.h"
#if IPHONE
#include "IPhoneObjCWrapper.h"
#endif


IMPLEMENT_CLASS(UPlatformInterfaceBase);
IMPLEMENT_CLASS(UPlatformInterfaceWebResponse);
IMPLEMENT_CLASS(UCloudStorageBase);
IMPLEMENT_CLASS(UCloudStorageUpgradeHelper);
IMPLEMENT_CLASS(UMicroTransactionBase);
IMPLEMENT_CLASS(UAnalyticEventsBase);
IMPLEMENT_CLASS(UTwitterIntegrationBase);
IMPLEMENT_CLASS(UAppNotificationsBase);
IMPLEMENT_CLASS(UInAppMessageBase);

// @todo: For now this is copied from UnEngine.cpp, we could try to merge code at some point
static UINT EncryptedMagic = 0xC0DEDBAD;

/**
 * Load a file and possibly decrypt if it has the magic tag
 */
static UBOOL LoadFileAndDecrypt(TArray<BYTE>& Result, const TCHAR* Filename)
{
	Result.Empty();

	// always decrypt data
	TArray<BYTE> LoadedData;
	if (!appLoadFileToArray(LoadedData, Filename))
	{
		return FALSE;
	}

	// is the first 4 bytes the magic?
	if (LoadedData.Num() >= 4 && ((UINT*)LoadedData.GetData())[0] == EncryptedMagic)
	{
		// make sure the encrypted data is a multiple of 16 bytes
		if (((LoadedData.Num() - 4) & 15) != 0)
		{
			debugf(TEXT("Loading an encrypted file (%s) that wasn't padded to 16 bytes, which it must have been to be encrypted. Failing."), Filename);
			return FALSE;
		}

		// copy encrypted data to the output
		Result.Add(LoadedData.Num() - 4);
		appMemcpy(Result.GetData(), LoadedData.GetTypedData() + 4, Result.Num());

		// decrypt the data
		appDecryptData(Result.GetTypedData(), Result.Num());
	}
	else
	{
		// if it wasn't encrypted, just copy it over
		Result = LoadedData;
	}

	return TRUE;
}

/**
 * Encrypt a block of data, then save it to disk
 */
static UBOOL EncryptAndSaveFile(const TArray<BYTE>& Array, const TCHAR* Filename)
{
	// we need to be aligned to 16 bytes
	TArray<BYTE> WorkArray;
	
	// add 4 bytes for the magic
	WorkArray.Add(4);
	((UINT*)WorkArray.GetData())[0] = EncryptedMagic;
	
	// put on the input data, unencrypted
	WorkArray.Append(Array);

	// make sure the encrypted part is a multiple of 16
	WorkArray.AddZeroed(Align(Array.Num(), 16) - Array.Num());

	// encrypt the data
	appEncryptData(WorkArray.GetTypedData() + 4, WorkArray.Num() - 4);

	// save to disk!
	return appSaveArrayToFile(WorkArray, Filename);
}

/*******************************************
 * Platform Interface Base                 *
 *******************************************/


/**
 * Determines if there are any delegates of the given type on this platform interface object.
 * This is useful to skip a bunch of FPlatformInterfaceDelegateResult if there is no
 * one even listening!
 *
 * @param DelegateType The type of delegate to look up delegates for
 *
 * @return TRUE if there are any delegates set of the given type
 */
UBOOL UPlatformInterfaceBase::HasDelegates(INT DelegateType)
{
	// has script ever put anything in for this delegate type?
	if (AllDelegates.Num() > DelegateType)
	{
		// if so are there currently any set?
		if (AllDelegates(DelegateType).Delegates.Num() > 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Call all the delegates currently set for the given delegate type with the given data
 *
 * @param DelegateType Which set of delegates to call (this is defined in the subclass of PlatformInterfaceBase)
 * @param Result Data to pass to each delegate
 */
void UPlatformInterfaceBase::CallDelegates(INT DelegateType, FPlatformInterfaceDelegateResult& Result)
{
	// make sure that script has ever put anything in for this delegate type
	if (AllDelegates.Num() <= DelegateType)
	{
		return;
	}

	// call all the delegates that are set
	FDelegateArray& DelegateArray = AllDelegates(DelegateType);
	
	// copy the array in case delegates are removed from the class's delegates array
	TArray<FScriptDelegate> ArrayCopy = DelegateArray.Delegates;
	for (INT DelegateIndex = 0; DelegateIndex < ArrayCopy.Num(); DelegateIndex++)
	{
		ProcessDelegate(NAME_None, &ArrayCopy(DelegateIndex), &Result);
	}
}

/**
 * Check for certain exec commands that map to the various subclasses (it will only
 * get/create the singleton if the first bit of the exec command matches a one of 
 * the special strings, like "ad" for ad manager)
 */
UBOOL UPlatformInterfaceBase::StaticExec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// @todo: Ask each subclass via a static function like GetExecBaseCommand(), then
	// get the singleton, and pass the rest of the command to it
	if (ParseCommand( &Cmd, TEXT("Ad")))
	{
		UInGameAdManager* AdManager = UPlatformInterfaceBase::GetInGameAdManagerSingleton();
		if (ParseCommand(&Cmd, TEXT("Show")))
		{
			AdManager->ShowBanner(appAtoi(Cmd));
		}
		else if (ParseCommand(&Cmd, TEXT("Hide")))
		{
			AdManager->HideBanner();
		}
		else if (ParseCommand(&Cmd, TEXT("Close")))
		{
			AdManager->ForceCloseAd();
		}
		return TRUE;
	}
	else if (ParseCommand( &Cmd, TEXT("FB")))
	{
		UFacebookIntegration* FB = UPlatformInterfaceBase::GetFacebookIntegrationSingleton();
		if (ParseCommand(&Cmd, TEXT("auth")))
		{
			FB->eventAuthorize();
		}
		else if (ParseCommand(&Cmd, TEXT("isauthed")))
		{
			Ar.Logf(TEXT("Authorized? %d"), FB->eventIsAuthorized());
		}
		else if (ParseCommand(&Cmd, TEXT("username")))
		{
			Ar.Logf(TEXT("FB username is %s"), *FB->UserName);
		}
		else if (ParseCommand(&Cmd, TEXT("disconnect")))
		{
			FB->eventDisconnect();
		}
		return TRUE;
	}

	return FALSE;
}


#define IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(Class, ClassDesc) \
	/** C++ interface to get the singleton */ \
	Class* UPlatformInterfaceBase::Get##ClassDesc##Singleton() \
	{ \
		/* the singleton object */ \
		static Class* Singleton = NULL; \
 \
		/* have we created the singleton yet? */ \
		if (Singleton == NULL) \
		{ \
			/* load the class name from the .ini */ \
			FString SingletonClassName; \
			GConfig->GetString(TEXT("PlatformInterface"), TEXT(#ClassDesc) TEXT("ClassName"), SingletonClassName, GEngineIni); \
 \
			/* load the class (most likely intrinsic) */ \
			UClass* SingletonClass = LoadClass<Class>(NULL, *SingletonClassName, NULL, LOAD_None, NULL); \
			if (SingletonClass == NULL) \
			{ \
				/* if something failed, then try loading the fallback class */ \
				/* load the class name from the .ini */ \
				GConfig->GetString(TEXT("PlatformInterface"), TEXT(#ClassDesc) TEXT("FallbackClassName"), SingletonClassName, GEngineIni); \
 \
				/* load the class (most likely intrinsic) */ \
				SingletonClass = LoadClass<Class>(NULL, *SingletonClassName, NULL, LOAD_None, NULL); \
				if (SingletonClass == NULL) \
				{ \
					/* if something failed with the fallback, then use the default */ \
					SingletonClass = Class::StaticClass(); \
				} \
			} \
 \
			/* make the singleton object that never goes away */ \
			Singleton = ConstructObject<Class>(SingletonClass); \
			check(Singleton); \
			Singleton->AddToRoot(); \
 \
			/* never Garbage Collect this singleton */ \
			Singleton->AddToRoot(); \
\
			/* initialize it */ \
			Singleton->eventInit(); \
		} \
 \
		/* there will be a Singleton by this point (or an error above) */ \
		return Singleton; \
	} \
 \
	 /** This is called on the default object, call the class static function */ \
	Class* UPlatformInterfaceBase::Get##ClassDesc() \
	{ \
		return Get##ClassDesc##Singleton(); \
	}


IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UCloudStorageBase, CloudStorageInterface)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UCloudStorageBase, LocalStorageInterface)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UFacebookIntegration, FacebookIntegration)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UInGameAdManager, InGameAdManager)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UMicroTransactionBase, MicroTransactionInterface)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UAnalyticEventsBase,AnalyticEventsInterface)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UTwitterIntegrationBase,TwitterIntegration)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UAppNotificationsBase,AppNotificationsInterface)
IMPLEMENT_PLATFORM_INTERFACE_SINGLETON(UInAppMessageBase,InAppMessageInterface)

/*******************************************
 * Cloud Storage                           *
 *******************************************/


/**
 * Perform any initialization
 */
void UCloudStorageBase::Init()
{
	// Make a note on the local system that we need to upgrade these local files if we ever move to the cloud
	// @todo ib2merge: Are these useful to everyone? Or just for IB? And is it needed for IB3?
	FString NeedsUpgradeKeyName = TEXT("UpgradeKey");
	FPlatformInterfaceData NeedsUpgradeValue;
	NeedsUpgradeValue.IntValue = 1;
	NeedsUpgradeValue.Type = PIDT_Int;
	WriteKeyValue(NeedsUpgradeKeyName, NeedsUpgradeValue);
}

/**
 * @return True if we are actually using local cloud storage emulation
 */
UBOOL UCloudStorageBase::IsUsingLocalStorage()
{
	return TRUE;
}

/**
 * Initiate reading a key/value pair from cloud storage. A CSD_KeyValueReadComplete
 * delegate will be called when it completes (if this function returns true)
 * 
 * @param KeyName String name of the key to retrieve
 * @param Type Type of data to retrieve 
 * @param SerializedObj If you are reading an object (PIDT_Object), it will de-serialize the binary blob into this object
 *
 * @return True if successful
 */
UBOOL UCloudStorageBase::ReadKeyValue(const FString& KeyName, BYTE Type, struct FPlatformInterfaceDelegateResult& Value)
{
	appMemzero(&Value, sizeof(Value));

	Value.bSuccessful = TRUE;
	Value.Data.Type = Type;
	Value.Data.DataName = FName(*KeyName);

	UBOOL bFileOpsDisabled = GConfig->AreFileOperationsDisabled();
	if (bFileOpsDisabled)
		GConfig->EnableFileOperations();
	static FString CloudStorageIni(appCloudDir() + TEXT("CloudStorage.ini"));
	switch (Type)
	{
		case PIDT_Int:
			GConfig->GetInt(TEXT("CloudStorageEmulation"), *KeyName, Value.Data.IntValue, *CloudStorageIni);
			break;
		case PIDT_Float:
			GConfig->GetFloat(TEXT("CloudStorageEmulation"), *KeyName, Value.Data.FloatValue, *CloudStorageIni);
			break;
		case PIDT_String:
			GConfig->GetString(TEXT("CloudStorageEmulation"), *KeyName, Value.Data.StringValue, *CloudStorageIni);
			break;
		case PIDT_Object:
			{
				debugf(TEXT("CONVERT SOME BYTE ARRAY TO UOBJECT HERE?"));
			}
			break;
	}
	if (bFileOpsDisabled)
		GConfig->DisableFileOperations();

	return TRUE;
}


/**
* Reads a key/value pair from the local backup of the cloud KVS storage 
* 
*
* @param KeyName String name of the key to retrieve
* @param Type Type of data to retrieve 
* @param Result The resulting value of the key we just read (supports multiple types of value)
*
* @return True if successful
*/
UBOOL UCloudStorageBase::ReadKeyValueFromLocalStore(const FString& KeyName, BYTE Type, struct FPlatformInterfaceDelegateResult& Value)
{
	//Base implementation for PC emulation always stores data on disk we should never need to do any resolution
	return ReadKeyValue(KeyName, Type, Value);
}

/**
 * Write a key/value pair to the cloud. A CSD_KeyValueWriteComplete
 * delegate will be called when it completes (if this function returns true)
 *
 * @param KeyName String name of the key to write
 * @param Value The type and value to write
 *
 * @return True if successful
 */
UBOOL UCloudStorageBase::WriteKeyValue(const FString&  KeyName, const FPlatformInterfaceData& Value)
{
	static FString CloudStorageIni(appCloudDir() + TEXT("CloudStorage.ini"));

	UBOOL bAreFileOperationsDisabled = GConfig->AreFileOperationsDisabled();
	GConfig->EnableFileOperations();
	switch (Value.Type)
	{
		case PIDT_Int:
			GConfig->SetInt(TEXT("CloudStorageEmulation"), *KeyName, Value.IntValue, *CloudStorageIni);
			break;
		case PIDT_Float:
			GConfig->SetFloat(TEXT("CloudStorageEmulation"), *KeyName, Value.FloatValue, *CloudStorageIni);
			break;
		case PIDT_String:
			GConfig->SetString(TEXT("CloudStorageEmulation"), *KeyName, *Value.StringValue, *CloudStorageIni);
			break;
		case PIDT_Object:
			{
				debugf(TEXT("CONVERT SOME BYTE ARRAY TO UOBJECT HERE"));
			}
			break;
		default:
			{
				debugf(TEXT("WriteKeyValue failed: Value.Type not set to supported type!"));
			}
			break;
	};
	// write it out
	GConfig->Flush(FALSE, *CloudStorageIni);
	if (bAreFileOperationsDisabled)
	{
		GConfig->DisableFileOperations();
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
UBOOL UCloudStorageBase::QueryForCloudDocuments()
{
	// look for the files
	LocalCloudFiles.Empty();
	appFindFilesInDirectory(LocalCloudFiles, *appCloudDir(), TRUE, TRUE);

	if (!bSuppressDelegateCalls)
	{
		// and we're done, call the delegates
		FPlatformInterfaceDelegateResult Result(EC_EventParm);
		Result.bSuccessful = TRUE;
		CallDelegates(CSD_DocumentQueryComplete, Result);
	}
	return TRUE;
}

/**
 * @return the number of documents that are known to exist in the cloud
 */
INT UCloudStorageBase::GetNumCloudDocuments(UBOOL bIsForConflict)
{
	if (bIsForConflict)
	{
		return 0;
	}
	return LocalCloudFiles.Num();
}

/**
 * @return the name of the given cloud by index (up to GetNumCloudDocuments() - 1)
 */
FString UCloudStorageBase::GetCloudDocumentName(INT Index)
{
	// verify the input
	if (Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return FString(TEXT(""));
	}

	// pull apart the URL to get the filename
  	return FFilename(LocalCloudFiles(Index)).GetCleanFilename();
}

/**
 * Create a new document in the cloud (uninitialized, unsaved, use the Write/Save functions)
 *
 * @param Filename Filename for the cloud document (with any extension you want)
 * 
 * @return Index of the new document, or -1 on failure
 */
INT UCloudStorageBase::CreateCloudDocument(const FString& Filename)
{
	FString FinalFilename = appCloudDir() + Filename;
	return LocalCloudFiles.AddItem(FinalFilename);
}

/**
 * Removes all of the files in the LocalCloudFiles array.
 */
void UCloudStorageBase::DeleteAllCloudDocuments()
{
	INT Index = 0;
	for (Index = 0; Index < LocalCloudFiles.Num(); ++Index)
	{
		GFileManager->Delete(*LocalCloudFiles(Index));
	}
	
	LocalCloudFiles.Empty();
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
UBOOL UCloudStorageBase::ReadCloudDocument(INT Index, UBOOL bIsForConflict)
{
	// verify the input
	if (bIsForConflict || Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return FALSE;
	}

	// just call the delegate, we'll read in in the Parse function
	if (GFileManager->FileSize(*LocalCloudFiles(Index)) != -1)
	{
		if (!bSuppressDelegateCalls)
		{
			FPlatformInterfaceDelegateResult Result(EC_EventParm);
			Result.bSuccessful = TRUE;
			// which document is this?
			Result.Data.Type = PIDT_Int;
			Result.Data.IntValue = Index;
			CallDelegates(CSD_DocumentReadComplete, Result);
		}		

		return TRUE;
	}
	return FALSE;
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
FString UCloudStorageBase::ParseDocumentAsString(INT Index, UBOOL bIsForConflict)
{
	// verify the input
	if (bIsForConflict || Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return FString(TEXT(""));
	}

	FString Result;
	appLoadFileToString(Result, *LocalCloudFiles(Index));
	return Result;
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
void UCloudStorageBase::ParseDocumentAsBytes(INT Index, TArray<BYTE>& ByteData, UBOOL bIsForConflict)
{
	// make sure a clean slate
	ByteData.Empty();

	// verify the input
	if (bIsForConflict || Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return;
	}

	LoadFileAndDecrypt(ByteData, *LocalCloudFiles(Index));
}

/**
 * Once a document has been read in, use this to return a string representing the 
 * entire document. This should only be used if SaveDocumentWithString was used to
 * generate the document.
 *
 * @param Index index of the document to read
 * @param ExpectedVersion Version number expected to be in the save data. If this doesn't match what's there, this function will return NULL
 * @param ObjectClass The class of the object to create
 *
 * @return The object deserialized from the document. It will be none if anything went wrongs
 */
UObject* UCloudStorageBase::ParseDocumentAsObject(INT Index, UClass* ObjectClass, INT ExpectedVersion, UBOOL bIsForConflict)
{
	TArray<BYTE> ObjectBytes;
	// read in a BYTE array
	ParseDocumentAsBytes(Index, ObjectBytes, bIsForConflict);

	// make sure we got some bytes
	if (ObjectBytes.Num() == 0)
	{
		return NULL;
	}

	FMemoryReader MemoryReader(ObjectBytes, TRUE);

	// load the version the object was saved with
	INT SavedVersion;
	MemoryReader << SavedVersion;

	// make sure it matches
	if (SavedVersion != ExpectedVersion)
	{
		// note that it failed to read
		debugf(TEXT("Load failed: Cloud document was saved with an incompatibly version (%d, expected %d)."), SavedVersion, ExpectedVersion);
		return NULL;
	}

	// use a wrapper archive that converts FNames and UObject*'s to strings that can be read back in
	FObjectAndNameAsStringProxyArchive Ar(MemoryReader);

	// NOTE: The following should be in shared functionality in UCloudStorageBase
	// create the object
	UObject* Obj = StaticConstructObject(ObjectClass);

	// serialize the object
	Obj->Serialize(Ar);

	// return the deserialized object
	return Obj;
}



/**
 * Writes a document that has been already "saved" using the SaveDocumentWith* functions.
 * A CSD_DocumentWriteComplete delegate will be called (if this function returns true).
 *
 * @param Index index of the document to read
 *
 * @param True if successful
 */
UBOOL UCloudStorageBase::WriteCloudDocument(INT Index)
{
	// verify the input
	if (Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return FALSE;
	}

	if (!bSuppressDelegateCalls)
	{
		// was already written out in the SaveDocument, so just call the delegate
		FPlatformInterfaceDelegateResult Result(EC_EventParm);
		Result.bSuccessful = TRUE;
		// which document is this?
		Result.Data.Type = PIDT_Int;
		Result.Data.IntValue = Index;
		CallDelegates(CSD_DocumentWriteComplete, Result);
	}
	return TRUE;
}

/**
 * Checks whether there are any pending writes.
 * Always false in base system that uses synchronous writes.
 */
UBOOL UCloudStorageBase::IsStillWritingFiles()
{
	return FALSE;
}

/**
 * Waits until there are no more pending writes, or returns false if the time runs out.
 */
UBOOL UCloudStorageBase::WaitForWritesToFinish(FLOAT MaxTimeSeconds)
{
	DOUBLE KillTime = appSeconds() + (DOUBLE)MaxTimeSeconds;

	while(IsStillWritingFiles())
	{
		// just waiting...
		appSleep(0.1f);

		// Kill this process if it is taking too long
		if( MaxTimeSeconds >= 0 && appSeconds() > KillTime )
		{
			return FALSE;
		}
	}

	return TRUE;
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
UBOOL UCloudStorageBase::SaveDocumentWithString(INT Index, const FString& StringData)
{
	// verify the input
	if (Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return FALSE;
	}

	return appSaveStringToFile(StringData, *LocalCloudFiles(Index));
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
UBOOL UCloudStorageBase::SaveDocumentWithBytes(INT Index, const TArray<BYTE>& ByteData)
{
	// verify the input
	if (Index < 0 || Index >= LocalCloudFiles.Num())
	{
		return FALSE;
	}
	
	return EncryptAndSaveFile(ByteData, *LocalCloudFiles(Index));
}

/**
 * Prepare a document for writing to the cloud with an object as input data. This is
 * synchronous
 *
 * @param Index index of the document to save
 * @param ObjectData The object to serialize to bytes and put into the document
 * @param SaveVersion The version of this object. Used so that when you load this object, if the version doesn't match it will skip loading the object
 *
 * @param True if successful
 */
UBOOL UCloudStorageBase::SaveDocumentWithObject(INT Index, UObject* ObjectData, INT SaveVersion)
{
	// verify the input
	if (GetCloudDocumentName(Index) == TEXT(""))
	{
		return FALSE;
	}

	TArray<BYTE> ObjectBytes;
	FMemoryWriter MemoryWriter(ObjectBytes);

	// save out a version
	MemoryWriter << SaveVersion;

	// use a wrapper archive that converts FNames and UObject*'s to strings that can be read back in
	FObjectAndNameAsStringProxyArchive Ar(MemoryWriter);

	// serialize the object
	ObjectData->Serialize(Ar);

	// now, push the byte array into the document
	SaveDocumentWithBytes(Index, ObjectBytes);

	return TRUE;
}

/**
 * If there was a conflict notification, this will simply tell the cloud interface
 * to choose the most recently modified version, and toss any others
 */
UBOOL UCloudStorageBase::ResolveConflictWithNewestDocument()
{
	return FALSE;
}

/**
 * If there was a conflict notification, this will tell the cloud interface
 * to choose the version with a given Index to be the master version, and to
 * toss any others
 *
 * @param Index Conflict version index
 */
UBOOL UCloudStorageBase::ResolveConflictWithVersionIndex(INT /*Index*/)
{
	return FALSE;
}


/**
 * Check if there are local files on disk. These would come from systems that allow both
 * cloud files and local files (e.g. iOS users not signed into iCloud).
 * HandleLocalDocument() will be called for each file this function finds, which you can
 * override to handle each document.
 *
 * @return True if there were any documents processed
 */
UBOOL UCloudStorageBase::UpgradeLocalStorageToCloud(const TScriptInterface<class ICloudStorageUpgradeHelper>& UpgradeHelper, UBOOL bForceSearchAgain)
{
#if IPHONE
	return FALSE;
#elif 1
	// For testing on PC only

	// Check if we have already upgraded the local system files
	FPlatformInterfaceDelegateResult LocalValue;
	FString NeedsUpgradeKeyName = TEXT("LOCAL::UpgradeKey");
	if( !bForceSearchAgain && ReadKeyValue(NeedsUpgradeKeyName, PIDT_Int, LocalValue) )
	{
		if( LocalValue.Data.IntValue == 0 )
		{
			// We've already done this, no need to do it again
			return FALSE;
		}
	}

	// make sure we don't send any delegate calls while performing some cloud upgrading
	bSuppressDelegateCalls = TRUE;

	// look for the files
	TArray<FString> LocalTestFiles;
	appFindFilesInDirectory(LocalTestFiles, *(appCloudDir() + FString(TEXT("..\\CloudLOCAL"))), TRUE, TRUE);
	
	// if we have any legacy saves, we need to upgrade them 
	INT NumLegacySaves = LocalTestFiles.Num();
	if (NumLegacySaves > 0)
	{
		// @todo: Show an alert message about what we are doing!
		debugf(TEXT("Converting %d legacy saves to iCloud saves!"), NumLegacySaves);
		for (INT SaveIndex = 0; SaveIndex < NumLegacySaves; SaveIndex++)
		{
			FString DocName = LocalTestFiles(SaveIndex);
			INT bShouldCreateCloudDoc = TRUE;
			INT bShouldDeleteLocalFile = TRUE;
			UpgradeHelper->eventHandleLocalDocument(DocName, bShouldCreateCloudDoc, bShouldDeleteLocalFile);
			if (bShouldCreateCloudDoc == TRUE)
			{
				// get the bytes
				TArray<BYTE> SaveData;
				LoadFileAndDecrypt(SaveData, *LocalTestFiles(SaveIndex));

				// create a cloud document
				INT CloudIndex = CreateCloudDocument(DocName);
				SaveDocumentWithBytes(CloudIndex, SaveData);
				WriteCloudDocument(CloudIndex);
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
		FString LocalKeyName = FString::Printf(TEXT("LOCAL::%s"), *CloudKeyName);
		appMemZero(CloudValue);
		CloudValue.DataName = NAME_None;

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
		if( ReadKeyValue(LocalKeyName, CloudValue.Type, LocalValue) )
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
				WriteKeyValue(LocalKeyName, CloudValue);
			}
		}
	}

	// Make a note on the local system that we don't need to do this again
	FPlatformInterfaceData NeedsUpgradeValue;
	NeedsUpgradeValue.IntValue = 0;
	NeedsUpgradeValue.Type = PIDT_Int;
	WriteKeyValue(NeedsUpgradeKeyName, NeedsUpgradeValue);

	// and back to normal
	bSuppressDelegateCalls = FALSE;

	return NumLegacySaves > 0;
#else
	return FALSE;
#endif
}






/*******************************************
 * Microtransactions                       *
 *******************************************/




/**
 * Perform any initialization
 */
void UMicroTransactionBase::Init()
{

}


/**
 * Query system for what purchases are available. Will fire a MTD_PurchaseQueryComplete
 * if this function returns true.
 *
 * @return True if the query started successfully (delegate will receive final result)
 */
UBOOL UMicroTransactionBase::QueryForAvailablePurchases()
{
	return FALSE;
}

/**
 * @return True if the user is allowed to make purchases - should give a nice message if not
 */
UBOOL UMicroTransactionBase::IsAllowedToMakePurchases()
{
	return FALSE;
}

/**
 * Triggers a product purchase. Will fire a MTF_PurchaseComplete if this function
 * returns true.
 *
 * @param Index which product to purchase
 * 
 * @param True if the purchase was kicked off (delegate will receive final result)
 */
UBOOL UMicroTransactionBase::BeginPurchase(INT Index)
{
	return FALSE;
}

/**
 * Returns the product's index from an ID
 *
 * @param Identifier the product Identifier
 * 
 * @return The index of the product in the AvailableProducts array
 */
INT UMicroTransactionBase::GetProductIndex(const FString& Identifier)
{
	for( INT i=0; i<AvailableProducts.Num(); i++ )
	{
		if( AvailableProducts(i).Identifier == Identifier )
		{
			return i;
		}
	}

	return INDEX_NONE;
}


/*******************************************
 * UAnalyticEventsBase
 *******************************************/

/**
 * If -DEBUGANALYTICS is on the command line, unsuppresses DevStats, and forces Swrve provider to use APIServerDebug
 */
void UAnalyticEventsBase::Init()
{
	// ensure DevStats is unsuppressed when testing analytics.
	if (ParseParam(appCmdLine(), TEXT("DEBUGANALYTICS")) || ParseParam(appCmdLine(), TEXT("TESTANALYTICS")))
	{
		GEngine->Exec(TEXT("UNSUPPRESS DevStats"));
	}

}
void UAnalyticEventsBase::SetUserId(const FString& NewUserId)
{
	UserId = NewUserId; 
}
void UAnalyticEventsBase::StartSession()
{
	bSessionInProgress = TRUE;
}
void UAnalyticEventsBase::EndSession()
{
	bSessionInProgress = FALSE;
}
void UAnalyticEventsBase::LogStringEvent(const FString& EventName,UBOOL bTimed){}
void UAnalyticEventsBase::EndStringEvent(const FString& EventName){}
void UAnalyticEventsBase::LogStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue,UBOOL bTimed){}
void UAnalyticEventsBase::EndStringEventParam(const FString& EventName,const FString& ParamName,const FString& ParamValue){}
void UAnalyticEventsBase::LogStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray,UBOOL bTimed){}
void UAnalyticEventsBase::EndStringEventParamArray(const FString& EventName,const TArray<struct FEventStringParam>& ParamArray){}
void UAnalyticEventsBase::LogErrorMessage(const FString& ErrorName,const FString& ErrorMessage){}
void UAnalyticEventsBase::LogUserAttributeUpdate(const FString& AttributeName, const FString& AttributeValue){}
void UAnalyticEventsBase::LogUserAttributeUpdateArray(const TArray<FEventStringParam>& AttributeArray) {}
void UAnalyticEventsBase::LogItemPurchaseEvent(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity){}
void UAnalyticEventsBase::LogCurrencyPurchaseEvent(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider){}
void UAnalyticEventsBase::LogCurrencyGivenEvent(const FString& GameCurrencyType, int GameCurrencyAmount){}
void UAnalyticEventsBase::SendCachedEvents(){}


/*******************************************
 * Twitter Integration
 *******************************************/

/**
 * Perform any needed initialization
 */
void UTwitterIntegrationBase::Init()
{
}

/**
 * Starts the process of authorizing the local user
 */
UBOOL UTwitterIntegrationBase::AuthorizeAccounts()
{
	return FALSE;
}

/**
 * @return The number of accounts that were authorized
 */
INT UTwitterIntegrationBase::GetNumAccounts()
{
	return 0;
}

/**
 * @return the display name of the given Twitter account
 */
FString UTwitterIntegrationBase::GetAccountName(INT AccountIndex)
{
	return TEXT("");
}

/**
 * @return the id of the given Twitter account
 */
FString UTwitterIntegrationBase::GetAccountId(INT AccountIndex)
{
	return TEXT("");
}

/**
 * @return true if the user is allowed to use the Tweet UI
 */
UBOOL UTwitterIntegrationBase::CanShowTweetUI()
{
	return FALSE;
}

/**
 * Kicks off a tweet, using the platform to show the UI. If this returns FALSE, or you are on a platform that doesn't support the UI,
 * you can use the TwitterRequest method to perform a manual tweet using the Twitter API
 *
 * @param InitialMessage [optional] Initial message to show
 * @param URL [optional] URL to attach to the tweet
 * @param Picture [optional] Name of a picture (stored locally, platform subclass will do the searching for it) to add to the tweet
 *
 * @return TRUE if a UI was displayed for the user to interact with, and a TID_TweetUIComplete will be sent
 */
UBOOL UTwitterIntegrationBase::ShowTweetUI(const FString& InitialMessage, const FString& URL, const FString& Picture)
{
	return FALSE;
}

/**
 * Kicks off a generic twitter request
 *
 * @param URL The URL for the twitter request
 * @param KeysAndValues The extra parameters to pass to the request (request specific). Separate keys and values: < "key1", "value1", "key2", "value2" >
 * @param Method The method for this request (get, post, delete)
 * @param AccountIndex A user index if an account is needed, or -1 if an account isn't needed for the request
 *
 * @return TRUE the request was sent off, and a TID_RequestComplete
 */
UBOOL UTwitterIntegrationBase::TwitterRequest(const FString& URL, const TArray<FString>& ParamKeysAndValues, BYTE RequestMethod, INT AccountIndex)
{
	return FALSE;
}


/*******************************************
 * Platform Interface Web Response         *
 *******************************************/

/** @return the number of header/value pairs */
INT UPlatformInterfaceWebResponse::GetNumHeaders()
{
	return Headers.Num();
}

/** Retrieve the header and value for the given index of header/value pair */
void UPlatformInterfaceWebResponse::GetHeader(INT HeaderIndex,FString& Header,FString& Value)
{
	// this is slow if script iterates over the map one at a time, but it's not expected this will be called often
	INT Index = 0;
	for (TMap<FString,FString>::TIterator It(Headers); It && Index < Headers.Num(); ++It, ++Index)
	{
		// is it the requested header?
		if (Index == HeaderIndex)
		{
			Header = It.Key();
			Value = It.Value();
		}
	}
}

/** @return the value for the given header (or "" if no matching header) */
FString UPlatformInterfaceWebResponse::GetHeaderValue(const FString& HeaderName)
{
	// look up the header
	FString* Val = Headers.Find(HeaderName);
	if (Val)
	{
		// return if it exists
		return *Val;
	}
	return TEXT("");
}

/*******************************************
 * Platform Interface In app message *
 *******************************************/
void UInAppMessageBase::Init()
{

}

UBOOL UInAppMessageBase::ShowInAppSMSUI(const FString& InitialMessage)
{
	return FALSE;
}

UBOOL UInAppMessageBase::ShowInAppEmailUI(const FString& InitialSubject, const FString& InitialMessage)
{
	return FALSE;
}

/*******************************************
 * UAppNotificationsBase
 *******************************************/

void UAppNotificationsBase::Init()
{
}

void UAppNotificationsBase::ScheduleLocalNotification(const struct FNotificationInfo& Notification,INT StartOffsetSeconds)
{

}
void UAppNotificationsBase::CancelAllScheduledLocalNotifications()
{
}
