/*=============================================================================
	UnProp.cpp: UProperty implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "UnScriptPatcher.h"

//@todo: fix hardcoded lengths

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

/**
 * Advances the character pointer past any spaces or tabs.
 * 
 * @param	Str		the buffer to remove whitespace from
 */
static void SkipWhitespace(const TCHAR*& Str)
{
	while(Str && appIsWhitespace(*Str))
	{
		Str++;
	}
}

//
// Parse a token.
//
const TCHAR* ReadToken( const TCHAR* Buffer, FString& String, UBOOL DottedNames=0 )
{
	if( *Buffer == TCHAR('"') )
	{
		// Get quoted string.
		Buffer++;
		while( *Buffer && *Buffer!=TCHAR('"') && *Buffer!=TCHAR('\n') && *Buffer!=TCHAR('\r') )
		{
			if( *Buffer != TCHAR('\\') ) // unescaped character
			{
				String += *Buffer++;
			}
			else if( *++Buffer==TCHAR('\\') ) // escaped backslash "\\"
			{
				String += TEXT("\\");
				Buffer++;
			}
			else if ( *Buffer == TCHAR('\"') ) // escaped double quote "\""
			{
				String += TCHAR('"');
				Buffer++;
			}
			else if ( *Buffer == TCHAR('n') ) // escaped newline
			{
				String += TCHAR('\n');
				Buffer++;
			}
			else if ( *Buffer == TCHAR('r') ) // escaped carriage return
			{
				String += TCHAR('\r');
				Buffer++;
			}
			else // some other escape sequence, assume it's a hex character value
			{
				String = FString::Printf(TEXT("%s%c"), *String, ParseHexDigit(Buffer[0])*16 + ParseHexDigit(Buffer[1]));
				Buffer += 2;
			}
		}
		if( *Buffer++ != TCHAR('"') )
		{
			warnf( NAME_Warning, TEXT("ReadToken: Bad quoted string") );
			return NULL;
		}
	}
	else if( appIsAlnum( *Buffer ) )
	{
		// Get identifier.
		while( (appIsAlnum(*Buffer) || *Buffer==TCHAR('_') || *Buffer==TCHAR('-') || (DottedNames && (*Buffer==TCHAR('.') || *Buffer==SUBOBJECT_DELIMITER_CHAR) )) )
		{
			String += *Buffer++;
		}
	}
	else
	{
		// Get just one.
		String += *Buffer;
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
:	ArrayDim( 1 )
#if !CONSOLE
,	ArraySizeEnum( NULL )
#endif
,	NextRef( NULL )
{}
UProperty::UProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
:	ArrayDim( 1 )
,	PropertyFlags( InFlags )
#if !CONSOLE
,	Category( InCategory )
,	ArraySizeEnum( NULL )
#endif
,	Offset( InOffset )
,	NextRef( NULL )
{
	// properties created in C++ should always be marked RF_Transient so that when the package containing
	// this property is saved, it doesn't try to save this UProperty into the ExportMap
	SetFlags(RF_Transient|RF_Native);
#if !WITH_EDITORONLY_DATA
	checkSlow(!HasAnyPropertyFlags(CPF_EditorOnly));
#endif // WITH_EDITORONLY_DATA
	checkSlow(GetOuterUField()->HasAllFlags(RF_Native|RF_Transient));

	GetOuterUField()->AddCppProperty(this);
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	
#if !CONSOLE
	TheClass->EmitObjectReference( STRUCT_OFFSET( UProperty, ArraySizeEnum ) );
#endif

	// Note: None of the acceleration links need to be emitted, as they are a subset of other links
}

//
// Serializer.
//
void UProperty::Serialize( FArchive& Ar )
{
	// Make sure that we aren't saving a property to a package that shouldn't be serialised.
#if WITH_EDITORONLY_DATA
	check( !Ar.IsFilterEditorOnly() || !IsEditorOnlyProperty() );
#endif // WITH_EDITORONLY_DATA

	Super::Serialize( Ar );

	// Archive the basic info.
	Ar << ArrayDim << PropertyFlags;
#if !CONSOLE
	UBOOL const bIsCookedForConsole = IsPackageCookedForConsole(Ar);
	if ( !bIsCookedForConsole && (!Ar.IsSaving() || !GIsCooking || !(GCookingTarget & UE3::PLATFORM_Console)) )
	{
		Ar << Category << ArraySizeEnum;
	}
#endif
	
#if CONSOLE
	// Make sure that we aren't saving a property to a package that shouldn't be serialised.
	check( !IsEditorOnlyProperty() );
#endif

	if( PropertyFlags & CPF_Net )
	{
		Ar << RepOffset;
	}

	if( Ar.IsLoading() )
	{
		Offset = 0;
		ConstructorLinkNext = NULL;
	}
}

/**
 * Verify that modifying this property's value via ImportText is allowed.
 * 
 * @param	PortFlags	the flags specified in the call to ImportText
 *
 * @return	TRUE if ImportText should be allowed
 */
UBOOL UProperty::ValidateImportFlags( DWORD PortFlags, FOutputDevice* ErrorHandler ) const
{
	UBOOL bResult = TRUE;

	// PPF_RestrictImportTypes is set when importing defaultproperties; it indicates that
	// we should not allow config/localized properties to be imported here
	if ( (PortFlags&PPF_RestrictImportTypes) != 0 &&
		(PropertyFlags&(CPF_Localized|CPF_Config)) != 0 )
	{
		FString PropertyType = (PropertyFlags&CPF_Config) != 0
			? (PropertyFlags&CPF_Localized) != 0
				? TEXT("config/localized")
				: TEXT("config")
			: TEXT("localized");


		FString ErrorMsg = FString::Printf(TEXT("Import failed for '%s': property is %s (Check to see if the property is listed in the DefaultProperties.  It should only be listed in the specific .ini/.int file)"), *GetName(), *PropertyType);

		if( ErrorHandler != NULL )
		{
			ErrorHandler->Logf( *ErrorMsg );
		}
		else
		{
			GWarn->Logf( NAME_Warning, *ErrorMsg );
		}

		bResult = FALSE;
	}

/*
	if ( (PortFlags&PPF_SkipObjectProperties) != 0
	&&	ConstCast<UObjectProperty>(this,CLASS_IsAUObjectProperty) != NULL )
	{
		bResult = FALSE;
	}
*/

	return bResult;
}

/**
* Returns the C++ name of the property, including the _DEPRECATED suffix if the 
* property is deprecated.
*
* @return C++ name of property
*/
FString UProperty::GetNameCPP() const
{
	return HasAnyPropertyFlags(CPF_Deprecated) ? GetName() + TEXT("_DEPRECATED") : GetName();
}

/**
 * Export this class property to an output device as a C++ header file.
 * 
 * @param	Out					archive to use for export the cpp text
 * @param	IsMember			whether this property is a member property of a struct/class
 * @param	IsParm				whether this property is part of a function parameter list
 * @param	bImportsDefaults	whether this property will import default values (will be exported as NoInit)
 */
void UProperty::ExportCppDeclaration( FOutputDevice& Out, UBOOL IsMember, UBOOL IsParm, UBOOL bImportsDefaults ) const
{
	TCHAR ArrayStr[MAX_SPRINTF]=TEXT("");

	// export the property type text (e.g. FString; INT; TArray, etc.)
	FString TypeText, ExtendedTypeText;
	TypeText = GetCPPType(&ExtendedTypeText);

	const UBOOL bIsInterfaceProp = IsA(UInterfaceProperty::StaticClass());

	// export 'const' for parameters
	if ( IsParm
	&&	(HasAnyPropertyFlags(CPF_Const) || (bIsInterfaceProp && !HasAllPropertyFlags(CPF_OutParm))) )
	{
		TypeText = FString::Printf(TEXT("const %s"), *TypeText);
	}

	if( ArrayDim != 1 )
	{
		appSprintf( ArrayStr, TEXT("[%i]"), ArrayDim );
	}

	if( IsA(UBoolProperty::StaticClass()) )
	{
		// if this is a member variable, export it as a bitfield
		if( ArrayDim==1 && IsMember )
		{
			// export as a BITFIELD member....bad to hardcode, but this is a special case that won't be used anywhere else
			Out.Logf( TEXT("BITFIELD%s %s%s:1"), *ExtendedTypeText, *GetNameCPP(), ArrayStr );
		}

		//@todo we currently can't have out bools.. so this isn't really necessary, but eventually out bools may be supported, so leave here for now
		else if( IsParm && HasAnyPropertyFlags(CPF_OutParm) )
		{
			// export as a reference
			Out.Logf( TEXT("%s%s%s %s%s"), *TypeText, *ExtendedTypeText, HasAnyPropertyFlags(CPF_OptionalParm) ? TEXT("*") : TEXT("&"), *GetNameCPP(), ArrayStr );
		}

		else
		{
			Out.Logf( TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *GetNameCPP(), ArrayStr );
		}
	}
	else if ( RequiresInit() )
	{
		if ( IsParm )
		{
			if ( ArrayDim > 1 )
			{
				// export as a pointer
				//@fixme ronp
				Out.Logf( TEXT("%s%s* %s"), *TypeText, *ExtendedTypeText, *GetNameCPP() );
			}
			else
			{
				// export as a reference (const ref if it isn't an out parameter)
				Out.Logf(TEXT("%s%s%s%s %s"),
					!HasAnyPropertyFlags(CPF_OutParm|CPF_Const) ? TEXT("const ") : TEXT(""),
					*TypeText, *ExtendedTypeText,
					HasAllPropertyFlags(CPF_OptionalParm|CPF_OutParm) ? TEXT("*") : TEXT("&"),
					*GetNameCPP());
			}
		}
		else if( bImportsDefaults && !HasAnyPropertyFlags(CPF_AlwaysInit) )
		{
			Out.Logf(TEXT("%sNoInit%s %s%s"), *TypeText, *ExtendedTypeText, *GetNameCPP(), ArrayStr);
		}
		else
		{
			Out.Logf( TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *GetNameCPP(), ArrayStr );
		}
	}
	else if ( IsParm )
	{
		if ( ArrayDim > 1 )
		{
			// export as a pointer
			//@fixme ronp
			Out.Logf( TEXT("%s%s* %s"), *TypeText, *ExtendedTypeText, *GetNameCPP() );
		}
		else
		{
			// export as a pointer if this is an optional out parm, reference if it's just an out parm, standard otherwise...
			TCHAR ModifierString[2]={0,0};
			if ( HasAllPropertyFlags(CPF_OptionalParm|CPF_OutParm) )
			{
				ModifierString[0] = TEXT('*');
			}
			else if ( HasAnyPropertyFlags(CPF_OutParm) || bIsInterfaceProp )
			{
				ModifierString[0] = TEXT('&');
			}
			Out.Logf( TEXT("%s%s%s %s%s"), *TypeText, *ExtendedTypeText, ModifierString, *GetNameCPP(), ArrayStr );
		}
	}
	else
	{
		Out.Logf( TEXT("%s%s %s%s"), *TypeText, *ExtendedTypeText, *GetNameCPP(), ArrayStr );
	}
}

//
// Export the contents of a property.
//
UBOOL UProperty::ExportText
(
	INT			Index,
	FString&	ValueStr,
	const BYTE*	Data,
	const BYTE*	Delta,
	UObject*	Parent,
	INT			PortFlags,
	EImportTextFormat TextFormat
) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	if( Data==Delta || !Matches(Data,Delta,Index,FALSE,PortFlags) )
	{
		ExportTextItem
		(
			ValueStr,
			Data + Offset + Index * ElementSize,
			Delta ? (Delta + Offset + Index * ElementSize) : NULL,
			Parent,
			PortFlags,
			TextFormat
		);
		return TRUE;
	}
	
	return FALSE;
}

/**
 * Copy the value for a single element of this property.
 * 
 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
 *									OFFSET = the Offset of this UProperty
 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
 *									SIZE = the ElementSize of this UProperty
 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
 * @param	SubobjectRoot		the first object in DestOwnerObject's Outer chain that is not a subobject.  SubobjectRoot will be the same as DestOwnerObject if DestOwnerObject is not a subobject.
 * @param	DestOwnerObject		the object that contains the destination data.  Only specified when creating the member properties for an object; DestOwnerObject is the object that will contain any instanced subobjects.
 */
void UProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	appMemcpy( Dest, Src, ElementSize );
}

/**
 * Copy the value for all elements of this property.
 * 
 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET * SIZE, where
 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
 *									OFFSET = the Offset of this UProperty
 *									SIZE = the ElementSize of this UProperty
 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
 * @param	SubobjectRoot		the first object in DestOwnerObject's Outer chain that is not a subobject.  SubobjectRoot will be the same as DestOwnerObject if DestOwnerObject is not a subobject (which normally indicates that we are simply duplicating an object)
 * @param	DestOwnerObject		the object that contains the destination data.  Only specified when creating the member properties for an object; DestOwnerObject is the object that will contain any instanced subobjects.
 */
void UProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	for( INT i=0; i<ArrayDim; i++ )
		CopySingleValue( (BYTE*)Dest+i*ElementSize, (BYTE*)Src+i*ElementSize, SubobjectRoot, DestOwnerObject, InstanceGraph );
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
	SerializeItem( Ar, Data, 0, NULL );
	return 1;
}

//
// Return whether the property should be exported to localization files.
//
UBOOL UProperty::IsLocalized() const
{
	return (PropertyFlags & CPF_Localized) != 0;
}

//
// Return whether the property should be exported.
//
UBOOL UProperty::Port( DWORD PortFlags/*=0*/ ) const
{
	// if no size, don't export
	if ( GetSize() <= 0 )
	{
		return FALSE;
	}

#if !CONSOLE
	// if we are a transient or native property and we don't have a category, don't export
	if ( Category == NAME_None )
	{
		// Normally native properties aren't exported (unless they're editable in editor), but sometimes
		// we need copy/paste to work on native properties so they'll be marked with SerializeText
		if ( HasAnyPropertyFlags(CPF_Native) && !HasAnyPropertyFlags(CPF_SerializeText) )
		{
			return FALSE;
		}

		// if we're parsing default properties or the user indicated that transient properties should be included
		if ( HasAnyPropertyFlags(CPF_Transient)
		&&	(PortFlags & (PPF_ParsingDefaultProperties|PPF_IncludeTransient)) == 0 )
		{
			return FALSE;
		}
	}
#endif

	// if this is the Object.Class property, don't export
	if ( GetFName() == NAME_Class && GetOwnerClass() == UObject::StaticClass() )
	{
		return FALSE;
	}

	// if we're only supposed to export components and this isn't a component property, don't export
	if ( (PortFlags&PPF_ComponentsOnly) != 0 && (PropertyFlags&CPF_Component) == 0 )
	{
		return FALSE;
	}

	// if we're not supposed to export object properties and this is an object property, don't export
	if ( (PortFlags&PPF_SkipObjectProperties) != 0
	&&	ConstCast<UObjectProperty>(this,CLASS_IsAUObjectProperty) != NULL )
	{
		return FALSE;
	}

	// hide EditHide properties when we're exporting for the property window
	if ( (PortFlags&PPF_PropertyWindow) != 0 && (PropertyFlags&CPF_EditHide) != 0 )
	{
		return FALSE;
	}

	return TRUE;
}

//
// Return type id for encoding properties in .u files.
//
FName UProperty::GetID() const
{
	return GetClass()->GetFName();
}

/**
 * Gets the user-friendly, localized (if exists) name of this property
 *
 * @param	OwnerClass	if specified, uses this class's loc file instead of the property's owner class
 *						useful for overriding the friendly name given a property inherited from a parent class.
 *
 * @return	the friendly name for this property.  localized first, then metadata, then the property's name.
 */
FString UProperty::GetFriendlyName( UClass* OwnerClass/*=NULL*/ ) const
{
	// first, try to pull the frienly name from the loc file
	UClass* RealOwnerClass = GetOwnerClass();
	if ( OwnerClass == NULL)
	{
		OwnerClass = RealOwnerClass;
	}
	checkSlow(OwnerClass);

	FString Result;
	UClass* CurrentClass = OwnerClass;
	do 
	{
		FString PropertyPathName = GetPathName(CurrentClass);
		Result = Localize(*CurrentClass->GetName(), *(PropertyPathName + TEXT(".FriendlyName")), *CurrentClass->GetOuter()->GetName(), NULL, TRUE);
		CurrentClass = CurrentClass->GetSuperClass();
	} while( CurrentClass != NULL && CurrentClass->IsChildOf(RealOwnerClass) && Result.Len() == 0 );

	if ( Result.Len() == 0 )
	{
		Result = GetMetaData(TEXT("FriendlyName"));
		if ( Result.Len() == 0 )
		{
			Result = GetName();
		}
	}
	return Result;
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

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UByteProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UByteProperty, Enum ) );
}

INT UByteProperty::GetMinAlignment() const
{
	return sizeof(BYTE);
}
void UByteProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(BYTE);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
}
void UByteProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(BYTE*)Dest = *(BYTE*)Src;
}
void UByteProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if( ArrayDim==1 )
		*(BYTE*)Dest = *(BYTE*)Src;
	else
		appMemcpy( Dest, Src, ArrayDim );
}
UBOOL UByteProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return *(BYTE*)A == (B ? *(BYTE*)B : 0);
}
void UByteProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	// Serialize enum values by name unless we're not saving or loading OR for backwards compatibility
	const UBOOL bUseBinarySerialization = (Enum == NULL) || (Ar.GetPortFlags() & PPF_ForceBinarySerialization) || (!Ar.IsLoading() && !Ar.IsSaving());
	if( bUseBinarySerialization )
	{
		Ar << *(BYTE*)Value;
	}
	// Loading
	else if (Ar.IsLoading())
	{
		FName EnumValueName;
		Ar << EnumValueName;
		// Make sure enum is properly populated
		if( Enum->HasAnyFlags(RF_NeedLoad) )
		{
			Ar.Preload(Enum);
		}

		//@compatibility: temp replacement for Exodus, remove later
#if !CONSOLE
		static FName NAME_EExoRealm(TEXT("EExoRealm"));
		if (Enum->GetFName() == NAME_EExoRealm)
		{
			if (EnumValueName == FName(TEXT("Ice")))
			{
				EnumValueName = FName(TEXT("REALM_Ice"));
			}
			else if (EnumValueName == FName(TEXT("Fire")))
			{
				EnumValueName = FName(TEXT("REALM_FIRE"));
			}
		}
#endif

		// There's no guarantee EnumValueName is still present in Enum, in which case Value will be set to the enum's max value.
		// On save, it will then be serialized as NAME_None.
		*(BYTE*)Value = Enum->FindEnumIndex(EnumValueName);
		if ( Enum->NumEnums() < *(BYTE*)Value )
		{
			*(BYTE*)Value = Enum->NumEnums() - 1;
		}
	}
	// Saving
	else
	{
		FName EnumValueName;
		BYTE ByteValue = *(BYTE*)Value;

		// subtract 1 because the last entry in the enum's Names array
		// is the _MAX entry
		if ( ByteValue < Enum->NumEnums() - 1 )
		{
			EnumValueName = Enum->GetEnum(ByteValue);
		}
		else
		{
			EnumValueName = NAME_None;
		}
		Ar << EnumValueName;
	}
}
UBOOL UByteProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	// -1 because the last item in the enum is the autogenerated _MAX item
	Ar.SerializeBits( Data, Enum ? appCeilLogTwo(Enum->NumEnums() - 1) : 8 );
	return 1;
}
void UByteProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Enum;
	if (Enum != NULL)
	{
		Ar.Preload(Enum);
	}
}
FString UByteProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("BYTE");
}
FString UByteProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("BYTE");
}
void UByteProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	if( Enum )
	{
		// if the value is the max value (the autogenerated *_MAX value), export as "INVALID", unless we're exporting text for copy/paste (for copy/paste,
		// the property text value must actually match an entry in the enum's names array)
		ValueStr += ((*PropertyValue < Enum->NumEnums() - 1 || ((PortFlags&PPF_Copy) != 0 && *PropertyValue < Enum->NumEnums())) 
			? Enum->GetEnum(*PropertyValue).ToString() 
			: TEXT("(INVALID)"));
	}
	else
	{
		ValueStr += appItoa(*PropertyValue);
	}
}
const TCHAR* UByteProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if (Enum != NULL && Enum->HasAnyFlags(RF_NeedLoad))
	{
			debugf(TEXT("ENUM %s NOT LOADED when loading property %s"), *Enum->GetPathName(), *GetFullName());
	}
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	FString Temp;
	if( Enum )
	{
		const TCHAR* Buffer = ReadToken( InBuffer, Temp );
		if( Buffer != NULL )
		{
			const FName EnumName = FName( *Temp, FNAME_Find );
			if( EnumName != NAME_None )
			{
				const INT EnumIndex = Enum->FindEnumIndex( EnumName );
				if( EnumIndex != INDEX_NONE )
				{
					*(BYTE*)Data = EnumIndex;
					return Buffer;
				}
			}
		}
	}
	if( appIsDigit(*InBuffer) )
	{
		*(BYTE*)Data = appAtoi( InBuffer );
		while( *InBuffer>='0' && *InBuffer<='9' )
		{
			InBuffer++;
		}
	}
	else
	{
		//debugf( "Import: Missing byte" );
		return NULL;
	}
	return InBuffer;
}
UBOOL UByteProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}

	return *(BYTE*)Data != 0;
}

void UByteProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	check(Data);

	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(BYTE*)Data = 0;
}

UBOOL UByteProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.ByteValue = *(BYTE*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UByteProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(BYTE*)PropertyValueAddress = PropertyValue.ByteValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UByteProperty);

/*-----------------------------------------------------------------------------
	UIntProperty.
-----------------------------------------------------------------------------*/

INT UIntProperty::GetMinAlignment() const
{
	return sizeof(INT);
}
void UIntProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(INT);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
}
void UIntProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(INT*)Dest = *(INT*)Src;
}
void UIntProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if( ArrayDim==1 )
	{
		*(INT*)Dest = *(INT*)Src;
	}
	else
	{
		appMemcpy(Dest,Src,ArrayDim*ElementSize);
	}
}
UBOOL UIntProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return *(INT*)A == (B ? *(INT*)B : 0);
}
void UIntProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	Ar << *(INT*)Value;
}
UBOOL UIntProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	Ar << *(INT*)Data;
	return 1;
}
FString UIntProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("INT");
}
FString UIntProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("INT");
}
void UIntProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	INT iVal = *( INT * )PropertyValue;
	if( PortFlags & PPF_ExportAsFriendlyRotation )
	{
		FLOAT Val = ( ( FLOAT )iVal ) * ( 360.0f / 65536.0f );

		FString Wk;
		if( Abs(Val) > 359.f )
		{
			const INT Revolutions = Val / 360.f;
			Val -= Revolutions * 360;
			Wk = FString::Printf( TEXT("%.2f%c %s %d"), Val, 176, (Revolutions < 0)?TEXT("-"):TEXT("+"), abs(Revolutions) );
		}
		else
		{
			// note : The degree symbol is ASCII code 248 (char code 176)
			Wk = FString::Printf( TEXT("%.2f%c"), Val, 176 );
		}
		ValueStr += Wk;
	}
	else
	{
		ValueStr += FString::Printf( TEXT("%i"), iVal );
	}
}
const TCHAR* UIntProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
	{
		return NULL;
	}

	if ( Buffer != NULL )
	{
		const TCHAR* Start = Buffer;
		if ( !appStrnicmp(Start,TEXT("0x"),2) )
		{
			Buffer+=2;
			while ( Buffer && (ParseHexDigit(*Buffer) != 0 || *Buffer == TCHAR('0')) )
			{
				Buffer++;
			}
		}
		else
		{
			while ( Buffer && (*Buffer == TCHAR('-') || *Buffer == TCHAR('+')) )
			{
				Buffer++;
			}

			while ( Buffer &&  appIsDigit(*Buffer) )
			{
				Buffer++;
			}
		}

		// If the integer being imported is non-numeric, try to match a const
		// or enum value
		if (GIsUCC && Start == Buffer)
		{
			TCHAR ValName[NAME_SIZE + 1];
			INT Count;
			// Figure out where the name param ends and copy it while searching
			for (Count = 0;
				*Buffer && *Buffer != TCHAR(')') && *Buffer != TCHAR(',') && Count < NAME_SIZE;
				Buffer++)
			{
				ValName[Count++] = *Buffer;
			}
			ValName[Count] = TCHAR('\0');

			// Used to detect finding FName but not a const/ enum of said name.
			UBOOL bFoundValue = FALSE;
			// Create the FName once
			FName ValFName(ValName,FNAME_Find);
			// No need to search for the constant if it wasn't in the name table
			if (ValFName != NAME_None)
			{
				// Iterate all UEnum/UConst objects for matching tags
				for (FObjectIterator It; It; ++It)
				{
					// See if the object is an enum
					UEnum* Enum = ExactCast<UEnum>(*It);
					if (Enum != NULL)
					{
						// Try to match the text to the enum name
						INT EnumVal = Enum->FindEnumIndex(ValFName);
						if (EnumVal != INDEX_NONE)
						{
							// Assign the index as the value
							*(INT*)Data = EnumVal;
							return Buffer;
						}
					}
					else
					{
						// Now check for a const
						UConst* Const = ExactCast<UConst>(*It);
						if (Const != NULL)
						{
							// Try to match the text to the const name
							if (appStrcmp(ValName,*Const->GetName()) == 0)
							{
								// Point start to the value of this constant
								Start = *Const->Value;
								bFoundValue = TRUE;
								break;
							}
						}
					}
				}
			}

			if( !bFoundValue )
			{
				// Reset so this name will cause an error
				Buffer = Start;
			}
		}

		if (Start == Buffer)
		{
			// import failure
			return NULL;
		}
		else
		{
			*(INT*)Data = appStrtoi(Start, NULL, 0);
		}
	}
	return Buffer;
}
UBOOL UIntProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(INT*)Data != 0;
}

void UIntProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(INT*)Data = 0;
}

UBOOL UIntProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.IntValue = *(INT*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UIntProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(INT*)PropertyValueAddress = PropertyValue.IntValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UIntProperty);

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UDelegateProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UDelegateProperty, Function ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UDelegateProperty, SourceDelegate ) );
}
INT UDelegateProperty::GetMinAlignment() const
{
#if _WIN64
	return __alignof(UObject);
#else
	return sizeof(UObject*);
#endif
}
void UDelegateProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(FScriptDelegate);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	PropertyFlags |= CPF_NeedCtorLink;
}
void UDelegateProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if ( DestOwnerObject )
	{
		UObject* SourceObject = ((FScriptDelegate*)Src)->Object;
		UObject* TargetObject = SourceObject;
		if ( SourceObject != NULL )
		{
			if ( SourceObject->HasAnyFlags(RF_ClassDefaultObject) )
			{
				if ( DestOwnerObject->IsA(SourceObject->GetClass()) )
				{
					TargetObject = DestOwnerObject;
				}
				else if ( SubobjectRoot != DestOwnerObject && SubobjectRoot->IsA(SourceObject->GetClass()) )
				{
					TargetObject = SubobjectRoot;
				}
				else if ( InstanceGraph != NULL )
				{
					UObject* InstancedTargetObject = InstanceGraph->GetDestinationObject(SourceObject);
					if (InstancedTargetObject != NULL)
					{
						TargetObject = InstancedTargetObject;
					}
				}
			}
		}

		((FScriptDelegate*)Dest)->FunctionName = ((FScriptDelegate*)Src)->FunctionName;
		((FScriptDelegate*)Dest)->Object = TargetObject;
	}
	else
	{
		*(FScriptDelegate*)Dest = *(FScriptDelegate*)Src;
	}
}
void UDelegateProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
    if( DestOwnerObject )
	{
		if( ArrayDim==1)
		{
			UObject* SourceObject = ((FScriptDelegate*)Src)->Object;
			UObject* TargetObject = SourceObject;
			if ( SourceObject != NULL )
			{
				if ( SourceObject->HasAnyFlags(RF_ClassDefaultObject) )
				{
					if ( DestOwnerObject->IsA(SourceObject->GetClass()) )
					{
						TargetObject = DestOwnerObject;
					}
					else if ( SubobjectRoot != DestOwnerObject && SubobjectRoot->IsA(SourceObject->GetClass()) )
					{
						TargetObject = SubobjectRoot;
					}
					else if ( InstanceGraph != NULL )
					{
						UObject* InstancedTargetObject = InstanceGraph->GetDestinationObject(SourceObject);
						if (InstancedTargetObject != NULL)
						{
							TargetObject = InstancedTargetObject;
						}
					}
				}
			}

			((FScriptDelegate*)Dest)->FunctionName = ((FScriptDelegate*)Src)->FunctionName;
			((FScriptDelegate*)Dest)->Object = TargetObject;
		}
		else
		{
			for( INT i=0; i<ArrayDim; i++ )
			{
				UObject* SourceObject = ((FScriptDelegate*)Src)[i].Object;
				UObject* TargetObject = SourceObject;
				if ( SourceObject != NULL )
				{
					if ( SourceObject->HasAnyFlags(RF_ClassDefaultObject) )
					{
						if ( DestOwnerObject->IsA(SourceObject->GetClass()) )
						{
							TargetObject = DestOwnerObject;
						}
						else if ( SubobjectRoot->IsA(SourceObject->GetClass()) )
						{
							TargetObject = SubobjectRoot;
						}
					}
				}
			
				((FScriptDelegate*)Dest)[i].FunctionName = ((FScriptDelegate*)Src)[i].FunctionName;
				((FScriptDelegate*)Dest)[i].Object = TargetObject;
			}
		}
	}
	else
	{
		if( ArrayDim==1 )
		{
			*(FScriptDelegate*)Dest = *(FScriptDelegate*)Src;
		}
		else
		{
			for( INT i=0; i<ArrayDim; i++ )
			{
				((FScriptDelegate*)Dest)[i] = ((FScriptDelegate*)Src)[i];
			}
		}
	}
}
UBOOL UDelegateProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	UBOOL bResult = FALSE;

	FScriptDelegate* DA = (FScriptDelegate*)A;
	FScriptDelegate* DB = (FScriptDelegate*)B;
	
	if( DB == NULL )
	{
		bResult = DA->FunctionName == NAME_None;
	}
	else if ( DA->FunctionName == DB->FunctionName )
	{
		if ( DA->Object == DB->Object )
		{
			bResult = TRUE;
		}
		else if	((DA->Object == NULL || DB->Object == NULL)
			&&	(PortFlags&PPF_DeltaComparison) != 0)
		{
			bResult = TRUE;
		}
	}

	return bResult;
}
void UDelegateProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	Ar << *(FScriptDelegate*)Value;
}
UBOOL UDelegateProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	// JohnB: Do not allow replication of delegates, as there is no way to make this secure (it allows the execution of any function in any object, on the remote client/server)
	return 1;
}
FString UDelegateProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("FScriptDelegate");
}
FString UDelegateProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("DELEGATE");
}
void UDelegateProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	FScriptDelegate* ScriptDelegate = (FScriptDelegate*)PropertyValue;
	check(ScriptDelegate != NULL);
	UBOOL bDelegateHasValue = ScriptDelegate->FunctionName != NAME_None;
	ValueStr += FString::Printf( TEXT("%s.%s"),
		ScriptDelegate->Object != NULL
			? *ScriptDelegate->Object->GetName()
			: bDelegateHasValue && Parent != NULL
				? *Parent->GetName()
				: TEXT("(null)"),
		*ScriptDelegate->FunctionName.ToString() );
}
const TCHAR* UDelegateProperty::ImportText( const TCHAR* Buffer, BYTE* PropertyValue, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	TCHAR ObjName[NAME_SIZE];
	TCHAR FuncName[NAME_SIZE];
	// Get object name
	INT i;
	for( i=0; *Buffer && *Buffer != TCHAR('.') && *Buffer != TCHAR(')') && *Buffer != TCHAR(','); Buffer++ )
	{
		ObjName[i++] = *Buffer;
	}
	ObjName[i] = TCHAR('\0');
	// Get function name
	if( *Buffer )
	{
		Buffer++;
		for( i=0; *Buffer && *Buffer != TCHAR(')') && *Buffer != TCHAR(','); Buffer++ )
		{
			FuncName[i++] = *Buffer;
		}
		FuncName[i] = '\0';                
	}
	else
	{
		FuncName[0] = '\0';
	}
	UClass *Cls = FindObject<UClass>(ANY_PACKAGE,ObjName);
	UObject *Object = NULL;
	if (Cls != NULL)
	{
		// If we're importing defaults for a class and this delegate is being assigned to some function in the class
		// don't set the object, otherwise it will reference the default object for the owning class, execDelegateFunction
		// will "do the right thing"
		if ( Parent == Cls->GetDefaultObject() )
		{
			Object = NULL;
		}
		else
		{
			Object = Cls->GetDefaultObject();
		}
	}
	else
	{
		Object = StaticFindObject( UObject::StaticClass(), ANY_PACKAGE, ObjName );
		if (Object != NULL)
		{
			Cls = Object->GetClass();
		}
	}
	UFunction *Func = FindField<UFunction>( Cls, FuncName );
	// Check function params.
	if( Func != NULL )
	{
		// Find the delegate UFunction to check params
		UFunction* Delegate = Function;
		check(Delegate && "Invalid delegate property");

		// check return type and params
		if(	Func->NumParms == Delegate->NumParms )
		{
			INT Count=0;
			for( TFieldIterator<UProperty> It1(Func),It2(Delegate); Count<Delegate->NumParms; ++It1,++It2,++Count )
			{
				if( It1->GetClass()!=It2->GetClass() || (It1->PropertyFlags&CPF_OutParm)!=(It2->PropertyFlags&CPF_OutParm) )
				{
					debugf(NAME_Warning,TEXT("Function %s does not match param types with delegate %s"), *Func->GetName(), *Delegate->GetName());
					Func = NULL;
					break;
				}
			}
		}
		else
		{
			debugf(NAME_Warning,TEXT("Function %s does not match number of params with delegate %s"),*Func->GetName(), *Delegate->GetName());
			Func = NULL;
		}
	}
	else 
	{
		debugf(NAME_Warning,TEXT("Unable to find delegate function %s in object %s (found class: %s)"),FuncName,ObjName,*Cls->GetName());
	}

	debugfSlow(TEXT("... importing delegate FunctionName:'%s'(%s)   Object:'%s'(%s)"),Func != NULL ? *Func->GetName() : TEXT("NULL"), FuncName, Object != NULL ? *Object->GetFullName() : TEXT("NULL"), ObjName);
	
	(*(FScriptDelegate*)PropertyValue).Object		= Func ? Object				: NULL;
	(*(FScriptDelegate*)PropertyValue).FunctionName = Func ? Func->GetFName()	: NAME_None;

	return Func != NULL ? Buffer : NULL;
}

void UDelegateProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Function << SourceDelegate;
}

UBOOL UDelegateProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return (*(FScriptDelegate*)Data).FunctionName != NAME_None;
}

void UDelegateProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	(*(FScriptDelegate*)Data).Object = NULL;
	(*(FScriptDelegate*)Data).FunctionName = NAME_None;
}

UBOOL UDelegateProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.DelegateValue = (FScriptDelegate*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UDelegateProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(FScriptDelegate*)PropertyValueAddress = *PropertyValue.DelegateValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UDelegateProperty);

/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

INT UBoolProperty::GetMinAlignment() const
{
	return sizeof(BITFIELD);
}
void UBoolProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	UBoolProperty* PrevBool = Cast<UBoolProperty>( Prev );
	ElementSize = sizeof(BITFIELD);
	if (GetOuterUField()->MergeBools() && PrevBool != NULL && NEXT_BITFIELD(PrevBool->BitMask))
	{
		Offset  = Prev->Offset;
		BitMask = NEXT_BITFIELD(PrevBool->BitMask);
	}
	else
	{
		Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
		BitMask = FIRST_BITFIELD;
	}
}
void UBoolProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << BitMask;
	}
}
FString UBoolProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("UBOOL");
}
FString UBoolProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("UBOOL");
}
void UBoolProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ExportFormat ) const
{
	const TCHAR* TrueStr = ExportFormat == ITF_Unreal ? TEXT("True") : TEXT("true");
	const TCHAR* FalseStr = ExportFormat == ITF_Unreal ? TEXT("False") : TEXT("false");
	TCHAR* Temp
	=	(TCHAR*) ((PortFlags & PPF_Localized)
	?	(((*(BITFIELD*)PropertyValue) & BitMask) ? GTrue  : GFalse )
	:	(((*(BITFIELD*)PropertyValue) & BitMask) ? TrueStr : FalseStr));
	ValueStr += FString::Printf( TEXT("%s"), Temp );
}
const TCHAR* UBoolProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	FString Temp; 
	Buffer = ReadToken( Buffer, Temp );
	if( !Buffer )
		return NULL;
	if( Temp==TEXT("1") || Temp==TEXT("True") || Temp==GTrue || Temp == TEXT("Yes") || Temp == GYes )
	{
		*(BITFIELD*)Data |= BitMask;
	}
	else 
	if( Temp==TEXT("0") || Temp==TEXT("False") || Temp==GFalse || Temp == TEXT("No") || Temp == GNo )
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
UBOOL UBoolProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return ((*(BITFIELD*)A ^ (B ? *(BITFIELD*)B : 0)) & BitMask) == 0;
}
void UBoolProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
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
void UBoolProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(BITFIELD*)Dest = (*(BITFIELD*)Dest & ~BitMask) | (*(BITFIELD*)Src & BitMask);
}
UBOOL UBoolProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(BITFIELD*)Data & BitMask;
}

void UBoolProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(BITFIELD*)Data &= ~BitMask;
}

UBOOL UBoolProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.BoolValue = (*(BITFIELD*)PropertyValueAddress & BitMask) ? TRUE : FALSE;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UBoolProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		if ( PropertyValue.BoolValue )
		{
			*(BITFIELD*)PropertyValueAddress |= BitMask;
		}
		else
		{
			*(BITFIELD*)PropertyValueAddress &= ~BitMask;
		}
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UBoolProperty);

/*-----------------------------------------------------------------------------
	UFloatProperty.
-----------------------------------------------------------------------------*/

