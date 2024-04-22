/*=============================================================================
	UnPatchCommandlets.cpp: Class implementations for unrealscript bytecode patch
							generation commandlet and helpers
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "UnScriptPatcher.h"

#if !CONSOLE

static FString GetExportObjectName( INT ExportIndex, ULinkerLoad* SourceLinker )
{
	FString Result;

	check(SourceLinker);
	check(SourceLinker->ExportMap.IsValidIndex(ExportIndex));

	FObjectExport& Export = SourceLinker->ExportMap(ExportIndex);
	check(Export._Object != NULL);

	// use the linker to generate the object's path name so that if this object corresponds to a forced export, we get a unique patch class name
	Result = SourceLinker->GetExportPathName(ExportIndex);
	Result.ReplaceInline(TEXT("."),TEXT("_"));
	Result.ReplaceInline(SUBOBJECT_DELIMITER,TEXT("_"));

	return Result.Replace(*GScriptPatchPackageSuffix,TEXT("")) + TEXT("_Export");
}

static INT FindPathSeparator( FString Path )
{
	if ( Path.Right(1) == TEXT("/") )
		Path = Path.LeftChop(1);
	else if ( Path.Right(1) == TEXT("\\") )
		Path = Path.LeftChop(1);

	INT pos = Path.InStr(TEXT("/"),TRUE);
	if ( pos == INDEX_NONE )
		pos = Path.InStr(TEXT("\\"),TRUE);

	return pos;
}

#include "UnPatchCommandlets.h"

/*----------------------------------------------------------------------------
	FObjectPair.
----------------------------------------------------------------------------*/

UBOOL FObjectPair::ShouldComparePropertyValue( UProperty* Property, UStruct* CurrentOwnerStruct, UStruct* OriginalOwnerStruct, UProperty** MatchingProperty/*=NULL*/ )
{
	check(Property);
	check(CurrentOwnerStruct);
	check(OriginalOwnerStruct);

	// Are we comparing property values inside a struct
	const UBOOL bInsideStructs = CurrentOwnerStruct->IsA(UScriptStruct::StaticClass()) && OriginalOwnerStruct->IsA(UScriptStruct::StaticClass());

	// disregard any Object properties and any properties which are marked native, transient, config, or localized
	const UBOOL bShouldSkipProperty = Property->GetOwnerClass()->GetFName() == NAME_Object
		|| (Property->PropertyFlags&(CPF_Native|CPF_Transient|CPF_Config|CPF_Localized)) != 0;

	// Object, native, transient, config or localized properties can't be declared in defaultproperties either BUT that shouldn't prevent struct defaults from being compared
	// MAKE should prevent actual config/localized properties from actually showing up in the default property section, so we're left with proper
	// initialized values for the rest of the struct
	if ( bShouldSkipProperty && !bInsideStructs)
	{
		return FALSE;
	}

	UBOOL bResult = FALSE;

	// find the corresponding property in the OriginalClass
	UProperty* OriginalProperty = FindField<UProperty>(OriginalOwnerStruct, Property->GetFName());

	// perform a few quick checks to determine whether this is in fact the same property
	UBOOL bFoundMatchingProperty = OriginalProperty != NULL			// if the property has the same name
		&& Property->GetID() == OriginalProperty->GetID()			// and it has the same class
		&& Property->ElementSize == OriginalProperty->ElementSize;	// and it still has the same array size

	if ( bFoundMatchingProperty )
	{
		bResult = TRUE;

		// perform a few additional checks
		UStructProperty* StructProp = Cast<UStructProperty>(Property), *OriginalStructProp=Cast<UStructProperty>(OriginalProperty);
		if ( StructProp == NULL )
		{
			UObjectProperty* ObjectProp = Cast<UObjectProperty>(Property), *OriginalObjectProp=Cast<UObjectProperty>(OriginalProperty);
			if ( ObjectProp == NULL )
			{
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property);
				if ( ArrayProp != NULL )
				{
					UArrayProperty* OriginalArrayProp = Cast<UArrayProperty>(OriginalProperty);
					if ( ArrayProp->Inner->GetID() == OriginalArrayProp->Inner->GetID()
					&&	ArrayProp->Inner->ElementSize == OriginalArrayProp->Inner->ElementSize )
					{
						StructProp = Cast<UStructProperty>(ArrayProp->Inner);
						if ( StructProp != NULL )
						{
							OriginalStructProp = Cast<UStructProperty>(OriginalArrayProp->Inner);
						}
						else
						{
							ObjectProp = Cast<UObjectProperty>(ArrayProp->Inner);
							if ( ObjectProp != NULL )
							{
								OriginalObjectProp=Cast<UObjectProperty>(OriginalArrayProp->Inner);
							}
						}
					}
					else
					{
						bResult = FALSE;
					}
				}
			}

			if ( bResult && ObjectProp != NULL )
			{
				if ( ObjectProp->PropertyClass->GetFName() != OriginalObjectProp->PropertyClass->GetFName() )
				{
					bResult = FALSE;
				}
				else
				{
					// if it's a class property, check to see if the meta class was changed
					UClassProperty* ClassProp = Cast<UClassProperty>(ObjectProp);
					if ( ClassProp != NULL )
					{
						UClassProperty* OriginalClassProp = Cast<UClassProperty>(OriginalObjectProp);
						if ( ClassProp->MetaClass->GetFName() != OriginalClassProp->MetaClass->GetFName() )
						{
							bResult = FALSE;
						}
					}
				}
			}
		}

		if ( bResult && StructProp != NULL )
		{
			// if this is a struct property, verify that all the properties inside the struct match as well
			if ( OriginalStructProp != NULL && StructProp->Struct->GetFName() == OriginalStructProp->Struct->GetFName() )
			{
				for ( UProperty* InnerStructProp = StructProp->Struct->PropertyLink; InnerStructProp; InnerStructProp = InnerStructProp->PropertyLinkNext )
				{
					if ( !ShouldComparePropertyValue(InnerStructProp, StructProp->Struct, OriginalStructProp->Struct) )
					{
						bResult = FALSE;
						break;
					}
				}
			}
		}
	}

	if ( MatchingProperty != NULL )
	{
		*MatchingProperty = OriginalProperty;
	}

	return bResult;
}

UBOOL FObjectPair::PerformStructPropertyComparison( UStruct* OriginalStruct, UStruct* CurrentStruct, BYTE* OriginalValue, BYTE* CurrentValue )
{
	UBOOL bResult=FALSE;

	// this will be used to determine whether this property's value is inherited or not; if its inherited, we don't need to consider it here
	BYTE* InheritedValueBase=NULL;
	INT InheritedDataCount=0;
	if ( CurrentStruct->IsA(UClass::StaticClass()) )
	{
		UClass* ParentClass = static_cast<UClass*>(CurrentStruct)->GetSuperClass();
		if ( ParentClass != NULL )
		{
			InheritedDataCount = ParentClass->GetPropertiesSize();
			InheritedValueBase = ((UClass*)CurrentStruct)->GetSuperClass()->GetDefaults();
		}
	}

	BYTE* OriginalInheritedValueBase=NULL;
	INT OriginalInheritedDataCount=0;
	if ( OriginalStruct->IsA(UClass::StaticClass()) )
	{
		UClass* ParentClass = static_cast<UClass*>(OriginalStruct)->GetSuperClass();
		if ( ParentClass != NULL )
		{
			OriginalInheritedDataCount = ParentClass->GetPropertiesSize();
			OriginalInheritedValueBase = ((UClass*)OriginalStruct)->GetSuperClass()->GetDefaults();
		}
	}

	for ( UProperty* Property = CurrentStruct->PropertyLink; Property; Property = Property->PropertyLinkNext )
	{
		// disregard any Object properties and any properties which are marked native, transient, config, or localized
		const UBOOL bShouldSkipProperty = Property->GetOwnerClass()->GetFName() == NAME_Object
			|| (Property->PropertyFlags&(CPF_Native|CPF_Transient|CPF_Config|CPF_Localized)) != 0;

		if ( !bShouldSkipProperty )
		{
			UProperty* OriginalProperty=NULL;
			if ( ShouldComparePropertyValue(Property, CurrentStruct, OriginalStruct, &OriginalProperty) )
			{
				// disregard property values which have been inherited from the parent class (we don't need to explicitly export these)
				UBOOL bInheritedValue = FALSE;
				if ( InheritedValueBase != NULL && Property->Offset + Property->ElementSize * Property->ArrayDim <= InheritedDataCount &&
						Property->ArrayDim == OriginalProperty->ArrayDim && Property->ElementSize == OriginalProperty->ElementSize)
				{
					for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
					{
						UBOOL bCurrentMatchesInherited = Property->Matches(InheritedValueBase, CurrentValue, ArrayIndex, FALSE, PPF_DeepComparison);
						UBOOL bOriginalMatchesInherited = OriginalProperty->Matches(OriginalInheritedValueBase, OriginalValue, ArrayIndex, FALSE, PPF_DeepComparison);

						// disregard if our new value matches parent value and also the original value
						if ( bCurrentMatchesInherited && bOriginalMatchesInherited )
						{
							bInheritedValue = TRUE;
						}
						else
						{
							bInheritedValue = FALSE;
							break;
						}
					}
				}

				if ( bInheritedValue )
				{
					continue;
				}

				// if we're here, we've determined that OriginalStruct and CurrentStruct are the same property and that it's OK to compare their values
				// now we need to figure out which method to use for extracting the property's value from the raw value addresses (Original/CurrentValue);
				// only structs and arrays require special handling, so first figure out if that's what we're dealing with.
				UStructProperty* StructProp = Cast<UStructProperty>(Property), *OriginalStructProp=Cast<UStructProperty>(OriginalProperty);
				UArrayProperty* ArrayProp = NULL;
				if ( StructProp == NULL )
				{
					// see if this an array of structs
					ArrayProp = Cast<UArrayProperty>(Property);
					if ( ArrayProp != NULL )
					{
						StructProp = Cast<UStructProperty>(ArrayProp->Inner);
						OriginalStructProp = Cast<UStructProperty>(Cast<UArrayProperty>(OriginalProperty)->Inner);
					}
				}

				// if it's a struct property, then we must be more careful as the variables inside the struct could have been rearranged.
				if ( StructProp != NULL )
				{
					BYTE* CurrentPropertyValue = NULL, *OriginalPropertyValue = NULL;
					if ( ArrayProp != NULL )
					{
						for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
						{
							FScriptArray* CurrentArray = (FScriptArray*)(CurrentValue + Property->Offset + ArrayIndex * Property->ElementSize);
							FScriptArray* DefaultArray = (FScriptArray*)(OriginalValue + OriginalProperty->Offset + ArrayIndex * Property->ElementSize);

							if ( CurrentArray->Num() != DefaultArray->Num() )
							{
								DifferenceCount++;
								bResult = TRUE;
								break;
							}
							else
							{
								for ( INT DynArrayIndex = 0; DynArrayIndex < CurrentArray->Num(); DynArrayIndex++ )
								{
									BYTE* StructValue = (BYTE*)CurrentArray->GetData() + DynArrayIndex * ArrayProp->Inner->ElementSize;
									BYTE* OriginalStructValue = (BYTE*)DefaultArray->GetData() + DynArrayIndex * ArrayProp->Inner->ElementSize;
									if ( PerformStructPropertyComparison(OriginalStructProp->Struct, StructProp->Struct, OriginalStructValue, StructValue) )
									{
										bResult = TRUE;
										break;
									}
								}
							}
						}

					}
					else
					{
						for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
						{
							CurrentPropertyValue = CurrentValue + Property->Offset + ArrayIndex * Property->ElementSize;
							OriginalPropertyValue = OriginalValue + OriginalProperty->Offset + ArrayIndex * Property->ElementSize;
							if ( PerformStructPropertyComparison(OriginalStructProp->Struct, StructProp->Struct, OriginalPropertyValue, CurrentPropertyValue) )
							{
								bResult = TRUE;
								break;
							}
						}
					}
				}
				else
				{
					for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
					{
						BYTE* CurrentPropertyValue = CurrentValue + Property->Offset + ArrayIndex * Property->ElementSize;
						BYTE* OriginalPropertyValue = OriginalValue + OriginalProperty->Offset + ArrayIndex * Property->ElementSize;
						
						const UBOOL bValuesMatch = Property->Identical(OriginalPropertyValue, CurrentPropertyValue, PPF_DeepComparison);
						if ( !bValuesMatch )
						{
							DifferenceCount++;
							bResult = TRUE;
							break;
						}
					}
				}
			}
			else if ( OriginalProperty == NULL )
			{
				// If the original version of the class doesn't contain this property, then it has been added since OriginalObject was saved; in this case,
				// the defaults are still considered identical if CurrentObject doesn't have a value for this property
				for ( INT ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++ )
				{
					BYTE* CurrentPropertyValue = CurrentValue + Property->Offset + ArrayIndex * Property->ElementSize;
					if ( Property->HasValue(CurrentPropertyValue) )
					{
						DifferenceCount++;
						bResult = TRUE;
						break;
					}
				}
			}
		}
	}

	return bResult;
}

