/*=============================================================================
	UnLevel.cpp: Level-related functions
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "EngineSequenceClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineDecalClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineAnimClasses.h"
#include "UnOctree.h"
#include "UnTerrain.h"
#include "ScenePrivate.h"
#include "PrecomputedLightVolume.h"
#include "UnNovodexSupport.h"
#include "NvApexManager.h"
#include "NvApexCommands.h"

#if WITH_APEX
#include "UnNovodexSupport.h"
#include <NxApexSDK.h>
#include <NxApexSDKCachedData.h>
#endif

IMPLEMENT_CLASS(ULineBatchComponent);

/*-----------------------------------------------------------------------------
	ULevelBase implementation.
-----------------------------------------------------------------------------*/

ULevelBase::ULevelBase( const FURL& InURL )
: Actors( this )
, URL( InURL )

{}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void ULevelBase::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevelBase, Actors ) );
}

void ULevelBase::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Actors;
	Ar << URL;
}
IMPLEMENT_CLASS(ULevelBase);


/*-----------------------------------------------------------------------------
	ULevel implementation.
-----------------------------------------------------------------------------*/

/** Whether we have a pending call to BuildStreamingData(). */
UBOOL ULevel::bStreamingDataDirty = FALSE;

/** Timestamp (in appSeconds) when the next call to BuildStreamingData() should be made, if bDirtyStreamingData is TRUE. */
DOUBLE ULevel::BuildStreamingDataTimer = 0.0;

//@deprecated with VER_SPLIT_SOUND_FROM_TEXTURE_STREAMING
struct FStreamableResourceInstanceDeprecated
{
	FSphere BoundingSphere;
	FLOAT TexelFactor;
	friend FArchive& operator<<( FArchive& Ar, FStreamableResourceInstanceDeprecated& ResourceInstance )
	{
		Ar << ResourceInstance.BoundingSphere;
		Ar << ResourceInstance.TexelFactor;
		return Ar;
	}
};
//@deprecated with VER_SPLIT_SOUND_FROM_TEXTURE_STREAMING
struct FStreamableResourceInfoDeprecated
{
	UObject* Resource;
	TArray<FStreamableResourceInstanceDeprecated> ResourceInstances;
	friend FArchive& operator<<( FArchive& Ar, FStreamableResourceInfoDeprecated& ResourceInfo )
	{
		Ar << ResourceInfo.Resource;
		Ar << ResourceInfo.ResourceInstances;
		return Ar;
	}
};
//@deprecated with VER_RENDERING_REFACTOR
struct FStreamableSoundInstanceDeprecated
{
	FSphere BoundingSphere;
	friend FArchive& operator<<( FArchive& Ar, FStreamableSoundInstanceDeprecated& SoundInstance )
	{
		Ar << SoundInstance.BoundingSphere;
		return Ar;
	}
};
//@deprecated with VER_RENDERING_REFACTOR
struct FStreamableSoundInfoDeprecated
{
	USoundNodeWave*	SoundNodeWave;
	TArray<FStreamableSoundInstanceDeprecated> SoundInstances;
	friend FArchive& operator<<( FArchive& Ar, FStreamableSoundInfoDeprecated& SoundInfo )
	{
		Ar << SoundInfo.SoundNodeWave;
		Ar << SoundInfo.SoundInstances;
		return Ar;
	}
};
//@deprecated with VER_RENDERING_REFACTOR
struct FStreamableTextureInfoDeprecated
{
	UTexture*							Texture;
	TArray<FStreamableTextureInstance>	TextureInstances;
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInfoDeprecated& TextureInfo )
	{
		Ar << TextureInfo.Texture;
		Ar << TextureInfo.TextureInstances;
		return Ar;
	}
};

INT FPrecomputedVisibilityHandler::NextId = 0;

/** Updates visibility stats. */
void FPrecomputedVisibilityHandler::UpdateVisibilityStats(UBOOL bAllocating) const
{
	if (bAllocating)
	{
		INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (INT BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).Cells.GetAllocatedSize());
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.GetAllocatedSize());
			for (INT ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.Num(); ChunkIndex++)
			{
				INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks(ChunkIndex).Data.GetAllocatedSize());
			}
		}
	}
	else
	{
		DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (INT BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).Cells.GetAllocatedSize());
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.GetAllocatedSize());
			for (INT ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.Num(); ChunkIndex++)
			{
				DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks(ChunkIndex).Data.GetAllocatedSize());
			}
		}
	}
}

/** Sets this visibility handler to be actively used by the rendering scene. */
void FPrecomputedVisibilityHandler::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && PrecomputedVisibilityCellBuckets.Num() > 0)
	{
		Scene->SetPrecomputedVisibility(this);
	}
}

/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
void FPrecomputedVisibilityHandler::Invalidate(FSceneInterface* Scene)
{
	Scene->SetPrecomputedVisibility(NULL);
	// Block until the renderer no longer references this FPrecomputedVisibilityHandler so we can delete its data
	FlushRenderingCommands();
	UpdateVisibilityStats(FALSE);
	PrecomputedVisibilityCellBucketOriginXY = FVector2D(0,0);
	PrecomputedVisibilityCellSizeXY = 0;
	PrecomputedVisibilityCellSizeZ = 0;
	PrecomputedVisibilityCellBucketSizeXY = 0;
	PrecomputedVisibilityNumCellBuckets = 0;
	PrecomputedVisibilityCellBuckets.Empty();
	// Bump the Id so FSceneViewState will know to discard its cached visibility data
	Id = NextId;
	NextId++;
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D )
{
	Ar << D.PrecomputedVisibilityCellBucketOriginXY;
	Ar << D.PrecomputedVisibilityCellSizeXY;
	Ar << D.PrecomputedVisibilityCellSizeZ;
	Ar << D.PrecomputedVisibilityCellBucketSizeXY;
	Ar << D.PrecomputedVisibilityNumCellBuckets;
	Ar << D.PrecomputedVisibilityCellBuckets;
	if (Ar.IsLoading())
	{
		D.UpdateVisibilityStats(TRUE);
	}
	return Ar;
}


/** Sets this volume distance field to be actively used by the rendering scene. */
void FPrecomputedVolumeDistanceField::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(this);
	}
}

/** Invalidates the level's volume distance field and frees any memory used by it. */
void FPrecomputedVolumeDistanceField::Invalidate(FSceneInterface* Scene)
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(NULL);
		// Block until the renderer no longer references this FPrecomputedVolumeDistanceField so we can delete its data
		FlushRenderingCommands();
		Data.Empty();
	}
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D )
{
	Ar << D.VolumeMaxDistance;
	Ar << D.VolumeBox;
	Ar << D.VolumeSizeX;
	Ar << D.VolumeSizeY;
	Ar << D.VolumeSizeZ;
	Ar << D.Data;

	return Ar;
}

IMPLEMENT_CLASS(ULevel);

ULevel::ULevel( const FURL& InURL )
:	ULevelBase( InURL )
,	PrecomputedLightVolume(NULL)
{
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void ULevel::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, Model ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, ModelComponents ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, GameSequences ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, NavListStart ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, NavListEnd ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, CoverListStart ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, CoverListEnd ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, PylonListStart ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, PylonListEnd ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, CrossLevelActors ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, CoverLinkRefs ) );

	new(TheClass,TEXT("LightmapTotalSize"),RF_Public) UFloatProperty(CPP_PROPERTY(LightmapTotalSize),TEXT(""),CPF_EditConst|CPF_Const);
	new(TheClass,TEXT("ShadowmapTotalSize"),RF_Public) UFloatProperty(CPP_PROPERTY(ShadowmapTotalSize),TEXT(""),CPF_EditConst|CPF_Const);
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void ULevel::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );

	// Let GC know that we're referencing some UTexture2D objects
	for( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(TextureToInstancesMap); It; ++It )
	{
		UTexture2D* Texture2D = It.Key();
		AddReferencedObject( ObjectArray, Texture2D );
	}

	// Let GC know that we're referencing some UTexture2D objects
	for( TMap<UPrimitiveComponent*,TArray<FDynamicTextureInstance> >::TIterator It(DynamicTextureInstances); It; ++It )
	{
		UPrimitiveComponent* Primitive = It.Key();
		TArray<FDynamicTextureInstance>& TextureInstances = It.Value();

		AddReferencedObject( ObjectArray, Primitive );
		for ( INT InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
		{
			FDynamicTextureInstance& Instance = TextureInstances( InstanceIndex );
			AddReferencedObject( ObjectArray, Instance.Texture );
		}
	}

	// Let GC know that we're referencing some UTexture2D objects
	for( TMap<UTexture2D*,UBOOL>::TIterator It(ForceStreamTextures); It; ++It )
	{
		UTexture2D* Texture2D = It.Key();
		AddReferencedObject( ObjectArray, Texture2D );
	}
	
 	for( INT CovIdx = 0; CovIdx < CoverLinkRefs.Num(); CovIdx++ )
 	{
		ACoverLink* Link = CoverLinkRefs(CovIdx);
		if( Link != NULL )
		{
			AddReferencedObject( ObjectArray, Link );
		} 
 	}
}

