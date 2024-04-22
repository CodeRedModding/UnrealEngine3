/*=============================================================================
	UMakeCommandlet.cpp: UnrealEd script recompiler.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"

// @todo: Put UnrealEd into the project settings

#include "UnCompileHelper.h"
#include "UnNativeClassExporter.h"
#include "UnScrPrecom.h"
#include "SourceControl.h"

namespace ClassManifestDefs
{
	const INT ManifestVersion = 4;
}

// currently unused
IMPLEMENT_COMPARE_POINTER( UClass, UMakeCommandlet,
{
	if ( A->HasAnyClassFlags(CLASS_Interface) )
	{
		if ( B->HasAnyClassFlags(CLASS_Interface) )
		{
			return A->GetFName().Compare(B->GetFName());
		}

		return -1;
	}
	else if ( B->HasAnyClassFlags(CLASS_Interface) )
	{
		return 1;
	}
	else
	{
		return A->GetFName().Compare(B->GetFName());
	}
})

IMPLEMENT_COMPARE_CONSTREF( FString, UMakeCommandlet, { return appStricmp(*A,*B); } )

FCompilerMetadataManager*	GScriptHelper = NULL;

#define PROP_MARKER_BEGIN TEXT("//## BEGIN PROPS")
#define PROP_MARKER_END	TEXT("//## END PROPS")

template<UBOOL bMatchInterfaceClasses>
class TInterfaceComparator
{
public:
	UBOOL IsValidClass( const class FClassTree* Node ) const
	{
		UClass* Cls = Node->GetClass();
		return Cls->HasAnyClassFlags(CLASS_Interface) == bMatchInterfaceClasses;
	}
};

class FClassPackageComparator
{
	UPackage* Package;

public:

	FClassPackageComparator( UPackage* InPackage )
	: Package(InPackage)
	{
	}

	UBOOL IsValidClass( const class FClassTree* Node ) const
	{
		UClass* Cls = Node->GetClass();
		return Cls == UObject::StaticClass() || Cls->GetOuter() == Package;
	}
};

/**
 * Helper class used to cache UClass* -> TCHAR* name lookup for finding the named used for C++ declaration.
 */
struct FNameLookupCPP
{
	/**
	 * Destructor, cleaning up allocated memory.
	 */
	~FNameLookupCPP()
	{
		for( TMap<UStruct*,TCHAR*>::TIterator It(StructNameMap); It; ++It )
		{
			TCHAR* Name = It.Value();
			delete [] Name;
		}
	}

	/**
	 * Returns the name used for declaring the passed in struct in C++
	 *
	 * @param	Struct	UStruct to obtain C++ name for
	 * @return	Name used for C++ declaration
	 */
	const TCHAR* GetNameCPP( UStruct* Struct )
	{
		TCHAR* NameCPP = StructNameMap.FindRef( Struct );
		if( NameCPP )
		{
			return NameCPP;
		}
		else
		{
			FString	TempName = FString(Struct->GetPrefixCPP()) + Struct->GetName();
			INT		StringLength	= TempName.Len();
			NameCPP					= new TCHAR[StringLength + 1];
			appStrcpy( NameCPP, StringLength + 1, *TempName );
			NameCPP[StringLength]	= 0;
			StructNameMap.Set( Struct, NameCPP );
			return NameCPP;
		}
	}

private:
	/** Map of UStruct pointers to C++ names */
	TMap<UStruct*,TCHAR*> StructNameMap;
};

/** C++ name lookup helper */
static FNameLookupCPP* NameLookupCPP;

/**
	* Determines whether the glue version of the specified native function
	* should be exported
	*
	* @param	Function	the function to check
	* @return	TRUE if the glue version of the function should be exported.
	*/
UBOOL FNativeClassHeaderGenerator::ShouldExportFunction( UFunction* Function )
{
	UBOOL bExport = TRUE;

	// don't export any script stubs for native functions declared in interface classes
	if ( Function->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface) )
	{
		bExport = FALSE;
	}
	else
	{
		// always export if the function is static
		if ( (Function->FunctionFlags&FUNC_Static) == 0 )
		{
			// don't export the function if this is not the original declaration and there is
			// at least one parent version of the function that is declared native
			for ( UFunction* ParentFunction = Function->GetSuperFunction(); ParentFunction;
				ParentFunction = ParentFunction->GetSuperFunction() )
			{
				if ( (ParentFunction->FunctionFlags&FUNC_Native) != 0 )
				{
					bExport = FALSE;
					break;
				}
			}
		}
	}

	return bExport;
}

/**
 * Determines whether this class's parent has been changed.
 * 
 * @return	TRUE if the class declaration for the current class doesn't match the disk-version of the class declaration
 */
