/*=============================================================================
	PhysXParticleSetMesh.cpp: PhysX Emitter Source.
	Copyright 2007-2008 AGEIA Technologies.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX && !NX_DISABLE_FLUIDS

#include "UnNovodexSupport.h"
#include "PhysXVerticalEmitter.h"
#include "PhysXParticleSystem.h"
#include "PhysXParticleSetMesh.h"

FPhysXParticleSetMesh::FPhysXParticleSetMesh(FParticleMeshPhysXEmitterInstance& InMeshInstance) :
FPhysXParticleSet(sizeof(PhysXRenderParticleMesh), InMeshInstance.PhysXTypeData.VerticalLod, InMeshInstance.PhysXTypeData.PhysXParSys),
PhysXTypeData(InMeshInstance.PhysXTypeData),
EmitterInstance(InMeshInstance)
{
	check(EmitterInstance.PhysXTypeData.PhysXParSys);
}
FPhysXParticleSetMesh::~FPhysXParticleSetMesh()
{
}

void FPhysXParticleSetMesh::AsyncUpdate(FLOAT DeltaTime, UBOOL bProcessSimulationStep)
{
	FPhysXParticleSystem& PSys = GetPSys();
	FillInVertexBuffer(DeltaTime, PhysXTypeData.PhysXRotationMethod, PhysXTypeData.FluidRotationCoefficient);

	if(bProcessSimulationStep)
	{
		DeathRowManagment();
		AsyncParticleReduction(DeltaTime);
	}
}

void FPhysXParticleSetMesh::RemoveAllParticles()
{
	RemoveAllParticlesInternal();
	check(EmitterInstance.ActiveParticles == EmitterInstance.NumSpawnedParticles);
	EmitterInstance.RemoveParticles();
}

INT FPhysXParticleSetMesh::RemoveParticle(INT RenderParticleIndex, bool bRemoveFromPSys)
{
	if(EmitterInstance.ActiveParticles > 0) //In the other case, the emitter instance has been cleared befor.
	{
		EmitterInstance.RemoveParticleFromActives(RenderParticleIndex);
	}

	return RemoveParticleFast(RenderParticleIndex, bRemoveFromPSys);
}
/**
Here we assume that the particles of the render instance emitter have been updated.
*/
void FPhysXParticleSetMesh::FillInVertexBuffer(FLOAT DeltaTime, BYTE FluidRotationMethod, FLOAT FluidRotationCoefficient)
{
	FPhysXParticleSystem& PSys = GetPSys();
	check(EmitterInstance.ActiveParticles == GetNumRenderParticles() + EmitterInstance.NumSpawnedParticles);
	TmpRenderIndices.Empty();

	if(GetNumRenderParticles() == 0)
		return;
	PhysXParticle* SdkParticles = PSys.ParticlesSdk;
	PhysXParticleEx* SdkParticlesEx = PSys.ParticlesEx;
	
	FLOAT Temp = NxMath::clamp(PhysXTypeData.VerticalLod.RelativeFadeoutTime, 1.0f, 0.0f);
	FLOAT TimeUntilFadeout = 1.0f - Temp;

	const NxReal MaxRotPerStep = 2.5f;
	const NxReal Epsilon = 0.001f;
	
	// Retrieve global up axis.
	NxVec3 UpVector(0.0f, 0.0f, 1.0f);
	PSys.GetGravity(UpVector);
	UpVector = -UpVector;
	UpVector.normalize();

	for (INT i=0; i<GetNumRenderParticles(); i++)
	{
		PhysXRenderParticleMesh& NParticle = *(PhysXRenderParticleMesh*)GetRenderParticle(i);
		PhysXParticle& SdkParticle = SdkParticles[NParticle.ParticleIndex];
		// ensure that particle is in sync
		check(NParticle.Id == SdkParticle.Id);
		NParticle.RelativeTime += NParticle.OneOverMaxLifetime * DeltaTime;
		if(NParticle.RelativeTime >= TimeUntilFadeout && (NParticle.Flags & PhysXRenderParticleMesh::PXRP_DeathQueue) == 0)
			TmpRenderIndices.Push(i);
		DECLARE_PARTICLE(Particle, EmitterInstance.ParticleData + EmitterInstance.ParticleStride*EmitterInstance.ParticleIndices[i]);
		FMeshRotationPayloadData* PayloadData	= (FMeshRotationPayloadData*)((BYTE*)&Particle + EmitterInstance.MeshRotationOffset);
		PayloadData->RotationRate				= PayloadData->RotationRateBase;
		FVector	Pos = N2UPosition(SdkParticle.Pos);
		if( EmitterInstance.MeshRotationActive )
		{
			switch(FluidRotationMethod)
			{
				case PMRM_Velocity:
				{
					FVector NewDirection = N2UVectorCopy(SdkParticle.Vel);
					NewDirection.Normalize();
					FVector OldDirection(1.0f, 0.0f, 0.0f);
					NParticle.Rot = FQuatFindBetween(OldDirection, NewDirection);
					break;
				}

				case PMRM_Spherical:
				{
					NxVec3             vel  = SdkParticle.Vel;
					NxU32              id   = NParticle.Id;
					vel.z = 0; // Project onto xy plane.
					NxReal velmag = vel.magnitude();
					if(velmag > Epsilon)
					{
						NxVec3 avel;
						avel.cross(vel, UpVector);
						NxReal avelm = avel.normalize();
						if(avelm > Epsilon)
						{
							// Set magnitude to magnitude of linear motion (later maybe clamp)
							avel *= -velmag;

							NxReal w = velmag;
							NxReal v = (NxReal)DeltaTime*w*FluidRotationCoefficient;
							NxReal q = NxMath::cos(v);
							NxReal s = NxMath::sin(v)/w;

							NxQuat& Rot = *reinterpret_cast<NxQuat*>(&NParticle.Rot.X);
							Rot.multiply(NxQuat(avel*s,q), Rot);
							Rot.normalize();
						}
					}
				}
				break;

				case PMRM_Box:
				case PMRM_LongBox:
				case PMRM_FlatBox:
				{
					const NxVec3& Vel = *reinterpret_cast<NxVec3*>(&SdkParticle.Vel);
					NxQuat& Rot       = *reinterpret_cast<NxQuat*>(&NParticle.Rot.X);
					NxVec3& AngVel    = *reinterpret_cast<NxVec3*>(&NParticle.AngVel.X);

					const NxVec3 &Contact = PSys.ParticleContactsSdk[NParticle.ParticleIndex];

					NxReal VelMagSqr  = Vel.magnitudeSquared();
					NxReal LimitSqr   = VelMagSqr * FluidRotationCoefficient * FluidRotationCoefficient;

					NxVec3 PoseCorrection(0.0f);		// Rest pose correction.

					// Check bounce...
					if(Contact.magnitudeSquared() > Epsilon)
					{
						NxVec3 UpVector = Contact; // So we can rest on a ramp.
						UpVector.normalize();
						NxVec3 t = Vel - UpVector * UpVector.dot(Vel);
						AngVel = -t.cross(UpVector) * FluidRotationCoefficient;

						NxMat33 rot33;
						rot33.fromQuat(Rot);
						NxVec3  Up(0.0f);

						switch(FluidRotationMethod)
						{
							case PMRM_FlatBox:
							{
								Up = rot33.getColumn(2);
								if(Up.z < 0)
								{
									Up = -Up;
								}
								break;
							}
							default:
							{
								NxReal Best = 0;
								for(int j = (FluidRotationMethod == PMRM_LongBox ? 1 : 0); j < 3; j++)
								{
									NxVec3 tmp = rot33.getColumn(j);
									NxReal d   = tmp.dot(UpVector);
									if(d > Best)
									{
										Up   = tmp;
										Best = d;
									}
									if(-d > Best)
									{
										Up   = -tmp;
										Best = -d;
									}
								}
								break;
							}
						}

						PoseCorrection = Up.cross(UpVector);
						NxReal Mag = PoseCorrection.magnitude();
						NxReal MaxMag = 0.5f / (1.0f + NxMath::sqrt(LimitSqr));
						if(Mag > MaxMag)
						{
							PoseCorrection *= MaxMag / Mag;
						}
					}

					// Limit angular velocity.
					NxReal MagSqr = AngVel.magnitudeSquared();
					if(Contact.magnitudeSquared() > Epsilon && MagSqr > LimitSqr)
					{
						AngVel *= NxMath::sqrt(LimitSqr) / NxMath::sqrt(MagSqr);
					}
					
					// Integrate rotation.
					NxVec3 DeltaRot = AngVel * (NxReal)DeltaTime;
					
					// Apply combined rotation.
					NxVec3 Axis  = DeltaRot + PoseCorrection;
					NxReal Angle = Axis.normalize();
					if(Angle > Epsilon)
					{
						if(Angle > MaxRotPerStep)
						{
							Angle = MaxRotPerStep;
						}
						NxQuat TempRot;
						TempRot.fromAngleAxisFast(Angle, Axis);
						Rot = TempRot * Rot;
					}
				}
				break;
			}
			Particle.Location = Pos;
			Particle.BaseVelocity = Particle.Velocity = N2UPosition(SdkParticle.Vel);

			// Calculate OldLocation based on Velocity and Location
			// (The shader uses the reverse calculation to determine direction of velocity).
			Particle.OldLocation = Particle.Location - Particle.Velocity * DeltaTime;

			FVector Euler	= NParticle.Rot.Euler();
			PayloadData->Rotation.X	= Euler.X;
			PayloadData->Rotation.Y	= Euler.Y;
			PayloadData->Rotation.Z	= Euler.Z;
		}
		else
		{
			Particle.Location = Pos;
			PayloadData->Rotation.X	= 0;
			PayloadData->Rotation.Y	= 0;
			PayloadData->Rotation.Z	= 0;
		}
	}
}
#endif	//#if WITH_NOVODEX && !NX_DISABLE_FLUIDS
