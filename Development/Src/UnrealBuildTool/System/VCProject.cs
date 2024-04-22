/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.XPath;

namespace UnrealBuildTool
{
	/** A Visual C++ project. */
	class VCProject
	{
		/** Reads the list of files in a project from the specified project file. */
		public static List<string> GetProjectFiles(string ProjectPath)
		{
			using (FileStream ProjectStream = new FileStream(ProjectPath, FileMode.Open, FileAccess.Read))
			{
				// Parse the project's root node.
				XPathDocument Doc = new XPathDocument(ProjectStream);
				XPathNavigator Nav = Doc.CreateNavigator();

// 				XPathNavigator Version = Nav.SelectSingleNode("/VisualStudioProject/@Version");
// 				if (Version != null && (Version.Value =="10.00" || Version.Value == "10,00"))
// 				{
// 					XPathNodeIterator Iter = Nav.Select("/VisualStudioProject/Files//File/@RelativePath");
// 					List<string> RelativeFilePaths = new List<string>(Iter.Count);
// 					foreach (XPathNavigator It in Iter)
// 					{
// 						RelativeFilePaths.Add(It.Value);
// 					}
// 					return RelativeFilePaths;
// 				}

				var MyNameTable = new NameTable();
				var NSManager = new XmlNamespaceManager( MyNameTable );
				NSManager.AddNamespace( "ns", "http://schemas.microsoft.com/developer/msbuild/2003" );

				XPathNavigator MSBuildProjVersion = Nav.SelectSingleNode( "/ns:Project/@ToolsVersion", NSManager );
				if( MSBuildProjVersion != null && ( MSBuildProjVersion.Value == "4.0" || MSBuildProjVersion.Value == "4,0" ) )
				{
					XPathNodeIterator Iter = Nav.Select( "/ns:Project/ns:ItemGroup/ns:ClCompile/@Include", NSManager );
					List<string> RelativeFilePaths = new List<string>( Iter.Count );
					foreach (XPathNavigator It in Iter)
					{
						RelativeFilePaths.Add( It.Value );
					}

					XPathNodeIterator MMIter = Nav.Select( "/ns:Project/ns:ItemGroup/ns:None/@Include", NSManager );
					foreach (XPathNavigator It in MMIter)
					{
						if ((It.Value.EndsWith(".m") == true) ||
							(It.Value.EndsWith(".mm") == true))
						{
							RelativeFilePaths.Add(It.Value);
						}
					}
                    
                    XPathNodeIterator RCIter = Nav.Select("/ns:Project/ns:ItemGroup/ns:ResourceCompile/@Include", NSManager);
                    foreach (XPathNavigator It in RCIter)
                    {
                        RelativeFilePaths.Add(It.Value);
                    }

                    return RelativeFilePaths;
				}
			}
			return new List<string>();
		}
	}

	/** A Visual C# project. */
	class VCSharpProject
	{
		/** Reads the list of dependencies from the specified project file. */
		public static List<string> GetProjectFiles(string ProjectPath)
		{
			List<string> RelativeFilePaths = new List<string>();
			XmlDocument Doc = new XmlDocument();
			Doc.Load(ProjectPath);
				
			var Tags = new string[]{ "Compile", "Page", "Resource" };
			foreach( var Tag in Tags )
			{
				var Elements = Doc.GetElementsByTagName( Tag );	
				foreach( XmlElement Element in Elements )
				{		
					RelativeFilePaths.Add( Element.GetAttribute("Include") );
				}
			}

			return RelativeFilePaths;
		}
	}
}
