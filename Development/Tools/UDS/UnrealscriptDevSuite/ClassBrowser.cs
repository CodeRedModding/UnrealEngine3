using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.IO;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;
using Extensibility;
using EnvDTE;
using EnvDTE80;
using EnvDTE90;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell.Interop;

namespace UnrealscriptDevSuite
{
	public partial class ClassBrowser : UserControl
	{

		// Cached reference to the Application Object
		private DTE2 ApplicationObject;

		// Cached reference to the Add-in
		private AddIn AddInInstance;
		
		// A list of all script classes in the class hierarchy.  This is a sorted flat list.
		public List<ScriptObject> FlatScriptList;

		// A list of all interface classes in the class hierarchy.  This is a sorted flat list.
		public List<ScriptObject> FlatInterfaceList;

		// A list of all include .UCL files in the solution.  THis is a sorted flat list.
		public List<IncludeObject> FlatIncludeList;


		// The COM Objects associated with these events need to remain in scope in order for them
		// to work.

		private EnvDTE80.Events2 AllEvents;

		// Solution events (like Open/Close/New)
		private SolutionEvents SEvents;

		// Solution Item Events (like add/remove/rename)
		private ProjectItemsEvents PItemEvents;

		// This will be true if we are loading the hierarchy
		private bool bLoadingHierarchy;

		private bool bFullLoadPending;

		// If this string is true, then we will only show classes that have this in their name
		public string TreeFilter = "";

		// Reference to the Find in Lineage form
		public SearchDialog SearchForm;

		// Cached reference to this form's VS ToolWindow container
		public Window ParentToolWindow;

		// holds a list of root nodes that need to be restored when you remove the actors only filter
		private List<TreeNode> HiddenRootNodes;

		/**
		 * Constructor
		 */
		public ClassBrowser()
		{
			InitializeComponent();

			FlatScriptList = new List<ScriptObject>();
			FlatInterfaceList = new List<ScriptObject>();
			FlatIncludeList = new List<IncludeObject>();

			HiddenRootNodes = new List<TreeNode>();

			HierarchyToolStrip.ImageList = ClassImageList;
			RefreshButton.ImageIndex = 0;
			ActorsOnly.ImageIndex = 1;
			tbFindClass.ImageIndex = 2;
			QuickSearch.ImageIndex = 3;
			ExpandTree.ImageIndex = 4;
			CollapseTree.ImageIndex = 5;
			btnSearchLineage.ImageIndex = 6;

			SearchForm = new SearchDialog();
		}

		/**
		 * Find a class in the hierarchy. This function is called from outside the browser form
		 * to look up a given class in the hierarchy.  See Connect.cs
		 * 
		 * @Param ClassFilename - the filename to find. 
		 */
		public void FindClassInHierarchy(string ClassFilename)
		{
			ClassFilename = ClassFilename.ToUpper();
			foreach (ScriptObject Obj in FlatScriptList)
			{
				if (Obj.Filename.ToUpper() == ClassFilename)
				{
					tvClassTree.SelectedNode = Obj.ItemNode;
					tvClassTree.Focus();
				}
			}
		}

		/**
		 * Write text to the status bar
		 */
		public void StatusUpdate(string Text)
		{
			ApplicationObject.DTE.StatusBar.Text = Text;
		}

		/**
		 * Reset all of the lists
		 */
		public void ResetLists()
		{
			FlatScriptList.Clear();
			FlatInterfaceList.Clear();
			FlatIncludeList.Clear();
		}

		/**
		 * Reset all of the trees.  NOTE: All 3 tress will be in the "update" state after this.  Make
		 * sure you end the update by calling UpdateTrees(false) at some point.
		 */
		public void ResetTrees()
		{
			UpdateTrees(true);

			// Clear the trees

			tvClassTree.Nodes.Clear();
			tvInterfaceTree.Nodes.Clear();
			lbIncludes.Items.Clear();

			// Display Loading message

			tvClassTree.Nodes.Add("Loading...");
			tvInterfaceTree.Nodes.Add("Loading...");
			lbIncludes.Items.Add("Loading...");

			// Force an update

			UpdateTrees(false);
			Application.DoEvents();
			UpdateTrees(true);

			// Reclear all 3

			tvClassTree.Nodes.Clear();
			tvInterfaceTree.Nodes.Clear();
			lbIncludes.Items.Clear();

		}