UBOOL FNativeClassHeaderGenerator::HasParentClassChanged()
{
	UObject* ExistingClassDefaultObject = CurrentClass->GetDefaultObject();

	// if we don't have a ClassDefaultObject here, it means that this is a new native class (if this was a previously
	// existing native class, it would have been statically registered, which would have created the class default object
	// In this case, we can use the HasParentClassChanged method, which is 100% reliable in determining whether the parent
	// class has indeed been changed
	if ( ExistingClassDefaultObject != NULL && ExistingClassDefaultObject->HasParentClassChanged() )
	{
		return TRUE;
	}

	// but we also need to check to see if any multiple inheritance parents changed

	// if this is the first time we're generating the header, no need to do anything
	if ( OriginalHeader.Len() == 0 )
		return FALSE;

	FString Temp = FString::Printf(TEXT("class %s : public "), NameLookupCPP->GetNameCPP( CurrentClass ));

	INT OriginalStart = OriginalHeader.InStr(*Temp);
	if ( OriginalStart == INDEX_NONE )
	{
		return FALSE;
	}

	OriginalStart += Temp.Len();
	FString OriginalParentClass = OriginalHeader.Mid(OriginalStart);

	INT OriginalEnd = OriginalParentClass.InStr(TEXT("\r\n"));
	if ( OriginalEnd == INDEX_NONE )
	{
		OriginalEnd = OriginalParentClass.InStr(TEXT("\n"));
	}
	check(OriginalEnd != INDEX_NONE);

	OriginalParentClass = OriginalParentClass.Left(OriginalEnd);

	// cut if off at any multiple inheritance
	// @todo: parse apart the whole thing and compare it to the list of parents in FClassMetaData
	OriginalEnd = OriginalParentClass.InStr(TEXT(","));
	if (OriginalEnd != INDEX_NONE)
	{
		OriginalParentClass = OriginalParentClass.Left(OriginalEnd);
	}

	const TCHAR* SuperClassCPPName	= NameLookupCPP->GetNameCPP( CurrentClass->GetSuperClass() );
	if ( OriginalParentClass != SuperClassCPPName )
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * Determines whether the property layout for this native class has changed
 * since the last time the header was generated
 *
 * @return	TRUE if the property block for the current class doesn't match the disk-version of the class declaration
 */
UBOOL FNativeClassHeaderGenerator::HavePropertiesChanged()
{
	// if this is the first time we're generating the header, no need to do anything
	if ( OriginalHeader.Len() == 0 )
		return FALSE;

	if ( HasParentClassChanged() )
		return TRUE;

	INT OriginalStart, OriginalEnd, CurrentStart, CurrentEnd;

	// first, determine the location of the beginning of the property block for the current class
	FString Temp = FString::Printf(TEXT("%s %s"), PROP_MARKER_BEGIN, *CurrentClass->GetName());
	OriginalStart = OriginalHeader.InStr(*Temp);
	if ( OriginalStart == INDEX_NONE )
	{
		// if the original header didn't contain this class, no need to do anything
		return FALSE;
	}

	CurrentStart = HeaderText.InStr(*Temp);
	check(CurrentStart != INDEX_NONE);

	// next, determine the location of the end of the property block for the current class
	Temp = FString::Printf(TEXT("%s %s"), PROP_MARKER_END, *CurrentClass->GetName());
	OriginalEnd = OriginalHeader.InStr(*Temp);
	check(OriginalEnd != INDEX_NONE);

	CurrentEnd = HeaderText.InStr(*Temp);
	check(CurrentEnd != INDEX_NONE);

	FString OriginalPropertyLayout = OriginalHeader.Mid(OriginalStart, OriginalEnd - OriginalStart);
	FString CurrentPropertyLayout = HeaderText.Mid(CurrentStart, CurrentEnd - CurrentStart);

	return OriginalPropertyLayout != CurrentPropertyLayout;
}

/**
 * Determines whether the specified class should still be considered misaligned,
 * and clears the RF_MisalignedObject flag if the existing member layout [on disk]
 * matches the current member layout.  Also propagates the result to any child classes
 * which are noexport classes (otherwise, they'd never be cleared since noexport classes
 * don't export header files).
 *
 * @param	ClassNode	the node for the class to check.  It is assumed that this class has already exported
 *						it new class declaration.
 */
void FNativeClassHeaderGenerator::ClearMisalignmentFlag( const FClassTree* ClassNode )
{
	UClass* Class = ClassNode->GetClass();
	UClass* SuperClass = Class->GetSuperClass();

	// we're guaranteed that our parent class has already been exported, so if
	// RF_MisalignedObject is still set, then it means that it did actually change,
	// so we're still misaligned at this point
	if ( !SuperClass->IsMisaligned() )
	{
		Class->ClearFlags(RF_MisalignedObject);

		TArray<const FClassTree*> ChildClasses;
		ClassNode->GetChildClasses(ChildClasses);

		for ( INT i = 0; i < ChildClasses.Num(); i++ )
		{
			const FClassTree* ChildNode = ChildClasses(i);
			
			UClass* ChildClass = ChildNode->GetClass();
			if ( ChildClass->HasAnyFlags(RF_MisalignedObject) && (!ChildClass->HasAnyClassFlags(CLASS_Native) || ChildClass->HasAnyClassFlags(CLASS_NoExport)) )
			{
				// propagate this change to any noexport or non-native child classes
				ClearMisalignmentFlag(ChildNode);
			}
		}
	}
}

/**
 * Exports the struct's C++ properties to the HeaderText output device and adds special
 * compiler directives for GCC to pack as we expect.
 *
 * @param	Struct				UStruct to export properties
 * @param	TextIndent			Current text indentation
 * @param	ImportsDefaults		whether this struct will be serialized with a default value
 */
void FNativeClassHeaderGenerator::ExportProperties( UStruct* Struct, INT TextIndent, UBOOL ImportsDefaults )
{
	UProperty*	Previous			= NULL;
	UProperty*	PreviousNonEditorOnly = NULL;
	UProperty*	LastInSuper			= NULL;
	UStruct*	InheritanceSuper	= Struct->GetInheritanceSuper();
	UBOOL		bEmittedHasEditorOnlyMacro = FALSE;
	UBOOL		bEmittedHasScriptAlign = FALSE;

	// Find last property in the lowest base class that has any properties
	UStruct* CurrentSuper = InheritanceSuper;
	while (LastInSuper == NULL && CurrentSuper)
	{
		for( TFieldIterator<UProperty> It(CurrentSuper,FALSE); It; ++It )
		{
			UProperty* Current = *It;

			// Disregard properties with 0 size like functions.
			if( It.GetStruct() == CurrentSuper && Current->ElementSize )
			{
				LastInSuper = Current;
			}
		}
		// go up a layer in the hierarchy
		CurrentSuper = CurrentSuper->GetSuperStruct();
	}

	EPropertyHeaderExportFlags CurrentExportType = PROPEXPORT_Public;

	// find structs that are nothing but bytes, account for editor only properties being 
	// removed on consoles
	INT NumProperties = 0;
	INT NumByteProperties = 0;
	INT NumNonEditorOnlyProperties = 0;
	INT NumNonEditorOnlyByteProperties = 0;
	for( TFieldIterator<UProperty> It(Struct,FALSE); It; ++It )
	{
		// treat bitfield and bytes the same
		UBOOL bIsByteProperty = It->IsA(UByteProperty::StaticClass());// || It->IsA(UBoolProperty::StaticClass());
		UBOOL bIsEditorOnlyProperty = It->IsEditorOnlyProperty();
		
		// count our propertie
		NumProperties++;
		if (bIsByteProperty)
		{
			NumByteProperties++;
		}
		if (!bIsEditorOnlyProperty)
		{
			NumNonEditorOnlyProperties++;
		}
		if (!bIsEditorOnlyProperty && bIsByteProperty)
		{
			NumNonEditorOnlyByteProperties++;
		}
	}

	// if all non-editor properties are bytes, or if all properties are bytes, then we need flag
	UBOOL bNeedsExtraAlignFlag = 
		(NumNonEditorOnlyProperties && NumNonEditorOnlyProperties == NumNonEditorOnlyByteProperties) ||
		NumProperties == NumByteProperties;


	// Iterate over all properties in this struct.
	for( TFieldIterator<UProperty> It(Struct,FALSE); It; ++It )
	{
		UProperty* Current = *It;

		FStringOutputDevice PropertyText;

		// Disregard properties with 0 size like functions.
		if( It.GetStruct()==Struct && Current->ElementSize )
		{
			// Skip noexport properties.
			if( !(Current->PropertyFlags&CPF_NoExport) )
			{
				if( (Current->PropertyFlags & CPF_Native) && Current->IsEditorOnlyProperty() )
				{
					warnf( NAME_DevCompile, TEXT("--Class %s references a native, editor-only property %s.  Please ensure the property handles serialisation correctly!"),
						*Struct->GetName(),
						*Current->GetName() );
				}

				// find the class info for this class
				FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

				// find the compiler token for this property
				FTokenData* PropData = ClassData->FindTokenData(Current);
				if ( PropData != NULL )
				{
					// if this property has a different access specifier, then export that now
					if ( (PropData->Token.PropertyExportFlags & CurrentExportType) == 0 )
					{
						FString AccessSpecifier;
						if ( (PropData->Token.PropertyExportFlags & PROPEXPORT_Private) != 0 )
						{
							CurrentExportType = PROPEXPORT_Private;
							AccessSpecifier = TEXT("private");
						}
						else if ( (PropData->Token.PropertyExportFlags & PROPEXPORT_Protected) != 0 )
						{
							CurrentExportType = PROPEXPORT_Protected;
							AccessSpecifier = TEXT("protected");
						}
						else
						{
							CurrentExportType = PROPEXPORT_Public;
							AccessSpecifier = TEXT("public");
						}

						if ( AccessSpecifier.Len() )
						{
							// If we are changing the access specifier we need to emit the #endif for the WITH_EDITORONLY_DATA macro first otherwise the access specifier may
							// only be conditionally compiled in.
							if( bEmittedHasEditorOnlyMacro )
							{
								PropertyText.Logf( TEXT("#endif // WITH_EDITORONLY_DATA\r\n") );
								bEmittedHasEditorOnlyMacro = FALSE;
							}

							PropertyText.Logf(TEXT("%s:%s"), *AccessSpecifier, LINE_TERMINATOR);
						}
					}
				}
				else if ( Current->IsA(UDelegateProperty::StaticClass()) )
				{
					// If we are changing the access specifier we need to emit the #endif for the WITH_EDITORONLY_DATA macro first otherwise the access specifier may
					// only be conditionally compiled in.
					if( bEmittedHasEditorOnlyMacro )
					{
						PropertyText.Logf( TEXT("#endif // WITH_EDITORONLY_DATA\r\n") );
						bEmittedHasEditorOnlyMacro = FALSE;
					}

					// this is a delegate property generated by the compiler due to a delegate function declaration
					// these are always marked public
					if ( CurrentExportType != PROPEXPORT_Public )
					{
						CurrentExportType = PROPEXPORT_Public;
						PropertyText.Log(TEXT("public:") LINE_TERMINATOR);
					}
				}

				// If we are switching from editor to non-editor or vice versa and the state of the WITH_EDITORONLY_DATA macro emission doesn't match, generate the 
				// #if or #endif appropriately.
				UBOOL RequiresHasEditorOnlyMacro = Current->IsEditorOnlyProperty();
				if( !bEmittedHasEditorOnlyMacro && RequiresHasEditorOnlyMacro )
				{
					// Indent code and export CPP text.
					PropertyText.Logf( TEXT("#if WITH_EDITORONLY_DATA\r\n") );
					bEmittedHasEditorOnlyMacro = TRUE;
				}
				else if( bEmittedHasEditorOnlyMacro && !RequiresHasEditorOnlyMacro )
				{
					PropertyText.Logf( TEXT("#endif // WITH_EDITORONLY_DATA\r\n") );
					bEmittedHasEditorOnlyMacro = FALSE;
				}

				// Indent code and export CPP text.
				PropertyText.Logf( appSpc(TextIndent+4) );

				// add extra alignment goo
				if (bNeedsExtraAlignFlag)
				{
					PropertyText.Logf(TEXT("MS_ALIGN(4) "));
				}

// the following is a work in progress for supporting any type to have a {} type qualifier
#if 0
				// look up a variable type override
				FString VariableTypeOverride;

				// find the class info for this class
				FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

				// FindTokenData can't be called on delegates?
				if (!Current->IsA(UDelegateProperty::StaticClass()))
				{
					FTokenData* PropData = ClassData->FindTokenData(Current);

					// if we have an override, set that as the type to export to the .h file
					if (PropData->Token.ExportInfo.Len() > 0)
					{
						VariableTypeOverride = PropData->Token.ExportInfo;

						// if it's the special "pointer" type, then append a * to the end of the declaration
						UStructProperty* StructProp = Cast<UStructProperty>(Current,CLASS_IsAUStructProperty);
						if (StructProp && StructProp->Struct->GetFName() == NAME_Pointer)
						{
							VariableTypeOverride += TEXT("*");
						}
					}
				}

				Current->ExportCppDeclaration( PropertyText, 1, 0, ImportsDefaults, VariableTypeOverride.Len() ? *VariableTypeOverride : NULL );
#else
				Current->ExportCppDeclaration( PropertyText, 1, 0, ImportsDefaults );
				ApplyAlternatePropertyExportText(*It, PropertyText);
#endif
				
				// add extra alignment goo
				if (bNeedsExtraAlignFlag)
				{
					PropertyText.Logf(TEXT(" GCC_ALIGN(4); // Extra alignment flags needed because all properties are bytes\r\n"));
					// only needed on first property
					bNeedsExtraAlignFlag = FALSE;
				}
				else
				{
					// Finish up line.
					PropertyText.Logf(TEXT(";\r\n"));
				}

				
				// Figure out whether we need to deal with alignment. We use unnamed 0 length bitfields (:0) to force alignment
				// after a series of bytes or bool properties. This ensures that anything following will be aligned to
				// sizeof(BITFIELD) (this is was GCC_BITFIELD_MAGIC used to be used for). Any struct that ends in a bool 
				// or byte will always end with the :0 bitfield so that any subclass or property following an inline struct property
				// will be aligned as well
				UBOOL bRequiresAlignment	= FALSE;
				UBOOL bCurrentIsBool		= Current->IsA(UBoolProperty::StaticClass());
				UBOOL bCurrentIsByte		= Current->IsA(UByteProperty::StaticClass());
				UBOOL bCurrentIsStruct		= Current->IsA(UStructProperty::StaticClass());
				UBOOL bPreviousIsByte		= Previous && Previous->IsA(UByteProperty::StaticClass());
				UBOOL bPreviousIsBool		= Previous && Previous->IsA(UBoolProperty::StaticClass());

				// switching from bool to byte needs alignment
				if (bPreviousIsBool && bCurrentIsByte)
				{
					bRequiresAlignment = TRUE;
				}
				// switching from byte to bool needs alignment
				else if (bPreviousIsByte && bCurrentIsBool)
				{
					bRequiresAlignment = TRUE;
				}
				// switching from byte or bool to a struct may need alignment, so assume needs alignment
				else if ((bPreviousIsBool || bPreviousIsByte) && bCurrentIsStruct) 
				{
					// @todo: this is only needed if the first exportable property of the struct is a byte or bool
					bRequiresAlignment = TRUE;
				}

				// if we need alignment, then we need to insert the :0 _before_ this property
				if (bRequiresAlignment)
				{
					FString BitfieldText = FString::Printf(TEXT("%sSCRIPT_ALIGN;%s"), appSpc(TextIndent + 4), LINE_TERMINATOR);
					BitfieldText += PropertyText;
					PropertyText = *BitfieldText;
					bEmittedHasScriptAlign = TRUE;
				}
				else
				{
					bEmittedHasScriptAlign = FALSE;
				}
			}
			// We want to make sure that noexport properties are handled by the user correctly otherwise alignments and strange crashes will occur.
			else if( Current->IsEditorOnlyProperty() )
			{
				warnf( NAME_DevCompile, TEXT("--Class %s references a noexport, editor-only property %s.  Please ensure the property has #if WITH_EDITORONLY_DATA around it."),
					*Struct->GetName(),
					*Current->GetName() );
			}

			LastInSuper	= NULL;
			Previous	= Current;
			if (!Current->IsEditorOnlyProperty())
			{
				PreviousNonEditorOnly = Current;
			}

			HeaderText.Log(PropertyText);
		}
	}

	// End of property list.  If we haven't generated the WITH_EDITORONLY_DATA #endif, do so now.
	if( bEmittedHasEditorOnlyMacro )
	{
		if (!bEmittedHasScriptAlign && PreviousNonEditorOnly && 
			(PreviousNonEditorOnly->IsA(UBoolProperty::StaticClass()) || PreviousNonEditorOnly->IsA(UByteProperty::StaticClass())))
		{
			HeaderText.Logf( TEXT("#else\r\n") );
			HeaderText.Logf( TEXT("%sSCRIPT_ALIGN;\r\n"), appSpc(TextIndent + 4) );
		}
		HeaderText.Logf( TEXT("#endif // WITH_EDITORONLY_DATA\r\n") );
	}

	// append a nameless :0 bitfield to the end of a struct if the last property was a bitfield
	if (Previous && (Previous->IsA(UBoolProperty::StaticClass()) || Previous->IsA(UByteProperty::StaticClass())))
	{
		HeaderText.Logf(TEXT("%sSCRIPT_ALIGN;%s"), appSpc(TextIndent + 4), LINE_TERMINATOR);
	}

	// if the last property that was exported wasn't public, emit a line to reset the access to "public" so that we don't interfere with cpptext
	if ( CurrentExportType != PROPEXPORT_Public )
	{
		HeaderText.Logf(TEXT("public:%s"), LINE_TERMINATOR);
	}
}

/**
 * Exports the C++ class declarations for a native interface class.
 */
void FNativeClassHeaderGenerator::ExportInterfaceClassDeclaration( UClass* Class )
{
	FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

	const FClassTree* ClassNode = ClassTree.FindNode(Class);
	ClearMisalignmentFlag(ClassNode);

	TArray<UEnum*>			Enums;
	TArray<UScriptStruct*>	Structs;
	TArray<UConst*>			Consts;
	TArray<UFunction*>		CallbackFunctions;
	TArray<UFunction*>		NativeFunctions;

	// get the lists of fields that belong to this class that should be exported
	RetrieveRelevantFields(Class, Enums, Structs, Consts, CallbackFunctions, NativeFunctions);

	// export enum declarations
	ExportEnums(Enums);

	// export #defines for all consts
	ExportConsts(Consts);

	// export struct declarations
	ExportStructs(Structs);

	UClass* SuperClass = Class->GetSuperClass();


	// the name for the C++ version of the UClass
	const TCHAR* ClassCPPName		= NameLookupCPP->GetNameCPP( Class );
	const TCHAR* SuperClassCPPName	= NameLookupCPP->GetNameCPP( SuperClass );

	// Export the UClass declaration
	// Class definition.
	HeaderText.Logf( TEXT("class %s"), ClassCPPName );
	if( SuperClass )
	{
		HeaderText.Logf( TEXT(" : public %s"), SuperClassCPPName );

		// look for multiple inheritance info
		const TArray<FMultipleInheritanceBaseClass>& InheritanceParents = ClassData->GetInheritanceParents();
		for (INT ParentIndex = 0; ParentIndex < InheritanceParents.Num(); ParentIndex++)
		{
			HeaderText.Logf(TEXT(", public %s"), *InheritanceParents(ParentIndex).ClassName);
		}
	}
	HeaderText.Logf( TEXT("\r\n{\r\npublic:\r\n") );

	// Build the DECLARE_CLASS line
	HeaderText.Logf( TEXT("%sDECLARE_ABSTRACT_CLASS(%s,"), appSpc(4), ClassCPPName );
	HeaderText.Logf( TEXT("%s,0"), SuperClassCPPName );

	// append class flags to the definition
	HeaderText.Logf(TEXT("%s,%s)%s"), *GetClassFlagExportText(Class), *Class->GetOuter()->GetName(), LINE_TERMINATOR);
	if(Class->ClassWithin != Class->GetSuperClass()->ClassWithin)
		HeaderText.Logf(TEXT("    DECLARE_WITHIN(%s)\r\n"), NameLookupCPP->GetNameCPP( Class->ClassWithin ) );

	// End of class.
	HeaderText.Logf( TEXT("    NO_DEFAULT_CONSTRUCTOR(%s)\r\n};\r\n\r\n"), ClassCPPName );



	// =============================================
	// Export the pure interface version of the class

	// the name of the pure interface class
	FString InterfaceCPPName		= FString::Printf(TEXT("I%s"), *Class->GetName());
	FString SuperInterfaceCPPName;
	if ( SuperClass != NULL )
	{
		SuperInterfaceCPPName = FString::Printf(TEXT("I%s"), *SuperClass->GetName());
	}

	// Class definition.
	HeaderText.Logf( TEXT("class %s"), *InterfaceCPPName );

	if( SuperClass )
	{
		// don't derive from IInterface, or we'll be unable to implement more than one interface
		// since the size of the interface's vtable [in the UObject class that implements the inteface]
		// will be different when more than one interface is implemented (caused by multiple inheritance of the same base class)
		if ( SuperClass != UObject::StaticClass() && SuperClass != UInterface::StaticClass()  )
		{
			HeaderText.Logf( TEXT(" : public %s"), *SuperInterfaceCPPName );
		}

		// look for multiple inheritance info
		const TArray<FMultipleInheritanceBaseClass>& InheritanceParents = ClassData->GetInheritanceParents();
		for (INT ParentIndex = 0; ParentIndex < InheritanceParents.Num(); ParentIndex++)
		{
			HeaderText.Logf(TEXT(", public %s"), *InheritanceParents(ParentIndex).ClassName);
		}
	}
	HeaderText.Logf( TEXT("\r\n{\r\n") );
	HeaderText.Logf( TEXT("protected:\r\n\tvirtual ~%s() {}\r\npublic:\r\n"), *InterfaceCPPName );
	HeaderText.Logf( TEXT("\ttypedef %s UClassType;\r\n"), ClassCPPName );

	// we'll need a way to get to the UObject portion of a native interface, so that we can safely pass native interfaces
	// to script VM functions
	if (SuperClass->IsChildOf(UInterface::StaticClass()))
	{
		HeaderText.Logf(TEXT("\tvirtual UObject* GetUObjectInterface%s()=0;\r\n"), *Class->GetName());
	}

	// C++ -> UnrealScript stubs (native function execs)
	ExportNativeFunctions(NativeFunctions);

	// UnrealScript -> C++ proxies (events and delegates).
	ExportCallbackFunctions(CallbackFunctions);

	FString Filename = GEditor->EditPackagesInPath * *Class->GetOuter()->GetName() * TEXT("Inc") * ClassCPPName + TEXT(".h");
	if( Class->CppText && Class->CppText->Text.Len() )
	{
		HeaderText.Log( *Class->CppText->Text );
	}
	else if( GFileManager->FileSize(*Filename) > 0 )
	{
		HeaderText.Logf( TEXT("    #include \"%s.h\"\r\n"), ClassCPPName );
	}
	else
	{
		HeaderText.Logf( TEXT("    NO_DEFAULT_CONSTRUCTOR(%s)\r\n"), *InterfaceCPPName );
	}

	// End of class.
	HeaderText.Logf( TEXT("};\r\n") );

	// End.
	HeaderText.Logf( TEXT("\r\n") );
}

/**
 * Helper function to find an element in a TArray of pointers by GetFName().
 * @param Array		TArray of pointers.
 * @param Name		Name to search for.
 * @return			Index of the found element, or INDEX_NONE if not found.
 */
template <typename T>
INT FindByIndirectName( const TArray<T>& Array, const FName& Name )
{
	for ( INT Index=0; Index < Array.Num(); ++Index )
	{
		if ( Array(Index)->GetFName() == Name )
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

/**
 * Appends the header definition for an inheritance hierarchy of classes to the header.
 * Wrapper for ExportClassHeaderRecursive().
 *
 * @param	Class				The class to be exported.
 */
void FNativeClassHeaderGenerator::ExportClassHeader( UClass* Class )
{
	TArray<UClass*> DependencyChain;
	VisitedMap.Empty( Classes.Num() );
	ExportClassHeaderRecursive( Class, DependencyChain, FALSE );
}

/**
 * Appends the header definition for an inheritance hierarchy of classes to the header.
 *
 * @param	Class					The class to be exported.
 * @param	DependencyChain			Used for finding errors. Must be empty before the first call.
 * @param	bCheckDependenciesOnly	Whether we should just keep checking for dependency errors, without exporting anything.
 */
void FNativeClassHeaderGenerator::ExportClassHeaderRecursive( UClass* Class, TArray<UClass*>& DependencyChain, UBOOL bCheckDependenciesOnly )
{
	UBOOL bIsExportClass = Class->ScriptText && Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_NoExport);
	UBOOL bIsCorrectHeader = Class->ClassHeaderFilename == ClassHeaderFilename && Class->GetOuter() == Package;

	// Check for circular header dependencies between export classes.
	if ( bIsExportClass )
	{
		if ( bIsCorrectHeader == FALSE )
		{
			if ( DependencyChain.Num() == 0 )
			{
				// The first export class we found doesn't belong in this header: No need to keep exporting along this dependency path.
				return;
			}
			else
			{
				// From now on, we're not going to export anything. Instead, we're going to check that no deeper dependency tries to export to this header file.
				bCheckDependenciesOnly = TRUE;
			}
		}
		else if ( bCheckDependenciesOnly )
		{
			FString DependencyChainString;
			for ( INT DependencyIndex=0; DependencyIndex < DependencyChain.Num(); DependencyIndex++ )
			{
				UClass* DependencyClass = DependencyChain(DependencyIndex);
				DependencyChainString += DependencyClass->GetName() + TEXT(" -> ");
			}
			DependencyChainString += Class->GetName();
			warnf( NAME_Warning, TEXT("Circular header dependency detected (%s), while exporting %s!"), *DependencyChainString, *(Package->GetName() + ClassHeaderFilename + TEXT("Classes.h")) );
		}
	}

	// Check if the Class has already been exported, after we've checked for circular header dependencies.
	if ( Class->HasAnyClassFlags(CLASS_Exported) )
	{
		return;
	}

	// Check for circular dependencies.
	UBOOL* bVisited = VisitedMap.Find( Class );
	if ( bVisited && *bVisited == TRUE )
	{
		warnf( NAME_Error, TEXT("Circular dependency detected for class %s!"), *Class->GetName() );
		return;
	}
	// Temporarily mark the Class as VISITED. Make sure to clear this flag before returning!
	VisitedMap.Set( Class, TRUE );

	if ( bIsExportClass )
	{
		DependencyChain.AddItem( Class );
	}

	// Export the super class first.
	UClass* SuperClass = Class->GetSuperClass();
	if ( SuperClass )
	{
		ExportClassHeaderRecursive(SuperClass, DependencyChain, bCheckDependenciesOnly);
	}

	// Export all classes we depend on.
	const TArray<FName>& DependentOnClassNames = Class->DependentOn;
	for( INT DependsOnIndex=0; DependsOnIndex < DependentOnClassNames.Num(); DependsOnIndex++ )
	{
		const FName& DependencyClassName = DependentOnClassNames(DependsOnIndex);
		INT ClassIndex = FindByIndirectName( Classes, DependencyClassName );
		UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE,*DependencyClassName.ToString());
		// Only export the class if it's in Classes array (it may be in another package).
		UClass* DependsOnClass = (ClassIndex != INDEX_NONE) ? FoundClass : NULL;
		if ( DependsOnClass )
		{
			ExportClassHeaderRecursive(DependsOnClass, DependencyChain, bCheckDependenciesOnly);
		}
		else if ( !FoundClass )
		{
			warnf( NAME_Error, TEXT("Unknown class %s used in dependson declaration in class %s!"), *DependencyClassName.ToString(), *Class->GetName() );
		}
	}

	// Export class header.
	if ( bIsExportClass && bIsCorrectHeader && !bCheckDependenciesOnly )
	{
		CurrentClass = Class;

		// Mark class as exported.
		Class->ClassFlags |= CLASS_Exported;

		if ( !Class->HasAnyClassFlags(CLASS_Interface) )
		{
			TArray<UEnum*>			Enums;
			TArray<UScriptStruct*>	Structs;
			TArray<UConst*>			Consts;
			TArray<UFunction*>		CallbackFunctions;
			TArray<UFunction*>		NativeFunctions;

			// get the lists of fields that belong to this class that should be exported
			RetrieveRelevantFields(Class, Enums, Structs, Consts, CallbackFunctions, NativeFunctions);

			// export enum declarations
			ExportEnums(Enums);

			// export #defines for all consts
			ExportConsts(Consts);

			// export struct declarations
			ExportStructs(Structs);

			// export parameters structs for all events and delegates
			ExportEventParms(CallbackFunctions);

			UClass* SuperClass = Class->GetSuperClass();

			const TCHAR* ClassCPPName		= NameLookupCPP->GetNameCPP( Class );
			const TCHAR* SuperClassCPPName	= NameLookupCPP->GetNameCPP( SuperClass );

			// Class definition.
			HeaderText.Logf( TEXT("class %s"), ClassCPPName );
			if( SuperClass )
			{
				HeaderText.Logf( TEXT(" : public %s"), SuperClassCPPName );

				// look for multiple inheritance info
				FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);
				const TArray<FMultipleInheritanceBaseClass>& InheritanceParents = ClassData->GetInheritanceParents();
		
				for (INT ParentIndex = 0; ParentIndex < InheritanceParents.Num(); ParentIndex++)
				{
					HeaderText.Logf(TEXT(", public %s"), *InheritanceParents(ParentIndex).ClassName);
				}
			}
			HeaderText.Log( TEXT("\r\n{\r\npublic:\r\n") );

			// export the class property marker
			HeaderText.Logf(TEXT("%s%s %s\r\n"), appSpc(4), PROP_MARKER_BEGIN, *CurrentClass->GetName());
			// Export the class' CPP properties.
			ExportProperties( Class, 0, 1 );
			HeaderText.Logf(TEXT("%s%s %s\r\n\r\n"), appSpc(4), PROP_MARKER_END, *CurrentClass->GetName());

			// if the properties for this native class haven't been changed since the last compile,
			// clear the misaligned flag
			if ( !HavePropertiesChanged() )
			{
				const FClassTree* ClassNode = ClassTree.FindNode(Class);
				ClearMisalignmentFlag(ClassNode);
			}

			// C++ -> UnrealScript stubs (native function execs)
			ExportNativeFunctions(NativeFunctions);

			// UnrealScript -> C++ proxies (events and delegates).
			ExportCallbackFunctions(CallbackFunctions);

			// Build the DECLARE_CLASS line
			{
				FString ClassDeclarationModifier;
				if ( Class->HasAnyClassFlags(CLASS_Abstract) )
				{
					ClassDeclarationModifier = TEXT("ABSTRACT_");
				}

				// this can only happen if a class was marked with a cast flag manually first, since there is no script keyword for this
				const UBOOL bCastedClass = Class->HasAnyCastFlag(CASTCLASS_AllFlags) && Class->GetSuperClass() && Class->ClassCastFlags != Class->GetSuperClass()->ClassCastFlags;
				if ( bCastedClass )
				{
					ClassDeclarationModifier += TEXT("CASTED_");
				}

				HeaderText.Logf( TEXT("%sDECLARE_%sCLASS(%s,"), appSpc(4), *ClassDeclarationModifier, ClassCPPName );
				HeaderText.Logf( TEXT("%s,0"), SuperClassCPPName );

				// append class flags to the definition
				HeaderText.Logf(TEXT("%s,%s"), *GetClassFlagExportText(Class), *Class->GetOuter()->GetName());
				if ( bCastedClass )
				{
					// append platform flags if any are set
					HeaderText.Logf(TEXT(",CASTCLASS_%s"), ClassCPPName);
				}
				HeaderText.Log(TEXT(")\r\n"));
				if(Class->ClassWithin != Class->GetSuperClass()->ClassWithin)
				{
					HeaderText.Logf(TEXT("    DECLARE_WITHIN(%s)\r\n"), NameLookupCPP->GetNameCPP( Class->ClassWithin ) );
				}
			}

			// export the class's config name
			if ( Class->ClassConfigName != NAME_None && Class->ClassConfigName != Class->GetSuperClass()->ClassConfigName )
			{
				HeaderText.Logf(TEXT("    static const TCHAR* StaticConfigName() {return TEXT(\"%s\");}\r\n\r\n"), *Class->ClassConfigName.ToString());
			}

			// arrays for preventing multiple export of accessor function in situations where the native class
			// implements multiple children of the same interface base class
			TArray<UClass*> UObjectExportedInterfaces;
			TArray<UClass*> ExportedInterfaces;

			for (TArray<FImplementedInterface>::TIterator It(Class->Interfaces); It; ++It)
			{
				UClass* InterfaceClass = It->Class;
				if ( InterfaceClass->HasAnyClassFlags(CLASS_Native) )
				{
					for ( UClass* IClass = InterfaceClass; IClass && IClass->HasAnyClassFlags(CLASS_Interface); IClass = IClass->GetSuperClass() )
					{
						if ( IClass != UInterface::StaticClass() && !UObjectExportedInterfaces.ContainsItem(IClass) )
						{
							UObjectExportedInterfaces.AddItem(IClass);
							HeaderText.Logf(TEXT("%svirtual UObject* GetUObjectInterface%s(){return this;}\r\n"), appSpc(4), *IClass->GetName());
						}
					}
				}
			}

			FString Filename = GEditor->EditPackagesInPath * *Class->GetOuter()->GetName() * TEXT("Inc") * ClassCPPName + TEXT(".h");
			if( Class->CppText && Class->CppText->Text.Len() )
			{
				HeaderText.Log( *Class->CppText->Text );
			}
			else if( GFileManager->FileSize(*Filename) > 0 )
			{
				HeaderText.Logf( TEXT("    #include \"%s.h\"\r\n"), ClassCPPName );
			}
			else
			{
				HeaderText.Logf( TEXT("    NO_DEFAULT_CONSTRUCTOR(%s)\r\n"), ClassCPPName );
			}

			// End of class.
			HeaderText.Log( TEXT("};\r\n") );

			// End.
			HeaderText.Log( TEXT("\r\n") );
		}
		// Handle NoExport classes with editoronly properties If a class is noexport then check all of its properties to let the user know those properties
		// need the WITH_EDITORONLY_DATA macro around them.
		else if( ( Class->HasAllClassFlags(CLASS_Native|CLASS_NoExport) 
				|| Class->HasAllClassFlags(CLASS_Native|CLASS_Intrinsic) )
				&& Class->GetOuter() == Package && Class->ClassHeaderFilename == ClassHeaderFilename)
		{
			for( UField *Child = Class->Children; Child; Child = Child->Next )
			{
				UProperty * Prop = Cast<UProperty>(Child);
				if( Prop && Prop->IsEditorOnlyProperty() )
				{
					TCHAR *ClassType = Class->HasAllClassFlags(CLASS_Native|CLASS_Intrinsic) ? TEXT("Intrinsic") : TEXT("NoExport");
					warnf( NAME_Log, TEXT("--%s class %s references an editor-only property %s.  Please ensure the property has #if WITH_EDITORONLY_DATA around it."),
						ClassType,
						*Class->GetName(),
						*Prop->GetName() );
				}
			}
		}
		else
		{
			// this is an interface class
			ExportInterfaceClassDeclaration(Class);
		}
	}

	// We're done visiting this Class.
	VisitedMap.Set( Class, FALSE );
	if ( bIsExportClass )
	{
		DependencyChain.RemoveSwap( DependencyChain.Num() - 1 );
	}
}

/**
 * Returns a string in the format CLASS_Something|CLASS_Something which represents all class flags that are set for the specified
 * class which need to be exported as part of the DECLARE_CLASS macro
 */
FString FNativeClassHeaderGenerator::GetClassFlagExportText( UClass* Class )
{
	FString StaticClassFlagText;

	check(Class);
	if ( Class->HasAnyClassFlags(CLASS_Transient) )
	{
		StaticClassFlagText += TEXT("|CLASS_Transient");
	}				
	if( Class->HasAnyClassFlags(CLASS_Config) )
	{
		StaticClassFlagText += TEXT("|CLASS_Config");
	}
	if( Class->HasAnyClassFlags(CLASS_NativeReplication) )
	{
		StaticClassFlagText += TEXT("|CLASS_NativeReplication");
	}
	if ( Class->HasAnyClassFlags(CLASS_Interface) )
	{
		StaticClassFlagText += TEXT("|CLASS_Interface");
	}
	if ( Class->HasAnyClassFlags(CLASS_Deprecated) )
	{
		StaticClassFlagText += TEXT("|CLASS_Deprecated");
	}

	return StaticClassFlagText;
}

/**
* Iterates through all fields of the specified class, and separates fields that should be exported with this class into the appropriate array.
*
* @param	Class				the class to pull fields from
* @param	Enums				[out] all enums declared in the specified class
* @param	Structs				[out] list of structs declared in the specified class
* @param	Consts				[out] list of pure consts declared in the specified class
* @param	CallbackFunctions	[out] list of delegates and events declared in the specified class
* @param	NativeFunctions		[out] list of native functions declared in the specified class
*/
void FNativeClassHeaderGenerator::RetrieveRelevantFields(UClass* Class, TArray<UEnum*>& Enums, TArray<UScriptStruct*>& Structs, TArray<UConst*>& Consts, TArray<UFunction*>& CallbackFunctions, TArray<UFunction*>& NativeFunctions)
{
	for ( TFieldIterator<UField> It(Class,FALSE); It; ++It )
	{
		UField* CurrentField = *It;
		UClass* FieldClass = CurrentField->GetClass();
		if ( FieldClass == UEnum::StaticClass() )
		{
			UEnum* Enum = (UEnum*)CurrentField;
			Enums.AddItem(Enum);
		}

		else if ( FieldClass == UScriptStruct::StaticClass() )
		{
			UScriptStruct* Struct = (UScriptStruct*)CurrentField;
			if ( Struct->HasAnyFlags(RF_Native) || ((Struct->StructFlags&STRUCT_Native) != 0) )
				Structs.AddItem(Struct);
		}

		else if ( FieldClass == UConst::StaticClass() )
		{
			UConst* Const = (UConst*)CurrentField;
			Consts.AddItem(Const);
		}

		else if ( FieldClass == UFunction::StaticClass() )
		{
			UFunction* Function = (UFunction*)CurrentField;
			if ( (Function->FunctionFlags&(FUNC_Event|FUNC_Delegate)) != 0 &&
				Function->GetSuperFunction() == NULL )
			{
				CallbackFunctions.AddItem(Function);
			}

			if ( (Function->FunctionFlags&FUNC_Native) != 0 )
			{
				NativeFunctions.AddItem(Function);
			}
		}
	}
}

