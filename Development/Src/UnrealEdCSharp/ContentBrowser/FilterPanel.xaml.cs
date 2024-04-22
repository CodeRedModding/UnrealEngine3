//=============================================================================
//	FilterPanel.xaml.cs: Content browser asset filtering panel
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
using System.Collections.ObjectModel;
using CustomControls;
using System.Globalization;
using System.Diagnostics;
using UnrealEd;


namespace ContentBrowser
{
	/// <summary>
	/// Filter panel. This panel contains filters that are similar to iTunes (not as fancy).
	/// </summary>
	public partial class FilterPanel : UserControl
	{
		/// A list of artist-friendly object types known to the system
		private List<String> BrowserObjectTypes { get; set; }

		/// An array of references to FilterTierVisuals
		private List<TagFilterTier> mFilterTiers = new List<TagFilterTier>();

		/// A reference to the TagFilterTier that was last modified.
		private TagFilterTier mTierLastModified = null;

		/// List of known types for autocomplete
		private NameSet mAutocomplete_KnownTypes = new NameSet();
		/// List of known tags for autocomplete
		private List<String> mAutocomplete_KnownTags = new List<String>();

		/// A reference to the filter model. We modify the filter model to reflect the filtering changes that the user performs in the UI.
		private AssetFilter m_Filter;

		/// A reference to the MainControl of the ContentBrowser
		private MainControl m_MainControl;

		
		/// Construct a FilterPanel
		public FilterPanel()
		{
			InitializeComponent();

			DataContext = m_Filter;

			// Add all the filter tiers to a list for easier managerment
			mFilterTiers.Add( mTagFilterTier0 );
			mFilterTiers.Add( mTagFilterTier1 );
			mFilterTiers.Add( mTagFilterTier2 );

			// - - METADATA FILTER PANEL - -																		    
			// Register handler for selection changes in ObjectType List
			mObjectTypeFilterTier.SelectionChanged += new SelectionChangedHandler(m_ObjectTypeFilterListView_SelectionChanged);
			mObjectTypeFilterTier.ActiveListChanged += new TypeFilterTier.ActiveListChangedHandler( mObjectTypeFilterTier_ActiveListChanged );
			// Register handler for selection changes in Tags Lists
			foreach (TagFilterTier Tier in mFilterTiers)
			{
				Tier.SelectionChanged += new SelectionChangedHandler( m_TagFilterListView_SelectionChanged );
			}

			// Register handler for user clicking on recents checkbox
			mShowRecentOnly.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mShowRecentOnly.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);
			mShowFlattenedTextures.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mShowFlattenedTextures.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);

