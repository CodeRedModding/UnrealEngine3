/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * This class is the base class for any kind of object that wants the following features:
 *  - be a subobject inside a class definition (called the owner class)
 *  - values of the component can be overridden in a subclass of the owner class, by
 *    defining a component in the subclass with the same name as the component in the base class
 *    definition. Note, you CANNOT GIVE IT A CLASS= DEFINITION! (See UDN page for more info)
 *  - Changes to the default values to the component will be propagated to components that are
 *    created inside a map, unless the value was changed away from the default value in the editor.
 */
class Component extends Object
	native
	abstract;

var const native Class	TemplateOwnerClass;
var const native name	TemplateName;

cpptext
{
	/**
	 * Given a subobject and an owner class, save a refernce to it for retrieveing defaults on load
	 * @param OriginalSubObject	The original template for this subobject (or another instance for a duplication?)
	 * @param OwnerClass			The class that contains the original template
	 * @param SubObjectName		If the OriginalSubObject is NULL, manually set the name of the subobject to this
	 */
	void LinkToSourceDefaultObject(UComponent* OriginalComponent, UClass* OwnerClass, FName ComponentName = NAME_None);

	/**
	 * Copies the SourceDefaultObject onto our own memory to propagate any modified defaults
	 * @param Ar	The archive used to serialize the pointer to the subobject template
	 */
	void PreSerialize(FArchive& Ar);

	/**
	 * Copies the Source DefaultObject onto our own memory to propagate any modified defaults
	 * @return The object pointed to by the SourceDefaultActorClass and SourceDefaultSubObjectName
	 */
	UComponent* ResolveSourceDefaultObject();

	/**
	 * Returns name to use for this component in component instancing maps.
	 *
	 * @return 	a name for this component which is unique within a single object graph.
	 */
	FName GetInstanceMapName() const;

	/**
	 * Returns whether this component was instanced from a component template.
	 *
	 * @return	TRUE if this component was instanced from a template.  FALSE if this component was created manually at runtime.
	 */
	UBOOL IsInstanced() const;

	/**
	 * Returns whether native properties are identical to the one of the passed in component.
	 *
	 * @param	Other	Other component to compare against
	 *
	 * @return TRUE if native properties are identical, FALSE otherwise
	 */
	virtual UBOOL AreNativePropertiesIdenticalTo( UComponent* Other ) const;

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	return TRUE if property values were added to the map.
	 */
	virtual UBOOL GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags=0 ) const;

	// UObject interface.

	virtual UBOOL IsPendingKill() const;

	/**
	 * @return if this object is a UComponent or subclass
	 */
	virtual UBOOL IsAComponent() const
	{
		return TRUE;
	}
}
