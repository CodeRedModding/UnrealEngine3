/*=============================================================================
	UnObjectTools.h: Object-related utilities

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UnObjectTools_h__
#define __UnObjectTools_h__

#ifdef _MSC_VER
	#pragma once
#endif

namespace ObjectTools
{

	/** Returns TRUE if the specified object can be displayed in a content browser */
	UBOOL IsObjectBrowsable( UObject* Obj, const TMap< UClass*, TArray< UGenericBrowserType* > >* InResourceTypes );

	
	/** Creates dictionaries mapping object types to the actual classes associated with that type, and the reverse map */
	void CreateBrowsableObjectTypeMaps( TArray< UGenericBrowserType* >& OutBrowsableObjectTypeList,
										TMap< UGenericBrowserType*, TArray< UClass* > >& OutBrowsableObjectTypeToClassMap,
										TMap< UClass*, TArray< UGenericBrowserType* > >& OutBrowsableClassToObjectTypeMap );

	/**
	 * Calls Init on the given resource type
	 * Used when the resource type needs to be updated due to new data being available
	 */
	void RefreshResourceType( UClass* ResourceType );

	/**
	 * An archive for collecting object references that are top-level objects.
	 */
	class FArchiveTopLevelReferenceCollector : public FArchive
	{
	public:
		FArchiveTopLevelReferenceCollector(
			TArray<UObject*>* InObjectArray,
			const TArray<UObject*>& InIgnoreOuters,
			const TArray<UClass*>& InIgnoreClasses,
			const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes );


		/** @return		TRUE if the specified objects should be serialized to determine asset references. */
		FORCEINLINE UBOOL ShouldSearchForAssets(const UObject* Object) const
		{
			// Discard class default objects.
			if ( Object->IsTemplate(RF_ClassDefaultObject) )
			{
				return FALSE;
			}
			// Check to see if we should be based on object class.
			if ( IsAnIgnoreClass(Object) )
			{
				return FALSE;
			}
			// Discard sub-objects of outer objects to ignore.
			if ( IsInIgnoreOuter(Object) )
			{
				return FALSE;
			}
			return TRUE;
		}

		/** @return		TRUE if the specified object is an 'IgnoreClasses' type. */
		FORCEINLINE UBOOL IsAnIgnoreClass(const UObject* Object) const
		{
			for ( INT ClassIndex = 0 ; ClassIndex < IgnoreClasses.Num() ; ++ClassIndex )
			{
				if ( Object->IsA(IgnoreClasses(ClassIndex)) )
				{
					return TRUE;
				}
			}
			return FALSE;
		}

		/** @return		TRUE if the specified object is not a subobject of one of the IngoreOuters. */
		FORCEINLINE UBOOL IsInIgnoreOuter(const UObject* Object) const
		{
			for ( INT OuterIndex = 0 ; OuterIndex < IgnoreOuters.Num() ; ++OuterIndex )
			{
				if( ensure( IgnoreOuters( OuterIndex ) != NULL ) )
				{
					if ( Object->IsIn(IgnoreOuters(OuterIndex)) )
					{
						return TRUE;
					}
				}
			}
			return FALSE;
		}

	private:
		/** 
		 * UObject serialize operator implementation
		 *
		 * @param Object	reference to Object reference
		 * @return reference to instance of this class
		 */
		FArchive& operator<<( UObject*& Obj );

		/** Stored pointer to array of objects we add object references to */
		TArray<UObject*>*		ObjectArray;

		/** Only objects not within these objects will be considered.*/
		const TArray<UObject*>&	IgnoreOuters;

		/** Only objects not of these types will be considered.*/
		const TArray<UClass*>&	IgnoreClasses;

		/** Allowed browsable resource types */
		const TMap< UClass*, TArray< UGenericBrowserType* > >* BrowsableObjectTypes;
	};

	/** Target package and object name for moving an asset. */
	class FMoveInfo
	{
	public:
		FString FullPackageName;
		FString NewObjName;

		void Set(const TCHAR* InFullPackageName, const TCHAR* InNewObjName);
		
		/** @return		TRUE once valid (non-empty) move info exists. */
		UBOOL IsValid() const;
	};


	void GetReferencedTopLevelObjects(UObject* InObject, TArray<UObject*>& OutTopLevelRefs, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes );

	/**
	 * Handles fully loading packages for a set of passed in objects.
	 *
	 * @param	Objects				Array of objects whose packages need to be fully loaded
	 * @param	OperationString		Localization key for a string describing the operation; appears in the warning string presented to the user.
	 * 
	 * @return TRUE if all packages where fully loaded, FALSE otherwise
	 */
	UBOOL HandleFullyLoadingPackages( const TArray<UObject*>& Objects, const TCHAR* OperationString );



	void DuplicateWithRefs( TArray<UObject*>& SelectedObjects, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes, TArray<UObject*>* OutObjects = NULL );
	
	/** Helper struct to detail the results of a consolidation operation */
	struct FConsolidationResults : public FSerializableObject
	{
		/** FSerializableObject interface; Serialize any object references */
		virtual void Serialize( FArchive& Ar )
		{
			Ar << DirtiedPackages << InvalidConsolidationObjs << FailedConsolidationObjs;
		}

		/** Packages dirtied by a consolidation operation */
		TArray<UPackage*>	DirtiedPackages;

		/** Objects which were not valid for consolidation */
		TArray<UObject*>	InvalidConsolidationObjs;

		/** Objects which failed consolidation (partially consolidated) */
		TArray<UObject*>	FailedConsolidationObjs;
	};

	/**
	 * Consolidates objects by replacing all references/uses of the provided "objects to consolidate" with references to the "object to consolidate to." This is
	 * useful for situations such as when a particular asset is duplicated in multiple places and it would be handy to allow all uses to point to one particular copy
	 * of the asset. When executed, the function first attempts to directly replace all relevant references located within objects that are already loaded and in memory.
	 * Next, it deletes the "objects to consolidate," leaving behind object redirectors to the "object to consolidate to" in their wake.
	 *
	 * @param	ObjectToConsolidateTo	Object to which all references of the "objects to consolidate" will instead refer to after this operation completes
	 * @param	ObjectsToConsolidate	Objects which all references of which will be replaced with references to the "object to consolidate to"; each will also be deleted
	 * @param	InResourceTypes			Resource/generic browser types associated with the "objects to consolidate"
	 *
	 * @note	This function performs NO type checking, by design. It is potentially dangerous to replace references of one type with another, so utilize caution.
	 * @note	The "objects to consolidate" are DELETED by this function.
	 *
	 * @return	Structure of consolidation results, specifying which packages were dirtied, which objects failed consolidation (if any), etc.
	 */
	FConsolidationResults ConsolidateObjects( UObject* ObjectToConsolidateTo, TArray<UObject*>& ObjectsToConsolidate, const TArray< UGenericBrowserType* >& InResourceTypes );

	/**
	 * Copies references for selected generic browser objects to the clipboard.
	 */
	void CopyReferences( const TArray< UObject* >& SelectedObjects ); // const

	void ShowReferencers( const TArray< UObject* >& SelectedObjects ); // const

	/**
 	 * Displays a tree(currently) of all assets which reference the passed in object.  
	 *
	 * @param ObjectToGraph		The object to find references to.
	 * @param InBrowsableTypes	A mapping of classes to browsable types.  The tool only shows browsable types or actors
	 */
	void ShowReferenceGraph( UObject* ObjectToGraph, TMap<UClass*, TArray<UGenericBrowserType*> >& InBrowsableTypes );
	/**
	 * Displays all of the objects the passed in object references
	 *
	 * @param	Object	Object whose references should be displayed
	 * @param	bGenerateCollection If true, generate a collection 
	 */
	void ShowReferencedObjs( UObject* Object, UBOOL bGenerateCollection );

	/**
	 * Select the object referencers in the level
	 *
	 * @param	Object			Object whose references are to be selected
	 *
	 */
	void SelectActorsInLevelDirectlyReferencingObject( UObject* RefObj );

	/**
	 * Select the object and it's external referencers' referencers in the level.
	 * This function calls AccumulateObjectReferencersForObjectRecursive to
	 * recursively build a list of objects to check for referencers in the level
	 *
	 * @param	Object			Object whose references are to be selected
	 *
	 */
	void SelectObjectAndExternalReferencersInLevel( UObject* Object );

	/**
	 * Recursively add the objects referencers to a single array
	 *
	 * @param	Object			Object whose references are to be selected
	 * @param	Referencers		Array of objects being referenced in level
	 *
	 */
	void AccumulateObjectReferencersForObjectRecursive( UObject* Object, TArray<UObject*>& Referencers );

	/**
	 * Deletes the list of objects
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 * @param	InResourceTypes		Resource types that are associated with the objects being deleted
	 *
	 * @return The number of objects successfully deleted
	 */
	INT DeleteObjects( const TArray< UObject* >& ObjectsToDelete, const TArray< UGenericBrowserType* >& InResourceTypes );


	/**
	 * Delete a single object
	 *
	 * @param	ObjectToDelete		The object to delete
	 * @param	InResourceTypes		Resource types that are associated with the objects being deleted
	 *
	 * @return If the object was successfully
	 */
	UBOOL DeleteSingleObject( UObject* ObjectToDelete, const TArray< UGenericBrowserType* >& InResourceTypes );


	/**
	 * Deletes the list of objects
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 * @param	InResourceTypes		Resource types that are associated with the objects being deleted
	 *
	 * @return The number of objects successfully deleted
	 */
	INT ForceDeleteObjects( const TArray< UObject* >& ObjectsToDelete, const TArray< UGenericBrowserType* >& InResourceTypes );

	/**
	 * Utility function to compose a string list of referencing objects
	 *
	 * @param References			Array of references to the relevant object
	 * @param RefObjNames			String list of all objects
	 * @param DefObjNames			String list of all objects referenced in default properties
	 *
     * @return Whether or not any objects are in default properties
	 */
	UBOOL ComposeStringOfReferencingObjects( TArray<FReferencerInformation>& References, FString& RefObjNames, FString& DefObjNames );

	/**
	 * Internal implementation of rename objects with refs
	 * 
	 * @param Objects		The objects to rename
	 * @param bLocPackages	If true, the objects belong in localized packages
	 * @param ObjectToLanguageExtMap	An array of mappings of object to matching language (for fixing up localization if the objects are moved ).  Note: Not used if bLocPackages is false
	 * @param InBrowsableObjectTypes	A mapping of classes to their generic browser type for finding object references only if they are browsable.
	 */
	UBOOL RenameObjectsWithRefsInternal( TArray<TArray<UObject*> >& Objects, UBOOL bLocPackages, const TArray<TMap< UObject*, FString > >* ObjectToLanguageExtMap, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes );

	/** 
	 * Finds all language variants for the passed in sound wave
	 * 
	 * @param OutObjects	A list of found localized sound wave objects
	 * @param OutObjectToLanguageExtMap	A mapping of sound wave objects to their language extension
	 * @param Wave	The sound wave to search for
	 */
	void AddLanguageVariants( TArray<UObject*>& OutObjects, TMap< UObject*, FString >& OutObjectToLanguageExtMap, USoundNodeWave* Wave );


	/**
	 * Renames an object and leaves redirectors so other content that references it does not break
	 * Also renames all loc instances of the same asset
	 * @param Objects		The objects to rename
	 * @param bLocPackages	If true, the objects belong in localized packages
	 * @param InBrowsableObjectTypes	A mapping of classes to their generic browser type for finding object references only if they are browsable.
	 */
	UBOOL RenameObjectsWithRefs( TArray< UObject* >& SelectedObjects, UBOOL bIncludeLocInstances, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes );

	/**
	 * Generates a list of all valid factory classes.
	 */
	void AssembleListOfImportFactories( TArray<UFactory*>& out_Factories, FString& out_Filetypes, FString& out_Extensions, TMultiMap<INT, UFactory*>& out_FilterIndexToFactory );

	/**
	 * Populates two strings with all of the file types and extensions the provided factory supports.
	 *
	 * @param	InFactory		Factory whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	void GenerateFactoryFileExtensions( const UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions, TMultiMap<INT, UFactory*>& out_FilterToFactory );
	
	/**
	 * Populates two strings with all of the file types and extensions the provided factories support.
	 *
	 * @param	InFactories		Factories whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	void GenerateFactoryFileExtensions( const TArray<UFactory*>& InFactories, FString& out_Filetypes, FString& out_Extensions, TMultiMap<INT, UFactory*>& out_FilterIndexToFactory );
	
	/**
	 * Generates a list of file types for a given class.
	 */
	void AppendFactoryFileExtensions( UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions );


	/**
	 * Iterates over all classes and assembles a list of non-abstract UExport-derived type instances.
	 */
	void AssembleListOfExporters(TArray<UExporter*>& OutExporters);

	/**
	 * Assembles a path from the outer chain of the specified object.
	 */
	void GetDirectoryFromObjectPath(const UObject* Obj, FString& OutResult);

	/**
	 * Opens a File Dialog based on the extensions requested in a factory
	 * @param InFactory - Factory with the appropriate extensions
	 * @param InMessage - Message to display in the dialog
	 * @param OutFileName - Filename that was selected by the dialog (if there was one)
	 * @return - TRUE if a file was successfully selected
	 */
	UBOOL FindFileFromFactory (UFactory* InFactory, const FString& InMessage, FString& OutFileName);


	class WxDlgImportGeneric : public wxDialog
	{
	public:
		WxDlgImportGeneric(UBOOL bInOKToAll=FALSE, UBOOL bInBulkImportMode=FALSE);
		virtual ~WxDlgImportGeneric()
		{
			FWindowUtil::SavePosSize( TEXT("DlgImportGeneric"), this );
		}

		const FString& GetPackage() const { return Package; }
		const FString& GetGroup() const { return Group; }

		/**
		* @param	bInImporting		Points to a flag that WxDlgImportGeneric sets to true just before DoImport() and false just afterwards.
		*/
		virtual int ShowModal( const FFilename& InFilename, const FString& InPackage, const FString& InGroup, UClass* InClass, UFactory* InFactory, UBOOL* bInImporting );
private:
		using wxDialog::ShowModal;		// Hide parent implementation
public:

		void OnOK( wxCommandEvent& In )
		{
			FlaggedDoImport();
		}

		void OnCancel(wxCommandEvent& In);
		void OnOKAll( wxCommandEvent& In )
		{
			bOKToAll = 1;
			FlaggedDoImport();
		}

		// Called when the "Cancel All" button is preseed.  (Bulk import mode only)
		void OnCancelAll( wxCommandEvent& In )
		{
			EndModal(ID_CANCEL_ALL);
		}

		void OnBuildFromPath( wxCommandEvent& In );
		
		// only for FBX importer to display FBX scene info
		void OnSceneInfo( wxCommandEvent& In );

		/** Syncs the GB to the newly imported objects that have been imported so far. */
		void SyncAssetViewToNewObjects()
		{
			if ( NewObjects.Num() > 0 )
			{
				FCallbackEventParameters Parms(NULL, CALLBACK_RefreshContentBrowser, CBR_SyncAssetView|CBR_FocusBrowser);
				for ( INT ObjIndex = 0; ObjIndex < NewObjects.Num(); ObjIndex++ )
				{
					Parms.EventObject = NewObjects(ObjIndex);
					GCallbackEvent->Send(Parms);
				}
			}
		}

		/** Is OK to All in effect */
		UBOOL GetOKToAll() const
		{
			return bOKToAll;
		}

	protected:
		/** Collects newly imported objects as they are created via successive calls to ShowModal. */
		TArray<UObject*> NewObjects;

		FString Package, Group, Name;
		UFactory* Factory;
		UClass* Class;
		FFilename Filename;
		WxPropertyWindowHost* PropertyWindow;
		UBOOL bOKToAll;
		UBOOL bBulkImportMode;
		UBOOL* bImporting;
	
		wxBoxSizer* PGNSizer;
		wxPanel* PGNPanel;
		WxPkgGrpNameCtrl* PGNCtrl;
		wxComboBox *FactoryCombo;
		wxBoxSizer* PropsPanelSizer;

		virtual void DoImport();

		DECLARE_EVENT_TABLE()

	private:
		/**
		 * Wraps DoImport() with bImporting flag toggling.
		 */
		void FlaggedDoImport()
		{
			// Set the client-specified bImporting flag to indicate that we're about to import.
			if ( bImporting )
			{
				*bImporting = true;
			}
			DoImport();
			if ( bImporting )
			{
				*bImporting = false;
			}
		}
	};


	/**
	 * Imports all of the files in the string array passed in.
	 *
	 * @param InFiles					Files to import
	 * @param InFactories				Array of UFactory classes to be used for importing.
	 */
	UBOOL ImportFiles( const wxArrayString& InFiles, const TArray<UFactory*>& InFactories, FString* out_ImportPath=NULL, FString PackageName=TEXT("MyPackage"), FString GroupName=TEXT("") );

	/**
	* Imports all of the files in the directory path passed in. 
	* At this time, this feature is mainly tuned for bulk importing of language specific audio files.
	* The importer assumes the following directory structure layout:
	* base_name (folder) In the content browser package tree this will look like GameName\Content\base_name
	*	sub_name (folder) In the content browser package tree this will look like GameName\Content\base_name\sub_name
	*		package_name (folder) In the content browser package tree this will look like GameName\Content\base name\sub_name\package_name.upk
	*			group_name (optional folder) In the content browser package tree this will appear as a group under the above package_name
	*				file1.ext (file) Specific files to import
	*				file2.ext (file) Specific files to import	
	*				...
	* Folders below the group_name level are ignored but are still recursed for files.
	* Files in other directories along the path will be ignored.
	*
	* @param InImportPath				The path of the top level directory to import from.
	* @param InFactories				Array of UFactory classes to be used for importing.
	*/
	UBOOL BulkImportFiles( const FString& InImportPath, const TArray<UFactory*>& InFactories );

	/*-----------------------------------------------------------------------------
		WxDlgExportGeneric.
	-----------------------------------------------------------------------------*/

	class WxDlgExportGeneric : public wxDialog
	{
	public:
		WxDlgExportGeneric();

		~WxDlgExportGeneric();

		/**
		 * Performs the export.  Modal.
		 *
		 * @param	InFilename	The filename to export the object as.
		 * @param	InObject	The object to export.
		 * @param	InExporter	The exporter to use.
		 * @param	bPrompt		If TRUE, display the export dialog and property window.
		 */
		virtual int ShowModal(const FFilename& InFilename, UObject* InObject, UExporter* InExporter, UBOOL bPrompt);
private:
		using wxDialog::ShowModal;		// Hide parent implementation
public:

	private:
		UObject* Object;
		UExporter* Exporter;
		FFilename Filename;
		WxPropertyWindowHost* PropertyWindow;

		wxTextCtrl *NameText;

		void OnOK( wxCommandEvent& In );

		void DoExport();

		DECLARE_EVENT_TABLE()
	};


	/**
	 * Exports the specified objects to file.
	 *
	 * @param	ObjectsToExport					The set of objects to export.
	 * @param	bPromptIndividualFilenames		If TRUE, prompt individually for filenames.  If FALSE, bulk export to a single directory.
	 * @param	ExportPath						receives the value of the path the user chose for exporting.
	 * @param	bUseProvidedExportPath			If TRUE and out_ExportPath is specified, use the value in out_ExportPath as the export path w/o prompting for a directory when applicable
	 */
	void ExportObjects( const TArray<UObject*>& ObjectsToExport, UBOOL bPromptIndividualFilenames, FString* ExportPath/*=NULL*/, UBOOL bUseProvidedExportPath = FALSE );


	/** Options for in use object tagging */
	enum EInUseSearchOption
	{
		SO_CurrentLevel, // Searches for in use objects refrenced by the current level
		SO_VisibleLevels, // Searches for in use objects referenced by visible levels
		SO_LoadedLevels // Searches for in use objects referenced by all loaded levels
	};

	/**
	 * Tags objects which are in use by levels specified by the search option 
	 *
	 * @param SearchOption	 The search option for finding in use objects
	 */
	void TagInUseObjects( EInUseSearchOption SearchOption );
}




