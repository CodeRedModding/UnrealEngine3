/*=============================================================================
	Core.h: Unreal core public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#ifndef _INC_CORE
#define _INC_CORE

/*----------------------------------------------------------------------------
	Low level includes.
----------------------------------------------------------------------------*/

// Unwanted Intel C++ level 4 warnings/ remarks.
#if __ICL
#pragma warning(disable : 873)
#pragma warning(disable : 981)
#pragma warning(disable : 522)
#pragma warning(disable : 271)
#pragma warning(disable : 424)
#pragma warning(disable : 193)
#pragma warning(disable : 444)
#pragma warning(disable : 440)
#pragma warning(disable : 171)
#pragma warning(disable : 1125)
#pragma warning(disable : 488)
#pragma warning(disable : 858)
#pragma warning(disable : 82)
#pragma warning(disable : 1)
#pragma warning(disable : 177)
#pragma warning(disable : 279)
#endif

// Build options.
#include "UnBuild.h"

// Are we on a console or not?
#ifdef XBOX
#define CONSOLE 1
// Put all data into own segment for use with raw memory images and to assess how many static variables we have.
//@todo xenon: use different sections for data and bss and update FMallocMemoryImageXenon.h
#define XENON_DATA_SEG ".epic"
#define XENON_BSS_SEG ".epic"
#define XENON_NOCLOBBER_SEG ".epicnc"
// According to some documentation section names are only up to 8 characters long and are not 0 terminated if 8 characters are used.
#define XENON_MAX_SECTIONNAME 8
#pragma data_seg(XENON_DATA_SEG)
#pragma bss_seg(XENON_BSS_SEG)
// The .epicnc segment holds static variables the memory image code won't clobber (nc == not clobbered).
#pragma section(XENON_NOCLOBBER_SEG)
#endif

#if _MSC_VER || __ICC || __LINUX__
	#define SUPPORTS_PRAGMA_PACK 1
#else
	#define SUPPORTS_PRAGMA_PACK 0
#endif

// Compiler specific include.
#ifdef XBOX
	#include "UnXenon.h"
#elif _MSC_VER
	#include "UnVcWin32.h"
#elif __GNUG__
	#include <string.h>
	#include "UnGnuG.h"
#elif __MWERKS__
	#error No Metrowerks support
#elif __ICC
	#if __LINUX_X86__
		#include <string.h>
		#include "UnGnuG.h"
	#else
		#error Not yet supported
	#endif
#else
	#error Unknown Compiler
#endif


// OS specific include.
#if __UNIX__
	#include "UnUnix.h"
	#include <signal.h>
#endif


// CPU specific includes.
#if ((__INTEL__) && (!defined __GNUC__))
#define __HAS_SSE__ 1
#pragma warning(disable : 4799)
#include <xmmintrin.h>
#include <fvec.h>
#endif
#ifdef _PPC_
#define __HAS_ALTIVEC__ 1
#include "ppcintrinsics.h"
struct __vector4_c : public __vector4
{
	__vector4_c( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW )
	{
		v[0] = InX; v[1] = InY; v[2] = InZ; v[3] = InW;
	}
	__vector4_c( DWORD InX, DWORD InY, DWORD InZ, DWORD InW )
	{
		u[0] = InX; u[1] = InY; u[2] = InZ; u[3] = InW;
	}

};
#endif

// Global constants.
enum {MAXBYTE		= 0xff       };
enum {MAXWORD		= 0xffffU    };
enum {MAXDWORD		= 0xffffffffU};
enum {MAXSBYTE		= 0x7f       };
enum {MAXSWORD		= 0x7fff     };
enum {MAXINT		= 0x7fffffff };
enum {INDEX_NONE	= -1         };
enum {UNICODE_BOM   = 0xfeff     };
enum ENoInit {E_NoInit = 0};