/**
* Exports the header text for the list of enums specified
*
* @param	Enums	the enums to export
*/
void FNativeClassHeaderGenerator::ExportEnums( const TArray<UEnum*>& Enums )
{
	// Enum definitions.
	for( INT EnumIdx = 0; EnumIdx < Enums.Num(); EnumIdx++ )
	{
		UEnum* Enum = Enums(EnumIdx);

		// Export enum.
		EnumHeaderText.Logf( TEXT("enum %s\r\n{\r\n"), *Enum->GetName() );
		for( INT i=0; i<Enum->NumEnums(); i++ )
		{
			EnumHeaderText.Logf( TEXT("    %-24s=%i,\r\n"), *Enum->GetEnum(i).ToString(), i );
		}
		EnumHeaderText.Logf( TEXT("};\r\n") );

		// Export FOREACH macro
		EnumHeaderText.Logf( TEXT("#define FOREACH_ENUM_%s(op) "), *Enum->GetName().ToUpper() );
		for( INT i=0; i<Enum->NumEnums()-1; i++ )
		{
			EnumHeaderText.Logf( TEXT("\\\r\n    op(%s) "), *Enum->GetEnum(i).ToString() );
		}
		EnumHeaderText.Logf( TEXT("\r\n") );
	}
}

/**
 * Exports the header text for the list of structs specified
 *
 * @param	Structs	the structs to export
 * @param	TextIndent	the current indentation of the header exporter
 */
void FNativeClassHeaderGenerator::ExportStructs( const TArray<UScriptStruct*>& NativeStructs, INT TextIndent/*=0*/ )
{
	// Struct definitions.

	// reverse the order.
	for( INT i=NativeStructs.Num()-1; i>=0; --i )
	{
		UScriptStruct* Struct = NativeStructs(i);

		// Export struct.
		HeaderText.Logf( TEXT("%sstruct %s"), appSpc(TextIndent), NameLookupCPP->GetNameCPP( Struct ) );
		if (Struct->SuperStruct != NULL)
		{
			HeaderText.Logf(TEXT(" : public %s"), NameLookupCPP->GetNameCPP(Struct->GetSuperStruct()));
		}
		HeaderText.Logf( TEXT("\r\n%s{\r\n"), appSpc(TextIndent) );

		// export internal structs
		TArray<UScriptStruct*> InternalStructs;
		for ( TFieldIterator<UField> It(Struct,FALSE); It; ++It )
		{
			UField* CurrentField = *It;
			UClass* FieldClass = CurrentField->GetClass();
			if ( FieldClass == UScriptStruct::StaticClass() )
			{
				UScriptStruct* InnerStruct = (UScriptStruct*)CurrentField;
				if ( InnerStruct->HasAnyFlags(RF_Native) || ((InnerStruct->StructFlags&STRUCT_Native) != 0) )
				{
					InternalStructs.AddItem(InnerStruct);
				}
			}
		}
		ExportStructs(InternalStructs, 4);

		// Export the struct's CPP properties.
		ExportProperties( Struct, TextIndent, TRUE );

		// Export serializer
		if( Struct->StructFlags&STRUCT_Export )
		{
			HeaderText.Logf( TEXT("%sfriend FArchive& operator<<(FArchive& Ar,%s& My%s)\r\n"), appSpc(TextIndent + 4), NameLookupCPP->GetNameCPP( Struct ), *Struct->GetName() );
			HeaderText.Logf( TEXT("%s{\r\n"), appSpc(TextIndent + 4) );
			HeaderText.Logf( TEXT("%sreturn Ar"), appSpc(TextIndent + 8) );

			// if this struct extends another struct, serialize its properties first
			UStruct* SuperStruct = Struct->GetSuperStruct();
			if ( SuperStruct )
			{
				HeaderText.Logf(TEXT(" << (%s&)My%s"), NameLookupCPP->GetNameCPP(SuperStruct), *Struct->GetName());
			}

			for ( TFieldIterator<UProperty> StructProp(Struct,FALSE); StructProp != NULL; ++StructProp )
			{
				FString PrefixText;
				if ( StructProp->IsA(UObjectProperty::StaticClass()) )
				{
					PrefixText = TEXT("(UObject*&)");
				}
				if( StructProp->ElementSize > 0 )
				{
					if( StructProp->ArrayDim > 1 )
					{
						for( INT i = 0; i < StructProp->ArrayDim; i++ )
						{
							HeaderText.Logf( TEXT(" << %sMy%s.%s[%d]"), *PrefixText, *Struct->GetName(), *StructProp->GetName(), i );
						}
					}
					else
					{
						HeaderText.Logf( TEXT(" << %sMy%s.%s"), *PrefixText, *Struct->GetName(), *StructProp->GetName() );
					}
				}
			}
			HeaderText.Logf( TEXT(";\r\n%s}\r\n"), appSpc(TextIndent + 4) );
		}

		// if the struct included cpptext, emit that now
		if ( Struct->CppText )
		{
			HeaderText.Logf(TEXT("%s%s\r\n"), appSpc(TextIndent), *Struct->CppText->Text);
		}
		else
		{
			FStringOutputDevice CtorAr, InitializationAr;

			if ( (Struct->StructFlags&STRUCT_Transient) != 0 )
			{
				INT PropIndex = 0;				

				// if the struct is transient, export initializers for the properties in the struct
				for ( TFieldIterator<UProperty> StructProp(Struct,FALSE); StructProp; ++StructProp )
				{
					UProperty* Prop = *StructProp;
					if ( (Prop->PropertyFlags&CPF_NeedCtorLink) == 0 )
					{
						if ( Prop->GetClass() != UStructProperty::StaticClass() ) // special case: constructors are called for any members which are structs when the outer structs constructor is called
						{
							InitializationAr.Logf(TEXT("%s%s %s(%s)\r\n"), appSpc(TextIndent + 4), PropIndex++ == 0 ? TEXT(":") : TEXT(","), *Prop->GetName(), *GetNullParameterValue(Prop,FALSE,TRUE));
						}
					}
					else if ( Prop->GetClass() == UStructProperty::StaticClass() )
					{
						InitializationAr.Logf(TEXT("%s%s %s(EC_EventParm)\r\n"), appSpc(TextIndent + 4), PropIndex++ == 0 ? TEXT(":") : TEXT(","), *Prop->GetName());
					}
				}
			}

			if ( InitializationAr.Len() > 0 )
			{
				CtorAr.Logf(TEXT("%s%s()\r\n"), appSpc(TextIndent + 4), NameLookupCPP->GetNameCPP(Struct));
				CtorAr.Log(*InitializationAr);
				CtorAr.Logf(TEXT("%s{}\r\n"), appSpc(TextIndent + 4));
			}
			else
			{
				CtorAr.Logf(TEXT("%s%s() {}\r\n"), appSpc(TextIndent + 4), NameLookupCPP->GetNameCPP(Struct));
			}

			// generate the event parm constructor
			CtorAr.Logf(TEXT("%s%s(EEventParm)\r\n"), appSpc(TextIndent + 4), NameLookupCPP->GetNameCPP( Struct ));
			CtorAr.Logf(TEXT("%s{\r\n"), appSpc(TextIndent + 4));
			CtorAr.Logf(TEXT("%sappMemzero(this, sizeof(%s));\r\n"), appSpc(TextIndent + 8), NameLookupCPP->GetNameCPP( Struct ));
			CtorAr.Logf(TEXT("%s}\r\n"), appSpc(TextIndent + 4));

			if ( CtorAr.Len() > 0 )
			{
				HeaderText.Logf(TEXT("\r\n%s/** Constructors */\r\n"), appSpc(TextIndent + 4));
				HeaderText.Logf(*CtorAr);
			}
		}

		HeaderText.Logf( TEXT("%s};\r\n\r\n"), appSpc(TextIndent) );
	}
}


/**
 * Exports the header text for the list of consts specified
 *
 * @param	Consts	the consts to export
 */
void FNativeClassHeaderGenerator::ExportConsts( const TArray<UConst*>& Consts )
{
	// Constants.
	for( INT i = 0; i < Consts.Num(); i++ )
	{
		UConst* Const = Consts(i);
		FString V = Const->Value;

		// remove all leading whitespace from the value of the const
		while( V.Len() > 0 && appIsWhitespace(**V) )
			V=V.Mid(1);

		// remove all trailing whitespace from the value of the const
		while ( V.Len() > 0 && appIsWhitespace(V.Right(1)[0]) )
			V = V.LeftChop(1);

		// remove literal name delimiters, if they exist
		if( V.Len()>1 && V.Left(1)==TEXT("'") && V.Right(1)==TEXT("'") )
			V = V.Mid(1,V.Len()-2);

		// if this is a string, wrap it with the TEXT macro
		if ( V.Len()>1 && V.Left(1)==TEXT("\"") && V.Right(1)==TEXT("\"") )
		{
			V = FString::Printf(TEXT("TEXT(%s)"), *V);
		}
		HeaderText.Logf( TEXT("#define UCONST_%s %s\r\n"), *Const->GetName(), *V );
	}
	if( Consts.Num() > 0 )
	{
		HeaderText.Logf( TEXT("\r\n") );
	}
}

/**
* Exports the parameter struct declarations for the list of functions specified
*
* @param	CallbackFunctions	the functions that have parameters which need to be exported
*/
void FNativeClassHeaderGenerator::ExportEventParms( const TArray<UFunction*>& CallbackFunctions )
{
	// Parms struct definitions.
	for ( INT i = 0; i < CallbackFunctions.Num(); i++ )
	{
		UFunction* Function = CallbackFunctions(i);
		FString EventParmStructName = FString::Printf(TEXT("%s_event%s_Parms"), *Function->GetOwnerClass()->GetName(), *Function->GetName() );
		HeaderText.Logf( TEXT("struct %s\r\n"), *EventParmStructName);
		HeaderText.Log( TEXT("{\r\n") );

		// keep track of any structs which contain properties that require construction that
		// are used as a parameter for this event call
		TArray<UStructProperty*> StructEventParms;
		for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags&CPF_Parm); ++It )
		{
			UProperty* Prop = *It;

			FStringOutputDevice PropertyText;
			PropertyText.Log( appSpc(4) );
			if (Prop->HasAnyPropertyFlags(CPF_Const)
			&&	Cast<UObjectProperty>(Prop,CLASS_IsAUObjectProperty) != NULL )
			{
				PropertyText.Log(TEXT("const "));
			}
			Prop->ExportCppDeclaration( PropertyText, 0, 0, 0 );
			ApplyAlternatePropertyExportText(Prop, PropertyText);
			
			PropertyText.Log( TEXT(";\r\n") );
			HeaderText += *PropertyText;

			UStructProperty* StructProp = Cast<UStructProperty>(Prop,CLASS_IsAUStructProperty);
			if ( StructProp != NULL )
			{
				// if this struct contains any properties which are exported as NoInit types, the event parm struct needs to call the
				// EEventParm ctor on the struct to initialize those properties to zero
				for ( UProperty* ConstructorProp = StructProp->Struct->ConstructorLink; ConstructorProp; ConstructorProp = ConstructorProp->ConstructorLinkNext )
				{
					if ( (ConstructorProp->PropertyFlags&CPF_AlwaysInit) == 0 )
					{
						StructEventParms.AddItem(StructProp);
						break;
					}
				}
			}	
		}

		// Export event parameter constructor, which will call the EEventParm constructor for any struct parameters
		// which contain properties that are NoInit
		HeaderText.Logf(TEXT("%s%s(EEventParm)\r\n"), appSpc(4), *EventParmStructName);
		for( INT i = 0; i < StructEventParms.Num(); i++ )
		{
			HeaderText.Logf(TEXT("%s%s %s(EC_EventParm)\r\n"), appSpc(4), i == 0 ? TEXT(":") : TEXT(","), *StructEventParms(i)->GetName());
		}
		HeaderText.Logf(TEXT("%s{\r\n"), appSpc(4));
		HeaderText.Logf(TEXT("%s}\r\n"), appSpc(4));
		HeaderText.Log( TEXT("};\r\n") );
	}
}

/**
 * Get the intrinsic null value for this property
 * 
 * @param	Prop				the property to get the null value for
 * @param	bMacroContext		TRUE when exporting the P_GET* macro, FALSE when exporting the friendly C++ function header
 * @param	bTranslatePointers	if true, FPointer structs will be set to NULL instead of FPointer()
 *
 * @return	the intrinsic null value for the property (0 for ints, TEXT("") for strings, etc.)
 */
FString FNativeClassHeaderGenerator::GetNullParameterValue( UProperty* Prop, UBOOL bMacroContext, UBOOL bTranslatePointers/*=FALSE*/ )
{
	if ( !bMacroContext && Prop->HasAllPropertyFlags(CPF_OptionalParm|CPF_OutParm) )
	{
		return TEXT("NULL");
	}

	UClass* PropClass = Prop->GetClass();
	if (PropClass == UByteProperty::StaticClass()
	||	PropClass == UIntProperty::StaticClass()
	||	PropClass == UBoolProperty::StaticClass()
	||	PropClass == UFloatProperty::StaticClass() )
	{
        // if we have a BoolProperty then set it to be FALSE instead of 0
 		if( PropClass == UBoolProperty::StaticClass() )
 		{
 			return TEXT("FALSE");
 		}

		return TEXT("0");
	}
	else if ( PropClass == UNameProperty::StaticClass() )
	{
		return TEXT("NAME_None");
	}
	else if ( PropClass == UStrProperty::StaticClass() )
	{
		return TEXT("TEXT(\"\")");
	}
	else if ( PropClass == UArrayProperty::StaticClass()
		||    PropClass == UMapProperty::StaticClass()
		||    PropClass == UStructProperty::StaticClass()
		||    PropClass == UDelegateProperty::StaticClass() )
	{
		if ( bTranslatePointers && Prop->GetFName() == NAME_Pointer && PropClass == UStructProperty::StaticClass() )
		{
			return TEXT("NULL");
		}
		else
		{
			FString Type, ExtendedType;
			Type = Prop->GetCPPType(&ExtendedType,CPPF_OptionalValue);
			return Type + ExtendedType + TEXT("(EC_EventParm)");
		}
	}
	else if ( PropClass->HasAnyCastFlag(CASTCLASS_UObjectProperty) )
	{
		return TEXT("NULL");
	}
	else if ( PropClass == UInterfaceProperty::StaticClass() )
	{
		return TEXT("NULL");
	}

	appErrorf(TEXT("GetNullParameterValue - Unhandled property type '%s': %s"), *PropClass->GetName(), *Prop->GetPathName());
	return TEXT("");
}

/**
 * Retrieve the default value for an optional parameter
 *
 * @param	Prop			the property being parsed
 * @param	bMacroContext	TRUE when exporting the P_GET* macro, FALSE when exporting the friendly C++ function header
 * @param	DefaultValue	[out] filled in with the default value text for this parameter
 *
 * @return	TRUE if default value text was successfully retrieved for this property
 */
UBOOL FNativeClassHeaderGenerator::GetOptionalParameterValue( UProperty* Prop, UBOOL bMacroContext, FString& DefaultValue )
{
	FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

	FTokenData* PropData = ClassData->FindTokenData(Prop);
	UBOOL bResult = PropData != NULL && PropData->DefaultValue != NULL;
	if ( bResult )
	{
		FTokenChain& TokenChain = PropData->DefaultValue->ParsedExpression;
		INT StartIndex = 0;

		// if the last link in the token chain is a literal value, we only need to export that
		if ( TokenChain.Top().TokenType == TOKEN_Const )
		{
			StartIndex = TokenChain.Num() - 1;
		}

		for ( INT TokenIndex = StartIndex; TokenIndex < TokenChain.Num(); TokenIndex++ )
		{
			FToken& Token = TokenChain(TokenIndex);

			// TokenType is TOKEN_None for functions
			if ( Token.TokenType != TOKEN_None )
			{
				if ( Token.TokenType == TOKEN_Const )
				{
					// constant value; either a literal or a reference to a script const
					if ( Token.Type == CPT_String )
					{
						DefaultValue += FString::Printf(TEXT("TEXT(\"%s\")"), *Token.GetValue());
					}
					else if ( Token.IsObject() )
					{
						// literal reference to object - these won't really work very well, but we'll try...

						// let's find out what other types there are  =)
						if  ( Token.TokenName != NAME_Class )
						{
							appErrorf(TEXT("The only type of explicit object reference allowed as the default value for an optional parameter of a native function is a class (%s.%s)"), *CurrentClass->GetName(), *Prop->GetOuter()->GetName());
						}

						if ( !Token.MetaClass->HasAnyClassFlags(CLASS_Native) )
						{
							appErrorf(TEXT("Not allowed to use an explicit reference to a non-native class as the default value for an optional parameter of a native function (%s.%s)"), *CurrentClass->GetName(), *Prop->GetOuter()->GetName());
						}

						if ( bMacroContext || TokenChain.Num() == 1 )
						{
							const TCHAR* MetaClassName = NameLookupCPP->GetNameCPP(Token.MetaClass);
							DefaultValue += FString::Printf(TEXT("%s::StaticClass()"), MetaClassName);

							// if this isn't the last token in the chain, emit the member access operator
							if ( TokenIndex < TokenChain.Num() - 1 )
							{
								DefaultValue += FString::Printf(TEXT("->GetDefaultObject<%s>()->"), MetaClassName);
							}
						}
						else
						{
							bResult = FALSE;
							break;
						}
					}
					else if (Token.Type == CPT_Name)
					{
						// name - need to add in FName constructor stuff
						//@todo: would be nice to do this for all hardcoded names, but for now, special case for the current use
						if (((FName*)Token.NameBytes)->GetIndex() == NAME_Timer)
						{
							DefaultValue += FString::Printf(TEXT("NAME_%s"), *Token.GetValue());
						}
						else
						{
							DefaultValue += FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *Token.GetValue());
						}
					}
					else
					{
						DefaultValue += Token.GetValue();
					}
				}
				else if ( Token.TokenType == TOKEN_Identifier )
				{
					if ( Token.Type == CPT_Struct )
					{
						DefaultValue += Token.Identifier;
						// if this isn't the last token in the chain, emit the member access operator
						if ( TokenIndex < TokenChain.Num() - 1 )
						{
							DefaultValue += TCHAR('.');
						}
					}
					else if ( Token.IsObject() )
					{
						DefaultValue += Token.Identifier;

						// if this isn't the last token in the chain, emit the member access operator
						if ( TokenIndex < TokenChain.Num() - 1 )
						{
							DefaultValue += TEXT("->");
						}
					}
					else if (bMacroContext)
					{
						if ( Token.Type == CPT_None )
						{
							if ( Token.Matches(NAME_Default) || Token.Matches(NAME_Static) )
							{
								// reference to the owning class's default object
								DefaultValue += FString::Printf(TEXT("GetClass()->GetDefaultObject<%s>()"), NameLookupCPP->GetNameCPP(CurrentClass));
								// if this isn't the last token in the chain, emit the member access operator
								if ( TokenIndex < TokenChain.Num() - 1 )
								{
									DefaultValue += TEXT("->");
								}
							}
							else if ( Token.TokenName == NAME_Super )
							{
								DefaultValue += FString::Printf(TEXT("%s::"), Token.MetaClass
										? NameLookupCPP->GetNameCPP(Token.MetaClass)
										: TEXT("Super"));
							}
							else
							{
								bResult = FALSE;
								break;
							}
						}
						else
						{
							DefaultValue += Token.Identifier;
						}
					}
					else
					{
						bResult = FALSE;
						break;
					}
				}
				else
				{
					appErrorf(TEXT("Unhandled token type %i in GetOptionalParameterValue!"), (INT)Token.TokenType);
				}
			}
			else if ( Token.Type == CPT_ObjectReference && Token.TokenName == NAME_Self )
			{
				if ( bMacroContext )
				{
					DefaultValue += TEXT("this");
				}
				else
				{
					FStringOutputDevice InvalidObjectString = TEXT("((");
					Prop->ExportCppDeclaration(InvalidObjectString, FALSE, TRUE, FALSE);
					ApplyAlternatePropertyExportText(Prop, InvalidObjectString);

					// string now looks like "((class UObject* PropName", so cut off the string at the last space character
					INT SpacePos = InvalidObjectString.InStr(TEXT(" "), TRUE);
					check(SpacePos>0);

					DefaultValue += InvalidObjectString.Left(SpacePos) + TEXT(")-1)");
				}
			}
			else if ( bMacroContext )
			{
				//@todo Super.FunctionCall still broken

				// it's a function call - these can only be used in the P_GET macros
				if ( Token.TokenFunction != NULL )
				{
					const FFuncInfo& FunctionData = Token.TokenFunction->GetFunctionData();
					UFunction* Function = FunctionData.FunctionReference;

					const UBOOL bIsEvent = Function->HasAnyFunctionFlags(FUNC_Event);
					const UBOOL bIsDelegate = Function->HasAnyFunctionFlags(FUNC_Delegate);
					const UBOOL bIsNativeFunc = Function->HasAnyFunctionFlags(FUNC_Native) && (FunctionData.FunctionExportFlags&FUNCEXPORT_NoExport) == 0;

					if ( !bIsEvent && !bIsDelegate && !bIsNativeFunc )
					{
						appErrorf(TEXT("Invalid optional parameter value in (%s.%s).  The only function type allowed as the default value for an optional parameter of a native function is event, delegate, or exported native function"), *CurrentClass->GetName(), *Prop->GetOuter()->GetName());
					}

					//@todo: process the parameter values to translate context references (i.e. replace . with -> for objects, etc.)
					if ( bIsEvent )
					{
						DefaultValue += TEXT("event");
					}
					else if ( bIsDelegate )
					{
						DefaultValue += TEXT("delegate");
					}
					DefaultValue += PropData->DefaultValue->RawExpression;
				}
			}
			else
			{
				bResult = FALSE;
				break;
			}
		}
	}

	if ( bResult && DefaultValue.Len() == 1 )
	{
		bResult = FALSE;
	}
	return bResult;
}

