/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using UnrealEd;

namespace CustomControls
{
	/// <summary>
	/// The CellSizer is used to resize and collapse the contents of cells in a Grid/Dock control.
	///
	/// GOAL
	/// The goal is to support both user-resizing and collapsing of controls in a Grid/Dock.
	/// 
	/// PROBLEM: WPF's prescribed approach to resizing Grid rows and columns is via the GridSplitter control.
	/// When the user drags a GridSplitter, the Grid will set the row heights and column widths to
	/// user-specified values (constants).
	/// Thus, if the contents of a cell were to be collapsed the Grid would not adjust. The space
	/// previously occupied by the collapsed control would appear empty.
	/// 
	/// SOLUTION: Use a Grid with all row and column dimensions set to AUTO and use
	/// CellSizer to resize the contents of individual cells.
	/// 
	/// NOTE: The CellSizer is not always the right solution. There are some cases when a GridSplitter works better.
	/// Use your judgement; look at examples.
	/// 
	/// NOTE: Despite deriving from HeaderedContentControl the CellSizer does not actually use the Header.
	/// 
	/// </summary>
	/// <example>
	/// <Grid>
	///    <Grid.ColumnDefinitions>
	///        <ColumnDefinition Width="Auto" />
	///        <ColumnDefinition Width="Auto" />
	///    </Grid.ColumnDefinitions>
	///    <Border x:Name="mColumnA"  Grid.Column="0" >
	///        <local:CellSizer ResizeDirection="East" GripSize="5" MinWidth="100" Width="150" MaxWidth="250">
	///                <Label>COLUMN A</Label>
	///        </local:CellSizer>
	///    </Border>
	///    <Border Grid.Column="1" >
	///        <StackPanel>
	///            <Label>COLUMN B</Label>
	///            <Button x:Name="mToggelA" Click="mToggelA_Click">Toggle A</Button>
	///        </StackPanel>
	///    </Border>
	/// </Grid>
	/// 
	/// void mToggelA_Click( object sender, RoutedEventArgs e )
	///	{
	///		// Toggle visibility
	///		mColumnA.Visibility = ( mColumnA.Visibility == Visibility.Visible ) ? Visibility.Collapsed : Visibility.Visible;
	///	}
	/// 
	/// </example>
	/// 
	/// 
	[TemplatePart( Name = "PART_N", Type = typeof( Thumb ) )]
	[TemplatePart( Name = "PART_W", Type = typeof( Thumb ) )]
	[TemplatePart( Name = "PART_E", Type = typeof( Thumb ) )]
	[TemplatePart( Name = "PART_S", Type = typeof( Thumb ) )]
	public class CellSizer : System.Windows.Controls.HeaderedContentControl
	{
		/// The four cardinal directions; used to specify edges of the control.
		public enum Direction
		{
			North,
			West,
			East,
			South,
			MAX_Direction
		}

		static CellSizer()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml
			DefaultStyleKeyProperty.OverrideMetadata( typeof( CellSizer ), new FrameworkPropertyMetadata( typeof( CellSizer ) ) );
		}

		/// Create a new CellSizer
		public CellSizer()
		{
			// Create handlers for users dragging thumbnails
			mNorthDragHandler = new DragDeltaEventHandler( mPart_NorthDragHandler );
			mWestDragHandler = new DragDeltaEventHandler( mPart_WestDragHandler );
			mEastDragHandler = new DragDeltaEventHandler( mPart_EastDragHandler );
			mSouthDragHandler = new DragDeltaEventHandler( mPart_SouthDragHandler );

			this.IsVisibleChanged += new DependencyPropertyChangedEventHandler( CellSizer_IsVisibleChanged );
		}

		void CellSizer_IsVisibleChanged( object sender, DependencyPropertyChangedEventArgs e )
		{
			UpdateGripVisibilityAndControlSizing();
		}

		public Size SavedSize { get; set; }

		public delegate void SizeAlteredHandler( CellSizer Sender, Size OldSize );
		

		public event SizeAlteredHandler CollapsedToggled;
		protected void RaiseCollapsedToggled( Size OldSize )
		{
			if ( CollapsedToggled != null )
			{
				CollapsedToggled( this, OldSize );
			}
		}

		
		public event SizeAlteredHandler ResizedByUser;
		protected void RaiseResizedByUser( Size OldSize )
		{
			if ( ResizedByUser != null )
			{
				ResizedByUser( this, OldSize );
			}
		}


		#region ResizeDirectionProperty
		
