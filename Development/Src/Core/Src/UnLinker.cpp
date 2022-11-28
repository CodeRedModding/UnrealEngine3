/*=============================================================================
	UnLinker.cpp: Unreal object linker.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Hash function.
-----------------------------------------------------------------------------*/

static inline INT HashNames( FName A, FName B, FName C )
{
	return A.GetIndex() + 7 * B.GetIndex() + 31*C.GetIndex();
}

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

FObjectExport::FObjectExport()
:	_Object			( NULL														)
,	_iHashNext		( INDEX_NONE												)
{}

FObjectExport::FObjectExport( UObject* InObject )
:	ClassIndex		( 0															)
,	SuperIndex		( 0															)
,	PackageIndex	( 0															)
,	ObjectName		( InObject ? (InObject->GetFName()			) : FName(NAME_None)	)
,	ObjectFlags		( InObject ? (InObject->GetFlags() & RF_Load) : 0			)
,	SerialSize		( 0															)
,	SerialOffset	( 0															)
,	_Object			( InObject													)
{}

FArchive& operator<<( FArchive& Ar, FObjectExport& E )
{
	Ar << E.ClassIndex;
	Ar << E.SuperIndex;
	Ar << E.PackageIndex;
	Ar << E.ObjectName;
	Ar << E.ObjectFlags;
	Ar << E.SerialSize;
	if( E.SerialSize )
	{
		Ar << E.SerialOffset;
	}
	Ar << E.ComponentMap;

	return Ar;
}

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

FObjectImport::FObjectImport()
{}

FObjectImport::FObjectImport( UObject* InObject )
:	ClassPackage	( InObject->GetClass()->GetOuter()->GetFName())
,	ClassName		( InObject->GetClass()->GetFName()		 )
,	PackageIndex	( 0                                      )
,	ObjectName		( InObject->GetFName()					 )
,	XObject			( InObject								 )
,	SourceLinker	( NULL									 )
,	SourceIndex		( INDEX_NONE							 )
{
	if( XObject )
		UObject::GImportCount++;
}

FArchive& operator<<( FArchive& Ar, FObjectImport& I )
{
	Ar << I.ClassPackage << I.ClassName;
	Ar << I.PackageIndex;
	Ar << I.ObjectName;
	if( Ar.IsLoading() )
	{
		I.SourceIndex = INDEX_NONE;
		I.XObject     = NULL;
	}
	return Ar;
}

/*----------------------------------------------------------------------------
	Items stored in Unrealfiles.
----------------------------------------------------------------------------*/

FGenerationInfo::FGenerationInfo( INT InExportCount, INT InNameCount )
: ExportCount(InExportCount), NameCount(InNameCount)
{}

FPackageFileSummary::FPackageFileSummary()
{
	appMemzero( this, sizeof(*this) );
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	Ar << Sum.Tag;
	Ar << Sum.FileVersion;
	Ar << Sum.PackageFlags;
	Ar << Sum.NameCount     << Sum.NameOffset;
	Ar << Sum.ExportCount   << Sum.ExportOffset;
	Ar << Sum.ImportCount   << Sum.ImportOffset;
	if( Sum.GetFileVersion()>=68 )
	{
		INT GenerationCount = Sum.Generations.Num();
		Ar << Sum.Guid << GenerationCount;
		//!!67 had: return
		if( Ar.IsLoading() )
			Sum.Generations = TArray<FGenerationInfo>( GenerationCount );
		for( INT i=0; i<GenerationCount; i++ )
			Ar << Sum.Generations(i);
	}
	else
		appErrorf(TEXT("!!oldver << FPackageFileSummary"));

	return Ar;
}

/*----------------------------------------------------------------------------
	ULinker.
----------------------------------------------------------------------------*/

ULinker::ULinker( UObject* InRoot, const TCHAR* InFilename )
:	LinkerRoot( InRoot )
,	Summary()
,	Success( 123456 )
,	Filename( InFilename )
,	_ContextFlags( 0 )
{
	check(LinkerRoot);
	check(InFilename);

	// Set context flags.
	if( GIsEditor ) _ContextFlags |= RF_LoadForEdit;
	if( GIsClient ) _ContextFlags |= RF_LoadForClient;
	if( GIsServer ) _ContextFlags |= RF_LoadForServer;
}

