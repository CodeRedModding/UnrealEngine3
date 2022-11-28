/*=============================================================================
	UnClass.h: UClass definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney.
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
	DECLARE_ABSTRACT_CLASS(UField,UObject,0,Core)
	NO_DEFAULT_CONSTRUCTOR(UField)

	// Variables.
	UField*			SuperField;
	UField*			Next;

	// Constructors.
	UField( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UField* InSuperField );
	UField( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags );
	UField( UField* InSuperField );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	void Register();

	// UField interface.
	virtual void AddCppProperty( UProperty* Property );
	virtual UBOOL MergeBools();
	virtual void Bind();
	virtual UClass* GetOwnerClass();
	virtual INT GetPropertiesSize();
};

/*-----------------------------------------------------------------------------
	TFieldIterator.
-----------------------------------------------------------------------------*/

//
// For iterating through a linked list of fields.
//
template <class T, EClassFlags Flag=CLASS_None, UBOOL IncludeSuper=1> class TFieldIterator 
{
public:
	TFieldIterator( UStruct* InStruct )
	: Struct( InStruct )
	, Field( InStruct ? InStruct->Children : NULL )
	{
		IterateToNext();
	}
	inline operator UBOOL()
	{
		return Field != NULL;
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
	inline UStruct* GetStruct()
	{
		return Struct;
	}
protected:
	inline void IterateToNext()
	{
		if ( Flag != CLASS_None )
		{
			while( Struct )
			{
				while( Field )
				{
					if( (Field->GetClass()->ClassFlags & Flag) )
						return;
					Field = Field->Next;
				}
				if( IncludeSuper )
				{
					Struct = Struct->GetInheritanceSuper();
					if( Struct )
						Field = Struct->Children;
				}
				else
					Struct = NULL;
			}
		}
		else
		{
			while( Struct )
			{
				while( Field )
				{
					if( Field->IsA(T::StaticClass()) )
						return;
					Field = Field->Next;
				}
				if( IncludeSuper )
				{
					Struct = Struct->GetInheritanceSuper();
					if( Struct )
						Field = Struct->Children;
				}
				else
					Struct = NULL;
			}
		}
	}
	UStruct* Struct;
	UField* Field;
};

/*-----------------------------------------------------------------------------
	UStruct.
-----------------------------------------------------------------------------*/

enum EStructFlags
{
	// State flags.
	STRUCT_Native			= 0x00000001,	
	STRUCT_Export			= 0x00000002,
	STRUCT_HasComponents	= 0x00000004,
};

//
// An UnrealScript structure definition.
//
class UStruct : public UField
{
	DECLARE_CLASS(UStruct,UField,0,Core)
	NO_DEFAULT_CONSTRUCTOR(UStruct)

	// Variables.
	UTextBuffer*		ScriptText;
	UTextBuffer*		CppText;
	UField*				Children;
	INT					PropertiesSize;
	TArray<BYTE>		Script;
	TArray<BYTE>		Defaults;

	// Compiler info.
	INT					TextPos;
	INT					Line;
	DWORD				StructFlags;
	INT					MinAlignment;

	// In memory only.
	UProperty*			RefLink;
	UProperty*			PropertyLink;
	UProperty*			ConfigLink;
	UProperty*			ConstructorLink;
	FString				DefaultStructPropText;

	// Constructors.
	UStruct( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UStruct* InSuperStruct );
	UStruct( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags );
	UStruct( UStruct* InSuperStruct );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	void Destroy();

	// UField interface.
	void AddCppProperty( UProperty* Property );

	// UStruct interface.
	virtual void FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner );
	virtual void InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual UStruct* GetInheritanceSuper() {return GetSuperStruct();}
	virtual void Link( FArchive& Ar, UBOOL Props );
	virtual void SerializeBin( FArchive& Ar, BYTE* Data, INT MaxReadBytes );
	virtual void SerializeTaggedProperties( FArchive& Ar, BYTE* Data, UStruct* DefaultsStruct );
	virtual void CleanupDestroyed( BYTE* Data, UObject* Owner );
	virtual EExprToken SerializeExpr( INT& iCode, FArchive& Ar );

	/**
	 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
	 *
	 * @return Prefix character used for C++ declaration of this struct/ class.
	 */
	virtual const TCHAR* GetPrefixCPP() { return TEXT("F"); }

	void PropagateStructDefaults();
	void AllocateStructDefaults();
	INT GetPropertiesSize()
	{
		return PropertiesSize;
	}
	INT GetMinAlignment()
	{
		return MinAlignment;
	}
	DWORD GetScriptTextCRC()
	{
		return ScriptText ? appStrCrc(*ScriptText->Text) : 0;
	}
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
		checkSlow(!SuperField||SuperField->IsA(UStruct::StaticClass()));
		return (UStruct*)SuperField;
	}
	UBOOL StructCompare( const void* A, const void* B );
};

