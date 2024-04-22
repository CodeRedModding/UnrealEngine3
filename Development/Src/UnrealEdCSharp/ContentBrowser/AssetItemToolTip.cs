//=============================================================================
//	AssetItemToolTip.cs: Content browser asset item tool tip
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Media;


namespace ContentBrowser
{
	
	/// <summary>
	/// Asset item tool tip
	/// </summary>
	public class AssetItemToolTip : ContentControl
	{
		/// The content browser we're associated with
		MainControl m_ContentBrowser;


		/// Whether the tool tip should currently be shown.  This is used to kick off animations and update
		/// the tool tip's visibility
		public bool ToolTipShouldBeVisible
		{
			get { return (bool)GetValue( ToolTipShouldBeVisibleProperty ); }
			set { SetValue( ToolTipShouldBeVisibleProperty, value ); }
		}
		public static readonly DependencyProperty ToolTipShouldBeVisibleProperty =
			DependencyProperty.Register( "ToolTipShouldBeVisible", typeof( bool ), typeof( AssetItemToolTip ) );


		/// Whether the tool tip is currently shown
		public bool ToolTipIsVisible
		{
			get { return (bool)GetValue( ToolTipIsVisibleProperty ); }
			set { SetValue( ToolTipIsVisibleProperty, value ); }
		}
		public static readonly DependencyProperty ToolTipIsVisibleProperty =
			DependencyProperty.Register( "ToolTipIsVisible", typeof( bool ), typeof( AssetItemToolTip ) );

		
		/// Name of the asset (no path)
		public static readonly DependencyProperty AssetNameProperty =
			DependencyProperty.Register( "AssetName", typeof( string ), typeof( AssetItemToolTip ) );


		/// Asset tags
		public static readonly DependencyProperty AssetTagsProperty =
			DependencyProperty.Register( "AssetTags", typeof( string ), typeof( AssetItemToolTip ) );
		
		/// Asset memory usage
		public String AssetMemUsage
		{
			get { return (String)GetValue(AssetMemUsageProperty); }
			set { SetValue(AssetMemUsageProperty, value); }
		}
		public static readonly DependencyProperty AssetMemUsageProperty =
			DependencyProperty.Register("AssetMemUsage", typeof( string ), typeof( AssetItemToolTip ) );

		/// Asset tags (private, unformatted)
		private static readonly DependencyProperty UnformattedAssetTagsProperty =
			DependencyProperty.Register( "UnformattedAssetTags", typeof( string ), typeof( AssetItemToolTip ), new PropertyMetadata( new PropertyChangedCallback( UnformattedAssetTagsChangedDelegate ) ) );

		/// Called when the unformatted asset tags string is changed.  Formats the string and stores it in AssetTagsProperty.
		static public void UnformattedAssetTagsChangedDelegate( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			AssetItemToolTip Self = (AssetItemToolTip)d;
			String NewTagsString = (String)e.NewValue;
			if( NewTagsString != null && NewTagsString.Length > 0 )
			{
				// ...
			}

			Self.SetValue( AssetTagsProperty, NewTagsString );
		}


		/// Asset collections
		public static readonly DependencyProperty AssetCollectionsProperty =
			DependencyProperty.Register( "AssetCollections", typeof( string ), typeof( AssetItemToolTip ) );

		
		/// Asset path
		public static readonly DependencyProperty AssetPathProperty =
			DependencyProperty.Register( "AssetPath", typeof( string ), typeof( AssetItemToolTip ) );


		/// Asset type (private, unformatted)
		private static readonly DependencyProperty UnformattedAssetTypeProperty =
			DependencyProperty.Register( "UnformattedAssetType", typeof( string ), typeof( AssetItemToolTip ), new PropertyMetadata( new PropertyChangedCallback( UnformattedAssetTypeChangedDelegate ) ) );

		/// Called when the unformatted asset type string is changed.  Formats the string and stores it in AssetTypeProperty.
		static public void UnformattedAssetTypeChangedDelegate( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			AssetItemToolTip Self = (AssetItemToolTip)d;
			Self.SetValue( AssetTypeProperty, "(" + (String)e.NewValue + ")" );
		}

		/// Asset type (formatted)
		public static readonly DependencyProperty AssetTypeProperty =
			DependencyProperty.Register( "AssetType", typeof( string ), typeof( AssetItemToolTip ) );




		/// Asset status (private, unformatted)
		private static readonly DependencyProperty UnformattedAssetStatusProperty =
			DependencyProperty.Register( "UnformattedAssetStatus", typeof( string ), typeof( AssetItemToolTip ), new PropertyMetadata( new PropertyChangedCallback( UnformattedAssetStatusChangedDelegate ) ) );

		/// Called when the unformatted asset type string is changed.  Formats the string and stores it in AssetStatusProperty.
		static public void UnformattedAssetStatusChangedDelegate( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			AssetItemToolTip Self = (AssetItemToolTip)d;
			Self.SetValue( AssetStatusProperty, "(" + (String)e.NewValue + ")" );
		}