namespace ThumbnailTools
{
	/** Thumbnail texture flush mode */
	namespace EThumbnailTextureFlushMode
	{
		enum Type
		{
			/** Don't flush texture streaming at all */
			NeverFlush = 0,

			/** Aggressively stream resources before rendering thumbnail to avoid blurry textures */
			AlwaysFlush,
		};
	}

	/** Finds the file path/name of an existing package for the specified object full name, or false if the package is not valid.  */
	UBOOL QueryPackageFileNameForObject( const FString& InFullName, FString& OutPackageFileName );

	/** Renders a thumbnail for the specified object */
	void RenderThumbnail( UObject* InObject, const UINT InImageWidth, const UINT InImageHeight, EThumbnailTextureFlushMode::Type InFlushMode, FObjectThumbnail& OutThumbnail );

	/** Generates a thumbnail for the specified object and caches it */
	FObjectThumbnail* GenerateThumbnailForObject( UObject* InObject );

	/**
	 * Caches a thumbnail into a package's thumbnail map.
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	Thumbnail		the thumbnail to cache; specify NULL to remove the current cached thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 *
	 * @return	pointer to the thumbnail data that was cached into the package
	 */
	FObjectThumbnail* CacheThumbnail( const FString& ObjectFullName, FObjectThumbnail* Thumbnail, UPackage* DestPackage );

	/**
	 * Caches an empty thumbnail entry
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 */
	void CacheEmptyThumbnail( const FString& ObjectFullName, UPackage* DestPackage );

	/** Searches for an object's thumbnail in memory and returns it if found */
	const FObjectThumbnail* FindCachedThumbnail( const FString& InFullName );

	/** Returns the thumbnail for the specified object or NULL if one doesn't exist yet */
	FObjectThumbnail* GetThumbnailForObject( UObject* InObject );

	/** Loads thumbnails from the specified package file name */
	UBOOL LoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet<FName>& InObjectFullNames, FThumbnailMap& InOutThumbnails );

	/** Loads thumbnails from a package unless they're already cached in that package's thumbnail map */
	UBOOL ConditionallyLoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails );

	/** Loads thumbnails for the specified objects (or copies them from a cache, if they're already loaded.) */
	UBOOL LoadThumbnailsForObjects( const TArray< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails );

	/** Standard thumbnail height setting used by generation */
	const INT DefaultThumbnailSize=256;
}





#endif //__UnObjectTools_h__
