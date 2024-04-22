/*=============================================================================
	UnObjGC.cpp: Unreal object garbage collection code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
   Garbage collection.
-----------------------------------------------------------------------------*/

#define PERF_DETAILED_PER_CLASS_GC_STATS				(_DEBUG || LOOKING_FOR_PERF_ISSUES)

// SHIPPING_PC_GAME has GShouldVerifyGCAssumptions=FALSE by default
#define VERIFY_DISREGARD_GC_ASSUMPTIONS			!FINAL_RELEASE

// Not enabled on console as we don't enable exception handling there.
#define CATCH_GC_CRASHES						(!CONSOLE)

#define	ENABLE_GC_DEBUG_OUTPUT					1

/** Object count during last mark phase																				*/
INT		GObjectCountDuringLastMarkPhase			= 0;
/** Count of objects purged since last mark phase																	*/
INT		GPurgedObjectCountSinceLastMarkPhase	= 0;
/**
 * If set and VERIFY_DISREGARD_GC_ASSUMPTIONS is true, we verify GC assumptions about RF_DisregardForGC. We also
 * verify that no unreachable actors/ components are referenced if VERIFY_NO_UNREACHABLE_OBJECTS_ARE_REFERENCED
 * is true.
 */
UBOOL	GShouldVerifyGCAssumptions				= !SHIPPING_PC_GAME && !MOBILE;

#if PERF_DETAILED_PER_CLASS_GC_STATS
/** Map from a UClass' FName to the number of objects that were purged during the last purge phase of this class.	*/
static TMap<const FName,DWORD> GClassToPurgeCountMap;
/** Map from a UClass' FName to the number of RF_DisregardForGC object references followed for all instances.		*/
static TMap<const FName,DWORD> GClassToDisregardedObjectRefsMap;
/** Map from a UClass' FName to the number of regular object references followed for all instances.					*/
static TMap<const FName,DWORD> GClassToRegularObjectRefsMap;
/** Map from a UClass' FName to the number of cycles spent with GC.													*/
static TMap<const FName,DWORD> GClassToCyclesMap;

/** Number of disregarded object refs for current object.															*/
static DWORD GCurrentObjectDisregardedObjectRefs;
/** Number of regulard object refs for current object.																*/
static DWORD GCurrentObjectRegularObjectRefs;

/**
 * Helper structure used for sorting class to count map.
 */
struct FClassCountInfo
{
	FName	ClassName;
	DWORD	InstanceCount;
};
IMPLEMENT_COMPARE_CONSTREF(FClassCountInfo,UnObjGC,{ return B.InstanceCount - A.InstanceCount; });

/**
 * Helper function to log the various class to count info maps.
 *
 * @param	LogText				Text to emit between number and class 
 * @param	ClassToCountMap		TMap from a class' FName to "count"
 * @param	NumItemsToList		Number of items to log
 * @param	TotalCount			Total count, if 0 will be calculated
 */
static void LogClassCountInfo( const TCHAR* LogText, TMap<const FName,DWORD>& ClassToCountMap, INT NumItemsToLog, DWORD TotalCount )
{
	// Array of class name and counts.
	TArray<FClassCountInfo> ClassCountArray;
	ClassCountArray.Empty( ClassToCountMap.Num() );

	// Figure out whether we need to calculate the total count.
	UBOOL bNeedToCalculateCount = FALSE;
	if( TotalCount == 0 )
	{
		bNeedToCalculateCount = TRUE;
	}
	// Copy TMap to TArray for sorting purposes (and to calculate count if needed).
	for( TMap<const FName,DWORD>::TIterator It(ClassToCountMap); It; ++It )
	{
		FClassCountInfo ClassCountInfo;
		ClassCountInfo.ClassName		= It.Key();
		ClassCountInfo.InstanceCount	= It.Value();
		ClassCountArray.AddItem( ClassCountInfo );
		if( bNeedToCalculateCount )
		{
			TotalCount += ClassCountInfo.InstanceCount;
		}
	}
	// Sort array by instance count.
	Sort<USE_COMPARE_CONSTREF(FClassCountInfo,UnObjGC)>( ClassCountArray.GetTypedData(), ClassCountArray.Num() );

	// Log top NumItemsToLog class counts
	for( INT Index=0; Index<Min(NumItemsToLog,ClassCountArray.Num()); Index++ )
	{
		const FClassCountInfo& ClassCountInfo = ClassCountArray(Index);
		const FLOAT Percent = 100.f * ClassCountInfo.InstanceCount / TotalCount;
		debugf( NAME_DevGarbage, TEXT("%5d [%5.2f%%] %s Class %s"), ClassCountInfo.InstanceCount, Percent, LogText, *ClassCountInfo.ClassName.ToString() ); 
	}

	// Empty the map for the next run.
	ClassToCountMap.Empty();
};
#endif


/** Array of callbacks called first thing in UObject::CollectGarbage. */
FGCCallback	GPreGarbageCollectionCallbacks[10];

/** Array of callbacks called last thing in UObject::CollectGarbage. */
FGCCallback	GPostGarbageCollectionCallbacks[10];

/** Wrapper function to handle default parameter when used as function pointer */
static void FlushAsyncLoadingWrapper()
{
	UObject::FlushAsyncLoading();
}

// Register GC callback for FlushAsyncLoading
IMPLEMENT_PRE_GARBAGE_COLLECTION_CALLBACK( FlushAsyncLoading, FlushAsyncLoadingWrapper, GCCB_PRE_FlushAsyncLoading );

/**
 * Registers passed in callback to be called before UObject::CollectGarbage executes.
 *
 * @param	Callback	Callback to register
 * @param	Index		Index to register at.
 * @return index into callback array this entry has been assigned
 */
INT GRegisterPreGCCallback( FGCCallback Callback, INT Index )
{
	check(Index<ARRAY_COUNT(GPreGarbageCollectionCallbacks));
	check(GPreGarbageCollectionCallbacks[Index] == NULL);
	GPreGarbageCollectionCallbacks[Index] = Callback;
	return Index;
}

/**
 * Registers passed in callback to be called before UObject::CollectGarbage returns.
 *
 * @param	Callback	Callback to register
 * @param	Index		Index to register at.
 * @return index into callback array this entry has been assigned
 */
INT GRegisterPostGCCallback( FGCCallback Callback, INT Index )
{
	check(Index<ARRAY_COUNT(GPostGarbageCollectionCallbacks));
	check(GPostGarbageCollectionCallbacks[Index] == NULL);
	GPostGarbageCollectionCallbacks[Index] = Callback;
	return Index;
}


/**
 * Prefetches entries in object array. 
 *
 * @param	CurrentIndex		Current index in array that is being operated on
 * @param	PrefetchDistance	Amount to look ahead
 */
void FORCEINLINE PREFETCH_OBJECT_ARRAY( INT CurrentIndex, INT PrefetchDistance )
{
	// Prefetch object further ahead.
	if( (CurrentIndex + PrefetchDistance) < UObject::GObjObjects.Num() )
	{
		PREFETCH( UObject::GObjObjects(CurrentIndex + PrefetchDistance) );
	}	
}


/**
 * Serializes the global root set and objects that have any of the passed in KeepFlags to passed in archive.
 *
 * @param Ar			Archive to serialize with
 * @param KeepFlags		Objects with any of those flags will be serialized regardless of whether they are part of the root
 *						set or not.
 */
void UObject::SerializeRootSet( FArchive& Ar, EObjectFlags KeepFlags )
{
	for( FObjectIterator It; It; ++It )
	{
		UObject* Obj = *It;
		if(	Obj->HasAnyFlags(KeepFlags|RF_RootSet) )
		{
			Ar << Obj;
		}
	}
}

/**
 * Archive for tagging unreachable objects in a non recursive manner.
 */
class FArchiveTagUsedNonRecursive : public FArchive
{
public:
	/**
	 * Default constructor.
	 */
	FArchiveTagUsedNonRecursive()
	:	CurrentObject(NULL)
	{
		ArIsObjectReferenceCollector = TRUE;
	}

