/*=============================================================================
	UnClass.h: UClass definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifdef _MSC_VER
#pragma warning( disable : 4121 )
#endif
// vogel: alignment of a member was sensitive to packing

/*-----------------------------------------------------------------------------
	Constants.
-----------------------------------------------------------------------------*/

// Boundary to align class properties on.
#define PROPERTY_ALIGNMENT 4

#define	INVALID_OBJECT	(UObject*)-1

/*-----------------------------------------------------------------------------
	FRepRecord.
-----------------------------------------------------------------------------*/

//
// Information about a property to replicate.
//
struct FRepRecord
{
	UProperty* Property;
	INT Index;
	FRepRecord(UProperty* InProperty,INT InIndex)
	: Property(InProperty), Index(InIndex)
	{}
};

/*-----------------------------------------------------------------------------
	FRepLink.
-----------------------------------------------------------------------------*/

//
// A tagged linked list of replicatable variables.
//
class FRepLink
{
public:
	UProperty*	Property;		// Replicated property.
	FRepLink*	Next;			// Next replicated link per class.
	FRepLink( UProperty* InProperty, FRepLink* InNext )
	:	Property	(InProperty)
	,	Next		(InNext)
	{}
};

/*-----------------------------------------------------------------------------
	FLabelEntry.
-----------------------------------------------------------------------------*/

//
// Entry in a state's label table.
//
struct FLabelEntry
{
	// Variables.
	FName	Name;
	INT		iCode;

	// Functions.
	FLabelEntry() {}
	FLabelEntry( FName InName, INT iInCode );
	friend FArchive& operator<<( FArchive& Ar, FLabelEntry &Label );
};

/*-----------------------------------------------------------------------------
	UField.
-----------------------------------------------------------------------------*/

//
// Base class of UnrealScript language objects.
//
class UField : public UObject
{
	DECLARE_ABSTRACT_CASTED_CLASS_INTRINSIC(UField,UObject,0,Core,CASTCLASS_UField)

	// Variables.
	UField*			Next;

	// Constructors.
	UField(ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags);
	UField(EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags);
	UField()
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();

	// UField interface.
	virtual void AddCppProperty( UProperty* Property );
	virtual UBOOL MergeBools();
	virtual void Bind();
	UClass* GetOwnerClass() const;
	UStruct* GetOwnerStruct() const;

private:
	// Hide CreateArchetype() from UField types
	UObject* CreateArchetype( const TCHAR* ArchetypeName, UObject* ArchetypeOuter, UObject* AlternateArchetype=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL )
	{
		check(0);
		
		// call Super here so that the compiler complains if UObject::CreateArchetype signature is changed without
		// changing the corresponding signature here
		return Super::CreateArchetype(ArchetypeName, ArchetypeOuter, AlternateArchetype, InstanceGraph);
	}
};

/*-----------------------------------------------------------------------------
	UStruct.
-----------------------------------------------------------------------------*/

enum EStructFlags
{
	// State flags.
	STRUCT_Native				= 0x00000001,	
	STRUCT_Export				= 0x00000002,
	STRUCT_HasComponents		= 0x00000004,
	STRUCT_Transient			= 0x00000008,

	/** Indicates that this struct should always be serialized as a single unit */
	STRUCT_Atomic				= 0x00000010,

	/** Indicates that this struct uses binary serialization; it is unsafe to add/remove members from this struct without incrementing the package version */
	STRUCT_Immutable			= 0x00000020,

	/** Indicates that only properties marked config/globalconfig within this struct can be read from .ini */
	STRUCT_StrictConfig			= 0x00000040,

	/** Indicates that this struct will be considered immutable on platforms using cooked data. */
	STRUCT_ImmutableWhenCooked	= 0x00000080,

	/** Indicates that this struct should always be serialized as a single unit on platforms using cooked data. */
	STRUCT_AtomicWhenCooked		= 0x00000100,

	/** Struct flags that are automatically inherited */
	STRUCT_Inherit				= STRUCT_HasComponents|STRUCT_Atomic|STRUCT_AtomicWhenCooked|STRUCT_StrictConfig,
};

enum EComponentInstanceFlags
{
	/** only instance this component if it isn't found in the InstanceMap (see UStruct::InstanceComponentTemplates */
	CIF_UniqueComponentsOnly		= 0x00000001,

	/** instance this component even if it's found in the InstanceMap (see UStruct::InstanceComponentTemplates) */
	CIF_ForceInstance				= 0x00000002,

	/** instance this component only if it differs from the version that exists in the InstanceMap */
	CIF_ModifiedComponentsOnly		= 0x00000004,

	/** combo flag */
	CIF_ReinstanceComponents		= CIF_ForceInstance|CIF_ModifiedComponentsOnly,
};

/**
 * Base class for all UObject types that contain fields.
 */
class UStruct : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStruct,UField,0,Core,CASTCLASS_UStruct)
	NO_DEFAULT_CONSTRUCTOR(UStruct)

	// Variables.
#if !CONSOLE
	UTextBuffer*		ScriptText;
	UTextBuffer*		CppText;
#endif
	UStruct*			SuperStruct;
	UField*				Children;
	INT					PropertiesSize;
	TArray<BYTE>		Script;

	// Compiler info.
#if !CONSOLE
	INT					TextPos;
	INT					Line;
