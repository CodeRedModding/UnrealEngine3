/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __UNCONSOLESUPPORTCONTAINER_H__
#define __UNCONSOLESUPPORTCONTAINER_H__

// don't include this on consoles or in gcc
#if !CONSOLE && (defined(_MSC_VER) || PLATFORM_UNIX)

#include "UnConsoleTools.h"

/**
 * Container class that holds pointers to all of the loaded Console-specific
 * console support objects.
 */

class FConsoleSupportContainer
{
public:
	/**
	 * Cleans up instances of FConsoleSupport.
	 */
	~FConsoleSupportContainer();

	/**
	 *	We will let the system clean this up when we exit...
	 */
	static FConsoleSupportContainer* GetConsoleSupportContainer()
	{
		if (TheConsoleSupportContainer == NULL)
		{
			TheConsoleSupportContainer = new FConsoleSupportContainer();
		}
		return TheConsoleSupportContainer;
	}
	
	/**
	 * Find all Console Support DLLs in the Binaries\\ConsoleSupport directory,
	 * loads them and registers the FConsoleSupport subclass defined in the DLL
	 *
	 * @param PlatformFilter If specified, only this platform will be loaded
	 */
	void LoadAllConsoleSupportModules(const TCHAR* PlatformFilter=NULL);

	/**
	 * Adds the given Console support object to the list of all Console supports
	 * This is usually called from the FindSupportModules function, not externally.
	 *
	 * @param	InConsoleSupport	The FConsoleSupport to add
	 */
	void AddConsoleSupport(FConsoleSupport* InConsoleSupport);

	/**
	 * Returns the total number of Console supports registered so far
	 *
	 * @return	Current Console support
	 */
	INT GetNumConsoleSupports() const;

	/**
	 * Returns the Console support as requested by index
	 * @param	Index	The index of the support to return, must be 0 <= Index < GetNumConsoleSupports()
	 *
	 * @return	Requested Console support object
	 */
	FConsoleSupport* GetConsoleSupport(INT Index) const;

	/**
	 * Returns the Console support as requested by string name
	 * @param	ConsoleName	A string name to search on, searched via case insensitive comparison
	 *
	 * @return	Requested Console support object
	 */
	FConsoleSupport* GetConsoleSupport(const TCHAR* ConsoleName) const;

protected:
	/** The list of all registered Console support classes */
	TArray<FConsoleSupport*> ConsoleSupports;
	static FConsoleSupportContainer* TheConsoleSupportContainer;
};

/**
 * An iterator class that makes it easy to iterate over all of the ConsoleSupports
 * currently registered in the ConsoleSupportContainer.
 */
class FConsoleSupportIterator
{
public:
	/**
	 * Default constructor, initializing all member variables.
	 */
	FConsoleSupportIterator();

	/**
	 * Iterates to next suitable support
	 */
	void operator++();
	/**
	 * Returns the current Console support pointed at by the Iterator
	 *
	 * @return	Current Console support
	 */
	FConsoleSupport* operator*();
	/**
	 * Returns the current Console support pointed at by the Iterator
	 *
	 * @return	Current Console support
	 */
	FConsoleSupport* operator->();
	/**
	 * Returns whether the iterator has reached the end and no longer points
	 * to a Console support.
	 *
	 * @return TRUE if iterator points to a Console support, FALSE if it has reached the end
	 */
	operator UBOOL();

protected:

	/** Current index into the Console support							*/
	INT		SupportIndex;
	/** Whether we already reached the end								*/
	UBOOL	ReachedEnd;
};

#endif // !CONSOLE && defined(_MSC_VER)
#endif
