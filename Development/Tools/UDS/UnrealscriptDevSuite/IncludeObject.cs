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
	public class IncludeObject
	{
		// Name of the include item
		public string IncludeName;

		public string Filename;

		public ProjectItem ProjItem;

		public IncludeObject( string NewIncludeName, string NewFileName, ProjectItem NewItem )
		{
			IncludeName = NewIncludeName;
			Filename = NewFileName;
			ProjItem = NewItem;
		}

		public override string ToString()
		{
			return IncludeName;
		}
	}
}
