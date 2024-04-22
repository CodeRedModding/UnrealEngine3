//=============================================================================
//	SourcesPanelViewModel.xaml.cs: Content browser sources panel view model
//	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================


using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.Windows;
using System.ComponentModel;
using System.Text;
using System.Windows.Media;
using System.Windows.Controls;
using System.Windows.Threading;
using System.Diagnostics;
using CustomControls;


namespace ContentBrowser
{
	/// Type of collection
	public enum EBrowserCollectionType
	{
		/// Shared collection
		Shared,

		/// Private collection
		Private,

        /// Local unsaved collection
        Local,
	}


	/// <summary>
    /// The model for the sources panel.
    /// The Sources Panel allows the user to select sources such as collections and packages.
    /// Items contained in collections and packages appeal in the AssetView.
    /// </summary>
    public class SourcesPanelModel
    {
		/// Cached content browser reference
		private MainControl m_ContentBrowser;

		/// A reference to the TreeView that we are populating with folders, packages, etc.
		private CustomControls.TreeView mTreeView;


		/// <summary>
        /// Default Constructor.
        /// </summary>
		public SourcesPanelModel()
        {
			StartupDirectory = "";
			RootDirectory = "";
        }


		/// Tell the tree view model which tree view to interact with
		public void SetNewPackageView(CustomControls.TreeView InTreeView)
		{
			mTreeView = InTreeView;
		}

		
		/// <summary>
		/// Initialize the asset view model
		/// </summary>
		/// <param name="InContentBrowser">Content browser that the asset view model is associated with</param>
		public void Init( MainControl InContentBrowser )
		{
			m_ContentBrowser = InContentBrowser;
		}


		#region Collections

		/// Shared collections
		protected ObservableCollection<Collection> mSharedCollections = new ObservableCollection<Collection>();
		public ObservableCollection<Collection> SharedCollections { get { return mSharedCollections; } }


		/// Private collections
		protected ObservableCollection<Collection> mPrivateCollections = new ObservableCollection<Collection>();
		public ObservableCollection<Collection> PrivateCollections { get { return mPrivateCollections; } }


		/// <summary>
		/// Synchronize the displayed collections with those passed in.
		/// </summary>
		/// <param name="InSharedCollectionNames">List of existing shared collection names</param>
		/// <param name="InPrivateCollectionNames">List of existing private collection names</param>
		/// <param name="InLocalCollectionNames">List of existing local collection names</param>
		public void SetCollectionNames( ICollection<String> InSharedCollectionNames, ICollection<String> InPrivateCollectionNames, ICollection<String> InLocalCollectionNames )
		{
			// @todo would be nice to use Sets and or FNames, but this list should be fairly short
			// so it should be a non-issue

			// Remove any collections that are no longer present
			{
				List<Collection> PendingDeletion = new List<Collection>();
				
				foreach ( Collection MyCollection in mSharedCollections )
				{
					if (!InSharedCollectionNames.Contains(MyCollection.Name))
					{
						PendingDeletion.Add( MyCollection );
					}
				}

				foreach ( Collection ToDelete in PendingDeletion )
				{
					mSharedCollections.Remove(ToDelete);
				}
			}
			{
				List<Collection> PendingDeletion = new List<Collection>();

				foreach( Collection MyCollection in mPrivateCollections )
				{
					if( !InPrivateCollectionNames.Contains( MyCollection.Name ) )
					{
						PendingDeletion.Add( MyCollection );
					}
				}

				foreach( Collection ToDelete in PendingDeletion )
				{
					mPrivateCollections.Remove( ToDelete );
				}
			}

			// Add any collections not currently displayed
			{
				// Build a list of current collection names
				List<String> CurrentCollectionNames = new List<String>( mSharedCollections.Count );
				foreach ( Collection MyCollection in mSharedCollections )
				{
					CurrentCollectionNames.Add( MyCollection.Name );
				}
				
				// Add collections new collections
				foreach ( String Candidate in InSharedCollectionNames )
				{
					if ( !CurrentCollectionNames.Contains( Candidate ) )
					{
						mSharedCollections.Add( new Collection( Candidate, false ) );
					}
					
				}
			}
			{
				// Build a list of current collection names
				List<String> CurrentCollectionNames = new List<String>( mPrivateCollections.Count );
				foreach( Collection MyCollection in mPrivateCollections )
				{
					CurrentCollectionNames.Add( MyCollection.Name );
				}

				// Add collections new collections
				foreach( String Candidate in InPrivateCollectionNames )
				{
					if( !CurrentCollectionNames.Contains( Candidate ) )
					{
                        bool bIsLocal = InLocalCollectionNames.Contains(Candidate);
                        mPrivateCollections.Add(new Collection(Candidate, bIsLocal));
					}

				}
			}

			
		}
		
		#endregion


		#region Packages Tree


		public String StartupDirectory
		{ get; set; }

		/// <summary>
		/// The directory path to the root directory (i.e. UnrealEngine3)
		/// </summary>
		public String RootDirectory
		{ get; set; }