/**
 * Exports a native function prototype
 *
 * @param	FunctionData	data representing the function to export
 * @param	bEventTag		TRUE to export this function prototype as an event stub, FALSE to export as a native function stub.
 *							Has no effect if the function is a delegate.
 * @param	Return			[out] will be assigned to the return value for this function, or NULL if the return type is void
 * @param	Parameters		[out] will be filled in with the parameters for this function
 */
void FNativeClassHeaderGenerator::ExportNativeFunctionHeader( const FFuncInfo& FunctionData, UBOOL bEventTag, UProperty*& Return, TArray<UProperty*>&Parameters )
{
	UFunction* Function = FunctionData.FunctionReference;

	UBOOL bIsInterface = Function->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface);
	UBOOL bOutputEnabled = bEventTag || (FunctionData.FunctionExportFlags&FUNCEXPORT_NoExportHeader) == 0;
	

	// Return type.
	Return = Function->GetReturnProperty();
	if (bOutputEnabled)
	{
		HeaderText.Log( appSpc(4) );

		// if the owning class is an interface class
		if ( bIsInterface ||
		//	or this is not an event, AND
		(	!bEventTag
		//	the function is not a static function, AND
		&&	!Function->HasAnyFunctionFlags(FUNC_Static)
		//	the function is either virtual or not marked final
		&& ((FunctionData.FunctionExportFlags&FUNCEXPORT_Virtual) != 0 || (FunctionData.FunctionExportFlags&FUNCEXPORT_Final) == 0)) )
		{
			HeaderText.Log(TEXT("virtual "));
		}

		if( !Return )
		{
			HeaderText.Log( TEXT("void") );
		}
		else
		{
			FString ReturnType, ExtendedReturnType;
			ReturnType = Return->GetCPPType(&ExtendedReturnType);
			FStringOutputDevice ReplacementText(*ReturnType);
			ApplyAlternatePropertyExportText(Return, ReplacementText);
			ReturnType = ReplacementText;
			HeaderText.Logf(TEXT("%s%s"), *ReturnType, *ExtendedReturnType);
		}

		// Function name and parms.
		FString FunctionType;
		if( Function->HasAnyFunctionFlags(FUNC_Delegate) )
		{
			FunctionType = TEXT("delegate");
		}
		else if ( bEventTag )
		{
			FunctionType = TEXT("event");
		}
		else
		{
			// nothing
		}

		HeaderText.Logf( TEXT(" %s%s("), *FunctionType, *Function->GetName() );
	}

	INT ParmCount=0;
	Parameters.Empty();
	UBOOL bHasOptionalParms = FALSE;
	for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
	{
		Parameters.AddItem(*It);
		if (bOutputEnabled)
		{
			if( ParmCount++ )
			{
				HeaderText.Log(TEXT(","));
			}

			FStringOutputDevice PropertyText;
			It->ExportCppDeclaration( PropertyText, FALSE, TRUE, FALSE );
			ApplyAlternatePropertyExportText(*It, PropertyText);

			HeaderText += PropertyText;
			if ( It->HasAnyPropertyFlags(CPF_OptionalParm) )
			{
				bHasOptionalParms = TRUE;

				FString DefaultValue=TEXT("=");
				if ( !GetOptionalParameterValue(*It, FALSE, DefaultValue) )
				{
					DefaultValue += GetNullParameterValue(*It,FALSE);
				}

				HeaderText.Log(*DefaultValue);
			}
		}
	}
	if (bOutputEnabled)
	{
		HeaderText.Log( TEXT(")") );
		if ( (FunctionData.FunctionExportFlags & FUNCEXPORT_Const) != 0 )
		{
			HeaderText.Log( TEXT(" const") );
		}

		if ( bIsInterface )
		{
			// all methods in interface classes are pure virtuals
			HeaderText.Log(TEXT("=0"));
		}
	}
}

/**
 * Exports the native stubs for the list of functions specified
 *
 * @param	NativeFunctions	the functions to export
 */
void FNativeClassHeaderGenerator::ExportNativeFunctions( const TArray<UFunction*>& NativeFunctions )
{
	// This is used to allow the C++ declarations and stubs to be separated without iterating through the list of functions twice
	// (put the stubs in this archive, then append this archive to the main archive once we've exported all declarations)
	FStringOutputDevice UnrealScriptWrappers;

	// find the class info for this class
	FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

	// export the C++ stubs
	for ( INT i = NativeFunctions.Num() - 1; i >= 0; i-- )
	{
		UFunction* Function = NativeFunctions(i);
		FFunctionData* CompilerInfo = ClassData->FindFunctionData(Function);
		check(CompilerInfo);

		const FFuncInfo& FunctionData = CompilerInfo->GetFunctionData();
		
		UProperty* Return = NULL;
		TArray<UProperty*> Parameters;

		UBOOL bExportExtendedWrapper = FALSE;
		if ( (FunctionData.FunctionExportFlags&FUNCEXPORT_NoExport) == 0 &&
			(Function->FunctionFlags&FUNC_Iterator) == 0 &&
			(Function->FunctionFlags&FUNC_Operator) == 0 )
			bExportExtendedWrapper = TRUE;

		if ( bExportExtendedWrapper)
		{
			ExportNativeFunctionHeader(FunctionData,FALSE,Return,Parameters);
			if ((FunctionData.FunctionExportFlags&FUNCEXPORT_NoExportHeader) == 0)
			{
				HeaderText.Logf(TEXT(";%s"), LINE_TERMINATOR);
			}
		}

		// if this function was originally declared in a base class, and it isn't a static function,
		// only the C++ function header will be exported
		if ( !ShouldExportFunction(Function) )
			continue;

		// export the script wrappers
		UnrealScriptWrappers.Logf( TEXT("%sDECLARE_FUNCTION(exec%s)"), appSpc(4), *Function->GetName() );

		if ( bExportExtendedWrapper )
		{
			UnrealScriptWrappers.Logf(TEXT("%s%s{%s"), LINE_TERMINATOR, appSpc(4), LINE_TERMINATOR);

			// export the GET macro for this parameter
			FString ParameterList;
			for ( INT ParameterIndex = 0; ParameterIndex < Parameters.Num(); ParameterIndex++ )
			{
				FString EvalBaseText = TEXT("P_GET_");	// e.g. P_GET_STR
				FString EvalModifierText;				// e.g. _OPTX, _OPTX_REF
				FString EvalParameterText;				// e.g. (UObject*,NULL)


				UProperty* Param = Parameters(ParameterIndex);
				FString TypeText;
				if ( Param->ArrayDim > 1 )
				{
					EvalBaseText += TEXT("ARRAY");
					// questionable
					TypeText = Param->GetCPPType();
				}
				else
				{
					EvalBaseText += Param->GetCPPMacroType(TypeText);
				}
				FStringOutputDevice ReplacementText(*TypeText);
				ApplyAlternatePropertyExportText(Param, ReplacementText);
				TypeText = ReplacementText;

				FString DefaultValueText;
				if ( Param->GetClass() == UStructProperty::StaticClass() )
				{
					// if this is a struct property which contains NoInit types
					UStructProperty* StructParam = ExactCast<UStructProperty>(Param);
					if ( (StructParam->Struct->StructFlags&STRUCT_Transient) == 0 &&
						StructParam->Struct->ConstructorLink != NULL )
					{
						EvalModifierText += TEXT("_INIT");
					}
				}

				// if this property is an optional parameter, add the OPTX tag
				if ( (Param->PropertyFlags&CPF_OptionalParm) != 0 )
				{
					DefaultValueText = TEXT(",");
					EvalModifierText += TEXT("_OPTX");
					if ( !GetOptionalParameterValue(Param, TRUE, DefaultValueText) )
					{
						DefaultValueText += GetNullParameterValue(Param, TRUE);
					}
				}

				// if this property is an out parm, add the REF tag
				if ( (Param->PropertyFlags&CPF_OutParm) != 0 )
				{
					EvalModifierText += TEXT("_REF");
				}

				// if this property requires a specialization, add a comma to the type name so we can print it out easily
				if ( TypeText != TEXT("") )
				{
					TypeText += TCHAR(',');
				}

				EvalParameterText = FString::Printf(TEXT("(%s%s%s)"), *TypeText, *Param->GetName(), *DefaultValueText);

				UnrealScriptWrappers.Logf(TEXT("%s%s%s%s;%s"), appSpc(8), *EvalBaseText, *EvalModifierText, *EvalParameterText, LINE_TERMINATOR);

				// add this property to the parameter list string
				if ( ParameterList.Len() )
				{
					ParameterList += TCHAR(',');
				}

				if ( Param->HasAllPropertyFlags(CPF_OptionalParm|CPF_OutParm) && Param->ArrayDim == 1 )
				{
					ParameterList += FString::Printf(TEXT("p%s ? &%s : NULL"), *Param->GetName(), *Param->GetName());
				}
				else
				{
					ParameterList += Param->GetName();
				}
			}

			UnrealScriptWrappers.Logf(TEXT("%sP_FINISH;%s"), appSpc(8), LINE_TERMINATOR);

			// write out the return value
			UnrealScriptWrappers.Log(appSpc(8));
			if ( Return != NULL )
			{
				FString ReturnType, ReturnExtendedType;
				ReturnType = Return->GetCPPType(&ReturnExtendedType);
				FStringOutputDevice ReplacementText(*ReturnType);
				ApplyAlternatePropertyExportText(Return, ReplacementText);
				ReturnType = ReplacementText;
				UnrealScriptWrappers.Logf(TEXT("*(%s%s*)Result="), *ReturnType, *ReturnExtendedType);
			}

			// export the call to the C++ version
			UnrealScriptWrappers.Logf(TEXT("this->%s(%s);%s"), *Function->GetName(), *ParameterList, LINE_TERMINATOR);
			UnrealScriptWrappers.Logf(TEXT("%s}%s"), appSpc(4), LINE_TERMINATOR);
		}
		else
		{
			UnrealScriptWrappers.Logf(TEXT(";%s"), LINE_TERMINATOR);
		}
	}

	HeaderText += UnrealScriptWrappers;
}

TMap<class UFunction*,INT> FuncEmitCountMap;

/**
 * Exports the methods which trigger UnrealScript events and delegates.
 *
 * @param	CallbackFunctions	the functions to export
 */
void FNativeClassHeaderGenerator::ExportCallbackFunctions( const TArray<UFunction*>& CallbackFunctions )
{
	// find the class info for this class
	FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);
	UBOOL bIsInterface = CurrentClass->HasAnyClassFlags(CLASS_Interface);

	for ( INT CallbackIndex = 0; CallbackIndex < CallbackFunctions.Num(); CallbackIndex++ )
	{
		UFunction* Function = CallbackFunctions(CallbackIndex);
		UClass* Class = CurrentClass;

		FFunctionData* CompilerInfo = ClassData->FindFunctionData(Function);
		check(CompilerInfo);

		const FFuncInfo& FunctionData = CompilerInfo->GetFunctionData();

		UProperty* Return = NULL;
		TArray<UProperty*> Parameters;

		// export the line that looks like: INT eventMain(const FString& Parms)
		ExportNativeFunctionHeader(FunctionData,TRUE,Return,Parameters);
		if ( bIsInterface )
		{
			HeaderText.Log(TEXT(";\r\n"));
			continue;
		}

		const UBOOL ProbeOptimization = (Function->GetFName().GetIndex()>=NAME_PROBEMIN && Function->GetFName().GetIndex()<NAME_PROBEMAX);

		// cache the TCHAR* for a few strings we'll use a lot here
		const FString FunctionName = Function->GetName();
		const FString ReturnName = Return ? Return->GetName() : TEXT("");

		// now the body - first we need to declare a struct which will hold the parameters for the event/delegate call
		HeaderText.Logf( TEXT("\r\n%s{\r\n"), appSpc(4) );

		// declare and zero-initialize the parameters and return value, if applicable
		if( Return != NULL || Parameters.Num() > 0 )
		{
			// export a line which will invoke the struct using the EC_EventParm ctor
			HeaderText.Logf( TEXT("%s%s_event%s_Parms Parms(EC_EventParm);\r\n"), appSpc(8), *Class->GetName(), *FunctionName );
			if ( Return != NULL )
			{
				// if we have a return value, initialize to 0 for types that require initialization.
				if ( Cast<UStructProperty>(Return,CLASS_IsAUStructProperty) != NULL )
				{
					HeaderText.Logf( TEXT("%sappMemzero(&Parms.%s,sizeof(Parms.%s));\r\n"), appSpc(8), *ReturnName, *ReturnName );
				}
				else if ( Cast<UStrProperty>(Return) == NULL && Cast<UInterfaceProperty>(Return) == NULL )
				{
					const FString NullValue = GetNullParameterValue(Return, TRUE, FALSE);
					if ( NullValue.Len() > 0 )
					{
						HeaderText.Logf(TEXT("%sParms.%s=%s;\r\n"), appSpc(8), *ReturnName, *NullValue);
					}
				}
			}
		}
		if( ProbeOptimization )
		{
			HeaderText.Logf(TEXT("%sif(IsProbing(NAME_%s)) {\r\n"), appSpc(8), *FunctionName);
		}

		if( Return != NULL || Parameters.Num() > 0 )
		{
			// Declare a parameter struct for this event/delegate and assign the struct members using the values passed into the event/delegate call.
			for ( INT ParmIndex = 0; ParmIndex < Parameters.Num(); ParmIndex++ )
			{
				UProperty* Prop = Parameters(ParmIndex);
				const FString PropertyName = Prop->GetName();

				if ( Prop->ArrayDim > 1 )
				{
					HeaderText.Logf( TEXT("%sappMemcpy(Parms.%s,%s,sizeof(Parms.%s));\r\n"), appSpc(8), *PropertyName, *PropertyName, *PropertyName );
				}
				else
				{
					FString ValueAssignmentText = PropertyName;
					if ( Cast<UBoolProperty>(Prop) != NULL )
					{
						ValueAssignmentText += TEXT(" ? FIRST_BITFIELD : FALSE");
					}
					else if ( Prop->HasAnyPropertyFlags(CPF_OptionalParm) && Cast<UObjectProperty>(Prop) != NULL )
					{
						// if it's an optional object parameter and the default value specified in script was "Self", the exporter
						// will convert that to INVALID_OBJECT....convert it back now.
						FString DefaultValue;
						if (GetOptionalParameterValue(Prop, FALSE, DefaultValue)
						&&	DefaultValue.Right(3) == TEXT("-1)") )
						{
							ValueAssignmentText += TEXT("==INVALID_OBJECT ? this : ");
							ValueAssignmentText += PropertyName;
						}
					}

					if ( Prop->HasAllPropertyFlags(CPF_OptionalParm|CPF_OutParm) )
					{
						HeaderText.Logf(TEXT("%sif(%s){Parms.%s=*%s;}\r\n"), appSpc(8), *PropertyName, *PropertyName, *ValueAssignmentText);
					}
					else
					{
						HeaderText.Logf( TEXT("%sParms.%s=%s;\r\n"), appSpc(8), *PropertyName, *ValueAssignmentText );
					}
				}
			}
			if( Function->HasAnyFunctionFlags(FUNC_Delegate) )
			{
				HeaderText.Logf( TEXT("%sProcessDelegate(%s_%s,&__%s__Delegate,&Parms);\r\n"), appSpc(8), *API, *FunctionName, *FunctionName );
			}
			else
			{
				HeaderText.Logf( TEXT("%sProcessEvent(FindFunctionChecked(%s_%s),&Parms);\r\n"), appSpc(8), *API, *FunctionName );
			}
		}
		else
		{
			if( Function->HasAnyFunctionFlags(FUNC_Delegate) )
			{
				HeaderText.Logf( TEXT("%sProcessDelegate(%s_%s,&__%s__Delegate,NULL);\r\n"), appSpc(8), *API, *FunctionName, *FunctionName );
			}
			else
			{
				HeaderText.Logf( TEXT("%sProcessEvent(FindFunctionChecked(%s_%s),NULL);\r\n"), appSpc(8), *API, *FunctionName );
			}
		}
		if( ProbeOptimization )
		{
			HeaderText.Logf(TEXT("%s}\r\n"), appSpc(8));
		}

		// Out parm copying.
		for ( INT ParmIndex = 0; ParmIndex < Parameters.Num(); ParmIndex++ )
		{
			UProperty* Prop = Parameters(ParmIndex);

			const FString PropertyName = Prop->GetName();
			if ( Prop->HasAnyPropertyFlags(CPF_OutParm) 
			&&	(!Prop->HasAnyPropertyFlags(CPF_Const)
				|| Cast<UObjectProperty>(Prop) != NULL) )
			{
				if ( Prop->ArrayDim > 1 )
				{
					HeaderText.Logf( TEXT("%sappMemcpy(&%s,&Parms.%s,sizeof(%s));\r\n"), appSpc(8), *PropertyName, *PropertyName, *PropertyName );
				}
				else if ( Prop->HasAnyPropertyFlags(CPF_OptionalParm) )
				{
					HeaderText.Logf(TEXT("%sif ( %s ) { *%s=Parms.%s; }\r\n"), appSpc(8), *PropertyName, *PropertyName, *PropertyName);
				}
				else
				{
					HeaderText.Logf(TEXT("%s%s=Parms.%s;\r\n"), appSpc(8), *PropertyName, *PropertyName);
				}
			}
		}

		// Return value.
		if( Return )
		{
			HeaderText.Logf( TEXT("%sreturn Parms.%s;\r\n"), appSpc(8), *ReturnName );
		}
		HeaderText.Logf( TEXT("%s}\r\n"), appSpc(4) );
	}
}


/**
 * Determines if the property has alternate export text associated with it and if so replaces the text in PropertyText with the
 * alternate version. (for example, structs or properties that specify a native type using export-text).  Should be called immediately
 * after ExportCppDeclaration()
 *
 * @param	Prop			the property that is being exported
 * @param	PropertyText	the string containing the text exported from ExportCppDeclaration
 */
void FNativeClassHeaderGenerator::ApplyAlternatePropertyExportText( UProperty* Prop, FStringOutputDevice& PropertyText )
{
	// if this is a pointer property and a pointer type was specified, export that type rather than FPointer
	UStructProperty* StructProp = Cast<UStructProperty>(Prop,CLASS_IsAUStructProperty);
	if ( StructProp == NULL )
	{
		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
		if ( ArrayProp != NULL )
		{
			StructProp = Cast<UStructProperty>(ArrayProp->Inner,CLASS_IsAUStructProperty);
		}
	}
	if ( StructProp )
	{
		// find the class info for this class
		FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

		// find the compiler token for this property
		FTokenData* PropData = ClassData->FindTokenData(Prop);
		if ( PropData != NULL )
		{
			FString ExportText;
			if ( PropData->Token.ExportInfo.Len() > 0 )
			{
				ExportText = PropData->Token.ExportInfo;
			}
			else
			{
				// we didn't have any export-text associated with the variable, so check if we have
				// anything for the struct itself
				FStructData* StructData = ClassData->FindStructData(StructProp->Struct);
				if ( StructData != NULL && StructData->StructData.ExportInfo.Len() )
				{
					ExportText = StructData->StructData.ExportInfo;
				}
			}

			if ( ExportText.Len() > 0 )
			{
				// special case the pointer struct - add the asterisk for them
				if ( StructProp->Struct->GetFName() == NAME_Pointer )
				{
					ExportText += TEXT("*");
				}

				(FString&)PropertyText = PropertyText.Replace(NameLookupCPP->GetNameCPP(StructProp->Struct), *ExportText);
			}
		}
	}
	else
	{
		UMapProperty* MapProp = Cast<UMapProperty>(Prop);
		if ( MapProp != NULL )
		{
			// find the class info for this class
			FClassMetaData* ClassData = GScriptHelper->FindClassData(CurrentClass);

			// find the compiler token for this property
			FTokenData* PropData = ClassData->FindTokenData(Prop);
			if ( PropData != NULL && PropData->Token.ExportInfo.Len() > 0 )
			{
				(FString&)PropertyText = PropertyText.Replace(TEXT("fixme"), *PropData->Token.ExportInfo);
			}
		}
	}
}

