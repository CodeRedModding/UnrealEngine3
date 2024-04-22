
class ParticleModuleTypeDataApex extends ParticleModuleTypeDataBase
	native(Particle)
	editinlinenew
	collapsecategories
	hidecategories(Object);

// Embedded IOFX asset...
var ApexGenericAsset ApexIOFX;
var ApexGenericAsset ApexEmitter;

cpptext
{
	virtual FParticleEmitterInstance *CreateInstance(UParticleEmitter *InEmitterParent, UParticleSystemComponent *InComponent);

	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void FinishDestroy();
	
	void ConditionalInitialize(void);
}

defaultproperties
{
	Begin Object Class=ApexGenericAsset Name=ApexGenericAsset0
		
	End Object
	
	Begin Object Class=ApexGenericAsset Name=ApexGenericAsset1
		
	End Object
	
	
	ApexIOFX=ApexGenericAsset0
	ApexEmitter=ApexGenericAsset1;
}