void FObjectPair::PerformComparison()
{
	check(OriginalObject);
	check(CurrentObject);

	// can't compare the object's Class pointers because for different versions of an object, the Class
	// pointer will always be different
	checkf(OriginalObject->GetClass()->GetFName() == CurrentObject->GetClass()->GetFName(), TEXT("OriginalObject:%s   Other OriginalObject:%s"), *OriginalObject->GetFullName(), *CurrentObject->GetFullName());

	DifferenceCount = 0;
	PerformStructPropertyComparison(OriginalObject->GetClass(), CurrentObject->GetClass(), (BYTE*)OriginalObject, (BYTE*)CurrentObject);
	if ( CurrentObject->GetClass()->GetPropertiesSize() != OriginalObject->GetClass()->GetPropertiesSize() )
	{
		for ( UClass* OriginalSuperClass = OriginalObject->GetClass(), *CurrentSuperClass = CurrentObject->GetClass();
			OriginalSuperClass && CurrentSuperClass; OriginalSuperClass = OriginalSuperClass->GetSuperClass(), CurrentSuperClass = CurrentSuperClass->GetSuperClass() )
		{
			if ( OriginalSuperClass->HasAnyFlags(RF_Native) || CurrentSuperClass->HasAnyFlags(RF_Native) )
			{
				if ( OriginalSuperClass->GetPropertiesSize() != CurrentSuperClass->GetPropertiesSize() )
				{
					CurrentObject->AddToRoot();
					OriginalObject->AddToRoot();
					break;
				}
			}
		}
	}
}

void FObjectPair::DisplayDifferences( INT Indent/*=0*/ )
{
}

/*----------------------------------------------------------------------------
	FStructComparison.
----------------------------------------------------------------------------*/
struct FStructComparison
{
public:
	UBOOL operator==( const FStructComparison& Other ) const
	{
		if ( (Struct == NULL) != (Other.Struct == NULL) )
			return FALSE;

		if ( Struct != NULL )
		{
			// the TArray::operator== will do the right thing
			return Struct->Script == Other.Struct->Script;
		}

		return TRUE;
	}
	UBOOL operator!=( const FStructComparison& Other ) const
	{
		if ( (Struct == NULL) != (Other.Struct == NULL) )
			return TRUE;

		if ( Struct != NULL )
		{
			// the TArray::operator!= will do the right thing
			return Struct->Script != Other.Struct->Script;
		}

		return FALSE;
	}

	FStructComparison( UStruct* inStruct )
	: Struct(inStruct)
	{
		check(Struct);
	}

	UStruct* Struct;
};

/*----------------------------------------------------------------------------
	FStructPair.
----------------------------------------------------------------------------*/
void FStructPair::CompileChildMap()
{
	if ( OriginalStruct != NULL )
	{
		for ( TFieldIterator<UStruct> It(OriginalStruct,FALSE); It; ++It )
		{
			UStruct* Struct = *It;
			FString StructId = Struct->GetPathName(Struct->GetOwnerClass());
			FStructPair* Pair = ChildMap->Find(*StructId);
			if ( Pair == NULL )
			{
				Pair = &ChildMap->Set(*StructId, FStructPair(*StructId));
			}

			check(Pair);
			Pair->OriginalStruct = Struct;
		}
	}

	if ( CurrentStruct != NULL )
	{
		for ( TFieldIterator<UStruct> It(CurrentStruct,FALSE); It; ++It )
		{
			UStruct* Struct = *It;
			FString StructId = Struct->GetPathName(Struct->GetOwnerClass());
			FStructPair* Pair = ChildMap->Find(*StructId);
			if ( Pair == NULL )
			{
				Pair = &ChildMap->Set(*StructId, FStructPair(*StructId));
			}

			check(Pair);
			Pair->CurrentStruct = Struct;
		}
	}

	// now verify that every struct has a corresponding struct in the other version of the package
	for ( TMap<FString,FStructPair>::TIterator It(*ChildMap); It; ++It )
	{
		FStructPair& Pair = It.Value();
		Pair.CompileChildMap();
	}
}

/**
 * Performs a comparison of two versions of the same struct or class, looking for structs which were added, removed, or have functions which are different.
 * When a difference is detected, the relevant data about the struct is put into the appopriate list.
 */
void FStructPair::PerformComparison(FComparisonResults& Result)
{
	if ( CurrentStruct == NULL )
	{
		check(OriginalStruct);
		new(Result.StructsRemoved) FStructPair(*this);
	}
	else if ( OriginalStruct == NULL )
	{
		check(CurrentStruct);
		new(Result.StructsAdded) FStructPair(*this);
	}
	else
	{
		FStructComparison Original(OriginalStruct), Current(CurrentStruct);
		if ( Original != Current )
		{
			new(Result.StructsModified) FStructPair(*this);
		}

		if ( CurrentStruct->IsA(UClass::StaticClass()) )
		{
			for ( TFieldIterator<UEnum> It(CurrentStruct); It; ++It )
			{
				UEnum* CurrentEnum = *It;
				UEnum* OriginalEnum = FindField<UEnum>(OriginalStruct, CurrentEnum->GetFName());
				if ( OriginalEnum != NULL )
				{
					TArray<FName> OriginalEnumNames, CurrentEnumNames;

					const INT OriginalEnumCount = OriginalEnum->NumEnums();
					const INT CurrentEnumCount = CurrentEnum->NumEnums();

					UBOOL bModifiedEnum = OriginalEnumCount != CurrentEnumCount;
					if ( !bModifiedEnum )
					{
						for ( INT EnumIndex = 0; EnumIndex < CurrentEnumCount; EnumIndex++ )
						{
							if ( OriginalEnum->GetEnum(EnumIndex) != CurrentEnum->GetEnum(EnumIndex) )
							{
								bModifiedEnum = TRUE;
								break;
							}
						}
					}

					if ( bModifiedEnum )
					{
						Result.ModifiedEnums.AddUniqueItem(CurrentEnum);
					}
				}
			}
		}
	}

	for ( TMap<FString,FStructPair>::TIterator It(*ChildMap); It; ++It )
	{
		FStructPair& Pair = It.Value();
		Pair.PerformComparison(Result);
	}
}

void FStructPair::DisplayDifferences( INT Indent /* = 0  */ )
{
	if ( CurrentStruct == NULL )
	{
		check(OriginalStruct);
		warnf(TEXT("%sRemoved '%s' (doesn't exist in current version)"), appSpc(Indent), *OriginalStruct->GetFullName());
	}

	else if ( OriginalStruct == NULL )
	{
		check(CurrentStruct);
		warnf(TEXT("%sAdded '%s' (doesn't exist in original version)"), appSpc(Indent), *CurrentStruct->GetFullName());
	}

	else if ( OriginalStruct->Script.Num() != CurrentStruct->Script.Num() )
	{
		warnf(TEXT("%sChanged '%s' (%i/%i)"), appSpc(Indent), *CurrentStruct->GetFullName(), OriginalStruct->Script.Num(), CurrentStruct->Script.Num());
	}

	for ( TMap<FString,FStructPair>::TIterator It(*ChildMap); It; ++It )
	{
		FStructPair& Pair = It.Value();
		Pair.DisplayDifferences(Indent + 2);
	}
}

/*----------------------------------------------------------------------------
	FPackageComparison.
----------------------------------------------------------------------------*/
/**
 * Builds a list of class pairs contained by this package and its corresponding original version.
 * 
 * @param	Classes		[out] map of classname to classes instances
 */
void FPackageComparison::CompileClassMap( TMap<FString,FClassPair>& Classes )
{
	for ( INT ExportIndex = 0; ExportIndex < OriginalPackage.Exports.Num(); ExportIndex++ )
	{
		const FObjectExport& Export = OriginalPackage.Exports(ExportIndex);
		if ( Export._Object != NULL && Export._Object->GetClass() == UClass::StaticClass() )
		{
			UClass* Class = static_cast<UClass*>(Export._Object);
			const FString ClassName = Class->GetName();
			FClassPair* Pair = Classes.Find(ClassName);
			if ( Pair == NULL )
			{
				Pair = &Classes.Set(*ClassName, FClassPair(*ClassName));
			}

			check(Pair);
			Pair->OriginalStruct = Class;
		}
	}

	for ( INT ExportIndex = 0; ExportIndex < CurrentPackage.Exports.Num(); ExportIndex++ )
	{
		const FObjectExport& Export = CurrentPackage.Exports(ExportIndex);
		if ( Export._Object != NULL && Export._Object->GetClass() == UClass::StaticClass() )
		{
			UClass* Class = static_cast<UClass*>(Export._Object);
			const FString ClassName = Class->GetName();
			FClassPair* Pair = Classes.Find(ClassName);
			if ( Pair == NULL )
			{
				Pair = &Classes.Set(*ClassName, FClassPair(*ClassName));
			}

			check(Pair);
			Pair->CurrentStruct = Class;
		}
	}

	// verify that all pairs contain both an original class and a current class
	for ( TMap<FString,FClassPair>::TIterator PairIt(Classes); PairIt; ++PairIt )
	{
		FString ClassName = PairIt.Key();
		FClassPair& Pair = PairIt.Value();

		if ( Pair.OriginalStruct == NULL )
		{
			warnf(TEXT(" Added class '%s' (doesn't exist in original package)"), *ClassName);
		}

		if ( Pair.CurrentStruct == NULL )
		{
			warnf(TEXT(" Removed class '%s' (doesn't exist in current package)"), *ClassName);
		}

		Pair.CompileChildMap();
	}
}

