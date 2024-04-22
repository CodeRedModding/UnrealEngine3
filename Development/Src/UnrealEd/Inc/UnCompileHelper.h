/*=============================================================================
UnCompileHelper.h: UnrealScript compiler helper classes.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnClassTree.h"

#ifndef __UNCOMPILEHELPER_H__
#define __UNCOMPILEHELPER_H__

extern class FCompilerMetadataManager*	GScriptHelper;

/*-----------------------------------------------------------------------------
	FPropertyBase.
-----------------------------------------------------------------------------*/

enum EPropertyReferenceType
{
	/** no reference */
	CPRT_None					=0,

	/** we're referencing this property in order to assign a value to it */
	CPRT_AssignValue			=1,

	/** we're referencing this property's value in such a way that doesn't require it be initialized (if checks, iterators, etc.) */
	CPRT_SimpleReference		=2,

	/** we're referecning this property's value in such a away that requires it to be initialized (assigning to another property, using in a function call, etc) */
	CPRT_AssignmentReference	=3,

	/** we're referencing this property in a way that both changes its value and references the value (combination of CPRT_AssignmentReference and CPRT_AssignValue) */
	CPRT_DualReference			=4,
};

enum EFunctionExportFlags
{
	FUNCEXPORT_Virtual		=0x00000001,	// function should be exported as a virtual function
	FUNCEXPORT_Final		=0x00000002,	// function declaration included "final" keyword.  Used to differentiate between functions that have FUNC_Final only because they're private
	FUNCEXPORT_NoExport		=0x00000004,	// only DECLARE_FUNCTION stub should be exported to header file
	FUNCEXPORT_Const		=0x00000008,	// export C++ declaration as const
	FUNCEXPORT_NoExportHeader =0x00000010,	// don't export the C++ declaration, only the DECLARE_FUNCTION wrapper
};

enum EPropertyHeaderExportFlags
{
	PROPEXPORT_Public		=0x00000001,	// property should be exported as public
	PROPEXPORT_Private		=0x00000002,	// property should be exported as private
	PROPEXPORT_Protected	=0x00000004,	// property should be exported as protected
};

#ifndef CASE_TEXT
#define CASE_TEXT(txt) case txt: return TEXT(#txt)
#endif

const TCHAR* GetPropertyTypeText( EPropertyType Type );
const TCHAR* GetPropertyRefText( EPropertyReferenceType Type );

/**
 * Basic information describing a type.
 */
class FPropertyBase
{
public:
	// Variables.
	EPropertyType Type;
	INT ArrayDim;
	QWORD PropertyFlags;

	/**
	 * A mask of EPropertyHeaderExportFlags which are used for modifying how this property is exported to the native class header
	 */
	DWORD PropertyExportFlags;
	union
	{
		class UEnum* Enum;
		class UClass* PropertyClass;
		class UScriptStruct* Struct;
		class UFunction* Function;
#if _WIN64
		QWORD BitMask;
		SQWORD StringSize;
#else
		DWORD BitMask;
		INT StringSize;
#endif
	};

	UClass*	MetaClass;

	/**
	 * For static array properties which use an enum value to specify the array size, corresponds to the UEnum that was used for the size.
	 */
	UEnum*	ArrayIndexEnum;

	FName	DelegateName;

	/** Raw string (not type-checked) used for specifying special text when exporting a property to the *Classes.h file */
	FString	ExportInfo;

	/** Map of key value pairs that will be added to the package's UMetaData for this property */
	TMap<FName, FString> MetaData;

	EPropertyReferenceType ReferenceType;

