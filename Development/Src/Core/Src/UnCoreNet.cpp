/*=============================================================================
	UnCoreNet.cpp: Core networking support.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FPackageInfo implementation.
-----------------------------------------------------------------------------*/

//
// FPackageInfo constructor.
//
FPackageInfo::FPackageInfo(UPackage* Package)
:	PackageName		(Package != NULL ? Package->GetFName() : NAME_None)
,	Parent			(Package)
,	Guid			(Package != NULL ? Package->GetGuid() : FGuid(0,0,0,0))
,	ObjectBase		( INDEX_NONE )
,	ObjectCount		( 0 )
,	LocalGeneration	(Package != NULL ? Package->GetGenerationNetObjectCount().Num() : 0)
,	RemoteGeneration( 0 )
,	PackageFlags	(Package != NULL ? Package->PackageFlags : 0)
,	ForcedExportBasePackageName(NAME_None)
,	FileName		(Package != NULL ? Package->FileName : NAME_None)
{
	// if we have a pacakge, find it's source file so that we can send the extension of the file
	if (Package != NULL)
	{
		FFilename PackageFile;
		if (GPackageFileCache->FindPackageFile(*Package->GetName(), NULL, PackageFile, NULL))
		{
			Extension = PackageFile.GetExtension();
		}
	}
}

//
// FPackageInfo serializer.
//
FArchive& operator<<( FArchive& Ar, FPackageInfo& I )
{
	return Ar << I.Parent;
}

/*-----------------------------------------------------------------------------
	FClassNetCache implementation.
-----------------------------------------------------------------------------*/

FClassNetCache::FClassNetCache()
{}
FClassNetCache::FClassNetCache( UClass* InClass )
: Class( InClass )
{}

/*-----------------------------------------------------------------------------
	UPackageMap implementation.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UPackageMap::StaticConstructor()
{
	UClass* TheClass = GetClass();
	const DWORD SkipIndexIndex = TheClass->EmitStructArrayBegin( STRUCT_OFFSET( UPackageMap, List ), sizeof(FPackageInfo) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( FPackageInfo, Parent ) );
	TheClass->EmitStructArrayEnd( SkipIndexIndex );
}

void UPackageMap::Copy( UPackageMap* Other )
{
	List = Other->List;
	PackageListMap = Other->PackageListMap;
}

UBOOL UPackageMap::SerializeName(FArchive& Ar, FName& Name)
{
	if (Ar.IsLoading())
	{
		BYTE bHardcoded = 0;
		Ar.SerializeBits(&bHardcoded, 1);
		if (bHardcoded)
		{
			// replicated by hardcoded index
			DWORD NameIndex;
			Ar.SerializeInt(NameIndex, MAX_NETWORKED_HARDCODED_NAME + 1);
			Name = EName(NameIndex);
			// hardcoded names never have a Number
		}
		else
		{
			// replicated by string
			FString InString;
			INT InNumber;
			Ar << InString << InNumber;
			Name = FName(*InString, InNumber);
		}
	}
	else if (Ar.IsSaving())
	{
		BYTE bHardcoded = Name.GetIndex() <= MAX_NETWORKED_HARDCODED_NAME;
		Ar.SerializeBits(&bHardcoded, 1);
		if (bHardcoded)
		{
			// send by hardcoded index
			checkSlow(Name.GetNumber() <= 0); // hardcoded names should never have a Number
			DWORD NameIndex = DWORD(Name.GetIndex());
			Ar.SerializeInt(NameIndex, MAX_NETWORKED_HARDCODED_NAME + 1);
		}
		else
		{
			// send by string
			FString OutString = Name.GetNameString();
			INT OutNumber = Name.GetNumber();
			Ar << OutString << OutNumber;
		}
	}
	return TRUE;
}
UBOOL UPackageMap::CanSerializeObject( UObject* Obj )
{
	appErrorf(TEXT("Unexpected UPackageMap::CanSerializeObject"));
	return 1;
}
UBOOL UPackageMap::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj )
{
	appErrorf(TEXT("Unexpected UPackageMap::SerializeObject"));
	return 1;
}

//
// Get a package map's net cache for a class.
//
FClassNetCache* UPackageMap::GetClassNetCache( UClass* Class )
{
	FClassNetCache* Result = ClassFieldIndices.FindRef(Class);
	if( !Result && SupportsObject(Class) )
	{
		Result                       = ClassFieldIndices.Set( Class, new FClassNetCache(Class) );
		Result->Super                = NULL;
		Result->RepConditionCount    = 0;
		Result->FieldsBase           = 0;
		if( Class->GetSuperClass() )
		{
			Result->Super		         = GetClassNetCache(Class->GetSuperClass());
			Result->RepProperties        = Result->Super->RepProperties;
			Result->RepConditionCount    = Result->Super->RepConditionCount;
			Result->FieldsBase           = Result->Super->GetMaxIndex();
		}

		Result->Fields.Empty( Class->NetFields.Num() );
		for( INT i=0; i<Class->NetFields.Num(); i++ )
		{
			// Add sandboxed items to net cache.
			UField* Field = Class->NetFields(i);
			if( SupportsObject(Field ) )
			{
				INT ConditionIndex = INDEX_NONE;
				INT ThisIndex      = Result->GetMaxIndex();
                UProperty* ItP     = Cast<UProperty>(Field,CLASS_IsAUProperty);
				if( ItP )
				{
					ConditionIndex = Result->RepConditionCount++;
				}
				new(Result->Fields)FFieldNetCache( Field, ThisIndex, ConditionIndex );
			}
		}

		Result->Fields.Shrink();
		for( TArray<FFieldNetCache>::TIterator It(Result->Fields); It; ++It )
		{
			Result->FieldMap.Set( It->Field, &*It );
		}

		for( TArray<FFieldNetCache>::TIterator It(Result->Fields); It; ++It )
		{
            UProperty* P = Cast<UProperty>(It->Field,CLASS_IsAUProperty);
			if( P )
			{
				if( It->ConditionIndex==INDEX_NONE )
				{
					It->ConditionIndex = Result->GetFromField(P)->ConditionIndex;
				}
				if( !(P->GetOwnerClass()->ClassFlags & CLASS_NativeReplication) )
				{
					Result->RepProperties.AddItem(&*It);
				}
			}
		}
	}
	return Result;
}

/** remove the cached field to index mappings for the given class (usually, because the class is being GC'ed) */
void UPackageMap::RemoveClassNetCache(UClass* Class)
{
	FClassNetCache* CacheToRemove = NULL;
	if (ClassFieldIndices.RemoveAndCopyValue(Class, CacheToRemove))
	{
		delete CacheToRemove;
	}
}

