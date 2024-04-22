/*=============================================================================
	UnObjectTools.h: Object-related utilities

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UnPackageTools_h__
#define __UnPackageTools_h__

#ifdef _MSC_VER
	#pragma once
#endif

class FLocalizationExportFilter;

namespace PackageTools
{
	/**
	 * Filters the global set of packages.
	 *
	 * @param	ObjectTypes					List of allowed object types (or NULL for all types)
	 * @param	OutGroupPackages			The map that receives the filtered list of group packages.
	 * @param	OutPackageList				The array that will contain the list of filtered packages.
	 */
	void GetFilteredPackageList( const TMap< UClass*, TArray< UGenericBrowserType* > >* ObjectTypes,
								 TSet<const UPackage*> &OutFilteredPackageMap,
								 TSet<UPackage*>* OutGroupPackages,
								 TArray<UPackage*> &OutPackageList );


	/**
	 * Fills the OutObjects list with all valid objects that are supported by the current
	 * browser settings and that reside withing the set of specified packages.
	 *
	 * @param	InPackages			Filters objects based on package.
	 * @param	ObjectTypes			List of allowed object types (or NULL for all types)
	 * @param	OutObjects			[out] Receives the list of objects
	 */
	void GetObjectsInPackages( const TArray<UPackage*>* InPackages,
							   const TMap< UClass*, TArray< UGenericBrowserType* > >* ObjectTypes,
							   TArray<UObject*>& OutObjects );

	/**
	 * Attempts to save the specified packages; helper function for by e.g. SaveSelectedPackages().
	 *
	 * @param		PackagesToSave				The content packages to save.
	 * @param		bUnloadPackagesAfterSave	If TRUE, unload each package if it was saved successfully.
	 * @param		pLastSaveDirectory			if specified, initializes the "Save File" dialog with this value
	 *											[out] will be filled in with the directory the user chose for this save operation.
	 * @param		InResourceTypes				If specified and the user saved a new package under a different name, we can 
	 *											delete the original objects to avoid duplicated objects. 
	 *
	 * @return									TRUE if all packages were saved successfully, FALSE otherwise.
	 */
	UBOOL SavePackages( const TArray<UPackage*>& PackagesToSave, 
						UBOOL bUnloadPackagesAfterSave, 
						FString* pLastSaveDirectory=NULL, 
						const TArray< UGenericBrowserType* >* InResourceTypes=NULL );

	/**
	 * Handles fully loading passed in packages.
	 *
	 * @param	TopLevelPackages	Packages to be fully loaded.
	 * @param	OperationString		Localization key for a string describing the operation; appears in the warning string presented to the user.
	 * 
	 * @return TRUE if all packages where fully loaded, FALSE otherwise
	 */
	UBOOL HandleFullyLoadingPackages( const TArray<UPackage*>& TopLevelPackages, const TCHAR* OperationString );


	/**
	 * Loads the specified package file (or returns an existing package if it's already loaded.)
	 *
	 * @param	InFilename	File name of package to load
	 *
	 * @return	The loaded package (or NULL if something went wrong.)
	 */
	UPackage* LoadPackage( FFilename InFilename );

	/**
	 * Helper function that attempts to unlaod the specified top-level packages.
	 *
	 * @param	PackagesToUnload	the list of packages that should be unloaded
	 *
	 * @return	TRUE if the set of loaded packages was changed
	 */
	UBOOL UnloadPackages( const TArray<UPackage*>& PackagesToUnload );

	/**
	 *	Exports the given packages to files.
	 *
	 * @param	PackagesToExport		The set of packages to export.
	 * @param	ExportPath				receives the value of the path the user chose for exporting.
	 * @param	bUseProvidedExportPath	If TRUE and ExportPath is specified, use ExportPath as the user's export path w/o prompting for a directory, where applicable
	 */
	void ExportPackages( const TArray<UPackage*>& PackagesToExport, FString* ExportPath=NULL, UBOOL bUseProvidedExportPath = FALSE );

	/**
	 * Wrapper method for multiple objects at once.
	 *
	 * @param	TopLevelPackages		the packages to be export
	 * @param	ClassToObjectTypeMap	mapping of class to GBT's which support that class.
	 * @param	LastExportPath			the path that the user last exported assets to
	 * @param	FilteredClasses			if specified, set of classes that should be the only types exported if not exporting to single file
	 * @param	bUseProvidedExportPath	If TRUE, use LastExportPath as the user's export path w/o prompting for a directory, where applicable
	 * @param	ExportFilter			Localization filter, if any, to apply to the bulk export
	 *
	 * @return	the path that the user chose for the export.
	 */
	FString DoBulkExport(const TArray<UPackage*>& TopLevelPackages, FString LastExportPath, TMap<UClass*, TArray<UGenericBrowserType*> >* ClassToObjectTypeMap, const TSet<UClass*>* FilteredClasses = NULL, UBOOL bUseProvidedExportPath = FALSE, FLocalizationExportFilter* ExportFilter = NULL );

	/**
	* Bulk Imports files based on directory structure
	* 
	* @param	The last import path the user selected
	*
	* @return	The import path the user chose for the bulk export
	*/
	FString DoBulkImport(FString LastImportPath);

	/**
	 * Utility method for exporting localized text for the assets contained in the specified packages.
	 *
	 * @param	TopLevelPackages	
	 */
	FString ExportLocalization( const TArray<UPackage*>& TopLevelPackages, FString LastExportPath, TMap<UClass*, TArray<UGenericBrowserType*> >* ClassToObjectTypeMap );

	/**
	 * Displays an error message if any of the packages in the list have been cooked.
	 *
	 * @param	Packages	The list of packages to check for cooked status.
	 *
	 * @return	TRUE if cooked packages were found; false otherwise.
	 */
	UBOOL DisallowOperationOnCookedPackages(const TArray<UPackage*>& Packages);

	/** Helper function that attempts to check out the specified top-level packages. */
	void CheckOutRootPackages( const TArray<UPackage*>& Packages );


	/**
	 * Checks if the passed in path is in an external directory. I.E Ones not found automatically in the content directory
	 *
	 * @param	PackagePath	Path of the package to check, relative or absolute
	 * @return	TRUE if PackagePath points to an external location
	 */
	UBOOL IsPackagePathExternal(const FString& PackagePath);

	/**
	 * Checks if the passed in package's filename is in an external directory. I.E Ones not found automatically in the content directory
	 *
	 * @param	Package	The package to check
	 * @return	TRUE if the package points to an external filename
	 */
	UBOOL IsPackageExternal(const UPackage& Package);

	/**
	 * Checks if the passed in packages have any references to  externally loaded packages.  I.E Ones not found automatically in the content directory
	 *
	 * @param	PackagesToCheck					The packages to check
	 * @param	OutPackagesWithExternalRefs		Optional list of packages that do have external references
	 * @param	LevelToCheck					The ULevel to check
	 * @param	OutObjectsWithExternalRefs		List of objects gathered from within the given ULevel that have external references
	 * @return	TRUE if PackagesToCheck has references to an externally loaded package
	 */
	UBOOL CheckForReferencesToExternalPackages(const TArray<UPackage*>* PackagesToCheck, TArray<UPackage*>* OutPackagesWithExternalRefs, ULevel* LevelToCheck=NULL, TArray<UObject*>* OutObjectsWithExternalRefs=NULL );
}




#endif //__UnPackageTools_h__
