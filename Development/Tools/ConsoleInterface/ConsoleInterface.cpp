/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// This is the main DLL file.

#include "stdafx.h"
#include "SharedSettings.h"
#include "GameSettings.h"
#include "TOCInfo.h"
#include "ConsoleInterface.h"
#include "EnumeratingPlatformsForm.h"
#include "SharedSettings.h"

using namespace System::IO;
using namespace System::Xml;
using namespace System::Xml::Serialization;
using namespace System::Threading;

// freacken macro's clash with .net types
#undef GetCurrentDirectory
#undef SetCurrentDirectory

namespace ConsoleInterface
{
	Tag::Tag() 
		: Name(nullptr)
	{
	}

	TagSet::TagSet() 
		: Name(String::Empty)
		, Tags(gcnew array<Tag^>(0))
	{
	}

	GameDescription::GameDescription()
		: Name(nullptr)
	{
	}

	LanguageDescription::LanguageDescription()
		: ThreeLetterCode(nullptr)
	{
	}

	TextureExtensionDescription::TextureExtensionDescription()
		: Name(nullptr)
	{
	}

	PlatformDescription::PlatformDescription()
		: Name(nullptr)
		, bCaseSensitiveFileSystem(false)
	{
	}

	SharedSettings::SharedSettings() 
		: TagSets(gcnew array<TagSet^>(0))
		, KnownGames(gcnew array<GameDescription^>(0))
		, KnownLanguages(gcnew array<LanguageDescription^>(0))
		, KnownTextureExtensions(gcnew array<TextureExtensionDescription^>(0))
		, KnownPlatforms(gcnew array<PlatformDescription^>(0))
		, FileGroups(gcnew array<FileGroup^>(0))
	{
	}

	FileFilter::FileFilter() 
		: Name(String::Empty)
		, bIsForFilename(true)
		, bIsForDirectory(false)
	{
	}

	FileSet::FileSet() 
		: Path(nullptr)
		, bIsRecursive(false)
		, FileFilters(gcnew array<FileFilter^>(0))
	{
	}

	GameSettings::GameSettings()
		: FileGroups(gcnew array<FileGroup^>(0))
	{
	}

	FileGroup::FileGroup() 
		: bIsForSync(true)
		, bIsForTOC(true)
		, bDeploy(false)
		, Tag(String::Empty)
		, Platform(nullptr)
		, Files(gcnew array<FileSet^>(0))
	{
	}

	TOCInfo::TOCInfo( String ^InFileName, String ^Hash, DateTime LastWrite, int SizeInBytes, int CompressedSizeInBytes, FileGroup^ Group )
		: FileName(InFileName)
		, CRC(Hash)
		, LastWriteTime(LastWrite)
		, Size(SizeInBytes)
		, CompressedSize(CompressedSizeInBytes)
	{
		if( Group != nullptr )
		{
			bIsForSync = Group->bIsForSync;
			bIsForTOC = Group->bIsForTOC;
			bIsTOC = false;
			bDeploy = Group->bDeploy;
		}
		else
		{
			bIsForSync = true;
			bIsForTOC = true;
			bIsTOC = true;
			bDeploy = false;
		}
	}

	ICollection<Platform^>^ DLLInterface::Platforms::get()
	{
		return mPlatforms->Values;
	}

	int DLLInterface::NumberOfPlatforms::get()
	{
		return mPlatforms->Count;
	}

	SharedSettings^ DLLInterface::Settings::get()
	{
		return mSharedSettings;
	}

	void DLLInterface::EnumeratingPlatformsUIThread(Object ^State)
	{
		ManualResetEvent ^Event = (ManualResetEvent^)State;

		if(Event != nullptr)
		{
			Application::Run(gcnew EnumeratingPlatformsForm(Event));
		}
	}

	FConsoleSupport* DLLInterface::LoadPlatformDLL(String ^DllPath)
	{
		if(DllPath == nullptr)
		{
			throw gcnew ArgumentNullException(L"Path");
		}

		//NOTE: We have to do this in case the target dll links to any dll's at load time.
		String^ CurDir = Directory::GetCurrentDirectory();
		DllPath = Path::GetFullPath(DllPath);
		String^ NewDir = Path::GetDirectoryName(DllPath);

		if(Directory::Exists(NewDir))
		{
			Directory::SetCurrentDirectory(NewDir);

			pin_ptr<const wchar_t> NativePath = PtrToStringChars(Path::GetFileName(DllPath));

			try
			{
				//NOTE: we'll leave this in our address space and let it be unloaded when the process is destroyed
				HMODULE Module = LoadLibraryW(NativePath);

				// restore our working directory
				Directory::SetCurrentDirectory(CurDir);

				if(Module)
				{
					FuncGetConsoleSupport SupportFunc = (FuncGetConsoleSupport)GetProcAddress(Module, "GetConsoleSupport");
					if(SupportFunc)
					{
						FConsoleSupport* ConsoleSupport = SupportFunc(Module);
						if( ConsoleSupport != NULL )
						{
							return ConsoleSupport;
						}
					}

					FreeLibrary( Module );
				}
			}
			catch( Exception^ Ex )
			{
				String^ Error = Ex->ToString();
				Console::WriteLine( Error );
				System::Diagnostics::Debug::WriteLine( Error );
			}
		}

		return NULL;
	}

	bool DLLInterface::HasPlatform(PlatformType PlatformToCheck)
	{
		return mPlatforms->ContainsKey(PlatformToCheck);
	}

