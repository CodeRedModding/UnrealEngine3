/*=============================================================================
	UnParticleComponent.cpp: Particle component implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "UnLinker.h"

IMPLEMENT_CLASS(AEmitter);

IMPLEMENT_CLASS(UParticleSystem);
IMPLEMENT_CLASS(UParticleEmitter);
IMPLEMENT_CLASS(UParticleSpriteEmitter);
IMPLEMENT_CLASS(UParticleSystemComponent);
IMPLEMENT_CLASS(UParticleMeshEmitter);
IMPLEMENT_CLASS(UParticleSpriteSubUVEmitter);

/*-----------------------------------------------------------------------------
	Conversion functions
-----------------------------------------------------------------------------*/
void Particle_ModifyFloatDistribution(UDistributionFloat* pkDistribution, FLOAT fScale)
{
	if (pkDistribution->IsA(UDistributionFloatConstant::StaticClass()))
	{
		UDistributionFloatConstant* pkDistConstant = Cast<UDistributionFloatConstant>(pkDistribution);
		pkDistConstant->Constant *= fScale;
	}
	else
	if (pkDistribution->IsA(UDistributionFloatUniform::StaticClass()))
	{
		UDistributionFloatUniform* pkDistUniform = Cast<UDistributionFloatUniform>(pkDistribution);
		pkDistUniform->Min *= fScale;
		pkDistUniform->Max *= fScale;
	}
	else
	if (pkDistribution->IsA(UDistributionFloatConstantCurve::StaticClass()))
	{
		UDistributionFloatConstantCurve* pkDistCurve = Cast<UDistributionFloatConstantCurve>(pkDistribution);

		INT iKeys = pkDistCurve->GetNumKeys();
		INT iCurves = pkDistCurve->GetNumSubCurves();

		for (INT KeyIndex = 0; KeyIndex < iKeys; KeyIndex++)
		{
			FLOAT fKeyIn = pkDistCurve->GetKeyIn(KeyIndex);
			for (INT SubIndex = 0; SubIndex < iCurves; SubIndex++)
			{
				FLOAT fKeyOut = pkDistCurve->GetKeyOut(SubIndex, KeyIndex);
				FLOAT ArriveTangent;
				FLOAT LeaveTangent;
                pkDistCurve->GetTangents(SubIndex, KeyIndex, ArriveTangent, LeaveTangent);

				pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * fScale);
				pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * fScale, LeaveTangent * fScale);
			}
		}
	}
}

void Particle_ModifyVectorDistribution(UDistributionVector* pkDistribution, FVector& vScale)
{
	if (pkDistribution->IsA(UDistributionVectorConstant::StaticClass()))
	{
		UDistributionVectorConstant* pkDistConstant = Cast<UDistributionVectorConstant>(pkDistribution);
		pkDistConstant->Constant *= vScale;
	}
	else
	if (pkDistribution->IsA(UDistributionVectorUniform::StaticClass()))
	{
		UDistributionVectorUniform* pkDistUniform = Cast<UDistributionVectorUniform>(pkDistribution);
		pkDistUniform->Min *= vScale;
		pkDistUniform->Max *= vScale;
	}
	else
	if (pkDistribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
	{
		UDistributionVectorConstantCurve* pkDistCurve = Cast<UDistributionVectorConstantCurve>(pkDistribution);

		INT iKeys = pkDistCurve->GetNumKeys();
		INT iCurves = pkDistCurve->GetNumSubCurves();

		for (INT KeyIndex = 0; KeyIndex < iKeys; KeyIndex++)
		{
			FLOAT fKeyIn = pkDistCurve->GetKeyIn(KeyIndex);
			for (INT SubIndex = 0; SubIndex < iCurves; SubIndex++)
			{
				FLOAT fKeyOut = pkDistCurve->GetKeyOut(SubIndex, KeyIndex);
				FLOAT ArriveTangent;
				FLOAT LeaveTangent;
                pkDistCurve->GetTangents(SubIndex, KeyIndex, ArriveTangent, LeaveTangent);

				switch (SubIndex)
				{
				case 1:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.Y);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.Y, LeaveTangent * vScale.Y);
					break;
				case 2:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.Z);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.Z, LeaveTangent * vScale.Z);
					break;
				case 0:
				default:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.X);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.X, LeaveTangent * vScale.X);
					break;
				}
			}
		}
	}
}

void Particle_ConvertEmitterModules(UParticleEmitter* pkEmitter)
{
	if (pkEmitter->ConvertedModules)
		return;

    // Convert all rotations to "rotations per second" not "degrees per second"
	UParticleModule* Module;
	for (INT i = 0; i < pkEmitter->Modules.Num(); i++)
	{
		Module = pkEmitter->Modules(i);

		if (!Module->IsA(UParticleModuleRotationBase::StaticClass()) &&
			!Module->IsA(UParticleModuleRotationRateBase::StaticClass()))
		{
			continue;
		}

		// Depending on the type, we will need to grab different distributions...
		FVector vScale = FVector(1.0f/360.0f, 1.0f/360.0f, 1.0f/360.0f);

		if (Module->IsA(UParticleModuleRotation::StaticClass()))
		{
			UParticleModuleRotation* pkPMRotation = CastChecked<UParticleModuleRotation>(Module);
			Particle_ModifyFloatDistribution(pkPMRotation->StartRotation, 1.0f/360.0f);
		}
		else
		if (Module->IsA(UParticleModuleMeshRotation::StaticClass()))
		{
			UParticleModuleMeshRotation* pkPMMRotation = CastChecked<UParticleModuleMeshRotation>(Module);
			Particle_ModifyVectorDistribution(pkPMMRotation->StartRotation, vScale);
		}
		else
		if (Module->IsA(UParticleModuleRotationRate::StaticClass()))
		{
			UParticleModuleRotationRate* pkPMRotationRate = CastChecked<UParticleModuleRotationRate>(Module);
			Particle_ModifyFloatDistribution(pkPMRotationRate->StartRotationRate, 1.0f/360.0f);
		}
		else
		if (Module->IsA(UParticleModuleMeshRotationRate::StaticClass()))
		{
			UParticleModuleMeshRotationRate* pkPMMRotationRate = CastChecked<UParticleModuleMeshRotationRate>(Module);
			Particle_ModifyVectorDistribution(pkPMMRotationRate->StartRotationRate, vScale);
		}
		else
		if (Module->IsA(UParticleModuleRotationOverLifetime::StaticClass()))
		{
			UParticleModuleRotationOverLifetime* pkPMRotOverLife = CastChecked<UParticleModuleRotationOverLifetime>(Module);
			Particle_ModifyFloatDistribution(pkPMRotOverLife->RotationOverLife, 1.0f/360.0f);
		}
	}

	pkEmitter->ConvertedModules = true;
}

/*-----------------------------------------------------------------------------
	FParticlesStatGroup
-----------------------------------------------------------------------------*/
struct FParticlesStatGroup: FStatGroup
{
	FStatCounter	SpriteParticles;
	FCycleCounter	SortingTime;

	FParticlesStatGroup():
		FStatGroup(TEXT("Particles")),
		SpriteParticles(this,TEXT("Sprite particles")),
		SortingTime(this,TEXT("Sorting ms"))
	{}
};
FParticlesStatGroup GParticlesStats;

/*-----------------------------------------------------------------------------
	AEmitter implementation.
-----------------------------------------------------------------------------*/

void AEmitter::SetTemplate(UParticleSystem* NewTemplate, UBOOL bDestroyOnFinish)
{
	ParticleSystemComponent->SetTemplate(NewTemplate);

	bDestroyOnSystemFinish = bDestroyOnFinish;
}

void AEmitter::execSetTemplate(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UParticleSystem,NewTemplate);
	P_GET_UBOOL_OPTX(bDestroyOnFinish, false);
	P_FINISH;

	SetTemplate(NewTemplate, bDestroyOnFinish);
}

/*-----------------------------------------------------------------------------
	FParticleEmitterInstance implementation.
-----------------------------------------------------------------------------*/

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	appFree(ParticleData);
	appFree(ParticleIndices);
}

void FParticleEmitterInstance::Init()
{
	// This assert makes sure that packing is as expected.
	check(sizeof(FBaseParticle) == 96);

	// Calculate particle struct size, size and average lifetime.
	ParticleSize			= sizeof(FBaseParticle);
	for(INT i=0; i<Template->SpawnModules.Num(); i++)
	{
		if (Template->SpawnModules(i)->RequiredBytes())
			ModuleOffsetMap.Set(Template->SpawnModules(i),ParticleSize);
		
		ParticleSize		+= Template->SpawnModules(i)->RequiredBytes();
	}

	// Offset into emitter specific payload (e.g. TrailComponent requires extra bytes).
	PayloadOffset			= ParticleSize;
	
	// Update size with emitter specific size requirements.
	ParticleSize			+= RequiredBytes();

	// Make sure everything is at least 16 byte aligned so we can use SSE for FVector.
	ParticleSize = Align(ParticleSize, 16);

	// E.g. trail emitters store trailing particles directly after leading one.
	ParticleStride			= CalculateParticleStride(ParticleSize);

	// Set initial values.
	SpawnFraction			= 0;
	SecondsSinceCreation	= 0;
	
	Location				= Component->LocalToWorld.GetOrigin();
	OldLocation				= Location;
	
	TrianglesToRender		= 0;
	MaxVertexIndex			= 0;

	ParticleData			= NULL;
	ParticleIndices			= NULL;
	MaxActiveParticles		= 0;
	ActiveParticles			= 0;

	ParticleBoundingBox.Init();

	// Resize to sensible default.
	if (Template->PeakActiveParticles)
	{
		// In-game... we assume the editor has set this properly
		Resize(Template->PeakActiveParticles);
	}
	else
	{
		// This is to force the editor to 'select' a value
		Resize(10);
	}
}