			// Register handlers for user clicking on in use filter radio buttons
			mInUseCurrentLevel.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseCurrentLevel.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseLoadedLevels.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseLoadedLevels.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseVisibleLevels.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseVisibleLevels.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseOff.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mInUseOff.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);

			// Register handler for user clicking on tagged/untagged radio buttons
			mShowTaggedAndUntagged.Checked += new RoutedEventHandler( StatusFilterOption_Checked );
			mShowTaggedOnly.Checked += new RoutedEventHandler( StatusFilterOption_Checked );
			mShowUntaggedOnly.Checked += new RoutedEventHandler( StatusFilterOption_Checked );

			// Register handler for user clicking on loaded/unloaded radio buttons
			mShowLoadedAndUnloaded.Checked += new RoutedEventHandler( StatusFilterOption_Checked );
			mShowLoadedOnly.Checked += new RoutedEventHandler( StatusFilterOption_Checked );
			mShowUnloadedOnly.Checked += new RoutedEventHandler( StatusFilterOption_Checked );

			// Register handler for clicking on Quarantined
			mShowQuarantinedOnly.Checked += new RoutedEventHandler(StatusFilterOption_Checked);
			mShowQuarantinedOnly.Unchecked += new RoutedEventHandler(StatusFilterOption_Checked);

			
			// - - Text Filter - -
			// Register handler for user changing the token requirements ("with all" vs. "with any")			
			mAllAnySwitch.SelectionChanged += new SelectionChangedEventHandler( mAllAnySwitch_SelectionChanged );

			// Register handler for user clicking on the FilterFields button (opens the faux combo)
			mShowSearchFieldsButton.Click += new RoutedEventHandler( mShowSearchFieldsButton_Click );
			// Register handler for filter field popup closing
			mSearchFieldsList.Closed += new EventHandler( mSearchFieldsList_Closed );

			// Register handler for user changing which fields should be searched for tokens
			mShouldSearchField_Tags.Checked += new RoutedEventHandler( mShouldSearchFields_OptionChanged ); mShouldSearchField_Tags.Unchecked += new RoutedEventHandler( mShouldSearchFields_OptionChanged );
			mShouldSearchField_Type.Checked += new RoutedEventHandler( mShouldSearchFields_OptionChanged ); mShouldSearchField_Type.Unchecked += new RoutedEventHandler( mShouldSearchFields_OptionChanged );
			mShouldSearchField_Path.Checked += new RoutedEventHandler( mShouldSearchFields_OptionChanged ); mShouldSearchField_Path.Unchecked += new RoutedEventHandler( mShouldSearchFields_OptionChanged );
			mShouldSearchField_Name.Checked += new RoutedEventHandler( mShouldSearchFields_OptionChanged ); mShouldSearchField_Name.Unchecked += new RoutedEventHandler( mShouldSearchFields_OptionChanged );

			// Register handler for user typing into the SearchByName textbox
			mSearchTextBox.TextChanged += new TextChangedEventHandler( mSearchTextBox_TextChanged );

			// Configure the autocomplete
			// prevent space bar for accepting an autocomplete suggestion
			mSearchFieldAutocomplete.AcceptAndWriteThroughKeylist = new List<Key>();
			// handle scope depth events to keep track of scope level on the autocomplete ctrl
			mSearchFieldAutocomplete.ScopeDepthChanged += mSearchFieldAutocomplete_ScopeDepthChanged;
			// tell it that the names of assets are scoped with '.' chars
			mSearchFieldAutocomplete.ScopeDelimiters = new List<char>(new [] { '.' });
			// prevent default word delimiters from working here
			mSearchFieldAutocomplete.WordDelimiters = new List<char>();

			// Handle user clicking ClearFilter button
			mClearFilterButton.Click += new RoutedEventHandler( mClearFilterButton_Click );

			// Handle the user resizing the expanding panel
			mExpandingPanel.SizeChanged += new SizeChangedEventHandler( mExpandingPanel_SizeChanged );

			// Register handler for Favorites changing so we can save them.
			mObjectTypeFilterTier.FavoritesChanged += new TypeFilterTier.FavoritesChangedHandler( mObjectTypeFilterTier_FavoritesChanged );


			// NOTE: Not REALLY part of the filter panel.
			// Register handlers for entering and exiting quarantine mode.
			mQuarantineMode.Checked += mQuarantineMode_Changed;
			mQuarantineMode.Unchecked += mQuarantineMode_Changed;
		}

		/// <summary>
		/// Handles event raised on the search field Autocomplete when its scope depth changes
		/// Used to alter the scope level of suggestions presented by the dictionary for autocompletion
		/// </summary>
		void mSearchFieldAutocomplete_ScopeDepthChanged(object sender, RoutedEventArgs args)
		{
			// simply call UpdateAutocompleteCatalog to allow the dictionary contents to be changed
			// to suggestions at the current scope level
			UpdateAutocompleteCatalog();
		}

		/// <summary>
		/// Initialize the FilterPanel
		/// </summary>
		/// <param name="MainControlIn">The MainControl in which this FilterPanel is being instantiated</param>
		/// <param name="FilterIn">The AssetFilter Model that we are visualizing</param>
		public void Init( MainControl MainControlIn, AssetFilter FilterIn )
		{
			m_MainControl = MainControlIn;
			
			// Init Asset Type names (UClass names)
			mAutocomplete_KnownTypes = m_MainControl.Backend.GetAssetTypeNames();
			//mAutocomplete_KnownTypes.Sort();

			// Init Browser Type names (artist-friendly meta type)
			BrowserObjectTypes = m_MainControl.Backend.GetObjectTypeFilterList();
			BrowserObjectTypes.Sort();

			m_Filter = FilterIn;
			m_Filter.KnownTypes = new List<String>(mAutocomplete_KnownTypes);

			// Init the "All Types" tab of the Type Filter Tier
			mObjectTypeFilterTier.SetFilterOptions( BrowserObjectTypes.ConvertAll<FilterOption>( TypeName => new FilterOption{ OptionText=TypeName } ) );

			// Load the favorites.
			this.mObjectTypeFilterTier.SetFavorites( m_MainControl.Backend.LoadTypeFilterFavorites().ConvertAll<FilterOption>( Favorite => new FilterOption { OptionText=Favorite } ) );
			List<String> Favorites = m_MainControl.Backend.LoadTypeFilterFavorites();
			RefreshTagTiers();

			this.ClearFilter();
			UpdateFilterSwitches();
			UpdateAutocompleteCatalog();
		}

		/// Exited or entered quarantine mode.
		void mQuarantineMode_Changed(object sender, RoutedEventArgs e)
		{			
			if ( !IsInQuarantineMode() )
			{
				mShowQuarantinedOnly.IsChecked = false;
				mShowQuarantinedOnly.Visibility = Visibility.Collapsed;
			}
			else
			{
				mShowQuarantinedOnly.Visibility = Visibility.Visible;
			}			

			m_MainControl.OnQuarantineMode_Changed();
		}

		/// In Quarantine mode users are able to see quarantined assets.
		/// Users can also quarantine assets or bring them out of quarantine.
		///
		/// When not in Quarantine mode, users do not see quarantined assets.
		public bool IsInQuarantineMode()
		{
			return mQuarantineMode.IsChecked ?? false;
		}

		/// Updates the Associated AssetFilter's TagTiers form UI
		public void UpdateFilterModelTagTiers()
		{
			UpdateTagTiersModel();
		}

		/// Called when the Favorites change in the Type Filter.
		void mObjectTypeFilterTier_FavoritesChanged()
		{
			// Save the Favorites.
			List<String> Favorites = mObjectTypeFilterTier.FavoriteTypes.ConvertAll<String>( CurFilterOption => CurFilterOption.OptionText );		
			m_MainControl.Backend.SaveTypeFilterFavorites( Favorites );
		}

		/// Called when the Types filter switches between "Types" and "Favorites".
		void mObjectTypeFilterTier_ActiveListChanged()
		{
			FilterTierChanged();
		}

		/// Called when user clicks the ClearFilter button
		void mClearFilterButton_Click( object sender, RoutedEventArgs e )
		{
			this.ClearFilter();

            // Take a history snapshot after the filter has been cleared
			m_MainControl.TakeHistorySnapshot( "Filter Cleared" );

		}

		/// Called when the user resizes the expanding panel
		void mExpandingPanel_SizeChanged( object sender, SizeChangedEventArgs e )
		{
			// Record any changes in size if the size is reasonable (i.e. a real number, nonzero )
			if ( !Double.IsNaN(mExpandingPanel.Height) && mExpandingPanel.Height != 0 )
			{
				mExpandingPanelHeight = this.mExpandingPanel.Height;
			}
		}

		/// Parameter to GenerateFilterOptions.
		enum IncludeAllTags
		{
			Yes,
			No
		}

		/// <summary>
		/// Given a set of tags, generate the tag options that would help filter down this list.
		/// Only tags that are present on the assets should appear. Each tag should know the number
		/// of assets that are tagged with it.
		/// </summary>
		/// <param name="InAssetItems"> Assets from which to build filter options </param>
		/// <param name="ShowAllTags"> If Yes, all tags will be added. Otherwise only tags with non-zero count are added.</param>
		/// <returns> A list of filter options generated from the InAssetItems. </returns>
		private List<FilterOption> GenerateFilterOptions(ICollection<AssetItem> InAssetItems, IncludeAllTags ShowAllTags)
		{
			// A union of tags found on the given Assets
			HashSet<String> TagsOnAssets = (ShowAllTags == IncludeAllTags.Yes) ? new HashSet<String>(mAutocomplete_KnownTags) : TagUtils.GatherTagsFromAssets(InAssetItems);

			// Make a map(tagname->assetcount) that helps us count assets under each tag.
			Dictionary<String, int> TagToAssetCount = new Dictionary<string, int>( InAssetItems.Count );
			foreach (String TagOption in TagsOnAssets)
			{
				TagToAssetCount.Add(TagOption, 0);
			}

			// Count how many assets are under each tag.
			foreach ( AssetItem Asset in InAssetItems )
			{
				foreach ( String TagOnAsset in Asset.Tags )
				{
					TagToAssetCount[TagOnAsset] = TagToAssetCount[TagOnAsset] + 1;
				}
			}

			// Build the list of filter options to show to the user.
			List<FilterOption> FilterOptions = new List<FilterOption>( TagToAssetCount.Count );
			foreach ( String TagOption in TagToAssetCount.Keys )
			{
				FilterOptions.Add( new TagFilterOption( new Tag( TagOption ) ) { AssetCount = TagToAssetCount[TagOption] } );
			}

			return FilterOptions;
		}


		/// Refresh the TagFilterVisuals such that the selection in tier N dictates the tags available in tier N+1.
		/// a.k.a Multi-tier tag filtering.
		public void RefreshTagTiers()
		{
			const int NumTagTiers = 3;

			// NOTE: Any changes made to the UI here should NOT require an update event because
			// the TagTiers' tags are built from the list of assets that is returned from the filter!
			// The one exception occurs when the source changes, causing selected tags to become deselected
			// This is worked around by updating the AssetFilter from UI every time a filter refresh is triggered.

			try
			{
				// First, disable any tag columns that are unused; this should fire the proper events.
				DisabledUnusedTagColumns();

				// Nothing we do here should trigger the asset filter refresh; we're just updating the visual on the filter panel.
				mDisableFilterEvent = true;

				// Get an index of the last tier that was modifier; we only want to update tiers after this one.
				int LastTierModifiedIndex = mFilterTiers.IndexOf( mTierLastModified );

				if ( m_Filter.IsNullFilter() )
				{
					// There is no filter; the first column should show the union of all tags on the assets visible in the AssetView.
					mFilterTiers[0].SetFilterOptions( GenerateFilterOptions( m_MainControl.MyAssets, IncludeAllTags.Yes ) );
					mFilterTiers[1].ClearTier();
					mFilterTiers[2].ClearTier();
				}
				else
				{
					if ( m_Filter.GetTagTierCount() >= NumTagTiers )
					{
						// Update Tier 0 only if an earlier part of the tag filter was modified.
						if (LastTierModifiedIndex < 0)
						{
							List<FilterOption> Tier0Tags = GenerateFilterOptions( m_Filter.GetInputToTier(0), IncludeAllTags.Yes );
							mTagFilterTier0.SetFilterOptions(Tier0Tags);
						}
						
						// Get tags selected in Tier0; we want to subtract them from tries 1 and 2.
						List<String> Tier0SelectedTags = mTagFilterTier0.GetSelectedOptions();
						
						// Update Tier1 if Tier0 or earlier was modifier.
						if ( LastTierModifiedIndex < 1 )
						{
							if ( !mTagFilterTier0.IsFilterInactive )
							{
								// Tier1 should show the union of tags on assets that passed Tier0 ...
								List<FilterOption> Tier1Tags = new List<FilterOption>( GenerateFilterOptions( m_Filter.GetInputToTier( 1 ), IncludeAllTags.No ) );
								// ... minus any tags selected in Tier0 as that would be redundant.
								Tier1Tags.RemoveAll( TagCandidate => Tier0SelectedTags.Contains( TagCandidate.OptionText ) );
								mTagFilterTier1.SetFilterOptions( Tier1Tags );
							}
							else
							{
								mTagFilterTier1.ClearTier();
							}

						}

						// Same logic as Tier1 (see above)
						if ( LastTierModifiedIndex < 2 )
						{
							if ( !mTagFilterTier1.IsFilterInactive )
							{
								List<FilterOption> Tier2Tags = new List<FilterOption>(GenerateFilterOptions(m_Filter.GetInputToTier(2), IncludeAllTags.No));
								List<String> Tier1SelectedTags = mTagFilterTier1.GetSelectedOptions();
								// remove any tags selected in Tier0 or Tier1
								Tier2Tags.RemoveAll( TagCandidate => ( Tier1SelectedTags.Contains( TagCandidate.OptionText ) || Tier0SelectedTags.Contains( TagCandidate.OptionText ) ) );
								mTagFilterTier2.SetFilterOptions( Tier2Tags );
							}
							else
							{
								mTagFilterTier2.ClearTier();
							}
						}
					}
				}
				
			}
			finally
			{
				// We refreshed the tiers, so no tier is modified.
				mTierLastModified = null;
				// We should re-enable filter events; the filter can be triggered outside this method.
				mDisableFilterEvent = false;

				UpdateFilterActiveAppearance();
			}
		}


		// Remember the height of the expanding panel so that we can save the panel's height even when it is collapsed.
		double mExpandingPanelHeight = 0;

        /// Restore the UI state from the ContentBrowserUIState
        public void RestoreContentBrowserUIState()
		{
            ContentBrowserUIState MyContentBrowserUIState = m_MainControl.ContentBrowserUIState;

            this.mShowExpandedButton.IsChecked = !MyContentBrowserUIState.IsFilterPanelCollapsed;
            mExpandingPanelHeight = MyContentBrowserUIState.FilterPanelHeight;
			this.mExpandingPanel.Height = mExpandingPanelHeight;
            this.mSearchTextBox.Text = MyContentBrowserUIState.FilterPanelSearchString;
		}

		/// <summary>
        /// Save the current UI state into a ContentBrowserUIState object for serialization
		/// </summary>
        /// <param name="OutContentBrowserUIState">ContentBrowserUIState object which should be updated with the current UI state parameters</param>
        public void SaveContentBrowserUIState(ContentBrowserUIState OutContentBrowserUIState)
		{
            OutContentBrowserUIState.IsFilterPanelCollapsed = !(this.mShowExpandedButton.IsChecked ?? true);
            OutContentBrowserUIState.FilterPanelHeight = this.mExpandingPanelHeight;
            OutContentBrowserUIState.FilterPanelSearchString = this.mSearchTextBox.Text;
		}

		/// Highlight the active parts of the filter panel in a bright orange.
		private void UpdateFilterActiveAppearance()
		{
			// Update the search highlight
			bool IsSearchActive = m_Filter.SearchString.Length > 0;
			mSearchBorder.BorderStyle = ( IsSearchActive ) ? SlateBorder.BorderStyleType.ActiveFilterHeader : SlateBorder.BorderStyleType.FilterHeader;

			// Note: The tag lists and ObjectType list update update themselves: see TagFilterTier


			// Highlight the loaded/unloaded tagged/untagged column
			bool bStatusFilterActive = (	m_Filter.LoadedFilterOption != AssetFilter.LoadedState.LoadedAndUnloaded ||
											m_Filter.TaggedFilterOption != AssetFilter.TaggedState.TaggedAndUntagged ||
											m_Filter.ShowRecentItemsOnly ||
											m_Filter.InUseFilterEnabled ||
											m_Filter.ShowFlattenedTextures ||
											m_Filter.ShowOnlyQuarantined);
			mStatusFilterBorder.BorderStyle = (bStatusFilterActive) ? SlateBorder.BorderStyleType.ActiveFilterHeader : SlateBorder.BorderStyleType.FilterHeader;


			// Turn off the highlights; re-enable as needed.
			this.mExpanderButtonBackground.BorderStyle = SlateBorder.BorderStyleType.FilterHeader;
			this.mClearButtonBackground.BorderStyle = SlateBorder.BorderStyleType.FilterHeader;
			this.mClearFilterButton.IsEnabled = false;			

			// Highlight the expander button if anything in the panel is active; the clear button also highlights and becomes active
			if (!this.mObjectTypeFilterTier.IsFilterInactive ||
				!this.mTagFilterTier0.IsFilterInactive ||
				!this.mTagFilterTier1.IsFilterInactive ||
				!this.mTagFilterTier2.IsFilterInactive ||
				bStatusFilterActive )
			{
				this.mExpanderButtonBackground.BorderStyle = SlateBorder.BorderStyleType.ActiveFilterHeader;
				this.mClearButtonBackground.BorderStyle = SlateBorder.BorderStyleType.ActiveFilterHeader;
				this.mClearFilterButton.IsEnabled = true;
			}

			// If the search is active the filter button should be highlighted and active.
			if (IsSearchActive)
			{
				this.mClearButtonBackground.BorderStyle = SlateBorder.BorderStyleType.ActiveFilterHeader;
				this.mClearFilterButton.IsEnabled = true;
			}
		}


		// Handle switching of the tagged state filter option
		void StatusFilterOption_Checked( object sender, RoutedEventArgs e )
		{
			UpdateFilterSwitches();

			// Update the active state appearance
			UpdateFilterActiveAppearance();

			RaiseFilterChangedEvent( RefreshFlags.Default );
			
			// Take a history snapshot after the status filter has changed
			m_MainControl.TakeHistorySnapshot( "Status Filter Changed" );
		}

		#region FauxCombo Handling
		/// Called when the user clicks on the FilterFields button
		void mShowSearchFieldsButton_Click( object sender, RoutedEventArgs e )
		{
			this.mSearchFieldsList.IsOpen = true;
			// Disable the button so that clicking on it again won't re-open the faux combo.
			mShowSearchFieldsButton.IsEnabled = false;
		}

		bool AllSearchFieldsUnchecked()
		{
			return
				!(mShouldSearchField_Tags.IsChecked ?? false) &&
				!(mShouldSearchField_Type.IsChecked ?? false) &&
				!(mShouldSearchField_Path.IsChecked ?? false) &&
				!(mShouldSearchField_Name.IsChecked ?? false);

		}

		/// Called when the FilterFields popup is closed
		void mSearchFieldsList_Closed( object sender, EventArgs e )
		{
			// Ensure that at least one checkbox is selected
			if ( AllSearchFieldsUnchecked() )
			{
				mShouldSearchField_Name.IsChecked = true;
			}

			// Re-enable the button so the user can open the combo again.
			mShowSearchFieldsButton.IsEnabled = true;
		}
		#endregion

		/// Called when the user changes which fields we should search for.
		void mShouldSearchFields_OptionChanged( object sender, RoutedEventArgs e )
		{
			UpdateFilterSwitches();
			UpdateAutocompleteCatalog();

			RaiseFilterChangedEvent( RefreshFlags.Default );

            if ( !AllSearchFieldsUnchecked() )
            {
                // Skip taking the history snapshot if all search fields are unchecked
                // since at least one checkbox has to be checked and another event will be fired to check it
                m_MainControl.TakeHistorySnapshot("Search Options Changed");
            }
		}

		/// Called when the user changes the "with all/with any" combo box.
		void mAllAnySwitch_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{
			UpdateFilterSwitches();
			RaiseFilterChangedEvent( RefreshFlags.Default );
		}


		/// Synchronize the switches and toggles in asset filter to the filter panel.
		void UpdateFilterSwitches()
		{
			if( m_Filter != null )
			{
				// Recent only or all
				m_Filter.ShowRecentItemsOnly = (mShowRecentOnly.IsChecked ?? false);
				m_Filter.ShowFlattenedTextures = (mShowFlattenedTextures.IsChecked ?? false);

				m_Filter.ShowCurrentLevelInUse = (mInUseCurrentLevel.IsChecked ?? false);
				m_Filter.ShowLoadedLevelsInUse = (mInUseLoadedLevels.IsChecked ?? false);
				m_Filter.ShowVisibleLevelsInUse = (mInUseVisibleLevels.IsChecked ?? false);
				m_Filter.InUseOff = (mInUseOff.IsChecked ?? false);

				// Tagged/Untagged/Both
				if( mShowUntaggedOnly.IsChecked ?? false )
				{
					m_Filter.TaggedFilterOption = AssetFilter.TaggedState.UntaggedOnly;

					// Clear the tag filter tiers so that any currently applied tags are disabled
					// before setting to "untagged"
					this.mTagFilterTier0.ClearTier();
				}
				else if( mShowTaggedOnly.IsChecked ?? false )
				{
					m_Filter.TaggedFilterOption = AssetFilter.TaggedState.TaggedOnly;
				}
				else
				{
					m_Filter.TaggedFilterOption = AssetFilter.TaggedState.TaggedAndUntagged;
				}

				//Loaded/Unloaded/Both
				if( mShowUnloadedOnly.IsChecked ?? false )
				{
					m_Filter.LoadedFilterOption = AssetFilter.LoadedState.UnloadedOnly;
				}
				else if( mShowLoadedOnly.IsChecked ?? false )
				{
					m_Filter.LoadedFilterOption = AssetFilter.LoadedState.LoadedOnly;
				}
				else
				{
					m_Filter.LoadedFilterOption = AssetFilter.LoadedState.LoadedAndUnloaded;
				}

				m_Filter.ShowOnlyQuarantined = mShowQuarantinedOnly.IsChecked ?? false;


				// Fields to search
				m_Filter.ShouldSearchField_Tags = this.mShouldSearchField_Tags.IsChecked ?? true;
				m_Filter.ShouldSearchField_Type = this.mShouldSearchField_Type.IsChecked ?? true;
				m_Filter.ShouldSearchField_Path = this.mShouldSearchField_Path.IsChecked ?? true;
				m_Filter.ShouldSearchField_Name = this.mShouldSearchField_Name.IsChecked ?? true;

				// Search Match All / Match Any
				m_Filter.RequireTokens = ( this.mAllAnySwitch.SelectedIndex == 0 ) ? AssetFilter.AllVsAny.All : AssetFilter.AllVsAny.Any;
			}
		}

		/// <summary>
		/// Resets the filtering options to "Show All".
		/// </summary>
		public void ClearFilter()
		{
			bool bClearForBrowserSync = false;
			bool bPreserveObjectType = false;
			ClearFilterInternal( bClearForBrowserSync, bPreserveObjectType );
		}

		/// <summary>
		/// Identical to ClearFilter, but doesn't update "Unloaded/Loaded/Both" filter unless "Unloaded is selected.
		/// </summary>
		public void ClearFilterForBrowserSync()
		{
			bool bClearForBrowserSync = true;
			bool bPreserveObjectType = false;
			ClearFilterInternal( bClearForBrowserSync, bPreserveObjectType );
		}

		/// <summary>
		/// Clears all the filter options for browser sync, except the object type filter option.
		/// </summary>
		public void ClearFilterExceptObjectType()
		{
			bool bClearForBrowserSync = true;
			bool bPreserveObjectType = true;
			ClearFilterInternal( bClearForBrowserSync, bPreserveObjectType );
		}

		/// <summary>
		/// Clear the filter; clear or don't clear the status filter.
		/// </summary>
		/// <param name="ClearLoadedStatusFilter"></param>
		/// <param name="bPreserveObjectType"></param>
		private void ClearFilterInternal( bool bClearForBrowserSync, bool bPreserveObjectType )
		{
			// Don't allow history snapshots to be taken during a filter clear
			// This prevents several history snapshots from being triggered when the user presses the clear button
			m_MainControl.AllowHistorySnapshots(false);

			this.mShowRecentOnly.IsChecked = false;
		
			this.mInUseCurrentLevel.IsChecked = false;
			this.mInUseLoadedLevels.IsChecked = false;
			this.mInUseVisibleLevels.IsChecked = false;
			this.mInUseOff.IsChecked = true;
			
			this.mShowTaggedAndUntagged.IsChecked = true;
			
			if ( !bClearForBrowserSync || ( mShowUnloadedOnly.IsChecked ?? false ) )
			{
				this.mShowLoadedAndUnloaded.IsChecked = true;
			}

			if ( !bPreserveObjectType )
			{
				this.mObjectTypeFilterTier.ClearSelection();
			}

			//toggle mShowFlattenedTextures if we are syncing to one...
			//or hide them (default) if we are clearing browser settings
			if (bClearForBrowserSync && !bPreserveObjectType)
			{
				this.mShowFlattenedTextures.IsChecked = true;
			}
			else if (!bClearForBrowserSync && !bPreserveObjectType)
			{
				this.mShowFlattenedTextures.IsChecked = false;
			}
			//else leave it alone
            
			this.mShowQuarantinedOnly.IsChecked = false;

			this.mTagFilterTier0.ClearSelection();
			this.mTagFilterTier1.ClearSelection();
			this.mTagFilterTier2.ClearSelection();


			// Clear out the tags!
			mSearchTextBox.Clear();

			// Done clearing the filter, allow history snapshots to be taken again
			m_MainControl.AllowHistorySnapshots(true);
		}

		/// Handle the change in selection for the Tags filter list
		private void m_TagFilterListView_SelectionChanged( FilterListView Sender )
		{
			mTierLastModified = mFilterTiers.Find( Tier => Tier.FilterList == Sender );

			if ( !m_MainControl.bRestoringHistory )
			{
				DisabledUnusedTagColumns();
			}

			if ( m_Filter != null && !mDisableFilterEvent )
			{
				UpdateTagTiersModel();
				
				UpdateFilterActiveAppearance();

                // Request a history snapshot be taken after the filter has been updated.
                // We can't directly call TakeHistorySnapshot, as the filter needs to go through a tick
                // to update the tag tiers based on what assets are in view.
                m_MainControl.RequestFilterChangedHistorySnapshot( "Tag Filters Changed" );
			}

			RaiseFilterChangedEvent( RefreshFlags.Default );
		}

		private void UpdateTagTiersModel()
		{
			m_Filter.SetTagTiers( new List<List<String>>() { mTagFilterTier0.GetSelectedOptions(), mTagFilterTier1.GetSelectedOptions(), mTagFilterTier2.GetSelectedOptions() } );
		}

		void DisabledUnusedTagColumns()
		{
			// Only show the filter tier when there are actually options to chose from (i.e. more than one item).
			if ( !mDisableFilterEvent )
			{
				// If the modified tier was set to "All Assets", disable any tiers after the one currently modified.
				for ( int TierIndex = 1; TierIndex < mFilterTiers.Count; ++TierIndex )
				{
					// If the filter tier before me is inactive, disable me!
					if ( mFilterTiers[TierIndex - 1].IsFilterInactive )
					{
						mFilterTiers[TierIndex].ClearTier();
					}
				}
			}
		}

		/// Handle the change in selection for the Tags filter list.
		private void m_ObjectTypeFilterListView_SelectionChanged( FilterListView Sender )
		{
			FilterTierChanged();
		}

		/// Update the AssetFilter and highlight status of various FilterPanel components.
		private void FilterTierChanged()
		{
			if ( m_Filter != null )
			{
				m_Filter.ObjectTypeDescriptions = mObjectTypeFilterTier.GetSelectedOptions();
				UpdateFilterActiveAppearance();
			}

			RaiseFilterChangedEvent( RefreshFlags.Default );

            // Request a history snapshot be taken after the filter has been updated.
            // We can't directly call TakeHistorySnapshot, as the filter needs to go through a tick
            // to update the tag tiers based on what assets are in view
            m_MainControl.RequestFilterChangedHistorySnapshot("Object Type Filter Changed");
		}
		/// <summary>
		/// Sets the list of known tags.
		/// </summary>
		/// <param name="InDictionary">A list of known tags.</param>
		/// <param name="InGroupNames">A list of known tag groups.</param>
		public void SetTagsCatalog( List<String> InDictionary, NameSet InGroupNames )
		{
			List<String> MetadataFilterTagsList = new List<string>(InDictionary);

			// Sort our copy of the incoming names
			MetadataFilterTagsList.Sort();

			// Update autocomplete catalog
			mAutocomplete_KnownTags = new List<String>( MetadataFilterTagsList );
			if ( m_Filter != null )
			{
				m_Filter.KnownTags = mAutocomplete_KnownTags;
			}			
			UpdateAutocompleteCatalog();
		}


		/// <summary>
		/// Update the words known to autocomplete
		/// </summary>
		void UpdateAutocompleteCatalog( )
		{
			List<String> FullAutoCompleteList = new List<String>(0);

			if ( (mShouldSearchField_Tags.IsChecked ?? false) && (mShouldSearchField_Type.IsChecked ?? false) )
			{
				// Both tags and type are checked; show autocomplete for both
				FullAutoCompleteList = new List<string>(mAutocomplete_KnownTags.Count + mAutocomplete_KnownTypes.Count);
				FullAutoCompleteList.AddRange(mAutocomplete_KnownTags);
				FullAutoCompleteList.AddRange(mAutocomplete_KnownTypes);
			}
			else if ( mShouldSearchField_Tags.IsChecked ?? false )
			{
				// Tags is checked; only show the tags Autocomplete
				FullAutoCompleteList = new List<string>(mAutocomplete_KnownTags);
			}
			else if ( mShouldSearchField_Type.IsChecked ?? false )
			{
				// Types is checked; only show the types Autocomplete
				FullAutoCompleteList = new List<string>(mAutocomplete_KnownTypes);
			}

			// Filter FullAutoCompleteList based on scope depth of mSearchFieldAutocomplete
			// Creates a reduced scope list
			List<String> ScopedAutoCompleteList = new List<String>(FullAutoCompleteList.Count);
			string previousScopedItem = string.Empty;
			int scopeDepth = mSearchFieldAutocomplete.ScopeDepth;
			char[] scopeDelimiters = mSearchFieldAutocomplete.ScopeDelimiters.ToArray();
			for (int index = 0; index < FullAutoCompleteList.Count; index++)
			{
				// reduce the scope level of the item to equal the current scopeDepth
				string dictionaryItem = FullAutoCompleteList[index];
				if (!string.IsNullOrEmpty(dictionaryItem))
				{
					string[] scopeLevels = dictionaryItem.Split(scopeDelimiters);
					int scopedItemLength = scopeLevels[0].Length + 1;
					for (int depth = 1; depth <= scopeDepth && depth < scopeLevels.Length; depth++)
					{
						scopedItemLength += scopeLevels[depth].Length + 1;
					}

					string scopedItem = dictionaryItem.Substring(0, scopedItemLength - 1);
					if (0 != string.Compare(previousScopedItem, scopedItem, StringComparison.OrdinalIgnoreCase))
					{
						ScopedAutoCompleteList.Add(scopedItem);
						previousScopedItem = scopedItem;
					}
				}
			}

			mSearchFieldAutocomplete.Dictionary = ScopedAutoCompleteList;
		}


		/// <summary>
		/// Fills the ObjectType in the Filter Panel list with ObjectType Descriptions.
		/// </summary>
		/// <param name="ObjectTypeDescriptions">Collection ObjectType Descriptions with which to populate.</param>
		public void PopulateObjectTypeFilter(ICollection<String> ObjectTypeDescriptions)
		{
			List<FilterOption> Descriptions = new List<FilterOption>( ObjectTypeDescriptions.Count );
			foreach ( String ObjectType in ObjectTypeDescriptions)
			{
				Descriptions.Add( new FilterOption() { OptionText = ObjectType, AssetCount = 0 } );
			}

			// Sort our copy of the incoming names
			Descriptions.Sort( ( ObjectTypeA, ObjectTypeB ) => String.Compare( ObjectTypeA.OptionText, ObjectTypeB.OptionText ) );

			mObjectTypeFilterTier.SetFilterOptions( Descriptions );
		}

		public ContentBrowser.TypeFilterTier ObjectTypeFilterTier
		{
			get { return mObjectTypeFilterTier; }
		}


		#region FilterChanged

		/// <summary>
		/// Handler for FilterChanged Event; triggered when the suggestion box becomes open due to user input (e.g. a user types one of the TriggerChars).
		/// </summary>
		public delegate void FilterChangedEventHandler( RefreshFlags InRefreshFlags );

		/// <summary>
		/// Raised when the suggestion box is triggered by the user.
		/// i.e. The user typed one of the TriggerChars characters or pressed a shortcut
		/// that activated the suggestion box.
		/// </summary>
		public event FilterChangedEventHandler FilterChanged;


		private bool mDisableFilterEvent = false;

		/// <summary>
		/// Raises a FilterChanged Event
		/// <param name="InRefreshType">Whether this change requires an immediate refresh or a deferred one</param>
		/// </summary>
		void RaiseFilterChangedEvent( RefreshFlags InRefreshFlags )
		{
			try
			{
				if ( FilterChanged != null && !mDisableFilterEvent )
				{
					FilterChanged( InRefreshFlags );
				}
			}
			catch (System.Exception e)
			{
				System.Console.WriteLine(e.Message);
			}
		}

		#endregion

		#region Search


		/// <summary>
		/// Sends keyboard focus to the search box and selects the existing text
		/// </summary>
		public void GoToSearchBox()
		{
			Keyboard.Focus( mSearchTextBox );
			mSearchTextBox.SelectAll();
		}


		/// Handle text changing in the mSearchByName textbox
		void mSearchTextBox_TextChanged( object sender, TextChangedEventArgs e )
		{
			m_Filter.SearchString = mSearchTextBox.Text;
			UpdateFilterActiveAppearance();
			RaiseFilterChangedEvent( RefreshFlags.Deferred );
		}

		/// <summary>
		/// Handle the change in the search mode dropdown; dropdown picks between requiring all tags for a match vs. requiring any of the tags for a match.
		/// </summary>
		/// <param name="sender">ignored</param>
		/// <param name="e">ignored</param>
		private void mSearchModeSwitcher_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{
			// Also notify that the Search has changed
			// @todo cb [reviewed; discuss]: re-add deferred refresh
			RaiseFilterChangedEvent( /*( m_Filter.Mode == AssetFilter.SearchMode.SearchByName ) ? RefreshType.Deferred : */RefreshFlags.Default );
		}


		/// <summary>
		/// Sets the search text to the specified string and updates the UI (designed to be called in cases
		/// where we need to set the search text in response to an event outside the content browser.)
		/// </summary>
		/// <param name="InNewSearchText">The name to search for</param>
		public void SearchByName( String InNewSearchText )
		{
			this.ClearFilter();

			// Setup the search text string
			mSearchTextBox.Text = InNewSearchText;		
			
			// Let the browser know that the search mode/string has changed
			RaiseFilterChangedEvent( RefreshFlags.Default );
		}

		#endregion

		#region History

		/// <summary>
		/// Saves important history data so the user can restore it later.  Called when taking a history snapshot of the content browser
		/// </summary>
		/// <param name="HistoryData">The history data object that will be saved</param>
		/// <param name="bFullSave">If true we should save all important history data. If false, only save data that should be updated during forward and back calls</param>
		public void SaveHistoryData( ContentBrowserHistoryData HistoryData, bool bFullSave )
		{
			if( bFullSave )
			{
                // Save all important data from the filter panel.  All this data will be restored when a user navigates to the history snapshot being taken
				HistoryData.SelectedObjectTypes = new List<String>(mObjectTypeFilterTier.GetSelectedOptions());
				HistoryData.SearchString = String.Copy(m_Filter.SearchString);
				HistoryData.TaggedFilterOption = (int)m_Filter.TaggedFilterOption;
				HistoryData.LoadedFilterOption = (int)m_Filter.LoadedFilterOption;
				HistoryData.FavoritesViewTabChecked = (bool)mObjectTypeFilterTier.mFavoritesViewTab.IsChecked;
				HistoryData.SelectedFilterOptions = new List<List<String>>(mFilterTiers.Count);
				HistoryData.FilterOptions = new List<List<FilterOption>>(mFilterTiers.Count);

                HistoryData.SearchFieldNameChecked = (bool)mShouldSearchField_Name.IsChecked;
                HistoryData.SearchFieldPathChecked = (bool)mShouldSearchField_Path.IsChecked;
                HistoryData.SearchFieldTagsChecked = (bool)mShouldSearchField_Tags.IsChecked;
                HistoryData.SearchFieldTypeChecked = (bool)mShouldSearchField_Type.IsChecked;

                for (int TierIdx = 0; TierIdx < mFilterTiers.Count; ++TierIdx)
				{
                    // Store a list of all filter options
					HistoryData.FilterOptions.Add(mFilterTiers[TierIdx].GetFilterOptions( false ));

                    // Store a list of selected filter options
					HistoryData.SelectedFilterOptions.Add(mFilterTiers[TierIdx].GetSelectedOptions());
				}
			}

            // The match filter should be saved when not performing a full save
            HistoryData.MatchFilter = mAllAnySwitch.SelectedIndex;

		}

        /// <summary>
        /// Restores the filter panel to a different state based on the passed in data
        /// </summary>
        /// <param name="HistoryData">History data object containing filter panel data to restore</param>
		public void RestoreHistoryData( ContentBrowserHistoryData HistoryData )
		{
            // Do not allow filter events to be triggered when we are restoring data
			mDisableFilterEvent = true;

			// Clear the filter back to defaults so we can select things without
			// worrying about history states being combined.
			ClearFilter();

			// Restore which object type tab is selected (Favorites or All types)
			mObjectTypeFilterTier.mTypesViewTab.IsChecked = !HistoryData.FavoritesViewTabChecked;
			mObjectTypeFilterTier.mFavoritesViewTab.IsChecked = HistoryData.FavoritesViewTabChecked;

			// Restore type filter options
			mObjectTypeFilterTier.SetSelectedOptions(HistoryData.SelectedObjectTypes);

			// Restore search string
			m_Filter.SearchString = HistoryData.SearchString;
			mSearchTextBox.Text = HistoryData.SearchString;

            // Restore search fields
            mShouldSearchField_Name.IsChecked = HistoryData.SearchFieldNameChecked;
            mShouldSearchField_Path.IsChecked = HistoryData.SearchFieldPathChecked;
            mShouldSearchField_Tags.IsChecked = HistoryData.SearchFieldTagsChecked;
            mShouldSearchField_Type.IsChecked = HistoryData.SearchFieldTypeChecked;

            // Restore match all/match any switch.
            mAllAnySwitch.SelectedIndex = HistoryData.MatchFilter;
   
			// Restore tagged and loaded button states.
			m_Filter.TaggedFilterOption = (AssetFilter.TaggedState)HistoryData.TaggedFilterOption;
			m_Filter.LoadedFilterOption = (AssetFilter.LoadedState)HistoryData.LoadedFilterOption;

			// Synchronize the filter with the tagged and loaded radio buttons.
			if( m_Filter.TaggedFilterOption == AssetFilter.TaggedState.UntaggedOnly )
			{
				mShowUntaggedOnly.IsChecked = true;
			}
			else if( m_Filter.TaggedFilterOption == AssetFilter.TaggedState.TaggedOnly )
			{
				mShowTaggedOnly.IsChecked = true;
			}
			else
			{
				mShowTaggedAndUntagged.IsChecked = true;
			}

			if ( m_Filter.LoadedFilterOption == AssetFilter.LoadedState.UnloadedOnly )
			{
				mShowUnloadedOnly.IsChecked = true;
			}
			else if ( m_Filter.LoadedFilterOption == AssetFilter.LoadedState.LoadedOnly )
			{
				mShowLoadedOnly.IsChecked = true;
			}
			else
			{
				mShowLoadedAndUnloaded.IsChecked = true;
			}
			
			// Restore tag tiers
			m_Filter.SetTagTiers(HistoryData.SelectedFilterOptions);
			
			// Iterate through each tier, restoring filter options and selecting them as needed
			for (int TierIdx = 0; TierIdx < mFilterTiers.Count; ++TierIdx)
			{
				// Restore filter options available at the time of this history.  This step is 
				// necessary so we can actually select something in the next step
				mFilterTiers[TierIdx].SetFilterOptions(HistoryData.FilterOptions[TierIdx]);
				// Restore selected filter options. 
				mFilterTiers[TierIdx].SetSelectedOptions(HistoryData.SelectedFilterOptions[TierIdx]);
			}

			// Done restoring tags so re-enable the filter event
			mDisableFilterEvent = false;
		}

		#endregion


	}

	/// A filter option that can be visualized in a TagFilterTier.
	/// Users select these to filter content.
	public class FilterOption
	{
		/// The actual filter option; e.g. Water or COG or Shiny
		public String OptionText { get; set; }

		/// How many assets are associated with this option; e.g. 10 assets tagged as Water or COG or Shiny
		public int AssetCount { get; set; }

		/// Necessary for find-as-you-type to work in lists that display this element.
		public override string ToString()
		{
			return OptionText;
		}

		/// Is this item the special "All" option in the list.
		virtual public bool IsSpecialOption()
		{
			return false;
		}

		/// How the name of this Filter Option should appear.
		virtual public String FormattedText { get { return OptionText; } }
		/// The name of the group in which this filter option should appear (if any).
		virtual public String GroupName { get { return null; } }
	}

	/// Represents the filter option that appears in a Tags filter column of the FilterPanel.
	public class TagFilterOption : FilterOption, IComparable<TagFilterOption>, IComparable
	{
		/// Create a new TagFilterOption based on the 'Tag'.
		public TagFilterOption( Tag InTag )
		{
			Tag = InTag;
			OptionText = Tag.FullName;
		}

		protected Tag Tag { get; set; }

		/// Get the name of the tag.
		public String TagName { get{ return Tag.Name; } }
		/// Get the group name of the tag.
		public String TagGroupName { get { return Tag.GroupName; } }

		/// TagFilterOptions should show up as simply their name in the list.
		override public String FormattedText { get { return Tag.Name; } }
		/// TagFilter options should be grouped based on the Tag's group.
		public override string GroupName { get { return TagGroupName; } }

		/// Support comparing to other TAgFilterOptions.
		public int CompareTo( TagFilterOption other )
		{
			return this.Tag.CompareTo( other.Tag );
		}

		/// Implement vanilla IComparable for compatibility with WPF.
		public int CompareTo( object obj )
		{
			return this.CompareTo( obj as TagFilterOption );
		}
	}


	// @todo move these somewhere appropriate
	/// <summary>
	/// Converts a boolean to its opposite value
	/// </summary>
	[ValueConversion(typeof(Boolean), typeof(Boolean))]
	public class NegatingConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			Boolean BoolValue = (Boolean)value;
			return !BoolValue;
		}

		/// Converts back to the source type from the target type
		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			Boolean BoolValue = (Boolean)value;
			return !BoolValue;
		}
	}
	
	/// <summary>
	/// Converts a boolean to its opposite value
	/// </summary>
	[ValueConversion( typeof( Object ), typeof( Visibility ) )]
	public class NullToVisibilityConverter
		: IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return ( value == null ) ? Visibility.Collapsed : Visibility.Visible;
		}

		/// Converts back to the source type from the target type
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;
		}
	}


}
