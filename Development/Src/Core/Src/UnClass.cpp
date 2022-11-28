/*=============================================================================
	UnClass.cpp: Object class implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

//
// A tag describing a class property, to aid in serialization.
//
struct FPropertyTag
{
	// Archive for counting property sizes.
	class FArchiveCountSize : public FArchive
	{
	public:
		FArchiveCountSize( FArchive& InSaveAr )
		: Size(0), SaveAr(InSaveAr)
		{
			ArIsSaving     = InSaveAr.IsSaving();
			ArIsPersistent = InSaveAr.IsPersistent();
		}
		INT Size;
	private:
		FArchive& SaveAr;
		FArchive& operator<<( UObject*& Obj )
		{
			INT Index = SaveAr.MapObject(Obj);
			FArchive& Ar = *this;
			return Ar << Index;
		}
		FArchive& operator<<( FName& Name )
		{
			INT Index = SaveAr.MapName(&Name);
			FArchive& Ar = *this;
			return Ar << Index;
		}
		void Serialize( void* V, INT Length )
		{
			Size += Length;
		}
	};

	// Variables.
	BYTE	Type;		// Type of property, 0=end.
	BYTE	Info;		// Packed info byte.
	FName	Name;		// Name of property.
	FName	ItemName;	// Struct name if UStructProperty.
	INT		Size;       // Property size.
	INT		ArrayIndex;	// Index if an array; else 0.

	// Constructors.
	FPropertyTag()
	{}
	FPropertyTag( FArchive& InSaveAr, UProperty* Property, INT InIndex, BYTE* Value )
	:	Type		( Property->GetID() )
	,	Name		( Property->GetFName() )
	,	ItemName	( NAME_None     )
	,	Size		( 0             )
	,	ArrayIndex	( InIndex       )
	,	Info		( Property->GetID() )
	{
		// Handle structs.
		UStructProperty* StructProperty = Cast<UStructProperty>( Property );
		if( StructProperty )
			ItemName = StructProperty->Struct->GetFName();

		// Set size.
		FArchiveCountSize ArCount( InSaveAr );
		SerializeTaggedProperty( ArCount, Property, Value, 0 );
		Size = ArCount.Size;

		// Update info bits.
		Info |=
		(	Size==1		? 0x00
		:	Size==2     ? 0x10
		:	Size==4     ? 0x20
		:	Size==12	? 0x30
		:	Size==16	? 0x40
		:	Size<=255	? 0x50
		:	Size<=65536 ? 0x60
		:			      0x70);
		UBoolProperty* Bool = Cast<UBoolProperty>( Property );
		if( ArrayIndex || (Bool && (*(BITFIELD*)Value & Bool->BitMask)) )
			Info |= 0x80;
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
	{
		BYTE	SizeByte;
		_WORD	SizeWord;
		INT		SizeInt;

		// Name.
		Ar << Tag.Name;
		if( Tag.Name == NAME_None )
			return Ar;
		
		// Packed info byte:
		// Bit 0..3 = raw type.
		// Bit 4..6 = serialized size: [1 2 4 12 16 byte word int].
		// Bit 7    = array flag.
		Ar << Tag.Info;
		Tag.Type = Tag.Info & 0x0f;
		if( Tag.Type == NAME_StructProperty )
			Ar << Tag.ItemName;
		switch( Tag.Info & 0x70 )
		{
			case 0x00:
				Tag.Size = 1;
				break;
			case 0x10:
				Tag.Size = 2;
				break;
			case 0x20:
				Tag.Size = 4;
				break;
			case 0x30:
				Tag.Size = 12;
				break;
			case 0x40:
				Tag.Size = 16;
				break;
			case 0x50:
				SizeByte =  Tag.Size;
				Ar       << SizeByte;
				Tag.Size =  SizeByte;
				break;
			case 0x60:
				SizeWord =  Tag.Size;
				Ar       << SizeWord;
				Tag.Size =  SizeWord;
				break;
			case 0x70:
				SizeInt		=  Tag.Size;
				Ar          << SizeInt;
				Tag.Size    =  SizeInt;
				break;
		}
		if( (Tag.Info&0x80) && Tag.Type!=NAME_BoolProperty )
		{
			BYTE B
			=	(Tag.ArrayIndex<=127  ) ? (Tag.ArrayIndex    )
			:	(Tag.ArrayIndex<=16383) ? (Tag.ArrayIndex>>8 )+0x80
			:	                          (Tag.ArrayIndex>>24)+0xC0;
			Ar << B;
			if( (B & 0x80)==0 )
			{
				Tag.ArrayIndex = B;
			}
			else if( (B & 0xC0)==0x80 )
			{
				BYTE C = Tag.ArrayIndex & 255;
				Ar << C;
				Tag.ArrayIndex = ((INT)(B&0x7F)<<8) + ((INT)C);
			}
			else
			{
				BYTE C = Tag.ArrayIndex>>16;
				BYTE D = Tag.ArrayIndex>>8;
				BYTE E = Tag.ArrayIndex;
				Ar << C << D << E;
				Tag.ArrayIndex = ((INT)(B&0x3F)<<24) + ((INT)C<<16) + ((INT)D<<8) + ((INT)E);
			}
		}
		else Tag.ArrayIndex = 0;
		return Ar;
	}

	// Property serializer.
	void SerializeTaggedProperty( FArchive& Ar, UProperty* Property, BYTE* Value, INT MaxReadBytes )
	{
		if( Property->GetClass()==UBoolProperty::StaticClass() )
		{
			UBoolProperty* Bool = (UBoolProperty*)Property;
			check(Bool->BitMask!=0);
			if( Ar.IsLoading() )				
			{
				if( Info&0x80)	*(BITFIELD*)Value |=  Bool->BitMask;
				else			*(BITFIELD*)Value &= ~Bool->BitMask;
			}
		}
		else
		{
			Property->SerializeItem( Ar, Value, MaxReadBytes );
		}
	}
};

/*-----------------------------------------------------------------------------
	UField implementation.
-----------------------------------------------------------------------------*/

