/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// GenericBrowserType_Custom: Custom resource types
//=============================================================================

class GenericBrowserType_Custom
	extends GenericBrowserType
	native;

cpptext
{
	/**
	 * Invokes the editor for all selected objects.
	 *
	 * This version loops through all of the supported classes for the custom type and
	 * calls the appropriate implementation of the function.
	 */
	virtual UBOOL ShowObjectEditor();

	/**
	 * Invokes the editor for an object.  The default behaviour is to
	 * open a property window for the object.  Dervied classes can override
	 * this with eg an editor which is specialized for the object's class.
	 *
	 * This version loops through all of the supported classes for the custom type and
	 * calls the appropriate implementation of the function.
	 *
	 * @param	InObject	The object to invoke the editor for.
	 */
	virtual UBOOL ShowObjectEditor( UObject* InObject );

	/**
	 * Opens a property window for the specified object.  By default, GEditor's
	 * notify hook is used on the property window.  Derived classes can override
	 * this method in order to eg provide their own notify hook.
	 *
	 * This version loops through all of the supported classes for the custom type and
	 * calls the appropriate implementation of the function.
	 *
	 * @param	InObject	The object to invoke the property window for.
	 */
	virtual UBOOL ShowObjectProperties( const TArray<UObject*>& InObjects );


	/**
	 * Invokes a custom menu item command for every selected object
	 * of a supported class.
	 *
	 * This version loops through all of the supported classes for the custom type and
	 * calls the appropriate implementation of the function.
	 *
	 * @param InCommand		The command to execute
	 */
	virtual void InvokeCustomCommand( INT InCommand );

	/**
	 * Calls the virtual "DoubleClick" function for each object
	 * of a supported class.
 	 *
	 * This version loops through all of the supported classes for the custom type and
	 * calls the appropriate implementation of the function.
	 */
	virtual void DoubleClick();
}

defaultproperties
{
	Description="_Custom_"
}
