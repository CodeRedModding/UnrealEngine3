//=============================================================================
//	SortableColumnHeader.xaml.cs: User control for a tag editing box
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
using ContentBrowser;
using System.Text.RegularExpressions;
using System.Windows.Media.Animation;
using System.Collections;


namespace CustomControls
{
	/// <summary>
	/// Interaction logic for SortableColumnHeader.xaml
	/// </summary>
	public partial class SortableColumnHeader : UserControl
	{

		/// Construct a SortableColumnHeader
		public SortableColumnHeader()
		{
			InitializeComponent();
			this.SortDirection = SortingOptions.Direction.NoSort;
		}

		/// Go to the next sort direction
		public void CycleSortDirection()
		{
			switch (mSortDirection)
			{
				case SortingOptions.Direction.Ascending:
					this.SortDirection = SortingOptions.Direction.Descending;
					break;
				case SortingOptions.Direction.NoSort:
				case SortingOptions.Direction.Descending:
					this.SortDirection = SortingOptions.Direction.Ascending;
					break;
			}
		}

		/// Update the arrow visual to match the current sort direction
		void UpdateVisuals()
		{
			if ( SortDirection == SortingOptions.Direction.Descending )
			{
				mUpArrow.Visibility = Visibility.Collapsed;
				mDownArrow.Visibility = Visibility.Visible;
				if ( DesignerProperties.GetIsInDesignMode(this) )
				{
					mText.Foreground = (SolidColorBrush)this.TryFindResource("Slate_Control_Foreground_Hover");
				}
				else
				{
					mText.Foreground = (SolidColorBrush)this.FindResource("Slate_Control_Foreground_Hover");
				}
			}
			else if ( SortDirection == SortingOptions.Direction.Ascending )
			{
				mUpArrow.Visibility = Visibility.Visible;
				mDownArrow.Visibility = Visibility.Collapsed;
				if ( DesignerProperties.GetIsInDesignMode(this) )
				{
					mText.Foreground = (SolidColorBrush)this.TryFindResource( "Slate_Control_Foreground_Hover" );
				}
				else
				{
					mText.Foreground = (SolidColorBrush)this.FindResource( "Slate_Control_Foreground_Hover" );
				}
			}
			else
			{
				mUpArrow.Visibility = Visibility.Collapsed;
				mDownArrow.Visibility = Visibility.Collapsed;
				if ( DesignerProperties.GetIsInDesignMode(this) )
				{
					mText.Foreground = (SolidColorBrush)this.TryFindResource( "Slate_Control_Foreground" );
				}
				else
				{
					mText.Foreground = (SolidColorBrush)this.FindResource( "Slate_Control_Foreground" );
				}
			}
		}

		/// Handler for sort direction changing
		delegate void SortDirectionChangedHandler();
		/// Event fired when SortDirection changes
		event SortDirectionChangedHandler SortDirectionChanged;

		/// SortDirection backing store
		private ContentBrowser.SortingOptions.Direction mSortDirection;
		/// Gets or sets the SortDirection of this column header
		public ContentBrowser.SortingOptions.Direction SortDirection
		{
			get { return mSortDirection; }
			set
			{
				mSortDirection = value;
				UpdateVisuals();
				if ( SortDirectionChanged != null )
				{
					SortDirectionChanged();
				}
			}
		}


		/// The text to be shown in the column header. This is a dependency property
		public String Text
		{
			get { return (String)GetValue( TextProperty ); }
			set { SetValue( TextProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for Text.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty TextProperty =
			DependencyProperty.Register( "Text", typeof( String ), typeof( SortableColumnHeader ) );
	}
}
