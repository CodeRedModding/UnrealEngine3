/*=============================================================================
	UnLinker.cpp: Unreal object linker.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "DebuggingDefines.h"
#include "CrossLevelReferences.h"

//@script patcher
#include "UnScriptPatcher.h"

/** Map that keeps track of any precached full package reads															*/
TMap<FString, ULinkerLoad::FPackagePrecacheInfo> ULinkerLoad::PackagePrecacheMap;

/** A mapping of package name to generated script SHA keys */
TMap<FString, TArray<BYTE> > ULinkerSave::PackagesToScriptSHAMap;



#if !FINAL_RELEASE

/**
 * Here is the format for the ClassRedirection:
 * 
 *  ; Basic redirects
 *  ;ActiveClassRedirects=(OldClassName="MyClass",NewClassName="NewNativePackage.MyClass")
 *	ActiveClassRedirects=(OldClassName="CylinderComponent",NewClassName="CapsuleComponent")
 *
 *	; Keep both classes around, but convert any existing instances of that object to a particular class (insert into the inheritance hierarchy
 *	;ActiveClassRedirects=(OldClassName="MyClass",NewClassName="MyClassParent",InstanceOnly="true")
 *
 */


TMap<FName, FName> ULinkerLoad::ObjectNameRedirects;			    // OldClassName to NewClassName for ImportMap
TMap<FName, FName> ULinkerLoad::ObjectNameRedirectsInstanceOnly;	// OldClassName to NewClassName for ExportMap
TMap<FName, FName> ULinkerLoad::ObjectNameRedirectsObjectOnly;		// Object name to NewClassName for export map

/**
 * Add redirects to ULinkerLoad static map
 */
void ULinkerLoad::CreateActiveRedirectsMap(const TCHAR* GEngineIniName)
{		
	if( GConfig )
	{
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("Engine.Engine"), FALSE, TRUE, GEngineIniName );
		for( FConfigSection::TIterator It(*PackageRedirects); It; ++It )
		{
			if( It.Key() == TEXT("ActiveClassRedirects") )
			{
				FName OldClassName = NAME_None;
				FName NewClassName = NAME_None;
				FName ObjectName = NAME_None;

				UBOOL bInstanceOnly = FALSE;

				ParseUBOOL( *It.Value(), TEXT("InstanceOnly="), bInstanceOnly );
				Parse( *It.Value(), TEXT("ObjectName="), ObjectName );

				Parse( *It.Value(), TEXT("OldClassName="), OldClassName );
				Parse( *It.Value(), TEXT("NewClassName="), NewClassName );

				//instances only
				if( bInstanceOnly )
				{
					ObjectNameRedirectsInstanceOnly.Set(OldClassName,NewClassName);
				}
				//objects only on a per-object basis
				else if( ObjectName != NAME_None )
				{
					ObjectNameRedirectsObjectOnly.Set(ObjectName, NewClassName);
				}
				//full redirect
				else
				{
					ObjectNameRedirects.Set(OldClassName,NewClassName);
				}
			}			
		}
	}
	else
	{
		warnf(TEXT(" **** ACTIVE CLASS REDIRECTS UNABLE TO INITIALIZE! (mActiveClassRedirects) **** "));
	}
}

#endif//!FINAL_RELEASE


/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/

/**
 * Fills in the passed in TArray with the packages that are in its PrecacheMap
 *
 * @param TArray<FString> to be populated
 */
void ULinkerLoad::GetListOfPackagesInPackagePrecacheMap( TArray<FString>& ListOfPackages )
{
	for ( TMap<FString, ULinkerLoad::FPackagePrecacheInfo>::TIterator It(PackagePrecacheMap); It; ++It )
	{
		ListOfPackages.AddItem( It.Key() );
	}
}

/**
 * Type hash implementation. Export indices are usually less than 100k, so are linker indices.
 *
 * @param	Ref		Reference to hash
 * @return	hash value
 */
DWORD GetTypeHash( const FDependencyRef& Ref  )
{
	return Ref.Linker->GetIndex() ^ (Ref.ExportIndex << 12);
}

/** 
 * Returns whether we should ignore the fact that this class has been removed instead of deprecated. 
 * Normally the script compiler would spit out an error but it makes sense to silently ingore it in 
 * certain cases in which case the below code should be extended to include the class' name.
 *
 * @param	ClassName	Name of class to find out whether we should ignore complaining about it not being present
 * @return	TRUE if we should ignore the fact that it doesn't exist, FALSE otherwise
 */
static UBOOL IgnoreMissingReferencedClass( FName ClassName )
{
	static TArray<FName>	MissingClassesToIgnore;
	static UBOOL			bAlreadyInitialized = FALSE;
	if( !bAlreadyInitialized )
	{
		//@deprecated with VER_RENDERING_REFACTOR
		MissingClassesToIgnore.AddItem( FName(TEXT("SphericalHarmonicMap")) );
		MissingClassesToIgnore.AddItem( FName(TEXT("LightMap1D")) );
		MissingClassesToIgnore.AddItem( FName(TEXT("LightMap2D")) );
		bAlreadyInitialized = TRUE;
	}
	return MissingClassesToIgnore.FindItemIndex( ClassName ) != INDEX_NONE;
}

static inline INT HashNames( FName A, FName B, FName C )
{
	return A.GetIndex() + 7 * B.GetIndex() + 31*C.GetIndex();
}

/** Helper struct to keep track of the first time CreateImport() is called in the current callstack. */
struct FScopedCreateImportCounter
{
	/**
	 *	Constructor. Called upon CreateImport() entry.
	 *	@param Linker	- Current Linker
	 *	@param Index	- Index of the current Import
	 */
	FScopedCreateImportCounter( ULinkerLoad* Linker, INT Index )
	{
		// First time CreateImport() is called for this callstack?
		if ( Counter++ == 0 )
		{
			// Remember the current linker and index.
			GSerializedImportLinker = Linker;
			GSerializedImportIndex = Index;
		}
	}

	/** Destructor. Called upon CreateImport() exit. */
	~FScopedCreateImportCounter()
	{
		// Last time CreateImport() exits for this callstack?
		if ( --Counter == 0 )
		{
			GSerializedImportLinker = NULL;
			GSerializedImportIndex = INDEX_NONE;
		}
	}

	/** Number of times CreateImport() has been called in the current callstack. */
	static INT	Counter;
};

/** Number of times CreateImport() has been called in the current callstack. */
INT FScopedCreateImportCounter::Counter = 0;

/** Helper struct to keep track of the CreateExport() entry/exit. */
struct FScopedCreateExportCounter
{
	/**
	 *	Constructor. Called upon CreateImport() entry.
	 *	@param Linker	- Current Linker
	 *	@param Index	- Index of the current Import
	 */
	FScopedCreateExportCounter( ULinkerLoad* Linker, INT Index )
	{
		GSerializedExportLinker = Linker;
		GSerializedExportIndex = Index;
	}

	/** Destructor. Called upon CreateImport() exit. */
	~FScopedCreateExportCounter()
	{
		GSerializedExportLinker = NULL;
		GSerializedExportIndex = INDEX_NONE;
	}
};


/*-----------------------------------------------------------------------------
	FObjectResource
-----------------------------------------------------------------------------*/

FObjectResource::FObjectResource()
{}

FObjectResource::FObjectResource( UObject* InObject )
:	ObjectName		( InObject ? InObject->GetFName() : FName(NAME_None)		)
,	OuterIndex		( 0															)
{
}

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

FObjectExport::FObjectExport()
:	FObjectResource	()
,	ClassIndex		( 0															)
,	SuperIndex		( 0															)
,	ArchetypeIndex	( 0															)
,	ObjectFlags		( 0															)
,	SerialSize		( 0															)
,	SerialOffset	( 0															)
,	ScriptSerializationStartOffset	( 0											)
,	ScriptSerializationEndOffset	( 0											)
,	_Object			( NULL														)
,	_iHashNext		( INDEX_NONE												)
,	ExportFlags		( EF_None													)
,	PackageGuid		( FGuid(0,0,0,0)											)
{}

FObjectExport::FObjectExport( UObject* InObject )
:	FObjectResource	( InObject													)
,	ClassIndex		( 0															)
,	SuperIndex		( 0															)
,	ArchetypeIndex	( 0															)
,	ObjectFlags		( InObject ? InObject->GetMaskedFlags(RF_Load) : 0			)
,	SerialSize		( 0															)
,	SerialOffset	( 0															)
,	ScriptSerializationStartOffset	( 0											)
,	ScriptSerializationEndOffset	( 0											)
,	_Object			( InObject													)
,	_iHashNext		( INDEX_NONE												)
,	ExportFlags		( EF_None													)
,	PackageGuid		( FGuid(0,0,0,0)											)
,	PackageFlags	( 0															)
{
	// Objects that are forced into the export table need to be dissociated after
	// loading as they might have been associated via FindObject and will have
	// their linker reset so they won't correctly dissociate with this export via 
	// SetLinker in the GC code.
	if( _Object && _Object->HasAnyFlags( RF_ForceTagExp ) )
	{
		UObject::GForcedExportCount++;
		SetFlags( EF_ForcedExport );
	}
}

FArchive& operator<<( FArchive& Ar, FObjectExport& E )
{
	Ar << E.ClassIndex;
	Ar << E.SuperIndex;
	Ar << E.OuterIndex;
	Ar << E.ObjectName;
	Ar << E.ArchetypeIndex;	
	Ar << E.ObjectFlags;

	Ar << E.SerialSize;
	Ar << E.SerialOffset;

	if( Ar.Ver() < VER_REMOVED_COMPONENT_MAP )
	{
		TMap<FName,INT>	LegacyComponentMap;
		Ar << LegacyComponentMap;
	}

	Ar << E.ExportFlags;

	Ar << E.GenerationNetObjectCount;
	Ar << E.PackageGuid;
	Ar << E.PackageFlags;

	return Ar;
}

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

FObjectImport::FObjectImport()
:	FObjectResource	()
{}

FObjectImport::FObjectImport( UObject* InObject )
:	FObjectResource	( InObject																)
,	ClassPackage	( InObject ? InObject->GetClass()->GetOuter()->GetFName()	: NAME_None	)
,	ClassName		( InObject ? InObject->GetClass()->GetFName()				: NAME_None	)
,	XObject			( InObject																)
,	SourceLinker	( NULL																	)
,	SourceIndex		( INDEX_NONE															)
{
	if( XObject != NULL )
	{
		UObject::GImportCount++;
	}
}

FArchive& operator<<( FArchive& Ar, FObjectImport& I )
{
	Ar << I.ClassPackage << I.ClassName;
	Ar << I.OuterIndex;
	Ar << I.ObjectName;
	if( Ar.IsLoading() )
	{
		I.SourceLinker	= NULL;
		I.SourceIndex	= INDEX_NONE;
		I.XObject		= NULL;
	}
	return Ar;
}

/*----------------------------------------------------------------------------
	FCompressedChunk.
----------------------------------------------------------------------------*/

FCompressedChunk::FCompressedChunk()
:	UncompressedOffset(0)
,	UncompressedSize(0)
,	CompressedOffset(0)
,	CompressedSize(0)
{}

/** I/O function */
FArchive& operator<<(FArchive& Ar,FCompressedChunk& Chunk)
{
	Ar << Chunk.UncompressedOffset;
	Ar << Chunk.UncompressedSize;
	Ar << Chunk.CompressedOffset;
	Ar << Chunk.CompressedSize;
	return Ar;
}


/*----------------------------------------------------------------------------
	Items stored in Unreal files.
----------------------------------------------------------------------------*/

FGenerationInfo::FGenerationInfo(INT InExportCount, INT InNameCount, INT InNetObjectCount)
: ExportCount(InExportCount), NameCount(InNameCount), NetObjectCount(InNetObjectCount)
{}

/** I/O function
 * we use a function instead of operator<< so we can pass in the package file summary for version tests, since Ar.Ver() hasn't been set yet
 */
void FGenerationInfo::Serialize(FArchive& Ar, const struct FPackageFileSummary& Summary)
{
	Ar << ExportCount << NameCount;
	Ar << NetObjectCount;
}

FArchive& operator<<( FArchive& Ar, FTextureAllocations::FTextureType& TextureType );
FArchive& operator<<( FArchive& Ar, FTextureAllocations& TextureAllocations );

FPackageFileSummary::FPackageFileSummary()
{
	appMemzero( this, sizeof(*this) );
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	Ar << Sum.Tag;
	// only keep loading if we match the magic
	if( Sum.Tag == PACKAGE_FILE_TAG || Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
	{
		// The package has been stored in a separate endianness than the linker expected so we need to force
		// endian conversion. Latent handling allows the PC version to retrieve information about cooked packages.
		if( Sum.Tag == PACKAGE_FILE_TAG_SWAPPED )
		{
			// Set proper tag.
			Sum.Tag = PACKAGE_FILE_TAG;
			// Toggle forced byte swapping.
			if( Ar.ForceByteSwapping() )
			{
				Ar.SetByteSwapping( FALSE );
			}
			else
			{
				Ar.SetByteSwapping( TRUE );
			}
		}

		Ar << Sum.FileVersion;
		Ar << Sum.TotalHeaderSize;
		Ar << Sum.FolderName;
		Ar << Sum.PackageFlags;
		if( Sum.PackageFlags & PKG_FilterEditorOnly )
		{
			Ar.SetFilterEditorOnly(TRUE);
		}
		Ar << Sum.NameCount     << Sum.NameOffset;
		Ar << Sum.ExportCount   << Sum.ExportOffset;
		Ar << Sum.ImportCount   << Sum.ImportOffset;
		Ar << Sum.DependsOffset;

		if (Sum.GetFileVersion() >= VER_ADDED_CROSSLEVEL_REFERENCES)
		{
			Ar << Sum.ImportExportGuidsOffset << Sum.ImportGuidsCount << Sum.ExportGuidsCount;
		}
		else
		{
			Sum.ImportExportGuidsOffset = INDEX_NONE;
		}

		if( Sum.GetFileVersion() >= VER_ASSET_THUMBNAILS_IN_PACKAGES )
		{
			Ar << Sum.ThumbnailTableOffset;
		}

		INT GenerationCount = Sum.Generations.Num();
		Ar << Sum.Guid << GenerationCount;
		if( Ar.IsLoading() && GenerationCount > 0 )
		{
			Sum.Generations = TArray<FGenerationInfo>( GenerationCount );
		}
		for( INT i=0; i<GenerationCount; i++ )
		{
			Sum.Generations(i).Serialize(Ar, Sum);
		}

		Ar << Sum.EngineVersion;

		// grab the CookedContentVersion if we are saving while cooking or loading 
		if( ( GIsCooking == TRUE ) || ( Ar.IsLoading() == TRUE ) )
		{
			Ar << Sum.CookedContentVersion;
		}
		else
		{
			INT Temp = 0;
			Ar << Temp;  // just put a zero as it not a cooked package and we should not dirty the waters
		}

		Ar << Sum.CompressionFlags;
		Ar << Sum.CompressedChunks;
		Ar << Sum.PackageSource;

		// serialize the array of additional packages to cook if the version is high enough
		if (Sum.GetFileVersion() >= VER_ADDITIONAL_COOK_PACKAGE_SUMMARY)
		{
			Ar << Sum.AdditionalPackagesToCook;
		}

		if (Sum.GetFileVersion() >= VER_TEXTURE_PREALLOCATION )
		{
			Ar << Sum.TextureAllocations;
		}
	}

	return Ar;
}

/*----------------------------------------------------------------------------
	ULinker.
----------------------------------------------------------------------------*/

ULinker::ULinker( UPackage* InRoot, const TCHAR* InFilename )
:	LinkerRoot( InRoot )
,	Summary()
,	Filename( InFilename )
,	_ContextFlags( 0 )
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	check(LinkerRoot);
	check(InFilename);

	// Set context flags.
	// if we're cooking for a console, do not load editor-only objects.
	if( GIsEditor && (!GIsCooking || (GCookingTarget & UE3::PLATFORM_Stripped) == 0) )
	{
		_ContextFlags |= RF_LoadForEdit;
	}
	if( GIsClient )
	{
		_ContextFlags |= RF_LoadForClient;
	}
	if( GIsServer )
	{
		_ContextFlags |= RF_LoadForServer;
	}
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void ULinker::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULinker, LinkerRoot ) );
	const DWORD SkipIndexIndex = TheClass->EmitStructArrayBegin( STRUCT_OFFSET( ULinker, ImportMap ), sizeof(FObjectImport) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( FObjectImport, SourceLinker ) );
	TheClass->EmitStructArrayEnd( SkipIndexIndex );
}

void ULinker::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsCountingMemory() )
	{
		// Can't use CountBytes as ExportMap is array of structs of arrays.
		Ar << ImportMap;
		Ar << ExportMap;
		Ar << DependsMap;
	}

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
/**
 * Return the path name of the UObject represented by the specified import. 
 * (can be used with StaticFindObject)
 * 
 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
 *
 * @return	the path name of the UObject represented by the resource at ImportIndex
 */
FString ULinker::GetImportPathName(INT ImportIndex)
{
	ULinkerLoad* Loader = Cast<ULinkerLoad>(this);

	FString Result;
	for (INT LinkerIndex = -ImportIndex - 1; LinkerIndex != ROOTPACKAGE_INDEX;)
	{
		FObjectResource Resource;
		UBOOL bSubobjectDelimiter=FALSE;

		// seek free loading can make import's outers be exports, not imports
		if ( (LinkerRoot->PackageFlags & PKG_Cooked) != 0 && !IS_IMPORT_INDEX(LinkerIndex) )
		{
			Resource = ExportMap(LinkerIndex - 1);

			// if this import is not a UPackage but this import's Outer is a UPackage, we need to use subobject notation
			if (Result.Len() > 0 && Loader != NULL && Loader->GetExportClassName(LinkerIndex-1) != NAME_Package
			&& (Resource.OuterIndex == ROOTPACKAGE_INDEX || Loader->GetExportClassName(Resource.OuterIndex - 1) == NAME_Package) )
			{
				bSubobjectDelimiter = TRUE;
			}
		}
		else
		{
			Resource = ImportMap(-LinkerIndex-1);
			if ( Result.Len() > 0 && Loader != NULL && ImportMap(-LinkerIndex-1).ClassName != NAME_Package
			&&	((IS_IMPORT_INDEX(Resource.OuterIndex) && ImportMap(-Resource.OuterIndex - 1).ClassName == NAME_Package)
			||	(!IS_IMPORT_INDEX(Resource.OuterIndex) && Loader->GetExportClassName(Resource.OuterIndex - 1) == NAME_Package)) )
			{
				bSubobjectDelimiter = TRUE;
			}
		}

		// don't append a dot in the first iteration
		if ( Result.Len() > 0 )
		{
			if ( bSubobjectDelimiter )
			{
				Result = US + SUBOBJECT_DELIMITER + Result;
			}
			else
			{
				Result = US + TEXT(".") + Result;
			}
		}

		Result = Resource.ObjectName.ToString() + Result;
		LinkerIndex = Resource.OuterIndex;
	}
	return Result;
}

/**
 * Return the path name of the UObject represented by the specified export.
 * (can be used with StaticFindObject)
 * 
 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
 * @param	bResolveForcedExports	if TRUE, the package name part of the return value will be the export's original package,
 *									not the name of the package it's currently contained within.
 *
 * @return	the path name of the UObject represented by the resource at ExportIndex
 */
FString ULinker::GetExportPathName(INT ExportIndex, const TCHAR* FakeRoot,UBOOL bResolveForcedExports/*=FALSE*/)
{
	FString Result;
	ULinkerLoad* Loader = Cast<ULinkerLoad>(this);

	UBOOL bForcedExport = FALSE;
	for ( PACKAGE_INDEX LinkerIndex = ExportIndex + 1; LinkerIndex != ROOTPACKAGE_INDEX; LinkerIndex = ExportMap(LinkerIndex - 1).OuterIndex )
	{ 
		const FObjectExport Export = ExportMap(LinkerIndex-1);

		// don't append a dot in the first iteration
		if ( Result.Len() > 0 )
		{
			// if this export is not a UPackage but this export's Outer is a UPackage, we need to use subobject notation
			if (Loader != NULL
			&&	(	Export.OuterIndex == ROOTPACKAGE_INDEX
				||	Loader->GetExportClassName(Export.OuterIndex - 1) == NAME_Package)
			&&	Loader->GetExportClassName(LinkerIndex-1) != NAME_Package)
			{
				Result = US + SUBOBJECT_DELIMITER + Result;
			}
			else
			{
				Result = US + TEXT(".") + Result;
			}
		}
		Result = Export.ObjectName.ToString() + Result;
		bForcedExport = bForcedExport || Export.HasAnyFlags(EF_ForcedExport);
	}

	if ( bForcedExport && FakeRoot == NULL && bResolveForcedExports )
	{
		// Result already contains the correct path name for this export
		return Result;
	}

	return (FakeRoot ? FakeRoot : LinkerRoot->GetPathName()) + TEXT(".") + Result;
}

/**
 * Return the full name of the UObject represented by the specified import.
 * 
 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
 *
 * @return	the full name of the UObject represented by the resource at ImportIndex
 */
FString ULinker::GetImportFullName(INT ImportIndex)
{
	return ImportMap(ImportIndex).ClassName.ToString() + TEXT(" ") + GetImportPathName(ImportIndex);
}

/**
 * Return the full name of the UObject represented by the specified export.
 * 
 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
 * @param	bResolveForcedExports	if TRUE, the package name part of the return value will be the export's original package,
 *									not the name of the package it's currently contained within.
 *
 * @return	the full name of the UObject represented by the resource at ExportIndex
 */
FString ULinker::GetExportFullName(INT ExportIndex, const TCHAR* FakeRoot,UBOOL bResolveForcedExports/*=FALSE*/)
{
	INT ClassIndex = ExportMap(ExportIndex).ClassIndex;
	FName ClassName = ClassIndex > 0 ? ExportMap(ClassIndex - 1).ObjectName : ClassIndex < 0 ? ImportMap(-ClassIndex - 1).ObjectName : FName(NAME_Class);

	return ClassName.ToString() + TEXT(" ") + GetExportPathName(ExportIndex, FakeRoot, bResolveForcedExports);
}


/** 
 * This will return the name given an index and number
 *
 * @param	Index					Index in to the name table of this linker
 * @param	Number					Instance number appended to end of Name if > 0
 *
 * @return	Constructed name corresponding to Index and Number
 **/
FName ULinker::IndexToName( NAME_INDEX Index, INT Number ) const
{
	FName Name;
	if( !NameMap.IsValidIndex( Index ) )
	{
		appErrorf( TEXT( "BOOM" ) );// LocalizeError(TEXT("NameIndex"),TEXT("Core")), Index, ExportMap.Num() );
	}

	if (NameMap(Index) == NAME_None)
	{
		// Ignore Number for NAME_None
		Name = NAME_None;
	}
	else
	{		
		// simply create the name from the NameMap's name index and the serialized instance number
		Name = FName((EName)NameMap(Index).GetIndex(), Number);
	}

	return Name;
}


//@script patcher
/**
 * Returns the index [into the ExportMap array] for the first export which has the EF_ScriptPatcherExport flag, or INDEX_NONE if there are none.
 */
INT ULinker::FindFirstPatchedExportIndex() const
{
	INT Result = INDEX_NONE;
	for ( INT ExportIndex = ExportMap.Num() - 1; ExportIndex >= 0; ExportIndex-- )
	{
		if ( !ExportMap(ExportIndex).HasAnyFlags(EF_ScriptPatcherExport) )
		{
			if ( ExportIndex < ExportMap.Num() - 1 )
			{
				Result = ExportIndex + 1;
			}
			break;
		}
	}

	return Result;
}


/**
 * Tell this linker to start SHA calculations
 */
void ULinker::StartScriptSHAGeneration()
{
	// create it if needed
	if (ScriptSHA == NULL)
	{
		ScriptSHA = new FSHA1;
	}

	// make sure it's reset
	ScriptSHA->Reset();
}

/**
 * If generating a script SHA key, update the key with this script code
 *
 * @param ScriptCode Code to SHAify
 */
void ULinker::UpdateScriptSHAKey(const TArray<BYTE>& ScriptCode)
{
	// if we are doing SHA, update it
	if (ScriptSHA && ScriptCode.Num())
	{
		ScriptSHA->Update((BYTE*)ScriptCode.GetTypedData(), ScriptCode.Num());
	}
}

/**
 * After generating the SHA key for all of the 
 *
 * @param OutKey Storage for the key bytes (20 bytes)
 */
void ULinker::GetScriptSHAKey(BYTE* OutKey)
{
	check(ScriptSHA);

	// finish up the calculation, and return it
	ScriptSHA->Final();
	ScriptSHA->GetHash(OutKey);
}

void ULinker::BeginDestroy()
{
	Super::BeginDestroy();

	// free any SHA memory
	delete ScriptSHA;
}


/*----------------------------------------------------------------------------
	ULinkerLoad.
----------------------------------------------------------------------------*/
#if SUPPORTS_SCRIPTPATCH_LOADING
//@script patcher
FScriptPatcher* ULinkerLoad::ScriptPatcher = NULL;
#endif

