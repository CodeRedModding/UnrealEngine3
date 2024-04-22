//=============================================================================
//	SourcesPanel.xaml.cs: Content browser sources panel
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

namespace CustomControls
{
	[TemplatePart( Name = "PART_AddToCollectionButton", Type = typeof( Button ) )]
	public class CollectionVisual : System.Windows.Controls.Control
	{
		static CollectionVisual()
		{
			DefaultStyleKeyProperty.OverrideMetadata( typeof( CollectionVisual ), new FrameworkPropertyMetadata( typeof( CollectionVisual ) ) );
		}

		/// Delegate type for notifying that add-to-collection button has been clicked.
		public delegate void AddToCollectionClicked_Handler( CollectionVisual Sender, ContentBrowser.Collection CollectionRepresentedByVisual );
		/// Event invoked when add-to-collection has been clicked.
		public event AddToCollectionClicked_Handler AddToCollectionClicked;

		/// Called when the user who is dragging something first hovers over the CollectionVisual.
		protected override void OnDragEnter( DragEventArgs e )
		{
			base.OnDragEnter( e );
			HoveredByDragDrop = true;
		}

		/// Called when the user's cursor that is dragging leaves the collection.
		protected override void OnDragLeave( DragEventArgs e )
		{
			base.OnDragLeave( e );
			HoveredByDragDrop = false;
		}

		/// Called when the user drops something onto a CollectionVisual.
		protected override void OnDrop( DragEventArgs e )
		{
			base.OnDrop( e );
			HoveredByDragDrop = false;
		}

		/// True when the CollectionVisual is being hovered by a user who is dragging something. This is a dependency property.
		public bool HoveredByDragDrop
		{
			get { return (bool)GetValue( HoveredByDragDropProperty ); }
			set { SetValue( HoveredByDragDropProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for HoveredByDragDrop.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty HoveredByDragDropProperty =
			DependencyProperty.Register( "HoveredByDragDrop", typeof( bool ), typeof( CollectionVisual ), new UIPropertyMetadata( false ) );



		/// A new template is being applied; get references to parts of interest.
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			if ( mPART_AddToCollectionButton != null )
			{
				mPART_AddToCollectionButton.Click -= mPART_AddToCollectionButton_Click;
			}

			mPART_AddToCollectionButton = (Button)Template.FindName( "PART_AddToCollectionButton", this );

			if (mPART_AddToCollectionButton != null)
			{
				mPART_AddToCollectionButton.Click += mPART_AddToCollectionButton_Click;
			}
		}

		/// Called when add-to-collection button is clicked.
		void mPART_AddToCollectionButton_Click( object sender, RoutedEventArgs e )
		{
			if ( AddToCollectionClicked != null )
			{
				AddToCollectionClicked( this, AssetCollection );
			}
		}


		/// Whether we can add items to this collection
		public bool CanAddToCollection
		{
			get { return (bool)GetValue( CanAddToCollectionProperty ); }
			set { SetValue( CanAddToCollectionProperty, value ); }
		}

		public static readonly DependencyProperty CanAddToCollectionProperty =
			DependencyProperty.Register( "CanAddToCollection", typeof( bool ), typeof( CollectionVisual ), new UIPropertyMetadata( false ) );
		


		/// The Collection of Assets that this CollectionVisual is visualizing. This is a dependency property.
		public ContentBrowser.Collection AssetCollection
		{
			get { return (ContentBrowser.Collection)GetValue( AssetCollectionProperty ); }
			set { SetValue( AssetCollectionProperty, value ); }
		}

		public static readonly DependencyProperty AssetCollectionProperty =
			DependencyProperty.Register( "AssetCollection", typeof( ContentBrowser.Collection ), typeof( CollectionVisual ), new UIPropertyMetadata( null ) );





		/// The icon to show in front of the collection name. This is a dependency property.
		public BitmapImage Icon
		{
			get { return (BitmapImage)GetValue( IconProperty ); }
			set { SetValue( IconProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for Icon.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IconProperty =
			DependencyProperty.Register( "Icon", typeof( BitmapImage ), typeof( CollectionVisual ), new UIPropertyMetadata( null ) );


		/// Reference to the add-to-collection button.
		private Button mPART_AddToCollectionButton;



	}

}
