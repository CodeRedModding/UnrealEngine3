/*=============================================================================
	LocalizationExport.cpp: Implementation of localized property export.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "LocalizationExport.h"
#include "FConfigCacheIni.h"

/** Temporary used to track the number of properties output during a localization dump. */
static INT GPropertyCount = 0;

const TCHAR* FLocalizationExportFilter::FilterDelimiter = TEXT("|||");

/** Default constructor */
FLocalizationExportFilter::FLocalizationExportFilter()
: TagFilterType( TFT_MatchAny )
{
}

/**
 * Construct from a specially formatted string
 *
 * @param	InInitString	String to construct from, formatted via the ToString method
 */
FLocalizationExportFilter::FLocalizationExportFilter( const FString& InInitString )
{
	// Split the string based on the filter delimiter
	InInitString.ParseIntoArray( &FilterTags, FilterDelimiter, TRUE );
	check( FilterTags.Num() > 0 );

	// The first entry should be the filter type, set it and remove it from the tags array
	TagFilterType = static_cast<ETagFilterType>( appAtoi( *FilterTags( 0 ) ) );
	FilterTags.Remove( 0 );
}

/**
 * Constructor
 *
 * @param	InFilterTags	Filter tags to use
 * @param	InTagFilterType	Filter type to use
 */
FLocalizationExportFilter::FLocalizationExportFilter( const TArray<FString>& InFilterTags, ETagFilterType InTagFilterType )
:	FilterTags( InFilterTags ),
	TagFilterType( InTagFilterType )
{
}

/**
 * Returns the string representation of the filter. Can be used to reconstruct a filter object.
 *
 * @return	String representation of the filter
 */
FString FLocalizationExportFilter::ToString() const
{
	FString TagString;
	for ( TArray<FString>::TConstIterator TagIter( FilterTags ); TagIter; ++TagIter )
	{
		TagString += *TagIter;
		TagString += FilterDelimiter;
	}
	return FString::Printf( TEXT("%d%s%s"), static_cast<INT>( TagFilterType ), FilterDelimiter, *TagString );
}

/**
 * Simple accessor to tag filter type
 *
 * @return	The tag filter type
 */
FLocalizationExportFilter::ETagFilterType FLocalizationExportFilter::GetTagFilterType() const
{
	return TagFilterType;
}

/**
 * Simple mutator to tag filter type
 *
 * @param	InType	New tag filter type to use
 */
void FLocalizationExportFilter::SetTagFilterType( ETagFilterType InType )
{
	TagFilterType = InType;
}

/**
 * Const accessor to the filter tag array
 *
 * @return Const reference to filter tag array
 */
const TArray<FString>& FLocalizationExportFilter::GetFilterTags() const
{
	return FilterTags;
}

/**
 * Non-const accessor to the filter tag array
 *
 * @return Reference to filter tag array
 */
TArray<FString>& FLocalizationExportFilter::GetFilterTags()
{
	return FilterTags;
}


/**
 * Generates an .int name from the package name.
 */
void FLocalizationExport::GenerateIntNameFromPackageName(const FString &PackageName, FString &OutIntName)
{
	OutIntName = PackageName;

	INT i = OutIntName.InStr(TEXT ("."), TRUE);
	if (i >= 0)
	{
		OutIntName = OutIntName.Left(i);
	}

	OutIntName += TEXT (".int");

	i = OutIntName.InStr( TEXT("/"), TRUE );
	if (i >= 0)
	{
		OutIntName = OutIntName.Mid(i+1);
	}

	i = OutIntName.InStr(TEXT ("\\"), TRUE);
	if (i >= 0)
	{
		OutIntName = OutIntName.Mid(i+1);
	}

	OutIntName = FString(appBaseDir()) + OutIntName;
}

