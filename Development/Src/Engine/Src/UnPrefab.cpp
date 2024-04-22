/*=============================================================================
	UnPrefab.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "EnginePrefabClasses.h"
#include "LevelUtils.h"

IMPLEMENT_CLASS(UPrefab);
IMPLEMENT_CLASS(APrefabInstance);

#define CHECK_PREFAB_BRUSHES 1


#if CHECK_PREFAB_BRUSHES
static void CheckPrefabBrushes(APrefabInstance* PrefInst)
{
	for ( TMap<UObject*,UObject*>::TIterator It(PrefInst->ArchetypeToInstanceMap); It; ++It )
	{
		ABrush* Brush = Cast<ABrush>( It.Value() );
		if(Brush)
		{
			// Check UModel pointed to by Actor and BrushComponent are the same.
			check(Brush->Brush == Brush->BrushComponent->Brush);

			if ( Brush->Brush == NULL )
			{
				appMsgf(AMT_OK, TEXT("NULL model in Brush '%s' contained by prefab '%s'"), *Brush->GetFullName(), *PrefInst->GetFullName());
			}
			else
			{
				// Check outer of UModel is the Actor.
				check(Brush->Brush->GetOuter() == Brush);
			}
		}
	}
}
#endif

/**
 * Determines whether this object is contained within a UPrefab.
 *
 * @param	OwnerPrefab		if specified, receives a pointer to the owning prefab.
 *
 * @return	TRUE if this object is contained within a UPrefab; FALSE if it IS a UPrefab or not contained within one.
 */
UBOOL UObject::IsAPrefabArchetype( UObject** OwnerPrefab/*=NULL*/ ) const
{
	UBOOL bResult = FALSE;
	for ( UObject* CheckOuter = GetOuter(); CheckOuter; CheckOuter = CheckOuter->GetOuter() )
	{
		if ( CheckOuter->IsA(UPrefab::StaticClass()) )
		{
			if ( OwnerPrefab != NULL )
			{
				*OwnerPrefab = CheckOuter;
			}
			bResult = TRUE;
			break;
		}
	}
	return bResult;
}

/**
 * @return		TRUE if the object is part of a prefab instance.
 */
UBOOL UObject::IsInPrefabInstance() const
{
	// See if it actually is a APrefabInstance actor.
	if( IsA(APrefabInstance::StaticClass()) )
	{
		return TRUE;
	}

	// Look at the archetype this object using, and see if it is part of a prefab.
	const UObject* Archetype = GetArchetype();
	checkMsg( Archetype, "Object has no archetype" );

	return Archetype->IsAPrefabArchetype();
}

// Dynamically cast an object type-safely.
class UClass;
template< class T > T* Cast(const UObject* Src)
{
	return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}

/**
 *	Utility for finding the PrefabInstance that 'owns' this actor.
 *	If the actor is not part of a prefab instance, returns NULL.
 *	If the actor _is_ a PrefabInstance, return itself.
 */
APrefabInstance* AActor::FindOwningPrefabInstance() const
{
	// Do nothing if this actor isn't part of a prefab.
	if ( !IsInPrefabInstance() )
	{
		return NULL;
	}

	// If this _is_ a PrefabInstance, return itself.
	APrefabInstance* ThisAsPrefab = Cast<APrefabInstance>( this );
	if( ThisAsPrefab )
	{
		return ThisAsPrefab;
	}

	// Iterate over all actors..
	for( FActorIterator It ; It ; ++It )
	{
		// If this actor is a PrefabInstance
		APrefabInstance* PrefInst = Cast<APrefabInstance>(*It);
		if(	PrefInst && 
			!PrefInst->bDeleteMe && !PrefInst->IsPendingKill() )
		{
			TArray<AActor*> InstActors;
			PrefInst->GetActorsInPrefabInstance(InstActors);

			if( InstActors.ContainsItem(const_cast<AActor*>(this)) )
			{
				return PrefInst;
			}
		}
	}

	// We failed to find a PrefabInstance owning this Actor, so return NULL.
	return NULL;
}

/*-----------------------------------------------------------------------------
	APrefabInstance
-----------------------------------------------------------------------------*/

void APrefabInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << ArchetypeToInstanceMap;

	Ar << PI_ObjectMap;
}

/** 
 *	Called when map is saved. 
 *	We save the differences between this instance and the Prefab defaults.
 *	If the prefab has changed when we load the map, we initialise the instance to its defaults and then load
 *	this archive again to propagate any changes that were made.
 */
void APrefabInstance::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	// Don't do this for default object.
	if( !IsTemplate() )
	{
		// If we are not cooking...
		UPackage* Package = CastChecked<UPackage>(GetOutermost());
		if( !(Package->PackageFlags & PKG_Cooked) )
		{
			// Update the archive containing the differences between this Actor and its Prefab defaults.
			SavePrefabDifferences();
		}
		else
		{
			// In cooking, empty the difference archive - this actor will never be opened in the editor now.
			PI_Bytes.Empty();
			PI_CompleteObjects.Empty();
			PI_ReferencedObjects.Empty();
			PI_SavedNames.Empty();
			PI_ObjectMap.Empty();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if REQUIRES_SAMECLASS_ARCHETYPE
/**
 * Provides PrefabInstance objects with a way to override incorrect behavior in ConditionalPostLoad()
 * until different-class archetypes are supported.
 *
 * @fixme - temporary hack; correct fix would be to support archetypes of a different class
 *
 * @return	pointer to an object instancing graph to use for logic in ConditionalPostLoad().
 */
FObjectInstancingGraph* APrefabInstance::GetCustomPostLoadInstanceGraph()
{
	FObjectInstancingGraph* Result = new FObjectInstancingGraph(this, TemplatePrefab);
	Result->SetLoadingObject(TRUE);
	Result->AddObjectPair(this, GetArchetype());

	return Result;
}
#endif

void APrefabInstance::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerVersion() < VER_FIXED_PREFAB_SEQUENCES && SequenceInstance != NULL )
	{
		SequenceInstance->SetOwnerPrefab(this);
// 		SequenceInstance->SetArchetype(TemplatePrefab->PrefabSequence);
		MarkPackageDirty();
	}
}

/** Applies a transform to the object if its an actor. */
void APrefabInstance::ApplyTransformIfActor(UObject* Obj, const FMatrix& Transform)
{
	AActor* Actor = Cast<AActor>(Obj);
	if(Actor)
	{
		FRotationTranslationMatrix  ActorTM(Actor->Rotation,Actor->Location);
		FMatrix NewActorTM = ActorTM * Transform;
		Actor->Location = NewActorTM.GetOrigin();
		Actor->Rotation = NewActorTM.Rotator();
	}
}

