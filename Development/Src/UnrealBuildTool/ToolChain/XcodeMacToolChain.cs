/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool
{
	class BuildMacFileItem
	{
		/** Descriptor for the item (path, name, whatever) */
		public string Name;

		/** Path for the item */
		public string Path;
		
		/** Guid for this item as a build item */
		public string BuildItemGuid;
		
		/** Guid for the file itself */
		public string FileGuid;

		/** Weak framework? */
		public bool bIsWeak;

		/** 
		 * Constructor
		 */
		public BuildMacFileItem(string InName)
		{
			Name = InName;
			Path = "";
			BuildItemGuid = XcodeMacToolChain.MakeGuid();
			FileGuid = XcodeMacToolChain.MakeGuid();
			bIsWeak = false;
		}

		/** 
		 * Constructor
		 */
		public BuildMacFileItem(string InName, string InPath)
		{
			Name = InName;
			Path = InPath;
			BuildItemGuid = XcodeMacToolChain.MakeGuid();
			FileGuid = XcodeMacToolChain.MakeGuid();
			bIsWeak = false;
		}

		/** 
		 * Constructor
		 */
		public BuildMacFileItem(string InName, bool bInIsWeak)
		{
			Name = InName;
			Path = "";
			BuildItemGuid = XcodeMacToolChain.MakeGuid();
			FileGuid = XcodeMacToolChain.MakeGuid();
			bIsWeak = bInIsWeak;
		}
	};

	class XcodeMacToolChain
	{
		/** Source files from CompileCPPFiles */
		static List<BuildMacFileItem> Sources = new List<BuildMacFileItem>();

		/** Preprocessor definitions from CPPEnvironment */
		static List<string> Defines = new List<string>();

		/** User include paths from CPPEnvironment */
		static List<string> IncludePaths = new List<string>();

		/** User include paths from FinalLinkEnvironment */
		static List<string> LibraryPaths = new List<string>();

		static public string GameName = "MacGame";

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
		{
			CPPOutput Result = new CPPOutput();

			if (SourceFiles.Count > 0)
			{
				// Cache unique include paths and defines
				foreach (string Path in CompileEnvironment.IncludePaths)
				{
					string Fixed = Path.Replace("\\", "/");
					if (!IncludePaths.Contains(Fixed))
					{
						IncludePaths.Add(Fixed);
					}
				}
				foreach (string Path in CompileEnvironment.SystemIncludePaths)
				{
					string Fixed = Path.Replace("\\", "/");
					if (!IncludePaths.Contains(Fixed))
					{
						IncludePaths.Add(Fixed);
					}
				}
				foreach (string Define in CompileEnvironment.Definitions)
				{
					if (!Defines.Contains(Define))
					{
						Defines.Add(Define);
					}
				}

				// Create a compile action for each source file.
				foreach (FileItem SourceFile in SourceFiles)
				{
					// make sure files get copied to Mac
					FileItem RemoteFile = MacToolChain.LocalToRemoteFileItem(SourceFile, true);
					Result.ObjectFiles.Add(RemoteFile);

					// remember this source file for compiling
					Sources.Add(new BuildMacFileItem(MacToolChain.LocalToMacPath(SourceFile.AbsolutePath, false)));

					// make sure that all the header files are copied over
					foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
					{
						MacToolChain.QueueFileForBatchUpload(IncludedFile);
					}
				}
			}
			return Result;
		}

		static Random Rand = new Random();
		/**
		 * Make a random Guid string usable by Xcode (24 characters exactly)
		 */
		public static string MakeGuid()
		{
			string Guid = "";

			byte[] Randoms = new byte[12];
			Rand.NextBytes(Randoms);
			for (int i = 0; i < 12; i++)
			{
				Guid += Randoms[i].ToString("X2");
			}

			return Guid;
		}


		public static FileItem LinkFiles( LinkEnvironment LinkEnvironment )
		{
			string Contents = "";

			// @todo: Help

			// cache framework info
			List<BuildMacFileItem> Frameworks = new List<BuildMacFileItem>();
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
				Frameworks.Add(new BuildMacFileItem(Framework, false));
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				Frameworks.Add(new BuildMacFileItem(Framework, true));
			}

			foreach (string Path in LinkEnvironment.LibraryPaths)
			{
				string Fixed = Path.Replace("\\", "/");
				if (!LibraryPaths.Contains(Fixed))
				{
					LibraryPaths.Add(Fixed);
				}
			}

			List<BuildMacFileItem> Libraries = new List<BuildMacFileItem>();
			foreach (string Library in LinkEnvironment.AdditionalLibraries)
			{
				string Path = "";
				foreach (string ShadowFile in LinkEnvironment.AdditionalShadowFiles)
				{
					if (ShadowFile.Contains(Library + "."))
					{
						Path = ShadowFile;
						break;
					}
				}
				Libraries.Add(new BuildMacFileItem(Library, Path));
			}

			List<BuildMacFileItem> LibrariesToCopy = new List<BuildMacFileItem>();
			foreach (BuildMacFileItem Library in Libraries)
			{
				if (Path.GetExtension(Library.Path) == ".dylib")
				{
					BuildMacFileItem LibraryToCopy = new BuildMacFileItem(Library.Name, Library.Path);
					LibraryToCopy.FileGuid = Library.FileGuid;
					LibrariesToCopy.Add(LibraryToCopy);
				}
			}

			// @todo: Help with resources
			List<BuildMacFileItem> Resources = new List<BuildMacFileItem>();
			BuildMacFileItem BuildItem = new BuildMacFileItem(string.Format("{0}-Info.plist", GameName));
			string PlistFileGuid = BuildItem.FileGuid;
			string PlistBuildGuid = BuildItem.BuildItemGuid;
			Resources.Add(BuildItem);
			BuildItem = new BuildMacFileItem(string.Format("{0}.icns", GameName));
			string IconsFileGuid = BuildItem.FileGuid;
			string IconsBuildGuid = BuildItem.BuildItemGuid;
			Resources.Add(BuildItem);
			BuildItem = new BuildMacFileItem("InfoPlist.strings");
			string PlistStringsFileGuid = BuildItem.FileGuid;
			string PlistStringsBuildGuid = BuildItem.BuildItemGuid;
			Resources.Add(BuildItem);
			BuildItem = new BuildMacFileItem("MainMenu.xib");
			string MainMenuFileGuid = BuildItem.FileGuid;
			string MainMenuBuildGuid = BuildItem.BuildItemGuid;
			Resources.Add(BuildItem);

			Contents += "// !$*UTF8*$!\r\n";
			Contents += "{\r\n";
			Contents += "\tarchiveVersion = 1;\r\n";
			Contents += "\tclasses = {\r\n";
			Contents += "\t};\r\n";
			Contents += "\tobjectVersion = 45;\r\n";
			Contents += "\tobjects = {\r\n";
			Contents += "\r\n";

			string AppGuid = MakeGuid();


			Contents += "/* Begin PBXBuildFile section */\r\n";
				foreach (BuildMacFileItem ResourceItem in Resources)
				{
					Contents += string.Format("\t\t{0} /* {1} in Resources */ = {{isa = PBXBuildFile; fileRef = {2} /* {1} */; }};\r\n",
						ResourceItem.BuildItemGuid, Path.GetFileName(ResourceItem.Name), ResourceItem.FileGuid);
				}

				foreach (BuildMacFileItem SourceItem in Sources)
				{
					Contents += string.Format("\t\t{0} /* {1} in Sources */ = {{isa = PBXBuildFile; fileRef = {2} /* {1} */; }};\r\n",
						SourceItem.BuildItemGuid, Path.GetFileName(SourceItem.Name), SourceItem.FileGuid);
				}

				foreach (BuildMacFileItem FrameworkItem in Frameworks)
				{
					Contents += string.Format("\t\t{0} /* {1}.framework in Frameworks */ = {{isa = PBXBuildFile; fileRef = {2} /* {1}.framework */; }};\r\n",
						FrameworkItem.BuildItemGuid, FrameworkItem.Name, FrameworkItem.FileGuid);
				}

				foreach (BuildMacFileItem LibraryItem in Libraries)
				{
					string Extension = (LibraryItem.Name == "z") ? ".dylib" : Path.GetExtension(LibraryItem.Path);
					string Name = "lib" + LibraryItem.Name + Extension;
					Contents += string.Format("\t\t{0} /* {1} in Frameworks */ = {{isa = PBXBuildFile; fileRef = {2} /* {1} */; }};\r\n",
						LibraryItem.BuildItemGuid, Name, LibraryItem.FileGuid);
				}

				foreach (BuildMacFileItem LibraryItem in LibrariesToCopy)
				{
					string Name = "lib" + LibraryItem.Name + ".dylib";
					Contents += string.Format("\t\t{0} /* {1} in CopyFiles */ = {{isa = PBXBuildFile; fileRef = {2} /* {1} */; }};\r\n",
						LibraryItem.BuildItemGuid, Name, LibraryItem.FileGuid);
				}
				
			Contents += "/* End PBXBuildFile section */\r\n";
			Contents += "\r\n";

			Contents += "/* Begin PBXFileReference section */\r\n";
				Contents += string.Format("\t\t{0} /* {1}.app */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; " +
					"includeInIndex = 0; path = {1}.app; sourceTree = BUILD_PRODUCTS_DIR; }};\r\n",
					AppGuid, GameName);

				Contents += string.Format("\t\t{0} /* {1}-Info.plist */ = {{isa = PBXFileReference; lastKnownFileType = text.plist.xml; " +
					"name = \"{1}-Info.plist\"; path = \"../../Development/Src/Mac/Resources/{1}-Info.plist\"; sourceTree = \"<group>\"; }};\r\n",
					PlistFileGuid, GameName);

				Contents += string.Format("\t\t{0} /* {1}.icns */ = {{isa = PBXFileReference; lastKnownFileType = image.icns; " +
					"name = \"{1}.icns\"; path = \"../../Development/Src/Mac/Resources/{1}.icns\"; sourceTree = \"<group>\"; }};\r\n",
					IconsFileGuid, GameName);

				Contents += string.Format("\t\t{0} /* InfoPlist.strings */ = {{isa = PBXFileReference; lastKnownFileType = text.plist.strings; " +
					"name = \"InfoPlist.strings\"; path = \"../../Development/Src/Mac/Resources/English.lproj/InfoPlist.strings\"; sourceTree = \"<group>\"; }};\r\n",
					PlistStringsFileGuid);

				Contents += string.Format("\t\t{0} /* MainMenu.xib */ = {{isa = PBXFileReference; lastKnownFileType = file.xib; " +
					"name = \"MainMenu.xib\"; path = \"../../Development/Src/Mac/Resources/English.lproj/MainMenu.xib\"; sourceTree = \"<group>\"; }};\r\n",
					MainMenuFileGuid);

				foreach (BuildMacFileItem SourceItem in Sources)
				{
					string Extension = Path.GetExtension(SourceItem.Name);
					// default to c++
					string FileType = "sourcecode.cpp.cpp";
					if (Extension == ".m")
					{
						FileType = "sourcecode.c.objc";
					}
					else if (Extension == ".mm")
					{
						FileType = "sourcecode.cpp.objcpp";
					}
					else if (Extension == ".c")
					{
						FileType = "sourcecode.c.c";
					}

					Contents += string.Format("\t\t{0} /* {1} */ = {{ isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = {3}; path = \"{2}\"; sourceTree = \"<group>\"; }};\r\n",
						SourceItem.FileGuid, Path.GetFileName(SourceItem.Name), SourceItem.Name, FileType);
				}

				foreach (BuildMacFileItem FrameworkItem in Frameworks)
				{
					Contents += string.Format("\t\t{0} /* {1}.framework */ = {{isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = {1}.framework; path = System/Library/Frameworks/{1}.framework; sourceTree = SDKROOT; }};\r\n",
						FrameworkItem.FileGuid, FrameworkItem.Name);
				}

				foreach (BuildMacFileItem LibraryItem in Libraries)
				{
					if (LibraryItem.Name == "z") // Special case for libz
					{
						Contents += string.Format("\t\t{0} /* libz.dylib */ = {{isa = PBXFileReference; lastKnownFileType = \"compiled.mach-o.dylib\"; name = libz.dylib; path = usr/lib/libz.dylib; sourceTree = SDKROOT; }};\r\n",
							LibraryItem.FileGuid);
					}
					else
					{
						string Extension = Path.GetExtension(LibraryItem.Path);
						string FileType = (Extension == ".dylib") ? "\"compiled.mach-o.dylib\"" : "archive.ar";
						Contents += string.Format("\t\t{0} /* lib{1}{2} */ = {{isa = PBXFileReference; lastKnownFileType = {3}; name = lib{1}{2}; path = ../../Development/Src/{4}; sourceTree = SOURCE_ROOT; }};\r\n",
							LibraryItem.FileGuid, LibraryItem.Name, Extension, FileType, LibraryItem.Path);
					}
				}

			Contents += "/* End PBXFileReference section */\r\n";
			Contents += "\r\n";


			Contents += "/* Begin PBXGroup section */\r\n";
		
			string ProductsGroupGuid = MakeGuid();
			Contents += string.Format("\t{0} /* Products */ = {{\r\n", ProductsGroupGuid);
				Contents += "\t\tisa = PBXGroup;\r\n";
				Contents += "\t\tchildren = (\r\n";
					Contents += string.Format("\t\t\t{0} /* {1}.app */,\r\n", AppGuid, GameName);
				Contents += "\t\t);\r\n";
				Contents += "\t\tname = Products;\r\n";
				Contents += "\t\tsourceTree = \"<group>\";\r\n";
			Contents += "\t};\r\n";

			string SourcesGroupGuid = MakeGuid();
			Contents += string.Format("\t{0} /* Sources */ = {{\r\n", SourcesGroupGuid);
				Contents += "\t\tisa = PBXGroup;\r\n";
				Contents += "\t\tchildren = (\r\n";
					foreach (BuildMacFileItem SourceItem in Sources)
					{
						Contents += string.Format("\t\t\t{0} /* {1} */,\r\n", SourceItem.FileGuid, Path.GetFileName(SourceItem.Name));
					}
				Contents += "\t\t);\r\n";
				Contents += "\t\tname = Sources;\r\n";
				Contents += "\t\tsourceTree = \"<group>\";\r\n";
			Contents += "\t};\r\n";


			string ResourcesGroupGuid = MakeGuid();
			Contents += string.Format("\t{0} /* Resources */ = {{\r\n", ResourcesGroupGuid);
				Contents += "\t\tisa = PBXGroup;\r\n";
				Contents += "\t\tchildren = (\r\n";
					foreach (BuildMacFileItem ResourceItem in Resources)
					{
						Contents += string.Format("\t\t\t{0} /* {1} */,\r\n", ResourceItem.FileGuid, ResourceItem.Name);
					}
				Contents += "\t\t);\r\n";
				Contents += "\t\tname = Resources;\r\n";
				Contents += "\t\tsourceTree = \"<group>\";\r\n";
			Contents += "\t};\r\n";


			string FrameworksGroupGuid = MakeGuid();
			Contents += string.Format("\t{0} /* Frameworks */ = {{\r\n", FrameworksGroupGuid);
				Contents += "\t\tisa = PBXGroup;\r\n";
				Contents += "\t\tchildren = (\r\n";
					foreach (BuildMacFileItem FrameworkItem in Frameworks)
					{
						Contents += string.Format("\t\t\t{0} /* {1} */,\r\n", FrameworkItem.FileGuid, FrameworkItem.Name);
					}
					foreach (BuildMacFileItem LibraryItem in Libraries)
					{
						string Extension = (LibraryItem.Name == "z") ? ".dylib" : Path.GetExtension(LibraryItem.Path);
						string Name = "lib" + LibraryItem.Name + Extension;
						Contents += string.Format("\t\t\t{0} /* {1} */,\r\n", LibraryItem.FileGuid, Name);
					}
				Contents += "\t\t);\r\n";
				Contents += "\t\tname = Frameworks;\r\n";
				Contents += "\t\tsourceTree = \"<group>\";\r\n";
			Contents += "\t};\r\n";

			string CustomTemplateGuid = MakeGuid();
			Contents += string.Format("\t{0} /* CustomTemplate */ = {{\r\n", CustomTemplateGuid);
				Contents += "\t\tisa = PBXGroup;\r\n";
				Contents += "\t\tchildren = (\r\n";
					Contents += string.Format("\t\t\t{0} /* Products */,\r\n", ProductsGroupGuid);
					Contents += string.Format("\t\t\t{0} /* Sources */,\r\n", SourcesGroupGuid);
					Contents += string.Format("\t\t\t{0} /* Resources */,\r\n", ResourcesGroupGuid);
					Contents += string.Format("\t\t\t{0} /* Frameworks */,\r\n", FrameworksGroupGuid);
				Contents += "\t\t);\r\n";
				Contents += "\t\tname = Frameworks;\r\n";
				Contents += "\t\tsourceTree = \"<group>\";\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXGroup section */\r\n";
			Contents += "\r\n";



			Contents += "/* Begin PBXNativeTarget section */\r\n";

			string NativeTargetGuid = MakeGuid();
			string NativeTargetConfigListGuid = MakeGuid();
			string ResourcesBuildPhaseGuid = MakeGuid();
			string SourcesBuildPhaseGuid = MakeGuid();
			string FrameworksBuildPhaseGuid = MakeGuid();
			string CopyFilesBuildPhaseGuid = MakeGuid();
			Contents += string.Format("\t{0} /* {1} */ = {{\r\n", NativeTargetGuid, GameName);
				Contents += "\t\tisa = PBXNativeTarget;\r\n";
				Contents += string.Format("\t\tbuildConfigurationList = {0} /* Build configuration list for PBXNativeTarget \"{1}\" */;\r\n", NativeTargetConfigListGuid, GameName);
				Contents += "\t\tbuildPhases = (\r\n";
					Contents += string.Format("\t\t\t{0} /* Resources */,\r\n", ResourcesBuildPhaseGuid);
					Contents += string.Format("\t\t\t{0} /* Sources */,\r\n", SourcesBuildPhaseGuid);
					Contents += string.Format("\t\t\t{0} /* Frameworks */,\r\n", FrameworksBuildPhaseGuid);
					Contents += string.Format("\t\t\t{0} /* CopyFiles */,\r\n", CopyFilesBuildPhaseGuid);
				Contents += "\t\t);\r\n";
				Contents += "\t\tbuildRules = (\r\n";
				Contents += "\t\t);\r\n";
				Contents += "\t\tdependencies = (\r\n";
				Contents += "\t\t);\r\n";
				Contents += string.Format("\t\tname = {0};\r\n", GameName);
				Contents += string.Format("\t\tproductName = {0};\r\n", GameName);
				Contents += string.Format("\t\tproductReference = {0} /* {1}.app */;\r\n", AppGuid, GameName);
				Contents += "\t\tproductType = \"com.apple.product-type.application\";\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXNativeTarget section */\r\n";
			Contents += "\r\n";


			Contents += "/* Begin PBXProject section */\r\n";

			string ProjectGuid = MakeGuid();
			string ProjectConfigListGuid = MakeGuid();
			Contents += string.Format("\t{0} /* Project object */ = {{\r\n", ProjectGuid);
				Contents += "\t\tisa = PBXProject;\r\n";
				Contents += string.Format("\t\tbuildConfigurationList = {0} /* Build configuration list for PBXProject \"{1}\" */;\r\n", ProjectConfigListGuid, GameName);
				Contents += "\t\tcompatibilityVersion = \"Xcode 3.2\";\r\n";
				Contents += "\t\tdevelopmentRegion = English;\r\n";
				// @todo: Maybe we should set this to 0?
				Contents += "\t\thasScannedForEncodings = 1;\r\n";
				Contents += "\t\tknownRegions = (\r\n";
					Contents += "\t\t\tEnglish,\r\n";
					Contents += "\t\t\tJapanese,\r\n";
					Contents += "\t\t\tFrench,\r\n";
					Contents += "\t\t\tGerman,\r\n";
					Contents += "\t\t\ten,\r\n";
				Contents += "\t\t);\r\n";
				Contents += string.Format("\t\tmainGroup = {0} /* CustomTemplate */;\r\n", CustomTemplateGuid);
				Contents += "\t\tprojectDirPath = \"\";\r\n";
				Contents += "\t\tprojectRoot = \"\";\r\n";
				Contents += "\t\ttargets = (\r\n";
					Contents += string.Format("\t\t\t{0} /* {1} */,\r\n", NativeTargetGuid, GameName);
				Contents += "\t\t);\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXProject section */\r\n";
			Contents += "\r\n";


		
			Contents += "/* Begin PBXFrameworksBuildPhase section */\r\n";
			Contents += string.Format("\t{0} /* Frameworks */ = {{\r\n", FrameworksBuildPhaseGuid);
				Contents += "\t\tisa = PBXFrameworksBuildPhase;\r\n";
				Contents += "\t\tbuildActionMask = 2147483647;\r\n";
				Contents += "\t\tfiles = (\r\n";
					foreach (BuildMacFileItem FrameworkItem in Frameworks)
					{
						Contents += string.Format("\t\t\t{0} /* {1}.framework in Frameworks */,\r\n",
							FrameworkItem.BuildItemGuid, FrameworkItem.Name);
					}
					foreach (BuildMacFileItem LibraryItem in Libraries)
					{
						string Extension = (LibraryItem.Name == "z") ? ".dylib" : Path.GetExtension(LibraryItem.Path);
						string Name = "lib" + LibraryItem.Name + Extension;
						Contents += string.Format("\t\t\t{0} /* {1} in Frameworks */,\r\n",
							LibraryItem.BuildItemGuid, Name);
					}
					Contents += "\t\t);\r\n";
				Contents += "\t\trunOnlyForDeploymentPostprocessing = 0;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXFrameworksBuildPhase section */\r\n";
			Contents += "\r\n";



			Contents += "/* Begin PBXResourcesBuildPhase section */\r\n";
			Contents += string.Format("\t{0} /* Resources */ = {{\r\n", ResourcesBuildPhaseGuid);
			Contents += "\t\tisa = PBXResourcesBuildPhase;\r\n";
				Contents += "\t\tbuildActionMask = 2147483647;\r\n";
				Contents += "\t\tfiles = (\r\n";
					foreach (BuildMacFileItem ResourceItem in Resources)
					{
						if (!ResourceItem.Name.Contains("-Info.plist"))
						{
							Contents += string.Format("\t\t\t{0} /* {1} in Resources */,\r\n",
								ResourceItem.BuildItemGuid, ResourceItem.Name);
						}
					}
				Contents += "\t\t);\r\n";
				Contents += "\t\trunOnlyForDeploymentPostprocessing = 0;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXResourcesBuildPhase section */\r\n";
			Contents += "\r\n";


			Contents += "/* Begin PBXSourcesBuildPhase section */\r\n";
			Contents += string.Format("\t{0} /* Sources */ = {{\r\n", SourcesBuildPhaseGuid);
			Contents += "\t\tisa = PBXSourcesBuildPhase;\r\n";
				Contents += "\t\tbuildActionMask = 2147483647;\r\n";
				Contents += "\t\tfiles = (\r\n";
					foreach (BuildMacFileItem SourceItem in Sources)
					{
						Contents += string.Format("\t\t\t{0} /* {1} in Sources */,\r\n",
							SourceItem.BuildItemGuid, Path.GetFileName(SourceItem.Name));
					}
				Contents += "\t\t);\r\n";
				Contents += "\t\trunOnlyForDeploymentPostprocessing = 0;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXSourcesBuildPhase section */\r\n";
			Contents += "\r\n";

			Contents += "/* Begin PBXCopyFilesBuildPhase section */\r\n";
			Contents += string.Format("\t{0} /* CopyFiles */ = {{\r\n", CopyFilesBuildPhaseGuid);
			Contents += "\tisa = PBXCopyFilesBuildPhase;\r\n";
			Contents += "\t\tbuildActionMask = 2147483647;\r\n";
				Contents += "\t\tdstPath = \"\";\r\n";
				Contents += "\t\tdstSubfolderSpec = 10;\r\n";
				Contents += "\t\tfiles = (\r\n";
					foreach (BuildMacFileItem LibraryItem in LibrariesToCopy)
					{
						Contents += string.Format("\t\t\t{0} /* lib{1}.dylib in CopyFiles */,\r\n",
							LibraryItem.BuildItemGuid, Path.GetFileName(LibraryItem.Name));
					}
				Contents += "\t\t);\r\n";
			Contents += "\t\trunOnlyForDeploymentPostprocessing = 0;\r\n";

			Contents += "\t};\r\n";
			Contents += "/* End PBXCopyFilesBuildPhase section */\r\n";
			Contents += "\r\n";

			Contents += "/* Begin XCBuildConfiguration section */\r\n";

			string DebugProjectConfigGuid = MakeGuid();
			string DebugTargetConfigGuid = MakeGuid();
			string ReleaseProjectConfigGuid = MakeGuid();
			string ReleaseTargetConfigGuid = MakeGuid();
			string TestProjectConfigGuid = MakeGuid();
			string TestTargetConfigGuid = MakeGuid();
			string ShippingProjectConfigGuid = MakeGuid();
			string ShippingTargetConfigGuid = MakeGuid();


			// empty project configs (put all into Target)
			Contents += string.Format("\t{0} /* Debug */ = {{\r\n", DebugProjectConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Debug;\r\n";
			Contents += "\t};\r\n";
			Contents += string.Format("\t{0} /* Release */ = {{\r\n", ReleaseProjectConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Release;\r\n";
			Contents += "\t};\r\n";
			Contents += string.Format("\t{0} /* Test */ = {{\r\n", TestProjectConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Test;\r\n";
			Contents += "\t};\r\n";
			Contents += string.Format("\t{0} /* Shipping */ = {{\r\n", ShippingProjectConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Shipping;\r\n";
			Contents += "\t};\r\n";


			Contents += string.Format("\t{0} /* Debug */ = {{\r\n", DebugTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_STANDARD_64_BIT)\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-Mac/Debug/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"../../Development/Src/Mac/Resources/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tMACOSX_DEPLOYMENT_TARGET = {0};\r\n", MacToolChain.MacOSVersion);
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += string.Format("\t\t\tSDKROOT = macosx{0};\r\n", MacToolChain.MacOSSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-Mac/Debug\";\r\n", GameName);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_VERSION=4.2;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
					Contents += "\t\t\tGCC_OPTIMIZATION_LEVEL = 0;\r\n";
					Contents += "\t\t\tGCC_C_LANGUAGE_STANDARD = c99;\r\n";
					Contents += "\t\t\tGCC_INLINES_ARE_PRIVATE_EXTERN = NO;\r\n";
					Contents += "\t\t\tGCC_SYMBOLS_PRIVATE_EXTERN = NO;\r\n";
					Contents += "\t\t\tOTHER_CFLAGS=\"-ffast-math\";\r\n";

					Contents += "\t\t\tHEADER_SEARCH_PATHS = (\r\n";
					foreach(string Dir in IncludePaths)
					{
						Contents += string.Format("\t\t\t\t\"../../Development/Src/{0}\",\r\n", Dir);
					}
					Contents += "\t\t\t);\r\n";

					Contents += "\t\t\tLIBRARY_SEARCH_PATHS = (\r\n";
					foreach (string Dir in LibraryPaths)
					{
						Contents += string.Format("\t\t\t\t\"../../Development/Src/{0}\",\r\n", Dir);
					}
					Contents += "\t\t\t);\r\n";

					Contents += "\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (\r\n";
					foreach(string Define in Defines)
					{
						Contents += string.Format("\t\t\t\t\"{0}\",\r\n", Define);
					}
					Contents += "\t\t\t);\r\n";

// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Debug;\r\n";
			Contents += "\t};\r\n";


			Contents += string.Format("\t{0} /* Release */ = {{\r\n", ReleaseTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_STANDARD_64_BIT)\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-Mac/Release/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"../../Development/Src/Mac/Resources/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tMACOSX_DEPLOYMENT_TARGET = {0};\r\n", MacToolChain.MacOSVersion);
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += string.Format("\t\t\tSDKROOT = macosx{0};\r\n", MacToolChain.MacOSSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-Mac/Release\";\r\n", GameName);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_VERSION=4.2;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
					Contents += "\t\t\tGCC_OPTIMIZATION_LEVEL = 3;\r\n";
					Contents += "\t\t\tGCC_C_LANGUAGE_STANDARD = c99;\r\n";
					Contents += "\t\t\tGCC_INLINES_ARE_PRIVATE_EXTERN = NO;\r\n";
					Contents += "\t\t\tGCC_SYMBOLS_PRIVATE_EXTERN = NO;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
					Contents += "\t\t\tOTHER_CFLAGS=\"-ffast-math\";\r\n";
					Contents += "\t\t};\r\n";
				Contents += "\t\tname = Release;\r\n";
			Contents += "\t};\r\n";

			Contents += string.Format("\t{0} /* Test */ = {{\r\n", TestTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_STANDARD_64_BIT)\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-Mac/Test/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"../../Development/Src/Mac/Resources/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tMACOSX_DEPLOYMENT_TARGET = {0};\r\n", MacToolChain.MacOSVersion);
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += string.Format("\t\t\tSDKROOT = macosx{0};\r\n", MacToolChain.MacOSSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-Mac/Test\";\r\n", GameName);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_VERSION=4.2;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
					Contents += "\t\t\tGCC_OPTIMIZATION_LEVEL = 3;\r\n";
					Contents += "\t\t\tGCC_C_LANGUAGE_STANDARD = c99;\r\n";
					Contents += "\t\t\tGCC_INLINES_ARE_PRIVATE_EXTERN = NO;\r\n";
					Contents += "\t\t\tGCC_SYMBOLS_PRIVATE_EXTERN = NO;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
					Contents += "\t\t\tOTHER_CFLAGS=\"-ffast-math\";\r\n";
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Test;\r\n";
			Contents += "\t};\r\n";

			Contents += string.Format("\t{0} /* Shipping */ = {{\r\n", ShippingTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_STANDARD_64_BIT)\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-Mac/Shipping/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"../../Development/Src/Mac/Resources/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tMACOSX_DEPLOYMENT_TARGET = {0};\r\n", MacToolChain.MacOSVersion);
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += string.Format("\t\t\tSDKROOT = macosx{0};\r\n", MacToolChain.MacOSSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-Mac/Shipping\";\r\n", GameName);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_VERSION=4.2;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
					Contents += "\t\t\tGCC_OPTIMIZATION_LEVEL = 3;\r\n";
					Contents += "\t\t\tGCC_C_LANGUAGE_STANDARD = c99;\r\n";
					Contents += "\t\t\tGCC_INLINES_ARE_PRIVATE_EXTERN = NO;\r\n";
					Contents += "\t\t\tGCC_SYMBOLS_PRIVATE_EXTERN = NO;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
					Contents += "\t\t\tOTHER_CFLAGS=\"-ffast-math\";\r\n";
					Contents += "\t\t};\r\n";
				Contents += "\t\tname = Shipping;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End XCBuildConfiguration section */\r\n";
			Contents += "\r\n";



			Contents += "/* Begin XCConfigurationList section */\r\n";

			Contents += string.Format("\t{0} /* Build configuration list for PBXNativeTarget \"{1}\" */ = {{\r\n", NativeTargetConfigListGuid, GameName);
			Contents += "\t\tisa = XCConfigurationList;\r\n";
				Contents += "\t\tbuildConfigurations = (\r\n";
					Contents += string.Format("\t\t\t{0} /* Debug */,\r\n", DebugTargetConfigGuid);
					Contents += string.Format("\t\t\t{0} /* Release */,\r\n", ReleaseTargetConfigGuid);
					Contents += string.Format("\t\t\t{0} /* Test */,\r\n", TestTargetConfigGuid);
					Contents += string.Format("\t\t\t{0} /* Shipping */,\r\n", ShippingTargetConfigGuid);
				Contents += "\t\t);\r\n";
				Contents += "\t\tdefaultConfigurationIsVisible = 0;\r\n";
				Contents += "\t\tdefaultConfigurationName = Release;\r\n";
			Contents += "\t};\r\n";

			Contents += string.Format("\t{0} /* Build configuration list for PBXProject \"{1}\" */ = {{\r\n", ProjectConfigListGuid, GameName);
			Contents += "\t\tisa = XCConfigurationList;\r\n";
				Contents += "\t\tbuildConfigurations = (\r\n";
					Contents += string.Format("\t\t\t{0} /* Debug */,\r\n", DebugProjectConfigGuid);
					Contents += string.Format("\t\t\t{0} /* Release */,\r\n", ReleaseProjectConfigGuid);
					Contents += string.Format("\t\t\t{0} /* Test */,\r\n", TestProjectConfigGuid);
					Contents += string.Format("\t\t\t{0} /* Shipping */,\r\n", ShippingProjectConfigGuid);
				Contents += "\t\t);\r\n";
				Contents += "\t\tdefaultConfigurationIsVisible = 0;\r\n";
				Contents += "\t\tdefaultConfigurationName = Release;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End XCConfigurationList section */\r\n";
			Contents += "\r\n";

			Contents += "\t};\r\n";
			Contents += string.Format("\trootObject = {0} /* Project object */;\r\n", ProjectGuid);
			Contents += "}";

			string ProjectPath = Path.Combine(Path.GetDirectoryName(LinkEnvironment.OutputFilePath), "UE3.xcodeproj/project.pbxproj");
			Directory.CreateDirectory(Path.GetDirectoryName(ProjectPath));
			File.WriteAllText(ProjectPath, Contents);
			
			return null;
		}
	};
}
