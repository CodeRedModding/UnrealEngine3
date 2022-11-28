/*

	FastFile.h - file io by epic

    Copyright (c) 1999,2000 Epic MegaGames, Inc. All Rights Reserved.
*/

#pragma once

#include "assert.h"

INT appGetVarArgs( char* Dest, INT Count, const char*& Fmt );

void PopupBox(char* PrintboxString, ... );

class LogFile
{
	public:
	INT Tabs;
	FILE* LStream;
	INT LastError;
	
	// Simple log file support.
	int Open(MString const& LogToPath, MString const& LogName, int Enabled)
	{	
		if( Enabled && LogToPath.length() && LogName.length() )
		{
			Tabs = 0;
			MString LogFileName = LogToPath + LogName;
			LStream = fopen(LogFileName.asChar(),"wt"); // Open for writing..
			if (!LStream) 
			{
				return 0;
			}
			return 1;
		}
		return 0;
	}

	void Logf(const char* LogString, ... )
	{
		if( LStream )
		{
			char TempStr[4096];
			appGetVarArgs(TempStr,4096,LogString);
			fprintf(LStream,TempStr);
		}
	}

	void LogfLn(char* LogString, ... )
	{
		if( LStream )
		{
			char TempStr[4096];
			appGetVarArgs(TempStr,4096,LogString);
			strcat( TempStr, "\n");
		
			fprintf(LStream,TempStr);
		}
	}

	void doTabs()
	{
		if( LStream )
		{
			for (INT i =0; i < (Tabs*4); i++ )
			{
				fprintf(LStream, " ");
			}
		}
	}

	int Error()
	{
		return ferror(LStream);
	}

	void doCR()
	{
		if ( LStream )
		{
			fprintf(LStream,"\r\n");
		}
	}

	int Close()
	{
		if( LStream )
		{
			fclose(LStream);
			LStream = NULL;
			return 1;			
		}
		return 0;
	}

	LogFile()
	{
		LStream = NULL;
		Tabs = 0;
	}

	~LogFile()
	{
		Close();
	}

};



//
// Quick-and-dirty simple file class. Reading is unbuffered. Fast buffered binary file writing.
//
class FastFileClass
{
private:

	HANDLE  FileHandle;    
	BYTE*	Buffer;
	int		BufCount;
	int     Error;
	int     ReadPos;
	unsigned long BytesWritten;
	#define	BufSize (8192)
	
public:
	// Ctor
	FastFileClass()
	{
		FileHandle = INVALID_HANDLE_VALUE;
		Buffer = NULL;
		BufCount = 0;
		Error = 0;
		ReadPos = 0;
		Buffer = new BYTE[BufSize];
	}

	~FastFileClass()
	{
		CloseFlush();
		if( Buffer )delete Buffer;
	}

	int CreateNewFile(const char *FileName)
	{
		Error = 0;
		if ((FileHandle = CreateFile(FileName, GENERIC_WRITE,FILE_SHARE_WRITE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))
		    == INVALID_HANDLE_VALUE)
		{
			Error = 1;
		}

		return Error;
	}

	void Write(void *Source,int ByteCount)
	{
		if ((BufCount+ByteCount) < BufSize)
		{
			memmove(&Buffer[BufCount],Source,ByteCount);
			BufCount += ByteCount;
		}
		else  
		if (ByteCount <= BufSize)
		{
			Flush();
			memmove(Buffer,Source,ByteCount);
			BufCount += ByteCount;
		}
		else // Too big a chunk to be buffered.
		{
			Flush();
			if (IsOpen()) {
				WriteFile(FileHandle,Source,ByteCount,&BytesWritten,NULL);
			}
		}
	}

	// Write one byte. Flush when closed...
	inline void WriteByte(const BYTE OutChar)
	{
		if (BufCount < BufSize)
		{
			Buffer[BufCount] = OutChar;
			BufCount++;
			return;
		}
		else
		{
			BYTE OutExtra = OutChar;
			Write(&OutExtra, 1);
		}
	}

	// Fast string writing. 
	inline void Print(char* OutString, ... )
	{
		char TempStr[4096];
		appGetVarArgs(TempStr,4096,OutString);
		Write(&TempStr, strlen(TempStr));		
	}

	int GetError()
	{
		return Error;
	}
	
	bool IsOpen()
	{
		return (FileHandle != INVALID_HANDLE_VALUE);
	}

	// Close for reading.
	int Close()
	{
		if (IsOpen()) {
			CloseHandle(FileHandle);
		}
		FileHandle = INVALID_HANDLE_VALUE;
		return Error;
	}

	void Flush()
	{
		if (IsOpen() && BufCount) {
			WriteFile(FileHandle, Buffer, BufCount, &BytesWritten, NULL); 
			BufCount = 0;
		}
	}

	// Close for writing.
	int CloseFlush()
	{
		ReadPos = 0;
		// flush 
		Flush();
		Close();
		return Error;
	}

	int OpenExistingFile(char *FileName)
	{
		ReadPos = 0;
		Error = 0;
		//Note: will return with error if file is write-protected.
		if ((FileHandle = CreateFile(FileName, GENERIC_WRITE | GENERIC_READ , FILE_SHARE_WRITE,NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
		    == INVALID_HANDLE_VALUE)
		{
			Error = 1;
		}
		return Error;
	}

	// Open for read only: will succeeed with write-protected files.
	int OpenExistingFileForReading(char *FileName)
	{
		ReadPos = 0;
		Error = 0;
		
		if ((FileHandle = CreateFile(FileName, GENERIC_READ , FILE_SHARE_WRITE,NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL))
		    == INVALID_HANDLE_VALUE)
		{
			Error = 1;
		}
		return Error;
	}

	// Get current size of a file opened for reading/writing...
	int GetSize()
	{
		DWORD Size;
		Size = GetFileSize(FileHandle, NULL);
		if( Size == 0xFFFFFFFF ) Size = 0;
		return (INT)Size;
 	}

	// Directly maps onto windows' filereading. Return number of bytes read.
	int Read(void *Dest,int ByteCount) // int InFilePos ? )
	{
		ULONG BytesRead = 0;

		if (! ReadFile( FileHandle, Dest, ByteCount, &BytesRead, NULL) )
			BytesRead = 0;

		ReadPos +=BytesRead;

		return BytesRead;
	}

	int Seek( INT InPos )
	{
		assert(InPos>=0);
		//assert(InPos<=Size);

		if( SetFilePointer( FileHandle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
		{
			return 0;
		}
		ReadPos     = InPos;
		return InPos;
	}

	// Quick and dirty read-from-position..
	int ReadFromPos(void *Dest, int ByteCount, int Position)
	{
		Seek( Position );
		Read( Dest, ByteCount);
	}

};