/** Utility for taking a map and inverting it. */
void APrefabInstance::CreateInverseMap(TMap<UObject*, UObject*>& OutMap, TMap<UObject*, UObject*>& InMap)
{
	for ( TMap<UObject*,UObject*>::TIterator It(InMap); It; ++It )
	{
		UObject* Key = It.Key();
		UObject* Value = It.Value();

		if(Value != NULL)
		{
			OutMap.Set(Value, Key);
		}
	}
}

/**
 *	Utility	for copying UModel from one ABrush to another.
 *	Sees if DestActor is an ABrush. If so, assumes SrcActor is one. Then uses StaticDuplicateObject to copy UModel from 
 *	SrcActor to DestActor.
 */
void APrefabInstance::CopyModelIfBrush(UObject* DestObj, UObject* SrcObj)
{
	// If this is a Brush, we have to use special function to copy the UModel.
	ABrush* DestBrush = Cast<ABrush>(DestObj);
	if(DestBrush)
	{
		ABrush* SrcBrush = CastChecked<ABrush>(SrcObj);
		if(SrcBrush->Brush)
		{
			DestBrush->Brush = (UModel*)UObject::StaticDuplicateObject(SrcBrush->Brush, SrcBrush->Brush, DestBrush, TEXT("None"));
			DestBrush->BrushComponent->Brush = DestBrush->Brush;
		}
		else
		{
			DestBrush->Brush = NULL;
			DestBrush->BrushComponent->Brush = NULL;
		}
	}
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void APrefabInstance::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );

	//@todo rtgc: this can be removed once we have proper TMap support in script/ RTGC
	for( TMap<UObject*,UObject*>::TIterator It(ArchetypeToInstanceMap); It; ++It )
	{
		AddReferencedObject( ObjectArray, It.Key() );
		AddReferencedObject( ObjectArray, It.Value() );
	}

	for( TMap<UObject*,INT>::TIterator It(PI_ObjectMap); It; ++It )
	{
		AddReferencedObject( ObjectArray, It.Key() );
	}
}

struct FSaveConnInfo
{
	class USequenceOp*	SeqOp;
	INT					OutputConnIndex;
	FString				InputConnDesc;

	FSaveConnInfo(USequenceOp* InSeqOp, INT InConnIndex, const FString& InInputDesc) 
		: SeqOp(InSeqOp)
		, OutputConnIndex(InConnIndex)
	{
		appMemzero(&InputConnDesc, sizeof(FString));
		InputConnDesc = InInputDesc;
	}
};

/** 
 *	Update this instance of a prefab to match the template prefab.
 *	This will destroy/create objects as necessary.
 *	It also recreates the Kismet sequence.
 */
