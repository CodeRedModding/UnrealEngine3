/*=============================================================================
	UnCoreNet.cpp: Core networking support.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FPackageInfo implementation.
-----------------------------------------------------------------------------*/

//
// FPackageInfo constructor.
//
FPackageInfo::FPackageInfo( ULinkerLoad* InLinker )
:	Linker			( InLinker )
,	Parent			( InLinker ? InLinker->LinkerRoot : NULL )
,	Guid			( InLinker ? InLinker->Summary.Guid : FGuid(0,0,0,0) )
,	FileSize		( InLinker ? InLinker->Loader->TotalSize() : 0 )
,	DownloadSize	( InLinker ? InLinker->Loader->TotalSize() : 0 )
,	PackageFlags	( InLinker ? InLinker->Summary.PackageFlags : 0 )
,	ObjectBase		( INDEX_NONE )
,	ObjectCount		( INDEX_NONE )
,	NameBase		( INDEX_NONE )
,	NameCount		( INDEX_NONE )
,	LocalGeneration	( 0 )
,	RemoteGeneration( 0 )
,	URL				()
{
	if( InLinker && *InLinker->Filename && (InLinker->Summary.PackageFlags & PKG_AllowDownload) )
		URL = *InLinker->Filename;
}

//
// FPackageInfo serializer.
//
FArchive& operator<<( FArchive& Ar, FPackageInfo& I )
{
	return Ar << I.Parent << I.Linker;
}

/*-----------------------------------------------------------------------------
	FFieldNetCache implementation.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FFieldNetCache& F )
{
	return Ar << F.Field;
}

/*-----------------------------------------------------------------------------
	FClassNetCache implementation.
-----------------------------------------------------------------------------*/

FClassNetCache::FClassNetCache()
{}
FClassNetCache::FClassNetCache( UClass* InClass )
: Class( InClass )
{}
FArchive& operator<<( FArchive& Ar, FClassNetCache& Cache )
{
	return Ar << Cache.Class << Cache.Fields;
	if( Cache.Super )
		Ar << *Cache.Super;
}

/*-----------------------------------------------------------------------------
	UPackageMap implementation.
-----------------------------------------------------------------------------*/

//
// General.
//
void UPackageMap::Copy( UPackageMap* Other )
{
	List              = Other->List;
	LinkerMap         = Other->LinkerMap;
	MaxObjectIndex    = Other->MaxObjectIndex;
	MaxNameIndex      = Other->MaxNameIndex;
	ClassFieldIndices = Other->ClassFieldIndices;
	NameIndices       = Other->NameIndices;
}

void UPackageMap::CopyLinkers( UPackageMap* Other )
{
	for(INT i=0; i < Other->List.Num(); i++)
		if( Other->List(i).Linker )
			AddLinker( Other->List(i).Linker );
	Compute();
}

UBOOL UPackageMap::SerializeName( FArchive& Ar, FName& Name )
{
	DWORD Index = Name.GetIndex()<NameIndices.Num() ? NameIndices(Name.GetIndex()) : MaxNameIndex;
	Ar.SerializeInt( Index, MaxNameIndex+1 );
	if( Ar.IsLoading() )
	{
		Name = NAME_None;
		if( Index<MaxNameIndex && !Ar.IsError() )
		{
			for( INT i=0; i<List.Num(); i++ )
			{
				FPackageInfo& Info = List(i);
				if( Index < (DWORD)Info.NameCount )
				{
					Name = Info.Linker->NameMap(Index);
					break;
				}
				Index -= Info.NameCount;
			}
		}
		return 1;
	}
	else return Index!=MaxNameIndex;
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
	if( !Result && ObjectToIndex(Class)!=INDEX_NONE )
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
		{for( INT i=0; i<Class->NetFields.Num(); i++ )
		{
			// Add sandboxed items to net cache.
			UField* Field = Class->NetFields(i);
			if( ObjectToIndex(Field)!=INDEX_NONE )
			{
				INT ConditionIndex = INDEX_NONE;
				INT ThisIndex      = Result->GetMaxIndex();
                UProperty* ItP     = Cast<UProperty>(Field,CLASS_IsAUProperty);
				if( ItP && (ItP->RepOwner==ItP || ObjectToIndex(ItP->RepOwner)==INDEX_NONE) )
					ConditionIndex = Result->RepConditionCount++;
				new(Result->Fields)FFieldNetCache( Field, ThisIndex, ConditionIndex );
			}
		}}
		Result->Fields.Shrink();
		{for( TArray<FFieldNetCache>::TIterator It(Result->Fields); It; ++It )
			Result->FieldMap.Set( It->Field, &*It );}
		{for( TArray<FFieldNetCache>::TIterator It(Result->Fields); It; ++It )
		{
            UProperty* P = Cast<UProperty>(It->Field,CLASS_IsAUProperty);
			if( P )
			{
				if( It->ConditionIndex==INDEX_NONE )
					It->ConditionIndex = Result->GetFromField(P->RepOwner)->ConditionIndex;
				if( !(P->GetOwnerClass()->ClassFlags & CLASS_NativeReplication) )
					Result->RepProperties.AddItem(&*It);
			}
		}}
	}
	return Result;
}