namespace ULinkerDefs
{
	/** Number of progress steps for reporting status to a GUI while loading packages */
	const INT TotalProgressSteps = 6;
}



#if SUPPORTS_SCRIPTPATCH_LOADING
//@script patcher
/**
 * Returns a pointer to the shared script patcher object.
 */
FScriptPatcher* ULinkerLoad::GetScriptPatcher()
{
	if ( ScriptPatcher == NULL )
	{
		ScriptPatcher = new FScriptPatcher();
	}

	return ScriptPatcher;
}

//@script patcher
/**
 * Returns a pointer to the shared script patcher object if it has already been created, NULL otherwise
 */
FScriptPatcher* ULinkerLoad::GetExistingScriptPatcher()
{
	// return it if it exists, or NULL otherwise
	return ScriptPatcher;
}


//@script patcher
/**
 * Creates a patch archive for this linker if one doesn't already exist.
 */
void ULinkerLoad::CreatePatchReader()
{
	if ( PatchDataAr == NULL )
	{
		PatchDataAr = new FPatchReader;
		PatchDataAr->SetLoader(this);
	}
}
#endif

/*-----------------------------------------------------------------------------
	ULinkerLoad creation BEGIN
-----------------------------------------------------------------------------*/


/**
 * Exception save guard to ensure GSerializedPackageLinker is reset after this
 * class goes out of scope.
 */
class FSerializedPackageLinkerGuard
{
	/** Pointer to restore to after going out of scope. */
	ULinkerLoad* PrevSerializedPackageLinker;
public:
	FSerializedPackageLinkerGuard() 
	:	PrevSerializedPackageLinker(GSerializedPackageLinker) 
	{}
	~FSerializedPackageLinkerGuard() 
	{ 
		GSerializedPackageLinker = PrevSerializedPackageLinker; 
	}
};

/**
 * Creates and returns a ULinkerLoad object.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 *
 * @return	new ULinkerLoad object for Parent/ Filename
 */
ULinkerLoad* ULinkerLoad::CreateLinker( UPackage* Parent, const TCHAR* Filename, DWORD LoadFlags )
{
	ULinkerLoad* Linker = CreateLinkerAsync( Parent, Filename, LoadFlags );
	{
		FSerializedPackageLinkerGuard Guard;	
		GSerializedPackageLinker = Linker;
		Linker->Tick( 0.f, FALSE );
	}
	return Linker;
}

/**
 * Looks for an existing linker for the given package, without trying to make one if it doesn't exist
 */
ULinkerLoad* ULinkerLoad::FindExistingLinkerForPackage(UPackage* Package)
{
	// See whether there already is a linker for this parent/ linker root.
	ULinkerLoad* Linker = NULL;
	if (Package)
	{
		for (INT LoaderIndex=0; LoaderIndex<GObjLoaders.Num(); LoaderIndex++)
		{
			if (UObject::GetLoader(LoaderIndex)->LinkerRoot == Package)
			{
				Linker = UObject::GetLoader(LoaderIndex);
				break;
			}
		}
	}

	return Linker;
}

/**
 * Creates a ULinkerLoad object for async creation. Tick has to be called manually till it returns
 * TRUE in which case the returned linker object has finished the async creation process.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 *
 * @return	new ULinkerLoad object for Parent/ Filename
 */
ULinkerLoad* ULinkerLoad::CreateLinkerAsync( UPackage* Parent, const TCHAR* Filename, DWORD LoadFlags )
{
	// See whether there already is a linker for this parent/ linker root.
	ULinkerLoad* Linker = FindExistingLinkerForPackage(Parent);
	if (Linker)
		{
			debugf(TEXT("ULinkerLoad::CreateLinkerAsync: Found existing linker for '%s'"), *Parent->GetName());
	}

	// Create a new linker if there isn't an existing one.
	if( Linker == NULL )
	{
		if( GUseSeekFreeLoading )
		{
			LoadFlags |= LOAD_SeekFree;
		}
		Linker = new ULinkerLoad( Parent, Filename, LoadFlags );
	}
	return Linker;
}

/**
 * Ticks an in-flight linker and spends InTimeLimit seconds on creation. This is a soft time limit used
 * if bInUseTimeLimit is TRUE.
 *
 * @param	InTimeLimit		Soft time limit to use if bInUseTimeLimit is TRUE
 * @param	bInUseTimeLimit	Whether to use a (soft) timelimit
 * 
 * @return	TRUE if linker has finished creation, FALSE if it is still in flight
 */
UBOOL ULinkerLoad::Tick( FLOAT InTimeLimit, UBOOL bInUseTimeLimit )
{
	UBOOL bExecuteNextStep	= TRUE;

	if( bHasFinishedInitialization == FALSE )
	{
		// Store variables used by functions below.
		TickStartTime		= appSeconds();
		bTimeLimitExceeded	= FALSE;
		bUseTimeLimit		= bInUseTimeLimit;
		TimeLimit			= InTimeLimit;

		do
		{
#if PS3
	 		// force shut down from main thread if a shutdown request happened
			if (GIsRequestingExit)
			{
				appRequestExit(1);
			}
#endif
			// Create loader, aka FArchive used for serialization and also precache the package file summary.
			// FALSE is returned until any precaching is complete.
			if( TRUE )
			{
				bExecuteNextStep = CreateLoader();
			}

			// Serialize the package file summary and presize the various arrays (name, import & export map)
			if( bExecuteNextStep )
			{
				bExecuteNextStep = SerializePackageFileSummary();
			}

			// Serialize the name map and register the names.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = SerializeNameMap();
			}

			// Serialize the import map.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = SerializeImportMap();
			}

			// Serialize the export map.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = SerializeExportMap();
			}

			// Start pre-allocation of texture memory.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = StartTextureAllocation();
			}

#if SUPPORTS_SCRIPTPATCH_LOADING
			//@script patcher
			if ( bExecuteNextStep )
			{
				bExecuteNextStep = IntegrateScriptPatches();
			}
#endif

			// Fix up import map for backward compatible serialization.
			if( bExecuteNextStep )
			{	
				bExecuteNextStep = FixupImportMap();
			}

			if ( bExecuteNextStep )
			{
				bExecuteNextStep = RemapClasses();
			}

#if SUPPORTS_SCRIPTPATCH_CREATION
			//@script patcher
			if ( bExecuteNextStep )
			{
				RemapLinkerPackageNames();
			}
#endif
			// Fix up export map for object class conversion 
			if( bExecuteNextStep )
			{	
				bExecuteNextStep = FixupExportMap();
			}

			if ( bExecuteNextStep )
			{
				RemapLinkerPackageNamesForMultilanguageCooks();
			}

			// Serialize the dependency map.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = SerializeDependsMap();
			}

			// Serialize the import/export guids map.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = SerializeGuidMaps();
			}

			// Hash exports.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = CreateExportHash();
			}

			// Find existing objects matching exports and associate them with this linker.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = FindExistingExports();
			}

			// Finalize creation process.
			if( bExecuteNextStep )
			{
				bExecuteNextStep = FinalizeCreation();
			}
#if WITH_GFx && PS3
            // Ordinary package loading can be triggered during gameplay by ActionScript.
            // Prevent infinite loops waiting for AsyncIO thread to perform any work
            if (!bUseTimeLimit && !bExecuteNextStep)
			{
                appSleep(0);
			}
#endif
		}
		// Loop till we are done if no time limit is specified.
		while( !bUseTimeLimit && !bExecuteNextStep );
	}

	// Return whether we completed or not.
	return bExecuteNextStep;
}

/**
 * Private constructor, passing arguments through from CreateLinker.
 *
 * @param	Parent		Parent object to load into, can be NULL (most likely case)
 * @param	Filename	Name of file on disk to load
 * @param	LoadFlags	Load flags determining behavior
 */
ULinkerLoad::ULinkerLoad( UPackage* InParent, const TCHAR* InFilename, DWORD InLoadFlags )
:	ULinker( InParent, InFilename )
,	LoadFlags( InLoadFlags )
,	bHaveImportsBeenVerified( FALSE )
//@script patcher
,	bHasIntegratedNamePatches(FALSE), bHasIntegratedImportPatches(FALSE), bHasIntegratedExportPatches(FALSE)
,	bHasIntegratedScriptPatches(FALSE), bHasIntegratedDefaultsPatches(FALSE), bHasIntegratedEnumPatches(FALSE)
,	bHasRemappedExternalPackageReferences(FALSE), PatchDataAr(NULL), OriginalLoader(NULL)
,	PotentialCrossLevelOwner(NULL), PotentialCrossLevelProperty(NULL)
{	

	// default to nothing to do
	bHasRemappedExternalPackageReferencesForMultilanguageCooks = TRUE;

	const UBOOL bIsEnglish = appStricmp( TEXT("INT"), UObject::GetLanguage() ) == 0;
	const UBOOL bIsSeekFree = LoadFlags & LOAD_SeekFree;
	// english loc packages don't need this correction, nor do we need it when we aren't seekfree
	if (!bIsEnglish && bIsSeekFree)
	{
		FString FilenameString(InFilename);
		FString Search = TEXT("_LOC_");
		Search += UObject::GetLanguage();
		Search += TEXT(".");
		if (FilenameString.InStr(Search,FALSE,TRUE) != INDEX_NONE)
		{
			bHasRemappedExternalPackageReferencesForMultilanguageCooks = FALSE;
		}
	}

	check(!HasAnyFlags(RF_ClassDefaultObject));
}

/**
 * Returns whether the time limit allotted has been exceeded, if enabled.
 *
 * @param CurrentTask	description of current task performed for logging spilling over time limit
 * @param Granularity	Granularity on which to check timing, useful in cases where appSeconds is slow (e.g. PC)
 *
 * @return TRUE if time limit has been exceeded (and is enabled), FALSE otherwise (including if time limit is disabled)
 */
UBOOL ULinkerLoad::IsTimeLimitExceeded( const TCHAR* CurrentTask, INT Granularity )
{
	IsTimeLimitExceededCallCount++;
	if( !bTimeLimitExceeded 
	&&	bUseTimeLimit 
	&&  (IsTimeLimitExceededCallCount % Granularity) == 0 )
	{
		DOUBLE CurrentTime = appSeconds();
		bTimeLimitExceeded = CurrentTime - TickStartTime > TimeLimit;
#if CONSOLE
		// Log single operations that take longer than timelimit.
		if( (CurrentTime - TickStartTime) > (2.5 * TimeLimit) )
		{
 			debugfSuppressed(NAME_DevStreaming,TEXT("ULinkerLoad: %s took (less than) %5.2f ms"), 
 				CurrentTask, 
 				(CurrentTime - TickStartTime) * 1000);
		}
#endif
	}
	return bTimeLimitExceeded;
}

/**
 * Creates loader used to serialize content.
 */
UBOOL ULinkerLoad::CreateLoader()
{
	static UBOOL bAlreadyInitialized_CreateActiveRedirectsMap = FALSE;
#if !FINAL_RELEASE
	if( bAlreadyInitialized_CreateActiveRedirectsMap == FALSE )
	{
		CreateActiveRedirectsMap( GEngineIni );
		bAlreadyInitialized_CreateActiveRedirectsMap = TRUE;
	}
#endif

	if( !Loader )
	{
		UBOOL bIsSeekFree = LoadFlags & LOAD_SeekFree;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			FString CleanFilename = FFilename( *Filename ).GetCleanFilename();
			GWarn->StatusUpdatef( 0, ULinkerDefs::TotalProgressSteps, LocalizeSecure(LocalizeProgress(TEXT("Loading"),TEXT("Core")), *CleanFilename) );
		}

		// NOTE: Precached memory read gets highest priority, then memory reader, then seek free, then normal

		// check to see if there is was an async preload request for this file
		FPackagePrecacheInfo* PrecacheInfo = PackagePrecacheMap.Find(*Filename);
		// if so, serialize from memory (note this will have uncompressed a fully compressed package)
		if (PrecacheInfo)
		{
			// block until the async read is complete
			if( PrecacheInfo->SynchronizationObject->GetValue() != 0 )
			{
				DOUBLE StartTime = appSeconds();
				while (PrecacheInfo->SynchronizationObject->GetValue() != 0)
				{
					SHUTDOWN_IF_EXIT_REQUESTED;
					appSleep(0);
				}
				FLOAT WaitTime = appSeconds() - StartTime;
				debugf(NAME_Init,TEXT("Waited %.3f sec for async package '%s' to complete caching."), WaitTime, *Filename);
			}

			// create a buffer reader using the read in data
			// assume that all precached startup packages have SHA entries
			Loader = new FBufferReaderWithSHA(PrecacheInfo->PackageData, PrecacheInfo->PackageDataSize, TRUE, *Filename, TRUE);

			// remove the precache info from the map
			PackagePrecacheMap.Remove(*Filename);
		}
		// if we're seekfree loading, and we got here, that means we are trying to read a fully compressed package
		// without an async precache... this should no longer happen
		else if( GUseSeekFreeLoading && GFileManager->UncompressedFileSize(*Filename) != -1 )
		{
			appErrorf(TEXT("Cannot load %s. Loading a fully compressed package should only happen during startup with precaching."), *Filename);
		}
		// if we aren't using seek free loading, check for the presence of a .uncompressed_size manifest file
		// if it exists, then the package was fully compressed, and must be uncompressed before using it
		else if( !GUseSeekFreeLoading && GFileManager->FileSize(*(Filename + TEXT(".uncompressed_size"))) != -1 )
		{
			FString SizeString;
			appLoadFileToString(SizeString, *(Filename + TEXT(".uncompressed_size")));
			check(SizeString.Len());
		
			// get the uncompressed size from the file
			INT UncompressedSize = appAtoi(*SizeString);

			// allocate space for uncompressed file
			void* Buffer = appMalloc(UncompressedSize);

			// open the file
            FArchive* CompressedFileReader = GFileManager->CreateFileReader(*Filename);
			check(CompressedFileReader);

			// read in and decompress data
			CompressedFileReader->SerializeCompressed(Buffer, UncompressedSize, GBaseCompressionMethod);

			// close the file
			delete CompressedFileReader;

			UBOOL bHasHashEntry = FSHA1::GetFileSHAHash(*Filename, NULL);
			if( bHasHashEntry )
			{
				// create buffer reader and spawn SHA verify when it gets closed
				Loader = new FBufferReaderWithSHA(Buffer, UncompressedSize, TRUE, *Filename, TRUE);
			}
			else
			{
				// create a buffer reader
				Loader = new FBufferReader(Buffer, UncompressedSize, TRUE, TRUE);
			}
		}
		else if ((LoadFlags & LOAD_MemoryReader) || !bIsSeekFree)
		{
			// Create file reader used for serialization.
			FArchive* FileReader = GFileManager->CreateFileReader( *Filename, 0, GError );
			if( !FileReader )
			{
				appThrowf( *FString::Printf(LocalizeSecure(LocalizeError(TEXT("OpenFailed"),TEXT("Core")), *Filename, *GFileManager->GetCurrentDirectory())) );
			}

			UBOOL bHasHashEntry = FSHA1::GetFileSHAHash(*Filename, NULL);
			// force preload into memory if file has an SHA entry
			if( LoadFlags & LOAD_MemoryReader || 
				bHasHashEntry )
			{
					// Serialize data from memory instead of from disk.
					check(FileReader);
					UINT	BufferSize	= FileReader->TotalSize();
					void*	Buffer		= appMalloc( BufferSize );
					FileReader->Serialize( Buffer, BufferSize );
					if( bHasHashEntry )
					{
						// create buffer reader and spawn SHA verify when it gets closed
						Loader = new FBufferReaderWithSHA( Buffer, BufferSize, TRUE, *Filename, TRUE );
					}
					else
					{
						// create a buffer reader
						Loader = new FBufferReader( Buffer, BufferSize, TRUE, TRUE );
					}
					delete FileReader;
			}
			else
			{
				// read directly from file
				Loader = FileReader;
			}
		}
		else if (bIsSeekFree)
		{
			// Use the async archive as it supports proper Precache and package compression.
			Loader = new FArchiveAsync( *Filename );

			// An error signifies that the package couldn't be opened.
			if( Loader->IsError() )
			{
				delete Loader;
				appThrowf( *FString::Printf(LocalizeSecure(LocalizeError(TEXT("OpenFailed"),TEXT("Core")), *Filename, *GFileManager->GetCurrentDirectory())) );
			}
		}
		check( Loader );
		check( !Loader->IsError() );
		// save off the original disk loader in case it gets replaced by the script patcher
		OriginalLoader = Loader;

		// Error if linker is already loaded.
		for( INT i=0; i<GObjLoaders.Num(); i++ )
		{
			if( GetLoader(i)->LinkerRoot == LinkerRoot )
			{
				appThrowf( LocalizeSecure(LocalizeError(TEXT("LinkerExists"),TEXT("Core")), *LinkerRoot->GetName()) );
			}
		}

		// Set status info.
		ArVer			= GPackageFileVersion;
		ArLicenseeVer	= GPackageFileLicenseeVersion;
		ArIsLoading		= TRUE;
		ArIsPersistent	= TRUE;
		ArForEdit		= GIsEditor;
		ArForClient		= TRUE;
		ArForServer		= TRUE;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 1, ULinkerDefs::TotalProgressSteps );
		}
	}

	UBOOL bExecuteNextStep = TRUE;
	if( bHasSerializedPackageFileSummary == FALSE )
	{
		// Precache up to one ECC block before serializing package file summary.
		// If the package is partially compressed, we'll know that quickly and
		// end up discarding some of the precached data so we can re-fetch
		// and decompress it.
		INT PrecacheSize = Min( DVD_ECC_BLOCK_SIZE, Loader->TotalSize() );
		check( PrecacheSize > 0 );
		// Wait till we're finished precaching before executing the next step.
		bExecuteNextStep = Loader->Precache( 0, PrecacheSize);
	}

	return bExecuteNextStep && !IsTimeLimitExceeded( TEXT("creating loader") );
}

/**
 * Serializes the package file summary.
 */
UBOOL ULinkerLoad::SerializePackageFileSummary()
{
	if( bHasSerializedPackageFileSummary == FALSE )
	{
		// Read summary from file.
		*this << Summary;

#if CONSOLE
		checkf((Summary.PackageFlags&PKG_ContainsDebugInfo)==0, TEXT("%s contains debug script!  Recompile scripts in Release mode and recook."), *LinkerRoot->GetName());
#endif

		// Mark both this linker load and the actual archive used for serialization as containing cooked data.
		if( Summary.PackageFlags & PKG_Cooked )
		{
			ThisContainsCookedData();
			Loader->ThisContainsCookedData();
		}

		// editor can load imports when opening cooked data
		if (GIsEditor)
		{
			Summary.PackageFlags &= ~PKG_RequireImportsAlreadyLoaded;
			if ( GIsUCCMake )
			{
				// Clear the cook flag when running make so that CreateExport is forced
				// to load from disk instead of trying to find in memory.
				Summary.PackageFlags &= ~PKG_Cooked;
			}
		}

		// Loader needs to be the same version.
		Loader->SetVer(Summary.GetFileVersion());
		Loader->SetLicenseeVer(Summary.GetFileVersionLicensee());
		ArVer = Summary.GetFileVersion();
		ArLicenseeVer = Summary.GetFileVersionLicensee();

		// Package has been stored compressed.
#if DEBUG_DISTRIBUTED_COOKING
		if( FALSE )
#else
		if( Summary.PackageFlags & PKG_StoreCompressed )
#endif
		{
			// Set compression mapping. Failure means Loader doesn't support package compression.
			check( Summary.CompressedChunks.Num() );
			if( !Loader->SetCompressionMap( &Summary.CompressedChunks, (ECompressionFlags) Summary.CompressionFlags ) )
			{
				// Current loader doesn't support it, so we need to switch to one known to support it.
				
				// We need keep track of current position as we already serialized the package file summary.
				INT		CurrentPos				= Loader->Tell();
				// Serializing the package file summary determines whether we are forcefully swapping bytes
				// so we need to propage this information from the old loader to the new one.
				UBOOL	bHasForcedByteSwapping	= Loader->ForceByteSwapping();

				// Delete existing loader...
				delete Loader;
				// ... and create new one using FArchiveAsync as it supports package compression.
				Loader = new FArchiveAsync( *Filename );
				check( !Loader->IsError() );

				// update the OriginalLoader pointer
				OriginalLoader = Loader;
				
				// Seek to current position as package file summary doesn't need to be serialized again.
				Loader->Seek( CurrentPos );
				// Propagate byte-swapping behavior.
				Loader->SetByteSwapping( bHasForcedByteSwapping );
				
				// Set the compression map and verify it won't fail this time.
				verify( Loader->SetCompressionMap( &Summary.CompressedChunks, (ECompressionFlags) Summary.CompressionFlags ) );
			}
		}

		UPackage* LinkerRootPackage = LinkerRoot;
		if( LinkerRootPackage )
		{
			// Propagate package flags, except the trash flag (which will be reset below if the package is still in the trash)
			LinkerRootPackage->PackageFlags = Summary.PackageFlags & ~PKG_Trash;

			// Propagate package folder name
			LinkerRootPackage->SetFolderName(*Summary.FolderName);

			if( Summary.EngineVersion > GEngineVersion )
			{
				// Warn user that package isn't saveable.
				if( GIsEditor )
				{
					// Only add the error if we haven't already seen it for this package
					if( ( LinkerRootPackage->PackageFlags & PKG_SavedWithNewerVersion ) == 0 )
					{
						GWarn->MapCheck_Add( MCTYPE_ERROR, NULL, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_SavedWithNewerVersion" ), *Filename, GEngineVersion, Summary.EngineVersion ) ), TEXT( "SavedWithNewerVersion" ) );
					}
				}
				else
				{
					debugf( NAME_Warning, LocalizeSecure(LocalizeError(TEXT("SavedWithNewerVersion"),TEXT("Core")), *Filename,GEngineVersion, Summary.EngineVersion) );
				}

				// Mark package as having been saved with a version that is newer then the current one. This is done
				// to later on avoid saving the package and potentially silently clobbering important changes.
				LinkerRootPackage->PackageFlags |= PKG_SavedWithNewerVersion;
			}
		}
		
		// Propagate fact that package cannot use lazy loading to archive (aka this). Cooked packages have this flag
		// removed in the Editor.
		if( (Summary.PackageFlags & PKG_DisallowLazyLoading) && !(GIsEditor && (Summary.PackageFlags & PKG_Cooked)) )
		{
			ArAllowLazyLoading = FALSE;
		}
		else
		{
			ArAllowLazyLoading = TRUE;
		}

		// if this package is in the trashcan, mark the linker's root package as Trash
		if( LinkerRootPackage && Filename.InStr(TRASHCAN_DIRECTORY_NAME) != -1 )
		{
			LinkerRootPackage->PackageFlags |= PKG_Trash;
		}

		// Check tag.
		if( Summary.Tag != PACKAGE_FILE_TAG )
		{
			appThrowf( LocalizeSecure(LocalizeError(TEXT("BinaryFormat"),TEXT("Core")), *Filename) );
		}

		// Validate the summary.
		if( Summary.GetFileVersion() < GPackageFileMinVersion )
		{
			appThrowf( LocalizeSecure(LocalizeError(TEXT("OldVersionFile"),TEXT("Core")), *Filename, GPackageFileMinVersion, Summary.GetFileVersion() ));
		}
		
		// Don't load packages that were saved with an engine version newer than the current one.
		if( (Summary.GetFileVersion() > GPackageFileVersion) || (Summary.GetFileVersionLicensee() > GPackageFileLicenseeVersion) )
		{
			warnf(LocalizeSecure(LocalizeError(TEXT("FileVersionDump"),TEXT("Core")), *Filename, Summary.GetFileVersion(), GPackageFileVersion, Summary.GetFileVersionLicensee(), GPackageFileLicenseeVersion));
			appThrowf(LocalizeSecure(LocalizeError(TEXT("FileVersionDump"),TEXT("Core")), *Filename, Summary.GetFileVersion(), GPackageFileVersion, Summary.GetFileVersionLicensee(), GPackageFileLicenseeVersion));

		}

#if CONSOLE && !SUPPORTS_SCRIPTPATCH_LOADING
		// check that the package being loaded has the correct CookedContentVersion
 		if( (Summary.PackageFlags & PKG_Cooked) && Summary.GetCookedContentVersion() != GPackageFileCookedContentVersion )
 		{
			appErrorf( LocalizeSecure(LocalizeError(TEXT("CookedPackagedVersionOlderThanEnginePackageFileCookedContentVersion"),TEXT("Core")), *Filename, GPackageFileCookedContentVersion, Summary.GetCookedContentVersion()) );
 		}
#endif // CONSOLE

		// Slack everything according to summary.
		ImportMap   .Empty( Summary.ImportCount   );
		ExportMap   .Empty( Summary.ExportCount   );
		NameMap		.Empty( Summary.NameCount     );
		// Depends map gets pre-sized in SerializeDependsMap if used.

		// Avoid serializing it again.
		bHasSerializedPackageFileSummary = TRUE;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 2, ULinkerDefs::TotalProgressSteps );
		}
	}

	return !IsTimeLimitExceeded( TEXT("serializing package file summary") );
}

