/*=============================================================================
	CallGraphParser.cs: .uprof parser classes
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
		
	Remember your first C program? Now imagine one that grew from hello world
	to doing something more complex and you'll understand the below.
=============================================================================*/

using System;
using System.IO;
using System.Collections;
using System.Windows.Forms;

namespace ueScriptProfiler
{
	/**
	 * Class used to parse raw 32 bit tokens serialized by the game.	
	 */
	public class RawToken
	{
		/** 32 bit wide unsigned integer used to to hold token value */
		private UInt32 Value;

		/**
		 * Constructor.
		 * 
		 * @param InValue	Raw token value.	
		 */
		public RawToken( UInt32 InValue )
		{
			Value = InValue;
		}

		/**
		 * Returns whether we reached the end of the input stream.
		 * 
		 * @return TRUE if this token signals the end of the stream, FALSE otherwise
		 */
		public bool IsEndMarker()
		{
			// Function pointers can't be NULL and other non function pointer tokens have 
			// the lowest bit set so there can't be any potential collisions with other 
			// tokens.
			return Value == 0;
		}

		/**
		 * Returns whether a frame transition occured.
		 *
		 * @return TRUE if this token signals the end of the frame, FALSE otherwise	
		 */
		public bool IsFrameEndMarker()
		{
			// Function pointers are always 4 byte aligned so we can use the lower 2 bits for
			// status information. In this case both bits set means that it's the end of a
			// frame.
			return (Value & 3) == 3;
		}

		/**
		 * Returns whether this token is used to store a function index/ pointer.
		 * 
		 * @return TRUE if this token holds a function index (pointer value), FALSE otherwise
		 */
		public bool IsFunction()
		{
			// Function pointers are always 4 byte aligned and never NULL so a non 0 value
			// that has the lowest two bits 0 has to be a function pointer.
			return (Value != 0) && ((Value & 3) == 0);
		}

		/**
		 * Returns whether this token is used to store cycle count informatoin.
		 * 
		 * @return TRUE if this token contains cycle count information, FALSE otherwise
		 */
		public bool IsCycleCount()
		{
			// Function pointers are always 4 byte aligned so we can use the lower 2 bits for
			// status information. In this case only the lowest of the lower two bits set means 
			// that the other bits contain cycle information. Instead of reducing the range of
			// the cycle counter by shifting we much rather live with the noise introduced
			// by abusing the lower two bits.
			return (Value & 3) == 1;
		}

		/**
		 * Returns the raw value of the token as passed to the constructor.
		 * 
		 * @return Raw value of the token.
		 */
		public UInt32 GetValue()
		{
			return Value;
		}
	}

	/**
	 * Compare class used to sort tree nodes by text in descending order.
	 */
	public class NodeTextDescendingComparer : IComparer 
	{
		/**
		 * Compare function used to compare two objects to each other.
		 * 
		 * @param	A	First TreeNode
		 * @param	B	Second TreeNode
		 * @return		0 if TreeNodes are equal, otherwise +/- 1 depending on order
		 */
		public int Compare( object A, object B ) 
		{
			TreeNode Node1 = (TreeNode) A;
			TreeNode Node2 = (TreeNode) B;
			return -String.Compare( Node1.Text, Node2.Text );
		}
	}

	/**
	 * Class to hold payload associated with tree node.
	 */
	public class NodePayload
	{
		/**
		 * Constructor, initializing member variables.
		 * 
		 * @param	InNode				Node this payload is for
		 * @param	InFunctionPointer	Function pointer associated with node
		 */
		public NodePayload( TreeNode InNode, UInt32 InFunctionPointer )
		{
			FunctionPointer = InFunctionPointer;
			Cycles			= 0.0;
			Calls			= 0;
		}

		/** Function pointer for this node		*/
		public UInt32	FunctionPointer;
		/** Inclusive Cycles spent on this node */
		public double	Cycles;
		/** Calls made to this node				*/
		public int		Calls;
	}

	/**
	 * Class hodling data associated with a UFunction.	
	 */
	public class FunctionInfo
	{
		/**
		 * Constructor, initializing all member variables.
		 * 
		 * @param InFunctionName	Name associated with this function.	
		 */
		public FunctionInfo( string InFunctionName )
		{
			Name			= InFunctionName;
			InclCycles		= 0;
			ExclCycles		= 0;
			Calls			= 0;
		}

		/** Function name									*/
		public string	Name;
		/** Inclusive Cycles in this function				*/
		public double	InclCycles;
		/** Exclusive Cycles in this function				*/
		public double	ExclCycles;
		/** Number of times this function has been called	*/
		public int		Calls;
	}

