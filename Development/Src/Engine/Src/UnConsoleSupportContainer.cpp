/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "UnConsoleSupportContainer.h"

// we don't need any of this on console
#if !CONSOLE && _MSC_VER

/////////////////////////////////////////////
//
// class FConsoleSupportContainer
//
/////////////////////////////////////////////
FConsoleSupportContainer* FConsoleSupportContainer::TheConsoleSupportContainer = NULL;

/**
 * Cleans up instances of FConsoleSupport.
 */
FConsoleSupportContainer::~FConsoleSupportContainer()
{
}

/**
 * Find all Console Support DLLs in the subdirectories of Binaries 
 * loads them and registers the FConsoleSupport subclass defined in the DLL.
 *
 * @param PlatformFilter If specified, only this platform will be loaded (THIS MUST BE THE SUBDIRECTORY NAME OF BINARIES!!! Be careful of PC/PCConsole vs Win32/Win64!!!)
 */
void FConsoleSupportContainer::LoadAllConsoleSupportModules(const TCHAR* PlatformName)
{
#if !UE3_LEAN_AND_MEAN
	if (appGetPlatformType() == UE3::PLATFORM_WindowsServer)
	{
		//Don't load this on dedicated servers
		return;
	}

	FString BaseToolsDirectory = FString( appBaseDir() ) * TEXT( ".." ) PATH_SEPARATOR;

	// get a list of all the subdirectories of the Binaries (current) directory
	TArray<FString> Subdirs;
	GFileManager->FindFiles(Subdirs, *( BaseToolsDirectory * TEXT( "*" ) ), 0, 1);

	// loop through all the ones we have found
	for (INT DirIndex = 0; DirIndex < Subdirs.Num(); DirIndex++)
	{
		// filter out non-desired platforms
		if (PlatformName && Subdirs(DirIndex) != PlatformName)
		{
			continue;
		}

		FString SearchDirectory = BaseToolsDirectory * Subdirs(DirIndex);

		// Make sure Windows knows where to look for DLLs
		SetDllDirectory(*SearchDirectory);

		// get a list of all the DLLs in this subdirectory which have Tools in the name
#if _WIN64
		FString ToolsDLLWildcard( TEXT( "*Tools_x64.dll" ) );
#else
		FString ToolsDLLWildcard( TEXT( "*Tools.dll" ) );
#endif

		TArray<FString> SupportDLLs;
		GFileManager->FindFiles(SupportDLLs, *( SearchDirectory * ToolsDLLWildcard), 1, 0);

		// loop through all the ones we have found
		for (INT DLLIndex = 0; DLLIndex < SupportDLLs.Num(); DLLIndex++)
		{
			// Load the DLL, so we can check for the GetConsoleSupport function
			void* ConsoleDLL = LoadLibrary(*SupportDLLs(DLLIndex));

			// did we load the DLL?
			if (ConsoleDLL)
			{
				// look for the main entry point function that returns a pointer to the ConsoleSupport subclass
				FuncGetConsoleSupport Proc = (FuncGetConsoleSupport)appGetDllExport(ConsoleDLL, TEXT("GetConsoleSupport"));
				FConsoleSupport* ConsoleSupport = Proc ? Proc(ConsoleDLL) : NULL;
				if (ConsoleSupport)
				{
					// if we found the function, call it and register the support object
					GetConsoleSupportContainer()->AddConsoleSupport(ConsoleSupport);
				}
				else
				{
					// unload the non-console support DLL
					appFreeDllHandle(ConsoleDLL);
				}
			}
		}
	}
#endif // !UE3_LEAN_AND_MEAN
}

/**
 * Adds the given Console support object to the list of all Console supports
 * This is usually called from the FindSupportModules function, not externally.
 *
 * @param	InConsoleSupport	The FConsoleSupport to add
 */
