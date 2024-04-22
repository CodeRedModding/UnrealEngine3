//! @file GenericBrowserType_SubstanceAirInstanceFactory.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief the GenericBrowserType for SubstanceAirInstanceFactory
//! Offers instantiate and preset loading functions

class GenericBrowserType_SubstanceAirInstanceFactory
	extends GenericBrowserType
	native(Browser);

cpptext
{
	virtual void Init();
	virtual UBOOL NotifyPreDeleteObject(UObject* ObjectToDelete);
	virtual void QuerySupportedCommands(class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands) const;
	virtual void InvokeCustomCommand(INT InCommand, TArray<UObject*>& InObjects);
	
	virtual INT QueryDefaultCommand( TArray<UObject*>& InObjects ) const;
	virtual UBOOL ShowObjectProperties( UObject* InObject );
}
	
defaultproperties
{
	Description="Substance Package"
}