/**
 * Serializes the name table.
 */
UBOOL ULinkerLoad::SerializeNameMap()
{
	// The name map is the first item serialized. We wait till all the header information is read
	// before any serialization. @todo async, @todo seamless: this could be spread out across name,
	// import and export maps if the package file summary contained more detailed information on
	// serialized size of individual entries.
	UBOOL bFinishedPrecaching = TRUE;

	if( NameMapIndex == 0 && Summary.NameCount > 0 )
	{
		Seek( Summary.NameOffset );
		// Make sure there is something to precache first.
		if( Summary.TotalHeaderSize > 0 )
		{
			// Precache name, import and export map.
			bFinishedPrecaching = Loader->Precache( Summary.NameOffset, Summary.TotalHeaderSize - Summary.NameOffset );
		}
		// Backward compat code for VER_MOVED_EXPORTIMPORTMAPS_ADDED_TOTALHEADERSIZE.
		else
		{
			bFinishedPrecaching = TRUE;
		}
	}

	while( bFinishedPrecaching && NameMapIndex < Summary.NameCount && !IsTimeLimitExceeded(TEXT("serializing name map"),100) )
	{
		// Read the name entry from the file.
		FNameEntry NameEntry(ENAME_LinkerConstructor);
		*this << NameEntry;

		// Add it to the name table. We disregard the context flags as we don't support flags on names for final release builds.

		// now, we make sure we DO NOT split the name here because it will have been written out
		// split, and we don't want to keep splitting A_3_4_9 every time

		NameMap.AddItem( 
			NameEntry.IsUnicode() ? 
				FName(ENAME_LinkerConstructor, NameEntry.GetUniName()) : 
				FName(ENAME_LinkerConstructor, NameEntry.GetAnsiName())
			);
		NameMapIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return (NameMapIndex == Summary.NameCount) && !IsTimeLimitExceeded( TEXT("serializing name map") );
}

/**
 * Serializes the import map.
 */
UBOOL ULinkerLoad::SerializeImportMap()
{
	if( ImportMapIndex == 0 && Summary.ImportCount > 0 )
	{
		Seek( Summary.ImportOffset );
	}

	while( ImportMapIndex < Summary.ImportCount && !IsTimeLimitExceeded(TEXT("serializing import map"),100) )
	{
		FObjectImport* Import = new(ImportMap)FObjectImport;
		*this << *Import;
		ImportMapIndex++;
	}
	
	// Return whether we finished this step and it's safe to start with the next.
	return (ImportMapIndex == Summary.ImportCount) && !IsTimeLimitExceeded( TEXT("serializing import map") );
}

/**
 * Fixes up the import map, performing remapping for backward compatibility and such.
 */
UBOOL ULinkerLoad::FixupImportMap()
{
	if( bHasFixedUpImportMap == FALSE )
	{
		// Fix up imports.

		// AUTO_CLASS_REDIRECTION_BEGIN
#if !FINAL_RELEASE
		// I hate goto labels too, but this mucks with the function the least
restartFixupImportMap: 
#endif
		// AUTO_CLASS_REDIRECTION_END


		for( INT i=0; i<ImportMap.Num(); i++ )
		{
			FObjectImport& Import = ImportMap(i);

			//////////////////
 			// Remap SoundCueLocalized class to SoundCue.
 			if( Import.ObjectName == NAME_SoundCueLocalized && Import.ClassName == NAME_Class )
 			{
 				// Verify that the outer for this class is Engine, so that a licensee class named
				// SoundCueLocalized won't be remapped.
 				const PACKAGE_INDEX OuterIndex = Import.OuterIndex;
 				if ( IS_IMPORT_INDEX( OuterIndex ) )
 				{
 					if ( ImportMap(-OuterIndex-1).ObjectName == NAME_Engine )
 					{
 						Import.ObjectName = NAME_SoundCue;
 					}
 				}
 			}
			// Remap SoundCueLocalized references to SoundCue.
			else if( Import.ClassName == NAME_SoundCueLocalized && Import.ClassPackage == NAME_Engine )
			{
				Import.ClassName = NAME_SoundCue;
			}

			//////////////////
			//@hack: The below fixes up the import table so maps referring to the old SequenceObjects package look for those 
			// classes & objects in the Engine package where they were moved to.
			if( Import.ObjectName == NAME_SequenceObjects && Import.ClassName == NAME_Package )
			{
				Import.ObjectName = NAME_Engine;
			}
			if( Import.ClassPackage == NAME_SequenceObjects )
			{
				Import.ClassPackage = NAME_Engine;
			}

			// AUTO_CLASS_REDIRECTION_BEGIN
#if !FINAL_RELEASE

			FString RedirectName, ResultPackage, ResultClass;
			FName* RedirectNameObj = ObjectNameRedirects.Find(Import.ObjectName);
			FName* RedirectNameClass = ObjectNameRedirects.Find(Import.ClassName);
			INT OldOuterIndex = 0;
			if ( (RedirectNameObj && Import.ClassName == NAME_Class) || (RedirectNameClass && Import.ClassPackage != NAME_Core) )
			{
				FString NewDefaultObjectName = Import.ObjectName.ToString();
				FObjectImport OldImport = Import;
				UBOOL bUpdateOuterIndex = FALSE;
				INT ImportPackage = -1;

				// We are dealing with an object that needs to be redirected to a new classname (possibly a new package as well)

				FString stringObjectName(FString(Import.ObjectName.ToString()));
				if ( RedirectNameClass )
				{
					// This is an object instance
					RedirectName = RedirectNameClass->ToString();
				}
				else if ( RedirectNameObj && Import.ClassName == NAME_Class )
				{
					// This is a class object (needs to have its OuterIndex changed if the package is different)
					bUpdateOuterIndex = TRUE;
					RedirectName = RedirectNameObj->ToString();
				}

				// Accepts either "PackageName.ClassName" or just "ClassName"
				INT Offset = RedirectName.InStr(TEXT("."));
				if ( Offset >= 0 )
				{
					// A package class name redirect
					ResultPackage = RedirectName.Left(Offset);
					ResultClass = RedirectName.Right(RedirectName.Len() - Offset - 1);
				}
				else
				{
					// Just a class name change within the same package
					ResultPackage = Import.ClassPackage.ToString();
					ResultClass = RedirectName;
					bUpdateOuterIndex = FALSE;
				}

				// Find the OuterIndex of the current package for the Import
				for ( INT i = 0; i < ImportMap.Num(); i++ )
				{
					if ( ImportMap(i).ClassName == NAME_Package && ImportMap(i).ObjectName == Import.ClassPackage )
					{
						OldOuterIndex = i;
						break;
					}
				}
				if ( Import.OuterIndex && Import.OuterIndex == -(OldOuterIndex + 1) )
				{
					// This is a object instance that is owned by a specific package (default class instance or an archtype etc)
					// (needs its OuterIndex changed if the package is different)
					if(ResultPackage != Import.ClassPackage.ToString())
					{
						bUpdateOuterIndex = TRUE;
					}					
				}

				if ( bUpdateOuterIndex && ResultPackage.Len() > 0 )
				{
					// Reset the Import.OuterIndex to the package it is intended to be in
					for ( INT i = 0; i < ImportMap.Num(); i++ )
					{
						if ( ImportMap(i).ClassName == NAME_Package && ImportMap(i).ObjectName == FName(*ResultPackage) )
						{
							ImportPackage = i;
							break;
						}
					}
					if ( ImportPackage == -1 && FName(*ResultPackage) != NAME_Core )
					{
						// We are adding a new import to the map as we need the new package dependency added to the works
						ImportMap.Add();
						ImportMap(ImportMap.Num()-1).ClassName = NAME_Package;
						ImportMap(ImportMap.Num()-1).ClassPackage = NAME_Core;
						ImportMap(ImportMap.Num()-1).ObjectName = FName(*ResultPackage);
						ImportMap(ImportMap.Num()-1).OuterIndex = 0;
						ImportMap(ImportMap.Num()-1).XObject = 0;
						ImportMap(ImportMap.Num()-1).SourceLinker = 0;
						ImportMap(ImportMap.Num()-1).SourceIndex = -1;
						ImportPackage = ImportMap.Num() - 1;

						// Since this destroys the array, the current Import object is invalid and we must restart the whole process again
						goto restartFixupImportMap;
					}

					// Assign the new OuterIndex for a default object instance or a class itself
					if ( ImportPackage != -1 )
					{
						Import.OuterIndex = -(ImportPackage + 1);
					}
				}

				if ( RedirectNameClass && Import.ClassPackage != NAME_Core )
				{
					// Changing the package and class name of an object instance
					Import.ClassPackage = *ResultPackage;
					Import.ClassName = *ResultClass;
				}

				if ( RedirectNameObj && Import.ClassName == NAME_Class )
				{
					// Changing the object name of a class object
					Import.ObjectName = *ResultClass;
				}

				// Default objects should be converted by name as well
				if ( NewDefaultObjectName.Left(9) == FString("Default__") )
				{
					NewDefaultObjectName = FString("Default__");
					NewDefaultObjectName += *ResultClass;
					Import.ObjectName = *NewDefaultObjectName;
				}

				// Log the object redirection to the console for review
				if ( OldImport.ObjectName != Import.ObjectName || OldImport.ClassName != Import.ClassName || OldImport.ClassPackage != Import.ClassPackage || OldImport.OuterIndex != Import.OuterIndex )
				{
					debugf(TEXT("ULinkerLoad::FixupImportMap() - Pkg<%s> [Obj<%s> Cls<%s> Pkg<%s> Out<%s>] -> [Obj<%s> Cls<%s> Pkg<%s> Out<%s>]"), *LinkerRoot->GetName(),
						*OldImport.ObjectName.ToString(), *OldImport.ClassName.ToString(), *OldImport.ClassPackage.ToString(), OldImport.OuterIndex < -1 ? *ImportMap((OldImport.OuterIndex * -1) - 1).ObjectName.ToString() : TEXT("None"),
						*Import.ObjectName.ToString(), *Import.ClassName.ToString(), *Import.ClassPackage.ToString(),	Import.OuterIndex < -1 ? *ImportMap((Import.OuterIndex * -1) - 1).ObjectName.ToString() : TEXT("None"));
				}
			}

#endif //!FINAL_RELEASE
			// AUTO_CLASS_REDIRECTION_END

		}

		// Avoid duplicate work in async case.
		bHasFixedUpImportMap = TRUE;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 3, ULinkerDefs::TotalProgressSteps );
		}
	}
	return !IsTimeLimitExceeded( TEXT("fixing up import map") );
}

/**
 * Serializes the export map.
 */
UBOOL ULinkerLoad::SerializeExportMap()
{
	if( ExportMapIndex == 0 && Summary.ExportCount > 0 )
	{
		Seek( Summary.ExportOffset );
	}

	while( ExportMapIndex < Summary.ExportCount && !IsTimeLimitExceeded(TEXT("serializing export map"),100) )
	{
		FObjectExport* Export = new(ExportMap)FObjectExport;
		*this << *Export;
		ExportMapIndex++;
	}

#if !WITH_FACEFX
 	if (GIsEditor)
	{
		UPackage* LinkerRootPackage = LinkerRoot;
		if (LinkerRootPackage)
		{
			for (INT ExportIdx = 0; ExportIdx < ExportMap.Num(); ExportIdx++)
			{
				FObjectExport& ExportObj = ExportMap(ExportIdx);
				if (IS_IMPORT_INDEX(ExportObj.ClassIndex))
				{
					check(ImportMap.IsValidIndex(-ExportObj.ClassIndex-1));
					FObjectImport& Import = ImportMap(-ExportObj.ClassIndex-1);

					if ((Import.ObjectName == FName(TEXT("FaceFXAsset"))) ||
						(Import.ObjectName == FName(TEXT("FaceFXAnimSet"))))
					{
						LinkerRootPackage->PackageFlags |= PKG_ContainsFaceFXData;
						break;
					}
				}
			}
		}
	}
#endif	//!WITH_FACEFX

	// Return whether we finished this step and it's safe to start with the next.
	return (ExportMapIndex == Summary.ExportCount) && !IsTimeLimitExceeded( TEXT("serializing export map") );
}

/**
 * Changes classes for imports / exports before the objects are created.  The new class MUST be a child of the current class.
 */
UBOOL ULinkerLoad::RemapClasses()
{
	const INT FileVersion = Summary.GetFileVersion();
	if( FileVersion < VER_RENAME_MOBILEGAME_TO_SIMPLEGAME )
	{
		// Change all references from MobileGame and CastleGame to UDKBase.  For the MobileGame/UDKGame merge, the mobile classes where moved to UDKBase
		static FName MobileGameName( TEXT("MobileGame") );
		static FName CastleGameName( TEXT("CastleGame") );
		static FName SimpleGameName( TEXT("SimpleGame") );
		static FName UDKBaseName( TEXT("UDKBase") );

		for ( INT ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++ )
		{
			FObjectImport& Import = ImportMap(ImportIndex);

			if( FileVersion < VER_FIXUP_MOBILEGAME_REFS )
			{
				if ( Import.ClassName == NAME_Package && ( Import.ObjectName == MobileGameName || Import.ObjectName == CastleGameName ) )
				{
					Import.ObjectName = UDKBaseName;
				}

				if ( Import.ClassPackage == MobileGameName || Import.ClassPackage == CastleGameName )
				{
					Import.ClassPackage = UDKBaseName;
				}
			}

			if( Import.ObjectName == MobileGameName )
			{
				Import.ObjectName = SimpleGameName;
			}
		}
	}

	if ( FileVersion < VER_FIXED_PREFAB_SEQUENCES )
	{
		UBOOL bRequiresSequenceFixup = FALSE;

		static FName PrefabClassName(TEXT("Prefab"));
		static FName PrefabInstanceClassName(TEXT("PrefabInstance"));

		// Fix up imports.
		for( INT ImportIndex=0; ImportIndex<ImportMap.Num(); ImportIndex++ )
		{
			FObjectImport& Import = ImportMap(ImportIndex);

			if ( Import.ClassName == NAME_Class
			&&	(Import.ObjectName == PrefabClassName || Import.ObjectName == PrefabInstanceClassName))
			{
				bRequiresSequenceFixup = TRUE;
				break;
			}
		}

		if ( bRequiresSequenceFixup )
		{
			PACKAGE_INDEX EnginePackageIndex = 0;
			PACKAGE_INDEX SequenceClassIndex = 0;
			PACKAGE_INDEX PrefabSeqContainerClassImportIndex=0;
			PACKAGE_INDEX PrefabSeqClassImportIndex=0;
			PACKAGE_INDEX PrefabClassIndex = 0;

			static const FName PrefabSeqContainerClassName = TEXT("PrefabSequenceContainer");
			static const FName PrefabSeqClassName = TEXT("PrefabSequence");
			static const FName SequenceClassName = TEXT("Sequence");
			static const FName PrefabSequenceName = TEXT("Prefabs");
			static const FName PrefabSeqContainerCDOName = TEXT("Default__PrefabSequenceContainer");
			static const FName PrefabSeqCDOName = TEXT("Default__PrefabSequence");

			// if we're here, we know that this package contains at least one prefab instance
			for ( INT ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++ )
			{
				FObjectImport& Import = ImportMap(ImportIndex);
				if ( Import.ClassName == NAME_Class )
				{
					if ( Import.ObjectName == PrefabSeqContainerClassName )
					{
						PrefabSeqContainerClassImportIndex = -ImportIndex-1;
					}
					else if ( Import.ObjectName == PrefabSeqClassName )
					{
						PrefabSeqClassImportIndex = -ImportIndex-1;
					}
					else if ( Import.ObjectName == SequenceClassName )
					{
						SequenceClassIndex = -ImportIndex-1;
					}
					else if ( Import.ObjectName == PrefabClassName )
					{
						PrefabClassIndex = -ImportIndex - 1;
					}
				}
				else if ( Import.ClassName == NAME_Package && Import.ObjectName == NAME_Engine )
				{
					EnginePackageIndex = -ImportIndex-1;
				}
			}


			// it's possible that none of the PrefabInstance or Prefabs actually have a sequence, in which case the sequence class might
			// not necessarily be in the import map
			if ( SequenceClassIndex != 0 )
			{
				warnf(TEXT("Performing sequence class fixup for %s"), *Filename);
				if ( PrefabSeqContainerClassImportIndex == 0 )
				{
					// add it
					check(EnginePackageIndex != 0);
					PrefabSeqContainerClassImportIndex = -ImportMap.Num()-1;
					FObjectImport* ContainerClassImport = new(ImportMap) FObjectImport(NULL);
					ContainerClassImport->ObjectName = PrefabSeqContainerClassName;
					ContainerClassImport->OuterIndex = EnginePackageIndex;
					ContainerClassImport->ClassName = NAME_Class;
					ContainerClassImport->ClassPackage = NAME_Core;
					Summary.ImportCount++;

					// then add the CDO for it
					FObjectImport* ContainerCDO = new(ImportMap) FObjectImport(NULL);
					ContainerCDO->ObjectName = PrefabSeqContainerCDOName;
					ContainerCDO->OuterIndex = EnginePackageIndex;
					ContainerCDO->ClassName = PrefabSeqContainerClassName;
					ContainerCDO->ClassPackage = NAME_Engine;
					Summary.ImportCount++;
				}

				if ( PrefabSeqClassImportIndex == 0 )
				{
					check(EnginePackageIndex!=0);
					check(PrefabSeqContainerClassImportIndex!=0);

					PrefabSeqClassImportIndex = -ImportMap.Num() - 1;
					FObjectImport* SeqClassImport = new(ImportMap) FObjectImport(NULL);
					SeqClassImport->ObjectName = PrefabSeqClassName;
					SeqClassImport->OuterIndex = EnginePackageIndex;
					SeqClassImport->ClassName = NAME_Class;
					SeqClassImport->ClassPackage = NAME_Core;
					Summary.ImportCount++;

					FObjectImport* PrefSeqCDO = new(ImportMap) FObjectImport(NULL);
					PrefSeqCDO->ObjectName = PrefabSeqCDOName;
					PrefSeqCDO->OuterIndex = EnginePackageIndex;
					PrefSeqCDO->ClassName = PrefabSeqClassName;
					PrefSeqCDO->ClassPackage = NAME_Engine;
					Summary.ImportCount++;
				}

				for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
				{
					FObjectExport& Export = ExportMap(ExportIndex);
					if ( Export.ClassIndex == SequenceClassIndex )
					{
						if ( Export.ObjectName == PrefabSequenceName )
						{
							warnf(TEXT("\tFound export for Prefabs sequence (%s).  Changing class to PrefabSequenceContainer (%i)."), 
								*GetExportPathName(ExportIndex), -PrefabSeqContainerClassImportIndex-1);

							Export.ClassIndex = PrefabSeqContainerClassImportIndex;
							LinkerRoot->MarkPackageDirty();
						}
						// else go up the outer chain if it has an outer
						else if (Export.OuterIndex != ROOTPACKAGE_INDEX)
						{
							FObjectExport& OuterExport = ExportMap(Export.OuterIndex - 1);
							if ( OuterExport.ObjectName == PrefabSequenceName || OuterExport.ClassIndex == PrefabClassIndex )
							{
								warnf(TEXT("\tFound export for %s sequence (%s).  Changing class to PrefabSequence (%i)."), 
									OuterExport.ClassIndex == PrefabClassIndex ? TEXT("Prefab") : TEXT("PrefabInstance"),
									*GetExportPathName(ExportIndex), -PrefabSeqClassImportIndex-1);

								Export.ClassIndex = PrefabSeqClassImportIndex;
								LinkerRoot->MarkPackageDirty();
							}
						}
					}
				}
			}
		}
	}

	return TRUE;
}

#if SUPPORTS_SCRIPTPATCH_LOADING
//@script patcher
/**
 * Integrates any new names, imports and exports from the shared script patcher object
 */
UBOOL ULinkerLoad::IntegrateScriptPatches()
{
	if ( !bHasIntegratedNamePatches || !bHasIntegratedImportPatches || !bHasIntegratedExportPatches
	|| !bHasIntegratedScriptPatches || !bHasIntegratedDefaultsPatches || !bHasIntegratedEnumPatches )
	{
		check(LinkerRoot);

		FLinkerPatchData* LinkerPatch = NULL;
		if ( GetScriptPatcher()->GetLinkerPatch(LinkerRoot->GetFName(), LinkerPatch) )
		{			
			checkSlow(LinkerPatch);

			// make sure we have a patch reader archive
			CreatePatchReader();

			if ( !bHasIntegratedNamePatches )
			{
				AppendNames(LinkerPatch->Names);
				bHasIntegratedNamePatches = TRUE;
			}

			if ( !bHasIntegratedImportPatches && !IsTimeLimitExceeded(TEXT("integrating script patch names"), 100) )
			{
				AppendImports(LinkerPatch->Imports);
				bHasIntegratedImportPatches = TRUE;
			}

			if ( !bHasIntegratedExportPatches && !IsTimeLimitExceeded(TEXT("integrating script patch imports"), 100) )
			{
				AppendExports(LinkerPatch->Exports, LinkerPatch->NewObjects);
				bHasIntegratedExportPatches = TRUE;
			}
			if ( !bHasIntegratedScriptPatches && !IsTimeLimitExceeded(TEXT("integrating script patch exports"), 100) )
			{
				for ( INT ScriptPatchIndex = 0; ScriptPatchIndex < LinkerPatch->ScriptPatches.Num(); ScriptPatchIndex++ )
				{
					FScriptPatchData& FunctionPatch = LinkerPatch->ScriptPatches(ScriptPatchIndex);
					for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
					{
						const FObjectExport& Export = ExportMap(ExportIndex);
						if ( Export.ObjectName == FunctionPatch.StructName )
						{
							// this might be the export which corresponds to the function that this FScriptPatchData must be applied to
							// so verify this by checking the export's full name
							FString ExportPathName = GetExportPathName(ExportIndex, NULL, TRUE);
							if ( ExportPathName == FunctionPatch.DataName )
							{
								FunctionsToPatch.Set(ExportIndex, &FunctionPatch);
								break;
							}
						}
					}
				}

				bHasIntegratedScriptPatches = TRUE;
			}
			if ( !bHasIntegratedDefaultsPatches && !IsTimeLimitExceeded(TEXT("integrating script bytecode patch"), 100) )
			{
				for ( INT DefaultsPatchIndex = 0; DefaultsPatchIndex < LinkerPatch->ModifiedClassDefaultObjects.Num(); DefaultsPatchIndex++ )
				{
					FPatchData& DefaultsPatch = LinkerPatch->ModifiedClassDefaultObjects(DefaultsPatchIndex);
					for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
					{
						const FObjectExport& Export = ExportMap(ExportIndex);
						if ( (Export.ObjectFlags&RF_ClassDefaultObject) != 0 && Export.ObjectName == *DefaultsPatch.DataName )
						{
							DefaultsToPatch.Set(ExportIndex, &DefaultsPatch);
							break;
						}
					}
				}
				bHasIntegratedDefaultsPatches = TRUE;
			}
			if ( !bHasIntegratedEnumPatches && !IsTimeLimitExceeded(TEXT("integrating defaults patches"), 100) )
			{
				for ( INT EnumPatchIndex = 0; EnumPatchIndex < LinkerPatch->ModifiedEnums.Num(); EnumPatchIndex++ )
				{
					FEnumPatchData& EnumPatch = LinkerPatch->ModifiedEnums(EnumPatchIndex);
					for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
					{
						const FObjectExport& Export = ExportMap(ExportIndex);
						if ( Export.ObjectName == EnumPatch.EnumName && EnumPatch.EnumPathName == GetExportPathName(ExportIndex) )
						{
							EnumsToPatch.Set(ExportIndex, &EnumPatch);
						}
					}
				}
				bHasIntegratedEnumPatches = TRUE;
			}
		}
		else
		{
			bHasIntegratedNamePatches = bHasIntegratedImportPatches = bHasIntegratedEnumPatches =
			bHasIntegratedExportPatches = bHasIntegratedScriptPatches = bHasIntegratedDefaultsPatches = TRUE;
		}

		if( bHasIntegratedNamePatches && bHasIntegratedImportPatches && bHasIntegratedExportPatches &&
			bHasIntegratedScriptPatches && bHasIntegratedDefaultsPatches && bHasIntegratedEnumPatches )
		{
			if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
			{
				GWarn->UpdateProgress( 4, ULinkerDefs::TotalProgressSteps );
			}
		}
	}

	// return TRUE if we're done with the script patcher
	return (bHasIntegratedNamePatches && bHasIntegratedImportPatches && bHasIntegratedExportPatches 
		&& bHasIntegratedScriptPatches && bHasIntegratedDefaultsPatches && bHasIntegratedEnumPatches)
		&& !IsTimeLimitExceeded(TEXT("integrating script patches"));
}

//@script patcher
/**
 * Changes all references to external packages in the ImportMap and ExportMap to contain the current script patcher
 * package suffix (e.g. _OriginalVer or _LatestVer).
 */