void FParticleEmitterInstance::Resize(UINT NewMaxActiveParticles)
{
	// Allocations > 16 byte are always 16 byte aligned so ParticleData can be used with SSE.
	ParticleData			= (BYTE*) appRealloc(ParticleData, ParticleStride * NewMaxActiveParticles);
	// Allocate memory for indices.
	ParticleIndices			= (_WORD*) appRealloc(ParticleIndices, sizeof(_WORD) * NewMaxActiveParticles);

	// Fill in default 1:1 mapping.
	for(UINT i=MaxActiveParticles; i<NewMaxActiveParticles; i++)
	{
		ParticleIndices[i]	= i;
	}

	MaxActiveParticles		= NewMaxActiveParticles;

	if ((INT)MaxActiveParticles > Template->PeakActiveParticles)
	{
		Template->PeakActiveParticles = MaxActiveParticles;
	}
}

UINT FParticleEmitterInstance::RequiredBytes()
{
	return 0;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, FLOAT InterpolationPercentage, FLOAT SpawnTime)
{
	// Interpolate position if using world space.
	if (!Template->UseLocalSpace)
	{
		if (FDistSquared(OldLocation, Location) > 1.f)
			Particle->Location += InterpolationPercentage * (OldLocation - Location);	
	}

	Particle->OldLocation = Particle->Location;
	Particle->Location   += SpawnTime * Particle->Velocity;
}

FLOAT FParticleEmitterInstance::Spawn(FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime)
{
	// Ensure continous spawning... lots of fiddling.
	FLOAT	NewLeftover = OldLeftover + DeltaTime * Rate;
	UINT	Number		= appFloor(NewLeftover);
	FLOAT	Increment	= 1.f / Rate;
	FLOAT	StartTime	= DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover			= NewLeftover - Number;
	
	// Handle growing arrays.
	if (ActiveParticles + Number >= MaxActiveParticles)
	{
		Resize((ActiveParticles + Number) * 1.5);
	}

	// Spawn particles.
	for(UINT i=0; i<Number; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * ParticleIndices[ActiveParticles]);
		FLOAT SpawnTime = StartTime - i * Increment;
	
		PreSpawn(Particle);
		for(INT n=0; n<Template->SpawnModules.Num(); n++)
		{
			UINT* Offset = ModuleOffsetMap.Find(Template->SpawnModules(n));
			Template->SpawnModules(n)->Spawn(this, Offset ? *Offset : 0, SpawnTime);
		}
		PostSpawn(Particle, 1.f - FLOAT(i+1) / FLOAT(Number), SpawnTime);

		ActiveParticles++;
	}

	return NewLeftover;
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle)
{
	appMemzero(Particle, ParticleSize);
	if (!Template->UseLocalSpace)
	{
		Particle->Location = Location;
	}
}

void FParticleEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	// Keep track of location for world- space interpolation and other effects.
	OldLocation	= Location;
	Location	= Component->LocalToWorld.GetOrigin();

	SecondsSinceCreation += DeltaTime;

	// Update time within emitter loop.
	EmitterTime = SecondsSinceCreation;
	if (Template->EmitterDuration > KINDA_SMALL_NUMBER)
	{
		EmitterTime = appFmod(SecondsSinceCreation, Template->EmitterDuration);
	}

	// If not suppressing spawning...
	if (!bSuppressSpawning)
	{
		// If emitter is not done - spawn at current rate.
		// If EmitterLoops is 0, then we loop forever, so always spawn.
		if (Template->EmitterLoops == 0 || SecondsSinceCreation < (Template->EmitterDuration * Template->EmitterLoops))
		{
			// Figure out spawn rate for this tick.
			FLOAT SpawnRate = Template->SpawnRate->GetValue(EmitterTime);

			// Spawn new particles...
			if (SpawnRate > 0.f)
				SpawnFraction = Spawn(SpawnFraction, SpawnRate, DeltaTime);
		}
	}

	// Reset velocity and size.
	for(UINT i=0; i<ActiveParticles; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
		Particle.Velocity		= Particle.BaseVelocity;
		Particle.Size			= Particle.BaseSize;
		Particle.RotationRate	= Particle.BaseRotationRate;
        Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
	}

	//@todo. Do we kill off the particles here??
	KillParticles();

	// Update existing particles (might respawn dying ones).
	for(INT i=0; i<Template->UpdateModules.Num(); i++)
	{
		if (!Template->UpdateModules(i))
			continue;

		UINT* Offset = ModuleOffsetMap.Find(Template->UpdateModules(i));
		Template->UpdateModules(i)->Update(this, Offset ? *Offset : 0, DeltaTime);
	}

	if (Template->TypeDataModule)
	{
		//@todo. Need to track TypeData offset into payload!
		Template->TypeDataModule->Update(this, 0, DeltaTime);
	}

	// Calculate bounding box and simulate velocity.
	UpdateBoundingBox(DeltaTime);
}

void FParticleEmitterInstance::Rewind()
{
	SecondsSinceCreation = 0;
	EmitterTime = 0;
}

UINT FParticleEmitterInstance::CalculateParticleStride(UINT ParticleSize)
{
	return ParticleSize;
}

// Return true if emitter has finished all its loops, and all particles are now dead.
UBOOL FParticleEmitterInstance::HasCompleted()
{
	if (Template->EmitterLoops == 0 || SecondsSinceCreation < (Template->EmitterDuration * Template->EmitterLoops))
		return false;

	if (ActiveParticles > 0)
		return false;

	return true;
}

void FParticleEmitterInstance::KillParticles()
{
	// Loop over the active particles... If their RelativeTime is > 1.0f (indicating they are dead),
	// move them to the 'end' of the active particle list.
	for(INT i=ActiveParticles-1; i>=0; i--)
	{
		const INT	CurrentIndex	= ParticleIndices[i];
		const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
		FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
		if (Particle.RelativeTime > 1.0f)
		{
			ParticleIndices[i]	= ParticleIndices[ActiveParticles-1];
			ParticleIndices[ActiveParticles-1]	= CurrentIndex;
			ActiveParticles--;
		}
	}
}

void FParticleEmitterInstance::RenderDebug(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI, UBOOL bCrosses)
{
	FMatrix LocalToWorld = Template->UseLocalSpace ? Component->LocalToWorld : FMatrix::Identity;

	FMatrix CameraToWorld = Context.View->ViewMatrix.Inverse();
	FVector CamX = CameraToWorld.TransformNormal(FVector(1,0,0));
	FVector CamY = CameraToWorld.TransformNormal(FVector(0,1,0));

	for(UINT i=0; i<ActiveParticles; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		FVector DrawLocation = LocalToWorld.TransformFVector(Particle.Location);
		if (bCrosses)
		{
			PRI->DrawLine(DrawLocation - (0.5f * Particle.Size.X * CamX), DrawLocation + (0.5f * Particle.Size.X * CamX), Template->EmitterEditorColor);
			PRI->DrawLine(DrawLocation - (0.5f * Particle.Size.Y * CamY), DrawLocation + (0.5f * Particle.Size.Y * CamY), Template->EmitterEditorColor);
		}
		else
		{
			PRI->DrawPoint(DrawLocation, Template->EmitterEditorColor, 2);
		}
	}
}

void FParticleEmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	FLOAT MaxSizeScale = 0.f;
	ParticleBoundingBox.Init();
	for(UINT i=0; i<ActiveParticles; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
		
		// Do linear integrator and update bounding box
		Particle.OldLocation = Particle.Location;
		Particle.Location	+= DeltaTime * Particle.Velocity;
		ParticleBoundingBox += Particle.Location;
		// Do angular integrator, and wrap result to within +/- 2 PI
		Particle.Rotation	+= DeltaTime * Particle.RotationRate;
		Particle.Rotation	 = appFmod(Particle.Rotation, 2.f*(FLOAT)PI);
		MaxSizeScale		 = Max(MaxSizeScale, Particle.Size.GetAbsMax()); //@todo particles: this does a whole lot of compares that can be avoided using SSE/ Altivec.
	}
	ParticleBoundingBox = ParticleBoundingBox.ExpandBy(MaxSizeScale);

	// Transform bounding box into world space if the emitter uses a local space coordinate system.
	if (Template->UseLocalSpace) 
		ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
}

/*-----------------------------------------------------------------------------
	FSpriteEmitterInstance implementation.
-----------------------------------------------------------------------------*/

