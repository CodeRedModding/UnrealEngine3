/*=============================================================================
	DecalComponent.cpp: Decal implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineDecalClasses.h"
#include "ScenePrivate.h"
#include "UnTerrain.h"
#include "UnTerrainRender.h"

IMPLEMENT_CLASS(UDecalComponent);

DECLARE_STATS_GROUP(TEXT("DECALS"),STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Attach Time"),STAT_DecalAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("  BSP Attach Time"),STAT_DecalBSPAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("  Static Mesh Attach Time"),STAT_DecalStaticMeshAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("  Terrain Attach Time"),STAT_DecalTerrainAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("  Skeletal Mesh Attach Time"),STAT_DecalSkeletalMeshAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT(" HitComponent Attach Time"),STAT_DecalHitComponentAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT(" HitNode Attach Time"),STAT_DecalHitNodeAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT(" MultiComponent Attach Time"),STAT_DecalMultiComponentAttachTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT(" ReceiverImages Attach Time"),STAT_DecalReceiverImagesAttachTime,STATGROUP_Decals);

/** Decal stats */
DECLARE_DWORD_COUNTER_STAT(TEXT("Decal Triangles"),STAT_DecalTriangles,STATGROUP_Decals);
DECLARE_DWORD_COUNTER_STAT(TEXT("Decal Draw Calls"),STAT_DecalDrawCalls,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Decal Render Time Unlit Total"),STAT_DecalRenderUnlitTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Decal Render Time Lit Total"),STAT_DecalRenderLitTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Decal Render Time BSP Dynamic"),STAT_DecalRenderDynamicBSPTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Decal Render Time SM Dynamic"),STAT_DecalRenderDynamicSMTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Decal Render Time Terrain Dynamic"),STAT_DecalRenderDynamicTerrainTime,STATGROUP_Decals);
DECLARE_CYCLE_STAT(TEXT("Decal Render Time Skel Mesh Dynamic"),STAT_DecalRenderDynamicSkelTime,STATGROUP_Decals);

/*-----------------------------------------------------------------------------
	Helpers
-----------------------------------------------------------------------------*/


/** Copies data from an FStaticReceiverData instance to an FDecalRenderData instance. */
static void CopyStaticReceiverDataToDecalRenderData(FDecalRenderData& Dst, const FStaticReceiverData& Src)
{
	Dst.InstanceIndex			= Src.InstanceIndex;
	Dst.Vertices				= Src.Vertices;
	Dst.IndexBuffer.Indices		= Src.Indices;
	Dst.NumTriangles			= Src.NumTriangles;
	Dst.LightMap1D				= Src.LightMap1D;
	Dst.ShadowMap1D				= Src.ShadowMap1D;
	Dst.bUsesIndexResources		= Src.Indices.Num() > 0;
	Dst.Data					= Src.Data;
}

/** Copies data from an FDecalRenderData instance to an FStaticReceiverData instance. */
static void CopyDecalRenderDataToStaticReceiverData(FStaticReceiverData& Dst, const FDecalRenderData& Src)
{
	Dst.InstanceIndex			= Src.InstanceIndex;
	Dst.Vertices				= Src.Vertices;
	Dst.Indices					= Src.IndexBuffer.Indices;
	Dst.NumTriangles			= Src.NumTriangles;
	Dst.LightMap1D				= Src.LightMap1D;
	Dst.ShadowMap1D				= Src.ShadowMap1D;
	Dst.Data					= Src.Data;
}

/*-----------------------------------------------------------------------------
	FDecalSceneProxy
-----------------------------------------------------------------------------*/


#if !FINAL_RELEASE
class FDecalSceneProxy : public FPrimitiveSceneProxy
{
public:
	FDecalSceneProxy(const UDecalComponent* Component)
		:	FPrimitiveSceneProxy( Component )
		,	Owner( Component->GetOwner() )
		,	DepthPriorityGroup( Component->DepthPriorityGroup )
		,	Bounds( Component->Bounds )
	{
		Component->CaptureDecalState(&DecalState);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		const EShowFlags ShowFlags = View->Family->ShowFlags;

		if ((ShowFlags & SHOW_Decals) != 0)
		{
			SetRelevanceForShowBounds(View->Family->ShowFlags, Result);
			if ( (ShowFlags & SHOW_Bounds) != 0 ||
				((ShowFlags & SHOW_DecalInfo) != 0 && (GIsGame || bSelected)) )
			{
				Result.bDynamicRelevance = TRUE;
				Result.SetDPG(SDPG_World,TRUE);
			}
		}

		return Result;
	}

