//=============================================================================
//	TestWindow.xaml.cs: Content browser window object for testing
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
using UnrealEd;


namespace TestBench
{
	using ContentBrowser;
	using System.ComponentModel;


	/// <summary>
	/// Interaction logic for Window1.xaml
	/// </summary>
	public partial class TestWindow : Window
	{

		private MainControl mainControl;
        public TestWindow()
		{
			InitializeComponent();

			mainControl = new MainControl();
			this.Content = mainControl;

			// Init editor backend singleton
			TestEditorBackend EditorBackend = new TestEditorBackend();
			UnrealEd.Backend.InitBackend( EditorBackend );

			// Init content browser backend
			TestBackend Backend = new TestBackend( mainControl );
			mainControl.Initialize( Backend );




			// We don't have a main application loop in our test app so we'll wire up to the
			// WPF rendering callback to simulate a frame "Tick".  In the Editor, this is
			// replaced by functionality in the CLR interop code.
			System.Windows.Media.CompositionTarget.Rendering += new EventHandler(CompositionTarget_Rendering);
		}


		DateTime LastRenderTime = DateTime.UtcNow;

		/// Called every time WPF renders the desktop
		void  CompositionTarget_Rendering(object sender, EventArgs e)
		{
			// Update no more than 60Hz
            TimeSpan TimeSinceLastRender = DateTime.UtcNow - LastRenderTime;
			if( TimeSinceLastRender.TotalSeconds > 1.0f / 60.0f )
			{
				// Update the asset canvas
				mainControl.AssetView.AssetCanvas.UpdateAssetCanvas();
			}
		}


	}


	/// <summary>
	/// A fake IEditorBackendInterface interface for testing purposes
	/// </summary>
	class TestEditorBackend : UnrealEd.IEditorBackendInterface
	{
		public void LogWarningMessage( string Text )
		{
			System.Diagnostics.Debugger.Log( 0, "", Text );
		}
	}
		


	/// <summary>
	/// A fake IContentBrowserBackendInterface interface for testing purposes
	/// </summary>
	class TestBackend : ContentBrowser.IContentBrowserBackendInterface
	{

		ContentBrowser.MainControl mMainControl;
		List<String> mTagsCatalog = new List<String>();