/**
 * Sorts the list of header files being exported from a package according to their dependency on each other.
 *
 * @param	HeaderDependencyMap		a mapping of header filenames to a list of header filenames that must be processed before that one.
 *									(intentionally a copy)
 * @param	SortedHeaderFilenames	[out] receives the sorted list of header filenames.
 */
UBOOL FNativeClassHeaderGenerator::SortHeaderDependencyMap( TMap<INT, TLookupMap<INT> >& HeaderDependencyMap, TLookupMap<INT>& SortedHeaderFilenames ) const
{
	TArray<UBOOL> AddedHeaders;
	AddedHeaders.AddZeroed(HeaderDependencyMap.Num());
	SortedHeaderFilenames.Empty(HeaderDependencyMap.Num());

	while ( SortedHeaderFilenames.Num() < HeaderDependencyMap.Num() )
	{
		UBOOL bAddedSomething = FALSE;

		// Find headers with no dependencies and add those to the list.
		for ( TMap<INT, TLookupMap<INT> >::TIterator It(HeaderDependencyMap); It; ++It )
		{
			INT HeaderIndex = It.Key();
			if ( AddedHeaders(HeaderIndex) == FALSE )
			{
				TLookupMap<INT>& Dependencies = It.Value();
				UBOOL bHasRemainingDependencies = FALSE;
				for ( INT DependencyIndex=0; DependencyIndex < Dependencies.Num(); ++DependencyIndex )
				{
					INT DependentHeaderIndex = Dependencies(DependencyIndex);
					if ( AddedHeaders(DependentHeaderIndex) == FALSE )
					{
						bHasRemainingDependencies = TRUE;
						break;
					}
				}

				if ( bHasRemainingDependencies == FALSE )
				{
					// Add it to the list.
					SortedHeaderFilenames.AddItem( HeaderIndex );
					AddedHeaders(HeaderIndex) = TRUE;
					bAddedSomething = TRUE;
				}
			}
		}

		// Circular dependency error?
		if ( bAddedSomething == FALSE )
		{
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Finds to headers that are dependent on each other.
 * Wrapper for FindInterDependencyRecursive().
 *
 * @param	HeaderDependencyMap	A map of headers and their dependencies. Each header is represented as an index into a TArray of the actual filename strings.
 * @param	HeaderIndex			A header to scan for any inter-dependency.
 * @param	OutHeader1			[out] Receives the first inter-dependent header index.
 * @param	OutHeader2			[out] Receives the second inter-dependent header index.
 * @return	TRUE if an inter-dependency was found.
 */
UBOOL FNativeClassHeaderGenerator::FindInterDependency( TMap<INT, TLookupMap<INT> >& HeaderDependencyMap, INT HeaderIndex, INT& OutHeader1, INT& OutHeader2 )
{
	TArray<UBOOL> VisitedHeaders;
	VisitedHeaders.AddZeroed( HeaderDependencyMap.Num() );
	return FindInterDependencyRecursive( HeaderDependencyMap, HeaderIndex, VisitedHeaders, OutHeader1, OutHeader2 );
}

/**
 * Finds to headers that are dependent on each other.
 *
 * @param	HeaderDependencyMap	A map of headers and their dependencies. Each header is represented as an index into a TArray of the actual filename strings.
 * @param	HeaderIndex			A header to scan for any inter-dependency.
 * @param	VisitedHeaders		Must be filled with FALSE values before the first call (must be large enough to be indexed by all headers).
 * @param	OutHeader1			[out] Receives the first inter-dependent header index.
 * @param	OutHeader2			[out] Receives the second inter-dependent header index.
 * @return	TRUE if an inter-dependency was found.
 */
UBOOL FNativeClassHeaderGenerator::FindInterDependencyRecursive( TMap<INT, TLookupMap<INT> >& HeaderDependencyMap, INT HeaderIndex, TArray<UBOOL>& VisitedHeaders, INT& OutHeader1, INT& OutHeader2 )
{
	VisitedHeaders(HeaderIndex) = TRUE;
	TLookupMap<INT>& Dependencies = *HeaderDependencyMap.Find( HeaderIndex );
	for ( INT DependencyIndex=0; DependencyIndex < Dependencies.Num(); ++DependencyIndex )
	{
		INT DependentHeaderIndex = Dependencies(DependencyIndex);
		if ( VisitedHeaders(DependentHeaderIndex) )
		{
			OutHeader1 = HeaderIndex;
			OutHeader2 = DependentHeaderIndex;
			return TRUE;
		}
		if ( FindInterDependencyRecursive( HeaderDependencyMap, DependentHeaderIndex, VisitedHeaders, OutHeader1, OutHeader2 ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Finds a dependency chain between two class header files.
 * Wrapper around FindDependencyChainRecursive().
 *
 * @param	Class				A class to scan for a dependency chain between the two headers.
 * @param	Header1				First class header filename.
 * @param	Header2				Second class header filename.
 * @param	DependencyChain		[out] Receives dependency chain, if found.
 * @return	TRUE if a dependency chain was found and filled in.
 */
UBOOL FNativeClassHeaderGenerator::FindDependencyChain( const UClass* Class, const FString& Header1, const FString& Header2, TArray<const UClass*>& DependencyChain )
{
	DependencyChain.Empty();
	return FindDependencyChainRecursive( Class, Header1, Header2, FALSE, DependencyChain );
}

/**
 * Finds a dependency chain between two class header files.
 *
 * @param	Class				A class to scan for a dependency chain between the two headers.
 * @param	Header1				First class header filename.
 * @param	Header2				Second class header filename.
 * @param	bChainStarted		Whether Header1 has been found and we've started to fill in DependencyChain. Must be FALSE to begin with.
 * @param	DependencyChain		[out] Receives dependency chain, if found. Must be empty before the call.
 * @return	TRUE if a dependency chain was found and filled in.
 */
UBOOL FNativeClassHeaderGenerator::FindDependencyChainRecursive( const UClass* Class, const FString& Header1, const FString& Header2, UBOOL bChainStarted, TArray<const UClass*>& DependencyChain )
{
	UBOOL bIsExportClass = Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_NoExport|CLASS_Intrinsic);
	if ( bIsExportClass )
	{
		if ( !bChainStarted && Class->ClassHeaderFilename == Header1 )
		{
			bChainStarted = TRUE;
		}
		if ( bChainStarted )
		{
			DependencyChain.AddItem( Class );
			if ( Class->ClassHeaderFilename == Header2 )
			{
				return TRUE;
			}
		}
	}

	UClass* SuperClass = Class->GetSuperClass();
	if ( SuperClass && SuperClass->GetOuter() == Class->GetOuter() && FindDependencyChainRecursive( SuperClass, Header1, Header2, bChainStarted, DependencyChain ) )
	{
		return TRUE;
	}

	for (TArray<FImplementedInterface>::TConstIterator It(Class->Interfaces); It; ++It)
	{
		UClass* InterfaceClass = It->Class;
		if ( InterfaceClass->GetOuter() == Class->GetOuter() && FindDependencyChainRecursive( InterfaceClass, Header1, Header2, bChainStarted, DependencyChain ) )
		{
			return TRUE;
		}
	}

	if ( bIsExportClass && bChainStarted )
	{
		DependencyChain.RemoveSwap( DependencyChain.Num() - 1 );
	}
	return FALSE;
}

// Constructor.
FNativeClassHeaderGenerator::FNativeClassHeaderGenerator( UPackage* InPackage, FClassTree& inClassTree )
: Package(InPackage)
, API(InPackage->GetName().ToUpper())
, CurrentClass(NULL)
, ClassTree(inClassTree)
, bAutoExport(ParseParam(appCmdLine(),TEXT("AUTO")))
{
	// If the Inc directory is missing, then this is a mod/runtime build, and we don't want
	// to export the headers.
	TArray<FString> IncDir;
	GFileManager->FindFiles(IncDir, *(GEditor->EditPackagesInPath * *Package->GetName() * TEXT("Inc")), FALSE, TRUE);
	// if we couldn't find the root directory, then abort
	if (IncDir.Num() == 0)
	{
		// clear the misaligned flag on the classes because the exporting isn't called which would normally do it
		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* Class = *It;
			if( Class->GetOuter()==Package)
			{
				Class->ClearFlags(RF_MisalignedObject);
			}
		}
		debugf(TEXT("Failed to find %s directory, skipping header generation."), *(GEditor->EditPackagesInPath * *Package->GetName() * TEXT("Inc")));
		
		return;
	}

	PackageNamesHeaderFilename = Package->GetName() + TEXT("Names.h");

	// Tag native classes in this package for export.

	// Names which are used by this package will be marked with RF_TagExp, so first we clear that flag for all names.
	// We also need to clear the RF_TagImp flag, as that is used to track duplicate names across multiple header files within the same package
	QWORD ClearMask = RF_TagExp|RF_TagImp;
	for ( INT i=0; i<FName::GetMaxNames(); i++ )
	{
		if( FName::GetEntry(i) )
		{
			FName::GetEntry(i)->ClearFlags( ClearMask );
		}
	}

	Classes.AddItem(UObject::StaticClass());
	ClassTree.GetChildClasses(Classes, FDefaultComparator(), TRUE);

	TMap<FName,UClass*> ClassNameMap;
	for ( INT ClassIndex = 0; ClassIndex < Classes.Num(); ClassIndex++ )
	{
		ClassNameMap.Set(Classes(ClassIndex)->GetFName(), Classes(ClassIndex));
	}

	TLookupMap<FString> UnsortedHeaderFilenames;
	TLookupMap<INT> HeaderFilenames;
	UBOOL bNoCircularDependencyDetected = TRUE;
 	{
	 	TMap<INT,TLookupMap<INT> > HeaderDependencyMap;
		TArray<UClass*> ClassesInPackage;

		ClassesInPackage.AddItem(UObject::StaticClass());
		ClassTree.GetChildClasses(ClassesInPackage, FClassPackageComparator(Package), TRUE);
		for ( INT ClassIndex = 0; ClassIndex < ClassesInPackage.Num(); ClassIndex++ )
		{
			UClass* Cls = ClassesInPackage(ClassIndex);
			UBOOL bIsExportClass = Cls->HasAnyClassFlags(CLASS_Native) && !Cls->HasAnyClassFlags(CLASS_NoExport|CLASS_Intrinsic);
			if ( bIsExportClass )
			{
				INT HeaderIndex = UnsortedHeaderFilenames.AddItem(Cls->ClassHeaderFilename);
				TLookupMap<INT>* pDependencies = HeaderDependencyMap.Find(HeaderIndex);
				if ( pDependencies == NULL )
				{
					pDependencies = &HeaderDependencyMap.Set(HeaderIndex, TLookupMap<INT>());
				}

				// Add the super class' header as a dependency if it's different.
				UClass* SuperClass = Cls->GetSuperClass();
				UBOOL bIsDependentExportClass = SuperClass && SuperClass->HasAnyClassFlags(CLASS_Native) && !SuperClass->HasAnyClassFlags(CLASS_NoExport|CLASS_Intrinsic);
				if ( bIsDependentExportClass && SuperClass->GetOuter() == Cls->GetOuter() && SuperClass->ClassHeaderFilename != Cls->ClassHeaderFilename )
				{
					INT DependentHeaderIndex = UnsortedHeaderFilenames.AddItem(SuperClass->ClassHeaderFilename);
					pDependencies->AddItem( DependentHeaderIndex );
				}

				// Add base interface headers as dependencies, if they're different.
				for (TArray<FImplementedInterface>::TIterator It(Cls->Interfaces); It; ++It)
				{
					UClass* InterfaceClass = It->Class;
					UBOOL bIsDependentExportClass = InterfaceClass->HasAnyClassFlags(CLASS_Native) && !InterfaceClass->HasAnyClassFlags(CLASS_NoExport|CLASS_Intrinsic);
					if ( bIsDependentExportClass && InterfaceClass->GetOuter() == Cls->GetOuter() && InterfaceClass->ClassHeaderFilename != Cls->ClassHeaderFilename )
					{
						INT DependentHeaderIndex = UnsortedHeaderFilenames.AddItem(InterfaceClass->ClassHeaderFilename);
						pDependencies->AddItem( DependentHeaderIndex );
					}
				}
			}
		}
	 	bNoCircularDependencyDetected = SortHeaderDependencyMap(HeaderDependencyMap, HeaderFilenames);
		if ( bNoCircularDependencyDetected == FALSE )
		{
			// Find one circular path (though there may be multiple).
			for ( TMap<INT, TLookupMap<INT> >::TIterator It(HeaderDependencyMap); It; ++It )
			{
				INT HeaderIndex = It.Key();
				INT Header1, Header2;
				if ( FindInterDependency( HeaderDependencyMap, HeaderIndex, Header1, Header2 ) )
				{
					const FString& ClassHeaderFilename1 = UnsortedHeaderFilenames(Header1);
					const FString& ClassHeaderFilename2 = UnsortedHeaderFilenames(Header2);
					TArray<const UClass*> DependencyChain1, DependencyChain2;
					for ( INT ClassIndex = 0; DependencyChain1.Num() == 0 && ClassIndex < ClassesInPackage.Num(); ClassIndex++ )
					{
						UClass* Class = ClassesInPackage(ClassIndex);
						FindDependencyChain( Class, ClassHeaderFilename1, ClassHeaderFilename2, DependencyChain1 );
					}
					for ( INT ClassIndex = 0; DependencyChain2.Num() == 0 && ClassIndex < ClassesInPackage.Num(); ClassIndex++ )
					{
						UClass* Class = ClassesInPackage(ClassIndex);
						FindDependencyChain( Class, ClassHeaderFilename2, ClassHeaderFilename1, DependencyChain2 );
					}
					if ( DependencyChain1.Num() > 0 && DependencyChain2.Num() > 0 )
					{
						FString DependencyChainString1 = DependencyChain1(0)->GetName();
						FString DependencyChainString2 = DependencyChain2(0)->GetName();
						for ( INT DependencyIndex=1; DependencyIndex < DependencyChain1.Num(); DependencyIndex++ )
						{
							const UClass* DependencyClass = DependencyChain1(DependencyIndex);
							DependencyChainString1 += TEXT(" -> ");
							DependencyChainString1 += DependencyClass->GetName();
						}
						for ( INT DependencyIndex=1; DependencyIndex < DependencyChain2.Num(); DependencyIndex++ )
						{
							const UClass* DependencyClass = DependencyChain2(DependencyIndex);
							DependencyChainString2 += TEXT(" -> ");
							DependencyChainString2 += DependencyClass->GetName();
						}
						warnf( NAME_Error, TEXT("Header interdependency: %s <-> %s (%s and %s)."),
							*(Package->GetName() + ClassHeaderFilename1 + TEXT("Classes.h")),
							*(Package->GetName() + ClassHeaderFilename2 + TEXT("Classes.h")),
							*DependencyChainString1,
							*DependencyChainString2 );
						break;
					}
				}
			}
			warnf( NAME_Error, TEXT("Interdependent headers detected - aborting!") );
		}
	}

	UBOOL bHasNamesForExport = FALSE;
	TempHeaderPaths.Empty();
	
	for ( INT HeaderIndex = 0; bNoCircularDependencyDetected && HeaderIndex < HeaderFilenames.Num(); HeaderIndex++ )
	{
		INT SortedHeaderIndex = HeaderFilenames(HeaderIndex);
		ClassHeaderFilename = UnsortedHeaderFilenames(SortedHeaderIndex);
		CurrentClass = NULL;
		EnumHeaderText.Empty();
		HeaderText.Empty();
		OriginalHeader.Empty();
		PreHeaderText.Empty();

		INT ClassCount = 0;
		for( INT ClassIndex = 0; ClassIndex < Classes.Num(); ClassIndex++ )
		{
			UClass* Class = Classes(ClassIndex);

			if( Class->GetOuter()==Package && Class->HasAnyClassFlags(CLASS_Native) && Class->ClassHeaderFilename == ClassHeaderFilename )
			{
				if ( !Class->HasAnyClassFlags(CLASS_Parsed|CLASS_Intrinsic) ) 
				{
					if (GIsUnattended
					||	bAutoExport
					||	GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("RemoveNativeClass"),TEXT("Core")),*Class->GetName())) )
					{
						warnf(TEXT("Class '%s' has no script to compile and is not marked intrinsic - removing from static class registration..."), *Class->GetName());
						ClassesToRemove.Add(Class);
						continue;
					}
					else
					{
						warnf(NAME_Warning,TEXT("Class '%s' has no script to compile.  It should be marked intrinsic unless it is being removed."), *Class->GetName());
					}
				}
				else if ( Class->HasAllClassFlags(CLASS_Parsed|CLASS_Intrinsic) )
				{
					warnf(NAME_Error, TEXT("Class '%s' contains script but is marked intrinsic."), *Class->GetName());
				}

				if ( Class->ScriptText && !Class->HasAnyClassFlags(CLASS_NoExport) )
				{
					ClassCount++;
					Class->ClearFlags(RF_TagImp);
					Class->SetFlags(RF_TagExp);

					for ( TFieldIterator<UFunction> Function(Class,FALSE); Function; ++Function)
					{
						if ( Function->HasAnyFunctionFlags(FUNC_Event|FUNC_Delegate) && !Function->GetSuperFunction() )
						{
							// mark this name as needing to be exported
							Function->GetFName().SetFlags(RF_TagExp);
						}
					}
				}
			}
			else
			{
				Class->ClearFlags( RF_TagImp | RF_TagExp );
			}
		}

		if( ClassCount == 0 )
		{
			continue;
		}

		// Load the original header file into memory
		FString HeaderFileLocation = GEditor->EditPackagesInPath * Package->GetName() * TEXT("Inc");
		FString	HeaderPath = HeaderFileLocation * Package->GetName() + ClassHeaderFilename + TEXT("Classes.h");
		appLoadFileToString(OriginalHeader,*HeaderPath);

		debugf( TEXT("Autogenerating C++ header: %s"), *ClassHeaderFilename );

		PreHeaderText.Logf(
			TEXT("/*===========================================================================\r\n")
			TEXT("    C++ class definitions exported from UnrealScript.\r\n")
			TEXT("    This is automatically generated by the tools.\r\n")
			TEXT("    DO NOT modify this manually! Edit the corresponding .uc files instead!\r\n")
			TEXT("    Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.\r\n")
			TEXT("===========================================================================*/\r\n")
			TEXT("#if SUPPORTS_PRAGMA_PACK\r\n")
			TEXT("#pragma pack (push,%i)\r\n")
			TEXT("#endif\r\n")
			TEXT("\r\n"),
			(BYTE)PROPERTY_ALIGNMENT
			);

		// if a global auto-include file exists, generate a line to have that file included
		FString GlobalAutoIncludeFilename = Package->GetName() + ClassHeaderFilename + TEXT("GlobalIncludes.h");
		if ( GFileManager->FileSize(*(HeaderFileLocation * GlobalAutoIncludeFilename)) > 0 )
		{
			PreHeaderText.Logf(TEXT("#include \"%s\"\r\n\r\n"), *GlobalAutoIncludeFilename);
		}

		// export the line to include the package FName header
		PreHeaderText.Logf(TEXT("#include \"%s\"\r\n\r\n"), *PackageNamesHeaderFilename);

		HeaderText.Logf( TEXT("#if !ENUMS_ONLY\r\n") );
		HeaderText.Logf( TEXT("\r\n") );

		HeaderText.Logf(
			TEXT("#ifndef NAMES_ONLY\r\n")
			TEXT("#define AUTOGENERATE_FUNCTION(cls,idx,name)\r\n")
			TEXT("#endif\r\n")
			TEXT("\r\n")
			);

		FString HeaderAPI = API;
		if( ClassHeaderFilename.Len() > 0 )
		{
			HeaderAPI = HeaderAPI + TEXT("_") + ClassHeaderFilename.ToUpper();
		}

		HeaderText.Logf( TEXT("\r\n#ifndef NAMES_ONLY\r\n\r\n") );

		// include guard
		HeaderText.Logf( TEXT("#ifndef INCLUDED_%s_CLASSES\r\n"), *HeaderAPI);
		HeaderText.Logf( TEXT("#define INCLUDED_%s_CLASSES 1\r\n"), *HeaderAPI);
		HeaderText.Log(TEXT("#define ENABLE_DECLARECLASS_MACRO 1\r\n"));
		HeaderText.Log(TEXT("#include \"UnObjBas.h\"\r\n"));
		HeaderText.Log(TEXT("#undef ENABLE_DECLARECLASS_MACRO\r\n"));
		HeaderText.Log( TEXT("\r\n") );

		for ( INT ClassIndex = 0; ClassIndex < Classes.Num(); ClassIndex++ )
		{
			UClass* Class = Classes(ClassIndex);
			if ( Class->ClassHeaderFilename == ClassHeaderFilename && Class->GetOuter() == Package )
			{
				ExportClassHeader(Class);
			}
		}

		HeaderText.Log(TEXT("#undef DECLARE_CLASS\r\n"));
		HeaderText.Log(TEXT("#undef DECLARE_CASTED_CLASS\r\n"));
		HeaderText.Log(TEXT("#undef DECLARE_ABSTRACT_CLASS\r\n"));
		HeaderText.Log(TEXT("#undef DECLARE_ABSTRACT_CASTED_CLASS\r\n"));
		HeaderText.Logf( TEXT("#endif // !INCLUDED_%s_CLASSES\r\n"), *HeaderAPI );
		HeaderText.Log( TEXT("#endif // !NAMES_ONLY\r\n") );
		HeaderText.Log( TEXT("\r\n") );

		for( INT i=0; i<Classes.Num(); i++ )
		{
			UClass* Class = Classes(i);
			if( !Class->HasAnyClassFlags(CLASS_Interface) && Class->HasAnyFlags(RF_TagExp) )
			{
				for( TFieldIterator<UFunction> Function(Class,FALSE); Function; ++Function )
				{
					if ( Function->HasAnyFunctionFlags(FUNC_Native)/*&& ShouldExportFunction(*Function)*/ )
					{
						HeaderText.Logf( TEXT("AUTOGENERATE_FUNCTION(%s,%i,exec%s);\r\n"), NameLookupCPP->GetNameCPP( Class ), Function->iNative ? Function->iNative : -1, *Function->GetName() );
					}
				}
			}
		}

		HeaderText.Logf( TEXT("\r\n") );
		HeaderText.Logf( TEXT("#ifndef NAMES_ONLY\r\n") );
		HeaderText.Logf( TEXT("#undef AUTOGENERATE_FUNCTION\r\n") );
		HeaderText.Logf( TEXT("#endif\r\n") );

		HeaderText.Logf( TEXT("\r\n") );

		HeaderText.Logf( TEXT("#ifdef STATIC_LINKING_MOJO\r\n"));

		HeaderText.Logf( TEXT("#ifndef %s_NATIVE_DEFS\r\n"), *HeaderAPI);
		HeaderText.Logf( TEXT("#define %s_NATIVE_DEFS\r\n"), *HeaderAPI);
		HeaderText.Logf( TEXT("\r\n") );


		HeaderText.Logf( TEXT("#define AUTO_INITIALIZE_REGISTRANTS_%s \\\r\n"), *HeaderAPI );
		for( INT i=0; i<Classes.Num(); i++ )
		{
			UClass* Class = Classes(i);

			UBOOL bShouldExport = Class->HasAnyClassFlags(CLASS_Native) && (ClassesToRemove.Find(Class) == NULL ? TRUE : FALSE) &&
				// @hack: I am sorry. This class can't be exported
				Class->GetName() != TEXT("GearOnlineEventsInterface");
			if( bShouldExport && (Class->GetOuter() == Package) && (Class->ClassHeaderFilename == ClassHeaderFilename) )
			{
				HeaderText.Logf( TEXT("\t%s::StaticClass(); \\\r\n"), NameLookupCPP->GetNameCPP( Class ) );
				if( !Class->HasAnyClassFlags(CLASS_Interface) && Class->HasNativesToExport( Package ) )
				{
					for( TFieldIterator<UFunction> Function(Class,FALSE); Function && Function.GetStruct()==Class; ++Function )
					{
						if( (Function->FunctionFlags&FUNC_Native) != 0 /*&& ShouldExportFunction(*Function)*/ )
						{
							HeaderText.Logf( TEXT("\tGNativeLookupFuncs.Set(FName(\"%s\"), G%s%sNatives); \\\r\n"), *Class->GetName(), *Package->GetName(), NameLookupCPP->GetNameCPP(Class) );
							break;
						}
					}
				}
			}
		}
		HeaderText.Logf( TEXT("\r\n") );

		HeaderText.Logf( TEXT("#endif // %s_NATIVE_DEFS\r\n"), *HeaderAPI ); // #endif // s_NATIVE_DEFS
		HeaderText.Logf( TEXT("\r\n") );

		HeaderText.Logf( TEXT("#ifdef NATIVES_ONLY\r\n") );
		for( INT i=0; i<Classes.Num(); i++ )
		{
			UClass* Class = Classes(i);
			if( !Class->HasAnyClassFlags(CLASS_Interface) && Class->HasNativesToExport( Package ) && (Class->ClassHeaderFilename == ClassHeaderFilename) )
			{
				TArray<UFunction*> Functions;

				for( TFieldIterator<UFunction> Function(Class,FALSE); Function; ++Function )
				{
					if( (Function->FunctionFlags&FUNC_Native) != 0 /*&& ShouldExportFunction(*Function)*/ )
					{
						Functions.AddUniqueItem(*Function);
					}
				}

				if( Functions.Num() )
				{
					HeaderText.Logf( TEXT("FNativeFunctionLookup G%s%sNatives[] = \r\n"), *Package->GetName(), NameLookupCPP->GetNameCPP(Class) );
					HeaderText.Logf( TEXT("{ \r\n"));
					for ( INT i = 0; i < Functions.Num(); i++ )
					{
						UFunction* Function = Functions(i);
						HeaderText.Logf( TEXT("\tMAP_NATIVE(%s, exec%s)\r\n"), NameLookupCPP->GetNameCPP(Class), *Function->GetName() );
					}
					HeaderText.Logf( TEXT("\t{NULL, NULL}\r\n") );
					HeaderText.Logf( TEXT("};\r\n") );
					HeaderText.Logf( TEXT("\r\n") );
				}
			}
		}
		HeaderText.Logf( TEXT("#endif // NATIVES_ONLY\r\n"), *HeaderAPI ); // #endif // NATIVES_ONLY
		HeaderText.Logf( TEXT("#endif // STATIC_LINKING_MOJO\r\n"), *HeaderAPI ); // #endif // STATIC_LINKING_MOJO

		// Generate code to automatically verify class offsets and size.
		HeaderText.Logf( TEXT("\r\n#ifdef VERIFY_CLASS_SIZES\r\n") ); // #ifdef VERIFY_CLASS_SIZES
		for( INT i=0; i<Classes.Num(); i++ )
		{
			UClass* Class = Classes(i);

			UBOOL bShouldExport = Class->HasAnyClassFlags(CLASS_Native) && !Class->HasAnyClassFlags(CLASS_Intrinsic) && (ClassesToRemove.Find(Class) == NULL ? TRUE : FALSE);
			if( bShouldExport && (Class->GetOuter() == Package) && (Class->ClassHeaderFilename ==  ClassHeaderFilename) )
			{
				// Only verify all property offsets for noexport classes to avoid running into compiler limitations.
				if( Class->HasAnyClassFlags(CLASS_NoExport) )
				{
					// Iterate over all properties that are new in this class.
					for( TFieldIterator<UProperty> It(Class,FALSE); It; ++It )
					{
						// We can't verify bools due to the packing and we skip noexport variables so you can e.g. declare a placeholder for the virtual function table in script.
						if( It->ElementSize && Cast<UBoolProperty>(*It) == NULL && !It->HasAnyPropertyFlags(CPF_NoExport) )
						{
							// Emit verification macro. Make sure all offsets are checked correctly.
							if( It->IsEditorOnlyProperty() )
							{
								HeaderText.Logf( TEXT("#if WITH_EDITORONLY_DATA\r\n") );
							}

							HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*It->GetNameCPP());

							if( It->IsEditorOnlyProperty() )
							{
								HeaderText.Logf( TEXT("#endif\r\n") );
							}
						}
					}
				}
				// Verify first and last property for regular classes.
				else
				{
					UProperty* First	= NULL;
					UProperty* Last		= NULL;
					// Track the first and last non-editor properties as we need to ensure their alignments are ok on the console.
					UProperty *FirstNonEditor = NULL;
					UProperty *LastNonEditor = NULL;

					// Iterate over all properties that are new in this class.
					for( TFieldIterator<UProperty> It(Class,FALSE); It; ++It )
					{
						// We can't verify bools due to the packing and we skip noexport variables so you can e.g. declare a placeholder for the virtual function table in script.
						if( It->ElementSize && Cast<UBoolProperty>(*It) == NULL && !It->HasAnyPropertyFlags(CPF_NoExport) )
						{
							// Keep track of first and last usable property.
							if( !First )
							{
								First = *It;
							}

							Last = *It;

							// If this is not an editor only property update the first and last non-editor props.
							if( !It->IsEditorOnlyProperty() )
							{
								if( !FirstNonEditor )
								{
									FirstNonEditor = *It;
								}
								LastNonEditor = *It;
							}
						}
					}

					// If the first property doesn't match the first non-editor property we know the first non-editor property comes after.  If the first non editor is null though
					// then we know that there are no non-editor properties.  Just wrap the first property in a #if WITH_EDITORONLY_DATA/#endif
					if( First != FirstNonEditor && !FirstNonEditor )
					{
						HeaderText.Logf( TEXT("#if WITH_EDITORONLY_DATA\r\n") );
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*First->GetNameCPP());
						HeaderText.Logf( TEXT("#endif\r\n") );
					}
					// In this case we do have a non-editor property that we need to verify the offset of.
					else if( First != FirstNonEditor && First && FirstNonEditor )
					{
						HeaderText.Logf( TEXT("#if WITH_EDITORONLY_DATA\r\n") );
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*First->GetNameCPP());
						HeaderText.Logf( TEXT("#else\r\n") );
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*FirstNonEditor->GetNameCPP());
						HeaderText.Logf( TEXT("#endif\r\n") );
					}
					else if( First )
					{
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*First->GetNameCPP());
					}
					// If the last and the last non-editor do not match then we know the non-editor property came first.
					// If there isn't a last non-editor then the property list is only editor properties.
					if( Last != LastNonEditor && !LastNonEditor )
					{
						HeaderText.Logf( TEXT("#if WITH_EDITORONLY_DATA\r\n") );
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*Last->GetNameCPP());
						HeaderText.Logf( TEXT("#endif\r\n") );
					}
					// We do have a last non-editor so ensure there's an #else clause.
					else if( Last != LastNonEditor && LastNonEditor )
					{
						HeaderText.Logf( TEXT("#if WITH_EDITORONLY_DATA\r\n") );
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*Last->GetNameCPP());
						if( LastNonEditor != FirstNonEditor )
						{
							HeaderText.Logf( TEXT("#else\r\n") );
							HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*LastNonEditor->GetNameCPP());
						}
						HeaderText.Logf( TEXT("#endif\r\n") );
					}
					else if( Last && Last != First )
					{
						HeaderText.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),NameLookupCPP->GetNameCPP(Class),*Class->GetName(),*Last->GetNameCPP());
					}
				}

				HeaderText.Logf( TEXT("VERIFY_CLASS_SIZE_NODIE(%s)\r\n"), NameLookupCPP->GetNameCPP(Class) );
			}
		}
		HeaderText.Logf( TEXT("#endif // VERIFY_CLASS_SIZES\r\n") ); // #endif // VERIFY_CLASS_SIZES

		HeaderText.Logf( TEXT("#endif // !ENUMS_ONLY\r\n") ); // #endif // !ENUMS_ONLY

		HeaderText.Logf( TEXT("\r\n") );
		HeaderText.Logf( TEXT("#if SUPPORTS_PRAGMA_PACK\r\n") );
		HeaderText.Logf( TEXT("#pragma pack (pop)\r\n") );
		HeaderText.Logf( TEXT("#endif\r\n") );

		// Save the header file;

		// build the full header file out of its pieces
		FString FullHeader = FString::Printf(
			TEXT("%s")
			TEXT("// Split enums from the rest of the header so they can be included earlier\r\n")
			TEXT("// than the rest of the header file by including this file twice with different\r\n")
			TEXT("// #define wrappers. See Engine.h and look at EngineClasses.h for an example.\r\n")
			TEXT("#if !NO_ENUMS && !defined(NAMES_ONLY)\r\n\r\n")
			TEXT("#ifndef INCLUDED_%s_ENUMS\r\n")
			TEXT("#define INCLUDED_%s_ENUMS 1\r\n\r\n")
			TEXT("%s\r\n")
			TEXT("#endif // !INCLUDED_%s_ENUMS\r\n")
			TEXT("#endif // !NO_ENUMS\r\n\r\n")
			TEXT("%s"),
			*PreHeaderText, 
			*HeaderAPI,
			*HeaderAPI,
			*EnumHeaderText, 
			*HeaderAPI,
			*HeaderText);

		if(OriginalHeader.Len() == 0 || appStrcmp(*OriginalHeader, *FullHeader))
		{
			if ( OriginalHeader.Len() > 0 && !bAutoExport
			&&	(GIsSilent || GIsUnattended) )
			{
				warnf(NAME_Error,TEXT("Cannot export %s while in silent/unattended mode."),*HeaderPath);
			}
			else 
			{
				const UBOOL bInitialExport = OriginalHeader.Len() == 0;

				// save the updated version to a tmp file so that the user can see what will be changing
				FString TmpHeaderFilename = GenerateTempHeaderName( HeaderPath, FALSE );

				// delete any existing temp file
				GFileManager->Delete( *TmpHeaderFilename, FALSE, TRUE );
				if ( !appSaveStringToFile(*FullHeader, *TmpHeaderFilename) )
				{
					warnf(NAME_Warning, TEXT("Failed to save header export preview: '%s'"), *TmpHeaderFilename);
				}

				TempHeaderPaths.AddItem(TmpHeaderFilename);
			}
		}
	}

	// Export all changed headers from their temp files to the .h files
	ExportUpdatedHeaders( bAutoExport, InPackage->GetPathName());

	// now export the names for the functions in this package
	// notice we always export this file (as opposed to only exporting if we have any marked names)
	// because there would be no way to know when the file was created otherwise
	if ( bNoCircularDependencyDetected )
	{
		ExportPackageNames();
	}
}

