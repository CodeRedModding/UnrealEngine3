/*=============================================================================
	UnType.h: Unreal engine base type definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	UProperty.
-----------------------------------------------------------------------------*/

// Property exporting flags.
enum EPropertyPortFlags
{
	/** Indicates that property data should be treated as text */
	PPF_Localized					= 0x00000001,

	/** Indicates that property data should be wrapped in quotes (for some types of properties) */
	PPF_Delimited					= 0x00000002,

	/** Indicates that the object reference should be verified */
	PPF_CheckReferences			= 0x00000004, 
	
	PPF_ExportsNotFullyQualified	= 0x00000008,
	
	PPF_AttemptNonQualifiedSearch	= 0x00000010,
	
	/** Indicates that importing values for config or localized properties is disallowed */
	PPF_RestrictImportTypes		= 0x00000020,
	
	/**
	 * only include properties that have the CPF_Config|CPF_GlobalConfig for Import/ExportText;
	 * not yet completely implemented as this will require major code changes to ensure that
	 * all properties declared inside structs are marked appropriate as config
	 */
	PPF_ConfigOnly				= 0x00000040,
	
	/** only include properties that have the CPF_Localized flag for ImportText/ExportText */
	PPF_LocalizedOnly				= 0x00000080,

	/** only include properties which are marked CPF_Component */
	PPF_ComponentsOnly			= 0x00000100,

	/**
	 * Only applicable to component properties (for now)
	 * Indicates that two object should be considered identical
	 * if the property values for both objects are all identical
	 */
	PPF_DeepComparison			= 0x00000200,

	/**
	 * Similar to PPF_DeepComparison, except that template components are always compared using standard object
	 * property comparison logic (basically if the pointers are different, then the property isn't identical)
	 */
	PPF_DeepCompareInstances		= 0x00000400,

	/**
	 * Set if this operation is copying in memory (for copy/paste) instead of exporting to a file. There are
	 * some subtle differences between the two
	 */
	PPF_Copy						= 0x00000800,

	/** Set when duplicating objects via serialization */
	PPF_Duplicate					= 0x00001000,

	/** Indicates that object property values should be exported without the package or class information */
	PPF_SimpleObjectText			= 0x00002000,

	/** object properties should not be exported/imported */
	PPF_SkipObjectProperties		= 0x00004000,

	/** parsing default properties - allow text for transient properties to be imported - also modifies ObjectProperty importing slightly for subobjects */
	PPF_ParsingDefaultProperties	= 0x00008000,

	/** force binary serialization of structs */
	PPF_ForceBinarySerialization	= 0x00010000,

	/** indicates that non-categorized transient properties should be exported (by default, they would not be) */
	PPF_IncludeTransient			= 0x00020000,

	/** modifies behavior of UProperty::Identical - indicates that the comparison is between an object and its archetype */
	PPF_DeltaComparison			= 0x00040000,

	/** indicates that we're exporting properties for display in the property window. - used to hide EditHide items in collapsed structs */
	PPF_PropertyWindow			= 0x00080000,

	/** indicates that we are exporting an object in a form that can be parsed as a classes defprops by the script compiler */
	PPF_ExportDefaultProperties	= 0x00100000,

	/** makes friendly rotational text */
	PPF_ExportAsFriendlyRotation	= 0x00200000,
};

enum EPropertyExportCPPFlags
{
	/** Indicates that we are exporting this property's CPP text for an optional parameter value */
	CPPF_OptionalValue			=	0x00000001,
};

/** formats available for ImportText calls. */
enum EImportTextFormat
{
	/** Standard Unreal text format (matches INI/LOC files) */
	ITF_Unreal,
	/** JSON Format. More or less a superset of JSON (supports the Unreal-specific number/string parsing exactly) */
	ITF_JSON,
};

union UPropertyValue
{
	BYTE					ByteValue;
	INT					IntValue;
	UBOOL				BoolValue;
	FLOAT				FloatValue;

	UObject*				ObjectValue;
	UComponent*			ComponentValue;
	UClass*				ClassValue;

	/**
	 * @caution: when this union is filled via UProperty::GetPropertyValue(), the following values
	 * will point to the actual data contained in the object, NOT a copy of the data
	 */
	FName*				NameValue;
	FString*				StringValue;
	FScriptArray*			ArrayValue;
	FScriptDelegate*		DelegateValue;
	FScriptInterface*		InterfaceValue;
};

void UStructProperty_ExportTextItem(class UScriptStruct* InStruct, FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal);
const TCHAR* UStructProperty_ImportText(class UScriptStruct* Struct, const TCHAR* InBuffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText);

//
// An UnrealScript variable.
//
class UProperty : public UField
{
	DECLARE_ABSTRACT_CASTED_CLASS_INTRINSIC(UProperty,UField,0,Core,CASTCLASS_UProperty)
	DECLARE_WITHIN(UField)

	// Persistent variables.
	INT			ArrayDim;
	INT			ElementSize;
	QWORD		PropertyFlags;
	WORD			RepOffset;
	WORD			RepIndex;
#if !CONSOLE
	FName		Category;

	/**
	 * For static array properties which use enums for the array size, corresponds to the UEnum object that was used to specify the array size
	 *
	 * @fixme ronp - should be removed once the general meta-data system is implemented
	 */
	UEnum*		ArraySizeEnum;
#endif