		public TestBackend(ContentBrowser.MainControl InMainControl)
		{
			mMainControl = InMainControl;

			// Create some fake tags
			mTagsCatalog.Add( "FakeTag" );
			mTagsCatalog.Add( "Building" );
			mTagsCatalog.Add( "caballine" );
			mTagsCatalog.Add( "cabas" );
			mTagsCatalog.Add( "cable" );
			mTagsCatalog.Add( "caboched" );
			mTagsCatalog.Add( "cabochon" );
			mTagsCatalog.Add( "caboose" );
			mTagsCatalog.Add( "cabotage" );
			mTagsCatalog.Add( "cabre" );
			mTagsCatalog.Add( "cabrie" );
			mTagsCatalog.Add( "cabriole" );
			mTagsCatalog.Add( "cabreallylongwordfortestingthewrappingoftheautocompletepopuplist" );
			mTagsCatalog.Add( "cabriolet" );
			mTagsCatalog.Add( "cacaesthesia" );
			mTagsCatalog.Add( "cachaemic" );
			mTagsCatalog.Add( "cachalot" );
			mTagsCatalog.Add( "cachepot" );
			mTagsCatalog.Add( "Character" );
			mTagsCatalog.Add( "COG" );
			mTagsCatalog.Add( "Column" );
			mTagsCatalog.Add( "Concrete" );
			mTagsCatalog.Add( "Corner" );
			mTagsCatalog.Add( "Damaged" );
			mTagsCatalog.Add( "Decal" );
			mTagsCatalog.Add( "Decoration" );
			mTagsCatalog.Add( "Destroyed" );
			mTagsCatalog.Add( "Electronic" );
			mTagsCatalog.Add( "Explosion" );
			mTagsCatalog.Add( "Exterior" );
			mTagsCatalog.Add( "Facade" );
			mTagsCatalog.Add( "Floor" );
			mTagsCatalog.Add( "Foliage" );
			mTagsCatalog.Add( "Font" );
			mTagsCatalog.Add( "Furniture" );
			mTagsCatalog.Add( "Glass" );
			mTagsCatalog.Add( "Gore" );
			mTagsCatalog.Add( "Impact" );
			mTagsCatalog.Add( "Interior" );
			mTagsCatalog.Add( "Internet" );
			mTagsCatalog.Add( "Locust" );
			mTagsCatalog.Add( "Mechanical" );
			mTagsCatalog.Add( "Metal" );
			mTagsCatalog.Add( "Mountain" );
			mTagsCatalog.Add( "MuzzleFlash" );
			mTagsCatalog.Add( "Neutral" );
			mTagsCatalog.Add( "Organic" );
			mTagsCatalog.Add( "Railing" );
			mTagsCatalog.Add( "Riftworm" );
			mTagsCatalog.Add( "Road" );
			mTagsCatalog.Add( "Rock" );
			mTagsCatalog.Add( "Sky" );
			mTagsCatalog.Add( "Snow" );
			mTagsCatalog.Add( "Trash" );
			mTagsCatalog.Add( "Tree" );
			mTagsCatalog.Add( "Urban" );
			mTagsCatalog.Add( "Vehicle" );
			mTagsCatalog.Add( "Wall" );
			mTagsCatalog.Add( "Weapon" );
			mTagsCatalog.Add( "Wheel" );
			mTagsCatalog.Add( "Wood" );
			//for ( int i = 0; i < 5000; i++ )
			//{
			//    mTagsCatalog.Add( String.Format("Wood {0:00000}", i) );
			//}


			mMainControl.MySources.SetRootDirectoryPath("c:/ContentBrowser/");
		}

		public bool AddAssetsToCollection( ICollection<string> InAssetFullNames, Collection InCollection, EBrowserCollectionType InType )
		{
			return true;
		}

		public bool AddTagToAssets( ICollection<AssetItem> InAssets, string TagToAdd )
		{
			return true;
		}

		public bool AddTagsToAsset( string AssetName, ICollection<string> TagsToAdd )
		{
			return true;
		}

		public void ApplyAssetSelectionToViewport()
		{
			
		}


		public NameSet GetAssetTypeNames()
		{
			return new NameSet();
		}


		public void CanExecuteMenuCommand( object Sender, CanExecuteRoutedEventArgs EvtArgs )
		{
			RoutedCommand Command = EvtArgs.Command as RoutedCommand;
			if ( Command != null )
			{
				if ( Command == PackageCommands.FullyLoadPackage
				||	Command == PackageCommands.UnloadPackage
				||	Command.OwnerType == typeof(SourceControlCommands) )
				{
					EvtArgs.CanExecute = true;
				}
			}
		}

		public bool CopyTag( String InCurrentTagName, String InNewTagName, bool bInMove )
		{
			return true;
		}

		public bool CreateCollection( string InCollectionName, EBrowserCollectionType InType )
		{
			return true;
		}

		public bool CreateTag( string InTag )
		{
			return true;
		}

		public bool DestroyCollection( string InCollectionName, EBrowserCollectionType InType )
		{
			return true;
		}

		public bool DestroyTag( string InTag )
		{
			return true;
		}

		public bool CopyCollection( String InCurrentCollectionName, EBrowserCollectionType InCurrentType, String InNewCollectionName, EBrowserCollectionType InNewType, bool bInMove )
		{
			return true;
		}

		public void ExecuteMenuCommand( object Sender, ExecutedRoutedEventArgs EvtArgs )
		{
			
		}

		public void ExpandDefaultPackages()
		{
			
		}

		public bool FullyLoadPackage( string PackageName )
		{
			return true;
		}


