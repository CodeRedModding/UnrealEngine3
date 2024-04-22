//=============================================================================
//	BackendInterface.cs: Content browser backend interface
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Controls.Primitives;
using System.IO;
using System.Collections.ObjectModel;
using System.ComponentModel;
using UnrealEd;


namespace ContentBrowser
{

	/// Types of confirmation prompts that can be disabled by the user
	public enum ConfirmationPromptType
	{
		/// Tagging many assets
		AddTagToAssets = 0,

		/// Add many assets to a collection
		AddAssetsToCollection,

		/// Untagging many assets
		RemoveTagFromAssets,

		/// Removing many assets from a collection
		RemoveAssetsFromCollection,

		// ...

		/// Number of prompt types
		NumPromptTypes
	}


	/**
	 * An Interface to be implemented by the C++/CLI portion of the code.
	 * This exists so that C# code can call C++/CLI code. 
	 */
	public interface IContentBrowserBackendInterface
	{

		/**
		 * Get tag-related parameters from the engine. E.g. max tag length
		 */
		ContentBrowser.TagUtils.EngineTagDefs GetTagDefs();

		/**
		 * Create a new Tag
		 * 
		 * @param InTag   A string tag to be created
		 *
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool CreateTag( String InTag );

		/**
		 * Destroy an existing Tag
		 * 
		 * @param InTag   A string tag to be destroyed
		 * 
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool DestroyTag( String InTag );

		/**
		 * Copies (or renames/moves) a tag
		 * 
		 * @param InCurrentTagName The tag to rename
		 * @param InNewTagName The new tag name
		 * @param bInMove True if the old tag should be destroyed after copying it
		 *
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool CopyTag( String InCurrentTagName, String InNewTagName, bool bInMove );

		/**
		 * Add a new collection.
		 * 
		 * @param InCollectionName The name of collection to add
		 * @param InType The type of collection specified by InCollectionName
		 * 
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool CreateCollection( String InCollectionName, EBrowserCollectionType InType );

		/**
		 * Destroy an existing collection
		 * 
		 * @param	InCollectionName	The name of collection to destroy
		 * @param	InType				The type of collection specified by InCollectionName
		 * 
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool DestroyCollection( String InCollectionName, EBrowserCollectionType InType );

		/**
		 * Copies (or renames/moves) a collection
		 * 
		 * @param InCurrentCollectionName The collection to rename
		 * @param InCurrentType The type of collection specified by InCurrentCollectionName
		 * @param InNewCollectionName The new name of the collection
		 * @param InNewType The type of collection specified by InNewCollectionName
		 * @param bInMove True if the old collection should be destroyed after copying it
		 *
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool CopyCollection( String InCurrentCollectionName, EBrowserCollectionType InCurrentType, String InNewCollectionName, EBrowserCollectionType InNewType, bool bInMove );

		/**
		 * Add a list of assets to collection
		 * 
		 * @param InAssetFullNames	Full names of assets to be added
		 * @param InCollection		Collections to which we add
		 * @param InType			The type of collection specified by InCollectionName
		 * 
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool AddAssetsToCollection( ICollection<String> InAssetFullNames, Collection InCollection, EBrowserCollectionType InType );


        /**
         * Remove assets from a collection
         * 
         * @param InAssetFullNames  Full names of assets to removed
         * @param InCollection  Collection from which to remove
         * @param InType The type of collection specified by InCollectionName
         * 
         * @return TRUE is succeeded, FALSE if failed.
         */
        bool RemoveAssetsFromCollection(ICollection<String> InAssetFullNames, Collection InCollection, EBrowserCollectionType InType);

		/**
		 * Mark assets as quarantined.
		 * 
		 * @param AssetsToQuarantine Assets that will be put under quarantine.
		 * 
		 * @return TRUE if succeeded, FALSE if failed.
		 */
		bool QuarantineAssets(ICollection<AssetItem> AssetsToQuarantine);

		/**
		 * List the quarantine on assets.
		 * 
		 * @param AssetsToRelease Lift quarantine on these assets.
		 * 
		 * @return TRUE if succeeded, FALSE if failed.
		 */
		bool LiftQuarantine(ICollection<AssetItem> AssetsToRelease);