	/** @name Constructors */
	//@{
	FPropertyBase( EPropertyType InType, EPropertyReferenceType InRefType = CPRT_None )
	:	Type(InType), ArrayDim(1), PropertyFlags(0), PropertyExportFlags(PROPEXPORT_Public)
	, BitMask(0), MetaClass(NULL), ArrayIndexEnum(NULL), DelegateName(NAME_None), ReferenceType(InRefType)
	{}
	FPropertyBase( UEnum* InEnum, EPropertyReferenceType InRefType = CPRT_None )
	:	Type(CPT_Byte), ArrayDim(1), PropertyFlags(0), PropertyExportFlags(PROPEXPORT_Public)
	, Enum(InEnum), MetaClass(NULL), ArrayIndexEnum(NULL), DelegateName(NAME_None), ReferenceType(InRefType)
	{}
	FPropertyBase( UClass* InClass, UClass* InMetaClass=NULL, EPropertyReferenceType InRefType = CPRT_None )
	:	Type(CPT_ObjectReference), ArrayDim(1), PropertyFlags(0), PropertyExportFlags(PROPEXPORT_Public)
	, PropertyClass(InClass), MetaClass(InMetaClass), ArrayIndexEnum(NULL), DelegateName(NAME_None), ReferenceType(InRefType)
	{
		// if this is an interface class, we use the UInterfaceProperty class instead of UObjectProperty
		if ( InClass->HasAnyClassFlags(CLASS_Interface) )
		{
			Type = CPT_Interface;
		}
	}
	FPropertyBase( UScriptStruct* InStruct, EPropertyReferenceType InRefType = CPRT_None )
	:	Type(CPT_Struct), ArrayDim(1), PropertyFlags(0), PropertyExportFlags(PROPEXPORT_Public)
	, Struct(InStruct), MetaClass(NULL), ArrayIndexEnum(NULL), DelegateName(NAME_None), ReferenceType(InRefType)
	{}
	FPropertyBase( UProperty* Property, EPropertyReferenceType InRefType = CPRT_None )
	: PropertyExportFlags(PROPEXPORT_Public), DelegateName(NAME_None)
	{
		checkSlow(Property);

		UBOOL DynArray=0;
		QWORD PropagateFlags = 0;
		if( Property->GetClass()==UArrayProperty::StaticClass() )
		{
			DynArray = 1;
			// if we're an array, save up Parm flags so we can propagate them.
			// below the array will be assigned the inner property flags. This allows propagation of Parm flags (out, optional..)
			//@note: we need to explicitly specify CPF_Const instead of adding it to CPF_ParmFlags because CPF_ParmFlags is treated as exclusive;
			// i.e., flags that are in CPF_ParmFlags are not allowed in other variable types and vice versa
			PropagateFlags = Property->PropertyFlags & (CPF_ParmFlags | CPF_Const);
			Property = CastChecked<UArrayProperty>(Property)->Inner;
		}
		if( Property->GetClass()==UByteProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Byte);
			Enum = Cast<UByteProperty>(Property)->Enum;
		}
		else if( Property->GetClass()==UIntProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Int);
		}
		else if( Property->GetClass()==UBoolProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Bool);
			BitMask = Cast<UBoolProperty>(Property)->BitMask;
		}
		else if( Property->GetClass()==UFloatProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Float);
		}
		else if( Property->GetClass()==UClassProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_ObjectReference);
			PropertyClass = Cast<UClassProperty>(Property)->PropertyClass;
			MetaClass = Cast<UClassProperty>(Property)->MetaClass;
		}
		else if( Property->GetClass()==UComponentProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_ObjectReference);
			PropertyClass = Cast<UComponentProperty>(Property)->PropertyClass;
		}
		else if( Property->GetClass()==UObjectProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_ObjectReference);
			PropertyClass = Cast<UObjectProperty>(Property)->PropertyClass;
		}
		else if( Property->GetClass()==UNameProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Name);
		}
		else if( Property->GetClass()==UStrProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_String);
		}
		else if ( Property->GetClass() == UMapProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Map);
		}
		else if( Property->GetClass()==UStructProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Struct);
			Struct = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty)->Struct;
		}
		else if( Property->GetClass()==UDelegateProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Delegate);
			Function = Cast<UDelegateProperty>(Property)->Function;
		}
		else if ( Property->GetClass()==UInterfaceProperty::StaticClass() )
		{
			*this = FPropertyBase(CPT_Interface);
			PropertyClass = Cast<UInterfaceProperty>(Property)->InterfaceClass;
		}
		else
		{
			appErrorf( TEXT("Unknown property type '%s'"), *Property->GetFullName() );
		}
		ArrayDim = DynArray ? 0 : Property->ArrayDim;
		PropertyFlags = Property->PropertyFlags | PropagateFlags;
		ReferenceType = InRefType;
		ArrayIndexEnum = Property->ArraySizeEnum;
	}
	//@}

	/** @name Functions */
	//@{
	INT GetSize() const //hardcoded sizes!!
	{
		static const INT ElementSizes[CPT_MAX] =
		{
			0 /*None*/,					sizeof(BYTE),		sizeof(INT),			sizeof(UBOOL),
			sizeof(FLOAT),				sizeof(UObject*),	sizeof(FName),			sizeof(FScriptDelegate),
			sizeof(FScriptInterface),	0 /*Array*/,		0 /*Struct*/,			0 /*Vector*/,
			0 /*Rotator*/,				0 /*Map*/,			sizeof(TMap<BYTE,BYTE>)
		};

		INT ElementSize;
		if ( Type == CPT_Struct )
		{
			if ( Struct->GetFName() == NAME_Pointer )
			{
				ElementSize = sizeof(void*);
			}
			else
			{
				ElementSize = Struct->GetPropertiesSize();
			}
		}
		else
		{
			ElementSize = ElementSizes[Type];
		}

		return ElementSize * ArrayDim;
	}

	/**
	 * Returns whether this token represents a vector
	 * 
	 * @return	TRUE if this token represents a vector
	 */
	UBOOL IsVector() const
	{
		return Type==CPT_Struct && Struct->GetFName() == NAME_Vector;
	}
	
	/**
	 * Returns whether this token represents a rotator
	 * 
	 * @return	TRUE if this token represents a rotator
	 */
	UBOOL IsRotator() const
	{
		return Type==CPT_Struct && Struct->GetFName() == NAME_Rotator;
	}

	/**
	 * Returns whether this token represents a pointer
	 * 
	 * @return	TRUE if this token represents a pointer
	 */
	UBOOL IsPointer() const
	{
		return Type == CPT_Struct && Struct->GetFName() == NAME_Pointer;
	}

	/**
	 * Returns whether this token represents an object reference
	 */
	UBOOL IsObject() const
	{
		return Type == CPT_ObjectReference || Type == CPT_Interface;
	}

	/**
	 * Determines whether this token represents a dynamic array.
	 */
	UBOOL IsDynamicArray() const
	{
		return ArrayDim == 0;
	}

	/**
	 * Determines whether this token represents an enum property
	 */
	UBOOL IsEnumVariable() const
	{
		return Type == CPT_Byte && Enum != NULL;
	}

	/**
	 * Determines whether this token's type is compatible with another token's type.
	 *
	 * @param	Other							the token to check against this one.
	 *											Given the following example expressions, VarA is Other and VarB is 'this':
	 *												VarA = VarB;
	 *
	 *												function func(type VarB) {}
	 *												func(VarA);
	 *
	 *												static operator==(type VarB_1, type VarB_2) {}
	 *												if ( VarA_1 == VarA_2 ) {}
	 *
	 * @param	bDisallowGeneralization			controls whether it should be considered a match if this token's type is a generalization
	 *											of the other token's type (or vice versa, when dealing with structs
	 * @param	bIgnoreImplementedInterfaces	controls whether two types can be considered a match if one type is an interface implemented
	 *											by the other type.
	 */
	UBOOL MatchesType( const FPropertyBase& Other, UBOOL bDisallowGeneralization, UBOOL bIgnoreImplementedInterfaces=FALSE ) const
	{
		check(Type!=CPT_None || !bDisallowGeneralization);

		UBOOL bIsObjectType = IsObject();
		UBOOL bOtherIsObjectType = Other.IsObject();
		UBOOL bIsObjectComparison = bIsObjectType && bOtherIsObjectType;
		UBOOL bReverseClassChainCheck = TRUE;

		// If converting to an l-value, we require an exact match with an l-value.
		if( (PropertyFlags&CPF_OutParm) != 0 )
		{
			// if the other type is not an l-value, disallow
			if ( (Other.PropertyFlags&CPF_OutParm) == 0 )
			{
				return FALSE;
			}

			// if the other type is const and we are not const, disallow
			if ( (Other.PropertyFlags&CPF_Const) != 0 && (PropertyFlags&CPF_Const) == 0 )
			{
				return FALSE;
			}

			if ( Type == CPT_Struct )
			{
				// Allow derived structs to be passed by reference, unless this is a dynamic array of structs
				bDisallowGeneralization = bDisallowGeneralization || IsDynamicArray() || Other.IsDynamicArray();
			}

			// if Type == CPT_ObjectReference, out object function parm; allow derived classes to be passed in
			// if Type == CPT_Interface, out interface function parm; allow derived classes to be passed in
			else if ( (PropertyFlags & CPF_Const) == 0 || !IsObject() )
			{
				// all other variable types must match exactly when passed as the value to an 'out' parameter
				bDisallowGeneralization = TRUE;
			}
			
			// both types are objects, but one is an interface and one is an object reference
			else if ( bIsObjectComparison && Type != Other.Type )
			{
				return FALSE;
			}
		}
		else if (Type == CPT_ObjectReference && Other.Type != CPT_Interface
			&&	(ReferenceType == CPRT_AssignmentReference || ReferenceType == CPRT_DualReference || (PropertyFlags & CPF_ReturnParm)))
		{
			bReverseClassChainCheck = FALSE;
		}

		// Check everything.
		if( Type==CPT_None && (Other.Type==CPT_None || !bDisallowGeneralization) )
		{
			// If Other has no type, accept anything.
			return TRUE;
		}
		else if( Type != Other.Type && !bIsObjectComparison )
		{
			// Mismatched base types.
			return FALSE;
		}
		else if( ArrayDim != Other.ArrayDim )
		{
			// Mismatched array dimensions.
			return FALSE;
		}
		else if( Type==CPT_Byte )
		{
			// Make sure enums match, or we're generalizing.
			return Enum==Other.Enum || (Enum==NULL && !bDisallowGeneralization);
		}
		else if( bIsObjectType )
		{
			check(PropertyClass!=NULL);

			// Make sure object types match, or we're generalizing.
			if( bDisallowGeneralization )
			{
				// Exact match required.
				return PropertyClass==Other.PropertyClass && MetaClass==Other.MetaClass;
			}
			else if( Other.PropertyClass==NULL )
			{
				// Cannonical 'None' matches all object classes.
				return TRUE;
			}
			else
			{
				// Generalization is ok (typical example of this check would look like: VarA = VarB;, where this is VarB and Other is VarA)
				if ( Other.PropertyClass->IsChildOf(PropertyClass) )
				{
					if ( !bIgnoreImplementedInterfaces || ((Type == CPT_Interface) == (Other.Type == CPT_Interface)) )
					{
						if ( PropertyClass!=UClass::StaticClass() || MetaClass == NULL || Other.MetaClass->IsChildOf(MetaClass) ||
							(bReverseClassChainCheck && (Other.MetaClass == NULL || MetaClass->IsChildOf(Other.MetaClass))) )
						{
							return TRUE;
						}
					}
				}
				// check the opposite class chain for object types
				else if (bReverseClassChainCheck && Type != CPT_Interface && bIsObjectComparison && PropertyClass != NULL && PropertyClass->IsChildOf(Other.PropertyClass))
				{
					if (Other.PropertyClass!=UClass::StaticClass() || MetaClass == NULL || Other.MetaClass == NULL || MetaClass->IsChildOf(Other.MetaClass) || Other.MetaClass->IsChildOf(MetaClass))
					{
						return TRUE;
					}
				}

				if ( PropertyClass->HasAnyClassFlags(CLASS_Interface) && !bIgnoreImplementedInterfaces )
				{
					if ( Other.PropertyClass->ImplementsInterface(PropertyClass) )
					{
						return TRUE;
					}
				}

				return FALSE;
			}
		}
		else if( Type==CPT_Struct )
		{
			check(Struct!=NULL);
			check(Other.Struct!=NULL);

			if ( Struct == Other.Struct )
			{
				// struct types match exactly 
				return TRUE;
			}

			// returning FALSE here prevents structs related through inheritance from being used interchangeably, such as passing a derived struct as the value for a parameter
			// that expects the base struct, or vice versa.  An easier example is assignment (e.g. Vector = Plane or Plane = Vector).
			// there are two cases to consider (let's use vector and plane for the example):
			// - Vector = Plane;
			//		in this expression, 'this' is the vector, and Other is the plane.  This is an unsafe conversion, as the destination property type is used to copy the r-value to the l-value
			//		so in this case, the VM would call CopyCompleteValue on the FPlane struct, which would copy 16 bytes into the l-value's buffer;  However, the l-value buffer will only be
			//		12 bytes because that is the size of FVector
			// - Plane = Vector;
			//		in this expression, 'this' is the plane, and Other is the vector.  This is a safe conversion, since only 12 bytes would be copied from the r-value into the l-value's buffer
			//		(which would be 16 bytes).  The problem with allowing this conversion is that what to do with the extra member (e.g. Plane.W); should it be left alone? should it be zeroed?
			//		difficult to say what the correct behavior should be, so let's just ignore inheritance for the sake of determining whether two structs are identical

			// Previously, the logic for determining whether this is a generalization of Other was reversed; this is very likely the culprit behind all current issues with 
			// using derived structs interchangeably with their base versions.  The inheritance check has been fixed; for now, allow struct generalization and see if we can find any further
			// issues with allowing conversion.  If so, then we disable all struct generalization by returning FALSE here.
 			// return FALSE;

			if ( bDisallowGeneralization )
			{
				return FALSE;
			}
			// if we don't need an exact match, see if we're generalizing.
			else
			{
				// Generalization is ok if this is not a dynamic array
				if ( !IsDynamicArray() && !Other.IsDynamicArray() )
				{
					if ( Other.Struct->IsChildOf(Struct) )
					{
						// this is the old behavior - it would allow memory overwrites if you assigned a derived value to a base variable; e.g. Vector = Plane;
// 						debugfSuppressed(NAME_DevCompile, TEXT("FPropertyBase::MatchesType PREVENTING match - Src:%s Destination:%s"), *Other.Struct->GetPathName(), *Struct->GetPathName());
					}
					else if ( Struct->IsChildOf(Other.Struct) )
					{
// 						debugfSuppressed(NAME_DevCompile, TEXT("FPropertyBase::MatchesType ALLOWING match - Src:%s Destination:%s"), *Other.Struct->GetPathName(), *Struct->GetPathName());
						return TRUE;
					}
				}

				return FALSE;
			}
		}
		else
		{
			// General match.
			return TRUE;
		}
	}

	FString Describe()
	{
		return FString::Printf(
			TEXT("Type:%s  Flags:%lli  Enum:%s  PropertyClass:%s  Struct:%s  Function:%s  MetaClass:%s"),
			GetPropertyTypeText(Type), PropertyFlags,
			Enum!=NULL?*Enum->GetName():TEXT(""),
			PropertyClass!=NULL?*PropertyClass->GetName():TEXT("NULL"),
			Struct!=NULL?*Struct->GetName():TEXT("NULL"),
			Function!=NULL?*Function->GetName():TEXT("NULL"),
			MetaClass!=NULL?*MetaClass->GetName():TEXT("NULL")
			);
	}
	//@}
};