		/**
		 * When we first get created in Connect.CreateClassBrowser we need to be
		 * initialized with various information re: the add-in.  Here we setup any of the
		 * callbacks, etc.
		 * 
		 * @Param AppObj - The application object of the add-in
		 * @Param Inst - The Add-in's instance
		 */
		public void ConnectToVS( DTE2 AppObj, AddIn Inst )
		{
			ApplicationObject = AppObj;
			AddInInstance = Inst;

			Enabled = false;
			Cursor = Cursors.WaitCursor;

			AllEvents = ApplicationObject.Events as Events2;
			if ( AllEvents != null )
			{
				SEvents = AllEvents.SolutionEvents;
				PItemEvents = AllEvents.ProjectItemsEvents;

				PItemEvents.ItemAdded += new _dispProjectItemsEvents_ItemAddedEventHandler(PItemEvents_ItemAdded);
				PItemEvents.ItemRemoved += new _dispProjectItemsEvents_ItemRemovedEventHandler(PItemEvents_ItemRemoved);
				PItemEvents.ItemRenamed += new _dispProjectItemsEvents_ItemRenamedEventHandler(PItemEvents_ItemRenamed);

				SEvents.BeforeClosing += new _dispSolutionEvents_BeforeClosingEventHandler(SolutionEvents_BeforeClosing);
				SEvents.Opened += new _dispSolutionEvents_OpenedEventHandler(SolutionEvents_Opened);
			}

			RequestFullRefreshHierarchy();

			using ( ServiceProvider sp = new ServiceProvider((Microsoft.VisualStudio.OLE.Interop.IServiceProvider)ApplicationObject) )
			{
				IVsFontAndColorStorage Storage = (IVsFontAndColorStorage)sp.GetService(typeof(SVsFontAndColorStorage));
				if ( Storage != null )
				{
					var Guid = new Guid("1F987C00-E7C4-4869-8A17-23FD602268B0"); // GUID for Environment Font
					if ( Storage.OpenCategory(ref Guid, (uint)( __FCSTORAGEFLAGS.FCSF_READONLY | __FCSTORAGEFLAGS.FCSF_LOADDEFAULTS )) == 0 )
					{
						LOGFONTW[] Fnt = new LOGFONTW[] { new LOGFONTW() };
						FontInfo[] Info = new FontInfo[] { new FontInfo() };
						try
						{
							Storage.GetFont(Fnt, Info);
							Font EnvironmentFont = new Font(Info[0].bstrFaceName, Info[0].wPointSize);
							if ( EnvironmentFont != null)
							{
								tvClassTree.Font = EnvironmentFont;
								tvInterfaceTree.Font = EnvironmentFont;
								lbIncludes.Font = EnvironmentFont;
							}
						}
						catch ( Exception e )
						{
							MessageBox.Show("Could not get the environment font.\r\nReason:" + e.ToString());
						}
					}
				}
			}
		}
		
		/**
		 * When a solution is loaded, make sure we perform a full refresh of the hierarchy
		 */
		private void SolutionEvents_Opened()
		{
			RequestFullRefreshHierarchy();
		}

		/**
		 * Before closing the solution, clear out the trees and lists
		 */
		private void SolutionEvents_BeforeClosing()
		{
			ResetLists();
			ResetTrees();
			UpdateTrees(false);
		}

		private void PItemEvents_ItemRenamed( ProjectItem Item, string OldName )
		{
			PItemEvents_ItemRemoved(Item);
			PItemEvents_ItemAdded(Item);
		}

		private void PItemEvents_ItemRemoved( ProjectItem Item )
		{
			ScriptObject ItemScriptObj;
			TreeView ItemTree;
			List<ScriptObject> ItemList;

			ItemScriptObj = FindScriptObjectByProjectItem(Item, FlatScriptList);
			if (ItemScriptObj == null)
			{
				// It wasn't a script class, look to see if it was an interface.
				ItemScriptObj = FindScriptObjectByProjectItem(Item, FlatInterfaceList);
				if (ItemScriptObj == null)
				{
					return; // NOTE: Needs to support removing UCI files as well.. later I'm tired.
				}
				else
				{
					ItemTree = tvInterfaceTree;
					ItemList = FlatInterfaceList;
				}
			}
			else
			{
				ItemTree = tvClassTree;
				ItemList = FlatScriptList;
			}

			if (ItemScriptObj != null && ItemScriptObj.ItemNode != null)
			{
				// We found the item.  Look to see if it has any children.

				if (ItemScriptObj.ItemNode.GetNodeCount(false) >0)
				{
					// Dang... need to move all of the children to the root since we go bye-bye
					foreach (TreeNode Node in ItemScriptObj.ItemNode.Nodes)
					{
						Node.Remove();
						ItemTree.Nodes.Add(Node);
					}
				}
				// Remove the tree node
				ItemScriptObj.ItemNode.Remove();

				// Resort the tree
				ItemTree.Sort();

				// Remove the script class
				RemoveScriptChild(ItemList, ItemScriptObj);
			}
		}

