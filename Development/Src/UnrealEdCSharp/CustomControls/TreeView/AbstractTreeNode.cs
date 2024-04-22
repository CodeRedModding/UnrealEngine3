//=============================================================================
//	AbstractTreeNode.cs: Abstract node for CustomControls.TreeView
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Controls;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Windows;
using System.Windows.Input;

namespace CustomControls
{

	/// <summary>
	/// The AbstractTreeNode serves as the base class for concrete nodes that can be added to a Tree.
	/// </summary>
	public abstract class AbstractTreeNode : DependencyObject
	{

		static AbstractTreeNode()
		{
		}


		/// <summary>
		/// Create an AbstractTreeNode. Root-level nodes should have the Tree's Root node as the parent.
		/// </summary>
		/// <param name="InParent">The parent of this node. This argument must never be null.</param>
		public AbstractTreeNode( AbstractTreeNode InParent )
		{
			this.Parent = InParent;
			if (Children != null)
			{
				// if this node supports children, then bind a callback for the collection of children changing.
				Children.CollectionChanged += new System.Collections.Specialized.NotifyCollectionChangedEventHandler( Children_CollectionChanged );
			}
		}

		/// Callback for the Children collection changing; keep HasChildren up to date.
		void Children_CollectionChanged( object sender, System.Collections.Specialized.NotifyCollectionChangedEventArgs e )
		{
			this.HasChildren = ( Children.Count != 0 );
		}

		/// Concrete TreeNodes should either implement this method to return a collection of their children or
		/// to return null if they do not plan to have children.
		public abstract ObservableCollection<AbstractTreeNode> Children { get; }

		/// Backing store for the Parent node reference.
		protected AbstractTreeNode mParent;
		
		/// Get the parent node.
		public AbstractTreeNode Parent
		{
			get { return mParent; }
			set
			{
				mParent = value;
				if ( mParent != null )
				{
					mOwnerTree = Parent.OwnerTree;
					if (this.OwnerTree == null)
					{
						throw new Exception("Parent has null OwnerTree; The OwnerTree must always be set." +
											"When using a custom Root, make sure to assign the root first and then add children to it.");
					}
				}
			}
		}

		/// Backing store for OwnerTree reference
		protected TreeView mOwnerTree;
		/// Get the tree that owns this TreeNode.
		/// You should never modify OwnerTree directly
		/// unless you are creating your own root node.
		public TreeView OwnerTree
		{
			get { return mOwnerTree; }
			set { mOwnerTree = value; }
		}


		/// <summary>
		/// Expand this node and any of its ancestors
		/// </summary>
		public void ExpandToRoot()
		{
			this.IsExpanded = true;
			if (IsExpanded && this.Parent != OwnerTree.Root && this.Parent != null)
			{
				this.Parent.ExpandToRoot();
			}
		}

		/// Toggle this node; recursively expand all descendant nodes if the toggle resulted in an expansion and recursively collapse descendants otherwise.
		public void ToggleRecursive()
		{
			if (this.IsExpanded)
			{
				CollapseRecursive();
			}
			else
			{
				ExpandRecursive();
			}
		}

		/// <summary>
		/// Expand this node and all of its descendants
		/// </summary>
		public void ExpandRecursive()
		{
			this.IsExpanded = true;
			if ( this.Children != null )
			{
				foreach ( AbstractTreeNode Node in this.Children )
				{
					Node.ExpandRecursive();
				}
			}
		}

		/// <summary>
		/// Collapse this node and all of its descendants
		/// </summary>
		public void CollapseRecursive()
		{
			this.IsExpanded = false;
			if ( this.Children != null)
			{
				foreach (AbstractTreeNode Node in this.Children)
				{
					Node.CollapseRecursive();
				}
			}
		}



		/// <summary>
		/// Copies various state properties from another tree node.  Used when replacing a node in the tree.
		/// </summary>
		/// <param name="OtherNode"></param>
		public void InitializeNode( AbstractTreeNode OtherNode )
		{
			IsExpanded = OtherNode.IsExpanded;
			if ( OtherNode.IsSelected )
			{
				OwnerTree.BeginBatchSelection();
				OwnerTree.Unselect(OtherNode);

				OwnerTree.Select(this);
				OwnerTree.EndBatchSelectionSilent();
			}
		}


		#region IsSelected Property

		/// Get or set whether this TreeNode is selected.
		/// Do not set this property directly; instead use
		/// CustomControls.TreeView.Select,
		/// Unselect, ClearSelection, and ToggleSelection.
		/// This is a dependency property.
		public bool IsSelected
		{
			get { return (bool)GetValue( IsSelectedProperty ); }
			set { SetValue( IsSelectedProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for IsSelected.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsSelectedProperty =
			DependencyProperty.Register( "IsSelected", typeof( bool ), typeof( AbstractTreeNode ), new UIPropertyMetadata( false ) );

		#endregion


		#region IsExpanded Property

		/// Get or set whether the TreeNode is Exapnded. This is a dependency property.
		public bool IsExpanded
		{
			get { return (bool)GetValue( IsExpandedProperty ); }
			set { SetValue( IsExpandedProperty, value ); }
		}

		// Using a DependencyProperty as the backing store for IsExpanded.  This enables animation, styling, binding, etc...
		public static readonly DependencyProperty IsExpandedProperty =
			DependencyProperty.Register( "IsExpanded", typeof( bool ), typeof( AbstractTreeNode ), new UIPropertyMetadata( false, IsExpandedChanged ) );

		static void IsExpandedChanged( DependencyObject Sender, DependencyPropertyChangedEventArgs e )
		{
			AbstractTreeNode This = (AbstractTreeNode)Sender;
			if ( This.OwnerTree.UnselectDescendants( This ) != 0 )
			{
				// If we unselected any nodes, then this node becomes selected
				This.OwnerTree.Select( This );
			}

		}

		#endregion


		#region HasChildren Property

		/// Find out whether this TreeNode has any children. This is a readonly dependency property.
		private static readonly DependencyPropertyKey HasChildrenKey =
			DependencyProperty.RegisterReadOnly( "HasChildren", typeof( bool ), typeof( AbstractTreeNode ), new PropertyMetadata( false ) );

		public static readonly DependencyProperty HasChildrenProperty = HasChildrenKey.DependencyProperty;

		/// Get whether this TreeNode has Children. This is a read-only dependency property.
		public bool HasChildren
		{
			get { return (bool)GetValue( HasChildrenProperty ); }
			private set { SetValue( HasChildrenKey, value ); }
		}		
		
		#endregion
		
	}
}