INT UFloatProperty::GetMinAlignment() const
{
	return sizeof(FLOAT);
}
void UFloatProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(FLOAT);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
}
void UFloatProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(FLOAT*)Dest = *(FLOAT*)Src;
}
void UFloatProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if( ArrayDim==1 )
	{
		*(FLOAT*)Dest = *(FLOAT*)Src;
	}
	else
	{
		appMemcpy( Dest, Src, ArrayDim*ElementSize );
	}
}
UBOOL UFloatProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return *(FLOAT*)A == (B ? *(FLOAT*)B : 0);
}
void UFloatProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	Ar << *(FLOAT*)Value;
}
UBOOL UFloatProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	Ar << *(FLOAT*)Data;
	return 1;
}
FString UFloatProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("FLOAT");
}
FString UFloatProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("FLOAT");
}
void UFloatProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	ValueStr += FString::Printf( TEXT("%f"), *(FLOAT*)PropertyValue );
}
const TCHAR* UFloatProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	if ( *Buffer == TCHAR('+') || *Buffer == TCHAR('-') || *Buffer == TCHAR('.') || (*Buffer >= TCHAR('0') && *Buffer <= TCHAR('9')) )
	{
		// only import this value if Buffer is numeric
		*(FLOAT*)Data = appAtof(Buffer);
		while( *Buffer == TCHAR('+') || *Buffer == TCHAR('-') || *Buffer == TCHAR('.') || (*Buffer >= TCHAR('0') && *Buffer <= TCHAR('9')) )
		{
			Buffer++;
		}

		if ( *Buffer == TCHAR('f') || *Buffer == TCHAR('F') )
		{
			Buffer++;
		}
	}
	return Buffer;
}
UBOOL UFloatProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(FLOAT*)Data != 0.f;
}

void UFloatProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(FLOAT*)Data = 0.f;
}

UBOOL UFloatProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.FloatValue = *(FLOAT*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UFloatProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(FLOAT*)PropertyValueAddress = PropertyValue.FloatValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UFloatProperty);

/*-----------------------------------------------------------------------------
	UObjectProperty.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UObjectProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UObjectProperty, PropertyClass ) );
}
INT UObjectProperty::GetMinAlignment() const
{
#if _WIN64
	return __alignof(UObject);
#else
	return sizeof(UObject*);
#endif
}
void UObjectProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(UObject*);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	if ((PropertyFlags & CPF_EditInline) && (PropertyFlags & CPF_ExportObject) && !(PropertyFlags & CPF_Component))
	{
		PropertyFlags |= CPF_NeedCtorLink;
	}
}

/**
 * Instances any UObjectProperty values that still match the default value.
 *
 * @param	Value				the address where the pointer to the instanced object should be stored.  This should always correspond to the BASE + OFFSET, where
 *									BASE = (for class member properties) the address of the UObject which contains this data, (for script struct member properties) the
 *										address of the struct's data
 *									OFFSET = the Offset of this UProperty from base
 * @param	DefaultValue		the address where the pointer to the default value is stored.  Evaluated the same way as Value
 * @param	OwnerObject			the object that contains the destination data.  Will be the used as the Outer for any newly instanced subobjects.
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UObjectProperty::InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if ( OwnerObject != NULL && (PropertyFlags&CPF_NeedCtorLink) != 0 )
	{
		for ( INT ArrayIndex = 0; ArrayIndex < ArrayDim; ArrayIndex++ )
		{
			UObject*& CurrentObj = ((UObject**)Value)[ArrayIndex];
			UObject* DefaultObj = DefaultValue ? ((UObject**)DefaultValue)[ArrayIndex] : NULL;

			if ( DefaultObj != NULL && CurrentObj != NULL && CurrentObj->IsTemplate() )
			{
				UBOOL bShouldInstance = CurrentObj == DefaultObj;
				if ( !bShouldInstance && OwnerObject->GetArchetype()->HasAnyFlags(RF_ArchetypeObject) && DefaultObj->IsBasedOnArchetype(CurrentObj) )
				{
					/*
					This block of code is intended to catch cases where the InitProperties() was called on OwnerObject before OwnerObject's archetype
					had called CondtionalPostLoad.  What happens in this case is that CurrentObj doesn't match DefaultObj;
					CurrentObj is actually an archetype of DefaultObj.  This seems to only happen with archetypes/prefabs, where e.g.

					1. PackageA is loaded, creating PrefabInstanceA, which causes PrefabA to be created
					2. PrefabInstanceA copies the properties from PrefabA, whose values are still the values inherited from PrefabA's archetype
						since CondtionalPostLoad() hasn't yet been called on PrefabA.
					3. CondtionalPostLoad() is called on PrefabA, which calls InstanceSubobjectTemplates and gives PrefabA its own copies of the new instanced subobjects
					4. CondtionalPostLoad() is called on PrefabInstanceA, and CurrentObj [for PrefabInstanceA] is still pointing to the inherited value
						from PrefabA's archetype.

					However, if we are updating a Prefab from a PrefabInstance, then DefaultObj->IsBasedOnArchetype(CurrentObj) will always return TRUE
					since SourceComponent will be the instance and CurrentValue will be its archetype.
					*/
					bShouldInstance = InstanceGraph == NULL || !InstanceGraph->IsUpdatingArchetype();
				}

				if ( bShouldInstance == TRUE )
				{
					FName NewObjName = NAME_None;
					if ( OwnerObject->IsTemplate() )
					{
						// when we're creating a subobject for an archetype/CDO, the name for the new object should be the same as the source object
						NewObjName = DefaultObj->GetFName();
						if ( StaticFindObjectFast(CurrentObj->GetClass(), OwnerObject, NewObjName) != NULL )
						{
							NewObjName = MakeUniqueObjectName(OwnerObject, CurrentObj->GetClass(), NewObjName);
						}
					}

					CurrentObj = StaticConstructObject(CurrentObj->GetClass(), OwnerObject, 
						NewObjName, OwnerObject->GetMaskedFlags(RF_PropagateToSubObjects), DefaultObj, GError,
						InstanceGraph ? InstanceGraph->GetDestinationRoot() : OwnerObject,
						InstanceGraph
						);
				}
			}
		}
	}
}

/**
 * Worker for CopySingleValue/CopyCompleteValue
 */
void UObjectProperty::InstanceValue( BYTE* DestAddress, BYTE* SrcAddress, UObject* SubobjectRoot, UObject* DestOwnerObject, FObjectInstancingGraph* InstanceGraph ) const
{
	// Don't instance subobjects if they are not marked as requiring deep copying.
	if(	(PropertyFlags & CPF_NeedCtorLink) 
	// Or if there is no owner object
	&&	DestOwnerObject 
	// Or if we explicitly disable subobject instancing
	&&	!(GUglyHackFlags & HACK_DisableSubobjectInstancing)
	// Or if the caller doesn't want us to instance subobjects
	&& (InstanceGraph == NULL || InstanceGraph->IsObjectInstancingEnabled()) )
	{
		UObject* SrcObject = *((UObject**)SrcAddress);
		UObject* CurrentValue = *((UObject**)DestAddress);
		if( SrcObject )
		{
			UClass* Cls = SrcObject->GetClass();

			// If we have a DestOwnerObject, get some object flags from it.
			EObjectFlags NewObjFlags = 0;
			if(DestOwnerObject)
			{
				NewObjFlags = DestOwnerObject->GetMaskedFlags(RF_PropagateToSubObjects);
			}
			
			UBOOL bIsCreatingArchetype = ((NewObjFlags & RF_ArchetypeObject) != 0 && !SrcObject->IsTemplate());
			UBOOL bIsUpdatingArchetype = (GUglyHackFlags&HACK_UpdateArchetypeFromInstance) != 0;
			if ( InstanceGraph != NULL )
			{
				// if we're loading from disk, we're not actually creating an archetype based on an instance
				bIsCreatingArchetype = bIsCreatingArchetype && InstanceGraph->IsCreatingArchetype(TRUE);
				bIsUpdatingArchetype = bIsUpdatingArchetype && InstanceGraph->IsUpdatingArchetype();
			}

			// determine what the new object name should be; normally, we are creating a new object, so use NAME_None
			FName NewObjName = NAME_None;

			// if we are updating an archetype, SrcObject will be an object instance placed in a map
			// so we don't want SrcObject to be set as the ObjectArchetype for the object we're creating here.  
			UObject* FinalObjectArchetype = NULL;

			if ( bIsCreatingArchetype == TRUE )
			{
				CurrentValue = SrcObject->GetArchetype();

				// if we are creating or updating an archetype from an instance, CurrentValue will always be zero, since we just re-initailized 
				// the archetype object against the instance.  The call to InitProperties memzeros the value of this property prior
				// to calling CopyCompleteValue.  In this case, the object previously assigned to this property's value is SrcObject's archetype.
				// If the value is already a CDO, however, then just use it as the template
				if ( bIsUpdatingArchetype == TRUE && !CurrentValue->IsTemplate(RF_ClassDefaultObject))
				{
					// when updating an archetype object, we want to reconstruct the existing object so
					// we need to use the same name and outer as the object currently assigned as the value for
					// this object property
					NewObjName = CurrentValue->GetFName();

					// when updating an archetype, CurrentValue points to the object we're about to replace in-memory; remember the
					// ObjectArchetype of the object currently assigned to this object property; after we've created the new object, we'll
					// restore its ObjectArchetype pointer to this value
					FinalObjectArchetype = CurrentValue->GetArchetype();

					// quick sanity check - if we are creating/updating an archetype, the current value of this property is already an archetype, and its Outer should be the
					// same object that we're creating objects for
					check(DestOwnerObject == CurrentValue->GetOuter());
				}
				else
				{
					// when creating a new archetype, SrcObject was linked to some other template (like a CDO or template subobject); that object will be the
					// object archetype for the new archetype we're creating
					FinalObjectArchetype = CurrentValue;
				}
			}
			else if ( DestOwnerObject->IsTemplate() )
			{
				// when we're creating a subobject for an archetype/CDO, the name for the new object should be the same as the source object
				NewObjName = SrcObject->GetFName();
				if ( StaticFindObjectFast(Cls, DestOwnerObject, NewObjName) != NULL )
				{
					NewObjName = MakeUniqueObjectName(DestOwnerObject, Cls, NewObjName);
				}
			}

			CurrentValue = *((UObject**)DestAddress) = StaticConstructObject( Cls, DestOwnerObject, NewObjName, NewObjFlags, SrcObject, GError, SubobjectRoot, InstanceGraph ); 

			// If we making an archetype and NOT using another archetype as our template (i.e. duplicating an archetype, or creating an archetype of an archetype)
			// it's archetype is currently the instance we used for creating the archetype, so change it to the previously determined archetype
			if( bIsCreatingArchetype )
			{
				check(CurrentValue);

				CurrentValue->SetArchetype( FinalObjectArchetype );

				UComponent* InstancedComponent = Cast<UComponent>(CurrentValue);
				if ( InstancedComponent != NULL )
				{
					UComponent* SourceComponent = CastChecked<UComponent>(FinalObjectArchetype);

					// we're here if a component was assigned to a non-component instanced object property.  in this case, we'll want to
					// do something similar to the code in GetInstancedComponent - restore the TemplateName and TemplateOwnerClass
					InstancedComponent->TemplateOwnerClass = SourceComponent->TemplateOwnerClass;
					InstancedComponent->TemplateName = SourceComponent->TemplateName;
				}
			}
		}
		else
		{
			*((UObject**)DestAddress) = SrcObject;
		}
	}
	else
	{
		*(UObject**)DestAddress = *(UObject**)SrcAddress;
	}
}

void UObjectProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	InstanceValue((BYTE*)Dest, (BYTE*)Src, SubobjectRoot, DestOwnerObject, InstanceGraph);
}
void UObjectProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	for ( INT ArrayIdx = 0; ArrayIdx < ArrayDim; ArrayIdx++ )
	{
		InstanceValue((BYTE*)Dest + ElementSize * ArrayIdx, (BYTE*)Src + ElementSize * ArrayIdx, SubobjectRoot, DestOwnerObject, InstanceGraph);
	}
}
UBOOL UObjectProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive )
	{
		UObject* ObjectA = *(UObject**)A;
		UObject* ObjectB = *(UObject**)B;
		return (ObjectA && ObjectB)
			? (ObjectA->GetLinkerIndex() == ObjectB->GetLinkerIndex())
			: (!ObjectA && !ObjectB);
	}
	else
#endif
	{
		// always serialize the cross level references, because they could be NULL
		// @todo: okay, this is pretty hacky overall - we should have a PortFlag or something
		// that is set during SavePackage. Other times, we don't want to immediately return FALSE
		// (instead of just this ExportDefProps case)
		if (!(PortFlags & PPF_ExportDefaultProperties) && (PropertyFlags & CPF_CrossLevel))
		{
			return FALSE;
		}

		UBOOL bResult = (A ? *(UObject**)A : NULL) == (B ? *(UObject**)B : NULL);
		if ( !bResult && (PortFlags&PPF_DeltaComparison) != 0 && A != NULL && B != NULL )
		{
			UClass* Scope = NULL;
			UObject* ObjectA = *(UObject**)A;
			UObject* ObjectB = *(UObject**)B;
			if ( ObjectA != NULL && ObjectB != NULL )
			{
				if (ObjectA->GetClass() == ObjectB->GetClass())
				{
					Scope = ObjectA->GetClass();
				}
				else if ( ObjectA->HasAnyFlags(RF_ClassDefaultObject) )
				{
					if ( ObjectA->GetArchetype() == ObjectB )
					{
						Scope = ObjectB->GetClass();
					}
					else if ( ObjectB->GetArchetype() == ObjectA )
					{
						Scope = ObjectA->GetClass();
					}
				}
			}

			if ( Scope != NULL )
			{
				bResult = TRUE;
				for ( UProperty* Prop = Scope->PropertyLink; Prop && bResult; Prop = Prop->PropertyLinkNext )
				{
					if ( Prop->Offset < Scope->GetPropertiesSize() && Prop->ShouldDuplicateValue() )
					{
						for ( INT Idx = 0; Idx < Prop->ArrayDim; Idx++ )
						{
							if ( !Prop->Matches(ObjectA, ObjectB, Idx, FALSE, PortFlags) )
							{
								bResult = FALSE;
								break;
							}
						}
					}
				}
			}
		}

		return bResult;
	}
}
void UObjectProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	if (//(Ar.IsLoading() || Ar.IsSaving()) && 
		PropertyFlags & CPF_CrossLevel)
	{
		// @todo: verify that this is actually okay to get the owner object
		Ar.WillSerializePotentialCrossLevelPointer((UObject*)((BYTE*)Value - Offset), this);
	}
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
FString UObjectProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return FString::Printf( TEXT("class %s%s*"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName() );
}
FString UObjectProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = FString::Printf(TEXT("%s%s"), PropertyClass->GetPrefixCPP(), *PropertyClass->GetName());
	return TEXT("OBJECT");
}

void UObjectProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	UObject* Temp = *(UObject **)PropertyValue;
	if( Temp != NULL )
	{
		UBOOL bExportFullyQualified = true;

		// when exporting t3d, we don't want to qualify object names of objects in the level,
		// because it won't be able to find the object when importing (no more myLevel!)
		if ((PortFlags & PPF_ExportsNotFullyQualified) && Parent != NULL)
		{
			// get a pointer to the level by going up the outer chain
			UObject* LevelOuter = Parent->GetOutermost();
			// if the object we are pointing to is in the level, then don't full qualify it
			if (Temp->IsIn(LevelOuter))
			{
				bExportFullyQualified = false;
			}
		}

		// if we want a full qualified object reference, use the pathname, otherwise, use just the object name
		if (bExportFullyQualified)
		{
			UObject* StopOuter = NULL;
			if ( (PortFlags&PPF_SimpleObjectText) != 0 && Parent != NULL )
			{
				StopOuter = Parent->GetOutermost();
			}

			ValueStr += FString::Printf( TEXT("%s'%s'"), *Temp->GetClass()->GetName(), *Temp->GetPathName(StopOuter) );
		}
		else
		{
			ValueStr += FString::Printf( TEXT("%s'%s'"), *Temp->GetClass()->GetName(), *Temp->GetName() );
		}
	}
	else
	{
		ValueStr += TEXT("None");
	}
}

/**
 * Parses a text buffer into an object reference.
 *
 * @param	Property			the property that the value is being importing to
 * @param	OwnerObject			the object that is importing the value; used for determining search scope.
 * @param	RequiredMetaClass	the meta-class for the object to find; if the object that is resolved is not of this class type, the result is NULL.
 * @param	PortFlags			bitmask of EPropertyPortFlags that can modify the behavior of the search
 * @param	Buffer				the text to parse; should point to a textual representation of an object reference.  Can be just the object name (either fully 
 *								fully qualified or not), or can be formatted as a const object reference (i.e. SomeClass'SomePackage.TheObject')
 *								When the function returns, Buffer will be pointing to the first character after the object value text in the input stream.
 * @param	ResolvedValue		receives the object that is resolved from the input text.
 *
 * @return	TRUE if the text is successfully resolved into a valid object reference of the correct type, FALSE otherwise.
 */
UBOOL UObjectProperty::ParseObjectPropertyValue( const UProperty* Property, UObject* OwnerObject, UClass* RequiredMetaClass, DWORD PortFlags, const TCHAR*& Buffer, UObject*& out_ResolvedValue )
{
	check(Property);
	check(RequiredMetaClass);

 	const TCHAR* InBuffer = Buffer;

	FString Temp;
	Buffer = ReadToken(Buffer, Temp, TRUE);
	if ( Buffer == NULL )
	{
		return FALSE;
	}

	if ( Temp == TEXT("None") )
	{
		out_ResolvedValue = NULL;
	}
	else
	{
		UClass*	ObjectClass = RequiredMetaClass;

		SkipWhitespace(Buffer);

		UBOOL bWarnOnNULL = (PortFlags&PPF_CheckReferences)!=0;

		if( *Buffer == TCHAR('\'') )
		{
			FString ObjectText;
			Buffer = ReadToken( ++Buffer, ObjectText, TRUE );
			if( Buffer == NULL )
			{
				return FALSE;
			}

			if( *Buffer++ != TCHAR('\'') )
			{
				return FALSE;
			}

			ObjectClass = FindObject<UClass>( ANY_PACKAGE, *Temp );
			if( ObjectClass == NULL )
			{
				if( bWarnOnNULL )
				{
					warnf( NAME_Error, TEXT("%s: unresolved cast in '%s'"), *Property->GetFullName(), InBuffer );
				}
				return FALSE;
			}

			// If ObjectClass is not a subclass of PropertyClass, fail.
			if( !ObjectClass->IsChildOf(RequiredMetaClass) )
			{
				warnf( NAME_Error, TEXT("%s: invalid cast in '%s'"), *Property->GetFullName(), InBuffer );
				return FALSE;
			}

			// Try the find the object.
			out_ResolvedValue = UObjectProperty::FindImportedObject(Property, OwnerObject, ObjectClass, RequiredMetaClass, *ObjectText, PortFlags);
		}
		else
		{
			// Try the find the object.
			out_ResolvedValue = UObjectProperty::FindImportedObject(Property, OwnerObject, ObjectClass, RequiredMetaClass, *Temp, PortFlags);
		}

		if ( out_ResolvedValue != NULL && !out_ResolvedValue->GetClass()->IsChildOf(RequiredMetaClass) )
		{
			if (bWarnOnNULL )
			{
				warnf( NAME_Error, TEXT("%s: bad cast in '%s'"), *Property->GetFullName(), InBuffer );
			}

			out_ResolvedValue = NULL;
			return FALSE;
		}

		// If we couldn't find it or load it, we'll have to do without it.
		if ( out_ResolvedValue == NULL )
		{
			if( bWarnOnNULL )
			{
				warnf( NAME_Warning, TEXT("%s: unresolved reference to '%s'"), *Property->GetFullName(), InBuffer );
			}
			return FALSE;
		}
	}

	return TRUE;
}