void APrefabInstance::UpdatePrefabInstance(USelection* Selection)
{
#if WITH_EDITORONLY_DATA
	// Should not be doing this in the game!
	check(!GWorld->HasBegunPlay());

	// Do nothing if template is NULL.
	if(!TemplatePrefab)
	{
		return;
	}

	if ( FLevelUtils::IsLevelLocked(GetLevel()) )
	{
		warnf(TEXT("UpdatePrefab: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		return;
	}

	// when updating a prefab instance, the current level may not be the level that contains this PrefabInstance, which would result in any actors that were added to
	// our source Prefab being spawned into a different level than this PrefabInstance (BAD!)
	// temporarily change the CurrentLevel to the level that contains this PrefabInstance; we'll restore it when we're done
	ULevel* RealCurrentLevel = GWorld->CurrentLevel;
	GWorld->CurrentLevel = GetLevel();

	FRotationTranslationMatrix PrefabToWorld(Rotation,Location);

	// Set up an archive for reading back the changed properties of this prefab instance.
	FPrefabUpdateArc UpdateAr;
	CopyToArchive(&UpdateAr);
	UpdateAr.SetPersistant(TRUE);
	UpdateAr.ActivateReader();

	// this value represents 1/2 degree in Unreal Units
	const static FLOAT RotationTolerance = 65536.f * (0.5f / 360.f);

	// Iterate over each object in the instance propagating changes in prefab.
	for ( TMap<UObject*,UObject*>::TIterator InstIt(ArchetypeToInstanceMap); InstIt; ++InstIt )
	{
		UObject* Archetype = InstIt.Key();
		UObject* ArcInst = InstIt.Value();
		if(ArcInst)
		{
			FRotator CurrentRotation(0,0,0);
			AActor* ActorInst = Cast<AActor>(ArcInst);
			if ( ActorInst != NULL )
			{
				CurrentRotation = ActorInst->Rotation;
			}
			ArcInst->PreEditChange(NULL);

			// Init properties to prefab defaults.
			UpdateAr.SerializeObject(ArcInst);

			// Turn Archetype references back into Instance references
			FArchiveReplaceObjectRef<UObject> ReplaceAr(ArcInst, ArchetypeToInstanceMap, false, true, true);

			// Turn relative locations back into worldlocations.
			ApplyTransformIfActor(ArcInst, PrefabToWorld);

			// if the actor's roatation was changed by less than a degree, it's probably due the inherent precision loss of extracting a rotator from the translation matrix
			if (ActorInst != NULL
			&&	Abs(CurrentRotation.Pitch - ActorInst->Rotation.Pitch) < RotationTolerance
			&&	Abs(CurrentRotation.Yaw - ActorInst->Rotation.Yaw) < RotationTolerance
			&&	Abs(CurrentRotation.Roll - ActorInst->Rotation.Roll) < RotationTolerance)
			{
				// correct it
				ActorInst->Rotation = CurrentRotation;
			}

			// If this is a brush, use special function to copy the UModel from the Prefab and clobber the old one.
			CopyModelIfBrush(ArcInst, Archetype);

			ArcInst->PostEditChange();
		}
	}

	/*
	- Try to find each Arch in TemplateArchetype in ArchToInst map.
	- If it doesn't exist, create new instance or that Arch and add to map.
	- Try to find each Arch in DeletedArchetype in ArchToInst map.
	- If it exists, delete Inst and remove entry in map.
	*/

	for(INT i=0; i<TemplatePrefab->PrefabArchetypes.Num(); i++)
	{
		UObject* Archetype = TemplatePrefab->PrefabArchetypes(i);
		UObject** ObjInstPtr = ArchetypeToInstanceMap.Find(Archetype);

		// If its not in the map at all, create an instance of it and add it.
		// Note that if its in the map but NULL we don't create it. This allows you to delete an instance from a prefab and keep it removed.
		if(!ObjInstPtr)
		{
			UObject* NewObj = NULL;
			if(Archetype->IsA(AActor::StaticClass()))
			{
				AActor* ArcActor = CastChecked<AActor>(Archetype);
				FRotationTranslationMatrix  RelTM(ArcActor->Rotation,ArcActor->Location);
				FMatrix WorldTM = RelTM * PrefabToWorld;

				NewObj = GWorld->SpawnActor(Archetype->GetClass(), NAME_None, WorldTM.GetOrigin(), WorldTM.Rotator(), ArcActor);

				// For Brushes, we need to do extra work to copy the UModel.
				CopyModelIfBrush(NewObj, Archetype);
			}
			else
			{
				// @todo Handle instancing non-Actor classes.
			}

			ArchetypeToInstanceMap.Set(Archetype, NewObj);
		}
	}

	for(INT i=0; i<TemplatePrefab->RemovedArchetypes.Num(); i++)
	{
		UObject* Archetype = TemplatePrefab->RemovedArchetypes(i);
		UObject** ObjInstPtr = ArchetypeToInstanceMap.Find(Archetype);

		// If we find an instance of this prefab, we want to remove it.
		if(ObjInstPtr)
		{
			UObject* ObjInst = *ObjInstPtr;
			// This will be NULL if they already deleted it themselves.
			if(ObjInst)
			{
				AActor* ActorInst = Cast<AActor>(ObjInst);
				if(ActorInst)
				{
					Selection->Deselect(ActorInst);
					GWorld->DestroyActor(ActorInst);
				}

				// We change the entry in the instance table so all references to instances of this Archetype get set to NULL.
				ArchetypeToInstanceMap.Set(Archetype, NULL);
			}
		}
	}

	// Fix up references in the newly created objects, and null out references to objects that have been destroyed.
	for ( TMap<UObject*,UObject*>::TIterator InstIt(ArchetypeToInstanceMap); InstIt; ++InstIt )
	{
		UObject* Archetype = InstIt.Key();
		UObject* ArcInst = InstIt.Value();
		check(Archetype);

		// replace any references to actors in the set with references to the archetype version of that object
		FArchiveReplaceObjectRef<UObject>(ArcInst, ArchetypeToInstanceMap, false, true, true);
	}

	// Fix up Kismet sequence.

	INT SeqPosX = 0;
	INT SeqPosY = 0;
	FString SeqName;

	// We dont copy events, because there is no 'external event' object..
	TArray<FSaveConnInfo> SeqInputLinks;
	TArray<FSeqOpOutputLink> SeqOutputLinks;
	TArray<FSeqVarLink> SeqVariableLinks;
	if(SequenceInstance)
	{
		SeqPosX = SequenceInstance->ObjPosX;
		SeqPosY = SequenceInstance->ObjPosY;
		SeqName = SequenceInstance->ObjName;

		// Need to find all the links to us. This is the hard bit

		// Get the sequence we are in
		USequence* ParentSeq = SequenceInstance->ParentSequence;
		if(ParentSeq)
		{
			// iterate through all other objects, looking for output links that point to this op
			for (INT chkIdx=0; chkIdx < ParentSeq->SequenceObjects.Num(); chkIdx++)
			{
				USequenceOp* ChkOp = Cast<USequenceOp>(ParentSeq->SequenceObjects(chkIdx));
				if(ChkOp)
				{
					// iterate through this op's output links
					for (INT outputIdx=0; outputIdx < ChkOp->OutputLinks.Num(); outputIdx++)
					{
						FSeqOpOutputLink& OutLink = ChkOp->OutputLinks(outputIdx);

						// iterate through all the inputs linked to this output
						for (INT inputIdx=0; inputIdx < OutLink.Links.Num(); inputIdx++)
						{
							// If this links to us..
							if( OutLink.Links(inputIdx).LinkedOp == SequenceInstance )
							{
								// Find the name of the input link on us that it connects to
								INT InputIndex = OutLink.Links(inputIdx).InputLinkIdx;
								FString InputDesc = SequenceInstance->InputLinks(InputIndex).LinkDesc;

								// Save that info.
								SeqInputLinks.AddItem( FSaveConnInfo(ChkOp, outputIdx, InputDesc) );
							}
						}
					}
				}
			}
		}

		// Links to other things are easy - we just copy the arrays
		SeqOutputLinks = SequenceInstance->OutputLinks;
		SeqVariableLinks = SequenceInstance->VariableLinks;
	}

	// First remove the old one.	
	DestroyKismetSequence();

	// Then create a new instance.
	InstanceKismetSequence(TemplatePrefab->PrefabSequence, SeqName);

	if(SequenceInstance)
	{
		SequenceInstance->ObjPosX = SeqPosX;
		SequenceInstance->ObjPosY = SeqPosY;

		// Match up each of the arrays we copied

		// Iterate over each output we copied
		for(INT i=0; i<SeqOutputLinks.Num(); i++)
		{
			// Try and find a output with the same name
			FSeqOpOutputLink OutputLink = SeqOutputLinks(i);
			UBOOL bFoundLink = FALSE;
			for (INT Idx = 0; Idx < SequenceInstance->OutputLinks.Num() && !bFoundLink; Idx++)
			{
				if(OutputLink.LinkDesc == SequenceInstance->OutputLinks(Idx).LinkDesc)
				{
					// If we do - copy the LinedVariables array over.
					bFoundLink = TRUE;
					SequenceInstance->OutputLinks(Idx).Links = OutputLink.Links;
				}
			}
		}


		// Iterate over each variable connector we copied
		for(INT i=0; i<SeqVariableLinks.Num(); i++)
		{
			// Try and find a var link with the same name
			FSeqVarLink VariableLink = SeqVariableLinks(i);
			UBOOL bFoundLink = FALSE;
			for (INT Idx = 0; Idx < SequenceInstance->VariableLinks.Num() && !bFoundLink; Idx++)
			{
				if(VariableLink.LinkDesc == SequenceInstance->VariableLinks(Idx).LinkDesc)
				{
					// If we do - copy the LinedVariables array over.
					bFoundLink = TRUE;
					SequenceInstance->VariableLinks(Idx).LinkedVariables = VariableLink.LinkedVariables;
				}
			}
		}


		// Fix up links from other objects to this one
		USequence* ParentSeq = SequenceInstance->ParentSequence;
		if(ParentSeq)
		{
			// Iterate over each link we copied.
			for(INT i=0; i<SeqInputLinks.Num(); i++)
			{
				// Find out if there is still an input with that name.
				INT InputIndex = INDEX_NONE;
				for(INT inputIdx=0; inputIdx < SequenceInstance->InputLinks.Num() && InputIndex == INDEX_NONE; inputIdx++)
				{
					if(SeqInputLinks(i).InputConnDesc == SequenceInstance->InputLinks(inputIdx).LinkDesc)
					{
						InputIndex = inputIdx;
					}
				}
			
				// If we found the input, so make a connection to it from the other object
				if(InputIndex != INDEX_NONE)
				{
					USequenceOp* ConnOp = SeqInputLinks(i).SeqOp;
					check( SeqInputLinks(i).OutputConnIndex < ConnOp->OutputLinks.Num() );

					// Get the ouput connector of the other object
					FSeqOpOutputLink& OutLink = ConnOp->OutputLinks(SeqInputLinks(i).OutputConnIndex);

					// Add new entry to its Links array.
					INT NewLinkIndex = OutLink.Links.AddZeroed();
					OutLink.Links(NewLinkIndex).LinkedOp = SequenceInstance;
					OutLink.Links(NewLinkIndex).InputLinkIdx = InputIndex;
				}
			}
		}
	}

	// Finally, set the instance version to be the same as the template version.
	TemplateVersion = TemplatePrefab->PrefabVersion;

#if CHECK_PREFAB_BRUSHES
	CheckPrefabBrushes(this);
#endif

	// now restore the current level to its previous value
	GWorld->CurrentLevel = RealCurrentLevel;
#endif // WITH_EDITORONLY_DATA
}

/** Instance the Kismet sequence if we have one into the 'Prefabs' subsequence. */
void APrefabInstance::InstanceKismetSequence(USequence* SrcSequence, const FString& InSeqName)
{
#if WITH_EDITORONLY_DATA
	// Do nothing if NULL sequence supplied.
	if(!SrcSequence)
	{
		return;
	}

	if ( FLevelUtils::IsLevelLocked(GetLevel()) )
	{
		warnf(TEXT("InstanceKismetSequence: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		return;
	}

	// Now instance the Kismet sequence if we have one into the 'Prefabs' subsequence.


	// find the level that contains this PrefabInstance. Typically GWorld->CurrentLevel, but when updating a prefab that has
	// PrefabInstances in multiple levels, all PrefabInstances based on that Prefab will be reinitialized, and this PrefabInstance's
	// level may not be the level currently being edited 
	ULevel* OwnerLevel = GetLevel();
	if ( OwnerLevel == NULL )
	{
		OwnerLevel = GWorld->CurrentLevel;
	}

	// Make sure the level that owns this 
	USequence* RootSeq = GWorld->GetGameSequence(OwnerLevel);
	if( RootSeq == NULL )
	{
		RootSeq = ConstructObject<USequence>( USequence::StaticClass(), OwnerLevel, TEXT("Main_Sequence"), RF_Transactional );
		GWorld->SetGameSequence( RootSeq, OwnerLevel );

		// probably redundant, but just to be sure we don't side-step any special-case code in GetGameSequence
		RootSeq = GWorld->GetGameSequence( OwnerLevel );
	}

	if(RootSeq)
	{
		USequence* PrefabsSequence = RootSeq->GetPrefabsSequence();
		PrefabsSequence->bDeletable = FALSE;
		PrefabsSequence->Modify();

		// Copy sequence from package.

		FString SeqName = SrcSequence->ObjName;

		// If a name was passed in, try use that instead.
		if(InSeqName.Len() > 0)
		{
			SeqName = InSeqName;
		}

		PrefabsSequence->ClearNameUsage( FName(*SeqName), REN_ForceNoResetLoaders );

		FObjectDuplicationParameters DupParms(SrcSequence, PrefabsSequence);
		DupParms.DestClass = UPrefabSequence::StaticClass();
		DupParms.DestName = *SeqName;
		DupParms.bMigrateArchetypes = TRUE;
		DupParms.FlagMask = ~(RF_ArchetypeObject|RF_ClassDefaultObject|RF_Public);

		SequenceInstance = CastChecked<UPrefabSequence>( StaticDuplicateObjectEx(DupParms) );
		SequenceInstance->ObjName = SequenceInstance->GetName();

		// Turn archetype refs into actual object refs.  Only clear private refs if the source prefab does not exist in the same level
		// as the prefab instance (don't clear references to private actors contained in the same level)
		FArchiveReplaceObjectRef<UObject> ReplaceAr(SequenceInstance, ArchetypeToInstanceMap, SrcSequence->GetOutermost() != GetOutermost(), TRUE, TRUE);

		// fixup event names not matching up to the newly placed prefab actors
		//@todo: might make sense to move this to a separate function if we determine that we need to do something similar for other sequence object types as well
		{
			TArray<USequenceEvent*> SeqEvents;
			SequenceInstance->FindSeqObjectsByClass(USequenceEvent::StaticClass(), (TArray<USequenceObject*>&)SeqEvents, TRUE);
			for ( INT EventIndex = 0; EventIndex < SeqEvents.Num(); EventIndex++ )
			{
				USequenceEvent* Evt = SeqEvents(EventIndex);
				const FString DefaultName = Evt->GetClass()->GetDefaultObject<USequenceEvent>()->ObjName;

				Evt->ObjName = Evt->Originator != NULL
					? Evt->Originator->GetName() + TEXT(" ") + DefaultName
					: DefaultName;
			}
		}

		//@todo: Set the position to be something sensible.

		// Add to 'Prefabs' sequence array of sequence objects.
		PrefabsSequence->AddSequenceObject(SequenceInstance);

		// If we have begun play..
		if(GWorld->HasBegunPlay())
		{
			// Add to optimised 'Nested Sequences' array.
			PrefabsSequence->NestedSequences.AddItem(SequenceInstance);

			// Call BeginPlay on it now.
			SequenceInstance->BeginPlay();
		}

		// Sanity check the ParentSequence pointers.
		SequenceInstance->CheckParentSequencePointers();

		// Refresh any Kismet windows that might be viewing this sequence.
		GCallbackEvent->Send(CALLBACK_RefreshEditor_Kismet);
	}
#endif // WITH_EDITORONLY_DATA
}

/** Destroy the Kismet sequence associated with this Prefab instance. */
void APrefabInstance::DestroyKismetSequence()
{
	if( SequenceInstance == NULL )
	{
		return; 
	}

	USequence* PrefabSequenceContainer = CastChecked<USequence>(SequenceInstance->GetOuter());
	PrefabSequenceContainer->RemoveObject(SequenceInstance);

	// if we just removed the last prefab sequence from the level's Prefab subsequence, remove it as well
	if ( PrefabSequenceContainer->SequenceObjects.Num() == 0 )
	{
		USequence* GameSequence = CastChecked<USequence>(PrefabSequenceContainer->GetOuter());
		GameSequence->RemoveObject(PrefabSequenceContainer);
	}

	Modify();
	SequenceInstance = NULL;

	// Refresh any Kismet windows that might be viewing this sequence.
	GCallbackEvent->Send(CALLBACK_RefreshEditor_Kismet);
}


/** 
 *	Create an instance of the supplied Prefab, including creating all objects and Kismet sequence.
 */
void APrefabInstance::InstancePrefab(UPrefab* InPrefab)
{
	check(InPrefab);
#if WITH_EDITORONLY_DATA
	if ( FLevelUtils::IsLevelLocked(GetLevel()) )
	{
		warnf(TEXT("UpdatePrefab: %s"), *LocalizeUnrealEd(TEXT("Error_OperationDisallowedOnLockedLevel")));
		return;
	}

	// when instancing actors for a prefab, the current level may not be the level that contains this PrefabInstance, which would result in any actors we create here
	// being spawned into a different level than this PrefabInstance (BAD!)
	// temporarily change the CurrentLevel to the level that contains this PrefabInstance; we'll restore it when we're done
	ULevel* RealCurrentLevel = GWorld->CurrentLevel;
	GWorld->CurrentLevel = GetLevel();

	TemplatePrefab = InPrefab;
	TemplateVersion = InPrefab->PrefabVersion;

	// Create transform for new Prefab, so we can transform each actors location into world space.
	FRotationTranslationMatrix PrefabTM(Rotation,Location);

	// create a map for archetypes and actor components - this is so we can fix up references, but not store
	// the components in the persistent map in the PrefabInstance
	TMap<UObject*, UObject*> ArchetypeAndComponentToInstanceMap;

	// Iterate over each Archetype, instancing it.
	for(INT i=0; i<TemplatePrefab->PrefabArchetypes.Num(); i++)
	{
		UObject* Archetype = TemplatePrefab->PrefabArchetypes(i);
		UObject* NewObj = NULL;
		AActor* ArcActor = NULL;
		AActor* NewActor = NULL;
		if(Archetype->IsA(AActor::StaticClass()))
		{
			ArcActor = CastChecked<AActor>(Archetype);
			FRotationTranslationMatrix RelTM(ArcActor->Rotation,ArcActor->Location);
			FMatrix WorldTM = RelTM * PrefabTM;

			NewObj = NewActor = GWorld->SpawnActor(Archetype->GetClass(), NAME_None, WorldTM.GetOrigin(), WorldTM.Rotator(), ArcActor, TRUE);

			// Special handling for Brushes
			CopyModelIfBrush(NewObj, Archetype);
		}
		else
		{
			// @todo Handle instancing non-Actor classes.
		}

		if(NewObj)
		{
			// Add new object to class->instance map.
			ArchetypeToInstanceMap.Set( Archetype, NewObj );
			ArchetypeAndComponentToInstanceMap.Set(Archetype, NewObj);
		}

 		// put the actor's components into the InstanceToArchetype map
 		if (ArcActor)
 		{
 			check(ArcActor->Components.Num() == NewActor->Components.Num());
 
 			for (INT ComponentIndex = 0; ComponentIndex < ArcActor->Components.Num(); ComponentIndex++)
 			{
 				check(ArcActor->Components(ComponentIndex)->GetClass() == NewActor->Components(ComponentIndex)->GetClass());
				// only add compontents to the local map
				ArchetypeAndComponentToInstanceMap.Set(ArcActor->Components(ComponentIndex), NewActor->Components(ComponentIndex));
 			}
 		}
	}

	// Now fix up references in new objects so they point to new instanced objects instead of archetype objects.
	UPrefab::ResolveInterDependencies(ArchetypeAndComponentToInstanceMap, false);

	// Allow instanced objects to update internal state by calling PostEditChange on each.
	for ( TMap<UObject*,UObject*>::TIterator It(ArchetypeToInstanceMap); It; ++It )
	{
		UObject* NewObj = It.Value();
		NewObj->PostEditChange();
	}

	// Now instance the Kismet sequence if we have one into the 'Prefabs' subsequence.
	if(InPrefab->PrefabSequence)
	{
		InstanceKismetSequence(InPrefab->PrefabSequence, FString(TEXT("")));
	}

#if CHECK_PREFAB_BRUSHES
	CheckPrefabBrushes(this);
#endif

	GWorld->CurrentLevel = RealCurrentLevel;
#endif // WITH_EDITORONLY_DATA
}

/**
 *	Do any teardown to destroy anything instanced for this PrefabInstance. 
 *	Sets TemplatePrefab back to NULL.
 */
void APrefabInstance::DestroyPrefab(USelection* Selection)
{
	Modify();

	// Destroy any actors that make up this prefab
	for ( TMap<UObject*,UObject*>::TIterator It(ArchetypeToInstanceMap); It; ++It )
	{
		UObject* Archetype = It.Key();
		UObject* Instance = It.Value();

		AActor* Actor = Cast<AActor>(Instance);
		if(Actor)
		{
			// DestroyActor will call Modify()
			GWorld->DestroyActor(Actor);
			Selection->Deselect(Actor);
		}
	}
	ArchetypeToInstanceMap.Empty();

	// Destroy Kismet sequence.
	DestroyKismetSequence();

	// set Prefab pointer to NULL.
	TemplatePrefab = NULL;
	TemplateVersion = 0;
}

/** 
 *	Convert this prefab instance to look like the Prefab archetype version of it (by changing object refs to archetype refs and 
 *	converting positions to local space). Then serialise it, so we only get things that are unique to this instance. We store this
 *	archive in the PrefabInstance.
 */
void APrefabInstance::SavePrefabDifferences()
{
	// Create Inst-to-Arch map by inverting Arch-to-Inst map
	TMap<UObject*,UObject*> InstanceToArchetypeMap;
	CreateInverseMap(InstanceToArchetypeMap, ArchetypeToInstanceMap);

	FRotationTranslationMatrix PrefabToWorld(Rotation,Location);
	FMatrix InversePrefabToWorld = PrefabToWorld.Inverse();

	// Set archive to write mode.
	FPrefabUpdateArc UpdateAr;
	UpdateAr.SetPortFlags(PPF_DeepCompareInstances);
	UpdateAr.SetPersistant(TRUE);
	UpdateAr.ActivateWriter();

	// this value represents 1/2 degree in Unreal Units
	const static FLOAT RotationTolerance = 65536.f * (0.5f / 360.f);

	// Add objects to 
	for ( TMap<UObject*,UObject*>::TIterator InstIt(ArchetypeToInstanceMap); InstIt; ++InstIt )
	{
		UObject* ObjArch = InstIt.Key();
		UObject* ObjInst = InstIt.Value();
		if(ObjInst)
		{
			FRotator CurrentRotation(0,0,0);			
			AActor* ActorInst = Cast<AActor>(ObjInst);

			// Clear components here so less complex stuff to serialize
			if(ActorInst)
			{
				// explicitly clear the base when referencing actors in other levels
				if ( ActorInst->Base && ActorInst->Base->GetOuter() != ActorInst->GetOuter() )
				{
					SetBase(NULL);
				}

				ActorInst->ClearComponents();
				CurrentRotation = ActorInst->Rotation;
			}

			// Temporarily turn references to other objects in the prefab into archetype references, so it matches the archetype.
			FArchiveReplaceObjectRef<UObject>(ObjInst, InstanceToArchetypeMap, false, true, true);

			// Turn world locations back into relative locations.
			APrefabInstance::ApplyTransformIfActor(ObjInst, InversePrefabToWorld);

			// Because transforming from local->world->local may have introduced some errors, we snap the location before serializing.
			if(ActorInst)
			{
				// ObjArch can be null if a class was deleted and this Prefab was referencing it
				if( ObjArch != NULL )
				{
					AActor* ActorArch = CastChecked<AActor>(ObjArch);

					if((ActorInst->Location - ActorArch->Location).Size() < 0.1f)
					{
						ActorInst->Location = ActorArch->Location;
					}

					// if the actor's rotation was different by less than a degree, it's probably due the inherent precision loss of extracting a rotator from the translation matrix
					if (Abs(ActorArch->Rotation.Pitch - ActorInst->Rotation.Pitch) < RotationTolerance
						&&	Abs(ActorArch->Rotation.Yaw - ActorInst->Rotation.Yaw) < RotationTolerance
						&&	Abs(ActorArch->Rotation.Roll - ActorInst->Rotation.Roll) < RotationTolerance)
					{
						// correct it
						ActorInst->Rotation = ActorArch->Rotation;
					}
				}
			}

			// Now save object. This will save properties which we have actually changed since we instanced the actor.
			UpdateAr.SerializeObject(ObjInst);

			// Turn back to normal.
			FArchiveReplaceObjectRef<UObject>(ObjInst, ArchetypeToInstanceMap, false, true, true);
			APrefabInstance::ApplyTransformIfActor(ObjInst, PrefabToWorld);

			// Init components again
			if(ActorInst)
			{
				// if the actor's rotation was changed by less than a degree, it's probably due the inherent precision loss of extracting a rotator from the translation matrix
				if (ActorInst != NULL
				&&	Abs(CurrentRotation.Pitch - ActorInst->Rotation.Pitch) < RotationTolerance
				&&	Abs(CurrentRotation.Yaw - ActorInst->Rotation.Yaw) < RotationTolerance
				&&	Abs(CurrentRotation.Roll - ActorInst->Rotation.Roll) < RotationTolerance)
				{
					// correct it
					ActorInst->Rotation = CurrentRotation;
				}
				ActorInst->ForceUpdateComponents();
			}
		}
	}

	// Keep information from this archive.
	CopyFromArchive(&UpdateAr);
}

/**
 *	Utility for getting all Actors that are part of this PrefabInstance.
 */
void APrefabInstance::GetActorsInPrefabInstance( TArray<AActor*>& OutActors ) const
{
	// Iterate over its map from Archetype to Instances
	for ( TMap<UObject*,UObject*>::TConstIterator InstIt(ArchetypeToInstanceMap); InstIt; ++InstIt )
	{
		// Add any entries which are Actors.
		const UObject* ArcInst = InstIt.Value();
		const AActor* ActorInst = Cast<AActor>(ArcInst);

		if(ActorInst && !ActorInst->bDeleteMe && !ActorInst->IsPendingKill())
		{
			OutActors.AddItem( const_cast<AActor*>(ActorInst) );
		}
	}
}

/**
 * Examines the selection status of each actor in this prefab instance, and
 * returns TRUE if the selection state of all actors matches the input state.
 */
UBOOL APrefabInstance::GetActorSelectionStatus(UBOOL bInSelected) const
{
	UBOOL bAllActorsMatchSelectionStatus = TRUE;

	TArray<AActor*> ActorsInPrefabInstance;
	GetActorsInPrefabInstance( ActorsInPrefabInstance );
	for( INT ActorIndex = 0 ; ActorIndex < ActorsInPrefabInstance.Num() ; ++ActorIndex )
	{
		AActor* PrefabActor = ActorsInPrefabInstance(ActorIndex);
		const UBOOL bSeleced = PrefabActor->IsSelected();
		if ( bSeleced != bInSelected )
		{
			bAllActorsMatchSelectionStatus = FALSE;
			break;
		}
	}

	return bAllActorsMatchSelectionStatus;
}

/**
 * Iterates through the ArchetypeToInstanceMap and verifies that the archetypes for each of this PrefabInstance's actors exist.
 * For any actors contained by this PrefabInstance that do not have a corresponding archetype, removes the actor from the
 * ArchetypeToInstanceMap.  This is normally caused by adding a new actor to a PrefabInstance, updating the source Prefab, then loading
 * a new map without first saving the package containing the updated Prefab.  When the original map is reloaded, though it contains
 * an entry for the newly added actor, the source Prefab's linker does not contain an entry for the corresponding archetype.
 *
 * @return	TRUE if each pair in the ArchetypeToInstanceMap had a valid archetype.  FALSE if one or more archetypes were NULL.
 */
UBOOL APrefabInstance::VerifyMemberArchetypes()
{
	UBOOL bMissingArchetypes = FALSE;

	for ( TMap<UObject*,UObject*>::TIterator It(ArchetypeToInstanceMap); It; ++It )
	{
		UObject* MemberInstance = It.Value();
		if ( It.Key() == NULL )
		{
			// this prefab member doesn't have a corresponding archetype
			bMissingArchetypes = TRUE;

			// notify the user that this prefab member was removed
			warnf(NAME_Warning, LocalizeSecure(LocalizeUnrealEd(TEXT("Prefab_MissingArchetypes")), *MemberInstance->GetFullName(), *GetPathName()));

			// now remove it
			It.RemoveCurrent();

			// the package needs to be resaved, so mark it dirty
			MarkPackageDirty();
		}
	}

	return !bMissingArchetypes;
}

/** Copy information to a FPrefabUpdateArchive from this PrefabInstance for updating a PrefabInstance with. */
void APrefabInstance::CopyToArchive(FPrefabUpdateArc* InArc)
{
	// package versions weren't always saved with the PrefabInstance data, so if the package versions match the default values,
	// then we'll attempt to pull the correct versions from the PrefabInstance's Linker.  This won't work if the PrefabInstance
	// has already been detached from its linker for whatever reason.
	if ( PI_PackageVersion == INDEX_NONE )
	{
		PI_PackageVersion = GetLinkerVersion();
	}

	if ( PI_LicenseePackageVersion == INDEX_NONE )
	{
		PI_LicenseePackageVersion = GetLinkerLicenseeVersion();
	}

	// when reading the stored property data from this prefab instance's PI_Bytes array, we need to make sure
	// that the archive's version is the same as the version used when the package containing this prefab instance
	// was saved, so that any compatibility code works as expected.
	InArc->SetVer( PI_PackageVersion );
	InArc->SetLicenseeVer( PI_LicenseePackageVersion );

	// Remove null objects that could result from a deleted class
	PI_CompleteObjects.RemoveItem(NULL);
	PI_ReferencedObjects.RemoveItem(NULL);

	InArc->Bytes				= PI_Bytes;
	InArc->CompleteObjects		= PI_CompleteObjects;
	InArc->ReferencedObjects	= PI_ReferencedObjects;
	InArc->SavedNames			= PI_SavedNames;
	InArc->ObjectMap			= PI_ObjectMap;
}

/** Copy information from a FPrefabUpdateArchive into this PrefabInstance for saving etc. */
void APrefabInstance::CopyFromArchive(FPrefabUpdateArc* InArc)
{
	PI_PackageVersion			= GPackageFileVersion;
	PI_LicenseePackageVersion	= GPackageFileLicenseeVersion;

	PI_Bytes				= InArc->Bytes;
	PI_ObjectMap			= InArc->ObjectMap;
	
	PI_SavedNames			= InArc->SavedNames;
	PI_CompleteObjects.Empty(InArc->CompleteObjects.Num());
	InArc->CompleteObjects.GenerateKeyArray(PI_CompleteObjects);

	PI_ReferencedObjects.Empty(InArc->ReferencedObjects.Num());
	InArc->ReferencedObjects.GenerateKeyArray(PI_ReferencedObjects);
}


/*-----------------------------------------------------------------------------
	UPrefab
-----------------------------------------------------------------------------*/

/** Print the number of archetypes that make up the prefab. */
FString UPrefab::GetDesc()
{
	// Count the number of actor archetypes in the prefab.
	INT NumActors = 0;
	for(INT i=0; i<PrefabArchetypes.Num(); i++)
	{
		if(PrefabArchetypes(i)->IsA(AActor::StaticClass()))
		{
			NumActors++;
		}
	}

	// If we have a Kismet sequence, show the total number of kismet sequence objects as well.
	if(PrefabSequence)
	{
		TArray<USequenceObject*> SeqObjs;
		PrefabSequence->FindSeqObjectsByClass(USequenceObject::StaticClass(), SeqObjs, true);

		return FString::Printf( TEXT("%d Actors, %d Kismet Objs"), NumActors, SeqObjs.Num() );
	}
	else
	{
		return FString::Printf( TEXT("%d Actors, No Kismet"), NumActors );
	}
}

/**
 * Called after the data for this prefab has been loaded from disk.  Removes any NULL elements from the PrefabArchetypes array.
 */
void UPrefab::PostLoad()
{
	Super::PostLoad();

	// remove any NULL elements from the PrefabArchetypes array
	if ( PrefabArchetypes.ContainsItem(NULL) )
	{
		warnf(NAME_Warning, TEXT("Removing null elements from PrefabArchetypes array '%s'"), *GetFullName());
		PrefabArchetypes.RemoveItem(NULL);
	}

	if ( PrefabSequence != NULL )
	{
		TArray<USequenceObject*> SeqObjs;
		PrefabSequence->FindSeqObjectsByClass(USequenceObject::StaticClass(), SeqObjs, TRUE);
		SeqObjs.AddUniqueItem(PrefabSequence);

		for ( INT ObjIndex = 0; ObjIndex < SeqObjs.Num(); ObjIndex++ )
		{
			SeqObjs(ObjIndex)->SetFlags(RF_Public|RF_Transactional);
		}
	}
}

/**
 * Fixes up object references for a group of archetypes.  For any references which refer 
 * to an actor from the original set, replaces that reference with a reference to the archetype
 * class itself.  For any references which refer to actors contained within the map package and not in the archetype set, 
 * null the reference to that actor in the archetype.
 * 
 * @param	ArchetypeBaseMap	map of original object instances to archetype instances
 * @param	bNullPrivateRefs	should we null references to any private objects
 */
void UPrefab::ResolveInterDependencies( TMap<UObject*,UObject*>& ArchetypeBaseMap, UBOOL bNullPrivateRefs )
{
	for ( TMap<UObject*,UObject*>::TIterator It(ArchetypeBaseMap); It; ++It )
	{
		UObject* OriginalObject = It.Key();
		UObject* ArchetypeRef = It.Value();

		check(OriginalObject);
		check(ArchetypeRef);
		check(ArchetypeRef->IsA(OriginalObject->GetClass()));

		// replace any references to actors in the set with references to the archetype version of that object
		FArchiveReplaceObjectRef<UObject>(ArchetypeRef, ArchetypeBaseMap, bNullPrivateRefs, true, true);
	}
}


/** Utility for copying a USequence from the level into a Prefab in a package, including fixing up references. */
void UPrefab::CopySequenceIntoPrefab(USequence* InPrefabSeq, TMap<UObject*,UObject*>& InstanceToArchetypeMap)
{
#if WITH_EDITORONLY_DATA
	// Use StaticDuplicateObject to create a copy of the sequence and all its subobjects.
	FString NewSeqName = FString::Printf( TEXT("%s_Seq"), *GetName() );

	FObjectDuplicationParameters Parms(InPrefabSeq, this);
	Parms.DestClass = UPrefabSequence::StaticClass();
	Parms.ApplyFlags = RF_Public|RF_ArchetypeObject|RF_Transactional;

	PrefabSequence = CastChecked<UPrefabSequence>( StaticDuplicateObjectEx(Parms) );
	PrefabSequence->ObjName = NewSeqName;
	PrefabSequence->ParentSequence = NULL;

	// Check all the ParentSequence pointers within the sequence are right.
	PrefabSequence->CheckParentSequencePointers();

	// Reset location
	PrefabSequence->ObjPosX = 0;
	PrefabSequence->ObjPosY = 0;

	// Clean up any connections the sequence may have had to other things.
	PrefabSequence->CleanupConnections();

	// Convert any references to existing instances into archetype references.
	// Also NULL out any references to other private stuff outside the sequence (like references to other Actors in the level).
	FArchiveReplaceObjectRef<UObject> ReplaceAr(PrefabSequence, InstanceToArchetypeMap, PrefabSequence->GetOutermost() != InPrefabSeq->GetOutermost(), true, true);
#endif // WITH_EDITORONLY_DATA
}

/*-----------------------------------------------------------------------------
	UPrefabSequenceContainer
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UPrefabSequenceContainer);

/* === UObject interface === */
/**
 * Called after importing property values for this object (paste, duplicate or .t3d import)
 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
 * are unsupported by the script serialization
 *
 * Updates the value of ObjName to match the name of the sequence.
 */
void UPrefabSequenceContainer::PostEditImport()
{
	Super::PostEditImport();
	ObjName = GetName();
}

/**
 * Called after this object is renamed; updates the value of ObjName to match the name of the sequence.
 */
void UPrefabSequenceContainer::PostRename()
{
	Super::PostRename();
	ObjName = GetName();
}

/**
 * Called after duplication & serialization and before PostLoad.
 *
 * Updates the value of ObjName to match the name of the sequence.
 */
void UPrefabSequenceContainer::PostDuplicate()
{
	Super::PostDuplicate();
	ObjName = GetName();
}


/*-----------------------------------------------------------------------------
	UPrefabSequence
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UPrefabSequence);
/* === UPrefabSequence interface === */
/**
 * Accessor for setting the value of OwnerPrefab.
 *
 * @param	InOwner		the PrefabInstance that created this PrefabSequence. 
 */
void UPrefabSequence::SetOwnerPrefab( APrefabInstance* inOwner )
{
	OwnerPrefab = inOwner;
}
/**
 * Wrapper for retrieving the current value of OwnerPrefab.
 *
 * @return	a reference to the PrefabInstance that created this PrefabSequence
 */
APrefabInstance* UPrefabSequence::GetOwnerPrefab() const
{
	return OwnerPrefab;
}

/* === UObject interface === */
void UPrefabSequence::PostLoad()
{
	// if this is the first time PostLoad() has been called on this object, and we're in a prefab instance
	if ( !GIsGame && !HasAnyFlags(RF_DebugPostLoad) )
	{
		TArray<UObject*> Subobjects;
		FArchiveObjectReferenceCollector Collector(&Subobjects, this, FALSE, TRUE, TRUE, TRUE);
		Serialize(Collector);

		if ( IsInPrefabInstance()
		||	(!HasAnyFlags(RF_ArchetypeObject) && !IsAPrefabArchetype()) )
		{
			UBOOL bMarkPackageDirty = FALSE;
			for ( INT ObjIndex = 0; ObjIndex < Subobjects.Num(); ObjIndex++ )
			{
				UObject* Obj = Subobjects(ObjIndex);
				if ( Obj->HasAnyFlags(RF_ArchetypeObject) )
				{
					bMarkPackageDirty = TRUE;
					Obj->ClearFlags(RF_ArchetypeObject);
				}
			}

			if ( bMarkPackageDirty )
			{
				MarkPackageDirty(TRUE);
			}
		}
		else
		{
			// skip over the sequence version as during creation of the prefab (i.e. duplication) our ParentSequence will still
			// be pointing to the sequence from the level (CopySequenceIntoPrefab will take care of clearing it)
			checkf(UObject::IsAPrefabArchetype(),TEXT("PrefabSequence not inside a prefab! %s   Outer:%s   Arc: %s   Outer's Arc: %s"),
				*GetFullName(), *GetOuter()->GetFullName(), *GetArchetype()->GetFullName(), *GetOuter()->GetArchetype()->GetFullName());

			UBOOL bMarkPackageDirty = !HasAllFlags(RF_Public|RF_ArchetypeObject);
			SetFlags(RF_Public|RF_ArchetypeObject);
			for ( INT ObjIndex = 0; ObjIndex < Subobjects.Num(); ObjIndex++ )
			{
				UObject* Obj = Subobjects(ObjIndex);
				if ( !Obj->HasAllFlags(RF_Public|RF_ArchetypeObject) )
				{
					bMarkPackageDirty = TRUE;
					Obj->SetFlags(RF_Public|RF_ArchetypeObject);
				}
			}

			if ( bMarkPackageDirty )
			{
				MarkPackageDirty(TRUE);
			}
		}
	}

	Super::PostLoad();
}

/**
 * Called after importing property values for this object (paste, duplicate or .t3d import)
 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
 * are unsupported by the script serialization
 *
 * Updates the value of ObjName to match the name of the sequence.
 */
void UPrefabSequence::PostEditImport()
{
	Super::PostEditImport();
	ObjName = GetName();
}

/**
 * Called after this object is renamed; updates the value of ObjName to match the name of the sequence.
 */
void UPrefabSequence::PostRename()
{
	Super::PostRename();
	ObjName = GetName();
}

/**
 * Called after duplication & serialization and before PostLoad.
 *
 * Updates the value of ObjName to match the name of the sequence.
 */
void UPrefabSequence::PostDuplicate()
{
	Super::PostDuplicate();
	ObjName = GetName();
}

/*-----------------------------------------------------------------------------
	FPrefabUpdateArc
-----------------------------------------------------------------------------*/
/** Constructor */
FPrefabUpdateArc::FPrefabUpdateArc()
: FReloadObjectArc()
{
	// since we store the serialized bytes and object references into a persistent location, we must ignore transient objects
	// since they won't be saved into the package
	bAllowTransientObjects = FALSE;

	//@todo ronp - prefabs need to set bInstanceSubobjectsOnLoad to FALSE because prefabs remap object references to make
	// the instance look like the archetype, but before this can be done, the calling code must be updated to call InstanceSubobjectTemplates
	// on the prefab instance's actors
	//bInstanceSubobjectsOnLoad=FALSE;
}

/**
 *	I/O function for FName
 *	Names are saved as indexes into an array of strings which is stored separately. This is because the name
 *	index may not be the same between saving and loading the map.
 */
FArchive& FPrefabUpdateArc::operator<<( class FName& Name )
{
	INT NameIndex;
	if ( IsLoading() )
	{
		Reader << NameIndex;

		check(NameIndex < SavedNames.Num());

		Name = FName(*SavedNames(NameIndex), FNAME_Add, TRUE);
	}
	else if ( IsSaving() )
	{
		// Make a String from the supplied name.
		FString NameString = Name.ToString();

		// See if this string is already in the table.
		NameIndex = SavedNames.FindItemIndex(NameString);

		// If not, add it now.
		if(NameIndex == INDEX_NONE)
		{
			NameIndex = SavedNames.AddItem(NameString);
		}

		Writer << NameIndex;
	}
	return *this;
}
