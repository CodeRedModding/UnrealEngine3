//=============================================================================
//	SourcesPanel.xaml.cs: Content browser sources panel
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
using System.ComponentModel;
using System.Globalization;
using System.Collections.ObjectModel;
using System.Text.RegularExpressions;

using UnrealEd;
using CustomControls;


namespace ContentBrowser
{
    public static class PackageCommands
    {
		private static RoutedUICommand mSaveAsset = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_SaveAsset"), "SaveAsset", typeof(PackageCommands));
		private static RoutedUICommand mFullyLoad = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_FullyLoad"), "FullyLoad", typeof(PackageCommands));
        private static RoutedUICommand mUnloadPackage = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_UnloadPackage"), "Unload", typeof(PackageCommands));
        
		private static RoutedUICommand mImportAsset = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_Import"), "ImportAsset", typeof(PackageCommands));
        private static RoutedUICommand mOpenPackage = new RoutedUICommand(
            UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_Open"), "OpenPackage", typeof(PackageCommands));
        private static RoutedUICommand mBulkExport = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_BulkExport"), "BulkExport", typeof(PackageCommands));
        private static RoutedUICommand mBulkImport = new RoutedUICommand(
            UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_BulkImport"), "BulkImport", typeof(PackageCommands));
		private static RoutedUICommand mLocExport = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_LocExport"), "LocExport", typeof(PackageCommands));

		private static RoutedUICommand mCheckErrors = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_MenuItem_CheckErrors"), "CheckErrors", typeof(PackageCommands));

		private static RoutedUICommand mSyncPackageView = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_AssetView_SyncPackageView"), "SyncPackageView", typeof(PackageCommands));

		private static RoutedUICommand mOpenExplorer = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_PackageList_Explorer"), "Explorer", typeof(PackageCommands));

// 		private static RoutedUICommand mResliceFracturedMeshes = new RoutedUICommand(
// 			(String)Utils.ApplicationInstance.FindResource("ContentBrowser_PackageList_MenuItem_ResliceFracturedMeshes"), "ResliceFracturedMeshes", typeof(PackageCommands));


		public static RoutedUICommand SaveAsset              { get { return mSaveAsset; } }
        public static RoutedUICommand FullyLoadPackage       { get { return mFullyLoad; } }
        public static RoutedUICommand UnloadPackage          { get { return mUnloadPackage; } }
        
		public static RoutedUICommand ImportAsset            { get { return mImportAsset; } }
        public static RoutedUICommand OpenPackage            { get { return mOpenPackage; } }
        public static RoutedUICommand BulkExport             { get { return mBulkExport; } }
        public static RoutedUICommand BulkImport             { get { return mBulkImport; } }
		public static RoutedUICommand LocExport              { get { return mLocExport; } }

        public static RoutedUICommand CheckErrors            { get { return mCheckErrors; } }

		public static RoutedUICommand SyncPackageView		 { get { return mSyncPackageView; } }
		public static RoutedUICommand OpenExplorer           { get { return mOpenExplorer; } }
       
    }

    public static class SourceControlCommands
    {
        private static RoutedUICommand mRefresh = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_SourceControl_MenuItem_SCCRefresh"), "SCCRefresh", typeof(SourceControlCommands));
        private static RoutedUICommand mCheckIn = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_SourceControl_MenuItem_SCCCheckIn"), "SCCCheckIn", typeof(SourceControlCommands));
        private static RoutedUICommand mCheckOut = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_SourceControl_MenuItem_SCCCheckOut"), "SCCCheckOut", typeof(SourceControlCommands));
		private static RoutedUICommand mRevertCmd = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_SourceControl_MenuItem_SCCRevert"), "SCCRevert", typeof(SourceControlCommands));
        private static RoutedUICommand mHistory = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_SourceControl_MenuItem_SCCRevHistory"), "SCCHistory", typeof(SourceControlCommands));

        public static RoutedUICommand RefreshSCC            { get { return mRefresh; } }
        public static RoutedUICommand CheckInSCC            { get { return mCheckIn; } }
        public static RoutedUICommand CheckOutSCC			{ get { return mCheckOut; } }
		public static RoutedUICommand RevertSCC				{ get { return mRevertCmd; } }
        public static RoutedUICommand RevisionHistorySCC	{ get { return mHistory; } }
    }

	public static class ObjectFactoryCommands
	{
		private static RoutedUICommand mCreateNewAssetFromFactory = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_ObjectFactory_CreateAsset"), "CreateAsset", typeof(ObjectFactoryCommands));

		public static RoutedUICommand CreateNewAsset		{ get { return mCreateNewAssetFromFactory; } }
	}


	/// <summary>
	/// Collection commands
	/// </summary>
	public static class CollectionCommands
	{
		/// Rename
		public static RoutedUICommand Rename { get { return m_Rename; } }
		private static RoutedUICommand m_Rename = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_CollectionCommand_Rename" ), "CollectionRename", typeof( CollectionCommands ) );

		/// Create Shared Copy
		public static RoutedUICommand CreateSharedCopy { get { return m_CreateSharedCopy; } }
		private static RoutedUICommand m_CreateSharedCopy = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_CollectionCommand_CreateSharedCopy" ), "CollectionCreateSharedCopy", typeof( CollectionCommands ) );

		/// Create Private Copy
		public static RoutedUICommand CreatePrivateCopy { get { return m_CreatePrivateCopy; } }
		private static RoutedUICommand m_CreatePrivateCopy = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_CollectionCommand_CreatePrivateCopy" ), "CollectionCreatePrivateCopy", typeof( CollectionCommands ) );

		/// Destroy
		public static RoutedUICommand Destroy { get { return m_Destroy; } }
		private static RoutedUICommand m_Destroy = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_CollectionCommand_Destroy" ), "CollectionDestroy", typeof( CollectionCommands ) );
	}



	/// <summary>
	/// Interaction logic for AutocompleteTextbox.xaml
	/// </summary>
    public partial class SourcesPanel : UserControl
	{
		public readonly static BitmapImage[] CollapserArrowImages;
		public readonly static BoolToImageConverter CollapsedToIconConverter;

		static SourcesPanel()
		{
			// Get handles to collapser images.
			CollapserArrowImages = new BitmapImage[] {
				(BitmapImage)Application.Current.Resources[ "imgSidewaysArrow" ],
				(BitmapImage)Application.Current.Resources[ "imgDownArrow" ] };

			CollapsedToIconConverter = new BoolToImageConverter();
		}

		/// <summary>
        /// Default constructor
        /// </summary>
        public SourcesPanel()
        {
			InitializeComponent();

			// Register handler for the "All Assets" item being selected
			this.mAllAssetsItem.Selected += new RoutedEventHandler( mAllAssetsItem_Selected );

			// Register handler for selection changing in the Collections list
			this.mSharedCollectionsList.SelectionChanged += new SelectionChangedEventHandler( SharedCollectionsList_SelectionChanged );
			this.mPrivateCollectionsList.SelectionChanged += new SelectionChangedEventHandler( PrivateCollectionsList_SelectionChanged );

			// Register handler for Collections "+" being clicked
			this.mAddSharedCollectionButton.Click += new RoutedEventHandler( mAddSharedCollectionButton_Click );
			this.mAddPrivateCollectionButton.Click += new RoutedEventHandler( mAddPrivateCollectionButton_Click );

			// Set a Validator to ensure that the user enters a valid name
			mAddSharedCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator(

					// Anonymous method for validating the shared collection name
					delegate( String NewCollectionName, String NewGroupName, Object Parameters )
					{
						return ValidateNewCollectionName( NewCollectionName, EBrowserCollectionType.Shared );
					}

				) );

			mAddPrivateCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator(

					// Anonymous method for validating the private collection name
					delegate( String NewCollectionName, String NewGroupName, Object Parameters )
					{
						// Validate private collection name
						return ValidateNewCollectionName( NewCollectionName, EBrowserCollectionType.Private );
					}

				) );

			// Register handler for user successfully entering the name of a collection to be created
			mAddSharedCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler( ConfirmCreateSharedCollection );
			mAddPrivateCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler( ConfirmCreatePrivateCollection );


			// "Rename Shared Collection"
			{
				// Set validator for renaming the collection
				mRenameSharedCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator( ValidateNewSharedCollectionName ) );

				// Register for user successfully entering the new name of a collection after initiating a rename
				mRenameSharedCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

						// Anonymous method for renaming the collection after the user clicks OK
						delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
						{
							// Grab the currently selected collection
							if( SelectedSharedCollections.Count != 1 )
							{
								throw new InvalidOperationException( "Expected only a single shared collection to be selected" );
							}
							Collection SelectedCollection = SelectedSharedCollections[ 0 ] as Collection;


							// We're naming so we'll always delete the old collection afterwards
							bool bShouldMove = true;

							// Rename it!
							bool bSucceeded =
								mMainControl.Backend.CopyCollection(
									SelectedCollection.Name,
									EBrowserCollectionType.Shared,
									InNewCollectionName,
									EBrowserCollectionType.Shared,
									bShouldMove );
						}
					);
			}


