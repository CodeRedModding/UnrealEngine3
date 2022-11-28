/*=============================================================================
	UnType.h: Unreal engine base type definitions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	UProperty.
-----------------------------------------------------------------------------*/

// Property exporting flags.
enum EPropertyPortFlags
{
	PPF_Localized = 1,
	PPF_Delimited = 2,
	PPF_CheckReferences = 4, 
};

//
// An UnrealScript variable.
//
class UProperty : public UField
{
	DECLARE_ABSTRACT_CLASS(UProperty,UField,CLASS_IsAUProperty,Core)
	DECLARE_WITHIN(UField)

	// Persistent variables.
	INT			ArrayDim;
	INT			ElementSize;
	QWORD		PropertyFlags;
	FName		Category;
	_WORD		RepOffset;
	_WORD		RepIndex;

	// In memory variables.
	INT			Offset;
	UProperty*	PropertyLinkNext;
	UProperty*	ConfigLinkNext;
	UProperty*	ConstructorLinkNext;
	UProperty*  NextRef;
	UProperty*	RepOwner;

	// Constructors.
	UProperty();
	UProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags );

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	virtual void CleanupDestroyed( BYTE* Data, UObject* Owner ) const;
	virtual void Link( FArchive& Ar, UProperty* Prev );
	virtual UBOOL Identical( const void* A, const void* B ) const=0;
	virtual void ExportCpp( FOutputDevice& Out, UBOOL IsLocal, UBOOL IsParm, UBOOL IsEvent, UBOOL IsStruct ) const;
	virtual void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const=0;
	virtual void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes=0 ) const=0;
	virtual UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	virtual void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const {}
	virtual void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const=0;
	virtual UBOOL ExportText( INT ArrayElement, FString& ValueStr, BYTE* Data, BYTE* Delta, UObject* Parent, INT PortFlags ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	virtual void DestroyValue( void* Dest ) const;
	virtual UBOOL Port() const;
	virtual BYTE GetID() const;
	virtual void FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner ) {}
	virtual void FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner ) {}
	virtual void InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner ) {}
	virtual void FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner ) {}

	// Inlines.
	UBOOL Matches( const void* A, const void* B, INT ArrayIndex ) const
	{
		INT Ofs = Offset + ArrayIndex * ElementSize;
		return Identical( (BYTE*)A + Ofs, B ? (BYTE*)B + Ofs : NULL );
	}
	INT GetSize() const
	{
		return ArrayDim * ElementSize;
	}
	UBOOL ShouldSerializeValue( FArchive& Ar ) const
	{
		UBOOL Skip
		=	((PropertyFlags & CPF_Native   )                      )
		||	((PropertyFlags & CPF_NonTransactional) && Ar.IsTrans())
		||	((PropertyFlags & CPF_Transient) && Ar.IsPersistent() )
		||	((PropertyFlags & CPF_Deprecated) && ( Ar.IsSaving() || Ar.IsTrans() )	);
		return !Skip;
	}
};

/*-----------------------------------------------------------------------------
	UByteProperty.
-----------------------------------------------------------------------------*/

//
// Describes an unsigned byte value or 255-value enumeration variable.
//
class UByteProperty : public UProperty
{
	DECLARE_CLASS(UByteProperty,UProperty,CLASS_IsAUProperty,Core)

	// Variables.
	UEnum* Enum;

	// Constructors.
	UByteProperty()
	{}
	UByteProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UEnum* InEnum=NULL )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	Enum( InEnum )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
};

/*-----------------------------------------------------------------------------
	UIntProperty.
-----------------------------------------------------------------------------*/

//
// Describes a 32-bit signed integer variable.
//
class UIntProperty : public UProperty
{
	DECLARE_CLASS(UIntProperty,UProperty,CLASS_IsAUProperty,Core)

	// Constructors.
	UIntProperty()
	{}
	UIntProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
};

/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

//
// Describes a single bit flag variable residing in a 32-bit unsigned double word.
//
class UBoolProperty : public UProperty
{
	DECLARE_CLASS(UBoolProperty,UProperty,CLASS_IsAUProperty|CLASS_IsAUBoolProperty,Core)

