/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// GenericBrowserType_Material: Materials
//=============================================================================

class GenericBrowserType_Material
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& Objects );

	/**
	 * Returns a list of commands that this object supports (or the object type supports, if InObject is NULL)
	 *
	 * @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
	 * @param	OutCommands		The list of custom commands to support
	 */
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;


	/**
	* Callback to register whether or not the object should be displayed
	* @param InObject - object that will be displayed in the GB
	* @return TRUE if should be displayed
	*/ 
	static UBOOL ShouldDisplayCallback( UObject* InObject );
}
	
defaultproperties
{
	Description="Materials"
}