/**
 * @param	Class						Class of the object being exported.
 * @param	SuperClass					Superclass of the object being exported.
 * @param	OuterClass
 * @param	Prop						Array property to export.
 * @param	IntName						Name of the .int file.
 * @param	SectionName					Config section to export to.
 * @param	KeyPrefix					Config key (property name) to export to.
 * @param	DataBase					Base data address for the data.
 * @param	DataOffset					Offset from the base address.
 * @param	bAtRoot
 * @param	bCompareAgainstDefaults		If TRUE, don't export the property unless it differs from defaults.
 */
void FLocalizationExport::ExportDynamicArray(UClass*			Class,
											 UClass*			SuperClass, 
											 UClass*			OuterClass, 
											 UArrayProperty*	Prop, 
											 const TCHAR*		IntName, 
											 const TCHAR*		SectionName, 
											 const TCHAR*		KeyPrefix, 
											 BYTE*				DataBase, 
											 INT				DataOffset, 
											 UBOOL				bAtRoot,
											 UBOOL				bCompareAgainstDefaults)
{
	// If requested, don't export the property if not different from defaults.
	BYTE* DefaultData = NULL;
	if ( bCompareAgainstDefaults )
	{
		if ( SuperClass && SuperClass->IsChildOf(OuterClass) )
		{
			DefaultData = (BYTE*)SuperClass->GetDefaultObject();
		}
		if ( DefaultData && DefaultData != DataBase )
		{
			if ( Prop->Identical(DataBase + DataOffset, DefaultData + DataOffset) )
			{
				return;
			}
		}
	}

	// Get array of properties (this is the start of the array property)
	FScriptArray* Ptr				= (FScriptArray*)(DataBase + DataOffset);
	FScriptArray* DefaultArray	= DefaultData ? (FScriptArray*)(DefaultData + DataOffset) : NULL;

	// This is used as the default value in the case of an array property that has
	// fewer elements than the exported object.
	BYTE* StructDefaults = NULL;
	if ( Cast<UStructProperty>(Prop->Inner,CLASS_IsAUStructProperty) != NULL )
	{
		UScriptStruct* InnerStruct = static_cast<UStructProperty*>(Prop->Inner)->Struct;
		checkSlow(InnerStruct);
		StructDefaults = InnerStruct->GetDefaults();
	}

	// Get the size of each element
	const INT Size	= Prop->Inner->ElementSize;

	// For each item in the array
	for( INT i = 0; i < Ptr->Num(); i++ )
	{
		// Get the address of the data we are exporting
		// Manually walk across the array by index * size
		BYTE* Dest = (BYTE*)Ptr->GetData() + i * Size;

		// The value to diff against will be either the value at that array element in the default,
		// or the property default value if the default array is too small.
		BYTE* PropDefault = (DefaultArray && DefaultArray->Num() > i)
			? (BYTE*)DefaultArray->GetData() + i * Size
			: StructDefaults;

		// Export the property into buffer
		FString RealValue;
		Prop->Inner->ExportTextItem( RealValue, Dest, PropDefault, NULL, PPF_Delimited|PPF_LocalizedOnly );

		if ( RealValue.Len() > 0 )
		{
			// Add array index number to end of key
			const FString IndexedKey( FString::Printf( TEXT("%s[%i]"), KeyPrefix, i ) );
			GConfig->SetString( SectionName, *IndexedKey, *RealValue, IntName );
		}
	}

	GPropertyCount++;
}

