//=============================================================================
//	MainControl.xaml.cs: Content browser control
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
using System.Windows.Media.Animation;
using System.Collections;
using System.Diagnostics;

using UnrealEd;


namespace ContentBrowser
{

	/// <summary>
	/// Indicate whether we should refresh immediately or delay
	/// @todo where to put this? ContentBrowserDefs?
	/// </summary>
	[FlagsAttribute]
	public enum RefreshFlags : int
	{
		None = 0, Default = 0,
		Deferred = 1 << 0,
		ScrollToTop = 1 << 1,
		KeepSelection = 1 << 2,
	}

	/// Syntax sugar for RefreshType
	static class RefreshTypeExtensionMethods
	{
		public static bool IsDeferred(this RefreshFlags InFlags) { return (InFlags & RefreshFlags.Deferred) != RefreshFlags.None; }
		public static bool IsImmediate(this RefreshFlags InFlags) { return (InFlags & RefreshFlags.Deferred) == RefreshFlags.None; }
		public static bool ShouldScrollToTop(this RefreshFlags InFlags) { return (InFlags & RefreshFlags.ScrollToTop) != RefreshFlags.None; }
		public static bool ShouldKeepSelection(this RefreshFlags InFlags) { return (InFlags & RefreshFlags.KeepSelection) != RefreshFlags.None; }

	}

	public static class ContentBrowserDefs
	{
		/// Any GAD operation that touches more than this number of assets will produce a warning prompt.
		public const int MaxNumAssetsForNoWarnGadOperation = 40;

		/// Number of "custom labels" to display in tool tips
		public const int MaxAssetListCustomLabels = 4;

		/// Number of "custom data" columns to display in the asset list view
		public const int MaxAssetListCustomDataColumns = 10;
	}

	/// <summary>
	/// The history data object, storing all important history data
	/// </summary>
	public class ContentBrowserHistoryData
	{
		/// History Description
		public String HistoryDesc { get; private set; }

		///////////// FilterPanel Data /////////////

		/// The search string typed by the user
		public String SearchString { get; set; }

		/// A list of filter tags selected.  Organized as a list of tag tiers and a list of selected options in each tier
		public List<List<String>> SelectedFilterOptions { get; set; }
		/// A list of actual filter options regardless of their selected state.  
		public List<List<FilterOption>> FilterOptions { get; set; }
		/// A list of selected object types
		public List<String> SelectedObjectTypes { get; set; }
		/// Tagged, Untagged, Both option
		public int TaggedFilterOption { get; set; }
		/// Loaded, Unloaded, Both option
		public int LoadedFilterOption { get; set; }
		/// If true the user has the favorites object type list tab selected, otherwise the all types tab is selected
		public bool FavoritesViewTabChecked { get; set; }

		/// Search field options
		public bool SearchFieldNameChecked { get; set; }
		public bool SearchFieldPathChecked { get; set; }
		public bool SearchFieldTagsChecked { get; set; }
		public bool SearchFieldTypeChecked { get; set; }

		// Match all/Match any state
		public int MatchFilter { get; set; }

		///////////// Sources Panel Data /////////////

		/// SourcesPanel (lists are mutually exclusive)
		public List<String> SelectedPackageTreeItems { get; set; }
		public List<String> SelectedSharedCollections { get; set; }
		public List<String> SelectedPrivateCollections { get; set; }

		///////////// Asset View Data /////////////

		/// All selected assets
		public List<String> SelectedAssets { get; set; }

		/// Scroll bar position of the asset view
		public double AssetViewScrollPos;

		public ContentBrowserHistoryData(String HistoryDesc)
		{
			this.HistoryDesc = HistoryDesc;
		}
	}

	/// The class responsible for managing all content browser history
	public class ContentBrowserHistoryManager
	{
		/// A list of history snapshots
		private List<ContentBrowserHistoryData> HistoryData;

		/// The current history index the user is at (changes when the user goes back,forward, or history snapshots are taken)
		public int CurrentHistoryIndex { get; private set; }

		/// Max number of history items that can be stored.  Once the max is reached, the oldest history item is removed
		public const int MaxHistoryEntries = 30;

		/// The command name for history context menu command bindings (we store this so it we can remove command bindings later)
		public const String RoutedCommandName = "HistoryMenuItemCommand";


		public ContentBrowserHistoryManager()
		{
			HistoryData = new List<ContentBrowserHistoryData>();
			CurrentHistoryIndex = 0;
		}

		/// <summary>
		/// Goes back one history snapshot and returns the history data at that snapshot
		/// </summary>
		public ContentBrowserHistoryData GoBack()
		{
			ContentBrowserHistoryData ReturnData = null;

			if (HistoryData.Count > 0 && CurrentHistoryIndex > 0)
			{
				// if its possible to go back, decrement the index we are at
				--CurrentHistoryIndex;
				// return the data at the new index
				ReturnData = HistoryData[CurrentHistoryIndex];
			}

			return ReturnData;
		}

		/// <summary>
		/// Goes forward one history snapshot and returns the history data at that snapshot
		/// </summary>
		public ContentBrowserHistoryData GoForward()
		{
			ContentBrowserHistoryData ReturnData = null;
			if (HistoryData.Count > 0 && CurrentHistoryIndex < HistoryData.Count - 1)
			{
				// if its possible to go forward, incremeent the index we are at
				++CurrentHistoryIndex;
				// return the data at the new index
				ReturnData = HistoryData[CurrentHistoryIndex];
			}

			return ReturnData;
		}

		/// <summary>
		/// Jumps to a specific history index (called when a user selects a history snapshot from a context menu)
		/// </summary>
		/// <param name="HistoryIndex">The history index to jump to</param>>
		public ContentBrowserHistoryData JumpToHistory(int HistoryIndex)
		{
			ContentBrowserHistoryData DataToReturn = null;
			if (HistoryIndex >= 0 && HistoryIndex < HistoryData.Count)
			{
				// if the history index is valid, set the current history index to the history index requested by the user
				CurrentHistoryIndex = HistoryIndex;
				// return the data at the new index
				DataToReturn = HistoryData[HistoryIndex];
			}

			return DataToReturn;
		}