		/**
		 * Marshal a collection of asset items into a string (only marshals the Type and PathName)
		 * 
		 * @param InAssetItems A collection of AssetItems to marshal
		 * 
		 * @return A string representing the AssetItems
		 */
		String MarshalAssetItems( ICollection<AssetItem> InAssetItems );

		/**
		 * Extract asset full names from a marshaled string.
		 * 
		 * @param InAssetItems A collection of AssetItems to marshal
		 * 
		 * @return A list of asset full names
		 */
		List<String> UnmarshalAssetFullNames( String MarshaledAssetNames );

		/**
		 * Update the list of sources (packages, collections, smart collection, etc)
		 * 
		 * @param UpdateSCC Whether an SCC state update should be included in this refresh
		 * @param UseFlatView When true do not build folders.
		 */
		void UpdateSourcesList( bool ShouldUpdateSCC );


		/**
		 * Update the list of packages
		 * 
		 * @param UseFlatView When true add all packages to the top-level layer.
		 */
		void UpdatePackagesTree( bool UseFlatView );


		/**
		 * Update the UI's list of all tags known to the game
		 */ 
		void UpdateTagsCatalogue();


		/**
		 * Refresh the visible assets based on the source selection and filter.
		 */
		void UpdateAssetsInView();

		/**
		 * Makes sure the objects for the currently selected assets have been loaded
		 */
		void LoadSelectedObjectsIfNeeded();

		/** Syncs the currently selected objects in the asset view with the engine's global selection set */
		void SyncSelectedObjectsWithGlobalSelectionSet();

		/**
		 * Attempts to fully load the specified package name
		 *
		 * @param	PackageName	Name of the package to fully load
		 *
		 * @return	True if the package was loaded successfully
		 */
		bool FullyLoadPackage( String PackageName );

		/** Returns a border color to use for the specified object type string */
		Color GetAssetVisualBorderColorForObjectTypeName( String InObjectTypeName );

		/** Updates custom label text for this asset if it has any */
		List<String> GenerateCustomLabelsForAsset( AssetItem InAssetItem );

		/** Updates custom data columns for this asset */
		List<String> GenerateCustomDataColumnsForAsset( AssetItem InAssetItem );

		/** Updates the 'Date Added' for an asset item */
		DateTime GenerateDateAddedForAsset( AssetItem InAssetItem );

		/** Calculates memory usage for the passed in asset */
		int CalculateMemoryUsageForAsset( AssetItem InAssetItem );

		/**
		 * Updates the status of the specified asset item
		 *
		 * @param	Asset		Asset to update
		 */
		void UpdateAssetStatus( AssetItem Asset, AssetStatusUpdateFlags UpdateFlags );

		/**
		 * Update the status of all packages in the tree.
		 */
		void UpdatePackagesTreeUI();

		/**
		 * Updates the status of the specified package
		 *
		 * @param	Pkg		Package to update
		 */
		void UpdatePackagesTreeUI( ObjectContainerNode Pkg );


		/**
		 * Returns menu item descriptions, to be displayed in the asset view's right click context menu
		 *
		 * @param	OutMenuItems	(Out) List of WPF menu items
		 */
		void QueryAssetViewContextMenuItems( out List<Object> OutMenuItems );


		/**
		 * Queries the list of collection names the specified asset is in (sorted alphabetically).  Note that
		 * for private collections, only collections created by the current user will be returned.
		 *
	 	 * @param	InFullName			The full name for the asset
		 * @param	InType				The type of collection specified by InCollectionName
		 *
		 * @return	List of collection names, sorted alphabetically
		 */
		List< String > QueryCollectionsForAsset( String InFullName, EBrowserCollectionType InType );


	    /**
	     * Populate a context menu with a menu item for each type of object that can be created using a class factory.
	     *
	     * @param	ctxMenu		the context menu to populate items for
	     */
		void PopulateObjectFactoryContextMenu( ContextMenu out_MenuItems );