	// In memory variables (generated during Link()).
	INT			Offset;
	UProperty*	PropertyLinkNext;
	UProperty*	ConstructorLinkNext;
	UProperty*	NextRef;

	// Constructors.
	UProperty();
	UProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	/** @name UProperty interface. */
	//@{
	void ExportCppDeclaration( FOutputDevice& Out, UBOOL IsMember, UBOOL IsParm, UBOOL bImportsDefaults ) const;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const PURE_VIRTUAL(UProperty::GetCPPMacroType,return TEXT(""););

	/**
	 * Returns the C++ name of the property, including the _DEPRECATED suffix if the 
	 * property is deprecated.
	 *
	 * @return C++ name of property
	 */
	FString GetNameCPP() const;

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const PURE_VIRTUAL(UProperty::GetCPPType,return TEXT(""););

	virtual void Link( FArchive& Ar, UProperty* Prev );
	/**
	 * Determines whether the property values are identical.
	 * 
	 * @param	A/B			property data to be compared
	 * @param	PortFlags	allows caller more control over how the property values are compared
	 *
	 * @return	TRUE if the property values are identical
	 */
	virtual UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const PURE_VIRTUAL(UProperty::Identical,return FALSE;);
	virtual void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes=0, void* Defaults=NULL ) const PURE_VIRTUAL(UProperty::SerializeItem,);
	virtual UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const PURE_VIRTUAL(UProperty::ExportTextItem,);
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const PURE_VIRTUAL(UProperty::ImportText,return NULL;);
	UBOOL ExportText( INT ArrayElement, FString& ValueStr, const BYTE* Data, const BYTE* Delta, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;

	/**
	 * Retrieves the value of this property and copies it into the union.
	 *
	 * @param	PropertyValueAddress	the address where the value of this property is stored. This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	out_PropertyValue		[out] will be filled with the value located at PropertyValueAddress intepreted as the type this UProperty handles
	 *
	 * @return	TRUE if out_PropertyValue was filled with a property value.  FALSE if this UProperty type doesn't support the union (structs and maps) or the address is invalid.
	 */
	virtual UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const PURE_VIRTUAL(UProperty::GetPropertyValue,return FALSE;);
	/**
	 * Sets the value for this property using the source value specified in PropertyValue
	 *
	 * @param	PropertyValueAddress	the address where the value should be copied to. This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	PropertyValue			contains the value that should be copied into the PropertyValueAddress.
	 *
	 * @return	TRUE if the PropertyValue was copied successfully into PropertyValueAddress.  FALSE if this UProperty type doesn't support the union
	 *			(structs and maps) or the address is invalid.
	 */
	virtual UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const PURE_VIRTUAL(UProperty::SetPropertyValue,return FALSE;);

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
	 * @param	InstanceGraph
	 *						contains the mappings of instanced objects and components to their templates
	 */
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	/**
	 * Copy the value for all elements of this property.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 * @param	SubobjectRoot		the first object in DestOwnerObject's Outer chain that is not a subobject.  SubobjectRoot will be the same as DestOwnerObject if DestOwnerObject is not a subobject (which normally indicates that we are simply duplicating an object)
	 * @param	DestOwnerObject		the object that contains the destination data.  Only specified when creating the member properties for an object; DestOwnerObject is the object that will contain any instanced subobjects.
	 * @param	InstanceGraph
	 *						contains the mappings of instanced objects and components to their templates
	 */
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;

	/**
	 * Determines whether this property has a non-zero value.
	 *
	 * @param	Data		the address of the value for this property
	 * @param	PortFlags	allows the caller to modify the logic for determining whether this property has a value.
	 *
	 * @return	TRUE if the address specified by Data contains a non-zero value.
	 */
	virtual UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const PURE_VIRTUAL(UProperty::HasValue,return FALSE;);

	/**
	 * Zeros the value for this property.
	 *
	 * @param	Data		the address of the value for this property that should be cleared.
	 * @param	PortFlags	allows the caller to modify which types of properties are cleared (e.g. loc only, etc.)
	 */
	virtual void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const PURE_VIRTUAL(UProperty::ClearValue,);
	virtual void DestroyValue( void* Dest ) const;

	/**
	 * Verify that modifying this property's value via ImportText is allowed.
	 * 
	 * @param	PortFlags	the flags specified in the call to ImportText
	 * @param	ErrorText	[out] set to the error message that should be displayed if returns false
	 *
	 * @return	TRUE if ImportText should be allowed
	 */
	UBOOL ValidateImportFlags( DWORD PortFlags, FOutputDevice* ErrorText = NULL ) const;
	UBOOL Port( DWORD PortFlags=0 ) const;
	virtual FName GetID() const;
	virtual UBOOL IsLocalized() const;
	virtual UBOOL RequiresInit() const { return false; }

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) {}

	virtual INT GetMinAlignment() const { return 1; }

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsInstancedObjectProperty() const
	{
		return FALSE;
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
	virtual void InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, struct FObjectInstancingGraph* InstanceGraph=NULL ) const {}

	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset );

	/** @name Inlines
	 * bNoOffset means that the A and B pointers are pointers to the actual property data, not the outer class data
	 */
	//@{
	UBOOL Matches( const void* A, const void* B, INT ArrayIndex, UBOOL bNoOffset=FALSE, DWORD PortFlags=0 ) const
	{
		INT Ofs = (bNoOffset ? 0 : Offset) + ArrayIndex * ElementSize;
		return Identical( (BYTE*)A + Ofs, B ? (BYTE*)B + Ofs : NULL, PortFlags );
	}
	INT GetSize() const
	{
		return ArrayDim * ElementSize;
	}
	UBOOL ShouldSerializeValue( FArchive& Ar ) const
	{
		UBOOL Skip = FALSE;
		if( (PropertyFlags & (CPF_Native|CPF_Transient|CPF_DuplicateTransient|CPF_ArchetypeProperty|CPF_NonTransactional|CPF_Deprecated|CPF_DevelopmentAssets)) != 0 )
		{
			Skip =	((PropertyFlags & CPF_Native) != 0)
				||	((PropertyFlags & CPF_Transient) != 0 && Ar.IsPersistent() && !Ar.IsSerializingDefaults() )
				||	((PropertyFlags & CPF_DuplicateTransient) != 0 && (Ar.GetPortFlags() & PPF_Duplicate) != 0)
				||  (IsEditorOnlyProperty() && Ar.IsFilterEditorOnly() )
				||	((PropertyFlags & CPF_ArchetypeProperty) != 0 && Ar.IsIgnoringArchetypeRef())
				||	((PropertyFlags & CPF_NonTransactional) != 0 && Ar.IsTransacting() )
				||	((PropertyFlags & CPF_Deprecated) != 0 && ( (Ar.IsSaving() && !GIsUCCMake) || Ar.IsTransacting() || Ar.WantBinaryPropertySerialization() ));
		}
		return !Skip;
	}

	/**
	 * Determines whether this property value is eligible for copying when duplicating an object
	 * 
	 * @return	TRUE if this property value should be copied into the duplicate object
	 */
	UBOOL ShouldDuplicateValue() const
	{
		return Port() && GetOwnerClass() != UObject::StaticClass();
	}

	/**
	 * Returns the first UProperty in this property's Outer chain that does not have a UProperty for an Outer
	 */
	UProperty* GetOwnerProperty()
	{
		UProperty* Result=this;
		for ( UProperty* PropBase=Cast<UProperty>(GetOuter()); PropBase; PropBase=Cast<UProperty>(PropBase->GetOuter()) )
		{
			Result = PropBase;
		}
		return Result;
	}

	/**
	 * Determines if the property has any metadata associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return TRUE if there is a (possibly blank) value associated with this key
	 */
	UBOOL HasMetaData(const TCHAR* Key) const
	{
		UPackage* Package = GetOutermost();
		check(Package);

		UMetaData* MetaData = Package->GetMetaData();
		check(MetaData);

		UBOOL bHasMetaData = MetaData->HasValue(this, Key);
		
		// If meta data isn't present, see if it's available as intrinsic class meta data in an ini file
		if ( !bHasMetaData )
		{
			UClass* OwnerClass = GetOwnerClass();
			check(OwnerClass);
			
			if ( UMetaData::AttemptParseIntrinsicMetaData( *GetOwnerClass(), *this, *MetaData ) )
			{
				bHasMetaData = MetaData->HasValue(this, Key);
			}
		}
		return bHasMetaData;
	}

	/**
	 * Find the metadata value associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key
	 */
	const FString& GetMetaData(const TCHAR* Key) const
	{
		UPackage* Package = GetOutermost();
		check(Package);

		UMetaData* MetaData = Package->GetMetaData();
		check(MetaData);

		const FString& MetaDataString = MetaData->GetValue(this, Key);
		
		// If meta data isn't present, see if it's available as intrinsic class meta data in an ini file
		if ( MetaDataString.Len() == 0 )
		{
			UClass* OwnerClass = GetOwnerClass();
			check(OwnerClass);

			if ( UMetaData::AttemptParseIntrinsicMetaData( *OwnerClass, *this, *MetaData ) )
			{
				return MetaData->GetValue(this, Key);
			}
		}
		return MetaDataString;
	}

	/**
	 * Sets the metadata value associated with the key
	 * 
	 * @param Key The key to lookup in the metadata
	 * @return The value associated with the key
	 */
	void SetMetaData(const TCHAR* Key, const FString& InValue)
	{
		UPackage* Package = GetOutermost();
		check(Package);

		return Package->GetMetaData()->SetValue(this, Key, InValue);
	}


	/**
	* Find the metadata value associated with the key
	* and return UBOOL 
	* @param Key The key to lookup in the metadata
	* @return return TRUE if the value was TRUE (case insensitive)
	*/
	UBOOL GetBoolMetaData(const TCHAR* Key) const
	{		
		const FString & BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "TRUE");
	}
	
	/**
	* Find the metadata value associated with the key
	* and return INT 
	* @param Key The key to lookup in the metadata
	* @return the int value stored in the metadata.
	*/
	INT GetINTMetaData(const TCHAR* Key) const
	{
		const FString & INTString = GetMetaData(Key);
		INT Value = appAtoi(*INTString);
		return Value;
	}

	/**
	* Find the metadata value associated with the key
	* and return FLOAT
	* @param Key The key to lookup in the metadata
	* @return the float value stored in the metadata.
	*/
	FLOAT GetFLOATMetaData(const TCHAR* Key) const
	{
		const FString & FLOATString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		FLOAT Value = appAtof(*FLOATString);
		return Value;
	}

	/**
	 * Gets the user-friendly, localized (if exists) name of this property
	 *
	 * @param	OwnerClass	if specified, uses this class's loc file instead of the property's owner class
	 *						useful for overriding the friendly name given a property inherited from a parent class.
	 *
	 * @return	the friendly name for this property.  localized first, then metadata, then the property's name.
	 */
	FString GetFriendlyName( UClass* OwnerClass=NULL ) const;

	/**
	 * Returns this property's propertyflags
	 */
	FORCEINLINE QWORD GetPropertyFlags() const
	{
		return PropertyFlags;
	}
	FORCEINLINE void SetPropertyFlags( QWORD NewFlags )
	{
		PropertyFlags |= NewFlags;
	}
	FORCEINLINE void ClearPropertyFlags( QWORD NewFlags )
	{
		PropertyFlags &= ~NewFlags;
	}
	/**
	 * Used to safely check whether any of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and UBOOL is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an UBOOL.
	 *
	 * @param FlagsToCheck	Object flags to check for.
	 *
	 * @return				TRUE if any of the passed in flags are set, FALSE otherwise  (including no flags passed in).
	 */
	FORCEINLINE UBOOL HasAnyPropertyFlags( QWORD FlagsToCheck ) const
	{
		return (PropertyFlags & FlagsToCheck) != 0 || FlagsToCheck == CPF_AllFlags;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and UBOOL is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an UBOOL.
	 *
	 * @param FlagsToCheck	Object flags to check for
	 *
	 * @return TRUE if all of the passed in flags are set (including no flags passed in), FALSE otherwise
	 */
	FORCEINLINE UBOOL HasAllPropertyFlags( QWORD FlagsToCheck ) const
	{
		return ((PropertyFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Returns the replication owner, which is the property itself, or NULL if this isn't important for replication.
	 * It is relevant if the property is a net relevant and not being run in the editor
	 */
	FORCEINLINE UProperty* GetRepOwner()
	{
		return (!GIsEditor && ((PropertyFlags & CPF_Net) != 0)) ? this : NULL;
	}

	/**
	 * Editor-only properties are those that only are used with the editor is present or cannot be removed from serialisation.
	 * Editor-only properties include: EditorOnly properties, NotForConsole properties
	 * Properties that cannot be removed from serialisation are:
	 *		Boolean properties (may affect GCC_BITFIELD_MAGIC computation)
	 *		Native properties (native serialisation)
	 */
	virtual UBOOL IsEditorOnlyProperty() const
	{
		return (PropertyFlags & CPF_DevelopmentAssets) != 0;
	}

	//@}
	//@}
};

/*-----------------------------------------------------------------------------
	UByteProperty.
-----------------------------------------------------------------------------*/

//
// Describes an unsigned byte value or 255-value enumeration variable.
//
class UByteProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UByteProperty,UProperty,0,Core,CASTCLASS_UByteProperty)

	// Variables.
	UEnum* Enum;

	// Constructors.
	UByteProperty()
	{}
	UByteProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UEnum* InEnum=NULL )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	Enum( InEnum )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UIntProperty.
-----------------------------------------------------------------------------*/

//
// Describes a 32-bit signed integer variable.
//
class UIntProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UIntProperty,UProperty,0,Core,CASTCLASS_UIntProperty)

	// Constructors.
	UIntProperty()
	{}
	UIntProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