// Unicode or single byte character set mappings.
#ifdef _UNICODE
	#ifndef _TCHAR_DEFINED
		typedef UNICHAR  TCHAR;
	#endif
	typedef UNICHARU TCHARU;

    #ifndef _TEXT_DEFINED
	#undef TEXT
	#define TEXT(s) L##s
    #endif

    #ifndef _US_DEFINED
	#undef US
	#define US FString(L"")
    #endif

	inline TCHAR    FromAnsi   ( ANSICHAR In ) { return (BYTE)In;                        }
	inline TCHAR    FromUnicode( UNICHAR In  ) { return In;                              }
	inline ANSICHAR ToAnsi     ( TCHAR In    ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline UNICHAR  ToUnicode  ( TCHAR In    ) { return In;                              }
#else
	#ifndef _TCHAR_DEFINED
		typedef ANSICHAR  TCHAR;
	#endif
	typedef ANSICHARU TCHARU;

	#undef TEXT
	#define TEXT(s) s
	#undef US
	#define US FString("")
	inline TCHAR    FromAnsi   ( ANSICHAR In ) { return In;                              }
	inline TCHAR    FromUnicode( UNICHAR In  ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline ANSICHAR ToAnsi     ( TCHAR In    ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline UNICHAR  ToUnicode  ( TCHAR In    ) { return (BYTE)In;                        }
#endif

/*----------------------------------------------------------------------------
	Forward declarations.
----------------------------------------------------------------------------*/

// Objects.
class	UObject;
class		UComponent;
class		UExporter;
class		UFactory;
class		UField;
class			UConst;
class			UEnum;
class			UProperty;
class				UByteProperty;
class				UIntProperty;
class				UBoolProperty;
class				UFloatProperty;
class				UObjectProperty;
class					UComponentProperty;
class					UClassProperty;
class				UNameProperty;
class				UStructProperty;
class               UStrProperty;
class               UArrayProperty;
class				UDelegateProperty;
class			UStruct;
class				UFunction;
class				UState;
class					UClass;
class		ULinker;
class			ULinkerLoad;
class			ULinkerSave;
class		UPackage;
class		USubsystem;
class			USystem;
class		UTextBuffer;
class       URenderDevice;
class		UPackageMap;
class		UDebugger; //DEBUGGER

// Structs.
class FName;
class FArchive;
class FCompactIndex;
class FExec;
class FGuid;
class FMemStack;
class FPackageInfo;
class FTransactionBase;
class FUnknown;
class FRepLink;
class FArray;
class FLazyLoader;
class FString;
class FMalloc;

// Templates.
template<class T> class TArray;
template<class T> class TTransArray;
template<class T> class TLazyArray;
template<class TK, class TI> class TMap;
template<class TK, class TI> class TMultiMap;

// Globals.
extern class FOutputDevice* GNull;

// EName definition.
#include "UnNames.h"

/*-----------------------------------------------------------------------------
	Ugly VarArgs type checking (debug builds only).
-----------------------------------------------------------------------------*/

#define VARARG_EXTRA(A) A,
#define VARARG_NONE
#define VARARG_PURE =0

#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall )	\
	FuncRet FuncName( ExtraDecl FmtType Fmt, ... ) Pure
#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl )		\
	FuncRet FuncName( ExtraDecl FmtType Fmt, ... )

/*-----------------------------------------------------------------------------
	Abstract interfaces.
-----------------------------------------------------------------------------*/

// An output device.
class FOutputDevice
{
public:
	// FOutputDevice interface.

	virtual void Serialize( const TCHAR* V, EName Event )=0;
	virtual void Flush(){};

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we might have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	virtual void TearDown(){};
		
	// Simple text printing.
	void Log( const TCHAR* S );
	void Log( enum EName Type, const TCHAR* S );
	void Log( const FString& S );
	void Log( enum EName Type, const FString& S );
	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );
	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(enum EName E), VARARG_EXTRA(E) );
};

/**
 * Abstract base version of FOutputDeviceRedirector, needed due to order of dependencies.
 */
class FOutputDeviceRedirectorBase : public FOutputDevice
{
public:
	/**
	 * Adds an output device to the chain of redirections.	
	 *
	 * @param OutputDevice	output device to add
	 */
	virtual void AddOutputDevice( FOutputDevice* OutputDevice ) = 0;
	/**
	 * Removes an output device from the chain of redirections.	
	 *
	 * @param OutputDevice	output device to remove
	 */
	virtual void RemoveOutputDevice( FOutputDevice* OutputDevice ) = 0;
	/**
	 * Returns whether an output device is currently in the list of redirectors.
	 *
	 * @param	OutputDevice	output device to check the list against
	 * @return	TRUE if messages are currently redirected to the the passed in output device, FALSE otherwise
	 */
	virtual UBOOL IsRedirectingTo( FOutputDevice* OutputDevice ) = 0;
};

// Error device.
class FOutputDeviceError : public FOutputDevice
{
public:
	virtual void HandleError()=0;
};

/**
 * This class servers as the base class for console window output.
 */
class FOutputDeviceConsole : public FOutputDevice
{
public:
	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (TRUE) or hide (FALSE) the console window.
	 */
	virtual void Show( UBOOL ShowWindow )=0;

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return TRUE if console is shown, FALSE otherwise
	 */
	virtual UBOOL IsShown()=0;
};

// Memory allocator.

enum
{
	MEMORYIMAGE_NotUsed			= 0,
	MEMORYIMAGE_Loading			= 1,
	MEMORYIMAGE_Saving			= 2,
	MEMORYIMAGE_DebugXMemAlloc	= 4,
};

enum ECacheBehaviour
{
	CACHE_Normal		= 0,
	CACHE_WriteCombine	= 1,
	CACHE_None			= 2
};

class FMalloc
{
public:
	virtual void* Malloc( DWORD Count ) { return NULL; }
	virtual void* Realloc( void* Original, DWORD Count ) { return NULL; }
	virtual void Free( void* Original ) {}
	virtual void* PhysicalAlloc( DWORD Count, ECacheBehaviour CacheBehaviour = CACHE_WriteCombine ) { return NULL; }
	virtual void PhysicalFree( void* Original ) {}
	virtual void DumpAllocs() {}
	virtual void HeapCheck() {}
	virtual void Init( UBOOL Reset ) {}
	virtual void Exit() {}
	virtual void DumpMemoryImage() {}
	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param Virtual	[out] size of virtual allocations
	 * @param Physical	[out] size of physical allocations	
	 */
	virtual void GetAllocationInfo( SIZE_T& Virtual, SIZE_T& Physical ) { Virtual = Physical = 0; }
};

// Configuration database cache.
class FConfigCache
{
public:
	virtual UBOOL GetBool( const TCHAR* Section, const TCHAR* Key, UBOOL& Value, const TCHAR* Filename )=0;
	virtual UBOOL GetInt( const TCHAR* Section, const TCHAR* Key, INT& Value, const TCHAR* Filename )=0;
	virtual UBOOL GetFloat( const TCHAR* Section, const TCHAR* Key, FLOAT& Value, const TCHAR* Filename )=0;
	virtual UBOOL GetString( const TCHAR* Section, const TCHAR* Key, class FString& Str, const TCHAR* Filename )=0;
	virtual FString GetStr( const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename )=0;
	virtual UBOOL GetSection( const TCHAR* Section, TArray<FString>& Value, const TCHAR* Filename )=0;
	virtual TMultiMap<FString,FString>* GetSectionPrivate( const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename )=0;
	virtual void EmptySection( const TCHAR* Section, const TCHAR* Filename )=0;
	virtual void SetBool( const TCHAR* Section, const TCHAR* Key, UBOOL Value, const TCHAR* Filename )=0;
	virtual void SetInt( const TCHAR* Section, const TCHAR* Key, INT Value, const TCHAR* Filename )=0;
	virtual void SetFloat( const TCHAR* Section, const TCHAR* Key, FLOAT Value, const TCHAR* Filename )=0;
	virtual void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename )=0;
	virtual void Flush( UBOOL Read, const TCHAR* Filename=NULL )=0;
	virtual void UnloadFile( const TCHAR* Filename )=0;
	virtual void Detach( const TCHAR* Filename )=0;
	virtual void Exit()=0;
	virtual void Dump( FOutputDevice& Ar )=0;
	virtual ~FConfigCache() {};
};

