/*================================================================================
	InteropCLR.cpp: Code for interfacing C++ with C++/CLI and WPF
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEdCLR.h"

#include "InteropShared.h"
#include "ManagedCodeSupportCLR.h"

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Windows::Markup;


namespace InteropTools
{
	/**
	 * A callback function to resolve assembly locations
	 */
	Assembly^ CurrentDomain_AssemblyResolve( Object^, ResolveEventArgs^ AssemblyDetails )
	{
		String^ FullPath = nullptr;

		// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
		array<String^> ^AssemblyInfo = AssemblyDetails->Name->Split( gcnew array<Char> { ',' } );

		// If we don't know the requesting assembly, presume it's the game trying to load a .NET assembly from the parent folder
		if( AssemblyDetails->RequestingAssembly == nullptr )
		{
			// Look in the binaries folder in the parent of the Windows folder
			FullPath = IO::Path::GetFullPath( IO::Path::Combine( Environment::CurrentDirectory, "..\\" + AssemblyInfo[0] + ".dll" ) );
			if( File::Exists( FullPath ) )
			{
				Assembly^ ResolvedAssembly = Assembly::LoadFile( FullPath );
				return ResolvedAssembly;
			}
		}
		else
		{
			// If we do know the requesting assembly, follow the convention that the bit specific assembly is in the Win32 or Win64 subfolder
			String^ AssemblyPath = IO::Path::GetDirectoryName(AssemblyDetails->RequestingAssembly->Location);

			if( Environment::Is64BitProcess )
			{
				FullPath = IO::Path::GetFullPath( IO::Path::Combine( AssemblyPath, "Win64\\" + AssemblyInfo[0] + ".dll" ) );
			}
			else
			{
				FullPath = IO::Path::GetFullPath( IO::Path::Combine( AssemblyPath, "Win32\\" + AssemblyInfo[0] + ".dll" ) );
			}

			if( File::Exists( FullPath ) )
			{
				Assembly^ ResolvedAssembly = Assembly::LoadFile( FullPath );
				return ResolvedAssembly;
			}
		}

		return nullptr;
	}

	/**
	 * Register a callback function to resolve assembly locations
	 */
	void AddResolveHandler()
	{
		// Add assembly resolution so we can find the UnrealEdCSharp dll
		AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler( CurrentDomain_AssemblyResolve );
	}

	/**
	 * Unreal editor backend interface.  This allows UnrealEdCSharp to call native C++ and C++/CLI
	 * editor functions.
	 */
	ref class MEditorBackend : UnrealEd::IEditorBackendInterface
	{
	public:
		/** Writes the specified text to the warning log */
		virtual void LogWarningMessage( String^ Text )
		{
			CLRTools::LogWarningMessage( Text );
		}
	};

	/** Initializes the backend interface for the the editor's companion module: UnrealEdCSharp */
	void InitUnrealEdCSharpBackend()
	{
		MEditorBackend^ EditorBackend = gcnew MEditorBackend();

		// NOTE: If this throws an exception then it's likely that UnrealEdCSharp.dll could not
		//       be loaded for some reason
		UnrealEd::Backend::InitBackend( EditorBackend );
	}


	/** Loads any resource dictionaries needed by our WPF controls (localized text, etc.) */
	void LoadResourceDictionaries()
	{
		// OK, first make sure that an Application singleton is allocated.  Because we're hosting WPF
		// controls in a Win32 app, there may not be an Application yet.  WPF will always search the
		// Application's resource dictionaries if it can't find a resource inside of the control itself.
		if( Application::Current == nullptr )
		{
			Application^ MyApp = gcnew Application();
		}


		// Grab the executable path
		TCHAR ApplicationPathChars[ MAX_PATH ];
		GetModuleFileName( NULL, ApplicationPathChars, MAX_PATH - 1 );
		FString ApplicationPath( ApplicationPathChars );



		// Setup parser context.  This is needed so that files that are referenced from within the .xaml file
		// (such as other .xaml files or images) can be located with paths relative to the application folder.
		ParserContext^ MyParserContext = gcnew ParserContext();
		{
			// Create and assign the base URI for the parser context
			Uri^ BaseUri = Packaging::PackUriHelper::Create( gcnew Uri( CLRTools::ToString( ApplicationPath ) ) );
			MyParserContext->BaseUri = BaseUri;
		}




		// Load up localized (string) resources first
		{
			const FString WPFResourcePath = FString::Printf( TEXT( "%sWPF\\Localized\\" ), *GetEditorResourcesDir() );

			// First look for "INT" files.  We'll always fall back on that if we can't find a file for
			// the editor's currently active language
			const TCHAR* DefaultLanguageName = TEXT( "INT" );
			FString DefaultLanguageFileNameSuffix = FString::Printf( TEXT( ".%s.xaml" ), DefaultLanguageName );
			TArray< FString > DefaultLanguageResourceFileNames;
			GFileManager->FindFiles(
				DefaultLanguageResourceFileNames,
				*FString::Printf( TEXT( "%s*%s" ), *WPFResourcePath, *DefaultLanguageFileNameSuffix ),
				TRUE,
				FALSE );

			for( INT CurResourceFileIndex = 0; CurResourceFileIndex < DefaultLanguageResourceFileNames.Num(); ++CurResourceFileIndex )
			{
				FString DefaultLanguageFileName = DefaultLanguageResourceFileNames( CurResourceFileIndex );
				FString WPFResourcePathAndFileName = WPFResourcePath * DefaultLanguageFileName;
				
				// First try to load default language (INT) version.
				try
				{
					// Allocate Xaml file reader
					auto_handle< StreamReader > MyStreamReader =
						gcnew StreamReader( CLRTools::ToString( WPFResourcePathAndFileName ) );

					// Load the file
					ResourceDictionary^ MyDictionary =
						static_cast< ResourceDictionary^ >( XamlReader::Load( MyStreamReader->BaseStream, MyParserContext ) );

					// Add to the application's list of dictionaries
					Application::Current->Resources->MergedDictionaries->Add( MyDictionary );
				}

				catch( Exception^ E )
				{
					// Error loading .xaml file
					appMsgf( AMT_OK,
						*FString::Printf( TEXT( "Error loading default .xaml resource dictionary from file [%s]; reason [%s]" ),
						*WPFResourcePathAndFileName,
						*CLRTools::ToFString( E->Message ) ) );
				}

				// Now check to see if we have a version of this file for the currently active language
				const TCHAR* LanguageName = UObject::GetLanguage();
				if( appStricmp(LanguageName, DefaultLanguageName) != 0 )
				{
					FString LanguageFileNameSuffix = FString::Printf( TEXT( ".%s.xaml" ), LanguageName );
					FString ChoppedFileName = DefaultLanguageFileName.Left( DefaultLanguageFileName.Len() - DefaultLanguageFileNameSuffix.Len() );
					FString LocalizedResourceFileName = ChoppedFileName + LanguageFileNameSuffix;
					FString LocalizedResourcePathAndFileName = WPFResourcePath * LocalizedResourceFileName;
					if( GFileManager->FileSize( *LocalizedResourcePathAndFileName ) >= 0 )
					{
						// Great, we found a localized version of the file!

						try
						{
							// Allocate Xaml file reader
							auto_handle< StreamReader > MyStreamReader =
								gcnew StreamReader( CLRTools::ToString( LocalizedResourcePathAndFileName ) );

							// Load the file
							ResourceDictionary^ MyDictionary =
								static_cast< ResourceDictionary^ >( XamlReader::Load( MyStreamReader->BaseStream, MyParserContext ) );

							// Add to the application's list of dictionaries
							// According to http://msdn.microsoft.com/en-us/library/aa350178.aspx in case of the same key in multiple dictionaries,
							// the key returned will be the one from the merged dictionary added LAST to the MergedDictionaries collection.
							Application::Current->Resources->MergedDictionaries->Add( MyDictionary );
						}

						catch( Exception^ E )
						{
							// Error loading .xaml file
							appMsgf( AMT_OK,
								*FString::Printf( TEXT( "Error loading localized .xaml resource dictionary from file [%s]; reason [%s]" ),
								*LocalizedResourcePathAndFileName,
								*CLRTools::ToFString( E->Message ) ) );
						}
					}
					else
					{
						// Print a warning about missing LOC file and continue loading.  We'll just use the INT
						// resource file for these strings.
						warnf( TEXT( "Editor warning: Missing localized version of WPF XAML resource file [%s] (expected [%s] for current language)" ), *WPFResourcePathAndFileName, *LocalizedResourcePathAndFileName );
					}
				}
			}
		}
	}

}