/**
* Create a temp header file name from the header name
*
* @param	CurrentFilename		The filename off of which the current filename will be generated
* @param	bReverseOperation	Get the header from the temp file name instead
*
* @return	The generated string
*/
FString FNativeClassHeaderGenerator::GenerateTempHeaderName( FString CurrentFilename, UBOOL bReverseOperation )
{
	FString TmpEnding = FString(TEXT(".tmp"));
	FString EmptyString = FString(TEXT(""));

	FString HeaderName = ( bReverseOperation)
		? CurrentFilename.Replace(*TmpEnding, *EmptyString)
		: CurrentFilename + TmpEnding;

	return HeaderName;
}

/** 
* Exports the temp header files into the .h files, then deletes the temp files.
* 
* @param	bAutoExport	Automatically checkout of SCC if true
* @param	PackageName	Name of the package being saved
*/
void FNativeClassHeaderGenerator::ExportUpdatedHeaders( UBOOL bAutoExport, FString PackageName )
{
	// Generate a string of the filenames
	FString FilenameList;
	FString Newline = FString(TEXT("\n"));

	if( TempHeaderPaths.Num() > 0 )
	{
		// Generate string of filenames for the message box
		for ( TArray<FString>::TConstIterator FilenameItr(TempHeaderPaths); FilenameItr; ++FilenameItr )
		{
			FString TmpFilename = *FilenameItr;
			FString Filename =GenerateTempHeaderName( TmpFilename, TRUE );
			FilenameList += Newline + Filename;
		}

		// Prompt to export headers
		UBOOL bExportHeaders = bAutoExport || GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("OverwriteMultiple"),TEXT("Core")),*FilenameList));

		for ( TArray<FString>::TConstIterator FilenameItr(TempHeaderPaths); FilenameItr; ++FilenameItr )
		{
			FString TmpFilename = *FilenameItr;
			FString Filename = GenerateTempHeaderName( TmpFilename, TRUE );

			if( bExportHeaders == TRUE )
			{
				if( GFileManager->IsReadOnly( *Filename ) )
				{
				#if HAVE_SCC
					if( bAutoExport || GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("CheckoutPerforce"),TEXT("Core")), *Filename)) )
					{
						// source control object
						FSourceControl::Init();

						// make a fully qualified filename
						FString Dir		= GFileManager->GetCurrentDirectory();
						FString File	= Filename;

						// Attempt to check out the header file
						TArray <FString> FileNames;
						FileNames.AddItem(appConvertRelativePathToFull(*(Dir + File)));
						FSourceControl::CheckOut(NULL, FileNames);

						FSourceControl::Close();
					}
				#endif
				}
				debugf(TEXT("Exported updated C++ header: %s"), *Filename);

				// Save the contents of the temp file into the header
				FString FullHeader;
				appLoadFileToString( FullHeader, *TmpFilename );
				if(!appSaveStringToFile(*FullHeader,*Filename))
				{
					warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("ExportWrite"),TEXT("Core")),*PackageName,*Filename));
				}
			}

			// Delete the temp file
			GFileManager->Delete( *TmpFilename, FALSE, TRUE );
		}
	}
}

/**
 * Exports names for all functions/events/delegates in package.  Names are exported to file using the name <PackageName>Names.h
 */
void FNativeClassHeaderGenerator::ExportPackageNames()
{
	FStringOutputDevice PackageNamesText;

	debugf( TEXT("Autogenerating C++ header: %s"), *PackageNamesHeaderFilename );

	PackageNamesText.Logf(
		TEXT("/*===========================================================================")	LINE_TERMINATOR
		TEXT("    FName C++ declarations exported from UnrealScript.")							LINE_TERMINATOR
		TEXT("    This is automatically generated by the tools.")								LINE_TERMINATOR
		TEXT("    DO NOT modify this manually! Edit the corresponding .uc files instead!")		LINE_TERMINATOR
		TEXT("    Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.")					LINE_TERMINATOR
		TEXT("===========================================================================*/")	LINE_TERMINATOR
		TEXT("#if !ENUMS_ONLY")																	LINE_TERMINATOR
		TEXT("#if SUPPORTS_PRAGMA_PACK")														LINE_TERMINATOR
		TEXT("#pragma pack (push,%i)")															LINE_TERMINATOR
		TEXT("#endif")																			LINE_TERMINATOR
																								LINE_TERMINATOR,
		(BYTE)PROPERTY_ALIGNMENT
		);

	const FString HeaderAPI = API + TEXT("_NAMES");
	PackageNamesText.Logf(
		TEXT("#if !defined(__%s_H__) || defined(NAMES_ONLY)")			LINE_TERMINATOR
																		LINE_TERMINATOR
		TEXT("#ifndef __%s_H__")										LINE_TERMINATOR
		TEXT("#define __%s_H__")										LINE_TERMINATOR
		TEXT("#endif")													LINE_TERMINATOR
																		LINE_TERMINATOR
		TEXT("#ifndef AUTOGENERATE_NAME")								LINE_TERMINATOR
		TEXT("#define DEFINED_NAME_MACRO")								LINE_TERMINATOR
		TEXT("#define AUTOGENERATE_NAME(name) extern FName %s_##name;")	LINE_TERMINATOR
		TEXT("#endif")													LINE_TERMINATOR
																		LINE_TERMINATOR
																		LINE_TERMINATOR,
		*HeaderAPI, *HeaderAPI, *HeaderAPI, *API
		);

	// find all names marked for export and add them to a list (for sorting)
	TArray<FString> Names;
	for( INT NameIndex=0; NameIndex<FName::GetMaxNames(); NameIndex++ )
	{
		FNameEntry* Entry = FName::GetEntry(NameIndex);
		if ( Entry && Entry->HasAnyFlags(RF_TagExp) )
		{
			Names.AddItem(Entry->GetNameString());
		}
	}

	// Autogenerate names (alphabetically sorted).
	Sort<USE_COMPARE_CONSTREF(FString,UMakeCommandlet)>( &Names(0), Names.Num() );
	for( INT NameIndex=0; NameIndex<Names.Num(); NameIndex++ )
	{
		PackageNamesText.Logf( TEXT("AUTOGENERATE_NAME(%s)") LINE_TERMINATOR, *Names(NameIndex) );
	}
	PackageNamesText.Logf(
		LINE_TERMINATOR
		TEXT("#ifdef DEFINED_NAME_MACRO")							LINE_TERMINATOR
		TEXT("#undef DEFINED_NAME_MACRO")							LINE_TERMINATOR
		TEXT("#undef AUTOGENERATE_NAME")							LINE_TERMINATOR
		TEXT("#endif")												LINE_TERMINATOR
		);

	PackageNamesText.Logf(
		LINE_TERMINATOR
		TEXT("#endif\t//HEADER_GUARD")	LINE_TERMINATOR
										LINE_TERMINATOR
		TEXT("#if SUPPORTS_PRAGMA_PACK")LINE_TERMINATOR
		TEXT("#pragma pack (pop)")		LINE_TERMINATOR
		TEXT("#endif")					LINE_TERMINATOR
										LINE_TERMINATOR
		TEXT("#endif\t//\t!ENUMS_ONLY")	LINE_TERMINATOR
										LINE_TERMINATOR
		);

	// Load the original header file into memory
	FString HeaderFileLocation = GEditor->EditPackagesInPath * Package->GetName() * TEXT("Inc");
	FString	HeaderPath = HeaderFileLocation * PackageNamesHeaderFilename;
	appLoadFileToString(OriginalNamesHeader,*HeaderPath);

	if(OriginalNamesHeader.Len() == 0 || appStrcmp(*OriginalNamesHeader, *PackageNamesText))
	{
		if ( OriginalNamesHeader.Len() > 0 && !bAutoExport
		&&	(GIsSilent || GIsUnattended) )
		{
			warnf(NAME_Error,TEXT("Cannot export %s while in silent/unattended mode."), *HeaderPath);
		}
		else 
		{
			UBOOL bExportUpdatedHeader = FALSE;
			const UBOOL bInitialExport = OriginalNamesHeader.Len() == 0;
			bExportUpdatedHeader = bInitialExport || bAutoExport;

			if ( bExportUpdatedHeader == FALSE )
			{
				// display a prompt to the user that this header needs to be updated.

				// save the updated version to a tmp file so that the user can see what will be changing
				FString TmpHeaderFilename = HeaderPath + TEXT(".tmp");

				// delete any existing temp file
				GFileManager->Delete( *TmpHeaderFilename, FALSE, TRUE );
				if ( !appSaveStringToFile(*PackageNamesText, *TmpHeaderFilename) )
				{
					warnf(NAME_Warning, TEXT("Failed to save header export preview: '%s'"), *TmpHeaderFilename);
				}

				bExportUpdatedHeader = GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("Overwrite"),TEXT("Core")),*HeaderPath));

				// delete the tmp file we created
				GFileManager->Delete( *TmpHeaderFilename, FALSE, TRUE );
			}

			if( bExportUpdatedHeader )
			{
				if( GFileManager->IsReadOnly( *HeaderPath ) )
				{
#if HAVE_SCC
					if( bAutoExport || GWarn->YesNof(LocalizeSecure(LocalizeQuery(TEXT("CheckoutPerforce"),TEXT("Core")), *HeaderPath)) )
					{
						// source control object
						FSourceControl::Init();

						// make a fully qualified filename
						FString Dir		= GFileManager->GetCurrentDirectory();
						FString File	= HeaderPath;

						// Attempt to check out the header file
						TArray <FString> FileNames;
						FileNames.AddItem(appConvertRelativePathToFull(*(Dir + File)));
						FSourceControl::CheckOut(NULL, FileNames);

						FSourceControl::Close();
					}
#endif
				}
				
				debugf(TEXT("Exported updated C++ header: %s"), *HeaderPath);
				if ( !appSaveStringToFile(*PackageNamesText,*HeaderPath) )
				{
					warnf( NAME_Error, LocalizeSecure(LocalizeError(TEXT("ExportWrite"),TEXT("Core")),*Package->GetPathName(),*HeaderPath));
				}
			}
		}
	}
}