		/// <summary>
		/// Stores new history data.  Called when creating a history snapshot
		/// </summary>
		/// <param name="InHistory"></param>
		public void AddHistoryData(ContentBrowserHistoryData InHistory)
		{
			if (HistoryData.Count == 0)
			{
				// History added to the beginning
				HistoryData.Add(InHistory);
				CurrentHistoryIndex = 0;
			}
			else if (CurrentHistoryIndex == HistoryData.Count - 1)
			{
				// History added to the end
				if (HistoryData.Count == MaxHistoryEntries)
				{
					// If max history entries has been reached
					// remove the oldest history
					HistoryData.RemoveAt(0);
				}
				HistoryData.Add(InHistory);
				// Current history index is the last index in the list
				CurrentHistoryIndex = HistoryData.Count - 1;
			}
			else if (CurrentHistoryIndex != HistoryData.Count - 1)
			{
				// History added to the middle
				// clear out all history after the current history index.
				HistoryData.RemoveRange(CurrentHistoryIndex + 1, HistoryData.Count - (CurrentHistoryIndex + 1));
				HistoryData.Add(InHistory);
				// Current history index is the last index in the list
				CurrentHistoryIndex = HistoryData.Count - 1;
			}
		}

		/// <summary>
		/// Determines if a user can go forward in history
		/// </summary>
		/// <returns>true if the user can navigate forward</returns>
		public bool CanGoForward()
		{
			bool CanGoForward = false;
			if (HistoryData.Count > 0 && CurrentHistoryIndex < HistoryData.Count - 1)
			{
				// User can go forward if there are items in the history data list, 
				// and the current history index isnt the last index in the list
				CanGoForward = true;
			}

			return CanGoForward;
		}

		/// <summary>
		/// Determines if a user can go back in history
		/// </summary>
		/// <returns>true if the user can navigate backward</returns>
		public bool CanGoBack()
		{
			bool CanGoBack = false;
			if (HistoryData.Count > 0 && CurrentHistoryIndex > 0)
			{
				// User can go back if there are items in the history data list,
				// and the current history index isn't the first index in the list
				CanGoBack = true;
			}

			return CanGoBack;
		}

		/// <summary>
		/// Retrieves the history data at the current history index
		/// </summary>
		/// <returns>History data at the current history index</returns>
		public ContentBrowserHistoryData GetCurrentHistory()
		{
			if (HistoryData.Count == 0)
			{
				// there is no history data
				return null;
			}
			else
			{
				return HistoryData[CurrentHistoryIndex];
			}
		}

		/// <summary>
		/// Populates a list of menu items that can be added to a context menu
		/// to allow a user to jump to different history snapshots instead of using the back and forward buttons
		/// </summary>
		/// <param name="bGetPrior">
		/// If true gets history snapshots prior to the current history index(for navigating back).  
		/// If false ges history snapshots after the current history index (for navigating forward).
		/// Note: This method always returns the current history index
		/// </param>
		/// <returns>A List of menu items the user should be able to select from a context menu</returns>
		public List<MenuItem> GetAvailableHistoryMenuItems(bool bGetPrior)
		{
			List<MenuItem> HistoryMenuItems = null;

			if (HistoryData.Count > 1)
			{
				// if there is at least 2 history items...

				// Start index is the first snapshot we should make a menu item out of
				int StartIndex = 0;
				// EndIndex is the last snapshot we should make a menu item out of
				int EndIndex = CurrentHistoryIndex;

				if (!bGetPrior)
				{
					// Need to return only items on or after the current history index
					StartIndex = CurrentHistoryIndex;
					EndIndex = HistoryData.Count - 1;
				}

				// Check to make sure the start and end indices are within the bounds of the history list
				if (StartIndex < HistoryData.Count && EndIndex != -1)
				{

					HistoryMenuItems = new List<MenuItem>();

					// Get all menu items between and including the start index and end index
					for (int HistoryIdx = StartIndex; HistoryIdx <= EndIndex; ++HistoryIdx)
					{
						MenuItem Item = new MenuItem();
						String HistoryDesc = HistoryData[HistoryIdx].HistoryDesc;
						// The menu item header is the description of this history snapshot
						Item.Header = HistoryDesc;
						// Setup a command for this menu item
						Item.Command = new RoutedUICommand(HistoryDesc, RoutedCommandName, typeof(ContentBrowserHistoryManager));
						// Store the history index so we can use it later to jump to this history if the user selects this menu item
						Item.CommandParameter = HistoryIdx;
						// The current history item in bold to let the user know this snapshot is what they are currently at.
						Item.FontWeight = HistoryIdx == CurrentHistoryIndex ? FontWeights.Bold : FontWeights.Normal;

						HistoryMenuItems.Add(Item);
					}
				}
			}

			return HistoryMenuItems;

		}


	}
	/// <summary>
	/// Content Browser main control
	/// </summary>
	public partial class MainControl : UserControl
	{
		/// Interface exposing core engine/editor functions to C# code
		public IContentBrowserBackendInterface Backend
		{
			get
			{
				return m_Backend;
			}
		}

		private IContentBrowserBackendInterface m_Backend;

		static MainControl()
		{
			// Override the default fonts for most controls.
			// @todo cb [reviewed; discuss]: these settings will be overriden by changing the theme while the application is running.
			TextElement.FontFamilyProperty.OverrideMetadata(typeof(TextElement), new FrameworkPropertyMetadata(new FontFamily("Tahoma")));
			TextElement.FontSizeProperty.OverrideMetadata(typeof(TextElement), new FrameworkPropertyMetadata(11.0));

			TextBlock.FontFamilyProperty.OverrideMetadata(typeof(TextBlock), new FrameworkPropertyMetadata(new FontFamily("Tahoma")));
			TextBlock.FontSizeProperty.OverrideMetadata(typeof(TextBlock), new FrameworkPropertyMetadata(11.0));
		}