void ULinkerLoad::RemapLinkerPackageNames()
{
#if SUPPORTS_SCRIPTPATCH_CREATION
	// note - we don't worry about checking whether our time limit has exceeded because this function should only
	// be called while the patchscript commandlet is running, where asynchronous loading is disabled
	if ( !bHasRemappedExternalPackageReferences && GIsScriptPatcherActive && GScriptPatchPackageSuffix.Len() > 0 )
	{
		for ( INT ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++ )
		{
			FObjectImport& Import = ImportMap(ImportIndex);
			if ( Import.ClassPackage != NAME_Core )
			{
				// if this import's ClassPackage references one of the packages we've remapped, update that reference
				if ( Import.ClassPackage.ToString().InStr(*GScriptPatchPackageSuffix) == INDEX_NONE )
				{
					FString RemappedPackageName = Import.ClassPackage.ToString() + GScriptPatchPackageSuffix;
					Import.ClassPackage = FName(*RemappedPackageName);
				}
			}
			else if ( Import.ClassName == NAME_Package && Import.OuterIndex == ROOTPACKAGE_INDEX && Import.ObjectName != NAME_Core )
			{
				// if this import corresponds to one of the packages we've remapped, change its name to reflect the new package name
				if ( Import.ObjectName.ToString().InStr(*GScriptPatchPackageSuffix) == INDEX_NONE )
				{
					FString RemappedPackageName = Import.ObjectName.ToString() + GScriptPatchPackageSuffix;
					Import.ObjectName = FName(*RemappedPackageName);
				}
			}
		}

		for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
		{
			FObjectExport& Export = ExportMap(ExportIndex);

			// if a UPackage export has an OuterIndex of ROOTPACKAGE_INDEX_, it is either a group or a top-level package that has been inserted into
			// this linker as a forced export.  We don't rename groups, but we do need to rename forced export top-level packages
			if ( Export.OuterIndex == ROOTPACKAGE_INDEX && GetExportClassName(ExportIndex) == NAME_Package && Export.HasAnyFlags(EF_ForcedExport) )
			{
				if ( Export.ObjectName.ToString().InStr(*GScriptPatchPackageSuffix) == INDEX_NONE )
				{
					FString RemappedPackageName = Export.ObjectName.ToString() + GScriptPatchPackageSuffix;
					Export.ObjectName = FName(*RemappedPackageName);
				}
			}
		}

		bHasRemappedExternalPackageReferences = TRUE;
	}
#else
	bHasRemappedExternalPackageReferences = TRUE;
#endif
}
#endif

/**
 * Changes all references to external localized packages ExportMap to strip the language extension, i.e. _FRA
 *
 */
void ULinkerLoad::RemapLinkerPackageNamesForMultilanguageCooks()
{
	if (!bHasRemappedExternalPackageReferencesForMultilanguageCooks)
	{

		check(appStricmp( TEXT("INT"), UObject::GetLanguage() ) != 0);
		check(LoadFlags & LOAD_SeekFree);
		FString Search = TEXT("_");
		Search += UObject::GetLanguage();

		for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
		{
			FObjectExport& Export = ExportMap(ExportIndex);

			// if a UPackage export has an OuterIndex of ROOTPACKAGE_INDEX_, it is either a group or a top-level package that has been inserted into
			// this linker as a forced export.  We don't rename groups, but we do need to rename forced export top-level packages
			if ( Export.OuterIndex == ROOTPACKAGE_INDEX && GetExportClassName(ExportIndex) == NAME_Package && Export.HasAnyFlags(EF_ForcedExport) )
			{
				if ( Export.ObjectName.ToString().InStr(Search) != INDEX_NONE )
				{
					FString RemappedPackageName = Export.ObjectName.ToString().Replace(*Search,TEXT(""),TRUE);
					Export.ObjectName = FName(*RemappedPackageName);
				}
			}
		}

		bHasRemappedExternalPackageReferencesForMultilanguageCooks = TRUE;
	}
}


/**
 * Serializes the depends map.
 */
UBOOL ULinkerLoad::SerializeDependsMap()
{
	// Skip serializing depends map if we are using seekfree loading
	if( GUseSeekFreeLoading 
	// or we are neither Editor nor commandlet
	|| !(GIsEditor || GIsUCC) )
	{
		return TRUE;
	}

	// depends map size is same as export map size
	if (DependsMapIndex == 0 && Summary.ExportCount > 0)
	{
		Seek(Summary.DependsOffset);

		// Pre-size array to avoid re-allocation of array of arrays!
		DependsMap.AddZeroed(Summary.ExportCount);
	}

	while (DependsMapIndex < Summary.ExportCount && !IsTimeLimitExceeded(TEXT("serializing depends map"), 100))
	{
		TArray<INT>& Depends = DependsMap(DependsMapIndex);
		*this << Depends;
		DependsMapIndex++;
	}
	
	// Return whether we finished this step and it's safe to start with the next.
	return (DependsMapIndex == Summary.ExportCount) && !IsTimeLimitExceeded( TEXT("serializing depends map") );
}

/**
 * Serializes the Import/ExportGuids maps
 */
UBOOL ULinkerLoad::SerializeGuidMaps()
{
	// if we didn't have any Guids, then just be done
	if (Summary.ImportExportGuidsOffset == INDEX_NONE)
	{
		return TRUE;
	}

	Seek(Summary.ImportExportGuidsOffset);

	// load the ImportGuids map
	LinkerRoot->ImportGuids.AddZeroed(Summary.ImportGuidsCount);

	for (INT ImportGuidIndex = 0; ImportGuidIndex < Summary.ImportGuidsCount; ImportGuidIndex++)
	{
		// read in the level name as a string
		FString LevelName;
		*this << LevelName;
		LinkerRoot->ImportGuids(ImportGuidIndex).LevelName = FName(*LevelName);

		// read in the array of Guids
		*this << LinkerRoot->ImportGuids(ImportGuidIndex).Guids;
	}

	// load the ExportGuids map
	for (INT ExportGuidIndex = 0; ExportGuidIndex < Summary.ExportGuidsCount; ExportGuidIndex++)
	{
		FGuid ObjectGuid;
		INT ExportIndex;
		*this << ObjectGuid << ExportIndex;

		// store the export index as a UObject*, temporarily, until the exports have been loaded
		ExportGuidsAwaitingLookup.Set(ObjectGuid, ExportIndex);
	}

	return TRUE;
}

/**
 * Serializes thumbnails
 */
UBOOL ULinkerLoad::SerializeThumbnails( UBOOL bForceEnableInGame/*=FALSE*/ )
{
#if WITH_EDITORONLY_DATA
	// Skip serializing thumbnails if we are using seekfree loading
	if( !bForceEnableInGame && (GUseSeekFreeLoading || !GIsEditor) )
	{
		return TRUE;
	}

	if( Summary.ThumbnailTableOffset > 0 )
	{
		// Seek to the thumbnail table of contents
		Seek( Summary.ThumbnailTableOffset );


		// Load number of thumbnails
		INT ThumbnailCount = 0;
		*this << ThumbnailCount;


		// Allocate a new thumbnail map if we need one
		if( !LinkerRoot->ThumbnailMap.IsValid() )
		{
			LinkerRoot->ThumbnailMap.Reset( new FThumbnailMap() );
		}


		// Load thumbnail names and file offsets
		TArray< FObjectFullNameAndThumbnail > ThumbnailInfoArray;
		for( INT CurObjectIndex = 0; CurObjectIndex < ThumbnailCount; ++CurObjectIndex )
		{
			FObjectFullNameAndThumbnail ThumbnailInfo;

			FString ObjectClassName;
			if( Summary.GetFileVersion() >= VER_CONTENT_BROWSER_FULL_NAMES )
			{
				// Newer packages always store the class name for each asset
				*this << ObjectClassName;
			}
			else
			{
				// OK we're loading an older package which didn't store the class name for each
				// asset.  Only the relative path was stored for these guys.

				// We'll store the thumnbnail with a bogus class name ("???") that will be recognized
				// by our fast-thumbnail-loading code, and it can try to repair the class name then.
				// When the package is saved in the editor, these thumbnails will always be repaired.
				ObjectClassName = TEXT( "???" );
			}


			// Object path
			FString ObjectPathWithoutPackageName;
			*this << ObjectPathWithoutPackageName;
			const FString ObjectPath( LinkerRoot->GetName() + TEXT( "." ) + ObjectPathWithoutPackageName );


			// Create a full name string with the object's class and fully qualified path
			const FString ObjectFullName( ObjectClassName + TEXT( " " ) + ObjectPath );
			ThumbnailInfo.ObjectFullName = FName( *ObjectFullName );

			// File offset for the thumbnail (already saved out.)
			*this << ThumbnailInfo.FileOffset;

			// Only bother loading thumbnails that don't already exist in memory yet.  This is because when we
			// go to load thumbnails that aren't in memory yet when saving packages we don't want to clobber
			// thumbnails that were freshly-generated during that editor session
			if( !LinkerRoot->ThumbnailMap->HasKey( ThumbnailInfo.ObjectFullName ) )
			{
				// Add to list of thumbnails to load
				ThumbnailInfoArray.AddItem( ThumbnailInfo );
			}
		}



		// Now go and load and cache all of the thumbnails
		for( INT CurObjectIndex = 0; CurObjectIndex < ThumbnailInfoArray.Num(); ++CurObjectIndex )
		{
			const FObjectFullNameAndThumbnail& CurThumbnailInfo = ThumbnailInfoArray( CurObjectIndex );


			// Seek to the location in the file with the image data
			Seek( CurThumbnailInfo.FileOffset );

			// Load the image data
			FObjectThumbnail LoadedThumbnail;
			LoadedThumbnail.Serialize( *this );

			// If the thumbnail was created before we had support for rectangular thumbs, then we'll immediately
			// mark it as dirty so that it will be refreshed on save.
			if( Summary.GetFileVersion() < VER_RECTANGULAR_THUMBNAILS_IN_PACKAGES )
			{
				// The thumbnail was created before we had proper support for rectangular thumbs.  We'll dirty it,
				// which will cause it to be regenerated on demand when possible.  For currently-unloaded assets,
				// the user may see an oddly-shaped thumbnail in the editor until the object becomes loaded.  An
				// editor package resave will fix everything up!
				LoadedThumbnail.MarkAsDirty();
			}

			// Store the data!
			LinkerRoot->ThumbnailMap->Set( CurThumbnailInfo.ObjectFullName, LoadedThumbnail );
		}
	}
#endif // WITH_EDITORONLY_DATA

	// Finished!
	return TRUE;
}



/** 
 * Creates the export hash. This relies on the import and export maps having already been serialized.
 */
UBOOL ULinkerLoad::CreateExportHash()
{
	// Zero initialize hash on first iteration.
	if( ExportHashIndex == 0 )
	{
		for( INT i=0; i<ARRAY_COUNT(ExportHash); i++ )
		{
			ExportHash[i] = INDEX_NONE;
		}
	}

	// Set up export hash, potentially spread across several frames.
	while( ExportHashIndex < ExportMap.Num() && !IsTimeLimitExceeded(TEXT("creating export hash"),100) )
	{
		FObjectExport& Export = ExportMap(ExportHashIndex);

		const INT iHash = HashNames( Export.ObjectName, GetExportClassName(ExportHashIndex), GetExportClassPackage(ExportHashIndex) ) & (ARRAY_COUNT(ExportHash)-1);
		Export._iHashNext = ExportHash[iHash];
		ExportHash[iHash] = ExportHashIndex;

		ExportHashIndex++;
	}

	// Return whether we finished this step and it's safe to start with the next.
	return (ExportHashIndex == ExportMap.Num()) && !IsTimeLimitExceeded( TEXT("creating export hash") );
}

/**
 * Finds existing exports in memory and matches them up with this linker. This is required for PIE to work correctly
 * and also for script compilation as saving a package will reset its linker and loading will reload/ replace existing
 * objects without a linker.
 */
UBOOL ULinkerLoad::FindExistingExports()
{
	if( bHasFoundExistingExports == FALSE )
	{
#if !CONSOLE
		// only look for existing exports in the editor after it has started up
		if( GIsEditor && GIsRunning )
		{
			// Hunt down any existing objects and hook them up to this linker unless the user is either currently opening this
			// package manually via the generic browser or the package is a map package. We want to overwrite (aka load on top)
			// the objects in those cases, so don't try to find existing exports.
			//
			// @todo: document why we also do it for GIsUCCMake
			UBOOL bContainsMap			= LinkerRoot ? LinkerRoot->ContainsMap() : FALSE;
			UBOOL bRequestFindExisting	= GCallbackQuery->Query(CALLBACK_LoadObjectsOnTop, Filename) == FALSE;
			if( (!GIsUCC && bRequestFindExisting && !bContainsMap) || GIsUCCMake )
			{
				for (INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++)
				{
					FindExistingExport(ExportIndex);
				}
			}
		}
#endif
		// Avoid duplicate work in the case of async linker creation.
		bHasFoundExistingExports = TRUE;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 5, ULinkerDefs::TotalProgressSteps );
		}
	}
	return !IsTimeLimitExceeded( TEXT("finding existing exports") );
}

/**
 * Finalizes linker creation, adding linker to loaders array and potentially verifying imports.
 */
UBOOL ULinkerLoad::FinalizeCreation()
{
	if( bHasFinishedInitialization == FALSE )
	{
		// Add this linker to the object manager's linker array.
		GObjLoaders.AddItem( this );

		// tell the root package to set up netplay info
		if (LinkerRoot != NULL)
		{
			LinkerRoot->InitNetInfo(this, INDEX_NONE);

			//@{
			//@script patcher
			// now fixup the object counts used for networking - since the patched exports may belong to packages which are forced exports,
			// we must follow each export's Outer chain to find the correct place to increment the net object count

			// this represents the exports that will be added to this linker's net object count
			INT GlobalPatchedExportCount=0;

			// find the position of the first patched export
			INT FirstPatchExportIndex = FindFirstPatchedExportIndex();
			if ( FirstPatchExportIndex != INDEX_NONE )
			{
				for ( INT ExportIndex = FirstPatchExportIndex; ExportIndex < ExportMap.Num(); ExportIndex++ )
				{
					FObjectExport& Export = ExportMap(ExportIndex);

					INT PackageExportIndex = ROOTPACKAGE_INDEX;
					for ( INT OuterIndex = Export.OuterIndex; OuterIndex != ROOTPACKAGE_INDEX; OuterIndex = ExportMap(OuterIndex-1).OuterIndex )
					{
						PackageExportIndex = OuterIndex - 1;
					}

					if ( PackageExportIndex == ROOTPACKAGE_INDEX )
					{
						// this export's outermost is the linker root
						GlobalPatchedExportCount++;
					}
					else
					{
						// this export was a forced export, so increment the net object count for the export containing it's outermost package instead
						FObjectExport& OuterExport = ExportMap(PackageExportIndex);
						if ( !OuterExport.HasAnyFlags(EF_ForcedExport) )
						{
							GlobalPatchedExportCount++;
						}
						else if ( OuterExport.GenerationNetObjectCount.Num() > 0 )
						{
							INT& CurrentGenerationNetObjectCount = OuterExport.GenerationNetObjectCount.Last();
							CurrentGenerationNetObjectCount++;
						}
					}
				}

				if ( GlobalPatchedExportCount > 0 )
				{
					const TArray<INT> NetObjectCounts = LinkerRoot->GetGenerationNetObjectCount();
					if ( NetObjectCounts.Num() > 0 )
					{
						LinkerRoot->PatchNetObjectList(NetObjectCounts.Last() + GlobalPatchedExportCount);
					}
				}
				//@}
			}
		}

		// check if the package source matches the package filename's CRC (if it doens't match, a user saved this package)
		if (Summary.PackageSource != appStrCrcCaps(*(FFilename(Filename).GetBaseFilename())))
		{
//			debugf(TEXT("Found a user created pacakge (%s)"), *(FFilename(Filename).GetBaseFilename()));
			appSetUserCreatedContentLoaded();
		}
		else
		{
			//used by the editor when a user tries to export an object that has had its source data stripped out
			LinkerRoot->PackageFlags |= PKG_NoExportAllowed;
		}

		if (!(LoadFlags & LOAD_NoVerify))
		{
			Verify();
		}

		// This means that _Linker references are not NULL'd when using FArchiveReplaceObjectRef
		SetFlags(RF_Public);

		// Avoid duplicate work in the case of async linker creation.
		bHasFinishedInitialization = TRUE;

		if ((LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0)
		{
			GWarn->UpdateProgress( 6, ULinkerDefs::TotalProgressSteps );
		}
	}
	return !IsTimeLimitExceeded( TEXT("finalizing creation") );
}

/*-----------------------------------------------------------------------------
	ULinkerLoad creation END
-----------------------------------------------------------------------------*/

/**
 * Before loading anything objects off disk, this function can be used to discover
 * the object in memory. This could happen in the editor when you save a package (which
 * destroys the linker) and then play PIE, which would cause the Linker to be
 * recreated. However, the objects are still in memory, so there is no need to reload
 * them.
 *
 * @param ExportIndex	The index of the export to hunt down
 * @return The object that was found, or NULL if it wasn't found
 */
UObject* ULinkerLoad::FindExistingExport(INT ExportIndex)
{
	check(ExportMap.IsValidIndex(ExportIndex));
	FObjectExport& Export = ExportMap(ExportIndex);

	// if we were already found, leave early
	if (Export._Object)
	{
		return Export._Object;
	}

	// find the outer package for this object, if it's already loaded
	UObject* OuterObject = NULL;
	if (Export.OuterIndex == 0)
	{
		// this export's outer is the UPackage root of this loader
		OuterObject = LinkerRoot;
	}
	else
	{
		// if we have a PackageIndex, then we are in a group or other obhect, and we should look for it
		OuterObject = FindExistingExport(Export.OuterIndex-1);
	}

	// if we found one, keep going. if we didn't find one, then this package has never been loaded before
	// things inside a class however should not be touched, as they are in .u files and shouldn't have SetLinker called on them
	if (OuterObject && !Outer->IsInA(UClass::StaticClass()))
	{
		// find the class of this object
		UClass* TheClass;
		if (Export.ClassIndex == 0)
		{
			TheClass = UClass::StaticClass();
		}
		else if (IS_IMPORT_INDEX(Export.ClassIndex))
		{
			TheClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ImportMap(-Export.ClassIndex - 1).ObjectName.ToString(), 1);
		}
		else
		{
			TheClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ExportMap(Export.ClassIndex - 1).ObjectName.ToString(), 1);
		}

		// if the class exists, try to find the object
		if (TheClass)
		{
			Export._Object = StaticFindObject(TheClass, OuterObject, *Export.ObjectName.ToString(), 1);

			// if we found an object, set it's linker to us
			if (Export._Object)
			{
                Export._Object->SetLinker(this, ExportIndex);
			}
		}
	}

	return Export._Object;
}

void ULinkerLoad::Verify()
{
	// look on the commandline for the CookPackages tag, because GIsCooking may be set too late
	// if we are cooking, we don't want to Verify, because it can potentially load too many
	// objects at the wrong time (especially combined with Redirectors which have issues being
	// garbage collected, which keeps objects improperly loaded across GC calls)
	static INT CookerFlag = -1;
#if !CONSOLE
	if (CookerFlag == -1)
	{
		CookerFlag = (appStristr(appCmdLine(), TEXT("CookPackages")) != NULL) ? 1 : 0;
	}
#endif

	if( !(LinkerRoot->PackageFlags & PKG_Cooked)  && (!GIsGame || GIsEditor || GIsUCC) && CookerFlag != 1)
	{
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
	    if( !bHaveImportsBeenVerified && ((Summary.PackageFlags & PKG_RequireImportsAlreadyLoaded) == 0 || GIsScriptPatcherActive) )
#else
		if( !bHaveImportsBeenVerified && (Summary.PackageFlags & PKG_RequireImportsAlreadyLoaded) == 0 )
#endif
		{
#if !EXCEPTIONS_DISABLED
			try
#endif
		{
			// Validate all imports and map them to their remote linkers.
			for( INT i=0; i<Summary.ImportCount; i++ )
			{
				VerifyImport( i );
			}
		}
#if !EXCEPTIONS_DISABLED
		catch( TCHAR* Error )
		{
				GObjLoaders.RemoveItem( this );
				throw( Error );
			}
#endif
		}
	}
	bHaveImportsBeenVerified = TRUE;
}

