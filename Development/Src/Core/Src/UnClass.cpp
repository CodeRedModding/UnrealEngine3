/*=============================================================================
	UnClass.cpp: Object class implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

#include "UnPropertyTag.h"

//@script patcher
#include "UnScriptPatcher.h"


/*
 * Shared function called from the various InitializePrivateStaticClass functions generated my the IMPLEMENT_CLASS macro.
 */
void InitializePrivateStaticClass( class UClass* TClass_Super_StaticClass, class UClass* TClass_PrivateStaticClass, class UClass* TClass_WithinClass_StaticClass )
{
	/* No recursive ::StaticClass calls allowed. Setup extras. */
	if (TClass_Super_StaticClass != TClass_PrivateStaticClass)
	{
		TClass_PrivateStaticClass->SuperStruct = TClass_Super_StaticClass;
	}
	else
	{
		TClass_PrivateStaticClass->SuperStruct = NULL;
	}
	TClass_PrivateStaticClass->ClassWithin = TClass_WithinClass_StaticClass;
	TClass_PrivateStaticClass->SetClass(UClass::StaticClass());

	/* Perform UObject native registration. */
	if( TClass_PrivateStaticClass->GetInitialized() && TClass_PrivateStaticClass->GetClass()==TClass_PrivateStaticClass->StaticClass() )
	{
		TClass_PrivateStaticClass->Register();
	}
}

/*-----------------------------------------------------------------------------
	UField implementation.
-----------------------------------------------------------------------------*/

UField::UField( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags)
: UObject				( EC_NativeConstructor, InClass, InName, InPackageName, InFlags )
, Next					( NULL )
{}
UField::UField( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags )
: UObject				( EC_StaticConstructor, InName, InPackageName, InFlags )
, Next					( NULL )
{}
/**
 * Static constructor, called once during static initialization of global variables for native 
 * classes. Used to e.g. register object references for native- only classes required for realtime
 * garbage collection or to associate UProperties.
 */
void UField::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UField, Next ) );
}
UClass* UField::GetOwnerClass() const
{
	const UObject* Obj;
	for( Obj=this; Obj->GetClass()!=UClass::StaticClass(); Obj=Obj->GetOuter() );
	return (UClass*)Obj;
}
UStruct* UField::GetOwnerStruct() const
{
	const UObject* Obj;
	for ( Obj=this; Obj && !Obj->IsA(UStruct::StaticClass()); Obj=Obj->GetOuter() );
	return (UStruct*)Obj;
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

	//@compatibility:
	if (Ar.Ver() < VER_MOVED_SUPERFIELD_TO_USTRUCT)
	{
		UField* SuperField = NULL;
		Ar << SuperField;
		UStruct* Struct = Cast<UStruct>(this);
		if (Struct != NULL)
		{
			Struct->SuperStruct = Cast<UStruct>(SuperField);
		}
	}

	Ar << Next;
}
UBOOL UField::MergeBools()
{
	return 1;
}
void UField::AddCppProperty( UProperty* Property )
{
	appErrorf(TEXT("UField::AddCppProperty"));
}
IMPLEMENT_CLASS(UField)

/*-----------------------------------------------------------------------------
	UStruct implementation.
-----------------------------------------------------------------------------*/

#if SERIAL_POINTER_INDEX
void *GSerializedPointers[MAX_SERIALIZED_POINTERS];
DWORD GTotalSerializedPointers = 0;
DWORD SerialPointerIndex(void *ptr)
{
    for (DWORD i = 0; i < GTotalSerializedPointers; i++)
    {
        if (GSerializedPointers[i] == ptr)
            return i;
    }
    check(GTotalSerializedPointers < MAX_SERIALIZED_POINTERS);
    GSerializedPointers[GTotalSerializedPointers] = ptr;
    return(GTotalSerializedPointers++);
}
#endif


//
// Constructors.
//
UStruct::UStruct( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags, UStruct* InSuperStruct )
:	UField			( EC_NativeConstructor, UClass::StaticClass(), InName, InPackageName, InFlags )
#if !CONSOLE
,	ScriptText		( NULL )
,	CppText			( NULL )
#endif
,	SuperStruct		( InSuperStruct )
,	Children		( NULL )
,	PropertiesSize	( InSize )
,	Script			()
#if !CONSOLE
,	TextPos			( 0 )
,	Line			( 0 )
#endif
,	MinAlignment	( 1 )
,	RefLink			( NULL )
,	PropertyLink	( NULL )
,	ConstructorLink	( NULL )
{}
UStruct::UStruct( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags )
:	UField			( EC_StaticConstructor, InName, InPackageName, InFlags )
#if !CONSOLE
,	ScriptText		( NULL )
,	CppText			( NULL )
#endif
,	Children		( NULL )
,	PropertiesSize	( InSize )
,	Script			()
#if !CONSOLE
,	TextPos			( 0 )
,	Line			( 0 )
#endif
,	MinAlignment	( 1 )
,	RefLink			( NULL )
,	PropertyLink	( NULL )
,	ConstructorLink	( NULL )
{}
UStruct::UStruct( UStruct* InSuperStruct )
:	SuperStruct( InSuperStruct )
,	PropertiesSize( InSuperStruct ? InSuperStruct->GetPropertiesSize() : 0 )
,	MinAlignment( Max(InSuperStruct ? InSuperStruct->GetMinAlignment() : 1,1) )
,	RefLink( NULL )
{}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UStruct::StaticConstructor()
{
	UClass* TheClass = GetClass();

	TheClass->EmitObjectReference( STRUCT_OFFSET( UStruct, SuperStruct ) );
#if !CONSOLE
	TheClass->EmitObjectReference( STRUCT_OFFSET( UStruct, ScriptText ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UStruct, CppText ) );
#endif
	TheClass->EmitObjectReference( STRUCT_OFFSET( UStruct, Children ) );

	// Note: None of the *Link members need to be emitted, as they only contain properties
	// that are in the Children chain or SuperStruct->Children chains.

	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( UStruct, ScriptObjectReferences ) );
}

void UStruct::Register()
{
	Super::Register();
	if (SuperStruct != NULL)
	{
		SuperStruct->ConditionalRegister();
	}
}

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
//@script patcher (LinkStart)
LinkStart:
	// Link the properties.
	if( Props )
	{
		PropertiesSize	= 0;
		MinAlignment	= 1;
		UStruct* InheritanceSuper = GetInheritanceSuper();
		if( InheritanceSuper )
		{
			Ar.Preload( InheritanceSuper );
			PropertiesSize	= InheritanceSuper->GetPropertiesSize();
			MinAlignment	= InheritanceSuper->GetMinAlignment();
#if XBOX || WIIU
			// The Xenon compiler don't crack open the padding of base classes.
			PropertiesSize	= Align( PropertiesSize, MinAlignment );
#else			
			// We at least always expect all structs to be SCRIPT_ALIGN 'padded' out to size of BITFIELD (see SCRIPT_ALIGN)
			PropertiesSize	= Align( PropertiesSize, sizeof(BITFIELD) );
#endif
		}

		UProperty* Prev = NULL;
		for( UField* Field=Children; Field; Field=Field->Next )
		{
			// calling Preload here is required in order to load the value of Field->Next
			Ar.Preload( Field );
			if( Field->GetOuter()!=this )
			{
				break;
			}

			UProperty* Property = Cast<UProperty>( Field );
			if( Property )
			{
#if !WITH_EDITORONLY_DATA
				// If we don't have the editor, make sure we aren't trying to link properties that are editor only.
				check( !Property->IsEditorOnlyProperty() );
#endif // WITH_EDITORONLY_DATA
				Property->Link( Ar, Prev );
				PropertiesSize	= Property->Offset + Property->GetSize();
				Prev			= Property;
				MinAlignment	= Max( MinAlignment, Property->GetMinAlignment() );
			}
		}
		// check for internal struct recursion via arrays
		if (GetClass() == UScriptStruct::StaticClass())
		{
			for (UField* Field = Children; Field; Field = Field->Next)
			{
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Field);
				if (ArrayProp != NULL)
				{
					UStructProperty* StructProp = Cast<UStructProperty>(ArrayProp->Inner);
					if (StructProp != NULL && StructProp->Struct == this)
					{
						StructProp->ElementSize = PropertiesSize;
						// the property was linked before us and therefore might not have known we would need a constructor
						StructProp->PropertyFlags |= CPF_NeedCtorLink;
					}
				}
			}
		}

		if( GetFName() == NAME_Matrix
		||	GetFName() == NAME_Plane
		||	GetFName() == NAME_SHVector
		||	GetFName() == NAME_Vector4 
		||	GetFName() == NAME_Quat)
		{
// @todo wiiu: This is horrible. Is there no way for GHS to align a type without getting this??
//		error #1982-D: target stack alignment is insufficient to guarantee alignment of variable
#if !WIIU
			MinAlignment = 16;
#endif
		}

		// @todo gcc: it is not apparent when (platform/cpu/compiler/etc) we need to align qwords and doubles
#if	PLATFORM_64BITS
		else if( GetFName() == NAME_QWord )
		{
			MinAlignment = __alignof(UObject);
			PropertiesSize = 8;
		}
		else if( GetFName() == NAME_Double )
		{
			MinAlignment = __alignof(UObject);
			PropertiesSize = 8;
		}
		else if( GetFName() == NAME_Pointer )
		{
			MinAlignment = __alignof(UObject);
			PropertiesSize = 8;
		}
#elif PS3 || (ANDROID && !ANDROID_X86) || NGP || WIIU || FLASH
 		else if( GetFName() == NAME_QWord )
 		{
 			MinAlignment = 8;
 		}
 		else if( GetFName() == NAME_Double )
 		{
 			MinAlignment = 8;
 		}
