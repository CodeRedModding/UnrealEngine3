//=============================================================================
//	TreeNodeVisual.cs: Visualization of a tree node for CustomControls.TreeView
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
using System.Collections.ObjectModel;

namespace CustomControls
{
	/// <summary>
	/// A custom control representing an item in the TreeView.
	/// Note that instances of TreeNodeVisual should never be created explicitly.
	/// Instead, ask the TreeView to instantiate these items via a HierarchicalDataTemplate.	
	/// </summary>
	/// 
	/// <example>
	/// <CustomControls:TreeView x:Name="mTree"
	///		Grid.Column="0"
	///		VirtualizingStackPanel.IsVirtualizing="True"
	///		Background="#4f4f4f">
	///		<CustomControls:TreeView.Resources>
	///			<!-- Note that local:LeafTreeNode implements AbstractTreeNode is a -->
	///			<HierarchicalDataTemplate DataType="{x:Type local:LeafTreeNode}" ItemsSource="{Binding Path=Children}">
	///				<!-- NodeModel="Binding" is necessary for tieing the VisualNode to the NodeModel -->
	///				<CustomControls:TreeNodeVisual DisplayLabel="{Binding Path=Name}"
	///											   NodeModel="{Binding}"
	///											   Icon="{StaticResource imgCheckedIn}" 
	///											   ContextMenu="{StaticResource WhurMahMenu}"
	///											   ContextMenuOpening="OnOpenContextMenu_TreeNodeVisual" />
	///			</HierarchicalDataTemplate>
	///			<!-- Note that local:FolderTreeNode implements AbstractTreeNode and has Children -->
	///			<HierarchicalDataTemplate DataType="{x:Type local:FolderTreeNode}" ItemsSource="{Binding Path=Children}">
	///				<!-- A FolderNodeVisual is a special type of TreeNodeVisual that switches between two different images -->
	///				<!-- depending on its expansion state (e.g. an open folder and a closed folder) -->
	///				<CustomControls:FolderNodeVisual DisplayLabel="{Binding Path=Name}"
	///												 NodeModel="{Binding}" />
	///			</HierarchicalDataTemplate>
	///			
	///		</CustomControls:TreeView.Resources>
	///	</CustomControls:TreeView>
	///
	///</example>
	[TemplatePart( Name = "PART_ExpanderGlyph", Type = typeof( Border ) )]
	public class TreeNodeVisual : System.Windows.Controls.Control
	{
		static TreeNodeVisual()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml

			DefaultStyleKeyProperty.OverrideMetadata( typeof( TreeNodeVisual ), new FrameworkPropertyMetadata( typeof( TreeNodeVisual ) ) );
		}

		/// Construct a tree node visual. You should never call this method directly; instead use a HierarchicalDataTemplate (see example above.)
		public TreeNodeVisual()
		{
		}

		/// Handle user clicking on the expander glyph by expanding/collapsing the node (and optionally descendant nodes)
		void mPART_ExpanderGlyph_MouseLeftButtonDown( object sender, RoutedEventArgs e )
		{
			if ( ( Keyboard.Modifiers & ModifierKeys.Shift ) != 0 )
			{
				this.NodeModel.ToggleRecursive();
			}
			else
			{
				this.NodeModel.IsExpanded = !this.NodeModel.IsExpanded;
			}
			
			e.Handled = true;
		}

		/// Called when a template is applied to this control.
		/// We must grab hold of the expander glyph control
		public override void OnApplyTemplate()
		{
			base.OnApplyTemplate();

			if ( mPART_ExpanderGlyph != null )
			{
				mPART_ExpanderGlyph.MouseLeftButtonDown -= mPART_ExpanderGlyph_MouseLeftButtonDown;
			}

			mPART_ExpanderGlyph = (Border)Template.FindName( "PART_ExpanderGlyph", this );

			if ( mPART_ExpanderGlyph != null )
			{
				mPART_ExpanderGlyph.MouseLeftButtonDown += mPART_ExpanderGlyph_MouseLeftButtonDown;
			}
		}