void ULevel::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << Model;

	Ar << ModelComponents;

	Ar << GameSequences;

	if( !Ar.IsTransacting() )
	{
		Ar << TextureToInstancesMap;

		if ( Ar.Ver() >= VER_DYNAMICTEXTUREINSTANCES )
		{
			Ar << DynamicTextureInstances;
		}

		if(Ar.Ver() >= VER_APEX_DESTRUCTION)
		{
#if WITH_APEX
			if(Ar.IsLoading())
			{
				DWORD Size;
				Ar << Size;
				if( Size > 16 )
				{
					InitializeApex();
					TArray<BYTE> Buffer;
					Buffer.Add( Size );
					Ar.Serialize( Buffer.GetData(), Size );
					physx::apex::NxApexSDKCachedData& nCachedData = GApexManager->GetApexSDK()->getCachedData();
					physx::PxFileBuf* nStream = GApexManager->GetApexSDK()->createMemoryReadStream( Buffer.GetData(), Size );
					if( nStream != NULL )
					{
						nCachedData.deserialize( *nStream );
						GApexManager->GetApexSDK()->releaseMemoryReadStream( *nStream );
					}
				}
				else
				{
					for (DWORD i=0; i<Size; i++)
					{
						char c;
						Ar << c;
					}
				}
			}
			else if ( Ar.IsSaving() )
			{
				physx::PxU32 Len = 0;
				void* Data = NULL;
				physx::PxFileBuf* nStream = NULL;
				if( GApexManager )
				{
					physx::apex::NxApexSDKCachedData& nCachedData = GApexManager->GetApexSDK()->getCachedData();
					nStream = GApexManager->GetApexSDK()->createMemoryWriteStream();
					if( nStream != NULL )
					{
						nCachedData.serialize( *nStream );
						Data = (void*)GApexManager->GetApexSDK()->getMemoryWriteBuffer( *nStream, Len );
					}
				}
				Ar << Len;
				if( Len != 0 )
				{
					Ar.Serialize( Data, Len );
				}
				if( nStream != NULL )
				{
					GApexManager->GetApexSDK()->releaseMemoryWriteStream( *nStream );
				}
			}
#else
			if (Ar.IsLoading())
			{
				DWORD Size;
				Ar << Size;
				Ar.Seek(Ar.Tell() + Size);
			}
			else if (Ar.IsSaving())
			{
				DWORD Len = 0;
				Ar << Len;
			}
#endif // if WITH_APEX
		}

		CachedPhysBSPData.BulkSerialize(Ar);
    
	    Ar << CachedPhysSMDataMap;
	    Ar << CachedPhysSMDataStore;
	    Ar << CachedPhysPerTriSMDataMap;
	    Ar << CachedPhysPerTriSMDataStore;
        Ar << CachedPhysBSPDataVersion;
	    Ar << CachedPhysSMDataVersion;
		Ar << ForceStreamTextures;

		if(Ar.Ver() >= VER_CONVEX_BSP)
		{
			Ar << CachedPhysConvexBSPData;
			Ar << CachedPhysConvexBSPVersion;
		}
	}

	// Mark archive and package as containing a map if we're serializing to disk.
	if( !HasAnyFlags( RF_ClassDefaultObject ) && Ar.IsPersistent() )
	{
		Ar.ThisContainsMap();
		GetOutermost()->ThisContainsMap();
	}

	// serialize the nav list
	Ar << NavListStart;
	Ar << NavListEnd;
	// and cover
	Ar << CoverListStart;
	Ar << CoverListEnd;
	// and pylons
	if(Ar.Ver() >= VER_PYLONLIST_IN_ULEVEL)
	{
		Ar << PylonListStart;
		Ar << PylonListEnd;
	}

	if( Ar.Ver() >= VER_COVERGUIDREFS_IN_ULEVEL )
	{
		Ar << CrossLevelCoverGuidRefs;
		Ar << CoverLinkRefs;
		Ar << CoverIndexPairs;
	}

	// serialize the list of cross level actors
	Ar << CrossLevelActors;
	if (Ar.Ver() >= VER_GI_CHARACTER_LIGHTING)
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			FPrecomputedLightVolume DummyVolume;
			Ar << DummyVolume;
		}
		else
		{
			if (!PrecomputedLightVolume)
			{
				PrecomputedLightVolume = new FPrecomputedLightVolume();
			}
			Ar << *PrecomputedLightVolume;
		}
	}

	if (Ar.Ver() >= VER_NONUNIFORM_PRECOMPUTED_VISIBILITY)
	{
		Ar << PrecomputedVisibilityHandler;
	}
	else if (Ar.Ver() >= VER_PRECOMPUTED_VISIBILITY)
	{
		FBox LegacyPrecomputedVisibilityVolume(0);
		FLOAT LegacyPrecomputedVisibilityCellSize = 0;
		TArray<TArray<BYTE> > LegacyPrecomputedVisibilityData;
		Ar << LegacyPrecomputedVisibilityVolume;
		Ar << LegacyPrecomputedVisibilityCellSize;
		Ar << LegacyPrecomputedVisibilityData;
	}

	if (Ar.Ver() >= VER_IMAGE_REFLECTION_SHADOWING)
	{
		Ar << PrecomputedVolumeDistanceField;
	}
}


/**
 * Sorts the actor list by net relevancy and static behaviour. First all not net relevant static
 * actors, then all net relevant static actors and then the rest. This is done to allow the dynamic
 * and net relevant actor iterators to skip large amounts of actors.
 */
void ULevel::SortActorList()
{
	TickableActors.Reset();
	PendingUntickableActors.Reset();

	INT StartIndex = 0;
	TArray<AActor*> NewActors;
	NewActors.Reserve(Actors.Num());

	// The world info and default brush have fixed actor indices.
	NewActors.AddItem(Actors(StartIndex++));
	NewActors.AddItem(Actors(StartIndex++));

	// Static not net relevant actors.
	for (INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);
		if (Actor != NULL && !Actor->bDeleteMe && Actor->IsStatic() && Actor->RemoteRole == ROLE_None)
		{
			NewActors.AddItem(Actor);
		}
	}
	iFirstNetRelevantActor = NewActors.Num();

	// Static net relevant actors.
	for (INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);		
		if (Actor != NULL && !Actor->bDeleteMe && Actor->IsStatic() && Actor->RemoteRole > ROLE_None)
		{
			NewActors.AddItem(Actor);
		}
	}
	iFirstDynamicActor = NewActors.Num();

	// Remaining (dynamic, potentially net relevant actors)
	for (INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);			
		if (Actor != NULL && !Actor->bDeleteMe && !Actor->IsStatic())
		{
			NewActors.AddItem(Actor);
			if (Actor->WantsTick())
			{
				TickableActors.AddItem(Actor);
			}
		}
	}

	// Replace with sorted list.
	Actors = NewActors;

	// Don't use sorted optimization outside of gameplay so we can safely shuffle around actors e.g. in the Editor
	// without there being a chance to break code using dynamic/ net relevant actor iterators.
	if (!GIsGame)
	{
		iFirstNetRelevantActor = 0;
		iFirstDynamicActor = 0;
	}
}

/**
 * Recreates the array of tickable actors starting from the given actor index. 
 *
 * @param	StartIndex	The index to start in the level's master Actor list. This 
 *						MUST be zero or greater. Otherwise, the function will crash. 
 *						Default StartIndex is zero.
 */
void ULevel::RebuildTickableActors( INT StartIndex )
{
	// The index must be at least zero or greater. If the index is equal to the 
	// number of elements or greater, the tickable actors array will be empty. 
	// This is a valid case when building the world for the first time.
	check( StartIndex >= 0 );

	TickableActors.Reset();

	for( INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);			

		// Tickable actors should not be marked to delete nor should they be static
		if( Actor != NULL && !Actor->bDeleteMe && !Actor->IsStatic() )
		{
			if( Actor->WantsTick() )
			{
				TickableActors.AddItem(Actor);
			}
		}
	}
}

/**
 * Makes sure that all light components have valid GUIDs associated.
 */
void ULevel::ValidateLightGUIDs()
{
	for( TObjectIterator<ULightComponent> It; It; ++It )
	{
		ULightComponent*	LightComponent	= *It;
		UBOOL				IsInLevel		= LightComponent->IsIn( this );

		if( IsInLevel )
		{
			LightComponent->ValidateLightGUIDs();
		}
	}
}

/** 
 * Associate teleporters with the portal volume they are in
 */
void ULevel::AssociatePortals( void )
{
	check( GWorld );

	for( TObjectIterator<APortalTeleporter> It; It; ++It )
	{
		APortalTeleporter* Teleporter = static_cast<APortalTeleporter*>( *It );
		APortalVolume* Volume = GWorld->GetWorldInfo()->GetPortalVolume( Teleporter->Location );

		if( Volume )
		{
			Volume->Portals.AddUniqueItem( Teleporter );
		}
	}
}

/**
 * Presave function, gets called once before the level gets serialized (multiple times) for saving.
 * Used to rebuild streaming data on save.
 */
