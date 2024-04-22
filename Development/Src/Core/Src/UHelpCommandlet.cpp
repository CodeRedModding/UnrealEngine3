/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This file contains the native portions of the help commandlet
 */

#include "CorePrivate.h"

/**
 * Builds a human readable name from the commandlets name
 *
 * @param Commandlet the commandlet to mangle the name of
 */
static FString GetCommandletName(UCommandlet* Commandlet)
{
	// Don't print out the default portion of the name
	static INT SkipLen = appStrlen(TEXT("Default__"));
	FString Name(*Commandlet->GetName() + SkipLen);
	// Strip off everything past the friendly name
	INT Index = Name.InStr(TEXT("Commandlet"));
	if (Index != -1)
	{
		Name = Name.Left(Index);
	}
	// Get the code package this commandlet is contained in
	UObject* Outer = Commandlet->GetOutermost();
	// Build Package.Commandlet as the name
	FString FullName = Outer->GetName();
	FullName += TEXT(".");
	FullName += Name;
	return FullName;
}

/**
 * Trys to load the UClass for the commandlet that was requested
 *
 * @param CommandletName the name of the commandlet to load
 */
static UClass* LoadCommandlet(const TCHAR* CommandletName)
{
	// Try to find the UClass for the commandlet (works for all but script classes)
	UClass* Class = FindObject<UClass>(ANY_PACKAGE,CommandletName,FALSE);

	// Don't accept classes that are not commandlets...
	if (!Class->IsChildOf(UCommandlet::StaticClass()))
	{
		Class = NULL;
	}

	// Name mangle by appending Commandlet
	FString AppendedName(CommandletName);
	AppendedName += TEXT("Commandlet");
	if (Class == NULL)
	{
		Class = FindObject<UClass>(ANY_PACKAGE,*AppendedName,FALSE);
		// Don't accept classes that are not commandlets...
		if (!Class->IsChildOf(UCommandlet::StaticClass()))
		{
			Class = NULL;
		}
	}
	// Let the user know that the commandlet wasn't found
	if (Class == NULL)
	{
		warnf(TEXT("Failed to load commandlet %s"),CommandletName);
	}
	return Class;
}

/**
 * Loads all of the UClass-es used by code so we can search for commandlets
 */
static void LoadAllClasses(void)
{
	TArray<FString> ScriptPackageNames;
	FConfigSection* Sec = GConfig->GetSectionPrivate( TEXT("UnrealEd.EditorEngine"), 0, 1, GEngineIni );
	if (Sec != NULL)
	{
		// Get the list of all code packages
		Sec->MultiFind( FName(TEXT("EditPackages")), ScriptPackageNames );
		// Iterate through loading each package
		for (INT Index = 0; Index < ScriptPackageNames.Num(); Index++)
		{
			// Loading without LOAD_Verify should load all objects
			UPackage* Package = UObject::LoadPackage(NULL,*ScriptPackageNames(Index),LOAD_NoWarn | LOAD_Quiet);
			if (Package == NULL)
			{
				warnf(TEXT("Error loading package %s"),*ScriptPackageNames(Index));
			}
		}
	}
	else
	{
		warnf(TEXT("Error finding the code packages"));
	}
}

/**
 * Loads all of the code packages completely (for script classes). Then iterates
 * through the UClass-es finding all commandlets and dumping information on them
 */
static void ListAllCommandlets(void)
{
	warnf(TEXT("%-40s    %s"),TEXT("Commandlet"),TEXT("Description"));
	warnf(TEXT("--------------------------------------------------------------------------------"));
	// Now iterate through all UClass-es looking for commandlets
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UCommandlet* DefObject = Cast<UCommandlet>(It->GetDefaultObject());
		if (DefObject != NULL)
		{
			// Ignore commandlets that can't be created
			if (It->HasAnyClassFlags(CLASS_Abstract) == FALSE)
			{
				const FString& Name = GetCommandletName(DefObject);
				warnf(TEXT("%-40s - %s"),*Name,*DefObject->HelpDescription);
			}
		}
	}
}

/**
 * Displays the extended help information for the commandlet
 *
 * @param Commandlet the commandlet to dump info for
 */
static void DumpCommandletInfo(UCommandlet* Commandlet)
{
	warnf(TEXT("Name:\t\t%s"),*GetCommandletName(Commandlet));
	warnf(TEXT("Description:\t%s"),*Commandlet->HelpDescription);
	warnf(TEXT("Usage:\t\t%s"),*Commandlet->HelpUsage);
	warnf(TEXT("Options:"));
	// Iterate through the options
	for (INT Index = 0; Index < Commandlet->HelpParamNames.Num(); Index++)
	{
		warnf(TEXT("\t%-20s - %s"),*Commandlet->HelpParamNames(Index),
			Commandlet->HelpParamDescriptions.IsValidIndex(Index) ? *Commandlet->HelpParamDescriptions(Index) : TEXT(""));
	}
	// Ask if they want to see the webhelp
	if (Commandlet->HelpWebLink.Len() > 0)
	{
		if (appMsgf(AMT_YesNo,TEXT("This commandlet has help information online. Do you wish to view it?")))
		{
			// Open a browser to the site specified in the default properties
			appLaunchURL(*Commandlet->HelpWebLink);
		}
	}
}

/**
 * Looks at the parameters and displays help based upon those parameters
 *
 * @param CmdLine the string containing the parameters for the commandlet
 */
INT UHelpCommandlet::Main(const FString& CmdLine)
{
	TArray<FString> Params;
	TArray<FString> Ignored;
	// Parse the command line into an array of strings
	ParseCommandLine(*CmdLine,Params,Ignored);
	// Validate the number of params passed in
	if (Params.Num() >= 1)
	{
		// Load everything up so we can find both native and script classes
		LoadAllClasses();
		warnf(TEXT(""));
		// Check for either "list" or "commandletname"
		if (Params.Num() == 1)
		{
			// Check for listing all commandlets
			if (appStricmp(TEXT("list"),*Params(0)) == 0)
			{
				ListAllCommandlets();
			}
			else
			{
				// Try to load the commandlet so we can get the help info
				UClass* TheClass = LoadCommandlet(*Params(0));
				if (TheClass != NULL)
				{
					DumpCommandletInfo(TheClass->GetDefaultObject<UCommandlet>());
				}
			}
		}
		// Must be the webhelp option
		else
		{
			// Try to load the commandlet so we can get the URL info
			UClass* TheClass = LoadCommandlet(*Params(1));
			if (TheClass != NULL)
			{
				// Get the default object so we can look at its props
				UCommandlet* DefObject = TheClass->GetDefaultObject<UCommandlet>();
				if (DefObject->HelpWebLink.Len() > 0)
				{
					// Open a browser to the site specified in the default properties
					appLaunchURL(*DefObject->HelpWebLink);
				}
			}
		}
	}
	else
	{
		warnf(TEXT("\r\nUsage:\r\n\r\n%s"),*HelpUsage);
	}
	return 0;
}
IMPLEMENT_CLASS(UHelpCommandlet);
