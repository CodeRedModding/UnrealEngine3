/*=============================================================================
	ParticleModuleTypeDataMeshPhysX.uc: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

class ParticleModuleTypeDataMeshPhysX extends ParticleModuleTypeDataMesh
	native(Particle)
	dependson(ParticleModuleTypeDataPhysX)
	editinlinenew
	collapsecategories
	hidecategories(Object);

/** Actual wrapper for NxFluid PhsyX SDK object */
var(PhysXEmitter) PhysXParticleSystem PhysXParSys<DisplayName="PhysX Par Sys">;

/** 
Methods for simulating the rotation of small differently 
shaped objects using particles. 
*/ 
enum EPhysXMeshRotationMethod
{
	PMRM_Disabled,
	PMRM_Spherical,
	PMRM_Box,
	PMRM_LongBox,
	PMRM_FlatBox,
	PMRM_Velocity 
};

var(PhysXEmitter) EPhysXMeshRotationMethod PhysXRotationMethod<DisplayName="PhysX Mesh Rotation Method">;
var(PhysXEmitter) float FluidRotationCoefficient;
/** Parameters for Vertical LOD: See ParticleModuleTypeDataPhysX.uc */
var(PhysXEmitter) PhysXEmitterVerticalLodProperties VerticalLod;

/** Offset in Z direction for PhysX instanced mesh particles */
var(PhysXEmitter) float ZOffset;

cpptext
{
	virtual FParticleEmitterInstance *CreateInstance(UParticleEmitter *InEmitterParent, UParticleSystemComponent *InComponent);
	virtual void SetToSensibleDefaults(UParticleEmitter* Owner);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void FinishDestroy();

 	virtual UBOOL	SupportsSubUV() const	{ return TRUE; }
	virtual UBOOL	IsAMeshEmitter() const	{ return TRUE; }
}

defaultproperties
{
	PhysXParSys = none
	PhysXRotationMethod=PMRM_Spherical
	FluidRotationCoefficient=5.0f
}