		/** Populates an item collection with package command menu items for use in a ContextMenu or MenuItem.  
		 *  Note: This function also populates package menu with dynamic menu items which do not use the passed in event handlers.  
		 *  Instead they use ExecuteCustomObjectCommand and CanExecuteCustomObjectCommand.
		 *  
		 * @param OutPackageListMenuItems		The context menu where menu items should be added.
		 * @param OutCommandBindings			The command binding collection where new command bindings should be added.
		 * @param InTypeId						The typeid of the ui element we are registering the commands with
		 * @param InEventHandler				Optional new event handler that should be called on these menu items.
		 * @param InCanExecuteEventHandler		Optional new event handler that should be called when determining if these menu items can be used.
		 */
		void PopulatePackageListMenuItems( ItemCollection OutPackageListMenuItems, CommandBindingCollection OutCommandBindings, Type InTypeId, ExecutedRoutedEventHandler InEventHandler, CanExecuteRoutedEventHandler InCanExecuteEventHandler  );

		/**
		 * Add tag to the assets. If a tag is on any of the assets already it will not be added again.
		 * 
		 * @param InAssets   AssetItems which need to be tagged
		 * @param TagToAdd   The tag to be added to the AssetItems
		 * 
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool AddTagToAssets( ICollection<AssetItem> InAssets, String TagToAdd );


		/**
		 * Remove a tag from a list of assets. Do nothing to assets that already lacks this tag.
		 * 
		 * @param InAssets    Assets to untag.
		 * @param TagToRemove Tags to remove.
		 * 
		 * @return TRUE is succeeded, FALSE if failed.
		 */
		bool RemoveTagFromAssets( ICollection<AssetItem> AssetName, String TagToRemove );


		/** Called by the asset view when the selection has changed */
		void OnAssetSelectionChanged( Object Sender, AssetView.AssetSelectionChangedEventArgs Args );

		/** Called when the user opts to "activate" an asset item (usually by double clicking on it) */
		void LoadAndActivateSelectedAssets();

        /**
         *  Called when the user opts to view the properties of asset items via hot-key; only displays properties if all of
         *  the selected assets do not have a specific editor for setting their attributes
         */
        void LoadAndDisplayPropertiesForSelectedAssets();

		/**
		 * Called when the user activates the preview action on an asset (not all assets support preview; currently used for sounds)
		 * 
		 * @param ObjectFullName		Full name of asset to preview
		 * 
		 * @return true if the preview process started; false if the action actually stopped an ongoing preview process.
		 */
		bool PreviewLoadedAsset( String ObjectFullName );

		/**
		* Get a list of the Favorites for the TypeFilter from [Game]EditorUserSettings.
		* Note that any invalid Favorites are removed. Invalid favorites are
		* ones that are not GenericBrowserTypes.
		* 
		* @return A list of the favorite type names .
		*/
        List<String> GetObjectTypeFilterList();

        /**
         * Get a list of the Browsable Item Type Names that for a particular item class
         * 
         * @param  ClassNameList The list of class names to search for
         * 
         * @return A list of Browsable Item Type names for the given classes
         * 
         */

        List<String> GetBrowsableTypeNameList(List<String> ClassNameList);

        /**
		 * Save the content browser UI state to config
         * 
         * @param SaveMe LayoutInfo to save
         */
        void SaveContentBrowserUIState(ContentBrowserUIState SaveMe);

        /**
         * Load the content browser UI state from config
         * 
         * @param LoadMe LayoutInfo to populate
         */
        void LoadContentBrowserUIState(ContentBrowserUIState LoadMe);

		/**
		 * Expand package directories mentioned in "[Core.System] Paths"
		 */
		void ExpandDefaultPackages();

		/**
		 * Check whether the game asset database is in read-only mode.
		 * 
		 * @return True if the GAD is read-only
		 */
		bool IsGameAssetDatabaseReadonly();

		/**
		 * Check whether this asset belongs to any of the browsable types.
		 * 
		 * @param ItemToCheck			Check this item's type
		 * @param BrowsableTypeNames	List of BrowsableTypeNames that this asset could be a type of
		 */
		bool IsAssetAnyOfBrowsableTypes( AssetItem ItemToCheck, ICollection<String> BrowsableTypeNames );

		/**
		 * Check whether this asset type belongs to any of the browsable types.
		 * 
		 * @param AssetType				The asset type to check.
		 * @param bIsArchetype			Is the asset an archetype. 
		 * @param BrowsableTypeNames	List of BrowsableTypeNames that this asset could be a type of
		 */
		bool IsAssetAnyOfBrowsableTypes( String AssetType, bool IsArchetype, ICollection<String> BrowsableTypeNames );