FName ULinkerLoad::GetExportClassPackage( INT i )
{
	FObjectExport& Export = ExportMap( i );
	if( IS_IMPORT_INDEX(Export.ClassIndex) )
	{
		check( ImportMap.IsValidIndex(-Export.ClassIndex-1) );
		FObjectImport& Import = ImportMap( -Export.ClassIndex-1 );
		if (IS_IMPORT_INDEX(Import.OuterIndex))
		{
			return ImportMap( -Import.OuterIndex-1 ).ObjectName;
		}
		else
		{
			return ExportMap(Import.OuterIndex-1).ObjectName;
		}
	}
	else if ( Export.ClassIndex != UCLASS_INDEX )
	{
		// the export's class is contained within the same package
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
	if( IS_IMPORT_INDEX(Export.ClassIndex) )
	{
		check( ImportMap.IsValidIndex(-Export.ClassIndex-1) );
		return ImportMap( -Export.ClassIndex-1 ).ObjectName;
	}
	else if( Export.ClassIndex != UCLASS_INDEX )
	{
		check( ExportMap.IsValidIndex(Export.ClassIndex-1) );
		return ExportMap( Export.ClassIndex-1 ).ObjectName;
	}
	else
	{
		return NAME_Class;
	}
}


FString ULinkerLoad::GetArchiveName() const
{
	return *Filename;
}


/**
 * Recursively gathers the dependencies of a given export (the recursive chain of imports
 * and their imports, and so on)

 * @param ExportIndex Index into the linker's ExportMap that we are checking dependencies
 * @param Dependencies Array of all dependencies needed
 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
 */
void ULinkerLoad::GatherExportDependencies(INT ExportIndex, TSet<FDependencyRef>& Dependencies, UBOOL bSkipLoadedObjects)
{
	// make sure we have dependencies
	// @todo: remove this check after all packages have been saved up to VER_ADDED_LINKER_DEPENDENCIES
	if (DependsMap.Num() == 0)
	{
		return;
	}

	// validate data
	check(DependsMap.Num() == ExportMap.Num());

	// get the list of imports the export needs
	TArray<INT>& ExportDependencies = DependsMap(ExportIndex);

//warnf(TEXT("Gathering dependencies for %s"), *GetExportFullName(ExportIndex));

	for (INT DependIndex = 0; DependIndex < ExportDependencies.Num(); DependIndex++)
	{
		INT ObjectIndex = ExportDependencies(DependIndex);

		// if it's an import, use the import version to recurse (which will add the export the import points to to the array)
		if (IS_IMPORT_INDEX(ObjectIndex))
		{
			GatherImportDependencies(-ObjectIndex - 1, Dependencies, bSkipLoadedObjects);
		}
		else
		{
			INT RefExportIndex = ObjectIndex - 1;
			FObjectExport& Export = ExportMap(RefExportIndex);

			if( (Export._Object) && ( bSkipLoadedObjects == TRUE ) )
			{
				continue;
			}

			// fill out the ref
			FDependencyRef NewRef;
			NewRef.Linker = this;
			NewRef.ExportIndex = RefExportIndex;

			// Add to set and recurse if not already present.
			UBOOL bIsAlreadyInSet = FALSE;
			Dependencies.Add( NewRef, &bIsAlreadyInSet );
			if (!bIsAlreadyInSet)
			{
				NewRef.Linker->GatherExportDependencies(RefExportIndex, Dependencies, bSkipLoadedObjects);
			}
		}
	}
}

/**
 * Recursively gathers the dependencies of a given import (the recursive chain of imports
 * and their imports, and so on). Will add itself to the list of dependencies

 * @param ImportIndex Index into the linker's ImportMap that we are checking dependencies
 * @param Dependencies Set of all dependencies needed
 * @param bSkipLoadedObjects Whether to skip already loaded objects when gathering dependencies
 */
void ULinkerLoad::GatherImportDependencies(INT ImportIndex, TSet<FDependencyRef>& Dependencies, UBOOL bSkipLoadedObjects)
{
	// get the import
	FObjectImport& Import = ImportMap(ImportIndex);

	// we don't need the top level package imports to be checked, since there is no real object associated with them
	if (Import.OuterIndex == ROOTPACKAGE_INDEX)
	{
		return;
	}
	//	warnf(TEXT("  Dependency import %s [%x, %d]"), *GetImportFullName(ImportIndex), Import.SourceLinker, Import.SourceIndex);

	// if the object already exists, we don't need this import
	if (Import.XObject)
	{
		return;
	}

	UObject::BeginLoad();

	// load the linker and find export in sourcelinker
	if (Import.SourceLinker == NULL || Import.SourceIndex == INDEX_NONE)
	{
#if DO_CHECK
		INT NumObjectsBefore = UObject::GetObjectArrayNum();
#endif

		// temp storage we can ignore
		FString Unused;

		// remember that we are gathering imports so that VerifyImportInner will no verify all imports
		bIsGatheringDependencies = TRUE;

		// if we failed to find the object, ignore this import
		// @todo: Tag the import to not be searched again
		VerifyImportInner(ImportIndex, Unused);

		// turn off the flag
		bIsGatheringDependencies = FALSE;

		UBOOL bIsValidImport =
			(Import.XObject != NULL && !Import.XObject->HasAnyFlags(RF_Native) && (!Import.XObject->HasAnyFlags(RF_ClassDefaultObject) || !Import.XObject->GetClass()->HasAllFlags(RF_Public|RF_Native|RF_Transient))) ||
			(Import.SourceLinker != NULL && Import.SourceIndex != INDEX_NONE);

		// make sure it succeeded
		if (!bIsValidImport)
		{
			// don't print out for intrinsic native classes
			if (!Import.XObject || !(Import.XObject->GetClass()->ClassFlags & CLASS_Intrinsic))
			{
				if (GIsCooking)
				{
					static INT Limit = 0;
					Limit++;
					if (Limit == 50)
					{
						// this message often occurs with missing audio localization
						warnf(NAME_Warning, TEXT("***************************************************************"));
						warnf(NAME_Warning, TEXT("****VerifyImportInner too many messages. Will not print more.**"));
						warnf(NAME_Warning, TEXT("***************************************************************"));
					}
					else if (Limit < 50)
					{

						warnf(NAME_Warning, TEXT("VerifyImportInner failed [(%x, %d), (%x, %d)] for %s with linker: %s %s - Was it deleted?"), 
							Import.XObject, Import.XObject ? (Import.XObject->HasAnyFlags(RF_Native) ? 1 : 0) : 0, 
							Import.SourceLinker, Import.SourceIndex, 
							*GetImportFullName(ImportIndex), *this->GetFullName(), *this->Filename );
					}
				}
				else
				{
					warnf(NAME_Warning, TEXT("VerifyImportInner failed [(%x, %d), (%x, %d)] for %s with linker: %s %s"), 
						Import.XObject, Import.XObject ? (Import.XObject->HasAnyFlags(RF_Native) ? 1 : 0) : 0, 
						Import.SourceLinker, Import.SourceIndex, 
						*GetImportFullName(ImportIndex), *this->GetFullName(), *this->Filename );
				}
			}
			UObject::EndLoad();
			return;
		}

#if DO_CHECK && !NO_LOGGING
		// only object we should create are one ULinkerLoad for source linker
		if (UObject::GetObjectArrayNum() - NumObjectsBefore > 2)
		{
			warnf(TEXT("Created %d objects checking %s"), UObject::GetObjectArrayNum() - NumObjectsBefore, *GetImportFullName(ImportIndex));
		}
#endif
	}

	// save off information BEFORE calling EndLoad so that the Linkers are still associated
	FDependencyRef NewRef;
	if (Import.XObject)
	{
		warnf(TEXT("Using non-native XObject %s!!!"), *Import.XObject->GetFullName());
		NewRef.Linker = Import.XObject->_Linker;
		NewRef.ExportIndex = Import.XObject->_LinkerIndex;
	}
	else
	{
		NewRef.Linker = Import.SourceLinker;
		NewRef.ExportIndex = Import.SourceIndex;
	}

	UObject::EndLoad();

	// Add to set and recurse if not already present.
	UBOOL bIsAlreadyInSet = FALSE;
	Dependencies.Add( NewRef, &bIsAlreadyInSet );
	if (!bIsAlreadyInSet)
	{
		NewRef.Linker->GatherExportDependencies(NewRef.ExportIndex, Dependencies, bSkipLoadedObjects);
	}
}




/**
 * A wrapper around VerifyImportInner. If the VerifyImportInner (previously VerifyImport) fails, this function
 * will look for a UObjectRedirector that will point to the real location of the object. You will see this if
 * an object was renamed to a different package or group, but something that was referencing the object was not
 * not currently open. (Rename fixes up references of all loaded objects, but naturally not for ones that aren't
 * loaded).
 *
 * @param	i	The index into this packages ImportMap to verify
 */
void ULinkerLoad::VerifyImport( INT i )
{
#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	check( (Summary.PackageFlags & PKG_RequireImportsAlreadyLoaded) == 0 || GIsScriptPatcherActive);
#else
	check( (Summary.PackageFlags & PKG_RequireImportsAlreadyLoaded) == 0);
#endif

	FObjectImport& Import = ImportMap(i);

	// keep a string of modifiers to add to the Editor Warning dialog
	FString WarningAppend;

	// try to load the object, but don't print any warnings on error (so we can try the redirector first)
	// note that a true return value here does not mean it failed or succeeded, just tells it how to respond to a further failure
	UBOOL bCrashOnFail = VerifyImportInner(i,WarningAppend);

	// by default, we haven't failed yet
	UBOOL bFailed = false;
	UBOOL bRedir = false;

	// these checks find out if the VerifyImportInner was successful or not 
	if (Import.SourceLinker && Import.SourceIndex == INDEX_NONE && Import.XObject == NULL && Import.OuterIndex != 0 && Import.ObjectName != NAME_ObjectRedirector)
	{
		// if we found the package, but not the object, look for a redirector
		FObjectImport OriginalImport = Import;
		Import.ClassName = NAME_ObjectRedirector;
		Import.ClassPackage = NAME_Core;

		// try again for the redirector
		VerifyImportInner(i,WarningAppend);

		// if the redirector wasn't found, then it truly doesn't exist
		if (Import.SourceIndex == INDEX_NONE)
		{
			bFailed = true;
		}
		// otherwise, we found that the redirector exists
		else
		{
			// this notes that for any load errors we get that a ObjectRedirector was involved (which may help alleviate confusion
			// when people don't understand why it was trying to load an object that was redirected from or to)
			WarningAppend += LocalizeError(TEXT("LoadWarningSuffix_redirection"),TEXT("UnrealEd"));

			// Create the redirector (no serialization yet)
			UObjectRedirector* Redir = Cast<UObjectRedirector>(Import.SourceLinker->CreateExport(Import.SourceIndex));
			// this should probably never fail, but just in case
			if (!Redir)
			{
				bFailed = true;
			}
			else
			{
				// serialize in the properties of the redirector (to get the object the redirector point to)
				Preload(Redir);

				UObject* DestObject = Redir->DestinationObject;

				// check to make sure the destination obj was loaded,
				if ( DestObject == NULL )
				{
					bFailed = true;
				}
				// check that in fact it was the type we thought it should be
				else if ( DestObject->GetClass()->GetFName() != OriginalImport.ClassName

					// if the destination object is a CDO, allow class changes
					&&	!DestObject->HasAnyFlags(RF_ClassDefaultObject) )
				{
					bFailed = true;
					// if the destination is a ObjectRedirector you've most likely made a nasty circular loop
					if( Redir->DestinationObject->GetClass() == UObjectRedirector::StaticClass() )
					{
						WarningAppend += LocalizeError(TEXT("LoadWarningSuffix_circularredirection"),TEXT("UnrealEd"));
					}
				}
				else
				{
					// send a callback saying we followed a redirector successfully
					GCallbackEvent->Send(CALLBACK_RedirectorFollowed, Filename, Redir);

					// now, fake our Import to be what the redirector pointed to
					Import.XObject = Redir->DestinationObject;
					GImportCount++;
				}
			}
		}

		// fix up the import. We put the original data back for the ClassName and ClassPackage (which are read off disk, and
		// are expected not to change)
		Import.ClassName = OriginalImport.ClassName;
		Import.ClassPackage = OriginalImport.ClassPackage;

		// if nothing above failed, then we are good to go
		if (!bFailed)
		{
			// we update the runtime information (SourceIndex, SourceLinker) to point to the object the redirector pointed to
			Import.SourceIndex = Import.XObject->_LinkerIndex;
			Import.SourceLinker = Import.XObject->_Linker;
		}
		else
		{
			// put us back the way we were and peace out
			Import = OriginalImport;
			// if the original VerifyImportInner told us that we need to throw an exception if we weren't redirected,
			// then do the throw here
			if (bCrashOnFail)
			{
				debugf( TEXT("Failed import: %s %s (file %s)"), *Import.ClassName.ToString(), *GetImportFullName(i), *Import.SourceLinker->Filename );
				appThrowf( LocalizeSecure(LocalizeError(TEXT("FailedImport"),TEXT("Core")), *Import.ClassName.ToString(), *GetImportFullName(i)) );
			}
			// otherwise just printout warnings, and if in the editor, popup the EdLoadWarnings box
			else
			{
				// try to get a pointer to the class of the original object so that we can display the class name of the missing resource
				UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage.ToString() );
				UClass* FindClass = ClassPackage ? FindObject<UClass>( ClassPackage, *OriginalImport.ClassName.ToString() ) : NULL;
				if( GIsEditor && !GIsUCC )
				{
					// put somethng into the load warnings dialog, with any extra information from above (in WarningAppend)
					EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *FString::Printf(TEXT("%s%s in %s"), *GetImportFullName(i), *WarningAppend, *LinkerRoot->GetName()) );
				}

#if _DEBUG
				if( !IgnoreMissingReferencedClass( Import.ObjectName ) )
				{
					// failure to load a class, most likely deleted instead of deprecated
					if ( (!GIsEditor || GIsUCC) && FindClass == UClass::StaticClass() )
					{
						warnf(NAME_Warning, TEXT("Missing Class '%s' referenced by package '%s' ('%s').  Classes should not be removed if referenced by content; mark the class 'deprecated' instead."),
							*GetImportFullName(i),
							*LinkerRoot->GetName(),
							GSerializedExportLinker ? *GSerializedExportLinker->GetExportPathName(GSerializedExportIndex) : TEXT("Unknown") );
					}
					// ignore warnings for missing imports if the object's class has been deprecated.
					else if ( FindClass == NULL || !FindClass->HasAnyClassFlags(CLASS_Deprecated) )
					{
						warnf(NAME_Warning, TEXT("Missing Class '%s' referenced by package '%s' ('%s')."),
							*GetImportFullName(i),
							*LinkerRoot->GetName(),
							GSerializedExportLinker ? *GSerializedExportLinker->GetExportPathName(GSerializedExportIndex) : TEXT("Unknown") );
					}
				}
#endif
			}
		}
	}
}

/**
 * Safely verify that an import in the ImportMap points to a good object. This decides whether or not
 * a failure to load the object redirector in the wrapper is a fatal error or not (return value)
 *
 * @param	i	The index into this packages ImportMap to verify
 *
 * @return TRUE if the wrapper should crash if it can't find a good object redirector to load
 */