// Any object that is capable of taking commands.
class FExec
{
public:
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )=0;
};

// Notification hook.
class FNotifyHook
{
public:
	virtual void NotifyDestroy( void* Src ) {}
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange ) {}
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged ) {}
	virtual void NotifyExec( void* Src, const TCHAR* Cmd ) {}
};

// Interface for returning a context string.
class FContextSupplier
{
public:
	virtual FString GetContext()=0;
};

// A context for displaying modal warning messages.
class FFeedbackContext : public FOutputDevice
{
public:
	VARARG_DECL( virtual UBOOL, UBOOL, return, YesNof, VARARG_PURE, const TCHAR*, VARARG_NONE, VARARG_NONE );
	virtual void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )=0;
	virtual void EndSlowTask()=0;
	VARARG_DECL( virtual UBOOL, UBOOL VARARGS, return, StatusUpdatef, VARARG_PURE, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator), VARARG_EXTRA(Numerator) VARARG_EXTRA(Denominator) );

	virtual void SetContext( FContextSupplier* InSupplier )=0;
	virtual void MapCheck_Show() {};
	virtual void MapCheck_ShowConditionally() {};
	virtual void MapCheck_Hide() {};
	virtual void MapCheck_Clear() {};
	virtual void MapCheck_Add( INT InType, void* InActor, const TCHAR* InMessage ) {};

	INT		WarningCount;
	INT		ErrorCount;
	UBOOL	TreatWarningsAsErrors;

	/**
	 * A pointer to the editors frame window.  This gives you the ability to parent windows
	 * correctly in projects that are at a lower level than UnrealEd.
	 */
	DWORD	winEditorFrame;				

	FFeedbackContext()
		: ErrorCount( 0 )
		, WarningCount( 0 )
		, TreatWarningsAsErrors( 0 )
		, winEditorFrame( 0 )
    {}
};