		/// Construct a Content Browser
		public MainControl()
		{
			bIsLoaded = false;
			this.DataContext = this;

			InitializeComponent();

			// Register handlers for search & filter changes
			FilterPanel.FilterChanged += new FilterPanel.FilterChangedEventHandler(FilterChanged);

			// Register handler for user clicking on the (x) button to dismiss the warning panel
			mDismissWarningButton.Click += new RoutedEventHandler(mDismissWarningButton_Click);

			// Register handlers for unloading
			this.Unloaded += new RoutedEventHandler(MainControl_Unloaded);

			// Grab references to storyboards
			mProgressBarFadeIn = (Storyboard)this.Resources["ProgressBarFadeIn"];
			mProgressBarFadeOut = (Storyboard)this.Resources["ProgressBarFadeOut"];
			mPlayNotification = (Storyboard)this.Resources["ShowNotification"];
			mShowQuickNotification = (Storyboard)this.Resources["ShowQuickNotification"];
			mShowHideWarning = (Storyboard)this.Resources["ShowHideWarning"];
			mHideWarning = (Storyboard)this.Resources["HideWarning"];

			this.CommandBindings.Add(new CommandBinding(AssetViewCommands.Refresh, RefreshCommandHandler, CanRefreshCommandHandler));
			this.CommandBindings.Add(new CommandBinding(AssetViewCommands.FullRefresh, FullRefreshCommandHandler, CanRefreshCommandHandler));
			this.CommandBindings.Add(new CommandBinding(AssetViewCommands.SetSourceToAllAssets, SetSourceToAllAssetsCommandHandler, CanSetSourceToAllAssetsCommandHandler));


			// Register the Loaded even handler.
			this.Loaded += new RoutedEventHandler(MainControl_Loaded);

		}

		/// In Quarantine mode users are able to see quarantined assets.
		/// Users can also quarantine assets or bring them out of quarantine.
		///
		/// When not in Quarantine mode, users do not see quarantined assets.
		public bool IsInQuarantineMode()
		{
			return FilterPanel.IsInQuarantineMode();
		}

		/// Exited or entered quarantine mode.
		public void OnQuarantineMode_Changed()
		{
			mCatchAllToolbar.Background = (IsInQuarantineMode()) ? Brushes.DarkRed : (SolidColorBrush)Application.Current.Resources["Slate_Panel_Background"];
			mQuarantineModeLabel.Visibility = (IsInQuarantineMode()) ? Visibility.Visible : Visibility.Hidden;
			RefreshAssetView(RefreshFlags.KeepSelection);
		}

		/// Called when asset population begins.
		public void OnAssetPopulationStarted(bool IsQuickUpdate)
		{
			// Start the progress bar.
			mProgressBarFadeIn.Begin(mProgress);

			AssetView.OnAssetPopulationStarted(IsQuickUpdate);
		}

		/// Called when asset population is complete.
		public void OnAssetPopulationComplete(bool IsQuickUpdate)
		{
			this.SetAssetUpdateProgress(100.0);
			AssetView.OnAssetPopulationComplete(IsQuickUpdate);
		}



		/// Called when the control is loaded.
		void MainControl_Loaded(object sender, RoutedEventArgs e)
		{
			// We want to restore and update after the controls are loaded.
			bIsLoaded = true;

			// Restore the layout
			RestoreUIState();

			// Update the asset view immediately.
			UpdateAssetsInView(RefreshFlags.Default); // there is no selection to keep; pass in false.

			// Snapshot a base history so the user can go back to the default state
			TakeHistorySnapshot("Content Browser Loaded");
		}

		/// Storyboard fades in progress bar
		private Storyboard mProgressBarFadeIn;
		/// Storyboard fades out the progress bar
		private Storyboard mProgressBarFadeOut;
		/// Storyboard shows a notification and then hides it
		private Storyboard mPlayNotification;
		/// Storyboard shows a notification and then hides it; faster than regular show notification
		private Storyboard mShowQuickNotification;
		/// Storyboard shows and hides the warning panel
		private Storyboard mShowHideWarning;
		/// Storyboard hides the warning panel
		private Storyboard mHideWarning;


		private bool m_IsGADReadOnly;
		/// True when the Asset Database is running in read-only mode; no tagging is allowed.
		public bool IsGADReadOnly { get { return m_IsGADReadOnly; } }

		public bool bIsLoaded
		{
			get;
			private set;
		}


		/// Access the close button
		public Button CloseButton { get { return m_CloseButton; } }

		/// Access the clone button
		public Button CloneButton { get { return m_CloneButton; } }

		/// Access the float/dock button
		public Button FloatOrDockButton { get { return m_FloatOrDockButton; } }

		/// Access to the history back button
		public Button BackButton { get { return m_BackButton; } }

		/// Access to the history forward button
		public Button ForwardButton { get { return m_ForwardButton; } }

		/// A global tool tip control displayed when the user hovers over an asset item in either the
		/// asset list or asset canvas
		public AssetItemToolTip m_GlobalAssetItemToolTip;



		/// <summary>
		/// Play a notification with the specified text, but faster than PlayNotification
		/// </summary>
		/// <param name="NotificationText">Text the appears in the notification</param>
		public void PlayQuickNotification(String NotificationText)
		{
			PlayQuickNotification(NotificationText, null, 20);
		}