#endif
	INT					MinAlignment;

	// In memory only.
	UProperty*			RefLink;
	UProperty*			PropertyLink;
	UProperty*			ConstructorLink;

	/** Array of object references embedded in script code. Mirrored for easy access by realtime garbage collection code */
	TArray<UObject*>	ScriptObjectReferences;

	// Constructors.
	UStruct( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags, UStruct* InSuperStruct );
	UStruct( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags );
	explicit UStruct( UStruct* InSuperStruct );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	void FinishDestroy();
	void Register();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 * 
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	// UField interface.
	void AddCppProperty( UProperty* Property );

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
	 * @param	DefaultsCount		the size of the buffer pointed to by DefaultValue
	 * @param	Owner				the object that contains the component currently located at Data
	 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
	 */
	void InstanceComponentTemplates( BYTE* Data, BYTE* DefaultData, INT DefaultsCount, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph );

	/**
	 * Instances any UObjectProperty values that still match the default value.
	 *
	 * @param	Value				the address where the pointers to the instanced object should be stored.  This should always correspond to (for class member properties) the address of the
	 *								UObject which contains this data, or (for script structs) the address of the struct's data
	 * @param	DefaultValue		the address where the pointers to the default value is stored.  Evaluated the same way as Value
	 * @param	DefaultsCount		the size of the buffer pointed to by DefaultValue
	 * @param	OwnerObject			the object that contains the destination data.  Will be the used as the Outer for any newly instanced subobjects.
	 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
	 */
	void InstanceSubobjectTemplates( BYTE* Value, BYTE* DefaultValue, INT DefaultsCount, UObject* OwnerObject, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;

	virtual UStruct* GetInheritanceSuper() const {return GetSuperStruct();}
	virtual void Link( FArchive& Ar, UBOOL Props );
	/**
	 * Serializes the passed in property with the struct's data residing in Data.
	 *
	 * @param	Property		property to serialize
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the struct's property data
	 */
	void SerializeBinProperty( UProperty* Property, FArchive& Ar, BYTE* Data ) const;
	virtual void SerializeBin( FArchive& Ar, BYTE* Data, INT MaxReadBytes ) const;
	/**
	 * Serializes the class properties that reside in Data if they differ from the corresponding values in DefaultData
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the property data
	 * @param	DefaultData		pointer to the location of the beginning of the data that should be compared against
	 * @param	DefaultsCount	size of the block of memory located at DefaultData 
	 */
	void SerializeBinEx( FArchive& Ar, BYTE* Data, BYTE* DefaultData, INT DefaultsCount ) const;
	void SerializeTaggedProperties( FArchive& Ar, BYTE* Data, UStruct* DefaultsStruct, BYTE* Defaults, INT DefaultsCount=0 ) const;
	virtual EExprToken SerializeExpr( INT& iCode, FArchive& Ar );

	virtual void PropagateStructDefaults();

	/**
	 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
	 *
	 * @return Prefix character used for C++ declaration of this struct/ class.
	 */
	virtual const TCHAR* GetPrefixCPP() { return TEXT("F"); }

	INT GetPropertiesSize() const
	{
		return PropertiesSize;
	}
	INT GetMinAlignment() const
	{
		return MinAlignment;
	}
#if !CONSOLE
	DWORD GetScriptTextCRC()
	{
		return ScriptText ? appStrCrc(*ScriptText->Text) : 0;
	}
#endif
	void SetPropertiesSize( INT NewSize )
	{
		PropertiesSize = NewSize;
	}
	UBOOL IsChildOf( const UStruct* SomeBase ) const
	{
		for( const UStruct* Struct=this; Struct; Struct=Struct->GetSuperStruct() )
			if( Struct==SomeBase ) 
				return 1;
		return 0;
	}
	UStruct* GetSuperStruct() const
	{
		return SuperStruct;
	}
	UBOOL StructCompare( const void* A, const void* B, DWORD PortFlags=0 );
};

/**
 * An UnrealScript structure definition.
 */
class UScriptStruct : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UScriptStruct,UStruct,0,Core,CASTCLASS_UScriptStruct)
	NO_DEFAULT_CONSTRUCTOR(UScriptStruct)

	UScriptStruct( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags, UScriptStruct* InSuperStruct );
	UScriptStruct( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags );
	explicit UScriptStruct( UScriptStruct* InSuperStruct );

	FString				DefaultStructPropText;

	DWORD				StructFlags;

	// UObject Interface
	void Serialize( FArchive& Ar );
	void FinishDestroy();

	// UStruct Interface
	void PropagateStructDefaults();

	// UScriptStruct Interface
	BYTE* GetDefaults() { return &StructDefaults(0); }
	INT GetDefaultsCount() { return StructDefaults.Num(); }
	TArray<BYTE>& GetDefaultArray() { return StructDefaults; }

	void AllocateStructDefaults();

	/**
	 * Returns whether this struct should be serialized atomically.
	 *
	 * @param	Ar	Archive the struct is going to be serialized with later on
	 */
	UBOOL ShouldSerializeAtomically( FArchive& Ar )
	{
		if( (StructFlags&STRUCT_Atomic) != 0
		||	((StructFlags&STRUCT_AtomicWhenCooked) != 0 && Ar.ContainsCookedData()))
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

private:
	TArray<BYTE>		StructDefaults;
};


/*-----------------------------------------------------------------------------
	UFunction.
-----------------------------------------------------------------------------*/

//
// An UnrealScript function.
//
class UFunction : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UFunction,UStruct,0,Core,CASTCLASS_UFunction)
	DECLARE_WITHIN(UState)
	NO_DEFAULT_CONSTRUCTOR(UFunction)

	// Persistent variables.
	DWORD	FunctionFlags;
	WORD	iNative;
	WORD	RepOffset;
#if !CONSOLE
	FName	FriendlyName;				// friendly version for this function, mainly for operators
#endif
#if WITH_LIBFFI
	void*	DLLImportFunctionPtr;		// pointer to a DLL function, for FUNC_DLLImport functions