/**
 * Returns the number of seconds since the specified directory has been modified,
 * including changes to any files contained within that directory.
 * 
 * @param	PackageScriptDirectoryPath	the path to the Classes subdirectory for the package being checked.
										(i.e. ../Development/Src/SomePackage/Classes/)
 *
 * @return	the number of seconds since the directory or any of its files has been modified.
 *			Values less than 0 indicate that the directory doesn't exist.
 */
static DOUBLE GetScriptDirectoryAgeInSeconds( const FString& PackageScriptDirectoryPath )
{
	// No script will ever be older than 100 years
	DOUBLE PackageDirectoryAgeInSeconds = 60.0 * 60.0 * 24.0 * 365.25 * 100.0;

	// Modifying a file in a folder does not touch the timestamp of the parent folder
	// Adding or deleting a file will touch the timestamp.
	//
	// Perforce always syncs via temporary files (i.e. creates and destroys files)
	// so the folder's timestamp is always updated if a script file gets updated via Perforce.
	//
	// This makes checking the folder's timestamp not work if you sync builds back and forth.
	// However, if a script file is deleted, it will not be detected
	const FString ClassesWildcard = PackageScriptDirectoryPath * TEXT("*.uc?");

	// grab the list of files matching the wildcard path
	TArray<FString> ClassesFiles;
	GFileManager->FindFiles( ClassesFiles, *ClassesWildcard, TRUE, FALSE );

	// check the timestamp for each .uc? file
	for(INT ClassIndex = 0; ClassIndex < ClassesFiles.Num(); ClassIndex++ )
	{
		// expanded name of this .uc? file
		const FString ClassName = PackageScriptDirectoryPath * ClassesFiles(ClassIndex);
		const DOUBLE ScriptFileAgeInSeconds = GFileManager->GetFileAgeSeconds(*ClassName);

		if( GIsBuildMachine )
		{
			warnf( TEXT( " ... against source script: %s (age %.1fs)" ), *ClassName, ScriptFileAgeInSeconds );
		}

		if( ScriptFileAgeInSeconds < PackageDirectoryAgeInSeconds )
		{
			// this script file's 'last modified' stamp is newer than the one for the
			// directory - some text editors save the file in such a way that the directory
			// modification timestamp isn't updated.
			PackageDirectoryAgeInSeconds = ScriptFileAgeInSeconds;
		}
	}

	return PackageDirectoryAgeInSeconds;
}

static UBOOL IsScriptManifestOutOfDate_Worker(const FString& ScriptPackagePath)
{
	UBOOL bFoundOutOfDateManifest = FALSE;

	const DOUBLE SourceAgeInSeconds	= GetScriptDirectoryAgeInSeconds(ScriptPackagePath);

	const FString ManifestFile			= appScriptManifestFile();
	const DOUBLE ManifestAgeInSeconds	= GFileManager->GetFileAgeSeconds(*ManifestFile);

	//if a manifest never existed before
	if( ManifestAgeInSeconds <= 0 )
	{
		// The compiled script package doesn't exist.
		if(  !GIsUCCMake || GIsUnattended )
		{
			warnf( NAME_Error, TEXT("Manifest doesn't exist: %s"), *ManifestFile );
		}
		bFoundOutOfDateManifest = TRUE;
	}
	//if the manifest is older than the script packages
	else if ( SourceAgeInSeconds < ManifestAgeInSeconds )
	{
		// The source script file is newer than the compiled script package.
		if(  !GIsUCCMake || GIsUnattended )
		{
			warnf( NAME_Error, TEXT("Manifest is out of date!") );
		}
		bFoundOutOfDateManifest = TRUE;
	}
	
	if (!bFoundOutOfDateManifest)
	{
		//file exists, so let's crack it open and make sure it has the proper version information
		FString Text;
		if( !appLoadFileToString( Text, *ManifestFile, GFileManager, 0 ) )
		{
			warnf( TEXT("Failed to open script manifest") );
			return TRUE;
		}
		//read version index with " " as a delimiter
		INT VersionInManifest = appAtoi(*Text);
		if (VersionInManifest != ClassManifestDefs::ManifestVersion)
		{
			bFoundOutOfDateManifest = TRUE;
		}
	}

	return bFoundOutOfDateManifest;
}

static UBOOL AreScriptPackagesOutOfDate_Worker(const TArray<FString>& ScriptPackageNames,
											   const FString& ScriptSourcePath,
											   const FString& ScriptPackagePath,
											   UBOOL bIsShippingPackage)
{
	UBOOL bFoundOutOfDatePackage = FALSE;

	// For each script package, see if any of its scripts are newer
	for( INT ScriptPackageIndex = 0 ; ScriptPackageIndex < ScriptPackageNames.Num() ; ++ScriptPackageIndex )
	{
		// The compiled script package file.
		const FString PackageFile			= ScriptPackagePath * ScriptPackageNames(ScriptPackageIndex) + TEXT(".u");
		const DOUBLE PackageAgeInSeconds	= GFileManager->GetFileAgeSeconds(*PackageFile);

		// If the script package is read-only, we can't recompile it so just skip it.
		if (!GIsUCCMake && (PackageAgeInSeconds > 0) && GFileManager->IsReadOnly(*PackageFile))
		{
			continue;
		}

		if( GIsBuildMachine )
		{
			warnf( TEXT( "Checking script package: %s (age %.1fs)" ), *PackageFile, PackageAgeInSeconds );
		}

		// The script source file.
		const FString SourceFile = ScriptSourcePath * ScriptPackageNames(ScriptPackageIndex) * TEXT("Classes");
		const DOUBLE SourceAgeInSeconds	= GetScriptDirectoryAgeInSeconds(SourceFile);

		// Does the script source file exist?
		if ( SourceAgeInSeconds > 0 )
		{
			if( PackageAgeInSeconds <= 0 )
			{
				// The compiled script package doesn't exist.
				if( GIsBuildMachine )
				{
					warnf( NAME_Error, TEXT("Script package doesn't exist: %s"), *PackageFile );
				}
				bFoundOutOfDatePackage = TRUE;
			}
			else if ( SourceAgeInSeconds <= PackageAgeInSeconds )
			{
				// The source script file is newer than the compiled script package.
				if( GIsBuildMachine )
				{
					warnf( NAME_Error, TEXT("Script package is out of date!") );
					warnf( NAME_Error, TEXT("    Source: %s, Package: %s, Time: %.1fs <= %.1fs"), *SourceFile, *PackageFile, SourceAgeInSeconds, PackageAgeInSeconds );
				}
				bFoundOutOfDatePackage = TRUE;
			}
			else if( GIsBuildMachine )
			{
				warnf( TEXT( " ... package is up to date" ) );
			}
		}
	}

	return bFoundOutOfDatePackage;
}

/** Check command line options to infer running in "user mode" */
inline UBOOL IsRunningAsUser()
{
	return ParseParam(appCmdLine(), TEXT("user"))
			|| ParseParam(appCmdLine(), TEXT("SEEKFREELOADING"))
			|| ParseParam(appCmdLine(), TEXT("SEEKFREELOADINGPCCONSOLE"))
			|| ParseParam(appCmdLine(), TEXT("SEEKFREELOADINGPCCSERVER"))
			|| ParseParam(appCmdLine(), TEXT("installed"));
}

extern void StripUnusedPackagesFromList(TArray<FString>& PackageList, const FString& ScriptSourcePath);

/** @return		TRUE if if scripts need to be recompiled. */
UBOOL AreScriptPackagesOutOfDate()
{
	UBOOL bResult = FALSE;

#if !SHIPPING_PC_GAME || UDK
	// If running in 'user mode', don't look to compile shipping script.
	const UBOOL bRunningAsUser = IsRunningAsUser();
	if ( !bRunningAsUser )
	{
		// Look for uncompiled shipping script.
		FString ShippingScriptSourcePath;
		verify(GConfig->GetString( TEXT("UnrealEd.EditorEngine"), TEXT("EditPackagesInPath"), ShippingScriptSourcePath, GEngineIni ));

		// Get the list of script filenames from .ini
		TArray<FString> ShippingScriptPackageNames;
		FConfigSection* Sec = GConfig->GetSectionPrivate( TEXT("UnrealEd.EditorEngine"), 0, 1, GEngineIni );
		Sec->MultiFind( FName(TEXT("EditPackages")), ShippingScriptPackageNames );

		// clean up the package list
		StripUnusedPackagesFromList(ShippingScriptPackageNames, ShippingScriptSourcePath);

		// Don't need to check SeekFreePCPaths here because this code won't be run in user-mode.
		const FString ScriptPackagePath = appScriptOutputDir();
		bResult |= AreScriptPackagesOutOfDate_Worker( ShippingScriptPackageNames, ShippingScriptSourcePath, ScriptPackagePath, TRUE );

		// get the list of extra mod packages from the .ini (for UDK)
		TArray<FString> ModEditPackages;
		GConfig->GetArray(TEXT("UnrealEd.EditorEngine"), TEXT("ModEditPackages"), ModEditPackages, GEngineIni);
		bResult |= AreScriptPackagesOutOfDate_Worker( ModEditPackages, ShippingScriptSourcePath, ScriptPackagePath, TRUE );

		bResult |= IsScriptManifestOutOfDate_Worker( ScriptPackagePath );
	}
#endif
	// Look for uncompiled mod script.
	// These are loaded from the editor ini because it's not signed in cooked pc builds.
	const TCHAR* ModIni = GEditorIni;

	//NOTE - The file manifest has not been fully tested for Mod support.  The manifest should be generated in the mod output directory
	//and the game should default to loading that (which should also include all native classes).

	// Get the list of script filenames from .ini
	TArray<FString> ModScriptPackageNames;
	FConfigSection* ModSec = GConfig->GetSectionPrivate( TEXT("ModPackages"), 0, 1, ModIni );
	if( ModSec != NULL )
	{
		ModSec->MultiFind( FName(TEXT("ModPackages")), ModScriptPackageNames );

		FString ModScriptSourcePath;
		if( GConfig->GetString( TEXT("ModPackages"), TEXT("ModPackagesInPath"), ModScriptSourcePath, ModIni ) )
		{
			FString ModOutputDir;
			if( GConfig->GetString( TEXT("ModPackages"), TEXT("ModOutputDir"), ModOutputDir, GEditorIni ) )
			{
				bResult |= AreScriptPackagesOutOfDate_Worker( ModScriptPackageNames, ModScriptSourcePath, ModOutputDir, FALSE );
			}
		}
	}
	return bResult;
}

/*-----------------------------------------------------------------------------
	UMakeCommandlet.
-----------------------------------------------------------------------------*/

/**
 * Allows commandlets to override the default behavior and create a custom engine class for the commandlet. If
 * the commandlet implements this function, it should fully initialize the UEngine object as well.  Commandlets
 * should indicate that they have implemented this function by assigning the custom UEngine to GEngine.
 */
void UMakeCommandlet::CreateCustomEngine()
{
	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_DisallowFiles, NULL );

	// must do this here so that the engine object that we create on the next line receives the correct property values
	UObject* DefaultEngine = EditorEngineClass->GetDefaultObject(TRUE);

	// ConditionalLink() won't call LoadConfig() if GIsUCCMake is true, so we must do it here so that the editor
	// engine has EditPackages
	EditorEngineClass->ConditionalLink();
	DefaultEngine->LoadConfig();

	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->InitEditor();
}

/** Stores a script package name and a flag indicating whether the package is a shipping package. */
class FPackageShipInfo
{
public:
	FPackageShipInfo(const TCHAR* InPackage, UBOOL bInIsShippingPackage)
		:	Package( InPackage )
		,	bIsShippingPackage( bInIsShippingPackage )
	{}

	/** Script package name. */
	FString Package;
	/** TRUE if this is a shipped game package, FALSE if it's a mod package. */
	UBOOL	bIsShippingPackage;
};