	/**
	 * Performs reachability analysis. This information is later used by e.g. IncrementalPurgeGarbage or IsReferenced. The 
	 * algorithm is a simple mark and sweep where all objects are marked as unreachable. The root set passed in is 
	 * considered referenced and also objects that have any of the KeepFlags but none of the IgnoreFlags. RF_PendingKill is 
	 * implicitly part of IgnoreFlags and no object in the root set can have this flag set.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not (see line below)
	 * @param IgnoreFlags	Objects with any of these flags will be ignored regardless of KeepFlags. Does not apply to objects
	 *						which are part of the root set.
	 */
	void PerformReachabilityAnalysis( EObjectFlags KeepFlags, EObjectFlags IgnoreFlags )
	{
		// Reset object count.
		GObjectCountDuringLastMarkPhase = 0;	

		// Iterate over all objects.
		for( FObjectIterator It; It; ++It )
		{
			UObject* Object	= *It;
			checkSlow(Object->IsValid());
			GObjectCountDuringLastMarkPhase++;

			// Special case handling for objects that are part of the root set.
			if( Object->HasAnyFlags( RF_RootSet ) )
			{
				checkSlow( Object->IsValid() );
				// We cannot use RF_PendingKill on objects that are part of the root set.
				checkCode( if( Object->HasAnyFlags( RF_PendingKill ) ) { appErrorf( TEXT("Object %s is part of root set though has been marked RF_PendingKill!"), *Object->GetFullName() ); } );
				// Add to list of objects to serialize.
				ObjectsToSerialize.AddItem( Object );
			}
			// Regular objects.
			else
			{
				// Remove all references to objects that are pending kill in the game.
				if( Object->IsPendingKill() && !Object->HasAnyFlags( RF_PendingKill ) )
				{
					// Make sure that setting pending kill is undoable if e.g. PerformReachabilityAnalysis is being called outside the
					// garbage collector, which is the case for the IsReferenced implementation.
					if( GUndo )
					{
						Object->Modify();
					}
					Object->MarkPendingKill();
				}

				// Mark objects as unreachable unless they have any of the passed in KeepFlags set and none of the passed in IgnoreFlags.
				if( Object->HasAnyFlags(KeepFlags) && !Object->HasAnyFlags( IgnoreFlags ) )
				{
					ObjectsToSerialize.AddItem( Object );
				}
				else
				{
					Object->SetFlags( RF_Unreachable );
				}
			}

			// Set debug flag check later after serialization to ensure that Serialize has been routed via Super::Serialize down to UObject::Serialize
			Object->ClearFlags( RF_DebugSerialize );
		}

		// Keep serializing objects till we reach the end of the growing array at which point
		// we are done.
		INT CurrentIndex = 0;
		while( CurrentIndex < ObjectsToSerialize.Num() )
		{
			CurrentObject = ObjectsToSerialize(CurrentIndex++);
			// Don't recurse into objects that have any of the ignore flags.
			if( !CurrentObject->HasAnyFlags( IgnoreFlags ) )
			{
				// Serialize object.
				CurrentObject->Serialize( *this );
				// Make sure Serialize gets correctly routed to UObject::Serialize.
				if( !CurrentObject->HasAnyFlags( RF_DebugSerialize ) )
				{
					appErrorf( TEXT("%s failed to route Serialize"), *CurrentObject->GetFullName() );
				}
			}
		}
	}

private:
	/**
	 * Adds passed in object to ObjectsToSerialize list and also removed RF_Unreachable
	 * which is used to signify whether an object already is in the list or not.
	 *
	 * @param	Object	object to add
	 */
	void AddToObjectList( UObject* Object )
	{
#if ENABLE_GC_DEBUG_OUTPUT
		// this message is to help track down culprits behind "Object in PIE world still referenced" errors
		if ( GIsEditor && !GIsPlayInEditorWorld && !CurrentObject->IsInPIEPackage() && Object->IsInPIEPackage() )
		{
			warnf(NAME_Warning, TEXT("GC detected illegal reference to PIE object from content [possibly via %s]:"), *GSerializedProperty->GetFullName());
			warnf(NAME_Warning, TEXT("      PIE object: %s"), *Object->GetFullName());
			warnf(NAME_Warning, TEXT("  NON-PIE object: %s"), *CurrentObject->GetFullName());
		}
#endif

		// Mark it as reachable.
		Object->ClearFlags( RF_Unreachable | RF_DebugSerialize );

		// Add it to the list of objects to serialize.
		ObjectsToSerialize.AddItem( Object );
	}

	FArchive& operator<<( UObject*& Object )
	{
#if !_WIN64
		// we are compiling 64bit config with 4byte alignment for now, so this assertion is invalid until we are 8 byte aligned.
		check( (((PTRINT) &Object) & (sizeof(PTRINT)-1)) == 0 );
#endif
		checkSlow( !Object || Object->IsValid() );		
		if( Object )
		{
			// Remove references to pending kill objects if we're allowed to do so.
			if( Object->HasAnyFlags( RF_PendingKill ) && IsAllowingReferenceElimination() )
			{
				// Make sure to handle undo/ redo for the Editor.
				CurrentObject->Modify();
				// Null out reference.
				Object = NULL;
			}
			// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
			else if( Object->HasAnyFlags( RF_Unreachable ) )
			{
				AddToObjectList( Object );
			}

		}
		return *this;
	}

	/** Object we're currently serializing */
	UObject*			CurrentObject;
	/** Growing array of objects that require serialization */
	TArray<UObject*>	ObjectsToSerialize;
};


/**
 * Handles object reference, potentially NULL'ing
 *
 * @param Object						Object pointer passed by reference
 * @param bAllowReferenceElimination	Whether to allow NULL'ing the reference if RF_PendingKill is set
 */
static FORCEINLINE void HandleObjectReference( TArray<UObject*>& ObjectsToSerialize, UObject*& Object, UBOOL bAllowReferenceElimination )
{
	checkSlow( Object == NULL || Object->IsValid() );
	// Disregard NULL objects and perform very fast check to see whether object is part of permanent
	// object pool and should therefore be disregarded. The check doesn't touch the object and is
	// cache friendly as it's just a pointer compare against to globals.
	if( Object )
	{
		if( !Object->ResidesInPermanentPool() )
		{
			// Remove references to pending kill objects if we're allowed to do so.
			if( Object->HasAnyFlags( RF_PendingKill ) && bAllowReferenceElimination )
			{
				// Null out reference.
				Object = NULL;
			}
			// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
			else if( Object->HasAnyFlags( RF_Unreachable ) )
			{
				// Mark it as reachable.
				Object->ClearFlags( RF_Unreachable );
				// Add it to the list of objects to serialize.
				ObjectsToSerialize.AddItem( Object );
			}
#if PERF_DETAILED_PER_CLASS_GC_STATS
			GCurrentObjectRegularObjectRefs++;
		}
		else
		{
			GCurrentObjectDisregardedObjectRefs++;
		}
#else
		}
#endif
	}
}

/**
 * Implementation of realtime garbage collector.
 *
 * The approach is to create an array of DWORD tokens for each class that describe object references. This is done for 
 * script exposed classes by traversing the properties and additionally via manual function calls to emit tokens for
 * native only classes in the StaticConstructor. A third alternative is a AddReferencedObjects callback per object which 
 * is used to deal with object references from types that aren't supported by UnrealScript's type system.
 * interface doesn't make sense to implement for.
 */
class FArchiveRealtimeGC
{
private:
	/** Helper struct for stack based approach */
	struct FStackEntry
	{
		/** Current data pointer, incremented by stride */
		BYTE*	Data;
		/** Current stride */
		INT		Stride;
		/** Current loop count, decremented each iteration */
		INT		Count;
		/** First token index in loop */
		INT		LoopStartIndex;
	};

public:
	/** Default constructor, initializing all members. */
	FArchiveRealtimeGC()
	: CurrentObject( NULL )
	{}

