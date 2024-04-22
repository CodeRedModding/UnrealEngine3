// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

using namespace System;
using namespace System::Drawing;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace ConsoleInterface
{
	//forward declarations
	ref class GameSettings;
	ref class OutputEventArgs;

	public delegate void OutputHandlerDelegate(Object ^sender, OutputEventArgs ^e);
	public delegate void WriteLineNativeDelegate(unsigned int TxtColor, [MarshalAs(UnmanagedType::LPWStr)] String ^Message);

	public ref class TOCSettings
	{
	private:
		OutputHandlerDelegate^			mOutputHandler;

	public:
		array<String^>^					Languages;
		String^							GameName;
		String^							TargetBaseDirectory;
		array<String^>^					TextureExtensions;
		Dictionary<String^, String^>^	GenericVars;
		GameSettings^					GameOptions;
		List<String^>^					DestinationPaths;
		List<String^>^					ZipFiles;
		List<String^>^					IsoFiles;
		List<String^>^					TargetsToSync;
		bool 							MergeExistingCRC;
		bool 							ComputeCRC;
		bool							bCaseSensitiveFileSystem;
		bool 							NoSync;
		bool 							NoDest;
		bool 							bInvertedCopy;
		bool 							VerifyCopy;
		bool 							Force;
		bool 							GenerateTOC;
		bool 							VerboseOutput;
		bool 							bRebootBeforeCopy;
		bool 							bNonInteractive;
		int								SleepDelay;

	public:
		TOCSettings(OutputHandlerDelegate ^OutputHandler);

		void Write(Color TxtColor, String^ Message, ... array<Object^>^ Parms);
		void WriteLine(Color TxtColor, String^ Message, ... array<Object^>^ Parms);
		void WriteLine(Color TxtColor, String^ Message);
		void WriteLine(unsigned int TxtColor, String^ Message);
	};
}