/*-----------------------------------------------------------------------------
	UFunction.
-----------------------------------------------------------------------------*/

//
// An UnrealScript function.
//
class UFunction : public UStruct
{
	DECLARE_CLASS(UFunction,UStruct,CLASS_IsAUFunction,Core)
	DECLARE_WITHIN(UState)
	NO_DEFAULT_CONSTRUCTOR(UFunction)

	// Persistent variables.
	DWORD FunctionFlags;
	_WORD iNative;
	_WORD RepOffset;
	BYTE  OperPrecedence;
	FName FriendlyName;				// friendly version for this function, mainly for operators

	// Variables in memory only.
	BYTE  NumParms;
	_WORD ParmsSize;
	_WORD ReturnValueOffset;
	void (UObject::*Func)( FFrame& TheStack, RESULT_DECL );

	// Constructors.
	UFunction( UFunction* InSuperFunction );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();

	// UField interface.
	void Bind();

	// UStruct interface.
	UBOOL MergeBools() {return 0;}
	UStruct* GetInheritanceSuper() {return NULL;}
	void Link( FArchive& Ar, UBOOL Props );

	// UFunction interface.
	UFunction* GetSuperFunction() const
	{
		checkSlow(!SuperField||SuperField->IsA(UFunction::StaticClass()));
		return (UFunction*)SuperField;
	}
	UProperty* GetReturnProperty();
};

/*-----------------------------------------------------------------------------
	UState.
-----------------------------------------------------------------------------*/

//
// An UnrealScript state.
//
class UState : public UStruct
{
	DECLARE_CLASS(UState,UStruct,CLASS_IsAUState,Core)
	NO_DEFAULT_CONSTRUCTOR(UState)

	/** List of functions currently probed by the current class (see UnNames.h) */
	QWORD ProbeMask;

	/** List of functions currently ignored by the current statenode (see UnNames.h) */
	QWORD IgnoreMask;

	/** Active state flags (see UnStack.h EStateFlags) */
	DWORD StateFlags;

	/** Offset into Script array that contains the table of FLabelEntry's */
	_WORD LabelTableOffset;

	/** Map of all functions by name contained in this state */
	TMap<FName,UFunction*> FuncMap;

	// Constructors.
	UState( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UState* InSuperState );
	UState( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags );
	UState( UState* InSuperState );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();

	// UStruct interface.
	UBOOL MergeBools() {return 1;}
	UStruct* GetInheritanceSuper() {return GetSuperState();}

	// UState interface.
	UState* GetSuperState() const
	{
		checkSlow(!SuperField||SuperField->IsA(UState::StaticClass()));
		return (UState*)SuperField;
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
	DECLARE_CLASS(UEnum,UField,0,Core)
	DECLARE_WITHIN(UStruct)
	NO_DEFAULT_CONSTRUCTOR(UEnum)

	// Variables.
	TArray<FName> Names;

	// Constructors.
	UEnum( UEnum* InSuperEnum );

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UEnum interface.
	UEnum* GetSuperEnum() const
	{
		checkSlow(!SuperField||SuperField->IsA(UEnum::StaticClass()));
		return (UEnum*)SuperField;
	}
};

/*-----------------------------------------------------------------------------
	UClass.
-----------------------------------------------------------------------------*/

//
// An object class.
//
class UClass : public UState
{
	DECLARE_CLASS(UClass,UState,0,Core)
	DECLARE_WITHIN(UPackage)

