/*=============================================================================
	UnName.cpp: Unreal global name code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FName helpers.
-----------------------------------------------------------------------------*/

/**
 * Helper function that can be used inside the debuggers watch window. E.g. "DebugFName(Class->Name.Index)". 
 *
 * @param	Index	Name index to look up string for
 * @return			Associated name
 */
const TCHAR* DebugFName(INT Index)
{
	return FName::SafeString((EName)Index);
}

/*-----------------------------------------------------------------------------
	FName statics.
-----------------------------------------------------------------------------*/

// Static variables.
UBOOL				FName::Initialized = 0;
FNameEntry*			FName::NameHash[4096];
TArray<FNameEntry*>	FName::Names;
TArray<INT>         FName::Available;

/*-----------------------------------------------------------------------------
	FName implementation.
-----------------------------------------------------------------------------*/

//
// Hardcode a name.
//
void FName::Hardcode( FNameEntry* AutoName )
{
	// Add name to name hash.
	INT iHash          = appStrihash(AutoName->Name) & (ARRAY_COUNT(NameHash)-1);
	AutoName->HashNext = NameHash[iHash];
	NameHash[iHash]    = AutoName;

	// Expand the table if needed.
	for( INT i=Names.Num(); i<=AutoName->Index; i++ )
		Names.AddItem( NULL );

	// Add name to table.
	if( Names(AutoName->Index) )
		appErrorf( TEXT("Hardcoded name %i was duplicated"), AutoName->Index );
	Names(AutoName->Index) = AutoName;
}

//
// FName constructor.
//
FName::FName( const TCHAR* Name, EFindName FindType )
{
	check(Name);
	if( !Initialized )
		appErrorf( TEXT("FName constructed before FName::StaticInit") );

	// If empty or invalid name was specified, return NAME_None.
	if( !Name[0] )
	{
		Index = NAME_None;
		return;
	}

	// Try to find the name in the hash.
	INT iHash = appStrihash(Name) & (ARRAY_COUNT(NameHash)-1);
	for( FNameEntry* Hash=NameHash[iHash]; Hash; Hash=Hash->HashNext )
	{
		if( appStricmp( Name, Hash->Name )==0 )
		{
			// Found it in the hash.
			Index = Hash->Index;

			// If it already existed in the hash, but we are adding an auto-generated
			// native name entry, set the flag to prevent it being GC'd.
			if( FindType==FNAME_Intrinsic )
				Names(Index)->Flags |= RF_Native;
			return;
		}
	}

	// Didn't find name.
	if( FindType==FNAME_Find )
	{
		// Not found.
		Index = NAME_None;
		return;
	}

	// Find an available entry in the name table.
	if( Available.Num() )
	{
		Index = Available( Available.Num()-1 );
		Available.Remove( Available.Num()-1 );
	}
	else
	{
		Index = Names.Add();
	}

	// Allocate and set the name.
	Names(Index) = NameHash[iHash] = AllocateNameEntry( Name, Index, 0, NameHash[iHash] );
	if( FindType==FNAME_Intrinsic )
		Names(Index)->Flags |= RF_Native;
}

/*-----------------------------------------------------------------------------
	FName subsystem.
-----------------------------------------------------------------------------*/

//
// Initialize the name subsystem.
//
void FName::StaticInit()
{
	check(Initialized==0);
	check((ARRAY_COUNT(NameHash)&(ARRAY_COUNT(NameHash)-1)) == 0);
	Initialized = 1;

	// Init the name hash.
	{for( INT i=0; i<ARRAY_COUNT(FName::NameHash); i++ )
		NameHash[i] = NULL;}

	// Register all hardcoded names.
	#define REGISTER_NAME(num,namestr) \
		Hardcode(AllocateNameEntry(TEXT(#namestr),num,RF_Native,NULL));
	#define REG_NAME_HIGH(num,namestr) \
		Hardcode(AllocateNameEntry(TEXT(#namestr),num,RF_Native|RF_HighlightedName,NULL));
	#include "UnNames.h"

	// Verify no duplicate names.
	{for( INT i=0; i<ARRAY_COUNT(NameHash); i++ )
		for( FNameEntry* Hash=NameHash[i]; Hash; Hash=Hash->HashNext )
			for( FNameEntry* Other=Hash->HashNext; Other; Other=Other->HashNext )
				if( appStricmp(Hash->Name,Other->Name)==0 )
					appErrorf( TEXT("Name '%s' was duplicated"), Hash->Name );}

	debugf( NAME_Init, TEXT("Name subsystem initialized") );
}

//
// Shut down the name subsystem.
//
void FName::StaticExit()
{
	check(Initialized);

	// Kill all names.
	for( INT i=0; i<Names.Num(); i++ )
		if( Names(i) )
			delete Names(i);

	// Empty tables.
	Names.Empty();
	Available.Empty();
	Initialized = 0;

	debugf( NAME_Exit, TEXT("Name subsystem shut down") );
}

//
// Display the contents of the global name hash.
//
void FName::DisplayHash( FOutputDevice& Ar )
{
	INT UsedBins=0, NameCount=0;
	for( INT i=0; i<ARRAY_COUNT(NameHash); i++ )
	{
		if( NameHash[i] != NULL ) UsedBins++;
		for( FNameEntry *Hash = NameHash[i]; Hash; Hash=Hash->HashNext )
			NameCount++;
	}
	Ar.Logf( TEXT("Hash: %i names, %i/%i hash bins"), NameCount, UsedBins, ARRAY_COUNT(NameHash) );
}

//
// Delete an name permanently; called by garbage collector.
//
void FName::DeleteEntry( INT i )
{
	// Unhash it.
	FNameEntry* NameEntry = Names(i);
	check(NameEntry);
	check(!(NameEntry->Flags & RF_Native));
	INT iHash = appStrihash(NameEntry->Name) & (ARRAY_COUNT(NameHash)-1);
	FNameEntry** HashLink;
	for( HashLink=&NameHash[iHash]; *HashLink && *HashLink!=NameEntry; HashLink=&(*HashLink)->HashNext );
	if( !*HashLink )
		appErrorf( TEXT("Unhashed name '%s'"), NameEntry->Name );
	*HashLink = (*HashLink)->HashNext;

	// Delete it.
	delete NameEntry;
	Names(i) = NULL;
	Available.AddItem( i );
}

/*-----------------------------------------------------------------------------
	FNameEntry implementation.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FNameEntry& E )
{
	FString Str( E.Name );
	Ar << Str;
	appStrcpy( E.Name, *Str.Left(NAME_SIZE-1) );
	return Ar << E.Flags;
}

FNameEntry* AllocateNameEntry( const TCHAR* Name, DWORD Index, DWORD Flags, FNameEntry* HashNext )
{
	FNameEntry* NameEntry = (FNameEntry*)appMalloc( sizeof(FNameEntry) - (NAME_SIZE - appStrlen(Name) - 1)*sizeof(TCHAR) );
	NameEntry->Index      = Index;
	NameEntry->Flags      = Flags;
	NameEntry->HashNext   = HashNext;
	appStrcpy( NameEntry->Name, Name );
	return NameEntry;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

