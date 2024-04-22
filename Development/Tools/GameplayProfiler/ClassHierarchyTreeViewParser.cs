using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Text;
using System.IO;

namespace GameplayProfiler
{
	class ClassHierarchyTreeViewParser
	{
		/**
		 * Payload for each node in the tree.	
		 */
		class NodePayload
		{
			/** Inclusive time. */
			public float InclusiveTime = 0;
			/** Class this node represents. */
			public Class NodeClass;

			/** Constructor, initializing all members. */
			public NodePayload(Class InNodeClass)
			{
				NodeClass = InNodeClass;
			}

			/**
			 * @return	Time string.
			 */
			private string GetTimeString()
			{
				return String.Format("{0:F2}", InclusiveTime).PadLeft(6) + "  ";
			}

			/**
			 * Returns the display string for the associated node. It is a mix of time and description.
			 * 
			 * @return	Display string for associated node in "Inclusive Time    Description" format
			 */
			public string GetDisplayString()
			{
				return GetTimeString() + NodeClass.ClassName;
			}
		}

		/**
		 * Parses a frame of the passed in stream into a class hierarchy tree view, taking into account display threshold.
		 */
		public static void Parse(TreeView ClassHierarchyTreeView, ProfilerStream ProfilerStream, int FrameNumber, float TimeThreshold)
		{
			ClassHierarchyTreeView.BeginUpdate();		
			ClassHierarchyTreeView.Nodes.Clear();
			ClassToNodeMap.Clear();

			// Add root actor, component, function nodes
			var ActorClassNode = new TreeNode("Actor");
			ActorClassNode.ForeColor = TableEntryColors.ActorColor;
			var ActorPayload = new NodePayload(ProfilerStream.ActorClass);
			ActorClassNode.Tag = ActorPayload;
			ClassToNodeMap.Add(ProfilerStream.ActorClass,ActorClassNode);
			ClassHierarchyTreeView.Nodes.Add(ActorClassNode);
			
			var ComponentClassNode = new TreeNode("ActorComponent");
			ComponentClassNode.ForeColor = TableEntryColors.ComponentColor;
			var ComponentPayload = new NodePayload( ProfilerStream.ComponentClass );
			ComponentClassNode.Tag = ComponentPayload;
			ClassToNodeMap.Add(ProfilerStream.ComponentClass,ComponentClassNode);
			ClassHierarchyTreeView.Nodes.Add(ComponentClassNode);
			
			var FunctionClassNode = new TreeNode("Function");
			FunctionClassNode.ForeColor = TableEntryColors.FunctionColor;
			var FunctionPayload = new NodePayload( ProfilerStream.FunctionClass );
			FunctionClassNode.Tag = FunctionPayload;
			ClassToNodeMap.Add(ProfilerStream.FunctionClass,FunctionClassNode);
			ClassHierarchyTreeView.Nodes.Add(FunctionClassNode);

			// Parse tokens, adding/ updating class hierarchy and such.
			bool bStopParsing = false;
			for (int TokenIndex = ProfilerStream.Frames[FrameNumber].StartIndex; true; TokenIndex++)
			{
				var Token = ProfilerStream.Tokens[TokenIndex];
				var Object = Token as TokenObject;
				switch (Token.TokenType)
				{
					case ETokenTypes.Function:
						AddToTree( FunctionClassNode, Object, true );
						break;
					case ETokenTypes.Actor:
						AddToTree( ActorClassNode, Object, false );
						break;
					case ETokenTypes.Component:
						AddToTree( ComponentClassNode, Object, true );
						break;
					case ETokenTypes.CycleStat:
						break;
					case ETokenTypes.EndOfScope:
						break;
					case ETokenTypes.Frame:
						// We only parse a single frame so we should stop when we encounter a frame marker.
						bStopParsing = true;
						break;
					// We should never encounter this due to way frames are parsed
					case ETokenTypes.EndOfStream:
					default:
						throw new InvalidDataException();
				}

				if (bStopParsing)
				{
					break;
				}
			}

			// Update and cull nodes. Need to update first for inclusive time to propagate.
			foreach( TreeNode RootNode in ClassHierarchyTreeView.Nodes )
			{
				UpdateNodes( RootNode );
			}
			CullNodes( ClassHierarchyTreeView, TimeThreshold );

			ClassHierarchyTreeView.TreeViewNodeSorter = new NodeTimeSorter();
			ClassHierarchyTreeView.EndUpdate();		
		}

