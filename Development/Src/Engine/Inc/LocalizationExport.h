/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LOCALIZATIONEXPORT_H__
#define __LOCALIZATIONEXPORT_H__

/** Helper class specifying a filter on the localization export data */
class FLocalizationExportFilter
{
public:
	/** Enumeration specifying the different filtering types available in regards to tag filtering */
	enum ETagFilterType
	{
		TFT_MatchAny,	// Assets which match any of the filter tags pass the filter
		TFT_MatchAll,	// Assets which match all of the filter tags pass the filter
		TFT_None		// No filtering is done by tags
	};

	/** Default constructor */
	FLocalizationExportFilter();

	/**
	 * Construct from a specially formatted string
	 *
	 * @param	InInitString	String to construct from, formatted via the ToString method
	 */
	FLocalizationExportFilter( const FString& InInitString );

	/**
	 * Constructor
	 *
	 * @param	InFilterTags	Filter tags to use
	 * @param	InTagFilterType	Filter type to use
	 */
	FLocalizationExportFilter( const TArray<FString>& InFilterTags, ETagFilterType InTagFilterType );

	/**
	 * Returns the string representation of the filter. Can be used to reconstruct a filter object.
	 *
	 * @return	String representation of the filter
	 */
	FString ToString() const;

	/**
	 * Simple accessor to tag filter type
	 *
	 * @return	The tag filter type
	 */
	ETagFilterType GetTagFilterType() const;

	/**
	 * Simple mutator to tag filter type
	 *
	 * @param	InType	New tag filter type to use
	 */
	void SetTagFilterType( ETagFilterType InType );

	/**
	 * Const accessor to the filter tag array
	 *
	 * @return Const reference to filter tag array
	 */
	const TArray<FString>& GetFilterTags() const;

	/**
	 * Non-const accessor to the filter tag array
	 *
	 * @return Reference to filter tag array
	 */
	TArray<FString>& GetFilterTags();

private:
	/** Tags to filter with */
	TArray<FString> FilterTags;

	/** Type of tag filter to perform */
	ETagFilterType TagFilterType;

	/** String delimiting filter tags when used in string format */
	static const TCHAR* FilterDelimiter;
};

/**
 * A set of static methods for exporting localized properties.
 */
class FLocalizationExport
{
public:

	/**
	 * @param	Package						The package to export.
	 * @param	IntName						Name of the .int name.
	 * @param	bCompareAgainstDefaults		If TRUE, don't export the property unless it differs from defaults.
	 * @param	bDumpEmptyProperties		If TRUE, export properties with zero length/empty strings.
	 * @param	ExportFilter				If specified, filter to apply to export
	 */
	static void ExportPackage(UPackage* Package, const TCHAR* IntName, UBOOL bCompareAgainstDefaults, UBOOL bDumpEmptyProperties, const FLocalizationExportFilter* ExportFilter = NULL );

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
	static void ExportStruct(UClass*			Class,
							 UClass*			SuperClass,
							 UClass*			OuterClass,
							 UStruct*		Struct,
							 const TCHAR*	IntName,
							 const TCHAR*	SectionName,
							 const TCHAR*	KeyPrefix,
							 BYTE*			DataBase,
							 INT				DataOffset,
							 UBOOL			bAtRoot,
							 UBOOL			bCompareAgainstDefaults,
							 UBOOL			bDumpEmptyProperties);

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
	static void ExportProp(UClass*		Class, 
						   UClass*		SuperClass, 
						   UClass*		OuterClass, 
						   UProperty*	Prop, 
						   const TCHAR*	IntName, 
						   const TCHAR*	SectionName, 
						   const TCHAR*	KeyPrefix, 
						   BYTE*			DataBase, 
						   INT			DataOffset,
						   UBOOL			bCompareAgainstDefaults,
						   UBOOL			bDumpEmptyProperties);

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
	static void ExportDynamicArray(UClass*			Class,
								   UClass*			SuperClass, 
								   UClass*			OuterClass, 
								   UArrayProperty*	Prop, 
								   const TCHAR*		IntName, 
								   const TCHAR*		SectionName, 
								   const TCHAR*		KeyPrefix, 
								   BYTE*				DataBase, 
								   INT				DataOffset, 
								   UBOOL				bAtRoot,
								   UBOOL				bCompareAgainstDefaults);

	/**
	 * Generates an .int name from the package name.
	 */
	static void GenerateIntNameFromPackageName(const FString &PackageName, FString &OutIntName);
};

#endif // __LOCALIZATIONEXPORT_H__
