/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool
{
	class IPhoneToolChain
	{
		public static void SetUpGlobalEnvironment()
		{
		}

		public static string LocalToMacPath(string LocalPath, bool bIsHomeRelative)
		{
			return null;
		}

		public static bool GetRemoteFileInfo(string RemotePath, out DateTime ModificationTime, out long Length)
		{
			ModificationTime = DateTime.MinValue;
			Length = 0;
			return false;
		}

		public static void SyncHelper( string GameName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string LocalShadowDirectoryRoot, bool bPreBuild )
		{
		}

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, IEnumerable<FileItem> SourceFiles)
		{
			return null;
		}

		public static FileItem LinkFiles(LinkEnvironment LinkEnvironment)
		{
			return null;
		}

		public static Double GetAdjustedProcessorCountMultiplier()
		{
			return 1.0;
		}
	};

	class XcodeToolChain : IPhoneToolChain
	{

	};
}