	// Variables.
	BITFIELD BitMask;

	// Constructors.
	UBoolProperty()
	{}
	UBoolProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	BitMask( 1 )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
};

/*-----------------------------------------------------------------------------
	UFloatProperty.
-----------------------------------------------------------------------------*/

//
// Describes an IEEE 32-bit floating point variable.
//
class UFloatProperty : public UProperty
{
	DECLARE_CLASS(UFloatProperty,UProperty,CLASS_IsAUProperty,Core)

	// Constructors.
	UFloatProperty()
	{}
	UFloatProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
};

/*-----------------------------------------------------------------------------
	UObjectProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class UObjectProperty : public UProperty
{
	DECLARE_CLASS(UObjectProperty,UProperty,CLASS_IsAUProperty|CLASS_IsAUObjectProperty,Core)

	// Variables.
	class UClass* PropertyClass;

	// Constructors.
	UObjectProperty()
	{}
	UObjectProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UClass* InClass )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	PropertyClass( InClass )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void CleanupDestroyed( BYTE* Data, UObject* Owner ) const;
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;

	// UObjectProperty interface.
	UObject* FindImportedObject(UObject* Parent,UClass* ObjectClass,const TCHAR* Text) const;
};

/*-----------------------------------------------------------------------------
	UComponentProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class UComponentProperty : public UObjectProperty
{
	DECLARE_CLASS(UComponentProperty,UObjectProperty,0,Core)

	// UProperty interface.
	virtual BYTE GetID() const
	{
		return NAME_ObjectProperty;
	}
	virtual void FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner );
	virtual void InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Context, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	virtual UBOOL Identical( const void* A, const void* B ) const;
};

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class UClassProperty : public UObjectProperty
{
	DECLARE_CLASS(UClassProperty,UObjectProperty,0,Core)

	// Variables.
	class UClass* MetaClass;

	// Constructors.
	UClassProperty()
	{}
	UClassProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UClass* InMetaClass )
	:	UObjectProperty( EC_CppProperty, InOffset, InCategory, InFlags, UClass::StaticClass() )
	,	MetaClass( InMetaClass )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	BYTE GetID() const
	{
		return NAME_ObjectProperty;
	}
};

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

//
// Describes a name variable pointing into the global name table.
//
class UNameProperty : public UProperty
{
	DECLARE_CLASS(UNameProperty,UProperty,CLASS_IsAUProperty,Core)

	// Constructors.
	UNameProperty()
	{}
	UNameProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
};

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic string variable.
//
class UStrProperty : public UProperty
{
	DECLARE_CLASS(UStrProperty,UProperty,CLASS_IsAUProperty,Core)

	// Constructors.
	UStrProperty()
	{}
	UStrProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void DestroyValue( void* Dest ) const;
};

/*-----------------------------------------------------------------------------
	UFixedArrayProperty.
-----------------------------------------------------------------------------*/

//
// Describes a fixed length array.
//
class UFixedArrayProperty : public UProperty
{
	DECLARE_CLASS(UFixedArrayProperty,UProperty,CLASS_IsAUProperty,Core)

	// Variables.
	UProperty* Inner;
	INT Count;

	// Constructors.
	UFixedArrayProperty()
	{}
	UFixedArrayProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void CleanupDestroyed( BYTE* Data, UObject* Owner ) const;
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void DestroyValue( void* Dest ) const;

	// UFixedArrayProperty interface.
	void AddCppProperty( UProperty* Property, INT Count );
};

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic array.
//
class UArrayProperty : public UProperty
{
	DECLARE_CLASS(UArrayProperty,UProperty,CLASS_IsAUProperty,Core)

	// Variables.
	UProperty* Inner;

	// Constructors.
	UArrayProperty()
	{}
	UArrayProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void CleanupDestroyed( BYTE* Data, UObject* Owner ) const;
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void DestroyValue( void* Dest ) const;
	virtual void FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner );
	virtual void InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );

	// UArrayProperty interface.
	void AddCppProperty( UProperty* Property );
};