/**
 * Performs a comparison of the names, imports, exports, and function bytecode for two versions of a package.  Any differences are added to the comparison's
 * internal lists which are used for generating the script patch header file.
 */
void FPackageComparison::ComparePackages()
{
	TMap<FString,FClassPair> Classes;
	CompileClassMap(Classes);

	ComparisonResults.SetPackageName(*CurrentPackage.PackageName);
	for ( INT NameIndex = OriginalPackage.Names.Num(); NameIndex < CurrentPackage.Names.Num(); NameIndex++ )
	{
		const FName& CheckName = CurrentPackage.Names(NameIndex);

		new(ComparisonResults.NamesAdded) FName(CheckName);
	}

	for ( INT ImportIndex = OriginalPackage.Imports.Num(); ImportIndex < CurrentPackage.Imports.Num(); ImportIndex++ )
	{
		const FObjectImport& Import = CurrentPackage.Imports(ImportIndex);

		INT idx = ComparisonResults.ImportsAdded.AddZeroed();
		ComparisonResults.ImportsAdded(idx) = Import;
	}

	for ( INT ExportIndex = OriginalPackage.Exports.Num(); ExportIndex < CurrentPackage.Exports.Num(); ExportIndex++ )
	{
		const FObjectExport& Export = CurrentPackage.Exports(ExportIndex);

		INT idx = ComparisonResults.ExportsAdded.AddZeroed();
		ComparisonResults.ExportsAdded(idx) = Export;
	}

	for ( TMap<FString,FClassPair>::TIterator PairIt(Classes); PairIt; ++PairIt )
	{
		const FString& ClassName = PairIt.Key();
		FClassPair& Pair = PairIt.Value();

		// compare the bytecodes for all functions in this class.
		Pair.PerformComparison(ComparisonResults);
		if ( Pair.OriginalStruct != NULL && Pair.CurrentStruct != NULL )
		{
			UClass* OriginalClass = CastChecked<UClass>(Pair.OriginalStruct);
			UClass* CurrentClass = CastChecked<UClass>(Pair.CurrentStruct);

			UObject* OriginalCDO = OriginalClass->GetDefaultObject();
			UObject* CurrentCDO = CurrentClass->GetDefaultObject();

			FObjectPair ObjectPair(OriginalCDO, CurrentCDO);
			ObjectPair.PerformComparison();

			if ( ObjectPair.DifferenceCount > 0 )
			{
				ComparisonResults.DefaultObjectsModified.AddItem(ObjectPair);
			}
		}
	}
}

/**
 * Displays a summary of the comparison results for this package to the console window.
 */
void FPackageComparison::DisplayComparisonResults()
{
	if ( ComparisonResults )
	{
		warnf(TEXT("%sResults for %s:"), LINE_TERMINATOR, *CurrentPackage.PackageName);
		ComparisonResults.DisplayResults(3);
	}
}


/**
 * Writes the patch data (if any) for this package to the specified output device.
 *
 * @param	Writer	the output device to pipe the results to
 *
 * @return	TRUE if patch data was written to the output device.
 */
UBOOL FPackageComparison::OutputComparisonResults( FScriptPatchExporter& Writer )
{
	UBOOL bResult = FALSE;
	if ( ComparisonResults )
	{
		ComparisonResults.ExportResults(Writer);
		bResult = TRUE;
	}
	return bResult;
}


void FPackageComparison::DisplayPackageSummary( FPackageData& Package, FOutputDevice* OutputDevice )
{
	OutputDevice->Log( TEXT("********************************************") );
	OutputDevice->Logf( TEXT("Package '%s' Summary"), *Package.FileName );
	OutputDevice->Log( TEXT("--------------------------------------------") );

	OutputDevice->Logf( TEXT("\tPackageFlags: 0x%08X"), Package.Summary.PackageFlags );
	OutputDevice->Logf( TEXT("\t   NameCount: %d"), Package.Summary.NameCount );
	OutputDevice->Logf( TEXT("\t  NameOffset: %d"), Package.Summary.NameOffset );
	OutputDevice->Logf( TEXT("\t ImportCount: %d"), Package.Summary.ImportCount );
	OutputDevice->Logf( TEXT("\tImportOffset: %d"), Package.Summary.ImportOffset );
	OutputDevice->Logf( TEXT("\t ExportCount: %d"), Package.Summary.ExportCount );
	OutputDevice->Logf( TEXT("\tExportOffset: %d"), Package.Summary.ExportOffset );

	FString szGUID = Package.Summary.Guid.String();
	OutputDevice->Logf( TEXT("\t        Guid: %s"), *szGUID );
	OutputDevice->Log ( TEXT("\t Generations:"));
	for( INT i = 0; i < Package.Summary.Generations.Num(); ++i )
	{
		const FGenerationInfo& generationInfo = Package.Summary.Generations( i );
		OutputDevice->Logf( TEXT("\t\t%d) ExportCount=%d, NameCount=%d"), i, generationInfo.ExportCount, generationInfo.NameCount );
	}

	INT ClassCount = 0;
	for ( INT ExportIndex = 0; ExportIndex < Package.Exports.Num(); ExportIndex++ )
	{
		const FObjectExport& Export = Package.Exports(ExportIndex);
		if ( Export.ClassIndex == UCLASS_INDEX && Export.ObjectName != NAME_None )
		{
			ClassCount++;
		}
	}

	warnf(TEXT("Package %s contains %i classes") LINE_TERMINATOR, *Package.FileName, ClassCount);
}

/*----------------------------------------------------------------------------
	FComparisonResults.
----------------------------------------------------------------------------*/
void FComparisonResults::DisplayResults( INT Indent )
{
	if ( StructsAdded.Num() || NamesAdded.Num() || ImportsAdded.Num() || ExportsAdded.Num() )
	{
		warnf(TEXT("%sAdded:"), appSpc(Indent));

		if ( NamesAdded.Num() )
		{
			warnf(TEXT("%s(Names)"), appSpc(Indent+1));
			for ( INT i = 0; i < NamesAdded.Num(); i++ )
			{
				const FName& TheName = NamesAdded(i);
				GWarn->Logf( TEXT("%s%s"), appSpc(Indent+3), *TheName.ToString() );
			}
		}
		if ( ImportsAdded.Num() )
		{
			warnf(TEXT("%s(Imports)"), appSpc(Indent+1));
			for ( INT i = 0; i < ImportsAdded.Num(); i++ )
			{
				FObjectImport& Import = ImportsAdded(i);
				GWarn->Logf(TEXT("%s%s"),appSpc(Indent+3), *Import.ObjectName.ToString());
			}
		}
		if ( ExportsAdded.Num() )
		{
			warnf(TEXT("%s(Exports)"), appSpc(Indent+1));
			for ( INT i = 0; i < ExportsAdded.Num(); i++ )
			{
				FObjectExport& Export = ExportsAdded(i);
				GWarn->Logf(TEXT("%s%s"),appSpc(Indent+3), *Export.ObjectName.ToString());
			}
		}
		if ( StructsAdded.Num() )
		{
			warnf(TEXT("%s(Structs)"), appSpc(Indent+1));
			for ( INT i = 0; i < StructsAdded.Num(); i++ )
			{
				FStructPair& Pair = StructsAdded(i);
				GWarn->Logf(TEXT("%s%s"),appSpc(Indent+3),*Pair.CurrentStruct->GetPathName());
			}
		}
	}

	if ( StructsRemoved.Num() )
	{
		warnf(TEXT("%sRemoved:"), appSpc(Indent));
		for ( INT i = 0; i < StructsRemoved.Num(); i++ )
		{
			FStructPair& Pair = StructsRemoved(i);
			GWarn->Logf(TEXT("%s%s"),appSpc(Indent+3),*Pair.OriginalStruct->GetPathName());
		}
	}

	if ( StructsModified.Num() > 0 || DefaultObjectsModified.Num() > 0 || ModifiedEnums.Num() > 0 )
	{
		UBOOL bInsertBlankLine=FALSE;
		if ( StructsModified.Num() > 0 )
		{
			warnf(TEXT("%sModified structs:"), appSpc(Indent));
			for ( INT i = 0; i < StructsModified.Num(); i++ )
			{
				FStructPair& Pair = StructsModified(i);
				GWarn->Logf(TEXT("%s%s (%i/%i)"),appSpc(Indent+3),*Pair.CurrentStruct->GetPathName(), Pair.OriginalStruct->Script.Num(), Pair.CurrentStruct->Script.Num());
			}
			bInsertBlankLine = TRUE;
		}
		if ( DefaultObjectsModified.Num() > 0 )
		{
			warnf(TEXT("%s%sModified defaults:"), bInsertBlankLine ? LINE_TERMINATOR : TEXT(""), appSpc(Indent));
			for ( INT i = 0; i < DefaultObjectsModified.Num(); i++ )
			{
				FObjectPair& Pair = DefaultObjectsModified(i);
				warnf(TEXT("%s%s (%i values differed)"), appSpc(Indent+3), *Pair.CurrentObject->GetPathName(Pair.CurrentObject->GetOutermost()), Pair.DifferenceCount);
			}
			bInsertBlankLine = TRUE;
		}
		if ( ModifiedEnums.Num() )
		{
			warnf(TEXT("%s%sModified enums:"), bInsertBlankLine ? LINE_TERMINATOR : TEXT(""), appSpc(Indent));
			for ( INT i = 0; i < ModifiedEnums.Num(); i++ )
			{
				UEnum* Enum = ModifiedEnums(i);
				warnf(TEXT("%s%s (%i names)"), appSpc(Indent+3), *Enum->GetPathName(Enum->GetOutermost()), Enum->NumEnums());
			}
			bInsertBlankLine = TRUE;
		}
	}
}

void FComparisonResults::ExportResults( FScriptPatchExporter& Writer )
{
	ExportNewExports(Writer);
	ExportModifiedDefaults(Writer);
	ExportModifiedScript(Writer);
	ExportModifiedEnums(Writer);

	ExportPackageData(Writer);
}

void FComparisonResults::ExportNewExports( FScriptPatchExporter& Writer )
{
	for ( INT i = 0; i < ExportsAdded.Num(); i++ )
	{
		FObjectExport& Export = ExportsAdded(i);

		//@todo ronp - fix this
		if ( Export._Object == NULL )
		{
			warnf(TEXT("NULL object detected for new export '%s'.  Skipping generation of patch data..."), *Export.ObjectName.ToString());
			continue;
		}

		ExportObjectData( Writer, i + OriginalExportCount );
	}
}