	/**
	 * Performs reachability analysis.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not
	 */
	void PerformReachabilityAnalysis( EObjectFlags KeepFlags )
	{
#if CATCH_GC_CRASHES
		try 
		{
#endif
		// Reset current object used for debugging. A NULL value indicates we're not currently in the 
		// serialization/ token loop.
		CurrentObject = NULL;

		// Reset object count.
		GObjectCountDuringLastMarkPhase = 0;

		// Presize array and add a bit of extra slack for prefetching.
		ObjectsToSerialize.Empty( UObject::GObjObjects.Num() - UObject::GObjFirstGCIndex + 2 );

		// The UClass class.
		const UClass* UClassClass = UClass::StaticClass();

		// Iterate over all objects, inlining FObjectIterator and avoiding unnecessary IsA.
		for( INT ObjectIndex=UObject::GObjFirstGCIndex; ObjectIndex<UObject::GObjObjects.Num(); ObjectIndex++ )
		{
			UObject* Object = UObject::GObjObjects(ObjectIndex);

			// Prefetch object further ahead.
			PREFETCH_OBJECT_ARRAY( ObjectIndex, 10 );
			
			// Skip NULL entries.
			if( !Object )
			{
				continue;
			}

			// We can't collect garbage during an async load operation and by now all unreachable objects should've been purged.
			checkf( !Object->HasAnyFlags(RF_AsyncLoading|RF_Unreachable), TEXT("%s"), *Object->GetFullName() );
	
			// Keep track of how many objects are around.
			GObjectCountDuringLastMarkPhase++;

			// Special case handling for objects that are part of the root set.
			if( Object->HasAnyFlags( RF_RootSet ) )
			{
				checkSlow( Object->IsValid() );
				// We cannot use RF_PendingKill on objects that are part of the root set.
				checkCode( if( Object->HasAnyFlags( RF_PendingKill ) ) { appErrorf( TEXT("Object %s is part of root set though has been marked RF_PendingKill!"), *Object->GetFullName() ); } );
				ObjectsToSerialize.AddItem( Object );
			}
			// Regular objects.
			else
			{
				// Remove all references to objects that are pending kill in the game.
				if( Object->IsPendingKill() )
				{
					Object->MarkPendingKill();
				}

				// Mark objects as unreachable unless they have any of the passed in KeepFlags set and it's not marked for elimination..
				if( Object->HasAnyFlags( KeepFlags ) && !Object->HasAnyFlags( RF_PendingKill ) )
				{	
					ObjectsToSerialize.AddItem( Object );
				}
				else
				{
					Object->SetFlags( RF_Unreachable );
				}
			}

			// Assemble token stream for UClass objects. This is only done once for each class.
			if( Object->GetClass() == UClassClass && !Object->HasAnyFlags( RF_TokenStreamAssembled ) )
			{
				UClass* Class = CastChecked<UClass>(Object);
				Class->AssembleReferenceTokenStream();
			}
		}

		// Presized "recursion" stack for handling arrays and structs.
		TArray<FStackEntry> Stack;
		Stack.Add( 128 ); //@todo rtgc: need to add code handling more than 128 layers of recursion or at least assert
	
		// Keep serializing objects till we reach the end of the growing array at which point
		// we are done.
		INT CurrentIndex = 0;
		while( CurrentIndex < ObjectsToSerialize.Num() )
		{
#if PERF_DETAILED_PER_CLASS_GC_STATS
			DWORD StartCycles = appCycles();
#endif
			CurrentObject = ObjectsToSerialize(CurrentIndex++);

			// Poor man's prefetching. @todo rtgc: this needs to be significantly improved.
			char* NextObject = (char*) (ObjectsToSerialize.GetTypedData()[CurrentIndex]); // special syntax avoiding out of bounds checking
			PREFETCH( NextObject );
			PREFETCH( NextObject + 128 );
			PREFETCH( NextObject + 256 );
			PREFETCH( NextObject + 384 );

			//@todo rtgc: we could potentially add a class/ object flag to avoid calling this function but it might 
			//@todo rtgc; not really be worth it.
			CurrentObject->AddReferencedObjects( ObjectsToSerialize );

			//@todo rtgc: we need to handle object references in struct defaults

			// Make sure that token stream has been assembled at this point as the below code relies on it.
			checkSlow( CurrentObject->GetClass()->HasAllFlags( RF_TokenStreamAssembled ) );

			// Get pointer to token stream and jump to the start.
			FGCReferenceTokenStream* RESTRICT TokenStream = &CurrentObject->GetClass()->ReferenceTokenStream;
			DWORD TokenStreamIndex			= 0;
			// Keep track of index to reference info. Used to avoid LHSs.
			DWORD ReferenceTokenStreamIndex	= 0;

			// Create strack entry and initialize sane values.
			FStackEntry* RESTRICT StackEntry = &Stack(0);
			BYTE* StackEntryData		= (BYTE*) CurrentObject;
			StackEntry->Data			= StackEntryData;
			StackEntry->Stride			= 0;
			StackEntry->Count			= -1;
			StackEntry->LoopStartIndex	= -1;
			
			// Keep track of token return count in separate integer as arrays need to fiddle with it.
			INT TokenReturnCount		= 0;

			// Parse the token stream.
			while( TRUE )
			{
				// Cache current token index as it is the one pointing to the reference info.
				ReferenceTokenStreamIndex = TokenStreamIndex;

				// Handle returning from an array of structs, array of structs of arrays of ... (yadda yadda)
				for( INT ReturnCount=0; ReturnCount<TokenReturnCount; ReturnCount++ )
				{
					// Make sure there's no stack underflow.
					check( StackEntry->Count != -1 );

					// We pre-decrement as we're already through the loop once at this point.
					if( --StackEntry->Count > 0 )
					{
						// Point data to next entry.
						StackEntryData	 = StackEntry->Data + StackEntry->Stride;
						StackEntry->Data = StackEntryData;

						// Jump back to the beginning of the loop.
						TokenStreamIndex = StackEntry->LoopStartIndex;
						ReferenceTokenStreamIndex = StackEntry->LoopStartIndex;
						// We're not done with this token loop so we need to early out instead of backing out further.
						break;
					}
					else
					{
						StackEntry--;
						StackEntryData = StackEntry->Data;
					}
				}

				// Instead of reading information about reference from stream and caching it like below we access
				// the same memory address over and over and over again to avoid a nasty LHS penalty. Not reading 
				// the reference info means we need to manually increment the token index to skip to the next one.
				TokenStreamIndex++;
				// Helper to make code more readable and hide the ugliness that is avoiding LHSs from caching.
				#define	REFERENCE_INFO TokenStream->AccessReferenceInfo( ReferenceTokenStreamIndex )

				if( REFERENCE_INFO.Type == GCRT_Object )
				{	
					// We're dealing with an object reference.
					UObject**	ObjectPtr	= (UObject**)(StackEntryData + REFERENCE_INFO.Offset);
					UObject*&	Object		= *ObjectPtr;
					TokenReturnCount		= REFERENCE_INFO.ReturnCount;
					HandleObjectReference( ObjectsToSerialize, Object, TRUE );
				}
				else if( REFERENCE_INFO.Type == GCRT_ArrayObject )
				{
					// We're dealing with an array of object references.
					TArray<UObject*>& ObjectArray = *((TArray<UObject*>*)(StackEntryData + REFERENCE_INFO.Offset));
					TokenReturnCount = REFERENCE_INFO.ReturnCount;
					for( INT ObjectIndex=0; ObjectIndex<ObjectArray.Num(); ObjectIndex++ )
					{
						UObject*& Object = ObjectArray(ObjectIndex);
						HandleObjectReference( ObjectsToSerialize, Object, TRUE );
					}
				}
				else if( REFERENCE_INFO.Type == GCRT_ArrayStruct )
				{
					// We're dealing with a dynamic array of structs.
					const FScriptArray& Array = *((FScriptArray*)(StackEntryData + REFERENCE_INFO.Offset));
					StackEntry++;
					StackEntryData				= (BYTE*) Array.GetData();
					StackEntry->Data			= StackEntryData;
					StackEntry->Stride			= TokenStream->ReadStride( TokenStreamIndex );
					StackEntry->Count			= Array.Num();
					
					const FGCSkipInfo SkipInfo	= TokenStream->ReadSkipInfo( TokenStreamIndex );
					StackEntry->LoopStartIndex	= TokenStreamIndex;
					
					if( StackEntry->Count == 0 )
					{
						// Skip empty array by jumping to skip index and set return count to the one about to be read in.
						TokenStreamIndex		= SkipInfo.SkipIndex;
						TokenReturnCount		= TokenStream->GetSkipReturnCount( SkipInfo );
					}
					else
					{	
						// Loop again.
						check( StackEntry->Data );
						TokenReturnCount		= 0;
					}
				}
				else if( REFERENCE_INFO.Type == GCRT_PersistentObject )
				{
					// We're dealing with an object reference.
					UObject**	ObjectPtr	= (UObject**)(StackEntryData + REFERENCE_INFO.Offset);
					UObject*&	Object		= *ObjectPtr;
					TokenReturnCount		= REFERENCE_INFO.ReturnCount;
					HandleObjectReference( ObjectsToSerialize, Object, FALSE );
				}
				else if( REFERENCE_INFO.Type == GCRT_FixedArray )
				{
					// We're dealing with a fixed size array
					BYTE* PreviousData	= StackEntryData;
					StackEntry++;
					StackEntryData				= PreviousData;
					StackEntry->Data			= PreviousData;
					StackEntry->Stride			= TokenStream->ReadStride( TokenStreamIndex );
					StackEntry->Count			= TokenStream->ReadCount( TokenStreamIndex );
					StackEntry->LoopStartIndex	= TokenStreamIndex;
					TokenReturnCount			= 0;
				}
				else if( REFERENCE_INFO.Type == GCRT_EndOfStream )
				{
					// Break out of loop.
					break;
				}
				else if (REFERENCE_INFO.Type == GCRT_ScriptDelegate)
				{
					// Script delegate, which requires special handling because if we NULL the object reference, we need to 
					// clear the function name as well
					FScriptDelegate*	DelegatePtr = (FScriptDelegate*) (StackEntryData + REFERENCE_INFO.Offset);
					UObject*&			ObjectPtr	= DelegatePtr->Object;
					const UBOOL			bWasNULL	= (ObjectPtr == NULL);
					TokenReturnCount				= REFERENCE_INFO.ReturnCount;
					HandleObjectReference( ObjectsToSerialize, ObjectPtr, TRUE );
					if( !bWasNULL && ObjectPtr == NULL )
					{
						// Clear the function name as well so the delegate isn't in an invalid state.
						DelegatePtr->FunctionName = NAME_None;
					}
				}				
				else if (REFERENCE_INFO.Type == GCRT_StateLocals)
				{
					// State local var, which requires special handling because the data is not located at the
					// end of the object memory.
					const FGCSkipInfo SkipInfo	= TokenStream->ReadSkipInfo(TokenStreamIndex);
					if (CurrentObject->StateFrame == NULL || CurrentObject->StateFrame->Locals == NULL)
					{
						TokenStreamIndex = SkipInfo.SkipIndex;
						TokenReturnCount = 0;
					}
					else
					{
						StackEntry++;
						StackEntryData = CurrentObject->StateFrame->Locals;
						StackEntry->Data = StackEntryData;
						StackEntry->Stride = 0;
						StackEntry->Count = 0;
						StackEntry->LoopStartIndex = -1;
						TokenReturnCount = 0;
					}
				}
				else
				{
					appErrorf(TEXT("Unknown token"));
				}
			}
			check( StackEntry == &Stack(0) );

#if PERF_DETAILED_PER_CLASS_GC_STATS
			DWORD DeltaCycles = appCycles() - StartCycles;
			// Keep track of how many refs we encountered for the object's class.
			const FName& ClassName = CurrentObject->GetClass()->GetFName();
			// Refs to objects that reside in permanent object pool.
			DWORD ClassDisregardedObjRefs = GClassToDisregardedObjectRefsMap.FindRef( ClassName );
			GClassToDisregardedObjectRefsMap.Set( ClassName, ClassDisregardedObjRefs + GCurrentObjectDisregardedObjectRefs );
			// Refs to regular objects.
			DWORD ClassRegularObjRefs = GClassToRegularObjectRefsMap.FindRef( ClassName );
			GClassToRegularObjectRefsMap.Set( ClassName, ClassRegularObjRefs + GCurrentObjectRegularObjectRefs );
			// Track per class cycle count spent in GC.
			DWORD ClassCycles = GClassToCyclesMap.FindRef( ClassName );
			GClassToCyclesMap.Set( ClassName, ClassCycles + DeltaCycles );
			// Reset current counts.
			GCurrentObjectDisregardedObjectRefs = 0;
			GCurrentObjectRegularObjectRefs		= 0;
		}
		// Log summary stats.
		LogClassCountInfo( TEXT("references to regular objects from"), GClassToRegularObjectRefsMap, 20, 0 );
		LogClassCountInfo( TEXT("references to permanent objects from"), GClassToDisregardedObjectRefsMap, 20, 0 );
		LogClassCountInfo( TEXT("cycles for GC"), GClassToCyclesMap, 20, 0 );
#else
		}
#endif
#if CATCH_GC_CRASHES
		} 
		catch ( ... )
		{
			FString CrashString = TEXT("Crashed during garbage collection. Please undefine CATCH_GC_CRASHES in UnObjGC.cpp to track this down further.");
			if( CurrentObject->IsValid() )
			{
				CrashString += LINE_TERMINATOR;
				CrashString += FString::Printf(TEXT("CurrentObject == %s"), *CurrentObject->GetFullName());
			}
			appErrorf(TEXT("%s"),*CrashString);
		}
#endif
	}