void ULinker::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Sizes.
	ImportMap	.CountBytes( Ar );
	ExportMap	.CountBytes( Ar );

	// Prevent garbage collecting of linker's names and package.
	Ar << NameMap << LinkerRoot;
	{for( INT i=0; i<ExportMap.Num(); i++ )
	{
		FObjectExport& E = ExportMap(i);
		Ar << E.ObjectName;
	}}
	{for( INT i=0; i<ImportMap.Num(); i++ )
	{
		FObjectImport& I = ImportMap(i);
		Ar << *(UObject**)&I.SourceLinker;
		Ar << I.ClassPackage << I.ClassName;
	}}
}

// ULinker interface.
FString ULinker::GetImportFullName( INT i )
{
	FString S;
	for( INT j=-i-1; j!=0; j=ImportMap(-j-1).PackageIndex )
	{
		if( j != -i-1 )
			S = US + TEXT(".") + S;
		S = FString(*ImportMap(-j-1).ObjectName) + S;
	}
	return FString(*ImportMap(i).ClassName) + TEXT(" ") + S ;
}

FString ULinker::GetExportFullName( INT i, const TCHAR* FakeRoot )
{
	FString S;
	for( INT j=i+1; j!=0; j=ExportMap(j-1).PackageIndex )
	{
		if( j != i+1 )
			S = US + TEXT(".") + S;
		S = FString(*ExportMap(j-1).ObjectName) + S;
	}
	INT ClassIndex = ExportMap(i).ClassIndex;
	FName ClassName = ClassIndex>0 ? ExportMap(ClassIndex-1).ObjectName : ClassIndex<0 ? ImportMap(-ClassIndex-1).ObjectName : FName(NAME_Class);
	return FString(*ClassName) + TEXT(" ") + (FakeRoot ? FakeRoot : LinkerRoot->GetPathName()) + TEXT(".") + S;
}

/*----------------------------------------------------------------------------
	ULinkerLoad.
----------------------------------------------------------------------------*/

ULinkerLoad::ULinkerLoad( UObject* InParent, const TCHAR* InFilename, DWORD InLoadFlags )
:	ULinker( InParent, InFilename )
,	LoadFlags( InLoadFlags )
{
	Loader = GFileManager->CreateFileReader( InFilename, 0, GError );
	if( !Loader )
		appThrowf( *LocalizeError(TEXT("OpenFailed"),TEXT("Core")) );

	// Error if linker already loaded.
	{for( INT i=0; i<GObjLoaders.Num(); i++ )
		if( GetLoader(i)->LinkerRoot == LinkerRoot )
			appThrowf( *LocalizeError(TEXT("LinkerExists"),TEXT("Core")), LinkerRoot->GetName() );}

	// Begin.
	GWarn->StatusUpdatef( 0, 0, *LocalizeProgress(TEXT("Loading"),TEXT("Core")), *Filename );

	// Set status info.
	ArVer			= GPackageFileVersion;
	ArLicenseeVer	= GPackageFileLicenseeVersion;
	ArIsLoading		= 1;
	ArIsPersistent	= 1;
	ArForEdit		= GIsEditor;
	ArForClient		= 1;
	ArForServer		= 1;

	// Read summary from file.
	*this << Summary;
	// Loader needs to be the same version.
	Loader->SetVer(Summary.GetFileVersion());
	Loader->SetLicenseeVer(Summary.GetFileVersionLicensee());
	ArVer = Summary.GetFileVersion();
	ArLicenseeVer = Summary.GetFileVersionLicensee();
	if( Cast<UPackage>(LinkerRoot) )
		Cast<UPackage>(LinkerRoot)->PackageFlags = Summary.PackageFlags;
	
	// Check tag.
	if( Summary.Tag != PACKAGE_FILE_TAG )
	{
		warnf( *LocalizeError(TEXT("BinaryFormat"),TEXT("Core")), *Filename );
		throw( *LocalizeError(TEXT("Aborted"),TEXT("Core")) );
	}

	// Validate the summary.
	if( Summary.GetFileVersion() < GPackageFileMinVersion )
		if( !GWarn->YesNof( *LocalizeQuery(TEXT("OldVersion"),TEXT("Core")), *Filename ) )
			throw( *LocalizeError(TEXT("Aborted"),TEXT("Core")) );
	
	// Slack everything according to summary.
	ImportMap   .Empty( Summary.ImportCount   );
	ExportMap   .Empty( Summary.ExportCount   );
	NameMap		.Empty( Summary.NameCount     );

	// Load and map names.
	if( Summary.NameCount > 0 )
	{
		Seek( Summary.NameOffset );
		for( INT i=0; i<Summary.NameCount; i++ )
		{
			// Read the name entry from the file.
			FNameEntry NameEntry;
			*this << NameEntry;

			// Add it to the name table if it's needed in this context.				
			NameMap.AddItem( (NameEntry.Flags & _ContextFlags) ? FName( NameEntry.Name, FNAME_Add ) : FName(NAME_None) );
		}
	}

	// Load import map.
	if( Summary.ImportCount > 0 )
	{
		Seek( Summary.ImportOffset );
		for( INT i=0; i<Summary.ImportCount; i++ )
		{
			*this << *new(ImportMap)FObjectImport;
		}
	}

	//@hack: The below fixes up the import table so maps refering to the old SequenceObjects package look for those 
	// classes & objects in the Engine package where they were moved to.
	for( INT i=0; i<ImportMap.Num(); i++ )
	{
		if( ImportMap(i).ObjectName == NAME_SequenceObjects && ImportMap(i).ClassName == NAME_Package )
			ImportMap(i).ObjectName = NAME_Engine;
		if( ImportMap(i).ClassPackage == NAME_SequenceObjects )
			ImportMap(i).ClassPackage = NAME_Engine;
	}

	// Load export map.
	if( Summary.ExportCount > 0 )
	{
		Seek( Summary.ExportOffset );
		for( INT i=0; i<Summary.ExportCount; i++ )
		{
			FObjectExport*	Export = new(ExportMap)FObjectExport;

			*this << *Export;
		}
	}
	
	// Create export hash.
	//warning: Relies on import & export tables, so must be done here.
	{for( INT i=0; i<ARRAY_COUNT(ExportHash); i++ )
	{
		ExportHash[i] = INDEX_NONE;
	}}
	{for( INT i=0; i<ExportMap.Num(); i++ )
	{
		INT iHash = HashNames( ExportMap(i).ObjectName, GetExportClassName(i), GetExportClassPackage(i) ) & (ARRAY_COUNT(ExportHash)-1);
		ExportMap(i)._iHashNext = ExportHash[iHash];
		ExportHash[iHash] = i;
	}}

	// Add this linker to the object manager's linker array.
	GObjLoaders.AddItem( this );
	if( !(LoadFlags & LOAD_NoVerify) )
		Verify();

	// Success.
	Success = 1;
}

