//=============================================================================
//	AssetView.xaml.cs: Content browser asset view implementation
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
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
using System.Collections.ObjectModel;
using System.Globalization;
using System.Collections;
using CustomControls;
using UnrealEd;
using System.Windows.Automation.Peers;


namespace ContentBrowser
{


	/// Declarations for commands specific to AssetView
	public static class AssetViewCommands
	{
		private static RoutedUICommand mRefresh = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_RefreshMenu_Refresh" ), "Refresh", typeof( AssetViewCommands ),
			new InputGestureCollection() { new KeyGesture( Key.F5 ), new KeyGesture( Key.R, ModifierKeys.Control ) }
			);

		private static RoutedUICommand mFullRefresh = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_RefreshMenu_FullRefresh" ), "FullRefresh", typeof( AssetViewCommands ),
			new InputGestureCollection() { new KeyGesture(Key.F5, ModifierKeys.Control), new KeyGesture(Key.R, ModifierKeys.Control | ModifierKeys.Shift) }
			);

		private static RoutedUICommand mSelectAll = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_AssetView_SelectAll" ), "SelectAll", typeof( AssetViewCommands ),
			new InputGestureCollection() { new KeyGesture(Key.A, ModifierKeys.Control) }
			);

		private static RoutedUICommand mSetSourceToAllAssets = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_AssetView_SetSourceToAllAssets" ), "SetSourceToAllAssets", typeof( AssetViewCommands ),
			new InputGestureCollection() { new KeyGesture( Key.A, ModifierKeys.Control | ModifierKeys.Shift) }
			);

		private static RoutedUICommand mToggleQuarantine = new RoutedUICommand(
			UnrealEd.Utils.Localize("ContentBrowser_AssetView_ToggleQuarantine"), "ToggleQuarantine", typeof(AssetViewCommands),
			new InputGestureCollection() { new KeyGesture(Key.Q, ModifierKeys.Control) }
			);


		/// Command that Refresh the asset view.
		public static RoutedUICommand Refresh { get { return mRefresh; } }
		
		/// Command that Refreshes the asset view by re-populating it.
  		public static RoutedUICommand FullRefresh { get { return mFullRefresh; } }

		/// Command that selects all assets
		public static RoutedUICommand SelectAll { get { return mSelectAll; } }

		/// Command that sets the source to AllAssets
		public static RoutedUICommand SetSourceToAllAssets { get { return mSetSourceToAllAssets; } }

		/// Place assets under quarantine or remove them from quarantine.
		public static RoutedUICommand ToggleQuarantine { get { return mToggleQuarantine; } }


	}


	/// Our extended list view that allows us to bulk-select items
	public class UnrealListView : System.Windows.Controls.ListView
	{
		#region Batch Selection

		/// <summary>
		/// Begin a batch selection operation (when calling more than 1 selection-related method).  Safe to nest calls to this method
		/// </summary>
		public void BeginBatchSelection()
		{
			BatchSelectionSemaphore++;
		}

		/// <summary>
		/// End a batch selection operation.  If all batch selection operations have been completed (due to nested calls to Begin),
		/// triggers selection-changed events.
		/// </summary>
		/// <param name="bFireSelectionChangedEvent">specify false to prevent selection change notifications from being fired</param>
		/// <returns>the remaining number of nested batch selection operations</returns>
		public int EndBatchSelection( bool bFireSelectionChangedEvent )
		{
			if ( --BatchSelectionSemaphore == 0 && bFireSelectionChangedEvent )
			{
				SelectionChangedEventArgs BatchEventArgs = new SelectionChangedEventArgs(
					System.Windows.Controls.Primitives.Selector.SelectionChangedEvent,
					RemovedDuringBatch, AddedDuringBatch
					);

				BatchEventArgs.Source = this;
				AddedDuringBatch.Clear();
				RemovedDuringBatch.Clear();

				OnSelectionChanged(BatchEventArgs);
			}

			return BatchSelectionSemaphore;
		}

		/// <summary>
		/// Utility method for testing whether a batch selection is active.
		/// </summary>
		/// <returns></returns>
		public bool IsBatchSelecting()
		{
			return BatchSelectionSemaphore > 0;
		}

		/// <summary>
		/// Tracks the current nesting level for batch selection operations.
		/// </summary>
		protected int BatchSelectionSemaphore = 0;

		/// <summary>
		/// Tracks the AssetItems that were added and removed from the selection set during batch selections.
		/// </summary>
		protected List<object> AddedDuringBatch = new List<object>();
		protected List<object> RemovedDuringBatch = new List<object>();

		#endregion

		public bool IsNotifyingSelectionChange
		{
			get; set;
		}

		/// Exposes the protected SetSelectedItems() method of WPF's ListView so that we can
		/// select more than one item at a time with reasonable performance
		public bool ForceSetSelectedItems( IEnumerable selectedItems )
		{
//@selection - this will result in calling OnSelectionChanged(); if we are in batch selection the event won't be passed on
			return SetSelectedItems( selectedItems );
		}

		/// <summary>
		/// Duplicate the list of selected items; return a copy of the list.
		/// </summary>
		/// <returns>A copy of the selection list.</returns>
		public List<AssetItem> CloneSelection()
		{
			List<AssetItem> SelectedAssetItems = new List<AssetItem>( this.SelectedItems.Count );
			foreach ( AssetItem SelectedAsset in this.SelectedItems )
			{
				SelectedAssetItems.Add( SelectedAsset );
			}
			return SelectedAssetItems;
		}

        /// <summary> 
        /// A virtual function that is called when the selection is changed. Default behavior
        /// is to raise a SelectionChangedEvent 
        /// </summary>
        /// <param name="e">The inputs for this event. Can be raised (default behavior) or processed
        ///   in some other way.</param>
        protected override void OnSelectionChanged(SelectionChangedEventArgs e) 
        {
			if ( IsBatchSelecting() )
			{
				foreach ( object obj in e.AddedItems )
				{
					AssetItem item = obj as AssetItem;
					if ( item != null )
					{
						AddedDuringBatch.Add(item);
					}
				}

				foreach ( object obj in e.RemovedItems )
				{
					AssetItem item = obj as AssetItem;
					if ( item != null )
					{
						RemovedDuringBatch.Add(item);
					}
				}

				e.Handled = true;
			}
			else
			{
//@selection ronp this call eventually results in calling AssetListView_SelectionChanged
				bool bOldValue = IsNotifyingSelectionChange;
				IsNotifyingSelectionChange = true;
				base.OnSelectionChanged(e);
				IsNotifyingSelectionChange = bOldValue;
			}
		}

		/// Scroll the ListView to the very top.
		/// (Warning: relies on the order of elements in the template)
		public void ScrollToTop()
		{
			if ( VisualTreeHelper.GetChildrenCount(this) > 0 )
			{
				// Grab the ScrollViewer from within our ListView template and set its scroll offset to 0.
				( ( (ScrollViewer)( (Border)VisualTreeHelper.GetChild( this, 0 ) ).Child ) ).ScrollToVerticalOffset( 0 );
			}
		}

	}


	#region Value Converters for the asset view

	namespace AssetViewValueConverters
	{
		/// <summary>
		/// Converts an asset's "Loaded Status" to a font weight for displaying in a list
		/// </summary>
		[ValueConversion( typeof( AssetItem.LoadedStatusType ), typeof( FontWeight ) )]
		public class AssetLoadedStatusToFontWeight
			: IValueConverter
		{
			/// Converts from the source type to the target type
			public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
			{
				AssetItem.LoadedStatusType LoadedStatus = (AssetItem.LoadedStatusType)value;
				switch( LoadedStatus )
				{
					default:
					case AssetItem.LoadedStatusType.NotLoaded:
						{
							// Use normal text for assets that aren't loaded
							return FontWeights.Normal;
						}

					case AssetItem.LoadedStatusType.Loaded:
					case AssetItem.LoadedStatusType.LoadedAndModified:
						{
							// Use bold text for loaded assets!
							return FontWeights.Bold;
						}
				}
			}

			/// Converts back to the source type from the target type
			public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
			{
				return null;
			}
		}

	}

	#endregion


	/// Describes how sorting should be performed in the asset view. Keeps track of all UI elements that are used for changing the sorting options.
	public class SortingOptions
	{
		/// Ways in which a column can be sorted
		/// (Currently does not support the concept of "cannot sort by this column")
		public enum Direction : int
		{
			/// Sort in ascending order
			Ascending = 0,
			/// Sort in descending order
			Descending = 1,
			/// Do not sort
			NoSort = 2,
			MaxValue,
		}

		/// Represents a single sort parameter. E.g. "Sort By Name".
		public class Option
		{
			/// Corresponding radio button in the Sort options menu (may be null).
			public RadioButton SortRadioButton { get; set; }
			/// Corresponding column header in the gridview.
			public SortableColumnHeader SortColumnHeader { get; set; }
			/// Comparer to be used during an ascending sort by this parameter.
			public IComparer AscendingComparer { get; set; }
			/// Comparer to be used during a descending sort by this parameter.
			public IComparer DescendingComparer { get; set; }
			/// Description of this sort parameter.
			public String Description { get; set; }
		}

		/// Given the SortableColumnHeader 'InColumnHeader', find its index in the internal list of sort options.
		/// (Note, this does not necessarily correspond to the order in which the columns appear on the screen.)
		public int IndexFromColumnHeader( SortableColumnHeader InColumnHeader )
		{
			return mSortingOptions.FindIndex( MatchCandidate => MatchCandidate.SortColumnHeader == InColumnHeader );
		}

		/// Get the index (in the internal list) of the RadioButton which is currently checked. May return -1 if none of the radio buttons are checked.
		public int IndexOfCheckedSortRadio()
		{
			return mSortingOptions.FindIndex( MatchCandidate => ( MatchCandidate.SortRadioButton != null && (MatchCandidate.SortRadioButton.IsChecked ?? false == true ) ) );
		}

		/// Get the SortableColumnHeader which corresponds to the current sort parameter. E.g. the SortableColumnHeader for "Sort by Name".
		public SortableColumnHeader ActiveColumnHeader { get { return mSortingOptions[ActiveColumnIndex].SortColumnHeader; } }
		/// Get the RadioButton which corresponds to the current sort parameter. E.g. the RadioButton for "Sort by Name".
		public RadioButton ActiveRadioButton { get { return ( ActiveColumnIndex >= 0 && ActiveColumnIndex < mSortingOptions.Count ) ? mSortingOptions[ActiveColumnIndex].SortRadioButton : null; } }
		/// Get the name of the current sort parameter. E.g. "Name" or "Date Added".
		public String ActiveSortDescription { get { return mSortingOptions[ActiveColumnIndex].Description; } }

		/// Makes none of the radio buttons checked.
		public void UncheckAllRadioButtons()
		{
			foreach ( Option SortOption in mSortingOptions )
			{
				if ( SortOption.SortRadioButton != null )
				{
					SortOption.SortRadioButton.IsChecked = false;
				}
			}
		}

		/// Constructor. Makes a default SortingOptions with no sort parameters. At least one sort parameter should be added before using any methods other than Add().
		public SortingOptions()
		{
			this.ActiveColumnIndex = 0;
			this.SortDirection = Direction.Ascending;
		}

		/// Get the comparer corresponding to the currently active search parameter and direction.
		public IComparer GetActiveComparer()
		{
			if (this.SortDirection == Direction.Ascending)
			{
				return mSortingOptions[this.ActiveColumnIndex].AscendingComparer;
			}
			else
			{
				return mSortingOptions[this.ActiveColumnIndex].DescendingComparer;
			}
		}

		private List<Option> mSortingOptions = new List<Option>();

		/// <summary>
		/// Add a new sorting option/parameter.
		/// </summary>
		/// <param name="InRadioButton">The RadioButton in the SortingOptions menu which corresponds to this parameter (may be null).</param>
		/// <param name="InColumnHeader">The SortableColumnHeader in the AssetList which corresponds to this parameter.</param>
		/// <param name="InAscendingComparer">The comparer to use when performing an ascending sort by this parameter.</param>
		/// <param name="InDescendingComparer">The comparer to use when performing an descending sort by this parameter.</param>
		/// <param name="InDescription">A string description of this sort parameter. E.g. "Name" or "Date Added".</param>
		public void Add( RadioButton InRadioButton, SortableColumnHeader InColumnHeader, IComparer InAscendingComparer, IComparer InDescendingComparer, String InDescription )
		{
			mSortingOptions.Add(
				new Option
				{
					SortRadioButton = InRadioButton,
					SortColumnHeader = InColumnHeader,
					AscendingComparer = InAscendingComparer,
					DescendingComparer = InDescendingComparer,
					Description = InDescription
				});
		}

		/// The index (in the internal list) of the parameter that is currently being used to sort.
		public int ActiveColumnIndex { get; set; }

		/// The direction in which we are currently sorting.
		public Direction SortDirection { get; set; }
	}



	/// <summary>
	/// AssetView
	/// </summary>
    public partial class AssetView : UserControl
    {
		/// Returns the asset list view control
		public UnrealListView AssetListView { get { return m_AssetListView; } }

		/// Access the asset canvas control
		public AssetCanvas AssetCanvas { get { return m_AssetCanvas; } }

		/// ViewModel that contains the UI's view of the asset data
		public AssetViewModel Model { get { return m_Model; } }
		private AssetViewModel m_Model = new AssetViewModel();

		/// Cached content browser reference
		private MainControl mMainControl;

		/// Number of milliseconds when the user has stopped typing and when the filter is applied
		private readonly static int MillisecondsFilterRefreshDelay = 250;

		/// Keep time to prevent frequent filter refreshes because they can be fairly expensive.
		private Stopwatch mFilterStopwatch = new Stopwatch();

		/// How to narrow down what the user is seeing (filter by name, tags, type)
		AssetFilter mAssetFilter;

		/// Should we refresh the collection view now?
		bool mShouldRefreshFilter = false;
		/// Should we refresh the filter tiers?
		bool mShouldRefreshFilterTiers = false;

		/// List of asset full names that we should select after the current query completes
		List<String> m_DeferredAssetFullNamesToSelect;

		/// Keeps a list of parameters by which we can sort, the current sorting settings, and the associated widgets
		private SortingOptions mSortingOptions = new SortingOptions();

		/// Is there a pending history snapshot from the filter panel
        public bool bPendingFilterHistorySnapshot = false;

        /// The description of the pending filter snapshot
        public String PendingFilterSnapshotDesc;

        /// The location where the mouse was pressed on an asset (either the asset visual or asset list item).
        /// This is used to determine how far we have dragged the mouse while panning and handling drag-drop.
        public Point mAssetMouseDownLocation = new Point();

		/// Static Constructor
		static AssetView()
		{
			//Register cut and copy (cut actually copies)
			CommandManager.RegisterClassCommandBinding( typeof( AssetView ), new CommandBinding( ApplicationCommands.Copy, CopyCommandHandler, CanCopyCommandHandler ) );
			CommandManager.RegisterClassCommandBinding( typeof( AssetView ), new CommandBinding( ApplicationCommands.Cut, CopyCommandHandler, CanCopyCommandHandler ) );
			CommandManager.RegisterClassCommandBinding( typeof( AssetView ), new CommandBinding( AssetViewCommands.SelectAll, SelectAllCommandHandler, CanSelectAllCommandHandler ) );
			CommandManager.RegisterClassCommandBinding( typeof( AssetView ), new CommandBinding( AssetViewCommands.ToggleQuarantine, OnToggleQuarantineCommand, OnCanToggleQuarantineCommand ) );
		}
		

		/// <summary>
		/// Constructor
		/// </summary>
        public AssetView()
		{
			InitializeComponent();

			// Assign the context for the asset view.  This tells the asset view that it can find
			// data within the scope of it's viewmodel object.
			DataContext = this;

			// Register handlers for user clicking on layout mode switches
			mListOnlyRadioButton.Checked += new RoutedEventHandler( mListOnlyRadioButton_Checked );
			mHorizontalSplitRadioButton.Checked += new RoutedEventHandler( mHorizontalSplitRadioButton_Checked );
			mVerticalSplitRadioButton.Checked += new RoutedEventHandler( mVerticalSplitRadioButton_Checked );
			mThumbsOnlyRadioButton.Checked += new RoutedEventHandler( mThumbsOnlyRadioButton_Checked );

			// Register handler for user resizing the asset view
			this.SizeChanged += new SizeChangedEventHandler( AssetView_SizeChanged );

			// Register handler for moving the splitter between list and thumbs view
			this.mAssetListViewSizer.SizeChanged += new SizeChangedEventHandler( mAssetListViewSizer_SizeChanged );

			// Register handler for user accepting the prompt to remove assets from a collection
			mProceedWithRemoveAssetsFromCollection.Accepted += new YesNoPrompt.PromptAcceptedHandler( mProceedWithRemoveAssetsFromCollection_Accepted );

			// Register handler for key presses
			this.PreviewKeyDown += new KeyEventHandler( AssetView_PreviewKeyDown );

			// Register for drag and drop support for importing assets
			this.AllowDrop = true;
			this.Drop += AssetView_Drop;
			this.DragOver += AssetView_DragOver;
		}

		#region Drag and Drop

		/// Position where the mouse was captured.
		Point mMouseCapturePosition;

		/// Attached to all list items. Overrides default list behavior to allow for dragging and dropping of multiple list view items.
		/// Note: drag can only be started with no modifier keys (ctrl, shift) pressed
		void ListItem_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
		{
			if ( Keyboard.Modifiers == ModifierKeys.None)
			{
				ListViewItem ListItemContainer = (ListViewItem)sender;

				mMouseCapturePosition = e.GetPosition(null);
				ListItemContainer.CaptureMouse();

				if (ListItemContainer.IsSelected)
				{
					// Must manually focus the list view because we are
					// handling this event.
					m_AssetListView.Focus();					
					e.Handled = true;
				}
			}
		}

		/// Attached to all list items. Overrides default list behavior: works with ListItem_MouseLeftButtonDown to allow drag multiple items from a list.
		void ListItem_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
		{
			ListViewItem ListItemContainer = (ListViewItem)sender;
			if (ListItemContainer == Mouse.Captured)
			{
				ListItemContainer.ReleaseMouseCapture();
				e.Handled = true;
			}

			if (Keyboard.Modifiers == ModifierKeys.None)
			{
				m_AssetListView.SelectedItems.Clear();
				ListItemContainer.IsSelected = true;
			}
			
		}

		/// Attached to all list items. Overrides default list behavior: initiated drag and drop after mouse is dragged a certain distance
		/// 
		void ListItem_MouseMove(object sender, MouseEventArgs e)
		{
			ListViewItem ListItemContainer = (ListViewItem)sender;
			if (ListItemContainer == Mouse.Captured)
			{
				Point CurMousePosition = e.GetPosition(null);
				Point Delta = new Point(Math.Abs(CurMousePosition.X - mMouseCapturePosition.X), Math.Abs(CurMousePosition.Y - mMouseCapturePosition.Y));

				if ((Delta.X > SystemParameters.MinimumHorizontalDragDistance ||
					Delta.Y > SystemParameters.MinimumVerticalDragDistance))
				{
					String SelectedAssetPaths = mMainControl.Backend.MarshalAssetItems( this.SelectedAssets );
					mMainControl.Backend.BeginDragDrop(SelectedAssetPaths);
				}
			}
		}
		#endregion


		/// Called when asset population begins.
		public void OnAssetPopulationStarted( bool IsQuickUpdate )
		{
			if ( !IsQuickUpdate )
			{
				// Reference to the CollectionView that we are sorting
				ListCollectionView MyAssetListView = (ListCollectionView)CollectionViewSource.GetDefaultView( this.Model );
				// During population, do not show any items.
				// This prevents thrashing the view.
				// Workaround: improves performance by eliminating unnecessary work done by ListCollectionView.
				MyAssetListView.Filter = ( Candidate => false );
			}
		}

		/// Called when asset population is complete.
		public void OnAssetPopulationComplete( bool IsQuickUpdate )
		{
			
		}


		/// 
		void AssetView_PreviewKeyDown( object sender, KeyEventArgs e )
		{
			// @hack: This is hardcoded because Sound Cue and Sound Wave are the only things that have ever been previewable.
			List<String> BrowserTypesSupportingPreview = new List<String>{"Sound Cues", "Sound Wave Data"};

			if ( e.Key == Key.Space && this.SelectedAssets.Count == 1 )
			{
				AssetItem AssetToPreview = this.SelectedAssets[0];
				// Check to see that we could preview this asset before we load it.
				if ( mMainControl.Backend.IsAssetAnyOfBrowsableTypes( AssetToPreview, BrowserTypesSupportingPreview ) )
				{
					mMainControl.Backend.LoadSelectedObjectsIfNeeded();
					if( mMainControl.Backend.PreviewLoadedAsset( AssetToPreview.FullName ) )
					{
						mMainControl.PlayQuickNotification( UnrealEd.Utils.Localize( "ContentBrowser_Notification_PreviewingSound" ), new FontFamily( "Webdings" ), 60 );
					}
					else
					{
						mMainControl.PlayQuickNotification( UnrealEd.Utils.Localize( "ContentBrowser_Notification_StopPreviewingSound" ), new FontFamily( "Webdings" ), 60 );
					}
					
				}

				
				e.Handled = true;
			}
		}


		/// Called when the user drops files onto this view with the mouse
		void AssetView_Drop( object sender, DragEventArgs e )
		{
			if ( e.Data.GetDataPresent(DataFormats.FileDrop, false) == true )
			{
				// Pass a list of filename strings to the main control	
				string[] strings = (string[])e.Data.GetData( DataFormats.FileDrop, false );

				List<String> Filenames = new List<String>();
				for ( int strIdx = 0; strIdx < strings.Length; ++strIdx )
					Filenames.Add( strings[strIdx] );

				// Import the files
				mMainControl.Backend.BeginDragDropForImport( Filenames );
			}
			e.Handled = true;
		}

		/// Called when the user drags something over the asset view
		/// Note: This is here to prevent the dragging of non files (I.E asset icons) onto the asset view.
		void AssetView_DragOver( object sender, DragEventArgs e )
		{
			e.Effects = DragDropEffects.None;

			// Ignore possible drag/drop requests if the user isn't dropping files. 
			if (e.Data.GetDataPresent(DataFormats.FileDrop, false) == true)
			{
				e.Effects = DragDropEffects.Copy;
			}

			e.Handled = true;
		}

		/// Scroll both views to the very top.
		public void ScrollToTop()
		{
			AssetListView.ScrollToTop();
			AssetCanvas.ScrollToTop();
		}

		void AssetView_SizeChanged( object sender, SizeChangedEventArgs e )
		{
			UpdateAssetViewSplitterMetrics();
		}

		
		#region Configurable Layout

		/// The various ways in which the user can show the list view and/or the thumbnails view
		public enum LayoutMode
		{
			ListOnly = 0,
			SplitHorizontal = 1,
			SplitVertical = 2,
			ThumbnailsOnly = 3,
		}

		/// Vertical Split position of the splitter
		private double SavedListViewWidth;
		/// Horizontal Split position of the splitter
		private double SavedListViewHeight;

		/// <summary>
		/// Finds the radio button that would be selected for the given LayoutMode
		/// </summary>
		/// <param name="InLayoutMode">Layout mode for which to find the button.</param>
		/// <returns>The RadioButton that - if selected - would switch us to the given layout mode.</returns>
		private RadioButton LayoutModeToRadioButton( LayoutMode InLayoutMode )
		{
			switch (InLayoutMode)
			{
				case LayoutMode.ListOnly:
					return mListOnlyRadioButton;
				case LayoutMode.ThumbnailsOnly:
					return mThumbsOnlyRadioButton;
				case LayoutMode.SplitHorizontal:
					return mHorizontalSplitRadioButton;
				case LayoutMode.SplitVertical:
					return mVerticalSplitRadioButton;
				default:
					return mThumbsOnlyRadioButton;
			}
		}

		/// The current layout mode
		private LayoutMode mLayoutMode = LayoutMode.ThumbnailsOnly;

		/// Called when the used clicks on the "switch to list view only" button
		void mListOnlyRadioButton_Checked( object sender, RoutedEventArgs e )
		{
			mLayoutMode = LayoutMode.ListOnly;
			UpdateAssetViewLayout();
		}

		/// Called when the used clicks on the "switch to horizontal split" button
		void mHorizontalSplitRadioButton_Checked( object sender, RoutedEventArgs e )
		{
			mLayoutMode = LayoutMode.SplitHorizontal;
			UpdateAssetViewLayout();
		}

		/// Called when the used clicks on the "switch to vertical split" button
		void mVerticalSplitRadioButton_Checked( object sender, RoutedEventArgs e )
		{
			mLayoutMode = LayoutMode.SplitVertical;
			UpdateAssetViewLayout();
		}

		/// Called when the used clicks on the "switch to thumbnails only" button
		void mThumbsOnlyRadioButton_Checked( object sender, RoutedEventArgs e )
		{
			mLayoutMode = LayoutMode.ThumbnailsOnly;
			UpdateAssetViewLayout();
		}

		/// Called when the splitter between ListView and ThumbsView is moved.
		void mAssetListViewSizer_SizeChanged( object sender, SizeChangedEventArgs e )
		{
			if (this.mLayoutMode == LayoutMode.SplitHorizontal)
			{
				this.SavedListViewHeight = mAssetListViewSizer.Height;
			}
			else if (this.mLayoutMode == LayoutMode.SplitVertical)
			{
				this.SavedListViewWidth = mAssetListViewSizer.Width;
			}
		}

		/// Update the dimensions and the mix/max dimensions of the listView sizer
		/// to prevent the user from getting into confusing layout states (e.g. dragging a
		/// splitter beyond the edge of the screen)
		void UpdateAssetViewSplitterMetrics()
		{
			switch ( this.mLayoutMode )
			{
				case LayoutMode.ListOnly:
				case LayoutMode.ThumbnailsOnly:
					mAssetListViewSizer.MaxWidth = this.ActualWidth;
					mAssetListViewSizer.MaxHeight = this.ActualHeight;
					break;
				case LayoutMode.SplitVertical:
					mAssetListViewSizer.MaxWidth = Math.Max( mAssetListViewSizer.MinWidth, this.ActualWidth * 0.85 );
					mAssetListViewSizer.MaxHeight = Double.PositiveInfinity;
					break;
				case LayoutMode.SplitHorizontal:
					mAssetListViewSizer.MaxWidth = Double.PositiveInfinity;
					mAssetListViewSizer.MaxHeight = Math.Max( mAssetListViewSizer.MinHeight, this.ActualHeight * 0.85 );
					break;

			}
		}

		/// Update the configurable layout to reflect the currently selected LayoutMode
		void UpdateAssetViewLayout()
		{

			RemoveFromLayout( m_AssetCanvas );
			RemoveFromLayout( mAssetListViewSizer );
			
			switch ( this.mLayoutMode )
			{
				case LayoutMode.ListOnly:
					{
						// Have to be added in this order because docking panels
						// have "last child fill", and we want the list view to fill the view.
						AddUniqueToLayout( m_AssetCanvas );
						AddUniqueToLayout( mAssetListViewSizer );


						DockPanel.SetDock( mAssetListViewSizer, Dock.Left );
						DockPanel.SetDock( m_AssetCanvas, Dock.Bottom );

						m_AssetCanvas.Visibility = Visibility.Collapsed;
						mAssetListViewSizer.IsCollapsed = false;
						mAssetListViewSizer.ResizingEnabled = false;
						//mAssetListViewSizer.ResizeDirection = CellSizer.Direction.AutoSize;
						
					}
					break;
				case LayoutMode.SplitVertical:
					{
						AddUniqueToLayout( mAssetListViewSizer );
						AddUniqueToLayout( m_AssetCanvas );

						DockPanel.SetDock( mAssetListViewSizer, Dock.Left );
						DockPanel.SetDock( m_AssetCanvas, Dock.Left );
						
						m_AssetCanvas.Visibility = Visibility.Visible;
						mAssetListViewSizer.IsCollapsed = false;
						mAssetListViewSizer.ResizingEnabled = true;
						mAssetListViewSizer.ResizeDirection = CellSizer.Direction.East;
						mAssetListViewSizer.Width = this.SavedListViewWidth;
						mAssetListViewSizer.ClearValue( CellSizer.HeightProperty );
					}
					break;
				case LayoutMode.SplitHorizontal:
					{
						AddUniqueToLayout( mAssetListViewSizer );
						AddUniqueToLayout( m_AssetCanvas );

						DockPanel.SetDock( mAssetListViewSizer, Dock.Top );
						DockPanel.SetDock( m_AssetCanvas, Dock.Top );

						m_AssetCanvas.Visibility = Visibility.Visible;
						mAssetListViewSizer.IsCollapsed = false;
						mAssetListViewSizer.ResizingEnabled = true;
						mAssetListViewSizer.ResizeDirection = CellSizer.Direction.South;
						mAssetListViewSizer.Height = this.SavedListViewHeight;
						mAssetListViewSizer.ClearValue( CellSizer.WidthProperty );
					}
					break;
				case LayoutMode.ThumbnailsOnly:
					{
						AddUniqueToLayout( mAssetListViewSizer );
						AddUniqueToLayout( m_AssetCanvas );

						m_AssetCanvas.Visibility = Visibility.Visible;
						mAssetListViewSizer.ResizingEnabled = false;
						mAssetListViewSizer.IsCollapsed = true;
					}
					break;
			}

			UpdateAssetViewSplitterMetrics();
		}

		/// Add a control to the AssetView dock panel.
		/// Does not add if the control is already added.
		/// (Used by the configurable layout.)
		void AddUniqueToLayout( Control AddMe )
		{
			if ( !mAssetViewDockPanel.Children.Contains(AddMe) )
			{
				mAssetViewDockPanel.Children.Add( AddMe );
			}
		}

		/// Remove a control from the AssetView dock panel; (used by the configurable layout)
		void RemoveFromLayout( Control RemoveMe )
		{
			mAssetViewDockPanel.Children.Remove( RemoveMe );
		}
		
		#endregion



		#region ApplicationCommands.Copy

		/// ApplicationCommands.Copy : CanExecture Handler
		private static void CanCopyCommandHandler( object sender, CanExecuteRoutedEventArgs e )
		{
			AssetView This = (AssetView)sender;
			e.CanExecute = ( This.SelectedCount > 0 );
		}

		/// ApplicationCommands.Copy : Execture Handler
		private static void CopyCommandHandler( object sender, ExecutedRoutedEventArgs e )
		{
			AssetView This = (AssetView)sender;
			This.CopySelectedAssets();			
		}

		public void CopySelectedAssets()
		{
			// Build a string from the selected assets' pathnames.
			// Put it in the Clipboard.
			StringBuilder ClipboardContent = new StringBuilder();

			ReadOnlyCollection<AssetItem> SelectedItems = This.SelectedAssets;

			if ( SelectedItems.Count > 0 )
			{
				AssetItem LastItem = SelectedItems[SelectedItems.Count - 1];
				foreach ( AssetItem CurItem in SelectedItems )
				{
					// NOTE: The format here is a little different than the AssetItem.FullName where we have
					// a space character separating the object type from the path.  This format is the script
					// style full name, where we wrap the path in single quotes
					ClipboardContent.AppendFormat( "{0}'{1}'", CurItem.AssetType, CurItem.FullyQualifiedPath );
					if ( CurItem != LastItem )
					{
						ClipboardContent.AppendLine();
					}
				}

				try
				{
					Clipboard.SetText( ClipboardContent.ToString() );
				}
				catch ( System.Runtime.InteropServices.COMException )
				{
					// Failed to open clipboard.  Another app may have it open right now.
					// HRESULT: 0x800401D0 (CLIPBRD_E_CANT_OPEN)
				}
			}
		}

		#endregion

		#region AssetViewCommands.SelectAll

		/// Can we execite a SelectAll?
		private static void CanSelectAllCommandHandler( object sender, CanExecuteRoutedEventArgs e )
		{
			AssetView This = (AssetView)sender;
			e.CanExecute = true;
			e.Handled = true;
		}

		/// Select all assets.
		private static void SelectAllCommandHandler( object sender, ExecutedRoutedEventArgs e )
		{
			AssetView This = (AssetView)sender;
			This.AssetListView.SelectAll();
			e.Handled = true;
		}

		#endregion



		/// <summary>
		/// Initialize the asset view
		/// </summary>
		/// <param name="InContentBrowser">Content browser that the asset view is associated with</param>
		public void Init( MainControl InContentBrowser )
		{
			mMainControl = InContentBrowser;

			// Initialize the asset view model
			Model.Init( InContentBrowser );


			// Initialize the asset canvas
			m_AssetCanvas.Init( InContentBrowser, this );

			// Set command bindings
			{
				RemoveFromCollectionCommand = new RoutedUICommand( Utils.Localize( "ContentBrowser_AssetItemContext_RemoveFromCollection" ), "RemoveFromCollectionCommand", typeof( AssetView ) );
			}


			// Find out when the selection set is changed
			AssetListView.SelectionChanged += AssetListView_SelectionChanged;

			// Register delegates
			AssetSelectionChanged += mMainControl.Backend.OnAssetSelectionChanged;

			AssetListView.PreviewMouseDoubleClick += new MouseButtonEventHandler( AssetListView_MouseDoubleClick );
			Model.CollectionChanged += new NotifyCollectionChangedEventHandler( Model_CollectionChanged );



			// -- Sorting in List View --
			{
				// Register handler for user clicking on asset list view column headers.
				// Also associate a comparer with each header, so that we can easily retrieve them when a header has been clicked.
				AssetListView_ColumnHeaderClickedHandler = new RoutedEventHandler( AssetListView_ColumnHeaderClicked );

				mSortingOptions.Add( mSort_ByName, mNameColumnHeaderContents, new AscendingAssetNameComparer(), new DescendingAssetNameComparer(), Utils.Localize( "ContentBrowser_AssetItemNameColumn" ) );
				mNameColumnHeader.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( mSort_ByType, mTypeColumnHeaderContents, new AscendingAssetTypeComparer(), new DescendingAssetTypeComparer(), Utils.Localize( "ContentBrowser_AssetItemTypeColumn" ) );
				mTypeColumnHeader.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( mSort_ByTags, mTagsColumnHeaderContents, new AscendingAssetTagsComparer(), new DescendingAssetTagsComparer(), Utils.Localize( "ContentBrowser_AssetItemTagsColumn" ) );
				mTagsColumnHeader.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( mSort_ByPath, mPathColumnHeaderContents, new AscendingAssetPathComparer(), new DescendingAssetPathComparer(), Utils.Localize( "ContentBrowser_AssetItemPathColumn" ) );				
				mPathColumnHeader.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( mSort_ByDateAdded, mDateAddedColumnHeaderContents, new AssetDateAddedComparer( true ), new AssetDateAddedComparer( false ), Utils.Localize( "ContentBrowser_AssetItemDateAddedColumn" ) );
				mDateAddedColumnHeader.Click += AssetListView_ColumnHeaderClickedHandler;

                mSortingOptions.Add( mSort_ByMemoryUsage, mMemoryUsageColumnHeaderContents, new AscendingAssetMemoryUsageComparer(), new DescendingAssetMemoryUsageComparer(), Utils.Localize( "ContentBrowser_AssetItemMemoryUsageColumn") );
                mMemoryUsageColumnHeader.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn0HeaderContents, new AssetCustomDataComparer( true, 0 ), new AssetCustomDataComparer( false, 0 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn0" ) );
				mCustomDataColumn0Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn1HeaderContents, new AssetCustomDataComparer( true, 1 ), new AssetCustomDataComparer( false, 1 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn1" ) );
				mCustomDataColumn1Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn2HeaderContents, new AssetCustomDataComparer( true, 2 ), new AssetCustomDataComparer( false, 2 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn2" ) );
				mCustomDataColumn2Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn3HeaderContents, new AssetCustomDataComparer( true, 3 ), new AssetCustomDataComparer( false, 3 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn3" ) );
				mCustomDataColumn3Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn4HeaderContents, new AssetCustomDataComparer( true, 4 ), new AssetCustomDataComparer( false, 4 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn4" ) );
				mCustomDataColumn4Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn5HeaderContents, new AssetCustomDataComparer( true, 5 ), new AssetCustomDataComparer( false, 5 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn5" ) );
				mCustomDataColumn5Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn6HeaderContents, new AssetCustomDataComparer( true, 6 ), new AssetCustomDataComparer( false, 6 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn6" ) );
				mCustomDataColumn6Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn7HeaderContents, new AssetCustomDataComparer( true, 7 ), new AssetCustomDataComparer( false, 7 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn7" ) );
				mCustomDataColumn7Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn8HeaderContents, new AssetCustomDataComparer( true, 8 ), new AssetCustomDataComparer( false, 8 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn8" ) );
				mCustomDataColumn8Header.Click += AssetListView_ColumnHeaderClickedHandler;

				mSortingOptions.Add( null, mCustomDataColumn9HeaderContents, new AssetCustomDataComparer( true, 9 ), new AssetCustomDataComparer( false, 9 ), Utils.Localize( "ContentBrowser_AssetItemCustomDataColumn9" ) );
				mCustomDataColumn9Header.Click += AssetListView_ColumnHeaderClickedHandler;
				
				// Auto-sort the list by the asset's Name property
				UpdateSortMenu();
				UpdateSortFromSortOptions();
			}

			// -- Sorting in AssetView --
			{
				// Register handlers for opening and closing the Sort options popup.
				mShowSortOptionsButton.Click += new RoutedEventHandler( mShowSortOptionsButton_Click );
				mSortOptionsPopup.Closed += new EventHandler( mSortOptionsPopup_Closed );

				// Register handlers for sort options changing.
				mSort_Ascending.Checked += new RoutedEventHandler( SortOptions_Changed );
				mSort_Descending.Checked += new RoutedEventHandler( SortOptions_Changed );
				mSort_ByName.Checked += new RoutedEventHandler( SortOptions_Changed );
				mSort_ByType.Checked += new RoutedEventHandler( SortOptions_Changed );
				mSort_ByTags.Checked += new RoutedEventHandler( SortOptions_Changed );
				mSort_ByPath.Checked += new RoutedEventHandler( SortOptions_Changed );
				mSort_ByDateAdded.Checked += new RoutedEventHandler( SortOptions_Changed );
                mSort_ByMemoryUsage.Checked += new RoutedEventHandler( SortOptions_Changed );
			}



			// Zoom widget for asset canvas
			{
				m_AssetCanvas.ZoomAmountChanged += new AssetCanvas.ZoomAmountChangedHandler( AssetCanvas_ZoomAmountChanged );

				m_ZoomWidgetResetButton.Click += new RoutedEventHandler( ZoomWidgetResetButton_Click );
				mZoomBorder.MouseDoubleClick += new MouseButtonEventHandler( ZoomWidgetResetButton_Click );

				m_ZoomDragSlider.ValueChanged += new RoutedPropertyChangedEventHandler<double>( ZoomDragSlider_ValueChanged );
				
				m_ZoomDragSlider.Minimum = AssetCanvasDefs.FurthestZoomedOutAmount;
				m_ZoomDragSlider.Maximum = AssetCanvasDefs.NearestZoomedInAmount;

				// Turn on 'draw as percentage'
				m_ZoomDragSlider.DrawAsPercentage = true;

				// Set exponential progress bar size
				m_ZoomDragSlider.ProgressRectExponent = 4.0;


				UpdateZoomWidgetState();
			}


			// Thumbnail size
			{
                m_ThumbnailSizeCombo.SelectionChanged += new SelectionChangedEventHandler(ThumbnailSizeCombo_SelectionChanged);
			}
		}
		
		/// Update the sort being applied to the asset list view to match the settings in mSortingOptions
		void UpdateSortFromSortOptions()
		{
			ListCollectionView MyAssetListView = (ListCollectionView)CollectionViewSource.GetDefaultView( this.Model );
			MyAssetListView.CustomSort = mSortingOptions.GetActiveComparer();
			AssetCanvas.NeedsVisualRefresh = true;
		}


		/// Called when the selected thumbnail size changes in the combo box
		void ThumbnailSizeCombo_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{
			// Parse the thumbnail size from the combo box
			string NewThumbnailSizeString = (string)( (ComboBoxItem)m_ThumbnailSizeCombo.SelectedValue ).Content;
			double NewThumbnailSize = double.Parse( NewThumbnailSizeString );

			// Update thumbnails!
			AssetCanvas.VisualThumbnailWidth = NewThumbnailSize;
			AssetCanvas.VisualThumbnailHeight = NewThumbnailSize;
			AssetCanvas.VisualWidth = NewThumbnailSize + AssetCanvasDefs.ExtraVisualWidth;
			AssetCanvas.VisualHeight = NewThumbnailSize + AssetCanvasDefs.ExtraVisualHeight;

			// Queue a refresh!
			AssetCanvas.NeedsVisualRefresh = true;
		}




		#region Zoom widget



		void UpdateZoomWidgetState()
		{
			m_ZoomDragSlider.Value = AssetCanvas.ZoomAmount;


			// Set value update rate while dragging or scrolling mouse wheel
			{
				// Apply zoom exponentially so that it feels better when zoomed in pretty far
				double ZoomProgress = MathUtils.Clamp(
					( AssetCanvas.ZoomAmount - AssetCanvasDefs.FurthestZoomedOutAmount ) /
					( AssetCanvasDefs.NearestZoomedInAmount - AssetCanvasDefs.FurthestZoomedOutAmount ),
					0.001,
					1.0 );
				double ExpZoomProgress = 1.0f - Math.Pow( 1.0f - ZoomProgress, 4.0 );


				// Drag
				{
					const double ZoomSpeed = 0.05;
					m_ZoomDragSlider.ValuesPerDragPixel = ZoomSpeed * ExpZoomProgress;
				}


				// Mouse wheel
				{
					const double ZoomSpeed = 0.01;
					m_ZoomDragSlider.ValuesPerMouseWheelScroll = ZoomSpeed * ExpZoomProgress;
				}

			}

		}



		void ZoomDragSlider_ValueChanged( object sender, RoutedPropertyChangedEventArgs<double> e )
		{
			// Default to using the center of the canvas
			Point ZoomTargetPoint =
				new Point( AssetCanvas.AssetCanvasScrollViewer.ActualWidth / 2, AssetCanvas.AssetCanvasScrollViewer.ActualHeight / 2 );

			// Zoom!!
			double ZoomTarget = e.NewValue;
			AssetCanvas.ZoomTo( ZoomTarget, ZoomTargetPoint );
		}



		/// <summary>
		/// Called when the canvas zoom level changed
		/// </summary>
		/// <param name="OldZoomAmount">Old zoom level</param>
		/// <param name="NewZoomAmount">The new zoom level</param>
		void AssetCanvas_ZoomAmountChanged( double OldZoomAmount, double NewZoomAmount )
		{
			// Canvas zoom changed, so update our widget
			UpdateZoomWidgetState();
		}



		/// Called when the "reset zoom to 100%" button is clicked
		void ZoomWidgetResetButton_Click( object sender, RoutedEventArgs e )
		{
			// Default to using the center of the canvas
			Point ZoomTargetPoint =
				new Point( AssetCanvas.AssetCanvasScrollViewer.ActualWidth / 2, AssetCanvas.AssetCanvasScrollViewer.ActualHeight / 2 );

			AssetCanvas.ZoomTo( 1.0, ZoomTargetPoint );

			e.Handled = true;
		}

#endregion




		#region Sort Options Popup

		/// When false, do not execute the SortOptions_Changed callback; used to prevent recursion.
		bool bRespondTo_SortOptions_SortChanged = true;
		/// Called when one of the sorting options in the Sorting Options Menu is changed.
		void SortOptions_Changed( object sender, RoutedEventArgs e )
		{
			if ( bRespondTo_SortOptions_SortChanged )
			{
				mSortingOptions.SortDirection = ( mSort_Ascending.IsChecked ?? true ) ? SortingOptions.Direction.Ascending : SortingOptions.Direction.Descending;
				int CheckedSortRadioIndex = mSortingOptions.IndexOfCheckedSortRadio();
				
				// If a radio sort option was checked, update the active sort column.
				// Otherwise, we are sorting by one of the custom columns and the user changed ascending/descending.

				bool RadioSortOptionChecked = ( CheckedSortRadioIndex >= 0 );
				if ( RadioSortOptionChecked )
				{
					mSortingOptions.ActiveColumnIndex = CheckedSortRadioIndex;
				}
				

				UpdateSortMenu();
				UpdateSortColumns();
				UpdateSortFromSortOptions();
			}			
		}

		/// Updated the Sort Menu appearance based on the state of mSortingOptions.
		void UpdateSortMenu()
		{
			try
			{
				// We want to tweak the UI to adjust the filter, so do not adjust the filter in response to these UI changes... prevents feedback.
				this.bRespondTo_SortOptions_SortChanged = false;

				// Make sure the appropriate radio button is checked based on current search parameter. Do not check anything if we are sorting by a custom attribute.
				RadioButton ActiveRadioButton = mSortingOptions.ActiveRadioButton;
				if ( ActiveRadioButton != null )
				{
					ActiveRadioButton.IsChecked = true;
				}
				else
				{
					mSortingOptions.UncheckAllRadioButtons();
				}

				
				// Update the ascending vs. descending radio buttons.
				if ( mSortingOptions.SortDirection == SortingOptions.Direction.Ascending )
				{
					this.mSort_Ascending.IsChecked = true;
				}
				else
				{
					this.mSort_Descending.IsChecked = true;
				}

				// Update the sort description that appears in the Faux ComboBox..
				this.mShowSortOptionsButton.Content = mSortingOptions.ActiveSortDescription;

			}
			finally
			{
				// We should start responding to changes now.
				this.bRespondTo_SortOptions_SortChanged = true;
			}

		}																															   																																   
		

		/// Called when user clicks on button to open the Sort Options Menu.
		void mShowSortOptionsButton_Click( object sender, RoutedEventArgs e )
		{
			this.mSortOptionsPopup.IsOpen = true;

			// Disable the button so that clicking on it again won't re-open the faux combo.
			mShowSortOptionsButton.IsEnabled = false;
		}

		/// Called when the sort options menu is closed.
		void mSortOptionsPopup_Closed( object sender, EventArgs e )
		{
			mShowSortOptionsButton.IsEnabled = true;
		}

		#endregion

		#region	List Sorting

		RoutedEventHandler AssetListView_ColumnHeaderClickedHandler;

		/// Sets the column sort options based on the sort options pop-down menu
		void UpdateSortColumns()
		{
			ResetSortColumns();
			int ActiveColumnIndex = mSortingOptions.ActiveColumnIndex;
			mSortingOptions.ActiveColumnHeader.SortDirection = mSortingOptions.SortDirection;
		}

		void AssetListView_ColumnHeaderClicked( object sender, RoutedEventArgs e )
		{

			// The control INSIDE the header that we clicked on
			SortableColumnHeader ClickedHeader = ( (GridViewColumnHeader)sender ).Content as SortableColumnHeader;

			// Did we click on a column that was not sorting already?
			if ( ClickedHeader.SortDirection == SortingOptions.Direction.NoSort )
			{
				ResetSortColumns();
			}

			// The clicked column either started sorting or toggled its sort
			ClickedHeader.CycleSortDirection();

			// Update the SortingOptions based on UI changes.
			int ClickedHeaderIndex = mSortingOptions.IndexFromColumnHeader( ClickedHeader );
			mSortingOptions.ActiveColumnIndex = ClickedHeaderIndex;
			mSortingOptions.SortDirection = ClickedHeader.SortDirection;			

			// Update the sorting menu to match the recent changes.
			UpdateSortMenu();

			// Look up the comparer 
			UpdateSortFromSortOptions();
		}

		/// Set all columns in the list view to not sort.
		void ResetSortColumns()
		{
			// reset all columns to NoSort
			mNameColumnHeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mTypeColumnHeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mTagsColumnHeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mPathColumnHeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mDateAddedColumnHeaderContents.SortDirection = SortingOptions.Direction.NoSort;
            mMemoryUsageColumnHeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn0HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn1HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn2HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn3HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn4HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn5HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn6HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
			mCustomDataColumn7HeaderContents.SortDirection = SortingOptions.Direction.NoSort;
		}


		class AscendingAssetNameComparer : IComparer
		{
			public int Compare(	object x, object y )
			{
				int	CompareResult =	String.Compare(	( x	as AssetItem ).Name, ( y as	AssetItem ).Name );
				return CompareResult;
			}
		}

		class DescendingAssetNameComparer :	IComparer
		{
			public int Compare(	object x, object y )
			{
				int	CompareResult =	String.Compare(	( y	as AssetItem ).Name, ( x as	AssetItem ).Name );
				return CompareResult;
			}
		}

		class AscendingAssetTypeComparer : IComparer
		{
			public int Compare(	object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult = String.Compare( AssetX.AssetType, AssetY.AssetType );

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					CompareResult = String.Compare( AssetX.Name, AssetY.Name );
				}

				return CompareResult;
			}
		}

		class DescendingAssetTypeComparer :	IComparer
		{
			public int Compare(	object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult = String.Compare( AssetY.AssetType, AssetX.AssetType );

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					CompareResult = String.Compare( AssetY.Name, AssetX.Name );
				}

				return CompareResult;
			}
		}

		class AscendingAssetTagsComparer : IComparer
		{
			public int Compare(	object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult = String.Compare( AssetX.TagsAsString, AssetY.TagsAsString );

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					CompareResult = String.Compare( AssetX.Name, AssetY.Name );
				}

				return CompareResult;
			}
		}

		class DescendingAssetTagsComparer :	IComparer
		{
			public int Compare(	object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult = String.Compare( AssetY.TagsAsString, AssetX.TagsAsString );

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					CompareResult = String.Compare( AssetY.Name, AssetX.Name );
				}

				return CompareResult;
			}
		}

		class AscendingAssetPathComparer : IComparer
		{
			public int Compare(	object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult = String.Compare( AssetX.PathOnly, AssetY.PathOnly );

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					CompareResult = String.Compare( AssetX.Name, AssetY.Name );
				}

				return CompareResult;
			}
		}

		class DescendingAssetPathComparer :	IComparer
		{
			public int Compare(	object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult = String.Compare( AssetX.PathOnly, AssetY.PathOnly );

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					CompareResult = String.Compare( AssetX.Name, AssetY.Name );
				}

				return CompareResult;
			}
		}

		class AssetDateAddedComparer : IComparer
		{
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="IsAscending">True for an ascending sort, otherwise false</param>
			public AssetDateAddedComparer( bool IsAscending )
			{
				m_IsAscending = IsAscending;
			}


			/// <summary>
			/// IComparer: Comparison function
			/// </summary>
			public int Compare( object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				int CompareResult;
				if( m_IsAscending )
				{
					CompareResult = DateTime.Compare( AssetX.DateAdded, AssetY.DateAdded );
				}
				else
				{
					CompareResult = DateTime.Compare( AssetY.DateAdded, AssetX.DateAdded );
				}

				if( CompareResult == 0 )
				{
					// Fall back to a name compare if needed
					if( m_IsAscending )
					{
						CompareResult = String.Compare( AssetX.Name, AssetY.Name );
					}
					else
					{
						CompareResult = String.Compare( AssetY.Name, AssetX.Name );
					}
				}

				return CompareResult;
			}


			/// True if we want an ascending sort, otherwise false
			private bool m_IsAscending = true;
		}

        class AscendingAssetMemoryUsageComparer : IComparer
        {
            public int Compare(object x, object y)
            {
                int XMemory = (x as AssetItem).RawMemoryUsage;
                int YMemory = (y as AssetItem).RawMemoryUsage;

                return XMemory.CompareTo( YMemory );
            }
        }

        class DescendingAssetMemoryUsageComparer : IComparer
        {
            public int Compare(object x, object y)
            {
                int XMemory = (x as AssetItem).RawMemoryUsage;
                int YMemory = (y as AssetItem).RawMemoryUsage;

                return YMemory.CompareTo( XMemory );
            }
        }

		class AssetCustomDataComparer : IComparer
		{
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="IsAscending">True for an ascending sort, otherwise false</param>
			/// <param name="CustomDataColumnIndex">Asset custom data column index to sort</param>
			public AssetCustomDataComparer( bool IsAscending, int CustomDataColumnIndex )
			{
				m_IsAscending = IsAscending;
				m_CustomDataColumnIndex = CustomDataColumnIndex;
			}


			/// <summary>
			/// IComparer: Comparison function
			/// </summary>
			public int Compare( object x, object y )
			{
				AssetItem AssetX = x as AssetItem;
				AssetItem AssetY = y as AssetItem;

				// Make sure blank columns get sorted to the beginning (or end) appropriately
				bool AssetXHasColumn = AssetX.CustomDataColumns.Count > m_CustomDataColumnIndex;
				bool AssetYHasColumn = AssetY.CustomDataColumns.Count > m_CustomDataColumnIndex;

				int CompareResult = 0;
				if( !AssetXHasColumn || !AssetYHasColumn )
				{
					if( AssetXHasColumn && !AssetYHasColumn )
					{
						CompareResult = m_IsAscending ? -1 : 1;
					}
					else if( !AssetXHasColumn && AssetYHasColumn )
					{
						CompareResult = m_IsAscending ? 1 : 0;
					}
				}
				else
				{
					if( m_IsAscending )
					{
						CompareResult = String.Compare( AssetX.CustomDataColumns[ m_CustomDataColumnIndex ], AssetY.CustomDataColumns[ m_CustomDataColumnIndex ] );
					}
					else
					{
						CompareResult = String.Compare( AssetY.CustomDataColumns[ m_CustomDataColumnIndex ], AssetX.CustomDataColumns[ m_CustomDataColumnIndex ] );
					}
				}

				// Fall back to a name compare if needed
				if( CompareResult == 0 )
				{
					if( m_IsAscending )
					{
						CompareResult = String.Compare( AssetX.Name, AssetY.Name );
					}
					else
					{
						CompareResult = String.Compare( AssetY.Name, AssetX.Name );
					}
				}

				return CompareResult;
			}


			/// True if we want an ascending sort, otherwise false
			private bool m_IsAscending = true;

			/// Data column we're sorting
			private int m_CustomDataColumnIndex = 0;
		} 

		#endregion		  
		
        #region Drag And Drop Support

        /// <summary>
        /// Helper function that checks if a drag-drop operation should be started based on the condition of the mouse and asset view
        /// </summary>
        /// <param name="LeftMouseButtonState">State of the left mouse button - pressed or released</param>
        /// <param name="CurPosition">Current position of the mouse while trying to initiate the drag-drop</param>
        public void AttemptDragDrop( MouseButtonState LeftMouseButtonState, Point CurPosition )
        {
            if ( LeftMouseButtonState == MouseButtonState.Pressed && !IsContextMenuOpen && ( SelectedCount > 0 ) )
            {
                double DeltaX = Math.Abs( CurPosition.X - mAssetMouseDownLocation.X );
                double DeltaY = Math.Abs( CurPosition.Y - mAssetMouseDownLocation.Y );

                if ( DeltaX > SystemParameters.MinimumHorizontalDragDistance || DeltaY > SystemParameters.MinimumVerticalDragDistance )
                {
                    String SelectedAssetPaths = mMainControl.Backend.MarshalAssetItems( SelectedAssets );
                    mMainControl.Backend.BeginDragDrop( SelectedAssetPaths );
                }
            }
        }

        #endregion

        /// Called when the left mouse button is double-clicked in the canvas scroll viewer
		void AssetListView_MouseDoubleClick( object sender, MouseButtonEventArgs e )
		{
			// Figure out which list item the user clicked on
			Object FoundListItem = FindListBoxItemAtPosition(
				AssetListView,
				e.GetPosition( AssetListView ) );

			// Did the user double click on a list item?  If not, it may have been the background or another
			// visual component of the list view
			if( FoundListItem != null )
			{
				// Selected item should ALWAYS be an asset item
				AssetItem ClickedAssetItem = (AssetItem)FoundListItem;

				// Activate the asset!
				mMainControl.Backend.LoadAndActivateSelectedAssets();

				// Update the loaded status
				mMainControl.Backend.UpdateAssetStatus( ClickedAssetItem, AssetStatusUpdateFlags.LoadedStatus );
			}
		}



		/// Called when the asset view model's item collection has changed
		void Model_CollectionChanged( object sender, NotifyCollectionChangedEventArgs e )
		{
			// Asset model data has changed, so we'll update the asset canvas next tick

			// NOTE: At this point, the AssetListView hasn't been notified that the asset item list
			//       has changed, thus it's internal item list will be out of date.  This is one of
			//		 the reasons we defer the update until the next tick
			AssetCanvas.NeedsVisualRefresh = true;
        }

        #region Asset List Item Mouse Handling
        /// Called when the mouse moves over an asset list item
		void AssetListItem_MouseEnter( object sender, MouseEventArgs e )
		{
			// Figure out which list item the mouse is over
			Object FoundListItem = FindListBoxItemAtPosition(
				AssetListView,
				e.GetPosition( AssetListView ) );

			if( FoundListItem != null )
			{
				mMainControl.StartAssetItemToolTip( (AssetItem)FoundListItem );
			}
		}

		/// Called when the mouse leaves an asset list item
		void AssetListItem_MouseLeave( object sender, MouseEventArgs e )
		{
			mMainControl.StopAssetItemToolTip();
		}

        /// <summary>
        /// Called when the mouse moves over an asset list item; attempts to begin a drag-drop operation if applicable
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        void AssetListItem_MouseMove( object sender, MouseEventArgs e )
        {
            AttemptDragDrop( e.LeftButton, e.GetPosition( this ) );
        }

        /// <summary>
        /// Called when the left mouse button is released after clicking on an asset list item; Resets the position of
        /// the asset mouse down location
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        void AssetListItem_MouseLeftButtonUp( object sender, MouseEventArgs e )
        {
            this.mAssetMouseDownLocation.X = 0;
            this.mAssetMouseDownLocation.Y = 0;
        }

        /// <summary>
        /// Called when the left mouse button is pressed down on an asset list item; Sets the position of the asset
        /// mouse down location
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        void AssetListItem_MouseLeftButtonDown( object sender, MouseEventArgs e )
        {
            this.mAssetMouseDownLocation = e.GetPosition(this);
        }

        #endregion

        #region Filter Refreshing
        /// <summary>
		/// Request the that filtering be refreshed. Refreshes will be deferred until .RefreshInterval has transpired.
		/// Note: Will also re-sort items due to usage of CollectionView
		/// <param name="AssetFilterIn">The asset filter we should use to repaint; use null to signify "use current filter"</param>
		/// <param name="RefreshType">Should we request a deferred refresh or an immediate refresh.</param>
		/// </summary>
		public void RequestFilterRefresh( AssetFilter AssetFilterIn, RefreshFlags InRefreshFlags )
		{
			if (AssetFilterIn != null)
			{
				mAssetFilter = AssetFilterIn;
			}

			if ( InRefreshFlags.ShouldScrollToTop() )
			{
				ScrollToTop();
			}

			if ( InRefreshFlags.IsDeferred() )
			{
				// We have a deferred refresh request, so start waiting to refresh.
				// In we were already waiting, then reset the clock.
				mFilterStopwatch.Reset();
				mFilterStopwatch.Start();				
			}
			else
			{
				// We have an immediate refresh request; just refresh.
				mShouldRefreshFilter = true;
				// If there was a pending refresh, it will be fulfilled next tick.
				mFilterStopwatch.Stop();
			}
		}

		/// <summary>
		/// If a refresh is pending we cause the CollectionView to refresh its filter.
		/// This is necessary to make the typing feel smooth, yet still give an illusion of "continuous filtering".
		/// Called every tick.
		/// </summary>
		/// <param name="bIsAssetQueryInProgress"/>True if we're in the middle of an asset query right now
		public void FilterRefreshTick( bool bIsAssetQueryInProgress )
		{
            // Allow history snapshots to be taken this tick
			mMainControl.AllowHistorySnapshots( true );

			bool RefreshIsPending = mFilterStopwatch.IsRunning;
			bool TimeToRefresh = mFilterStopwatch.ElapsedMilliseconds > MillisecondsFilterRefreshDelay;
			
			if ( RefreshIsPending && TimeToRefresh )
			{
				mShouldRefreshFilter = true;
				mFilterStopwatch.Reset();

                // Once the user has finished typing for a set amount of time, take a history snapshot.
				mMainControl.TakeHistorySnapshot( "Search String Changed" );
			}

			// Refresh the filter tiers only if there is no pending filter change.
			// It's more important for the user to get a fast response to their filter
			// than to see the tiers update right away.
			if ( mShouldRefreshFilterTiers && !mShouldRefreshFilter && !bIsAssetQueryInProgress )
			{
				mShouldRefreshFilterTiers = false;
				// Refresh the tag tiers.
				mMainControl.FilterPanel.RefreshTagTiers();

                if ( bPendingFilterHistorySnapshot )
                {
                    // if there is a pending filter take a history snapshot
                    mMainControl.TakeHistorySnapshot(PendingFilterSnapshotDesc);
                    // Once the history snapshot is taken, there should not be a pending one
                    bPendingFilterHistorySnapshot = false;
                    PendingFilterSnapshotDesc = null;
                }
			}

			if ( mShouldRefreshFilter && !bIsAssetQueryInProgress )
			{
				mShouldRefreshFilter = false;
				mShouldRefreshFilterTiers = true;

				if ( mMainControl.Filter.InUseFilterEnabled )
				{
					// If the in use filter is enabled we need to re-tag in use objects
					mMainControl.Backend.TagInUseObjects( );
				}

				ListCollectionView MyAssetListView = (ListCollectionView)CollectionViewSource.GetDefaultView( this.Model );
				// @todo: we should probably just update the entire AssetFilter from the filter panel UI here instead of doing it piecemeal.
				mMainControl.FilterPanel.UpdateFilterModelTagTiers();
				MyAssetListView.Filter = mAssetFilter.GetFilterPredicate();


				// Tell the asset canvas to refresh
				AssetCanvas.NeedsVisualRefresh = true;
				
				mMainControl.UpdateAssetCount();


				if( this.AssetListView.Items.Count <= 0 )
				{
					// Only display a warning about nothing matching the filter if we're not currently in the middle
					// of refreshing the source data
					if( !bIsAssetQueryInProgress )
					{
						mMainControl.PlayQuickNotification( UnrealEd.Utils.Localize( "ContentBrowser_Notification_NothingToShowCheckYourFilter" ) );
					}
				}
			}

			// Highlight the entire asset view's border whenever we are filtering.
			// We really want the user to know that they might be missing some assets
			mAssetViewBorder.BorderBrush = ( mAssetFilter.IsNullFilter() ) ? Brushes.Transparent : Brushes.OrangeRed;
		}
		#endregion


		/// <summary>
		/// Determines if the given asset type will be visible given the existing object type filter.
		/// </summary>
		/// <param name="AssetType">The type of asset to check against the existing object type filter.</param>
		/// <param name="IsArchetype">Is the asset an archetype?</param>
		/// <returns>true if the given asset type will pass the object type filter.</returns>
		public bool PassesObjectTypeFilter( String AssetType, bool IsArchetype )
		{
			return mAssetFilter.PassesObjectTypeFilter( AssetType, IsArchetype );
		}

		/// <summary>
		/// Determines if the given asset will be shown if it passes the FlattenedTexture filter
		/// </summary>
		/// <param name="Asset"></param>
		/// <returns></returns>
		public bool PassesShowFlattenedTextureFilter(String AssetType, String AssetName)
		{
			return mAssetFilter.PassesShowFlattenedTextureFilter(AssetType, AssetName);
		}

		#region Layout
		/// <summary>
		/// Restore the UI state
		/// </summary>
        /// <param name="ContentBrowserUIStateIn">ContentBrowserUIState about the state of the UI</param>
        public void RestoreContentBrowserUIState(ContentBrowserUIState ContentBrowserUIStateIn)
		{
			SavedListViewHeight = ContentBrowserUIStateIn.AssetListViewHeight;
			SavedListViewWidth = ContentBrowserUIStateIn.AssetListViewWidth;
			LayoutModeToRadioButton( ContentBrowserUIStateIn.GetAssetViewLayout() ).IsChecked = true;

            string SavedThumbnailSizeAsString = ContentBrowserUIStateIn.AssetViewThumbnailSize.ToString();
            int SavedSizeIndex = 0;

            // Confirm the saved thumbnail size is a valid choice in the combo box, and if so, obtain its index in the box
            for (int i = 0; i < m_ThumbnailSizeCombo.Items.Count; ++i)
            {
                string CurComboBoxThumbnailSize = (string)(((ComboBoxItem)m_ThumbnailSizeCombo.Items.GetItemAt(i)).Content);
                if (String.Compare(SavedThumbnailSizeAsString, CurComboBoxThumbnailSize) == 0)
                {
                    SavedSizeIndex = i;
                    break;
                }

            }
            this.m_ThumbnailSizeCombo.SelectedIndex = SavedSizeIndex;

			this.mNameColumn.Width		= ContentBrowserUIStateIn.AssetView_List_NameColumnWidth;
			this.mAssetTypeColumn.Width	= ContentBrowserUIStateIn.AssetView_List_AssetTypeColumnWidth;
			this.mTagsColumn.Width		= ContentBrowserUIStateIn.AssetView_List_TagsColumnWidth;
			this.mPathColumn.Width		= ContentBrowserUIStateIn.AssetView_List_PathColumnWidth;
			this.mDateAddedColumn.Width = ContentBrowserUIStateIn.AssetView_List_DateAddedColumnWidth;
			this.mMemoryUsageColumn.Width = ContentBrowserUIStateIn.AssetView_List_MemoryUsageColumnWidth;
			this.mCustomDataColumn0.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 0 ];
			this.mCustomDataColumn1.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 1 ];
			this.mCustomDataColumn2.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 2 ];
			this.mCustomDataColumn3.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 3 ];
			this.mCustomDataColumn4.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 4 ];
			this.mCustomDataColumn5.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 5 ];
			this.mCustomDataColumn6.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 6 ];
			this.mCustomDataColumn7.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 7 ];
			this.mCustomDataColumn8.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 8 ];
			this.mCustomDataColumn9.Width = ContentBrowserUIStateIn.AssetView_List_CustomDataColumnWidths[ 9 ];

		}

		/// <summary>
        /// Saves the relevant UI state changes to the ContentBrowserUIState
		/// </summary>
        /// <param name="OutContentBrowserUIState">ContentBrowserUIState to modify in accordance to the current ui state</param>
        public void SaveContentBrowserUIState(ContentBrowserUIState OutContentBrowserUIState)
		{
			OutContentBrowserUIState.AssetViewLayoutAsDouble = (double)this.mLayoutMode;
            OutContentBrowserUIState.AssetViewThumbnailSize = double.Parse( (string)((ComboBoxItem)m_ThumbnailSizeCombo.SelectedValue).Content );
			OutContentBrowserUIState.AssetListViewHeight = SavedListViewHeight;
			OutContentBrowserUIState.AssetListViewWidth = SavedListViewWidth;
			OutContentBrowserUIState.AssetView_List_NameColumnWidth = this.mNameColumn.ActualWidth;
			OutContentBrowserUIState.AssetView_List_AssetTypeColumnWidth = this.mAssetTypeColumn.ActualWidth;
			OutContentBrowserUIState.AssetView_List_TagsColumnWidth = this.mTagsColumn.ActualWidth;
			OutContentBrowserUIState.AssetView_List_PathColumnWidth = this.mPathColumn.ActualWidth;
			OutContentBrowserUIState.AssetView_List_DateAddedColumnWidth = this.mDateAddedColumn.ActualWidth;
			OutContentBrowserUIState.AssetView_List_MemoryUsageColumnWidth = this.mMemoryUsageColumn.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 0 ] = this.mCustomDataColumn0.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 1 ] = this.mCustomDataColumn1.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 2 ] = this.mCustomDataColumn2.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 3 ] = this.mCustomDataColumn3.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 4 ] = this.mCustomDataColumn4.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 5 ] = this.mCustomDataColumn5.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 6 ] = this.mCustomDataColumn6.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 7 ] = this.mCustomDataColumn7.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 8 ] = this.mCustomDataColumn8.ActualWidth;
			OutContentBrowserUIState.AssetView_List_CustomDataColumnWidths[ 9 ] = this.mCustomDataColumn9.ActualWidth;
		}
		#endregion

		/// Visible count
		public int PostFilterCount { get { return m_AssetListView.Items.Count; } }

        #region Asset Selection

		/// <summary>
		/// Scrolls the asset list and canvas so that the specified item is onscreen
		/// </summary>
		/// <param name="TargetItem">the asset item to sync the views to</param>
		public void SynchronizeViewToAsset( AssetItem TargetItem )
		{
			if ( TargetItem != null )
			{
				if ( AssetListView != null )
				{
					try
					{
						AssetListView.ScrollIntoView( TargetItem );
					}

					// @todo CB: A bug in .NET 3.5 SP1 "recycling" virtualization mode requires this hack
					//		http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=346158
					//  After we update/require .NET 4.0+ we should be able to remove this exception block!
					catch( System.NullReferenceException )
					{
						// (Absorb)
					}
					catch( System.ArgumentOutOfRangeException )
					{
						// (Absorb)
					}
				}
				if ( AssetCanvas != null )
				{
					AssetCanvas.ScrollAssetCanvasToItem(TargetItem);
				}
			}
		}

		#region Asset Selection Data

		/// Get the number of assets currently selected
		public int SelectedCount { get { return m_AssetListView.SelectedItems.Count; } }

		/// <summary>
		/// Assets currently selected by the user
		/// </summary>
		public ReadOnlyCollection<AssetItem> SelectedAssets
		{
			get
			{
				List<AssetItem> MySelectedAssets = new List<AssetItem>(m_AssetListView.SelectedItems.Count);
				foreach ( AssetItem SelectedItem in m_AssetListView.SelectedItems )
				{
					MySelectedAssets.Add(SelectedItem);
				}
				return MySelectedAssets.AsReadOnly();
			}
		}

		public List<String> CloneSelectedAssetFullNames()
		{
			List<String> SelectedAssetFullNames = new List<String>( m_AssetListView.SelectedItems.Count );
			foreach( AssetItem SelectedItem in m_AssetListView.SelectedItems )
			{
				SelectedAssetFullNames.Add( SelectedItem.FullName );
			}
			return SelectedAssetFullNames;
		}

		#endregion

		#region Asset Selection Methods
		/// <summary>
		/// Unselect all assets and select the 'SelectMe' asset.
		/// </summary>
		/// <param name="SelectMe">Asset to be selected; must be a valid asset in the AssetView.</param>
		public void SetSelection( AssetItem SelectMe )
		{
			if ( !AssetListView.IsNotifyingSelectionChange )
			{
				AssetListView.BeginBatchSelection();
				AssetListView.SelectedItems.Clear();
				if ( SelectMe != null )
				{
					AssetListView.SelectedItems.Add(SelectMe);
				}
				else
				{
					AssetListView.SelectedItem = null;
				}

				AssetListView.EndBatchSelection(true);

				SynchronizeViewToAsset(SelectMe);
			}
		}

		/// <summary>
		/// Helper function to select multiple assets at once that's a bit more performance-friendly
		/// than adding each item to the selection set one-by-one.
		/// </summary>
		/// <param name="InAssetsToSelect">List of assets to select</param>
		/// <param name="bClearExistingSelection">controls whether currently selected items will be de-selected first</param>
		public void SelectMultipleAssets( ICollection InAssetsToSelect, bool bClearExistingSelection )
		{
			if ( !AssetListView.IsNotifyingSelectionChange )
			{
				m_AssetListView.BeginBatchSelection();
				if ( bClearExistingSelection && SelectedCount > 0 )
				{
					AssetListView.SelectedItems.Clear();
				}

				// Select all items in the view model
				// @todo cb : Ideally, WPF would support an AddRange method here! (also see MultiSelector in .NET 3.5 SP1)
				AssetListView.ForceSetSelectedItems( InAssetsToSelect );

				m_AssetListView.EndBatchSelection(true);
			}
		}

		/// <summary>
		/// Initiates deferred selection of multiple assets.  The assets will become selected after the current
		/// query is complete and the filter is applied.
		/// </summary>
		/// <param name="AssetNamesToSelect">List of asset full names to select</param>
		public void StartDeferredAssetSelection( List<String> AssetFullNamesToSelect )
		{
			// Set the list of asset names we'll select after the current query is finished
			if( m_DeferredAssetFullNamesToSelect == null )
			{
				m_DeferredAssetFullNamesToSelect = new List<String>();
			}

			foreach ( String AssetFullName in AssetFullNamesToSelect )
			{
				if( !Utils.ListContainsString( AssetFullName, m_DeferredAssetFullNamesToSelect, true ) )
				{
					m_DeferredAssetFullNamesToSelect.Add( AssetFullName );
				}
			}
		}

		/// <summary>
		/// Selects any deferred assets if we any were queued for selection
		/// </summary>
		public void SelectDeferredAssetsIfNeeded()
		{
			if( m_DeferredAssetFullNamesToSelect != null && m_DeferredAssetFullNamesToSelect.Count > 0 )
			{
				// Build up a list of asset items to select
				var AssetsToSelect = new List<AssetItem>();
				foreach( String CurAssetFullName in m_DeferredAssetFullNamesToSelect )
				{
					AssetItem CurAsset = FindAssetItem( CurAssetFullName );
					if ( CurAsset != null )
					{
						AssetsToSelect.Add( CurAsset );
					}
				}

				// Clear the pending list of assets to select now that they've been selected
				m_DeferredAssetFullNamesToSelect.Clear();

				// Select the assets
				SelectMultipleAssets( AssetsToSelect, true );
			}
		}
		#endregion

		#region Selection Events and Handlers

		public class AssetSelectionChangedEventArgs : RoutedEventArgs
        {
			public List<AssetItem> AddedItems = new List<AssetItem>();
			public List<AssetItem> RemovedItems = new List<AssetItem>();
        }

        public static readonly RoutedEvent AssetSelectionChangedEvent = EventManager.RegisterRoutedEvent(
            "AssetSelectionChanged",
            RoutingStrategy.Bubble,
            typeof(AssetSelectionChangedEventHandler),
            typeof(AssetView)
			);

        public delegate void AssetSelectionChangedEventHandler(object sender, AssetSelectionChangedEventArgs args);

		/// <summary>
		/// An event triggered when the AssetSelection changes
		/// </summary>
        public event AssetSelectionChangedEventHandler AssetSelectionChanged
        {
            add		{	AddHandler(AssetSelectionChangedEvent, value);		}
            remove	{	RemoveHandler(AssetSelectionChangedEvent, value);	}
        }


		/// Called by the ListView when the currently selected item is changed
		private void AssetListView_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{
			RaiseAssetSelectionChangedEvent(e.AddedItems, e.RemovedItems);
			if ( e.AddedItems.Count == 1 )
			{
				SynchronizeViewToAsset(e.AddedItems[0] as AssetItem);
			}
		}

		/// <summary>
		/// Must be called whenever the selected items are changed in the asset list view.  If you need good
		/// performance when adding multiple items to a selection set, you should unhook the list view's
		/// SelectionChanged event and call this function yourself after adding the items.
		/// </summary>
		/// <param name="InAddedItems">Items added to selection set</param>
		/// <param name="InRemovedItems">Items removed from selection set</param>
        private void RaiseAssetSelectionChangedEvent( IEnumerable InAddedItems, IEnumerable InRemovedItems )
        {
			AssetSelectionChangedEventArgs args = new AssetSelectionChangedEventArgs();
			args.RoutedEvent = AssetView.AssetSelectionChangedEvent;
			args.Source = this;

			// Keep track of which items were added and removed from the selection set
			foreach( AssetItem CurAssetItem in InRemovedItems )
			{
				CurAssetItem.Selected = false;
				args.RemovedItems.Add(CurAssetItem);
			}

			AssetItem FirstSelectedItem = null;
			foreach( AssetItem CurAssetItem in InAddedItems )
			{
				if( FirstSelectedItem == null )
				{
					FirstSelectedItem = CurAssetItem;
				}
				CurAssetItem.Selected = true;
				args.AddedItems.Add(CurAssetItem);
			}


//@selection - this will result in calling SyncSelectedObjectsWithGlobalSelectionSet
            RaiseEvent(args);
		}
		#endregion
		#endregion


		#region Toggle Quarantine

		private static void OnToggleQuarantineCommand( object Sender, ExecutedRoutedEventArgs EventArgs )
		{
			AssetView This = (AssetView)Sender;

			List<AssetItem> SelectedAssets = new List<AssetItem>(This.SelectedAssets);

			if (This.SelectedCount > 0)
			{
				List<AssetItem> QuarantinedInSelection = SelectedAssets.FindAll(SomeAsset => SomeAsset.IsQuarantined);

				bool Success = false;
				if (QuarantinedInSelection.Count > 0)
				{
					Success = This.mMainControl.Backend.LiftQuarantine(QuarantinedInSelection);
					if (Success)
					{
						This.mMainControl.PlayQuickNotification(UnrealEd.Utils.Localize("ContentBrowser_Notification_QuarantineLifted", QuarantinedInSelection.Count));
					}
				}
				else
				{
					Success = This.mMainControl.Backend.QuarantineAssets(SelectedAssets);
					if (Success)
					{
						This.mMainControl.PlayQuickNotification(UnrealEd.Utils.Localize("ContentBrowser_Notification_Quarantined", SelectedAssets.Count));
					}
				}
				
				if ( Success )
				{					
					foreach ( AssetItem Item in SelectedAssets )
					{
						This.mMainControl.MyAssets.UpdateAssetStatus( Item, AssetStatusUpdateFlags.Quarantined );
					}			
				}
	
			}
		}

		private static void OnCanToggleQuarantineCommand( object Sender, CanExecuteRoutedEventArgs EventArgs )
		{
			AssetView This = (AssetView)Sender;
			EventArgs.CanExecute = This.mMainControl.IsInQuarantineMode() && !This.mMainControl.Backend.IsGameAssetDatabaseReadonly();
		}

		#endregion

		#region "Remove from Collection" Command

		/// "Remove From Collection" command for asset view context menu
		private static RoutedUICommand RemoveFromCollectionCommand;

		/// Execute the "Remove from Collection" Command
		private void OnRemoveFromCollectionCommand( object Sender, ExecutedRoutedEventArgs EventArgs )
		{
			EBrowserCollectionType RemovingFromType;
			Collection RemovingFrom;
			if( mMainControl.MySourcesPanel.SelectedSharedCollections.Count > 0 )
			{
				RemovingFromType = EBrowserCollectionType.Shared;
				RemovingFrom = (Collection)mMainControl.MySourcesPanel.SelectedSharedCollections[ 0 ];
			}
			else
			{
				RemovingFromType = EBrowserCollectionType.Private;
				RemovingFrom = (Collection)mMainControl.MySourcesPanel.SelectedPrivateCollections[ 0 ];
                if (RemovingFrom.IsLocal)
                {
                    // If the collection is local, change the type to local
                    RemovingFromType = EBrowserCollectionType.Local;
                }
			}

			bool ShouldShowConfirmationPrompt = mMainControl.Backend.ShouldShowConfirmationPrompt( ConfirmationPromptType.RemoveAssetsFromCollection );
			if ( !ShouldShowConfirmationPrompt ||
				 this.SelectedCount < ContentBrowserDefs.MaxNumAssetsForNoWarnGadOperation)
			{
                List<String> AssetNames = new List<String>();
                foreach( AssetItem Asset in SelectedAssets ) 
                {
                    AssetNames.Add( Asset.FullName );
                }

				RemoveFromCollection( AssetNames, RemovingFrom, RemovingFromType );
			}
			else
			{
                String PromptTextKey = (RemovingFromType == EBrowserCollectionType.Private) ? (RemovingFromType == EBrowserCollectionType.Local) ?
                                        "ContentBrowser_RemoveFromCollectionPrompt_Prompt(Local)" :
                                        "ContentBrowser_RemoveFromCollectionPrompt_Prompt(Private)" :
                                        "ContentBrowser_RemoveFromCollectionPrompt_Prompt(Shared)";
				mProceedWithRemoveAssetsFromCollection.PromptText = Utils.Localize( PromptTextKey , SelectedCount, RemovingFrom.Name );
				mProceedWithRemoveAssetsFromCollection.ShowOptionToSuppressFuturePrompts = true;
				mProceedWithRemoveAssetsFromCollection.SuppressFuturePrompts = !ShouldShowConfirmationPrompt;
				mProceedWithRemoveAssetsFromCollection.Show( new Object[]{ this.SelectedAssets, RemovingFrom, RemovingFromType } );
			}
		}

		/// Called when the user accepts the prompt to remove assets from a collection.
		void mProceedWithRemoveAssetsFromCollection_Accepted( object Parameters )
		{
			Object[] Params = (Object[])Parameters;

            ICollection<AssetItem> AssetItems = (ICollection<AssetItem>)Params[0];

            List<String> AssetNames = new List<String>();
            foreach (AssetItem Asset in AssetItems )
            {
                AssetNames.Add(Asset.FullName);
            }

            RemoveFromCollection( AssetNames, (Collection)Params[1], (EBrowserCollectionType)Params[2]);

			if( mProceedWithRemoveAssetsFromCollection.SuppressFuturePrompts )
			{
				mMainControl.Backend.DisableConfirmationPrompt( ConfirmationPromptType.RemoveAssetsFromCollection );
			}
		}

		/// <summary>
		/// Remove assets from a specified collection.
		/// </summary>
        /// <param name="AssetFullNames">AssetItem(s) corresponding to assets to remove.</param>
		/// <param name="RemoveFrom">Collection from which the assets should be removed.</param>
		/// <param name="RemovingFromType">Type of collection: shared or private.</param>
		void RemoveFromCollection( ICollection<String> AssetFullNames, Collection RemoveFrom, EBrowserCollectionType RemoveFromType )
		{
            if (this.mMainControl.Backend.RemoveAssetsFromCollection( AssetFullNames, RemoveFrom, RemoveFromType))
			{
				// If we removed successfully, then update the view and notify the user
				mMainControl.UpdateAssetsInView( RefreshFlags.KeepSelection ); // Keep the selection.

				// Display a Content Browser-wide notification
				String Message = UnrealEd.Utils.Localize( "ContentBrowser_Notification_RemovedAssetsFromCollection" );
                Message = String.Format(Message, AssetFullNames.Count, RemoveFrom.Name);
				this.mMainControl.PlayNotification( Message );
			}
		}


		/// Determine if the "Remove from Collection" command can be executed
		private void CanExecuteRemoveFromCollectionCommand( object Sender, CanExecuteRoutedEventArgs EventArgs )
		{
			EventArgs.CanExecute = false;

			// Allow the user to remove from collections when there is one and only one selected collection.
			if ( ( mMainControl.MySourcesPanel.SelectedSharedCollections.Count + mMainControl.MySourcesPanel.SelectedPrivateCollections.Count ) == 1 )
			{
				EventArgs.CanExecute = true;
			}
		}

		#endregion



		#region Context menus

		/// Keep track of whether we have a context menu open
		/// Used by drag and drop, drag and pan
		int m_NumOpenContextMenus = 0;
		public bool IsContextMenuOpen
		{
			get
			{
				return m_NumOpenContextMenus > 0;
			}
		}

        /// <summary>
		/// Called when a context menu is summoned in the asset view, right before it's displayed to the user
		/// </summary>
		public void OnAssetCMOpening( object Sender, ContextMenuEventArgs EventArgs )
		{
			FrameworkElement SenderElement = (FrameworkElement)Sender;

			// Sometimes WPF forgets to call the ContextMenuClosing events, such as when a context menu's
			// closing animation gets interrupted by the user right clicking on a different asset.  We'll
			// make sure our state is good by faking these events if needed.
			while( IsContextMenuOpen )
			{
				OnAssetCMClosing( Sender, EventArgs );
			}


			// Grab the context menu from the element and wipe it clean
			ContextMenu ItemContextMenu = SenderElement.ContextMenu;
			ItemContextMenu.Items.Clear();

// 			Backend.Instance.LogWarningMessage(String.Format("Opening context menu {0}  Sender:{1}   AlreadyOpen:{2}", ItemContextMenu.Name, Sender, m_NumOpenContextMenus));

			++m_NumOpenContextMenus;

			
			// Figure out whether this is a "background" context menu or an "item" context menu.  We named
			// the menus in the .xaml file so we could identify them here!
			bool IsMenuForItem = false;
			if( Sender is AssetVisual || Sender is ListViewItem )
			{
				// Menu was summoned for an asset item
				IsMenuForItem = true;
			}
			else if( Sender is AssetCanvas )
			{
				foreach( AssetItem CurAssetItem in AssetCanvas.AssetItemsOnCanvas.Keys )
				{
					if( CurAssetItem.Visual != null )
					{
						if( CurAssetItem.Visual.IsMouseOver )
						{
							// Mouse is over this asset visual, so we need an item context menu instead!
							IsMenuForItem = true;
							break;
						}
					}
				}
			}


			// Append our items
			if( IsMenuForItem )
			{
				bool AnyCustomMenuItems = false;

				// Add quick access to the "Sync Package View" command.
				MenuItem SyncPackagesMI = new MenuItem();
				SyncPackagesMI.Command = PackageCommands.SyncPackageView;
				ItemContextMenu.Items.Add( SyncPackagesMI );
				CommandBinding syncBinding = new CommandBinding(
												PackageCommands.SyncPackageView,
												ExecuteAssetPackageCommand
												);
				CommandBindings.Add( syncBinding );
				ItemContextMenu.Items.Add( new Separator() );


				if( mMainControl != null )
				{
					// Gather context menu items from the engine for the currently selected objects
					List<Object> ObjectSpecificMenuItems = new List<Object>();
					mMainControl.Backend.QueryAssetViewContextMenuItems( out ObjectSpecificMenuItems ); // Out

					// Add the items to the context menu
					foreach( Object CurObject in ObjectSpecificMenuItems )
					{
						MenuItem CurMenuItem = CurObject as MenuItem;
						if( CurMenuItem != null )
						{
							ItemContextMenu.Items.Add( CurMenuItem );
						}

						Separator CurSeparator = CurObject as Separator;
						if( CurSeparator != null )
						{
							ItemContextMenu.Items.Add( CurSeparator );
						}

						AnyCustomMenuItems = true;
					}
				}


				// Add a separator if we need to
				if( AnyCustomMenuItems )
				{
					ItemContextMenu.Items.Add( new Separator() );
				}


				// Append a quick-access Source Control menu; the bindings are attached in AppendPackageSubmenu() below.
				AppendSourceControlSubmenu( ItemContextMenu );

				AppendCommonPackageTasks( ItemContextMenu );

				ItemContextMenu.Items.Add( new Separator() );

				// "Remove from collection" (only display option if exactly one collection is selected)
				if ( ( mMainControl.MySourcesPanel.SelectedSharedCollections.Count + mMainControl.MySourcesPanel.SelectedPrivateCollections.Count ) == 1 )
				{
					CommandBindings.Add( new CommandBinding( RemoveFromCollectionCommand, OnRemoveFromCollectionCommand, CanExecuteRemoveFromCollectionCommand ) );
					MenuItem MyMenuItem = new MenuItem();
					MyMenuItem.Command = RemoveFromCollectionCommand;
					ItemContextMenu.Items.Add(MyMenuItem);
				}

				// "Toggle Quarantine"
				{
					// Not adding command binding because it is already registered for the class.
					ItemContextMenu.Items.Add(new MenuItem() { Command = AssetViewCommands.ToggleQuarantine });
				}
			}
			else
			{
				if ( mMainControl != null )
				{
                    // Create a new menu item for the import command
                    MenuItem ImportMI = new MenuItem();
                    ImportMI.Command = PackageCommands.ImportAsset;
                    ItemContextMenu.Items.Add( ImportMI );
                    CommandBindings.Add( new CommandBinding( PackageCommands.ImportAsset, ExecuteMenuCommand, CanExecuteMenuCommand ) );
                    
                    // Append an separator
                    ItemContextMenu.Items.Add( new Separator() );

                    // Add the factory context menu items
					mMainControl.Backend.PopulateObjectFactoryContextMenu( ItemContextMenu );
				}
			}
		}

		/// Called whenever a context menu is closing
		public void OnAssetCMClosing( object Sender, ContextMenuEventArgs EventArgs )
		{
			FrameworkElement SenderElement = (FrameworkElement)Sender;
			//			Backend.Instance.LogWarningMessage( String.Format( "Closing context menu {0}  Sender:{1}   AlreadyOpen:{2}", SenderElement.ContextMenu, Sender, m_NumOpenContextMenus ) );

			ClearPackageCommandBindings();

			--m_NumOpenContextMenus;
			if( m_NumOpenContextMenus < 0 )
			{
				m_NumOpenContextMenus = 0;
			}
		}
		
		/// Add the Save/Fully Load items so that packages can be loaded via AssetItems' context menu.
		private void AppendCommonPackageTasks( ContextMenu ItemContextMenu )
		{
			// Add a save package menu item
			CommandBinding SaveCmd = new CommandBinding( PackageCommands.SaveAsset, ExecuteAssetPackageCommand, CanExecuteAssetPackageCommand );
			CommandBindings.Add( SaveCmd );
			MenuItem Save = new MenuItem();
			Save.Command = PackageCommands.SaveAsset;

			// Add a fully load package menu item
			CommandBinding FullyLoadCmd = new CommandBinding( PackageCommands.FullyLoadPackage, ExecuteAssetPackageCommand, CanExecuteAssetPackageCommand);
			CommandBindings.Add( FullyLoadCmd );
			MenuItem FullyLoad = new MenuItem();
			FullyLoad.Command = PackageCommands.FullyLoadPackage;

			// Add the new commands to the passed in menu
			ItemContextMenu.Items.Add( Save );
			ItemContextMenu.Items.Add( FullyLoad );
		}

		private void AppendSourceControlSubmenu( ContextMenu ItemContextMenu )
		{
			// We are going to clone menu items out of the prototype menu into the real menu item.
			MenuItem SourceControlPrototypeMenu = null;
			if ( DesignerProperties.GetIsInDesignMode(this) )
			{
				SourceControlPrototypeMenu = (MenuItem)mMainControl.MySourcesPanel.TryFindResource( "SourceControlSubmenu" );
			}
			else
			{
				SourceControlPrototypeMenu = (MenuItem)mMainControl.MySourcesPanel.FindResource( "SourceControlSubmenu" );
			}

			AppendSourceControlSubmenu( ItemContextMenu, SourceControlPrototypeMenu, this, ExecuteAssetPackageCommand, CanExecuteAssetPackageCommand );
		}

		/// <summary>
		/// Builds a sub-menu of commands for manipulating the packages containing the selected assets, and
		/// adds the sub-menu to the passed-in context menu.
		/// </summary>
		public static void AppendSourceControlSubmenu( ContextMenu ContextMenu, MenuItem SourceControlPrototype, Control ReceivesBindings, ExecutedRoutedEventHandler Execute, CanExecuteRoutedEventHandler InCanExecute )
		{
			// package menu items
			MenuItem SourceControlMenuItem = new MenuItem();
			SourceControlMenuItem.Header = UnrealEd.Utils.Localize( "ContentBrowser_PackageList_MenuItem_SourceControl" );
			{
				if ( SourceControlMenuItem != null )
				{
					foreach ( Object subObj in SourceControlPrototype.Items )
					{
						if ( subObj is Separator )
						{
							SourceControlMenuItem.Items.Add( new Separator() );
						}
						else
						{
							MenuItem subItem = subObj as MenuItem;
							if ( subItem != null )
							{
								RoutedCommand rCmd = subItem.Command as RoutedCommand;
								if ( rCmd == null || rCmd.OwnerType == typeof(SourceControlCommands) )
								{
									MenuItem subItemCopy = new MenuItem();
									subItemCopy.Command = subItem.Command;
									subItemCopy.Header = subItem.Header;

									SourceControlMenuItem.Items.Add( subItemCopy );
									if ( subItem.Command != null )
									{
										CommandBinding binding = new CommandBinding(
											subItemCopy.Command,
											Execute,
											InCanExecute
											);
										ReceivesBindings.CommandBindings.Add( binding );
									}
								}
							}
						}
					}
				}
			}
			ContextMenu.Items.Add( SourceControlMenuItem );
		}

		/// <summary>
		/// Removes any CommandBindings that were dynamically added for a programatically generated context menu item
		/// </summary>
		private void ClearPackageCommandBindings()
		{
			for ( int BindingIndex = CommandBindings.Count - 1; BindingIndex >= 0; BindingIndex-- )
			{
				CommandBinding binding = CommandBindings[BindingIndex];
				
				RoutedCommand cmd = binding.Command as RoutedCommand;
				if ( cmd != null )
				{
					CommandBindings.RemoveAt(BindingIndex);
				}
			}
		}

		/// <summary>
		/// CanExecuteRoutedEventHandler for the asset view's package context menu items
		/// </summary>
		private void CanExecuteAssetPackageCommand( object Sender, CanExecuteRoutedEventArgs EventArgs )
		{
			mMainControl.Backend.CanExecuteAssetCommand(Sender, EventArgs);
		}

		/// <summary>
		/// ExecutedRoutedEventHandler for the asset view's package context menu items
		/// </summary>
		private void ExecuteAssetPackageCommand( object Sender, ExecutedRoutedEventArgs EventArgs )
		{
			mMainControl.Backend.ExecuteAssetCommand(Sender, EventArgs);
		}

        /// <summary>
        /// CanExecuteRoutedEventHandler for the ContentBrowser's custom commands
        /// </summary>
        /// <param name="Sender"></param>
        /// <param name="EvtArgs"></param>
        private void CanExecuteMenuCommand(object Sender, CanExecuteRoutedEventArgs EvtArgs)
        {
            if (mMainControl != null)
            {
                mMainControl.Backend.CanExecuteMenuCommand(Sender, EvtArgs);
            }
        }

        /// <summary>
        /// ExecutedRoutedEventHandler for the ContentBrowser's custom commands
        /// </summary>
        /// <param name="Sender"></param>
        /// <param name="EvtArgs"></param>
        private void ExecuteMenuCommand(object Sender, ExecutedRoutedEventArgs EvtArgs)
        {
            if (mMainControl != null)
            {
                mMainControl.Backend.ExecuteMenuCommand(Sender, EvtArgs);
            }
        }

		#endregion



		/// <summary>
		/// Returns the list item at the specified position or null if one wasn't found
		/// </summary>
		public object FindListBoxItemAtPosition( ListBox InListBox, Point InPoint )
		{
			UIElement Element = (UIElement)InListBox.InputHitTest(InPoint);
			if ( Element != null )
			{
				while ( Element != InListBox )
				{
					Object Item = InListBox.ItemContainerGenerator.ItemFromContainer(Element);
					if ( !(Item.Equals(DependencyProperty.UnsetValue)) )
					{
						// Found it!
						return Item;
					}

					Element = VisualTreeHelper.GetParent(Element) as UIElement;
					if ( Element == null )
					{
						return null;
					}
				}
			}

			return null;
		}

		/// <summary>
		/// Wrapper method for finding an AssetItem for a specific UObject
		/// </summary>
		/// <param name="AssetPathName">an Unreal object full name for the asset to find</param>
		/// <returns>a reference to the AssetItem associated with the pathname</returns>
		public AssetItem FindAssetItem( String AssetFullName )
		{
			AssetItem Result = null;

			if ( Model != null )
			{
				int Index = Model.FindAssetIndex( AssetFullName );
				Result = ( Index >= 0 ) ? this.Model[Index] : null;
			}

			return Result;
		}

		/// <summary>
		/// Wrapper method for finding the index of an AssetItem for a specific UObject
		/// </summary>
		/// <param name="AssetPathName">an Unreal object full name for the asset to find</param>
		/// <returns>The index of the item within the model</returns>
		public int FindAssetIndex( String AssetFullName )
		{
			int Result = -1;

			if ( Model != null )
			{
				Result = Model.FindAssetIndex( AssetFullName );
			}

			return Result;
		}


		/// <summary>
		/// Saves important history data so the user can restore it later.  Called when taking a history snapshot of the content browser
		/// </summary>
		/// <param name="HistoryData">The history data object that will be saved</param>
		/// <param name="bFullSave">If true we should save all important history data. If false, only save data that should be updated during forward and back calls</param>
		public void SaveHistoryData( ContentBrowserHistoryData HistoryData, bool bFullSave )
		{
			// Store the scroll bar position of the asset view
			HistoryData.AssetViewScrollPos = this.AssetCanvas.m_AssetCanvasScrollViewer.VerticalOffset;
			// Store a list of selected asset names
			HistoryData.SelectedAssets = new List<String>( SelectedAssets.Count );
			foreach( AssetItem SelectedAsset in SelectedAssets )
			{
				HistoryData.SelectedAssets.Add( SelectedAsset.FullName );
			}
		}

		/// <summary>
		/// Restores important history data.  Called when restoring a history snapshot requested by a user
		/// </summary>
		/// <param name="HistoryData">The history data object containing data to restore</param>
		public void RestoreHistoryData( ContentBrowserHistoryData HistoryData )
		{
			if( HistoryData.SelectedAssets.Count > 0 )
			{
				// Start an asset selection of assets that were selected at the history snapshot being restored
				StartDeferredAssetSelection( HistoryData.SelectedAssets );
			}
		
			// Scroll the asset view
			this.AssetCanvas.m_AssetCanvasScrollViewer.ScrollToVerticalOffset(HistoryData.AssetViewScrollPos);
		}
    }

}
