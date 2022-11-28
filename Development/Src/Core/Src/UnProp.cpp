/*=============================================================================
	UnClsPrp.cpp: FProperty implementation
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

//@todo: fix hardcoded lengths
/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

//
// Parse a hex digit.
//
static INT HexDigit( TCHAR c )
{
	if( c>='0' && c<='9' )
		return c - '0';
	else if( c>='a' && c<='f' )
		return c + 10 - 'a';
	else if( c>='A' && c<='F' )
		return c + 10 - 'A';
	else
		return 0;
}

//
// Parse a token.
//
const TCHAR* ReadToken( const TCHAR* Buffer, FString& String, UBOOL DottedNames=0 )
{
	if( *Buffer == 0x22 )
	{
		// Get quoted string.
		Buffer++;
		while( *Buffer && *Buffer!=0x22 && *Buffer!=13 && *Buffer!=10 )
		{
			if( *Buffer != '\\' )
			{
				String = FString::Printf(TEXT("%s%c"), *String, *Buffer++);
			}
			else if( *++Buffer=='\\' )
			{
				String += TEXT("\\");
				Buffer++;
			}
			else
			{
				String = FString::Printf(TEXT("%s%c"), *String, HexDigit(Buffer[0])*16 + HexDigit(Buffer[1]));
				Buffer += 2;
			}
		}
		if( *Buffer++!=0x22 )
		{
			warnf( NAME_Warning, TEXT("ReadToken: Bad quoted string") );
			return NULL;
		}
	}
	else if( appIsAlnum( *Buffer ) )
	{
		// Get identifier.
		while( (appIsAlnum(*Buffer) || *Buffer=='_' || *Buffer=='-' || (DottedNames && *Buffer=='.' )) )
			String = FString::Printf(TEXT("%s%c"), *String, *Buffer++);
	}
	else
	{
		// Get just one.
		String = FString::Printf(TEXT("%s%c"), *String, *Buffer);
	}
	return Buffer;
}

/*-----------------------------------------------------------------------------
	UProperty implementation.
-----------------------------------------------------------------------------*/

//
// Constructors.
//
UProperty::UProperty()
:	UField( NULL )
,	ArrayDim( 1 )
,	NextRef( NULL )
{}
UProperty::UProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
:	UField( NULL )
,	ArrayDim( 1 )
,	PropertyFlags( InFlags )
,	Category( InCategory )
,	Offset( InOffset )
,	NextRef( NULL )
{
	GetOuterUField()->AddCppProperty( this );
}
void UProperty::CleanupDestroyed( BYTE* Data, UObject* Owner ) const {}

//
// Serializer.
//
void UProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Archive the basic info.
	Ar << ArrayDim << PropertyFlags << Category;
	
	if( PropertyFlags & CPF_Net )
		Ar << RepOffset;
	if( Ar.IsLoading() )
	{
		Offset = 0;
		ConstructorLinkNext = NULL;
	}
}

//
// Export this class property to an output
// device as a C++ header file.
//
void UProperty::ExportCpp( FOutputDevice& Out, UBOOL IsLocal, UBOOL IsParm, UBOOL IsEvent, UBOOL IsStruct ) const
{
	TCHAR ArrayStr[80] = TEXT("");
	if
	(	IsParm
	&&	IsA(UStrProperty::StaticClass())
	&&	!(PropertyFlags & CPF_OutParm) )
		Out.Log( TEXT("const ") );
	ExportCppItem( Out, IsEvent || IsStruct );
	if( ArrayDim != 1 )
		appSprintf( ArrayStr, TEXT("[%i]"), ArrayDim );
	if( IsA(UBoolProperty::StaticClass()) )
	{
		if( ArrayDim==1 && !IsLocal && !IsParm )
			Out.Logf( TEXT(" %s%s:1"), GetName(), ArrayStr );
		else if( IsParm && (PropertyFlags & CPF_OutParm) )
			Out.Logf( TEXT("& %s%s"), GetName(), ArrayStr );
		else
			Out.Logf( TEXT(" %s%s"), GetName(), ArrayStr );
	}
	else if( IsA(UStrProperty::StaticClass()) )
	{
		if( IsParm && ArrayDim>1 )
			Out.Logf( TEXT("* %s"), GetName() );
		else if( IsParm )
			Out.Logf( TEXT("& %s"), GetName() );
		else if( IsLocal )
			Out.Logf( TEXT(" %s"), GetName() );
		else if( IsStruct )
			Out.Logf( TEXT(" %s%s"), GetName(), ArrayStr );
		else
			Out.Logf( TEXT("NoInit %s%s"), GetName(), ArrayStr );
	}
	else
	{
		if( IsParm && ArrayDim>1 )
			Out.Logf( TEXT("* %s"), GetName() );
		else if( IsParm && (PropertyFlags & CPF_OutParm) )
			Out.Logf( TEXT("& %s%s"), GetName(), ArrayStr );
		else
			Out.Logf( TEXT(" %s%s"), GetName(), ArrayStr );
	}
}

//
// Export the contents of a property.
//
void UProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	ExportTextItem(ValueStr,PropertyValue,DefaultValue,PortFlags);
}
UBOOL UProperty::ExportText
(
	INT		Index,
	FString& ValueStr,
	BYTE*	Data,
	BYTE*	Delta,
	UObject* Parent,
	INT		PortFlags
) const
{
	if( Data==Delta || !Matches(Data,Delta,Index) )
	{
		ExportTextItem
		(
			ValueStr,
			Data + Offset + Index * ElementSize,
			Delta ? (Delta + Offset + Index * ElementSize) : NULL,
			Parent,
			PortFlags
		);
		return 1;
	}
	else return 0;
}

//
// Copy a unique instance of a value.
//
void UProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	appMemcpy( Dest, Src, ElementSize );
}

//
// Destroy a value.
//
void UProperty::DestroyValue( void* Dest ) const
{}

//
// Net serialization.
//
UBOOL UProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	SerializeItem( Ar, Data, 0 );
	return 1;
}

//
// Return whether the property should be exported.
//
UBOOL UProperty::Port() const
{
	return 
	(	GetSize()
	&&	(Category!=NAME_None || !(PropertyFlags & (CPF_Transient | CPF_Native)))
	&&	GetFName()!=NAME_Class );
}

//
// Return type id for encoding properties in .u files.
//
BYTE UProperty::GetID() const
{
	return GetClass()->GetFName().GetIndex();
}

//
// Copy a complete value.
//
void UProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	for( INT i=0; i<ArrayDim; i++ )
		CopySingleValue( (BYTE*)Dest+i*ElementSize, (BYTE*)Src+i*ElementSize, SuperObject );
}

//
// Link property loaded from file.
//
void UProperty::Link( FArchive& Ar, UProperty* Prev )
{}

IMPLEMENT_CLASS(UProperty);

/*-----------------------------------------------------------------------------
	UByteProperty.
-----------------------------------------------------------------------------*/

void UByteProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(BYTE);
	Offset      = Align( GetOuterUField()->GetPropertiesSize(), sizeof(BYTE) );
}
void UByteProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(BYTE*)Dest = *(BYTE*)Src;
}
void UByteProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	if( ArrayDim==1 )
		*(BYTE*)Dest = *(BYTE*)Src;
	else
		appMemcpy( Dest, Src, ArrayDim );
}
UBOOL UByteProperty::Identical( const void* A, const void* B ) const
{
	return *(BYTE*)A == (B ? *(BYTE*)B : 0);
}
void UByteProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(BYTE*)Value;
}
UBOOL UByteProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	Ar.SerializeBits( Data, Enum ? appCeilLogTwo(Enum->Names.Num()) : 8 );
	return 1;
}
void UByteProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Enum;
}
void UByteProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("BYTE") );
}
void UByteProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	if( Enum )
		ValueStr += FString::Printf( TEXT("%s"), *PropertyValue < Enum->Names.Num() ? *Enum->Names(*PropertyValue) : TEXT("(INVALID)") );
	else
		ValueStr += FString::Printf( TEXT("%i"), *PropertyValue );
}
const TCHAR* UByteProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	FString Temp;
	if( Enum )
	{
		Buffer = ReadToken( Buffer, Temp );
		if( !Buffer )
			return NULL;
		FName EnumName = FName( *Temp, FNAME_Find );
		if( EnumName != NAME_None )
		{
			INT EnumIndex=0;
			if( Enum->Names.FindItem( EnumName, EnumIndex ) )
			{
				*(BYTE*)Data = EnumIndex;
				return Buffer;
			}
		}
	}
	if( appIsDigit(*Buffer) )
	{
		*(BYTE*)Data = appAtoi( Buffer );
		while( *Buffer>='0' && *Buffer<='9' )
			Buffer++;
	}
	else
	{
		//debugf( "Import: Missing byte" );
		return NULL;
	}
	return Buffer;
}
IMPLEMENT_CLASS(UByteProperty);

