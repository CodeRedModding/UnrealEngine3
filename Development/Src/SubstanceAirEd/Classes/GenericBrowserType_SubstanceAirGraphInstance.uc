//! @file GenericBrowserType_SubstanceAirGraphInstance.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief the GenericBrowserType for SubstanceAirGraphInstance
//! Offers instantiate and preset loading functions

class GenericBrowserType_SubstanceAirGraphInstance
	extends GenericBrowserType
	native(Browser);

cpptext
{
	virtual void Init();
	virtual UBOOL NotifyPreDeleteObject(UObject* ObjectToDelete);
	virtual void QuerySupportedCommands(class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands) const;
	virtual void InvokeCustomCommand(INT InCommand, TArray<UObject*>& InObjects);
	UBOOL ShowObjectEditor(UObject* InObject);
}
	
defaultproperties
{
	Description="Substance Graph Instance"
}