#endif
		else if( GetFName() == NAME_Color )
		{
			MinAlignment = 4;
		}
		else
		{
			MinAlignment = Max( MinAlignment, 4 );
		}
	}
	else
	{
		UProperty* Prev = NULL;
		for( UField* Field=Children; Field && Field->GetOuter()==this; Field=Field->Next )
		{
			UProperty* Property = Cast<UProperty>( Field );
			if( Property )
			{
				UBoolProperty*	BoolProperty	= Cast<UBoolProperty>( Property, CLASS_IsAUBoolProperty );
				INT				SavedOffset		= Property->Offset;
				BITFIELD		SavedBitMask	= BoolProperty ? BoolProperty->BitMask : 0;

				Property->Link( Ar, Prev );

				Property->Offset				= SavedOffset;
				Prev							= Property;
				if( BoolProperty )
				{
					BoolProperty->BitMask = SavedBitMask;
				}
			}
		}
	}

	//@{
	//@script patcher
	if ( HasAnyFlags(RF_PendingFieldPatches) && Ar.IsLoading() && Props )
	{
		// there are member properties or functions that will be added by a script patch 
		ULinker* LinkerAr = Ar.GetLinker();
		if ( LinkerAr != NULL )
		{
			checkSlow(LinkerAr == GetLinker());

			// since the list of UFields that we're going to be adding might be out of order, we'll need to sort them first, so that
			// the value of 'Next' for the first field we add is either an existing UField in this struct or NULL (indicating that the
			// new field should be last in the linked list)
			TArray<UField*> UnsortedFields;

			// first, find the exports that contain the fields we're going to add
			for ( INT ExportIndex = LinkerAr->ExportMap.Num() - 1; ExportIndex >= 0; ExportIndex-- )
			{
				FObjectExport& Export = LinkerAr->ExportMap(ExportIndex);
				if ( !Export.HasAnyFlags(EF_ScriptPatcherExport) )
				{
					// reached the last export that was originally part of the linker - stop here
					break;
				}

				if ( Export.OuterIndex == GetLinkerIndex() + 1 )
				{
					if ( Export._Object == NULL )
					{
						GetLinker()->CreateExport(ExportIndex);
					}

					if ( Export._Object != NULL )
					{
						UField* FieldObject = Cast<UField>(Export._Object);
						if ( FieldObject != NULL )
						{
							// calling Preload is required in order to load the value of FieldObject->Next; we must call Preload on all fields that we'll
							// be adding, before attempting to sort them so that they all have the correct values for Next
							Ar.Preload( FieldObject );
							UnsortedFields.AddItem(FieldObject);
						}
					}
				}
			}

			if (UnsortedFields.Num() > 0)
			{
				// Create an array representing the linked list of all existing UFields
				TArray<UField*> ClassFields;
				ClassFields.Reserve(100);
				for( UField* ExistingField=Children; ExistingField && ExistingField->GetOuter()==this; ExistingField=ExistingField->Next )
				{
					ClassFields.AddItem(ExistingField);
				}

				// While there are elements unsorted, continue to order new items by their Next pointer
				while ( UnsortedFields.Num() > 0 )
				{
					for (INT UnsortedIdx = 0; UnsortedIdx < UnsortedFields.Num(); UnsortedIdx++)
					{
						UField* UnsortedField = UnsortedFields(UnsortedIdx);
						if (UnsortedField->Next != NULL)
						{
						    INT InsertIndex = ClassFields.FindItemIndex(UnsortedField->Next);
							if (InsertIndex != INDEX_NONE)
							{
								ClassFields.InsertItem(UnsortedField, InsertIndex);
								UnsortedFields.RemoveItem(UnsortedField);
								UnsortedIdx--;
							}
						}
						else
						{
							ClassFields.AddItem(UnsortedField);
							UnsortedFields.RemoveItem(UnsortedField);
							UnsortedIdx--;
						}
					}
				}

				// Fixup pointers
				for (INT FieldIdx = 0; FieldIdx < ClassFields.Num() - 1; FieldIdx++)
				{
					ClassFields(FieldIdx)->Next = ClassFields(FieldIdx + 1);
				}

				// Fixup Children
				Children = ClassFields(0);
			}
		}


		// clear the flag and relink everything
		ClearFlags(RF_PendingFieldPatches);
		goto LinkStart;
	}
	//@}

#if !__INTEL_BYTE_ORDER__
	// Object.uc declares FColor as BGRA which doesn't match up with what we'd like to use on
	// Xenon to match up directly with the D3D representation of D3DCOLOR. We manually fiddle 
	// with the property offsets to get everything to line up.
	// In any case, on big-endian systems we want to byte-swap this.
	//@todo cooking: this should be moved into the data cooking step.
	if( GetFName() == NAME_Color )
	{
		UProperty*	ColorComponentEntries[4];
		UINT		ColorComponentIndex = 0;

		for( UField* Field=Children; Field && Field->GetOuter()==this; Field=Field->Next )
		{
			UProperty* Property = Cast<UProperty>( Field );
			check(Property);
			ColorComponentEntries[ColorComponentIndex++] = Property;
		}
		check( ColorComponentIndex == 4 );

		Exchange( ColorComponentEntries[0]->Offset, ColorComponentEntries[3]->Offset );
		Exchange( ColorComponentEntries[1]->Offset, ColorComponentEntries[2]->Offset );
	}
#endif

	// Link the references, structs, and arrays for optimized cleanup.
	// Note: Could optimize further by adding UProperty::NeedsDynamicRefCleanup, excluding things like arrays of ints.
	UProperty** PropertyLinkPtr		= &PropertyLink;
	UProperty** ConstructorLinkPtr	= &ConstructorLink;
	UProperty** RefLinkPtr			= (UProperty**)&RefLink;

	for( TFieldIterator<UProperty> It(this); It; ++It)
	{
		UProperty* Property = *It;

		if( Property->ContainsObjectReference() )
		{
			*RefLinkPtr = Property;
			RefLinkPtr=&(*RefLinkPtr)->NextRef;
		}

		if( Property->HasAnyPropertyFlags(CPF_NeedCtorLink) )
		{
			*ConstructorLinkPtr = Property;
			ConstructorLinkPtr  = &(*ConstructorLinkPtr)->ConstructorLinkNext;
		}

		if( Property->HasAnyPropertyFlags(CPF_Net) && !GIsEditor )
		{
			FArchive TempAr;
			INT iCode = Property->RepOffset;
			Property->GetOwnerClass()->SerializeExpr( iCode, TempAr );
		}

		*PropertyLinkPtr = Property;
		PropertyLinkPtr  = &(*PropertyLinkPtr)->PropertyLinkNext;
	}

	*PropertyLinkPtr    = NULL;
	*ConstructorLinkPtr = NULL;
	*RefLinkPtr			= NULL;
}

/**
 * Serializes the passed in property with the struct's data residing in Data.
 *
 * @param	Property		property to serialize
 * @param	Ar				the archive to use for serialization
 * @param	Data			pointer to the location of the beginning of the struct's property data
 */
void UStruct::SerializeBinProperty( UProperty* Property, FArchive& Ar, BYTE* Data ) const
{
	if( Property->ShouldSerializeValue(Ar) )
	{
		UProperty* OldSerializedProperty = GSerializedProperty;
		for( INT Idx=0; Idx<Property->ArrayDim; Idx++ )
		{
			GSerializedProperty = Property;
			Property->SerializeItem( Ar, Data + Property->Offset + Idx * Property->ElementSize, 0 );
		}
		GSerializedProperty = OldSerializedProperty;
	}
}