		/** Returns true if the specified asset uses a stock thumbnail resource */
		bool AssetUsesSharedThumbnail( AssetItem Asset );


		/**
		 * Attempts to generate a thumbnail for the specified object
		 * 
		 * @param	Asset	the asset that needs a thumbnail
		 * @param	CheckSharedThumbnailAssets	True if we should even check to see if assets that use a stock thumbnail resource exist in the package file
		 * @param	OutFailedToLoadThumbnail	True if we tried to load the asset's thumbnail from disk and couldn't find it.
		 * 
		 * @return	Returns the BitmapSource for the generated thumbnail, or null if a thumbnail is not available
		 */
		BitmapSource GenerateThumbnailForAsset( AssetItem Asset, out bool OutFailedToLoadThumbnail );


		/**
		 * Attempts to generate a *preview* thumbnail for the specified object
		 * 
		 * @param	ObjectFullName Full name of the object
		 * @param	PreferredSize The preferred resolution of the thumbnail
		 * @param	IsAnimating True if the thumbnail will be updated frequently
		 * @param	ExistingThumbnail The current preview thumbnail for the asset, if any
		 * 
		 * @return	Returns the BitmapSource for the generated thumbnail, or null if a thumbnail is not available
		 */
		BitmapSource GeneratePreviewThumbnailForAsset( String ObjectFullName, int PreferredSize, bool IsAnimating, BitmapSource ExistingThumbnail );

		/**
		 * Removes the uncompressed image data held by the UPackage for the specified asset.
		 *
		 * @param	AssetFullName	the Unreal full name for the object to remove the thumbnail data for
		 */
		void ClearCachedThumbnailForAsset( String AssetFullName );

		/** Locally tags the specified asset as a "ghost" so that it will no longer appear in the editor */
		void LocallyTagAssetAsGhost( AssetItem Asset );

		/** Locally removes the "unverified" tag from the specified asset */
		void LocallyRemoveUnverifiedTagFromAsset( AssetItem Asset );


		/**
		 * Applies the currently selected assets to the objects selected in the level viewport.  For example,
		 * if a material is selected in the content browser and a surface is selected in the 3D view, this
		 * could assign the material to that surface
		 */
		void ApplyAssetSelectionToViewport();

		/**
		 * Get a list of all known asset type names as strings.
		 * The Archetype classes are ignored here because just about any class can be an archetype.
		 * Note, these are the Unreal class names and NOT generic browser type names.
		 */
		NameSet GetAssetTypeNames();

		/**
		 * Save a list of the Favorites for the TypeFilter from [Game]EditorUserSettings.
		 * 
		 * @return A list of the favorite type names .
		 */
		List<String> LoadTypeFilterFavorites();

		/**
		 * Get the list of the Favorites for the TypeFilter into [Game]EditorUserSettings.
		 * 
		 * @param InFavorites A list of Favorite Type names.
		 */
		void SaveTypeFilterFavorites( List<String> InFavorites );

		/** Tags all in use objects based on content browser filter options */
		void TagInUseObjects( );

		/** 
		 * Determines if an asset is in use by looking if RF_TagExp was set by TagInUseObjects.
	 	 *
	 	 * @param Asset	The asset to check
	 	 */
		bool IsObjectInUse( AssetItem Asset );

		#region Confirmation prompts

		/**
		 * Returns true if confirmation prompt should be displayed
		 * 
		 * @param InType Type of confirmation prompt to check
		 */
		bool ShouldShowConfirmationPrompt( ConfirmationPromptType InType );


		/**
		 * Disables a type of confirmation prompt
		 * 
		 * @param InType Type of confirmation prompt to disable
		 */
		void DisableConfirmationPrompt( ConfirmationPromptType InType );

		#endregion


		#region ContextMenuRouting

		/**
		 * CanExecuteRoutedEventHandler for the ContentBrowser's custom commands
		 * 
		 * @param	Sender	the object that generated the event
		 * @param	EvtArgs	details about the event that was generated
		 */
		void CanExecuteMenuCommand( object Sender, CanExecuteRoutedEventArgs EvtArgs );