	/**
	* @return TRUE if the proxy requires occlusion queries
	*/
	virtual UBOOL RequiresOcclusion(const FSceneView* View) const
	{
		// only allow occlusion queries for decals when viewing decal frustums
		return (View->Family->ShowFlags & SHOW_DecalInfo) != 0;
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
		if ((View->Family->ShowFlags & SHOW_Decals) != 0)
		{	
			RenderBounds(PDI, DPGIndex, View->Family->ShowFlags, PrimitiveSceneInfo->Bounds, !Owner || bSelected);

			if ( (View->Family->ShowFlags & SHOW_DecalInfo) != 0 && ((View->Family->ShowFlags & SHOW_Game) != 0 || !Owner || bSelected)  )
			{
				//if ( DPGIndex == SDPG_World )
				{
					const FColor White(255, 255, 255);
					const FColor Red(255,0,0);
					const FColor Green(0,255,0);
					const FColor Blue(0,0,255);
					
					const FLOAT Width = DecalState.Width;
					const FLOAT Height = DecalState.Height;
					const FVector& HitLocation = DecalState.HitLocation;
					const FVector& HitNormal = DecalState.HitNormal;
					const FVector& HitTangent = DecalState.HitTangent;
					const FVector& HitBinormal = DecalState.HitBinormal;

					// Upper box.
					PDI->DrawLine( DecalState.FrustumVerts[0], DecalState.FrustumVerts[1], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[1], DecalState.FrustumVerts[2], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[2], DecalState.FrustumVerts[3], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[3], DecalState.FrustumVerts[0], White, SDPG_World );

					// Lower box.
					PDI->DrawLine( DecalState.FrustumVerts[4], DecalState.FrustumVerts[5], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[5], DecalState.FrustumVerts[6], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[6], DecalState.FrustumVerts[7], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[7], DecalState.FrustumVerts[4], White, SDPG_World );

					// Vertical box pieces.
					PDI->DrawLine( DecalState.FrustumVerts[0], DecalState.FrustumVerts[4], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[1], DecalState.FrustumVerts[5], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[2], DecalState.FrustumVerts[6], White, SDPG_World );
					PDI->DrawLine( DecalState.FrustumVerts[3], DecalState.FrustumVerts[7], White, SDPG_World );

					// Normal, Tangent, Binormal.
					const FLOAT HalfWidth = Width/2.f;
					const FLOAT HalfHeight = Height/2.f;
					PDI->DrawLine( HitLocation, HitLocation + (HitTangent*HalfWidth), Green, SDPG_World );
					PDI->DrawLine( HitLocation, HitLocation + (HitBinormal*HalfHeight), Blue, SDPG_World );
				}
			}
		}
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	AActor* Owner;

	BITFIELD DepthPriorityGroup : UCONST_SDPG_NumBits;

	FBoxSphereBounds Bounds;

	FDecalState DecalState;
};
#endif // !FINAL_RELEASE

/*-----------------------------------------------------------------------------
	UDecalComponent
-----------------------------------------------------------------------------*/

/**
 * @return	TRUE if the decal is enabled as specified in scalability options, FALSE otherwise.
 */
static UBOOL bForceEnableForSave=FALSE;
UBOOL UDecalComponent::IsEnabled() const
{
	const UBOOL bShowInEditor = (!HiddenEditor && (!Owner || !Owner->IsHiddenEd())) || bForceEnableForSave;
	const UBOOL bShowInGame	= !HiddenGame && (!Owner || !Owner->bHidden || bIgnoreOwnerHidden || bCastHiddenShadow);
	const UBOOL bAllowed = (bStaticDecal && GSystemSettings.bAllowStaticDecals) || (!bStaticDecal && GSystemSettings.bAllowDynamicDecals);
	return bAllowed && ((GIsGame && bShowInGame) || (!GIsGame && bShowInEditor));
}

/**
 * Builds orthogonal planes from HitLocation, HitNormal, Width, Height and Thickness.
 */
void UDecalComponent::UpdateOrthoPlanes()
{
	enum PlaneIndex
	{
		DP_LEFT,
		DP_RIGHT,
		DP_FRONT,
		DP_BACK,
		DP_NEAR,
		DP_FAR,
		DP_NUM
	};

	// Set the decal's backface direction based on the handedness of the owner frame if it's static decal.
	bFlipBackfaceDirection = bStaticDecal && Owner ? (Owner->DrawScale3D.X*Owner->DrawScale3D.Y*Owner->DrawScale3D.Z < 0.f ) : FALSE;

	const FVector& Position = Location;
	const FVector Normal = Orientation.Vector().SafeNormal() * (bFlipBackfaceDirection ? 1.0f : -1.0f);
	const FRotationMatrix NewFrame( Orientation );
	const FLOAT OffsetRad = static_cast<FLOAT>( PI*DecalRotation/180. );
	const FLOAT CosT = appCos( OffsetRad );
	const FLOAT SinT = appSin( OffsetRad );

	const FMatrix OffsetMatrix(
		FPlane(1.0f,	0.0f,	0.0f,	0.0f),
		FPlane(0.0f,	CosT,	SinT,	0.0f),
		FPlane(0.0f,	-SinT,	CosT,	0.0f),
		FPlane(0.0f,	0.0f,	0.0f,	1.0f) );

	const FMatrix OffsetFrame( OffsetMatrix*NewFrame );

	const FVector Tangent = -OffsetFrame.GetAxis(1).SafeNormal();
	const FVector Binormal = OffsetFrame.GetAxis(2).SafeNormal();

	// Ensure the Planes array is correctly sized.
	if ( Planes.Num() != DP_NUM )
	{
		Planes.Empty( DP_NUM );
		Planes.Add( DP_NUM );
	}

	const FLOAT TDotP = Tangent | Position;
	const FLOAT BDotP = Binormal | Position;
	const FLOAT NDotP = Normal | Position;

	Planes(DP_LEFT)	= FPlane( -Tangent, Width/2.f - TDotP );
	Planes(DP_RIGHT)= FPlane( Tangent, Width/2.f + TDotP );

	Planes(DP_FRONT)= FPlane( -Binormal, Height/2.f - BDotP );
	Planes(DP_BACK)	= FPlane( Binormal, Height/2.f + BDotP );

	Planes(DP_NEAR)	= FPlane( Normal, -NearPlane + NDotP );
	Planes(DP_FAR)	= FPlane( -Normal, FarPlane - NDotP );

	HitLocation = Position;
	HitNormal = Normal;
	HitBinormal = Binormal;
	HitTangent = Tangent;
}

/**
 * @return		TRUE if Super::IsValidComponent() return TRUE.
 */
UBOOL UDecalComponent::IsValidComponent() const
{
	return Super::IsValidComponent();
}

/** 
  * EditorLinkSelectionInterface 
  */
UBOOL UDecalComponent::LinkSelection(USelection* SelectedActors)
{
	UBOOL bRet = FALSE;
	if ( SelectedActors )
	{
		for( INT SelectedIdx=0; SelectedIdx<SelectedActors->Num(); SelectedIdx++ )
		{
			AActor* Actor = Cast<AActor>((*SelectedActors)(SelectedIdx));
			if ( Actor && !Actor->IsA(ADecalActorBase::StaticClass()) )
			{
				Filter.AddUniqueItem( Actor );
				bRet = TRUE;
			}
		}
	}
	return bRet;
}

UBOOL UDecalComponent::UnLinkSelection(USelection* SelectedActors)
{
	UBOOL bRet = FALSE;
	if ( SelectedActors )
	{
		for( INT SelectedIdx=0; SelectedIdx<SelectedActors->Num(); SelectedIdx++ )
		{
			AActor* Actor = Cast<AActor>((*SelectedActors)(SelectedIdx));
			if ( Actor && !Actor->IsA(ADecalActorBase::StaticClass()) )
			{
				Filter.RemoveItem( Actor );
				bRet = TRUE;
			}
		}
	}
	return bRet;
}

#if WITH_EDITOR
void UDecalComponent::CheckForErrors()
{
	Super::CheckForErrors();

	// Get the decal owner's name.
	FString OwnerName(GNone);
	if ( Owner )
	{
		OwnerName = Owner->GetName();
	}

	if ( !DecalMaterial && ( bDecalMaterialSetAtRunTime == FALSE ) )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DecalMaterialNULL" ), *GetName(), *OwnerName ) ), TEXT( "DecalMaterialNull" ) );
	}

	const UBOOL bDecalIsMovable = Owner && Owner->bMovable && bMovableDecal;
	const UBOOL bDecalIsAttachedToBone = Owner && Owner->Base && Owner->BaseSkelComponent;
	const UBOOL bDecalIsUnfiltered = (FilterMode == FM_None) || (Filter.Num() == 0);
	if( bDecalIsMovable && bDecalIsAttachedToBone && bDecalIsUnfiltered )
	{
		GWarn->MapCheck_Add( MCTYPE_PERFORMANCEWARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_UnfilteredSkeletalMeshDecal" ), *GetName(), *OwnerName ) ), TEXT( "UnfilteredSkeletalMeshDecal" ) );
	}
}
#endif

/**
 * If the decal is non-static, allocates a sort key and, if the decal is lit, a random
 * depth bias to the decal.
 */
void UDecalComponent::AllocateSortKey()
{
	if ( !bStaticDecal )
	{
		// Initialise the sort keys to some large value which hopefully won't interfere with
		// the sort keys set by designers on static decals.
		static INT SDecalSortKey = 10000;
		SortOrder = ++SDecalSortKey;
	}
}

void UDecalComponent::Attach()
{
	// needed to make sure that the render proxy has updated position,tangents
	UpdateOrthoPlanes();

	// We need to make sure the transform/bounds is updated before we call ComputeReceivers
	Super::Attach();

	const UBOOL bDetailModeAllowsRendering	= DetailMode <= GSystemSettings.DetailMode;
	if (!GIsUCC && bDetailModeAllowsRendering && GWorld && GWorld->bAllowDecalAttach)
	{
		DetachFromReceivers();		

		// Dynamic decals and static decals not in game attach here.
		// Static decals in game attach in UDecalComponent::BeginPlay().
		if( !bStaticDecal || (bStaticDecal && !GIsGame) || bHasBeenAttached )
		{
			// All decals outside of game will fully compute receivers.
			if ( !GIsGame || StaticReceivers.Num() == 0 )
			{
				ComputeReceivers();
			}
			else
			{
				AttachToStaticReceivers();
			}
			bHasBeenAttached=TRUE;
		}
	}	
}

void UDecalComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);

	if( !bHasBeenAttached )
	{
		if (DecalTransform == DecalTransform_SpawnRelative)
		{
			// whenever we are about to reattach, save off the original attachment 
			// location and orientation relative to the world transform of our attachment
			ParentRelLocRotMatrix = FRotationTranslationMatrix(Orientation,Location) * ParentToWorld.Inverse();
			
		}
		else if (DecalTransform == DecalTransform_OwnerRelative)
		{
			// Location/Orientation are transformed relative to the owning actor's coordinates
			ParentRelLocRotMatrix = FRotationTranslationMatrix(ParentRelativeOrientation,ParentRelativeLocation);
		}
	}

	// Location/Orientation are obtained from the owning actor's absolute world coordinates
	if( DecalTransform == DecalTransform_OwnerAbsolute )
	{
		Location = Owner->Location;
		Orientation = Owner->Rotation;
	}
	else
	{
		// transform parent relative attachment locations to world space
		FMatrix Mat = ParentRelLocRotMatrix * ParentToWorld;
		Location = Mat.GetOrigin();
		Orientation = Mat.Rotator();
	}
}