		public Color GetAssetVisualBorderColorForObjectTypeName( String InObjectTypeName )
		{
			return Color.FromArgb( 200, 255, 255, 255 );
		}


		public List<String> GenerateCustomLabelsForAsset( AssetItem InAssetItem )
		{
			return new List<String> { "", "", "", "" };
		}


		public List<String> GenerateCustomDataColumnsForAsset( AssetItem InAssetItem )
		{
			return new List<String> { "", "", "", "", "", "", "", "" };
		}


		public DateTime GenerateDateAddedForAsset( AssetItem InAssetItem )
		{
			return DateTime.Today;
		}


		public void ClearCachedThumbnailForAsset( String AssetFullName )
		{
			
		}

		public TagUtils.EngineTagDefs GetTagDefs()
		{
			return new TagUtils.EngineTagDefs(1024, '[', ']');
		}

		public bool IsAssetAnyOfBrowsableTypes( AssetItem ItemToCheck, ICollection<string> BrowsableTypeNames )
		{
			return true;
		}

		public bool IsAssetAnyOfBrowsableTypes( string AssetType, bool IsArchetype, ICollection<string> BrowsableTypeNames )
		{
			return true;
		}

		public bool IsGameAssetDatabaseReadonly()
		{
			return true;
		}

		public void LoadContentBrowserUIState( ContentBrowserUIState LoadMe )
		{
			LoadMe.SetToDefault();
			LoadMe.AssetListViewHeight = 350;
			LoadMe.AssetListViewWidth = 350;
			LoadMe.AssetViewLayoutAsDouble = (double)AssetView.LayoutMode.SplitHorizontal;
		}

		/**
		 *  Called when the user opts to view the properties of asset items via hot-key; only displays properties if all of
		 *  the selected assets do not have a specific editor for setting their attributes
		 */
		public void LoadAndDisplayPropertiesForSelectedAssets()
		{
		}
		
		public void OnAssetSelectionChanged( object Sender, AssetView.AssetSelectionChangedEventArgs Args )
		{
			
		}

		/** Called when the user opts to "activate" an asset item (usually by double clicking on it) */
		public void LoadAndActivateSelectedAssets()
		{
		}
		
		public void QueryContextMenuItems( out List<object> OutMenuItems )
		{
			OutMenuItems = new List<object>();
		}

		
		public List< String > QueryCollectionsForAsset( String InFullName, EBrowserCollectionType InType )
		{
			return new List<String>();
		}


		public bool RemoveAssetsFromCollection( ICollection<AssetItem> InAssetItems, Collection InCollection, EBrowserCollectionType InType )
		{
			return true;
		}

		public bool RemoveTagFromAssets( ICollection<AssetItem> AssetName, string TagToRemove )
		{
			return true;
		}

		public bool RemoveTagsFromAsset( string AssetName, ICollection<string> TagToRemove )
		{
			return true;
		}

		public void SaveContentBrowserUIState( ContentBrowserUIState SaveMe )
		{
			
		}

		public void UpdateAssetStatus( AssetItem Asset, AssetStatusUpdateFlags UpdateFlags )
		{
			
		}