//
// Token types.
//
enum ETokenType
{
	TOKEN_None				= 0x00,		// No token.
	TOKEN_Identifier		= 0x01,		// Alphanumeric identifier.
	TOKEN_Symbol			= 0x02,		// Symbol.
	TOKEN_Const				= 0x03,		// A constant.
	TOKEN_Max				= 0x0D
};

/*-----------------------------------------------------------------------------
	FToken.
-----------------------------------------------------------------------------*/
//
// Information about a token that was just parsed.
//
class FToken : public FPropertyBase
{
public:
	/** @name Variables */
	//@{
	/** Type of token. */
	ETokenType				TokenType;
	/** Name of token. */
	FName					TokenName;
	/** Starting position in script where this token came from. */
	INT						StartPos;
	/** Starting line in script. */
	INT						StartLine;
	/** Always valid. */
	TCHAR					Identifier[NAME_SIZE];
	/** property that corresponds to this FToken - null if this Token doesn't correspond to a UProperty */
	UProperty*				TokenProperty;
	/** function that corresponds to this FToken - null if this Token doesn't correspond to a function */
	class FFunctionData*	TokenFunction;
	union
	{
		// TOKEN_Const values.
		BYTE	Byte;								 // If CPT_Byte.
		INT		Int;								 // If CPT_Int.
		UBOOL	Bool;								 // If CPT_Bool.
		FLOAT	Float;								 // If CPT_Float.
		UObject* Object;							 // If CPT_ObjectReference or CPT_Interface.
		BYTE	NameBytes[sizeof(FName)];			 // If CPT_Name.
		TCHAR	String[MAX_STRING_CONST_SIZE];		 // If CPT_String or IsPointer().
		BYTE	VectorBytes[sizeof(FVector)];		 // If CPT_Struct && IsVector().
		BYTE	RotationBytes[sizeof(FRotator)];	 // If CPT_Struct && IsRotator().
		UBOOL	Interface;
	};
	//@}

	/**
	 * Copies the properties from this token into another.
	 *
	 * @param	Other	the token to copy this token's properties to.
	 */
	void Clone( const FToken& Other );

	FString GetValue()
	{
		if ( TokenType == TOKEN_Const )
		{
			if (IsVector())
			{
				FVector& Vect = *(FVector*)VectorBytes;
				return FString::Printf(TEXT("FVector(%f,%f,%f)"),Vect.X, Vect.Y, Vect.Z);
			}
			else if (IsRotator())
			{
				FRotator& Rot = *(FRotator*)RotationBytes;
				return FString::Printf(TEXT("FRotator(%i,%i,%i)"), Rot.Pitch, Rot.Yaw, Rot.Roll);
			}
			else
			{
				switch ( Type )
				{
				case CPT_Byte:				return FString::Printf(TEXT("%u"), Byte);
				case CPT_Int:				return FString::Printf(TEXT("%i"), Int);
				// Don't use GTrue/GFalse here because they can be localized
				case CPT_Bool:				return FString::Printf(TEXT("%s"), Bool ? *(FName::GetEntry(NAME_TRUE)->GetNameString()) : *(FName::GetEntry(NAME_FALSE)->GetNameString()));
				case CPT_Float:				return FString::Printf(TEXT("%f"), Float);
				case CPT_Name:				return FString::Printf(TEXT("%s"), *(*(FName*)NameBytes).ToString());
				case CPT_String:			return String;
				case CPT_Vector:
					{
						FVector& Vect = *(FVector*)VectorBytes;
						return FString::Printf(TEXT("FVector(%f,%f,%f)"),Vect.X, Vect.Y, Vect.Z);
					}
				case CPT_Rotation:
					{
						FRotator& Rot = *(FRotator*)RotationBytes;
						return FString::Printf(TEXT("FRotator(%i,%i,%i)"), Rot.Pitch, Rot.Yaw, Rot.Roll);
					}

				// unsupported
				case CPT_Range:
				case CPT_Struct:
				case CPT_ObjectReference:
				case CPT_Interface:
				case CPT_Delegate:
					return TEXT("");

				}

				return TEXT("???");
			}
		}
		else
		{
			return TEXT("N/A");
		}
	}