UField::UField( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UField* InSuperField )
: UObject				( EC_NativeConstructor, InClass, InName, InPackageName, InFlags )
, SuperField			( InSuperField )
, Next					( NULL )
{}
UField::UField( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
: UObject				( EC_StaticConstructor, InName, InPackageName, InFlags )
, Next					( NULL )
{}
UField::UField( UField* InSuperField )
:	SuperField( InSuperField )
{}
UClass* UField::GetOwnerClass()
{
	UObject* Obj;
	for( Obj=this; Obj->GetClass()!=UClass::StaticClass(); Obj=Obj->GetOuter() );
	return (UClass*)Obj;
}
void UField::Bind()
{
}
void UField::PostLoad()
{
	Super::PostLoad();
	Bind();
}
void UField::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << SuperField << Next;
}
INT UField::GetPropertiesSize()
{
	return 0;
}
UBOOL UField::MergeBools()
{
	return 1;
}
void UField::AddCppProperty( UProperty* Property )
{
	appErrorf(TEXT("UField::AddCppProperty"));
}
void UField::Register()
{
	Super::Register();
	if( SuperField )
		SuperField->ConditionalRegister();
}
IMPLEMENT_CLASS(UField)

/*-----------------------------------------------------------------------------
	UStruct implementation.
-----------------------------------------------------------------------------*/

//
// Constructors.
//
UStruct::UStruct( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UStruct* InSuperStruct )
:	UField			( EC_NativeConstructor, UClass::StaticClass(), InName, InPackageName, InFlags, InSuperStruct )
,	ScriptText		( NULL )
,	CppText			( NULL )
,	Children		( NULL )
,	PropertiesSize	( InSize )
,	Script			()
,	Defaults		()
,	TextPos			( 0 )
,	Line			( 0 )
,	MinAlignment	( 1 )
,	RefLink			( NULL )
,	PropertyLink	( NULL )
,	ConfigLink	    ( NULL )
,	ConstructorLink	( NULL )
{}
UStruct::UStruct( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
:	UField			( EC_StaticConstructor, InName, InPackageName, InFlags )
,	ScriptText		( NULL )
,	CppText			( NULL )
,	Children		( NULL )
,	PropertiesSize	( InSize )
,	Script			()
,	Defaults		()
,	TextPos			( 0 )
,	Line			( 0 )
,	MinAlignment	( 1 )
,	RefLink			( NULL )
,	PropertyLink	( NULL )
,	ConfigLink	    ( NULL )
,	ConstructorLink	( NULL )
{}
UStruct::UStruct( UStruct* InSuperStruct )
:	UField( InSuperStruct )
,	PropertiesSize( InSuperStruct ? InSuperStruct->GetPropertiesSize() : 0 )
,	MinAlignment( Max(InSuperStruct ? InSuperStruct->GetMinAlignment() : 1,1) )
,	RefLink( NULL )
{}

//
// Add a property.
//
void UStruct::AddCppProperty( UProperty* Property )
{
	Property->Next = Children;
	Children       = Property;
}

//
// Link offsets.
//
void UStruct::Link( FArchive& Ar, UBOOL Props )
{
	// Link the properties.
	if( Props )
	{
		PropertiesSize = 0;
		if( GetInheritanceSuper() )
		{
			Ar.Preload( GetInheritanceSuper() );
			// Visual Studio .NET 2003 seems to crack open the base class padding in certain cases when adding members to a derived class.
			// This seems to be fixed for the Xenon frontend so we need two different codepathes for now.
#ifdef XBOX
			PropertiesSize = Align(GetInheritanceSuper()->GetPropertiesSize(),Max(PROPERTY_ALIGNMENT,GetInheritanceSuper()->GetMinAlignment())) ;
#else
			PropertiesSize = Align(GetInheritanceSuper()->GetPropertiesSize(),PROPERTY_ALIGNMENT) ;
#endif
		}
		UProperty* Prev = NULL;
		for( UField* Field=Children; Field; Field=Field->Next )
		{
			Ar.Preload( Field );
			if( Field->GetOuter()!=this )
				break;
			UProperty* Property = Cast<UProperty>( Field );
			if( Property )
			{
				Property->Link( Ar, Prev );
				PropertiesSize = Property->Offset + Property->GetSize();
				Prev = Property;
			}
		}
		// Visual Studio .NET 2003 seems to crack open the base class padding in certain cases when adding members to a derived class.
		// This seems to be fixed for the Xenon frontend so we need two different codepathes for now.
#ifdef XBOX
		PropertiesSize = Align(PropertiesSize,Max(PROPERTY_ALIGNMENT,MinAlignment));
#else
		PropertiesSize = Align(PropertiesSize,PROPERTY_ALIGNMENT);
#endif
	}
	else
	{
		UProperty* Prev = NULL;
		for( UField* Field=Children; Field && Field->GetOuter()==this; Field=Field->Next )
		{
			UProperty* Property = Cast<UProperty>( Field );
			if( Property )
			{
				UBoolProperty*	BoolProperty = Cast<UBoolProperty>(Property);
				INT				SavedOffset = Property->Offset;
				BITFIELD		SavedBitMask = BoolProperty ? BoolProperty->BitMask : 0;

				Property->Link( Ar, Prev );
				Property->Offset = SavedOffset;
				Prev = Property;

				if(Cast<UBoolProperty>(Property))
					Cast<UBoolProperty>(Property)->BitMask = SavedBitMask;
			}
		}
	}

#if !__INTEL_BYTE_ORDER__
	// Object.uc declares FColor in a fixed "Intel- Endian" byte order which doesn't match up with C++ on non "Intel- Endian" platforms.
	// The workaround is to manually fiddle with the property offsets so everything matches.
	//@todo xenon cooking: this should be moved into the data cooking step.
	if( GetFName() == NAME_Color )
	{
		UProperty*	ColorComponentEntries[4];
		UINT		ColorComponentIndex = 0;

		for( UField* Field=Children; Field && Field->GetOuter()==this; Field=Field->Next )
		{
			ColorComponentEntries[ColorComponentIndex++] = CastChecked<UProperty>( Field );
		}
		check( ColorComponentIndex == 4 );

		Exchange( ColorComponentEntries[0]->Offset, ColorComponentEntries[3]->Offset );
		Exchange( ColorComponentEntries[1]->Offset, ColorComponentEntries[2]->Offset );
	}
#endif

	// Link the references, structs, and arrays for optimized cleanup.
	// Note: Could optimize further by adding UProperty::NeedsDynamicRefCleanup, excluding things like arrays of ints.
	TMap<UProperty*,INT> Map;
	UProperty** PropertyLinkPtr		= &PropertyLink;
	UProperty** ConfigLinkPtr		= &ConfigLink;
	UProperty** ConstructorLinkPtr	= &ConstructorLink;
	UProperty** RefLinkPtr			= (UProperty**)&RefLink;

	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It)
	{
		if( Cast<UObjectProperty>(*It) || Cast<UStructProperty>(*It) || Cast<UArrayProperty>(*It) || Cast<UDelegateProperty>(*It) )
		{
			*RefLinkPtr = *It;
			RefLinkPtr=&(*RefLinkPtr)->NextRef;
		}
		if( It->PropertyFlags & CPF_NeedCtorLink )
		{
			*ConstructorLinkPtr = *It;
			ConstructorLinkPtr  = &(*ConstructorLinkPtr)->ConstructorLinkNext;
		}
		if( It->PropertyFlags & CPF_Config )
		{
			*ConfigLinkPtr = *It;
			ConfigLinkPtr  = &(*ConfigLinkPtr)->ConfigLinkNext;
		}
		*PropertyLinkPtr = *It;
		PropertyLinkPtr  = &(*PropertyLinkPtr)->PropertyLinkNext;
#if 0
		//@todo re-evaluate the need for the below code.
		if( (ItC->PropertyFlags & CPF_Net) && !GIsEditor )
		{
			ItC->RepOwner = *ItC;
			FArchive TempAr;
			INT iCode = ItC->RepOffset;
			ItC->GetOwnerClass()->SerializeExpr( iCode, TempAr );
			Map.Set( *ItC, iCode );
			for( TFieldIterator<UProperty,CLASS_IsAUProperty> ItD(this); *ItD!=*ItC; ++ItD )
			{
				if( ItD->PropertyFlags & CPF_Net )
				{
					INT* iCodePtr = Map.Find( *ItD );
					check(iCodePtr);
					if
					(	iCode-ItC->RepOffset==*iCodePtr-ItD->RepOffset
					&&	appMemcmp(&ItC->GetOwnerClass()->Script(ItC->RepOffset),&ItD->GetOwnerClass()->Script(ItD->RepOffset),iCode-ItC->RepOffset)==0 )
					{
						ItD->RepOwner = ItC->RepOwner;
					}
				}
			}
		}
#endif
	}
	*PropertyLinkPtr    = NULL;
	*ConfigLinkPtr      = NULL;
	*ConstructorLinkPtr = NULL;
	*RefLinkPtr			= NULL;
}