		void PItemEvents_ItemAdded( ProjectItem Item )
		{
			if ( Item.Kind == EnvDTE.Constants.vsProjectItemKindPhysicalFile)
			{
				string Filename = Item.get_FileNames(0);
				if ( Path.GetExtension(Filename).ToUpper() == ".UC" )
				{
					// We have a new Script class.  Parse it
					ApplicationObject.ToolWindows.OutputWindow.ActivePane.OutputString("[UDS] Adding " + Filename + "\r\n");
					ExamineScriptFile(Item, Filename);
				}
			}	
		}


		/**
		 * Iterate over a given object list and look for a script object that has the target ProjectItem
		 * 
		 * Item	- The Project Item to search for
		 * ObjList - The List to Search
		 */
		private ScriptObject FindScriptObjectByProjectItem(ProjectItem Item, List<ScriptObject> ObjList)
		{
			foreach ( ScriptObject Obj in ObjList )
			{
				if (Obj.ProjItem == Item)
				{
					return Obj;
				}
			}
			return null;
		}

		/**
		 * Help function to tell the 3 trees to being or end their update process
		 * 
		 * @param bBegin - Are we beginning or ending the update
		 */
		public void UpdateTrees( bool bBegin )
		{
			if ( bBegin )
			{
				tvClassTree.BeginUpdate();
				tvInterfaceTree.BeginUpdate();
				lbIncludes.BeginUpdate();
			}
			else
			{
				tvClassTree.EndUpdate();
				tvInterfaceTree.EndUpdate();
				lbIncludes.EndUpdate();
			}
		}

		private void RequestFullRefreshHierarchy()
		{
			bFullLoadPending = true;
			RefreshTimer.Interval = 100;
			RefreshTimer.Enabled = true;
		}

		private void RefreshTimer_Tick( object sender, EventArgs e )
		{
			RefreshTimer.Enabled = false;
			bFullLoadPending = false;
			FullRefreshHierarchy();
		}


		/**
		 * Perform a full refresh of the class hierarchy
		 */
		public void FullRefreshHierarchy()
		{
			if ( bFullLoadPending || bLoadingHierarchy )
			{
				return;
			}
			HierarchyToolStrip.Enabled = false;

			bLoadingHierarchy = true;
			RefreshTimer.Enabled = false;

			Enabled = false;
			Cursor = Cursors.WaitCursor;

			StatusUpdate("Parsing Classes");
			Application.DoEvents();

			ResetLists();
			ResetTrees();

			// Iterate over all of the solution objects and add their children
			Solution3 CurrentSolution = ApplicationObject.Solution as Solution3;
			foreach ( Project Proj in CurrentSolution.Projects )
			{
				if (Proj.ProjectItems != null)
				{
					foreach ( ProjectItem Item in Proj.ProjectItems )
					{
						AddProjectItem(Item);
					}
				}
			}

			// Sort everything
			tvClassTree.Sort();
			tvInterfaceTree.Sort();
			lbIncludes.Sorted = true;

			foreach (TreeNode T in tvClassTree.Nodes)
			{
				T.Expand();
			}

			foreach ( TreeNode T in tvInterfaceTree.Nodes )
			{
				T.Expand();
			}

			UpdateTrees(false);
			Enabled = true;
			Cursor = Cursors.Default;

			HierarchyToolStrip.Enabled = true;

			StatusUpdate("");
			Application.DoEvents();

			bLoadingHierarchy = false;
		}