	// Constructors.
	FToken() : FPropertyBase(CPT_None, CPRT_None), TokenProperty(NULL), TokenFunction(NULL)
	{
		InitToken(CPT_None, CPRT_None);
	}
	FToken( EPropertyReferenceType InRefType )
	: FPropertyBase( CPT_None, InRefType ), TokenProperty(NULL), TokenFunction(NULL)
	{
		InitToken( CPT_None, InRefType );
	}
	FToken( UProperty* Property, EPropertyReferenceType InRefType = CPRT_None )
	: FPropertyBase(Property, InRefType), TokenProperty(Property), TokenFunction(NULL)
	{
		StartPos = 0;
		StartLine = 0;

		TokenName = TokenProperty->GetFName();
		appStrncpy(Identifier, *TokenName.ToString(), NAME_SIZE);
		TokenType = TOKEN_Identifier;
		appMemzero(String, MAX_STRING_CONST_SIZE);
	}

	// copy constructors
	FToken( const FPropertyBase& InType )
	: FPropertyBase( CPT_None, InType.ReferenceType ), TokenProperty(NULL), TokenFunction(NULL)
	{
		InitToken( CPT_None, InType.ReferenceType );
		(FPropertyBase&)*this = InType;
	}

	// Inlines.
	void InitToken( EPropertyType InType, EPropertyReferenceType InRefType = CPRT_None )
	{
		(FPropertyBase&)*this = FPropertyBase(InType, InRefType);
		TokenType		= TOKEN_None;
		TokenName		= NAME_None;
		StartPos		= 0;
		StartLine		= 0;
		*Identifier		= 0;
		appMemzero(String, sizeof(Identifier));
	}
	UBOOL Matches( const TCHAR* Str ) const
	{
		return (TokenType==TOKEN_Identifier || TokenType==TOKEN_Symbol) && appStricmp(Identifier,Str)==0;
	}
	UBOOL Matches( const FName& Name ) const
	{
		return TokenType==TOKEN_Identifier && TokenName==Name;
	}
	void AttemptToConvertConstant( const FPropertyBase& NewType )
	{
		check(TokenType==TOKEN_Const);
		switch( NewType.Type )
		{
		case CPT_Int:		{INT        V(0);           if( GetConstInt     (V) ) SetConstInt     (V); break;}
		case CPT_Bool:		{UBOOL      V(0);           if( GetConstBool    (V) ) SetConstBool    (V); break;}
		case CPT_Float:		{FLOAT      V(0.f);         if( GetConstFloat   (V) ) SetConstFloat   (V); break;}
		case CPT_Name:		{FName      V(NAME_None);   if( GetConstName    (V) ) SetConstName    (V); break;}
		case CPT_Struct:
			{
				if( NewType.IsVector() )
				{
					FVector V( 0.f, 0.f, 0.f );
					if( GetConstVector( V ) )
					{
						SetConstVector( V );
					}
				}
				else if( NewType.IsRotator() )
				{
					FRotator V( 0, 0, 0 );
					if( GetConstRotation( V ) )
					{
						SetConstRotation( V );
					}
				}
				//!!struct conversion support would be nice
				break;
			}
		case CPT_String:
			{
				break;
			}
		case CPT_Byte:
			{
				BYTE V=0;
				if( NewType.Enum==NULL && GetConstByte(V) )
				{
					SetConstByte(NULL,V); 
				}
				break;
			}
		case CPT_ObjectReference:
			{
				UObject* Ob=NULL; 
				if( GetConstObject( NewType.PropertyClass, Ob ) )
				{
					SetConstObject( Ob ); 
				}
				break;
			}
		}
	}

	// Setters.

	void SetTokenProperty( UProperty* Property )
	{
		TokenProperty = Property;
		TokenName = Property->GetFName();
		appStrcpy(Identifier, *TokenName.ToString());
		TokenType = TOKEN_Identifier;
	}

	void SetTokenFunction( FFunctionData* Function )
	{
		TokenFunction = Function;
	}
	void SetConstByte( UEnum* InEnum, BYTE InByte )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Byte);
		Enum			= InEnum;
		Byte			= InByte;
		TokenType		= TOKEN_Const;
	}
	void SetConstInt( INT InInt )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Int);
		Int				= InInt;
		TokenType		= TOKEN_Const;
	}
	void SetConstBool( UBOOL InBool )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Bool);
		Bool 			= InBool;
		TokenType		= TOKEN_Const;
	}
	void SetConstFloat( FLOAT InFloat )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Float);
		Float			= InFloat;
		TokenType		= TOKEN_Const;
	}
	void SetConstObject( UObject* InObject )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_ObjectReference);
		PropertyClass	= InObject ? InObject->GetClass() : NULL;
		Object			= InObject;
		TokenType		= TOKEN_Const;
		if( PropertyClass==UClass::StaticClass() )
		{
			MetaClass = CastChecked<UClass>(InObject);
		}
	}
	void SetConstDelegate()
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Delegate);
		TokenType = TOKEN_Const;
	}
	void SetConstName( FName InName )
	{
		(FPropertyBase&)*this = FPropertyBase(CPT_Name);
		*(FName *)NameBytes = InName;
		TokenType		= TOKEN_Const;
	}
	void SetConstString( TCHAR* InString, INT MaxLength=MAX_STRING_CONST_SIZE )
	{
		check(MaxLength>0);
		(FPropertyBase&)*this = FPropertyBase(CPT_String);
		if( InString != String )
		{
			appStrncpy( String, InString, MaxLength );
		}
		TokenType = TOKEN_Const;
	}
	void SetConstVector( FVector &InVector )
	{
		(FPropertyBase&)*this   = FPropertyBase(CPT_Struct);
		static UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Vector"));
		Struct                  = VectorStruct;
		*(FVector *)VectorBytes = InVector;
		TokenType		        = TOKEN_Const;
	}
	void SetConstRotation( FRotator &InRotation )
	{
		(FPropertyBase&)*this      = FPropertyBase(CPT_Struct);
		static UScriptStruct* RotatorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Rotator"));
		Struct                     = RotatorStruct;
		*(FRotator *)RotationBytes = InRotation;
		TokenType		           = TOKEN_Const;
	}
	//!!struct constants

	// Getters.
	UBOOL GetConstByte( BYTE& B ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Byte )
		{
			B = Byte;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Int && Int>=0 && Int<256 )
		{
			B = (BYTE) Int;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Float && Float>=0 && Float<255 && Float==appTrunc(Float))
		{
			B = (BYTE) Float;
			return 1;
		}
		else return 0;
	}
	UBOOL GetConstInt( INT& I ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Int )
		{
			I = Int;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Byte )
		{
			I = Byte;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Float && Float==appTrunc(Float))
		{
			I = (INT) Float;
			return 1;
		}
		else return 0;
	}
	UBOOL GetConstBool( UBOOL& B ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Bool )
		{
			B = Bool;
			return 1;
		}
		else return 0;
	}
	UBOOL GetConstFloat( FLOAT& R ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Float )
		{
			R = Float;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Int )
		{
			R = Int;
			return 1;
		}
		else if( TokenType==TOKEN_Const && Type==CPT_Byte )
		{
			R = Byte;
			return 1;
		}
		else return 0;
	}
	UBOOL GetConstObject( UClass* DesiredClass, UObject*& Ob ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_ObjectReference && (DesiredClass==NULL || PropertyClass->IsChildOf(DesiredClass)) )
		{
			Ob = Object;
			return 1;
		}
		return 0;
	}
	UBOOL GetConstName( FName& n ) const
	{
		if( TokenType==TOKEN_Const && Type==CPT_Name )
		{
			n = *(FName *)NameBytes;
			return 1;
		}
		return 0;
	}
	UBOOL GetConstVector( FVector& v ) const
	{
		if( TokenType==TOKEN_Const && IsVector() )
		{
			v = *(FVector *)VectorBytes;
			return 1;
		}
		return 0;
	}
	//!!struct constants
	UBOOL GetConstRotation( FRotator& r ) const
	{
		if( TokenType==TOKEN_Const && IsRotator() )
		{
			r = *(FRotator *)RotationBytes;
			return 1;
		}
		return 0;
	}

	FString Describe()
	{
		return FString::Printf(
			TEXT("Property:%s  Type:%s  TokenName:%s  Value:%s  Struct:%s  Flags:%lli  RefType:%s"),
			TokenProperty!=NULL?*TokenProperty->GetName():TEXT("NULL"),
			GetPropertyTypeText(Type), *TokenName.ToString(), *GetValue(),
			Struct!=NULL?*Struct->GetName():TEXT("NULL"),
			PropertyFlags, GetPropertyRefText(ReferenceType)
			);
	}
};