		/// <summary>
		/// Sets the RootDirectory to path of the root game directory (i.e. D:\Code\UnrealEngine3\)
		/// </summary>
		/// <param name="BaseDirectoryPath">the directory containing the process image - should be retrieved by calling appBaseDir()</param>
		public void SetRootDirectoryPath( String BaseDirectoryPath )
		{
			StartupDirectory = BaseDirectoryPath;

			String tmpPath = BaseDirectoryPath + System.IO.Path.DirectorySeparatorChar + ".." + System.IO.Path.DirectorySeparatorChar + "..";
			RootDirectory = System.IO.Path.GetFullPath(tmpPath) + System.IO.Path.DirectorySeparatorChar;
		}

		/// <summary>
		/// The root folder contains all the packages in a tree.
		/// </summary>
		public Folder RootFolder { get { return (Folder)mTreeView.Root; } }

		/// <summary>
		/// Ensures that the specified packages are the only tree nodes selected in the package treeview.  Called
		/// when an asset is filtered via the "Sync to Browser" level viewport context menu command.
		/// </summary>
		/// <param name="NewSelectionSet">the list of packages which should be selected</param>
		/// <returns>True if the package selection was modified; false in the selection was not modified (usually because of the trivial case where the only package being selected is already the selection).</returns>
		public bool SynchronizeSelection( List<ObjectContainerNode> NewSelectionSet )
		{
			bool SyncIsNecessary = false;
			if ( mTreeView != null )
			{
				SyncIsNecessary = true;
				
				// Handle the trivial case where modifying the selection is not necessary
				// because the single package we want to select is already the only thing selected
				ReadOnlyCollection<AbstractTreeNode> CurrentSelection = mTreeView.GetSelection();
				if ( NewSelectionSet.Count == 1 &&
					 CurrentSelection.Count == 1 &&
					 NewSelectionSet[0] == CurrentSelection[0] )
				{
					SyncIsNecessary = false;
				}

				if ( SyncIsNecessary )
				{
					// Clear the package filter because it may be hiding a package that we need to select.
					m_ContentBrowser.MySourcesPanel.ClearPackageFilter();
					
					List<AbstractTreeNode> TreeNodeList = new List<AbstractTreeNode>();
					foreach ( ObjectContainerNode item in NewSelectionSet )
					{
						TreeNodeList.Add( item );
					}

					mTreeView.SetSelection( TreeNodeList );
					foreach ( ObjectContainerNode item in NewSelectionSet )
					{
						item.EnsureVisible();
					}
				}
			}

			return SyncIsNecessary;			
		}

		/// <summary>
		/// Used for saving the expanded state of the tree.
		/// </summary>
		/// <returns></returns>
		public String ExpandedNodesString
		{
			get
			{
				StringBuilder StringBuilder = new StringBuilder();
				BuildExpandedNodesString(this.RootFolder, StringBuilder);
				return StringBuilder.ToString();
			}
		}

		/// Separator between paths. Used for saving paths.
		public static char TreeViewPathSeparator = ';';
		/// <summary>
		/// Build a string representing the expanded nodes. Useful in saving expanded nodes to config.
		/// </summary>
		/// <param name="NodeIn">Root of the Subtree to process.</param>
		/// <param name="StringBuilder">The string builder to populate with the output</param>
		private void BuildExpandedNodesString( SourceTreeNode NodeIn, StringBuilder StringBuilder )
		{
			if ( NodeIn.IsExpanded )
			{
				StringBuilder.Append(NodeIn.FullTreeviewPath);
				StringBuilder.Append(TreeViewPathSeparator);
			}
			foreach ( SourceTreeNode CurNode in NodeIn.Children )
			{
				BuildExpandedNodesString(CurNode, StringBuilder);
			}
		}

		/// <summary>
		/// Converts a string representing the path to a package on disk into a standard format where the first part of the
		/// converted name is the name of the immediate subdirectory of the root UnrealEngine3 folder. i.e. 
		/// ..\..\Engine\Content\ BECOMES Engine\Content\
		/// 
		/// D:\UnrealEngine3\Engine\Content BECOMES Engine\Content
		/// </summary>
		/// <param name="PackagePathName"></param>
		/// <returns>see comments above</returns>
		public String NormalizePackageFilePathName( String PackagePathName )
		{
			return UnrealEd.Utils.NormalizePackageFilePathName(PackagePathName, RootDirectory, StartupDirectory);
		}

