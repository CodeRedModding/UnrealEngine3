/*=============================================================================
	UnObjBas.h: Unreal object base class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// This is a hack, but MUST be defined for 64-bit platforms right now! --ryan.
#ifdef __x86_64__
#define SERIAL_POINTER_INDEX 1
#endif

#if SERIAL_POINTER_INDEX
#define MAX_SERIALIZED_POINTERS (1024 * 64)
extern "C" {
    extern void *GSerializedPointers[MAX_SERIALIZED_POINTERS];
    extern INT GTotalSerializedPointers;
    INT SerialPointerIndex(void *ptr);
}
#endif

/*-----------------------------------------------------------------------------
	Core enumerations.
-----------------------------------------------------------------------------*/

//
// Flags for loading objects.
//
enum ELoadFlags
{
	LOAD_None			= 0x0000,	// No flags.
	LOAD_NoFail			= 0x0001,	// Critical error if load fails.
	LOAD_NoWarn			= 0x0002,	// Don't display warning if load fails.
	LOAD_Throw			= 0x0008,	// Throw exceptions upon failure.
	LOAD_Verify			= 0x0010,	// Only verify existance; don't actually load.
	LOAD_AllowDll		= 0x0020,	// Allow plain DLLs.
	LOAD_DisallowFiles  = 0x0040,	// Don't load from file.
	LOAD_NoVerify       = 0x0080,   // Don't verify imports yet.
	LOAD_Quiet			= 0x2000,   // No log warnings.
	LOAD_FindIfFail		= 0x4000,	// Tries FindObject if a linker cannot be obtained (e.g. package is currently being compiled)
	LOAD_Propagate      = 0,
};

//
// Package flags.
//
enum EPackageFlags
{
	PKG_AllowDownload	= 0x0001,	// Allow downloading package.
	PKG_ClientOptional  = 0x0002,	// Purely optional for clients.
	PKG_ServerSideOnly  = 0x0004,   // Only needed on the server side.
	PKG_Cooked			= 0x0008,	// Whether this package has been cooked for the target platform.
	PKG_Unsecure        = 0x0010,   // Not trusted.
	PKG_Need			= 0x8000,	// Client needs to download this package.
};

//
// Internal enums.
//
enum ENativeConstructor    {EC_NativeConstructor};
enum EStaticConstructor    {EC_StaticConstructor};
enum EInternal             {EC_Internal};
enum ECppProperty          {EC_CppProperty};
enum EInPlaceConstructor   {EC_InPlaceConstructor};

//
// Result of GotoState.
//
enum EGotoState
{
	GOTOSTATE_NotFound		= 0,
	GOTOSTATE_Success		= 1,
	GOTOSTATE_Preempted		= 2,
};

//
// Flags describing a class.
//
enum EClassFlags
{
	// Base flags.
	CLASS_None				= 0x00000000,
	CLASS_Abstract          = 0x00000001,	// Class is abstract and can't be instantiated directly.
	CLASS_Compiled			= 0x00000002,	// Script has been compiled successfully.
	CLASS_Config			= 0x00000004,	// Load object configuration at construction time.
	CLASS_Transient			= 0x00000008,	// This object type can't be saved; null it out at save time.
	CLASS_Parsed            = 0x00000010,	// Successfully parsed.
	CLASS_Localized         = 0x00000020,	// Class contains localized text.
	CLASS_SafeReplace       = 0x00000040,	// Objects of this class can be safely replaced with default or NULL.
	CLASS_RuntimeStatic     = 0x00000080,	// Objects of this class are static during gameplay.
	CLASS_NoExport          = 0x00000100,	// Don't export to C++ header.
	CLASS_Placeable         = 0x00000200,	// Allow users to create in the editor.
	CLASS_PerObjectConfig   = 0x00000400,	// Handle object configuration on a per-object basis, rather than per-class.
	CLASS_NativeReplication = 0x00000800,	// Replication handled in C++.
	CLASS_EditInlineNew		= 0x00001000,	// Class can be constructed from editinline New button.
	CLASS_CollapseCategories= 0x00002000,	// Display properties in the editor without using categories.
    CLASS_IsAUProperty      = 0x00008000,	// IsA UProperty
    CLASS_IsAUObjectProperty= 0x00010000,	// IsA UObjectProperty
    CLASS_IsAUBoolProperty  = 0x00020000,	// IsA UBoolProperty
    CLASS_IsAUState         = 0x00040000,	// IsA UState
    CLASS_IsAUFunction      = 0x00080000,	// IsA UFunction

	CLASS_NeedsDefProps		= 0x00100000,	// Class needs its defaultproperties imported
	CLASS_HasComponents		= 0x00400000,	// Class has component properties.

	CLASS_Hidden			= 0x00800000,	// Don't show this class in the editor class browser or edit inline new menus.
	CLASS_Deprecated		= 0x01000000,	// Don't save objects of this class when serializing
	CLASS_HideDropDown		= 0x02000000,	// Class not shown in editor drop down for class selection

	CLASS_Exported			= 0x04000000,	// Class has been exported to a header file
		

	// Flags to inherit from base class.
	CLASS_Inherit           = CLASS_Transient | CLASS_Config | CLASS_Localized | CLASS_SafeReplace | CLASS_RuntimeStatic | CLASS_PerObjectConfig | CLASS_Placeable | CLASS_IsAUProperty | CLASS_IsAUObjectProperty | CLASS_IsAUBoolProperty | CLASS_IsAUState | CLASS_IsAUFunction | CLASS_HasComponents | CLASS_Deprecated,
	CLASS_RecompilerClear   = CLASS_Inherit | CLASS_Abstract | CLASS_NoExport | CLASS_NativeReplication,
	CLASS_ScriptInherit		= CLASS_Inherit | CLASS_EditInlineNew | CLASS_CollapseCategories,
};

//
// Flags associated with each property in a class, overriding the
// property's default behavior.
//

// For compilers that don't support 64 bit enums.
#define	CPF_Edit				0x0000000000000001		// Property is user-settable in the editor.
#define	CPF_Const				0x0000000000000002		// Actor's property always matches class's default actor property.
#define CPF_Input				0x0000000000000004		// Variable is writable by the input system.
#define CPF_ExportObject		0x0000000000000008		// Object can be exported with actor.
#define CPF_OptionalParm		0x0000000000000010		// Optional parameter (if CPF_Param is set).
#define CPF_Net					0x0000000000000020		// Property is relevant to network replication.
#define CPF_EditConstArray		0x0000000000000040		// Prevent adding/removing of items from dynamic a array in the editor.
#define CPF_Parm				0x0000000000000080		// Function/When call parameter.
#define CPF_OutParm				0x0000000000000100		// Value is copied out after function call.
#define CPF_SkipParm			0x0000000000000200		// Property is a short-circuitable evaluation function parm.
#define CPF_ReturnParm			0x0000000000000400		// Return value.
#define CPF_CoerceParm			0x0000000000000800		// Coerce args into this function parameter.
#define CPF_Native      		0x0000000000001000		// Property is native: C++ code is responsible for serializing it.
#define CPF_Transient   		0x0000000000002000		// Property is transient: shouldn't be saved, zero-filled at load time.
#define CPF_Config      		0x0000000000004000		// Property should be loaded/saved as permanent profile.
#define CPF_Localized   		0x0000000000008000		// Property should be loaded as localizable text.
#define CPF_Travel      		0x0000000000010000		// Property travels across levels/servers.
#define CPF_EditConst   		0x0000000000020000		// Property is uneditable in the editor.
#define CPF_GlobalConfig		0x0000000000040000		// Load config from base class, not subclass.
#define CPF_Component			0x0000000000080000		// Property containts component references.
#define CPF_NeedCtorLink		0x0000000000400000		// Fields need construction/destruction.
#define CPF_NoExport    		0x0000000000800000		// Property should not be exported to the native class header file.
#define CPF_NoClear				0x0000000002000000		// Hide clear (and browse) button.
#define CPF_EditInline			0x0000000004000000		// Edit this object reference inline.
#define CPF_EdFindable			0x0000000008000000		// References are set by clicking on actors in the editor viewports.
#define CPF_EditInlineUse		0x0000000010000000		// EditInline with Use button.
#define CPF_Deprecated  		0x0000000020000000		// Property is deprecated.  Read it from an archive, but don't save it.
#define CPF_EditInlineNotify	0x0000000040000000		// EditInline, notify outer object on editor change.
#define CPF_RepNotify			0x0000000100000000		// Notify actors when a property is replicated
#define CPF_Interp				0x0000000200000000		// interpolatable property for use with matinee
#define CPF_NonTransactional	0x0000000400000000		// Property isn't transacted

