//=============================================================================
//	AssetVisual.cs: Content browser asset visual control
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Text;
using System.Windows;
using System.Windows.Data;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Input;
using UnrealEdCSharp.ContentBrowser;

namespace ContentBrowser
{
	/// Rules for generating a new thumbnail.  Note that even if you set this to Always, the engine
	/// may still pull a thumbnail from the package's thumbnail cache (unless you dirty that first!)
	public enum ThumbnailGenerationPolicy
	{
		/// Never generate
		Never,

		/// Generate a preview thumbnail
		Preview,

		/// Only generate if a thumbnail doesn't already exist in the cache
		OnlyIfNotInFreeList,

		/// Always generate a new thumbnail
		Force
	}

	
	/// <summary>
	/// Asset visual
	/// </summary>
	[TemplatePart( Name = "PART_LoadAssetButton", Type = typeof( AssetVisualLoadAssetButton ) )]
	public class AssetVisual : ContentControl
	{
		/// The asset item this visual is associated with
		public AssetItem AssetItem { get { return m_AssetItem; } }
		private AssetItem m_AssetItem;

		/// <summary>
		/// Visual states
		/// </summary>
		public enum VisualStateType
		{
			/// Default visual state (not selected)
			Default,

			/// Deselected interactively by the user
			DeselectedInteractively,

			/// Selected
			Selected,

			/// Selected interactively by the user (using the mouse or keyboard)
			SelectedInteractively,
		}


		/// Current visual state of the asset visual
		public VisualStateType VisualState
		{
			get { return (VisualStateType)GetValue( m_VisualStateProperty ); }
			set { SetValue( m_VisualStateProperty, value ); }
		}
		public static readonly DependencyProperty m_VisualStateProperty =
			DependencyProperty.Register( "VisualState", typeof( VisualStateType ), typeof( AssetVisual ) );



		/// Label text for the asset visual
		public string Label
		{
			get { return (string)GetValue( LabelProperty ); }
			set { SetValue( LabelProperty, value ); }
		}
		public static readonly DependencyProperty LabelProperty =
			DependencyProperty.Register( "Label", typeof( string ), typeof( AssetVisual ) );


		/// Custom label text for this asset
		public string CustomLabel
		{
			get { return (string)GetValue( CustomLabelProperty ); }
			set { SetValue( CustomLabelProperty, value ); }
		}
		public static readonly DependencyProperty CustomLabelProperty =
			DependencyProperty.Register( "CustomLabel", typeof( string ), typeof( AssetVisual ) );



		/// Thumbnail image
		public ImageSource Thumbnail
		{
			get { return (ImageSource)GetValue( ThumbnailProperty ); }
			set { SetValue( ThumbnailProperty, value ); }
		}
		public static readonly DependencyProperty ThumbnailProperty =
			DependencyProperty.Register( "Thumbnail", typeof( ImageSource ), typeof( AssetVisual ) );

		/// True when using a default thumbanil image
		private bool UsingDefaultThumbnail { get; set; }


		/// The "preferred size" of the thumbnail.  We cache this so that we can check to see if
		/// we need to "up-res" the thumbnail later.  Remember the actual size of the thumbnail may
		/// be much smaller!  This is simply the size we're requesting.
		public int PreferredThumbnailSize;
		
		/// The object type this visual refers to.
		private string ObjectTypeString;

		/// The label to display over the thumbnail. 
		public string ObjectTypeLabel
		{
			get { return (string)GetValue( ObjectTypeLabelProperty ); }
			set { SetValue( ObjectTypeLabelProperty, value ); }
		}
		public static readonly DependencyProperty ObjectTypeLabelProperty =
			DependencyProperty.Register( "ObjectTypeLabel", typeof( string ), typeof( AssetVisual ) );


		/// The warning label to display over the thumbnail. 
		public string WarningLabel
		{
			get { return (string)GetValue(WarningLabelProperty); }
			set { SetValue(WarningLabelProperty, value); }
		}
		public static readonly DependencyProperty WarningLabelProperty =
			DependencyProperty.Register("WarningLabel", typeof(string), typeof(AssetVisual));



