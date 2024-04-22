/*=============================================================================
	UnFracturedStaticMesh.cpp: Contains definitions for static meshes whose parts can be hidden.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnFracturedStaticMesh.h"
#include "EngineMeshClasses.h"
#include "EngineParticleClasses.h"
#include "EnginePhysicsClasses.h"
#include "UnFracturedMeshRender.h"
#include "UnPhysicalMaterial.h"
#include "EngineDecalClasses.h"
#include "PrimitiveSceneInfo.h"
#include "UnDecalRenderData.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(UFracturedStaticMesh);
IMPLEMENT_CLASS(UFracturedBaseComponent);
IMPLEMENT_CLASS(UFracturedStaticMeshComponent);
IMPLEMENT_CLASS(UFracturedSkinnedMeshComponent);
IMPLEMENT_CLASS(AFracturedStaticMeshActor);
IMPLEMENT_CLASS(AFracturedStaticMeshPart);
IMPLEMENT_CLASS(AFractureManager);

/*-----------------------------------------------------------------------------
	UFracturedStaticMesh
-----------------------------------------------------------------------------*/

/** 
 * Version used to track when UFracturedStaticMesh's need to be rebuilt.  
 * This does not actually force a rebuild on load unlike STATICMESH_VERSION.
 * Instead, it provides a mechanism to warn content authors when a non-critical FSM rebuild is needed.
 * Do not use this version for native serialization backwards compatibility, instead use VER_LATEST_ENGINE/VER_LATEST_ENGINE_LICENSEE.
 */
const WORD FSMNonCriticalBuildVersion = 3;
const WORD LicenseeFSMNonCriticalBuildVersion = 1;


/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UFracturedStaticMesh::InitializeIntrinsicPropertyValues()
{
	CoreFragmentIndex = -1;
	InteriorElementIndex = -1;
	FragmentDestroyEffectScale = 1.f;
	FragmentHealthScale = 1.f;
	FragmentMinHealth = 0.f;
	FragmentMaxHealth = 100.f;
	ChunkLinVel=150.f;
	ChunkAngVel=4.f;
	ChunkLinHorizontalScale=3.f;
	MinConnectionSupportArea=20.f;
	ExplosionVelScale=1.f;
	bSpawnPhysicsChunks=TRUE;
	NormalChanceOfPhysicsChunk=1.0;
	ExplosionChanceOfPhysicsChunk=1.0f;
	NormalPhysicsChunkScaleMin=1.0f;
	NormalPhysicsChunkScaleMax=1.0f;
	ExplosionPhysicsChunkScaleMin=1.0f;
	ExplosionPhysicsChunkScaleMax=1.0f;
	InfluenceVertexBuffer = NULL;
	bSliceUsingCoreCollision = TRUE;
}


void UFracturedStaticMesh::StaticConstructor()
{
	new(GetClass(),TEXT("FragmentDestroyEffect"),RF_Public)	UObjectProperty(CPP_PROPERTY(FragmentDestroyEffect),TEXT(""),0,UParticleSystem::StaticClass());

	UArrayProperty* B = new(GetClass(),TEXT("FragmentDestroyEffects"),RF_Public)UArrayProperty( CPP_PROPERTY(FragmentDestroyEffects), TEXT(""), CPF_Edit );
	B->Inner = new(B, TEXT("ObjectProperty0"), RF_Public) UObjectProperty( EC_CppProperty,0,TEXT(""),CPF_Edit, UParticleSystem::StaticClass() );

	new(GetClass(),TEXT("FragmentDestroyEffectScale"),RF_Public)UFloatProperty(CPP_PROPERTY(FragmentDestroyEffectScale), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("FragmentHealthScale"),RF_Public)UFloatProperty(CPP_PROPERTY(FragmentHealthScale), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("FragmentMinHealth"),RF_Public)UFloatProperty(CPP_PROPERTY(FragmentMinHealth), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("FragmentMaxHealth"),RF_Public)UFloatProperty(CPP_PROPERTY(FragmentMaxHealth), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("bUniformFragmentHealth"),RF_Public)UBoolProperty(CPP_PROPERTY(bUniformFragmentHealth), TEXT(""),CPF_Edit);

#if WITH_EDITORONLY_DATA
	new(GetClass(),TEXT("SourceCoreMesh"),RF_Public)UObjectProperty(CPP_PROPERTY(SourceCoreMesh),TEXT(""),CPF_Edit|CPF_EditConst|CPF_EditorOnly,UStaticMesh::StaticClass());
#endif // WITH_EDITORONLY_DATA
	new(GetClass(),TEXT("CoreMeshScale"),RF_Public)UFloatProperty(CPP_PROPERTY(CoreMeshScale), TEXT(""), CPF_Edit|CPF_EditConst);
	new(GetClass(),TEXT("bSliceUsingCoreCollision"),RF_Public)UBoolProperty(CPP_PROPERTY(bSliceUsingCoreCollision), TEXT(""), CPF_Edit);

	new(GetClass(),TEXT("ChunkLinVel"),RF_Public)UFloatProperty(CPP_PROPERTY(ChunkLinVel), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("ChunkAngVel"),RF_Public)UFloatProperty(CPP_PROPERTY(ChunkAngVel), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("ChunkLinHorizontalScale"),RF_Public)UFloatProperty(CPP_PROPERTY(ChunkLinHorizontalScale), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("ExplosionVelScale"),RF_Public)UFloatProperty(CPP_PROPERTY(ExplosionVelScale), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("bCompositeChunksExplodeOnImpact"),RF_Public)UBoolProperty(CPP_PROPERTY(bCompositeChunksExplodeOnImpact), TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("bFixIsolatedChunks"),RF_Public)UBoolProperty(CPP_PROPERTY(bFixIsolatedChunks), TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("bAlwaysBreakOffIsolatedIslands"),RF_Public)UBoolProperty(CPP_PROPERTY(bAlwaysBreakOffIsolatedIslands), TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("bSpawnPhysicsChunks"),RF_Public)UBoolProperty(CPP_PROPERTY(bSpawnPhysicsChunks), TEXT(""),CPF_Edit);
	new(GetClass(),TEXT("ChanceOfPhysicsChunk"),RF_Public)UFloatProperty(CPP_PROPERTY(NormalChanceOfPhysicsChunk), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("ExplosionChanceOfPhysicsChunk"),RF_Public)UFloatProperty(CPP_PROPERTY(ExplosionChanceOfPhysicsChunk), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("NormalPhysicsChunkScaleMin"),RF_Public)UFloatProperty(CPP_PROPERTY(NormalPhysicsChunkScaleMin), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("NormalPhysicsChunkScaleMax"),RF_Public)UFloatProperty(CPP_PROPERTY(NormalPhysicsChunkScaleMax), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("ExplosionPhysicsChunkScaleMin"),RF_Public)UFloatProperty(CPP_PROPERTY(ExplosionPhysicsChunkScaleMin), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("ExplosionPhysicsChunkScaleMax"),RF_Public)UFloatProperty(CPP_PROPERTY(ExplosionPhysicsChunkScaleMax), TEXT(""), CPF_Edit);
	new(GetClass(),TEXT("MinConnectionSupportArea"),RF_Public)UFloatProperty(CPP_PROPERTY(MinConnectionSupportArea), TEXT(""), CPF_Edit);

	new(GetClass(),TEXT("DynamicOutsideMaterial"),RF_Public)UObjectProperty(CPP_PROPERTY(DynamicOutsideMaterial),TEXT(""),CPF_Edit,UMaterialInterface::StaticClass());
	new(GetClass(),TEXT("LoseChunkOutsideMaterial"),RF_Public)UObjectProperty(CPP_PROPERTY(LoseChunkOutsideMaterial),TEXT(""),CPF_Edit,UMaterialInterface::StaticClass());
	new(GetClass(),TEXT("OutsideMaterialIndex"),RF_Public)UIntProperty(CPP_PROPERTY(OutsideMaterialIndex), TEXT(""), CPF_Edit);


	// Handle garbage collection. Declaring the UObject property above is not sufficient for intrinsic classes.
	UClass* TheClass = GetClass();
#if WITH_EDITORONLY_DATA
	TheClass->EmitObjectReference( STRUCT_OFFSET( UFracturedStaticMesh, SourceStaticMesh ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UFracturedStaticMesh, SourceCoreMesh ) );
#endif // WITH_EDITORONLY_DATA
	TheClass->EmitObjectReference( STRUCT_OFFSET( UFracturedStaticMesh, DynamicOutsideMaterial ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UFracturedStaticMesh, LoseChunkOutsideMaterial ) );
	
	TheClass->EmitObjectReference( STRUCT_OFFSET( UFracturedStaticMesh, FragmentDestroyEffect ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( UFracturedStaticMesh, FragmentDestroyEffects ) );
}

void UFracturedStaticMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << SourceStaticMesh;
	Ar << Fragments;
	Ar << CoreFragmentIndex;

	if (Ar.Ver() >= VER_FRAGMENT_INTERIOR_INDEX)
	{
		Ar << InteriorElementIndex;
	}
	else if(Ar.IsLoading())
	{
		InteriorElementIndex = -1;
	}

	if(Ar.Ver() >= VER_FRACTURE_CORE_SCALE_OFFSET)
	{
		Ar << CoreMeshScale3D;
		Ar << CoreMeshOffset;
	}
	else if(Ar.IsLoading())
	{
		CoreMeshScale3D = FVector(1,1,1);
		CoreMeshOffset = FVector(0,0,0);
	}

	if(Ar.Ver() >= VER_FRACTURE_CORE_ROTATION_PERCHUNKPHYS)
	{
		Ar << CoreMeshRotation;
	}
	else if(Ar.IsLoading())
	{
		CoreMeshRotation = FRotator(0,0,0);
	}

	if(Ar.Ver() >= VER_FRACTURE_SAVE_PLANEBIAS)
	{
		Ar << PlaneBias;

		// Hack to fix content that got broken and had PlaneBias set to (0,0,0)
		if(Ar.IsLoading() && PlaneBias.IsZero())
		{
			PlaneBias = FVector(1,1,1);
		}
	}
	else if(Ar.IsLoading())
	{
		PlaneBias = FVector(1,1,1);
	}

	if (Ar.Ver() >= VER_FRACTURE_NONCRITICAL_BUILD_VERSION)
	{
		Ar << NonCriticalBuildVersion;
		Ar << LicenseeNonCriticalBuildVersion;
	}
	else if (Ar.IsLoading())
	{
		NonCriticalBuildVersion = 1;
		LicenseeNonCriticalBuildVersion = 1;
	}
}

void UFracturedStaticMesh::PostLoad()
{
	Super::PostLoad();

	if (!GIsEditor || GCookingTarget & UE3::PLATFORM_Stripped)
	{
#if WITH_EDITORONLY_DATA
		//release the reference to the source static mesh when in game or cooking for consoles
		SourceStaticMesh = NULL;
		SourceCoreMesh = NULL;
#endif // WITH_EDITORONLY_DATA

		for (INT i = 0; i < Fragments.Num(); i++)
		{
			Fragments(i).ConvexHull.Reset();
		}
	}

	//ignore all triangles in the interior element for texture streaming, 
	//since they have very irregular texture density usages which break the texture streaming factor heuristic.
	ElementToIgnoreForTexFactor = InteriorElementIndex;

	// Handle transition from FragmentDestroyEffect -> FragmentDestroyEffects
	if(FragmentDestroyEffect && FragmentDestroyEffects.Num() == 0)
	{
		FragmentDestroyEffects.AddItem(FragmentDestroyEffect);
		FragmentDestroyEffect = NULL;
	}
}

void UFracturedStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	NormalPhysicsChunkScaleMin = Clamp<FLOAT>(NormalPhysicsChunkScaleMin, 0.01f, NormalPhysicsChunkScaleMax);
	NormalPhysicsChunkScaleMax = Clamp<FLOAT>(NormalPhysicsChunkScaleMax, NormalPhysicsChunkScaleMin, 100.0f);
	ExplosionPhysicsChunkScaleMin = Clamp<FLOAT>(ExplosionPhysicsChunkScaleMin, 0.01f, ExplosionPhysicsChunkScaleMax);
	ExplosionPhysicsChunkScaleMax = Clamp<FLOAT>(ExplosionPhysicsChunkScaleMax, ExplosionPhysicsChunkScaleMin, 100.0f);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UFracturedStaticMesh::FinishDestroy()
{
	if (InfluenceVertexBuffer)
	{
		delete InfluenceVertexBuffer;
		InfluenceVertexBuffer = NULL;
	}

	Super::FinishDestroy();
}

// Accessors

const TArray<FFragmentInfo>& UFracturedStaticMesh::GetFragments() const
{
	return Fragments;
}

INT UFracturedStaticMesh::GetNumFragments() const
{
	return Fragments.Num();
}

INT UFracturedStaticMesh::GetCoreFragmentIndex() const
{
	return CoreFragmentIndex;
}

/**
 * Returns whether all neighbors of the given fragment are visible.
 */
UBOOL UFracturedStaticMesh::AreAllNeighborFragmentsVisible(INT FragmentIndex, const TArray<BYTE>& VisibleFragments) const
{
	UBOOL bAnyNeighborsHidden = FALSE;
	//check if any of the current fragment's neighbors are hidden
	for (INT NeighborIndex = 0; NeighborIndex < Fragments(FragmentIndex).Neighbours.Num(); NeighborIndex++)
	{
		BYTE NeighborFragmentIndex = Fragments(FragmentIndex).Neighbours(NeighborIndex);
		if(NeighborFragmentIndex != 255)
		{
			checkSlow(NeighborFragmentIndex < Fragments.Num());
			if (VisibleFragments(NeighborFragmentIndex) == 0)
			{
				bAnyNeighborsHidden = TRUE;
				break;
			}
		}
	}
	return !bAnyNeighborsHidden;
}

/** Returns if this fragment is destroyable. */
UBOOL UFracturedStaticMesh::IsFragmentDestroyable(INT FragmentIndex) const
{
	if(FragmentIndex < 0 || FragmentIndex >= Fragments.Num())
	{
		return FALSE;
	}

	return Fragments(FragmentIndex).bCanBeDestroyed;
}

/** Changes if a fragmen is destroyable. */
void UFracturedStaticMesh::SetFragmentDestroyable(INT FragmentIndex, UBOOL bDestroyable)
{
	if(FragmentIndex < 0 || FragmentIndex >= Fragments.Num())
	{
		return;
	}

	Fragments(FragmentIndex).bCanBeDestroyed = bDestroyable;
	MarkPackageDirty(TRUE);
}

/** Returns if this is a supporting 'root' fragment.  */
UBOOL UFracturedStaticMesh::IsRootFragment(INT FragmentIndex) const
{
	if(FragmentIndex < 0 || FragmentIndex >= Fragments.Num())
	{
		return FALSE;
	}

	return Fragments(FragmentIndex).bRootFragment;
}

/** Changes if a fragment is a 'root' fragment. */
void UFracturedStaticMesh::SetIsRootFragment(INT FragmentIndex, UBOOL bRootFragment)
{
	if(FragmentIndex < 0 || FragmentIndex >= Fragments.Num())
	{
		return;
	}

	Fragments(FragmentIndex).bRootFragment = bRootFragment;
	MarkPackageDirty(TRUE);
}

/** Returns if this fragment should never spawn a physics object.  */
UBOOL UFracturedStaticMesh::IsNoPhysFragment(INT FragmentIndex) const
{
	if(FragmentIndex < 0 || FragmentIndex >= Fragments.Num())
	{
		return FALSE;
	}

	return Fragments(FragmentIndex).bNeverSpawnPhysicsChunk;
}

/** Changes if a fragment should never spawn a physics object. */
void UFracturedStaticMesh::SetIsNoPhysFragment(INT FragmentIndex, UBOOL bNoPhysFragment)
{
	if(FragmentIndex < 0 || FragmentIndex >= Fragments.Num())
	{
		return;
	}

	Fragments(FragmentIndex).bNeverSpawnPhysicsChunk = bNoPhysFragment;
	MarkPackageDirty(TRUE);
}

/** Returns bounding box of a particular chunk (graphics verts) in local (component) space. */
FBox UFracturedStaticMesh::GetFragmentBox(INT FragmentIndex) const
{
	if(FragmentIndex >= 0 && FragmentIndex < Fragments.Num())
	{
		return Fragments(FragmentIndex).Bounds.GetBox();
	}

	return FBox(FVector(0,0,0), FVector(0,0,0));
}

/** Returns average exterior normal of a particular chunk. */
FVector UFracturedStaticMesh::GetFragmentAverageExteriorNormal(INT FragmentIndex) const
{
	if(FragmentIndex >= 0 && FragmentIndex < Fragments.Num())
	{
		return Fragments(FragmentIndex).AverageExteriorNormal;
	}

	return FVector(0,0,0);
}

