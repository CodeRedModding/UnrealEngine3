/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Translates tags of the form <Strings:Blah.Foo.Bar/> via the localization system.
 */
class StringsTag extends TranslatorTag
	native;

/**
 * Return the translated text given the option.
 * E.g. In the case of <Controller:GBA_POI/> the argument is 'GBA_POI'.
 *
 * @param InArgument The text after the : in the tag.
 */
native function string Translate( String InArgument );

DefaultProperties
{
	Tag="Strings"
}