        /// <summary>
        /// Add a package to the tree of packages. The package will be added to according to the path.
        /// Any intermediate folders in the path will be created if they do not already exist.
        /// If the package does not exist, create
        /// </summary>
        /// <param name="PackagePath">The path to the package; includes package name.</param>
		public Package AddPackage( String PackagePathName, bool UsingFlatList )
		{
			PackagePathName = NormalizePackageFilePathName(PackagePathName);
			List<String> pathToPackage = new List<String>(System.IO.Path.GetDirectoryName(PackagePathName).Split(System.IO.Path.DirectorySeparatorChar));
			String packageName = System.IO.Path.GetFileNameWithoutExtension(PackagePathName);

			if (UsingFlatList)
			{
				Package NewPackage = new Package(this.RootFolder, packageName);
				return ((Folder)this.RootFolder).AddChildNode<Package>(NewPackage);
			}
			else
			{
				// Ensure that the path exists
				Folder curFolder = this.RootFolder;
				foreach (String folderName in pathToPackage)
				{
					curFolder = curFolder.GetSubFolder(folderName, true);
				}

				// Add package
				// The unreal object path here would always be its name because these are top-level packages.
				Package NewPackage = new Package(curFolder, packageName);
				return curFolder.AddChildNode<Package>(NewPackage);
			}		
			
        }
		public GroupPackage AddGroup( String OuterPathName, GroupPackage Group )
		{
			ObjectContainerNode ParentNode = FindDescendantNode(OuterPathName) as ObjectContainerNode;
			if ( ParentNode != null )
			{
				return ParentNode.AddChildNode<GroupPackage>(Group);
			}
			return null;
		}
		public GroupPackage AddGroup( String OuterPathName, String GroupName )
		{
			ObjectContainerNode ParentNode = FindDescendantNode(OuterPathName) as ObjectContainerNode;
			if ( ParentNode != null )
			{
				return ParentNode.AddChildNode<GroupPackage>(new GroupPackage(ParentNode, GroupName));
			}
			return null;
		}
		public GroupPackage AddGroup( ObjectContainerNode ParentNode, GroupPackage Group )
		{
			GroupPackage Result = null;
			if ( ParentNode != null )
			{
				Result = ParentNode.AddChildNode<GroupPackage>(Group);
			}
			return Result;
		}
		public GroupPackage AddGroup( ObjectContainerNode ParentNode, String GroupName )
		{
			GroupPackage Result = null;
			if ( ParentNode != null )
			{
				Result = ParentNode.AddChildNode<GroupPackage>(new GroupPackage(ParentNode, GroupName));
			}
			return Result;
		}

		/// <summary>
		/// Remove the specified object node from the tree view.  If the parent is a folder and the last node is removed,
		/// the folder will be removed as well.
		/// </summary>
		/// <param name="PackageToRemove">the item that should be removed</param>
		public void RemovePackage( ObjectContainerNode PackageToRemove )
		{
			if ( PackageToRemove != null && PackageToRemove.Parent != null )
			{
				SourceTreeNode TreeNode = PackageToRemove.Parent as SourceTreeNode;
				if ( TreeNode.RemoveChildNode(PackageToRemove) )
				{
					Folder ParentFolder = TreeNode as Folder;
					while ( ParentFolder != null && ParentFolder.Children.Count == 0 && ParentFolder.Parent != null )
					{
						SourceTreeNode GrandparentFolder = ParentFolder.Parent as SourceTreeNode;
						if ( GrandparentFolder != null )
						{
							// if the folder is now empty, remove it too!
							GrandparentFolder.RemoveChildNode(ParentFolder);
						}

						// it's possible that removing this folder removed the last child of the next folder up
						ParentFolder = GrandparentFolder as Folder;
					}
				}
			}
		}

		/// <summary>
		/// Finds a descendant node in the tree given a path. The path is separated by /. e.g. Engine/Content/Maps/MyMap
		/// </summary>
		/// <param name="SourceTreePath">Path with / separating nodes.</param>
		/// <returns>The found node; null if node was not found.</returns>
		public SourceTreeNode FindDescendantNode( String SourceTreePath )
		{
			char[] NodeSeparator = { SourceTreeNode.TreeViewNodeSeparator };

			// convert a file-system path name into a tree-pathname, if necessary
			SourceTreePath = SourceTreePath.Replace(System.IO.Path.DirectorySeparatorChar, SourceTreeNode.TreeViewNodeSeparator);
			List<String> PathSegments = new List<String>(SourceTreePath.Split(NodeSeparator, StringSplitOptions.RemoveEmptyEntries));

			SourceTreeNode CurNode = RootFolder;

			foreach ( String PathSegment in PathSegments )
			{
				SourceTreeNode NextNode = null;

				//Find next node in the path
				for ( int ChildIdx = 0; NextNode == null && ChildIdx < CurNode.Children.Count; ++ChildIdx )
				{
					SourceTreeNode CurChild = CurNode.Children[ChildIdx] as SourceTreeNode;
					if ( CurChild.Name == PathSegment )
					{
						NextNode = CurChild;
					}
				}

				if ( NextNode == null )
				{
					// We could not find this node in the path; returning null
					return null;
				}
				else
				{
					CurNode = NextNode;
				}
			}

			return CurNode;
		}