		/// Asset type (formatted)
		public static readonly DependencyProperty AssetStatusProperty =
			DependencyProperty.Register( "AssetStatus", typeof( string ), typeof( AssetItemToolTip ) );


		/// Custom label strings for this asset (may be null)
		public List<String> CustomLabels
		{
			get { return (List<String>)GetValue( CustomLabelsProperty ); }
			set { SetValue( CustomLabelsProperty, value ); }
		}

		/// Custom label dependency property
		public static readonly DependencyProperty CustomLabelsProperty =
			DependencyProperty.Register( "CustomLabels", typeof( List<String> ), typeof( AssetItemToolTip ) );



		/// Date added (text)
		public static readonly DependencyProperty DateAddedProperty =
			DependencyProperty.Register( "DateAdded", typeof( string ), typeof( AssetItemToolTip ) );

		/// Date added (DateTime) (private, unformatted)
		private static readonly DependencyProperty UnformattedDateAddedProperty =
			DependencyProperty.Register( "UnformattedDateAdded", typeof( DateTime ), typeof( AssetItemToolTip ), new PropertyMetadata( new PropertyChangedCallback(

				/// Called when the unformatted date added is changed.  Formats a string and stores it in DateAddedProperty.
				delegate( DependencyObject d, DependencyPropertyChangedEventArgs e )
				{
					AssetItemToolTip Self = (AssetItemToolTip)d;
					DateTime DateAdded = (DateTime)e.NewValue;

					String FormattedDateAdded = DateAdded.ToShortDateString();

					// Also append a time span
					TimeSpan TimeSinceAdded = DateTime.Today - DateAdded;
					if( TimeSinceAdded.Days == 0 )		// Today?
					{
						FormattedDateAdded += UnrealEd.Utils.Localize( "ContentBrowser_AssetItemToolTip_DateAddedTimeSpanSuffix_Today" );
					}
					else if( TimeSinceAdded.Days < 1000 )		// Don't display time spans for super-old assets
					{
						FormattedDateAdded += UnrealEd.Utils.Localize( "ContentBrowser_AssetItemToolTip_DateAddedTimeSpanSuffix_DayCount", TimeSinceAdded.Days );
					}

					Self.SetValue( DateAddedProperty, FormattedDateAdded );
				}
			) ) );


		/// Border color for color-coding visuals based on the object's type
		public Color ObjectTypeHighlightColor
		{
			get { return (Color)GetValue( ObjectTypeHighlightColorProperty ); }
			set { SetValue( ObjectTypeHighlightColorProperty, value ); }
		}
		public static readonly DependencyProperty ObjectTypeHighlightColorProperty =
			DependencyProperty.Register( "ObjectTypeHighlightColor", typeof( Color ), typeof( AssetItemToolTip ), new UIPropertyMetadata( Color.FromArgb( 200, 255, 255, 255 ) ) );


		/// <summary>
		/// Constructor
		/// </summary>
		public AssetItemToolTip()
		{
			if( DesignerProperties.GetIsInDesignMode( this ) )
			{
				SetValue( AssetNameProperty, "MyAssetName" );
				SetValue( UnformattedAssetTagsProperty, "Tag1 Tag2 Tag3" );
				SetValue( AssetCollectionsProperty, "Collection1 Collection2" );
				SetValue( AssetPathProperty, "MyPackage.MyGroup" );
				SetValue( UnformattedAssetTypeProperty, "ObjectType" );
				SetValue( UnformattedAssetStatusProperty, "Status" );
				SetValue( AssetMemUsageProperty, "Memory" ); 

				var CustomLabelsList = new List<String>();
				CustomLabelsList.Add( "Custom Label One" );
				CustomLabelsList.Add( "Custom Label Two" );
				CustomLabelsList.Add( "Custom Label Three" );
				CustomLabelsList.Add( "Custom Label Four" );
				SetValue( CustomLabelsProperty, CustomLabelsList );
			}
		}



		/// <summary>
		/// Static constructor
		/// </summary>
		static AssetItemToolTip()
		{
			// NOTE: This is required for WPF to load the style and control template from generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( AssetItemToolTip ), new FrameworkPropertyMetadata( typeof( AssetItemToolTip ) ) );
		}



