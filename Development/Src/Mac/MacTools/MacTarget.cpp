/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "MacTarget.h"
#include "..\Inc\MacCrashDefines.h"

///////////////////////CMacTarget/////////////////////////////////

CMacTarget::CMacTarget(const sockaddr_in* InRemoteAddress, FMacSocket* InTCPClient, FMacSocket* InUDPClient)
 : CTarget( InRemoteAddress, InTCPClient, InUDPClient )
 , bCollectingCallstack(false)
{
}

CMacTarget::~CMacTarget()
{
}

bool CMacTarget::ShouldSendTTY()
{
	bool bSendToTTY = true;
	if (bCollectingCallstack == false)
	{
		// Scan the buffer for UE3SYS:CRASH.
		char* CrashString = strstr(PacketBuffer, MacSysCrashToken);
		if (CrashString != NULL)
		{
			// We have a crash.
			bCollectingCallstack = true;

			int LenToCrash = (int)(CrashString - PacketBuffer);
			int CrashStringLen = (int)strlen(CrashString);
			CrashCallstackBuffer.empty();
			wchar_t TempBuffer[PACKET_BUFFER_SIZE + 16] = L"";
			int Length = MultiByteToWideChar(CP_UTF8, 0, CrashString, CrashStringLen, TempBuffer, PACKET_BUFFER_SIZE);
			if (Length > 0)
			{
				TempBuffer[Length] = '\0';
				CrashCallstackBuffer += TempBuffer;
			}

			int LenOfEndString = (int)(strlen(MacSysEndCrashToken));
			char* EndCrashString = strstr(PacketBuffer, MacSysEndCrashToken);
			if (EndCrashString != NULL)
			{
				// We have the complete callstack in this buffer...
				int LenToEndOfCrash = (int)(EndCrashString - CrashString + LenOfEndString);
				CrashCallstackBuffer[LenToEndOfCrash] = TEXT('\0');
				// Now report the crash
				ReportCrash();
				bCollectingCallstack = false;
			}

			if (LenToCrash > 0)
			{
				// Send what was in the buffer pre-crash
				PacketBuffer[LenToCrash] = '\0';
			}
			else
			{
				bSendToTTY = false;
			}

		}
	}
	else
	{
		// We are in the middle of collecting a callstack...
		// Search for the end.
		int LenOfEndString = (int)(strlen(MacSysEndCrashToken));
		char* EndCrashString = strstr(PacketBuffer, MacSysEndCrashToken);
		if (EndCrashString != NULL)
		{
			// We have the complete callstack in this buffer...
			int LenToEndOfCrash = (int)(EndCrashString - PacketBuffer + LenOfEndString);
			PacketBuffer[LenToEndOfCrash] = '\0';
			wchar_t TempBuffer[PACKET_BUFFER_SIZE + 16] = L"";
			int Length = MultiByteToWideChar(CP_UTF8, 0, PacketBuffer, (int)strlen(PacketBuffer), TempBuffer, PACKET_BUFFER_SIZE);
			if (Length > 0)
			{
				TempBuffer[Length] = '\0';
				CrashCallstackBuffer += TempBuffer;
			}
			// Now report the crash
			ReportCrash();
			bCollectingCallstack = false;
		}
		else
		{
			// Keep adding it to the end of the callstack buffer
			wchar_t TempBuffer[PACKET_BUFFER_SIZE + 16] = L"";
			int Length = MultiByteToWideChar(CP_UTF8, 0, PacketBuffer, (int)strlen(PacketBuffer), TempBuffer, PACKET_BUFFER_SIZE);
			if (Length > 0)
			{
				TempBuffer[Length] = '\0';
				CrashCallstackBuffer += TempBuffer;
			}
		}
		bSendToTTY = false;
	}
	return bSendToTTY;
}