UBOOL ULinkerLoad::VerifyImportInner(INT ImportIndex, FString& WarningSuffix)
{
	//@script patcher fixme: triggered when running script patcher
	check(GObjBeginLoadCount>0);

	FObjectImport& Import = ImportMap(ImportIndex);

	if
	(	(Import.SourceLinker && Import.SourceIndex != INDEX_NONE)
	||	Import.ClassPackage	== NAME_None
	||	Import.ClassName	== NAME_None
	||	Import.ObjectName	== NAME_None )
	{
		// Already verified, or not relevent in this context.
		return FALSE;
	}


	UBOOL SafeReplace = FALSE;
	UObject* Pkg=NULL;

	// Find or load the linker load that contains the FObjectExport for this import
	if( Import.OuterIndex == ROOTPACKAGE_INDEX )
	{
		// our Outer is a UPackage
		check(Import.ClassName==NAME_Package);
		check(Import.ClassPackage==NAME_Core);

		//@script patcher (LOAD_RemappedPackage)
		UPackage* TmpPkg = CreatePackage( NULL, *Import.ObjectName.ToString(), (LoadFlags&LOAD_RemappedPackage) != 0 );

#if !EXCEPTIONS_DISABLED
		try
#endif
		{
			// if we're currently compiling this package, don't allow
			// it to be linked to any other package's ImportMaps
			if ( (TmpPkg->PackageFlags&PKG_Compiling) != 0 )
			{
				return FALSE;
			}

			DWORD InternalLoadFlags = LOAD_Throw|(LoadFlags&(LOAD_RemappedPackage|LOAD_NoVerify|LOAD_Verify|LOAD_NoWarn|LOAD_Quiet));
			if ( GIsUCCMake )
			{
				InternalLoadFlags |= LOAD_FindIfFail;
			}

			// while gathering dependencies, there is no need to verify all of the imports for the entire package
			if (bIsGatheringDependencies)
			{
				InternalLoadFlags |= LOAD_NoVerify;
			}

			Import.SourceLinker = GetPackageLinker( TmpPkg, NULL, InternalLoadFlags, NULL, NULL );
		}
#if !EXCEPTIONS_DISABLED
		catch( const TCHAR* Error )
		{
			if( LoadFlags & LOAD_FindIfFail )
			{
				// clear the Import's SourceLinker so that VerifyImport doesn't attempt to
				// do anything else with this FObjectImport (such as search for a redirect
				// for this missing resource, etc.)
				Import.SourceLinker = NULL;
			}
			else
			{
				appThrowf( Error );
			}
		}
#endif
	}
	else
	{
		if (LinkerRoot->PackageFlags & PKG_Cooked)
		{
			// outer can be an export if the package is cooked
			if (!IS_IMPORT_INDEX(Import.OuterIndex))
			{
				// @todo: This is possibly not the right thing to do here!
				// it could attempt to find the original PackageLinker from disk and use that
				return FALSE;
			}
		}
		else
		{
			// this resource's Outer is not a UPackage
			checkf(IS_IMPORT_INDEX(Import.OuterIndex),TEXT("Outer for Import %s (%i) is not an import - OuterIndex:%i"), *GetImportFullName(ImportIndex), ImportIndex, Import.OuterIndex);
		}

		VerifyImport( -Import.OuterIndex-1 );

		// Copy the SourceLinker from the FObjectImport for our Outer
		Import.SourceLinker = ImportMap(-Import.OuterIndex-1).SourceLinker;

		//check(Import.SourceLinker);
		//@todo what does it mean if we don't have a SourceLinker here?
		if( Import.SourceLinker )
		{
			FObjectImport* Top;
			for (Top = &Import;	IS_IMPORT_INDEX(Top->OuterIndex); Top = &ImportMap(-Top->OuterIndex-1))
			{
				// for loop does what we need
			}

			// Top is now pointing to the top-level UPackage for this resource
			//@script patcher (LOAD_RemappedPackage)
			Pkg = CreatePackage(NULL, *Top->ObjectName.ToString(), (LoadFlags&LOAD_RemappedPackage) != 0 );

			// Find this import within its existing linker.
			INT iHash = HashNames( Import.ObjectName, Import.ClassName, Import.ClassPackage) & (ARRAY_COUNT(ExportHash)-1);

			for( INT j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap(j)._iHashNext )
			{
				FObjectExport& SourceExport = Import.SourceLinker->ExportMap( j );
				if
				(	(SourceExport.ObjectName	                  ==Import.ObjectName               )
				&&	(Import.SourceLinker->GetExportClassName   (j)==Import.ClassName                )
				&&  (Import.SourceLinker->GetExportClassPackage(j)==Import.ClassPackage) )
				{
					// at this point, SourceExport is an FObjectExport in another linker that looks like it
					// matches the FObjectImport we're trying to load - double check that we have the correct one
					if( IS_IMPORT_INDEX(Import.OuterIndex) )
					{
						// OuterImport is the FObjectImport for this resource's Outer
						FObjectImport& OuterImport = ImportMap(-Import.OuterIndex-1);
						if( OuterImport.SourceLinker )
						{
							// if the import for our Outer doesn't have a SourceIndex, it means that
							// we haven't found a matching export for our Outer yet.  This should only
							// be the case if our Outer is a top-level UPackage
							if( OuterImport.SourceIndex==INDEX_NONE )
							{
								// At this point, we know our Outer is a top-level UPackage, so
								// if the FObjectExport that we found has an Outer that is
								// not a linker root, this isn't the correct resource
								if( SourceExport.OuterIndex != ROOTPACKAGE_INDEX )
								{
									continue;
								}
							}

							// The import for our Outer has a matching export - make sure that the import for
							// our Outer is pointing to the same export as the SourceExport's Outer
							else if( OuterImport.SourceIndex + 1 != SourceExport.OuterIndex )
							{
								continue;
							}
						}
					}
					if( !(SourceExport.ObjectFlags & RF_Public) )
					{
						SafeReplace = SafeReplace || (GIsEditor && !GIsUCC);

						// determine if this find the thing that caused this import to be saved into the map
						PACKAGE_INDEX FoundIndex = -(ImportIndex + 1);
						for ( INT i = 0; i < Summary.ExportCount; i++ )
						{
							FObjectExport& Export = ExportMap(i);
							if ( Export.SuperIndex == FoundIndex )
							{
								debugf(TEXT("Private import was referenced by export '%s' (parent)"), *Export.ObjectName.ToString());
								SafeReplace = FALSE;
							}
							else if ( Export.ClassIndex == FoundIndex )
							{
								debugf(TEXT("Private import was referenced by export '%s' (class)"), *Export.ObjectName.ToString());
								SafeReplace = FALSE;
							}
							else if ( Export.OuterIndex == FoundIndex )
							{
								debugf(TEXT("Private import was referenced by export '%s' (outer)"), *Export.ObjectName.ToString());
								SafeReplace = FALSE;
							}
							else if ( Export.ArchetypeIndex == FoundIndex )
							{
								debugf(TEXT("Private import was referenced by export '%s' (template)"), *Export.ObjectName.ToString());
								SafeReplace = FALSE;
							}
						}
						for ( INT i = 0; i < Summary.ImportCount; i++ )
						{
							if ( i != FoundIndex )
							{
								FObjectImport& TestImport = ImportMap(i);
								if ( TestImport.OuterIndex == FoundIndex )
								{
									debugf(TEXT("Private import was referenced by import '%s' (outer)"), *Import.ObjectName.ToString());
									SafeReplace = FALSE;
								}
							}
						}

						if ( !SafeReplace )
						{
							appThrowf( LocalizeSecure(LocalizeError(TEXT("FailedImportPrivate"),TEXT("Core")), *Import.ClassName.ToString(), *GetImportFullName(ImportIndex)) );
						}
						else
						{
							FString Suffix = LocalizeError(TEXT("LoadWarningSuffix_privateobject"),TEXT("UnrealEd"));
							if ( WarningSuffix.InStr(Suffix) == INDEX_NONE )
								WarningSuffix += Suffix;
							break;
						}
					}

					// Found the FObjectExport for this import
					Import.SourceIndex = j;
					break;
				}
			}
		}
	}

	if( (Pkg == NULL) && ((LoadFlags & LOAD_FindIfFail) != 0) )
	{
		Pkg = ANY_PACKAGE;
	}

	// If not found in file, see if it's a public native transient class or field.
	if( Import.SourceIndex==INDEX_NONE && Pkg!=NULL )
	{
		UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage.ToString() );
		if( ClassPackage )
		{
			UClass* FindClass = FindObject<UClass>( ClassPackage, *Import.ClassName.ToString() );
			if( FindClass )
			{
				UObject* FindOuter			= Pkg;

				if ( IS_IMPORT_INDEX(Import.OuterIndex) )
				{
					// if this import corresponds to an intrinsic class, OuterImport's XObject will be NULL if this import
					// belongs to the same package that the import's class is in; in this case, the package is the correct Outer to use
					// for finding this object
					// otherwise, this import represents a field of an intrinsic class, and OuterImport's XObject should be non-NULL (the object
					// that contains the field)
					FObjectImport& OuterImport	= ImportMap(-Import.OuterIndex-1);
					if ( OuterImport.XObject != NULL )
					{
						FindOuter = OuterImport.XObject;
					}
				}

				UObject* FindObject = StaticFindObject(FindClass, FindOuter, *Import.ObjectName.ToString());
				// reference to native transient class or CDO of such a class
				UBOOL IsNativeTransient	= FindObject != NULL && (FindObject->HasAllFlags(RF_Public|RF_Native|RF_Transient) || (FindObject->HasAnyFlags(RF_ClassDefaultObject) && FindObject->GetClass()->HasAllFlags(RF_Public|RF_Native|RF_Transient)));
				if (FindObject != NULL && ((LoadFlags & LOAD_FindIfFail) || IsNativeTransient))
				{
					Import.XObject = FindObject;
					GImportCount++;
				}
				else
				{
					SafeReplace = TRUE;
				}
			}
			else
			{
				SafeReplace = TRUE;
			}
		}

		if (!Import.XObject && !SafeReplace)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Loads all objects in package.
 *
 * @param bForcePreload	Whether to explicitly call Preload (serialize) right away instead of being
 *						called from EndLoad()
 */
void ULinkerLoad::LoadAllObjects( UBOOL bForcePreload )
{
	if ( (LoadFlags&LOAD_SeekFree) != 0 )
	{
		bForcePreload = TRUE;
	}

	UBOOL bAllowedToShowStatusUpdate = (LoadFlags & ( LOAD_Quiet | LOAD_SeekFree ) ) == 0;
	DOUBLE StartTime = appSeconds();

	//@script patcher: use ExportMap.Num() rather than Summary.ExportCount because when we add patch exports we don't alter the summary's ExportCount
	for( INT i=0; i< ExportMap.Num(); i++ )
	{
#if WITH_EDITOR
		// If we have been loading in here for a while, display a status update.  We will only send this update once and only during the editor load splash screen.
		if ( GIsEditor && !GIsUCC && !GIsSlowTask && bAllowedToShowStatusUpdate &&  (appSeconds() - StartTime) > 0.5f )
		{
			FString CleanFilename = FFilename( *Filename ).GetCleanFilename();
			GWarn->StatusUpdatef( 0, ULinkerDefs::TotalProgressSteps, LocalizeSecure(LocalizeProgress(TEXT("Loading"),TEXT("Core")), *CleanFilename) );
			bAllowedToShowStatusUpdate = FALSE;
		}
#endif

		UObject* Object = CreateExport( i );
		if( Object && (bForcePreload || Object->GetClass() == UClass::StaticClass() || Object->IsTemplate()) )
		{
			Preload( Object );
		}
	}

	// Mark package as having been fully loaded.
	if( LinkerRoot )
	{
		LinkerRoot->MarkAsFullyLoaded();
	}
}

/**
 * Returns the ObjectName associated with the resource indicated.
 * 
 * @param	ResourceIndex	location of the object resource
 *
 * @return	ObjectName for the FObjectResource at ResourceIndex, or NAME_None if not found
 */
FName ULinkerLoad::ResolveResourceName( PACKAGE_INDEX ResourceIndex )
{
	if ( ResourceIndex > 0 )
	{
		check(ExportMap.IsValidIndex(ResourceIndex-1));
		return ExportMap(ResourceIndex-1).ObjectName;
	}
	else if ( ResourceIndex < 0 )
	{
		check(ImportMap.IsValidIndex(-ResourceIndex-1));
		return ImportMap(-ResourceIndex-1).ObjectName;
	}
	return NAME_None;
}

// Find the index of a specified object without regard to specific package.
INT ULinkerLoad::FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, INT ExportOuterIndex )
{
	INT iHash = HashNames( ObjectName, ClassName, ClassPackage ) & (ARRAY_COUNT(ExportHash)-1);
	for( INT i=ExportHash[iHash]; i!=INDEX_NONE; i=ExportMap(i)._iHashNext )
	{
		if
		(  (ExportMap(i).ObjectName  ==ObjectName                              )
		&& (ExportMap(i).OuterIndex  ==ExportOuterIndex || ExportOuterIndex==INDEX_NONE)
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

		if(Export.ObjectName == ObjectName && (ExportOuterIndex == INDEX_NONE || Export.OuterIndex == ExportOuterIndex))
		{
			UClass*	ExportClass = Cast<UClass>(IndexToObject(Export.ClassIndex));

			// See if this export's class inherits from the requested class.
			for(UClass* ParentClass = ExportClass;ParentClass;ParentClass = ParentClass->GetSuperClass())
			{
				if(ParentClass->GetFName() == ClassName)
				{
					return ExportIndex;
				}
			}
		}
	}

	return INDEX_NONE;
}

/**
 * Function to create the instance of, or verify the presence of, an object as found in this Linker.
 *
 * @param ObjectClass	The class of the object
 * @param ObjectName	The name of the object
 * @param Outer			Find the object inside this outer (and only directly inside this outer, as we require fully qualified names)
 * @param LoadFlags		Flags used to determine if the object is being verified or should be created
 * @param Checked		Whether or not a failure will throw an error
 * @return The created object, or (UObject*)-1 if this is just verifying
 */
UObject* ULinkerLoad::Create( UClass* ObjectClass, FName ObjectName, UObject* Outer, DWORD LoadFlags, UBOOL Checked )
{
	// We no longer handle a NULL outer, which used to mean look in any outer, but we need fully qualified names now
	// The other case where this was NULL is if you are calling StaticLoadObject on the top-level package, but
	// you should be using LoadPackage. If for some weird reason you need to load the top-level package with this,
	// then I believe you'd want to set OuterIndex to 0 when Outer is NULL, but then that could get confused with
	// loading A.A (they both have OuterIndex of 0, according to Ron)
	check(Outer);

	INT OuterIndex = INDEX_NONE;

	// if the outer is the outermost of the package, then we want OuterIndex to be 0, as objects under the top level
	// will have an OuterIndex to 0
	if (Outer == Outer->GetOutermost())
	{
		OuterIndex = 0;
	}
	// otherwise get the linker index of the outer to be the outer index that we look in
	else
	{
		OuterIndex = Outer->GetLinkerIndex();
		// we _need_ the linker index of the outer to look in, which means that the outer must have been actually 
		// loaded off disk, and not just CreatePackage'd
		check(OuterIndex != INDEX_NONE);

		// we have to add 1 to put it into the Package Index format (- for imports, + for exports)
		OuterIndex += 1;
	}

	INT Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, OuterIndex);
	if (Index != INDEX_NONE)
	{
		return (LoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
	}

	// since we didn't find it, see if we can find an object redirector with the same name
	// Are we allowed to follow redirects?
	if( !( LoadFlags & LOAD_NoRedirects ) )
	{
		Index = FindExportIndex(UObjectRedirector::StaticClass()->GetFName(), NAME_Core, ObjectName, OuterIndex);

		// if we found a redirector, create it, and move on down the line
		if (Index != INDEX_NONE)
		{
			// create the redirector,
			UObjectRedirector* Redir = (UObjectRedirector*)CreateExport(Index);
			Preload(Redir);
			// if we found what it was point to, then return it
			if (Redir->DestinationObject && Redir->DestinationObject->GetClass() == ObjectClass)
			{
				// send a callback saying we followed a redirector successfully
				GCallbackEvent->Send(CALLBACK_RedirectorFollowed, Filename, Redir);
				// and return the object we are being redirected to
				return Redir->DestinationObject;
			}
		}
	}


// Set this to 1 to find nonqualified names anyway
#define FIND_OBJECT_NONQUALIFIED 0
// Set this to 1 if you want to see what it would have found previously. This is useful for fixing up hundreds
// of now-illegal references in script code.
#define DEBUG_PRINT_NONQUALIFIED_RESULT 1

#if DEBUG_PRINT_NONQUALIFIED_RESULT || FIND_OBJECT_NONQUALIFIED
	Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, INDEX_NONE);
	if (Index != INDEX_NONE)
	{
#if DEBUG_PRINT_NONQUALIFIED_RESULT
		warnf(NAME_Warning, TEXT("Using a non-qualified name (would have) found: %s"), *GetExportFullName(Index));
#endif
#if FIND_OBJECT_NONQUALIFIED
		return (LoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
#endif
	}
#endif


	// if we are checking for failure cases, and we failed, throw an error
	if( Checked )
	{
		appThrowf(LocalizeSecure(LocalizeError(TEXT("FailedCreate"),TEXT("Core")), *ObjectClass->GetName(), *ObjectName.ToString()));
	}
	return NULL;
}

/** version of Create() using export/linker index for Outer instead of an object reference
  * @param ObjectClass	The class of the object
  * @param ObjectName	The name of the object
  * @param OuterIndex	export/linker index of the Outer for this object (ROOTPACKAGE_INDEX if Outer is LinkerRoot)
  * @param LoadFlags	Flags used to determine if the object is being verified or should be created
  * @param Checked		Whether or not a failure will throw an error
  */
UObject* ULinkerLoad::CreateByOuterIndex(UClass* ObjectClass, FName ObjectName, INT OuterIndex, DWORD LoadFlags, UBOOL Checked)
{
	// export/linker index only
	check(OuterIndex >= 0);

	if (OuterIndex != 0)
	{
		// we have to add 1 to put it into the Package Index format (- for imports, + for exports)
		OuterIndex += 1;
	}

	INT Index = FindExportIndex(ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, OuterIndex);
	if (Index != INDEX_NONE)
	{
		return (LoadFlags & LOAD_Verify) ? INVALID_OBJECT : CreateExport(Index);
	}

	// since we didn't find it, see if we can find an object redirector with the same name
	// Are we allowed to follow redirects?
	if (!(LoadFlags & LOAD_NoRedirects))
	{
		Index = FindExportIndex(UObjectRedirector::StaticClass()->GetFName(), NAME_Core, ObjectName, OuterIndex);

		// if we found a redirector, create it, and move on down the line
		if (Index != INDEX_NONE)
		{
			// create the redirector,
			UObjectRedirector* Redir = (UObjectRedirector*)CreateExport(Index);
			Preload(Redir);
			// if we found what it was point to, then return it
			if (Redir->DestinationObject && Redir->DestinationObject->GetClass() == ObjectClass)
			{
				// send a callback saying we followed a redirector successfully
				GCallbackEvent->Send(CALLBACK_RedirectorFollowed, Filename, Redir);
				// and return the object we are being redirected to
				return Redir->DestinationObject;
			}
		}
	}

	// if we are checking for failure cases, and we failed, throw an error
	if (Checked)
	{
		appThrowf(LocalizeSecure(LocalizeError(TEXT("FailedCreate"),TEXT("Core")), *ObjectClass->GetName(), *ObjectName.ToString()));
	}
	return NULL;
}

/**
 * Serialize the object data for the specified object from the unreal package file.  Loads any
 * additional resources required for the object to be in a valid state to receive the loaded
 * data, such as the object's Outer, Class, or ObjectArchetype.
 *
 * When this function exits, Object is guaranteed to contain the data stored that was stored on disk.
 *
 * @param	Object	The object to load data for.  If the data for this object isn't stored in this
 *					ULinkerLoad, routes the call to the appropriate linker.  Data serialization is 
 *					skipped if the object has already been loaded (as indicated by the RF_NeedLoad flag
 *					not set for the object), so safe to call on objects that have already been loaded.
 *					Note that this function assumes that Object has already been initialized against
 *					its template object.
 *					If Object is a UClass and the class default object has already been created, calls
 *					Preload for the class default object as well.
 */
void ULinkerLoad::Preload( UObject* Object )
{
	check(IsValid());
	check(Object);
	// Preload the object if necessary.
	if( Object->HasAnyFlags(RF_NeedLoad) )
	{
		if( Object->GetLinker()==this )
		{
			UClass* Cls = NULL;

			// If this is a struct, make sure that its parent struct is completely loaded
			if(	Object->IsA(UStruct::StaticClass()) )
			{
				Cls = Cast<UClass>(Object);
				if( ((UStruct*)Object)->SuperStruct )
				{
					Preload( ((UStruct*)Object)->SuperStruct );
				}
			}

			// make sure this object didn't get loaded in the above Preload call
			if (Object->HasAnyFlags(RF_NeedLoad))
			{
				// grab the resource for this Object
				FObjectExport& Export = ExportMap( Object->_LinkerIndex );
				check(Export._Object==Object);

#if SUPPORTS_SCRIPTPATCH_LOADING
				//@script patcher: if this export was added by the script patcher, swap the file reader for the patcher's memory reader
				FArchive* SavedLoader = Loader;
				if ( Export.HasAnyFlags(EF_ScriptPatcherExport) )
				{
					// if we have an export with the EF_ScriptPatcherExport flag, we should have already created a patch archive when we were first created
					checkSlow(PatchDataAr);
					Loader = PatchDataAr;
				}
				else if (!GIsScriptPatcherActive)
				{
					// load object with original loader since it was not patched
					Loader = OriginalLoader;
				}
#endif

				const INT SavedPos = Loader->Tell();

				// move to the position in the file where this object's data
				// is stored
				Loader->Seek( Export.SerialOffset );

				// tell the file reader to read the raw data from disk
				Loader->Precache( Export.SerialOffset, Export.SerialSize );

				// mark the object to indicate that it has been loaded
				Object->ClearFlags ( RF_NeedLoad );

				if ( Object->HasAnyFlags(RF_ClassDefaultObject) )
				{
					// In order to ensure that the CDO inherits config & localized property values from the parent class, we can't initialize the CDO until
					// the parent class's CDO has serialized its data from disk and called LoadConfig/LoadLocalized. Since ULinkerLoad::Preload guarantees that
					// Preload is always called on the archetype first, at this point we know that the CDO's archetype (the CDO of the parent class) has already
					// loaded its config/localized data, so it's now safe to initialize the CDO
					Object->InitClassDefaultObject(Object->GetClass());

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
					DOUBLE PreviousLocalTime = SerializationPerfTracker.GetTotalEventTime(Object->GetClass());
					DOUBLE PreviousGlobalTime = GObjectSerializationPerfTracker->GetTotalEventTime(Object->GetClass());
					DOUBLE StartTime = appSeconds();
#endif

					Object->GetClass()->SerializeDefaultObject(Object, *this);

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
					DOUBLE TimeSpent = (appSeconds() - StartTime) * 1000;
					// add the data to the linker-specific tracker
					SerializationPerfTracker.TrackEvent(Object->GetClass(), PreviousLocalTime, TimeSpent);
					// add the data to the global tracker
					GObjectSerializationPerfTracker->TrackEvent(Object->GetClass(), PreviousGlobalTime, TimeSpent);
#endif
				}
				else
				{
#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
					DOUBLE PreviousLocalTime = SerializationPerfTracker.GetTotalEventTime(Object->GetClass());
					DOUBLE PreviousGlobalTime = GObjectSerializationPerfTracker->GetTotalEventTime(Object->GetClass());
					DOUBLE PreviousClassGlobalTime=0.f;
					if ( Object->GetClass() == UClass::StaticClass() )
					{
						PreviousClassGlobalTime = GClassSerializationPerfTracker->GetTotalEventTime(static_cast<UClass*>(Object));
					}
					DOUBLE StartTime = appSeconds();
#endif

					// Maintain the current GSerializedObjects.
					UObject* PrevSerializedObject = GSerializedObject;
					GSerializedObject = Object;
					Object->Serialize( *this );
					GSerializedObject = PrevSerializedObject;

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
					DOUBLE TimeSpent = (appSeconds() - StartTime) * 1000;
					// add the data to the linker-specific tracker
					SerializationPerfTracker.TrackEvent(Object->GetClass(), PreviousLocalTime, TimeSpent);
					// add the data to the global tracker
					GObjectSerializationPerfTracker->TrackEvent(Object->GetClass(), PreviousGlobalTime, TimeSpent);
					if ( Object->GetClass() == UClass::StaticClass() )
					{
						GClassSerializationPerfTracker->TrackEvent(static_cast<UClass*>(Object), PreviousClassGlobalTime, TimeSpent);
					}
#endif
				}

				// Make sure we serialized the right amount of stuff.
				if( Tell()-Export.SerialOffset != Export.SerialSize )
				{
					appErrorf( LocalizeSecure(LocalizeError(TEXT("SerialSize"),TEXT("Core")), *Object->GetFullName(), Tell()-Export.SerialOffset, Export.SerialSize) );
				}

				Loader->Seek( SavedPos );

#if SUPPORTS_SCRIPTPATCH_LOADING
				//@script patcher
				Loader = SavedLoader;
#endif


				if ( Object->HasAnyFlags(RF_ClassDefaultObject) && !GIsUCCMake )
				{
					// if this is the class default object, we'll need to import all
					// config and localized data here, so that when InitClassDefaultObject
					// is called for child classes' default object, the values for this class
					// are correct when the derived class's default object is initialized in
					// InitProperties
					Object->LoadConfig();

					//@todo ronp - passing TRUE for bLoadHierarchecally is only really necessary for classes that
					// inherit arrays of structs which contain both localized and non-localized data.  What happens
					// is if the derived class adds any elements to the array (or changes any existing elements in defprops),
					// it will serialize the entire array into the package.  When the array is de-serialized from disk, it
					// will first clear the existing elements from the array causing the class to lose any localized data
					// for those elements which were defined in the parent class's loc section but not duplicated into the
					// derived class's loc section
					Object->LoadLocalized(NULL, TRUE);
				}
				// if this is a UClass object and it already has a class default object
				else if ( Cls != NULL && Cls->GetDefaultsCount() )
				{
					// make sure that the class default object is completely loaded as well
					Preload(Cls->GetDefaultObject());
				}
			}
		}
		else if( Object->GetLinker() )
		{
			// Send to the object's linker.
			Object->GetLinker()->Preload( Object );
		}
	}
}

/**
 * Builds a string containing the full path for a resource in the export table.
 *
 * @param OutPathName		[out] Will contain the full path for the resource
 * @param ResourceIndex		Index of a resource in the export table
 */
void ULinkerLoad::BuildPathName( FString& OutPathName, INT ResourceIndex ) const
{
	if ( ResourceIndex == ROOTPACKAGE_INDEX )
	{
		return;
	}

	if ( ResourceIndex > 0 )
	{
		const FObjectExport& Export = ExportMap( ResourceIndex - 1 );
		BuildPathName( OutPathName, Export.OuterIndex );
		if ( OutPathName.Len() > 0 )
		{
			OutPathName += TEXT('.');
		}
		OutPathName += Export.ObjectName.ToString();
	}
	else
	{
		const FObjectImport& Import = ImportMap( -ResourceIndex - 1 );
		BuildPathName( OutPathName, Import.OuterIndex );
		if ( OutPathName.Len() > 0 )
		{
			OutPathName += TEXT('.');
		}
		OutPathName += Import.ObjectName.ToString();
	}
}

/**
 * Checks if the specified export should be loaded or not.
 * Performs similar checks as CreateExport().
 *
 * @param ExportIndex	Index of the export to check
 * @return				TRUE of the export should be loaded
 */
UBOOL ULinkerLoad::WillTextureBeLoaded( UClass* Class, INT ExportIndex )
{
	const FObjectExport& Export = ExportMap( ExportIndex );

	// Already loaded, or not allowed to load?
	if ( Export._Object || (Export.ObjectFlags & _ContextFlags) == 0 )
	{
		return FALSE;
	}

	// Build path name
	FString PathName;
	PathName.Reserve(256);
	BuildPathName( PathName, ExportIndex + 1 );

//	UObject* ExistingTexture = StaticFindObject( Class, ANY_PACKAGE, *PathName );
	UObject* ExistingTexture = StaticFindObjectFastExplicit( Class, Export.ObjectName, PathName, FALSE, FALSE );
	if ( ExistingTexture )
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

UObject* ULinkerLoad::CreateExport( INT Index )
{
	FScopedCreateExportCounter ScopedCounter( this, Index );

	// Map the object into our table.
	FObjectExport& Export = ExportMap( Index );

	// Check whether we already loaded the object and if not whether the context flags allow loading it.
	if( !Export._Object && (Export.ObjectFlags & _ContextFlags) )
	{
		check(Export.ObjectName!=NAME_None || !(Export.ObjectFlags&RF_Public));
		check(GObjBeginLoadCount>0);

		// editor loads force exported object from original package, not cooked package (unless we are script patching)
		if (GIsEditor && !GIsScriptPatcherActive && Export.HasAnyFlags(EF_ForcedExport))
		{
			const FName& ClassName = GetExportClassName(Index);
			FName OuterClassName;

			// don't bother loading top level packages
			if (Export.OuterIndex == ROOTPACKAGE_INDEX)
			{
				return NULL;
			}

			// get the class name of the outer, for imports or export outers
			if (IS_IMPORT_INDEX(Export.OuterIndex))
			{
				OuterClassName = ImportMap(-Export.OuterIndex-1).ClassName;
			}
			else
			{
				OuterClassName = GetExportClassName(Export.OuterIndex - 1);
			}

			// get the export's class
			UClass* LoadClass = Export.ClassIndex == 0 ? UClass::StaticClass() : (UClass*)IndexToObject(Export.ClassIndex);

			// don't load packages or subobjects (objects not directory in a package)
			if (ClassName == NAME_Package || OuterClassName != NAME_Package)
			{
				// but try to at least find them
				UObject* Outer = IndexToObject(Export.OuterIndex);
				if (Outer)
				{
					Outer->GetLinker()->Preload(Outer);
					// must use FindObjectFast so that we don't create packages while looking for subobjects
					return UObject::StaticFindObjectFast(LoadClass, Outer, Export.ObjectName, TRUE);
				}
				return NULL;
			}

			// load from original package
			UObject* OriginalLinkerObject = UObject::StaticLoadObject(LoadClass, NULL, *GetExportPathName(Index, NULL, TRUE), NULL, LOAD_None, NULL);

			if (!OriginalLinkerObject)
			{
				warnf(TEXT("Failed to load forceexport object %s from original package"), *GetExportFullName(Index));
			}

			// this will cause Detach to crash because the _Linker is mismatched
			//Export._Object = OriginalLinkerObject;

			// return the object we just found (or NULL if we didn't)
			return OriginalLinkerObject;
		}

		// Get the object's class.
		UClass* LoadClass = (UClass*)IndexToObject( Export.ClassIndex );
		if( !LoadClass && Export.ClassIndex!=UCLASS_INDEX ) // Hack to load packages with classes which do not exist.
		{
			return NULL;
		}
		if( !LoadClass )
		{
			LoadClass = UClass::StaticClass();
		}

#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher: when running the patch commandlet, we'll have multiple versions of native classes in memory, but only the first will
		// receive the correct ClassConstructor, so if we're about to create a class that already exists in memory (in another package), copy its class
		// constructor over to the new class
		UClass* OriginalVersion = NULL;
		if ( GIsScriptPatcherActive && LoadClass == UClass::StaticClass() && (Export.ObjectFlags&RF_Native) != 0 )
		{
			OriginalVersion = FindObject<UClass>(ANY_PACKAGE, *Export.ObjectName.ToString(), TRUE);
			if( OriginalVersion != NULL )
			{
				check(OriginalVersion->ClassConstructor);
			}
		}
#endif
		check(LoadClass);
		check(LoadClass->GetClass()==UClass::StaticClass());

		// Only UClass objects and UProperty objects of intrinsic classes can have RF_Native set. Those property objects are never
		// serialized so we only have to worry about classes. If we encounter an object that is not a class and has RF_Native set
		// we warn about it and remove the flag.
		if( (Export.ObjectFlags & RF_Native) != 0 && !LoadClass->IsChildOf(UField::StaticClass()) )
		{
			warnf(NAME_Warning,TEXT("%s %s has RF_Native set but is not a UField derived class"),*LoadClass->GetName(),*Export.ObjectName.ToString());
			// Remove RF_Native;
			Export.ObjectFlags &= ~RF_Native;
		}

		if ( !LoadClass->HasAnyClassFlags(CLASS_Intrinsic) )
		{
			Preload( LoadClass );
			if ( LoadClass->HasAnyClassFlags(CLASS_Deprecated) && GIsEditor && !GIsUCC && !GIsGame )
			{
				if ( (Export.ObjectFlags&RF_ClassDefaultObject) == 0 )
				{
					warnf( NAME_Warning, LocalizeSecure(LocalizeError(TEXT("LoadedDeprecatedClassInstance"), TEXT("Core")), *GetExportFullName(Index), *LoadClass->GetPathName()) );
					EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, LocalizeSecure(LocalizeError(TEXT("LoadedDeprecatedClassInstance"), TEXT("Core")), *GetExportFullName(Index), *LoadClass->GetPathName()) );
				}
			}
		}
		else if ( !LoadClass->HasAnyCastFlag(CASTCLASS_UField) )
		{
			// if this object's class is intrinsic, we need to make sure that the class has been linked before creating any instances
			// of this class; otherwise, the instance will be initialized against incorrect defaults when InitProperties is called
			LoadClass->ConditionalLink();
		}

		// detect cases where a class has been made transient when there are existing instances of this class in content packages,
		// and this isn't the class default object; when this happens, it can cause issues which are difficult to debug since they'll
		// only appear much later after this package has been loaded
		if ( LoadClass->HasAnyClassFlags(CLASS_Transient) && (Export.ObjectFlags&RF_ClassDefaultObject) == 0 )
		{
			//@todo - should this actually be an assertion?
			warnf(NAME_Warning, LocalizeSecure(LocalizeError(TEXT("LoadingTransientInstance"), TEXT("Core")), *Filename, *Export.ObjectName.ToString(), *LoadClass->GetPathName()));

			if ( GIsEditor && !GIsUCC && !GIsGame )
			{
				EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, LocalizeSecure(LocalizeError(TEXT("LoadingTransientInstance"), TEXT("Core")), *Filename, *Export.ObjectName.ToString(), *LoadClass->GetPathName()) );
			}
		}

		// Find or create the object's Outer.
		UObject* ThisParent = NULL;
		if( Export.OuterIndex != ROOTPACKAGE_INDEX )
		{
			ThisParent = IndexToObject(Export.OuterIndex);
		}
		else if( Export.HasAnyFlags( EF_ForcedExport ) )
		{
			// Create the forced export in the TopLevel instead of LinkerRoot. Please note that CreatePackage
			// will find and return an existing object if one exists and only create a new one if there doesn't.
			Export._Object = CreatePackage( NULL, *Export.ObjectName.ToString() );
			check(Export._Object);
			((UPackage*)Export._Object)->InitNetInfo(this, Index);
			GForcedExportCount++;
		}
		else
		{
			ThisParent = LinkerRoot;
		}

		// If loading the object's Outer caused the object to be loaded or if it was a forced export package created
		// above, return it.
		if( Export._Object != NULL )
		{
			return Export._Object;
		}

		if( ThisParent == NULL )
		{
			// mark this export as unloadable (so that other exports that
			// reference this one won't continue to execute the above logic), then return NULL
			Export.ObjectFlags &= ~_ContextFlags;
			if ( GIsEditor && !GIsUCC )
			{
				EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *FString::Printf(TEXT("Outer object for %s"), *GetExportFullName(Index)) );
			}
			else
			{
				// otherwise, return NULL and let the calling code determine what to do
				FString OuterName;
				if ( IS_IMPORT_INDEX(Export.OuterIndex) )
				{
					OuterName = GetImportFullName( -Export.OuterIndex - 1 );
				}
				else if ( Export.OuterIndex > 0 )
				{
					OuterName = GetExportFullName(Export.OuterIndex - 1 );
				}
				else
				{
					OuterName = LinkerRoot->GetFullName();
				}
				warnf(NAME_Warning, TEXT("CreateExport: Failed to load Outer for resource '%s': %s"), *Export.ObjectName.ToString(), *OuterName);
			}
			return NULL;
		}

		if ( Export.ArchetypeIndex == Index + 1 )
		{
			// this export's archetype is pointing to itself!!
			// mark this export as unloadable (so that other exports that
			// reference this one won't continue to execute the above logic), then return NULL
			Export.ObjectFlags &= ~_ContextFlags;
			if ( GIsEditor && !GIsUCC )
			{
				EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *FString::Printf(TEXT("Circular reference to archetype for %s.%s"), *ThisParent->GetPathName(), *Export.ObjectName.ToString()) );
			}
			else
			{
				warnf(NAME_Error, TEXT("Circular reference to archetype for %s.%s"), *ThisParent->GetPathName(), *Export.ObjectName.ToString());
			}

			return NULL;
		}

		// make sure an imported Template exists 
		if( !(LinkerRoot->PackageFlags & PKG_Cooked)  && (!GIsGame || GIsEditor || GIsUCC) && IS_IMPORT_INDEX(Export.ArchetypeIndex) ) 
		{
			VerifyImport(-Export.ArchetypeIndex - 1); 
		}

		// Find the Archetype object for the one we are loading.
		UObject* Template = NULL;
		if(Export.ArchetypeIndex != CLASSDEFAULTS_INDEX)
		{
			Template = IndexToObject(Export.ArchetypeIndex);

			// we couldn't load the exports's ObjectArchetype; unless the object's class is deprecated, issue a warning that the archetype is missing
			if( Template == NULL && !LoadClass->HasAnyClassFlags(CLASS_Deprecated) )
			{
				if( GIsEditor && !GIsUCC )
				{
					// if we're running in the editor, allow the resource to be loaded against the class default object
					// but flag this as a warning
					FString ArchetypeName;
					if ( IS_IMPORT_INDEX(Export.ArchetypeIndex) )
					{
						ArchetypeName = GetImportFullName( -Export.ArchetypeIndex - 1 );
					}
					else if ( Export.ArchetypeIndex > 0 )
					{
						ArchetypeName = GetExportFullName(Export.ArchetypeIndex - 1 );
					}
					else
					{
						ArchetypeName = TEXT("ClassDefaultObject");
					}
					EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, TEXT("ObjectArchetype for '%s': %s in %s"), *Export.ObjectName.ToString(), *ArchetypeName, *ThisParent->GetPathName() );
				}
				else
				{
					// otherwise, log a warning and just use the class default object
					FString ArchetypeName;
					if ( IS_IMPORT_INDEX(Export.ArchetypeIndex) )
					{
						ArchetypeName = GetImportFullName( -Export.ArchetypeIndex - 1 );
					}
					else if ( Export.ArchetypeIndex > 0 )
					{
						ArchetypeName = GetExportFullName(Export.ArchetypeIndex - 1 );
					}
					else
					{
						ArchetypeName = TEXT("ClassDefaultObject");
					}
					debugf( NAME_Warning, TEXT("Failed to load ObjectArchetype for resource '%s': %s in %s"), *Export.ObjectName.ToString(), *ArchetypeName, *ThisParent->GetPathName() );
				}
			}
		}

		// If we could not find the Template (eg. the package containing it was deleted), use the class defaults instead.
		if(Template == NULL)
		{
			// force the default object to be created because we can't create an object
			// of this class unless it has a template to initialize itself against

			//@script patcher (only need to call GetSuperClass()->GetDefaultObject() when running script patcher
			Template = ((Export.ObjectFlags&RF_ClassDefaultObject) == 0 || LoadClass->GetFName() == NAME_Object)
				? LoadClass->GetDefaultObject(TRUE)
				: LoadClass->GetSuperClass()->GetDefaultObject(TRUE);
		}

		check(Template);
		//@script patcher todo: might need to adjust the following assertion
		checkSlow((Export.ObjectFlags&RF_ClassDefaultObject)!=0 || Template->IsA(LoadClass));

		// make sure the object's archetype is fully loaded before creating the object
		Preload(Template);

		// Try to find existing object first in case we're a forced export to be able to reconcile. Also do it for the
		// case of async loading as we cannot in-place replace objects.
		if(	(LinkerRoot->PackageFlags & PKG_Cooked)
		||	(GIsGame && !GIsEditor && !GIsUCC) 
		||	GIsAsyncLoading 
		||	Export.HasAnyFlags( EF_ForcedExport ) 
		||	LinkerRoot->ShouldFindExportsInMemoryFirst()
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		||	GIsScriptPatcherActive
#endif
			)
		{
			// Find object after making sure it isn't already set. This would be bad as the code below NULLs it in a certain
			// case, which if it had been set would cause a linker detach mismatch.
			check( Export._Object == NULL );
			Export._Object = StaticFindObjectFastInternal( LoadClass, ThisParent, Export.ObjectName, TRUE, FALSE, 0 );
		
			// Object is found in memory.
			if( Export._Object )
			{
				// Native classes need to be in-place replaced for their initial load.
				if( LoadClass == UClass::StaticClass() && (Export.ObjectFlags&RF_Native) != 0 && ((UClass*)Export._Object)->bNeedsPropertiesLinked )
				{
					Export._Object = NULL;
				}
				// Found object, associate and return.
				else
				{
					// Mark that we need to dissociate forced exports later on if we are a forced export.
					if( Export.HasAnyFlags( EF_ForcedExport ) )
					{
						GForcedExportCount++;
					}
					// Associate linker with object to avoid detachment mismatches.
					else
					{
						Export._Object->SetLinker( this, Index );
					}
					return Export._Object;
				}
			}
		}

		// Create the export object, marking it with the appropriate flags to
		// indicate that the object's data still needs to be loaded.