		/// Border color for color-coding visuals based on the object's type
		public Color ObjectTypeHighlightColor
		{
			get { return (Color)GetValue( ObjectTypeHighlightColorProperty ); }
			set { SetValue( ObjectTypeHighlightColorProperty, value ); }
		}
		public static readonly DependencyProperty ObjectTypeHighlightColorProperty =
			DependencyProperty.Register( "ObjectTypeHighlightColor", typeof( Color ), typeof( AssetVisual ), new UIPropertyMetadata( Color.FromArgb( 200, 255, 255, 255 ) ) );

	
		
		/// Font size for Label
		public static readonly DependencyProperty DynamicLabelFontSizeProperty =
			DependencyProperty.Register( "DynamicLabelFontSize", typeof( double ), typeof( AssetVisual ) );

		
		/// Font size for ObjectTypeLabel
		public static readonly DependencyProperty DynamicObjectTypeLabelFontSizeProperty =
			DependencyProperty.Register( "DynamicObjectTypeLabelFontSize", typeof( double ), typeof( AssetVisual ) );


		/// Font size for WarningTypeLabel
		public static readonly DependencyProperty DynamicWarningLabelFontSizeProperty =
			DependencyProperty.Register("DynamicWarningLabelFontSize", typeof(double), typeof(AssetVisual));



		/// True if we want the "Load Asset" button to be visible and interactive
		public bool ShouldShowLoadAssetButton
		{
			get { return (bool)GetValue( ShouldShowLoadAssetButtonProperty ); }
			set { SetValue( ShouldShowLoadAssetButtonProperty, value ); }
		}
		public static readonly DependencyProperty ShouldShowLoadAssetButtonProperty =
			DependencyProperty.Register( "ShouldShowLoadAssetButton", typeof( bool ), typeof( AssetVisual ) );




		/// True if the object is 'verified', meaning the checkpoint commandlet has blessed it yet (or we're
		/// pretty sure for some other reason that the asset really exists)
		public bool IsVerified
		{
			get { return (bool)GetValue( IsVerifiedProperty ); }
			set { SetValue( IsVerifiedProperty, value ); }
		}
		public static readonly DependencyProperty IsVerifiedProperty =
			DependencyProperty.Register( "IsVerified", typeof( bool ), typeof( AssetVisual ) );


		
		/// The StrokeDashArray to be used for rendering the selection. This is a dependency property.
		public DoubleCollection SelectionStrokeDashArray
		{
			get { return (DoubleCollection)GetValue( SelectionStrokeDashArrayProperty ); }
			set { SetValue( SelectionStrokeDashArrayProperty, value ); }
		}
		public static readonly DependencyProperty SelectionStrokeDashArrayProperty =
			DependencyProperty.Register( "SelectionStrokeDashArray", typeof( DoubleCollection ), typeof( AssetVisual ), new UIPropertyMetadata( new DoubleCollection() ) );



		/// Is this AssetVisual under quarantine?
		public bool IsQuarantined
		{
			get { return (bool)GetValue(IsQuarantinedProperty); }
			set { SetValue(IsQuarantinedProperty, value); }
		}
		public static readonly DependencyProperty IsQuarantinedProperty =
			DependencyProperty.Register("IsQuarantined", typeof(bool), typeof(AssetVisual), new UIPropertyMetadata(false));







		
		/// True if we should try to refresh the thumbnail image when possible.  We don't
		/// want this set initially since we'd prefer to pull a thumbnail from our cache if
		/// at all possible.
		public ThumbnailGenerationPolicy ShouldRefreshThumbnail = ThumbnailGenerationPolicy.Never;


		/// The content browser control that's running the show
		private MainControl m_ContentBrowser;

		/// A reference to the LoadAsset button component of the AssetVisual template
		AssetVisualLoadAssetButton mPART_LoadAssetButton = null;
		




		/// <summary>
		/// Constructor
		/// </summary>
		public AssetVisual()
		{
			if( DesignerProperties.GetIsInDesignMode( this ) )
			{
				Label = "My Label";
				ObjectTypeLabel = "My Object Type";
				WarningLabel = "My Warning";
			}
			LoadAssetButtonPressedHandler = new MouseButtonEventHandler( mPART_LoadAssetButton_MouseLeftButtonDown );
		}



		/// <summary>
		/// Static constructor
		/// </summary>
		static AssetVisual()
		{
			// NOTE: This is required for WPF to load the style and control template from generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( AssetVisual ), new FrameworkPropertyMetadata( typeof( AssetVisual ) ) );
		}