/** 
* This function will remove all of the currently attached decals from the object.  
* Basically, we want to have decals attach to these objects and then on state change (due to damage usually), we will
* just detach them all with the big particle effect that occurs it should not be noticeable.
**/
void AFracturedStaticMeshActor::RemoveDecals( INT IndexToRemoveDecalsFrom )
{
	for( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex )
	{
		UDecalComponent* DecalComp = Cast<UDecalComponent>(Components(ComponentIndex));
		if( DecalComp != NULL )
		{
			if( DecalComp->FracturedStaticMeshComponentIndex == IndexToRemoveDecalsFrom )
			{
				warnf( TEXT( "DETACHING DECAL!!!!!! %d %s"), IndexToRemoveDecalsFrom, *DecalComp->GetFullName() );
				DecalComp->ResetToDefaults();
			}			
		}
	}
}


static void AddTriangleToBox(FBox& Box, const FStaticMeshTriangle& Tri)
{
	Box += Tri.Vertices[0];
	Box += Tri.Vertices[1];
	Box += Tri.Vertices[2];
}

/**
 * Creates a fractured static mesh from raw triangle data.
 * @param Outer - the new object's outer
 * @param Name - name of the object to be created
 * @param Flags - object creation flags
 * @param RawFragments - raw triangles associated with a specific fragment
 * @param BaseLODInfo - the LOD info to use for the new UFracturedStaticMesh's base LOD
 * @param BaseElements - the Elements to use for the new UFracturedStaticMesh's base LOD
 */
UFracturedStaticMesh* UFracturedStaticMesh::CreateFracturedStaticMesh(
	UObject* Outer,
	const TCHAR* Name, 
	EObjectFlags Flags, 
	TArray<FRawFragmentInfo>& RawFragments, 
	const FStaticMeshLODInfo& BaseLODInfo,
	INT NewInteriorElementIndex,
	const TArray<FStaticMeshElement>& BaseElements,
	UFracturedStaticMesh* ExistingFracturedMesh)
{
	check(Outer);

	// needed in order to update existing references since we are creating in place
	FStaticMeshComponentReattachContext ComponentReattachContext(ExistingFracturedMesh);

	UFracturedStaticMesh* NewFracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticConstructObject(UFracturedStaticMesh::StaticClass(), Outer, Name, Flags));

	//create the base LOD
	FStaticMeshRenderData* NewRenderData = new FStaticMeshRenderData();

	//init fragment information array
	NewFracturedStaticMesh->Fragments.AddZeroed(RawFragments.Num());

	TArray<FStaticMeshTriangle> AccumulatedTriangles;
	//accumulate triangles and assign fragment indices
	for (INT FragmentIndex = 0; FragmentIndex < RawFragments.Num(); FragmentIndex++ )
	{	
		FRawFragmentInfo& Frag = RawFragments(FragmentIndex);
		FBox FragmentBox(0);
		for (INT TriangleIndex = 0; TriangleIndex < Frag.RawTriangles.Num(); TriangleIndex++ )
		{	
			//this will ensure that fragments remain contiguous index ranges after the static mesh is built
			Frag.RawTriangles(TriangleIndex).FragmentIndex = FragmentIndex;

			// Update bounding box
			AddTriangleToBox(FragmentBox, Frag.RawTriangles(TriangleIndex));
		}
		AccumulatedTriangles.Append(Frag.RawTriangles);

		// Update fragment info
		NewFracturedStaticMesh->Fragments(FragmentIndex) = FFragmentInfo(Frag.Center, Frag.ConvexHull, Frag.Neighbours, Frag.NeighbourDims, Frag.bCanBeDestroyed, Frag.bRootFragment, Frag.bNeverSpawnPhysicsChunk, Frag.AverageExteriorNormal);
		NewFracturedStaticMesh->Fragments(FragmentIndex).Bounds = FBoxSphereBounds(FragmentBox);
	}

	//copy accumulated triangles into the base LOD's raw triangles
	NewRenderData->RawTriangles.Lock(LOCK_READ_WRITE);
	void* NewTriangleData = NewRenderData->RawTriangles.Realloc(AccumulatedTriangles.Num());
	check(NewRenderData->RawTriangles.GetBulkDataSize() == AccumulatedTriangles.Num() * AccumulatedTriangles.GetTypeSize());
	appMemcpy( NewTriangleData, AccumulatedTriangles.GetData(), NewRenderData->RawTriangles.GetBulkDataSize() );
	NewRenderData->RawTriangles.Unlock();

	//add a base LOD
	NewFracturedStaticMesh->LODModels.AddRawItem(NewRenderData);
	NewFracturedStaticMesh->LODInfo.AddZeroed();
	NewFracturedStaticMesh->LODInfo(0) = BaseLODInfo;

	//copy over elements and set the number of fragments
	for (INT ElementIndex = 0; ElementIndex < BaseElements.Num(); ElementIndex++)
	{
		NewRenderData->Elements.AddItem(FStaticMeshElement(BaseElements(ElementIndex).Material, ElementIndex));
		NewRenderData->Elements(ElementIndex).Fragments.AddZeroed(RawFragments.Num());
	}

	//build the static mesh, generating render resources and collision data
	NewFracturedStaticMesh->Build();

	//verify that the build was successful
	check(NewFracturedStaticMesh->LODModels.Num() > 0);
	check(NewFracturedStaticMesh->LODModels(0).Elements.Num() > 0);
	for (INT ElementIndex = 0; ElementIndex < NewFracturedStaticMesh->LODModels(0).Elements.Num(); ElementIndex++)
	{
		check(NewFracturedStaticMesh->Fragments.Num() == NewFracturedStaticMesh->LODModels(0).Elements(ElementIndex).Fragments.Num());
	}

	NewFracturedStaticMesh->InteriorElementIndex = NewInteriorElementIndex;
	NewFracturedStaticMesh->ElementToIgnoreForTexFactor = NewInteriorElementIndex;

	NewFracturedStaticMesh->MarkPackageDirty();

	return NewFracturedStaticMesh;
}

void UFracturedStaticMesh::InitResources()
{
	UStaticMesh::InitResources();

	if ((appGetPlatformType() & UE3::PLATFORM_WindowsServer) == 0)
	{
		if (!InfluenceVertexBuffer)
		{
			InfluenceVertexBuffer = new FBoneInfluenceVertexBuffer(this);		
		}
		BeginInitResource(InfluenceVertexBuffer);
	}
}

void UFracturedStaticMesh::ReleaseResources()
{
	if (InfluenceVertexBuffer)
	{
		BeginReleaseResource(InfluenceVertexBuffer);
	}

	//Note: using UStaticMesh's ReleaseResourcesFence to track release progress
	UStaticMesh::ReleaseResources();
}


/*-----------------------------------------------------------------------------
	UFracturedBaseComponent
-----------------------------------------------------------------------------*/

/** 
 * Blocks until the component's render resources have been released so that they can safely be modified
 */
void UFracturedBaseComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	ReleaseResources();
}

void UFracturedBaseComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{	
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged )
	{
		if(PropertyThatChanged->GetName()==TEXT("StaticMesh"))
		{
			if (StaticMesh)
			{
				UFracturedStaticMesh* FracturedStaticMesh = Cast<UFracturedStaticMesh>(StaticMesh);
				if (FracturedStaticMesh)
				{
					ResetVisibility();
				}
				else
				{
					//the user tried to set an object that was not a UFracturedStaticMesh
					const FString ErrorMsg = FString::Printf(*LocalizeUnrealEd("Error_FracturedStaticMeshInvalidStaticMesh"));
					appMsgf(AMT_OK, *ErrorMsg);
					StaticMesh = NULL;
				}
			}
		}
	}

	InitResources();
	BeginDeferredReattach();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Signals to the object to begin asynchronously releasing resources */
void UFracturedBaseComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources();
}

/**
 * Check for asynchronous resource cleanup completion
 * @return	TRUE if the rendering resources have been released
 */
UBOOL UFracturedBaseComponent::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.GetNumPendingFences() == 0;
}

/** Returns array of currently visible fragments. */
TArray<BYTE> UFracturedBaseComponent::GetVisibleFragments() const
{
	return VisibleFragments;
}

/** Returns whether the specified fragment is currently visible or not. */
UBOOL UFracturedBaseComponent::IsFragmentVisible(INT FragmentIndex) const
{
	if(FragmentIndex < 0 || FragmentIndex >= VisibleFragments.Num())
	{
		return FALSE;
	}

	return VisibleFragments(FragmentIndex) != 0;
}

/** Get the number of chunks in the assigned fractured mesh. */
INT UFracturedBaseComponent::GetNumFragments() const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		return FracMesh->GetNumFragments();
	}

	return 0;
}

/** Get the number of chunks that are currently visible. */
INT UFracturedBaseComponent::GetNumVisibleFragments() const
{
	INT TotalVisible = 0;
	for(INT i=0; i<VisibleFragments.Num(); i++)
	{
		if(VisibleFragments(i) != 0)
		{
			TotalVisible++;
		}
	}
	return TotalVisible;
}

INT UFracturedBaseComponent::GetNumVisibleTriangles() const
{
	INT NumVisibleTriangles = 0;

	if (StaticMesh && bUseDynamicIndexBuffer)
	{
		//only handling LOD0 for now
		//@todo - handle !bUseDynamicIndexBuffer
		NumVisibleTriangles = ComponentBaseResources->InstanceIndexBuffer.Indices.Num() / 3;
	}

	return NumVisibleTriangles;
}

UBOOL UFracturedBaseComponent::GetInitialVisibilityValue() const
{
	return bInitialVisibilityValue;
}

/**
 * Attaches the component to the scene, and initializes the component's resources if they have not been yet.
 */
void UFracturedBaseComponent::Attach()
{
	if (StaticMesh)
	{
		UFracturedStaticMesh* FracturedStaticMesh = Cast<UFracturedStaticMesh>(StaticMesh);
		check(FracturedStaticMesh); 
		const UINT NumSourceFragments = FracturedStaticMesh->GetNumFragments();

		//This can happen if the fragments in the resource changed since the component was last attached
		//Reset all fragments to visible since there's no way to carry over the previous visibility
		//@todo: need to detect the resource changing and num fragments does not
		if (VisibleFragments.Num() != NumSourceFragments)
		{
			ResetVisibility();
			ReleaseBaseResources();
		}

		//check if we need to switch to a dynamic index buffer
		if (bUseDynamicIBWithHiddenFragments)
		{
			UBOOL bAnyFragmentsHidden = FALSE;
			for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
			{
				if (VisibleFragments(FragmentIndex) == 0)
				{
					bAnyFragmentsHidden = TRUE;
					break;
				}
			}

			if (bAnyFragmentsHidden)
			{
				//at least one fragment is hidden, switch to using a dynamic index buffer
				bUseDynamicIndexBuffer = TRUE;
			}
			else
			{
				//or no fragments are hidden, switch to not using a dynamic index buffer
				bUseDynamicIndexBuffer = FALSE;
				ReleaseBaseResources();
			}
		}

		//ensure that all render resources are initialized before attaching
		InitResources();

		//update the component's index buffer
		UpdateComponentIndexBuffer();

		//no instance index buffers should be created unless bUseDynamicIndexBuffer is true
		check(bUseDynamicIndexBuffer || ComponentBaseResources == NULL);
	}

	Super::Attach();
}

/** Checks if the given fragment is visible. */
UBOOL UFracturedBaseComponent::IsElementFragmentVisible(INT ElementIndex, INT FragmentIndex, INT InteriorElementIndex, INT CoreFragmentIndex, UBOOL bAnyFragmentsHidden) const
{
	return VisibleFragments(FragmentIndex) != 0;
}

/** 
 * Updates the fragments of this component that are visible.  
 * @param NewVisibleFragments - visibility factors for this component, corresponding to FracturedStaticMesh's Fragments array
 * @param bForceUpdate - whether to update this component's resources even if no fragments have changed visibility
 */
void UFracturedBaseComponent::UpdateVisibleFragments(const TArray<BYTE>& NewVisibleFragments, UBOOL bForceUpdate)
{
	if (StaticMesh)
	{
		UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
		//only update resources if visibility has changed or an update is being forced
		if (bForceUpdate || VisibleFragments != NewVisibleFragments)
		{
			//verify assumptions
			check(NewVisibleFragments.Num() == FracturedStaticMesh->GetNumFragments());
			//mark us dirty so that the dynamic index buffer will be rebuilt
			bVisibilityHasChanged = TRUE;
			//update the internal list of visible fragments
			VisibleFragments = NewVisibleFragments;
		}
	}
}

/** 
 * Resets VisibleFragments to bInitialVisibilityValue. 
 * Does not cause a reattach, so the results won't be propagated to the render thread until the next reattach. 
 */
void UFracturedBaseComponent::ResetVisibility()
{
	if (StaticMesh)
	{
		bVisibilityReset = TRUE;
		UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
		check(FracturedStaticMesh); 
		const UINT NumSourceFragments = FracturedStaticMesh->GetNumFragments();
		TArray<BYTE> ResetVisibleFragments(NumSourceFragments);
		appMemset(ResetVisibleFragments.GetData(), bInitialVisibilityValue, NumSourceFragments * ResetVisibleFragments.GetTypeSize());

		if (bInitialVisibilityValue && bUseDynamicIBWithHiddenFragments)
		{
			//visibility was just set to everything visible so disable the dynamic index buffer
			//since we can draw the mesh in the same number of draw calls anyway
			bUseDynamicIndexBuffer = FALSE;
		}

		UpdateVisibleFragments(ResetVisibleFragments, TRUE);
	}
}

/** 
 * Change the StaticMesh used by this instance, and resets VisibleFragments to all be visible if NewMesh is valid.
 * @param NewMesh - StaticMesh to set.  If this is not also a UFracturedStaticMesh, assignment will fail.
 * @return bool - TRUE if assignment succeeded.
 */
UBOOL UFracturedBaseComponent::SetStaticMesh(UStaticMesh* NewMesh, UBOOL bForce)
{
	if (NewMesh != StaticMesh || bForce)
	{
		UFracturedStaticMesh* FracturedStaticMesh = NewMesh ? Cast<UFracturedStaticMesh>(NewMesh) : NULL;
		//only allow changing static mesh if NewMesh is NULL or non-NULL and also a UFracturedStaticMesh
		if ((!NewMesh && !FracturedStaticMesh) || (NewMesh && FracturedStaticMesh))
		{
			// flag static mesh reset so that all decals can be reset as well
			TGuardValue<INT>(bResetStaticMesh,TRUE);

			if (UStaticMeshComponent::SetStaticMesh(NewMesh))
			{
				//if NewMesh is valid, reset all fragments to be visible
				if (NewMesh && FracturedStaticMesh)
				{
					ResetVisibility();
				}
				else
				{
					//a NULL StaticMesh has been set, release resources
					VisibleFragments.Empty();
					ReleaseResources();
				}

				return TRUE;
			}
		}
	}
	return FALSE;
}

/*-----------------------------------------------------------------------------
	FFracturedStaticMeshCollisionDataProvider
-----------------------------------------------------------------------------*/

typedef TkDOPTreeCompact<class FFracturedStaticMeshCollisionDataProvider,WORD> kDOPTreeTypeFractured;

/** 
* This struct provides the interface into the fractured static mesh collision data 
* NB. The MaterialIndex property of the FkDOPCollisionTriangle for a FracturedStaticMesh actually encodes the chunk and material index!
*/
class FFracturedStaticMeshCollisionDataProvider
{
	/** The component this mesh is attached to */
	const UFracturedStaticMeshComponent* Component;
	/** The fractured mesh that is being collided against */
	UFracturedStaticMesh* FracMesh;

	/** Number of fragments */
	INT NumFragments;

	/** Pointer to vertex buffer containing position data. */
	FPositionVertexBuffer* PositionVertexBuffer;

	/** Hide default ctor */
	FFracturedStaticMeshCollisionDataProvider(void)
	{
	}

public:
	/** Sets the component and mesh members */
	FORCEINLINE FFracturedStaticMeshCollisionDataProvider(const UFracturedStaticMeshComponent* InComponent) 
		: Component(InComponent)
		, PositionVertexBuffer(&InComponent->StaticMesh->LODModels(0).PositionVertexBuffer)
	{
		FracMesh = CastChecked<UFracturedStaticMesh>(InComponent->StaticMesh);
		NumFragments = FracMesh->GetNumFragments();
	}

	/** Given an index, returns the position of the vertex */
	FORCEINLINE const FVector& GetVertex(WORD Index) const
	{
		return PositionVertexBuffer->VertexPosition(Index);
	}

	/**
	 * Given an index and a uv channel, returns the uv at the index in the uv channel
	 *
	 * @param Index		The index into the vertex buffer
	 * @param UVChannel	The uv channel to retrieve uv's from
	 */
	FORCEINLINE const FVector2D GetVertexUV( WORD Index, UINT UVChannel ) const
	{
		return FracMesh->LODModels(0).VertexBuffer.GetVertexUV( Index, UVChannel );
	}

	/** Returns the material for a triangle based upon material index */
	FORCEINLINE UMaterialInterface* GetMaterial(WORD MaterialIndex) const
	{
		return Component->GetMaterial( MaterialIndex/NumFragments );
	}

	/** Returns additional information. */
	FORCEINLINE INT GetItemIndex(WORD MaterialIndex) const
	{
		return MaterialIndex % NumFragments;
	}

	/** Returns additional information. */
	FORCEINLINE INT GetElementIndex(WORD MaterialIndex) const
	{
		return MaterialIndex / NumFragments;
	}