//
// Serialize all of the class's data that belongs in a particular
// bin and resides in Data.
//
void UStruct::SerializeBin( FArchive& Ar, BYTE* Data, INT MaxReadBytes ) const
{
	if( Ar.IsObjectReferenceCollector() )
	{
		for( UProperty* RefLinkProperty=RefLink; RefLinkProperty!=NULL; RefLinkProperty=RefLinkProperty->NextRef )
		{
			SerializeBinProperty( RefLinkProperty, Ar, Data );
		}
	}
	else
	{
		for (UProperty* Property = PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
		{
			SerializeBinProperty(Property, Ar, Data);
		}
	}
}
/**
 * Serializes the class properties that reside in Data if they differ from the corresponding values in DefaultData
 *
 * @param	Ar				the archive to use for serialization
 * @param	Data			pointer to the location of the beginning of the property data
 * @param	DefaultData		pointer to the location of the beginning of the data that should be compared against
 * @param	DefaultsCount	size of the block of memory located at DefaultData 
 */
void UStruct::SerializeBinEx( FArchive& Ar, BYTE* Data, BYTE* DefaultData, INT DefaultsCount ) const
{
	if ( DefaultData == NULL || DefaultsCount == 0 )
	{
		SerializeBin(Ar, Data, 0);
		return;
	}

	for( TFieldIterator<UProperty> It(this); It; ++It )
	{
		UProperty* Property = *It;
		if( Property->ShouldSerializeValue(Ar) )
		{
			for( INT Idx=0; Idx<Property->ArrayDim; Idx++ )
			{
				const INT Offset = Property->Offset + Idx * Property->ElementSize;
				if ( !Property->Matches(Data, (Offset + Property->ElementSize <= DefaultsCount) ? DefaultData : NULL, Idx, FALSE, Ar.GetPortFlags()) )
				{
					UProperty* OldSerializedProperty = GSerializedProperty;
					GSerializedProperty = Property;

					Property->SerializeItem( Ar, Data + Offset, 0, DefaultData + Offset );
					
					GSerializedProperty = OldSerializedProperty;
				}
			}
		}
	}
}

void UStruct::SerializeTaggedProperties( FArchive& Ar, BYTE* Data, UStruct* DefaultsStruct, BYTE* Defaults, INT DefaultsCount/*=0*/ ) const
{
	FName PropertyName(NAME_None);

	check(Ar.IsLoading() || Ar.IsSaving() || GIsUCCMake);

	UClass* DefaultsClass = Cast<UClass>(DefaultsStruct);
	UScriptStruct* DefaultsScriptStruct = Cast<UScriptStruct>(DefaultsStruct);

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
			{
				break;
			}
			PropertyName = Tag.Name;

			// redirect SkeletalMeshActor.bCollideActors to bCollideActors_OldValue for backwards compatibility with default value change
			if (Ar.Ver() < VER_REMOVED_DEFAULT_SKELETALMESHACTOR_COLLISION)
			{
				static FName NAME_bCollideActors(TEXT("bCollideActors"));
				static FName NAME_bCollideActors_OldValue(TEXT("bCollideActors_OldValue"));
				static FName NAME_SkeletalMeshActor(TEXT("SkeletalMeshActor"));
				if (PropertyName == NAME_bCollideActors)
				{
					for (UClass* TestClass = DefaultsClass; DefaultsClass != NULL; DefaultsClass = DefaultsClass->GetSuperClass())
					{
						if (TestClass->GetFName() == NAME_SkeletalMeshActor)
						{
							Tag.Name = NAME_bCollideActors_OldValue;
							PropertyName = Tag.Name;
							break;
						}
					}
				}
			}

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


			//@{
			//@compatibility
			// Check to see if we are loading an old InterpCurve Struct.
			UBOOL bNeedCurveFixup = FALSE;
			if( Ar.Ver() < VER_NEW_CURVE_AUTO_TANGENTS && Tag.Type == NAME_StructProperty && Cast<UStructProperty>(Property, CLASS_IsAUStructProperty) )
			{
				FName StructName = ((UStructProperty*)Property)->Struct->GetFName();
				if( StructName == NAME_InterpCurveFloat || StructName == NAME_InterpCurveVector2D ||
					StructName == NAME_InterpCurveVector || StructName == NAME_InterpCurveTwoVectors ||
					StructName == NAME_InterpCurveQuat )
				{
					bNeedCurveFixup = TRUE;
				}
			}
			//@}


			UBOOL bSkipSkipWarning = FALSE;

			if( !Property )
			{
				//@{
				//@compatibility
				if ( Tag.Name == NAME_InitChild2StartBone )
				{
					UProperty* NewProperty = FindField<UProperty>(DefaultsClass, TEXT("BranchStartBoneName"));
					if (NewProperty != NULL && NewProperty->IsA(UArrayProperty::StaticClass()) && ((UArrayProperty*)NewProperty)->Inner->IsA(UNameProperty::StaticClass()))
					{
						FName OldName;
						Ar << OldName;
						((TArray<FName>*)(Data + NewProperty->Offset))->AddItem(OldName);
						AdvanceProperty = FALSE;
						continue;
					}
				}
				//@}
				
				debugfSlow( NAME_Warning, TEXT("Property %s of %s not found for package:  %s"), *Tag.Name.ToString(), *GetFullName(), *Ar.GetArchiveName() );
			}
			// editoronly properties should be skipped if we are NOT the editor, or we are 
			// the editor but are cooking for console (editoronly implies notforconsole)
			else if ((Property->PropertyFlags & CPF_EditorOnly) && (!GIsEditor || (GCookingTarget & UE3::PLATFORM_Stripped)) && !(GUglyHackFlags & HACK_ForceLoadEditorOnly))
			{
				debugfSuppressed(NAME_DevLoad, TEXT("Skipping editor-only property %s"), *Tag.Name.ToString());
				bSkipSkipWarning = TRUE;
			}
			// notforconsole properties should be skipped if we are cooking for a console
			// or we are running on a console
			else if ((Property->PropertyFlags & CPF_NotForConsole) && ((GCookingTarget & UE3::PLATFORM_Stripped) || (appGetPlatformType() & UE3::PLATFORM_Stripped)))
			{
				debugfSuppressed(NAME_DevLoad, TEXT("Skipping not-for-console property %s"), *Tag.Name.ToString());
				bSkipSkipWarning = TRUE;
			}
			// check for valid array index
			else if( Tag.ArrayIndex >= Property->ArrayDim || Tag.ArrayIndex < 0 )
			{
				debugf( NAME_Warning, TEXT("Array bounds in %s of %s: %i/%i for package:  %s"), *Tag.Name.ToString(), *GetName(), Tag.ArrayIndex, Property->ArrayDim, *Ar.GetArchiveName() );
			}
			else if( Tag.Type==NAME_StrProperty && Cast<UNameProperty>(Property) != NULL )
			{ 
				FString str;  
				Ar << str; 
				*(FName*)(Data + Property->Offset + Tag.ArrayIndex * Property->ElementSize ) = FName(*str);  
				AdvanceProperty = TRUE;
				continue; 
			}
			else if ( Tag.Type == NAME_ByteProperty && Property->GetID() == NAME_IntProperty )
			{
				// this property's data was saved as a BYTE, but the property has been changed to an INT.  Since there is no loss of data
				// possible, we can auto-convert to the right type.
				BYTE PreviousValue;

				// de-serialize the previous value
				// if the byte property had an enum, it's serialized differently so we need to account for that
				if (Tag.EnumName != NAME_None)
				{
					//@warning: mirrors loading code in UByteProperty::SerializeItem()
					FName EnumValue;
					Ar << EnumValue;
					UEnum* Enum = FindField<UEnum>((DefaultsClass != NULL) ? DefaultsClass : DefaultsStruct->GetTypedOuter<UClass>(), Tag.EnumName);
					if (Enum == NULL)
					{
						Enum = FindObject<UEnum>(ANY_PACKAGE, *Tag.EnumName.ToString(), TRUE);
					}
					if (Enum == NULL)
					{
						debugf(NAME_Warning, TEXT("Failed to find enum '%s' when converting property '%s' to int during property loading"), *Tag.EnumName.ToString(), *Tag.Name.ToString());
						PreviousValue = 0;
					}
					else
					{
						Ar.Preload(Enum);
						PreviousValue = Enum->FindEnumIndex(EnumValue);
						if (Enum->NumEnums() < PreviousValue)
						{
							PreviousValue = Enum->NumEnums() - 1;
						}
					}
				}
				else
				{
					Ar << PreviousValue;
				}

				// now copy the value into the object's address spaace
				*(INT*)(Data + Property->Offset + Tag.ArrayIndex * Property->ElementSize) = PreviousValue;
				AdvanceProperty = TRUE;
				continue;
			}
			else if( Tag.Type!=Property->GetID() )
			{
				debugf( NAME_Warning, TEXT("Type mismatch in %s of %s - Previous (%s) Current(%s) for package:  %s"), *Tag.Name.ToString(), *GetName(), *Tag.Type.ToString(), *Property->GetID().ToString(), *Ar.GetArchiveName() );
			}
			else if( Tag.Type==NAME_StructProperty && Tag.StructName!=CastChecked<UStructProperty>(Property)->Struct->GetFName() )
			{
				debugf( NAME_Warning, TEXT("Property %s of %s struct type mismatch %s/%s for package:  %s"), *Tag.Name.ToString(), *GetName(), *Tag.StructName.ToString(), *CastChecked<UStructProperty>(Property)->Struct->GetName(), *Ar.GetArchiveName() );
			}
			else if( !Property->ShouldSerializeValue(Ar) )
			{
				debugf( NAME_Warning, TEXT("Property %s of %s is not serializable for package:  %s"), *Tag.Name.ToString(), *GetName(), *Ar.GetArchiveName() );
			}
			else if ( Tag.Type == NAME_ByteProperty && ( (Tag.EnumName == NAME_None && ExactCast<UByteProperty>(Property)->Enum != NULL) || 
														(Tag.EnumName != NAME_None && ExactCast<UByteProperty>(Property)->Enum == NULL) ) &&
					Ar.Ver() >= VER_BYTEPROP_SERIALIZE_ENUM )
			{
				// a byte property gained or lost an enum
				// attempt to convert it
				BYTE PreviousValue;
				if (Tag.EnumName == NAME_None)
				{
					// simply pretend the property still doesn't have an enum and serialize the single byte
					Ar << PreviousValue;
				}
				else
				{
					// attempt to find the old enum and get the byte value from the serialized enum name
					//@warning: mirrors loading code in UByteProperty::SerializeItem()
					FName EnumValue;
					Ar << EnumValue;
					UEnum* Enum = FindField<UEnum>((DefaultsClass != NULL) ? DefaultsClass : DefaultsStruct->GetTypedOuter<UClass>(), Tag.EnumName);
					if (Enum == NULL)
					{
						Enum = FindObject<UEnum>(ANY_PACKAGE, *Tag.EnumName.ToString(), TRUE);
					}
					if (Enum == NULL)
					{
						debugf(NAME_Warning, TEXT("Failed to find enum '%s' when converting property '%s' to byte during property loading"), *Tag.EnumName.ToString(), *Tag.Name.ToString());
						PreviousValue = 0;
					}
					else
					{
						Ar.Preload(Enum);
						PreviousValue = Enum->FindEnumIndex(EnumValue);
						if (Enum->NumEnums() < PreviousValue)
						{
							PreviousValue = Enum->NumEnums() - 1;
						}
					}
				}
				
				// now copy the value into the object's address spaace
				*(BYTE*)(Data + Property->Offset + Tag.ArrayIndex * Property->ElementSize) = PreviousValue;
				AdvanceProperty = TRUE;
				continue;
			}
			else
			{
				//@hack: to allow the components array to always be loaded correctly in the editor, don't serialize it
				UBOOL bSkipProperty =
					!GIsGame													// if we are in the editor
					&& !Ar.IsTransacting()										// and we are not transacting
					&& DefaultsClass != NULL									// and we are serializing object data
					&& Property->GetFName() == NAME_Components					// and this property's name is 'Components'
					&& Property->GetOwnerClass()->GetFName() == NAME_Actor		// and this property is declared in 'Actor'
					&& !((UObject*)Data)->HasAnyFlags(RF_ClassDefaultObject)	// and we aren't serializing the default object
					&& (Ar.GetPortFlags()&PPF_Duplicate) == 0;					// and we aren't duplicating an actor

				if ( !bSkipProperty )
				{
					BYTE* DestAddress = Data + Property->Offset + Tag.ArrayIndex * Property->ElementSize;

					// This property is ok.			
					Tag.SerializeTaggedProperty( Ar, Property, DestAddress, Tag.Size, NULL );


					//@{
					//@compatibility
					// If we're fixing up interp curves, we need to set the curve method property manually.
					if( bNeedCurveFixup )
					{
						UScriptStruct* CurveStruct = Cast<UStructProperty>(Property, CLASS_IsAUStructProperty)->Struct;
						checkSlow(CurveStruct);

						UProperty *CurveMethodProperty = FindField<UByteProperty>(CurveStruct, TEXT("InterpMethod"));

						// Old packages store the interp method value one less than what it should be
						*(BYTE*)((BYTE*)DestAddress + CurveMethodProperty->Offset) = *(BYTE*)((BYTE*)DestAddress + CurveMethodProperty->Offset) + 1;
					}
					//@}


					AdvanceProperty = TRUE;
					continue;
				}
			}

			AdvanceProperty = FALSE;

			// Skip unknown or bad property.
			debugfSlow( bSkipSkipWarning ? NAME_DevLoad : NAME_Warning, TEXT("Skipping %i bytes of type %s for package:  %s"), Tag.Size, *Tag.Type.ToString(), *Ar.GetArchiveName() );
			
			BYTE B;
			for( INT i=0; i<Tag.Size; i++ )
			{
				Ar << B;
			}
		}
	}
	else
	{
		// Find defaults.
		BYTE* DefaultData   = Defaults;

		/** If TRUE, it means that we want to serialize all properties of this struct if any properties differ from defaults */
		UBOOL bUseAtomicSerialization = FALSE;
		if( DefaultsStruct )
		{
			if ( DefaultsClass != NULL )
			{
				if ( DefaultsCount <= 0 )
				{
					UObject* Archetype = DefaultData ? (UObject*)DefaultData : ((UObject*)Data)->GetArchetype();
					if ( Archetype != NULL )
					{
						DefaultsCount = Archetype->GetClass()->GetDefaultsCount();
					}
				}
			}
			else if ( DefaultsScriptStruct != NULL )
			{
				bUseAtomicSerialization = DefaultsScriptStruct->ShouldSerializeAtomically(Ar);
				if ( DefaultsCount <= 0 )
				{
					DefaultsCount = DefaultsScriptStruct->GetDefaultsCount();
				}
			}
		}

		// Save tagged properties.

		// Iterate over properties in the order they were linked and serialize them.
		for( UProperty* Property = PropertyLink; Property; Property = Property->PropertyLinkNext )
		{
			if( Property->ShouldSerializeValue(Ar) )
			{
				//@hack: to allow the components array to always be loaded correctly in the editor, don't serialize it
				if ( !GIsGame													// if we are in the editor
					&& DefaultsClass != NULL									// and we are serializing object data
					&& Property->GetFName() == NAME_Components					// and this property's name is 'Components'
					&& Property->GetOwnerClass()->GetFName() == NAME_Actor		// and this property is declared in 'Actor'
					&& !((UObject*)Data)->HasAnyFlags(RF_ClassDefaultObject)	// and we aren't serializing the default object
					&& (Ar.GetPortFlags()&PPF_Duplicate) == 0)					// and we aren't duplicating an object
				{
					continue;
				}

				PropertyName = Property->GetFName();
				for( INT Idx=0; Idx<Property->ArrayDim; Idx++ )
				{
					const INT Offset = Property->Offset + Idx * Property->ElementSize;
#if !CONSOLE
					// make sure all FRawDistributions are up-to-date (this is NOT a one-time, Ar.Ver()'able thing
					// because we must make sure FDist is up-to-date with after any change to a UDist that wasn't
					// baked out in a GetValue() call)
					if (Property->IsA(UStructProperty::StaticClass()))
					{
						FName StructName = ((UStructProperty*)Property)->Struct->GetFName();
						if (StructName == NAME_RawDistributionFloat)
						{
							FRawDistributionFloat* RawDistribution = (FRawDistributionFloat*)(Data + Offset);
							RawDistribution->Initialize();
						}
						else if (StructName == NAME_RawDistributionVector)
						{
							FRawDistributionVector* RawDistribution = (FRawDistributionVector*)(Data + Offset);
							RawDistribution->Initialize();
						}
					}
#endif

					if( (!IsA(UClass::StaticClass())&&!DefaultData) || !Property->Matches( Data, (Offset+Property->ElementSize<=DefaultsCount) ? DefaultData : NULL, Idx, FALSE, Ar.GetPortFlags()) )
					{
						BYTE* DefaultValue = (!bUseAtomicSerialization && DefaultData && (Offset + Property->ElementSize <= DefaultsCount)) ? DefaultData + Offset : NULL;
						FPropertyTag Tag( Ar, Property, Idx, Data + Offset, DefaultValue );
						Ar << Tag;

						// need to know how much data this call to SerializeTaggedProperty consumes, so mark where we are
						INT DataOffset = Ar.Tell();

						Tag.SerializeTaggedProperty( Ar, Property, Data + Offset, 0, DefaultValue );

						// set the tag's size
						Tag.Size = Ar.Tell() - DataOffset;

						if ( Tag.Size >  0 )
						{
							// mark our current location
							DataOffset = Ar.Tell();

							// go back and re-serialize the size now that we know it
							Ar.Seek(Tag.SizeOffset);
							Ar << Tag.Size;

							// return to the current location
							Ar.Seek(DataOffset);
						}
					}
				}
			}
		}
		FName Temp(NAME_None);
		Ar << Temp;
	}
}
void UStruct::FinishDestroy()
{
	Script.Empty();
	Super::FinishDestroy();
}
void UStruct::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Serialize stuff.
	if (Ar.Ver() >= VER_MOVED_SUPERFIELD_TO_USTRUCT)
	{
		Ar << SuperStruct;
	}