//
// Describes a single bit flag variable residing in a 32-bit unsigned double word.
//
class UBoolProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UBoolProperty,UProperty,0,Core,CASTCLASS_UBoolProperty)

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
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;

	/**
	 * This is the boolean property override to ensure boolean properties are still included in the property list on the console to avoid the additional
	 * complexity of handling GCC_BITFIELD_MAGIC.
	 */
	virtual UBOOL IsEditorOnlyProperty() const
	{
		return FALSE;
	}
};

/*-----------------------------------------------------------------------------
	UFloatProperty.
-----------------------------------------------------------------------------*/

//
// Describes an IEEE 32-bit floating point variable.
//
class UFloatProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UFloatProperty,UProperty,0,Core,CASTCLASS_UFloatProperty)

	// Constructors.
	UFloatProperty()
	{}
	UFloatProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UObjectProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class UObjectProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UObjectProperty,UProperty,0,Core,CASTCLASS_UObjectProperty)

	// Variables.
	class UClass* PropertyClass;

	// Constructors.
	UObjectProperty()
	{}
	UObjectProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UClass* InClass )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	PropertyClass( InClass )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	// UObjectProperty interface.
private:
	/**
	 * Worker for CopySingleValue/CopyCompleteValue
	 */
	void InstanceValue( BYTE* DestAddress, BYTE* SrcAddress, UObject* SubobjectRoot, UObject* DestOwnerObject, FObjectInstancingGraph* InstanceGraph ) const;

