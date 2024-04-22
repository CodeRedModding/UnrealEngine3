/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#pragma once

#include "PlatformTarget.h"

using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Security::Cryptography;
using namespace System::Text;
using namespace System::Text::RegularExpressions;
using namespace System::Windows::Forms;

namespace ConsoleInterface
{
	//forward declarations
	ref class SharedSettings;
	ref class GameSettings;
	ref class TOCSettings;
	ref class TOCInfo;
	ref class Tag;
	ref class FileGroup;
	ref class FileFilter;
	ref class FileSet;

	[Flags]
	public enum class PlatformType
	{
		None		=	0x00000000,
		PC			=	0x00000001,
		PCServer	=	0x00000002,		// Windows platform dedicated server mode ("lean and mean" cooked as console without editor support)
		Xbox360		=	0x00000004,
		PS3			=	0x00000008,
		Linux		=	0x00000010,
		MacOSX		=	0x00000020,
		PCConsole	=	0x00000040,     // Windows platform cooked as console without editor support
		IPhone		=	0x00000080,
		NGP			=	0x00000100,
		Android		=	0x00000200,
		WiiU		=	0x00000400,
		Flash		=	0x00000800,
		All = PC | PS3 | Xbox360 | PCServer | IPhone | PCConsole | Android | NGP | MacOSX | WiiU | Flash,
		Mobile = IPhone | Android | NGP | Flash,
	};