		/// Handle mouse presses to control selection.
		/// This method talks to the OwnerTree to control selection.
		/// This replaces WPF's selection behavior and adds multi-selection handling.
		protected override void OnMouseDown( MouseButtonEventArgs e )
		{
			if (OwnerTree == null)
			{
				// We need a parent tree so that we can control selection
				throw new Exception( "ParentTree property must be set. TreeNodeVisual is meant to be instantiated via HierarchicalDataTemplate; set the ParentTree property to the owner tree." );
			}

			bool CtrlIsDown = Keyboard.IsKeyDown( Key.LeftCtrl ) || Keyboard.IsKeyDown( Key.RightCtrl );
			bool ShiftIsDown = Keyboard.IsKeyDown( Key.LeftShift ) || Keyboard.IsKeyDown( Key.RightShift );

			if ( CtrlIsDown )
			{
				if ( e.RightButton == MouseButtonState.Pressed )
				{
					// [Ctrl + Right Click]
					// Do not change selection. Context menu will pop up.
				}
				else if ( e.LeftButton == MouseButtonState.Pressed )
				{
					// [Ctrl + Left Click]
					// Toggle the clicked item
					OwnerTree.ToggleSelection(this.NodeModel);
					OwnerTree.ShiftSelectStart = this.NodeModel;
				}
			}
			else if ( ShiftIsDown )
			{
				if ( e.RightButton == MouseButtonState.Pressed )
				{
					if ( this.IsSelected )
					{
						// [Shift + Right Click] on an already selected item
						// Do nothing; context menu will pop up.
					}
					else
					{
						// [Shift + Right Click] on an unselected item
						OwnerTree.ClearSelectionInternal();
						OwnerTree.Select(this.NodeModel);
					}
				}
				else if ( e.LeftButton == MouseButtonState.Pressed )
				{
					// [Shift + LeftClick] Perform a range selection 
					OwnerTree.EnsureShiftSelectStartIsVisible();
					OwnerTree.ClearSelectionInternal();
					OwnerTree.SelectBetween(OwnerTree.ShiftSelectStart, this.NodeModel);
				}
			}
			else
			{
				if ( e.RightButton == MouseButtonState.Pressed )
				{
					if ( this.IsSelected )
					{
						// [Right Click] on an already selected item
						// Do nothing; context menu will pop up.
					}
					else
					{
						// [Right Click] on an unselected item
						// Unselect everything, reselect this item. Context menu will pop up.
						OwnerTree.ClearSelectionInternal();
						OwnerTree.Select(this.NodeModel);
						OwnerTree.ShiftSelectStart = this.NodeModel;
					}
				}
				else if ( e.LeftButton == MouseButtonState.Pressed )
				{
					// [Left Click] on an item
					ReadOnlyCollection<AbstractTreeNode> CurrentSelection = OwnerTree.GetSelection();
					// Check the special case where this item is already selected and is the only thing selected.
					if ( CurrentSelection.Count != 1 || CurrentSelection[0] != this.NodeModel )
					{
						// Unselect everything. Select this item.
						OwnerTree.ClearSelectionInternal();
						OwnerTree.Select( this.NodeModel );
					}
					OwnerTree.ShiftSelectStart = this.NodeModel;
				}

			}

			base.OnMouseDown( e );
		}


		#region ParentTree Property
		/// Get the tree that owns this TreeNodeVisual.
		public CustomControls.TreeView OwnerTree
		{
			get { return NodeModel.OwnerTree; }
		}
		
		#endregion


		#region NodeModel Property

		/// Gets or sets the TreeNode which this TreeNodeVisual represents. This is a dependency property.
		public AbstractTreeNode NodeModel
		{
			get { return (AbstractTreeNode)GetValue(NodeModelProperty); }
			set { SetValue(NodeModelProperty, value); }
		}

		// Using a DependencyProperty as the backing store for TreeNode.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty NodeModelProperty =
			DependencyProperty.Register("NodeModel", typeof(AbstractTreeNode), typeof(TreeNodeVisual), new PropertyMetadata(null, NodeModelChangedCallback));

		/// <summary>
		/// The model node associated with this tree visual changed.
		/// </summary>
		/// <param name="d">The dependency object whose property changed (should always be this)</param>
		/// <param name="e">The events describing how the property changed.</param>
		static void NodeModelChangedCallback( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			TreeNodeVisual This = (TreeNodeVisual) d ;
			This.OnNodeModelChanged();
		}

