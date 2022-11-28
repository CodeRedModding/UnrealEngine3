//=============================================================================
// GeomModifier: Base class of all geometry mode modifiers.
//=============================================================================

class GeomModifier
	extends Object
	abstract
	hidecategories(Object,GeomModifier)
	native;

/** A human readable name for this modifier (appears on buttons, menus, etc) */
var(GeomModifier) string Description;

/** If true, this modifier should be displayed as a push button instead of a radio button */
var(GeomModifier) bool bPushButton;

/**
 * If true, the modifier has been initialized.
 *
 * This is useful for interpreting user input and mouse drags correctly.
 */
var(GeomModifier) bool bInitialized;

cpptext
{
	FString GetModifierDescription() { return Description; }
	
	virtual UBOOL InputDelta(struct FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	
	/**
	 * Displays an error message for a modifier.
	 */
	void GeomError( FString InMsg );
	
	/**
	 * Applies any values the user may have entered into this modifiers
	 * "Keyboard" section of variables in the property window.
	 */
	 
	virtual UBOOL Supports( int InSelType ) { return 1; }
	
	/**
	 * Starts and ends the modification of geometry data.
	 */
	 
	virtual UBOOL StartModify();
	virtual UBOOL EndModify();
	
	/**
	 * Gives the individual modifiers a chance to do something the first time they are activated.
	 */
	virtual void Initialize() {}
	
	/**
	 * Determines if this modifier supports a specific selection mode.
	 *
	 * @param	InSelType	The selection type being checked
	 */
	 
	virtual void Apply() {}

	/**
	 * Handles the starting/stopping of transactions against the selected ABrushes
	 */

	void StartTrans();
	void EndTrans();
}

defaultproperties
{
	Description="None"
	bPushButton=False
	bInitialized=False
}