	public ref class Platform
	{
	private:
		ref class HwndWrapper : IWin32Window
		{
		private:
			IntPtr mHandle;

		public:
			property IntPtr Handle
			{
				virtual IntPtr get() 
				{ 
					return mHandle; 
				}
			}

			HwndWrapper(HWND hWnd) : mHandle(hWnd) 
			{
			}

			HwndWrapper(IntPtr hWnd) : mHandle(hWnd) 
			{
			}
		};

	private:
		literal String ^MANIFEST_EXTENSION = L".uncompressed_size";

		FConsoleSupport *mConsoleSupport;
		Dictionary<String^, PlatformTarget^> ^mTargets;
		SharedSettings ^mSharedSettings;
		PlatformType mType;
		int mTargetEnumerationCount;
		static array<String^>^ ValidExpansions =
		{
			gcnew String( L"%GAME%" ),
			gcnew String( L"%NOTGAME%" ),
			gcnew String( L"%NOTROOTGAME%" ),
			gcnew String( L"%LANGUAGE%" ),
			gcnew String( L"%NOTLANGUAGE%" ),
			gcnew String( L"%PLATFORM%" ),
			gcnew String( L"%NOTPLATFORM%" ),
			gcnew String( L"%TEXEXTENSION%" ),
			gcnew String( L"%NOTTEXEXTENSION%" ),
		};

	public:
		property void* Module
		{
			void* get();
		}

		property array<PlatformTarget^>^ Targets
		{
			array<PlatformTarget^>^ get();
		}

		property bool NeedsToSync
		{
			bool get();
		}

		property bool IsIntelByteOrder
		{
			bool get();
		}

		property String^ Name
		{
			String^ get();
		}

		property PlatformType Type
		{
			PlatformType get();
		}

		property PlatformTarget^ DefaultTarget
		{
			PlatformTarget^ get();
		}

		property int TargetEnumerationCount
		{
			int get();
		}

	internal:
		property FConsoleSupport* ConsoleSupport
		{
			FConsoleSupport* get();
		}

	internal:
		Platform( PlatformType PlatType, FConsoleSupport *Support, SharedSettings ^Shared );

	private:
		void GetPathAndPattern( String^ FullName, String^& Path, String^& Pattern );
		List<TOCInfo^>^ TocBuild(String^ TOCName, TOCSettings^ BuildSettings, array<Tag^>^ SyncTags, String^ Language);
		void ProcessFileGroups(List<TOCInfo^>^ TOC, array<FileGroup^>^ FileGroups, TOCSettings^ BuildSettings, array<Tag^>^ SyncTags, List<String^>^ NotPlatforms, List<String^>^ NotLanguages, List<String^>^ NotGames, List<String^>^ NotTextureExtensions, String^ Language, array<String^> ^TexExtensions);
		void TocAddFile(List<TOCInfo^>^ TOC, TOCSettings^ BuildSettings, FileInfo^ SrcFile, String^ RootFolder, FileGroup^ Group);
		void TocIncludeFiles(List<TOCInfo^>^ TOC, TOCSettings^ BuildSettings, String^ SourceFolder, String^ SourcePattern, FileSet^ CurSet, FileGroup^ Group, List<String^>^ NotPlatforms, List<String^>^ NotLanguages, List<String^>^ NotGames, List<String^>^ NotTextureExtensions, String^ Language, array<String^> ^TexExtensions);
		bool IsMatchingPlatform( FileGroup^ Group );
		void FixString(String^% Str, TOCSettings^ BuildSettings, String^ Language);
		void TocMerge(List<TOCInfo^>^ CurrentTOC, List<TOCInfo^>^ OldTOC, DateTime TocLastWriteTime);
		List<TOCInfo^>^ TocRead(String^ TOCPath);
		void TocGenerateCRC(List<TOCInfo^>^ TOC, String^ TOCFilename, TOCSettings^ BuildSettings);
		void TocWrite(List<TOCInfo^>^ TOC, String^ TOCPath);
		TOCInfo^ TocFindInfoFromConsoleName(List<TOCInfo^>^ TOC, String^ ConsoleName);
		bool PcInit(TOCSettings^ BuildSettings, List<String^>^ TargetPaths);
		bool PcSync(List<List<TOCInfo^>^>^ TOCList, TOCSettings^ BuildSettings);
		bool PcRequireCopy(TOCSettings^ BuildSettings, FileInfo^ SrcFile, FileInfo^ DstFile);
		bool PcCopyFiles( TOCSettings^ BuildSettings, List<String^> ^TargetPaths, List<List<TOCInfo^>^>^ TOCList, bool bDoSyncableFiles, Int64% NumBytesPublished );
		bool PcCopyFile(TOCSettings^ BuildSettings, List<String^>^ CRCMismatchedFiles, String^ SourcePath, String^ DestPath, String^ SrcCRC);
		String^ CreateCRC(FileInfo^ SrcFile);
		bool FilterFileName(String^ FileName, String^ Filter);
		bool FilterDirectory(String^ FullPath, String^ Filter);

	protected:
		!Platform();

	public:
		~Platform();
		
		virtual String^ ToString() override;
		List<TOCInfo^>^ GenerateTOC(String^ TagSetName, TOCSettings^ BuildSettings, String^ Language);
		delegate bool SaveZipDelegate( List<List<TOCInfo^>^>^ TOCList, TOCSettings^ BuildSettings );
		delegate bool SaveIsoDelegate( List<List<TOCInfo^>^>^ TOCList, TOCSettings^ BuildSettings );
		bool TargetSync(TOCSettings^ BuildSettings, String^ TagSetName, SaveIsoDelegate^ SaveIso, SaveZipDelegate^ SaveZip);
		bool TargetSync(TOCSettings^ BuildSettings);
		bool TargetSync(TOCSettings^ BuildSettings, List<List<TOCInfo^>^>^ TOCList);

		/**
		 * Create a PlatformTarget for a given TargetHandle
		 */
		void AddTarget( TARGETHANDLE Handle );

		/**
		 * Attempt to find targets using the console tools dll
		 */
		int EnumerateAvailableTargets();

		/**
		 * Adds a stub target for UnrealConsole to use
		 */
		PlatformTarget^ ForceAddTarget( String^ TargetAddress );

		static WORD GetApplicationSubsystem();
	};
}