		/// <summary>
		/// Play a notification with the specified text, but faster than PlayNotification
		/// </summary>
		/// <param name="NotificationText">Text the appears in the notification</param>
		/// <param name="Font">The font to use when showing the notification; null for default.</param>
		/// <param name="FontSize">The font size</param>
		public void PlayQuickNotification(String NotificationText, FontFamily Font, double FontSize)
		{
			mNotificationLabel.Text = NotificationText;
			mShowQuickNotification.Begin(mNotification);
			this.mNotificationLabel.FontSize = FontSize;
			if (Font == null)
			{
				this.mNotificationLabel.ClearValue(TextBlock.FontFamilyProperty);
			}
			else
			{
				this.mNotificationLabel.FontFamily = Font;
			}

		}


		/// <summary>
		/// Play a notification with the specified text
		/// </summary>
		/// <param name="NotificationText">Text the appears in the notification</param>
		public void PlayNotification(String NotificationText)
		{
			this.mNotificationLabel.ClearValue(TextBlock.FontFamilyProperty);
			this.mNotificationLabel.FontSize = 20;
			mNotificationLabel.Text = NotificationText;
			mPlayNotification.Begin(mNotification);
		}

		/// <summary>
		/// Show the user a non-modal, non-intrusive warning
		/// </summary>
		/// <param name="WarningText">Text the appears in the warning</param>
		public void PlayWarning(String WarningText)
		{
			mWarningLabel.Text = WarningText;
			mShowHideWarning.Begin(mWarningPanel);
		}

		static private readonly TimeSpan WarningPanelHideAnimationOffset = TimeSpan.FromSeconds(10);
		/// Called when the user click on the (x) button to hide the current warning.
		void mDismissWarningButton_Click(object sender, RoutedEventArgs e)
		{
			mHideWarning.Begin(mWarningPanel);
		}


		/// <summary>
		/// Update the percentage of AssetUpdate task that has been completed
		/// </summary>
		/// <param name="PercentComplete">Percent of progress completed.</param>
		public void SetAssetUpdateProgress(double PercentComplete)
		{
			if (PercentComplete >= 100)
			{
				// We have completed the task
				mProgressBarFadeOut.Begin(mProgressPanel);
			}

			mProgress.Value = PercentComplete;
		}


		/// <summary>
		/// Handle the ContentBrowser shutting down
		/// </summary>
		/// <param name="sender">ignored</param>
		/// <param name="e">ignored</param>
		private void MainControl_Unloaded(object sender, RoutedEventArgs e)
		{
			SaveUIState();

			// Clean up our global tooltip control if we need to
			if (m_GlobalAssetItemToolTip != null)
			{
				TopLevelGrid.Children.Remove(m_GlobalAssetItemToolTip);
				m_GlobalAssetItemToolTip = null;
			}

			bIsLoaded = false;
		}

		/// Handle the user changing the search/filter
		private void FilterChanged(RefreshFlags InRefreshFlags)
		{
			this.RequestFilterRefresh(InRefreshFlags | RefreshFlags.ScrollToTop);
		}

		/// Request the asset filter (defined by the filter panel) to be reapplied.
		/// <param name="InRefreshType">Refresh immediately (next tick) or after a delay.</param>
		public void RequestFilterRefresh(RefreshFlags InRefreshFlags)
		{
			this.AssetView.RequestFilterRefresh(mFilter, InRefreshFlags);
		}

		/// Updates the Assets currently being viewed based on the filter.
		/// <param name="ShouldKeepSelection">Pass in true if the currently selected assets should be re-selected. False to clear the selection.</param>
		public void UpdateAssetsInView(RefreshFlags InRefreshFlags)
		{

			List<String> AssetsToSelect;
			if (InRefreshFlags.ShouldKeepSelection())
			{
				AssetsToSelect = AssetView.CloneSelectedAssetFullNames();
			}
			else
			{
				AssetsToSelect = new List<String>(0);
			}

			// Begin the update
			Backend.UpdateAssetsInView();
			if (InRefreshFlags.ShouldScrollToTop())
			{
				AssetView.ScrollToTop();
			}

			AssetView.StartDeferredAssetSelection(AssetsToSelect);
		}

		/// <summary>
		/// Get the info about the UI state; used for saving/restoring the UI.
		/// </summary>
		public ContentBrowserUIState ContentBrowserUIState { get { return mContentBrowserUIState; } }
		private ContentBrowserUIState mContentBrowserUIState = new ContentBrowserUIState();


		/// Load the UI state from configs and apply it to the ContentBrowser
		private void RestoreUIState()
		{
			Backend.LoadContentBrowserUIState(mContentBrowserUIState);
			mContentBrowserUIState.EnsureSanity();

			// Left Panel
			mLeftPanel.Width = ContentBrowserUIState.LeftPanelWidth;
			mLeftPanelCollapseTrigger.IsChecked = !ContentBrowserUIState.IsLeftPanelCollapsed;


			// Right Panel
			mRightPanelCollapseTrigger.IsChecked = !ContentBrowserUIState.IsRightPanelCollapsed;
			mRightPanel.Width = ContentBrowserUIState.RightPanelWidth;

			AssetView.RestoreContentBrowserUIState(mContentBrowserUIState);
			m_FilterPanel.RestoreContentBrowserUIState();
			MySourcesPanel.RestoreContentBrowserUIState();
		}

		/// Save the UI state to configs.
		private void SaveUIState()
		{
			// Left Panel
			ContentBrowserUIState.IsLeftPanelCollapsed = mLeftPanel.IsCollapsed;
			if (!Double.IsNaN(mLeftPanel.Width))
			{
				ContentBrowserUIState.LeftPanelWidth = mLeftPanel.Width;
			}

			MySourcesPanel.SaveContentBrowserUIState(mContentBrowserUIState);


			// Right Panel
			if (!Double.IsNaN(mRightPanel.Width))
			{
				ContentBrowserUIState.RightPanelWidth = mRightPanel.Width;
			}
			ContentBrowserUIState.IsRightPanelCollapsed = mRightPanel.IsCollapsed;

			m_FilterPanel.SaveContentBrowserUIState(mContentBrowserUIState);

			AssetView.SaveContentBrowserUIState(mContentBrowserUIState);

			Backend.SaveContentBrowserUIState(mContentBrowserUIState);
		}