		/// <summary>
		/// Called when the model node changes.
		/// </summary>
		protected virtual void OnNodeModelChanged()
		{
			// Bind the relevant properties between the NodeModel and the NodeVisual.
			Utils.CreateBinding(this, IsSelectedProperty, NodeModel, "IsSelected");
			Utils.CreateBinding(this, IsExpandedProperty, NodeModel, "IsExpanded");
			Utils.CreateBinding(this, HasChildrenProperty, NodeModel, "HasChildren");
		}
		
		#endregion

		
		#region IsExpanded Property

		/// Get or set whether this visual tree node is expanded
		public bool IsExpanded
		{
			get { return (bool)GetValue( IsExpandedProperty ); }
			set { SetValue( IsExpandedProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for IsExpanded.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsExpandedProperty =
			DependencyProperty.Register( "IsExpanded", typeof( bool ), typeof( TreeNodeVisual ), new UIPropertyMetadata( IsExpandedChangedHandler ) );

		/// Callback for IsExpandedProperty changing.
		protected static void IsExpandedChangedHandler( DependencyObject d, DependencyPropertyChangedEventArgs e )
		{
			( (TreeNodeVisual)d ).OnIsExpandedChanged();
		}

		/// Called when the IsExpanded property changes.
		protected virtual void OnIsExpandedChanged()
		{
		}

		#endregion


		#region IsSelected Property

		/// Do not modify this property directly; instead use the selection methods in TreeView.
		/// Get or set whether this TreeNodeVisual item appears selected. This is a dependency property.
		public bool IsSelected
		{
			get { return (bool)GetValue( IsSelectedProperty ); }
			protected set { SetValue( IsSelectedProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for IsSelected.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsSelectedProperty =
			DependencyProperty.Register( "IsSelected", typeof( bool ), typeof( TreeNodeVisual ), new UIPropertyMetadata( false ) );
		
		#endregion


		#region DisplayLabel Property

		/// Get or set the text that appears on the visual
		public String DisplayLabel
		{
			get { return (String)GetValue( DisplayLabelProperty ); }
			set { SetValue( DisplayLabelProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for DisplayLabel.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty DisplayLabelProperty =
			DependencyProperty.Register( "DisplayLabel", typeof( String ), typeof( TreeNodeVisual ), new UIPropertyMetadata( "" ) );

		#endregion


		#region HasChildren Property

		/// Get or set whether this TreeNodeVisual's corresponding Model has child nodes. This is a dependency property.
		public bool HasChildren
		{
			get { return (bool)GetValue( HasChildrenProperty ); }
			set { SetValue( HasChildrenProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for HasChildren.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty HasChildrenProperty =
			DependencyProperty.Register( "HasChildren", typeof( bool ), typeof( TreeNodeVisual ), new UIPropertyMetadata( false ) ); 
		
		#endregion


		#region Icon Property

		// Get or set the icon that appears before the text. This is a dependency property.
		public BitmapImage Icon
		{
			get { return (BitmapImage)GetValue( IconProperty ); }
			set { SetValue( IconProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for Icon.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IconProperty =
			DependencyProperty.Register( "Icon", typeof( BitmapImage ), typeof( TreeNodeVisual ), new UIPropertyMetadata( null ) );

		
		#endregion


		#region OverlayIcon Property

		// Get or set the icon that is overlayed on top of the Icon. Can be used to indicate that something is a shortcut or to show an item's state. This is a dependency property.
		public BitmapImage OverlayIcon
		{
			get { return (BitmapImage)GetValue( OverlayIconProperty ); }
			set { SetValue( OverlayIconProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for OverlayIcon.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty OverlayIconProperty =
			DependencyProperty.Register( "OverlayIcon", typeof( BitmapImage ), typeof( TreeNodeVisual ), new UIPropertyMetadata( null ) );

		
		#endregion


		/// Reference to glyph that the user can click on to expand/collapse a node.
		private Border mPART_ExpanderGlyph = null;
	}
}
