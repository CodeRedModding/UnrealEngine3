/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
//-----------------------------------------------------------
// Browser type for prefabs
//-----------------------------------------------------------
class GenericBrowserType_Prefab extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}

DefaultProperties
{
	Description="Prefabs"
}