//
// Serialize all of the class's data that belongs in a particular
// bin and resides in Data.
//
void UStruct::SerializeBin( FArchive& Ar, BYTE* Data, INT MaxReadBytes )
{
	INT MaxReadPos = Ar.Tell() + MaxReadBytes;
	INT Index=0;

	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
	{
		if( It->ShouldSerializeValue(Ar) )
			for( Index=0; Index<It->ArrayDim; Index++ )
				It->SerializeItem( Ar, Data + It->Offset + Index*It->ElementSize, 0 );
	}
}
void UStruct::SerializeTaggedProperties( FArchive& Ar, BYTE* Data, UStruct* DefaultsStruct )
{
	FName PropertyName(NAME_None);
	INT Index=-1;
	check(Ar.IsLoading() || Ar.IsSaving());

	// Find defaults.
	BYTE* Defaults      = NULL;
	INT   DefaultsCount = 0;
	if( DefaultsStruct )
	{
		Defaults      = &DefaultsStruct->Defaults(0);
		DefaultsCount =  DefaultsStruct->Defaults.Num();
	}

	if( Ar.IsLoading() )
	{
		// Load tagged properties.

		// This code assumes that properties are loaded in the same order they are saved in. This removes a n^2 search 
		// and makes it an O(n) when properties are saved in the same order as they are loaded (default case). In the 
		// case that a property was reordered the code falls back to a slower search.
		UProperty*	Property			= PropertyLink;
		UBOOL		AdvanceProperty		= 0;
		INT			RemainingArrayDim	= Property ? Property->ArrayDim : 0;

		// Load all stored properties, potentially skipping unknown ones.
		while( 1 )
		{
			FPropertyTag Tag;
			Ar << Tag;
			if( Tag.Name == NAME_None )
				break;
			PropertyName = Tag.Name;

			// Move to the next property to be serialized
			if( AdvanceProperty && --RemainingArrayDim <= 0 )
			{
				Property = Property->PropertyLinkNext;
				// Skip over properties that don't need to be serialized.
				while( Property && !Property->ShouldSerializeValue( Ar ) )
				{
					Property = Property->PropertyLinkNext;
				}
				AdvanceProperty		= 0;
				RemainingArrayDim	= Property ? Property->ArrayDim : 0;
			}

			// If this property is not the one we expect (e.g. skipped as it matches the default value), do the brute force search.
			if( Property == NULL || Property->GetFName() != Tag.Name )
			{
				UProperty* CurrentProperty = Property;
				// Search forward...
				for ( ; Property; Property=Property->PropertyLinkNext )
				{
					if( Property->GetFName() == Tag.Name )
					{
						break;
					}
				}
				// ... and then search from the beginning till we reach the current property if it's not found.
				if( Property == NULL )
				{
					for( Property = PropertyLink; Property && Property != CurrentProperty; Property = Property->PropertyLinkNext )
					{
						if( Property->GetFName() == Tag.Name )
						{
							break;
						}
					}

					if( Property == CurrentProperty )
					{
						// Property wasn't found.
						Property = NULL;
					}
				}

				RemainingArrayDim = Property ? Property->ArrayDim : 0;
			}

			if( !Property )
			{
				debugfSlow( NAME_Warning, TEXT("Property %s of %s not found"), *Tag.Name, *GetFullName() );
			}
			else if( Tag.Type==NAME_StrProperty && Property->GetID()==NAME_NameProperty )  
			{ 
				FString str;  
				Ar << str; 
				*(FName*)(Data + Property->Offset + Tag.ArrayIndex * Property->ElementSize ) = FName(*str);  
				AdvanceProperty = TRUE;
				continue; 
			}
			else if( Tag.Type!=Property->GetID() )
			{
				debugf( NAME_Warning, TEXT("Type mismatch in %s of %s: file %i, class %i"), *Tag.Name, GetName(), Tag.Type, Property->GetID() );
			}
			else if( Tag.ArrayIndex>=Property->ArrayDim )
			{
				debugf( NAME_Warning, TEXT("Array bounds in %s of %s: %i/%i"), *Tag.Name, GetName(), Tag.ArrayIndex, Property->ArrayDim );
			}
			else if( Tag.Type==NAME_StructProperty && Tag.ItemName!=CastChecked<UStructProperty>(Property)->Struct->GetFName() )
			{
				debugf( NAME_Warning, TEXT("Property %s of %s struct type mismatch %s/%s"), *Tag.Name, GetName(), *Tag.ItemName, CastChecked<UStructProperty>(Property)->Struct->GetName() );
			}
			else if( !Property->ShouldSerializeValue(Ar) )
			{
				if( appStricmp(*Tag.Name,TEXT("XLevel"))!=0 )
				{
					debugf( NAME_Warning, TEXT("Property %s of %s is not serialiable"), *Tag.Name, GetName() );
				}
			}
			else
			{
				// This property is ok.			
				Tag.SerializeTaggedProperty( Ar, Property, Data + Property->Offset + Tag.ArrayIndex*Property->ElementSize, Tag.Size );
				AdvanceProperty = TRUE;
				continue;
			}

			// Skip unknown or bad property.
			if( appStricmp(*Tag.Name,TEXT("XLevel"))!=0 )
			{
				debugfSlow( NAME_Warning, TEXT("Skipping %i bytes of type %i"), Tag.Size, Tag.Type );
			}
			BYTE B;
			for( INT i=0; i<Tag.Size; i++ )
			{
				Ar << B;
			}
		}
	}
	else
	{
		// Save tagged properties.

		// Iterate over properties in the order they were linked and serialize them.
		for( UProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext )
		{
			if( Property->ShouldSerializeValue(Ar) )
			{
				PropertyName = Property->GetFName();
				for( Index=0; Index<Property->ArrayDim; Index++ )
				{
					INT Offset = Property->Offset + Index * Property->ElementSize;
					if( (!IsA(UClass::StaticClass())&&!Defaults) || !Property->Matches( Data, (Offset+Property->ElementSize<=DefaultsCount) ? Defaults : NULL, Index) )
					{
						FPropertyTag Tag( Ar, Property, Index, Data + Offset );
						Ar << Tag;
						Tag.SerializeTaggedProperty( Ar, Property, Data + Offset, 0 );
					}
				}
			}
		}
		FName Temp(NAME_None);
		Ar << Temp;
	}
}
void UStruct::Destroy()
{
	Script.Empty();
	Super::Destroy();
	DefaultStructPropText=TEXT("");
}

