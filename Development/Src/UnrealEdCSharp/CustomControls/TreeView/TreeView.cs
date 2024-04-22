//=============================================================================
//	TreeView.cs: CustomControls.TreeView is a wrapper around WPF's Tree view 
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

using System;
using System.Collections.Generic;
using System.ComponentModel;
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
using System.Collections.ObjectModel;

namespace CustomControls
{

	/// <summary>
	/// A TreeView class that wraps the WPF TreeView implementation with support for multi-selection, programmatic selection and node expansion/collapsing.
	/// </summary>
	public class TreeView : System.Windows.Controls.TreeView
	{
		static TreeView()
		{
			//This OverrideMetadata call tells the system that this element wants to provide a style that is different than its base class.
			//This style is defined in themes\generic.xaml

			// !! DO NOT OVERRIDE THE TREE STYLE !!
			// Overriding tree control's style/template seems to turn VIRTUALIZATION OFF.
			//DefaultStyleKeyProperty.OverrideMetadata( typeof( TreeView ), new FrameworkPropertyMetadata( typeof( TreeView ) ) );			
		}


		/// Hide the SelectedItem property of the WPF tree; we don't want the user accidentally using it.
		/// @todo cb [reviewed; discuss]: Unfortunately they still can call this method if they have a base class reference. Bad.
		/// Could use containment (instead of inheritance) to get around this.
		new public object SelectedItem { get { throw new Exception("Use GetSelection() instead."); } }


		/// Construct a new TreeView.
		public TreeView()
		{
			this.Root = new RootNode( this );
		}

		/// Backing store for root of the tree.
		private AbstractTreeNode mRoot;

		/// Get or set the root of the tree.
		/// The root is not visible; its children are used as the top-level 'ItemsSource' for the TreeView.
		/// A default root node is created for you.
		/// Note that the root node must be created and assigned before adding any children
		/// <example>
		/// // TreeView t;
		/// 
		/// //  -- Using the default root node --
		/// // MyTreeNode must be AbstractTreeNode.
		/// t.Root.AddChild( new MyTreeNode( /*Parent*/ t.Root, /*Name*/ "Node Name" ) );
		/// 
		/// // -- Using a custom root node --
		/// t.Root = new MyTreeNode( /*Parent*/ null );
		/// </example>
		public AbstractTreeNode Root
		{
			get { return mRoot; }
			set
			{
				mRoot = value;
				mRoot.OwnerTree = this;
				UnrealEd.Utils.CreateBinding( this, ItemsSourceProperty, this, "Root.Children" );
			}
		}

		/// Hide the ItemsSource field of the WPF TreeView. We want the user populating the tree via TreeView.Root.AddChild.
		/// @todo cb [reviewed; discuss]: same issue as SelectedItem; works well in XAML thoug
		new protected System.Collections.IEnumerable ItemsSource
		{
			get { return base.ItemsSource; }
			set { base.ItemsSource = value; }
		}


		// Backing store for ClearSelectionOnBackgroundClick
		private bool mClearSelectionOnBackgroundClick = false;
		/// <summary>
		/// Get or set whether treeview selection is cleared when the user clicks on the tree's background.
		/// </summary>		
		public bool ClearSelectionOnBackgroundClick
		{
			get { return mClearSelectionOnBackgroundClick; }
			set { mClearSelectionOnBackgroundClick = value; }
		}
	
		/// <summary>
		/// Provides the tree with a chance to override the removal of a node.
		/// </summary>
		/// <param name="NodeToRemove"></param>
		/// <returns>false if the tree wishes the prevent the node from being removed.</returns>
		public bool AllowNodeRemoval( AbstractTreeNode NodeToRemove )
		{
			return true;
		}

		/// <summary>
		/// Notifies the tree that a node was removed; removes the specified node from the selection set, if necessary
		/// </summary>
		/// <param name="RemovedNode"></param>
		public void NotifyRemovedNode( AbstractTreeNode RemovedNode )
		{
			BeginBatchSelection();
			{
				Unselect(RemovedNode);
				UnselectDescendants(RemovedNode);
			}
			EndBatchSelection();
		}

