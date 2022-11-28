//=============================================================================
// GenericBrowserType_All: All resource types
//=============================================================================

class GenericBrowserType_All
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor();
	virtual void InvokeCustomCommand( INT InCommand );
	virtual void DoubleClick();
}
	
defaultproperties
{
	Description="-All-"
}