	/** If we should test against this material. */ 
	FORCEINLINE UBOOL ShouldCheckMaterial(INT MaterialIndex) const
	{
		INT ChunkIndex = MaterialIndex % NumFragments;
		return Component->IsFragmentVisible(ChunkIndex);
	}

	/** Returns the kDOPTree for this mesh */
	FORCEINLINE const kDOPTreeTypeFractured& GetkDOPTree(void) const
	{
		return (kDOPTreeTypeFractured&)FracMesh->kDOPTree;
	}

	/** Returns the local to world for the component */
	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return Component->LocalToWorld;
	}

	/** Returns the world to local for the component */
	FORCEINLINE const FMatrix GetWorldToLocal(void) const
	{
		return Component->LocalToWorld.Inverse();
	}

	/** Returns the local to world transpose adjoint for the component */
	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return Component->LocalToWorld.TransposeAdjoint();
	}

	/** Returns the determinant for the component */
	FORCEINLINE FLOAT GetDeterminant(void) const
	{
		return Component->LocalToWorldDeterminant;
	}
};


// Specialized physical material collision check for Fractured Static Meshes
// Calculates the physical material that was hit from the physical material mask on the hit material (if it exists)
template<typename KDOP_IDX_TYPE> 
struct TkDOPPhysicalMaterialCheck<class FFracturedStaticMeshCollisionDataProvider, KDOP_IDX_TYPE>
{
	static UPhysicalMaterial* DetermineMaskedPhysicalMaterial(	const class FFracturedStaticMeshCollisionDataProvider& CollDataProvider, 
		const FVector& Intersection,
		const FkDOPCollisionTriangle<KDOP_IDX_TYPE>& CollTri,
		KDOP_IDX_TYPE MaterialIndex )
	{
		// The hit physical material
		UPhysicalMaterial* ReturnMat = NULL;

 #if !DEDICATED_SERVER
		// Get the hit material interface
		UMaterialInterface* HitMaterialInterface = CollDataProvider.GetMaterial(MaterialIndex);
		// Only call from the game thread and if we are in a game session.
		if( IsInGameThread() && GIsGame && HitMaterialInterface && HitMaterialInterface->HasValidPhysicalMaterialMask() )
		{

			// Get the UV channel for the physical material mask
			const INT MaskUVChannel = HitMaterialInterface->GetPhysMaterialMaskUVChannel();

			// The UV channel could be invalid if this material type does not support phys mat masks. 
			if( MaskUVChannel != -1 )
			{
				// Get the vertices on the collided triangle
				const FVector& v1 = CollDataProvider.GetVertex(CollTri.v1);
				const FVector& v2 = CollDataProvider.GetVertex(CollTri.v2);
				const FVector& v3 = CollDataProvider.GetVertex(CollTri.v3);

				// Get the UV's from the hit triangle on the masked UV channel
				const FVector2D& UV1 = CollDataProvider.GetVertexUV( CollTri.v1, MaskUVChannel );
				const FVector2D& UV2 = CollDataProvider.GetVertexUV( CollTri.v2, MaskUVChannel ); 
				const FVector2D& UV3 = CollDataProvider.GetVertexUV( CollTri.v3, MaskUVChannel );

				// Get the barycentric coordinates for the hit point on the triangle to determine the uv at the hit point.
				const FVector Barycentric = ComputeBaryCentric2D( Intersection, v1, v2, v3 );

				// Interpolate between each UV of the triangle using the barycentric coordinates as weights
				FVector2D HitUV = Barycentric.X * UV1 + Barycentric.Y * UV2 + Barycentric.Z * UV3;

				// Get the physical material hit
				ReturnMat = HitMaterialInterface->DetermineMaskedPhysicalMaterialFromUV( HitUV );
			}
		}
#endif

		return ReturnMat;
	}
};

/*-----------------------------------------------------------------------------
	UFracturedStaticMeshComponent
-----------------------------------------------------------------------------*/

/** Change the set of visible fragments. */
void UFracturedStaticMeshComponent::SetVisibleFragments(const TArray<BYTE>& NewVisibleFragments)
{
	if (VisibleFragments != NewVisibleFragments && GSystemSettings.bAllowFracturedDamage)
	{
		if (bUseSkinnedRendering)
		{
			//using skinned rendering, so forward fragment visibility changes to the skinned component, and don't start a reattach
			check(VisibleFragments.Num() == NewVisibleFragments.Num());
			if (SkinnedComponent)
			{
				for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
				{
					if (VisibleFragments(FragmentIndex) != NewVisibleFragments(FragmentIndex))
					{
						SkinnedComponent->SetFragmentVisibility(FragmentIndex, NewVisibleFragments(FragmentIndex));
					}
				}
			}
			UpdateVisibleFragments(NewVisibleFragments, FALSE);
		}
		else
		{
			UpdateVisibleFragments(NewVisibleFragments, FALSE);
			//start a deferred reattach so that the visibility change will be propagated to the render thread
			BeginDeferredReattach();
		}
	}
}

void UFracturedStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//no need to serialize the component index buffers, these will be regenerated on postload
	if (Ar.Ver() < VER_FRAGMENT_INTERIOR_INDEX)
	{
		TIndirectArray<FRawIndexBuffer> Dummy;
		Ar << Dummy;
	}
}

/** If desired, calculate bounds based only on visible graphics geometry. */
void UFracturedStaticMeshComponent::UpdateBounds()
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh && bUseVisibleVertsForBounds)
	{
		// Convert to FBoxSphereBounds and assign.
		Bounds = FBoxSphereBounds(VisibleBox.TransformBy(LocalToWorld));
		Bounds.BoxExtent *= BoundsScale;
		Bounds.SphereRadius *= BoundsScale;
	}
	else
	{
		Super::UpdateBounds();
	}
}

#if WITH_EDITOR
void UFracturedStaticMeshComponent::CheckForErrors()
{
	Super::CheckForErrors();

	if (StaticMesh)
	{
		UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
		if (FracturedStaticMesh->NonCriticalBuildVersion < FSMNonCriticalBuildVersion 
			|| FracturedStaticMesh->LicenseeNonCriticalBuildVersion < LicenseeFSMNonCriticalBuildVersion )
		{
			GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_FracturedSMNeedsReslicing" ), *FracturedStaticMesh->GetPathName() ) ), TEXT( "FracturedSMNeedsReslicing" ) );
		}

		if (LODData.Num() > 0 && LODData(0).LightMap)
		{
			const FLightMap1D* LightMap1D = LODData(0).LightMap->GetLightMap1D();
			if (LightMap1D)
			{
				GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_FracturedCompVertLightMaps" ), *GetDetailedInfo() ) ), TEXT( "FracturedCompVertLightMaps" ) );
			}
		}
	}
}
#endif

/** Allocates an implementation of FStaticLightingMesh that will handle static lighting for this component */
FStaticMeshStaticLightingMesh* UFracturedStaticMeshComponent::AllocateStaticLightingMesh(INT LODIndex, const TArray<ULightComponent*>& InRelevantLights)
{
	return new FFracturedStaticLightingMesh(this,LODIndex,InRelevantLights);
}

/**
 * Attaches the component to the scene, and initializes the component's resources if they have not been yet.
 */
void UFracturedStaticMeshComponent::Attach()
{
	if (StaticMesh)
	{
		UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
		// Calculate the local space bounding box based on which fragments are visible here, so it will not be repeated on every UpdateTransform
		if (FracturedStaticMesh && bUseVisibleVertsForBounds)
		{
			VisibleBox = FBox(0);
			const TArray<FFragmentInfo>& Fragments = FracturedStaticMesh->GetFragments();
			if (VisibleFragments.Num() == Fragments.Num())
			{
				// Iterate over all fragments..
				for(INT i=0; i < VisibleFragments.Num(); i++)
				{
					// ..if visible, add it to total box
					if (VisibleFragments(i) != 0)
					{
						VisibleBox += Fragments(i).Bounds.GetBox();
					}
				}
			}
		}
	}

	Super::Attach();

	// Update top/bottom Z
	UpdateFragmentMinMaxZ();

	// regenerate any missing static mesh elements for attached decals
	if( SceneInfo && SceneInfo->Proxy )
	{
		SceneInfo->Proxy->BuildMissingDecalStaticMeshElements_GameThread();
	}
}

/** 
* Detach the component from the scene and remove its render proxy 
* @param bWillReattach TRUE if the detachment will be followed by an attachment
*/
void UFracturedStaticMeshComponent::Detach( UBOOL bWillReattach )
{
	if( DecalList.Num() > 0 )
	{	
		// detach decals which are interacting with this receiver primitive
		TArray<UDecalComponent*> DecalsToDetach;
		for( INT DecalIdx=0; DecalIdx < DecalList.Num(); ++DecalIdx )
		{
			FDecalInteraction* DecalInteraction = DecalList(DecalIdx);
			if( DecalInteraction && 
				DecalInteraction->Decal )
			{
				if( bWillReattach &&
					DecalInteraction->RenderData &&
					// detach all decals if reseting the static mesh since visibility will be reset as well
					!bResetStaticMesh )
				{
					UBOOL bHasVisibleFragments = FALSE;
					UBOOL bAllFragmentsVisible = TRUE;

					FDecalRenderData* DecalRenderData = DecalInteraction->RenderData;
					for( TSet<INT>::TIterator It(DecalRenderData->FragmentIndices); It; ++It )
					{
						UBOOL bFragmentVisible = IsFragmentVisible(*It);
						if( bFragmentVisible )
						{
							bHasVisibleFragments = TRUE;							
						}
						else
						{							
							bAllFragmentsVisible = FALSE;							
						}						
					}

					if( bHasVisibleFragments )
					{
						if( bAllFragmentsVisible )
						{
							// if all of the fragments for the decal interaction are visible then leave it attached
						}
						else
						{
							// if only some of the fragments for the decal interaction are visible then detach/reattach it
							DecalsToDetach.AddUniqueItem(DecalInteraction->Decal);
							DecalsToReattach.AddUniqueItem(DecalInteraction->Decal);
							DecalRenderData->FragmentIndices.Empty();							
						}
					}
					else
					{
						// if all of the fragments for the decal interaction are hidden then detach it
						DecalsToDetach.AddUniqueItem(DecalInteraction->Decal);
					}
				}
				else
				{
					// remove all decals if the mesh is being detached
					DecalsToDetach.AddUniqueItem(DecalInteraction->Decal);
				}
			}
		}

		// detach desired decals from this primitive
		for( INT DetachIdx=0; DetachIdx < DecalsToDetach.Num(); ++DetachIdx )
		{
			UDecalComponent* DecalToDetach = DecalsToDetach(DetachIdx);
			DecalToDetach->DetachFromReceiver(this);
		}
	}

	Super::Detach(bWillReattach);
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UFracturedStaticMeshComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	Super::GetUsedMaterials( OutMaterials );

	if( LoseChunkOutsideMaterialOverride )
	{
		// Add the override material if one exists
		OutMaterials.AddItem( LoseChunkOutsideMaterialOverride );
	}
	else
	{
		// If no override exists add the meshes outside material
		UFracturedStaticMesh* Mesh = Cast<UFracturedStaticMesh>( StaticMesh );
		if( Mesh )
		{
			OutMaterials.AddItem( Mesh->LoseChunkOutsideMaterial );
		}
	}
}

void UFracturedStaticMeshComponent::UpdateTransform()
{
	Super::UpdateTransform();
	
	if (bUseSkinnedRendering && SkinnedComponent)
	{
		//using skinned rendering, so forward changed transforms to the skinned component
		for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
		{
			if (VisibleFragments(FragmentIndex) != 0)
			{
				SkinnedComponent->SetFragmentTransform(FragmentIndex, LocalToWorld);
			}
		}
	}
}

/** Checks if the given fragment is visible. */
UBOOL UFracturedStaticMeshComponent::IsElementFragmentVisible(INT ElementIndex, INT FragmentIndex, INT InteriorElementIndex, INT CoreFragmentIndex, UBOOL bAnyFragmentsHidden) const
{
	//the fragment-element combination is visible if it is marked visible in VisibleFragments
	const UBOOL bVisible = VisibleFragments(FragmentIndex) != 0;
	//and the fragment is not the core, unless at least one fragment is hidden.
	const UBOOL bVisibleBasedOnCore = FragmentIndex != CoreFragmentIndex || bAnyFragmentsHidden;

	UBOOL bAnyNeighborsHidden = FALSE;
	//only check neighbors for the interior element
	const UBOOL bDependsOnNeighbors = ElementIndex == InteriorElementIndex;
	//get the cached value for whether all neighbors are visible if needed
	if (bDependsOnNeighbors && bVisible && bVisibleBasedOnCore)
	{
		bAnyNeighborsHidden = FragmentNeighborsVisible(FragmentIndex) == 0;
	}

	//visible based on neighbors if at least one of its neighbors are hidden, or it is not in the interior element
	const UBOOL bVisibleBasedOnNeighbors = bAnyNeighborsHidden || !bDependsOnNeighbors;
	return bVisible && bVisibleBasedOnNeighbors && bVisibleBasedOnCore;
}

/** 
 * Updates the fragments of this component that are visible.  
 * @param NewVisibleFragments - visibility factors for this component, corresponding to FracturedStaticMesh's Fragments array
 * @param bForceUpdate - whether to update this component's resources even if no fragments have changed visibility
 */
void UFracturedStaticMeshComponent::UpdateVisibleFragments(const TArray<BYTE>& NewVisibleFragments, UBOOL bForceUpdate)
{
	if (StaticMesh)
	{
		//only update resources if visibility has changed or an update is being forced
		const UBOOL bNeedsUpdate = bForceUpdate || VisibleFragments != NewVisibleFragments;
		Super::UpdateVisibleFragments(NewVisibleFragments, bForceUpdate);

		if (bNeedsUpdate && !bUseSkinnedRendering)
		{
			UBOOL bAnyFragmentsHidden = FALSE;
			//check if any fragments are hidden
			for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
			{
				if (VisibleFragments(FragmentIndex) == 0)
				{
					bAnyFragmentsHidden = TRUE;
					break;
				}
			}

			//resize FragmentNeighborsVisible if necessary
			if (FragmentNeighborsVisible.Num() != VisibleFragments.Num())
			{
				FragmentNeighborsVisible.Empty(VisibleFragments.Num());
				FragmentNeighborsVisible.Add(VisibleFragments.Num());
			}

			if (bAnyFragmentsHidden)
			{
				UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);
				//cache neighbor visibility information
				for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
				{
					FragmentNeighborsVisible(FragmentIndex) = FracturedStaticMesh->AreAllNeighborFragmentsVisible(FragmentIndex, VisibleFragments);
				}
			}
			else
			{
				//no fragments are hidden
				appMemset(FragmentNeighborsVisible.GetData(), 1, FragmentNeighborsVisible.Num() * FragmentNeighborsVisible.GetTypeSize());
			}
		}
	}
}

/** Need our own line check to ignore hidden fragments and set Item. */
UBOOL UFracturedStaticMeshComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	if(!StaticMesh)
	{
		return Super::LineCheck(Result, End, Start, Extent, TraceFlags);
	}

	UBOOL bHit = FALSE;
	UBOOL bZeroExtent = Extent.IsZero();
	UBOOL bWantSimpleCheck = (StaticMesh->UseSimpleBoxCollision && !bZeroExtent) || (StaticMesh->UseSimpleLineCollision && bZeroExtent);

	// For simplified collision, use StaticMeshComponent code.
	if( Owner && 
		bWantSimpleCheck && 
		!(TraceFlags & TRACE_ShadowCast) &&
		!(TraceFlags & TRACE_ComplexCollision) )
	{
		return Super::LineCheck(Result, End, Start, Extent, TraceFlags);
	}
	else if (StaticMesh->kDOPTree.Nodes.Num())
	{
		// Create the object that knows how to extract information from the fractured static mesh info
		FFracturedStaticMeshCollisionDataProvider Provider(this);

		const kDOPTreeTypeFractured* FracTree = (kDOPTreeTypeFractured*)&(StaticMesh->kDOPTree);

		if (bZeroExtent)
		{
			// Create the check structure with all the local space fun
			TkDOPLineCollisionCheck<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured> kDOPCheck(Start,End,TraceFlags,Provider,&Result);
			// Do the line check
			bHit = FracTree->LineCheck(kDOPCheck);
			if (bHit)
			{
				Result.Normal = kDOPCheck.GetHitNormal();
			}
		}
		else
		{
			// Create the check structure with all the local space fun
			TkDOPBoxCollisionCheck<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured> kDOPCheck(Start,End,Extent,TraceFlags,Provider,&Result);
			// And collide against it
			bHit = FracTree->BoxCheck(kDOPCheck);
			if(bHit)
			{
				Result.Normal = kDOPCheck.GetHitNormal();
			}
		}
		// We hit this mesh, so update the common out values
		if (bHit == TRUE)
		{
			Result.Actor = Owner;
			Result.Component = this;
			if (TraceFlags & TRACE_Accurate)
			{
				Result.Time = Clamp(Result.Time,0.0f,1.0f);
			}
			else
			{
				Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
			}
			Result.Location = Start + (End - Start) * Result.Time;		
		}
	}
	return !bHit;
}