void ULevel::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	if( !IsTemplate() )
	{
		UPackage* Package = CastChecked<UPackage>(GetOutermost());

		ValidateLightGUIDs();

		// Build bsp-trimesh data for physics engine
		BuildPhysBSPData();

		// clean up the nav list
		GWorld->RemoveLevelNavList(this);
        // if one of the pointers are NULL then eliminate both to prevent possible crashes during gameplay before paths are rebuilt
        if (NavListStart == NULL || NavListEnd == NULL)
        {
            if (HasPathNodes())
            {
                //@todo - add a message box (but make sure it doesn't pop up for autosaves?)
                debugf(NAME_Warning,TEXT("PATHING NEEDS TO BE REBUILT FOR %s"),*GetPathName());
            }
            NavListStart = NULL;
            NavListEnd = NULL;
        }

		// Associate portal teleporters with portal volumes
		AssociatePortals();

		// Clear out any crosslevel references
		for( INT ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
		{
			AActor *Actor = Actors(ActorIdx);
			if( Actor != NULL )
			{
				Actor->ClearCrossLevelReferences();
			}
		}

		// Build the list of cross level actors
		CrossLevelActors.Empty();
		for( INT ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
		{
			AActor *Actor = Actors(ActorIdx);
			if( Actor != NULL && !Actor->IsPendingKill() )
			{
				TArray<FActorReference*> ActorRefs;
				Actor->GetActorReferences(ActorRefs,TRUE);
				Actor->GetActorReferences(ActorRefs,FALSE);
				if( ActorRefs.Num() > 0 )
				{
					// and null the cross level references
					UBOOL bHasCrossLevelRef = FALSE;
					for( INT Idx = 0; Idx < ActorRefs.Num(); Idx++ )
					{
						if( ActorRefs(Idx)->Actor == NULL || Cast<ULevel>(ActorRefs(Idx)->Actor->GetOuter()) != this )
						{
							bHasCrossLevelRef = TRUE;
							ActorRefs(Idx)->Actor = NULL;
						}
						else
						{
							ActorRefs(Idx)->Guid = FGuid(EC_EventParm);
						}
					}
					if( bHasCrossLevelRef )
					{
						CrossLevelActors.AddItem(Actor);
					}
				}
			}
		}

		// Fixup crosslevel cover refs/guids
		ClearCrossLevelCoverReferences();

		// Don't rebuild streaming data if we're saving out a cooked package as the raw data required has already been stripped.
		if( !(Package->PackageFlags & PKG_Cooked) )
		{
			BuildStreamingData(NULL, this);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Removes existing line batch components from actors and associates streaming data with level.
 */
void ULevel::PostLoad()
{
	Super::PostLoad();

	//@todo: investigate removal of code cleaning up existing LineBatchComponents.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if(Actor)
		{
			for( INT ComponentIndex=0; ComponentIndex<Actor->Components.Num(); ComponentIndex++ )
			{
				UActorComponent* Component = Actor->Components(ComponentIndex);
				if( Component && Component->IsA(ULineBatchComponent::StaticClass()) )
				{
					check(!Component->IsAttached());
					Actor->Components.Remove(ComponentIndex--);
				}
			}
		}
	}

	// reattach decals to receivers after level has been fully loaded
	GEngine->IssueDecalUpdateRequest();

	// in the Editor, sort Actor list immediately (at runtime we wait for the level to be added to the world so that it can be delayed in the level streaming case)
	if (GIsEditor)
	{
		SortActorList();
	}

	// Remove UTexture2D references that are NULL (missing texture).
	ForceStreamTextures.RemoveKey( NULL );
}

/**
 * Clears all components of actors associated with this level (aka in Actors array) and 
 * also the BSP model components.
 */
void ULevel::ClearComponents()
{
	bAreComponentsCurrentlyAttached = FALSE;

	// Remove the model components from the scene.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->ConditionalDetach();
		}
	}

	// Remove the actors' components from the scene.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor )
		{
			Actor->ClearComponents();
		}
	}

	// clear global motion blur state info
	if (GEngine != NULL && 
		GEngine->GameViewport != NULL &&
		GEngine->GameViewport->Viewport != NULL)
	{
		GEngine->GameViewport->Viewport->SetClearMotionBlurInfoGameThread(TRUE);
	}
}

/**
 * A TMap key type used to sort BSP nodes by locality and zone.
 */
struct FModelComponentKey
{
	UINT	ZoneIndex;
	UINT	X;
	UINT	Y;
	UINT	Z;
	DWORD	MaskedPolyFlags;
	DWORD	LightingChannels;

	friend UBOOL operator==(const FModelComponentKey& A,const FModelComponentKey& B)
	{
		return	A.ZoneIndex == B.ZoneIndex 
			&&	A.X == B.X 
			&&	A.Y == B.Y 
			&&	A.Z == B.Z 
			&&	A.MaskedPolyFlags == B.MaskedPolyFlags
			&&	A.LightingChannels == B.LightingChannels;
	}

	friend DWORD GetTypeHash(const FModelComponentKey& Key)
	{
		return appMemCrc(&Key,sizeof(Key),0);
	}
};

/**
 * Updates all components of actors associated with this level (aka in Actors array) and 
 * creates the BSP model components.
 */
void ULevel::UpdateComponents()
{
	// Update all components in one swoop.
	IncrementalUpdateComponents( 0 );
}


/**
 * Incrementally updates all components of actors associated with this level.
 *
 * @param NumComponentsToUpdate	Number of components to update in this run, 0 for all
 */
void ULevel::IncrementalUpdateComponents( INT NumComponentsToUpdate )
{
	// A value of 0 means that we want to update all components.
	UBOOL bForceUpdateAllActors = FALSE;
	if( NumComponentsToUpdate == 0 )
	{
		NumComponentsToUpdate = Actors.Num();
		bForceUpdateAllActors = TRUE;
	}
	// Only the game can use incremental update functionality.
	else
	{
		checkMsg(!GIsEditor && GIsGame,TEXT("Cannot call IncrementalUpdateComponents with non 0 argument in the Editor/ commandlets."));
	}

	// Do BSP on the first pass.
	if( CurrentActorIndexForUpdateComponents == 0 )
	{
		UpdateModelComponents();
	}

	// Do as many Actor's as we were told, with the exception of 'collection' actors. They contain a variable number of 
	// components that can take more time than we are anticipating at a higher level. Unless we do a force full update we
	// only do up to the first collection and only one collection at a time.
	UBOOL bShouldBailOutEarly = FALSE;
	NumComponentsToUpdate = Min( NumComponentsToUpdate, Actors.Num() - CurrentActorIndexForUpdateComponents );
	for( INT i=0; i<NumComponentsToUpdate && !bShouldBailOutEarly; i++ )
	{
		AActor* Actor = Actors(CurrentActorIndexForUpdateComponents++);
		if( Actor )
		{
			// Request an early bail out if we encounter a SMCA... unless we force update all actors.
			UBOOL bIsCollectionActor = Actor->IsA(AStaticMeshCollectionActor::StaticClass()) || Actor->IsA(AProcBuilding::StaticClass());
			bShouldBailOutEarly = bIsCollectionActor ? !bForceUpdateAllActors : FALSE; 

			// Always do at least one and keep going as long as its not a SMCA
			if( !bShouldBailOutEarly || i == 0 )  
			{
#if PERF_TRACK_DETAILED_ASYNC_STATS
				DOUBLE Start = appSeconds();
#endif

				Actor->ClearComponents();
				Actor->ConditionalUpdateComponents();
				// Shrink various components arrays for static actors to avoid waste due to array slack.
				if( Actor->IsStatic() )
				{
					Actor->Components.Shrink();
					Actor->AllComponents.Shrink();
				}

#if PERF_TRACK_DETAILED_ASYNC_STATS
				// Add how long this took to class->time map
				DOUBLE Time = appSeconds() - Start;
				UClass* ActorClass = Actor->GetClass();
				FMapTimeEntry* CurrentEntry = UpdateComponentsTimePerActorClass.Find(ActorClass);
				// Is an existing entry - add to it
				if(CurrentEntry)
				{
					CurrentEntry->Time += Time;
					CurrentEntry->ObjCount += 1;
				}
				// Make a new entry for this class
				else
				{
					UpdateComponentsTimePerActorClass.Set(ActorClass, FMapTimeEntry(ActorClass, 1, Time));
				}
#endif
			}
			else
			{
				// Rollback since we didn't actually process the actor
				CurrentActorIndexForUpdateComponents--;
				break;
			}		
		}
	}

	// See whether we are done.
	if( CurrentActorIndexForUpdateComponents == Actors.Num() )
	{
		CurrentActorIndexForUpdateComponents	= 0;
		bAreComponentsCurrentlyAttached			= TRUE;
	}
	// Only the game can use incremental update functionality.
	else
	{
		check(!GIsEditor && GIsGame);
	}
}



/**
 * Updates the model components associated with this level
 */
