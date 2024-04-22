/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#using <mscorlib.dll>
#using <System.dll>
//#using <..\..\..\..\RPCUtility.exe>

#include "Common.h"


using namespace System;
using namespace System::IO;
using namespace System::Text;
using namespace System::Diagnostics;
using namespace System::Runtime::InteropServices;
using namespace System::Threading;
using namespace System::Collections;
using namespace System::Net;
using namespace System::Net::NetworkInformation;

using namespace RPCUtility;

#include "IPhoneSymbolParser.h"

namespace IPhoneTools
{
/**
 * Constructor.
 */
FIPhoneManagedSymbolParser::FIPhoneManagedSymbolParser() 
{
	MacName = Environment::GetEnvironmentVariable( "ue3.iPhone_SigningServerName" );
	if( MacName == nullptr )
	{
		MacName = "a1487";
	}

	// Ensure we have a Mac available to resolve symbols
	Ping^ PingSender = gcnew Ping;
	PingReply^ Reply = PingSender->Send( MacName );

	if( Reply->Status != IPStatus::Success )
	{
		throw( "A target Mac is Required. Please ensure your Mac is switched on and the ue3.iPhone_SigningServerName environment variable is set." );
	}

	RPCSocket = RPCUtility::CommandHelper::ConnectToUnrealRemoteTool( MacName );
}

/**
 * Converted to virtual void Dispose().
 */
FIPhoneManagedSymbolParser::~FIPhoneManagedSymbolParser()
{
	this->!FIPhoneManagedSymbolParser();
	GC::SuppressFinalize( this );
}

/**
 * Finalizer frees native memory.
 */
FIPhoneManagedSymbolParser::!FIPhoneManagedSymbolParser()
{
}

/**
 * Loads symbols for an executable.
 *
 * @param	ExePath		The path to the executable whose symbols are going to be loaded.
 * @param	Modules		The list of modules loaded by the process.
 */
bool FIPhoneManagedSymbolParser::LoadSymbols( String^ ExePath, String^, bool, Object^ )
{
	//ExePath here translates to symbols path, e.g. usually in the format "UDKGame-IPhone-Release.ipa";
	DecoratedGameNameString = Path::GetFileNameWithoutExtension(ExePath);

	// For debug purposes. Ensure we can access the dSYM
	if( Environment::CurrentDirectory->IndexOf( "Binaries" ) < 0 )
	{
		Environment::CurrentDirectory = "../../../../Binaries";
	}

	// The file which will be copied and extracted on the mac. This is the file we will use to symbolicate our addresses
	String^ FullSymbolsPath = Environment::CurrentDirectory + "/IPhone/" + DecoratedGameNameString + ".app.dSYM.zip";
	FullSymbolsPath = FullSymbolsPath->Replace("\\", "/");

	bool bFileExist = File::Exists( FullSymbolsPath );
	if( bFileExist == false )
	{
		// The Symbols file doesn't exist. Make sure that you have the one which should match the stub/ipa. BuildConfiguration::bUseMallocProfiler
		return false;
	}

	// Get the path of the symbols file without our windows specified drive. Following suit from iOS builder here. 
	String^ StrippedDir = FullSymbolsPath->Substring( FullSymbolsPath->IndexOf( ":" )+1 );
	String^ MacdSYMPath = "/UnrealEngine3/Builds/" + Environment::MachineName + StrippedDir;

	// Used later for atos symbolication
	MacWorkingDirectory = MacdSYMPath->Substring( 0, MacdSYMPath->LastIndexOf("/")+1 );
	
	try
	{
		Hashtable result = RPCUtility::CommandHelper::RPCUpload( RPCSocket, FullSymbolsPath, MacdSYMPath );
		if( (Int64)result["ExitCode"] == -1 )
		{
			// RPCUpload failed on the mac side. see result["CommandOutput"]
			return false;
		}
	}
	catch( ... )
	{
		// RPCUpload threw exception. Probably unable to connect to mac
		return false;
	}

	// Delete any lingering historic .dSym Files we have extracted previously. Any further ones we extract will not maintain the name we want.
	String^ ExtractedFile = MacdSYMPath->Substring( 0, MacdSYMPath->LastIndexOf(".zip") );

	Hashtable DeleteCommandResult = RPCUtility::CommandHelper::RPCCommand( RPCSocket, "/", "rm", "-r "+ExtractedFile, nullptr );
	// Give the mac a chance to delete the old files
	Sleep(1000);

	// Extract the dSym from the zip
	Hashtable ExtractCommandResult = RPCUtility::CommandHelper::RPCCommand( RPCSocket, "/", "open", MacdSYMPath, nullptr );

	if( (Int64)ExtractCommandResult["ExitCode"] == 0 )
	{
		// Give the mac a chance to extract the file, Will probably need upped depending on network..?
		Sleep(3000);
	}
	else
	{
		return false;
	}

	return true;

}

/**
 * Unloads any currently loaded symbols.
 */
void FIPhoneManagedSymbolParser::UnloadSymbols()
{

}


/**
 * Delegate used to capture output from RPCUtility and parse the call-stack line we receive in return
 * 
 * Symbolicated line is in the example below
 * FMallocProfiler::Realloc(void*, unsigned long, unsigned long) (in UDKGame-IPhone-Release) (FMallocProfiler.h:512)
 *
 * @param	JaggedCallstack	Callstack in the form its returned from Atos
 */
void FIPhoneManagedSymbolParser::ParseCallStack( String^ JaggedCallstack, array<String^>^ %OutFileName, array<String^>^ %OutFunction, array<int>^ %OutLineNumber )
{
	String^ Output = JaggedCallstack;

	array<String^>^ IndependentJaggedCallstack = gcnew array<String^>(OutFileName->Length);
	
	for( int i = 0; i < IndependentJaggedCallstack->Length; i++ )
	{
		int EndIndex = Output->IndexOf("\n");
		IndependentJaggedCallstack[i] = Output->Substring( 0, EndIndex );
		Output = Output->Substring( EndIndex + 1 );

		String^ CallstackToBeParsed = IndependentJaggedCallstack[i];

		int pos1 = CallstackToBeParsed->LastIndexOf( ")" );
		int pos2 = CallstackToBeParsed->Length;

		bool bIsValid = pos2 == ( pos1 + 1 ) && CallstackToBeParsed->IndexOf( " (in" ) > 0 && CallstackToBeParsed->LastIndexOf( ":" ) > CallstackToBeParsed->LastIndexOf( "(" );

		if( bIsValid )
		{
			// Strip out the function name
			int FunctionStartPos = 0;
			int FunctionEndPos = CallstackToBeParsed->IndexOf( " (in" );
			OutFunction[i] = CallstackToBeParsed->Substring( FunctionStartPos, FunctionEndPos );

			// Strip our the file name
			int FileStartPos = CallstackToBeParsed->LastIndexOf( "(" ) + 1;
			int FileEndPos = CallstackToBeParsed->LastIndexOf( ":" );
			OutFileName[i] = CallstackToBeParsed->Substring( FileStartPos, FileEndPos-FileStartPos );

			// Strip out the Line number
			int LineStartPos = FileEndPos + 1;
			int LineEndPos = CallstackToBeParsed->LastIndexOf( ")" );
			OutLineNumber[i] = Convert::ToInt32( CallstackToBeParsed->Substring( LineStartPos, LineEndPos-LineStartPos ) );
		}
		else
		{
			OutFileName[i] = "???";
			OutFunction[i] = CallstackToBeParsed;
			OutLineNumber[i] = 0;
			if( CallstackToBeParsed->IndexOf( " (in" ) > 0 && CallstackToBeParsed->LastIndexOf(")") != CallstackToBeParsed->Length-1 )
			{
				int FunctionStartPos = 0;
				int FunctionEndPos = CallstackToBeParsed->IndexOf( " (in" );
				OutFunction[i] = CallstackToBeParsed->Substring( FunctionStartPos, FunctionEndPos );

				int LineStartPos = CallstackToBeParsed->LastIndexOf( " " );
				OutLineNumber[i] = Convert::ToInt32( CallstackToBeParsed->Substring( LineStartPos ) );
			}
		}
	}
}


/**
 * Not used on iOS at the minute. Instead we use the batched process. See below.
 *
 * @param	Address			The address to retrieve the symbol information for.
 * @param	OutFileName		The file that the instruction at Address belongs to.
 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
 * @return	True if the function succeeds.
 */
bool FIPhoneManagedSymbolParser::ResolveAddressToSymboInfo( QWORD , String^ %, String^ %, int%  )
{
	return false;
}

/**
 * Retrieves the symbol info for a range of addresses in a batch.
 *
 * @param	Address			The addresses to retrieve the symbol information for.
 * @param	OutFileName		The file that the instruction at Address belongs to.
 * @param	OutFunction		The name of the function that owns the instruction pointed to by Address.
 * @param	OutLineNumber	The line number within OutFileName that is represented by Address.
 * @return	True if the function succeeds.
 */
bool FIPhoneManagedSymbolParser::ResolveAddressBatchesToSymbolInfo( array<QWORD>^ Address, array<String^>^ %OutFileName, array<String^>^ %OutFunction, array<int>^ %OutLineNumber )
{
	String^ AddressesToSeek;
	// Parse the address to a hex format
	for( int i = 0; i < Address->Length; i++ )
	{
		Int64 AddressAsInt = Address[i];
		AddressesToSeek += ( " 0x" + AddressAsInt.ToString( "X16" ) );
	}

	String^ RPCProcessCommandLine = "-o " + 
									MacWorkingDirectory +
									DecoratedGameNameString + ".app.dSYM" +
									"/Contents/Resources/DWARF/" + 
									DecoratedGameNameString +
									" -arch armv7" +
									AddressesToSeek;

	Hashtable CommandResult = RPCUtility::CommandHelper::RPCCommand( RPCSocket, "/", "atos", RPCProcessCommandLine, nullptr );
	
	if( (Int64)CommandResult["ExitCode"] == 0 )
	{
		ParseCallStack( (String^)CommandResult["CommandOutput"], OutFileName, OutFunction, OutLineNumber );
	}
	else
	{
		return false;
	}

	return true;
}

}
