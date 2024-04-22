/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// GenericBrowserType_Font: Fonts
//=============================================================================

class GenericBrowserType_Font
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	/**
	 * Displays the font properties window for editing & importing/exporting of
	 * font pages
	 *
	 * @param InObject the object being edited
	 */
	virtual UBOOL ShowObjectEditor( UObject* InObject );


	/**
	 * Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
	 *
	 * @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
	 * @param	OutCommands		The list of custom commands to support
	 */
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;


	/**
	 * Invokes a custom menu item command.
	 *
	 * @param InCommand		The command to execute
	 * @param InObject		The object to invoke the command against
	 */
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
}
	
defaultproperties
{
	Description="Fonts"
}
