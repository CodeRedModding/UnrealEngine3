//! @file GenericBrowserType_SubstanceAirImageInput.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief the GenericBrowserType for SubstanceAirImageInput

class GenericBrowserType_SubstanceAirImageInput
	extends GenericBrowserType
	native(Browser);

cpptext
{
	virtual void Init();
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
	virtual UBOOL ShowObjectProperties(UObject* InObject);
}
	
defaultproperties
{
	Description="Substance Image Input"
}