		/// Called when a new template is applied to this AssetVisual.
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			if ( mPART_LoadAssetButton != null )
			{
				mPART_LoadAssetButton.MouseLeftButtonDown -= LoadAssetButtonPressedHandler;
			}

			mPART_LoadAssetButton = (AssetVisualLoadAssetButton)Template.FindName( "PART_LoadAssetButton", this );

			if ( mPART_LoadAssetButton != null )
			{
				mPART_LoadAssetButton.MouseLeftButtonDown += LoadAssetButtonPressedHandler;
			}
		}

		/// Handling for user clicking the LoadAsset Button in the AssetVisual.
		MouseButtonEventHandler LoadAssetButtonPressedHandler = null;
		void mPART_LoadAssetButton_MouseLeftButtonDown( object sender, MouseButtonEventArgs e )
		{
			bool bCtrlIsDown = ( Keyboard.Modifiers & ModifierKeys.Control ) != 0;
			bool bShiftIsDown = ( Keyboard.Modifiers & ModifierKeys.Shift ) != 0;

			if ( !( bCtrlIsDown || bShiftIsDown ) )
			{
				if ( !this.AssetItem.Selected )
				{
					m_ContentBrowser.AssetView.SetSelection( this.AssetItem );
				}

				// Load the object!
				this.m_ContentBrowser.Backend.LoadSelectedObjectsIfNeeded();
			}
			e.Handled = false;
		}



		/// <summary>
		/// Initialize the control.  Must be called after the control is created.
		/// </summary>
		/// <param name="InContentBrowser">Content browser object that owns us</param>
		public void Init( MainControl InContentBrowser )
		{
			m_ContentBrowser = InContentBrowser;


			// Bind the asset view's font size settings to this visual.  Now, when the asset canvas zoom level
			// changes, the visual will be updated automatically
			UnrealEd.Utils.CreateBinding( this, AssetVisual.DynamicLabelFontSizeProperty, InContentBrowser.AssetView.AssetCanvas, "AssetVisualDynamicLabelFontSize" );
			UnrealEd.Utils.CreateBinding( this, AssetVisual.DynamicObjectTypeLabelFontSizeProperty, InContentBrowser.AssetView.AssetCanvas, "AssetVisualDynamicObjectTypeLabelFontSize" );
			UnrealEd.Utils.CreateBinding(this, AssetVisual.DynamicWarningLabelFontSizeProperty, InContentBrowser.AssetView.AssetCanvas, "AssetVisualDynamicWarningLabelFontSize");

			// Bind properties for metrics.  Note that thumbnail dimensions are bound in .xaml.
			UnrealEd.Utils.CreateBinding( this, WidthProperty, m_ContentBrowser.AssetView.AssetCanvas, "VisualWidth" );
			UnrealEd.Utils.CreateBinding( this, HeightProperty, m_ContentBrowser.AssetView.AssetCanvas, "VisualHeight" );

			// Bind the selection dash array to that of the AssetCanvas so that we can display non-authoritative selection as a dashed selection border.
			UnrealEd.Utils.CreateBinding( this, SelectionStrokeDashArrayProperty, m_ContentBrowser.AssetView.AssetCanvas, "SelectionStrokeDashArray" );		



			// Register event handlers
			MouseEnter += new System.Windows.Input.MouseEventHandler( AssetVisual_MouseEnter );
			MouseLeave += new System.Windows.Input.MouseEventHandler( AssetVisual_MouseLeave );
		}