/**
* @param	Class						Class of the object being exported.
* @param	SuperClass					Superclass of the object being exported.
* @param	OuterClass
* @param	Prop						Property to export.
* @param	IntName						Name of the .int file.
* @param	SectionName					Config section to export to.
* @param	KeyPrefix					Config key (property name) to export to.
* @param	DataBase					Base data address for the data.
* @param	DataOffset					Offset from the base address.
* @param	bCompareAgainstDefaults		If TRUE, don't export the property unless it differs from defaults.
* @param	bDumpEmptyProperties		If TRUE, export properties with zero length/empty strings.
*/
void FLocalizationExport::ExportProp(UClass*		Class, 
									 UClass*		SuperClass, 
									 UClass*		OuterClass, 
									 UProperty*		Prop, 
									 const TCHAR*	IntName, 
									 const TCHAR*	SectionName, 
									 const TCHAR*	KeyPrefix, 
									 BYTE*			DataBase, 
									 INT			DataOffset,
									 UBOOL			bCompareAgainstDefaults,
									 UBOOL			bDumpEmptyProperties)
{
	// If property is a struct . . .
	UStructProperty* StructProperty = Cast<UStructProperty>( Prop );
	if( StructProperty )
	{
		// Export it as such and exit.
		ExportStruct( Class, SuperClass, OuterClass,
						StructProperty->Struct,
						IntName, SectionName, KeyPrefix,
						DataBase, DataOffset, FALSE,
						bCompareAgainstDefaults, bDumpEmptyProperties );
		return;
	}

	// If property is a dynamic array . . .
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>( Prop );
	if( ArrayProperty )
	{
		// Export it as such and exit.
		ExportDynamicArray( Class, SuperClass, OuterClass,
							ArrayProperty,
							IntName, SectionName, KeyPrefix,
							DataBase, DataOffset, FALSE,
							bCompareAgainstDefaults );
		return;
	}

	// If requested, don't export the property if not different from defaults.
	BYTE* DefaultData = NULL;
	if ( bCompareAgainstDefaults )
	{
		if ( SuperClass && SuperClass->IsChildOf(OuterClass) )
		{
			DefaultData = (BYTE*)SuperClass->GetDefaultObject();
		}

		if ( DefaultData && DefaultData != DataBase )
		{
			if ( Prop->Identical(DataBase + DataOffset, DefaultData + DataOffset) )
			{
				return;
			}
		}
	}

	// Ask the property to format itself into a string assigned into the RealValue out string with delimiters included.
	FString RealValue;
	Prop->ExportTextItem( RealValue, DataBase + DataOffset, DefaultData ? DefaultData + DataOffset : NULL, NULL, PPF_Delimited|PPF_LocalizedOnly );

	// If property value is empty (zero length or empty string) - exit unless
	// the caller requested that empty properties be output.
	const INT RealLength		= RealValue.Len();
	const UBOOL bIsEmptyString	= appStrcmp( *RealValue, TEXT("\"\"") ) == 0;

	if( RealLength == 0 || bIsEmptyString )
	{
		if ( bDumpEmptyProperties )
		{
			RealValue = TEXT("\"\"");
		}
		else
		{
			return;
		}
	}

	// File output should be:
	// [SectionName]
	// <KeyName>=<RealValue>
	GConfig->SetString( SectionName, KeyPrefix, *RealValue, IntName );

	// Update exported property count.
	GPropertyCount++;
}

/**
 * @param	Class						Class of the object being exported.
 * @param	SuperClass					Superclass of the object being exported.
 * @param	OuterClass
 * @param	Struct						Struct property to export.
 * @param	IntName						Name of the .int file.
 * @param	SectionName					Config section to export to.
 * @param	KeyPrefix					Config key (property name) to export to.
 * @param	DataBase					Base data address for the data.
 * @param	DataOffset					Offset from the base address.
 * @param	bAtRoot
 * @param	bCompareAgainstDefaults		If TRUE, don't export the property unless it differs from defaults.
 * @param	bDumpEmptyProperties		If TRUE, export properties with zero length/empty strings.
 */
