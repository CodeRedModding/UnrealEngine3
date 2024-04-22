//=============================================================================
//	AssetCanvas.xaml.cs: Content browser asset canvas implementation
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


namespace ContentBrowser
{

	/// Asset canvas design constraints
	public static class AssetCanvasDefs
	{
		/// Width of an asset visual
		public static int DefaultVisualThumbnailWidth = 128;
		public static int ExtraVisualWidth = 4 /* Borders */ + 20; /* Margins */
		public static int DefaultVisualWidth = DefaultVisualThumbnailWidth + ExtraVisualWidth;

		/// Height of an asset visual
		public static int DefaultVisualThumbnailHeight = 128;
		public static int ExtraVisualHeight = 4 /* Borders */ + 46; /* Label */
		public static int DefaultVisualHeight = DefaultVisualThumbnailHeight + ExtraVisualHeight;

		/// Width and height of scrollbars in pixels
		public static int ScrollBarSize = 18;

		/// Horizontal spacing from the edge of the canvas content area to where visuals are placed
		public readonly static int CanvasHorizBorderSpacing = 6;

		/// Vertical spacing from the edge of the canvas content area to where visuals are placed
		public readonly static int CanvasVertBorderSpacing = 14;

		/// Horizontal spacing between items in pixels
		public readonly static int MinHorizSpacingBetweenVisuals = 0;

		/// Vertical spacing between items in pixels
		public readonly static int VertSpacingBetweenVisuals = 2;

		/// Maximum zoom value to wrap the asset canvas based on user zooming.  For example when set to 1.0, the
		/// canvas will wrap items when zoomed out beyond zoom=1.0, but not when zoomed in closer than that.  When
		/// set to zero we'll never wrap asset items based on zoom.  Set to double.MaxValue to always wrap.
		public readonly static double WrapCanvasZoomThreshold = 1.0;	//double.MaxValue;

		/// How closely to allow the user to zoom in (larger = closer)
		public readonly static double NearestZoomedInAmount = 20.0;

		/// How far to allow the user to zoom out (smaller = further)
		public readonly static double FurthestZoomedOutAmount = 1.0;	// 0.5

		/// Maximum number of cached thumbnails 
		public readonly static int MaxCachedThumbnails = 200;

		/// Maximum amount of memory used by high resolution thumbnail images before we'll start purging
		/// them from memory (in bytes)
		public readonly static int MaxHighResThumbnailMemory = 80 * 1024 * 1024;

		/// The default (normal) thumbnail resolution.  Important:  This must match the engine's default thumbnail
		/// size (in ThumbnailTools::GenerateAndCacheThumbnailForObject)
		public readonly static int NormalThumbnailResolution = 256;

		/// The max thumbnail resolution we'll ever try to render
		public readonly static int MaxThumbnailResolution = 2048;

		/// Maximum size of our asset visual free list.  Really this doesn't need to be much larger than
		/// twice the number of assets that may fit in a single row
		public readonly static int MaxAssetVisualsOnFreeList = 200;

		/// Number of milliseconds to allow for thumbnail generation each tick
		public readonly static int MaxMillisecondsPerUpdate = 1;

		/// Maximum time between ticks to use for physics-based animation (absorbs hitches)
		public readonly static double MaxQuantumForPhysics = 1.0 / 10.0;	// 100 ms
	}



	/// <summary>
	/// Thumbnail entry in the free list
	/// </summary>
	public class ThumbnailFreeListEntry
	{
		/// The full name of this asset (e.g: class package.[group.]name)
		public String AssetFullName;
		
		/// Thumbnail bitmap
		public BitmapSource	Thumbnail;

		/// The requested size of the thumbnail
		public int PreferredThumbnailSize;

		/** Constructor */
		public ThumbnailFreeListEntry( String InFullName, BitmapSource InThumb, int InPreferredSize )
		{
			AssetFullName = InFullName;
			Thumbnail = InThumb;
			PreferredThumbnailSize = InPreferredSize;
		}
	}


	/// <summary>
	/// AssetCanvas
	/// </summary>
	public partial class AssetCanvas : UserControl
	{
		/// Width and an asset visual
		public double VisualWidth
		{
			get { return (double)GetValue( VisualWidthProperty ); }
			set { SetValue( VisualWidthProperty, value ); }
		}
		public static readonly DependencyProperty VisualWidthProperty =
			DependencyProperty.Register( "VisualWidth", typeof( double ), typeof( AssetCanvas ) );


		/// Height of an asset visual
		public double VisualHeight
		{
			get { return (double)GetValue( VisualHeightProperty ); }
			set { SetValue( VisualHeightProperty, value ); }
		}
		public static readonly DependencyProperty VisualHeightProperty =
			DependencyProperty.Register( "VisualHeight", typeof( double ), typeof( AssetCanvas ) );

		
		/// Thumbnail width
		public double VisualThumbnailWidth
		{
			get { return (double)GetValue( VisualThumbnailWidthProperty ); }
			set { SetValue( VisualThumbnailWidthProperty, value ); }
		}
		public static readonly DependencyProperty VisualThumbnailWidthProperty =
			DependencyProperty.Register( "VisualThumbnailWidth", typeof( double ), typeof( AssetCanvas ) );

		
		/// Thumbnail height
		public double VisualThumbnailHeight
		{
			get { return (double)GetValue( VisualThumbnailHeightProperty ); }
			set { SetValue( VisualThumbnailHeightProperty, value ); }
		}
		public static readonly DependencyProperty VisualThumbnailHeightProperty =
			DependencyProperty.Register( "VisualThumbnailHeight", typeof( double ), typeof( AssetCanvas ) );


		/// Thumbnail highlight border thickness (for emphasizing object type color coding)
		public double VisualThumnbailHighlightBorderThickness
		{
			get { return (double)GetValue( VisualThumnbailHighlightBorderThicknessProperty ); }
			set { SetValue( VisualThumnbailHighlightBorderThicknessProperty, value ); }
		}
		public static readonly DependencyProperty VisualThumnbailHighlightBorderThicknessProperty =
			DependencyProperty.Register( "VisualThumnbailHighlightBorderThickness", typeof( double ), typeof( AssetCanvas ), new UIPropertyMetadata( 1.0 ) );

		/// Spring for animating thumbnail border thickness
		private UnrealEd.DoubleSpring m_VisualThumbnailHighlightBorderThicknessSpring = new UnrealEd.DoubleSpring( 1.0 );


		/// Dash pattern for rendering the asset selection; used to indicate whether the selection is authoritative or non-authoritative.
		public DoubleCollection SelectionStrokeDashArray
		{
			get { return (DoubleCollection)GetValue( SelectionStrokeDashArrayProperty ); }
			set { SetValue( SelectionStrokeDashArrayProperty, value ); }
		}
		public static readonly DependencyProperty SelectionStrokeDashArrayProperty =
			DependencyProperty.Register( "SelectionStrokeDashArray", typeof( DoubleCollection ), typeof( AssetCanvas ), new UIPropertyMetadata( new DoubleCollection() ) );


		


		/// Cached content browser reference
		private MainControl m_MainControl;

		/// The asset list view associated with this canvas.  We need an asset list as we use this as
		/// our data source, and always keep selection in sync.
		private AssetView m_AssetView;

		/// Access the asset canvas' scroll viewer
		public ScrollViewer AssetCanvasScrollViewer { get { return m_AssetCanvasScrollViewer; } }

		/// Access the asset canvas control
		public Canvas ItemCanvas { get { return m_ItemCanvas; } }

		/// Asset items that are currently placed on the asset canvas
		Dictionary<AssetItem, AssetItem> m_AssetItemsOnCanvas = new Dictionary<AssetItem, AssetItem>();
		public Dictionary<AssetItem, AssetItem> AssetItemsOnCanvas { get { return m_AssetItemsOnCanvas; } }

		/// The default thumbnail image
		public ImageSource DefaultThumbnailImageSource { get { return m_DefaultThumbnailImageSource; } }
		private ImageSource m_DefaultThumbnailImageSource;

		/// Cached map of thumbnails waiting to be cleaned up.  We'll keep thumbnails around for awhile
		/// after we're done displaying them in case they're needed again soon.  This dictionary maps
		/// case-insensitive asset full names to the thumbnail for that asset.
		OrderedDictionary ThumbnailFreeList = new OrderedDictionary( StringComparer.OrdinalIgnoreCase );

		/// How much memory is currently allocated for high res thumbnails
		int HighResThumbnailMemoryUsed = 0;

		/// True if we need to refresh the asset canvas (next tick)
		public bool NeedsVisualRefresh = false;

		/// Current zoom level for the asset canvas
		public double ZoomAmount
		{
			get { return m_ZoomAmount; }
		}
		private double m_ZoomAmount = 1.0;

		/// Time we last updated the asset canvas
		private DateTime m_LastCanvasUpdateTime = DateTime.MinValue;

		/// True if the user is panning the canvas view by dragging the mouse
		private bool m_bIsPanningCanvas = false;

		/// The time that the user last dragged the mouse to pan the canvas viewport
		private DateTime m_LastInteractivePanTime = DateTime.MinValue;

		/// How fast (and which direction) the asset canvas is currently panning
		private Point m_PanningVelocity = new Point( 0.0, 0.0 );

		/// How fast (and which direction) we're currently zooming the asset canvas
		private double m_ZoomVelocity = 0.0;

		/// Mouse position over the scroll viewer where the user last initiated a zoom action
		private Point m_ZoomMousePositionOverScrollViewer = new Point( 0.0, 0.0 );

		/// Position that we last clicked the mouse in the scroll viewer
		private Point m_ScrollViewerRightButtonDownPosition;

		/// Stored mouse cursor position while panning the view by dragging the mouse
		private Point m_ScrollViewerLastMousePosition;

		/// Stopwatch used for limiting the amount of work we perform per tick
		private Stopwatch m_UpdateStopwatch = new Stopwatch();

		/// List of asset visuals that are no longer in use and may be recycled in place
		List<AssetVisual> m_AssetVisualFreeList = new List<AssetVisual>();