public:

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
	static UBOOL ParseObjectPropertyValue( const UProperty* Property, UObject* OwnerObject, UClass* RequiredMetaClass, DWORD PortFlags, const TCHAR*& Buffer, UObject*& out_ResolvedValue );
	static UObject* FindImportedObject( const UProperty* Property, UObject* OwnerObject, UClass* ObjectClass, UClass* RequiredMetaClass, const TCHAR* Text, DWORD PortFlags = 0);

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsInstancedObjectProperty() const;

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
	virtual void InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	
	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset );

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UComponentProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class UComponentProperty : public UObjectProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UComponentProperty,UObjectProperty,0,Core,CASTCLASS_UComponentProperty)

	// UProperty interface.
	virtual FName GetID() const
	{
		return NAME_ObjectProperty;
	}

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph );

	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class UClassProperty : public UObjectProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UClassProperty,UObjectProperty,0,Core,CASTCLASS_UClassProperty)

	// Variables.
	class UClass* MetaClass;

	// Constructors.
	UClassProperty()
	{}
	UClassProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UClass* InMetaClass )
	:	UObjectProperty( EC_CppProperty, InOffset, InCategory, InFlags, UClass::StaticClass() )
	,	MetaClass( InMetaClass )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	FName GetID() const
	{
		return NAME_ObjectProperty;
	}
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UInterfaceProperty.
-----------------------------------------------------------------------------*/