/**
 * A group of FTokens.  Used for keeping track of reference chains tokens
 * e.g. SomeObject.default.Foo.DoSomething()
 */
class FTokenChain : public TArray<FToken>
{
public:
	FToken& operator+=( const FToken& NewToken )
	{
		FToken& Token = (*this)(AddZeroed()) = NewToken;
		return Token;
	}
};

/**
 * Information about a function being compiled.
 */
struct FFuncInfo
{
	/** @name Variables */
	//@{
	/** Name of the function or operator. */
	FToken		Function;
	/** Binary operator precedence. */
	INT			Precedence;
	/** Function flags. */
	DWORD		FunctionFlags;
	/** Function flags which are only required for exporting */
	DWORD		FunctionExportFlags;
	/** Index of native function. */
	INT			iNative;
	/** Number of parameters expected for operator. */
	INT			ExpectParms;
	/** Pointer to the UFunction corresponding to this FFuncInfo */
	UFunction*	FunctionReference;
	//@}

	/** Constructor. */
	FFuncInfo()
	:	Function		()
	,	Precedence		(0)
	,	FunctionFlags   (0)
	,	FunctionExportFlags(0)
	,	iNative			(0)
	,	ExpectParms		(0)
	,	FunctionReference(NULL)
	{}

	FFuncInfo( const FFuncInfo& Other )
	:	Precedence		(Other.Precedence)
	,	FunctionFlags   (Other.FunctionFlags)
	,	FunctionExportFlags(Other.FunctionExportFlags)
	,	iNative			(Other.iNative)
	,	ExpectParms		(Other.ExpectParms)
	,	FunctionReference(Other.FunctionReference)
	{
		Function.Clone(Other.Function);
	}
};

/**
 * Information about a state being compiled.
 */
struct FStateInfo
{
	/** @name Variables */
	//@{
	/** Name of the state. */
	FToken StateToken;
	/** Pointer to the UState corresponding to this FStateInfo. */
	UState* StateReference;
	//@}

	/** Constructor. */
	FStateInfo()
	: StateToken(), StateReference(NULL)
	{}

	FStateInfo(const FStateInfo& Other)
	: StateReference(Other.StateReference)
	{
		StateToken.Clone(Other.StateToken);
	}
};

/**
 * Struct for storing information about an expression that is being used as the default value for an optional parameter.
 */
struct FDefaultParameterValue
{
	/** the original text for this expression - useful for complex expressions (such as context expressions, etc.) */
	FString			RawExpression;

	/** 
	 * Contains the FTokens that belong to each "chunk" of the complete expression.
	 * e.g. given the complete expression 'SomeObject.default.Foo', ParsedExpression will
	 * contain 3 members
	 */
	FTokenChain			ParsedExpression;

	/**
	 * The evaluated bytecode for the complete expression
	 */
	TArray<BYTE>	EvaluatedExpression;

	INT				InputLine;
	INT				InputPos;


	/**
	 * Copies the stored bytecode for the default value expression into the buffer specified
	 * 
	 * @param	Stream	the buffer to insert the stream into - typically TopNode->Script
	 * @return	whether a value was emitted
	 */
	UBOOL EmitValue( TArray<BYTE>& Stream )
	{
		if ( EvaluatedExpression.Num() )
		{
			INT iStart = Stream.Add(EvaluatedExpression.Num());
			appMemcpy( &Stream(iStart), &EvaluatedExpression(0), EvaluatedExpression.Num() );
			return true;
		}

		return false;
	}

	FDefaultParameterValue()
	: InputLine(1), InputPos(1)
	{}
};

/**
 * Stores "compiler" data about an FToken.  "Compiler" data is data that is associated with a
 * specific property, function or class that is only needed during script compile.
 * This class is designed to make adding new compiler data very simple.
 *
 * - stores the raw evaluated bytecode associated with an FToken
 */
struct FTokenData
{
	/** The token tracked by this FTokenData. */
	FToken			Token;

	/** For optional function parameters, holds the default value data */
	TScopedPointer<FDefaultParameterValue> DefaultValue;
	
	/**
	 * Creates a FDefaultParameterValue for this FTokenData.
	 *
	 * @return	the default value associated with this token
	 */
	FDefaultParameterValue* SetDefaultValue()
	{
		if ( !DefaultValue.IsValid() )
		{
			DefaultValue.Reset(new FDefaultParameterValue());
		}

		return DefaultValue;
	}

	/**
	 * Copies the stored bytecode for the expression represented by this FTokenData
	 * into the buffer specified
	 * 
	 * @param	Stream	the buffer to insert the stream into - typically TopNode->Script
	 * @return	whether a value was emitted
	 */
	UBOOL EmitValue( TArray<BYTE>& Stream )
	{
		UBOOL bResult = FALSE;

		if ( DefaultValue.IsValid() )
		{
			bResult = DefaultValue->EmitValue(Stream);
		}

		return bResult;
	}

	/** @name Constructors */
	//@{
	/**
	 * Defalt constructor
	 */
	FTokenData() : DefaultValue(NULL)
	{}

	/**
	 * Copy constructor
	 */
	FTokenData( const FToken& inToken )
	: Token(inToken), DefaultValue(NULL)
	{}
	//@}
};

/**
 * Class for storing data about a list of properties.  Though FToken contains a reference to its
 * associated UProperty, it's faster lookup to use the UProperty as the key in a TMap.
 */
class FPropertyData : public TMap< UProperty*, TScopedPointer<FTokenData> >
{
	typedef TMap<UProperty*, TScopedPointer<FTokenData> >	Super;

public:
	/**
	 * Returns the value associated with a specified key.
	 * @param	Key - The key to search for.
	 * @return	A pointer to the value associated with the specified key, or NULL if the key isn't contained in this map.  The pointer
	 *			is only valid until the next change to any key in the map.
	 */
	FTokenData* Find(UProperty* Key)
	{
		FTokenData* Result = NULL;

		TScopedPointer<FTokenData>* pResult = Super::Find(Key);
		if ( pResult != NULL )
		{
			Result = pResult->GetOwnedPointer();
		}
		return Result;
	}
	const FTokenData* Find(UProperty* Key) const
	{
		const FTokenData* Result = NULL;

		const TScopedPointer<FTokenData>* pResult = Super::Find(Key);
		if ( pResult != NULL )
		{
			Result = pResult->GetOwnedPointer();
		}
		return Result;
	}