void ULinkerLoad::Verify()
{
	if( !Verified )
	{
		try
		{
			// Validate all imports and map them to their remote linkers.
			for( INT i=0; i<Summary.ImportCount; i++ )
				VerifyImport( i );
		}
		// !! PSX2 gcc doesn't like catch( TCHAR* Error )
		#if UNICODE
		catch( TCHAR* Error )
		#else
		catch( char* Error )
		#endif
		{
			GObjLoaders.RemoveItem( this );
			throw( Error );
		}
	}
	Verified=1;
}

FName ULinkerLoad::GetExportClassPackage( INT i )
{
	FObjectExport& Export = ExportMap( i );
	if( Export.ClassIndex < 0 )
	{
		FObjectImport& Import = ImportMap( -Export.ClassIndex-1 );
		checkSlow(Import.PackageIndex<0);
		return ImportMap( -Import.PackageIndex-1 ).ObjectName;
	}
	else if( Export.ClassIndex > 0 )
	{
		return LinkerRoot->GetFName();
	}
	else
	{
		return NAME_Core;
	}
}

FName ULinkerLoad::GetExportClassName( INT i )
{
	FObjectExport& Export = ExportMap(i);
	if( Export.ClassIndex < 0 )
	{
		return ImportMap( -Export.ClassIndex-1 ).ObjectName;
	}
	else if( Export.ClassIndex > 0 )
	{
		return ExportMap( Export.ClassIndex-1 ).ObjectName;
	}
	else
	{
		return NAME_Class;
	}
}