void UDecalComponent::UpdateTransform()
{
	// We need to make sure the transform/bounds is updated before we call ComputeReceivers
	Super::UpdateTransform();

	const UBOOL bDecalIsMovable = bMovableDecal && Owner && Owner->bMovable;
	const UBOOL bDecalIsAttached = Owner && Owner->Base;
	const UBOOL bDecalIsFiltered = (FilterMode == FM_Affect) && (Filter.Num() > 0);

	// Movable decals that are not manually attached to, and filtered on, a base actor or
	// actors will reattach to all possible receivers dynamically (if manually attached
	// and filtered, we avoid this additional processing)
	if( bDecalIsMovable && !(bDecalIsAttached && bDecalIsFiltered) )
	{
		// Note that this will regenerate new decal render data and could be slow
		DetachFromReceivers();
		ComputeReceivers();
	}
	else
	{
		// All non-movable, as well as all optimally configured movable, decals
		// go through this fast path
		UpdateOrthoPlanes();
	}
}

#if STATS || LOOKING_FOR_PERF_ISSUES
namespace {

static DOUBLE GSumMsecsSpent = 0.;
static INT GNumSamples = 0;
static INT GSampleSize = 20;

class FScopedTimingBlock
{
	const UDecalComponent* Decal;
	const TCHAR* OutputString;
	DOUBLE StartTime;

public:
	FScopedTimingBlock(UDecalComponent* InDecal, const TCHAR* InString)
		:	Decal( InDecal )
		,	OutputString( InString )
	{
		StartTime = appSeconds();
	}

	~FScopedTimingBlock()
	{
		const DOUBLE MsecsSpent = (appSeconds() - StartTime)*1000.0;

		if ( !Decal->bStaticDecal )
		{
			GSumMsecsSpent += MsecsSpent;
			++GNumSamples;
			if ( GNumSamples == GSampleSize )
			{
				const DOUBLE MsecsAvg = GSumMsecsSpent / static_cast<DOUBLE>(GNumSamples);
				debugf( NAME_DevDecals, TEXT("Decal AVG: (%.3f)"), MsecsAvg );
				GSumMsecsSpent = 0.;
				GNumSamples = 0;
			}
		}

		//if ( MsecsSpent > 1.0 )
		{
			const FString DecalMaterial = Decal->GetDecalMaterial()->GetMaterial()->GetName();
			if ( Decal->bStaticDecal && Decal->GetOwner() )
			{
				// Log the decal actor name if the decal is static (level-placed) and has an owner.
				debugf( NAME_DevDecals, TEXT("Decal %s(%i,%s) - %s(%.3f)"),
					*Decal->GetOwner()->GetName(), Decal->DecalReceivers.Num(), Decal->bNoClip ? TEXT("TRUE"):TEXT("FALSE"), OutputString, MsecsSpent );
			}
			else
			{
				// Log the decal material.
				debugf( NAME_DevDecals, TEXT("Decal %s(%i,%s) - %s(%.3f)"),
					*DecalMaterial, Decal->DecalReceivers.Num(), Decal->bNoClip ? TEXT("TRUE"):TEXT("FALSE"), OutputString, MsecsSpent );
			}
		}
	}
};
} // namespace
#endif

/**
* Enqueues decal render data deletion. Split into separate function to work around ICE with VS.NET 2003.
*
* @param DecalRenderData	Decal render data to enqueue deletion for
*/
static void FORCENOINLINE EnqueueDecalRenderDataDeletion( FDecalRenderData* DecalRenderData )
{
	// We have to clear the lightmap reference on the game thread because FLightMap1D::Cleanup enqueues
	// a rendering command.  Rendering commands cannot be enqueued from the rendering thread, which would
	// happen if we let FDecalRenderData's dtor clear the lightmap reference.
	DecalRenderData->LightMap1D = NULL;
	for (INT ShadowMap1DIdx=0; ShadowMap1DIdx < DecalRenderData->ShadowMap1D.Num(); ShadowMap1DIdx++)
	{
		if (DecalRenderData->ShadowMap1D(ShadowMap1DIdx) != NULL)
		{
			BeginReleaseResource(DecalRenderData->ShadowMap1D(ShadowMap1DIdx));
		}
	}

	// Enqueue deletion of render data and releasing its resources.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( 
		DeleteDecalRenderDataCommand, 
		FDecalRenderData*,
		DecalRenderData,
		DecalRenderData,
	{
		delete DecalRenderData;
	} );
}

#define ATTACH_RECEIVER(PrimitiveComponent) \
	if( (PrimitiveComponent) && (PrimitiveComponent)->IsAttached() && (PrimitiveComponent)->GetScene() == GetScene() ) \
	{ \
		AttachReceiver( PrimitiveComponent ); \
	}

/**
 * Attaches to static receivers.
 */
void UDecalComponent::AttachToStaticReceivers()
{
	if ( !IsEnabled() || bMovableDecal )
	{
		return; 
	}

	// Make sure the planes and hit info are up to date for bounds calculations.
	UpdateOrthoPlanes();

	if ( DecalMaterial ) 
	{
		// Is the decal material lit?
		const UMaterial* Material = DecalMaterial->GetMaterial();
		const UBOOL bLitDecalMaterial = Material && (Material->LightingModel != MLM_Unlit);

		// const FScopedTimingBlock TimingBlock( this, TEXT("StaticReceivers") );
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < StaticReceivers.Num() ; ++ReceiverIndex )
		{
			FStaticReceiverData* StaticReceiver = StaticReceivers(ReceiverIndex);
			UPrimitiveComponent* Receiver = StaticReceiver->Component;
			if ( Receiver && Receiver->IsAttached() && Receiver->GetScene() == GetScene() )
			{
				// Only terrain needs to recompute decal data
				if( Receiver->IsA(UTerrainComponent::StaticClass()) || Receiver->IsA(ULandscapeComponent::StaticClass()) ) 
				{
					FDecalState DecalState;
					CaptureDecalState( &DecalState );

					// Generate decal geometry.
					static TArray< FDecalRenderData* > DecalRenderDataList;
					DecalRenderDataList.Reset();

					Receiver->GenerateDecalRenderData( &DecalState, DecalRenderDataList );

					// Was any geometry created?
					for( INT CurDecalRenderDataIndex = 0; CurDecalRenderDataIndex < DecalRenderDataList.Num(); ++CurDecalRenderDataIndex )
					{
						FDecalRenderData* ThisRenderData = DecalRenderDataList( CurDecalRenderDataIndex );
						check( ThisRenderData != NULL );

						if( ThisRenderData->NumTriangles > 0 )
						{
							ThisRenderData->InitResources_GameThread();
							if( ThisRenderData->DecalVertexFactory )
							{
								// Add the decal attachment to the receiver.
								Receiver->AttachDecal( this, ThisRenderData, &DecalState );

								// Attach this render data to the terrain. We will have one per RenderData.
								FDecalReceiver* NewDecalReceiver = new(DecalReceivers) FDecalReceiver;
								NewDecalReceiver->Component = Receiver;
								NewDecalReceiver->RenderData = ThisRenderData;
							}
							else
							{
								EnqueueDecalRenderDataDeletion(ThisRenderData);
							}
						}
						else
						{
							EnqueueDecalRenderDataDeletion(ThisRenderData);
						}
					}
				}
				else if( StaticReceiver->NumTriangles > 0 )
				{
					FDecalRenderData* DecalRenderData = new FDecalRenderData( NULL, TRUE, TRUE );
					CopyStaticReceiverDataToDecalRenderData( *DecalRenderData, *StaticReceiver );

					if( Receiver->IsA(UModelComponent::StaticClass()) )
					{
						// set the blending interval - currently disabled for BSP
						static const FVector2D DisabledBlendRage( appCos(89.5f * PI/180.f), appCos(89.0f * PI/180.f) );
						DecalRenderData->DecalBlendRange = DisabledBlendRage;
					}
					else
					{	
						// set the blending interval
						DecalRenderData->DecalBlendRange = CalcDecalDotProductBlendRange();
					}

					DecalRenderData->InitResources_GameThread();
					if( DecalRenderData->DecalVertexFactory )
					{
						// Add the decal attachment to the receiver.
						Receiver->AttachDecal( this, DecalRenderData, NULL );

						// Add the receiver to this decal.
						FDecalReceiver* NewDecalReceiver = new(DecalReceivers) FDecalReceiver;
						NewDecalReceiver->Component = Receiver;
						NewDecalReceiver->RenderData = DecalRenderData;
					}
					else
					{
						EnqueueDecalRenderDataDeletion(DecalRenderData);
					}   
				}
			}
		}
	}

	// free the static receiver data serialized for the decal after copying it as decal render data
	FreeStaticReceivers();
}