const TCHAR* UObjectProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	const TCHAR* Buffer = InBuffer;

	if ( !ParseObjectPropertyValue(this, Parent, PropertyClass, PortFlags, Buffer, *(UObject**)Data) )
	{
		return NULL;
	}

	return Buffer;
}
UObject* UObjectProperty::FindImportedObject( const UProperty* Property, UObject* OwnerObject, UClass* ObjectClass, UClass* RequiredMetaClass, const TCHAR* Text, DWORD PortFlags/*=0*/ )
{
	UObject*	Result = NULL;
	check( ObjectClass->IsChildOf(RequiredMetaClass) );

	UBOOL AttemptNonQualifiedSearch = (PortFlags & PPF_AttemptNonQualifiedSearch) != 0; 

	// if we are importing default properties, first look for a matching subobject by
	// looking through the archetype chain at each outer and stop once the outer chain reaches the owning class's default object
	if (PortFlags & PPF_ParsingDefaultProperties)
	{
		for (UObject* SearchStart = OwnerObject; Result == NULL && SearchStart != NULL; SearchStart = SearchStart->GetOuter())
		{
			UObject* ScopedSearchRoot = SearchStart;
			while (Result == NULL && ScopedSearchRoot != NULL)
			{
				Result = StaticFindObject(ObjectClass, ScopedSearchRoot, Text);
				// don't think it's possible to get a non-subobject here, but it doesn't hurt to check
				if (Result != NULL && !Result->IsTemplate(RF_ClassDefaultObject))
				{
					Result = NULL;
				}

				ScopedSearchRoot = ScopedSearchRoot->GetArchetype();
			}
			if (SearchStart->HasAnyFlags(RF_ClassDefaultObject))
			{
				break;
			}
		}
	}
	
	// if we have a parent, look in the parent, then it's outer, then it's outer, ... 
	// this is because exported object properties that point to objects in the level aren't
	// fully qualified, and this will step up the nested object chain to solve any name
	// collisions within a nested object tree
	UObject* ScopedSearchRoot = OwnerObject;
	while (Result == NULL && ScopedSearchRoot != NULL)
	{
		Result = StaticFindObject(ObjectClass, ScopedSearchRoot, Text);
		// disallow class default subobjects here while importing defaults
		// this prevents the use of a subobject name that doesn't exist in the scope of the default object being imported
		// from grabbing some other subobject with the same name and class in some other arbitrary default object
		if (Result != NULL && (PortFlags & PPF_ParsingDefaultProperties) && Result->IsTemplate(RF_ClassDefaultObject))
		{
			Result = NULL;
		}

		ScopedSearchRoot = ScopedSearchRoot->GetOuter();
	}

	if (Result == NULL)
	{
		// attempt to find a fully qualified object
		Result = StaticFindObject(ObjectClass, NULL, Text);

		if (Result == NULL)
		{
			// match any object of the correct class whose path contains the specified path
			Result = StaticFindObject(ObjectClass, ANY_PACKAGE, Text);
			// disallow class default subobjects here while importing defaults
			if (Result != NULL && (PortFlags & PPF_ParsingDefaultProperties) && Result->IsTemplate(RF_ClassDefaultObject))
			{
				Result = NULL;
			}
		}
	}

	// if we haven;t found it yet, then try to find it without a qualified name
	if (!Result)
	{
		TCHAR* Dot = appStrrchr(Text, '.');
		if (Dot)
		{
			if (AttemptNonQualifiedSearch)
			{
				// search with just the object name
				Result = FindImportedObject(Property, OwnerObject, ObjectClass, RequiredMetaClass, Dot + 1);
			}

			// If we still can't find it, try to load it. (Only try to load fully qualified names)
			if(!Result)
			{
				DWORD LoadFlags = LOAD_NoWarn | LOAD_FindIfFail;

				// if we're running make and need to load an outside package, load the package, but don't try to load
				// all the objects in the other package's import table, as the external package may have a dependency
				// on a .u package that isn't compiled yet
				// (also don't allow redirects to be followed)
				if ( GIsUCCMake )
				{
					LoadFlags |= LOAD_Quiet; // remove unnecessary load warnings as the compiler will already log one with the property that was being imported and such
					FScopedRedirectorCatcher Catcher(Text);
					// attempt to load the object
					Result = StaticLoadObject(ObjectClass, NULL, Text, NULL, LoadFlags | LOAD_NoVerify, NULL);
					// if the object was a redirector, spit out a compiler error
					if (Catcher.WasRedirectorFollowed())
					{
						warnf(NAME_Error, TEXT("Object '%s' has been renamed or moved.  Named references in script must not point to redirectors as these cannot be repaired by FixupRedirects.  Change text to:\n   '%s'."), Text, *Result->GetPathName());
					}
				}
				// we don't want to load another level, so only keep cross-level references to levels that were already loaded when copy/pasting
				else if (!(Property->PropertyFlags & CPF_CrossLevel))
				{
					Result = StaticLoadObject(ObjectClass, NULL, Text, NULL, LoadFlags, NULL);
				}
			}
		}
	}

	// if we found an object, and we have a parent, make sure we are in the same package if the found object is private, unless it's a cross level property
	if ((Property->PropertyFlags & CPF_CrossLevel) == 0 && Result && !Result->HasAnyFlags(RF_Public) && OwnerObject && Result->GetOutermost() != OwnerObject->GetOutermost())
	{
		warnf( NAME_Warning, TEXT("Illegal text reference to a private object in external package (%s) from referencer (%s).  Import failed..."), *Result->GetFullName(), *OwnerObject->GetFullName());
		Result = NULL;
	}

	check(!Result || Result->IsA(RequiredMetaClass));
	return Result;
}

UBOOL UObjectProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(UObject**)Data != NULL;
}

void UObjectProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(UObject**)Data = NULL;
}

UBOOL UObjectProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.ObjectValue = *(UObject**)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UObjectProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(UObject**)PropertyValueAddress = PropertyValue.ObjectValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UObjectProperty);

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UClassProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UClassProperty, MetaClass ) );
}
void UClassProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << MetaClass;
	check(MetaClass||HasAnyFlags(RF_ClassDefaultObject));
}
const TCHAR* UClassProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	const TCHAR* Result = UObjectProperty::ImportText( Buffer, Data, PortFlags, Parent, ErrorText );
	if( Result )
	{
		// Validate metaclass.
		UClass*& C = *(UClass**)Data;
		if (C != NULL && (C->GetClass() != UClass::StaticClass() || !C->IsChildOf(MetaClass)))
		{
			// the object we imported doesn't implement our interface class
			if ( ErrorText != NULL )
			{
				ErrorText->Logf(TEXT("Invalid object '%s' specified for property '%s'"), *C->GetFullName(), *GetName());
			}
			else
			{
				warnf(NAME_Error, TEXT("Invalid object '%s' specified for property '%s'"), *C->GetFullName(), *GetName());
			}
			C = NULL;
			Result = NULL;
		}
	}
	return Result;
}
UBOOL UClassProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(UClass**)Data != NULL;
}

void UClassProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(UClass**)Data = NULL;
}
FString UClassProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = TEXT("UClass");
	return TEXT("OBJECT");
}

UBOOL UClassProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.ClassValue = *(UClass**)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UClassProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(UClass**)PropertyValueAddress = PropertyValue.ClassValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UClassProperty);
/*-----------------------------------------------------------------------------
	UComponentProperty.
-----------------------------------------------------------------------------*/
/**
 * Determine whether the editable properties of CompA and CompB are identical. Used
 * to determine whether the instanced component has been modified in the editor.
 * 
 * @param	CompA		the first UComponent to compare
 * @param	CompB		the second UComponent to compare
 * @param	PortFlags	flags for modifying the criteria used for determining whether the components are identical
 *
 * @return	TRUE if the values of all of the editable properties of CompA match the values in CompB
 */
static UBOOL AreComponentsIdentical( UComponent* CompA, UComponent* CompB, DWORD PortFlags )
{
	check(CompA);
	check(CompB);

	if ( CompA->GetClass() != CompB->GetClass() )
		return FALSE;

	UBOOL bPerformDeepComparison = (PortFlags&PPF_DeepComparison) != 0;
	if ( (PortFlags&PPF_DeepCompareInstances) != 0 )
	{
		UBOOL bCompAIsTemplate = CompA->IsTemplate();
		UBOOL bCompBIsTemplate = CompB->IsTemplate();
		if ( !bPerformDeepComparison )
		{
			bPerformDeepComparison = bCompAIsTemplate != bCompBIsTemplate;
		}

		// if we are comparing instances to their template, and the instance isn't actually linked to a
		// template (as indicated by the component's TemplateName == NAME_None), then the instance must
		// should be considered different, since it would not be automatically instanced when the package
		// is reloaded
		if ( (!bCompAIsTemplate && !CompA->IsInstanced()) ||
			(!bCompBIsTemplate && !CompB->IsInstanced()) )
		{
			bPerformDeepComparison = FALSE;
		}
	}

	if ( bPerformDeepComparison )
	{
		for ( UProperty* Prop = CompA->GetClass()->PropertyLink; Prop; Prop = Prop->PropertyLinkNext )
		{
			// only the properties that could have been modified in the editor should be compared
			// (skipping the name and archetype properties, since name will almost always be different)
			UBOOL bConsiderProperty = Prop->ShouldDuplicateValue();
			if ( (PortFlags&PPF_Copy) != 0 )
			{
				bConsiderProperty = (Prop->PropertyFlags&CPF_Edit) != 0;
			}

			UBOOL bComparisonResultDifference = FALSE;
			if ( bConsiderProperty && (Prop->PropertyFlags&CPF_Edit) == 0 )
			{
				bComparisonResultDifference = TRUE;
				// this will really spam the log if we leave it in, so only enable it for debugging purposes
// 				debugfSuppressed(NAME_Dev, TEXT("Previous code wouldn't have considered the value for property %s when comparing '%s' and '%s'"),
// 					*Prop->GetPathName(), *CompA->GetFullName(), *CompB->GetFullName());
			}
			if ( bConsiderProperty )
			{
				for ( INT i = 0; i < Prop->ArrayDim; i++ )
				{
					if ( !Prop->Matches(CompA, CompB, i, FALSE, PortFlags) )
					{
						if ( bComparisonResultDifference == TRUE )
						{
							debugfSuppressed(NAME_DevComponents, TEXT("****  AreComponentsIdentical now returns FALSE because of property %s while comparing '%s' and '%s'   *****"),
								*Prop->GetPathName(), *CompA->GetFullName(), *CompB->GetFullName());
						}
						return FALSE;
					}
				}
			}
		}

		// Allow the component to compare its native/ intrinsic properties.
		UBOOL bNativePropertiesAreIdentical = CompA->AreNativePropertiesIdenticalTo( CompB );
		return bNativePropertiesAreIdentical;
	}

	return CompA == CompB;
}

/**
 * Copies the values for all of the editable properties of a UComponent from one component
 * to another.
 * 
 * @param	NewTemplate			the component to copy the values to
 * @param	InstanceComponent	the component to copy the values from
 * @param	SubobjectRoot		the first object in InstanceComponen
 */
static void InitializeFromInstance( UComponent* NewTemplate, UComponent* InstanceComponent, UObject* SubobjectRoot )
{
	check(NewTemplate);
	check(InstanceComponent);
	verify(NewTemplate->IsA(InstanceComponent->GetClass()));

	BYTE* Dest = (BYTE*)NewTemplate;
	BYTE* Src  = (BYTE*)InstanceComponent;
	for ( TFieldIterator<UProperty> It(NewTemplate->GetClass()); It; ++It )
	{
		UProperty* Prop = *It;
		// only the properties that could be modified in the editor need be copied
		if ( (Prop->PropertyFlags&CPF_Edit)!=0 && Prop->ShouldDuplicateValue() )
		{
			Prop->CopyCompleteValue(Dest + Prop->Offset, Src + Prop->Offset, SubobjectRoot, NewTemplate);
		}
	}
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
 */
void UComponentProperty::InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	if ( (PropertyFlags&CPF_Native) == 0 )
	{
		for ( INT ArrayIndex = 0; ArrayIndex < ArrayDim; ArrayIndex++ )
		{
			UComponent* CurrentValue = *((UComponent**)(Data + ArrayIndex * ElementSize));
			if ( CurrentValue != NULL )
			{
				UComponent* ComponentTemplate = DefaultData ? *((UComponent**)(DefaultData + ArrayIndex * ElementSize)): NULL;
				UComponent* NewValue = CurrentValue;

				// if the object we're instancing the components for (Owner) has the current component's outer in its archetype chain, and its archetype has a NULL value
				// for this component property it means that the archetype didn't instance its component, so we shouldn't either.
				//@fixme components: it seems like we're basically trying to figure out whether ComponentTemplate is NULL because this our archetype doesn't have a value, or if
				// it's NULL simply because this property is defined in Owner's class and Owner is a CDO.
				if (ComponentTemplate == NULL && CurrentValue != NULL && Owner->IsBasedOnArchetype(CurrentValue->GetOuter()))
				{
					debugfSuppressed(NAME_DevComponents, TEXT("Clearing component reference for component '%s' (archetype '%s'), owner '%s' (archetype '%s'), property '%s'"),*CurrentValue->GetFullName(),*CurrentValue->GetArchetype()->GetFullName(),*Owner->GetFullName(),*Owner->GetArchetype()->GetFullName(),*GetFullName());
					NewValue = NULL;
				}
				else
				{
					if ( ComponentTemplate == NULL )
					{
						// should only be here if our archetype doesn't contain this component property
						ComponentTemplate = CurrentValue;
					}
					else if ( InstanceGraph->IsUpdatingArchetype() )
					{
						// when updating an archetype from an instance, CurrentValue will be pointing to the component owned by the instance, since we've just
						// called InitProperties on Owner (which is the archetype) passing in the instance.
						UComponent* ComponentArchetype = CurrentValue->GetArchetype<UComponent>();

						//@note A: runtime component addition during archetype update
						// if CurrentValue's archetype is the CDO, it means that the component owned by the instance was not instanced from a template component owned by the archetype
						// object, but rather was created at runtime via editinlinenew.  In this case, we leave the CurrentValue pointing to the instance and check for that in
						// GetInstancedComponent...
						if ( !ComponentArchetype->HasAnyFlags(RF_ClassDefaultObject) )
						{
							if ( HasAnyPropertyFlags(CPF_Transient) && ComponentArchetype != ComponentTemplate->GetArchetype() && ComponentTemplate->GetArchetype()->IsTemplate() )
							{
								CurrentValue = ComponentTemplate->GetArchetype<UComponent>();
							}
							else
							{
								CurrentValue = CurrentValue->GetArchetype<UComponent>();
							}
						}
					}

					NewValue = InstanceGraph->GetInstancedComponent(ComponentTemplate, CurrentValue, Owner);
				}

				if ( NewValue != INVALID_OBJECT )
				{
					*((UComponent**)(Data + ArrayIndex * ElementSize)) = NewValue;
				}
			}
		}
	}
}

void UComponentProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat ) const
{
	if (!Parent || Parent->HasAnyFlags(RF_ClassDefaultObject))
	{
		UComponent*	Component = *(UComponent**)PropertyValue;
		UClass*		ParentClass = Parent
			? Parent->IsA(UClass::StaticClass())
				? CastChecked<UClass>(Parent) 
				: Parent->GetClass()
			: NULL;

		if (Component != NULL)
		{
			// get the name of the template for this component in the class (if it exists)
			// TemplateName is now always valid for components that are instanced
			FName ComponentName = ParentClass != NULL ? Component->TemplateName : NAME_None;

			if (ComponentName != NAME_None)
			{
				ValueStr += ComponentName.ToString();
			}
			else
			{
				UObject* StopOuter = NULL;
				if ( (PortFlags&PPF_SimpleObjectText) != 0 )
				{
					StopOuter = Parent->GetOutermost();
				}

				ValueStr += Component->GetPathName(StopOuter);
			}
		}
		else
		{
			ValueStr += TEXT("None");
		}
	}
	else
	{
#if USE_MASSIVE_LOD
		// for the particular case of the ReplacementPrimitive property with MassiveLOD, the text 
		// export/import won't work if they are exported without fully qualified names (they will
		// find the wrong actor's components), so for this property, we always use fully qualified names
		if (GetName() == TEXT("ReplacementPrimitive"))
		{
			PortFlags &= ~PPF_ExportsNotFullyQualified;
		}
#endif
		UObjectProperty::ExportTextItem(ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, TextFormat);
	}
}

UBOOL UComponentProperty::Identical(const void* A,const void* B, DWORD PortFlags) const
{
	if(Super::Identical(A,B,(PortFlags&(~PPF_DeltaComparison))))
	{
		return TRUE;
	}

	UComponent**	ComponentA = (UComponent**)A;
	UComponent**	ComponentB = (UComponent**)B;

	if(ComponentA && ComponentB && (*ComponentA) && (*ComponentB))
	{
		return AreComponentsIdentical(*ComponentA,*ComponentB,PortFlags);
	}
	else
	{
		return FALSE;
	}
}

const TCHAR* UComponentProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	// @todo nested components: To support nested components, this will have to change to allow taking a component as a parent, but still running this code
	UObject* TemplateOwner = NULL;
	for ( UObject* CheckOuter = Parent; CheckOuter; CheckOuter = CheckOuter->GetOuter() )
	{
		if ( CheckOuter->HasAnyFlags(RF_ClassDefaultObject) )
		{
			TemplateOwner = CheckOuter;
			break;
		}
	}

	// if TemplateOwner != NULL, Data is pointing to a class default object
	if(TemplateOwner)
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
			UClass*	OuterClass = TemplateOwner->GetClass();
			while(OuterClass)
			{
				//@fixme components - add some accessor functions to UObject for iterating components easily
				UComponent** ComponentDefaultObject = OuterClass->ComponentNameToDefaultObjectMap.Find(FName(*Temp, FNAME_Find, TRUE));

				if(ComponentDefaultObject && (*ComponentDefaultObject)->IsA(PropertyClass))
				{
					Result = *ComponentDefaultObject;
					break;
				}
				// we don't have owner classes, this would only be useful for nested components anyway
				// @todo nested components
				OuterClass = NULL;
			}

			check(!Result || Result->IsA(PropertyClass));

			*(UObject**)Data = Result;

			// If we couldn't find it or load it, we'll have to do without it.
			if( !Result )
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
		return UObjectProperty::ImportText(InBuffer,Data,PortFlags,Parent,ErrorText);
	}
}
UBOOL UComponentProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(UComponent**)Data != NULL;
}
void UComponentProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(UComponent**)Data = NULL;
}

UBOOL UComponentProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.ComponentValue = *(UComponent**)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UComponentProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(UComponent**)PropertyValueAddress = PropertyValue.ComponentValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UComponentProperty);

/*-----------------------------------------------------------------------------
	UInterfaceProperty.
-----------------------------------------------------------------------------*/
/**
 * Returns the text to use for exporting this property to header file.
 *
 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
 * @param	CPPExportFlags		flags for modifying the behavior of the export
 */
FString UInterfaceProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(InterfaceClass);

	UClass* ExportClass = InterfaceClass;
	while ( ExportClass && !ExportClass->HasAnyClassFlags(CLASS_Native) )
	{
		ExportClass = ExportClass->GetSuperClass();
	}
	check(ExportClass);
	check(ExportClass->HasAnyClassFlags(CLASS_Interface));

	ExtendedTypeText = FString::Printf(TEXT("I%s"), *ExportClass->GetName());
	return TEXT("TINTERFACE");
}

/**
 * Returns the text to use for exporting this property to header file.
 *
 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
 * @param	CPPExportFlags		flags for modifying the behavior of the export
 */