void UStruct::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Serialize stuff.
	Ar << ScriptText << Children;
	Ar << CppText;
	// Compiler info.
	Ar << Line << TextPos;
	Ar << MinAlignment;

	// Script code.
	INT ScriptSize = Script.Num();
	Ar << ScriptSize;
	if( Ar.IsLoading() )
	{
		Script.Empty( ScriptSize );
		Script.Add( ScriptSize );
	}
	INT iCode = 0;
	while( iCode < ScriptSize )
		SerializeExpr( iCode, Ar );
	if( iCode != ScriptSize )
		appErrorf( TEXT("Script serialization mismatch: Got %i, expected %i"), iCode, ScriptSize );

	Ar.ThisContainsCode();
	// Link the properties.
	if( Ar.IsLoading() )
		Link( Ar, 1 );

	// Defaults for (and only for) UStructs.
	if( GetClass()->GetFName() == NAME_Struct )
	{
		if( Ar.IsLoading() )
		{
			AllocateStructDefaults();
			SerializeTaggedProperties( Ar, &Defaults(0), GetSuperStruct() );
		}
		else if( Ar.IsSaving() )
		{
			check(Defaults.Num()==GetPropertiesSize());
			SerializeTaggedProperties( Ar, &Defaults(0), GetSuperStruct() );
		}
		else
		{
			if( !Defaults.Num() )
				check(Defaults.Num()==GetPropertiesSize());
			Defaults.CountBytes( Ar );
			SerializeBin( Ar, &Defaults(0), 0 );
		}
	}
}

//
// Actor reference cleanup.
//
void UStruct::CleanupDestroyed( BYTE* Data, UObject* Owner )
{
	if( GIsEditor )
	{
		// Slow cleanup in editor where optimized structures don't exist.
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
			(*It)->CleanupDestroyed(Data+It->Offset, Owner);
	}
	else
	{
		// Optimal cleanup during gameplay.
		for( UProperty* Ref=RefLink; Ref; Ref=Ref->NextRef )
			Ref->CleanupDestroyed(Data+Ref->Offset, Owner);
	}
}

IMPLEMENT_CLASS(UStruct);

/*-----------------------------------------------------------------------------
	UState.
-----------------------------------------------------------------------------*/

UState::UState( UState* InSuperState )
: UStruct( InSuperState )
{}
UState::UState( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags, UState* InSuperState )
: UStruct( EC_NativeConstructor, InSize, InName, InPackageName, InFlags, InSuperState )
, ProbeMask( 0 )
, IgnoreMask( 0 )
, StateFlags( 0 )
, LabelTableOffset( 0 )
{}
UState::UState( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
: UStruct( EC_StaticConstructor, InSize, InName, InPackageName, InFlags )
, ProbeMask( 0 )
, IgnoreMask( 0 )
, StateFlags( 0 )
, LabelTableOffset( 0 )
{}
void UState::Destroy()
{
	Super::Destroy();
}
void UState::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	// Class/State-specific union info.
	Ar << ProbeMask << IgnoreMask;
	Ar << LabelTableOffset << StateFlags;
	// serialize the function map
	Ar << FuncMap;
}
IMPLEMENT_CLASS(UState);

/*-----------------------------------------------------------------------------
	UClass implementation.
-----------------------------------------------------------------------------*/