#if !CONSOLE
	// if reading data that's cooked for console, skip this data
	UBOOL const bIsCookedForConsole = IsPackageCookedForConsole(Ar);
	if ( !bIsCookedForConsole && (!Ar.IsSaving() || !GIsCooking || !(GCookingTarget & UE3::PLATFORM_Console)) )
	{
		Ar << ScriptText;
	}
#endif
	Ar << Children;
#if !CONSOLE
	if (!bIsCookedForConsole && (!Ar.IsSaving() || !GIsCooking || !(GCookingTarget & UE3::PLATFORM_Console)) )
	{
		Ar << CppText;
		// Compiler info.
		Ar << Line << TextPos;
	}
#endif

#if SUPPORTS_SCRIPTPATCH_CREATION && !WITH_EDITORONLY_DATA
	// Clean up children here
	UField** Previous = &Children;
	UField* Current = Children;
	while( Current )
	{
		Ar.Preload( Current );

		UBOOL bSet = FALSE;
		UProperty* Prop = Cast<UProperty>( Current );
		if( Prop )
		{
			if( Prop->IsEditorOnlyProperty() )
			{
				*Previous = Current->Next;
				bSet = TRUE;
			}
		}

		if( !bSet )
		{
			Previous = &Current->Next;
		}
		Current = Current->Next;
	}
#endif

	//@script patcher: if script patches were applied to our linker, then we need to check to see if this struct's bytecode is being patched.
	// the linker should have a mapping of ExportMap index => bytecode patches, so we can check with negligable performance impact 
	FScriptPatchData* PatchedBytecode = NULL;
	
	// Script code.
	INT ScriptBytecodeSize = Script.Num();
	INT ScriptStorageSize = 0;
	INT ScriptStorageSizeOffset = 0;
	if ( Ar.IsLoading() )
	{
		Ar << ScriptBytecodeSize;

		if (Ar.Ver() >= VER_USTRUCT_SERIALIZE_ONDISK_SCRIPTSIZE)
		{
			Ar << ScriptStorageSize;
		}

#if SUPPORTS_SCRIPTPATCH_LOADING
		//@{
		//@script patcher
		ULinker* LinkerAr = Ar.GetLinker();
		if ( LinkerAr != NULL )
		{
			checkSlow(LinkerAr == GetLinker());
			PatchedBytecode = GetLinker()->FindBytecodePatch(GetLinkerIndex());
			if ( PatchedBytecode != NULL )
			{
				if (ScriptStorageSize > 0)
				{
					// first, increment the file reader beyond the bytecode we would have serialized
					Ar.Seek(Ar.Tell() + ScriptStorageSize);
				}
				else
				{
					// to handle old packages that don't have the on-disk data size for us, 
					// we'll skip the proper amount on disk size by simply serializing and throwing
					// away the results

					Script.Empty( ScriptBytecodeSize );
					Script.Add( ScriptBytecodeSize );

					INT iCode = 0;
					while( iCode < ScriptBytecodeSize )
					{	
						SerializeExpr( iCode, Ar );
					}
					if( iCode != ScriptBytecodeSize )
					{	
						appErrorf( TEXT("Script serialization mismatch: Got %i, expected %i"), iCode, ScriptBytecodeSize );
					}
				}

				// update the number of bytes which will serialized with the number of bytes in the patch, so that we allocate the correct number of elements in the Script array
				ScriptBytecodeSize = PatchedBytecode->Data.Num();

			}
		}
		//@}
#endif

		Script.Empty( ScriptBytecodeSize );
		Script.Add( ScriptBytecodeSize );
	}

	// Ensure that last byte in script code is EX_EndOfScript to work around script debugger implementation.
	else if( Ar.IsSaving() )
	{
		if ( GIsUCCMake && ScriptBytecodeSize && Ar.GetLinker() != NULL )
		{
			Script.AddItem( EX_EndOfScript );
			ScriptBytecodeSize++;
		}

		Ar << ScriptBytecodeSize;

		// drop a zero here.  will seek back later and re-write it when we know it
		ScriptStorageSizeOffset = Ar.Tell();
		Ar << ScriptStorageSize;
	}

	//@script patcher: if there is a bytecode patch for this struct, we'll serialize from there instead of the linker's file reader.  But, we'll need to make sure to
	// increment the linker's pos by the size of the bytecode we would have serialized.
	if ( PatchedBytecode == NULL )
	{
		// no bytecode patch for this struct - serialize normally [i.e. from disk]
		INT iCode = 0;
		INT const BytecodeStartOffset = Ar.Tell();

		if (Ar.IsPersistent() && Ar.GetLinker())
		{
			if (Ar.IsLoading())
			{
				// make sure this is a ULinkerLoad
				ULinkerLoad* LinkerLoad = CastChecked<ULinkerLoad>(Ar.GetLinker());

				// remember how we were loading
				FArchive* SavedLoader = LinkerLoad->Loader;

				// preload the bytecode
				TArray<BYTE> TempScript;
				TempScript.Add(ScriptStorageSize);
				Ar.Serialize(TempScript.GetData(), ScriptStorageSize);

				// force reading from the pre-serialized buffer
				FMemoryReader MemReader(TempScript, Ar.IsPersistent());
				LinkerLoad->Loader = &MemReader;

				// now, use the linker to load the byte code, but reading from memory
				while( iCode < ScriptBytecodeSize )
				{	
					SerializeExpr( iCode, Ar );
				}

				// restore the loader
				LinkerLoad->Loader = SavedLoader;

				// and update the SHA (does nothing if not currently calculating SHA)
				LinkerLoad->UpdateScriptSHAKey(TempScript);
			}
			else
			{
				// make sure this is a ULinkerSave
				ULinkerSave* LinkerSave = CastChecked<ULinkerSave>(Ar.GetLinker());
				
				// remember how we were saving
				FArchive* SavedSaver = LinkerSave->Saver;

				// force writing to a buffer
				TArray<BYTE> TempScript;
				FMemoryWriter MemWriter(TempScript, Ar.IsPersistent());
				LinkerSave->Saver = &MemWriter;

				// now, use the linker to save the byte code, but writing to memory
				while( iCode < ScriptBytecodeSize )
				{	
					SerializeExpr( iCode, Ar );
				}

				// restore the saver
				LinkerSave->Saver = SavedSaver;

				// now write out the memory bytes
				Ar.Serialize(TempScript.GetData(), TempScript.Num());

				// and update the SHA (does nothing if not currently calculating SHA)
				LinkerSave->UpdateScriptSHAKey(TempScript);
			}
		}
		else
		{	
			while( iCode < ScriptBytecodeSize )
			{	
				SerializeExpr( iCode, Ar );
			}
		}

		if( iCode != ScriptBytecodeSize )
		{	
			appErrorf( TEXT("Script serialization mismatch: Got %i, expected %i"), iCode, ScriptBytecodeSize );
		}

		if (Ar.IsSaving())
		{
			INT const BytecodeEndOffset = Ar.Tell();

			// go back and write on-disk size
			Ar.Seek(ScriptStorageSizeOffset);
			ScriptStorageSize = BytecodeEndOffset - BytecodeStartOffset;
			Ar << ScriptStorageSize;

			// back to where we were
			Ar.Seek(BytecodeEndOffset);
		}
	}
#if SUPPORTS_SCRIPTPATCH_LOADING
	else
	{
		//@script patcher
		// now we swap the linker's current loader with the archive that contains the object data for the exports that this script patcher added
		FArchive* SavedLoader = GetLinker()->Loader;
		GetLinker()->Loader = GetLinker()->GetScriptPatchArchive();

		// then create a patch reader which will contain only the function's updated bytecode
		FPatchReader BytecodePatcher(*PatchedBytecode);
		BytecodePatcher.SetLoader(GetLinker());

		// and serialize it!
		INT iCode = 0;
		while( iCode < ScriptBytecodeSize )
		{	
			SerializeExpr( iCode, BytecodePatcher );
		}
		if( iCode != ScriptBytecodeSize )
		{	
			appErrorf( TEXT("Script serialization mismatch: Got %i, expected %i"), iCode, ScriptBytecodeSize );
		}

		// now restore the linker's previous loader
		GetLinker()->Loader = SavedLoader;
	}
#endif

	if( Ar.IsLoading() )
	{
		// Collect references to objects embedded in script and store them in easily accessible array. This is skipped if
		// the struct is disregarded for GC as the references won't be of any use.
		ScriptObjectReferences.Empty();
		if( !IsDisregardedForGC() )
		{
			FArchiveObjectReferenceCollector ObjectReferenceCollector( &ScriptObjectReferences );
			INT iCode2 = 0;
			while( iCode2 < Script.Num() )
			{	
				SerializeExpr( iCode2, ObjectReferenceCollector );
			}
		}
	
		// Link the properties.
		Link( Ar, TRUE );
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UStruct::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData);

#if WITH_EDITORONLY_DATA
	// Get rid of cpp and script text, except for Windows
	if (!(PlatformsToKeep & UE3::PLATFORM_Windows))
	{
		// Retain script text when cooking for PC so that modders can compile script.
		ScriptText	= NULL;
	}
	CppText		= NULL;
#endif // WITH_EDITORONLY_DATA
}

//
// Serialize an expression to an archive.
// Returns expression token.
//
EExprToken UStruct::SerializeExpr( INT& iCode, FArchive& Ar )
{
#define SERIALIZEEXPR_INC
#include "ScriptSerialization.h"
	return Expr;
#undef SERIALIZEEXPR_INC

#undef XFER
#undef XFERPTR
#undef XFERNAME
#undef XFER_LABELTABLE
#undef HANDLE_OPTIONAL_DEBUG_INFO
#undef XFER_FUNC_POINTER
#undef XFER_FUNC_NAME
#undef XFER_PROP_POINTER
}

void UStruct::PostLoad()
{
	Super::PostLoad();
}

/**
 * Creates new copies of components
 * 
 * @param	Data						pointer to the address of the UComponent referenced by this UComponentProperty
 * @param	DefaultData					pointer to the address of the default value of the UComponent referenced by this UComponentProperty
 * @param	DefaultsCount		the size of the buffer pointed to by DefaultValue
 * @param	Owner						the object that contains the component currently located at Data
 * @param	InstanceFlags				contains the mappings of instanced objects and components to their templates
 */