/**
 * Updates ortho planes, computes the receiver set, and connects to a decal manager.
 */
void UDecalComponent::ComputeReceivers()
{
	SCOPE_CYCLE_COUNTER(STAT_DecalAttachTime);

	if ( !IsEnabled() )
	{
		return;
	}

	// Make sure the planes and hit info are up to date for bounds calculations.
	UpdateOrthoPlanes();

	const UBOOL bHasBegunPlay = GWorld->HasBegunPlay();

	if( !DecalMaterial || DecalMaterial == GEngine->DefaultMaterial )
	{
		debugf( NAME_DevDecals, TEXT("%s missing DecalMaterial=%s"),
			*GetPathName(),
			DecalMaterial ? *DecalMaterial->GetPathName() : TEXT("None"));
	}
	else
	{
		// Allocate sort key and possibly depth-bias to non-static decals.
		AllocateSortKey();
		const UBOOL bProjectOnNonBSP = bProjectOnStaticMeshes || bProjectOnSkeletalMeshes || bProjectOnTerrain;
		const AActor* DecalOwner = GetOwner();

		// Hit component to attach to was specified.
		if ( HitComponent && bProjectOnNonBSP ) 
		{
			SCOPE_CYCLE_COUNTER(STAT_DecalHitComponentAttachTime);
			check( !HitComponent->IsA(UModelComponent::StaticClass()) );
			ATTACH_RECEIVER( HitComponent );
		}
		// Hit node index of BSP to attach to was specified.
		else if( HitNodeIndex != INDEX_NONE && bProjectOnBSP )
		{
			SCOPE_CYCLE_COUNTER(STAT_DecalHitNodeAttachTime);
			if( HitLevelIndex != INDEX_NONE )
			{
				if( GWorld->Levels.IsValidIndex(HitLevelIndex) )
				{
					// Retrieve model component associated with node and level and attach it.
					ULevel*				Level			= GWorld->Levels(HitLevelIndex);
					if( Level->Model->Nodes.IsValidIndex(HitNodeIndex) )
					{
						const FBspNode&		Node			= Level->Model->Nodes( HitNodeIndex );
						if( Level->ModelComponents.IsValidIndex(Node.ComponentIndex) )
						{
							UModelComponent*	ModelComponent	= Level->ModelComponents( Node.ComponentIndex );
							ATTACH_RECEIVER( ModelComponent );
						}
						else
						{
							UMaterial* DecalMat = DecalMaterial->GetMaterial();
							warnf(TEXT("Failed to attach %s decalmat=%s to BSP Level=%s Level->ModelComponents.Num()=%d Node.ComponentIndex=%d"),
								*GetFullName(),
								DecalMat ? *DecalMat->GetPathName() : TEXT("None"),
								*Level->GetPathName(),
								Level->ModelComponents.Num(),
								Node.ComponentIndex
								);
						}
					}
					else
					{
						UMaterial* DecalMat = DecalMaterial->GetMaterial();
						warnf(TEXT("Failed to attach %s decalmat=%s to BSP Level=%s Level->Model->Nodes.Num()=%d HitNodeIndex=%d"),
							*GetFullName(),
							DecalMat ? *DecalMat->GetPathName() : TEXT("None"),
							*Level->GetPathName(),
							Level->Model->Nodes.Num(),
							HitNodeIndex
							);

					}
				}
				else
				{
					UMaterial* DecalMat = DecalMaterial->GetMaterial();
					warnf(TEXT("Failed to attach %s decalmat=%s to BSP GWorld->Levels.Num()=%d HitLevelIndex=%d"),
						*GetFullName(),
						DecalMat ? *DecalMat->GetPathName() : TEXT("None"),
						GWorld->Levels.Num(),
						HitLevelIndex
						);
				}
			}
			else
			{
				UMaterial* DecalMat = DecalMaterial->GetMaterial();
				warnf(TEXT("Failed to attach %s decalmat=%s to BSP HitNodeIndex=%d HitLevelIndex=%d"),
					*GetFullName(),
					DecalMat ? *DecalMat->GetPathName() : TEXT("None"),
					HitNodeIndex,
					HitLevelIndex
					);
			}			
		}
		// Receivers were specified.
		else if ( ReceiverImages.Num() > 0 )
		{
			SCOPE_CYCLE_COUNTER(STAT_DecalReceiverImagesAttachTime);
			for ( INT ReceiverIndex = 0 ; ReceiverIndex < ReceiverImages.Num() ; ++ReceiverIndex )
			{
				UPrimitiveComponent* PotentialReceiver = ReceiverImages(ReceiverIndex);
				// Fix up bad content as we can't attach a model component via receiver images as it's lacking node information.
				if( PotentialReceiver && PotentialReceiver->IsA(UModelComponent::StaticClass()) && bProjectOnBSP )
				{
					warnf(TEXT("Trying to attach %s to %s via ReceiverImages. This is not valid, entry is being removed."),*GetDetailedInfo(),*PotentialReceiver->GetDetailedInfo());
					ReceiverImages.Remove(ReceiverIndex--);
				}
				ATTACH_RECEIVER( PotentialReceiver );
			}
		}
		// No information about hit, determine ourselves. HitLevelIndex might be set but we ignore it.
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_DecalMultiComponentAttachTime);

			// If the decal is static, get its level as we only want to attach it to the same level.
			ULevel* StaticDecalLevel = NULL;
			if( bStaticDecal && DecalOwner )
			{
				StaticDecalLevel = DecalOwner->GetLevel();
			}

			// Update the decal bounds and query the collision hash for potential receivers.
			UpdateBounds();

			// Handle manually placed attachments on skeletal meshes
			if( DecalOwner &&
				DecalOwner->BaseSkelComponent &&
				DecalOwner->BaseBoneName != NAME_None )
			{
				// Attach if the manually specified skeletal mesh and bone name are valid
				ATTACH_RECEIVER( DecalOwner->BaseSkelComponent );
			}

			// Handle actors, aka static meshes, skeletal meshes and terrain.
			if( bProjectOnNonBSP )
			{
				FMemMark Mark(GMainThreadMemStack);
				FCheckResult* ActorResult = GWorld->Hash->ActorOverlapCheck( GMainThreadMemStack, NULL, Bounds.Origin, Bounds.SphereRadius, TRACE_AllComponents );

				// Attach to overlapping actors.
				for( FCheckResult* HitResult = ActorResult ; HitResult ; HitResult = HitResult->GetNext() )
				{
					UPrimitiveComponent* HitComponent = HitResult->Component;
					// Only project on actors that accept decals and also check for detail mode as detail mode leaves components in octree
					// for gameplay reasons and simply doesn't render them.
					const UBOOL bAcceptsStaticDecals = HitComponent && HitComponent->bAcceptsStaticDecals && (bStaticDecal || bMovableDecal);
					const UBOOL bAcceptsDynamicDecals = HitComponent && HitComponent->bAcceptsDynamicDecals && ((bHasBegunPlay && !bStaticDecal) || bMovableDecal);
					if (bAcceptsStaticDecals || bAcceptsDynamicDecals)
					{
						// If this is a static decal with a level, make sure the receiver is in the same level as the decal.
						if ( StaticDecalLevel )
						{
							const AActor* ReceiverOwner = HitComponent->GetOwner();
							if ( ReceiverOwner && !ReceiverOwner->IsInLevel(StaticDecalLevel) )
							{
								continue;
							}
						}

						ULandscapeComponent* LandComp = Cast<ULandscapeComponent>(HitComponent);
						if (LandComp)
						{
							// Reject at grazing angles, for Landscape this seem to be a good place for that
							// Assume that Landscape Component and Collision Component are in same actor for the match...
							FMemMark		Mark(GMainThreadMemStack);
							FCheckResult*	FirstHit	= NULL;
							DWORD			TraceFlags	= TRACE_Terrain|TRACE_TerrainIgnoreHoles;

							UBOOL bNeedCulling = TRUE;
							FirstHit	= GWorld->MultiLineCheck(GMainThreadMemStack, 
								Location - (HitNormal * FarPlane),
								Location - (HitNormal * NearPlane),
								FVector(0.f,0.f,0.f), TraceFlags, NULL);

							for( FCheckResult* Test = FirstHit; Test; Test = Test->GetNext() )
							{
								ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(Test->Component);
								if( CollisionComponent )
								{
									// Normal check
									const FLOAT Dot = HitNormal | Test->Normal;
									// Determine whether decal is front facing.
									const UBOOL bIsFrontFacing = bFlipBackfaceDirection ? -Dot > BackfaceAngle : Dot > BackfaceAngle;

									// Even if backface culling is disabled, reject triangles that view the decal at grazing angles.
									bNeedCulling = !bIsFrontFacing && ( !bProjectOnBackfaces || Abs( Dot ) <= BackfaceAngle );
									if (!bNeedCulling)
									{
										break;
									}
								}
							}

							Mark.Pop();

							if (bNeedCulling)
							{
								continue;
							}
						}

						// Attach the decal if the level matched, or if the decal was spawned in game.
						ATTACH_RECEIVER( HitComponent );
					}
				}

				Mark.Pop();
			}

			// Handle BSP.
			if( bProjectOnBSP )
			{
				// Indices into levels component arrays nodes are associated with.
				TArray<INT> ModelComponentIndices;

				// Iterate over all levels, checking for overlap.
				for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
				{
					ULevel* Level = GWorld->Levels(LevelIndex);

					// If the decal is static, make sure the receiving BSP is in the same level as the decal.
					if( StaticDecalLevel && StaticDecalLevel != Level )
					{
						continue;
					}

					// Filter bounding box down the BSP tree, retrieving overlapping nodes.
					HitNodeIndices.Reset();
					ModelComponentIndices.Reset();
					Level->Model->GetBoxIntersectingNodesAndComponents( Bounds.GetBox(), HitNodeIndices, ModelComponentIndices );

					// Iterate over model components to attach.
					for( INT ModelComponentIndexIndex=0;  ModelComponentIndexIndex<ModelComponentIndices.Num(); ModelComponentIndexIndex++ )
					{
						INT ModelComponentIndex = ModelComponentIndices(ModelComponentIndexIndex);
						if( Level->ModelComponents.IsValidIndex( ModelComponentIndex ) )
						{
							UModelComponent* ModelComponent = Level->ModelComponents(ModelComponentIndex);
							ATTACH_RECEIVER( ModelComponent );
						}
						else
						{
							UMaterial* DecalMat = DecalMaterial->GetMaterial();
							warnf(TEXT("Failed to attach %s decalmat=%s to BSP Level=%s Level->ModelComponents.Num()=%d Node.ComponentIndex=%d"),
								*GetFullName(),
								DecalMat ? *DecalMat->GetPathName() : TEXT("None"),
								*Level->GetPathName(),
								Level->ModelComponents.Num(),
								ModelComponentIndex
								);
						}
					}
				}
			}
		}
	}	
} 

