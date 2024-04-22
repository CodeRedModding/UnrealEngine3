
#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"

#if WITH_APEX_PARTICLES

#include "ParticleEmitterInstances_Apex.h"
#include "NvApexScene.h"

IMPLEMENT_PARTICLEEMITTERINSTANCE_TYPE(FParticleApexEmitterInstance);

/** Constructor	*/
FParticleApexEmitterInstance::FParticleApexEmitterInstance(class UParticleModuleTypeDataApex &TypeData) :
	ApexTypeData(TypeData)
{
	ApexEmitter  = NULL;
	CascadeScene = NULL;
	
	FRBPhysScene *InRBPhysScene = GetScene();
	FIApexScene  *ApexScene     = InRBPhysScene ? InRBPhysScene->ApexScene : 0;
	if(ApexScene && TypeData.ApexEmitter && TypeData.ApexEmitter->MApexAsset )
	{
		ApexEmitter = ApexScene->CreateApexEmitter(TypeData.ApexEmitter->MApexAsset->GetAssetName());
	}
}

/** Destructor	*/
FParticleApexEmitterInstance::~FParticleApexEmitterInstance(void)
{
	if(ApexEmitter) 
	{
		ApexEmitter->release();
		ApexEmitter = NULL;
	}
	if(CascadeScene)
	{
		DestroyRBPhysScene(CascadeScene);
		CascadeScene = NULL;
	}
}

void FParticleApexEmitterInstance::Tick(FLOAT DeltaTime, UBOOL bSuppressSpawning)
{
	if ( GetApexEmitter() && !GetApexEmitter()->IsEmpty() )
	{
		ActiveParticles = 1;
	}
	else
	{
		ActiveParticles = 0;
	}
	//Super::Tick(DeltaTime, bSuppressSpawning); // TODO: should we just override any default behavior?
	if(GIsEditor == TRUE && GIsGame == FALSE && CascadeScene) //cascade mode
	{
		TickRBPhysScene(CascadeScene, DeltaTime);
		WaitRBPhysScene(CascadeScene);
	}
}

FRBPhysScene *FParticleApexEmitterInstance::GetScene(void)
{
	FRBPhysScene* Scene = NULL;
	if(GIsEditor == TRUE && GIsGame == FALSE) //cascade mode
	{
		if(!CascadeScene)
		{
			AWorldInfo *pInfo = (AWorldInfo*)AWorldInfo::StaticClass()->GetDefaultObject();
			check(pInfo);
			FVector Gravity(0, 0, pInfo->DefaultGravityZ * pInfo->RBPhysicsGravityScaling);
			CascadeScene = CreateRBPhysScene(Gravity);
			check(CascadeScene);

			NxPlaneShapeDesc PlaneShape;
			NxActorDesc Actor;
			PlaneShape.normal.set(0.0f, 0.0f, 1.0f);
			PlaneShape.d = -5.0f;
			FRBCollisionChannelContainer CollidesWith(0);
			CollidesWith.SetChannel(RBCC_Default, TRUE);
			PlaneShape.groupsMask = CreateGroupsMask(RBCC_Default, &CollidesWith);
			Actor.shapes.pushBack(&PlaneShape);
			NxScene* SceneNx = CascadeScene->GetNovodexPrimaryScene();
			check(SceneNx);
			SceneNx->createActor(Actor);
		}
		Scene = CascadeScene;
	
	}
	else if(GIsGame)
	{
		// TODO...
		check(0);
	}
	return Scene;
}

FDynamicEmitterDataBase* FParticleApexEmitterInstance::GetDynamicData(UBOOL bSelected)
{
	FDynamicEmitterDataBase*ret = NULL;
	FRBPhysScene *InRBPhysScene = GetScene();
	FIApexScene  *ApexScene     = InRBPhysScene ? InRBPhysScene->ApexScene : 0;
	if(ApexScene)
	{
		FDynamicApexEmitterDataBase *EmitterDataBase = new FDynamicApexEmitterDataBase(CurrentLODLevel->RequiredModule,ApexScene,this);
		ret = EmitterDataBase;
	}
	return ret;
}

FDynamicApexEmitterDataBase::FDynamicApexEmitterDataBase(const UParticleModuleRequired *required,FIApexScene *apexScene,FParticleApexEmitterInstance *emitterInstance) : FDynamicEmitterDataBase(required)
{
	ApexScene = apexScene;
	EmitterInstance = emitterInstance;
}

FDynamicApexEmitterDataBase::~FDynamicApexEmitterDataBase(void)
{

}

// Render thread only draw call
INT FDynamicApexEmitterDataBase::Render(FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex)
{
	if ( EmitterInstance && EmitterInstance->GetApexEmitter() )
	{
		FApexEmitter *emitter = EmitterInstance->GetApexEmitter();
		if ( !emitter->IsEmpty() )
		{
			physx::apex::NxUserRenderer *Renderer = GApexRender->CreateDynamicRenderer( PDI,FALSE,FALSE,FALSE,View,DPGIndex);

			emitter->UpdateRenderResources();
			emitter->Render(*Renderer);

			GApexRender->ReleaseDynamicRenderer(Renderer);
		}
	}
	return 1;
}

const FMaterialRenderProxy*		FDynamicApexEmitterDataBase::GetMaterialRenderProxy(UBOOL bSelected)
{
	return NULL;
}

/**
 *	Called during FSceneRenderer::InitViews for view processing on scene proxies before rendering them
 *  Only called for primitives that are visible and have bDynamicRelevance
 *
 *	@param	Proxy			The 'owner' particle system scene proxy
 *	@param	ViewFamily		The ViewFamily to pre-render for
 *	@param	VisibilityMap	A BitArray that indicates whether the primitive was visible in that view (index)
 *	@param	FrameNumber		The frame number of this pre-render
 */
void FDynamicApexEmitterDataBase::PreRenderView(FParticleSystemSceneProxy* Proxy, const FSceneViewFamily* ViewFamily, const DWORD VisibilityMap, INT FrameNumber)
{
}

/** Returns the source data for this particle system */
const FDynamicEmitterReplayDataBase& FDynamicApexEmitterDataBase::GetSource(void) const
{
	return ReplaySource;
}

/** Release the resource for the data */
void FDynamicApexEmitterDataBase::ReleaseResource(void)
{

}

#endif