		/**
		 * Add a ProjectItem to the hierarchy.  It will look at the filetype and determine what
		 * to do.  Then recurse over all children of this project item and add them as well
		 * 
		 * @param Item - The Project Item to Add
		 */
		private void AddProjectItem( ProjectItem Item)
		{
			if (Item.Kind == EnvDTE.Constants.vsProjectItemKindPhysicalFile)
			{
				try
				{
					string Filename = Item.get_FileNames(0);
					if (Path.GetExtension(Filename).ToUpper() == ".UC")
					{
						ExamineScriptFile(Item, Filename);
					}
					else if ( Path.GetExtension(Filename).ToUpper() == ".UCI" )
					{
						foreach ( object o in lbIncludes.Items )
						{
							IncludeObject obj = o as IncludeObject;
							if ( obj.Filename == Filename )
							{
								break;
							}
						}
						IncludeObject IncludeObj = new IncludeObject(Item.Name, Filename, Item);
						lbIncludes.Items.Add(IncludeObj);
					}
				}
				catch 
				{
				}
			}
			else if (Item.SubProject != null && Item.SubProject.ProjectItems != null)
			{

				foreach (ProjectItem SubItem in Item.SubProject.ProjectItems)
				{
					AddProjectItem(SubItem);
				}
			}
			else if (Item.ProjectItems != null)
			{
				foreach (ProjectItem Child in Item.ProjectItems)
				{
					AddProjectItem(Child);
				}
			}
		}

		private ScriptObject ExamineScriptFile(ProjectItem Item, string ClassFilename)
		{
			string ClassName = "";
			string ParentName = "";
			bool bIsInterface = false;

			if (ParseScriptFile(ClassFilename, out ClassName, out ParentName, out bIsInterface))
			{
				// Make sure the parsed class name is the same as the file's class name, otherwise use the file's.
				string FileBasedClassName = Path.GetFileNameWithoutExtension(ClassFilename);
				if (FileBasedClassName.ToLower() != ClassName.ToLower())
				{
					ApplicationObject.ToolWindows.OutputWindow.ActivePane.OutputString("Script Class " + ClassFilename + " has real class name of " + ClassName);
					ClassName = FileBasedClassName;
				}
				return AddAClass(bIsInterface, ClassName, ParentName , ClassFilename, Item);
			}
			return null;
		}

		/**
		 * Parse a file and determine if it's a valid script file and if it's a class or interface.
		 * 
		 * @Param Item - The ProjectItem we are currently working on
		 * @Param UCFilename - The Filename of the script file
		 * @Param NewClassName - Returns the class name of the script file
		 * @Param NewParentNAme - Returns the name of the parent for this class
		 * @Param bNewInterface - Will be true if this is an interface
		 * @returns true if successful
		 */
		private bool ParseScriptFile( string UCFilename, out string NewClassName, out string NewParentName, out bool bNewInterface )
		{
			NewClassName = "";
			NewParentName = "";
			bNewInterface = false;

			try
			{
				using ( FileStream InputFS = new FileStream(UCFilename, FileMode.Open, FileAccess.Read) )
				{
					try
					{
						using ( MemoryStream MemBuffer = new MemoryStream((int)InputFS.Length) )
						{
							InputFS.Read(MemBuffer.GetBuffer(), 0, (int)InputFS.Length);

							// Parse the classes 

							int CharPos = 0;
							byte[] TextBuffer = MemBuffer.GetBuffer();
							string Symbol = "";

							do
							{
								CharPos = GetNext(out Symbol, TextBuffer, CharPos);
								if ( Symbol.ToUpper() == "CLASS" || Symbol.ToUpper() == "INTERFACE" )
								{
									bNewInterface = Symbol.ToUpper() == "INTERFACE";
									
									CharPos = GetNext(out NewClassName, TextBuffer, CharPos);
									if ( CharPos < 0 )
									{
										MessageBox.Show("Error Parsing [" + UCFilename + "].  No Class name found");
										return false;
									}

									CharPos = GetNext(out Symbol, TextBuffer, CharPos);
									if (CharPos < 0 || Symbol.ToUpper() != "EXTENDS")
									{
										// Ok, we have found a class with no parent (like OBJECT).  
										return true;
									}

									CharPos = GetNext(out NewParentName, TextBuffer, CharPos);
									if ( CharPos < 0 )
									{
										MessageBox.Show("Error Parsing [" + UCFilename + "].  No parent found");
										return false;
									}

									return true;
								}
							}
							while ( CharPos >= 0 );
						}
					}
					catch ( Exception E )
					{
						MessageBox.Show("An unexpected error occurred: " + E.Message);
					}
				}

				return false;
			}
			catch ( FileNotFoundException )
			{
				//MessageBox.Show("UDS Attempted to load a file that seemed to no exist [" + UCFilename + "]");
			}
			catch ( Exception E )
			{
				MessageBox.Show("An unexpected file error occurred: " + E.Message);
			}

			return false;

		}

