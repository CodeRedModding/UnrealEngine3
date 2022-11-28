/*=============================================================================
	UnLinker.h: Unreal object linker.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

//
// Information about an exported object.
//
struct FObjectExport
{
	// Variables.
	INT         ClassIndex;		// Persistent.
	INT         SuperIndex;		// Persistent (for UStruct-derived objects only).
	INT			PackageIndex;	// Persistent.
	FName		ObjectName;		// Persistent.
	DWORD		ObjectFlags;	// Persistent.
	INT         SerialSize;		// Persistent.
	INT         SerialOffset;	// Persistent (for checking only).
	UObject*	_Object;		// Internal.
	INT			_iHashNext;		// Internal.

	TMap<FName,INT>	ComponentMap; // Persistent.

	// Functions.
	FObjectExport();
	FObjectExport( UObject* InObject );
	
	friend FArchive& operator<<( FArchive& Ar, FObjectExport& E );
};

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

//
// Information about an imported object.
//
struct FObjectImport
{
	// Variables.
	FName			ClassPackage;	// Persistent.
	FName			ClassName;		// Persistent.
	INT				PackageIndex;	// Persistent.
	FName			ObjectName;		// Persistent.
	UObject*		XObject;		// Internal (only really needed for saving, can easily be gotten rid of for loading).
	ULinkerLoad*	SourceLinker;	// Internal.
	INT             SourceIndex;	// Internal.

	// Functions.
	FObjectImport();
	FObjectImport( UObject* InObject );
	
	friend FArchive& operator<<( FArchive& Ar, FObjectImport& I );
};

/*----------------------------------------------------------------------------
	Items stored in Unrealfiles.
----------------------------------------------------------------------------*/

//
// Unrealfile summary, stored at top of file.
//
struct FGenerationInfo
{
	INT ExportCount, NameCount;
	FGenerationInfo( INT InExportCount, INT InNameCount );
	friend FArchive& operator<<( FArchive& Ar, FGenerationInfo& Info )
	{
		return Ar << Info.ExportCount << Info.NameCount;
	}
};

struct FPackageFileSummary
{
	// Variables.
	INT		Tag;
protected:
	INT		FileVersion;
public:
	DWORD	PackageFlags;
	INT		NameCount,		NameOffset;
	INT		ExportCount,	ExportOffset;
	INT     ImportCount,	ImportOffset;
	FGuid	Guid;
	TArray<FGenerationInfo> Generations;

	// Constructor.
	FPackageFileSummary();
	INT GetFileVersion() const { return (FileVersion & 0xffff); }
	INT GetFileVersionLicensee() const { return ((FileVersion >> 16) & 0xffff); }
	void SetFileVersions(INT Epic, INT Licensee) { if( GSys->LicenseeMode == 0 ) FileVersion = Epic; else FileVersion = ((Licensee << 16) | Epic); }
	friend FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum );
};

/*----------------------------------------------------------------------------
	ULinker.
----------------------------------------------------------------------------*/

//
// A file linker.
//
class ULinker : public UObject
{
	DECLARE_CLASS(ULinker,UObject,CLASS_Transient,Core)
	NO_DEFAULT_CONSTRUCTOR(ULinker)

	// Variables.
	UObject*				LinkerRoot;			// The linker's root object.
	FPackageFileSummary		Summary;			// File summary.
	TArray<FName>			NameMap;			// Maps file name indices to name table indices.
	TArray<FObjectImport>	ImportMap;			// Maps file object indices >=0 to external object names.
	TArray<FObjectExport>	ExportMap;			// Maps file object indices >=0 to external object names.
	INT						Success;			// Whether the object was constructed successfully.
	FString					Filename;			// Filename.
	DWORD					_ContextFlags;		// Load flag mask.

	// Constructors.
	ULinker( UObject* InRoot, const TCHAR* InFilename );
	void Serialize( FArchive& Ar );
	FString GetImportFullName( INT i );
	FString GetExportFullName( INT i, const TCHAR* FakeRoot=NULL );
};

/*----------------------------------------------------------------------------
	ULinkerLoad.
----------------------------------------------------------------------------*/

//
// A file loader.
//
class ULinkerLoad : public ULinker, public FArchive
{
	DECLARE_CLASS(ULinkerLoad,ULinker,CLASS_Transient,Core)
	NO_DEFAULT_CONSTRUCTOR(ULinkerLoad)

	// Friends.
	friend class UObject;
	friend class UPackageMap;

	// Variables.
	DWORD					LoadFlags;
	UBOOL					Verified;
	INT						ExportHash[256];
	TArray<FLazyLoader*>	LazyLoaders;
	FArchive*				Loader;

	ULinkerLoad( UObject* InParent, const TCHAR* InFilename, DWORD InLoadFlags );

	void Verify();
	FName GetExportClassPackage( INT i );
	FName GetExportClassName( INT i );
	void VerifyImport( INT i );
	void LoadAllObjects();
	INT FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, INT PackageIndex );
	UObject* Create( UClass* ObjectClass, FName ObjectName, DWORD LoadFlags, UBOOL Checked );
	void Preload( UObject* Object );

private:
	UObject* CreateExport( INT Index );
	UObject* CreateImport( INT Index );

	UObject* IndexToObject( INT Index );
	void DetachExport( INT i );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();

	// FArchive interface.
	void AttachLazyLoader( FLazyLoader* LazyLoader );
	void DetachLazyLoader( FLazyLoader* LazyLoader );
	void DetachAllLazyLoaders( UBOOL Load );
	void Seek( INT InPos );
	INT Tell();
	INT TotalSize();
	void Serialize( void* V, INT Length );
	FArchive& operator<<( UObject*& Object )
	{
		INT Index;
		*Loader << Index;
		UObject* Temporary = IndexToObject( Index );
		appMemcpy(&Object, &Temporary, sizeof(UObject*));

		return *this;
	}

	FArchive& operator<<( FName& Name )
	{
		NAME_INDEX NameIndex;
		*Loader << NameIndex;

		if( !NameMap.IsValidIndex(NameIndex) )
			appErrorf( TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num() );
		FName Temporary = NameMap( NameIndex );
		appMemcpy(&Name, &Temporary, sizeof(FName));

		return *this;
	}
};

/*----------------------------------------------------------------------------
	ULinkerSave.
----------------------------------------------------------------------------*/

//
// A file saver.
//
class ULinkerSave : public ULinker, public FArchive
{
	DECLARE_CLASS(ULinkerSave,ULinker,CLASS_Transient,Core);
	NO_DEFAULT_CONSTRUCTOR(ULinkerSave);

	// Variables.
	FArchive* Saver;
	TArray<INT> ObjectIndices;
	TArray<INT> NameIndices;

	// Constructor.
	ULinkerSave( UObject* InParent, const TCHAR* InFilename );
	void Destroy();

	// FArchive interface.
	INT MapName( FName* Name );
	INT MapObject( UObject* Object );
	FArchive& operator<<( FName& Name )
	{
		INT Save = NameIndices(Name.GetIndex());
		FArchive& Ar = *this;
		return Ar << Save;
	}
	FArchive& operator<<( UObject*& Obj )
	{
		INT Save = Obj ? ObjectIndices(Obj->GetIndex()) : 0;
		FArchive& Ar = *this;
		return Ar << Save;
	}
	void Seek( INT InPos );
	INT Tell();
	void Serialize( void* V, INT Length );
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

