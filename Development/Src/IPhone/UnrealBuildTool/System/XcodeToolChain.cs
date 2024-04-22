/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;

namespace UnrealBuildTool
{
	class BuildFileItem
	{
		/** Descriptor for the item (path, name, whatever) */
		public string Name;
		
		/** Guid for this item as a build item */
		public string BuildItemGuid;
		
		/** Guid for the file itself */
		public string FileGuid;

		/** Weak framework? */
		public bool bIsWeak;

		/** 
		 * Constructor
		 */
		public BuildFileItem(string InName)
		{
			Name = InName;
			BuildItemGuid = XcodeToolChain.MakeGuid();
			FileGuid = XcodeToolChain.MakeGuid();
			bIsWeak = false;
		}

		/** 
		 * Constructor
		 */
		public BuildFileItem(string InName, bool bInIsWeak)
		{
			Name = InName;
			BuildItemGuid = XcodeToolChain.MakeGuid();
			FileGuid = XcodeToolChain.MakeGuid();
			bIsWeak = bInIsWeak;
		}
	};

	class XcodeToolChain
	{
		/** Source files from CompileCPPFiles */
		static List<BuildFileItem> Sources = new List<BuildFileItem>();

		/** Preprocessor definitions from CPPEnvironment */
		static List<string> Defines = new List<string>();

