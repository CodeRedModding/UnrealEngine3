//=============================================================================
//	FielterTierVisual.cs: A single column/tier of the Filter Panel.
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

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
using System.Collections.ObjectModel;
using System.Globalization;
using System.Collections;

namespace ContentBrowser
{
	/// <summary>
	/// Represents a single column/tier of the Filter Panel.
	/// </summary>
	public partial class TagFilterTier : UserControl, FilterTier
	{
		public TagFilterTier()
		{
			InitializeComponent();
			mFilterListView.SelectionChanged += new CustomControls.SelectionChangedHandler( mFilterListView_SelectionChanged );

			// Make sure the tags are sorted and grouped so that the "All" item appears on top, tags appear in groups, and all the ungrouped tags appear at the bottom.
			this.FilterList.SetCustomSort(TagFilterOptionComparer.SharedInstance);
			this.SetGroupDescription(new PropertyGroupDescription(null, new FilterOptionToGroupConverter()));
		}

		/// The filter list control contained within this filter tier.
		public FilterListView FilterList { get { return mFilterListView; } }
	

		/// Called when the selection changes in this filter tier.
		void mFilterListView_SelectionChanged( FilterListView Sender )
		{
			// Highlight the header if the tier is active.
			mFilterBorder.BorderStyle = ( this.mFilterListView.IsAllSelected ) ? SlateBorder.BorderStyleType.FilterHeader : SlateBorder.BorderStyleType.ActiveFilterHeader;
		}

		#region Title Property

		/// The title that should appear in the header above the list. This is a dependency property.
		public String Title
		{
			get { return (String)GetValue( TitleProperty ); }
			set { SetValue( TitleProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for Title.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty TitleProperty =
			DependencyProperty.Register( "Title", typeof( String ), typeof( TagFilterTier ), new UIPropertyMetadata( "" ) );

		#endregion

		/// Remove all but the "AllTags" option from this tier
		internal void ClearTier()
		{
			this.mFilterListView.ClearList();

			UpdateInactivityGlyph();
		}

		#region FilterTier Implementation

		public void SetFilterOptions( List<FilterOption> InFilterOptions )
		{
			this.FilterList.SetFilterOptions( InFilterOptions );
			
			UpdateInactivityGlyph();
		}

		/// <summary>
		/// Returns a list of filter options in this filter tier
		/// </summary>
		/// <param name="bGetAllFilter">If true the "All" filter option will be returned</param>
		public List<FilterOption> GetFilterOptions( bool bGetAllFilter )
		{
			List<FilterOption> FilterOptions = new List<FilterOption>(this.FilterList.NumItems);
			foreach (Object Item in this.FilterList.m_ListView.Items )
			{
				FilterOption Option = (FilterOption)Item;
				if ( !bGetAllFilter && Option.IsSpecialOption() )
				{ 
					// Skip the "all" option if we arent getting the all filter
					continue;
				}
			
				FilterOptions.Add(Option);
			}

			return FilterOptions;
		}

		/// <summary>
		/// Returns a list of currently selected options in this filter tier
		/// </summary>
		public List<string> GetSelectedOptions()
		{
			return this.FilterList.GetSelectedOptions();
		}

		/// <summary>
		/// Sets Selected options for this filter tier
		/// </summary>
		/// <param name="SelectedOptions">A list of options to select</param>
		public void SetSelectedOptions( List<String> InSelectedOptions )
		{
			FilterList.SetSelectedOptions( InSelectedOptions );
		}

		public string GenerateSpecialItemText( List<FilterOption> InFilterOptions )
		{
			return this.FilterList.GenerateSpecialItemText( InFilterOptions );
		}

		public void ClearSelection()
		{
			this.FilterList.ClearSelection();
			UpdateInactivityGlyph();
		}

		public void SetGroupDescription( PropertyGroupDescription InGroupDescription )
		{
			this.FilterList.SetGroupDescription( InGroupDescription );
		}

		public bool IsFilterInactive { get { return mFilterListView.IsFilterInactive; } }

		public event CustomControls.SelectionChangedHandler SelectionChanged
		{
			add { mFilterListView.SelectionChanged += value; }
			remove { mFilterListView.SelectionChanged -= value; }
		}

		private void UpdateInactivityGlyph()
		{
			if ( IsFilterInactive && this.FilterList.NumItems <= 1 )
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

		#endregion

	}

	/// Comparer used to sort tags
	public class TagFilterOptionComparer : IComparer
	{
		public static TagFilterOptionComparer SharedInstance = new TagFilterOptionComparer();
		public int Compare( object x, object y )
		{
			FilterOption XFilterOption = x as FilterOption;
			FilterOption YFilterOption = y as FilterOption;
			if (x == y)
			{
				return 0;
			}
			else if ( XFilterOption.IsSpecialOption() )
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
				return ( x as TagFilterOption ).CompareTo( y as TagFilterOption );
			}
			
		}
	}

	public class TagFilterListView : FilterListView
	{
		/// Generate the "all (# tags)" text.
		public override String GenerateSpecialItemText( List<FilterOption> InFilterOptions )
		{
			String LocalizedString = UnrealEd.Utils.Localize( "ContentBrowser_TagFilterList_All" ); //"All ({0} Tags)
			return String.Format( LocalizedString, InFilterOptions.Count );
		}
	}

	/// <summary>
	/// Converts an asset's "Loaded Status" to a string for the status in the tooltip
	/// </summary>
	[ValueConversion( typeof( FilterOption ), typeof( String ) )]
	public class FilterOptionToGroupConverter : IValueConverter
	{
		/// Converts from the source type to the target type
		public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
		{
			FilterOption InFilterOption = value as FilterOption;
			return InFilterOption.GroupName;			
		}

		/// Converts back to the source type from the target type
		public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
		{
			return null;	// Not supported
		}
	}
}