		public void UpdateAssetsInView()
		{
			// If we're in the designer tool, add some fake data to the asset canvas
			if ( mMainControl.MyAssets.Count == 0  )
			{
				int FakeAssetCount = 18550;
				for ( int FakeAssetIndex = 0; FakeAssetIndex < FakeAssetCount; ++FakeAssetIndex )
				{
					AssetItem NewFakeAsset =
						new AssetItem(
							mMainControl,
							"FakeAsset" + FakeAssetIndex.ToString(),
							"FakeAssetPath" + FakeAssetIndex.ToString() );
					NewFakeAsset.AssetType = "Texture2D";

					NewFakeAsset.IsVerified = true;
					if( FakeAssetIndex % 3 != 0 )
					{
						if( FakeAssetIndex % 2 != 0 )
						{
							NewFakeAsset.LoadedStatus = AssetItem.LoadedStatusType.LoadedAndModified;
						}
						else
						{
							NewFakeAsset.LoadedStatus = AssetItem.LoadedStatusType.Loaded;
						}
					}
					else
					{
						NewFakeAsset.LoadedStatus = AssetItem.LoadedStatusType.NotLoaded;
						if( FakeAssetIndex % 9 != 0 )
						{
							NewFakeAsset.IsVerified = false;
						}
					}

					// Assign some tags
					if ( FakeAssetIndex % 3 != 0 )
					{
						List<String> NewTagList = new List<String>();
						NewTagList.Add( mTagsCatalog[0] );
						NewTagList.Add( mTagsCatalog[5 % mTagsCatalog.Count] );
						NewTagList.Add( mTagsCatalog[10 % mTagsCatalog.Count] );
						NewTagList.Add( mTagsCatalog[20 % mTagsCatalog.Count] );
						NewFakeAsset.Tags = NewTagList ;
					}
					else if ( FakeAssetIndex % 7 != 0 )
					{
						List<String> NewTagList = new List<String>();
						NewTagList.Add( mTagsCatalog[0] );
						NewTagList.Add( mTagsCatalog[5 % mTagsCatalog.Count] );
						NewTagList.Add( mTagsCatalog[10 % mTagsCatalog.Count] );
						NewTagList.Add( mTagsCatalog[20 % mTagsCatalog.Count] );
						NewTagList.Add( mTagsCatalog[40 % mTagsCatalog.Count] );
						NewFakeAsset.Tags = NewTagList;
					}
					else if ( FakeAssetIndex % 2 != 0 )
					{
						List<String> NewTagList = new List<String>();
						NewTagList.Add( mTagsCatalog[5 % mTagsCatalog.Count] );
						NewFakeAsset.Tags = NewTagList;
					}
					else if ( FakeAssetIndex == 0 )
					{
						// Add 50 tags for test
						List<String> NewTagList = new List<String>();
						for ( int CurTag = 0; CurTag < 20 && CurTag < mTagsCatalog.Count; CurTag++ )
						{
							NewTagList.Add( mTagsCatalog[CurTag] );
						}
						NewFakeAsset.Tags = NewTagList;
					}
					else
					{
						List<String> NewTagList = new List<String>();
						NewFakeAsset.Tags = NewTagList;
					}

					mMainControl.MyAssets.Add( NewFakeAsset );
				}
			}

		}

		public void UpdatePackagesTreeUI( ContentBrowser.ObjectContainerNode Pkg ) {}

		public void UpdatePackagesTreeUI(){}

		public void UpdatePackagesTree(bool UseFlatList)
		{
			UpdateSourcesList(UseFlatList);
		}

