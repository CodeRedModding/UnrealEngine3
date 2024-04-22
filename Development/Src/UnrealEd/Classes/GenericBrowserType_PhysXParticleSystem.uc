/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class GenericBrowserType_PhysXParticleSystem
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor(UObject *InObject);
}

defaultproperties
{
  	Description = "PhysX Particle Systems"
}