		/// <summary>
		/// Deselects all items then 
		/// </summary>
		/// <param name="NewSelectionSet"></param>
		public void SetSelection( ICollection<AbstractTreeNode> NewSelectionSet )
		{
			ClearSelectionInternal();
			foreach ( AbstractTreeNode CurTreeNode in NewSelectionSet )
			{
				if (CurTreeNode != null)
				{
					CurTreeNode.ExpandToRoot();
					SelectInternal(CurTreeNode);
				}
			}

			RaiseSelectionChanged();
		}

		/// <summary>
		/// Empty the selection in this TreeView.
		/// </summary>
		public void ClearSelection()
		{
			ClearSelectionInternal();

			RaiseSelectionChanged();
		}

		/// Internal version of ClearSelection. Does the actual selecting; does not raise an event.
		public void ClearSelectionInternal()
		{
			MarkSelectionDirty(mSelectedItems.Count > 0);
			foreach ( AbstractTreeNode CurTreeNode in mSelectedItems )
			{
				MarkSelectionDirty(CurTreeNode.IsSelected == true);
				CurTreeNode.IsSelected = false;
			}
			mSelectedItems.Clear();
		}

		/// <summary>
		/// Select a TreeNode. Do not modify AbstractTreeNode.IsSelected directly, instead always use this method.
		/// </summary>
		/// <param name="TreeNodeToSelect">An AbstactTreeNode to select.</param>
		public void Select( AbstractTreeNode TreeNodeToSelect )
		{
			SelectInternal( TreeNodeToSelect );
			RaiseSelectionChanged();
		}

		/// <summary>
		/// Select multiple tree nodes.  Generates a single selection-changed event.
		/// </summary>
		/// <param name="TreeNodesToSelect">the list of nodes to select</param>
		public void Select( ICollection<AbstractTreeNode> TreeNodesToSelect )
		{
			foreach ( AbstractTreeNode CurTreeNode in TreeNodesToSelect )
			{
				SelectInternal(CurTreeNode);
			}

			RaiseSelectionChanged();
		}

		/// Internal version of Select. Does the actual selecting; does not raise an event.
		private void SelectInternal( AbstractTreeNode TreeNodeToSelect )
		{
			if ( !TreeNodeToSelect.IsSelected )
			{
				MarkSelectionDirty(true);
				mSelectedItems.Add(TreeNodeToSelect);
				TreeNodeToSelect.IsSelected = true;
			}
		}
		
		/// <summary>
		/// Unselect a TreeNode. Do not modify AbstractTreeNode.IsSelected directly, instead always use this method.
		/// </summary>
		/// <param name="TreeNodeToUnselect">The AbstractTreeNode to unselect</param>
		public void Unselect( AbstractTreeNode TreeNodeToUnselect )
		{
			UnselectInternal( TreeNodeToUnselect );
			RaiseSelectionChanged();
		}

		/// <summary>
		/// Unselect multiple tree nodes at once.  Generates a single selection-changed event.
		/// </summary>
		/// <param name="TreeNodesToDeselect"></param>
		public void Unselect( ICollection<AbstractTreeNode> TreeNodesToDeselect )
		{
			foreach ( AbstractTreeNode CurTreeNode in TreeNodesToDeselect )
			{
				UnselectInternal(CurTreeNode);
			}

			RaiseSelectionChanged();
		}

		/// Internal version of Unselect. Does the actual unselecting; does not raise the event.
		private void UnselectInternal( AbstractTreeNode TreeNodeToUnselect )
		{
			if ( TreeNodeToUnselect.IsSelected )
			{
				MarkSelectionDirty(true);
				mSelectedItems.Remove( TreeNodeToUnselect );
				TreeNodeToUnselect.IsSelected = false;
			}
		}