// Combinations of flags.
#define	CPF_ParmFlags				(CPF_OptionalParm | CPF_Parm | CPF_OutParm | CPF_SkipParm | CPF_ReturnParm | CPF_CoerceParm)
#define	CPF_PropagateFromStruct		(CPF_Const | CPF_Native | CPF_Transient)
#define	CPF_PropagateToArrayInner	(CPF_ExportObject | CPF_EditInline | CPF_EditInlineUse | CPF_EditInlineNotify | CPF_Localized | CPF_Component)

//
// Flags describing an object instance.
//
enum EObjectFlags
{
	RF_Transactional    = 0x00000001,   // Object is transactional.
	RF_Unreachable		= 0x00000002,	// Object is not reachable on the object graph.
	RF_Public			= 0x00000004,	// Object is visible outside its package.
	RF_TagImp			= 0x00000008,	// Temporary import tag in load/save.
	RF_TagExp			= 0x00000010,	// Temporary export tag in load/save.
	RF_Obsolete			= 0x00000020,   // Object marked as obsolete and should be replaced.
	RF_TagGarbage		= 0x00000040,	// Check during garbage collection.
	RF_Final			= 0x00000080,	// Object is not visible outside of class.
	RF_PerObjectLocalized=0x00000100,	// Object is localized by instance name, not by class.
	RF_NeedLoad			= 0x00000200,   // During load, indicates object needs loading.
	RF_HighlightedName  = 0x00000400,	// A hardcoded name which should be syntax-highlighted.
	RF_EliminateObject  = 0x00000400,   // NULL out references to this during garbage collecion.
	RF_InSingularFunc   = 0x00000800,	// In a singular function.
	RF_RemappedName     = 0x00000800,   // Name is remapped.
	RF_Suppress         = 0x00001000,	//warning: Mirrored in UnName.h. Suppressed log name.
	RF_StateChanged     = 0x00001000,   // Object did a state change.
	RF_InEndState       = 0x00002000,   // Within an EndState call.
	RF_Transient        = 0x00004000,	// Don't save object.
	RF_Preloading       = 0x00008000,   // Data is being preloaded from file.
	RF_LoadForClient	= 0x00010000,	// In-file load for client.
	RF_LoadForServer	= 0x00020000,	// In-file load for client.
	RF_LoadForEdit		= 0x00040000,	// In-file load for client.
	RF_Standalone       = 0x00080000,   // Keep object around for editing even if unreferenced.
	RF_NotForClient		= 0x00100000,	// Don't load this object for the game client.
	RF_NotForServer		= 0x00200000,	// Don't load this object for the game server.
	RF_NotForEdit		= 0x00400000,	// Don't load this object for the editor.
	RF_Destroyed        = 0x00800000,	// Object Destroy has already been called.
	RF_NeedPostLoad		= 0x01000000,   // Object needs to be postloaded.
	RF_HasStack         = 0x02000000,	// Has execution stack.
	RF_Native			= 0x04000000,   // Native (UClass only).
	RF_Marked			= 0x08000000,   // Marked (for debugging).
	RF_ErrorShutdown    = 0x10000000,	// ShutdownAfterError called.
	RF_DebugPostLoad    = 0x20000000,   // For debugging Serialize calls.
	RF_DebugSerialize   = 0x40000000,   // For debugging Serialize calls.
	RF_DebugDestroy     = 0x80000000,   // For debugging Destroy calls.
	RF_EdSelected		= 0x80000000,   // Object is selected in one of the editors browser windows.
	RF_ContextFlags		= RF_NotForClient | RF_NotForServer | RF_NotForEdit, // All context flags.
	RF_LoadContextFlags	= RF_LoadForClient | RF_LoadForServer | RF_LoadForEdit, // Flags affecting loading.
	RF_Load  			= RF_ContextFlags | RF_LoadContextFlags | RF_Public | RF_Final | RF_Standalone | RF_Native | RF_Obsolete | RF_Transactional | RF_HasStack | RF_PerObjectLocalized, // Flags to load from Unrealfiles.
	RF_Keep             = RF_Native | RF_Marked | RF_PerObjectLocalized, // Flags to persist across loads.
	RF_ScriptMask		= RF_Transactional | RF_Public | RF_Final | RF_Transient | RF_NotForClient | RF_NotForServer | RF_NotForEdit | RF_Standalone // Script-accessible flags.
};

//NEW: trace facility
enum ProcessEventType
{
	PE_Toggle,
	PE_Suppress,
	PE_Enable,
};

/*----------------------------------------------------------------------------
	Core types.
----------------------------------------------------------------------------*/

//
// Globally unique identifier.
//
class FGuid
{
public:
	DWORD A,B,C,D;
	FGuid()
	{}
	FGuid( DWORD InA, DWORD InB, DWORD InC, DWORD InD )
	: A(InA), B(InB), C(InC), D(InD)
	{}
	friend UBOOL operator==(const FGuid& X, const FGuid& Y)
		{return X.A==Y.A && X.B==Y.B && X.C==Y.C && X.D==Y.D;}
	friend UBOOL operator!=(const FGuid& X, const FGuid& Y)
		{return X.A!=Y.A || X.B!=Y.B || X.C!=Y.C || X.D!=Y.D;}
	friend FArchive& operator<<( FArchive& Ar, FGuid& G )
	{
		return Ar << G.A << G.B << G.C << G.D;
	}
	FString String() const
	{
		return FString::Printf( TEXT("%08X%08X%08X%08X"), A, B, C, D );
	}
	friend DWORD GetTypeHash(const FGuid& Guid)
	{
		return appMemCrc(&Guid,sizeof(FGuid));
	}
};
inline INT CompareGuids( FGuid* A, FGuid* B )
{
	INT Diff;
	Diff = A->A-B->A; if( Diff ) return Diff;
	Diff = A->B-B->B; if( Diff ) return Diff;
	Diff = A->C-B->C; if( Diff ) return Diff;
	Diff = A->D-B->D; if( Diff ) return Diff;
	return 0;
}

/*----------------------------------------------------------------------------
	Core macros.
----------------------------------------------------------------------------*/

// Special canonical package for FindObject, ParseObject.
#define ANY_PACKAGE ((UPackage*)-1)

// Define private default constructor.
#define NO_DEFAULT_CONSTRUCTOR(cls) \
	protected: cls() {} public:

