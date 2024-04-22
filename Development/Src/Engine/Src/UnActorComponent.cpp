/*=============================================================================
	UnActorComponent.cpp: Actor component implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"

#define PERF_LOG_DETAILED_COMPONENT_UPDATE_STATS !FINAL_RELEASE && (LOOKING_FOR_PERF_ISSUES || 0)
#define PERF_LOG_DETAILED_ACTOR_UPDATE_STATS		!FINAL_RELEASE && (LOOKING_FOR_PERF_ISSUES || 0)

/** Global boolean to toggle the log of detailed tick stats. */
/** Needs PERF_LOG_DETAILED_COMPONENT_UPDATE_STATS to be 1 **/
UBOOL GLogDetailedComponentUpdateStats = TRUE; 

/** Global boolean to toggle the log of detailed tick stats. */
/** Needs PERF_LOG_DETAILED_ACTOR_UPDATE_STATS to be 1 **/
UBOOL GLogDetailedActorUpdateStats = TRUE; 

#if LOOKING_FOR_PERF_ISSUES
#define NUM_FRAMES_BETWEEN_LOG_DETAILS 7000
#else
#define NUM_FRAMES_BETWEEN_LOG_DETAILS 1
#endif

#if PERF_LOG_DETAILED_COMPONENT_UPDATE_STATS
/** Global detailed update stats. */
static FDetailedTickStats GDetailedComponentUpdateStats( 20, 10, 1, 8, TEXT("updating") );
#define PERF_TRACK_DETAILED_COMPONENT_UPDATE_STATS(Object)	FScopedDetailTickStats DetailedTickStats(GDetailedComponentUpdateStats,Object);
#else
#define PERF_TRACK_DETAILED_COMPONENT_UPDATE_STATS(Object)
#endif

#if PERF_LOG_DETAILED_ACTOR_UPDATE_STATS
/** Global detailed update stats. */
static FDetailedTickStats GDetailedActorUpdateStats( 20, 10, 1, 8, TEXT("updating") );
#define TRACK_DETAILED_ACTOR_UPDATE_STATS(Object)	FScopedDetailTickStats DetailedTickStats(GDetailedActorUpdateStats,Object);
#else
#define TRACK_DETAILED_ACTOR_UPDATE_STATS(Object)
#endif

// type FRAMECOMPUPDATES  to get the data
#define PERF_ENABLE_FRAME_COMPUPDATE_DUMP LOOKING_FOR_PERF_ISSUES || (0) 

IMPLEMENT_CLASS(UActorComponent);
IMPLEMENT_CLASS(UAudioComponent);

IMPLEMENT_CLASS(UWindDirectionalSourceComponent);
IMPLEMENT_CLASS(AWindPointSource);
IMPLEMENT_CLASS(UWindPointSourceComponent);

IMPLEMENT_CLASS(UPrimitiveComponentFactory);
IMPLEMENT_CLASS(UMeshComponentFactory);
IMPLEMENT_CLASS(UStaticMeshComponentFactory);

/** Static var indicating activity of reattach context */
INT FGlobalComponentReattachContext::ActiveGlobalReattachContextCount = 0;

/** 
* Initialization constructor. 
*/
FGlobalComponentReattachContext::FGlobalComponentReattachContext()
{
	ActiveGlobalReattachContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach all actor components.
	for(TObjectIterator<UActorComponent> ComponentIt;ComponentIt;++ComponentIt)
	{
		new(ComponentContexts) FComponentReattachContext(*ComponentIt);		
	}

	GEngine->IssueDecalUpdateRequest();
}

/** 
* Initialization constructor. 
*
* @param ExcludeComponents - Component types to exclude when reattaching 
*/
FGlobalComponentReattachContext::FGlobalComponentReattachContext(const TArray<UClass*>& ExcludeComponents)
{
	ActiveGlobalReattachContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach only actor components that are not in the excluded list
	for(TObjectIterator<UActorComponent> ComponentIt;ComponentIt;++ComponentIt)
	{
		UBOOL bShouldReattach=TRUE;
		for( INT Idx=0; Idx < ExcludeComponents.Num(); Idx++ )
		{
			UClass* ExcludeClass = ExcludeComponents(Idx);
			if( ExcludeClass &&
				ComponentIt->IsA(ExcludeClass) )
			{
				bShouldReattach = FALSE;
				break;
			}
		}
		if( bShouldReattach )
		{
			new(ComponentContexts) FComponentReattachContext(*ComponentIt);		
		}
	}

	GEngine->IssueDecalUpdateRequest();
}

	/**
	 * Initialization constructor
	 * Only re-attach those components whose replacement primitive is in a direct child of one of the InParentActors
	 * 
	 * @param InParentActors - list of actors called out for reattachment
	 */