		/// <summary>
		/// Unselect all descendants of a TreeNode.
		/// </summary>
		/// <param name="TreeNodeToUnselect">The AbstractTreeNode whose descendants to unselect.</param>
		/// <returns>The number of AbstractTreeNodes that were selected and became unselected.</returns>
		public int UnselectDescendants( AbstractTreeNode TreeNodeToUnselect )
		{
			int NumUnselected = UnselectDescendantsInternal( TreeNodeToUnselect );
			if ( NumUnselected > 0 )
			{
				RaiseSelectionChanged();
			}

			return NumUnselected;
		}

		
		/// Internal version of UnselectDescendants; does not raise an event.
		private int UnselectDescendantsInternal( AbstractTreeNode TreeNodeToUnselect )
		{
			int NumUnselected = 0;
			if ( TreeNodeToUnselect.Children != null )
			{
				foreach ( AbstractTreeNode CurNode in TreeNodeToUnselect.Children )
				{
					if (CurNode.IsSelected)
					{
						UnselectInternal( CurNode );
						++NumUnselected;
					}
					NumUnselected += UnselectDescendants( CurNode );
				}				
			}

			return NumUnselected;
		}

		/// <summary>
		/// Move the shift-start location to up the parent-child
		/// hierarchy the top-most collapsed node.
		/// This ensures that the Shift-Start location is visible.
		/// If the location is already visible it does not move.
		/// </summary>
		public void EnsureShiftSelectStartIsVisible()
		{
			if ( this.ShiftSelectStart != null )
			{
				AbstractTreeNode CurNode = this.ShiftSelectStart;
				AbstractTreeNode TopmostCollapsedNode = this.ShiftSelectStart;
				// Walk to the root; if we see a collapsed node, then move the start of shift-selection up.
				while ( CurNode != mRoot )
				{
					if (!CurNode.IsExpanded)
					{
						this.ShiftSelectStart = CurNode;
					}
					CurNode = CurNode.Parent;
				}
			}
		}

		/// <summary>
		/// Toggle the selection of a TreeNode.
		/// </summary>
		/// <param name="TreeNodeToToggle">The AbstractTreeNode whose selection to toggle.</param>
		public void ToggleSelection( AbstractTreeNode TreeNodeToToggle )
		{
			if ( TreeNodeToToggle.IsSelected )
			{
				UnselectInternal( TreeNodeToToggle );
			}
			else
			{
				SelectInternal( TreeNodeToToggle );
			}


			RaiseSelectionChanged();
		}

		/// <summary>
		/// Select all the visible items between NodeA and NodeB (or NodeB and NodeA). The order of NodeB and NodeA does not matter.
		/// </summary>
		/// <param name="NodeA">One of the nodes between which to select. If this node is null the beginning of the root list will be used.</param>
		/// <param name="NodeB">The other node.</param>
		public void SelectBetween( AbstractTreeNode NodeA, AbstractTreeNode NodeB )
		{
			// If NodeA is null, use the first element in the root.
			if ( NodeA == null )
			{
				NodeA = Root.Children[0];
			}

			//If we the endpoint and starting point are the same select the one point. We're done.
			if ( NodeA == NodeB )
			{
				SelectInternal( NodeA );
				return;
			}


			// Find the lowest common ancestor.
			List<AbstractTreeNode> PathToRootA = new List<AbstractTreeNode>();
			List<AbstractTreeNode> PathToRootB = new List<AbstractTreeNode>();

			// Build paths to root.
			{
				AbstractTreeNode CurNode = NodeA;
				while ( CurNode != null )
				{
					PathToRootA.Insert( 0, CurNode );
					CurNode = CurNode.Parent;
				}

				CurNode = NodeB;
				while ( CurNode != null )
				{
					PathToRootB.Insert( 0, CurNode );
					CurNode = CurNode.Parent;
				}
			}

			// Compute the index of the lowest common ancestor (LCA).
			int LCAIndex = 0;
			int ShorterPathLength = Math.Min( PathToRootA.Count, PathToRootB.Count );
			for ( int CurPathIdx = 0; CurPathIdx < ShorterPathLength; ++CurPathIdx )
			{
				// Whenever the path's diverge
				if ( PathToRootA[CurPathIdx] != PathToRootB[CurPathIdx] )
				{
					// Stop, the LCA
					break;
				}
				LCAIndex = CurPathIdx;
			}

			AbstractTreeNode LCA = PathToRootA[LCAIndex];
			AbstractTreeNode StartSelectNode = null;
			AbstractTreeNode EndSelectNode = null;
			// Compute which node would be found first during a top-down, depth-first traversal.
			if ( LCA == NodeA )
			{
				// When NodeA is the LCA, it has to be encountered first.
				StartSelectNode = NodeA;
				EndSelectNode = NodeB;
			}
			else if ( LCA == NodeB )
			{
				// When NodeB is the LCA, it has to be encountered first.
				StartSelectNode = NodeB;
				EndSelectNode = NodeA;
			}
			else
			{
				// The two children of the LCA which differ (this is where the paths branch).
				AbstractTreeNode LCAChildA = PathToRootA[LCAIndex + 1];
				AbstractTreeNode LCAChildB = PathToRootB[LCAIndex + 1];
				foreach ( AbstractTreeNode LCAChildNode in LCA.Children )
				{
					if ( LCAChildA == LCAChildNode )
					{
						StartSelectNode = NodeA;
						EndSelectNode = NodeB;
						break;
					}
					else if( LCAChildB == LCAChildNode )
					{
						StartSelectNode = NodeB;
						EndSelectNode = NodeA;
						break;
					}
				}
			}

			bool IsInSelection = StartSelectNode == LCA;
			if (IsInSelection)
			{
				SelectInternal( LCA );
			}
			// Select between the nodes.
			SelectBetweenHelper( LCA, StartSelectNode, EndSelectNode, ref IsInSelection );

			RaiseSelectionChanged();
		}


