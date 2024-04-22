// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace CodeSizeProfiler
{
    class FunctionInfo
    {
        public string FullName = "Unknown";
		public string LibraryName = "Unknown";
		public int Segment = 0;
        public int Offset = 0;
        public int Size = 0;
    }

    class LibraryInfo
	{
		public string LibraryName = "Unknown";
		public int TotalSize = 0;
		public int TotalCount = 0;
	}
    
    class MapFileParser
    {
		/**
		 * Node sorter that implements the IComparer interface. Nodes are sorted by size.
		 */ 
		public class NodeSizeSorter : System.Collections.IComparer
		{
			public int Compare(object ObjectA, object ObjectB)
			{
				// We sort by size, which requires payload.
				TreeNode NodeA = ObjectA as TreeNode;
				TreeNode NodeB = ObjectB as TreeNode;
				LibraryInfo LibraryInfoA = NodeA.Tag as LibraryInfo;
				LibraryInfo LibraryInfoB = NodeB.Tag as LibraryInfo;
				FunctionInfo FunctionInfoA = NodeA.Tag as FunctionInfo;
				FunctionInfo FunctionInfoB = NodeB.Tag as FunctionInfo;

				// Library node
				if( LibraryInfoA != null && LibraryInfoB != null )
				{
					// Sort by size, descending.
					return Math.Sign( LibraryInfoB.TotalSize - LibraryInfoA.TotalSize );
				}
				// Function node
				else if( FunctionInfoA != null && FunctionInfoB != null )
				{
					// Sort by size, descending.
					return Math.Sign( FunctionInfoB.Size - FunctionInfoA.Size );
				}
				// Treat missing payload as unsorted
				else
				{
					return 0;
				}
			}
		}
	
		public static void ParseFunctionListIntoTreeView( List<FunctionInfo> FunctionList, TreeView FunctionTreeView )
		{
			// Lock down treeview while we update to avoid needless redrawing.
			FunctionTreeView.BeginUpdate();
			FunctionTreeView.Nodes.Clear();

			// Iterate over all functions and add root nodes for librarys and function nodes for children.
			var LibraryToNode = new Dictionary<string,TreeNode>();
			foreach( FunctionInfo Function in FunctionList )
			{
				TreeNode LibraryNode = null;

				// Found library in node map.
				if( LibraryToNode.ContainsKey( Function.LibraryName ) )
				{
					LibraryNode = LibraryToNode[ Function.LibraryName ];
					// Track total size and function count per library.
					LibraryInfo LibraryInfo = (LibraryInfo) LibraryNode.Tag;
					LibraryInfo.TotalCount++;
					LibraryInfo.TotalSize += Function.Size;
				}
				// Encountered new library.
				else
				{
					// Set up library info used as node payload.
					LibraryInfo LibraryInfo = new LibraryInfo();
					LibraryInfo.LibraryName = Function.LibraryName;
					LibraryInfo.TotalCount = 1;
					LibraryInfo.TotalSize = Function.Size;

					// Hook up to tree and dictionary.
					LibraryNode = new TreeNode();
					LibraryNode.Tag = LibraryInfo;
					LibraryToNode.Add( Function.LibraryName, LibraryNode );
					FunctionTreeView.Nodes.Add(LibraryNode);
				}

				// Add function node to library.
				string FunctionString = String.Format( "{0,8} {1}", Function.Size, Function.FullName );
				TreeNode FunctionNode = new TreeNode(FunctionString);
				FunctionNode.Tag = Function;
				LibraryNode.Nodes.Add(FunctionNode);
			}

			// Iterate over all library nodes now that all functions are parsed and totals are set.
			foreach( TreeNode LibraryNode in FunctionTreeView.Nodes )
			{
				LibraryInfo LibraryInfo = (LibraryInfo) LibraryNode.Tag;
				LibraryNode.Text = String.Format( "{0,15} {1,8} {2}", LibraryInfo.TotalSize, LibraryInfo.TotalCount, LibraryInfo.LibraryName );
			}

			FunctionTreeView.TreeViewNodeSorter = new NodeSizeSorter();
			FunctionTreeView.EndUpdate();
		}

        public static List<FunctionInfo> ParseFileIntoFunctionList(string Filename, bool bNeedsUndecoration)
        {
			// Return list of functions parsed from passed in map file.
			var FunctionList = new List<FunctionInfo>();
			
			StreamReader MapStreamReader = null;

			// Undecorate all symbols in the passed in file if needed.
            if( bNeedsUndecoration )
			{
				// x64 version of undname is in subfolder of bin. Using 32 bit should work for 64 bit map files though.
				string Undecorator = Path.Combine( Environment.GetEnvironmentVariable("VS90COMNTOOLS"), "../../VC/bin/undname.exe" );			
				System.Diagnostics.ProcessStartInfo ProcessStartInfo = new System.Diagnostics.ProcessStartInfo( Undecorator, Filename ); 
				ProcessStartInfo.UseShellExecute = false;
				ProcessStartInfo.CreateNoWindow = true;
				ProcessStartInfo.RedirectStandardOutput = true;
				
				// Run and wait for process to exit.
				var UndecorateProcess = System.Diagnostics.Process.Start( ProcessStartInfo );
				UndecorateProcess.WaitForExit();
				MapStreamReader = UndecorateProcess.StandardOutput;
			}
			else
			{
				MapStreamReader = new StreamReader(Filename);
			}

			// Iterate over all lines and parse the functions.		
			bool bShouldStartParsingFunctions = false;
			int BlankLineCount = 0;
			while( MapStreamReader.Peek() >= 0 )
			{
				string Line = MapStreamReader.ReadLine();

				// Skip summary and start parsing after we encounter the header.
				if( !bShouldStartParsingFunctions && Line.Contains("Address         Publics by Value              Rva+Base       Lib:Object") )
				{
					bShouldStartParsingFunctions = true;
				}
				else if( bShouldStartParsingFunctions )
				{
					// Break out of loop if we encounter 2nd blank line. The first is right after the header.
					if( Line == "" )
					{
						// We're done parsing.
						if( ++BlankLineCount == 2 )
						{
							break;
						}
						// Skip processing blank lines.
						else
						{
							continue;
						}
					}			

					// Parse function into newly created object.
					var Function = new FunctionInfo();

					// Address is " 0000:00000000" and is Segment:Offset
					string Segment = Line.Substring(1,4);
					Function.Segment = int.Parse(Segment, System.Globalization.NumberStyles.HexNumber);
					string Offset = Line.Substring(6,8);
					Function.Offset = int.Parse(Offset, System.Globalization.NumberStyles.HexNumber);

					// Calculate size of previous entry relative to current. This does not work for the last entry in a segment.
					if( FunctionList.Count > 0 )
					{
						var LastFunction = FunctionList.Last();
						if( LastFunction.Segment == Function.Segment )
						{
							LastFunction.Size = Function.Offset - LastFunction.Offset;
						}
					}

					// Library name is last token, assuming no spaces in library name!
					var Substrings = Line.Split(' ');
					Function.LibraryName = Substrings[Substrings.Length-1];

					// Format is " 0001:00003240       "UndecoratedName" 00404240 f i "LibraryName
					Function.FullName = Line.Substring(21, Line.Length - (Function.LibraryName.Length + 14 + 21)).Trim();

					// Last but not least, add parsed function.
					FunctionList.Add( Function );
				}
			}

            return FunctionList;
        }
    }
}
