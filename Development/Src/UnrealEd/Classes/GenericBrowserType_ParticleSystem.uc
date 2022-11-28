//=============================================================================
// GenericBrowserType_ParticleSystem: ParticleSystems
//=============================================================================

class GenericBrowserType_ParticleSystem
	extends GenericBrowserType
	native;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
	
defaultproperties
{
	Description="Particle System"
}