/** Need special version of PointCheck to ignore hidden chunks and set Item on result. */
UBOOL UFracturedStaticMeshComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
	if(!StaticMesh)
	{
		return Super::PointCheck(Result,Location,Extent,TraceFlags);
	}

	SCOPE_CYCLE_COUNTER(STAT_StaticMeshPointTime);

	UBOOL bHit = FALSE;
	UBOOL bZeroExtent = Extent.IsZero();
	if (!(TraceFlags & TRACE_ComplexCollision) && ((StaticMesh->UseSimpleBoxCollision && !bZeroExtent) || (StaticMesh->UseSimpleLineCollision && bZeroExtent)))
	{
		return Super::PointCheck(Result,Location,Extent,TraceFlags);
	}
	else if(StaticMesh->kDOPTree.Nodes.Num())
	{ 
		// Create the object that knows how to extract information from the component/mesh
		FFracturedStaticMeshCollisionDataProvider Provider(this);

		const kDOPTreeTypeFractured* FracTree = (kDOPTreeTypeFractured*)&(StaticMesh->kDOPTree);

		// Create the check structure with all the local space fun
		TkDOPPointCollisionCheck<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured> kDOPCheck(Location,Extent,Provider,&Result);
		bHit = FracTree->PointCheck(kDOPCheck);
		if (bHit)
		{
			Result.Normal = kDOPCheck.GetHitNormal();
			Result.Location = kDOPCheck.GetHitLocation();
		}

		if(bHit)
		{
			Result.Normal.Normalize();
			// Now calculate the location of the hit in world space
			Result.Actor = Owner;
			Result.Component = this;
		}
	}

	return !bHit;
}

void UFracturedStaticMeshComponent::CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount)
{
#if 1
	Super::CookPhysConvexDataForScale(Level, TotalScale3D, TriByteCount, TriMeshCount, HullByteCount, HullCount);
#else
	check(StaticMesh);
	check(StaticMesh->BodySetup);
	UFracturedStaticMesh* FracMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);

	// First see if its already in the cache
	FKCachedConvexData* TestData = Level->FindPhysStaticMeshCachedData(StaticMesh, TotalScale3D);

	// If not, cook it and add it.
	if(!TestData)
	{
		TArray<FKConvexElem> ChunkElems;
		const TArray<FFragmentInfo>& FragInfo = FracMesh->GetFragments();
		ChunkElems.AddZeroed(FragInfo.Num());
		for(INT i=0; i<FragInfo.Num(); i++)
		{
			ChunkElems(i) = FragInfo(i).ConvexHull;
		}

		// Create new struct for the cache
		INT NewConvexDataIndex = Level->CachedPhysSMDataStore.AddZeroed();
		FKCachedConvexData* NewConvexData = &Level->CachedPhysSMDataStore(NewConvexDataIndex);

		FCachedPhysSMData NewCachedData;
		NewCachedData.Scale3D = TotalScale3D;
		NewCachedData.CachedDataIndex = NewConvexDataIndex;

		// Cook the collision geometry at the scale its used at in-level.
		FString DebugName = FString::Printf(TEXT("FRAC %s %s"), *GetOwner()->GetName(), *StaticMesh->GetName() );
		MakeCachedConvexDataForAggGeom( NewConvexData, ChunkElems, TotalScale3D, *DebugName );

		// Add to memory used total.
		for(INT HullIdx = 0; HullIdx < NewConvexData->CachedConvexElements.Num(); HullIdx++)
		{
			FKCachedConvexDataElement& Hull = NewConvexData->CachedConvexElements(HullIdx);
			HullByteCount += Hull.ConvexElementData.Num();
			HullCount++;
		}

		// And add to the cache.
		Level->CachedPhysSMDataMap.Add( StaticMesh, NewCachedData );

		//debugf( TEXT("Added FRAC: %d - %s @ [%f %f %f]"), NewConvexDataIndex, *SMComp->StaticMesh->GetName(), TotalScale3D.X, TotalScale3D.Y, TotalScale3D.Z );
	}
#endif
}

/** Returns if this fragment is destroyable. */
UBOOL UFracturedStaticMeshComponent::IsFragmentDestroyable(INT FragmentIndex) const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		if(bTopFragmentsRootNonDestroyable || bBottomFragmentsRootNonDestroyable)
		{
			return !FragmentInstanceIsSupportNonDestroyable(FragmentIndex);
		}
		else
		{
			return FracMesh->IsFragmentDestroyable(FragmentIndex);
		}
	}

	return FALSE;
}

/** Returns if this is a supporting 'root' fragment.  */
UBOOL UFracturedStaticMeshComponent::IsRootFragment(INT FragmentIndex) const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		if(bTopFragmentsRootNonDestroyable || bBottomFragmentsRootNonDestroyable)
		{
			return FragmentInstanceIsSupportNonDestroyable(FragmentIndex);
		}
		else
		{
			return FracMesh->IsRootFragment(FragmentIndex);
		}
	}

	return FALSE;
}

/** Returns if this fragment should never spawn a physics object.  */
UBOOL UFracturedStaticMeshComponent::IsNoPhysFragment(INT FragmentIndex) const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		return FracMesh->IsNoPhysFragment(FragmentIndex);
	}

	return FALSE;
}

/** Get the bounding box of a specific chunk, in world space. */
FBox UFracturedStaticMeshComponent::GetFragmentBox(INT ChunkIndex) const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		return FracMesh->GetFragmentBox(ChunkIndex).TransformBy(LocalToWorld);
	}

	return 	FBox(LocalToWorld.GetOrigin(), LocalToWorld.GetOrigin());
}

/** Returns average exterior normal of a particular chunk. */
FVector UFracturedStaticMeshComponent::GetFragmentAverageExteriorNormal(INT ChunkIndex) const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		FVector LocalNormal = FracMesh->GetFragmentAverageExteriorNormal(ChunkIndex);
		FVector WorldNormal = LocalToWorld.TransposeAdjoint().TransformNormal(LocalNormal);
		// Flip direction if negative scaling
		if(LocalToWorldDeterminant < 0.f)
		{
			WorldNormal *= -1.f;
		}
		return WorldNormal.SafeNormal();
	}

	return FVector(0,0,0);
}

/** Gets the index that is the 'core' of this mesh. */
INT UFracturedStaticMeshComponent::GetCoreFragmentIndex() const
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(FracMesh)
	{
		return FracMesh->GetCoreFragmentIndex();
	}

	return INDEX_NONE;
}

/** Update FragmentBoundsMin/MaxZ */
void UFracturedStaticMeshComponent::UpdateFragmentMinMaxZ()
{
	// Only do this if not movable
	if(GetOwner() && !GetOwner()->bMovable)
	{
		FragmentBoundsMaxZ = -10000000000000.0;
		FragmentBoundsMinZ = 10000000000000.0;

		for(INT i=0; i<GetNumFragments(); i++)
		{
			// Only update for visible chunk
			if(IsFragmentVisible(i))
			{
				const FBox ChunkBox = GetFragmentBox(i);

				FragmentBoundsMaxZ = ::Max<FLOAT>(FragmentBoundsMaxZ, ChunkBox.Max.Z);
				FragmentBoundsMinZ = ::Min<FLOAT>(FragmentBoundsMinZ, ChunkBox.Min.Z);
			}
		}
	}
}

