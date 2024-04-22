/*=============================================================================
	UnObj.cpp: Unreal object manager.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#if PS3
#include "../../PS3/Inc/FMallocPS3.h"
#endif

#include "FMallocProfiler.h"
#include "ScriptCallstackDecoder.h"

#include "UnScriptPatcher.h"
#include "ProfilingHelpers.h"
#include "CrossLevelReferences.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Object manager internal variables.
UBOOL						UObject::GObjInitialized						= 0;
UBOOL						UObject::GObjNoRegister							= 0;
INT							UObject::GObjBeginLoadCount						= 0;
UBOOL						UObject::GIsSavingPackage						= FALSE;
INT							UObject::GObjRegisterCount						= 0;
INT							UObject::GImportCount							= 0;
/** Forced exports for EndLoad optimization.											*/
INT							UObject::GForcedExportCount						= 0;
UObject*					UObject::GAutoRegister							= NULL;
UPackage*					UObject::GObjTransientPkg						= NULL;
TCHAR						UObject::GObjCachedLanguage[32]					= TEXT("");
TCHAR						UObject::GLanguage[64]							= TEXT("INT");
UObject*					UObject::GObjHash[OBJECT_HASH_BINS];
UObject*					UObject::GObjHashOuter[OBJECT_HASH_BINS];
TArray<UObject*>			UObject::GObjLoaded;
/** Objects that have been constructed during async loading phase.						*/
TArray<UObject*>			UObject::GObjConstructedDuringAsyncLoading;
TArray<UObject*>			UObject::GObjObjects;
TArray<INT>					UObject::GObjAvailable;
TArray<ULinkerLoad*>		UObject::GObjLoaders;
TMap<FName, FName>			UObject::PackageNameToFileMapping;
TArray<UObject*>			UObject::GObjRegistrants;
TIndirectArray<FAsyncPackage>		UObject::GObjAsyncPackages;
/** Whether incremental object purge is in progress										*/
UBOOL						UObject::GObjIncrementalPurgeIsInProgress		= FALSE;
/** Whether FinishDestroy has already been routed to all unreachable objects. */
UBOOL						UObject::GObjFinishDestroyHasBeenRoutedToAllObjects	= FALSE;
/** Whether we need to purge objects or not.											*/
UBOOL						UObject::GObjPurgeIsRequired					= FALSE;
/** Current object index for incremental purge.											*/
INT							UObject::GObjCurrentPurgeObjectIndex			= 0;
/** First index into objects array taken into account for GC.							*/
INT							UObject::GObjFirstGCIndex						= 0;
/** Index pointing to last object created in range disregarded for GC.					*/
INT							UObject::GObjLastNonGCIndex						= INDEX_NONE;
/** Size in bytes of pool for objects disregarded for GC.								*/
INT							UObject::GPermanentObjectPoolSize				= 0;
/** Begin of pool for objects disregarded for GC.										*/
BYTE*						UObject::GPermanentObjectPool					= NULL;
/** Current position in pool for objects disregarded for GC.							*/
BYTE*						UObject::GPermanentObjectPoolTail				= NULL;
/**
 * Array that we'll fill with indices to objects that are still pending destruction after
 * the first GC sweep (because they weren't ready to be destroyed yet.) 
 */
TArray< INT >				UObject::GGCObjectsPendingDestruction;
/** Number of objects actually still pending destruction */
INT							UObject::GGCObjectsPendingDestructionCount		= 0;

#if !CONSOLE
/** Map of path redirections used when calling SavePackage				*/
TMap<FString, FString>		UObject::GSavePackagePathRedirections;
#endif

/** Should GetResourceSize() return a true exclusive resource size, or an approximate total
    size for the asset including some forms of referenced assets that would be otherwise counted
	separately. */
UBOOL						UObject::GExclusiveResourceSizeMode				= FALSE;

DECLARE_CYCLE_STAT(TEXT("InitProperties"),STAT_InitProperties,STATGROUP_Object);
DECLARE_CYCLE_STAT(TEXT("ConstructObject"),STAT_ConstructObject,STATGROUP_Object);
DECLARE_CYCLE_STAT(TEXT("LoadConfig"),STAT_LoadConfig,STATGROUP_Object);
DECLARE_CYCLE_STAT(TEXT("LoadLocalized"),STAT_LoadLocalized,STATGROUP_Object);
DECLARE_CYCLE_STAT(TEXT("LoadObject"),STAT_LoadObject,STATGROUP_Object);
DECLARE_DWORD_COUNTER_STAT(TEXT("FindObject"),STAT_FindObject,STATGROUP_Object);
DECLARE_DWORD_COUNTER_STAT(TEXT("FindObjectFast"),STAT_FindObjectFast,STATGROUP_Object);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NameTable Entries"),STAT_NameTableEntries,STATGROUP_Object);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NameTable ANSI Entries"),STAT_NameTableAnsiEntries,STATGROUP_Object);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NameTable Unicode Entries"),STAT_NameTableUnicodeEntries,STATGROUP_Object);
DECLARE_MEMORY_STAT(TEXT("NameTable Memory Size"),STAT_NameTableMemorySize,STATGROUP_Object);
DECLARE_CYCLE_STAT(TEXT("~UObject"),STAT_DestroyObject,STATGROUP_Object);

/*-----------------------------------------------------------------------------
	UObject constructors.
-----------------------------------------------------------------------------*/
UObject::UObject()
{}
UObject::UObject( const UObject& Src )
{
	check(&Src);
	if( Src.GetClass()!=GetClass() )
		appErrorf( TEXT("Attempt to copy-construct %s from %s"), *GetFullName(), *Src.GetFullName() );
}
UObject::UObject( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags )
:	HashNext		( NULL													)
,	ObjectFlags		( InFlags | RF_Native | RF_RootSet | RF_DisregardForGC	)	
,	HashOuterNext	( NULL													)
,	StateFrame		( NULL													)
,	_Linker			( NULL													)
,	_LinkerIndex	( (PTRINT)INDEX_NONE									)
,	Index			( INDEX_NONE											)
,	Outer			( NULL													)
,	Name			( NAME_None												)
,	Class			( InClass												)
,	ObjectArchetype	( NULL													)
{
	// Make sure registration is allowed now.
	check(!GObjNoRegister);

	// Setup registration info, for processing now (if inited) or later (if starting up).
	check(sizeof(Outer       )>=sizeof(InPackageName));
	check(sizeof(_LinkerIndex)>=sizeof(GAutoRegister));
	check(sizeof(Name        )>=sizeof(DWORD        ));
	*(const TCHAR  **)&Name			= InName;
	*(const TCHAR  **)&Outer        = InPackageName;
	*(UObject      **)&_LinkerIndex = GAutoRegister;
	GAutoRegister                   = this;

	// Call native registration from terminal constructor.
	if( GetInitialized() && GetClass()==StaticClass() )
		Register();
}
UObject::UObject( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags )
:	HashNext		( NULL													)
,	ObjectFlags		( InFlags | RF_Native | RF_RootSet | RF_DisregardForGC	)
,	StateFrame		( NULL													)
,	_Linker			( NULL													)
,	_LinkerIndex	( (PTRINT)INDEX_NONE									)
,	Index			( INDEX_NONE											)
,	Outer			( NULL													)
,	Name			( NAME_None												)
,	Class			( NULL													)
,	ObjectArchetype	( NULL													)
{
	// Setup registration info, for processing now (if inited) or later (if starting up).
	check(sizeof(Outer       )>=sizeof(InPackageName));
	check(sizeof(_LinkerIndex)>=sizeof(GAutoRegister));
	check(sizeof(Name        )>=sizeof(DWORD        ));
	*(const TCHAR  **)&Name			= InName;
	*(const TCHAR  **)&Outer        = InPackageName;

	// If we are not initialized yet, auto register.
	if (!GObjInitialized)
	{
		*(UObject      **)&_LinkerIndex = GAutoRegister;
		GAutoRegister                   = this;
	}
}

/*-----------------------------------------------------------------------------
	UObject class initializer.
-----------------------------------------------------------------------------*/

void UObject::StaticConstructor()
{
}

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UObject::InitializeIntrinsicPropertyValues()
{
}

/*-----------------------------------------------------------------------------
	UObject implementation.
-----------------------------------------------------------------------------*/

/**
 * @return		TRUE if the object is selected, FALSE otherwise.
 */
UBOOL UObject::IsSelected() const
{
	return HasAnyFlags(RF_EdSelected);
}

//
// Rename this object to a unique name.
//
UBOOL UObject::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	// Check that we are not renaming a within object into an Outer of the wrong type.
	if( NewOuter && !NewOuter->IsA(GetClass()->ClassWithin) )	
	{
		appErrorf( TEXT("Cannot rename %s into Outer %s as it is not of type %s"), 
			*GetFullName(), 
			*NewOuter->GetFullName(), 
			*GetClass()->ClassWithin->GetName() );
	}

	// find an object with the same name and same class in the new outer
	if (InName)
	{
		UObject* ExistingObject = StaticFindObject(GetClass(), NewOuter, InName, TRUE);
		if (ExistingObject == this)
		{
			return TRUE;
		}
		else if (ExistingObject)
		{
			if (Flags & REN_Test)
			{
				return FALSE;
			}
			else
			{
				appErrorf(TEXT("Renaming an object (%s) on top of an existing object (%s) is not allowed"), *GetFullName(), *ExistingObject->GetFullName());
			}
		}
	}

	// if we are just testing, and there was no conflict, then return a success
	if (Flags & REN_Test)
	{
		return TRUE;
	}

	if (!(Flags & REN_ForceNoResetLoaders))
	{
		UObject::ResetLoaders( GetOuter() );
	}

	FName NewName = InName ? FName(InName) : MakeUniqueObjectName( NewOuter ? NewOuter : GetOuter(), GetClass() );

	// propagate any name changes
	GObjectPropagator->OnObjectRename(this, *NewName.ToString());

	UnhashObject();
	debugfSlow( TEXT("Renaming %s to %s"), *Name.ToString(), *NewName.ToString() );

	// Mark touched packages as dirty.
	if (Flags & REN_DoNotDirty)
	{
		// This will only mark dirty if in a transaction,
		// the object is transactional, and the object is
		// not in a PlayInEditor package.
		Modify(FALSE);
	}
	else
	{
		// This will maintain previous behavior...
		// Which was to directly call MarkPackageDirty
		Modify(TRUE);
	}

	if ( HasAnyFlags(RF_Public) )
	{
		const UBOOL bUniquePathChanged	= ((NewOuter != NULL && Outer != NewOuter) || (Name != NewName));
		const UBOOL bRootPackage		= GetClass() == UPackage::StaticClass() && GetOuter() == NULL;
		const UBOOL bRedirectionAllowed = !GIsGame;

		// We need to create a redirector if we changed the Outer or Name of an object that can be referenced from other packages
		// [i.e. has the RF_Public flag] so that references to this object are not broken.
		if ( bRootPackage == FALSE && bUniquePathChanged == TRUE && bRedirectionAllowed == TRUE )
		{
			// create a UObjectRedirector with the same name as the old object we are redirecting
			UObjectRedirector* Redir = (UObjectRedirector*)StaticConstructObject(UObjectRedirector::StaticClass(), Outer, Name, RF_Standalone | RF_Public);
			// point the redirector object to this object
			Redir->DestinationObject = this;

		}
	}

	if( NewOuter )
	{
		// objects that have been renamed cannot have refs to them serialized over the network, so clear NetIndex for this object and all objects inside it
		if (GIsGame || !(Flags & REN_KeepNetIndex))
		{
			SetNetIndex(INDEX_NONE);
			GetOutermost()->ClearAllNetObjectsInside(this);
		}

		if (!(Flags & REN_DoNotDirty))
		{
			NewOuter->MarkPackageDirty();
		}

		// Replace outer.
		Outer = NewOuter;
	}
	Name = NewName;
	HashObject();

	PostRename();

	return TRUE;
}

//
// Shutdown after a critical error.
//
void UObject::ShutdownAfterError()
{
}

//
// Make sure the object is valid.
//
UBOOL UObject::IsValid()
{
	if( !this )
	{
		debugf( NAME_Warning, TEXT("NULL object") );
		return FALSE;
	}
	else if( !GObjObjects.IsValidIndex(GetIndex()) )
	{
		debugf( NAME_Warning, TEXT("Invalid object index %i"), GetIndex() );
		debugf( NAME_Warning, TEXT("This is: %s"), *GetFullName() );
		return FALSE;
	}
	else if( GObjObjects(GetIndex())==NULL )
	{
		debugf( NAME_Warning, TEXT("Empty slot") );
		debugf( NAME_Warning, TEXT("This is: %s"), *GetFullName() );
		return FALSE;
	}
	else if( GObjObjects(GetIndex())!=this )
	{
		debugf( NAME_Warning, TEXT("Other object in slot") );
		debugf( NAME_Warning, TEXT("This is: %s"), *GetFullName() );
		debugf( NAME_Warning, TEXT("Other is: %s"), *GObjObjects(GetIndex())->GetFullName() );
		return FALSE;
	}
	else return TRUE;
}

/**
 * Verifies the hash chain of this object.
 */
void UObject::VerifyObjectHashChain()
{
	// Iterate over hash chain.
	UObject* Object = this;
	while( Object )
	{
		check(Object->IsValid());
		Object = Object->HashNext;
	}
	// Iterate over outer hash chain.
	Object = this;
	while( Object )
	{
		check(Object->IsValid());
		Object = Object->HashOuterNext;
	}
}

/**
 * Verifies validity of object hash and all its entries.
 */
void UObject::VerifyObjectHash()
{
	// Iterate over all objects in hash and iterate over their hash and outer hash chains
	for( INT iHash=0; iHash<OBJECT_HASH_BINS; iHash++ )
	{
		UObject* Object = GObjHash[iHash];
		Object->VerifyObjectHashChain();
	}
	// Iterate over all objects in outer hash and iterate over their hash and outer hash chains
	for( INT iHash=0; iHash<OBJECT_HASH_BINS; iHash++ )
	{
		UObject* Object = GObjHashOuter[iHash];
		Object->VerifyObjectHashChain();
	}
}

//
// Do any object-specific cleanup required
// immediately after loading an object, and immediately
// after any undo/redo.
//
void UObject::PostLoad()
{
	// Note that it has propagated.
	SetFlags( RF_DebugPostLoad );

	/*
	By this point, all default properties have been loaded from disk
	for this object's class and all of its parent classes.  It is now
	safe to import config and localized data for "special" objects:
	- per-object config objects (Class->ClassFlags & CLASS_PerObjectConfig)
	- subobjects/components (RF_PerObjectLocalized)
	*/
	if ( !GIsUCCMake )
	{
		if( GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig) ||
			HasAnyFlags(RF_PerObjectLocalized) )
		{
			LoadConfig();
			LoadLocalized();
		}
	}
}

//
// Edit change notification.
//
void UObject::PreEditChange(UProperty* PropertyAboutToChange)
{
	Modify();
}

/**
 * @param	PropertyThatChanged that could be NULL, then the asset should be recompiled/compressed/converted.
 * Intentionally non-virtual as it calls the FPropertyChangedEvent version
 */
void UObject::PostEditChange(void)
{
	FPropertyChangedEvent EmptyPropertyUpdateStruct(NULL);
	this->PostEditChangeProperty(EmptyPropertyUpdateStruct);
}

/**
 * Called when a property on this object has been modified externally
 *
 * @param PropertyThatChanged the property that was modified
 */
void UObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if !WITH_FACEFX
	UPackage* Package = Cast<UPackage>(GetOutermost());
	if (Package)
	{
		if ((Package->PackageFlags & PKG_ContainsFaceFXData) != 0)
		{
			appMsgf(AMT_OK, TEXT("You are editing\n%s\nwhich contains FaceFX data.\nYou will NOT be allowed to save it!"), *(Package->GetName()));
		}
	}
#endif	//#if !WITH_FACEFX

	// @todo: call this for auto-prop window updating
	GCallbackEvent->Send(CALLBACK_ObjectPropertyChanged, this);
}

/**
 * This alternate version of PreEditChange is called when properties inside structs are modified.  The property that was actually modified
 * is located at the tail of the list.  The head of the list of the UStructProperty member variable that contains the property that was modified.
 */
void UObject::PreEditChange( FEditPropertyChain& PropertyAboutToChange )
{
	if ( HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) && PropertyAboutToChange.GetActiveMemberNode() == PropertyAboutToChange.GetHead() && !GIsGame)
	{
		// this object must now be included in the undo/redo buffer
		SetFlags(RF_Transactional);

		// If we have an active memory archive and we're modifying an archetype, save the
		// the property data for all instances and child classes of this archetype before the archetype's
		// property values are changed.  Once the archetype's values have been changed, we'll refresh each
		// instances values with the new values from the archetype, then reload the custom values for each
		// object that were stored in the memory archive (PostEditChange).
		if ( GMemoryArchive != NULL )
		{
			// first, get a list of all objects which will be affected by this change; 
			TArray<UObject*> Objects;
			GetArchetypeInstances(Objects);
			SaveInstancesIntoPropagationArchive(Objects);
		}
	}

	// now forward the notification to the UProperty* version of PreEditChange
	PreEditChange(PropertyAboutToChange.GetActiveNode()->GetValue());
}

/**
 * This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
 * is located at the tail of the list.  The head of the list of the UStructProperty member variable that contains the property that was modified.
 */
void UObject::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FPropertyChangedEvent PropertyEvent(PropertyChangedEvent.PropertyChain.GetActiveNode()->GetValue(), PropertyChangedEvent.bChangesTopology, PropertyChangedEvent.ChangeType);

	PostEditChangeProperty(PropertyEvent);

	if ( HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) && PropertyChangedEvent.PropertyChain.GetActiveMemberNode() == PropertyChangedEvent.PropertyChain.GetHead() && !GIsGame )
	{
		// If we have an active memory archive and we're modifying an archetype class, reload
		// the property data for all instances and child classes of this archetype, then reimport
		// the property data for the modified properties of each object.
		if ( GMemoryArchive != NULL )
		{
			// first, get a list of all objects which will be affected by this change; 
			TArray<UObject*> Objects;
			GetArchetypeInstances(Objects);
			LoadInstancesFromPropagationArchive(Objects);
		}
	}
}



/**
 * Called by the editor to query whether a property of this object is allowed to be modified.
 * The property editor uses this to disable controls for properties that should not be changed.
 * When overriding this function you should always call the parent implementation first.
 *
 * @param	InProperty	The property to query
 *
 * @return	TRUE if the property can be modified in the editor, otherwise FALSE
 */
UBOOL UObject::CanEditChange( const UProperty* InProperty ) const
{
	const UBOOL bIsMutable = !InProperty->HasAnyPropertyFlags( CPF_EditConst );
	return bIsMutable;
}



/**
 * Determines whether changes to archetypes of this class should be immediately propagated to instances of this
 * archetype.
 *
 * @param	PropagationManager	if specified, receives a pointer to the object which manages propagation for this archetype.
 *
 * @return	TRUE if this object's archetype propagation is managed by another object (usually the case when the object
 *			is part of a prefab or something); FALSE if this object is a standalone archetype.
 *
 */
UBOOL UObject::UsesManagedArchetypePropagation( UObject** PropagationManager/*=NULL*/ ) const
{
	return IsAPrefabArchetype(PropagationManager);
}

/**
 * Builds a list of objects which have this object in their archetype chain.
 *
 * @param	Instances	receives the list of objects which have this one in their archetype chain
 */
void UObject::GetArchetypeInstances( TArray<UObject*>& Instances )
{
	Instances.Empty();

	if ( HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) )
	{
		// use an FObjectIterator because we need to evaluate CDOs as well.

		// if this object is the class default object, any object of the same class (or derived classes) could potentially be affected
		if ( !HasAnyFlags(RF_ArchetypeObject) )
		{
			for ( FObjectIterator It; It; ++It )
			{
				UObject* Obj = *It;

				// if this object is the correct type
				if ( Obj != this && Obj->IsA(GetClass()) )
				{
					Instances.AddItem(Obj);
				}
			}
		}
		else
		{
			// editing an archetype object - objects of child classes won't be affected
			for ( FObjectIterator It; It; ++It )
			{
				UObject* Obj = *It;

				// if this object is the correct type and its archetype is this object, add it to the list
				if ( Obj != this && Obj->IsA(GetClass()) && Obj->IsBasedOnArchetype(this) )
				{
					Instances.AddItem(Obj);
				}
			}
		}
	}
}

/**
 * Serializes all objects which have this object as their archetype into GMemoryArchive, then recursively calls this function
 * on each of those objects until the full list has been processed.
 * Called when a property value is about to be modified in an archetype object. 
 *
 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
 */
void UObject::SaveInstancesIntoPropagationArchive( TArray<UObject*>& AffectedObjects )
{
	check(GMemoryArchive || AffectedObjects.Num()==0);

	TArray<UObject*> Instances;

	for ( INT i = 0; i < AffectedObjects.Num(); i++ )
	{
		UObject* Obj = AffectedObjects(i);

		// in order to ensure that all objects are saved properly, only process the objects which have this object as their
		// ObjectArchetype since we are going to call Pre/PostEditChange on each object (which could potentially affect which data is serialized
		if ( Obj->GetArchetype() == this )
		{
			// add this object to the list that we're going to process
			Instances.AddItem(Obj);

			// remove this object from the input list so that when we pass the list to our instances they don't need to check those objects again.
			AffectedObjects.Remove(i--);
		}
	}

	for ( INT i = 0; i < Instances.Num(); i++ )
	{
		UObject* Obj = Instances(i);

		// this object must now be included in any undo/redo operations
		Obj->SetFlags(RF_Transactional);

		// This will call ClearComponents in the Actor case, so that we do not serialize more stuff than we need to.
		Obj->PreSerializeIntoPropagationArchive();

		// save the current property values for this object to the memory archive
		GMemoryArchive->SerializeObject(Obj);

		// notify the object that all changes are complete
		Obj->PostSerializeIntoPropagationArchive();

		// now recurse into this object, saving its instances
		//@todo ronp - should this be called *before* we call PostEditChange on Obj?
		Obj->SaveInstancesIntoPropagationArchive(AffectedObjects);
	}
}

/**
 * De-serializes all objects which have this object as their archetype from the GMemoryArchive, then recursively calls this function
 * on each of those objects until the full list has been processed.
 *
 * @param	AffectedObjects		the array of objects which have this object in their ObjectArchetype chain and will be affected by the change.
 *								Objects which have this object as their direct ObjectArchetype are removed from the list once they're processed.
 */
void UObject::LoadInstancesFromPropagationArchive( TArray<UObject*>& AffectedObjects )
{
	check(GMemoryArchive || AffectedObjects.Num()==0);
	TArray<UObject*> Instances;

	for ( INT i = 0; i < AffectedObjects.Num(); i++ )
	{
		UObject* Obj = AffectedObjects(i);

		// in order to ensure that all objects are re-initialized properly, only process the objects which have this object as their
		// ObjectArchetype
		if ( Obj->GetArchetype() == this )
		{
			// add this object to the list that we're going to process
			Instances.AddItem(Obj);

			// remove this object from the input list so that when we pass the list to our instances they don't need to check those objects again.
			AffectedObjects.Remove(i--);
		}
	}

	for ( INT i = 0; i < Instances.Num(); i++ )
	{
		UObject* Obj = Instances(i);

		// this object must now be included in any undo/redo operations
		Obj->SetFlags(RF_Transactional);

		// notify this object that it is about to be modified
		Obj->PreSerializeFromPropagationArchive();

		// refresh the object's property values
		GMemoryArchive->SerializeObject(Obj);

		// notify the object that all changes are complete
		Obj->PostSerializeFromPropagationArchive();

		// now recurse into this object, loading its instances
		//@todo ronp - should this be called *before* we call PostEditChange on Obj?
		Obj->LoadInstancesFromPropagationArchive(AffectedObjects);
	}
}

/**
 * Called just before a property in this object's archetype is to be modified, prior to serializing this object into
 * the archetype propagation archive.
 *
 * Allows objects to perform special cleanup or preparation before being serialized into an FArchetypePropagationArc
 * against its archetype. Only called for instances of archetypes, where the archetype has the RF_ArchetypeObject flag.  
 */
void UObject::PreSerializeIntoPropagationArchive()
{
	PreEditChange(NULL);
}

/**
 * Called just before a property in this object's archetype is to be modified, immediately after this object has been
 * serialized into the archetype propagation archive.
 *
 * Allows objects to perform special cleanup or preparation before being serialized into an FArchetypePropagationArc
 * against its archetype. Only called for instances of archetypes, where the archetype has the RF_ArchetypeObject flag.  
 */
void UObject::PostSerializeIntoPropagationArchive()
{
	PostEditChange();
}

/**
 * Called just after a property in this object's archetype is modified, prior to serializing this object from the archetype
 * propagation archive.
 *
 * Allows objects to perform reinitialization specific to being de-serialized from an FArchetypePropagationArc and
 * reinitialized against an archetype. Only called for instances of archetypes, where the archetype has the RF_ArchetypeObject flag.  
 */
void UObject::PreSerializeFromPropagationArchive()
{
	PreEditChange(NULL);
}

/**
 * Called just after a property in this object's archetype is modified, immediately after this object has been de-serialized
 * from the archetype propagation archive.
 *
 * Allows objects to perform reinitialization specific to being de-serialized from an FArchetypePropagationArc and
 * reinitialized against an archetype. Only called for instances of archetypes, where the archetype has the RF_ArchetypeObject flag.  
 */
void UObject::PostSerializeFromPropagationArchive()
{
	PostEditChange();
}

/** Called before applying a transaction to the object.  Default implementation simply calls PreEditChange. */
void UObject::PreEditUndo()
{
	PreEditChange(NULL);
}

/** Called after applying a transaction to the object.  Default implementation simply calls PostEditChange. */
void UObject::PostEditUndo()
{
	PostEditChange();
}

/**
 * Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
 * asynchronous cleanup process.
 */
void UObject::BeginDestroy()
{
	// Unhash object, removing it from object hash so it cannot be found from now on.
	UnhashObject();

	// Remove from linker's export table.
	SetLinker( NULL, INDEX_NONE );

	// remove net mapping
	SetNetIndex(INDEX_NONE);

	// Sanity assertion to ensure ConditionalBeginDestroy is the only code calling us.
	if( !HasAnyFlags(RF_BeginDestroyed) )
	{
		appErrorf(
			TEXT("Trying to call UObject::BeginDestroy from outside of UObject::ConditionalBeginDestroy on object %s. Please fix up the calling code."),
			*GetName()
			);
	}

	// Set debug flag to ensure BeginDestroy has been routed back to UObject::BeginDestroy.
	SetFlags( RF_DebugBeginDestroyed );
}

/**
 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
 *
 * note: because ExitProperties() is called here, Super::FinishDestroy() should always be called at the end of your child class's
 * FinishDestroy() method, rather than at the beginning.
 */
void UObject::FinishDestroy()
{
	if( !HasAnyFlags(RF_FinishDestroyed) )
	{
		appErrorf(
			TEXT("Trying to call UObject::FinishDestroy from outside of UObject::ConditionalFinishDestroy on object %s. Please fix up the calling code."),
			*GetName()
			);
	}

	check( _Linker == NULL );
	check( _LinkerIndex	== INDEX_NONE );

	SetFlags( RF_DebugFinishDestroyed );

	// Destroy properties.
	ExitProperties( (BYTE*)this, GetClass() );

	// Log message.
	if( GObjInitialized && !GIsCriticalError )
	{
		debugfSlow( NAME_DevKill, TEXT("Destroying %s"), *GetName() );
	}

	// no need to cleanup cross level stuff during shutdown (although commandlets set GIsRequestingExit, so allow them)
	if (!GIsRequestingExit || GIsUCC)
	{
#if !CONSOLE
		FCrossLevelReferenceManager* PrevManager = GCrossLevelReferenceManager;
		if (GIsEditor)
		{
			// if shutting down PIE, during the GC, the GCrossLevelRefManager may not be pointing
			// to the manager that knows about this object, so we let the manager figure out which
			// manager to use for operations on this object (with PIE, there will be one Guid but two 
			// objects, one in each manager)
			FCrossLevelReferenceManager::SwitchToManagerWithObject(this);

			// make sure this object is no longer in the global object to guid map
			GCrossLevelReferenceManager->CrossLevelObjectToGuidMap.Remove(this);
		}
#endif

		// if this object's class has any potential cross level references to other objects, then we need to see
		// if there are any pending fixups in this object. this doesn't need to be done at level streamout out time
		// it can be done in the FinishDestroy time
		if (GetClass()->ClassFlags & CLASS_HasCrossLevelRefs)
		{
			// find any entries in the delayed fixup structure pointing into this object, and remove them
			TMultiMap<FGuid, FDelayedCrossLevelRef> EntriesToRemove;
			for (TMultiMap<FGuid, FDelayedCrossLevelRef>::TIterator It(GCrossLevelReferenceManager->DelayedCrossLevelFixupMap); It; ++It)
			{
				// is the delayed fixup for this object?
				if (It.Value().Object == this)
				{
					// if so, mark this pair for removal
					EntriesToRemove.Set(It.Key(), It.Value());
				}
			}
			// we use a secondary map because removing while iterating is not safe
			for (TMultiMap<FGuid, FDelayedCrossLevelRef>::TIterator It(EntriesToRemove); It; ++It)
			{
				GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.RemovePair(It.Key(), It.Value());
			}

			TMultiMap<UObject*, FDelayedCrossLevelRef> TeardownEntriesToRemove;
			for (TMultiMap<UObject*, FDelayedCrossLevelRef>::TIterator It(GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap); It; ++It)
			{
				// is the delayed fixup for this object?
				if (It.Value().Object == this)
				{
					// if so, mark this pair for removal
					TeardownEntriesToRemove.Set(It.Key(), It.Value());
				}
			}
			// we use a secondary map because removing while iterating is not safe
			for (TMultiMap<UObject*, FDelayedCrossLevelRef>::TIterator It(TeardownEntriesToRemove); It; ++It)
			{
				GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap.RemovePair(It.Key(), It.Value());
			}

		}

#if !CONSOLE
		if (GIsEditor)
		{
			// restore the manager
			GCrossLevelReferenceManager = PrevManager;
		}
#endif
	
		// cleanup any cross level references to this object
		ConditionalCleanupCrossLevelReferences();
	}
}

/**
 * Handle this cross-level-referenced object going away (level streaming, destruction, etc)
 */
void UObject::ConditionalCleanupCrossLevelReferences()
{
	// only spend time cleaning up references to this object if it was actually referenced
	if (!GIsRequestingExit && HasAllFlags(RF_IsCrossLevelReferenced))
	{
#if !CONSOLE
		FCrossLevelReferenceManager* PrevManager = GCrossLevelReferenceManager;
		if (GIsEditor)
		{
			// if shutting down PIE, during the GC, the GCrossLevelRefManager may not be pointing
			// to the manager that knows about this object, so we let the manager figure out which
			// manager to use for operations on this object (with PIE, there will be one Guid but two 
			// objects, one in each manager)
			FCrossLevelReferenceManager::SwitchToManagerWithObject(this);
		}
#endif

		// now look for any cross elvel references pointing to me, and NULL them out
		TArray<FDelayedCrossLevelRef> PropertiesToTeardown;
		GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap.MultiFind(this, PropertiesToTeardown);

		// did we find any?
		if (PropertiesToTeardown.Num())
		{
			// find this objects guid in its package's ExportGuids map
			const FGuid* ObjectGuid = GetOutermost()->ExportGuids.FindKey(this);
			check(ObjectGuid);

			for (INT PropertyIndex = 0; PropertyIndex < PropertiesToTeardown.Num(); PropertyIndex++)
			{
				// NULL out the reference in the other object to this object
				FDelayedCrossLevelRef& Fixup = PropertiesToTeardown(PropertyIndex);
				UObject** PtrLoc = (UObject**)((BYTE*)Fixup.Object + Fixup.Offset);
				*PtrLoc = NULL;

				// let the object handle the NULLing out
				if (Fixup.Object->IsValid() && !Fixup.Object->HasAnyFlags(RF_PendingKill))
				{
					Fixup.Object->PostCrossLevelFixup();
				}

				// remember this pointer so that if the level gets streamed back in, we can restore the pointer
				GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.Add(*ObjectGuid, Fixup);
			}

			// yank out all entries for this object
			GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap.Remove(this);
		}

		// after cleanup, nothing is pointing to this object, so don't let this function do anything again
		ClearFlags(RF_IsCrossLevelReferenced);

#if !CONSOLE
		if (GIsEditor)
		{
			// restore the manager
			GCrossLevelReferenceManager = PrevManager;
		}
#endif

	}
}


/**
 * Changes the linker and linker index to the passed in one. A linker of NULL and linker index of INDEX_NONE
 * indicates that the object is without a linker.
 *
 * @param LinkerLoad	New LinkerLoad object to set
 * @param LinkerIndex	New LinkerIndex to set
 */
void UObject::SetLinker( ULinkerLoad* LinkerLoad, INT LinkerIndex )
{
	// Detach from existing linker.
	if( _Linker )
	{
		check(!HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad));
		check(_Linker->ExportMap(_LinkerIndex)._Object!=NULL);
		check(_Linker->ExportMap(_LinkerIndex)._Object==this);
		_Linker->ExportMap(_LinkerIndex)._Object = NULL;
	}
	
	// Set new linker.
	_Linker      = LinkerLoad;
	_LinkerIndex = LinkerIndex;
}

/**
 * Returns the version of the linker for this object.
 *
 * @return	the version of the engine's package file when this object
 *			was last saved, or GPackageFileVersion (current version) if
 *			this object does not have a linker, which indicates that
 *			a) this object is a native only class,
 *			b) this object's linker has been detached, in which case it is already fully loaded
 */
INT UObject::GetLinkerVersion() const
{
	ULinkerLoad* Loader = _Linker;

	// No linker.
	if( Loader == NULL )
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			// Iterate over all loaders and try to find one that has this object as the linker root.
			for( INT i=0; i<GObjLoaders.Num(); i++ )
			{
				ULinkerLoad* LinkerLoad = GetLoader(i);
				// We found a match, return its version.
				if( LinkerLoad->LinkerRoot == this )
				{
					Loader = LinkerLoad;
					break;
				}
			}
		}
	}

	if ( Loader != NULL )
	{
		// We have a linker so we can return its version.
		return Loader->Ver();

	}
	else
	{
		// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
		return GPackageFileVersion;
	}
}

/**
 * Returns the licensee version of the linker for this object.
 *
 * @return	the licensee version of the engine's package file when this object
 *			was last saved, or GPackageFileLicenseeVersion (current version) if
 *			this object does not have a linker, which indicates that
 *			a) this object is a native only class, or
 *			b) this object's linker has been detached, in which case it is already fully loaded
 */
INT UObject::GetLinkerLicenseeVersion() const
{
	ULinkerLoad* Loader = _Linker;

	// No linker.
	if( Loader == NULL )
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			// Iterate over all loaders and try to find one that has this object as the linker root.
			for( INT i=0; i<GObjLoaders.Num(); i++ )
			{
				ULinkerLoad* LinkerLoad = GetLoader(i);
				// We found a match, return its version.
				if( LinkerLoad->LinkerRoot == this )
				{
					Loader = LinkerLoad;
					break;
				}
			}
		}
	}

	if ( Loader != NULL )
	{
		// We have a linker so we can return its version.
		return Loader->LicenseeVer();

	}
	else
	{
		// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
		return GPackageFileLicenseeVersion;
	}
}

/** sets the NetIndex associated with this object for network replication */
void UObject::SetNetIndex(INT InNetIndex)
{
	if (InNetIndex != NetIndex)
	{
		UPackage* Package = GetOutermost();
		// skip if package is not meant to be replicated
		if (!(Package->PackageFlags & PKG_ServerSideOnly))
		{
			if (NetIndex != INDEX_NONE)
			{
				// remove from old
				Package->RemoveNetObject(this);
			}
			NetIndex = InNetIndex;
			if (NetIndex != INDEX_NONE)
			{
				Package->AddNetObject(this);
			}
		}
	}
}

/**
 * Returns the fully qualified pathname for this object, in the format:
 * 'Outermost.[Outer:]Name'
 *
 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObject::GetPathName( const UObject* StopOuter/*=NULL*/ ) const
{
	FString Result;
	GetPathName(StopOuter, Result);
	return Result;
}

/**
 * Internal version of GetPathName() that eliminates unnecessary copies.
 */
void UObject::GetPathName( const UObject* StopOuter, FString& ResultString ) const
{
	if( this != StopOuter && this != NULL )
	{
		if ( Outer && Outer != StopOuter )
		{
			Outer->GetPathName( StopOuter, ResultString );

			// SUBOBJECT_DELIMITER is used to indicate that this object's outer is not a UPackage
			if (Outer->GetClass() != UPackage::StaticClass()
			&&	Outer->GetOuter()->GetClass() == UPackage::StaticClass())
			{
				ResultString += SUBOBJECT_DELIMITER;
			}
			else
			{
				ResultString += TEXT(".");
			}
		}
		AppendName(ResultString);
	}
	else
	{
		ResultString += TEXT("None");
	}
}

/**
 * Returns the fully qualified pathname for this object as well as the name of the class, in the format:
 * 'ClassName Outermost.[Outer:]Name'.
 *
 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObject::GetFullName( const UObject* StopOuter/*=NULL*/ ) const
{
	FString Result;  
	if( this )
	{
		Result.Empty(128);
		GetClass()->AppendName(Result);
		Result += TEXT(" ");
		GetPathName( StopOuter, Result );
		// could possibly put a Result.Shrink() here, but this isn't used much in a shipping game
	}
	else
	{
		Result += TEXT("None");
	}
	return Result;  
}


/**
 * This will return detail info about this specific object. (e.g. AudioComponent will return the name of the cue,
 * ParticleSystemComponent will return the name of the ParticleSystem)  The idea here is that in many places
 * you have a component of interest but what you really want is some characteristic that you can use to track
 * down where it came from.  
 *
 * @note	safe to call on NULL object pointers!
 */
FString UObject::GetDetailedInfo() const
{
	FString Result;  
	if( this )
	{
		Result = GetDetailedInfoInternal();
	}
	else
	{
		Result = TEXT("None");
	}
	return Result;  
}

#if USE_GAMEPLAY_PROFILER
/** 
 *	Return an asset object associated w/ this object...
 */
UObject* UObject::GetProfilerAssetObject() const
{
	if (this)
	{
		return GetProfilerAssetObjectInternal();
	}
	return NULL;
}
#endif

/**
 * Walks up the chain of packages until it reaches the top level, which it ignores.
 *
 * @param	bStartWithOuter		whether to include this object's name in the returned string
 * @return	string containing the path name for this object, minus the outermost-package's name
 */
FString UObject::GetFullGroupName( UBOOL bStartWithOuter ) const
{
	const UObject* Obj = bStartWithOuter ? GetOuter() : this;
	return Obj ? Obj->GetPathName(GetOutermost()) : TEXT("");
}

/** 
 * Walks up the list of outers until it finds the highest one.
 *
 * @return outermost non NULL Outer.
 */
UPackage* UObject::GetOutermost() const
{
	UObject* Top;
	for( Top = (UObject*)this ; Top && Top->GetOuter() ; Top = Top->GetOuter() );
	return CastChecked<UPackage>(Top);
}

/**
 * Route BeginDestroy() and EndDestroy() on this object if this object is still valid.
 *
 * @note:	This method should never be called directly (it is called from the destructor), or the object can be left 
 *			in a bad state
 */
void UObject::ConditionalDestroy()
{
	// Check that the object hasn't been destroyed yet.
	if(!HasAnyFlags(RF_FinishDestroyed))
	{
		// Begin the asynchronous object cleanup.
		ConditionalBeginDestroy();

		// Wait for the object's asynchronous cleanup to finish.
		while(!IsReadyForFinishDestroy()) 
		{
			appSleep(0);
		}

		// Finish destroying the object.
		ConditionalFinishDestroy();
	}
}

UBOOL UObject::ConditionalBeginDestroy()
{
	if( Index!=INDEX_NONE && !HasAnyFlags(RF_BeginDestroyed) )
	{
		SetFlags(RF_BeginDestroyed);
		ClearFlags(RF_DebugBeginDestroyed);
		BeginDestroy();
		if( !HasAnyFlags(RF_DebugBeginDestroyed) )
		{
			appErrorf( TEXT("%s failed to route BeginDestroy"), *GetFullName() );
		}
		return TRUE;
	}
	else 
	{
		return FALSE;
	}
}

UBOOL UObject::ConditionalFinishDestroy()
{
	if( Index!=INDEX_NONE && !HasAnyFlags(RF_FinishDestroyed) )
	{
		SetFlags(RF_FinishDestroyed);
		ClearFlags(RF_DebugFinishDestroyed);
		FinishDestroy();
		if( !HasAnyFlags(RF_DebugFinishDestroyed) )
		{
			appErrorf( TEXT("%s failed to route FinishDestroy"), *GetFullName() );
		}
		return TRUE;
	}
	else 
	{
		return FALSE;
	}
}

//
// Register if needed.
//
void UObject::ConditionalRegister()
{
	if( GetIndex()==INDEX_NONE )
	{
#if 0
		// Verify this object is on the list to register.
		INT i;
		for( i=0; i<GObjRegistrants.Num(); i++ )
			if( GObjRegistrants(i)==this )
				break;
		check(i!=GObjRegistrants.Num());
#endif

		// Register it.
		Register();
	}
}

//
// PostLoad if needed.
//
void UObject::ConditionalPostLoad()
{
	if( HasAnyFlags(RF_NeedPostLoad) )
	{
		check(GetLinker());

		ClearFlags( RF_NeedPostLoad | RF_DebugPostLoad );

#if REQUIRES_SAMECLASS_ARCHETYPE
		//@fixme hack - hacky hacky
		FObjectInstancingGraph* CustomInstanceGraph = GetCustomPostLoadInstanceGraph();
		UObject* ObjectArchetype = (CustomInstanceGraph == NULL || CustomInstanceGraph->GetSourceRoot() == NULL)
			? this->ObjectArchetype
			: CustomInstanceGraph->GetSourceRoot();
#endif

		if ( ObjectArchetype != NULL )
		{
			//make sure our archetype executes ConditionalPostLoad first.
			ObjectArchetype->ConditionalPostLoad();
		}

#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		if ( GIsScriptPatcherActive )
		{
			ClearFlags(RF_NeedPostLoadSubobjects);
			SetFlags(RF_DebugPostLoad);
			return;
		}
#endif

#if REQUIRES_SAMECLASS_ARCHETYPE
		ConditionalPostLoadSubobjects(CustomInstanceGraph);
		delete CustomInstanceGraph;
		CustomInstanceGraph = NULL;
#else
		ConditionalPostLoadSubobjects();
#endif
		PostLoad();

		if( !HasAnyFlags(RF_DebugPostLoad) )
		{
			appErrorf( TEXT("%s failed to route PostLoad.  Please call Super::PostLoad() in your <className>::PostLoad() function. "), *GetFullName() );
		}
	}
}

/**
 * Instances components for objects being loaded from disk, if necessary.  Ensures that component references
 * between nested components are fixed up correctly.
 *
 * @param	OuterInstanceGraph	when calling this method on subobjects, specifies the instancing graph which contains all instanced
 *								subobjects and components for a subobject root.
 */
void UObject::ConditionalPostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph/*=NULL*/ )
{
	// if this class contains instanced object properties and a new object property has been added since this object was saved,
	// this object won't receive its own unique instance of the object assigned to the new property, since we don't instance object during loading
	// so go over all instanced object properties and look for cases where the value for that property still matches the default value.
	if ( HasAnyFlags(RF_NeedPostLoadSubobjects) )
	{
		if ( IsTemplate(RF_ClassDefaultObject) )
		{
			// never instance and fixup subobject/components for CDOs and their subobjects - these are instanced during script compilation and are
			// serialized using shallow comparison (serialize if they're different objects), rather than deep comparison.  Therefore subobjects and components
			// inside of CDOs will always be loaded from disk and never need to be instanced at runtime.
			ClearFlags(RF_NeedPostLoadSubobjects);
			return;
		}

		// make sure our Outer has already called ConditionalPostLoadSubobjects
		if ( Outer != NULL && Outer->HasAnyFlags(RF_NeedPostLoadSubobjects) )
		{
			if ( Outer->HasAnyFlags(RF_NeedPostLoad) )
			{
				Outer->ConditionalPostLoad();
			}
			else
			{
				Outer->ConditionalPostLoadSubobjects();
			}
			if ( !HasAnyFlags(RF_NeedPostLoadSubobjects) )
			{
				// if calling ConditionalPostLoadSubobjects on our Outer resulted in ConditionalPostLoadSubobjects on this object, stop here
				return;
			}
		}

		// clear the flag so that we don't re-enter this method
		ClearFlags(RF_NeedPostLoadSubobjects);

		FObjectInstancingGraph CurrentInstanceGraph;
	
		FObjectInstancingGraph* InstanceGraph = OuterInstanceGraph;
		if ( InstanceGraph == NULL )
		{
			CurrentInstanceGraph.SetDestinationRoot(this,NULL);
			CurrentInstanceGraph.SetLoadingObject(TRUE);

			// if we weren't passed an instance graph to use, create a new one and use that
			InstanceGraph = &CurrentInstanceGraph;
		}

		if ( !GIsUCCMake )
		{
			// this will instance any subobjects that have been added to our archetype since this object was saved
			InstanceSubobjectTemplates(InstanceGraph);
		}

		if( Class->HasAnyClassFlags(CLASS_HasComponents) )
		{
			// this will be filled with the list of component instances which were serialized from disk
			TArray<UComponent*> SerializedComponents;
			// fill the array with the component contained by this object that were actually serialized to disk through property references
			CollectComponents(SerializedComponents, FALSE);

			// now, add all of the instanced components to the instance graph that will be used for instancing any components that have been added
			// to this object's archetype since this object was last saved
			for ( INT ComponentIndex = 0; ComponentIndex < SerializedComponents.Num(); ComponentIndex++ )
			{
				UComponent* PreviouslyInstancedComponent = SerializedComponents(ComponentIndex);
				InstanceGraph->AddComponentPair(Cast<UComponent>(PreviouslyInstancedComponent->GetArchetype()), PreviouslyInstancedComponent);
			}

			InstanceComponentTemplates(InstanceGraph);
		}
	}

}

//
// UObject destructor.
//warning: Called at shutdown.
//
UObject::~UObject()
{
	SCOPE_CYCLE_COUNTER(STAT_DestroyObject);

	// Only in-place replace and garbage collection purge phase is allowed to delete UObjects.
	//@todo: enable again after linkers no longer throw from within constructor: check( GIsReplacingObject || GIsPurgingObject );

	// If not initialized, skip out.
	if( Index!=INDEX_NONE && GObjInitialized && !GIsCriticalError )
	{
		// Validate it.
		check(IsValid());

		// Destroy the object if necessary.
		ConditionalDestroy();

		GObjObjects(Index) = NULL;
		GObjAvailable.AddItem( Index );
	}

	// Free execution stack.
	if( StateFrame )
	{
		delete StateFrame;
	}
}

void UObject::operator delete( void* Object, size_t Size )
{
	check(GObjBeginLoadCount==0);

	// Nothing to do if we're deleting NULL. Worth noting that even though ResidesInPermanentPool is safe
	// to call on a NULL object, we are not doing it as it might be reported as being part of permanent
	// object pool if the feature is disabled and therefore assert in the check for exit purge when deleting
	// a NULL object.
	if( Object )
	{
		// Only free memory if it was allocated directly from allocator and not from permanent object pool.
		if( ((UObject*)Object)->ResidesInPermanentPool() == FALSE )
		{
			appFree( Object );
		}
		// We only destroy objects residing in permanent object pool during the exit purge.
		else
		{
			check(GExitPurge);
		}
	}
}

/**
 * Note that the object has been modified.  If we are currently recording into the 
 * transaction buffer (undo/redo), save a copy of this object into the buffer and 
 * marks the package as needing to be saved.
 *
 * @param	bAlwaysMarkDirty	if TRUE, marks the package dirty even if we aren't
 *								currently recording an active undo/redo transaction
 */
void UObject::Modify( UBOOL bAlwaysMarkDirty/*=TRUE*/ )
{
	// Do not consider PIE world objects or script packages, as they should never end up in the
	// transaction buffer and we don't want to mark them dirty here either.
	if ( (GetOutermost()->PackageFlags & (PKG_PlayInEditor|PKG_ContainsScript)) == 0 )
	{
		// Attempt to mark the package dirty and save a copy of the object to the transaction
		// buffer. The save will fail if there isn't a valid transactor, the object isn't
		// transactional, etc.
		UBOOL bSavedToTransactionBuffer = SaveToTransactionBuffer( TRUE );
		
		// If we failed to save to the transaction buffer, but the user requested the package
		// marked dirty anyway, do so
		if ( !bSavedToTransactionBuffer && bAlwaysMarkDirty )
		{
			MarkPackageDirty();
		}
	}
}

/**
 * Save a copy of this object into the transaction buffer if we are currently recording into
 * one (undo/redo). If bMarkDirty is TRUE, will also mark the package as needing to be saved.
 *
 * @param	bMarkDirty	If TRUE, marks the package dirty if we are currently recording into a
 *						transaction buffer
 *
 * @return	TRUE if a copy of the object was saved and the package potentially marked dirty; FALSE
 *			if we are not recording into a transaction buffer, the package is a PIE/script package,
 *			or the object is not transactional (implies the package was not marked dirty)
 */
UBOOL UObject::SaveToTransactionBuffer( UBOOL bMarkDirty )
{
	UBOOL bSavedToTransactionBuffer = FALSE;

	// Neither PIE world objects nor script packages should end up in the transaction buffer. Additionally, in order
	// to save a copy of the object, we must have a transactor and the object must be transactional.
	if ( ( ( GetOutermost()->PackageFlags & ( PKG_PlayInEditor|PKG_ContainsScript ) ) == 0 )
			&& GUndo
			&& HasAnyFlags( RF_Transactional ) )
	{
		// Mark the package dirty, if requested
		if ( bMarkDirty )
		{
			MarkPackageDirty();
		}

		// Save a copy of the object to the transactor
		GUndo->SaveObject( this );
		bSavedToTransactionBuffer = TRUE;
	}

	return bSavedToTransactionBuffer;
}

/**
 * Serializes a pushed state, calculating the offset in the same manner as the normal
 * StateFrame->StateNode code offset is calculated in UObject::Serialize().
 */
FArchive& operator<<(FArchive& Ar,FStateFrame::FPushedState& PushedState)
{
	INT Offset = Ar.IsSaving() ? PushedState.Code - &PushedState.Node->Script(0) : INDEX_NONE;
	Ar << PushedState.State << PushedState.Node << Offset;
	if (Offset != INDEX_NONE)
	{
		PushedState.Code = &PushedState.Node->Script(Offset);
	}
	return Ar;
}

/** serializes NetIndex from the passed in archive; in a separate function to share with default object serialization */
void UObject::SerializeNetIndex(FArchive& Ar)
{
	// do not serialize NetIndex when duplicating objects via serialization
	if (!(Ar.GetPortFlags() & PPF_Duplicate))
	{
		INT InNetIndex = NetIndex;
		Ar << InNetIndex;
		if (Ar.IsLoading())
		{
#if SUPPORTS_SCRIPTPATCH_CREATION
			//@script patcher
			if (GIsScriptPatcherActive || _Linker == NULL || _Linker->LinkerRoot == NULL || (_Linker->LinkerRoot->PackageFlags & PKG_Cooked))
#else
			if (_Linker == NULL || _Linker->LinkerRoot == NULL || (_Linker->LinkerRoot->PackageFlags & PKG_Cooked))
#endif
			{
				// use serialized net index for cooked packages
				SetNetIndex(InNetIndex);
			}
			// set net index from linker
			else if (_Linker != NULL && _LinkerIndex != INDEX_NONE)
			{
				SetNetIndex(_LinkerIndex);
			}
		}
	}
}

//
// UObject serializer.
//
void UObject::Serialize( FArchive& Ar )
{
	SetFlags( RF_DebugSerialize );

	// Make sure this object's class's data is loaded.
	if( Class != UClass::StaticClass() )
	{
		Ar.Preload( Class );

		// if this object's class is intrinsic, its properties may not have been linked by this point, if a non-intrinsic
		// class has a reference to an intrinsic class in its script or defaultproperties....so make osure the class is linked
		// before any data is loaded
		if ( Ar.IsLoading() )
		{
			Class->ConditionalLink();
		}

		// make sure this object's template data is loaded - the only objects
		// this should actually affect are those that don't have any defaults
		// to serialize.  for objects with defaults that actually require loading
		// the class default object should be serialized in ULinkerLoad::Preload, before
		// we've hit this code.
		if ( !HasAnyFlags(RF_ClassDefaultObject) && Class->GetDefaultsCount() > 0 )
		{
			Ar.Preload(Class->GetDefaultObject());
		}
	}

	// Special info.
	if( (!Ar.IsLoading() && !Ar.IsSaving()) )
	{
		Ar << Name;
		// We don't want to have the following potentially be clobbered by GC code.
		Ar.AllowEliminatingReferences( FALSE );
		if(!Ar.IsIgnoringOuterRef())
		{
			Ar << Outer;
		}
		Ar.AllowEliminatingReferences( TRUE );
		if ( !Ar.IsIgnoringClassRef() )
		{
			Ar << Class;
		}
		Ar << _Linker;
		if( !Ar.IsIgnoringArchetypeRef() )
		{
			Ar.AllowEliminatingReferences(FALSE);
			Ar << ObjectArchetype;
			Ar.AllowEliminatingReferences(TRUE);
		}
	}
	// Special support for supporting undo/redo of renaming and changing Archetype.
	else if( Ar.IsTransacting() )
	{
		if(!Ar.IsIgnoringOuterRef())
		{
			if(Ar.IsLoading())
			{
				UObject* LoadOuter = Outer;
				FName LoadName = Name;
				Ar << LoadName << LoadOuter;

				// If the name we loaded is different from the current one,
				// unhash the object, change the name and hash it again.
				UBOOL bDifferentName = Name != NAME_None && LoadName != Name;
				UBOOL bDifferentOuter = LoadOuter != Outer;
				if ( bDifferentName == TRUE || bDifferentOuter == TRUE )
				{
					UnhashObject();
					Name = LoadName;
					Outer = LoadOuter;
					HashObject();
				}
			}
			else
			{
				Ar << Name << Outer;
			}
		}

		if( !Ar.IsIgnoringArchetypeRef() )
		{
			Ar << ObjectArchetype;
		}
	}

	// Execution stack.
	//!!how does the stack work in conjunction with transaction tracking?
	if( !Ar.IsTransacting() )
	{
		if( HasAnyFlags(RF_HasStack) )
		{
			if( !StateFrame )
			{
				StateFrame = new FStateFrame( this );
			}
			Ar << StateFrame->Node << StateFrame->StateNode;
			if (Ar.Ver() < VER_REDUCED_PROBEMASK_REMOVED_IGNOREMASK)
			{
				QWORD ProbeMask = 0;
				Ar << ProbeMask;
				// old mask was scrambled due to probe name changes, so just reset
				StateFrame->ProbeMask = (StateFrame->StateNode != NULL) ? (StateFrame->StateNode->ProbeMask | GetClass()->ProbeMask) : GetClass()->ProbeMask;
			}
			else
			{
				Ar << StateFrame->ProbeMask;
			}
			if (Ar.Ver() < VER_REDUCED_STATEFRAME_LATENTACTION_SIZE)
			{
				INT LatentAction = 0;
				Ar << LatentAction;
				StateFrame->LatentAction = WORD(LatentAction);
			}
			else
			{
				Ar << StateFrame->LatentAction;
			}
			Ar << StateFrame->StateStack;
			if( StateFrame->Node )
			{
				Ar.Preload( StateFrame->Node );
				if( Ar.IsSaving() && StateFrame->Code )
				{
					BYTE* Start = StateFrame->Node->Script.GetTypedData();
					BYTE* End	= StateFrame->Node->Script.GetTypedData() + StateFrame->Node->Script.Num();
					check(Start != End);
					check(StateFrame->Code >= Start);
					check(StateFrame->Code < End);
				}
				INT Offset = StateFrame->Code ? StateFrame->Code - &StateFrame->Node->Script(0) : INDEX_NONE;
				Ar << Offset;
				if( Offset!=INDEX_NONE )
				{
					if( Offset<0 || Offset>=StateFrame->Node->Script.Num() )
					{
						appErrorf( TEXT("%s: Offset mismatch: %i %i"), *GetFullName(), Offset, StateFrame->Node->Script.Num() );
					}
				}
				StateFrame->Code = Offset!=INDEX_NONE ? &StateFrame->Node->Script(Offset) : NULL;
			}
			else 
			{
				StateFrame->Code = NULL;
			}
		}
		else if( StateFrame )
		{
			delete StateFrame;
			StateFrame = NULL;
		}
	}

	// if this is a subobject, then we need to copy the source defaults from the template subobject living in
	// inside the class in the .u file
	if (IsAComponent())
	{
		((UComponent*)this)->PreSerialize(Ar);
	}

	SerializeNetIndex(Ar);

	// Serialize object properties which are defined in the class.
	if( Class != UClass::StaticClass() )
	{
		SerializeScriptProperties(Ar);
	}

	// Keep track of object flags that are part of RF_UndoRedoMask when transacting.
	if( Ar.IsTransacting() )
	{
		if( Ar.IsLoading() )
		{
			EObjectFlags OriginalObjectFlags;
			Ar << OriginalObjectFlags;
			ClearFlags( RF_UndoRedoMask );
			SetFlags( OriginalObjectFlags & RF_UndoRedoMask );
		}
		else if( Ar.IsSaving() )
		{
			Ar << ObjectFlags;
		}
	}

	// Memory counting (with proper alignment to match C++)
	SIZE_T Size = Align(GetClass()->GetPropertiesSize(),GetClass()->GetMinAlignment());
	Ar.CountBytes( Size, Size );
}


/**
 * Serializes the unrealscript property data located at Data.  When saving, only saves those properties which differ from the corresponding
 * value in the specified 'DiffObject' (usually the object's archetype).
 *
 * @param	Ar				the archive to use for serialization
 * @param	DiffObject		the object to use for determining which properties need to be saved (delta serialization);
 *							if not specified, the ObjectArchetype is used
 * @param	DefaultsCount	maximum number of bytes to consider for delta serialization; any properties which have an Offset+ElementSize greater
 *							that this value will NOT use delta serialization when serializing data;
 *							if not specified, the result of DiffObject->GetClass()->GetPropertiesSize() will be used.
 */
void UObject::SerializeScriptProperties( FArchive& Ar, UObject* DiffObject/*=NULL*/, INT DiffCount/*=0*/ ) const
{
	Ar.MarkScriptSerializationStart(this);
	if( HasAnyFlags(RF_ClassDefaultObject) )
	{
		Ar.StartSerializingDefaults();
	}

	if( (Ar.IsLoading() || Ar.IsSaving()) && !Ar.WantBinaryPropertySerialization() )
	{
		if ( DiffObject == NULL )
		{
			DiffObject = GetArchetype();
		}
		GetClass()->SerializeTaggedProperties( Ar, (BYTE*)this, HasAnyFlags(RF_ClassDefaultObject) ? Class->GetSuperClass() : Class, (BYTE*)DiffObject, DiffCount );		
	}
	else if ( Ar.GetPortFlags() != 0 )
	{
		if ( DiffObject == NULL )
		{
			DiffObject = GetArchetype();
		}
		if ( DiffCount == 0 && DiffObject != NULL )
		{
			DiffCount = DiffObject->GetClass()->GetPropertiesSize();
		}

		GetClass()->SerializeBinEx( Ar, (BYTE*)this, (BYTE*)DiffObject, DiffCount );
	}
	else
	{
		GetClass()->SerializeBin( Ar, (BYTE*)this, 0 );
	}

	if (HasAnyFlags(RF_HasStack) && StateFrame->Locals != NULL)
	{
		SerializeStateLocals(Ar);
	}

	if( HasAnyFlags(RF_ClassDefaultObject) )
	{
		Ar.StopSerializingDefaults();
	}
	Ar.MarkScriptSerializationEnd(this);
}

/**
 * Serializes the unrealscript property data in state local variables.
 *
 * @param	Ar				the archive to use for serialization
 */
void UObject::SerializeStateLocals(FArchive& Ar) const
{
	if (Ar.IsObjectReferenceCollector())
	{
		for (TFieldIterator<UState> State(GetClass()); State; ++State)
		{
			if (State->StateFlags & STATE_HasLocals)
			{
				State->SerializeBin(Ar, StateFrame->Locals, 0);
			}
		}
	}
}

/**
 * Finds the component that is contained within this object that has the specified component name.
 *
 * @param	ComponentName	the component name to search for
 * @param	bRecurse		if TRUE, also searches all objects contained within this object for the component specified
 *
 * @return	a pointer to a component contained within this object that has the specified component name, or
 *			NULL if no components were found within this object with the specified name.
 */
UComponent* UObject::FindComponent( FName ComponentName, UBOOL bRecurse/*=FALSE*/ )
{
	UComponent* Result = NULL;

	if ( GetClass()->HasAnyClassFlags(CLASS_HasComponents) )
	{
		TArray<UComponent*> ComponentReferences;

		UObject* ComponentRoot = this;

		/**
		 * Currently, a component is instanced only if the Owner [passing into InstanceComponents] is the Outer for the
		 * component that is being instanced.  The following loop allows components to be instanced the first time a reference
		 * is encountered, regardless of whether the current object is the component's Outer or not.
		 */
		while ( ComponentRoot->GetOuter() && ComponentRoot->GetOuter()->GetClass() != UPackage::StaticClass() )
		{
			ComponentRoot = ComponentRoot->GetOuter();
		}

		TArchiveObjectReferenceCollector<UComponent> ComponentCollector(&ComponentReferences, 
																		ComponentRoot,				// Required Outer
																		FALSE,						// bRequireDirectOuter
																		TRUE,						// bShouldIgnoreArchetypes
																		bRecurse					// bDeepSearch
																		);

		Serialize(ComponentCollector);
		for ( INT CompIndex = 0; CompIndex < ComponentReferences.Num(); CompIndex++ )
		{
			UComponent* Component = ComponentReferences(CompIndex);
			if ( Component->TemplateName == ComponentName )
			{
				Result = Component;
				break;
			}
		}

		if ( Result == NULL && HasAnyFlags(RF_ClassDefaultObject) )
		{
			// see if this component exists in the class's component map
			UComponent** TemplateComponent = GetClass()->ComponentNameToDefaultObjectMap.Find(ComponentName);
			if ( TemplateComponent != NULL )
			{
				Result = *TemplateComponent;
			}
		}
	}

	return Result;
}

/**
 * Uses the TArchiveObjectReferenceCollector to build a list of all components referenced by this object which have this object as the outer
 *
 * @param	out_ComponentMap			the map that should be populated with the components "owned" by this object
 * @param	bIncludeNestedComponents	controls whether components which are contained by this object, but do not have this object
 *										as its direct Outer should be included
 */
void UObject::CollectComponents( TMap<FName,UComponent*>& out_ComponentMap, UBOOL bIncludeNestedComponents/*=FALSE*/ )
{
	TArray<UComponent*> ComponentArray;
	CollectComponents(ComponentArray,bIncludeNestedComponents);

	out_ComponentMap.Empty();
	for ( INT ComponentIndex = 0; ComponentIndex < ComponentArray.Num(); ComponentIndex++ )
	{
		UComponent* Comp = ComponentArray(ComponentIndex);
		out_ComponentMap.Set(Comp->GetInstanceMapName(), Comp);
	}
}

/**
 * Uses the TArchiveObjectReferenceCollector to build a list of all components referenced by this object which have this object as the outer
 *
 * @param	out_ComponentArray			the array that should be populated with the components "owned" by this object
 * @param	bIncludeNestedComponents	controls whether components which are contained by this object, but do not have this object
 *										as its direct Outer should be included
 */
void UObject::CollectComponents( TArray<UComponent*>& out_ComponentArray, UBOOL bIncludeNestedComponents/*=FALSE*/ )
{
	out_ComponentArray.Empty();
	TArchiveObjectReferenceCollector<UComponent> ComponentCollector(
		&out_ComponentArray,		//	InObjectArray
		this,						//	LimitOuter
		!bIncludeNestedComponents,	//	bRequireDirectOuter
		TRUE,						//	bIgnoreArchetypes
		TRUE,						//	bSerializeRecursively
		FALSE						//	bShouldIgnoreTransient
		);
	Serialize( ComponentCollector );
}

/**
 * Wrapper for InternalDumpComponents which allows this function to be easily called from a debugger watch window.
 */
void UObject::DumpComponents()
{
	// make sure we don't have any side-effects by ensuring that all objects' ObjectFlags stay the same
	FScopedObjectFlagMarker Marker;
	for ( FObjectIterator It; It; ++It )
	{
		It->ClearFlags(RF_TagExp|RF_TagImp);
	}

	if ( appIsDebuggerPresent() )
	{
		// if we have a debugger attached, the watch window won't be able to display the full output if we attempt to log it as a single string
		// so pass in GLog instead so that each line is sent separately;  this causes the output to have an extra line break between each log statement,
		// but at least we'll be able to see the full output in the debugger's watch window
		debugf(TEXT("Components for '%s':"), *GetFullName());
		ExportProperties( NULL, *GLog, GetClass(), (BYTE*)this, 0, NULL, NULL, this, PPF_ComponentsOnly );
		debugf(TEXT("<--- DONE!"));
	}
	else
	{
		FStringOutputDevice Output;
			Output.Logf(TEXT("Components for '%s':\r\n"), *GetFullName());
			ExportProperties( NULL, Output, GetClass(), (BYTE*)this, 2, NULL, NULL, this, PPF_ComponentsOnly );
			Output.Logf(TEXT("<--- DONE!\r\n"));
		debugf(*Output);
	}
}

/**
 * Exports the property values for the specified object as text to the output device.
 *
 * @param	Context			Context from which the set of 'inner' objects is extracted.  If NULL, an object iterator will be used.
 * @param	Out				the output device to send the exported text to
 * @param	ObjectClass		the class of the object to dump properties for
 * @param	Object			the address of the object to dump properties for
 * @param	Indent			number of spaces to prepend to each line of output
 * @param	DiffClass		the class to use for comparing property values when delta export is desired.
 * @param	Diff			the address of the object to use for determining whether a property value should be exported.  If the value in Object matches the corresponding
 *							value in Diff, it is not exported.  Specify NULL to export all properties.
 * @param	Parent			the UObject corresponding to Object
 * @param	PortFlags		flags used for modifying the output and/or behavior of the export
 */
void UObject::ExportProperties
(
	const FExportObjectInnerContext* Context,
	FOutputDevice&	Out,
	UClass*			ObjectClass,
	BYTE*			Object,
	INT				Indent,
	UClass*			DiffClass,
	BYTE*			Diff,
	UObject*		Parent,
	DWORD			PortFlags
)
{
	FString ThisName = TEXT("(none)");
	check(ObjectClass!=NULL);

	// catch any legacy code that is still passing in a UClass as a parent...this is no longer valid since
	// class default objects are now real UObjects, therefore subobjects will never have a UClass as the Outer
	// (it would be the class's default object instead)
	if ( Parent->GetClass() == UClass::StaticClass() )
	{
		Parent = ((UClass*)Parent)->GetDefaultObject(TRUE);
	}

	for( UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if( Property->Port(PortFlags) )
		{
			ThisName = Property->GetName();
			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
			UBOOL bExportObject = (Property->PropertyFlags & CPF_ExportObject) != 0 && Cast<UObjectProperty>(Property,CLASS_IsAUObjectProperty);
			const DWORD ExportFlags = PortFlags | PPF_Delimited;

			if ( ArrayProperty != NULL )
			{
				// Export dynamic array.
				UProperty* InnerProp = ArrayProperty->Inner;
				bExportObject = (Property->PropertyFlags & CPF_ExportObject) != 0 && Cast<UObjectProperty>(InnerProp,CLASS_IsAUObjectProperty);

				for( INT PropertyArrayIndex=0; PropertyArrayIndex<Property->ArrayDim; PropertyArrayIndex++ )
				{
					FScriptArray* Arr = (FScriptArray*)((BYTE*)Object + Property->Offset + PropertyArrayIndex*Property->ElementSize);
					FScriptArray*	DiffArr = NULL;
					if( DiffClass && Property->Offset < DiffClass->GetPropertiesSize() )
					{
						DiffArr = (FScriptArray*)(Diff + Property->Offset + PropertyArrayIndex*Property->ElementSize);
					}

					UBOOL bAnyElementDiffered = FALSE;
					for( INT DynamicArrayIndex=0;DynamicArrayIndex<Arr->Num();DynamicArrayIndex++ )
					{
						FString	Value;

						// compare each element's value manually so that elements which match the NULL value for the array's inner property type
						// but aren't in the diff array are still exported
						BYTE* SourceData = (BYTE*)Arr->GetData() + DynamicArrayIndex * InnerProp->ElementSize;
						BYTE* DiffData = DiffArr && DynamicArrayIndex < DiffArr->Num()
							? (BYTE*)DiffArr->GetData() + DynamicArrayIndex * InnerProp->ElementSize
							: NULL;
						if (DiffData == NULL && (InnerProp->GetClass()->ClassCastFlags & CASTCLASS_UStructProperty))
						{
							DiffData = ((UStructProperty*)InnerProp)->Struct->GetDefaults();
						}

						UBOOL bExportItem = DiffData == NULL || (DiffData != SourceData && !InnerProp->Identical(SourceData, DiffData, ExportFlags));
						if ( bExportItem )
						{
							bAnyElementDiffered = TRUE;
							InnerProp->ExportTextItem(Value, SourceData, DiffData, Parent, ExportFlags);
							if( bExportObject )
							{
								UObject* Obj = ((UObject**)Arr->GetData())[DynamicArrayIndex];
								if( Obj && !Obj->HasAnyFlags(RF_TagImp) )
								{
									// only export the BEGIN OBJECT block for a component if Parent is the component's Outer....when importing subobject definitions,
									// (i.e. BEGIN OBJECT), whichever BEGIN OBJECT block a component's BEGIN OBJECT block is located within is the object that will be
									// used as the Outer to create the component

									// Is this an array of components?
									if ( InnerProp->GetClass() == UComponentProperty::StaticClass() )
									{
										if ( Obj->GetOuter() == Parent )
										{
											// Don't export more than once.
											Obj->SetFlags( RF_TagImp );
											UExporter::ExportToOutputDevice( Context, Obj, NULL, Out, TEXT("T3D"), Indent, PortFlags );
										}
										else
										{
											// set the RF_TagExp flag so that the calling code knows we wanted to export this object
											Obj->SetFlags(RF_TagExp);
										}
									}
									else
									{
										// Don't export more than once.
										Obj->SetFlags( RF_TagImp );
										UExporter::ExportToOutputDevice( Context, Obj, NULL, Out, TEXT("T3D"), Indent, PortFlags );
									}
								}
							}

							Out.Logf( TEXT("%s%s(%i)=%s\r\n"), appSpc(Indent), *Property->GetName(), DynamicArrayIndex, *Value );
						}
						// if some other element has already been determined to differ from the defaults, then export this item with no data so that
						// the different array's size is maintained on import (this item will get the default values for that index, if any)
						// however, if no elements of the array have changed, we still don't want to export anything
						// so that the array size will also be taken from the defaults, which won't be the case if any element is exported
						else if (bAnyElementDiffered)
						{
							Out.Logf( TEXT("%s%s(%i)=()\r\n"), appSpc(Indent), *Property->GetName(), DynamicArrayIndex );
						}
					}
				}
			}
			else
			{
				for( INT PropertyArrayIndex=0; PropertyArrayIndex<Property->ArrayDim; PropertyArrayIndex++ )
				{
					FString	Value;
					// Export single element.

					BYTE* DiffData = (DiffClass && Property->Offset < DiffClass->GetPropertiesSize()) ? Diff : NULL;
					if( Property->ExportText( PropertyArrayIndex, Value, Object, DiffData, Parent, ExportFlags ) )
					{
						if ( bExportObject )
						{
							UObject* Obj = *(UObject **)((BYTE*)Object + Property->Offset + PropertyArrayIndex*Property->ElementSize);
							if( Obj && !Obj->HasAnyFlags(RF_TagImp) )
							{
								// only export the BEGIN OBJECT block for a component if Parent is the component's Outer....when importing subobject definitions,
								// (i.e. BEGIN OBJECT), whichever BEGIN OBJECT block a component's BEGIN OBJECT block is located within is the object that will be
								// used as the Outer to create the component
								if ( Property->GetClass() == UComponentProperty::StaticClass() )
								{
									if ( Obj->GetOuter() == Parent )
									{
										// Don't export more than once.
										Obj->SetFlags( RF_TagImp );
										UExporter::ExportToOutputDevice( Context, Obj, NULL, Out, TEXT("T3D"), Indent, PortFlags );
									}
									else
									{
										// set the RF_TagExp flag so that the calling code knows we wanted to export this object
										Obj->SetFlags(RF_TagExp);
									}
								}
								else
								{
									// Don't export more than once.
									Obj->SetFlags( RF_TagImp );
									UExporter::ExportToOutputDevice( Context, Obj, NULL, Out, TEXT("T3D"), Indent, PortFlags );
								}
							}
						}

						if( Property->ArrayDim == 1 )
						{
							Out.Logf( TEXT("%s%s=%s\r\n"), appSpc(Indent), *Property->GetName(), *Value );
						}
						else
						{
							Out.Logf( TEXT("%s%s(%i)=%s\r\n"), appSpc(Indent), *Property->GetName(), PropertyArrayIndex, *Value );
						}
					}
				}
			}
		}
	}

	// Allows to import/export C++ properties in case the automatic unreal script mesh wouldn't work.
	Parent->ExportCustomProperties(Out, Indent);
}

//
// Initialize script execution.
//
void UObject::InitExecution()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		check(GetClass()!=NULL);

		if( StateFrame )
		{
			delete StateFrame;
		}
		StateFrame = new FStateFrame( this );
		SetFlags( RF_HasStack );
	}
}

//
// Command line.
//
UBOOL UObject::ScriptConsoleExec( const TCHAR* Str, FOutputDevice& Ar, UObject* Executor )
{
	// Find UnrealScript exec function.
	FString MsgStr;
	FName Message = NAME_None;
	UFunction* Function = NULL;
	if
	(	!ParseToken(Str,MsgStr,TRUE)
	||	(Message=FName(*MsgStr,FNAME_Find))==NAME_None
	||	(Function=FindFunction(Message))==NULL
	||	(Function->FunctionFlags & FUNC_Exec) == 0 )
	{
		return FALSE;
	}

	UProperty* LastParameter=NULL;

	// find the last parameter
	for ( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm)) == CPF_Parm; ++It )
	{
		LastParameter = *It;
	}

	UStrProperty* LastStringParameter = Cast<UStrProperty>(LastParameter);


	// Parse all function parameters.
	BYTE* Parms = (BYTE*)appAlloca(Function->ParmsSize);
	appMemzero( Parms, Function->ParmsSize );

	// if this exec function has optional parameters, we'll need to process the default value opcodes
	FFrame* ExecFunctionStack=NULL;
	if ( Function->HasAnyFunctionFlags(FUNC_HasOptionalParms) )
	{
		ExecFunctionStack = new FFrame( this, Function, 0, Parms, NULL );
	}

	UBOOL Failed = 0;
	INT NumParamsEvaluated = 0;
	for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It, NumParamsEvaluated++ )
	{
		BYTE* CurrentPropAddress = Parms + It->Offset;
		
		if ( It->HasAnyPropertyFlags(CPF_OptionalParm) )
		{
			// set the runtime flag so we can evaluate defaults for any optionals
			GRuntimeUCFlags |= RUC_SkippedOptionalParm;

			checkSlow(ExecFunctionStack);
			if ( Function->HasAnyFunctionFlags(FUNC_HasDefaults) )
			{
				UStructProperty* StructProp = Cast<UStructProperty>(*It, CLASS_IsAUStructProperty);
				if ( StructProp != NULL )
				{
					StructProp->InitializeValue(CurrentPropAddress);
				}
			}
			ExecFunctionStack->Step(ExecFunctionStack->Object, CurrentPropAddress);
		}

		if( NumParamsEvaluated == 0 && Executor )
		{
			UObjectProperty* Op = Cast<UObjectProperty>(*It,CLASS_IsAUObjectProperty);
			if( Op && Executor->IsA(Op->PropertyClass) )
			{
				// First parameter is implicit reference to object executing the command.
				*(UObject**)(Parms + It->Offset) = Executor;
				continue;
			}
		}

		ParseNext( &Str );

		DWORD ExportFlags = PPF_Localized;

		// if this is the last parameter of the exec function and it's a string (and not delimited), make sure that it accepts the remainder of the passed in value
		if ( LastStringParameter != *It || *Str == '"')
		{
			ExportFlags |= PPF_Delimited;
		}
		const TCHAR* PreviousStr = Str;
		const TCHAR* Result = It->ImportText( Str, Parms+It->Offset, ExportFlags, NULL );
		UBOOL bFailedImport = (Result == NULL || Result == PreviousStr);
		if( bFailedImport )
		{
			if( !It->HasAnyPropertyFlags(CPF_OptionalParm) )
			{
				Ar.Logf( LocalizeSecure(LocalizeError(TEXT("BadProperty"),TEXT("Core")), *Message.ToString(), *It->GetName()) );
				Failed = TRUE;
			}

			// still need to process the remainder of the optional default values
			if ( ExecFunctionStack != NULL )
			{
				for ( ++It; It; ++It )
				{
					if ( !It->HasAnyPropertyFlags(CPF_Parm) || It->HasAnyPropertyFlags(CPF_ReturnParm) )
					{
						break;
					}

					if ( It->HasAnyPropertyFlags(CPF_OptionalParm) )
					{
						// set the runtime flag so we can evaluate defaults for any optionals
						GRuntimeUCFlags |= RUC_SkippedOptionalParm;

						CurrentPropAddress = Parms + It->Offset;

						if ( Function->HasAnyFunctionFlags(FUNC_HasDefaults) )
						{
							UStructProperty* StructProp = Cast<UStructProperty>(*It, CLASS_IsAUStructProperty);
							if ( StructProp != NULL )
							{
								StructProp->InitializeValue(CurrentPropAddress);
							}
						}
						ExecFunctionStack->Step(ExecFunctionStack->Object, CurrentPropAddress);
					}
				}
			}
			break;
		}

		// move to the next parameter
		Str = Result;
	}

	// reset the runtime flag
	GRuntimeUCFlags &= ~RUC_SkippedOptionalParm;

	if( !Failed )
	{
		ProcessEvent( Function, Parms );
	}

	//!!destructframe see also UObject::ProcessEvent
	for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
	{
		It->DestroyValue( Parms + It->Offset );
	}

	// Success.
	delete ExecFunctionStack;
	return TRUE;
}

//
// Find an UnrealScript field.
//warning: Must be safe with class default metaobjects.
//
UField* UObject::FindObjectField( FName InName, UBOOL Global )
{
	// Search current state scope.
	if( StateFrame && StateFrame->StateNode && !Global )
	{
		for( TFieldIterator<UField> It(StateFrame->StateNode); It; ++It )
		{
			if( It->GetFName()==InName )
			{
				return *It;
			}
		}
	}

	// Search the global scope.
	for( TFieldIterator<UField> It(GetClass()); It; ++It )
	{
		if( It->GetFName()==InName )
		{
			return *It;
		}
	}

	// Not found.
	return NULL;
}
UFunction* UObject::FindFunction( FName InName, UBOOL Global ) const
{
	UFunction *Function = NULL;
	if( StateFrame != NULL && StateFrame->StateNode != NULL && !Global )
	{
		// search current/parent states
		UState *SearchState = StateFrame->StateNode;
		while( SearchState != NULL && Function == NULL)
		{
			Function	= SearchState->FuncMap.FindRef(InName);
			SearchState = SearchState->GetSuperState();
		}
	}
	if( Function == NULL )
	{
		// and search the global state
		UClass *SearchClass = GetClass();
		while( SearchClass != NULL && Function == NULL )
		{
			Function	= SearchClass->FuncMap.FindRef(InName);
			SearchClass = SearchClass->GetSuperClass();
		}
	}
    return Function;
}
UFunction* UObject::FindFunctionChecked( FName InName, UBOOL Global ) const
{
    UFunction* Result = FindFunction(InName,Global);
	if( !Result )
	{
		appErrorf( TEXT("Failed to find function %s in %s"), *InName.ToString(), *GetFullName() );
	}
	return Result;
}
UState* UObject::FindState( FName InName )
{
	UState* State = NULL;
	for (TFieldIterator<UState> It(GetClass()); It && !State; ++It)
	{
		if (It->GetFName() == InName)
		{
			State = *It;
		}
	}
    return State;
}

/**
 * Determines whether the specified object should load values using PerObjectConfig rules
 */
static UBOOL UsesPerObjectConfig( UObject* SourceObject )
{
	checkSlow(SourceObject);
	return (SourceObject->GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig) && !SourceObject->HasAnyFlags(RF_ClassDefaultObject));
}

/**
 * Returns the file to load ini values from for the specified object, taking into account PerObjectConfig-ness
 */
static FString GetConfigFilename( UObject* SourceObject )
{
	checkSlow(SourceObject);

	// if this is a PerObjectConfig object that is not contained by the transient package,
	// unless the PerObjectConfig class specified a different ini file.
	return (UsesPerObjectConfig(SourceObject) && SourceObject->GetOutermost() != UObject::GetTransientPackage())
		? (appGameConfigDir() + FString( GGameName ) + *SourceObject->GetOutermost()->GetName() + TEXT(".ini"))
		: SourceObject->GetClass()->GetConfigName();

}

/*-----------------------------------------------------------------------------
	UObject configuration.
-----------------------------------------------------------------------------*/
/**
 * Wrapper method for LoadConfig that is used when reloading the config data for objects at runtime which have already loaded their config data at least once.
 * Allows the objects the receive a callback that it's configuration data has been reloaded.
 */
void UObject::ReloadConfig( UClass* ConfigClass/*=NULL*/, const TCHAR* InFilename/*=NULL*/, DWORD PropagationFlags/*=LCPF_None*/, UProperty* PropertyToLoad/*=NULL*/ )
{
	LoadConfig(ConfigClass, InFilename, PropagationFlags|UE3::LCPF_ReloadingConfigData|UE3::LCPF_ReadParentSections, PropertyToLoad);
}

/**
 * Imports property values from an .ini file.
 *
 * @param	Class				the class to use for determining which section of the ini to retrieve text values from
 * @param	InFilename			indicates the filename to load values from; if not specified, uses ConfigClass's ClassConfigName
 * @param	PropagationFlags	indicates how this call to LoadConfig should be propagated; expects a bitmask of UE3::ELoadConfigPropagationFlags values.
 * @param	PropertyToLoad		if specified, only the ini value for the specified property will be imported.
 */
void UObject::LoadConfig( UClass* ConfigClass/*=NULL*/, const TCHAR* InFilename/*=NULL*/, DWORD PropagationFlags/*=LCPF_None*/, UProperty* PropertyToLoad/*=NULL*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_LoadConfig);

	// OriginalClass is the class that LoadConfig() was originally called on
	static UClass* OriginalClass = NULL;

	if( !ConfigClass )
	{
		// if no class was specified in the call, this is the OriginalClass
		ConfigClass = GetClass();
		OriginalClass = ConfigClass;
	}

	if( !ConfigClass->HasAnyClassFlags(CLASS_Config) )
	{
		return;
	}

#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive )
	{
		return;
	}
#endif

	UClass* ParentClass = ConfigClass->GetSuperClass();
	if ( ParentClass != NULL )
	{
		if ( ParentClass->HasAnyClassFlags(CLASS_Config) )
		{
			if ( (PropagationFlags&UE3::LCPF_ReadParentSections) != 0 )
			{
				// call LoadConfig on the parent class
				LoadConfig( ParentClass, NULL, PropagationFlags, PropertyToLoad );

				// if we are also notifying child classes or instances, stop here as this object's properties will be imported as a result of notifying the others
				if ( (PropagationFlags & (UE3::LCPF_PropagateToChildDefaultObjects|UE3::LCPF_PropagateToInstances)) != 0 )
				{
					return;
				}
			}
			else if ( (PropagationFlags&UE3::LCPF_PropagateToChildDefaultObjects) != 0 )
			{
				// not propagating the call upwards, but we are propagating the call to all child classes
				for (TObjectIterator<UClass> It; It; ++It)
				{
					if (It->IsChildOf(ConfigClass))
					{
						// mask out the PropgateToParent and PropagateToChildren values
						It->GetDefaultObject()->LoadConfig(*It, NULL, (PropagationFlags&(UE3::LCPF_PersistentFlags|UE3::LCPF_PropagateToInstances)), PropertyToLoad);
					}
				}

				// LoadConfig() was called on this object during iteration, so stop here 
				return;
			}
			else if ( (PropagationFlags&UE3::LCPF_PropagateToInstances) != 0 )
			{
				// call LoadConfig() on all instances of this class (except the CDO)
				// Do not propagate this call to parents, and do not propagate to children or instances (would be redundant) 
				for (TObjectIterator<UObject> It; It; ++It)
				{
					if (It->IsA(ConfigClass))
					{
						if ( !GIsEditor )
						{
							// make sure to pass in the class so that OriginalClass isn't reset
							It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE3::LCPF_PersistentFlags), PropertyToLoad);
						}
						else
						{
							It->PreEditChange(NULL);

							// make sure to pass in the class so that OriginalClass isn't reset
							It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE3::LCPF_PersistentFlags), PropertyToLoad);

							It->PostEditChange();
						}
					}
				}
			}
		}
		else if ( (PropagationFlags&UE3::LCPF_PropagateToChildDefaultObjects) != 0 )
		{
			// we're at the base-most config class
			for ( TObjectIterator<UClass> It; It; ++It )
			{
				if ( It->IsChildOf(ConfigClass) )
				{
					if ( !GIsEditor )
					{
						// make sure to pass in the class so that OriginalClass isn't reset
						It->GetDefaultObject()->LoadConfig( *It, NULL, (PropagationFlags&(UE3::LCPF_PersistentFlags|UE3::LCPF_PropagateToInstances)), PropertyToLoad );
					}
					else
					{
						It->PreEditChange(NULL);

						// make sure to pass in the class so that OriginalClass isn't reset
						It->GetDefaultObject()->LoadConfig( *It, NULL, (PropagationFlags&(UE3::LCPF_PersistentFlags|UE3::LCPF_PropagateToInstances)), PropertyToLoad );

						It->PostEditChange();
					}
				}
			}

			return;
		}
		else if ( (PropagationFlags&UE3::LCPF_PropagateToInstances) != 0 )
		{
			for ( TObjectIterator<UObject> It; It; ++It )
			{
				if ( It->GetClass() == ConfigClass )
				{
					if ( !GIsEditor )
					{
						// make sure to pass in the class so that OriginalClass isn't reset
						It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE3::LCPF_PersistentFlags), PropertyToLoad);
					}
					else
					{
						It->PreEditChange(NULL);

						// make sure to pass in the class so that OriginalClass isn't reset
						It->LoadConfig(It->GetClass(), NULL, (PropagationFlags&UE3::LCPF_PersistentFlags), PropertyToLoad);
						It->PostEditChange();
					}
				}
			}
		}
	}

	const FString Filename
	// if a filename was specified, always load from that file
	=	InFilename
		? InFilename
		: GetConfigFilename(this);

	const UBOOL bPerObject = UsesPerObjectConfig(this);

	FString ClassSection;
	if ( bPerObject == TRUE )
	{
		FString PathNameString;
		if ( GetOutermost() == GetTransientPackage() )
		{
			PathNameString = GetName();
		}
		else
		{
			GetPathName(GetOutermost(), PathNameString);
		}
		ClassSection = PathNameString + TEXT(" ") + GetClass()->GetName();
	}

	// If any of my properties are class variables, then LoadConfig() would also be called for each one of those classes.
	// Since OrigClass is a static variable, if the value of a class variable is a class different from the current class, 
	// we'll lose our nice reference to the original class - and cause any variables which were declared after this class variable to fail 
	// the 'if (OriginalClass != Class)' check....better store it in a temporary place while we do the actual loading of our properties 
	UClass* MyOrigClass = OriginalClass;

	if ( PropertyToLoad == NULL )
	{
		debugfSuppressed(NAME_DevSave, TEXT("(%s) '%s' loading configuration from %s"), *ConfigClass->GetName(), *GetName(), *Filename);
	}
	else
	{
		debugfSuppressed(NAME_DevSave, TEXT("(%s) '%s' loading configuration for property %s from %s"), *ConfigClass->GetName(), *GetName(), *PropertyToLoad->GetName(), *Filename);
	}

	for ( UProperty* Property = ConfigClass->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if ( !Property->HasAnyPropertyFlags(CPF_Config) )
		{
			continue;
		}

		// if we're only supposed to load the value for a specific property, skip all others
		if ( PropertyToLoad != NULL && PropertyToLoad != Property )
		{
			continue;
		}

		// Don't load config properties that are marked editoronly if not in the editor
		if ((Property->PropertyFlags & CPF_EditorOnly) && !GIsEditor)
		{
			continue;
		}

#if CONSOLE
		// Don't load config properties that are marked notforconsole
		if (Property->PropertyFlags & CPF_NotForConsole)
		{
			continue;
		}
#endif

		const UBOOL bGlobalConfig = (Property->PropertyFlags&CPF_GlobalConfig) != 0;
		UClass* OwnerClass = Property->GetOwnerClass();

		UClass* BaseClass = bGlobalConfig ? OwnerClass : ConfigClass;
		if ( !bPerObject )
		{
			ClassSection = BaseClass->GetPathName();
		}

		// globalconfig properties should always use the owning class's config file
		// specifying a value for InFilename will override this behavior (as it does with normal properties)
		const FString& PropFileName = (bGlobalConfig && InFilename == NULL) ? OwnerClass->GetConfigName() : Filename;

		FString Key = Property->GetName();
		debugfSuppressed(NAME_DevSave, TEXT("   Loading value for %s from [%s]"), *Key, *ClassSection);
		UArrayProperty* Array = Cast<UArrayProperty>( Property );
		if( Array == NULL )
		{
			for( INT i=0; i<Property->ArrayDim; i++ )
			{
				if( Property->ArrayDim!=1 )
				{
					Key = FString::Printf(TEXT("%s[%i]"), *Property->GetName(), i);
				}

				FString Value;
				if( GConfig->GetString( *ClassSection, *Key, Value, *PropFileName ) )
				{
					if (Property->ImportText(*Value, (BYTE*)this + Property->Offset + i*Property->ElementSize, PPF_ConfigOnly, this) == NULL)
					{
						// this should be an error as the properties from the .ini / .int file are not correctly being read in and probably are affecting things in subtle ways
						warnf(NAME_Error, TEXT("LoadConfig (%s): import failed for %s in: %s"), *GetPathName(), *Property->GetName(), *Value);
					}
				}
			}
		}
		else
		{
			FConfigSection* Sec = GConfig->GetSectionPrivate( *ClassSection, FALSE, TRUE, *PropFileName );
			if( Sec )
			{
				TArray<FString> List;
				Sec->MultiFind(FName(*Key,FNAME_Find),List);

				const INT Size = Array->Inner->ElementSize;
				// Only override default properties if there is something to override them with.
				if ( List.Num() > 0 )
				{
					FScriptArray* Ptr  = (FScriptArray*)((BYTE*)this + Property->Offset);
					Array->DestroyValue( Ptr );
					Ptr->AddZeroed( List.Num(), Size );

					UStructProperty* InnerStructProp = Cast<UStructProperty>(Array->Inner,CLASS_IsAUStructProperty);
					if ( InnerStructProp != NULL && InnerStructProp->Struct
					&&	InnerStructProp->Struct->GetDefaultsCount()
					&&	InnerStructProp->HasValue(InnerStructProp->Struct->GetDefaults()) )
					{
						for ( INT ArrayIndex = List.Num() - 1; ArrayIndex >= 0; ArrayIndex-- )
						{
							BYTE* ValueAddress = (BYTE*)Ptr->GetData() + ArrayIndex * Size;
							InnerStructProp->InitializeValue(ValueAddress);
						}
					}
					for( INT i=List.Num()-1,c=0; i>=0; i--,c++ )
					{
						Array->Inner->ImportText( *List(i), (BYTE*)Ptr->GetData() + c*Size, PPF_ConfigOnly, this );
					}
				}
				else
				{
					// If nothing was found, try searching for indexed keys
					FScriptArray*		Ptr  = (FScriptArray*)((BYTE*)this + Property->Offset);

					UStructProperty* InnerStructProp = Cast<UStructProperty>(Array->Inner,CLASS_IsAUStructProperty);
					if ( InnerStructProp != NULL
					&&	(InnerStructProp->Struct == NULL
					||	!InnerStructProp->Struct->GetDefaultsCount()
					||	!InnerStructProp->HasValue(InnerStructProp->Struct->GetDefaults())) )
					{
						InnerStructProp = NULL;
					}

					INT Index = 0;
					FString* ElementValue = NULL;
					do
					{
						// Add array index number to end of key
						FString IndexedKey = FString::Printf(TEXT("%s[%i]"), *Key, Index);

						// Try to find value of key
						ElementValue  = Sec->Find( FName(*IndexedKey,FNAME_Find) );

						// If found, import the element
						if ( ElementValue != NULL )
						{
							// expand the array if necessary so that Index is a valid element
							if (Index >= Ptr->Num())
							{
								Ptr->AddZeroed(Index - Ptr->Num() + 1, Size);
							}
							BYTE* ValueAddress = (BYTE*)Ptr->GetData() + Index * Size;

							if ( InnerStructProp != NULL )
							{
								// initialize struct defaults if applicable
								InnerStructProp->InitializeValue(ValueAddress);
							}

							Array->Inner->ImportText(**ElementValue, ValueAddress, PPF_ConfigOnly, this);
						}

						Index++;
					} while( ElementValue || Index < Ptr->Num() );
				}
			}
		}
	}

	// if we are reloading config data after the initial class load, fire the callback now
	if ( (PropagationFlags&UE3::LCPF_ReloadingConfigData) != 0 )
	{
		PostReloadConfig(PropertyToLoad);
	}
}

//static
void UObject::LoadLocalizedDynamicArray(UArrayProperty *Prop, const TCHAR* IntName, const TCHAR* SectionName, const TCHAR* KeyPrefix, UObject* Parent, BYTE* Data)
{
	FConfigSection* Sec = NULL;

	// Find the localization file containing the section.
	// This code should search the same files as Localize(...)
	const TCHAR* LangExt = UObject::GetLanguage();
	for( INT PathIndex=GSys->LocalizationPaths.Num()-1; PathIndex>=0; PathIndex-- )
	{
		// Try specified language first and fall back to default (int) if not found.
		const FFilename FilenameLang	= FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s") PATH_SEPARATOR TEXT("%s.%s"), *GSys->LocalizationPaths(PathIndex), LangExt	  , IntName, LangExt	 );
		Sec = GConfig->GetSectionPrivate( SectionName, 0, 1, *FilenameLang );
		if ( Sec )
		{
			break;
		}

		// Fall back to using INT unless the above was already using INT.
		if( appStricmp(LangExt,TEXT("INT")) != 0 )
		{
			const FFilename FilenameInt	= FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s") PATH_SEPARATOR TEXT("%s.%s"), *GSys->LocalizationPaths(PathIndex), TEXT("INT"), IntName, TEXT("INT") );
			Sec = GConfig->GetSectionPrivate( SectionName, 0, 1, *FilenameInt );
			if ( Sec )
			{
				break;
			}
		}
	}

	// If the section was found
	if( Sec )
	{
		// Get array of properties
		FScriptArray* Ptr  = (FScriptArray*)(Data + Prop->Inner->Offset);

		TMap<INT,FString> LocValues;
		TArray<FString> List;

		// Find each entry in the section
		//Sec->MultiFind( FName(*KeyPrefix), List );
		Sec->MultiFind( KeyPrefix, List );

		if( List.Num() > 0 )
		{
			for ( INT i = List.Num() - 1, cnt = 0; i >= 0; i--, cnt++ )
			{
				LocValues.Set(cnt, *List(i));
			}
		}
		else
		{
			// If nothing was found, try searching for indexed keys
			INT Index = 0;
			FString* FoundKey = NULL;

			do
			{
				// Add array index number to end of key
				TCHAR IndexedKey[MAX_SPRINTF]=TEXT("");
				appSprintf(IndexedKey, TEXT("%s[%i]"), KeyPrefix, Index);

				// Try to find value of key
				FoundKey  = Sec->Find( IndexedKey );

				// If found
				if( FoundKey )
				{
					// add to map of loc values
					LocValues.Set(Index, **FoundKey);
				}

				Index++;
			} while( FoundKey || Index < Ptr->Num() );
		}

		// Get size of each element to help walk the list
		const INT Size = Prop->Inner->ElementSize;

		INT ImportedCount = 0;
		for ( TMap<INT,FString>::TIterator It(LocValues); It; ++It )
		{
			const INT ArrayIndex = It.Key();
			const FString& LocString = It.Value();

			// if this index is higher than the number of elements currently in the array (+1),
			// add empty elements to fill the gap
			if ( ArrayIndex >= Ptr->Num() )
			{
				const INT AddCount = (ArrayIndex - Ptr->Num()) + 1;
				Ptr->AddZeroed( AddCount, Size );
			}

			// keep track of the highest index we imported - we should remove all pre-existing elements that
			// are located higher than this index
			if ( ArrayIndex + 1 > ImportedCount )
			{
				ImportedCount = ArrayIndex + 1;
			}

			Prop->Inner->ImportText( *LocString, (BYTE*)Ptr->GetData() + ArrayIndex * Size, PPF_LocalizedOnly, Parent );
		}

		// this code would remove any pre-existing elements that had a higher index than the highest
		// index we found while importing localized text, but I haven't found any situations where
		// we actually want this to occur....
		//		if ( Ptr->Num() > 0 && ImportedCount > 0 && Ptr->Num() > ImportedCount )
		//			Ptr->Remove(ImportedCount, Ptr->Num() - ImportedCount, Size);
	}

#if !NO_LOGGING && !FINAL_RELEASE
	else
	{
		static UBOOL bShowMissingLoc = ParseParam(appCmdLine(), TEXT("SHOWMISSINGLOC"));
		if ( !bShowMissingLoc )
		{
			const UBOOL bAllowMissingLocData = 
				// static arrays generate lots of false positives, so just ignore them as they aren't that common anymore
				Prop->ArrayDim > 1 ||

				// if the class is abstract or deprecated then the localized data is probably defined in concrete child classes
				Parent->GetClass()->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated) || 

				// localization for PerObjectConfig objects is usually done per-instance, so don't require it for the class's CDO
				(Parent->GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig) && Parent->HasAnyFlags(RF_ClassDefaultObject)) ||

				// if this is an inherited property and the object being localized already has a value for this property, it means that the loc data was defined
				// in the parent's loc section and was inherited from the parent - no need to complain about missing loc data.
				(Prop->GetOwnerClass() != Parent->GetClass() && Prop->HasValue(Data,PPF_LocalizedOnly));

			if ( !bAllowMissingLocData )
			{
				debugfSuppressed(NAME_LocalizationWarning, TEXT("SECTION [%s] not found in FILE '%s' while attempting to load localized DYNAMIC ARRAY %s.%s"),
					SectionName, IntName, *Parent->GetPathName(), *Prop->GetName());
			}
		}
	}
#endif
}

//static
void UObject::LoadLocalizedStruct( UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, UObject* Parent, BYTE* Data )
{
	checkSlow(Struct);
	for ( UProperty* Prop = Struct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext )
	{
		if ( !Prop->IsLocalized() )
		{
			continue;
		}

		for( INT i = 0; i < Prop->ArrayDim; i++ )
		{
			FString NewPrefix;

			// If a key prefix already exists, prepare to append more to it.
			if( KeyPrefix )
			{
				NewPrefix = FString::Printf( TEXT("%s."), KeyPrefix );
			}

			if( Prop->ArrayDim > 1 )
			{
				// Key is an index into a static array.
				NewPrefix += FString::Printf( TEXT("%s[%d]"), *Prop->GetName(), i );
			}
			else
			{
				// Only one entry -- just use the property name.
				NewPrefix += Prop->GetName();
			}

			BYTE* NewData = Data + (Prop->Offset) + (i * Prop->ElementSize );
			LoadLocalizedProp( Prop, IntName, SectionName, *NewPrefix, Parent, NewData );
		}
	}
}

//static
void UObject::LoadLocalizedProp( UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, UObject* Parent, BYTE* Data )
{
	// Is the property a struct property?
	UStructProperty* StructProperty = ExactCast<UStructProperty>( Prop );
	if( StructProperty )
	{
		LoadLocalizedStruct(StructProperty->Struct, IntName, SectionName, KeyPrefix, Parent, Data );
		return;
	}

	// Is the property a dyanmic array?
	UArrayProperty* ArrayProperty = ExactCast<UArrayProperty>( Prop );
	if( ArrayProperty )
	{
		// Load each item in the array - this actually imports the text into the property
		LoadLocalizedDynamicArray( ArrayProperty, IntName, SectionName, KeyPrefix, Parent, Data );
		return;
	}

	FString LocalizedText = Localize( SectionName, KeyPrefix, IntName, NULL, TRUE );
	if( LocalizedText.Len() > 0 )
	{
		Prop->ImportText( *LocalizedText, Data, PPF_LocalizedOnly, Parent );
	}
#if !NO_LOGGING && !FINAL_RELEASE
	else
	{
		static UBOOL bShowMissingLoc = ParseParam(appCmdLine(), TEXT("SHOWMISSINGLOC"));
		if ( !bShowMissingLoc )
		{
			const UBOOL bAllowMissingLocData = 
				// static arrays generate lots of false positives, so just ignore them as they aren't that common anymore
				Prop->ArrayDim > 1 ||
				// if the class is abstract or deprecated then the localized data is probably defined in concrete child classes
				Parent->GetClass()->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated) || 

				// localization for PerObjectConfig objects is usually done per-instance, so don't require it for the class's CDO
				(Parent->GetClass()->HasAnyClassFlags(CLASS_PerObjectConfig) && Parent->HasAnyFlags(RF_ClassDefaultObject)) ||

				// if the object being localized already has a value for this property, it means that the loc data was defined
				// in the object's archetype's loc section and was inherited - no need to complain about missing loc data.
				Prop->HasValue(Data,PPF_LocalizedOnly);

			if ( !bAllowMissingLocData )
			{
				debugfSuppressed(NAME_LocalizationWarning, TEXT("No localized value found for PROPERTY %s.%s in FILE '%s' SECTION [%s] KEY '%s'"), 
					*Parent->GetPathName(), *Prop->GetName(), IntName, SectionName, KeyPrefix);
			}
		}
	}
#endif
}

/**
 * Imports the localized property values for this object.
 *
 * @param	LocBase					the object to use for determing where to load the localized property from; defaults to 'this';  should always be
 *									either 'this' or an archetype of 'this'
 * @param	bLoadHierachecally		specify TRUE to have this object import the localized property data from its archetype's localization location first.
 */
void UObject::LoadLocalized( UObject* LocBase/*=NULL*/, UBOOL bLoadHierachecally/*=FALSE*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_LoadLocalized);
	if ( LocBase == NULL )
	{
		LocBase = this;
	}
	else if ( LocBase != this )
	{
		checkfSlow( IsBasedOnArchetype(LocBase), TEXT("%s is not an archetype of %s"), *LocBase->GetFullName(), *GetFullName() );
	}

	// we want to load localized properties in the editor, but not when compiling scripts
	if( GIsUCCMake )
	{
		return;
	}

	UClass* LocClass = LocBase->GetClass();

	// if this class isn't localized, no data to load
	if ( !LocClass->HasAnyClassFlags(CLASS_Localized) )
	{
		return;
	}

#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive )
	{
		return;
	}
#endif

	debugfSlow(NAME_Localization, TEXT("%sImporting localized property values for %s"), LocBase == this ? TEXT("") : TEXT(">>>  "), *GetFullName());

	if ( bLoadHierachecally == TRUE )
	{
		// if we're supposed to read localized data from our archetype's sections first
		LoadLocalized(LocBase->GetArchetype(), TRUE);
	}

	FString LocFilename, LocSectionName, LocPrefix;
	if ( GetLocalizationDataLocation(LocBase, LocFilename, LocSectionName, LocPrefix) )
	{
		LoadLocalizedStruct( LocClass, *LocFilename, *LocSectionName, LocPrefix.Len() > 0 ? *LocPrefix : NULL, this, (BYTE*)this );
	}
}

/**
 * Retrieves the location of the property values for this object's localized properties.
 *
 * @param	LocBase			the object to use for determing where to load the localized property from; should always be
 *							either 'this' or an archetype of 'this'
 * @param	LocFilename		[out] receives the filename which contains the loc data for this object.
 *							this value will contain the base filename only; no path or extension information
 * @param	LocSection		[out] receives the section name that contains the loc data for this object
 * @param	LocPrefix		[out] receives the prefix string to use when reading keynames from the loc section; usually only relevant when loading
 *							loading loc data for subobjects, and in that case will always be the name of the subobject template
 *
 * @return	TRUE if LocFilename and LocSection were filled with valid values; FALSE if this object's class isn't localized or the loc data location
 *			couldn't be found for some reason.
 */
UBOOL UObject::GetLocalizationDataLocation( UObject* LocBase, FString& LocFilename, FString& LocSection, FString& LocPrefix )
{
	UBOOL bResult = FALSE;

	if ( LocBase == NULL )
	{
		LocBase = this;
	}
	checkSlow(LocBase);

	UClass* LocClass = LocBase->GetClass();
	if ( LocClass->HasAnyClassFlags(CLASS_Localized|CLASS_PerObjectLocalized) || LocBase->HasAnyFlags(RF_PerObjectLocalized) )
	{
		bResult = TRUE;

		if ( LocBase->HasAnyFlags(RF_ClassDefaultObject) )
		{
			// for class default objects, the filename is the package containing the class and the section is the name of the class
			LocFilename = LocClass->GetOutermost()->GetName();
			LocSection = LocClass->GetName();
			LocPrefix = TEXT("");
		}
		else
		{
			// for instances, we have three possibilities (all of which first inherit their localized data from their archetype):
			// 1. instance of a class marked PerObjectConfig - loads its own localized data from a loc section unique to that instance
			// 2. subobject of a class not marked PerObjectConfig - loads its own localized data from the loc section of its Outer's class using a key prefix
			// 3. instance of a class not marked PerObjectConfig - doesn't load its own localized data, reads shared loc data from its class's loc data location
			if ( LocBase->HasAnyFlags(RF_PerObjectLocalized) )
			{
				if ( LocClass->HasAnyClassFlags(CLASS_PerObjectConfig|CLASS_PerObjectLocalized) )
				{
					LocPrefix = TEXT("");

					// case 1 - for POC objects, we need to do a little extra work
					// if the POC object is contained within a content package, the filename is the name of the package containing the instance
					// otherwise, filename is the name of the package containing the class
					// in both cases, the section is formatted like so: [InstanceName ClassName] so that it matches the way that PerObjectConfig .ini sections are named
					if ( LocBase->GetOutermost() != UObject::GetTransientPackage() )
					{
						// contained within a content package
						LocFilename = LocBase->GetOutermost()->GetName();

						FString PathNameString;
						LocBase->GetPathName(LocBase->GetOutermost(), PathNameString);
						LocSection = PathNameString + TEXT(" ") + LocClass->GetName();
					}
					else
					{
						// created at runtime (which hopefully means it's in the transient package)
						LocFilename = LocClass->GetOutermost()->GetName();
						LocSection = LocBase->GetName() + TEXT(" ") + LocClass->GetName();
					}
				}
				else
				{
					checkfSlow(LocBase->GetOuter()->GetOuter(), TEXT("%s marked PerObjectLocalized but is not a subobject or PerObjectConfig (while loading loc data for %s)"), 
						*LocBase->GetFullName(), *GetFullName());

					// case 2 - for instanced subobjects, the prefix is the subobject's name
					// if the subobject is contained within a content package, the filename is the name of the package, and the section is the name of the object's Outer
					// otherwise, the filename is its Outer's class's package name and the section is the Outer's class name
					if ( LocBase->GetOutermost() != UObject::GetTransientPackage() )
					{
						// instanced subobject which lives in a content package
						LocFilename = LocBase->GetOutermost()->GetName();
						LocSection = LocBase->GetOuter()->GetName();
					}
					else
					{
						// instanced subobject created at runtime
						LocFilename = LocBase->GetOuter()->GetClass()->GetOutermost()->GetName();
						LocSection = LocBase->GetOuter()->GetClass()->GetName();
					}
					LocPrefix = LocBase->GetName();
				}
			}
			else
			{
				// case 3 - for normal object instances, the filename is the package containing the class and the section is the name of the class
				LocFilename = LocClass->GetOutermost()->GetName();
				LocSection = LocClass->GetName();
				LocPrefix = TEXT("");
			}
		}
	}

	return bResult;
}

/**
 * Wrapper method for LoadLocalized that is used when reloading localization data for objects at runtime which have already loaded their localization data at least once.
 */
void UObject::ReloadLocalized()
{
	debugfSuppressed(NAME_Localization, TEXT("Reloading localization data for %s"), *GetFullName());
	LoadLocalized(this, TRUE);
}

//
// Save configuration.
//warning: Must be safe on class-default metaobjects.
//!!may benefit from hierarchical propagation, deleting keys that match superclass...not sure what's best yet.
//
void UObject::SaveConfig( QWORD Flags, const TCHAR* InFilename )
{
	if( !GetClass()->HasAnyClassFlags(CLASS_Config) )
	{
		return;
	}

	DWORD PropagationFlags = UE3::LCPF_None;

	const FString Filename
	// if a filename was specified, always load from that file
	=	InFilename
		? InFilename
		: GetConfigFilename(this);

	const UBOOL bPerObject = UsesPerObjectConfig(this);
	FString Section;
	if ( bPerObject == TRUE )
	{
		FString PathNameString;
		GetPathName(GetOutermost(), PathNameString);
		Section = PathNameString + TEXT(" ") + GetClass()->GetName();
	}

	for ( UProperty* Property = GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if ( !Property->HasAnyPropertyFlags(CPF_Config) )
		{
			continue;
		}

		if( (Property->PropertyFlags & Flags) == Flags )
		{
			UClass* BaseClass = GetClass();

			if (Property->PropertyFlags & CPF_GlobalConfig)
			{
				// call LoadConfig() on child classes if any of the properties were global config
				PropagationFlags |= UE3::LCPF_PropagateToChildDefaultObjects;
				BaseClass = Property->GetOwnerClass();
				if ( BaseClass != GetClass() )
				{
					// call LoadConfig() on parent classes only if the global config property was declared in a parent class
					PropagationFlags |= UE3::LCPF_ReadParentSections;
				}
			}

			FString Key				= Property->GetName();
			if ( !bPerObject )
			{
				Section = BaseClass->GetPathName();
			}

			// globalconfig properties should always use the owning class's config file
			// specifying a value for InFilename will override this behavior (as it does with normal properties)
			const FString& PropFileName = ((Property->PropertyFlags & CPF_GlobalConfig) && InFilename == NULL) ? Property->GetOwnerClass()->GetConfigName() : Filename;

			UArrayProperty* Array   = Cast<UArrayProperty>( Property );
			if( Array )
			{
				FConfigSection* Sec = GConfig->GetSectionPrivate( *Section, 1, 0, *PropFileName );
				check(Sec);
				Sec->Remove( *Key );
				FScriptArray* Ptr		= (FScriptArray*)((BYTE*)this + Property->Offset);
				const INT Size	= Array->Inner->ElementSize;
				for( INT i=0; i<Ptr->Num(); i++ )
				{
					FString	Buffer;
					BYTE*	Dest = (BYTE*)Ptr->GetData() + i*Size;
					Array->Inner->ExportTextItem( Buffer, Dest, Dest, this, PPF_ConfigOnly );
					Sec->Add( *Key, *Buffer );
				}
			}
			else
			{
				UMapProperty* Map = Cast<UMapProperty>( Property );
				if( Map )
				{
					FConfigSection* Sec = GConfig->GetSectionPrivate( *Section, 1, 0, *PropFileName );
					check(Sec);
					Sec->Remove( *Key );
					//FScriptArray* Ptr  = (FScriptArray*)((BYTE*)this + Property->Offset);
					//INT     Size = Array->Inner->ElementSize;
					//for( INT i=0; i<Ptr->Num(); i++ )
					//{
					//	TCHAR Buffer[1024]="";
					//	BYTE* Dest = (BYTE*)Ptr->GetData() + i*Size;
					//	Array->Inner->ExportTextItem( Buffer, Dest, Dest, this, PPF_ConfigOnly );
					//	Sec->Add( Key, Buffer );
					//}
				}
				else
				{
					TCHAR TempKey[MAX_SPRINTF]=TEXT("");
					for( INT Index=0; Index<Property->ArrayDim; Index++ )
					{
						if( Property->ArrayDim!=1 )
						{
							appSprintf( TempKey, TEXT("%s[%i]"), *Property->GetName(), Index );
							Key = TempKey;
						}
						FString	Value;
						Property->ExportText( Index, Value, (BYTE*)this, (BYTE*)this, this, PPF_ConfigOnly );
						GConfig->SetString( *Section, *Key, *Value, *PropFileName );
					}
				}
			}
		}
	}

	GConfig->Flush( 0 );
	GetClass()->GetDefaultObject()->LoadConfig( NULL, *Filename, PropagationFlags );
}

/*-----------------------------------------------------------------------------
	Mo Functions.
-----------------------------------------------------------------------------*/

//
// Object accessor.
//
UObject* UObject::GetIndexedObject( INT Index )
{
	if ( GObjObjects.IsValidIndex(Index) )
	{
		return GObjObjects(Index);
	}

	return NULL;
}

/**
 * Variation of StaticFindObjectFast that uses explicit path.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectName		The to be found object's class
 * @param	ObjectPathName	Full path name for the object to search for
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* UObject::StaticFindObjectFastExplicit( UClass* ObjectClass, FName ObjectName, const FString& ObjectPathName, UBOOL bExactClass, EObjectFlags ExcludeFlags/*=0*/ )
{
	// Find an object with the specified name and (optional) class, in any package; if bAnyPackage is FALSE, only matches top-level packages
	INT iHash = GetObjectHash( ObjectName );
	for( UObject* Hash=GObjHash[iHash]; Hash!=NULL; Hash=Hash->HashNext )
	{
		/*
		InName: the object name to search for. Two possibilities.
			A = No dots. ie: 'S_Actor', a texture in Engine
			B = Dots. ie: 'Package.Name' or 'Package.Group.Name', or an even longer chain of outers. The first one needs to be relative to InObjectPackage.
			I'll define InName's package to be everything before the last period, or "" otherwise.
		InObjectPackage: the package or Outer to look for the object in. Can be ANY_PACKAGE, or NULL. Three possibilities:
			A = Non-null. Search for the object relative to this package.
			B = Null. We're looking for the object, as specified exactly in InName.
			C = ANY_PACKAGE. Search anywhere for the package (restrictions on functionality, see below)
		ObjectPackage: The package we need to be searching in. NULL means we don't care what package to search in
			InName.A &&  InObjectPackage.C ==> ObjectPackage = NULL
			InName.A && !InObjectPackage.C ==> ObjectPackage = InObjectPackage
			InName.B &&  InObjectPackage.C ==> ObjectPackage = InName's package
			InName.B && !InObjectPackage.C ==> ObjectPackage = InName's package, but as a subpackage of InObjectPackage
		*/
		if
		(	(Hash->GetFName()==ObjectName)

		/* Don't return objects that have any of the exclusive flags set */
		&&	!Hash->HasAnyFlags(ExcludeFlags)

		/** If a class was specified, check that the object is of the correct class */
		&&	(ObjectClass==NULL || (bExactClass ? Hash->GetClass()==ObjectClass : Hash->IsA(ObjectClass)))

		/** Finally check the explicit path */
		&& ( Hash->GetPathName() == ObjectPathName )
		)
		{
			checkf( !Hash->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Hash->GetFullName() );
			return Hash;
		}
	}
	return NULL;
}

/**
 * Private internal version of StaticFindObjectFast that allows using 0 exclusion flags.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectPackage	The to be found object's outer
 * @param	ObjectName		The to be found object's class
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	bAnyPackage		Whether to look in any package
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* UObject::StaticFindObjectFastInternal( UClass* ObjectClass, UObject* ObjectPackage, FName ObjectName, UBOOL bExactClass, UBOOL bAnyPackage, EObjectFlags ExcludeFlags )
{
	INC_DWORD_STAT(STAT_FindObjectFast);
	// If they specified an outer use that during the hashing
	if (ObjectPackage != NULL)
	{
		// Find in the specified package using the outer hash
		INT iHash = GetObjectOuterHash(ObjectName,(PTRINT)ObjectPackage);
		for( UObject* Hash = GObjHashOuter[iHash]; Hash != NULL; Hash = Hash->HashOuterNext )
		{
			/*
			InName: the object name to search for. Two possibilities.
				A = No dots. ie: 'S_Actor', a texture in Engine
				B = Dots. ie: 'Package.Name' or 'Package.Group.Name', or an even longer chain of outers. The first one needs to be relative to InObjectPackage.
				I'll define InName's package to be everything before the last period, or "" otherwise.
			InObjectPackage: the package or Outer to look for the object in. Can be ANY_PACKAGE, or NULL. Three possibilities:
				A = Non-null. Search for the object relative to this package.
				B = Null. We're looking for the object, as specified exactly in InName.
				C = ANY_PACKAGE. Search anywhere for the package (resrictions on functionality, see below)
			ObjectPackage: The package we need to be searching in. NULL means we don't care what package to search in
				InName.A &&  InObjectPackage.C ==> ObjectPackage = NULL
				InName.A && !InObjectPackage.C ==> ObjectPackage = InObjectPackage
				InName.B &&  InObjectPackage.C ==> ObjectPackage = InName's package
				InName.B && !InObjectPackage.C ==> ObjectPackage = InName's package, but as a subpackage of InObjectPackage
			*/
			if
			/* check that the name matches the name we're searching for */
			(	(Hash->GetFName()==ObjectName)

			/* Don't return objects that have any of the exclusive flags set */
			&&	!Hash->HasAnyFlags(ExcludeFlags)

			/* check that the object has the correct Outer */
			&&	Hash->Outer == ObjectPackage

			/** If a class was specified, check that the object is of the correct class */
			&&	(ObjectClass==NULL || (bExactClass ? Hash->GetClass()==ObjectClass : Hash->IsA(ObjectClass))) )
			{
				checkf( !Hash->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Hash->GetFullName() );
// @todo ObjHash change -- enable FindObject(ANY_PACKAGE) calls have been changed. This will find any items that should not be searched for by name
//				ensureMsgf(IsNameHashed(),TEXT("Object (%s) searched for by name only but should not be in the name hash"),*ObjectName.ToString());
				return Hash;
			}
		}
	}
	else
	{
		// Find an object with the specified name and (optional) class, in any package; if bAnyPackage is FALSE, only matches top-level packages
		INT iHash = GetObjectHash( ObjectName );
		for( UObject* Hash=GObjHash[iHash]; Hash!=NULL; Hash=Hash->HashNext )
		{
			/*
			InName: the object name to search for. Two possibilities.
				A = No dots. ie: 'S_Actor', a texture in Engine
				B = Dots. ie: 'Package.Name' or 'Package.Group.Name', or an even longer chain of outers. The first one needs to be relative to InObjectPackage.
				I'll define InName's package to be everything before the last period, or "" otherwise.
			InObjectPackage: the package or Outer to look for the object in. Can be ANY_PACKAGE, or NULL. Three possibilities:
				A = Non-null. Search for the object relative to this package.
				B = Null. We're looking for the object, as specified exactly in InName.
				C = ANY_PACKAGE. Search anywhere for the package (restrictions on functionality, see below)
			ObjectPackage: The package we need to be searching in. NULL means we don't care what package to search in
				InName.A &&  InObjectPackage.C ==> ObjectPackage = NULL
				InName.A && !InObjectPackage.C ==> ObjectPackage = InObjectPackage
				InName.B &&  InObjectPackage.C ==> ObjectPackage = InName's package
				InName.B && !InObjectPackage.C ==> ObjectPackage = InName's package, but as a subpackage of InObjectPackage
			*/
			if
			(	(Hash->GetFName()==ObjectName)

			/* Don't return objects that have any of the exclusive flags set */
			&&	!Hash->HasAnyFlags(ExcludeFlags)

			/*If there is no package (no InObjectPackage specified, and InName's package is "")
				and the caller specified any_package, then accept it, regardless of its package.*/
			&&	(bAnyPackage
			/*Or, if the object is a top-level package then accept it immediately.*/
			||	(Hash->Outer == ObjectPackage) )

			/** If a class was specified, check that the object is of the correct class */
			&&	(ObjectClass==NULL || (bExactClass ? Hash->GetClass()==ObjectClass : Hash->IsA(ObjectClass))) )
			{
				checkf( !Hash->HasAnyFlags(RF_Unreachable), TEXT("%s"), *Hash->GetFullName() );
				return Hash;
			}
		}
	}
	// Not found.
	return NULL;
}

/**
 * Fast version of StaticFindObject that relies on the passed in FName being the object name
 * without any group/ package qualifiers.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectPackage	The to be found object's outer
 * @param	ObjectName		The to be found object's class
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	AnyPackage		Whether to look in any package
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* UObject::StaticFindObjectFast( UClass* ObjectClass, UObject* ObjectPackage, FName ObjectName, UBOOL ExactClass, UBOOL AnyPackage, EObjectFlags ExclusiveFlags )
{
	if (GIsSavingPackage || GIsGarbageCollecting)
	{
		appErrorf(TEXT("Illegal call to StaticFindObjectFast() while serializing object data or garbage collecting!"));
	}

	// We don't want to return any objects that are currently being background loaded unless we're using FindObject during async loading.
	ExclusiveFlags |= GIsAsyncLoading ? 0 : RF_AsyncLoading;
	return StaticFindObjectFastInternal( ObjectClass, ObjectPackage, ObjectName, ExactClass, AnyPackage, ExclusiveFlags );
}

//
// Find an optional object.
//
UObject* UObject::StaticFindObject( UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, UBOOL ExactClass )
{
	INC_DWORD_STAT(STAT_FindObject);

	if (GIsSavingPackage || GIsGarbageCollecting)
	{
		appErrorf(TEXT("Illegal call to StaticFindObject() while serializing object data or garbage collecting!"));
	}

	// Resolve the object and package name.
	UObject* ObjectPackage = InObjectPackage!=ANY_PACKAGE ? InObjectPackage : NULL;
	FString InName = OrigInName;

	UObject* MatchingObject = NULL;

#if !CONSOLE
	// If the editor is running, and T3D is being imported, ensure any packages referenced are fully loaded.
	if ((GIsEditor == TRUE) && (GIsImportingT3D == TRUE))// && (ObjectPackage != ANY_PACKAGE) && (ObjectPackage != NULL))
	{
		static UBOOL s_bCurrentlyLoading = FALSE;

		if (s_bCurrentlyLoading == FALSE)
		{
			FString NameCheck = OrigInName;
			if ((NameCheck.InStr(TEXT(".")) != -1) && 
				(NameCheck.InStr(TEXT("'")) == -1) && 
				(NameCheck.InStr(TEXT(":")) == -1))
			{
				s_bCurrentlyLoading = TRUE;
				MatchingObject = UObject::StaticLoadObject(ObjectClass, NULL, OrigInName, NULL,  LOAD_NoWarn, NULL);
				s_bCurrentlyLoading = FALSE;
				if (MatchingObject != NULL)
				{
					return MatchingObject;
				}
			}
		}
	}
#endif	//#if !CONSOLE

	if( !ResolveName( ObjectPackage, InName, FALSE, FALSE, ObjectClass==UPackage::StaticClass() ) )
	{
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		// if the script patcher is active and we ARE searching for a package by name, attempt to resolve the
		// package name without the remap suffix (as only native script packages are actually remapped during script patching)
		if(	!GIsScriptPatcherActive
		||	!(ObjectClass == UPackage::StaticClass())
		||	!ResolveName(ObjectPackage, InName, FALSE, FALSE, FALSE) )
		{
			return NULL;
		}
#else
		return NULL;
#endif
	}

	FName ObjectName(*InName, FNAME_Add, TRUE);
	return StaticFindObjectFast( ObjectClass, ObjectPackage, ObjectName, ExactClass, InObjectPackage==ANY_PACKAGE );
}

//
// Find an object; can't fail.
//
UObject* UObject::StaticFindObjectChecked( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, UBOOL ExactClass )
{
	UObject* Result = StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
#if !FINAL_RELEASE
	if( !Result )
	{
		appErrorf( LocalizeSecure(LocalizeError(TEXT("ObjectNotFound"),TEXT("Core")), *ObjectClass->GetName(), ObjectParent==ANY_PACKAGE ? TEXT("Any") : ObjectParent ? *ObjectParent->GetName() : TEXT("None"), InName) );
	}
#endif
	return Result;
}


/**
 * Wrapper for calling UClass::InstanceSubobjectTemplates
 */
void UObject::InstanceSubobjectTemplates( FObjectInstancingGraph* InstanceGraph )
{
	if ( Class->HasAnyClassFlags(CLASS_HasInstancedProps) 
		// don't instance subobjects when compiling script or when instancing is explicitly disabled
	&&	(GUglyHackFlags&HACK_DisableSubobjectInstancing) == 0 )
	{
		Class->InstanceSubobjectTemplates((BYTE*)this, (BYTE*)GetArchetype(), GetArchetype() ? GetArchetype()->GetClass()->GetPropertiesSize() : 0, this, InstanceGraph);
	}
}

/**
 * Wrapper for calling UClass::InstanceComponentTemplates() for this object.
 */
void UObject::InstanceComponentTemplates( FObjectInstancingGraph* InstanceGraph )
{
	if ( Class->HasAnyClassFlags(CLASS_HasComponents) )
	{
		Class->InstanceComponentTemplates( (BYTE*)this, (BYTE*)GetArchetype(), GetArchetype() ? GetArchetype()->GetClass()->GetPropertiesSize() : 0, this, InstanceGraph );
	}
}

// not yet implemented - way too many potential issues...will need to think this one through;
// it's possible that we can't support it at all, or that we might have to use a text buffer to move property values over....

#if 0
/**
 * Safely changes the class of this object, preserving as much of the object's state as possible.
 *
 * @param	Parameters	the parameters to use for this method.
 *
 * @return	TRUE if this object's class was successfully changed to the specified class, FALSE otherwise.
 */
UBOOL UObject::ChangeObjectClass( FChangeObjectClassParameters& Parameters )
{
	UBOOL bResult = FALSE;
	
	UClass* NewClass = *Parameters;
	check(NewClass);


	// first, determine the most-derived class which is a base of both NewClass and this object's current class.
	const UClass* BaseClass = NULL;
	
	if ( Parameters.bAllowNonDerivedClassChange )
	{
		BaseClass = FindNearestCommonBaseClass(NewClass);
	}
	else
	{
		BaseClass = GetClass();
	}

	if ( BaseClass != NULL )
	{
// 		// setup the parmameters for the duplication
// 		FObjectDuplicationParameters DupParms(this, DestOuter);
// 		DupParms.FlagMask = RF_AllFlags;
// 		DupParms.DestName = GetFName();
// 
// 		// we want to duplicate this object but not any of its subobjects, so fill the duplication seed with any subobjects
// 		TArray<UObject*> Subobjects;
// 		FArchiveObjectReferenceCollector SubobjectCollector(&Subobjects, this, FALSE, FALSE, TRUE, FALSE);
// 		Serialize( SubobjectCollector );
// 		for ( INT ObjIndex = 0; ObjIndex < Subobjects.Num(); ObjIndex++ )
// 		{
// 			DupParms.DuplicationSeed.Set(Subobjects(ObjIndex), Subobjects(ObjIndex));
// 		}
// 
// 		StaticDuplicateObjectEx(DupParms);
	}

	return bResult;
}

/**
 * Replaces the archetype for this object with the specified archetype, preserving any values which have been modified on this object.
 *
 * @param	Parameters		the parameters to use for replacing this object's archetype.
 *
 * @return	TRUE if the replacement was successful; FALSE otherwise.
 */
UBOOL UObject::ReplaceArchetype( FReplaceArchetypeParameters& Parameters )
{
	UBOOL bResult = FALSE;

	// verify the input parameters
	checkSlow(Parameters.NewArchetype);
	// verify that the specified archetype is the same class; for changing the class, a different method should be used.
	checkSlow(Parameters.NewArchetype->GetClass() == GetClass());

	// serialize all differences into the reload object archive

	// have the reload object arc map all subobject references, where applicable

	// change the archetype for this object and its subobjects

	// 

	// now de-serialize the data back 

	return bResult;
}
#endif

/**
 * Wrapper function for InitProperties() which handles safely tearing down this object before re-initializing it
 * from the specified source object.
 *
 * @param	SourceObject	the object to use for initializing property values in this object.  If not specified, uses this object's archetype.
 * @param	InstanceGraph	contains the mappings of instanced objects and components to their templates
 */
void UObject::InitializeProperties( UObject* SourceObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ )
{
	if ( SourceObject == NULL )
	{
		SourceObject = GetArchetype();
	}

	check( SourceObject||Class==UObject::StaticClass() );
	checkSlow(Class==UObject::StaticClass()||IsA(SourceObject->Class));

	UClass* SourceClass = SourceObject ? SourceObject->GetClass() : NULL;

#if 0
	INT Size = GetClass()->GetPropertiesSize();
	INT DefaultsCount = SourceClass ? SourceClass->GetPropertiesSize() : 0;

	// since this is not a new UObject, it's likely that it already has existing values
	// for all of its UProperties.  We must destroy these existing values in a safe way.
	// Failure to do this can result in memory leaks in FScriptArrays.
	ExitProperties( (BYTE*)this, GetClass() );

	InitProperties( (BYTE*)this, Size, SourceClass, (BYTE*)SourceObject, DefaultsCount, this, this, InstanceGraph );
#else

	// Recreate this object based on the new archetype - using StaticConstructObject rather than manually tearing down and re-initializing
	// the properties for this object ensures that any cleanup required when an object is reinitialized from defaults occurs properly
	// for example, when re-initializing UPrimitiveComponents, the component must notify the rendering thread that its data structures are
	// going to be re-initialized
	StaticConstructObject( GetClass(), GetOuter(), GetFName(), GetFlags(), SourceObject, GError, HasAnyFlags(RF_ClassDefaultObject) ? NULL : this, InstanceGraph );
#endif
}

/**
 * Sets the ObjectArchetype for this object, optionally reinitializing this object
 * from the new archetype.
 *
 * @param	NewArchetype	the object to change this object's ObjectArchetype to
 * @param	bReinitialize	TRUE if we should the property values should be reinitialized
 *							using the new archetype.
 * @param	InstanceGraph	contains the mappings of instanced objects and components to their templates; only relevant
 *							if bReinitialize is TRUE
 */
void UObject::SetArchetype( UObject* NewArchetype, UBOOL bReinitialize/*=FALSE*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ )
{
	check(NewArchetype);
	check(NewArchetype != this);

	ObjectArchetype = NewArchetype;
	if ( bReinitialize )
	{
		InitializeProperties(NULL, InstanceGraph);
	}
}

/**
 * Wrapper for InitProperties which calls ExitProperties first if this object has already had InitProperties called on it at least once.
 */
void UObject::SafeInitProperties( BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* DefaultData, INT DefaultsCount, UObject* DestObject/*=NULL*/, UObject* SubobjectRoot/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ )
{
	if ( HasAnyFlags(RF_InitializedProps) )
	{
		// since this is not a new UObject, it's likely that it already has existing values
		// for all of its UProperties.  We must destroy these existing values in a safe way.
		// Failure to do this can result in memory leaks in FScriptArrays.
		ExitProperties( Data, GetClass() );
	}

	SetFlags(RF_InitializedProps);
	InitProperties(Data, DataCount, DefaultsClass, DefaultData, DefaultsCount, DestObject, SubobjectRoot, InstanceGraph);
}

/**
 * Binary initialize object properties to zero or defaults.
 *
 * @param	Data				the data that needs to be initialized
 * @param	DataCount			the size of the buffer pointed to by Data
 * @param	DefaultsClass		the class to use for initializing the data
 * @param	DefaultData			the buffer containing the source data for the initialization
 * @param	DefaultsCount		the size of the buffer pointed to by DefaultData
 * @param	DestObject			if non-NULL, corresponds to the object that is located at Data
 * @param	SubobjectRoot
 *						Only used to when duplicating or instancing objects; in a nested subobject chain, corresponds to the first object in DestObject's Outer chain that is not a subobject (including DestObject).
 *						A value of INVALID_OBJECT for this parameter indicates that we are calling StaticConstructObject to duplicate or instance a non-subobject (which will be the subobject root for any subobjects of the new object)
 *						A value of NULL indicates that we are not instancing or duplicating an object.
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 */
void UObject::InitProperties( BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* DefaultData, INT DefaultsCount, UObject* DestObject/*=NULL*/, UObject* SubobjectRoot/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ )
{
	SCOPE_CYCLE_COUNTER(STAT_InitProperties);
	check( !DefaultsClass || !DefaultsClass->GetMinAlignment() || Align(DataCount, DefaultsClass->GetMinAlignment()) >= sizeof(UObject) );

	// copying properties during registration runs the risk of a double free if
	// not all default objects get initialised during startup
	if (GIsUCCMake && (GUglyHackFlags & HACK_SkipCopyDuringRegistration))
	{
		return;
	}

	// Inited needs to be the PropertiesSize of UObject (not sizeof(UObject) which may get padded out on some platforms)
	// , but we may not have it here yet. So, we get the properties size by taking the offset of the last property and 
	// adding the size of it.
	INT Inited = STRUCT_OFFSET(UObject, ObjectArchetype) + sizeof(UObject*);

	// Find class defaults if no template was specified.
	//warning: At startup, DefaultsClass->Defaults.Num() will be zero for some native classes.
	if( !DefaultData && DefaultsClass && DefaultsClass->GetDefaultsCount() )
	{
		DefaultData   = DefaultsClass->GetDefaults();
		DefaultsCount = DefaultsClass->GetDefaultsCount();
	}

	// Copy defaults appended after the UObject variables.
	if( DefaultData && DefaultsCount > Inited )
	{
		checkSlow(DefaultsCount>=Inited);
		// NOTE: The check below can crash with a false postive because it's not taking all alignments into account.
//		checkSlow(DefaultsCount<=DataCount);
		appMemcpy( Data+Inited, DefaultData+Inited, DefaultsCount-Inited );
		Inited = DefaultsCount;
	}

	// Zero-fill any remaining portion. (moved to StaticAllocateObject to support
	// reinitializing objects that have already been initialized from a stable state
	// at least once.
	//@fixme - 
	// probably need to make this a parameter, so that we can clear existing values
	// that for properties declared in the target class, when reinitializing objects
//	if( Inited < DataCount )
//		appMemzero( Data+Inited, DataCount-Inited );

	// if SubobjectRoot is INVALID_OBJECT, it means we are instancing or duplicating a root object (non-subobject).  In this case,
	// we'll want to use DestObject as the SubobjectRoot for instancing any components/subobjects contained by this object.
	if ( SubobjectRoot == INVALID_OBJECT )
	{
		SubobjectRoot = DestObject;
	}

	if( DefaultsClass && SubobjectRoot )
	{
		// This is a duplicate. The value for all transient or non-duplicatable properties should be copied
		// from the source class's defaults.
		checkSlow(DestObject);
		BYTE* ClassDefaults = DefaultsClass->GetDefaults();		

		for( UProperty* P=DestObject->GetClass()->PropertyLink; P; P = P->PropertyLinkNext )
		{		
			if( !P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient) )
			{
				continue;
			}

			// Bools are packed bitfields which might contain both transient and non- transient members so we can't use memcpy.
			if( Cast<UBoolProperty>(P,CLASS_IsAUBoolProperty) )
			{
				P->CopyCompleteValue(Data + P->Offset, ClassDefaults + P->Offset, NULL);
			}
			else if( !P->HasAnyPropertyFlags(CPF_NeedCtorLink) )
			{
				// if this property's value doesn't use dynamically allocated memory, just block copy
				appMemcpy( Data + P->Offset, ClassDefaults + P->Offset, P->ArrayDim * P->ElementSize );
			}
			else
			{
				// the heap memory allocated at this address is owned by the parent class, so
				// zero out the existing data so that the UProperty code doesn't attempt to
				// de-allocate it before allocating the memory for the new value
				appMemzero( Data + P->Offset, P->GetSize() );
				P->CopyCompleteValue( Data + P->Offset, ClassDefaults + P->Offset, SubobjectRoot, DestObject, InstanceGraph );
			}
		}
	}

	// Construct anything required.
	if( DefaultsClass && DefaultData )
	{
		for( UProperty* P=DefaultsClass->ConstructorLink; P; P=P->ConstructorLinkNext )
		{
			if( P->Offset < DefaultsCount )
			{
				// skip if SourceOwnerObject != NULL and this is a transient property - in this
				// situation, the new value for the property has already been copied from the class defaults
				// in the block of code above
				if ( SubobjectRoot == NULL || !P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient) )
				{
					// the heap memory allocated at this address is owned the source object, so
					// zero out the existing data so that the UProperty code doesn't attempt to
					// de-allocate it before allocating the memory for the new value
					appMemzero( Data + P->Offset, P->GetSize() );//bad for bools, but not a real problem because they aren't constructed!!
					P->CopyCompleteValue( Data + P->Offset, DefaultData + P->Offset, SubobjectRoot ? SubobjectRoot : DestObject, DestObject, InstanceGraph );
				}
			}
		}
	}
}

//
// Destroy properties.
//
void UObject::ExitProperties( BYTE* Data, UClass* Class )
{
	UProperty* P=NULL;
	for( P=Class->ConstructorLink; P; P=P->ConstructorLinkNext )
	{
		// Only destroy values of properties which have been loaded completely.
		// This can be encountered by loading a package in ucc make which references a class which has already been compiled and saved.
		// The class is already in memory, so when the class is loaded from the package on disk, it reuses the memory address of the existing class with the same name.
		// Before reusing the memory address, it destructs the default properties of the existing class.
		// However, at this point, the properties may have been reallocated but not deserialized, leaving them in an invalid state.
		if(!P->HasAnyFlags(RF_NeedLoad))
		{
			P->DestroyValue( Data + P->Offset );
		}
		else
		{
			check(GIsUCC);
		}
	}
}

/**
 * Initializes the properties for this object based on the property values of the
 * specified class's default object
 *
 * @param	InClass		the class to use for initializing this object
 * @param	SetOuter	TRUE if the Outer for this object should be changed
 * @param	bPseudoObject	TRUE if 'this' does not point to a real UObject.  Used when
 *							treating an arbitrary block of memory as a UObject.  Specifying
 *							TRUE for this parameter has the following side-effects:
 *							- vtable for this UObject is copied from the specified class
 *							- sets the Class for this object to the specified class
 *							- sets the Index property of this UObject to INDEX_NONE
 */
void UObject::InitClassDefaultObject( UClass* InClass, UBOOL SetOuter, UBOOL bPseudoObject )
{
	if ( bPseudoObject )
	{
		// Init UObject portion.
		// @note: clang warns about overwriting the vtable here, the (void*) cast allows it
		appMemset( (void*)this, 0, sizeof(UObject) );
		*(void**)this = *(void**)InClass;
		Class         = InClass;
		Index         = INDEX_NONE;
	}
	else if ( HasAnyFlags(RF_InitializedProps) && InClass->HasAnyClassFlags(CLASS_Intrinsic) )
	{
		// since intrinsic classes aren't loaded from disk, clear any RF_NeedLoad flags so that InitProperties doesn't think
		// we're replacing an object that is waiting to load....
		ClearFlags(RF_NeedLoad);
		InitializeProperties(GetArchetype(), NULL);
	}

	// Init post-UObject portion.
	else
	{
		if( SetOuter )
		{
			Outer = InClass->GetOuter();
		}

		SafeInitProperties( (BYTE*)this, InClass->GetPropertiesSize(), InClass->GetSuperClass(), NULL, 0, SetOuter ? this : NULL );
	}
}

/**
 * Returns a pointer to this object safely converted to a pointer to the specified interface class.
 *
 * @param	InterfaceClass	the interface class to use for the returned type
 *
 * @return	a pointer that can be assigned to a variable of the interface type specified, or NULL if this object's
 *			class doesn't implement the interface indicated.  Will be the same value as 'this' if the interface class
 *			isn't native.
 */
void* UObject::GetInterfaceAddress( UClass* InterfaceClass )
{
	void* Result = NULL;

	if ( InterfaceClass != NULL && InterfaceClass->HasAnyClassFlags(CLASS_Interface) && InterfaceClass != UInterface::StaticClass() )
	{
		if ( !InterfaceClass->HasAnyClassFlags(CLASS_Native) )
		{
			if ( GetClass()->ImplementsInterface(InterfaceClass) )
			{
				// if it isn't a native interface, the address won't be different
				Result = this;
			}
		}
		else
		{
			for( UClass* CurrentClass=GetClass(); Result == NULL && CurrentClass != NULL; CurrentClass = CurrentClass->GetSuperClass() )
			{
				for (TArray<FImplementedInterface>::TIterator It(CurrentClass->Interfaces); It; ++It)
				{
					UClass* ImplementedInterfaceClass = It->Class;
					if ( ImplementedInterfaceClass == InterfaceClass || ImplementedInterfaceClass->IsChildOf(InterfaceClass) )
					{
						UProperty* VfTableProperty = It->PointerProperty;
						if ( VfTableProperty != NULL )
						{
							checkSlow(VfTableProperty->ArrayDim == 1);
							Result = (BYTE*)this + VfTableProperty->Offset;
							break;
						}
						else
						{
							// if it isn't a native interface, the address won't be different
							Result = this;
							break;
						}
					}
				}
			}
		}
	}

	return Result;
}

//
// Global property setting.
//
void UObject::GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, INT Offset, UBOOL bNotifyObjectOfChange )
{
	if ( Property != NULL && Class != NULL && (Property->PropertyFlags&CPF_Native) == 0 )
	{
		// Apply to existing objects of the class.
		for( FObjectIterator It; It; ++It )
		{	
			UObject* Object = *It;
			if( Object->IsA(Class) && !Object->IsPendingKill() )
			{
				// If we're in a PIE session then only allow set commands to affect PlayInEditor objects.
				if( !GIsPlayInEditorWorld || ( Object->GetOutermost()->PackageFlags & PKG_PlayInEditor ) != 0 )
				{
					if( !Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange )
					{
						Object->PreEditChange(Property);
					}
					Property->ImportText( Value, (BYTE*)Object + Offset, PPF_Localized, Object );
					if( !Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange )
					{
						FPropertyChangedEvent PropertyEvent(Property);
						Object->PostEditChangeProperty(PropertyEvent);
					}
				}
			}
		}

#if !CONSOLE
		// Apply to defaults.
		UObject* DefaultObject = Class->GetDefaultObject();
		check(DefaultObject != NULL);
		DefaultObject->SaveConfig();
#endif
	}
}

/*-----------------------------------------------------------------------------
	Object registration.
-----------------------------------------------------------------------------*/

//
// Preregister an object.
//warning: Sometimes called at startup time.
//
void UObject::Register()
{
	check(GObjInitialized);

	// Get stashed registration info.
	const TCHAR* InOuter = *(const TCHAR**)&Outer;
	const TCHAR* InName  = *(const TCHAR**)&Name;

	// Set object properties.
	Outer        = CreatePackage(NULL,InOuter);
	Name         = InName;
	_LinkerIndex = (PTRINT)INDEX_NONE;
	NetIndex = INDEX_NONE;

	// Validate the object.
	if( Outer==NULL )
		appErrorf( TEXT("Autoregistered object %s is unpackaged"), *GetFullName() );
	if( GetFName()==NAME_None )
		appErrorf( TEXT("Autoregistered object %s has invalid name"), *GetFullName() );
	if( StaticFindObject( NULL, GetOuter(), *GetName() ) )
		appErrorf( TEXT("Autoregistered object %s already exists"), *GetFullName() );

	// Add to the global object table.
	AddObject( INDEX_NONE );
}

//
// Handle language change.
//
void UObject::LanguageChange()
{
	LoadLocalized(NULL, GIsRunning);
}

/*-----------------------------------------------------------------------------
	StaticInit & StaticExit.
-----------------------------------------------------------------------------*/

//
// Init the object manager and allocate tables.
//
void UObject::StaticInit()
{
	GObjNoRegister = 1;

	// Checks.
	check(sizeof(BYTE)==1);
	check(sizeof(SBYTE)==1);
	check(sizeof(WORD)==2);
	check(sizeof(DWORD)==4);
	check(sizeof(QWORD)==8);
	check(sizeof(ANSICHAR)==1);
	check(sizeof(UNICHAR)==2);
	check(sizeof(SWORD)==2);
	check(sizeof(INT)==4);
	check(sizeof(SQWORD)==8);
	check(sizeof(UBOOL)==4);
	check(sizeof(FLOAT)==4);
	check(sizeof(DOUBLE)==8);
	check(GEngineMinNetVersion<=GEngineVersion);

#if !_WIN64
	// On 32-bit platforms we make sure that ObjectFlags is 8-byte aligned to reduce memory waste
	check(STRUCT_OFFSET(UObject,ObjectFlags)==Align(STRUCT_OFFSET(UObject,ObjectFlags),8));
#endif

	// Zero initialize and later on get value from .ini so it is overridable per game/ platform...
	INT MaxObjectsNotConsideredByGC	= 0;  
	INT SizeOfPermanentObjectPool	= 0;

	// To properly set MaxObjectsNotConsideredByGC look for "Log: XXX objects as part of root set at end of initial load."
	// in your log file. This is being logged from LaunchEnglineLoop after objects have been added to the root set. 

	// Disregard for GC relies on seekfree loading for interaction with linkers. We also don't want to use it in the Editor, for which
	// GUseSeekFreeLoading will be FALSE. Please note that GIsEditor and GIsGame are not valid at this point.
	if( GUseSeekFreeLoading )
	{
		GConfig->GetInt( TEXT("Core.System"), TEXT("MaxObjectsNotConsideredByGC"), MaxObjectsNotConsideredByGC, GEngineIni );
#if CONSOLE
		// Not used on PC as in-place creation inside bigger pool interacts with the exit purge and deleting UObject directly.
		GConfig->GetInt( TEXT("Core.System"), TEXT("SizeOfPermanentObjectPool"), SizeOfPermanentObjectPool, GEngineIni );
#endif
	}

	// Log what we're doing to track down what really happens as log in LaunchEngineLoop doesn't report those settings in pristine form.
	debugf( NAME_Init, TEXT("Presizing for %i objects not considered by GC, pre-allocating %i bytes."), MaxObjectsNotConsideredByGC, SizeOfPermanentObjectPool );

	// GObjFirstGCIndex is the index at which the garbage collector will start for the mark phase.
	GObjFirstGCIndex			= MaxObjectsNotConsideredByGC;
	GPermanentObjectPoolSize	= SizeOfPermanentObjectPool;
	GPermanentObjectPool		= (BYTE*) appMalloc( GPermanentObjectPoolSize );
	GPermanentObjectPoolTail	= GPermanentObjectPool;

	// Presize array.
	check( GObjObjects.Num() == 0 );
	if( GObjFirstGCIndex )
	{
		GObjObjects.AddZeroed( GObjFirstGCIndex );
	}

	// Init hash.
	appMemzero(GObjHash,sizeof(UObject*) * OBJECT_HASH_BINS);
	appMemzero(GObjHashOuter,sizeof(UObject*) * OBJECT_HASH_BINS);

	// If statically linked, initialize registrants.
	INT Lookup = 0; // Dummy required by AUTO_INITIALIZE_REGISTRANTS_CORE
	extern void AutoInitializeRegistrantsCore( INT& Lookup );
	AutoInitializeRegistrantsCore( Lookup );

	// Note initialized.
	GObjInitialized = 1;

	// Add all autoregistered classes.
	ProcessRegistrants();

	// Allocate special packages.
	GObjTransientPkg = new( NULL, TEXT("Transient") )UPackage;
	GObjTransientPkg->AddToRoot();

	debugf( NAME_Init, TEXT("Object subsystem initialized") );
}

//
// Process all objects that have registered.
//
void UObject::ProcessRegistrants()
{
	GObjRegisterCount++;
	TArray<UObject*>	ObjRegistrants;
	// Make list of all objects to be registered.
	for( ; GAutoRegister; GAutoRegister=*(UObject **)&GAutoRegister->_LinkerIndex )
		ObjRegistrants.AddItem( GAutoRegister );
	for( INT i=0; i<ObjRegistrants.Num(); i++ )
	{
		ObjRegistrants(i)->ConditionalRegister();
		for( ; GAutoRegister; GAutoRegister=*(UObject **)&GAutoRegister->_LinkerIndex )
			ObjRegistrants.AddItem( GAutoRegister );
	}
	ObjRegistrants.Empty();
	check(!GAutoRegister);
	--GObjRegisterCount;
}

//
// Shut down the object manager.
//
void UObject::StaticExit()
{
	check(GObjLoaded.Num()==0);
	check(GObjRegistrants.Num()==0);
	check(!GAutoRegister);

	// Send notice that we're shutting down
	if( GCallbackEvent != NULL )
	{
		GCallbackEvent->Send( CALLBACK_PreEngineShutdown );
	}

	// Cleanup root.
	if (GObjTransientPkg != NULL)
	{
		GObjTransientPkg->RemoveFromRoot();
	}

	// Make sure any pending purge has finished before changing RF_Unreachable and don't use time limit.
	if( GObjIncrementalPurgeIsInProgress )
	{
		IncrementalPurgeGarbage( FALSE );
	}

	// Keep track of how many objects there are for GC stats as we simulate a mark pass.
	extern INT GObjectCountDuringLastMarkPhase;
	GObjectCountDuringLastMarkPhase = 0;

	// Tag all non template & class objects as unreachable. We can't use object iterators for this as they ignore certain objects.
	//
	// Excluding class default, archetype and class objects allows us to not have to worry about fixing issues with initialization 
	// and certain CDO objects like UNetConnection and UChildConnection having members with arrays that point to the same data and 
	// will be double freed if destroyed. Hacky, but much cleaner and lower risk than trying to fix the root cause behind it all. 
	// We need the exit purge for closing network connections and such and only operating on instances of objects is sufficient for 
	// this purpose.
	for( INT ObjectIndex=0; ObjectIndex<UObject::GObjObjects.Num(); ObjectIndex++ )
	{
		UObject* Object = UObject::GObjObjects(ObjectIndex);
		if( Object )
		{
			// Valid object.
			GObjectCountDuringLastMarkPhase++;

			// Mark as unreachable so purge phase will kill it.
			Object->SetFlags( RF_Unreachable );
		}
	}

	// Route BeginDestroy. This needs to be a separate pass from marking as RF_Unreachable as code might rely on RF_Unreachable to be 
	// set on all objects that are about to be deleted. One example is ULinkerLoad detaching textures - the SetLinker call needs to 
	// not kick off texture streaming.
	//
	// Can't use object iterators for this as they ignore certain objects.
	for( INT ObjectIndex=0; ObjectIndex<UObject::GObjObjects.Num(); ObjectIndex++ )
	{
		UObject* Object = UObject::GObjObjects(ObjectIndex);
		if( Object && Object->HasAnyFlags( RF_Unreachable ) )
		{
			// Begin the object's asynchronous destruction.
			Object->ConditionalBeginDestroy();
		}
	}

	// Fully purge all objects, not using time limit.
	GExitPurge					= TRUE;
 	GObjPurgeIsRequired			= TRUE;
	GObjFirstGCIndex			= 0;
	GObjCurrentPurgeObjectIndex	= 0;

	// when compiling scripts, this can crash if a native class changed sizes
	if (!GIsUCCMake || !GIsRequestingExit)
	{
		IncrementalPurgeGarbage( FALSE );
	}

	// Empty arrays to prevent falsely-reported memory leaks.
	GObjLoaded			.Empty();
	GObjObjects			.Empty();
	GObjAvailable		.Empty();
	GObjLoaders			.Empty();
	GObjRegistrants		.Empty();
	GObjAsyncPackages	.Empty();

	GObjInitialized = 0;
	debugf( NAME_Exit, TEXT("Object subsystem successfully closed.") );
}
	
/*-----------------------------------------------------------------------------
	UObject Tick.
-----------------------------------------------------------------------------*/

/**
 * Static UObject tick function, used to verify certain key assumptions and to tick the async loading code.
 *
 * @warning: The streaming stats rely on this function not doing any work besides calling ProcessAsyncLoading.
 * @todo: Move stats code into core?
 *
 * @param DeltaTime	Time in seconds since last call
 */
void UObject::StaticTick( FLOAT DeltaTime )
{
	check(GObjBeginLoadCount==0);

	// Spend a bit of time (pre)loading packages - currently 5 ms.
	ProcessAsyncLoading( TRUE, 0.005f );

	// Check natives.
	extern int GNativeDuplicate;
	if( GNativeDuplicate )
	{
		appErrorf( TEXT("Duplicate native registered: %i"), GNativeDuplicate );
	}
	// Check for duplicates.
	extern int GCastDuplicate;
	if( GCastDuplicate )
	{
		appErrorf( TEXT("Duplicate cast registered: %i"), GCastDuplicate );
	}

	// Enable the below to verify object hash every frame.
	// VerifyObjectHash();

#if STATS
	// Set name table stats.
	INT NameTableEntries = FName::GetMaxNames();
	INT NameTableAnsiEntries = FName::GetNumAnsiNames();
	INT NameTableUnicodeEntries = FName::GetNumUnicodeNames();
	INT NameTableMemorySize = FName::GetNameTableMemorySize();
	SET_DWORD_STAT( STAT_NameTableEntries, NameTableEntries );
	SET_DWORD_STAT( STAT_NameTableAnsiEntries, NameTableAnsiEntries );
	SET_DWORD_STAT( STAT_NameTableUnicodeEntries, NameTableUnicodeEntries);
	SET_DWORD_STAT( STAT_NameTableMemorySize, NameTableMemorySize );

	// Set async I/O bandwidth stats.
	static DWORD PreviousReadSize	= 0;
	static DWORD PrevioudReadCount	= 0;
	static FLOAT PreviousReadTime	= 0;
	FLOAT ReadTime	= GStatManager.GetStatValueFLOAT( STAT_AsyncIO_PlatformReadTime );
	DWORD ReadSize	= GStatManager.GetStatValueDWORD( STAT_AsyncIO_FulfilledReadSize );
	DWORD ReadCount	= GStatManager.GetStatValueDWORD( STAT_AsyncIO_FulfilledReadCount );

	// It is possible that the stats are update in between us reading the values so we simply defer till
	// next frame if that is the case. This also handles partial updates. An individual value might be 
	// slightly wrong but we have enough small requests to smooth it out over a few frames.
	if( (ReadTime  - PreviousReadTime ) > 0.f 
	&&	(ReadSize  - PreviousReadSize ) > 0 
	&&	(ReadCount - PrevioudReadCount) > 0 )
	{
		FLOAT Bandwidth = (ReadSize - PreviousReadSize) / (ReadTime - PreviousReadTime) / 1048576.f;
		SET_FLOAT_STAT( STAT_AsyncIO_Bandwidth, Bandwidth );
		PreviousReadTime	= ReadTime;
		PreviousReadSize	= ReadSize;
		PrevioudReadCount	= ReadCount;
	}
	else
	{
		SET_FLOAT_STAT( STAT_AsyncIO_Bandwidth, 0.f );
	}
#endif
}

/*-----------------------------------------------------------------------------
   Shutdown.
-----------------------------------------------------------------------------*/

//
// Make sure this object has been shut down.
//
void UObject::ConditionalShutdownAfterError()
{
	if( !HasAnyFlags(RF_ErrorShutdown) )
	{
		SetFlags( RF_ErrorShutdown );
#if !EXCEPTIONS_DISABLED
		try
#endif
		{
			ShutdownAfterError();
		}
#if !EXCEPTIONS_DISABLED
		catch( ... )
		{
			debugf( NAME_Exit, TEXT("Double fault in object ShutdownAfterError") );
		}
#endif
	}
}

//
// After a critical error, shutdown all objects which require
// mission-critical cleanup, such as restoring the video mode,
// releasing hardware resources.
//
void UObject::StaticShutdownAfterError()
{
#if PS3 // @todo hack - break here so GDB has a prayer of helping
	appDebugBreak();
#endif
	if( GObjInitialized )
	{
		static UBOOL Shutdown=0;
		if( Shutdown )
			return;
		Shutdown = 1;
		debugf( NAME_Exit, TEXT("Executing UObject::StaticShutdownAfterError") );
#if !EXCEPTIONS_DISABLED
		try
#endif
		{
			for( INT i=0; i<GObjObjects.Num(); i++ )
				if( GObjObjects(i) )
					GObjObjects(i)->ConditionalShutdownAfterError();
		}
#if !EXCEPTIONS_DISABLED
		catch( ... )
		{
			debugf( NAME_Exit, TEXT("Double fault in object manager ShutdownAfterError") );
		}
#endif
	}
}

//
// Bind package to DLL.
//warning: Must only find packages in the \Unreal\System directory!
//
void UObject::BindPackage( UPackage* Package )
{
	if( !Package->IsBound() && !Package->GetOuter())
	{
		Package->SetBound( TRUE );
		GObjNoRegister = 0;
		GObjNoRegister = 1;
		ProcessRegistrants();
	}
}

/*-----------------------------------------------------------------------------
   Command line.
-----------------------------------------------------------------------------*/
#include "UnClassTree.h"

static void ShowIntrinsicClasses( FOutputDevice& Ar )
{
	FClassTree MarkedClasses(UObject::StaticClass());
	FClassTree UnmarkedClasses(UObject::StaticClass());

	for ( TObjectIterator<UClass> It; It; ++It )
	{
		if ( It->HasAnyClassFlags(CLASS_Native) )
		{
			if ( It->HasAllClassFlags(CLASS_Intrinsic) )
			{
				MarkedClasses.AddClass(*It);
			}
			else if ( !It->HasAnyClassFlags(CLASS_Parsed) )
			{
				UnmarkedClasses.AddClass(*It);
			}
		}
	}

	Ar.Logf(TEXT("INTRINSIC CLASSES WITH FLAG SET: %i classes"), MarkedClasses.Num());
	MarkedClasses.DumpClassTree(0, Ar);

	Ar.Logf(TEXT("INTRINSIC CLASSES WITHOUT FLAG SET: %i classes"), UnmarkedClasses.Num());
	UnmarkedClasses.DumpClassTree(0, Ar);
}

//
// Show the inheritance graph of all loaded classes.
//
static void ShowClasses( UClass* Class, FOutputDevice& Ar, int Indent )
{
	Ar.Logf( TEXT("%s%s (%d)"), appSpc(Indent), *Class->GetName(), Class->GetPropertiesSize() );
	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->GetSuperClass() == Class )
		{
			ShowClasses( *It, Ar, Indent+2 );
		}
	}
}

struct FItem
{
	UClass*	Class;
	INT		Count;
	SIZE_T	Num, Max, Res, TrueRes;
	FItem( UClass* InClass=NULL )
	: Class(InClass), Count(0), Num(0), Max(0), Res(0), TrueRes(0)
	{}
	void Add( FArchiveCountMem& Ar, SIZE_T InRes, SIZE_T InTrueRes )
	{
		Count++;
		Num += Ar.GetNum();
		Max += Ar.GetMax();
		Res += InRes;
		TrueRes += InTrueRes;
	}
};
struct FSubItem
{
	UObject* Object;
	SIZE_T Num, Max, Res, TrueRes;
	FSubItem( UObject* InObject, SIZE_T InNum, SIZE_T InMax, SIZE_T InRes, SIZE_T InTrueRes )
	: Object( InObject ), Num( InNum ), Max( InMax ), Res( InRes ), TrueRes( InTrueRes )
	{}
};

static UBOOL bAlphaSort = FALSE;
static UBOOL bCountSort = FALSE;
IMPLEMENT_COMPARE_CONSTREF( FSubItem, UnObj, { return bAlphaSort ? appStricmp(*A.Object->GetPathName(),*B.Object->GetPathName()) : B.Max - A.Max; } );
IMPLEMENT_COMPARE_CONSTREF( FItem, UnObj, { return bAlphaSort ? appStricmp(*A.Class->GetName(),*B.Class->GetName()) : bCountSort ? B.Count - A.Count : B.Max - A.Max; } );

FReferencerInformation::FReferencerInformation( UObject* inReferencer, INT InReferences, const TArray<UProperty*>& InProperties )
: Referencer(inReferencer), TotalReferences(InReferences), ReferencingProperties(InProperties)
{
}

FReferencerInformationList::FReferencerInformationList()
{
}
FReferencerInformationList::FReferencerInformationList( const TArray<FReferencerInformation>& InternalRefs, const TArray<FReferencerInformation>& ExternalRefs )
: InternalReferences(InternalRefs), ExternalReferences(ExternalRefs)
{
}


/**
 * Outputs a string to an arbitrary output device, describing the list of objects which are holding references to this one.
 *
 * @param	Ar						the output device to send output to
 * @param	bIncludeTransients		controls whether objects marked RF_Transient will be considered
 * @param	out_Referencers			optionally allows the caller to receive the list of objects referencing this one.
 */
void UObject::OutputReferencers( FOutputDevice& Ar, UBOOL bIncludeTransients, FReferencerInformationList* out_Referencers/*=NULL*/ )
{
	TArray<FReferencerInformation> InternalReferences;
	TArray<FReferencerInformation> ExternalReferences;
	RetrieveReferencers(&InternalReferences, &ExternalReferences, bIncludeTransients);

	Ar.Log( TEXT("\r\n") );
	if ( InternalReferences.Num() > 0 || ExternalReferences.Num() > 0 )
	{
		if ( ExternalReferences.Num() > 0 )
		{
			Ar.Logf( TEXT("External referencers of %s:\r\n"), *GetFullName() );

			for ( INT RefIndex = 0; RefIndex < ExternalReferences.Num(); RefIndex++ )
			{
				FReferencerInformation& RefInfo = ExternalReferences(RefIndex);

				Ar.Logf( TEXT("   %s (%i)\r\n"), *RefInfo.Referencer->GetFullName(), RefInfo.TotalReferences );
				for ( INT i = 0; i < RefInfo.TotalReferences; i++ )
				{
					if ( i < RefInfo.ReferencingProperties.Num() )
					{
						UProperty* Referencer = RefInfo.ReferencingProperties(i);
						Ar.Logf(TEXT("      %i) %s\r\n"), i, *Referencer->GetFullName());
					}
					else
					{
						Ar.Logf(TEXT("      %i) [[native reference]]\r\n"), i);
					}
				}
			}
		}

		if ( InternalReferences.Num() > 0 )
		{
			if ( ExternalReferences.Num() > 0 )
			{
				Ar.Log(TEXT("\r\n"));
			}

			Ar.Logf( TEXT("Internal referencers of %s:\r\n"), *GetFullName() );
			for ( INT RefIndex = 0; RefIndex < InternalReferences.Num(); RefIndex++ )
			{
				FReferencerInformation& RefInfo = InternalReferences(RefIndex);

				Ar.Logf( TEXT("   %s (%i)\r\n"), *RefInfo.Referencer->GetFullName(), RefInfo.TotalReferences );
				for ( INT i = 0; i < RefInfo.TotalReferences; i++ )
				{
					if ( i < RefInfo.ReferencingProperties.Num() )
					{
						UProperty* Referencer = RefInfo.ReferencingProperties(i);
						Ar.Logf(TEXT("      %i) %s\r\n"), i, *Referencer->GetFullName());
					}
					else
					{
						Ar.Logf(TEXT("      %i) [[native reference]]\r\n"), i);
					}
				}
			}
		}
	}
	else
	{
		Ar.Logf(TEXT("%s is not referenced"), *GetFullName());
	}

	Ar.Logf(TEXT("\r\n") );

#if !FINAL_RELEASE
	Ar.Logf(TEXT("Shortest reachability from root to %s:\r\n"), *GetFullName() );
	TMap<UObject*,UProperty*> Rt = FArchiveTraceRoute::FindShortestRootPath(this,bIncludeTransients,GARBAGE_COLLECTION_KEEPFLAGS);

	FString RootPath = FArchiveTraceRoute::PrintRootPath(Rt, this);
	Ar.Log(*RootPath);

	Ar.Logf(TEXT("\r\n") );
#endif

	if ( out_Referencers != NULL )
	{
		*out_Referencers = FReferencerInformationList(ExternalReferences, InternalReferences);
	}
}

void UObject::RetrieveReferencers( TArray<FReferencerInformation>* OutInternalReferencers, TArray<FReferencerInformation>* OutExternalReferencers, UBOOL bIncludeTransients )
{
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;

		if ( Object == this )
		{
			// this one is pretty easy  :)
			continue;
		}

		FArchiveFindCulprit ArFind(this,Object,false);
		TArray<UProperty*> Referencers;

		INT Count = ArFind.GetCount(Referencers);
		if ( Count > 0 )
		{
			if ( Object->IsIn(this) )
			{
				if (OutInternalReferencers != NULL)
				{
					// manually allocate just one element - much slower but avoids slack which improves success rate on consoles
					OutInternalReferencers->Reserve(OutInternalReferencers->Num() + 1);
					new(*OutInternalReferencers) FReferencerInformation(Object, Count, Referencers);
				}
			}
			else
			{
				if (OutExternalReferencers != NULL)
				{
					// manually allocate just one element - much slower but avoids slack which improves success rate on consoles
					OutExternalReferencers->Reserve(OutExternalReferencers->Num() + 1);
					new(*OutExternalReferencers) FReferencerInformation(Object, Count, Referencers);
				}
			}
		}
	}
}

/**
 * Maps object flag to human-readable string.
 */
class FObjectFlag
{
public:
	EObjectFlags	ObjectFlag;
	const TCHAR*	FlagName;
	FObjectFlag(EObjectFlags InObjectFlag, const TCHAR* InFlagName)
		:	ObjectFlag( InObjectFlag )
		,	FlagName( InFlagName )
	{}
};

/**
 * Initializes the singleton list of object flags.
 */
static TArray<FObjectFlag> PrivateInitObjectFlagList()
{
	TArray<FObjectFlag> ObjectFlagList;
#ifdef	DECLARE_OBJECT_FLAG
#error DECLARE_OBJECT_FLAG already defined
#else
#define DECLARE_OBJECT_FLAG( ObjectFlag ) ObjectFlagList.AddItem( FObjectFlag( RF_##ObjectFlag, TEXT(#ObjectFlag) ) );
	DECLARE_OBJECT_FLAG( InSingularFunc )
	DECLARE_OBJECT_FLAG( StateChanged )
	DECLARE_OBJECT_FLAG( DebugPostLoad	)
	DECLARE_OBJECT_FLAG( DebugSerialize )
	DECLARE_OBJECT_FLAG( DebugBeginDestroyed )
	DECLARE_OBJECT_FLAG( DebugFinishDestroyed )
	DECLARE_OBJECT_FLAG( EdSelected )
	DECLARE_OBJECT_FLAG( ZombieComponent )
	DECLARE_OBJECT_FLAG( Protected )
	DECLARE_OBJECT_FLAG( ClassDefaultObject )
	DECLARE_OBJECT_FLAG( ArchetypeObject )
	DECLARE_OBJECT_FLAG( ForceTagExp )
	DECLARE_OBJECT_FLAG( TokenStreamAssembled )
	DECLARE_OBJECT_FLAG( MisalignedObject )
	DECLARE_OBJECT_FLAG( Transactional )
	DECLARE_OBJECT_FLAG( Unreachable )
	DECLARE_OBJECT_FLAG( Public	)
	DECLARE_OBJECT_FLAG( TagImp	)
	DECLARE_OBJECT_FLAG( TagExp )
	DECLARE_OBJECT_FLAG( Obsolete )
	DECLARE_OBJECT_FLAG( TagGarbage )
	DECLARE_OBJECT_FLAG( DisregardForGC )
	DECLARE_OBJECT_FLAG( PerObjectLocalized )
	DECLARE_OBJECT_FLAG( NeedLoad )
	DECLARE_OBJECT_FLAG( AsyncLoading )
	DECLARE_OBJECT_FLAG( Suppress )
	DECLARE_OBJECT_FLAG( InEndState )
	DECLARE_OBJECT_FLAG( Transient )
	DECLARE_OBJECT_FLAG( Cooked )
	DECLARE_OBJECT_FLAG( LoadForClient )
	DECLARE_OBJECT_FLAG( LoadForServer )
	DECLARE_OBJECT_FLAG( LoadForEdit	)
	DECLARE_OBJECT_FLAG( Standalone )
	DECLARE_OBJECT_FLAG( NotForClient )
	DECLARE_OBJECT_FLAG( NotForServer )
	DECLARE_OBJECT_FLAG( NotForEdit )
	DECLARE_OBJECT_FLAG( RootSet )
	DECLARE_OBJECT_FLAG( BeginDestroyed )
	DECLARE_OBJECT_FLAG( FinishDestroyed )
	DECLARE_OBJECT_FLAG( NeedPostLoad )
	DECLARE_OBJECT_FLAG( HasStack )
	DECLARE_OBJECT_FLAG( Native )
	DECLARE_OBJECT_FLAG( Marked )
	DECLARE_OBJECT_FLAG( ErrorShutdown )
	DECLARE_OBJECT_FLAG( PendingKill	)
#undef DECLARE_OBJECT_FLAG
#endif
	return ObjectFlagList;
}
/**
 * Dumps object flags from the selected objects to debugf.
 */
static void PrivateDumpObjectFlags(UObject* Object, FOutputDevice& Ar)
{
	static TArray<FObjectFlag> SObjectFlagList = PrivateInitObjectFlagList();

	if ( Object )
	{
		FString Buf( FString::Printf( TEXT("%s:\t"), *Object->GetFullName() ) );
		for ( INT FlagIndex = 0 ; FlagIndex < SObjectFlagList.Num() ; ++FlagIndex )
		{
			const FObjectFlag& CurFlag = SObjectFlagList( FlagIndex );
			if ( Object->HasAnyFlags( CurFlag.ObjectFlag ) )
			{
				Buf += FString::Printf( TEXT("%s "), CurFlag.FlagName );
			}
		}
		Ar.Logf( TEXT("%s"), *Buf );
	}
}

/**
 * Recursively visits all object properties and dumps object flags.
 */
static void PrivateRecursiveDumpFlags(UStruct* Struct, BYTE* Data, FOutputDevice& Ar)
{
	for( TFieldIterator<UProperty> It(Struct); It; ++It )
	{
		if ( It->GetOwnerClass()->GetPropertiesSize() != sizeof(UObject) )
		{
			for( INT i=0; i<It->ArrayDim; i++ )
			{
				BYTE* Value = Data + It->Offset + i*It->ElementSize;
				if( Cast<UObjectProperty>(*It) )
				{
					UObject*& Obj = *(UObject**)Value;
					PrivateDumpObjectFlags( Obj, Ar );
				}
				else if( Cast<UStructProperty>(*It, CLASS_IsAUStructProperty) )
				{
					PrivateRecursiveDumpFlags( ((UStructProperty*)*It)->Struct, Value, Ar );
				}
			}
		}
	}
}

/** 
 * Performs the work for "SET" and "SETNOPEC".
 *
 * @param	Str						reset of console command arguments
 * @param	Ar						output device to use for logging
 * @param	bNotifyObjectOfChange	whether to notify the object about to be changed via Pre/PostEditChange
 */
static void PerformSetCommand( const TCHAR* Str, FOutputDevice& Ar, UBOOL bNotifyObjectOfChange )
{
	// Set a class default variable.
	TCHAR ObjectName[256], PropertyName[256];
	if (ParseToken(Str, ObjectName, ARRAY_COUNT(ObjectName), TRUE) && ParseToken(Str, PropertyName, ARRAY_COUNT(PropertyName), TRUE))
	{
		UClass* Class = FindObject<UClass>(ANY_PACKAGE, ObjectName);
		if (Class != NULL)
		{
			UProperty* Property = FindField<UProperty>(Class, PropertyName);
			if (Property != NULL)
			{
				while (*Str == ' ')
				{
					Str++;
				}
				UObject::GlobalSetProperty(Str, Class, Property, Property->Offset, bNotifyObjectOfChange);
			}
			else
			{
				Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized property %s on class %s"), PropertyName, ObjectName);
			}
		}
		else
		{
			UObject* Object = FindObject<UObject>(ANY_PACKAGE, ObjectName);
			if (Object != NULL)
			{
				UProperty* Property = FindField<UProperty>(Object->GetClass(), PropertyName);
				if (Property != NULL)
				{
					while (*Str == ' ')
					{
						Str++;
					}

					if (!Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange)
					{
						Object->PreEditChange(Property);
					}
					Property->ImportText(Str, (BYTE*)Object + Property->Offset, PPF_Localized, Object);
					if (!Object->HasAnyFlags(RF_ClassDefaultObject) && bNotifyObjectOfChange)
					{
						FPropertyChangedEvent PropertyEvent(Property);
						Object->PostEditChangeProperty(PropertyEvent);
					}
				}
			}
			else
			{
				Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized class or object %s"), ObjectName);
			}
		}
	}
	else 
	{
		Ar.Logf(NAME_ExecWarning, TEXT("Unexpected input; format is 'set [class or object name] [property name] [value]"));
	}
}

extern void FlushRenderingCommands();

IMPLEMENT_COMPARE_CONSTPOINTER(UObject,SortObjectByClassName,{	return A->GetClass()->GetName() > B->GetClass()->GetName() ? 1 : -1; })

/** Helper structure for property listing console command */
struct FListPropsWildcardPiece
{
	FString Str;
	UBOOL bMultiChar;
	FListPropsWildcardPiece(const FString& InStr, UBOOL bInMultiChar)
		: Str(InStr), bMultiChar(bInMultiChar)
	{}
};

UBOOL UObject::StaticExec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR *Str = Cmd;

	if( ParseCommand(&Str,TEXT("SETTRACKINGBASELINE")))
	{
		FlushAsyncLoading();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,TRUE);
		FlushRenderingCommands();

		return GMalloc->Exec(Cmd,Ar);
	}
#if !SHIPPING_PC_GAME
	else if( ParseCommand(&Str,TEXT("DUMPNATIVES")) )
	{
#if _MSC_VER
		// ISO C++ forbids taking the address of a bound member function to form a pointer to member function.  Say `&UObject::execUndefined'.
		for( INT i=0; i<EX_Max; i++ )
			if( GNatives[i] == &execUndefined )
				debugf( TEXT("Native index %i is available"), i );
#endif
		return TRUE;
	}
#endif
	else if( ParseCommand(&Str,TEXT("GET")) )
	{
		// Get a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				FString	Temp;
				if( Class->GetDefaultsCount() )
				{
					Property->ExportText( 0, Temp, Class->GetDefaults(), Class->GetDefaults(), Class, PPF_Localized|PPF_IncludeTransient );
				}
				Ar.Log( *Temp );
			}
			else
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
			}
		}
		else
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("LISTPROPS")))
	{
		// list all properties of the specified class that match the specified wildcard string
		TCHAR ClassName[256];
		UClass* Class;
		FString PropWildcard;

		if ( ParseToken(Str, ClassName, ARRAY_COUNT(ClassName), 1) &&
			(Class = FindObject<UClass>(ANY_PACKAGE, ClassName)) != NULL &&
			ParseToken(Str, PropWildcard, TRUE) )
		{
			// split up the search string by wildcard symbols
			TArray<FListPropsWildcardPiece> WildcardPieces;
			UBOOL bFound;
			do
			{
				bFound = FALSE;
				INT AsteriskPos = PropWildcard.InStr(TEXT("*"));
				INT QuestionPos = PropWildcard.InStr(TEXT("?"));
				if (AsteriskPos != INDEX_NONE || QuestionPos != INDEX_NONE)
				{
					if (AsteriskPos != INDEX_NONE && (QuestionPos == INDEX_NONE || QuestionPos > AsteriskPos))
					{
						new(WildcardPieces) FListPropsWildcardPiece(PropWildcard.Left(AsteriskPos), TRUE);
						PropWildcard = PropWildcard.Right(PropWildcard.Len() - AsteriskPos - 1);
						bFound = TRUE;
					}
					else if (QuestionPos != INDEX_NONE)
					{
						new(WildcardPieces) FListPropsWildcardPiece(PropWildcard.Left(QuestionPos), FALSE);
						PropWildcard = PropWildcard.Right(PropWildcard.Len() - QuestionPos - 1);
						bFound = TRUE;
					}
				}
			} while (bFound);
			UBOOL bEndedInConstant = (PropWildcard.Len() > 0);
			if (bEndedInConstant)
			{
				new(WildcardPieces) FListPropsWildcardPiece(PropWildcard, FALSE);
			}

			// search for matches
			INT Count = 0;
			for (TFieldIterator<UProperty> It(Class); It; ++It)
			{
				FString Match = It->GetName();
				UBOOL bResult = TRUE;
				for (INT i = 0; i < WildcardPieces.Num(); i++)
				{
					if (WildcardPieces(i).Str.Len() > 0)
					{
						INT Pos = Match.InStr(WildcardPieces(i).Str, FALSE, TRUE);
						if (Pos == INDEX_NONE || (i == 0 && Pos != 0))
						{
							bResult = FALSE;
							break;
						}
						else if (i > 0 && !WildcardPieces(i - 1).bMultiChar && Pos != 1)
						{
							bResult = FALSE;
							break;
						}

						Match = Match.Right(Match.Len() - Pos - WildcardPieces(i).Str.Len());
					}
				}
				if (bResult)
				{
					// validate ending wildcard, if any
					if (bEndedInConstant)
					{
						bResult = (Match.Len() == 0);
					}
					else if (!WildcardPieces.Last().bMultiChar)
					{
						bResult = (Match.Len() == 1);
					}

					if (bResult)
					{
						FString ExtraInfo;
						if (It->GetClass()->ClassCastFlags & CASTCLASS_UStructProperty)
						{
							ExtraInfo = *Cast<UStructProperty>(*It)->Struct->GetName();
						}
						else if (It->GetClass()->ClassCastFlags & CASTCLASS_UClassProperty)
						{
							ExtraInfo = FString::Printf(TEXT("class<%s>"), *Cast<UClassProperty>(*It)->MetaClass->GetName());
						}
						else if (It->GetClass()->ClassCastFlags & CASTCLASS_UObjectProperty)
						{
							ExtraInfo = *Cast<UObjectProperty>(*It)->PropertyClass->GetName();
						}
						else
						{
							ExtraInfo = It->GetClass()->GetName();
						}
						Ar.Logf(TEXT("%i) %s (%s)"), Count, *It->GetName(), *ExtraInfo);
						Count++;
					}
				}
			}
			if (Count == 0)
			{
				Ar.Logf(TEXT("- No matches"));
			}
		}
		else
		{
			Ar.Logf(NAME_ExecWarning, TEXT("ListProps: expected format is 'ListProps [class] [wildcard]"));
		}

		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("GETALL")))
	{
		// iterate through all objects of the specified type and return the value of the specified property for each object
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;

		if ( ParseToken(Str,ClassName,ARRAY_COUNT(ClassName), 1) &&
			(Class=FindObject<UClass>( ANY_PACKAGE, ClassName)) != NULL )
		{
			if ( ParseToken(Str,PropertyName,ARRAY_COUNT(PropertyName),1) )
			{
				if ( (Property=FindField<UProperty>(Class,PropertyName)) != NULL )
				{
					INT cnt = 0;
					UObject* LimitOuter = NULL;

					const UBOOL bHasOuter = appStrfind(Str,TEXT("OUTER=")) ? TRUE : FALSE;
					ParseObject<UObject>(Str,TEXT("OUTER="),LimitOuter,ANY_PACKAGE);

					// Check for a specific object name
					TCHAR ObjNameStr[256];
					FName ObjName(NAME_None);
					if (Parse(Str,TEXT("NAME="),ObjNameStr,ARRAY_COUNT(ObjNameStr)))
					{
						ObjName = FName(ObjNameStr);
					}
					
					if( bHasOuter && !LimitOuter )
					{
						Ar.Logf( NAME_ExecWarning, TEXT("Failed to find outer %s"), appStrfind(Str,TEXT("OUTER=")) );
					}
					else
					{
						UBOOL bShowDefaultObjects = ParseCommand(&Str,TEXT("SHOWDEFAULTS"));
						UBOOL bShowPendingKills = ParseCommand(&Str, TEXT("SHOWPENDINGKILLS"));
						UBOOL bShowDetailedInfo = ParseCommand(&Str, TEXT("DETAILED"));
						for ( FObjectIterator It; It; ++It )
						{
							UObject* CurrentObject = *It;

							if ( LimitOuter != NULL && !CurrentObject->IsIn(LimitOuter) )
							{
								continue;
							}

							if ( CurrentObject->IsTemplate(RF_ClassDefaultObject) && bShowDefaultObjects == FALSE )
							{
								continue;
							}

							if (ObjName != NAME_None && CurrentObject->GetFName() != ObjName)
							{
								continue;
							}

							if ( (bShowPendingKills || !CurrentObject->IsPendingKill()) && CurrentObject->IsA(Class) )
							{
								if ( Property->ArrayDim > 1 || Cast<UArrayProperty>(Property) != NULL )
								{
									BYTE* BaseData = (BYTE*)CurrentObject + Property->Offset;
									Ar.Logf(TEXT("%i) %s.%s ="), cnt++, *CurrentObject->GetFullName(), *Property->GetName());

									INT ElementCount = Property->ArrayDim;

									UProperty* ExportProperty = Property;
									if ( Property->ArrayDim == 1 )
									{
										FScriptArray* Array = (FScriptArray*)BaseData;
										UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);

										BaseData = (BYTE*)Array->GetData();
										ElementCount = Array->Num();
										ExportProperty = ArrayProp->Inner;
									}

									INT ElementSize = ExportProperty->ElementSize;
									for ( INT ArrayIndex = 0; ArrayIndex < ElementCount; ArrayIndex++ )
									{
										FString ResultStr;
										BYTE* ElementData = BaseData + ArrayIndex * ElementSize;
										ExportProperty->ExportTextItem(ResultStr, ElementData, NULL, CurrentObject, PPF_Localized|PPF_IncludeTransient);

										if (bShowDetailedInfo)
										{
											Ar.Logf(TEXT("\t%i: %s %s"), ArrayIndex, *ResultStr, *CurrentObject->GetDetailedInfo());
										}
										else
										{
											Ar.Logf(TEXT("\t%i: %s"), ArrayIndex, *ResultStr);
										}
									}
								}
								else
								{
									BYTE* BaseData = (BYTE*)CurrentObject;
									FString ResultStr;
									for (INT i = 0; i < Property->ArrayDim; i++)
									{
										Property->ExportText(i, ResultStr, BaseData, BaseData, CurrentObject, PPF_Localized|PPF_IncludeTransient);
									}

									if (bShowDetailedInfo)
									{
										Ar.Logf(TEXT("%i) %s.%s = %s %s"), cnt++, *CurrentObject->GetFullName(), *Property->GetName(), *ResultStr, *CurrentObject->GetDetailedInfo() );
									}
									else
									{
										Ar.Logf(TEXT("%i) %s.%s = %s"), cnt++, *CurrentObject->GetFullName(), *Property->GetName(), *ResultStr);
									}
								}
							}
						}
					}
				}
				else
				{
					Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
				}
			}
			else
			{
				Ar.Logf(NAME_ExecWarning, TEXT("No property specified"));
			}
		}
		else
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("GETALLSTATE")))
	{
		// iterate through all objects of the specified class and log the state they're in
		TCHAR ClassName[256];
		UClass* Class;

		if ( ParseToken(Str, ClassName, ARRAY_COUNT(ClassName), 1) &&
			(Class = FindObject<UClass>(ANY_PACKAGE, ClassName)) != NULL )
		{
			UBOOL bShowPendingKills = ParseCommand(&Str, TEXT("SHOWPENDINGKILLS"));
			INT cnt = 0;
			for (TObjectIterator<UObject> It; It; ++It)
			{
				if ((bShowPendingKills || !It->IsPendingKill()) && It->IsA(Class))
				{
					Ar.Logf( TEXT("%i) %s is in state %s"), cnt++, *It->GetFullName(),
							(It->GetStateFrame() && It->GetStateFrame()->StateNode) ?
								*It->GetStateFrame()->StateNode->GetName() :
								TEXT("None") );
				}
			}
		}
		else
		{
			Ar.Logf(NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName);
		}
		return TRUE;
	}
	else if (ParseCommand(&Str, TEXT("MATCHVALUE")))
	{
		// iterate through properties of all objects and match the specified text
		TCHAR ClassName[256];
		UClass* Class;

		if ( ParseToken(Str,ClassName,ARRAY_COUNT(ClassName), 1) &&
			(Class=FindObject<UClass>( ANY_PACKAGE, ClassName)) != NULL )
		{
			INT cnt = 0;

			while (*Str != 0 && appIsWhitespace(*Str))
			{
				Str++;
			}
			FString FindStr(Str);
			for (FObjectIterator It; It; ++It)
			{
				UObject* CurrentObject = *It;

				if (CurrentObject->IsA(Class))
				{
					for (TFieldIterator<UProperty> PropIt(CurrentObject->GetClass()); PropIt; ++PropIt)
					{
						BYTE* BaseData = (BYTE*)CurrentObject;
						FString ResultStr(FString::Printf(TEXT("%s.%s = "), *CurrentObject->GetFullName(), *PropIt->GetName()));
						for (INT i = 0; i < PropIt->ArrayDim; i++)
						{
							PropIt->ExportText(i, ResultStr, BaseData, BaseData, CurrentObject, PPF_Localized | PPF_IncludeTransient);
						}

						if (ResultStr.InStr(*FindStr, FALSE, TRUE) != INDEX_NONE)
						{
							Ar.Logf(TEXT("%i) %s"), cnt++, *ResultStr);
						}
					}
				}
			}
		}
		else
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		}
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		PerformSetCommand( Str, Ar, TRUE );
		return TRUE;
	}
	else if( ParseCommand(&Str,TEXT("SETNOPEC")) )
	{
		PerformSetCommand( Str, Ar, FALSE );
		return TRUE;
	}
	else if ( ParseCommand(&Str,TEXT("VERIFYCOMPONENTS")) )
	{
		for ( TObjectIterator<UComponent> It; It; ++It )
		{
			UComponent* Component = *It;
			UComponent* ComponentTemplate = Cast<UComponent>(Component->GetArchetype());
			UObject* Owner = Component->GetOuter();
			UObject* OwnerTemplate = Owner->GetArchetype();
			UObject* TemplateOwner = ComponentTemplate->GetOuter();
			if ( !ComponentTemplate->HasAnyFlags(RF_ClassDefaultObject) )
			{
				if ( TemplateOwner != OwnerTemplate )
				{

					FString RealArchetypeName;
					if ( Component->TemplateName != NAME_None )
					{
						UComponent* RealArchetype = Owner->GetArchetype()->FindComponent(Component->TemplateName);
						if ( RealArchetype != NULL )
						{
							RealArchetypeName = RealArchetype->GetFullName();
						}
						else
						{
							RealArchetypeName = FString::Printf(TEXT("NULL: no matching components found in Owner Archetype %s"), *Owner->GetArchetype()->GetFullName());
						}
					}
					else
					{
						RealArchetypeName = TEXT("NULL");
					}

					warnf(TEXT("Possible corrupted component: '%s'	Archetype: '%s'	TemplateName: '%s'	ResolvedArchetype: '%s'"),
						*Component->GetFullName(), 
						*ComponentTemplate->GetPathName(),
						*Component->TemplateName.ToString(),
						*RealArchetypeName
						);
				}
			}
		}

		return TRUE;
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( ParseCommand(&Str,TEXT("GARBAGE")) || ParseCommand(&Str,TEXT("GC")) )
		{
			// Purge unclaimed objects.
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MARK")) )
		{
			debugf( TEXT("Marking objects") );
			for( FObjectIterator It; It; ++It )
				It->SetFlags( RF_Marked );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("MARKCHECK")) )
		{
			debugf( TEXT("Unmarked objects:") );
			for( FObjectIterator It; It; ++It )
				if( !It->HasAnyFlags(RF_Marked) )
					debugf( TEXT("%s"), *It->GetFullName() );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("REFS")) )
		{
			UClass* Class;
			UObject* Object;
			// Don't require the class= part anymore
			if (ParseObject<UClass>(Str,TEXT("CLASS="),Class,ANY_PACKAGE) == FALSE)
			{
				// Set the class to "object"
				Class = UObject::StaticClass();
			}
			if (ParseObject(Str,TEXT("NAME="),Class,Object,ANY_PACKAGE))
			{
				FStringOutputDevice TempAr;
				Object->OutputReferencers(TempAr,TRUE);

				TArray<FString> Lines;
				TempAr.ParseIntoArray(&Lines, LINE_TERMINATOR, 0);
				for ( INT i = 0; i < Lines.Num(); i++ )
				{
					Ar.Log(*Lines(i));
				}
			}
			else
			{
				debugf(TEXT("Couldn't find object for class (%s)"),*Class->GetName());
			}
			return TRUE;
		}
		else if (ParseCommand(&Str, TEXT("SINGLEREF")))
		{
			UBOOL bListClass = FALSE;
			UClass* Class;
			UClass* ReferencerClass = NULL;
			FString ReferencerName;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), Class, ANY_PACKAGE) == FALSE)
			{
				Class = UObject::StaticClass();
				bListClass = TRUE;
			}
			if (ParseObject<UClass>(Str, TEXT("REFCLASS="), ReferencerClass, ANY_PACKAGE) == FALSE)
			{
				ReferencerClass = NULL;
			}
			TCHAR TempStr[1024];
			if (Parse(Str, TEXT("REFNAME="), TempStr, ARRAY_COUNT(TempStr)))
			{
				ReferencerName = TempStr;
			}

			for (TObjectIterator<UObject> It; It; ++It)
			{
				UObject* Object = *It;
				if ((Object->IsA(Class)) && (Object->IsTemplate() == FALSE) && (Object->HasAnyFlags(RF_ClassDefaultObject) == FALSE))
				{
					TArray<FReferencerInformation> OutExternalReferencers;
					Object->RetrieveReferencers(NULL, &OutExternalReferencers, TRUE);

					if (OutExternalReferencers.Num() == 1)
					{
						FReferencerInformation& Info = OutExternalReferencers(0);
						UObject* RefObj = Info.Referencer;
						if (RefObj)
						{
							UBOOL bDumpIt = TRUE;
							if (ReferencerName.Len() > 0)
							{
								if (RefObj->GetName() != ReferencerName)
								{
									bDumpIt = FALSE;
								}
							}
							if (ReferencerClass)
							{
								if (RefObj->IsA(ReferencerClass) == FALSE)
								{
									bDumpIt = FALSE;
								}
							}

							if (bDumpIt)
							{
								FArchiveCountMem Count(Object);

								// Get the 'old-style' resource size and the truer resource size
								UBOOL SavedExclusiveMode = GExclusiveResourceSizeMode;
								GExclusiveResourceSizeMode = FALSE;
								const SIZE_T ResourceSize = It->GetResourceSize();
								GExclusiveResourceSizeMode = TRUE;
								const SIZE_T TrueResourceSize = It->GetResourceSize();
								GExclusiveResourceSizeMode = SavedExclusiveMode;

								if (bListClass)
								{
									Ar.Logf(TEXT("%64s: %64s, %8d,%8d,%8d,%8d"), *(Object->GetClass()->GetName()), *(Object->GetPathName()),
										Count.GetNum(), Count.GetMax(), ResourceSize, TrueResourceSize);
								}
								else
								{
									Ar.Logf(TEXT("%64s, %8d,%8d,%8d,%8d"), *(Object->GetPathName()),
										Count.GetNum(), Count.GetMax(), ResourceSize, TrueResourceSize);
								}
								Ar.Logf(TEXT("\t%s"), *(RefObj->GetPathName()));
							}
						}
					}
				}
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("HASH")) )
		{
			// Hash info.
			FName::DisplayHash( Ar );
			INT ObjCount=0, HashCount=0;
			for( FObjectIterator It; It; ++It )
				ObjCount++;
			for( INT i=0; i<ARRAY_COUNT(GObjHash); i++ )
			{
				INT c=0;
				for( UObject* Hash=GObjHash[i]; Hash; Hash=Hash->HashNext )
					c++;
				if( c )
					HashCount++;
				//debugf( "%i: %i", i, c );
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("HASHOUTER")) )
		{
			INT SlotsInUse = 0;
			INT TotalCollisions = 0;
			INT MinCollisions = 0;
			INT MaxCollisions = 0;
			INT MaxBin = 0;
			// Work through each slot and figure out how many collisions
			for (DWORD CurrSlot = 0; CurrSlot < OBJECT_HASH_BINS; CurrSlot++)
			{
				INT Collisions = 0;
				// Get the first item in the slot
				UObject* Hash = GObjHashOuter[CurrSlot];
				// Determine if this slot is being used
				if (Hash != NULL)
				{
					// This slot is in use
					SlotsInUse++;
					// Now count how many hash collisions there are
					while (Hash)
					{
						Hash = Hash->HashOuterNext;
						// If there is another item in the chain
						if (Hash != NULL)
						{
							// Then increment the number of collisions
							Collisions++;
						}
					}
					// Keep the global stats
					TotalCollisions += Collisions;
					if (Collisions > MaxCollisions)
					{
						MaxBin = CurrSlot;
					}
					MaxCollisions = Max<INT>(Collisions,MaxCollisions);
					MinCollisions = Min<INT>(Collisions,MinCollisions);
					// Now log the output
					Ar.Logf(TEXT("\tSlot %d has %d collisions"),CurrSlot,Collisions);
				}
			}
			// Dump how many slots were in use
			Ar.Logf(TEXT("Slots in use %d"),SlotsInUse);
			// Now dump how efficient the hash is
			Ar.Logf(TEXT("Collision Stats: Best Case (%d), Average Case (%d), Worst Case (%d)"),
				MinCollisions,appFloor(((FLOAT)TotalCollisions / (FLOAT)OBJECT_HASH_BINS)),
				MaxCollisions);
			// Dump the first 30 objects in the worst bin for inspection
			INT Count = 0;
			UObject* Hash = GObjHashOuter[MaxBin];
			while (Hash != NULL && Count < 30)
			{
				Ar.Logf(TEXT("Object is %s (%s)"),*Hash->GetName(),*Hash->GetFullName());
				Hash = Hash->HashOuterNext;
				Count++;
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("HASHOBJ")) )
		{
			INT SlotsInUse = 0;
			INT TotalCollisions = 0;
			INT MinCollisions = 0;
			INT MaxCollisions = 0;
			INT MaxBin = 0;
			// Work through each slot and figure out how many collisions
			for (DWORD CurrSlot = 0; CurrSlot < OBJECT_HASH_BINS; CurrSlot++)
			{
				INT Collisions = 0;
				// Get the first item in the slot
				UObject* Hash = GObjHash[CurrSlot];
				// Determine if this slot is being used
				if (Hash != NULL)
				{
					// This slot is in use
					SlotsInUse++;
					// Now count how many hash collisions there are
					while (Hash)
					{
						Hash = Hash->HashNext;
						// If there is another item in the chain
						if (Hash != NULL)
						{
							// Then increment the number of collisions
							Collisions++;
						}
					}
					// Keep the global stats
					TotalCollisions += Collisions;
					if (Collisions > MaxCollisions)
					{
						MaxBin = CurrSlot;
					}
					MaxCollisions = Max<INT>(Collisions,MaxCollisions);
					MinCollisions = Min<INT>(Collisions,MinCollisions);
					// Now log the output
					Ar.Logf(TEXT("\tSlot %d has %d collisions"),CurrSlot,Collisions);
				}
			}
			// Dump how many slots were in use
			Ar.Logf(TEXT("Slots in use %d"),SlotsInUse);
			// Now dump how efficient the hash is
			Ar.Logf(TEXT("Collision Stats: Best Case (%d), Average Case (%d), Worst Case (%d)"),
				MinCollisions,appFloor(((FLOAT)TotalCollisions / (FLOAT)OBJECT_HASH_BINS)),
				MaxCollisions);
			// Dump the first 30 objects in the worst bin for inspection
			INT Count = 0;
			UObject* Hash = GObjHash[MaxBin];
			while (Hash != NULL && Count < 30)
			{
				Ar.Logf(TEXT("Object is %s (%s)"),*Hash->GetName(),*Hash->GetFullName());
				Hash = Hash->HashNext;
				Count++;
			}
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("CLASSES")) )
		{
			ShowClasses( StaticClass(), Ar, 0 );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("INTRINSICCLASSES")) )
		{
			ShowIntrinsicClasses(Ar);
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("DEPENDENCIES")) )
		{
			UPackage* Pkg;
			if( ParseObject<UPackage>(Str,TEXT("PACKAGE="),Pkg,NULL) )
			{
				TArray<UObject*> Exclude;

				// check if we want to ignore references from any packages
				for( INT i=0; i<16; i++ )
				{
					TCHAR Temp[MAX_SPRINTF]=TEXT("");
					appSprintf( Temp, TEXT("EXCLUDE%i="), i );
					FName F;
					if( Parse(Str,Temp,F) )
						Exclude.AddItem( CreatePackage(NULL,*F.ToString()) );
				}
				Ar.Logf( TEXT("Dependencies of %s:"), *Pkg->GetPathName() );

				UBOOL Dummy=0;

				// Should we recurse into inner packages?
				UBOOL bRecurse = ParseUBOOL(Str, TEXT("RECURSE"), Dummy);

				// Iterate through the object list
				for( FObjectIterator It; It; ++It )
				{
					// if this object is within the package specified, serialize the object
					// into a specialized archive which logs object names encountered during
					// serialization -- rjp
					if ( It->IsIn(Pkg) )
					{
						if ( It->GetOuter() == Pkg )
						{
							FArchiveShowReferences ArShowReferences( Ar, Pkg, *It, Exclude );
						}
						else if ( bRecurse )
						{
							// Two options -
							// a) this object is a function or something (which we don't care about)
							// b) this object is inside a group inside the specified package (which we do care about)
							UObject* CurrentObject = *It;
							UObject* CurrentOuter = It->GetOuter();
							while ( CurrentObject && CurrentOuter )
							{
								// this object is a UPackage (a group inside a package)
								// abort
								if ( CurrentObject->GetClass() == UPackage::StaticClass() )
									break;

								// see if this object's outer is a UPackage
								if ( CurrentOuter->GetClass() == UPackage::StaticClass() )
								{
									// if this object's outer is our original package, the original object (It)
									// wasn't inside a group, it just wasn't at the base level of the package
									// (its Outer wasn't the Pkg, it was something else e.g. a function, state, etc.)
									/// ....just skip it
									if ( CurrentOuter == Pkg )
										break;

									// otherwise, we've successfully found an object that was in the package we
									// were searching, but would have been hidden within a group - let's log it
									FArchiveShowReferences ArShowReferences( Ar, CurrentOuter, CurrentObject, Exclude );
									break;
								}

								CurrentObject = CurrentOuter;
								CurrentOuter = CurrentObject->GetOuter();
							}
						}
					}
				}
			}
			else
				debugf(TEXT("Package wasn't found."));
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("BULK")) )
		{
			FUntypedBulkData::DumpBulkDataUsage( Ar );
			return TRUE;
		}
		else if( ParseCommand(&Str,TEXT("LISTCONTENTREFS")) )
		{
			UClass*	Class		= NULL;
			UClass*	ListClass	= NULL;
			ParseObject<UClass>(Str, TEXT("CLASS="		), Class,		ANY_PACKAGE );
			ParseObject<UClass>(Str, TEXT("LISTCLASS="  ), ListClass,	ANY_PACKAGE );
		
			if( Class )
			{
				/** Helper class for only finding object references we "care" about. See operator << for details. */
				struct FArchiveListRefs : public FArchive
				{
					/** Set of objects ex and implicitly referenced by root based on criteria in << operator. */
					TSet<UObject*> ReferencedObjects;
					
					/** 
					 * Constructor, performing serialization of root object.
					 */
					FArchiveListRefs( UObject* InRootObject )
					{
						ArIsObjectReferenceCollector = TRUE;
						RootObject = InRootObject;
						RootObject->Serialize( *this );
					}

				private:
					/** Src/ root object to serialize. */
					UObject* RootObject;

					// The serialize operator is private as we don't support changing RootObject. */
					FArchive& operator<<( UObject*& Object )
					{
						if ( Object != NULL )
						{
							// Avoid serializing twice.
							if( ReferencedObjects.Find( Object ) == NULL )
							{
								ReferencedObjects.Add( Object );

								// Recurse if we're in the same package.
								if( RootObject->GetOutermost() == Object->GetOutermost() 
								// Or if package doesn't contain script.
								||	!(Object->GetOutermost()->PackageFlags & PKG_ContainsScript) )
								{
									// Serialize object. We don't want to use the << operator here as it would call 
									// this function again instead of serializing members.
									Object->Serialize( *this );
								}
							}
						}							
						return *this;
					}
				};

				// Create list of object references.
				FArchiveListRefs ListRefsAr(Class);

				// Give a choice of whether we want sorted list in more human read-able format or whether we want to list in Excel.
				UBOOL bShouldListAsCSV = ParseParam( Str, TEXT("CSV") );

				// If specified only lists objects not residing in script packages.
				UBOOL bShouldOnlyListContent = !ParseParam( Str, TEXT("LISTSCRIPTREFS") );

				// Sort refs by class name (un-qualified name).
				ListRefsAr.ReferencedObjects.Sort<COMPARE_CONSTPOINTER_CLASS(UObject,SortObjectByClassName)>();
				
				if( bShouldListAsCSV )
				{
					debugf(TEXT(",Class,Object"));
				}
				else
				{
					debugf(TEXT("Dumping references for %s"),*Class->GetFullName());
				}

				// Iterate over references and dump them to log. Either in CSV format or sorted by class.
				for( TSet<UObject*>::TConstIterator It(ListRefsAr.ReferencedObjects); It; ++It ) 
				{
					UObject* ObjectReference = *It;
					// Only list certain class if specified.
					if( (!ListClass || ObjectReference->GetClass() == ListClass)
					// Only list non-script objects if specified.
					&&	(!bShouldOnlyListContent || !(ObjectReference->GetOutermost()->PackageFlags & PKG_ContainsScript))
					// Exclude the transient package.
					&&	ObjectReference->GetOutermost() != UObject::GetTransientPackage() )
					{
						if( bShouldListAsCSV )
						{
							debugf(TEXT(",%s,%s"),*ObjectReference->GetClass()->GetPathName(),*ObjectReference->GetPathName());
						}
						else
						{
							debugf(TEXT("   %s"),*ObjectReference->GetFullName());
						}
					}
				}
			}
		}
		else if( ParseCommand(&Str,TEXT("LIST")) )
		{
			UBOOL SavedExclusiveResourceMode = GExclusiveResourceSizeMode;

			FString CmdLineOut = FString::Printf(TEXT("Obj List: %s"), Cmd);
			Ar.Log( *CmdLineOut );
			Ar.Log( TEXT("Objects:") );
			Ar.Log( TEXT("") );

			UClass*   CheckType     = NULL;
			UClass*   MetaClass		= NULL;
			const UBOOL bExportToFile = ParseParam(Str,TEXT("FILE"));

			// allow checking for any Outer, not just a UPackage
			UObject* CheckOuter = NULL;
			UPackage* InsidePackage = NULL;
			UObject* InsideObject = NULL;
			ParseObject<UClass>(Str, TEXT("CLASS="  ), CheckType, ANY_PACKAGE );
			ParseObject<UObject>(Str, TEXT("OUTER="), CheckOuter, ANY_PACKAGE);

			ParseObject<UPackage>(Str, TEXT("PACKAGE="), InsidePackage, NULL);
			if ( InsidePackage == NULL )
			{
				ParseObject<UObject>( Str, TEXT("INSIDE=" ), InsideObject, NULL );
			}
			INT Depth = -1;
			Parse(Str, TEXT("DEPTH="), Depth);

			TArray<FItem> List;
			TArray<FSubItem> Objects;
			FItem Total;

			// support specifying metaclasses when listing class objects
			if ( CheckType && CheckType == UClass::StaticClass() )
			{
				ParseObject<UClass>  ( Str, TEXT("TYPE="   ), MetaClass,     ANY_PACKAGE );
			}

			// if we specified a parameter in the command, but no objects of that parameter were found,
			// don't be dumb and list all objects
			if ((CheckType		||	!appStrfind(Str,TEXT("CLASS=")))
			&&	(MetaClass		||	!appStrfind(Str,TEXT("TYPE=")))
			&&	(CheckOuter		||	!appStrfind(Str,TEXT("OUTER=")))
			&&	(InsidePackage	||	!appStrfind(Str,TEXT("PACKAGE="))) 
			&&	(InsideObject	||	!appStrfind(Str,TEXT("INSIDE="))))
			{
				const UBOOL bTrackDetailedObjectInfo		= (CheckType != NULL && CheckType != UObject::StaticClass()) || CheckOuter != NULL || InsideObject != NULL || InsidePackage != NULL;
				const UBOOL bOnlyListGCObjects				= ParseParam( Str, TEXT("GCONLY") );
				const UBOOL bShouldIncludeDefaultObjects	= ParseParam( Str, TEXT("INCLUDEDEFAULTS") );
				const UBOOL bOnlyListDefaultObjects			= ParseParam( Str, TEXT("DEFAULTSONLY") );
				const UBOOL bShowDetailedObjectInfo			= ParseParam( Str, TEXT("NODETAILEDINFO") ) == FALSE && bTrackDetailedObjectInfo;

				for( FObjectIterator It; It; ++It )
				{
					if (It->IsTemplate(RF_ClassDefaultObject))
					{
						if( !bShouldIncludeDefaultObjects )
						{
							continue;
						}
					}
					else if( bOnlyListDefaultObjects )
					{
						continue;
					}

					if ( bOnlyListGCObjects && It->HasAnyFlags( RF_DisregardForGC ) )
					{
						continue;
					}

					if ( CheckType && !It->IsA(CheckType) )
					{
						continue;
					}

					if ( CheckOuter && It->GetOuter() != CheckOuter )
					{
						continue;
					}

					if ( InsidePackage && !It->IsIn(InsidePackage) )
					{
						continue;
					}

					if ( InsideObject && !It->IsIn(InsideObject) )
					{
						continue;
					}

					if ( MetaClass )
					{
						UClass* ClassObj = Cast<UClass>(*It);
						if ( ClassObj && !ClassObj->IsChildOf(MetaClass) )
						{
							continue;
						}
					}

					FArchiveCountMem Count( *It );

					// Get the 'old-style' resource size and the truer resource size
					GExclusiveResourceSizeMode = FALSE;
					const SIZE_T ResourceSize = It->GetResourceSize();
					GExclusiveResourceSizeMode = TRUE;
					const SIZE_T TrueResourceSize = It->GetResourceSize();

					INT i;

					// which class are we going to file this object under? by default, it's class
					UClass* ClassToUse = It->GetClass();
					// if we specified a depth to use, then put this object into the class Depth away from Object
					if (Depth != -1)
					{
						UClass* Travel = ClassToUse;
						// go up the class hierarchy chain, using a trail pointer Depth away
						for (INT Up = 0; Up < Depth && Travel != UObject::StaticClass(); Up++)
						{
							Travel = (UClass*)Travel->SuperStruct;
						}
						// when travel is a UObject, ClassToUse will be pointing to a class Depth away
						while (Travel != UObject::StaticClass())
						{
							Travel = (UClass*)Travel->SuperStruct;
							ClassToUse = (UClass*)ClassToUse->SuperStruct;
						}
					}

					for( i=0; i<List.Num(); i++ )
					{
						if( List(i).Class == ClassToUse )
						{
							break;
						}
					}
					if( i==List.Num() )
					{
						i = List.AddItem(FItem( ClassToUse ));
					}
					
					if( bShowDetailedObjectInfo )
					{
						new(Objects)FSubItem( *It, Count.GetNum(), Count.GetMax(), ResourceSize, TrueResourceSize );
					}
					List(i).Add( Count, ResourceSize, TrueResourceSize );
					Total.Add( Count, ResourceSize, TrueResourceSize );
				}
			}

			bAlphaSort = ParseParam( Str, TEXT("ALPHASORT") );
			bCountSort = ParseParam( Str, TEXT("COUNTSORT") );

			if( Objects.Num() )
			{
				Sort<USE_COMPARE_CONSTREF(FSubItem,UnObj)>( &Objects(0), Objects.Num() );
#if USE_LS_SPEC_FOR_UNICODE
				Ar.Logf( TEXT("%100ls % 10ls % 10ls % 10ls % 10ls"), TEXT("Object"), TEXT("NumBytes"), TEXT("MaxBytes"), TEXT("ResKBytes"), TEXT("TrueResKBytes") );
#else
				Ar.Logf( TEXT("%100s % 10s % 10s % 10s % 10s"), TEXT("Object"), TEXT("NumBytes"), TEXT("MaxBytes"), TEXT("ResKBytes"), TEXT("TrueResKBytes") );
#endif
				for( INT ObjIndex=0; ObjIndex<Objects.Num(); ObjIndex++ )
				{
					const FSubItem& ObjItem = Objects(ObjIndex);

					///MSSTART DAN PRICE MICROSOFT Mar 12th, 2007 export object data to a file
					if(bExportToFile)
					{
						FString Path = TEXT(".\\ObjExport");
						const FString Filename = Path * ObjItem.Object->GetOutermost()->GetName() + TEXT(".") +  *ObjItem.Object->GetName() + TEXT(".t3d");
						Ar.Logf( TEXT("%s"),*Filename);
						UExporter::ExportToFile(ObjItem.Object, NULL, *Filename, 1, 0);
					}					
					//MSEND
#if USE_LS_SPEC_FOR_UNICODE
					Ar.Logf( TEXT("%100ls % 10i % 10i % 10i % 10i"), *ObjItem.Object->GetFullName(), ObjItem.Num, ObjItem.Max, ObjItem.Res / 1024, ObjItem.TrueRes / 1024 );
#else
					Ar.Logf( TEXT("%100s % 10i % 10i % 10i % 10i"), *ObjItem.Object->GetFullName(), ObjItem.Num, ObjItem.Max, ObjItem.Res / 1024, ObjItem.TrueRes / 1024 );
#endif
				}
				Ar.Log( TEXT("") );
			}
			if( List.Num() )
			{
				Sort<USE_COMPARE_CONSTREF(FItem,UnObj)>( &List(0), List.Num() );
#if USE_LS_SPEC_FOR_UNICODE
				Ar.Logf(TEXT(" %100ls % 6ls % 10ls % 10ls % 10ls % 10ls"), TEXT("Class"), TEXT("Count"), TEXT("NumBytes"), TEXT("MaxBytes"), TEXT("ResBytes"), TEXT("TrueResBytes") );
#else
				Ar.Logf(TEXT(" %100s % 6s % 10s % 10s % 10s % 10s"), TEXT("Class"), TEXT("Count"), TEXT("NumBytes"), TEXT("MaxBytes"), TEXT("ResBytes"), TEXT("TrueResBytes") );
#endif
				for( INT i=0; i<List.Num(); i++ )
				{
#if USE_LS_SPEC_FOR_UNICODE
					Ar.Logf(TEXT(" %100ls % 6i % 10iK % 10iK % 10iK % 10iK"), *List(i).Class->GetName(), List(i).Count, List(i).Num/1024, List(i).Max/1024, List(i).Res/1024, List(i).TrueRes/1024 );
#else
					Ar.Logf(TEXT(" %100s % 6i % 10iK % 10iK % 10iK % 10iK"), *List(i).Class->GetName(), List(i).Count, List(i).Num/1024, List(i).Max/1024, List(i).Res/1024, List(i).TrueRes/1024 );
#endif
				}
				Ar.Log( TEXT("") );
			}
			Ar.Logf( TEXT("%i Objects (%.3fM / %.3fM / %.3fM / %.3fM)"), Total.Count, (FLOAT)Total.Num/1024.0/1024.0, (FLOAT)Total.Max/1024.0/1024.0, (FLOAT)Total.Res/1024.0/1024.0, (FLOAT)Total.TrueRes/1024.0/1024.0 );

			GExclusiveResourceSizeMode = SavedExclusiveResourceMode;
			return TRUE;

		}
		else if( ParseCommand(&Str,TEXT("LINKERS")) )
		{
			Ar.Logf( TEXT("Linkers:") );
			for( INT i=0; i<GObjLoaders.Num(); i++ )
			{
				ULinkerLoad* Linker = GObjLoaders(i);
				INT NameSize = 0;
				for( INT j=0; j<Linker->NameMap.Num(); j++ )
				{
					if( Linker->NameMap(j) != NAME_None )
					{
						NameSize += FNameEntry::GetSize( *Linker->NameMap(j).ToString() );
					}
				}
				Ar.Logf
				(
					TEXT("%s (%s): Names=%i (%iK/%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Bulk=%i"),
					*Linker->Filename,
					*Linker->LinkerRoot->GetFullName(),
					Linker->NameMap.Num(),
					Linker->NameMap.Num() * sizeof(FName) / 1024,
					NameSize / 1024,
					Linker->ImportMap.Num(),
					Linker->ImportMap.Num() * sizeof(FObjectImport) / 1024,
					Linker->ExportMap.Num(),
					Linker->ExportMap.Num() * sizeof(FObjectExport) / 1024,
					Linker->Summary.Generations.Num(),
					Linker->BulkDataLoaders.Num()
				);
			}

			return TRUE;
		}
		else if ( ParseCommand(&Str,TEXT("FLAGS")) )
		{
			// Dump all object flags for objects rooted at the named object.
			TCHAR ObjectName[NAME_SIZE];
			UObject* Obj = NULL;
			if ( ParseToken(Str,ObjectName,ARRAY_COUNT(ObjectName), 1) )
			{
				Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
			}

			if ( Obj )
			{
				PrivateDumpObjectFlags( Obj, Ar );
				PrivateRecursiveDumpFlags( Obj->GetClass(), (BYTE*)Obj, Ar );
			}

			return TRUE;
		}
		else if ( ParseCommand(&Str,TEXT("COMPONENTS")) )
		{
			UObject* Obj=NULL;
			FString ObjectName;

			if ( ParseToken(Str,ObjectName,TRUE) )
			{
				Obj = FindObject<UObject>(ANY_PACKAGE,*ObjectName);

				if ( Obj != NULL )
				{
					Ar.Log(TEXT(""));
					Obj->DumpComponents();
					Ar.Log(TEXT(""));
				}
				else
				{
					Ar.Logf(TEXT("No objects found named '%s'"), *ObjectName);
				}
			}
			else
			{
				Ar.Logf(TEXT("Syntax: OBJ COMPONENTS <Name Of Object>"));
			}
			return TRUE;
		}
		else if ( ParseCommand(&Str,TEXT("DUMP")) )
		{
			// Dump all variable values for the specified object
			// supports specifying categories to hide or show
			// OBJ DUMP playercontroller0 hide="actor,object,lighting,movement"     OR
			// OBJ DUMP playercontroller0 show="playercontroller,controller"        OR
			// OBJ DUMP class=playercontroller name=playercontroller0 show=object OR
			// OBJ DUMP playercontroller0 recurse=true
			TCHAR ObjectName[1024];
			UObject* Obj = NULL;
			UClass* Cls = NULL;

			TArray<FString> HiddenCategories, ShowingCategories;

			if ( !ParseObject<UClass>( Str, TEXT("CLASS="), Cls, ANY_PACKAGE ) || !ParseObject(Str,TEXT("NAME="), Cls, Obj, ANY_PACKAGE) )
			{
				if ( ParseToken(Str,ObjectName,ARRAY_COUNT(ObjectName), 1) )
				{
					Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
				}
			}

			if ( Obj )
			{
				if ( Cast<UClass>(Obj) != NULL )
				{
					Obj = Cast<UClass>(Obj)->GetDefaultObject();
				}

				FString Value;

				Ar.Logf(TEXT(""));

				const UBOOL bRecurse = Parse(Str, TEXT("RECURSE=TRUE"), Value);
				Ar.Logf(TEXT("*** Property dump for object %s'%s' ***"), bRecurse ? TEXT("(Recursive) ") : TEXT(""), *Obj->GetFullName() );

				if ( bRecurse )
				{
					const FExportObjectInnerContext Context;
					ExportProperties( &Context, Ar, Obj->GetClass(), (BYTE*)Obj, 0, Obj->GetArchetype()->GetClass(), (BYTE*)Obj->GetArchetype(), Obj, PPF_Localized|PPF_IncludeTransient );
				}
				else
				{
#if !CONSOLE
					//@todo: add support to Parse() for specifying characters that should be ignored
					if ( Parse(Str, TEXT("HIDE="), Value/*, TEXT(",")*/) )
					{
						Value.ParseIntoArray(&HiddenCategories,TEXT(","),1);
					}
					else if ( Parse(Str, TEXT("SHOW="), Value/*, TEXT(",")*/) )
					{
						Value.ParseIntoArray(&ShowingCategories,TEXT(","),1);
					}
#endif
					UClass* LastOwnerClass = NULL;
					for ( TFieldIterator<UProperty> It(Obj->GetClass()); It; ++It )
					{
						Value.Empty();
#if !CONSOLE
						if ( HiddenCategories.Num() )
						{
							INT i;
							for ( i = 0; i < HiddenCategories.Num(); i++ )
							{
								if ( It->Category != NAME_None && HiddenCategories(i) == It->Category.ToString() )
								{
									break;
								}

								if ( HiddenCategories(i) == *It->GetOwnerClass()->GetName() )
								{
									break;
								}
							}

							if ( i < HiddenCategories.Num() )
							{
								continue;
							}
						}
						else if ( ShowingCategories.Num() )
						{
							INT i;
							for ( i = 0; i < ShowingCategories.Num(); i++ )
							{
								if ( It->Category != NAME_None && ShowingCategories(i) == It->Category.ToString() )
								{
									break;
								}

								if ( ShowingCategories(i) == *It->GetOwnerClass()->GetName() )
								{
									break;
								}
							}

							if ( i == ShowingCategories.Num() )
							{
								continue;
							}
						}
#endif
						if ( LastOwnerClass != It->GetOwnerClass() )
						{
							LastOwnerClass = It->GetOwnerClass();
							Ar.Logf(TEXT("=== %s properties ==="), *LastOwnerClass->GetName());
						}

						if ( It->ArrayDim > 1 )
						{
							for ( INT i = 0; i < It->ArrayDim; i++ )
							{
								Value.Empty();
								It->ExportText(i, Value, (BYTE*)Obj, (BYTE*)Obj, Obj, PPF_Localized|PPF_IncludeTransient);
								Ar.Logf(TEXT("  %s[%i]=%s"), *It->GetName(), i, *Value);
							}
						}
						else
						{
							UArrayProperty* ArrayProp = Cast<UArrayProperty>(*It);
							if ( ArrayProp != NULL )
							{
								FScriptArray* Array       = (FScriptArray*)((BYTE*)Obj + ArrayProp->Offset);
								const INT ElementSize = ArrayProp->Inner->ElementSize;
								for( INT i=0; i<Min(Array->Num(),100); i++ )
								{
									Value.Empty();
									BYTE* Data = (BYTE*)Array->GetData() + i * ElementSize;
									ArrayProp->Inner->ExportTextItem( Value, Data, Data, Obj, PPF_Localized|PPF_IncludeTransient );
									Ar.Logf(TEXT("  %s(%i)=%s"), *ArrayProp->GetName(), i, *Value);
								}

								if ( Array->Num() >= 100 )
								{
									Ar.Logf(TEXT("  ... %i more elements"), Array->Num() - 99);
								}
							}
							else
							{
								It->ExportText(0, Value, (BYTE*)Obj, (BYTE*)Obj, Obj, PPF_Localized|PPF_IncludeTransient);
								Ar.Logf(TEXT("  %s=%s"), *It->GetName(), *Value);
							}
						}
					}
				}

				TMap<FString,FString> NativePropertyValues;
				if ( Obj->GetNativePropertyValues(NativePropertyValues) )
				{
					INT LargestKey = 0;
					for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
					{
						LargestKey = Max(LargestKey, It.Key().Len());
					}

					Ar.Log(TEXT("=== Native properties ==="));
					for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
					{
						Ar.Logf(TEXT("  %s%s"), *It.Key().RightPad(LargestKey), *It.Value());
					}
				}
			}
			else
			{
				Ar.Logf(NAME_ExecWarning, TEXT("No objects found using command '%s'"), Cmd);
			}

			return TRUE;
		}
		else if (ParseCommand(&Str, TEXT("NET")))
		{
			TCHAR PackageName[1024];
			if (ParseToken(Str, PackageName, ARRAY_COUNT(PackageName), TRUE))
			{
				UPackage* Package = FindObject<UPackage>(NULL, PackageName, TRUE);
				if (Package != NULL)
				{
					if (Package->PackageFlags & PKG_ServerSideOnly)
					{
						Ar.Logf(NAME_ExecWarning, TEXT("OBJ NET: package '%s' has ServerSideOnly flag set"), PackageName);
					}
					else
					{
						INT NetObjects = Package->GetGenerationNetObjectCount().Last();
						INT LoadedNetObjects = Package->GetCurrentNumNetObjects();
						Ar.Logf(TEXT("Package '%s' has %i total net objects (%i loaded)"), PackageName, NetObjects, LoadedNetObjects);
						for (INT i = 0; i < NetObjects; i++)
						{
							UObject* Obj = Package->GetNetObjectAtIndex(i);
							if (Obj != NULL)
							{
								Ar.Logf(TEXT(" - %i: %s"), i, *Obj->GetFullName());
							}
						}
					}
				}
				else
				{
					Ar.Logf(NAME_ExecWarning, TEXT("OBJ NET: failed to find package '%s'"), PackageName);
				}
			}
			else
			{
				Ar.Logf(NAME_ExecWarning, TEXT("OBJ NET: missing package name"));
			}
			return TRUE;
		}
		else return FALSE;
	}
#endif
#if SUPPORT_NAME_FLAGS
	else if (ParseCommand(&Str,TEXT("SUPPRESS")))
	{
		FString EventNameString = ParseToken(Str,0);
		FName	EventName		= FName(*EventNameString);
		if( EventName == NAME_All )
		{
			// We don't want to bloat GSys->Suppress with the entire list of FNames so we only set the
			// suppression flag for all currently active names and leave GSys->Suppress be.
			INT MaxNames = FName::GetMaxNames();			
			for( INT NameIndex=0; NameIndex<MaxNames; NameIndex++ )
			{
				FNameEntry* NameEntry = FName::GetEntry(NameIndex);
				if( NameEntry )
				{
					NameEntry->SetFlags(RF_Suppress);
				}
			}
		}
		else if( EventName != NAME_None )
		{
			EventName.SetFlags(RF_Suppress);
			debugf(TEXT("Suppressed event %s"),*EventNameString);
			GSys->Suppress.AddUniqueItem( EventName );
		}
		return TRUE;
	}
	else if (ParseCommand(&Str,TEXT("UNSUPPRESS")))
	{
		FString	EventNameString = ParseToken(Str,0);
		FName	EventName		= FName(*EventNameString);
		if( EventName == NAME_All )
		{
			// Iterate over all names, clearing suppression flag.
			INT MaxNames = FName::GetMaxNames();			
			for( INT NameIndex=0; NameIndex<MaxNames; NameIndex++ )
			{
				FNameEntry* NameEntry = FName::GetEntry(NameIndex);
				if( NameEntry )
				{
					NameEntry->ClearFlags(RF_Suppress);
				}
			}
			// Empty serialized array of suppressed names.
			GSys->Suppress.Empty();
		}
		else if( EventName != NAME_None )
		{
			EventName.ClearFlags(RF_Suppress);
			GSys->Suppress.RemoveItem( EventName );
			debugf(TEXT("Unsuppressed event %s"),*EventNameString);
		}
		return TRUE;
	}
#endif
	else if ( ParseCommand(&Str, TEXT("STRUCTPERFDATA")) )
	{
#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
		if ( ParseCommand(&Str, TEXT("DUMP")) )
		{
			if ( GObjectSerializationPerfTracker != NULL )
			{
				Ar.Logf(NAME_PerfEvent, TEXT("Global Performance Data:"));
				GObjectSerializationPerfTracker->DumpPerformanceData(&Ar);
			}

			for( INT LinkerIdx = 0; LinkerIdx < GObjLoaders.Num(); LinkerIdx++ )
			{
				ULinkerLoad* Loader = GObjLoaders(LinkerIdx);
				if ( Loader != NULL && Loader->LinkerRoot != NULL && Loader->SerializationPerfTracker.HasPerformanceData() )
				{
					Ar.Logf(NAME_PerfEvent, TEXT("============================================================"));
					Ar.Logf(NAME_PerfEvent, TEXT("Performance data for %s:"), *Loader->Filename);
					Loader->SerializationPerfTracker.DumpPerformanceData(&Ar);
					Ar.Logf(NAME_PerfEvent, TEXT("============================================================"));
				}
			}

			if ( GClassSerializationPerfTracker != NULL )
			{
				Ar.Log(NAME_PerfEvent, TEXT("============================================================"));
				Ar.Log(NAME_PerfEvent, TEXT("Class Serialization Performance Data:"));
				GClassSerializationPerfTracker->DumpPerformanceData(&Ar);
				Ar.Log(NAME_PerfEvent, TEXT("============================================================"));
			}
		}
		else if ( ParseCommand(&Str,TEXT("RESET")) )
		{
			if ( GObjectSerializationPerfTracker != NULL )
			{
				Ar.Logf(NAME_PerfEvent, TEXT("Clearing Global Performance Data..."));
				GObjectSerializationPerfTracker->ClearEvents();
			}

			for( INT LinkerIdx = 0; LinkerIdx < GObjLoaders.Num(); LinkerIdx++ )
			{
				ULinkerLoad* Loader = GObjLoaders(LinkerIdx);
				if ( Loader != NULL && Loader->LinkerRoot != NULL && Loader->SerializationPerfTracker.HasPerformanceData() )
				{
					Ar.Logf(NAME_PerfEvent, TEXT("Clearing performance data for %s:"), *Loader->Filename);
					Loader->SerializationPerfTracker.ClearEvents();
				}
			}

			if ( GClassSerializationPerfTracker != NULL )
			{
				GClassSerializationPerfTracker->ClearEvents();
			}
		}
#else
		Ar.Logf(NAME_PerfEvent, TEXT("Serialization performance tracking not enabled - #define PERF_TRACK_SERIALIZATION_PERFORMANCE or LOOKING_FOR_PERF_ISSUES to 1 to enable"));
#endif

		return TRUE;
	}
	// For reloading config on a particular object
	else if( ParseCommand(&Str,TEXT("RELOADCONFIG")) ||
		ParseCommand(&Str,TEXT("RELOADCFG")))
	{
		TCHAR ClassName[256];
		// Determine the object/class name
		if (ParseToken(Str,ClassName,ARRAY_COUNT(ClassName),1))
		{
			// Try to find a corresponding class
			UClass* ClassToReload = FindObject<UClass>(ANY_PACKAGE,ClassName);
			if (ClassToReload)
			{
				ClassToReload->ReloadConfig();
			}
			else
			{
				// If the class is missing, search for an object with that name
				UObject* ObjectToReload = FindObject<UObject>(ANY_PACKAGE,ClassName);
				if (ObjectToReload)
				{
					ObjectToReload->ReloadConfig();
				}
			}
		}
		return TRUE;
	}
	// For reloading localization on a particular object
	else if( ParseCommand(&Str,TEXT("RELOADLOC")))
	{
		TCHAR ClassName[256];
		// Determine the object/class name
		if (ParseToken(Str,ClassName,ARRAY_COUNT(ClassName),1))
		{
			// Try to find a corresponding class
			UClass* ClassToReload = FindObject<UClass>(ANY_PACKAGE,ClassName);
			if (ClassToReload)
			{
				ClassToReload->ReloadLocalized();
			}
			else
			{
				// If the class is missing, search for an object with that name
				UObject* ObjectToReload = FindObject<UObject>(ANY_PACKAGE,ClassName);
				if (ObjectToReload)
				{
					ObjectToReload->ReloadLocalized();
				}
			}
		}
		return TRUE;
	}
#if USE_GAMEPLAY_PROFILER
	else if( FGameplayProfiler::Exec( Cmd, Ar ) )
	{
		return TRUE;
	}
#endif
	// Route to self registering exec handlers.
	else
	{
		// Iterate over all registered exec handles and route command.
		for( INT i=0; i<FSelfRegisteringExec::RegisteredExecs.Num(); i++ )
		{
			// Return is command was handled.
			if( FSelfRegisteringExec::RegisteredExecs(i)->Exec( Cmd, Ar ) )
			{
				return TRUE;
			}
		}
	}
	
	return FALSE; // Not executed
}

/*-----------------------------------------------------------------------------
   File loading.
-----------------------------------------------------------------------------*/

//
// Safe load error-handling.
//
void UObject::SafeLoadError( UObject* Outer, DWORD LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ... )
{
	// Variable arguments setup.
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	if( ParseParam( appCmdLine(), TEXT("TREATLOADWARNINGSASERRORS") ) == TRUE )
	{
		warnf( NAME_Error, TEXT("%s"), TempStr ); 
	}
	else
	{
		if( (LoadFlags & LOAD_Quiet) == 0 )
		{ 
			warnf( NAME_Warning, TEXT("%s"), TempStr ); 
		}

		if( (LoadFlags & LOAD_Throw) != 0 )
		{
			appThrowf( TEXT("%s"), Error   ); 
		}

		if( (LoadFlags & LOAD_NoWarn) == 0 && (LoadFlags&LOAD_Quiet) != 0 )
		{ 
			warnf( TEXT("%s"), TempStr ); 
		}
	}
}

//
// Find or create the linker for a package.
//
ULinkerLoad* UObject::GetPackageLinker
(
	UPackage*		InOuter,
	const TCHAR*	InFilename,
	DWORD			LoadFlags,
	UPackageMap*	Sandbox,
	FGuid*			CompatibleGuid
)
{

	// See if there is already a linker for this package.
	ULinkerLoad* Result = ULinkerLoad::FindExistingLinkerForPackage(InOuter);

	// Try to load the linker.
#if !EXCEPTIONS_DISABLED
	try
#endif
	{
		// See if the linker is already loaded.
		FString NewFilename;
		if( Result )
		{
			// Linker already found.
			NewFilename = TEXT("");
		}
		else
		if( !InFilename )
		{
			// Resolve filename from package name.
			if( !InOuter )
			{
#if EXCEPTIONS_DISABLED
				// try to recover from this instead of throwing, it seems recoverable just by doing this
				return NULL;
#else
				appThrowf( *LocalizeError(TEXT("PackageResolveFailed"),TEXT("Core")) );
#endif
			}

			// We don't have a filename, so we should first look up to see if the package name has a filename mapping
			const FName InOutersFName = InOuter->GetFName();

			FName *MappedName = PackageNameToFileMapping.Find(InOutersFName);
			const FName *FileToLookup = MappedName != NULL ? MappedName : &InOutersFName;

			if( !GPackageFileCache->FindPackageFile( *FileToLookup->ToString(), CompatibleGuid, NewFilename ) )
			{
				// See about looking in the dll.
				if( (LoadFlags & LOAD_AllowDll) && InOuter->IsA(UPackage::StaticClass()) && ((UPackage*)InOuter)->IsBound() )
				{
					return NULL;
				}
				appThrowf( LocalizeSecure(LocalizeError(TEXT("PackageNotFound"),TEXT("Core")), *InOuter->GetName(), 
					GSerializedPackageLinker ? *(GSerializedPackageLinker->Filename) : TEXT("NULL")) );
			}
		}
		else
		{
			// Verify that the file exists.
			if( !GPackageFileCache->FindPackageFile( InFilename, CompatibleGuid, NewFilename ) )
			{
#if EXCEPTIONS_DISABLED
				// try to recover from this instead of throwing, it seems recoverable just by doing this
				return NULL;
#else
				appThrowf( LocalizeSecure(LocalizeError(TEXT("FileNotFound"),TEXT("Core")), InFilename) );
#endif
			}

			// Resolve package name from filename.
			TCHAR Tmp[256], *T=Tmp;
			appStrncpy( Tmp, InFilename, ARRAY_COUNT(Tmp) );
			while( 1 )
			{
				if( appStrstr(T,PATH_SEPARATOR) )
				{
					T = appStrstr(T,PATH_SEPARATOR)+appStrlen(PATH_SEPARATOR);
				}
				else if( appStrstr(T,TEXT("/")) )
				{
					T = appStrstr(T,TEXT("/"))+1;
				}
				else if( appStrstr(T,TEXT(":")) )
				{
					T = appStrstr(T,TEXT(":"))+1;
				}
				else
				{
					break;
				}
			}
			if( appStrstr(T,TEXT(".")) )
			{
				*appStrstr(T,TEXT(".")) = 0;
			}
			//@script patcher (LOAD_RemappedPackage)
			UPackage* FilenamePkg = CreatePackage( NULL, T, (LoadFlags&LOAD_RemappedPackage) != 0 );

			// If no package specified, use package from file.
			if( InOuter==NULL )
			{
				if( !FilenamePkg )
				{
					appThrowf( LocalizeSecure(LocalizeError(TEXT("FilenameToPackage"),TEXT("Core")), InFilename) );
				}
				InOuter = FilenamePkg;
				for( INT i=0; i<GObjLoaders.Num() && !Result; i++ )
				{
					if( GetLoader(i)->LinkerRoot == InOuter )
					{
						Result = GetLoader(i);
					}
				}
			}
			else if( InOuter != FilenamePkg )//!!should be tested and validated in new UnrealEd
			{
				// Loading a new file into an existing package, so reset the loader.
				debugf( TEXT("New File, Existing Package (%s, %s)"), *InOuter->GetFullName(), *FilenamePkg->GetFullName() );
				ResetLoaders( InOuter );
			}
		}

		// Make sure the package is accessible in the sandbox.
		if( Sandbox && !Sandbox->SupportsPackage(InOuter) )
		{
			appThrowf( LocalizeSecure(LocalizeError(TEXT("Sandbox"),TEXT("Core")), *InOuter->GetName()) );
		}

		// Create new linker.
		if( !Result )
		{
			//@script patcher: moved from top of function
			check(GObjBeginLoadCount);

			// we will already have found the filename above
			check(NewFilename.Len() > 0);
			Result = ULinkerLoad::CreateLinker( InOuter, *NewFilename, LoadFlags );
		}

		// Verify compatibility.
		if( CompatibleGuid && Result->Summary.Guid!=*CompatibleGuid )
		{
			// This should never fire, because FindPackageFile should never return an incompatible file
			appThrowf( LocalizeSecure(LocalizeError(TEXT("PackageVersion"),TEXT("Core")), *InOuter->GetName()) );
		}
	}
#if !EXCEPTIONS_DISABLED
	catch( const TCHAR* Error )
	{
		// If we're in the editor (and not running from UCC) we don't want this to be a fatal error.
		if( GIsEditor && !GIsUCC )
		{
			FString Filename = InFilename ? InFilename : InOuter ? InOuter->GetPathName() : TEXT("NULL");

			// if we don't want to be warned, skip the load warning
			if (!(LoadFlags & LOAD_NoWarn))
			{
				// If the filename is already in the error, no need to display it twice
				if (appStrstr(Error, *Filename))
				{
					EdLoadErrorf(FEdLoadError::TYPE_FILE, Error);
				}
				else
				{
					// If the error doesn't contain the file name, tack it on the end. This might be something like "..\Content\Foo.upk (Out of Memory)"
					EdLoadErrorf(FEdLoadError::TYPE_FILE, TEXT("%s (%s)"), *Filename, Error);
				}
				SET_WARN_COLOR(COLOR_RED);

				// Can we give a more detailed error message?
				if ( GSerializedObject && GSerializedImportLinker )
				{
					EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *FString::Printf(TEXT("%s (Check the log to see all references!)"), *GSerializedImportLinker->GetImportPathName(GSerializedImportIndex)) );
					warnf( NAME_Warning, TEXT("Failed to load '%s'! Referenced by '%s' ('%s')."), *GSerializedImportLinker->GetImportPathName(GSerializedImportIndex), *GSerializedObject->GetPathName(), GSerializedProperty ? *GSerializedProperty->GetPathName() : TEXT("---") );
				}
				warnf( LocalizeSecure(LocalizeError(TEXT("FailedLoad"),TEXT("Core")), *Filename, Error));
				CLEAR_WARN_COLOR();
			}
		}
		else
		{
			if (!(LoadFlags & LOAD_NoWarn) && GSerializedObject && GSerializedImportLinker)
			{
				warnf( NAME_Warning, TEXT("Failed to load '%s'! Referenced by '%s' ('%s')."), *GSerializedImportLinker->GetImportPathName(GSerializedImportIndex), *GSerializedObject->GetPathName(), GSerializedProperty ? *GSerializedProperty->GetPathName() : TEXT("---") );
			}

			// @see ResavePackagesCommandlet
			if( ParseParam(appCmdLine(),TEXT("SavePackagesThatHaveFailedLoads")) == TRUE )
			{
				warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("FailedLoad"),TEXT("Core")), InFilename ? InFilename : InOuter ? *InOuter->GetName() : TEXT("NULL"), Error) );
			}
			else
			{
				// Don't propagate error up so we can gracefully handle missing packages in the game as well.
				LoadFlags &= ~LOAD_Throw;
				SafeLoadError( InOuter, LoadFlags, Error, LocalizeSecure(LocalizeError(TEXT("FailedLoad"),TEXT("Core")), InFilename ? InFilename : InOuter ? *InOuter->GetName() : TEXT("NULL"), Error) );
			}
		}
	}
#endif

	// Success.
	return Result;
}

/**
 * Find an existing package by name
 * @param InOuter		The Outer object to search inside
 * @param PackageName	The name of the package to find
 *
 * @return The package if it exists
 */
UPackage* UObject::FindPackage( UObject* InOuter, const TCHAR* PackageName )
{
	FString InName;
	if( PackageName )
	{
		InName = PackageName;
	}
	else
	{
		InName = MakeUniqueObjectName( InOuter, UPackage::StaticClass() ).ToString();
	}
	ResolveName( InOuter, InName, 1, 0 );

	UPackage* Result = NULL;
	if ( InName != TEXT("None") )
	{
		Result = FindObject<UPackage>( InOuter, *InName );
	}
	else
	{
		appErrorf( *LocalizeError(TEXT("PackageNamedNone"), TEXT("Core")) );
	}
	return Result;
}

/**
 * Find an existing package by name or create it if it doesn't exist
 * @param	InOuter					The Outer object to search inside
 * @param	PackageName				The name of the package to find
 * @param	bRemappedPackageName	indicates that this is a native script package which has been loaded using a different name;
 *									causes GScriptPackageSuffix to be appended to the package name
 *
 * @return The existing package or a newly created one
 */
UPackage* UObject::CreatePackage( UObject* InOuter, const TCHAR* PackageName, UBOOL bRemappedPackageName/*=FALSE*/ )
{
	FString InName;

	if( PackageName )
	{
		InName = PackageName;
	}
	if( InName.EndsWith( TEXT( "." ) ) )
	{
		FString InName2 = InName.Left( InName.Len() - 1 );
		debugf( TEXT( "Invalid Package Name entered - '%s' renamed to '%s'" ), *InName, *InName2 );
		InName = InName2;
	}

	if(InName.Len() == 0)
	{
		InName = MakeUniqueObjectName( InOuter, UPackage::StaticClass() ).ToString();
	}

	//@script patcher (bRemappedPackageName)
	ResolveName( InOuter, InName, 1, 0, bRemappedPackageName );

	UPackage* Result = NULL;
	if ( InName.Len() == 0 )
	{
		appErrorf( *LocalizeError(TEXT("EmptyPackageName"), TEXT("Core")) );
	}

	if ( InName != TEXT("None") )
	{
		Result = FindObject<UPackage>( InOuter, *InName );
		if( Result == NULL )
		{
			Result = new( InOuter, FName(*InName, FNAME_Add, TRUE), RF_Public )UPackage;
			// default is for packages to be downloadable
			Result->PackageFlags |= PKG_AllowDownload;
		}
	}
	else
	{
		appErrorf( *LocalizeError(TEXT("PackageNamedNone"), TEXT("Core")) );
	}
	return Result;
}

//
// Resolve a package and name.
//
UBOOL UObject::ResolveName( UObject*& InPackage, FString& InName, UBOOL Create, UBOOL Throw, UBOOL bIsPackageName/*=TRUE*/ )
{
	const TCHAR* IniFilename = NULL;

	// See if the name is specified in the .ini file.
	if( appStrnicmp( *InName, TEXT("engine-ini:"), appStrlen(TEXT("engine-ini:")) )==0 )
	{
		IniFilename = GEngineIni;
	}
	else if( appStrnicmp( *InName, TEXT("game-ini:"), appStrlen(TEXT("game-ini:")) )==0 )
	{
		IniFilename = GGameIni;
	}
	else if( appStrnicmp( *InName, TEXT("input-ini:"), appStrlen(TEXT("input-ini:")) )==0 )
	{
		IniFilename = GInputIni;
	}
	else if( appStrnicmp( *InName, TEXT("editor-ini:"), appStrlen(TEXT("editor-ini:")) )==0 )
	{
		IniFilename = GEditorIni;
	}
	else if ( appStrnicmp( *InName, TEXT("ui-ini:"), appStrlen(TEXT("ui-ini:")) ) == 0 )
	{
		IniFilename = GUIIni;
	}


	if( IniFilename && InName.InStr(TEXT("."))!=-1 )
	{
		// Get .ini key and section.
		FString Section = InName.Mid(1+InName.InStr(TEXT(":")));
		INT i = Section.InStr(TEXT("."), 1);
		FString Key;
		if( i != -1)
		{
			Key = Section.Mid(i+1);
			Section = Section.Left(i);
		}

		// Look up name.
		FString Result;
		if( !GConfig->GetString( *Section, *Key, Result, IniFilename ) )
		{
			if( Throw == TRUE )
			{
				FString ErrorMsg = FString::Printf( LocalizeSecure(LocalizeError(TEXT("ConfigNotFound"),TEXT("Core")), *InName, *Section, *Key) );
				//GConfig->Dump( *GLog );
				appThrowf( *FString::Printf( TEXT( " %s %s " ), *ErrorMsg, IniFilename ) );
			}
			return FALSE;
		}
		InName = Result;
	}

	// Strip off the object class.
	INT NameStartIndex = InName.InStr(TEXT("\'"));
	if(NameStartIndex != INDEX_NONE)
	{
		INT NameEndIndex = InName.InStr(TEXT("\'"),TRUE);
		if(NameEndIndex > NameStartIndex)
		{
			InName = InName.Mid(NameStartIndex + 1,NameEndIndex - NameStartIndex - 1);
		}
	}

	// Handle specified packages.
	INT i;

	// if you're attempting to find an object in any package using a dotted name that isn't fully
	// qualified (such as ObjectName.SubobjectName - notice no package name there), you normally call
	// StaticFindObject and pass in ANY_PACKAGE as the value for InPackage.  When StaticFindObject calls ResolveName,
	// it passes NULL as the value for InPackage, rather than ANY_PACKAGE.  As a result, unless the first chunk of the
	// dotted name (i.e. ObjectName from the above example) is a UPackage, the object will not be found.  So here we attempt
	// to detect when this has happened - if we aren't attempting to create a package, and a UPackage with the specified
	// name couldn't be found, pass in ANY_PACKAGE as the value for InPackage to the call to FindObject<UObject>().
	UBOOL bSubobjectPath = FALSE;

	// to make parsing the name easier, replace the subobject delimiter with an extra dot
	InName.ReplaceInline(SUBOBJECT_DELIMITER, TEXT(".."));
	while( (i=InName.InStr(TEXT("."))) != -1 )
	{
		FString PartialName = InName.Left(i);

#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		if ( GIsScriptPatcherActive && InPackage == NULL && GScriptPatchPackageSuffix.Len() > 0 && PartialName != TEXT("Core") )
		{
			if ( PartialName.Right(GScriptPatchPackageSuffix.Len()) != GScriptPatchPackageSuffix )
			{
				PartialName += GScriptPatchPackageSuffix;
			}
		}
#endif

		// if the next part of InName ends in two dots, it indicates that the next object in the path name
		// is not a top-level object (i.e. it's a subobject).  e.g. SomePackage.SomeGroup.SomeObject..Subobject
		if ( InName.Mid(i+1,1) == TEXT(".") )
		{
			InName = PartialName + InName.Mid(i+1);
			bSubobjectPath = TRUE;
			Create = FALSE;
		}

		if( Create )
		{
			InPackage = CreatePackage( InPackage, *PartialName );
		}
		else
		{
			UObject* NewPackage = FindObject<UPackage>( InPackage, *PartialName );
			if( !NewPackage )
			{
				NewPackage = FindObject<UObject>( InPackage == NULL ? ANY_PACKAGE : InPackage, *PartialName );
				if( !NewPackage )
				{
					return bSubobjectPath;
				}
			}
			InPackage = NewPackage;
		}
		InName = InName.Mid(i+1);
	}

#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive && GScriptPatchPackageSuffix.Len() > 0 && InName.Len() > 0 && InPackage == NULL && bIsPackageName && InName != TEXT("Core") )
	{
		if ( InName.Right(GScriptPatchPackageSuffix.Len()) != GScriptPatchPackageSuffix )
		{
			InName += GScriptPatchPackageSuffix;
		}
	}
#endif

	return TRUE;
}

/**
 * Find or load an object by string name with optional outer and filename specifications.
 * These are optional because the InName can contain all of the necessary information.
 *
 * @param ObjectClass	The class (or a superclass) of the object to be loaded.
 * @param InOuter		An optional object to narrow where to find/load the object from
 * @param InName		String name of the object. If it's not fully qualified, InOuter and/or Filename will be needed
 * @param Filename		An optional file to load from (or find in the file's package object)
 * @param LoadFlags		Flags controlling how to handle loading from disk
 * @param Sandbox		A list of packages to restrict the search for the object
 * @param bAllowObjectReconciliation	Whether to allow the object to be found via FindObject in the case of seek free loading
 *
 * @return The object that was loaded or found. NULL for a failure.
 */
UObject* UObject::StaticLoadObject(UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox, UBOOL bAllowObjectReconciliation )
{
	SCOPE_CYCLE_COUNTER(STAT_LoadObject);
	check(ObjectClass);
	check(InName);

	FString		StrName				= InName;
	UObject*	Result				=NULL;
	// Keep track of whether BeginLoad has been called in case we throw an exception.
	UBOOL		bNeedsEndLoadCalled = FALSE;

#if !EXCEPTIONS_DISABLED
	try
#endif
	{
		// break up the name into packages, returning the innermost name and its outer
		ResolveName(InOuter, StrName, TRUE, TRUE, FALSE);
		if ( InOuter != NULL )
		{
			if( bAllowObjectReconciliation && ((GIsGame && !GIsEditor && !GIsUCC) 
#if !CONSOLE
				|| GIsImportingT3D
#endif
				))
			{
				Result = StaticFindObjectFast(ObjectClass, InOuter, *StrName);
			}

			if( !Result )
			{
#if CONSOLE || DEDICATED_SERVER
				if( GUseSeekFreeLoading )
				{
					if ( (LoadFlags&LOAD_NoWarn) == 0 )
					{
						debugf(NAME_Warning,TEXT("StaticLoadObject for %s %s %s couldn't find object in memory!"),
							*ObjectClass->GetName(),
							*InOuter->GetName(),
							*StrName);
					}
				}
				else
#endif //CONSOLE || DEDICATED_SERVER
				{
					BeginLoad();
					bNeedsEndLoadCalled = TRUE;

					// the outermost will be the top-level package that is the root package for the file
					UPackage* TopOuter = InOuter->GetOutermost();

					// find or create a linker object which contains the object being loaded
					ULinkerLoad* Linker = NULL;
					if (!(LoadFlags & LOAD_DisallowFiles))
					{
						Linker = GetPackageLinker(TopOuter, Filename, LoadFlags | LOAD_Throw | LOAD_AllowDll, Sandbox, NULL);
					}
					// if we have a linker for the outermost package, use it to get the object from its export table
					if (Linker)
					{
						// handling an error case
						UBOOL bSkipCreate = FALSE;

						// make sure the InOuter has been loaded off disk so that it has a _LinkerIndex, which
						// Linker->Create _needs_ to be able to find the proper object (two objects with the same
						// name in different groups are only differentiated by their Outer's LinkerIndex!)
						if (InOuter != TopOuter && InOuter->GetLinkerIndex() == INDEX_NONE)
						{
							UObject* LoadedOuter = StaticLoadObject(InOuter->GetClass(), NULL, *InOuter->GetPathName(), Filename, LoadFlags, Sandbox, FALSE);
							// if the outer failed to load, or was otherwise a different object than what ResolveName made, 
							// they are using an invalid group name
							if (LoadedOuter != InOuter || LoadedOuter->GetLinkerIndex() == INDEX_NONE)
							{
								if (!(LoadFlags & LOAD_Quiet))
								{
									warnf(NAME_Warning, TEXT("The Outer object (%s) for '%s' couldn't be loaded [while loading package %s]: %s"),
										*InOuter->GetFullName(), InName, *TopOuter->GetName(),
										(LoadedOuter != InOuter 
										? *FString::Printf(TEXT("Incorrect class for Outer - found object (%s)"), *LoadedOuter->GetFullName())
										: *FString::Printf(TEXT("Invalid linker index [couldn't load %s]?"), *LoadedOuter->GetFullName()))
									);
								}

								// we can't call create without a properly loaded outer
								bSkipCreate = TRUE;
							}
						}
						// if no error yet, create it
						if (!bSkipCreate)
						{
							UObject* ObjOuter = InOuter;

							INT i = StrName.InStr(TEXT("."));
							while ( i != INDEX_NONE )
							{
								FString NextOuterName = StrName.Left(i);
								StrName = StrName.Mid(i+1);
								i = StrName.InStr(TEXT("."));

								ObjOuter = Linker->Create(UObject::StaticClass(), *NextOuterName, ObjOuter, LoadFlags, FALSE);
							}

							// InOuter should now be properly loaded with a linkerindex, so we can use it to create the actual desired object
							Result = Linker->Create(ObjectClass, *StrName, ObjOuter ? ObjOuter : InOuter, LoadFlags, 0);
						}
					}

					// if we haven't created it yet, try to find it (which will always be the case if LOAD_DisallowFiles is on)
					if (!Result)
					{
						Result = StaticFindObjectFast(ObjectClass, InOuter, *StrName);
					}

					bNeedsEndLoadCalled = FALSE;
					EndLoad( *StrName );
				}
			}
		}
		// if we haven't created or found the object, error
		if (!Result)
		{
#if EXCEPTIONS_DISABLED
			return NULL;
#else
			appThrowf(LocalizeSecure(LocalizeError(TEXT("ObjectNotFound"),TEXT("Core")), *ObjectClass->GetName(), InOuter ? *InOuter->GetPathName() : TEXT("None"), *StrName));
#endif
		}
	}
#if !EXCEPTIONS_DISABLED
	catch( const TCHAR* Error )
	{
		if( bNeedsEndLoadCalled )
		{
			EndLoad();
		}
		SafeLoadError(InOuter, LoadFlags, Error, LocalizeSecure(LocalizeError(TEXT("FailedLoadObject"),TEXT("Core")), *ObjectClass->GetName(), InOuter ? *InOuter->GetPathName() : TEXT("None"), *StrName, Error) );
	}
#endif
	return Result;
}

//
// Load a class.
//
UClass* UObject::StaticLoadClass( UClass* BaseClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	check(BaseClass);
#if !EXCEPTIONS_DISABLED
	try
#endif
	{
		UClass* Class = LoadObject<UClass>( InOuter, InName, Filename, LoadFlags | LOAD_Throw, Sandbox );
		if( Class && !Class->IsChildOf(BaseClass) )
			appThrowf( LocalizeSecure(LocalizeError(TEXT("LoadClassMismatch"),TEXT("Core")), *Class->GetFullName(), *BaseClass->GetFullName()) );
		return Class;
	}
#if !EXCEPTIONS_DISABLED
	catch( const TCHAR* Error )
	{
		// Failed.
		SafeLoadError( InOuter, LoadFlags, Error, Error );
		return NULL;
	}
#endif
}

/**
 * Loads a package and all contained objects that match context flags.
 *
 * @param	InOuter		Package to load new package into (usually NULL or ULevel->GetOuter())
 * @param	Filename	Name of file on disk
 * @param	LoadFlags	Flags controlling loading behavior
 * @return	Loaded package if successful, NULL otherwise
 */
UPackage* UObject::LoadPackage( UPackage* InOuter, const TCHAR* Filename, DWORD LoadFlags )
{
	UPackage* Result = NULL;

    if( *Filename == '\0' )
        return NULL;

	// Try to load.
	BeginLoad();
#if !EXCEPTIONS_DISABLED
	try
#endif
	{
		// Keep track of start time.
		DOUBLE StartTime = appSeconds();

		FFilename FileToLoad = Filename ? Filename : *InOuter->GetName();
		// Create a new linker object which goes off and tries load the file.
		ULinkerLoad* Linker = GetPackageLinker( InOuter, *FileToLoad, LoadFlags | LOAD_Throw, NULL, NULL );
        if( !Linker )
		{
			EndLoad();
            return( NULL );
		}
		Result = Linker->LinkerRoot;

		//If the filename is passed in AND the packagename is not equal to none AND is not equal to the name of the package, save the filename
		if (Filename && InOuter && appStricmp(TEXT("None"),*InOuter->GetName()) != 0 && appStricmp(Filename, *InOuter->GetName()) != 0)
		{
			Result->FileName = FName(*FileToLoad);
		}

		// is there a script SHA hash for this package?
		BYTE SavedScriptSHA[20];
		UBOOL bHasScriptSHAHash = FSHA1::GetFileSHAHash(*Linker->LinkerRoot->GetName(), SavedScriptSHA, FALSE);
		if (bHasScriptSHAHash)
		{
			// if there is, start generating the SHA for any script code in this package
			Linker->StartScriptSHAGeneration();
		}

		if( !(LoadFlags & LOAD_Verify) )
		{
			Linker->LoadAllObjects();
		}


#if WITH_EDITOR
		// Add a LoadContext string to the endload function in the form of: "<FileToLoad> Package"
		EndLoad( GIsEditor ? *FileToLoad.GetBaseFilename() : NULL );
#else
		EndLoad();
#endif
		// Cancel all texture allocations that haven't been claimed yet.
		Linker->Summary.TextureAllocations.CancelRemainingAllocations( TRUE );

		// if we are calculating the script SHA for a package, do the comparison now
		if (bHasScriptSHAHash)
		{
			// now get the actual hash data
			BYTE LoadedScriptSHA[20];
			Linker->GetScriptSHAKey(LoadedScriptSHA);

			// compare SHA hash keys
			if (appMemcmp(SavedScriptSHA, LoadedScriptSHA, 20) != 0)
			{
				appOnFailSHAVerification(*Linker->Filename, FALSE);
			}
		}

		// now that all the objects are loaded, lookup any objects that need looking up
		Result->LookupAllOutstandingCrossLevelExports(Linker);

		// Only set time it took to load package if the above EndLoad is the "outermost" EndLoad.
		if( Result && GObjBeginLoadCount == 0 && !(LoadFlags & LOAD_Verify) )
		{
			Result->SetLoadTime( appSeconds() - StartTime );
		}

		if (GUseSeekFreeLoading)
		{
			// give a hint to the IO system that we are done with this file for now
			FIOSystem* AsyncIO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
			if (AsyncIO)
			{
				AsyncIO->HintDoneWithFile(*Linker->Filename);
			}

#if SUPPORTS_SCRIPTPATCH_LOADING
			// okay, now we're done with the patch data, we can toss it
			FScriptPatcher* Patcher = Linker->GetExistingScriptPatcher();
			if (Patcher)
			{
				Patcher->FreeLinkerPatch(Result->GetFName());
			}
#endif
		}

	}
#if !EXCEPTIONS_DISABLED
	catch( const TCHAR* Error )
	{
		EndLoad();
		SafeLoadError( InOuter, LoadFlags, Error, LocalizeSecure(LocalizeError(TEXT("FailedLoadPackage"),TEXT("Core")), Error) );
		Result = NULL;
	}
#endif
	if( GUseSeekFreeLoading && Result && !(LoadFlags & LOAD_NoSeekFreeLinkerDetatch) )
	{
		// We no longer need the linker. Passing in NULL would reset all loaders so we need to check for that.
		UObject::ResetLoaders( Result );
	}
	return Cast<UPackage>(Result);
}


/**
 * Dissociates all linker import and forced export object references. This currently needs to 
 * happen as the referred objects might be destroyed at any time.
 */
void UObject::DissociateImportsAndForcedExports()
{
	if( GImportCount )
	{
		for( INT LoaderIndex=0; LoaderIndex<GObjLoaders.Num(); LoaderIndex++ )
		{
			ULinkerLoad* Linker = GetLoader(LoaderIndex);
			//@todo optimization: only dissociate imports for loaders that had imports created
			//@todo optimization: since the last time this function was called.
			for( INT ImportIndex=0; ImportIndex<Linker->ImportMap.Num(); ImportIndex++ )
			{
				FObjectImport& Import = Linker->ImportMap(ImportIndex);
				if( Import.XObject && !Import.XObject->HasAnyFlags(RF_Native) )
				{
					Import.XObject = NULL;
				}
				Import.SourceLinker = NULL;
				// when the SourceLinker is reset, the SourceIndex must also be reset, or recreating
				// an import that points to a redirector will fail to find the redirector
				Import.SourceIndex = INDEX_NONE;
			}
		}
	}
	GImportCount = 0;

#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GForcedExportCount && !GIsScriptPatcherActive )
#else
	if( GForcedExportCount )
#endif
	{
		for( INT LoaderIndex=0; LoaderIndex<GObjLoaders.Num(); LoaderIndex++ )
		{
			ULinkerLoad* Linker = GetLoader(LoaderIndex);
			//@todo optimization: only dissociate exports for loaders that had forced exports created
			//@todo optimization: since the last time this function was called.
			for( INT ExportIndex=0; ExportIndex<Linker->ExportMap.Num(); ExportIndex++ )
			{
				FObjectExport& Export = Linker->ExportMap(ExportIndex);
				if( Export._Object && Export.HasAnyFlags(EF_ForcedExport) )
				{
					Export._Object->SetLinker( NULL, INDEX_NONE );
					Export._Object = NULL;
				}
			}
		}
	}
	GForcedExportCount = 0;
}

//
// Begin loading packages.
//warning: Objects may not be destroyed between BeginLoad/EndLoad calls.
//
void UObject::BeginLoad()
{
	if( ++GObjBeginLoadCount == 1 )
	{
		// Make sure we're finishing up all pending async loads, and trigger texture streaming next tick if necessary.
		FlushAsyncLoading( NAME_None );

		// Validate clean load state.
		//@script patcher fixme: asserts when patching
		check(GObjLoaded.Num()==0);
		check(!GAutoRegister);
	}
}

// Sort objects by linker name and file offset
IMPLEMENT_COMPARE_POINTER( UObject, UnObj,
{
	ULinker* LinkerA = A->GetLinker();
	ULinker* LinkerB = B->GetLinker();

	// Both objects have linkers.
	if( LinkerA && LinkerB )
	{
		// Identical linkers, sort by offset in file.
		if( LinkerA == LinkerB )
		{
			FObjectExport& ExportA = LinkerA->ExportMap( A->GetLinkerIndex() );
			FObjectExport& ExportB = LinkerB->ExportMap( B->GetLinkerIndex() );
			return ExportA.SerialOffset - ExportB.SerialOffset;
		}
		// Sort by linker name.
		else
		{
			return LinkerA->GetFName().GetIndex() - LinkerB->GetFName().GetIndex();
		}
	}
	// Neither objects have a linker, don't do anything.
	else if( LinkerA == LinkerB )
	{
		return 0;
	}
	// Sort objects with linkers vs. objects without
	else
	{
		return LinkerA ? -1 : 1;
	}
}
)

//
// End loading packages.
//
void UObject::EndLoad( const TCHAR* LoadContext )
{
	check(GObjBeginLoadCount>0);

	// Used to control animation of the load progress status updates.
	INT ProgressIterator = 2;

	while( --GObjBeginLoadCount == 0 && (GObjLoaded.Num() || GImportCount || GForcedExportCount) )
	{
		// Make sure we're not recursively calling EndLoad as e.g. loading a config file could cause
		// BeginLoad/EndLoad to be called.
		GObjBeginLoadCount++;


#if !EXCEPTIONS_DISABLED
		try
#endif
		{
			// Temporary list of loaded objects as GObjLoaded might expand during iteration.
			TArray<UObject*> ObjLoaded;
			TSet<ULinkerLoad*> LoadedLinkers; 


#if WITH_EDITOR
			// Stores the progress symbols that we will animate through during long operations
			FString ProgressSymbols[3] = {".","..","..."};
			DOUBLE StartTime = appSeconds();
			DOUBLE UpdateDelta = 0.75;
			BOOL bIsLoadContextValid = LoadContext != NULL && *LoadContext != '\0';
			// We currently only allow status updates during the editor load splash screen.
			BOOL bAllowStatusUpdate = GIsEditor && !GIsUCC && !GIsSlowTask && bIsLoadContextValid;

			if ( bAllowStatusUpdate && GObjLoaded.Num() > 0 )
			{
				GWarn->StatusUpdatef( -1, -1, LocalizeSecure(LocalizeProgress(TEXT("LoadingRefObjects"),TEXT("Core")), LoadContext, *ProgressSymbols[ProgressIterator]));
				ProgressIterator = (ProgressIterator + 1) % 3;
			}
#endif

			while( GObjLoaded.Num() )
			{
				// Accumulate till GObjLoaded no longer increases.
				ObjLoaded += GObjLoaded;
				GObjLoaded.Empty();

				// Sort by Filename and Offset.
				Sort<USE_COMPARE_POINTER(UObject,UnObj)>( &ObjLoaded(0), ObjLoaded.Num() );

				// Finish loading everything.
				debugfSlow( NAME_DevLoad, TEXT("Loading objects...") );
				for( INT i=0; i<ObjLoaded.Num(); i++ )
				{
#if WITH_EDITOR
					// This can be a long operation so we will output some progress feedback to the 
					//  user in the form of 3 dots that animate between "." ".." "..."
					DOUBLE CurrTime = appSeconds();
					if ( bAllowStatusUpdate && CurrTime - StartTime > UpdateDelta )
					{
						StartTime = CurrTime;
						GWarn->StatusUpdatef( -1, -1, LocalizeSecure(LocalizeProgress(TEXT("LoadingRefObjects"),TEXT("Core")), LoadContext, *ProgressSymbols[ProgressIterator]));
						ProgressIterator = (ProgressIterator + 1) % 3;

					}		
#endif
					// Preload.
					UObject* Obj = ObjLoaded(i);
					if( Obj->HasAnyFlags(RF_NeedLoad) )
					{
						check(Obj->GetLinker());
						Obj->GetLinker()->Preload( Obj );
					}
				}

				// Start over again as new objects have been loaded that need to have "Preload" called on them before
				// we can safely PostLoad them.
				if(GObjLoaded.Num())
				{
					continue;
				}

				// Return the progress iterator to the default value for future operations.
				ProgressIterator = 2;

				if ( GIsEditor )
				{
					for( INT i=0; i<ObjLoaded.Num(); i++ )
					{
						UObject* Obj = ObjLoaded(i);
						if ( Obj->GetLinker() )
						{
							LoadedLinkers.Add(Obj->GetLinker());
						}
					}
				}

				
				// set this so that we can perform certain operations in which are only safe once all objects have been de-serialized.
				GIsRoutingPostLoad = TRUE;

				// Postload objects.
				for( INT i=0; i<ObjLoaded.Num(); i++ )
				{
#if WITH_EDITOR
					// This can be a long operation so we will output some progress feedback to the 
					//  user in the form of 3 dots that animate between "." ".." "..."
					DOUBLE CurrTime = appSeconds();
					if ( bAllowStatusUpdate && CurrTime - StartTime > UpdateDelta )
					{
						StartTime = CurrTime;
						GWarn->StatusUpdatef( -1, -1, LocalizeSecure(LocalizeProgress(TEXT("ProcessingRefObjects"),TEXT("Core")), LoadContext, *ProgressSymbols[ProgressIterator]));
						ProgressIterator = (ProgressIterator + 1) % 3;
					}
#endif

					UObject* Obj = ObjLoaded(i);
					check(Obj);
					Obj->ConditionalPostLoad();
				}

				GIsRoutingPostLoad = FALSE;

#if !CONSOLE
				// Send global notification for each object that was loaded.
				// Useful for updating UI such as ContentBrowser's loaded status.
				if (GIsWatchingEndLoad)
				{
					FCallbackEventParameters EventParams(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateAssetListUI);
					for( INT CurObjIndex=0; CurObjIndex<ObjLoaded.Num(); CurObjIndex++ )
					{
						UObject* Obj = ObjLoaded(CurObjIndex);
						check(Obj);
						EventParams.EventObject = Obj;
						GCallbackEvent->Send( EventParams );
					}
				}
#endif

				// Empty array before next iteration as we finished postloading all objects.
				ObjLoaded.Empty( GObjLoaded.Num() );
			}

			if ( GIsEditor && LoadedLinkers.Num() > 0 )
			{
				for ( TSet<ULinkerLoad*>::TIterator It(LoadedLinkers); It; ++It )
				{
					ULinkerLoad* LoadedLinker = *It;
					check(LoadedLinker);

					if ( LoadedLinker->LinkerRoot != NULL && !LoadedLinker->LinkerRoot->IsFullyLoaded() )
					{
						UBOOL bAllExportsCreated = TRUE;
						for ( INT ExportIndex = 0; ExportIndex < LoadedLinker->ExportMap.Num(); ExportIndex++ )
						{
							FObjectExport& Export = LoadedLinker->ExportMap(ExportIndex);
							if ( !Export.HasAnyFlags(EF_ForcedExport) && Export._Object == NULL )
							{
								bAllExportsCreated = FALSE;
								break;
							}
						}

						if ( bAllExportsCreated )
						{
							LoadedLinker->LinkerRoot->MarkAsFullyLoaded();
						}
					}
				}
			}

			// Dissociate all linker import and forced export object references, since they
			// may be destroyed, causing their pointers to become invalid.
			DissociateImportsAndForcedExports();
		}
#if !EXCEPTIONS_DISABLED
		catch( const TCHAR* Error )
		{
			// Any errors here are fatal.
			GError->Logf( Error );
		}
#endif
	}
}

//
// Empty the loaders.
//
void UObject::ResetLoaders( UObject* InPkg )
{
	// Make sure we're not in the middle of loading something in the background.
	FlushAsyncLoading();

	// Top level package to reset loaders for.
	UObject*		TopLevelPackage = InPkg ? InPkg->GetOutermost() : NULL;
	// Linker to reset/ detach.
	ULinkerLoad*	LinkerToReset	= NULL;

	// Find loader/ linker associated with toplevel package. We do this upfront as Detach resets LinkerRoot.
	if( TopLevelPackage )
	{
		for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
		{
			ULinkerLoad* Linker = GetLoader(i);
			if( Linker->LinkerRoot == TopLevelPackage )
			{
				LinkerToReset = Linker;
				break;
			}
		}
	}

	// Need to iterate in reverse order as Detach removes entries from GObjLoaders array.
	if ( TopLevelPackage == NULL || LinkerToReset != NULL )
	{
		for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
		{
			ULinkerLoad* Linker = GetLoader(i);
			// Detach linker if it has matching linker root or we are detaching all linkers.
			if( !TopLevelPackage || Linker->LinkerRoot==TopLevelPackage )
			{
				// Detach linker, also removes from array and sets LinkerRoot to NULL.
#if SUPPORTS_SCRIPTPATCH_CREATION
				//@script patcher
				Linker->Detach( !GIsScriptPatcherActive );
#else
				Linker->Detach( TRUE );
#endif
			}
			// Detach LinkerToReset from other linker's import table.
			else
			{
				for( INT j=0; j<Linker->ImportMap.Num(); j++ )
				{
					if( Linker->ImportMap(j).SourceLinker == LinkerToReset )
					{
						Linker->ImportMap(j).SourceLinker	= NULL;
						Linker->ImportMap(j).SourceIndex	= INDEX_NONE;
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

//
// Return whether statics are initialized.
//
UBOOL UObject::GetInitialized()
{
	return UObject::GObjInitialized;
}

//
// Return the static transient package.
//
UPackage* UObject::GetTransientPackage()
{
	return UObject::GObjTransientPkg;
}

//
// Return the ith loader.
//?
ULinkerLoad* UObject::GetLoader( INT i )
{
	return GObjLoaders(i);
}

//
// Add an object to the root set. This prevents the object and all
// its descendants from being deleted during garbage collection.
//
void UObject::AddToRoot()
{
	SetFlags( RF_RootSet );
}

//
// Remove an object from the root set.
//
void UObject::RemoveFromRoot()
{
	ClearFlags( RF_RootSet );
}

/*-----------------------------------------------------------------------------
	Object name functions.
-----------------------------------------------------------------------------*/

/**
 * Create a unique name by combining a base name and an arbitrary number string.
 * The object name returned is guaranteed not to exist.
 *
 * @param	Parent		the outer for the object that needs to be named
 * @param	Class		the class for the object
 * @param	BaseName	optional base name to use when generating the unique object name; if not specified, the class's name is used
 *
 * @return	name is the form BaseName_##, where ## is the number of objects of this
 *			type that have been created since the last time the class was garbage collected.
 */
FName UObject::MakeUniqueObjectName( UObject* Parent, UClass* Class, FName BaseName/*=NAME_None*/ )
{
	check(Class);
	if ( BaseName == NAME_None )
	{
		BaseName = Class->GetFName();
	}

	// cache the class's name's index for faster name creation later
	EName BaseNameIndex = (EName)BaseName.GetIndex();
	FName TestName;
#if CONSOLE
	if (GUglyHackFlags & HACK_FastPathUniqueNameGeneration)
	{
		/*   Fast Path Name Generation
		* A significant fraction of object creation time goes into verifying that the a chosen unique name is really unique.
		* The idea here is to generate unique names using very high numbers and only in situations where collisions are 
		* impossible for other reasons.
		*
		* Rationale for uniqueness as used here.
		* - Consoles do not save objects in general, and certainly not animation trees. So we could never load an object that would later clash.
		* - We assume that we never load or create any object with a "name number" as large as, say, MAXINT / 2, other than via 
		*   HACK_FastPathUniqueNameGeneration.
		* - After using one of these large "name numbers", we decrement the static UniqueIndex, this no two names generated this way, during the
		*   same run, could ever clash.
		* - We assume that we could never create anywhere near MAXINT/2 total objects at runtime, within a single run. 
		* - We require an outer for these items, thus outers must themselves be unique. Therefore items with unique names created on the fast path
		*   could never clash with anything with a different outer. For animation trees, these outers are never saved or loaded, thus clashes are 
		*   impossible.
		*/
		static INT UniqueIndex = MAXINT - 1000;
		TestName = FName(BaseNameIndex, --UniqueIndex);
		checkSlow(Parent); 
		checkSlow(Parent!=ANY_PACKAGE); 
		checkSlow(!StaticFindObjectFastInternal( NULL, Parent, TestName, FALSE, Parent==ANY_PACKAGE, 0 ));
	}
	else
#endif
	{
		do
		{
			// create the next name in the sequence for this class
			if (BaseNameIndex == NAME_Package)
			{
				//package names should default to "Untitled"
				TestName = FName(NAME_Untitled, ++Class->ClassUnique);
			}
			else
			{
				TestName = FName(BaseNameIndex, ++Class->ClassUnique);
			}
		} 
		while( StaticFindObjectFastInternal( NULL, Parent, TestName, FALSE, Parent==ANY_PACKAGE, 0 ) );
	} 
	return TestName;
}
/*-----------------------------------------------------------------------------
	Object hashing.
-----------------------------------------------------------------------------*/

//
// Add an object to the hash table.
//
void UObject::HashObject()
{
	INT iHash = 0;
// @todo ObjHash change -- enable once the ensure has found all the culprits and all FindObject(ANY_PACKAGE) has been vetted
//	if (IsNameHashed())
	{
		iHash = GetObjectHash( Name );
		HashNext        = GObjHash[iHash];
		GObjHash[iHash] = this;
	}
	// Now hash using the outer
	iHash			= GetObjectOuterHash(Name,(PTRINT)Outer);
	HashOuterNext	= GObjHashOuter[iHash];
	GObjHashOuter[iHash] = this;
}

//
// Remove an object from the hash table.
//
void UObject::UnhashObject()
{
	INT       iHash   = 0;
	UObject** Hash    = NULL;
#if _DEBUG
	INT       Removed = 0;
#endif

// @todo ObjHash change -- enable once the ensure has found all the culprits and all FindObject(ANY_PACKAGE) has been vetted
//	if (IsNameHashed())
	{
		iHash   = GetObjectHash( Name );
		Hash    = &GObjHash[iHash];

		while( *Hash != NULL )
		{
			if( *Hash != this )
			{
				Hash = &(*Hash)->HashNext;
 			}
			else
			{
				*Hash = (*Hash)->HashNext;
#if _DEBUG
				// Verify that we find one and just one object in debug builds.		
				Removed++;
#else
				break;
#endif
			}
		}
#if _DEBUG
		checkSlow(Removed != 0);
		checkSlow(Removed == 1);
#endif
	}
	// Remove the object from the outer hash.
	// NOTE: It relies on the outer being untouched and treats it as an int to avoid potential crashes during GC
	iHash   = GetObjectOuterHash(Name,(PTRINT)Outer);
	Hash    = &GObjHashOuter[iHash];
#if _DEBUG
	Removed = 0;
#endif
	// Search through the hash bin and remove the item
	while( *Hash != NULL )
	{
		if( *Hash != this )
		{
			Hash = &(*Hash)->HashOuterNext;
 		}
		else
		{
			*Hash = (*Hash)->HashOuterNext;
#if _DEBUG
			Removed++;
#else
			return;
#endif
		}
	}
#if _DEBUG
	checkSlow(Removed != 0);
	checkSlow(Removed == 1);
#endif
}

/*-----------------------------------------------------------------------------
	Creating and allocating data for new objects.
-----------------------------------------------------------------------------*/

//
// Add an object to the table.
//
void UObject::AddObject( INT InIndex )
{
	// Find an available index.
	if( InIndex==INDEX_NONE )
	{
		// Special non- garbage collectable range.
		if( HasAnyFlags( RF_DisregardForGC ) && (++GObjLastNonGCIndex < GObjFirstGCIndex) )
		{
			// "Last" was pre-incremented above. This is done to allow keeping track of how many objects we requested
			// to be disregarded, regardless of actual allocated pool size.
			InIndex = GObjLastNonGCIndex;
		}
		// Regular pool/ range.
		else
		{
			if( GObjAvailable.Num() )
			{
				InIndex = GObjAvailable.Pop();
				check(GObjObjects(InIndex)==NULL);
			}
			else
			{
				InIndex = GObjObjects.Add();
			}
		}
	}

	// Clear object flag signaling disregard for GC if object was allocated in garbage collectible range.
	if( InIndex >= GObjFirstGCIndex )
	{
		// Object is allocated in regular pool so we need to clear RF_DisregardForGC if it was set.
		ClearFlags( RF_DisregardForGC );
	}

	// Make sure only objects in disregarded index range have the object flag set.
	check( !HasAnyFlags( RF_DisregardForGC ) || (InIndex < GObjFirstGCIndex) );
	// Make sure that objects disregarded for GC are part of root set.
	check( !HasAnyFlags( RF_DisregardForGC ) || HasAnyFlags( RF_RootSet ) );

	// Add to global table.
	GObjObjects(InIndex) = this;
	Index = InIndex;
	HashObject();
}

/**
 * Create a new instance of an object or replace an existing object.  If both an Outer and Name are specified, and there is an object already in memory with the same Class, Outer, and Name, the
 * existing object will be destructed, and the new object will be created in its place.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @param	Template	if specified, the property values from this object will be copied to the new object, and the new object's ObjectArchetype value will be set to this object.
 *						If NULL, the class default object is used instead.
 * @param	Error		the output device to use for logging errors
 * @param	Ptr			the address to use for allocating the new object.  If specified, new object will be created in the same memory block as the existing object.
 * @param	SubobjectRoot
 *						Only used to when duplicating or instancing objects; in a nested subobject chain, corresponds to the first object that is not a subobject.
 *						A value of INVALID_OBJECT for this parameter indicates that we are calling StaticConstructObject to duplicate or instance a non-subobject (which will be the subobject root for any subobjects of the new object)
 *						A value of NULL indicates that we are not instancing or duplicating an object.
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer to a fully initialized object of the specified class.
 */
UObject* UObject::StaticAllocateObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	EObjectFlags	InFlags,
	UObject*		InTemplate,
	FOutputDevice*	Error,
	UObject*		Ptr,
	UObject*		SubobjectRoot,
	FObjectInstancingGraph* InstanceGraph
)
{
	check(Error);
	check(!InClass || InClass->ClassWithin);
	check(!InClass || InClass->ClassConstructor);

	// Validation checks.
	if( !InClass )
	{
		Error->Logf( TEXT("Empty class for object %s"), *InName.ToString() );
		return NULL;
	}
	if( InClass->GetIndex()==INDEX_NONE && GObjRegisterCount==0 )
	{
		Error->Logf( TEXT("Unregistered class for %s"), *InName.ToString() );
		return NULL;
	}

	// for abstract classes that are being loaded NOT in the editor we want to error.  If they are in the editor we do not want to have an error
	if ( InClass->HasAnyClassFlags(CLASS_Abstract) && (InFlags&RF_ClassDefaultObject) == 0 )
	{
		if ( GIsEditor )
		{
			// if we are trying instantiate an abstract class in the editor we'll warn the user that it will be nulled out on save
			warnf( NAME_Warning, TEXT("Class which was marked abstract was trying to be loaded.  It will be nulled out on save. %s %s"), *InName.ToString(), *InClass->GetName() );
		}
		else
		{
			Error->Logf( LocalizeSecure(LocalizeError(TEXT("Abstract"),TEXT("Core")), *InName.ToString(), *InClass->GetName()) );
			return NULL;
		}
	}

	if( InOuter == NULL )
	{
		if ( InClass != UPackage::StaticClass() )
		{
			Error->Logf( LocalizeSecure(LocalizeError(TEXT("NotPackaged"),TEXT("Core")), *InClass->GetName(), *InName.ToString()) );
			return NULL;
		}
		else if ( InName == NAME_None )
		{
			Error->Logf( *LocalizeError(TEXT("PackageNamedNone"), TEXT("Core")) );
			return NULL;
		}
	}

	if ( (InFlags & RF_ClassDefaultObject) != 0 )
	{
		TCHAR Result[NAME_SIZE] = DEFAULT_OBJECT_PREFIX;
		FString ClassName = InClass->Index == INDEX_NONE
			? *(TCHAR* const*)&InClass->Name
			: InClass->Name.ToString();

		appStrncat(Result, *ClassName, NAME_SIZE);
		InName = FName(Result);
	}
	else if ( InOuter != NULL && !InOuter->IsA(InClass->ClassWithin) )
	{
		Error->Logf( LocalizeSecure(LocalizeError(TEXT("NotWithin"),TEXT("Core")), *InClass->GetName(), *InName.ToString(), *InOuter->GetClass()->GetName(), *InClass->ClassWithin->GetName()) );
		return NULL;
	}
	else if ( InClass->IsMisaligned() )
	{
		appThrowf(LocalizeSecure(LocalizeError(TEXT("NativeRebuildRequired"), TEXT("Core")), *InClass->GetName()) );
	}

#if USE_MALLOC_PROFILER_DECODE_SCRIPT
	// The only early exit from this function is if we're running the Make commandlet, so in order
	// to keep the begin/end calls balanced, we skip allocation tracking altogether.
	if( !GIsUCCMake )
	{
		GMallocProfiler->GetScriptCallStackDecoder()->BeginAllocateObject( InClass );
	}
#endif

	UObject* Obj = NULL;

	// Compose name, if unnamed.
	if( InName==NAME_None )
	{
		InName = MakeUniqueObjectName( InOuter, InClass );
	}
	else
	{
		// See if object already exists.
		// Using bExactClass = TRUE, since TTP 173663, fixes crash when objects with same full name path but different classes are loaded.
		Obj = StaticFindObjectFastInternal( InClass, InOuter, InName, TRUE );//oldver: Should use NULL instead of InClass to prevent conflicts by name rather than name-class.
	}

	UClass*			Cls							= ExactCast<UClass>( Obj );
	INT				Index						= INDEX_NONE;
	UClass*			ClassWithin					= NULL;
	UObject*		DefaultObject				= NULL;
	UObject*		ObjectArchetype				= InTemplate;
	DWORD			ClassFlags					= 0;
	DWORD			CastFlags					= CASTCLASS_None;
	void			(*ClassConstructor)(void*)	= NULL;
	ULinkerLoad*	Linker						= NULL;
	INT				LinkerIndex					= INDEX_NONE;
	INT				OldNetIndex					= INDEX_NONE;

	/**
	 * remember the StaticClassConstructor for the class so that we can restore it after we've call
	 * appMemzero() on the object's memory address, which results in the non-intrinsic classes losing
	 * its ClassStaticConstructor
	 */
	// the class's StaticConstructor method
	void			(UObject::*StaticClassConstructor)() = NULL;

	// the class's StaticInitialize method
	void			(UObject::*StaticClassInitializer)() = NULL;

	if( Obj == NULL )
	{
		// Figure out size, alignment and aligned size of object.
		INT Size		= InClass->GetPropertiesSize();

		// Enforce 8 byte alignment to ensure ObjectFlags are never accessed misaligned.
		INT Alignment	= Max( 8, InClass->GetMinAlignment() );
		INT AlignedSize = Align( Size, Alignment );

		// Reuse existing pointer, in-place replace.
		if( Ptr )
		{
			Obj = Ptr;
		}
		// Use object memory pool for objects disregarded by GC (initially loaded ones). This allows identifying their
		// GC status by simply looking at their address.
		else if( (InFlags&RF_DisregardForGC) != 0 && (Align(GPermanentObjectPoolTail,Alignment) + Size) <= (GPermanentObjectPool + GPermanentObjectPoolSize) )
		{
			// Align current tail pointer and use it for object. 
			BYTE* AlignedPtr = Align( GPermanentObjectPoolTail, Alignment );
			Obj = (UObject*)AlignedPtr;

			// Update tail pointer.
			GPermanentObjectPoolTail = AlignedPtr + Size;
		}
		// Allocate new memory of the appropriate size and alignment.
		else
		{
			Obj = (UObject*)appMalloc( AlignedSize );
		}

		// InClass->Index == INDEX_NONE when InClass hasn't been fully initialized yet (during static registration)
		// thus its Outer and Name won't be valid yet (@see UObject::Register)
		if ( InClass->Index != INDEX_NONE && ObjectArchetype == NULL && InClass != UObject::StaticClass() )
		{
			ObjectArchetype = InClass->GetDefaultObject();
		}
	}
	else
	{
		// Replace an existing object without affecting the original's address or index.
		check(!Ptr || Ptr==Obj);
		check(!GIsAsyncLoading);
		check(!Obj->HasAnyFlags(RF_Unreachable|RF_AsyncLoading));
		debugfSlow( NAME_DevReplace, TEXT("Replacing %s"), *Obj->GetName() );

		// Can only replace if class is identical.
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		if ( GIsScriptPatcherActive )
		{
			if ( Obj->GetClass()->GetFName() != InClass->GetFName() )
			{
				appErrorf( LocalizeSecure(LocalizeError(TEXT("NoReplace"),TEXT("Core")), *Obj->GetFullName(), *InClass->GetName()) );
			}
		}
		else
#endif
		if( Obj->GetClass() != InClass )
		{
			appErrorf( LocalizeSecure(LocalizeError(TEXT("NoReplace"),TEXT("Core")), *Obj->GetFullName(), *InClass->GetName()) );
		}

		// Remember linker, flags, index, and native class info.
		Linker		= Obj->_Linker;
		LinkerIndex = Obj->_LinkerIndex;
		InFlags		|= Obj->GetMaskedFlags(RF_Keep);
		Index		= Obj->Index;
		OldNetIndex = Obj->NetIndex;

		// if this is the class default object, it means that InClass is native and the CDO was created during static registration.
		// Propagate the load flags but don't replace it - the CDO will be initialized by InitClassDefaultObject
		if ( Obj->HasAnyFlags(RF_ClassDefaultObject) )
		{
			Obj->ObjectFlags |= InFlags;

			// never call PostLoad on class default objects
			Obj->ClearFlags(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);

			if ( GIsUCCMake )
			{
				return Obj;
			}
		}
		else
		{

			// if this object was found and we were searching for the class default object
			// it should be marked as such
			check((InFlags&RF_ClassDefaultObject)==0);
			checkf(!Obj->HasAnyFlags(RF_NeedLoad), LocalizeSecure(LocalizeError(TEXT("ReplaceNotFullyLoaded_f"),TEXT("Core")), *Obj->GetFullName()) );
			if ( Obj->HasAnyFlags(RF_NeedPostLoad) )
			{
				// this is only safe while we are routing PostLoad to all loaded objects - i.e. it is unsafe to do if we are still in the Preload
				// portion of UObject::EndLoad
				checkf(GIsRoutingPostLoad,LocalizeSecure(LocalizeError(TEXT("ReplaceNoPostLoad_f"),TEXT("Core")),*Obj->GetFullName()));

				// Clear these flags so that we don't trigger the assertion in UObject::SetLinker.  We don't call ConditionalPostLoad (which would take of
				// all this for us) because we don't want the object to do all of the subobject/component instancing stuff because we're about to destruct
				// this object
				Obj->ClearFlags(RF_NeedPostLoad|RF_DebugPostLoad);

				// It is assumed that if an object is serialized, it will always have PostLoad called on it.  Based on this assumption, some
				// classes might be doing two-part operations where the first part happens in Serialize and the second part happens in PostLoad.
				// So if we're about to replace the object and it has been loaded but still needs PostLoad called, do that now.
				Obj->PostLoad();
			}
		}

#if 0
		// ronp - this seems to have to have too many potentially negative side-effects (for example, if you're creating a completely new object that shouldn't 
		// have the same archetype as the old object).  It breaks an essential assumption: if I pass NULL for the InArchetype parameters, the object that is created
		// will use the class's default object for its archetype.
		// I'm not sure under which circumstances you'd want this behavior, but I suspect there is an edge case when constructing archetypes where you'd want this so I'll leave it in for now...
		if ( ObjectArchetype == NULL )
		{
			ObjectArchetype = Obj->ObjectArchetype
				? Obj->ObjectArchetype
				: Obj->GetClass()->GetDefaultObject();
		}
#else
		if ( InClass != UObject::StaticClass() )
		{
			if ( ObjectArchetype == NULL )
			{
				ObjectArchetype = InClass->GetDefaultObject();
			}
		}
		else if ( (InFlags&RF_ClassDefaultObject) != 0 )
		{
			// for the Object CDO, make sure that we do not use an archetype
			ObjectArchetype = NULL;
		}
#endif

		if( Cls )
		{
			ClassWithin		 = Cls->ClassWithin;
			ClassFlags       = Cls->ClassFlags & CLASS_Abstract;
			CastFlags        = Cls->ClassCastFlags;
			ClassConstructor = Cls->ClassConstructor;
			StaticClassConstructor = Cls->ClassStaticConstructor;
			StaticClassInitializer = Cls->ClassStaticInitializer;
			
			DefaultObject	 = Cls->GetDefaultsCount() ? Cls->GetDefaultObject() : NULL;
		}

		// Destroy the object.
		TGuardValue<UBOOL> IsReplacingObject(GIsReplacingObject,TRUE);
		TGuardValue<UBOOL> IsReplacingCDO(GIsAffectingClassDefaultObject,Obj->HasAnyFlags(RF_ClassDefaultObject));
		Obj->~UObject();

		check(GObjAvailable.Num() && GObjAvailable.Last()==Index);
		GObjAvailable.Pop();
	}

	if( (InFlags&RF_ClassDefaultObject) == 0 )
	{
		// If class is transient, objects must be transient.
		if ( (InClass->ClassFlags & CLASS_Transient) != 0 )
		{
            InFlags |= RF_Transient;
		}
	}
	else
	{
		// never call PostLoad on class default objects
		InFlags &= ~(RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
	}

	// If the class is marked PerObjectConfig, mark the object as needing per-instance localization.
	if ( InClass->HasAllClassFlags(CLASS_PerObjectConfig|CLASS_Localized) || ((InClass->ClassFlags & CLASS_PerObjectLocalized) != 0) )
	{
		InFlags |= RF_PerObjectLocalized;
	}

	// @note: clang warns about overwriting the vtable here, the (void*) cast allows it
	appMemzero( (void*)Obj, InClass->GetPropertiesSize() );

	// Set the base properties.
	Obj->Index			 = INDEX_NONE;
	Obj->HashNext		 = NULL;
	Obj->StateFrame      = NULL;
	Obj->_Linker		 = Linker;
	Obj->_LinkerIndex	 = LinkerIndex;
	Obj->Outer			 = InOuter;
	Obj->ObjectFlags	 = InFlags;
	Obj->Name			 = InName;
	Obj->Class			 = InClass;
	Obj->ObjectArchetype = ObjectArchetype;

	// Reassociate the object with it's linker.
	if(Linker)
	{
		check(Linker->ExportMap(LinkerIndex)._Object == NULL);
		Linker->ExportMap(LinkerIndex)._Object = Obj;

#if !NO_LOGGING && !FINAL_RELEASE && DO_LOG_SLOW
		FString ObjectName;
		if ( InOuter != NULL )
		{
			ObjectName = InOuter->GetPathName() + TEXT(".");
		}

		ObjectName += InName.ToString();
		debugfSlow( NAME_DevReplace, TEXT("Reassociating %s with %s serving %s"),*ObjectName,*Linker->GetName(),*Linker->LinkerRoot->GetPathName());
#endif
	}

	// Init the properties.
	UClass* BaseClass = Obj->HasAnyFlags(RF_ClassDefaultObject) ? InClass->GetSuperClass() : InClass;
	if ( BaseClass == NULL )
	{
		check(InClass==UObject::StaticClass());
		BaseClass = InClass;
	}

	INT DefaultsCount = ObjectArchetype
		? ObjectArchetype->GetClass()->GetPropertiesSize()
		: BaseClass->GetPropertiesSize();

	if ( InstanceGraph != NULL )
	{
		if ( InstanceGraph->IsInitialized() )
		{
			InstanceGraph->AddObjectPair(Obj, ObjectArchetype);
		}
		else
		{
			InstanceGraph->SetDestinationRoot(Obj);
		}
	}

	Obj->SafeInitProperties( (BYTE*)Obj, InClass->GetPropertiesSize(), BaseClass, (BYTE*)ObjectArchetype, DefaultsCount, Obj->HasAnyFlags(RF_NeedLoad) ? NULL : Obj, SubobjectRoot, InstanceGraph );

	// reset NetIndex after InitProperties so that the value from the template is ignored
	Obj->NetIndex = INDEX_NONE;
	Obj->SetNetIndex(OldNetIndex);

	// Add to global table.
	Obj->AddObject( Index );
	check(Obj->IsValid());

	// Restore class information if replacing native class.
	if( Cls )
	{
		Cls->ClassWithin				= ClassWithin;
		Cls->ClassFlags					|= ClassFlags;
		Cls->ClassCastFlags				|= CastFlags;
		Cls->ClassConstructor			= ClassConstructor;
		Cls->ClassStaticConstructor		= StaticClassConstructor;
		Cls->ClassStaticInitializer		= StaticClassInitializer;
		Cls->ClassDefaultObject			= DefaultObject;
	}

	// Mark objects created during async loading process (e.g. from within PostLoad or CreateExport) as async loaded so they 
	// cannot be found. This requires also keeping track of them so we can remove the async loading flag later one when we 
	// finished routing PostLoad to all objects.
	if( GIsAsyncLoading )
	{
		Obj->SetFlags( RF_AsyncLoading );
		GObjConstructedDuringAsyncLoading.AddItem(Obj);
	}

#if USE_MALLOC_PROFILER_DECODE_SCRIPT
	if( !GIsUCCMake )
	{
		GMallocProfiler->GetScriptCallStackDecoder()->EndAllocateObject();
	}
#endif

	// Success.
	return Obj;
}

/**
 * Create a new instance of an object.  The returned object will be fully initialized.  If InFlags contains RF_NeedsLoad (indicating that the object still needs to load its object data from disk), components
 * are not instanced (this will instead occur in PostLoad()).  The different between StaticConstructObject and StaticAllocateObject is that StaticConstructObject will also call the class constructor on the object
 * and instance any components.
 * 
 * @param	Class		the class of the object to create
 * @param	InOuter		the object to create this object within (the Outer property for the new object will be set to the value specified here).
 * @param	Name		the name to give the new object. If no value (NAME_None) is specified, the object will be given a unique name in the form of ClassName_#.
 * @param	SetFlags	the ObjectFlags to assign to the new object. some flags can affect the behavior of constructing the object.
 * @param	Template	if specified, the property values from this object will be copied to the new object, and the new object's ObjectArchetype value will be set to this object.
 *						If NULL, the class default object is used instead.
 * @param	Error		the output device to use for logging errors
 * @param	SubobjectRoot
 *						Only used to when duplicating or instancing objects; in a nested subobject chain, corresponds to the first object that is not a subobject.
 *						A value of INVALID_OBJECT for this parameter indicates that we are calling StaticConstructObject to duplicate or instance a non-subobject (which will be the subobject root for any subobjects of the new object)
 *						A value of NULL indicates that we are not instancing or duplicating an object.
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer to a fully initialized object of the specified class.
 */
UObject* UObject::StaticConstructObject
(
	UClass*			InClass,
	UObject*		InOuter								/*=GetTransientPackage()*/,
	FName			InName								/*=NAME_None*/,
	EObjectFlags	InFlags								/*=0*/,
	UObject*		InTemplate							/*=NULL*/,
	FOutputDevice*	Error								/*=GError*/,
	UObject*		SubobjectRoot						/*=NULL*/,
	FObjectInstancingGraph* InInstanceGraph				/*=NULL*/
)
{
	SCOPE_CYCLE_COUNTER(STAT_ConstructObject);

	check(Error);
#if !WITH_FACEFX
	if ((InOuter != NULL) && (GIsInitialLoad == FALSE) && (GObjBeginLoadCount == 0))
	{
		UPackage* SavePackage = Cast<UPackage>(InOuter->GetOutermost());
		if (SavePackage == NULL)
		{
			SavePackage = Cast<UPackage>(InOuter);
		}
		if (SavePackage && ((SavePackage->PackageFlags & PKG_ContainsFaceFXData) != 0))
		{
			appMsgf(AMT_OK, TEXT("Object creation failed.\nSource package %s contains FaceFX data.\nYou would NOT be allowed to save it!"), *(SavePackage->GetName()));
			return NULL;
		}
	}
#endif	//#if !WITH_FACEFX

	FObjectInstancingGraph* InstanceGraph = InInstanceGraph;
	if ( InstanceGraph == NULL && InClass->HasAnyClassFlags(CLASS_HasComponents) )
	{
		InstanceGraph = new FObjectInstancingGraph;
	}

	// Allocate the object.
	UObject* Result = StaticAllocateObject( InClass, InOuter, InName, InFlags, InTemplate, Error, NULL, SubobjectRoot, InstanceGraph );

	if( Result )
	{
		{
			TGuardValue<UBOOL> IsAffectingCDO(GIsAffectingClassDefaultObject,(InFlags&RF_ClassDefaultObject)!=0);

			// call the base UObject class constructor if the class is misaligned (i.e. a native class that is currently being recompiled)
			if ( !InClass->IsMisaligned() )
			{
				(*InClass->ClassConstructor)( Result );
			}
			else
			{
				(*UObject::StaticClass()->ClassConstructor)( Result );
			}
		}

		// if InFlags & RF_NeedLoad, it means that this object is being created either
		// by the linker, as a result of a call to CreateExport(), or that this object's
		// archetype has not yet been completely initialized (most likely not yet serialized)
		// In this case, components will be safely instanced once the archetype for the Result
		// object has been completely initialized.
		if( (InFlags & RF_NeedLoad) == 0 )
		{
			if (
				// if we're not running make
				!GIsUCCMake &&													

				// and InClass is PerObjectConfig
				InClass->HasAnyClassFlags(CLASS_PerObjectConfig) &&
				
				// and we're not creating a CDO or Archetype object
				(InFlags&(RF_ClassDefaultObject|RF_ArchetypeObject)) == 0 )		
			{
				Result->LoadConfig();
				Result->LoadLocalized();
			}

			if ( InClass->HasAnyClassFlags(CLASS_HasComponents) )
			{
				const UBOOL bIsDefaultObject = Result->HasAnyFlags(RF_ClassDefaultObject);

				if ( bIsDefaultObject == FALSE )
				{
					// if the caller hasn't disabled component instancing, instance the components for the new object now.
					if ( InstanceGraph->IsComponentInstancingEnabled() )
					{
						InClass->InstanceComponentTemplates((BYTE*)Result, (BYTE*)Result->GetArchetype(),
							Result->GetArchetype() ? Result->GetArchetype()->GetClass()->GetPropertiesSize() : 0,
							Result, InstanceGraph);
					}
				}
			}
		}
	}

	if ( InInstanceGraph == NULL && InstanceGraph != NULL )
	{
		delete InstanceGraph;
		InstanceGraph = NULL;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	FReplaceArchetypeParameters.
-----------------------------------------------------------------------------*/
/**
 * Constructor
 */
FReplaceArchetypeParameters::FReplaceArchetypeParameters( UObject* InNewArchetype )
: NewArchetype(InNewArchetype)
{
	checkSlow(InNewArchetype);
}

/*-----------------------------------------------------------------------------
	FChangeObjectClassParameters.
-----------------------------------------------------------------------------*/
/**
 * Constructor
 *
 * @param	InClass		will become the value of NewObjectClass.
 */
FChangeObjectClassParameters::FChangeObjectClassParameters( UClass* InClass )
: NewObjectClass(InClass), bAllowNonDerivedClassChange(FALSE)
{
	check(InClass);
}

/*-----------------------------------------------------------------------------
   Duplicating Objects.
-----------------------------------------------------------------------------*/

/**
 * Constructor - zero initializes all members
 */
FObjectDuplicationParameters::FObjectDuplicationParameters( UObject* InSourceObject, UObject* InDestOuter )
: SourceObject(InSourceObject), DestOuter(InDestOuter), DestName(NAME_None)
, FlagMask(RF_AllFlags), ApplyFlags(0), DestClass(NULL), bMigrateArchetypes(FALSE), CreatedObjects(NULL)
{
	checkSlow(SourceObject);
	checkSlow(DestOuter);
	checkSlow(SourceObject->IsValid());
	checkSlow(DestOuter->IsValid());
	DestClass = SourceObject->GetClass();
}

/**
 * Creates a copy of SourceObject using the Outer and Name specified, as well as copies of all objects contained by SourceObject.  
 * Any objects referenced by SourceOuter or RootObject and contained by SourceOuter are also copied, maintaining their name relative to SourceOuter.  Any
 * references to objects that are duplicated are automatically replaced with the copy of the object.
 *
 * @param	SourceObject	the object to duplicate
 * @param	RootObject		should always be the same value as SourceObject (unused)
 * @param	DestOuter		the object to use as the Outer for the copy of SourceObject
 * @param	DestName		the name to use for the copy of SourceObject
 * @param	FlagMask		a bitmask of EObjectFlags that should be propagated to the object copies.  The resulting object copies will only have the object flags
 *							specified copied from their source object.
 * @param	DestClass		optional class to specify for the destination object. MUST BE SERIALIZATION COMPATIBLE WITH SOURCE OBJECT!!!
 *
 * @return	the duplicate of SourceObject.
 *
 * @note: this version is deprecated in favor of StaticDuplicateObjectEx
 */
UObject* UObject::StaticDuplicateObject(UObject* SourceObject,UObject* RootObject,UObject* DestOuter,const TCHAR* DestName,EObjectFlags FlagMask/*=~0*/,UClass* DestClass/*=NULL*/, UBOOL bMigrateArchetypes/*=FALSE*/)
{
	FObjectDuplicationParameters Parameters(SourceObject, DestOuter);
	if ( DestName && appStrcmp(DestName,TEXT("")) )
	{
		Parameters.DestName = FName(DestName, FNAME_Add, TRUE);
	}

	if ( DestClass == NULL )
	{
		Parameters.DestClass = SourceObject->GetClass();
	}
	else
	{
		Parameters.DestClass = DestClass;
	}
	Parameters.FlagMask = FlagMask;
	Parameters.bMigrateArchetypes = bMigrateArchetypes;

	return StaticDuplicateObjectEx(Parameters);
}

UObject* UObject::StaticDuplicateObjectEx( FObjectDuplicationParameters& Parameters )
{
	// make sure the two classes are the same size, as this hopefully will mean they are serialization
	// compatible. It's not a guarantee, but will help find errors
	checkf( (	Align(Parameters.DestClass->GetPropertiesSize(), Parameters.DestClass->GetMinAlignment()) >= 
				Align(Parameters.SourceObject->GetClass()->GetPropertiesSize(), Parameters.SourceObject->GetClass()->GetMinAlignment()))
				|| (GUglyHackFlags & HACK_AllowDifferingClassSizesWhenDuplicatingObjects),
		TEXT("Source and destination class sizes differ.  Source: %s (%i)   Destination: %s (%i)"),
		*Parameters.SourceObject->GetClass()->GetName(), Align(Parameters.SourceObject->GetClass()->GetPropertiesSize(), Parameters.SourceObject->GetClass()->GetMinAlignment()),
		*Parameters.DestClass->GetName(), Align(Parameters.DestClass->GetPropertiesSize(), Parameters.DestClass->GetMinAlignment()));
	FObjectInstancingGraph InstanceGraph;

	// make sure we are not duplicating RF_DisregardForGC as this flag can only be set during initial load
	// also make sure we are not duplicating the RF_ClassDefaultObject flag as this can only be set on the real CDO
	Parameters.FlagMask &= ~(RF_RootSet|RF_DisregardForGC|RF_ClassDefaultObject);

	// disable object and component instancing while we're duplicating objects, as we're going to instance components manually a little further below
	InstanceGraph.EnableObjectInstancing(FALSE);
	InstanceGraph.EnableComponentInstancing(FALSE);

	// we set this flag so that the component instancing code doesn't think we're creating a new archetype, because when creating a new archetype,
	// the ObjectArchetype for instanced components is set to the ObjectArchetype of the source component, which in the case of duplication (or loading)
	// will be changing the archetype's ObjectArchetype to the wrong object (typically the CDO or something)
	InstanceGraph.SetLoadingObject(TRUE);

	UObject* DupRootObject = Parameters.DuplicationSeed.FindRef(Parameters.SourceObject);
	if ( DupRootObject == NULL )
	{
		DupRootObject = UObject::StaticConstructObject(	Parameters.DestClass,
														Parameters.DestOuter,
														Parameters.DestName,
														Parameters.ApplyFlags|Parameters.SourceObject->GetMaskedFlags(Parameters.FlagMask),
														Parameters.SourceObject->GetArchetype()->GetClass() == Parameters.DestClass
																? Parameters.SourceObject->GetArchetype()
																: NULL,
														GError,
														INVALID_OBJECT,
														&InstanceGraph
														);
	}

	TArray<BYTE>							ObjectData;
	TMap<UObject*,FDuplicatedObjectInfo*>	DuplicatedObjects;

	// if seed objects were specified, add those to the DuplicatedObjects map now
	if ( Parameters.DuplicationSeed.Num() > 0 )
	{
		for ( TMap<UObject*,UObject*>::TIterator It(Parameters.DuplicationSeed); It; ++It )
		{
			UObject* Src = It.Key();
			UObject* Dup = It.Value();
			checkSlow(Src);
			checkSlow(Dup);

			// create the DuplicateObjectInfo for this object
			FDuplicatedObjectInfo* Info = DuplicatedObjects.Set(Src,new FDuplicatedObjectInfo());
			Info->DupObject = Dup;
		}
	}

	FDuplicateDataWriter					Writer(DuplicatedObjects,ObjectData,Parameters.SourceObject,DupRootObject,Parameters.FlagMask,Parameters.ApplyFlags,&InstanceGraph);
	TArray<UObject*>						SerializedObjects;
	//UObject*								DupRoot = Writer.GetDuplicatedObject(Parameters.SourceObject);

	InstanceGraph.SetDestinationRoot(DupRootObject, DupRootObject->GetArchetype());
	while(Writer.UnserializedObjects.Num())
	{
		UObject*	Object = Writer.UnserializedObjects.Pop();
		Object->Serialize(Writer);
		SerializedObjects.AddItem(Object);
	};

	FDuplicateDataReader	Reader(DuplicatedObjects,ObjectData);
	for(INT ObjectIndex = 0;ObjectIndex < SerializedObjects.Num();ObjectIndex++)
	{
		FDuplicatedObjectInfo*	ObjectInfo = DuplicatedObjects.FindRef(SerializedObjects(ObjectIndex));
		check(ObjectInfo);
		if ( !SerializedObjects(ObjectIndex)->HasAnyFlags(RF_ClassDefaultObject) )
		{
			ObjectInfo->DupObject->Serialize(Reader);
		}
		else
		{
			// if the source object was a CDO, then transient property values were serialized by the FDuplicateDataWriter
			// and in order to read those properties out correctly, we'll need to enable defaults serialization on the
			// reader as well.
			Reader.StartSerializingDefaults();
			ObjectInfo->DupObject->Serialize(Reader);
			Reader.StopSerializingDefaults();
		}
	}

/*
	@note ronp: this should be completely unnecessary since all subobject and component references should have been handled by the duplication archives.
	for(TMap<UObject*,FDuplicatedObjectInfo*>::TIterator It(DuplicatedObjects);It;++It)
	{
		UObject* OrigObject = It.Key();
		FDuplicatedObjectInfo* DupObjectInfo = It.Value();
		UObject* Duplicate = DupObjectInfo->DupObject;

		// don't include any objects which were included in the duplication seed map in the instance graph, as the "duplicate" of these objects
		// may not necessarily be the object that is supposed to be its archetype (the caller can populate the duplication seed map with any objects they wish)
		// and the DuplicationSeed is only used for preserving inter-object references, not for object graphs in SCO
		if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
		{
			UComponent* DuplicateComponent = Cast<UComponent>(Duplicate);
			if ( DuplicateComponent != NULL )
			{
				InstanceGraph.AddComponentPair(DuplicateComponent->GetArchetype<UComponent>(), DuplicateComponent);
			}
			else
			{
				InstanceGraph.AddObjectPair(Duplicate);
			}
		}
	}
*/

	InstanceGraph.EnableComponentInstancing(TRUE);
	InstanceGraph.EnableObjectInstancing(TRUE);

	for(TMap<UObject*,FDuplicatedObjectInfo*>::TIterator It(DuplicatedObjects);It;++It)
	{
		UObject* OrigObject = It.Key();
		FDuplicatedObjectInfo*	DupObjectInfo = It.Value();

		// don't include any objects which were included in the duplication seed map in the instance graph, as the "duplicate" of these objects
		// may not necessarily be the object that is supposed to be its archetype (the caller can populate the duplication seed map with any objects they wish)
		// and the DuplicationSeed is only used for preserving inter-object references, not for object graphs in SCO and we don't want to call PostDuplicate/PostLoad
		// on them as they weren't actually duplicated
		if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
		{
			UObject* DupObjectArchetype = DupObjectInfo->DupObject->GetArchetype();

			// the InstanceGraph's ComponentMap may not contain the correct arc->inst mappings
			// if there were subobjects of the same class inside DupObject, so make sure that the mappings
			// used for this call are the right ones.
			//@note ronp - this should be completely unnecessary since all component references should have been fixed up during
			// duplication
/*
			DupObjectInfo->DupObject->GetClass()->InstanceComponentTemplates(
				(BYTE*)DupObjectInfo->DupObject,
				(BYTE*)DupObjectArchetype,
				DupObjectArchetype ? DupObjectArchetype->GetClass()->GetPropertiesSize() : NULL,
				DupObjectInfo->DupObject,&InstanceGraph);
*/

			DupObjectInfo->DupObject->PostDuplicate();
			DupObjectInfo->DupObject->PostLoad();
		}
	}

	if ( Parameters.bMigrateArchetypes )
	{
		for(TMap<UObject*,FDuplicatedObjectInfo*>::TIterator It(DuplicatedObjects);It;++It)
		{
			UObject* OrigObject = It.Key();
			FDuplicatedObjectInfo* DupInfo = It.Value();

			// don't change the archetypes for any objects which were included in the duplication seed map, as the "duplicate" of these objects
			// may not necessarily be the object that is supposed to be its archetype (the caller can populate the duplication seed map with any objects they wish)
			if ( Parameters.DuplicationSeed.Find(OrigObject) == NULL )
			{
				DupInfo->DupObject->SetArchetype(OrigObject);
				UComponent* ComponentDup = Cast<UComponent>(DupInfo->DupObject);
				if ( ComponentDup != NULL && ComponentDup->TemplateName == NAME_None && OrigObject->IsTemplate() )
				{
					ComponentDup->TemplateName = CastChecked<UComponent>(OrigObject)->TemplateName;
				}
			}
		}
	}

	// if the caller wanted to know which objects were created, do that now
	if ( Parameters.CreatedObjects != NULL )
	{
		// note that we do not clear the map first - this is to allow callers to incrementally build a collection
		// of duplicated objects through multiple calls to StaticDuplicateObject

		// now add each pair of duplicated objects;
		// NOTE: we don't check whether the entry was added from the DuplicationSeed map, so this map
		// will contain those objects as well.
		for ( TMap<UObject*,FDuplicatedObjectInfo*>::TIterator It(DuplicatedObjects); It; ++It )
		{
			UObject* Src = It.Key();
			UObject* Dst = It.Value()->DupObject;

			// don't include any objects which were in the DuplicationSeed map, as CreatedObjects should only contain the list
			// of objects actually created during this call to SDO
			if ( Parameters.DuplicationSeed.Find(Src) == NULL )
			{
				Parameters.CreatedObjects->Set(Src, Dst);
			}
		}
	}

	// cleanup our DuplicatedObjects map
	for ( TMap<UObject*,FDuplicatedObjectInfo*>::TIterator It(DuplicatedObjects); It; ++It )
	{
		delete It.Value();
	}

	//return DupRoot;
	return DupRootObject;
}

/**
 * Creates a new archetype based on this UObject.  The archetype's property values will match
 * the current values of this UObject.
 *
 * @param	ArchetypeName			the name for the new class
 * @param	ArchetypeOuter			the outer to create the new class in (package?)
 * @param	AlternateArchetype		if specified, is set as the ObjectArchetype for the newly created archetype, after the new archetype
 *									is initialized against "this".  Should only be specified in cases where you need the new archetype to
 *									inherit the property values of this object, but don't want this object to be the new archetype's ObjectArchetype.
 * @param	InstanceGraph			contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer to a UObject which has values identical to this object
 */
UObject* UObject::CreateArchetype( const TCHAR* ArchetypeName, UObject* ArchetypeOuter, UObject* AlternateArchetype/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ )
{
	check(ArchetypeName);
	check(ArchetypeOuter);

	EObjectFlags ArchetypeObjectFlags = RF_Public | RF_ArchetypeObject;
	
	// Archetypes residing directly in packages need to be marked RF_Standalone though archetypes that are part of prefabs
	// should not have this flag in order for the prefabs to be deletable via the generic browser.
	if( ArchetypeOuter->IsA(UPackage::StaticClass()) )
	{
		ArchetypeObjectFlags |= RF_Standalone;
	}

	UObject* ArchetypeObject = StaticConstructObject(GetClass(), ArchetypeOuter, ArchetypeName, ArchetypeObjectFlags, this, GError, INVALID_OBJECT, InstanceGraph);
	check(ArchetypeObject);

	UObject* NewArchetype = AlternateArchetype == NULL
		? GetArchetype()
		: AlternateArchetype;

	check(NewArchetype);
	// make sure the alternate archetype has the same class
	check(NewArchetype->GetClass()==GetClass());

	if ( NewArchetype != ArchetypeObject )
	{
		ArchetypeObject->SetArchetype(NewArchetype);
	}
	return ArchetypeObject;
}

/**
 *	Update the ObjectArchetype of this UObject based on this UObject's properties.
 */
void UObject::UpdateArchetype()
{
	DWORD OldUglyHackFlags = GUglyHackFlags;
	GUglyHackFlags |= HACK_UpdateArchetypeFromInstance;

	FObjectInstancingGraph InstanceGraph(ObjectArchetype, this);

	// we're going to recreate this object's archetype inplace which will reinitialize the archetype against this object, replacing the values for all its component properties
	// to point to components owned by this object.  

	//@todo components: determine if this is actually necessary...
	// what we're basically doing here is generating a list of all subobjects contained by this instance, which we'll later iterate through and call InstanceComponents on
	// the idea being that nested subobjects instanced as a result of calling CreateArchetype() won't have the correct components....need to verify that this is indeed the case.
	TArray<UObject*> SubobjectInstances;
	FArchiveObjectReferenceCollector SubobjectCollector(
		&SubobjectInstances,		//	InObjectArray
		this,						//	LimitOuter
		FALSE,						//	bRequireDirectOuter
		TRUE,						//	bIgnoreArchetypes
		TRUE,						//	bSerializeRecursively
		FALSE						//	bShouldIgnoreTransient
		);
	Serialize( SubobjectCollector );

	UObject* NewArchetype = CreateArchetype( *ObjectArchetype->GetName(), ObjectArchetype->GetOuter(), ObjectArchetype->GetArchetype(), &InstanceGraph);

	// now instance components for the new archetype
	//@todo components: seems like this should happen already because StaticConstructObject() calls InstanceComponents...don't think I need this, but it certainly doesn't
	// hurt anything for it to be here.
	NewArchetype->GetClass()->InstanceComponentTemplates( (BYTE*)NewArchetype, (BYTE*)this, GetClass()->GetPropertiesSize(), NewArchetype, &InstanceGraph );

	// now instance components for subobjects and components contained by our archetype
	TArray<UObject*> ArchetypeSubobjects;
	InstanceGraph.RetrieveObjectInstances(NewArchetype, ArchetypeSubobjects, TRUE);
	for ( INT ObjIndex = 0; ObjIndex < ArchetypeSubobjects.Num(); ObjIndex++ )
	{
		UObject* Subobject = ArchetypeSubobjects(ObjIndex);
		UObject* Instance = InstanceGraph.GetDestinationObject(Subobject,TRUE);

		Subobject->GetClass()->InstanceComponentTemplates((BYTE*)Subobject, (BYTE*)Instance, Instance->GetClass()->GetPropertiesSize(), Subobject, &InstanceGraph );
	}

	check(NewArchetype == ObjectArchetype);

	GUglyHackFlags = OldUglyHackFlags;
}


/**
 * Finds the most-derived class which is a parent of both TestClass and this object's class.
 *
 * @param	TestClass	the class to find the common base for
 */
const UClass* UObject::FindNearestCommonBaseClass( const UClass* TestClass ) const
{
	const UClass* Result = NULL;

	if ( TestClass != NULL )
	{
		const UClass* CurrentClass = GetClass();

		// early out if it's the same class or one is the parent of the other
		// (the check for TestClass->IsChildOf(CurrentClass) returns TRUE if TestClass == CurrentClass
		if ( TestClass->IsChildOf(CurrentClass) )
		{
			Result = CurrentClass;
		}
		else if ( CurrentClass->IsChildOf(TestClass) )
		{
			Result = TestClass;
		}
		else
		{
			// find the nearest parent of TestClass which is also a parent of CurrentClass
			for ( UClass* Cls = TestClass->GetSuperClass(); Cls; Cls = Cls->GetSuperClass() )
			{
				if ( CurrentClass->IsChildOf(Cls) )
				{
					Result = Cls;
					break;
				}
			}
		}
	}

	// at this point, Result should only be NULL if TestClass is NULL
	checkfSlow(Result != NULL || TestClass == NULL, TEXT("No common base class found for object '%s' with TestClass '%s'"), *GetFullName(), *TestClass->GetFullName());
	return Result;
}

/*-----------------------------------------------------------------------------
	Importing and exporting.
-----------------------------------------------------------------------------*/

//
// Import an object from a file.
//
void UObject::ParseParms( const TCHAR* Parms )
{
	if( !Parms )
		return;
	for( TFieldIterator<UProperty> It(GetClass()); It; ++It )
	{
		if( It->GetOuter()!=UObject::StaticClass() )
		{
			FString Value;
			if( Parse(Parms,*(FString(*It->GetName())+TEXT("=")),Value) )
				It->ImportText( *Value, (BYTE*)this + It->Offset, PPF_Localized, this );
		}
	}
}

// Marks the package containing this object as needing to be saved.
void UObject::MarkPackageDirty( UBOOL InDirty ) const
{
	// since transient objects will never be saved into a package, there is no need to mark a package dirty
	// if we're transient
	if ( !HasAnyFlags(RF_Transient) )
	{
		UPackage* Package = Cast<UPackage>(GetOutermost());

		if( Package != NULL )
		{
// 			if ( !Package->IsDirty() && Package != UObject::GetTransientPackage() )
// 			{
// 				debugf( TEXT( "%s dirtied by: %s"), *Package->GetPathName(), *GetPathName() );
// 			}


			// It is against policy to dirty a map or package during load in the Editor, to enforce this policy
			// we explicitly disable the ability to dirty a package or map during load.  Commandlets can still
			// set the dirty state on load.
			if( !GIsEditor || GIsUCC || 
				(GIsEditor && !GIsEditorLoadingMap))
			{
				Package->SetDirtyFlag(InDirty);

				if( GIsEditor											// Only fire the callback in editor mode
					&& !(Package->PackageFlags & PKG_ContainsScript)	// Skip script packages
					&& !(Package->PackageFlags & PKG_PlayInEditor)		// Skip packages for PIE
					&& GetTransientPackage() != Package )				// Skip the transient package
				{
					// Package is changing dirty state, let the editor know so we may prompt for source control checkout
					GCallbackEvent->Send( CALLBACK_PackageModified, Package );
				}
			}
		}
	
	}
}

/*----------------------------------------------------------------------------
	Description functions.
----------------------------------------------------------------------------*/

/** 
 * Returns detailed info to populate listview columns (defaults to the one line description)
 */
FString UObject::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	if( InIndex == 0 )
	{
		Description = GetDesc();
	}
	return( Description );
}

/*----------------------------------------------------------------------------
	Language functions.
----------------------------------------------------------------------------*/

const TCHAR* UObject::GetLanguage()
{
	return GLanguage;
}
void UObject::SetLanguage( const TCHAR* LangExt, UBOOL bReloadObjects )
{
	if ( appStricmp(LangExt,GLanguage) != 0 )
	{
		appStrcpy( GLanguage, *FString(LangExt).ToUpper() );

		appStrcpy( GNone,  *LocalizeGeneral( TEXT("None"),  TEXT("Core")) );
		appStrcpy( GTrue,  *LocalizeGeneral( TEXT("True"),  TEXT("Core")) );
		appStrcpy( GFalse, *LocalizeGeneral( TEXT("False"), TEXT("Core")) );
		appStrcpy( GYes,   *LocalizeGeneral( TEXT("Yes"),   TEXT("Core")) );
		appStrcpy( GNo,    *LocalizeGeneral( TEXT("No"),    TEXT("Core")) );

		if (bReloadObjects == TRUE)
		{
			for( FObjectIterator It; It; ++It )
			{
				It->LanguageChange();
			}
		}
	}
}

/**
 * @return TRUE if this object should be in the name hash, FALSE if it should be excluded
 */
UBOOL UObject::IsNameHashed(void)
{
	// Classes, default objects, packages, etc. need to be findable without knowing the outer
	return Class->HasAnyCastFlag(CASTCLASS_UClass) ||
		Class->HasAnyCastFlag(CASTCLASS_UEnum) ||
		Class->HasAnyCastFlag(CASTCLASS_UScriptStruct) ||
		HasAnyFlags(RF_ClassDefaultObject) ||
		Outer == NULL;
}

/**
* Clear out the PackageNameToFileMapping map
*/
void UObject::ClearPackageToFileMapping()
{
	PackageNameToFileMapping.Empty();
}

/**
* Return the PackageNameToFileMapping map
*/
TMap<FName, FName>* UObject::GetPackageNameToFileMapping()
{
	return &PackageNameToFileMapping;
}

IMPLEMENT_CLASS(UObject);
