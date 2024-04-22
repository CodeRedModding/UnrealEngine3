/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//-----------------------------------------------------------
// Browser type for archetype classes
//-----------------------------------------------------------
class GenericBrowserType_Archetype extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();

	/**
	 * Determines whether the specified object is an archetype that should be handled by this generic browser type.
	 *
	 * @param	Object	a pointer to a object with the RF_ArchetypeObject flag
	 *
	 * @return	TRUE if this generic browser type supports to object specified.
	 */
	static UBOOL IsArchetypeSupported( UObject* Object );


	/**
	 * Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
	 *
	 * @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
	 * @param	OutCommands		The list of custom commands to support
	 */
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;

	/**
	 * Returns the default command to be executed given the selected object.
	 *
	 * @param	InObject		The objects to query the default command for
	 *
	 * @return The ID of the default action command (i.e. command that happens on double click or enter).
	 */
	virtual INT QueryDefaultCommand( TArray<UObject*>& InObjects ) const;
}

DefaultProperties
{
	Description="Archetypes"
}