/** See if the bTopFragmentsSupportNonDestroyable/bBottomFragmentsSupportNonDestroyable flags indicate this chunk. */
UBOOL UFracturedStaticMeshComponent::FragmentInstanceIsSupportNonDestroyable(INT FragmentIndex) const
{
	FBox ChunkBox = GetFragmentBox(FragmentIndex);

	// Only do this if not movable
	if(GetOwner() && !GetOwner()->bMovable)
	{
		// See if near top
		if(bTopFragmentsRootNonDestroyable)
		{
			if((FragmentBoundsMaxZ - ChunkBox.Max.Z) < TopBottomFragmentDistThreshold)
			{
				return TRUE;
			}
		}

		// See if near bottom
		if(bBottomFragmentsRootNonDestroyable)
		{
			if((ChunkBox.Min.Z - FragmentBoundsMinZ) < TopBottomFragmentDistThreshold)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/** Worker function that will recursively add fragments reachable from the supplied one. */
static void AddFragmentAndChildrenToGroup(const TArray<FFragmentInfo>& Fragments, TArray<UBOOL>& FragAdded, const TArray<BYTE>& VisibleFragments, FLOAT MinConnectionArea, INT FragIndex, FFragmentGroup& Group, const UFracturedStaticMeshComponent* FracMeshComp)
{
	check(Fragments.Num() == FragAdded.Num());
	check(FragAdded.Num() == VisibleFragments.Num());

	if( VisibleFragments(FragIndex) && !FragAdded(FragIndex) )
	{
		Group.FragmentIndices.AddItem(FragIndex);
		FragAdded(FragIndex) = TRUE;
		
		// Update flag as to whether this group contains a fragment that is a 'root' part
		Group.bGroupIsRooted |= FracMeshComp->IsRootFragment(FragIndex);

		// Now try and add any neighbours.
		for(INT i=0; i<Fragments(FragIndex).Neighbours.Num(); i++)
		{
			BYTE NIndex = Fragments(FragIndex).Neighbours(i);
			FLOAT NArea = Fragments(FragIndex).NeighbourDims(i);
			if(NIndex != 255 && NArea >= MinConnectionArea)
			{
				AddFragmentAndChildrenToGroup(Fragments, FragAdded, VisibleFragments, MinConnectionArea, NIndex, Group, FracMeshComp);
			}
		}
	}
}

/** Based on the hidden state of chunks, groups which are connected.  */
TArray<FFragmentGroup> UFracturedStaticMeshComponent::GetFragmentGroups(const TArray<INT>& IgnoreFragments, FLOAT MinConnectionArea) const
{
	TArray<FFragmentGroup> OutGroups;

	// Check we have a FSM
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(!FracMesh)
	{
		return OutGroups;
	}

	// Init array indicating which fragments have already been added to a group
	TArray<UBOOL> FragAdded;
	FragAdded.AddZeroed(FracMesh->GetNumFragments());

	// Grab fragment data from FSM
	const TArray<FFragmentInfo>& Fragments = FracMesh->GetFragments();
	check(Fragments.Num() == FragAdded.Num());

	// Combine in extra set of fragments to ignore.
	TArray<BYTE> UseVis = VisibleFragments;
	for(INT i=0; i<IgnoreFragments.Num(); i++)
	{
		INT IgnoreFrag = IgnoreFragments(i);
		if(IgnoreFrag >= 0 && IgnoreFrag < UseVis.Num())
		{
			UseVis(IgnoreFrag) = 0;
		}
	}

	// For the purpose of island finding, ignore core as well
	INT CoreFragmentIndex = FracMesh->GetCoreFragmentIndex();
	if(CoreFragmentIndex != INDEX_NONE)
	{
		check(CoreFragmentIndex < UseVis.Num());
		UseVis(CoreFragmentIndex) = 0;
	}

	// Walk through array looking for chunks that have not been added yet
	for(INT i=0; i<Fragments.Num(); i++)
	{
		// Found one not added - create a new group and add everything reachable
		if( UseVis(i) && !FragAdded(i) )
		{
			INT NewGroupIndex = OutGroups.AddZeroed();
			FFragmentGroup& NewGroup = OutGroups(NewGroupIndex);
			AddFragmentAndChildrenToGroup(Fragments, FragAdded, UseVis, MinConnectionArea, i, NewGroup, this);
		}
	}

	return OutGroups;
}


struct FBoundayFragInfo
{
	INT		FragmentIndex;
	FLOAT	ConnectedArea;
};

IMPLEMENT_COMPARE_CONSTREF(FBoundayFragInfo, UnFracturedStaticMesh, { return appCeil(B.ConnectedArea - A.ConnectedArea); } );


/**
*	Return set of fragments that are hidden, but who have at least one visible neighbour.
*	@param AdditionalVisibleFragments	Additional fragments to consider 'visible' when finding fragments. Will not end up in resulting array.
*/
TArray<INT> UFracturedStaticMeshComponent::GetBoundaryHiddenFragments(const TArray<INT>& AdditionalVisibleFragments) const
{
	// Contains a 1 for each boundary hidden fragment, 0 otherwise.
	TArray<FBoundayFragInfo> BoundaryHiddenFragments;
	// Set of indices of boundary hidden fragments
	TArray<INT> OutFragments;

	// Check we have a FSM
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if(!FracMesh)
	{
		return OutFragments;
	}

	// Combine in extra set of fragments to ignore.
	TArray<BYTE> UseVis = VisibleFragments;

	for(INT i=0; i<AdditionalVisibleFragments.Num(); i++)
	{
		INT FragIndex = AdditionalVisibleFragments(i);
		if(FragIndex >= 0 && FragIndex < UseVis.Num())
		{
			UseVis(FragIndex) = 1;
		}
	}

	// Get info about fragments
	const TArray<FFragmentInfo>& Fragments = FracMesh->GetFragments();
	check(UseVis.Num() == Fragments.Num());

	// Allocate array of flags indicating 'boundary hidden fragment'
	BoundaryHiddenFragments.AddZeroed(UseVis.Num());
	for(INT i=0; i<UseVis.Num(); i++)
	{
		BoundaryHiddenFragments(i).FragmentIndex = i;
	}

	// Iterate over all fragments
	for(INT FragIndex=0; FragIndex<UseVis.Num(); FragIndex++)
	{
		// ..for this hidden fragment..
		if(UseVis(FragIndex) == 0)
		{
			// ..see if we have a visible neighbour.
			for(INT i=0; i<Fragments(FragIndex).Neighbours.Num(); i++)
			{
				BYTE NIndex = Fragments(FragIndex).Neighbours(i);
				if(NIndex != 255 && UseVis(NIndex))
				{
					// Add area to connected visible neighbour
					BoundaryHiddenFragments(FragIndex).ConnectedArea += Fragments(FragIndex).NeighbourDims(i);
				}
			}
		}
	}

	Sort<USE_COMPARE_CONSTREF(FBoundayFragInfo,UnFracturedStaticMesh)>(&BoundaryHiddenFragments(0),BoundaryHiddenFragments.Num());

	for(INT i=0; i<BoundaryHiddenFragments.Num(); i++)
	{
		if(BoundaryHiddenFragments(i).ConnectedArea > 0.f)
		{
			OutFragments.AddItem(BoundaryHiddenFragments(i).FragmentIndex);
		}
	}

	return OutFragments;
}

void UFracturedStaticMeshComponent::GenerateDecalRenderData(class FDecalState* Decal, TArray< FDecalRenderData* >& OutDecalRenderDatas) const
{
	SCOPE_CYCLE_COUNTER(STAT_DecalStaticMeshAttachTime);

	OutDecalRenderDatas.Reset();

	// Do nothing if the specified decal doesn't project on static meshes.
	if( !Decal->bProjectOnStaticMeshes ||
		(Decal->bStaticDecal && !Decal->bMovableDecal))
	{
		return;
	}
	
	// Perform a kDOP query to retrieve intersecting leaves.
	const FFracturedStaticMeshCollisionDataProvider MeshData( this );
	TArray<TkDOPFrustumQuery<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured>::FTriangleRun> Runs;
	TkDOPFrustumQuery<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured> kDOPQuery( 
		Decal->Planes.GetData(),
		Decal->Planes.Num(),
		Runs,
		MeshData );	

	const kDOPTreeTypeFractured& kDOPTree = MeshData.GetkDOPTree();
	const UBOOL bHit = kDOPTree.FrustumQuery( kDOPQuery );

	// Early out if there is no overlap.
	if ( !bHit )
	{
		return;
	}

	// Transform decal properties into local space.
	const FDecalLocalSpaceInfoClip DecalInfo( Decal, LocalToWorld, LocalToWorld.Inverse() );
	const FStaticMeshRenderData& StaticMeshRenderData = StaticMesh->LODModels(0);

	// We modify the incoming decal state to set whether or not we want to actually use software clipping
	Decal->bUseSoftwareClip = FALSE;

	// Allocate a FDecalRenderData object.  Use vertex factory from receiver static mesh
	FDecalRenderData* DecalRenderData = new FDecalRenderData( NULL, FALSE, TRUE, &StaticMeshRenderData.VertexFactory );

	const UFracturedStaticMesh* FracturedStaticMesh = CastChecked<UFracturedStaticMesh>(StaticMesh);

	const INT CoreFragmentIndex = FracturedStaticMesh->GetCoreFragmentIndex();
	const INT InteriorElementIndex = FracturedStaticMesh->GetInteriorElementIndex();
	
	// if fragment index is specified check to see if it is the core fragment	
	const UBOOL bOnlyCoreTriangles = Decal->FracturedStaticMeshComponentIndex != -1 && 
		Decal->FracturedStaticMeshComponentIndex == CoreFragmentIndex;	

	// if fragment index is specified check to see if it a non-core fragment
	const UBOOL bOnlyNonCoreTriangles = Decal->FracturedStaticMeshComponentIndex != -1 && 
		Decal->FracturedStaticMeshComponentIndex != CoreFragmentIndex;

	// used for calculating visible elements
	UBOOL bAnyFragmentsHidden = HasHiddenFragments();

	TArray<WORD>& VertexIndices = DecalRenderData->IndexBuffer.Indices;
	for( INT RunIndex = 0 ; RunIndex < Runs.Num() ; ++RunIndex )
	{
		const TkDOPFrustumQuery<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured>::FTriangleRun& Run = Runs(RunIndex);
		const WORD FirstTriangle						= Run.FirstTriangle;
		const WORD LastTriangle							= FirstTriangle + Run.NumTriangles;

		for( WORD TriangleIndex=FirstTriangle; TriangleIndex<LastTriangle; ++TriangleIndex )
		{
			const FkDOPCollisionTriangle<WORD>& Triangle = kDOPTree.Triangles(TriangleIndex);
			const INT FragmentIdx = MeshData.GetItemIndex(Triangle.MaterialIndex);
			const UBOOL bIsCoreFragment = FragmentIdx == CoreFragmentIndex;
			const INT ElementIdx = MeshData.GetElementIndex(Triangle.MaterialIndex);
			const UBOOL bIsInteriorElement = ElementIdx == InteriorElementIndex;

			// If the index is the core fragment then the decal will only project on core triangles.	
			// If the index is a non-core fragment then the decal will only project on non-core triangles.
			const UBOOL bFragmentIsRelevant = 
				(bIsCoreFragment && bOnlyCoreTriangles) || 
				(!bIsCoreFragment && bOnlyNonCoreTriangles && !bIsInteriorElement) || 
				(!bOnlyCoreTriangles && !bOnlyNonCoreTriangles);

			if( bFragmentIsRelevant )
			{
				// check to see if the fragment is already in our visible list or detect visibility
				const UBOOL bHasFragment = DecalRenderData->FragmentIndices.Contains(FragmentIdx);
				// Check the visibility of the fragment
				const UBOOL bIsElementVisible = bHasFragment || 
					IsElementFragmentVisible(ElementIdx,FragmentIdx,InteriorElementIndex,CoreFragmentIndex,bAnyFragmentsHidden);
#if 0
				// make sure that the fragment bounds also intersect the decal frustum since the kDOP only gives a rough set of intersecting triangles
				FConvexVolume DecalFrustumVolume(Decal->Planes);
				FBox FragmentBox = GetFragmentBox(FragmentIdx);
 				const UBOOL bIntersectsFragmentBox = bHasFragment || 
 					(bIsElementVisible && (bIsCoreFragment || DecalFrustumVolume.IntersectBox(FragmentBox.GetCenter(),FragmentBox.GetExtent()))); 
#endif

				if( bIsElementVisible )
				{
					if( !bHasFragment )
					{
						// keep track of the fragments that are visible for this decal
						DecalRenderData->FragmentIndices.Add(FragmentIdx);
						//debugf(TEXT("%s fragment=%d"), *Decal->DecalComponent->GetName(),FragmentIdx);
					}

					// Calculate face direction, used for backface culling.
					const FVector& V1 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v1);
					const FVector& V2 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v2);
					const FVector& V3 = StaticMeshRenderData.PositionVertexBuffer.VertexPosition(Triangle.v3);
					FVector FaceNormal = (V2 - V1 ^ V3 - V1);

					// Normalize direction, if not possible skip triangle as it's zero surface.
					if( FaceNormal.Normalize(KINDA_SMALL_NUMBER) )
					{
						// Calculate angle between look vector and decal face normal.
						const FLOAT Dot = DecalInfo.LocalLookVector | FaceNormal;
						// Determine whether decal is front facing.
						const UBOOL bIsFrontFacing = Decal->bFlipBackfaceDirection ? -Dot > Decal->DecalComponent->BackfaceAngle : Dot > Decal->DecalComponent->BackfaceAngle;

						// Even if backface culling is disabled, reject triangles that view the decal at grazing angles.
						if( bIsFrontFacing || ( Decal->bProjectOnBackfaces && Abs( Dot ) > Decal->DecalComponent->BackfaceAngle ) )
						{
							VertexIndices.AddItem( Triangle.v1 );
							VertexIndices.AddItem( Triangle.v2 );
							VertexIndices.AddItem( Triangle.v3 );
						}
					}
				}
			}
		}
	}
	// Set triangle count.
	DecalRenderData->NumTriangles = VertexIndices.Num()/3;
	// set the blending interval
	DecalRenderData->DecalBlendRange = Decal->DecalComponent->CalcDecalDotProductBlendRange();

	OutDecalRenderDatas.AddItem( DecalRenderData );
}

/** Re-create physics state - needed if hiding parts would change physics collision of the object. */
void UFracturedStaticMeshComponent::RecreatePhysState()
{
	FVector AngVel(0,0,0);
	if (BodyInstance != NULL)
	{
		// Backup angular vel
		if (BodyInstance->IsValidBodyInstance())
		{
			AngVel = BodyInstance->GetUnrealWorldAngularVelocity();
		}

		// Shutdown existing physics
		TermComponentRBPhys(NULL);
	}

	// Recreate physics (should call ModifyNxActorDesc again to create new geom)
	InitComponentRBPhys(Owner == NULL || Owner->Physics != PHYS_RigidBody);
	// Restore ang vel
	SetRBAngularVelocity(AngVel, FALSE);
	// Wake up
	WakeRigidBody();
}


/** Util for getting the PhysicalMaterial applied to this FSMA's FracturedStaticMesh. */
UPhysicalMaterial* UFracturedStaticMeshComponent::GetFracturedMeshPhysMaterial()
{
	FCheckResult Check(0.f);
	Check.Component = this;

	return DetermineCorrectPhysicalMaterial(Check);
}

/*-----------------------------------------------------------------------------
	UFracturedSkinnedMeshComponent
-----------------------------------------------------------------------------*/

/** If desired, calculate bounds based only on visible graphics geometry. */
void UFracturedSkinnedMeshComponent::UpdateBounds()
{
	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(StaticMesh);
	if (FracMesh)
	{
		const TArray<FFragmentInfo>& Fragments = FracMesh->GetFragments();
		FBox VisBox(0);
		check(VisibleFragments.Num() == Fragments.Num());
		for(INT i=0; i<VisibleFragments.Num(); i++)
		{
			if(VisibleFragments(i) != 0)
			{
				FBox LocalBox = Fragments(i).Bounds.GetBox();
#if DQ_SKINNING
				VisBox += LocalBox.TransformBy(FragmentTransforms(i)*LocalToWorld);
#else
				VisBox += LocalBox.TransformBy(FragmentTransforms(i));
#endif
			}
		}

		Bounds = FBoxSphereBounds(VisBox);
	}
	else
	{
		Super::UpdateBounds();
	}
}

/* Sets the visiblity of a single fragment, and starts a deferred reattach if visiblity changed. */
void UFracturedSkinnedMeshComponent::SetFragmentVisibility(INT FragmentIndex, UBOOL bVisibility)
{
	check(FragmentIndex >= 0 && FragmentIndex < VisibleFragments.Num());
	if (VisibleFragments(FragmentIndex) != bVisibility)
	{
		//mark visibility as dirty so that the component index buffer will be rebuilt
		bVisibilityHasChanged = TRUE;
		if (bVisibilityReset && !bInitialVisibilityValue && bVisibility)
		{
			//mark the component as having at least one fragment becoming visible after being reset
			bBecameVisible = TRUE;
		}
		bVisibilityReset = FALSE;
		//start a deferred reattach so that the visibility change will be propagated to the rendering thread
		BeginDeferredReattach();
	}
}

/* Updates the transform of a single fragment. */
void UFracturedSkinnedMeshComponent::SetFragmentTransform(INT FragmentIndex, const FMatrix& InLocalToWorld)
{
	//initialize FragmentTransforms if necessary
	if (VisibleFragments.Num() != FragmentTransforms.Num())
	{
		FragmentTransforms.Empty();
		FragmentTransforms.AddZeroed(VisibleFragments.Num());
	}

	check(FragmentIndex >= 0 && FragmentIndex < VisibleFragments.Num());

#if DQ_SKINNING
	// In DQ skinning, I need hierarchical transform information - 
	// first InLocalToWorld (from Part Actor) has Draw3DScale same as this->LocalToWorld (from FracturedActor)
	// So I remove Scaling from Transform and use RelativeScale - for this Part Actor - 
	// When rendered, LocalToWorld of FracturedActor (SkinnedComponent) will contain LocalToWorld
		
	// First get component space transform
	FMatrix Transform = InLocalToWorld*LocalToWorld.InverseSafe();
#if DO_GUARD_SLOWISH && !FINAL_RELEASE
	// add warning if scale isn't uniform
	if ( Transform.GetScaleVector().IsUniform() == FALSE )
	{
		debugf(NAME_Warning, TEXT("Detects non-uniform scale from fracture transform."));
	}
#endif
	// Copy to BoneTransform
	FragmentTransforms(FragmentIndex) = Transform;
#else
	// When rendered, LocalToWorld of FracturedActor (SkinnedComponent) will contain Identity
	// Allowing this part to contain LocalToWorld information
	FragmentTransforms(FragmentIndex) = InLocalToWorld;
#endif
	//start a deferred update transform so that transform information will be propagated to the rendering thread
	BeginDeferredUpdateTransform();

	// Mark transforms as dirty, since we can't rely on bNeedsUpdateTransform in cases where we're
	// (re)attaching in the same frame (e.g. SpawnPart is called)
	bFragmentTransformsChanged = TRUE;
}

/* Adds a dependent component whose visibility will affect this component's visibility. */
void UFracturedSkinnedMeshComponent::RegisterDependentComponent(UFracturedStaticMeshComponent* InComponent)
{
	check(InComponent);
	DependentComponents.AddItem(InComponent);

	// Force a visibility update for the skinned component because we've added a dependent component
	BeginDeferredReattach();
}

/* Removes a dependent component whose visibility will affect this component's visibility. */
void UFracturedSkinnedMeshComponent::RemoveDependentComponent(UFracturedStaticMeshComponent* InComponent)
{
	check(InComponent);
	INT Index = DependentComponents.FindItemIndex(InComponent);
	check(Index != INDEX_NONE);
	DependentComponents.RemoveSwap(Index);

	//visibility has potentially changed
	bVisibilityHasChanged = TRUE;

	if (DependentComponents.Num() == 0)
	{
		if (Owner && !CastChecked<AFracturedStaticMeshActor>(Owner)->bBreakChunksOnActorTouch)
		{
			//return the owner back to stasis since none of the components need to be ticked anymore
			Owner->SetTickIsDisabled(TRUE);
		}

		//disable the light environment
		//this will remove the lights from the scene and speed up the rendering thread, even though those lights aren't used by anything
		if (LightEnvironment)
		{
			LightEnvironment->SetEnabled(FALSE);
		}

		SetStaticMesh(NULL);

		//toss transforms now that we don't need them anymore
		FragmentTransforms.Empty();
		VisibleFragments.Empty();

		bFragmentTransformsChanged = TRUE;

		//@todo - get rid of any skinned component index memory
	}

	BeginDeferredReattach();
}

/**
 * Attaches the component to the scene, and initializes the component's resources if they have not been yet.
 */
void UFracturedSkinnedMeshComponent::Attach()
{
	if (StaticMesh)
	{
		UFracturedStaticMesh* FracturedStaticMesh = Cast<UFracturedStaticMesh>(StaticMesh);
		check(FracturedStaticMesh); 
		const UINT NumSourceFragments = FracturedStaticMesh->GetNumFragments();

		if (FragmentTransforms.Num() != NumSourceFragments)
		{
			FragmentTransforms.Empty();
			FragmentTransforms.AddZeroed(NumSourceFragments);

			for ( INT I=0; I<FragmentTransforms.Num(); ++I )
			{
				FragmentTransforms(I) = FMatrix::Identity;
			}

			ReleaseSkinResources();

			bFragmentTransformsChanged = TRUE;
		}

		//search through DependentComponents to find which fragments should be visible in this component
		for (INT FragmentIndex = 0; FragmentIndex < VisibleFragments.Num(); FragmentIndex++)
		{
			UBOOL bFragmentIsVisible = FALSE;
			for (INT ComponentIndex = 0; ComponentIndex < DependentComponents.Num(); ComponentIndex++)
			{
				UFracturedStaticMeshComponent* CurrentComponent = DependentComponents(ComponentIndex);
				if (CurrentComponent && CurrentComponent->IsFragmentVisible(FragmentIndex))
				{
					bFragmentIsVisible = TRUE;
					break;
				}
			}
			VisibleFragments(FragmentIndex) = bFragmentIsVisible;
		}

		//find the base component whose bounds we should use for the light environment
		UFracturedStaticMeshComponent* BaseComponent = NULL;
		if (Owner)
		{
			for(INT ComponentIndex = 0;ComponentIndex < Owner->Components.Num();ComponentIndex++)
			{
				UFracturedStaticMeshComponent* Component = Cast<UFracturedStaticMeshComponent>(Owner->Components(ComponentIndex));
				if(Component)
				{
					BaseComponent = Component;
					break;
				}
			}
		}

		//if the component had at least one fragment become visible since being reset, 
		//reset the light environment now that the component is being attached
		//this is necessary because the light environment has bDynamic=FALSE and is only setup correctly when this component is attached
		if (bBecameVisible)
		{
			UDynamicLightEnvironmentComponent* DynamicLightEnv = CastChecked<UDynamicLightEnvironmentComponent>(LightEnvironment);

			if (BaseComponent 
				//BaseComponent->Bounds won't be up to date unless it is already attached
				//@todo - need to handle the case where BaseComponent isn't attached yet
				&& BaseComponent->IsAttached())
			{
				//override the light environment's bounds with the base component's bounds
				DynamicLightEnv->BoundsMethod = DLEB_ManualOverride;
				DynamicLightEnv->OverriddenBounds = BaseComponent->Bounds;
				//setup the light environment to do visibility checks from the edges of the bounds
				//this is necessary to avoid false shadowing due to the BaseComponent being embedded in some other level geometry
				DynamicLightEnv->bTraceFromClosestBoundsPoint = TRUE;
				//make sure the light environment re-captures its environment now that we have changed it
				DynamicLightEnv->ResetEnvironment();
			}

			bBecameVisible = FALSE;
		}

		//setup our materials using the same materials that the base component uses for rendering
		//@todo - what other properties of the base component should be copied over?
		if (BaseComponent && BaseComponent->StaticMesh == StaticMesh)
		{
			Materials.Empty();
			for (INT MaterialIndex = 0; MaterialIndex < StaticMesh->LODModels(0).Elements.Num(); MaterialIndex++)
			{
				Materials.AddItem(BaseComponent->GetMaterial(MaterialIndex));
			}
		}
	}

#if WITH_EDITORONLY_DATA
	// Don't allow cull distance volumes to affect this component
	// This is also enforced by bAllowCullDistanceVolume=FALSE but legacy content will have CachedCullDistance set incorrectly
	CachedMaxDrawDistance = LDCullDistance;
#endif // WITH_EDITORONLY_DATA

	Super::Attach();


	// Make sure bone matrices are up to date
	if( bFragmentTransformsChanged )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SkinnedComponentUpdateDataCommand,
			FFracturedSkinResources*, ComponentSkinResources, ComponentSkinResources,
			TArray<FMatrix>, FragmentTransforms, FragmentTransforms,
			{
				UpdateDynamicBoneData_RenderThread(ComponentSkinResources, FragmentTransforms);
			}
		);

		bFragmentTransformsChanged = FALSE;
	}
}

/*-----------------------------------------------------------------------------
	FFracturedStaticLightingMesh
-----------------------------------------------------------------------------*/


/** Initialization constructor. */
FFracturedStaticLightingMesh::FFracturedStaticLightingMesh(const UFracturedStaticMeshComponent* InPrimitive,INT InLODIndex,const TArray<ULightComponent*>& InRelevantLights):
	FStaticMeshStaticLightingMesh(InPrimitive, InLODIndex, InRelevantLights),
	FracturedMesh(CastChecked<UFracturedStaticMesh>(InPrimitive->StaticMesh)),
	FracturedComponent(InPrimitive)
{
}

UBOOL FFracturedStaticLightingMesh::ShouldCastShadow(ULightComponent* Light,const FStaticLightingMapping* Receiver) const
{
	if (Receiver->Mesh == this)
	{
		// Disable self-shadowing on fractured static meshes.
		// This will reduce the artifacts when a fragment is hidden dynamically.
		return FALSE;
	}
	else
	{
		return FStaticMeshStaticLightingMesh::ShouldCastShadow(Light, Receiver);
	}
}

UBOOL FFracturedStaticLightingMesh::IsUniformShadowCaster() const
{
	return FALSE;
}

