using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Extensibility;
using EnvDTE;
using EnvDTE80;
using EnvDTE90;
using System.Windows.Forms;

namespace UnrealscriptDevSuite
{
	public class ScriptObject : IComparable
	{
		// Name of the class
		public string ClassName;

		// Name of the parent
		public string ParentName;

		// The filename
		public string Filename;

		// True if this object is an interface
		public bool bIsInterface;

		// Holds a reference to the script object that is the parent of this object
		public ScriptObject ParentObj;

		// Holds a list of children objects
		public List<ScriptObject> ChildrenObjs;

		// Holds access to the Project Item associated with this item
		public ProjectItem ProjItem;

		// Holds access to the Project Item that contains this class's project itme
		public ProjectItem ParentProjItem;

		// This will be filled out once this object is added to a tree
		public TreeNode ItemNode;

		/**
		 * Add a new Script Object
		 */
		public ScriptObject( string NewClassName, string NewParentName, bool bNewIsInterface, string NewFilename, ProjectItem NewProjItem)
		{
			ClassName = NewClassName;
			ParentName = NewParentName;
			bIsInterface = bNewIsInterface;
			Filename = NewFilename;
			ParentObj = null;
			ChildrenObjs = new List<ScriptObject>();
			ProjItem = NewProjItem;
		}

		int IComparable.CompareTo( object obj )
		{
			ScriptObject ScriptObj = (ScriptObject)obj;
			return string.Compare(this.ClassName, ScriptObj.ClassName);
		}
	}
}
