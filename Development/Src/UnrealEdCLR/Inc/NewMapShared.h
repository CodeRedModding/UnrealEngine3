/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef __NEW_MAP_H__
#define __NEW_MAP_H__

#include "InteropShared.h"

#ifdef __cplusplus_cli

// Forward declarations
ref class MWPFFrame;
ref class MNewMapPanel;

#endif // #ifdef __cplusplus_cli

/** Non-managed singleton wrapper around the new map screen */
class FNewMapScreen
{
public:
	/** Display the new map screen
	*
	* @param	Templates - List of templates to show in the new map screen (not including the blank map option)
	*
	* @param	OutTemplateName	- (out) The template selected by the user. Empty if blank map selected.
	*
	* @return	TRUE if the user selected a valid item, FALSE if the user cancelled
	*
	*/
	static BOOL DisplayNewMapScreen(const TArray<UTemplateMapMetadata*>& Templates, FString& OutTemplateName);

	/** Shut down the new map screen singleton */
	static void Shutdown();

private:
	/** Constructor */
	FNewMapScreen();

	/** Destructor */
	~FNewMapScreen();

	// Copy constructor and assignment operator intentionally left unimplemented
	FNewMapScreen( const FNewMapScreen& );
	FNewMapScreen& operator=( const FNewMapScreen& );

	/**
	 * Return internal singleton instance of the class
	 *
	 * @return	Reference to the internal singleton instance of the class
	 */
	static FNewMapScreen& GetInternalInstance();

	/** Frame used for the new map screen */
	GCRoot( MWPFFrame^ ) NewMapScreenFrame;

	/** Panel used for the welcome screen */
	GCRoot( MNewMapPanel^ ) NewMapPanel;

	/** Singleton instance of the class */
	static FNewMapScreen* Instance;
};

#endif // #ifndef __NEW_MAP_H__
