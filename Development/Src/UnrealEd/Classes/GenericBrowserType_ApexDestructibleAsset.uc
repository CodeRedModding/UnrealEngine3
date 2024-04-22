/*=============================================================================
	GenericBrowserType_ApexDestructibleAsset.uc: Apex integration for Destructible Assets.
	Copyright 2008-2009 NVIDIA corporation..
=============================================================================*/

class GenericBrowserType_ApexDestructibleAsset
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
	Description="Apex destructible asset"
}
