// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "Stdafx.h"
#include "TOCSettings.h"
#include "GameSettings.h"
#include "OutputEventArgs.h"

using namespace System;
using namespace System::IO;

namespace ConsoleInterface
{
	TOCSettings::TOCSettings( OutputHandlerDelegate ^OutputHandler ) 
		: 
//		  Languages( gcnew array<String^> { L"INT" } )
		  Languages( gcnew array<String^>(1) )
		, GameName( L"ExampleGame" )
		, TargetBaseDirectory( String::Concat( L"UE3-", Environment::MachineName ) )
		, TextureExtensions( nullptr )
		, GenericVars( gcnew Dictionary<String^, String^>() )
		, GameOptions( gcnew GameSettings() )
		, DestinationPaths( gcnew List<String^>() )
		, ZipFiles( gcnew List<String^>() )
		, IsoFiles( gcnew List<String^>() )
		, TargetsToSync( gcnew List<String^>() )
		, MergeExistingCRC( false )
		, NoSync( false )
		, bInvertedCopy( false )
		, ComputeCRC( false )
		, bCaseSensitiveFileSystem( false )
		, VerifyCopy( false )
		, Force( false )
		, GenerateTOC( true )
		, VerboseOutput( false )
		, bRebootBeforeCopy( false )
		, bNonInteractive( false )
		, SleepDelay( 0 )
		, mOutputHandler(OutputHandler)
	{
		TargetBaseDirectory = UnrealControls::DefaultTargetDirectory::GetDefaultTargetDirectory();

		Languages[0] = L"INT";
	}

	void TOCSettings::Write(Color TxtColor, String ^Message, ... array<Object^> ^Parms)
	{
		if(mOutputHandler)
		{
			mOutputHandler(this, gcnew OutputEventArgs(TxtColor, String::Format(Message, Parms)));
		}
	}

	void TOCSettings::WriteLine(Color TxtColor, String ^Message, ... array<Object^> ^Parms)
	{
		if(mOutputHandler)
		{
			mOutputHandler(this, gcnew OutputEventArgs(TxtColor, String::Concat(String::Format(Message, Parms), Environment::NewLine)));
		}
	}

	void TOCSettings::WriteLine(Color TxtColor, String ^Message)
	{
		if(mOutputHandler)
		{
			mOutputHandler(this, gcnew OutputEventArgs(TxtColor, String::Concat(Message, Environment::NewLine)));
		}
	}

	void TOCSettings::WriteLine(unsigned int TxtColor, String ^Message)
	{
		if(mOutputHandler)
		{
			mOutputHandler(this, gcnew OutputEventArgs(Color::FromArgb(TxtColor), String::Concat(Message, Environment::NewLine)));
		}
	}
}