	/**
	 * Sets the value associated with a key.  If the key already exists in the map, uses the same
	 * value pointer and reinitalized the FTokenData with the input value.
	 *
	 * @param	InKey	the property to get a token wrapper for
	 * @param	InValue	the token wrapper for the specified key
	 *
	 * @return	a pointer to token data created associated with the property
	 */
	FTokenData* Set(UProperty* InKey, const FTokenData& InValue)
	{
		FTokenData* Result = NULL;

		TScopedPointer<FTokenData>* pResult = Super::Find(InKey);
		if ( pResult != NULL )
		{
			Result = pResult->GetOwnedPointer();
			*Result = FTokenData(InValue);
		}
		else
		{
			pResult = &Super::Set(InKey, new FTokenData(InValue));
			Result = *pResult;
		}

		return Result;

// 		// Remove existing values associated with the specified key.
// 		// This is only necessary if the TSet allows duplicate keys; otherwise TSet::Add replaces the existing key-value pair.
// 		if(KeyFuncs::bAllowDuplicateKeys)
// 		{
// 			for(typename PairSetType::TKeyIterator It(Pairs,InKey);It;++It)
// 			{
// 				It.RemoveCurrent();
// 			}
// 		}
// 
// 		// Add the key-value pair to the set.  TSet::Add will replace any existing key-value pair that has the same key.
// 		const FSetElementId PairId = Pairs.Add(FPairInitializer(InKey,InValue));
// 
// 		return Pairs(PairId).Value;
	}

	/**
	 * (debug) Dumps the values of this FPropertyData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump( INT Indent )
	{
		for ( TMap<UProperty*, TScopedPointer<FTokenData> >::TIterator It(*this); It; ++It )
		{
			TScopedPointer<FTokenData>& PointerVal = It.Value();
			FToken& Token = PointerVal.GetOwnedPointer()->Token;
			if ( Token.Type != CPT_None )
			{
				debugf(TEXT("%s%s"), appSpc(Indent), *Token.Describe());
			}
		}
	}
};

/**
 * Class for storing additional data about compiled structs and struct properties
 */
class FStructData
{
public:
	/** info about the struct itself */
	FToken			StructData;

private:
	/** info for the properties contained in this struct */
	FPropertyData	StructPropertyData;

public:
	/**
	 * Adds a new struct property token
	 * 
	 * @param	PropertyToken	token that should be added to the list
	 */
	void AddStructProperty( const FToken& PropertyToken )
	{
		check(PropertyToken.TokenProperty);
		StructPropertyData.Set(PropertyToken.TokenProperty, PropertyToken);
	}

	FPropertyData& GetStructPropertyData()
	{
		return StructPropertyData;
	}
	const FPropertyData& GetStructPropertyData() const
	{
		return StructPropertyData;
	}

	/**
	* (debug) Dumps the values of this FStructData to the log file
	* 
	* @param	Indent	number of spaces to insert at the beginning of each line
	*/	
	void Dump( INT Indent )
	{
		debugf(TEXT("%s%s"), appSpc(Indent), *StructData.Describe());

		debugf(TEXT("%sproperties:"), appSpc(Indent));
		StructPropertyData.Dump(Indent + 4);
	}

	/** Constructor */
	FStructData( const FToken& StructToken ) : StructData(StructToken) {}
};

/**
 * Class for storing additional data about compiled function properties.
 */
class FFunctionData
{
	/** info about the function associated with this FFunctionData */
	FFuncInfo		FunctionData;

	/** return value for this function */
	FTokenData		ReturnTypeData;

	/** function parameter data */
	FPropertyData	ParameterData;

	/** function local property data */
	FPropertyData	LocalData;

	/**
	 * Adds a new parameter token
	 * 
	 * @param	PropertyToken	token that should be added to the list
	 */
	void AddParameter( const FToken& PropertyToken )
	{
		check(PropertyToken.TokenProperty);
		ParameterData.Set(PropertyToken.TokenProperty, PropertyToken);
	}

	/**
	 * Adds a new local property token
	 * 
	 * @param	PropertyToken	token that should be added to the list
	 */
	void AddLocalProperty( const FToken& PropertyToken )
	{
		check(PropertyToken.TokenProperty);
		LocalData.Set(PropertyToken.TokenProperty, PropertyToken);
	}

	/**
	 * Sets the value of the return token for this function
	 * 
	 * @param	PropertyToken	token that should be added
	 */
	void SetReturnData( const FToken& PropertyToken )
	{
		check(PropertyToken.TokenProperty);
		ReturnTypeData.Token = PropertyToken;
	}

public:
	/** Constructors */
	FFunctionData() {}
	FFunctionData( const FFunctionData& Other )
	{
		(*this) = Other;
	}
	FFunctionData( const FFuncInfo& inFunctionData )
	: FunctionData(inFunctionData)
	{}

	/** Copy operator */
	FFunctionData& operator=( const FFunctionData& Other )
	{
		FunctionData	= Other.FunctionData;
		ParameterData	= Other.ParameterData;
		LocalData		= Other.LocalData;
		ReturnTypeData.Token.Clone(Other.ReturnTypeData.Token);
		return *this;
	}
	
	/** @name getters */
	//@{
	const	FFuncInfo&		GetFunctionData()	const	{	return FunctionData;	}
	const	FToken&			GetReturnData()		const	{	return ReturnTypeData.Token;	}
	const	FPropertyData&	GetParameterData()	const	{	return ParameterData;	}
	const	FPropertyData&	GetLocalData()		const	{	return LocalData;		}
	FPropertyData&			GetParameterData()			{	return ParameterData;	}
	FPropertyData&			GetLocalData()				{	return LocalData;		}
	FTokenData* GetReturnTokenData() { return &ReturnTypeData; }
	//@}

	/**
	 * Adds a new function property to be tracked.  Determines whether the property is a
	 * function parameter, local property, or return value, and adds it to the appropriate
	 * list
	 * 
	 * @param	PropertyToken	the property to add
	 */
	void AddProperty( const FToken& PropertyToken )
	{
		const UProperty* Prop = PropertyToken.TokenProperty;
		check(Prop);

		if ( (Prop->PropertyFlags&CPF_ReturnParm) != 0 )
		{
			SetReturnData(PropertyToken);
		}
		else if ( (Prop->PropertyFlags&CPF_Parm) != 0 )
		{
			AddParameter(PropertyToken);
		}
		else
		{
			AddLocalProperty(PropertyToken);
		}
	}

	/**
	 * Empties the list of local property data for this function.  Called once the function is out of scope to reduce unecessary memory bloat.
	 */
	void ClearLocalPropertyData()
	{
		LocalData.Empty();
		LocalData.Shrink();
	}

	/**
	 * (debug) Dumps the values of this FFunctionData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump( INT Indent )
	{
		debugf(TEXT("%slocals:"), appSpc(Indent));
		LocalData.Dump(Indent + 4);

		debugf(TEXT("%sparameters:"), appSpc(Indent));
		ParameterData.Dump(Indent + 4);

		debugf(TEXT("%sreturn prop:"), appSpc(Indent));
		if ( ReturnTypeData.Token.Type != CPT_None )
		{
			debugf(TEXT("%s%s"), appSpc(Indent + 4), *ReturnTypeData.Token.Describe());
		}
	}

	/**
	 * Sets the specified function export flags
	 */
	void SetFunctionExportFlag( DWORD NewFlags )
	{
		FunctionData.FunctionExportFlags |= NewFlags;
	}

	/**
	 * Clears the specified function export flags
	 */
	void ClearFunctionExportFlags( DWORD ClearFlags )
	{
		FunctionData.FunctionExportFlags &= ~ClearFlags;
	}
};

/**
 * Class for storing additional data about compiled state properties.
 */
class FStateData
{
public:
	/** info about the state itself */
	FStateInfo		StateData;

private:
	/** state local property data */
	FPropertyData	LocalData;

public:
	/** Constructors */
	FStateData() {}
	FStateData(const FStateData& Other)
	{
		(*this) = Other;
	}
	FStateData(const FStateInfo& InStateData)
	: StateData(InStateData)
	{}

	/** Copy operator */
	FStateData& operator=(const FStateData& Other)
	{
		StateData = Other.StateData;
		LocalData = Other.LocalData;
		return *this;
	}

	/** @name getters */
	//@{
	const FPropertyData& GetLocalData() const
	{
		return LocalData;
	}
	FPropertyData& GetLocalData()
	{
		return LocalData;
	}
	//@}

