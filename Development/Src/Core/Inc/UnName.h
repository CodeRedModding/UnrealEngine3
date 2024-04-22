/*=============================================================================
	UnName.h: Unreal global name types.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Definitions.
----------------------------------------------------------------------------*/

/** Maximum size of name. */
enum {NAME_SIZE	= 1024};

/** Name index. */
typedef INT NAME_INDEX;

/** Externally, the instance number to represent no instance number is NAME_NO_NUMBER, 
    but internally, we add 1 to indices, so we use this #define internally for 
	zero'd memory initialization will still make NAME_None as expected */
#define NAME_NO_NUMBER_INTERNAL	0

/** Conversion routines between external representations and internal */
#define NAME_INTERNAL_TO_EXTERNAL(x) (x - 1)
#define NAME_EXTERNAL_TO_INTERNAL(x) (x + 1)

/** Special value for an FName with no number */
#define NAME_NO_NUMBER NAME_INTERNAL_TO_EXTERNAL(NAME_NO_NUMBER_INTERNAL)


namespace FNameDefs
{
#if CONSOLE
	// Use a modest bucket count on consoles
	static const UINT NameHashBucketCount = 4096;
#else
	// On PC platform we use a large number of name hash buckets to accommodate the editor's
	// use of FNames to store asset path and content tags
	static const UINT NameHashBucketCount = 65536;
#endif
}


enum ELinkerNameTableConstructor    {ENAME_LinkerConstructor};

/** Enumeration for finding name. */
enum EFindName
{
	/** Find a name; return 0 if it doesn't exist. */
	FNAME_Find,

	/** Find a name or add it if it doesn't exist. */
	FNAME_Add,

	/** Finds a name and replaces it. Adds it if missing */
	FNAME_Replace,
};

typedef QWORD EObjectFlags;	// @warning: mirrored in UnObjBas.h

/*----------------------------------------------------------------------------
	FNameEntry.
----------------------------------------------------------------------------*/

/** 
 * Mask for index bit used to determine whether string is encoded as TCHAR or ANSICHAR. We don't
 * add an extra bool in order to keep the name size to a minimum and 2 billion names is impractical
 * so there are a few bits left in the index.
 */
#define NAME_UNICODE_MASK 0x1
#define NAME_INDEX_SHIFT 1

/**
 * A global name, as stored in the global name table.
 */
struct FNameEntry
{
private:
#if SUPPORT_NAME_FLAGS
	/** RF_TagImp, RF_TagExp */
	EObjectFlags	Flags;
#endif

	/** Index of name in hash. */
	NAME_INDEX		Index;

public:
	/** Pointer to the next entry in this hash bin's linked list. */
	FNameEntry*		HashNext;

private:
	/** Name, variable-sized - note that AllocateNameEntry only allocates memory as needed. */
	union
	{
		ANSICHAR	AnsiName[NAME_SIZE];
		TCHAR		UniName[NAME_SIZE];
	};

	// DO NOT ADD VARIABLES BELOW UNION!

public:
	/** Default constructor doesn't do anything. AllocateNameEntry is responsible for work. */
	FNameEntry()
	{}

	/** 
	 * Constructor called from the linker name table serialization function. Initializes the index
	 * to a value that indicates Unicode as that's what the linker is going to serialize.
	 */
	FNameEntry( enum ELinkerNameTableConstructor )
	{
		Index = NAME_UNICODE_MASK;
	}

	/** 
	 * Sets whether or not the NameEntry will have a unicode string, or an ansi string
	 *
	 * @param bIsUnicode TRUE if we are going to serialize a unicode string
	 */
	FORCEINLINE void PreSetIsUnicodeForSerialization(UBOOL bIsUnicode)
	{
		Index = bIsUnicode ? NAME_UNICODE_MASK : 0;
	}

	/**
	 * Returns index of name in hash passed to FNameEntry via AllocateNameEntry. The lower bits
	 * are used for internal state, which is why we need to shift.
	 *
	 * @return Index of name in hash
	 */
	FORCEINLINE INT GetIndex() const
	{
		return Index >> NAME_INDEX_SHIFT;
	}