private:
	/** Growing array of objects that require serialization */
	TArray<UObject*>	ObjectsToSerialize;
	/** Object we're currently serializing */
	UObject*			CurrentObject;
};


/**
 * Incrementally purge garbage by deleting all unreferenced objects after routing Destroy.
 *
 * Calling code needs to be EXTREMELY careful when and how to call this function as 
 * RF_Unreachable cannot change on any objects unless any pending purge has completed!
 *
 * @param	bUseTimeLimit	whether the time limit parameter should be used
 * @param	TimeLimit		soft time limit for this function call
 */
void UObject::IncrementalPurgeGarbage( UBOOL bUseTimeLimit, FLOAT TimeLimit )
{
	// Early out if there is nothing to do.
	if( !GObjPurgeIsRequired )
	{
		return;
	}

	// Set 'I'm garbage collecting' flag - might be checked inside UObject::Destroy etc.
	GIsGarbageCollecting									= TRUE; 
	// Incremental purge is now in progress.
	GObjIncrementalPurgeIsInProgress						= TRUE;

	// Keep track of start time to enforce time limit unless bForceFullPurge is TRUE;
	const DOUBLE		StartTime							= appSeconds();
	UBOOL		bTimeLimitReached							= FALSE;
	// Depending on platform appSeconds might take a noticeable amount of time if called thousands of times so we avoid
	// enforcing the time limit too often, especially as neither Destroy nor actual deletion should take significant
	// amounts of time.
	const INT	TimeLimitEnforcementGranularityForDestroy	= 10;
	const INT	TimeLimitEnforcementGranularityForDeletion	= 100;

	if( !GObjFinishDestroyHasBeenRoutedToAllObjects && !bTimeLimitReached )
	{
		// Try to dispatch all FinishDestroy messages to unreachable objects.  We'll iterate over every
		// single object and destroy any that are ready to be destroyed.  The objects that aren't yet
		// ready will be added to a list to be processed afterwards.
		INT TimePollCounter = 0;
		while( GObjCurrentPurgeObjectIndex < GObjObjects.Num() )
		{
			UObject* Object = GObjObjects(GObjCurrentPurgeObjectIndex);

			// Prefetch object further ahead.
			PREFETCH_OBJECT_ARRAY( GObjCurrentPurgeObjectIndex, 10 );

			if(	Object && Object->HasAnyFlags(RF_Unreachable) )
			{
				// Object should always have had BeginDestroy called on it and never already be destroyed
				check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

				// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
				if(Object->IsReadyForFinishDestroy())
				{
#if PERF_DETAILED_PER_CLASS_GC_STATS
					// Keep track of how many objects of a certain class we're purging.
					const FName& ClassName = Object->GetClass()->GetFName();
					INT InstanceCount = GClassToPurgeCountMap.FindRef( ClassName );
					GClassToPurgeCountMap.Set( ClassName, ++InstanceCount );
#endif
					// Send FinishDestroy message.
					Object->ConditionalFinishDestroy();
				}
				else
				{
					// The object isn't ready for FinishDestroy to be called yet.  This is common in the
					// case of a graphics resource that is waiting for the render thread "release fence"
					// to complete.  Just calling IsReadyForFinishDestroy may begin the process of releasing
					// a resource, so we don't want to block iteration while waiting on the render thread.

					// Add the object index to our list of objects to revisit after we process everything else
					GGCObjectsPendingDestruction.AddItem( GObjCurrentPurgeObjectIndex );
					GGCObjectsPendingDestructionCount++;
				}
			}

			// We've processed the object so increment our global iterator.  It's important to do this before
			// we test for the time limit so that we don't process the same object again next tick!
			GObjCurrentPurgeObjectIndex++;

			// Only check time limit every so often to avoid calling appSeconds too often.
			const UBOOL bPollTimeLimit = ((TimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
			if( bUseTimeLimit && bPollTimeLimit && ((appSeconds() - StartTime) > TimeLimit) )
			{
				bTimeLimitReached = TRUE;
				break;
			}
		}

		// Have we finished the first round of attempting to call FinishDestroy on unreachable objects?
		if( GObjCurrentPurgeObjectIndex == GObjObjects.Num() )
		{
			// We've finished iterating over all unreachable objects, but we need still need to handle
			// objects that were deferred.
			while( GGCObjectsPendingDestructionCount > 0 )
			{
				INT CurPendingObjIndex = 0;
				while( CurPendingObjIndex < GGCObjectsPendingDestructionCount )
				{
					// Grab the actual object for the current pending object list iteration
					const INT CurObjectIndex = GGCObjectsPendingDestruction( CurPendingObjIndex );
					check( GObjObjects.IsValidIndex( CurObjectIndex ) );
					UObject* Object = GObjObjects( CurObjectIndex );

					// Object should never have been added to the list if it failed this criteria
					check( Object != NULL && Object->HasAnyFlags( RF_Unreachable ) );

					// Object should always have had BeginDestroy called on it and never already be destroyed
					check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

					// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
					if( Object->IsReadyForFinishDestroy() )
					{
#if PERF_DETAILED_PER_CLASS_GC_STATS
						// Keep track of how many objects of a certain class we're purging.
						const FName& ClassName = Object->GetClass()->GetFName();
						INT InstanceCount = GClassToPurgeCountMap.FindRef( ClassName );
						GClassToPurgeCountMap.Set( ClassName, ++InstanceCount );
#endif
						// Send FinishDestroy message.
						Object->ConditionalFinishDestroy();

						// Remove the object index from our list quickly (by swapping with the last object index).
						// NOTE: This is much faster than calling TArray.RemoveSwap and avoids shrinking allocations
						{
							// Swap the last index into the current index
							GGCObjectsPendingDestruction( CurPendingObjIndex ) = GGCObjectsPendingDestruction( GGCObjectsPendingDestructionCount - 1 );

							// Decrement the object count
							GGCObjectsPendingDestructionCount--;
						}
					}
					else
					{
						// We'll revisit this object the next time around.  Move on to the next.
						CurPendingObjIndex++;
					}

					// Only check time limit every so often to avoid calling appSeconds too often.
					const UBOOL bPollTimeLimit = ((TimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
					if( bUseTimeLimit && bPollTimeLimit && ((appSeconds() - StartTime) > TimeLimit) )
					{
						bTimeLimitReached = TRUE;
						break;
					}
				}

				if( bUseTimeLimit )
				{
					// A time limit is set and we've completed a full iteration over all leftover objects, so
					// go ahead and bail out even if we have more time left or objects left to process.  It's
					// likely in this case that we're waiting for the render thread.
					break;
				}
				else if( GGCObjectsPendingDestructionCount > 0 )
				{
					// Sleep before the next pass to give the render thread some time to release fences.
					appSleep( 0 );
				}
			}

			// Have all objects been destroyed now?
			if( GGCObjectsPendingDestructionCount == 0 )
			{
				// Release memory we used for objects pending destruction, leaving some slack space
				GGCObjectsPendingDestruction.Empty( 256 );

				// Destroy has been routed to all objects so it's safe to delete objects now.
				GObjFinishDestroyHasBeenRoutedToAllObjects	= TRUE;
				GObjCurrentPurgeObjectIndex					= GObjFirstGCIndex;
			}
		}
	}		

	if( GObjFinishDestroyHasBeenRoutedToAllObjects && !bTimeLimitReached )
	{
		// Perform actual object deletion.
		// @warning: Can't use FObjectIterator here because classes may be destroyed before objects.
		while( GObjCurrentPurgeObjectIndex<GObjObjects.Num() )
		{
			// Prefetch object further ahead.
			PREFETCH_OBJECT_ARRAY( GObjCurrentPurgeObjectIndex, 10 );

			UObject* Object = GObjObjects(GObjCurrentPurgeObjectIndex);
			if(	Object && Object->HasAnyFlags(RF_Unreachable) )
			{
				const UBOOL bCurrentValue		= GIsAffectingClassDefaultObject;	
				GIsAffectingClassDefaultObject	= Object->HasAnyFlags(RF_ClassDefaultObject);
				checkf(Object->HasAllFlags(RF_FinishDestroyed|RF_BeginDestroyed), TEXT("%s was not destroyed properly"), *Object->GetDetailedInfo());
				// Actually delete object.

				//warnf( TEXT("Deleting Object: %s"), *Object->GetDetailedInfo() );

				GIsPurgingObject				= TRUE; 
				delete Object;
				GIsPurgingObject				= FALSE;
				GIsAffectingClassDefaultObject	= bCurrentValue;
				// Keep track of purged stats.
				GPurgedObjectCountSinceLastMarkPhase++;
			}

			// Advance to the next object.
			GObjCurrentPurgeObjectIndex++;

			// Only check time limit every so often to avoid calling appSeconds too often.
			if( bUseTimeLimit && (GObjCurrentPurgeObjectIndex % TimeLimitEnforcementGranularityForDeletion == 0) && ((appSeconds() - StartTime) > TimeLimit) )
			{
				bTimeLimitReached = TRUE;
				break;
			}
		}

		if( GObjCurrentPurgeObjectIndex == GObjObjects.Num() )
		{
			// Incremental purge is finished, time to reset variables.
			GObjIncrementalPurgeIsInProgress			= FALSE;
			GObjFinishDestroyHasBeenRoutedToAllObjects	= FALSE;
			GObjPurgeIsRequired							= FALSE;
			GObjCurrentPurgeObjectIndex					= GObjFirstGCIndex;

			// Log status information.
			debugf( NAME_DevGarbage, TEXT("GC purged %i objects (%i -> %i)"), GPurgedObjectCountSinceLastMarkPhase, GObjectCountDuringLastMarkPhase, GObjectCountDuringLastMarkPhase - GPurgedObjectCountSinceLastMarkPhase );

#if PERF_DETAILED_PER_CLASS_GC_STATS
			LogClassCountInfo( TEXT("objects of"), GClassToPurgeCountMap, 10, GPurgedObjectCountSinceLastMarkPhase );
#endif
		}
	}

	GIsGarbageCollecting = FALSE;

	if ( !bUseTimeLimit )
	{
		// Waits for all deferred deleted objects to be cleaned up by rendering thread. Should only be called from the game thread.
		extern void FlushDeferredDeletion();
		FlushDeferredDeletion();

		// Trim memory on full purge so we can immediately reclaim it.
		GMalloc->TrimMemory(0);
	}
}

/**
 * Returns whether an incremental purge is still pending/ in progress.
 *
 * @return	TRUE if incremental purge needs to be kicked off or is currently in progress, FALSE othwerise.
 */
UBOOL UObject::IsIncrementalPurgePending()
{
	return GObjIncrementalPurgeIsInProgress || GObjPurgeIsRequired;
}

/** Callback used by the editor to */
typedef void (*EditorPostReachabilityAnalysisCallbackType)();
EditorPostReachabilityAnalysisCallbackType EditorPostReachabilityAnalysisCallback = NULL;

/** 
 * Deletes all unreferenced objects, keeping objects that have any of the passed in KeepFlags set
 *
 * @param	KeepFlags			objects with those flags will be kept regardless of being referenced or not
 * @param	bPerformFullPurge	if TRUE, perform a full purge after the mark pass
 */
void UObject::CollectGarbage( EObjectFlags KeepFlags, UBOOL bPerformFullPurge )
{
	// We can't collect garbage while there's a load in progress. E.g. one potential issue is Import.XObject
	check( GObjBeginLoadCount==0 );

	// Route callbacks so we can ensure that we are e.g. not in the middle of loading something by flushing
	// the async loading, etc...
	for( INT CallbackIndex=0; CallbackIndex<ARRAY_COUNT(GPreGarbageCollectionCallbacks); CallbackIndex++ )
	{
		if( GPreGarbageCollectionCallbacks[CallbackIndex] )
		{
			(*GPreGarbageCollectionCallbacks[CallbackIndex])();
		}
	}

	GCallbackEvent->Send( CALLBACK_PreGarbageCollection );


	// Set 'I'm garbage collecting' flag - might be checked inside various functions.
	GIsGarbageCollecting = TRUE; 

	debugf( NAME_DevGarbage, TEXT("Collecting garbage") );

	// Make sure previous incremental purge has finished or we do a full purge pass in case we haven't kicked one
	// off yet since the last call to garbage collection.
	if( GObjIncrementalPurgeIsInProgress || GObjPurgeIsRequired )
	{
		IncrementalPurgeGarbage( FALSE );
	}
	check( !GObjIncrementalPurgeIsInProgress );
	check( !GObjPurgeIsRequired );

#if VERIFY_DISREGARD_GC_ASSUMPTIONS
	// Only verify assumptions if option is enabled. This avoids false positives in the Editor or commandlets.
	if( GObjFirstGCIndex && GShouldVerifyGCAssumptions )
	{
		// Verify that objects marked to be disregarded for GC are not referencing objects that are not part of the root set.
		for( FObjectIterator It; It; ++It )
		{
			UBOOL bShouldAssert = FALSE;
			UObject* Object = *It;
			if( Object->HasAnyFlags( RF_DisregardForGC ) )
			{
				// Serialize object with reference collector.
				TArray<UObject*> CollectedReferences;
				FArchiveObjectReferenceCollector ObjectReferenceCollector( &CollectedReferences );
				Object->Serialize( ObjectReferenceCollector );
				
				// Iterate over referenced objects, finding bad ones.
				for( INT ReferenceIndex=0; ReferenceIndex<CollectedReferences.Num(); ReferenceIndex++ )
				{
					UObject* ReferencedObject = CollectedReferences(ReferenceIndex);
					if( ReferencedObject && !ReferencedObject->HasAnyFlags( RF_DisregardForGC | RF_RootSet ) )
					{
						if( !ReferencedObject->IsA(ULinkerLoad::StaticClass()) || ReferencedObject->HasAnyFlags(RF_ClassDefaultObject) )
						{
							debugf(NAME_Warning,TEXT("RF_DisregardForGC object %s referencing %s which is not part of root set"),
								*Object->GetFullName(),
								*ReferencedObject->GetFullName());
							bShouldAssert = TRUE;
						}
					}
				}
			}
			// Assert if we encountered any objects breaking implicit assumptions.
			if( bShouldAssert )
			{
				appErrorf(TEXT("Encountered object(s) breaking RF_DisregardForGC assumption. Please check log for details."));
			}
		}
	}
#endif

	if( GIsEditor )
	{
		// Use iterative serialization based GC for Editor.
		const DOUBLE StartTime = appSeconds();
		FArchiveTagUsedNonRecursive TagUsedArNonRecursive;
		TagUsedArNonRecursive.PerformReachabilityAnalysis( KeepFlags, 0 );
		debugf( NAME_DevGarbage, TEXT("%f ms for iterative GC"), (appSeconds() - StartTime) * 1000 );
		if ( EditorPostReachabilityAnalysisCallback )
		{
			EditorPostReachabilityAnalysisCallback();
		}
	}
	else 
	{
		// Use RTGC for game.
		const DOUBLE StartTime = appSeconds();
		FArchiveRealtimeGC TagUsedRealtimeGC;		
		TagUsedRealtimeGC.PerformReachabilityAnalysis( KeepFlags );
		debugf( NAME_DevGarbage, TEXT("%f ms for realtime GC"), (appSeconds() - StartTime) * 1000 );
	}

	// Unhash all unreachable objects.
	const DOUBLE StartTime = appSeconds();
	for( INT ObjectIndex=GObjFirstGCIndex; ObjectIndex<UObject::GObjObjects.Num(); ObjectIndex++ )
	{
		// Prefetch object further ahead. @todo rtgc: this needs to be significantly improved.
		const INT PREFETCH_DISTANCE = 10;
		if( (ObjectIndex+PREFETCH_DISTANCE) < UObject::GObjObjects.Num() )
		{
			PREFETCH( UObject::GObjObjects(ObjectIndex + PREFETCH_DISTANCE) );
		}

		UObject* Object = UObject::GObjObjects(ObjectIndex);
		if( Object && Object->HasAnyFlags( RF_Unreachable ) )
		{
			// Begin the object's asynchronous destruction.
			Object->ConditionalBeginDestroy();
		}
	}
	debugf( NAME_DevGarbage, TEXT("%f ms for unhashing unreachable objects"), (appSeconds() - StartTime) * 1000 );

	// Notify script debugger to clear its stack, since all FFrames will be destroyed.
	if( GDebugger )
	{
		GDebugger->NotifyGC();
	}

	// Set flag to indicate that we are relying on a purge to be performed.
	GObjPurgeIsRequired = TRUE;
	// Reset purged count.
	GPurgedObjectCountSinceLastMarkPhase = 0;

	// Perform a full purge by not using a time limit for the incremental purge. The Editor always does a full purge.
	if( bPerformFullPurge || GIsEditor )
	{
		IncrementalPurgeGarbage( FALSE );	
	}

	// We're done collecting garbage. Note that IncrementalPurgeGarbage above might already clear it internally.
	GIsGarbageCollecting = FALSE;

	// Route callbacks to verify GC assumptions
	for( INT CallbackIndex=0; CallbackIndex<ARRAY_COUNT(GPostGarbageCollectionCallbacks); CallbackIndex++ )
	{
		if( GPostGarbageCollectionCallbacks[CallbackIndex] )
		{
			(*GPostGarbageCollectionCallbacks[CallbackIndex])();
		}
	}

	// Refresh Editor browsers after GC in case objects where removed.  Note that if the user is currently
	// playing in a PIE level, we don't want to interrupt performance by refreshing the Generic Browser window.
	if( GIsEditor && !GIsPlayInEditorWorld )
	{
		GCallbackEvent->Send( CALLBACK_RefreshEditor_AllBrowsers );
	}
}

/**
 * Returns whether an object is referenced, not counting the one
 * reference at Obj.
 *
 * @param	Obj			Object to check
 * @param	KeepFlags	Objects with these flags will be considered as being referenced
 * @return TRUE if object is referenced, FALSE otherwise
 */
UBOOL UObject::IsReferenced( UObject*& Obj, EObjectFlags KeepFlags )
{
	check(!Obj->HasAnyFlags(RF_Unreachable));

	FScopedObjectFlagMarker ObjectFlagMarker;

	// Tag objects.
	FArchiveTagUsedNonRecursive ObjectReferenceTagger;
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;
		Object->ClearFlags( RF_TagGarbage );
	}
	// Ignore this object.
	Obj->SetFlags( RF_TagGarbage );

	// We need to disable removal of references to pending kill objects if we want to be free of side effects.
	ObjectReferenceTagger.AllowEliminatingReferences( FALSE );
	// Exclude passed in object when peforming reachability analysis.
	ObjectReferenceTagger.PerformReachabilityAnalysis( KeepFlags, RF_TagGarbage );
	
	// Return whether the object was referenced and restore original state.
	const UBOOL bIsReferenced = !Obj->HasAnyFlags( RF_Unreachable );
	return bIsReferenced;
}

/**
 * Helper function to add a referenced object to the passed in array. The function ensures that the item
 * won't be added twice by checking the RF_Unreachable flag.
 *
 * @todo rtgc: temporary helper as references cannot be NULLed out this way
 *
 * @param ObjectARray	array to add object to
 * @param Object		Object to add if it isn't already part of the array (is reachable)
 */
void UObject::AddReferencedObject( TArray<UObject*>& ObjectArray, UObject* Object )
{
	HandleObjectReference( ObjectArray, Object, FALSE );
}

/**
 * Helper function to add referenced objects via serialization and an FArchiveObjectReferenceCollector 
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UObject::AddReferencedObjectsViaSerialization( TArray<UObject*>& ObjectArray )
{
	// Collect object references...
	TArray<UObject*> CollectedReferences;
	FArchiveObjectReferenceCollector ObjectReferenceCollector( &CollectedReferences );
	Serialize( ObjectReferenceCollector );
	
	// ... and add them.
	for( INT ObjectIndex=0; ObjectIndex<CollectedReferences.Num(); ObjectIndex++ )
	{
		UObject* Object = CollectedReferences(ObjectIndex);
		AddReferencedObject( ObjectArray, Object );
	}
}


/*-----------------------------------------------------------------------------
	Implementation of realtime garbage collection helper functions in 
	UProperty, UClass, ...
-----------------------------------------------------------------------------*/

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
 */
UBOOL UProperty::ContainsObjectReference() const
{
	return FALSE;
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
 */
UBOOL UObjectProperty::ContainsObjectReference() const
{
	return TRUE;
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
 */
UBOOL UArrayProperty::ContainsObjectReference() const
{
	check(Inner);
	return Inner->ContainsObjectReference();
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
 */
UBOOL UStructProperty::ContainsObjectReference() const
{
	// prevent recursion in the case of structs containing dynamic arrays of themselves
	static TArray<const UStructProperty*> EncounteredStructProps;
	if (EncounteredStructProps.ContainsItem(this))
	{
		return FALSE;
	}
	else
	{
		EncounteredStructProps.AddItem(this);

		check(Struct);
		UProperty* Property = Struct->PropertyLink;
		while( Property )
		{
			if( Property->ContainsObjectReference() )
			{
				EncounteredStructProps.RemoveSingleItemSwap(this);
				return TRUE;
			}
			Property = Property->PropertyLinkNext;
		}
		EncounteredStructProps.RemoveSingleItemSwap(this);
		return FALSE;
	}
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
 */
UBOOL UMapProperty::ContainsObjectReference() const
{
#if TMAPS_IMPLEMENTED
	check(Key);
	check(Value);
	return Key->ContainsObjectReference() || Value->ContainsObjectReference();
#else
	return FALSE;
#endif
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
 */
UBOOL UDelegateProperty::ContainsObjectReference() const
{
	return TRUE;
}

/**
 * Scope helper structure to emit tokens for fixed arrays in the case of ArrayDim (passed in count) being > 1.
 */
struct FGCReferenceFixedArrayTokenHelper
{
	/**
	 * Constructor, emitting necessary tokens for fixed arrays if count > 1 and also keeping track of count so 
	 * destructor can do the same.
	 *
	 * @param InReferenceTokenStream	Token stream to emit tokens to
	 * @param InOffset					offset into object/ struct
	 * @param InCount					array count
	 * @param InStride					array type stride (e.g. sizeof(struct) or sizeof(UObject*))
	 */
	FGCReferenceFixedArrayTokenHelper( FGCReferenceTokenStream* InReferenceTokenStream, INT InOffset, INT InCount, INT InStride )
	:	ReferenceTokenStream( InReferenceTokenStream )
	,	Count( InCount )
	{
		if( InCount > 1 )
		{
			FGCReferenceInfo FixedArrayReference( GCRT_FixedArray, InOffset );
			ReferenceTokenStream->EmitReferenceInfo( FixedArrayReference );
			ReferenceTokenStream->EmitStride( InStride );
			ReferenceTokenStream->EmitCount( InCount );
		}
	}

	/** Destructor, emitting return if ArrayDim > 1 */
	~FGCReferenceFixedArrayTokenHelper()
	{
		if( Count > 1 )
		{
			ReferenceTokenStream->EmitReturn();
		}
	}

private:
	/** Reference token stream used to emit to */
	FGCReferenceTokenStream*	ReferenceTokenStream;
	/** Size of fixed array */
	INT							Count;
};


/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UProperty::EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset )
{
}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UObjectProperty::EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset )
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper( ReferenceTokenStream, BaseOffset + Offset, ArrayDim, sizeof(UObject*) );

	EGCReferenceType ReferenceType;
	if( GetOuter()->GetFName() == NAME_Object &&
		(GetFName() == NAME_Outer || GetFName() == NAME_ObjectArchetype) )
	{
		ReferenceType = GCRT_PersistentObject;
	}
	else
	{
		ReferenceType = GCRT_Object;
	}
	FGCReferenceInfo ObjectReference( ReferenceType, BaseOffset + Offset );
	ReferenceTokenStream->EmitReferenceInfo( ObjectReference );
}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UArrayProperty::EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset )
{
	if( Inner->ContainsObjectReference() )
	{
		if( Inner->IsA(UStructProperty::StaticClass()) )
		{
			FGCReferenceInfo ReferenceInfo( GCRT_ArrayStruct, BaseOffset + Offset );
			ReferenceTokenStream->EmitReferenceInfo( ReferenceInfo );
			ReferenceTokenStream->EmitStride( Inner->ElementSize );
			const DWORD SkipIndexIndex = ReferenceTokenStream->EmitSkipIndexPlaceholder();
			Inner->EmitReferenceInfo( ReferenceTokenStream, 0 );
			const DWORD SkipIndex = ReferenceTokenStream->EmitReturn();
			ReferenceTokenStream->UpdateSkipIndexPlaceholder( SkipIndexIndex, SkipIndex );
		}
		else
		if( Inner->IsA(UObjectProperty::StaticClass()) )
		{
			FGCReferenceInfo ReferenceInfo( GCRT_ArrayObject, BaseOffset + Offset );
			ReferenceTokenStream->EmitReferenceInfo( ReferenceInfo );
		}
		else if ( Inner->IsA(UInterfaceProperty::StaticClass()) )
		{
			FGCReferenceInfo ReferenceInfo( GCRT_ArrayStruct, BaseOffset + Offset );
			ReferenceTokenStream->EmitReferenceInfo( ReferenceInfo );
			ReferenceTokenStream->EmitStride( Inner->ElementSize );
			const DWORD SkipIndexIndex = ReferenceTokenStream->EmitSkipIndexPlaceholder();

			FGCReferenceInfo InnerReferenceInfo( GCRT_Object, 0 );
			ReferenceTokenStream->EmitReferenceInfo( InnerReferenceInfo );

			const DWORD SkipIndex = ReferenceTokenStream->EmitReturn();
			ReferenceTokenStream->UpdateSkipIndexPlaceholder( SkipIndexIndex, SkipIndex );
		}
		else if (Inner->IsA(UDelegateProperty::StaticClass()))
		{
			FGCReferenceInfo ReferenceInfo( GCRT_ArrayStruct, BaseOffset + Offset );
			ReferenceTokenStream->EmitReferenceInfo( ReferenceInfo );
			ReferenceTokenStream->EmitStride( Inner->ElementSize );
			const DWORD SkipIndexIndex = ReferenceTokenStream->EmitSkipIndexPlaceholder();

			FGCReferenceInfo InnerReferenceInfo( GCRT_ScriptDelegate, 0 );
			ReferenceTokenStream->EmitReferenceInfo( InnerReferenceInfo );

			const DWORD SkipIndex = ReferenceTokenStream->EmitReturn();
			ReferenceTokenStream->UpdateSkipIndexPlaceholder( SkipIndexIndex, SkipIndex );
		}
		else
		{
			appErrorf( TEXT("Encountered unknown property containing object or name reference: %s in %s"), *Inner->GetFullName(), *GetFullName() );
		}
	}
}


/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UMapProperty::EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset )
{

}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UStructProperty::EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset )
{
	check(Struct);
	if( ContainsObjectReference() )
	{
		FGCReferenceFixedArrayTokenHelper FixedArrayHelper( ReferenceTokenStream, BaseOffset + Offset, ArrayDim, ElementSize );

		UProperty* Property = Struct->PropertyLink;
		while( Property )
		{
			Property->EmitReferenceInfo( ReferenceTokenStream, BaseOffset + Offset );
			Property = Property->PropertyLinkNext;
		}
	}
}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UInterfaceProperty::EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset )
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper( ReferenceTokenStream, BaseOffset + Offset, ArrayDim, sizeof(FScriptInterface) );

	FGCReferenceInfo ObjectReference( GCRT_Object, BaseOffset + Offset );
	ReferenceTokenStream->EmitReferenceInfo( ObjectReference );
}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UDelegateProperty::EmitReferenceInfo(FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(ReferenceTokenStream, BaseOffset + Offset, ArrayDim, sizeof(FScriptDelegate));

	FGCReferenceInfo ObjectReference(GCRT_ScriptDelegate, BaseOffset + Offset);
	ReferenceTokenStream->EmitReferenceInfo(ObjectReference);
}