INT UMakeCommandlet::Main( const FString& Params )
{
#if !SUPPORT_NAME_FLAGS
#error Please set SUPPORT_NAME_FLAGS to 1 in UnBuild.h if you want to support compiling script
#endif

	check(GIsUCCMake);

	// TRUE if 'make' is being run in user mode (ie compile mod script, not shipping game script).
#if SHIPPING_PC_GAME && !UDK
	const UBOOL bIsRunningAsUser = TRUE;
#else
	const UBOOL bIsRunningAsUser = IsRunningAsUser();
#endif

	UBOOL bHeaders = FALSE;
	if( ParseParam( appCmdLine(), TEXT("HEADERS") ) )
	{
		bHeaders = TRUE;
	}	

	// Can't rely on properties being serialized so we need to manually set them via the .ini config system.
	verify(GConfig->GetString( TEXT("UnrealEd.EditorEngine"), TEXT("FRScriptOutputPath"), GEditor->FRScriptOutputPath, GEngineIni ));
	verify(GConfig->GetString( TEXT("UnrealEd.EditorEngine"), TEXT("EditPackagesOutPath"), GEditor->EditPackagesOutPath, GEngineIni ));
	verify(GConfig->GetString( TEXT("UnrealEd.EditorEngine"), TEXT("EditPackagesInPath"), GEditor->EditPackagesInPath, GEngineIni ));
	verify(GConfig->GetArray( TEXT("UnrealEd.EditorEngine"), TEXT("EditPackages"), GEditor->EditPackages, GEngineIni ));

	// clean up the package list
	StripUnusedPackagesFromList(GEditor->EditPackages, GEditor->EditPackagesInPath);

	// get the list of extra mod packages from the .ini (for UDK)
	TArray<FString> ModEditPackages;
	GConfig->GetArray(TEXT("UnrealEd.EditorEngine"), TEXT("ModEditPackages"), ModEditPackages, GEngineIni);
	GEditor->EditPackages.Append(ModEditPackages);

	// get a list of packages to skip warning about it the directory doesn't exist
	TArray<FString> OptionalPackages;
	GConfig->GetArray( TEXT("UnrealEd.EditorEngine"), TEXT("OptionalScriptPackages"), OptionalPackages, GEngineIni );

	FString ShippingOutputDir;
	if ( bIsRunningAsUser )
	{
		USystem* DefaultSystemObject = USystem::StaticClass()->GetDefaultObject<USystem>();
		ShippingOutputDir = DefaultSystemObject->SeekFreePCPaths(0);
	}
	else
	{
		ShippingOutputDir = appScriptOutputDir();
	}

	// Look to the editor ini for mod script because the editor ini is not signed in cooked pc builds.
	const TCHAR* ModIni = GEditorIni;
	FString ModPackagesInPath;
	FString ModOutputDir;
	TArray<FString> ModPackages;
	verify(GConfig->GetString( TEXT("ModPackages"), TEXT("ModPackagesInPath"), ModPackagesInPath, ModIni ) || !bIsRunningAsUser);
	verify(GConfig->GetString( TEXT("ModPackages"), TEXT("ModOutputDir"), ModOutputDir, GEditorIni ) || !bIsRunningAsUser);
	GConfig->GetArray( TEXT("ModPackages"), TEXT("ModPackages"), ModPackages, ModIni );

	// PackagesAndPaths stores a list of all shipping packages, followed by a list of all mod packages.
	TArray<FPackageShipInfo> PackagesAndPaths;
	for ( INT PackageIndex = 0 ; PackageIndex < GEditor->EditPackages.Num() ; ++PackageIndex )
	{
		PackagesAndPaths.AddItem(FPackageShipInfo(*GEditor->EditPackages(PackageIndex),TRUE));
	}

	// Stores the index into PackagesAndPaths of the first mod package.
	const INT FirstModPackageIndex = PackagesAndPaths.Num();

	for ( INT PackageIndex = 0 ; PackageIndex < ModPackages.Num() ; ++PackageIndex )
	{
		PackagesAndPaths.AddItem(FPackageShipInfo(*ModPackages(PackageIndex),FALSE));
	}

	// Store the script package directory and package list that are deletable.
	const FString DeletePackagesDir = bIsRunningAsUser ? ModOutputDir : ShippingOutputDir;

	// When compiling in 'user' mode, only mod packages can be deleted.
	const TArray<FString>& DeletePackages = bIsRunningAsUser ? ModPackages : GEditor->EditPackages;

	// indicates that we should delete the current package -
	// used to ensure that dependent packages are always recompiled
	UBOOL bDeletePackage = FALSE;

	// If -full was specified on the command-line, we want to wipe all .u files. Listing unreferenced functions also
	// relies on a full recompile.
	if( ParseParam(appCmdLine(), TEXT("FULL")) || ParseParam(appCmdLine(),TEXT("LISTUNREFERENCED")) )
	{
		bDeletePackage = TRUE;
	}

	if(!bDeletePackage && !AreScriptPackagesOutOfDate())
	{
		warnf(TEXT("No scripts need recompiling."));
		GIsRequestingExit = TRUE;
		return 0;
	}

	NameLookupCPP				= new FNameLookupCPP();

	FCompilerMetadataManager*	ScriptHelper = GScriptHelper = new FCompilerMetadataManager();

	// indicates that we should only never delete successive .u files in the dependency chain
	const UBOOL bNoDelete = ParseParam(appCmdLine(), TEXT("nodelete"));

	if(bDeletePackage)
	{
		DeleteEditPackages( DeletePackagesDir, DeletePackages, 0 );
	}

	// Load classes for editing.
	UClassFactoryUC* ClassFactory = new UClassFactoryUC;
	UBOOL Success = TRUE;

	UBOOL bDebugMode = ParseParam(appCmdLine(), TEXT("DEBUG"));
	for( INT PackageIndex=0; PackageIndex<PackagesAndPaths.Num(); PackageIndex++ )
	{
		// Try to load class.
		const UBOOL bIsShippingPackage = PackagesAndPaths(PackageIndex).bIsShippingPackage;
		const TCHAR* Pkg = *PackagesAndPaths( PackageIndex ).Package;
		const FString& PackageInPath = bIsShippingPackage ? GEditor->EditPackagesInPath : ModPackagesInPath;
		const FString Filename = FString::Printf(TEXT("%s") PATH_SEPARATOR TEXT("%s.u"), bIsShippingPackage ? *ShippingOutputDir : *ModOutputDir, Pkg );
		GWarn->Log(NAME_Heading, FString::Printf(TEXT("%s - %s"), Pkg, bDebugMode ? TEXT("Debug") : TEXT("Release"))); //DEBUGGER

		// Check whether this package needs to be recompiled because a class is newer than the package.
		//@warning: This won't detect changes to resources being compiled into packages.
		UBOOL bFileExists = GFileManager->FileSize(*Filename) >= 0;
		if( bFileExists )
		{
			// Can't recompile shipping packages when running as user.
			const UBOOL bCanRecompilePackage = !bIsShippingPackage || !bIsRunningAsUser;
			if ( bCanRecompilePackage )
			{
				const DOUBLE		PackageBinaryAgeInSeconds	= GFileManager->GetFileAgeSeconds(*Filename);
				const FString		PackageDirectory = PackageInPath * Pkg * TEXT("Classes");
				const DOUBLE		PackageDirectoryAgeInSeconds = GetScriptDirectoryAgeInSeconds(PackageDirectory);

				UBOOL bNeedsRecompile = FALSE;
				if (PackageDirectoryAgeInSeconds > 0 && PackageDirectoryAgeInSeconds <= PackageBinaryAgeInSeconds)
				{
					warnf(TEXT("Package %s changed, recompiling"), Pkg);
					bNeedsRecompile = TRUE;
				}
				else
				{
					BeginLoad();
					UPackage* TempPackage = CreatePackage(NULL, NULL);
					ULinkerLoad* Linker = GetPackageLinker(TempPackage, *Filename, LOAD_NoVerify, NULL, NULL);
					EndLoad();
					if (Linker != NULL && ((Linker->LinkerRoot->PackageFlags & PKG_ContainsDebugInfo) ? TRUE : FALSE) != bDebugMode)
					{
						if (Linker->LinkerRoot->PackageFlags & PKG_ContainsDebugInfo)
						{
							warnf(TEXT("Package %s was compiled in debug mode, recompiling in release"), Pkg);
						}
						else
						{
							warnf(TEXT("Package %s was compiled in release mode, recompiling in debug"), Pkg);
						}
						bNeedsRecompile = TRUE;
					}
					else if (Linker->LinkerRoot->PackageFlags & PKG_StrippedSource)
					{
						warnf(TEXT("Package %s has stripped source, recompiling"), Pkg);
						bNeedsRecompile = TRUE;
					}
					if (Linker != NULL)
					{
						ResetLoaders(TempPackage);
					}
				}

				if (bNeedsRecompile)
				{
					bFileExists = FALSE;
					INT DeleteCount = INDEX_NONE;
					if ( bNoDelete )
					{
						// in this case, we only want to delete one package
						DeleteCount = 1;
					}
					else
					{
						bDeletePackage = TRUE;
					}

					// Delete package and all the ones following in EditPackages. This is required in certain cases so we rather play safe.
					INT PackageIndexToDelete = PackageIndex;
					if ( bIsRunningAsUser )
					{
						PackageIndexToDelete -= FirstModPackageIndex;
					}
					DeleteEditPackages(DeletePackagesDir,DeletePackages,PackageIndexToDelete,DeleteCount);
				}
			}
		}
		else
		{
			// Can't find a shipping package!
			if ( bIsShippingPackage && bIsRunningAsUser )
			{
				appErrorf( TEXT("Missing shipping script package %s"), Pkg );
			}

			if ( !bNoDelete )
			{
				// if we've encountered a missing package, and we haven't already deleted all
				// dependent packages, do that now
				if ( !bDeletePackage )
				{
					INT PackageIndexToDelete = PackageIndex;
					if ( bIsRunningAsUser )
					{
						PackageIndexToDelete -= FirstModPackageIndex;
					}
					DeleteEditPackages(DeletePackagesDir,DeletePackages,PackageIndexToDelete);
				}

				bDeletePackage = TRUE;
			}
		}

		if( GWarn->Errors.Num() > 0 )
		{
			break;
		}

		// disable loading of objects outside of this package (or more exactly, objects which aren't UFields, CDO, or templates)
		GUglyHackFlags |= HACK_VerifyObjectReferencesOnly;
		UPackage* Package = (bDeletePackage && (!bIsRunningAsUser || !bIsShippingPackage))
			? NULL 
			: Cast<UPackage>(LoadPackage( NULL, *Filename, LOAD_NoWarn|LOAD_NoVerify ));
		GUglyHackFlags &= ~HACK_VerifyObjectReferencesOnly;

		const UBOOL bNeedsCompile = Package == NULL;

		// if we couldn't load the package, but the .u file exists, then we have some other problem that is preventing
		// the .u file from being loaded - in this case we don't want to attempt to recompile the package
		if ( bNeedsCompile && bFileExists )
		{
			warnf(NAME_Error, TEXT("Could not load existing package file '%s'.  Check the log file for more information."), *Filename);
			Success = FALSE;
		}

		if ( Success )
		{
			// Create package.
			const FString IniName = PackageInPath * Pkg * TEXT("Classes") * Pkg + TEXT(".upkg");

			if ( bNeedsCompile )
			{
				// Rebuild the class from its directory.
				const FString Spec = PackageInPath * Pkg * TEXT("Classes") * TEXT("*.uc");

				TArray<FString> Files;
				GFileManager->FindFiles( Files, *Spec, 1, 0 );
				if ( Files.Num() > 0 )
				{
					GWarn->Log( TEXT("Analyzing...") );
					Package = CreatePackage( NULL, Pkg );

					// set some package flags for indicating that this package contains script
					Package->PackageFlags |= PKG_ContainsScript;

					if (bDebugMode)
					{
						Package->PackageFlags |= PKG_ContainsDebugInfo;
					}

					// Try reading from package's .ini file.
					Package->PackageFlags &= ~(PKG_AllowDownload|PKG_ClientOptional|PKG_ServerSideOnly);

					UBOOL B=0;
					// the default for AllowDownload is TRUE
					if (!GConfig->GetBool(TEXT("Flags"), TEXT("AllowDownload"), B, *IniName) || B)
					{
						Package->PackageFlags |= PKG_AllowDownload;
					}
					// the default for ClientOptional is FALSE
					if (GConfig->GetBool(TEXT("Flags"), TEXT("ClientOptional"), B, *IniName) && B)
					{
						Package->PackageFlags |= PKG_ClientOptional;
					}
					// the default for ServerSideOnly is FALSE
					if (GConfig->GetBool(TEXT("Flags"), TEXT("ServerSideOnly"), B, *IniName) && B)
					{
						Package->PackageFlags |= PKG_ServerSideOnly;
					}

					Package->PackageFlags |= PKG_Compiling;

					// Make script compilation deterministic by sorting .uc files by name.
					Sort<USE_COMPARE_CONSTREF(FString,UMakeCommandlet)>( &Files(0), Files.Num() );

					for( INT i=0; i<Files.Num() && Success; i++ )
					{
						// Import class.
						const FString Filename = PackageInPath * Pkg * TEXT("Classes") * Files(i);
						const FString ClassName = Files(i).LeftChop(3);
						Success = ImportObject<UClass>( Package, *ClassName, RF_Public|RF_Standalone, *Filename, NULL, ClassFactory ) != NULL;
					}
				}
				else
				{
					// look to see if this package is optional; if so, skip the warning
					INT Index;
					if (!OptionalPackages.FindItem(Pkg, Index))
					{
						FString DisplayFilename( GFileManager->ConvertToAbsolutePath(*Spec) );
						if ( !bIsShippingPackage )
						{
							DisplayFilename = GFileManager->ConvertAbsolutePathToUserPath(*DisplayFilename);
						}
						warnf(NAME_Warning,TEXT("Can't find files matching %s"), *DisplayFilename );
					}
					continue;
				}
			}
			else
			{
				// if the package isn't recompiled we still need to load the macros
				FMacroProcessingFilter DummyMacroFilter(Pkg,TEXT("NULL"));
				DummyMacroFilter.ProcessGlobalInclude();
			}

			if ( Success )
			{
				// Verify that all script declared superclasses exist.
				for( TObjectIterator<UClass> ItC; ItC; ++ItC )
				{
					const UClass* ScriptClass = *ItC;
					if( ScriptClass->ScriptText && ScriptClass->GetSuperClass() )
					{
						if( !ScriptClass->GetSuperClass()->ScriptText )
						{
							warnf(NAME_Error, TEXT("Superclass %s of class %s not found"), *ScriptClass->GetSuperClass()->GetName(), *ScriptClass->GetName());
							Success = FALSE;
						}
					}
				}

				if (Success)
				{
					// Bootstrap-recompile changed scripts.
					GEditor->Bootstrapping = 1;
					GEditor->ParentContext = Package;

					GUglyHackFlags |= HACK_VerifyObjectReferencesOnly;
					Success = GEditor->MakeScripts( NULL, GWarn, 0, TRUE, TRUE, Package, !bNeedsCompile, bHeaders );
					GUglyHackFlags &= ~HACK_VerifyObjectReferencesOnly;
					GEditor->ParentContext = NULL;
					GEditor->Bootstrapping = 0;
				}
			}
		}

        if( !Success )
        {
            warnf ( TEXT("Compile aborted due to errors.") );
            break;
        }

		if ( bNeedsCompile && !bHeaders )
		{
			// Save package.
			ULinkerLoad* Conform = NULL;
			const FFilename OldFilename = appGameDir() * TEXT("ScriptOriginal") * Pkg + TEXT(".u");
#if !SHIPPING_PC_GAME
			if( !ParseParam(appCmdLine(),TEXT("NOCONFORM")) && GFileManager->FileSize( *OldFilename ) != INDEX_NONE )
			{
				// check the default location for script packages to conform against, if a like-named package exists in the
				// auto-conform directory, use that as the conform package
				BeginLoad();
				UPackage* OldPackage = CreatePackage( NULL, *(OldFilename.GetBaseFilename() + TEXT("_OLD")) );
				Conform = UObject::GetPackageLinker( OldPackage, *OldFilename, LOAD_Verify|LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
				EndLoad();
				if( Conform )
				{
					debugf( TEXT("Conforming: %s"), Pkg );
				}

				// Calling GetPackageLinker with an alternate location for a known package will cause that package's entry in the file cache
				// to point to the alternate location from here on out.  We don't want this (otherwise subsequence attempts to load that package
				// will result in loading the alternate version of the package) so we restore it now
				GPackageFileCache->CachePackage(*Filename, TRUE, FALSE);
			}
#endif

			Package->PackageFlags &= ~PKG_Compiling;

			// write a message indicating that we have finished the "compilation" phase and are beginning the "saving" part, so that crashes in SavePackage aren't
			// mistakenly assumed to be related to script compilation.

			FString DisplayFilename( GFileManager->ConvertToAbsolutePath(*Filename) );
			if ( !bIsShippingPackage )
			{
				// For mod packages, make sure to display the true location of the saved script package.
				DisplayFilename = GFileManager->ConvertAbsolutePathToUserPath(*DisplayFilename);
			}

			warnf(TEXT("Scripts successfully compiled - saving package '%s'"), *DisplayFilename);

			if( ParseParam( appCmdLine(), TEXT( "STRIPSOURCE" ) ) )
			{
				// Strip source text from the package before saving
				for( TObjectIterator<UStruct> It; It; ++It )
				{
					if( It->GetOutermost() == Package && It->ScriptText )
					{
						warnf(NAME_DevCompile, TEXT("  Stripping source code from struct %s"), *It->GetName() );
						It->ScriptText->Text = FString( TEXT( " " ) );
						It->ScriptText->Pos = 0;
						It->ScriptText->Top = 0;
					}

					if( It->GetOutermost() == Package && It->CppText )
					{
						warnf(NAME_DevCompile, TEXT("  Stripping cpptext from struct %s"), *It->GetName() );
						It->CppText->Text = FString( TEXT( " " ) );
						It->CppText->Pos = 0;
						It->CppText->Top = 0;
					}
				}

				Package->PackageFlags |= PKG_StrippedSource;
			}
			else
			{// Remove strip source flag just to be careful
				Package->ClearFlags( PKG_StrippedSource );
			}

			if (!SavePackage(Package, NULL, RF_Standalone, *Filename, GWarn, Conform))
			{
				warnf(TEXT("Package saving failed."));
				break;
			}
		}

		// Avoid TArray slack for meta data.
		GScriptHelper->Shrink();
	}

// DB: No more 'publish by copy'!
#if 0
	if ( bIsRunningAsUser )
	{
		if( !ParseParam(appCmdLine(),TEXT("noautopublish")) )
		{
			// Shell out a copy commandline.
			const FString UserGameDir = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*appGameDir()));
			// The /d switch means 'only copy those files whose source time is newer than the destiation time'.
			// So, we'll only be copying newly-created script packages.
			const FString Command = FString::Printf(TEXT("xcopy /d /s /i \"%sUnpublished\" \"%sPublished\""), *UserGameDir, *UserGameDir);
			system(TCHAR_TO_ANSI(*Command));
		}
	}
#endif

	if (!bHeaders)
	{
		// if we successfully compiled scripts, perform any post-compilation steps
		if (Success)
		{
			//build manifest might fail
			Success = BuildManifest();
		}
		if (Success)
		{
			GEditor->PostScriptCompile();
		}
		if (ParseParam(appCmdLine(),TEXT("LISTUNREFERENCED")))
		{
			warnf( TEXT("Checking for unreferenced functions...") );
			INT UnrefCount = 0;
			for (TObjectIterator<UFunction> It; It; ++It)
			{
				UFunction *Func = *It;
				// ignore natives/events/delegates
				if ( Func->GetOwnerClass()->HasAnyClassFlags(CLASS_Interface) || Func->HasAnyFunctionFlags(FUNC_Event|FUNC_Native|FUNC_Delegate|FUNC_Operator|FUNC_PreOperator|FUNC_Exec) )
				{
					continue;
				}
				INT *EmitCnt = FuncEmitCountMap.Find(Func);
				if (EmitCnt == NULL)
				{
					//debugf(TEXT("- function %s not directly referenced, checking parents"),*Func->GetPathName());
					// check to see if this function's parents are referenced
					UBOOL bFoundRef = FALSE;
					UFunction *SearchFunc = Func;
					UFunction *SuperFunc = NULL;
					do
					{
						// try the direct parent
						SuperFunc = SearchFunc->GetSuperFunction();
						if (SuperFunc == NULL)
						{
							// otherwise look up the state/class tree
							SuperFunc = UAnalyzeScriptCommandlet::FindSuperFunction(SearchFunc);
						}
						if (SuperFunc != NULL)
						{
							if ((SuperFunc->FunctionFlags & FUNC_Native) != 0 ||
								(SuperFunc->FunctionFlags & FUNC_Event) != 0 ||
								(SuperFunc->FunctionFlags & FUNC_Exec) != 0)
							{
							}
							//debugf(TEXT("-+ checking parent %s of %s"),*SuperFunc->GetPathName(),*SearchFunc->GetPathName());
							EmitCnt = FuncEmitCountMap.Find(SuperFunc);
							if (EmitCnt != NULL)
							{
								bFoundRef = TRUE;
								//debugf(TEXT("-+ parent is ref'd!!!"));
								break;
							}
						}
						SearchFunc = SuperFunc;
					} while (SuperFunc != NULL);
					if (!bFoundRef)
					{
						UnrefCount++;
						warnf( TEXT("- function %s was never referenced"), *Func->GetPathName());
					}
				}
			}
			warnf( TEXT("%d unreferenced functions found"), UnrefCount);
		}
	}
	delete NameLookupCPP;
	NameLookupCPP = NULL;

	delete ScriptHelper;
	ScriptHelper = NULL;

	delete FMacroProcessingFilter::GlobalSymbols;
	FMacroProcessingFilter::GlobalSymbols = NULL;

	GEditor = NULL;
	GIsRequestingExit = TRUE;

	return 0;
}

/**
 * Helper class for building the manifest
 */
struct ClassManifestDesc
{
	FString ClassName;
	FString FullInheritanceName;
	INT Depth;

	UClass* Class;
};

IMPLEMENT_COMPARE_CONSTREF( ClassManifestDesc, MakeCommandlet, { return appStricmp( *(A.FullInheritanceName), *(B.FullInheritanceName) ); } )

/**
 * Builds a text manifest of the class hierarchy to be used instead of loading all editor packages at start up
 */
UBOOL UMakeCommandlet::BuildManifest(void)
{
	FString ManifestFileName = appScriptManifestFile();
	INT Flags = FILEWRITE_EvenIfReadOnly;

	// and create the actual archive
	FArchive* ManifestFilePtr = GFileManager->CreateFileWriter(*ManifestFileName, Flags, GWarn);
	if (!ManifestFilePtr)
	{
		warnf( TEXT("Failed to open script manifest") );
		return FALSE;
	}

	//build fully qualified class names
	TArray <ClassManifestDesc> ClassManifestDescriptions;
	ClassManifestDescriptions.Reserve(1000);
	for( TObjectIterator<UClass> ItC; ItC; ++ItC )
	{
		
		//Make description for this class
		ClassManifestDesc TempDesc;

		//fill in the details
		TempDesc.Class = *ItC;
		TempDesc.ClassName = TempDesc.Class->GetName();
		TempDesc.FullInheritanceName = TempDesc.ClassName;
		TempDesc.Depth = 0;

		UClass* ParentClass = TempDesc.Class->GetSuperClass();
		while(ParentClass)
		{
			TempDesc.Depth++;
			TempDesc.FullInheritanceName = ParentClass->GetName() + TEXT("-") + TempDesc.FullInheritanceName;
			ParentClass = ParentClass->GetSuperClass();
		}

		//add to the full list
		ClassManifestDescriptions.AddItem(TempDesc);
	}

	//sort by name
	Sort<USE_COMPARE_CONSTREF( ClassManifestDesc, MakeCommandlet )>( &ClassManifestDescriptions( 0 ), ClassManifestDescriptions.Num() );

	ANSICHAR ansiStr[1024];

	//Write version number
	FString VersionString = FString::Printf(TEXT("%d"), ClassManifestDefs::ManifestVersion);
	INT idx;
	for (idx = 0; idx < VersionString.Len() && idx < 1024 - 3; idx++)
	{
		ansiStr[idx] = ToAnsi((*VersionString)[idx]);
	}
	// null terminate
	ansiStr[idx++] = '\r';
	ansiStr[idx++] = '\n';
	ansiStr[idx] = '\0';
	ManifestFilePtr->Serialize(ansiStr, idx);


	// convert to ansi
	for (int i = 0; i < ClassManifestDescriptions.Num(); ++i)
	{
		const ClassManifestDesc &TempDesc = ClassManifestDescriptions(i);
		UClass* TempClass = TempDesc.Class;
		UObject* DefaultObject = TempClass->GetDefaultObject();

		FString ManifestLineEntry;
		for (int SpaceIndex = 0; SpaceIndex < TempDesc.Depth; ++SpaceIndex)
		{
			ManifestLineEntry.AppendChar(' ');
		}
		//depth in the tree
		ManifestLineEntry += FString::Printf(TEXT("%d"), TempDesc.Depth);
		ManifestLineEntry.AppendChar(' ');
		//class name
		ManifestLineEntry += TempDesc.ClassName;
		ManifestLineEntry.AppendChar(' ');
		//package name
		ManifestLineEntry += TempClass->GetOutermost()->GetName();

		FString FlagString;
		if(  TempClass->IsChildOf(UActorFactory::StaticClass()) &&
		   !(TempClass->ClassFlags & CLASS_Abstract) &&
		    (DefaultObject != NULL && ((UActorFactory*)DefaultObject)->bPlaceable))
		{
			//should be added to the actorfactory array
			FlagString += TEXT("F");

			FString UnusedErrorMsg;
			UActorFactory* NewFactory = ConstructObject<UActorFactory>( TempClass );
			//class can be added to the create actor menu
			if (NewFactory->CanCreateActor( UnusedErrorMsg ))
			{
				FlagString += TEXT("C");
			}
		}
		if (( TempClass->ClassFlags & CLASS_Hidden ) || (TempClass->ClassFlags & CLASS_Deprecated))
		{
			FlagString += TEXT("H");
		}
		if ( TempClass->ClassFlags & CLASS_Placeable )
		{
			FlagString += TEXT("P");
		}
		if ( TempClass->ClassFlags & CLASS_Abstract )
		{
			FlagString += TEXT("A");
		}
 
		ManifestLineEntry += FString::Printf(TEXT(" [%s]"), *FlagString );

		// Add class group names
		FString GroupNameStr;
		for( INT NameIndex = 0; NameIndex < TempClass->ClassGroupNames.Num(); ++NameIndex )
		{
			if( GroupNameStr.Len() > 0 )
			{
				GroupNameStr += TEXT(",");
			}
			GroupNameStr += *TempClass->ClassGroupNames( NameIndex ).ToString();
		}
		ManifestLineEntry += FString::Printf( TEXT(" [%s]"), *GroupNameStr );

		for (idx = 0; idx < ManifestLineEntry.Len() && idx < 1024 - 3; idx++)
		{
			ansiStr[idx] = ToAnsi((*ManifestLineEntry)[idx]);
		}

		// null terminate
		ansiStr[idx++] = '\r';
		ansiStr[idx++] = '\n';
		ansiStr[idx] = '\0';

		// and serialize to the archive
		ManifestFilePtr->Serialize(ansiStr, idx);
	}

	// delete it
	ManifestFilePtr->Flush();
	delete ManifestFilePtr;

	//successfully created the manifest
	return TRUE;
}


/**
 * Deletes all dependent .u files.  Given an index into the EditPackages array, deletes the .u files corresponding to
 * that index, as well as the .u files corresponding to subsequent members of the EditPackages array.
 *
 * @param	ScriptOutputDir		output directory for script packages
 * @param	PackageList			list of package names to delete
 * @param	StartIndex			index to start deleting packages
 * @param	Count				number of packages to delete - defaults to all
 */
void UMakeCommandlet::DeleteEditPackages(const FString& ScriptOutputDir, const TArray<FString>& PackageList, INT StartIndex, INT Count /* = INDEX_NONE */ ) const
{
	check(StartIndex>=0);

	if ( Count == INDEX_NONE )
	{
		Count = PackageList.Num() - StartIndex;
	}

	// Delete package and all the ones following in EditPackages. This is required in certain cases so we rather play safe.
	for( INT DeleteIndex = StartIndex; DeleteIndex < PackageList.Num() && Count > 0; DeleteIndex++, Count-- )
	{
		// create the output directory, if it doesn't exist
		GFileManager->MakeDirectory( *ScriptOutputDir, TRUE );

		const FString Filename = FString::Printf(TEXT("%s") PATH_SEPARATOR TEXT("%s.u"), *ScriptOutputDir, *PackageList(DeleteIndex) );
		if( !GFileManager->Delete(*Filename, 0, 0) )
		{
#if SHIPPING_PC_GAME
			warnf(NAME_Warning, TEXT("Failed to delete %s"), *Filename);
#else
			warnf( NAME_Error,TEXT("Failed to delete %s"), *Filename );
#endif
		}
#if SHIPPING_PC_GAME
		// also attempt to delete Published dir's version so the engine can't possibly load it and break things
		GFileManager->Delete(*GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*Filename)).Replace(TEXT("Unpublished"), TEXT("Published"), TRUE));
#endif
	}
}

IMPLEMENT_CLASS(UMakeCommandlet)