	/**
	 * Returns whether this name entry is represented via TCHAR or ANSICHAR
	 */
	FORCEINLINE UBOOL IsUnicode() const
	{
		return (Index & NAME_UNICODE_MASK);
	}

	/**
	 * @return FString of name portion minus number.
	 */
	FString GetNameString() const;

	/**
	 * Appends this name entry to the passed in string.
	 *
	 * @param	String	String to append this name to
	 */
	void AppendNameToString( FString& String ) const;

	/**
	 * @return case insensitive hash of name
	 */
	DWORD GetNameHash() const;

	/**
	 * @return length of name
	 */
	INT GetNameLength() const;

	/**
	 * Compares name without looking at case.
	 *
	 * @param	InName	Name to compare to
	 * @return	TRUE if equal, FALSE otherwise
	 */
#if !TCHAR_IS_1_BYTE
	UBOOL IsEqual( const ANSICHAR* InName ) const;
#endif

	/**
	 * Compares name without looking at case.
	 *
	 * @param	InName	Name to compare to
	 * @return	TRUE if equal, FALSE otherwise
	 */
	UBOOL IsEqual( const TCHAR* InName ) const;

	/**
	 * @return direct access to ANSI name if stored in ANSI
	 */
	inline ANSICHAR* GetAnsiName()
	{
		check(!IsUnicode());
		return AnsiName;
	}

	/**
	 * @return direct access to UNICODE name if stored in UNICODE
	 */
	inline TCHAR* GetUniName()
	{
		check(IsUnicode());
		return UniName;
	}

	/**
	 * Used to safely check whether any of the passed in flags are set. This is required
	 * as EObjectFlags currently is a 64 bit data type and UBOOL is a 32 bit data type so
	 * simply using GetFlags() & RF_MyFlagBiggerThanMaxInt won't work correctly when 
	 * assigned directly to an UBOOL.
	 *
	 * @param	FlagsToCheck	Object flags to check for.
	 * @return					TRUE if any of the passed in flags are set, FALSE otherwise  (including no flags passed in).
	 */
	UBOOL HasAnyFlags( EObjectFlags FlagsToCheck ) const;

	/**
	 * Sets the passed in flags on the name entry.
	 *
	 * @param	FlagsToSet		Flags to set.
	 */
	void SetFlags( EObjectFlags Set );

	/**
	 * Clears the passed in flags on the name entry.
	 *
	 * @param	FlagsToClear	Flags to clear.
	 */
	void ClearFlags( EObjectFlags Clear );

	/**
	 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
	 *
	 * @param	Name	Name to determine size for
	 * @return	required size of FNameEntry structure to hold this string (might be unicode or ansi)
	 */
	static INT GetSize( const TCHAR* Name );

	/**
	 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
	 *
	 * @param	Length			Length of name
	 * @param	bIsPureAnsi		Whether name is pure ANSI or not
	 * @return	required size of FNameEntry structure to hold this string (might be unicode or ansi)
	 */
	static INT GetSize( INT Length, UBOOL bIsPureAnsi );

	// Functions.
	friend FArchive& operator<<( FArchive& Ar, FNameEntry& E );

	// Friend for access to Flags.
	friend FNameEntry* AllocateNameEntry( const void* Name, NAME_INDEX Index, FNameEntry* HashNext, UBOOL bIsPureAnsi );
};

FNameEntry* AllocateNameEntry( const void* Name, NAME_INDEX Index, FNameEntry* HashNext, UBOOL bIsPureAnsi );

/*----------------------------------------------------------------------------
	FName.
----------------------------------------------------------------------------*/

#define checkName checkSlow

/**
 * Public name, available to the world.  Names are stored as a combination of
 * an index into a table of unique strings and an instance number.
 * Names are case-insensitive.
 */
class FName 
{
public:

	/** @name Accessors */
	//@{
	NAME_INDEX GetIndex() const
	{
		checkName(Index >= 0 && Index < Names.Num());
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher: we will want to create nother function which is GetRawIndex() and use that for the ScriptPatching calls to GetIndex()
		// the names are not updated when we are just reading in INT values and not actually creating the FNames
		if( !GIsScriptPatcherActive ) 
#endif
		{
			checkName(Names(Index));
		}

		return Index;
	}
	INT GetNumber() const;
	const FString GetNameString() const
	{
		return Names(Index)->GetNameString();
	}
	//@}

	/**
	 * Converts an FName to a readable format
	 *
	 * @return String representation of the name
	 */
	FString ToString() const;

	/**
	 * Converts an FName to a readable format, in place
	 * 
	 * @param Out String to fill ot with the string representation of the name
	 */
	void ToString(FString& Out) const;

	/**
	 * Converts an FName to a readable format, in place, appending to an existing string (ala GetFullName)
	 * 
	 * @param Out String to append with the string representation of the name
	 */
	void AppendString(FString& Out) const;

	/** @name Name flags */
	//@{
	/**
	 * Used to safely check whether the passed in flag is set. This is required as 
	 * EObjectFlags currently is a 64 bit data type and UBOOL is a 32 bit data type so
	 * simply using GetFlags() & RF_MyFlagBiggerThanMaxInt won't work correctly when 
	 * assigned directly to an UBOOL.
	 *
	 * @param	FlagToCheck		Object flag to check for.
	 * @return					TRUE if the passed in flag is set, FALSE otherwise (including no flag passed in).
	 */
	UBOOL HasAnyFlags( EObjectFlags FlagToCheck ) const
	{
		return Names(Index)->HasAnyFlags( FlagToCheck );
	}
	void SetFlags( EObjectFlags Set ) const
	{
		checkName(Index < Names.Num());
		checkName(Names(Index));
		Names(Index)->SetFlags( Set );
	}
	void ClearFlags( EObjectFlags Clear ) const
	{
		checkName(Index < Names.Num());
		checkName(Names(Index));
		Names(Index)->ClearFlags( Clear );
	}
	//@}

	FORCEINLINE UBOOL operator==( const FName& Other ) const
	{
		/** Make sure FName compares are a single 64-bit compare on platforms where it makes sense. */
#if XBOX || PS3 || _WIN64
		return ( *((QWORD*)this) == *((QWORD*)&Other) );
#else
		return Index == Other.Index && Number == Other.Number;
#endif
	}
	FORCEINLINE UBOOL operator!=( const FName& Other ) const
	{
		/** Make sure FName compares are a single 64-bit compare on platforms where it makes sense. */
#if XBOX || PS3 || _WIN64
		return ( *((QWORD*)this) != *((QWORD*)&Other) );
#else
		return Index != Other.Index || Number != Other.Number;
#endif
	}
	UBOOL IsValid() const
	{
		return Index>=0 && Index<Names.Num() && Names(Index)!=NULL;
	}

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
	// Assignment operator
	inline FName& operator=(const FName& Other)
	{
		this->Index = Other.Index;
		this->Number = Other.Number;

		return *this;
	}
#endif

	/**
	 * Compares name to passed in one. Sort is alphabetical ascending.
	 *
	 * @param	Other	Name to compare this against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	INT Compare( const FName& Other ) const;

	/**
	 * Create an FName with a hardcoded string index.
	 *
	 * @param N The harcdcoded value the string portion of the name will have. The number portion will be NAME_NO_NUMBER
	 */
	FName( enum EName N )
	: Index( N )
	, Number( NAME_NO_NUMBER_INTERNAL )
	{}

	/**
	 * Create an FName with a hardcoded string index and (instance).
	 *
	 * @param N The harcdcoded value the string portion of the name will have
	 * @param InNumber The hardcoded value for the number portion of the name
	 */
	FName( enum EName N, INT InNumber )
	: Index( N )
	, Number( InNumber )
	{}

	/**
	 * Create an uninitialized FName
	 */
	FName()
	{}

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name			Value for the string portion of the name
	 * @param FindType		Action to take (see EFindName)
	 * @param unused
	 */
	FName( const TCHAR* Name, EFindName FindType=FNAME_Add, UBOOL bUnused=TRUE );
#if !TCHAR_IS_1_BYTE
	FName( const ANSICHAR* Name, EFindName FindType=FNAME_Add );
#endif