		/// <summary>
		/// Find the source tree node given a full path to it
		/// </summary>
		/// <param name="FullPath"></param>
		/// <returns></returns>
		public Folder FindFolder( String FullPath )
		{
			List<String> Path = new List<String>( FullPath.Split(System.IO.Path.DirectorySeparatorChar) );
			
			Folder CurNode = this.RootFolder;
			for (int PathIdx = 0; PathIdx < Path.Count && CurNode != null; ++PathIdx)
			{
				String Directory = Path[PathIdx];
				CurNode = CurNode.GetSubFolder(Directory, false);
			}
			return CurNode;
		}

		public enum EPackageSearchOptions
		{
			RequireFullMatch,
			AllowIncompleteMatch,
		}

		/// <summary>
		/// Searches the tree for the a package with the specified path-name.
		/// </summary>
		/// <param name="PackagePathName">The path-name (not including folder name) for the package to find</param>
		/// <returns></returns>
		public ObjectContainerNode FindPackage( String PackagePathName, EPackageSearchOptions SearchMode  )
		{
			ObjectContainerNode Result = null;

			if ( RootFolder != null )
			{
				char[] dot = {'.'};
				List<String> ObjectNames = new List<String>(PackagePathName.Split(dot, StringSplitOptions.None));

				SourceTreeNode CurrentNode = RootFolder;
				while ( CurrentNode != null && ObjectNames.Count > 0 )
				{
					SourceTreeNode NextNode = CurrentNode.FindPackage(ObjectNames[0]);
					ObjectNames.RemoveAt(0);

					if ( NextNode == null && SearchMode != EPackageSearchOptions.RequireFullMatch )
					{
						break;
					}

					CurrentNode = NextNode;
				}
				Result = CurrentNode as ObjectContainerNode;
			}

			return Result;
		}
		/// <summary>
		/// Searches the tree for the a package with the specified path-name.
		/// </summary>
		/// <param name="PackagePathName">The path-name (not including folder name) for the package to find</param>
		/// <returns></returns>
		public ObjectContainerNode FindPackage( String PackagePathName )
		{
			ObjectContainerNode Result = null;

			if ( RootFolder != null )
			{
				char[] dot = { '.' };
				List<String> ObjectNames = new List<String>(PackagePathName.Split(dot, StringSplitOptions.None));

				SourceTreeNode CurrentNode = RootFolder;
				while ( CurrentNode != null && ObjectNames.Count > 0 )
				{
					Result = CurrentNode.FindPackage(ObjectNames[0]);
					CurrentNode = Result;

					ObjectNames.RemoveAt(0);
				}
			}

			return Result;
		}

		/// <summary>
		/// Get the list of all nodes in the tree
		/// </summary>
		/// <typeparam name="TNodeType">the node type to filter by, or SourceTreeNode for no filtering</typeparam>
		/// <param name="out_Nodes">receives the list of child nodes</param>
		public void GetChildNodes<TNodeType>( List<TNodeType> out_Nodes ) where TNodeType : SourceTreeNode
		{
			GetChildNodes<TNodeType>(out_Nodes, true);
		}
		public void GetChildNodes<TNodeType>( List<TNodeType> out_Nodes, bool bRecurse ) where TNodeType : SourceTreeNode
		{
			GetChildNodes<TNodeType>(RootFolder, out_Nodes, bRecurse);
		}
		
		/// <summary>
		/// Get the list of child nodes for the specified parent node.
		/// </summary>
		/// <typeparam name="TNodeType">the node type to filter by, or SourceTreeNode for no filtering</typeparam>
		/// <param name="parentNode">the node containing the child nodes being searched for</param>
		/// <param name="out_Nodes">receives the list of child nodes</param>
		/// <param name="bRecurse">indicates whether the search include children et al, of child nodes</param>
		public void GetChildNodes<TNodeType>( SourceTreeNode parentNode, List<TNodeType> out_Nodes, bool bRecurse ) where TNodeType : SourceTreeNode
		{
			if ( parentNode != null && out_Nodes != null )
			{
				foreach ( SourceTreeNode node in parentNode.Children )
				{
					TNodeType TNode = node as TNodeType;
					if ( TNode != null )
					{
						// if( out_Nodes.Contains( TNode ) ) throw new Exception( "Error: Duplicate entry detected" );
						out_Nodes.Add(TNode);
					}
				}

				if ( bRecurse )
				{
					foreach ( SourceTreeNode node in parentNode.Children )
					{
						GetChildNodes<TNodeType>(node, out_Nodes, true);
					}
				}
			}
		}


		#endregion

		#region Package Status


		/// <summary>
		/// Updates the status for all packages in the view model
		/// </summary>
		public void UpdateStatusForAllPackages()
		{
			UpdateStatusForAllPackagesRecursively( RootFolder );
		}
	
