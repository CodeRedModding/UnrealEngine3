/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "CommandLineWrapper.h"

FCommandLineWrapper::FCommandLineWrapper()
{
	Init();
}

FCommandLineWrapper::~FCommandLineWrapper()
{
	Terminate();
}

void FCommandLineWrapper::Init()
{
	StdInRead = NULL;
	StdInWrite = NULL;
	StdOutRead = NULL;
	StdOutWrite = NULL;
	Buffer[0] = '\0';
	BufferStart = 0;
	BufferEnd = 0;
	ZeroMemory( &ProcInfo, sizeof( ProcInfo ) );
}

bool FCommandLineWrapper::IsCreated()
{
	return ProcInfo.hProcess != NULL;
}

bool FCommandLineWrapper::Create( char* CmdLine )
{
	SECURITY_ATTRIBUTES SecurityAttr; 
	SecurityAttr.nLength = sizeof( SecurityAttr ); 
	SecurityAttr.bInheritHandle = TRUE; 
	SecurityAttr.lpSecurityDescriptor = NULL; 

	CreatePipe( &StdInRead, &StdInWrite, &SecurityAttr, 0 );
	CreatePipe( &StdOutRead, &StdOutWrite, &SecurityAttr, 0 );
	SetHandleInformation( StdInWrite, HANDLE_FLAG_INHERIT, 0 );
	SetHandleInformation( StdOutRead, HANDLE_FLAG_INHERIT, 0 );

	// set up process spawn structures
	STARTUPINFOA StartupInfo;
	ZeroMemory( &StartupInfo, sizeof( StartupInfo ) );
	StartupInfo.cb = sizeof( StartupInfo );
	StartupInfo.wShowWindow = SW_HIDE;
	StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	StartupInfo.hStdInput = StdInRead;
	StartupInfo.hStdOutput = StdOutWrite;
	StartupInfo.hStdError = StdOutWrite;

	// kick off the command line process
	BOOL bOk = CreateProcessA( NULL, CmdLine, NULL, NULL, TRUE, /*CREATE_NO_WINDOW|*/CREATE_NEW_PROCESS_GROUP, NULL, NULL, &StartupInfo, &ProcInfo );
	if( !bOk )
	{
		GetLastError();
		return false;
	}

	return true;
}

bool FCommandLineWrapper::Terminate()
{
	BOOL CtrlEventSucceeded = TRUE;
	if( ProcInfo.hProcess )
	{
		CtrlEventSucceeded = GenerateConsoleCtrlEvent( CTRL_BREAK_EVENT, ProcInfo.dwProcessId );
		CloseHandle( ProcInfo.hProcess );
		CloseHandle( ProcInfo.hThread );
	}
	CloseHandle( StdInRead );
	CloseHandle( StdInWrite );
	CloseHandle( StdOutRead );
	CloseHandle( StdOutWrite );

	Init();

	return CtrlEventSucceeded ? true : false;
}

DWORD FCommandLineWrapper::Poll( DWORD Timeout )
{
	DWORD Status = WaitForSingleObject( ProcInfo.hProcess, Timeout );
	return Status;
}

char* FCommandLineWrapper::ReadLine( DWORD Timeout /*==5000*/ )
{
	char* Str = ReadLineInternal();
	if( Str || Timeout == 0 )
	{
		return Str;
	}

	DWORD Retrials = max( Timeout / 50, 1 );
	Str = ReadLineInternal();
	while( --Retrials && Str == NULL )
	{
		Poll( 50 );
		Str = ReadLineInternal();
	}

	return Str;
}

char* FCommandLineWrapper::ReadLineInternal()
{
	// Make sure we have a nul-terminated string
	Buffer[BufferEnd] = '\0';

	if( BufferStart == BufferEnd )
	{
		BufferStart = 0;
		BufferEnd = 0;
	}
	else
	{
		// Early scan for line data:
		char* LineData = GetNextLine();
		if( LineData )
		{
			return LineData;
		}

		// Move un-read data to the beginning of 'Buffer' (portion of a line)
		if( BufferStart != 0 )
		{
			DWORD Size = BufferEnd - BufferStart;
			for( DWORD n = 0; n <= Size; ++n )
			{
				Buffer[n] = Buffer[BufferStart + n];
			}
			BufferStart = 0;
			BufferEnd = Size;
		}
	}

	DWORD BufferAvailable = BUFFERSIZE - BufferEnd - 1;

	// Buffer overflow? Return what we have.
	if( BufferAvailable == 0 )
	{
		BufferStart = 0;
		BufferEnd = 0;		// NOTE: Won't index a nul char in this case.
		return Buffer;
	}

	DWORD SizeToRead, SizeRead;
	PeekNamedPipe( StdOutRead, NULL, 0, NULL, &SizeToRead, NULL );
	if( SizeToRead > BufferAvailable )
	{
		SizeToRead = BufferAvailable;
	}

	if( SizeToRead )
	{
		ReadFile( StdOutRead, Buffer+BufferEnd, SizeToRead, &SizeRead, NULL);
		BufferEnd += SizeRead;
		Buffer[BufferEnd] = '\0';
		return GetNextLine();
	}

	return NULL;
}

char* FCommandLineWrapper::GetNextLine()
{
	char* EndLine = strchr( Buffer + BufferStart, '\n' );
	if( !EndLine )
	{
		return NULL;
	}

	char* LineData = Buffer + BufferStart;

	// Strip trailing whitespace
	char* Ptr = EndLine;
	while( Ptr >= ( Buffer + BufferStart ) && isspace( *Ptr ) )
	{
		*Ptr-- = '\0';
	}

	// Bump 'BufferStart' to point to next un-read data
	BufferStart = DWORD( EndLine - Buffer ) + 1;

	return LineData;
}

void FCommandLineWrapper::Write( const char* String )
{
	DWORD BytesToWrite = ( DWORD )strlen( String );
	DWORD NumBytesWritten;
	WriteFile( StdInWrite, String, BytesToWrite, &NumBytesWritten, NULL );
}