void ULevel::UpdateModelComponents()
{
	// Create/update the level's BSP model components.
	if(!ModelComponents.Num())
	{
		// Update the model vertices and edges.
		Model->UpdateVertices();

		Model->InvalidSurfaces = 0;

	    // Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();

		TMap< FModelComponentKey, TArray<WORD> > ModelComponentMap;

		// Sort the nodes by zone, grid cell and masked poly flags.
		for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Model->Nodes(NodeIndex);
			FBspSurf& Surf = Model->Surfs(Node.iSurf);

			if(Node.NumVertices > 0)
			{
				for(INT BackFace = 0;BackFace < ((Surf.PolyFlags & PF_TwoSided) ? 2 : 1);BackFace++)
				{
					// Calculate the bounding box of this node.
					FBox NodeBounds(0);
					for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						NodeBounds += Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
					}

					// Create a sort key for this node using the grid cell containing the center of the node's bounding box.
#define MODEL_GRID_SIZE_XY	2048.0f
#define MODEL_GRID_SIZE_Z	4096.0f
					FModelComponentKey Key;
					Key.ZoneIndex		= Model->NumZones ? Node.iZone[1 - BackFace] : INDEX_NONE;

					if (GWorld->GetWorldInfo()->bMinimizeBSPSections)
					{
						Key.X				= 0;
						Key.Y				= 0;
						Key.Z				= 0;
					}
					else
					{
						Key.X				= appFloor(NodeBounds.GetCenter().X / MODEL_GRID_SIZE_XY);
						Key.Y				= appFloor(NodeBounds.GetCenter().Y / MODEL_GRID_SIZE_XY);
						Key.Z				= appFloor(NodeBounds.GetCenter().Z / MODEL_GRID_SIZE_Z);
					}

					Key.MaskedPolyFlags = Surf.PolyFlags & PF_ModelComponentMask;
					Key.LightingChannels = Surf.LightingChannels.Bitfield;
					// Don't accept lights if material is unlit.
					if( Surf.Material && Surf.Material->GetMaterial() && Surf.Material->GetMaterial()->LightingModel == MLM_Unlit )
					{
						Key.MaskedPolyFlags	= Key.MaskedPolyFlags & (~PF_AcceptsLights);
					}
			
					// Find an existing node list for the grid cell.
					TArray<WORD>* ComponentNodes = ModelComponentMap.Find(Key);
					if(!ComponentNodes)
					{
						// This is the first node we found in this grid cell, create a new node list for the grid cell.
						ComponentNodes = &ModelComponentMap.Set(Key,TArray<WORD>());
					}

					// Add the node to the grid cell's node list.
					ComponentNodes->AddUniqueItem(NodeIndex);
				}
			}
			else
			{
				// Put it in component 0 until a rebuild occurs.
 				Node.ComponentIndex = 0;
			}
		}

		// Create a UModelComponent for each grid cell's node list.
		for(TMap< FModelComponentKey, TArray<WORD> >::TConstIterator It(ModelComponentMap);It;++It)
		{
			const FModelComponentKey&	Key		= It.Key();
			const TArray<WORD>&			Nodes	= It.Value();	

			for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
			{
				Model->Nodes(Nodes(NodeIndex)).ComponentIndex = ModelComponents.Num();							
				Model->Nodes(Nodes(NodeIndex)).ComponentNodeIndex = NodeIndex;
			}
			
			UModelComponent* ModelComponent = new(this) UModelComponent(Model,Key.ZoneIndex,ModelComponents.Num(),Key.MaskedPolyFlags,Key.LightingChannels,Nodes);
			ModelComponents.AddItem(ModelComponent);

			for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
			{
				Model->Nodes(Nodes(NodeIndex)).ComponentElementIndex = INDEX_NONE;
				
				const WORD								Node	 = Nodes(NodeIndex);
				const TIndirectArray<FModelElement>&	Elements = ModelComponent->GetElements();
				for( INT ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++ )
				{
					if( Elements(ElementIndex).Nodes.FindItemIndex( Node ) != INDEX_NONE )
					{
						Model->Nodes(Nodes(NodeIndex)).ComponentElementIndex = ElementIndex;
						break;
					}
				}
			}
		}

		// Clear old cached data in case we don't regenerate it below, e.g. after removing all BSP from a level.
		Model->NumIncompleteNodeGroups = 0;
		Model->CachedMappings.Empty();

		// Work only needed if we actually have BSP in the level.
		if( ModelComponents.Num() )
		{
			// Build the static lighting vertices!
			/** The lights in the world which the system is building. */
			TArray<ULightComponent*> Lights;
			// Prepare lights for rebuild.
			for(TObjectIterator<ULightComponent> LightIt;LightIt;++LightIt)
			{
				ULightComponent* const Light = *LightIt;
				const UBOOL bLightIsInWorld = Light->GetOwner() && GWorld->ContainsActor(Light->GetOwner());
				if (bLightIsInWorld && (Light->HasStaticShadowing() || Light->HasStaticLighting()))
				{
					// Make sure the light GUIDs and volumes are up-to-date.
					Light->ValidateLightGUIDs();

					// Add the light to the system's list of lights in the world.
					Lights.AddItem(Light);
				}
			}

			// For BSP, we aren't Component-centric, so we can't use the GetStaticLightingInfo 
			// function effectively. Instead, we look across all nodes in the Level's model and
			// generate NodeGroups - which are groups of nodes that are coplanar, adjacent, and 
			// have the same lightmap resolution (henceforth known as being "conodes"). Each 
			// NodeGroup will get a mapping created for it

			// create all NodeGroups
			Model->GroupAllNodes(this, Lights);

			// now we need to make the mappings/meshes
			for (TMap<INT, FNodeGroup*>::TIterator It(Model->NodeGroups); It; ++It)
			{
				FNodeGroup* NodeGroup = It.Value();

				if (NodeGroup->Nodes.Num())
				{
					// get one of the surfaces/components from the NodeGroup
					// @lmtodo: Remove need for GetSurfaceLightMapResolution to take a surfaceindex, or a ModelComponent :)
					UModelComponent* SomeModelComponent = ModelComponents(Model->Nodes(NodeGroup->Nodes(0)).ComponentIndex);
					INT SurfaceIndex = Model->Nodes(NodeGroup->Nodes(0)).iSurf;

					// fill out the NodeGroup/mapping, as UModelComponent::GetStaticLightingInfo did
					SomeModelComponent->GetSurfaceLightMapResolution(SurfaceIndex, TRUE, NodeGroup->SizeX, NodeGroup->SizeY, NodeGroup->WorldToMap, &NodeGroup->Nodes);
					NodeGroup->MapToWorld = NodeGroup->WorldToMap.Inverse();

					// Cache the surface's vertices and triangles.
					NodeGroup->BoundingBox.Init();

					UBOOL bForceLightMap = FALSE;

					for(INT NodeIndex = 0;NodeIndex < NodeGroup->Nodes.Num();NodeIndex++)
					{
						const FBspNode& Node = Model->Nodes(NodeGroup->Nodes(NodeIndex));
						const FBspSurf& NodeSurf = Model->Surfs(Node.iSurf);
						// If ANY surfaces in this group has ForceLightMap set, they all get it...
						if ((NodeSurf.PolyFlags & PF_ForceLightMap) > 0)
						{
							bForceLightMap = TRUE;
						}
						const FVector& TextureBase = Model->Points(NodeSurf.pBase);
						const FVector& TextureX = Model->Vectors(NodeSurf.vTextureU);
						const FVector& TextureY = Model->Vectors(NodeSurf.vTextureV);
						const INT BaseVertexIndex = NodeGroup->Vertices.Num();
						// Compute the surface's tangent basis.
						FVector NodeTangentX = Model->Vectors(NodeSurf.vTextureU).SafeNormal();
						FVector NodeTangentY = Model->Vectors(NodeSurf.vTextureV).SafeNormal();
						FVector NodeTangentZ = Model->Vectors(NodeSurf.vNormal).SafeNormal();

						// Generate the node's vertices.
						for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							/*const*/ FVert& Vert = Model->Verts(Node.iVertPool + VertexIndex);
							const FVector& VertexWorldPosition = Model->Points(Vert.pVertex);

							FStaticLightingVertex* DestVertex = new(NodeGroup->Vertices) FStaticLightingVertex;
							DestVertex->WorldPosition = VertexWorldPosition;
							DestVertex->TextureCoordinates[0].X = ((VertexWorldPosition - TextureBase) | TextureX) / 128.0f;
							DestVertex->TextureCoordinates[0].Y = ((VertexWorldPosition - TextureBase) | TextureY) / 128.0f;
							DestVertex->TextureCoordinates[1].X = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).X;
							DestVertex->TextureCoordinates[1].Y = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).Y;
							DestVertex->WorldTangentX = NodeTangentX;
							DestVertex->WorldTangentY = NodeTangentY;
							DestVertex->WorldTangentZ = NodeTangentZ;

							// TEMP - Will be overridden when lighting is build!
							Vert.ShadowTexCoord = DestVertex->TextureCoordinates[1];

							// Include the vertex in the surface's bounding box.
							NodeGroup->BoundingBox += VertexWorldPosition;
						}

						// Generate the node's vertex indices.
						for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + 0);
							NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex);
							NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex - 1);

							// track the source surface for each triangle
							NodeGroup->TriangleSurfaceMap.AddItem(Node.iSurf);
						}
					}
				}
			}
		}
		Model->UpdateVertices();

		for (INT UpdateCompIdx = 0; UpdateCompIdx < ModelComponents.Num(); UpdateCompIdx++)
		{
			UModelComponent* ModelComp = ModelComponents(UpdateCompIdx);
			ModelComp->GenerateElements(TRUE);
		}
	}
	else
	{
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents(ComponentIndex))
			{
				ModelComponents(ComponentIndex)->ConditionalDetach();
			}
		}
	}

	// Initialize the model's index buffers.
	for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
		IndexBufferIt;
		++IndexBufferIt)
	{
		BeginInitResource(IndexBufferIt.Value());
	}

	// Update model components.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->ConditionalAttach(GWorld->Scene,NULL,FMatrix::Identity);
		}
	}
}


/** Called before an Undo action occurs */
void ULevel::PreEditUndo()
{
	Super::PreEditUndo();

	// Release the model's resources.
	Model->BeginReleaseResources();
	Model->ReleaseResourcesFence.Wait();

	// Detach existing model components.  These are left in the array, so they are saved for undoing the undo.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->ConditionalDetach();
		}
	}

	// Wait for the components to be detached.
	FlushRenderingCommands();
}


/** Called after an Undo action occurs */
void ULevel::PostEditUndo()
{
	Super::PostEditUndo();
	
	// Rebuild the list of tickable actors because a tickable actor may have 
	// been removed during undo or redo. In that case, the tickable actors list 
	// will be holding a pointer to garbage after the next garbage collection, 
	// which will crash the engine.
	RebuildTickableActors();
	Model->UpdateVertices();
	// Update model components that were detached earlier
	UpdateModelComponents();
}