		/// <summary>
		/// Initialize the Content Browser. Necessary because WPF widgets have to have default constructors.
		/// </summary>
		/// <param name="InBackend"></param>
		public void Initialize(IContentBrowserBackendInterface InBackend)
		{
			m_Backend = InBackend;
			mFilter = new AssetFilter(this);

			mContentBrowserHistoryManager = new ContentBrowserHistoryManager();

			TagUtils.SetTagDefs(m_Backend.GetTagDefs());

			m_IsGADReadOnly = m_Backend.IsGameAssetDatabaseReadonly();

			// Initialize the sources panel
			m_SourcesPanel.Init(this);

			// Search and filter panels
			m_FilterPanel.Init(this, this.Filter);

			// Initialize the asset view
			m_AssetView.Init(this);
			mAssetInspector.Init(this); //m_AssetInfoPanel.Init(this);


			// Register handler for the changing of the selected source(s)
			m_SourcesPanel.SelectionChanged += new SourcesPanel.SelectionChangedDelegate(m_SourcesPanel_SelectionChanged);

			// Register handler for the changing of the selects asset(s)
			m_AssetView.AssetSelectionChanged += new AssetView.AssetSelectionChangedEventHandler(m_AssetView_AssetSelectionChanged);

			// Mouse movement handler for tooltip positioning
			TopLevelGrid.PreviewMouseMove += new MouseEventHandler(TopLevelGrid_PreviewMouseMove);

			Backend.UpdateTagsCatalogue();

			Backend.UpdateSourcesList(true);
		}

		/// Handle a request to refresh the view.
		private void RefreshCommandHandler(object sender, ExecutedRoutedEventArgs e)
		{
			this.PlayQuickNotification(Utils.Localize("ContentBrowser_Notification_Refreshing"));
			this.AssetView.RequestFilterRefresh(null, RefreshFlags.Default);
			e.Handled = true;
		}

		/// Handle a request to refresh the view.
		private void CanRefreshCommandHandler(object sender, CanExecuteRoutedEventArgs e)
		{
			e.CanExecute = true;
			e.Handled = true;
		}

		/// Handle a request to refresh the view.
		private void FullRefreshCommandHandler(object sender, ExecutedRoutedEventArgs e)
		{
			RefreshAssetView(RefreshFlags.KeepSelection); // We want to keep the selection during this refresh
			this.Backend.UpdateSourcesList(true);
			e.Handled = true;
		}

		/// Refresh the asset view (also notify the user that we're refreshing)
		private void RefreshAssetView( RefreshFlags InRefreshFlags )
		{
			this.PlayQuickNotification(Utils.Localize("ContentBrowser_Notification_Refreshing"));
			this.UpdateAssetsInView(InRefreshFlags); 
		}

		/// Can we set the source to AllAssets?
		private void CanSetSourceToAllAssetsCommandHandler(object sender, CanExecuteRoutedEventArgs e)
		{
			e.CanExecute = true;
			e.Handled = true;
		}

		/// Handle a request to set the source to All Assets.
		private void SetSourceToAllAssetsCommandHandler(object sender, ExecutedRoutedEventArgs e)
		{
			this.MySourcesPanel.SetSourceToAllAssets();
			e.Handled = true;
		}



		/// Called when the mouse is moved anywhere in the content browser
		void TopLevelGrid_PreviewMouseMove(object sender, MouseEventArgs e)
		{
			// Mouse has moved, so update the tooltip
			UpdateAssetItemToolTipPosition(e);
		}



		/// Called to start drawing a tooltip for the specified asset item
		public void StartAssetItemToolTip(AssetItem InAssetItem)
		{
			if (m_GlobalAssetItemToolTip == null)
			{
				// Create a new tool tip
				m_GlobalAssetItemToolTip = new AssetItemToolTip();

				// Attach the tooltip control to the content browser's top level grid
				TopLevelGrid.Children.Add(m_GlobalAssetItemToolTip);
			}


			// Tell the tooltip to bind to the specified asset
			m_GlobalAssetItemToolTip.BindToAssetItem(this, InAssetItem);
		}



		/// Called to stop drawing the tooltip for an asset
		public void StopAssetItemToolTip()
		{
			// Unbind the tooltip (this will trigger a fade out)
			m_GlobalAssetItemToolTip.BindToAssetItem(this, null);
		}



		/// Updates the position of the asset item tool tip
		private void UpdateAssetItemToolTipPosition(System.Windows.Input.MouseEventArgs e)
		{
			if (m_GlobalAssetItemToolTip != null)
			{
				Point GridSpaceMousePosition = e.GetPosition(TopLevelGrid);
				Point ToolTipPosition = new Point(GridSpaceMousePosition.X + 16, GridSpaceMousePosition.Y + 16);


				// Adjust tooltip position for the edges of the browser (WPF content can't display outside
				// of the root control)

				/*
				// @todo CB: Can't compute tooltip dimensions for some reason (they're never correct!)

				double ToolTipWidth = m_ToolTip.ActualWidth;
				double ToolTipHeight = m_ToolTip.ActualHeight;
				
	
				if( ToolTipPosition.X + ToolTipWidth >= m_ContentBrowser.TopLevelGrid.ActualWidth )
				{
					// We're out of space for the tooltip near the right of the content browser, so
					// we'll display it to the left of the mouse cursor instead of to the right
					ToolTipPosition.X -= ToolTipWidth;
				}

				if( ToolTipPosition.Y + ToolTipHeight >= m_ContentBrowser.TopLevelGrid.ActualHeight )
				{
					// We're out of space for the tooltip near the bottom of the content browser, so
					// we'll display it above the mouse cursor instead of under it
					ToolTipPosition.Y -= ToolTipHeight;
				}
				 */



				// We use the top left margin on the grid as the tooltip's position.  Note that we set the
				// opposite margin to a negative value so that the tooltip's layout isn't clamped by the
				// bounds of the content browser
				m_GlobalAssetItemToolTip.Margin = new Thickness(
					ToolTipPosition.X, ToolTipPosition.Y,
					-400, -400);
			}
		}