void FLocalizationExport::ExportStruct(UClass*		Class,
									   UClass*		SuperClass,
									   UClass*		OuterClass,
									   UStruct*		Struct,
									   const TCHAR*	IntName,
									   const TCHAR*	SectionName,
									   const TCHAR*	KeyPrefix,
									   BYTE*		DataBase,
									   INT			DataOffset,
									   UBOOL		bAtRoot,
									   UBOOL		bCompareAgainstDefaults,
									   UBOOL		bDumpEmptyProperties)
{
	for ( UProperty* Prop = Struct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext )
	{
		if ( Prop->IsLocalized() )
		{
			for( INT i = 0; i < Prop->ArrayDim; i++ )
			{
				FString NewPrefix;

				if( KeyPrefix )
				{
					NewPrefix = FString::Printf( TEXT("%s."), KeyPrefix );
				}

				if( Prop->ArrayDim > 1 )
				{
					NewPrefix += FString::Printf( TEXT("%s[%d]"), *Prop->GetName(), i );
				}
				else
				{
					NewPrefix += Prop->GetName();
				}

				const INT NewOffset = DataOffset + (Prop->Offset) + (i * Prop->ElementSize );
				ExportProp( Class, SuperClass, bAtRoot ? CastChecked<UClass>(Prop->GetOuter()) : OuterClass,
							Prop,
							IntName, SectionName, *NewPrefix,
							DataBase, NewOffset,
							bCompareAgainstDefaults, bDumpEmptyProperties );
			}
		}
	}
}

/**
 * Compares the paths the input objects.
 */
IMPLEMENT_COMPARE_POINTER( UObject, UnEdSrv,
{
	//return appStricmp( *AOuter->GetName(), *BOuter->GetName() );
	return appStricmp( *A->GetPathName(), *B->GetPathName() );
} )

/**
 * @param	Package						The package to export.
 * @param	IntName						Name of the .int name.
 * @param	bCompareAgainstDefaults		If TRUE, don't export the property unless it differs from defaults.
 * @param	bDumpEmptyProperties		If TRUE, export properties with zero length/empty strings.
 * @param	ExportFilter				If specified, filter to apply to export
 */