		/**
		 * Look at the current text buffer and return the next symbol.
		 * 
		 * @param SymbolText - [OUT] returns the symbol
		 * @param TextBuffer - The buffer containing the text we are parsing
		 * @param STartingIndex - Where in the buffer should we parse
		 * @returns the new caret position in the buffer
		 */
		private int GetNext( out string SymbolText, byte[] TextBuffer, int StartingIndex )
		{
			int Index = StartingIndex;
			SymbolText = "";
			while ( Index < TextBuffer.Length )
			{
				// Grab the next character in the Buffer
				char C = (char) TextBuffer[Index++];
				if ( !char.IsWhiteSpace(C) )
				{
					// Check for special characters.

					if ( C == '/' && Index < TextBuffer.Length && TextBuffer[Index] == '/' )
					{
						Index++;
						// In a single line comment, skip to the end.
						while ( Index < TextBuffer.Length && TextBuffer[Index] != '\n' )
						{
							Index++;
						}
					}
					else if ( C == '/' && Index < TextBuffer.Length && TextBuffer[Index] == '*' )
					{
						Index++;
						// Multiline comment.  We need to find the end of the comment block
						while ( Index < TextBuffer.Length - 1 && ( TextBuffer[Index] != '*' || TextBuffer[Index + 1] != '/' ) )
						{
							Index++;
						}
						Index+=2; // Skip the end 2 characters
					}
					// Ok.. we are in a valid symbol.. get and return it
					else
					{
						SymbolText += C;
						while ( Index < TextBuffer.Length && (char)TextBuffer[Index] != ';' && !char.IsWhiteSpace((char)TextBuffer[Index]) )
						{
							SymbolText += (char) TextBuffer[Index++];
						}
						return Index;
					}
				}
			}

			// Failed to find anything
			SymbolText = "";
			return -1;
		}

		/**
		 * Adds a class to the hierarchy.  This function will add it to the list, then add it to the tree
		 * 
		 * @Param bIsInterface - Is used to determine which list/tree to add things to
		 * @Param ClassName - The name of the class
		 * @Param ParentName - The name of this class' parent
		 * @Param Filename - The filename of the .UC file
		 * @Param Item - The ProjectItem this is associated with
		 */
		public ScriptObject AddAClass(bool bIsInterface, string ClassName, string ParentName, string Filename, ProjectItem Item)
		{
			if ( !bIsInterface && TreeFilter != "" && !ClassName.ToUpper().Contains(TreeFilter) )
			{
				return null;
			}

			ScriptObject Obj = new ScriptObject(ClassName, ParentName, bIsInterface, Filename, Item);
			AddScriptChild(bIsInterface ? FlatInterfaceList : FlatScriptList, Obj);
			AddToTree(bIsInterface ? tvInterfaceTree : tvClassTree, Obj);
			Application.DoEvents();
			return Obj;
		}

		/**
		 * Adds a ScriptObject to the flat list.  This list is sorted by class name
		 * 
		 * @Param ObjList - The flat list to add the child to
		 * @Param Child - The child to add
		 */
		private void AddScriptChild( List<ScriptObject> ObjList, ScriptObject Child )
		{
			// Look to see if this child is already in the list.  If it is, quickly exit
			if (FindScriptObj(ObjList,Child.ClassName) != null)
			{
				return;
			}

			// Just seek over the list.. 
			int InsertAt = 0;
			while (InsertAt < ObjList.Count && 
				string.Compare(ObjList[InsertAt].ClassName, Child.ClassName) < 0)
			{
				InsertAt++;
			}
			if ( InsertAt >= ObjList.Count )
			{
				ObjList.Add(Child);
			}
			else
			{
				ObjList.Insert(InsertAt, Child);
			}
		}

		/**
		 * Removes an object from the list
		 * 
		 * @param ObjList - The list to remove the object from
		 * @param Child - The object to remove
		 */
		private void RemoveScriptChild( List<ScriptObject> ObjList, ScriptObject Child)
		{
			int ObjIndex = FindScriptObjIndex(ObjList, Child.ClassName);
			if (ObjIndex>=0)
			{
				ObjList.RemoveAt(ObjIndex);
			}
		}