FString UInterfaceProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	checkSlow(InterfaceClass);

	if ( ExtendedTypeText != NULL )
	{
		UClass* ExportClass = InterfaceClass;
		while ( ExportClass && !ExportClass->HasAnyClassFlags(CLASS_Native) )
		{
			ExportClass = ExportClass->GetSuperClass();
		}
		check(ExportClass);
		check(ExportClass->HasAnyClassFlags(CLASS_Interface));

		*ExtendedTypeText = FString::Printf(TEXT("<class I%s>"), *ExportClass->GetName());
	}

	return TEXT("TScriptInterface");
}

void UInterfaceProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link(Ar, Prev);

	// we have a larger size than UObjectProperties
	ElementSize = sizeof(FScriptInterface);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());

	// for now, we won't support instancing of interface properties...it might be possible, but for the first pass we'll keep it simple
	PropertyFlags &= ~CPF_InterfaceClearMask;
}

UBOOL UInterfaceProperty::Identical( const void* A, const void* B, DWORD PortFlags/*=0*/ ) const
{
	FScriptInterface* InterfaceA = (FScriptInterface*)A;
	FScriptInterface* InterfaceB = (FScriptInterface*)B;

	if ( InterfaceB == NULL )
	{
		return InterfaceA->GetObject() == NULL;
	}

	return (InterfaceA->GetObject() == InterfaceB->GetObject() && InterfaceA->GetInterface() == InterfaceB->GetInterface());
}

void UInterfaceProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	FScriptInterface* InterfaceValue = (FScriptInterface*)Value;

	Ar << InterfaceValue->GetObjectRef();
	if ( Ar.IsLoading() || Ar.IsTransacting() )
	{
		if ( InterfaceValue->GetObject() != NULL )
		{
			InterfaceValue->SetInterface(InterfaceValue->GetObject()->GetInterfaceAddress(InterfaceClass));
		}
		else
		{
			InterfaceValue->SetInterface(NULL);
		}
	}
}

UBOOL UInterfaceProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	//@todo
	return FALSE;
}

void UInterfaceProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	FScriptInterface* InterfaceValue = (FScriptInterface*)PropertyValue;

	UObject* Temp = InterfaceValue->GetObject();
	if( Temp != NULL )
	{
		UBOOL bExportFullyQualified = true;

		// when exporting t3d, we don't want to qualify object names of objects in the level,
		// because it won't be able to find the object when importing (no more myLevel!)
		if ((PortFlags & PPF_ExportsNotFullyQualified) && Parent != NULL)
		{
			// get a pointer to the level by going up the outer chain
			UObject* LevelOuter = Parent->GetOutermost();
			// if the object we are pointing to is in the level, then don't full qualify it
			if (Temp->IsIn(LevelOuter))
			{
				bExportFullyQualified = false;
			}
		}

		// if we want a full qualified object reference, use the pathname, otherwise, use just the object name
		if (bExportFullyQualified)
		{
			UObject* StopOuter = NULL;
			if ( (PortFlags&PPF_SimpleObjectText) != 0 && Parent != NULL )
			{
				StopOuter = Parent->GetOutermost();
			}

			ValueStr += FString::Printf( TEXT("%s'%s'"), *Temp->GetClass()->GetName(), *Temp->GetPathName(StopOuter) );
		}
		else
		{
			ValueStr += FString::Printf( TEXT("%s'%s'"), *Temp->GetClass()->GetName(), *Temp->GetName() );
		}
	}
	else
	{
		ValueStr += TEXT("None");
	}
}

const TCHAR* UInterfaceProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
	{
		return NULL;
	}

	FScriptInterface* InterfaceValue = (FScriptInterface*)Data;
	UObject* ResolvedObject = InterfaceValue->GetObject();
	void* InterfaceAddress = InterfaceValue->GetInterface();

	const TCHAR* Buffer = InBuffer;
	if ( !UObjectProperty::ParseObjectPropertyValue(this, Parent, UObject::StaticClass(), PortFlags, Buffer, ResolvedObject) )
	{
		// we only need to call SetObject here - if ObjectAddress was not modified, then InterfaceValue should not be modified either
		// if it was set to NULL, SetObject will take care of clearing the interface address too
		InterfaceValue->SetObject(ResolvedObject);
		return NULL;
	}

	// so we should now have a valid object
	if ( ResolvedObject == NULL )
	{
		// if ParseObjectPropertyValue returned TRUE but ResolvedObject is NULL, the imported text was "None".  Make sure the interface pointer
		// is cleared, then stop
		InterfaceValue->SetObject(NULL);
		return Buffer;
	}

	void* NewInterfaceAddress = ResolvedObject->GetInterfaceAddress(InterfaceClass);
	if ( NewInterfaceAddress == NULL )
	{
		// the object we imported doesn't implement our interface class
		if ( ErrorText != NULL )
		{
			ErrorText->Logf(TEXT("%s: specified object doesn't implement the required interface class '%s': %s"),
				*GetFullName(), *InterfaceClass->GetName(), InBuffer);
		}
		else
		{
			warnf(NAME_Error, TEXT("%s: specified object doesn't implement the required interface class '%s': %s"),
				*GetFullName(), *InterfaceClass->GetName(), InBuffer);
		}

		return NULL;
	}

	InterfaceValue->SetObject(ResolvedObject);
	InterfaceValue->SetInterface(NewInterfaceAddress);
	return Buffer;
}

void UInterfaceProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(FScriptInterface*)Dest = *(FScriptInterface*)Src;
}

//@fixme - for now, just call through to the base version
void UInterfaceProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	Super::CopyCompleteValue(Dest,Src,SubobjectRoot,DestOwnerObject,InstanceGraph);
}

UBOOL UInterfaceProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return ((FScriptInterface*)Data)->GetInterface() != NULL;
}

void UInterfaceProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}

	FScriptInterface* InterfaceValue = (FScriptInterface*)Data;
	InterfaceValue->SetObject(NULL);
}

INT UInterfaceProperty::GetMinAlignment() const
{
#if _WIN64
	return __alignof(UObject);
#else
	return sizeof(UObject*);
#endif
}

UBOOL UInterfaceProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.InterfaceValue = (FScriptInterface*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}

UBOOL UInterfaceProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(FScriptInterface*)PropertyValueAddress = *PropertyValue.InterfaceValue;
		bResult = TRUE;
	}
	return bResult;
}

/* === UObject interface. === */
/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UInterfaceProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UInterfaceProperty, InterfaceClass ) );
}

/** Manipulates the data referenced by this UProperty */
void UInterfaceProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << InterfaceClass;
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		checkSlow(InterfaceClass);
	}
}
IMPLEMENT_CLASS(UInterfaceProperty);

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

INT UNameProperty::GetMinAlignment() const
{
	return 4;
	// @GEMINI_TODO FNAME: Use this when the first param of FName is a TCHAR*
//	return sizeof(TCHAR*);
}
void UNameProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );
	ElementSize = sizeof(FName);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
}
void UNameProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(FName*)Dest = *(FName*)Src;
}
void UNameProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if( ArrayDim==1 )
		*(FName*)Dest = *(FName*)Src;
	else
		for( INT i=0; i<ArrayDim; i++ )
			((FName*)Dest)[i] = ((FName*)Src)[i];
}
UBOOL UNameProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return *(FName*)A == (B ? *(FName*)B : FName(NAME_None));
}
void UNameProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	Ar << *(FName*)Value;
}
FString UNameProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("FName");
}
FString UNameProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("NAME");
}
void UNameProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	FName Temp = *(FName*)PropertyValue;
	if( !(PortFlags & PPF_Delimited) )
		ValueStr += Temp.ToString();
	else if ( HasValue(PropertyValue) )
	{
		ValueStr += FString::Printf( TEXT("\"%s\""), *Temp.ToString() );
	}
}
const TCHAR* UNameProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	FString Temp;
	Buffer = ReadToken( Buffer, Temp );
	if( !Buffer )
		return NULL;

	*(FName*)Data = FName(*Temp, FNAME_Add, TRUE);
	return Buffer;
}
UBOOL UNameProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return *(FName*)Data != NAME_None;
}

void UNameProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	*(FName*)Data = NAME_None;
}

UBOOL UNameProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.NameValue = (FName*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UNameProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(FName*)PropertyValueAddress = *PropertyValue.NameValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UNameProperty);

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

INT UStrProperty::GetMinAlignment() const
{
#if _WIN64
	return __alignof(UObject);
#else
	return sizeof(void*);
#endif
}
void UStrProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link(Ar, Prev);
	ElementSize = sizeof(FString);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	if (!(PropertyFlags & CPF_Native))
	{
		PropertyFlags |= CPF_NeedCtorLink;
	}
}
UBOOL UStrProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return appStricmp( **(const FString*)A, B ? **(const FString*)B : TEXT("") )==0;
}
void UStrProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	Ar << *(FString*)Value;
}
void UStrProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
}
FString UStrProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	return TEXT("FString");
}
FString UStrProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	return TEXT("STR");
}
void UStrProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
	if( !(PortFlags & PPF_Delimited) )
	{
		ValueStr += **(FString*)PropertyValue;
	}
	else if ( HasValue(PropertyValue) )
	{
		FString& StringValue = *(FString*)PropertyValue;
		ValueStr += FString::Printf( TEXT("\"%s\""), *(StringValue.ReplaceCharWithEscapedChar()) );
	}
}
const TCHAR* UStrProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	if( !(PortFlags & PPF_Delimited) )
	{
		*(FString*)Data = Buffer;

		// in order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += appStrlen(Buffer);
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
void UStrProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	*(FString*)Dest = *(FString*)Src;
}
void UStrProperty::DestroyValue( void* Dest ) const
{
	for( INT i=0; i<ArrayDim; i++ )
		(*(FString*)((BYTE*)Dest+i*ElementSize)).~FString();
}
UBOOL UStrProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return FALSE;
	}
	return (*(FString*)Data).Len() > 0;
}

void UStrProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 && !IsLocalized() )
	{
		return;
	}
	(*(FString*)Data).Empty();
}

UBOOL UStrProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.StringValue = (FString*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UStrProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(FString*)PropertyValueAddress = *PropertyValue.StringValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UStrProperty);

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UArrayProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UArrayProperty, Inner ) );

	// Ensure that TArray and FScriptArray are interchangeable, as FScriptArray will be used to access a native array property
	// from script that is declared as a TArray in C++.
	checkAtCompileTime(sizeof(FScriptArray) == sizeof(TArray<BYTE>),FScriptArrayAndTArrayMustBeInterchangable);
}
INT UArrayProperty::GetMinAlignment() const
{
#if _WIN64
	return __alignof(UObject);
#else
	return sizeof(void*);
#endif
}
void UArrayProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link(Ar, Prev);
	Ar.Preload(Inner);
	Inner->Link(Ar, NULL);
	ElementSize = sizeof(FScriptArray);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	if (!HasAnyPropertyFlags(CPF_Native))
	{
		PropertyFlags |= CPF_NeedCtorLink;
	}
}
UBOOL UArrayProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	checkSlow(Inner);

	FScriptArray* ArrayA = (FScriptArray*)A;
	FScriptArray* ArrayB = (FScriptArray*)B;

	const INT ArrayNum = ArrayA->Num();
	if ( ArrayNum != (ArrayB ? ArrayB->Num() : 0) )
	{
		return FALSE;
	}
// 	INT n = ((FScriptArray*)A)->Num();
// 	if( n!=(B ? ((FScriptArray*)B)->Num() : 0) )
// 		return 0;
// 	INT   c = Inner->ElementSize;
// 	BYTE* p = (BYTE*)((FScriptArray*)A)->GetData();
// 	if( B )
// 	{
// 		BYTE* q = (BYTE*)((FScriptArray*)B)->GetData();
// 		for( INT i=0; i<n; i++ )
// 			if( !Inner->Identical( p+i*c, q+i*c, PortFlags ) )
// 				return 0;
// 	}
// 	else
// 	{
// 		for( INT i=0; i<n; i++ )
// 			if( !Inner->Identical( p+i*c, 0, PortFlags ) )
// 				return 0;
// 	}

	const INT InnerSize = Inner->ElementSize;
	const BYTE* const ArrayAData = (BYTE*)ArrayA->GetData();
	if ( ArrayB != NULL )
	{
		const BYTE* const ArrayBData = (BYTE*)ArrayB->GetData();
		for ( INT ArrayIndex = 0; ArrayIndex < ArrayNum; ArrayIndex++ )
		{
			if ( !Inner->Identical( ArrayAData + ArrayIndex * InnerSize, ArrayBData + ArrayIndex * InnerSize, PortFlags) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		for ( INT ArrayIndex = 0; ArrayIndex < ArrayNum; ArrayIndex++ )
		{
			if ( !Inner->Identical( ArrayAData + ArrayIndex * InnerSize, 0, PortFlags) )
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}
void UArrayProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	checkSlow(Inner);

	FScriptArray* Array	= (FScriptArray*)Value;
	INT		c		= Inner->ElementSize;
	INT		n		= Array->Num();
	Ar << n;

	if( Ar.IsLoading() )
	{
		// need to use DestroyValue() if Inner needs destruction to prevent memory leak
		if (Inner->PropertyFlags & CPF_NeedCtorLink)
		{
			DestroyValue(Value);
		}
		Array->Empty( n, c );
		Array->AddZeroed( n, c );
	}
	BYTE* p = (BYTE*)Array->GetData();
	Array->CountBytes( Ar, Inner->ElementSize );
	for( INT i=0; i<n; i++ )
	{
		Inner->SerializeItem( Ar, p+i*c, MaxReadBytes>0?MaxReadBytes/n:0 );
	}
}
UBOOL UArrayProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	return 1;
}
void UArrayProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Inner;
	checkSlow(Inner||HasAnyFlags(RF_ClassDefaultObject));
}
FString UArrayProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	checkSlow(Inner);

	if ( ExtendedTypeText != NULL )
	{
		FString InnerExtendedTypeText;
		FString InnerTypeText = Inner->GetCPPType(&InnerExtendedTypeText, CPPExportFlags);
		if ( InnerExtendedTypeText.Len() && InnerExtendedTypeText.Right(1) == TEXT(">") )
		{
			// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
			InnerExtendedTypeText += TEXT(" ");
		}
		*ExtendedTypeText = FString::Printf(TEXT("<%s%s>"), *InnerTypeText, *InnerExtendedTypeText);
	}
	return TEXT("TArray");
}
FString UArrayProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(Inner);
	FString InnerExtendedText;
	ExtendedTypeText = Inner->GetCPPType(&InnerExtendedText);
	ExtendedTypeText += InnerExtendedText;
	return TEXT("TARRAY");
}
void UArrayProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat ) const
{
	checkSlow(Inner);

	TCHAR ArrayStartChar = TextFormat == ITF_Unreal ? TEXT('(') : TEXT('[');
	TCHAR ArrayEndChar = TextFormat == ITF_Unreal ? TEXT(')') : TEXT(']');
	FScriptArray* Array       = (FScriptArray*)PropertyValue;
	FScriptArray* Default     = (FScriptArray*)DefaultValue;
	INT     ElementSize = Inner->ElementSize;

	BYTE* StructDefaults = NULL;
	if ( Cast<UStructProperty>(Inner,CLASS_IsAUStructProperty) != NULL )
	{
		UScriptStruct* InnerStruct = static_cast<UStructProperty*>(Inner)->Struct;
		checkSlow(InnerStruct);
		StructDefaults = InnerStruct->GetDefaults();
	}

	INT Count = 0;
	for( INT i=0; i<Array->Num(); i++ )
	{
		if ( ++Count == 1 )
		{
			ValueStr += ArrayStartChar;
		}
		else
		{
			ValueStr += TCHAR(',');
		}

		BYTE* PropData = (BYTE*)Array->GetData() + i * ElementSize;
		BYTE* PropDefault = (Default && Default->Num() > i)
			? (BYTE*)Default->GetData() + i * ElementSize
			: StructDefaults;

		// Do not re-export duplicate data from superclass when exporting to .int file
		if ( (PortFlags & PPF_LocalizedOnly) != 0 && Inner->Identical(PropData, PropDefault) )
		{
			continue;
		}

		Inner->ExportTextItem( ValueStr, PropData, PropDefault, Parent, PortFlags|PPF_Delimited, TextFormat );
	}

	if ( Count > 0 )
	{
		ValueStr += ArrayEndChar;
	}
}

const TCHAR* UArrayProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat TextFormat ) const
{
	TCHAR ArrayStartChar = TEXT('(');
	TCHAR ArrayEndChar = TEXT(')');
	if (TextFormat == ITF_JSON)
	{
		ArrayStartChar = TEXT('[');
		ArrayEndChar = TEXT(']');
	}

	checkSlow(Inner);

	if ( !ValidateImportFlags(PortFlags,ErrorText) || Buffer == NULL || *Buffer++ != ArrayStartChar )
	{
		return NULL;
	}

	FScriptArray* Array       = (FScriptArray*)Data;
	INT     ElementSize = Inner->ElementSize;

	// only clear the array if we're not importing localized text
	if ( (PortFlags&PPF_LocalizedOnly) == 0 )
	{
		// need to use DestroyValue() if Inner needs destruction to prevent memory leak
		if (Inner->PropertyFlags & CPF_NeedCtorLink)
		{
			DestroyValue(Array);
		}
		else
		{
			Array->Empty(0,ElementSize);
		}
	}

	SkipWhitespace(Buffer);

	BYTE* StructDefaults = NULL;
	if ( Cast<UStructProperty>(Inner,CLASS_IsAUStructProperty) != NULL )
	{
		StructDefaults = static_cast<UStructProperty*>(Inner)->Struct->GetDefaults();
	}

	INT Index = 0;
	while( *Buffer != ArrayEndChar )
	{
		// advance past empty elements
		while ( *Buffer == TCHAR(',') )
		{
			Buffer++;
			if ( Index >= Array->Num() )
			{
				Array->Add( 1, ElementSize );
				appMemzero( (BYTE*)Array->GetData() + Index * ElementSize, ElementSize );

				if ( StructDefaults != NULL )
				{
					checkSlow(Cast<UStructProperty>(Inner,CLASS_IsAUStructProperty) != NULL);
					static_cast<UStructProperty*>(Inner)->InitializeValue((BYTE*)Array->GetData() + Index * ElementSize);
				}
			}

			Index++;

			if ( *Buffer == ArrayEndChar )
			{
				// remove any additional elements from the array
				//				// this was only required when localized values could be specified in defaultproperties
				// 				if ( (PortFlags&PPF_LocalizedOnly) != 0 && Index < Array->Num() )
				// 				{
				// 					Array->Remove( Index, Array->Num() - Index, ElementSize );
				// 				}

				Buffer++;
				return Buffer;
			}
		}

		if ( Index >= Array->Num() )
		{
			Array->Add( 1, ElementSize );
			appMemzero( (BYTE*)Array->GetData() + Index * ElementSize, ElementSize );

			if ( StructDefaults != NULL )
			{
				checkSlow(Cast<UStructProperty>(Inner,CLASS_IsAUStructProperty) != NULL);
				static_cast<UStructProperty*>(Inner)->InitializeValue((BYTE*)Array->GetData() + Index * ElementSize);
			}
		}

		Buffer = Inner->ImportText( Buffer, (BYTE*)Array->GetData() + Index*ElementSize, PortFlags|PPF_Delimited, Parent, ErrorText, TextFormat );
		Index++;

		if( Buffer == NULL )
		{
			return NULL;
		}

		SkipWhitespace(Buffer);

		// if the next character isn't the element delimiter, stop processing the text
		if( *Buffer != TCHAR(',') )
		{
			break;
		}
		Buffer++;
		SkipWhitespace(Buffer);
	}

	if( *Buffer++ != ArrayEndChar )
	{
		return NULL;
	}

	// remove any additional elements from the array
	//	// this was only required when localized values could be specified in defaultproperties
	// 	if ( (PortFlags&PPF_LocalizedOnly) != 0 && Index < Array->Num() )
	// 	{
	// 		Array->Remove( Index, Array->Num() - Index, ElementSize );
	// 	}

	return Buffer;
}
void UArrayProperty::AddCppProperty( UProperty* Property )
{
	check(!Inner);
	check(Property);

	Inner = Property;
}