FGlobalComponentReattachContext::FGlobalComponentReattachContext(const TArray<AActor*>& InParentActors)
{
	ActiveGlobalReattachContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach only actor components that are children of the actors list provided
	for(TObjectIterator<UActorComponent> ComponentIt;ComponentIt;++ComponentIt)
	{
		UBOOL bShouldReattach=FALSE;
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(*ComponentIt);
		if (PrimitiveComponent && PrimitiveComponent->ReplacementPrimitive)
		{
			UPrimitiveComponent* ReplacementPrimitive = PrimitiveComponent->ReplacementPrimitive;
			AActor* ParentActor = Cast<AActor>(ReplacementPrimitive->GetOuter());
			if (ParentActor && InParentActors.ContainsItem(ParentActor))
			{
				bShouldReattach = TRUE;
			}
		}
		if( bShouldReattach )
		{
			new(ComponentContexts) FComponentReattachContext(*ComponentIt);		
		}
	}

	GEngine->IssueDecalUpdateRequest();
}


FGlobalComponentReattachContext::~FGlobalComponentReattachContext()
{
	check(ActiveGlobalReattachContextCount > 0);
	// We empty the array now, to ensure that the FComponentReattachContext destructors are called while ActiveGlobalReattachContextCount still indicates activity
	ComponentContexts.Empty();
	ActiveGlobalReattachContextCount--;
}

/**
 * Constructor, removing primitive from scene and caching it if we need to readd .
 */
FPrimitiveSceneAttachmentContext::FPrimitiveSceneAttachmentContext(UPrimitiveComponent* InPrimitive )
:	Scene(NULL)
{
	check(InPrimitive);
	checkf(!InPrimitive->HasAnyFlags(RF_Unreachable), TEXT("%s"), *InPrimitive->GetFullName());
	if( (InPrimitive->IsAttached() || !InPrimitive->IsValidComponent()) && InPrimitive->GetScene() )
	{
		Primitive	= InPrimitive;
		Scene		= Primitive->GetScene();
		Scene->RemovePrimitive( Primitive, FALSE );
	}
	else
	{
		Primitive = NULL;
	}
}

/**
 * Destructor, adding primitive to scene again if needed. 
 */
FPrimitiveSceneAttachmentContext::~FPrimitiveSceneAttachmentContext()
{
	if( Primitive && Primitive->IsValidComponent() )
	{
		const UBOOL bShowInEditor				= !(Primitive->HiddenEditor);
		const UBOOL bShowInGame					= !(Primitive->HiddenGame);
		const UBOOL bDetailModeAllowsRendering	= Primitive->DetailMode <= GSystemSettings.DetailMode;
		if ( bDetailModeAllowsRendering && ((GIsGame && bShowInGame) || (!GIsGame && bShowInEditor)) )
		{
			Scene->AddPrimitive( Primitive );
		}
	}
}


void AActor::ClearComponents()
{
	for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		UActorComponent* Component = Components(ComponentIndex); 
		if( Component )
		{
			Component->ConditionalDetach();
		}
	}
}

/**
 * Works through the component arrays marking entries as pending kill so references to them
 * will be NULL'ed.
 *
 * @param	bAllowComponentOverride		Whether to allow component to override marking the setting
 */
void AActor::MarkComponentsAsPendingKill( UBOOL bAllowComponentOverride )
{
	// Iterate over Components array and mark entries as pending kill.
	for (INT Index = 0; Index < Components.Num(); Index++)
	{
		UActorComponent* Component = Components(Index);
		if( Component != NULL )
		{
			// Modify component so undo/ redo works in the editor.
			if( GIsEditor )
			{
				Component->Modify();
			}
			// Mark as pending kill if forced or component allows it.
			if( !bAllowComponentOverride || Component->AllowBeingMarkedPendingKill() )
			{
				Component->MarkPendingKill();
			}
		}
	}
	// Iterate over AllComponents array and mark entries as pending kill.
	for (INT Index = 0; Index < AllComponents.Num(); Index++)
	{
		UActorComponent* Component = AllComponents(Index);
		if( Component != NULL )
		{
			// Modify component so undo/ redo works in the editor.
			if( GIsEditor )
			{
				Component->Modify();
			}
			// Mark as pending kill if forced or component allows it.
			if( !bAllowComponentOverride || Component->AllowBeingMarkedPendingKill() )
			{
				Component->MarkPendingKill();
			}
		}
	}
}

/**
 * Flags all components as dirty so that they will be guaranteed an update from
 * AActor::Tick(), and also be conditionally reattached by AActor::ConditionalUpdateComponents().
 * @param	bTransformOnly	- True if only the transform has changed.
 */