//
// Register the native class.
//
void UClass::Register()
{
	Super::Register();

	// Get stashed registration info.
	const TCHAR* InClassConfigName = *(TCHAR**)&ClassConfigName;
	ClassConfigName = InClassConfigName;

	// Init default object.
	Defaults.Empty( GetPropertiesSize() );
	Defaults.Add( GetPropertiesSize() );
	GetDefaultObject()->InitClassDefaultObject( this );

	// Perform static construction.
	if( !GetSuperClass() || GetSuperClass()->ClassStaticConstructor!=ClassStaticConstructor )
		(GetDefaultObject()->*ClassStaticConstructor)();

	// Propagate inhereted flags.
	if( SuperField )
		ClassFlags |= (GetSuperClass()->ClassFlags & CLASS_Inherit);

	// Link the cleanup.
	FArchive ArDummy;
	Link( ArDummy, 0 );

	// Load defaults.
	GetDefaultObject()->LoadConfig();
	GetDefaultObject()->LoadLocalized();
}

//
// Find the class's native constructor.
//
void UClass::Bind()
{
	UStruct::Bind();
	check(GIsEditor || GetSuperClass() || this==UObject::StaticClass());
	if( !ClassConstructor && (GetFlags() & RF_Native) )
	{
		// Find the native implementation.
		TCHAR ProcName[256];
		appSprintf( ProcName, TEXT("autoclass%s"), GetName() );

		// Find export from the DLL.
		UPackage* ClassPackage = GetOuterUPackage();
		UClass** ClassPtr = (UClass**)ClassPackage->GetExport( ProcName, 0 );
		if( ClassPtr )
		{
			check(*ClassPtr);
			check(*ClassPtr==this);
			ClassConstructor = (*ClassPtr)->ClassConstructor;
		}
		else if( !GIsEditor )
		{
			appErrorf( TEXT("Can't bind to native class %s"), *GetPathName() );
		}
	}
	if( !ClassConstructor && GetSuperClass() )
	{
		// Chase down constructor in parent class.
		GetSuperClass()->Bind();
		ClassConstructor = GetSuperClass()->ClassConstructor;
	}
	check(GIsEditor || ClassConstructor);
}

/**
 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
 * Classes deriving from AActor have an 'A' prefix and other UObject classes an 'U' prefix.
 *
 * @return Prefix character used for C++ declaration of this struct/ class.
 */
const TCHAR* UClass::GetPrefixCPP()
{
	UClass* Class			= this;
	UBOOL	IsActorClass	= false;
	while( Class && !IsActorClass )
	{
		IsActorClass	= Class->GetFName() == NAME_Actor;
		Class			= Class->GetSuperClass();
	}
	return IsActorClass ? TEXT("A") : TEXT("U");
}

FString UClass::GetDescription() const
{
	// Look up the the classes name in the INT file and return the class name if there is no match.
	FString Description = Localize( TEXT("Objects"), GetName(), TEXT("Descriptions"), GetLanguage(), 1 );
	if( Description.Len() )
		return Description;
	else
		return FString( GetName() );
}


/*-----------------------------------------------------------------------------
	UClass UObject implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_COMPARE_POINTER( UField, UnClass, { if( !A->GetLinker() || !B->GetLinker() ) return 0; else return A->GetLinkerIndex() - B->GetLinkerIndex(); } )

void UClass::Destroy()
{
	// Empty arrays.
	//warning: Must be emptied explicitly in order for intrinsic classes
	// to not show memory leakage on exit.
	NetFields.Empty();
	PackageImports.Empty();
	ExitProperties( &Defaults(0), this );
	Defaults.Empty();
	DefaultPropText=TEXT("");

	Super::Destroy();
}
void UClass::PostLoad()
{
	check(ClassWithin);
	Super::PostLoad();

	// Postload super.
	if( GetSuperClass() )
		GetSuperClass()->ConditionalPostLoad();
}
void UClass::Link( FArchive& Ar, UBOOL Props )
{
	Super::Link( Ar, Props );
	if( !GIsEditor )
	{
		NetFields.Empty();
		ClassReps = SuperField ? GetSuperClass()->ClassReps : TArray<FRepRecord>();
		for( TFieldIterator<UField> It(this); It && It->GetOwnerClass()==this; ++It )
		{
			UProperty* P;
			UFunction* F;
			if( (P=Cast<UProperty>(*It))!=NULL )
			{
				if( P->PropertyFlags&CPF_Net )
				{
					NetFields.AddItem( *It );
					if( P->GetOuter()==this )
					{
						P->RepIndex = ClassReps.Num();
						for( INT i=0; i<P->ArrayDim; i++ )
							new(ClassReps)FRepRecord(P,i);
					}
				}
			}
			else if( (F=Cast<UFunction>(*It))!=NULL )
			{
				if( (F->FunctionFlags&FUNC_Net) && !F->GetSuperFunction() )
					NetFields.AddItem( *It );
			}
		}
		NetFields.Shrink();
		Sort<USE_COMPARE_POINTER(UField,UnClass)>( &NetFields(0), NetFields.Num() );
	}
}

//@deprecated: 186
class FDeprecatedDependency
{
public:
	UClass*	Class;
	UBOOL	Deep;
	DWORD	ScriptTextCRC;
	friend FArchive& operator<<( FArchive& Ar, FDeprecatedDependency& Dep )
	{
		return Ar << Dep.Class << Dep.Deep << Dep.ScriptTextCRC;
	}
};

void UClass::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Variables.
	Ar << ClassFlags << ClassGuid;
	if( Ar.Ver() < 186 )
	{
		TArray<FDeprecatedDependency> DeprecatedDependencies;
		Ar << DeprecatedDependencies;
	}
	Ar << PackageImports << ClassWithin << ClassConfigName << HideCategories;
	Ar << ComponentClassToNameMap << ComponentNameToDefaultObjectMap;

	if( Ar.Ver() >= 185 )
	{
		Ar << AutoExpandCategories;
	}

	// Defaults.
	if( Ar.IsLoading() )
	{
		check(GetPropertiesSize()>=sizeof(UObject));
		check(!GetSuperClass() || !(GetSuperClass()->GetFlags()&RF_NeedLoad));
		Defaults.Empty( GetPropertiesSize() );
		Defaults.Add( GetPropertiesSize() );
		GetDefaultObject()->InitClassDefaultObject( this );
		SerializeTaggedProperties( Ar, &Defaults(0), GetSuperClass() );
		GetDefaultObject()->LoadConfig();
		GetDefaultObject()->LoadLocalized();
		ClassUnique = 0;
	}
	else if( Ar.IsSaving() )
	{
		check(Defaults.Num()==GetPropertiesSize());
		SerializeTaggedProperties( Ar, &Defaults(0), GetSuperClass() );
	}
	else
	{
		check(Defaults.Num()==GetPropertiesSize());
		Defaults.CountBytes( Ar );
		SerializeBin( Ar, &Defaults(0), 0 );
	}
}

/*-----------------------------------------------------------------------------
	UClass constructors.
-----------------------------------------------------------------------------*/