	// Variables.
	DWORD				ClassFlags;
	INT					ClassUnique;
	FGuid				ClassGuid;
	UClass*				ClassWithin;
	FName				ClassConfigName;
	TArray<FRepRecord>	ClassReps;
	TArray<UField*>		NetFields;
	TArray<FName>		PackageImports;
	TArray<FName>		HideCategories;
	TArray<FName>		AutoExpandCategories;
    TArray<FName>       DependentOn;
	FString				ClassHeaderFilename;
	void(*ClassConstructor)(void*);
	void(UObject::*ClassStaticConstructor)();

	UClass*					ComponentOwnerClass;
	TMap<UClass*,FName>		ComponentClassToNameMap;
	TMap<FName,UComponent*>	ComponentNameToDefaultObjectMap;

	// In memory only.
	FString				DefaultPropText;

	// Constructors.
	UClass();
	UClass( UClass* InSuperClass );
	UClass( ENativeConstructor, DWORD InSize, DWORD InClassFlags, UClass* InBaseClass, UClass* InWithinClass, FGuid InGuid, const TCHAR* InNameStr, const TCHAR* InPackageName, const TCHAR* InClassConfigName, DWORD InFlags, void(*InClassConstructor)(void*), void(UObject::*InClassStaticConstructor)() );
	UClass( EStaticConstructor, DWORD InSize, DWORD InClassFlags, FGuid InGuid, const TCHAR* InNameStr, const TCHAR* InPackageName, const TCHAR* InClassConfigName, DWORD InFlags, void(*InClassConstructor)(void*), void(UObject::*InClassStaticConstructor)() );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	void Destroy();
	void Register();

	// UField interface.
	void Bind();

	// UStruct interface.
	UBOOL MergeBools() {return 1;}
	UStruct* GetInheritanceSuper() {return GetSuperClass();}
	void Link( FArchive& Ar, UBOOL Props );
	
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
		else if( ClassConfigName == NAME_None )
		{
			appErrorf(TEXT("UObject::GetConfigName() called on class with config name 'None'. Class flags = %d"), ClassFlags );
			return TEXT("");
		}
		else
		{
			FString ConfigGameName		= appGameConfigDir() + GGameName + *ClassConfigName + TEXT(".ini");
			FString ConfigDefaultName	= appGameConfigDir() + TEXT("Default") + *ClassConfigName + TEXT(".ini");
			appAssembleIni( *ConfigGameName, *ConfigDefaultName );
			return ConfigGameName;
		}
	}
	UClass* GetSuperClass() const
	{
		return (UClass *)SuperField;
	}
	UObject* GetDefaultObject()
	{
		check(Defaults.Num()==GetPropertiesSize());
		return (UObject*)&Defaults(0);
	}
	class AActor* GetDefaultActor()
	{
		check(Defaults.Num());
		return (AActor*)&Defaults(0);
	}
	UBOOL HasNativesToExport( UObject* Outer )
	{
		if( GetFlags() & RF_Native )
		{
			if( GetFlags() & RF_TagExp )
				return 1;		
			if( ScriptText && GetOuter() == Outer )
				for( TFieldIterator<UFunction> Function(this); Function && Function.GetStruct()==this; ++Function )
					if( Function->FunctionFlags & FUNC_Native )
						return 1;
		}
		return 0;			
	}

	virtual FString GetDesc()
	{
		return GetName();
	}
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz )
	{
	}
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz )
	{
		FThumbnailDesc td;
		td.Width = td.Height = 0;
		return td;
	}
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels )
	{
		InLabels->Empty();
		return 0;
	}

	FString GetDescription() const;

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
	DECLARE_CLASS(UConst,UField,0,Core)
	DECLARE_WITHIN(UStruct)
	NO_DEFAULT_CONSTRUCTOR(UConst)

	// Variables.
	FString Value;

	// Constructors.
	UConst( UConst* InSuperConst, const TCHAR* InValue );

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UConst interface.
	UConst* GetSuperConst() const
	{
		checkSlow(!SuperField||SuperField->IsA(UConst::StaticClass()));
		return (UConst*)SuperField;
	}
};
	void Serialize( FArchive& Ar );  

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

