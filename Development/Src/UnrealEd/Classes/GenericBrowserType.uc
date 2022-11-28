
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

struct GenericBrowserTypeInfo
{
	var class	Class;
	var color	BorderColor;
	var int		ContextMenu;	// Pointer to a context menu object (wxMenu* on native side)
};

// A list of information that this type supports.
var array<GenericBrowserTypeInfo> SupportInfo;

// The color of the border drawn around this type in the browser.
var color BorderColor;

cpptext
{
	FString GetBrowserTypeDescription() { return Description; }
	FColor GetBorderColor( UObject* InObject );

	/**
	 * Does any initial set up that the type requires.
	 */
	virtual void Init() {}
	
	/**
	 * Checks to see if the specified class is handled by this type.
	 *
	 * @param	InObject	The object we need to check if we support
	 */
	UBOOL Supports( UObject* InObject );
	
	/**
	 * Creates a context menu specific to the type of object passed in.
	 *
	 * @param	InObject	The object we need the menu for
	 */
	wxMenu* GetContextMenu( UObject* InObject );
	
	/**
	 * Invokes the editor for an object.
	 *
	 * @param	InObject	The object to invoke the editor for
	 */
	virtual UBOOL ShowObjectEditor( UObject* InObject ) { return 0; }
	
	/**
	 * Invokes the editor for all selected objects.
	 */
	virtual UBOOL ShowObjectEditor();
	
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

	virtual void InvokeCustomCommand( INT InCommand, UObject* InObject ) {}
	
	/**
	 * Calls the virtual "DoubleClick" function for each object
	 * of a supported class.
	 */
	 
	virtual void DoubleClick();

	/**
	 * Allows each type to handle double clicking as they see fit.
	 */
	 
	virtual void DoubleClick( UObject* InObject );
}

defaultproperties
{
	Description="None"
}