void UDecalComponent::AttachReceiver(UPrimitiveComponent* Receiver)
{
	// Invariant: Receiving component is not already in the decal's receiver list
	const UBOOL bHasBegunPlay = GWorld->HasBegunPlay();
	const UBOOL bAcceptsStaticDecals = Receiver && Receiver->bAcceptsStaticDecals && (bStaticDecal || bMovableDecal);
	const UBOOL bAcceptsDynamicDecals = Receiver && Receiver->bAcceptsDynamicDecals && ((bHasBegunPlay && !bStaticDecal) || bMovableDecal);
	if( (bAcceptsStaticDecals || bAcceptsDynamicDecals) &&
		Receiver->IsValidComponent() &&
		Receiver->IsAttached() &&
		Receiver->SupportsDecalRendering() )
	{
		const UBOOL bIsReceiverHidden = (Receiver->GetOwner() && Receiver->GetOwner()->bHidden) || Receiver->HiddenGame;
		if ( !bIsReceiverHidden || bProjectOnHidden )
		{
			// look for an existing receiver before attaching the new primitive
			UBOOL bFoundReceiver = FALSE;
			for( INT Idx=0; Idx < DecalReceivers.Num(); Idx++ )
			{
				const FDecalReceiver& CurReceiver = DecalReceivers(Idx);
				if( CurReceiver.Component == Receiver )
				{
					bFoundReceiver=TRUE;					
				}
			}

			if( !bFoundReceiver && 
				FilterComponent( Receiver ) )
			{
				FDecalState DecalState;
				CaptureDecalState( &DecalState );

				// Generate decal geometry.
				static TArray< FDecalRenderData* > DecalRenderDataList;
				DecalRenderDataList.Reset();

				Receiver->GenerateDecalRenderData( &DecalState, DecalRenderDataList );

				// Was any geometry created?
				for( INT CurDecalRenderDataIndex = 0; CurDecalRenderDataIndex < DecalRenderDataList.Num(); ++CurDecalRenderDataIndex )
				{
					FDecalRenderData* DecalRenderData = DecalRenderDataList( CurDecalRenderDataIndex );
					check( DecalRenderData != NULL );

					DecalRenderData->InitResources_GameThread();

					// Add the decal attachment to the receiver.
					Receiver->AttachDecal( this, DecalRenderData, &DecalState );

					// Add the receiver to this decal.
					FDecalReceiver* NewDecalReceiver = new(DecalReceivers) FDecalReceiver;
					NewDecalReceiver->Component = Receiver;
					NewDecalReceiver->RenderData = DecalRenderData;
				}
			}
		}
	}
}

/**
 * Disconnects the decal from its manager and detaches receivers.
 */
void UDecalComponent::DetachFromReceivers()
{
	for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num() ; ++ReceiverIndex )
	{
		FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
		if ( Receiver.Component )
		{
			// Detach the decal from the primitive.
			Receiver.Component->DetachDecal( this );
			Receiver.Component = NULL;
		}
	}

	// Now that the receiver has been disassociated from the decal, clear its render data.
	ReleaseResources( GIsEditor );
}

/**
* Disconnects the decal from its manager and detaches receivers matching the given primitive component.
* This will also release the resources associated with the receiver component.
*
* @param Receiver	The receiving primitive to detach decal render data for
*/
void UDecalComponent::DetachFromReceiver(UPrimitiveComponent* InReceiver)
{
	if( InReceiver )
	{
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num() ; ++ReceiverIndex )
		{
			FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
			if ( Receiver.Component == InReceiver )
			{
				// Detach the decal from the primitive.
				Receiver.Component->DetachDecal( this );
				Receiver.Component = NULL;
			}
		}

		// Now that the receiver has been disassociated from the decal, clear its render data.
		ReleaseResources( GIsEditor, InReceiver );	
	}
}

void UDecalComponent::Detach( UBOOL bWillReattach )
{
	//@todo - fix DecalTransform_SpawnRelative mode in UDecalComponent::SetParentToWorld
	//bHasBeenAttached = FALSE;
	if ( !GIsUCC )
	{
		DetachFromReceivers();
	}
	Super::Detach( bWillReattach );
}

