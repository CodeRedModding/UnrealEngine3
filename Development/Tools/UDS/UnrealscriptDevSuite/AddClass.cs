using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using Extensibility;
using EnvDTE;
using EnvDTE80;
using EnvDTE90;
using System.IO;


namespace UnrealscriptDevSuite
{
	public partial class AddClass : Form
	{

		private class Storage
		{
			public ProjectItem Item;
			public string BasePath;
		}

		private DTE2 ApplicationObject;
		private ScriptObject ParentObject;
		private ClassBrowser Browser;
		private TreeNode LastSelected;

		private UDSSettings Settings;

		public AddClass()
		{
			InitializeComponent();
			Settings = new UDSSettings();
		}

		public AddClass(DTE2 NewAppObject, ClassBrowser NewBrowser, ScriptObject NewParentObject, List<ScriptObject> ClassList)
		{
			InitializeComponent();
			Settings = new UDSSettings();

			ApplicationObject = NewAppObject;
			ParentObject = NewParentObject;
			Browser = NewBrowser;
			BuildSolutionTree();

			// Find the proper package

			string PackageName = Path.GetFileNameWithoutExtension(ParentObject.ProjItem.ContainingProject.FullName);

			foreach (TreeNode TopNode in tvSolutionView.Nodes)
			{
				if (TopNode.Text.ToLower() == PackageName.ToLower())
				{
					TopNode.ExpandAll();
					TreeNode ClassesNode = TopNode.FirstNode;
					tvSolutionView.SelectedNode = ClassesNode;
					break;
				}
			}

			foreach (ScriptObject Obj in ClassList)
			{
				cbParentClass.Items.Add(Obj.ClassName);				
			}
			cbParentClass.SelectedIndex = cbParentClass.FindString(ParentObject.ClassName);
		}

		private void BuildSolutionTree()
		{
			Solution3 CurrentSolution = ApplicationObject.Solution as Solution3;
			foreach ( Project Proj in CurrentSolution.Projects )
			{
				foreach ( ProjectItem Item in Proj.ProjectItems )
				{
					CheckSolutionItem(Item);
				}
			}
		}

		private void CheckSolutionItem(ProjectItem Item)
		{
			if (Item.SubProject == null && (Item.ProjectItems == null || Item.ProjectItems.Count == 0))
			{
				return;
			}

			// Check to see if this is a classes node.  If it is, add everything under it.
			if (Item.Name.ToLower() == "classes")
			{
				string PackageName = Item.ContainingProject.FileName;
				PackageName = Path.GetFileNameWithoutExtension(PackageName);
				string BasePath = Path.GetDirectoryName(Item.ContainingProject.FileName) + "\\Classes";
				if (Directory.Exists(BasePath))
				{
					Storage S = new Storage();
					S.Item = Item;
					S.BasePath = BasePath;
					TreeNode PackageNode = tvSolutionView.Nodes.Add(PackageName);
					PackageNode.Tag = S;
					AddBranch(Item, PackageNode, BasePath);
				}
			}

			if (Item.SubProject != null && Item.SubProject.ProjectItems != null)
			{
				foreach (ProjectItem SubItem in Item.SubProject.ProjectItems)
				{
					CheckSolutionItem(SubItem);
				}
			}
			else if (Item.ProjectItems != null)
			{
				foreach (ProjectItem Child in Item.ProjectItems)
				{
					CheckSolutionItem(Child);
				}
			}
		}

		private void AddBranch( ProjectItem Item, TreeNode ParentNode, string BasePath )
		{
			if ( Item.SubProject == null && ( Item.ProjectItems == null || Item.ProjectItems.Count == 0 ) )
			{
				return;
			}

			TreeNode Node = ParentNode.Nodes.Add(Item.Name);
			Storage S = new Storage();
			S.Item = Item;
			S.BasePath = BasePath;
			Node.Tag = S;

			if ( Item.SubProject != null && Item.SubProject.ProjectItems != null )
			{
				foreach ( ProjectItem SubItem in Item.SubProject.ProjectItems )
				{
					AddBranch(SubItem, Node, BasePath);
				}
			}
			else if ( Item.ProjectItems != null )
			{
				foreach ( ProjectItem Child in Item.ProjectItems )
				{
					AddBranch(Child, Node, BasePath);
				}
			}
		}