void UStruct::InstanceComponentTemplates( BYTE* Data, BYTE* DefaultData, INT DefaultsCount, UObject* Owner, FObjectInstancingGraph* InstanceGraph )
{
	checkSlow(Data);
	checkSlow(Owner);

	for ( UProperty* Property = RefLink; Property != NULL; Property = Property->NextRef )
	{
		if (Property->HasAnyPropertyFlags(CPF_Component))
		{
			Property->InstanceComponents( Data + Property->Offset, (DefaultData && (Property->Offset < DefaultsCount)) ? DefaultData + Property->Offset : NULL, Owner, InstanceGraph );
		}
	}
}

/**
 * Instances any UObjectProperty values that still match the default value.
 *
 * @param	Value				the address where the pointers to the instanced object should be stored.  This should always correspond to (for class member properties) the address of the
 *								UObject which contains this data, or (for script structs) the address of the struct's data
 *								address of the struct's data
 * @param	DefaultValue		the address where the pointers to the default value is stored.  Evaluated the same way as Value
 * @param	DefaultsCount		the size of the buffer pointed to by DefaultValue
 * @param	OwnerObject			the object that contains the destination data.  Will be the used as the Outer for any newly instanced subobjects.
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UStruct::InstanceSubobjectTemplates( BYTE* Value, BYTE* DefaultValue, INT DefaultsCount, UObject* OwnerObject, FObjectInstancingGraph* InstanceGraph/*=NULL*/ ) const
{
	checkSlow(Value);
	checkSlow(OwnerObject);

	for ( UProperty* Property = RefLink; Property != NULL; Property = Property->NextRef )
	{
		if ( Property->ContainsInstancedObjectProperty() )
		{
			Property->InstanceSubobjects(Value + Property->Offset, (DefaultValue && (Property->Offset < DefaultsCount)) ? DefaultValue + Property->Offset : NULL, OwnerObject, InstanceGraph);
		}
	}
}

void UStruct::PropagateStructDefaults()
{
	// flag any functions which contain struct properties that have defaults
	for( TFieldIterator<UFunction> Functions(this,FALSE); Functions; ++Functions )
	{
		UFunction* Function = *Functions;
		for ( TFieldIterator<UStructProperty> Parameters(Function,FALSE); Parameters; ++Parameters )
		{
			UStructProperty* Prop = *Parameters;
			if ( (Prop->PropertyFlags&CPF_Parm) == 0 && Prop->Struct->GetDefaultsCount() > 0 )
			{
				Function->FunctionFlags |= FUNC_HasDefaults;
				break;
			}
		}
	}
}

IMPLEMENT_CLASS(UStruct);

/*-----------------------------------------------------------------------------
	UScriptStruct.
-----------------------------------------------------------------------------*/
UScriptStruct::UScriptStruct( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags, UScriptStruct* InSuperStruct )
:	UStruct			( EC_NativeConstructor, InSize, InName, InPackageName, InFlags, InSuperStruct )
,	StructDefaults	()
{}
UScriptStruct::UScriptStruct( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags )
:	UStruct			( EC_StaticConstructor, InSize, InName, InPackageName, InFlags )
,	StructDefaults	()
{}
UScriptStruct::UScriptStruct( UScriptStruct* InSuperStruct )
:	UStruct( InSuperStruct )
,	StructDefaults	()
{}

void UScriptStruct::AllocateStructDefaults()
{
	// We must use the struct's aligned size so that if Struct's aligned size is larger than its PropertiesSize, we don't overrun the defaults when
	// UStructProperty::CopyCompleteValue performs an appMemcpy using the struct property's ElementSize (which is always aligned)
	const INT BufferSize = Align(GetPropertiesSize(),GetMinAlignment());

	StructDefaults.Empty( BufferSize );
	StructDefaults.AddZeroed( BufferSize );
}

void UScriptStruct::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	// serialize the struct's flags
	Ar << StructFlags;

	// serialize the struct's defaults

	// look to see if our parent struct is a script struct, and has any defaults
	BYTE* SuperDefaults = Cast<UScriptStruct>(GetSuperStruct()) ? ((UScriptStruct*)GetSuperStruct())->GetDefaults() : NULL;

	// mark the archive we are serializing defaults
	Ar.StartSerializingDefaults();
	if( Ar.IsLoading() || Ar.IsSaving() )
	{
		// Allocate struct defaults on load.
		if( Ar.IsLoading() )
		{
			AllocateStructDefaults();
		}
		// Ensure struct defaults has enough memory to hold data.
		else
		{
			check(StructDefaults.Num()==Align(GetPropertiesSize(),GetMinAlignment()));
		}

		if( Ar.WantBinaryPropertySerialization() )
		{
			SerializeBin( Ar, &StructDefaults(0), 0 );
		}
		else
		{
			SerializeTaggedProperties( Ar, &StructDefaults(0), GetSuperStruct(), SuperDefaults );
		}
	}
	else
	{
		if( StructDefaults.Num() == 0 )
		{
			check(StructDefaults.Num()==Align(GetPropertiesSize(),GetMinAlignment()));
		}

		StructDefaults.CountBytes( Ar );
		SerializeBin( Ar, &StructDefaults(0), 0 );
	}

	// mark the archive we that we are no longer serializing defaults
	Ar.StopSerializingDefaults();
}

void UScriptStruct::PropagateStructDefaults()
{
	BYTE* DefaultData = GetDefaults();
	if ( DefaultData != NULL )
	{
		for( TFieldIterator<UStructProperty> It(this,FALSE); It; ++It )
		{
			UStructProperty* StructProperty = *It;

			// don't overwrite the values of properties which are marked native, since these properties
			// cannot be serialized by script.  For example, this would otherwise overwrite the 
			// VfTableObject property of UObject, causing all UObjects to have a NULL v-table.
			if ( (StructProperty->PropertyFlags&CPF_Native) == 0 )
			{
				StructProperty->InitializeValue( DefaultData + StructProperty->Offset );
			}
		}
	}

	Super::PropagateStructDefaults();
}

void UScriptStruct::FinishDestroy()
{
	DefaultStructPropText=TEXT("");
	Super::FinishDestroy();
}
IMPLEMENT_CLASS(UScriptStruct);

/*-----------------------------------------------------------------------------
	UState.
-----------------------------------------------------------------------------*/

UState::UState( UState* InSuperState )
: UStruct( InSuperState )
{}

UState::UState( ENativeConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags, UState* InSuperState )
: UStruct( EC_NativeConstructor, InSize, InName, InPackageName, InFlags, InSuperState )
, ProbeMask( 0 )
, StateFlags( 0 )
, LabelTableOffset( 0 )
{}

UState::UState( EStaticConstructor, INT InSize, const TCHAR* InName, const TCHAR* InPackageName, EObjectFlags InFlags )
: UStruct( EC_StaticConstructor, InSize, InName, InPackageName, InFlags )
, ProbeMask( 0 )
, StateFlags( 0 )
, LabelTableOffset( 0 )
{}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UState::StaticConstructor()
{
	GetClass();
	//@todo rtgc: UState::TMap<FName,UFunction*> FuncMap;
}

void UState::Serialize( FArchive& Ar )
{
	//@script patcher (bRebuildFunctionMap)
	const UBOOL bRebuildFunctionMap = HasAnyFlags(RF_PendingFieldPatches);
	Super::Serialize( Ar );

	Ar.ThisContainsCode();

	// @script patcher
	// if serialization set the label table offset, we want to ignore what we read below
	WORD const TmpLabelTableOffset = LabelTableOffset;

	// Class/State-specific union info.
	Ar << ProbeMask;
	Ar << LabelTableOffset << StateFlags;
	// serialize the function map
	//@todo ronp - why is this even serialized anyway?
	Ar << FuncMap;

	// @script patcher
	if (TmpLabelTableOffset > 0)
	{
		// ignore the serialized data by reverting to saved offset
		LabelTableOffset = TmpLabelTableOffset;
	}

	//@script patcher
	if ( bRebuildFunctionMap )
	{
		for ( TFieldIterator<UFunction> It(this,FALSE); It; ++It )
		{
			// if this is a UFunction, we must also add it to the owning struct's lookup map
			FuncMap.Set(It->GetFName(),*It);
		}
	}
}
IMPLEMENT_CLASS(UState);

/*-----------------------------------------------------------------------------
	UClass implementation.
-----------------------------------------------------------------------------*/

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UClass::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UClass, ClassWithin ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( UClass, NetFields ) );
	//@todo rtgc: I don't believe we need to handle TArray<FRepRecord> UClass::ClassReps;
	TheClass->EmitObjectReference( STRUCT_OFFSET( UClass, ClassDefaultObject ) );
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void UClass::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );
	for( TMap<FName,UComponent*>::TIterator It(ComponentNameToDefaultObjectMap); It; ++It )
	{
		AddReferencedObject( ObjectArray, It.Value() );
	}
	for (TArray<FImplementedInterface>::TIterator It(Interfaces); It; ++It)
	{
		AddReferencedObject(ObjectArray, It->Class);
	}
}

UObject* UClass::GetDefaultObject( UBOOL bForce /* = FALSE */ )
{
	if ( ClassDefaultObject == NULL )
	{
		UBOOL bCreateObject = bForce;
		if ( !bCreateObject )
		{
			// when running make, only create default objects for intrinsic classes
			if ( !GIsUCCMake )
			{
				bCreateObject = TRUE;
			}
			else
			{
				bCreateObject = HasAnyClassFlags(CLASS_Intrinsic) || (GUglyHackFlags&HACK_ClassLoadingDisabled) != 0;
			}
		}

		if ( bCreateObject )
		{
			QWORD LoadFlags = RF_Public|RF_ClassDefaultObject|RF_NeedLoad;

			UClass* ParentClass = GetSuperClass();
			UObject* ParentDefaultObject = NULL;
			if ( ParentClass != NULL )
			{
				ParentDefaultObject = ParentClass->GetDefaultObject(bForce);
			}

			if ( ParentDefaultObject != NULL || this == UObject::StaticClass() )
			{
				ClassDefaultObject = StaticConstructObject(this, GetOuter(), NAME_None, LoadFlags, ParentDefaultObject);

				// Perform static construction.
				if( HasAnyFlags(RF_Native) && ClassDefaultObject != NULL )
				{
					// only allowed to not have a static constructor during make
					check(ClassStaticConstructor||GIsUCCMake);
					if ( ClassStaticConstructor != NULL &&
						(!GetSuperClass() || GetSuperClass()->ClassStaticConstructor!=ClassStaticConstructor) )
					{
						(ClassDefaultObject->*ClassStaticConstructor)();
					}

					ConditionalLink();
				}
			}
		}
	}
	return ClassDefaultObject;
}

//
// Register the native class.
//
void UClass::Register()
{
	Super::Register();

	// Get stashed registration info.
	const TCHAR* InClassConfigName = *(TCHAR**)&ClassConfigName;
	ClassConfigName = InClassConfigName;

	// Propagate inherited flags.
	if (SuperStruct != NULL)
	{
		UClass* SuperClass = GetSuperClass();
		ClassFlags |= (SuperClass->ClassFlags & CLASS_Inherit);
		ClassCastFlags |= SuperClass->ClassCastFlags;
	}

	// Ensure that native classes receive a default object as soon as they are registered, so that
	// the class static constructor can be called (which uses the class default object) immediately.
	GetDefaultObject();
}