/*-----------------------------------------------------------------------------
	UIntProperty.
-----------------------------------------------------------------------------*/

void UIntProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(INT);
	Offset      = Align( GetOuterUField()->GetPropertiesSize(), sizeof(INT) );
}
void UIntProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(INT*)Dest = *(INT*)Src;
}
void UIntProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	if( ArrayDim==1 )
		*(INT*)Dest = *(INT*)Src;
	else
		for( INT i=0; i<ArrayDim; i++ )
			((INT*)Dest)[i] = ((INT*)Src)[i];
}
UBOOL UIntProperty::Identical( const void* A, const void* B ) const
{
	return *(INT*)A == (B ? *(INT*)B : 0);
}
void UIntProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(INT*)Value;
}
UBOOL UIntProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	Ar << *(INT*)Data;
	return 1;
}
void UIntProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("INT") );
}
void UIntProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	ValueStr += FString::Printf( TEXT("%i"), *(INT *)PropertyValue );
}
const TCHAR* UIntProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	if( *Buffer=='-' || (*Buffer>='0' && *Buffer<='9') )
		*(INT*)Data = appAtoi( Buffer );
	while( *Buffer=='-' || (*Buffer>='0' && *Buffer<='9') )
		Buffer++;
	return Buffer;
}
IMPLEMENT_CLASS(UIntProperty);

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

void UDelegateProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(FScriptDelegate);
	Offset      = Align( GetOuterUField()->GetPropertiesSize(), sizeof(INT) );
	PropertyFlags |= CPF_NeedCtorLink;
}
void UDelegateProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(FScriptDelegate*)Dest = *(FScriptDelegate*)Src;
}
void UDelegateProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
    if( SuperObject )
	{
		if( ArrayDim==1)
		{
			UClass* Cls = Cast<UClass>(((FScriptDelegate*)Src)->Object);			
			((FScriptDelegate*)Dest)->FunctionName = ((FScriptDelegate*)Src)->FunctionName;
			((FScriptDelegate*)Dest)->Object = (Cls && SuperObject->IsA(Cls)) ? SuperObject : ((FScriptDelegate*)Src)->Object;
		}
		else
		{
			for( INT i=0; i<ArrayDim; i++ )
			{
				UClass* Cls = Cast<UClass>(((FScriptDelegate*)Src)[i].Object);			
				((FScriptDelegate*)Dest)[i].FunctionName = ((FScriptDelegate*)Src)[i].FunctionName;
				((FScriptDelegate*)Dest)[i].Object = (Cls && SuperObject->IsA(Cls)) ? SuperObject : ((FScriptDelegate*)Src)[i].Object;
			}
		}
	}
	else
	{
	if( ArrayDim==1 )
		*(FScriptDelegate*)Dest = *(FScriptDelegate*)Src;
	else
		for( INT i=0; i<ArrayDim; i++ )
			((FScriptDelegate*)Dest)[i] = ((FScriptDelegate*)Src)[i];
	}
}
UBOOL UDelegateProperty::Identical( const void* A, const void* B ) const
{
	FScriptDelegate* DA = (FScriptDelegate*)A;
	FScriptDelegate* DB = (FScriptDelegate*)B;
	if( !DB )
		return DA->Object==NULL;
	return ( DA->Object == DB->Object && DA->FunctionName == DB->FunctionName );
}
void UDelegateProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(FScriptDelegate*)Value;
}
UBOOL UDelegateProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	Ar << *(FScriptDelegate*)Data;
	return 1;
}
void UDelegateProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("FScriptDelegate") );
}
void UDelegateProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	FScriptDelegate *scriptDelegate = (FScriptDelegate*)PropertyValue;
	checkSlow(scriptDelegate != NULL);
	ValueStr += FString::Printf( TEXT("%s.%s"),
								 scriptDelegate->Object != NULL ? scriptDelegate->Object->GetName() : TEXT("(null)"),
								 scriptDelegate->FunctionName );
}
const TCHAR* UDelegateProperty::ImportText( const TCHAR* Buffer, BYTE* PropertyValue, INT PortFlags, UObject* Parent ) const
{
	TCHAR ObjName[NAME_SIZE];
	TCHAR FuncName[NAME_SIZE];
	// Get object name
	INT i;
	for( i=0; *Buffer && *Buffer != '.' && *Buffer != ')'; Buffer++ )
		ObjName[i++] = *Buffer;
	ObjName[i] = '\0';
	// Get function name
	if( *Buffer )
	{
		Buffer++;
		for( i=0; *Buffer && *Buffer != ')'; Buffer++ )
			FuncName[i++] = *Buffer;
		FuncName[i] = '\0';                
	}
	else
	{
		FuncName[0] = '\0';
	}
	UObject* Object = StaticFindObject( UObject::StaticClass(), ANY_PACKAGE, ObjName );
	UFunction* Func = NULL;
	if( Object )
	{
        UClass* Cls = Cast<UClass>(Object);
		if( !Cls )
			Cls = Object->GetClass();
		// Check function params.
		Func = FindField<UFunction>( Cls, FuncName );
		if( Func )
		{
			// Find the delegate UFunction to check params
			UFunction* Delegate = Function;
			check(Delegate && "Invalid delegate property");
			// check return type and params
			if(	Func->NumParms == Delegate->NumParms )
			{
				INT Count=0;
				for( TFieldIterator<UProperty,CLASS_IsAUProperty> It1(Func),It2(Delegate); Count<Delegate->NumParms; ++It1,++It2,++Count )
				{
					if( It1->GetClass()!=It2->GetClass() || (It1->PropertyFlags&CPF_OutParm)!=(It2->PropertyFlags&CPF_OutParm) )
					{
						Func = NULL;
						break;
					}
				}
			}
			else
			{
				Func = NULL;
			}
		}
	}
	(*(FScriptDelegate*)PropertyValue).Object		= Func ? Object				: NULL;
	(*(FScriptDelegate*)PropertyValue).FunctionName = Func ? Func->GetFName()	: NAME_None;
	return Buffer;
}

void UDelegateProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Function;
	Ar << DelegateName;
}
void UDelegateProperty::CleanupDestroyed( BYTE* Data, UObject* Owner ) const
{
	for(int i=0; i<ArrayDim; i++)
	{
		FScriptDelegate* Delegate = (FScriptDelegate*)(Data+i*ElementSize);
		if(Delegate->Object)
		{
			check(Delegate->Object->IsValid());
			if( Delegate->Object->IsPendingKill() )
			{
				Owner->Modify();
				Delegate->Object = NULL;
				Delegate->FunctionName = NAME_None;
			}
		}
	}
}
IMPLEMENT_CLASS(UDelegateProperty);

/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

void UBoolProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	UBoolProperty* PrevBool = Cast<UBoolProperty>( Prev );
	if( GetOuterUField()->MergeBools() && PrevBool && NEXT_BITFIELD(PrevBool->BitMask) )
	{
		Offset  = Prev->Offset;
		BitMask = NEXT_BITFIELD(PrevBool->BitMask);
	}
	else
	{
		Offset  = Align(GetOuterUField()->GetPropertiesSize(),sizeof(BITFIELD));
		BitMask = FIRST_BITFIELD;
	}
	ElementSize = sizeof(BITFIELD);
}
void UBoolProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << BitMask;
}
void UBoolProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("BITFIELD") );
}
void UBoolProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	TCHAR* Temp
	=	(TCHAR*) ((PortFlags & PPF_Localized)
	?	(((*(BITFIELD*)PropertyValue) & BitMask) ? GTrue  : GFalse )
	:	(((*(BITFIELD*)PropertyValue) & BitMask) ? TEXT("True") : TEXT("False")));
	ValueStr += FString::Printf( TEXT("%s"), Temp );
}
const TCHAR* UBoolProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	FString Temp;
	Buffer = ReadToken( Buffer, Temp );
	if( !Buffer )
		return NULL;
	if( Temp==TEXT("1") || Temp==TEXT("True") || Temp==GTrue )
	{
		*(BITFIELD*)Data |= BitMask;
	}
	else 
	if( Temp==TEXT("0") || Temp==TEXT("False") || Temp==GFalse )
	{
		*(BITFIELD*)Data &= ~BitMask;
	}
	else
	{
		//debugf( "Import: Failed to get bool" );
		return NULL;
	}
	return Buffer;
}
UBOOL UBoolProperty::Identical( const void* A, const void* B ) const
{
	return ((*(BITFIELD*)A ^ (B ? *(BITFIELD*)B : 0)) & BitMask) == 0;
}
void UBoolProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	BYTE B = (*(BITFIELD*)Value & BitMask) ? 1 : 0;
	Ar << B;
	if( B ) *(BITFIELD*)Value |=  BitMask;
	else    *(BITFIELD*)Value &= ~BitMask;
}
UBOOL UBoolProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	BYTE Value = ((*(BITFIELD*)Data & BitMask)!=0);
	Ar.SerializeBits( &Value, 1 );
	if( Value )
		*(BITFIELD*)Data |= BitMask;
	else
		*(BITFIELD*)Data &= ~BitMask;
	return 1;
}
void UBoolProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(BITFIELD*)Dest = (*(BITFIELD*)Dest & ~BitMask) | (*(BITFIELD*)Src & BitMask);
}
IMPLEMENT_CLASS(UBoolProperty);