/**
 * Realtime garbage collection helper function used to emit token containing information about a 
 * direct UObject reference at the passed in offset.
 *
 * @param Offset	offset into object at which object reference is stored
 */
void UClass::EmitObjectReference( INT Offset )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	FGCReferenceInfo ObjectReference( GCRT_Object, Offset );
	ReferenceTokenStream.EmitReferenceInfo( ObjectReference );
}

/**
 * Realtime garbage collection helper function used to emit token containing information about a 
 * an array of UObject references at the passed in offset. Handles both TArray and TTransArray.
 *
 * @param Offset	offset into object at which array of objects is stored
 */
void UClass::EmitObjectArrayReference( INT Offset )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	FGCReferenceInfo ObjectReference( GCRT_ArrayObject, Offset );
	ReferenceTokenStream.EmitReferenceInfo( ObjectReference );
}

/**
 * Realtime garbage collection helper function used to indicate an array of structs at the passed in 
 * offset.
 *
 * @param Offset	offset into object at which array of structs is stored
 * @param Stride	size/ stride of struct
 * @return	index into token stream at which later on index to next token after the array is stored
 *			which is used to skip over empty dynamic arrays
 */
DWORD UClass::EmitStructArrayBegin( INT Offset, INT Stride )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	FGCReferenceInfo ReferenceInfo( GCRT_ArrayStruct, Offset );
	ReferenceTokenStream.EmitReferenceInfo( ReferenceInfo );
	ReferenceTokenStream.EmitStride( Stride );
	const DWORD SkipIndexIndex = ReferenceTokenStream.EmitSkipIndexPlaceholder();
	return SkipIndexIndex;
}

