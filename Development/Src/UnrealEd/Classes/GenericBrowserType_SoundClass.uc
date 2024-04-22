/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// GenericBrowserType_SoundClass: Sound classes
//=============================================================================

class GenericBrowserType_SoundClass
	extends GenericBrowserType_Sounds
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL NotifyPreDeleteObject( UObject* ObjectToDelete );
	virtual void NotifyPostDeleteObject();
}
	
defaultproperties
{
	Description="Sound Classes"
}
