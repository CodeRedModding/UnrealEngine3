//=============================================================================
//	FilterListView.xaml.cs: Custom control for a filterable list view
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
using ContentBrowser;
using System.Collections;
using System.Collections.ObjectModel;


namespace CustomControls
{

	/// <summary>
	/// FilterListView wraps a ListView to provide functionality similar to the iTunes library browser.
    /// The first item is treated as the "All" item, and its selection is exclusive with the rest of the items
    /// in the list. It is up to the user to make sure the first item is named appropriately by providing an implementation
	/// of GenerateSpecialItemText().
	/// </summary>
	public abstract partial class FilterListView : UserControl, FilterTier
	{

		/// Construct a FilterListView
		public FilterListView()
		{
			InitializeComponent();
			
			// This list view will display the filter options. We start with an empty list.
			this.mFitlerOptions = new ObservableCollection<FilterOption>();
			m_ListView.ItemsSource = mFitlerOptions;

			// Create a handler for the selection change event; we will be binding/unbinding this handler often to prevent recursion.
			MySelectionChangedHandler = new SelectionChangedEventHandler( m_ListView_SelectionChanged );

			// Register handler for selection change so we can provide exclusive selection between "All" item and the rest of the list.
			m_ListView.SelectionChanged += MySelectionChangedHandler;
			
		}

		/// Test whether the item passed in is the special "All" item in this list.
		public bool IsSpecialItem( ListViewItem InItem )
		{
			return this.m_ListView.Items != null && this.m_ListView.Items.Count > 0 && this.m_ListView.Items[0] == InItem;
		}

		/// Effectively disable this list.
		public void ClearList()
		{
			if ( this.ListView.Items.Count > 1 )
			{
				this.SetFilterOptions( new List<FilterOption>(0) );
			}			
		}

		/// <summary>
		/// Set the options available to the user for filtering in this tier (column).
		/// </summary>
		/// <param name="InFilterOptions">List of options from which to choose.</param>
		public virtual void SetFilterOptions( List<FilterOption> InFilterOptions )
		{
			if (InFilterOptions.Count == 0)
			{
				mFitlerOptions.Clear();
			}
			else
			{
				// Make a copy; we are about to add an "All" filter option into the list.
				List<Object> FilterOptions = new List<Object>(InFilterOptions.Count + 1);
				foreach (FilterOption Option in InFilterOptions)
				{
					FilterOptions.Add(Option);
				}

				// Save the selection before the filter options are set
				List<String> SelectionBeforeNewOptions = GetSelectedOptions();

				// Now insert our special "All" option at the top of the list
				FilterOptions.Insert(0, new SpecialFilterOption { OptionText = GenerateSpecialItemText(InFilterOptions) });

				List<FilterOption> ItemsToAdd = new List<FilterOption>();
				List<FilterOption> ItemsToRemove = new List<FilterOption>();
				foreach (FilterOption ExistingOption in this.ListView.Items)
				{
					if (!FilterOptions.Contains(ExistingOption))
					{
						ItemsToRemove.Add(ExistingOption);
					}
				}

				foreach (FilterOption NewOption in FilterOptions)
				{
					if (!this.ListView.Items.Contains(NewOption))
					{
						ItemsToAdd.Add(NewOption);
					}
				}

				foreach (FilterOption Option in ItemsToRemove)
				{
					mFitlerOptions.Remove(Option);
				}
				foreach (FilterOption Option in ItemsToAdd)
				{
					mFitlerOptions.Add(Option);
				}






				// Restore the selection
				foreach (String PreviouslySelected in SelectionBeforeNewOptions)
				{
					FilterOption PreviouslySelectionOption = (FilterOption)FilterOptions.Find(CurFilterOption => (CurFilterOption is FilterOption) && ((FilterOption)CurFilterOption).OptionText == PreviouslySelected);
					if (PreviouslySelectionOption != null)
					{
						this.ListView.SelectedItems.Add(PreviouslySelectionOption);
					}
				}

				EnsureSpecialItemSelection();
			}
		}

		/// Set the comparer to use for sorting this list.
		public void SetCustomSort( IComparer InComparer )
		{
			ListCollectionView ProxyCollection = (ListCollectionView)CollectionViewSource.GetDefaultView( this.ListView.ItemsSource );
			ProxyCollection.CustomSort = InComparer;
		}

		
		/// Get the currently selected filter options; returns an empty list when not active.
		public List<String> GetSelectedOptions()
		{
			if ( !IsFilterInactive )
			{
				List<String> SelectedItems = new List<String>( ListView.SelectedItems.Count );
				foreach ( Object Item in ListView.SelectedItems )
				{
					SelectedItems.Add( ( (FilterOption)Item ).OptionText );
				}

				return new List<String>( SelectedItems );
			}
			else
			{
				return new List<String>( 0 );
			}
		}