/*-----------------------------------------------------------------------------
	UFloatProperty.
-----------------------------------------------------------------------------*/

void UFloatProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(FLOAT);
	Offset      = Align( GetOuterUField()->GetPropertiesSize(), sizeof(FLOAT) );
}
void UFloatProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(FLOAT*)Dest = *(FLOAT*)Src;
}
void UFloatProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	if( ArrayDim==1 )
		*(FLOAT*)Dest = *(FLOAT*)Src;
	else
		for( INT i=0; i<ArrayDim; i++ )
			((FLOAT*)Dest)[i] = ((FLOAT*)Src)[i];
}
UBOOL UFloatProperty::Identical( const void* A, const void* B ) const
{
	return *(FLOAT*)A == (B ? *(FLOAT*)B : 0);
}
void UFloatProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(FLOAT*)Value;
}
UBOOL UFloatProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	Ar << *(FLOAT*)Data;
	return 1;
}
void UFloatProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("FLOAT") );
}
void UFloatProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	ValueStr += FString::Printf( TEXT("%f"), *(FLOAT*)PropertyValue );
}
const TCHAR* UFloatProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	*(FLOAT*)Data = appAtof(Buffer);
	while( *Buffer && *Buffer!=',' && *Buffer!=')' && *Buffer!=13 && *Buffer!=10 )
		Buffer++;
	return Buffer;
}
IMPLEMENT_CLASS(UFloatProperty);

/*-----------------------------------------------------------------------------
	UObjectProperty.
-----------------------------------------------------------------------------*/

void UObjectProperty::CleanupDestroyed( BYTE* Data, UObject* Owner ) const
{
	for(int i=0; i<ArrayDim; i++)
	{
		UObject*& Obj=*(UObject**)(Data+i*ElementSize);
		if(Obj)
		{
			check(Obj->IsValid());
			if( Obj->IsPendingKill() && (GetFName() != NAME_Outer) )
			{
				//debugf( TEXT("CleanupDestroyed: Clearing %s In %s"), GetName(), Owner->GetName() );

				if(Owner) // Can we ever NOT have an Owner?
				{
					Owner->Modify();
				}

				Obj = NULL;
			}
		}
	}
}
void UObjectProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(UObject*);
	Offset      = Align( GetOuterUField()->GetPropertiesSize(), sizeof(UObject*) );
	if ((PropertyFlags & CPF_EditInline) &&
		(PropertyFlags & CPF_ExportObject) &&
		!(PropertyFlags & CPF_Component))
	{
		PropertyFlags |= CPF_NeedCtorLink;
	}
}
void UObjectProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(UObject**)Dest = *(UObject**)Src;
}
void UObjectProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	//!! Don't instance subobjects if running UCC MAKE.
	if( !(GIsEditor&&GIsUCCMake) && (PropertyFlags & CPF_NeedCtorLink) && SuperObject )
	{
		for( INT i=0; i<ArrayDim; i++ )
		{
			if( ((UObject**)Src)[i] )
			{
				UClass* Cls = ((UObject**)Src)[i]->GetClass();
				((UObject**)Dest)[i] = StaticConstructObject( Cls, SuperObject->GetOuter(), NAME_None, 0, ((UObject**)Src)[i], GError, SuperObject ); 
			}
			else
				((UObject**)Dest)[i] = ((UObject**)Src)[i];
		}
	}
	else
	{
		for( INT i=0; i<ArrayDim; i++ )
			((UObject**)Dest)[i] = ((UObject**)Src)[i];
	}
}
UBOOL UObjectProperty::Identical( const void* A, const void* B ) const
{
	return *(UObject**)A == (B ? *(UObject**)B : NULL);
}
void UObjectProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(UObject**)Value;
}
UBOOL UObjectProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	return Map->SerializeObject( Ar, PropertyClass, *(UObject**)Data );
}
void UObjectProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << PropertyClass;
}
void UObjectProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Logf( TEXT("class %s%s*"), PropertyClass->GetPrefixCPP(), PropertyClass->GetName() );
}
void UObjectProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	UObject* Temp = *(UObject **)PropertyValue;
	if( Temp != NULL )
	{
		if((GUglyHackFlags & 0x04) && Parent)
		{
			if( Temp->GetOuter() == Parent->GetOuter() || Temp->GetOuter() == Parent )
			{
				ValueStr += FString::Printf( TEXT("%s'%s'"), Temp->GetClass()->GetName(), Temp->GetName() );
			}
			else
			{
				ValueStr += FString::Printf( TEXT("%s'%s'"), Temp->GetClass()->GetName(), *Temp->GetPathName() );
			}
		}
		else
		{
			ValueStr += FString::Printf( TEXT("%s'%s'"), Temp->GetClass()->GetName(), *Temp->GetPathName() );
		}
	}
	else
	{
		ValueStr += TEXT("None");
	}
}
const TCHAR* UObjectProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
    const TCHAR* Buffer = InBuffer;
	FString Temp;
	Buffer = ReadToken( Buffer, Temp, 1 );
	if( !Buffer )
	{
		return NULL;
	}
	if( Temp==TEXT("None") )
	{
		*(UObject**)Data = NULL;
	}
	else
	{
		UClass*	ObjectClass = PropertyClass;

		while( *Buffer == ' ' )
			Buffer++;
		if( *Buffer == '\'' )
		{
			FString Other;
			Buffer++;
			Buffer = ReadToken( Buffer, Other, 1 );
			if( !Buffer )
				return NULL;
			if( *Buffer++ != '\'' )
				return NULL;
			ObjectClass = FindObject<UClass>( ANY_PACKAGE, *Temp );
			if( !ObjectClass )
            {
                if( PortFlags & PPF_CheckReferences )
					warnf( NAME_Error, TEXT("%s: unresolved cast in '%s'"), *GetFullName(), InBuffer );
				return NULL;
			}
			// If ObjectClass is not a subclass of PropertyClass, fail.
			if( !ObjectClass->IsChildOf(PropertyClass) )
			{
				warnf( NAME_Error, TEXT("%s: invalid cast in '%s'"), *GetFullName(), InBuffer );
				return NULL;
			}

			// Try the find the object.
			*(UObject**)Data = FindImportedObject(Parent,ObjectClass,*Other);
		}
		else
		{
			// Try the find the object.
			*(UObject**)Data = FindImportedObject(Parent,ObjectClass,*Temp);
		}
        if( *(UObject**)Data && !(*(UObject**)Data)->GetClass()->IsChildOf(PropertyClass) )
		{
			if(PortFlags & PPF_CheckReferences)
				warnf( NAME_Error, TEXT("%s: bad cast in '%s'"), *GetFullName(), InBuffer );
			*(UObject**)Data = NULL;
		}

		// If we couldn't find it or load it, we'll have to do without it.
		if( !*(UObject**)Data )
        {
            if( PortFlags & PPF_CheckReferences )
				warnf( NAME_Warning, TEXT("%s: unresolved reference to '%s'"), *GetFullName(), InBuffer );
			return NULL;
        }
	}
	return Buffer;
}
UObject* UObjectProperty::FindImportedObject(UObject* Parent,UClass* ObjectClass,const TCHAR* Text) const
{
	UObject*	Result = NULL;

	check( ObjectClass->IsChildOf(PropertyClass) );

	// If hack flag is set, 
	if( (GUglyHackFlags & 0x02) && Parent )
	{
		Result = StaticFindObject(ObjectClass,Parent->GetOuter(),Text);
	}

	if(!Result)
	{
		Result = StaticFindObject(ObjectClass,ANY_PACKAGE,Text);
	}

	// If we can't find it, try to load it.
	if(!Result)
	{
		Result = StaticLoadObject(ObjectClass,NULL,Text,NULL,LOAD_NoWarn|LOAD_FindIfFail,NULL);
	}

	check(!Result || Result->IsA(PropertyClass));

	return Result;
}
IMPLEMENT_CLASS(UObjectProperty);

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

void UClassProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << MetaClass;
	check(MetaClass);
}
const TCHAR* UClassProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	const TCHAR* Result = UObjectProperty::ImportText( Buffer, Data, PortFlags, Parent );
	if( Result )
	{
		// Validate metaclass.
		UClass*& C = *(UClass**)Data;
		if( C && C->GetClass()!=UClass::StaticClass() || !C->IsChildOf(MetaClass) )
			C = NULL;
	}
	return Result;
}
IMPLEMENT_CLASS(UClassProperty);

/*-----------------------------------------------------------------------------
	UComponentProperty.
-----------------------------------------------------------------------------*/
void UComponentProperty::FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		for(INT Index = 0;Index < ArrayDim;Index++)
		{
			UComponent** Component = (UComponent**)(Data+Index*ElementSize);
			if( *Component && (*Component)->IsIn(Owner))
			{
				UClass*	OwnerClass = Owner->IsA(UClass::StaticClass()) ? CastChecked<UClass>(Owner) : Owner->GetClass();
				FName*	ComponentName = OwnerClass->ComponentClassToNameMap.Find((*Component)->GetClass());
				if(ComponentName)
				{
					if(!InstanceMap.Find(*ComponentName))
					{
						InstanceMap.Set(*ComponentName,*Component);
						(*Component)->GetClass()->FindInstancedComponents(InstanceMap,(BYTE*)*Component,Owner);
					}
				}
			}
		}
	}
}
void UComponentProperty::FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		for(INT Index = 0;Index < ArrayDim;Index++)
		{
			UComponent**	Component = (UComponent**)(Data+Index*ElementSize);
			UComponent**	DefaultComponent = (UComponent**)(Defaults+Index*ElementSize);
			if( *Component && *DefaultComponent )
			{
				UClass*	OwnerClass = Owner->IsA(UClass::StaticClass()) ? CastChecked<UClass>(Owner) : Owner->GetClass();
				FName*	ComponentName = OwnerClass->ComponentClassToNameMap.Find((*Component)->GetClass());
				FName*	DefaultComponentName = OwnerClass->ComponentClassToNameMap.Find((*DefaultComponent)->GetClass());
				if(!ComponentName && DefaultComponentName)
				{
					if((*DefaultComponent)->IsA((*Component)->GetClass()) && (*DefaultComponent)->GetClass()->PropertiesSize == (*Component)->GetClass()->PropertiesSize)
						(*Component)->SetClass((*DefaultComponent)->GetClass());
				}
				else if(ComponentName && DefaultComponentName)
				{
					if(*ComponentName != *DefaultComponentName)
						debugf(TEXT("Component name of instance %s doesn't equal component name of default object: %s %s"),*(*Component)->GetFullName(),**ComponentName,**DefaultComponentName);
					if((*Component)->GetClass() != (*DefaultComponent)->GetClass())
					{
						if((*DefaultComponent)->IsA((*Component)->GetClass()) && (*DefaultComponent)->GetClass()->PropertiesSize == (*Component)->GetClass()->PropertiesSize)
						{
							(*Component)->SetClass((*DefaultComponent)->GetClass());
						}
					}
				}
			}
		}
	}
}
void UComponentProperty::InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		for(INT Index = 0;Index < ArrayDim;Index++)
		{
			UComponent** Component = (UComponent**)(Data+Index*ElementSize);
			if( *Component )
			{
				UClass*	OwnerClass = Owner->IsA(UClass::StaticClass()) ? CastChecked<UClass>(Owner) : Owner->GetClass();
				FName*	ComponentName = OwnerClass->ComponentClassToNameMap.Find((*Component)->GetClass());
				if(ComponentName)
				{
                    UComponent** Instance = InstanceMap.Find(*ComponentName);
					if( Instance )
					{
						*Component = *Instance;
					}
					else
					{
						UComponent**	ComponentDefaultObject = OwnerClass->ComponentNameToDefaultObjectMap.Find(*ComponentName);
						check(Owner);
						check(ComponentDefaultObject);
						UComponent* NewComponent = CastChecked<UComponent>( StaticConstructObject( (*ComponentDefaultObject)->GetClass(), Owner, NAME_None, RF_Public, *ComponentDefaultObject, GError, Owner ) ); 
						InstanceMap.Set( *ComponentName, NewComponent );
						*Component = NewComponent;
						(*Component)->GetClass()->InstanceComponents(InstanceMap,(BYTE*)*Component,Owner);
					}
				}
			}
		}
	}
}
void UComponentProperty::FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	static TArray<UComponent*> VisitedComponents;
	if(!(PropertyFlags & CPF_Native))
	{
		for(INT Index = 0;Index < ArrayDim;Index++)
		{
			UComponent** Component = (UComponent**)(Data+Index*ElementSize);
			if( *Component )
			{
				UClass*	OwnerClass = Owner->IsA(UClass::StaticClass()) ? CastChecked<UClass>(Owner) : Owner->GetClass();
				FName*	ComponentName = OwnerClass->ComponentClassToNameMap.Find((*Component)->GetClass());
				if(ComponentName)
				{
					UComponent** Instance = InstanceMap.Find(*ComponentName);
					if( Instance )
						*Component = *Instance;

					if(VisitedComponents.FindItemIndex(*Component) == INDEX_NONE)
					{
						VisitedComponents.AddItem(*Component);
						(*Component)->GetClass()->FixupComponentReferences( InstanceMap, (BYTE*)*Component, Owner );
						VisitedComponents.Pop();
					}
				}
			}
		}
	}
}
void UComponentProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	UComponent*	Component = *(UComponent**)PropertyValue;
	UClass*		ParentClass = Parent ? Parent->IsA(UClass::StaticClass()) ? CastChecked<UClass>(Parent) : Parent->GetClass() : NULL;
	if(Component != NULL)
	{
		FName*	ComponentName = ParentClass ? ParentClass->ComponentClassToNameMap.Find(Component->GetClass()) : NULL;
		if(ComponentName)
			ValueStr += FString::Printf(TEXT("%s"),**ComponentName);
		else
			ValueStr += FString::Printf(TEXT("%s"),Component->GetPathName());
	}
	else
		ValueStr += TEXT("None");
}
UBOOL UComponentProperty::Identical(const void* A,const void* B) const
{
	if(Super::Identical(A,B))
		return 1;

	UComponent**	ComponentA = (UComponent**)A;
	UComponent**	ComponentB = (UComponent**)B;

	if(ComponentA && ComponentB && (*ComponentA) && (*ComponentB))
	{
		check((*ComponentA)->IsA(UComponent::StaticClass()));
		check((*ComponentB)->IsA(UComponent::StaticClass()));

		UComponent*	ComponentInstance;
		UComponent*	PotentialComponentDefaultObject;

		if(!(*ComponentA)->GetOuter()->IsA(UClass::StaticClass()) && (*ComponentB)->GetOuter()->IsA(UClass::StaticClass()))
		{
			ComponentInstance = *ComponentA;
			PotentialComponentDefaultObject = *ComponentB;
		}
		else if((*ComponentA)->GetOuter()->IsA(UClass::StaticClass()) && !(*ComponentB)->GetOuter()->IsA(UClass::StaticClass()))
		{
			PotentialComponentDefaultObject = *ComponentA;
			ComponentInstance = *ComponentB;
		}
		else
			return 0;

		if(PotentialComponentDefaultObject->GetClass() != ComponentInstance->GetClass())
			return 0;

		FName*	ComponentName = ComponentInstance->GetOuter()->GetClass()->ComponentClassToNameMap.Find(ComponentInstance->GetClass());
		if(!ComponentName)
			return 0;

		UComponent**	ComponentDefaultObject = ComponentInstance->GetOuter()->GetClass()->ComponentNameToDefaultObjectMap.Find(*ComponentName);
		if(!ComponentDefaultObject || *ComponentDefaultObject != PotentialComponentDefaultObject)
			return 0;

		return 1;
	}
	else
		return 0;
}
const TCHAR* UComponentProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	if(Parent->IsA(UClass::StaticClass()))
	{
		// For default property assignment, only allow references to components declared in the default properties of the class.
		const TCHAR* Buffer = InBuffer;
		FString Temp;
		Buffer = ReadToken( Buffer, Temp, 1 );
		if( !Buffer )
		{
			return NULL;
		}
		if( Temp==TEXT("None") )
		{
			*(UObject**)Data = NULL;
		}
		else
		{
			UObject* Result = NULL;
		
			// Try to find the component through the parent class.
			UClass*	OuterClass = CastChecked<UClass>(Parent);
			while(OuterClass)
			{
				UComponent**	ComponentDefaultObject = OuterClass->ComponentNameToDefaultObjectMap.Find(*Temp);

				if(ComponentDefaultObject && (*ComponentDefaultObject)->IsA(PropertyClass))
				{
					Result = *ComponentDefaultObject;
					break;
				}

				OuterClass = OuterClass->ComponentOwnerClass;
			}

			check(!Result || Result->IsA(PropertyClass));

			*(UObject**)Data = Result;

			// If we couldn't find it or load it, we'll have to do without it.
			if( !*(UObject**)Data )
			{
				if( PortFlags & PPF_CheckReferences )
					warnf( NAME_Warning, TEXT("%s: unresolved reference to '%s'"), *GetFullName(), InBuffer );
				return NULL;
			}

		}
		return Buffer;
	}
	else
	{
		return UObjectProperty::ImportText(InBuffer,Data,PortFlags,Parent);
	}
}
IMPLEMENT_CLASS(UComponentProperty);

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

void UNameProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(FName);
	Offset      = Align( GetOuterUField()->GetPropertiesSize(), sizeof(FName) );
}
void UNameProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(FName*)Dest = *(FName*)Src;
}
void UNameProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	if( ArrayDim==1 )
		*(FName*)Dest = *(FName*)Src;
	else
		for( INT i=0; i<ArrayDim; i++ )
			((FName*)Dest)[i] = ((FName*)Src)[i];
}
UBOOL UNameProperty::Identical( const void* A, const void* B ) const
{
	return *(FName*)A == (B ? *(FName*)B : FName(NAME_None));
}
void UNameProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(FName*)Value;
}
void UNameProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("FName") );
}
void UNameProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	FName Temp = *(FName*)PropertyValue;
	if( !(PortFlags & PPF_Delimited) )
		ValueStr += *Temp;
	else
		ValueStr += FString::Printf( TEXT("\"%s\""), *Temp );
}
const TCHAR* UNameProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	FString Temp;
	Buffer = ReadToken( Buffer, Temp );
	if( !Buffer )
		return NULL;
	*(FName*)Data = FName(*Temp);
	return Buffer;
}
IMPLEMENT_CLASS(UNameProperty);

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

void UStrProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize    = sizeof(FString);
	Offset         = Align( GetOuterUField()->GetPropertiesSize(), PROPERTY_ALIGNMENT );
	if( !(PropertyFlags & CPF_Native) )
		PropertyFlags |= CPF_NeedCtorLink;
}
UBOOL UStrProperty::Identical( const void* A, const void* B ) const
{
	return appStricmp( **(const FString*)A, B ? **(const FString*)B : TEXT("") )==0;
}
void UStrProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	Ar << *(FString*)Value;
}
void UStrProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
}
void UStrProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Log( TEXT("FString") );
}
void UStrProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, INT PortFlags ) const
{
	if( !(PortFlags & PPF_Delimited) )
		ValueStr += **(FString*)PropertyValue;
	else
		ValueStr += FString::Printf( TEXT("\"%s\""), **(FString*)PropertyValue );
}
const TCHAR* UStrProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	if( !(PortFlags & PPF_Delimited) )
	{
		*(FString*)Data = Buffer;
	}
	else
	{
		FString Temp;
		Buffer = ReadToken( Buffer, Temp );
		if( !Buffer )
			return NULL;
		*(FString*)Data = Temp;
	}
	return Buffer;
}
void UStrProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	*(FString*)Dest = *(FString*)Src;
}
void UStrProperty::DestroyValue( void* Dest ) const
{
	for( INT i=0; i<ArrayDim; i++ )
		(*(FString*)((BYTE*)Dest+i*ElementSize)).~FString();
}
IMPLEMENT_CLASS(UStrProperty);

/*-----------------------------------------------------------------------------
	UFixedArrayProperty.
-----------------------------------------------------------------------------*/

void UFixedArrayProperty::CleanupDestroyed( BYTE* Data, UObject* Owner ) const
{
	for(int i=0; i<Count; i++)
		Inner->CleanupDestroyed(Data+i*GetSize(), Owner);
}
void UFixedArrayProperty::Link( FArchive& Ar, UProperty* Prev )
{
	checkSlow(Count>0);
	Super::Link( Ar, Prev );
	Ar.Preload( Inner );
	Inner->Link( Ar, NULL );
	ElementSize    = Inner->ElementSize * Count;
	Offset         = Align( GetOuterUField()->GetPropertiesSize(), PROPERTY_ALIGNMENT );
	if( !(PropertyFlags & CPF_Native) )
		PropertyFlags |= (Inner->PropertyFlags & CPF_NeedCtorLink);
}
UBOOL UFixedArrayProperty::Identical( const void* A, const void* B ) const
{
	checkSlow(Inner);
	for( INT i=0; i<Count; i++ )
		if( !Inner->Identical( (BYTE*)A+i*Inner->ElementSize, B ? ((BYTE*)B+i*Inner->ElementSize) : NULL ) )
			return 0;
	return 1;
}
void UFixedArrayProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	checkSlow(Inner);
	for( INT i=0; i<Count; i++ )
		Inner->SerializeItem( Ar, (BYTE*)Value + i*Inner->ElementSize, MaxReadBytes>0?MaxReadBytes/Count:0 );
}
UBOOL UFixedArrayProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	return 1;
}
void UFixedArrayProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Inner << Count;
	checkSlow(Inner);
}
void UFixedArrayProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	checkSlow(Inner);
	Inner->ExportCppItem( Out );
	Out.Logf( TEXT("[%i]"), Count );
}
void UFixedArrayProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	checkSlow(Inner);
	ValueStr += TEXT("(");
	for( INT i=0; i<Count; i++ )
	{
		if( i>0 )
			ValueStr += TEXT(",");
		Inner->ExportTextItem( ValueStr, PropertyValue + i*Inner->ElementSize, DefaultValue ? (DefaultValue + i*Inner->ElementSize) : NULL, Parent, PortFlags|PPF_Delimited );
	}
	ValueStr += TEXT(")");
}
const TCHAR* UFixedArrayProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	checkSlow(Inner);
	if( *Buffer++ != '(' )
		return NULL;
	appMemzero( Data, ElementSize );
	for( INT i=0; i<Count; i++ )
	{
		Buffer = Inner->ImportText( Buffer, Data + i*Inner->ElementSize, PortFlags|PPF_Delimited, Parent );
		if( !Buffer )
			return NULL;
		if( *Buffer!=',' && i!=Count-1 )
			return NULL;
		Buffer++;
	}
	if( *Buffer++ != ')' )
		return NULL;
	return Buffer;
}
void UFixedArrayProperty::AddCppProperty( UProperty* Property, INT InCount )
{
	check(!Inner);
	check(Property);
	check(InCount>0);

	Inner = Property;
	Count = InCount;
}
void UFixedArrayProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	for( INT i=0; i<Count; i++ )
		Inner->CopyCompleteValue( (BYTE*)Dest + i*Inner->ElementSize, Src ? ((BYTE*)Src + i*Inner->ElementSize) : NULL );
}
void UFixedArrayProperty::DestroyValue( void* Dest ) const
{
	for( INT i=0; i<Count; i++ )
		Inner->DestroyValue( (BYTE*)Dest + i*Inner->ElementSize );
}
IMPLEMENT_CLASS(UFixedArrayProperty);

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/