/**
 * Instances any UObjectProperty values that still match the default value.
 *
 * @param	Value				the address where the pointer to the instanced object should be stored.  This should always correspond to the BASE + OFFSET, where
 *									BASE = (for class member properties) the address of the UObject which contains this data, (for script struct member properties) the
 *										address of the struct's data
 *									OFFSET = the Offset of this UProperty from base
 * @param	DefaultValue		the address where the pointer to the default value is stored.  Evaluated the same way as Value
 * @param	OwnerObject			the object that contains the destination data.  Will be the used as the Outer for any newly instanced subobjects.
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UArrayProperty::InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	checkSlow(Value);

	if ( Inner->ContainsInstancedObjectProperty() && DefaultValue )
	{
		for ( INT StaticArrayIndex = 0; StaticArrayIndex < ArrayDim; StaticArrayIndex++ )
		{
			FScriptArray* ValueArray   = (FScriptArray*) ((BYTE*)Value + StaticArrayIndex * ElementSize);
			FScriptArray* DefaultArray = (FScriptArray*) ((BYTE*)DefaultValue + StaticArrayIndex * ElementSize);
			INT Size             = Inner->ElementSize;

			for ( INT ArrayIndex = 0; ArrayIndex < ValueArray->Num() && ArrayIndex < DefaultArray->Num(); ArrayIndex++ )
			{
				BYTE* InnerValue = (BYTE*)ValueArray->GetData() + ArrayIndex * Size;
				BYTE* InnerDefaultValue = (BYTE*)DefaultArray->GetData() + ArrayIndex * Size;
				Inner->InstanceSubobjects(InnerValue, InnerDefaultValue, OwnerObject, InstanceGraph);
			}
		}
	}
}
void UArrayProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	// don't do anything if the pointers are the same (no point, and otherwise they'll incorrectly get emptied)
	if (Src != Dest)
	{
		FScriptArray* SrcArray  = (FScriptArray*)Src;
		FScriptArray* DestArray = (FScriptArray*)Dest;
		INT     Size      = Inner->ElementSize;
		// need to use DestroyValue() if Inner needs destruction to prevent memory leak
		if (Inner->PropertyFlags & CPF_NeedCtorLink)
		{
			DestroyValue(Dest);
		}
		DestArray->Empty(SrcArray->Num(),Size);

		if( Inner->PropertyFlags & CPF_NeedCtorLink )
		{
			// Copy all the elements.
			DestArray->AddZeroed( SrcArray->Num(), Size );
			BYTE* SrcData  = (BYTE*)SrcArray->GetData();
			BYTE* DestData = (BYTE*)DestArray->GetData();
			for( INT i=0; i<DestArray->Num(); i++ )
				Inner->CopyCompleteValue( DestData+i*Size, SrcData+i*Size, SubobjectRoot, DestOwnerObject, InstanceGraph );
		}
		else if ( SrcArray->Num() )
		{
			// Copy all the elements.
			DestArray->Add( SrcArray->Num(), Size );
			appMemcpy( DestArray->GetData(), SrcArray->GetData(), SrcArray->Num()*Size );
		}
	}
}
void UArrayProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	UBOOL bClearArrayValue = TRUE;
	
	// if we only want to clear localized values and this property isn't localized, don't clear the value
	if ( (PortFlags&PPF_LocalizedOnly) != 0 )
	{
		if ( !IsLocalized() )
		{
			return;
		}


		// if this is an array of structs, the struct may contain members which are not localized - in this case, we won't clear the array...we'll just clear
		// the values for the properties which are marked localized
		UStructProperty* InnerStructProp = ExactCast<UStructProperty>(Inner);
		if ( InnerStructProp != NULL )
		{
			FScriptArray* Array = (FScriptArray*)Data;
			BYTE* ArrayData = (BYTE*)Array->GetData();
			for ( INT ArrayIndex = 0; ArrayIndex < Array->Num(); ArrayIndex++ )
			{
				BYTE* ElementData = ArrayData + (ArrayIndex * Inner->ElementSize);
				InnerStructProp->ClearValue(ElementData, PortFlags);

				// if the struct has values for any non-localized properties, don't clear the array
				if ( InnerStructProp->HasValue(ElementData, (PortFlags&~PPF_LocalizedOnly)) )
				{
					bClearArrayValue = FALSE;

					// don't break out as we want to clear the localized values from all structs in this array
				}
			}
		}
	}
	
	if ( bClearArrayValue == TRUE )
	{
		//need to use DestroyValue() if Inner needs destruction to prevent memory leak
		if (Inner->PropertyFlags & CPF_NeedCtorLink)
		{
			//@todo: what effect does calling the FScriptArray destructor have on our Data?
			DestroyValue(Data);
		}
		else
		{
			(*(FScriptArray*)Data).Empty(0,ElementSize);
		}
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
	FScriptArray* DestArray = (FScriptArray*)Dest;
	if( Inner->PropertyFlags & CPF_NeedCtorLink )
	{
		BYTE* DestData = (BYTE*)DestArray->GetData();
		INT   Size     = Inner->ElementSize;
		for( INT i=0; i<DestArray->Num(); i++ )
		{
			Inner->DestroyValue( DestData+i*Size );
		}
	}
	DestArray->~FScriptArray();
}

UBOOL UArrayProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	const FScriptArray* Array = (FScriptArray*)Data;
	UBOOL bHasValue = Array->Num() > 0;
	if ( bHasValue && (PortFlags&PPF_LocalizedOnly) != 0 )
	{
		// if the caller wants to know if we have values for our localized properties only, check each individual element if this is an array of structs
		UBOOL bHasLocalizedValues = FALSE;
		UStructProperty* InnerStructProp = ExactCast<UStructProperty>(Inner);
		if ( InnerStructProp != NULL )
		{
			const BYTE* ArrayData = (const BYTE*)Array->GetData();
			for ( INT ArrayIndex = 0; ArrayIndex < Array->Num(); ArrayIndex++ )
			{
				const BYTE* ElementData = ArrayData + (ArrayIndex * Inner->ElementSize);
				if ( InnerStructProp->HasValue(ElementData, PortFlags) )
				{
					bHasLocalizedValues = TRUE;
					break;
				}
			}
		}
		else
		{
			bHasLocalizedValues = IsLocalized();
		}

		bHasValue = bHasLocalizedValues;
	}

	return bHasValue;
}
UBOOL UArrayProperty::IsLocalized() const
{
	if ( Inner->IsLocalized() )
		return TRUE;

	return Super::IsLocalized();
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
 */
void UArrayProperty::InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	if( (PropertyFlags & CPF_Native) == 0 )
	{
		BYTE* ArrayData = (BYTE*)((FScriptArray*)Data)->GetData();
		BYTE* DefaultArrayData = DefaultData ? (BYTE*)((FScriptArray*)DefaultData)->GetData() : NULL;

		if( (Inner->PropertyFlags&CPF_Component) != 0 && ArrayData != NULL )
		{
			for( INT ElementIndex = 0; ElementIndex < ((FScriptArray*)Data)->Num(); ElementIndex++ )
			{
				BYTE* DefaultValue = (DefaultArrayData && ElementIndex < ((FScriptArray*)DefaultData)->Num()) ? DefaultArrayData + ElementIndex * Inner->ElementSize : NULL;
				Inner->InstanceComponents( ArrayData + ElementIndex * Inner->ElementSize, DefaultValue, Owner, InstanceGraph );
			}
		}
	}
}

UBOOL UArrayProperty::GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		out_PropertyValue.ArrayValue = (FScriptArray*)PropertyValueAddress;
		bResult = TRUE;
	}
	return bResult;
}
UBOOL UArrayProperty::SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
{
	UBOOL bResult = FALSE;
	if ( PropertyValueAddress != NULL )
	{
		*(FScriptArray*)PropertyValueAddress = *PropertyValue.ArrayValue;
		bResult = TRUE;
	}
	return bResult;
}

IMPLEMENT_CLASS(UArrayProperty);

/*-----------------------------------------------------------------------------
	UMapProperty.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UMapProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UMapProperty, Key ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UMapProperty, Value ) );
}
INT UMapProperty::GetMinAlignment() const
{
	// the minimum alignment for a TMap is the size of the largest member of TMap
#if _WIN64
	return __alignof(UObject);
#else
	return sizeof(void*);
#endif
}

void UMapProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link( Ar, Prev );

#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);

	Ar.Preload( Key );
	Key->Link( Ar, NULL );
	Ar.Preload( Value );
	Value->Link( Ar, NULL );
#endif

	ElementSize = sizeof(TMap<BYTE,BYTE>);
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	if (!(PropertyFlags & CPF_Native))
	{
		PropertyFlags |= CPF_NeedCtorLink;
	}
}

UBOOL UMapProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
#endif

	/*
	INT n = ((FScriptArray*)A)->Num();
	if( n!=(B ? ((FScriptArray*)B)->Num() : 0) )
		return 0;
	INT   c = Inner->ElementSize;
	BYTE* p = (BYTE*)((FScriptArray*)A)->GetData();
	if( B )
	{
		BYTE* q = (BYTE*)((FScriptArray*)B)->GetData();
		for( INT i=0; i<n; i++ )
			if( !Inner->Identical( p+i*c, q+i*c, PortFlags ) )
				return 0;
	}
	else
	{
		for( INT i=0; i<n; i++ )
			if( !Inner->Identical( p+i*c, 0, PortFlags ) )
				return 0;
	}
	*/

	return 1;
}

void UMapProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
#endif

	/*
	INT   c = Inner->ElementSize;
	INT   n = ((FScriptArray*)Value)->Num();
	Ar << n;
	if( Ar.IsLoading() )
	{
		((FScriptArray*)Value)->Empty( n, c );
		((FScriptArray*)Value)->Add( n, c );
	}
	BYTE* p = (BYTE*)((FScriptArray*)Value)->GetData();
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
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
#endif
}

FString UMapProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
#endif

	if ( ExtendedTypeText != NULL )
	{
#if TMAPS_IMPLEMENTED
		*ExtendedTypeText = FString::Printf(TEXT("< %s, %s >"), *Key->GetCPPType(NULL,CPPExportFlags), *Value->GetCPPType(NULL,CPPExportFlags));
#else
		*ExtendedTypeText = TEXT("< fixme >");
#endif
	}

	return TEXT("TMap");
}

FString UMapProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
#if !TMAPS_IMPLEMENTED
	appErrorf(TEXT("No configured CPPMacroType for maps!"));
#endif

	return TEXT("");
}

void UMapProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat ) const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
#endif
	/*
	*ValueStr++ = '(';
	FScriptArray* Array       = (FScriptArray*)PropertyValue;
	FScriptArray* Default     = (FScriptArray*)DefaultValue;
	INT     ElementSize = Inner->ElementSize;
	for( INT i=0; i<Array->Num(); i++ )
	{
		if( i>0 )
			*ValueStr++ = ',';
		Inner->ExportTextItem( ValueStr, (BYTE*)Array->GetData() + i*ElementSize, Default ? (BYTE*)Default->GetData() + i*ElementSize : 0, PortFlags|PPF_Delimited, TextFormat );
		ValueStr += appStrlen(ValueStr);
	}
	*ValueStr++ = ')';
	*ValueStr++ = 0;
	*/
}

const TCHAR* UMapProperty::ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat ) const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;
	/*
	if( *Buffer++ != '(' )
		return NULL;
	FScriptArray* Array       = (FScriptArray*)Data;
	INT     ElementSize = Inner->ElementSize;
	Array->Empty( 0, ElementSize );
	while( *Buffer != ')' )
	{
		INT Index = Array->Add( 1, ElementSize );
		appMemzero( (BYTE*)Array->GetData() + Index*ElementSize, ElementSize );
		Buffer = Inner->ImportText( Buffer, (BYTE*)Array->GetData() + Index*ElementSize, PortFlags|PPF_Delimited, Parent,ErrorText );
		if( !Buffer )
			return NULL;
		if( *Buffer!=',' )
			break;
		Buffer++;
	}
	if( *Buffer++ != ')' )
		return NULL;
	*/
#endif
	return Buffer;
}

/**
 * Instances any UObjectProperty values that still match the default value.
 *
 * @param	Value				the address where the pointer to the instanced object should be stored.  This should always correspond to the BASE + OFFSET, where
 *									BASE = (for class member properties) the address of the UObject which contains this data, (for script struct member properties) the
 *										address of the struct's data
 *									OFFSET = the Offset of this UProperty from base
 * @param	DefaultValue		the address where the pointer to the default value is stored.  Evaluated the same way as Value
 * @param	OwnerObject			the object that contains the destination data.  Will be the used as the Outer for any newly instanced subobjects.
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UMapProperty::InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
}

void UMapProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
}


void UMapProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
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
		DestArray->AddZeroed( SrcArray->Num(), Size );
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
	FScriptArray* DestArray = (FScriptArray*)Dest;
	if( Inner->PropertyFlags & CPF_NeedsCtorLink )
	{
		BYTE* DestData = (BYTE*)DestArray->GetData();
		INT   Size     = Inner->ElementSize;
		for( INT i=0; i<DestArray->Num(); i++ )
			Inner->DestroyValue( DestData+i*Size );
	}
	DestArray->~FScriptArray();
	*/
}

UBOOL UMapProperty::IsLocalized() const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);

	return Key->IsLocalized() || Value->IsLocalized();
#else
	return FALSE;
#endif
}


UBOOL UMapProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
#if TMAPS_IMPLEMENTED
	checkSlow(Key);
	checkSlow(Value);
#endif

	return FALSE;
}

void UMapProperty::AddCppProperty( UProperty* Property )
{
}

IMPLEMENT_CLASS(UMapProperty);

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UStructProperty::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UStructProperty, Struct ) );
}
INT UStructProperty::GetMinAlignment() const
{
	return Struct->GetMinAlignment();
}
void UStructProperty::Link( FArchive& Ar, UProperty* Prev )
{
	Super::Link(Ar, Prev);

	// Preload is required here in order to load the value of Struct->PropertiesSize
	Ar.Preload(Struct);
	ElementSize = Align(Struct->PropertiesSize, GetMinAlignment());
	Offset = Align((GetOuter()->GetClass()->ClassCastFlags & CASTCLASS_UStruct) ? ((UStruct*)GetOuter())->GetPropertiesSize() : 0, GetMinAlignment());
	
	if (Struct->ConstructorLink && !(PropertyFlags & CPF_Native))
	{
		PropertyFlags |= CPF_NeedCtorLink;
	}
}
UBOOL UStructProperty::Identical( const void* A, const void* B, DWORD PortFlags ) const
{
	return Struct->StructCompare(A, B, PortFlags);
}
void UStructProperty::SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const
{
	UBOOL bUseBinarySerialization =	!(Ar.IsLoading() || Ar.IsSaving()) 
								||	Ar.WantBinaryPropertySerialization()
								||  ((Struct->StructFlags & STRUCT_ImmutableWhenCooked) != 0 && (Ar.ContainsCookedData() || (GIsCooking && Ar.IsSaving())))
								||	((Struct->StructFlags & STRUCT_Immutable) != 0
								|| (Ar.GetPortFlags() & PPF_ForceBinarySerialization)
									// when the min package version is bumped, the remainder of this check can be removed
									&&	(Struct->GetFName() != NAME_FontCharacter || Ar.Ver() >= VER_FIXED_FONTS_SERIALIZATION));

	// Preload struct before serialization tracking to not double count time.
	if ( bUseBinarySerialization == TRUE )
	{
		Ar.Preload( Struct );
	}

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
	ULinker*	LinkerAr			= Ar.GetLinker();
	DOUBLE		PreviousLocalTime	= 0;
	DOUBLE		PreviousGlobalTime	= 0;
	DOUBLE		StartTime			= 0;
	if( LinkerAr )
	{
		PreviousLocalTime = LinkerAr->SerializationPerfTracker.GetTotalEventTime(Struct);
		PreviousGlobalTime = GObjectSerializationPerfTracker->GetTotalEventTime(Struct);
		StartTime = appSeconds();
	}
#endif

	if( bUseBinarySerialization )
	{
		// Struct is already preloaded above.

		if ( !Ar.IsPersistent() && Ar.GetPortFlags() != 0 && !Struct->ShouldSerializeAtomically(Ar) )
		{
			Struct->SerializeBinEx( Ar, (BYTE*)Value, (BYTE*)Defaults, Struct->GetPropertiesSize() );
		}
		else
		{
			Struct->SerializeBin( Ar, (BYTE*)Value, MaxReadBytes );
		}
	}
	else
	{
		Struct->SerializeTaggedProperties( Ar, (BYTE*)Value, Struct, (BYTE*)Defaults );
	}

#if PERF_TRACK_SERIALIZATION_PERFORMANCE || LOOKING_FOR_PERF_ISSUES
	if( LinkerAr )
	{
		DOUBLE TimeSpent = (appSeconds() - StartTime) * 1000;
		// add the data to the linker-specific tracker
		LinkerAr->SerializationPerfTracker.TrackEvent(Struct, PreviousLocalTime, TimeSpent);
		// add the data to the global tracker
		GObjectSerializationPerfTracker->TrackEvent(Struct, PreviousGlobalTime, TimeSpent);
	}
#endif
}