			// "Rename Private Collection"
			{
				// Set validator for renaming the collection
				mRenamePrivateCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator( ValidateNewPrivateCollectionName ) );

				// Register for user successfully entering the new name of a collection after initiating a rename
				mRenamePrivateCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

						// Anonymous method for renaming the collection after the user clicks OK
						delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
						{
							// Grab the currently selected collection
							if( SelectedPrivateCollections.Count != 1 )
							{
								throw new InvalidOperationException( "Expected only a single private collection to be selected" );
							}
							Collection SelectedCollection = SelectedPrivateCollections[ 0 ] as Collection;


							// We're renaming so we'll always delete the old collection afterwards
							bool bShouldMove = true;

							// Rename it!
							bool bSucceeded =
								mMainControl.Backend.CopyCollection(
									SelectedCollection.Name,
									EBrowserCollectionType.Private,
									InNewCollectionName,
									EBrowserCollectionType.Private,
									bShouldMove );
						}
					);

			}



			// "Create Shared -> Shared Collection Copy"
			{
				// Set validator for copying the collection
				mCreateSharedCopyOfSharedCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator( ValidateNewSharedCollectionName ) );

				// Register for user successfully entering the new name of a collection after initiating a rename
				mCreateSharedCopyOfSharedCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

						// Anonymous method for copying a shared collection to a new shared collection
						delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
						{
							ConfirmCopySelectedCollection( InNewCollectionName, EBrowserCollectionType.Shared, EBrowserCollectionType.Shared );
						}
					);
			}


			// "Create Shared -> Private Collection Copy"
			{
				// Set validator for copying the collection
				mCreatePrivateCopyOfSharedCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator( ValidateNewPrivateCollectionName ) );

				// Register for user successfully entering the new name of a collection after initiating a rename
				mCreatePrivateCopyOfSharedCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

						// Anonymous method for copying a shared collection to a new private collection
						delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
						{
							ConfirmCopySelectedCollection( InNewCollectionName, EBrowserCollectionType.Shared, EBrowserCollectionType.Private );
						}
					);
			}


			// "Create Private -> Shared Collection Copy"
			{
				// Set validator for copying the collection
				mCreateSharedCopyOfPrivateCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator( ValidateNewSharedCollectionName ) );

				// Register for user successfully entering the new name of a collection after initiating a rename
				mCreateSharedCopyOfPrivateCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

						// Anonymous method for copying a private collection to a new shared collection
						delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
						{
							ConfirmCopySelectedCollection( InNewCollectionName, EBrowserCollectionType.Private, EBrowserCollectionType.Shared );
						}
					);
			}


			// "Create Private -> Private Collection Copy"
			{
				// Set validator for copying the collection
				mCreatePrivateCopyOfPrivateCollectionPromptPopup.SetValidator( new NameEntryPrompt.Validator( ValidateNewPrivateCollectionName ) );

				// Register for user successfully entering the new name of a collection after initiating a rename
				mCreatePrivateCopyOfPrivateCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

						// Anonymous method for copying a private collection to a new private collection
						delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
						{
							ConfirmCopySelectedCollection( InNewCollectionName, EBrowserCollectionType.Private, EBrowserCollectionType.Private );
						}
					);
			}


            // "Create Local -> Shared Collection Copy"
            {
                // Set validator for copying the collection
                mCreateSharedCopyOfLocalCollectionPromptPopup.SetValidator(new NameEntryPrompt.Validator(ValidateNewPrivateCollectionName));

                // Register for user successfully entering the new name of a collection after initiating a rename
                mCreateSharedCopyOfLocalCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

                        // Anonymous method for copying a private collection to a new private collection
                        delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
                        {
                            ConfirmCopySelectedCollection( InNewCollectionName, EBrowserCollectionType.Local, EBrowserCollectionType.Shared );
                        }
                    );
            }

            // "Create Local -> Private Collection Copy"
            {
                // Set validator for copying the collection
                mCreatePrivateCopyOfLocalCollectionPromptPopup.SetValidator(new NameEntryPrompt.Validator(ValidateNewPrivateCollectionName));

                // Register for user successfully entering the new name of a collection after initiating a rename
                mCreatePrivateCopyOfLocalCollectionPromptPopup.Succeeded += new NameEntryPrompt.SucceededHandler(

                        // Anonymous method for copying a private collection to a new private collection
                        delegate( String InNewCollectionName, String NewGroupName, Object Parameters )
                        {
                            ConfirmCopySelectedCollection( InNewCollectionName, EBrowserCollectionType.Local, EBrowserCollectionType.Private );
                        }
                    );
            }


			// Register handler for Collections "-" being clicked
			this.mRemoveSharedCollectionButton.Click += new RoutedEventHandler( mRemoveSharedCollectionButton_Click );
			this.mRemovePrivateCollectionButton.Click += new RoutedEventHandler( mRemovePrivateCollectionButton_Click );

			// Register handlers for user accepting "DestroyCollection?" prompts.
			this.mDestroyPrivateCollectionPrompt.Accepted += new YesNoPrompt.PromptAcceptedHandler( mDestroyPrivateCollectionPrompt_Accepted );
			this.mDestroySharedCollectionPrompt.Accepted += new YesNoPrompt.PromptAcceptedHandler( mDestroySharedCollectionPrompt_Accepted );

			// Register handler for user accepting the prompt to add many assets to a collection
			mProceedWithAddToCollection.Accepted += new YesNoPrompt.PromptAcceptedHandler( mProceedWithAddToCollection_Accepted );

			// -- Packages View and Model Init-- 
			{
				// Initialize the sources model
				Model = new SourcesPanelModel();
				DataContext = Model;

				// Create the Tree/List View
				CreatePackageView(this.UsingFlatList);

				// Register handler for selection in package tree view changing
				this.PackagesTreeView.SelectionChanged += new CustomControls.TreeView.SelectionChangedEventHandler(PackagesTreeView_SelectionChanged);

				// Register switching between flat and tree views
				mPackageViewMode_List.Checked += new RoutedEventHandler(mPackageViewMode_Changed);
				mPackageViewMode_Tree.Checked += new RoutedEventHandler(mPackageViewMode_Changed);

				// Register handlers for altering the filter state.
				mPackageFilterText.TextChanged += PackageFilterChanged;
				mMatchAny.Checked += PackageFilterChanged;
				mMatchAny.Unchecked += PackageFilterChanged;
				mShowNonRecursive.Checked += RecursiveViewChanged;
				mShowNonRecursive.Unchecked += RecursiveViewChanged;
				mShowDirtyOnly.Checked += PackageFilterChanged;
				mShowDirtyOnly.Unchecked += PackageFilterChanged;
				mShowCheckOutOnly.Checked += PackageFilterChanged;
				mShowCheckOutOnly.Unchecked += PackageFilterChanged;
			}

		}

		#region Package Filtering and Sorting

		/// Creates the PackagesView, adds it to its place in the layout, and sets up the package tree.
		void CreatePackageView(bool UseListMode)
		{
			// Remove the any existing package views if they are present
			mPackageViewGrid.Children.Remove(mPackagesView);

			// Add a new package view
			mPackagesView = new CustomControls.TreeView();
			mPackageViewGrid.Children.Add(mPackagesView);

			mPackagesView.ItemContainerStyle = (Style)this.Resources["TreeViewItemContainerStyle"];
			mPackagesView.ItemsPanel = (ItemsPanelTemplate)this.Resources["SupportsSelectPackageFromCode"];
			Grid.SetRow(mPackagesView, 1);
			mPackagesView.BorderThickness = new Thickness(0);
			mPackagesView.Background = Brushes.Transparent;
			mPackagesView.ClearSelectionOnBackgroundClick = false;
			ScrollViewer.SetHorizontalScrollBarVisibility(mPackagesView, ScrollBarVisibility.Disabled);
			mPackagesView.AllowDrop = true;
			mPackagesView.Drop += PackagesView_Drop;
			
			CustomControls.CustomVirtualizingStackPanel.SetIsVirtualizing(mPackagesView, true);

			mPackagesView.Root = new Folder(null, "<root>") { OwnerTree = mPackagesView };
			Model.SetNewPackageView(mPackagesView);

			// @hack : when virtualization is enabled TreeViewNodes sometimes end up searching for a DataContext.
			// We make sure they find the root package, because otherwise they will find some object that they
			// are not meant to visualize.
			mPackagesView.DataContext = mPackagesView.Root;
		}

		/// Called when the user switches between list and tree mode
		void mPackageViewMode_Changed(object sender, RoutedEventArgs e)
		{
			// 1) Save view state
			ContentBrowserUIState MyContentBrowserUIState = this.mMainControl.ContentBrowserUIState;
			if (UsingFlatList)
			{
				// Save expanded packages when we're switching to tree mode
				MyContentBrowserUIState.ExpandedPackages = Model.ExpandedNodesString;
			}
			else
			{
				ClearPackageFilter();
			}

			// 2) Recreate package view
			if (mPackagesView == null)
			{
				CreatePackageView(UsingFlatList);
			}

			mPackagesView.Root = new Folder(null, "<root>") { OwnerTree = mPackagesView };
			((ListCollectionView)CollectionViewSource.GetDefaultView(this.mPackagesView.Root.Children)).Filter = (SomePkg => false);
			mMainControl.Backend.UpdatePackagesTree(UsingFlatList);
			DisableFiltering();

			if ( !this.UsingFlatList )
			{
				RestoreExpandedPackages(this.mMainControl.ContentBrowserUIState);
			}
		}

		/// Remove any filtering that is being applied to the Package View.
		public void ClearPackageFilter()
		{
			mPackageFilterText.Clear();
			mShowCheckOutOnly.IsChecked = false;
			mShowDirtyOnly.IsChecked = false;
			mShowNonRecursive.IsChecked = false;
            mMatchAny.IsChecked = false;
		}

		/// Are we viewing the flat list version of the package view?
        public bool PreviousList { get; set; }
		public bool UsingFlatList
		{
			get { return mPackageViewMode_List.IsChecked.HasValue && mPackageViewMode_List.IsChecked.Value; }
			set
			{
				mPackageViewMode_List.IsChecked = value;
				mPackageViewMode_Tree.IsChecked = !value;
			}
		}

		/// Attach a filter to the packages view
		private void EnableFiltering()
		{
			ListCollectionView PackagesCollectionView = (ListCollectionView)CollectionViewSource.GetDefaultView(this.mPackagesView.Root.Children);
			PackagesCollectionView.Filter = new Predicate<Object>(FilterPackagesMethod);
		}

		/// Remove a filter from the packages view
		private void DisableFiltering()
		{
			ListCollectionView PackagesCollectionView = (ListCollectionView)CollectionViewSource.GetDefaultView(this.mPackagesView.Root.Children);
			PackagesCollectionView.Filter = null;
		}

        /// Is a filter on the packages view
        private bool IsFiltering()
        {
            ListCollectionView PackagesCollectionView = (ListCollectionView)CollectionViewSource.GetDefaultView(this.mPackagesView.Root.Children);
            return PackagesCollectionView.Filter != null ? true : false;
        }

		/// Called when the recursive view option has changed
		void RecursiveViewChanged( object sender, RoutedEventArgs e )
		{
			if ( SelectionChanged != null )
			{
				SelectionChanged();
			}
		}

		/// Called when the user types in the Filter textbox
		void PackageFilterChanged(object sender, RoutedEventArgs e)
		{
			RefreshPackageFilter();
		}

		/// Refreshes package filter if it is active. Does nothing otherwise.
		public void RefreshPackageFilter()
		{
            if (IsPackageFilterActive())
            {
                if (!IsFiltering())
                {
                    PreviousList = UsingFlatList;
                }

                if (!UsingFlatList)
                {
                    UsingFlatList = true;
                }

                mPackageFilterTokens = new List<String>(mPackageFilterText.Text.
                    Trim().
                    ToLower().
                    Split(FilterStringDelimeters, StringSplitOptions.RemoveEmptyEntries)
                );

                EnableFiltering();
            }
            else
            {
                if (IsFiltering())
                {
                    UsingFlatList = PreviousList;
                }

                DisableFiltering();
            }
		}

		bool IsPackageFilterActive()
		{
			return
				(mShowCheckOutOnly.IsChecked ?? false) ||
				(mShowDirtyOnly.IsChecked ?? false) ||
				(mShowNonRecursive.IsChecked ?? false) ||
                (mMatchAny.IsChecked ?? false) ||
				mPackageFilterText.Text.Trim() != String.Empty;
		}

		/// Method for predicate that filters package; shows packages that match the filter text in the mPackageFilterText textbox
		private bool FilterPackagesMethod(Object CandidatePackageAsObj)
		{
			Package CandidatePackage = (Package)(CandidatePackageAsObj);

			// When both buttons are checked, being checked out || modified passes the filter
			bool bShowDirty = (mShowDirtyOnly.IsChecked ?? false);
			bool bShowCheckedOut = ( mShowCheckOutOnly.IsChecked ?? false );

			bool PassesFilter =
				(!bShowDirty && !bShowCheckedOut) ||
				(bShowDirty && CandidatePackage.IsModified) || 				
				(bShowCheckedOut && CandidatePackage.NodeIcon == ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut);

			
			if ( PassesFilter && mPackageFilterTokens.Count > 0 )
			{
				String CandidateString = CandidatePackage.DisplayName.ToLower();
                if (mMatchAny.IsChecked == false)
                {
                    PassesFilter = mPackageFilterTokens.TrueForAll(Token => CandidateString.Contains(Token));
                }
                else
                {
                    PassesFilter = !mPackageFilterTokens.TrueForAll(Token => !CandidateString.Contains(Token)); // TrueForAny
                }
			}

			return PassesFilter;
		}

		private List<String> mPackageFilterTokens = new List<String>();
		private readonly char[] FilterStringDelimeters = { ' ', '\t' };

		#endregion


		/// <summary>
		/// Triggered when selection changes in the packages treeview.
		/// </summary>
		void PackagesTreeView_SelectionChanged()
		{
			// If a package was selected, unselect the All Selected item

			ReadOnlyCollection<AbstractTreeNode> Selection = PackagesTreeView.GetSelection();

			if (Selection.Count > 0)
			{
				this.mSharedCollectionsList.UnselectAll();
				this.mPrivateCollectionsList.UnselectAll();
			}

			UpdateAllAssetItemState();

			// Trigger the SourcesPanel.SelectionChanged event
			if (SelectionChanged != null)
			{
				SelectionChanged();
			}
		}

        #region Context menus


		/// Keep track of whether we have a context menu open
		int m_NumOpenContextMenus = 0;
		public bool IsContextMenuOpen
		{
			get
			{
				return m_NumOpenContextMenus > 0;
			}
		}

		public void OnPackageTreeCMOpening( object sender, ContextMenuEventArgs EventArgs )
		{
			// Sometimes WPF forgets to call the ContextMenuClosing events, such as when a context menu's
			// closing animation gets interrupted by the user right clicking on a different asset.  We'll
			// make sure our state is good by faking these events if needed.
			while (IsContextMenuOpen)
			{
				OnPackageTreeCMClosing(sender, EventArgs);
			}

			// Get the current Context menu and clear out all its items
			FrameworkElement SenderElement = (FrameworkElement)sender;
			ContextMenu PackageCM = SenderElement.ContextMenu;
			PackageCM.Items.Clear();

			if( mMainControl != null )
			{
				// Get menu items for the menu that should be opening.
				mMainControl.Backend.PopulatePackageListMenuItems( PackageCM.Items, CommandBindings, GetType(), null, null );
			}

			// A new context menu has been opened
			++m_NumOpenContextMenus;

		}


        public void OnPackageTreeCMClosing(object sender, ContextMenuEventArgs EventArgs)
        {
			// Clear out all bindings we created when the context menu was opened
			for (int BindingIndex = CommandBindings.Count - 1; BindingIndex >= 0; BindingIndex--)
			{
				CommandBinding binding = CommandBindings[BindingIndex];

				RoutedCommand cmd = binding.Command as RoutedCommand;
				if ( cmd != null && cmd.Name == "CustomObjectCommand" )
				{
					// Only delete custom object commands.
					CommandBindings.RemoveAt(BindingIndex);
				}
			}

			// A context menu has closed
			--m_NumOpenContextMenus;
			if (m_NumOpenContextMenus < 0)
			{
				m_NumOpenContextMenus = 0;
			}
        }

        #endregion

        /// <summary>
        /// Save the current UI state into a ContentBrowserUIState object for serialization
		/// </summary>
        /// <param name="OutContentBrowserUIState">ContentBrowserUIState object which should be updated with the current UI state parameters</param>
        public void SaveContentBrowserUIState(ContentBrowserUIState OutContentBrowserUIState)
		{
			if ( !UsingFlatList )
			{
				OutContentBrowserUIState.ExpandedPackages = Model.ExpandedNodesString;
			}			
            OutContentBrowserUIState.LeftSubpanelsState = mExpandoPanel.GetPanelStateAsString();
			OutContentBrowserUIState.IsUsingFlatPackageList = this.UsingFlatList;

			//@todo save ExpandoPanel state here
		}

		#region Collections

		#region Collections Drag and Drop

		/// <summary>
		/// Called when the user is dragging something over the collections panel.
		/// </summary>
		/// <param name="e"></param>
		void AnyCollection_DragOver( object sender, DragEventArgs e )
		{
			String DropData = e.Data.GetData(typeof(String)) as String;
			if (DropData != null)
			{
				//@todo cb [reviewed; discuss]- check e.source?
				bool bDraggingValidAsset = false;

				List<String> DraggedAssetItems = mMainControl.Backend.UnmarshalAssetFullNames(DropData);
				foreach (String AssetFullName in DraggedAssetItems)
				{
					// if at least one asset is valid for tagging (which implies that it is valid for being in a collection)
					// allow the drop
					if (mMainControl.Backend.IsAssetValidForTagging(AssetFullName))
					{
						bDraggingValidAsset = true;
						break;
					}
				}

				if (bDraggingValidAsset)
				{
					e.Effects = DragDropEffects.Copy;
				}
				else
				{
					e.Effects = DragDropEffects.None;
				}
			}
			else
			{
				// No valid data is being dragged over.
				e.Effects = DragDropEffects.None;
			}
			e.Handled = true;
		}
		/// Handle an object being dropped onto a collection
		void SharedCollection_Drop( object sender, DragEventArgs e )
		{
			AnyCollection_Drop( sender, e, EBrowserCollectionType.Shared );
		}

		/// Handle an object being dropped onto a collection
		void PrivateCollection_Drop( object sender, DragEventArgs e )
		{
			AnyCollection_Drop( sender, e, EBrowserCollectionType.Private );
		}

		/// Handle an object being dropped onto a collection
		void AnyCollection_Drop(object sender, DragEventArgs e, EBrowserCollectionType InCollectionType)
		{
			// @todo I am not sure I understand the GetData() method entirely.
			String DropData = e.Data.GetData(typeof(String)) as String;
			if (DropData != null)
			{
				Collection TargetCollection = ((CollectionVisual)sender).AssetCollection;

				if (TargetCollection != null)
				{
					List<String> DraggedAssetFullNames = mMainControl.Backend.UnmarshalAssetFullNames(DropData);

					bool ShouldShowConfirmationPrompt = mMainControl.Backend.ShouldShowConfirmationPrompt(ConfirmationPromptType.AddAssetsToCollection);
					if (!ShouldShowConfirmationPrompt ||
						DraggedAssetFullNames.Count < ContentBrowser.ContentBrowserDefs.MaxNumAssetsForNoWarnGadOperation)
					{
						AddAssetsToCollection(DraggedAssetFullNames, TargetCollection, TargetCollection.IsLocal ? EBrowserCollectionType.Local : InCollectionType );
					}
					else
					{
						mProceedWithAddToCollection.PlacementTarget = (Control)sender;
                        String PromptTextKey = (InCollectionType == EBrowserCollectionType.Private) ? TargetCollection.IsLocal ?
                                                "ContentBrowser_AddToCollectionPrompt_Prompt(Local)" :
                                                "ContentBrowser_AddToCollectionPrompt_Prompt(Private)" :
                                                "ContentBrowser_AddToCollectionPrompt_Prompt(Shared)";
						mProceedWithAddToCollection.PromptText = Utils.Localize(PromptTextKey, DraggedAssetFullNames.Count, TargetCollection.Name);
						mProceedWithAddToCollection.ShowOptionToSuppressFuturePrompts = true;
						mProceedWithAddToCollection.SuppressFuturePrompts = !ShouldShowConfirmationPrompt;
                        mProceedWithAddToCollection.Show(new Object[] { DraggedAssetFullNames, TargetCollection, TargetCollection.IsLocal ? EBrowserCollectionType.Local : InCollectionType });
					}

				}
			}
		}

		/// Called when the user drags over the package tree
		void PackagesView_DragOver( object sender, DragEventArgs e )
		{
			// Ignore possible drag/drop requests if the user isn't dropping files. 
			if (e.Data.GetDataPresent(DataFormats.FileDrop, false) == true)
			{
				e.Effects = DragDropEffects.Copy;
			}
			else
			{
				e.Effects = DragDropEffects.None;
			}
			e.Handled = true;
		}

		/// Called when the user drops files onto the package tree  with the mouse
		void PackagesView_Drop( object sender, DragEventArgs e )
		{
			// Ignore possible drag/drop requests if the user isn't dropping files. 
			if (e.Data.GetDataPresent(DataFormats.FileDrop, false) == true)
			{
				// Pass a list of filename strings to the main control	
				string[] strings = (string[])e.Data.GetData(DataFormats.FileDrop, false);
	    
				List<String> Filenames = new List<String>();
				for (int strIdx = 0; strIdx < strings.Length; ++strIdx)
					Filenames.Add(strings[strIdx]);
	    
				// Import the files
				mMainControl.Backend.BeginDragDropForImport(Filenames);
			}
			e.Handled = true;
		}

		/// <summary>
		/// Called when the user accepts the prompt to add many assets to a collection.
		/// </summary>
		/// <param name="Parameters">In this case, an array of AssetNames to Add, the target collection, the type of target collection.</param>
		void mProceedWithAddToCollection_Accepted( object Parameters )
		{
			Object[] Params = (Object[])Parameters;
			AddAssetsToCollection( (List<String>)Params[0], (Collection)Params[1], (EBrowserCollectionType)Params[2] );

			if( mProceedWithAddToCollection.SuppressFuturePrompts )
			{
				mMainControl.Backend.DisableConfirmationPrompt( ConfirmationPromptType.AddAssetsToCollection );
			}
		}

		/// <summary>
		/// Add assets to a collection.
		/// </summary>
		/// <param name="AssetFullNamesToAdd">List of asset full names to add.</param>
		/// <param name="TargetCollection">Collection to which to add.</param>
		/// <param name="InCollectionType">Type of collection (public/private) to which to add.</param>	
		void AddAssetsToCollection( ICollection<String> AssetFullNamesToAdd, Collection TargetCollection, EBrowserCollectionType InCollectionType )
		{
			if ( this.mMainControl.Backend.AddAssetsToCollection( AssetFullNamesToAdd, TargetCollection, InCollectionType ) )
			{
				// Display a Content Browser-wide notification
				String Message = UnrealEd.Utils.Localize( "ContentBrowser_Notification_AddedAssetsToCollection" );
				Message = String.Format( Message, AssetFullNamesToAdd.Count, TargetCollection.Name );
				mMainControl.PlayNotification( Message );
			}
		}

		#endregion

		/// Called when asset selection changes.
		void AssetView_AssetSelectionChanged( object sender, AssetView.AssetSelectionChangedEventArgs args )
		{
			// Update whether we have selected items.
			AreItemsSelected = mMainControl.AssetView.SelectedCount != 0;
		}

		/// Do we have any selected items?
		public bool AreItemsSelected
		{
			get { return (bool)GetValue( AreItemsSelectedProperty ); }
			set { SetValue( AreItemsSelectedProperty, value ); }
		}
		// Using a DependencyProperty as the backing store for AreItemsSelected.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty AreItemsSelectedProperty =
			DependencyProperty.Register( "AreItemsSelected", typeof( bool ), typeof( SourcesPanel ), new UIPropertyMetadata( false ) );

		/// Called when a user clicks the add-to-collection button for a shared collection
		protected void CollectionVisual_AddToSharedCollectionClicked( CollectionVisual Sender, ContentBrowser.Collection CollectionRepresentedByVisual )  
		{
			AddAssetsToCollection( mMainControl.AssetView.CloneSelectedAssetFullNames(), CollectionRepresentedByVisual, EBrowserCollectionType.Shared );			
		}

		/// Called when a user clicks the add-to-collection button for a private collection
		protected void CollectionVisual_AddToPrivateCollectionClicked( CollectionVisual Sender, ContentBrowser.Collection CollectionRepresentedByVisual )
		{
            AddAssetsToCollection(mMainControl.AssetView.CloneSelectedAssetFullNames(), CollectionRepresentedByVisual, CollectionRepresentedByVisual.IsLocal ? EBrowserCollectionType.Local : EBrowserCollectionType.Private );
		}


		///  Get the list of selected shared collections
		public System.Collections.IList SelectedSharedCollections { get { return mSharedCollectionsList.SelectedItems; } }

		///  Get the list of selected private collections
		public System.Collections.IList SelectedPrivateCollections { get { return mPrivateCollectionsList.SelectedItems; } }

		/// Get a list of selected collections
		public List<String> GetSelectedCollectionNames( EBrowserCollectionType InType )
		{
			var CollectionsList = InType == EBrowserCollectionType.Shared ? mSharedCollectionsList : mPrivateCollectionsList;
			List<String> NamesList = new List<String>( CollectionsList.SelectedItems.Count );
			foreach ( Collection CollectionToAdd in CollectionsList.SelectedItems )
			{
                // local collections are in the same list as private collections but we should only add them to the name list if we are requesting local collections 
                if ( (InType != EBrowserCollectionType.Local && !CollectionToAdd.IsLocal) || ( InType == EBrowserCollectionType.Local && CollectionToAdd.IsLocal ) )
                {
                    NamesList.Add(CollectionToAdd.Name);
                }
			}

			return NamesList;
		}

		/// Describes what a valid string of tags looks like. ^ and $ needed to force match entire string.
		private static Regex mNameValidator = new Regex(@"^[\w ]+$", RegexOptions.IgnoreCase);

		/// Create the shared collection (if no errors are present)
		void ConfirmCreateSharedCollection( String NewCollectionName, String InGroupName, Object Parameters )
		{
			mMainControl.Backend.CreateCollection( NewCollectionName, EBrowserCollectionType.Shared );
		}

		/// Create the private collection (if no errors are present)
		void ConfirmCreatePrivateCollection( String NewCollectionName, String InGroupName, Object Parameters )
		{
			mMainControl.Backend.CreateCollection( NewCollectionName, EBrowserCollectionType.Private );
		}


		/// Copy the collection
		void ConfirmCopySelectedCollection( String InNewCollectionName, EBrowserCollectionType SourceType, EBrowserCollectionType TargetType )
		{
			// Grab the currently selected collection in the source list
			var SelectedCollections = SourceType == EBrowserCollectionType.Shared ? SelectedSharedCollections : SelectedPrivateCollections;
			if( SelectedCollections.Count != 1 )
			{
				throw new InvalidOperationException( "Expected only a single shared collection to be selected" );
			}
			Collection SelectedCollection = SelectedCollections[ 0 ] as Collection;


			// We're copying so we'll leave the old collection along afterwards
			bool bShouldMove = false;

			// Copy it!
			bool bSucceeded =
				mMainControl.Backend.CopyCollection(
					SelectedCollection.Name,
					SourceType,
					InNewCollectionName,
					TargetType,
					bShouldMove );
		}


		/// <summary>
		/// Check the collection name and return the error text if there is one.
		/// </summary>
		/// <param name="NewCollectionName">Proposed collection name</param>
		/// <param name="InCollectionType">Type of collection</param>
		/// <returns>Error string; null if there is no error</returns>
		String ValidateNewCollectionName( String NewCollectionName, EBrowserCollectionType InCollectionType )
		{
			if( NewCollectionName.Length <= 0 )
			{
				// Name must not be empty
				return Utils.Localize( "ContentBrowser_NewCollection_EmptyNameError" );
			}
			else if( !mNameValidator.IsMatch( NewCollectionName ) )
			{
				// Only letters and numbers are allowed. (and spaces, but we don't mention that)
				return Utils.Localize( "ContentBrowser_NewCollection_DisallowedCharactersError" );
			}

			// Make sure that the collection doesn't already exist
			var ExistingCollections = InCollectionType == EBrowserCollectionType.Shared ? Model.SharedCollections : Model.PrivateCollections;
			foreach( Collection CurCollection in ExistingCollections )
			{
				if( CurCollection.Name.Equals( NewCollectionName, StringComparison.OrdinalIgnoreCase ) )
				{
					// Name must not match an existing collection!
					return Utils.Localize( "ContentBrowser_NewCollection_NameCollision" );
				}
			}

			// Good to go!
			return null;
		}



		/// <summary>
		/// Check the shared collection name and return the error text if there is one.
		/// </summary>
		/// <param name="NewCollectionName">Proposed collection name</param>
		/// <param name="NewGroupName">Proposed group name (ingored)</param>
		/// <returns>Error string; null if there is no error</returns>
		String ValidateNewSharedCollectionName( String NewCollectionName, String NewGroupName, Object Parameters )
		{
			return ValidateNewCollectionName( NewCollectionName, EBrowserCollectionType.Shared );
		}


		/// <summary>
		/// Check the private collection name and return the error text if there is one.
		/// </summary>
		/// <param name="NewCollectionName">Proposed collection name</param>
		/// <param name="NewGroupName">Proposed group name (ingored)</param>
		/// <returns>Error string; null if there is no error</returns>
		String ValidateNewPrivateCollectionName( String NewCollectionName, String NewGroupName, Object Parameters )
		{
			return ValidateNewCollectionName( NewCollectionName, EBrowserCollectionType.Private );
		}

	
		/// Handle user clicking on "+" button under Collections
		void mAddSharedCollectionButton_Click(object sender, RoutedEventArgs e)
		{
			this.mSharedCollectionsSizer.IsCollapsed = false;
			mAddSharedCollectionPromptPopup.Show();
		}

		/// Handle user clicking on "+" button under Collections
		void mAddPrivateCollectionButton_Click( object sender, RoutedEventArgs e )
		{
			this.mPrivateCollectionsSizer.IsCollapsed = false;			
			mAddPrivateCollectionPromptPopup.Show();
		}


		/// Destroy selected collections
		void ConfirmDestroyingCollection( EBrowserCollectionType InCollectionListType )
		{
			String FormatString = UnrealEd.Utils.Localize( "ContentBrowser_Notification_CollectionDestroyed" );
			String Message = "";

            var CollectionsList = InCollectionListType == EBrowserCollectionType.Shared ? mSharedCollectionsList : mPrivateCollectionsList;

			foreach ( Collection SelectedCollection in CollectionsList.SelectedItems )
			{
                if (this.mMainControl.Backend.DestroyCollection(SelectedCollection.Name, SelectedCollection.IsLocal ? EBrowserCollectionType.Local : InCollectionListType))
				{
					// Display a Content Browser-wide notification
					if ( Message.Length > 0 )
					{
						Message += "\r\n";
					}

					Message += String.Format(FormatString, SelectedCollection.Name);
				}
			}

			if ( Message.Length > 0 )
			{
				mMainControl.PlayNotification(Message);
			}
		}


		/// Asks the user if they want to destroy the selected shared collections, and if so, destroys them!
		public void ShowPromptToDestroySelectedSharedCollections()
		{
			String Message = UnrealEd.Utils.Localize( "ContentBrowser_DestroyCollection_Message", mSharedCollectionsList.SelectedItems.Count );
			mDestroySharedCollectionPrompt.PromptText = Message;
			mDestroySharedCollectionPrompt.Show( null );
		}


		/// Handle user clicking the "-" button under Collections
		void mRemoveSharedCollectionButton_Click( object sender, RoutedEventArgs e )
		{
			this.mSharedCollectionsSizer.IsCollapsed = false;
			ShowPromptToDestroySelectedSharedCollections();
		}


		/// Asks the user if they want to destroy the selected private collections, and if so, destroys them!
		public void ShowPromptToDestroySelectedPrivateCollections()
		{
			String Message = UnrealEd.Utils.Localize( "ContentBrowser_DestroyCollection_Message", mPrivateCollectionsList.SelectedItems.Count );
			mDestroyPrivateCollectionPrompt.PromptText = Message;
			mDestroyPrivateCollectionPrompt.Show( null );
		}


		/// Handle user clicking the "-" button under Collections
		void mRemovePrivateCollectionButton_Click( object sender, RoutedEventArgs e )
		{
			this.mPrivateCollectionsSizer.IsCollapsed = false;
			ShowPromptToDestroySelectedPrivateCollections();
		}

		/// Handle user accepting the destruction of a shared collection
		void mDestroySharedCollectionPrompt_Accepted( object Parameters )
		{
			ConfirmDestroyingCollection(EBrowserCollectionType.Shared);
		}

		/// Handle user accepting the destruction of a private collection
		void mDestroyPrivateCollectionPrompt_Accepted( object Parameters )
		{
			ConfirmDestroyingCollection( EBrowserCollectionType.Private );
		}


		/// Called when the user asks to rename the selected collection
		public void ShowPromptToRenameCollection( EBrowserCollectionType InCollectionType )
		{
			if( InCollectionType == EBrowserCollectionType.Shared )
			{
				mRenameSharedCollectionPromptPopup.Show();
			}
			else
			{
				mRenamePrivateCollectionPromptPopup.Show();
			}
		}


		/// Called when the user asks to copy the selected collection
		public void ShowPromptToCopyCollection( EBrowserCollectionType InSourceType, EBrowserCollectionType InTargetType )
		{
			if( InSourceType == EBrowserCollectionType.Shared )
			{
				if( InTargetType == EBrowserCollectionType.Shared )
				{
					mCreateSharedCopyOfSharedCollectionPromptPopup.Show();
				}
				else
				{
					mCreatePrivateCopyOfSharedCollectionPromptPopup.Show();
				}
			}
            else if (InSourceType == EBrowserCollectionType.Private)
            {
                if (InTargetType == EBrowserCollectionType.Shared)
                {
                    mCreateSharedCopyOfPrivateCollectionPromptPopup.Show();
                }
                else
                {
                    mCreatePrivateCopyOfPrivateCollectionPromptPopup.Show();
                }
            }
            else
            {
                if (InTargetType == EBrowserCollectionType.Shared)
                {
                    mCreateSharedCopyOfLocalCollectionPromptPopup.Show();
                }
                else
                {
                    mCreatePrivateCopyOfLocalCollectionPromptPopup.Show();
                }
            }
		}

	
		/// Handle selection changing in the shared collections list
		void SharedCollectionsList_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{
			AnyCollectionsList_SelectionChanged( sender, e, EBrowserCollectionType.Shared );
		}


		/// Handle selection changing in the private collections list
		void PrivateCollectionsList_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{
			AnyCollectionsList_SelectionChanged( sender, e, EBrowserCollectionType.Private );
		}


		/// Handle selection changing in the collections list
		void AnyCollectionsList_SelectionChanged( object sender, SelectionChangedEventArgs e, EBrowserCollectionType InType )
		{
			bool bSharedCollectionSelectionPresent = mSharedCollectionsList.SelectedItems.Count > 0;
			bool bPrivateCollectionSelectionPresent = mPrivateCollectionsList.SelectedItems.Count > 0;

			if( bSharedCollectionSelectionPresent || bPrivateCollectionSelectionPresent )
			{
				// Packages should never be selected simultaneously with collections
				mPackagesView.ClearSelection();

				if( InType == EBrowserCollectionType.Shared )
				{
					if( bSharedCollectionSelectionPresent )
					{
						mPrivateCollectionsList.UnselectAll();
					}
					else if( bPrivateCollectionSelectionPresent )
					{
						mSharedCollectionsList.UnselectAll();
					}
				}
				else
				{
					if( bPrivateCollectionSelectionPresent )
					{
						mSharedCollectionsList.UnselectAll();
					}
					else if( bSharedCollectionSelectionPresent )
					{
						mPrivateCollectionsList.UnselectAll();
					}
				}
			}

			UpdateAllAssetItemState();

			// The remove-collection button should be disabled unless we have a selected collection
			mRemoveSharedCollectionButton.IsEnabled = mSharedCollectionsList.SelectedItems.Count > 0 && mMainControl.Backend.IsUserCollectionsAdmin();
			mRemovePrivateCollectionButton.IsEnabled = mPrivateCollectionsList.SelectedItems.Count > 0;

			List<String> SharedCollectionNames = GetSelectedCollectionNames( ContentBrowser.EBrowserCollectionType.Shared );
			if (SharedCollectionNames.Count > 0)
			{
				ContentBrowserUIState MyContentBrowserUIState = this.mMainControl.ContentBrowserUIState;
				MyContentBrowserUIState.SelectedSharedCollection = SharedCollectionNames[0];
			}
			
			// Emit the selection changed event
			if( SelectionChanged != null )
			{
				SelectionChanged();
			}
		}


		#endregion



		/// Handle Special Collections subpanel collapsed / expanded
		private void m_SpecialCollectionsExpander_Toggled(object sender, RoutedEventArgs e)
		{
			//this.mMainControl.LayoutInfo.AreSpecialCollectionsExpanded = m_SpecialCollectionsExpander.IsExpanded;
		}

        /// Restore the UI state from the ContentBrowserUIState
        public void RestoreContentBrowserUIState()
		{
            ContentBrowserUIState MyContentBrowserUIState = this.mMainControl.ContentBrowserUIState;

            mExpandoPanel.RestoreFromString( MyContentBrowserUIState.LeftSubpanelsState );

            this.PreviousList = this.UsingFlatList = MyContentBrowserUIState.IsUsingFlatPackageList;

			if (MyContentBrowserUIState.SelectedSharedCollection.Trim().Length > 0)
			{
				SelectCollection(MyContentBrowserUIState.SelectedSharedCollection, ContentBrowser.EBrowserCollectionType.Shared);
			}
		}

		public void RestoreExpandedPackages( ContentBrowserUIState InContentBrowserUIState )
		{
			if (InContentBrowserUIState.ExpandedPackages.Replace(" ", "") != "")
			{
				// Restore the expanded nodes
				char[] PathsSeparator = { SourcesPanelModel.TreeViewPathSeparator };
				List<String> PathsOfExpandedNodes = new List<String>(InContentBrowserUIState.ExpandedPackages.Split(PathsSeparator, StringSplitOptions.RemoveEmptyEntries));
				foreach (String Path in PathsOfExpandedNodes)
				{
					SourceTreeNode Node = Model.FindDescendantNode(Path);
					if (Node != null)
					{
						Node.IsExpanded = true;
					}
				}

			}
			else
			{
				// Don't know which nodes were previously expanded; expand the paths mentioned in the Paths variable
				mMainControl.Backend.ExpandDefaultPackages();
			}
		}

		/// Set the AllAsset Item's Selected State
		void UpdateAllAssetItemState()
		{
			bool NewSelectionState =
				PackagesTreeView.GetSelection().Count == 0 &&
				this.mSharedCollectionsList.SelectedItems.Count == 0 &&
				this.mPrivateCollectionsList.SelectedItems.Count == 0;
			if (NewSelectionState != this.mAllAssetsItem.IsSelected)
			{
				this.mAllAssetsItem.IsSelected = NewSelectionState;
			}
		}

		/// <summary>
		/// Called when the All Assets item is selected
		/// </summary>
		/// <param name="sender">ignored</param>
		/// <param name="e">event arguments</param>
		void mAllAssetsItem_Selected(object sender, RoutedEventArgs e)
		{
			mPackagesView.ClearSelection();
			mSharedCollectionsList.UnselectAll();
			mPrivateCollectionsList.UnselectAll();
			
			if (SelectionChanged != null)
			{
				SelectionChanged();
			}
			
		}


		/// <summary>
		/// Resets the sources panel such that "All Assets" is selected and clears everything else
		/// </summary>
		public void ResetSourceSelectionToAllAssets()
		{
			// Clear the selection in the packages list
			mPackagesView.ClearSelection();

			// Clear the selection in the collections list
			mSharedCollectionsList.UnselectAll();
			mPrivateCollectionsList.UnselectAll();

			// Select "All Assets"
			mAllAssetsItem.IsSelected = true;

			// Update everything!
			if ( SelectionChanged != null )
			{
				SelectionChanged();
			}
		}

		#region Source Selection

		/// <summary>
		/// Delegate type for handling selection changes in the source panel
		/// </summary>
		public delegate void SelectionChangedDelegate();

		/// <summary>
		/// Triggered whenever the source selection changes.
		/// </summary>
		public event SelectionChangedDelegate SelectionChanged;

		#endregion


		/// <summary>
		/// Initialize the sources panel
		/// </summary>
		/// <param name="InContentBrowser">Content browser that the sources panel is associated with</param>
		public void Init( MainControl InContentBrowser )
		{
			Model.Init( InContentBrowser );
			this.m_SpecialCollectionsList.SelectedItem = mAllAssetsItem;
			mMainControl = InContentBrowser;
			this.mAddSharedCollectionButton.IsEnabled = mMainControl.Backend.IsUserCollectionsAdmin();

			mMainControl.AssetView.AssetSelectionChanged += AssetView_AssetSelectionChanged;
		}

		private MainControl mMainControl;

        /// <summary>
        /// The ViewModel for the SourcesPanel
        /// </summary>
		public SourcesPanelModel Model { get; set; }

        /// <summary>
        /// The TreeView that shows the packages
        /// </summary>
		public CustomControls.TreeView PackagesTreeView { get { return mPackagesView; } }

		/// <summary>
		/// Clears the current selection and selects the specified packages in the tree view.
		/// </summary>
		/// <param name="PackagesToSelect"></param>
		/// <returns>True if package selection was changed; false if we did not change the package selection (usually due to the trivial case that we wanted to select one package and it was already selected)</returns>
		public bool SetSelectedPackages( List<ObjectContainerNode> PackagesToSelect )
		{
			bool SyncWasNecessary = Model.SynchronizeSelection(PackagesToSelect);
			if ( SyncWasNecessary )
			{
				mPackagesSizer.IsCollapsed = false;
			}

			return SyncWasNecessary;
		}

		/// Set the source to AllAssets.
		public void SetSourceToAllAssets()
		{
			this.mAllAssetsItem.IsSelected = true;
		}


		/// <summary>
		/// Returns true if any nodes are selected (type-invariant)
		/// </summary>
		public bool AnyNodesSelected()
		{
			return PackagesTreeView.GetSelection().Count > 0;
		}


		/// <summary>
		/// All selected nodes, regardless of type
		/// </summary>
		/// <returns></returns>
		public ReadOnlyCollection<SourceTreeNode> MakeSelectedNodeList()
		{
			ReadOnlyCollection<AbstractTreeNode> SelectedTreeNodes = PackagesTreeView.GetSelection();
			List<SourceTreeNode> MySelectedNodes = new List<SourceTreeNode>();
			foreach ( AbstractTreeNode SelectedTreeNode in SelectedTreeNodes )
			{
				SourceTreeNode SelectedNode = SelectedTreeNode as SourceTreeNode;
				if ( SelectedNode != null )
				{
					// if( MySelectedNodes.Contains( SelectedNode ) ) throw new Exception( "Error: Duplicate entry detected" );
					MySelectedNodes.Add(SelectedNode);
				}

				Model.GetChildNodes<SourceTreeNode>(SelectedNode, MySelectedNodes, true);
			}

			return MySelectedNodes.AsReadOnly();
		}

		/** An empty node to act as a placeholder package */
		ObjectContainerNode EmptyPackageNode = new Package(null, "EmptyPackageNode");

		/// <summary>
		/// All packages and groups selected
		/// </summary>
		public ReadOnlyCollection<ObjectContainerNode> MakeSelectedPackageAndGroupList()
        {
			ReadOnlyCollection<AbstractTreeNode> SelectedTreeNodes = PackagesTreeView.GetSelection();
			List<ObjectContainerNode> MySelectedPackages = new List<ObjectContainerNode>();

			foreach ( SourceTreeNode SelectedTreeNode in SelectedTreeNodes )
			{
				ObjectContainerNode SelectedNode = SelectedTreeNode as ObjectContainerNode;
				if ( SelectedNode != null )
				{
					// if( MySelectedPackages.Contains( SelectedNode ) ) throw new Exception( "Error: Duplicate entry detected" );
					MySelectedPackages.Add(SelectedNode);
				}

				if ( mShowNonRecursive.IsChecked == false )
				{
					Model.GetChildNodes<ObjectContainerNode>(SelectedTreeNode, MySelectedPackages, true);
				}
			}

			if ( mShowNonRecursive.IsChecked == true && SelectedTreeNodes.Count != 0 && MySelectedPackages.Count == 0 )
			{
				// Add a placeholder package node so "All Assets" doesn't become selected
				MySelectedPackages.Add(EmptyPackageNode);
			}

			return MySelectedPackages.AsReadOnly();
        }

		/// <summary>
		/// Get all selected top-level packages.
		/// </summary>
		/// <returns></returns>
		public ReadOnlyCollection<Package> MakeSelectedTopLevelPackageList()
		{
			List<Package> PackageList = new List<Package>();

			ReadOnlyCollection<ObjectContainerNode> SelectedPackageGroups = MakeSelectedPackageAndGroupList();
			foreach ( ObjectContainerNode node in SelectedPackageGroups )
			{
				Package SelectedPackage = node.OutermostPackage;

				// Other selected groups may have already added this outermost package, so we need to check
				// to make sure we're not adding a duplicate entry
				if ( !PackageList.Contains(SelectedPackage) )
				{
					PackageList.Add(SelectedPackage);
				}
			}
			return PackageList.AsReadOnly();
		}

        /// <summary>
        /// Recursive method that adds the paths of all the children to a list
        /// </summary>
        /// <param name="Parent">The parent package</param>
        /// <param name="ChildList">Children's names</param>
		private void HarvestChildFullNames( ObjectContainerNode Parent, NameSet ChildNameList )
        {
			foreach ( ObjectContainerNode Child in Parent.Children )
            {
				// if( ChildNameList.Contains( Child.ObjectPathName ) ) throw new Exception( "Error: Duplicate entry detected" );
                ChildNameList.Add(Child.ObjectPathName);
                HarvestChildFullNames(Child, ChildNameList);
            }
        }


		/// <summary>
		/// Fully qualified names of all the selected packages or groups and their children.
		/// <param name="OutSelectedPathNames">All groups and packages that are selected or are in a selected folder.</param>
		/// <param name="OutSelectedOutermostFullNames">All packages that are selected, are in a selected folder, or contain one or more selected groups.</param>
		/// <param name="OutExplicitlySelectedOutermostPackages">All packages that are selected or are in a selected folder.</param>
		/// </summary>
		public void MakeSelectedPathNameAndOutermostFullNameList( out NameSet OutSelectedPathNames, out NameSet OutSelectedOutermostFullNames, out NameSet OutExplicitlySelectedOutermostPackages )
		{
			ReadOnlyCollection<ObjectContainerNode> selectedPackages = MakeSelectedPackageAndGroupList();
			OutSelectedPathNames = new NameSet();
			OutSelectedOutermostFullNames = new NameSet();
			OutExplicitlySelectedOutermostPackages = new NameSet();
			foreach( ObjectContainerNode pkg in selectedPackages )
			{
				OutSelectedPathNames.Add( pkg.ObjectPathName );

				if ( mShowNonRecursive.IsChecked == false )
				{
					HarvestChildFullNames( pkg, OutSelectedPathNames );
				}

				OutSelectedOutermostFullNames.Add( pkg.OutermostPackage.Name );
				// Is this package an outermost package?
				if ( pkg.GetType() == typeof(Package) )
				{
					OutExplicitlySelectedOutermostPackages.Add( pkg.OutermostPackage.Name );
				}
			}
        }

		/// <summary>
		/// Selects a collection
		/// <param name="InCollectionName">The name of the collection to create.</param>
		/// <param name="InType">The type of collection to select</param>
		/// </summary>
        public void SelectCollection( String InCollectionName, ContentBrowser.EBrowserCollectionType InType )
        {
			// Clear out all other sources panel collections
            mPackagesView.ClearSelection();
			mSharedCollectionsList.UnselectAll();
			mPrivateCollectionsList.UnselectAll();

            if( InType == ContentBrowser.EBrowserCollectionType.Shared )
            {
				// Find the collection name in the list of shared collections and select it if it exists
                foreach( Collection MyCollection in mSharedCollectionsList.Items )
				{
					if( MyCollection.Name == InCollectionName )
                    {
                        mSharedCollectionsList.SelectedItems.Add( MyCollection );
						// Collection names are unique, no need to keep searching
                        break;
                    }
				}
            }
            else
            {
                foreach( Collection MyCollection in mPrivateCollectionsList.Items )
				{
					// Find the collection name in the list of private collections and select it if it exists
					if( MyCollection.Name == InCollectionName )
                    {
						
                        if( InType == ContentBrowser.EBrowserCollectionType.Private && MyCollection.IsLocal == false )
                        {
                            mPrivateCollectionsList.SelectedItems.Add(MyCollection);
                            break;
                        }
                        else if (InType == ContentBrowser.EBrowserCollectionType.Local && MyCollection.IsLocal == true)
                        {
                            mPrivateCollectionsList.SelectedItems.Add(MyCollection);
                            break;
                        }
                    }
				}
            }
        }

        #region History
        /// <summary>
		/// Saves important history data so the user can restore it later.  Called when taking a history snapshot of the content browser
		/// </summary>
		/// <param name="HistoryData">The history data object that will be saved</param>
		/// <param name="bFullSave">If true we should save all important history data. If false only save data that should be updated during forward and back calls</param>
		public void SaveHistoryData( ContentBrowserHistoryData HistoryData, bool bFullSave )
		{
			if( bFullSave )
			{
				// Perform a full save 

				// Get a list of selected tree items
				ReadOnlyCollection<AbstractTreeNode> SelectedItems = PackagesTreeView.GetSelection();

				HistoryData.SelectedPackageTreeItems = new List<String>(SelectedItems.Count);
				foreach (SourceTreeNode Node in SelectedItems)
				{
					// Save the full path of each selected tree item.  We store the strings of each selected item
					// instead of the actual node because nodes sometimes get deleted and re-added which invalidates any stored references we would have.
					// For example, this happens when fully loading a package.
					HistoryData.SelectedPackageTreeItems.Add(Node.FullTreeviewPath);
				}

				// Get a list of selected private and shared collection names.
				HistoryData.SelectedSharedCollections = GetSelectedCollectionNames(EBrowserCollectionType.Shared);
				HistoryData.SelectedPrivateCollections = GetSelectedCollectionNames(EBrowserCollectionType.Private);
			}
		}

		/// <summary>
		/// Restores important history data.  Called when restoring a history snapshot requested by a user
		/// </summary>
		/// <param name="HistoryData">The history data object containing data to restore</param>
		public void RestoreHistoryData( ContentBrowserHistoryData HistoryData )
		{
			// Clear all selections as we are about to restore different selection sets
			mPackagesView.ClearSelection();
			mSharedCollectionsList.UnselectAll();
			mPrivateCollectionsList.UnselectAll();

			if (HistoryData.SelectedPackageTreeItems.Count > 0)
			{
				// Find the tree item corresponding to each string in our stored history.  We store the strings of each selected item
				// instead of the actual node because nodes sometimes get deleted and re-added which invalidates any stored references we would have, 
				// causing the correct nodes to not be selected
				List<AbstractTreeNode> NodesToSelect = new List<AbstractTreeNode>(HistoryData.SelectedPackageTreeItems.Count);
				foreach (String Path in HistoryData.SelectedPackageTreeItems )
				{
					// Make a list of nodes to select.
					AbstractTreeNode NodeToSelect = Model.FindDescendantNode(Path);
					if( NodeToSelect != null )
					{
						NodesToSelect.Add(NodeToSelect);
					}
				}
				// Select the nodes
				mPackagesView.Select(NodesToSelect);
			}
			else if( HistoryData.SelectedPrivateCollections.Count > 0 )
			{
				// Private collections were selected
				// Go through each collection and see if it exists in the history data
				// If it does, select it.
				foreach( Collection PrivateCollection in mPrivateCollectionsList.Items)
				{
					if (HistoryData.SelectedSharedCollections.Contains(PrivateCollection.Name))
					{
						mPrivateCollectionsList.SelectedItems.Add(PrivateCollection);
					}
				}
			}
			else if( HistoryData.SelectedSharedCollections.Count > 0 )
			{
				// Shared collections were selected
				// Go through each collection and see if it exists in the history data
				// If it does, select it.
				foreach (Collection c in mSharedCollectionsList.Items)
				{
					if (HistoryData.SelectedSharedCollections.Contains(c.Name))
					{
						mSharedCollectionsList.SelectedItems.Add(c);
					}
				}
			}
        }

        #endregion

        #region CommandBindings

        /// <summary>
		/// CanExecuteRoutedEventHandler for the ContentBrowser's custom commands
		/// </summary>
		/// <param name="Sender"></param>
		/// <param name="EvtArgs"></param>
		private void CanExecuteMenuCommand(object Sender, CanExecuteRoutedEventArgs EvtArgs)
		{
			if ( mMainControl != null )
			{
				mMainControl.Backend.CanExecuteMenuCommand(Sender, EvtArgs);
			}
		}

		/// <summary>
		/// ExecutedRoutedEventHandler for the ContentBrowser's custom commands
		/// </summary>
		/// <param name="Sender"></param>
		/// <param name="EvtArgs"></param>
		private void ExecuteMenuCommand( object Sender, ExecutedRoutedEventArgs EvtArgs )
		{
			if ( mMainControl != null )
			{
				mMainControl.Backend.ExecuteMenuCommand(Sender, EvtArgs);
			}
		}

		#endregion

		private CustomControls.TreeView mPackagesView;
	}

	#region Converters

	/// Bool to image converter.
	/// Takes an array of 2 image resources as parameter.
	/// Returns the 1st resource if the binding is true, the 2nd otherwise.
	[ValueConversion( typeof( Boolean ), typeof( BitmapImage ) )]
	public class BoolToImageConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			BitmapImage[] ImageOptions = (BitmapImage[])parameter;
			if ( (bool)value )
			{
				return ImageOptions[0];
			}
			else
			{
				return ImageOptions[1];
			}
		}

		/// Converts back to the source type from the target type
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;
		}
	}

	#endregion

}