void UArrayProperty::CleanupDestroyed( BYTE* Data, UObject* Owner ) const
{
	for(int i=0; i<ArrayDim; i++)
	{
		FArray* A=(FArray*)( Data + i*ElementSize );
		for(int j=0; j<A->Num(); j++)
			Inner->CleanupDestroyed((BYTE*)A->GetData() + j*Inner->GetSize(), Owner);
	}
}
void UArrayProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	Ar.Preload( Inner );
	Inner->Link( Ar, NULL );
	ElementSize    = sizeof(FArray);
	Offset         = Align( GetOuterUField()->GetPropertiesSize(), PROPERTY_ALIGNMENT );
	if( !(PropertyFlags & CPF_Native) )
		PropertyFlags |= CPF_NeedCtorLink;
}
UBOOL UArrayProperty::Identical( const void* A, const void* B ) const
{
	checkSlow(Inner);
	INT n = ((FArray*)A)->Num();
	if( n!=(B ? ((FArray*)B)->Num() : 0) )
		return 0;
	INT   c = Inner->ElementSize;
	BYTE* p = (BYTE*)((FArray*)A)->GetData();
	if( B )
	{
		BYTE* q = (BYTE*)((FArray*)B)->GetData();
		for( INT i=0; i<n; i++ )
			if( !Inner->Identical( p+i*c, q+i*c ) )
				return 0;
	}
	else
	{
		for( INT i=0; i<n; i++ )
			if( !Inner->Identical( p+i*c, 0 ) )
				return 0;
	}
	return 1;
}
void UArrayProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	checkSlow(Inner);
	INT   c = Inner->ElementSize;
	INT   n = ((FArray*)Value)->Num();
	Ar << n;
	if( Ar.IsLoading() )
	{
		((FArray*)Value)->Empty( c );
		((FArray*)Value)->AddZeroed( c, n );
	}
	BYTE* p = (BYTE*)((FArray*)Value)->GetData();
	for( INT i=0; i<n; i++ )
		Inner->SerializeItem( Ar, p+i*c, MaxReadBytes>0?MaxReadBytes/n:0 );
}
UBOOL UArrayProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	return 1;
}
void UArrayProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Inner;
	checkSlow(Inner);
}
void UArrayProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	checkSlow(Inner);
	if( IsParam )
		Out.Log( TEXT("TArray<") );
	else
		Out.Log( TEXT("TArrayNoInit<") );
	Inner->ExportCppItem( Out );
	Out.Log( TEXT(">") );
}
void UArrayProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	checkSlow(Inner);
	ValueStr += TEXT("(");
	FArray* Array       = (FArray*)PropertyValue;
	FArray* Default     = (FArray*)DefaultValue;
	INT     ElementSize = Inner->ElementSize;
	for( INT i=0; i<Array->Num(); i++ )
	{
		if( i>0 )
			ValueStr += TEXT(",");
		Inner->ExportTextItem( ValueStr, (BYTE*)Array->GetData() + i*ElementSize, (Default && Default->Num()>=i) ? (BYTE*)Default->GetData() + i*ElementSize : 0, Parent, PortFlags|PPF_Delimited );
	}
	ValueStr += TEXT(")");
}
const TCHAR* UArrayProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	checkSlow(Inner);
	if( *Buffer++ != '(' )
		return NULL;
	FArray* Array       = (FArray*)Data;
	INT     ElementSize = Inner->ElementSize;
	Array->Empty( ElementSize );
	while( *Buffer != ')' )
	{
		INT Index = Array->Add( 1, ElementSize );
		appMemzero( (BYTE*)Array->GetData() + Index*ElementSize, ElementSize );
		Buffer = Inner->ImportText( Buffer, (BYTE*)Array->GetData() + Index*ElementSize, PortFlags|PPF_Delimited, Parent );
		if( !Buffer )
			return NULL;
		if( *Buffer!=',' )
			break;
		Buffer++;
	}
	if( *Buffer++ != ')' )
		return NULL;
	return Buffer;
}
void UArrayProperty::AddCppProperty( UProperty* Property )
{
	check(!Inner);
	check(Property);

	Inner = Property;
}
void UArrayProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	FArray* SrcArray  = (FArray*)Src;
	FArray* DestArray = (FArray*)Dest;
	INT     Size      = Inner->ElementSize;
	DestArray->Empty( Size, SrcArray->Num() );//!!must destruct it if really copying
	if( Inner->PropertyFlags & CPF_NeedCtorLink )
	{
		// Copy all the elements.
		DestArray->AddZeroed( Size, SrcArray->Num() );
		BYTE* SrcData  = (BYTE*)SrcArray->GetData();
		BYTE* DestData = (BYTE*)DestArray->GetData();
		for( INT i=0; i<DestArray->Num(); i++ )
			Inner->CopyCompleteValue( DestData+i*Size, SrcData+i*Size, SuperObject );
	}
	else
	{
		// Copy all the elements.
		DestArray->Add( SrcArray->Num(), Size );
		appMemcpy( DestArray->GetData(), SrcArray->GetData(), SrcArray->Num()*Size );
	}
}
void UArrayProperty::DestroyValue( void* Dest ) const
{
	//!! fix for ucc make crash
	// This is the simplest way to solve a crash which happens when loading a package in ucc make that
	// contains an instance of a newly compiled class.  The class has been saved to the .u file, but the
	// class in-memory isn't associated with the appropriate export of the .u file.  Upon encountering
	// the instance of the newly compiled class in the package being loaded, it attempts to load the class
	// from the .u file.  The class in the .u file is loaded into memory over the existing in-memory class,
	// causing the destruction of the existing class.  The class destructs it's default properties, which
	// involves calling DestroyValue for each of the class's properties.  However, some of the properties
	// may be in an interim state between being created to represent an export in the package and being
	// deserialized from the package.  This can result in UArrayProperty::DestroyValue being called with
	// a bogus Offset of 0.
	if( (Offset == 0) && GetOuter()->IsA(UClass::StaticClass()) )
 		return;
	FArray* DestArray = (FArray*)Dest;
	if( Inner->PropertyFlags & CPF_NeedCtorLink )
	{
		BYTE* DestData = (BYTE*)DestArray->GetData();
		INT   Size     = Inner->ElementSize;
		for( INT i=0; i<DestArray->Num(); i++ )
			Inner->DestroyValue( DestData+i*Size );
	}
	DestArray->~FArray();
}
void UArrayProperty::FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		BYTE* ArrayData = (BYTE*)((FArray*)Data)->GetData();
		if( (Inner->PropertyFlags&CPF_Component) && ArrayData )
			for(INT ElementIndex = 0;ElementIndex < ((FArray*)Data)->Num();ElementIndex++)
				Inner->FindInstancedComponents( InstanceMap, ArrayData + ElementIndex * Inner->ElementSize, Owner );
	}
}
void UArrayProperty::FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		BYTE*	ArrayData = (BYTE*)((FArray*)Data)->GetData();
		BYTE*	DefaultArrayData = (BYTE*)((FArray*)Defaults)->GetData();
		if( (Inner->PropertyFlags&CPF_Component) && ArrayData && DefaultArrayData )
			for(INT ElementIndex = 0;ElementIndex < ((FArray*)Data)->Num() && ElementIndex < ((FArray*)Defaults)->Num();ElementIndex++)
				Inner->FixLegacyComponents( ArrayData + ElementIndex * Inner->ElementSize, DefaultArrayData + ElementIndex * Inner->ElementSize, Owner );
	}
}
void UArrayProperty::InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		BYTE* ArrayData = (BYTE*)((FArray*)Data)->GetData();
		if( (Inner->PropertyFlags&CPF_Component) && ArrayData )
			for(INT ElementIndex = 0;ElementIndex < ((FArray*)Data)->Num();ElementIndex++)
				Inner->InstanceComponents( InstanceMap, ArrayData + ElementIndex * Inner->ElementSize, Owner );
	}
}
void UArrayProperty::FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
	{
		BYTE* ArrayData = (BYTE*)((FArray*)Data)->GetData();
		if( (Inner->PropertyFlags&CPF_Component) && ArrayData )
			for(INT ElementIndex = 0;ElementIndex < ((FArray*)Data)->Num();ElementIndex++)
				Inner->FixupComponentReferences( InstanceMap, ArrayData + ElementIndex * Inner->ElementSize, Owner );
	}
}
IMPLEMENT_CLASS(UArrayProperty);

/*-----------------------------------------------------------------------------
	UMapProperty.
-----------------------------------------------------------------------------*/

void UMapProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	Ar.Preload( Key );
	Key->Link( Ar, NULL );
	Ar.Preload( Value );
	Value->Link( Ar, NULL );
	ElementSize    = sizeof(TMap<BYTE,BYTE>);
	Offset         = Align( GetOuterUField()->GetPropertiesSize(), PROPERTY_ALIGNMENT );
	if( !(PropertyFlags&CPF_Native) )
		PropertyFlags |= CPF_NeedCtorLink;
}
UBOOL UMapProperty::Identical( const void* A, const void* B ) const
{
	checkSlow(Key);
	checkSlow(Value);
	/*
	INT n = ((FArray*)A)->Num();
	if( n!=(B ? ((FArray*)B)->Num() : 0) )
		return 0;
	INT   c = Inner->ElementSize;
	BYTE* p = (BYTE*)((FArray*)A)->GetData();
	if( B )
	{
		BYTE* q = (BYTE*)((FArray*)B)->GetData();
		for( INT i=0; i<n; i++ )
			if( !Inner->Identical( p+i*c, q+i*c ) )
				return 0;
	}
	else
	{
		for( INT i=0; i<n; i++ )
			if( !Inner->Identical( p+i*c, 0 ) )
				return 0;
	}
	*/
	return 1;
}
void UMapProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	checkSlow(Key);
	checkSlow(Value);
	/*
	INT   c = Inner->ElementSize;
	INT   n = ((FArray*)Value)->Num();
	Ar << n;
	if( Ar.IsLoading() )
	{
		((FArray*)Value)->Empty( c );
		((FArray*)Value)->Add( n, c );
	}
	BYTE* p = (BYTE*)((FArray*)Value)->GetData();
	for( INT i=0; i<n; i++ )
		Inner->SerializeItem( Ar, p+i*c );
	*/
}
UBOOL UMapProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	return 1;
}
void UMapProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Key << Value;
	checkSlow(Key);
	checkSlow(Value);
}
void UMapProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	checkSlow(Key);
	checkSlow(Value);
	Out.Log( TEXT("TMap<") );
	Key->ExportCppItem( Out );
	Out.Log( TEXT(",") );
	Value->ExportCppItem( Out );
	Out.Log( TEXT(">") );
}
void UMapProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	checkSlow(Key);
	checkSlow(Value);
	/*
	*ValueStr++ = '(';
	FArray* Array       = (FArray*)PropertyValue;
	FArray* Default     = (FArray*)DefaultValue;
	INT     ElementSize = Inner->ElementSize;
	for( INT i=0; i<Array->Num(); i++ )
	{
		if( i>0 )
			*ValueStr++ = ',';
		Inner->ExportTextItem( ValueStr, (BYTE*)Array->GetData() + i*ElementSize, Default ? (BYTE*)Default->GetData() + i*ElementSize : 0, PortFlags|PPF_Delimited );
		ValueStr += appStrlen(ValueStr);
	}
	*ValueStr++ = ')';
	*ValueStr++ = 0;
	*/
}
const TCHAR* UMapProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
	checkSlow(Key);
	checkSlow(Value);
	/*
	if( *Buffer++ != '(' )
		return NULL;
	FArray* Array       = (FArray*)Data;
	INT     ElementSize = Inner->ElementSize;
	Array->Empty( ElementSize );
	while( *Buffer != ')' )
	{
		INT Index = Array->Add( 1, ElementSize );
		appMemzero( (BYTE*)Array->GetData() + Index*ElementSize, ElementSize );
		Buffer = Inner->ImportText( Buffer, (BYTE*)Array->GetData() + Index*ElementSize, PortFlags|PPF_Delimited );
		if( !Buffer )
			return NULL;
		if( *Buffer!=',' )
			break;
		Buffer++;
	}
	if( *Buffer++ != ')' )
		return NULL;
	*/
	return Buffer;
}
void UMapProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	/*
	TMap<BYTE,BYTE>* SrcMap    = (TMap<BYTE,BYTE>*)Src;
	TMap<BYTE,BYTE>* DestMap   = (TMap<BYTE,BYTE>*)Dest;
	INT              KeySize   = Key->ElementSize;
	INT              ValueSize = Value->ElementSize;
	DestMap->Empty( Size, SrcArray->Num() );//must destruct it if really copying
	if( Inner->PropertyFlags & CPF_NeedsCtorLink )
	{
		// Copy all the elements.
		DestArray->AddZeroed( Size, SrcArray->Num() );
		BYTE* SrcData  = (BYTE*)SrcArray->GetData();
		BYTE* DestData = (BYTE*)DestArray->GetData();
		for( INT i=0; i<DestArray->Num(); i++ )
			Inner->CopyCompleteValue( DestData+i*Size, SrcData+i*Size );
	}
	else
	{
		// Copy all the elements.
		DestArray->Add( SrcArray->Num(), Size );
		appMemcpy( DestArray->GetData(), SrcArray->GetData(), SrcArray->Num()*Size );
	}*/
}
void UMapProperty::DestroyValue( void* Dest ) const
{
	/*
	FArray* DestArray = (FArray*)Dest;
	if( Inner->PropertyFlags & CPF_NeedsCtorLink )
	{
		BYTE* DestData = (BYTE*)DestArray->GetData();
		INT   Size     = Inner->ElementSize;
		for( INT i=0; i<DestArray->Num(); i++ )
			Inner->DestroyValue( DestData+i*Size );
	}
	DestArray->~FArray();
	*/
}
IMPLEMENT_CLASS(UMapProperty);

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