		/// <summary>
		/// Binds the asset visual to an asset item and prepares it for display
		/// </summary>
		/// <param name="InAssetItem">The asset item we're being created for</param>
		public void BindToAsset( AssetItem InAssetItem )
		{
			m_AssetItem = InAssetItem;


			// Bind the visual's label text
			UnrealEd.Utils.CreateBinding( this, LabelProperty, InAssetItem, "FormattedName" );


			// Set the "object type" string.
			ObjectTypeString = InAssetItem.AssetType;


			// Clear thumbnail for now
			Thumbnail = null;
			PreferredThumbnailSize = 0;


			// Setup the thumbnail image for this visual.  We don't want to generate a thumbnail image
			// yet, but if we have a thumbnail image cached we'll use that.
			ShouldRefreshThumbnail = ThumbnailGenerationPolicy.Never;
			UpdateThumbnail( 0, false );	// Preferred size, IsAnimated


			// Set the border color using the asset's type
			ObjectTypeHighlightColor = m_ContentBrowser.Backend.GetAssetVisualBorderColorForObjectTypeName( InAssetItem.AssetType );


			// Queue the visual for async thumbnail generation
			ShouldRefreshThumbnail = ThumbnailGenerationPolicy.OnlyIfNotInFreeList;


			// Update the visual state, too.  This shouldn't trigger any visual transitions since
			// we'll only be setting non-interactive states here.
			if( AssetItem.Selected )
			{
				VisualState = VisualStateType.Selected;
			}
			else
			{
				VisualState = VisualStateType.Default;
			}


			
			// Bind the first custom label to the asset visual's custom label property
			UnrealEd.Utils.CreateBinding( this, AssetVisual.CustomLabelProperty, InAssetItem, "CustomLabels[0]" );

			// Bind the last custom label to the asset visual's warning label property
			UnrealEd.Utils.CreateBinding(this, AssetVisual.WarningLabelProperty, InAssetItem, "CustomLabels[4]");

			// Bind the asset's "LoadedStatus" to the visibility of our "load asset" button
			ShouldShowLoadAssetButton = false;
			UnrealEd.Utils.CreateBinding( this, AssetVisual.ShouldShowLoadAssetButtonProperty, InAssetItem, "LoadedStatus", new AssetVisualValueConverters.AssetLoadedStatusToInvVisibilityBool() );

			// Bind the "IsVerified" state
			IsVerified = true;
			UnrealEd.Utils.CreateBinding( this, AssetVisual.IsVerifiedProperty, InAssetItem, "IsVerified" );

			// Bind the "IsQuarantined" state
			IsQuarantined = false;
			UnrealEd.Utils.CreateBinding( this, AssetVisual.IsQuarantinedProperty, InAssetItem, "IsQuarantined" );

			// Show self!
			Visibility = Visibility.Visible;
		}

		
		
		/// Clears out the asset visual so it can be used again later
		public void UnbindFromAsset()
		{
			// Hide self
			Visibility = Visibility.Hidden;

			// Clear bindings to the asset item
			UnrealEd.Utils.ClearBinding( this, AssetVisual.ShouldShowLoadAssetButtonProperty );
			UnrealEd.Utils.ClearBinding( this, AssetVisual.CustomLabelProperty );
			UnrealEd.Utils.ClearBinding( this, AssetVisual.IsVerifiedProperty );


			Thumbnail = null;
			PreferredThumbnailSize = 0;
			m_AssetItem = null;
			Label = "";
			ObjectTypeLabel = "";
			WarningLabel = "";
			ObjectTypeString = "";
			ShouldRefreshThumbnail = ThumbnailGenerationPolicy.Never;
			CustomLabel = "";
			

			if( VisualState == VisualStateType.Selected || VisualState == VisualStateType.SelectedInteractively )
			{
				VisualState = VisualStateType.Default;
			}
		}



		/// Called when the mouse enters the asset visual
		void AssetVisual_MouseEnter( object sender, System.Windows.Input.MouseEventArgs e )
		{
			m_ContentBrowser.StartAssetItemToolTip( AssetItem );
		}



		/// Called when the mouse leaves the asset visual
		void AssetVisual_MouseLeave( object sender, System.Windows.Input.MouseEventArgs e )
		{
			m_ContentBrowser.StopAssetItemToolTip();
		}