		/**
		 * Finds a script object in the list
		 * 
		 * @param ObjList - The list to search
		 * @param ClassName - The name of the class to find
		 * @returns the object or null if not found
		 */
		private ScriptObject FindScriptObj(List<ScriptObject> ObjList, string ClassName )
		{
			int ObjIndex = FindScriptObjIndex(ObjList,ClassName);
			return ObjIndex >= 0 ? ObjList[ObjIndex] : null;
		}

		/**
		 * Perform a binary search and find a given class in the list
		 * 
		 * @param ObjList - The list to search
		 * @param ClassName - The name of the class to find
		 * @returns the index of the object or -1 if not found
		 */
		private int FindScriptObjIndex( List<ScriptObject> ObjList, string ClassName )
		{
			int min = 0;
			int max = ObjList.Count-1;
			while (min <= max)
			{
				int mid = (min + max) / 2;
				int cmp = string.Compare(ClassName.ToUpper(), ObjList[mid].ClassName.ToUpper());
				if ( cmp == 0 )
				{
					return mid;
				}
				else if ( cmp < 0 )
				{
					max = mid - 1;
				}
				else
				{
					min = mid + 1;
				}
			}
			return -1;
		}

		/**
		 * Adds a script object to a treeview
		 * 
		 * @param Tree - The treeview to add to
		 * @param ScriptObj - The Script object to add
		 */
		private void AddToTree(TreeView Tree, ScriptObject ScriptObj)
		{
			// Find which list to use

			List<ScriptObject> ObjList = ScriptObj.bIsInterface ? FlatInterfaceList : FlatScriptList;

			// First, see if we can find the parent for this object.
			ScriptObject ParentObj = FindScriptObj(ObjList, ScriptObj.ParentName);
			ScriptObj.ParentProjItem = ( ParentObj == null ) ? null : ParentObj.ProjItem;

			// If we don't have a parent object, then place this in the top of the tree.

			TreeNode Node = (ParentObj == null) ? 
				Tree.Nodes.Add(ScriptObj.ClassName) :
				ParentObj.ItemNode.Nodes.Add(ScriptObj.ClassName);

			// Store a reference to the TreeNode
			ScriptObj.ItemNode = Node;
			Node.Tag = ScriptObj;

			// Look though the flat list and see if there are any children that have this object 
			// as their parent.

			foreach (ScriptObject Child in ObjList)
			{
				if (Child.ParentName.ToUpper() == ScriptObj.ClassName.ToUpper())
				{
					// Add this child to this object's list
					AddScriptChild(ScriptObj.ChildrenObjs, Child);
					
					// Tell the child about the parent

					Child.ParentObj = ScriptObj;

					// Remove this child from the tree and re-add it to this branch

					Child.ItemNode.Remove();
					Node.Nodes.Add(Child.ItemNode);
				}
			}

		}

		/**
		 * Perform a full refresh of the hierarchy
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void RefreshButton_Click( object sender, EventArgs e )
		{
			FullRefreshHierarchy();
		}

		/**
		 * Open up the selected item
		 * @param Sender - not used
		 * @param e - Event Args that holds the node that was clicked
		 */
		private void ClassTree_NodeMouseDoubleClick( object sender, TreeNodeMouseClickEventArgs e )
		{
			if ( e.Node != null && e.Node.Tag != null )
			{
				ScriptObject Obj = e.Node.Tag as ScriptObject;
				if ( Obj != null && Obj.ProjItem != null )
				{
					Obj.ProjItem.Open(EnvDTE.Constants.vsViewKindCode).Activate();
				}
			}
		}

		/**
		 * Display the full filename down in the status bar
		 * @param Sender - not used
		 * @param e - Event Args that holds the node we are hoving over
		 */
		private void ClassTree_NodeMouseHover( object sender, TreeNodeMouseHoverEventArgs e )
		{
			if ( e.Node != null && e.Node.Tag != null )
			{
				ScriptObject Obj = e.Node.Tag as ScriptObject;
				if ( Obj != null && Obj.ProjItem != null )
				{
					StatusUpdate(Obj.ProjItem.get_FileNames(0));
				}
			}
		}

		/**
		 * Open up the selected item
		 * @param Sender - not used
		 * @param e - Event Args that holds the node that was clicked
		 */
		private void tvInterfaceTree_NodeMouseDoubleClick( object sender, TreeNodeMouseClickEventArgs e )
		{
			if ( e.Node != null && e.Node.Tag != null )
			{
				ScriptObject Obj = e.Node.Tag as ScriptObject;
				if ( Obj.ProjItem != null )
				{
					Obj.ProjItem.Open(EnvDTE.Constants.vsViewKindCode).Activate();
				}
			}

		}

