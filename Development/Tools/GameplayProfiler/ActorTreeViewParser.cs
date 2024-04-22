using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace GameplayProfiler
{
	class ActorTreeViewParser
	{
		/** Mapping from level name index to its node in the tree. */
		static Dictionary<Int32,TreeNode> LevelNameIndexToNodeMap = new Dictionary<Int32,TreeNode>();
		/** Mapping from actor name to its node in the tree. */
		static Dictionary<string,TreeNode> ActorNameToNodeMap = new Dictionary<string,TreeNode>();

		/**
		 * Payload for each node in the tree.	
		 */
		class NodePayload
		{
			/** Description/ human readable name for the node. This is used in combination with time. */
			public string Description;
			/** Description/ human readable name for the node in grouped mode. E.g. per class. This is used in combination with time. */
			public string DescriptionGrouped;
			/** Number of inclusiev child nodes. */
			public int InclusiveChildren = 0;
			
			/** Counter associated with this node. */
			private TokenCounterBase Counter = null;
			/** Inclusive time in case object is null. */
			private float InclusiveTime = 0;

			/** Constructor, initializing all members. */
			public NodePayload(TokenCounterBase InCounter, string InDescription, string InDescriptionGrouped)
			{
				Counter = InCounter;
				Description = InDescription;
				DescriptionGrouped = InDescriptionGrouped;
			}

			/** Updates inclusive time for non-TokenCounter based nodes, like e.g. level nodes. */
			public void AddToInclusiveTime( float ChildTime )
			{
				// Counters already handle their own inclusive time.
				if( Counter == null )
				{
					InclusiveTime += ChildTime;
				}
				// Don't call on objects!
				else
				{
					throw new InvalidOperationException();
				}
			}

			/** @return inclusive time for this node. */
			public float GetInclusiveTime()
			{
				if( Counter != null )
				{
					return Counter.InclusiveTime;
				}
				else
				{
					return InclusiveTime;
				}
			}

			/**
			 * @return	Time string.
			 */
			private string GetTimeString()
			{
				return String.Format("{0:F2}", GetInclusiveTime()).PadLeft(6) + "  ";
			}

			/**
			 * Returns the display string for the associated node. It is a mix of time and description.
			 * 
			 * @param	ExclusiveChildren	If > 0 display after time.
			 * @return	Display string for associated node in "Inclusive Time    Description" format
			 */
			public string GetDisplayString( int ExclusiveChildren )
			{
				string DisplayName = GetTimeString() + Description;
				if( ExclusiveChildren >= 0 )
				{
					DisplayName = DisplayName.PadRight(40);
					DisplayName += " (" + ExclusiveChildren.ToString().PadLeft(5) + " excl, " + InclusiveChildren.ToString().PadLeft(5) + " incl)";
				}

				return DisplayName;
			}
		}

		/**
		 * Parses a frame of the passed in stream into the tree views taking into account display thresholds.
		 */
		public static void Parse(TreeView PerActorTreeView, TreeView PerClassTreeView, ProfilerStream ProfilerStream, int FrameNumber, float TimeThreshold, bool bShowCycleStats)
		{
			PerClassTreeView.BeginUpdate();
			PerActorTreeView.BeginUpdate();

			PerActorTreeView.Nodes.Clear();
			PerClassTreeView.Nodes.Clear();

			ParseActorTreeView( PerActorTreeView, ProfilerStream, FrameNumber, bShowCycleStats );
			ConvertActorTreeViewToClassTreeView( PerActorTreeView, PerClassTreeView );

			// Update object node text now that time has properly propagated.
			CullNodesAndUpdateNodeText(PerActorTreeView, TimeThreshold,false);
			CullNodesAndUpdateNodeText(PerClassTreeView, TimeThreshold,true);
			
			// Sort by time in descending order.
			PerActorTreeView.TreeViewNodeSorter = new NodeTimeSorter();
			PerClassTreeView.TreeViewNodeSorter = new NodeTimeSorter();

			PerActorTreeView.EndUpdate();
			PerClassTreeView.EndUpdate();
		}

		private static void ConvertActorTreeViewToClassTreeView( TreeView PerActorTreeView, TreeView PerClassTreeView )
		{
			// Iterate over all actor nodes. Luckily we still have that from calling ParseActorTreeView first.
			var ClassNameToNodeMap = new Dictionary<string, TreeNode>();
			foreach( var ActorToNodeMapping in ActorNameToNodeMap )
			{
				TreeNode ActorNode = (TreeNode) ActorToNodeMapping.Value.Clone();
				var ActorPayload = (NodePayload) ActorNode.Tag;
				string ClassName = ActorPayload.DescriptionGrouped;
				TreeNode ClassNode = null;

				if( ClassNameToNodeMap.ContainsKey( ClassName ) )
				{
					ClassNode = ClassNameToNodeMap[ClassName];
				}
				else
				{
					ClassNode = new TreeNode( "Class" );
					ClassNode.ForeColor = TableEntryColors.ClassColor;
					ClassNode.Tag = new NodePayload( null, ClassName, ClassName );
					PerClassTreeView.Nodes.Add(ClassNode);
					ClassNameToNodeMap.Add(ClassName,ClassNode);
				}

				ClassNode.Nodes.Add(ActorNode);
				((NodePayload)ClassNode.Tag).AddToInclusiveTime( ActorPayload.GetInclusiveTime() );
			}
		}

		/**
		 * Parses a frame of the passed in stream into the per actor tree view.
		 */
		private static void ParseActorTreeView(TreeView PerActorTreeView, ProfilerStream ProfilerStream, int FrameNumber, bool bShowCycleStats )
		{
			// Clear out existing lookup tables.
			LevelNameIndexToNodeMap.Clear();
			ActorNameToNodeMap.Clear();

			// Parse the frame's actors.
			var Frame = ProfilerStream.Frames[FrameNumber];
			foreach( var Actor in Frame.Actors )
			{
				// Find or create level node. Also adds it to the root of the tree.
				var LevelNode = FindOrCreateLevelNode( PerActorTreeView, Actor );

				if (!ActorNameToNodeMap.ContainsKey(Actor.GetActorPathName(false)))
                {
                    // Add actor node and children (recursive) and keep track of actor name to node mapping.
                    var ActorNode = AddToTree(Actor, LevelNode, bShowCycleStats);
                    ActorNameToNodeMap.Add(Actor.GetActorPathName(true), ActorNode);
                }
			}

			// Update level times.
			foreach( TreeNode LevelNode in PerActorTreeView.Nodes )
			{
				var LevelPayload = (NodePayload) LevelNode.Tag;
				foreach( TreeNode ActorNode in LevelNode.Nodes )
				{
					var ActorPayload = (NodePayload) ActorNode.Tag;
					LevelPayload.AddToInclusiveTime( ActorPayload.GetInclusiveTime() );
				}
			}
		}

		/**
		 * Recursively adds
		 */
		private static TreeNode AddToTree( TokenCounterBase Counter, TreeNode RootNode, bool bShowCycleStats )
		{
			TreeNode Node = RootNode;
			if( bShowCycleStats || !( Counter is TokenCycleStat ) )
			{
				Node = new TreeNode( "Counter" );
				if( Counter is TokenObject )
				{
					var Object = (TokenObject)Counter;
					Node.Tag = new NodePayload( Object, Object.GetObjectName(true), Object.GetClassName() );
				}
				else if( Counter is TokenCycleStat )
				{
					var CycleStat = (TokenCycleStat)Counter;
					Node.Tag = new NodePayload( CycleStat, CycleStat.GetCycleStatName(), "<STAT>" );
				}
				if( Counter is TokenFunction )
				{
					Node.ForeColor = TableEntryColors.FunctionColor;
				}
				else if( Counter is TokenActor )
				{
					Node.ForeColor = TableEntryColors.ActorColor;
				}
				else if( Counter is TokenComponent )
				{
					Node.ForeColor = TableEntryColors.ComponentColor;
				}
				else if( Counter is TokenCycleStat )
				{
					Node.ForeColor = TableEntryColors.CycleStatColor;
				}
				RootNode.Nodes.Add( Node );
			}
			foreach( var Child in Counter.Children ) 
			{
				AddToTree( Child, Node, bShowCycleStats );
			}
			return Node;
		}

		/**
		 * Finds of creates a level node and adds it to the root of the tree if not already there.
		 * 
		 * @param	LevelsAsRootTreeView	Tree view to add to
		 * @param	Actor					Actor to find/ add level for
		 */
		private static TreeNode FindOrCreateLevelNode( TreeView LevelsAsRootTreeView, TokenActor Actor )
		{
			var LevelNameIndex = Actor.OutermostNameIndex;
			TreeNode LevelNode = null;
			if( LevelNameIndexToNodeMap.ContainsKey(LevelNameIndex) )
			{
				// Found
				LevelNode = LevelNameIndexToNodeMap[LevelNameIndex];
			}
			else
			{
				// Not found. Create and add for future use.
				string LevelName = Actor.ProfilerStream.GetName(LevelNameIndex);
				LevelNode = new TreeNode(LevelName);
				LevelNode.ForeColor = TableEntryColors.LevelColor;
				LevelNode.Tag = new NodePayload( null, LevelName, LevelName );
				LevelsAsRootTreeView.Nodes.Add( LevelNode );
				LevelNameIndexToNodeMap.Add(LevelNameIndex, LevelNode);
			}
			return LevelNode;
		}

		/**
		 * Uses recursion to update inclusive child count for nodes.
		 */
		private static int UpdateNodeInclusiveChildCount( TreeNode RootNode )
		{
			var RootPayload = (NodePayload)RootNode.Tag;
			foreach( TreeNode ChildNode in RootNode.Nodes )
			{
				var ChildPayload = (NodePayload)ChildNode.Tag;
				ChildPayload.InclusiveChildren = UpdateNodeInclusiveChildCount( ChildNode );
				RootPayload.InclusiveChildren += ChildPayload.InclusiveChildren + 1;
			}
			return RootPayload.InclusiveChildren;
		}

		/**
		 * Iterate over all nodes based on passed in root node and either update node text or cull them
		 * from the tree if the elapsed time is below the threshold.
		 */
		private static void CullNodesAndUpdateNodeText(TreeView TreeView,float TimeThreshold,bool bIncludeCount)
		{
			// Compile a list of all nodes in the graph without recursion... we do however use recursion if count is wanted.
			var NodesToUpdate = new List<TreeNode>();
			var NodesToRemove = new List<TreeNode>();
			foreach( TreeNode RootNode in TreeView.Nodes )
			{
				if( bIncludeCount )
				{
					UpdateNodeInclusiveChildCount( RootNode );
				}
				NodesToUpdate.Add( RootNode );
			}
			int NodeIndex = 0;
			while( NodeIndex < NodesToUpdate.Count )
			{
				var Node = NodesToUpdate[NodeIndex];
				var Payload = (NodePayload) Node.Tag;
				if( Payload.GetInclusiveTime() < TimeThreshold )
				{
					NodesToRemove.Add(Node);
				}
				else
				{
					foreach( TreeNode ChildNode in Node.Nodes )
					{
						NodesToUpdate.Add(ChildNode);
					}
				}
				NodeIndex++;
			}

			// Iterate over all nodes to update display text. Done before removing as we might display
			// number of child nodes.
			foreach( TreeNode Node in NodesToUpdate )
			{
				var Payload = (NodePayload) Node.Tag;					
				Node.Text = Payload.GetDisplayString( bIncludeCount ? Node.Nodes.Count : -1 );				
			}

			// Iterate over all nodes to remove and remove them.
			foreach( TreeNode Node in NodesToRemove )
			{
				if( Node.Parent != null )
				{
					// regular node
					Node.Parent.Nodes.Remove( Node );
				}
				else
				{
					// root node
					TreeView.Nodes.Remove( Node );
				}
			}
		}

		/**
		 * Node sorter that implements the IComparer interface. Nodes are sorted by time in descending order.
		 */ 
		public class NodeTimeSorter : System.Collections.IComparer
		{
			public int Compare(object ObjectA, object ObjectB)
			{
				TreeNode NodeA = ObjectA as TreeNode;
				TreeNode NodeB = ObjectB as TreeNode;
				NodePayload PayloadA = NodeA.Tag as NodePayload;
				NodePayload PayloadB = NodeB.Tag as NodePayload;
				// Can only sort if there is payload.
				if( PayloadA != null && PayloadB != null )
				{
					// Sort by size, descending.
					return Math.Sign( PayloadB.GetInclusiveTime() - PayloadA.GetInclusiveTime() );
				}
				// Treat missing payload as unsorted
				else
				{
					return 0;
				}
			}
		}
	}
}
