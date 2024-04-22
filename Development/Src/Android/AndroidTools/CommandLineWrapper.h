/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef COMMANDLINEWRAPPER_H
#define COMMANDLINEWRAPPER_H

#include "windows.h"

class FCommandLineWrapper
{
public:
	FCommandLineWrapper();
	~FCommandLineWrapper();

	bool			IsCreated( );
	bool			Create( char* CmdLine );
	bool			Terminate( );
	DWORD		Poll( DWORD Timeout );				// Timeout in millisec

	char*		ReadLine( DWORD Timeout = 5000 );	// Timeout in millisec
	char*		ReadAll();
	void			Write( const char* String );

protected:
	enum			{ BUFFERSIZE = 4 * 1024 };
	void			Init();
	char*		GetNextLine();
	char*		ReadLineInternal();

	PROCESS_INFORMATION	ProcInfo;
	HANDLE				StdInRead;
	HANDLE				StdInWrite;
	HANDLE				StdOutRead;
	HANDLE				StdOutWrite;
	char					Buffer[BUFFERSIZE];
	DWORD				BufferStart;				// Index to beginning of un-read content
	DWORD				BufferEnd;				// Index to end of un-read content (usually pointing to a nul char).
};

#endif