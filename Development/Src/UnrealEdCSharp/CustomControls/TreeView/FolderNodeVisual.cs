//=============================================================================
//	FolderNodeVisual.cs: Visualization of a Folder node for CustomControls.TreeView
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

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
	public class FolderNodeVisual : CustomControls.TreeNodeVisual
	{
		static FolderNodeVisual()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml

			// !! DO NOT OVERRIDE. We want to reuse the template from TreeNodeVisual. !!

			//DefaultStyleKeyProperty.OverrideMetadata( typeof( FolderNodeVisual ), new FrameworkPropertyMetadata( typeof( FolderNodeVisual ) ) );
		}

		/// Construct a FolderNodeVisual. You should never call this method directly; instead use a HierarchicalDataTemplate (see TreeNodeVisual example)
		public FolderNodeVisual()
		{
			UpdateIcon();
		}

		/// Override collapsing and expanding the node to also update the icon
		override protected void OnIsExpandedChanged()
		{
			UpdateIcon();
		}



		/// Image to show when the folder is open
		public BitmapImage OpenFolderImage
		{
			get { return (BitmapImage)GetValue( OpenFolderImageProperty ); }
			set { SetValue( OpenFolderImageProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for OpenFolderImage.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty OpenFolderImageProperty =
			DependencyProperty.Register( "OpenFolderImage", typeof( BitmapImage ), typeof( FolderNodeVisual ), new PropertyMetadata( Utils.ApplicationInstance.Resources[ "imgFolderOpen" ], OnIconsChanged ) );


		/// Image to show when the folder is closed
		public BitmapImage ClosedFolderImage
		{
			get { return (BitmapImage)GetValue( ClosedFolderImageProperty ); }
			set { SetValue( ClosedFolderImageProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for ClosedFolderImage.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty ClosedFolderImageProperty =
			DependencyProperty.Register( "ClosedFolderImage", typeof( BitmapImage ), typeof( FolderNodeVisual ), new PropertyMetadata( Utils.ApplicationInstance.Resources[ "imgFolderClosed" ], OnIconsChanged ) );


		/// Invoked whenever the CloseFolderImage or OpenFolderImage changes.
		static void OnIconsChanged( DependencyObject Sender, DependencyPropertyChangedEventArgs e )
		{
			( Sender as FolderNodeVisual ).UpdateIcon();
		}





		/// Update the status icon to be a closed or opened folder
		private void UpdateIcon()
		{
			// Then update the folder icon
			if ( this.IsExpanded )
			{
				Icon = OpenFolderImage;
			}
			else
			{
				Icon = ClosedFolderImage;
			}
		}


	}
}
