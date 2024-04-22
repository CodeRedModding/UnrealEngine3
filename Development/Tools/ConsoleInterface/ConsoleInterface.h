/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#pragma once

#include "Platform.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace ConsoleInterface
{
	//forward declarations
	ref class SharedSettings;

	public ref class DLLInterface
	{
	private:
		static Dictionary<PlatformType, Platform^> ^mPlatforms = gcnew Dictionary<PlatformType, Platform^>();
		static SharedSettings ^mSharedSettings;

	private:
		static FConsoleSupport* LoadPlatformDLL(String ^DllPath);
		static void EnumeratingPlatformsUIThread(Object ^State);

	public:
		static property ICollection<Platform^>^ Platforms
		{
			ICollection<Platform^>^ get();
		}

		static property int NumberOfPlatforms
		{
			int get();
		}

		static property SharedSettings^ Settings
		{
			SharedSettings^ get();
		}

		static bool HasPlatform(PlatformType PlatformToCheck);
		static bool LoadCookerSyncManifest( void );
		static PlatformType LoadPlatform(PlatformType PlatformsToLoad, PlatformType CurrentType, String^ ToolFolder, String^ ToolPrefix);
		static PlatformType LoadPlatforms(PlatformType PlatformsToLoad);
		static void UnloadPlatform(PlatformType PlatformsToUnload, PlatformType CurrentType);
		static void UnloadPlatforms(PlatformType PlatformsToUnload);
		static bool TryGetPlatform(PlatformType PlatformToGet, Platform ^%OutPlatform);
	};
}