UBOOL UStructProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const
{
	if( Struct->GetFName()==NAME_Vector )
	{
		FVector& V = *(FVector*)Data;
		V.SerializeCompressed( Ar );
	}
	else if( Struct->GetFName()==NAME_Rotator )
	{
		FRotator& R = *(FRotator*)Data;
		R.SerializeCompressed( Ar );
	}
	else if (Struct->GetFName() == NAME_Quat)
	{
		FQuat Q = *(FQuat*)Data;

		if (Ar.IsSaving())
		{
			// Make sure we have a non null SquareSum. It shouldn't happen with a quaternion, but better be safe.
			if(Q.SizeSquared() <= SMALL_NUMBER)
			{
				Q = FQuat::Identity;
			}
			else
			{
				// All transmitted quaternions *MUST BE* unit quaternions, in which case we can deduce the value of W.
				Q.Normalize();
				// force W component to be non-negative
				if (Q.W < 0.f)
				{
					Q.X *= -1.f;
					Q.Y *= -1.f;
					Q.Z *= -1.f;
					Q.W *= -1.f;
				}
			}
		}

		Ar << Q.X << Q.Y << Q.Z;
		if ( Ar.IsLoading() )
		{
			const FLOAT XYZMagSquared = (Q.X*Q.X + Q.Y*Q.Y + Q.Z*Q.Z);
			const FLOAT WSquared = 1.0f - XYZMagSquared;
			// If mag of (X,Y,Z) <= 1.0, then we calculate W to make magnitude of Q 1.0
			if (WSquared >= 0.f)
			{
				Q.W = appSqrt(WSquared);
			}
			// If mag of (X,Y,Z) > 1.0, we set W to zero, and then renormalize 
			else
			{
				Q.W = 0.f;
				
				const FLOAT XYZInvMag = appInvSqrt(XYZMagSquared);
				Q.X *= XYZInvMag;
				Q.Y *= XYZInvMag;
				Q.Z *= XYZInvMag;
			}
			*(FQuat*)Data = Q;
		}
	}
	else if( Struct->GetFName()==NAME_Plane )
	{
		FPlane& P = *(FPlane*)Data;
		SWORD X(appRound(P.X)), Y(appRound(P.Y)), Z(appRound(P.Z)), W(appRound(P.W));
		Ar << X << Y << Z << W;
		if( Ar.IsLoading() )
			P = FPlane(X,Y,Z,W);
	}
	else if (Struct->GetFName() == NAME_UniqueNetId)
	{
		// This struct only contains a QWORD
		QWORD& Qword = *(QWORD*)Data;
		Ar << Qword;
	}
	else
	{
		UBOOL bMapped = TRUE;
		for( TFieldIterator<UProperty> It(Struct); It; ++It )
		{
			if( Map->SupportsObject(*It) )
			{
				for( INT i=0; i<It->ArrayDim; i++ )
				{
					bMapped = It->NetSerializeItem( Ar, Map, (BYTE*)Data+It->Offset+i*It->ElementSize ) && bMapped;
				}
			}
		}

		// pretend we succeeded if this property doesn't want retries when replication fails to serialize something
		return (bMapped || !(PropertyFlags & CPF_RepRetry));
	}
	return 1;
}
void UStructProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Struct;
}
FString UStructProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, DWORD CPPExportFlags/*=0*/ ) const
{
	UBOOL bExportForwardDeclaration = 
		(CPPExportFlags&CPPF_OptionalValue) == 0 &&
		!(Struct->GetOwnerClass()->HasAnyClassFlags(CLASS_NoExport) || (Struct->StructFlags&STRUCT_Native) == 0);

	return FString::Printf( TEXT("%sF%s"), 
		bExportForwardDeclaration ? TEXT("struct ") : TEXT(""),
		*Struct->GetName() );
}
FString UStructProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	ExtendedTypeText = GetCPPType();
	return TEXT("STRUCT");
}

void UStructProperty_ExportTextItem(class UScriptStruct* InStruct, FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat)
{
	INT Count=0;

	TCHAR StructStartChar = TextFormat == ITF_Unreal ? TEXT('(') : TEXT('{');
	TCHAR StructEndChar = TextFormat == ITF_Unreal ? TEXT(')') : TEXT('}');
	TCHAR StructDelimiterChar = TextFormat == ITF_Unreal ? TEXT('=') : TEXT(':');
	// if this struct is configured to be serialized as a unit, it must be exported as a unit as well. This codepath does not need to handle
	// STRUCT_AtomicWhenCooked as the archive used to serialize it with won't have ContainsCookedData() return TRUE.
	if ((InStruct->StructFlags&STRUCT_Atomic) != 0)
	{
		// change DefaultValue to match PropertyValue so that ExportText always exports this item
		DefaultValue = PropertyValue;
	}

	const UBOOL bConfigPropertiesOnly = (PortFlags&PPF_ConfigOnly) != 0 && (InStruct->StructFlags&STRUCT_StrictConfig) != 0;
	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		if (It->Port(PortFlags) &&	
			(!bConfigPropertiesOnly || It->HasAnyPropertyFlags(CPF_Config)))
		{
			for (INT Index=0; Index<It->ArrayDim; Index++)
			{
				FString InnerValue;
				if (It->ExportText(Index,InnerValue,PropertyValue,DefaultValue,Parent,PPF_Delimited | PortFlags, TextFormat))
				{
					Count++;
					if ( Count == 1 )
					{
						ValueStr += StructStartChar;
					}
					else
					{
						ValueStr += TEXT(",");
					}

					if( It->ArrayDim == 1 )
					{
						ValueStr += FString::Printf( TEXT("%s%c"), *It->GetName(),StructDelimiterChar );
					}
					else
					{
						ValueStr += FString::Printf( TEXT("%s[%i]%c"), *It->GetName(), Index,StructDelimiterChar );
					}
					ValueStr += InnerValue;
				}
			}
		}
	}

	if (Count > 0)
	{
		ValueStr += StructEndChar;
	}
} 

void UStructProperty::ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat ) const
{
	UStructProperty_ExportTextItem(Struct, ValueStr, PropertyValue, DefaultValue, Parent, PortFlags, TextFormat);
} 

/**
 * Attempts to read an array index (xxx) sequence.  Handles const/enum replacements, etc.
 * @param	Scope		the scope of the object/struct containing the property we're currently importing
 * @param	Str			[out] pointer to the the buffer containing the property value to import
 * @param	Error		[out] only filled if an array index was specified, but couldn't be resolved. contains more information about the problem
 *
 * @return	the array index for this defaultproperties line.  INDEX_NONE if this line doesn't contains an array specifier, or 0 if there was an error parsing the specifier.
 */
static INT ReadArrayIndex(UObject* ScopeObj, const TCHAR*& Str, FStringOutputDevice& Error)
{
	const TCHAR* Start = Str;
	INT Index = INDEX_NONE;
	SkipWhitespace(Str);

	if (*Str == '[')
	{
		Str++;
		FString IndexText(TEXT(""));
		while ( *Str && *Str != ']' )
		{
			if ( *Str == TCHAR('=') )
			{
				// we've encountered an equals sign before the closing bracket
				Error.Logf(TEXT("Missing ']' in default properties subscript for '%s'"), Start);
				return 0;
			}

			IndexText += *Str++;
		}

		if ( *Str++ )
		{
			if (IndexText.Len() > 0 )
			{
				if ( appIsAlpha(IndexText[0]))
				{
					FString EnumNameString, EnumValueString;
					if ( IndexText.Split(TEXT("."), &EnumNameString, &EnumValueString) )
					{
						for ( UObject* CurrentScope = ScopeObj; CurrentScope; CurrentScope = CurrentScope->GetOuter() )
						{
							UEnum* Enum = FindField<UEnum>(CurrentScope->GetClass(), *EnumNameString);
							if ( Enum != NULL )
							{
								FName EnumName = FName(*EnumValueString,FNAME_Find);
								if ( EnumName != NAME_None )
								{
									Index = Enum->FindEnumIndex(EnumName);
									if (Index == INDEX_NONE)
									{
										Error.Logf(TEXT("Invalid subscript in defaultproperties: Unable to resolve enum reference '%s'"), *EnumName.ToString());
										return Index;
									}
								}
								break;
							}
						}
					}
					if ( Index == INDEX_NONE )
					{
						// check for any enum references
						FName EnumName = FName(*IndexText,FNAME_Find);
						if (EnumName != NAME_None)
						{
							// search for the enum in question
							for (TObjectIterator<UEnum> It; It && Index == INDEX_NONE; ++It)
							{
								Index = It->FindEnumIndex(EnumName);
							}

							if (Index == INDEX_NONE)
							{
								Index = 0;
								Error.Logf(TEXT("Invalid subscript in defaultproperties: Unable to resolve enum reference '%s'"), *EnumName.ToString());
							}
						}
						else
						{
							UConst* Const = NULL;

							// search for const ref
							for ( UObject* CurrentScope = ScopeObj; CurrentScope; CurrentScope = CurrentScope->GetOuter() )
							{
								Const = FindField<UConst>(CurrentScope->GetClass(), *IndexText);

								if ( Const != NULL )
								{
									Index = appAtoi(*Const->Value);
									break;
								}
							}

							if ( Const == NULL )
							{
								Index = 0;
								Error.Logf(TEXT("Invalid subscript '%s' in default properties"), *IndexText);
							}
						}
					}
				}
				else
				{
					Index = appAtoi(*IndexText);
				}
			}
			else
			{
				Index = 0;
				// nothing was specified between the opening and closing parenthesis
				Error.Logf(TEXT("Invalid subscript in default properties for '%s'"), Start);
			}
		}
		else
		{
			Index = 0;
			Error.Logf(TEXT("Missing ']' in default properties subscript for '%s'"), Start);
		}
	}
	return Index;
}

/** 
 * Do not attempt to import this property if there is no value for it - i.e. (Prop1=,Prop2=)
 * This normally only happens for empty strings or empty dynamic arrays, and the alternative
 * is for strings and dynamic arrays to always export blank delimiters, such as Array=() or String="", 
 * but this tends to cause problems with inherited property values being overwritten, especially in the localization 
 * import/export code, or when a property has both CPF_Localized & CPF_Config flags

 * The safest way is to interpret blank delimiters as an indication that the current value should be overwritten with an empty
 * value, while the lack of any value or delimiter as an indication to not import this property, thereby preventing any current
 * values from being overwritten if this is not the intent.

 * Thus, arrays and strings will only export empty delimiters when overriding an inherited property's value with an 
 * empty value.
 */
static UBOOL IsPropertyValueSpecified( const TCHAR*& Buffer, TCHAR StructEndChar, EImportTextFormat TextFormat )
{
	const UBOOL bIsNull = TextFormat == ITF_JSON && appStrnicmp(Buffer,TEXT("null"),4) == 0;
	if (bIsNull)
	{
		Buffer += 4;
		return FALSE;
	}
	return Buffer && *Buffer && *Buffer != TCHAR(',') && *Buffer != StructEndChar;
}