	/**
	 * Parser class, turning token stream into tree and list views.
	 */
	public class CallGraphParser
	{
		/** Mapping from function pointer to function info structure								*/
		private Hashtable	PointerToFunctionInfo;
		/** Frame counter																			*/
		private int			FrameCount				= 0;
		/** Total count of cycles. Sum of inclusive cycles of toplevel functions					*/
		private double		TotalCycleCount			= 0;
		/** Non valid (lower 2 bits != 0) pointer used for lookup of "self" into function info map. */
		private UInt32		SelfPointer				= 0xFFFFFFFF;
		/** Micro seconds per cycle																	*/
		private double		usecsPerCycle			= 1.0;
		
		/**
		 * Recursively traverses the tree bottom down, filling in the node text and adding self nodes where necessary
		 * 
		 * @param	Parent	Parent tree node.
		 */
		private void RecurseNodes( TreeNode Parent )
		{
			// Total amount of cycles spent in child nodes.
			double		ChildCycles		= 0;
			// List of nodes used for sorting as NodeCollection cannot be sorted :(
			ArrayList	ChildNodeList	= new ArrayList();

			// Traverse the tree recursively, set the text on nodes and collect total time spent in child nodes.
			foreach( TreeNode ChildNode in Parent.Nodes )
			{
				NodePayload Payload	= (NodePayload) ChildNode.Tag;
				if( Payload != null )
				{
					ChildCycles += Payload.Cycles;
				}

				RecurseNodes( ChildNode );

				FunctionInfo	Function	= (FunctionInfo) PointerToFunctionInfo[ Payload.FunctionPointer ];
				String			Percentage	= String.Format( "{0:0.00}", 100 * Payload.Cycles / TotalCycleCount ).PadLeft( 5, ' ' );
				ChildNode.Text				= Percentage + "%  " + Function.Name;

				ChildNodeList.Add( ChildNode );
			}

			// Update incl./ excl. cycles and add a self node if wanted.
			NodePayload ParentPayload = (NodePayload) Parent.Tag;
			// Top node doesn't have payload.
			if( ParentPayload != null ) 
			{	
				double ExclCycles		= ParentPayload.Cycles - ChildCycles;
				FunctionInfo Function	= (FunctionInfo) PointerToFunctionInfo[ ParentPayload.FunctionPointer ];
				Function.ExclCycles		+= ExclCycles;
				Function.InclCycles		+= ParentPayload.Cycles;

				// Add a "self" node to the parent for exclusive time unless we're a leaf.
				if( Parent.Nodes.Count > 0 )
				{
					TreeNode		SelfTimeNode	= new TreeNode("");
					NodePayload		Payload			= new NodePayload( SelfTimeNode, SelfPointer );
					SelfTimeNode.Tag				= Payload;
					Payload.Cycles					= ExclCycles;
					FunctionInfo	SelfFunction	= (FunctionInfo) PointerToFunctionInfo[ Payload.FunctionPointer ];
					String			Percentage		= String.Format( "{0:0.00}", 100 * Payload.Cycles / TotalCycleCount ).PadLeft( 5, ' ' );
					SelfTimeNode.Text				= Percentage + "%  " + SelfFunction.Name;
					ChildNodeList.Add( SelfTimeNode );
				}
			}

			// Sort nodes in descending order.
			NodeTextDescendingComparer Comparer = new NodeTextDescendingComparer();
			ChildNodeList.Sort( Comparer );

			// Add nodes in sorted order. Being able to sort a NodeCollection directly would be nice.
			Parent.Nodes.Clear();
			foreach( TreeNode Node in ChildNodeList )
			{
				Parent.Nodes.Add( Node );
			}
		}

		/**
		 * Recursively traverses tree updating the tree node's text with inclusive usec per number of toplevel calls.
		 *
		 * @param	Parent			Node to traverse.
		 * @param	TopLevelCalls	Number of calls to top level node.
		 */
		private void RecurseNodesExpensiveFunctions( TreeNode Parent, int TopLevelCalls )
		{
			foreach( TreeNode ChildNode in Parent.Nodes )
			{
				// Update text with inclusive usec per number of toplevel function calls.
				NodePayload		Payload			= (NodePayload) ChildNode.Tag;
				FunctionInfo	Function		= (FunctionInfo) PointerToFunctionInfo[ Payload.FunctionPointer ];
				String			usecPerCall		= String.Format( "{0:0.00}",  usecsPerCycle * Payload.Cycles / TopLevelCalls ).PadLeft( 9, ' ' );
				ChildNode.Text					= usecPerCall + " usec  " + Function.Name;
				
				// And recurse.
				RecurseNodesExpensiveFunctions( ChildNode, TopLevelCalls );
			}
		}