		/// <summary>
		/// Sets the search text to the specified string, updates the user interface and kicks off the search
		/// </summary>
		/// <param name="InNewSearchText">The text string to search for</param>
		public void StartSearchByName(String InNewSearchText)
		{
			// Set the search text
			m_FilterPanel.SearchByName(InNewSearchText);
		}


		/// User defined content filter; set by the SearchPanel and FilterPanel.
		private AssetFilter mFilter;

		private ContentBrowserHistoryManager mContentBrowserHistoryManager;

		/// Whether or not we are restoring history data
		public bool bRestoringHistory { get; private set; }

		/// Whether or not taking history snapshots are allowed
		public bool bHistorySnapshotsAllowed { get; private set; }

		/// Handle selection changing in Asset View (user selects/deselects some assets)
		private void m_AssetView_AssetSelectionChanged(object sender, AssetView.AssetSelectionChangedEventArgs args)
		{
			//m_AssetInfoPanel.AssetsUnderInspection = m_AssetView.SelectedAssets;
			mAssetInspector.AssetsUnderInspection = m_AssetView.SelectedAssets;
			UpdateAssetCount();
		}

		/// Handle change in source selection (user selects package, collection, etc.); stuff in the Left Panel.
		private void m_SourcesPanel_SelectionChanged()
		{
			UpdateAssetsInView(RefreshFlags.Default); // Do not keep the selection since the source was changed.
			TakeHistorySnapshot("Package Selection Changed");
		}

		/// Update the label specifying how many assets and present and how many are selected.
		public void UpdateAssetCount()
		{
			mItemCountLabel.Text = Utils.Localize("ContentBrowser_ItemCount", AssetView.PostFilterCount, AssetView.SelectedCount);
			mAssetViewProgressStats.Text = Utils.Localize("ContentBrowser_ProgressAssetCount", MyAssets.Count);
		}

		/// Get the Filter. The filter represents user's query for assets.
		public AssetFilter Filter { get { return mFilter; } }

		/// Get the FilterPanel. Filter Panel allows user to modify the Filter.
		public FilterPanel FilterPanel { get { return m_FilterPanel; } }

		/// Get the Model for the Sources Panel
		public SourcesPanelModel MySources { get { return m_SourcesPanel.Model; } }

		/// Get the SourcesPanel. SourcesPanel allows user to select the source of Assets to filter.
		public SourcesPanel MySourcesPanel { get { return m_SourcesPanel; } }

		///	The view model for Assets shown to user.
		public AssetViewModel MyAssets { get { return m_AssetView.Model; } }

		/// List of assets displayed to the user
		public AssetView AssetView { get { return m_AssetView; } }


		/// Set the list of tags known to the ContentBrowser
		public void SetTagsCatalog(List<String> InDictionary)
		{
			NameSet GroupNames = new NameSet();
			// Discover group names from list of tags.
			foreach (String FullTagName in InDictionary)
			{
				String GroupName = TagUtils.GetGroupNameFromFullName(FullTagName);
				if (GroupName != String.Empty)
				{
					GroupNames.Add(GroupName);
				}
			}


			this.FilterPanel.SetTagsCatalog(InDictionary, GroupNames);
			this.mAssetInspector.SetTagsCatalog(InDictionary, GroupNames);
		}

		#region CommandBindings

		/// <summary>
		/// CanExecuteRoutedEventHandler for the ContentBrowser's custom commands
		/// </summary>
		/// <param name="Sender"></param>
		/// <param name="EvtArgs"></param>
		private void CanExecuteMenuCommand(object Sender, CanExecuteRoutedEventArgs EvtArgs)
		{
			Backend.CanExecuteMenuCommand(Sender, EvtArgs);
		}

		/// <summary>
		/// ExecutedRoutedEventHandler for the ContentBrowser's custom commands
		/// </summary>
		/// <param name="Sender"></param>
		/// <param name="EvtArgs"></param>
		private void ExecuteMenuCommand(object Sender, ExecutedRoutedEventArgs EvtArgs)
		{
			Backend.ExecuteMenuCommand(Sender, EvtArgs);
		}

		#endregion

		#region History

		/// Alerts the asset view that there is a pending filter history snapshot
		public void RequestFilterChangedHistorySnapshot(String PendingFilterSnapshotDesc)
		{
			if (!bRestoringHistory && bIsLoaded)
			{
				// Only allow pending snapshots that dont occur before the main control is loaded and we arent restoring history.
				AssetView.bPendingFilterHistorySnapshot = true;
				AssetView.PendingFilterSnapshotDesc = PendingFilterSnapshotDesc;
			}
		}

		/// Updates the enabled/disabled state of the forward and back history buttons
		private void UpdateHistoryButtons()
		{
			if (mContentBrowserHistoryManager.CanGoBack())
			{
				// Back button should be enabled if we have history data the user can go back to
				BackButton.IsEnabled = true;
			}
			else
			{
				BackButton.IsEnabled = false;
			}

			if (mContentBrowserHistoryManager.CanGoForward())
			{
				// Forward button should be enabled if we have history data the user can go back to
				ForwardButton.IsEnabled = true;
			}
			else
			{
				ForwardButton.IsEnabled = false;
			}
		}