void FComparisonResults::ExportModifiedDefaults( FScriptPatchExporter& Writer )
{
	for ( INT DefaultsIndex = 0; DefaultsIndex < DefaultObjectsModified.Num(); DefaultsIndex++ )
	{
		FObjectPair& ModifiedCDOPair = DefaultObjectsModified(DefaultsIndex);

		// we have to brute-force search because if the object is a forced export, GetLinker() won't necessarily return the same linker as
		// the PackageLinker
		INT LinkerIndex;
		for ( LinkerIndex = 0; LinkerIndex < PackageLinker->ExportMap.Num(); LinkerIndex++ )
		{
			if ( PackageLinker->ExportMap(LinkerIndex)._Object == ModifiedCDOPair.CurrentObject )
			{
				break;
			}
		}

		// make sure we found it in the PackageLinker
		check(LinkerIndex<PackageLinker->ExportMap.Num());
		ModifiedCDOPair.CurrentLinkerIndex = LinkerIndex;
		ExportObjectData(Writer, LinkerIndex);
	}
}


//@script patcher todo: refactor this so that different types of exporters can process the same raw patch data
//	- export as text:
//		data exported to one or more header files (this is the current method).  Bloats the executable, and if one map or script is changed, the entire executable
//		must be patched.
//	- export as data blob:
//		slighly better than exporting as text - no change to executable size, but data file will be very large.  If one map is changed, entire data file
//		would need to be updated
//	- export as package-specific data blobs:
//		Probably the most ideal solution; each map has its own data file which contains the patch data for that map.  Any patch data which is common to all maps
//			(such as patches to script code in native classes) would be written to a shared patch file.  No change to executable size, same disk footprint as using
//			a single data file (actually slightly more, but negligable), and changes to non-native scripts (e.g. a weapon that is only in two maps) only require the
//			the data file for those maps to be patched
void FComparisonResults::ExportObjectData( FScriptPatchExporter& Writer, INT ExportMapIndex )
{
	check(PackageLinker);
	check(PackageLinker->ExportMap.IsValidIndex(ExportMapIndex));

	FObjectExport& ObjectData = PackageLinker->ExportMap(ExportMapIndex);

	// grab the linker for this object
	check(ObjectData._Object);
	FString PatchClassName = GetExportObjectName(ExportMapIndex, PackageLinker);

	// ULinkerLoad keeps its Serialize function private, so grab the loader's
	// underlying file reader
	FArchive* Reader = PackageLinker->Loader;
	INT PrevPos = Reader->Tell();

	// move to the beginning of this object's data on disk
	Reader->Seek(ObjectData.SerialOffset);

	TArray<BYTE> Data;
	Data.AddZeroed(ObjectData.SerialSize);

	Reader->Serialize(Data.GetData(), ObjectData.SerialSize*sizeof(BYTE));
	Reader->Seek(PrevPos);

	// now we can create the byte array that will be embedded in the exe
	// class declaration
	Writer.Logf(TEXT("struct %s : public FPatchData {\r\n"), *PatchClassName);

	// constructor declaration
	Writer.Logf(TEXT("\t%s() {\r\n"), *PatchClassName);

	// constructor body
	Writer.Logf(TEXT("\t\tData.Add(%i);\r\n"), ObjectData.SerialSize);
	Writer.Logf(TEXT("\t\tDataName = TEXT(\"%s\");\r\n"), *ObjectData.ObjectName.ToString().Replace(*GScriptPatchPackageSuffix,TEXT("")));
	Writer.Logf(TEXT("\t\tBYTE bytes[%i]=\r\n\t\t{"), Data.Num());

	// bytecode array definition
	for ( INT Index = 0; Index < Data.Num(); Index++ )
	{
		// 32 bytes-per-line
		if ( Index % 32 == 0 )
		{
			Writer.Log(TEXT("\r\n\t\t\t"));
		}

		Writer.Logf(TEXT("0x%02X%s"), Data(Index), Index < Data.Num() - 1 ? TEXT(",") : TEXT(""));
	}
	Writer.Log(TEXT("\r\n\t\t};\r\n"));

	Writer.Logf(TEXT("\t\tappMemcpy(&Data(0),bytes,%i*sizeof(BYTE));\r\n\t}\r\n"), Data.Num());
}

void FComparisonResults::ExportModifiedScript( FScriptPatchExporter& Writer )
{
	for ( INT i = 0; i < StructsModified.Num(); i++ )
	{
		UStruct* Struct = StructsModified(i).CurrentStruct;
		FString StructPathName = Struct->GetPathName().Replace(OriginalPackageSuffix,TEXT("")).Replace(LatestPackageSuffix,TEXT(""));

		// we have to brute-force search because if Struct is a forced export, GetLinker() won't necessarily return the same linker as
		// the PackageLinker
		INT LinkerIndex;
		for ( LinkerIndex = 0; LinkerIndex < PackageLinker->ExportMap.Num(); LinkerIndex++ )
		{
			if ( PackageLinker->ExportMap(LinkerIndex)._Object == Struct )
			{
				break;
			}
		}

		// make sure we found it in the PackageLinker
		check(LinkerIndex<PackageLinker->ExportMap.Num());

		const INT x = Writer.PatchClassNames.AddZeroed();
		FString& PatchClassName = Writer.PatchClassNames(x);
		PatchClassName = GetExportObjectName(LinkerIndex, PackageLinker);

		// class declaration
		Writer.Logf(TEXT("struct %s : public FScriptPatchData\r\n{\r\n"), *PatchClassName);

		// constructor declaration
		Writer.Logf(TEXT("\t%s()\r\n\t{\r\n"), *PatchClassName);

		// constructor body
		Writer.Logf(TEXT("\t\tData.Add(%i);\r\n"), Struct->Script.Num());
		Writer.Logf(TEXT("\t\tDataName = TEXT(\"%s\");\r\n"), *StructPathName); // need to strip off the Package name which has been made unique and append the PackageName
		Writer.Logf(TEXT("\t\tStructName = TEXT(\"%s\");\r\n"), *Struct->GetName());
		Writer.Logf(TEXT("\t\tBYTE bytes[%i]=\r\n\t\t{"), Struct->Script.Num());

		// bytecode array definition
		// when the struct's script was loaded from disk, it may have been byte-swapped (i.e. when generating patches from cooked packges)
		// so run the bytecode through an archive first
		TArray<BYTE> Bytecode;
		Bytecode.AddZeroed(Struct->Script.Num());

		FMemoryWriter ByteswapperAr(Bytecode);
		ByteswapperAr.SetByteSwapping(Struct->GetLinker()->ForceByteSwapping());
		INT iCode=0;
		while ( iCode < Bytecode.Num() )
		{
			Struct->SerializeExpr(iCode, ByteswapperAr);
		}

		for ( INT ScriptIndex = 0; ScriptIndex < Bytecode.Num(); ScriptIndex++ )
		{
			// 32 bytes-per-line
			if ( ScriptIndex % 32 == 0 )
				Writer.Log(TEXT("\r\n\t\t\t"));

			Writer.Logf(TEXT("0x%02X%s"), Bytecode(ScriptIndex), ScriptIndex < Bytecode.Num() - 1 ? TEXT(",") : TEXT(""));
		}
		Writer.Log(TEXT("\r\n\t\t};\r\n"));

		Writer.Logf(TEXT("\t\tappMemcpy(&Data(0),bytes,%i*sizeof(BYTE));\r\n\t}\r\n\r\n"), Bytecode.Num());
	}
}

void FComparisonResults::ExportModifiedEnums( FScriptPatchExporter& Writer )
{
	for ( INT ModifiedEnumIndex = 0; ModifiedEnumIndex < ModifiedEnums.Num(); ModifiedEnumIndex++ )
	{
		UEnum* ModifiedEnum = ModifiedEnums(ModifiedEnumIndex);

		FString PatchClassName = GetExportObjectName(ModifiedEnum->GetLinkerIndex(), PackageLinker);

		// class declaration
		Writer.Logf(TEXT("struct %s : public FEnumPatchData\r\n{\r\n"), *PatchClassName);

		// constructor declaration
		Writer.Logf(TEXT("\t%s()\r\n\t{\r\n"), *PatchClassName);

		// constructor body
		Writer.Logf(TEXT("\t\tEnumName		= TEXT(\"%s\");\r\n"), *ModifiedEnum->GetName());

		FString EnumPathName = ModifiedEnum->GetPathName().Replace(OriginalPackageSuffix,TEXT("")).Replace(LatestPackageSuffix,TEXT(""));
		Writer.Logf(TEXT("\t\tEnumPathName	= TEXT(\"%s\");\r\n"), *EnumPathName);

		// the list of enum values
		for ( INT NameIndex = 0; NameIndex < ModifiedEnum->NumEnums(); NameIndex++ )
		{
			Writer.Logf(TEXT("\t\tnew(EnumValues) FName(TEXT(\"%s\"));\r\n"), *ModifiedEnum->GetEnum(NameIndex).ToString());
		}

		// class & constructor closing braces
		Writer.Log(TEXT("\t}\r\n};\r\n"));
	}
}