/**
 * Invalidates the cached data used to render the level's UModel.
 */
void ULevel::InvalidateModelGeometry()
{
	// Save the level/model state for transactions.
	Model->Modify();
	Modify();

	// Begin releasing the model's resources.
	Model->BeginReleaseResources();

	// Remove existing model components.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->Modify();
			ModelComponents(ComponentIndex)->ConditionalDetach();
		}
	}
	ModelComponents.Empty();
}

/**
 * Discards the cached data used to render the level's UModel.  Assumes that the
 * faces and vertex positions haven't changed, only the applied materials.
 */
void ULevel::InvalidateModelSurface()
{
	Model->InvalidSurfaces = TRUE;
}

void ULevel::CommitModelSurfaces()
{
	if(Model->InvalidSurfaces)
	{
		// Detach the model components
		TIndirectArray<FPrimitiveSceneAttachmentContext> ComponentContexts;
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			UPrimitiveComponent* Component = ModelComponents(ComponentIndex);
			if (Component)
			{
				new(ComponentContexts) FPrimitiveSceneAttachmentContext(Component);
			}
		}

		// Begin releasing the model's resources.
		Model->BeginReleaseResources();

		// Wait for the model's resources to be released.
		FlushRenderingCommands();

		// Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();

		// Update the model vertices.
		Model->UpdateVertices();

		// Update the model components.
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents(ComponentIndex))
			{
				ModelComponents(ComponentIndex)->CommitSurfaces();
			}
		}
		Model->InvalidSurfaces = 0;
		
		// Initialize the model's index buffers.
		for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
			IndexBufferIt;
			++IndexBufferIt)
		{
			BeginInitResource(IndexBufferIt.Value());
		}

		// After this line, the elements in the ComponentContexts array will be destructed, causing components to reattach.
	}
}

IMPLEMENT_COMPARE_CONSTREF( FStreamableTextureInstance, BuildStreamingData, { return (A.TexelFactor - B.TexelFactor) >= 0.0f ? 1 : -1 ; } )

/**
 * Rebuilds static streaming data for all levels in the specified UWorld.
 *
 * @param World				Which world to rebuild streaming data for. If NULL, all worlds will be processed.
 * @param TargetLevel		[opt] Specifies a single level to process. If NULL, all levels will be processed.
 * @param TargetTexture		[opt] Specifies a single texture to process. If NULL, all textures will be processed.
 */
void ULevel::BuildStreamingData(UWorld* World, ULevel* TargetLevel/*=NULL*/, UTexture2D* TargetTexture/*=NULL*/)
{
#if !CONSOLE
	DOUBLE StartTime = appSeconds();

	UBOOL bUseDynamicStreaming = FALSE;
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("UseDynamicStreaming"), bUseDynamicStreaming, GEngineIni);

	// Clear the streaming data.
	if ( TargetLevel )
	{
		// Update the streaming manager.
		GStreamingManager->RemoveLevel( TargetLevel );
		TargetLevel->TextureToInstancesMap.Empty();
		TargetLevel->DynamicTextureInstances.Empty();
		TargetLevel->ForceStreamTextures.Empty();
	}
	else if ( World )
	{
		for ( INT LevelIndex=0; LevelIndex < World->Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = World->Levels(LevelIndex);
			// Update the streaming manager.
			GStreamingManager->RemoveLevel( Level );
			Level->TextureToInstancesMap.Empty();
			Level->DynamicTextureInstances.Empty();
			Level->ForceStreamTextures.Empty();
		}
	}
	else
	{
		for (TObjectIterator<ULevel> It; It; ++It)
		{
			ULevel* Level = *It;
			// Update the streaming manager.
			GStreamingManager->RemoveLevel( Level );
			Level->TextureToInstancesMap.Empty();
			Level->DynamicTextureInstances.Empty();
			Level->ForceStreamTextures.Empty();
		}
	}

	for ( TObjectIterator<UPrimitiveComponent> It; It; ++It )
	{
		UPrimitiveComponent* Primitive = *It;

		UBOOL bIsClassDefaultOjbect = Primitive->IsTemplate(RF_ClassDefaultObject);

		if ( !bIsClassDefaultOjbect && (Primitive->IsAttached() || GIsCooking) )
		{
			// Find which level the primitive resides in.
			ULevel* Level = NULL;
			if ( TargetLevel )
			{
				Level = Primitive->IsIn(TargetLevel) ? TargetLevel : NULL;
			}
			else
			{
				for (UObject* It=Primitive; It && !Level; It = It->GetOuter())
				{
					Level = Cast<ULevel>(It);
				}
			}
			UBOOL bProcessLevel = (Level && !World) ? TRUE : FALSE;

			// Check that this level is part of the specified UWorld.
			if ( Level && World )
			{
				for ( INT LevelIndex=0; LevelIndex < World->Levels.Num(); LevelIndex++ )
				{
					if ( Level == World->Levels(LevelIndex) )
					{
						bProcessLevel = TRUE;
						break;
					}
				}
			}

			if ( bProcessLevel )
			{
				const AActor* const Owner				= Primitive->GetOwner();
				const UBOOL bIsStaticMeshComponent		= Primitive->IsA(UStaticMeshComponent::StaticClass());
				const UBOOL bIsSkeletalMeshComponent	= Primitive->IsA(USkeletalMeshComponent::StaticClass());
				const UBOOL bIsStatic					= !Owner || Owner->IsStatic();
				const UBOOL bIsLevelPlacedDynamicActor	= Owner && (Owner->IsA(AKActor::StaticClass()) || Owner->IsA(AInterpActor::StaticClass()) || Owner->IsA(ADynamicSMActor::StaticClass()));
				const UBOOL bUseAllStaticMeshComponents	= bIsStaticMeshComponent && (!Owner || Owner->bConsiderAllStaticMeshComponentsForStreaming);
				const UBOOL bStreamNonWorldTextures		= bIsStatic || bIsLevelPlacedDynamicActor || bUseAllStaticMeshComponents;
				const UBOOL bIsDynamicDecal				= !bIsStatic && Primitive->IsA(UDecalComponent::StaticClass());

				TArray<FStreamingTexturePrimitiveInfo> PrimitiveStreamingTextures;

				// Don't check dynamic decals. We don't want to add any references to a DecalComponent.
				if ( !bIsDynamicDecal )
				{
					// Ask the primitive to enumerate the streaming textures it uses.
					Primitive->GetStreamingTextureInfo(PrimitiveStreamingTextures);
				}

				for(INT TextureIndex = 0;TextureIndex < PrimitiveStreamingTextures.Num();TextureIndex++)
				{
					const FStreamingTexturePrimitiveInfo& PrimitiveStreamingTexture = PrimitiveStreamingTextures(TextureIndex);
					UTexture2D* Texture2D = Cast<UTexture2D>(PrimitiveStreamingTexture.Texture);
					UBOOL bCanBeStreamedByDistance = !appIsNearlyZero(PrimitiveStreamingTexture.TexelFactor) && !appIsNearlyZero(PrimitiveStreamingTexture.Bounds.W);

					// Only handle 2D textures that match the target texture.
					const UBOOL bIsTargetTexture = (!TargetTexture || TargetTexture == Texture2D);
					UBOOL bShouldHandleTexture = (Texture2D && bIsTargetTexture);

					// Check if this is a lightmap/shadowmap that shouldn't be streamed.
					if ( bShouldHandleTexture )
					{
						UShadowMapTexture2D* ShadowMap2D	= Cast<UShadowMapTexture2D>(Texture2D);
						ULightMapTexture2D* Lightmap2D		= Cast<ULightMapTexture2D>(Texture2D);
						if ( (Lightmap2D && (Lightmap2D->LightmapFlags & LMF_Streamed) == 0) ||
							(ShadowMap2D && (ShadowMap2D->ShadowmapFlags & SMF_Streamed) == 0) )
						{
							bShouldHandleTexture			= FALSE;
						}
					}

					// Check if this is a duplicate texture
					if (bShouldHandleTexture)
					{
						for (INT HandledTextureIndex = 0; HandledTextureIndex < TextureIndex; HandledTextureIndex++)
						{
							const FStreamingTexturePrimitiveInfo& HandledStreamingTexture = PrimitiveStreamingTextures(HandledTextureIndex);
							if ( PrimitiveStreamingTexture.Texture == HandledStreamingTexture.Texture &&
								 appIsNearlyEqual(PrimitiveStreamingTexture.TexelFactor, HandledStreamingTexture.TexelFactor) &&
								 PrimitiveStreamingTexture.Bounds.Equals( HandledStreamingTexture.Bounds ) )
							{
								// It's a duplicate, don't handle this one.
								bShouldHandleTexture = FALSE;
								break;
							}
						}
					}
					
					if(bShouldHandleTexture)
					{
						// Check if this is a world texture.
						const UBOOL bIsWorldTexture			= 
							Texture2D->LODGroup == TEXTUREGROUP_World ||
							Texture2D->LODGroup == TEXTUREGROUP_WorldNormalMap ||
							Texture2D->LODGroup == TEXTUREGROUP_WorldSpecular;

						// Check if we should consider this a static mesh texture instance.
						UBOOL bIsStaticMeshTextureInstance = (bStreamNonWorldTextures || bIsWorldTexture) && !bIsSkeletalMeshComponent;

						// Treat textures bIsLevelPlacedDynamicActor dynamically instead.
						if ( bIsLevelPlacedDynamicActor && bUseDynamicStreaming )
						{
							bIsStaticMeshTextureInstance = FALSE;
						}

						// Is the primitive set to force its textures to be resident?
						if ( Primitive->bForceMipStreaming )
						{
							// Add them to the ForceStreamTextures set.
							Level->ForceStreamTextures.Set(Texture2D,TRUE);
						}
						// Is this a static mesh texture instance?
						else if ( bIsStaticMeshTextureInstance && bCanBeStreamedByDistance )
						{
							// Texture instance information.
							FStreamableTextureInstance TextureInstance;
							TextureInstance.BoundingSphere	= PrimitiveStreamingTexture.Bounds;
							TextureInstance.TexelFactor		= PrimitiveStreamingTexture.TexelFactor;

							// See whether there already is an instance in the level.
							TArray<FStreamableTextureInstance>* TextureInstances = Level->TextureToInstancesMap.Find( Texture2D );
							// We have existing instances.
							if( TextureInstances )
							{
								// Add to the array.
								TextureInstances->AddItem( TextureInstance );
							}
							// This is the first instance.
							else
							{
								// Create array with current instance as the only entry.
								TArray<FStreamableTextureInstance> NewTextureInstances;
								NewTextureInstances.AddItem( TextureInstance );
								// And set it .
								Level->TextureToInstancesMap.Set( Texture2D, NewTextureInstances );
							}
						}
						// Is the texture used by a dynamic object that we can track at run-time.
						else if ( bUseDynamicStreaming && Owner && bCanBeStreamedByDistance )
						{
							// Texture instance information.
							FDynamicTextureInstance TextureInstance;
							TextureInstance.Texture = Texture2D;
							TextureInstance.BoundingSphere = PrimitiveStreamingTexture.Bounds;
							TextureInstance.TexelFactor	= PrimitiveStreamingTexture.TexelFactor;
							TextureInstance.OriginalRadius = PrimitiveStreamingTexture.Bounds.W;

							// See whether there already is an instance in the level.
							TArray<FDynamicTextureInstance>* TextureInstances = Level->DynamicTextureInstances.Find( Primitive );
							// We have existing instances.
							if( TextureInstances )
							{
								// Add to the array.
								TextureInstances->AddItem( TextureInstance );
							}
							// This is the first instance.
							else
							{
								// Create array with current instance as the only entry.
								TArray<FDynamicTextureInstance> NewTextureInstances;
								NewTextureInstances.AddItem( TextureInstance );
								// And set it .
								Level->DynamicTextureInstances.Set( Primitive, NewTextureInstances );
							}
						}
					}
				}
			}
		}
	}

	TObjectIterator<ULevel> It;
	INT LevelIndex = 0;
	while ( true )
	{
		ULevel* Level;
		if ( TargetLevel )
		{
			Level = TargetLevel;
		}
		else if ( World )
		{
			if ( LevelIndex >= World->Levels.Num() )
			{
				break;
			}
			Level = World->Levels(LevelIndex++);
		}
		else
		{
			if ( !It )
			{
				break;
			}
			Level = *It;
			++It;
		}

		for ( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(Level->TextureToInstancesMap); It; ++It )
		{
			UTexture2D* Texture2D = TargetTexture ? TargetTexture : It.Key();
			if ( Texture2D->LODGroup == TEXTUREGROUP_Lightmap || Texture2D->LODGroup == TEXTUREGROUP_Shadowmap )
			{
				TArray<FStreamableTextureInstance>& TextureInstances = It.Value();

				// Clamp texelfactors to 20-80% range.
				// This is to prevent very low-res or high-res charts to dominate otherwise decent streaming.
				Sort<USE_COMPARE_CONSTREF(FStreamableTextureInstance,BuildStreamingData)>( &(TextureInstances(0)), TextureInstances.Num() );

				FLOAT MinTexelFactor = TextureInstances( TextureInstances.Num() * 0.2f ).TexelFactor;
				FLOAT MaxTexelFactor = TextureInstances( TextureInstances.Num() * 0.8f ).TexelFactor;
				for ( INT InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
				{
					FStreamableTextureInstance& Instance = TextureInstances(InstanceIndex);
					Instance.TexelFactor = Clamp( Instance.TexelFactor, MinTexelFactor, MaxTexelFactor );
				}
			}
			if ( TargetTexture )
			{
				break;
			}
		}

		// Update the streaming manager.
		GStreamingManager->AddLevel( Level );

		if ( TargetLevel )
		{
			break;
		}
	}

	//debugf(TEXT("ULevel::BuildStreamingData took %.3f seconds."), appSeconds() - StartTime);
#else
	appErrorf(TEXT("ULevel::BuildStreamingData should not be called on a console"));
#endif
}