// Class for handling undo/redo transactions among objects.
typedef void( *STRUCT_AR )( FArchive& Ar, void* TPtr );
typedef void( *STRUCT_DTOR )( void* TPtr );
class FTransactionBase
{
public:
	virtual void SaveObject( UObject* Object )=0;
	virtual void SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )=0;
	virtual void Apply()=0;
};

// File manager.
enum EFileTimes
{
	FILETIME_Create				= 0,
	FILETIME_LastAccess			= 1,
	FILETIME_LastWrite			= 2,
};
enum EFileWrite
{
	FILEWRITE_NoFail            = 0x01,
	FILEWRITE_NoReplaceExisting = 0x02,
	FILEWRITE_EvenIfReadOnly    = 0x04,
	FILEWRITE_Unbuffered        = 0x08,
	FILEWRITE_Append			= 0x10,
	FILEWRITE_AllowRead         = 0x20,
};
enum EFileRead
{
	FILEREAD_NoFail             = 0x01,
};
enum ECopyCompress
{
	FILECOPY_Normal				= 0x00,
	FILECOPY_Compress			= 0x01,
	FILECOPY_Decompress			= 0x02,
};
enum ECopyResult
{
	COPY_OK						= 0x00,
	COPY_MiscFail				= 0x01,
	COPY_ReadFail				= 0x02,
	COPY_WriteFail				= 0x03,
	COPY_CompFail				= 0x04,
	COPY_DecompFail				= 0x05,
	COPY_Canceled				= 0x06,
};
#define COMPRESSED_EXTENSION	TEXT(".uz2")

struct FCopyProgress
{
	virtual UBOOL Poll( FLOAT Fraction )=0;
};


const TCHAR* appBaseDir();

