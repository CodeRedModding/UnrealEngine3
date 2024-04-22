//////////////////////////////////////////////////////////////////////////
// This file Loads the IPC/Socket dll to communicate with the IDE

#ifndef __WTDEBUGGERINTERFACE_H_
#define __WTDEBUGGERINTERFACE_H_

#define GETPROCADDRESS GetProcAddress


#include "WTGlobals.h"

static const int nFringeSupportVersion = 2;

typedef DWORD (*OnCommandToUCProto)(INT, LPCWSTR);
typedef DWORD (*OnCommandToVSProto)(INT, DWORD, DWORD, LPCWSTR, LPCWSTR);
typedef void (*SetCallbackProtoUC)(OnCommandToUCProto);
typedef void (*SetCallbackProtoVS)(OnCommandToVSProto);
typedef void (*NotifyBeginTickProto)();
typedef UINT (*NotifyDebugInfoProto)(UINT*);
typedef void (*nFringeSupportProto)(INT);

DWORD OnCommandToUC(INT cmdId, LPCWSTR cmdStr);
extern OnCommandToVSProto IPCSendCommandToVS;
extern NotifyBeginTickProto IPCNotifyBeginTick;
extern NotifyDebugInfoProto IPCNotifyDebugInfo;
extern nFringeSupportProto IPCnFringeSupport;
class IPC_UC
{
	HMODULE hLib;
public:
#pragma warning(push)
#pragma warning(disable:4191) // disable: unsafe conversion from 'type of expression' to 'type required'
	IPC_UC()
		: hLib(NULL)
	{
	}

	~IPC_UC()
	{
		SendCommandToVS(CMD_GameEnded);
		Sleep(500);
		if ( hLib != NULL )
		{
			IPCSendCommandToVS = NULL;
			IPCNotifyBeginTick = NULL;
			IPCNotifyDebugInfo = NULL;
			IPCnFringeSupport = NULL;
			FreeLibrary(hLib);
		}
		hLib = NULL;
	}

	void Load()
	{
		hLib = LoadLibraryW(g_WTGlobals.WT_INTERFACEDLL);
		SetCallbackProtoUC SetCallback = (SetCallbackProtoUC) GETPROCADDRESS(hLib, "IPCSetCallbackUC");
		IPCNotifyBeginTick = (NotifyBeginTickProto) GETPROCADDRESS(hLib, "IPCNotifyBeginTick");
		IPCNotifyDebugInfo = (NotifyDebugInfoProto) GETPROCADDRESS(hLib, "IPCNotifyDebugInfo");
		IPCnFringeSupport = (nFringeSupportProto) GETPROCADDRESS(hLib, "IPCnFringeSupport");
		IPCSendCommandToVS = (OnCommandToVSProto) GETPROCADDRESS(hLib, "IPCSendCommandToVS");

		if ( IPCnFringeSupport )
		{
			IPCnFringeSupport(nFringeSupportVersion);
		}

		if ( SetCallback )
		{
			SetCallback(OnCommandToUC);
			/* Allow time for the pipe from Visual Studio to be established. We don't want to block or
			 * the game will simply not load with the -vadebug flag, which eliminates the ability to
			 * use the Attach To Process feature. If the connection is not established within this time,
			 * the worst thing that happens is breakpoints will not be set until the first tick.
			 */
			Sleep(1000);
		}

		if (GDebugger)
		{
			GDebugger->NotifyBeginTick();
		}
	}

	UBOOL IsLoaded() const
	{
		return hLib != NULL;
	}
#pragma warning(pop)

	DWORD SendCommandToVS( INT cmdId, DWORD dw1 = 0, DWORD dw2 = 0, LPCWSTR s1 = NULL, LPCWSTR s2 = NULL )
	{
		return IPCSendCommandToVS ? IPCSendCommandToVS(cmdId, dw1, dw2, s1, s2) : 0;
	}
	DWORD SendCommandToVS( INT cmdId, LPCWSTR s1, LPCWSTR s2 = NULL )
	{
		return SendCommandToVS(cmdId, 0, 0, s1, s2);
	}
	void Close()
	{
	}
};

#endif