void FComparisonResults::ExportPackageData(FScriptPatchExporter& Writer)
{
	INT idx = Writer.LoaderPatchClassNames.AddZeroed();
	FString& PatchClass = Writer.LoaderPatchClassNames(idx);
	
	PatchClass = PackageName + TEXT("_Patch");

	// class declaration
	Writer.Logf(TEXT("struct %s : public FLinkerPatchData\r\n{\r\n"), *PatchClass);

	// constructor declaration
	Writer.Logf(TEXT("\t%s()\r\n\t{\r\n"), *PatchClass);

	// initialization
	Writer.Logf(TEXT("\t\tPackageName = TEXT(\"%s\");\r\n"), *PackageName);

	// names
	for ( INT i = 0; i < NamesAdded.Num(); i++ )
	{
		Writer.Logf(TEXT("\t\tnew(Names) FName(TEXT(\"%s\"));\r\n"), *NamesAdded(i).ToString());
	}

	for ( INT i = 0; i < ImportsAdded.Num(); i++ )
	{
		FObjectImport& Import = ImportsAdded(i);

		Writer.Logf(TEXT("\t\tnew(Imports) FObjectImport(BuildImport(FName(TEXT(\"%s\")),FName(TEXT(\"%s\")),FName(TEXT(\"%s\")),%i,%i));\r\n"),
			*Import.ObjectName.ToString(), *Import.ClassName.ToString(), *Import.ClassPackage.ToString(), Import.OuterIndex, Import.SourceIndex);
	}

	TArray<FString> NewExportNames;
	for ( INT i = 0; i < ExportsAdded.Num(); i++ )
	{
		FObjectExport& Export = ExportsAdded(i);
		if ( Export._Object != NULL )
		{
			new(NewExportNames) FString(GetExportObjectName(i + OriginalExportCount, PackageLinker));

			Writer.Logf(TEXT("\t\tnew(Exports) FObjectExport(BuildExport(FName(TEXT(\"%s\")), %lli, %d, %i, %i, %i, %i));\r\n"),
				*Export.ObjectName.ToString().Replace(*GScriptPatchPackageSuffix, TEXT("")), Export.ObjectFlags, Export.ExportFlags, Export.ClassIndex, Export.SuperIndex, Export.OuterIndex, Export.ArchetypeIndex);
		}
	}

	if ( NewExportNames.Num() > 0 )
	{
		//Writer.Log(TEXT("\r\n"));
		for ( INT i = 0; i < NewExportNames.Num(); i++ )
		{
			Writer.Logf(TEXT("\t\tNewObjects.AddItem(new %s());\r\n"), *NewExportNames(i));
		}
	}

	if ( DefaultObjectsModified.Num() > 0 )
	{
		//Writer.Log(LINE_TERMINATOR);
		for ( INT ObjIndex = 0; ObjIndex < DefaultObjectsModified.Num(); ObjIndex++ )
		{
			FObjectPair& ObjPair = DefaultObjectsModified(ObjIndex);

			// CurrentLinkerIndex should have been set to a valid index in ExportModifiedDefaults
			check(PackageLinker->ExportMap.IsValidIndex(ObjPair.CurrentLinkerIndex));
			
			FString MangledObjectName = GetExportObjectName(ObjPair.CurrentLinkerIndex, PackageLinker);
			Writer.Logf(TEXT("\t\tModifiedClassDefaultObjects.AddItem(new %s());%s"), *MangledObjectName, LINE_TERMINATOR);
		}
	}

	if ( Writer.PatchClassNames.Num() )
	{
		//Writer.Log(TEXT("\r\n"));
		for ( INT i = 0; i < Writer.PatchClassNames.Num(); i++ )
		{
			Writer.Logf(TEXT("\t\tScriptPatches.AddItem(new %s());\r\n"), *Writer.PatchClassNames(i));
		}

		Writer.PatchClassNames.Empty();
	}

	if ( ModifiedEnums.Num() > 0 )
	{
		Writer.Log(LINE_TERMINATOR);
		for ( INT EnumIndex = 0; EnumIndex < ModifiedEnums.Num(); EnumIndex++ )
		{
			UEnum* Enum = ModifiedEnums(EnumIndex);
			FString EnumPatchClassName = GetExportObjectName(Enum->GetLinkerIndex(), PackageLinker);
			Writer.Logf(TEXT("\t\tModifiedEnums.AddItem(new %s());\r\n"), *EnumPatchClassName);
		}
	}

	Writer.Log(TEXT("\t}\r\n};\r\n"));

}


/**
* Add the results for this package to the given array
*
* AllPackagePatches [out] Array to add this package's patch data to
*/
void FComparisonResults::BuildStructureForBinary(TArray<FLinkerPatchData>& AllPackagePatches, UBOOL bNonIntelByteCode)
{
	// make a new patch data object for the package
	FLinkerPatchData* PatchData = new (AllPackagePatches) FLinkerPatchData;

	// fill it out
	// copy over name and array of FNames
	PatchData->PackageName = FName(*PackageName);
	PatchData->Names = NamesAdded;

	// rebuild import objects using existing BuildImport method
	PatchData->Imports.Reserve(ImportsAdded.Num());
	for ( INT i = 0; i < ImportsAdded.Num(); i++ )
	{
		FObjectImport& Import = ImportsAdded(i);
		new(PatchData->Imports) FObjectImport(FLinkerPatchData::BuildImport(Import.ObjectName, Import.ClassName, Import.ClassPackage, Import.OuterIndex, Import.SourceIndex));
	}

	// rebuild export objects using existing BuildExport method
	PatchData->Exports.Reserve(ExportsAdded.Num());
	for ( INT i = 0; i < ExportsAdded.Num(); i++ )
	{
		FObjectExport& Export = ExportsAdded(i);
		if ( 1|| Export._Object != NULL )
		{
			new(PatchData->Exports) FObjectExport(FLinkerPatchData::BuildExport(Export.ObjectName, Export.ObjectFlags, Export.ExportFlags, Export.ClassIndex, Export.SuperIndex, Export.OuterIndex, Export.ArchetypeIndex));
		}
	}

	// here we just add generic FPatchData objects instead of the named subclasses that we see in the ScriptPatches.h
	PatchData->NewObjects.Reserve(ExportsAdded.Num());
	for ( INT i = 0; i < ExportsAdded.Num(); i++ )
	{
		FObjectExport& Export = ExportsAdded(i);

		//@todo ronp - fix this
		if ( 0&&Export._Object == NULL )
		{
			warnf(TEXT("NULL object detected for new export '%s'.  Skipping generation of patch data..."), *Export.ObjectName.ToString());
			continue;
		}

		FPatchData* NewObject = new (PatchData->NewObjects) FPatchData;

		// build the patch object
		BuildObjectDataForBinary(NewObject, i + OriginalExportCount);
	}

	// copy over default objects
	PatchData->ModifiedClassDefaultObjects.Reserve(DefaultObjectsModified.Num());
	for ( INT ObjIndex = 0; ObjIndex < DefaultObjectsModified.Num(); ObjIndex++ )
	{
		FObjectPair& ObjPair = DefaultObjectsModified(ObjIndex);
		FPatchData* NewObject = new (PatchData->ModifiedClassDefaultObjects) FPatchData;

		// we have to brute-force search because if the object is a forced export, GetLinker() won't necessarily return the same linker as
		// the PackageLinker
		INT LinkerIndex;
		for ( LinkerIndex = 0; LinkerIndex < PackageLinker->ExportMap.Num(); LinkerIndex++ )
		{
			if ( PackageLinker->ExportMap(LinkerIndex)._Object == ObjPair.CurrentObject )
			{
				break;
			}
		}

		// make sure we found it in the PackageLinker
		check(LinkerIndex<PackageLinker->ExportMap.Num());

		// build the patch object
		BuildObjectDataForBinary(NewObject, LinkerIndex);
	}

	// copy over script patch data
	PatchData->ScriptPatches.Reserve(StructsModified.Num());
	for ( INT i = 0; i < StructsModified.Num(); i++ )
	{
		FScriptPatchData* ScriptPatch = new (PatchData->ScriptPatches) FScriptPatchData;
		BuildScriptDataForBinary(ScriptPatch, i, bNonIntelByteCode);
	}

	// copy over new enums
	PatchData->ModifiedEnums.Reserve(ModifiedEnums.Num());
	for ( INT EnumIndex = 0; EnumIndex < ModifiedEnums.Num(); EnumIndex++ )
	{
		FEnumPatchData* NewEnum = new (PatchData->ModifiedEnums) FEnumPatchData;
		BuildEnumDataForBinary(NewEnum, EnumIndex);
	}

}

/**
 * Fill out an FPatchDataObject
 *
 * @todo comment
 */
void FComparisonResults::BuildObjectDataForBinary(FPatchData* PatchObject, INT ExportMapIndex)
{
	check(PackageLinker);
	check(PackageLinker->ExportMap.IsValidIndex(ExportMapIndex));

	FObjectExport& ObjectData = PackageLinker->ExportMap(ExportMapIndex);

	// @todo: Make this an FName?????
	PatchObject->DataName = ObjectData.ObjectName.ToString().Replace(*GScriptPatchPackageSuffix, TEXT(""));

	// grab the linker for this object
//	check(ObjectData._Object);

	// ULinkerLoad keeps its Serialize function private, so grab the loader's
	// underlying file reader
	FArchive* Reader = PackageLinker->Loader;
	INT PrevPos = Reader->Tell();

	// move to the beginning of this object's data on disk
	Reader->Seek(ObjectData.SerialOffset);

	// read the object right into the Data array
	PatchObject->Data.AddZeroed(ObjectData.SerialSize);

	Reader->Serialize(PatchObject->Data.GetData(), ObjectData.SerialSize*sizeof(BYTE));
	Reader->Seek(PrevPos);

}

void FComparisonResults::BuildScriptDataForBinary(FScriptPatchData* ScriptPatch, INT StructIndex, UBOOL bNonIntelByteCode)
{
	UStruct* Struct = StructsModified(StructIndex).CurrentStruct;
	FString StructPathName = Struct->GetPathName().Replace(OriginalPackageSuffix,TEXT("")).Replace(LatestPackageSuffix,TEXT(""));

	// we have to brute-force search because if Struct is a forced export, GetLinker() won't necessarily return the same linker as
	// the PackageLinker
	INT LinkerIndex;
	for ( LinkerIndex = 0; LinkerIndex < PackageLinker->ExportMap.Num(); LinkerIndex++ )
	{
		if ( PackageLinker->ExportMap(LinkerIndex)._Object == Struct )
		{
			break;
		}
	}

	// copy stuff from the struct
	ScriptPatch->DataName = StructPathName; // need to strip off the Package name which has been made unique and append the PackageName
	ScriptPatch->StructName = Struct->GetFName();

	// get appropriate bytecode
	ScriptPatch->Data.AddZeroed(Struct->Script.Num());

	FMemoryWriter ByteswapperAr(ScriptPatch->Data);
	ByteswapperAr.SetByteSwapping(Struct->GetLinker()->ForceByteSwapping());
	INT iCode=0;
	while ( iCode < ScriptPatch->Data.Num() )
	{
		Struct->SerializeExpr(iCode, ByteswapperAr);
	}
}

/**
 * @todo comment
 */
void FComparisonResults::BuildEnumDataForBinary(FEnumPatchData* NewEnum, INT EnumIndex)
{
	UEnum* Enum = ModifiedEnums(EnumIndex);

	// get values from the UEnum
	NewEnum->EnumName = Enum->GetFName();
	NewEnum->EnumPathName = Enum->GetPathName().Replace(OriginalPackageSuffix,TEXT("")).Replace(LatestPackageSuffix,TEXT("")); 

	// the list of enum values
	NewEnum->EnumValues.Reserve(Enum->NumEnums());
	for ( INT NameIndex = 0; NameIndex < Enum->NumEnums(); NameIndex++ )
	{
		NewEnum->EnumValues.AddItem(Enum->GetEnum(NameIndex));
	}
}