void UDecalComponent::ReleaseResources(UBOOL bBlockOnRelease, UPrimitiveComponent* InReceiver)
{
	// Iterate over all receivers and enqueue their deletion.
	for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num() ; ++ReceiverIndex )
	{
		FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
		const UBOOL bMatchingReceiver = !InReceiver || (InReceiver && !Receiver.Component);
		if ( Receiver.RenderData && bMatchingReceiver )
		{
			// Ensure the component has already been disassociated from the decal before clearing its render data.
			check( Receiver.Component == NULL );
			// Enqueue deletion of decal render data.
			EnqueueDecalRenderDataDeletion( Receiver.RenderData );
			// No longer safe to access, NULL out.
			Receiver.RenderData = NULL;
		}
		// if an explicit receiver was specified then only remove its entry
		if( InReceiver && bMatchingReceiver )
		{
			DecalReceivers.Remove(ReceiverIndex);
			break;
		}
	}

	if( !InReceiver )
	{
		// Empty DecalReceivers array now that resource deletion has been enqueued.
		DecalReceivers.Empty();
	}

	// Create a fence for deletion of component in case there is a pending init e.g.
	if ( !ReleaseResourcesFence )
	{
		ReleaseResourcesFence = new FRenderCommandFence;
	}
	ReleaseResourcesFence->BeginFence();

	// Wait for fence in case we requested to block on release.
	if( bBlockOnRelease )
	{
		ReleaseResourcesFence->Wait();
	}
}

void UDecalComponent::BeginPlay()
{
	Super::BeginPlay();

	// Static decals in game attach here.
	// Dynamic decals and static decals not in game attach in UDecalComponent::Attach().
	const UBOOL bDetailModeAllowsRendering	= DetailMode <= GSystemSettings.DetailMode;
	if ( bStaticDecal && GIsGame && !GIsUCC && bDetailModeAllowsRendering )
	{
		if ( StaticReceivers.Num() == 0 )
		{
			ComputeReceivers();
		}
		else
		{
			AttachToStaticReceivers();
		}		
		bHasBeenAttached=TRUE;
	}
}

void UDecalComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources( FALSE );
	FreeStaticReceivers();
}

/**
 * Frees any StaticReceivers data.
 */
void UDecalComponent::FreeStaticReceivers()
{
	// Free any static receiver information.
	for ( INT ReceiverIndex = 0 ; ReceiverIndex < StaticReceivers.Num() ; ++ReceiverIndex )
	{
		delete StaticReceivers(ReceiverIndex);
	}
	StaticReceivers.Empty();
}

UBOOL UDecalComponent::IsReadyForFinishDestroy()
{
	if (ReleaseResourcesFence)
	{
		const UBOOL bDecalIsReadyForFinishDestroy = ReleaseResourcesFence->GetNumPendingFences() == 0;
		return bDecalIsReadyForFinishDestroy && Super::IsReadyForFinishDestroy();
	}
	return Super::IsReadyForFinishDestroy();
}

void UDecalComponent::FinishDestroy()
{
	// Finish cleaning up any receiver render data.
	for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num() ; ++ReceiverIndex )
	{
		FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
		delete Receiver.RenderData;
	}
	DecalReceivers.Empty();

	// Delete any existing resource fence.
	delete ReleaseResourcesFence;
	ReleaseResourcesFence = NULL;

	Super::FinishDestroy();
}

UBOOL UDecalComponent::IsWaitingForResetToDefaultsToComplete()
{
	// checks DetachFence and ReleaseResourcesFence
	return IsReadyForFinishDestroy();
}

/** detaches the component and resets the component's properties to the values of its template */
void UDecalComponent::ResetToDefaults( void )
{
	if( !IsTemplate() )
	{
		// needed for resetting parent to world transforms
		bHasBeenAttached=FALSE;
		// make sure we're fully detached 
		DetachFromAny();
		ReleaseResources(FALSE);
		FreeStaticReceivers(); 

		UDecalComponent* Default = GetArchetype<UDecalComponent>();

		// copy all non-native, non-duplicatetransient, non-Component properties we have from all classes up to and including UActorComponent
		for( UProperty* Property = GetClass()->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext )
		{
			if( !( Property->PropertyFlags & CPF_Native ) && !( Property->PropertyFlags & CPF_DuplicateTransient ) && !( Property->PropertyFlags & CPF_Component ) &&
				Property->GetOwnerClass()->IsChildOf( UActorComponent::StaticClass() ) )
			{
				Property->CopyCompleteValue( ( BYTE* )this + Property->Offset, ( BYTE* )Default + Property->Offset, NULL, this );
			}
		}
	}	
}

void UDecalComponent::SetDecalMaterial(class UMaterialInterface* NewDecalMaterial)
{
	DecalMaterial = NewDecalMaterial;
	BeginDeferredReattach();
}

class UMaterialInterface* UDecalComponent::GetDecalMaterial() const
{
	return DecalMaterial;
}

void UDecalComponent::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	// Mitigate receiver attachment cost for static decals by storing off receiver render data.
	// Don't save static receivers when cooking because intersection queries with potential receivers
	// may not function properly.  Instead, we require static receivers to have been computed when the
	// level was saved in the editor.
	if ( bStaticDecal && !GIsUCC && !bMovableDecal )
	{
		FreeStaticReceivers();

		// only process decals in actors with a level outer (ie. not prefabs)
		ULevel* OwnerLevel = GetOwner() ? Cast<ULevel>(GetOwner()->GetOuter()) : NULL;
		if( OwnerLevel )
		{
			// always generate data before saving to store in static receivers
			if( !GIsGame )
			{
				TGuardValue<UBOOL> ForceEnableForSave(bForceEnableForSave,TRUE);
				DetachFromReceivers();
				ComputeReceivers();
			}

			for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num(); ++ReceiverIndex )
			{
				FDecalReceiver& DecalReceiver = DecalReceivers(ReceiverIndex);

				if( DecalReceiver.Component && 
					DecalReceiver.RenderData &&
					DecalReceiver.RenderData->NumTriangles > 0 )
				{
					// find the decal interaction matching this receiver
					FStaticReceiverData* NewStaticReceiver	= new FStaticReceiverData;
					NewStaticReceiver->Component			= DecalReceiver.Component;
					CopyDecalRenderDataToStaticReceiverData( *NewStaticReceiver, *DecalReceiver.RenderData );

					StaticReceivers.AddItem( NewStaticReceiver );
				}
			}
			StaticReceivers.Shrink();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * @return		TRUE if the application filter passes the specified component, FALSE otherwise.
 */
UBOOL UDecalComponent::FilterComponent(UPrimitiveComponent* Component) const
{
	UBOOL bResult = TRUE;

	const AActor* TheOwner = Component->GetOwner();
	if ( !TheOwner )
	{
		// Actors with no owners fail if the filter is an affect filter.
		if ( FilterMode == FM_Affect )
		{
			bResult = FALSE;
		}
	}
	else
	{
		// The actor has an owner; pass it through the filter.
		if ( FilterMode == FM_Ignore )
		{
			// Reject if the component is in the filter.
			bResult = !Filter.ContainsItem( const_cast<AActor*>(TheOwner) );
		}
		else if ( FilterMode == FM_Affect )
		{
			// Accept if the component is in the filter.
			bResult = Filter.ContainsItem( const_cast<AActor*>(TheOwner) );
		}
	}

	return bResult;
}

/**
* Determines whether the proxy for this primitive type needs to be recreated whenever the primitive moves.
* @return TRUE to recreate the proxy when UpdateTransform is called.
*/
UBOOL UDecalComponent::ShouldRecreateProxyOnUpdateTransform() const
{
#if !FINAL_RELEASE
	// always recreate proxy for decals in order to update frustum verts
	return TRUE;	
#else
	// no proxy for FR
	return FALSE;	
#endif // !FINAL_RELEASE
	
}

/**
 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
 *
 * @return The proxy object.
 */
FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{	
#if !FINAL_RELEASE
	return new FDecalSceneProxy( this );
#else
	return NULL;
#endif // !FINAL_RELEASE
}

/**
 * Sets the component's bounds based on the vertices of the decal frustum.
 */
void UDecalComponent::UpdateBounds()
{
	FVector Verts[8];
	GenerateDecalFrustumVerts( Verts );

	Bounds = FBoxSphereBounds( FBox( Verts, 8 ) );

	// Expand the bounds slightly to prevent false occlusion.
	static FLOAT s_fOffset	= 1.0f;
	static FLOAT s_fScale	= 1.1f;
	const FVector Value(Bounds.BoxExtent.X + s_fOffset, Bounds.BoxExtent.Y + s_fOffset, Bounds.BoxExtent.Z + s_fOffset);
	Bounds = FBoxSphereBounds(Bounds.Origin,(Bounds.BoxExtent + FVector(s_fOffset)) * s_fScale,(Bounds.SphereRadius + s_fOffset) * s_fScale);
}

/**
 * Enumerates the streaming textures used by the primitive.
 * @param OutStreamingTextures - Upon return, contains a list of the streaming textures used by the primitive.
 */
void UDecalComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	UMaterialInterface* Material = GetDecalMaterial();
	if ( Material )
	{
		TArray<UTexture*> Textures;
		Material->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);

		const FSphere BoundingSphere	= Bounds.GetSphere();
		const FLOAT LocalTexelFactor	= Max( Abs(Width), Max(Abs(Height), Abs(FarPlane-NearPlane)) );
		const FLOAT WorldTexelFactor	= LocalTexelFactor * LocalToWorld.GetMaximumAxisScale();

		// Add each texture to the output with the appropriate parameters.
		// TODO: Take into account which UVIndex is being used.
		for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
		{
			FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
			StreamingTexture.Bounds = BoundingSphere;
			StreamingTexture.TexelFactor = WorldTexelFactor * StreamingDistanceMultiplier;
			StreamingTexture.Texture = Textures(TextureIndex);
		}
	}
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UDecalComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.AddItem( GetDecalMaterial() );
}

