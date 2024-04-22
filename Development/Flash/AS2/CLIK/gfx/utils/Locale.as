/**
 * The locale class manages translation calls from the component framework, interfacing directly with translation features of the player, or using the GameEngine to make calls to the game for translation.
 */

/**************************************************************************

Filename    :   Locale.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

class gfx.utils.Locale {
	
// Constants:	

// Static Interface
	/**
	 * Requested a Locale string from the Player. Currently, this is not wired to the GameEngine or Player.
	 * @param value The untranslated string or ID.
	 * @returns A translated string
	 */
	public static function getTranslatedString(value:String):String {
		return value;//Intrinsic.getTranslatedString(value);
	}

}