FParticleSpriteEmitterInstance::FParticleSpriteEmitterInstance(UParticleSpriteEmitter* InTemplate, UParticleSystemComponent* InComponent)
:	FParticleEmitterInstance(InTemplate, InComponent),
	SpriteTemplate(InTemplate),
	VertexBuffer(this),
	IndexBuffer(),
	VertexFactory()
{}

void FParticleSpriteEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	VertexFactory.PositionComponent		= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteVertex, Position		), VCT_Float3		);
	VertexFactory.OldPositionComponent	= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteVertex, OldPosition	), VCT_Float3		);
	VertexFactory.SizeComponent			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteVertex, Size			), VCT_Float3		);
	VertexFactory.UVComponent			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteVertex, U				), VCT_Float2		);
	VertexFactory.RotationComponent		= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteVertex, Rotation		), VCT_Float1		);
	VertexFactory.ColorComponent		= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteVertex, Color			), VCT_UByte4		);
	
	VertexFactory.LocalToWorld			= Template->UseLocalSpace ? Component->LocalToWorld : FMatrix::Identity;
	VertexFactory.ScreenAlignment		= SpriteTemplate->ScreenAlignment;

	GResourceManager->UpdateResource(&VertexBuffer);
	GResourceManager->UpdateResource(&IndexBuffer  );
	GResourceManager->UpdateResource(&VertexFactory);
}

static FSceneView* ParticleView = NULL;

void FParticleSpriteEmitterInstance::Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	//@todo particles: move to PostEditChange
	VertexFactory.LocalToWorld		= Template->UseLocalSpace ? Component->LocalToWorld : FMatrix::Identity;
	VertexFactory.ScreenAlignment	= SpriteTemplate->ScreenAlignment;

	VertexBuffer.SetSize(ActiveParticles);
	IndexBuffer.SetSize(ActiveParticles);

	ParticleView = Context.View;
	GResourceManager->UpdateResource(&VertexBuffer);
	GResourceManager->UpdateResource(&IndexBuffer  );
	GResourceManager->UpdateResource(&VertexFactory);

	TrianglesToRender	= ActiveParticles * 2;
	MaxVertexIndex		= TrianglesToRender * 2 - 1;

	if (0 == TrianglesToRender)
		return;

	GParticlesStats.SpriteParticles.Value += ActiveParticles;

	if (Template->EmitterRenderMode == ERM_Normal)
	{
		if (Context.View->ViewMode & SVM_WireframeMask)
		{
			PRI->DrawWireframe(
				&VertexFactory,
				&IndexBuffer,
				WT_TriList,
				FColor(255,0,0),
				0,
				TrianglesToRender,
				0,
				MaxVertexIndex
				);
		}
		else
		{
			PRI->DrawMesh(
				&VertexFactory,
				&IndexBuffer,
				Component->SelectedMaterial(Context,SpriteTemplate->Material ? SpriteTemplate->Material : GEngine->DefaultMaterial),
				(SpriteTemplate->Material ? SpriteTemplate->Material : GEngine->DefaultMaterial)->GetInstanceInterface(),
				0,
				TrianglesToRender,
				0,
				MaxVertexIndex
				);
		}
	}
	else
	if (Template->EmitterRenderMode == ERM_Point)
	{
		RenderDebug(Scene, Context, PRI, false);
	}
	else
	if (Template->EmitterRenderMode == ERM_Cross)
	{
		RenderDebug(Scene, Context, PRI, true);
	}
}


/*-----------------------------------------------------------------------------
	FMeshEmitterInstance implementation.
-----------------------------------------------------------------------------*/

FParticleMeshEmitterInstance::FParticleMeshEmitterInstance(
	UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) :
	FParticleEmitterInstance(InTemplate, InComponent),
	LastScene(NULL)
{
	EmitterTemplate = CastChecked<UParticleSpriteEmitter>(InTemplate);
	check(EmitterTemplate);
	MeshTypeData = CastChecked<UParticleModuleTypeDataMesh>(EmitterTemplate->TypeDataModule);
	check(MeshTypeData);

    // Grab the MeshRotationRate module offset, if there is one...
    MeshRotationActive = false;
    for (INT i = 0; i < InTemplate->Modules.Num(); i++)
    {
        if ((InTemplate->Modules(i)->IsA(UParticleModuleMeshRotationRate::StaticClass())) ||
            (InTemplate->Modules(i)->IsA(UParticleModuleMeshRotation::StaticClass())))
        {
            MeshRotationActive = true;
            break;
        }
    }

	MeshInstances.Empty();
}

FParticleMeshEmitterInstance::~FParticleMeshEmitterInstance()
{
	for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
	{
		UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
		if (pkComponent)
		{
			if (pkComponent->Initialized)
				pkComponent->Destroyed();
			delete pkComponent;
			MeshInstances(InstanceIndex) = 0;
		}
	}
}

void FParticleMeshEmitterInstance::Serialize(FArchive& Ar)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
		{
			UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
			if (pkComponent)
			{
				Ar << pkComponent;
			}
		}
	}
}


void FParticleMeshEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	if (MeshTypeData->Mesh)
	{
		for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
		{
			UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
			if (!pkComponent)
			{
				pkComponent = ConstructObject<UStaticMeshComponent>(
					UStaticMeshComponent::StaticClass());
				pkComponent->StaticMesh = MeshTypeData->Mesh;
				pkComponent->SetParentToWorld(FScaleMatrix(FVector(1,1,1)));
				pkComponent->CastShadow = MeshTypeData->CastShadows;
				pkComponent->Scene = 0;
				pkComponent->Owner = 0;
				pkComponent->Initialized = 0;

				MeshInstances(InstanceIndex) = pkComponent;
			}
		}
	}
}

void FParticleMeshEmitterInstance::Resize(UINT NewMaxActiveParticles)
{
	UINT OldMaxActiveParticles = MaxActiveParticles;
	FParticleEmitterInstance::Resize(NewMaxActiveParticles);
	MeshInstances.AddZeroed(NewMaxActiveParticles - OldMaxActiveParticles);
    if (MeshRotationActive)
    {
        for (UINT i = 0; i < NewMaxActiveParticles; i++)
	    {
		    DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
            FVector* pkVector = (FVector*)((BYTE*)&Particle + PayloadOffset);
            // FVector  MeshRotation
            // FVector  MeshRotationRate
            // FVector  MeshRotationRateBase
            if (i >= OldMaxActiveParticles)
                pkVector[2] = FVector(0.0f);
	    }
    }
}

void FParticleMeshEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
    if (MeshRotationActive)
    {
        for (UINT i = 0; i < ActiveParticles; i++)
	    {
		    DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
            FVector* pkVector = (FVector*)((BYTE*)&Particle + PayloadOffset);
            // FVector  MeshRotation
            // FVector  MeshRotationRate
            pkVector[1] = pkVector[2];
            // FVector  MeshRotationRateBase
	    }
    }

	if (MeshTypeData->Mesh)
	{
		for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
		{
			UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
			if (pkComponent)
			{
				pkComponent->HiddenGame = false;
				pkComponent->HiddenEditor = false;
			}
		}
	}

	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);

    if (MeshRotationActive)
    {
        for (UINT i = 0; i < ActiveParticles; i++)
	    {
		    DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
            FVector* pkVectorData = (FVector*)((BYTE*)&Particle + PayloadOffset);
            pkVectorData[0] += DeltaTime * pkVectorData[1];
        }
    }

	// Do we need to tick the mesh instances or will the engine do it?
	if ((ActiveParticles == 0) & bSuppressSpawning)
	{
		RemovedFromScene();
	}

	if (MeshTypeData->Mesh)
	{
		for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
		{
			UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
			if (pkComponent)
			{
				pkComponent->HiddenGame = true;
				pkComponent->HiddenEditor = true;
			}
		}
	}
}

void FParticleMeshEmitterInstance::Render(FScene* Scene, 
	const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if (ActiveParticles == 0)
		return;

	if (Template->EmitterRenderMode == ERM_Normal)
	{
		bool bForceReinit = false;
		if (LastScene != Scene)
		{
			bForceReinit = true;
			LastScene = Scene;
		}

		for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
		{
			UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
			if (pkComponent && (!pkComponent->Initialized || bForceReinit))
			{
				if (pkComponent->Initialized)
					pkComponent->Destroyed();

				pkComponent->SetParentToWorld(FScaleMatrix(FVector(1,1,1)));
				
				pkComponent->Scene = Scene;
				if (pkComponent->IsValidComponent())
				{
					pkComponent->Created();
				}
			}
		}

		FMatrix kMat(FMatrix::Identity);
		// Reset velocity and size.
		for (INT i=ActiveParticles-1; i>=0; i--)
		{
			UStaticMeshComponent* pkComponent = MeshInstances(i);
			if (pkComponent)
			{
				pkComponent->HiddenGame = false;
				pkComponent->HiddenEditor = false;

				const INT	CurrentIndex	= ParticleIndices[i];
				const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
				FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);

				if (Particle.RelativeTime < 1.0f)
				{
					FTranslationMatrix kTransMat(Particle.Location);
					FScaleMatrix kScaleMat(FVector(Particle.Size.X, Particle.Size.Y, Particle.Size.Z));
					FRotator kRotator;

                    if (MeshRotationActive)
                    {
                        FVector* pkVectorData = (FVector*)((BYTE*)&Particle + PayloadOffset);
                        // FVector  MeshRotation
                        // FVector  MeshRotationRate
                        // FVector  MeshRotationRateBase
						kRotator = FRotator::MakeFromEuler(pkVectorData[0]);
					}
					else
					{
						FLOAT fRot = Particle.Rotation * 180.0f / PI;
						FVector kRotVec = FVector(fRot, fRot, fRot);
						kRotator = FRotator::MakeFromEuler(kRotVec);
					}
					
					FRotationMatrix kRotMat(kRotator);

					kMat = kRotMat * kScaleMat * kTransMat;

					pkComponent->SetParentToWorld(kMat);
					pkComponent->Update();
					pkComponent->Render(Context, PRI);
				}
				else
				{
					// Remove it from the scene???
//					if (pkComponent && pkComponent->Initialized && pkComponent->Scene)
//					{
//						pkComponent->HiddenGame = true;
//					}
				}

				pkComponent->HiddenGame = true;
				pkComponent->HiddenEditor = true;
			}
		}
	}
	else 
	if (Template->EmitterRenderMode == ERM_Point)
	{
		RenderDebug(Scene, Context, PRI, false);
	}
	else
