using System;
using System.Collections.Generic;
using System.IO;
using System.Windows.Forms;
using System.Text;
using System.Drawing;

namespace GameplayProfiler
{
	class FunctionTreeViewParser
	{
		/**
		 * Payload for each node in the tree.	
		 */
		public class NodePayload
		{
			/** Inclusive time. */
			public float InclusiveTime = 0;
			/** Number of calls. */
			public float CallCount = 0;
			/** Name of this payload entry (function, or cycle stat.) */
			public string Name;

			/** Constructor, initializing all members. */
			public NodePayload( string InName )
			{
				Name = InName;
			}

			/**
			 * @return	Time string.
			 */
			private string GetTimeString()
			{
				return String.Format("{0:F2}", InclusiveTime).PadLeft(7) + " ";
			}

			/**
			 * Returns the display string for the associated node. It is a mix of time and description.
			 * 
			 * @return	Display string for associated node in "Inclusive Time    Description" format
			 */
			public string GetDisplayString()
			{
				return GetTimeString() + CallCount.ToString().PadLeft(6) + "  " + Name;
			}
		}


		/**
		 * Parses a frame of the passed in stream into the function tree view taking into account display thresholds.
		 */
		public static void Parse(TreeView FunctionTreeView, ProfilerStream ProfilerStream, int FrameIndex, bool bOnlyParseSingleFrame, float TimeThreshold, bool bShowCycleStats)
		{
			FunctionTreeView.BeginUpdate();
			FunctionTreeView.Nodes.Clear();
			
			// Create temporary root node so we always have a parent. We'll simply remove it as a post process step.
			var RootNode = new TreeNode( "TempRoot" );
			var CurrentNode = RootNode;
			var ActiveCounters = new Stack<TokenCounterBase>();

			// Iterate over tokens starting at passed in frame. We either iterate till the frame or end of stream
			// marker based on passed in option.
			bool bStopParsing = false;
			for (int TokenIndex = ProfilerStream.Frames[FrameIndex].StartIndex; true; TokenIndex++)
			{
				var Token = ProfilerStream.Tokens[TokenIndex];
				var Counter = Token as TokenCounterBase;
				switch (Token.TokenType)
				{
					case ETokenTypes.Function:
						CurrentNode = AddFunction( CurrentNode, (TokenFunction)Counter );
						ActiveCounters.Push( Counter );
						break;
					case ETokenTypes.Actor:
					case ETokenTypes.Component:
						ActiveCounters.Push( Counter );
						break;
					case ETokenTypes.CycleStat:
						if( bShowCycleStats )
						{
							CurrentNode = AddCycleStat( CurrentNode, (TokenCycleStat)Token );
						}
						ActiveCounters.Push( Counter );
						break;
					case ETokenTypes.EndOfScope:
						var CurrentCounter = ActiveCounters.Pop();
						if( CurrentCounter is TokenFunction ||
							( bShowCycleStats && CurrentCounter is TokenCycleStat ) )
						{
							CurrentNode = CurrentNode.Parent;
						}
						break;
					case ETokenTypes.Frame:
						if( bOnlyParseSingleFrame )
						{
							bStopParsing = true;
						}
						break;
					case ETokenTypes.EndOfStream:
						bStopParsing = true;
						break;
					default:
						throw new InvalidDataException();
				}

				if (bStopParsing)
				{
					break;
				}
			}

			// Add "Self" entries to the tree
			RecursivelyGenerateSelfEntries( RootNode );

			// Recursively updates node text with final time information.
			RecursivelyUpdateNodeText( RootNode );

			// Add root nodes's node to tree.
			foreach( TreeNode Node in RootNode.Nodes )
			{
				FunctionTreeView.Nodes.Add( Node );
			}

			// Remove any nodes with insignificant inclusive times
			CullNodes( FunctionTreeView, TimeThreshold );

			FunctionTreeView.TreeViewNodeSorter = new NodeTimeSorter();
			FunctionTreeView.EndUpdate();
		}

		/**
		 * Adds the passed in function to the passed in node or updates if already a child.
		 */
		private static TreeNode AddFunction( TreeNode ParentNode, TokenFunction Function )
		{
			string FunctionName = Function.GetFunctionName();
			TreeNode FunctionNode = null;
			NodePayload FunctionPayload = null;
			
			// Look whether parent already has a node for this function.
			foreach( TreeNode ChildNode in ParentNode.Nodes )
			{
				var Payload = (NodePayload) ChildNode.Tag;
				if( Payload.Name == FunctionName )
				{
					FunctionNode = ChildNode;
					FunctionPayload = Payload;
					break;
				}
			}

			// Create node if parent doesn't already have one.
			if( FunctionNode == null )
			{
				FunctionNode = new TreeNode(FunctionName);
				FunctionPayload = new NodePayload( FunctionName );
				FunctionNode.ForeColor = TableEntryColors.FunctionColor;
				FunctionNode.Tag = FunctionPayload;
				ParentNode.Nodes.Add(FunctionNode);
			}

			// Update information.
			FunctionPayload.InclusiveTime += Function.InclusiveTime;
			FunctionPayload.CallCount++;

			return FunctionNode;
		}