//
// Add a linker to this linker map.
//
INT UPackageMap::AddLinker( ULinkerLoad* Linker )
{
	// Skip if server only.
	if( Linker->Summary.PackageFlags & PKG_ServerSideOnly )
		return INDEX_NONE;

	// Skip if already on list.
	{for( INT i=0; i<List.Num(); i++ )
		if( List(i).Parent == Linker->LinkerRoot )
			return i;}

	// Add to list.
	INT Index = List.Num();
	new(List)FPackageInfo( Linker );

	// Recurse.
	{for( INT i=0; i<Linker->ImportMap.Num(); i++ )
		if( Linker->ImportMap(i).ClassName==NAME_Package && Linker->ImportMap(i).PackageIndex==0 )
			for( INT j=0; j<UObject::GObjLoaders.Num(); j++ )
				if( UObject::GetLoader(j)->LinkerRoot->GetFName()==Linker->ImportMap(i).ObjectName )
					AddLinker( UObject::GetLoader(j) );}

	return Index;
}

//
// Compute mapping info.
//
void UPackageMap::Compute()
{
	{for( INT i=0; i<List.Num(); i++ )
		check(List(i).Linker);}
	NameIndices.Empty( FName::GetMaxNames() );
	NameIndices.Add( FName::GetMaxNames() );
	{for( INT i=0; i<NameIndices.Num(); i++ )
		NameIndices(i) = -1;}
	LinkerMap.Empty();
	MaxObjectIndex = 0;
	MaxNameIndex   = 0;
	{for( INT i=0; i<List.Num(); i++ )
	{
		FPackageInfo& Info    = List(i);
		Info.ObjectBase       = MaxObjectIndex;
		Info.NameBase         = MaxNameIndex;
		Info.ObjectCount      = Info.Linker->ExportMap.Num();
		Info.NameCount        = Info.Linker->NameMap.Num();
		Info.LocalGeneration  = Info.Linker->Summary.Generations.Num();
		if( Info.RemoteGeneration==0 )
		{
			Info.RemoteGeneration = Info.LocalGeneration;
		}
		if( Info.RemoteGeneration<Info.LocalGeneration )
		{
			Info.ObjectCount  = Min( Info.ObjectCount, Info.Linker->Summary.Generations(Info.RemoteGeneration-1).ExportCount );
			Info.NameCount    = Min( Info.NameCount,   Info.Linker->Summary.Generations(Info.RemoteGeneration-1).NameCount   );
		}
		MaxObjectIndex       += Info.ObjectCount;
		MaxNameIndex         += Info.NameCount;
		//debugf( TEXT("** Package %s: %i objs, %i names, gen %i-%i"), *Info.URL. Info.ObjectCount, Info.NameCount, Info.LocalGeneration, Info.RemoveGeneration );
		for( INT j=0; j<Min(Info.Linker->NameMap.Num(),Info.NameCount); j++ )
			if( NameIndices(Info.Linker->NameMap(j).GetIndex()) == -1 )
				NameIndices(Info.Linker->NameMap(j).GetIndex()) = Info.NameBase + j;
		LinkerMap.Set( Info.Linker, i );
	}}
}

//
// Mapping functions.
//
INT UPackageMap::ObjectToIndex( UObject* Object )
{
	if( Object && Object->GetLinker() && Object->_LinkerIndex!=INDEX_NONE )
	{
		INT* Found = LinkerMap.Find( Object->GetLinker() );
		if( Found )
		{
			FPackageInfo& Info = List( *Found );
			if( Object->_LinkerIndex<Info.ObjectCount )
				return Info.ObjectBase + Object->_LinkerIndex;
		}
	}
	return INDEX_NONE;
}
UBOOL UPackageMap::SupportsPackage( UObject* InOuter )
{
	for( INT i=0; i<List.Num(); i++ )
		if( List(i).Parent == InOuter )
			return 1;
	return 0;
}
UObject* UPackageMap::IndexToObject( INT Index, UBOOL Load )
{
	if( Index>=0 )
	{
		for( INT i=0; i<List.Num(); i++ )
		{
			FPackageInfo& Info = List(i);
			if( Index < Info.ObjectCount )
			{
				UObject* Result = Info.Linker->ExportMap(Index)._Object;
				if( !Result && Load )
				{
					UObject::BeginLoad();
					Result = Info.Linker->CreateExport(Index);
					UObject::EndLoad();
				}
				return Result;
			}
			Index -= Info.ObjectCount;
		}
	}
	return NULL;
}
void UPackageMap::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << List << LinkerMap << ClassFieldIndices;
}
void UPackageMap::Destroy()
{
	Super::Destroy();
	for( TMap<UObject*,FClassNetCache*>::TIterator It(ClassFieldIndices); It; ++It )
		delete It.Value();
}
IMPLEMENT_CLASS(UPackageMap);

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

