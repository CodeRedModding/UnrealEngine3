/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
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
using CustomControls;
using System.Collections;

namespace ContentBrowser
{

	public static class TypeFilterTierCommands
	{
		private static RoutedUICommand mAddToFavorites = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_FilterPanel_AddToFavoritesCommand" ), "AddToFavorites", typeof( TypeFilterTierCommands ) );

		/// Command that Refresh the asset view.
		public static RoutedUICommand AddToFavorites { get { return mAddToFavorites; } }

		private static RoutedUICommand mRemoveFromFavorites = new RoutedUICommand(
			UnrealEd.Utils.Localize( "ContentBrowser_FilterPanel_RemoveFromFavoritesCommand" ), "RemoveFromFavorites", typeof( TypeFilterTierCommands ) );

		/// Command that Refresh the asset view.
		public static RoutedUICommand RemoveFromFavorites { get { return mRemoveFromFavorites; } }

	}

	/// <summary>
	/// Interaction logic for TypeFilterTier.xaml
	/// </summary>
	public partial class TypeFilterTier : UserControl, FilterTier
	{

		public TypeFilterTier()
		{
			InitializeComponent();

			FavoriteTypes = new List<FilterOption>();
			this.mFavoritesListView.SetCustomSort(TypeFilterOptionComparer.SharedInstance);
			this.mTypesListView.SetCustomSort(TypeFilterOptionComparer.SharedInstance);
			
			// @todo cb: make this configurable?
			this.mFavoritesListView.SetFilterOptions( new List<FilterOption> { } );
			this.mFavoritesListView.ClearSelection();

			// Register handlers for FauxTabs being clicked; this causes a switch between "Favorites" and "Types".
			this.mFavoritesViewTab.Checked += new RoutedEventHandler( FauxTab_Switched );
			this.mTypesViewTab.Checked += new RoutedEventHandler( FauxTab_Switched );

			// Register handlers for user changing the current filter options
			this.mFavoritesListView.SelectionChanged += new SelectionChangedHandler( FilterOptions_SelectionChanged );
			this.mTypesListView.SelectionChanged += new SelectionChangedHandler( FilterOptions_SelectionChanged );

			// Modify the tier's appearance when enabled/disabled.
			this.IsEnabledChanged += new DependencyPropertyChangedEventHandler( FilterNodeVisual_IsEnabledChanged );

			this.CommandBindings.Add( new CommandBinding( TypeFilterTierCommands.AddToFavorites, AddToFavoritesHandler, CanAddToFavoritesHandler ) );
			this.CommandBindings.Add( new CommandBinding( TypeFilterTierCommands.RemoveFromFavorites, RemoveFromFavoritesHandler, CanRemoveFromFavoritesHandler ) );
		}

		/// Returns the currently visible list: Types or Favorites.
		private FilterListView SelectedFilterList
		{
			get
			{
				if ( mFavoritesViewTab.IsChecked ?? false )
				{
					return mFavoritesListView;
				}
				else
				{
					return mTypesListView;
				}
			}
		}

		/// Called whenever IsEnabled is toggled.
		void FilterNodeVisual_IsEnabledChanged( object sender, DependencyPropertyChangedEventArgs e )
		{
			if ( (bool)e.NewValue == false )
			{
				// show the special glyph.
				this.mDisabledGlyph.Visibility = Visibility.Visible;
			}
			else
			{
				// hide the special glyph
				this.mDisabledGlyph.Visibility = Visibility.Hidden;
			}
		}

		/// Conditionally highlights the Header to signify that the filter is active
		void UpdateHighlight()
		{
			// Highlight the header if the tier is active.
			mFilterBorder.BorderStyle = ( SelectedFilterList.IsAllSelected ) ? SlateBorder.BorderStyleType.FilterHeader : SlateBorder.BorderStyleType.ActiveFilterHeader;
		}

		/// Called when the selection in either Favorites or Types has changed.
		void FilterOptions_SelectionChanged( FilterListView Sender )
		{
			UpdateHighlight();
		}

		#region Favorites Command Handling