/**
 * Fills in the specified vertex list with the local-space decal frustum vertices.
 */
void UDecalComponent::GenerateDecalFrustumVerts(FVector Verts[8]) const
{
	const FLOAT HalfWidth = Width/2.f;
	const FLOAT HalfHeight = Height/2.f;
	Verts[0] = Location + (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	Verts[1] = Location + (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	Verts[2] = Location - (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	Verts[3] = Location - (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	Verts[4] = Location + (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * FarPlane);
	Verts[5] = Location + (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * FarPlane);
	Verts[6] = Location - (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * FarPlane);
	Verts[7] = Location - (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * FarPlane);
}

/**
 * Fills in the specified decal state object with this decal's state.
 */
void UDecalComponent::CaptureDecalState(FDecalState* DecalState) const
{
	DecalState->DecalComponent = this;

	// Capture the decal material, or the default material if no material was specified.
	DecalState->DecalMaterial = DecalMaterial;

	if( !DecalState->DecalMaterial )
	{
		warnf( TEXT("Decal (%s) was missing a material and DefaultDecalMaterial was used!"), *GetFullName() );
		DecalState->DecalMaterial = GEngine->DefaultDecalMaterial;
	}

	// Materials used on decals must set the MATUSAGE_Decals and MATUSAGE_StaticLighting usage flags.
	// Force them on here or generate a warning (and use the default material) if not possible.
	if(!DecalState->DecalMaterial->CheckMaterialUsage(MATUSAGE_Decals))
	{
		warnf( TEXT("Decal (%s) had a material with invalid usage (bUsedWithDecals==0), DefaultDecalMaterial was used!"),
				*DecalState->DecalMaterial->GetFullName() );
		DecalState->DecalMaterial = GEngine->DefaultDecalMaterial;
	}
	if(bStaticDecal && !DecalState->DecalMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting))
	{
		warnf( TEXT("Decal (%s) had a material with invalid usage (bUsedWithStaticLighting==0), DefaultDecalMaterial was used!"),
				*DecalState->DecalMaterial->GetFullName() );
		DecalState->DecalMaterial = GEngine->DefaultDecalMaterial;
	}
	if(bProjectOnSkeletalMeshes && !DecalState->DecalMaterial->CheckMaterialUsage(MATUSAGE_SkeletalMesh, TRUE))
	{
		warnf( TEXT("Decal (%s) had a material with invalid usage (bUsedWithSkeletalMesh==0), DefaultDecalMaterial was used!"),
			*DecalState->DecalMaterial->GetFullName() );
		DecalState->DecalMaterial = GEngine->DefaultDecalMaterial;
	}

	// Special engine materials don't cache decal vertex factory shaders on platforms with AllowDebugViewmodes() returning FALSE
	// A special engine material will only be the base material if DecalState->DecalMaterial is setup incorrectly, for example a NULL parent, 
	// In which case we want to fall back to the default decal material.
	if( !DecalState->DecalMaterial->GetMaterial() 
		|| (DecalState->DecalMaterial->GetMaterial()->bUsedAsSpecialEngineMaterial && DecalState->DecalMaterial->GetMaterial() != GEngine->DefaultDecalMaterial))
	{
		warnf( TEXT("Decal material %s was using base material %s, which was a special engine material!"), 
			*DecalState->DecalMaterial->GetPathName(), 
			*DecalState->DecalMaterial->GetMaterial()->GetPathName());
		DecalState->DecalMaterial = GEngine->DefaultDecalMaterial;
	}

	// Compute the decal state's material view relevance flags.
	DecalState->MaterialViewRelevance = DecalState->DecalMaterial->GetViewRelevance();

	DecalState->OrientationVector = Orientation.Vector();
	DecalState->HitLocation = HitLocation;
	DecalState->HitNormal = HitNormal;
	DecalState->HitTangent = HitTangent;
	DecalState->HitBinormal = HitBinormal;
	DecalState->OffsetX = OffsetX;
	DecalState->OffsetY = OffsetY;

	DecalState->Width = Width;
	DecalState->Height = Height;
	DecalState->NearPlaneDistance = NearPlane;
	DecalState->FarPlaneDistance = FarPlane;

	DecalState->DepthBias = DepthBias;
	DecalState->SlopeScaleDepthBias = SlopeScaleDepthBias;
	DecalState->SortOrder = SortOrder;

	DecalState->Bounds = Bounds.GetBox();
	if( bStaticDecal )
	{
		DecalState->SquaredCullDistance = CachedMaxDrawDistance * CachedMaxDrawDistance;
	}
	else
	{
		// scale cull distance by global system setting
		DecalState->SquaredCullDistance = CachedMaxDrawDistance * CachedMaxDrawDistance * GSystemSettings.DecalCullDistanceScale * GSystemSettings.DecalCullDistanceScale;
	}

	DecalState->Planes = Planes;
	DecalState->WorldTexCoordMtx = FMatrix( HitTangent * (TileX/Width),
											HitBinormal * (TileY/Height),
											HitNormal,
											FVector(0.f,0.f,0.f) ).Transpose();
	DecalState->HitBone = HitBone;
	DecalState->HitBoneIndex = INDEX_NONE;
	if( HitNodeIndex != INDEX_NONE )
	{
		DecalState->HitNodeIndices.Empty(1);
		DecalState->HitNodeIndices.AddItem(HitNodeIndex);
	}
	else
	{
		DecalState->HitNodeIndices = HitNodeIndices;
	}
	DecalState->HitLevelIndex = HitLevelIndex;
	DecalState->FracturedStaticMeshComponentIndex = FracturedStaticMeshComponentIndex;

	DecalState->DepthPriorityGroup = DepthPriorityGroup;

	// Store whether or not we want the decal to be clipped at render time
	// NOTE: bUseSoftwareClip may be be overridden for static meshes by UStaticMeshComponent::GenerateDecalRenderData()
	DecalState->bNoClip = bNoClip;
	DecalState->bUseSoftwareClip = !bNoClip;


	DecalState->bProjectOnBackfaces = bProjectOnBackfaces;
	DecalState->bFlipBackfaceDirection = bFlipBackfaceDirection;
	DecalState->bProjectOnBSP = bProjectOnBSP;
	DecalState->bProjectOnStaticMeshes = bProjectOnStaticMeshes;
	DecalState->bProjectOnSkeletalMeshes = bProjectOnSkeletalMeshes;
	DecalState->bProjectOnTerrain = bProjectOnTerrain;
	DecalState->bStaticDecal = bStaticDecal;
	DecalState->bMovableDecal = bMovableDecal;

	DecalState->bDecalMaterialHasStaticLightingUsage = (
		DecalState->DecalMaterial && 
		DecalState->DecalMaterial->GetMaterial() &&
		DecalState->DecalMaterial->GetMaterial()->GetUsageByFlag(MATUSAGE_StaticLighting) &&
		DecalState->DecalMaterial->GetMaterial()->LightingModel != MLM_Unlit
		);

	DecalState->bDecalMaterialHasUnlitLightingModel = (
		DecalState->DecalMaterial && 
		DecalState->DecalMaterial->GetMaterial() &&
		DecalState->DecalMaterial->GetMaterial()->LightingModel == MLM_Unlit
		);


	// Compute frustum verts.
	const FLOAT HalfWidth = Width/2.f;
	const FLOAT HalfHeight = Height/2.f;
	DecalState->FrustumVerts[0] = HitLocation + (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	DecalState->FrustumVerts[1] = HitLocation + (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	DecalState->FrustumVerts[2] = HitLocation - (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	DecalState->FrustumVerts[3] = HitLocation - (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * NearPlane);
	DecalState->FrustumVerts[4] = HitLocation + (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * FarPlane);
	DecalState->FrustumVerts[5] = HitLocation + (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * FarPlane);
	DecalState->FrustumVerts[6] = HitLocation - (HitBinormal * HalfHeight) - (HitTangent * HalfWidth) - (HitNormal * FarPlane);
	DecalState->FrustumVerts[7] = HitLocation - (HitBinormal * HalfHeight) + (HitTangent * HalfWidth) - (HitNormal * FarPlane);
}

/** 
* convert interval specified in degrees to clamped dot product range
* @return min,max range of blending decal
*/
FVector2D UDecalComponent::CalcDecalDotProductBlendRange() const
{
	// decal component interval specified in degrees. convert to dot product range
	FVector2D BlendMinMaxCos( appCos(BlendRange.X * PI/180.f), appCos(BlendRange.Y * PI/180.f) );
	// make sure min < max
	FVector2D BlendMinMax( Min<FLOAT>(BlendMinMaxCos.X,BlendMinMaxCos.Y), Max<FLOAT>(BlendMinMaxCos.X,BlendMinMaxCos.Y) );
	// make sure min != max
	if( (BlendMinMax.Y - BlendMinMax.X) < (KINDA_SMALL_NUMBER*2) )
	{	
		BlendMinMax.X -= SMALL_NUMBER;
	}
	// clamp to [-1,1]
	BlendMinMax.X = Clamp<FLOAT>(BlendMinMax.X,-1.f,1.f);
	BlendMinMax.Y = Clamp<FLOAT>(BlendMinMax.Y,-1.f,1.f);

	return BlendMinMax;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Serialization
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UDecalComponent::Serialize(FArchive& Ar)
{
	Super::Serialize( Ar );

	if ( Ar.IsLoading() )
	{
		/////////////////////////
		// Loading.

		// Load number of static receivers from archive.
		INT NumStaticReceivers = 0;
		Ar << NumStaticReceivers;
		// Free existing static receivers.
		FreeStaticReceivers();
		StaticReceivers.AddZeroed(NumStaticReceivers);
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < NumStaticReceivers ; ++ReceiverIndex )
		{
			// Allocate new static receiver data.
			FStaticReceiverData* NewStaticReceiver = new FStaticReceiverData;
			// Fill in its members from the archive.
			Ar << *NewStaticReceiver;
			// Add it to the list of static receivers.
			StaticReceivers(ReceiverIndex) = NewStaticReceiver;
		}
	}
	else if ( Ar.IsSaving() )
	{
		/////////////////////////
		// Saving.
		
		// Ignore entries for statis receivers which haven't been updated yet and have invalid components
		// These should be updated later during PreSave
		INT NumStaticReceivers = 0;	
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < StaticReceivers.Num(); ++ReceiverIndex )
		{
			FStaticReceiverData* StaticReceiver = StaticReceivers(ReceiverIndex);
			if( StaticReceiver && StaticReceiver->Component )
			{
				NumStaticReceivers++;																
			}
		}

		// Save number of static receivers to archive.
		Ar << NumStaticReceivers;

		// Write each receiver to the archive.
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < StaticReceivers.Num(); ++ReceiverIndex )
		{
			FStaticReceiverData* StaticReceiver = StaticReceivers(ReceiverIndex);
			if( StaticReceiver && StaticReceiver->Component )
			{
				Ar << *StaticReceiver;
			}
		}
	}
	else if ( Ar.IsObjectReferenceCollector() )
	{
		// When collecting object references, be sure to include the components referenced via StaticReceivers.
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < StaticReceivers.Num() ; ++ReceiverIndex)
		{
			FStaticReceiverData* StaticReceiver = StaticReceivers(ReceiverIndex);
			if (StaticReceiver != NULL)
			{
				Ar << StaticReceiver->Component;
				for (INT ShadowMapIdx=0; ShadowMapIdx < StaticReceiver->ShadowMap1D.Num(); ShadowMapIdx++)
				{
					if (StaticReceiver->ShadowMap1D(ShadowMapIdx) != NULL)
					{
						Ar << StaticReceiver->ShadowMap1D(ShadowMapIdx);
					}
				}
			}
		}
		// Dynamic receivers also
		for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num() ; ++ReceiverIndex)
		{
			FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
			Ar << Receiver.Component;
			if (Receiver.RenderData != NULL)
			{
				for (INT ShadowMapIdx=0; ShadowMapIdx < Receiver.RenderData->ShadowMap1D.Num(); ShadowMapIdx++)
				{
					if (Receiver.RenderData->ShadowMap1D(ShadowMapIdx) != NULL)
					{
						Ar << Receiver.RenderData->ShadowMap1D(ShadowMapIdx);
					}
				}
			}
		}		
	}
}

/**
* Count memory used by the decal including the vertex data generated for each of its receivers
* @return size of memory used by this decal for display in editor
*/
INT	UDecalComponent::GetResourceSize(void)
{
	INT ResourceSize = 0;
	if (!GExclusiveResourceSizeMode)
	{	
		FArchiveCountMem CountBytesSize( this );
		ResourceSize += CountBytesSize.GetNum();
	}

	for( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num(); ++ReceiverIndex )
	{
		const FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
		if( Receiver.RenderData )
		{
			ResourceSize += Receiver.RenderData->GetMemoryUsage();
		}
	}
	return ResourceSize;
}

/**
 * Called when a property on this object has been modified externally
 *
 * @param PropertyThatChanged the property that was modified
 */
void UDecalComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if ( GIsEditor && PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("StreamingDistanceMultiplier") )
	{
		// Recalculate in a few seconds.
		ULevel::TriggerStreamingDataRebuild();
	}

	bHasBeenAttached = FALSE;
	BeginDeferredReattach();
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UDecalComponent::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	// When collecting object references, be sure to include the components referenced via StaticReceivers.
	for ( INT ReceiverIndex = 0 ; ReceiverIndex < StaticReceivers.Num() ; ++ReceiverIndex)
	{
		FStaticReceiverData* StaticReceiver = StaticReceivers(ReceiverIndex);
		if (StaticReceiver != NULL)
		{
			if (StaticReceiver->Component != NULL)
			{
				AddReferencedObject( ObjectArray, StaticReceiver->Component );
			}
			for (INT ShadowMapIdx=0; ShadowMapIdx < StaticReceiver->ShadowMap1D.Num(); ShadowMapIdx++)
			{
				if (StaticReceiver->ShadowMap1D(ShadowMapIdx) != NULL)
				{
					AddReferencedObject( ObjectArray, StaticReceiver->ShadowMap1D(ShadowMapIdx) );
				}
			}
		}
	}
	// Dynamic receivers also
	for ( INT ReceiverIndex = 0 ; ReceiverIndex < DecalReceivers.Num() ; ++ReceiverIndex)
	{
		FDecalReceiver& Receiver = DecalReceivers(ReceiverIndex);
		if (Receiver.Component != NULL)
		{
			AddReferencedObject( ObjectArray, Receiver.Component );
		}
		if (Receiver.RenderData != NULL)
		{
			for (INT ShadowMapIdx=0; ShadowMapIdx < Receiver.RenderData->ShadowMap1D.Num(); ShadowMapIdx++)
			{
				if (Receiver.RenderData->ShadowMap1D(ShadowMapIdx) != NULL)
				{
					AddReferencedObject( ObjectArray, Receiver.RenderData->ShadowMap1D(ShadowMapIdx) );
				}
			}
		}
	}
}