		public void UpdateSourcesList( bool ShouldUpdateSCC )
		{
			SourcesPanelModel Sources = mMainControl.MySources;

			Sources.RootDirectory = "..\\";

			bool UseFlatView = this.mMainControl.MySourcesPanel.UsingFlatList;

			// Create some fake collection items
			Sources.SharedCollections.Add( new Collection( "Fauxllection 0", true ) );
			Sources.SharedCollections.Add( new Collection( "Cadillacs and", true ) );
			Sources.SharedCollections.Add( new Collection( "Dinosaurs", false ) );
			Sources.SharedCollections.Add( new Collection( "Dino DNA", false ) );
			Sources.SharedCollections.Add( new Collection( "Fauxllection 9", true ) );

			Sources.PrivateCollections.Add( new Collection( "Cedar 23", true ) );
			Sources.PrivateCollections.Add( new Collection( "Oak Trees", true ) );
			Sources.PrivateCollections.Add( new Collection( "Maple Leaves", true ) );
			Sources.PrivateCollections.Add( new Collection( "Mahogany", true ) );
			Sources.PrivateCollections.Add( new Collection( "B111111rch", true ) );

			// Create some fake package items
			Sources.AddPackage(@"..\..\Engine\StatusTest\Default.upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\StatusTest\NotLoaded.upk", UseFlatView).Status = Package.PackageStatus.NotLoaded;
			Sources.AddPackage(@"..\..\Engine\StatusTest\PartiallyLoaded.upk", UseFlatView).Status = Package.PackageStatus.PartiallyLoaded;
			Sources.AddPackage(@"..\..\Engine\StatusTest\FullyLoaded.upk", UseFlatView).Status = Package.PackageStatus.FullyLoaded;

			{
				Package Pkg = Sources.AddPackage(@"..\..\Engine\StatusTest\Default_WithGroups.upk", UseFlatView);
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group0" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group1" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group2" ) );
			}

			{
				Package Pkg = Sources.AddPackage(@"..\..\Engine\StatusTest\NotLoaded_WithGroups(impossible).upk", UseFlatView);
				Pkg.Status = Package.PackageStatus.NotLoaded;
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group0" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group1" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group2" ) );
			}

			{
				Package Pkg = Sources.AddPackage(@"..\..\Engine\StatusTest\PartiallyLoaded_WithGroups.upk", UseFlatView);
				Pkg.Status = Package.PackageStatus.NotLoaded;
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group0" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group1" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group2" ) );
			}

			{
				Package Pkg = Sources.AddPackage(@"..\..\Engine\StatusTest\FullyLoaded_WithGroups.upk", UseFlatView);
				Pkg.Status = Package.PackageStatus.FullyLoaded;
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group0" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group1" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Group2" ) );
			}

			{
				Package Pkg = Sources.AddPackage(@"..\..\Engine\StatusTest\FullyLoaded_WithGroups_2.upk", UseFlatView);
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Status_Set" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "after" ) );
				Sources.AddGroup( Pkg, new GroupPackage( Pkg, "Groups_added" ) );
				Pkg.Status = Package.PackageStatus.FullyLoaded;
			}


			{
				Package tmpPackage = Sources.AddPackage(@"..\..\Engine\StatusTest\FullyLoadedWriteable.upk", UseFlatView);
				tmpPackage.Status = Package.PackageStatus.FullyLoaded;
				tmpPackage.NodeIcon = ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut;
			}

			{
				Package tmpPackage = Sources.AddPackage(@"..\..\Engine\StatusTest\PartialLoadedWriteable.upk", UseFlatView);
				tmpPackage.Status = Package.PackageStatus.PartiallyLoaded;
				tmpPackage.NodeIcon = ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut;
			}

			{
				Package tmpPackage = Sources.AddPackage(@"..\..\Engine\StatusTest\FullyLoadedModified.upk", UseFlatView);
				tmpPackage.Status = Package.PackageStatus.FullyLoaded;
				tmpPackage.NodeIcon = ObjectContainerNode.TreeNodeIconType.ICON_CheckedOut;
				tmpPackage.IsModified = true;
			}

			Sources.AddPackage(@"..\..\Engine\AddExistingTest\AddedTwice(IsGroup).upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\AddExistingTest\Dummy1.upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\AddExistingTest\Dummy2.upk", UseFlatView);

			{
				Package tmpPackage = Sources.AddPackage(@"..\..\Engine\AddExistingTest\AddedTwice(IsGroup).upk", UseFlatView);
				tmpPackage.Status = ObjectContainerNode.PackageStatus.FullyLoaded;
				tmpPackage.NodeIcon = ObjectContainerNode.TreeNodeIconType.ICON_Group;
			}

			Sources.AddPackage(@"..\..\Engine\Bar\Package3.upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\Bar\Package4.upk", UseFlatView).Status = Package.PackageStatus.PartiallyLoaded;

			Sources.AddPackage(@"..\..\Engine\Baz\Package8.upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\Baz\Package9.upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\Baz\Package10.upk", UseFlatView);
			Sources.AddPackage(@"..\..\Engine\Baz\Package11.upk", UseFlatView);


			Sources.AddPackage(@"..\..\ExampleGame\Foo\Package1.upk", UseFlatView).Status = Package.PackageStatus.FullyLoaded;
			Sources.AddPackage(@"..\..\ExampleGame\Foo\Package2.upk", UseFlatView).Status = Package.PackageStatus.FullyLoaded;

			Sources.AddPackage(@"..\..\ExampleGame\Bar\Package3.upk", UseFlatView);
			Sources.AddPackage(@"..\..\ExampleGame\Bar\Package4.upk", UseFlatView);

			Sources.AddPackage(@"..\..\ExampleGame\Baz\Package8.upk", UseFlatView);
			Sources.AddPackage(@"..\..\ExampleGame\Baz\Package9.upk", UseFlatView);
			Sources.AddPackage(@"..\..\ExampleGame\Baz\Package10.upk", UseFlatView);
			Sources.AddPackage(@"..\..\ExampleGame\Baz\Package11.upk", UseFlatView);

			//for (int FolderIndex = 0; FolderIndex < 200; ++FolderIndex )
			//{
				for (int PkgIndex = 0; PkgIndex < 3000; ++PkgIndex )
				{
					Package Pkg = Sources.AddPackage(String.Format("..\\..\\ExampleGame\\Massive_{0:0000}.upk", PkgIndex), UseFlatView);

					for (int GrpIndex = 0; GrpIndex < 10; ++GrpIndex)
					{
						Sources.AddGroup( Pkg, new GroupPackage( Pkg, String.Format("Group_{0}", GrpIndex) ) );
					}
				}
			//}
			
		}

