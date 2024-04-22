using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Extensibility;
using EnvDTE;
using EnvDTE80;
using EnvDTE90;

namespace UnrealscriptDevSuite
{
	public class InterfaceObject
	{
		// Name of the interface
		public string InterfaceName;

		// Name of the parent
		public string ParentName;

		public string Filename;

		public ProjectItem ProjItem;
		public InterfaceObject ParentObj;
		public List<InterfaceObject> ChildrenObjs;

		public InterfaceObject( string NewInterfaceName, string NewParentName, string NewFilename, ProjectItem NewItem )
		{
			InterfaceName = NewInterfaceName;
			ParentName = NewParentName;
			Filename = NewFilename;
			ProjItem = NewItem;
			ParentObj = null;
			ChildrenObjs = new List<InterfaceObject>();
		}
	}
}