		/// <summary>
		/// Updates the status for the specified package
		/// </summary>
		public void UpdateStatusForPackage( Package Pkg )
		{
			if( m_ContentBrowser != null )
			{
				// Update the status of this package!
				m_ContentBrowser.Backend.UpdatePackagesTreeUI( Pkg );
			}
		}


		
		/// <summary>
		/// Recursively updates status for all packages in the specified node's hierarchy
		/// </summary>
		private void UpdateStatusForAllPackagesRecursively( SourceTreeNode ParentNode )
		{
			// If this is a package then update it's status
            Package pkgNode = ParentNode as Package;
            if( pkgNode != null )
			{
				UpdateStatusForPackage( pkgNode );
            }

			// Recurse!
            foreach( SourceTreeNode CurChildNode in ParentNode.Children )
            {
				UpdateStatusForAllPackagesRecursively( CurChildNode );
            }
		}


		#endregion
	}


    /// <summary>
    /// The base class for nodes in a sources tree. All SourceTreeNode's have a name and a collection of children.
    /// </summary>
	[DebuggerDisplay("{Name} {Children.Count} Children")]
	public class SourceTreeNode : AbstractTreeNode, INotifyPropertyChanged
    {
		/// <summary>
		/// Construct a source tree node
		/// </summary>
		/// <param name="ParentIn">The parent: node under which this node will appear.</param>
		public SourceTreeNode( AbstractTreeNode ParentIn ):
			base( ParentIn )
		{
			
		}

		private String m_Name;
		/// The name of this node.
        virtual public String Name
        {
            get { return m_Name; }
            set 
			{
				m_Name = value;
				NotifyPropertyChanged("Name");
			}
        }

		protected ObservableCollection<AbstractTreeNode> m_Children = new ObservableCollection<AbstractTreeNode>();
		/// The child SourceTreeNodes.
		public override ObservableCollection<AbstractTreeNode> Children { get { return m_Children; } }


		/// <summary>
		/// Adjusts the position of this node to bring it into view within the parent scroll-window
		/// </summary>
		/// <returns>true if the node was successfully brought into view</returns>
		public bool EnsureVisible()
		{
			bool bResult = true;

			// starting at the root
			List<SourceTreeNode> PathToThisNode = new List<SourceTreeNode>();
			for ( SourceTreeNode node = this; node != null; node = node.Parent as SourceTreeNode )
			{
				PathToThisNode.Insert(0, node);
			}

			ItemsControl ic = OwnerTree;
			SourceTreeNode previousNode = PathToThisNode[0];
			for ( int NodeIdx = 1; NodeIdx < PathToThisNode.Count; NodeIdx++ )
			{
				bool foundContainer = false;

				SourceTreeNode node = PathToThisNode[NodeIdx];
				CustomVirtualizingStackPanel itemsHost = previousNode.FindVisualChild<CustomVirtualizingStackPanel>(ic);
	
				if ( itemsHost != null )
				{
					int childIndex;

					// OwnerTree.Children points to the same nodes as OwnerTree.Root.Children, so if we are starting the search
					// go directly to the children of the root node
					if ( previousNode == null )
					{
						childIndex = PathToThisNode[0].Children.IndexOf(node);
					}
					else
					{
						childIndex = previousNode.Children.IndexOf(node);
					}

					// Due to virtualization, BringIntoView may not predict the offset correctly the first time.
					ItemsControl nic = null;
					while ( nic == null && (childIndex >= 0 && childIndex < ic.Items.Count) )
					{
						foundContainer = true;
						itemsHost.BringIntoView(childIndex);
						Dispatcher.Invoke(DispatcherPriority.Background, (DispatcherOperationCallback)delegate(object unused)
						{
							nic = (ItemsControl)ic.ItemContainerGenerator.ContainerFromIndex(childIndex);
							return null;
						}, null);
					}

					ic = nic;
				}
				previousNode = node;

				if ( !foundContainer || ic == null )
				{
					// abort
					bResult = false;
					break;
				}
			}

			if ( bResult )
			{
				OwnerTree.Select(this);
			}

			return bResult;
		}

		/// <summary>
		/// Add a new node as a child of this one. Insert the child alphabetically.
		/// If a child with this name and type already exists, replace the existing child.
		/// Folders always come first.
		/// </summary>
		/// <param name="NewChild"></param>
		/// <returns></returns>
		public TNodeType AddChildNode<TNodeType>( TNodeType NewChild ) where TNodeType : ObjectContainerNode
		{
			int FoundIndex = FindChildInsertionLocation(NewChild);
			
			if ( FoundIndex < this.Children.Count )
			{
				SourceTreeNode NodeAtFoundLocation = ( (SourceTreeNode)Children[FoundIndex] );
				if (NodeAtFoundLocation.Name == NewChild.Name)
				{
					NewChild.InitializeNode( m_Children[FoundIndex] );
					m_Children[FoundIndex] = NewChild;
				}
				else
				{
					m_Children.Insert( FoundIndex, NewChild );
				}
			}
			else
			{
				m_Children.Add( NewChild );
			}					

			return NewChild;
		}