	/**
	 * Adds a new local property token
	 *
	 * @param	PropertyToken	token that should be added to the list
	 */
	void AddLocalProperty(const FToken& PropertyToken)
	{
		check(PropertyToken.TokenProperty);
		LocalData.Set(PropertyToken.TokenProperty, PropertyToken);
	}

	/**
	 * (debug) Dumps the values of this FStateData to the log file
	 *
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */
	void Dump(INT Indent)
	{
		debugf(TEXT("%s%s"), appSpc(Indent), *StateData.StateToken.Describe());

		debugf(TEXT("%slocals:"), appSpc(Indent));
		LocalData.Dump(Indent + 4);
	}
};

/**
 * Tracks information about a multiple inheritance parent declaration for native script classes.
 */
struct FMultipleInheritanceBaseClass
{
	/**
	 * The name to use for the base class when exporting the script class to header file.
	 */
	FString ClassName;

	/**
	 * For multiple inheritance parents declared using 'Implements', corresponds to the UClass for the interface.  For multiple inheritance parents declared
	 * using 'Inherits', this value will be NULL.
	 */
	UClass* InterfaceClass;

	/**
	 * Constructors
	 */
	FMultipleInheritanceBaseClass(const FString& BaseClassName)
	: ClassName(BaseClassName), InterfaceClass(NULL)
	{}

	FMultipleInheritanceBaseClass(UClass* ImplementedInterfaceClass)
	: InterfaceClass(ImplementedInterfaceClass)
	{
		ClassName = FString::Printf(TEXT("I%s"), *ImplementedInterfaceClass->GetName());
	}
};

/**
 * Class for storing compiler metadata about a class's properties.
 */
class FClassMetaData
{
	/** member properties for this class */
	FPropertyData											GlobalPropertyData;

	/** structs declared in this class */
	TMap< UScriptStruct*, TScopedPointer<FStructData> >		StructData;
	/** functions of this class */
	TMap< UFunction*, TScopedPointer<FFunctionData> >		FunctionData;
	/** states of this class */
	TMap< UState*, TScopedPointer<FStateData> >				StateData;

	/** base classes to multiply inherit from (other than the main base class */
	TArray<FMultipleInheritanceBaseClass>					MultipleInheritanceParents;

	/** whether this class declares delegate functions or properties */
	UBOOL													bContainsDelegates;

public:
	/** Default constructor */
	FClassMetaData()
	: bContainsDelegates(FALSE)
	{
	}

	/**
	 * Adds a new function to be tracked
	 * 
	 * @param	FunctionInfo	the function to add
	 *
	 * @return	a pointer to the newly added FFunctionData
	 */
	FFunctionData* AddFunction( const FFuncInfo& FunctionInfo )
	{
		check(FunctionInfo.FunctionReference!=NULL);

		FFunctionData* Result = NULL;

		TScopedPointer<FFunctionData>* pFuncData = FunctionData.Find(FunctionInfo.FunctionReference);
		if ( pFuncData != NULL )
		{
			Result = pFuncData->GetOwnedPointer();
			*Result = FFunctionData(FunctionInfo);
		}
		else
		{
			pFuncData = &FunctionData.Set(FunctionInfo.FunctionReference, new FFunctionData(FunctionInfo));
			Result = pFuncData->GetOwnedPointer();
		}

		// update optimization flags
		bContainsDelegates = bContainsDelegates || FunctionInfo.FunctionReference->HasAnyFunctionFlags(FUNC_Delegate);

		return Result;
	}

 	/**
	 * Adds a new state to be tracked
	 *
	 * @param	StateInfo		the state to add
	 *
	 * @return	a pointer to the newly added FStateData
	 */
	FStateData* AddState(const FStateInfo& StateInfo)
	{
		check(StateInfo.StateReference != NULL);

		FStateData* Result = NULL;

		TScopedPointer<FStateData>* pStateData = StateData.Find(StateInfo.StateReference);
		if (pStateData != NULL)
		{
			Result = pStateData->GetOwnedPointer();
			*Result = FStateData(StateInfo);
		}
		else
		{
			pStateData = &StateData.Set(StateInfo.StateReference, new FStateData(StateInfo));
			Result = pStateData->GetOwnedPointer();
		}

		return Result;
	}

	/**
	 * Adds a new struct to be tracked
	 * 
	 * @param	StructToken		the token for the struct to add
	 *
	 * @return	a pointer to the newly added FStructData
	 */
	FStructData* AddStruct( const FToken& StructToken )
	{
		check(StructToken.Struct != NULL);

		FStructData* Result = NULL;

		TScopedPointer<FStructData>* pStructData = StructData.Find(StructToken.Struct);
		if ( pStructData != NULL )
		{
			Result = pStructData->GetOwnedPointer();
			*Result = FStructData(StructToken);
		}
		else
		{
			pStructData = &StructData.Set(StructToken.Struct, new FStructData(StructToken));
			Result = pStructData->GetOwnedPointer();
		}

		return Result;
	}

	/**
	 * Adds a new property to be tracked.  Determines the correct list for the property based on
	 * its owner (function, struct, etc).
	 * 
	 * @param	PropertyToken	the property to add
	 */
	void AddProperty( const FToken& PropertyToken )
	{
		UProperty* Prop = PropertyToken.TokenProperty;
		check(Prop);

		UObject* Outer = Prop->GetOuter();
		UClass* OuterClass = Cast<UClass>(Outer);
		if ( OuterClass != NULL )
		{
			// global property
			GlobalPropertyData.Set(Prop,PropertyToken);
		}
		else
		{
			UFunction* OuterFunction = Cast<UFunction>(Outer);
			if ( OuterFunction != NULL )
			{
				// function parameter, return, or local property
				TScopedPointer<FFunctionData>* FuncData = FunctionData.Find(OuterFunction);
				check(FuncData != NULL);

				(*FuncData)->AddProperty(PropertyToken);
			}
			else
			{
				// struct property
				UScriptStruct* OuterStruct = Cast<UScriptStruct>(Outer);
				if (OuterStruct != NULL)
				{
					TScopedPointer<FStructData>* StructInfo = StructData.Find(OuterStruct);
					check(StructInfo!=NULL);

					(*StructInfo)->AddStructProperty(PropertyToken);
				}
				else
				{
					// state property
					UState* OuterState = Cast<UState>(Outer);
					check(OuterState != NULL);

					TScopedPointer<FStateData>* StateDatum = StateData.Find(OuterState);
					check(StateDatum != NULL);

					(*StateDatum)->AddLocalProperty(PropertyToken);
				}
			}
		}

		// update the optimization flags
		if ( !bContainsDelegates )
		{
			UDelegateProperty* DelProp = Cast<UDelegateProperty>(Prop);
			if ( DelProp == NULL )
			{
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
				if ( ArrayProp != NULL )
				{
					DelProp = Cast<UDelegateProperty>(ArrayProp->Inner);
				}
			}

			bContainsDelegates = DelProp != NULL;
		}
	}

	/**
	 * Adds new editor-only property metadata (key/value pairs) to the class or struct that
	 * owns this property.
	 * 
	 * @param	PropertyToken	the property to add to
	 */
	void AddMetaData(const FToken& PropertyToken)
	{
		// only add if we have some!
		if (PropertyToken.MetaData.Num() == 0)
		{
			return;
		}

		// get the property and its outer
		UProperty* Prop = PropertyToken.TokenProperty;
		check(Prop);

		// only allow class/struct properties to have data
		// get (or create) a metadata object for this package
		UMetaData* MetaData = Prop->GetOutermost()->GetMetaData();

		// set the metadata for this property
		MetaData->SetObjectValues(Prop, PropertyToken.MetaData);
	}


	/**
	 * Finds the metadata for the function specified
	 * 
	 * @param	Func	the function to search for
	 *
	 * @return	pointer to the metadata for the function specified, or NULL
	 *			if the function doesn't exist in the list (for example, if it
	 *			is declared in a package that is already compiled and has had its
	 *			source stripped)
	 */
	FFunctionData* FindFunctionData( UFunction* Func );

