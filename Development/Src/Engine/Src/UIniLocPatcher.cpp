/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

#include "FConfigCacheIni.h"

IMPLEMENT_CLASS(UIniLocPatcher);

#define DEBUG_DOWNLOADS !FINAL_RELEASE

#if DEBUG_DOWNLOADS
/**
 * Save off the files for verification
 *
 * @param FileName the name of the file to dump to disk
 * @param FileData the contents of the file to write out
 */
void DebugWriteIniLocPatchedFile(const FString& FileName,const TArray<BYTE>& FileData)
{
	// Make sure the directory exists
	FString PathName(appGameDir() + TEXT("Logs") PATH_SEPARATOR + TEXT("EMS"));
	GFileManager->MakeDirectory(*PathName);
	// Now append the file name to the path and write it out
	FString FullPathName = PathName + PATH_SEPARATOR + FileName;
	appSaveArrayToFile(FileData,*FullPathName);
}
#endif


/**
 * Builds the path information needed to find the file in the config cache
 *
 * @param FileName the file to prepend with the path
 *
 * @return the new file name to use
 */
static FString CreateIniLocFilePath(const FString& FileName)
{
	FString NewFilename;
	FFilename DownloadedFile(FileName);
	FString Extension = DownloadedFile.GetExtension();
	// Use the extension to determine if it is loc or not
	if (Extension == TEXT("ini"))
	{
		NewFilename = appGameConfigDir() + FileName;
	}
	else
	{
		// Build in the proper language sub-directory (..\ExampleGame\Localization\fra\ExampleGame.fra)
		for (INT LocIdx=GSys->LocalizationPaths.Num()-1; LocIdx >= 0; LocIdx--)
		{
			NewFilename = GSys->LocalizationPaths(LocIdx) * Extension * FileName;
			if (GConfig->FindConfigFile(*NewFilename) != NULL)
			{
				break;
			}
		}
	}
	return NewFilename;
}

/**
 * Takes the data, merges with the INI/Loc system, and then reloads the config for the
 * affected objects
 *
 * @param FileName the name of the file being merged
 * @param bIsUnicode whether to treat the file as unicode or not
 * @param FileData the file data to merge with the config cache
 */