//	if (Template->EmitterRenderMode == ERM_Cross)
	{
		RenderDebug(Scene, Context, PRI, true);
	}
}

UINT FParticleMeshEmitterInstance::CalculateParticleStride(UINT ParticleSize)
{
	return ParticleSize;
}

void FParticleMeshEmitterInstance::ChildSpawn(FBaseParticle* MainParticle, 
	FBaseParticle* ChildParticle, FLOAT InterpolationPercentage)
{
}

void FParticleMeshEmitterInstance::UpdateBoundingBox(FLOAT DeltaTime)
{
	//@todo. Implement proper bound determination for mesh emitters.
	// Currently, just 'forcing' the mesh size to be taken into account.

	FBoxSphereBounds MeshBound = MeshTypeData->Mesh->Bounds;

	FLOAT MaxSizeScale = 0.f;
	ParticleBoundingBox.Init();
	for(UINT i=0; i<ActiveParticles; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);
		
		// Do linear integrator and update bounding box
		Particle.OldLocation = Particle.Location;
		Particle.Location	+= DeltaTime * Particle.Velocity;
		ParticleBoundingBox += (Particle.Location + MeshBound.SphereRadius * Particle.Size);
		ParticleBoundingBox += (Particle.Location - MeshBound.SphereRadius * Particle.Size);

		// Do angular integrator, and wrap result to within +/- 2 PI
		Particle.Rotation	+= DeltaTime * Particle.RotationRate;
		Particle.Rotation	 = appFmod(Particle.Rotation, 2.f*(FLOAT)PI);
		MaxSizeScale		 = Max(MaxSizeScale, Particle.Size.GetAbsMax()); //@todo particles: this does a whole lot of compares that can be avoided using SSE/ Altivec.
	}
	ParticleBoundingBox = ParticleBoundingBox.ExpandBy(MaxSizeScale);

	// Transform bounding box into world space if the emitter uses a local space coordinate system.
	if (Template->UseLocalSpace) 
		ParticleBoundingBox = ParticleBoundingBox.TransformBy(Component->LocalToWorld);
}

UINT FParticleMeshEmitterInstance::RequiredBytes()
{
    // FVector  MeshRotation
    // FVector  MeshRotationRate
    // FVector  MeshRotationRateBase
	return (sizeof(FVector) * 3);
}

void FParticleMeshEmitterInstance::PostSpawn(FBaseParticle* Particle, 
	FLOAT InterpolationPercentage, FLOAT SpawnTime)
{
	FParticleEmitterInstance::PostSpawn(Particle, InterpolationPercentage, SpawnTime);

	// Initialize first child particle.
//	ChildSpawn(Particle, (FBaseParticle*) ((BYTE*) Particle + ParticleSize), InterpolationPercentage);
}

void FParticleMeshEmitterInstance::KillParticles()
{
	// Loop through the active particles. If the particle is 'dead', slide it to the end
	// of the active particle list. 
	//@todo. 'Kill' the mesh as well here??
	for(INT i=ActiveParticles-1; i>=0; i--)
	{
		const INT	CurrentIndex	= ParticleIndices[i];
		const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
		FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
		if (Particle.RelativeTime > 1.0f)
		{
			ParticleIndices[i]	= ParticleIndices[ActiveParticles-1];
			ParticleIndices[ActiveParticles-1]	= CurrentIndex;
			ActiveParticles--;

			//
			UStaticMeshComponent* pkComponent = MeshInstances(i);
			if (pkComponent)
			{
//				pkComponent->HiddenGame = true;
//				pkComponent->Destroyed();
			}
		}
	}
}

void FParticleMeshEmitterInstance::RemovedFromScene()
{
	for (UINT InstanceIndex = 0; InstanceIndex < (UINT)MeshInstances.Num(); InstanceIndex++)
	{
		UStaticMeshComponent* pkComponent = MeshInstances(InstanceIndex);
		if (pkComponent && pkComponent->Initialized && pkComponent->Scene)
		{
			pkComponent->Destroyed();
		}
	}
}

/*-----------------------------------------------------------------------------
	FParticleSpriteSubUVEmitterInstance implementation.
-----------------------------------------------------------------------------*/
FParticleSpriteSubUVEmitterInstance::FParticleSpriteSubUVEmitterInstance(
	UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) :
	FParticleEmitterInstance(InTemplate, InComponent),
	VertexBuffer(this),
	IndexBuffer()
{
	SpriteTemplate = CastChecked<UParticleSpriteEmitter>(InTemplate);
	check(SpriteTemplate);
	SubUVTypeData = CastChecked<UParticleModuleTypeDataSubUV>(SpriteTemplate->TypeDataModule);
	check(SubUVTypeData);
}

void FParticleSpriteSubUVEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	VertexFactory.PositionComponent		= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, Position		), VCT_Float3);
	VertexFactory.OldPositionComponent	= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, OldPosition	), VCT_Float3);
	VertexFactory.SizeComponent			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, Size			), VCT_Float3);
	VertexFactory.UVComponent			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, U				), VCT_Float2);
	VertexFactory.RotationComponent		= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, Rotation		), VCT_Float1);
	VertexFactory.ColorComponent		= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, Color			), VCT_UByte4);
	VertexFactory.UVComponent2			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, U2			), VCT_Float2);
	VertexFactory.Interpolation			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, Interp		), VCT_Float2);
	VertexFactory.SizeScaler			= FVertexStreamComponent(&VertexBuffer, STRUCT_OFFSET(FParticleSpriteSubUVVertex, SizeU			), VCT_Float2);
	
	VertexFactory.LocalToWorld			= Template->UseLocalSpace ? Component->LocalToWorld : FMatrix::Identity;
	VertexFactory.ScreenAlignment		= SpriteTemplate->ScreenAlignment;

	GResourceManager->UpdateResource(&VertexBuffer);
	GResourceManager->UpdateResource(&IndexBuffer);
	GResourceManager->UpdateResource(&VertexFactory);
}

void FParticleSpriteSubUVEmitterInstance::Resize(UINT NewMaxActiveParticles)
{
	FParticleEmitterInstance::Resize(NewMaxActiveParticles);
}

void FParticleSpriteSubUVEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);
}

void FParticleSpriteSubUVEmitterInstance::Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
    // 
	VertexFactory.LocalToWorld		= Template->UseLocalSpace ? Component->LocalToWorld : FMatrix::Identity;
	VertexFactory.ScreenAlignment	= this->SpriteTemplate->ScreenAlignment;

	VertexBuffer.SetSize(ActiveParticles);
	IndexBuffer.SetSize(ActiveParticles);

	GResourceManager->UpdateResource(&VertexBuffer);
	GResourceManager->UpdateResource(&IndexBuffer);
	GResourceManager->UpdateResource(&VertexFactory);

	TrianglesToRender	= ActiveParticles * 2;
	MaxVertexIndex		= TrianglesToRender * 2 - 1;

	if (0 == TrianglesToRender)
		return;

	if (Template->EmitterRenderMode == ERM_Normal)
	{
		if (Context.View->ViewMode & SVM_WireframeMask)
		{
			PRI->DrawWireframe(
				&VertexFactory,
				&IndexBuffer,
				WT_TriList,
				FColor(255,0,0),
				0,
				TrianglesToRender,
				0,
				MaxVertexIndex
				);
		}
		else
		{
			PRI->DrawMesh(
				&VertexFactory,
				&IndexBuffer,
				Component->SelectedMaterial(Context,SpriteTemplate->Material ? SpriteTemplate->Material : GEngine->DefaultMaterial),
				(SpriteTemplate->Material ? SpriteTemplate->Material : GEngine->DefaultMaterial)->GetInstanceInterface(),
				0,
				TrianglesToRender,
				0,
				MaxVertexIndex
				);
		}
	}
	else
	if (Template->EmitterRenderMode == ERM_Point)
	{
		RenderDebug(Scene, Context, PRI, false);
	}
	else
	if (Template->EmitterRenderMode == ERM_Cross)
	{
		RenderDebug(Scene, Context, PRI, true);
	}
}