// Verify the a class definition and C++ definition match up.
#define VERIFY_CLASS_OFFSET(Pre,ClassName,Member) \
    {for( TFieldIterator<UProperty,CLASS_IsAUProperty> It( FindObjectChecked<UClass>( Pre##ClassName::StaticClass()->GetOuter(), TEXT(#ClassName) ) ); It; ++It ) \
		if( appStricmp(It->GetName(),TEXT(#Member))==0 ) \
			if( It->Offset != STRUCT_OFFSET(Pre##ClassName,Member) ) \
				appErrorf(TEXT("Class %s Member %s problem: Script=%i C++=%i"), TEXT(#ClassName), TEXT(#Member), It->Offset, STRUCT_OFFSET(Pre##ClassName,Member) );}

// Verify that C++ and script code agree on the size of a class.
#define VERIFY_CLASS_SIZE(ClassName) \
	check(sizeof(ClassName)==Align(ClassName::StaticClass()->GetPropertiesSize(),ClassName::StaticClass()->GetMinAlignment()));

// Verify a class definition and C++ definition match up (don't assert).
#define VERIFY_CLASS_OFFSET_NODIE(Pre,ClassName,Member) \
{ \
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It( FindObjectChecked<UClass>( Pre##ClassName::StaticClass()->GetOuter(), TEXT(#ClassName) ) ); It; ++It ) \
	if( appStricmp(It->GetName(),TEXT(#Member))==0 ) { \
		if( It->Offset != STRUCT_OFFSET(Pre##ClassName,Member) ) { \
			debugf(TEXT("VERIFY: Class %s Member %s problem: Script=%i C++=%i\n"), TEXT(#ClassName), TEXT(#Member), It->Offset, STRUCT_OFFSET(Pre##ClassName,Member) ); \
			Mismatch = true; \
		} \
	} \
}

// Verify that C++ and script code agree on the size of a class (don't assert).
#define VERIFY_CLASS_SIZE_NODIE(ClassName) \
	if (sizeof(ClassName)!=Align(ClassName::StaticClass()->GetPropertiesSize(),ClassName::StaticClass()->GetMinAlignment())) { \
		debugf(TEXT("VERIFY: Class %s size problem; Script=%i C++=%i"), TEXT(#ClassName), (int) ClassName::StaticClass()->GetPropertiesSize(), sizeof(ClassName)); \
		Mismatch = true; \
	}


// Declare the base UObject class.

#define DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
public: \
	/* Identification */ \
	enum {StaticClassFlags=TStaticFlags}; \
	private: \
	static UClass* PrivateStaticClass; \
	public: \
	typedef TSuperClass Super;\
	typedef TClass ThisClass;\
	static UClass* GetPrivateStaticClass##TClass( TCHAR* Package ); \
	static void InitializePrivateStaticClass##TClass(); \
	static UClass* StaticClass() \
	{ \
		if (!PrivateStaticClass) \
		{ \
			PrivateStaticClass = GetPrivateStaticClass##TClass( TEXT(#TPackage) ); \
			InitializePrivateStaticClass##TClass(); \
		} \
		return PrivateStaticClass; \
	} \
	void* operator new( size_t Size, UObject* Outer=(UObject*)GetTransientPackage(), FName Name=NAME_None, DWORD SetFlags=0 ) \
		{ return StaticAllocateObject( StaticClass(), Outer, Name, SetFlags ); } \
	void* operator new( size_t Size, EInternal* Mem ) \
		{ return (void*)Mem; }

// Declare a concrete class.
#define DECLARE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
	DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
	friend FArchive &operator<<( FArchive& Ar, TClass*& Res ) \
		{ return Ar << *(UObject**)&Res; } \
	virtual ~TClass() \
		{ ConditionalDestroy(); } \
	static void InternalConstructor( void* X ) \
		{ new( (EInternal*)X )TClass; } \

// Declare an abstract class.
#define DECLARE_ABSTRACT_CLASS( TClass, TSuperClass, TStaticFlags, TPackage ) \
	DECLARE_BASE_CLASS( TClass, TSuperClass, TStaticFlags | CLASS_Abstract, TPackage ) \
	friend FArchive &operator<<( FArchive& Ar, TClass*& Res ) \
		{ return Ar << *(UObject**)&Res; } \
	virtual ~TClass() \
		{ ConditionalDestroy(); } \

// Declare that objects of class being defined reside within objects of the specified class.
#define DECLARE_WITHIN( TWithinClass ) \
	typedef TWithinClass WithinClass; \
	TWithinClass* GetOuter##TWithinClass() const { return (TWithinClass*)GetOuter(); }

// Register a class at startup time.

#define IMPLEMENT_CLASS(TClass) \
	UClass* TClass::PrivateStaticClass = NULL; \
	UClass* TClass::GetPrivateStaticClass##TClass( TCHAR* Package ) \
	{ \
		UClass* ReturnClass; \
		ReturnClass = ::new UClass \
		( \
			EC_StaticConstructor, \
			sizeof(TClass), \
			StaticClassFlags, \
			FGuid(GUID1,GUID2,GUID3,GUID4), \
			TEXT(#TClass)+1, \
			Package, \
			StaticConfigName(), \
			RF_Public | RF_Standalone | RF_Transient | RF_Native, \
			(void(*)(void*))TClass::InternalConstructor, \
			(void(UObject::*)())&TClass::StaticConstructor \
		); \
		check(ReturnClass); \
		return ReturnClass; \
	} \
	/* Called from ::StaticClass after GetPrivateStaticClass */ \
	void TClass::InitializePrivateStaticClass##TClass() \
	{ \
		/* No recursive ::StaticClass calls allowed. Setup extras. */ \
		if (TClass::Super::StaticClass() != TClass::PrivateStaticClass) \
			TClass::PrivateStaticClass->SuperField = TClass::Super::StaticClass(); \
		else \
			TClass::PrivateStaticClass->SuperField = NULL; \
		TClass::PrivateStaticClass->ClassWithin = TClass::WithinClass::StaticClass(); \
		TClass::PrivateStaticClass->SetClass(UClass::StaticClass()); \
		/* Perform UObject native registration. */ \
		if( TClass::PrivateStaticClass->GetInitialized() && TClass::PrivateStaticClass->GetClass()==TClass::PrivateStaticClass->StaticClass() ) \
			TClass::PrivateStaticClass->Register(); \
	}

#define IMPLEMENT_NESTED_CLASS(TClass,TClassName) \
	UClass* TClass::PrivateStaticClass = NULL; \
	UClass* TClass::GetPrivateStaticClass##TClassName( TCHAR* Package ) \
	{ \
		UClass* ReturnClass; \
		ReturnClass = ::new UClass \
		( \
			EC_StaticConstructor, \
			sizeof(TClass), \
			StaticClassFlags, \
			FGuid(GUID1,GUID2,GUID3,GUID4), \
			TEXT(#TClassName)+1, \
			Package, \
			StaticConfigName(), \
			RF_Public | RF_Standalone | RF_Transient | RF_Native, \
			(void(*)(void*))TClass::InternalConstructor, \
			(void(UObject::*)())&TClass::StaticConstructor \
		); \
		check(ReturnClass); \
		return ReturnClass; \
	} \
	/* Called from ::StaticClass after GetPrivateStaticClass */ \
	void TClass::InitializePrivateStaticClass##TClassName() \
	{ \
		/* No recursive ::StaticClass calls allowed. Setup extras. */ \
		if (TClass::Super::StaticClass() != TClass::PrivateStaticClass) \
			TClass::PrivateStaticClass->SuperField = TClass::Super::StaticClass(); \
		else \
			TClass::PrivateStaticClass->SuperField = NULL; \
		TClass::PrivateStaticClass->ClassWithin = TClass::WithinClass::StaticClass(); \
		TClass::PrivateStaticClass->SetClass(UClass::StaticClass()); \
		/* Perform UObject native registration. */ \
		if( TClass::PrivateStaticClass->GetInitialized() && TClass::PrivateStaticClass->GetClass()==TClass::PrivateStaticClass->StaticClass() ) \
			TClass::PrivateStaticClass->Register(); \
	}

/*-----------------------------------------------------------------------------
	FScriptDelegate.
-----------------------------------------------------------------------------*/
struct FScriptDelegate
{
	UObject* Object;
	FName FunctionName;

	friend FArchive& operator<<( FArchive& Ar, FScriptDelegate& D )
	{
		return Ar << D.Object << D.FunctionName;
	}
};

/*-----------------------------------------------------------------------------
	FThumbnailDesc.

	Used by the editor to gather information about thumbnail
	representations of resources.
-----------------------------------------------------------------------------*/

struct FThumbnailDesc
{
	FThumbnailDesc()
	{
		Width = Height = 0;
	}

	INT Width, Height;
};

/*-----------------------------------------------------------------------------
	EThumbnailPrimType.

	Types of primitives for drawing thumbnails of resources.
-----------------------------------------------------------------------------*/

enum EThumbnailPrimType
{
	TPT_None		= -1,
	TPT_Sphere,
	TPT_Cube,
	TPT_Plane,
	TPT_Cylinder,
};

/*-----------------------------------------------------------------------------
	Fpointer
-----------------------------------------------------------------------------*/

typedef void* Fpointer;

/*-----------------------------------------------------------------------------
	UObject.
-----------------------------------------------------------------------------*/

//
// The base class of all objects.
//
class UObject
{
	// Declarations.
	DECLARE_BASE_CLASS(UObject,UObject,CLASS_Abstract,Core)
	typedef UObject WithinClass;
	enum {GUID1=0,GUID2=0,GUID3=0,GUID4=0};
	static const TCHAR* StaticConfigName() {return TEXT("Engine");}

	// Friends.
	friend class FObjectIterator;
	friend class TSelectedObjectIterator;
	friend class ULinkerLoad;
	friend class ULinkerSave;
	friend class UPackageMap;
	friend class FArchiveTagUsed;
	friend struct FObjectImport;
	friend struct FObjectExport;

private:
	// Internal per-object variables.
	INT						Index;				// Index of object into table.
	UObject*				HashNext;			// Next object in this hash bin.
	FStateFrame*			StateFrame;			// Main script execution stack.
	ULinkerLoad*			_Linker;			// Linker it came from, or NULL if none.
	INT						_LinkerIndex;		// Index of this object in the linker's export map.
	UObject*				Outer;				// Object this object resides in.
	DWORD					ObjectFlags;		// Private EObjectFlags used by object manager.
	FName					Name;				// Name of the object.
	UClass*					Class;	  			// Class the object belongs to.

	// Private systemwide variables.
	static UBOOL			GObjInitialized;	// Whether initialized.
	static UBOOL            GObjNoRegister;		// Registration disable.
	static INT				GObjBeginLoadCount;	// Count for BeginLoad multiple loads.
	static INT				GObjRegisterCount;  // ProcessRegistrants entry counter.
	static INT				GImportCount;		// Imports for EndLoad optimization.
	static UObject*			GObjHash[4096];		// Object hash.
	static UObject*			GAutoRegister;		// Objects to automatically register.
	static TArray<UObject*> GObjLoaded;			// Objects that might need preloading.
	static TArray<UObject*>	GObjRoot;			// Top of active object graph.
	static TArray<UObject*>	GObjObjects;		// List of all objects.
	static TArray<INT>      GObjAvailable;		// Available object indices.
	static TArray<UObject*>	GObjLoaders;		// Array of loaders.
	static UPackage*		GObjTransientPkg;	// Transient package.
	static TCHAR			GObjCachedLanguage[32]; // Language;
	static TArray<UObject*> GObjRegistrants;		// Registrants during ProcessRegistrants call.
	static TCHAR GLanguage[64];

	// Private functions.
	void AddObject( INT Index );
	void HashObject();
	void UnhashObject( INT OuterIndex );
	void SetLinker( ULinkerLoad* L, INT I );

	// Private systemwide functions.
	static ULinkerLoad* GetLoader( INT i );
	static FName MakeUniqueObjectName( UObject* Outer, UClass* Class );
	static UBOOL ResolveName( UObject*& Outer, FString& Name, UBOOL Create, UBOOL Throw );
	static void SafeLoadError( UObject* Outer, DWORD LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ... );
	static void PurgeGarbage();
	
public:
	// Constructors.
	UObject();
	UObject( const UObject& Src );
	UObject( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags );
	UObject( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags );
	UObject( EInPlaceConstructor, UClass* InClass, UObject* InOuter, FName InName, DWORD InFlags );
	UObject& operator=( const UObject& );
	void StaticConstructor();
	static void InternalConstructor( void* X )
		{ new( (EInternal*)X )UObject; }

	// Destructors.
	virtual ~UObject();
	void operator delete( void* Object, size_t Size )
		{ appFree( Object ); }

	// UObject interface.
	virtual void ProcessEvent( UFunction* Function, void* Parms, void* Result=NULL );
	virtual void ProcessDelegate( FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* Result=NULL );
	virtual void ProcessState( FLOAT DeltaSeconds );
	virtual UBOOL ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack );
	virtual void Modify();
	virtual void PostLoad();
	/**
	 * Presave function. Gets called once before an object gets serialized for saving. This function is necessary
	 * for save time computation as Serialize gets called three times per object from within UObject::SavePackage.
	 */
	virtual void PreSave() {};
	virtual void Destroy();
	virtual void Serialize( FArchive& Ar );
	virtual UBOOL IsPendingKill() {return 0;}
	virtual EGotoState GotoState( FName State, UBOOL bForceEvents = 0 );
	virtual INT GotoLabel( FName Label );
	virtual void InitExecution();
	virtual void ShutdownAfterError();
	virtual void PreEditChange();
	virtual void PostEditChange(UProperty* PropertyThatChanged);
	virtual void PreEditUndo() {}
	virtual void PostEditUndo() {}
	virtual void PostRename() {}
	virtual void CallFunction( FFrame& TheStack, RESULT_DECL, UFunction* Function );
	virtual UBOOL ScriptConsoleExec( const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor );
	virtual void Register();
	virtual void LanguageChange();

	virtual UBOOL NeedsLoadForClient() const { return !(GetFlags() & RF_NotForClient); }
	virtual UBOOL NeedsLoadForServer() const { return !(GetFlags() & RF_NotForServer); }
	virtual UBOOL NeedsLoadForEdit() const { return !(GetFlags() & RF_NotForEdit); }

	//NEW: trace facility
	static void EventToggle( FName Event );
	static void EventEnable( FName Event, UBOOL bEnable );
	static void EventToggleAndLog( FName Event, FOutputDevice& Ar );
	static void EventEnableAndLog( FName Event, UBOOL bEnable, FOutputDevice& Ar );
	static void ProcessEventCommand( ProcessEventType PE, const TCHAR* EventList, FOutputDevice& Ar  );

	// Systemwide functions.
	static UObject* StaticFindObject( UClass* Class, UObject* InOuter, const TCHAR* Name, UBOOL ExactClass=0 );
	static UObject* StaticFindObjectChecked( UClass* Class, UObject* InOuter, const TCHAR* Name, UBOOL ExactClass=0 );
	static UObject* StaticLoadObject( UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox );
	static UClass* StaticLoadClass( UClass* BaseClass, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox );
	static UObject* StaticAllocateObject( UClass* Class, UObject* InOuter=(UObject*)GetTransientPackage(), FName Name=NAME_None, DWORD SetFlags=0, UObject* Template=NULL, FOutputDevice* Error=GError, UObject* Ptr=NULL, UObject* SuperObject=NULL );
	static UObject* StaticConstructObject( UClass* Class, UObject* InOuter=(UObject*)GetTransientPackage(), FName Name=NAME_None, DWORD SetFlags=0, UObject* Template=NULL, FOutputDevice* Error=GError, UObject* SuperObject=NULL );
	static UObject* StaticDuplicateObject(UObject* SourceObject,UObject* RootObject,UObject* DestOuter,const TCHAR* DestName,DWORD FlagMask = ~0);
	static void StaticInit();
	static void StaticExit();
	static UBOOL StaticExec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	static void StaticTick();
	static UObject* LoadPackage( UObject* InOuter, const TCHAR* Filename, DWORD LoadFlags );
	static UBOOL SavePackage( UObject* InOuter, UObject* Base, DWORD TopLevelFlags, const TCHAR* Filename, FOutputDevice* Error=GError, ULinkerLoad* Conform=NULL );
	static void CollectGarbage( DWORD KeepFlags );
	static void SerializeRootSet( FArchive& Ar, DWORD KeepFlags, DWORD RequiredFlags );
	static UBOOL IsReferenced( UObject*& Res, DWORD KeepFlags, UBOOL IgnoreReference );
	static UBOOL AttemptDelete( UObject*& Res, DWORD KeepFlags, UBOOL IgnoreReference );
	static void BeginLoad();
	static void EndLoad();
	static void InitProperties( BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* Defaults, INT DefaultsCount, UObject* DestObject=NULL, UObject* SuperObject=NULL );
	static void ExitProperties( BYTE* Data, UClass* Class );
	static void ResetLoaders( UObject* InOuter, UBOOL DynamicOnly, UBOOL ForceLazyLoad );
	static void LoadEverything();
	static UPackage* CreatePackage( UObject* InOuter, const TCHAR* PkgName );
	static ULinkerLoad* GetPackageLinker( UObject* InOuter, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox, FGuid* CompatibleGuid );
	static void StaticShutdownAfterError();
	static UObject* GetIndexedObject( INT Index );
	static void GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, INT Offset, UBOOL Immediate );
	static void ExportProperties( FOutputDevice& Out, UClass* ObjectClass, BYTE* Object, INT Indent, UClass* DiffClass, BYTE* Diff, UObject* Parent );
	static UBOOL GetInitialized();
	static UPackage* GetTransientPackage();
	static void VerifyLinker( ULinkerLoad* Linker );
	static void ProcessRegistrants();
	static void BindPackage( UPackage* Pkg );
	static const TCHAR* GetLanguage();
	static void SetLanguage( const TCHAR* LanguageExt );
	static INT GetObjectHash( FName ObjName, INT Outer )
	{
		return (ObjName.GetIndex() /*^ Outer*/) & (ARRAY_COUNT(GObjHash)-1);
	}

	// Functions.
	void AddToRoot();
	void RemoveFromRoot();
	FString GetFullName() const;
	FString GetPathName( UObject* StopOuter=NULL ) const;
	UBOOL IsValid();
	void ConditionalRegister();
	UBOOL ConditionalDestroy();
	void ConditionalPostLoad();
	void ConditionalShutdownAfterError();
	UBOOL IsA( UClass* SomeBaseClass ) const;
	UBOOL IsIn( UObject* SomeOuter ) const;
	UBOOL IsProbing( FName ProbeName );
	virtual void Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL );
	UField* FindObjectField( FName InName, UBOOL Global=0 );
	UFunction* FindFunction( FName InName, UBOOL Global=0 );
	UFunction* FindFunctionChecked( FName InName, UBOOL Global=0 );
	UState* FindState( FName InName );
	void SaveConfig( QWORD Flags=CPF_Config, const TCHAR* Filename=NULL );
	void LoadConfig( UBOOL Propagate=0, UClass* Class=NULL, const TCHAR* Filename=NULL );
	void LoadLocalized();
	void InitClassDefaultObject( UClass* InClass, UBOOL SetOuter = 0 );
	void ProcessInternal( FFrame& TheStack, void*const Result );
	void ParseParms( const TCHAR* Parms );
	virtual void NetDirty(UProperty* property) {}
	virtual void ExecutingBadStateCode(FFrame& Stack);
	
	// Accessors.
	inline UClass* GetClass() const
	{
		return Class;
	}
	void SetClass(UClass* NewClass)
	{
		Class = NewClass;
	}
	DWORD GetFlags() const
	{
		return ObjectFlags;
	}
	void SetFlags( DWORD NewFlags )
	{
		ObjectFlags |= NewFlags;
		checkSlow(Name!=NAME_None || !(ObjectFlags&RF_Public));
	}
	void ClearFlags( DWORD NewFlags )
	{
		ObjectFlags &= ~NewFlags;
		checkSlow(Name!=NAME_None || !(ObjectFlags&RF_Public));
	}
	const TCHAR* GetName() const
	{
		return *Name;
	}
	const FName GetFName() const
	{
		return Name;
	}
	UObject* GetOuter() const
	{
		return Outer;
	}
	// Walks up the list of outers until it finds the highest one
	UObject* GetOutermost() const
	{
		UObject* Top;
		for( Top = GetOuter() ; Top && Top->GetOuter() ; Top = Top->GetOuter() );
		return Top;
	}
	DWORD GetIndex() const
	{
		return Index;
	}
	ULinkerLoad* GetLinker() const
	{
		return _Linker;
	}
	INT GetLinkerIndex() const
	{
		return _LinkerIndex;
	}
	FStateFrame* GetStateFrame() const
	{
		return StateFrame;
	}

	// delegates
	#define DELEGATE_IS_SET(del) (__##del##__Delegate.Object!=NULL && !__##del##__Delegate.Object->IsPendingKill())
	#define OBJ_DELEGATE_IS_SET(obj,del) (obj##->##__##del##__Delegate.Object!=NULL && !obj##->##__##del##__Delegate.Object->IsPendingKill())

	// UnrealScript intrinsics.
	#define DECLARE_FUNCTION(func) void func( FFrame& TheStack, RESULT_DECL );
	DECLARE_FUNCTION(execUndefined)
	DECLARE_FUNCTION(execLocalVariable)
	DECLARE_FUNCTION(execInstanceVariable)
	DECLARE_FUNCTION(execDefaultVariable)
	DECLARE_FUNCTION(execArrayElement)
	DECLARE_FUNCTION(execDynArrayElement)
	DECLARE_FUNCTION(execDynArrayLength)
	DECLARE_FUNCTION(execDynArrayInsert)
	DECLARE_FUNCTION(execDynArrayRemove)
	DECLARE_FUNCTION(execBoolVariable)
	DECLARE_FUNCTION(execClassDefaultVariable)
	DECLARE_FUNCTION(execEndFunctionParms)
	DECLARE_FUNCTION(execNothing)
	DECLARE_FUNCTION(execStop)
	DECLARE_FUNCTION(execEndCode)
	DECLARE_FUNCTION(execSwitch)
	DECLARE_FUNCTION(execCase)
	DECLARE_FUNCTION(execJump)
	DECLARE_FUNCTION(execJumpIfNot)
	DECLARE_FUNCTION(execAssert)
	DECLARE_FUNCTION(execGotoLabel)
	DECLARE_FUNCTION(execLet)
	DECLARE_FUNCTION(execLetBool)
	DECLARE_FUNCTION(execLetDelegate)
	DECLARE_FUNCTION(execEatString)
	DECLARE_FUNCTION(execSelf)
	DECLARE_FUNCTION(execContext)
	DECLARE_FUNCTION(execVirtualFunction)
	DECLARE_FUNCTION(execFinalFunction)
	DECLARE_FUNCTION(execGlobalFunction)
	DECLARE_FUNCTION(execDelegateFunction)
	DECLARE_FUNCTION(execDelegateProperty)
	DECLARE_FUNCTION(execStructCmpEq)
	DECLARE_FUNCTION(execStructCmpNe)
	DECLARE_FUNCTION(execStructMember)
	DECLARE_FUNCTION(execIntConst)
	DECLARE_FUNCTION(execFloatConst)
	DECLARE_FUNCTION(execStringConst)
	DECLARE_FUNCTION(execUnicodeStringConst)
	DECLARE_FUNCTION(execObjectConst)
	DECLARE_FUNCTION(execNameConst)
	DECLARE_FUNCTION(execByteConst)
	DECLARE_FUNCTION(execIntZero)
	DECLARE_FUNCTION(execIntOne)
	DECLARE_FUNCTION(execTrue)
	DECLARE_FUNCTION(execFalse)
	DECLARE_FUNCTION(execNoObject)
	DECLARE_FUNCTION(execIntConstByte)
	DECLARE_FUNCTION(execDynamicCast)
	DECLARE_FUNCTION(execMetaCast)
	DECLARE_FUNCTION(execPrimitiveCast)
	DECLARE_FUNCTION(execByteToInt)
	DECLARE_FUNCTION(execByteToBool)
	DECLARE_FUNCTION(execByteToFloat)
	DECLARE_FUNCTION(execByteToString)
	DECLARE_FUNCTION(execIntToByte)
	DECLARE_FUNCTION(execIntToBool)
	DECLARE_FUNCTION(execIntToFloat)
	DECLARE_FUNCTION(execIntToString)
	DECLARE_FUNCTION(execBoolToByte)
	DECLARE_FUNCTION(execBoolToInt)
	DECLARE_FUNCTION(execBoolToFloat)
	DECLARE_FUNCTION(execBoolToString)
	DECLARE_FUNCTION(execFloatToByte)
	DECLARE_FUNCTION(execFloatToInt)
	DECLARE_FUNCTION(execFloatToBool)
	DECLARE_FUNCTION(execFloatToString)
	DECLARE_FUNCTION(execRotationConst)
	DECLARE_FUNCTION(execVectorConst)
	DECLARE_FUNCTION(execStringToVector)
	DECLARE_FUNCTION(execStringToRotator)
	DECLARE_FUNCTION(execVectorToBool)
	DECLARE_FUNCTION(execVectorToString)
	DECLARE_FUNCTION(execVectorToRotator)
	DECLARE_FUNCTION(execRotatorToBool)
	DECLARE_FUNCTION(execRotatorToVector)
	DECLARE_FUNCTION(execRotatorToString)
    DECLARE_FUNCTION(execRotRand)
    DECLARE_FUNCTION(execGetUnAxes)
    DECLARE_FUNCTION(execGetAxes)
    DECLARE_FUNCTION(execSubtractEqual_RotatorRotator)
    DECLARE_FUNCTION(execAddEqual_RotatorRotator)
    DECLARE_FUNCTION(execSubtract_RotatorRotator)
    DECLARE_FUNCTION(execAdd_RotatorRotator)
    DECLARE_FUNCTION(execDivideEqual_RotatorFloat)
    DECLARE_FUNCTION(execMultiplyEqual_RotatorFloat)
    DECLARE_FUNCTION(execDivide_RotatorFloat)
    DECLARE_FUNCTION(execMultiply_FloatRotator)
    DECLARE_FUNCTION(execMultiply_RotatorFloat)
    DECLARE_FUNCTION(execNotEqual_RotatorRotator)
    DECLARE_FUNCTION(execEqualEqual_RotatorRotator)
	DECLARE_FUNCTION(execGetRand)
	DECLARE_FUNCTION(execRSize)
	DECLARE_FUNCTION(execRMin)
	DECLARE_FUNCTION(execRMax)
	DECLARE_FUNCTION(execMirrorVectorByNormal)
    DECLARE_FUNCTION(execVRand)
	DECLARE_FUNCTION(execVLerp)
	DECLARE_FUNCTION(execVSmerp)
    DECLARE_FUNCTION(execNormal)
    DECLARE_FUNCTION(execVSize)
    DECLARE_FUNCTION(execVSize2D)
    DECLARE_FUNCTION(execVSizeSq)
    DECLARE_FUNCTION(execSubtractEqual_VectorVector)
    DECLARE_FUNCTION(execAddEqual_VectorVector)
    DECLARE_FUNCTION(execDivideEqual_VectorFloat)
    DECLARE_FUNCTION(execMultiplyEqual_VectorVector)
    DECLARE_FUNCTION(execMultiplyEqual_VectorFloat)
    DECLARE_FUNCTION(execCross_VectorVector)
    DECLARE_FUNCTION(execDot_VectorVector)
    DECLARE_FUNCTION(execNotEqual_VectorVector)
    DECLARE_FUNCTION(execEqualEqual_VectorVector)
    DECLARE_FUNCTION(execGreaterGreater_VectorRotator)
    DECLARE_FUNCTION(execLessLess_VectorRotator)
    DECLARE_FUNCTION(execSubtract_VectorVector)
    DECLARE_FUNCTION(execAdd_VectorVector)
    DECLARE_FUNCTION(execDivide_VectorFloat)
    DECLARE_FUNCTION(execMultiply_VectorVector)
    DECLARE_FUNCTION(execMultiply_FloatVector)
    DECLARE_FUNCTION(execMultiply_VectorFloat)
    DECLARE_FUNCTION(execSubtract_PreVector)
	DECLARE_FUNCTION(execOrthoRotation);
	DECLARE_FUNCTION(execNormalize);
	DECLARE_FUNCTION(execRLerp);
	DECLARE_FUNCTION(execRSmerp);
	DECLARE_FUNCTION(execClockwiseFrom_IntInt);
	DECLARE_FUNCTION(execObjectToBool)
	DECLARE_FUNCTION(execObjectToString)
	DECLARE_FUNCTION(execNameToBool)
	DECLARE_FUNCTION(execNameToString)
	DECLARE_FUNCTION(execStringToName)
	DECLARE_FUNCTION(execStringToByte)
	DECLARE_FUNCTION(execStringToInt)
	DECLARE_FUNCTION(execStringToBool)
	DECLARE_FUNCTION(execStringToFloat)
	DECLARE_FUNCTION(execNot_PreBool)
	DECLARE_FUNCTION(execEqualEqual_BoolBool)
	DECLARE_FUNCTION(execNotEqual_BoolBool)
	DECLARE_FUNCTION(execAndAnd_BoolBool)
	DECLARE_FUNCTION(execXorXor_BoolBool)
	DECLARE_FUNCTION(execOrOr_BoolBool)
	DECLARE_FUNCTION(execMultiplyEqual_ByteByte)
	DECLARE_FUNCTION(execDivideEqual_ByteByte)
	DECLARE_FUNCTION(execAddEqual_ByteByte)
	DECLARE_FUNCTION(execSubtractEqual_ByteByte)
	DECLARE_FUNCTION(execAddAdd_PreByte)
	DECLARE_FUNCTION(execSubtractSubtract_PreByte)
	DECLARE_FUNCTION(execAddAdd_Byte)
	DECLARE_FUNCTION(execSubtractSubtract_Byte)
	DECLARE_FUNCTION(execComplement_PreInt)
	DECLARE_FUNCTION(execSubtract_PreInt)
	DECLARE_FUNCTION(execMultiply_IntInt)
	DECLARE_FUNCTION(execDivide_IntInt)
	DECLARE_FUNCTION(execAdd_IntInt)
	DECLARE_FUNCTION(execSubtract_IntInt)
	DECLARE_FUNCTION(execLessLess_IntInt)
	DECLARE_FUNCTION(execGreaterGreater_IntInt)
	DECLARE_FUNCTION(execGreaterGreaterGreater_IntInt)
	DECLARE_FUNCTION(execLess_IntInt)
	DECLARE_FUNCTION(execGreater_IntInt)
	DECLARE_FUNCTION(execLessEqual_IntInt)
	DECLARE_FUNCTION(execGreaterEqual_IntInt)
	DECLARE_FUNCTION(execEqualEqual_IntInt)
	DECLARE_FUNCTION(execNotEqual_IntInt)
	DECLARE_FUNCTION(execAnd_IntInt)
	DECLARE_FUNCTION(execXor_IntInt)
	DECLARE_FUNCTION(execOr_IntInt)
	DECLARE_FUNCTION(execMultiplyEqual_IntFloat)
	DECLARE_FUNCTION(execDivideEqual_IntFloat)
	DECLARE_FUNCTION(execAddEqual_IntInt)
	DECLARE_FUNCTION(execSubtractEqual_IntInt)
	DECLARE_FUNCTION(execAddAdd_PreInt)
	DECLARE_FUNCTION(execSubtractSubtract_PreInt)
	DECLARE_FUNCTION(execAddAdd_Int)
	DECLARE_FUNCTION(execSubtractSubtract_Int)
	DECLARE_FUNCTION(execRand)
	DECLARE_FUNCTION(execMin)
	DECLARE_FUNCTION(execMax)
	DECLARE_FUNCTION(execClamp)
	DECLARE_FUNCTION(execSubtract_PreFloat)
	DECLARE_FUNCTION(execMultiplyMultiply_FloatFloat)
	DECLARE_FUNCTION(execMultiply_FloatFloat)
	DECLARE_FUNCTION(execDivide_FloatFloat)
	DECLARE_FUNCTION(execPercent_FloatFloat)
	DECLARE_FUNCTION(execAdd_FloatFloat)
	DECLARE_FUNCTION(execSubtract_FloatFloat)
	DECLARE_FUNCTION(execLess_FloatFloat)
	DECLARE_FUNCTION(execGreater_FloatFloat)
	DECLARE_FUNCTION(execLessEqual_FloatFloat)
	DECLARE_FUNCTION(execGreaterEqual_FloatFloat)
	DECLARE_FUNCTION(execEqualEqual_FloatFloat)
	DECLARE_FUNCTION(execNotEqual_FloatFloat)
	DECLARE_FUNCTION(execComplementEqual_FloatFloat)
	DECLARE_FUNCTION(execMultiplyEqual_FloatFloat)
	DECLARE_FUNCTION(execDivideEqual_FloatFloat)
	DECLARE_FUNCTION(execAddEqual_FloatFloat)
	DECLARE_FUNCTION(execSubtractEqual_FloatFloat)
	DECLARE_FUNCTION(execAbs)
	DECLARE_FUNCTION(execDebugInfo) //DEBUGGER
	DECLARE_FUNCTION(execSin)
	DECLARE_FUNCTION(execAsin)
	DECLARE_FUNCTION(execCos)
	DECLARE_FUNCTION(execAcos)
	DECLARE_FUNCTION(execTan)
	DECLARE_FUNCTION(execAtan)
	DECLARE_FUNCTION(execExp)
	DECLARE_FUNCTION(execLoge)
	DECLARE_FUNCTION(execSqrt)
	DECLARE_FUNCTION(execSquare)
	DECLARE_FUNCTION(execFRand)
	DECLARE_FUNCTION(execFMin)
	DECLARE_FUNCTION(execFMax)
	DECLARE_FUNCTION(execFClamp)
	DECLARE_FUNCTION(execLerp)
	DECLARE_FUNCTION(execSmerp)
	DECLARE_FUNCTION(execConcat_StringString)
	DECLARE_FUNCTION(execAt_StringString)
	DECLARE_FUNCTION(execLess_StringString)
	DECLARE_FUNCTION(execGreater_StringString)
	DECLARE_FUNCTION(execLessEqual_StringString)
	DECLARE_FUNCTION(execGreaterEqual_StringString)
	DECLARE_FUNCTION(execEqualEqual_StringString)
	DECLARE_FUNCTION(execNotEqual_StringString)
	DECLARE_FUNCTION(execComplementEqual_StringString)
	DECLARE_FUNCTION(execLen)
	DECLARE_FUNCTION(execInStr)
	DECLARE_FUNCTION(execMid)
	DECLARE_FUNCTION(execLeft)
	DECLARE_FUNCTION(execRight)
	DECLARE_FUNCTION(execCaps)
	DECLARE_FUNCTION(execChr)
	DECLARE_FUNCTION(execAsc)
	DECLARE_FUNCTION(execQuatProduct)
	DECLARE_FUNCTION(execQuatInvert)
	DECLARE_FUNCTION(execQuatRotateVector)
	DECLARE_FUNCTION(execQuatFindBetween)
	DECLARE_FUNCTION(execQuatFromAxisAndAngle)
	DECLARE_FUNCTION(execQuatFromRotator)
	DECLARE_FUNCTION(execQuatToRotator)
	DECLARE_FUNCTION(execEqualEqual_ObjectObject)
	DECLARE_FUNCTION(execNotEqual_ObjectObject)
	DECLARE_FUNCTION(execEqualEqual_NameName)
	DECLARE_FUNCTION(execNotEqual_NameName)
	DECLARE_FUNCTION(execLog)
	DECLARE_FUNCTION(execWarn)
	DECLARE_FUNCTION(execNew)
	DECLARE_FUNCTION(execClassIsChildOf)
	DECLARE_FUNCTION(execClassContext)
	DECLARE_FUNCTION(execGoto)
	DECLARE_FUNCTION(execGotoState)
	DECLARE_FUNCTION(execPushState)
	DECLARE_FUNCTION(execPopState)
	DECLARE_FUNCTION(execIsA)
	DECLARE_FUNCTION(execEnable)
	DECLARE_FUNCTION(execDisable)
	DECLARE_FUNCTION(execIterator)
	DECLARE_FUNCTION(execLocalize)
	DECLARE_FUNCTION(execNativeParm)
	DECLARE_FUNCTION(execGetPropertyText)
	DECLARE_FUNCTION(execSetPropertyText)
	DECLARE_FUNCTION(execSaveConfig)
	DECLARE_FUNCTION(execStaticSaveConfig)
	DECLARE_FUNCTION(execGetEnum)
	DECLARE_FUNCTION(execDynamicLoadObject)
	DECLARE_FUNCTION(execFindObject)
	DECLARE_FUNCTION(execIsInState)
	DECLARE_FUNCTION(execGetStateName)
	DECLARE_FUNCTION(execGetFuncName)
	DECLARE_FUNCTION(execScriptTrace)
	DECLARE_FUNCTION(execHighNative0)
	DECLARE_FUNCTION(execHighNative1)
	DECLARE_FUNCTION(execHighNative2)
	DECLARE_FUNCTION(execHighNative3)
	DECLARE_FUNCTION(execHighNative4)
	DECLARE_FUNCTION(execHighNative5)
	DECLARE_FUNCTION(execHighNative6)
	DECLARE_FUNCTION(execHighNative7)
	DECLARE_FUNCTION(execHighNative8)
	DECLARE_FUNCTION(execHighNative9)
	DECLARE_FUNCTION(execHighNative10)
	DECLARE_FUNCTION(execHighNative11)
	DECLARE_FUNCTION(execHighNative12)
	DECLARE_FUNCTION(execHighNative13)
	DECLARE_FUNCTION(execHighNative14)
	DECLARE_FUNCTION(execHighNative15)

	DECLARE_FUNCTION(execProjectOnTo)
	DECLARE_FUNCTION(execIsZero)

	// UnrealScript calling stubs.
    void eventBeginState()
    {
        ProcessEvent(FindFunctionChecked(NAME_BeginState),NULL);
    }
    void eventEndState()
    {
        ProcessEvent(FindFunctionChecked(NAME_EndState),NULL);
    }
	void eventPushedState()
	{
		ProcessEvent(FindFunctionChecked(NAME_PushedState),NULL);
	}
	void eventPoppedState()
	{
		ProcessEvent(FindFunctionChecked(NAME_PoppedState),NULL);
	}
	void eventPausedState()
	{
		ProcessEvent(FindFunctionChecked(NAME_PausedState),NULL);
	}
	void eventContinuedState()
	{
		ProcessEvent(FindFunctionChecked(NAME_ContinuedState),NULL);
	}
	static TArray<UObject*> GetLoaderList()
	{
		return GObjLoaders;
	}

	//
	// UnrealEd browser utility functions
	//

	// Returns a description of this object that can be used as extra information in list views
	virtual FString GetDesc()	{ return TEXT(""); }

	// Draws a thumbnail for this object at the specified location
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz ) { }

	// Returns a FThumbnailDesc structure describing the thumbnail for this object
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz ) { return FThumbnailDesc(); }

	// Returns a set of strings that describe the object
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels ) { return 0; }

	virtual UBOOL IsSelected()
	{
		return GetFlags()&RF_EdSelected;
	}

	// Marks the package containing this object as needing to be saved.

	void MarkPackageDirty( UBOOL InDirty = 1 );
};

/*----------------------------------------------------------------------------
	Core templates.
----------------------------------------------------------------------------*/

// Hash function.
inline DWORD GetTypeHash( const UObject* A )
{
	return A ? A->GetIndex() : 0;
}

// Parse an object name in the input stream.
template< class T > UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, T*& Obj, UObject* Outer )
{
	return ParseObject( Stream, Match, T::StaticClass(), *(UObject **)&Obj, Outer );
}

// Find an optional object.
template< class T > T* FindObject( UObject* Outer, const TCHAR* Name, UBOOL ExactClass=0 )
{
	return (T*)UObject::StaticFindObject( T::StaticClass(), Outer, Name, ExactClass );
}

// Find an object, no failure allowed.
template< class T > T* FindObjectChecked( UObject* Outer, const TCHAR* Name, UBOOL ExactClass=0 )
{
	return (T*)UObject::StaticFindObjectChecked( T::StaticClass(), Outer, Name, ExactClass );
}

// Dynamically cast an object type-safely.
class UClass;
template< class T > T* Cast( UObject* Src )
{
		return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}

template< class T > T* Cast( UObject* Src, EClassFlags Flag )
{
	return Src && (Src->GetClass()->ClassFlags & Flag) ? (T*)Src : NULL;
}

template< class T, class U > T* CastChecked( U* Src )
{
	if( !Src || !Src->IsA(T::StaticClass()) )
		appErrorf( TEXT("Cast of %s to %s failed"), Src ? *Src->GetFullName() : TEXT("NULL"), *T::StaticClass()->GetName() );
	return (T*)Src;
}
template< class T > const T* ConstCast( const UObject* Src )
{
		return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}

template< class T > const T* ConstCast( const UObject* Src, EClassFlags Flag )
{
	return Src && (Src->GetClass()->ClassFlags & Flag) ? (T*)Src : NULL;
}

template< class T, class U > const T* ConstCastChecked( const U* Src )
{
	if( !Src || !Src->IsA(T::StaticClass()) )
		appErrorf( TEXT("Cast of %s to %s failed"), Src ? *Src->GetFullName() : TEXT("NULL"), *T::StaticClass()->GetName() );
	return (T*)Src;
}

// Construct an object of a particular class.
template< class T > T* ConstructObject( UClass* Class, UObject* Outer=(UObject*)-1, FName Name=NAME_None, DWORD SetFlags=0,
										UObject* Template=NULL )
{
	check(Class->IsChildOf(T::StaticClass()));
	if( Outer==(UObject*)-1 )
		Outer = UObject::GetTransientPackage();
	return (T*)UObject::StaticConstructObject( Class, Outer, Name, SetFlags, Template );
}

// Load an object.
template< class T > T* LoadObject( UObject* Outer, const TCHAR* Name, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	return (T*)UObject::StaticLoadObject( T::StaticClass(), Outer, Name, Filename, LoadFlags, Sandbox );
}

// Load a class object.
template< class T > UClass* LoadClass( UObject* Outer, const TCHAR* Name, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	return UObject::StaticLoadClass( T::StaticClass(), Outer, Name, Filename, LoadFlags, Sandbox );
}

// Get default object of a class.
template< class T > T* GetDefault()
{
	return (T*)&T::StaticClass()->Defaults(0);
}

/**
 * Takes an FName and checks to see that it follows the rules that Unreal requires.
 *
 * @param	InName		The name to check
 * @param	InReason	If the check fails, this string is filled in with the reason why.
 *
 * @return	1 if the name is valid, 0 if it is not
 */

inline UBOOL FIsValidObjectName( const FName& InName, FString& InReason )
{
	FString Name = *InName;

	// See if the name contains invalid characters.

	FString InvalidChars( TEXT("\"' ,.") ), Char;

	for( INT x = 0 ; x < InvalidChars.Len() ; ++x )
	{
		Char = InvalidChars.Mid( x, 1 );

		if( Name.InStr( Char ) != INDEX_NONE )
		{
			InReason = FString::Printf( TEXT("Name contains an invalid character : [%s]"), *Char );
			return 0;
		}
	}

	return 1;
}

/**
 * Takes an FName and checks to see that it is unique among all loaded objects.
 *
 * @param	InName		The name to check
 * @param	InReason	If the check fails, this string is filled in with the reason why.
 *
 * @return	1 if the name is valid, 0 if it is not
 */

inline UBOOL FIsUniqueObjectName( const FName& InName, FString& InReason )
{
	FString Name = *InName;

	// See if the name is already in use.

	if( UObject::StaticFindObject( UObject::StaticClass(), (UObject*)ANY_PACKAGE, *InName ) != NULL )
	{
		InReason = TEXT("Name is already in use by another object.");
		return 0;
	}

	return 1;
}


/*----------------------------------------------------------------------------
	Object iterators.
----------------------------------------------------------------------------*/

//
// Class for iterating through all objects.
//
class FObjectIterator
{
public:
	FObjectIterator( UClass* InClass=UObject::StaticClass() )
	:	Class( InClass ), Index( -1 )
	{
		check(Class);
		++*this;
	}
	void operator++()
	{
		while( ++Index<UObject::GObjObjects.Num() && (!UObject::GObjObjects(Index) || !UObject::GObjObjects(Index)->IsA(Class)) );
	}
	UObject* operator*()
	{
		return UObject::GObjObjects(Index);
	}
	UObject* operator->()
	{
		return UObject::GObjObjects(Index);
	}
	operator UBOOL()
	{
		return Index<UObject::GObjObjects.Num();
	}
protected:
	UClass* Class;
	INT Index;
};

//
// Class for iterating through all objects which inherit from a
// specified base class.
//
template< class T > class TObjectIterator : public FObjectIterator
{
public:
	TObjectIterator()
	:	FObjectIterator( T::StaticClass() )
	{}
	T* operator* ()
	{
		return (T*)FObjectIterator::operator*();
	}
	T* operator-> ()
	{
		return (T*)FObjectIterator::operator->();
	}
};

//
// Class for iterating through all selected objects which inherit from a
// specified base class.
//

class TSelectedObjectIterator
{
public:
	TSelectedObjectIterator( UClass* InClass )
		: Class( InClass ), Index( -1 )
	{
		check(Class);
		++*this;
	}
	void operator++()
	{
		while(
			++Index < UObject::GObjObjects.Num()
				&& ( !UObject::GObjObjects(Index) || !UObject::GObjObjects(Index)->IsA(Class) || !UObject::GObjObjects(Index)->IsSelected() )
		);
	}
	UObject* operator*()
	{
		return UObject::GObjObjects(Index);
	}
	UObject* operator->()
	{
		return UObject::GObjObjects(Index);
	}
	operator UBOOL()
	{
		return Index<UObject::GObjObjects.Num();
	}
protected:
	UClass* Class;
	INT Index;
};

/**
 * Archive for replacing a reference to an object. This classes uses
 * serialization to replace all references to one object with another.
 * Note that this archive will only traverse objects with an Outer
 * that matches InSearchObject.
 */
class FArchiveReplaceObjectRef : public FArchive
{
public:
	/**
	 * Initializes variables and starts the serialization search
	 *
	 * @param InSearchObject The object to start the search on
	 * @param InObjectsToFind The objects to find references for
	 * @param InReplaceObjectWith The object to replace references with (null zeros them)
	 */
	FArchiveReplaceObjectRef(UObject* InSearchObject,TArray<UObject*> *InObjectsToFind,UObject* InReplaceObjectWith = NULL);

	/**
	 * Returns the number of times the object was referenced
	 */
	INT GetCount()
	{
		return Count;
	}

	/**
	 * Serializes the reference to the object
	 */
	FArchive& operator<<( class UObject*& Obj );

protected:
	/** Initial object to start the reference search from */
	UObject* SearchObject;

	/** The objects to find references to */
	TArray<UObject*> *ObjectsToFind;

	/** The object to replace references with */
	UObject* ReplaceObjectWith;
	
	/** The number of times encountered */
	INT Count;

	/** List of objects that have already been serialized */
	TArray<UObject*> SerializedObjects;
};

#define AUTO_INITIALIZE_REGISTRANTS \
	UObject::StaticClass(); \
	UClass::StaticClass(); \
	USubsystem::StaticClass(); \
	USystem::StaticClass(); \
	UProperty::StaticClass(); \
	UStructProperty::StaticClass(); \
	UField::StaticClass(); \
	UMapProperty::StaticClass(); \
	UArrayProperty::StaticClass(); \
	UFixedArrayProperty::StaticClass(); \
	UStrProperty::StaticClass(); \
	UNameProperty::StaticClass(); \
	UClassProperty::StaticClass(); \
	UObjectProperty::StaticClass(); \
	UFloatProperty::StaticClass(); \
	UBoolProperty::StaticClass(); \
	UIntProperty::StaticClass(); \
	UByteProperty::StaticClass(); \
	UDelegateProperty::StaticClass(); \
	ULanguage::StaticClass(); \
	UTextBufferFactory::StaticClass(); \
	UFactory::StaticClass(); \
	UPackage::StaticClass(); \
	UCommandlet::StaticClass(); \
	ULinkerSave::StaticClass(); \
	ULinker::StaticClass(); \
	ULinkerLoad::StaticClass(); \
	UEnum::StaticClass(); \
	UTextBuffer::StaticClass(); \
	UPackageMap::StaticClass(); \
	UConst::StaticClass(); \
	UFunction::StaticClass(); \
	UStruct::StaticClass(); \
	UComponentProperty::StaticClass();

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