void UStructProperty::CleanupDestroyed( BYTE* Data, UObject* Owner ) const
{
	for(int i=0; i<ArrayDim; i++)
		Struct->CleanupDestroyed(Data+i*ElementSize, Owner);
}
void UStructProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	Ar.Preload( Struct );
	ElementSize		= Align( Struct->PropertiesSize, Struct->GetMinAlignment() );
	DWORD Alignment = Max( ElementSize >= PROPERTY_ALIGNMENT ? PROPERTY_ALIGNMENT : ElementSize==2 ? 2 : ElementSize>=4 ? 4 : 1, Struct->GetMinAlignment() );
	Offset			= Align( GetOuterUField()->GetPropertiesSize(), Alignment );
	
	if( Struct->ConstructorLink && !(PropertyFlags & CPF_Native) )
		PropertyFlags |= CPF_NeedCtorLink;
}
UBOOL UStructProperty::Identical( const void* A, const void* B ) const
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
		for( INT i=0; i<It->ArrayDim; i++ )
			if( !It->Matches(A,B,i) )
				return 0;
	return 1;
}
void UStructProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes ) const
{
	if( Ar.IsLoading() )
		appMemcpy( Value, &Struct->Defaults(0), Struct->Defaults.Num() );
	if( !(Ar.IsLoading() || Ar.IsSaving()) 
	||	Struct->GetFName()==NAME_Vector
	||	Struct->GetFName()==NAME_Rotator
	||	Struct->GetFName()==NAME_Color 
	)
	{
		Ar.Preload( Struct );
		Struct->SerializeBin( Ar, (BYTE*)Value, MaxReadBytes );
	}
	else
	{
		Struct->SerializeTaggedProperties( Ar, (BYTE*)Value, NULL );
	}
}
UBOOL UStructProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	if( Struct->GetFName()==NAME_Vector )
	{
		FVector& V = *(FVector*)Data;
		INT X(appRound(V.X)), Y(appRound(V.Y)), Z(appRound(V.Z));
		DWORD Bits = Clamp<DWORD>( appCeilLogTwo(1+Max(Max(Abs(X),Abs(Y)),Abs(Z))), 1, 20 )-1;
		Ar.SerializeInt( Bits, 20 );
		INT   Bias = 1<<(Bits+1);
		DWORD Max  = 1<<(Bits+2);
		DWORD DX(X+Bias), DY(Y+Bias), DZ(Z+Bias);
		Ar.SerializeInt( DX, Max );
		Ar.SerializeInt( DY, Max );
		Ar.SerializeInt( DZ, Max );
		if( Ar.IsLoading() )
			V = FVector((INT)DX-Bias,(INT)DY-Bias,(INT)DZ-Bias);
	}
	else if( Struct->GetFName()==NAME_Rotator )
	{
		FRotator& R = *(FRotator*)Data;
		BYTE Pitch(R.Pitch>>8), Yaw(R.Yaw>>8), Roll(R.Roll>>8), B;
		B = (Pitch!=0);
		Ar.SerializeBits( &B, 1 );
		if( B )
			Ar << Pitch;
		else
			Pitch = 0;
		B = (Yaw!=0);
		Ar.SerializeBits( &B, 1 );
		if( B )
			Ar << Yaw;
		else
			Yaw = 0;
		B = (Roll!=0);
		Ar.SerializeBits( &B, 1 );
		if( B )
			Ar << Roll;
		else
			Roll = 0;
		if( Ar.IsLoading() )
			R = FRotator(Pitch<<8,Yaw<<8,Roll<<8);
	}
	else if( Struct->GetFName()==NAME_Plane )
	{
		FPlane& P = *(FPlane*)Data;
		SWORD X(appRound(P.X)), Y(appRound(P.Y)), Z(appRound(P.Z)), W(appRound(P.W));
		Ar << X << Y << Z << W;
		if( Ar.IsLoading() )
			P = FPlane(X,Y,Z,W);
	}
	else if ( Struct->GetFName()==NAME_CompressedPosition )
	{
		FCompressedPosition& C = *(FCompressedPosition*)Data;

		INT VX = appRound(C.Velocity.X);
		INT VY = appRound(C.Velocity.Y);
		INT VZ = appRound(C.Velocity.Z);
		DWORD VelBits = Clamp<DWORD>( appCeilLogTwo(1+Max(Max(Abs(VX),Abs(VY)),Abs(VZ))), 1, 20 )-1;

		// Location
		INT X(appRound(C.Location.X)), Y(appRound(C.Location.Y)), Z(appRound(C.Location.Z));
		DWORD Bits = Clamp<DWORD>( appCeilLogTwo(1+Max(Max(Abs(X),Abs(Y)),Abs(Z))), 1, 20 )-1;
		Ar.SerializeInt( Bits, 20 );
		INT   Bias = 1<<(Bits+1);
		DWORD Max  = 1<<(Bits+2);
		DWORD DX(X+Bias), DY(Y+Bias), DZ(Z+Bias);
		Ar.SerializeInt( DX, Max );
		Ar.SerializeInt( DY, Max );
		Ar.SerializeInt( DZ, Max );
		if( Ar.IsLoading() )
			C.Location = FVector((INT)DX-Bias,(INT)DY-Bias,(INT)DZ-Bias);

		// Rotation
		BYTE Pitch(C.Rotation.Pitch>>8), Yaw(C.Rotation.Yaw>>8);
		Ar << Pitch;
		Ar << Yaw;
		if( Ar.IsLoading() )
			C.Rotation = FRotator(Pitch<<8,Yaw<<8,0);
		
		// Velocity
		Ar.SerializeInt( VelBits, 20 );
		Bias = 1<<(VelBits+1);
		Max  = 1<<(VelBits+2);
		DX = VX+Bias;
		DY = VY+Bias;
		DZ = VZ+Bias;
		Ar.SerializeInt( DX, Max );
		Ar.SerializeInt( DY, Max );
		Ar.SerializeInt( DZ, Max );
		if( Ar.IsLoading() )
			C.Velocity = FVector((INT)DX-Bias,(INT)DY-Bias,(INT)DZ-Bias);
	}
	else
	{
		for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
			if( Map->ObjectToIndex(*It)!=INDEX_NONE )
				for( INT i=0; i<It->ArrayDim; i++ )
					It->NetSerializeItem( Ar, Map, (BYTE*)Data+It->Offset+i*It->ElementSize );
	}
	return 1;
}
void UStructProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Struct;
}
void UStructProperty::ExportCppItem( FOutputDevice& Out, UBOOL IsParam ) const
{
	Out.Logf( TEXT("F%s"), Struct->GetName() );
}
void UStructProperty::ExportTextItem( FString& ValueStr, BYTE* PropertyValue, BYTE* DefaultValue, UObject* Parent, INT PortFlags ) const
{
	INT Count=0;
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
	{
		if( It->Port() )
		{
			for( INT Index=0; Index<It->ArrayDim; Index++ )
			{
				FString	InnerValue;
				if( It->ExportText(Index,InnerValue,PropertyValue,DefaultValue,Parent,PPF_Delimited) )
				{
					Count++;
					if( Count == 1 )
						ValueStr += TEXT("(");
					else
						ValueStr += TEXT(",");
					if( It->ArrayDim == 1 )
						ValueStr += FString::Printf( TEXT("%s="), It->GetName() );
					else
						ValueStr += FString::Printf( TEXT("%s[%i]="), It->GetName(), Index );
					ValueStr += InnerValue;
				}
			}
		}
	}
	if( Count > 0 )
		ValueStr += TEXT(")");
}
const TCHAR* UStructProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent ) const
{
    const TCHAR* Buffer = InBuffer;
	if( *Buffer++ == '(' )
	{
		// Parse all properties.
		while( *Buffer != ')' )
		{
			// Get key name.
			TCHAR Name[NAME_SIZE];
			int Count=0;
			while( Count<NAME_SIZE-1 && *Buffer && *Buffer!='=' && *Buffer!='[' )
				Name[Count++] = *Buffer++;
			Name[Count++] = 0;

			// Get optional array element.
			INT Element=0;
			if( *Buffer=='[' )
			{
				const TCHAR* Start=++Buffer;
				while( *Buffer>='0' && *Buffer<='9' )
					Buffer++;
				if( *Buffer++ != ']' )
				{
					warnf( NAME_Error, TEXT("ImportText: Illegal array element in: %s"), InBuffer );
					return NULL;
				}
				Element = appAtoi( Start );
			}

			// Verify format.
			if( *Buffer++ != '=' )
			{
				warnf( NAME_Error, TEXT("ImportText: Missing key name in: %s"), InBuffer );
				return NULL;
			}

            if( Element < 0 )
            {
				warnf( NAME_Error, TEXT("ImportText: Negative array index in: %s"), InBuffer );
				return NULL;
            }

			// See if the property exists in the struct.
			FName GotName( Name, FNAME_Find );
			UBOOL Parsed = 0;
			if( GotName!=NAME_None )
			{
				for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
				{
					UProperty* Property = *It;

					if(	Property->GetFName()!=GotName || 
						Property->GetSize()==0 ||
						!Property->Port() )
                        continue;

					if( Element >= Property->ArrayDim )
					{
				        warnf( NAME_Error, TEXT("ImportText: Array index out of bounds in: %s"), InBuffer );
				        return NULL;
                    }
					Buffer = Property->ImportText( Buffer, Data + Property->Offset + Element*Property->ElementSize, PortFlags|PPF_Delimited, Parent );
					if( Buffer == NULL )
                    {
				        warnf( NAME_Warning, TEXT("ImportText failed in: %s"), InBuffer );
						return NULL;
                    }

					Parsed = 1;
                    break;
				}
			}

			// If not parsed, skip this property in the stream.
			if( !Parsed )
			{
			    warnf( NAME_Error, TEXT("Unknown member %s in %s"), Name, GetName() );

				INT SubCount=0;
				while
				(	*Buffer
				&&	*Buffer!=10
				&&	*Buffer!=13 
				&&	(SubCount>0 || *Buffer!=')')
				&&	(SubCount>0 || *Buffer!=',') )
				{
					if( *Buffer == 0x22 )
					{
						while( *Buffer && *Buffer!=0x22 && *Buffer!=10 && *Buffer!=13 )
							Buffer++;
						if( *Buffer != 0x22 )
						{
							warnf( NAME_Error, TEXT("ImportText: Bad quoted string in: %s"), InBuffer );
							return NULL;
						}
					}
					else if( *Buffer == '(' )
					{
						SubCount++;
					}
					else if( *Buffer == ')' )
					{
						SubCount--;
						if( SubCount < 0 )
						{
							warnf( NAME_Error, TEXT("ImportText: Bad parenthesised struct in: "), InBuffer );
							return NULL;
						}
					}
					Buffer++;
				}
				if( SubCount > 0 )
				{
					warnf( NAME_Error, TEXT("ImportText: Incomplete parenthesised struct in: "), InBuffer );
					return NULL;
				}
			}

			// Skip comma.
			if( *Buffer==',' )
			{
				// Skip comma.
				Buffer++;
			}
			else if( *Buffer!=')' )
			{
				warnf( NAME_Error, TEXT("ImportText: Bad termination: %s in: %s"), Buffer, InBuffer );
				return NULL;
			}
		}

		// Skip trailing ')'.
		Buffer++;
	}
	else
	{
		debugf( NAME_Warning, TEXT("ImportText: Struct missing '(' in: %s"), InBuffer );
		return NULL;
	}
	return Buffer;
}
void UStructProperty::CopySingleValue( void* Dest, void* Src, UObject* SuperObject ) const
{
	for( TFieldIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
		It->CopyCompleteValue( (BYTE*)Dest + It->Offset, (BYTE*)Src + It->Offset, SuperObject );
	//could just do memcpy + ReinstanceCompleteValue
}
void UStructProperty::DestroyValue( void* Dest ) const
{
	for( UProperty* P=Struct->ConstructorLink; P; P=P->ConstructorLinkNext )
	{
		if( ArrayDim <= 0 )
		{
			P->DestroyValue( (BYTE*) Dest + P->Offset );
		}
		else
		{
			for( INT i=0; i<ArrayDim; i++ )
				P->DestroyValue( (BYTE*)Dest + i*ElementSize + P->Offset );
		}
	}
}
void UStructProperty::FindInstancedComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
		Struct->FindInstancedComponents( InstanceMap, Data+Offset, Owner );
}
void UStructProperty::FixLegacyComponents( BYTE* Data, BYTE* Defaults, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
		Struct->FixLegacyComponents( Data + Offset, Defaults + Offset, Owner );
}
void UStructProperty::InstanceComponents( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
		Struct->InstanceComponents( InstanceMap, Data+Offset, Owner );
}
void UStructProperty::FixupComponentReferences( TMap<FName,UComponent*>& InstanceMap, BYTE* Data, UObject* Owner )
{
	if(!(PropertyFlags & CPF_Native))
		Struct->FixupComponentReferences( InstanceMap, Data+Offset, Owner );
}
IMPLEMENT_CLASS(UStructProperty);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