		/**
		 * ExecutedRoutedEventHandler for the ContentBrowser's custom commands
		 * 
		 * @param	Sender	the object that generated the event
		 * @param	EvtArgs	details about the event that was generated
		 */
		void ExecuteMenuCommand( object Sender, ExecutedRoutedEventArgs EvtArgs );


		/**
		 * CanExecuteRoutedEventHandler for the asset view's package context menu items
		 * 
		 * @param	Sender	the object that generated the event
		 * @param	EvtArgs	details about the event that was generated
		 */
		void CanExecuteAssetCommand( object Sender, CanExecuteRoutedEventArgs EvtArgs );

		/**
		 * ExecutedRoutedEventHandler for the asset view's package context menu items
		 * 
		 * @param	Sender	the object that generated the event
		 * @param	EvtArgs	details about the event that was generated
		 */
		void ExecuteAssetCommand( object Sender, ExecutedRoutedEventArgs EvtArgs );

		#endregion

		#region DragDrop Methods
		
	
		/**
		 * Begin a drag-n-drop operation.
		 *
		 * @param	SelectedAssetPaths	string containing fully qualified pathnames for the assets which will be part
		 *			of the d&d operation, delimited by the pipe character
		 */
		void BeginDragDrop( String SelectedAssetPaths );

        /**
         * Begin a drag-n-drop operation for import.  Called when a user drags files from windows to either the asset canvas or the package tree
         *
         * @param	A list of filenames the user dropped
         *
         */
        void BeginDragDropForImport( List<String> DroppedFiles );

		#endregion

		#region Utility methods that really belong in another interface

		/**
		 * Wrapper for determining whether an asset is a map package or contained in a map package
		 *
		 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
		 *
		 * @return	TRUE if the specified asset is a map package or contained in a map package
		 */
		bool IsMapPackageAsset( String AssetPathName );

		/**
		 * Wrapper for determining whether an asset is eligible to be loaded on its own.
		 * 
		 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
		 * 
		 * @return	true if the specified asset can be loaded on its own
		 */
		bool IsAssetValidForLoading( String AssetPathName );

		/**
		 * Wrapper for determining whether an asset is eligible to be placed in a level.
		 * 
		 * @param	AssetPathName	the fully qualified [Unreal] pathname of the asset to check
		 * 
		 * @return	true if the specified asset can be placed in a level
		 */
		bool IsAssetValidForPlacing( String AssetPathName );

		/**
		 * Wrapper for determining whether an asset is eligible to be tagged.
		 * 
		 * @param	AssetFullName	the full name of the asset to check
		 * 
		 * @return	true if the specified asset can be tagged
		 */
		bool IsAssetValidForTagging( String AssetFullName );

		/**
		 * Is the user allowed to Create/Destroy tags (a.k.a TagAdmin).
		 * 
		 * @return	True if the user is TagAdmin, false otherwise.
		 */
		bool IsUserTagAdmin();

		/**
		 * Is the user allowed to Create/Destroy/AddTo/RemoveFrom shared collections (a.k.a CollectionsAdmin)
		 *
		 * @return	True if the user is CollectionAdmin, false otherwise.
		 */
		bool IsUserCollectionsAdmin();

		#endregion
	}


    /**
     * ContentBrowserUIState stores various info about the UI state, including layout dimensions, collapse states, etc.
     */
    public class ContentBrowserUIState
	{
		// FILTER PANEL
		public double FilterPanelHeight { get; set; }

		public bool IsFilterPanelCollapsed { get; set; }
        public String FilterPanelSearchString { get; set; }

		// LEFT PANEL		
		public double LeftPanelWidth { get; set; }
		public bool IsLeftPanelCollapsed { get; set; }
		public String ExpandedPackages { get; set; }
		public String LeftSubpanelsState { get; set; }
		public String SelectedSharedCollection { get; set; }
		public bool IsUsingFlatPackageList { get; set; }


		// Parameters of the AssetView
		private AssetView.LayoutMode mAssetViewLayout;
		public AssetView.LayoutMode GetAssetViewLayout() { return mAssetViewLayout; }
		public double AssetViewLayoutAsDouble
		{
			get { return (double)mAssetViewLayout; }
			set
			{
				try
				{
					mAssetViewLayout = (AssetView.LayoutMode)value;
				}
				catch ( System.Exception )
				{
					mAssetViewLayout = AssetView.LayoutMode.ThumbnailsOnly;
				}
			}
		}

