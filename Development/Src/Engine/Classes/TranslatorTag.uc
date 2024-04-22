/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * A TranslatorTag is part of the system that translates text with embedded
 * tags. E.g.: Press <Controller:GBA_POI/> to look at point of interest.
 * In this example, 'Controller' is a tag, and we want it to process GBA_POI.
 * The Controller TranslatorTag would look up the controls for the associated player
 * and return a text representation of the control.
 * 
 * See TranslationContext for more info.
 */
class TranslatorTag extends Object
	native;

/**
 * The Tag that this Translator handles.
 * E.g. In the case of <Controller:GBA_POI/> the tag is 'Controller'.
 */
var name Tag;

/**
 * Return the translated text given the option.
 * E.g. In the case of <Controller:GBA_POI/> the argument is 'GBA_POI'.
 *
 * @param InArgument The text after the : in the tag.
 */
native function string Translate( String InArgument );

defaultproperties
{
}
