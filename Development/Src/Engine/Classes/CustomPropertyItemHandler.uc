/**
 * Provides an interface for overriding the default behavior of routing and applying property values received
 * from a custom editor property window item.
 *
 * Classes which implement this interface would also need to specify custom property item windows
 * @see UnrealEd.CustomPropertyItemBindings
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
interface CustomPropertyItemHandler
	native;

cpptext
{
	/**
	 * Determines whether the specified property value matches the current value of the property.  Called after the user
	 * has changed the value of a property handled by a custom property window item.  Is used to determine whether Pre/PostEditChange
	 * should be called for the selected objects.
	 *
	 * @param	InProperty			the property whose value is being checked.
	 * @param	NewPropertyValue	the value to compare against the current value of the property.
	 * @param	ArrayIndex			the array index for the element being compared; only relevant for array properties
	 *
	 * @return	TRUE if NewPropertyValue matches the current value of the property specified, indicating that no effective changes
	 *			were actually made.
	 */
	virtual UBOOL IsCustomPropertyValueIdentical( UProperty* InProperty, const union UPropertyValue& NewPropertyValue, INT ArrayIndex=INDEX_NONE )=0;

	/**
	 * Method for overriding the default behavior of applying property values received from a custom editor property window item.
	 *
	 * @param	InProperty		the property that is being edited
	 * @param	PropertyValue	the value to assign to the property
	 * @param	ArrayIndex		the array index for the element being changed; only relevant for array properties
	 *
	 * @return	TRUE if the property was handled by this object and the property value was successfully applied to the
	 *			object's data.
	 */
	virtual UBOOL EditorSetPropertyValue( UProperty* InProperty, const union UPropertyValue& PropertyValue, INT ArrayIndex=INDEX_NONE )=0;
}