UINT FParticleSpriteSubUVEmitterInstance::RequiredBytes()
{
	//	FLOAT Interp
	//	FLOAT ImageH, ImageV
	//	FLOAT Image2H, Image2V
	return (sizeof(FLOAT) * 5);
}

UINT FParticleSpriteSubUVEmitterInstance::CalculateParticleStride(UINT ParticleSize)
{
	return ParticleSize;
}

void FParticleSpriteSubUVEmitterInstance::KillParticles()
{
	// Loop over the active particles... If their RelativeTime is > 1.0f (indicating they are dead),
	// move them to the 'end' of the active particle list.
	for(INT i=ActiveParticles-1; i>=0; i--)
	{
		const INT	CurrentIndex	= ParticleIndices[i];
		const BYTE* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;
		FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);
		if (Particle.RelativeTime > 1.0f)
		{
            FLOAT* pkFloats = (FLOAT*)((BYTE*)&Particle + PayloadOffset);
			pkFloats[0] = 0.0f;
			pkFloats[1] = 0.0f;
			pkFloats[2] = 0.0f;
			pkFloats[3] = 0.0f;
			pkFloats[4] = 0.0f;

			ParticleIndices[i]	= ParticleIndices[ActiveParticles-1];
			ParticleIndices[ActiveParticles-1]	= CurrentIndex;
			ActiveParticles--;
		}
	}
}

/*-----------------------------------------------------------------------------
	FParticleTrailEmitterInstance implementation.
-----------------------------------------------------------------------------*/
//	UParticleSpriteEmitter*				SpriteTemplate;		// Sprite emitter template.
//	UParticleModuleTypeDataTrail*		TrailTypeData;
//	FParticleTrailVertexBuffer			VertexBuffer;		// Vertex buffer.
//	FQuadIndexBuffer					IndexBuffer;		// Index buffer.
//	FParticleTrailVertexFactory			VertexFactory;		// specific vertex factory

FParticleTrailEmitterInstance::FParticleTrailEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) :
	FParticleEmitterInstance(InTemplate, InComponent), 
	SpriteTemplate(0), 
	TrailTypeData(0), 
	VertexBuffer(this),
	IndexBuffer()
{
}

void FParticleTrailEmitterInstance::Init()
{
}

void FParticleTrailEmitterInstance::Resize(UINT NewMaxActiveParticles)
{
}

void FParticleTrailEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
}

void FParticleTrailEmitterInstance::Render(FScene* Scene, const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
}

UINT FParticleTrailEmitterInstance::RequiredBytes()
{
	// 3               1              0
	// 1...|...|...|...5...|...|...|..0
	// TtPppppppppppppppNnnnnnnnnnnnnnn
	// Tt				= Type flags --> 00 = Middle of trail (nothing...)
	//									 01 = Start of trail
	//									 10 = End of trail
	// Ppppppppppppppp	= Previous index
	// Nnnnnnnnnnnnnnn	= Next index
	//		INT				Flags;
	//
	// NOTE: These values DO NOT get packed into the vertex buffer!
	return (sizeof(INT));
}

UINT FParticleTrailEmitterInstance::CalculateParticleStride(UINT ParticleSize)
{
	return ParticleSize;
}

void FParticleTrailEmitterInstance::KillParticles()
{
	// We will probably want to 'pass along' tangents here...
	FParticleEmitterInstance::KillParticles();
}

/*-----------------------------------------------------------------------------
	UParticleEmitter implementation.
-----------------------------------------------------------------------------*/

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	appErrorf(TEXT("UParticleEmitter::CreateInstance is pure virtual")); 
	return NULL; 
}

void UParticleEmitter::UpdateModuleLists()
{
	SpawnModules.Empty();
	UpdateModules.Empty();

	UParticleModule* Module;
	INT TypeDataModuleIndex = -1;

	for (INT i = 0; i < Modules.Num(); i++)
	{
		Module = Modules(i);
		if (!Module)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.AddItem(Module);
		}
		if (Module->bUpdateModule)
		{
			UpdateModules.AddItem(Module);
		}

		if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			TypeDataModule = Module;
			if (!Module->bSpawnModule && !Module->bUpdateModule)
			{
				// For now, remove it from the list and set it as the TypeDataModule
				TypeDataModuleIndex = i;
			}
		}
	}

	if (TypeDataModuleIndex != -1)
	{
		Modules.Remove(TypeDataModuleIndex);
	}
}

void UParticleEmitter::PostLoad()
{
	Super::PostLoad();

	if (GetLinker() && (GetLinker()->Ver() < 183))
	{
		ConvertedModules = false;
	}

	if (GIsEditor)
	{
		PeakActiveParticles = 0;
	}

	UpdateModuleLists();
}

void UParticleEmitter::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	check(GIsEditor);

	UpdateModuleLists();

	for(TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template)
		{
			INT i;

			for (i=0; i<It->Template->Emitters.Num(); i++)
			{
				if (It->Template->Emitters(i) == this)
				{
					It->UpdateInstances();
				}
			}
		}
	}

}

void UParticleEmitter::SetEmitterName(FName& Name)
{
	EmitterName = Name;
}

FName& UParticleEmitter::GetEmitterName()
{
	return EmitterName;
}

void UParticleEmitter::AddEmitterCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
	EdSetup->AddCurveToCurrentTab(SpawnRate, FString(TEXT("Spawn Rate")), EmitterEditorColor);
}

void UParticleEmitter::RemoveEmitterCurvesFromEditor(UInterpCurveEdSetup* EdSetup)
{
	EdSetup->RemoveCurve(SpawnRate);
	// Remove each modules curves as well.
    for (INT ii = 0; ii < Modules.Num(); ii++)
	{
		if (Modules(ii)->IsDisplayedInCurveEd(EdSetup))
		{
			// Remove it from the curve editor!
			Modules(ii)->RemoveModuleCurvesFromEditor(EdSetup);
		}
	}
}

/*-----------------------------------------------------------------------------
	UParticleSpriteEmitter implementation.
-----------------------------------------------------------------------------*/

FParticleEmitterInstance* UParticleSpriteEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	FParticleEmitterInstance* Instance = 0;

	Particle_ConvertEmitterModules(this);

	if (TypeDataModule)
	{
		UParticleModuleTypeDataBase* pkTypeData = CastChecked<UParticleModuleTypeDataBase>(TypeDataModule);
		if (pkTypeData)
		{
			Instance = pkTypeData->CreateInstance(this, InComponent);
		}
	}

	if (Instance == 0)
	{
		Instance = new FParticleSpriteEmitterInstance(this, InComponent);
	}

	check(Instance);

	Instance->Init();
	return Instance;
}

// Sets up this sprite emitter with sensible defaults so we can see some particles as soon as its created.
void UParticleSpriteEmitter::SetToSensibleDefaults()
{
	PreEditChange();

	// Spawn rate
	UDistributionFloatConstant* SpawnRateDist = Cast<UDistributionFloatConstant>(SpawnRate);
	if (SpawnRateDist)
	{
		SpawnRateDist->Constant = 20.f;
	}

	// Create basic set of modules

	// Lifetime module
	UParticleModuleLifetime* LifetimeModule = ConstructObject<UParticleModuleLifetime>(UParticleModuleLifetime::StaticClass(), this);
	UDistributionFloatUniform* LifetimeDist = Cast<UDistributionFloatUniform>(LifetimeModule->Lifetime);
	if (LifetimeDist)
	{
		LifetimeDist->Min = 1.0f;
		LifetimeDist->Max = 1.0f;
	}
	Modules.AddItem(LifetimeModule);

	// Size module
	UParticleModuleSize* SizeModule = ConstructObject<UParticleModuleSize>(UParticleModuleSize::StaticClass(), this);
	UDistributionVectorUniform* SizeDist = Cast<UDistributionVectorUniform>(SizeModule->StartSize);
	if (SizeDist)
	{
		SizeDist->Min = FVector(1.f, 1.f, 1.f);
		SizeDist->Max = FVector(1.f, 1.f, 1.f);
	}
	Modules.AddItem(SizeModule);

	// Initial velocity module
	UParticleModuleVelocity* VelModule = ConstructObject<UParticleModuleVelocity>(UParticleModuleVelocity::StaticClass(), this);
	UDistributionVectorUniform* VelDist = Cast<UDistributionVectorUniform>(VelModule->StartVelocity);
	if (VelDist)
	{
		VelDist->Min = FVector(-10.f, -10.f, 50.f);
		VelDist->Max = FVector(10.f, 10.f, 100.f);
	}
	Modules.AddItem(VelModule);

	PostEditChange(NULL);
}

DWORD UParticleSpriteEmitter::GetLayerMask() const
{ 
	if (Material)
	{
		return Material->GetLayerMask(); 
	}
	else
	{
		return GEngine->DefaultMaterial->GetLayerMask();
	}
}