		/** User include paths from CPPEnvironment */
		static List<string> IncludePaths = new List<string>();

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
		{
			CPPOutput Result = new CPPOutput();

			if (SourceFiles.Count > 0)
			{
				// Cache unique include paths and defines
				foreach (string IncPath in CompileEnvironment.IncludePaths)
				{
					string Fixed = IncPath.Replace("\\", "/");
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
					FileItem RemoteFile = IPhoneToolChain.LocalToRemoteFileItem(SourceFile, true);
					Result.ObjectFiles.Add(RemoteFile);

					// remember this source file for compiling
					Sources.Add(new BuildFileItem(IPhoneToolChain.LocalToMacPath(SourceFile.AbsolutePath, false)));

					// make sure that all the header files are copied over
					foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
					{
						IPhoneToolChain.LocalToRemoteFileItem(IncludedFile, true);
					}
				}

// 				Action DummyAction = new Action();
// 				DummyAction.WorkingDirectory = Path.GetFullPath(".");
// 				DummyAction.CommandPath = "whoami.exe";
// 				DummyAction.StatusDescription = "Dummy action 1";
// 				DummyAction.CommandArguments = "";
// 				DummyAction.ProducedItems.Add(Result.ObjectFiles[0]);
// 				DummyAction.bShouldBlockStandardOutput = true;

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
			string GameName = "UDKGame";

			// cache framework info
			List<BuildFileItem> Frameworks = new List<BuildFileItem>();
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
				Frameworks.Add(new BuildFileItem(Framework, false));
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				Frameworks.Add(new BuildFileItem(Framework, true));
			}

			// @todo: Help with resources
			List<BuildFileItem> Resources = new List<BuildFileItem>();

			Contents += "// !$*UTF8*$!\r\n";
			Contents += "{\r\n";
			Contents += "\tarchiveVersion = 1;\r\n";
			Contents += "\tclasses = {\r\n";
			Contents += "\t};\r\n";
			Contents += "\tobjectVersion = 45;\r\n";
			Contents += "\tobjects = {\r\n";
			Contents += "\r\n";

			string AppGuid = MakeGuid();
			string PlistGuid = MakeGuid();


			Contents += "/* Begin PBXBuildFile section */\r\n";
				foreach (BuildFileItem SourceItem in Sources)
				{
					Contents += string.Format("\t\t{0} /* {1} in Sources */ = {{isa = PBXBuildFile; fileRef = {2} /* {1} */; }};\r\n",
						SourceItem.BuildItemGuid, Path.GetFileName(SourceItem.Name), SourceItem.FileGuid);
				}

				foreach (BuildFileItem FrameworkItem in Frameworks)
				{
					Contents += string.Format("\t\t{0} /* {1}.framework in Frameworks */ = {{isa = PBXBuildFile; fileRef = {2} /* {1}.framework */; }};\r\n",
						FrameworkItem.BuildItemGuid, FrameworkItem.Name, FrameworkItem.FileGuid);
				}

			Contents += "/* End PBXBuildFile section */\r\n";
			Contents += "\r\n";

			Contents += "/* Begin PBXFileReference section */\r\n";
				Contents += string.Format("\t\t{0} /* {1}.app */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; " +
					"includeInIndex = 0; path = {1}.app; sourceTree = BUILD_PRODUCTS_DIR; }};\r\n",
					AppGuid, GameName);

				Contents += string.Format("\t\t{0} /* {1}-Info.plist */ = {{isa = PBXFileReference; lastKnownFileType = text.plist.xml; " +
					"name = \"{1}-Info.plist\"; path = \"UE3-Xcode/{1}-Info.plist\"; sourceTree = \"<group>\"; }};\r\n",
					PlistGuid, GameName);

				foreach (BuildFileItem SourceItem in Sources)
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

				foreach (BuildFileItem FrameworkItem in Frameworks)
				{
					Contents += string.Format("\t\t{0} /* {1}.framework */ = {{isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = {1}.framework; path = System/Library/Frameworks/{1}.framework; sourceTree = SDKROOT; }};\r\n",
						FrameworkItem.FileGuid, FrameworkItem.Name);
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
					foreach (BuildFileItem SourceItem in Sources)
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
					foreach (BuildFileItem ResourceItem in Resources)
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
					foreach (BuildFileItem FrameworkItem in Frameworks)
					{
						Contents += string.Format("\t\t\t{0} /* {1} */,\r\n", FrameworkItem.FileGuid, FrameworkItem.Name);
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
			Contents += string.Format("\t{0} /* {1} */ = {{\r\n", NativeTargetGuid, GameName);
				Contents += "\t\tisa = PBXNativeTarget;\r\n";
				Contents += string.Format("\t\tbuildConfigurationList = {0} /* Build configuration list for PBXNativeTarget \"{1}\" */;\r\n", NativeTargetConfigListGuid, GameName);
				Contents += "\t\tbuildPhases = (\r\n";
					Contents += string.Format("\t\t\t{0} /* Resources */,\r\n", ResourcesBuildPhaseGuid);
					Contents += string.Format("\t\t\t{0} /* Sources */,\r\n", SourcesBuildPhaseGuid);
					Contents += string.Format("\t\t\t{0} /* Frameworks */,\r\n", FrameworksBuildPhaseGuid);
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
					foreach (BuildFileItem FrameworkItem in Frameworks)
					{
						Contents += string.Format("\t\t\t{0} /* {1}.framework in Frameworks */,\r\n",
							FrameworkItem.BuildItemGuid, FrameworkItem.Name);
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
					foreach (BuildFileItem ResourceItem in Resources)
					{
						Contents += string.Format("\t\t\t{0} /* {1} in Resources */,\r\n",
							ResourceItem.BuildItemGuid, ResourceItem.Name);
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
					foreach (BuildFileItem SourceItem in Sources)
					{
						Contents += string.Format("\t\t\t{0} /* {1} in Sources */,\r\n",
							SourceItem.BuildItemGuid, Path.GetFileName(SourceItem.Name));
					}
				Contents += "\t\t);\r\n";
				Contents += "\t\trunOnlyForDeploymentPostprocessing = 0;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End PBXSourcesBuildPhase section */\r\n";
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
			string DistributionProjectConfigGuid = MakeGuid();
			string DistributionTargetConfigGuid = MakeGuid();


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
			Contents += string.Format("\t{0} /* Distribution */ = {{\r\n", DistributionProjectConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Distribution;\r\n";
			Contents += "\t};\r\n";


			Contents += string.Format("\t{0} /* Debug */ = {{\r\n", DebugTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_UNIVERSAL_IPHONE_OS)\";\r\n";
					Contents += "\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"iPhone Developer\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-IPhone/Debug/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"UE3-Xcode/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tIPHONEOS_DEPLOYMENT_TARGET = {0};\r\n", IPhoneToolChain.IPhoneOSVersion);
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += "\t\t\t\"PROVISIONING_PROFILE[sdk=iphoneos*]\" = \"\";\r\n";
					Contents += string.Format("\t\t\tSDKROOT = iphoneos{0};\r\n", IPhoneToolChain.IPhoneSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-IPhone/Debug\";\r\n", GameName);
					Contents += string.Format("\t\t\tTARGETED_DEVICE_FAMILY = \"{0}\";\r\n", IPhoneToolChain.DeviceFamilies);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tVALID_ARCHS = armv7;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
					Contents += "\t\t\tGCC_OPTIMIZATION_LEVEL = 0;\r\n";
					Contents += "\t\t\tGCC_THUMB_SUPPORT = NO;\r\n";
					Contents += "\t\t\tGCC_C_LANGUAGE_STANDARD = c99;\r\n";


					Contents += "\t\t\tHEADER_SEARCH_PATHS = (\r\n";
					foreach(string Dir in IncludePaths)
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
					Contents += "\t\t\tARCHS = \"$(ARCHS_UNIVERSAL_IPHONE_OS)\";\r\n";
					Contents += "\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"iPhone Developer\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-IPhone/Release/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"UE3-Xcode/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tIPHONEOS_DEPLOYMENT_TARGET = {0};\r\n", IPhoneToolChain.IPhoneOSVersion);
					Contents += "\t\t\tIPHONEOS_DEPLOYMENT_TARGET = 3.1.3;\r\n";
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += "\t\t\t\"PROVISIONING_PROFILE[sdk=iphoneos*]\" = \"\";\r\n";
					Contents += string.Format("\t\t\tSDKROOT = iphoneos{0};\r\n", IPhoneToolChain.IPhoneSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-IPhone/Release\";\r\n", GameName);
					Contents += string.Format("\t\t\tTARGETED_DEVICE_FAMILY = \"{0}\";\r\n", IPhoneToolChain.DeviceFamilies);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tVALID_ARCHS = armv7;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Release;\r\n";
			Contents += "\t};\r\n";

			Contents += string.Format("\t{0} /* Test */ = {{\r\n", TestTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_UNIVERSAL_IPHONE_OS)\";\r\n";
					Contents += "\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"iPhone Developer\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-IPhone/Test/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"UE3-Xcode/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tIPHONEOS_DEPLOYMENT_TARGET = {0};\r\n", IPhoneToolChain.IPhoneOSVersion);
					Contents += "\t\t\tIPHONEOS_DEPLOYMENT_TARGET = 3.1.3;\r\n";
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += "\t\t\t\"PROVISIONING_PROFILE[sdk=iphoneos*]\" = \"\";\r\n";
					Contents += string.Format("\t\t\tSDKROOT = iphoneos{0};\r\n", IPhoneToolChain.IPhoneSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-IPhone/Test\";\r\n", GameName);
					Contents += string.Format("\t\t\tTARGETED_DEVICE_FAMILY = \"{0}\";\r\n", IPhoneToolChain.DeviceFamilies);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tVALID_ARCHS = armv7;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Test;\r\n";
			Contents += "\t};\r\n";

			Contents += string.Format("\t{0} /* Shipping */ = {{\r\n", ShippingTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_UNIVERSAL_IPHONE_OS)\";\r\n";
					Contents += "\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"iPhone Developer\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-IPhone/Shipping/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"UE3-Xcode/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tIPHONEOS_DEPLOYMENT_TARGET = {0};\r\n", IPhoneToolChain.IPhoneOSVersion);
					Contents += "\t\t\tIPHONEOS_DEPLOYMENT_TARGET = 3.1.3;\r\n";
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += "\t\t\t\"PROVISIONING_PROFILE[sdk=iphoneos*]\" = \"\";\r\n";
					Contents += string.Format("\t\t\tSDKROOT = iphoneos{0};\r\n", IPhoneToolChain.IPhoneSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-IPhone/Shipping\";\r\n", GameName);
					Contents += string.Format("\t\t\tTARGETED_DEVICE_FAMILY = \"{0}\";\r\n", IPhoneToolChain.DeviceFamilies);
					Contents += "\t\t\tVALIDATE_PRODUCT = NO;\r\n";
					Contents += "\t\t\tVALID_ARCHS = armv7;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = NO;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Shipping;\r\n";
			Contents += "\t};\r\n";

			Contents += string.Format("\t{0} /* Distribution */ = {{\r\n", DistributionTargetConfigGuid);
			Contents += "\t\tisa = XCBuildConfiguration;\r\n";
				Contents += "\t\tbuildSettings = {\r\n";
					Contents += "\t\t\tARCHS = \"$(ARCHS_UNIVERSAL_IPHONE_OS)\";\r\n";
					Contents += "\t\t\t\"CODE_SIGN_IDENTITY[sdk=iphoneos*]\" = \"iPhone Distribution\";\r\n";
					Contents += string.Format("\t\t\tCONFIGURATION_BUILD_DIR = \"{0}-IPhone/Distribution/Payload\";\r\n", GameName);
					Contents += string.Format("\t\t\tINFOPLIST_FILE = \"UE3-Xcode/{0}-Info.plist\";\r\n", GameName);
					Contents += "\t\t\tINFOPLIST_OUTPUT_FORMAT = xml;\r\n";
					Contents += string.Format("\t\t\tIPHONEOS_DEPLOYMENT_TARGET = {0};\r\n", IPhoneToolChain.IPhoneOSVersion);
					Contents += "\t\t\tIPHONEOS_DEPLOYMENT_TARGET = 3.1.3;\r\n";
					Contents += "\t\t\tOBJROOT = \"UE3-Xcode/build\";\r\n";
					Contents += string.Format("\t\t\tPRODUCT_NAME = \"{0}\";\r\n", GameName);
					Contents += "\t\t\t\"PROVISIONING_PROFILE[sdk=iphoneos*]\" = \"\";\r\n";
					Contents += string.Format("\t\t\tSDKROOT = iphoneos{0};\r\n", IPhoneToolChain.IPhoneSDKVersion);
					Contents += string.Format("\t\t\tSYMROOT = \"{0}-IPhone/Distribution\";\r\n", GameName);
					Contents += string.Format("\t\t\tTARGETED_DEVICE_FAMILY = \"{0}\";\r\n", IPhoneToolChain.DeviceFamilies);
					Contents += "\t\t\tVALIDATE_PRODUCT = YES;\r\n";
					Contents += "\t\t\tVALID_ARCHS = armv7;\r\n";
					Contents += "\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\r\n";
					Contents += "\t\t\tCOPY_PHASE_STRIP = YES;\r\n";
					Contents += "\t\t\tGCC_DYNAMIC_NO_PIC = NO;\r\n";
					Contents += "\t\t\tVALIDATE_PRODUCT = YES;\r\n";
// 					Contents += "\t\t\tGCC_PRECOMPILE_PREFIX_HEADER = YES;\r\n";
// 					Contents += string.Format("\t\t\tGCC_PREFIX_HEADER = UE3.pch;\r\n", DeviceFamilies);
				Contents += "\t\t};\r\n";
				Contents += "\t\tname = Distribution;\r\n";
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
					Contents += string.Format("\t\t\t{0} /* Distribution */,\r\n", DistributionTargetConfigGuid);
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
					Contents += string.Format("\t\t\t{0} /* Distribution */,\r\n", DistributionProjectConfigGuid);
				Contents += "\t\t);\r\n";
				Contents += "\t\tdefaultConfigurationIsVisible = 0;\r\n";
				Contents += "\t\tdefaultConfigurationName = Release;\r\n";
			Contents += "\t};\r\n";

			Contents += "/* End XCConfigurationList section */\r\n";
			Contents += "\r\n";

			Contents += "\t};\r\n";
			Contents += string.Format("\trootObject = {0} /* Project object */;\r\n", ProjectGuid);
			Contents += "}";

			string ProjectPath = Path.Combine(Path.GetDirectoryName(LinkEnvironment.OutputFilePath), "UBT.xcodeproj/project.pbxproj");
			Directory.CreateDirectory(Path.GetDirectoryName(ProjectPath));
			File.WriteAllText(ProjectPath, Contents);

Console.WriteLine("Generated " + ProjectPath);
			FileItem RemoteProject = IPhoneToolChain.LocalToRemoteFileItem(FileItem.GetItemByPath(ProjectPath), true);
			return RemoteProject;
//  
//  			Action DummyAction = new Action();
// 			DummyAction.WorkingDirectory = Path.GetFullPath(".");
// 			DummyAction.CommandPath = "whoami.exe";
// 			DummyAction.CommandArguments = "";
// 
// 			// we need to copy the files over to the Mac (set up in the Compile step)
// 			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
// 			{
// 				DummyAction.PrerequisiteItems.Add(InputFile);
// 			}
// 			
// 			// touch a file that is always new, so that we run this every time we build. We could check for the .vcproj
// 			// files getting updated, but then modifying UBT source code wouldn't make this re-run, unlike other platforms
// 			string TouchFile = Path.Combine(Path.GetDirectoryName(LinkEnvironment.OutputFilePath), "UBT.xcodeproj.touch");
// 			File.WriteAllText(TouchFile, "touch");
// 			DummyAction.PrerequisiteItems.Add(FileItem.GetItemByPath(TouchFile));
// 
// 			DummyAction.ProducedItems.Add(RemoteProject);
// 			return DummyAction.ProducedItems[0];
		}
	};
}