		/** Mapping from a class to node in the tree. */
		private static Dictionary<Class,TreeNode> ClassToNodeMap = new Dictionary<Class,TreeNode>();

		private static void AddToTree( TreeNode RootNode, TokenObject Object, bool bOnlyAddExclusiveTime )
		{
			var RootNodePayload = (NodePayload) RootNode.Tag;
			var RootClass = RootNodePayload.NodeClass;
			var ObjectClass = Object.GetClass();
			
			// Verify that we're barking up the right tree.
			if( !ObjectClass.IsChildOf( RootClass ) )
			{
				throw new InvalidDataException();
			}

			// Add new node if not found.
			if( !ClassToNodeMap.ContainsKey( ObjectClass ) )
			{
				// Create stack of classes from current to root. Using stack as we need to create tree in
				// reverse order.
				var Classes = new Stack<Class>();
				Class CurrentClass = ObjectClass;
				while( CurrentClass != RootClass )
				{
					Classes.Push(CurrentClass);
					CurrentClass = CurrentClass.SuperClass;
				}

				// Add classes. We can rely on the SuperClass field to have a node associated.
				while( Classes.Count > 0 )
				{
					CurrentClass = Classes.Pop();
					if( !ClassToNodeMap.ContainsKey( CurrentClass ) )
					{
						var TempNode = new TreeNode(CurrentClass.ClassName);
						TempNode.ForeColor = TableEntryColors.ClassColor;
						TempNode.Tag = new NodePayload( CurrentClass );
						ClassToNodeMap.Add(CurrentClass,TempNode);
						var SuperClassNode = ClassToNodeMap[CurrentClass.SuperClass];
						SuperClassNode.Nodes.Add(TempNode);
					}
				}
			}

			var ClassNode = ClassToNodeMap[ObjectClass];
			var ClassPayload = (NodePayload)ClassNode.Tag;

			// Update time.
			if (bOnlyAddExclusiveTime)
			{
				float ExclusiveTime = Object.InclusiveTime - Object.ChildrenTime;
				ClassPayload.InclusiveTime += ExclusiveTime;

			}
			else
			{
				ClassPayload.InclusiveTime += Object.InclusiveTime;
			}
		}

		/**
		 * Recursively bubbles up inclusive time and updates node text.
		 * 
		 * @param	CurrentNode		Current node to recurse/ traverse
		 * @return	Inclusive time of passed in node
		 */
		private static float UpdateNodes( TreeNode CurrentNode )
		{
			var CurrentPayload = (NodePayload) CurrentNode.Tag;
			foreach( TreeNode ChildNode in CurrentNode.Nodes )
			{
				CurrentPayload.InclusiveTime += UpdateNodes( ChildNode );
			}
			CurrentNode.Text = CurrentPayload.GetDisplayString();
			return CurrentPayload.InclusiveTime;
		}

		/**
		 * Iterate over all nodes based on passed in root node and cull them from the tree if 
		 * the elapsed time is below the threshold.
		 */
		private static void CullNodes(TreeView TreeView,float TimeThreshold)
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
				var Node = Nodes[NodeIndex];
				var Payload = (NodePayload) Node.Tag;
				if( Payload.InclusiveTime < TimeThreshold )
				{
					NodesToRemove.Add(Node);
				}
				else
				{
					foreach( TreeNode ChildNode in Node.Nodes )
					{
						Nodes.Add(ChildNode);
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
					return Math.Sign( PayloadB.InclusiveTime - PayloadA.InclusiveTime );
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