/*-----------------------------------------------------------------------------
	UParticleSpriteSubUVEmitter implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleSpriteSubUVEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	//@todo. Once this class can be removed, also remove the F* class below
	FParticleEmitterInstance* Instance = new FParticleSubUVDummyInstance(this, InComponent);
	return Instance;
}

// Sets up this sprite emitter with sensible defaults so we can see some particles as soon as its created.
void UParticleSpriteSubUVEmitter::SetToSensibleDefaults()
{
	PreEditChange();

	// Spawn rate
	UDistributionFloatConstant* SpawnRateDist = Cast<UDistributionFloatConstant>(SpawnRate);
	if (SpawnRateDist)
	{
		SpawnRateDist->Constant = 20.f;
	}

	// Create basic set of modules

	// Lifetime module
	UParticleModuleLifetime* LifetimeModule = ConstructObject<UParticleModuleLifetime>(UParticleModuleLifetime::StaticClass(), this);
	UDistributionFloatUniform* LifetimeDist = Cast<UDistributionFloatUniform>(LifetimeModule->Lifetime);
	if (LifetimeDist)
	{
		LifetimeDist->Min = 1.0f;
		LifetimeDist->Max = 1.0f;
	}
	Modules.AddItem(LifetimeModule);

	// Size module
	UParticleModuleSize* SizeModule = ConstructObject<UParticleModuleSize>(UParticleModuleSize::StaticClass(), this);
	UDistributionVectorUniform* SizeDist = Cast<UDistributionVectorUniform>(SizeModule->StartSize);
	if (SizeDist)
	{
		SizeDist->Min = FVector(15.f, 15.f, 15.f);
		SizeDist->Max = FVector(30.f, 30.f, 30.f);
	}
	Modules.AddItem(SizeModule);

	// Initial velocity module
	UParticleModuleVelocity* VelModule = ConstructObject<UParticleModuleVelocity>(UParticleModuleVelocity::StaticClass(), this);
	UDistributionVectorUniform* VelDist = Cast<UDistributionVectorUniform>(VelModule->StartVelocity);
	if (VelDist)
	{
		VelDist->Min = FVector(-10.f, -10.f, 50.f);
		VelDist->Max = FVector(10.f, 10.f, 100.f);
	}
	Modules.AddItem(VelModule);

    // SubUV image index module
    UParticleModuleSubUV* SubUVModule = ConstructObject<UParticleModuleSubUV>(UParticleModuleSubUV::StaticClass(), this);
    Modules.AddItem(SubUVModule);

	PostEditChange(NULL);
}

DWORD UParticleSpriteSubUVEmitter::GetLayerMask() const
{ 
	if (Material)
	{
		return Material->GetLayerMask(); 
	}
	else
	{
		return GEngine->DefaultMaterial->GetLayerMask();
	}
}

/*-----------------------------------------------------------------------------
	UParticleSystem implementation.
-----------------------------------------------------------------------------*/

void UParticleSystem::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	UpdateTime_Delta = 1.0f / UpdateTime_FPS;

	for(TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template == this)
		{
			It->UpdateInstances();
		}
	}
}

void UParticleSystem::PostLoad()
{
	Super::PostLoad();

	bool bHadDeprecatedEmitters = false;

	// Remove any old emitters
	for (INT i = Emitters.Num() - 1; i >= 0; i--)
	{
		UParticleEmitter* Emitter = Emitters(i);
		if (Emitter->IsA(UParticleSpriteSubUVEmitter::StaticClass()))
		{
			Emitters.Remove(i);
			MarkPackageDirty();
			bHadDeprecatedEmitters = true;
		}
		else
		if (Emitter->IsA(UParticleMeshEmitter::StaticClass()))
		{
			Emitters.Remove(i);
			MarkPackageDirty();
			bHadDeprecatedEmitters = true;
		}
	}

	if (bHadDeprecatedEmitters && CurveEdSetup)
	{
		CurveEdSetup->ResetTabs();
	}
}

//
//	FParticleSystemThumbnailScene
//

struct FParticleSystemThumbnailScene: FPreviewScene
{
	FSceneView	View;
	UParticleSystemComponent* PartComponent;

	// Constructor/destructor.

	FParticleSystemThumbnailScene(UParticleSystem* InParticleSystem)
	{
		UBOOL bNewComponent = false;

		// If no preview component currently existing - create it now and warm it up.
		if (!InParticleSystem->PreviewComponent)
		{
			InParticleSystem->PreviewComponent = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass());
			InParticleSystem->PreviewComponent->Template = InParticleSystem;

			InParticleSystem->PreviewComponent->LocalToWorld.SetIdentity();
	
			bNewComponent = true;
		}

		PartComponent = InParticleSystem->PreviewComponent;
		check(PartComponent);

		// Add Particle component to this scene.
		if (PartComponent->IsValidComponent())
		{
			Components.AddItem(PartComponent);
			PartComponent->Scene = this;
			PartComponent->Created();
		}

		// If its new - tick it so its at the warmup time.
		if (bNewComponent && (PartComponent->WarmupTime == 0.0f))
		{
			FLOAT WarmupElapsed = 0.f;
			FLOAT WarmupTimestep = 0.02f;

			while(WarmupElapsed < InParticleSystem->ThumbnailWarmup)
			{
				InParticleSystem->PreviewComponent->Tick(WarmupTimestep);
				WarmupElapsed += WarmupTimestep;
			}
		}


		View.ViewMatrix = FTranslationMatrix(InParticleSystem->ThumbnailAngle.Vector() * InParticleSystem->ThumbnailDistance);
		View.ViewMatrix = View.ViewMatrix * FInverseRotationMatrix(InParticleSystem->ThumbnailAngle);
		View.ViewMatrix = View.ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));
		View.ProjectionMatrix = FPerspectiveMatrix(
			75.0f * (FLOAT)PI / 360.0f,
			1.0f,
			1.0f,
			NEAR_CLIPPING_PLANE
			);

		View.ShowFlags = SHOW_DefaultEditor;
		View.BackgroundColor = FColor(0,0,0);
	}

	virtual ~FParticleSystemThumbnailScene()
	{
		check(PartComponent);

		// Remove particle component from this scene before it is destroyed.
		PartComponent->Destroyed();
		PartComponent->Scene = NULL;
		Components.RemoveItem(PartComponent);
	}
};

void UParticleSystem::DrawThumbnail(EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz)
{
	FParticleSystemThumbnailScene	ThumbnailScene(this);

	InRI->DrawScene(
		FSceneContext(
			&ThumbnailScene.View,
			&ThumbnailScene,
			InX,
			InY,
			InFixedSz ? InFixedSz : 512 * InZoom,
			InFixedSz ? InFixedSz : 512 * InZoom
			)
		);
}

FThumbnailDesc UParticleSystem::GetThumbnailDesc(FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz)
{
	FThumbnailDesc td;

	td.Width = InFixedSz ? InFixedSz : 512*InZoom;
	td.Height = InFixedSz ? InFixedSz : 512*InZoom;
	return td;
}

INT UParticleSystem::GetThumbnailLabels(TArray<FString>* InLabels)
{
	InLabels->Empty();
	new(*InLabels)FString(TEXT("ParticleSystem"));
	new(*InLabels)FString(GetName());
	return InLabels->Num();
}


/*-----------------------------------------------------------------------------
	UParticleSystemComponent implementation.
-----------------------------------------------------------------------------*/
void UParticleSystemComponent::PostLoad()
{
	// Replace any 'SubUVStub' emitter instances with the new emitters w/ type data
	for (INT i = 0; i < EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances(i);
	}
	Super::PostLoad();
}

void UParticleSystemComponent::Destroy()
{
	ResetParticles();
	Super::Destroy();
}

void UParticleSystemComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		for(INT i=0; i<EmitterInstances.Num(); i++)
		{
			EmitterInstances(i)->Serialize(Ar);
		}
	}
}

void UParticleSystemComponent::Created()
{
	Super::Created();

	if (bAutoActivate)
	{
		InitializeSystem();
	}
}

void UParticleSystemComponent::Destroyed()
{
	for(INT i=0; i<EmitterInstances.Num(); i++)
	{
		EmitterInstances(i)->RemovedFromScene();
	}
	Super::Destroyed();
}

void UParticleSystemComponent::PreEditChange()
{
	ResetParticles();
	Super::PreEditChange();
}

void UParticleSystemComponent::PostEditChange(UProperty* PropertyThatChanged)
{
	Super::PostEditChange(PropertyThatChanged);

	if (bAutoActivate)
	{
		InitializeSystem();
	}
}

DWORD UParticleSystemComponent::GetLayerMask(const FSceneContext& Context) const
{
	DWORD Mask = 0;
	for(INT i=0; i<EmitterInstances.Num(); i++)
	{
		Mask |= EmitterInstances(i)->Template->GetLayerMask();
	}

	if ((Context.View->ShowFlags & SHOW_Bounds) && (!GIsEditor || !Owner || GSelectionTools.IsSelected(Owner)))
		Mask |= PLM_Foreground;

	return Mask;
}

void UParticleSystemComponent::Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	for(INT i=0; i<EmitterInstances.Num(); i++)
		EmitterInstances(i)->Render(Scene, Context, PRI);
}