/*----------------------------------------------------------------------------
	FScriptPatchExporter.
----------------------------------------------------------------------------*/
void FScriptPatchExporter::SaveToFile()
{
	// optionally output header file
	if (ParseParam(appCmdLine(), TEXT("outputheaders")))
	{
		Log(TEXT("\r\nvoid BuildPatchList( TArray<FLinkerPatchData*>& PackagePatches )\r\n{\r\n"));
		if ( LoaderPatchClassNames.Num() > 0 )
		{
			Log(TEXT("\tstatic UBOOL bDisablePatching = GIsUCC || ParseParam(appCmdLine(), TEXT(\"NOPATCH\"));\r\n"));
			Log(TEXT("\tif ( !bDisablePatching )\r\n\t{\r\n"));
			for ( INT i = 0; i < LoaderPatchClassNames.Num(); i++ )
			{
				Logf(TEXT("\t\tPackagePatches.AddItem(new %s());\r\n"), *LoaderPatchClassNames(i));
			}
			Log(TEXT("\t}\r\n"));
		}
		Log(TEXT("}\r\n\r\n"));
	
		FString HeaderPathName = FString::Printf(TEXT("%sScriptPatches.h"), bNonIntelByteOrder ? TEXT("Swapped") : TEXT(""));
		if ( !appSaveStringToFile(*this, *HeaderPathName) )
		{
			warnf(TEXT("Failed to overwrite %s!"), *HeaderPathName);
		}
	}

	// export each package to its own bin file
	for (INT PackageIndex = 0; PackageIndex < AllPackagePatches.Num(); PackageIndex++)
	{
		TArray<BYTE> Buffer;
		FPatchBinaryWriter Writer(Buffer);
		Writer.SetByteSwapping(bNonIntelByteOrder);

		// write out the patch using the special PatchBinaryWriter archive
		Writer << AllPackagePatches(PackageIndex);
		
		FString DstFilename = FString::Printf(TEXT("%sPatches\\%s\\ScriptPatch_%s.bin"), *appGameDir(), *appPlatformTypeToString(GPatchingTarget), *AllPackagePatches(PackageIndex).PackageName.ToString());
		FArchive* BinFile = GFileManager->CreateFileWriter(*DstFilename);

		// write it out compressed
		BinFile->SetByteSwapping(bNonIntelByteOrder);
		BinFile->SerializeCompressed(Buffer.GetData(), Buffer.Num(), (ECompressionFlags)(GBaseCompressionMethod | COMPRESS_BiasMemory));
		delete BinFile;

		// write out our helper file containing uncompressed size
		FString SizeString = FString::Printf(TEXT("%d%s"), Buffer.Num(), LINE_TERMINATOR);
		if (GPatchingTarget & UE3::PLATFORM_PC) 
		{
			appSaveStringToFile(SizeString, *(DstFilename + TEXT(".uncompressed_size")));
		}
		else
		{
			appSaveStringToFile(SizeString, *(DstFilename + TEXT("_us")));
		}
	}
}


/*-----------------------------------------------------------------------------
	UPatchScriptCommandlet.
-----------------------------------------------------------------------------*/


FScriptPatchWorker::FScriptPatchWorker()
: bNonIntelScriptPatch(FALSE),bStrippedScriptPatch(FALSE)
{
}

INT FScriptPatchWorker::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;

	// get the platform from the Params (MUST happen first thing)
	if ( !SetPlatform( appCmdLine() ) )
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(NAME_Error, TEXT("Platform not specified. You must use:\n   platform=[=[%s]"), *appValidPlatformsString());
		CLEAR_WARN_COLOR();
		return 1;
	}

	// Parse command line.
	UCommandlet::ParseCommandLine(Parms, Tokens, Switches);
	if (Switches.ContainsItem(TEXT("nonintel")))
	{
		SET_WARN_COLOR(COLOR_RED);
		warnf(NAME_Warning, TEXT("-nonintel is deprecated, use -platform, forcing non-intel byte order."));
		CLEAR_WARN_COLOR();
		bNonIntelScriptPatch = TRUE;
		//Non-intel implies console implies stripped
		bStrippedScriptPatch = TRUE;
	}

	if ( !InitCommandlet() )
	{
		return 1;
	}

	FScriptPatchExporter Writer(bNonIntelScriptPatch);

	// load all native packages
	GenerateNativePackageComparisons(Writer);
	GenerateNonNativePackageComparisons(Writer);

	Writer.SaveToFile();

	GIsScriptPatcherActive = FALSE;
	
 	if (GLog)
 	{
 		GLog->FlushThreadedLogs();
 		GLog->Flush();
 	}
 
 	// Proper cleanup isn't working right now (seems to be GameInfo script modification causing failure in PreExit)
 	appRequestExit(TRUE);
	return 0;
}

/**
 * Parses the specified array of tokens to determine the operational parameters to use for running the commandlet.
 *
 * @return	TRUE if all parameters were parsed successfully and the specified paths and package name/wildcard corresponded to valid files
 */
UBOOL FScriptPatchWorker::InitCommandlet()
{
#if !SUPPORTS_SCRIPTPATCH_CREATION
	// trying to create script patch without support for that enabled
	warnf(NAME_Error, TEXT("Executable not compiled with SUPPORTS_SCRIPTPATCH_CREATION=1.  Patch creation failed."));
	return FALSE;
#endif

	if (GPatchingTarget & (UE3::PLATFORM_Console | UE3::PLATFORM_WindowsServer))
	{
		UpdateCookedPlatformIniFilesFromDefaults(GPatchingTarget, PlatformEngineConfigFilename, PlatformSystemSettingsConfigFilename);
	}
	else
	{
		// Use the current engine ini file on the PC.
		appStrcpy( PlatformEngineConfigFilename, GEngineIni );
		appStrcpy( PlatformSystemSettingsConfigFilename, GSystemSettingsIni );
	}

	// call the shared function to get all native script package names
	appGetScriptPackageNames(NativeScriptPackageNames, SPT_Native | (!bStrippedScriptPatch ? SPT_Editor : 0) | SPT_SeekfreeLoc, PlatformEngineConfigFilename);
	// it's possible that Startup has always loaded script code in it, so add them to the list of 'native' script packages
	NativeScriptPackageNames.AddItem(FString(TEXT("Startup_LOC")));
	NativeScriptPackageNames.AddItem(FString(TEXT("Startup")));

	appGetScriptPackageNames(NonNativeScriptPackageNames, SPT_NonNative);

	//@script patcher todo: might need to provide a way to add game-specific special packages...might even be able to just use the PackagesToAlwaysCook list or something
// 	NonNativeScriptPackageNames.AddItem(TEXT("WarfareGameContentWeapons"));

	// now process the command line parameters to determine which files we're patching
	FString PackageName, OriginalDirectory, CurrentDirectory;
	if ( Tokens.Num() > 0 )
	{
		PackageName = Tokens(0);
	}
	else
	{
		// default to patching for PC
		PackageName = TEXT("*.u");
	}

	FString DefaultScriptDir, DefaultConformSourceDir;
	if ( Tokens.Num() > 1 )
	{
		CurrentDirectory = Tokens(1);

		// if xenon, set proper compression
		if ( GPatchingTarget & UE3::PLATFORM_Xbox360 )
		{
			GBaseCompressionMethod = COMPRESS_DefaultXbox360;
		}
	}
	else
	{
		TArray<FString> ScriptDirectories;
		appGetScriptPackageDirectories(ScriptDirectories);
		check(ScriptDirectories.Num() > 0);

		//@script patcher fixme: add support for multiple script directories
		DefaultScriptDir = CurrentDirectory = ScriptDirectories(0);
	}

	if ( Tokens.Num() > 2 )
	{
		OriginalDirectory = Tokens(2);
	}
	else
	{
		//@script patcher todo: make this an .ini option?
		DefaultConformSourceDir = OriginalDirectory = appGameDir() * TEXT("ScriptOriginal");
	}

	// preserve any path information

	// make sure the directory containing the current script files exists
	TArray<FString> DirectoryPath;
	GFileManager->FindFiles(DirectoryPath, *CurrentDirectory, 0, 1);
	if ( DirectoryPath.Num() == 0 )
	{
		warnf(NAME_Error, TEXT("You must specify a valid location for the current package files!"));
		return FALSE;
	}

	// make sure the directory containing the original script files exists
	GFileManager->FindFiles(DirectoryPath, *OriginalDirectory, 0, 1);
	if ( DirectoryPath.Num() == 0 )
	{
		warnf(NAME_Error, TEXT("You must specify a valid location for the original package files or place the package in the %s directory!"), *DefaultConformSourceDir);
		return FALSE;
	}

	// reset all loaders so we can load up the .u packages all nice and safe; set the script patch flag so that ResetLoaders doesn't
	// force all bulk data to be loaded
	GIsScriptPatcherActive = TRUE;
	UObject::ResetLoaders(NULL);

	// we have to unset it because any objects which have already been loaded by this point did so without the flag set, which means that
	// all of their object references and FNames were serialized normally - therefore we don't want them to serialize those references using
	// the crazy "don't really convert objects/names into objects/names" scheme we have to do for patching when we perform our first garbage
	// collection below, so we temporarily turn off the script patch flag
	GIsScriptPatcherActive = FALSE;

	// Collect garbage to free up the memory associated with the linkers we just reset and get rid of those objects
	UObject::CollectGarbage(RF_Native);

	// now set the flag which will affect the way that serialization and package loading works
	GIsScriptPatcherActive = TRUE;


	// Now, we build the list of package pathnames that will be included in the patch.

	// array of package names found in the current directory - these are the only ones we patch
	TArray<FString> UnsortedPackageNames;
	GFileManager->FindFiles(UnsortedPackageNames, *(CurrentDirectory * PackageName), TRUE, FALSE);
	if ( UnsortedPackageNames.Num() == 0 )
	{
		warnf(NAME_Error, TEXT("No files matching '%s' found in specified directory: %s"), *PackageName, *CurrentDirectory);
		return FALSE;
	}

	PackageNamesToPatch.Empty();
	LatestPackagePathnames.Empty(PackageNamesToPatch.Num());
	OriginalPackagePathnames.Empty(PackageNamesToPatch.Num());

	FString PackageExtension = FFilename(PackageName).GetExtension();

	// build the list of pathnames for the packages in each location, if the file exists, starting with the native script packages,
	// then the non-native script package names, then the rest of them.
	for ( INT NativePackageIndex = 0; NativePackageIndex < NativeScriptPackageNames.Num(); NativePackageIndex++ )
	{
		FString ScriptPackageName = NativeScriptPackageNames(NativePackageIndex);
		if ( ScriptPackageName.Right(appStrlen(LOCALIZED_SEEKFREE_SUFFIX)) == LOCALIZED_SEEKFREE_SUFFIX )
		{
			ScriptPackageName += TEXT("_");
			const INT PackageNameLength = ScriptPackageName.Len();
			for ( INT FileIndex = UnsortedPackageNames.Num() - 1; FileIndex >= 0; FileIndex-- )
			{
				const FString& Filename = UnsortedPackageNames(FileIndex);
				if ( Filename.Len() >= PackageNameLength && Filename.Left(PackageNameLength) == ScriptPackageName )
				{
					PackageNamesToPatch.AddItem(UnsortedPackageNames(FileIndex));
					UnsortedPackageNames.Remove(FileIndex);
				}
			}
		}
		else
		{
			ScriptPackageName = ScriptPackageName + TEXT(".") + PackageExtension;
			PackageNamesToPatch.AddItem(ScriptPackageName);

			INT ListIndex = UnsortedPackageNames.FindItemIndex(ScriptPackageName);
			if ( ListIndex != INDEX_NONE )
			{
				UnsortedPackageNames.Remove(ListIndex);
			}
		}
	}
	for ( INT NonnativePackageIndex = 0; NonnativePackageIndex < NonNativeScriptPackageNames.Num(); NonnativePackageIndex++ )
	{
		FString ScriptPackageName = NonNativeScriptPackageNames(NonnativePackageIndex);
		if ( ScriptPackageName.Right(appStrlen(LOCALIZED_SEEKFREE_SUFFIX)) == LOCALIZED_SEEKFREE_SUFFIX )
		{
			ScriptPackageName += TEXT("_");
			const INT PackageNameLength = ScriptPackageName.Len();
			for ( INT FileIndex = UnsortedPackageNames.Num() - 1; FileIndex >= 0; FileIndex-- )
			{
				const FString& Filename = UnsortedPackageNames(FileIndex);
				if ( Filename.Len() >= PackageNameLength && Filename.Left(PackageNameLength) == ScriptPackageName )
				{
					PackageNamesToPatch.AddItem(UnsortedPackageNames(FileIndex));
					UnsortedPackageNames.Remove(FileIndex);
				}
			}
		}
		else
		{
			ScriptPackageName = ScriptPackageName + TEXT(".") + PackageExtension;
			PackageNamesToPatch.AddItem(ScriptPackageName);

			INT ListIndex = UnsortedPackageNames.FindItemIndex(ScriptPackageName);
			if ( ListIndex != INDEX_NONE )
			{
				UnsortedPackageNames.Remove(ListIndex);
			}
		}
	}

	for ( INT ContentIndex = 0; ContentIndex < UnsortedPackageNames.Num(); ContentIndex++ )
	{
		FFilename ContentPackageName = UnsortedPackageNames(ContentIndex);
		INT LocSuffixPosition = ContentPackageName.InStr(LOCALIZED_SEEKFREE_SUFFIX);
		if ( LocSuffixPosition != INDEX_NONE )
		{
			FFilename NonLocContentPackageName = ContentPackageName.Left(LocSuffixPosition) + TEXT(".") + ContentPackageName.GetExtension();
			INT NonLocArrayIndex = UnsortedPackageNames.FindItemIndex(NonLocContentPackageName);
			if ( NonLocArrayIndex != INDEX_NONE )
			{
				check(NonLocArrayIndex!=ContentIndex);
				if ( NonLocArrayIndex < ContentIndex )
				{
					UnsortedPackageNames.Remove(ContentIndex);
					UnsortedPackageNames.InsertItem(ContentPackageName, NonLocArrayIndex);
				}
				else
				{
					UnsortedPackageNames.InsertItem(ContentPackageName, NonLocArrayIndex);
					UnsortedPackageNames.Remove(ContentIndex--);
				}
			}
		}
	}

	// now just add the rest to the end of the array
	PackageNamesToPatch += UnsortedPackageNames;
	UnsortedPackageNames.Empty();

	for ( INT PackageIndex = 0; PackageIndex < PackageNamesToPatch.Num(); PackageIndex++ )
	{
		// results returned from GFileManager->FindFiles will not contain path information,
		// so we need to re-add it here
		FFilename PathToCurrent = CurrentDirectory * PackageNamesToPatch(PackageIndex);
		FFilename PathToOriginal = OriginalDirectory * PackageNamesToPatch(PackageIndex);
		if ( GFileManager->FileSize(*PathToCurrent) > 0 && GFileManager->FileSize(*PathToOriginal) > 0 )
		{
			LatestPackagePathnames.AddItem(PathToCurrent);
			OriginalPackagePathnames.AddItem(PathToOriginal);
		}
	}

	if ( LatestPackagePathnames.Num() == 0 || OriginalPackagePathnames.Num() == 0 || LatestPackagePathnames.Num() != OriginalPackagePathnames.Num() )
	{
		warnf(NAME_Error, TEXT("Some files couldn't be found in one or both locations (Current:%i  Original:%i)"), LatestPackagePathnames.Num(), OriginalPackagePathnames.Num());
		warnf(TEXT("Files found in \"current\" directory:"));
		for ( INT idx = 0; idx < LatestPackagePathnames.Num(); idx++ )
		{
			warnf(TEXT("%i) %s"), idx, *LatestPackagePathnames(idx));
		}

		warnf(TEXT("\r\nFiles found in \"original\" directory:"));
		for ( INT idx = 0; idx < OriginalPackagePathnames.Num(); idx++ )
		{
			warnf(TEXT("%i) %s"), idx, *OriginalPackagePathnames(idx));
		}

		return FALSE;
	}

	return TRUE;
}