#endif
	BYTE	OperPrecedence;

	// Variables in memory only.
	BYTE	NumParms;
	WORD	ParmsSize;
	WORD	ReturnValueOffset;

	/** pointer to first local struct property in this UFunction that contains defaults */
	UStructProperty* FirstStructWithDefaults;

	void (UObject::*Func)( FFrame& Stack, RESULT_DECL );

	// Constructors.
	explicit UFunction( UFunction* InSuperFunction );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();

	// UField interface.
	void Bind();

	// UStruct interface.
	UBOOL MergeBools() {return 0;}
	UStruct* GetInheritanceSuper() const {return NULL;}
	void Link( FArchive& Ar, UBOOL Props );

	// UFunction interface.
	UFunction* GetSuperFunction() const
	{
		checkSlow(!SuperStruct||SuperStruct->IsA(UFunction::StaticClass()));
		return (UFunction*)SuperStruct;
	}
	UProperty* GetReturnProperty();

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	TRUE if the passed in flag is set, FALSE otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE UBOOL HasAnyFunctionFlags( DWORD FlagsToCheck ) const
	{
		return (FunctionFlags&FlagsToCheck) != 0 || FlagsToCheck == FUNC_AllFlags;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @return TRUE if all of the passed in flags are set (including no flags passed in), FALSE otherwise
	 */
	FORCEINLINE UBOOL HasAllFunctionFlags( DWORD FlagsToCheck ) const
	{
		return ((FunctionFlags & FlagsToCheck) == FlagsToCheck);
	}
	/**
	 * Returns function flags that are both in the mask and set on the function.
	 *
	 * @param Mask		Mask to mask FunctionFlags with
	 */
	FORCEINLINE DWORD GetMaskedFlags( DWORD Mask ) const
	{
		return FunctionFlags & Mask;
	}
};

/*-----------------------------------------------------------------------------
	UState.
-----------------------------------------------------------------------------*/

//
// An UnrealScript state.
//
class UState : public UStruct
{
	DECLARE_CASTED_CLASS_INTRINSIC(UState,UStruct,0,Core,CASTCLASS_UState)
	NO_DEFAULT_CONSTRUCTOR(UState)

	/** List of functions currently probed by the current class (see UnNames.h) */
	DWORD ProbeMask;

	/** Active state flags (see UnStack.h EStateFlags) */
	DWORD StateFlags;

	/** Offset into Script array that contains the table of FLabelEntry's */
	WORD LabelTableOffset;

	/** Map of all functions by name contained in this state */
	TMap<FName,UFunction*> FuncMap;

	// Constructors.
	UState( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags, UState* InSuperState );
	UState( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags );
	explicit UState( UState* InSuperState );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UStruct interface.
	UBOOL MergeBools() {return 1;}
	UStruct* GetInheritanceSuper() const {return GetSuperState();}

	// UState interface.
	UState* GetSuperState() const
	{
		checkSlow(!SuperStruct||SuperStruct->IsA(UState::StaticClass()));
		return (UState*)SuperStruct;
	}
};

/*-----------------------------------------------------------------------------
	UEnum.
-----------------------------------------------------------------------------*/

//
// An enumeration, a list of names usable by UnrealScript.
//
class UEnum : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC(UEnum,UField,0,Core,CASTCLASS_UEnum)
	DECLARE_WITHIN(UStruct)

protected:
	// Variables.
	TArray<FName> Names;

public:
	UEnum()
	{}
	// UObject interface.
	void Serialize( FArchive& Ar );

	/**
	 * Sets the array of enums.
	 *
	 * @return	TRUE unless the MAX enum already exists and isn't the last enum.
	 */
	UBOOL SetEnums(TArray<FName>& InNames);

	/**
	 * @return	The enum name at the specified Index.
	 */
	FName GetEnum(INT InIndex) const
	{
		if (Names.IsValidIndex(InIndex))
		{
			return Names(InIndex);
		}
		return NAME_None;
	}

	/**
	 * @return	The index of the specified name, if it exists in the enum names list.
	 */
	INT FindEnumIndex(FName InName) const
	{
		return Names.FindItemIndex( InName );
	}

	/**
	 * @return	 The number of enum names.
	 */
	INT NumEnums() const
	{
		return Names.Num();
	}

	/**
	 * Find the longest common prefix of all items in the enumeration.
	 * 
	 * @return	the longest common prefix between all items in the enum.  If a common prefix
	 *			cannot be found, returns the full name of the enum.
	 */
	FString GenerateEnumPrefix() const;

	/**
	 * Adds a virtual _MAX entry to the enum's list of names, unless the
	 * enum already contains one.
	 *
	 * @return	TRUE unless the MAX enum already exists and isn't the last enum.
	 */
	UBOOL GenerateMaxEnum();

	/**
	 * Wrapper method for easily determining whether this enum has metadata associated with it.
	 * 
	 * @param	Key			the metadata tag to check for
	 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
	 *
	 * @return TRUE if the specified key exists in the list of metadata for this enum, even if the value of that key is empty
	 */
	UBOOL HasMetaData( const TCHAR* Key, INT NameIndex=INDEX_NONE ) const;

	/**
	 * Return the metadata value associated with the specified key.
	 * 
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 *
	 * @return	the value for the key specified, or an empty string if the key wasn't found or had no value.
	 */
	const FString& GetMetaData( const TCHAR* Key, INT NameIndex=INDEX_NONE ) const;
};

/*-----------------------------------------------------------------------------
	UClass.
-----------------------------------------------------------------------------*/

#define REPLACEMENT_SUFFIX TEXT("$OLD")