		/// Handler for the AddToFavorites Command (actually adds an item to favorites).
		void AddToFavoritesHandler( object sender, ExecutedRoutedEventArgs e )
		{
			ListViewItem CommandSourceItem = (ListViewItem)e.OriginalSource;
			String NewFavoritesOption = ( (FilterOption)CommandSourceItem.Content ).OptionText;
			FavoriteTypes.Add( new FilterOption { OptionText = NewFavoritesOption } );
			SortOptions( FavoriteTypes );
			mFavoritesListView.SetFilterOptions( FavoriteTypes );
			RaiseFavoritesChanged();
		}

		/// Determines whether the right clicked item can be added to favorites.
		void CanAddToFavoritesHandler( object sender, CanExecuteRoutedEventArgs e )
		{
			ListViewItem CommandSourceItem = (ListViewItem)e.OriginalSource;
			if (CommandSourceItem.Content is FilterOption)
			{
				String RightClickedOptionText = ( (FilterOption)CommandSourceItem.Content ).OptionText;
				bool IsSpecialItemInFavorites = this.mFavoritesListView.IsSpecialItem( CommandSourceItem );
				bool IsSpecialItemInTypes = this.mTypesListView.IsSpecialItem( CommandSourceItem );
				bool InFavorites = null != this.FavoriteTypes.Find( CandidateItem => CandidateItem.OptionText == RightClickedOptionText );

				e.CanExecute = !IsSpecialItemInFavorites && !IsSpecialItemInTypes && !InFavorites;
			}
			
		}

		/// Handler for the RemoveFromFavorites Command (actually removes an item from Favorites).
		void RemoveFromFavoritesHandler( object sender, ExecutedRoutedEventArgs e )
		{
			ListViewItem CommandSourceItem = (ListViewItem)e.OriginalSource;
			String FavoritesOptionToRemove = ( (FilterOption)CommandSourceItem.Content ).OptionText;
			FavoriteTypes.RemoveAll( RemoveCandidate=>RemoveCandidate.OptionText == FavoritesOptionToRemove );
			mFavoritesListView.SetFilterOptions( FavoriteTypes );
			RaiseFavoritesChanged();
		}

		/// Determines if the right-clicked item can be removed from favorites.
		void CanRemoveFromFavoritesHandler( object sender, CanExecuteRoutedEventArgs e )
		{
			ListViewItem CommandSourceItem = (ListViewItem)e.OriginalSource;
			if ( CommandSourceItem.Content is FilterOption )
			{
				String RightClickedOptionText = ( (FilterOption)CommandSourceItem.Content ).OptionText;
				bool IsSpecialItemInFavorites = this.mFavoritesListView.IsSpecialItem( CommandSourceItem );
				bool IsSpecialItemInTypes = this.mTypesListView.IsSpecialItem( CommandSourceItem );
				bool InFavorites = null != this.FavoriteTypes.Find( CandidateItem => CandidateItem.OptionText == RightClickedOptionText );

				e.CanExecute = !IsSpecialItemInFavorites && !IsSpecialItemInTypes && InFavorites;
			}
		}

		#endregion


		public List<FilterOption> FavoriteTypes { get; protected set; }
		
		/// Set the types in the Favorites list.
		/// Note: this method does not trigger a FavoritesChanged event.
		public void SetFavorites( List<FilterOption> InFavorites )
		{
			this.FavoriteTypes = InFavorites;
			SortOptions( this.FavoriteTypes );
			this.mFavoritesListView.SetFilterOptions( InFavorites );
		}

		/// Sort the favorites 
		private static void SortOptions( List<FilterOption> OptionsToSort )
		{
			OptionsToSort.Sort( ( FavoriteA, FavoriteB ) => String.Compare( FavoriteA.OptionText, FavoriteB.OptionText, true ) );
		}
		

		/// Called when the FauxTab selection is switched (Types or Favorites was clicked)
		void FauxTab_Switched( object sender, RoutedEventArgs e )
		{
			UpdateHighlight();
			if ( ActiveListChanged != null )
			{
				ActiveListChanged();
			}			
		}