class FFileManager
{
public:
	virtual void Init(UBOOL Startup) {}
	virtual FArchive* CreateFileReader( const TCHAR* Filename, DWORD ReadFlags=0, FOutputDevice* Error=GNull )=0;
	virtual FArchive* CreateFileWriter( const TCHAR* Filename, DWORD WriteFlags=0, FOutputDevice* Error=GNull )=0;
	virtual INT FileSize( const TCHAR* Filename )=0;
	virtual UBOOL IsReadOnly( const TCHAR* Filename )=0;
	virtual UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )=0;
	virtual DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, DWORD Compress=FILECOPY_Normal, FCopyProgress* Progress=NULL )=0;
	virtual UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )=0;
	virtual UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )=0;
	virtual UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )=0;
	virtual void FindFiles( TArray<FString>& FileNames, const TCHAR* Filename, UBOOL Files, UBOOL Directories )=0;
	virtual DOUBLE GetFileAgeSeconds( const TCHAR* Filename )=0;
	virtual UBOOL SetDefaultDirectory()=0;
	virtual FString GetCurrentDirectory()=0;
};

class FEdLoadError;

/*----------------------------------------------------------------------------
	Global variables.
----------------------------------------------------------------------------*/

class FCallbackDevice;
class FOutputDeviceRedirector;

// Core globals.
extern FMemStack				GMem;
extern FOutputDeviceRedirectorBase*	GLog;
extern FOutputDevice*			GNull;
extern FOutputDevice*			GThrow;
extern FOutputDeviceError*		GError;
extern FFeedbackContext*		GWarn;
extern FConfigCache*			GConfig;
extern FTransactionBase*		GUndo;
extern FOutputDeviceConsole*	GLogConsole;
extern FMalloc*					GMalloc;
extern FFileManager*			GFileManager;
extern FCallbackDevice*			GCallback;
extern USystem*					GSys;
extern UProperty*				GProperty;
extern BYTE*					GPropAddr;
extern UObject*					GPropObject;
extern DWORD					GRuntimeUCFlags;
extern USubsystem*				GWindowManager;
extern class UPropertyWindowManager*	GPropertyWindowManager;
//extern USubsystem*				GPropertyWindowManager;
extern TCHAR				    GErrorHist[4096];
extern TCHAR					GTrue[64], GFalse[64], GYes[64], GNo[64], GNone[64];
extern DOUBLE					GSecondsPerCycle;
extern INT						GScriptCycles;
extern DWORD					GPageSize;
extern DWORD					GUglyHackFlags;
extern UBOOL					GIsEditor;
extern UBOOL					GIsUCC;
extern UBOOL					GIsUCCMake;
extern UBOOL					GEdSelectionLock;
extern UBOOL					GIsClient;
extern UBOOL					GIsServer;
extern UBOOL					GIsCriticalError;
extern UBOOL					GIsStarted;
extern UBOOL					GIsRunning;
extern UBOOL					GIsGarbageCollecting;
extern UBOOL					GIsSlowTask;
extern UBOOL					GIsGuarded;
extern UBOOL					GIsRequestingExit;
extern UBOOL					GLazyLoad;
extern UBOOL					GUnicode;
extern UBOOL					GUnicodeOS;
extern class FGlobalMath		GMath;
extern class FArchive*			GDummySave;
extern TArray<FEdLoadError>		GEdLoadErrors;
extern UDebugger*				GDebugger; //DEBUGGER
extern UBOOL					GIsBenchmarking;
extern QWORD					GMakeCacheIDIndex;
extern TCHAR					GEngineIni[1024];
extern TCHAR					GEditorIni[1024];
extern TCHAR					GInputIni[1024];
extern TCHAR					GGameIni[1024];
extern FLOAT					NEAR_CLIPPING_PLANE;
extern DOUBLE					GCurrentTime;
extern INT						GSRandSeed;
#ifdef XBOX
extern TCHAR					GMemoryImageName[256];
extern DWORD					GMemoryImageFlags;
#endif
extern class FCriticalSection*	GHostByNameCriticalSection;
extern TCHAR					GGameName[64];
/** Exec handler for game debugging tool, allowing commands like "editactor", ...	*/
extern FExec*					GDebugToolExec;
/** Whether we're currently cooking for console										*/
extern UBOOL					GIsConsoleCooking;