		/// Font size for asset visual labels
		public static readonly DependencyProperty AssetVisualDynamicLabelFontSizeProperty =
			DependencyProperty.Register( "AssetVisualDynamicLabelFontSize", typeof( double ), typeof( AssetCanvas ) );

		/// Font size for asset visual object type labels
		public static readonly DependencyProperty AssetVisualDynamicObjectTypeLabelFontSizeProperty =
			DependencyProperty.Register( "AssetVisualDynamicObjectTypeLabelFontSize", typeof( double ), typeof( AssetCanvas ) );

		/// Font size for asset visual warning labels
		public static readonly DependencyProperty AssetVisualDynamicWarningLabelFontSizeProperty =
			DependencyProperty.Register("AssetVisualDynamicWarningLabelFontSize", typeof(double), typeof(AssetCanvas));



		/// <summary>
		/// Handler for an event that's fired when the zoom level changes
		/// </summary>
		/// <param name="OldZoomAmount">Old zoom level</param>
		/// <param name="NewZoomAmount">The new zoom level</param>
		public delegate void ZoomAmountChangedHandler( double OldZoomAmount, double NewZoomAmount );

		/// <summary>
		/// This event is fired when the canvas zoom level is changed
		/// </summary>
		public event ZoomAmountChangedHandler ZoomAmountChanged;

		/// A cursor that is display when the user is dragging the canvas.
		System.Windows.Input.Cursor GrabCursor;


		/// <summary>
		/// Constructor
		/// </summary>
        public AssetCanvas()
		{
			InitializeComponent();

			VisualWidth = AssetCanvasDefs.DefaultVisualWidth;
			VisualHeight = AssetCanvasDefs.DefaultVisualHeight;
			VisualThumbnailWidth = AssetCanvasDefs.DefaultVisualThumbnailWidth;
			VisualThumbnailHeight = AssetCanvasDefs.DefaultVisualThumbnailHeight;

			// Assign the context for the asset canvas.  This tells the asset view that it can find
			// data within the scope of it's viewmodel object.
			DataContext = this;

			/// The GrabCursor is stored in a textblock so that it can be placed in a resource dictionary.
			GrabCursor = ( (TextBlock)this.FindResource( "curGrab" ) ).Cursor;
		}


	
		/// <summary>
		/// Initialize the asset view
		/// </summary>
		/// <param name="InContentBrowser">Content browser that the asset view is associated with</param>
		public void Init( MainControl InContentBrowser, AssetView InAssociatedAssetView )
		{
			m_MainControl = InContentBrowser;
			m_AssetView = InAssociatedAssetView;


			// Setup horizontal scrollbar visibility.  We only need this if we're using a fixed-width canvas.
			AssetCanvasScrollViewer.HorizontalScrollBarVisibility =
				AssetCanvasDefs.WrapCanvasZoomThreshold == Double.MaxValue ? ScrollBarVisibility.Hidden : ScrollBarVisibility.Visible;


			// Canvas refresh events
			AssetCanvasScrollViewer.SizeChanged += new SizeChangedEventHandler( AssetCanvasScrollViewer_SizeChanged );
			AssetCanvasScrollViewer.ScrollChanged += new ScrollChangedEventHandler( AssetCanvasScrollViewer_ScrollChanged );

			// For panning
			AssetCanvasScrollViewer.PreviewMouseDown += new MouseButtonEventHandler( AssetCanvasScrollViewer_PreviewMouseButtonDown );
			AssetCanvasScrollViewer.PreviewMouseUp += new MouseButtonEventHandler( AssetCanvasScrollViewer_PreviewMouseButtonUp );
			AssetCanvasScrollViewer.PreviewMouseMove += new MouseEventHandler( AssetCanvasScrollViewer_PreviewMouseMove );

			// For asset canvas zooming
			AssetCanvasScrollViewer.PreviewMouseWheel += new MouseWheelEventHandler( AssetCanvasScrollViewer_MouseWheel );

			// For keyboard shortcut support (Ctrl + A to Select All, for example.)
			AssetCanvasScrollViewer.PreviewKeyDown += new KeyEventHandler( AssetCanvasScrollViewer_PreviewKeyDown );

			// For catching clicks on the asset canvas background
			m_BackgroundRectangle.MouseLeftButtonDown += new MouseButtonEventHandler( CanvasBackgroundRectangle_MouseLeftButtonDown );
			m_BackgroundRectangle.MouseRightButtonDown += new MouseButtonEventHandler( CanvasBackgroundRectangle_MouseRightButtonDown );


			// Load and cache our default thumbnail image
			if ( DesignerProperties.GetIsInDesignMode(this) )
			{
				m_DefaultThumbnailImageSource = (ImageSource)TryFindResource( "imgDefaultThumbnail" );
			}
			else
			{
				m_DefaultThumbnailImageSource = (ImageSource)FindResource("imgDefaultThumbnail");
			}


			// Refresh the asset canvas next tick
			NeedsVisualRefresh = true;
		}




		/// <summary>
		/// Asset canvas metrics
		/// </summary>
		public struct AssetCanvasMetrics
		{
			/// How many assets will fit on each row (minimum of 1)
			public int NumAssetsPerRow;

			/// Number of rows required for all of the assets
			public int NumRows;

			/// Width of the canvas to fit all of the assets
			public int CanvasWidth;

			/// Height of the canvas to fit all of the assets
			public int CanvasHeight;

			/// Dimensions of the visible region in canvas space (based on position of scroll viewer and zoom level)
			public double ViewWidth;
			public double ViewHeight;
			public double ViewLeft;
			public double ViewTop;
			public double ViewRight;
			public double ViewBottom;

			/// Dimensions of the scroll viewer region in canvas space (based on position of scroll viewer and zoom level)
			/// NOTE: These dimensions encapsulate the scroll bar area
			public double ScrollViewerViewWidth;
			public double ScrollViewerViewHeight;
			public double ScrollViewerViewLeft;
			public double ScrollViewerViewTop;
			public double ScrollViewerViewRight;
			public double ScrollViewerViewBottom;

			/// Extra horizontal margin before leftmost asset
			public int LeftmostAssetHorizMargin;

			/// Horizontal spacing between visuals
			public int HorizSpacingBetweenVisuals;
		}



		/// <summary>
		/// Computes information about the canvas dimensions and scrollable area
		/// </summary>
		/// <returns></returns>
		public AssetCanvasMetrics ComputeCanvasMetrics()
		{
			AssetCanvasMetrics Metrics = new AssetCanvasMetrics();


			int TotalCanvasHorizBorderSpace = AssetCanvasDefs.CanvasHorizBorderSpacing * 2;
			int TotalCanvasVertBorderSpace = AssetCanvasDefs.CanvasVertBorderSpacing * 2;


			double ScrollViewerAreaWidth = AssetCanvasScrollViewer.ActualWidth;
			if( AssetCanvasScrollViewer.HorizontalScrollBarVisibility != ScrollBarVisibility.Hidden )
			{
				ScrollViewerAreaWidth -= AssetCanvasDefs.ScrollBarSize;
			}
			double ScrollViewerAreaHeight = AssetCanvasScrollViewer.ActualHeight;
			if( AssetCanvasScrollViewer.VerticalScrollBarVisibility != ScrollBarVisibility.Hidden )
			{
				ScrollViewerAreaHeight -= AssetCanvasDefs.ScrollBarSize;
			}



			// Compute the number of assets we can fit in each row using the width of the control
			{
				double UsableCanvasWidth = ScrollViewerAreaWidth - TotalCanvasHorizBorderSpace;
				if( m_ZoomAmount < AssetCanvasDefs.WrapCanvasZoomThreshold )
				{
					UsableCanvasWidth /= m_ZoomAmount;
				}
				Metrics.NumAssetsPerRow =
					(int)( UsableCanvasWidth + AssetCanvasDefs.MinHorizSpacingBetweenVisuals ) /
						 ( (int)VisualWidth + AssetCanvasDefs.MinHorizSpacingBetweenVisuals );
				if( Metrics.NumAssetsPerRow < 1 )
				{
					Metrics.NumAssetsPerRow = 1;
				}
			}



			// Figure out how many rows we'll need to hold all of the content
			int AssetItemCount = m_AssetView.AssetListView.Items.Count;
			{
				Metrics.NumRows = AssetItemCount / Metrics.NumAssetsPerRow;
				if( ( AssetItemCount % Metrics.NumAssetsPerRow ) != 0 )
				{
					++Metrics.NumRows;
				}
			}



			// Compute the actual scrollable area width
			Metrics.CanvasWidth =
				TotalCanvasHorizBorderSpace +
				Metrics.NumAssetsPerRow * (int)VisualWidth +
				( Metrics.NumAssetsPerRow - 1 ) * AssetCanvasDefs.MinHorizSpacingBetweenVisuals;
			if( Metrics.CanvasWidth < 0 )
			{
				Metrics.CanvasWidth = 0;
			}


			// Compute the actual scrollable area height
			Metrics.CanvasHeight =
				TotalCanvasVertBorderSpace +
				Metrics.NumRows * (int)VisualHeight +
				( Metrics.NumRows - 1 ) * AssetCanvasDefs.VertSpacingBetweenVisuals;
			if( Metrics.CanvasHeight < 0 )
			{
				Metrics.CanvasHeight = 0;
			}



			// Setup dimensions of visible area in canvas spaces
			Metrics.ViewWidth = ScrollViewerAreaWidth / m_ZoomAmount;
			Metrics.ViewHeight = ScrollViewerAreaHeight / m_ZoomAmount;
			Metrics.ViewLeft = AssetCanvasScrollViewer.HorizontalOffset / m_ZoomAmount;
			Metrics.ViewTop = AssetCanvasScrollViewer.VerticalOffset / m_ZoomAmount;
			Metrics.ViewRight = Metrics.ViewLeft + Metrics.ViewWidth;
			Metrics.ViewBottom = Metrics.ViewTop + Metrics.ViewHeight;


			// Choose a horizontal spacing that distributes the visuals more evenly across the visible area
			Metrics.LeftmostAssetHorizMargin = 0;
			Metrics.HorizSpacingBetweenVisuals = AssetCanvasDefs.MinHorizSpacingBetweenVisuals;
			if( (int)Metrics.ViewWidth > Metrics.CanvasWidth )
			{
				// Only bother adjusting spacing if we actually have enough assets in a row
				if( AssetItemCount > Metrics.NumAssetsPerRow )
				{
					int LeftOverHorizPixels = (int)Metrics.ViewWidth - Metrics.CanvasWidth;
					Metrics.HorizSpacingBetweenVisuals += LeftOverHorizPixels / Metrics.NumAssetsPerRow;
					Metrics.LeftmostAssetHorizMargin = ( LeftOverHorizPixels / Metrics.NumAssetsPerRow ) / 2;

					// Now expand the canvas width to encapsulate the entire horizontal visible area.  We've chosen
					// a horizontal spacing for the asset visuals so that they'll consume the entire row now.
					Metrics.CanvasWidth = (int)Metrics.ViewWidth;
				}
			}

			
			// Also compute the same dimensions with the scrollbar area included.  We need this for zooming
			// since we zoom using the canvas's layout transform which underlaps the scroll bars.
			Metrics.ScrollViewerViewWidth = AssetCanvasScrollViewer.ActualWidth / m_ZoomAmount;
			Metrics.ScrollViewerViewHeight = AssetCanvasScrollViewer.ActualHeight / m_ZoomAmount;
			Metrics.ScrollViewerViewLeft = AssetCanvasScrollViewer.HorizontalOffset / m_ZoomAmount;
			Metrics.ScrollViewerViewTop = AssetCanvasScrollViewer.VerticalOffset / m_ZoomAmount;
			Metrics.ScrollViewerViewRight = Metrics.ScrollViewerViewLeft + Metrics.ScrollViewerViewWidth;
			Metrics.ScrollViewerViewBottom = Metrics.ScrollViewerViewTop + Metrics.ScrollViewerViewHeight;


			return Metrics;
		}