		private void tvSolutionView_AfterSelect(object sender, TreeViewEventArgs e)
		{
			if (LastSelected != null)
			{
				LastSelected.BackColor = SystemColors.Window;
				LastSelected.ForeColor = SystemColors.WindowText;
			}

			tvSolutionView.SelectedNode.ForeColor = SystemColors.Highlight;
			LastSelected = tvSolutionView.SelectedNode;
		}

		private void btnOK_Click( object sender, EventArgs e )
		{

			if (tvSolutionView.SelectedNode == null)
			{
				MessageBox.Show("You need to select a destination for the new script file in the project.");
				return;
			}

			string Header = Settings.NewClassHeader;
			if (Header == null || Header == "")
			{
				Header = "//\r\n//\r\n";
			}


			string ClassBody = Header + "class " + tbNewClassName.Text + " extends " + cbParentClass.Text;
			if (tbAdditionalParams.Text != "")
			{
				for ( int i = 0; i < tbAdditionalParams.Lines.Count(); i++ )
				{
					if (tbAdditionalParams.Lines[i].Trim() != "")
					{
						ClassBody = ClassBody + "\r\n\t" + tbAdditionalParams.Lines[i];
					}
				}
			}
				
			if (ClassBody.Substring(ClassBody.Length-1,1) != ";")
			{
				ClassBody = ClassBody + ";\r\n";
			}
			else
			{
				ClassBody = ClassBody + "\r\n";
			}

			ClassBody = ClassBody + "\r\ndefaultproperties\r\n{\r\n}\r\n";
			string Filename = tbNewClassName.Text + ".uc";
		
			TreeNode Sel = tvSolutionView.SelectedNode;

			if (Sel.Tag == null)
			{
				MessageBox.Show("Critical Error:  Sel.tag should never be null");
				return;
			}

			Storage Store = Sel.Tag as Storage;
			if (Store == null)
			{
				MessageBox.Show("Critical Error: Sel.Tag is not Storage");
			}

			string Path = Store.BasePath;
			Filename = Path + "\\" + Filename;

			// Create the file..

			if (File.Exists(Filename))
			{
				MessageBox.Show("The class you are trying to create already exists in the selected package.");
				return;
			}

			using (StreamWriter OutputFile = new StreamWriter(Filename))
			{
				OutputFile.Write(ClassBody);
			}

			// Add the new file to the project.  NOTE: This will trigger it to be added to the hierarchy
			// as well.
						
			ProjectItem NewItem = Store.Item.ProjectItems.AddFromFile(Filename);
			if (NewItem != null)
			{
				UIHierarchy SolutionExplorer = ApplicationObject.ToolWindows.SolutionExplorer;
				foreach (UIHierarchyItem Item in SolutionExplorer.UIHierarchyItems)
				{
					if (RecurseHierarchy(Item, NewItem))
					{
						break;
					}
				}
			}
			NewItem.Open(EnvDTE.Constants.vsViewKindCode).Activate();
			DialogResult = DialogResult.OK;
			Close();
		}

		private bool RecurseHierarchy(UIHierarchyItem Item, ProjectItem SearchItem)
		{
			if (Item.Object as ProjectItem != null)
			{
				ProjectItem PItem = Item.Object as ProjectItem;
				if ( PItem == SearchItem )
				{
					Item.Select(vsUISelectionType.vsUISelectionTypeSelect);
					return true;
				}
			}

			foreach (UIHierarchyItem Child in Item.UIHierarchyItems)
			{
				bool b = RecurseHierarchy(Child, SearchItem);
				if ( b ) return true;
			}

			return false;
		}
	}
}
