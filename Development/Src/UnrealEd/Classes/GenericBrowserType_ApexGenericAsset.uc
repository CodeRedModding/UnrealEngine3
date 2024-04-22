/*=============================================================================
	GenericBrowserType_ApexGenericAsset.uc: Apex integration for Generic Assets.
	Copyright 2008-2009 NVIDIA corporation..
=============================================================================*/

class GenericBrowserType_ApexGenericAsset
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;
	virtual void InvokeCustomCommand( INT InCommand );
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
	virtual void DoubleClick();
	virtual void DoubleClick( UObject* InObject );
	virtual INT QueryDefaultCommand( TArray<UObject*>& InObjects ) const;
	virtual UBOOL ShowObjectEditor( UObject* InObject);

}

defaultproperties
{
	Description="APEX Asset";
}