void AActor::MarkComponentsAsDirty(UBOOL bTransformOnly)
{
	// Make a copy of the AllComponents array, since BeginDeferredReattach may change the order by reattaching the component.
	TArray<UActorComponent*,TInlineAllocator<32> > LocalAllComponents = AllComponents;

	for (INT Idx = 0; Idx < LocalAllComponents.Num(); Idx++)
	{
		if (LocalAllComponents(Idx) != NULL)
		{
			if (bStatic)
			{
				LocalAllComponents(Idx)->ConditionalDetach();
			}
			else
			{
				if(bTransformOnly)
				{
					LocalAllComponents(Idx)->BeginDeferredUpdateTransform();
				}
				else
				{
					LocalAllComponents(Idx)->BeginDeferredReattach();
				}
			}
		}
	}

	if (bStatic  && !IsPendingKill())
	{
		ConditionalUpdateComponents(FALSE);
	}
}

/**
 * Verifies that neither this actor nor any of its components are RF_Unreachable and therefore pending
 * deletion via the GC.
 *
 * @return TRUE if no unreachable actors are referenced, FALSE otherwise
 */
UBOOL AActor::VerifyNoUnreachableReferences()
{
	UBOOL bHasUnreachableReferences = FALSE;
	extern UBOOL GShouldVerifyGCAssumptions;
	if( GShouldVerifyGCAssumptions )
	{
		// Check object itself.
		if( HasAnyFlags(RF_Unreachable) )
		{
			bHasUnreachableReferences = TRUE;
		}
		// Check components in Components array.
		for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
		{
			UActorComponent* Component = Components(ComponentIndex); 
			if( Component && Component->HasAnyFlags(RF_Unreachable) )
			{
				bHasUnreachableReferences = TRUE;
			}
		}
		// Check components in AllComponents array.
		for(INT ComponentIndex = 0;ComponentIndex < AllComponents.Num();ComponentIndex++)
		{
			UActorComponent* Component = AllComponents(ComponentIndex); 
			if( Component && Component->HasAnyFlags(RF_Unreachable) )
			{
				bHasUnreachableReferences = TRUE;
			}
		}

		// Detailed logging of culprit.
		if( bHasUnreachableReferences )
		{
			debugf(TEXT("Actor '%s' has references to unreachable objects."), *GetFullName());
			debugf(TEXT("%s  Actor             Flags: 0x%016I64X  %s"), HasAnyFlags(RF_Unreachable) ? TEXT("X") : TEXT(" "), GetFlags(), *GetFullName());

			// Components array.
			for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
			{
				UActorComponent* Component = Components(ComponentIndex); 
				if( Component )
				{
					debugf(TEXT("%s  Components    %2i  Flags: 0x%016I64X  %s"), 
						Component->HasAnyFlags(RF_Unreachable) ? TEXT("X") : TEXT(" "), 
						ComponentIndex,
						Component->GetFlags(), 
						*Component->GetFullName());
				}
				else
				{
					debugf(TEXT("%s  Components    %2i"), TEXT(" "), ComponentIndex);
				}
			}
			// AllComponents array.
			for(INT ComponentIndex = 0;ComponentIndex < AllComponents.Num();ComponentIndex++)
			{
				UActorComponent* Component = AllComponents(ComponentIndex); 
				if( Component )
				{
					debugf(TEXT("%s  AllComponents %2i  Flags: 0x%016I64X  %s"), 
						Component->HasAnyFlags(RF_Unreachable) ? TEXT("X") : TEXT(" "), 
						ComponentIndex,
						Component->GetFlags(), 
						*Component->GetFullName());
				}
				else
				{
					debugf(TEXT("%s  AllComponents %2i"), TEXT(" "), ComponentIndex);
				}
			}
		}
	}
	return !bHasUnreachableReferences;
}

void AActor::ConditionalUpdateComponents(UBOOL bCollisionUpdate)
{
#if VERIFY_NO_UNREACHABLE_OBJECTS_ARE_REFERENCED
	// Verify that actor has no references to unreachable components.
	VerifyNoUnreachableReferences();
#endif
#if PERF_LOG_DETAILED_ACTOR_UPDATE_STATS
	static QWORD LastFrameCounter = 0;
	if( GLogDetailedActorUpdateStats && (LastFrameCounter != GFrameCounter) && !(GFrameCounter % NUM_FRAMES_BETWEEN_LOG_DETAILS) )
	{
		GDetailedActorUpdateStats.DumpStats();
		GDetailedActorUpdateStats.Reset();
		LastFrameCounter = GFrameCounter;
	}
#endif

	SCOPE_CYCLE_COUNTER(STAT_UpdateComponentsTime);

	// Don't update components on destroyed actors and default objects/ archetypes.
	if( !ActorIsPendingKill()
	&&	!HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) )
	{
		TRACK_DETAILED_ACTOR_UPDATE_STATS(this);
		UpdateComponentsInternal( bCollisionUpdate );
	}
}