		/**
		 * Open up the selected item
		 * @param Sender - not used
		 * @param e - Event Args that holds the item that was clicked
		 */
		private void lbIncludes_MouseDoubleClick( object sender, MouseEventArgs e )
		{
			if ( lbIncludes.SelectedItem != null )
			{
				IncludeObject Obj = lbIncludes.SelectedItem as IncludeObject;
				if (Obj != null)
				{
					if ( Obj.ProjItem != null )
					{
						Obj.ProjItem.Open(EnvDTE.Constants.vsViewKindCode).Activate();
					}
				}
			}
		}

		/**
		 * Filter the node list to display ONLY the children of the selected class.
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void ActorsOnly_Click( object sender, EventArgs e )
		{
			if (!ActorsOnly.Checked)
			{
				// Get the currently selected node

				TreeNode Node = tvClassTree.SelectedNode;
				if (Node != null)
				{
					// Find all of the root nodes and remove them from the tree
					foreach (TreeNode RootNode in tvClassTree.Nodes)
					{
						HiddenRootNodes.Add(RootNode);
					}

					// Clear the list and set to just the selected node
					if (Node != null)
					{
						tvClassTree.Nodes.Clear();
						tvClassTree.Nodes.Add(Node);
						ActorsOnly.Checked = true;
					}
				}
			}
			else
			{
				// Clear the list and then restore all of the root nodes
				tvClassTree.Nodes.Clear();
				foreach ( TreeNode RootNode in HiddenRootNodes )
				{
					tvClassTree.Nodes.Add(RootNode);
				}

				tvClassTree.Sort();
				HiddenRootNodes.Clear();
				ActorsOnly.Checked = false;
			}
		}

		/**
		 * Filter the hierarchy.
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void QuickSearch_Click( object sender, EventArgs e )
		{
			if ( !QuickSearch.Checked )
			{
				TreeFilter = tbTextFilter.Text.ToUpper();
				tbTextFilter.Enabled = false;
				QuickSearch.ImageIndex = 7;
				FullRefreshHierarchy();
				QuickSearch.Checked = true;
			}
			else
			{
				TreeFilter = "";
				FullRefreshHierarchy();
				QuickSearch.Checked = false;
				tbTextFilter.Enabled = true;
				QuickSearch.ImageIndex = 3;
			}
		}

		/**
		 * Expand the full tree
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void ExpandTree_Click( object sender, EventArgs e )
		{
			tvClassTree.BeginUpdate();
			tvClassTree.ExpandAll();
			tvClassTree.EndUpdate();
		}

		/**
		 * Colapse the full tree
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void CollapseTree_Click( object sender, EventArgs e )
		{
			tvClassTree.BeginUpdate();
			tvClassTree.CollapseAll();
			tvClassTree.EndUpdate();
		}

		/**
		 * Find the currently open class in the hierarchy
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void tbFindClass_Click( object sender, EventArgs e )
		{
			// Find the class in the hierarchy
			EnvDTE.Document CurrentDocument = ApplicationObject.ActiveDocument;
			if ( Path.GetExtension(CurrentDocument.FullName).ToUpper() == ".UC" )
			{
				FindClassInHierarchy(CurrentDocument.FullName);
			}

		}

		/**
		 * When we are in the filter textbox, look for [ENTER] and perform quick search when it's pressed
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void tbTextFilter_KeyPress( object sender, KeyPressEventArgs e )
		{
			if (e.KeyChar == (char) 13)
			{
				QuickSearch_Click(null, null);
			}
		}

		/**
		 * Search the hierarchy up, down or both
		 */
		public void SearchLineage()
		{
			if (tvClassTree.SelectedNode != null)
			{
				SearchForm.Text = "Search Lineage of " + ( (ScriptObject)tvClassTree.SelectedNode.Tag ).ClassName;
				if (SearchForm.ShowDialog() == DialogResult.OK)
				{
					Find2 FindObj = ApplicationObject.Find as Find2;

					vsFindAction OldAction = FindObj.Action;
					bool OldBackwards = FindObj.Backwards;
					string OldFilesOfType = FindObj.FilesOfType;
					string OldFindWhat = FindObj.FindWhat;
					vsFindPatternSyntax OldPatternSyntax = FindObj.PatternSyntax;
					vsFindResultsLocation OldResultLocation = FindObj.ResultsLocation;
					vsFindTarget OldTarget = FindObj.Target;
					bool OldMatchWholeWord = FindObj.MatchWholeWord;
					bool OldMatchCase = FindObj.MatchCase;

					FindObj.Action = vsFindAction.vsFindActionFindAll;
					FindObj.Backwards = false;
					FindObj.FilesOfType = GenerateSearchStr(SearchForm.SearchDirection);
					FindObj.FindWhat = SearchForm.SearchFor;
					FindObj.PatternSyntax = vsFindPatternSyntax.vsFindPatternSyntaxLiteral;
					FindObj.ResultsLocation = SearchForm.bUseFind2 ? vsFindResultsLocation.vsFindResults2 : vsFindResultsLocation.vsFindResults1;
					FindObj.Target = vsFindTarget.vsFindTargetSolution;
					FindObj.MatchWholeWord = SearchForm.bWholeWords;
					FindObj.MatchCase = SearchForm.bMatchCase;
					FindObj.Execute();

					FindObj.Action = OldAction;
					FindObj.Backwards = OldBackwards;
					FindObj.FilesOfType = OldFilesOfType;
					FindObj.FindWhat = OldFindWhat;
					FindObj.PatternSyntax = OldPatternSyntax;
					FindObj.ResultsLocation = OldResultLocation;
					FindObj.Target = OldTarget;
					FindObj.MatchWholeWord = OldMatchWholeWord;
					FindObj.MatchCase = OldMatchCase;
				}
			}
		}