		/// <summary>
		/// Binds the tool tip to the specified asset item. (may be null)
		/// </summary>
		/// <param name="InAssetItem">The asset item we're being created for</param>
		public void BindToAssetItem( MainControl InContentBrowser, AssetItem InAssetItem )
		{
			m_ContentBrowser = InContentBrowser;


			if( InAssetItem != null )
			{
				// Start fading in the tool tip
				SetValue( ToolTipShouldBeVisibleProperty, true );


				// Cache the collections that this asset is associated with.  (Not updated dynamically)
				{
					StringBuilder CollectionsString = new StringBuilder();

					// @todo: Color code shared collections vs private collections separately?

					// Add shared collections
					List<String> SharedCollectionNames = m_ContentBrowser.Backend.QueryCollectionsForAsset( InAssetItem.FullName, EBrowserCollectionType.Shared );
					foreach( String CurCollectionName in SharedCollectionNames )
					{
						if( CollectionsString.Length > 0 )
						{
							CollectionsString.Append( ", " );
						}
						CollectionsString.Append( CurCollectionName );
					}

					// Add private collections
					List<String> PrivateCollectionNames = m_ContentBrowser.Backend.QueryCollectionsForAsset( InAssetItem.FullName, EBrowserCollectionType.Private );
					foreach( String CurCollectionName in PrivateCollectionNames )
					{
						if( CollectionsString.Length > 0 )
						{
							CollectionsString.Append( ", " );
						}
						CollectionsString.Append( CurCollectionName );
					}

					SetValue( AssetCollectionsProperty, CollectionsString.ToString() );
				}


	
				// Set the border color using the asset's type
				ObjectTypeHighlightColor = m_ContentBrowser.Backend.GetAssetVisualBorderColorForObjectTypeName( InAssetItem.AssetType );


	
				// Bind the asset item's properties to our own
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.AssetNameProperty, InAssetItem, "Name" );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.UnformattedAssetTagsProperty, InAssetItem, "TagsAsString" );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.UnformattedAssetTypeProperty, InAssetItem, "AssetType" );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.AssetPathProperty, InAssetItem, "PathOnly" );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.AssetStatusProperty, InAssetItem, "LoadedStatus", new AssetItemToolTipValueConverters.AssetLoadedStatusToString() );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.CustomLabelsProperty, InAssetItem, "CustomLabels" );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.UnformattedDateAddedProperty, InAssetItem, "DateAdded" );
				UnrealEd.Utils.CreateBinding( this, AssetItemToolTip.AssetMemUsageProperty, InAssetItem, "MemoryUsageText" );
			}
			else
			{
				// Start fading out the tool tip
				SetValue( ToolTipShouldBeVisibleProperty, false );


				// Capture the current values before we clear the bindings so that the panel's contents
				// will appear unchanged as we're fading out
				SetValue( AssetNameProperty, GetValue( AssetNameProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.AssetNameProperty );

				SetValue( UnformattedAssetTagsProperty, GetValue( UnformattedAssetTagsProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.UnformattedAssetTagsProperty );

				SetValue( UnformattedAssetTypeProperty, GetValue( UnformattedAssetTypeProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.UnformattedAssetTypeProperty );

				SetValue( AssetPathProperty, GetValue( AssetPathProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.AssetPathProperty );

				SetValue( AssetStatusProperty, GetValue( AssetStatusProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.AssetStatusProperty );

				SetValue( CustomLabelsProperty, GetValue( CustomLabelsProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.CustomLabelsProperty );

				SetValue( UnformattedDateAddedProperty, GetValue( UnformattedDateAddedProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.UnformattedDateAddedProperty );

				SetValue( AssetMemUsageProperty, GetValue( AssetMemUsageProperty ) );
				UnrealEd.Utils.ClearBinding( this, AssetItemToolTip.AssetMemUsageProperty );
			}
		}

	}



	#region Value Converters for the asset tooltip

	namespace AssetItemToolTipValueConverters
	{

		/// <summary>
		/// Converts an asset's "Loaded Status" to a string for the status in the tooltip
		/// </summary>
		[ValueConversion( typeof( AssetItem.LoadedStatusType ), typeof( string ) )]
		public class AssetLoadedStatusToString
			: IValueConverter
		{
			/// Converts from the source type to the target type
			public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
			{
				AssetItem.LoadedStatusType LoadedStatus = (AssetItem.LoadedStatusType)value;
				if( LoadedStatus == AssetItem.LoadedStatusType.NotLoaded )
				{
					return UnrealEd.Utils.Localize( "ContentBrowser_AssetItemToolTip_Status_NotLoaded" );
				}
				else if( LoadedStatus == AssetItem.LoadedStatusType.Loaded )
				{
					return UnrealEd.Utils.Localize( "ContentBrowser_AssetItemToolTip_Status_Loaded" );
				}
				else if( LoadedStatus == AssetItem.LoadedStatusType.LoadedAndModified )
				{
					return UnrealEd.Utils.Localize( "ContentBrowser_AssetItemToolTip_Status_LoadedAndModified" );
				}
				
				return null;
			}

			/// Converts back to the source type from the target type
			public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
			{
				return null;	// Not supported
			}
		}



		/// <summary>
		/// Converts a string to a collapsed visibility state.  (Empty string == collapsed)
		/// </summary>
		[ValueConversion( typeof( string ), typeof( Visibility ) )]
		public class EmptyStringToCollapsedVisibility
			: IValueConverter
		{
			/// Converts from the source type to the target type
			public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
			{
				String TheString = (String)value;
				return ( TheString == null || TheString.Length == 0 ) ? Visibility.Collapsed : Visibility.Visible;
			}

			/// Converts back to the source type from the target type
			public object ConvertBack( object value, Type targetType, object parameter, CultureInfo culture )
			{
				return null;	// Not supported
			}
		}
	}

	#endregion

}