void AActor::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetFullName());
	checkf(!HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject), TEXT("%s"), *GetFullName());
	checkf(!ActorIsPendingKill(), TEXT("%s"), *GetFullName());

	const FMatrix&	ActorToWorld = LocalToWorld();

	// if it's a collision only update
	if (bCollisionUpdate)
	{
		// then only bother with components asking for updates, or the collision component
		for(UINT ComponentIndex = 0;ComponentIndex < (UINT)this->Components.Num();ComponentIndex++)
		{
			UPrimitiveComponent*	primComp = Cast<UPrimitiveComponent>(this->Components(ComponentIndex));

			if( primComp != NULL && primComp->IsAttached() && (primComp == CollisionComponent || primComp->AlwaysCheckCollision) )
			{
				primComp->UpdateComponent(GWorld->Scene,this,ActorToWorld,TRUE);
			}
		}
	}
	else
	{
		// Look for components which should be directly attached to the actor, but aren't yet.
		for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
		{
			UActorComponent* Component = Components(ComponentIndex); 
			if( ComponentIndex+2 < Components.Num() )
			{
				CONSOLE_PREFETCH(Components(ComponentIndex+2));
			}
			if( Component )
			{
				Component->UpdateComponent(GWorld->Scene,this,ActorToWorld);
			}
		}
	}
}

/**
 * Flags all components as dirty and then calls UpdateComponents().
 *
 * @param	bCollisionUpdate	[opt] As per UpdateComponents; defaults to FALSE.
	 * @param	bTransformOnly		[opt] TRUE to update only the component transforms, FALSE to update the entire component.
 */
void AActor::ForceUpdateComponents(UBOOL bCollisionUpdate,UBOOL bTransformOnly)
{
	MarkComponentsAsDirty(bTransformOnly);
	ConditionalUpdateComponents( bCollisionUpdate );
}

/**
 * Flags all components as dirty if in the editor, and then calls UpdateComponents().
 *
 * @param	bCollisionUpdate	[opt] As per UpdateComponents; defaults to FALSE.
	 * @param	bTransformOnly		[opt] TRUE to update only the component transforms, FALSE to update the entire component.
 */
void AActor::ConditionalForceUpdateComponents(UBOOL bCollisionUpdate,UBOOL bTransformOnly)
{
	if ( GIsEditor )
	{
		MarkComponentsAsDirty(bTransformOnly);
	}
	ConditionalUpdateComponents( bCollisionUpdate );
}

void AActor::InvalidateLightingCache()
{
	// Make a copy of AllComponents as calling InvalidateLightingCache on some components will cause them to reattach, modifying AllComponents
	TArray<UActorComponent*> TempAllComponents = AllComponents;
	for(INT ComponentIndex = 0;ComponentIndex < TempAllComponents.Num();ComponentIndex++)
	{
		if(TempAllComponents(ComponentIndex))
		{
			TempAllComponents(ComponentIndex)->InvalidateLightingCache();
		}
	}
}

UBOOL AActor::ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	for (INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(ComponentIndex));
		if (Primitive != NULL && Primitive->ShouldCollide() && !Primitive->LineCheck(Result, End, Start, Extent, TraceFlags))
		{
			// we stop at the first hit detected, regardless of whether it's the closest
			return FALSE;
		}
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// ACTOR COMPONENT
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UActorComponent::CheckForErrors()
{
	if (Owner != NULL && GetClass()->ClassFlags & CLASS_Deprecated)
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_DeprecatedClass" ), *GetName(), *Owner->GetName() ) ), TEXT( "DeprecatedClass" ) );
	}

	if (Owner != NULL && GetClass()->ClassFlags & CLASS_Abstract)
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, Owner, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_AbstractClass" ), *GetName(), *Owner->GetName() ) ), TEXT( "AbstractClass" ) );
	}
}
#endif

UBOOL UActorComponent::IsOwnerSelected() const
{
	return Owner && Owner->IsSelected();
}

void UActorComponent::BeginDestroy()
{
	ConditionalDetach();
	Super::BeginDestroy();
}

/** Do not load this component if our Outer (the Actor) is not needed. */
UBOOL UActorComponent::NeedsLoadForClient() const
{
	check(GetOuter());
	return (GetOuter()->NeedsLoadForClient() && Super::NeedsLoadForClient());
}

/** Do not load this component if our Outer (the Actor) is not needed. */
UBOOL UActorComponent::NeedsLoadForServer() const
{
	check(GetOuter());
	return (GetOuter()->NeedsLoadForServer() && Super::NeedsLoadForServer());
}