UBOOL UClass::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	UBOOL bSuccess = Super::Rename( InName, NewOuter, Flags );

	// If we have a default object, rename that to the same package as the class, and rename so it still matches the class name (Default__ClassName)
	if(bSuccess && (ClassDefaultObject != NULL))
	{
		// Make new, correct name for default object
		TCHAR Result[NAME_SIZE] = DEFAULT_OBJECT_PREFIX;
		appStrncat(Result, *GetName(), NAME_SIZE);

		ClassDefaultObject->Rename(Result, NewOuter, Flags);
	}

	// Now actually rename the class
	return bSuccess;
}


/**
 * Ensures that UClass::Link() isn't called until it is valid to do so.  For intrinsic classes, this shouldn't occur
 * until their non-intrinsic parents have been fully loaded (otherwise the intrinsic class's UProperty linked lists
 * won't contain any properties from the parent class)
 */
void UClass::ConditionalLink()
{
	// We need to know whether we're running the make commandlet before allowing any classes to be linked, since classes must
	// be allowed to link prior to serialization during make (since .u files may not exist).  However, GIsUCCMake can't be
	// set until after we've checked for outdated script files - otherwise, GIsUCCMake wouldn't be set if the user was attempting
	// to load a map and the .u files were outdated.  The check for outdated script files required GConfig, which isn't initialized until
	// appInit() returns;  appInit() calls UObject::StaticInit(), which registers all classes contained in Core.  Therefore, we need to delay
	// any linking until we can be sure that we're running make or not - by checking for GSys (which is set immediately after appInit() returns)
	// we can tell whether the value of GUglyHackFlags is reliable or not.
	if ( GSys != NULL && bNeedsPropertiesLinked )
	{
		// script classes are normally linked at the end of UClass::Serialize, so only intrinsic classes should be
		// linked manually.  During make, .u files may not exist, so allow all classes to be linked from here.
		if ( HasAnyClassFlags(CLASS_Intrinsic) || (GUglyHackFlags&HACK_ClassLoadingDisabled) != 0 )
		{
			UBOOL bReadyToLink = TRUE;

			UClass* ParentClass = GetSuperClass();
			if ( ParentClass != NULL )
			{
				ParentClass->ConditionalLink();

				// we're not ready to link until our parent has successfully linked, or we aren't able to load classes from disk
				bReadyToLink = ParentClass->PropertyLink != NULL || (GUglyHackFlags&HACK_ClassLoadingDisabled) != 0;
			}

			if ( bReadyToLink )
			{
				// we must have a class default object by this point.
				checkSlow(ClassDefaultObject!=NULL);
				if (SuperStruct != NULL && HasAnyClassFlags(CLASS_Intrinsic))
				{
					// re-propagate the class flags from the parent class, so that any class flags that were loaded from disk will be
					// correctly inherited
					UClass* SuperClass = GetSuperClass();
					ClassFlags |= (SuperClass->ClassFlags & CLASS_Inherit);
					ClassCastFlags |= SuperClass->ClassCastFlags;
				}

				// now link the class, which sets up PropertyLink, ConfigLink, ConstructorLink, etc.
				FArchive DummyAr;
				Link(DummyAr,FALSE);

				// we may have inherited some properties which require construction (arrays, strings, etc.),
				// so now we should reinitialize the class default object against its archetype.
				ClassDefaultObject->InitClassDefaultObject(this);
				if ( ClassStaticInitializer != NULL )
				{
					// now that we've linked the class and constructed our own versions of any ctor props,
					// we're ready to allow the intrinsic classes to initialize their values from C++
					(ClassDefaultObject->*ClassStaticInitializer)();
				}

				// ok, everything is now hooked up - all UProperties of this class are linked in (including inherited properties)
				// and all values have been initialized (where desired)...now we're ready to load config and localized values
				if ( !GIsUCCMake )
				{
					ClassDefaultObject->LoadConfig();
					ClassDefaultObject->LoadLocalized();
				}
			}
		}
	}
}

/**
 * Find the class's native constructor.
 */
void UClass::Bind()
{
	UStruct::Bind();
	checkf(GIsEditor || GetSuperClass() || this==UObject::StaticClass(), TEXT("Unable to bind %s at this time"), *GetPathName());
	if( !ClassConstructor && HasAnyFlags(RF_Native) && !GIsEditor )
	{
		appErrorf( TEXT("Can't bind to native class %s"), *GetPathName() );
	}
	if( !ClassConstructor && GetSuperClass() )
	{
		// Chase down constructor in parent class.
		GetSuperClass()->Bind();
		ClassConstructor = GetSuperClass()->ClassConstructor;

		// propagate casting flags.
		ClassCastFlags |= GetSuperClass()->ClassCastFlags;
	}
#if WITH_LIBFFI
	if( DLLBindName != NAME_None )
	{
		FString DLLBindNameString = DLLBindName.ToString();
		// Check the user is not trying load a DLL outside the Binaries folder.
		if( DLLBindNameString.InStr(TEXT(":"))==-1 && DLLBindNameString.InStr(TEXT("\\"))==-1 && 
			DLLBindNameString.InStr(TEXT("/"))==-1 && DLLBindNameString.InStr(TEXT("."))==-1 )
		{
			// Set current directory to be the UserCode folder, should the DLL load other DLLs
			FString UserCodeDir = FString(appBaseDir()) + TEXT("UserCode");
			GFileManager->SetCurDirectory(*UserCodeDir);
			DLLBindNameString = UserCodeDir + PATH_SEPARATOR + DLLBindNameString + TEXT(".DLL");
			DLLBindHandle = appGetDllHandle( *DLLBindNameString );

			if( DLLBindHandle != NULL )
			{
				// Attempt to call DLLBindInit function.
				struct FDLLBindInitData
				{
					INT Version;
					void* (*ReallocFunction)(void* Original, DWORD Count, DWORD Alignment);
				};
				typedef void (*DLLBindInitFunctionPtrType)(FDLLBindInitData* InitData);

				DLLBindInitFunctionPtrType DLLBindInitFunctionPtr = (DLLBindInitFunctionPtrType)appGetDllExport( DLLBindHandle, TEXT("DLLBindInit") );
				if( DLLBindInitFunctionPtr != NULL )
				{
					FDLLBindInitData InitData;
					InitData.Version = 1;
					InitData.ReallocFunction = &appRealloc;
					(*DLLBindInitFunctionPtr)( &InitData );
				}
			}

			// Restore current directory.
			GFileManager->SetDefaultDirectory();
		}
		if( DLLBindHandle == NULL )
		{
			warnf( NAME_Warning, TEXT("Class %s can't bind to DLL %s"), *GetPathName(), *DLLBindNameString );
		}
	}
#endif
	check(GIsEditor || ClassConstructor);
}

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
UComponent* UClass::FindComponent( FName ComponentName, UBOOL bRecurse/*=FALSE*/ )
{
	UComponent* Result = NULL;

	UComponent** TemplateComponent = ComponentNameToDefaultObjectMap.Find(ComponentName);
	if ( TemplateComponent != NULL )
	{
		Result = *TemplateComponent;
	}

	if ( Result == NULL && ClassDefaultObject != NULL )
	{
		Result = ClassDefaultObject->FindComponent(ComponentName,bRecurse);
	}
	return NULL;
}

UBOOL UClass::ChangeParentClass( UClass* NewParentClass )
{
	check(NewParentClass);

	// changing parent classes is only allowed when running make
	if ( !GIsUCCMake )
	{
		return FALSE;
	}

#if !CONSOLE
	// only native script classes should ever change their parent on-the-fly, and
	// we can only change parents if we haven't been parsed yet.
	if ( HasAnyClassFlags(CLASS_Parsed|CLASS_Intrinsic) )
	{
		return FALSE;
	}

	// if we don't have an existing parent class, can't change it
	UClass* CurrentParent = GetSuperClass();
	if ( CurrentParent == NULL )
	{
		return FALSE;
	}

	// Cannot change our parent to a class that is a child of this class
	if ( NewParentClass->IsChildOf(this) )
	{
		return FALSE;
	}

	// First, remove the class flags that were inherited from the old parent class
	DWORD InheritedClassFlags = (CurrentParent->ClassFlags&CLASS_Inherit);
	ClassFlags &= ~InheritedClassFlags;
	ClassCastFlags &= ~StaticClassCastFlags;

	// then propagate the new parent's inheritable class flags
	ClassFlags |= (NewParentClass->ClassFlags&CLASS_ScriptInherit)|StaticClassFlags;
	ClassCastFlags |= NewParentClass->ClassCastFlags;

	// if this class already has a class default object, we'll need to detach it
	if ( ClassDefaultObject != NULL )
	{
		// ClassConfigName - figure out if this class defined its own StaticConfigName
		if ( ClassDefaultObject != NULL && !ClassDefaultObject->HasUniqueStaticConfigName() )
		{
			// if we inherited the StaticConfigName function from our ParentClass, change our ConfigName
			// to the new parent's ConfigName
			if ( NewParentClass->ClassDefaultObject != NULL )
			{
				ClassConfigName = NewParentClass->StaticConfigName();
			}
		}

		// break down any existing properties
		ClassDefaultObject->ExitProperties((BYTE*)ClassDefaultObject, this);

		// reset the object's vftable so that it doesn't cause an access violation when it's destructed
		(*UObject::StaticClass()->ClassConstructor)( ClassDefaultObject );

		// clear the reference
		ClassDefaultObject = NULL;
	}

	// next, copy the category info from the new parent class
#if !CONSOLE && !DEDICATED_SERVER
	HideCategories = NewParentClass->HideCategories;
	AutoExpandCategories = NewParentClass->AutoExpandCategories;
	AutoCollapseCategories = NewParentClass->AutoCollapseCategories;
	DontSortCategories = NewParentClass->DontSortCategories;
	bForceScriptOrder = NewParentClass->bForceScriptOrder;
#endif

	// we're now considered misaligned
	SetFlags(RF_MisalignedObject);
	SuperStruct = NewParentClass;

	// finally, tell all our children to re-initialize themselves so that the new
	// data is propagated correctly
	for ( TObjectIterator<UClass> It; It; ++It )
	{
		if ( It->GetSuperClass() == this )
		{
			It->ChangeParentClass(this);
		}
	}

	return TRUE;
#endif
}


UBOOL UClass::HasNativesToExport( UObject* InOuter )
{
#if !CONSOLE
	if( HasAnyFlags( RF_Native ) )
	{
		if( HasAnyFlags( RF_TagExp ) )
		{
			return TRUE;
		}

		if( ScriptText && GetOuter() == InOuter )
		{
			for( TFieldIterator<UFunction> Function(this,FALSE); Function; ++Function )
			{
				if( Function->FunctionFlags & FUNC_Native )
				{
					return TRUE;
				}
			}
		}
	}
#endif
	return FALSE;			
}


/**
 * Determines whether this class's memory layout is different from the C++ version of the class.
 * 
 * @return	TRUE if this class or any of its parent classes is marked as misaligned
 */