//
// Compute mapping info.
//
void UPackageMap::Compute()
{
	PackageListMap.Reset();
	DWORD MaxObjectIndex = 0;
	for (INT i = 0; i < List.Num(); i++)
	{
		FPackageInfo& Info = List(i);
		Info.ObjectBase = MaxObjectIndex;
		
		// update ObjectCount if both sides have loaded the package
		if (Info.RemoteGeneration > 0 && Info.Parent != NULL)
		{
			if (Min<INT>(Info.RemoteGeneration, Info.LocalGeneration) - 1 < Info.Parent->GetGenerationNetObjectCount().Num())
			{
				// we can only update the ObjectCount if the package is loaded
				if (Info.LocalGeneration - 1 < Info.Parent->GetGenerationNetObjectCount().Num())
				{
					Info.ObjectCount = Info.Parent->GetNetObjectCount(Info.LocalGeneration - 1);
					if (Info.RemoteGeneration < Info.LocalGeneration)
					{
						Info.ObjectCount = Min(Info.ObjectCount, Info.Parent->GetNetObjectCount(Info.RemoteGeneration - 1));
					}
				}
				else
				{
					Info.ObjectCount = Info.Parent->GetNetObjectCount(Info.RemoteGeneration - 1);
				}
			}
			// add to PackageListMap for quick lookup in ObjectToIndex()
			PackageListMap.Set(Info.Parent->GetFName(), i);
		}
		MaxObjectIndex += Info.ObjectCount;
		if (MaxObjectIndex > MAX_OBJECT_INDEX)
		{
			debugf(TEXT("Exceeded maximum of %u net serializable objects, listing packagemap:"), MAX_OBJECT_INDEX);
			LogDebugInfo(*GLog);
			appErrorf(TEXT("Exceeded maximum of %u net serializable objects"), MAX_OBJECT_INDEX);
		}
	}
}

/** logs debug info (package list, etc) to the specified output device
 * @param Ar - the device to log to
 */
