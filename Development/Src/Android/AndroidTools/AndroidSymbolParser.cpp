/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "Common.h"

using namespace System;
using namespace System::IO;
using namespace System::Text;

#include "CommandLineWrapper.h"
#include "AndroidSymbolParser.h"

namespace AndroidTools
{
/**
 * Constructor.
 */
FAndroidManagedSymbolParser::FAndroidManagedSymbolParser() 
	: CmdLineProcess( new FCommandLineWrapper() )
{
}

/**
 * Converted to virtual void Dispose().
 */
FAndroidManagedSymbolParser::~FAndroidManagedSymbolParser()
{
	this->!FAndroidManagedSymbolParser();
	GC::SuppressFinalize( this );
}

/**
 * Finalizer frees native memory.
 */
FAndroidManagedSymbolParser::!FAndroidManagedSymbolParser()
{
	if( CmdLineProcess )
	{
		delete CmdLineProcess;
		CmdLineProcess = NULL;
	}
}

/**
 * Loads symbols for an executable.
 *
 * @param	ExePath		The path to the executable whose symbols are going to be loaded.
 * @param	Modules		The list of modules loaded by the process.
 */
bool FAndroidManagedSymbolParser::LoadSymbols( String^ ExePath, String^, bool, Object^ )
{
// 		// make sure the file exists
// 		if(!File::Exists(ExePath))
	{
		System::Windows::Forms::OpenFileDialog^ OpenFileDlg = gcnew System::Windows::Forms::OpenFileDialog();

		OpenFileDlg->Title = L"Find .so file for the running Android game";
		OpenFileDlg->Filter = L".so Files|*.so";
		OpenFileDlg->RestoreDirectory = true;
		OpenFileDlg->DefaultExt = "so";
		OpenFileDlg->AddExtension = true;

		if( OpenFileDlg->ShowDialog() == System::Windows::Forms::DialogResult::OK )
		{
			ExePath = OpenFileDlg->FileName;
		}
		else
		{
			return false;
		}
	}

	// cache the exe path
	ExecutablePath = ExePath;

	// hunt down the addr2line executable (allowing for an environment variable to help)
	String^ AndroidRoot = Environment::GetEnvironmentVariable( "ANDROID_ROOT" );
	if( AndroidRoot == nullptr )
	{
		AndroidRoot = L"C:\\Android";
		if( !File::Exists( L"C:\\Android") && File::Exists( L"D:\\Android" ) )
		{
			AndroidRoot = L"D:\\Android";
		}
	}

	// get NDK directory
	String^ NDKRoot = Environment::GetEnvironmentVariable( "NDKROOT" );
	if( NDKRoot == nullptr )
	{
		// create NDKRoot
		NDKRoot = AndroidRoot + L"\\android-ndk-r8b";
	}

	if( Environment::GetEnvironmentVariable("CYGWIN") == nullptr )
	{
		Environment::SetEnvironmentVariable( "CYGWIN", "nodosfilewarning" );
	}

	if( Environment::GetEnvironmentVariable( "CYGWIN_HOME" ) == nullptr )
	{
		Environment::SetEnvironmentVariable( "CYGWIN_HOME", Path::Combine( AndroidRoot, "cygwin" ) );
	}

	// put cygwin/bin into the path
	Environment::SetEnvironmentVariable( "PATH", String::Format( "{0};{1}/bin",
		Environment::GetEnvironmentVariable( "PATH" ),
		Environment::GetEnvironmentVariable( "CYGWIN_HOME" ) ) );

	// generate the commandline for interactive addr2line
	String^ CmdLine = String::Format( L"{0}\\toolchains\\arm-linux-androideabi-4.4.3\\prebuilt\\windows\\bin\\arm-linux-androideabi-addr2line.exe -fCe {1}", NDKRoot, ExecutablePath );

	pin_ptr<Byte> NativeCmdLine = &Encoding::UTF8->GetBytes( CmdLine )[0];

	// attempt to create it; if it fails, then we do arm-eabi-addr2line in ResolveAddressToSymboInfo with one call per line
	CmdLineProcess->Create( ( char* )NativeCmdLine );

	// if we got here, we are good to go
	return true;
}

/**
 * Unloads any currently loaded symbols.
 */
void FAndroidManagedSymbolParser::UnloadSymbols()
{
	CmdLineProcess->Terminate();
}

/**
 * Retrieves the symbol info for an address.
 *
 * @param	Address			The address to retrieve the symbol information for.
 * @param	OutFileName		The file that the instruction at Address belongs to.
 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
 * @return	True if the function succeeds.
 */
bool FAndroidManagedSymbolParser::ResolveAddressToSymboInfo( QWORD Address, String^ %OutFileName, String^ %OutFunction, int% OutLineNumber )
{
	// if we didn't already create a addr2line process, there's a problem
	if( !CmdLineProcess->IsCreated() )
	{
		System::Windows::Forms::MessageBox::Show( nullptr, L"Failed to create arm-eabi-addr2line process. Make sure the Android tools are installed as expected.", L"Process creation error", System::Windows::Forms::MessageBoxButtons::OK, System::Windows::Forms::MessageBoxIcon::Error );
		return false;
	}

	// send an address to addr2line
	char Temp[128];
	sprintf_s( Temp, 128, "0x%I64x", Address );

	CmdLineProcess->Write( Temp );
	CmdLineProcess->Write( "\n" );

	// now read the output; sample:
	// UEngine::Exec(char const*, FOutputDevice&)
	// d:\dev\SomeFile.cpp::45
	char* Symbol = CmdLineProcess->ReadLine( 5000 );
	char* FileAndLineNumber = CmdLineProcess->ReadLine( 5000 );

	// find the last colon (before the line number); will always exist
	char* LastColon = strrchr( FileAndLineNumber, ':' );
		
	// if this fails, something really weird happened
	if( !LastColon )
	{
		OutLineNumber = 0;
		OutFileName = gcnew String( "???" );
	}
	else
	{
		// cut off the string at the colon
		*( LastColon ) = 0;

		// and pull it apart into two parts
		OutLineNumber = atoi( LastColon + 1 );
		OutFileName = gcnew String( FileAndLineNumber );
	}
		
	// use the symbols
	if( Symbol )
	{
		OutFunction = gcnew String( Symbol );
	}
	else
	{
		OutFunction = gcnew String( "Unknown" );
	}

	return true;
}
}