/** information about an interface a class implements */
struct FImplementedInterface
{
	/** the interface class */
	UClass* Class;
	/** the pointer property that is located at the offset of the interface's vtable */
	UProperty* PointerProperty;

	FImplementedInterface()
	{}
	FImplementedInterface(UClass* InClass, UProperty* InPointer)
		: Class(InClass), PointerProperty(InPointer)
	{}

	friend FArchive& operator<<(FArchive& Ar, FImplementedInterface& A);
};

/**
 * An object class.
 */
class UClass : public UState
{
	DECLARE_CASTED_CLASS_INTRINSIC(UClass,UState,0,Core,CASTCLASS_UClass)
	DECLARE_WITHIN(UPackage)

	// Variables.
	DWORD				ClassFlags;
	DWORD				ClassCastFlags;
	INT					ClassUnique;
	UClass*				ClassWithin;
	FName				ClassConfigName;
	TArray<FRepRecord>	ClassReps;
	TArray<UField*>		NetFields;
#if !CONSOLE && !DEDICATED_SERVER
	TArray<FName>		HideCategories;
	TArray<FName>		AutoExpandCategories;
	TArray<FName>		AutoCollapseCategories;
	TArray<FName>		DontSortCategories;
	TArray<FName>       DependentOn;
	TArray<FName>		ClassGroupNames;
	UBOOL				bForceScriptOrder;
	FString				ClassHeaderFilename;
#endif
#if WITH_LIBFFI
	FName				DLLBindName;
	void*				DLLBindHandle;
#endif
	UObject*			ClassDefaultObject;

	void(*ClassConstructor)(void*);
	void(UObject::*ClassStaticConstructor)();
	void(UObject::*ClassStaticInitializer)();

	/** A mapping of the component template names inside this class to the template itself */
	TMap<FName,class UComponent*>	ComponentNameToDefaultObjectMap;

	/**
	 * The list of interfaces which this class implements, along with the pointer property that is located at the offset of the interface's vtable.
	 * If the interface class isn't native, the property will be NULL.
	 **/
	TArray<FImplementedInterface> Interfaces;

	// In memory only.
#if !CONSOLE
	FString					DefaultPropText;
#endif

	/** Indicates whether this class has been property linked (e.g. PropertyLink chain is up-to-date) */
	UBOOL					bNeedsPropertiesLinked;

	/** Reference token stream used by realtime garbage collector, finalized in AssembleReferenceTokenStream */
	FGCReferenceTokenStream ReferenceTokenStream;