        public double AssetViewThumbnailSize { get; set; }
		public double AssetListViewWidth { get; set; }
		public double AssetListViewHeight { get; set; }
		public double AssetView_List_NameColumnWidth { get; set; }
		public double AssetView_List_AssetTypeColumnWidth { get; set; }
		public double AssetView_List_TagsColumnWidth { get; set; }
		public double AssetView_List_PathColumnWidth { get; set; }
		public double AssetView_List_DateAddedColumnWidth { get; set; }
		public double AssetView_List_MemoryUsageColumnWidth { get; set; }
		public double[] AssetView_List_CustomDataColumnWidths = new double[ ContentBrowserDefs.MaxAssetListCustomDataColumns ];

		


		// RIGHT PANEL
		public double RightPanelWidth { get; set; }
		public bool IsRightPanelCollapsed { get; set; }



		/**
		 * Make this LayoutInfo use default values.
		 * These is the definitive source for default Layout state.
		 */
		public void SetToDefault()
		{
			// Search and filter
			this.FilterPanelHeight = 200;
			this.IsFilterPanelCollapsed = false;
            this.FilterPanelSearchString = "";
			
			// Left Panel
			this.LeftPanelWidth = 200;
			this.IsLeftPanelCollapsed = false;
			this.ExpandedPackages = "";
			this.LeftSubpanelsState = "";
			this.SelectedSharedCollection = "";
			this.IsUsingFlatPackageList = false;

			//AssetView defaults
			this.AssetViewLayoutAsDouble = (double)AssetView.LayoutMode.ThumbnailsOnly;
            this.AssetViewThumbnailSize = 128.0;
			this.AssetListViewWidth = 100;
			this.AssetListViewHeight = 100;
			this.AssetView_List_NameColumnWidth = 250;
			this.AssetView_List_AssetTypeColumnWidth = 100;
			this.AssetView_List_TagsColumnWidth = 150;
			this.AssetView_List_PathColumnWidth = 150;
			this.AssetView_List_DateAddedColumnWidth = 60;
			this.AssetView_List_MemoryUsageColumnWidth = 70;
			for( int CurColumnIndex = 0; CurColumnIndex < ContentBrowserDefs.MaxAssetListCustomDataColumns; ++CurColumnIndex )
			{
				this.AssetView_List_CustomDataColumnWidths[ CurColumnIndex ] = 80;
			}

			// Right Panel
			this.RightPanelWidth = 200;
			this.IsRightPanelCollapsed = true;
		}

        /**
         * Clamps any values that tend to get insane. This prevents restoring insane values from bad config files.
         */
		internal void EnsureSanity()
		{
			this.AssetView_List_NameColumnWidth = MathUtils.Clamp( this.AssetView_List_NameColumnWidth, 30, 8000 );
			this.AssetView_List_AssetTypeColumnWidth = MathUtils.Clamp( this.AssetView_List_AssetTypeColumnWidth, 30, 8000 );
			this.AssetView_List_TagsColumnWidth = MathUtils.Clamp( this.AssetView_List_TagsColumnWidth, 30, 8000 );
			this.AssetView_List_PathColumnWidth = MathUtils.Clamp( this.AssetView_List_PathColumnWidth, 30, 8000 );
			this.AssetView_List_DateAddedColumnWidth = MathUtils.Clamp( this.AssetView_List_DateAddedColumnWidth, 30, 8000 );
			this.AssetView_List_MemoryUsageColumnWidth = MathUtils.Clamp( this.AssetView_List_MemoryUsageColumnWidth, 30, 8000 );
			for( int CurColumnIndex = 0; CurColumnIndex < ContentBrowserDefs.MaxAssetListCustomDataColumns; ++CurColumnIndex )
			{
				this.AssetView_List_CustomDataColumnWidths[ CurColumnIndex ] = MathUtils.Clamp( this.AssetView_List_CustomDataColumnWidths[ CurColumnIndex ], 30, 8000 );
			}
		}
	}

}