		/// Refreshes the asset canvas and creates visuals as needed
		public void RefreshAssetCanvas()
		{

			// Calculate the size of the viewable canvas area
			AssetCanvasMetrics Metrics = ComputeCanvasMetrics();



			// Set the horizontal scroll range (width) of the canvas
			ItemCanvas.Width = Metrics.CanvasWidth;

			// Set the vertical scroll range (height) of the canvas
			ItemCanvas.Height = Metrics.CanvasHeight;




			// Figure out where in our asset list to start iterating.  This allows us to avoid ever scanning
			// the entire list of assets, and instead we can focus only on those assets that are potentially visible!
			int FirstPotentiallyVisibleAssetIndex, LastPotentiallyVisibleAssetIndex;
			{
				// Compute the first potentially visible row/column...
				int FirstPotentiallyVisibleRow =
					( (int)Metrics.ViewTop - AssetCanvasDefs.CanvasVertBorderSpacing - AssetCanvasDefs.VertSpacingBetweenVisuals ) /
					( (int)VisualHeight + AssetCanvasDefs.VertSpacingBetweenVisuals );
				int FirstPotentiallyVisibleColumn =
					( (int)Metrics.ViewLeft - AssetCanvasDefs.CanvasHorizBorderSpacing - Metrics.LeftmostAssetHorizMargin - Metrics.HorizSpacingBetweenVisuals ) /
					( (int)VisualWidth + Metrics.HorizSpacingBetweenVisuals );
				FirstPotentiallyVisibleAssetIndex =
					Math.Max( 0, FirstPotentiallyVisibleRow * Metrics.NumAssetsPerRow + FirstPotentiallyVisibleColumn );

				// ...and last potentially visible row/column!
				int LastPotentiallyVisibleRow =
					( (int)Metrics.ViewBottom - AssetCanvasDefs.CanvasVertBorderSpacing - AssetCanvasDefs.VertSpacingBetweenVisuals ) /
					( (int)VisualHeight + AssetCanvasDefs.VertSpacingBetweenVisuals ) + 1;
				int LastPotentiallyVisibleColumn =
					( (int)Metrics.ViewRight - AssetCanvasDefs.CanvasHorizBorderSpacing - Metrics.LeftmostAssetHorizMargin - Metrics.HorizSpacingBetweenVisuals ) /
					( (int)VisualWidth + Metrics.HorizSpacingBetweenVisuals ) + 1;
				LastPotentiallyVisibleAssetIndex =
					Math.Min( m_AssetView.AssetListView.Items.Count - 1, LastPotentiallyVisibleRow * Metrics.NumAssetsPerRow + LastPotentiallyVisibleColumn );
			}


			// We'll build a map of all of the assets we want on the canvas
			Dictionary<AssetItem, AssetItem> NewAssetsOnCanvas = new Dictionary<AssetItem, AssetItem>();


			// Iterate over potentially-visible asset items
			for( int AssetItemIndex = FirstPotentiallyVisibleAssetIndex;
				 AssetItemIndex <= LastPotentiallyVisibleAssetIndex;
				 ++AssetItemIndex )
			{
				AssetItem CurAssetItem = (AssetItem)m_AssetView.AssetListView.Items[ AssetItemIndex ];


				// Compute the position of the current asset in canvas space
				int CurColumn = AssetItemIndex % Metrics.NumAssetsPerRow;
				int XPos = AssetCanvasDefs.CanvasHorizBorderSpacing + Metrics.LeftmostAssetHorizMargin + CurColumn * ( (int)VisualWidth + Metrics.HorizSpacingBetweenVisuals );

				int CurRow = AssetItemIndex / Metrics.NumAssetsPerRow;
				int YPos = AssetCanvasDefs.CanvasVertBorderSpacing + CurRow * ( (int)VisualHeight + AssetCanvasDefs.VertSpacingBetweenVisuals );


				// Cull the asset based on the viewable area.  Really, pretty much nothing should be culled out
				// since we've already reduced the range of assets we're iterating over to a minimal set
				if( XPos + VisualWidth >= Metrics.ViewLeft &&
					YPos + VisualHeight >= Metrics.ViewTop &&
					XPos <= Metrics.ViewRight &&
					YPos <= Metrics.ViewBottom )
				{

					// Only create a visual for this item on demand if we haven't already cached one
					if( CurAssetItem.Visual == null )
					{
						// Create the visual for this asset!
						CurAssetItem.Visual = CreateAssetVisual( CurAssetItem );
					}

					// Add to our list of assets that we definitely want to be visible
					NewAssetsOnCanvas.Add( CurAssetItem, CurAssetItem );

					// Add the item to the canvas if it's not already on there
					if( !m_AssetItemsOnCanvas.ContainsKey( CurAssetItem ) )
					{
						if( !ItemCanvas.Children.Contains( CurAssetItem.Visual ) )
						{
							ItemCanvas.Children.Add( CurAssetItem.Visual );
						}
						CurAssetItem.Visual.Visibility = Visibility.Visible;
					}

					// Set the position of this item on the canvas
					if( Canvas.GetLeft( CurAssetItem.Visual ) != XPos )
					{
						Canvas.SetLeft( CurAssetItem.Visual, XPos );
					}
					if( Canvas.GetTop( CurAssetItem.Visual ) != YPos )
					{
						Canvas.SetTop( CurAssetItem.Visual, YPos );
					}
				}
			}


			// Remove visuals for any asset items that are no longer on the canvas
			foreach( AssetItem CurAssetItem in m_AssetItemsOnCanvas.Keys )
			{
				if( !NewAssetsOnCanvas.ContainsKey( CurAssetItem ) )
				{
					if( CurAssetItem.Visual != null )
					{
						// Hide the asset.  Removing it outright is too slow in WPF.
						CurAssetItem.Visual.Visibility = Visibility.Hidden;

						// Unbind from the asset item
						CurAssetItem.Visual.UnbindFromAsset();

						// Add the visual to our free list unless it already has a lot of visuals in it
						if( m_AssetVisualFreeList.Count < AssetCanvasDefs.MaxAssetVisualsOnFreeList )
						{
							m_AssetVisualFreeList.Add( CurAssetItem.Visual );
						}
						else
						{
							// OK the free list is full so go ahead and remove the asset visual
							ItemCanvas.Children.Remove( CurAssetItem.Visual );
						}
						CurAssetItem.Visual = null;
					}
				}
			}


			// Update our cached list of currently-displayed assets.  This will make it faster to add/remove
			// items that next time we do an update.
			m_AssetItemsOnCanvas = NewAssetsOnCanvas;


			// Purge high resolution thumbnails if we're over our memory budget
			while( HighResThumbnailMemoryUsed > AssetCanvasDefs.MaxHighResThumbnailMemory )
			{
				// Older thumbnails are at the top of the list
				for( int CurThumbIndex = 0; CurThumbIndex < ThumbnailFreeList.Count; ++CurThumbIndex )
				{
					ThumbnailFreeListEntry CurEntry = (ThumbnailFreeListEntry)ThumbnailFreeList[ CurThumbIndex ];
					ImageSource CurThumbnail = CurEntry.Thumbnail;

					// Is this a high resolution thumbnail?
					if( (int)CurThumbnail.Width > AssetCanvasDefs.NormalThumbnailResolution ||
						(int)CurThumbnail.Height > AssetCanvasDefs.NormalThumbnailResolution )
					{
						// Kill it!
						ThumbnailFreeListEntry PurgeItem = PopThumbnailAtIndexOffFreeList( CurThumbIndex );
						if ( PurgeItem != null )
						{
							// notify native code that it should unload any cached thumbnails
							m_MainControl.Backend.ClearCachedThumbnailForAsset( PurgeItem.AssetFullName );
						}

						// Have we free'd up enough memory yet?
						if( HighResThumbnailMemoryUsed <= AssetCanvasDefs.MaxHighResThumbnailMemory )
						{
							break;
						}

						--CurThumbIndex;
					}
				}
			}


			// Purge the oldest entries from our thumbnail cache.  We'll limit the size of the freelist
			// using the fixed freelist maximum size plus the number of currently visible assets
			int MaxAllowedThumbs = AssetCanvasDefs.MaxCachedThumbnails + NewAssetsOnCanvas.Count;
			while( ThumbnailFreeList.Count > MaxAllowedThumbs )
			{
				// Oldest thumbnails are always at the top of the list
				ThumbnailFreeListEntry PurgeItem = PopFirstThumbnailOffFreeList();
				if ( PurgeItem != null )
				{
					// notify native code that it should unload any cached thumbnails
					m_MainControl.Backend.ClearCachedThumbnailForAsset( PurgeItem.AssetFullName );
				}
			}


			double ScrollViewerAreaWidth = AssetCanvasScrollViewer.ActualWidth;
			if( AssetCanvasScrollViewer.HorizontalScrollBarVisibility != ScrollBarVisibility.Hidden )
			{
				ScrollViewerAreaWidth -= AssetCanvasDefs.ScrollBarSize;
			}
			double ScrollViewerAreaHeight = AssetCanvasScrollViewer.ActualHeight;
			if( AssetCanvasScrollViewer.VerticalScrollBarVisibility != ScrollBarVisibility.Hidden )
			{
				ScrollViewerAreaHeight -= AssetCanvasDefs.ScrollBarSize;
			}




			double CanvasBackgroundWidth = Math.Max( Metrics.CanvasWidth, ScrollViewerAreaWidth / m_ZoomAmount );
			double CanvasBackgroundHeight = Math.Max( Metrics.CanvasHeight, ScrollViewerAreaHeight / m_ZoomAmount );

			// Fit the canvas background exactly to the visible area of the scroll viewer
			m_BackgroundRectangle.Width = CanvasBackgroundWidth;
			m_BackgroundRectangle.Height = CanvasBackgroundHeight;


			// Update the background deco graphic
 			Canvas.SetLeft( m_BackgroundDecoImage, CanvasBackgroundWidth - m_BackgroundDecoImage.ActualWidth );
 			Canvas.SetTop( m_BackgroundDecoImage, CanvasBackgroundHeight - m_BackgroundDecoImage.ActualHeight );


			// Update the asset visual font sizes for the new zoom level
			UpdateAssetVisualFontSizes();


			// All refreshed, now!
			NeedsVisualRefresh = false;
		}