	bool DLLInterface::LoadCookerSyncManifest( void )
	{
		if( mSharedSettings == nullptr )
		{
			try
			{
				XmlReader^ Reader = XmlReader::Create( L"CookerSync.xml" );
				XmlSerializer^ Serializer = gcnew XmlSerializer( SharedSettings::typeid );

				mSharedSettings = ( SharedSettings^ )Serializer->Deserialize( Reader );

				delete Serializer;
				delete Reader;
			}
			catch( Exception^ Ex )
			{
				String^ Error = Ex->ToString();
				Console::WriteLine( Error );
				System::Diagnostics::Debug::WriteLine( Error );
				mSharedSettings = gcnew SharedSettings();

				System::Windows::Forms::MessageBox::Show( nullptr, 
														  "CookerSync.xml failed to load; UnrealFrontEnd will not function correctly.\r\n\r\n" + Error, 
														  "CookerSync.xml Load Error!", 
														  System::Windows::Forms::MessageBoxButtons::OK, 
														  System::Windows::Forms::MessageBoxIcon::Error );
				return( false );
			}
		}

		return( true );
	}

	PlatformType DLLInterface::LoadPlatform( PlatformType PlatformsToLoad, PlatformType CurrentType, String^ ToolFolder, String^ ToolPrefix )
	{
		if( ( PlatformsToLoad & CurrentType ) == CurrentType && !mPlatforms->ContainsKey( CurrentType ) )
		{
#if _WIN64
			FConsoleSupport *Support = LoadPlatformDLL( ToolFolder + L"\\" + ToolPrefix + L"Tools_x64.dll" );
#else
			FConsoleSupport *Support = LoadPlatformDLL( ToolFolder + L"\\" + ToolPrefix + L"Tools.dll" );
#endif
			if( Support )
			{
				mPlatforms->Add( CurrentType, gcnew Platform( CurrentType, Support, mSharedSettings ) );
			}
			else
			{
				PlatformsToLoad = ( PlatformType )( PlatformsToLoad & ~CurrentType );
			}
		}

		return( PlatformsToLoad );
	}

	void DLLInterface::UnloadPlatform( PlatformType PlatformsToUnload, PlatformType CurrentType )
	{
		if( ( PlatformsToUnload & CurrentType ) == CurrentType && mPlatforms->ContainsKey( CurrentType ) )
		{
			Platform^ TargetPlatform = nullptr;
			mPlatforms->TryGetValue( CurrentType, TargetPlatform );

			if( TargetPlatform != nullptr )
			{
				HMODULE Module = ( HMODULE )TargetPlatform->Module;

				TargetPlatform->~Platform();
				if( Module != NULL )
				{
					FreeLibrary(Module);
				}

				mPlatforms->Remove( CurrentType );
			}
		}
	}

	/** 
	 * Attempt to load all the requested tools dlls
	 */
	PlatformType DLLInterface::LoadPlatforms( PlatformType PlatformsToLoad )
	{
#if _WIN64
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PC, L"Win64", L"Windows" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PCServer, L"Win64", L"Windows" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PCConsole, L"Win64", L"Windows" );
#else
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PC, L"Win32", L"Windows" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PCServer, L"Win32", L"Windows" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PCConsole, L"Win32", L"Windows" );
#endif
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::Xbox360, L"Xbox360", L"Xbox360" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::PS3, L"PS3", L"PS3" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::MacOSX, L"Mac", L"Mac" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::IPhone, L"IPhone", L"IPhone" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::Android, L"Android", L"Android" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::NGP, L"NGP", L"NGP" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::WiiU, L"WiiU", L"WiiU" );
		PlatformsToLoad = LoadPlatform( PlatformsToLoad, PlatformType::Flash, L"Flash", L"Flash" );

		return PlatformsToLoad;
	}

	/** 
	 * Unload all the requested (potentially) loaded tools dlls
	 */
	void DLLInterface::UnloadPlatforms( PlatformType PlatformsToUnload )
	{
#if _WIN64
		UnloadPlatform( PlatformsToUnload, PlatformType::PC );
		UnloadPlatform( PlatformsToUnload, PlatformType::PCServer );
		UnloadPlatform( PlatformsToUnload, PlatformType::PCConsole );
#else
		UnloadPlatform( PlatformsToUnload, PlatformType::PC );
		UnloadPlatform( PlatformsToUnload, PlatformType::PCServer );
		UnloadPlatform( PlatformsToUnload, PlatformType::PCConsole );
#endif
		UnloadPlatform( PlatformsToUnload, PlatformType::Xbox360 );
		UnloadPlatform( PlatformsToUnload, PlatformType::PS3 );
		UnloadPlatform( PlatformsToUnload, PlatformType::MacOSX );
		UnloadPlatform( PlatformsToUnload, PlatformType::IPhone );
		UnloadPlatform( PlatformsToUnload, PlatformType::Android );
		UnloadPlatform( PlatformsToUnload, PlatformType::NGP );
		UnloadPlatform( PlatformsToUnload, PlatformType::WiiU );
		UnloadPlatform( PlatformsToUnload, PlatformType::Flash );
	}

	bool DLLInterface::TryGetPlatform(PlatformType PlatformToGet, Platform^ %OutPlatform)
	{
		return mPlatforms->TryGetValue(PlatformToGet, OutPlatform);
	}
}