// Safely verify an import.
void ULinkerLoad::VerifyImport( INT i )
{
	FObjectImport& Import = ImportMap(i);
	if
	(	(Import.SourceLinker && Import.SourceIndex != INDEX_NONE)
	||	Import.ClassPackage	== NAME_None
	||	Import.ClassName	== NAME_None
	||	Import.ObjectName	== NAME_None )
	{
		// Already verified, or not relevent in this context.
		return;
	}

	// Find or load this import's linker.
	INT Depth=0;
	UObject* Pkg=NULL;
	if( Import.PackageIndex == 0 )
	{
		check(Import.ClassName==NAME_Package);
		check(Import.ClassPackage==NAME_Core);
		UPackage* TmpPkg = CreatePackage( NULL, *Import.ObjectName );
		try
		{
			Import.SourceLinker = GetPackageLinker( TmpPkg, NULL, LOAD_Throw | (LoadFlags & LOAD_Propagate), NULL, NULL );
		}
		catch( const TCHAR* Error )//oldver
		{
			if( LoadFlags & LOAD_FindIfFail )
			{
				Import.SourceLinker = NULL;
			}
			else
			{
				appThrowf( Error );
			}
		}
	}
	else
	{
		check(Import.PackageIndex<0);
		VerifyImport( -Import.PackageIndex-1 );
		Import.SourceLinker = ImportMap(-Import.PackageIndex-1).SourceLinker;
		//check(Import.SourceLinker);
		if( Import.SourceLinker )
		{
			FObjectImport* Top;
			for
			(	Top = &Import
			;	Top->PackageIndex<0
			;	Top = &ImportMap(-Top->PackageIndex-1),Depth++ );
			Pkg = CreatePackage( NULL, *Top->ObjectName );
		}
	}

	// Find this import within its existing linker.
	UBOOL SafeReplace = 0;
	INT iHash = HashNames( Import.ObjectName, Import.ClassName, Import.ClassPackage) & (ARRAY_COUNT(ExportHash)-1);
	if( Import.SourceLinker )
	{
		for( INT j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap(j)._iHashNext )
		{
			FObjectExport& Source = Import.SourceLinker->ExportMap( j );
			if
			(	(Source.ObjectName	                          ==Import.ObjectName               )
			&&	(Import.SourceLinker->GetExportClassName   (j)==Import.ClassName                )
			&&  (Import.SourceLinker->GetExportClassPackage(j)==Import.ClassPackage) )
			{
				if( Import.PackageIndex<0 )
				{
					FObjectImport& ParentImport = ImportMap(-Import.PackageIndex-1);
					if( ParentImport.SourceLinker )
					{
						if( ParentImport.SourceIndex==INDEX_NONE )
						{
							if( Source.PackageIndex!=0 )
							{
								continue;
							}
						}
						else if( ParentImport.SourceIndex+1 != Source.PackageIndex )
						{
							if( Source.PackageIndex!=0 )
							{
								continue;
							}
						}
					}
				}
				if( !(Source.ObjectFlags & RF_Public) )
				{
					appThrowf( *LocalizeError(TEXT("FailedImportPrivate"),TEXT("Core")), *Import.ClassName, *GetImportFullName(i) );
				}
				Import.SourceIndex = j;
				break;
			}
		}
	}

	if( (Pkg == NULL) && (LoadFlags & LOAD_FindIfFail) )
	{
		Pkg = ANY_PACKAGE;
	}

	// If not found in file, see if it's a public native transient class.
	if( Import.SourceIndex==INDEX_NONE && Pkg!=NULL )
	{
		UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage );
		if( ClassPackage )
		{
			UClass* FindClass = FindObject<UClass>( ClassPackage, *Import.ClassName );
			if( FindClass )
			{
				UObject* FindObject			= StaticFindObject( FindClass, Pkg, *Import.ObjectName );
				UBOOL	 IsNativeTransient	= FindObject && (FindObject->GetFlags() & RF_Public) && (FindObject->GetFlags() & RF_Native) && (FindObject->GetFlags() & RF_Transient);
		
				if(	FindObject && ( (LoadFlags & LOAD_FindIfFail) || IsNativeTransient ) )
				{
					Import.XObject = FindObject;
					GImportCount++;
				}
				else
				{
					if( GIsEditor && !GIsUCC )
						EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *GetImportFullName(i) );
					debugf( NAME_Warning, TEXT("Missing %s %s"), FindClass->GetName(), *GetImportFullName(i) );
					SafeReplace = 1;
				}
			}
			else
			{
				if( GIsEditor && !GIsUCC )
					EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *GetImportFullName(i) );
				debugf( NAME_Warning, TEXT("Missing %s"), *GetImportFullName(i) );
				SafeReplace = 1;
			}
		}
		if( !Import.XObject && !SafeReplace )
		{
			debugf( TEXT("Failed import: %s %s (file %s)"), *Import.ClassName, *GetImportFullName(i), *Import.SourceLinker->Filename );
			appThrowf( *LocalizeError(TEXT("FailedImport"),TEXT("Core")), *Import.ClassName, *GetImportFullName(i) );
		}
	}
}