/**
 * Setup the commandlet's platform setting based on commandlet params
 * @param Params The commandline parameters to the commandlet - should include "platform=xxx"
 */
UBOOL FScriptPatchWorker::SetPlatform(const FString& Params)
{
	// default to success
	UBOOL Ret = TRUE;

	FString PlatformStr;
	if (Parse(*Params, TEXT("PLATFORM="), PlatformStr))
	{
		if (PlatformStr == TEXT("PS3"))
		{
			GPatchingTarget = UE3::PLATFORM_PS3;
			bStrippedScriptPatch = TRUE;
			bNonIntelScriptPatch = TRUE;
		}
		else if (PlatformStr == TEXT("xenon") || PlatformStr == TEXT("xbox360"))
		{	
			GPatchingTarget = UE3::PLATFORM_Xbox360;
			bStrippedScriptPatch = TRUE;
			bNonIntelScriptPatch = TRUE;
		}
		else if (PlatformStr == TEXT("pc") || PlatformStr == TEXT("win32"))
		{
			GPatchingTarget = UE3::PLATFORM_Windows;
		}
		else if (PlatformStr == TEXT("pcconsole") || PlatformStr == TEXT("win32console"))
		{
			GPatchingTarget = UE3::PLATFORM_WindowsConsole;
			bStrippedScriptPatch = TRUE;
		}
		else if (PlatformStr == TEXT("pcserver") || PlatformStr == TEXT("win32server"))
		{
			GPatchingTarget = UE3::PLATFORM_WindowsServer;
			bStrippedScriptPatch = TRUE;
		}
		else
		{
			SET_WARN_COLOR(COLOR_RED);
			warnf(NAME_Error, TEXT("Unknown platform!"));
			CLEAR_WARN_COLOR();

			// this is a failure
			Ret = FALSE;
		}
	}
	else
	{
		Ret = FALSE;
	}

	return Ret;
}


void FScriptPatchWorker::UpdatePackageCache( const TArray<FFilename>& PackagePathnames )
{
	for ( INT PackageIndex = 0; PackageIndex < PackagePathnames.Num(); PackageIndex++ )
	{
		const FFilename& PackageFilename = PackagePathnames(PackageIndex);

		FString PackageName = PackageFilename.GetBaseFilename();
		if ( bStrippedScriptPatch && IsNonNativeScriptPackage(PackageName) )
		{
			debugf(TEXT("Skipping non-native script package %s"), *PackageFilename);
			continue;
		}
		GPackageFileCache->CachePackage( *PackageFilename, TRUE, FALSE );
	}
}

void FScriptPatchWorker::RemapLoadingToOriginalFiles()
{
	GScriptPatchPackageSuffix = OriginalPackageSuffix;
	UpdatePackageCache(OriginalPackagePathnames);
}
void FScriptPatchWorker::RemapLoadingToLatestFiles()
{
	GScriptPatchPackageSuffix = LatestPackageSuffix;
	UpdatePackageCache(LatestPackagePathnames);
}
void FScriptPatchWorker::ResetPackageRemapping()
{
	GScriptPatchPackageSuffix.Empty();
	GPackageFileCache->CachePaths();
}

INT FScriptPatchWorker::FindNativePackageComparison( const FString PackageName ) const
{
	INT Result = INDEX_NONE;
	for ( INT NativePackageIndex = 0; NativePackageIndex < NativePackageComparisons.Num(); NativePackageIndex++ )
	{
		FPackageComparison* Comparison = NativePackageComparisons(NativePackageIndex);
		if ( Comparison->PackageName == PackageName )
		{
			Result = NativePackageIndex;
			break;
		}
	}

	return Result;
}

void FScriptPatchWorker::MoveIntrinsicClasses( FName PackageName, UPackage* TargetOuter )
{
	if ( PackageName != NAME_None && TargetOuter != NULL )
	{
		TArray<UClass*> IntrinsicClasses;
		IntrinsicClassMap.MultiFind(PackageName, IntrinsicClasses);
		for ( INT IntrinsicClassIndex = 0; IntrinsicClassIndex < IntrinsicClasses.Num(); IntrinsicClassIndex++ )
		{
			UClass* IntrinsicClass = IntrinsicClasses(IntrinsicClassIndex);
			IntrinsicClass->Rename(*IntrinsicClass->GetName(), TargetOuter, REN_ForceNoResetLoaders);
		}
	}
	else
	{
		TMap<FName,UPackage*> PackageCache;
		for ( TMultiMap<FName,UClass*>::TIterator It(IntrinsicClassMap); It; ++It )
		{
			PackageName = It.Key();
			FString PackageNameString = PackageName.ToString();
			if ( IsNativeScriptPackage(PackageNameString) )
			{
				UClass* IntrinsicClass = It.Value();
				check(IntrinsicClass);

				TargetOuter = PackageCache.FindRef(PackageName);
				if ( TargetOuter == NULL )
				{
					TargetOuter = UObject::FindPackage(NULL, *PackageNameString);
                    checkf(TargetOuter, TEXT("Couldn't find corresponding package for %s"), *PackageNameString);

					PackageCache.Set(PackageName, TargetOuter);
				}

				IntrinsicClass->Rename(*IntrinsicClass->GetName(), TargetOuter, REN_ForceNoResetLoaders);
			}
		}
	}
}