/*-----------------------------------------------------------------------------
	UMapProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic map.
//
class UMapProperty : public UProperty
{
	DECLARE_CLASS(UMapProperty,UProperty,CLASS_IsAUProperty,Core)

	// Variables.
	UProperty* Key;
	UProperty* Value;

	// Constructors.
	UMapProperty()
	{}
	UMapProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void DestroyValue( void* Dest ) const;
};

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

//
// Describes a structure variable embedded in (as opposed to referenced by) 
// an object.
//
class UStructProperty : public UProperty
{
	DECLARE_CLASS(UStructProperty,UProperty,CLASS_IsAUProperty,Core)

	// Variables.
	class UStruct* Struct;

	// Constructors.
	UStructProperty()
	{}
	UStructProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UStruct* InStruct )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	Struct( InStruct )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void CleanupDestroyed( BYTE* Data, UObject* Owner ) const;
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void DestroyValue( void* Dest ) const;
	virtual void FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner );
	virtual void InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
	virtual void FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner );
};

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

//
// Describes a pointer to a function bound to an Object
//
class UDelegateProperty : public UProperty
{
	DECLARE_CLASS(UDelegateProperty,UProperty,CLASS_IsAUProperty,Core)

	/** Function this delegate is mapped to */
	UFunction* Function;

	/** Name of delegate function for property references */
	FName DelegateName;

	// Constructors.
	UDelegateProperty()
	{}
	UDelegateProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void CleanupDestroyed( BYTE* Data, UObject* Owner ) const;
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	void ExportCppItem( FOutputDevice& Out, UBOOL IsEvent=0 ) const;
	void ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const;
	void CopySingleValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
	void CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject=NULL ) const;
};

/*-----------------------------------------------------------------------------
	Field templates.
-----------------------------------------------------------------------------*/

//
// Find a typed field in a struct.
//
template <class T> T* FindField( UStruct* Owner, const TCHAR* FieldName )
{
	for( TFieldIterator<T>It( Owner ); It; ++It )
		if( appStricmp( It->GetName(), FieldName )==0 )
			return *It;
	return NULL;
}

/*-----------------------------------------------------------------------------
	UObject accessors that depend on UClass.
-----------------------------------------------------------------------------*/

//
// See if this object belongs to the specified class.
//
inline UBOOL UObject::IsA( class UClass* SomeBase ) const
{
	for( UClass* TempClass=Class; TempClass; TempClass=(UClass*)TempClass->SuperField )
		if( TempClass==SomeBase )
			return 1;
	return SomeBase==NULL;
}

//
// See if this object is in a certain package.
//
inline UBOOL UObject::IsIn( class UObject* SomeOuter ) const
{
	for( UObject* It=GetOuter(); It; It=It->GetOuter() )
		if( It==SomeOuter )
			return 1;
	return SomeOuter==NULL;
}

//
// Return whether an object wants to receive a named probe message.
//
inline UBOOL UObject::IsProbing( FName ProbeName )
{
	return	(ProbeName.GetIndex() <  NAME_PROBEMIN)
	||		(ProbeName.GetIndex() >= NAME_PROBEMAX)
	||		(!StateFrame)
	||		(StateFrame->ProbeMask & ((QWORD)1 << (ProbeName.GetIndex() - NAME_PROBEMIN)));
}

/*-----------------------------------------------------------------------------
	UStruct inlines.
-----------------------------------------------------------------------------*/

//
// UStruct inline comparer.
//
inline UBOOL UStruct::StructCompare( const void* A, const void* B )
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
		for( INT i=0; i<It->ArrayDim; i++ )
			if( !It->Matches(A,B,i) )
				return 0;
	return 1;
}

/*-----------------------------------------------------------------------------
	C++ property macros.
-----------------------------------------------------------------------------*/

#define CPP_PROPERTY(name) \
	EC_CppProperty, (BYTE*)&((ThisClass*)NULL)->name - (BYTE*)NULL

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