		/// <summary>
		/// Perform a depth first traversal of the tree starting with the 'Ancestor.Children' as the root list.
		/// After encountering the 'StartNode' begin selecting every visible node until we encounter 'EndNode'.
		/// A visible node is one whose parent is expanded.
		/// </summary>
		/// <param name="Ancestor">The root of the subtree to traverse.</param>
		/// <param name="StartNode">The node that begins our selection range.</param>
		/// <param name="EndNode">The node that terminates the selection range.</param>
		/// <param name="IsInSelection">Whether we have encountered the StartNode, and therefor should be selecting nodes.</param>
		protected void SelectBetweenHelper( AbstractTreeNode Ancestor, AbstractTreeNode StartNode, AbstractTreeNode EndNode, ref bool IsInSelection )
		{
			// Get the current packages view to check which packages are filtered
			ListCollectionView PackagesCollectionView = (ListCollectionView)CollectionViewSource.GetDefaultView(Ancestor.Children);

			if (Ancestor.Children != null)
			{
				foreach ( AbstractTreeNode CurNode in Ancestor.Children )
				{
					// Should we start selecting?
					if ( CurNode == StartNode )
					{
						IsInSelection = true;
					}

					if (IsInSelection && PackagesCollectionView.Contains(CurNode))
					{
						// Select the item.
						this.SelectInternal( CurNode );
					}

					if ( CurNode == EndNode )
					{
						// We're done. Stop
						IsInSelection = false;
						return;
					}

					if ( CurNode.IsExpanded )
					{
						// This node's children are visible; descend.
						SelectBetweenHelper( CurNode, StartNode, EndNode, ref IsInSelection );
					}
				}
			}
		}

		/// <summary>
		/// The user clicked on the tree itself; Clear the selection.
		/// </summary>
		protected override void OnMouseLeftButtonDown( MouseButtonEventArgs e )
		{
			if ( ClearSelectionOnBackgroundClick )
			{
				this.ClearSelection();
			}
			base.OnMouseLeftButtonDown( e );
		}

		/// How many items are selected in the treeview at the moment.
		public int SelectedCount { get { return mSelectedItems.Count; } }

		/// <summary>
		/// Get a list of selected nodes.
		/// </summary>
		/// <returns>The list of currently selected nodes.</returns>
		public ReadOnlyCollection<AbstractTreeNode> GetSelection() { return mSelectedItems.AsReadOnly(); }
		/// Backing store for the selection list.
		private List<AbstractTreeNode> mSelectedItems = new List<AbstractTreeNode>();

