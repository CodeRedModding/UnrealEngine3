/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// NVIDIA

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	class AndroidToolChain
	{
		public static CPPOutput CompileCPPFiles( CPPEnvironment CompileEnvironment, IEnumerable<FileItem> SourceFiles )
		{
			return null;
		}

		public static FileItem LinkFiles( LinkEnvironment LinkEnvironment )
		{
			return null;
		}

		public static bool AndroidAPKPacker( string GameName, UnrealTargetConfiguration Configuration, string OutputDirectory )
		{
			return true;
		}
	};
}