void UPackageMap::LogDebugInfo(FOutputDevice& Ar)
{
	for (INT i = 0; i < List.Num(); i++)
	{
		Ar.Logf( TEXT("      Package %i: Name - %s, LocalGeneration - %i, RemoteGeneration - %i, BaseIndex - %i, ObjectCount - %i"),
					i, *List(i).PackageName.ToString(), List(i).LocalGeneration, List(i).RemoteGeneration, List(i).ObjectBase, List(i).ObjectCount );
	}
}

/** adds all packages from UPackage::GetNetPackages() to the package map */
void UPackageMap::AddNetPackages()
{
	// clear everything
	List.Reset();
	PackageListMap.Reset();

	const TArray<UPackage*> NetPackages = UPackage::GetNetPackages();
	for (INT i = 0; i < NetPackages.Num(); i++)
	{
		new(List) FPackageInfo(NetPackages(i));
	}

	Compute();
}

/** adds a single package to the package map */
INT UPackageMap::AddPackage(UPackage* Package)
{
	// make sure this package isn't already in the map
	for (INT i = 0; i < List.Num(); i++)
	{
		if (List(i).Parent == Package)
		{
			// do nothing
			return i;
		}
		else if (List(i).PackageName == Package->GetFName() && List(i).Guid == Package->GetGuid())
		{
			// there is an entry, but it's not hooked up to the UPackage reference
			List(i).Parent = Package;
			return i;
		}
	}

	new(List) FPackageInfo(Package);

	Compute();
	return (List.Num() - 1);
}

/** removes a package from the package map
 * @param Package the package to remove
 * @param bAllowEntryDeletion (optional) whether to actually delete the entry in List or to leave the entry around to maintain indices
 */
void UPackageMap::RemovePackage(UPackage* Package, UBOOL bAllowEntryDeletion)
{
	INT* Found = PackageListMap.Find(Package->GetFName());
	INT Index = INDEX_NONE;
	if (Found != NULL)
	{
		Index = *Found;
		PackageListMap.Remove(Package->GetFName());
	}
	else
	{
		// we need to fall back to manually iterating the List as PackageListMap only contains packages that have been fully synchronized
		for (INT i = 0; i < List.Num(); i++)
		{
			if (List(i).Parent == Package)
			{
				Index = i;
				break;
			}
		}
	}
	if (Index != INDEX_NONE)
	{
		if (bAllowEntryDeletion)
		{
			// actually remove the entry
			List.Remove(Index);
			if (PackageListMap.Num() > 0)
			{
				Compute();
			}
		}
		else
		{
			// dissociate the Package reference
			List(Index).Parent = NULL;
			List(Index).RemoteGeneration = 0;
		}
	}
}

/** removes a package from the package map by GUID instead of by package reference */
void UPackageMap::RemovePackageByGuid(const FGuid& Guid)
{
	for (INT i = 0; i < List.Num(); i++)
	{
		if (List(i).Guid == Guid)
		{
			UPackage* Package = List(i).Parent;
			if (Package != NULL)
			{
				List(i).Parent = NULL;
				List(i).RemoteGeneration = 0;
				PackageListMap.Remove(Package->GetFName());
				return;
			}
		}
	}
}

/** removes the passed in package only if both sides have completely sychronized it
 * @param Package the package to remove
 * @return TRUE if the package was removed or was not in the package map, FALSE if it was not removed because it has not been fully synchronized
 */
UBOOL UPackageMap::RemovePackageOnlyIfSynced(UPackage* Package)
{
	INT* Found = PackageListMap.Find(Package->GetFName());
	INT Index = INDEX_NONE;
	if (Found != NULL)
	{
		Index = *Found;
	}
	else
	{
		// we need to fall back to manually iterating the List as PackageListMap only contains packages that have been fully synchronized
		for (INT i = 0; i < List.Num(); i++)
		{
			if (List(i).Parent == Package)
			{
				Index = i;
				break;
			}
		}
	}
	if (Index != INDEX_NONE)
	{
		if (List(Index).RemoteGeneration == 0)
		{
			// other side has not synchronized this package
			return FALSE;
		}
		else
		{
			// dissociate the Package reference
			List(Index).Parent = NULL;
			List(Index).RemoteGeneration = 0;
			if (Found != NULL)
			{
				PackageListMap.Remove(Package->GetFName());
			}
			return TRUE;
		}
	}
	else
	{
		// package was not found at all
		return TRUE;
	}
}