	/**
	 * Constructor that allows for (FName)0 (see TMap.FindRef) that won't conflict with 
	 * the above TCHAR* / ANSICHAR* constructors
	 */
	explicit FName( INT SomeInt )
	{
		checkSlow(SomeInt == 0);
		Index = Number = 0;
	}

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name Value for the string portion of the name
	 * @param Number Value for the number portion of the name
	 * @param FindType Action to take (see EFindName)
	 */
	FName( const TCHAR* Name, INT InNumber, EFindName FindType=FNAME_Add );

	/**
	 * Constructor used by ULinkerLoad when loading its name table; Creates an FName with an instance
	 * number of 0 that does not attempt to split the FName into string and number portions.
	 */
	FName( ELinkerNameTableConstructor, const TCHAR* Name );

	/**
	 * Constructor used by ULinkerLoad when loading its name table; Creates an FName with an instance
	 * number of 0 that does not attempt to split the FName into string and number portions.
	 */
#if !TCHAR_IS_1_BYTE
	FName( ELinkerNameTableConstructor, const ANSICHAR* Name );
#endif

	/**
	 * Comparision operator.
	 *
	 * @param	Other	String to compare this name to
	 * @return TRUE if name matches the string, FALSE otherwise
	 */
	UBOOL operator==( const TCHAR * Other ) const;

	/** @name Name subsystem */
	//@{
	static void StaticInit();
	static void DisplayHash( class FOutputDevice& Ar );
	static void Hardcode( FNameEntry* AutoName );
	//@}

	/** @name Name subsystem accessors */
	//@{
	static FString SafeString( EName Index, INT InstanceNumber=NAME_NO_NUMBER_INTERNAL )
	{
		return GetIsInitialized()
			? (Names.IsValidIndex(Index) && Names(Index))
				? FName(Index, InstanceNumber).ToString()
				: FString(TEXT("*INVALID*"))
			: FString(TEXT("*UNINITIALIZED*"));
	}
	static UBOOL SafeSuppressed( EName Index )
	{
#ifdef RF_Suppress
		#error
#else
		#define RF_Suppress				DECLARE_UINT64(0x0000100000000000)		// @warning: Mirrored in UnObjBas.h. Suppressed log name.
#endif
#if SUPPORT_NAME_FLAGS
		return GetIsInitialized() && (Names(Index)->HasAnyFlags(RF_Suppress));
#else
		// Don't suppress data if name flags are not supported.
		return FALSE;
#endif
#undef RF_Suppress
	}
	static INT GetMaxNames()
	{
		return Names.Num();
	}
	/**
	 * @return Size of all name entries.
	 */
	static INT GetNameEntryMemorySize()
	{
		return NameEntryMemorySize;
	}
	/**
	* @return Size of Name Table object as a whole
	*/
	static INT GetNameTableMemorySize()
	{
		return GetNameEntryMemorySize() + GetMaxNames() * sizeof(FNameEntry*) + sizeof(NameHash);
	}

	/**
	 * @return number of ansi names in name table
	 */
	static INT GetNumAnsiNames()
	{
		return NumAnsiNames;
	}
	/**
	 * @return number of unicode names in name table
	 */
	static INT GetNumUnicodeNames()
	{
		return NumUnicodeNames;
	}
	static FNameEntry* GetEntry( int i )
	{
		return Names(i);
	}
	static UBOOL GetInitialized()
	{
		return GetIsInitialized();
	}
	//@}

	/**
	 * Helper function to split an old-style name (Class_Number, ie Rocket_17) into
	 * the component parts usable by new-style FNames
	 *
	 * @param OldName		Old-style name
	 * @param NewName		Ouput string portion of the name/number pair
	 * @param NewNumber		Number portion of the name/number pair
	 */
	static void SplitOldName(const TCHAR* OldName, FString& NewName, INT& NewNumber);

