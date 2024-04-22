/*=============================================================================
	GenericBrowserType_ApexClothingAsset.uc: Apex integration for Clothing Assets.
	Copyright 2008-2009 NVIDIA corporation..
=============================================================================*/

class GenericBrowserType_ApexClothingAsset
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
}

defaultproperties
{
	Description="Apex clothing asset"
}