/**
 * This variable type provides safe access to a native interface pointer.  The data class for this variable is FScriptInterface, and is exported to auto-generated
 * script header files as a TScriptInterface.
 */
class UInterfaceProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInterfaceProperty,UProperty,0,Core,CASTCLASS_UInterfaceProperty)

	/** The native interface class that this interface property refers to */
	class	UClass*		InterfaceClass;

	/* === UProperty interface. === */
	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;

	virtual void Link( FArchive& Ar, UProperty* Prev );
	virtual UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	virtual void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	virtual UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const { return TRUE; }

	/* === UObject interface. === */

	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset );

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	/** Manipulates the data referenced by this UProperty */
	void Serialize( FArchive& Ar );
};

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

//
// Describes a name variable pointing into the global name table.
//
class UNameProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UNameProperty,UProperty,0,Core,CASTCLASS_UNameProperty)

	// Constructors.
	UNameProperty()
	{}
	UNameProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic string variable.
//
class UStrProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStrProperty,UProperty,0,Core,CASTCLASS_UStrProperty)

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
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	void DestroyValue( void* Dest ) const;
	virtual INT GetMinAlignment() const;
	virtual UBOOL RequiresInit() const { return true; }

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic array.
//
class UArrayProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UArrayProperty,UProperty,0,Core,CASTCLASS_UArrayProperty)

	// Variables.
	UProperty* Inner;

	// Constructors.
	UArrayProperty()
	{}
	UArrayProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	/** @name UProperty interface */
	//@{
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;

	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	void DestroyValue( void* Dest ) const;
	UBOOL IsLocalized() const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph );

	virtual INT GetMinAlignment() const;
	virtual UBOOL RequiresInit() const { return true; }

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsInstancedObjectProperty() const;

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
	virtual void InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;

	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset );
	//@}

	// UArrayProperty interface.
	void AddCppProperty( UProperty* Property );

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;
};

/*-----------------------------------------------------------------------------
	UMapProperty.
-----------------------------------------------------------------------------*/

/**
 *  Describes a dynamic map.
 */
class UMapProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UMapProperty,UProperty,0,Core,CASTCLASS_UMapProperty)

	// Variables.
	UProperty* Key;
	UProperty* Value;

	// Constructors.
	UMapProperty()
	{}
	UMapProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const {}
	void DestroyValue( void* Dest ) const;
	UBOOL IsLocalized() const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) {}

	virtual INT GetMinAlignment() const;
	virtual UBOOL RequiresInit() const
	{
#if TMAPS_IMPLEMENTED
		return TRUE; 
#else
		return FALSE;
#endif
	}


	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsInstancedObjectProperty() const;

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
	virtual void InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	
	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset );

	void AddCppProperty( UProperty* Property );
	//@}

#if !TMAPS_IMPLEMENTED
	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
	{
		return FALSE;
	}
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
	{
		return FALSE;
	}
#endif
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
	DECLARE_CASTED_CLASS_INTRINSIC(UStructProperty,UProperty,0,Core,CASTCLASS_UStructProperty)

	// Variables.
	class UScriptStruct* Struct;

	// Constructors.
	UStructProperty()
	{}
	UStructProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags, UScriptStruct* InStruct )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	,	Struct( InStruct )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	void DestroyValue( void* Dest ) const;
	UBOOL IsLocalized() const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the UComponent referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the UComponent referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceFlags		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceComponents( BYTE* Data, BYTE* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph );

	virtual INT GetMinAlignment() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsInstancedObjectProperty() const;

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
	virtual void InstanceSubobjects( void* Value, void* DefaultValue, UObject* OwnerObject, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;

	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset );

	// UStructProperty Interface
	void InitializeValue( BYTE* Dest ) const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const
	{
		return FALSE;
	}
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const
	{
		return FALSE;
	}
};

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

/**
 * Describes a pointer to a function bound to an Object.
 */
class UDelegateProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UDelegateProperty,UProperty,0,Core,CASTCLASS_UDelegateProperty)

	/** Function this delegate is mapped to */
	UFunction* Function;

	/**
	 * If this DelegateProperty corresponds to an actual delegate variable (as opposed to the hidden property the script compiler creates when you declare a delegate function)
	 * points to the source delegate function (the function declared with the delegate keyword) used in the declaration of this delegate property.
	 */
	UFunction* SourceDelegate;

	// Constructors.
	UDelegateProperty()
	{}
	UDelegateProperty( ECppProperty, INT InOffset, const TCHAR* InCategory, QWORD InFlags )
	:	UProperty( EC_CppProperty, InOffset, InCategory, InFlags )
	{}

	/**
	 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
	 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
	 * properties for native- only classes.
	 */
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UProperty interface.
	void Link( FArchive& Ar, UProperty* Prev );
	UBOOL Identical( const void* A, const void* B, DWORD PortFlags=0 ) const;
	void SerializeItem( FArchive& Ar, void* Value, INT MaxReadBytes, void* Defaults ) const;
	UBOOL NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data ) const;
	FString GetCPPType( FString* ExtendedTypeText=NULL, DWORD CPPExportFlags=0 ) const;
	FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual void ExportTextItem( FString& ValueStr, const BYTE* PropertyValue, const BYTE* DefaultValue, UObject* Parent, INT PortFlags, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual const TCHAR* ImportText( const TCHAR* Buffer, BYTE* Data, INT PortFlags, UObject* Parent, FOutputDevice* ErrorText = NULL, EImportTextFormat TextFormat = ITF_Unreal ) const;
	virtual void CopySingleValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	virtual void CopyCompleteValue( void* Dest, void* Src, UObject* SubobjectRoot=NULL, UObject* DestOwnerObject=NULL, struct FObjectInstancingGraph* InstanceGraph=NULL ) const;
	UBOOL HasValue( const BYTE* Data, DWORD PortFlags=0 ) const;
	void ClearValue( BYTE* Data, DWORD PortFlags=0 ) const;
	virtual INT GetMinAlignment() const;

	UBOOL GetPropertyValue( BYTE* PropertyValueAddress, UPropertyValue& out_PropertyValue ) const;
	UBOOL SetPropertyValue( BYTE* PropertyValueAddress, const UPropertyValue& PropertyValue ) const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return TRUE if property (or sub- properties) contain a UObject reference, FALSE otherwise
	 */
	virtual UBOOL ContainsObjectReference() const;
	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo(FGCReferenceTokenStream* ReferenceTokenStream, INT BaseOffset);
};

/**
 * This class represents the chain of member properties leading to an internal struct property.  It is used
 * for tracking which member property corresponds to the UScriptStruct that owns a particular property.
 */
class FEditPropertyChain : public TDoubleLinkedList<UProperty*>
{

public:
	/** Constructors */
	FEditPropertyChain() : ActivePropertyNode(NULL), ActiveMemberPropertyNode(NULL) {}

	/**
	 * Sets the ActivePropertyNode to the node associated with the property specified.
	 *
	 * @param	NewActiveProperty	the UProperty that is currently being evaluated by Pre/PostEditChange
	 *
	 * @return	TRUE if the ActivePropertyNode was successfully changed to the node associated with the property
	 *			specified.  FALSE if there was no node corresponding to that property.
	 */
	UBOOL SetActivePropertyNode( UProperty* NewActiveProperty );

	/**
	 * Sets the ActiveMemberPropertyNode to the node associated with the property specified.
	 *
	 * @param	NewActiveMemberProperty		the member UProperty which contains the property currently being evaluated
	 *										by Pre/PostEditChange
	 *
	 * @return	TRUE if the ActiveMemberPropertyNode was successfully changed to the node associated with the
	 *			property specified.  FALSE if there was no node corresponding to that property.
	 */
	UBOOL SetActiveMemberPropertyNode( UProperty* NewActiveMemberProperty );

	/**
	 * Returns the node corresponding to the currently active property.
	 */
	TDoubleLinkedListNode* GetActiveNode() const;

	/**
	 * Returns the node corresponding to the currently active property, or if the currently active property
	 * is not a member variable (i.e. inside of a struct/array), the node corresponding to the member variable
	 * which contains the currently active property.
	 */
	TDoubleLinkedListNode* GetActiveMemberNode() const;

protected:
	/**
	 * In a hierarchy of properties being edited, corresponds to the property that is currently
	 * being processed by Pre/PostEditChange
	 */
	TDoubleLinkedListNode* ActivePropertyNode;

	/**
	 * In a hierarchy of properties being edited, corresponds to the class member property which
	 * contains the property that is currently being processed by Pre/PostEditChange.  This will
	 * only be different from the ActivePropertyNode if the active property is contained within a struct,
	 * dynamic array, or static array.
	 */
	TDoubleLinkedListNode* ActiveMemberPropertyNode;


	/** TDoubleLinkedList interface */
	/**
	 * Updates the size reported by Num().  Child classes can use this function to conveniently
	 * hook into list additions/removals.
	 *
	 * This version ensures that the ActivePropertyNode and ActiveMemberPropertyNode point to a valid nodes or NULL if this list is empty.
	 *
	 * @param	NewListSize		the new size for this list
	 */
	virtual void SetListSize( INT NewListSize );
};

/*-----------------------------------------------------------------------------
	FCanEditChangeCache
	Helper structure for asking if a property is Editable
-----------------------------------------------------------------------------*/
class FPropertyWindowDataCache
{
public:
	/**Retrieves a current list of all USeqAct_Interps for use in property window updating */
	virtual const TArray<UObject*>& GetEditedSeqActInterps (void) = 0;
};