		/// <summary>
		/// Return the index into the children array for the specified node. If not found, returns the index which is the closest match.
		/// </summary>
		/// <param name="ChildNode">the node to search for; searches are performed by name</param>
		/// <returns>the index into the children array for a node with a name matching the node specified</returns>
		private int FindChildInsertionLocation( SourceTreeNode ChildNode )
		{
			if ( ChildNode is Folder )
			{
				// Folders should always come before packages.
				for ( int CurIndex = 0; CurIndex < Children.Count; ++CurIndex )
				{
					SourceTreeNode CurNode = (SourceTreeNode)Children[CurIndex];
					if ( !(CurNode is Folder) )
					{
						return CurIndex;
					}
					else if ( String.Compare(ChildNode.Name, CurNode.Name, true) <= 0 )
					{
						return CurIndex;
					}
				}
				return Children.Count;
			}
			else
			{
				for ( int CurIndex = 0; CurIndex < Children.Count; ++CurIndex )
				{
					SourceTreeNode CurNode = (SourceTreeNode)Children[CurIndex];
					if ( !(CurNode is Folder) && String.Compare(ChildNode.Name, CurNode.Name, true) <= 0 )
					{
						return CurIndex;
					}
				}

				return Children.Count;
			}

		}


		/// <summary>
		/// Removes the specified node from the tree.  For now, non-recursive - doesn't allow you to call on the root node for example (unless it's a direct child of the root node)
		/// </summary>
		/// <param name="ChildToRemove"></param>
		/// <returns></returns>
		public bool RemoveChildNode( SourceTreeNode ChildToRemove )
		{
			bool bResult = false;

			if ((OwnerTree == null || OwnerTree.AllowNodeRemoval(ChildToRemove))
			&&	m_Children.Remove(ChildToRemove) )
			{
				if ( OwnerTree != null )
				{
					OwnerTree.NotifyRemovedNode(ChildToRemove);
				}
				bResult = true;
			}

			return bResult;
		}

		/// <summary>
		/// Search for a child node by name.
		/// </summary>
		/// <typeparam name="TNodeType">the type of node to search for; must be derived from SourceTreeNode</typeparam>
		/// <param name="NodeName">the name of the node to search for; will correspond to the node's Name</param>
		/// <param name="bAllowRecursion">indicates whether searching children of this node is allowed</param>
		/// <returns>the child node having the name specified, or null if no children have the name</returns>
		public TNodeType FindChildNode<TNodeType>( String NodeName, bool bAllowRecursion ) where TNodeType : SourceTreeNode
		{
			TNodeType Result = null;
			foreach ( AbstractTreeNode node in m_Children )
			{
				SourceTreeNode treeNode = node as SourceTreeNode;
				if ( treeNode != null && treeNode is TNodeType && String.Equals(treeNode.Name, NodeName, StringComparison.OrdinalIgnoreCase) )
				{
					Result = treeNode as TNodeType;
					break;
				}
			}

			if ( Result == null && bAllowRecursion )
			{
				foreach ( AbstractTreeNode node in m_Children )
				{
					SourceTreeNode treeNode = node as SourceTreeNode;
					if ( treeNode != null )
					{
						Result = treeNode.FindChildNode<TNodeType>(NodeName, bAllowRecursion);
						if ( Result != null )
						{
							break;
						}
					}
				}
			}

			return Result;
		}

		
		/// <summary>
		/// Finds the first child of the specified WPF visual object that is of the correct type. 
		/// </summary>
		/// <typeparam name="T">indicates the type of child to look for</typeparam>
		/// <param name="visual">the parent object to search in</param>
		/// <param name="bAllowRecursion">specify true to allow searching recursively through children of the specified object</param>
		/// <returns>reference to a child of the correct type, or null if none was found</returns>
		private T FindVisualChild<T>( Visual visual, bool bAllowRecursion ) where T : Visual
		{
			for ( int i = 0; i < VisualTreeHelper.GetChildrenCount(visual); i++ )
			{
				Visual child = (Visual)VisualTreeHelper.GetChild(visual, i);
				if ( child != null )
				{
					T correctlyTyped = child as T;
					if ( correctlyTyped != null )
					{
						return correctlyTyped;
					}

					if ( bAllowRecursion )
					{
						T descendent = FindVisualChild<T>(child, true);
						if ( descendent != null )
						{
							return descendent;
						}
					}
				}
			}

			return null;
		}
		private T FindVisualChild<T>( Visual visual ) where T : Visual
		{
			return FindVisualChild<T>(visual, true);
		}
		
		/// <summary>
        /// Find a Package with a given name. Recurse through children
        /// </summary>
        /// <param name="SearchName">The Name of the TreeNode to find</param>
        /// <returns>The package that was found. Null if nothing was found.</returns>
		public ObjectContainerNode FindPackage( String SearchName )
        {
			ObjectContainerNode Result = null;

			ObjectContainerNode pkgNode = this as ObjectContainerNode;
			if ( pkgNode != null && pkgNode.Name == SearchName )
            {
                // We called FindPackage() directly on the package that we were looking for.
                Result = pkgNode;
            }
            else
            {
				Result = FindChildNode<ObjectContainerNode>(SearchName, true);
            }

			return Result;
        }