		public void UpdateTagsCatalogue()
		{			
			mMainControl.SetTagsCatalog( mTagsCatalog );
		}

		public void PopulateObjectFactoryContextMenu( ContextMenu out_MenuItems )
		{

		}


		#region DragDrop Methods


		/**
		 * Begin a drag-n-drop operation.
		 *
		 * @param	SelectedAssetPaths	string containing fully qualified pathnames for the assets which will be part
		 *			of the d&d operation, delimited by the pipe character
		 */
		public void BeginDragDrop( String SelectedAssetPaths )
		{
		}

        /**
         * Begin a drag-n-drop operation for import.  Called when a user drags files from windows to either the asset canvas or the package tree
         *
         * @param	A list of filenames the user dropped
         *
         */
        public void BeginDragDropForImport( List<String> DroppedFiles )
		{
		}
		
		#endregion


		public string MarshalAssetItems( ICollection<AssetItem> InAssetItems )
		{
			return "";
		}

		public List<String> UnmarshalAssetFullNames( string MarshaledAssetNames )
		{
			return new List<String>();
		}

		public List<string> GetAllAssetTypeNames()
		{
			return new List<String> { "StaticMesh", "Texture2D", "AnimTree" };
		}

		public void CanExecuteAssetCommand( object Sender, CanExecuteRoutedEventArgs EvtArgs )
		{

		}

		public void ExecuteAssetCommand( object Sender, ExecutedRoutedEventArgs EvtArgs )
		{

		}

		/**
		 * Makes sure the objects for the currently selected assets have been loaded
		 */
		public void LoadSelectedObjectsIfNeeded() { }

		/** Syncs the currently selected objects in the asset view with the engine's global selection set */
		public void SyncSelectedObjectsWithGlobalSelectionSet() { }

		/** Returns true if the specified asset uses a stock thumbnail resource */
		public bool AssetUsesSharedThumbnail( AssetItem Asset )
		{
			return false;
		}