/**
 * Triggers a call to BuildStreamingData(GWorld,NULL,NULL) within a few seconds.
 */
void ULevel::TriggerStreamingDataRebuild()
{
	bStreamingDataDirty = TRUE;
	BuildStreamingDataTimer = appSeconds() + 5.0;
}

/**
 * Calls BuildStreamingData(GWorld,NULL,NULL) if it has been triggered within the last few seconds.
 */
void ULevel::ConditionallyBuildStreamingData()
{
	if ( bStreamingDataDirty && appSeconds() > BuildStreamingDataTimer )
	{
		bStreamingDataDirty = FALSE;
		BuildStreamingData( GWorld );
	}
}

/**
 *	Retrieves the array of streamable texture instances.
 *
 */
TArray<FStreamableTextureInstance>* ULevel::GetStreamableTextureInstances(UTexture2D*& TargetTexture)
{
	typedef TArray<FStreamableTextureInstance>	STIA_Type;
	for (TMap<UTexture2D*,STIA_Type>::TIterator It(TextureToInstancesMap); It; ++It)
	{
		TArray<FStreamableTextureInstance>& TSIA = It.Value();
		TargetTexture = It.Key();
		return &TSIA;
	}		

	return NULL;
}

/**
 * Returns the default brush for this level.
 *
 * @return		The default brush for this level.
 */
ABrush* ULevel::GetBrush() const
{
	checkMsg( Actors.Num() >= 2, *GetName() );
	ABrush* DefaultBrush = Cast<ABrush>( Actors(1) );
	checkMsg( DefaultBrush != NULL, *GetName() );
	checkMsg( DefaultBrush->BrushComponent, *GetName() );
	checkMsg( DefaultBrush->Brush != NULL, *GetName() );
	return DefaultBrush;
}

/**
 * Returns the world info for this level.
 *
 * @return		The AWorldInfo for this level.
 */
AWorldInfo* ULevel::GetWorldInfo() const
{
	check( Actors.Num() >= 2 );
	AWorldInfo* WorldInfo = Cast<AWorldInfo>( Actors(0) );
	check( WorldInfo != NULL );
	return WorldInfo;
}

/**
 * Returns the sequence located at the index specified.
 *
 * @return	a pointer to the USequence object located at the specified element of the GameSequences array.  Returns
 *			NULL if the index is not a valid index for the GameSequences array.
 */
USequence* ULevel::GetGameSequence() const
{
	USequence* Result = NULL;

	if( GameSequences.Num() )
	{
		Result = GameSequences(0);
	}

	return Result;
}

/**
 * Initializes all actors after loading completed.
 *
 * @param bForDynamicActorsOnly If TRUE, this function will only act on non static actors
 */
void ULevel::InitializeActors(UBOOL bForDynamicActorsOnly)
{
	UBOOL			bIsServer				= GWorld->IsServer();
	APhysicsVolume*	DefaultPhysicsVolume	= GWorld->GetDefaultPhysicsVolume();

	// Kill non relevant client actors, initialize render time, set initial physic volume, initialize script execution and rigid body physics.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor && ( !bForDynamicActorsOnly || !Actor->IsStatic() ) )
		{
			// Kill off actors that aren't interesting to the client.
			if( !bIsServer && !Actor->bScriptInitialized )
			{
				if (Actor->IsStatic() || Actor->bNoDelete)
				{
					if (!Actor->bExchangedRoles)
					{
						Exchange( Actor->Role, Actor->RemoteRole );
						Actor->bExchangedRoles = TRUE;
					}
				}
				else
				{
					GWorld->DestroyActor( Actor );
				}
			}

			if( !Actor->ActorIsPendingKill() )
			{
				Actor->LastRenderTime	= -FLT_MAX;
				Actor->PhysicsVolume	= DefaultPhysicsVolume;
				Actor->Touching.Empty();
				// don't reinitialize actors that have already been initialized (happens for actors that persist through a seamless level change)
				if (!Actor->bScriptInitialized || Actor->GetStateFrame() == NULL)
				{
					Actor->InitExecution();
				}
			}
		}
	}
}

/**
 * Routes pre and post begin play to actors and also sets volumes.
 *
 * @param bForDynamicActorsOnly If TRUE, this function will only act on non static actors
 *
 * @todo seamless worlds: this doesn't correctly handle volumes in the multi- level case
 */