/** adds a package info for a package that may or may not be loaded
 * if an entry for the given package already exists, updates it
 * @param Info the info to add
 */
void UPackageMap::AddPackageInfo(const FPackageInfo& Info)
{
	// if this is a duplicate entry, update the old one
	for (INT i = 0; i < List.Num(); i++)
	{
		if (List(i).PackageName == Info.PackageName && List(i).Guid == Info.Guid)
		{
			List(i).Parent = Info.Parent;
			List(i).RemoteGeneration = Info.RemoteGeneration;
			List(i).LocalGeneration = Info.LocalGeneration;
			Compute();
			return;
		}
	}

	// add new entry
	List.AddItem(Info);
	Compute();
}

//
// Mapping functions.
//
INT UPackageMap::ObjectToIndex( UObject* Object )
{
	if (Object != NULL && Object->NetIndex != INDEX_NONE)
	{
		INT* Found = PackageListMap.Find(Object->GetOutermost()->GetFName());
		if (Found != NULL)
		{
			FPackageInfo& Info = List(*Found);
			if (Object->NetIndex < Info.ObjectCount)
			{
				return Info.ObjectBase + Object->NetIndex;
			}
		}
	}
	return INDEX_NONE;
}

UBOOL UPackageMap::SupportsObject( UObject* Object )
{
	return ObjectToIndex( Object ) != INDEX_NONE;
}
UBOOL UPackageMap::SupportsPackage( UObject* InOuter )
{
	for( INT i=0; i<List.Num(); i++ )
		if( List(i).Parent == InOuter )
			return 1;
	return 0;
}

UObject* UPackageMap::IndexToObject( INT InIndex, UBOOL bLoad )
{
	if (InIndex >= 0)
	{
		for (INT i = 0; i < List.Num(); i++)
		{
			FPackageInfo& Info = List(i);
			if (InIndex < Info.ObjectCount)
			{
				if (Info.Parent != NULL)
				{
					UObject* Result = Info.Parent->GetNetObjectAtIndex(InIndex);
					if (Result != NULL)
					{
						return Result;
					}
					else if (bLoad)
					{
						//@fixme: in the case where the client has a newer version of a conformed package with some objects removed
						//		this code causes unnecessary flushing of async loading as it will be impossible to actually load the object requested
#if CONSOLE	|| DEDICATED_SERVER
						if( GUseSeekFreeLoading )
						{
							// we can't load the linker from disk using seekfree loading because the indices don't match up to the seekfree packages
							// so we just flush async loading and see if that works
							FlushAsyncLoading();
							Result = Info.Parent->GetNetObjectAtIndex(InIndex);
							if (Result != NULL)
							{
								debugf(NAME_Warning, TEXT("Flushed async loading to find replicated reference to '%s'"), *Result->GetFullName());
							}
							else
							{
								debugf(NAME_Warning, TEXT("Received non-loaded object at index %i in package %s"), InIndex, *Info.PackageName.ToString());
							}
						}
						else
#endif
						{
							UBOOL bWasAsyncLoading = IsAsyncLoading();
							UObject::BeginLoad();
							ULinkerLoad* Linker = GetPackageLinker(NULL, *Info.PackageName.ToString(), LOAD_None, NULL, &Info.Guid);
							Result = (Linker != NULL) ? Result = Linker->CreateExport(InIndex) : NULL;
							if (bWasAsyncLoading)
							{
								debugf(NAME_Warning, TEXT("Flushed async loading to load replicated reference to '%s'"), *Result->GetFullName());
							}
							UObject::EndLoad();
						}
						return Result;
					}
					else
					{
						return NULL;
					}
				}
				else
				{
					debugf(NAME_Warning, TEXT("Received index %i for removed/unloaded package %s"), InIndex, *Info.PackageName.ToString());
					return NULL;
				}
			}
			InIndex -= Info.ObjectCount;
		}
	}
	return NULL;
}
void UPackageMap::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << List;
}
void UPackageMap::FinishDestroy()
{
	for( TMap<UClass*, FClassNetCache*>::TIterator It(ClassFieldIndices); It; ++It )
	{
		// if this assertion crashes, memory is being leaked due to FClassNetCaches not being cleaned up for classes that are destroyed
		check(It.Key()->IsValid());
		delete It.Value();
	}
	Super::FinishDestroy();
}
IMPLEMENT_CLASS(UPackageMap);