/** FComponentReattachContexts for components which have had PreEditChange called but not PostEditChange. */
static TMap<UActorComponent*,FComponentReattachContext*> EditReattachContexts;

void UActorComponent::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if(IsAttached())
	{
		// The component or its outer could be pending kill when calling PreEditChange when applying a transaction.
		// Don't do do a full recreate in this situation, and instead simply detach.
		if( !IsPendingKill() )
		{
			check(!EditReattachContexts.Find(this));
			EditReattachContexts.Set(this,new FComponentReattachContext(this));
		}
		else
		{
			ConditionalDetach();
		}
	}

	// Flush rendering commands to ensure the rendering thread processes the component detachment before it is modified.
	FlushRenderingCommands();
}

void UActorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// NOTE: The reattach context won't bother attaching components that are 'pending kill'.  That's a good thing,
	//  in case the component became deleted as part of an Undo action (such as undoing creation of an actor.)
	FComponentReattachContext* ReattachContext = EditReattachContexts.FindRef(this);
	if(ReattachContext)
	{
		delete ReattachContext;
		EditReattachContexts.Remove(this);
	}

	// The component or its outer could be pending kill when calling PostEditChange when applying a transaction.
	// Don't do do a full recreate in this situation, and instead simply detach.
	if( IsPendingKill() )
	{
		ConditionalDetach();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Returns whether the component is valid to be attached.
 * This should be implemented in subclasses to validate parameters.
 * If this function returns false, then Attach won't be called.
 */
UBOOL UActorComponent::IsValidComponent() const 
{ 
	return IsPendingKill() == FALSE; 
}

/**
 * Attaches the component to a ParentToWorld transform, owner and scene.
 * Requires IsValidComponent() == true.
 */
void UActorComponent::Attach()
{
	checkf(!HasAnyFlags(RF_Unreachable), TEXT("%s"), *GetDetailedInfo());
	checkf(!GetOuter()->IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo());
	checkf(!IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo() );
	checkf(Scene, TEXT("Attach: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );
	checkf(IsValidComponent(), TEXT("Attach: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!IsAttached(), TEXT("Attach: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!IsPendingKill(), TEXT("Attach: %s to %s"), *GetDetailedInfo(), Owner ? *Owner->GetFullName() : TEXT("*** No Owner ***") );

	bAttached = TRUE;

	if(Owner)
	{
		check(!Owner->IsPendingKill());
		// Add the component to the owner's list of all owned components.
		Owner->AllComponents.AddItem(this);
	}
}

/**
 * Updates state dependent on the ParentToWorld transform.
 * Requires bAttached == true
 */
void UActorComponent::UpdateTransform()
{
	check(bAttached);
}

/**
 * Detaches the component from the scene it is in.
 * Requires bAttached == true
 */
void UActorComponent::Detach( UBOOL bWillReattach )
{
	check(IsAttached());

	bAttached = FALSE;

	if(Owner)
	{
		// Remove the component from the owner's list of all owned components.
		Owner->AllComponents.RemoveItem(this);
	}
}

/**
 * Starts gameplay for this component.
 * Requires bAttached == true.
 */
void UActorComponent::BeginPlay()
{
	check(bAttached);
}

/**
 * Updates time dependent state for this component.
 * Requires bAttached == true.
 * @param DeltaTime - The time since the last tick.
 */
void UActorComponent::Tick(FLOAT DeltaTime)
{
	check(bAttached);
}

/**
 * Conditionally calls Attach if IsValidComponent() == true.
 * @param InScene - The scene to attach the component to.
 * @param InOwner - The actor which the component is directly or indirectly attached to.
 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
 */
void UActorComponent::ConditionalAttach(FSceneInterface* InScene,AActor* InOwner,const FMatrix& ParentToWorld)
{
	// If the component was already attached, detach it before reattaching.
	if(IsAttached())
	{
		DetachFromAny();
	}

	bNeedsReattach = FALSE;
	bNeedsUpdateTransform = FALSE;

	Scene = InScene;
	Owner = InOwner;
	SetParentToWorld(ParentToWorld);
	if(IsValidComponent())
	{
		Attach();
	}

	// Notify the streaming system. Will only update the component data if it's already tracked.
	const UPrimitiveComponent* Primitive = ConstCast<UPrimitiveComponent>(this);
	if ( Primitive )
	{
		GStreamingManager->NotifyPrimitiveUpdated( Primitive );
	}
}

/**
 * Conditionally calls UpdateTransform if bAttached == true.
 * @param ParentToWorld - The ParentToWorld transform the component is attached to.
 */
void UActorComponent::ConditionalUpdateTransform(const FMatrix& ParentToWorld)
{
	bNeedsUpdateTransform = FALSE;

	SetParentToWorld(ParentToWorld);
	if(bAttached)
	{
		UpdateTransform();
	}
}

/**
 * Conditionally calls UpdateTransform if bAttached == true.
 */
void UActorComponent::ConditionalUpdateTransform()
{
	if(bAttached)
	{
		UpdateTransform();
	}
}

/**
 * Conditionally calls Detach if bAttached == true.
 */
void UActorComponent::ConditionalDetach( UBOOL bWillReattach )
{
	if(bAttached)
	{
		Detach( bWillReattach );
	}
	Scene = NULL;
	Owner = NULL;
}

/**
 * Conditionally calls BeginPlay if bAttached == true.
 */
void UActorComponent::ConditionalBeginPlay()
{
	if(bAttached)
	{
		BeginPlay();
	}
}

/**
 * Conditionally calls Tick if bAttached == true.
 * @param DeltaTime - The time since the last tick.
 */
void UActorComponent::ConditionalTick(FLOAT DeltaTime)
{
	if(bAttached)
	{
		GAMEPLAY_PROFILER_TRACK_COMPONENT_WITH_ASSET(this, GetProfilerAssetObject());
		Tick(DeltaTime);
	}
}

/**
 * Sets the ticking group for this component
 *
 * @param NewTickGroup the new group to assign
 */
void UActorComponent::SetTickGroup(BYTE NewTickGroup)
{
	check((NewTickGroup == TG_EffectsUpdateWork) ? this->IsA(UParticleSystemComponent::StaticClass()) : 1);
	TickGroup = NewTickGroup;
}

/** force this component to be updated right now
 * component must be directly attached to its Owner (not attached to another component)
 * @param bTransformOnly - if true, only update transform, otherwise do a full reattachment
 */
void UActorComponent::execForceUpdate(FFrame& Stack, RESULT_DECL)
{
	P_GET_UBOOL(bTransformOnly);
	P_FINISH;

	if (bAttached && Owner != NULL && Owner->Components.ContainsItem(this))
	{
		if (bTransformOnly)
		{
			BeginDeferredUpdateTransform();
		}
		else
		{
			BeginDeferredReattach();
		}
		UpdateComponent(GWorld->Scene, Owner, Owner->LocalToWorld());
	}
}

/**
 * Marks the appropriate world as lighting requiring a rebuild.
 */
void UActorComponent::MarkLightingRequiringRebuild()
{
	// Whether this component is contributing to static lighting.
	UBOOL bIsContributingToStaticLighting	= FALSE;

	// Only primitive components with static shadowing contribute to static lighting.
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(this);
	bIsContributingToStaticLighting			= bIsContributingToStaticLighting || (PrimitiveComponent && PrimitiveComponent->HasStaticShadowing());

	// Only light components with static shadowing contribute to static lighting.
	ULightComponent* LightComponent			= Cast<ULightComponent>(this);
	bIsContributingToStaticLighting			= bIsContributingToStaticLighting || (LightComponent && LightComponent->HasStaticShadowing());

	// No need to mark lighting as requiring to be rebuilt if component doesn't contribute to static lighting.
	if( bIsContributingToStaticLighting )
	{
		// Find out whether the component resides in a PIE package, in which case we don't have to mark anything
		// as requiring lighting to be rebuilt.
		UBOOL bIsInPIEPackage = (GetOutermost()->PackageFlags & PKG_PlayInEditor) ? TRUE : FALSE;
		if( !bIsInPIEPackage )
		{
			// Find out whether the component is part of loaded level/ world.
			UObject*	Object	 = GetOuter();
			UWorld*		WorldObj = NULL;
			while( Object )
			{
				if( Object->IsA( UWorld::StaticClass() ) )
				{
					WorldObj = Cast<UWorld>(Object);
					break;
				}
				Object = Object->GetOuter();
			}

			// Mark the world as requiring lighting to be rebuilt.
			if (WorldObj)
			{
				if (WorldObj->GetWorldInfo())
				{
					WorldObj->GetWorldInfo()->SetMapNeedsLightingFullyRebuilt( TRUE );
				}
			}
		}
	}
}

/** used by DetachFromAny() - recurses over SkeletalMeshComponent attachments until the component is detached or there are no more SkeletalMeshComponents
 * @return whether Component was successfully detached
 */
static UBOOL DetachComponentFromMesh(UActorComponent* Component, USkeletalMeshComponent* Mesh)
{
	Mesh->DetachComponent(Component);
	if (!Component->IsAttached())
	{
		// successfully removed from Mesh
		return TRUE;
	}
	else
	{
		// iterate over attachments and recurse over any SkeletalMeshComponents found
		for (INT i = 0; i < Mesh->Attachments.Num(); i++)
		{
			USkeletalMeshComponent* AttachedMesh = Cast<USkeletalMeshComponent>(Mesh->Attachments(i).Component);
			if (AttachedMesh != NULL && DetachComponentFromMesh(Component, AttachedMesh))
			{
				return TRUE;
			}
		}

		return FALSE;
	}
}

/** if the component is attached, finds out what it's attached to and detaches it from that thing
 * slower, so use only when you don't have an easy way to find out where it's attached
 */
void UActorComponent::DetachFromAny()
{
	if (IsAttached())
	{
		// if there is no owner, just call Detach()
		if (Owner == NULL)
		{
			ConditionalDetach();
		}
		else
		{
			// try to detach from the actor directly
			Owner->DetachComponent(this);
			if (IsAttached())
			{
				// check if the owner has any SkeletalMeshComponents, and if so make sure this component is not attached to them
				// we could optimize this by adding a pointer to ActorComponent for the SkeletalMeshComponent it's attached to
				for (INT i = 0; i < Owner->AllComponents.Num(); i++)
				{
					USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Owner->AllComponents(i));
					if (Mesh != NULL && DetachComponentFromMesh(this, Mesh))
					{
						break;
					}
				}
			}
		}
		if (IsAttached())
		{
			debugf(NAME_Error, TEXT("ActorComponent::DetachFromAny() failed to detach %s (%s) from %s - missing from AllComponents array and/or SkeletalMeshComponent's Attachments array?"), *GetName(), *GetDetailedInfo(), *Owner->GetName());
			appErrorfDebug(NAME_Error, TEXT("ActorComponent::DetachFromAny() failed to detach %s (%s) from %s - missing from AllComponents array and/or SkeletalMeshComponent's Attachments array?"), *GetName(), *GetDetailedInfo(), *Owner->GetName());
		}
	}
}

#if !FINAL_RELEASE && PERF_ENABLE_FRAME_COMPUPDATE_DUMP
extern UBOOL GShouldLogOutAFrameOfComponentUpdates;
#endif

void UActorComponent::UpdateComponent(FSceneInterface* InScene,AActor* InOwner,const FMatrix& InLocalToWorld, UBOOL bCollisionUpdate)
{
#if PERF_LOG_DETAILED_COMPONENT_UPDATE_STATS
	static QWORD LastFrameCounter = 0;
	if( GLogDetailedComponentUpdateStats && (LastFrameCounter != GFrameCounter) && !(GFrameCounter % NUM_FRAMES_BETWEEN_LOG_DETAILS) )
	{
		GDetailedComponentUpdateStats.DumpStats();
		GDetailedComponentUpdateStats.Reset();
		LastFrameCounter = GFrameCounter;
	}
#endif

	{
		PERF_TRACK_DETAILED_COMPONENT_UPDATE_STATS(this);

#if !FINAL_RELEASE && PERF_ENABLE_FRAME_COMPUPDATE_DUMP
		const DOUBLE StartUpdateTime = appSeconds();
#endif

		if( !IsAttached() )
		{
			// initialize the component if it hasn't already been
			ConditionalAttach(InScene,InOwner,InLocalToWorld);
		}
		else if(bNeedsReattach)
		{
			// Reattach the component if it has changed since it was last updated.
			const UBOOL bWillReattach = TRUE;
			ConditionalDetach( bWillReattach );
			ConditionalAttach(InScene,InOwner,InLocalToWorld);

#if !FINAL_RELEASE && PERF_ENABLE_FRAME_COMPUPDATE_DUMP
			DOUBLE TotalTime = appSeconds() - StartUpdateTime;
			if(GShouldLogOutAFrameOfComponentUpdates)
			{
				debugf(TEXT("CR %s %s %5.3f"), *GetPathName(), *GetDetailedInfo(), TotalTime*1000.f);
			}
#endif
		}
		else if(bNeedsUpdateTransform)
		{
			// Update the component's transform if the actor has been moved since it was last updated.
			ConditionalUpdateTransform(InLocalToWorld);

#if !FINAL_RELEASE && PERF_ENABLE_FRAME_COMPUPDATE_DUMP
			DOUBLE TotalTime = appSeconds() - StartUpdateTime;
			if(GShouldLogOutAFrameOfComponentUpdates)
			{
				debugf(TEXT("UT %s %s %5.3f"), *GetPathName(), *GetDetailedInfo(), TotalTime*1000.f);
			}
#endif
		}
	}

	// Update the components attached indirectly via this component.
	//@note - testing whether or not we can skip this for collision only updates to improve performance
	if (!bCollisionUpdate)
	{
		UpdateChildComponents();
	}
}

void UActorComponent::BeginDeferredReattach()
{
	bNeedsReattach = TRUE;

	if (Owner != NULL)
	{
		// If the component has a static owner, reattach it directly.
		// If it has a dynamic owner, it will be reattached at the end of the tick.
		if (!Owner->WantsTick())
		{
			Owner->ConditionalUpdateComponents(FALSE);
		}
	}
	else
	{
		// If the component doesn't have an owner, reattach it directly using its existing transform.
		FComponentReattachContext(this);
	}
}

void UActorComponent::BeginDeferredUpdateTransform()
{
	bNeedsUpdateTransform = TRUE;

	if (Owner != NULL)
	{
		// If the component has a static owner, update its transform directly.
		// If it has a dynamic owner, it will be updated at the end of the tick.
		if (!Owner->WantsTick())
		{
			Owner->ConditionalUpdateComponents(FALSE);
		}
	}
	else
	{
		// If the component doesn't have an owner, just call UpdateTransform without actually applying a new transform.
		ConditionalUpdateTransform();
	}
}

/** 
 * Create the bounding box/sphere for this primitive component
 */
void UDrawQuadComponent::UpdateBounds()
{
	const FLOAT MinThick=16.f;
	Bounds = FBoxSphereBounds( 
		LocalToWorld.TransformFVector(FVector(0,0,0)), 
		FVector(MinThick,Width,Height), 
		Max<FLOAT>(Width,Height) );
}

UBOOL FWindSourceSceneProxy::GetWindParameters(const FVector& EvaluatePosition, FVector4& WindDirectionAndSpeed, FLOAT& Weight) const 
{ 
	if (bIsPointSource)
	{
		const FLOAT Distance = (Position - EvaluatePosition).Size();
		if (Distance <= Radius)
		{
			// Mimic UE3 point light attenuation with a FalloffExponent of 1
			const FLOAT RadialFalloff = Max(1.0f - ((EvaluatePosition - Position) / Radius).SizeSquared(), 0.0f);
			WindDirectionAndSpeed = FVector4((EvaluatePosition - Position) / Distance * Strength * RadialFalloff, Speed); 
			Weight = Distance / Radius * Strength;
			return TRUE;
		}
		Weight = 0;
		WindDirectionAndSpeed = FVector4(0,0,0,0);
		return FALSE;
	}

	Weight = Strength;
	WindDirectionAndSpeed = FVector4(Direction * Strength, Speed); 
	return TRUE;
}

void UWindDirectionalSourceComponent::Attach()
{
	Super::Attach();
	Scene->AddWindSource(this);
}

void UWindDirectionalSourceComponent::UpdateTransform()
{
	Super::UpdateTransform();
	Scene->RemoveWindSource(this);
	Scene->AddWindSource(this);
}

void UWindDirectionalSourceComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach( bWillReattach );
	Scene->RemoveWindSource(this);
}

//
//	UWindDirectionalSourceComponent::GetRenderData
//

FWindSourceSceneProxy* UWindDirectionalSourceComponent::CreateSceneProxy() const
{
	return new FWindSourceSceneProxy(
		Owner->LocalToWorld().TransformNormal(FVector(1,0,0)).SafeNormal(),
		Strength,
		Speed
		);
}

void UWindPointSourceComponent::UpdatePreviewRadius()
{
	if ( PreviewRadiusComponent )
	{
		PreviewRadiusComponent->SphereRadius = Radius;
		PreviewRadiusComponent->Translation = FVector(0,0,0);
	}
}

void UWindPointSourceComponent::Attach()
{
	UpdatePreviewRadius();
	Super::Attach();
}

void UWindPointSourceComponent::UpdateTransform()
{
	Super::UpdateTransform();
	UpdatePreviewRadius();
}

FWindSourceSceneProxy* UWindPointSourceComponent::CreateSceneProxy() const
{
	return new FWindSourceSceneProxy(
		Owner->Location,
		Strength,
		Speed,
		Radius
		);
}

//
//	UStaticMeshComponentFactory::CreatePrimitiveComponent
//

UPrimitiveComponent* UStaticMeshComponentFactory::CreatePrimitiveComponent(UObject* InOuter)
{
	UStaticMeshComponent*	Component = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(),InOuter);

	Component->CollideActors = CollideActors;
	Component->BlockActors = BlockActors;
	Component->BlockZeroExtent = BlockZeroExtent;
	Component->BlockNonZeroExtent = BlockNonZeroExtent;
	Component->BlockRigidBody = BlockRigidBody;
	Component->HiddenGame = HiddenGame;
	Component->HiddenEditor = HiddenEditor;
	Component->CastShadow = CastShadow;
	Component->Materials = Materials;
	Component->StaticMesh = StaticMesh;

	return Component;
}