void FScriptPatchWorker::GenerateNativePackageComparisons( FScriptPatchExporter& Writer )
{
	for ( INT i = 0; i < NativePackageComparisons.Num(); i++ )
	{
		delete NativePackageComparisons(i);
	}
	NativePackageComparisons.Empty();

	// initialize the lookup map for intrinsic classes
	IntrinsicClassMap.Empty();
	for ( TObjectIterator<UClass> It; It; ++It )
	{
		UClass* Class = *It;
		if ( Class->HasAnyClassFlags(CLASS_Intrinsic) && Class->GetOuter()->GetFName() != NAME_Core )
		{
			IntrinsicClassMap.Add(Class->GetOuter()->GetFName(), Class);
		}
	}

	// first, do the original packages
	RemapLoadingToOriginalFiles();
	for ( INT FileIndex = 0; FileIndex < OriginalPackagePathnames.Num(); FileIndex++ )
	{
		const FFilename& PackagePath = OriginalPackagePathnames(FileIndex);
		FString PackageName = PackagePath.GetBaseFilename();

		if ( IsNativeScriptPackage(PackageName) && PackageName != TEXT("Core") )
		{
			// FindObject will find the original version of the package (i.e. the one without the _OriginalVer on the end), while
			// CreatePackage will always find the new version, if it exists.
			UPackage* NativePackage = UObject::CreatePackage( NULL, *PackageName, TRUE );

			// rename all intrinsic classes contained in the original version of this package into the new version of the package
			MoveIntrinsicClasses(*PackageName, NativePackage);

			// now load the file using this alternate package as the LinkerRoot
			warnf(TEXT("Loading original package '%s'"), *PackagePath);
			NativePackage = UObject::LoadPackage( NativePackage, *PackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage );

			// then grab the linker
			ULinkerLoad* Linker = UObject::GetPackageLinker( NativePackage, *PackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage, NULL, NULL );
			check(Linker);

			INT idx = NativePackageComparisons.AddZeroed();
			FPackageComparison* PackageComparison = NativePackageComparisons(idx) = new FPackageComparison(PackageName);
			PackageComparison->PackageName = PackageName;
			PackageComparison->OriginalPackage = FPackageData(Linker, PackageName);

			// ensure that none of this linker's exports are GC'd
			for ( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap(ExportIndex);
				if ( Export._Object != NULL )
				{
					Export._Object->AddToRoot();
				}
			}
		}
	}

	// probably don't even need to do this part once we've found all the places where package names are requested without using ResolveName or appending the GScriptPatchPackageSuffix
	RemapLoadingToLatestFiles();
	for ( INT FileIndex = 0; FileIndex < LatestPackagePathnames.Num(); FileIndex++ )
	{
		const FFilename& PackagePath = LatestPackagePathnames(FileIndex);
		FString PackageName = PackagePath.GetBaseFilename();
		if ( IsNativeScriptPackage(PackageName) && PackageName != TEXT("Core") )
		{
 			UPackage* NativePackage = UObject::CreatePackage( NULL, *PackageName, TRUE );

			// rename all intrinsic classes contained in the original version of this package into the new version of the package
			MoveIntrinsicClasses(*PackageName, NativePackage);

			// now load the file using this alternate package as the LinkerRoot
			warnf(TEXT("Loading current package '%s'"), *PackagePath);
			NativePackage = UObject::LoadPackage( NativePackage, *PackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage );

			// then grab the linker
			ULinkerLoad* Linker = UObject::GetPackageLinker( NativePackage, *PackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage, NULL, NULL );
			check(Linker);

			INT ComparisonIdx = FindNativePackageComparison(PackageName);
			check(NativePackageComparisons.IsValidIndex(ComparisonIdx));

			FPackageComparison* Comparison = NativePackageComparisons(ComparisonIdx);
			Comparison->ComparisonResults.PackageLinker = Linker;
			Comparison->ComparisonResults.OriginalExportCount = Comparison->OriginalPackage.Exports.Num();
			Comparison->CurrentPackage = FPackageData(Linker, PackageName);

			// ensure that none of this linker's exports are GC'd
			for ( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
			{
				const FObjectExport& Export = Linker->ExportMap(ExportIndex);
				if ( Export._Object != NULL )
				{
					Export._Object->AddToRoot();
				}
			}
		}
	}


	// now perform the comparisons
	for ( INT NativePackageIndex = 0; NativePackageIndex < NativePackageComparisons.Num(); NativePackageIndex++ )
	{
		FPackageComparison* Comparison = NativePackageComparisons(NativePackageIndex);
		warnf(TEXT("%s================================================================%s"), LINE_TERMINATOR, LINE_TERMINATOR);
		warnf( TEXT( "Comparing: %s to %s"), *Comparison->CurrentPackage.FileName, *Comparison->OriginalPackage.FileName );
		Comparison->ComparePackages();

		if ( !Switches.ContainsItem(TEXT("nosummary")) )
		{
			FOutputDevice* OutputDevice;
			if ( Switches.ContainsItem(TEXT("suppressoutput")) )
			{
				OutputDevice = GLog;
			}
			else
			{
				OutputDevice = GWarn;
			}
			Comparison->DisplaySummaries(OutputDevice);
		}
		Comparison->DisplayComparisonResults();

		// output the results of the comparison in both intel and non-intel formats
		Comparison->OutputComparisonResults(Writer);

		if (Comparison->ComparisonResults)
		{
			Comparison->ComparisonResults.BuildStructureForBinary(Writer.AllPackagePatches, Writer.bNonIntelByteOrder);
		}
	}

	// try to reclaim as much memory as possible
	UObject::CollectGarbage(RF_Native);
	ResetPackageRemapping();
}

void FScriptPatchWorker::GenerateNonNativePackageComparisons( FScriptPatchExporter& Writer )
{
	// first, do the original packages
	for ( INT FileIndex = 0; FileIndex < LatestPackagePathnames.Num(); FileIndex++ )
	{
		const FFilename& LatestPackagePath = LatestPackagePathnames(FileIndex);
		FString PackageName = LatestPackagePath.GetBaseFilename();

		if ( !IsNativeScriptPackage(PackageName) && !IsNonNativeScriptPackage(PackageName) )
		{
			//@script patcher todo: =what if FileIndex > OriginalPackagePathnames.Num()?
			const FFilename& OriginalPackagePath = OriginalPackagePathnames(FileIndex);
			// assert for now - later we'll need to handle this gracefully....but it shouldn't be able to happen
			check(PackageName==OriginalPackagePath.GetBaseFilename());

			RemapLoadingToOriginalFiles();

			UPackage* OriginalPackage = UObject::CreatePackage( NULL, *PackageName, TRUE );

			// rename all intrinsic classes contained in the non-cooked version of all native packages into the original cooked version of those package
			MoveIntrinsicClasses(NAME_None, NULL);

			// now load the file using this alternate package as the LinkerRoot
			debugf(TEXT("Loading original package '%s'"), *OriginalPackagePath);
			OriginalPackage = UObject::LoadPackage( OriginalPackage, *OriginalPackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage );

			// then grab the linker
			ULinkerLoad* OriginalLinker = UObject::GetPackageLinker( OriginalPackage, *OriginalPackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage, NULL, NULL );
			check(OriginalLinker);


			// now load the latest version of the package
			RemapLoadingToLatestFiles();
			UPackage* LatestPackage = UObject::CreatePackage( NULL, *PackageName, TRUE );

			// rename all intrinsic classes contained in the original cooked version of all native packages into the new version of those packages
			MoveIntrinsicClasses(NAME_None, NULL);

			// now load the file using this alternate package as the LinkerRoot
			debugf(TEXT("Loading current package '%s'"), *LatestPackagePath);
			LatestPackage = UObject::LoadPackage( LatestPackage, *LatestPackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage );

			// then grab the linker
			ULinkerLoad* LatestLinker = UObject::GetPackageLinker( LatestPackage, *LatestPackagePath, LOAD_NoWarn|LOAD_NoVerify|LOAD_RemappedPackage, NULL, NULL );
			check(LatestLinker);

			// now compare the packages
			{
				FPackageData OriginalPackageData(OriginalLinker, PackageName), LatestPackageData(LatestLinker, PackageName);
				FPackageComparison Comparison(LatestPackageData, OriginalPackageData);
				Comparison.ComparisonResults.PackageLinker = LatestLinker;
				Comparison.ComparisonResults.OriginalExportCount = OriginalLinker->ExportMap.Num();

				warnf(TEXT("%s================================================================%s"), LINE_TERMINATOR, LINE_TERMINATOR);
				warnf( TEXT( "Comparing: %s to %s"), *Comparison.CurrentPackage.FileName, *Comparison.OriginalPackage.FileName );
				Comparison.ComparePackages();

				if ( !Switches.ContainsItem(TEXT("nosummary")) )
				{
					FOutputDevice* OutputDevice;
					if ( Switches.ContainsItem(TEXT("suppressoutput")) )
					{
						OutputDevice = GLog;
					}
					else
					{
						OutputDevice = GWarn;
					}
					Comparison.DisplaySummaries(OutputDevice);
				}
				Comparison.DisplayComparisonResults();
				Comparison.OutputComparisonResults(Writer);

				if (Comparison.ComparisonResults)
				{
					Comparison.ComparisonResults.BuildStructureForBinary(Writer.AllPackagePatches, Writer.bNonIntelByteOrder);
				}
			}

			//@script patcher fixme: is the shared MP localized sounds package always going to be called MPSounds or
			// is this an .ini option somewhere?
			if ( LatestPackagePath.InStr(LOCALIZED_SEEKFREE_SUFFIX) != INDEX_NONE
			||	LatestPackagePath.InStr(TEXT("MPSounds")) != INDEX_NONE 
			||	LatestPackagePath.InStr(TEXT("EngineMaterials")) != INDEX_NONE )
			{
				// add package to root so we can find it
				OriginalPackage->AddToRoot();
				LatestPackage->AddToRoot();

				// add the objects to the root set so that it will not be GC'd
				for (TObjectIterator<UObject> It; It; ++It)
				{
					if (It->IsIn(LatestPackage) || It->IsIn(OriginalPackage) )
					{
						It->AddToRoot();
					}
				}
				// don't GC if this package is a loc package as the next package will need the assets from this one
				continue;
			}

			// ok - we're done with this comparison; reset the loaders for these packages and collect garbage to
			// try to reclaim as much memory as possible
			UObject::ResetLoaders(OriginalPackage);
			UObject::ResetLoaders(LatestPackage);

			UObject::CollectGarbage(RF_Native);
		}
	}

	ResetPackageRemapping();
}

/**
 * Determines whether the specified package is contained in the NativeScriptPackageNames array.
 *
 * @param	PackageName		the pathname for the package to check; all path and extension information will be removed before the search.
 */
UBOOL FScriptPatchWorker::IsNativeScriptPackage( FFilename PackageName ) const
{
	UBOOL bResult = FALSE;

	if ( PackageName.Len() > 0 )
	{
		// first, remove all path and extension info
		PackageName = PackageName.GetBaseFilename();

		// now remove any localization suffixes
		INT CutLocation = PackageName.InStr(LOCALIZED_SEEKFREE_SUFFIX);
		if ( CutLocation != INDEX_NONE )
		{
			PackageName = PackageName.Left(CutLocation);
		}
		
		bResult = NativeScriptPackageNames.ContainsItem(PackageName);
	}

	return bResult;
}

/**
 * Determines whether the specified package is contained in the NonNativeScriptPackageNames array.
 *
 * @param	PackageName		the pathname for the package to check; all path and extension information will be removed before the search.
 */
UBOOL FScriptPatchWorker::IsNonNativeScriptPackage( FFilename PackageName ) const
{
	UBOOL bResult = FALSE;

	if ( PackageName.Len() > 0 )
	{
		// first, remove all path and extension info
		PackageName = PackageName.GetBaseFilename();

		// now remove any localization suffixes
		INT CutLocation = PackageName.InStr(LOCALIZED_SEEKFREE_SUFFIX);
		if ( CutLocation != INDEX_NONE )
		{
			PackageName = PackageName.Left(CutLocation);
		}

		bResult = NonNativeScriptPackageNames.ContainsItem(PackageName);
	}

	return bResult;
}

#endif // !CONSOLE

//=============================================================================================================================
void UPatchScriptCommandlet::StaticInitialize()
{
	Worker = NULL;
	IsClient = FALSE;
	LogToConsole = TRUE;
}

INT UPatchScriptCommandlet::Main( const FString& Params )
{
	INT Result = 0;
#if !CONSOLE
	// use this wrapper class so we don't have to keep recompiling everything due to the commandlet declaration being in EditorCommandlets.h
	Worker = new FScriptPatchWorker;
	Result = Worker->Main(Params);

	delete Worker;
	Worker = NULL;
#endif // !CONSOLE
	return Result;
}


IMPLEMENT_CLASS(UPatchScriptCommandlet);