void FConsoleSupportContainer::AddConsoleSupport(FConsoleSupport* InConsoleSupport)
{
	// add on the support!
	ConsoleSupports.AddUniqueItem(InConsoleSupport);
	FString GameName = appGetGameName();
	GameName += TEXT("Game");

	// enumerate the targets early so that the Menu Items set in Initialize are proper
	if (appStricmp(CONSOLESUPPORT_NAME_360, InConsoleSupport->GetPlatformName()) == 0 ||
		appStricmp(CONSOLESUPPORT_NAME_PS3, InConsoleSupport->GetPlatformName()) == 0 ||
		appStricmp(CONSOLESUPPORT_NAME_WIIU, InConsoleSupport->GetPlatformName()) == 0 ||
		appStricmp(CONSOLESUPPORT_NAME_IPHONE, InConsoleSupport->GetPlatformName()) == 0 ||
		appStricmp(CONSOLESUPPORT_NAME_NGP, InConsoleSupport->GetPlatformName()) == 0 ||
		appStricmp(CONSOLESUPPORT_NAME_FLASH, InConsoleSupport->GetPlatformName()) == 0)
	{
		InConsoleSupport->EnumerateAvailableTargets();
	}

#ifdef _DEBUG
	InConsoleSupport->Initialize(*GameName, TEXT("Debug"));
#else
	InConsoleSupport->Initialize(*GameName, TEXT("Release"));
#endif
}

/**
 * Returns the total number of Console supports registered so far
 *
 * @return	Current Console support
 */
INT FConsoleSupportContainer::GetNumConsoleSupports() const
{
	return ConsoleSupports.Num();
}

/**
 * Returns the Console support as requested by index
 *
 * @return	Requested Console support object
 */
FConsoleSupport* FConsoleSupportContainer::GetConsoleSupport(INT Index) const
{
	// validate index
	check(Index >= 0 && Index < ConsoleSupports.Num());

	// return the requested support
	return ConsoleSupports(Index);
}

/**
 * Returns the Console support as requested by string name
 *
 * @return	Requested Console support object
 */
FConsoleSupport* FConsoleSupportContainer::GetConsoleSupport(const TCHAR* ConsoleName) const
{
	// go through all of the Consoles
	for (FConsoleSupportIterator It; It; ++It)
	{
		// if we match the name, return it!
		if (appStricmp(ConsoleName, It->GetPlatformName()) == 0)
		{
			return *It;
		}
	}

	// if we didn't find one, return NULL
	return NULL;
}

/////////////////////////////////////////////
//
// class FConsoleSupportIterator
//
/////////////////////////////////////////////

/**
 * Default constructor, initializing all member variables.
 */
FConsoleSupportIterator::FConsoleSupportIterator()
{
	// initialize our counter
	SupportIndex = 0;
	// check if we are already finished
	ReachedEnd = (FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports() == 0);
}
/**
 * Iterates to next suitable support
 */
void FConsoleSupportIterator::operator++()
{
	// go to the next support, and see if we are done
	SupportIndex++;
	if (SupportIndex == FConsoleSupportContainer::GetConsoleSupportContainer()->GetNumConsoleSupports())
		ReachedEnd = true;
}
/**
 * Returns the current Console support pointed at by the Iterator
 *
 * @return	Current Console support
 */
FConsoleSupport* FConsoleSupportIterator::operator*()
{
	return FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(SupportIndex);
}
/**
 * Returns the current Console support pointed at by the Iterator
 *
 * @return	Current Console support
 */
FConsoleSupport* FConsoleSupportIterator::operator->()
{
	return FConsoleSupportContainer::GetConsoleSupportContainer()->GetConsoleSupport(SupportIndex);
}
/**
 * Returns whether the iterator has reached the end and no longer points
 * to a Console support.
 *
 * @return TRUE if iterator points to a Console support, FALSE if it has reached the end
 */
FConsoleSupportIterator::operator UBOOL()
{
	return !ReachedEnd;
}


#else

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
INT UnConsoleSupportContainerLinkerHelper;

#endif // !CONSOLE && _MSC_VER