/**
 * Realtime garbage collection helper function used to indicate the end of an array of structs. The
 * index following the current one will be written to the passed in SkipIndexIndex in order to be
 * able to skip tokens for empty dynamic arrays.
 *
 * @param SkipIndexIndex
 */
void UClass::EmitStructArrayEnd( DWORD SkipIndexIndex )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	const DWORD SkipIndex = ReferenceTokenStream.EmitReturn();
	ReferenceTokenStream.UpdateSkipIndexPlaceholder( SkipIndexIndex, SkipIndex );
}

/**
 * Realtime garbage collection helper function used to indicate the beginning of a fixed array.
 * All tokens issues between Begin and End will be replayed Count times.
 *
 * @param Offset	offset at which fixed array starts
 * @param Stride	Stride of array element, e.g. sizeof(struct) or sizeof(UObject*)
 * @param Count		fixed array count
 */
void UClass::EmitFixedArrayBegin( INT Offset, INT Stride, INT Count )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	FGCReferenceInfo FixedArrayReference( GCRT_FixedArray, Offset );
	ReferenceTokenStream.EmitReferenceInfo( FixedArrayReference );
	ReferenceTokenStream.EmitStride( Stride );
	ReferenceTokenStream.EmitCount( Count );
}

/**
 * Realtime garbage collection helper function used to indicated the end of a fixed array.
 */