/** Report the crash contains in the CrashCallstackBuffer */
void CMacTarget::ReportCrash()
{
	static bool s_bReportingCrash = false;

	if (s_bReportingCrash == false)
	{
		s_bReportingCrash = true;
		CrashCallstack.empty();
		TCHAR GameName[256] = {};
		bool bInBacktrace = false;
		wstring TempBuffer = CrashCallstackBuffer;
		const TCHAR* CrashBuffer = TempBuffer.c_str();
		while (CrashBuffer != NULL)
		{
			wstring CurrentLine = CrashBuffer;

			int Count = 0;
			// Find the newline or end of buffer
			while ((CrashBuffer[0] != TEXT('\0')) && (CrashBuffer[0] != TEXT('\n')))
			{
				Count++;
				CrashBuffer++;
			}

			TCHAR ErrorMessage[2048];
			DWORD EngineVersion;
			DWORD ChangeList;
			DWORD BacktraceCount = 0;
			TCHAR BuildConfiguration[64];

			if (Count > 0)
			{
				CurrentLine[Count] = TEXT('\0');
				if (bInBacktrace == true)
				{
					DWORD Address;
					if (swscanf_s(CurrentLine.c_str(), MacSysCallstackEntryString, &Address) == 1)
					{
						CrashCallstack.push_back(Address);
					}
					else if (wcsstr(CurrentLine.c_str(), MacSysCallstackEndString) != NULL)
					{
						bInBacktrace = false;
					}
				}
				else
				{
					if (swscanf_s(CurrentLine.c_str(), MacSysCrashString, ErrorMessage, _countof(ErrorMessage)) == 1)
					{
						// Error message (what caused the error typically)
						OutputDebugString(ErrorMessage);
						OutputDebugString(TEXT("\n"));
					}
					else if (swscanf_s(CurrentLine.c_str(), MacSysGameString, GameName, _countof(GameName)) == 1)
					{
						// Information line...
					}
					else if (swscanf_s(CurrentLine.c_str(), MacSysEngineVersionString, &EngineVersion) == 1)
					{
						// Information line...
					}
					else if (swscanf_s(CurrentLine.c_str(), MacSysChangelistString, &ChangeList) == 1)
					{
						// Information line...
					}
					else if (swscanf_s(CurrentLine.c_str(), MacSysConfigurationString, BuildConfiguration, _countof(BuildConfiguration)) == 1)
					{
						// Build configuration... needed for RPCUtility calls to symbolicate
						BuildConfigurationString = BuildConfiguration;
						OutputDebugString(BuildConfiguration);
						OutputDebugString(TEXT("\n"));
					}
					else if (swscanf_s(CurrentLine.c_str(), MacSysCallstackStartString, &BacktraceCount) == 1)
					{
						// End of backtrace information
						bInBacktrace = true;
					}
					else if (wcsstr(CurrentLine.c_str(), MacSysEndCrashString) != NULL)
					{
						// End of crash
					}
				}
			}

			if (CrashBuffer[0] == TEXT('\0'))
			{
				CrashBuffer = NULL;
			}
			else
			{
				// Skip past the '\n'
				CrashBuffer++;
			}
		}

		SendTTY("Generating crash report...\n");
		if (GetHumanReadableCallstack())
		{
			// Process the actual crash
			wstring GameNameString = GameName;
			CrashCallback(GameNameString.c_str(), FinalCallstackBuffer.c_str(), L"");
		}

		s_bReportingCrash = false;
	}
}

bool RunChildProcess(const char* ApplicationName, const char* CommandLine, char* Errors, int ErrorsSize, DWORD* ProcessReturnValue);

