/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "Stdafx.h"
#include "Platform.h"
#include "TOCSettings.h"
#include "TOCInfo.h"
#include "SharedSettings.h"
#include "GameSettings.h"
#include "SyncFileSocket.h"
#include "ConnectToConsoleRetryForm.h"
#include <stdio.h>
#include <vector>

using namespace System::Xml;
using namespace System::Xml::Serialization;
using namespace System::Reflection;
using namespace System::Net;
using namespace System::Net::Sockets;
using namespace System::Diagnostics;

#define VALIDATE_SUPPORT(x) if(!(x)) throw gcnew InvalidOperationException(L"Invalid platform!")
#undef GetCurrentDirectory

namespace ConsoleInterface
{
	void* Platform::Module::get()
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		return mConsoleSupport->GetModule();
	}

	array<PlatformTarget^>^ Platform::Targets::get()
	{
		Dictionary<String^, PlatformTarget^>::ValueCollection ^ValCollection = mTargets->Values;

		array<PlatformTarget^> ^Result = gcnew array<PlatformTarget^>(ValCollection->Count);
		ValCollection->CopyTo(Result, 0);

		return Result;
	}

	bool Platform::NeedsToSync::get()
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		return mConsoleSupport->PlatformNeedsToCopyFiles();
	}

	bool Platform::IsIntelByteOrder::get()
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		return mConsoleSupport->GetIntelByteOrder();
	}

	FConsoleSupport* Platform::ConsoleSupport::get()
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		return mConsoleSupport;
	}

	String^ Platform::Name::get()
	{
		return mType.ToString();
	}

	PlatformType Platform::Type::get()
	{
		return mType;
	}

	PlatformTarget^ Platform::DefaultTarget::get()
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		PlatformTarget^ DefaultTarget = nullptr;
		TargetHandle DefaultHandle = TargetHandle( mConsoleSupport->GetDefaultTarget() );

		for each(PlatformTarget ^CurTarget in mTargets->Values)
		{
			if(CurTarget->Handle == DefaultHandle)
			{
				DefaultTarget = CurTarget;
				break;
			}
		}

		return DefaultTarget;
	}

	int Platform::TargetEnumerationCount::get()
	{
		return mTargetEnumerationCount;
	}

	Platform::Platform(PlatformType PlatType, FConsoleSupport *Support, SharedSettings ^Shared) 
		: mTargetEnumerationCount(0)
	{
		if(Support == NULL)
		{
			throw gcnew ArgumentNullException(L"Support");
		}

		mSharedSettings = Shared;
		mType = PlatType;
		mConsoleSupport = Support;
		mTargets = gcnew Dictionary<String^, PlatformTarget^>();
	}

	Platform::~Platform()
	{
		this->!Platform();
		GC::SuppressFinalize(this);
	}

	Platform::!Platform()
	{
		mConsoleSupport->Destroy();
		mConsoleSupport = NULL;
	}

	String^ Platform::ToString()
	{
		return mType.ToString();
	}

	String^ OptionallyUppercase( TOCSettings^ BuildSettings, String^ In, bool bIsForSync )
	{		
		if( BuildSettings->bCaseSensitiveFileSystem && bIsForSync )
		{
			return( In->ToUpper() );
		}
		
		return( In );
	}

	List<TOCInfo^>^ Platform::GenerateTOC(String^ TagSetName, TOCSettings^ BuildSettings, String^ Language)
	{
		if(BuildSettings == nullptr)
		{
			throw gcnew ArgumentNullException(L"BuildSettings");
		}

		if(Language == nullptr)
		{
			throw gcnew ArgumentNullException(L"Language");
		}

		// Set up the language
		if(BuildSettings->Languages == nullptr || BuildSettings->Languages->Length == 0)
		{
			BuildSettings->Languages = gcnew array<String^> { L"INT" };
		}

		if(Language->Length != 3)
		{
			throw gcnew System::ArgumentException("Languages must be 3 characters long i.e. \"INT\".", "Language");
		}

		Language = Language->ToUpper();
		String ^TOCFilename;
		if (Language->Equals(L"INT"))
		{
			TOCFilename = String::Format(L"{0}TOC.txt", mType.ToString());
		}
		else
		{
			TOCFilename = String::Format(L"{0}TOC_{1}.txt", mType.ToString(), Language);
		}
		String^ TOCPath = Path::Combine(Directory::GetCurrentDirectory(), String::Format(L"..\\{0}\\{1}", BuildSettings->GameName, TOCFilename));

		TOCPath = OptionallyUppercase( BuildSettings, TOCPath, true );
		
		BuildSettings->WriteLine( Color::Black, L"Using TOC: {0}", TOCPath );

		array<Tag^> ^SyncTags = nullptr;

		if(TagSetName != nullptr)
		{
			for each(TagSet ^CurSet in mSharedSettings->TagSets)
			{
				if(TagSetName->Equals(CurSet->Name,StringComparison::OrdinalIgnoreCase))
				{
					SyncTags = CurSet->Tags;

					// Display the tagset we're using
					BuildSettings->WriteLine(Color::Black, L"Using tagset: {0}", CurSet->Name);
					break;
				}
			}

			if( SyncTags == nullptr )
			{
				BuildSettings->WriteLine(Color::Black, L"No tagset found matching: {0}", TagSetName);
			}
		}

		// Build up the file-list (we always need to do this)
		List<TOCInfo^> ^TOC = TocBuild(TOCFilename, BuildSettings, SyncTags, Language);

		// Merge the CRCs from the existing TOC file (if there is one)

		if(BuildSettings->MergeExistingCRC)
		{
			FileInfo ^TocInfo = gcnew FileInfo(TOCPath);

			if(TocInfo->Exists)
			{
				List<TOCInfo^> ^PreviousTOC = TocRead(TOCPath);
				TocMerge(TOC, PreviousTOC, TocInfo->LastWriteTimeUtc);
			}
		}

		if(BuildSettings->ComputeCRC)
		{
			TocGenerateCRC(TOC, TOCFilename, BuildSettings);
		}

		if(BuildSettings->GenerateTOC)
		{
			TocWrite(TOC, TOCPath);
		}

		return TOC;
	}

	void Platform::GetPathAndPattern( String^ FullName, String^& Path, String^& Pattern )
	{
		// find the last slash in the path
		int LastSlash = FullName->LastIndexOfAny( gcnew array<Char> { L'/', L'\\' } );

		// if no slash, use the current directory
		if( LastSlash == -1 )
		{
			Pattern = FullName;
		}
		else
		{
			// get the first bit including the slash
			Path = FullName->Substring( 0, LastSlash + 1 );
			Pattern = FullName->Substring( LastSlash + 1 );
		}
	}

	List<TOCInfo^>^ Platform::TocBuild(String^ TOCName, TOCSettings^ BuildSettings, array<Tag^>^ SyncTags, String^ Language)
	{
		if(BuildSettings == nullptr)
		{
			throw gcnew ArgumentNullException(L"BuildSettings");
		}

		BuildSettings->WriteLine(Color::Black, L"Generating {0}...", TOCName);

		List<TOCInfo^>^ TOC = gcnew List<TOCInfo^>();
		List<String^>^ NotPlatforms = gcnew List<String^>();
		List<String^>^ NotLanguages = gcnew List<String^>();
		List<String^>^ NotTexExtensions = gcnew List<String^>();
		List<String^>^ NotGames = gcnew List<String^>();
		String ^PlatformStr = mType.ToString();
		int CurTime = Environment::TickCount;

		// make the list of texture extensions (making dummy in the common case of none)
		array<String^>^ TextureExtensions = BuildSettings->TextureExtensions;
		if (TextureExtensions == nullptr)
		{
			TextureExtensions = gcnew array<String^>(1);
			TextureExtensions[0] = L"";
		}

		// add all platforms not this one
		for each(PlatformDescription ^PlatformDesc in mSharedSettings->KnownPlatforms)
		{
			// check this platform to see if it doesn't match the one we are syncing
			if(!PlatformDesc->Name->Equals(PlatformStr, StringComparison::OrdinalIgnoreCase))
			{
				NotPlatforms->Add(PlatformDesc->Name);
			}
		}

		// add all languages not this one
		for each(LanguageDescription ^LanguageDesc in mSharedSettings->KnownLanguages)
		{
			// check this platform to see if it doesn't matches the one we are syncing
			if(!LanguageDesc->ThreeLetterCode->Equals(Language, StringComparison::OrdinalIgnoreCase))
			{
				NotLanguages->Add(LanguageDesc->ThreeLetterCode);
			}
		}

		// add all texture extensions not this one
		for each(TextureExtensionDescription ^TextureDesc in mSharedSettings->KnownTextureExtensions)
		{
			// check this texture extension to see if it doesn't match the ones we are syncing
			bool IsExtensionUsed = false;
			for each (String^ TexExtension in TextureExtensions)
			{
				if (TextureDesc->Name->Equals(TexExtension, StringComparison::OrdinalIgnoreCase))
				{
					IsExtensionUsed = true;
				}	
			}

			if (!IsExtensionUsed)
			{
				NotTexExtensions->Add(TextureDesc->Name);
			}	
		}

		// add all games not this one
		List<String^>^ KnownGames = UnrealControls::GameLocator::LocateGames();
		for each(String^ Game in KnownGames)
		{
			// check this platform to see if it doesn't matches the one we are syncing
			if(String::Compare(String::Format( L"{0}Game", Game), BuildSettings->GameName, StringComparison::OrdinalIgnoreCase) != 0)
			{
				NotGames->Add(Game);
			}
		}

		// process all the file groups
		if(mSharedSettings != nullptr && mSharedSettings->FileGroups != nullptr)
		{
			ProcessFileGroups(TOC, mSharedSettings->FileGroups, BuildSettings, SyncTags, NotPlatforms, NotLanguages, NotGames, NotTexExtensions, Language, TextureExtensions);
		}

		if(BuildSettings->GameOptions != nullptr && BuildSettings->GameOptions->FileGroups != nullptr)
		{
			ProcessFileGroups(TOC, BuildSettings->GameOptions->FileGroups, BuildSettings, SyncTags, NotPlatforms, NotLanguages, NotGames, NotTexExtensions, Language, TextureExtensions);
		}

		// *ALWAYS* include TOC
		String^ DestinationFolder = String::Format( L"{0}\\", BuildSettings->GameName );
		String^ RootFolder = Path::GetFullPath( ".." );
		String^ SourceFolder = Path::Combine( RootFolder, DestinationFolder );

		try
		{
			if(TOCName->Length > 0)
			{
				String ^TOCPath = Path::Combine( SourceFolder, TOCName );
				TocAddFile(TOC, BuildSettings, gcnew FileInfo( TOCPath ), RootFolder, nullptr);
			}
		}
		catch(Exception ^Ex) //Note: an ArgumentException will be thrown for an improperly formatted path name
		{
			String ^Error = Ex->ToString();
			System::Diagnostics::Debug::WriteLine(Error);
			Console::WriteLine(Error);
		}

		float Elapsed = (Environment::TickCount - CurTime) / 1000.0f;
		BuildSettings->WriteLine(Color::Black, L"Generating {0} took {1} seconds.", TOCName, Elapsed.ToString());

		return TOC;
	}

	static bool DoesPlatformMatchGroup(PlatformType Platform, String^ GroupPlatform)
	{
		// Check for the all platforms wildcard
		if( GroupPlatform->Equals( L"*" ) )
		{
			return( true );
		}

		// Check for the all pc platforms wildcard
		if( GroupPlatform->Equals( L"pc*" ) )
		{
			// All PC types accept this
			if( Platform == PlatformType::PC || Platform == PlatformType::PCConsole || Platform == PlatformType::PCServer )
			{
				return( true );
			}
		}

		// Check for the console wildcard
		if( GroupPlatform->Equals( L"console" ) )
		{
			// Console is everything other than PC and PCServer
			if( Platform != PlatformType::PC && Platform != PlatformType::PCServer )
			{
				return( true );
			}
		}

		// Check for the mobile wildcard
		if( GroupPlatform->Equals( L"mobile" ) )
		{
			// match against any mobile platform bit
			if( ((DWORD)Platform & (DWORD)PlatformType::Mobile) != 0 )
			{
				return( true );
			}
		}

		// Check for exact platform match
		if( GroupPlatform->Equals( Platform.ToString()->ToLower() ) )
		{
			return( true );
		}

		return false;

	}

	bool Platform::IsMatchingPlatform( FileGroup^ Group )
	{
		if( Group->Platform == nullptr )
		{
			return( true );
		}

		// the platform can have an optional "not" platform(s)
		array<String^>^ GroupPlatformTokens = Group->Platform->ToLower()->Split('!');
		// make sure we have at least one token
		if (GroupPlatformTokens->Length == 0)
		{
			return false;
		}

		// first token is the inclusive (yes) platform
		if (DoesPlatformMatchGroup(mType, GroupPlatformTokens[0]))
		{
			// any others are exclusive (no) platforms
			for (int NoPlatformIndex = 1; NoPlatformIndex < GroupPlatformTokens->Length; NoPlatformIndex++)
			{
				if (DoesPlatformMatchGroup(mType, GroupPlatformTokens[NoPlatformIndex]))
				{
					return false;
				}
			}

			// if it got into this block, and didn't return false in the above block
			return true;
		}

		return( false );
	}

	void Platform::ProcessFileGroups(List<TOCInfo^> ^TOC, array<FileGroup^> ^FileGroups, TOCSettings ^BuildSettings, array<Tag^> ^SyncTags, List<String^> ^NotPlatforms, List<String^> ^NotLanguages, List<String^> ^NotGames, List<String^> ^NotTextureExtensions, String ^Language, array<String^> ^TexExtensions)
	{
		for each(FileGroup ^Group in FileGroups)
		{
			if (IsMatchingPlatform( Group ) )
			{
				// is this group used for this sync based on tags?
				bool bTagsMatch = false;

				// if the SyncTags array is null, then we will sync all possible tags
				if(SyncTags == nullptr)
				{
					bTagsMatch = true;
				}
				else
				{
					// process tags
					for each(Tag ^CurTag in SyncTags)
					{
						// case-insensitive comparison of group's tag with the list of tags to sync
						if(String::Compare(Group->Tag, CurTag->Name, true) == 0)
						{
							bTagsMatch = true;
						}
					}
				}

				// at this point, property filtering passed, platform matched, and one or more tags matched,
				// so add it to the TOC
				if(bTagsMatch)
				{
					for each (FileSet ^CurSet in Group->Files)
					{
						String ^Path = String::Empty;
						String ^Wildcard = String::Empty;

						GetPathAndPattern( CurSet->Path, Path, Wildcard );

						// add the files to the TOC
						if( BuildSettings->VerboseOutput )
						{
							BuildSettings->WriteLine( Color::Purple, L"Including: " + Path + L" " + Wildcard );
						}

						TocIncludeFiles(TOC, BuildSettings, Path, Wildcard, CurSet, Group, NotPlatforms, NotLanguages, NotGames, NotTextureExtensions, Language, TexExtensions);
					}
				}
			}
		}
	}

	void Platform::TocAddFile(List<TOCInfo^> ^TOC, TOCSettings ^BuildSettings, FileInfo ^SrcFile, String^ CurrentDir, FileGroup^ Group)
	{
		if( CurrentDir == nullptr )
		{
			throw gcnew ArgumentNullException( L"CurrentDir" );
		}

		// Full source and destination filenames.
		String^ SourceFile = SrcFile->FullName->Replace( '/', '\\' );
		String^ TargetFile = ".." + SourceFile->Replace( CurrentDir, "" );

		// Check for duplicates - this is not efficient =(
		for each( TOCInfo^ TOCEntry in TOC )
		{
			if( !String::Compare( TOCEntry->FileName, TargetFile, true ) )
			{
				BuildSettings->WriteLine( Color::Orange, L"WARNING: Attempted to add duplicate file to TOC '" + TargetFile + L"'" );
				return;
			}
		}

		// by default, this file wasn't compressed fully
		int UncompressedSize = 0;

		String ^ManifestFilename = String::Concat(SourceFile, MANIFEST_EXTENSION);

		// look to see if there is an .uncompressed_size file 
		if(File::Exists(ManifestFilename))
		{
			// if so, open it and read the size out of it
			StreamReader ^Reader = File::OpenText(ManifestFilename);
			String ^Line = Reader->ReadLine();
			UncompressedSize = int::Parse(Line);
			Reader->Close();
			delete Reader;
		}

		TargetFile = OptionallyUppercase(BuildSettings, TargetFile, Group ? Group->bIsForSync : true );

		if(SrcFile->Exists)
		{
			// add the file size, and optional uncompressed size
			TOC->Add(gcnew TOCInfo(TargetFile, L"0", SrcFile->LastWriteTimeUtc, (int)SrcFile->Length, UncompressedSize, Group));
		}
		else
		{
			// In case the file is force added and doesn't exist - e.g. the TOC file
			TOC->Add(gcnew TOCInfo(TargetFile, L"0", DateTime::UtcNow, 0, UncompressedSize, Group));
		}
	}

	void Platform::TocIncludeFiles(List<TOCInfo^> ^TOC, TOCSettings ^BuildSettings, String ^SourceFolder, String ^SourcePattern, FileSet ^CurSet, FileGroup ^Group, List<String^> ^NotPlatforms, List<String^> ^NotLanguages, List<String^> ^NotGames, List<String^> ^NotTextureExtensions, String ^Language, array<String^> ^TexExtensions)
	{
		String^ RootFolder = gcnew String( L"" );

		// replace special tags
		FixString(SourceFolder, BuildSettings, Language);
		FixString(SourcePattern, BuildSettings, Language);

		// Convert relative paths to platform specific ones.
		if( BuildSettings->bInvertedCopy )
		{
			// "d:\Dest" + "UnrealEngine3"
			RootFolder = Path::Combine( BuildSettings->DestinationPaths[0], BuildSettings->TargetBaseDirectory );
			SourceFolder = Path::Combine(RootFolder, SourceFolder);
		}
		else
		{
			// "d:\depot\UnrealEngine3"
			RootFolder = Path::GetFullPath( ".." );
			SourceFolder = Path::Combine(RootFolder, SourceFolder);
		}

		// Canonise the folder names
		RootFolder = RootFolder->Replace( '/', '\\' );
		SourceFolder = SourceFolder->Replace( '/', '\\' );

		// make sure destination folder exists
		try
		{
			// Find files matching pattern.
			DirectoryInfo ^SourceDirectory = gcnew DirectoryInfo(SourceFolder);

			if(!SourceDirectory->Exists)
			{
				return;
			}

			array<FileInfo^> ^Files = SourceDirectory->GetFiles(SourcePattern, CurSet->bIsRecursive ? SearchOption::AllDirectories : SearchOption::TopDirectoryOnly);
			List<String^> ^ExpandedFileFilters = nullptr;
			List<String^> ^ExpandedDirFilters = nullptr;

			if(CurSet->FileFilters != nullptr)
			{
				// handle replacement of some trickier variables (that don't have a 1 to 1 replacement)
				for each(FileFilter ^Filter in CurSet->FileFilters)
				{
					// one pass for files, one for directories
					for (INT FileDirPass = 0; FileDirPass < 2; FileDirPass++)
					{
						// only allow file filters in pass 0, and dir filters in pass 1
						if ((Filter->bIsForFilename && FileDirPass != 0) || (Filter->bIsForDirectory && FileDirPass != 1))
						{
							continue;
						}

						List<String^> ^ExpandedFilters = nullptr;
						// pick the proper filter list, allocating it if necessary
						if (FileDirPass == 0)
						{
							if (ExpandedFileFilters == nullptr)
							{
								ExpandedFileFilters = gcnew List<String^>();
							}
							ExpandedFilters = ExpandedFileFilters;
						}
						else
						{
							if (ExpandedDirFilters == nullptr)
							{
								ExpandedDirFilters = gcnew List<String^>();
							}
							ExpandedFilters = ExpandedDirFilters;
						}

						// Find the expansion type
						int Expansion = 0;
						bool bFoundExpansion = false;
						for( int ExpansionIndex = 0; ExpansionIndex < ValidExpansions->Length; ExpansionIndex++ )
						{
							if( Filter->Name->Contains( ValidExpansions[ExpansionIndex] ) )
							{
								if( !bFoundExpansion )
								{
									Expansion = ExpansionIndex;
									bFoundExpansion = true;
								}
								else
								{
									Expansion = -1;
								}
							}
						}

						// We can't handle more than one at a time
						if( Expansion < 0 )
						{
							String^ Error = String::Format(L"Only one %var% is currently allowed in FileFilters in [{0}]", Filter->Name);
							System::Diagnostics::Debug::WriteLine(Error);
							Console::WriteLine(Error);
							continue;
						}

						if( !bFoundExpansion )
						{
							// Raw string filter
							ExpandedFilters->Add( Filter->Name );
						}
						else
						{
							String ^Fmt;

							switch( Expansion )
							{
							case 0:		// "%GAME%"
								Fmt = Filter->Name->Replace( L"%GAME%", L"{0}" );
								ExpandedFilters->Add( String::Format( Fmt, BuildSettings->GameName ) );
								break;

							case 1:		// "%NOTGAME%"
								Fmt = Filter->Name->Replace( L"%NOTGAME%", L"{0}Game" );

								for each( String ^CurGame in NotGames )
								{
									ExpandedFilters->Add( String::Format( Fmt, CurGame ) );
								}
								break;

							case 2:		// "%NOTROOTGAME%"
								Fmt = Filter->Name->Replace( L"%NOTROOTGAME%", L"{0}" );

								for each( String ^CurGame in NotGames )
								{
									ExpandedFilters->Add( String::Format( Fmt, CurGame ) );
								}
								break;

							case 3:		// "%LANGUAGE%"
								Fmt = Filter->Name->Replace( L"%LANGUAGE%", L"{0}" );
								ExpandedFilters->Add( String::Format( Fmt, Language ) );
								break;

							case 4:		// "%NOTLANGUAGE%"
								Fmt = Filter->Name->Replace( L"%NOTLANGUAGE%", L"{0}" );

								for each( String ^CurLang in NotLanguages )
								{
									ExpandedFilters->Add( String::Format( Fmt, CurLang ) );
								}
								break;

							case 5:		// "%PLATFORM%"
								Fmt = Filter->Name->Replace( L"%PLATFORM%", L"{0}" );
								ExpandedFilters->Add( String::Format( Fmt, mType.ToString() ) );
								break;

							case 6:		// "%NOTPLATFORM%"
								Fmt = Filter->Name->Replace( L"%NOTPLATFORM%", L"{0}" );

								for each( String ^CurPlatform in NotPlatforms )
								{
									ExpandedFilters->Add( String::Format( Fmt, CurPlatform ) );
								}
								break;

							case 7:		// "%TEXEXTENSION%"
								if (TexExtensions && TexExtensions[0] != "")
								{
									Fmt = Filter->Name->Replace( L"%TEXEXTENSION%", L"_{0}" );

									for each( String ^CurTex in TexExtensions )
									{ 
										ExpandedFilters->Add( String::Format( Fmt, CurTex ) );
									}
								}
								else
								{
									ExpandedFilters->Add( Filter->Name->Replace( L"%TEXEXTENSION%", L"" ) );
								}
								break;

							case 8:		// "%NOTTEXEXTENSION%"
								Fmt = Filter->Name->Replace( L"%NOTTEXEXTENSION%", L"{0}" );

								for each( String ^CurTex in NotTextureExtensions )
								{
									ExpandedFilters->Add( String::Format( Fmt, CurTex ) );
								}
								break;
							}
						}
					}
				}
			}

			for each(FileInfo ^SrcFile in Files)
			{
				bool bWasFilteredOut = false;

				// look to see if this file should not be copied
				if(ExpandedFileFilters != nullptr)
				{
					for each(String ^Filter in ExpandedFileFilters)
					{
						if(FilterFileName(SrcFile->Name, Filter))
						{
							bWasFilteredOut = true;
							break;
						}
					}
				}

				if (ExpandedDirFilters != nullptr)
				{
					for each(String ^Filter in ExpandedDirFilters)
					{
						if(FilterDirectory(SrcFile->FullName, Filter))
						{
							bWasFilteredOut = true;
							break;
						}
					}
				}

				// only deal with this file if it wasn't filtered out
				if (!bWasFilteredOut)
				{
					TocAddFile(TOC, BuildSettings, SrcFile, RootFolder, Group);
					if( BuildSettings->VerboseOutput )
					{
						BuildSettings->WriteLine( Color::Green, L"    Added:     " + SrcFile->ToString() );
					}
				}
				else
				{
					if( BuildSettings->VerboseOutput )
					{
						BuildSettings->WriteLine( Color::Red, L"    Filtered: " + SrcFile->ToString() );
					}
				}
			}
		}
		// gracefully handle copying from non existent folder.
		catch(System::IO::DirectoryNotFoundException^)
		{
		}
	}

	bool Platform::FilterFileName(String ^FileName, String ^Filter)
	{
		if(!Filter->Contains(L"*"))
		{
			return FileName->Equals(Filter, StringComparison::OrdinalIgnoreCase);
		}

		array<String^> ^FilterSegments = Filter->Split(gcnew array<Char> { L'*' }, StringSplitOptions::None);
		bool bHasVariableBeginning = FilterSegments[0]->Length == 0;
		bool bHasVariableEnd = FilterSegments[FilterSegments->Length - 1]->Length == 0;
		int NumSegments = FilterSegments[FilterSegments->Length - 1]->Length == 0 ? FilterSegments->Length - 1 : FilterSegments->Length;
		int CurSegment = bHasVariableBeginning ? 1 : 0;
		int FileNameIndex = 0;

		for(; FileNameIndex < FileName->Length && CurSegment < NumSegments; ++FileNameIndex)
		{
			if(Char::ToLowerInvariant(FileName[FileNameIndex]) == Char::ToLowerInvariant(FilterSegments[CurSegment][0]) && FileName->IndexOf(FilterSegments[CurSegment], FileNameIndex, StringComparison::OrdinalIgnoreCase) == FileNameIndex)
			{
				// -1 because the loop does a + 1
				FileNameIndex += FilterSegments[CurSegment]->Length - 1;
				++CurSegment;
			}
		}

		// if the filter doesn't have a variable length end (ends with a *) and there are characters remaining the filter fails
		return CurSegment == NumSegments && (bHasVariableEnd || FileNameIndex == FileName->Length);
	}

	bool Platform::FilterDirectory(String ^FullPath, String ^Filter)
	{
		// loop over the directories, but not the file, in the full path of the file
		array<String^> ^DirSegments = FullPath->Split(gcnew array<Char> { L'/', L'\\' }, StringSplitOptions::None);

		// go over each directory, but skip the filename (the last entry)
		for (INT DirIndex = 0; DirIndex < DirSegments->Length - 1; DirIndex++)
		{
			// if a directory in the path matches, return true
			if (FilterFileName(DirSegments[DirIndex], Filter))
			{
				return true;
			}
		}

		// if no matches were made, then return false
		return false;
	}

	void Platform::FixString(String ^%Str, TOCSettings ^BuildSettings, String ^Language)
	{
		if(Str != nullptr)
		{
			Str = Str->Replace("%GAME%", BuildSettings->GameName);
			Str = Str->Replace("%PLATFORM%", mType.ToString());
			Str = Str->Replace("%LANGUAGE%", Language);

			// do any extra var replacement
			if(BuildSettings->GenericVars != nullptr)
			{
				for each (KeyValuePair<String^, String^> Pair in BuildSettings->GenericVars)
				{
					String ^Var = String::Format(L"%{0}%",  Pair.Key->ToUpper());
					Str = Str->Replace(Var, Pair.Value);
				}
			}
		}
	}

	void Platform::TocMerge(List<TOCInfo^> ^CurrentTOC, List<TOCInfo^> ^OldTOC, DateTime TocLastWriteTime)
	{
		for each (TOCInfo ^CurrentEntry in CurrentTOC)
		{
			TOCInfo ^OldEntry = TocFindInfoFromConsoleName(OldTOC, CurrentEntry->FileName);
			
			if(OldEntry != nullptr)
			{
				if(DateTime::Compare(TocLastWriteTime, CurrentEntry->LastWriteTime) < 0)
				{
					continue;
				}

				if(CurrentEntry->Size == OldEntry->Size && CurrentEntry->CompressedSize == OldEntry->CompressedSize)
				{
					CurrentEntry->CRC = OldEntry->CRC;
				}
			}
		}
	}

	List<TOCInfo^>^ Platform::TocRead(String ^TOCPath)
	{
		List<TOCInfo^> ^TOC = gcnew List<TOCInfo^>();

		try
		{
			array<String^> ^Lines = File::ReadAllLines(TOCPath);

			for each(String ^CurLine in Lines)
			{
				if(CurLine->Length == 0)
				{
					continue;
				}

				array<String^> ^Words = CurLine->Split(gcnew array<Char> { L' ' }, StringSplitOptions::RemoveEmptyEntries);

				if(Words->Length < 3 )
				{
					continue;
				}

				int Size = Int32::Parse(Words[0]);
				int CompressedSize = Int32::Parse(Words[1]);
				String ^ConsoleFilename = Words[2];
				String ^CRC = (Words->Length > 3) ? Words[3] : L"0";

				TOC->Add(gcnew TOCInfo(ConsoleFilename, CRC, DateTime(0), Size, CompressedSize, nullptr));
			}

			return TOC;
		}
		catch(Exception^)
		{
			return gcnew List<TOCInfo^>();
		}
	}

	void Platform::TocGenerateCRC( List<TOCInfo^>^ TOC, String^ TOCFilename, TOCSettings^ BuildSettings )
	{
		BuildSettings->WriteLine( Color::Green, L"[GENERATING CRC STARTED]" );
		DateTime StartTime = DateTime::UtcNow;

		for each( TOCInfo^ Entry in TOC )
		{
			if( !Entry->bIsForTOC || !Entry->CRC->Equals( L"0" ) )
			{
				continue;
			}

			// Get the filename of the file to CRC
			String^ SourceFilename = Entry->FileName;
			if( BuildSettings->bInvertedCopy )
			{
				// "d:\Dest" + "UnrealEngine3"
				String^ SourceFolder = Path::Combine( BuildSettings->DestinationPaths[0], BuildSettings->TargetBaseDirectory );

				// Strip off the leading "../"
				SourceFilename = Path::Combine( SourceFolder, SourceFilename->Substring( 3 ) );
			}

			FileInfo ^FilePtr = gcnew FileInfo( SourceFilename );
			
			if( FilePtr->Name->Equals( TOCFilename ) )
			{
				// ignore the TOC file
				Entry->CRC = L"0";
			}
			else
			{
				BuildSettings->WriteLine( Color::Black, SourceFilename );

				if( FilePtr->Exists )
				{
					Entry->CRC = CreateCRC( FilePtr );
				}
				else
				{
					Entry->CRC = L"0";
				}

				BuildSettings->WriteLine( Color::Black, L"\t... {0}", Entry->CRC );
			}
		}

		TimeSpan Duration = DateTime::UtcNow.Subtract( StartTime );
		BuildSettings->WriteLine( Color::Green, L"Operation took {0}:{1}", Duration.Minutes.ToString(), Duration.Seconds.ToString( L"D2" ) );
		BuildSettings->WriteLine( Color::Green, L"[GENERATING CRC FINISHED]" );
	}

	/**
	 * Writes the table of contents to disk.
	 * 
	 * @param	TOC			Table of contents
	 * @param	TOCPath		Destination path for table of contents
	 */
	void Platform::TocWrite(List<TOCInfo^> ^TOC, String ^TOCPath)
	{
		try
		{
			// Make sure the file is not read only
			FileInfo ^Info = gcnew FileInfo(TOCPath);
			if(Info->Exists)
			{
				Info->Attributes = FileAttributes::Normal;
			}

			// delete it first because sometimes it appends, not overwrites, unlike the false says in new StreamWriter!!
			File::Delete(TOCPath);

			// Write out each element of the TOC file
			StreamWriter ^Writer = gcnew StreamWriter(TOCPath, false);
			for each(TOCInfo ^Entry in TOC)
			{
				// skip files that don't need to be in the TOC file
				if(!Entry->bIsForTOC)
				{
					continue;
				}

				if (Entry->CRC != nullptr)
				{
					Writer->WriteLine(L"{0} {1} {2} {3}", Entry->Size, Entry->CompressedSize, Entry->FileName, Entry->CRC);
				}
				else
				{
					Writer->WriteLine(L"{0} {1} {2} 0", Entry->Size, Entry->CompressedSize, Entry->FileName);
				}
			}

			Writer->Close();
			delete Writer;
		}
		catch(Exception ^e)
		{
			String ^ErrMsg = e->ToString();
			Console::WriteLine(ErrMsg);
			System::Diagnostics::Debug::WriteLine(ErrMsg);
		}
	}

	TOCInfo^ Platform::TocFindInfoFromConsoleName(List<TOCInfo^> ^TOC, String ^ConsoleName)
	{
		for each(TOCInfo ^Entry in TOC)
		{
			if(Entry->FileName->Equals(ConsoleName,StringComparison::OrdinalIgnoreCase))
			{
				return Entry;
			}
		}

		return nullptr;
	}

	bool Platform::TargetSync(TOCSettings ^BuildSettings, String ^TagSetName, SaveIsoDelegate^ SaveIso, SaveZipDelegate^ SaveZip)
	{
		bool bSuccess = true;

		List<List<TOCInfo^>^> ^TOCList = gcnew List<List<TOCInfo^>^>();


		for each(String ^CurLang in BuildSettings->Languages)
		{
			TOCList->Add(GenerateTOC(TagSetName, BuildSettings, CurLang));
		}

		if(BuildSettings->TargetsToSync->Count > 0)
		{
			if(!TargetSync(BuildSettings, TOCList))
			{
				bSuccess = false;
			}
		}

		if(BuildSettings->DestinationPaths->Count > 0)
		{
			if(!PcSync(TOCList, BuildSettings))
			{
				bSuccess = false;
			}
		}

		if(BuildSettings->ZipFiles->Count > 0 && SaveZip != nullptr)
		{
			if(!SaveZip(TOCList, BuildSettings))
			{
				bSuccess = false;
			}
		}

		if(BuildSettings->IsoFiles->Count > 0 && SaveIso != nullptr)
		{
			if(!SaveIso(TOCList, BuildSettings))
			{
				bSuccess = false;
			}
		}

		return bSuccess;
	}

	/**
	 * Syncs the console with the PC.
	 * 
	 * @return true if succcessful
	 */
	bool Platform::TargetSync(TOCSettings ^BuildSettings, List<List<TOCInfo^>^> ^TOCList)
	{
		if(!this->NeedsToSync || BuildSettings->NoSync)
		{
			return true;
		}

		bool bSuccess = true;
		Int64 NumBytesPublished = 0;

		List<PlatformTarget^> ^Targets = gcnew List<PlatformTarget^>();
		List<PlatformTarget^> ^TargetsToDisconnect = gcnew List<PlatformTarget^>();
		List<String^> ^TargetsToSync = gcnew List<String^>(BuildSettings->TargetsToSync);

		BuildSettings->WriteLine(Color::Black, L"\r\nEnumerating targets...");

		for each(PlatformTarget ^CurTarget in mTargets->Values)
		{
			if(TargetsToSync->Count == 0)
			{
				break;
			}

			String ^TMName = CurTarget->TargetManagerName;
			String ^Name = CurTarget->Name;
			IPAddress ^DebugIP = CurTarget->DebugIPAddress;
			IPAddress ^TitleIP = CurTarget->IPAddress;

			for(int i = 0; i < TargetsToSync->Count; ++i)
			{
				String ^CurSyncTarget = TargetsToSync[i];
				IPAddress ^SyncIP = nullptr;

				if(TMName->Equals(CurSyncTarget, StringComparison::OrdinalIgnoreCase) || Name->Equals(CurSyncTarget, StringComparison::OrdinalIgnoreCase))
				{
					Targets->Add(CurTarget);
					TargetsToSync->RemoveAt(i);
					--i;
				}
				else if(IPAddress::TryParse(CurSyncTarget, SyncIP))
				{
					if(DebugIP->Equals(SyncIP) || TitleIP->Equals(SyncIP))
					{
						Targets->Add(CurTarget);
						TargetsToSync->RemoveAt(i);
						--i;
					}
				}
			}
		}

		if(TargetsToSync->Count > 0)
		{			
			bSuccess = false;

			for each(String ^CurTarget in TargetsToSync)
			{
				BuildSettings->WriteLine(Color::Red, L"Target \'{0}\' either does not exist or is currently unavailable!", CurTarget);
			}
		}

		WORD Subsystem = GetApplicationSubsystem();
		IWin32Window ^ParentWindow = nullptr;

		if(Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
		{
			// There's high probability if the current application is a console app then it's CookerSync
			// lets see if the parent application is UnrealFrontend because if it is then we'll want to use its
			// main window handle instead of the console application window handle as that will be hidden
			HANDLE SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

			if(SnapshotHandle != NULL)
			{
				PROCESSENTRY32 ProcInfo;
				ProcInfo.dwSize = sizeof(PROCESSENTRY32);

				try
				{
					if(Process32First(SnapshotHandle, &ProcInfo))
					{
						Process ^CurProcess = Process::GetCurrentProcess();
						Process ^ParentProcess = nullptr;

						do 
						{
							if(ProcInfo.th32ProcessID == ( DWORD )CurProcess->Id)
							{
								ParentProcess = Process::GetProcessById(ProcInfo.th32ParentProcessID);
								
								if(ParentProcess->MainModule->ModuleName->Equals(L"UnrealFrontend.exe", StringComparison::OrdinalIgnoreCase))
								{
									ParentWindow = gcnew HwndWrapper(ParentProcess->MainWindowHandle);
								}

								delete ParentProcess;

								break;
							}
						} while(Process32Next(SnapshotHandle, &ProcInfo));

						delete CurProcess;
					}
				}
				catch(Exception ^ex)
				{
					System::Diagnostics::Debug::WriteLine(ex->ToString());
				}

				CloseHandle(SnapshotHandle);
			}

			// Either the parent application isn't UnrealFrontend or we failed to get a valid parent window from UnrealFrontend
			// default to the console window handle
			if(ParentWindow == nullptr)
			{
				ParentWindow = gcnew HwndWrapper(GetConsoleWindow());
			}
		}
		else if(Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
		{
			try
			{
				Process ^CurProcess = Process::GetCurrentProcess();
				ParentWindow = gcnew HwndWrapper(CurProcess->MainWindowHandle);
				delete CurProcess;
			}
			catch(Exception ^ex)
			{
				System::Diagnostics::Debug::WriteLine(ex->ToString());
			}
		}

		ConnectToConsoleRetryForm ^RetryForm = gcnew ConnectToConsoleRetryForm();
		RetryForm->Text = L"CookerSync: Target Connection Error";

		for(int i = 0; i < Targets->Count; ++i)
		{
			PlatformTarget ^CurTarget = Targets[i];

			bool bConnected = true;

			if(!CurTarget->IsConnected)
			{
				RetryForm->Message = String::Format(L"Could not connect to target \'{0}\'!", CurTarget->TargetManagerName);

				do 
				{
					bConnected = CurTarget->Connect();

					if(!bConnected)
					{
						if(BuildSettings->bNonInteractive)
						{
							BuildSettings->WriteLine(Color::Orange, L"Target \'{0}\' skipped!", CurTarget->TargetManagerName);
							bSuccess = false;

							Targets->RemoveAt(i);
							--i;

							break;
						}
						else
						{
							System::Media::SystemSounds::Exclamation->Play();

							if(RetryForm->ShowDialog(ParentWindow) == System::Windows::Forms::DialogResult::Cancel)
							{
								BuildSettings->WriteLine(Color::Orange, L"Target \'{0}\' skipped!", CurTarget->TargetManagerName);
								bSuccess = false;
								
								Targets->RemoveAt(i);
								--i;

								break;
							}
						}
					}
					else
					{
						TargetsToDisconnect->Add(CurTarget);
					}
				} while(!bConnected);
			}

			if(bConnected)
			{
				CurTarget->InitializeSyncing();
			}
		}

		delete RetryForm;

		if(Targets->Count == 0)
		{
			BuildSettings->WriteLine(Color::Red, L"List of targets to sync does not contain any valid targets. Ending sync operation.");
			return false;
		}

		DateTime StartTime = DateTime::UtcNow;

		try
		{
			// Should only be Xenon that gets this far
			if(BuildSettings->bRebootBeforeCopy)
			{
				BuildSettings->WriteLine(Color::Green, L"\r\n[REBOOTING TARGETS]");

				for each(PlatformTarget ^CurTarget in Targets)
				{
					// Grab name before reboot because you can't retrieve it in certain reboot stages
					String ^TargetName = CurTarget->Name;

					if(!CurTarget->Reboot())
					{
						BuildSettings->WriteLine(Color::Red, L"Target \'{0}\' failed to reboot!", TargetName);
					}
					else
					{
						BuildSettings->WriteLine(Color::Black, "Target \'{0}\' has been rebooted.", TargetName);
					}
				}

				MSG Msg;
				bool bAllTargetsRebooted = true;
				int Ticks = Environment::TickCount;
				const int REBOOT_TIMEOUT = 60000;

				// need to wait for xenon's to finish rebooting before we start copying files
				// otherwise bad things happen
				do
				{
					bAllTargetsRebooted = true;

					// pump COM and UI messages
					if(PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE))
					{
						TranslateMessage(&Msg);
						DispatchMessage(&Msg);
					}

					for(int i = 0; i < Targets->Count; ++i)
					{
						if(Targets[i]->State == TargetState::Rebooting)
						{
							bAllTargetsRebooted = false;
							break;
						}
					}
				} while(!bAllTargetsRebooted && Environment::TickCount - Ticks < REBOOT_TIMEOUT);

				if(Environment::TickCount - Ticks >= REBOOT_TIMEOUT)
				{
					BuildSettings->WriteLine(Color::Orange, L"Warning: Waiting for all targets to finish rebooting has timed out. This is probably the result of corrupted state for one or more of the targets.");
				}
			}

			// start copying files
			BuildSettings->WriteLine(Color::Green, L"\r\n[SYNCING WITH TARGETS STARTED]");

			VALIDATE_SUPPORT(mConsoleSupport);

			std::vector<TARGETHANDLE> NativeTargets;
			std::vector<const wchar_t*> NativeSrcFiles;
			std::vector<const wchar_t*> NativeDestFiles;
			std::vector<const wchar_t*> NativeDirectories;
			Dictionary<String^, String^> ^DirectoriesToMake = gcnew Dictionary<String^, String^>();
			Dictionary<String^, String^> ^FilesToCopy = gcnew Dictionary<String^, String^>();
			pin_ptr<const wchar_t> PinTemp;
			wchar_t* Temp = NULL;

			for each(PlatformTarget ^CurTarget in Targets)
			{
				NativeTargets.push_back(CurTarget->Handle);
			}

			// Check for Android Expansion Mode
			bool SkipTOC = false;
			String^ ExpansionFile;
			if(BuildSettings->GenericVars->TryGetValue("AndroidExpansion",ExpansionFile))
			{
				// Don't load anything from TOC if we have an OBB to sync instead
				SkipTOC = true;
				String^ FileName = "..\\Binaries\\Android\\assets\\" + ExpansionFile;

				String^ FullFileName = FileName;
				if( FullFileName->StartsWith( "..\\" ) )
				{
					FullFileName = FullFileName->Substring( 3 );
				}

				String^ ExpansionDir = "";
				BuildSettings->GenericVars->TryGetValue("AndroidExpansionDirectory",ExpansionDir);

				String^ ExpansionFileDestination = "";
				if (!BuildSettings->GenericVars->TryGetValue("AndroidExpansionDestination",ExpansionFileDestination))
				{
					ExpansionFileDestination = ExpansionFile;
				}

				String^ DestPath = Path::Combine(BuildSettings->TargetBaseDirectory + ExpansionDir, ExpansionFileDestination);
				String^ DestDir = Path::GetDirectoryName(DestPath);

				// Skip OBB sync if Amazon Distribution
				if (ExpansionFile != "AMAZON")
				{
					BuildSettings->WriteLine(Color::Black, L"Syncing OBB file, this may take several minutes for large projects");

					if(!FilesToCopy->ContainsKey(DestPath))
					{
						PinTemp = PtrToStringChars(FileName);
						Temp = new wchar_t[FileName->Length + 1];
						wcscpy_s(Temp, FileName->Length + 1, PinTemp);
						NativeSrcFiles.push_back(Temp);

						PinTemp = PtrToStringChars(DestPath);
						Temp = new wchar_t[DestPath->Length + 1];
						wcscpy_s(Temp, DestPath->Length + 1, PinTemp);
						NativeDestFiles.push_back(Temp);

						FilesToCopy[DestPath] = DestPath;
					}

					if(!DirectoriesToMake->ContainsKey(DestDir))
					{
						PinTemp = PtrToStringChars(DestDir);
						Temp = new wchar_t[DestDir->Length + 1];
						wcscpy_s(Temp, DestDir->Length + 1, PinTemp);
						NativeDirectories.push_back(Temp);

						DirectoriesToMake[DestDir] = DestDir;
					}
				}

				// Also add Command Line so that -install can be triggered
				FileName = "..\\UDKGame\\CookedAndroid\\UE3CommandLine.txt";

				FullFileName = FileName;
				if( FullFileName->StartsWith( "..\\" ) )
				{
					FullFileName = FullFileName->Substring( 3 );
				}

				DestPath = Path::Combine(BuildSettings->TargetBaseDirectory, FullFileName);
				DestDir = Path::GetDirectoryName(DestPath);

				if(!FilesToCopy->ContainsKey(DestPath))
				{
					PinTemp = PtrToStringChars(FileName);
					Temp = new wchar_t[FileName->Length + 1];
					wcscpy_s(Temp, FileName->Length + 1, PinTemp);
					NativeSrcFiles.push_back(Temp);

					PinTemp = PtrToStringChars(DestPath);
					Temp = new wchar_t[DestPath->Length + 1];
					wcscpy_s(Temp, DestPath->Length + 1, PinTemp);
					NativeDestFiles.push_back(Temp);

					FilesToCopy[DestPath] = DestPath;
				}

				if(!DirectoriesToMake->ContainsKey(DestDir))
				{
					PinTemp = PtrToStringChars(DestDir);
					Temp = new wchar_t[DestDir->Length + 1];
					wcscpy_s(Temp, DestDir->Length + 1, PinTemp);
					NativeDirectories.push_back(Temp);

					DirectoriesToMake[DestDir] = DestDir;
				}
			}
			
			if (!SkipTOC)
			{
				for each(List<TOCInfo^> ^TOC in TOCList)
				{
					for each(TOCInfo ^Entry in TOC)
					{
						if (Entry->bIsForSync)
						{
							String^ FullFileName = Entry->FileName;
							if( FullFileName->StartsWith( "..\\" ) )
							{
								FullFileName = FullFileName->Substring( 3 );
							}
				
							// If we're deploying, move the file to the root
							if( Entry->bDeploy )
							{
								FullFileName = Path::GetFileName( FullFileName );
							}

							String^ DestPath = Path::Combine(BuildSettings->TargetBaseDirectory, FullFileName);
							String^ DestDir = Path::GetDirectoryName(DestPath);

							if(!FilesToCopy->ContainsKey(DestPath))
							{
								PinTemp = PtrToStringChars(Entry->FileName);
								Temp = new wchar_t[Entry->FileName->Length + 1];
								wcscpy_s(Temp, Entry->FileName->Length + 1, PinTemp);
								NativeSrcFiles.push_back(Temp);

								PinTemp = PtrToStringChars(DestPath);
								Temp = new wchar_t[DestPath->Length + 1];
								wcscpy_s(Temp, DestPath->Length + 1, PinTemp);
								NativeDestFiles.push_back(Temp);

								FilesToCopy[DestPath] = DestPath;
							}

							if(!DirectoriesToMake->ContainsKey(DestDir))
							{
								PinTemp = PtrToStringChars(DestDir);
								Temp = new wchar_t[DestDir->Length + 1];
								wcscpy_s(Temp, DestDir->Length + 1, PinTemp);
								NativeDirectories.push_back(Temp);

								DirectoriesToMake[DestDir] = DestDir;
							}

							NumBytesPublished += Entry->Size;
						}
					}
				}
			}

			try
			{
				// NOTE: We need to hold on to this for the duration of SyncFiles() so that the native to managed thunk (__stdcall to __clrcall) doesn't get collected
				WriteLineNativeDelegate ^TTYDelegate = gcnew WriteLineNativeDelegate(BuildSettings, &TOCSettings::WriteLine);

				ColoredTTYEventCallbackPtr TTYCallback = (ColoredTTYEventCallbackPtr)Marshal::GetFunctionPointerForDelegate(TTYDelegate).ToPointer();

				bool bSyncResult = mConsoleSupport->SyncFiles(&NativeTargets[0], (int)NativeTargets.size(), &NativeSrcFiles[0], &NativeDestFiles[0], (int)NativeSrcFiles.size(), &NativeDirectories[0], (int)NativeDirectories.size(), TTYCallback);

				bSuccess = bSuccess && bSyncResult;
			}
			finally
			{
				for(size_t i = 0; i < NativeSrcFiles.size(); ++i)
				{
					delete [] NativeSrcFiles[i];
					delete [] NativeDestFiles[i];
				}

				for(size_t i = 0; i < NativeDirectories.size(); ++i)
				{
					delete [] NativeDirectories[i];
				}
			}
		}
		finally
		{
			for each(PlatformTarget ^CurTarget in TargetsToDisconnect)
			{
				CurTarget->Disconnect();
			}
		}

		Int64 BytesPublished = NumBytesPublished * TargetsToDisconnect->Count;
		double GBPublished = BytesPublished / ( 1024.0 * 1024.0 * 1024.0 );
		TimeSpan Duration = DateTime::UtcNow.Subtract(StartTime);
		BuildSettings->WriteLine(Color::Black, L"\r\nOperation synced {0} GB taking {1}:{2}", GBPublished.ToString(L"0.00"), Duration.Minutes.ToString(), Duration.Seconds.ToString(L"D2"));

		if( BuildSettings->VerboseOutput )
		{
			BuildSettings->WriteLine(Color::Black, L"[PERFCOUNTER] {0}XboxSyncBytes {1}", BuildSettings->GameName, BytesPublished.ToString() );
		}

		BuildSettings->WriteLine(Color::Green, L"[SYNCING WITH TARGETS FINISHED]");

		return bSuccess;
	}

	/**
	 * Initializes the target paths.
	 */
	bool Platform::PcInit(TOCSettings^ BuildSettings, List<String^>^ TargetPaths)
	{
		// used to indicate potential existence
		bool bWouldExist = false;
		TargetPaths->Clear();

		// Get each console that was checked
		for each(String^ Path in BuildSettings->DestinationPaths)
		{
			// Make sure all the folders exist
			DirectoryInfo^ Info = gcnew DirectoryInfo(Path);
			if (!Info->Exists)
			{
				if(BuildSettings->NoSync)
				{
					BuildSettings->WriteLine(Color::Black, L"Would create directory: \'{0}\'", Path);
					bWouldExist = true;
				}
				else
				{
					BuildSettings->WriteLine(Color::Black, L"Creating directory: \'{0}\'", Path);

					try
					{
						Info->Create();
					}
					catch(Exception ^e)
					{
						BuildSettings->WriteLine(Color::Red, String::Format(L"Error: {0}", e->Message));
					}

					Info->Refresh();
				}
			}

			if(Info->Exists || bWouldExist)
			{
				String ^TargetPath = Path;
				if(!TargetPath->EndsWith(L"\\"))
				{
					TargetPath = String::Concat(TargetPath, L"\\");
				}

				TargetPaths->Add(TargetPath);
			}
		}

		// this was successful if we found at least one target we can use
		if(TargetPaths->Count > 0)
		{
			return true;
		}

		BuildSettings->WriteLine(Color::Red, "Error: Failed to find any valid target paths.");
		return false;
	}

	/**
	 * Syncs the build to a PC directory (for any platform)
	 * 
	 * @return true if succcessful
	 */
	bool Platform::PcSync(List<List<TOCInfo^>^>^ TOCList, TOCSettings^ BuildSettings)
	{
		bool bSuccess = true;

		List<String^> ^TargetPaths = gcnew List<String^>();

		// Initialize the target paths.
		if (PcInit(BuildSettings, TargetPaths) == false)
		{
			return false;
		}

		BuildSettings->WriteLine(Color::Green, L"[SYNCING WITH FOLDERS STARTED]");
		DateTime StartTime = DateTime::UtcNow;
		Int64 NumBytesPublished = 0;

		// Copy all the files marked as syncable
		bSuccess &= PcCopyFiles( BuildSettings, TargetPaths, TOCList, true, NumBytesPublished );
		// Copy all the support files
		bSuccess &= PcCopyFiles( BuildSettings, TargetPaths, TOCList, false, NumBytesPublished );

		Int64 BytesPublished = NumBytesPublished * TargetPaths->Count;
		double GBPublished = BytesPublished / ( 1024.0 * 1024.0 * 1024.0 );
		TimeSpan Duration = DateTime::UtcNow.Subtract(StartTime);
		BuildSettings->WriteLine(Color::Black, L"\r\nOperation synced {0} GB taking {1}:{2}", GBPublished.ToString(L"0.00"), Duration.Minutes.ToString(), Duration.Seconds.ToString(L"D2"));

		if( BuildSettings->VerboseOutput )
		{
			BuildSettings->WriteLine(Color::Black, L"[PERFCOUNTER] {0}PCSyncBytes {1}", BuildSettings->GameName, BytesPublished.ToString() );
		}

		BuildSettings->WriteLine(Color::Green, L"[SYNCING WITH FOLDERS FINISHED]");

		return bSuccess;
	}

	bool Platform::PcCopyFiles( TOCSettings^ BuildSettings, List<String^> ^TargetPaths, List<List<TOCInfo^>^>^ TOCList, bool bDoSyncableFiles, Int64% NumBytesPublished )
	{
		Dictionary<String^, String^> ^FilesCopied = gcnew Dictionary<String^, String^>();
		List<String^> ^CRCMismatchedFiles = gcnew List<String^>();

		// Do the action on each path
		for each(String^ CurPath in TargetPaths)
		{
			for each(List<TOCInfo^> ^TOC in TOCList)
			{
				// Copy each file from the table of contents
				for each (TOCInfo ^Entry in TOC)
				{
					if( ( Entry->bIsForSync & !Entry->bIsTOC ) == bDoSyncableFiles )
					{
						String^ FullFileName = Entry->FileName;
						if( FullFileName->StartsWith( "..\\" ) )
						{
							FullFileName = FullFileName->Substring( 3 );
						}
						String^ DestFileName = Path::Combine( CurPath, Path::Combine( BuildSettings->TargetBaseDirectory, FullFileName ) );

						if(!FilesCopied->ContainsKey(DestFileName))
						{
							FilesCopied[DestFileName] = DestFileName;

							if( BuildSettings->bInvertedCopy )
							{
								if(!PcCopyFile(BuildSettings, CRCMismatchedFiles, DestFileName, Entry->FileName, Entry->CRC))
								{
									return false;
								}
							}
							else
							{
								if(!PcCopyFile(BuildSettings, CRCMismatchedFiles, Entry->FileName, DestFileName, Entry->CRC))
								{
									return false;
								}
							}
						}

						NumBytesPublished += Entry->Size;
					}
				}
			}
		}

		if(CRCMismatchedFiles->Count > 0)
		{
			BuildSettings->WriteLine(Color::Red, L"[CRC MISMATCHED FILES]");

			for each(String ^File in CRCMismatchedFiles)
			{
				BuildSettings->WriteLine(Color::Red, File);
			}
		}

		return( CRCMismatchedFiles->Count == 0 );
	}

	bool Platform::PcCopyFile( TOCSettings^ BuildSettings, List<String^>^ CRCMismatchedFiles, String^ SourcePath, String^ DestPath, String^ SrcCRC )
	{
		bool bCopySucceeded = true;
		const int MaxCopyRetries = 5;

		FileInfo^ SrcFile = gcnew FileInfo( SourcePath );
		FileInfo^ DstFile = gcnew FileInfo( DestPath );

		// Check to see if the file is out of date in some way, and needs copying
		if( PcRequireCopy( BuildSettings, SrcFile, DstFile ) )
		{
			if( !BuildSettings->NoSync )
			{
				bool bCreatedDestinationFolder = false;
				bool bWroteToDestination = false;

				BuildSettings->WriteLine( Color::Black, L"Copying {0} to {1}", SourcePath, DestPath );

				// Create the folder if necessary
				int CreateRetries = 0;

				while( !bCreatedDestinationFolder && CreateRetries < MaxCopyRetries )
				{
					try
					{
						FileInfo^ DstFile = gcnew FileInfo( DestPath );

						if( !DstFile->Directory->Exists )
						{
							BuildSettings->WriteLine( Color::Black, L" ... Creating folder " + DstFile->Directory->FullName );
							DstFile->Directory->Create();
							DstFile->Directory->Refresh();
						}

						bCreatedDestinationFolder = true;
					}
					catch( Exception^ E )
					{
						bCreatedDestinationFolder = false;
						CreateRetries++;

						String^ ErrorDetail = L"Unspecified";

						try
						{
							if( E->Message != nullptr )
							{
								ErrorDetail = E->Message;
							}
						}
						catch( Exception^ )
						{
						}

						BuildSettings->WriteLine( Color::Orange, L"=> NETWORK WRITE ERROR: {0}", ErrorDetail );

						// Pause for a long time before retrying
						System::Threading::Thread::Sleep( 30 * 1000 );
					}
				}

				// Copy the file - and delete it if it already exists (which shouldn't be possible)
				if( bCreatedDestinationFolder )
				{
					int CopyRetries = 0;

					while( !bWroteToDestination && CopyRetries < MaxCopyRetries )
					{
						try
						{
							FileInfo^ DstFile = gcnew FileInfo( DestPath );

							if( DstFile->Exists )
							{
								DstFile->IsReadOnly = false;
								DstFile->Delete();
								DstFile->Refresh();
							}

							SrcFile->CopyTo( DestPath, true );

							bWroteToDestination = true;
						}
						catch( Exception^ E )
						{
							bCopySucceeded = false;
							CopyRetries++;

							String^ ErrorDetail = L"Unspecified";
						
							try
							{
								if( E->Message != nullptr )
								{
									ErrorDetail = E->Message;
								}
							}
							catch( Exception^ )
							{
							}

							BuildSettings->WriteLine( Color::Orange, L"=> NETWORK WRITE ERROR: {0}", ErrorDetail );

							// Pause for a long time before retrying
							System::Threading::Thread::Sleep( 30 * 1000 );
						}
					}
				}

				if( !bWroteToDestination || !bCreatedDestinationFolder )
				{
					BuildSettings->WriteLine( Color::Red, L"==> NETWORK WRITE ERROR: Failed to copy \'{0}\' after {1} retries 60 seconds apart.", SrcFile->Name, MaxCopyRetries.ToString() );
				}

				System::Threading::Thread::Sleep( BuildSettings->SleepDelay );
			}
			else
			{
				if( !BuildSettings->VerifyCopy )
				{
					BuildSettings->WriteLine( Color::Black, L"Would copy {0} to {1}", SourcePath, DestPath );
				}
				else
				{
					BuildSettings->WriteLine( Color::Black, L"Verifying {0}", DestPath );
				}
			}

			if( BuildSettings->VerifyCopy && bCopySucceeded )
			{
				DstFile->Refresh();
				bool bFoundFile = false;
				int ReadRetries = 0;

				while( !bFoundFile && ReadRetries < MaxCopyRetries )
				{
					if( DstFile->Exists )
					{
						if( !SrcCRC->Equals( L"0" ) )
						{
							String^ DstCRC = CreateCRC( DstFile );

							if( !SrcCRC->Equals( DstCRC ) )
							{
								BuildSettings->WriteLine( Color::Red, L"==> ERROR: CRC Mismatch" );
								CRCMismatchedFiles->Add( String::Format( L"Error: CRC Mismatch for \'{0}\'", DestPath ) );
							}
						}
						else
						{
							BuildSettings->WriteLine( Color::Red, L"Note: No CRC available for \'{0}\'", DestPath );
						}

						bFoundFile = true;
					}
					else
					{
						ReadRetries++;

						if( BuildSettings->NoSync )
						{
							BuildSettings->WriteLine( Color::Orange, L"=> NETWORK READ ERROR\r\n" );
						}
					}
				}

				if( !bFoundFile )
				{
					CRCMismatchedFiles->Add( String::Format( L"Error: Missing file \'{0}\'", DestPath ) );
					BuildSettings->WriteLine( Color::Red, L"==> NETWORK READ ERROR: Failed to read \'{0}\' after {1} retries 60 seconds apart.", DstFile->Name, ReadRetries.ToString() );
				}

				System::Threading::Thread::Sleep( BuildSettings->SleepDelay );
			}
		}

		return bCopySucceeded;
	}

	bool Platform::PcRequireCopy(TOCSettings ^BuildSettings, FileInfo ^SrcFile, FileInfo ^DstFile)
	{
		if(!SrcFile->Exists)
		{
			if( BuildSettings->VerboseOutput )
			{
				BuildSettings->WriteLine( Color::Green, L"Source file: '" + SrcFile->ToString() + L"' does not exist (do not copy)" );
			}
			return false;
		}

		if(BuildSettings->Force)
		{
			if( BuildSettings->VerboseOutput )
			{
				BuildSettings->WriteLine( Color::Green, L"'" + DstFile->ToString() + "' force copying (copy)" );
			}
			return true;
		}

		if(!DstFile->Exists)
		{
			if( BuildSettings->VerboseOutput )
			{
				BuildSettings->WriteLine( Color::Green, L"Dest file: '" + DstFile->ToString() + L" does not exist (copy)" );
			}
			return true;
		}

		// compare the lengths
		if(DstFile->Length != SrcFile->Length)
		{
			if( BuildSettings->VerboseOutput )
			{
				BuildSettings->WriteLine( Color::Green, "'" + DstFile->ToString() + L"' is a different size (copy)" );
			}
			return true;
		}

		// compare the time stamps
		if(DateTime::Compare(DstFile->LastWriteTimeUtc, SrcFile->LastWriteTimeUtc) < 0)
		{
			if( BuildSettings->VerboseOutput )
			{
				BuildSettings->WriteLine( Color::Green, "'" + DstFile->ToString() + L"' is older (copy)" );
			}
			return true;
		}

		if( BuildSettings->VerboseOutput )
		{
			BuildSettings->WriteLine( Color::Green, L"'" + DstFile->ToString() + L"' == '" + SrcFile->ToString() + L"' (do not copy)" );
		}

		return false;
	}

	String^ Platform::CreateCRC( FileInfo ^SrcFile )
	{
		MD5CryptoServiceProvider^ Hasher = gcnew MD5CryptoServiceProvider();

		FileStream ^Stream = SrcFile->Open( FileMode::Open, FileAccess::Read, FileShare::Read );

		array<Byte> ^HashData = Hasher->ComputeHash( Stream );

		Stream->Close();
		delete Stream;

		StringBuilder ^HashCodeBuilder = gcnew StringBuilder( HashData->Length * 2 );

		for( int Index = 0; Index < HashData->Length; ++Index )
		{
			HashCodeBuilder->Append( HashData[Index].ToString( L"x2" ) );
		}

		return( HashCodeBuilder->ToString() );
	}

	bool Platform::TargetSync(TOCSettings ^BuildSettings)
	{
		return TargetSync(BuildSettings, (String^)nullptr, nullptr, nullptr);
	}

	WORD Platform::GetApplicationSubsystem()
	{
		pin_ptr<const wchar_t> AsmPath = PtrToStringChars(Assembly::GetEntryAssembly()->Location);

		FILE *FilePtr = _wfopen(AsmPath, L"rb");

		if(!FilePtr)
		{
			return 0;
		}

		IMAGE_DOS_HEADER DosHeader;

		fread(&DosHeader, sizeof(DosHeader), 1, FilePtr);

		if(DosHeader.e_magic != IMAGE_DOS_SIGNATURE)
		{
			return 0;
		}

		fseek(FilePtr, DosHeader.e_lfanew, SEEK_SET);

		ULONG NtSignature;
		fread(&NtSignature, sizeof(NtSignature), 1, FilePtr);

		if(NtSignature != IMAGE_NT_SIGNATURE)
		{
			return 0;
		}

		IMAGE_FILE_HEADER FileHeader;
		fread(&FileHeader, sizeof(FileHeader), 1, FilePtr);

		IMAGE_OPTIONAL_HEADER OptionalHeader;
		fread(&OptionalHeader, sizeof(OptionalHeader), 1, FilePtr);

		fclose(FilePtr);

		return OptionalHeader.Subsystem;
	}

	/**
	 * Create a PlatformTarget for a given TargetHandle
	 */
	void Platform::AddTarget( TARGETHANDLE Handle )
	{
		String ^TMName = gcnew String( mConsoleSupport->GetUnresolvedTargetName( Handle ) );
		int TargetPlatformType = ( int )mConsoleSupport->GetPlatformType();
		if( TMName->Length > 0 && TargetPlatformType == ( int )Type && !mTargets->ContainsKey( TMName ) )
		{
			mTargets[TMName] = gcnew PlatformTarget( TargetHandle( Handle ), this );
		}
	}

	/**
	 * Attempt to find targets using the console tools dll
	 */
	int Platform::EnumerateAvailableTargets()
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		++mTargetEnumerationCount;
		int NumTargets = 0;
		TARGETHANDLE *TargList = NULL;

		// refresh the target list on android
		if (Type == PlatformType::Android)
		{
			delete mTargets;
			mTargets = gcnew Dictionary<String^, PlatformTarget^>();
		}
		
		try
		{
			// Get a list of handles to the targets
			mConsoleSupport->EnumerateAvailableTargets();

			NumTargets = mConsoleSupport->GetTargets(NULL);
			TargList = new TARGETHANDLE[NumTargets];
			mConsoleSupport->GetTargets(TargList);

			// Extract the name and create a new PlatformTarget class for each target
			for( int i = 0; i < NumTargets; ++i )
			{
				AddTarget( TargList[i] );
			}
		}
		catch( Exception^ Ex )
		{
			String^ Error = Ex->ToString();
			Debug::WriteLine( Error );
		}
		finally
		{
			if( TargList )
			{
				delete [] TargList;
			}
		}

		return NumTargets;
	}

	/**
	 * Adds a stub target for UnrealConsole to use
	 */
	PlatformTarget^ Platform::ForceAddTarget( String^ TargetAddress )
	{
		VALIDATE_SUPPORT(mConsoleSupport);

		pin_ptr<const wchar_t> PinTemp = PtrToStringChars( TargetAddress );
		wchar_t* Temp = new wchar_t[TargetAddress->Length + 1];
		wcscpy_s( Temp, TargetAddress->Length + 1, PinTemp );

		TARGETHANDLE Handle = mConsoleSupport->ForceAddTarget( Temp );
	
		delete Temp;

		AddTarget( Handle );

		return( mTargets[TargetAddress] );
	}
}