		/// Separator between path segments in a path.
		public static char TreeViewNodeSeparator = '/';
		/// Get the complete name of this node starting from the root node. e.g. /Engine/Content/Weapons/Lancer
		public String FullTreeviewPath
		{
			get
			{
				SourceTreeNode ParentNode = this.Parent as SourceTreeNode;
				if ( ParentNode != null )
				{
					return ParentNode.FullTreeviewPath + TreeViewNodeSeparator + this.Name;
				}
				else
				{
					return String.Empty; // The root is an empty string
				}				
			}
		}
		
		#region INotifyPropertyChanged implementation

		/// Handler for property change notification
		public event PropertyChangedEventHandler PropertyChanged;
		/// Helper for raising property changed events
		protected virtual void NotifyPropertyChanged(String info)
		{
			if (PropertyChanged != null)
			{
				PropertyChanged(this, new PropertyChangedEventArgs(info));
			}
		}

		#endregion

    }

    /// <summary>
    /// A SourceTreeNode that represents a disk folder; folders can contain packages and other folders.
    /// </summary>
    public class Folder : SourceTreeNode
    {
        /// <summary>
        /// Folder Constructor.
        /// </summary>
		/// <param name="ParentIn">Parent tree node</param>
        /// <param name="NameIn">The name of the folder.</param>
        public Folder( AbstractTreeNode ParentIn, String NameIn )
		: base(ParentIn)
        {
            Name = NameIn;
        }

		/// <summary>
        /// Adds a new folder with the given name. Does nothing if folder already exists.
        /// </summary>
        /// <param name="name">The name of a folder to add</param>
		/// <param name="bCreateIfMissing">When true, if a folder is not found it will be created</param>
        /// <returns>The folder that was found or created.</returns>
        public Folder GetSubFolder(String SubfolderName, bool bCreateIfMissing)
        {
			Folder Result = FindChildNode<Folder>(SubfolderName, false);
			if ( Result == null && bCreateIfMissing )
            {
                // Not found, so make one and return it
				Result = new Folder(this, SubfolderName);
				m_Children.Add(Result);
            }

			return Result;
		}
	}

	/// <summary>
	/// Represents a UPackage in the sources tree - either a top-level package or a group.
	/// </summary>
	public class ObjectContainerNode : SourceTreeNode
	{
		public enum PackageStatus
		{
			NotLoaded,
			PartiallyLoaded,
			FullyLoaded,
		}

		/// <summary>
		/// The different types of SCC icons we can show - note that the order of these enum values
		/// must match the order of 
		/// </summary>
		public enum TreeNodeIconType
		{
			/** SCC state is either unknown, or SCC doesn't apply to this type of node. */
			ICON_Unknown,

			/** File is checked out to current user. */
			ICON_CheckedOut,

			/** File is not checked out (but IS controlled by the source control system). */
			ICON_CheckedIn,

			/** File is not at the head revision - must sync the file before editing. */
			ICON_NotCurrent,

			/** File is new and not in the depot - needs to be added. */
			ICON_NotInDepot,

			/** File is checked out by another user and cannot be checked out locally. */
			ICON_CheckedOutOther,

			ICON_Group,

			/**
			 * Certain packages are best ignored by the SCC system (MyLevel, Transient, etc).
			 */
			ICON_Ignore,
		};

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParent">the parent to use for node</param>
		/// <param name="InName">the name of the package</param>
		public ObjectContainerNode( SourceTreeNode InParent, String InName )
		: base(InParent)
		{
			Name = InName;
		}

		#region Properties

		public static readonly DependencyProperty NodeIconProperty = DependencyProperty.Register(
			"NodeIcon",
			typeof(TreeNodeIconType),
			typeof(SourceTreeNode),
			new PropertyMetadata(TreeNodeIconType.ICON_Unknown)
			);

		/// The icon type to use for this tree node
		public TreeNodeIconType NodeIcon
		{
			get { return (TreeNodeIconType)GetValue(NodeIconProperty); }
			set { SetValue(NodeIconProperty, value); }
		}


		/// The status of the package: fully loaded, partially loaded, not loaded. This is a dependency property.
		virtual public PackageStatus Status
		{
			get { return (PackageStatus)GetValue(StatusProperty); }
			set { SetValue(StatusProperty, value); }
		}
		public static readonly DependencyProperty StatusProperty =
			DependencyProperty.Register( "Status", typeof( PackageStatus ), typeof( ObjectContainerNode ), new PropertyMetadata( PackageStatusChanged ) );

		static void PackageStatusChanged( DependencyObject Sender, DependencyPropertyChangedEventArgs e )
		{
			ObjectContainerNode This = Sender as ObjectContainerNode;

			foreach ( GroupPackage ChildPackage in This.Children )
			{
				ChildPackage.Status = (PackageStatus)e.NewValue;
			}			 
		}

