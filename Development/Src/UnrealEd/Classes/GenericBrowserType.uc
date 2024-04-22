/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// GenericBrowserType
//
// This class provides a generic interface for extending the generic browsers
// base list of resource types.

class GenericBrowserType
	extends Object
	abstract
	hidecategories(Object,GenericBrowserType)
	native;

// A human readable name for this modifier
var string Description;


/** Describes a command that a type of object supports */
struct transient native ObjectSupportedCommandType
{
	/** The ID we'll use to identify this command.  It's a constant. */
	var int CommandID;

	/** The index of the parent menu item */
	var int ParentIndex;

	/** How this command should appear in a context menu (localized!) */
	var string LocalizedName;

	/** Whether the command should appear enabled (true) or greyed-out (false) */
	var bool bIsEnabled;

	structcpptext
	{
		/** Constructor that initializes all elements */
		FObjectSupportedCommandType( INT InCommandID, const FString& InLocalizedName, UBOOL bInIsEnabled = TRUE, INT InParentIndex = -1 )
			: CommandID( InCommandID ),
			  LocalizedName( InLocalizedName ),
			  bIsEnabled( bInIsEnabled ),
			  ParentIndex( InParentIndex )
		{
		}
	}
};


struct native GenericBrowserTypeInfo
{
	/** the class associated with this browser type */
	var const class				Class;

	/** the color to use for rendering objects of this type */
	var const color				BorderColor;

	/** if specified, only objects that have these flags will be considered */
	var native const qword		RequiredFlags;

	/** Pointer to the GenericBrowserType that should be called to handle events for this type. */
	var GenericBrowserType		BrowserType;

	/** Callback used to determine whether object is Supported*/
	var native pointer			IsSupportedCallback;

structcpptext
{
	typedef UBOOL (*GenericBrowserSupportCallback)(UObject* Object);

	FGenericBrowserTypeInfo(
		UClass* InClass,
		const FColor& InBorderColor,
		QWORD InRequiredFlags = 0,
		UGenericBrowserType* InBrowserType = NULL,
		GenericBrowserSupportCallback InIsSupportedCallback = NULL
	)
	:	Class(InClass)
	,	RequiredFlags(InRequiredFlags)
	,	BorderColor(InBorderColor)
	,	BrowserType(InBrowserType)
	,	IsSupportedCallback(InIsSupportedCallback)
	{}

	UBOOL Supports( UObject* Object ) const
	{
		UBOOL bResult = FALSE;
		if ( Object->IsA(Class) )
		{
			bResult = TRUE;
			if ( RequiredFlags != 0 )
			{
				bResult = Object->HasAllFlags(RequiredFlags);
			}
			if( bResult && IsSupportedCallback )
			{
				GenericBrowserSupportCallback Callback = (GenericBrowserSupportCallback) IsSupportedCallback;
				bResult = Callback( Object );
			}
		}
		return bResult;
	}

	inline UBOOL operator==( const FGenericBrowserTypeInfo& Other ) const
	{
		return ( Class == Other.Class && RequiredFlags == Other.RequiredFlags );
	}
}
};

// A list of information that this type supports.
var native array<GenericBrowserTypeInfo> SupportInfo;

// The color of the border drawn around this type in the browser.
var color BorderColor;