		/// Called when the back button context menu is opening
		private void OnBackHistoryCMOpening(object sender, RoutedEventArgs e)
		{
			// Remove all menu items from the context menu as history data may have changed
			mHistoryBackContextMenu.Items.Clear();

			// Create a list of menu items from the history snapshots we can go to by moving backward
			List<MenuItem> MenuItems = mContentBrowserHistoryManager.GetAvailableHistoryMenuItems(true);

			// Add the menu items to the context menu in reverse so ones closest to the current history index are at the top of the menu
			// Implements a Firefox/Chrome style context menu that appears when you right click on a forward or back button
			for (int ItemIdx = MenuItems.Count - 1; ItemIdx >= 0; --ItemIdx)
			{
				MenuItem Item = MenuItems[ItemIdx];
				mHistoryBackContextMenu.Items.Add(Item);
				this.CommandBindings.Add(new CommandBinding(Item.Command, HistoryMenuItemExecuted));
			}
		}

		/// Called when the back button context menu is closing
		private void OnBackHistoryCMClosing(object sender, RoutedEventArgs e)
		{
			// Remove history command bindings created for the context menu that just closed
			RemoveHistoryCommandBindings();
		}

		/// Called when the forward button context menu is opening
		private void OnForwardHistoryCMOpening(object sender, RoutedEventArgs e)
		{
			// Remove all menu items from the context menu as history data may have changed
			mHistoryForwardContextMenu.Items.Clear();

			// Create a list of menu items from the history snapshots we can go to by moving forward
			List<MenuItem> MenuItems = mContentBrowserHistoryManager.GetAvailableHistoryMenuItems(false);

			// Add the menu items to the context menu in reverse so ones closest to the current history index are at the bottom of the menu
			// Implements a Firefox/Chrome style context menu that appears when you right click on a forward or back button
			for (int ItemIdx = MenuItems.Count - 1; ItemIdx >= 0; --ItemIdx)
			{
				MenuItem Item = MenuItems[ItemIdx];
				mHistoryForwardContextMenu.Items.Add(Item);
				this.CommandBindings.Add(new CommandBinding(Item.Command, HistoryMenuItemExecuted));
			}
		}

		/// Called when the forward button context menu closes
		private void OnForwardHistoryCMClosing(object sender, RoutedEventArgs e)
		{
			// Remove history command bindings created for the context menu that just closed
			RemoveHistoryCommandBindings();
		}

		/// Removes old history command bindings
		private void RemoveHistoryCommandBindings()
		{
			for (int BindingIdx = 0; BindingIdx < CommandBindings.Count; ++BindingIdx)
			{
				CommandBinding Binding = CommandBindings[BindingIdx];
				RoutedCommand cmd = Binding.Command as RoutedCommand;

				// Only remove command bindings created by the content browser history manager
				if (cmd != null && cmd.Name == ContentBrowserHistoryManager.RoutedCommandName)
				{
					CommandBindings.RemoveAt(BindingIdx);
				}
			}
		}

		/// Called when a user selects a history snapshot from a context menu
		private void HistoryMenuItemExecuted(object sender, ExecutedRoutedEventArgs e)
		{
			// Get the history index from the event args
			int HistoryIdx = (int)e.Parameter;

			// Only jump to a new history if we arent already at the jump location.
			if (HistoryIdx != mContentBrowserHistoryManager.CurrentHistoryIndex)
			{
				// Update current history before we jump
				UpdateCurrentHistory();

				// Jump to the history data selected by the user
				ContentBrowserHistoryData HistoryData = mContentBrowserHistoryManager.JumpToHistory(HistoryIdx);

				// Update the state of the forward and back buttons as we have changed our position in the history list
				UpdateHistoryButtons();

				// Restore the old history data
				RestoreHistory(HistoryData);
			}
		}

		/// Called when the back history button is called on the content browser
		private void OnBackButtonClicked(object sender, RoutedEventArgs e)
		{
			// Go backwards one history point
			HistoryGoBack();
		}

		/// Called when the forward history button is called on the content browser
		private void OnForwardButtonClicked(object sender, RoutedEventArgs e)
		{
			// Go forward one history point
			HistoryGoForward();
		}

		/// Go backward one history point if possible and restore data.
		public void HistoryGoBack()
		{
			// only go backward if its possible(I.E there are history items to go back to)
			if (mContentBrowserHistoryManager.CanGoBack())
			{
				// Update the current history item before we go back
				UpdateCurrentHistory();

				// Move backwards, getting the history data that should be restored
				ContentBrowserHistoryData HistoryData = mContentBrowserHistoryManager.GoBack();

				// Update the state of the forward and back buttons as we have changed our position in the history list
				UpdateHistoryButtons();

				// Restore old history data
				RestoreHistory(HistoryData);
			}
		}

		/// Go forward one history point if possible and restore data.
		public void HistoryGoForward()
		{
			// Only go forward if its possible (I.E there are history items to go forward to)
			if (mContentBrowserHistoryManager.CanGoForward())
			{
				// Update the current history item before we go forward
				UpdateCurrentHistory();

				// Move forwards, getting the history data that should be restored
				ContentBrowserHistoryData HistoryData = mContentBrowserHistoryManager.GoForward();

				// Update the state of the forward and back buttons as we have changed our position in the history list
				UpdateHistoryButtons();

				// Restore old history data
				RestoreHistory(HistoryData);
			}
		}

		/// <summary>
		/// Enables or disables the ability to take history snapshots
		/// </summary>
		/// <param name="bAllow">If true, snapshots should be allowed to be taken, if false taking history snapshots will be disabled</param>
		public void AllowHistorySnapshots(bool bAllow)
		{
			if (bAllow && !bRestoringHistory)
			{
				// Only allow history snapshots to be taken if bAllow is true and history is not being restored
				bHistorySnapshotsAllowed = true;
			}
			else
			{
				bHistorySnapshotsAllowed = false;
			}
		}