		/// Updates the canvas and thumbnails.  Should be called once per tick.
		public void UpdateAssetCanvas()
		{
			DateTime CurTime = DateTime.UtcNow;

			// Compute time interval
			TimeSpan TimeSinceLastUpdate = CurTime - m_LastCanvasUpdateTime;
			double Quantum = Math.Max( 0.0001, TimeSinceLastUpdate.TotalSeconds );


			// We'll track how much time we spend doing work here
			m_UpdateStopwatch.Reset();
			m_UpdateStopwatch.Start();

			// Refresh the asset view if we need to
			if( NeedsVisualRefresh )
			{
				RefreshAssetCanvas();
			}

			// Do we want to create a high resolution thumbnail for these assets?
			int CanvasPreferredThumbnailSize = 0;
			int VisibleThumbnailSize = Math.Min( AssetCanvasDefs.MaxThumbnailResolution, (int)( VisualThumbnailWidth * m_ZoomAmount ) );
			if( VisibleThumbnailSize > AssetCanvasDefs.NormalThumbnailResolution )
			{
				CanvasPreferredThumbnailSize = AssetCanvasDefs.MaxThumbnailResolution;

				// Snap the preferred size to a power of two
				while( CanvasPreferredThumbnailSize > AssetCanvasDefs.NormalThumbnailResolution &&
					   CanvasPreferredThumbnailSize / 2 >= VisibleThumbnailSize )
				{
					CanvasPreferredThumbnailSize /= 2;
				}
			}


			// First we'll generate a thumbnail for any assets that need on (unless we run out of time.)
			// Preview thumbnails will only be rendered after everything already has a regular thumbnail.
			bool bWasThumbnailGenerated = false;
			foreach( AssetItem CurAssetItem in m_AssetItemsOnCanvas.Keys )
			{
				if( CurAssetItem.Visual != null )
				{
					// Refresh the thumbnail!  (This may take awhile)
					bool IsPreviewAnimated = false;
					if( CurAssetItem.Visual.UpdateThumbnail( CanvasPreferredThumbnailSize, IsPreviewAnimated ) )
					{
						// OK, we generated at least one thumbnail.  Do we have time to do any more work
						// in the current tick?
						bWasThumbnailGenerated = true;
						if( m_UpdateStopwatch.ElapsedMilliseconds >= AssetCanvasDefs.MaxMillisecondsPerUpdate )
						{
							break;
						}
					}
				}
			}


			// Do we have time for any more thumbnails this tick?
			if( !bWasThumbnailGenerated ||
				m_UpdateStopwatch.ElapsedMilliseconds < AssetCanvasDefs.MaxMillisecondsPerUpdate )
			{
				// Update thumbnails
				foreach( AssetItem CurAssetItem in m_AssetItemsOnCanvas.Keys )
				{
					if( CurAssetItem.Visual != null )
					{
						// Only generate a "preview" thumbnail if we don't have any other (more important) refreshes
						// happening right now.
						bool IsPreviewAnimated = false;
						int ThumbnailPreferredPreviewSize = CanvasPreferredThumbnailSize;
						if( CurAssetItem.Visual.ShouldRefreshThumbnail == ThumbnailGenerationPolicy.Never )
						{
							// Only force a refresh if we're not currently in the middle of zooming as this
							// may cause a minor hitch that makes zooming feel pretty bad
							if( Math.Abs( m_ZoomVelocity ) < 0.1 )
							{
								// Check to see if we should refresh this thumbnail because we've zoomed in!
								if( CurAssetItem.LoadedStatus != AssetItem.LoadedStatusType.NotLoaded &&
									CurAssetItem.Visual.Thumbnail != null &&
									CurAssetItem.Visual.Thumbnail != DefaultThumbnailImageSource )
								{
									if( CurAssetItem.Visual.PreferredThumbnailSize < CanvasPreferredThumbnailSize )
									{
										// Force a refresh but don't bother dirtying the cached thumbnail since we only
										// care about the dynamic high resolution thumbnail
										CurAssetItem.Visual.ShouldRefreshThumbnail = ThumbnailGenerationPolicy.Preview;
									}


									// Is the mouse currently over this thumbnail?
									if( CurAssetItem.Visual.IsMouseOver )
									{
										// Don't animate thumbnails larger than 1024x1024 as the performance isn't
										// good enough for that right now.
										if( ThumbnailPreferredPreviewSize <= 1024 )
										{
											// Prepare to render an animated thumbnail
											CurAssetItem.Visual.ShouldRefreshThumbnail = ThumbnailGenerationPolicy.Preview;
											IsPreviewAnimated = true;
											if( ThumbnailPreferredPreviewSize == 0 )
											{
												ThumbnailPreferredPreviewSize = AssetCanvasDefs.NormalThumbnailResolution;
											}
										}
									}


									if( CurAssetItem.Visual.ShouldRefreshThumbnail == ThumbnailGenerationPolicy.Preview )
									{
										// Render a "preview" thumbnail
										if( CurAssetItem.Visual.UpdateThumbnail( ThumbnailPreferredPreviewSize, IsPreviewAnimated ) )
										{
											// OK, we generated at least one thumbnail.  Do we have time to do any more work
											// in the current tick?
											bWasThumbnailGenerated = true;
											if( m_UpdateStopwatch.ElapsedMilliseconds >= AssetCanvasDefs.MaxMillisecondsPerUpdate )
											{
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			m_UpdateStopwatch.Stop();

			// Apply scrolling/zooming physics
			UpdateCanvasScrollingAndZooming( Quantum );


			// Update thumbnail border size based on whether a key is held down
			{
				double NewBorderThickness = 1.0;
				if( m_AssetCanvasScrollViewer.IsMouseOver && Keyboard.IsKeyDown( Key.B ) )
				{
					NewBorderThickness = 4.0;
				}
				m_VisualThumbnailHighlightBorderThicknessSpring.TargetPosition = NewBorderThickness;

				// Update the spring simulation
				m_VisualThumbnailHighlightBorderThicknessSpring.Update( Quantum );

				if( m_VisualThumbnailHighlightBorderThicknessSpring.Position != VisualThumnbailHighlightBorderThickness )
				{
					VisualThumnbailHighlightBorderThickness = m_VisualThumbnailHighlightBorderThicknessSpring.Position;
				}
			}

			m_LastCanvasUpdateTime = CurTime;
		}



		/// <summary>
		/// Pushes a thumbnail onto the free list
		/// </summary>
		/// <param name="InFullName">Full name of the asset to add</param>
		/// <param name="InAssetThumbnail">The thumbnail image</param>
		public void PushThumbnailOntoFreeList( String InFullName, BitmapSource InAssetThumbnail, int PreferredThumbnailSize )
		{
			ThumbnailFreeListEntry NewEntry = new ThumbnailFreeListEntry( InFullName, InAssetThumbnail, PreferredThumbnailSize);

			// Add to the free list dictionary
			ThumbnailFreeList.Add( InFullName, NewEntry );

			
			// Update memory count
			if( InAssetThumbnail.Width > AssetCanvasDefs.NormalThumbnailResolution ||
				InAssetThumbnail.Height > AssetCanvasDefs.NormalThumbnailResolution )
			{
				int ImageSize = (int)InAssetThumbnail.Width * (int)InAssetThumbnail.Height * 4;
				HighResThumbnailMemoryUsed += ImageSize;
			}
		}



		/// <summary>
		/// Locates a thumbnail for the specified asset in the free list and removes it if found
		/// </summary>
		/// <param name="InFullName">Full name of the asset to remove</param>
		/// <returns>The image that was removed from the free list</returns>
		public ThumbnailFreeListEntry PopSpecificThumbnailOffFreeList( String InFullName )
		{
			ThumbnailFreeListEntry ThumbEntry = null;
			if( ThumbnailFreeList.Contains( InFullName ) )
			{
				ThumbEntry = (ThumbnailFreeListEntry)ThumbnailFreeList[ InFullName ];
				BitmapSource AssetThumbnail = ThumbEntry.Thumbnail;

				
				// Update memory count
				if( AssetThumbnail.Width > AssetCanvasDefs.NormalThumbnailResolution ||
					AssetThumbnail.Height > AssetCanvasDefs.NormalThumbnailResolution )
				{
					int ImageSize = (int)AssetThumbnail.Width * (int)AssetThumbnail.Height * 4;
					HighResThumbnailMemoryUsed -= ImageSize;
				}


				// Remove it from the cache so that it will be bumped to the end of the list when we
				// add it again
				ThumbnailFreeList.Remove( InFullName);
			}

			return ThumbEntry;
		}



		/// <summary>
		/// Removes a thumbnail by index
		/// </summary>
		/// <returns>The removed thumbnail</returns>
		public ThumbnailFreeListEntry PopThumbnailAtIndexOffFreeList( int ListIndex )
		{
			ThumbnailFreeListEntry ThumbEntry = null;
			if( ListIndex < ThumbnailFreeList.Count )
			{
				ThumbEntry = (ThumbnailFreeListEntry)ThumbnailFreeList[ ListIndex ];
				BitmapSource AssetThumbnail = ThumbEntry.Thumbnail;

				// Update memory count
				if( AssetThumbnail.Width > AssetCanvasDefs.NormalThumbnailResolution ||
					AssetThumbnail.Height > AssetCanvasDefs.NormalThumbnailResolution )
				{
					int ImageSize = (int)AssetThumbnail.Width * (int)AssetThumbnail.Height * 4;
					HighResThumbnailMemoryUsed -= ImageSize;
				}


				// Remove it!
				ThumbnailFreeList.RemoveAt( ListIndex );
			}

			return ThumbEntry;
		}

		
		
		/// <summary>
		/// Removes the first thumbnail in the free list
		/// </summary>
		/// <returns>The removed thumbnail</returns>
		public ThumbnailFreeListEntry PopFirstThumbnailOffFreeList()
		{
			if( ThumbnailFreeList.Count > 0 )
			{
				return PopThumbnailAtIndexOffFreeList( 0 );
			}

			return null;
		}

		
		
		/// Dampens a velocity using the specified settings and returns the result
		private double ComputeDampedVelocity( double Value, double Quantum, double Strength, double MinVelocityThreshold )
		{
			if( Value > 0.0 )
			{
				Value -= Value * Strength * Quantum;
				if( Value < MinVelocityThreshold )
				{
					Value = 0.0;
				}
			}
			else
			{
				Value -= Value * Strength * Quantum;
				if( Value > -MinVelocityThreshold )
				{
					Value = 0.0;
				}
			}

			return Value;
		}



		/// Checks to see if we have any scroll velocity and applies movement if needed
		private void UpdateCanvasScrollingAndZooming( double InQuantum )
		{
			// Never allow a time delta greater than 10Hz for scrolling/zooming animation.  This helps
			// avoid distracting jumps after an object loading hitch, etc.
			if( InQuantum > AssetCanvasDefs.MaxQuantumForPhysics )
			{
				InQuantum = AssetCanvasDefs.MaxQuantumForPhysics;
			}
			double MinVelocityThreshold = 0.1;

			// Update scrolling
			if( !m_bIsPanningCanvas )
			{
				if( Math.Abs( m_PanningVelocity.X ) > MinVelocityThreshold ||
					Math.Abs( m_PanningVelocity.Y ) > MinVelocityThreshold )
				{
					// Compute distance to scroll based on time passed and current velocity
					Point ScrollAmount = new Point();
					ScrollAmount.X = m_PanningVelocity.X * InQuantum;
					ScrollAmount.Y = m_PanningVelocity.Y * InQuantum;

					// Scroll the canvas!  Note that sub-pixel increments are allowed.
					AssetCanvasScrollViewer.ScrollToHorizontalOffset(
						AssetCanvasScrollViewer.HorizontalOffset + ScrollAmount.X );
					AssetCanvasScrollViewer.ScrollToVerticalOffset(
						AssetCanvasScrollViewer.VerticalOffset + ScrollAmount.Y );

					// Apply some friction to slow down the scrolling
					double DampingAmount = 4.0;
					m_PanningVelocity.X = ComputeDampedVelocity( m_PanningVelocity.X, InQuantum, DampingAmount, MinVelocityThreshold );
					m_PanningVelocity.Y = ComputeDampedVelocity( m_PanningVelocity.Y, InQuantum, DampingAmount, MinVelocityThreshold );
				}
			}



			// Update zooming
			if( Math.Abs( m_ZoomVelocity ) > 0.1 )
			{
				AssetCanvasMetrics OldMetrics = ComputeCanvasMetrics();


				// Apply zoom delta exponentially
				double OldZoomAmount = ZoomAmount;
				double NewZoomAmount = OldZoomAmount + ( m_ZoomVelocity * OldZoomAmount * InQuantum );
				ZoomTo( NewZoomAmount, m_ZoomMousePositionOverScrollViewer );


				// Apply some friction to slow down the zooming
				double DampingAmount = 12.0;
				m_ZoomVelocity = ComputeDampedVelocity( m_ZoomVelocity, InQuantum, DampingAmount, MinVelocityThreshold );
			}
		}



		/// <summary>
		/// Sets the zoom level and updates the asset canvas
		/// </summary>
		/// <param name="InNewZoomAmount">New zoom amount</param>
		/// <param name="ZoomTargetPointOverScrollViewer">Position over the scrollviewer to zoom to/from relative to</param>
		public void ZoomTo( double InNewZoomAmount, Point ZoomTargetPointOverScrollViewer )
		{
			if( InNewZoomAmount != m_ZoomAmount )
			{
				AssetCanvasMetrics OldMetrics = ComputeCanvasMetrics();


				// Store the new zoom level
				double OldZoomAmount = m_ZoomAmount;
				m_ZoomAmount = InNewZoomAmount;
				if( m_ZoomAmount < AssetCanvasDefs.FurthestZoomedOutAmount )
				{
					m_ZoomAmount = AssetCanvasDefs.FurthestZoomedOutAmount;
				}
				if( m_ZoomAmount > AssetCanvasDefs.NearestZoomedInAmount )
				{
					m_ZoomAmount = AssetCanvasDefs.NearestZoomedInAmount;
				}


				// Apply the transform
				ItemCanvas.LayoutTransform = new ScaleTransform( m_ZoomAmount, m_ZoomAmount );


				// Zoom to/from the mouse cursor's position.  We do this by automatically scrolling the view
				// to keep the cursor over the same position as we zoom.  Magic! :)
				{
					// We zoom using a layout transform which unfortunately includes the area beneath the
					// scroll viewer's scroll viewer, so we need to use the actual dimensions of the scroll
					// viewer for our calculations

					// Compute the new horizontal view range
					double CursorHorizPosOverCanvas = OldMetrics.ScrollViewerViewLeft + ( ZoomTargetPointOverScrollViewer.X / m_ZoomAmount );
					double CursorHorizPosScalar = ( CursorHorizPosOverCanvas - OldMetrics.ScrollViewerViewLeft ) / OldMetrics.ScrollViewerViewWidth;
					double NewViewWidth = AssetCanvasScrollViewer.ActualWidth / m_ZoomAmount;
					double NewViewLeft = CursorHorizPosOverCanvas - CursorHorizPosScalar * NewViewWidth;

					// Compute the new vertical view range
					double CursorVertPosOverCanvas = OldMetrics.ScrollViewerViewTop + ( ZoomTargetPointOverScrollViewer.Y / m_ZoomAmount );
					double CursorVertPosScalar = ( CursorVertPosOverCanvas - OldMetrics.ScrollViewerViewTop ) / OldMetrics.ScrollViewerViewHeight;
					double NewViewHeight = AssetCanvasScrollViewer.ActualHeight / m_ZoomAmount;
					double NewViewTop = CursorVertPosOverCanvas - CursorVertPosScalar * NewViewHeight;

					// Scroll the view to keep the cursor in place.  This will also trigger a refresh of the
					// asset canvas (potentially wrapping the asset visuals and moving things around.)
					AssetCanvasScrollViewer.ScrollToVerticalOffset( NewViewTop * m_ZoomAmount );
					AssetCanvasScrollViewer.ScrollToHorizontalOffset( NewViewLeft * m_ZoomAmount );
				}


				// Fire an event
				ZoomAmountChanged( OldZoomAmount, m_ZoomAmount );
			}
		}



		/// Updates the size of asset visual text for the current zoom level
		private void UpdateAssetVisualFontSizes()
		{
			int DefaultLabelFontSize = 11;
			int DefaultObjectTypeFontSize = 9;


			// If the thumbnail size is small, also shrink the text
			double LabelFontSize = DefaultLabelFontSize;
			double ObjectTypeFontSize = DefaultObjectTypeFontSize;
			if( VisualThumbnailWidth < AssetCanvasDefs.DefaultVisualThumbnailWidth &&
				VisualThumbnailHeight < AssetCanvasDefs.DefaultVisualThumbnailHeight )
			{
				LabelFontSize = 8;
				ObjectTypeFontSize = 7;
			}


			// Scale the label font up as we zoom in, but not too much
			if( m_ZoomAmount > 1.0 )
			{
				LabelFontSize /= Math.Min( m_ZoomAmount, 2.25 );
				ObjectTypeFontSize /= Math.Min( m_ZoomAmount, 2.25 );
			}


			// Note: When we set these properties, all asset visuals will be updated (through property bindings)
			SetValue( AssetVisualDynamicLabelFontSizeProperty, LabelFontSize );
			SetValue( AssetVisualDynamicObjectTypeLabelFontSizeProperty, ObjectTypeFontSize );
			SetValue( AssetVisualDynamicWarningLabelFontSizeProperty, ObjectTypeFontSize );
		}


		/// Scroll the thumbnails view to the very top.
		public void ScrollToTop()
		{
			this.AssetCanvasScrollViewer.ScrollToVerticalOffset(0);
		}


		/// Scrolls the asset canvas such that the specified asset is visible
		public void ScrollAssetCanvasToItem( AssetItem InAssetToScrollTo )
		{
			if( m_AssetView.AssetListView.Items.Count > 0 )
			{
				// Figure out where in the global (sorted) list this asset is
				int SortedAssetIndex = m_AssetView.AssetListView.Items.IndexOf( InAssetToScrollTo );
				if( SortedAssetIndex != -1 )
				{
					// Calculate the size of the viewable canvas area
					AssetCanvasMetrics Metrics = ComputeCanvasMetrics();

					// Don't bother scrolling if we're zoomed in so far that the asset is too big for the
					// view extents anyway.  This makes clicking on the asset less disruptive when zoomed in.
					if( Metrics.ViewWidth > VisualWidth &&
						Metrics.ViewHeight > VisualHeight )
					{
						int AssetColumn = SortedAssetIndex % Metrics.NumAssetsPerRow;
						int AssetRow = SortedAssetIndex / Metrics.NumAssetsPerRow;

						// Check to see if the desired item is already fully visible
						// Compute the first entirely visible row...
						int FirstEntirelyVisibleRow =
							(int)( ( Metrics.ViewTop - AssetCanvasDefs.CanvasVertBorderSpacing - AssetCanvasDefs.VertSpacingBetweenVisuals ) /
								   ( VisualHeight + AssetCanvasDefs.VertSpacingBetweenVisuals ) + 1.0 );
						int FirstEntirelyVisibleColumn =
							(int)( ( Metrics.ViewLeft - AssetCanvasDefs.CanvasHorizBorderSpacing - Metrics.LeftmostAssetHorizMargin - Metrics.HorizSpacingBetweenVisuals ) /
								   ( VisualWidth + Metrics.HorizSpacingBetweenVisuals ) + 1.0 );

						// ...and last entirely visible column!
						int LastEntirelyVisibleRow =
							( (int)Metrics.ViewBottom - AssetCanvasDefs.CanvasVertBorderSpacing - AssetCanvasDefs.VertSpacingBetweenVisuals ) /
							( (int)VisualHeight + AssetCanvasDefs.VertSpacingBetweenVisuals ) - 1;
						int LastEntirelyVisibleColumn =
							( (int)Metrics.ViewRight - AssetCanvasDefs.CanvasHorizBorderSpacing - Metrics.LeftmostAssetHorizMargin - Metrics.HorizSpacingBetweenVisuals ) /
							( (int)VisualWidth + Metrics.HorizSpacingBetweenVisuals ) - 1;


						// Is the asset currently out of view (even partially)?  If so, we'll need to scroll!

						bool bWasViewScrolled = false;

						if( AssetColumn < FirstEntirelyVisibleColumn || AssetColumn > LastEntirelyVisibleColumn )
						{
							// Compute the position of the current asset in canvas space
							int XPos = AssetCanvasDefs.CanvasHorizBorderSpacing + Metrics.LeftmostAssetHorizMargin + AssetColumn * ( (int)VisualWidth + Metrics.HorizSpacingBetweenVisuals );

							if( AssetColumn < FirstEntirelyVisibleColumn )
							{
								AssetCanvasScrollViewer.ScrollToHorizontalOffset( XPos * m_ZoomAmount );
							}
							else
							{
								AssetCanvasScrollViewer.ScrollToHorizontalOffset( ( XPos - Metrics.ViewWidth + VisualWidth ) * m_ZoomAmount );
							}

							bWasViewScrolled = true;
						}


						if( AssetRow < FirstEntirelyVisibleRow || AssetRow > LastEntirelyVisibleRow )
						{
							// Compute the position of the current asset in canvas space
							int YPos = AssetCanvasDefs.CanvasVertBorderSpacing + AssetRow * ( (int)VisualHeight + AssetCanvasDefs.VertSpacingBetweenVisuals );

							if( AssetRow < FirstEntirelyVisibleRow )
							{
								AssetCanvasScrollViewer.ScrollToVerticalOffset( YPos * m_ZoomAmount );
							}
							else
							{
								AssetCanvasScrollViewer.ScrollToVerticalOffset( ( YPos - Metrics.ViewHeight + VisualHeight ) * m_ZoomAmount );
							}

							bWasViewScrolled = true;
						}

						if( bWasViewScrolled )
						{
							// Kill scrolling velocity since we just teleported the view and we don't want it
							// to drift away.
							m_PanningVelocity.X = 0.0;
							m_PanningVelocity.Y = 0.0;
						}
					}
				}
			}

		}


		/// <summary>
		/// Creates a UI element to represent the specified asset item
		/// </summary>
		/// <param name="CurAssetItem">The asset item to create an visual for</param>
		/// <returns>The new asset visual UI element</returns>
		private AssetVisual CreateAssetVisual( AssetItem CurAssetItem )
		{
			AssetVisual NewAssetVisual = null;
			if( m_AssetVisualFreeList.Count > 0 )
			{
				// Recycle an asset visual from our free list
				int LastItemIndex = m_AssetVisualFreeList.Count - 1;
				NewAssetVisual = m_AssetVisualFreeList[ LastItemIndex ];
				m_AssetVisualFreeList.RemoveAt( LastItemIndex );
			}
			else
			{
				// Create a new visual
				NewAssetVisual = new AssetVisual();
				NewAssetVisual.Init( m_MainControl );

				// Listen to the left mouse button down event for selection
				NewAssetVisual.MouseLeftButtonDown += new MouseButtonEventHandler( AssetVisual_MouseLeftButtonDown );
				NewAssetVisual.MouseLeftButtonUp += new MouseButtonEventHandler( AssetVisual_MouseLeftButtonUp );
				NewAssetVisual.MouseMove += new MouseEventHandler( AssetVisual_MouseMove );
				NewAssetVisual.MouseRightButtonDown += new MouseButtonEventHandler( AssetVisual_MouseRightButtonDown );
				NewAssetVisual.MouseRightButtonUp += new MouseButtonEventHandler( AssetVisual_MouseRightButtonUp );
				NewAssetVisual.MouseDoubleClick += new MouseButtonEventHandler( AssetVisual_MouseDoubleClick );
			}

			
			// Bind the asset item to the asset visual.  This will setup the thumbnail graphic and labels.
			NewAssetVisual.BindToAsset( CurAssetItem );


			return NewAssetVisual;
		}

		
		
		
		/// Called when the left mouse button is clicked on the canvas background
		void CanvasBackgroundRectangle_MouseLeftButtonDown( object sender, MouseButtonEventArgs e )
		{
			// Canvas background was clicked, so allow the scrollviewer to steal keyboard focus
			Keyboard.Focus( AssetCanvasScrollViewer );


			// The user clicked on empty space on the canvas background, so unless the Control key is held down, go
			// ahead and clear the selected asset list.
			if( ( Keyboard.Modifiers & ModifierKeys.Control ) == 0 )
			{
				// Clear the current selection and select this asset
				m_AssetView.SetSelection(null);
			}


			e.Handled = true;
		}



		/// Called when the right mouse button is clicked on the canvas background
		void CanvasBackgroundRectangle_MouseRightButtonDown( object sender, MouseButtonEventArgs e )
		{
			// Clear the current selection
			m_AssetView.SetSelection(null);
			e.Handled = false;
		}



		/// Called when the asset canvas scrollviewer is scrolled
		void AssetCanvasScrollViewer_ScrollChanged( object sender, ScrollChangedEventArgs e )
		{
			// Scrolled position changed, so make sure the canvas data is up to date.  We need
			// to do this because the canvas items are virtualized; also we wrap items, etc.

			// Also, we want to do this immediately so that it doesn't feel lagged while scrolling

			RefreshAssetCanvas();
		}



		/// Called with the scroll viewer's size has changed
		void AssetCanvasScrollViewer_SizeChanged( object sender, SizeChangedEventArgs e )
		{
			// View has been resized, so make sure everything gets refreshed

			// We'll do this immediately so that it doesn't feel delayed while resizing

			RefreshAssetCanvas();
		}



		/// Called when a mouse button is clicked on the canvas background
		void AssetCanvasScrollViewer_PreviewMouseButtonDown( object sender, MouseButtonEventArgs e )
		{
			// Go ahead and take the keyboard focus
			Keyboard.Focus( AssetCanvasScrollViewer );

			if( e.ChangedButton == MouseButton.Right )
			{
				AssetCanvasScrollViewer.CaptureMouse();
				m_ScrollViewerRightButtonDownPosition = m_ScrollViewerLastMousePosition = e.GetPosition( AssetCanvasScrollViewer );

				// The canvas may be scrolling right now, but when the user right clicks on the canvas
				// we want to 'catch' the scrolling immediately, so we'll zero out the scrolling velocity
				m_PanningVelocity.X = 0.0;
				m_PanningVelocity.Y = 0.0;

				e.Handled = true;
			}
		}



		/// Called when a mouse button is released on the canvas background
		void AssetCanvasScrollViewer_PreviewMouseButtonUp( object sender, MouseButtonEventArgs e )
		{
			if( e.ChangedButton == MouseButton.Right )
			{
				AssetCanvasScrollViewer.ReleaseMouseCapture();
				if( m_bIsPanningCanvas )
				{
					m_bIsPanningCanvas = false;
					Mouse.OverrideCursor = null;


					// If the mouse was still when the user releases the button then make sure our
					// panning velocity is zero so that we don't continue scrolling
					DateTime CurTime = DateTime.UtcNow;
					TimeSpan TimeSinceLastInteractivePan = CurTime - m_LastInteractivePanTime;
					if( TimeSinceLastInteractivePan.TotalSeconds > 0.1 )
					{
						m_PanningVelocity.X = 0.0;
						m_PanningVelocity.Y = 0.0;
					}


					e.Handled = true;
				}
			}
		}


		/// Called when the mouse is moved over the scroll viewer
		void AssetCanvasScrollViewer_PreviewMouseMove( object sender, MouseEventArgs e )
		{
			// If the Shift key is held down we'll increase the scroll speed
			const double ScrollSpeedBoostCoefficient = 4.0;

			if( e.RightButton == MouseButtonState.Pressed && AssetCanvasScrollViewer.IsMouseCaptured && !m_AssetView.IsContextMenuOpen )
			{
				Point MousePosition = e.GetPosition( AssetCanvasScrollViewer );

				// First check to see if we've moved the mouse far enough from the click position to start panning.
				// This is so that it's still easy for the user to summon context menus by right clicking.
				Point AbsMouseDeltaSinceClick = new Point(
					Math.Abs( MousePosition.X - m_ScrollViewerRightButtonDownPosition.X ),
					Math.Abs( MousePosition.Y - m_ScrollViewerRightButtonDownPosition.Y ) );
				if( AbsMouseDeltaSinceClick.X > SystemParameters.MinimumHorizontalDragDistance ||
					AbsMouseDeltaSinceClick.Y > SystemParameters.MinimumVerticalDragDistance )
				{
					Point MouseDelta = new Point();
					MouseDelta.X = m_ScrollViewerLastMousePosition.X - MousePosition.X;
					MouseDelta.Y = m_ScrollViewerLastMousePosition.Y - MousePosition.Y;

					// Compute the scroll amount and apply a speed boost if the Control key is held
					Point ScrollAmount = MouseDelta;
					if( ( Keyboard.Modifiers & ModifierKeys.Shift ) != 0 )
					{
						ScrollAmount.X *= ScrollSpeedBoostCoefficient;
						ScrollAmount.Y *= ScrollSpeedBoostCoefficient;
					}

					if( ScrollAmount.X != 0 || ScrollAmount.Y != 0 )
					{
						AssetCanvasScrollViewer.ScrollToHorizontalOffset( AssetCanvasScrollViewer.HorizontalOffset + ScrollAmount.X );
						AssetCanvasScrollViewer.ScrollToVerticalOffset( AssetCanvasScrollViewer.VerticalOffset + ScrollAmount.Y );

						DateTime CurTime = DateTime.UtcNow;
						if( !m_bIsPanningCanvas )
						{
							// Start panning!
							m_bIsPanningCanvas = true;
							Mouse.OverrideCursor = GrabCursor;

							m_LastInteractivePanTime = CurTime;
						}


						// Compute the scroll velocity.  We'll need this when the user releases the mouse so
						// that we can retain the "escape velocity", allowing the user to "throw" the canvas scroller
						TimeSpan TimeSinceLastInteractivePan = CurTime - m_LastInteractivePanTime;
						double ClampedQuantum = Math.Max( 0.0001, TimeSinceLastInteractivePan.TotalSeconds );
						if( ClampedQuantum > AssetCanvasDefs.MaxQuantumForPhysics )
						{
							ClampedQuantum = AssetCanvasDefs.MaxQuantumForPhysics;
						}
						m_PanningVelocity.X = ScrollAmount.X / ClampedQuantum;
						m_PanningVelocity.Y = ScrollAmount.Y / ClampedQuantum;


						// Keep track of the last time that the user applied some mouse motion to the pan
						m_LastInteractivePanTime = CurTime;

						e.Handled = true;
					}
				}
			}
			else
			{
				Mouse.OverrideCursor = null;
			}

			m_ScrollViewerLastMousePosition = e.GetPosition( AssetCanvasScrollViewer );
		}



		/// Called when a mouse wheel event occurs over the asset canvas scroll viewer area (including the scroll bar)
		void AssetCanvasScrollViewer_MouseWheel( object sender, MouseWheelEventArgs e )
		{
			if( ( Keyboard.Modifiers & ModifierKeys.Control ) != 0 )
			{
				// Grab the mouse cursor's position over the scroll viewer
				m_ZoomMousePositionOverScrollViewer = e.GetPosition( AssetCanvasScrollViewer );

				// Apply some acceleration
				m_ZoomVelocity += e.Delta * 0.025;

				// Mark the event as handled so that scroll viewer doesn't get a chance to scroll the view
				e.Handled = true;
			}
			else
			{
				// User is using the mouse wheel to pan the canvas vertically, so clear out any existing motion 
				m_PanningVelocity.X = 0.0;
				m_PanningVelocity.Y = 0.0;
			}
		}



		/// Called when a keyboard button is pressed
		void AssetCanvasScrollViewer_PreviewKeyDown( object sender, KeyEventArgs e )
		{
			bool bIsControlKeyDown = (Keyboard.Modifiers & ModifierKeys.Control) == ModifierKeys.Control;
			bool bIsShiftKeyDown = (Keyboard.Modifiers&ModifierKeys.Shift) == ModifierKeys.Shift;

			bool bHandled = false;

			// Ctrl + A: Select all asset visuals
			Key key = e.Key;
			switch ( key )
			{
			case Key.Up:
			case Key.Down:
			case Key.Left:
			case Key.Right:
			case Key.Home:
			case Key.End:
				if ( mSelectionAnchor != null )
				{
					int SelectionAnchorIndex = m_AssetView.AssetListView.Items.IndexOf(mSelectionAnchor);
					int SelectionTailIndex = m_AssetView.AssetListView.Items.IndexOf(mSelectionTail);
					if ( SelectionTailIndex == -1 )
					{
						SelectionTailIndex = SelectionAnchorIndex;
					}

					if ( SelectionAnchorIndex != -1 )
					{
						bHandled = true;

						AssetCanvasMetrics Metrics = ComputeCanvasMetrics();

						int NewTailIndex = -1;
						switch ( key )
						{
						case Key.Up:
							NewTailIndex = Math.Max(0, SelectionTailIndex - Metrics.NumAssetsPerRow);
							break;

						case Key.Down:
							NewTailIndex = Math.Min(SelectionTailIndex + Metrics.NumAssetsPerRow, m_AssetView.AssetListView.Items.Count - 1);
							break;

						case Key.Left:
							NewTailIndex = Math.Max(0, SelectionTailIndex - 1);
							break;

						case Key.Right:
							NewTailIndex = Math.Min(SelectionTailIndex + 1, m_AssetView.AssetListView.Items.Count - 1);
							break;

						case Key.Home:
							NewTailIndex = 0;
							break;

						case Key.End:
							NewTailIndex = m_AssetView.AssetListView.Items.Count - 1;
							break;
						}

						if ( NewTailIndex >= 0 && NewTailIndex < m_AssetView.AssetListView.Items.Count && NewTailIndex != SelectionTailIndex )
						{
							bool bShiftPressed = (Keyboard.Modifiers & ModifierKeys.Shift) != 0;
							bool bCtrlPressed = (Keyboard.Modifiers & ModifierKeys.Control) != 0;

							AssetItem NewSelection = m_AssetView.AssetListView.Items[NewTailIndex] as AssetItem;
							if ( bShiftPressed )
							{
								List<AssetItem> AssetsToSelect = new List<AssetItem>();
								for ( int shiftIdx = Math.Min(SelectionAnchorIndex, NewTailIndex); shiftIdx <= Math.Max(SelectionAnchorIndex, NewTailIndex); shiftIdx++ )
								{
									AssetItem newItem = m_AssetView.AssetListView.Items[shiftIdx] as AssetItem;
									AssetsToSelect.Add(newItem);
								}

								// Select the assets!  If not pressing Control, clear out the selected items array
								m_AssetView.SelectMultipleAssets(AssetsToSelect, !bCtrlPressed);

								// Scroll the asset list and canvas such that the newly selected item is in view
								m_AssetView.SynchronizeViewToAsset(NewSelection);
							}
							else
							{
								// mSelectionAnchor always represents the item that would be the start of a shift-selection or drag-n-drop
								if ( bCtrlPressed )
								{
									if ( NewSelection.Selected )
									{
										if (SelectionTailIndex >= 0 && SelectionTailIndex < m_AssetView.AssetListView.Items.Count
										&&	Math.Abs(NewTailIndex - SelectionAnchorIndex) < Math.Abs(SelectionTailIndex - SelectionAnchorIndex))
										{
											m_AssetView.AssetListView.SelectedItems.Remove(m_AssetView.AssetListView.Items[SelectionTailIndex]);
										}
										else
										{
											// If the Control key is pressed then we'll just toggle the item's selection
											m_AssetView.AssetListView.SelectedItems.Remove(NewSelection);
										}
									}
									else
									{
										// If the Control key is pressed then we'll just toggle the item's selection
										m_AssetView.AssetListView.SelectedItems.Add(NewSelection);

										// Scroll the asset list view such that the clicked item is in view
										m_AssetView.SynchronizeViewToAsset(NewSelection);
									}
								}
								else
								{
									mSelectionAnchor = NewSelection;

									// make sure the list's "focused" item is in-sync with the starting item for shift-selection
									m_AssetView.SetSelection(NewSelection);

									// Scroll the asset list and canvas such that the newly selected item is in view
									m_AssetView.SynchronizeViewToAsset(NewSelection);
								}
							}
							mSelectionTail = NewSelection;
						}
					}
				}

				break;

            case Key.F4:
                // Display property window for the selected assets, if applicable
                m_MainControl.Backend.LoadAndDisplayPropertiesForSelectedAssets();
                break;

            case Key.Enter:
                // Execute the selected assets' default command (as if they had been double-clicked)
                m_MainControl.Backend.LoadAndActivateSelectedAssets();
                break;
			}

			e.Handled = bHandled;
		}

		/// The most recent AssetItem to be un/selected outside of shift-selection.  Used to determine which AssetItem should be used as the beginning of a shift-selection
		AssetItem mSelectionAnchor = null;
		public AssetItem SelectionAnchor { set { mSelectionAnchor = value; } }


		/// <summary>
		/// The AssetItem which was most recently selected as part of a shift-selection.  Used to determine how many items to select when performing shift-selection with the
		/// keyboard.
		/// </summary>
		AssetItem mSelectionTail = null;
		public AssetItem SelectionTail { set { mSelectionTail = value; } }

		/// The location where the mouse was pressed in the asset visual.
		/// This is used to determine how far we have dragged the mouse while panning.
		Point mAssetMouseDownLocation = new Point();

        /// Used to track if a new CTRL-based selection was made during the last mouse down.
        /// This allows CTRL selection to occur on mouse down, but CTRL deselection to occur on mouse up (to support drag-drop).
        bool m_bNewCtrlSelectionLastMouseDown = false;

		/// Called when the mouse button is pressed over an asset visual in the asset canvas
		void AssetVisual_MouseLeftButtonDown( object sender, System.Windows.Input.MouseButtonEventArgs e )
		{
            m_bNewCtrlSelectionLastMouseDown = false;
			mAssetMouseDownLocation = e.GetPosition( this );

			AssetVisual ClickedAssetVisual = (AssetVisual)sender;
			AssetItem ClickedAssetItem = ClickedAssetVisual.AssetItem;

			if( ClickedAssetItem != null )
			{
				// Go ahead and take the keyboard focus
				Keyboard.Focus( AssetCanvasScrollViewer );

				bool bShiftPressed = ( Keyboard.Modifiers & ModifierKeys.Shift ) != 0;
				bool bCtrlPressed = ( Keyboard.Modifiers & ModifierKeys.Control ) != 0;

				// If Shift is pressed, do shift-selection
				if( bShiftPressed )
				{
					// shift-selection starts with the last item that was clicked on without shift pressed
					ItemCollection SortedAssets = m_AssetView.AssetListView.Items;
					int SelectionStartIdx = SortedAssets.IndexOf( mSelectionAnchor );
					if( SelectionStartIdx == -1 )
					{
						// if user shift-clicked into the canvas and there weren't any selected items, select everything
						// from the first item up to the clicked item
						SelectionStartIdx = 0;
						mSelectionAnchor = ClickedAssetItem;
					}

					int SelectionEndIdx = SortedAssets.IndexOf( ClickedAssetItem );

					List<AssetItem> AssetsToSelect = new List<AssetItem>();
					for( int shiftIdx = Math.Min( SelectionStartIdx, SelectionEndIdx ); shiftIdx <= Math.Max( SelectionStartIdx, SelectionEndIdx ); shiftIdx++ )
					{
						AssetItem newItem = SortedAssets[ shiftIdx ] as AssetItem;
						AssetsToSelect.Add( newItem );
					}

					// Select the assets!  If not pressing Control, clear out the selected items array
					m_AssetView.SelectMultipleAssets( AssetsToSelect, !bCtrlPressed );
					m_AssetView.SynchronizeViewToAsset(ClickedAssetItem);

					mSelectionTail = ClickedAssetItem;
				}
				else
				{
					// mSelectionAnchor always represents the item that would be the start of a shift-selection or drag-n-drop
					mSelectionAnchor = ClickedAssetItem;
					mSelectionTail = ClickedAssetItem;
					if( ClickedAssetItem.Selected )
					{
						if ( !bCtrlPressed ) 
						{
							// OK the asset was already selected, but the editor's selection may not still
							// be in sync with what the user's seeing the Content Browser, so we'll force
							// the browser to sync the editor's selection set with ours.
							m_MainControl.Backend.SyncSelectedObjectsWithGlobalSelectionSet();

							// Scroll the asset list view such that the newly selected item is in view
							m_AssetView.SynchronizeViewToAsset(ClickedAssetItem);

							// If the Alt key is pressed then we'll "Apply" the object to the scene.  That is, for materials
							// we'll apply the material to the currently selected surface(s) in the level viewport
							if( ( Keyboard.Modifiers & ModifierKeys.Alt ) != 0 )
							{
								m_MainControl.Backend.ApplyAssetSelectionToViewport();
							}
						}
					}
					else
					{
						if( bCtrlPressed )
						{
							// If the Control key is pressed then we'll just toggle the item's selection
							m_AssetView.AssetListView.SelectedItems.Add( ClickedAssetItem );

							// Scroll the asset list view such that the clicked item is in view
							m_AssetView.SynchronizeViewToAsset(ClickedAssetItem);

                            // Signify that a new control selection occurred this mouse down (so mouse up doesn't immediately deselect it!)
                            m_bNewCtrlSelectionLastMouseDown = true;
						}
						else
						{
							m_AssetView.SetSelection(ClickedAssetItem);

							// Scroll the asset list view such that the newly selected item is in view
							m_AssetView.SynchronizeViewToAsset(ClickedAssetItem);

							// If the Alt key is pressed then we'll "Apply" the object to the scene.  That is, for materials
							// we'll apply the material to the currently selected surface(s) in the level viewport
							if( ( Keyboard.Modifiers & ModifierKeys.Alt ) != 0 )
							{
								m_MainControl.Backend.ApplyAssetSelectionToViewport();
							}
						}
					}
				}

				e.Handled = true;
			}
		}

		/// Called when the mouse button is released over an asset visual in the asset canvas
		void AssetVisual_MouseLeftButtonUp( object sender, MouseButtonEventArgs e )
		{
			m_AssetView.mAssetMouseDownLocation.X = 0;
            m_AssetView.mAssetMouseDownLocation.Y = 0;

			AssetVisual ClickedAssetVisual = (AssetVisual)sender;
			AssetItem ClickedAssetItem = ClickedAssetVisual.AssetItem;

			// Should never really be null, but it's possible in cases where the user clicks down on an asset,
			// then the asset visual scrolls out of view and gets recycled
			if( ClickedAssetItem != null )
			{
				bool bCtrlIsDown = ( Keyboard.Modifiers & ModifierKeys.Control ) != 0;
				bool bShiftIsDown = ( Keyboard.Modifiers & ModifierKeys.Shift ) != 0;

				if( ClickedAssetItem.Selected )
				{
                    if ( !bCtrlIsDown && !bShiftIsDown )
                    {
                        if (m_AssetView.AssetListView.SelectedItems.Count > 1)
                        {
                            m_AssetView.SetSelection(ClickedAssetItem);
                        }
                    }
                    else if ( bCtrlIsDown && !m_bNewCtrlSelectionLastMouseDown )
                    {
                        // If the Control key is pressed then we'll just toggle the item's selection
                        m_AssetView.AssetListView.SelectedItems.Remove(ClickedAssetItem);
                    }
				}
			}
		}



		/// Called when the mouse moves within an Asset Visual
		void AssetVisual_MouseMove( object sender, MouseEventArgs e )
		{
            m_AssetView.AttemptDragDrop( e.LeftButton, e.GetPosition( this ) );
		}


		/// Called when an asset visual is right clicked
		void AssetVisual_MouseRightButtonDown( object sender, MouseButtonEventArgs e )
		{
			if( !m_bIsPanningCanvas )
			{
				AssetVisual ClickedAssetVisual = (AssetVisual)sender;
				AssetItem ClickedAssetItem = ClickedAssetVisual.AssetItem;

				if ( ClickedAssetItem != null )
				{
					// Go ahead and take the keyboard focus
					Keyboard.Focus( AssetCanvasScrollViewer );


					// If the user right clicked on an already-selected item, then we'll retain the entire
					// selection set so they can batch process those items.  Otherwise we'll clear the selection
					// and select the item they right clicked on
					if ( !m_AssetView.AssetListView.SelectedItems.Contains( ClickedAssetItem ) )
					{
						// Clear the current selection and select this asset
						m_AssetView.SetSelection( ClickedAssetItem );
					}

					// Scroll the asset list view such that the newly selected item is in view
					m_AssetView.SynchronizeViewToAsset( ClickedAssetItem );
				}
			}
		}


		/// Called when an asset visual is right clicked
		void AssetVisual_MouseRightButtonUp( object sender, MouseButtonEventArgs e )
		{
			if( !m_bIsPanningCanvas )
			{
				AssetVisual ClickedAssetVisual = (AssetVisual)sender;
				AssetItem ClickedAssetItem = ClickedAssetVisual.AssetItem;

				// Should never really be null, but it's possible in cases where the user clicks down on an asset,
				// then the asset visual scrolls out of view and gets recycled
				if( ClickedAssetItem != null )
				{
					// Go ahead and take the keyboard focus
					Keyboard.Focus( AssetCanvasScrollViewer );


					// If the user right clicked on an already-selected item, then we'll retain the entire
					// selection set so they can batch process those items.  Otherwise we'll clear the selection
					// and select the item they right clicked on
					if( !m_AssetView.AssetListView.SelectedItems.Contains( ClickedAssetItem ) )
					{
						// Clear the current selection and select this asset
						m_AssetView.SetSelection(ClickedAssetItem);
					}
				}
			}
		}



		/// Called when the user double clicks on a visual
		void AssetVisual_MouseDoubleClick( object sender, MouseButtonEventArgs e )
		{
			AssetVisual ClickedAssetVisual = (AssetVisual)sender;
			AssetItem ClickedAssetItem = ClickedAssetVisual.AssetItem;

            bool bAltIsDown = (Keyboard.Modifiers & ModifierKeys.Alt) != 0;

			if ( ClickedAssetItem != null )
			{
                if (bAltIsDown)
                {
                    // Create a list of item classes from the selected items
                    List<String> SelectedItemClasses = new List<string>();
                    foreach (AssetItem Item in m_AssetView.AssetListView.SelectedItems)
                    {
                        if (!SelectedItemClasses.Contains(Item.AssetType))
                        {
                            SelectedItemClasses.Add(Item.AssetType);
                        }
                    }

                    // Get our new items from the list of selected classes
                    List<String> NewSelectedOptions = m_MainControl.Backend.GetBrowsableTypeNameList(SelectedItemClasses);

                    // Adjust set the selected options using our new list
                    m_MainControl.FilterPanel.mObjectTypeFilterTier.SetSelectedOptions(NewSelectedOptions);
                }
                // Otherwise, behave as normal
                else
                {
                    if (m_AssetView.AssetListView.SelectedItems.Count > 1)
                    {
                        // Clear the current selection and select this asset
                        m_AssetView.SetSelection(ClickedAssetItem);
                    }

                    // Scroll the asset list view such that the newly selected item is in view
                    m_AssetView.SynchronizeViewToAsset(ClickedAssetItem);

                    // Let the backend deal with it from here
                    m_MainControl.Backend.LoadAndActivateSelectedAssets();

                    // Update the loaded status
                    m_MainControl.Backend.UpdateAssetStatus(ClickedAssetItem, AssetStatusUpdateFlags.LoadedStatus);
                }
			}


			e.Handled = true;
		}

		/// <summary>
		/// Tell the canvas whether to draw its selection to appear authoritative or non-authoritative (i.e. solid border vs. dotted)
		/// </summary>
		/// <param name="ShouldAppearAuthoritative">Should be true for solid border</param>
		public void SetSelectionAppearsAuthoritative( bool ShouldAppearAuthoritative )
		{
			if ( ShouldAppearAuthoritative )
			{
				this.SelectionStrokeDashArray = (DoubleCollection)this.Resources["AuthoritativeDashPattern"];
			}
			else
			{
				this.SelectionStrokeDashArray = (DoubleCollection)this.Resources["NonAuthoritativeDashPattern"];
			}
		}

	}

}