// Per module globals.
extern "C" TCHAR GPackage[];

// Normal includes.
#include "UnFile.h"						// Low level utility code.
#include "UnObjVer.h"					// Object version info.
#include "UnArc.h"						// Archive class.
#include "UnTemplate.h"					// Dynamic arrays.
#include "UnName.h"						// Global name subsystem.
#include "UnStack.h"					// Script stack definition.
#include "UnObjBas.h"					// Object base class.
#include "UnCoreNet.h"					// Core networking.
#include "UnCorObj.h"					// Core object class definitions.
#include "UnClass.h"					// Class definition.
#include "UnType.h"						// Base property type.
#include "UnScript.h"					// Script class.
#include "UFactory.h"					// Factory definition.
#include "UExporter.h"					// Exporter definition.
#include "UnMem.h"						// Stack based memory management.
#include "UnCId.h"						// Cache ID's.
#include "UnBits.h"						// Bitstream archiver.
#include "UnMath.h"						// Vector math functions.
#include "FCallbackDevice.h"			// Base class for callback devices.
#include "UnThreadingBase.h"			// Non-platform specific multi-threaded support.
#include "FOutputDeviceRedirector.h"	// Output redirector.

// Worker class for tracking loading errors in the editor
class FEdLoadError
{
public:
	FEdLoadError()
	{}
	FEdLoadError( INT InType, TCHAR* InDesc )
	{
		Type = InType;
		Desc = InDesc;
	}
	~FEdLoadError()
	{}

	// The types of things that could be missing.
	enum
	{
		TYPE_FILE		= 0,	// A totally missing file
		TYPE_RESOURCE	= 1,	// Texture/Sound/StaticMesh/etc
	};

	INT Type;		// TYPE_
	FString Desc;	// Description of the error

	UBOOL operator==( const FEdLoadError& LE ) const
	{
		return Type==LE.Type && Desc==LE.Desc;
	}
	FEdLoadError& operator=( const FEdLoadError Other )
	{
		Type = Other.Type;
		Desc = Other.Desc;
		return *this;
	}
};

//
// Archive for counting memory usage.
//
class FArchiveCountMem : public FArchive
{
public:
	FArchiveCountMem( UObject* Src )
	: Num(0), Max(0)
	{
		Src->Serialize( *this );
	}
	SIZE_T GetNum()
	{
		return Num;
	}
	SIZE_T GetMax()
	{
		return Max;
	}
	void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		Num += InNum;
		Max += InMax;
	}
protected:
	SIZE_T Num, Max;
};

enum
{
	MCTYPE_ERROR	= 0,
	MCTYPE_WARNING	= 1,
	MCTYPE_NOTE		= 2
};

typedef struct {
	INT Type;
	AActor* Actor;
	FString Message;
} MAPCHECK;

// A convenience to allow referring to axis by name instead of number

enum EAxis
{
	AXIS_None	= 0,
	AXIS_X		= 1,
	AXIS_Y		= 2,
	AXIS_Z		= 4,
	AXIS_XY		= AXIS_X|AXIS_Y,
	AXIS_XZ		= AXIS_X|AXIS_Z,
	AXIS_YZ		= AXIS_Y|AXIS_Z,
	AXIS_XYZ	= AXIS_X|AXIS_Y|AXIS_Z,
};

// Coordinate system identifiers

enum ECoordSystem
{
	COORD_None	= -1,
	COORD_World,
	COORD_Local,
};

// Very basic abstract debugger class.

class UDebugger //DEBUGGER
{
public:
	virtual ~UDebugger() {};

	virtual void  DebugInfo( const UObject* Debugee, const FFrame* Stack, BYTE OpCode, INT LineNumber, INT InputPos )=0;

	virtual void  NotifyBeginTick()=0;
	virtual void  NotifyGC()=0;
	virtual void  NotifyAccessedNone()=0;
	virtual UBOOL NotifyAssertionFailed( const INT LineNumber )=0;
	virtual UBOOL NotifyInfiniteLoop()=0;
};

#include "UnCoreNative.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