		/**
		 * Parses token stream and updates the passed in treeviews and listview.
		 * 
		 * File format:
		 * 
		 * DOUBLE				usecsPerCycle
		 * DWORD				NumFunctions
		 *		DWORD			Function pointer
		 *		DWORD			Length of string
		 *		ANSICHAR[Lengt]	Function name
		 * 
		 * DWORD[...]			Tokens
		 * 
		 * @param	Filename			Filename to open
		 * @param	CallGraph			TreeView used to parse call graph information sorted by %inclusive time into
		 * @param	ExpensiveFunctions	TreeView used to parse call graph information sorted by absolute inclusive time per parent call into
		 * @param	SortedView			ListView used to parse various stats into
		 * @param	LoadingProgress		Progress bar that is being updated as we parse
		 */
		public void ParseStream( String Filename, TreeView CallGraph, TreeView ExpensiveFunctions, ListView SortedView, ProgressBar LoadingProgress )
		{
			// Empty views as we might be opening a stream for the second time.
			CallGraph.Nodes.Clear();
			ExpensiveFunctions.Nodes.Clear();
			SortedView.Items.Clear();

			// Create binary reader and file info object from filename.
			BinaryReader	BinaryStream	= new BinaryReader( File.OpenRead(Filename) );
			FileInfo		Info			= new FileInfo( Filename );
			long			BytesRead		= 0;

			// Begin parsing the stream.
			usecsPerCycle					= BinaryStream.ReadDouble();
			UInt32			NumFunctions	= BinaryStream.ReadUInt32();	
			BytesRead						+= 4;
			PointerToFunctionInfo			= new Hashtable();

			// Populate the function pointer to name map with data serialized from stream.
			for( uint FunctionIndex=0; FunctionIndex<NumFunctions; FunctionIndex++ )
			{
				UInt32		Pointer			= BinaryStream.ReadUInt32();
				UInt32		Length			= BinaryStream.ReadUInt32();
				String		FunctionName	= new String( BinaryStream.ReadChars( (int) Length ) );
				PointerToFunctionInfo.Add( Pointer, new FunctionInfo(FunctionName) );	
				
				BytesRead					+= 4 + 4 + (long) Length;
				LoadingProgress.Value		= (int) (80 * BytesRead / Info.Length);
			}

			// Create "self" pointer function info required for name lookup later.
			PointerToFunctionInfo.Add( SelfPointer, new FunctionInfo("self") );

			// Create dummy root node which is going to be removed later.
			TreeNode		CurrentNode		= new TreeNode("Call Graph");
			TreeNode		TopNode			= CurrentNode;						
			bool			Finished		= false;
			Stack			CallStack		= new Stack();
			CallGraph.Nodes.Add( CurrentNode );

			// Parse token stream.
			while( !Finished )
			{
				try
				{
					// Read token from stream and update progress bar.
					RawToken Token			= new RawToken( BinaryStream.ReadUInt32() );
					BytesRead				+= 4;
					LoadingProgress.Value	= (int)(80 * BytesRead / Info.Length);

					if( Token.IsFunction() )
					{
						// Function call token.

						bool			Found			= false;
						UInt32			FunctionPointer = Token.GetValue();
						NodePayload		Payload			= (NodePayload) CurrentNode.Tag;
	
						FunctionInfo	Function		= (FunctionInfo) PointerToFunctionInfo[ FunctionPointer ];
						Function.Calls++;

						if( Payload != null && Payload.FunctionPointer == FunctionPointer )
						{
							// Simple recursion.
							// @warning: the code neither handles mutual recursion nor non trivial loops correctly.
							Payload.Calls++;
						}
						else
						{
							// See whether we already created a node for this function...
							foreach( TreeNode ChildNode in CurrentNode.Nodes )
							{	
								NodePayload ChildPayload = (NodePayload) ChildNode.Tag;
								if( ChildPayload.FunctionPointer == FunctionPointer )
								{
									Found		= true;
									CurrentNode = ChildNode;
									break;
								}
							}
							
							// ... and set up one if not.
							if( !Found )
							{
								TreeNode ChildNode = new TreeNode( "" );
								CurrentNode.Nodes.Add( ChildNode );
								CurrentNode = ChildNode;
								CurrentNode.Tag = new NodePayload( CurrentNode, FunctionPointer );
							}

							// CurrentNode now points to "this" function call.

							Payload	= (NodePayload) CurrentNode.Tag;
							Payload.Calls++;
						}

						// Keep track of call stack so we can detect and correctly handle simple recursion.
						CallStack.Push( Function );
					}
					else if( Token.IsCycleCount() )
					{
						// Cycle count token.

						FunctionInfo Function = (FunctionInfo) CallStack.Pop();
						if( (CallStack.Count > 0) && (CallStack.Peek() == Function) )
						{
							// Simple recursion.
							// @warning: the code neither handles mutual recursion nor non trivial loops correctly.
						}
						else
						{
							// Update inclusive time and "return" by walking up the tree.
							NodePayload Payload = (NodePayload) CurrentNode.Tag;
							CurrentNode			= CurrentNode.Parent;
							Payload.Cycles		+= Token.GetValue();
							if( CurrentNode.Parent == null )
							{
								// A "null" parent indicates we returned form a top level so we update the total
								// cycle count.
								TotalCycleCount += Token.GetValue();
							}
						}
					}
					else if( Token.IsFrameEndMarker() )
					{
						// End of frame marker.

						if( CallStack.Count != 0 )
						{
							//@todo: better error handling.
							Console.WriteLine("Stack underflow.");
							Finished = true;
						}
						FrameCount++;
					}
					else if( Token.IsEndMarker() )
					{
						// End of stream marker.
					
						Console.WriteLine("Stack depth == {0}", CallStack.Count );
						Finished = true;
					}
					else
					{
						//@todo: better error handling.
						Console.WriteLine("Unrecognized token.");
						Finished = true;
					}
				}
				catch( System.IO.EndOfStreamException )
				{
					//@todo: better error handling.
					Console.WriteLine("Unexpected end of stream.");
					Finished = true;
				}
			}

			LoadingProgress.Value = 80;
			// Recurse all nodes, update information, add "self" where necessary and sort nodes based on percentage. Yes, this function does a lot.
			RecurseNodes( TopNode );
			LoadingProgress.Value = 90;

			// Clone *entire* tree for "expensive function graph.
			TreeNode OtherTopNode = (TreeNode) TopNode.Clone();
			
			// Remove dummy top node from default call graph tree.
			CallGraph.Nodes.Clear();
			foreach( TreeNode Node in TopNode.Nodes )
			{
				CallGraph.Nodes.Add( Node );
			}

			// Keep track of top level functions in a sortable array.
			ArrayList ExpensiveFunctionsList = new ArrayList();
			foreach( TreeNode Node in OtherTopNode.Nodes )
			{
				ExpensiveFunctionsList.Add( Node );
			}

			// Display inclusive time in usec per call for top level functions...
			foreach( TreeNode Node in ExpensiveFunctionsList )
			{
				NodePayload		Payload			= (NodePayload) Node.Tag;
				FunctionInfo	Function		= (FunctionInfo) PointerToFunctionInfo[ Payload.FunctionPointer ];
				String			usecPerCall		= String.Format( "{0:0.00}",  usecsPerCycle * Payload.Cycles / Payload.Calls ).PadLeft( 9, ' ' );
				Node.Text						= usecPerCall + " usec  " + Function.Name;

				// ... and traverse the tree to update the displayed text with inclusive time in usec per top level function call.
				RecurseNodesExpensiveFunctions( Node, Payload.Calls );
			}

			// Sort expensive function tree view top nodes.
			NodeTextDescendingComparer Comparer = new NodeTextDescendingComparer();
			ExpensiveFunctionsList.Sort( Comparer );

			// Add nodes in sorted order. Being able to sort a NodeCollection directly would be nice.
			ExpensiveFunctions.Nodes.Clear();
			foreach( TreeNode Node in ExpensiveFunctionsList )
			{
				ExpensiveFunctions.Nodes.Add( Node );
			}

			// Populate list view with stats and sort it.
			foreach( FunctionInfo Function in PointerToFunctionInfo.Values )
			{
				// No need to pollute list with uncalled functions (comparatively large fraction).
				if( Function.Calls > 0 )
				{
					String InclPercentage	= String.Format( "{0:0.00}", 100 * Function.InclCycles / TotalCycleCount ).PadLeft( 5, ' ' );
					String ExclPercentage	= String.Format( "{0:0.00}", 100 * Function.ExclCycles / TotalCycleCount ).PadLeft( 5, ' ' );
					String CallsPerFrame	= String.Format( "{0:0.0000}", (float) Function.Calls / FrameCount ).PadLeft( 7, ' ' );
					String InclPerCall		= String.Format( "{0:0.00}", usecsPerCycle * Function.InclCycles / Function.Calls ).PadLeft( 9, ' ' );
					String ExclPerCall		= String.Format( "{0:0.00}", usecsPerCycle * Function.ExclCycles / Function.Calls ).PadLeft( 9, ' ' );
					string[] Row			= new string[] { Function.Name, InclPercentage, ExclPercentage, CallsPerFrame, InclPerCall, ExclPerCall };
					SortedView.Items.Add( new ListViewItem(Row) );
				}
			}
			SortedView.Sort();

			LoadingProgress.Value = 100;
		}
	}
}