cpptext
{
	/**
	 * @return Returns the browser type description string.
	 */
	const FString& GetBrowserTypeDescription() const
	{
		return Description;
	}

	FColor GetBorderColor( UObject* InObject );

	/**
	 * Does any initial set up that the type requires.
	 */
	virtual void Init() 
	{
	}

	/**
	 * Clear out any old data before calling Init() again
	 */
	virtual void Clear();

	/**
	 * Checks to see if the specified class is handled by this type.
	 *
	 * @param	InObject	The object we need to check if we support
	 */
	UBOOL Supports( UObject* InObject ) const;

	/**
	 * Creates a context menu specific to the type of objects that are selected.
	 *
	 * @param	Selection	The selected object set.
	 */
	class WxMBGenericBrowserContextBase* GetContextMenu( USelection* Selection );

	/**
	 * Invokes the editor for an object.  The default behaviour is to
	 * open a property window for the object.  Dervied classes can override
	 * this with eg an editor which is specialized for the object's class.
	 *
	 * @param	InObject	The object to invoke the editor for.
	 */
	virtual UBOOL ShowObjectEditor( UObject* InObject )
	{
		return ShowObjectProperties( InObject );
	}

	/**
	 * Opens a property window for the specified object.  By default, GEditor's
	 * notify hook is used on the property window.  Derived classes can override
	 * this method in order to eg provide their own notify hook.
	 *
	 * @param	InObject	The object to invoke the property window for.
	 */
	virtual UBOOL ShowObjectProperties( UObject* InObject );

	/**
	 * Opens a property window for the specified objects.  By default, GEditor's
	 * notify hook is used on the property window.  Derived classes can override
	 * this method in order to eg provide their own notify hook.
	 *
	 * @param	InObjects	The objects to invoke the property window for.
	 */
	virtual UBOOL ShowObjectProperties( const TArray<UObject*>& InObjects );

	/**
	 * Invokes the editor for all selected objects.
	 */
	virtual UBOOL ShowObjectEditor();

	/**
	 * Displays the object properties window for all selected objects that this
	 * GenericBrowserType supports.
	 */
	UBOOL ShowObjectProperties();


	/**
	 * Static: Returns a list of standard context menu commands supported by the specified objects
	 *
	 * @param	InObjects		The objects to query commands for (if NULL, query commands for all objects of this type.)
	 * @param	OutCommands		The list of custom commands to support
	 */
	static void QueryStandardSupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands );


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


	/**
	 * Invokes a custom menu item command for every selected object
	 * of a supported class.
	 *
	 * @param InCommand		The command to execute
	 */

	virtual void InvokeCustomCommand( INT InCommand );

	/**
	 * Invokes a custom menu item command.
	 *
	 * @param InCommand		The command to execute
	 * @param InObject		The object to invoke the command against
	 */

	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects ) 
	{
	}

	/**
	 * Calls the virtual "DoubleClick" function for each object
	 * of a supported class.
	 */

	virtual void DoubleClick();

	/**
	 * Allows each type to handle double clicking as they see fit.
	 */

	virtual void DoubleClick( UObject* InObject );

	/**
	 * Retrieves a list of objects supported by this browser type which
	 * are currently selected in the generic browser.
	 */
	void GetSelectedObjects( TArray<UObject*>& Objects );

	/**
	 * Determines whether the specified package is allowed to be saved.
	 */
	virtual UBOOL IsSavePackageAllowed( UPackage* PackageToSave );

protected:
	/**
	 * Determines whether the specified package is allowed to be saved.
	 *
	 * @param	PackageToSave		the package that is about to be saved
	 * @param	StandaloneObjects	a list of objects from PackageToSave which were marked RF_Standalone
	 */
	virtual UBOOL IsSavePackageAllowed( UPackage* PackageToSave, TArray<UObject*>& StandaloneObjects ) 
	{ 
		return TRUE; 
	}


	/**
	 * Static: Returns true if any of the specified objects have already been cooked
	 */
	static UBOOL AnyObjectsAreCooked( USelection* InObjects );


public:
	/**
	 * Called when the user chooses to delete objects from the generic browser.  Gives the resource type the opportunity
	 * to perform any special logic prior to the delete.
	 *
	 * @param	ObjectToDelete	the object about to be deleted.
	 *
	 * @return	TRUE to allow the object to be deleted, FALSE to prevent the object from being deleted.
	 */
	virtual UBOOL NotifyPreDeleteObject( UObject* ObjectToDelete ) 
	{ 
		return TRUE; 
	}

	/**
	 * Called when the user chooses to delete objects from the generic browser, after the object has been checked for referencers.
	 * Gives the resource type the opportunity to perform any special logic after the delete.
	 *
	 * @param	ObjectToDelete		the object that was deleted.
	 * @param	bDeleteSuccessful	TRUE if the object wasn't referenced and was successfully marked for deletion.
	 */
	virtual void NotifyPostDeleteObject() 
	{
	}
}

defaultproperties
{
}