const TCHAR* UStructProperty_ImportText( class UScriptStruct* Struct, const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText )
{
	// this keeps track of the number of errors we've logged, so that we can add new lines when logging more than one error
	INT ErrorCount = 0;
	const TCHAR* Buffer = InBuffer;
	if( *Buffer++ == TCHAR('(') )
	{
		// Parse all properties.
		while( *Buffer != TCHAR(')') )
		{
			SkipWhitespace(Buffer);

			// Get key name.
			TCHAR Name[NAME_SIZE];
			int Count=0;
			while( Count<NAME_SIZE-1 && *Buffer && *Buffer!=TCHAR('=') && *Buffer!=TCHAR('[') && *Buffer!=TCHAR('(') && !appIsWhitespace(*Buffer) )
			{
				Name[Count++] = *Buffer++;
			}
			Name[Count++] = 0;

			// Get optional array element.
			FStringOutputDevice ArrayErrorText;
			INT ArrayIndex = ReadArrayIndex(Parent, Buffer, ArrayErrorText);
			if( ArrayErrorText.Len() )
			{
				if( ErrorText )
				{
					ErrorText->Logf( TEXT("%sImportText (%s): %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), *ArrayErrorText );
				}
				else
				{
					warnf( NAME_Error, TEXT("ImportText (%s): %s"), *Struct->GetName(), *ArrayErrorText );
				}

				return NULL;
			}

			SkipWhitespace(Buffer);

			// Verify format.
			if( *Buffer++ != TCHAR('=') )
			{
				if ( ErrorText )
				{
					if ( appStrlen(Name) == 0 )
					{
						ErrorText->Logf(TEXT("%sImportText (%s): Missing key name in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), InBuffer );
					}
					else
					{
						ErrorText->Logf(TEXT("%sImportText (%s): Missing value for property '%s'"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), Name);
					}
				}
				else
				{
					if ( appStrlen(Name) == 0 )
					{
						warnf( NAME_Error, TEXT("ImportText (%s): Missing key name in: %s"), *Struct->GetName(), InBuffer );
					}
					else
					{
						warnf( NAME_Error, TEXT("ImportText (%s): Missing value for property '%s'"), *Struct->GetName(), Name);
					}
				}
				return NULL;
			}

			// See if the property exists in the struct.
			FName GotName( Name, FNAME_Find, TRUE );
			UBOOL Parsed = 0;
			if( GotName != NAME_None )
			{
				const UBOOL bConfigPropertiesOnly = (PortFlags&PPF_ConfigOnly) != 0 && (Struct->StructFlags&STRUCT_StrictConfig) != 0;
				for( TFieldIterator<UProperty> It(Struct); It; ++It )
				{
					UProperty* Property = *It;

					if ( Property->GetFName() != GotName
					||	 Property->GetSize() == 0
					||	!Property->Port(PortFlags)
					||	(bConfigPropertiesOnly && !Property->HasAnyPropertyFlags(CPF_Config)))
					{
                        continue;
					}

					// check that the array element (if any) specified is valid
					// note that if the property is a dynamic array, any index is valid
					if( ArrayIndex >= Property->ArrayDim && !Property->IsA(UArrayProperty::StaticClass()))
					{
						if ( ErrorText )
						{
							ErrorText->Logf(TEXT("%sImportText (%s): Array index out of bounds for property %s in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), *Property->GetName(), InBuffer);
						}
						else
						{
							warnf(NAME_Error, TEXT("ImportText (%s): Array index out of bounds for property %s in: %s"), *Struct->GetName(), *Property->GetName(), InBuffer);
						}
				        return NULL;
                    }
					// copied from ImportProperties() to support setting single dynamic array elements
					if( Property->IsA(UArrayProperty::StaticClass()) )
					{
						if( ArrayIndex == INDEX_NONE )
						{
							// This case is triggered when ArrayIndex couldn't be pulled. But it's not an error.
							// This is when importing without indices, for instance:
							// MyVar=(MyArray=(MyVal1,MyVal2,MayVal3))
							ArrayIndex = 0;
							SkipWhitespace(Buffer);
							if ( IsPropertyValueSpecified(Buffer, TEXT(')'),ITF_Unreal) )
							{
								Buffer = Property->ImportText(Buffer, Data + Property->Offset + ArrayIndex * Property->ElementSize, PortFlags | PPF_Delimited, Parent, ErrorText);
							}
						}
						else
						{
							SkipWhitespace(Buffer);
							if ( IsPropertyValueSpecified(Buffer, TEXT(')'),ITF_Unreal) )
							{
								FScriptArray* Array = (FScriptArray*)(Data + Property->Offset);
								UArrayProperty* ArrayProp = (UArrayProperty*)Property;
								if( ArrayIndex >= Array->Num() )
								{
									INT NumToAdd = ArrayIndex - Array->Num() + 1;
									Array->AddZeroed(NumToAdd, ArrayProp->Inner->ElementSize);
									UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProp->Inner,CLASS_IsAUStructProperty);
									if (StructProperty)
									{
										// initialize struct defaults for each element we had to add to the array
										for (INT i = 1; i <= NumToAdd; i++)
										{
											StructProperty->CopySingleValue((BYTE*)Array->GetData() + ((Array->Num() - i) * ArrayProp->Inner->ElementSize), StructProperty->Struct->GetDefaults(), NULL);
										}
									}
								}

								Buffer = ArrayProp->Inner->ImportText(Buffer, (BYTE*)Array->GetData() + ArrayIndex * ArrayProp->Inner->ElementSize, (PortFlags&PPF_RestrictImportTypes)|PPF_Delimited|PPF_CheckReferences, Parent);
							}
						}
					}
					else
					{
						if( ArrayIndex == INDEX_NONE )
						{
							ArrayIndex = 0;
						}
						SkipWhitespace(Buffer);

						if ( IsPropertyValueSpecified(Buffer, TEXT(')'),ITF_Unreal) )
						{
							Buffer = Property->ImportText(Buffer, Data + Property->Offset + ArrayIndex * Property->ElementSize, PortFlags | PPF_Delimited, Parent, ErrorText);
						}
					}

					if (Buffer == NULL)
                    {
						if ( !ErrorText )
						{
							// this should be an error as the properties from the .ini / .int file are not correctly being read in and probably are affecting things in subtle ways
							warnf(NAME_Error, TEXT("ImportText (%s): Property import failed for %s in: %s"), *Struct->GetName(), *Property->GetName(), InBuffer);
						}
						return NULL;
                    }

					Parsed = 1;
                    break;
				}
			}

			// If not parsed, skip this property in the stream.
			if( !Parsed )
			{
				if ( ErrorText )
				{
					ErrorText->Logf(TEXT("%sImportText (%s): Unknown member %s in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), Name, InBuffer );
				}
				else
				{
					warnf( NAME_Error, TEXT("ImportText (%s): Unknown member %s in: %s"), *Struct->GetName(), Name, InBuffer );
				}

				INT SubCount=0;
				while
				(	*Buffer
				&&	*Buffer!=TCHAR('\r')
				&&	*Buffer!=TCHAR('\n') 
				&&	(SubCount>0 || *Buffer!=TCHAR(')'))
				&&	(SubCount>0 || *Buffer!=TCHAR(',')) )
				{
					SkipWhitespace(Buffer);
					if( *Buffer == TCHAR('\"') )
					{
						while( *Buffer && *Buffer!=TCHAR('\"') && *Buffer != TCHAR('\n') && *Buffer != TCHAR('\r') )
						{
							Buffer++;
						}

						if( *Buffer != TCHAR('\"') )
						{
							if ( ErrorText )
							{
								ErrorText->Logf(TEXT("%sImportText (%s): Bad quoted string at: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), Buffer );
							}
							else
							{
								warnf( NAME_Error, TEXT("ImportText (%s): Bad quoted string at: %s"), *Struct->GetName(), Buffer );
							}
							return NULL;
						}
					}
					else if( *Buffer == TCHAR('(') )
					{
						SubCount++;
					}
					else if( *Buffer == TCHAR(')') )
					{
						SubCount--;
						if( SubCount < 0 )
						{
							if ( ErrorText )
							{
								ErrorText->Logf(TEXT("%sImportText (%s): Too many closing parenthesis in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), InBuffer );
							}
							else
							{
								warnf( NAME_Error, TEXT("ImportText (%s): Too many closing parenthesis in: %s"), *Struct->GetName(), InBuffer );
							}
							return NULL;
						}
					}
					Buffer++;
				}
				if( SubCount > 0 )
				{
					if ( ErrorText )
					{
						ErrorText->Logf(TEXT("%sImportText(%s): Not enough closing parenthesis in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), InBuffer );
					}
					else
					{
						warnf( NAME_Error, TEXT("ImportTex (%s): Not enough closing parenthesis in: %s"), *Struct->GetName(), InBuffer );
					}
					return NULL;
				}
			}

			SkipWhitespace(Buffer);

			// Skip comma.
			if( *Buffer==TCHAR(',') )
			{
				// Skip comma.
				Buffer++;
			}
			else if( *Buffer!=TCHAR(')') )
			{
				if ( ErrorText )
				{
					ErrorText->Logf(TEXT("%sImportText (%s): Missing closing parenthesis: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), InBuffer );
				}
				else
				{
					warnf( NAME_Error, TEXT("ImportText (%s): Missing closing parenthesis: %s"), *Struct->GetName(), InBuffer );
				}
				return NULL;
			}

			SkipWhitespace(Buffer);
		}

		// Skip trailing ')'.
		Buffer++;
	}
	else
	{
		if ( ErrorText )
		{
			ErrorText->Logf(TEXT("%sImportText (%s): Missing opening parenthesis: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *Struct->GetName(), InBuffer );
		}
		else
		{
			warnf( NAME_Warning, TEXT("ImportText (%s): Missing opening parenthesis: %s"), *Struct->GetName(), InBuffer );
		}
		return NULL;
	}
	return Buffer;
}

const TCHAR* UStructProperty::ImportText( const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText, EImportTextFormat TextFormat ) const
{
	TCHAR StructStartChar = TEXT('(');
	TCHAR StructEndChar = TEXT(')');
	TCHAR StructKeyValuePairSeparator = TEXT('=');
	if (TextFormat == ITF_JSON)
	{
		StructStartChar = TEXT('{');
		StructEndChar = TEXT('}');
		StructKeyValuePairSeparator = TEXT(':');
	}

	if ( !ValidateImportFlags(PortFlags,ErrorText) )
		return NULL;

	// this keeps track of the number of errors we've logged, so that we can add new lines when logging more than one error
	INT ErrorCount = 0;
	const TCHAR* Buffer = InBuffer;
	if( *Buffer++ == StructStartChar )
	{
		// Parse all properties.
		while( *Buffer != StructEndChar )
		{
			SkipWhitespace(Buffer);

			// Get key name.
			TCHAR Name[NAME_SIZE];
			int Count=0;
			while( Count<NAME_SIZE-1 && *Buffer && *Buffer!=StructKeyValuePairSeparator && *Buffer!=TCHAR('[') && *Buffer!=TCHAR('(') && !appIsWhitespace(*Buffer) )
			{
				Name[Count++] = *Buffer++;
			}
			Name[Count++] = 0;
			// Strip quotes from key name
			if (TextFormat == ITF_JSON)
			{
				FString NameStripped = FString(Name).Replace(TEXT("\""),TEXT(""));
				appStrncpy(Name,*NameStripped,NAME_SIZE);
			}

			// Get optional array element.
			FStringOutputDevice ArrayErrorText;
			INT ArrayIndex = ReadArrayIndex(Parent, Buffer, ArrayErrorText);
			if( ArrayErrorText.Len() )
			{
				if( ErrorText )
				{
					ErrorText->Logf( TEXT("%sImportText (%s): %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), *ArrayErrorText );
				}
				else
				{
					warnf( NAME_Error, TEXT("ImportText (%s): %s"), *GetName(), *ArrayErrorText );
				}

				return NULL;
			}

			SkipWhitespace(Buffer);

			// Verify format.
			if( *Buffer++ != StructKeyValuePairSeparator )
			{
				if ( ErrorText )
				{
					if ( appStrlen(Name) == 0 )
					{
						ErrorText->Logf(TEXT("%sImportText (%s): Missing key name in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), InBuffer );
					}
					else
					{
						ErrorText->Logf(TEXT("%sImportText (%s): Missing value for property '%s'"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), Name);
					}
				}
				else
				{
					if ( appStrlen(Name) == 0 )
					{
						warnf( NAME_Error, TEXT("ImportText (%s): Missing key name in: %s"), *GetName(), InBuffer );
					}
					else
					{
						warnf( NAME_Error, TEXT("ImportText (%s): Missing value for property '%s'"), *GetName(), Name);
					}
				}
				return NULL;
			}

			// See if the property exists in the struct.
			FName GotName( Name, FNAME_Find, TRUE );
			UBOOL Parsed = 0;
			if( GotName != NAME_None )
			{
				const UBOOL bConfigPropertiesOnly = (PortFlags&PPF_ConfigOnly) != 0 && (Struct->StructFlags&STRUCT_StrictConfig) != 0;
				for( TFieldIterator<UProperty> It(Struct); It; ++It )
				{
					UProperty* Property = *It;

					if ( Property->GetFName() != GotName
					||	 Property->GetSize() == 0
					||	!Property->Port(PortFlags)
					||	(bConfigPropertiesOnly && !Property->HasAnyPropertyFlags(CPF_Config)))
					{
                        continue;
					}

					// check that the array element (if any) specified is valid
					// note that if the property is a dynamic array, any index is valid
					if( ArrayIndex >= Property->ArrayDim && !Property->IsA(UArrayProperty::StaticClass()))
					{
						if ( ErrorText )
						{
							ErrorText->Logf(TEXT("%sImportText (%s): Array index out of bounds for property %s in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), *Property->GetName(), InBuffer);
						}
						else
						{
							warnf(NAME_Error, TEXT("ImportText (%s): Array index out of bounds for property %s in: %s"), *GetName(), *Property->GetName(), InBuffer);
						}
				        return NULL;
                    }
					// copied from ImportProperties() to support setting single dynamic array elements
					if( Property->IsA(UArrayProperty::StaticClass()) )
					{
						if( ArrayIndex == INDEX_NONE )
						{
							// This case is triggered when ArrayIndex couldn't be pulled. But it's not an error.
							// This is when importing without indices, for instance:
							// MyVar=(MyArray=(MyVal1,MyVal2,MayVal3))
							ArrayIndex = 0;
							SkipWhitespace(Buffer);
							if ( IsPropertyValueSpecified(Buffer, StructEndChar, TextFormat) )
							{
								Buffer = Property->ImportText(Buffer, Data + Property->Offset + ArrayIndex * Property->ElementSize, PortFlags | PPF_Delimited, Parent, ErrorText, TextFormat);
							}
						}
						else
						{
							SkipWhitespace(Buffer);
							if ( IsPropertyValueSpecified(Buffer, StructEndChar, TextFormat) )
							{
								FScriptArray* Array = (FScriptArray*)(Data + Property->Offset);
								UArrayProperty* ArrayProp = (UArrayProperty*)Property;
								if( ArrayIndex >= Array->Num() )
								{
									INT NumToAdd = ArrayIndex - Array->Num() + 1;
									Array->AddZeroed(NumToAdd, ArrayProp->Inner->ElementSize);
									UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProp->Inner,CLASS_IsAUStructProperty);
									if (StructProperty)
									{
										// initialize struct defaults for each element we had to add to the array
										for (INT i = 1; i <= NumToAdd; i++)
										{
											StructProperty->CopySingleValue((BYTE*)Array->GetData() + ((Array->Num() - i) * ArrayProp->Inner->ElementSize), StructProperty->Struct->GetDefaults(), NULL);
										}
									}
								}

								Buffer = ArrayProp->Inner->ImportText(Buffer, (BYTE*)Array->GetData() + ArrayIndex * ArrayProp->Inner->ElementSize, (PortFlags&PPF_RestrictImportTypes)|PPF_Delimited|PPF_CheckReferences, Parent, ErrorText, TextFormat);
							}
						}
					}
					else
					{
						if( ArrayIndex == INDEX_NONE )
						{
							ArrayIndex = 0;
						}
						SkipWhitespace(Buffer);

						if ( IsPropertyValueSpecified(Buffer, StructEndChar, TextFormat) )
						{
							Buffer = Property->ImportText(Buffer, Data + Property->Offset + ArrayIndex * Property->ElementSize, PortFlags | PPF_Delimited, Parent, ErrorText, TextFormat);
						}
					}

					if (Buffer == NULL)
                    {
						if ( !ErrorText )
						{
							// this should be an error as the properties from the .ini / .int file are not correctly being read in and probably are affecting things in subtle ways
							warnf(NAME_Error, TEXT("ImportText (%s): Property import failed for %s in: %s"), *GetName(), *Property->GetName(), InBuffer);
						}
						return NULL;
                    }

					Parsed = 1;
                    break;
				}
			}

			// If not parsed, skip this property in the stream.
			if( !Parsed )
			{
				if ( ErrorText )
				{
					ErrorText->Logf(TEXT("%sImportText (%s): Unknown member %s in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), Name, InBuffer );
				}
				else
				{
					warnf( NAME_Error, TEXT("ImportText (%s): Unknown member %s in: %s"), *GetName(), Name, InBuffer );
				}

				INT SubCount=0;
				while
				(	*Buffer
				&&	*Buffer!=TCHAR('\r')
				&&	*Buffer!=TCHAR('\n') 
				&&	(SubCount>0 || *Buffer!=StructEndChar)
				&&	(SubCount>0 || *Buffer!=TCHAR(',')) )
				{
					SkipWhitespace(Buffer);
					if( *Buffer == TCHAR('\"') )
					{
						Buffer++;
						while( *Buffer && *Buffer!=TCHAR('\"') && *Buffer != TCHAR('\n') && *Buffer != TCHAR('\r') )
						{
							Buffer++;
						}

						if( *Buffer != TCHAR('\"') )
						{
							if ( ErrorText )
							{
								ErrorText->Logf(TEXT("%sImportText (%s): Bad quoted string at: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), Buffer );
							}
							else
							{
								warnf( NAME_Error, TEXT("ImportText (%s): Bad quoted string at: %s"), *GetName(), Buffer );
							}
							return NULL;
						}
					}
					else if( *Buffer == StructStartChar )
					{
						SubCount++;
					}
					else if( *Buffer == StructEndChar )
					{
						SubCount--;
						if( SubCount < 0 )
						{
							if ( ErrorText )
							{
								ErrorText->Logf(TEXT("%sImportText (%s): Too many closing parenthesis in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), InBuffer );
							}
							else
							{
								warnf( NAME_Error, TEXT("ImportText (%s): Too many closing parenthesis in: %s"), *GetName(), InBuffer );
							}
							return NULL;
						}
					}
					Buffer++;
					SkipWhitespace(Buffer);
				}
				if( SubCount > 0 )
				{
					if ( ErrorText )
					{
						ErrorText->Logf(TEXT("%sImportText(%s): Not enough closing parenthesis in: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), InBuffer );
					}
					else
					{
						warnf( NAME_Error, TEXT("ImportTex (%s): Not enough closing parenthesis in: %s"), *GetName(), InBuffer );
					}
					return NULL;
				}
			}

			SkipWhitespace(Buffer);

			// Skip comma.
			if( *Buffer==TCHAR(',') )
			{
				// Skip comma.
				Buffer++;
			}
			else if( *Buffer!=StructEndChar )
			{
				if ( ErrorText )
				{
					ErrorText->Logf(TEXT("%sImportText (%s): Missing closing parenthesis: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), InBuffer );
				}
				else
				{
					warnf( NAME_Error, TEXT("ImportText (%s): Missing closing parenthesis: %s"), *GetName(), InBuffer );
				}
				return NULL;
			}

			SkipWhitespace(Buffer);
		}

		// Skip trailing ')'.
		Buffer++;
	}
	else
	{
		if ( ErrorText )
		{
			ErrorText->Logf(TEXT("%sImportText (%s): Missing opening parenthesis: %s"), ErrorCount++ > 0 ? LINE_TERMINATOR : TEXT(""), *GetName(), InBuffer );
		}
		else
		{
			warnf( NAME_Warning, TEXT("ImportText (%s): Missing opening parenthesis: %s"), *GetName(), InBuffer );
		}
		return NULL;
	}
	return Buffer;
}

/**
 * Instances any UObjectProperty values that still match the default value.
 *
 * @param	Value				the address where the pointer to the instanced object should be stored.  This should always correspond to the BASE + OFFSET, where
 *									BASE = (for class member properties) the address of the UObject which contains this data, (for script struct member properties) the
 *										address of the struct's data
 *									OFFSET = the Offset of this UProperty from base
 * @param	DefaultValue		the address where the pointer to the default value is stored.  Evaluated the same way as Value
 * @param	OwnerObject			the object that contains the destination data.  Will be the used as the Outer for any newly instanced subobjects.
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UStructProperty::InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	for ( INT ArrayIndex = 0; ArrayIndex < ArrayDim; ArrayIndex++ )
	{
		BYTE* StructValue = (BYTE*)Value + ArrayIndex * ElementSize;
		BYTE* DefaultStructValue = DefaultValue ? (BYTE*)DefaultValue + ArrayIndex * ElementSize : NULL;

		Struct->InstanceSubobjectTemplates(StructValue, DefaultStructValue, Struct->GetPropertiesSize(), OwnerObject, InstanceGraph);
	}
}

void UStructProperty::CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if ( (PropertyFlags & CPF_NeedCtorLink) != 0 )
	{
		for( TFieldIterator<UProperty> It(Struct); It; ++It )
		{
			It->CopyCompleteValue( (BYTE*)Dest + It->Offset, (BYTE*)Src + It->Offset, SubobjectRoot, DestOwnerObject, InstanceGraph );
		}
	}
	else
	{
		appMemcpy( Dest, Src, ElementSize );
	}
}
void UStructProperty::CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot/*=NULL*/, UObject* DestOwnerObject/*=NULL*/, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	if ( (PropertyFlags & CPF_NeedCtorLink) != 0 )
	{
		Super::CopyCompleteValue(Dest,Src,SubobjectRoot,DestOwnerObject, InstanceGraph);
	}
	else
	{
		// memcpy the entire struct
		appMemcpy( Dest, Src, ArrayDim*ElementSize );
	}
}
void UStructProperty::InitializeValue( BYTE* Dest ) const
{
	if ( Struct && Struct->GetDefaultsCount() && HasValue(Struct->GetDefaults()) )
	{
		for ( INT ArrayIndex = 0; ArrayIndex < ArrayDim; ArrayIndex++ )
		{
			CopySingleValue( Dest + ArrayIndex * ElementSize, Struct->GetDefaults() );
		}
	}
}

void UStructProperty::ClearValue( BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	checkSlow(Struct);

	for ( UProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if ( Property->ArrayDim > 0 )
		{
			for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
			{
				BYTE* PropertyData = Data + Property->Offset + ArrayIndex * Property->ElementSize;
				Property->ClearValue(PropertyData,PortFlags);
			}
		}
		else
		{
			Property->ClearValue(Data + Property->Offset, PortFlags);
		}
	}
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
			{
				P->DestroyValue( (BYTE*)Dest + i*ElementSize + P->Offset );
			}
		}
	}
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
 */
void UStructProperty::InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	if( (PropertyFlags & CPF_Native) == 0 )
	{
		for (INT Index = 0; Index < ArrayDim; Index++)
		{
			Struct->InstanceComponentTemplates( Data + ElementSize * Index, DefaultData ? DefaultData + ElementSize * Index : NULL, Struct->GetPropertiesSize(), Owner, InstanceGraph );
		}
	}
}

UBOOL UStructProperty::HasValue( const BYTE* Data, DWORD PortFlags/*=0*/ ) const
{
	checkSlow(Struct);
	for ( UProperty* Property = Struct->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		if ( Property->ArrayDim > 0 )
		{
			for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
			{
				const BYTE* PropertyData = Data + Property->Offset + ArrayIndex * Property->ElementSize;
				if ( Property->HasValue(PropertyData, PortFlags) )
				{
					return TRUE;
				}
			}
		}
		else
		{
			if ( Property->HasValue(Data + Property->Offset, PortFlags) )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}
UBOOL UStructProperty::IsLocalized() const
{
	// prevent recursion in the case of structs containing dynamic arrays of themselves
	static TArray<const UStructProperty*> EncounteredStructProps;
	if (EncounteredStructProps.ContainsItem(this))
	{
		return Super::IsLocalized();
	}
	else
	{
		EncounteredStructProps.AddItem(this);
		for ( TFieldIterator<UProperty> It(Struct); It; ++It )
		{
			if ( It->IsLocalized() )
			{
				EncounteredStructProps.RemoveSingleItemSwap(this);
				return TRUE;
			}
		}
		EncounteredStructProps.RemoveSingleItemSwap(this);
		return Super::IsLocalized();
	}
}

IMPLEMENT_CLASS(UStructProperty);

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
 *
 * @return TRUE if property (or sub- properties) contain a UObjectProperty that is marked CPF_NeedCtorLink, FALSE otherwise
 */
UBOOL UObjectProperty::ContainsInstancedObjectProperty() const
{
	return (PropertyFlags&CPF_NeedCtorLink) != 0;
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
 *
 * @return TRUE if property (or sub- properties) contain a UObjectProperty that is marked CPF_NeedCtorLink, FALSE otherwise
 */
UBOOL UArrayProperty::ContainsInstancedObjectProperty() const
{
	check(Inner);
	return Inner->ContainsInstancedObjectProperty();
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
 *
 * @return TRUE if property (or sub- properties) contain a UObjectProperty that is marked CPF_NeedCtorLink, FALSE otherwise
 */
UBOOL UMapProperty::ContainsInstancedObjectProperty() const
{
#if TMAPS_IMPLEMENTED
	check(Key);
	check(Value);
	return Key->ContainsInstancedObjectProperty() || Value->ContainsInstancedObjectProperty();
#else
	return FALSE;
#endif
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
 *
 * @return TRUE if property (or sub- properties) contain a UObjectProperty that is marked CPF_NeedCtorLink, FALSE otherwise
 */
UBOOL UStructProperty::ContainsInstancedObjectProperty() const
{
	check(Struct);
	UProperty* Property = Struct->RefLink;
	while( Property )
	{
		if( Property->ContainsInstancedObjectProperty() )
		{
			return TRUE;
		}
		Property = Property->NextRef;
	}
	return FALSE;
}

/* ==========================================================================================================
	FEditPropertyChain
========================================================================================================== */
/**
 * Sets the ActivePropertyNode to the node associated with the property specified.
 *
 * @param	NewActiveProperty	the UProperty that is currently being evaluated by Pre/PostEditChange
 *
 * @return	TRUE if the ActivePropertyNode was successfully changed to the node associated with the property
 *			specified.  FALSE if there was no node corresponding to that property.
 */
UBOOL FEditPropertyChain::SetActivePropertyNode( UProperty* NewActiveProperty )
{
	UBOOL bResult = FALSE;

	TDoubleLinkedListNode* PropertyNode = FindNode(NewActiveProperty);
	if ( PropertyNode != NULL )
	{
		ActivePropertyNode = PropertyNode;
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Sets the ActiveMemberPropertyNode to the node associated with the property specified.
 *
 * @param	NewActiveMemberProperty		the member UProperty which contains the property currently being evaluated
 *										by Pre/PostEditChange
 *
 * @return	TRUE if the ActiveMemberPropertyNode was successfully changed to the node associated with the
 *			property specified.  FALSE if there was no node corresponding to that property.
 */
UBOOL FEditPropertyChain::SetActiveMemberPropertyNode( UProperty* NewActiveMemberProperty )
{
	UBOOL bResult = FALSE;

	TDoubleLinkedListNode* PropertyNode = FindNode(NewActiveMemberProperty);
	if ( PropertyNode != NULL )
	{
		ActiveMemberPropertyNode = PropertyNode;
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Returns the node corresponding to the currently active property.
 */
FEditPropertyChain::TDoubleLinkedListNode* FEditPropertyChain::GetActiveNode() const
{
	return ActivePropertyNode;
}

/**
 * Returns the node corresponding to the currently active property, or if the currently active property
 * is not a member variable (i.e. inside of a struct/array), the node corresponding to the member variable
 * which contains the currently active property.
 */
FEditPropertyChain::TDoubleLinkedListNode* FEditPropertyChain::GetActiveMemberNode() const
{
	return ActiveMemberPropertyNode;
}

/**
 * Updates the size reported by Num().  Child classes can use this function to conveniently
 * hook into list additions/removals.
 *
 * This version ensures that the ActivePropertyNode either points to a valid node, or NULL if this list is empty.
 *
 * @param	NewListSize		the new size for this list
 */
void FEditPropertyChain::SetListSize( INT NewListSize )
{
	INT PreviousListSize = Num();
	TDoubleLinkedList<UProperty*>::SetListSize(NewListSize);

	if ( Num() == 0 )
	{
		ActivePropertyNode = ActiveMemberPropertyNode = NULL;
	}
	else if ( PreviousListSize != NewListSize )
	{
		// if we have no active property node, set it to the tail of the list, which would be the property that was
		// actually changed by the user (assuming this FEditPropertyChain is being used by the code that handles changes
		// to property values in the editor)
		if ( ActivePropertyNode == NULL )
		{
			ActivePropertyNode = GetTail();
		}

		// now figure out which property the ActiveMemberPropertyNode should be pointing at
		if ( ActivePropertyNode != NULL )
		{
			// start at the currently active property
			TDoubleLinkedListNode* PropertyNode = ActivePropertyNode;

			// then iterate backwards through the chain, searching for the first property which is owned by a UClass - this is our member property
			for ( TIterator It(PropertyNode); It; --It )
			{
				// if we've found the member property, we can stop here
				if ( It->GetOuter()->GetClass() == UClass::StaticClass() )
				{
					PropertyNode = It.GetNode();
					break;
				}
			}

			ActiveMemberPropertyNode = PropertyNode;
		}
	}
}