void ULevel::RouteBeginPlay(UBOOL bForDynamicActorsOnly)
{
	// this needs to only be done once, so when we do this again for reseting
	// dynamic actors, we can't do it again
	if (!bForDynamicActorsOnly)
	{
		GWorld->AddLevelNavList( this, TRUE );
	}

	// Send PreBeginPlay, set zones and collect volumes.
	TArray<AVolume*> LevelVolumes;		
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor && ( !bForDynamicActorsOnly || !Actor->IsStatic() ) )
		{
			if( !Actor->bScriptInitialized && (!Actor->IsStatic() || Actor->bRouteBeginPlayEvenIfStatic) )
			{
				Actor->PreBeginPlay();
			}

			// Only collect non-blocking volumes
			AVolume* Volume = Actor->GetAVolume();
			if( Volume && !Volume->bBlockActors )
			{
				LevelVolumes.AddItem(Volume);
			}
		}
	}

	// Send set volumes, beginplay on components, and postbeginplay.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor && ( !bForDynamicActorsOnly || !Actor->IsStatic() ) )
		{
			if( !Actor->bScriptInitialized )
			{
				Actor->SetVolumes( LevelVolumes );
			}

			if( !Actor->IsStatic() || Actor->bRouteBeginPlayEvenIfStatic )
			{
#ifdef PERF_DEBUG_CHECKCOLLISIONCOMPONENTS
				INT NumCollisionComponents = 0;
#endif
				// Call BeginPlay on Components.
				for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
				{
					UActorComponent* ActorComponent = Actor->Components(ComponentIndex);
					if( ActorComponent && ActorComponent->IsAttached() )
					{
						ActorComponent->ConditionalBeginPlay();
#ifdef PERF_DEBUG_CHECKCOLLISIONCOMPONENTS
						UPrimitiveComponent *C = Cast<UPrimitiveComponent>(ActorComponent);
						if ( C && C->ShouldCollide() )
						{
							NumCollisionComponents++;
							if( NumCollisionComponents > 1 )
							{
								debugf(TEXT("additional collision component %s owned by %s"), *C->GetName(), *GetName());
							}
						}
#endif
					}
				}
			}
			if( !Actor->bScriptInitialized )
			{
				if( !Actor->IsStatic() || Actor->bRouteBeginPlayEvenIfStatic )
				{
					Actor->PostBeginPlay();
#if WITH_FACEFX
					APawn* ActorPawn = Cast<APawn>(Actor);
					ASkeletalMeshActor *ActorSkelMesh = Cast<ASkeletalMeshActor>(Actor);
					if(ActorPawn || ActorSkelMesh)
					{
						GWorld->MountPersistentFaceFXAnimSetOnActor(Actor);
					}				
#endif	//#if WITH_FACEFX
				}
				// Set script initialized if we skip routing begin play as some code relies on it.
				else
				{
					Actor->bScriptInitialized = TRUE;
				}
			}
		}
	}
}

UBOOL ULevel::HasAnyActorsOfType(UClass *SearchType)
{
	// just search the actors array
	for (INT Idx = 0; Idx < Actors.Num(); Idx++)
	{
		AActor *Actor = Actors(Idx);
		// if valid, not pending kill, and
		// of the correct type
		if (Actor != NULL &&
			!Actor->IsPendingKill() &&
			Actor->IsA(SearchType))
		{
			return TRUE;
		}
	}
	return FALSE;
}

UBOOL ULevel::HasPathNodes()
{
	// if this is the editor
	if (GIsEditor)
	{
		// check the actor list, as paths may not be rebuilt
		return HasAnyActorsOfType(ANavigationPoint::StaticClass());
	}
	else
	{
		// otherwise check the nav list pointers
		return (NavListStart != NULL && NavListEnd != NULL);
	}
}

//debug
//pathdebug
#if 0 && !PS3 && !FINAL_RELEASE
#define CHECKNAVLIST(b, x, n) \
		if( !GIsEditor && ##b ) \
		{ \
			debugf(*##x); \
			for (ANavigationPoint *T = GWorld->GetFirstNavigationPoint(); T != NULL; T = T->nextNavigationPoint) \
			{ \
				T->ClearForPathFinding(); \
			} \
			UWorld::VerifyNavList(*##x, ##n); \
		}
#else
#define CHECKNAVLIST(b, x, n)
#endif

void ULevel::AddToNavList( ANavigationPoint *Nav, UBOOL bDebugNavList )
{
	if (Nav != NULL)
	{
		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT("ADD %s to nav list %s"), *Nav->GetFullName(), *GetFullName()), Nav );

		UBOOL bNewList = FALSE;

		// if the list is currently invalid,
		if (NavListStart == NULL || NavListEnd == NULL)
		{
			// set the new nav as the start/end of the list
			NavListStart = Nav;
			NavListEnd = Nav;
			Nav->nextNavigationPoint = NULL;
			bNewList = TRUE;
		}
		else
		{
			// otherwise insert the nav at the end
			ANavigationPoint* Next = NavListEnd->nextNavigationPoint;
			NavListEnd->nextNavigationPoint = Nav;
			NavListEnd = Nav;
			Nav->nextNavigationPoint = Next;
		}
		// add to the cover list as well
		ACoverLink *Link = Cast<ACoverLink>(Nav);
		if (Link != NULL)
		{
			if (CoverListStart == NULL || CoverListEnd == NULL)
			{
				CoverListStart = Link;
				CoverListEnd = Link;
				Link->NextCoverLink = NULL;
			}
			else
			{
				ACoverLink* Next = CoverListEnd->NextCoverLink;
				CoverListEnd->NextCoverLink = Link;
				CoverListEnd = Link;
				Link->NextCoverLink = Next;
			}
		}
		APylon* Pylon = Cast<APylon>(Nav);
		if( Pylon != NULL )
		{
			if( PylonListStart == NULL || PylonListEnd == NULL )
			{
				PylonListStart = Pylon;
				PylonListEnd = Pylon;
				Pylon->NextPylon = NULL;
			}
			else
			{
				APylon* Next = PylonListEnd->NextPylon;
				PylonListEnd->NextPylon = Pylon;
				PylonListEnd = Pylon;
				Pylon->NextPylon = Next;
			}
		}

		if (bNewList && GIsGame)
		{
			GWorld->AddLevelNavList(this,bDebugNavList);
			debugfSuppressed(NAME_DevPath, TEXT(">>>  ADDED %s to world nav list because of %s"), *GetFullName(), *Nav->GetFullName());
		}

		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT(">>> ADDED %s to nav list"), *Nav->GetFullName()), Nav );
	}
}

void ULevel::RemoveFromNavList( ANavigationPoint *Nav, UBOOL bDebugNavList )
{
	if( GIsEditor && !GIsGame )
	{
		// skip if in the editor since this shouldn't be reliably used (build paths only)
		return;
	}

	if (Nav != NULL)
	{
		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT("REMOVE %s from nav list"), *Nav->GetFullName()), Nav );

		AWorldInfo *Info = GWorld->GetWorldInfo();

		// navigation point
		{
			// this is the nav that was pointing to this nav in the linked list
			ANavigationPoint *PrevNav = NULL;

			// remove from the world list
			// first check to see if this is the head of the world nav list
			if (Info->NavigationPointList == Nav)
			{
				// adjust to the next
				Info->NavigationPointList = Nav->nextNavigationPoint;
			}
			else
			{
				// otherwise hunt through the list for it
				for (ANavigationPoint *ChkNav = Info->NavigationPointList; ChkNav != NULL; ChkNav = ChkNav->nextNavigationPoint)
				{
					if (ChkNav->nextNavigationPoint == Nav)
					{
						// remove from the list
						PrevNav = ChkNav;
						ChkNav->nextNavigationPoint = Nav->nextNavigationPoint;
						break;
					}
				}
			}

			// check to see if it was the head of the level list
			if (Nav == NavListStart)
			{
				NavListStart = Nav->nextNavigationPoint;
			}

			// check to see if it was the end of the level list
			if (Nav == NavListEnd)
			{
				// if the previous nav is in this level
				if (PrevNav != NULL &&
					PrevNav->GetLevel() == this)
				{
					// then set the end to that
					NavListEnd = PrevNav;
				}
				// otherwise null the end
				else
				{
					NavListEnd = NULL;
				}
			}
		}

		// update the cover list as well (MIRROR NavList* update!)
		ACoverLink *Link = Cast<ACoverLink>(Nav);
		if (Link != NULL)
		{
			// this is the nav that was pointing to this nav in the linked list
			ACoverLink *PrevLink = NULL;

			// remove from the world list
			// first check to see if this is the head of the world nav list
			if (Info->CoverList == Link)
			{
				// adjust to the next
				Info->CoverList = Link->NextCoverLink;
			}
			else
			{
				// otherwise hunt through the list for it
				for (ACoverLink *ChkLink = Info->CoverList; ChkLink != NULL; ChkLink = ChkLink->NextCoverLink)
				{
					if (ChkLink->NextCoverLink == Link)
					{
						// remove from the list
						PrevLink = ChkLink;
						ChkLink->NextCoverLink = Link->NextCoverLink;
						break;
					}
				}
			}

			// check to see if it was the head of the level list
			if (Link == CoverListStart)
			{
				CoverListStart = Link->NextCoverLink;
			}

			// check to see if it was the end of the level list
			if (Link == CoverListEnd)
			{
				// if the previous nav is in this level
				if (PrevLink != NULL &&
					PrevLink->GetLevel() == this)
				{
					// then set the end to that
					CoverListEnd = PrevLink;
				}
				// otherwise null the end
				else
				{
					CoverListEnd = NULL;
				}
			}
		}

		// update the Pylon list as well (MIRROR NavList* update!)
		APylon *Pylon = Cast<APylon>(Nav);
		if (Pylon != NULL)
		{
			// this is the nav that was pointing to this nav in the linked list
			APylon *PrevPylon = NULL;

			// remove from the world list
			// first check to see if this is the head of the world nav list
			if (Info->PylonList == Pylon)
			{
				// adjust to the next
				Info->PylonList = Pylon->NextPylon;
			}
			else
			{
				// otherwise hunt through the list for it
				for( APylon *Chk = Info->PylonList; Chk != NULL; Chk = Chk->NextPylon )
				{
					if( Chk->NextPylon == Pylon )
					{
						// remove from the list
						PrevPylon = Chk;
						Chk->NextPylon = Pylon->NextPylon;
						break;
					}
				}
			}

			// check to see if it was the head of the level list
			if( Pylon == PylonListStart )
			{
				PylonListStart = Pylon->NextPylon;
			}

			// check to see if it was the end of the level list
			if( Pylon == PylonListEnd )
			{
				// if the previous nav is in this level
				if( PrevPylon != NULL &&
					PrevPylon->GetLevel() == this )
				{
					// then set the end to that
					PylonListEnd = PrevPylon;
				}
				// otherwise null the end
				else
				{
					PylonListEnd = NULL;
				}
			}
		}

		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT(">>> REMOVED %s from nav list"), *Nav->GetFullName()), Nav );
	}
}