void FLocalizationExport::ExportPackage(UPackage* Package, const TCHAR* IntName, UBOOL bCompareAgainstDefaults, UBOOL bDumpEmptyProperties, const FLocalizationExportFilter* ExportFilter /* = NULL */ )
{
	FConfigCacheIni* GConfigCache = static_cast<FConfigCacheIni*>(GConfig);
	FFilename IntFilename = IntName;

	// Make a list of objects that belong to the package.
	TArray<UObject*> ObjectsToExport;
	for ( FObjectIterator It ; It ; ++It )
	{
		UObject* Obj = *It;

		if ( Obj->HasAnyFlags(RF_Transient|RF_NotForClient|RF_NotForServer) )
		{
			continue;
		}

		if ( Obj->IsIn( Package ) )
		{
			UClass* Class = Cast<UClass>( Obj );
			if ( !Class )
			{
				// Skip this object if it isn't a PerObjectConfig object or if it is not RF_PerObjectLocalized.
				UClass* ObjClass = Obj->GetClass();
				if( ObjClass->HasAnyClassFlags(CLASS_Localized) || Obj->HasAnyFlags(RF_PerObjectLocalized) )
				{
					ObjectsToExport.AddItem( Obj );

					// we need to first remove any existing data in this object's section of the loc file so that we don't resave obsolete data
					FString LocFilename, LocSectionName, KeyPrefix;
					verifyf(Obj->GetLocalizationDataLocation(Obj,LocFilename, LocSectionName, KeyPrefix), TEXT("Failed to find the localization data location for %s - LocFilename:%s  LocSectionName:%s  KeyPrefix:%s"),
						*Obj->GetFullName(), *LocFilename, *LocSectionName, *KeyPrefix);

					// re-add the path and extension to the filename
					LocFilename = IntFilename.GetPath() * (LocFilename + TEXT(".") + IntFilename.GetExtension());

					// find the file corresponding to this object's loc file, loading it if necessary
					FConfigFile* LocFile = GConfigCache->Find(*LocFilename, TRUE);
					check(LocFile);

					// now remove the section
					LocFile->Remove(*LocSectionName);
				}
			}
			else
			{
				if ( Class->HasAnyClassFlags( CLASS_Localized ) )
				{
					ObjectsToExport.AddItem( Obj );

					// we need to first remove any existing data in this object's section of the loc file so that we don't resave obsolete data
					// re-add the path and extension to the filename
					FString LocFilename = IntFilename.GetPath() * (Class->GetOutermost()->GetName() + TEXT(".") + IntFilename.GetExtension());

					// find the file corresponding to this object's loc file, loading it if necessary
					FConfigFile* LocFile = GConfigCache->Find(*LocFilename, TRUE);
					check(LocFile);

					// now remove the section
					LocFile->Remove(*Class->GetName());
				}
			}
		}
	}

	// Sort based on object path.
	Sort<USE_COMPARE_POINTER(UObject,UnEdSrv)>( ObjectsToExport.GetTypedData(), ObjectsToExport.Num() );

	TArray<FString> FilesToFlush;

	// Prep the filter query in case filtering is to be used
	FString QueryString = ExportFilter ? ExportFilter->ToString() : TEXT("");
	FCallbackQueryParameters FilterQueryParams( NULL, CALLBACK_LocalizationExportFilter, QueryString );


	// Export objects.
	GPropertyCount = 0;
	for ( INT ObjectIndex = 0 ; ObjectIndex < ObjectsToExport.Num() ; ++ObjectIndex )
	{
		UObject* Obj			= ObjectsToExport(ObjectIndex);

		// If a filter was specified, query to see if the object passes the localization filter. If it does not, ignore it for
		// exporting purposes.
		if ( ExportFilter )
		{
			FilterQueryParams.QueryObject = Obj;
			if ( !GCallbackQuery->Query( FilterQueryParams ) )
			{
				continue;
			}
		}

		UClass* Class			= Cast<UClass>( Obj );

		// Is the object a class?
		if ( Class )
		{
			FString LocFilename = IntFilename.GetPath() * (Class->GetOutermost()->GetName() + TEXT(".") + IntFilename.GetExtension());

			// for class default objects, the filename is the package containing the class and the section is the name of the class
			ExportStruct( Class, Class->GetSuperClass(), Class,
							Class,
							*LocFilename, 
							*Class->GetName(), NULL,
							(BYTE*)Class->GetDefaultObject(), 0, TRUE,
							bCompareAgainstDefaults, bDumpEmptyProperties );

			FilesToFlush.AddUniqueItem(LocFilename);
		}
		else
		{
			UClass* ObjClass		= Obj->GetClass();

			FString LocFilename, LocSectionName, KeyPrefix;
			verifyf(Obj->GetLocalizationDataLocation(Obj,LocFilename, LocSectionName, KeyPrefix), TEXT("Failed to find the localization data location for %s - LocFilename:%s  LocSectionName:%s  KeyPrefix:%s"),
				*Obj->GetFullName(), *LocFilename, *LocSectionName, *KeyPrefix);

			// re-add the path and extension to the filename
			LocFilename = IntFilename.GetPath() * (LocFilename + TEXT(".") + IntFilename.GetExtension());

			// PerObjectConfig or subobject instance
			const UBOOL bInstancedSubobject = !ObjClass->HasAnyClassFlags(CLASS_PerObjectConfig|CLASS_PerObjectLocalized);
			ExportStruct( bInstancedSubobject ? NULL : ObjClass, ObjClass, NULL, ObjClass,
				*LocFilename, *LocSectionName, KeyPrefix.Len() > 0 ? *KeyPrefix : NULL,
				(BYTE*)Obj, 0, TRUE, bCompareAgainstDefaults, bDumpEmptyProperties );

			FilesToFlush.AddUniqueItem(LocFilename);
		}
	}

	for ( INT FlushIndex = 0; FlushIndex < FilesToFlush.Num(); FlushIndex++ )
	{
		FString LocFile = FilesToFlush(FlushIndex);
		GConfig->Flush( FALSE, *LocFile );
	}
	GWarn->Logf( TEXT("Exported %d properties."), GPropertyCount );
}