void UClass::EmitFixedArrayEnd()
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	ReferenceTokenStream.EmitReturn();
}

/**
 * Assembles the token stream for realtime garbage collection by combining the per class only
 * token stream for each class in the class hierarchy. This is only done once and duplicate
 * work is avoided by using an object flag.
 */
void UClass::AssembleReferenceTokenStream()
{
	if( !HasAnyFlags( RF_TokenStreamAssembled ) )
	{
		if( GetSuperClass() )
		{
			// Make sure super class has valid token stream.
			GetSuperClass()->AssembleReferenceTokenStream();

			// Prepend super's stream. This automatically handles removing the EOS token.
			ReferenceTokenStream.PrependStream( GetSuperClass()->ReferenceTokenStream );
		}

		// Emit end of stream token.
		FGCReferenceInfo EndOfStream( GCRT_EndOfStream, 0 );
		ReferenceTokenStream.EmitReferenceInfo( EndOfStream );

		// Shrink reference token stream to proper size.
		ReferenceTokenStream.Shrink();

		// Avoid doing this for successive garbage collection runs.
		SetFlags( RF_TokenStreamAssembled );
	}
}


/**
 * Prepends passed in stream to existing one.
 *
 * @param Other	stream to concatenate
 */
void FGCReferenceTokenStream::PrependStream( const FGCReferenceTokenStream& Other )
{
	// Remove embedded EOS token if needed.
	TArray<DWORD> TempTokens = Other.Tokens;
	FGCReferenceInfo EndOfStream( GCRT_EndOfStream, 0 );
	if( TempTokens.Last() == EndOfStream )
	{
		TempTokens.Remove( TempTokens.Num() - 1 );
	}
	// TArray doesn't have a general '+' operator.
	TempTokens += Tokens;
	Tokens = TempTokens;
}

