/**
 * Base class for data providers which provide data pulled directly from member UProperties.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIPropertyDataProvider extends UIDataProvider
	native(inherit)
	abstract;

/**
 * the list of property classes for which values cannot be automatically derived; if your script-only child class has a member
 * var of one of these types, you'll need to provide the value yourself via the GetCustomPropertyValue event
 */
var const	array<class<Property> >		ComplexPropertyTypes;

/**
 * Allows script only data stores to indicate whether they'd like to handle a property which is not natively supported.
 *
 * @param	UnsupportedProperty		the property that isn't supported natively
 *
 * @return	TRUE if this data provider wishes to perform custom logic to handle the property.
 */
delegate bool CanSupportComplexPropertyType( Property UnsupportedProperty );

DefaultProperties
{
	ComplexPropertyTypes(0)=class'StructProperty'
	ComplexPropertyTypes(1)=class'MapProperty'
	ComplexPropertyTypes(2)=class'ArrayProperty'
	ComplexPropertyTypes(3)=class'DelegateProperty'
}