void UIniLocPatcher::ProcessIniLocFile(const FString& FileName,UBOOL bIsUnicode,const TArray<BYTE>& FileData)
{
	// Make sure that data was returned
	if (FileName.Len() > 0 && FileData.Num())
	{
		const FString FileNameWithPath = CreateIniLocFilePath(FileName);
		FFilename DownloadedFile(FileName);
		FString Extension = DownloadedFile.GetExtension();
#if DEBUG_DOWNLOADS
		// Save off the files for verification
		DebugWriteIniLocPatchedFile(FileName,FileData);
#endif
		// Use the extension to determine if it is loc or not
		UBOOL bIsLocFile = Extension != TEXT("ini");
		// Find the config file in the config cache and skip if not present
		FConfigFile* ConfigFile = GConfig->Find( *FileNameWithPath, true );
		// If not found, add this file to the config cache
		if (ConfigFile == NULL)
		{
			ConfigFile = &GConfig->Set(*FileNameWithPath,FConfigFile());
		}
		check(ConfigFile);
#if !CONSOLE
		// We don't want to save out the merged results
		ConfigFile->NoSave = TRUE;
#endif
		// Make sure the string is null terminated
		((TArray<BYTE>&)FileData).AddItem(0);
		((TArray<BYTE>&)FileData).AddItem(0);
		// Now convert to a string for config updating
		FString IniLocData;
		if (bIsUnicode)
		{
			BYTE* RawData = (BYTE*)FileData.GetTypedData();
			// Check for a byte order marker and skip it if present
			if (RawData[0] == 255 &&
				RawData[1] == 254)
			{
				RawData += 2;
			}
#if TCHAR_IS_4_BYTES
			WORD* WideRawData16 = (WORD*)RawData;
			TCHAR* WideRawData = (TCHAR*)appMalloc(FileData.Num()*2);
			appMemzero(WideRawData,FileData.Num()*2);
			for (INT CharIndex = 0; WideRawData16[CharIndex] != 0; CharIndex++)
			{
				WideRawData[CharIndex] = INTEL_ORDER32(WideRawData16[CharIndex]);
			}
			IniLocData = (const TCHAR*)WideRawData;
			appFree(WideRawData);
#elif !__INTEL_BYTE_ORDER__
			TCHAR* WideRawData = (TCHAR*)RawData;
			// We need to byte swap on non-Intel machines
			for (INT CharIndex = 0; WideRawData[CharIndex] != 0; CharIndex++)
			{
				WideRawData[CharIndex] = INTEL_ORDER16((WORD)WideRawData[CharIndex]);
			}
			IniLocData = (const TCHAR*)WideRawData;
#else
			IniLocData = (const TCHAR*)RawData;
#endif
		}
		else
		{
			IniLocData = (const ANSICHAR*)FileData.GetTypedData();
		}
		// Merge the string into the config file
		ConfigFile->CombineFromBuffer(*FileName,*IniLocData);
		TArray<UClass*> Classes;
		INT StartIndex = 0;
		INT EndIndex = 0;
		// Find the set of object classes that were affected
		while (StartIndex >= 0 && StartIndex < IniLocData.Len() && EndIndex >= StartIndex)
		{
			// Find the next section header
			StartIndex = IniLocData.InStr(TEXT("["),FALSE,FALSE,StartIndex);
			if (StartIndex > -1)
			{
				// Find the ending section identifier
				EndIndex = IniLocData.InStr(TEXT("]"),FALSE,FALSE,StartIndex);
				if (EndIndex > StartIndex)
				{
					// Snip the text out and try to find the class for that
					const FString ClassName = IniLocData.Mid(StartIndex + 1,EndIndex - StartIndex - 1);
					INT PerObjectNameIndex = ClassName.InStr(TEXT(" "));
					// Per object config entries will have a space in the name, but classes won't
					if (PerObjectNameIndex == -1)
					{
						// Find the class for this so we know what to update
						UClass* Class = FindObject<UClass>(NULL,*ClassName,TRUE);
						if (Class)
						{
							// Add this to the list to check against
							Classes.AddItem(Class);
						}
					}
					// Handle the per object config case by finding the object and having it reload
					else
					{
						const FString PerObjectName = ClassName.Left(PerObjectNameIndex);
						// Explicitly search the transient package (won't update non-transient objects)
						UObject* PerObject = FindObject<UObject>(ANY_PACKAGE,*PerObjectName,FALSE);
						if (PerObject)
						{
							if (bIsLocFile)
							{
								debugf(TEXT("Reloading loc data for %s with file %s"),*PerObject->GetName(),*FileName);
								// Force a reload of the localized vars
								PerObject->ReloadLocalized();
							}
							else
							{
								debugf(TEXT("Reloading config for %s with file %s"),*PerObject->GetName(),*FileName);
								// Force a reload of the config vars
								PerObject->ReloadConfig();
							}
						}
					}
					StartIndex = EndIndex;
				}
			}
		}
		DOUBLE StartTime = appSeconds();
		if (Classes.Num())
		{
			// Now that we have a list of classes to update, we can iterate objects and reload
			for (FObjectIterator It; It; ++It)
			{
				UClass* Class = It->GetClass();
				// Don't do anything for non-config/localized classes
				if (Class->HasAnyClassFlags(CLASS_Config | CLASS_Localized))
				{
					// Check to see if this class is in our list
					for (INT ClassIndex = 0; ClassIndex < Classes.Num(); ClassIndex++)
					{
						if (It->IsA(Classes(ClassIndex)))
						{
							// Don't reload config if this isn't a config file or the class isn't a config class
							if (bIsLocFile == FALSE && Class->HasAnyClassFlags(CLASS_Config))
							{
								// Force a reload of the config vars
								It->ReloadConfig();
							}
							// Don't reload loc if this isn't a loc file or the class isn't a loc-ed class
							if (bIsLocFile && Class->HasAnyClassFlags(CLASS_Localized))
							{
								// Force a reload of the localized vars
								It->ReloadLocalized();
							}
						}
					}
				}
			}
		}
		debugf(TEXT("Updating config/loc from %s took %f seconds"),*FileName,(FLOAT)(appSeconds() - StartTime));
	}
}

/**
 * Gets the proper language extension for the loc file
 *
 * @param FileName the file name being modified
 *
 * @return the modified file name for this language setting
 */
FString UIniLocPatcher::UpdateLocFileName(const FString& FileName)
{
	const FString LangExt = appGetLanguageExt();
	// Only try to rename .INT files if we aren't in English
	if (LangExt != TEXT("int"))
	{
		FFilename LocFile(FileName);
		const FString Extension = LocFile.GetExtension();
		// If the loc file is int, switch to the specific language extension
		if (Extension == TEXT("int"))
		{
			return LocFile.GetBaseFilename() + TEXT(".") + LangExt;
		}
	}
	return FileName;
}