#if CONSOLE
		EObjectFlags LoadFlags = (Export.ObjectFlags&RF_Load)|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects;
#else
		EObjectFlags LoadFlags = (Export.ObjectFlags&RF_Load);
		// if we are loading objects just to verify an object reference during script compilation,
		if ((GUglyHackFlags&HACK_VerifyObjectReferencesOnly) == 0
		||	(LoadFlags&RF_ClassDefaultObject) != 0					// only load this object if it's a class default object
		||	(LinkerRoot->PackageFlags&PKG_ContainsScript) != 0		// or we're loading an existing package and it's a script package
		||	ThisParent->IsTemplate(RF_ClassDefaultObject)			// or if its a subobject template in a CDO
		||	LoadClass->IsChildOf(UField::StaticClass())				// or if it is a UField
		||	LoadClass->IsChildOf(UObjectRedirector::StaticClass()))	// or if its a redirector to another object
		{
			LoadFlags |= (RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects);
		}
#endif
		Export._Object = StaticConstructObject
		(
			LoadClass,
			ThisParent,
			Export.ObjectName,
			LoadFlags | (GIsInitialLoad ? (RF_RootSet | RF_DisregardForGC) : 0),
			Template
		);
		
		if( Export._Object )
		{
			Export._Object->SetLinker( this, Index );

			// we created the object, but the data stored on disk for this object has not yet been loaded,
			// so add the object to the list of objects that need to be loaded, which will be processed
			// in UObject::EndLoad()
			GObjLoaded.AddItem( Export._Object );
			debugfSlow( NAME_DevLoad, TEXT("Created %s"), *Export._Object->GetFullName() );
		}
		else
		{
			debugf( NAME_Warning, TEXT("ULinker::CreatedExport failed to construct object %s %s"), *LoadClass->GetName(), *Export.ObjectName.ToString() );
		}

		if ( Export._Object != NULL )
		{
			// If it's a struct or class, set its parent.
			if( Export._Object->IsA(UStruct::StaticClass()) )
			{
				//@script patcher: if this struct has some member properties or functions that will be added by a script patch, mark the
				// struct with a flag so that it retrieves those fields before it links everything
				if ( Export.HasAnyFlags(EF_MemberFieldPatchPending) )
				{
					Export._Object->SetFlags(RF_PendingFieldPatches);
				}

				if ( Export.SuperIndex != NULLSUPER_INDEX )
				{
					((UStruct*)Export._Object)->SuperStruct = (UStruct*)IndexToObject( Export.SuperIndex );
				}

				// If it's a class, bind it to C++.
				if( Export._Object->IsA( UClass::StaticClass() ) )
				{
					UClass* ClassObject = static_cast<UClass*>(Export._Object);
#if SUPPORTS_SCRIPTPATCH_CREATION
					//@script patcher: here we we make certain we have a valid ClassConstructor after possibly setting it above
					if( OriginalVersion != NULL )
					{
						ClassObject->ClassConstructor = OriginalVersion->ClassConstructor;
					}
#endif
					ClassObject->Bind();
				}
			}
	
			// Mark that we need to dissociate forced exports later on.
			if( Export.HasAnyFlags( EF_ForcedExport ) )
			{
				GForcedExportCount++;
			}
		}
	}
	return Export._Object;
}

// Return the loaded object corresponding to an import index; any errors are fatal.
UObject* ULinkerLoad::CreateImport( INT Index )
{
	FScopedCreateImportCounter ScopedCounter( this, Index );
	FObjectImport& Import = ImportMap( Index );

	if( Import.XObject == NULL )
	{
		// Look in memory first.
		if ((!GIsEditor && !GIsUCC)
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		||	GIsScriptPatcherActive
#endif
			)
		{
			// Try to find existing version in memory first.
			UObject* ClassPackage = StaticFindObjectFast( UPackage::StaticClass(), NULL, Import.ClassPackage, FALSE, FALSE ); 
			if( ClassPackage )
			{
				UClass*	FindClass = (UClass*) StaticFindObjectFast( UClass::StaticClass(), ClassPackage, Import.ClassName, FALSE, FALSE ); 
				if( FindClass )
				{
					UObject*	FindObject		= NULL;
	
					// Import is a toplevel package.
					if( Import.OuterIndex == ROOTPACKAGE_INDEX )
					{
						FindObject = CreatePackage( NULL, *Import.ObjectName.ToString() );
					}
					// Import is regular import/ export.
					else
					{
						// Find the imports' outer.
						UObject* FindOuter = NULL;
						// Import.
						if( IS_IMPORT_INDEX(Import.OuterIndex) )
						{
							FObjectImport& OuterImport = ImportMap(-Import.OuterIndex-1);
							// Outer already in memory.
							if( OuterImport.XObject )
							{
								FindOuter = OuterImport.XObject;
							}
							// Outer is toplevel package, create/ find it.
							else if( OuterImport.OuterIndex == ROOTPACKAGE_INDEX )
							{
								FindOuter = CreatePackage( NULL, *OuterImport.ObjectName.ToString() );
							}
							// Outer is regular import/ export, use IndexToObject to potentially recursively load/ find it.
							else
							{
								FindOuter = IndexToObject( Import.OuterIndex );
							}
						}
						// Export.
						else 
						{
							// Create/ find the object's outer.
							FindOuter = IndexToObject( Import.OuterIndex );
						}
						if (!FindOuter)
						{
							FString OuterName;
							if ( IS_IMPORT_INDEX(Import.OuterIndex) )
							{
								OuterName = GetImportFullName( -Import.OuterIndex - 1 );
							}
							else if ( Import.OuterIndex > 0 )
							{
								OuterName = GetExportFullName(Import.OuterIndex - 1 );
							}
							else
							{
								OuterName = LinkerRoot->GetFullName();
							}
	
							warnf(NAME_Warning, TEXT("CreateImport: Failed to load Outer for resource '%s': %s"), *Import.ObjectName.ToString(), *OuterName);
							return NULL;
						}
	
						// Find object now that we know it's class, outer and name.
						FindObject = StaticFindObjectFast( FindClass, FindOuter, Import.ObjectName, FALSE, FALSE );
					}

					if( FindObject )
					{		
						// Associate import and indicate that we associated an import for later cleanup.
						Import.XObject = FindObject;
						GImportCount++;
					}
				}
			}
		}

#if !SUPPORTS_SCRIPTPATCH_CREATION
		if( Import.XObject == NULL && !(Summary.PackageFlags & PKG_RequireImportsAlreadyLoaded)  )
#else
		//@script patcher
		if( Import.XObject == NULL &&
		(!(Summary.PackageFlags & PKG_RequireImportsAlreadyLoaded) || GIsScriptPatcherActive ) )
#endif
		{
			if( Import.SourceLinker == NULL )
			{
				VerifyImport(Index);
			}
			if(Import.SourceIndex != INDEX_NONE)
			{
				check(Import.SourceLinker);
				Import.XObject = Import.SourceLinker->CreateExport( Import.SourceIndex );
				GImportCount++;
			}
		}
	}
	return Import.XObject;
}

// Map an import/export index to an object; all errors here are fatal.
UObject* ULinkerLoad::IndexToObject( PACKAGE_INDEX Index )
{
	if( Index > 0 )
	{
		if( !ExportMap.IsValidIndex( Index-1 ) )
			appErrorf( LocalizeSecure(LocalizeError(TEXT("ExportIndex"),TEXT("Core")), Index-1, ExportMap.Num()) );			
		return CreateExport( Index-1 );
	}
	else if( Index < 0 )
	{
		if( !ImportMap.IsValidIndex( -Index-1 ) )
			appErrorf( LocalizeSecure(LocalizeError(TEXT("ImportIndex"),TEXT("Core")), -Index-1, ImportMap.Num()) );
		return CreateImport( -Index-1 );
	}
	else return NULL;
}

#if SUPPORTS_SCRIPTPATCH_LOADING

/**
* Adds the specified name to this ULinkerLoad's NameTable at runtime.  If the name already exists in the
* name table, it will not be added twice.
* 
* @param	NewName		the name to add
*/
void ULinkerLoad::AppendNames( const TArray<FName>& NewNames )
{
	for ( INT NameIndex = 0; NameIndex < NewNames.Num(); NameIndex++ )
	{
		const FName& NewName = NewNames(NameIndex);

		// have to add these entries, even if they already exist; otherwise, name references in any patched exports could be off
		new(NameMap) FName(NewName);
	}
}

/**
 * Adds a new import to the linker's ImportMap at runtime.
 * 
 * @param	NewImports	the imports to add
 */
void ULinkerLoad::AppendImports( const TArray<FObjectImport>& NewImports )
{
	for ( INT AppendIndex = 0; AppendIndex < NewImports.Num(); AppendIndex++ )
	{
		const FObjectImport& NewImport = NewImports(AppendIndex);

#if !FINAL_RELEASE
		// make sure we don't already have this import in the import map
		for ( INT ImportIndex = 0; ImportIndex < ImportMap.Num(); ImportIndex++ )
		{
			const FObjectImport& CheckImport = ImportMap(ImportIndex);
			if (	CheckImport.ObjectName		== NewImport.ObjectName
				&&	CheckImport.OuterIndex		== NewImport.OuterIndex
				&&	CheckImport.ClassPackage	== NewImport.ClassPackage
				&&	CheckImport.ClassName		== NewImport.ClassName
				&&	CheckImport.SourceIndex		== NewImport.SourceIndex
				)
			{
				// We cannot skip adding these patch imports, or any exports which reference imports which occur after this one will
				// be mapped to the wrong element.  The script patcher shouldn't create an import patch which has information identical
				// to an existing import; no way to recover, so complain
				// actually, it might be possible to simply add a NULL import here, instead of asserting...
				appThrowf(TEXT("Script patch attempting to add new import (%i) which matches an existing import (%i): %s"),
					AppendIndex, ImportIndex, *GetImportFullName(ImportIndex));
			}
		}
#endif

		new(ImportMap) FObjectImport(NewImport);
	}

}

/**
 * Adds new exports to the linker's ExportMap at runtime, after the linker is in a stable
 * (completely loaded and serialized) state.  Also creates objects for the new exports.
 * 
 * @param	Exports				the exports to be added
 * @param	ExportPatchData		the data for the exports being added
 */
void ULinkerLoad::AppendExports( const TArray<FObjectExport>& Exports, const TArray<FPatchData>& ExportPatchData )
{
	check(Exports.Num() == ExportPatchData.Num());

	for ( INT PatchIndex = 0; PatchIndex < Exports.Num(); PatchIndex++ )
	{
		const FObjectExport& NewExport = Exports(PatchIndex);

#if !FINAL_RELEASE
		// make sure we don't already have this export in the export map
		for ( INT ExportIndex = 0; ExportIndex < ExportMap.Num(); ExportIndex++ )
		{
			const FObjectExport& CheckExport = ExportMap(ExportIndex);
			if (	CheckExport.ObjectName		== NewExport.ObjectName
				&&	CheckExport.OuterIndex		== NewExport.OuterIndex
				&&	CheckExport.ClassIndex		== NewExport.ClassIndex
				&&	CheckExport.SuperIndex		== NewExport.SuperIndex
				&&	CheckExport.ArchetypeIndex	== NewExport.ArchetypeIndex
				)
			{
				// We cannot skip adding these patch exports, or any other exports which reference exports that occur after this one will
				// be mapped to the wrong element.  The script patcher shouldn't create an export patch which has information identical
				// to an existing export ; no way to recover, so complain
				// actually, it might be possible to simply add a NULL export here, instead of asserting...
				appThrowf(TEXT("Script patch attempting to add new export (%i) which matches an existing export (%i): %s"),
					PatchIndex, ExportIndex, *GetExportFullName(ExportIndex));
			}
		}
#endif

		// add the patch export to the linker's ExportMap
		FObjectExport* PatchExport = new(ExportMap) FObjectExport(NewExport);

		// and mark the export so that the linker knows that this export's data must be loaded from the PatchDataAr
		PatchExport->SetFlags(EF_ScriptPatcherExport);

		// now add the patch data to the patch reader archive and set the export's SerialOffset and SerialSize appropriately

		// set this export's offset to the current location of the memory-reader
		PatchExport->SerialOffset = PatchDataAr->TotalSize();

		// load the memory reader with the UObject data corresponding to this new export
		PatchDataAr->AddPatch(ExportPatchData(PatchIndex));

		// set the export's size
		PatchExport->SerialSize = PatchDataAr->TotalSize() - PatchExport->SerialOffset;

		// finally, if this export corresponds to a class member variable, we'll need to notify the mark the class's export so that
		// the class isn't linked (which generates the GC token stream for the class) until this export has been added to the class

		// it doesn't make sense for a patch export to have an outer in another package, and we don't need to do the patching if our
		// outer is also a patched export.
		if ( PatchExport->OuterIndex != ROOTPACKAGE_INDEX && PatchExport->OuterIndex < ExportMap.Num() && !ExportMap(PatchExport->OuterIndex-1).HasAnyFlags(EF_ScriptPatcherExport) )
		{
			FName ExportClassName = GetExportClassName(ExportMap.Num() - 1);

			// Name indexes between 1 and 19 are reserved for UProperty class names
			const UBOOL bRequiresLink = ExportClassName == NAME_Function || ExportClassName.GetIndex() < NAME_Core;
			if ( bRequiresLink )
			{
				FObjectExport& OuterExport = ExportMap(PatchExport->OuterIndex-1);
				OuterExport.SetFlags(EF_MemberFieldPatchPending);
			}
		}
	}
}

//@script patcher
/**
 * Returns a bytecode patch given an index into the ExportMap.
 *
 * @param	ExportIndex		an inde [into the ExportMap] for the patch to find
 *
 * @return	a pointer to the bytecode patch for the specified export, or NULL if that export doesn't have a bytecode patch
 */
FScriptPatchData* ULinkerLoad::FindBytecodePatch( INT ExportIndex ) const
{
	return FunctionsToPatch.FindRef(ExportIndex);
}

//@script patcher
/**
 * Returns an object data patch given an index into the ExportMap.
 *
 * @param	ExportIndex		an index [into the ExportMap] for the patch to find
 *
 * @return	a pointer to the object data patch for the specified export, or NULL if that export doesn't have a bytecode patch
 */
FPatchData* ULinkerLoad::FindDefaultsPatch( INT ExportIndex ) const
{
	return DefaultsToPatch.FindRef(ExportIndex);
}

//@script patcher
/**
 * Returns an enum patch given an index into the ExportMap
 *
 * @param	ExportIndex		an index [into the ExportMap] for the patch to find
 *
 * @return	a pointer to the enum value patch for the specified export, or NULL if that export doesn't have a patch
 */
FEnumPatchData* ULinkerLoad::FindEnumPatch( INT ExportIndex ) const
{
	return EnumsToPatch.FindRef(ExportIndex);
}

#endif // SUPPORTS_SCRIPTPATCH_LOADING