FLightRayIntersection FFracturedStaticLightingMesh::IntersectLightRay(const FVector& Start,const FVector& End,UBOOL bFindNearestIntersection) const
{
	// Create the object that knows how to extract information from the component/mesh
	FFracturedStaticMeshCollisionDataProvider Provider(FracturedComponent);

	// Create the check structure with all the local space fun
	FCheckResult Result(1.0f);
	TkDOPLineCollisionCheck<FFracturedStaticMeshCollisionDataProvider,WORD,kDOPTreeTypeFractured> kDOPCheck(Start,End,!bFindNearestIntersection ? TRACE_StopAtAnyHit : 0,Provider,&Result);

	const kDOPTreeTypeFractured* FracTree = (kDOPTreeTypeFractured*)&(FracturedMesh->kDOPTree);

	// Do the line check
	const UBOOL bIntersects = FracTree->LineCheck(kDOPCheck);

	// Setup a vertex to represent the intersection.
	FStaticLightingVertex IntersectionVertex;
	if(bIntersects)
	{
		IntersectionVertex.WorldPosition = Start + (End - Start) * Result.Time;
		IntersectionVertex.WorldTangentZ = kDOPCheck.GetHitNormal();
	}
	else
	{
		IntersectionVertex.WorldPosition.Set(0,0,0);
		IntersectionVertex.WorldTangentZ.Set(0,0,1);
	}
	return FLightRayIntersection(bIntersects,IntersectionVertex);
}

/*-----------------------------------------------------------------------------
	AFracturedStaticMeshActor
-----------------------------------------------------------------------------*/

void AFracturedStaticMeshActor::BreakOffIsolatedIslands(TArray<BYTE>& FragmentVis, const TArray<INT>& IgnoreFrags, FVector ChunkDir, const TArray<class AFracturedStaticMeshPart*>& DisableCollWithPart, UBOOL bWantPhysChunks)
{
	UFracturedStaticMesh* FracMesh = CastChecked<UFracturedStaticMesh>(FracturedStaticMeshComponent->StaticMesh);
	TArray<FFragmentGroup> FragGroups = FracturedStaticMeshComponent->GetFragmentGroups(IgnoreFrags, FracMesh->MinConnectionSupportArea);

	const AWorldInfo* const WorldInfo = GWorld->GetWorldInfo();
	// Find disconnected islands - ignore the part we just hid.
	// Iterate over each group..
	for (INT GroupIdx = 0; GroupIdx < FragGroups.Num(); GroupIdx++)
	{
		FFragmentGroup FragGroup = FragGroups(GroupIdx);
		// If we are a fixed mesh - spawn off all groups which are not rooted
		// If we are dynamic piece, this actor becomes group 0, spawn all other groups as their own actors
		if (!FragGroup.bGroupIsRooted || (Physics == PHYS_RigidBody && GroupIdx > 0))
		{
			if( bWantPhysChunks )
			{
				// .. if not, spawn this group as one whole part.
				FVector ChunkAngVel = 0.25 * VRand() * FracMesh->ChunkAngVel;
				ChunkAngVel.Z *= 0.5f;
				FLOAT PartScale = FracMesh->NormalPhysicsChunkScaleMin + appFrand() * (FracMesh->NormalPhysicsChunkScaleMax - FracMesh->NormalPhysicsChunkScaleMin);

                // this is scary in that it will make islands not all break if there already have been a max amount of pieces
                // spawned this frame.  
                // @todo:  figure out some way to defer the spawning but do the "breaking" of the island
				if( WorldInfo->CanSpawnMoreFracturedChunksThisFrame() == FALSE )
				{
					break;
				}

				// Spawn part- inherit owners velocity
				AFracturedStaticMeshPart* BigPart = SpawnPartMulti(FragGroup.FragmentIndices, (ChunkDir * FracMesh->ChunkLinVel) + Velocity, ChunkAngVel, PartScale, FALSE);
				if (!BigPart)
				{
					continue;
				}
				// Disable collision between big chunk and both the little part that just broke off and the original mesh.
				for (INT i = 0; i < DisableCollWithPart.Num(); i++)
				{
					BigPart->FracturedStaticMeshComponent->DisableRBCollisionWithSMC(DisableCollWithPart(i)->FracturedStaticMeshComponent, TRUE);
				}
				BigPart->FracturedStaticMeshComponent->DisableRBCollisionWithSMC(FracturedStaticMeshComponent, TRUE);


				UBOOL bRBNotify = FALSE;
				// If this is a single piece that wants impact effects, enable notifies
				if(BigPart->PartImpactEffect.Sound && (FragGroup.FragmentIndices.Num() == 1))
				{
					bRBNotify = TRUE;
				}
				// If composite (multiple chunks) - set rigid body impact stuff if desired
				else if (FracMesh->bCompositeChunksExplodeOnImpact && (FragGroup.FragmentIndices.Num() > 1))
				{
					bRBNotify = TRUE;
					BigPart->bCompositeThatExplodesOnImpact = TRUE;
				}

				BigPart->FracturedStaticMeshComponent->SetNotifyRigidBodyCollision(bRBNotify);
			}

			// Set all fragments in this group to be hidden.
			for (INT i = 0; i < FragGroup.FragmentIndices.Num(); i++)
			{
				INT FragIndex = FragGroup.FragmentIndices(i);
				FragmentVis(FragIndex) = 0;
			}
		}
	}
}



/**
  * Gives the actor a chance to spawn chunks that may have been deferred
  *
  * @return	Returns true if there are still any deferred parts left to spawn
  */
UBOOL AFracturedStaticMeshActor::SpawnDeferredParts()
{
	// NOTE: This function is called every Tick by the world info's FractureManager

	if( DeferredPartsToSpawn.Num() > 0 )
	{
		// Keep track of how many parts we spawn
		INT NumPartsSpawned = 0;

		for( INT CurPartIndex = 0; CurPartIndex < DeferredPartsToSpawn.Num(); ++ CurPartIndex )
		{
			const AWorldInfo* const WorldInfo = GWorld->GetWorldInfo();
			// Make sure we haven't already spawned all of our allowed parts this tick
			if( NumPartsSpawned >= MaxPartsToSpawnAtOnce || WorldInfo->CanSpawnMoreFracturedChunksThisFrame() == FALSE )
			{
				break;
			}

			const FDeferredPartToSpawn& CurDeferredPart = DeferredPartsToSpawn( CurPartIndex );

			// NOTE: Particle effects and whatnot have already been fired for these deferred parts, to
			//   help hide that the part was missing for a frame or two

			AFracturedStaticMeshPart* FracPart =
				SpawnPart(
					CurDeferredPart.ChunkIndex,
					CurDeferredPart.InitialVel,
					CurDeferredPart.InitialAngVel,
					CurDeferredPart.RelativeScale,
					CurDeferredPart.bExplosion );

			if( FracPart != NULL )
			{
				FracPart->FracturedStaticMeshComponent->DisableRBCollisionWithSMC( FracturedStaticMeshComponent, TRUE );

				// disallow collisions between all those parts.
				FracPart->FracturedStaticMeshComponent->SetRBCollidesWithChannel( RBCC_FracturedMeshPart, FALSE );

				// NOTE: Unfortunately, parts that are spawned deferred like this don't have a chance to
				//    have their collision again floating island pieces disabled
//				NoCollParts.AddItem(FracPart);
			}

			// We spawned a part!
			++NumPartsSpawned;
		}

		// Remove all of the parts we spawned
		DeferredPartsToSpawn.RemoveSwap( 0, NumPartsSpawned );
	}

	return ( DeferredPartsToSpawn.Num() == 0 );
}

void AFracturedStaticMeshActor::BreakOffPartsInRadius(FVector Origin, FLOAT Radius, FLOAT RBStrength, UBOOL bWantPhysChunksAndParticles)
{
	UFracturedStaticMesh* FracMesh = CastChecked<UFracturedStaticMesh>(FracturedStaticMeshComponent->StaticMesh);
	const AWorldInfo* const WorldInfo = GWorld->GetWorldInfo();

	TArray<BYTE> FragmentVis = FracturedStaticMeshComponent->GetVisibleFragments();
	INT TotalVisible = FracturedStaticMeshComponent->GetNumVisibleFragments();

	// If we are losing the first chunk, change exterior material (if replacement is defined).
	if ((FracMesh->LoseChunkOutsideMaterial != NULL) && (TotalVisible == FragmentVis.Num()))
	{
		eventSetLoseChunkReplacementMaterial();
	}	

	// Keep track of how many parts we spawned so far
	INT NumPartsSpawned = 0;
	UBOOL bAnyPartsWereDeferred = FALSE;

	// Get fracture settings from relevant WorldInfo.
	FWorldFractureSettings FractureSettings = GWorld->GetWorldInfo()->GetWorldFractureSettings();
	
	TArray<INT> IgnoreFrags;
	TArray<AFracturedStaticMeshPart*> NoCollParts;
	// Iterate over visible, non-core, destroyable parts
	for (INT i=0; i<FragmentVis.Num(); i++)
	{
		if( (FragmentVis(i) != 0) && 
			(i != FracturedStaticMeshComponent->GetCoreFragmentIndex()) && 
			FracturedStaticMeshComponent->IsFragmentDestroyable(i) )
		{
			FBox FracBox = FracturedStaticMeshComponent->GetFragmentBox(i);
			FVector FracCenter = 0.5 * (FracBox.Max + FracBox.Min);
			FVector ToFracCenter = FracCenter - Origin;
			FLOAT ChunkDist = ToFracCenter.Size();
			if (ChunkDist < Radius)
			{
				FragmentVis(i) = 0;
				IgnoreFrags.AddItem(i);

				// See if we want to spawn physics chunks, and take into account chance of it happening
				FLOAT PhysChance = FractureSettings.bEnableChanceOfPhysicsChunkOverride ? FractureSettings.ChanceOfPhysicsChunkOverride : FracMesh->ExplosionChanceOfPhysicsChunk;

				if (WorldInfo->MyFractureManager)
				{
					PhysChance *= WorldInfo->MyFractureManager->GetFSMRadialSpawnChanceScale();
				}

				if (bWantPhysChunksAndParticles && FracMesh->bSpawnPhysicsChunks && (appFrand() < PhysChance) && !FracturedStaticMeshComponent->IsNoPhysFragment(i))
				{
					//PartVel = FracturedStaticMeshComponent.GetFragmentAverageExteriorNormal(i);
					FLOAT VelScale = 1.0 - (ChunkDist / Radius); // Reduce vel based on dist from explosion

					// Calculate direction to throw part
					FVector PartVel;
					// If we have a core - throw out from mesh
					if(FracturedStaticMeshComponent->GetCoreFragmentIndex() != INDEX_NONE)
					{
						PartVel = FracturedStaticMeshComponent->GetFragmentAverageExteriorNormal(i);
						PartVel.Z = Max(PartVel.Z, 0.f); // Take out any downwards force
						PartVel.Z /= FracMesh->ChunkLinHorizontalScale; // Reduce Z vel
						PartVel = PartVel.SafeNormal(); // Normalize
						PartVel *= (RBStrength * FracMesh->ExplosionVelScale * VelScale * FractureSettings.FractureExplosionVelScale);
					}
					// If no core - throw away from explosion
					else
					{
						PartVel = (ToFracCenter / ChunkDist) * RBStrength * VelScale * FracMesh->ExplosionVelScale * FractureSettings.FractureExplosionVelScale; // Normalize dir vector and scale
					}

					FLOAT PartScale = FracMesh->ExplosionPhysicsChunkScaleMin + appFrand() * (FracMesh->ExplosionPhysicsChunkScaleMax - FracMesh->ExplosionPhysicsChunkScaleMin);
					FVector PartAngVel = VRand() * FracMesh->ChunkAngVel;

					// Try to avoid spawning too many rigid bodies for this actor in a single tick, since this
					// will often cause hitches.  After we spawn so many parts, we'll defer the rest until later.
					if( NumPartsSpawned >= MaxPartsToSpawnAtOnce || WorldInfo->CanSpawnMoreFracturedChunksThisFrame() == FALSE )
					{
						// Add this part to the list of deferred parts to spawn
						FDeferredPartToSpawn& DeferredPartToSpawn = DeferredPartsToSpawn( DeferredPartsToSpawn.Add() );
						DeferredPartToSpawn.ChunkIndex = i;
						DeferredPartToSpawn.InitialVel = PartVel;
						DeferredPartToSpawn.InitialAngVel = PartAngVel;
						DeferredPartToSpawn.RelativeScale = PartScale;
						DeferredPartToSpawn.bExplosion = TRUE;

						// We deferred the spawn of at least one part, so we'll need to make sure the
						// fracture manager is updated down below.
						bAnyPartsWereDeferred = TRUE;

						// NOTE: We'll go ahead and play particle effects immediately, even for deferred parts,
						//   since it will help to hide the invisible chunk for frame or two its missing.
					}
					else
					{
						// Go ahead and spawn the part
						AFracturedStaticMeshPart* FracPart = SpawnPart(i, PartVel, PartAngVel, PartScale, TRUE);
						if (!FracPart)
						{
							continue;
						}

						FracPart->FracturedStaticMeshComponent->DisableRBCollisionWithSMC(FracturedStaticMeshComponent, TRUE);

						// disallow collisions between all those parts.
						FracPart->FracturedStaticMeshComponent->SetRBCollidesWithChannel(RBCC_FracturedMeshPart, FALSE);

						NoCollParts.AddItem(FracPart);

						// We spawned a part!
						++NumPartsSpawned;
					}

					// If we have a fracture manager and want effects for chunks during radial damage - do that now
					if(WorldInfo->MyFractureManager && WorldInfo->MyFractureManager->bEnableSpawnChunkEffectForRadialDamage)
					{
						UParticleSystem* EffectPSys = NULL;
						// Assign effect if there is one.
						// Look for override first
						if(OverrideFragmentDestroyEffects.Num() > 0)
						{
							// Pick randomly
							EffectPSys = OverrideFragmentDestroyEffects(RandHelper(OverrideFragmentDestroyEffects.Num()));
						}
						// No override array, try the mesh
						else if(FracMesh->FragmentDestroyEffects.Num() > 0)
						{
							EffectPSys = FracMesh->FragmentDestroyEffects(RandHelper(FracMesh->FragmentDestroyEffects.Num()));
						}

						// If we have an effect and a manager - spawn it
						if(EffectPSys != NULL && WorldInfo->MyFractureManager != NULL)
						{
							WorldInfo->MyFractureManager->eventSpawnChunkDestroyEffect(EffectPSys, FracBox, PartVel, FracMesh->FragmentDestroyEffectScale);
						}
					}
				}
			}
		}
	}

	
	// Did we defer the spawn of any broken-off parts?
	if (bAnyPartsWereDeferred && WorldInfo->MyFractureManager)
	{
		// Make sure the FSM manager knows about this actor
		// NOTE: Its possible for the same actor to exist multiple times in this list; no big deal, its faster
		//   this way and has no extra overhead.
		WorldInfo->MyFractureManager->ActorsWithDeferredPartsToSpawn.AddItem( this );
	}


	// Need to look for disconnected parts now - if no core (or 'always break off isolated parts')
	if(FracMesh->bAlwaysBreakOffIsolatedIslands || FracturedStaticMeshComponent->GetCoreFragmentIndex() == INDEX_NONE)
	{
		FVector ApproxExploDir = (FracturedStaticMeshComponent->Bounds.Origin - Origin).SafeNormal();
		BreakOffIsolatedIslands(FragmentVis, IgnoreFrags, ApproxExploDir, NoCollParts, bWantPhysChunksAndParticles);
	}

	// Right at the end, change fragment visibility
	FracturedStaticMeshComponent->SetVisibleFragments(FragmentVis);

	// If we broke a few pieces off - play the sound
	if(ExplosionFractureSound && IgnoreFrags.Num() > 3)
	{
		PlaySound(ExplosionFractureSound, TRUE, TRUE, TRUE, &Origin, TRUE);
	}

	// If this is a physical part - reset physics state, to take notice of new hidden parts.
	if(Physics == PHYS_RigidBody)
	{
		FracturedStaticMeshComponent->RecreatePhysState();
	}
}

/** Unhides all fragments */
void AFracturedStaticMeshActor::ResetVisibility()
{
	if (Cast<AFracturedStaticMeshPart>(this) == NULL)
	{
		check(FracturedStaticMeshComponent);
		const BYTE InitialVisibilityValue = (BYTE)FracturedStaticMeshComponent->GetInitialVisibilityValue();
		TArray<BYTE> NewVisibility;
		const INT NumFragments = FracturedStaticMeshComponent->GetNumFragments();
		NewVisibility.Add(NumFragments);
		for (INT FragmentIndex = 0; FragmentIndex < NumFragments; FragmentIndex++)
		{
			NewVisibility(FragmentIndex) = InitialVisibilityValue;
		}
		FracturedStaticMeshComponent->SetVisibleFragments(NewVisibility);
	}
}