// Load all objects; all errors here are fatal.
void ULinkerLoad::LoadAllObjects()
{
	for( INT i=0; i<Summary.ExportCount; i++ )
		CreateExport( i );
}

// Find the index of a specified object without regard to specific package.
INT ULinkerLoad::FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, INT PackageIndex )
{
	INT iHash = HashNames( ObjectName, ClassName, ClassPackage ) & (ARRAY_COUNT(ExportHash)-1);
	for( INT i=ExportHash[iHash]; i!=INDEX_NONE; i=ExportMap(i)._iHashNext )
	{
		if
		(  (ExportMap(i).ObjectName  ==ObjectName                              )
		&& (ExportMap(i).PackageIndex==PackageIndex || PackageIndex==INDEX_NONE)
		&& (GetExportClassPackage(i) ==ClassPackage                            )
		&& (GetExportClassName   (i) ==ClassName                               ) )
		{
			return i;
		}
	}

	// If an object with the exact class wasn't found, look for objects with a subclass of the requested class.

	for(INT ExportIndex = 0;ExportIndex < ExportMap.Num();ExportIndex++)
	{
		FObjectExport&	Export = ExportMap(ExportIndex);

		if(Export.ObjectName == ObjectName && (PackageIndex == INDEX_NONE || Export.PackageIndex == PackageIndex))
		{
			UClass*	ExportClass = Cast<UClass>(IndexToObject(Export.ClassIndex));

			// See if this export's class inherits from the requested class.

			for(UClass* ParentClass = ExportClass;ParentClass;ParentClass = ParentClass->GetSuperClass())
			{
				if(ParentClass->GetFName() == ClassName)
					return ExportIndex;
			}
		}
	}

	return INDEX_NONE;
}

// Create a single object.
UObject* ULinkerLoad::Create( UClass* ObjectClass, FName ObjectName, DWORD LoadFlags, UBOOL Checked )
{
	INT Index = FindExportIndex( ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, INDEX_NONE );
	if( Index!=INDEX_NONE )
		return (LoadFlags & LOAD_Verify) ? (UObject*)-1 : CreateExport(Index);
	if( Checked )
		appThrowf( *LocalizeError(TEXT("FailedCreate"),TEXT("Core")), ObjectClass->GetName(), *ObjectName );
	return NULL;
}

void ULinkerLoad::Preload( UObject* Object )
{
	check(IsValid());
	check(Object);
	if( Object->GetFlags() & RF_Preloading )
	{
		// Warning for internal development.
		//debugf( "Object preload reentrancy: %s", Object->GetFullName() );
	}
	if( Object->GetLinker()==this )
	{
		// Preload the object if necessary.
		if( Object->GetFlags() & RF_NeedLoad )
		{
			// If this is a struct, preload its super.
			if(	Object->IsA(UStruct::StaticClass()) )
				if( ((UStruct*)Object)->SuperField )
					Preload( ((UStruct*)Object)->SuperField );

			// Load the local object now.
			FObjectExport& Export = ExportMap( Object->_LinkerIndex );
			check(Export._Object==Object);
			INT SavedPos = Loader->Tell();
			Loader->Seek( Export.SerialOffset );
			Loader->Precache( Export.SerialSize );

			// Load the object.
			Object->ClearFlags ( RF_NeedLoad );
			Object->SetFlags   ( RF_Preloading );
			Object->Serialize  ( *this );
			Object->ClearFlags ( RF_Preloading );
			//debugf(NAME_Log,"    %s: %i", Object->GetFullName(), Export.SerialSize );

			// Make sure we serialized the right amount of stuff.
			if( Tell()-Export.SerialOffset != Export.SerialSize )
				appErrorf( *LocalizeError(TEXT("SerialSize"),TEXT("Core")), *Object->GetFullName(), Tell()-Export.SerialOffset, Export.SerialSize );
			Loader->Seek( SavedPos );
		}
	}
	else if( Object->GetLinker() )
	{
		// Send to the object's linker.
		Object->GetLinker()->Preload( Object );
	}
}