        /// <summary>
        /// Sets the selected options for this listview
        /// </summary>
        /// <param name="SelectedOptions">A list of options to select</param>
		public void SetSelectedOptions( List<String> SelectedOptions )
		{
            // Unselect all currently selected options first
			ListView.UnselectAll();
			foreach( FilterOption Option in ListView.Items )
			{
				if( SelectedOptions.Contains( Option.OptionText ) )
				{
                    // make sure the item to be selected actually exists in the list before selecting it
					ListView.SelectedItems.Add(Option);
				}
			}
		}

		/// Is the filter being applied, or is it allowing all assets through?
		public bool IsFilterInactive { get { return this.IsAllSelected; } }

		public int NumItems { get { return this.ListView.Items.Count; } }


		/// Called when an item in the list is clicked.
		void ItemLeftMouseDown( object sender, MouseButtonEventArgs e )
		{
			ListViewItem CurItem = sender as ListViewItem;

			// The list and item should both be focused now.
			Keyboard.Focus( CurItem );

			// Toggle item selection
			if ( !CurItem.IsSelected )
			{
			    m_ListView.SelectedItems.Add( m_ListView.ItemContainerGenerator.ItemFromContainer(CurItem) );
			}
			else
			{
				m_ListView.SelectedItems.Remove( m_ListView.ItemContainerGenerator.ItemFromContainer( CurItem ) );
			}
			
			e.Handled = true;
		}

		/// Called when an item in the list is double clicked.
		/// We want to unselect everything and select only the doubleclicked item.
		void ItemDoubleClick( object sender, MouseButtonEventArgs e )
		{
			ListViewItem CurItem = sender as ListViewItem;

			Keyboard.Focus( CurItem );

			// Disable selection events and select every item in the list, so
			// the user can see them all get un-selected. This is necessary to
			// provide the hint that everything in the list got un-selected.
			// Only do this if we are not doubleclicking the all item.
			if ( m_ListView.ItemContainerGenerator.IndexFromContainer(CurItem) != 0 )
			{
				DisableSelectionEvent();
				m_ListView.SelectAll();
				EnableSelectionEvent();
			}

			m_ListView.SelectedIndex = m_ListView.ItemContainerGenerator.IndexFromContainer( CurItem );
			
			e.Handled = true;
		}


		/// An event handler for selection changes.
		SelectionChangedEventHandler MySelectionChangedHandler;

		/// <summary>
		/// Handles selection changes in the wrapped list and sets off our own (non-Routed) event.
		/// Enforces the exclusive selection between the "All" item and everything else in the list.
		/// </summary>
		/// <param name="sender">ignored</param>
		/// <param name="e">ignored</param>
		void m_ListView_SelectionChanged( object sender, SelectionChangedEventArgs e )
		{

			// Disable selection event because we're about to manipulate the selection and this would cause infinite recursion.
			DisableSelectionEvent();

			if ( m_ListView.Items.Count > 0 )
			{
				if ( e.AddedItems.Contains( m_ListView.Items[0] ) && e.AddedItems.Count == 1 )
				{
					// The "all" item was just selected

					// unselect everything but the all item
					m_ListView.UnselectAll();
					m_ListView.SelectedIndex = 0;
				}
				else
				{
					// unselect the all item; keep the rest of the selection
					m_ListView.SelectedItems.Remove( m_ListView.Items[0] );
				}

				EnsureSpecialItemSelection();
				
			}

			// We're done manipulating selection, re-enable selection events.
			EnableSelectionEvent();

			// Fire a friendly version of the selection changed event;
			// Tell the receiver whether the "All" item is selected.
			if ( SelectionChanged != null )
			{
				this.IsAllSelected = m_ListView.Items.Count <= 0 || m_ListView.SelectedItems.Contains( m_ListView.Items[0] );
				SelectionChanged( this );
			}


		}

		/// Make sure that the special "all" item is selected if the list currently has empty selection.
		public void EnsureSpecialItemSelection()
		{
			if ( m_ListView.SelectedItems.Count == 0 )
			{
				// Nothing is selected; make sure the all item is selected
				if ( m_ListView.Items.Count > 0 )
				{
					m_ListView.SelectedIndex = 0;
				}
			}
		}