void UParticleSystemComponent::UpdateBounds()
{
	FBox BoundingBox;
	BoundingBox.Init();

	for(INT i=0; i<EmitterInstances.Num(); i++)
		BoundingBox += EmitterInstances(i)->GetBoundingBox();

	Bounds = FBoxSphereBounds(BoundingBox);
}

void UParticleSystemComponent::Tick(FLOAT DeltaTime)
{
	if (!Template)
		return;

	// 
	if (Template->SystemUpdateMode == EPSUM_FixedTime)
	{
		// Use the fixed delta time!
		DeltaTime = Template->UpdateTime_Delta;
	}

	// Tick Subemitters.
	for(INT i=0; i<EmitterInstances.Num(); i++)
	{
		EmitterInstances(i)->Tick(DeltaTime, bSuppressSpawning);
	}

	// If component has just totally finished, call script event.
	UBOOL bIsCompleted = HasCompleted(); 
	if (bIsCompleted && !bWasCompleted)
	{
		// When system is done - destroy all subemitters etc. We don't need them any more.
		ResetParticles();

		AEmitter* EmitActor = Cast<AEmitter>(Owner);
		if (EmitActor)
		{
			EmitActor->eventOnParticleSystemFinished(this);
		}
	}
	bWasCompleted = bIsCompleted;

	// Update bounding box.
	Update();

	//@todo. Biggest HACK attempt!!!!
//	if (GIsEditor || ShouldCollide())
//	{
//		Scene->RemovePrimitive(this);
//	}

	PartSysVelocity = (LocalToWorld.GetOrigin() - OldPosition) / DeltaTime;
	OldPosition = LocalToWorld.GetOrigin();
}

// If particles have not already been initialised (ie. EmitterInstances created) do it now.
void UParticleSystemComponent::InitParticles()
{
	if (Template)
	{
		WarmupTime = Template->WarmupTime;

		// If nothing is initialised, create EmitterInstances here.
		if (EmitterInstances.Num() == 0)
		{
			for(INT i=0; i<Template->Emitters.Num(); i++)
			{
				EmitterInstances.AddItem(Template->Emitters(i)->CreateInstance(this));
			}

			bWasCompleted = false;
		}
	}
	else
	{
		check(EmitterInstances.Num() == 0);
	}
}

void UParticleSystemComponent::ResetParticles()
{
	for(INT i=0; i<EmitterInstances.Num(); i++)
	{
		delete EmitterInstances(i);
		EmitterInstances(i) = NULL;
	}
	EmitterInstances.Empty();
}

void UParticleSystemComponent::SetTemplate(class UParticleSystem* NewTemplate)
{
	ResetParticles();

	Template = NewTemplate;
	if (Template)
	{
		WarmupTime = Template->WarmupTime;
	}
	else
	{
		WarmupTime = 0.0f;
	}


	if (bAutoActivate)
	{
		InitializeSystem();
	}
}

void UParticleSystemComponent::execSetTemplate(FFrame& Stack, RESULT_DECL)
{
	P_GET_OBJECT(UParticleSystem,NewTemplate);
	P_FINISH;

	SetTemplate(NewTemplate);
}

void UParticleSystemComponent::execActivateSystem(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	if (EmitterInstances.Num() == 0)
	{
		InitializeSystem();
	}
	else
	{
		// If currently running, re-activating rewinds the emitter to the start. Existing particles should stick around.
		for(INT i=0; i<EmitterInstances.Num(); i++)
		{
			EmitterInstances(i)->Rewind();
		}
	}

	// Stop suppressing particle spawning.
	bSuppressSpawning = false;
}

void UParticleSystemComponent::execDeactivateSystem(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	bSuppressSpawning = true;
}

void UParticleSystemComponent::UpdateInstances()
{
	ResetParticles();

	if (bAutoActivate)
	{
		InitializeSystem();
	}
}

UBOOL UParticleSystemComponent::HasCompleted()
{
	UBOOL bHasCompleted = true;

	for(INT i=0; i<EmitterInstances.Num(); i++)
	{
		if (!EmitterInstances(i)->HasCompleted())
			bHasCompleted = false;
	}

	return bHasCompleted;
}

void UParticleSystemComponent::InitializeSystem()
{
	if (Initialized)
	{
		InitParticles();

		if (WarmupTime != 0.0f)
		{
			FLOAT WarmupElapsed = 0.f;
			FLOAT WarmupTimestep = 0.02f;

			while (WarmupElapsed < WarmupTime)
			{
				Tick(WarmupTimestep);
				WarmupElapsed += WarmupTimestep;
			}
			WarmupTime = 0.0f;
		}
	}
}

void UParticleSystemComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	if ((Context.View->ShowFlags & SHOW_Bounds) && (!GIsEditor || !Owner || GSelectionTools.IsSelected(Owner)))
	{
		PRI->DrawWireBox(Bounds.GetBox(), FColor(72,72,255));

		PRI->DrawCircle(Bounds.Origin,FVector(1,0,0),FVector(0,1,0),FColor(255,255,0),Bounds.SphereRadius,32);
		PRI->DrawCircle(Bounds.Origin,FVector(1,0,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32);
		PRI->DrawCircle(Bounds.Origin,FVector(0,1,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32);
	}
}

INT UParticleSystemComponent::SetFloatParameter(const FName& Name, FLOAT Param)
{
	return -1;
}

void UParticleSystemComponent::execSetFloatParameter(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(Name);
	P_GET_FLOAT(Value);
	P_FINISH;

	*(INT*)Result = SetFloatParameter(Name, Value);
}

INT UParticleSystemComponent::SetVectorParameter(const FName& Name, const FVector& Param)
{
	return -1;
}

void UParticleSystemComponent::execSetVectorParameter(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(Name);
	P_GET_VECTOR(Value);
	P_FINISH;

	*(INT*)Result = SetVectorParameter(Name, Value);
}

INT UParticleSystemComponent::SetColorParameter(const FName& Name, const FColor& Param)
{
	return -1;
}

void UParticleSystemComponent::execSetColorParameter(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(Name);
	P_GET_STRUCT(FColor,Value);
	P_FINISH;

	*(INT*)Result = SetColorParameter(Name, Value);
}

INT UParticleSystemComponent::SetActorParameter(const FName& Name, const AActor* Param)
{
	return -1;
}

void UParticleSystemComponent::execSetActorParameter(FFrame& Stack, RESULT_DECL)
{
	P_GET_NAME(Name);
	P_GET_ACTOR(Value);
	P_FINISH;

	*(INT*)Result = SetActorParameter(Name, Value);
}

/*-----------------------------------------------------------------------------
	FQuadIndexBuffer implementation.
-----------------------------------------------------------------------------*/

FQuadIndexBuffer::FQuadIndexBuffer()
{
	NumQuads	= 0;
	Stride		= sizeof(_WORD);
	Size		= NumQuads * 6 * Stride;
}

void FQuadIndexBuffer::SetSize(INT InNumQuads)
{
	NumQuads	= InNumQuads;
	Stride		= InNumQuads > 5000 ? sizeof(DWORD) : sizeof(_WORD);
	Size		= NumQuads * 6 * Stride;
}

void FQuadIndexBuffer::GetData(void* Buffer)
{
	if (Stride == sizeof(_WORD))
	{
		_WORD* Index = (_WORD*) Buffer;
		for(INT i=0; i<NumQuads; i++)
		{
			*(Index++) = 4*i + 0;
			*(Index++) = 4*i + 2;
			*(Index++) = 4*i + 3;

			*(Index++) = 4*i + 0;
			*(Index++) = 4*i + 1;
			*(Index++) = 4*i + 2;
		}
	}
	else
	{
		DWORD* Index = (DWORD*) Buffer;
		for(INT i=0; i<NumQuads; i++)
		{
			*(Index++) = 4*i + 0;
			*(Index++) = 4*i + 2;
			*(Index++) = 4*i + 3;

			*(Index++) = 4*i + 0;
			*(Index++) = 4*i + 1;
			*(Index++) = 4*i + 2;
		}
	}
}

/*-----------------------------------------------------------------------------
	FParticleSpriteVertexBuffer implementation.
-----------------------------------------------------------------------------*/

FParticleSpriteVertexBuffer::FParticleSpriteVertexBuffer(FParticleSpriteEmitterInstance* InOwner)
{
	Owner		= InOwner;
	Stride		= sizeof(FParticleSpriteVertex);
	Size		= 0;
}

void FParticleSpriteVertexBuffer::SetSize(INT NumQuads)
{
	Size		= NumQuads * 4 * Stride;
}

struct FParticleOrder
{
	INT		ParticleIndex;
	FLOAT	Z;
	FParticleOrder(INT InParticleIndex,FLOAT InZ):
		ParticleIndex(InParticleIndex),
		Z(InZ)
	{}
};

IMPLEMENT_COMPARE_CONSTREF(FParticleOrder,UnParticleComponents,{ return A.Z < B.Z ? 1 : -1; });

void FParticleSpriteVertexBuffer::GetData(void* Buffer)
{
	FParticleSpriteVertex*	Vertex			= (FParticleSpriteVertex*) Buffer;
	const INT				ScreenAlignment	= ((UParticleSpriteEmitter*)(Owner->Template))->ScreenAlignment;

	TArray<FParticleOrder> ParticleOrder;

	BEGINCYCLECOUNTER(GParticlesStats.SortingTime);
	ParticleOrder.Empty(Owner->ActiveParticles);
	for(UINT ParticleIndex = 0;ParticleIndex < Owner->ActiveParticles;ParticleIndex++)
	{
		DECLARE_PARTICLE(Particle, Owner->ParticleData + Owner->ParticleStride * Owner->ParticleIndices[ParticleIndex]);
		new(ParticleOrder) FParticleOrder(ParticleIndex,ParticleView->Project(Particle.Location).Z);
	}
	Sort<USE_COMPARE_CONSTREF(FParticleOrder,UnParticleComponents)>(&ParticleOrder(0),ParticleOrder.Num());
	ENDCYCLECOUNTER;

	for(INT i=0; i<ParticleOrder.Num(); i++)
	{
		DECLARE_PARTICLE(Particle, Owner->ParticleData + Owner->ParticleStride * Owner->ParticleIndices[ParticleOrder(i).ParticleIndex]);

		FVector Size = Particle.Size;
		if (ScreenAlignment == PSA_Square)
			Size.Y = Size.X;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->U			= 0;
		Vertex->V			= 0;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex++;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->U			= 0;
		Vertex->V			= 1;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex++;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->U			= 1;
		Vertex->V			= 1;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex++;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->U			= 1;
		Vertex->V			= 0;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex++;
	}
}

/*-----------------------------------------------------------------------------
	FParticleSpriteSubUVVertexBuffer implementation.
-----------------------------------------------------------------------------*/
FParticleSpriteSubUVVertexBuffer::FParticleSpriteSubUVVertexBuffer(FParticleSpriteSubUVEmitterInstance* InOwner)
{
	Owner		= InOwner;
	Stride		= sizeof(FParticleSpriteSubUVVertex);
	Size		= 0;
}

void FParticleSpriteSubUVVertexBuffer::SetSize(INT InNumQuads)
{
	Size		= InNumQuads * 4 * Stride;
}

void FParticleSpriteSubUVVertexBuffer::GetData(void* Buffer)
{
	FParticleSpriteSubUVVertex*	Vertex			= (FParticleSpriteSubUVVertex*)Buffer;
	const INT					ScreenAlignment	= ((UParticleSpriteEmitter*)(Owner->Template))->ScreenAlignment;

	FParticleSpriteSubUVEmitterInstance* Instance = (FParticleSpriteSubUVEmitterInstance*)(Owner);

	UParticleModuleTypeDataSubUV* SubUVTypeData = Instance->SubUVTypeData;

	INT iTotalSubImages = SubUVTypeData->SubImages_Horizontal * SubUVTypeData->SubImages_Vertical;
	FLOAT baseU = (1.0f / SubUVTypeData->SubImages_Horizontal);
	FLOAT baseV = (1.0f / SubUVTypeData->SubImages_Vertical);

	for (UINT i=0; i<Owner->ActiveParticles; i++)
	{
		DECLARE_PARTICLE(Particle, Owner->ParticleData + Owner->ParticleStride * Owner->ParticleIndices[i]);

		FVector Size = Particle.Size;
		if (ScreenAlignment == PSA_Square)
			Size.Y = Size.X;

		FLOAT* PayloadData = (FLOAT*)(((BYTE*)&Particle) + Owner->PayloadOffset);

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex->Interp		= PayloadData[0];
		Vertex->U			= baseU * ((INT)PayloadData[1] + 0);
		Vertex->V			= baseV * ((INT)PayloadData[2] + 0);
		Vertex->U2			= baseU * ((INT)PayloadData[3] + 0);
		Vertex->V2			= baseV * ((INT)PayloadData[4] + 0);
		Vertex->SizeU		= 0;
		Vertex->SizeV		= 0;
		Vertex++;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex->Interp		= PayloadData[0];
		Vertex->U			= baseU * ((INT)PayloadData[1] + 0);
		Vertex->V			= baseV * ((INT)PayloadData[2] + 1);
		Vertex->U2			= baseU * ((INT)PayloadData[3] + 0);
		Vertex->V2			= baseV * ((INT)PayloadData[4] + 1);
		Vertex->SizeU		= 0;
		Vertex->SizeV		= 1;
		Vertex++;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex->Interp		= PayloadData[0];
		Vertex->U			= baseU * ((INT)PayloadData[1] + 1);
		Vertex->V			= baseV * ((INT)PayloadData[2] + 1);
		Vertex->U2			= baseU * ((INT)PayloadData[3] + 1);
		Vertex->V2			= baseV * ((INT)PayloadData[4] + 1);
		Vertex->SizeU		= 1;
		Vertex->SizeV		= 1;
		Vertex++;

		Vertex->Position	= Particle.Location;
		Vertex->OldPosition	= Particle.OldLocation;
		Vertex->Size		= Size;
		Vertex->Rotation	= Particle.Rotation;
		Vertex->Color		= Particle.Color;
		Vertex->Interp		= PayloadData[0];
		Vertex->U			= baseU * ((INT)PayloadData[1] + 1);
		Vertex->V			= baseV * ((INT)PayloadData[2] + 0);
		Vertex->U2			= baseU * ((INT)PayloadData[3] + 1);
		Vertex->V2			= baseV * ((INT)PayloadData[4] + 0);
		Vertex->SizeU		= 1;
		Vertex->SizeV		= 0;
		Vertex++;
	}
}

/*-----------------------------------------------------------------------------
	FParticleTrailVertexBuffer implementation.
-----------------------------------------------------------------------------*/
FParticleTrailVertexBuffer::FParticleTrailVertexBuffer(FParticleTrailEmitterInstance* InOwner)
{
	Owner	= InOwner;
	// We don't pack the data that is stored in the 'vertex'...
	Stride	= sizeof(FParticleSpriteVertex);
	Size	= 0;
}

void FParticleTrailVertexBuffer::SetSize(INT InNumQuads)
{
	Size	= InNumQuads * 4 * Stride;
}

void FParticleTrailVertexBuffer::GetData(void* Buffer)
{
}

/*-----------------------------------------------------------------------------
	UParticleMeshEmitter implementation.
-----------------------------------------------------------------------------*/
FParticleEmitterInstance* UParticleMeshEmitter::CreateInstance(
	UParticleSystemComponent* InComponent)
{
	//@todo. Once this class can be removed, also remove the F* class below
	FParticleEmitterInstance* Instance = new FParticleMeshDummyInstance(this, InComponent);
    Instance->Init();

	return Instance;
}

void UParticleMeshEmitter::SetToSensibleDefaults()
{
	PreEditChange();

	// Spawn rate
	UDistributionFloatConstant* SpawnRateDist = Cast<UDistributionFloatConstant>(SpawnRate);
	if (SpawnRateDist)
	{
		SpawnRateDist->Constant = 20.f;
	}

	// Create basic set of modules

	// Lifetime module
	UParticleModuleLifetime* LifetimeModule = ConstructObject<UParticleModuleLifetime>(UParticleModuleLifetime::StaticClass(), this);
	UDistributionFloatUniform* LifetimeDist = Cast<UDistributionFloatUniform>(LifetimeModule->Lifetime);
	if (LifetimeDist)
	{
		LifetimeDist->Min = 1.0f;
		LifetimeDist->Max = 1.0f;
	}
	Modules.AddItem(LifetimeModule);

	// Size module
	UParticleModuleSize* SizeModule = ConstructObject<UParticleModuleSize>(UParticleModuleSize::StaticClass(), this);
	UDistributionVectorUniform* SizeDist = Cast<UDistributionVectorUniform>(SizeModule->StartSize);
	if (SizeDist)
	{
		SizeDist->Min = FVector(15.f, 15.f, 15.f);
		SizeDist->Max = FVector(30.f, 30.f, 30.f);
	}
	Modules.AddItem(SizeModule);

	// Initial velocity module
	UParticleModuleVelocity* VelModule = ConstructObject<UParticleModuleVelocity>(UParticleModuleVelocity::StaticClass(), this);
	UDistributionVectorUniform* VelDist = Cast<UDistributionVectorUniform>(VelModule->StartVelocity);
	if (VelDist)
	{
		VelDist->Min = FVector(-10.f, -10.f, 50.f);
		VelDist->Max = FVector(10.f, 10.f, 100.f);
	}
	Modules.AddItem(VelModule);

	PostEditChange(NULL);
}

DWORD UParticleMeshEmitter::GetLayerMask() const
{
	DWORD LayerMask = 0;
	if (Mesh)
	{
		for(INT MaterialIndex = 0;MaterialIndex < Mesh->Materials.Num();MaterialIndex++)
		{
			if (Mesh->Materials(MaterialIndex).Material)
				LayerMask |= Mesh->Materials(MaterialIndex).Material->GetLayerMask();
		}
	}

	if (LayerMask)
		return LayerMask;
	return GEngine->DefaultMaterial->GetLayerMask();
}