//
// Internal constructor.
//
UClass::UClass()
:	ClassWithin( UObject::StaticClass() )
{}

//
// Create a new UClass given its superclass.
//
UClass::UClass( UClass* InBaseClass )
:	UState( InBaseClass )
,	ClassWithin( UObject::StaticClass() )
{
	if( GetSuperClass() )
	{
		ClassWithin = GetSuperClass()->ClassWithin;
		Defaults = GetSuperClass()->Defaults;
		Bind();		
	}
}

//
// UClass autoregistry constructor.
//warning: Called at DLL init time.
//
UClass::UClass
(
	ENativeConstructor,
	DWORD			InSize,
	DWORD			InClassFlags,
	UClass*			InSuperClass,
	UClass*			InWithinClass,
	FGuid			InGuid,
	const TCHAR*	InNameStr,
	const TCHAR*    InPackageName,
	const TCHAR*    InConfigName,
	DWORD			InFlags,
	void			(*InClassConstructor)(void*),
	void			(UObject::*InClassStaticConstructor)()
)
:	UState					( EC_NativeConstructor, InSize, InNameStr, InPackageName, InFlags, InSuperClass!=this ? InSuperClass : NULL )
,	ClassFlags				( InClassFlags | CLASS_Parsed | CLASS_Compiled )
,	ClassUnique				( 0 )
,	ClassGuid				( InGuid )
,	ClassWithin				( InWithinClass )
,	ClassConfigName			()
,	PackageImports			()
,	NetFields				()
,	ClassConstructor		( InClassConstructor )
,	ClassStaticConstructor	( InClassStaticConstructor )
{
	*(const TCHAR**)&ClassConfigName = InConfigName;
}

// Called when statically linked.
UClass::UClass
(
	EStaticConstructor,
	DWORD			InSize,
	DWORD			InClassFlags,
	FGuid			InGuid,
	const TCHAR*	InNameStr,
	const TCHAR*    InPackageName,
	const TCHAR*    InConfigName,
	DWORD			InFlags,
	void			(*InClassConstructor)(void*),
	void			(UObject::*InClassStaticConstructor)()
)
:	UState					( EC_StaticConstructor, InSize, InNameStr, InPackageName, InFlags )
,	ClassFlags				( InClassFlags | CLASS_Parsed | CLASS_Compiled )
,	ClassUnique				( 0 )
,	ClassGuid				( InGuid )
,	ClassWithin				( NULL )
,	ClassConfigName			()
,	PackageImports			()
,	NetFields				()
,	ClassConstructor		( InClassConstructor )
,	ClassStaticConstructor	( InClassStaticConstructor )
{
	*(const TCHAR**)&ClassConfigName = InConfigName;
}

IMPLEMENT_CLASS(UClass);

/*-----------------------------------------------------------------------------
	FLabelEntry.
-----------------------------------------------------------------------------*/

FLabelEntry::FLabelEntry( FName InName, INT iInCode )
:	Name	(InName)
,	iCode	(iInCode)
{}
FArchive& operator<<( FArchive& Ar, FLabelEntry &Label )
{
	Ar << Label.Name;
	Ar << Label.iCode;
	return Ar;
}

/*-----------------------------------------------------------------------------
	UStruct implementation.
-----------------------------------------------------------------------------*/

#if SERIAL_POINTER_INDEX
void *GSerializedPointers[MAX_SERIALIZED_POINTERS];
INT GTotalSerializedPointers = 0;

// !!! FIXME: This has GOT to be a mad performance hit.  --ryan.
INT SerialPointerIndex(void *ptr)
{
    for (INT i = 0; i < GTotalSerializedPointers; i++)
    {
        if (GSerializedPointers[i] == (void *) ptr)
            return(i);
    }

    check(GTotalSerializedPointers < MAX_SERIALIZED_POINTERS);
    GSerializedPointers[GTotalSerializedPointers] = ptr;
    //printf("Added new serialized pointer: number (%d) at (%p).\n", GTotalSerializedPointers, ptr);
    return(GTotalSerializedPointers++);
}
#endif


