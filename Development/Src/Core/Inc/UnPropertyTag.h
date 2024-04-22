/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

#ifndef __UNPROPERTYTAG_H__
#define __UNPROPERTYTAG_H__

/**
 *  A tag describing a class property, to aid in serialization.
 */
struct FPropertyTag
{
	// Variables.
	FName	Type;		// Type of property
	BYTE	BoolVal;	// a boolean property's value (never need to serialize data for bool properties except here)
	FName	Name;		// Name of property.
	FName	StructName;	// Struct name if UStructProperty.
	FName	EnumName;	// Enum name if UByteProperty
	INT		Size;       // Property size.
	INT		ArrayIndex;	// Index if an array; else 0.
	INT		SizeOffset;	// location in stream of tag size member

	// Constructors.
	FPropertyTag()
	{}
	FPropertyTag( FArchive& InSaveAr, UProperty* Property, INT InIndex, BYTE* Value, BYTE* Defaults )
	:	Type		(Property->GetID())
	,	Name		(Property->GetFName())
	,	StructName	(NAME_None)
	,	EnumName	(NAME_None)
	,	Size		(0)
	,	ArrayIndex	(InIndex)
	,	SizeOffset	(INDEX_NONE)
	{
		// Handle structs.
		UStructProperty* StructProperty = Cast<UStructProperty>(Property, CLASS_IsAUStructProperty);
		if (StructProperty != NULL)
		{
			StructName = StructProperty->Struct->GetFName();
		}
		else
		{
			UByteProperty* ByteProp = ExactCast<UByteProperty>(Property);
			if (ByteProp != NULL && ByteProp->Enum != NULL)
			{
				EnumName = ByteProp->Enum->GetFName();
			}
		}

		UBoolProperty* Bool = Cast<UBoolProperty>(Property, CLASS_IsAUBoolProperty);
		BoolVal = (Bool && (*(BITFIELD*)Value & Bool->BitMask)) ? TRUE : FALSE;
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
	{
		// Name.
		Ar << Tag.Name;
		if( Tag.Name == NAME_None )
		{
			return Ar;
		}

		Ar << Tag.Type;
		if ( Ar.IsSaving() )
		{
			// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
			// property has been serialized.
			Tag.SizeOffset = Ar.Tell();
		}
		Ar << Tag.Size << Tag.ArrayIndex;

		// only need to serialize this for structs
		if (Tag.Type == NAME_StructProperty)
		{
			Ar << Tag.StructName;
		}
		// only need to serialize this for bools
		else if (Tag.Type == NAME_BoolProperty)
		{
			if (Ar.Ver() < VER_PROPERTYTAG_BOOL_OPTIMIZATION)
			{
				UBOOL Value = 0;
				Ar << Value;
				Tag.BoolVal = BYTE(Value);
			}
			else
			{
				Ar << Tag.BoolVal;
			}
		}
		// only need to serialize this for bytes
		else if (Tag.Type == NAME_ByteProperty && Ar.Ver() >= VER_BYTEPROP_SERIALIZE_ENUM)
		{
			Ar << Tag.EnumName;
		}

		return Ar;
	}

	// Property serializer.
	void SerializeTaggedProperty( FArchive& Ar, UProperty* Property, BYTE* Value, INT MaxReadBytes, BYTE* Defaults )
	{
		if (Property->GetClass() == UBoolProperty::StaticClass())
		{
			UBoolProperty* Bool = (UBoolProperty*)Property;
			check(Bool->BitMask!=0);
			if (Ar.IsLoading())
			{
				if (BoolVal)
				{
					*(BITFIELD*)Value |=  Bool->BitMask;
				}
				else
				{
					*(BITFIELD*)Value &= ~Bool->BitMask;
				}
			}
		}
		else
		{
			UProperty* OldSerializedProperty = GSerializedProperty;
			GSerializedProperty = Property;

			Property->SerializeItem( Ar, Value, MaxReadBytes, Defaults );

			GSerializedProperty = OldSerializedProperty;
		}
	}
};

#endif	// __UNPROPERTYTAG_H__
