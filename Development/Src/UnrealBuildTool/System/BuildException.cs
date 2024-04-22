/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	class BuildException : Exception
	{
		public BuildException(string MessageFormat, params Object[] MessageObjects):
			base( string.Format(MessageFormat, MessageObjects))
		{
		}
	};
}
