/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

using namespace System;
using namespace System::Xml::Serialization;

namespace ConsoleInterface
{
	//forward declarations
	ref class FileGroup;

	// Summary description for per-game CookerSettings.
	[Serializable]
	public ref class GameSettings
	{
	public:
		// Set of file groups for syncing
		[XmlArray]
		array<FileGroup^> ^FileGroups;

	public:
		GameSettings();
	};
}
