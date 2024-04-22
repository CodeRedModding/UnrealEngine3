//! @file GenericBrowserType_SubstanceAirGraphInstance.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief the GenericBrowserType for SubstanceAirTexture2D
//! Used to deactivate the corresponding Substance output

class GenericBrowserType_SubstanceAirTexture2D
	extends GenericBrowserType_Texture
	native(Browser);

cpptext
{
	virtual void Init();
	virtual void QuerySupportedCommands( class USelection* InObjects, TArray< FObjectSupportedCommandType >& OutCommands ) const;
	virtual void InvokeCustomCommand( INT InCommand, TArray<UObject*>& InObjects );
	virtual UBOOL NotifyPreDeleteObject(UObject* ObjectToDelete);
}
	
defaultproperties
{
	Description="Substance Texture 2D"
}