		/// <summary>
		/// Takes a history snapshot of the content browser. Saving all important data so it can be restored by a user later
		/// </summary>
		/// <param name="HistoryDesc">
		/// A string describing what happened to cause a history snapshot. 
		/// This allows a user to select the string from a menu, allowing them to "jump" to different history states
		/// </param>
		public void TakeHistorySnapshot(String HistoryDesc)
		{
			// Only allow snapshots to be taken if the main control is loaded, we arent restoring history 
			//and we aren't completely clearing the search filter (history snapshot is taken seperatley for that)
			if (bIsLoaded && !bRestoringHistory && bHistorySnapshotsAllowed)
			{
				// No more history snapshots should be allowed until next tick.
				bHistorySnapshotsAllowed = false;

				// Update the current history before we make a new snapshot
				UpdateCurrentHistory();

				// Create a new history data object
				ContentBrowserHistoryData HistoryData = new ContentBrowserHistoryData(HistoryDesc);

				// Save history data on each of the important panels
				FilterPanel.SaveHistoryData(HistoryData, true);
				MySourcesPanel.SaveHistoryData(HistoryData, true);
				AssetView.SaveHistoryData(HistoryData, true);

				// Add the snapshotted history data.
				mContentBrowserHistoryManager.AddHistoryData(HistoryData);
				//System.Diagnostics.Debug.WriteLine("History Snapshot Taken " + mContentBrowserHistoryManager.CurrentHistoryIndex);

				// Update the forward and back buttons since history data changed
				UpdateHistoryButtons();

			}
		}

		/// <summary>
		/// Restores the state of the content browser panels from a different time
		/// </summary>
		/// <param name="DataToRestore">The history data object containing all the data we should restore</param>
		public void RestoreHistory(ContentBrowserHistoryData DataToRestore)
		{
			// Set the restoring history flag to true.  This prevents history snapshots from being taken 
			// when we are restoring history. History snapshots happen when events called on different controls
			// when selected state changes so we must prevent those events from causing snapshots during a restore.
			bRestoringHistory = true;

			// Restore the sources panel state
			MySourcesPanel.RestoreHistoryData(DataToRestore);

			// Restore the filter panel state
			FilterPanel.RestoreHistoryData(DataToRestore);

			// Update all assets in view based on data we restored
			UpdateAssetsInView(RefreshFlags.Default);

			// Restore asset view state.  We do this after refreshing assets in view
			// so we can properly select the correct assets.
			AssetView.RestoreHistoryData(DataToRestore);

			// Done restoring history. Allow new snapshots to be taken.
			bRestoringHistory = false;

		}

		/// <summary>
		/// Updates the current history object without creating a new snapshot.  
		/// This is useful for storing the state of parts of the content browser that are too granular to cause a history snapshot
		/// but should still be saved when navigating back and forth.
		/// </summary>
		public void UpdateCurrentHistory()
		{
			// Only update current history if we arent already restoring history
			if (!bRestoringHistory)
			{
				// Get the current history object from the history manager
				ContentBrowserHistoryData CurrentHistory = mContentBrowserHistoryManager.GetCurrentHistory();
				if (CurrentHistory != null)
				{
					// Save history data from the sources panel
					MySourcesPanel.SaveHistoryData(CurrentHistory, false);
					// Save history data from the filter panel
					FilterPanel.SaveHistoryData(CurrentHistory, false);
					// Save history data from the asset view
					AssetView.SaveHistoryData(CurrentHistory, false);
				}
			}
		}

		#endregion

		#region RecentItems
		/// A list of the recently accessed assets
		public static List<String> RecentAssets { get; private set; }

		/// The max size of the recent asset list
		public static int MaxNumberRecentItems { get; private set; }

		/// <summary>
		/// Creates a static list to store recent assets
		/// </summary>
		/// <param name="MaxNumberOfItems">The maximum number of recent assets allowed</param>
		public void InitRecentItems(int MaxNumberOfItems)
		{
			MaxNumberRecentItems = Math.Max(0, MaxNumberOfItems);
			RecentAssets = new List<String>(MaxNumberRecentItems);
		}

		/// <summary>
		/// Adds an item to the list of recent items
		/// </summary>
		/// <param name="RecentAssetPath">The full name of the asset</param>
		public void AddRecentItem(String RecentAssetPath)
		{
			int CurrentItemIndex = RecentAssets.LastIndexOf(RecentAssetPath);
			if (CurrentItemIndex >= 0)
			{// Item already exists.  Remove it so that it will only be at the top of the stack when added again.
				RecentAssets.Remove(RecentAssetPath);
			}

			// Don't allow more than the max number of items in the array
			if (MaxNumberRecentItems > 0)
			{
				if (RecentAssets.Count >= MaxNumberRecentItems)
				{
					int NumToRemove = (RecentAssets.Count - MaxNumberRecentItems) + 1;
					RecentAssets.RemoveRange(0, NumToRemove);
				}

				RecentAssets.Add(RecentAssetPath);
			}
			else
			{
				RecentAssets.Clear();
			}

		}

		/// <summary>
		/// Is the object in the array of recent objects
		/// </summary>
		/// <param name="ObjectPath">The string path of the object</param>
		/// <returns>If the object was recently accessed</returns>
		public bool IsObjectInRecents(String ObjectPath)
		{
			return RecentAssets.Contains(ObjectPath);
		}
		#endregion
	}



	/// Flags for optimizing which elements of an asset are refreshed
	[FlagsAttribute]
	public enum AssetStatusUpdateFlags : int
	{
		/// No flag
		None = 0,

		/// Loaded status
		LoadedStatus = 1 << 0,

		/// Update tags
		Tags = 1 << 1,

		/// Update quarantined status
		Quarantined = 1 << 2,

		/// ...

		/// All elements
		All = LoadedStatus | Tags | Quarantined
	}



}