#undef CHECKNAVLIST

void ULevel::ResetNavList()
{
	NavListStart = NULL;
	NavListEnd = NULL;
	CoverListStart = NULL;
	CoverListEnd = NULL;
	PylonListStart = NULL;
	PylonListEnd = NULL;
}

/** finds all Material references pointing to the specified material relevant to material parameter modifiers (Matinee tracks, etc)
 * in this level and adds entries to the arrays in the structure
 */
void ULevel::GetMaterialRefs(FMaterialReferenceList& ReferenceInfo, UBOOL bFindPostProcessRefsOnly/*= FALSE*/)
{
	if (!bFindPostProcessRefsOnly)
	{
		for (INT i = 0; i < Actors.Num(); i++)
		{
			AActor* Actor = Actors(i);
			if (Actor != NULL && !Actor->ActorIsPendingKill())
			{
				for (INT j = 0; j < Actor->AllComponents.Num(); j++)
				{
					UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->AllComponents(j));
					if (Primitive != NULL)
					{
						INT Num = Primitive->GetNumElements();
						for (INT k = 0; k < Num; k++)
						{
							UMaterialInterface* Material = Primitive->GetElementMaterial(k);
							// check if the primitive's material is dependent on the target material
							if ( Material != NULL && Material->IsDependent(ReferenceInfo.TargetMaterial) )
							{
								new(ReferenceInfo.AffectedMaterialRefs) FPrimitiveMaterialRef(Primitive, k);
							}
						}
					}
				}
			}
		}
	}

	if (GIsGame)
	{
		for (INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); ++PlayerIndex)
		{
			ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
			if (Player && Player->PlayerPostProcess)
			{
				for (INT EffectIdx = 0; EffectIdx < Player->PlayerPostProcess->Effects.Num(); ++EffectIdx)
				{
					UMaterialEffect* MaterialEffect = Cast<UMaterialEffect>(Player->PlayerPostProcess->Effects(EffectIdx));

					if (MaterialEffect && MaterialEffect->Material)
					{
						UMaterialInterface* Material = MaterialEffect->Material;
						if ( Material == ReferenceInfo.TargetMaterial ||
							( Material != NULL && Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()) &&
							((UMaterialInstanceConstant*)Material)->Parent == ReferenceInfo.TargetMaterial ) )
						{
							new(ReferenceInfo.AffectedPPChainMaterialRefs) FPostProcessMaterialRef(MaterialEffect);
						}
					}
				}
			}
		}
	}
	else if (GIsEditor && !GIsGame)
	{
		UPostProcessChain* WorldPostProcessChain = GEngine->GetWorldPostProcessChain();
		if (WorldPostProcessChain)
		{
			for (INT EffectIdx = 0; EffectIdx < WorldPostProcessChain->Effects.Num(); ++EffectIdx)
			{
				UMaterialEffect* MaterialEffect = Cast<UMaterialEffect>(WorldPostProcessChain->Effects(EffectIdx));

				if (MaterialEffect && MaterialEffect->Material)
				{
					UMaterialInterface* Material = MaterialEffect->Material;
					if ( Material == ReferenceInfo.TargetMaterial ||
						( Material != NULL && Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()) &&
						((UMaterialInstanceConstant*)Material)->Parent == ReferenceInfo.TargetMaterial ) )
					{
						new(ReferenceInfo.AffectedPPChainMaterialRefs) FPostProcessMaterialRef(MaterialEffect);
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	ULineBatchComponent implementation.
-----------------------------------------------------------------------------*/

/** Represents a LineBatchComponent to the scene manager. */
class FLineBatcherSceneProxy : public FPrimitiveSceneProxy
{
 public:
	FLineBatcherSceneProxy(const ULineBatchComponent* InComponent):
		FPrimitiveSceneProxy(InComponent), Lines(InComponent->BatchedLines), Points(InComponent->BatchedPoints)
	{
		ViewRelevance.bDynamicRelevance = TRUE;
		for(INT LineIndex = 0;LineIndex < Lines.Num();LineIndex++)
		{
			const ULineBatchComponent::FLine& Line = Lines(LineIndex);
			ViewRelevance.SetDPG(Line.DepthPriority,TRUE);
		}

		for(INT PointIndex = 0;PointIndex < Points.Num();PointIndex++)
		{
			const ULineBatchComponent::FPoint& Point = Points(PointIndex);
			ViewRelevance.SetDPG(Point.DepthPriority,TRUE);
		}
	}

	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		for (INT i = 0; i < Lines.Num(); i++)
		{
			PDI->DrawLine(Lines(i).Start, Lines(i).End, Lines(i).Color, Lines(i).DepthPriority, Lines(i).Thickness);
		}

		for (INT i = 0; i < Points.Num(); i++)
		{
			PDI->DrawPoint(Points(i).Position, Points(i).Color, Points(i).PointSize, Points(i).DepthPriority);
		}
	}

	/**
	 *  Returns a struct that describes to the renderer when to draw this proxy.
	 *	@param		Scene view to use to determine our relevence.
	 *  @return		View relevance struct
	 */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		return ViewRelevance;
	}
	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize() ); }

 private:
	 TArray<ULineBatchComponent::FLine> Lines;
	 TArray<ULineBatchComponent::FPoint> Points;
	 FPrimitiveViewRelevance ViewRelevance;
};

void ULineBatchComponent::DrawLine(const FVector& Start,const FVector& End,const FLinearColor& Color,BYTE DepthPriority,const FLOAT Thickness)
{
	new(BatchedLines) FLine(Start,End,Color,DefaultLifeTime,Thickness,DepthPriority);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

/** Provide many lines to draw - faster than calling DrawLine many times. */
void ULineBatchComponent::DrawLines(const TArray<FLine>& InLines)
{
	BatchedLines.Append(InLines);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

void ULineBatchComponent::DrawPoint(
	const FVector& Position,
	const FLinearColor& Color,
	FLOAT PointSize,
	BYTE DepthPriority
	)
{
	new(BatchedPoints) FPoint(Position,Color,PointSize,DepthPriority);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

/** Draw a box. */
void ULineBatchComponent::DrawBox(const FBox& Box, const FMatrix& TM, const FColor& Color, BYTE DepthPriorityGroup)
{
	FVector	B[2],P,Q;
	INT ai,aj;
	const FMatrix& L2W = TM;
	B[0]=Box.Min;
	B[1]=Box.Max;

	for( ai=0; ai<2; ai++ ) for( aj=0; aj<2; aj++ )
	{
		P.X=B[ai].X; Q.X=B[ai].X;
		P.Y=B[aj].Y; Q.Y=B[aj].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		new(BatchedLines) FLine(TM.TransformFVector(P), TM.TransformFVector(Q), Color, DefaultLifeTime, 0.0f, DepthPriorityGroup);

		P.Y=B[ai].Y; Q.Y=B[ai].Y;
		P.Z=B[aj].Z; Q.Z=B[aj].Z;
		P.X=B[0].X; Q.X=B[1].X;
		new(BatchedLines) FLine(TM.TransformFVector(P), TM.TransformFVector(Q), Color, DefaultLifeTime, 0.0f, DepthPriorityGroup);

		P.Z=B[ai].Z; Q.Z=B[ai].Z;
		P.X=B[aj].X; Q.X=B[aj].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		new(BatchedLines) FLine(TM.TransformFVector(P), TM.TransformFVector(Q), Color, DefaultLifeTime, 0.0f, DepthPriorityGroup);
	}
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

void ULineBatchComponent::Tick(FLOAT DeltaTime)
{
	// Update the life time of batched lines, removing the lines which have expired.
	for(INT LineIndex = 0;LineIndex < BatchedLines.Num();LineIndex++)
	{
		FLine& Line = BatchedLines(LineIndex);
		if(Line.RemainingLifeTime > 0.0f)
		{
			Line.RemainingLifeTime -= DeltaTime;
			if(Line.RemainingLifeTime <= 0.0f)
			{
				// The line has expired, remove it.
				BatchedLines.Remove(LineIndex--);
			}
		}
	}
}

/**
 * Creates a new scene proxy for the line batcher component.
 * @return	Pointer to the FLineBatcherSceneProxy
 */
FPrimitiveSceneProxy* ULineBatchComponent::CreateSceneProxy()
{
	return new FLineBatcherSceneProxy(this);
}