//-----------------------------------------------------------------------------
//EPropertyNodeFlags - Flags used internally by FPropertyNode
//-----------------------------------------------------------------------------
namespace EPropertyChangeType
{
	typedef UINT Type;

	//default value.  Add new enums to add new functionality.
	const Type Unspecified = 1 << 0;
	//Array Add
	const Type ArrayAdd = 1 << 1;
	//Value Set
	const Type ValueSet = 1 << 2;
	//Duplicate
	const Type Duplicate = 1 << 3;
	//Undo
	const Type Undo = 1 <<4;
	//Redo
	const Type Redo = 1 <<5;
};

/**
 * Structure for passing pre and post edit change events
 */
struct FPropertyChangedEvent
{
	//default constructor
	FPropertyChangedEvent(UProperty* InProperty, const UBOOL bInChangesTopology=FALSE, EPropertyChangeType::Type InChangeType=EPropertyChangeType::Unspecified)
	{
		Property = InProperty;
		bChangesTopology = bInChangesTopology;
		ChangeType = InChangeType;

		//default to out of bounds/unused
		ObjectIteratorIndex = -1;
		//default to no array index knowledge
		ArrayIndicesPerObject = NULL;
	}

	/**
	 * Saves off map of array indices per object being set.
	 */
	void SetArrayIndexPerObject(const TArray< TMap<FString,INT> >& InArrayIndices)
	{
		ArrayIndicesPerObject = &InArrayIndices; 
	}

	/**
	 * Gets the Array Index of the "current object" based on a particular name
	 * InName - Name of the property to find the array index for
	 */
	INT GetArrayIndex (const FString& InName)
	{
		//default to unknown index
		INT Retval = -1;
		if (ArrayIndicesPerObject && ArrayIndicesPerObject->IsValidIndex(ObjectIteratorIndex))
		{
			const INT* ValuePtr = (*ArrayIndicesPerObject)(ObjectIteratorIndex).Find(InName);
			if (ValuePtr)
			{
				Retval = *ValuePtr;
			}
		}
		return Retval;
	}


	UProperty*                Property;
	UBOOL                     bChangesTopology;
	EPropertyChangeType::Type ChangeType;
	//Used by the param system to say which object is receiving the event in the case of multi-select
	INT                       ObjectIteratorIndex;
private:
	//In the property window, multiple objects can be selected at once.  In the case of adding/inserting to an array, each object COULD have different indices for the new entries in the array
	const TArray< TMap<FString,INT> >* ArrayIndicesPerObject;
};

/**
 * Structure for passing pre and post edit change events
 */
struct FPropertyChangedChainEvent : public FPropertyChangedEvent
{
	FPropertyChangedChainEvent(FEditPropertyChain& InPropertyChain, FPropertyChangedEvent& SrcChangeEvent) :
		FPropertyChangedEvent(SrcChangeEvent),
		PropertyChain(InPropertyChain)
	{
	}
	FEditPropertyChain& PropertyChain;
};


/*-----------------------------------------------------------------------------
	Field templates.
-----------------------------------------------------------------------------*/

//
// Find a typed field in a struct.
//
template <class T> T* FindField( UStruct* Owner, const TCHAR* FieldName )
{
	// lookup the string name in the Name hash
	FName Name(FieldName, FNAME_Find);
	// If we didn't find it, we know the field won't exist in this Struct
	if (Name == NAME_None)
		return NULL;
	// Search by comparing FNames (INTs), not strings
	for( TFieldIterator<T>It( Owner ); It; ++It )
	{
		if( It->GetFName() == Name )
		{
			return *It;
		}
	}
	// If we didn't find it, return no field
	return NULL;
}

template <class T> T* FindField( UStruct* Owner, FName FieldName )
{
	// Search by comparing FNames (INTs), not strings
	for( TFieldIterator<T>It( Owner ); It; ++It )
	{
		if( It->GetFName() == FieldName )
		{
			return *It;
		}
	}

	// If we didn't find it, return no field
	return NULL;
}

/**
 * Search for the named field within the specified scope, including any Outer classes; assert on failure.
 *
 * @param	Scope		the scope to search for the field in
 * @param	FieldName	the name of the field to search for.
 */
template<typename T>
T* FindFieldChecked( UStruct* Scope, FName FieldName )
{
	if ( FieldName != NAME_None && Scope != NULL )
	{
		UStruct* InitialScope = Scope;
		do 
		{
			UStruct* OriginalScope = Scope;
			for ( Scope; Scope; Scope = Cast<UStruct>(Scope->GetOuter()) )
			{
				for ( TFieldIterator<T> It(Scope); It; ++It )
				{
					if ( It->GetFName() == FieldName )
					{
						return *It;
					}
				}
			}

			// now search Outer classes
			UClass* OuterClass = OriginalScope->GetOwnerClass()->ClassWithin;
			if ( OuterClass != NULL && OuterClass != UObject::StaticClass() )
			{
				Scope = OuterClass;
			}
		} while( Scope != NULL );
		
		appErrorf( TEXT("Failed to find %s %s in %s"), *T::StaticClass()->GetName(), *FieldName.ToString(), *InitialScope->GetFullName() );
	}

	return NULL;
}