		/**
		 * Attempts to generate a thumbnail for the specified object
		 * 
		 * @param	FullyQualifiedPath Fully qualified path to the object
		 * @param	CheckSharedThumbnailAssets	True if we should even check to see if assets that use a stock thumbnail resource exist in the package file
		 * @param	OutFailedToLoadThumbnail	True if we tried to load the asset's thumbnail from disk and couldn't find it.
		 * 
		 * @return	Returns the BitmapSource for the generated thumbnail, or null if a thumbnail is not available
		 */
		public BitmapSource GenerateThumbnailForAsset( AssetItem Asset, out bool OutFailedToLoadThumbnail )
		{
			OutFailedToLoadThumbnail = false;
			return null;
		}

		/**
		 * Attempts to generate a *preview* thumbnail for the specified object
		 * 
		 * @param	FullyQualifiedPath Fully qualified path to the object
		 * @param	PreferredSize The preferred resolution of the thumbnail
		 * @param	IsAnimating True if the thumbnail will be updated frequently
		 * @param	ExistingThumbnail The current preview thumbnail for the asset, if any
		 * 
		 * @return	Returns the BitmapSource for the generated thumbnail, or null if a thumbnail is not available
		 */
		public BitmapSource GeneratePreviewThumbnailForAsset( String FullyQualifiedPath, int PreferredSize, bool IsAnimating, BitmapSource ExistingThumbnail )
		{
			return null;
		}


		/** Locally tags the specified asset as a "ghost" so that it will no longer appear in the editor */
		public void LocallyTagAssetAsGhost( AssetItem Asset )
		{
		}


		/** Locally removes the "unverified" tag from the specified asset */
		public void LocallyRemoveUnverifiedTagFromAsset( AssetItem Asset )
		{
		}

		#region Utility methods that really belong in another interface

		public bool IsMapPackageAsset( String AssetPathName )
		{
			return false;
		}

		public bool IsAssetValidForLoading( String AssetPathName )
		{
			return true;
		}

		public bool IsAssetValidForPlacing( String AssetPathName )
		{
			return true;
		}


		public bool IsAssetValidForTagging( string AssetFullName )
		{
			throw new NotImplementedException();
		}


		#endregion

		public bool IsUserTagAdmin()
		{
			return true;
		}

		public bool IsUserCollectionsAdmin()
		{
			return true;
		}


		public bool PreviewLoadedAsset( string ObjectFullName )
		{
			return true;
		}


		public List<string> GetObjectTypeFilterList()
		{
			return new List<String>(0);
		}

		public List<string> LoadTypeFilterFavorites()
		{
			return new List<String>( 0 );
		}

		public void SaveTypeFilterFavorites( List<string> InFavorites )
		{
			
		}


		public bool ShouldShowConfirmationPrompt( ConfirmationPromptType InType )
		{
			return true;
		}


		public void DisableConfirmationPrompt( ConfirmationPromptType InType )
		{
		}

		public void QueryAssetViewContextMenuItems( out List<object> OutMenuItems )
		{
			OutMenuItems = new List<object>();
		}

		public void PopulatePackageListMenuItems( ItemCollection OutPackageListMenuItems, CommandBindingCollection OutCommandBindings, Type InTypeId, ExecutedRoutedEventHandler InEventHandler, CanExecuteRoutedEventHandler InCanExecuteEventHandler )
		{
			
		}

		public bool QuarantineAssets(ICollection<AssetItem> AssetsToQuarantine)
		{
			return true;
		}

		public bool LiftQuarantine(ICollection<AssetItem> AssetsToRelease)
		{
			return true;
		}


		public bool RemoveAssetsFromCollection( ICollection<string> InAssetFullNames, Collection InCollection, EBrowserCollectionType InType )
		{
			return true;
		}

		public int CalculateMemoryUsageForAsset( AssetItem InAssetItem )
		{
			return 0;
		}

		public List<string> GetBrowsableTypeNameList( List<string> ClassNameList )
		{
			return new List<String>();
		}

		public void TagInUseObjects()
		{
		}

		public bool IsObjectInUse( AssetItem Asset )
		{
			return true;
		}

	}
}
