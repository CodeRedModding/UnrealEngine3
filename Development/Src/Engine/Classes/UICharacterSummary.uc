/**
 * Provides information about the static resources associated with a single character class.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UICharacterSummary extends UIResourceDataProvider
	PerObjectConfig
	Config(Game);

var	config				string				ClassPathName;
var	config	localized	string				CharacterName;
var	config	localized	string				CharacterBio;

var	config				bool				bIsDisabled;

DefaultProperties
{

}