/**
 * Emit reference info
 *
 * @param ReferenceInfo	reference info to emit
 */
void FGCReferenceTokenStream::EmitReferenceInfo( FGCReferenceInfo ReferenceInfo )
{
	Tokens.AddItem( ReferenceInfo );
}

/**
 * Emit placeholder for aray skip index, updated in UpdateSkipIndexPlaceholder
 *
 * @return the index of the skip index, used later in UpdateSkipIndexPlaceholder
 */
DWORD FGCReferenceTokenStream::EmitSkipIndexPlaceholder()
{
	return Tokens.AddItem( E_GCSkipIndexPlaceholder );
}

/**
 * Updates skip index place holder stored and passed in skip index index with passed
 * in skip index. The skip index is used to skip over tokens in the case of an emtpy 
 * dynamic array.
 * 
 * @param SkipIndexIndex index where skip index is stored at.
 * @param SkipIndex index to store at skip index index
 */
void FGCReferenceTokenStream::UpdateSkipIndexPlaceholder( DWORD SkipIndexIndex, DWORD SkipIndex )
{
	check( SkipIndex > 0 && SkipIndex <= (DWORD)Tokens.Num() );			
	const FGCReferenceInfo& ReferenceInfo = Tokens(SkipIndex-1);
	check( ReferenceInfo.Type != GCRT_None );
	check( Tokens(SkipIndexIndex) == E_GCSkipIndexPlaceholder );
	check( SkipIndexIndex < SkipIndex );
	check( ReferenceInfo.ReturnCount >= 1 );
	FGCSkipInfo SkipInfo;
	SkipInfo.SkipIndex			= SkipIndex - SkipIndexIndex;
	// We need to subtract 1 as ReturnCount includes return from this array.
	SkipInfo.InnerReturnCount	= ReferenceInfo.ReturnCount - 1; 
	Tokens(SkipIndexIndex)		= SkipInfo;
}

/**
 * Emit count
 *
 * @param Count count to emit
 */
void FGCReferenceTokenStream::EmitCount( DWORD Count )
{
	Tokens.AddItem( Count );
}

/**
 * Emit stride
 *
 * @param Stride stride to emit
 */
void FGCReferenceTokenStream::EmitStride( DWORD Stride )
{
	Tokens.AddItem( Stride );
}

/**
 * Increase return count on last token.
 *
 * @return index of next token
 */
DWORD FGCReferenceTokenStream::EmitReturn()
{
	FGCReferenceInfo ReferenceInfo = Tokens.Last();
	check( ReferenceInfo.Type != GCRT_None );
	ReferenceInfo.ReturnCount++;
	Tokens.Last() = ReferenceInfo;
	return Tokens.Num();
}