		/**
		 * SearchLinage works by creating a list of files to search and passing that list to the 
		 * find object.  Here we traverse the hierarchy in either or both directions and generate
		 * that list.
		 * 
		 * @param Direction - 0 = both, 1 = parents, 2 = children
		 */
		private string GenerateSearchStr(int Direction)
		{
			string Result;
			TreeNode CurrentNode = tvClassTree.SelectedNode;
			Result = ( (ScriptObject)CurrentNode.Tag ).ClassName + ".uc";
			if (Direction < 2)
			{
				// Find the parents
				TreeNode ParentNode = CurrentNode.Parent;
				while (ParentNode != null)
				{
					Result = Result + ";" + ( (ScriptObject)ParentNode.Tag ).ClassName + ".uc";
					ParentNode = ParentNode.Parent;
				}
			}

			if (Direction != 1)
			{
				TreeNode Child = CurrentNode.FirstNode;
				while ( Child != null )
				{
					Result = Result + RecurseChildren(Child);
					Child = Child.NextNode;
				}
			}

			return Result;
		}

		/**
		 * Find the next child and add it to the search string
		 */
		private string RecurseChildren(TreeNode CurrentNode)
		{
			string Result;
			Result = ";" + ( (ScriptObject)CurrentNode.Tag ).ClassName + ".uc";
			TreeNode Child = CurrentNode.FirstNode;
			while (Child != null)
			{
				Result = Result + RecurseChildren(Child);	
				Child = Child.NextNode;
			}
			return Result;
		}

		/**
		 * Someone wants to search the lineage
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void btnSearchLineage_Click( object sender, EventArgs e )
		{
			SearchLineage();
		}

		/**
		 * Someone wants to add a class.  Popup the dialgo and go
		 * 
		 * @param Sender - not used
		 * @param e - not used
		 */
		private void miAddClass_Click(object sender, EventArgs e)
		{
			if (tvClassTree.SelectedNode != null)
			{
				AddClass AC = new AddClass(ApplicationObject, this, tvClassTree.SelectedNode.Tag as ScriptObject, FlatScriptList);
				AC.ShowDialog();
				return;
			}
		}

		/**
		 * We override BeforeSelect on the various trees to perform better highlight
		 * 
		 * @param Sender - The Tree
		 * @param e - Holds the node
		 */
		private void tvBeforeSelect( object sender, TreeViewCancelEventArgs e )
		{
			ClassTree Tree = sender as ClassTree;
			Tree.SelectedNode.NodeFont = new Font(Tree.Font, FontStyle.Regular);
			e.Node.NodeFont = new Font(Tree.Font, FontStyle.Bold);
			e.Node.Text = e.Node.Text;
		}
	}
}