//
// Serialize an expression to an archive.
// Returns expression token.
//
EExprToken UStruct::SerializeExpr( INT& iCode, FArchive& Ar )
{
	EExprToken Expr=(EExprToken)0;
	#define XFER(T) {Ar << *(T*)&Script(iCode); iCode += sizeof(T); }

#if !SERIAL_POINTER_INDEX
    #define XFERPTR(T) XFER(T)
#else
    #define XFERPTR(T) \
    { \
        T x = NULL; \
        if (!Ar.IsLoading()) \
            x = (T) GSerializedPointers[*(INT*)&Script(iCode)]; \
        Ar << x; \
        *(INT*)&Script(iCode) = SerialPointerIndex(x); \
        iCode += sizeof (INT); \
    }
#endif

#ifndef CONSOLE
	//DEBUGGER: To mantain compatability between debug and non-debug clases
	#define HANDLE_OPTIONAL_DEBUG_INFO \
    if (iCode < Script.Num()) \
    { \
	    int RemPos = Ar.Tell(); \
	    int OldiCode = iCode;	\
	    XFER(BYTE); \
	    int NextCode = Script(iCode-1); \
	    int GVERSION = -1;	\
	    if ( NextCode == EX_DebugInfo ) \
	    {	\
		    XFER(INT); \
		    GVERSION = *(INT*)&Script(iCode-sizeof(INT));	\
	    }	\
	    iCode = OldiCode;	\
	    Ar.Seek( RemPos );	\
	    if ( GVERSION == 100 )	\
		    SerializeExpr( iCode, Ar );	\
    }
#else
	// Console builds cannot deal with debug classes.
	#define HANDLE_OPTIONAL_DEBUG_INFO __noop
#endif

	// Get expr token.
	XFER(BYTE);
	Expr = (EExprToken)Script(iCode-1);
	if( Expr >= EX_FirstNative )
	{
		// Native final function with id 1-127.
		while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms );
		HANDLE_OPTIONAL_DEBUG_INFO; //DEBUGGER
	}
	else if( Expr >= EX_ExtendedNative )
	{
		// Native final function with id 256-16383.
		XFER(BYTE);
		while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms );
		HANDLE_OPTIONAL_DEBUG_INFO; //DEBUGGER
	}
	else switch( Expr )
	{
		case EX_PrimitiveCast:
		{
			// A type conversion.
			XFER(BYTE); //which kind of conversion
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_Let:
		case EX_LetBool:
		case EX_LetDelegate:
		{
			SerializeExpr( iCode, Ar ); // Variable expr.
			SerializeExpr( iCode, Ar ); // Assignment expr.
			break;
		}
		case EX_Jump:
		{
			XFER(_WORD); // Code offset.
			break;
		}
		case EX_LocalVariable:
		case EX_InstanceVariable:
		case EX_DefaultVariable:
		{
			XFERPTR(UProperty*);
			break;
		}
		case EX_DebugInfo:
		{
			XFER(INT);	// Version
			XFER(INT);	// Line number
			XFER(INT);	// Character pos
			XFER(BYTE); // OpCode
			break;
		}
		case EX_BoolVariable:
		case EX_Nothing:
		case EX_EndFunctionParms:
		case EX_IntZero:
		case EX_IntOne:
		case EX_True:
		case EX_False:
		case EX_NoObject:
		case EX_Self:
		case EX_IteratorPop:
		case EX_Stop:
		case EX_IteratorNext:
		{
			break;
		}
		case EX_EatString:
		{
			SerializeExpr( iCode, Ar ); // String expression.
			break;
		}
		case EX_Return:
		{
			SerializeExpr( iCode, Ar ); // Return expression.
			break;
		}
		case EX_FinalFunction:
		{
			XFERPTR(UStruct*); // Stack node.
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms ); // Parms.
			HANDLE_OPTIONAL_DEBUG_INFO; //DEBUGGER
			break;
		}
		case EX_VirtualFunction:
		{
			XFER(BYTE); // Super function call
			XFER(FName); // Virtual function name.
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms ); // Parms.
			HANDLE_OPTIONAL_DEBUG_INFO; //DEBUGGER
			break;
		}
		case EX_GlobalFunction:
		{
			XFER(FName); // Virtual function name.
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms ); // Parms.
			HANDLE_OPTIONAL_DEBUG_INFO; //DEBUGGER
			break;
		}
		case EX_DelegateFunction:
		{
			XFERPTR(BYTE); // local prop
			XFERPTR(UProperty*);	// Delegate property
			XFER(FName);		// Delegate function name (in case the delegate is NULL)
			break;
		}
		case EX_NativeParm:
		{
			XFERPTR(UProperty*);
			break;
		}
		case EX_ClassContext:
		case EX_Context:
		{
			SerializeExpr( iCode, Ar ); // Object expression.
			XFER(_WORD); // Code offset for NULL expressions.
			XFER(BYTE); // Zero-fill size if skipped.
			SerializeExpr( iCode, Ar ); // Context expression.
			break;
		}
		case EX_ArrayElement:
		case EX_DynArrayElement:
		{
			SerializeExpr( iCode, Ar ); // Index expression.
			SerializeExpr( iCode, Ar ); // Base expression.
			break;
		}
		case EX_DynArrayLength:
		{
			SerializeExpr( iCode, Ar ); // Base expression.
			break;
		}
		case EX_DynArrayInsert:
		case EX_DynArrayRemove:
		{
			SerializeExpr( iCode, Ar ); // Base expression
			SerializeExpr( iCode, Ar ); // Index
			SerializeExpr( iCode, Ar ); // Count
			break;
 		}
		case EX_New:
		{
			SerializeExpr( iCode, Ar ); // Parent expression.
			SerializeExpr( iCode, Ar ); // Name expression.
			SerializeExpr( iCode, Ar ); // Flags expression.
			SerializeExpr( iCode, Ar ); // Class expression.
			break;
		}
		case EX_IntConst:
		{
			XFER(INT);
			break;
		}
		case EX_FloatConst:
		{
			XFER(FLOAT);
			break;
		}
		case EX_StringConst:
		{
			do XFER(BYTE) while( Script(iCode-1) );
			break;
		}
		case EX_UnicodeStringConst:
		{
			do XFER(_WORD) while( Script(iCode-1) );
			break;
		}
		case EX_ObjectConst:
		{
			XFERPTR(UObject*);
			break;
		}
		case EX_NameConst:
		{
			XFER(FName);
			break;
		}
		case EX_RotationConst:
		{
			XFER(INT); XFER(INT); XFER(INT);
			break;
		}
		case EX_VectorConst:
		{
			XFER(FLOAT); XFER(FLOAT); XFER(FLOAT);
			break;
		}
		case EX_ByteConst:
		case EX_IntConstByte:
		{
			XFER(BYTE);
			break;
		}
		case EX_MetaCast:
		{
			XFERPTR(UClass*);
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_DynamicCast:
		{
			XFERPTR(UClass*);
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_JumpIfNot:
		{
			XFER(_WORD); // Code offset.
			SerializeExpr( iCode, Ar ); // Boolean expr.
			break;
		}
		case EX_Iterator:
		{
			SerializeExpr( iCode, Ar ); // Iterator expr.
			XFER(_WORD); // Code offset.
			break;
		}
		case EX_Switch:
		{
			XFER(BYTE); // Value size.
			SerializeExpr( iCode, Ar ); // Switch expr.
			break;
		}
		case EX_Assert:
		{
			XFER(_WORD); // Line number.
			SerializeExpr( iCode, Ar ); // Assert expr.
			break;
		}
		case EX_Case:
		{
			_WORD W;
//			_WORD* W=(_WORD*)&Script(iCode);
			XFER(_WORD); // Code offset.
			appMemcpy(&W, &Script(iCode-sizeof(_WORD)), sizeof(_WORD));
			if( W != MAXWORD )
				SerializeExpr( iCode, Ar ); // Boolean expr.
			break;
		}
		case EX_LabelTable:
		{
			check((iCode&3)==0);
			for( ; ; )
			{
				FLabelEntry* E = (FLabelEntry*)&Script(iCode);
				XFER(FLabelEntry);
				if( E->Name == NAME_None )
					break;
			}
			break;
		}
		case EX_GotoLabel:
		{
			SerializeExpr( iCode, Ar ); // Label name expr.
			break;
		}
		case EX_Skip:
		{
			XFER(_WORD); // Skip size.
			SerializeExpr( iCode, Ar ); // Expression to possibly skip.
			break;
		}
		case EX_StructCmpEq:
		case EX_StructCmpNe:
		{
			XFERPTR(UStruct*); // Struct.
			SerializeExpr( iCode, Ar ); // Left expr.
			SerializeExpr( iCode, Ar ); // Right expr.
			break;
		}
		case EX_StructMember:
		{
			XFERPTR(UProperty*); // Property.
			SerializeExpr( iCode, Ar ); // Inner expr.
			break;
		}
		case EX_DelegateProperty:
		{
			XFER(FName);	// Name of function we're assigning to the delegate.
			break;
		}
		default:
		{
			// This should never occur.
			appErrorf( TEXT("Bad expr token %02x"), (BYTE)Expr );
			break;
		}
	}
	return Expr;
	#undef XFER
	#undef XFERPTR
}