/**
 * Dynamically cast a property to the specified type; if the type is a UArrayProperty, will return the UArrayProperty's Inner member, if it is of the correct type.
 */
template<typename T>
T* SmartCastProperty( UProperty* Src )
{
	T* Result = Cast<T>(Src);
	if ( Result == NULL )
	{
		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Src);
		if ( ArrayProp != NULL )
		{
			Result = Cast<T>(ArrayProp->Inner);
		}
	}
	return Result;
}

/*-----------------------------------------------------------------------------
	UObject accessors that depend on UClass.
-----------------------------------------------------------------------------*/

/**
 * @return	TRUE if this object is of the specified type.
 */
inline UBOOL UObject::IsA( const UClass* SomeBase ) const
{
	if ( SomeBase==NULL )
	{
		return TRUE;
	}

#if SUPPORTS_SCRIPTPATCH_CREATION
	//@script patcher
	if ( GIsScriptPatcherActive )
	{
		for( UClass* TempClass=Class; TempClass; TempClass=static_cast<UClass*>(TempClass->SuperStruct) )
		{
			// not exact, but close enough for script patching
			if( TempClass->GetFName()==SomeBase->GetFName() )
			{
				return TRUE;
			}
		}
		return FALSE;
	}
	else
#endif
	{
		for ( UClass* TempClass=Class; TempClass; TempClass=static_cast<UClass*>(TempClass->SuperStruct) )
		{
			if ( TempClass == SomeBase )
			{
				return TRUE;
			}
		}

		return FALSE;
	}
}

/**
 * @return	TRUE if the specified object appears somewhere in this object's outer chain.
 */
inline UBOOL UObject::IsIn( const UObject* SomeOuter ) const
{
	for( UObject* It=GetOuter(); It; It=It->GetOuter() )
	{
		if( It==SomeOuter )
		{
			return TRUE;
		}
	}
	return SomeOuter==NULL;
}

/**
 * Find out if this object is inside (has an outer) that is of the specified class
 * @param SomeBaseClass	The base class to compare against
 * @return True if this object is in an object of the given type.
 */
inline UBOOL UObject::IsInA( const UClass* SomeBaseClass ) const
{
	for (const UObject* It=this; It; It = It->GetOuter())
	{
		if (It->IsA(SomeBaseClass))
		{
			return TRUE;
		}
	}
	return SomeBaseClass == NULL;
}

/**
 * Determine if this object has SomeObject in its archetype chain.
 */
inline UBOOL UObject::IsBasedOnArchetype(  const UObject* const SomeObject ) const
{
	if ( this && SomeObject != this )
	{
		for ( UObject* Template = ObjectArchetype; Template; Template = Template->GetArchetype() )
		{
			if ( SomeObject == Template )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
* Checks whether this object's top-most package has any of the specified flags
*
* @param	CheckFlagMask	a bitmask of EPackageFlags values to check for
*
* @return	TRUE if the PackageFlags member of this object's top-package has any bits from the mask set.
*/
inline UBOOL UObject::RootPackageHasAnyFlags( DWORD CheckFlagMask ) const
{
	return (NULL != this && (GetOutermost()->PackageFlags&CheckFlagMask) != 0);
}
/**
* Checks whether this object's top-most package has the specified flags
*
* @param	CheckFlagMask	a bitmask of EPackageFlags values to check for
*
* @return	TRUE if the PackageFlags member of this object's top-package has all bits from the mask set.
*/
inline UBOOL UObject::RootPackageHasAllFlags( DWORD CheckFlagMask ) const
{
	return (NULL != this && (GetOutermost()->PackageFlags&CheckFlagMask) == CheckFlagMask);
}

//
// Return whether an object wants to receive a named probe message.
//
inline UBOOL UObject::IsProbing( FName ProbeName )
{
	return	(ProbeName.GetIndex() <  NAME_PROBEMIN)
	||		(ProbeName.GetIndex() >= NAME_PROBEMAX)
	||		(!StateFrame)
	||		(StateFrame->ProbeMask & ((DWORD)1 << (ProbeName.GetIndex() - NAME_PROBEMIN)));
}

/*-----------------------------------------------------------------------------
	UStruct inlines.
-----------------------------------------------------------------------------*/

//
// UStruct inline comparer.
//
inline UBOOL UStruct::StructCompare( const void* A, const void* B, DWORD PortFlags/*=0*/ )
{
	for( TFieldIterator<UProperty> It(this); It; ++It )
	{
		for( INT i=0; i<It->ArrayDim; i++ )
		{
			if( !It->Matches(A,B,i,FALSE,PortFlags) )
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

/*-----------------------------------------------------------------------------
	C++ property macros.
-----------------------------------------------------------------------------*/

#ifdef __GNUC__
/**
 * gcc3 thinks &((myclass*)NULL)->member is an invalid use of the offsetof
 * macro. This is a broken heuristic in the compiler and the workaround is
 * to use a non-zero offset.
 */
#define CPP_PROPERTY(name)	EC_CppProperty, (((BYTE*)(&((ThisClass*)0x1)->name)) - ((BYTE*)0x1))
#else
#define CPP_PROPERTY(name)	EC_CppProperty, ((BYTE*)&((ThisClass*)NULL)->name - (BYTE*)NULL)
#endif

