/**
 * Provides information about the static resources associated with a single weapon class.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class UIWeaponSummary extends UIResourceDataProvider
	PerObjectConfig
	Config(Game);

var	config				string				ClassPathName;

var	config	localized	string				FriendlyName;
var	config	localized	string				WeaponDescription;

var	config				bool				bIsDisabled;


DefaultProperties
{

}