		/**
		 * Adds the passed in cycle stat to the passed in node or updates if already a child.
		 */
		private static TreeNode AddCycleStat( TreeNode ParentNode, TokenCycleStat CycleStat )
		{
			string CycleStatName = CycleStat.GetCycleStatName();
			TreeNode CycleStatNode = null;
			NodePayload CycleStatPayload = null;

			// Look whether parent already has a node for this cycle stat.
			foreach( TreeNode ChildNode in ParentNode.Nodes )
			{
				var Payload = (NodePayload)ChildNode.Tag;
				if( Payload.Name == CycleStatName )
				{
					CycleStatNode = ChildNode;
					CycleStatPayload = Payload;
					break;
				}
			}

			// Create node if parent doesn't already have one.
			if( CycleStatNode == null )
			{
				CycleStatNode = new TreeNode( CycleStatName );
				CycleStatNode.ForeColor = TableEntryColors.CycleStatColor;
				CycleStatPayload = new NodePayload( CycleStatName );
				CycleStatNode.Tag = CycleStatPayload;
				ParentNode.Nodes.Add( CycleStatNode );
			}

			// Update information.
			CycleStatPayload.InclusiveTime += CycleStat.InclusiveTime;
			CycleStatPayload.CallCount++;

			return CycleStatNode;
		}

		/**
		 * Iterate over all nodes based on passed in root node and cull them from the tree if 
		 * the elapsed time is below the threshold.
		 */
		private static void CullNodes( TreeView TreeView, float TimeThreshold )
		{
			// Compile a list of all nodes in the graph without recursion... we do however use recursion if count is wanted.
			var Nodes = new List<TreeNode>();
			var NodesToRemove = new List<TreeNode>();
			foreach( TreeNode RootNode in TreeView.Nodes )
			{
				Nodes.Add( RootNode );
			}
			int NodeIndex = 0;
			while( NodeIndex < Nodes.Count )
			{
				var Node = Nodes[ NodeIndex ];
				var Payload = (NodePayload)Node.Tag;
				if( Payload.InclusiveTime < TimeThreshold )
				{
					NodesToRemove.Add( Node );
				}
				else
				{
					foreach( TreeNode ChildNode in Node.Nodes )
					{
						Nodes.Add( ChildNode );
					}
				}
				NodeIndex++;
			}

			// Iterate over all nodes to remove and remove them. This is done separately as we can't
			// remove nodes while using a foreach over the Nodes collection.
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

		private static void RecursivelyGenerateSelfEntries( TreeNode ParentNode )
		{
			double ChildrenInclusiveTime = 0.0;
			foreach( TreeNode Node in ParentNode.Nodes )
			{
				NodePayload Payload = (NodePayload)Node.Tag;
				ChildrenInclusiveTime += Payload.InclusiveTime;

				RecursivelyGenerateSelfEntries( Node );
				Node.Text = Payload.GetDisplayString();
			}

			// Don't bother adding a child 'Self' node if we have no children
			if( ParentNode.Nodes.Count > 0 )
			{
				// Make sure the parent node has valid data.  Root nodes won't have a payload.
				NodePayload ParentPayload = (NodePayload)ParentNode.Tag;
				if( ParentPayload != null )
				{
					// "<Self>" time is our parent function's exclusive time
					double SelfTime = Math.Max( 0.0, ParentPayload.InclusiveTime - ChildrenInclusiveTime );

					TreeNode SelfNode = null;
					NodePayload SelfPayload = null;

					SelfNode = new TreeNode( "<Self>" );
					SelfPayload = new NodePayload( "<Self>" );
					SelfPayload.InclusiveTime = (float)SelfTime;
					SelfPayload.CallCount = ParentPayload.CallCount;
					SelfNode.ForeColor = ParentNode.ForeColor;
					SelfNode.Tag = SelfPayload;

					ParentNode.Nodes.Add( SelfNode );
				}
			}
		}

		private static void RecursivelyUpdateNodeText( TreeNode ParentNode )
		{
			foreach( TreeNode Node in ParentNode.Nodes )
			{
				RecursivelyUpdateNodeText( Node );
				Node.Text = ((NodePayload) Node.Tag).GetDisplayString();
			}
		}

		/**
		 * Node sorter that implements the IComparer interface. Nodes are sorted by time in descending order.
		 */ 
		public class NodeTimeSorter : System.Collections.IComparer
		{
			public int Compare(object ObjectA, object ObjectB)
			{
				NodePayload PayloadA = (NodePayload)((TreeNode)ObjectA).Tag;
				NodePayload PayloadB = (NodePayload)((TreeNode)ObjectB).Tag;;
				// Sort by size, descending.
				return Math.Sign( PayloadB.InclusiveTime - PayloadA.InclusiveTime );
			}
		}
	}
}