		/// Backing store for the location where the shift-selection will start.
		private AbstractTreeNode mShiftSelectStart;
		/// Get or set the location where the shift-selection range begins.
		public AbstractTreeNode ShiftSelectStart
		{
			get { return mShiftSelectStart; }
			set { mShiftSelectStart = value; }
		}


		/// Handler for SelectionChagnedEvent
		public delegate void SelectionChangedEventHandler();
		/// An event that is raised when the selection in this tree view changes.
		public event SelectionChangedEventHandler SelectionChanged;
		private void RaiseSelectionChanged()
		{
			if ( bSelectionDirty && BatchSelectionSemaphore == 0 )
			{
				if ( SelectionChanged != null )
				{
					SelectionChanged();
				}

				bSelectionDirty = false;
			}
		}

		/// <summary>
		/// Safely flags the selection state as dirty.
		/// </summary>
		private void MarkSelectionDirty( bool bValue )
		{
			bSelectionDirty = bSelectionDirty || bValue;
		}


		#region Batch Selection
		/// <summary>
		/// Counter for disabling selection notifications.
		/// </summary>
		private int BatchSelectionSemaphore;
		private bool bSelectionDirty;

		/// <summary>
		/// Disables selection change notifications.  Useful when performing multiple selection operations where the selection changed
		/// notification only needs to be fired once.
		/// </summary>
		/// <returns>the number of times BeginBatchSelection has been called without a matching call to EndBatchSelection</returns>
		public int BeginBatchSelection()
		{
			return ++BatchSelectionSemaphore;
		}

		/// <summary>
		/// Indicates that batch selection operations have been completed.  If this is the last batch operation, notification is fired.
		/// </summary>
		/// <returns>the current batch selection counter</returns>
		public int EndBatchSelection()
		{
			BatchSelectionSemaphore--;
			RaiseSelectionChanged();

			return BatchSelectionSemaphore;
		}

		/// <summary>
		/// Indicate that batch selection operations have been completed, without firing any notifications.
		/// </summary>
		/// <returns>the number of outstanding batch selection operations</returns>
		public int EndBatchSelectionSilent()
		{
			if ( --BatchSelectionSemaphore == 0 )
			{
				bSelectionDirty = false;
			}

			return BatchSelectionSemaphore;
		}

		#endregion

		/// <summary>
		/// A special implementation of AbstractTreeNode that serves as the root of the tree.
		/// </summary>
		public class RootNode : AbstractTreeNode
		{
			/// Construct a Root node class with a null parent and an OwnerTree.
			/// This is the only TreeNode that's allowed to have a null parent.
			public RootNode( TreeView InOwnerTree )
				: base( null )
			{
				this.mParent = null;
				this.mOwnerTree = InOwnerTree;
			}

			/// Backing store for the root-level list of nodes.
			private ObservableCollection<AbstractTreeNode> mChildren = new ObservableCollection<AbstractTreeNode>();
			/// Get the root-level child list.
			public override ObservableCollection<AbstractTreeNode> Children
			{
				get { return mChildren; }
			}

			/// <summary>
			/// Add a child to the root TreeNode list.
			/// </summary>
			/// <param name="InNode">A TreeNode to add to the root list.</param>
			public void AddChild( AbstractTreeNode InNode )
			{
				Children.Add(InNode);
			}

			/// <summary>
			/// Remove the specified item from this node's list of children
			/// </summary>
			/// <param name="NodeToRemove">the item that should be removed</param>
			/// <returns>true if the item was successfully removed.</returns>
			public bool RemoveChild( AbstractTreeNode NodeToRemove )
			{
				bool bResult = Children.Contains(NodeToRemove);
				if ( bResult && (OwnerTree == null || OwnerTree.AllowNodeRemoval(NodeToRemove)) )
				{
					Children.Remove(NodeToRemove);
					OwnerTree.NotifyRemovedNode(NodeToRemove);
				}

				return bResult;
			}
		}
	}

	public class CustomVirtualizingStackPanel : VirtualizingStackPanel
	{
		public void BringIntoView( int index )
		{
			this.BringIndexIntoView(index);
		}
	}
}