		/// Gets or sets the direction in which this control is to be resized (NORTH, EAST, SOUTH, WEST)
		public Direction ResizeDirection
		{
			get { return (Direction)GetValue( ResizeDirectionProperty ); }
			set { SetValue( ResizeDirectionProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for ResizeDirection.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty ResizeDirectionProperty =
			DependencyProperty.Register( "ResizeDirection", typeof( Direction ), typeof( CellSizer ), new UIPropertyMetadata( new PropertyChangedCallback( ResizeDirectionPropertyChanged ) ) );

		static void ResizeDirectionPropertyChanged( DependencyObject Sender, DependencyPropertyChangedEventArgs Args )
		{
			CellSizer This = Sender as CellSizer;
			
			// Hide the old grip
			Direction OldResizeDirection = (Direction)Args.OldValue;
			Thumb OldActiveGrip = This.GetGripControl( OldResizeDirection );
			if ( OldActiveGrip != null )
			{
				OldActiveGrip.Visibility = Visibility.Collapsed;
			}
			
			// Show the new grip and attach a handler for it
			Direction NewResizeDirection = (Direction)Args.NewValue;
			Thumb ActiveGrip = This.GetGripControl( NewResizeDirection );
			if ( ActiveGrip != null )
			{
				ActiveGrip.Visibility = Visibility.Visible;
				This.AttachHandlerToGrip( ActiveGrip );
			}
		}

		#endregion
		
		#region GripSizeProperty

		/// Gets or sets the thickness of the grip in pixels
		public double GripSize
		{
			get { return (double)GetValue( GripSizeProperty ); }
			set { SetValue( GripSizeProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for GripSize.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty GripSizeProperty =
			DependencyProperty.Register( "GripSize", typeof( double ), typeof( CellSizer ), new UIPropertyMetadata( 5.0 ) );

		#endregion


		#region IsCollapsedProperty
		/// Whether the CellSizer should appear collapsed. The collapse animation is based on the resize direction. This is a dependency property.
		public bool IsCollapsed
		{
			get { return (bool)GetValue( IsCollapsedProperty ); }
			set { SetValue( IsCollapsedProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for IsCollapsed.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsCollapsedProperty =
			DependencyProperty.Register( "IsCollapsed", typeof( bool ), typeof( CellSizer ), new FrameworkPropertyMetadata( false, FrameworkPropertyMetadataOptions.AffectsParentArrange, OnIsCollapsedPropertyChanged ) );

		static void OnIsCollapsedPropertyChanged( DependencyObject Sender, DependencyPropertyChangedEventArgs EventArgs )
		{
			CellSizer This = Sender as CellSizer;
			This.OnIsCollapsedPropertyChanged( EventArgs );
		}

		virtual protected void OnIsCollapsedPropertyChanged( DependencyPropertyChangedEventArgs EventArgs )
		{
			Size OldSize;
			if ( this.IsCollapsed )
			{
				// We were just collapsed, so save the size for when we want to restore it.
				this.SavedSize = new Size( Double.IsNaN( this.Width ) ? this.ActualWidth : this.Width, Double.IsNaN( this.Height ) ? this.ActualHeight : this.Height );
				OldSize = this.SavedSize;
			}
			else
			{
				OldSize = new Size( this.ActualWidth, this.ActualHeight );
				// We were just expanded, so restore the Width or Height (only do it if they are non-0)
				if ( this.ResizeDirection == Direction.North || this.ResizeDirection == Direction.South )
				{
					if ( this.SavedSize.Height > 0 )
					{
						this.Height = this.SavedSize.Height;
					}
				}
				else
				{
					if ( this.SavedSize.Width > 0 )
					{
						this.Width = this.SavedSize.Width;
					}
				}
			}

			this.RaiseCollapsedToggled( OldSize );

		}

		#endregion

		#region ResizingEnabledProperty

		/// Gets or sets whether this CellSizer can be resized by the user
		public bool ResizingEnabled
		{
			get { return (bool)GetValue( ResizingEnabledProperty ); }
			set { SetValue( ResizingEnabledProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for ResizingEnabled.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty ResizingEnabledProperty =
			DependencyProperty.Register( "ResizingEnabled", typeof( bool ), typeof( CellSizer ), new UIPropertyMetadata( true, new PropertyChangedCallback( ResizingAllowedPropertyChanged ) ) ); 

		/// Called whenever the ResizingEnabled Property changes
		static void ResizingAllowedPropertyChanged( DependencyObject Sender, DependencyPropertyChangedEventArgs Args )
		{
			CellSizer This = Sender as CellSizer;
			This.UpdateGripVisibilityAndControlSizing();
			
		}



		#endregion

		/// Will be triggered after the collapse animation completes, assuming that the collapse animation sets visibility to Collapsed.
		/// Will be triggered at the start of the expand animation.
		protected void UpdateGripVisibilityAndControlSizing()
		{
			Thumb Grip = this.GetGripControl( ResizeDirection );
			if ( Grip != null )
			{
				Grip.Visibility = ( this.ResizingEnabled && !this.IsCollapsed ) ? Visibility.Visible : Visibility.Collapsed;

				if ( Grip.Visibility != Visibility.Visible )
				{
					// When the user cannot resize the control, make it autosize
					this.ClearValue( WidthProperty );
					this.ClearValue( HeightProperty );
				}
			}
		}

		
		/// <summary>
		/// Returns the Thumb Control corresponding to the Direction
		/// </summary>
		/// <param name="InResizeDirection">The resize direction (N, E, S, W) </param>
		/// <returns>The Thumb Control corresponding to the ResizeGrip.</returns>
		private Thumb GetGripControl( Direction InResizeDirection )
		{
			switch ( InResizeDirection )
			{
				case Direction.North:
					return mPART_N;
				case Direction.West:
					return mPART_W;
				case Direction.South:
					return mPART_S;
				case Direction.East:
					return mPART_E;
				default:
					return null;
			}
		}

		/// <summary>
		/// Attach the appropriate handler to a grip depending on which direction the grip resizes.
		/// </summary>
		/// <param name="Grip">The grip to which a mandler is to be attached.</param>
		void AttachHandlerToGrip( Thumb Grip )
		{
			if ( Grip == mPART_N )
			{
				Grip.DragDelta -= mNorthDragHandler;
				Grip.DragDelta += mNorthDragHandler;
			}
			else if ( Grip == mPART_W )
			{
				Grip.DragDelta -= mWestDragHandler;
				Grip.DragDelta += mWestDragHandler;
			}
			else if ( Grip == mPART_E )
			{
				Grip.DragDelta -= mEastDragHandler;
				Grip.DragDelta += mEastDragHandler;
			}
			else if ( Grip == mPART_S )
			{
				Grip.DragDelta -= mSouthDragHandler;
				Grip.DragDelta += mSouthDragHandler;
			}
			else
			{
				throw new ArgumentException("Grip must be ");
			}
		}

		/// This control expects the template to have thumb controls for resizing.
		/// This is our opportunity to grab references to those controls.
		override public void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			// Retrieve the resizer grips from the current template
			mPART_N = (Thumb)Template.FindName( "PART_N", this );
			mPART_W = (Thumb)Template.FindName( "PART_W", this );
			mPART_E = (Thumb)Template.FindName( "PART_E", this );
			mPART_S = (Thumb)Template.FindName( "PART_S", this );

			// Collapse all grippers
			if ( mPART_N != null )
			{
				mPART_N.Visibility = Visibility.Collapsed;
			}

			if ( mPART_W != null )
			{
				mPART_W.Visibility = Visibility.Collapsed;
			}

			if ( mPART_E != null )
			{
				mPART_E.Visibility = Visibility.Collapsed;
			}

			if ( mPART_S != null )
			{
				mPART_S.Visibility = Visibility.Collapsed;
			}

			// Show and bind to the active gripper		
			Thumb NewGripper = GetGripControl( this.ResizeDirection );
			if ( NewGripper != null )
			{
				NewGripper.Visibility = (this.ResizingEnabled) ? Visibility.Visible : Visibility.Collapsed;
				AttachHandlerToGrip( NewGripper );
			}
		}

		/// Delegate for handling resizing from the East edge of the control
		DragDeltaEventHandler mEastDragHandler;
		/// Handler resizing from the East edge of the control
		void mPart_EastDragHandler( object sender, DragDeltaEventArgs e )
		{
			ResizeHorizontal(e.HorizontalChange);
		}

		/// Delegate for handling resizing from the West edge of the control
		DragDeltaEventHandler mWestDragHandler;
		/// Handler resizing from the West edge of the control
		void mPart_WestDragHandler( object sender, DragDeltaEventArgs e )
		{
			ResizeHorizontal( - e.HorizontalChange );
		}

		/// Delegate for handling resizing from the North edge of the control
		DragDeltaEventHandler mNorthDragHandler;
		/// Handler resizing from the Noth edge of the control
		void mPart_NorthDragHandler( object sender, DragDeltaEventArgs e )
		{
			ResizeVertical( - e.VerticalChange );
		}

		/// Delegate for handling resizing from the South edge of the control
		DragDeltaEventHandler mSouthDragHandler;
		/// Handler resizing from the South edge of the control
		void mPart_SouthDragHandler( object sender, DragDeltaEventArgs e )
		{
			ResizeVertical( e.VerticalChange );
		}

		/// <summary>
		/// Alter the width of the CellSizer by Delta.
		/// </summary>
		/// <param name="Delta">Amount by which to change the width.</param>
		protected virtual void ResizeHorizontal( double Delta )
		{
			Size OldSize = new Size( this.ActualWidth, this.ActualHeight );
			if ( double.IsNaN( this.Width ) )
			{
				this.Width = this.ActualWidth;
			}
			this.Width = MathUtils.Clamp( this.Width + Delta, this.MinWidth, this.MaxWidth );
			RaiseResizedByUser( OldSize );
		}

		/// <summary>
		/// Alter the height of the CellSizer by Delta.
		/// </summary>
		/// <param name="Delta">Amount by which to change the height.</param>
		protected virtual void ResizeVertical( double Delta )
		{
			Size OldSize = new Size( this.ActualWidth, this.ActualHeight );
			if ( double.IsNaN( this.Height ) )
			{
				this.Height = this.ActualHeight;
			}
			this.Height = MathUtils.Clamp( this.Height + Delta, this.MinHeight, this.MaxHeight );
			RaiseResizedByUser( OldSize );
		}

		/// North resizer grip
		private Thumb mPART_N;
		/// West resizer grip
		private Thumb mPART_W;
		/// East resizer grip
		private Thumb mPART_E;
		/// South resizer grip
		private Thumb mPART_S;

	}
}