UBOOL UClass::IsMisaligned()
{
#if CONSOLE
	// Currently - we never compile script on console, so misalignment can't happen
	// (The RF_MisalignedObject flag is only set by the script compiler...)
	return FALSE;
#else
	UBOOL bResult = FALSE;
	for ( UClass* TestClass = this; TestClass; TestClass = TestClass->GetSuperClass() )
	{
		if ( TestClass->HasAnyFlags(RF_MisalignedObject) )
		{
			bResult = TRUE;
			break;
		}
	}

	return bResult;
#endif
}

/**
 * Returns the struct/ class prefix used for the C++ declaration of this struct/ class.
 * Classes deriving from AActor have an 'A' prefix and other UObject classes an 'U' prefix.
 *
 * @return Prefix character used for C++ declaration of this struct/ class.
 */
const TCHAR* UClass::GetPrefixCPP()
{
	UClass* TheClass		= this;
	UBOOL	bIsActorClass	= FALSE;
	UBOOL	bIsDeprecated	= TheClass->HasAnyClassFlags(CLASS_Deprecated);
	while( TheClass && !bIsActorClass )
	{
		bIsActorClass	= TheClass->GetFName() == NAME_Actor;
		TheClass		= TheClass->GetSuperClass();
	}

	if( bIsActorClass )
	{
		if( bIsDeprecated )
		{
			return TEXT("ADEPRECATED_");
		}
		else
		{
			return TEXT("A");
		}
	}
	else
	{
		if( bIsDeprecated )
		{
			return TEXT("UDEPRECATED_");
		}
		else
		{
			return TEXT("U");
		}		
	}
}

FString UClass::GetDescription() const
{
	// Look up the the classes name in the INT file and return the class name if there is no match.
	FString Description = Localize( TEXT("Objects"), *GetName(), TEXT("Descriptions"), GetLanguage(), 1 );
	if( Description.Len() )
		return Description;
	else
		return FString( GetName() );
}

//	UClass UObject implementation.

IMPLEMENT_COMPARE_POINTER( UField, UnClass, { return A->GetNetIndex() - B->GetNetIndex(); } )

void UClass::FinishDestroy()
{
	// Empty arrays.
	//warning: Must be emptied explicitly in order for intrinsic classes
	// to not show memory leakage on exit.
	NetFields.Empty();

	ClassDefaultObject = NULL;
#if !CONSOLE
	DefaultPropText = TEXT("");
#endif

#if WITH_LIBFFI
	if( DLLBindHandle != NULL )
	{
		appFreeDllHandle(DLLBindHandle);
		DLLBindHandle = NULL;
	}
#endif

	Super::FinishDestroy();
}
void UClass::PostLoad()
{
	check(ClassWithin);
	Super::PostLoad();

	// Postload super.
	if( GetSuperClass() )
	{
		GetSuperClass()->ConditionalPostLoad();
	}
}
void UClass::Link( FArchive& Ar, UBOOL Props )
{
	Super::Link( Ar, Props );
	if( !GIsEditor )
	{
		NetFields.Empty();
		ClassReps = (SuperStruct != NULL) ? GetSuperClass()->ClassReps : TArray<FRepRecord>();
		for( TFieldIterator<UField> It(this,FALSE); It; ++It )
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

	// Emit tokens for all properties that are unique to this class.
	if( Props )
	{
		// Iterate over properties defined in this class
		for( TFieldIterator<UProperty> It(this,FALSE); It; ++It)
		{
			UProperty* Property = *It;
			Property->EmitReferenceInfo( &ReferenceTokenStream, 0 );
		}

		// If this class has state local variables, update their offsets into a
		// shared memory buffer and emit tokens.
		//
		// NOTE:
		// I am not particually happy with this implementation.  It seems a bit
		// hacky to update the state properties offset here.
		//
		// I thought about placing the properties in the UClass object instead of
		// each UState, but then I became concerned about the size difference when
		// allocating the object instance and what changes would be neccessary
		// to not affect native header generation.
		//
		// I also thought about creating a special UScriptStruct at compile time
		// and placing all state locals in that object.  Again, I was concerned
		// about all of the code that dealt with UScriptStruct loading and what
		// special case rules I would need to add to that.

		INT StateOffset = 0;
		DWORD SkipIndexIndex = DWORD(INDEX_NONE);

		// Iterate over state local variables defined in this class
		for (TFieldIterator<UState> StateIt(this); StateIt && StateIt->GetOwnerClass() == this; ++StateIt)
		{
			UState* State = *StateIt;
			if (State->StateFlags & STATE_HasLocals)
			{
				for (TFieldIterator<UProperty> It(State, FALSE); It; ++It)
				{
					UProperty* Property = *It;
					Property->Offset += StateOffset;
					if (Property->ContainsObjectReference())
					{
						if (SkipIndexIndex == DWORD(INDEX_NONE))
						{
							FGCReferenceInfo ReferenceInfo(GCRT_StateLocals, 0);
							ReferenceTokenStream.EmitReferenceInfo(ReferenceInfo);
							SkipIndexIndex = ReferenceTokenStream.EmitSkipIndexPlaceholder();
						}
						Property->EmitReferenceInfo(&ReferenceTokenStream, 0);
					}
				}

				StateOffset += State->PropertiesSize;
			}
		}

		if (SkipIndexIndex != DWORD(INDEX_NONE))
		{
			const DWORD SkipIndex = ReferenceTokenStream.EmitReturn();
			ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
		}
 	}

	bNeedsPropertiesLinked = PropertyLink == NULL && (GUglyHackFlags&HACK_ClassLoadingDisabled) == 0;
}

void UClass::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Variables.
	Ar << ClassFlags;
	Ar << ClassWithin << ClassConfigName;
	Ar << ComponentNameToDefaultObjectMap;
	Ar << Interfaces;
#if !CONSOLE

#if DEDICATED_SERVER
	// Consume all this data in a non-cooked environment (the variables are not in the class)
	if (!GIsSeekFreePCServer)
	{
		TArray<FName> UnusedArray;
		UBOOL UnusedBool;
		FString UnusedString;
		if( Ar.Ver() >= VER_DONTSORTCATEGORIES_ADDED )
		{
			Ar << UnusedArray;
		}

		Ar << UnusedArray << UnusedArray << UnusedArray;

		if( Ar.Ver() >= VER_FORCE_SCRIPT_DEFINED_ORDER_PER_CLASS )
		{
			Ar << UnusedBool;
		}

		if( Ar.Ver() >= VER_ADDED_CLASS_GROUPS )
		{
			Ar << UnusedArray;
		}

		Ar << UnusedString;
	}
#else
	// if reading data that's cooked for console/pcserver, skip this data
	UBOOL const bIsCookedForConsole = IsPackageCookedForConsole(Ar);
	UBOOL const bIsCookedForPCServer = Ar.GetLinker() && (Ar.GetLinker()->LinkerRoot->PackageFlags & PKG_Cooked) && (GPatchingTarget & UE3::PLATFORM_WindowsServer);	
	UBOOL const bCookingConsoleOrPCServer = (GCookingTarget & (UE3::PLATFORM_Console|UE3::PLATFORM_WindowsServer)) != 0;

	if ( !bIsCookedForConsole && !bIsCookedForPCServer && 
		(!bCookingConsoleOrPCServer || !Ar.IsSaving() || Ar.GetLinker() == NULL) )
	{
		if( Ar.Ver() >= VER_DONTSORTCATEGORIES_ADDED )
		{
			Ar << DontSortCategories;
		}

		Ar << HideCategories << AutoExpandCategories << AutoCollapseCategories;

		if( Ar.Ver() >= VER_FORCE_SCRIPT_DEFINED_ORDER_PER_CLASS )
		{
			Ar << bForceScriptOrder;
		}
		else
		{
			bForceScriptOrder = 0;
		}

		if( Ar.Ver() >= VER_ADDED_CLASS_GROUPS )
		{
			Ar << ClassGroupNames;
		}

		Ar << ClassHeaderFilename;
	}
#endif //DEDICATED_SERVER
#endif //!CONSOLE

	if( Ar.Ver() >= VER_SCRIPT_BIND_DLL_FUNCTIONS )
	{
#if WITH_LIBFFI
		Ar << DLLBindName;
#else
		FName Dummy = NAME_None;
		Ar << Dummy;
#endif
	}

	// Defaults.

	// mark the archive as serializing defaults
	Ar.StartSerializingDefaults();

	if( Ar.IsLoading() )
	{
		checkf((DWORD)Align(GetPropertiesSize(), GetMinAlignment()) >= sizeof(UObject), TEXT("Aligned size is %d, sizeof if %d"), Align(GetPropertiesSize(), GetMinAlignment()), sizeof(UObject));
		check(!GetSuperClass() || !GetSuperClass()->HasAnyFlags(RF_NeedLoad));
		Ar << ClassDefaultObject;

		// In order to ensure that the CDO inherits config & localized property values from the parent class, we can't initialize the CDO until
		// the parent class's CDO has serialized its data from disk and called LoadConfig/LoadLocalized - this occurs in ULinkerLoad::Preload so the
		// call to InitClassDefaultObject [for non-intrinsic classes] is deferred until then.
		// When running make, we don't load data from .ini/.int files, so there's no need to wait
		if ( GIsUCCMake )
		{
			ClassDefaultObject->InitClassDefaultObject(this);
		}

		ClassUnique = 0;
	}
	else
	{
#if SUPPORTS_SCRIPTPATCH_CREATION
		//@script patcher
		check(GIsScriptPatcherActive||GetDefaultsCount()==GetPropertiesSize());
#else
		check(GetDefaultsCount()==GetPropertiesSize());
#endif

		// only serialize the class default object if the archive allows serialization of ObjectArchetype
		// otherwise, serialize the properties that the ClassDefaultObject references
		// The logic behind this is the assumption that the reason for not serializing the ObjectArchetype
		// is because we are performing some actions on objects of this class and we don't want to perform
		// that action on the ClassDefaultObject.  However, we do want to perform that action on objects that
		// the ClassDefaultObject is referencing, so we'll serialize it's properties instead of serializing
		// the object itself
		if ( !Ar.IsIgnoringArchetypeRef() )
		{
			Ar << ClassDefaultObject;
		}
		else if ( ClassDefaultObject != NULL )
		{
			ClassDefaultObject->Serialize(Ar);
		}
	}

	// mark the archive we that we are no longer serializing defaults
	Ar.StopSerializingDefaults();
}

/** serializes the passed in object as this class's default object using the given archive
 * @param Object the object to serialize as default
 * @param Ar the archive to serialize from
 */
