class GenericBrowserType_ApexDestructibleDamageParameters
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor(UObject *InObject);
}

defaultproperties
{
  	Description = "Apex Destructible Damage Parameters"
}