	/**
	 * Finds the metadata for the struct specified
	 * 
	 * @param	Struct	the struct to search for
	 *
	 * @return	pointer to the metadata for the struct specified, or NULL
	 *			if the struct doesn't exist in the list (for example, if it
	 *			is declared in a package that is already compiled and has had its
	 *			source stripped)
	 */
	FStructData* FindStructData( UScriptStruct* Struct );

	/**
	 * Finds the metadata for the property specified
	 * 
	 * @param	Prop	the property to search for
	 *
	 * @return	pointer to the metadata for the property specified, or NULL
	 *			if the property doesn't exist in the list (for example, if it
	 *			is declared in a package that is already compiled and has had its
	 *			source stripped)
	 */
	FTokenData* FindTokenData( UProperty* Prop );

	/**
	 * (debug) Dumps the values of this FFunctionData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump( INT Indent );

	/**
	 * Add a string to the list of inheritance parents for this class.
	 *
	 * @param Inparent	The C++ class name to add to the multiple inheritance list
	 */
	void AddInheritanceParent(const FString& InParent)
	{
		new(MultipleInheritanceParents) FMultipleInheritanceBaseClass(InParent);
	}

	/**
	 * Add a string to the list of inheritance parents for this class.
	 *
	 * @param Inparent	The C++ class name to add to the multiple inheritance list
	 */
	void AddInheritanceParent(UClass* ImplementedInterfaceClass)
	{
		new(MultipleInheritanceParents) FMultipleInheritanceBaseClass(ImplementedInterfaceClass);
	}

	/**
	 * Return the list of inheritance parents
	 */
	const TArray<FMultipleInheritanceBaseClass>& GetInheritanceParents() const
	{
		return MultipleInheritanceParents;
	}

	/**
	 * Returns whether this class contains any delegate properties which need to be fixed up.
	 */
	UBOOL ContainsDelegates() const
	{
		return bContainsDelegates;
	}

	/**
	 * Shrink TMaps to avoid slack in Pairs array.
	 */
	void Shrink()
	{
		GlobalPropertyData.Shrink();
		StructData.Shrink();
		FunctionData.Shrink();
		StateData.Shrink();
		MultipleInheritanceParents.Shrink();
	}
};

/**
 * Class for storing and linking data about properties and functions that is only required by the compiler.
 * The type of data tracked by this class is data that would otherwise only be accessible by adding a 
 * member property to UFunction/UProperty.  
 */
class FCompilerMetadataManager : protected TMap<UClass*, TScopedPointer<FClassMetaData> >
{
public:

	~FCompilerMetadataManager()
	{
		if ( this == GScriptHelper )
		{
			GScriptHelper = NULL;
		}
	}

	/**
	 * Adds a new class to be tracked
	 * 
	 * @param	Cls	the UClass to add
	 *
	 * @return	a pointer to the newly added metadata for the class specified
	 */
	FClassMetaData* AddClassData( UClass* Cls )
	{
		TScopedPointer<FClassMetaData>* pClassData = Find(Cls);
		if ( pClassData == NULL )
		{
			pClassData = &Set(Cls, new FClassMetaData());
		}

		return *pClassData;
	}

	/**
	 * Find the metadata associated with the class specified
	 * 
	 * @param	Cls	the UClass to add
	 *
	 * @return	a pointer to the newly added metadata for the class specified
	 */
	FClassMetaData* FindClassData( UClass* Cls )
	{
		FClassMetaData* Result = NULL;

		TScopedPointer<FClassMetaData>* pClassData = Find(Cls);
		if ( pClassData )
		{
			Result = pClassData->GetOwnedPointer();
		}

		return Result;
	}

	/**
	 * (debug) Dumps the values of this FFunctionData to the log file
	 * 
	 * @param	Indent	number of spaces to insert at the beginning of each line
	 */	
	void Dump()
	{
		for ( TMap<UClass*,TScopedPointer<FClassMetaData> >::TIterator It(*this); It; ++It )
		{
			UClass* Cls = It.Key();
			TScopedPointer<FClassMetaData>& Data = It.Value();
			debugf(TEXT("=== %s ==="), *Cls->GetName());
			Data->Dump(4);
		}
	}

	/**
	 * Shrink TMaps to avoid slack in Pairs array.
	 */
	void Shrink()
	{
		TMap<UClass*,TScopedPointer<FClassMetaData> >::Shrink();
		for( TMap<UClass*,TScopedPointer<FClassMetaData> >::TIterator It(*this); It; ++It )
		{
			FClassMetaData* MetaData = It.Value();
			MetaData->Shrink();
		}
	}
};

/*-----------------------------------------------------------------------------
	Retry points.
-----------------------------------------------------------------------------*/

/**
 * A point in the script compilation state that can be set and returned to
 * using InitScriptLocation() and ReturnToLocation().  This is used in cases such as testing
 * to see which overridden operator should be used, where code must be compiled
 * and then "undone" if it was found not to match.
 * <p>
 * Retries are not allowed to cross command boundaries (and thus nesting 
 * boundaries).  Retries can occur across a single command or expressions and
 * subexpressions within a command.
 */
struct FScriptLocation
{
	static class FScriptCompiler* Compiler;

	/** the text buffer for the class associated with this retry point */
	const TCHAR* Input;

	/** the position into the Input buffer where this retry point is located */
	INT InputPos;

	/** the LineNumber of the compiler when this retry point was created */
	INT InputLine;

	/** the index into the Script array associated with this retry */
	INT CodeTop;

	/** Constructor */
	FScriptLocation();
};

/**
 * Information about a local variable declaration.
 */
struct FLocalProperty
{
	/** the UProperty represented by this FLocalProperty */
	UProperty *property;

	/** linked list functionality */
	FLocalProperty *next;


	/** first line that the property is assigned a value */
	INT AssignedLine;
	/** first line that the property is referenced */
	INT ReferencedLine;
	/** property is referenced in the function */
	UBOOL	bReferenced;
	/** property is used before it has ever been assigned a value. */
	UBOOL bUninitializedValue;//
	/** property's value is used for something */
	UBOOL bValueAssigned;
	/** property is assigned a value somewhere in the function */
	UBOOL bValueReferenced;

	FLocalProperty( UProperty* inProp )
	: bReferenced(0), bValueAssigned(0), bValueReferenced(0), bUninitializedValue(0), next(NULL)
	, AssignedLine(0), ReferencedLine(0)
	, property(inProp)
	{
		// if this is a struct property that has a default values, always consider it as having been assigned a value
		UStructProperty* StructProp = Cast<UStructProperty>(inProp,CLASS_IsAUStructProperty);
		if ( StructProp && StructProp->Struct->GetDefaultsCount() )
		{
			UClass* Class = StructProp->GetOwnerClass();

			FClassMetaData* ClassData = GScriptHelper->FindClassData(Class);
			if ( ClassData )
			{
				FTokenData* PropData = ClassData->FindTokenData(StructProp);
				if ( PropData )
				{
					ValueAssigned(PropData->Token.StartLine);
				}
			}
		}
	}

	FLocalProperty* GetLocalProperty( UProperty* SearchProp )
	{
		FLocalProperty* Result = NULL;
		if ( SearchProp == property )
			Result = this;

		if ( Result == NULL && next != NULL )
			Result = next->GetLocalProperty(SearchProp);

		return Result;
	}

	void ValueReferenced( INT Line )
	{
		bValueReferenced = true;
		if ( ReferencedLine == 0 )
			ReferencedLine = Line;
	}

	void ValueAssigned( INT Line )
	{
		bValueAssigned = true;

		if ( AssignedLine == 0 )
			AssignedLine = Line;
	}
};


#endif	// __UNCOMPILEHELPER_H__
