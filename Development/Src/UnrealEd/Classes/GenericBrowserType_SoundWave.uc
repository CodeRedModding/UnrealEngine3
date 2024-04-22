/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// GenericBrowserType_SoundWave: SoundWaves
//=============================================================================

class GenericBrowserType_SoundWave
	extends GenericBrowserType_Sounds
	native;

cpptext
{
	virtual void Init( void );
		
	/**
	 * Invokes a custom menu item command for every selected object
	 * of a supported class.
	 *
	 * @param InCommand		The command to execute
	 * @param InObjects		The objects to invoke the command against
	 */
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
}
	
defaultproperties
{
	Description="Sound Wave Data"
}