AFracturedStaticMeshPart* AFracturedStaticMeshActor::SpawnPartMulti(const TArray<INT>& ChunkIndices, FVector InitialVel, FVector InitialAngVel, FLOAT RelativeScale, UBOOL bExplosion)
{
	if(!FracturedStaticMeshComponent || !GSystemSettings.bAllowFracturedDamage || !SkinnedComponent)
	{
		return NULL;
	}

	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(FracturedStaticMeshComponent->StaticMesh);
	if(!FracMesh)
	{
		return NULL;
	}

	// Check all ChunkIndices are in range
	for(INT i=0; i<ChunkIndices.Num(); i++)
	{
		INT ChunkIndex = ChunkIndices(i);
		if(ChunkIndex < 0 || ChunkIndex >= FracMesh->GetNumFragments())
		{
			debugf(TEXT("SpawnPartMulti ERROR: ChunkIndex '%d' is out of range ('%s' - '%s')"), ChunkIndex, *GetName(), *FracMesh->GetName());
			return NULL;
		}
	}

	// Check FractureManager present.
	if(!WorldInfo || !WorldInfo->MyFractureManager)
	{
		debugf(TEXT("SpawnPartMulti: Missing WorldInfo or FractureManager."));
		return NULL;
	}

	// Get fracture settings from relevant WorldInfo.
	FWorldFractureSettings FractureSettings = GWorld->GetWorldInfo()->GetWorldFractureSettings();


	// If just one pieces - see if we want to limit its size
	FVector ChunkCenter(0,0,0);
	if(ChunkIndices.Num() == 1)
	{
		// .. see how big the piece is
		FBox ChunkBox = FracturedStaticMeshComponent->GetFragmentBox(ChunkIndices(0));
		FVector ChunkExtent(0,0,0);
		ChunkBox.GetCenterAndExtents(ChunkCenter, ChunkExtent);

		// Get max size based on whether we are an explosion or not
		UBOOL bLimitSize;
		FLOAT MaxSize;
		if(bExplosion)
		{
			bLimitSize = FractureSettings.bLimitExplosionChunkSize;
			MaxSize = FractureSettings.MaxExplosionChunkSize;
		}
		else
		{
			bLimitSize = FractureSettings.bLimitDamageChunkSize;
			MaxSize = FractureSettings.MaxDamageChunkSize;
		}

		MaxSize = Max<FLOAT>(MaxSize, 1.f); // Ensure its not stupid small

		// If we want to limit..
		if(bLimitSize)
		{
			FLOAT MaxDim = ChunkExtent.GetMax();

			if(MaxDim > MaxSize)
			{
				RelativeScale *= (MaxSize/MaxDim);
			}
		}
	}

	FVector SpawnLocation = Location;

	// Offset spawn location so scaled down pieces come from the middle of the hole
	SpawnLocation += ((1.f - RelativeScale) * (ChunkCenter - Location));

	// Spawn new actor
	AFracturedStaticMeshPart* NewPart = WorldInfo->MyFractureManager->eventSpawnPartActor(this, SpawnLocation, Rotation);

	if(!NewPart || !SkinnedComponent)
	{
		return NULL;
	}

	GWorld->GetWorldInfo()->NumFacturedChunksSpawnedThisFrame++;

	//Note: any properties not belonging to the newly spawned actor that are initialized here 
	//need to be disabled when all the parts have been recycled, in UFracturedSkinnedMeshComponent::RemoveDependentComponent().
	check(SkinnedComponent);
	check(NewPart->FracturedStaticMeshComponent);
	if (SkinnedComponent->LightEnvironment)
	{
		SkinnedComponent->LightEnvironment->SetEnabled(TRUE);
	}
	NewPart->SkinnedComponent = SkinnedComponent;
	NewPart->FracturedStaticMeshComponent->SkinnedComponent = SkinnedComponent;
	AFracturedStaticMeshPart* ThisPart = Cast<AFracturedStaticMeshPart>(this);
	//if we are spawning parts off of an existing part, pass on the reference to the base AFracturedStaticMeshActor
	if (ThisPart)
	{
		check(ThisPart->BaseFracturedMeshActor);
		NewPart->BaseFracturedMeshActor = ThisPart->BaseFracturedMeshActor;
	}
	else
	{
		//otherwise we are spawning parts off of the base
		NewPart->BaseFracturedMeshActor = this;
		//bring the base out of stasis so that it's components will get ticked (esp the light environment)
		SetTickIsDisabled(FALSE);
	}
	SkinnedComponent->SetStaticMesh(FracMesh);
	SkinnedComponent->RegisterDependentComponent(NewPart->FracturedStaticMeshComponent);

	NewPart->FracturedStaticMeshComponent->TermComponentRBPhys(NULL);

	NewPart->FracturedStaticMeshComponent->bDisableAllRigidBody = TRUE;
	NewPart->setPhysics(PHYS_RigidBody);
#if DQ_SKINNING
	if (DrawScale3D.IsUniform() == FALSE)
	{
		// DQ skinning does not support non uniform scaling due to BoneAtom does not have 3d scale
		debugf(NAME_Warning, TEXT("Fracutre: Detected non uniform scaling for %s. Physics and rendering won't match."), *NewPart->BaseFracturedMeshActor->GetName());
	}
#endif
	NewPart->SetDrawScale3D(DrawScale * DrawScale3D * RelativeScale);
	NewPart->FracturedStaticMeshComponent->SetStaticMesh(FracMesh);
	NewPart->FracturedStaticMeshComponent->bDisableAllRigidBody = FALSE;

	// Grab materials array from base mesh
	NewPart->FracturedStaticMeshComponent->Materials = FracturedStaticMeshComponent->Materials;
	// Grab phys material override from base mesh
	NewPart->FracturedStaticMeshComponent->PhysMaterialOverride = FracturedStaticMeshComponent->PhysMaterialOverride;
	// Grab impact sounds from base mesh
	NewPart->PartImpactEffect = PartImpactEffect;
	NewPart->ExplosionFractureSound = ExplosionFractureSound;
	NewPart->SingleChunkFractureSound = SingleChunkFractureSound;

	NewPart->FracturedStaticMeshComponent->BeginDeferredReattach();

	// If set, override material for dynamic parts.
	if(FracMesh->DynamicOutsideMaterial)
	{
		NewPart->FracturedStaticMeshComponent->SetMaterial(FracMesh->OutsideMaterialIndex, FracMesh->DynamicOutsideMaterial);
	}

	// Set all desired parts to be visible.
	TArray<BYTE> VisChunks;
	VisChunks.AddZeroed( FracMesh->GetNumFragments() );
	for(INT i=0; i<ChunkIndices.Num(); i++)
	{
		INT ChunkIndex = ChunkIndices(i);
		VisChunks(ChunkIndex) = 1;
	}
	NewPart->FracturedStaticMeshComponent->SetVisibleFragments(VisChunks);

	NewPart->FracturedStaticMeshComponent->ConditionalUpdateTransform(NewPart->LocalToWorld());
	NewPart->FracturedStaticMeshComponent->InitComponentRBPhys(FALSE);

	// If this is a single chunk 
	if(ChunkIndices.Num() == 1)
	{
		// ..and has an impact sound to play, enable physics collision events
		if(PartImpactEffect.Sound)
		{
			NewPart->FracturedStaticMeshComponent->SetNotifyRigidBodyCollision(TRUE);
		}
		// only show warning once.
		else if(!bHasShownMissingSoundWarning)
		{
			debugf(TEXT("FracturedMesActor: NO SOUND for %s"), *GetName() );
			bHasShownMissingSoundWarning = TRUE;
		}

		// Play fracture-off sound if present.
		if(SingleChunkFractureSound && !bExplosion)
		{
			PlaySound(SingleChunkFractureSound, TRUE, TRUE, TRUE, &ChunkCenter, TRUE);
		}
	}

	NewPart->FracturedStaticMeshComponent->WakeRigidBody();
	NewPart->FracturedStaticMeshComponent->SetRBLinearVelocity(InitialVel);
	NewPart->FracturedStaticMeshComponent->SetRBAngularVelocity(InitialAngVel);

	// Propagate bBreakChunksOnActorTouch option if this is a composite chunk
	if(bBreakChunksOnActorTouch && ChunkIndices.Num() > 1)
	{
		NewPart->bBreakChunksOnActorTouch = TRUE;
	}

	// Init health for this new part.
	NewPart->ResetHealth();

	return NewPart;
}



/** Spawn one chunk of this mesh as its own Actor, with the supplied velocities. */
AFracturedStaticMeshPart* AFracturedStaticMeshActor::SpawnPart(INT ChunkIndex, FVector InitialVel, FVector InitialAngVel, FLOAT RelativeScale, UBOOL bExplosion)
{
	// Just call multi version
	TArray<INT> DummyChunks;
	DummyChunks.AddItem(ChunkIndex);

	return SpawnPartMulti(DummyChunks, InitialVel, InitialAngVel, RelativeScale, bExplosion);
}

/** Used to init/reset health array. */
void AFracturedStaticMeshActor::ResetHealth()
{
	check(FracturedStaticMeshComponent);

	ChunkHealth.Empty();

	UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(FracturedStaticMeshComponent->StaticMesh);
	if(!FracMesh && !bMovable) // bMovable check stops us generating warning for pools of parts
	{
		debugf(TEXT("FSMA::ResetHealth : No FSM! (%s)"), *GetPathName());
		return;
	}

	ChunkHealth.AddZeroed(FracturedStaticMeshComponent->GetNumFragments());

	for(INT i=0; i<ChunkHealth.Num(); i++)
	{
		// Only calc health for visible chunk
		if(FracturedStaticMeshComponent->IsFragmentVisible(i))
		{
			if(FracMesh->bUniformFragmentHealth)
			{
				ChunkHealth(i) = appTrunc(Clamp(ChunkHealthScale * 1.f, FracMesh->FragmentMinHealth, FracMesh->FragmentMaxHealth));
			}
			else
			{
				// Get extent of chunk bounding box
				const FBox ChunkBox = FracturedStaticMeshComponent->GetFragmentBox(i);
				const FVector Extent = ChunkBox.Max - ChunkBox.Min;

				// Find largest face area
				FLOAT MaxArea = Extent.X * Extent.Y;
				MaxArea = Max(MaxArea, Extent.X * Extent.Z);
				MaxArea = Max(MaxArea, Extent.Y * Extent.Z);

				ChunkHealth(i) = appTrunc(Clamp(ChunkHealthScale * FracMesh->FragmentHealthScale * MaxArea * 0.001f, FracMesh->FragmentMinHealth, FracMesh->FragmentMaxHealth));
			}
		}
	}
}

void AFracturedStaticMeshActor::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);

	// If desired, look for touching pawns, and knock out pieces that overlap them
	if(bBreakChunksOnActorTouch)
	{
		TArray<BYTE> FragmentVis;
		TArray<INT> IgnoreFrags;
		TArray<AFracturedStaticMeshPart*> NoCollParts;
		FVector PawnVel(0,0,0);

		UFracturedStaticMesh* FracMesh = Cast<UFracturedStaticMesh>(FracturedStaticMeshComponent->StaticMesh);
		if(!FracMesh)
		{
			return;
		}

		// ITerate over each touching actor
		UBOOL bPieceRemoved = FALSE;
		for(INT TouchIdx=0; TouchIdx<Touching.Num(); TouchIdx++)
		{
			if(Touching(TouchIdx))
			{
				AActor* A = Touching(TouchIdx);
				if(A && A->CanCauseFractureOnTouch() && A->CollisionComponent)
				{
					// Don't get vis info until we need it.
					if(FragmentVis.Num() == 0)
					{
						FragmentVis = FracturedStaticMeshComponent->GetVisibleFragments();
					}

					for(INT ChunkIdx=0; ChunkIdx<FragmentVis.Num(); ChunkIdx++)
					{
						if((FragmentVis(ChunkIdx) != 0) && (ChunkIdx != FracturedStaticMeshComponent->GetCoreFragmentIndex()))
						{
							FBox FracBox = FracturedStaticMeshComponent->GetFragmentBox(ChunkIdx);
							FCheckResult Result(1.f);
							FVector FracCenter = FracBox.GetCenter();

							UBOOL bHit = !A->CollisionComponent->PointCheck(Result, FracCenter, FracBox.GetExtent(), TRACE_Pawns);
							if(bHit)
							{
								FVector AngAxis = -1.f * ((FracCenter - A->Location) ^ (A->Velocity)).SafeNormal();

								FVector PartVel = (1.f + (appFrand() * 1.f)) * A->Velocity; // Between 1 and 2 times pawn velocity
								FLOAT PartScale = FracMesh->NormalPhysicsChunkScaleMin + appFrand() * (FracMesh->NormalPhysicsChunkScaleMax - FracMesh->NormalPhysicsChunkScaleMin);
								AFracturedStaticMeshPart* FracPart = SpawnPart(ChunkIdx, PartVel, AngAxis * FracMesh->ChunkAngVel, PartScale, FALSE);
								if (!FracPart)
								{
									continue;
								}
								FracPart->FracturedStaticMeshComponent->DisableRBCollisionWithSMC(FracturedStaticMeshComponent, TRUE);

								bPieceRemoved = TRUE;

								// Hide part, and remember for island-breaking
								FragmentVis(ChunkIdx) = 0; 
								NoCollParts.AddItem(FracPart);
								IgnoreFrags.AddItem(ChunkIdx);

								// If we have a fracture manager
								if(WorldInfo->MyFractureManager)
								{
									UParticleSystem* EffectPSys = NULL;
									// Assign effect if there is one.
									// Look for override first
									if(OverrideFragmentDestroyEffects.Num() > 0)
									{
										// Pick randomly
										EffectPSys = OverrideFragmentDestroyEffects(RandHelper(OverrideFragmentDestroyEffects.Num()));
									}
									// No override array, try the mesh
									else if(FracMesh->FragmentDestroyEffects.Num() > 0)
									{
										EffectPSys = FracMesh->FragmentDestroyEffects(RandHelper(FracMesh->FragmentDestroyEffects.Num()));
									}

									// If we have an effect and a manager - spawn it
									if(EffectPSys != NULL && WorldInfo->MyFractureManager != NULL)
									{
										WorldInfo->MyFractureManager->eventSpawnChunkDestroyEffect(EffectPSys, FracBox, PartVel, FracMesh->FragmentDestroyEffectScale);
									}
								}
							}
						}
					}

					PawnVel += A->Velocity;
				}
			}
		}

		// If we removed a piece, check for isolated parts
		if(bPieceRemoved)
		{
			check(FragmentVis.Num() > 0); // Should always have this here

			// Need to look for disconnected parts now - if no core.
			if(FracturedStaticMeshComponent->GetCoreFragmentIndex() == INDEX_NONE)
			{
				const UBOOL bWantPhysChunks = TRUE;
				eventBreakOffIsolatedIslands(FragmentVis, IgnoreFrags, PawnVel.SafeNormal(), NoCollParts, bWantPhysChunks);
			}

			// Right at the end, change fragment visibility
			FracturedStaticMeshComponent->SetVisibleFragments(FragmentVis);

			// If this is a physical part - reset physics state, to take notice of new hidden parts.
			if(Physics == PHYS_RigidBody)
			{
				FracturedStaticMeshComponent->RecreatePhysState();
			}
		}
	}
}

UBOOL AFracturedStaticMeshActor::InStasis()
{
	return !bBreakChunksOnActorTouch;
}

void AFracturedStaticMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	// For FSMAs, turn off bWorldGeometry automatically when you want touch notifies
	if (PropertyThatChanged != NULL && PropertyThatChanged->GetFName() == FName(TEXT("CollisionType")))
	{
		if(	CollisionType == COLLIDE_TouchAll || 
			CollisionType == COLLIDE_TouchWeapons || 
			CollisionType == COLLIDE_TouchAllButWeapons )
		{
			bWorldGeometry = FALSE;
		}
		else
		{
			bWorldGeometry = TRUE;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/*-----------------------------------------------------------------------------
	AFracturedStaticMeshPart
-----------------------------------------------------------------------------*/

void AFracturedStaticMeshPart::Initialize()
{
	SetTickIsDisabled(FALSE);
	bHasBeenRecycled = FALSE;
	SetHidden(FALSE);
	FracturedStaticMeshComponent->SetHiddenGame(FALSE);
	setPhysics(PHYS_RigidBody);
	SetCollision(TRUE, FALSE, FALSE);
	CurrentVibrationLevel = 0.f;
}

void AFracturedStaticMeshPart::RecyclePart(UBOOL bAddToFreePool)
{
	if (bHasBeenRecycled)
	{
		return;
	}

	bHasBeenRecycled = TRUE;

	// Set flags to default when we put back in pool
	bCompositeThatExplodesOnImpact = FALSE;

	SetHidden(TRUE);
	if (FracturedStaticMeshComponent)
	{
		FracturedStaticMeshComponent->SetHiddenGame(TRUE);
		if (SkinnedComponent)
		{
			//hide this part's fragments rendered through the SkinnedComponent
			SkinnedComponent->RemoveDependentComponent(FracturedStaticMeshComponent);
		}
		FracturedStaticMeshComponent->SetStaticMesh(NULL);

		// Empty Material Array
		FracturedStaticMeshComponent->Materials.Empty();
		// Clear the physical material override
		FracturedStaticMeshComponent->PhysMaterialOverride = NULL;
	}

	// Clear references to other UObjects
	PartImpactEffect.Effect = NULL;
	PartImpactEffect.Sound = NULL;
	ExplosionFractureSound = NULL;
	SingleChunkFractureSound = NULL;

	setPhysics(PHYS_None);
	SetCollision(FALSE, FALSE, FALSE);	
	static FName CleanUpTimerName = FName(TEXT("TryToCleanUp"));
	ClearTimer(CleanUpTimerName);

	// Inform the fracture manager this part has returned.
	if(bAddToFreePool && GWorld->GetWorldInfo() && GWorld->GetWorldInfo()->MyFractureManager)
	{
		GWorld->GetWorldInfo()->MyFractureManager->eventReturnPartActor(this);
	}

	//remove UObject references
	SkinnedComponent = NULL;
	BaseFracturedMeshActor = NULL;

	// this is to stop the ACs which this Actors owns.  We need to do this as we put this actor in bStasis=TRUE
	TArray<UAudioComponent*> ACsToStop;
	for( INT CompIdx = 0; CompIdx < AllComponents.Num(); ++CompIdx )
	{
		UAudioComponent* AC = Cast<UAudioComponent>(AllComponents(CompIdx));
		if( AC != NULL )
		{
			// copy them out as ->Stop() will modify the AllComponents array
			ACsToStop.AddItem( AC );
		}
	}
	
	for( INT CompIdx = 0; CompIdx < ACsToStop.Num(); ++CompIdx )
	{
		UAudioComponent* AC = ACsToStop(CompIdx);
		//warnf( TEXT( "RecyclePart Has cue still in it!!: %s %d" ), *AC->SoundCue->GetName(), AC->bWasPlaying );
		// there is some crazy bug where sounds are not starting or are starting but not finishing correctly
		// this ensures that the delegate is called and that is the function that resets the properties and DetachFromAny()
		// without that ocurring we end up leaking 100s of AudioComponents which have SoundCues referenced
		AC->Stop();
		AC->delegateOnAudioFinished( AC );
	}

	SetTickIsDisabled(TRUE);
}

void AFracturedStaticMeshPart::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);

	if (BaseFracturedMeshActor)
	{
		check(BaseFracturedMeshActor->FracturedStaticMeshComponent);
		const FLOAT DistSq = (Location - BaseFracturedMeshActor->FracturedStaticMeshComponent->Bounds.Origin).SizeSquared();
		const FLOAT CompareDistance = DestroyPartRadiusFactor * BaseFracturedMeshActor->FracturedStaticMeshComponent->Bounds.SphereRadius;
		if (CompareDistance > 0.0f && DistSq > CompareDistance * CompareDistance)
		{
			 RecyclePart(TRUE);
		}
	}

	// If desired, look to see if part is 'vibrating' and destroy
	AFractureManager* FracManager = GWorld->GetWorldInfo()->MyFractureManager;
	if(FracManager && FracManager->bEnableAntiVibration)
	{
		// See if we have changed angular direction
		FLOAT VelChange = AngularVelocity | OldVelocity;
		OldVelocity = AngularVelocity;

		// If we have, increase vibration level
		if(VelChange < -KINDA_SMALL_NUMBER)
		{
			CurrentVibrationLevel += 1.f;
		}
		// If we haven't, decrease towards zero
		else
		{
			CurrentVibrationLevel = Max((CurrentVibrationLevel - 0.25f), 0.f);
		}

#if 0
		GWorld->LineBatcher->DrawBox(FracturedStaticMeshComponent->Bounds.GetBox(), FMatrix::Identity, Lerp(FLinearColor(0,0,0), FLinearColor(1,0,0), (CurrentVibrationLevel/FracManager->DestroyVibrationLevel)), SDPG_World);

		// If we are vibrating too much, destroy.
		if(CurrentVibrationLevel > FracManager->DestroyVibrationLevel && (AngularVelocity.SizeSquared() <= Square(FracManager->DestroyMinAngVel)))
		{
			debugf(TEXT("TOO SLOW: %f"), AngularVelocity.Size());
		}
#endif

		if((CurrentVibrationLevel > FracManager->DestroyVibrationLevel) && (AngularVelocity.SizeSquared() > Square(FracManager->DestroyMinAngVel)))
		{
			//debugf(TEXT("F: %f %f"), CurrentVibrationLevel, AngularVelocity.Size());
			RecyclePart(TRUE);
		}
	}
	


	// If desired, change RBChannel when it goes to sleep
	if(bChangeRBChannelWhenAsleep)
	{
		if(!(FracturedStaticMeshComponent && FracturedStaticMeshComponent->RigidBodyIsAwake()))
		{
			FracturedStaticMeshComponent->SetRBChannel((ERBCollisionChannel)AsleepRBChannel);
		}
	}
}

FLOAT AFracturedStaticMeshPart::GetGravityZ()
{
	return Super::GetGravityZ() * FracPartGravScale;
}

void AFracturedStaticMeshPart::OnRigidBodyCollision(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData)
{
	// If its a composite part- explode on impact
	if(bCompositeThatExplodesOnImpact)
	{
		eventExplode();
		// Get no more notifies.
		//FracturedStaticMeshComponent->SetNotifyRigidBodyCollision(FALSE);
	}
	// If not - see if we want to play a sound
	else
	{
		// If using physics and has a sound assigned
		if( Physics == PHYS_RigidBody && PartImpactEffect.Sound )
		{
			check(RigidCollisionData.ContactInfos.Num() > 0);

			// Check its been long enough since last impact sound
			FLOAT TimeSinceLastImpact = GWorld->GetTimeSeconds() - LastImpactSoundTime;
			if(TimeSinceLastImpact > PartImpactEffect.ReFireDelay)
			{
				// Find relative velocity.
				const FVector Velocity0 = RigidCollisionData.ContactInfos(0).ContactVelocity[0];
				const FVector Velocity1 = RigidCollisionData.ContactInfos(0).ContactVelocity[1];
				const FVector RelVel = Velocity1 - Velocity0;

				// Then project along contact normal, and take magnitude.
				const FLOAT ImpactVelMag =  Abs( RelVel | RigidCollisionData.ContactInfos(0).ContactNormal );
				// If hard enough impact - play sound, and remember time!
				if(ImpactVelMag > PartImpactEffect.Threshold)
				{
					FVector ContactPos = RigidCollisionData.ContactInfos(0).ContactPosition;
					PlaySound(PartImpactEffect.Sound, TRUE, TRUE, TRUE, &ContactPos, TRUE);
					LastImpactSoundTime = GWorld->GetTimeSeconds();
				}
			}
		}		
	}
}

#if WITH_NOVODEX

/** Used to create physics collision based on bounding box of un-hidden pieces. */
void AFracturedStaticMeshPart::ModifyNxActorDesc(NxActorDesc& ActorDesc,UPrimitiveComponent* PrimComp, const class NxGroupsMask& GroupsMask, UINT MatIndex)
{
	check(FracturedStaticMeshComponent);
	check(FracturedStaticMeshComponent == CollisionComponent);
	UFracturedStaticMesh* FracMesh = CastChecked<UFracturedStaticMesh>(FracturedStaticMeshComponent->StaticMesh);

	// Clear existing shapes
	ActorDesc.shapes.clear();

	// Get bounding box for all visible parts.
	FBox ChunkBox(0);
	TArray<BYTE> VisFragments = FracturedStaticMeshComponent->GetVisibleFragments();
	for(INT i=0; i<VisFragments.Num(); i++)
	{
		if(VisFragments(i))
		{
			ChunkBox += FracMesh->GetFragmentBox(i);
		}
	}

	// Make physics shape based on this box.
	FMatrix BoxTM = FTranslationMatrix(ChunkBox.GetCenter() * DrawScale * DrawScale3D);
	FVector BoxDim = ChunkBox.GetExtent() * DrawScale * DrawScale3D;
	BoxDim.X = Abs(BoxDim.X);
	BoxDim.Y = Abs(BoxDim.Y);
	BoxDim.Z = Abs(BoxDim.Z);

	NxBoxShapeDesc* BoxDesc = new NxBoxShapeDesc;
	BoxDesc->dimensions = U2NPosition(BoxDim) + NxVec3(PhysSkinWidth, PhysSkinWidth, PhysSkinWidth);
	BoxDesc->localPose = U2NTransform(BoxTM);

	// Use desired collision mask and material
	BoxDesc->groupsMask = GroupsMask;
	BoxDesc->materialIndex = MatIndex;

	ActorDesc.shapes.push_back(BoxDesc);
}

/** Used to clean up box shape we added in ModifyNxActorDesc. */
void AFracturedStaticMeshPart::PostInitRigidBody(NxActor* nActor, NxActorDesc& ActorDesc, UPrimitiveComponent* PrimComp)
{
	check(FracturedStaticMeshComponent);
	check(FracturedStaticMeshComponent == CollisionComponent);

	// Clean up the box shape we added in ModifyNxActorDesc
	INT NumShapes = ActorDesc.shapes.size();
	check(NumShapes == 1);

	for(INT i=0; i<NumShapes; i++)
	{
		check(ActorDesc.shapes[i]->getType() == NX_SHAPE_BOX);
		delete ActorDesc.shapes[i];
		ActorDesc.shapes[i] = NULL;
	}
}
#endif // WITH_NOVODEX

void AFractureManager::TickSpecial( FLOAT DeltaSeconds )
{
	Super::TickSpecial(DeltaSeconds);

	SET_DWORD_STAT( STAT_FracturePartPoolUsed, PartPool.Num() - FreeParts.Num() );
}

FLOAT AFractureManager::GetNumFSMPartsScale()
{
	return GSystemSettings.NumFracturedPartsScale;
}


/** Returns a scalar to the percentage chance of a fractured static mesh spawning a rigid body after
	taking direct damage */
FLOAT AFractureManager::GetFSMDirectSpawnChanceScale()
{
	return GSystemSettings.FractureDirectSpawnChanceScale;
}


/** Returns a scalar to the percentage chance of a fractured static mesh spawning a rigid body after
	taking radial damage, such as from an explosion */
FLOAT AFractureManager::GetFSMRadialSpawnChanceScale()
{
	return GSystemSettings.FractureRadialSpawnChanceScale;
}


/** Returns a distance scale for whether a fractured static mesh should actually fracture when damaged */
FLOAT AFractureManager::GetFSMFractureCullDistanceScale()
{
	return GSystemSettings.FractureCullDistanceScale;
}

void AFractureManager::CreateFSMParts()
{
	DOUBLE StartTime = appSeconds();
	INT PartsCreated = 0;

	INT ConfigPartPoolSize = 0;
	if (GConfig &&
		GConfig->GetInt(TEXT("Engine.FractureManager"), TEXT("FSMPartPoolSize"), ConfigPartPoolSize, GEngineIni))
	{
		FSMPartPoolSize = ConfigPartPoolSize;
	}

	if (FSMPartPoolSize > 0)
	{
		// 262144 is HALF_WORLD_MAX @see engine.h
		// reduced a bit from that so that it doesn't get destroyed due to being out of the world
		FVector SpawnLocation = FVector(HALF_WORLD_MAX * 0.95f,HALF_WORLD_MAX * 0.95f,HALF_WORLD_MAX * 0.95f);

		INT NumFSMParts = appFloor(FSMPartPoolSize * GetNumFSMPartsScale());
		if(PartPool.Num() != NumFSMParts)
		{
			PartPool.Reset();
			PartPool.AddZeroed(NumFSMParts);
		}

		for( INT Idx=0; Idx<PartPool.Num(); Idx++ )
		{
			if (PartPool(Idx) == NULL)
			{
				AFracturedStaticMeshPart* NewPart = CastChecked<AFracturedStaticMeshPart>( GWorld->SpawnActor( AFracturedStaticMeshPart::StaticClass(), NAME_None, SpawnLocation, FRotator(0,0,0), NULL, FALSE, FALSE, this ));
				if(NewPart)
				{
					NewPart->LifeSpan = 0.f; // By default FSMParts have a lifespan which will clean them up - we want them to stay around forever.
					NewPart->RecyclePart(FALSE);

					NewPart->PartPoolIndex = Idx;
					PartPool(Idx) = NewPart;
					// Add to free set.
					FreeParts.Push(Idx);

					PartsCreated++;
				}
			}
		}
	}
	else
	{
		warnf(NAME_Log, TEXT("Skipping initialization of FSMPartPool!"));
	}

	DOUBLE EndTime = appSeconds();
	// debugf(TEXT("CreateFSMParts: Took %5.2f ms (%d created)"), 1000.f * (EndTime - StartTime), PartsCreated);
}

/** Recycles any active parts */
void AFractureManager::ResetPoolVisibility()
{
	for( INT Idx=0; Idx<PartPool.Num(); Idx++ )
	{
		AFracturedStaticMeshPart* CurrentPart = PartPool(Idx);
		if (CurrentPart != NULL && !CurrentPart->bHasBeenRecycled)
		{
			CurrentPart->RecyclePart(TRUE);
		}
	}
}

/** Grab a FSMP from the free pool, or forcibly recycle a suitable one from the world. */
AFracturedStaticMeshPart* AFractureManager::GetFSMPart(class AFracturedStaticMeshActor* Parent, FVector SpawnLocation, FRotator SpawnRotation)
{
	AFracturedStaticMeshPart* NewPart = NULL;

	INT NumFSMParts = appFloor(FSMPartPoolSize * GetNumFSMPartsScale());
	if (NumFSMParts == 0)
	{
		return NULL;
	}

	if (PartPool.Num() < NumFSMParts)
	{
		CreateFSMParts();
	}

	// First check free list.
	if(FreeParts.Num() > 0)
	{
		// Pop
		INT NewPartIndex = FreeParts.Pop();

		// Get part at that entry
		NewPart = PartPool(NewPartIndex);
		//debugf(TEXT("FROM FREE LIST"));

		// Empty slot - refill now and grab again
		if(NewPart == NULL)
		{
			CreateFSMParts();
			NewPart = PartPool(NewPartIndex);
		}

#if !FINAL_RELEASE
		// debug check
		if(NewPart)
		{
			if (!NewPart->bHasBeenRecycled)
			{
				warnf(TEXT("FSMP %s in free list but bHasBeenRecycled is FALSE!"), *NewPart->GetName());
			}
			if (NewPart == Parent)
			{
				warnf(TEXT("GetFSMPart: Parent %s was in free pool!"), *NewPart->GetName());
			}
		}
#endif // !FINAL_RELEASE
	}

	// Don't have one - find the oldest part, but also look to see if any are not being seen.
	if(NewPart == NULL)
	{
		AFracturedStaticMeshPart* OldestPart = NULL;
		FLOAT OldestAge = 0.f;
		AFracturedStaticMeshPart* OldestUnseenPart = NULL;
		FLOAT OldestUnseenAge = 0.f;

		// Iterate over pool
		for(INT i=0; i<PartPool.Num(); i++)
		{
			AFracturedStaticMeshPart* TestPart = PartPool(i);

			// If empty slot, fill in empty slots and grab again.
			if(TestPart == NULL)
			{
				CreateFSMParts();
				TestPart = PartPool(i);
			}

			if(TestPart)
			{
#if !FINAL_RELEASE
				// debug check
				if(TestPart->bHasBeenRecycled)
				{
					debugf(TEXT("Nothing in Free List, but part has bHasBeenRecycled TRUE!"));
				}
#endif // !FINAL_RELEASE

				// Don't reuse the parent
				// Don't recycle this part if it was spawned less than FSM_DEFAULTRECYCLETIME seconds ago
				if(TestPart == Parent || ( TestPart->LastSpawnTime + UCONST_FSM_DEFAULTRECYCLETIME > GWorld->GetTimeSeconds() ))
				{
					continue;
				}

				// See how long since spawned
				FLOAT ThisAge = GWorld->GetTimeSeconds() - TestPart->LastSpawnTime;

				// See if it hasn't been rendered for a bit - if so use that right away
				if(	TestPart->BaseFracturedMeshActor && 
					TestPart->BaseFracturedMeshActor->SkinnedComponent &&
					(GWorld->GetTimeSeconds() - TestPart->BaseFracturedMeshActor->SkinnedComponent->LastRenderTime > 1.0) )
				{
					// See if this is the oldest unseen so far.
					if(ThisAge > OldestUnseenAge)
					{
						OldestUnseenPart = TestPart;
						OldestUnseenAge = ThisAge;
					}
				}
				else
				{
					// See if this is the oldest visible so far.
					if(ThisAge > OldestAge)
					{
						OldestPart = TestPart;
						OldestAge = ThisAge;
					}
				}
			}
		}

		// Didn't find an unseen one, use the oldest one
		if(OldestUnseenPart != NULL)
		{
			NewPart = OldestUnseenPart;
			//debugf(TEXT("USING OLDEST UNSEEN"));
		}
		else
		{
			NewPart = OldestPart;
			//debugf(TEXT("USING OLDEST VISIBLE"));
		}
	}

	// We have a part
	if( NewPart != NULL )
	{
		// Make sure part is in its 'recycled' state.
		if (!NewPart->bHasBeenRecycled)
		{
			NewPart->RecyclePart(FALSE);
		}

		NewPart->SetLocation(SpawnLocation);
		NewPart->SetRotation(SpawnRotation);
		NewPart->FracturedStaticMeshComponent->SetRBPosition(SpawnLocation);
		NewPart->FracturedStaticMeshComponent->SetRBRotation(SpawnRotation);
		NewPart->SetDrawScale(1.f);
		NewPart->SetDrawScale3D(FVector(1.f,1.f,1.f));
		NewPart->Initialize();
		NewPart->LastSpawnTime = GWorld->GetTimeSeconds();
	}

	return NewPart;
}