	/**
	 * Helper function to split an old-style name (Class_Number, ie Rocket_17) into
	 * the component parts usable by new-style FNames. Only use results if this function
	 * returns TRUE.
	 *
	 * @param OldName		Old-style name
	 * @param NewName		Ouput string portion of the name/number pair
	 * @param NewNameLen	Size of NewName buffer (in TCHAR units)
	 * @param NewNumber		Number portion of the name/number pair
	 *
	 * @return TRUE if the name was split, only then will NewName/NewNumber have valid values
	 */
	static UBOOL SplitNameWithCheck(const TCHAR* OldName, TCHAR* NewName, INT NewNameLen, INT& NewNumber);

private:

	/** Index into the Names array (used to find String portion of the string/number pair) */
	NAME_INDEX	Index;

	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	INT			Number;

	// Static subsystem variables.
	/** Table of all names. This is a NoInit because we may need to use it before the constructor is called due to random static variable initialization order */
	static TArrayNoInit<FNameEntry*>	Names;
	/** Name hash.												*/
	static FNameEntry*					NameHash[ FNameDefs::NameHashBucketCount ];
	/** Size of all name entries.								*/
	static INT							NameEntryMemorySize;	
	/** Number of ANSI names in name table.						*/
	static INT							NumAnsiNames;			
	/** Number of Unicode names in name table.					*/
	static INT							NumUnicodeNames;

	/**
	 * Return the static initialized flag. Must be in a function like this so we don't have problems with 
	 * different initialization order of static variables across the codebase. Use this function to get or set the variable.
	 */
PRAGMA_DISABLE_INLINING
	FORCENOINLINE static UBOOL& GetIsInitialized()
	{
		static UBOOL bIsInitialized = 0;
		return bIsInitialized;
	}
PRAGMA_ENABLE_INLINING

	friend const TCHAR* DebugFName(INT);
	friend const TCHAR* DebugFName(INT, INT);
	friend const TCHAR* DebugFName(FName&);
	friend const TCHAR* DebugFName(UObject*);
	friend FNameEntry* AllocateNameEntry( const void* Name, NAME_INDEX Index, FNameEntry* HashNext, UBOOL bIsPureAnsi );

	/**
	 * Shared initialization code (between two constructors)
	 * 
	 * @param InName String name of the name/number pair
	 * @param InNumber Number part of the name/number pair
	 * @param FindType Operation to perform on names
	 * @param bSplitName If TRUE, this function will attempt to split a number off of the string portion (turning Rocket_17 to Rocket and number 17)
	 */
	void Init(const TCHAR* InName, INT InNumber, EFindName FindType, UBOOL bSplitName=TRUE);

	/**
	 * Optimized initialization code with the name is known to be pure ansi already. Name splitting IS NOT SUPPORTED in this 
	 * version of the function
	 * 
	 * @param InName		String name of the name/number pair
	 * @param InNumber		Number part of the name/number pair
	 * @param FindType		Operation to perform on names
	 */
#if !TCHAR_IS_1_BYTE
	void Init(const ANSICHAR* InName, INT InNumber, EFindName FindType );
#endif

public:
	/**
	 * This function is only for use by debugging code; it should never be called otherwise.
	 */
	static TArray<FNameEntry*>&	GetPureNamesTable(void)
	{
		return FName::Names;
	}
};

inline DWORD GetTypeHash( const FName N )
{
#if XBOX || PS3 
	// A very specific PPC optimization to fix a LHS with TMap<FName,...>
	return DWORD((*((QWORD*)&N))>>32);
#else
	return N.GetIndex();
#endif
}

/**
 * Comparison operator with TCHAR* on left hand side and FName on right hand side
 * 
 * @param	LHS		TCHAR to compare to FName
 * @param	RHS		FName to compare to TCHAR
 * @return TRUE if strings match, FALSE otherwise
 */
inline UBOOL operator==( const TCHAR *LHS, const FName &RHS )
{
	return RHS == LHS;
}

/** FNames act like PODs. */
template <> struct TIsPODType<FName> { enum { Value = true }; };