		/// Handler delegate for switching the Faux Tabs.
		public delegate void ActiveListChangedHandler();
		/// Event that is emitted when a FauxTab becomes active (i.e. someone clicked Types or Favorites).
		public event ActiveListChangedHandler ActiveListChanged;

		/// Handler for users adding or removing from the Favorites
		public delegate void FavoritesChangedHandler();
		/// Event that is emitted whenever the Favorites changes.
		public event FavoritesChangedHandler FavoritesChanged;
		/// Raises the FavoritesChanged event.
		void RaiseFavoritesChanged()
		{
			if (FavoritesChanged != null)
			{
				this.FavoritesChanged();
			}
		}

		#region FilterTier Interface Implementation

		/// The event fired when the selection in this FilterTier changes.
		public event CustomControls.SelectionChangedHandler SelectionChanged
		{
			add
			{
				mTypesListView.SelectionChanged += value;
				mFavoritesListView.SelectionChanged += value;
			}
			remove
			{
				mTypesListView.SelectionChanged -= value;
				mFavoritesListView.SelectionChanged -= value;
			}
		}

		public void SetFilterOptions( List<FilterOption> InFilterOptions )
		{
			// We share the special "all" item between Favorites and All Types.

			this.mTypesListView.NumObjectTypes = InFilterOptions.Count;
			this.mTypesListView.SetFilterOptions( InFilterOptions );

			this.mFavoritesListView.NumObjectTypes = InFilterOptions.Count;
			this.mFavoritesListView.SetFilterOptions( FavoriteTypes ); // Force a refresh of the number of object types in the "All" item under Favorites.
		}

        /// <summary>
        /// Returns a list of currently selected options in this filter tier
        /// </summary>
		public List<string> GetSelectedOptions()
		{
			return SelectedFilterList.GetSelectedOptions();
		}

        /// <summary>
        /// Sets Selected options for this filter tier
        /// </summary>
        /// <param name="SelectedOptions">A list of options to select</param>
		public void SetSelectedOptions( List<String> SelectedOptions ) 
		{
			SelectedFilterList.SetSelectedOptions( SelectedOptions );
			
		}

		public string GenerateSpecialItemText( List<FilterOption> InFilterOptions )
		{
			throw new NotImplementedException("This method should never be called");
		}

		public void ClearSelection()
		{
			SelectedFilterList.ClearSelection();
		}

		public void SetGroupDescription( PropertyGroupDescription InGroupDescription )
		{
			mTypesListView.SetGroupDescription( InGroupDescription );
		}

		public bool IsFilterInactive { get { return SelectedFilterList.IsFilterInactive; } }

		#endregion
	}

	/// Comparer used to sort object types
	public class TypeFilterOptionComparer : IComparer
	{
		public static TypeFilterOptionComparer SharedInstance = new TypeFilterOptionComparer();
		public int Compare(object x, object y)
		{
			FilterOption XFilterOption = x as FilterOption;
			FilterOption YFilterOption = y as FilterOption;
			if (x == y)
			{
				return 0;
			}
			else if (XFilterOption.IsSpecialOption())
			{
				// The "All" item should always appear on top
				return -1;
			}
			else if (YFilterOption.IsSpecialOption())
			{
				// The "All" item should always appear on top
				return +1;
			}
			else
			{
				return XFilterOption.OptionText.CompareTo(YFilterOption.OptionText);
			}

		}
	}



	/// A FilterListView for Types; has "All ({0} Object Types)" as its special item.
	public class TypeFilterListView : FilterListView
	{
		public override String GenerateSpecialItemText( List<FilterOption> InFilterOptions )
		{
			// Now insert our special "All" option at the top of the list
			String LocalizedString = UnrealEd.Utils.Localize( "ContentBrowser_ObjectTypeFilterList_All" ); // ({0} Object Types)
			return String.Format( LocalizedString, NumObjectTypes );
		}

		/// How many object types there are; the Favorites List does not know unless we tell it.
		public int NumObjectTypes { get; set; }

	}
}