	// Constructors.
	UClass();
	explicit UClass( UClass* InSuperClass );
	UClass( ENativeConstructor, DWORD InSize, DWORD InClassFlags, DWORD InClassCastFlags, UClass* InBaseClass, UClass* InWithinClass,
		const TCHAR* InNameStr, const TCHAR* InPackageName, const TCHAR* InClassConfigName, EObjectFlags InFlags, void(*InClassConstructor)(void*), void(UObject::*InClassStaticConstructor)(), void(UObject::*InClassStaticInitializer)() );
	UClass( EStaticConstructor, DWORD InSize, DWORD InClassFlags, DWORD InClassCastFlags,
		const TCHAR* InNameStr, const TCHAR* InPackageName, const TCHAR* InClassConfigName, EObjectFlags InFlags, void(*InClassConstructor)(void*), void(UObject::*InClassStaticConstructor)(), void(UObject::*InClassStaticInitializer)() );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	void FinishDestroy();
	void Register();
	virtual UBOOL Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None );


	/**
	 * Finds the component that is contained within this object that has the specified component name.
	 * This version routes the call to the class's default object.
	 *
	 * @param	ComponentName	the component name to search for
	 * @param	bRecurse		if TRUE, also searches all objects contained within this object for the component specified
	 *
	 * @return	a pointer to a component contained within this object that has the specified component name, or
	 *			NULL if no components were found within this object with the specified name.
	 */
	virtual UComponent* FindComponent( FName ComponentName, UBOOL bRecurse=FALSE );

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	// UField interface.
	void Bind();

	// UStruct interface.
	void PropagateStructDefaults();
	BYTE* GetDefaults()
	{
		return (BYTE*)GetDefaultObject();
	}
	INT GetDefaultsCount()
	{
		return ClassDefaultObject != NULL ? GetPropertiesSize() : 0;
	}
	UBOOL MergeBools() {return 1;}
	UStruct* GetInheritanceSuper() const {return GetSuperClass();}

	/**
	 * Ensures that UClass::Link() isn't called until it is valid to do so.  For intrinsic classes, this shouldn't occur
	 * until their non-intrinsic parents have been fully loaded (otherwise the intrinsic class's UProperty linked lists
	 * won't contain any properties from the parent class)
	 */
	void ConditionalLink();
	void Link( FArchive& Ar, UBOOL bRelinkExistingProperties );
	
	/**
	 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
	 *
	 * @return Prefix character used for C++ declaration of this struct/ class.
	 */
	virtual const TCHAR* GetPrefixCPP();

	/**
	 * Translates the hardcoded script config names (engine, editor, input and 
	 * game) to their global pendants and otherwise uses config(myini) name to
	 * look for a game specific implementation and creates one based on the
	 * default if it doesn't exist yet.
	 *
	 * @return	name of the class specific ini file
 	 */
	const FString GetConfigName() const
	{
		if( ClassConfigName == NAME_Engine )
		{
			return GEngineIni;
		}
		else if( ClassConfigName == NAME_Editor )
		{
			return GEditorIni;
		}
		else if( ClassConfigName == NAME_Input )
		{
			return GInputIni;
		}
		else if( ClassConfigName == NAME_Game )
		{
			return GGameIni;
		}
		else if ( ClassConfigName == NAME_UI )
		{
			return GUIIni;
		}
		else if( ClassConfigName == NAME_None )
		{
			appErrorf(TEXT("UObject::GetConfigName() called on class with config name 'None'. Class flags = %d"), ClassFlags );
			return TEXT("");
		}
		else
		{
			FString ConfigGameName		= appGameConfigDir() + FString( GGameName )  + ClassConfigName.ToString() + TEXT(".ini");
			FString ConfigDefaultName	= appGameConfigDir() + TEXT("Default") + ClassConfigName.ToString() + TEXT(".ini");

			UINT YesNoAll = ART_No;
			const UBOOL bTryToPreserveContents = FALSE;
			appCheckIniForOutdatedness( *ConfigGameName, *ConfigDefaultName, bTryToPreserveContents, YesNoAll );
			return ConfigGameName;
		}
	}
	UClass* GetSuperClass() const
	{
		return (UClass*)SuperStruct;
	}
	UObject* GetDefaultObject( UBOOL bForce = FALSE );

	template<class T>
	T* GetDefaultObject( UBOOL bForce = FALSE )
	{
		return (T*)GetDefaultObject(bForce);
	}
	class AActor* GetDefaultActor()
	{
		return (AActor*)GetDefaultObject();
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	TRUE if the passed in flag is set, FALSE otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE UBOOL HasAnyClassFlags( DWORD FlagsToCheck ) const
	{
		return (ClassFlags & FlagsToCheck) != 0;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Class flags to check for
	 * @return TRUE if all of the passed in flags are set (including no flags passed in), FALSE otherwise
	 */
	FORCEINLINE UBOOL HasAllClassFlags( DWORD FlagsToCheck ) const
	{
		return ((ClassFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		the cast flag to check for (value should be one of the EClassCastFlag enums)
	 *
	 * @return	TRUE if the passed in flag is set, FALSE otherwise
	 *			(including no flag passed in)
	 */
	FORCEINLINE UBOOL HasAnyCastFlag( EClassCastFlag FlagToCheck ) const
	{
		return (ClassCastFlags&FlagToCheck) != 0;
	}
	FORCEINLINE UBOOL HasAllCastFlags( EClassCastFlag FlagsToCheck ) const
	{
		return (ClassCastFlags&FlagsToCheck) == FlagsToCheck;
	}

	UBOOL HasNativesToExport( UObject* InOuter );


	/**
	 * Determines whether this class's memory layout is different from the C++ version of the class.
	 * 
	 * @return	TRUE if this class or any of its parent classes is marked as misaligned
	 */
	UBOOL IsMisaligned();

	virtual FString GetDesc()
	{
		return GetName();
	}
	FString GetDescription() const;

	UBOOL ChangeParentClass( UClass* NewParentClass );

	/**
	 * Realtime garbage collection helper function used to emit token containing information about a 
	 * direct UObject reference at the passed in offset.
	 *
	 * @param Offset	offset into object at which object reference is stored
	 */
	void EmitObjectReference( INT Offset );

	/**
	 * Realtime garbage collection helper function used to emit token containing information about a 
	 * an array of UObject references at the passed in offset. Handles both TArray and TTransArray.
	 *
	 * @param Offset	offset into object at which array of objects is stored
	 */
	void EmitObjectArrayReference( INT Offset );

	/**
	 * Realtime garbage collection helper function used to indicate an array of structs at the passed in 
	 * offset.
	 *
	 * @param Offset	offset into object at which array of structs is stored
	 * @param Stride	size/ stride of struct
	 * @return	index into token stream at which later on index to next token after the array is stored
	 *			which is used to skip over empty dynamic arrays
	 */
	DWORD EmitStructArrayBegin( INT Offset, INT Stride );

	/**
	 * Realtime garbage collection helper function used to indicate the end of an array of structs. The
	 * index following the current one will be written to the passed in SkipIndexIndex in order to be
	 * able to skip tokens for empty dynamic arrays.
	 *
	 * @param SkipIndexIndex
	 */
	void EmitStructArrayEnd( DWORD SkipIndexIndex );

	/**
	 * Realtime garbage collection helper function used to indicate the beginning of a fixed array.
	 * All tokens issues between Begin and End will be replayed Count times.
	 *
	 * @param Offset	offset at which fixed array starts
	 * @param Stride	Stride of array element, e.g. sizeof(struct) or sizeof(UObject*)
	 * @param Count		fixed array count
	 */
	void EmitFixedArrayBegin( INT Offset, INT Stride, INT Count );
	
	/**
	 * Realtime garbage collection helper function used to indicated the end of a fixed array.
	 */
	void EmitFixedArrayEnd();

	/**
	 * Assembles the token stream for realtime garbage collection by combining the per class only
	 * token stream for each class in the class hierarchy. This is only done once and duplicate
	 * work is avoided by using an object flag.
	 */
	void AssembleReferenceTokenStream();

	/** 
	* This will return whether or not this class implements the passed in class / interface 
	*
	* @param SomeClass - the interface to check and see if this class implements it
	**/
	inline UBOOL ImplementsInterface( const class UClass* SomeInterface ) const
	{
		if (SomeInterface != NULL && SomeInterface->HasAnyClassFlags(CLASS_Interface) && SomeInterface != UInterface::StaticClass())
		{
			for (const UClass* CurrentClass = this; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
			{
				// SomeInterface might be a base interface of our implemented interface
				for (TArray<FImplementedInterface>::TConstIterator It(CurrentClass->Interfaces); It; ++It)
				{
					const UClass* InterfaceClass = It->Class;
					if (InterfaceClass->IsChildOf(SomeInterface))
					{
						return TRUE;
					}
				}
			}
		}

		return FALSE;
	}

	/** serializes the passed in object as this class's default object using the given archive
	 * @param Object the object to serialize as default
	 * @param Ar the archive to serialize from
	 */
	void SerializeDefaultObject(UObject* Object, FArchive& Ar);

private:
	// Hide IsA because calling IsA on a class almost always indicates
	// an error where the caller should use IsChildOf.
	UBOOL IsA( UClass* Parent ) const {return UObject::IsA(Parent);}
};

/*-----------------------------------------------------------------------------
	UConst.
-----------------------------------------------------------------------------*/

//
// An UnrealScript constant.
//
class UConst : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC(UConst,UField,0,Core,CASTCLASS_UConst)
	DECLARE_WITHIN(UStruct)
	NO_DEFAULT_CONSTRUCTOR(UConst)

	// Variables.
	FString Value;

	// Constructors.
	UConst(const TCHAR* InValue);

	// UObject interface.
	void Serialize( FArchive& Ar );
};

/**
 * This struct encapsulates all data necessary when instancing components.
 */
struct FComponentInstanceParameters
{
	/** The outer-most object which is not a component */
	class UObject*						ComponentRoot;

	/** A map of component names to component instances */
	TMap<class FName,class UComponent*>	ComponentMap;

	/** The flags to pass to InstanceComponentTemplates */
	DWORD								InstanceFlags;

	/** Default Constructor */
	FComponentInstanceParameters( DWORD InInstanceFlags=CIF_UniqueComponentsOnly, UObject* InRoot=NULL )
	: ComponentRoot(InRoot), InstanceFlags(InInstanceFlags)
	{}

	FComponentInstanceParameters( UObject* InRoot )
	: ComponentRoot(InRoot), InstanceFlags(CIF_UniqueComponentsOnly)
	{}

	/**
	 * Fills this struct's ComponentMap with the components referenced by ComponentRoot which have an Outer of ComponentRoot.
	 *
	 * @param	bRequireDirectOuter		controls whether nested components will be added to the component map
	 *
	 * @return	TRUE if the map was successfully populated
	 */
	UBOOL PopulateComponentMap( UBOOL bIncludeNestedComponents=FALSE );
};

struct FObjectInstancingGraph
{
public:

	/** Default Constructor */
	FObjectInstancingGraph();

	/**
	 * Standard constructor
	 *
	 * @param	DestinationSubobjectRoot	the top-level object that is being created
	 * @param	SourceSubobjectRoot			the top-level object that is the source for the object construction; if unspecified, uses DestinationSubobjectRoot's
	 *										ObjectArchetype
	 */
	FObjectInstancingGraph( class UObject* DestinationSubobjectRoot, class UObject* SourceSubobjectRoot=NULL );

	/**
	 * Returns whether this instancing graph has a valid destination root.
	 */
	UBOOL IsInitialized() const
	{
		return DestinationRoot != NULL;
	}

	/**
	 * Sets the DestinationRoot for this instancing graph.
	 *
	 * @param	DestinationSubobjectRoot	the top-level object that is being created
	 * @param	SourceSubobjectRoot			the top-level object that is the source for the object construction; if unspecified, uses DestinationSubobjectRoot's
	 *										ObjectArchetype
	 */
	void SetDestinationRoot( class UObject* DestinationSubobjectRoot, class UObject* SourceSubobjectRoot=NULL );

	/**
	 * Finds the object instance corresponding to the specified source object.  Does not create the object if it is not found, as GetInstancedComponent does.
	 *
	 * @param	SourceObject			the object to find the corresponding instance for
	 * @param	bSearchSourceObject		by default, this method searches for an instance corresponding to a source object; specifying TRUE for this parameter
	 *									reverses the logic of the search and instead returns a source corresponding to the instance specified by the first parameter.
	 */
	class UObject* GetDestinationObject( class UObject* SourceObject, UBOOL bSearchSourceObjects=FALSE );

	/**
	 * Returns the component that has SourceComponent as its archetype, instancing the component as necessary.
	 *
	 * @param	SourceComponent		the component to find the corresponding component instance for
	 * @param	CurrentValue		the component currently assigned as the value for the component property
	 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
	 *								replaces the existing component instance in memory.
	 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
	 *								that should be the Outer for the new component.
	 *
	 * @return	if SourceComponent is contained within SourceRoot, returns a pointer to a unique component instance corresponding to
	 *			SourceComponent if SourceComponent is allowed to be instanced in this context, or NULL if the component isn't allowed to be
	 *			instanced at this time (such as when we're a client and the component isn't loaded on clients)
	 *			if SourceComponent is not contained by SourceRoot, return INVALID_OBJECT, indicating that the that has SourceComponent as its ObjectArchetype, or NULL if SourceComponent is not contained within
	 *			SourceRoot.
	 */
	class UComponent* GetInstancedComponent( class UComponent* SourceComponent, class UComponent* CurrentValue, class UObject* CurrentObject );

	/**
	 * Returns a pointer to the destination subobject root.
	 */
	class UObject* GetDestinationRoot() const
	{
		return DestinationRoot;
	}

	/**
	 * @return	pointer to the graph's source root object
	 */
	class UObject* GetSourceRoot() const
	{
		return SourceRoot;
	}

	/**
	 * Adds an object instance to the map of source objects to their instances.  If there is already a mapping for this object, it will be replaced
	 * and the value corresponding to ObjectInstance's archetype will now point to ObjectInstance.
	 *
	 * @param	ObjectInstance	the object that should be added as the corresopnding instance for ObjectSource
	 * @param	ObjectSource	the object that should be added as the key for the pair; if not specified, will use
	 *							the ObjectArchetype of ObjectInstance
	 */
	void AddObjectPair( class UObject* ObjectInstance, class UObject* ObjectSource=NULL );

	/**
	 * Adds a component template/instance pair directly to the tracking map.  Used only when pre-initializing a object instance graph, such as when loading
	 * an existing object from disk.
	 *
	 * @param	ComponentTemplate	the component that should be added as the key for this pair
	 * @param	ComponentInstance	the component instance corresponding to ComponentTemplate
	 */
	void AddComponentPair( class UComponent* ComponentTemplate, class UComponent* ComponentInstance );

	/**
	 * Removes the specified component from the component source -> instance mapping.
	 *
	 * @param	SourceComponent		the component to remove from the mapping.
	 */
	void RemoveComponent( class UComponent* SourceComponent );

	/**
	 * Clears the mapping of component templates to component instances.
	 */
	void ClearComponentMap();

	/**
	 * Retrieves a list of objects that have the specified Outer
	 *
	 * @param	SearchOuter		the object to retrieve object instances for
	 * @param	out_Components	receives the list of objects contained by SearchOuter
	 * @param	bIncludeNested	if FALSE, the output array will only contain objects that have SearchOuter as their Outer;
	 *							if TRUE, the output array will contain objects that have SearchOuter anywhere in their Outer chain
	 */
	void RetrieveObjectInstances( class UObject* SearchOuter, TArray<class UObject*>& out_Objects, UBOOL bIncludeNested=FALSE );

	/**
	 * Retrieves a list of components that have the specified Outer
	 *
	 * @param	SearchOuter		the object to retrieve components for
	 * @param	out_Components	receives the list of components contained by SearchOuter
	 * @param	bIncludeNested	if FALSE, the output array will only contain components that have SearchOuter as their Outer;
	 *							if TRUE, the output array will contain components that have SearchOuter anywhere in their Outer chain
	 */
	void RetrieveComponents( class UObject* SearchOuter, TArray<class UComponent*>& out_Components, UBOOL bIncludeNested=FALSE );

	/**
	 * Enables / disables component instancing.
	 */
	void EnableComponentInstancing( UBOOL bEnabled )
	{
		bEnableComponentInstancing = bEnabled;
	}

	/**
	 * Enables/ disables object instancing
	 */
	void EnableObjectInstancing( UBOOL bEnabled )
	{
		bEnableObjectInstancing = bEnabled;
	}

	/**
	 * Returns whether object instancing is enabled
	 */
	UBOOL IsObjectInstancingEnabled() const
	{
		return bEnableObjectInstancing;
	}

	/**
	 * Returns whether component instancing is enabled
	 */
	UBOOL IsComponentInstancingEnabled() const
	{
		return bEnableComponentInstancing;
	}

	/**
	 * Sets whether DestinationRoot is currently being loaded from disk.
	 */
	void SetLoadingObject( UBOOL bIsLoading )
	{
		bLoadingObject = bIsLoading;
	}

	/**
	 * Returns whether DestinationRoot is currently being loaded from disk.
	 */
	UBOOL IsLoadingObject() const
	{
		return bLoadingObject;
	}

	/**
	 * Returns whether DestinationRoot corresponds to an archetype object.
	 *
	 * @param	bUserGeneratedOnly	TRUE indicates that we only care about cases where the user selected "Create [or Update] Archetype" in the editor
	 *								FALSE causes this function to return TRUE even if we are just loading an archetype from disk
	 */
	UBOOL IsCreatingArchetype( UBOOL bUserGeneratedOnly=TRUE ) const
	{
		// if we only want cases where we are creating an archetype in response to user input, return FALSE if we are in fact just loading the object from disk
		return bCreatingArchetype && (!bUserGeneratedOnly || !bLoadingObject);
	}

	/**
	 * Returns whether DestinationRoot corresponds to an archetype object, and we are currently
	 * updating it from an instance of the archetype.
	 */
	UBOOL IsUpdatingArchetype() const
	{
		return bUpdatingArchetype;
	}

	/** const accessor to SourceToDestinationMap */
	FORCEINLINE const TMap<class UObject*, class UObject*>* GetSourceToDestinationMap() const
	{
		return &SourceToDestinationMap;
	}

private:
	/**
	 * The root of the object tree that is the source used for instancing components;
	 * - when placing an instance of an actor class, this would be the actor class default object
	 * - when placing an instance of an archetype, this would be the archetype
	 * - when creating an archetype/prefab, this would be the actor instance
	 * - when updating an archetype/prefab, this would be the prefab instance
	 * - when duplicating an object, this would be the duplication source
	 */
	class		UObject*						SourceRoot;

	/**
	 * The root of the object tree that is the destination used for instancing components
	 * - when placing an instance of an actor class, this would be the placed actor
	 * - when placing an instance of an archetype, this would be the placed actor
	 * - when creating an archetype/prefab, this would be the actor archetype/prefab
	 * - when updating an archetype/prefab, this would be the source prefab/archetype
	 * - when duplicating an object, this would be the copied object (destination)
	 */
	class		UObject*						DestinationRoot;

	/** The flags to pass to InstanceComponentTemplates */
	DWORD										InstanceFlags;

	/**
	 * Indicates whether we are currently instancing components for an archetype.  TRUE if we are creating or updating an archetype.
	 */
	UBOOL										bCreatingArchetype;

	/**
	 * Indicates whether we are updating an archetype.
	 */
	UBOOL										bUpdatingArchetype;

	/**
	 * If FALSE, components will not be instanced.
	 */
	UBOOL										bEnableComponentInstancing;

	/**
	 * If FALSE, subobjects referenced via properties marked as editinline+export/instanced will not be instanced
	 */
	UBOOL										bEnableObjectInstancing;

	/**
	 * TRUE when loading object data from disk.
	 */
	UBOOL										bLoadingObject;

	/**
	 * Tracks the mapping of 
	 */
	TMap<class UObject*,class UObject*>			SourceToDestinationMap;

	/**
	 * Tracks the mapping of unique component templates to the corresponding unique component instance.
	 */
	TMap<class UComponent*,class UComponent*>	ComponentInstanceMap;
};

/**
 * Construct an object of a particular class.
 * 
 * @param	Class		the class of object to construct
 * @param	Outer		the outer for the new object.  If not specified, object will be created in the transient package.
 * @param	Name		the name for the new object.  If not specified, the object will be given a transient name via
 *						MakeUniqueObjectName
 * @param	SetFlags	the object flags to apply to the new object
 * @param	Template	the object to use for initializing the new object.  If not specified, the class's default object will
 *						be used
 * @param	SubobjectRoot
 *						Only used to when duplicating or instancing objects; in a nested subobject chain, corresponds to the first object that is not a subobject.
 *						A value of INVALID_OBJECT for this parameter indicates that we are calling StaticConstructObject to duplicate or instance a non-subobject (which will be the subobject root for any subobjects of the new object)
 *						A value of NULL indicates that we are not instancing or duplicating an object.
 * @param	InstanceGraph
 *						contains the mappings of instanced objects and components to their templates
 *
 * @return	a pointer of type T to a new object of the specified class
 *
 * GCC 3.4 - had to move this here (from UnObjBas.h) for gcc 3.4, since template code is now checked for types to be declared at template
 * declaration time, not template use time.
 */
template< class T >
T* ConstructObject(UClass* Class, UObject* Outer=INVALID_OBJECT, FName Name=NAME_None, EObjectFlags SetFlags=0, UObject* Template=NULL, UObject* SubobjectRoot=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL )
{
	checkf(Class, TEXT("ConstructObject called with a NULL class object"));
	checkSlow(Class->IsChildOf(T::StaticClass()));
	if( Outer==INVALID_OBJECT )
		Outer = UObject::GetTransientPackage();
	return (T*)UObject::StaticConstructObject(Class, Outer, Name, SetFlags, Template, GError, SubobjectRoot, InstanceGraph);
}

/**
 * Convenience template for duplicating an object
 *
 * @param SourceObject the object being copied
 * @param Outer the outer to use for the object
 * @param Name the optional name of the object
 *
 * @return the copied object or null if it failed for some reason
 */
template< class T >
T* DuplicateObject(T* SourceObject,UObject* Outer,const TCHAR* Name = TEXT("None"))
{
	if (SourceObject != NULL)
	{
		if (Outer == NULL || Outer == INVALID_OBJECT)
		{
			Outer = UObject::GetTransientPackage();
		}
		return (T*)UObject::StaticDuplicateObject(SourceObject,SourceObject,Outer,Name);
	}
	return NULL;
}

// Get default object of a class.
template< class T > 
inline const T* GetDefault()
{
	return (const T*)T::StaticClass()->GetDefaultObject();
}

// Get default object of a class.
template< class T > 
inline const T* GetDefault(UClass *Class)
{
	checkSlow(Class->GetDefaultObject()->IsA(T::StaticClass()));
	return (const T*)Class->GetDefaultObject();
}

/*-----------------------------------------------------------------------------
TFieldIterator.
-----------------------------------------------------------------------------*/

//
// For iterating through a linked list of fields.
//
template <class T>
class TFieldIterator
{
	// private class for safe bool conversion
	struct PrivateBooleanHelper { INT Value; };

protected:
	/** The object being searched for the specified field */
	const UStruct* Struct;
	/** The current location in the list of fields being iterated */
	UField* Field;
	/** Whether to include the super class or not */
	const UBOOL bShouldIncludeSuper;

public:
	TFieldIterator(const UStruct* InStruct, const UBOOL IncludeSuper = TRUE) :
		Struct( InStruct ),
		Field( InStruct ? InStruct->Children : NULL ),
		bShouldIncludeSuper(IncludeSuper)
	{
		IterateToNext();
	}
	/** conversion to "bool" returning TRUE if the iterator is valid. */
	typedef bool PrivateBooleanType;
	inline operator PrivateBooleanType() const 
	{ 
		return Field != NULL ? &PrivateBooleanHelper::Value : NULL; 
	}
	inline bool operator !() const 
	{ 
		return !operator PrivateBooleanType(); 
	}

	inline void operator++()
	{
		checkSlow(Field);
		Field = Field->Next;
		IterateToNext();
	}
	inline T* operator*()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline T* operator->()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline const UStruct* GetStruct()
	{
		return Struct;
	}
protected:
	inline void IterateToNext()
	{
		UField* CurrentField = Field;
		const UStruct* CurrentStruct = Struct;
		while( CurrentStruct )
		{
			while( CurrentField )
			{
				if( CurrentField->GetClass()->HasAllCastFlags((const EClassCastFlag)T::StaticClassCastFlags) )
				{
					Struct = CurrentStruct;
					Field = CurrentField;
					return;
				}
				CurrentField = CurrentField->Next;
			}

			if( bShouldIncludeSuper )
			{
				CurrentStruct = CurrentStruct->GetInheritanceSuper();
				if( CurrentStruct )
				{
					CurrentField = CurrentStruct->Children;
				}
			}
			else
			{
				CurrentStruct = NULL;
			}
		}

		Struct = CurrentStruct;
		Field = CurrentField;
	}
};
