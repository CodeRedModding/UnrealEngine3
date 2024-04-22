/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool
{
	partial class UE3BuildTarget
	{
		bool IPhoneSupportEnabled()
		{
			return false;
		}

		void SetUpIPhoneEnvironment()
		{            
		}

        void SetUpIPhonePhysXEnvironment()
        {
        }

		List<FileItem> GetIPhoneOutputItems()
		{
			return new List<FileItem>();
		}
	}
}