bool CMacTarget::GetHumanReadableCallstack()
{
	FinalCallstackBuffer.empty();

	string ApplicationNameString;
	string BaseCommandString;
	string MacName;
	TCHAR MacNameFromEnvironmentVariable[256];
	DWORD Error = GetEnvironmentVariable(TEXT("ue3.Mac_SigningServerName"), MacNameFromEnvironmentVariable, 256);
	if (Error > 0)
	{
		MacName = ToString(MacNameFromEnvironmentVariable);
	}
	else
	{
		MacName = "a1487";
	}

	// This is the guts of appComputerName()
	string ComputerName;
	char TempCompName[256];
	gethostname(TempCompName, 256);
	ComputerName = TempCompName;

	// Retrieve the executable path
	string WorkingDir;
	char TempPathName[1024];
	if (GetModuleFileNameA(NULL, TempPathName, 1024) != 0)
	{
		// This should be <DRIVE>:/<FOLDER>/UnrealEngine3/Binaries/UnrealConsole.exe"
		WorkingDir = TempPathName;
		// Parse off the <Drive>:
		string::size_type ColonPos = WorkingDir.find(":\\");
		if (ColonPos == string::npos)
		{
			ColonPos = WorkingDir.find(":/");
		}
		if (ColonPos != string::npos)
		{
			WorkingDir.replace(0, ColonPos + 1, "");
		}

		// Find the last /
		string::size_type LastSlashPos = WorkingDir.rfind("\\");
		if (LastSlashPos == string::npos)
		{
			LastSlashPos = WorkingDir.rfind("/");
		}
		if (LastSlashPos != string::npos)
		{
			// Get rid of the exe name
			WorkingDir.replace(LastSlashPos + 1, WorkingDir.length() - LastSlashPos - 1, "");
		}

		// Now convert any "\\" to "/"
		string::size_type ForwardSlashPos = WorkingDir.find("\\");
		while (ForwardSlashPos != string::npos)
		{
			WorkingDir.replace(ForwardSlashPos, 1, "/");
			ForwardSlashPos = WorkingDir.find("\\");
		}
	}
	else
	{
		throw("Failed to get executable path!");
	}
	string GameNameString = ToString(GameName);

	ApplicationNameString = "RPCUtility.exe";
	BaseCommandString  = "RPCUtility.exe ";
	BaseCommandString += MacName;
	BaseCommandString += " . atos ";
	BaseCommandString += "-o /UnrealEngine3/Builds/";
	BaseCommandString += ComputerName;
	BaseCommandString += WorkingDir;
	BaseCommandString += "Mac/";
	BaseCommandString += ToString(BuildConfigurationString);
	BaseCommandString += "-macosx/";
	BaseCommandString += GameNameString;
	BaseCommandString += "Game/Payload/";
	BaseCommandString += GameNameString;
	BaseCommandString += "Game.app.dSYM ";
	BaseCommandString += "-arch armv7 ";

	string dSYMNameString = "(in ";
	dSYMNameString += ToString(GameName);
	dSYMNameString += "Game.app.dSYM) ";

	for (int CallstackIdx = 0; CallstackIdx < int(CrashCallstack.size()); CallstackIdx++)
	{
		char CallstackEntry[32];
		sprintf_s(CallstackEntry, 32, "0x%08x", CrashCallstack[CallstackIdx]);
		
		string CommandString = BaseCommandString + CallstackEntry;

		char Errors[1024*64];
		DWORD ReturnValue;
		// spawn the child process with no DOS window popping up
		if (RunChildProcess(ApplicationNameString.c_str(), CommandString.c_str(), Errors, sizeof(Errors) - 1, &ReturnValue) == true)
		{
			if (ReturnValue == 0)
			{
				string ConvertedCallstack = Errors;
				// remove the '(in <GAMENAME>.app.dSYM) ' chunk...
				string::size_type dSYMPos = ConvertedCallstack.find(dSYMNameString.c_str());
				if (dSYMPos != string::npos)
				{
					ConvertedCallstack.replace(dSYMPos, dSYMNameString.length(), "");
				}

				//@todo. Fixup handling of objective-C functions...
				// This actually doesn't properly resolve the file/line#...
				// It says that MacAppDelegate MainUE3GameThread is in UnStats.h...

				// Convert obj-c lines to the proper format
				string::size_type ObjCStartPos = ConvertedCallstack.find("-[");
				if (ObjCStartPos == 0)
				{
					string::size_type ObjCEndPos = ConvertedCallstack.rfind("]");
					if (ObjCEndPos != string::npos)
					{
						ConvertedCallstack.replace(ObjCEndPos, 1, ")");
						ConvertedCallstack.replace(ObjCStartPos, 2, "(");
					}
				}

				// Now convert "(<FILE>:<LINE>)" to "[<FILE>:LINE]" for the crashreporter application
				string::size_type LastParenPos = ConvertedCallstack.rfind(')');
				string::size_type PrevParenPos = ConvertedCallstack.rfind('(', LastParenPos-1);
				string::size_type LastColonPos = ConvertedCallstack.rfind(':');
				if ((LastColonPos < LastParenPos) && (LastColonPos > PrevParenPos))
				{
					string IncludeFile;
					string SourceFile;
					string ObjCSourceFile;
					char TempBuffer[8];
					ConvertedCallstack._Copy_s(TempBuffer, 8, 2, LastColonPos-2);
					IncludeFile = TempBuffer;
					ConvertedCallstack._Copy_s(TempBuffer, 8, 4, LastColonPos-4);
					SourceFile = TempBuffer;
					ConvertedCallstack._Copy_s(TempBuffer, 8, 3, LastColonPos-3);
					ObjCSourceFile = TempBuffer;

					if ((IncludeFile.find(".h") != string::npos) ||
						(IncludeFile.find(".m") != string::npos) ||
						(ObjCSourceFile.find(".mm") != string::npos) ||
						(SourceFile.find(".cpp") != string::npos))
					{
						// It's the string we are looking for!
						ConvertedCallstack.replace(PrevParenPos, 1, "[");
						// 'Refind' the last paren
						LastParenPos = ConvertedCallstack.rfind(')');
						ConvertedCallstack.replace(LastParenPos, 1, "]");
					}
				}

				FinalCallstackBuffer += ToWString(ConvertedCallstack);
			}
		}
	}

	return TRUE;
}