UObject* ULinkerLoad::CreateExport( INT Index )
{
	// Map the object into our table.
	FObjectExport& Export = ExportMap( Index );
	if( !Export._Object && (Export.ObjectFlags & _ContextFlags) )
	{
		check(Export.ObjectName!=NAME_None || !(Export.ObjectFlags&RF_Public));

		// Get the object's class.
		UClass* LoadClass = (UClass*)IndexToObject( Export.ClassIndex );
		if( !LoadClass && Export.ClassIndex!=0 ) // Hack to load packages with classes which do not exist.
			return NULL;
		if( !LoadClass )
			LoadClass = UClass::StaticClass();
		check(LoadClass);
		check(LoadClass->GetClass()==UClass::StaticClass());
		if( (LoadClass->GetFName()==NAME_Camera) || (LoadClass->GetFName()==NAME_PlayerInput) )
			appErrorf(TEXT("!!oldver ULinkerLoad::CreateExport"));
		Preload( LoadClass );

		// Get the outer object. If that caused the object to load, return it.
		UObject* ThisParent = Export.PackageIndex ? IndexToObject(Export.PackageIndex) : LinkerRoot;
		if( Export._Object )
			return Export._Object;

		// If the parent has been removed from a package, repackage it to transient and hope it goes away.
		if( GIsEditor && !ThisParent )
		{
			EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *FString::Printf(TEXT("Outer object for %s"), *Export.ObjectName) );
			ThisParent = GetTransientPackage();
		}

		// Create the export object.
		Export._Object = StaticConstructObject
		(
			LoadClass,
			ThisParent,
			Export.ObjectName,
			(Export.ObjectFlags & RF_Load) | RF_NeedLoad | RF_NeedPostLoad
		);
		Export._Object->SetLinker( this, Index );
		GObjLoaded.AddItem( Export._Object );
		debugfSlow( NAME_DevLoad, TEXT("Created %s"), *Export._Object->GetFullName() );

		// If it's a struct or class, set its parent.
		if( Export._Object->IsA(UStruct::StaticClass()) && Export.SuperIndex!=0 )
			((UStruct*)Export._Object)->SuperField = (UStruct*)IndexToObject( Export.SuperIndex );

		// If it's a class, bind it to C++.
		if( Export._Object->IsA( UClass::StaticClass() ) )
			((UClass*)Export._Object)->Bind();
	}
	return Export._Object;
}

// Return the loaded object corresponding to an import index; any errors are fatal.
UObject* ULinkerLoad::CreateImport( INT Index )
{
	FObjectImport& Import = ImportMap( Index );
	if( !Import.XObject )
	{
		if(!Import.SourceLinker)
		{
			BeginLoad();
			VerifyImport(Index);
			EndLoad();
		}

		if(Import.SourceIndex != INDEX_NONE)
		{
			Import.XObject = Import.SourceLinker->CreateExport( Import.SourceIndex );
			GImportCount++;
		}
	}
	return Import.XObject;
}

// Map an import/export index to an object; all errors here are fatal.
UObject* ULinkerLoad::IndexToObject( INT Index )
{
	if( Index > 0 )
	{
		if( !ExportMap.IsValidIndex( Index-1 ) )
			appErrorf( *LocalizeError(TEXT("ExportIndex"),TEXT("Core")), Index-1, ExportMap.Num() );			
		return CreateExport( Index-1 );
	}
	else if( Index < 0 )
	{
		if( !ImportMap.IsValidIndex( -Index-1 ) )
			appErrorf( *LocalizeError(TEXT("ImportIndex"),TEXT("Core")), -Index-1, ImportMap.Num() );
		return CreateImport( -Index-1 );
	}
	else return NULL;
}

// Detach an export from this linker.
void ULinkerLoad::DetachExport( INT i )
{
	FObjectExport& E = ExportMap( i );
	check(E._Object);
	if( !E._Object->IsValid() )
		appErrorf( TEXT("Linker object %s %s.%s is invalid"), *GetExportClassName(i), LinkerRoot->GetName(), *E.ObjectName );
	if( E._Object->GetLinker()!=this )
		appErrorf( TEXT("Linker object %s %s.%s mislinked"), *GetExportClassName(i), LinkerRoot->GetName(), *E.ObjectName );
	if( E._Object->_LinkerIndex!=i )
		appErrorf( TEXT("Linker object %s %s.%s misindexed"), *GetExportClassName(i), LinkerRoot->GetName(), *E.ObjectName );
	ExportMap(i)._Object->SetLinker( NULL, INDEX_NONE );
}