		/// Prevent the selection event from being emitted.
		void DisableSelectionEvent()
		{
			m_ListView.SelectionChanged -= MySelectionChangedHandler;
		}

		/// Enable the selection event being emitted.
		void EnableSelectionEvent()
		{
			m_ListView.SelectionChanged += MySelectionChangedHandler;
		}

		/// <summary>
		/// Resets selection to "All" (index 0)
		/// </summary>
		public void ClearSelection()
		{
			if (m_ListView.Items.Count > 0)
			{
				m_ListView.SelectedIndex = 0;
				m_ListView.ScrollIntoView( m_ListView.Items[0] );
			}
		}

		/// Set the PropertyGroupDescription to be used for grouping the items in this list.
		public void SetGroupDescription( PropertyGroupDescription InGroupDescription )
		{
			ListCollectionView ProxyCollection = (ListCollectionView)CollectionViewSource.GetDefaultView( m_ListView.ItemsSource );
			ProxyCollection.GroupDescriptions.Clear();
			ProxyCollection.GroupDescriptions.Add( InGroupDescription );
		}

		/// <summary>
		/// Whether the "All" item at index 0 is selected.
		/// </summary>
		public bool IsAllSelected
		{
			get { return (bool)GetValue(IsAllSelectedProperty); }
			set { SetValue(IsAllSelectedProperty, value); }
		}

		// Using a DependencyProperty as the backing store for IsAllSelected.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsAllSelectedProperty =
			DependencyProperty.Register("IsAllSelected", typeof(bool), typeof(FilterListView), new UIPropertyMetadata(true));


		#region ItemContextMenu Property
		
		/// The context menu which should be set on every FilterOption in the list. This is a dependency property.
		public ContextMenu ItemContextMenu
		{
			get { return (ContextMenu)GetValue( ItemContextMenuProperty ); }
			set { SetValue( ItemContextMenuProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for ItemContextMenu.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty ItemContextMenuProperty =
			DependencyProperty.Register( "ItemContextMenu", typeof( ContextMenu ), typeof( FilterListView ), new UIPropertyMetadata( null ) );

		#endregion



        /// Get the list view being wrapped.
		private ListView ListView
		{
            get { return m_ListView; }
		}


		private ObservableCollection<FilterOption> mFitlerOptions;

		/// <summary>
		/// Invoked when the list need to generate the text for the special "All" item.
		/// Implement this method to customize the text.
		/// </summary>
		/// <param name="InFilterOptions"> The filter options which will be shown in the list. </param>
		/// <returns> The text of the special "All" item. </returns>
		public abstract string GenerateSpecialItemText( List<FilterOption> InFilterOptions );

		/// The selection changed handler for selection.
		public event SelectionChangedHandler SelectionChanged;

	}

	/// <summary>
	/// Handler for an event that is fired when the selection changes.
	/// </summary>
	/// <param name="Sender">The filter list whose selection changed.</param>
	public delegate void SelectionChangedHandler( FilterListView Sender );


	/// A FilterTier is a column in the FilterPanel. FilterTiers have a set of filter options and a special "All" item.
	/// The "All" item's selection is exclusive with all other options. 
	public interface FilterTier
	{
		/// <summary>
		/// This event is fired when the selection changes.
		/// </summary>
		event SelectionChangedHandler SelectionChanged;

		/// <summary>
		/// Set the options available to the user for filtering in this tier (column).
		/// </summary>
		/// <param name="InFilterOptions">List of options from which to choose.</param>
		void SetFilterOptions( List<FilterOption> InFilterOptions );

		/// <summary>
		/// Generate the text for the special "all" item that the user selects to clear the options selected in this tier.
		/// </summary>
		/// <param name="InFilterOptions">The options in this tier, in case this information is needed.</param>
		/// <returns>The text of the special item.</returns>
		String GenerateSpecialItemText( List<FilterOption> InFilterOptions );

		/// Get the values selected by the user (what the filter is currently doing); returns an empty list when not active.
		List<String> GetSelectedOptions();

		/// True when this FilterTier is not filtering.
		bool IsFilterInactive { get; }
		
		/// Clear any options selected by the user; effectively makes the filter inactive.
		void ClearSelection();

		void SetGroupDescription( PropertyGroupDescription InGroupDescription );
	}

	/// The special filter option used by the "All" item.
	class SpecialFilterOption : FilterOption
	{
		override public bool IsSpecialOption()
		{
			return true;
		}
	}
}
