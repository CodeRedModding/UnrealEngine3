class ActorFactory extends Object
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew
	config(Editor)
	abstract;

cpptext
{
	/** Called to actual create an actor at the supplied location/rotation, using the properties in the ActorFactory */
	virtual AActor* CreateActor(ULevel* Level, const FVector* const Location, const FRotator* const Rotation);

	/**
	 * If the ActorFactory thinks it could create an Actor with the current settings. 
	 * Used to determine if we should add to context menu for example.
	 */
	virtual UBOOL CanCreateActor() { return true; }

	/** Fill in parameters automatically, using GSelectionTools etc. */
	virtual void AutoFillFields() {}

	/** Name to put on context menu. */
	virtual FString GetMenuName() { return MenuName; }
}

/** Name used as basis for 'New Actor' menu. */
var string			MenuName;

/** Indicates how far up the menu item should be. The higher the number, the higher up the list.*/
var config int		MenuPriority;

/** Actor subclass this ActorFactory creates. */
var	class<Actor>	NewActorClass;

/** Whether to appear on menu (or this Factory only used through scripts etc.) */
var bool			bPlaceable;


defaultproperties
{
	MenuName="Add Actor"
	MenuPriority=0
	NewActorClass=class'Engine.Actor'
	bPlaceable=true
}