// UObject interface.
void ULinkerLoad::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	LazyLoaders.CountBytes( Ar );
}

void ULinkerLoad::Destroy()
{
	debugf( TEXT("%1.1fms Unloading: %s"), appSeconds() * 1000.0, *LinkerRoot->GetFullName() );

	// Detach all lazy loaders.
	DetachAllLazyLoaders( 0 );

	// Detach all objects linked with this linker.
	for( INT i=0; i<ExportMap.Num(); i++ )
		if( ExportMap(i)._Object )
			DetachExport( i );

	// Remove from object manager, if it has been added.
	GObjLoaders.RemoveItem( this );
	if( Loader )
		delete Loader;
	Loader = NULL;

	Super::Destroy();
}

// FArchive interface.
void ULinkerLoad::AttachLazyLoader( FLazyLoader* LazyLoader )
{
	checkSlow(LazyLoader->SavedAr==NULL);
	checkSlow(LazyLoaders.FindItemIndex(LazyLoader)==INDEX_NONE);

	LazyLoaders.AddItem( LazyLoader );
	LazyLoader->SavedAr  = this;
	LazyLoader->SavedPos = Tell();
}

void ULinkerLoad::DetachLazyLoader( FLazyLoader* LazyLoader )
{
	checkSlow(LazyLoader->SavedAr==this);

	INT RemovedCount = LazyLoaders.RemoveItem(LazyLoader);
	if( RemovedCount!=1 )
		appErrorf( TEXT("Detachment inconsistency: %i (%s)"), RemovedCount, *Filename );
	LazyLoader->SavedAr = NULL;
	LazyLoader->SavedPos = 0;
}

void ULinkerLoad::DetachAllLazyLoaders( UBOOL Load )
{
	for( INT i=0; i<LazyLoaders.Num(); i++ )
	{
		FLazyLoader* LazyLoader = LazyLoaders( i );
		if( Load )
			LazyLoader->Load();
		LazyLoader->SavedAr  = NULL;
		LazyLoader->SavedPos = 0;
	}
	LazyLoaders.Empty();
}

// FArchive interface.
void ULinkerLoad::Seek( INT InPos )
{
	Loader->Seek( InPos );
}

INT ULinkerLoad::Tell()
{
	return Loader->Tell();
}

INT ULinkerLoad::TotalSize()
{
	return Loader->TotalSize();
}

void ULinkerLoad::Serialize( void* V, INT Length )
{
	Loader->Serialize( V, Length );
}

/*----------------------------------------------------------------------------
	ULinkerSave.
----------------------------------------------------------------------------*/

ULinkerSave::ULinkerSave( UObject* InParent, const TCHAR* InFilename )
:	ULinker( InParent, InFilename )
,	Saver( NULL )
{
	// Create file saver.
	Saver = GFileManager->CreateFileWriter( InFilename, 0, GThrow );
	if( !Saver )
		appThrowf( *LocalizeError(TEXT("OpenFailed"),TEXT("Core")) );

	// Set main summary info.
	Summary.Tag           = PACKAGE_FILE_TAG;
	Summary.SetFileVersions( GPackageFileVersion, GPackageFileLicenseeVersion );
	Summary.PackageFlags  = Cast<UPackage>(LinkerRoot) ? Cast<UPackage>(LinkerRoot)->PackageFlags : 0;

	// Set status info.
	ArIsSaving     = 1;
	ArIsPersistent = 1;
	ArForEdit      = GIsEditor;
	ArForClient    = 1;
	ArForServer    = 1;

	// Allocate indices.
	ObjectIndices.AddZeroed( UObject::GObjObjects.Num() );
	NameIndices  .AddZeroed( FName::GetMaxNames() );

	// Success.
	Success=1;
}

void ULinkerSave::Destroy()
{
	if( Saver )
		delete Saver;
	Saver = NULL;
	Super::Destroy();
}

// FArchive interface.
INT ULinkerSave::MapName( FName* Name )
{
	return NameIndices(Name->GetIndex());
}

INT ULinkerSave::MapObject( UObject* Object )
{
	return Object ? ObjectIndices(Object->GetIndex()) : 0;
}

void ULinkerSave::Seek( INT InPos )
{
	Saver->Seek( InPos );
}

INT ULinkerSave::Tell()
{
	return Saver->Tell();
}

void ULinkerSave::Serialize( void* V, INT Length )
{
	Saver->Serialize( V, Length );
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