void UStruct::PostLoad()
{
	Super::PostLoad();
}

void UStruct::PropagateStructDefaults()
{
	for( TFieldIterator<UStructProperty,CLASS_None,0> It(this); It; ++It )
	{
		UStructProperty* StructProperty = *It;
		for( INT i=0; i<StructProperty->ArrayDim; i++ )
			StructProperty->CopySingleValue( &Defaults(0) + StructProperty->Offset + i * StructProperty->ElementSize, &StructProperty->Struct->Defaults(0), NULL );
	}
}

void UStruct::AllocateStructDefaults()
{
	Defaults.Empty( GetPropertiesSize() );
	Defaults.AddZeroed( GetPropertiesSize() );
}

void UStruct::FindInstancedComponents(TMap<FName,UComponent*>& InstanceMap,BYTE* Data,UObject* Owner)
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
	{
		if( It->PropertyFlags & CPF_Component )
			It->FindInstancedComponents( InstanceMap, Data+It->Offset, Owner );
	}
}

void UStruct::FixLegacyComponents(BYTE* Data,BYTE* Defaults,UObject* Owner)
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
	{
		if( It->PropertyFlags & CPF_Component )
			It->FixLegacyComponents( Data+It->Offset, Defaults+It->Offset, Owner );
	}
}

void UStruct::InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
	{
		if( It->PropertyFlags & CPF_Component )
			It->InstanceComponents( InstanceMap, Data+It->Offset, Owner );
	}
}

void UStruct::FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It; ++It )
	{
		if( It->PropertyFlags & CPF_Component )
			It->FixupComponentReferences( InstanceMap, Data+It->Offset, Owner );
	}
}

/*-----------------------------------------------------------------------------
	UFunction.
-----------------------------------------------------------------------------*/

UFunction::UFunction( UFunction* InSuperFunction )
: UStruct( InSuperFunction )
{}
void UFunction::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Function info.
	Ar << iNative;
	Ar << OperPrecedence;
	Ar << FunctionFlags;

	// Replication info.
	if( FunctionFlags & FUNC_Net )
		Ar << RepOffset;

	// Precomputation.
	if( Ar.IsLoading() )
	{
		NumParms          = 0;
		ParmsSize         = 0;
		ReturnValueOffset = MAXWORD;
		for( UProperty* Property=Cast<UProperty>(Children); Property && (Property->PropertyFlags & CPF_Parm); Property=Cast<UProperty>(Property->Next) )
		{
			NumParms++;
			ParmsSize = Property->Offset + Property->GetSize();
			if( Property->PropertyFlags & CPF_ReturnParm )
				ReturnValueOffset = Property->Offset;
		}
	}

	Ar << FriendlyName;
}
void UFunction::PostLoad()
{
	Super::PostLoad();
}
UProperty* UFunction::GetReturnProperty()
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(this); It && (It->PropertyFlags & CPF_Parm); ++It )
		if( It->PropertyFlags & CPF_ReturnParm )
			return *It;
	return NULL;
}
void UFunction::Bind()
{
	if( !(FunctionFlags & FUNC_Native) )
	{
		// Use UnrealScript processing function.
		check(iNative==0);
		Func = &UObject::ProcessInternal;
	}
	else if( iNative != 0 )
	{
		// Find hardcoded native.
		check(iNative<EX_Max);
		check(GNatives[iNative]!=0);
		Func = GNatives[iNative];
	}
	else
	{
		// Find dynamic native.
		TCHAR Proc[256];
		appSprintf( Proc, TEXT("int%s%sexec%s"), GetOwnerClass()->GetPrefixCPP() ,GetOwnerClass()->GetName(), GetName() );
		UPackage* ClassPackage = GetOwnerClass()->GetOuterUPackage();
		Native* Ptr = (Native*)ClassPackage->GetExport( Proc, 1 );
		if( Ptr )
			Func = *Ptr;
	}
}
void UFunction::Link( FArchive& Ar, UBOOL Props )
{
	Super::Link( Ar, Props );
}
IMPLEMENT_CLASS(UFunction);

/*-----------------------------------------------------------------------------
	UConst.
-----------------------------------------------------------------------------*/

UConst::UConst( UConst* InSuperConst, const TCHAR* InValue )
:	UField( InSuperConst )
,	Value( InValue )
{}
void UConst::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Value;
}
IMPLEMENT_CLASS(UConst);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