/**
 * Runs a child process without spawning a command prompt window for each one
 *
 * @param CommandLine The commandline of the child process to run
 * @param Errors Output buffer for any errors
 * @param ErrorsSize Size of the Errors buffer
 * @param ProcessReturnValue Optional out value that the process returned
 *
 * @return TRUE if the process was run (even if the process failed)
 */
bool RunChildProcess(const char* ApplicationName, const char* CommandLine, char* Errors, int ErrorsSize, DWORD* ProcessReturnValue)
{
	// run the command (and avoid a window popping up)
	SECURITY_ATTRIBUTES SecurityAttr; 
	SecurityAttr.nLength = sizeof(SecurityAttr); 
	SecurityAttr.bInheritHandle = TRUE; 
	SecurityAttr.lpSecurityDescriptor = NULL; 

	HANDLE StdOutRead, StdOutWrite;
	CreatePipe(&StdOutRead, &StdOutWrite, &SecurityAttr, 0);
	SetHandleInformation(StdOutRead, HANDLE_FLAG_INHERIT, 0);

	// set up process spawn structures
	STARTUPINFOA StartupInfo;
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);
	StartupInfo.hStdOutput = StdOutWrite;
	StartupInfo.hStdError = StdOutWrite;
	StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
	PROCESS_INFORMATION ProcInfo;
	memset(&ProcInfo, 0, sizeof(ProcInfo));

	// kick off the child process
	if (!CreateProcessA(ApplicationName, (LPSTR)CommandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &StartupInfo, &ProcInfo))
	{
		sprintf(Errors, "\nFailed to start process '%s %s'\n", ApplicationName, CommandLine);
		return false;
	}

	bool bProcessComplete = false;
	char Buffer[1024 * 64];
	DWORD BufferSize = sizeof(Buffer);
	Errors[0] = 0;

	// wait until the process is finished
	while (!bProcessComplete)
	{
		DWORD Reason = WaitForSingleObject(ProcInfo.hProcess, 0);

		// read up to 64k of error (that would be crazy amount of error, but whatever)
		DWORD SizeToRead;
		// See if we have some data to read
		PeekNamedPipe(StdOutRead, NULL, 0, NULL, &SizeToRead, NULL);
		while (SizeToRead > 0)
		{
			// read some output
			DWORD SizeRead;
			ReadFile(StdOutRead, &Buffer, min(SizeToRead, BufferSize - 1), &SizeRead, NULL);
			Buffer[SizeRead] = 0;

			// decrease how much we need to read
			SizeToRead -= SizeRead;

			// append the output to the 
			strncpy(Errors, Buffer, ErrorsSize - 1);
		}

		// when the process signals, its done
		if (Reason == WAIT_OBJECT_0)
		{
			// berak out of the loop
			bProcessComplete = true;
		}
	}

	// Get the return value
	DWORD ErrorCode;
	GetExitCodeProcess(ProcInfo.hProcess, &ErrorCode);

	// pass back the return code if desired
	if (ProcessReturnValue)
	{
		*ProcessReturnValue = ErrorCode;
	}

	// Close process and thread handles. 
	CloseHandle(ProcInfo.hProcess);
	CloseHandle(ProcInfo.hThread);
	CloseHandle(StdOutRead);
	CloseHandle(StdOutWrite);

	return true;
}
