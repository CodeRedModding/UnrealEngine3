#pragma once
enum {
	CMD_ShowDllForm,
	CMD_EditorCommand,
	CMD_EditorLoadTextBuffer,
	CMD_AddClassToHierarchy,
	CMD_ClearHierarchy,
	CMD_BuildHierarchy,
	CMD_ClearWatch,
	CMD_AddWatch,
	CMD_SetCallback,
	CMD_AddBreakpoint,
	CMD_RemoveBreakpoint,	
	CMD_EditorGotoLine,
	CMD_AddLineToLog,
	CMD_EditorLoadClass,
	CMD_CallStackClear,
	CMD_CallStackAdd,
	CMD_DebugWindowState,
	CMD_ClearAWatch,
	CMD_AddAWatch,
	CMD_LockList,
	CMD_UnlockList,
	CMD_SetCurrentObjectName,
	CMD_GameEnded
};
#include <direct.h>

#if USE_SECURE_CRT
#define FOPENW _wfopen_s
#else
#define FOPENW _wfopen
#endif
#define SPRINTFW wsprintfW
#define FPUTSW fputws
#define FFLUSH fflush
#define FCLOSE fclose
#define FSCANFW fwscanf
#define TOLOWER tolower

#ifndef ASSERT
#define ASSERT(x)
#endif

class WTStr
{
public:
	static int WStrCmpI(LPCWSTR s1, LPCWSTR s2)
	{
		for(int i = 0; s1[i] && s2[i]; i++)
			if(TOLOWER(s2[i]) != TOLOWER(s1[i]))
				return s2[i] - s1[i];
		return 0;
	}
	static LPCWSTR WStrStrI(LPCWSTR str, LPCWSTR substr)
	{
		for(int i = 0; str[i]; i++)
			if(!WStrCmpI(&str[i], substr))
				return &str[i];
		return NULL;
	}
	static void WStrCpy(LPWSTR to, LPCWSTR from)
	{
		int i = 0;
		for(; from[i]; i++)
			to[i] = from[i];
		to[i] = '\0';
	}
	static void WStrCat(LPWSTR to, LPCWSTR appendStr)
	{
		int len = 0;
		for(; to[len]; len++); // get len
		WStrCpy(&to[len], appendStr);
	}
	static void GetFilePath(LPCWSTR file, LPWSTR path)
	{
		LPWSTR lastSlash = path;
		if(file[0] == '"')
			file++;
		for(int i = 0; i < 512 && file[i];i++)
		{
			path[i] = file[i];
			if(file[i] == '\\' || file[i] == '/')
				lastSlash = &path[i];
			if(file[i] == '"')
				break;
			if(file[i] == ' ' && file[i+1] == '-')
				break;
		}
		lastSlash[0] = '\0';

		// Get full path
		WCHAR full[MAX_PATH+1];
		LPWSTR fname;
		if(GetFullPathNameW(path, MAX_PATH, full, &fname)>0)
			WStrCpy(path, full);
	}
};

class WTGlobals : public WTStr
{
	FILE *fpLog;
public:
	WCHAR WT_DLLPATH[1024];
	WCHAR WT_INTERFACEDLL[1024];
	WCHAR WT_WATCHFILE[1024];
	WCHAR WT_GAMEPATH[1024];
	WCHAR WT_GAMESRCPATH[1024];
	WCHAR WT_TARGET[1024];
	WCHAR WT_PORT[1024];
	WCHAR WT_TMPDIR[1024];
	WTGlobals(LPCWSTR logname = NULL)
	{
		fpLog = NULL;
		GetTempPathW(MAX_PATH, WT_TMPDIR);
		InitValues();
		// OpenLog(logname);
	}
	void OpenLog(LPCWSTR logname = NULL)
	{
		if(fpLog)
			FCLOSE(fpLog);
		fpLog = NULL;
		if(WT_TMPDIR[0] && logname)
		{
			WCHAR logfile[1024];
			SPRINTFW(logfile, L"%s\\%s.log", WT_TMPDIR, logname);
#if USE_SECURE_CRT
			if( FOPENW(&fpLog,logfile, L"w") != 0 )
			{
				fpLog = NULL;
			}
#else
			fpLog = FOPENW(logfile, L"w");
#endif
			if(!fpLog)
				::MessageBoxW(NULL, logfile, L"Error opening logfile:", MB_OK);
		}

	}
	void Log(LPCWSTR txt, LPCWSTR optText = NULL)
	{
		if(fpLog && txt)
		{
			FPUTSW(txt, fpLog);
			if(optText)
			{
				FPUTSW(L", ", fpLog);
				FPUTSW(optText, fpLog);
			}
			FPUTSW(L"\n", fpLog);
			FFLUSH(fpLog);
		}

	}
	void InitValues()
	{
		int argc = 0;
		LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if(argv)
		{
			GetTempPathW(MAX_PATH, WT_TMPDIR);
			WStrCat(WT_TMPDIR, L"UCDebugger");
			_wmkdir(WT_TMPDIR);
			WStrCpy(WT_WATCHFILE, WT_TMPDIR);
			WStrCat(WT_WATCHFILE, L"\\WatchFile.txt");
			WStrCpy(WT_TARGET, L"localhost");
			WStrCpy(WT_PORT, L"8888");

			LPWSTR fname;
			if(GetFullPathNameW(argv[0], MAX_PATH, WT_DLLPATH, &fname)>0)
			{
				fname[0] = '\0';
				if(!WStrStrI(WT_DLLPATH, L"WTDebugger"))
					WStrCat(WT_DLLPATH, L"WTDebugger\\");
				WStrCpy(WT_INTERFACEDLL, WT_DLLPATH);
				WStrCat(WT_INTERFACEDLL, L"UCDebuggerSocket.dll");
				WStrCpy(WT_GAMESRCPATH, WT_DLLPATH);
				WStrCat(WT_GAMESRCPATH, L"..\\..\\Development\\Src\\");
			}
		}
	}
};
#define LOG g_WTGlobals.Log

extern WTGlobals g_WTGlobals;