		/// <summary>
		/// Updates the thumbnail for this asset visual
		/// </summary>
		/// <param name="PreferredSize">Preferred resolution of the thumbnail or zero for "smallest possible"</param>
		/// <param name="IsAnimating">True if the thumbnail will be changing frequently</param>
		/// <returns>True if a thumbnail was generated (e.g. we did a lot of work)</returns>
		public bool UpdateThumbnail( int PreferredSize, bool IsAnimating )
		{
			bool bWasThumbnailGenerated = false;


			// Only grab cached thumbnail if we don't already have one or we were asked to refresh
			if( ShouldRefreshThumbnail != ThumbnailGenerationPolicy.Never ||
				Thumbnail == null ||
				UsingDefaultThumbnail )
			{
				// First check to see if we have a thumbnail in our cache.  Also, we'll remove it from the cache
				// so that it will be bumped to the end of the list when we add it again
				BitmapSource AssetThumbnail = null;
				ThumbnailFreeListEntry ThumbEntry = m_ContentBrowser.AssetView.AssetCanvas.PopSpecificThumbnailOffFreeList( m_AssetItem.FullName );
				if( ThumbEntry != null )
				{
					AssetThumbnail = ThumbEntry.Thumbnail;
				}

				// If we were asked to generate a thumbnail, we'll always do that, even if we found an
				// (outdated) cached copy of the thumbnail
				if( ShouldRefreshThumbnail == ThumbnailGenerationPolicy.Force ||
					( AssetThumbnail == null && ShouldRefreshThumbnail == ThumbnailGenerationPolicy.OnlyIfNotInFreeList ) )
				{
					// Try to generate a thumbnail for this asset
					bool FailedToLoadThumbnail = false;
					AssetThumbnail = m_ContentBrowser.Backend.GenerateThumbnailForAsset( m_AssetItem, out FailedToLoadThumbnail );
					if( AssetThumbnail != null )
					{
						bWasThumbnailGenerated = true;
						// Set the object type label to whatever type we have.
						ObjectTypeLabel = ObjectTypeString;
						UsingDefaultThumbnail = false;
					}
					else
					{
						// No true thumbnail, so use a stock resource
						// Get the border color for this asset type in case a new thumbnail has to be generated
						Color BorderColor = m_ContentBrowser.Backend.GetAssetVisualBorderColorForObjectTypeName(m_AssetItem.AssetType);
						Thumbnail = ThumbnailCache.Get().GetThumbnailForType(AssetItem.AssetType, AssetItem.IsArchetype, BorderColor);
						UsingDefaultThumbnail = true;
						// Don't display a label for this object type.  The thumbnail will display that information
						ObjectTypeLabel = "";
						PreferredThumbnailSize = 0;
					}


					// For unverified assets, if we know the object actually exists then we can remove the
					// "unverified" marking on the object.
					if( !AssetItem.IsVerified && !FailedToLoadThumbnail && bWasThumbnailGenerated )
					{
						AssetItem.IsVerified = true;
						m_ContentBrowser.Backend.LocallyRemoveUnverifiedTagFromAsset( AssetItem );
					}

					
					// If the asset is "unverified" and we couldn't load a thumbnail for it, we'll assume that
					// the object doesn't actually exist and we'll hide it from view.  We're essentially verifying
					// that it doesn't exist.
					if( !AssetItem.IsVerified && FailedToLoadThumbnail )
					{
						// Locally tag this asset as a ghost so that it won't be visible in the editor after
						// the next full asset view refresh.  
						m_ContentBrowser.Backend.LocallyTagAssetAsGhost( AssetItem );

						// Meanwhile, we'll remove the asset item from the asset view's list of displayed assets.
						m_ContentBrowser.MyAssets.Remove( AssetItem );
					}
				}


				if( AssetThumbnail != null )
				{
					// Also generate a *preview* thumbnail if we need that
					bool bWasPreviewThumbnailRendered = false;
					if ((ShouldRefreshThumbnail == ThumbnailGenerationPolicy.Preview && PreferredSize > 0 ) && !m_ContentBrowser.Backend.AssetUsesSharedThumbnail(AssetItem))
					{
						BitmapSource AssetPreviewThumbnail = m_ContentBrowser.Backend.GeneratePreviewThumbnailForAsset(
								m_AssetItem.FullName, PreferredSize, IsAnimating, AssetThumbnail );
						if( AssetPreviewThumbnail != null )
						{
							AssetThumbnail = AssetPreviewThumbnail;
							bWasPreviewThumbnailRendered = true;

		
							// @todo: For now we don't include preview thumbs as we don't want to hold up
							//     generation of cached thumbnails while a mouse-over is causing a preview
							//	   to be generated every tick
							bWasThumbnailGenerated = true;
						}
					}


					// If we didn't end up generating anything then just store the original preferred size
					if( ThumbEntry != null && !bWasPreviewThumbnailRendered )
					{
						PreferredSize = ThumbEntry.PreferredThumbnailSize;
					}

					// Add it to the thumbnail bitmap cache (end of the list == newest!)
					m_ContentBrowser.AssetView.AssetCanvas.PushThumbnailOntoFreeList(
						m_AssetItem.FullName, AssetThumbnail, PreferredSize );

					// Great, we have a real thumbnail to use!
					Thumbnail = AssetThumbnail;
					PreferredThumbnailSize = PreferredSize;
					// Set the object type label to whatever type we have.
					ObjectTypeLabel = ObjectTypeString;
				}
				else
				{
					// check whether this is a type that uses one of the shared static thumbnails
					bool bUsesSharedThumbnail = m_ContentBrowser.Backend.AssetUsesSharedThumbnail(AssetItem);
					if (bUsesSharedThumbnail)
					{
						// Get the border color for this asset type in case a new thumbnail has to be generated
						Color BorderColor = m_ContentBrowser.Backend.GetAssetVisualBorderColorForObjectTypeName(m_AssetItem.AssetType);
						Thumbnail = ThumbnailCache.Get().GetThumbnailForType(m_AssetItem.AssetType, m_AssetItem.IsArchetype, BorderColor);
						UsingDefaultThumbnail = true;
						// Don't display a label for this object type.  The thumbnail will display that information
						ObjectTypeLabel = "";
					}
				}

				// Done refreshing!
				ShouldRefreshThumbnail = ThumbnailGenerationPolicy.Never;
			}

			// Note that our bitmap source may be a different resolution than the image and WPF will
			// just do a filtered scale.  This is nice because if the user zooms in they'll still see
			// the higher resolution source data.


			return bWasThumbnailGenerated;
		}

	}




	/// <summary>
	/// Asset visual "load asset" button
	/// </summary>
	public class AssetVisualLoadAssetButton : ContentControl
	{
		/// <summary>
		/// Visual states
		/// </summary>
		public enum VisualStateType
		{
			/// Default visual state (not selected)
			Default,

			/// Deselected interactively by the user
			DeselectedInteractively,

			/// Selected
			Selected,

			/// Selected interactively by the user (using the mouse or keyboard)
			SelectedInteractively,
		}


		/// Current visual state of the asset visual
		public VisualStateType VisualState
		{
			get { return (VisualStateType)GetValue( m_VisualStateProperty ); }
			set { SetValue( m_VisualStateProperty, value ); }
		}
		public static readonly DependencyProperty m_VisualStateProperty =
			DependencyProperty.Register( "VisualState", typeof( VisualStateType ), typeof( AssetVisualLoadAssetButton ) );


		/// The asset visual that own's this button
		private AssetVisual m_AssetVisual;





		/// <summary>
		/// Constructor
		/// </summary>
		public AssetVisualLoadAssetButton()
		{
			if( DesignerProperties.GetIsInDesignMode( this ) )
			{
				// ...
			}
		}



		/// <summary>
		/// Static constructor
		/// </summary>
		static AssetVisualLoadAssetButton()
		{
			// NOTE: This is required for WPF to load the style and control template from generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( AssetVisualLoadAssetButton ), new FrameworkPropertyMetadata( typeof( AssetVisualLoadAssetButton ) ) );
		}



		/// <summary>
		/// Initialize the control.  Must be called after the control is created.
		/// </summary>
		/// <param name="InAssetVisual">Asset visual that owns this button</param>
		public void Init( AssetVisual InAssetVisual )
		{
			m_AssetVisual = InAssetVisual;

			VisualState = VisualStateType.Default;
		}

	}


	#region Value Converters for the asset visual

	namespace AssetVisualValueConverters
	{

		/// <summary>
		/// Converts an asset's "Loaded Status" to a bool indicating whether a button should be visible
		/// </summary>
		[ValueConversion( typeof( AssetItem.LoadedStatusType ), typeof( bool ) )]
		public class AssetLoadedStatusToInvVisibilityBool
			: IValueConverter
		{
			/// Converts from the source type to the target type
			public object Convert( object value, Type targetType, object parameter, CultureInfo culture )
			{
				AssetItem.LoadedStatusType LoadedStatus = (AssetItem.LoadedStatusType)value;
				return ( LoadedStatus != AssetItem.LoadedStatusType.NotLoaded ) ? false : true;
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