void UClass::SerializeDefaultObject(UObject* Object, FArchive& Ar)
{
	Object->SerializeNetIndex(Ar);

	// tell the archive that it's allowed to load data for transient properties
	Ar.StartSerializingDefaults();

	//@script patcher: if script patches were applied to our linker, then we need to check to see if this struct's bytecode is being patched.
	// the linker should have a mapping of ExportMap index => bytecode patches, so we can check with negligable performance impact 
	FPatchData* PatchedDefaults = NULL;

	if( GIsUCCMake || ((Ar.IsLoading() || Ar.IsSaving()) && !Ar.WantBinaryPropertySerialization()) )
	{
#if SUPPORTS_SCRIPTPATCH_LOADING
		//@{
		//@script patcher
	    if ( Ar.IsLoading() )
	    {
		    ULinker* LinkerAr = Ar.GetLinker();
		    if ( LinkerAr != NULL )
		    {
			    ULinkerLoad* Linker = Object->GetLinker();
			    checkSlow(LinkerAr == Linker);
			    
			    PatchedDefaults = Linker->FindDefaultsPatch(Object->GetLinkerIndex());
			    if ( PatchedDefaults != NULL )
			    {
				    // first, move the regular file reader beyond the data that we're going to be skipping
				    FObjectExport& CDOExport = Linker->ExportMap(Object->GetLinkerIndex());
				    Ar.Seek(CDOExport.SerialOffset + CDOExport.SerialSize);

				    // then create a patch reader containing only the modified default properties for this object
				    FPatchReader BytecodePatcher(*PatchedDefaults);
				    BytecodePatcher.SetLoader(Linker);

				    // need to re-serialize the NetIndex from the patch reader
				    Object->SerializeNetIndex(BytecodePatcher);

				    // then read in all the property data
				    SerializeTaggedProperties(BytecodePatcher, (BYTE*)Object, GetSuperClass(), (BYTE*)Object->GetArchetype());
			    }
		    }
	    }
		//@}
#endif
	    if ( PatchedDefaults == NULL )
	    {
		    // class default objects do not always have a vtable (such as when saving the object
		    // during make), so use script serialization as opposed to native serialization to
		    // guarantee that all property data is loaded into the correct location
		    SerializeTaggedProperties(Ar, (BYTE*)Object, GetSuperClass(), (BYTE*)Object->GetArchetype());
		}
	}
	else if ( Ar.GetPortFlags() != 0 )
	{
		SerializeBinEx(Ar, (BYTE*)Object, (BYTE*)Object->GetArchetype(), GetSuperClass() ? GetSuperClass()->GetPropertiesSize() : 0 );
	}
	else
	{
		SerializeBin(Ar, (BYTE*)Object, 0);
	}
	Ar.StopSerializingDefaults();
}

void UClass::PropagateStructDefaults()
{
	BYTE* DefaultData = GetDefaults();
	if ( DefaultData != NULL )
	{
		for( TFieldIterator<UStructProperty> It(this,FALSE); It; ++It )
		{
			UStructProperty* StructProperty = *It;

			// don't overwrite the values of properties which are marked native, since these properties
			// cannot be serialized by script.  For example, this would otherwise overwrite the 
			// VfTableObject property of UObject, causing all UObjects to have a NULL v-table.
			if ( (StructProperty->PropertyFlags&CPF_Native) == 0 )
			{
				StructProperty->InitializeValue( DefaultData + StructProperty->Offset );
			}
		}
	}

	Super::PropagateStructDefaults();
}

FArchive& operator<<(FArchive& Ar, FImplementedInterface& A)
{
	Ar << A.Class << A.PointerProperty;
	return Ar;
}

/*-----------------------------------------------------------------------------
	UClass constructors.
-----------------------------------------------------------------------------*/

/**
 * Internal constructor.
 */
UClass::UClass()
:	ClassWithin( UObject::StaticClass() )
#if WITH_LIBFFI
,	DLLBindName(NAME_None)
,	DLLBindHandle(NULL)
#endif
{}

/**
 * Create a new UClass given its superclass.
 */
UClass::UClass( UClass* InBaseClass )
:	UState( InBaseClass )
,	ClassWithin( UObject::StaticClass() )
,	ClassDefaultObject( NULL )
,	bNeedsPropertiesLinked( TRUE )
#if WITH_LIBFFI
,	DLLBindName(NAME_None)
,	DLLBindHandle(NULL)
#endif
{
	UClass* ParentClass = GetSuperClass();
	if( ParentClass )
	{
		ClassWithin = ParentClass->ClassWithin;
		Bind();

		// if this is a native class, we may have defined a StaticConfigName() which overrides
		// the one from the parent class, so get our config name from there
		if ( HasAnyFlags(RF_Native) )
		{
			ClassConfigName = StaticConfigName();
		}
		else
		{
			// otherwise, inherit our parent class's config name
			ClassConfigName = ParentClass->ClassConfigName;
		}
	}

	if ( !GIsUCCMake )
	{
		// this must be after the call to Bind(), so that the class has a ClassStaticConstructor
		UObject* DefaultObject = GetDefaultObject();
		if ( DefaultObject != NULL )
		{
			DefaultObject->InitClassDefaultObject(this);
			DefaultObject->LoadConfig();
			DefaultObject->LoadLocalized();
		}
	}
}

/**
 * UClass autoregistry constructor.
 * warning: Called at DLL init time.
 */
UClass::UClass
(
	ENativeConstructor,
	DWORD			InSize,
	DWORD			InClassFlags,
	DWORD			InClassCastFlags,
	UClass*			InSuperClass,
	UClass*			InWithinClass,
	const TCHAR*	InNameStr,
	const TCHAR*    InPackageName,
	const TCHAR*    InConfigName,
	EObjectFlags	InFlags,
	void			(*InClassConstructor)(void*),
	void			(UObject::*InClassStaticConstructor)(),
	void			(UObject::*InClassStaticInitializer)()

)
:	UState					( EC_NativeConstructor, InSize, InNameStr, InPackageName, InFlags, InSuperClass!=this ? InSuperClass : NULL )
,	ClassFlags				( InClassFlags | CLASS_Native )
,	ClassCastFlags			( InClassCastFlags )
,	ClassUnique				( 0 )
,	ClassWithin				( InWithinClass )
,	ClassConfigName			()
,	NetFields				()
,	ClassDefaultObject		( NULL )
,	ClassConstructor		( InClassConstructor )
,	ClassStaticConstructor	( InClassStaticConstructor )
,	ClassStaticInitializer	( InClassStaticInitializer )
,	bNeedsPropertiesLinked	( TRUE )
#if !CONSOLE && !DEDICATED_SERVER
,	bForceScriptOrder		(FALSE)
#endif
#if WITH_LIBFFI
,	DLLBindName(NAME_None)
,	DLLBindHandle(NULL)
#endif
{
	*(const TCHAR**)&ClassConfigName = InConfigName;
}

/**
 * Called when statically linked.
 */
UClass::UClass
(
	EStaticConstructor,
	DWORD			InSize,
	DWORD			InClassFlags,
	DWORD			InClassCastFlags,
	const TCHAR*	InNameStr,
	const TCHAR*    InPackageName,
	const TCHAR*    InConfigName,
	EObjectFlags	InFlags,
	void			(*InClassConstructor)(void*),
	void			(UObject::*InClassStaticConstructor)(),
	void			(UObject::*InClassStaticInitializer)()
)
:	UState					( EC_StaticConstructor, InSize, InNameStr, InPackageName, InFlags )
,	ClassFlags				( InClassFlags | CLASS_Native )
,	ClassCastFlags			( InClassCastFlags )
,	ClassUnique				( 0 )
,	ClassWithin				( NULL )
,	ClassConfigName			()
,	NetFields				()
,	ClassDefaultObject		( NULL )
,	ClassConstructor		( InClassConstructor )
,	ClassStaticConstructor	( InClassStaticConstructor )
,	ClassStaticInitializer	( InClassStaticInitializer )
,	bNeedsPropertiesLinked	( TRUE )
#if !CONSOLE && !DEDICATED_SERVER
,	bForceScriptOrder		(FALSE)
#endif
#if WITH_LIBFFI
,	DLLBindName(NAME_None)
,	DLLBindHandle(NULL)
#endif
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
#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if( !GIsScriptPatcherActive ) 
	{
#endif
		Ar << Label.Name;
		Ar << Label.iCode;
#if SUPPORTS_SCRIPTPATCH_CREATION
	}
	else
	{
		//@script patcher
		// use class layout trickery to get the data we need
		Ar << *(NAME_INDEX*)&Label.Name; // NameIndex
		Ar << *(INT*)(((BYTE*)&Label.Name)+sizeof(NAME_INDEX)); // Number
		Ar << Label.iCode;
	}
#endif

	return Ar;
}

/*-----------------------------------------------------------------------------
	UFunction.
-----------------------------------------------------------------------------*/

UFunction::UFunction( UFunction* InSuperFunction )
: UStruct( InSuperFunction ), FirstStructWithDefaults(NULL)
{}
void UFunction::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar.ThisContainsCode();

	// Function info.
	Ar << iNative;
	Ar << OperPrecedence;
	Ar << FunctionFlags;

	// Replication info.
	if( FunctionFlags & FUNC_Net )
	{
		Ar << RepOffset;
	}

	// Precomputation.
	if( Ar.IsLoading() )
	{
		NumParms          = 0;
		ParmsSize         = 0;
		ReturnValueOffset = MAXWORD;
		for( UProperty* Property=Cast<UProperty>(Children); Property; Property=Cast<UProperty>(Property->Next) )
		{
			if (Property->PropertyFlags & CPF_Parm)
			{
				NumParms++;
				ParmsSize = Property->Offset + Property->GetSize();
				if( Property->PropertyFlags & CPF_ReturnParm )
					ReturnValueOffset = Property->Offset;
			}
			else if ( (FunctionFlags&FUNC_HasDefaults) != 0 )
			{
				UStructProperty* StructProp = Cast<UStructProperty>(Property,CLASS_IsAUStructProperty);
				if ( StructProp && StructProp->Struct->GetDefaultsCount() )
				{
					FirstStructWithDefaults = StructProp;
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

#if !CONSOLE
	UBOOL const bIsCookedForConsole = IsPackageCookedForConsole(Ar);
	if ( !bIsCookedForConsole && (!Ar.IsSaving() || !GIsCooking || !(GCookingTarget & UE3::PLATFORM_Console)) )
	{
		Ar << FriendlyName;
	}
#endif
}
void UFunction::PostLoad()
{
	Super::PostLoad();
}
UProperty* UFunction::GetReturnProperty()
{
	for( TFieldIterator<UProperty> It(this); It && (It->PropertyFlags & CPF_Parm); ++It )
	{
		if( It->PropertyFlags & CPF_ReturnParm )
		{
			return *It;
		}
	}
	return NULL;
}
void UFunction::Bind()
{
	UClass* OwnerClass = GetOwnerClass();

	// if this isn't a native function, or this function belongs to a native interface class (which has no C++ version), 
	// use ProcessInternal (call into script VM only) as the function pointer for this function
	if( !HasAnyFunctionFlags(FUNC_Native) || OwnerClass->HasAnyClassFlags(CLASS_Interface) )
	{
#if WITH_LIBFFI
		if( HasAnyFunctionFlags(FUNC_DLLImport) && OwnerClass->DLLBindHandle != NULL )
		{
			DLLImportFunctionPtr = appGetDllExport( OwnerClass->DLLBindHandle, *GetName() );
			Func = NULL;	// Should not be used.
		}
		else
#endif
		{
			// Use UnrealScript processing function.
			check(iNative==0);
			Func = &UObject::ProcessInternal;
		}
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
		ANSICHAR Proc[MAX_SPRINTF];

		// @todo: We could remove the PrefixCPP if we exported to the headers without it
		appStrcpyANSI(Proc, TCHAR_TO_ANSI(OwnerClass->GetPrefixCPP()));
		appStrcatANSI(Proc, TCHAR_TO_ANSI(*OwnerClass->GetName()));
		appStrcatANSI(Proc, "exec");
		appStrcatANSI(Proc, TCHAR_TO_ANSI(*GetName()));

		// look up the native function by string name
		Func = FindNative(OwnerClass->GetFName(), Proc);
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

UConst::UConst(const TCHAR* InValue)
:	Value(InValue)
{}
void UConst::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Value;
}
IMPLEMENT_CLASS(UConst);