		/// <summary>
		/// Get the Package associated with the top-level UPackage for this node's corresponding UObject
		/// </summary>
		/// <returns>the Package that corresponds to this node's top-level package</returns>
		virtual public Package OutermostPackage
		{
			get
			{
				Package Result = null;
				for ( SourceTreeNode node = this; node != null && node != OwnerTree.Root; node = node.Parent as SourceTreeNode )
				{
					Package pkg = node as Package;
					if ( pkg != null )
					{
						Result = pkg;
						break;
					}
				}
				return Result;
			}
		}

		public String ObjectPathName
		{
			get
			{
				ObjectContainerNode NextParent = Parent as ObjectContainerNode;
				if ( NextParent != null )
				{
					return NextParent.ObjectPathName + "." + Name;
				}
				return Name;
			}
		}
		
		#endregion
		#region INotifyPropertyChanged implementation

		/// Helper for raising property changed events
		override protected void NotifyPropertyChanged( String info )
		{
			if ( info == "Status" )
			{
				foreach ( GroupPackage group in m_Children )
				{
					group.Status = Status;
				}
			}

			base.NotifyPropertyChanged(info);
		}

		#endregion
	}

    /// <summary>
    /// Represents a package in a source tree. A package can contain other packages.
    /// </summary>
	public class Package : ObjectContainerNode
    {
        /// <summary>
        /// Construct a package.
        /// </summary>
		/// <param name="InParent">Parent tree node</param>
		/// <param name="InName">The name of the package.</param>
        public Package(SourceTreeNode InParent, String InName)
		: base(InParent, InName)
        {
        }

		/// <summary>
		/// Simple wrapper to encapsulate setting the package's DisplayName according to whether the package is dirty or not.
		/// </summary>
		private void UpdateDisplayName()
		{
			DisplayName = IsModified ? Name + "*" : Name;
		}

		#region Properties

		private String m_DisplayName;
		/// <summary>
		/// The name to display in the tree view for this package.  Might contain an asterisk if the package is dirty.
		/// </summary>
		public String DisplayName
		{
			get { return m_DisplayName; }
			set
			{
				m_DisplayName = value;
				NotifyPropertyChanged("DisplayName");
			}
		}

		/// <summary>
		/// Indicates whether this package is modified.
		/// </summary>
		private bool m_Dirty = false;
		public bool IsModified
		{
			get { return m_Dirty; }
			set
			{
				m_Dirty = value;
				NotifyPropertyChanged("IsModified");
				UpdateDisplayName();
			}
		}

		/// <summary>
		/// The name of the UPackage associated with this Package.  Ensures that DisplayName is kept synchronized
		/// with the value of Name & IsModified
		/// </summary>
		override public String Name
		{
			get { return base.Name; }
			set
			{
				base.Name = value;
				UpdateDisplayName();
			}
		}

		/// <summary>
		/// Get the Package associated with the top-level UPackage for this node's corresponding UObject
		/// </summary>
		/// <returns>the Package that corresponds to this node's top-level package</returns>
		override public Package OutermostPackage
		{
			get { return this; }
		}

		#endregion
	}

	

	/// <summary>
	/// Represents a group package (package within a package).
	/// </summary>
	public class GroupPackage : ObjectContainerNode
	{
        /// <summary>
        /// Construct a group
        /// </summary>
		/// <param name="ParentIn">Parent tree node</param>
		/// <param name="InName">name of the group</param>
		public GroupPackage( SourceTreeNode InParent, String InName )
		: base(InParent, InName)
        {
			ObjectContainerNode ParentNode = InParent as ObjectContainerNode;
			Status = ParentNode.Status;
        }

		static GroupPackage()
		{
			ObjectContainerNode.NodeIconProperty.OverrideMetadata(
				typeof(GroupPackage),
				new PropertyMetadata(TreeNodeIconType.ICON_Group)
				);

		}

		#region Properties
		/// <summary>
		/// Get the name of the package containing this group. If this is a package then the package's name is returned.
		/// </summary>
		public String OutermostPackageName
		{
			get { return OutermostPackage.Name; }
		}

		#endregion
	}



	/// <summary>
	/// Model for a Collection
	/// </summary>
	public class Collection : DependencyObject
	{
		/// <summary>
		/// Create a named collection.
		/// </summary>
		/// <param name="InName">The name of the collection</param>
		/// <param name="bIsLocal">True if the collection is local and not one saved in the GAD</param>
		public Collection( String InName, bool bIsLocal )
		{
			this.mName = InName;
            this.mIsLocal = bIsLocal;
		}

        private bool mIsLocal;
		private String mName;

		/// Gets the real name of this Collection
		public String Name
		{
			get { return mName; }
		}

        /// Gets the display name of this collection
        public String DisplayName
        {
            get 
            {
                if( mIsLocal )
                {
					// If the collection is local, append the word "Local" on the end as an indication
                    return Name + " (Local)";
                }
                else
                {
                    return Name;
                }
            }
        }

        /// Get or set the name of this Collection
        public bool IsLocal
        {
            get { return mIsLocal; }
        }
	}
}