// Detach an export from this linker.
void ULinkerLoad::DetachExport( INT i )
{
	FObjectExport& E = ExportMap( i );
	check(E._Object);
	if( !E._Object->IsValid() )
	{
		appErrorf( TEXT("Linker object %s %s.%s is invalid"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString() );
	}
#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive && E.HasAnyFlags(EF_ForcedExport) && E._Object->GetLinker() != this )
	{
		return;
	}
#endif
	if( E._Object->GetLinker()!=this )
	{
		UObject* Object = E._Object;
		debugf(TEXT("Object            : %s"), *Object->GetFullName() );
		debugf(TEXT("Object Linker     : %s"), *Object->GetLinker()->GetFullName() );
		debugf(TEXT("Linker LinkerRoot : %s"), Object->GetLinker() ? *Object->GetLinker()->LinkerRoot->GetFullName() : TEXT("None") );
		debugf(TEXT("Detach Linker     : %s"), *GetFullName() );
		debugf(TEXT("Detach LinkerRoot : %s"), *LinkerRoot->GetFullName() );
		appErrorf( TEXT("Linker object %s %s.%s mislinked!"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString() );
	}
	if( E._Object->_LinkerIndex!=i )
	{
		appErrorf( TEXT("Linker object %s %s.%s misindexed!"), *GetExportClassName(i).ToString(), *LinkerRoot->GetName(), *E.ObjectName.ToString() );
	}
	ExportMap(i)._Object->SetLinker( NULL, INDEX_NONE );
}

/**
 * Detaches linker from bulk data/ exports and removes itself from array of loaders.
 *
 * @param	bEnsureAllBulkDataIsLoaded	Whether to load all bulk data first before detaching.
 */
void ULinkerLoad::Detach( UBOOL bEnsureAllBulkDataIsLoaded )
{
	// Detach all lazy loaders.
	DetachAllBulkData( bEnsureAllBulkDataIsLoaded );

	// Detach all objects linked with this linker.
	for( INT i=0; i<ExportMap.Num(); i++ )
	{	
		if( ExportMap(i)._Object )
		{
			DetachExport( i );
		}
	}

	// Remove from object manager, if it has been added.
	GObjLoaders.RemoveItem( this );
	if( Loader )
	{
		delete Loader;
	}
	Loader = NULL;

#if SUPPORTS_SCRIPTPATCH_LOADING
	//@script patcher
	if ( PatchDataAr != NULL )
	{
		delete PatchDataAr;
	}
	PatchDataAr = NULL;
#endif

	// Empty out no longer used arrays.
	NameMap.Empty();
	ImportMap.Empty();
	ExportMap.Empty();

	//@script patcher
	FunctionsToPatch.Empty();
	DefaultsToPatch.Empty();
	EnumsToPatch.Empty();

	// Make sure we're never associated with LinkerRoot again.
	LinkerRoot = NULL;
}

void ULinkerLoad::BeginDestroy()
{
	// Detaches linker.
	Detach( FALSE );
	Super::BeginDestroy();
}

/**
 * Attaches/ associates the passed in bulk data object with the linker.
 *
 * @param	Owner		UObject owning the bulk data
 * @param	BulkData	Bulk data object to associate
 */
void ULinkerLoad::AttachBulkData( UObject* Owner, FUntypedBulkData* BulkData )
{
	check( BulkDataLoaders.FindItemIndex(BulkData)==INDEX_NONE );
	BulkDataLoaders.AddItem( BulkData );
}

/**
 * Detaches the passed in bulk data object from the linker.
 *
 * @param	BulkData	Bulk data object to detach
 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
 */
void ULinkerLoad::DetachBulkData( FUntypedBulkData* BulkData, UBOOL bEnsureBulkDataIsLoaded )
{
	INT RemovedCount = BulkDataLoaders.RemoveItem( BulkData );
	if( RemovedCount!=1 )
	{	
		appErrorf( TEXT("Detachment inconsistency: %i (%s)"), RemovedCount, *Filename );
	}
	BulkData->DetachFromArchive( this, bEnsureBulkDataIsLoaded );
}

/**
 * Detaches all attached bulk  data objects.
 *
 * @param	bEnsureBulkDataIsLoaded	Whether to ensure that the bulk data is loaded before detaching
 */
void ULinkerLoad::DetachAllBulkData( UBOOL bEnsureAllBulkDataIsLoaded )
{
	for( INT BulkDataIndex=0; BulkDataIndex<BulkDataLoaders.Num(); BulkDataIndex++ )
	{
		FUntypedBulkData* BulkData = BulkDataLoaders(BulkDataIndex);
		check( BulkData );
		BulkData->DetachFromArchive( this, bEnsureAllBulkDataIsLoaded );
	}
	BulkDataLoaders.Empty();
}

/**
 * Hint the archive that the region starting at passed in offset and spanning the passed in size
 * is going to be read soon and should be precached.
 *
 * The function returns whether the precache operation has completed or not which is an important
 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
 * implement this function or only partially precache so it is required that given sufficient time
 * the function will return TRUE. Archives not based on async I/O should always return TRUE.
 *
 * This function will not change the current archive position.
 *
 * @param	PrecacheOffset	Offset at which to begin precaching.
 * @param	PrecacheSize	Number of bytes to precache
 * @return	FALSE if precache operation is still pending, TRUE otherwise
 */
UBOOL ULinkerLoad::Precache( INT PrecacheOffset, INT PrecacheSize )
{
	return Loader->Precache( PrecacheOffset, PrecacheSize );
}

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

FArchive& ULinkerLoad::operator<<( UObject*& Object )
{
	INT Index;
	FArchive& Ar = *this;
	Ar << Index;

	UObject* Temporary = NULL;
	UBOOL bUseNormalIndexToObject = TRUE;

	// is this object reference a potential cross level reference? if so, handle is specially
	if (PotentialCrossLevelOwner)
	{
		DWORD DwordIndex = (DWORD&)Index;
		if ((DwordIndex & 0xFF000000) == 0xF0000000)
		{
			// if we saw the magic in the first bit, never use the IndexToObject at the bottom of this function, it will fail
			bUseNormalIndexToObject = FALSE;

			// this is a special cross level reference, so look up in the ImportGuids for the Guid of the object
			// format is 0xF0LLGGGG, where LL is level index, GGGG is Guid index
			INT LevelIndex = (DwordIndex & 0xFF0000) >> 16;
			INT GuidIndex = DwordIndex & 0xFFFF;

			// look up the object in the other level
			Temporary = ResolveCrossLevelReference(LevelIndex, GuidIndex, PotentialCrossLevelOwner, PotentialCrossLevelProperty);
		}

		// reset the pointeray
		PotentialCrossLevelOwner = NULL;
		PotentialCrossLevelProperty = NULL;
	}

	// if the cross-level stuff didn't run, then lookup the object the old fashioned way
	if (bUseNormalIndexToObject)
	{
		Temporary = IndexToObject( Index );
	}

	appMemcpy(&Object, &Temporary, sizeof(UObject*));
	return *this;
}

/**
 * Resolves a cross level reference (as specified by the serialized LevelIndex and GuidIndex pair)
 * into a UObject. If it fails, then the destination level probably hasn't been loaded yet, the
 * the reference will need to be filled in later, when the level has been loaded
 *
 * @param LevelIndex Index of level in the package/linker's ImportGuids array
 * @param GuidIndex Index of level in the level's Guids array (in ImportGuids(LevelIndex))
 * @param PropertyOwner The object that owns the property/pointer that is being serialized, not the destination object
 * @param Property The property that describes the data being serialized
 *
 * @return Object in another level, or NULL if invalid input or the other level isn't loaded yet
 */
UObject* ULinkerLoad::ResolveCrossLevelReference(INT LevelIndex, INT GuidIndex, UObject* PropertyOwner, const UProperty* Property)
{
	UObject* Obj = NULL;

	// validate the inputs
	if (LevelIndex >= LinkerRoot->ImportGuids.Num())
	{
		debugf(NAME_Warning, TEXT("Resolving cross level reference with invalid LevelIndex [%d/%d]"), LevelIndex, LinkerRoot->ImportGuids.Num());
		return NULL;	
	}

	FLevelGuids& LevelGuids = LinkerRoot->ImportGuids(LevelIndex);

	if (LevelIndex >= LinkerRoot->ImportGuids.Num())
	{
		debugf(NAME_Warning, TEXT("Resolving cross level reference with invalid GuidIndex [%d/%d]"), GuidIndex, LevelGuids.Guids.Num());
		return NULL;	
	}

	FGuid& ObjectGuid = LevelGuids.Guids(GuidIndex);
	check(ObjectGuid.IsValid());

	UPackage* LinkerRoot = FindObject<UPackage>(NULL, *LevelGuids.LevelName.ToString());
	if (LinkerRoot)
	{
		// look in the package's ExportGuids table
		Obj = LinkerRoot->ExportGuids.FindRef(ObjectGuid);
		if (!Obj)
		{
			// if not found, it may need looking up in the linker (if it's linker is still being loaded)
			// now find the object based on the Guid, in the target levels 
			for (INT LinkerIndex = 0; LinkerIndex < GObjLoaders.Num(); LinkerIndex++)
			{
				if (GObjLoaders(LinkerIndex)->LinkerRoot->GetFName() == LevelGuids.LevelName)
				{
					// if the object is NULL in the ExportGuids, it may not have been looked up yet
					INT ExportIndex = 0;
					GObjLoaders(LinkerIndex)->ExportGuidsAwaitingLookup.RemoveAndCopyValue(ObjectGuid, ExportIndex);
					// if the other map doesn't have this Guid, then it truly doesn't exist
					if (ExportIndex == 0)
					{
						debugf(NAME_DevCrossLevel, TEXT("Guid %s doesn't exist in target level %s"), *ObjectGuid.String(), *LevelGuids.LevelName.ToString());
						return NULL;
					}

					// look it up 
					Obj = GObjLoaders(LinkerIndex)->ExportMap(ExportIndex - 1)._Object;

					// cache it if it's been loaded yet
					if (Obj != NULL)
					{
						LinkerRoot->ExportGuids.Set(ObjectGuid, Obj);

#if !CONSOLE
						// the editor needs to keep track of objects and their guids for saving packages
						// so, for every cross level export object that is loaded, remember its Guid
						if (GIsEditor)
						{
							GCrossLevelReferenceManager->CrossLevelObjectToGuidMap.Set(Obj, ObjectGuid);
						}
#endif

						// only allow delayed fixups/teardowns for properties that are active, or passive properties in the editor
						if (!GIsGame || (PotentialCrossLevelProperty->PropertyFlags & CPF_CrossLevelActive))
						{
							// remember this location, so if the object is unloaded, we can NULL it out
							GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap.Add(Obj, FDelayedCrossLevelRef(PropertyOwner, Property->Offset));

							// mark that the object has had an active cross level reference to it
							Obj->SetFlags(RF_IsCrossLevelReferenced);
						}

						// look for any objects that need to be fixed up for this object's guid
						// @todo: Move this into shared code, it's also called from LookupAllOutstandingCrossLevelExports
						TArray<FDelayedCrossLevelRef> ObjectsToFixup;
						GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.MultiFind(ObjectGuid, ObjectsToFixup);

						if (ObjectsToFixup.Num())
						{
							// now, for all references to this object (by the Guid), push the value into the memory location
							for (INT FixupIndex = 0; FixupIndex < ObjectsToFixup.Num(); FixupIndex++)
							{
								// stick the now-loaded object into the object property's memory for one object
								FDelayedCrossLevelRef& Fixup = ObjectsToFixup(FixupIndex);
								UObject** PtrLoc = (UObject**)((BYTE*)Fixup.Object + Fixup.Offset);
								*PtrLoc = Obj;

								// let the object handle the pointer update
								Fixup.Object->PostCrossLevelFixup();

								// remember this location, so if the object is unloaded, we can NULL it out
								GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap.Add(Obj, Fixup);

								// mark that the object has had a cross level reference to it
								Obj->SetFlags(RF_IsCrossLevelReferenced);
							}

							// remove all the entries we just filled out from the delayed fixup list
							GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.Remove(ObjectGuid);
						}
					}

					break;
				}
			}
		}
	}

	if (Obj == NULL)
	{
		debugf(NAME_DevCrossLevel, TEXT("Guid %s isn't loaded yet for cross level reference [%s:%s]"), *ObjectGuid.String(), *PropertyOwner->GetFullName(), *Property->GetName());

		// only allow delayed fixups for properties that are active, or passive properties in the editor
		if (!GIsGame || (PotentialCrossLevelProperty->PropertyFlags & CPF_CrossLevelActive))
		{
			// we failed to find the target object, highly likely the other level isn't loaded yet, so
			// we need to keep track of this location for the object to be stored and try to hook it up later
			GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.Add(ObjectGuid, FDelayedCrossLevelRef(PropertyOwner, Property->Offset));
		}
	}
	else
	{
		debugf(NAME_DevCrossLevel, TEXT("Got cross level ref pointing to %s"), *Obj->GetFullName());
	}

	// return whatever Obj is now, be it NULL (not found) or otherwise
	return Obj;
}


FArchive& ULinkerLoad::operator<<( FName& Name )
{
	NAME_INDEX NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		appErrorf( TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num() );
	}

	// @GEMINI_TODO: Are names ever not loaded because of NotForClient, etc? If not, remove this check
	// if the name wasn't loaded (because it wasn't valid in this context)
	if (NameMap(NameIndex) == NAME_None)
	{
		INT TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		INT Number;
		Ar << Number;
		// simply create the name from the NameMap's name index and the serialized instance number
		Name = FName((EName)NameMap(NameIndex).GetIndex(), Number);
	}

	return *this;
}

void ULinkerLoad::Serialize( void* V, INT Length )
{
	Loader->Serialize( V, Length );
}

/**
* Kick off an async load of a package file into memory
* 
* @param PackageName Name of package to read in. Must be the same name as passed into LoadPackage
*/
void ULinkerLoad::AsyncPreloadPackage(const TCHAR* PackageName)
{
	// get package filename
	FString PackageFilename;
	if (!GPackageFileCache->FindPackageFile(PackageName, NULL, PackageFilename))
	{
		appErrorf(TEXT("Failed to find file for package %s for async preloading."), PackageName);
	}

	// make sure it wasn't already there
	check(PackagePrecacheMap.Find(*PackageFilename) == NULL);

	// add a new one to the map
	FPackagePrecacheInfo& PrecacheInfo = PackagePrecacheMap.Set(*PackageFilename, FPackagePrecacheInfo());

	// make a new sync object (on heap so the precache info can be copied in the map, etc)
	PrecacheInfo.SynchronizationObject = new FThreadSafeCounter;

	// increment the sync object, later we'll wait for it to be decremented
	PrecacheInfo.SynchronizationObject->Increment();
	
	// request generic async IO system.
	FIOSystem* IO = GIOManager->GetIOSystem(IOSYSTEM_GenericAsync); 

	// default to not compressed
	UBOOL bWasCompressed = FALSE;

	// get filesize (first checking if it was compressed)
	const INT UncompressedSize = GFileManager->UncompressedFileSize(*PackageFilename);;
	const INT FileSize = GFileManager->FileSize(*PackageFilename);

	// if we were compressed, the size we care about on the other end is the uncompressed size
	PrecacheInfo.PackageDataSize = UncompressedSize == -1 ? FileSize : UncompressedSize;
	
	// allocate enough space
	PrecacheInfo.PackageData = appMalloc(PrecacheInfo.PackageDataSize);

	QWORD RequestId;
	// kick off the async read (uncompressing if needed) of the whole file and make sure it worked
	if (UncompressedSize != -1)
	{
		PrecacheInfo.PackageDataSize = UncompressedSize;
		RequestId = IO->LoadCompressedData(
						PackageFilename, 
						0, 
						FileSize, 
						UncompressedSize, 
						PrecacheInfo.PackageData, 
						GBaseCompressionMethod, 
						PrecacheInfo.SynchronizationObject,
						AIOP_Normal);
	}
	else
	{
		PrecacheInfo.PackageDataSize = FileSize;
		RequestId = IO->LoadData(
						PackageFilename, 
						0, 
						PrecacheInfo.PackageDataSize, 
						PrecacheInfo.PackageData, 
						PrecacheInfo.SynchronizationObject, 
						AIOP_Normal);
	}

	// give a hint to the IO system that we are done with this file for now
	IO->HintDoneWithFile(PackageFilename);

	check(RequestId);
}

/**
 * Called when an object begins serializing property data using script serialization.
 */
void ULinkerLoad::MarkScriptSerializationStart( const UObject* Obj )
{
	if ( Obj != NULL && Obj->GetLinker() == this && ExportMap.IsValidIndex(Obj->GetLinkerIndex()) )
	{
		FObjectExport& Export = ExportMap(Obj->GetLinkerIndex());
		Export.ScriptSerializationStartOffset = Tell();
	}
}

/**
 * Called when an object stops serializing property data using script serialization.
 */
void ULinkerLoad::MarkScriptSerializationEnd( const UObject* Obj )
{
	if ( Obj != NULL && Obj->GetLinker() == this && ExportMap.IsValidIndex(Obj->GetLinkerIndex()) )
	{
		FObjectExport& Export = ExportMap(Obj->GetLinkerIndex());
		Export.ScriptSerializationEndOffset = Tell();
	}
}

/*----------------------------------------------------------------------------
	ULinkerSave.
----------------------------------------------------------------------------*/

ULinkerSave::ULinkerSave( UPackage* InParent, const TCHAR* InFilename, UBOOL bForceByteSwapping )
:	ULinker( InParent, InFilename )
,	Saver( NULL )
,	bIsNextObjectSerializePotentialCrossLevelRef( FALSE )
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

#if !CONSOLE
	// Create file saver.
	Saver = GFileManager->CreateFileWriter( InFilename, 0, GThrow );
	if( !Saver )
	{
		appThrowf( *FString::Printf(LocalizeSecure(LocalizeError(TEXT("OpenFailed"),TEXT("Core")), InFilename, *GFileManager->GetCurrentDirectory())) );
	}

	// Set main summary info.
	Summary.Tag           = PACKAGE_FILE_TAG;
	Summary.SetFileVersions( GPackageFileVersion, GPackageFileLicenseeVersion );
	Summary.EngineVersion =	GEngineVersion;
	Summary.CookedContentVersion = GPackageFileCookedContentVersion;
	Summary.PackageFlags  = Cast<UPackage>(LinkerRoot) ? Cast<UPackage>(LinkerRoot)->PackageFlags : 0;

	UPackage *Package = Cast<UPackage>(LinkerRoot);
	if (Package)
	{
		Summary.FolderName = Package->GetFolderName().ToString();
	}

	// Set status info.
	ArIsSaving				= 1;
	ArIsPersistent			= 1;
	ArForEdit				= GIsEditor;
	ArForClient				= 1;
	ArForServer				= 1;
	ArForceByteSwapping		= bForceByteSwapping;
	ArIsFinalPackageSave	= TRUE;
	ArContainsCookedData	= (InParent->PackageFlags & PKG_Cooked) == PKG_Cooked;
	
	// Allocate indices.
	ObjectIndices.AddZeroed( UObject::GObjObjects.Num() );
	NameIndices  .AddZeroed( FName::GetMaxNames() );
	
#endif
}

ULinkerSave::ULinkerSave( UPackage* InParent, UBOOL bForceByteSwapping )
:	ULinker( InParent,TEXT("$$Memory$$") )
,	Saver( NULL )
,	bIsNextObjectSerializePotentialCrossLevelRef( FALSE )
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

#if !CONSOLE
	// Create file saver.
	Saver = new FBufferArchive();
	check(Saver);

	// Set main summary info.
	Summary.Tag           = PACKAGE_FILE_TAG;
	Summary.SetFileVersions( GPackageFileVersion, GPackageFileLicenseeVersion );
	Summary.EngineVersion =	GEngineVersion;
	Summary.CookedContentVersion = GPackageFileCookedContentVersion;
	Summary.PackageFlags  = Cast<UPackage>(LinkerRoot) ? Cast<UPackage>(LinkerRoot)->PackageFlags : 0;

	UPackage *Package = Cast<UPackage>(LinkerRoot);
	if (Package)
	{
		Summary.FolderName = Package->GetFolderName().ToString();
	}

	// Set status info.
	ArIsSaving				= 1;
	ArIsPersistent			= 1;
	ArForEdit				= GIsEditor;
	ArForClient				= 1;
	ArForServer				= 1;
	ArForceByteSwapping		= bForceByteSwapping;
	ArIsFinalPackageSave	= TRUE;
	ArContainsCookedData	= (InParent->PackageFlags & PKG_Cooked) == PKG_Cooked;

	// Allocate indices.
	ObjectIndices.AddZeroed( UObject::GObjObjects.Num() );
	NameIndices  .AddZeroed( FName::GetMaxNames() );

#endif
}
/**
 * Detaches file saver and hence file handle.
 */
void ULinkerSave::Detach()
{
	if( Saver )
	{
		delete Saver;
	}
	Saver = NULL;
}

void ULinkerSave::BeginDestroy()
{
	// Detach file saver/ handle.
	Detach();
	Super::BeginDestroy();
}

// FArchive interface.
INT ULinkerSave::MapName( const FName* Name ) const
{
	return NameIndices(Name->GetIndex());
}

INT ULinkerSave::MapObject( const UObject* Object ) const
{
	return Object != NULL 
		? ObjectIndices(Object->GetIndex())
		: 0;
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
	
FArchive& ULinkerSave::operator<<( UObject*& Obj )
{
#if !CONSOLE
	// only supporting cross level ref saving in the editor
	if (GIsEditor && bIsNextObjectSerializePotentialCrossLevelRef)
	{
		bIsNextObjectSerializePotentialCrossLevelRef = FALSE;

		const FGuid* ExternalGuid = NULL;
		FName LevelName = NAME_None;
		
		if (Obj)
		{
			// by the time we save the package, the CrossLevelObjectToGuidMap will have been filled out
			// with all objects, so we can find out if this is an actual cross level reference
			ExternalGuid = GCrossLevelReferenceManager->CrossLevelObjectToGuidMap.Find(Obj);

			// what level name we are going to search for in the ImportGuids
			LevelName = FName(*(GCrossLevelReferenceManager->GetPIEPrefix() + Obj->GetOutermost()->GetName()));
		}

		// if the external Guid is NULL, then the destination object's level may not have been loaded, in which case we
		// need to retain the reference to it
		if (!ExternalGuid && !Obj)
		{
			BYTE* ObjLocation = (BYTE*)&Obj;

			// look up the object location for delayed fixups, as that will have the associated guid
			for (TMultiMap<FGuid, FDelayedCrossLevelRef>::TIterator It(GCrossLevelReferenceManager->DelayedCrossLevelFixupMap); It; ++It)
			{
				FDelayedCrossLevelRef& Fixup = It.Value();
				// is this delayed fixup pointing to the location of this serialized property?
				if (((BYTE*)Fixup.Object + Fixup.Offset) == ObjLocation)
				{
					ExternalGuid = &It.Key();
					break;
				}
			}

			if (ExternalGuid)
			{
				debugf(NAME_DevCrossLevel, TEXT("Found delayed fixup for this object location, preserving cross-level reference..."));

				// we don't know what level this object is in, so look in all levels
				LevelName = NAME_None;
			}
		}

		if (ExternalGuid)
		{
			// the cross level reference is 8bits of magic (binary 11110000), followed by index into the FLevelGuid's Guids array
			DWORD CrossLevelReference = 0;

			// find the object's guid in the linker's ImportGuids table
			for (INT LevelIndex = 0; LevelIndex < LinkerRoot->ImportGuids.Num(); LevelIndex++)
			{
				FLevelGuids& LevelGuids = LinkerRoot->ImportGuids(LevelIndex);
				if (LevelName == NAME_None || LevelGuids.LevelName == LevelName)
				{
					// the object should already have been put into the ImportGuids array
					INT GuidIndex = LevelGuids.Guids.FindItemIndex(*ExternalGuid);
					if (GuidIndex != INDEX_NONE)
					{
						// the cross level reference is 8bits of magic (binary 11110000), followed by index into the FLevelGuid's Guids array
						CrossLevelReference = 0xF0000000 | (LevelIndex << 16) | GuidIndex;

						debugf(NAME_DevCrossLevel, TEXT("Saving cross level reference %x [%s / %s]"), CrossLevelReference, *LevelName.ToString(), *ExternalGuid->String());

						return (*this) << CrossLevelReference;
					}
				}
			}

			// it's possible to get to this if Obj was at some point marked as a cross level object (ie, 
			// we were in a sublevel and pointed to Obj, but then we switched to the same level as Obj),
			// the above code would find Obj in CrossLevelObjectToGuidMap, but then it won't be
			// in this package's ImportGuids arra. So, if that happens, and the Obj is in the package
			// we are saving, just return the object as normal
			if (Obj && LinkerRoot == Obj->GetOutermost())
			{
				// do nothing in this case and let it fall out to the actual serialization based 
				// on ObjectIndices at the end of this function
			}
			else
			{
				warnf(TEXT("Trying to serialize a crosslevel pointer to Obj, but it wasn't found in the ImportGuids anywhere"), Obj ? *Obj->GetFullName() : TEXT("NULL"));
				INT Null = 0;
				return *this << Null;
			}
		}
	}
#endif

	INT Save = Obj ? ObjectIndices(Obj->GetIndex()) : 0;
	FArchive& Ar = *this;
	return Ar << Save;
}

/**
 * Locates the class adjusted index and its package adjusted index for a given class name in the import map
 */
UBOOL ULinkerLoad::FindImportClassAndPackage( FName ClassName, PACKAGE_INDEX &ClassIdx, PACKAGE_INDEX &PackageIdx )
{
	for ( INT ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++ )
	{
		if ( ImportMap(ImportMapIdx).ObjectName == ClassName && ImportMap(ImportMapIdx).ClassName == NAME_Class )
		{
			ClassIdx = -(ImportMapIdx + 1);
			PackageIdx = ImportMap(ImportMapIdx).OuterIndex;
			return TRUE;
		}
	}

	return FALSE;
}

/**
* Attempts to find the index for the given class object in the import list and adds it + its package if it does not exist
*/
UBOOL ULinkerLoad::CreateImportClassAndPackage( FName ClassName, FName PackageName, PACKAGE_INDEX &ClassIdx, PACKAGE_INDEX &PackageIdx )
{
	//look for an existing import first
	//might as well look for the package at the same time ...
	UBOOL bPackageFound = FALSE;		
	for ( INT ImportMapIdx = 0; ImportMapIdx < ImportMap.Num(); ImportMapIdx++ )
	{
		//save one iteration by checking for the package in this loop
		if( PackageName != NAME_None && ImportMap(ImportMapIdx).ClassName == NAME_Package && ImportMap(ImportMapIdx).ObjectName == PackageName )
		{
			bPackageFound = TRUE;
			PackageIdx = -(ImportMapIdx + 1);
		}
		if ( ImportMap(ImportMapIdx).ObjectName == ClassName && ImportMap(ImportMapIdx).ClassName == NAME_Class )
		{
			ClassIdx = -(ImportMapIdx + 1);
			PackageIdx = ImportMap(ImportMapIdx).OuterIndex;
			return TRUE;
		}
	}

	//an existing import couldn't be found, so add it
	//first add the needed package if it didn't already exist in the import map
	if( !bPackageFound )
	{
		ImportMap.Add();
		ImportMap(ImportMap.Num()-1).ClassName = NAME_Package;
		ImportMap(ImportMap.Num()-1).ClassPackage = NAME_Core;
		ImportMap(ImportMap.Num()-1).ObjectName = PackageName;
		ImportMap(ImportMap.Num()-1).OuterIndex = 0;
		ImportMap(ImportMap.Num()-1).XObject = 0;
		ImportMap(ImportMap.Num()-1).SourceLinker = 0;
		ImportMap(ImportMap.Num()-1).SourceIndex = -1;
		PackageIdx = -ImportMap.Num();
	}

	//now add the class import
	ImportMap.Add();
	ImportMap(ImportMap.Num()-1).ClassName = NAME_Class;
	ImportMap(ImportMap.Num()-1).ClassPackage = NAME_Core;
	ImportMap(ImportMap.Num()-1).ObjectName = ClassName;
	ImportMap(ImportMap.Num()-1).OuterIndex = PackageIdx;
	ImportMap(ImportMap.Num()-1).XObject = 0;
	ImportMap(ImportMap.Num()-1).SourceLinker = 0;
	ImportMap(ImportMap.Num()-1).SourceIndex = -1;
	ClassIdx = -ImportMap.Num();

	return TRUE;
}

/**
* Allows object instances to be converted to other classes upon loading a package
*/
UBOOL ULinkerLoad::FixupExportMap()
{
#if !FINAL_RELEASE
	// seekfree loading implies cooked packages which means they have already been through this logic
	if (bFixupExportMapDone || (LinkerRoot->PackageFlags & PKG_Cooked) || (ObjectNameRedirectsInstanceOnly.Num() == 0 && ObjectNameRedirectsObjectOnly.Num() == 0))
	{
		return TRUE;
	}

	FString RootName = LinkerRoot->GetName();
	for ( INT ExportMapIdx = 0; ExportMapIdx < ExportMap.Num(); ExportMapIdx++ )
	{
		FObjectExport &Export = ExportMap(ExportMapIdx);

		FName NameClass = GetExportClassName(ExportMapIdx);
		FName NamePackage = GetExportClassPackage(ExportMapIdx);

		FName *RedirectName = ObjectNameRedirectsInstanceOnly.Find(NameClass);
		if ( RedirectName )
		{
			FString StrRedirectName, ResultPackage, ResultClass;
			FString StrObjectName = Export.ObjectName.ToString();

			StrRedirectName = RedirectName->ToString();

			// Accepts either "PackageName.ClassName" or just "ClassName"
			INT Offset = StrRedirectName.InStr(TEXT("."));
			if ( Offset >= 0 )
			{
				// A package class name redirect
				ResultPackage = StrRedirectName.Left(Offset);
				ResultClass = StrRedirectName.Right(StrRedirectName.Len() - Offset - 1);
			}
			else
			{
				// Just a class name change within the same package
				ResultPackage = NamePackage.ToString();
				ResultClass = StrRedirectName;
			}

			// Never modify the default object instances
			if ( StrObjectName.Left(9) != FString("Default__") )
			{
				PACKAGE_INDEX NewClassIndex;
				PACKAGE_INDEX NewPackageIndex;
				if ( CreateImportClassAndPackage(*ResultClass, *ResultPackage, NewClassIndex, NewPackageIndex) )
				{
					Export.ClassIndex = NewClassIndex;
					//Export.OuterIndex = newPackageIndex;

					Logf(TEXT("ULinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> [Obj<%s> Cls<%s> ClsPkg<%s>]"), *LinkerRoot->GetName(),
						*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString(),
						*Export.ObjectName.ToString(), *ResultClass, *ResultPackage);
				}
				else
				{
					Logf(TEXT("ULinkerLoad::FixupExportMap() - object redirection failed at %s"), *Export.ObjectName.ToString());
				}
			}
		}
		else
		{
			//debugf(TEXT("Export: <%s>"), *(RootName + TEXT(".") + Export.ObjectName.ToString()));
			RedirectName = ObjectNameRedirectsObjectOnly.Find(*(RootName + TEXT(".") + *Export.ObjectName.ToString()));
			if ( RedirectName )
			{
				FString StrRedirectName, ResultPackage, ResultClass;
				FString StrObjectName = Export.ObjectName.ToString();

				StrRedirectName = RedirectName->ToString();

				// Accepts either "PackageName.ClassName" or just "ClassName"
				INT Offset = StrRedirectName.InStr(TEXT("."));
				if ( Offset >= 0 )
				{
					// A package class name redirect
					ResultPackage = StrRedirectName.Left(Offset);
					ResultClass = StrRedirectName.Right(StrRedirectName.Len() - Offset - 1);
				}
				else
				{
					ResultClass = StrRedirectName;
				}

				// Never modify the default object instances
				if ( StrObjectName.Left(9) != FString("Default__") )
				{
					PACKAGE_INDEX NewClassIndex;
					PACKAGE_INDEX NewPackageIndex;
					if ( CreateImportClassAndPackage(*ResultClass, *ResultPackage, NewClassIndex, NewPackageIndex) )
					{
						Export.ClassIndex = NewClassIndex;

						Logf(TEXT("ULinkerLoad::FixupExportMap() - Pkg<%s> [Obj<%s> Cls<%s> ClsPkg<%s>] -> [Obj<%s> Cls<%s> ClsPkg<%s>]"), *LinkerRoot->GetName(),
							*Export.ObjectName.ToString(), *NameClass.ToString(), *NamePackage.ToString(),
							*Export.ObjectName.ToString(), *ResultClass, *ResultPackage);
					}
					else
					{
						Logf(TEXT("ULinkerLoad::FixupExportMap() - object redirection failed at %s"), *Export.ObjectName.ToString());
					}
				}
			}
		}	
	}

	bFixupExportMapDone = TRUE;
	return !IsTimeLimitExceeded( TEXT("fixing up export map") );
#else
	return TRUE;
#endif // #if !FINAL_RELEASE
}




IMPLEMENT_CLASS(ULinker);
IMPLEMENT_CLASS(ULinkerLoad);
IMPLEMENT_CLASS(ULinkerSave);


