/*=============================================================================
	WintabSupport.h - support for tablet pressure sensitivty
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _WINTAB_SUPPORT_H_
#define _WINTAB_SUPPORT_H_

#include "../../External/wintab/Include/msgpack.h"
#define NOFIX32
#include "../../External/wintab/Include/wintab.h"

#define PACKETNAME		PRESSURE_
#define PRESSURE_PACKETDATA	PK_NORMAL_PRESSURE
#define PRESSURE_PACKETMODE	PK_BUTTONS
#include "../../External/wintab/Include/pktdef.h"


typedef UINT (WINAPI * WTINFOA) (UINT, UINT, LPVOID);
typedef HCTX (WINAPI * WTOPENA)(HWND, LPLOGCONTEXTA, BOOL);
typedef BOOL (WINAPI * WTGETA) (HCTX, LPLOGCONTEXT);
typedef BOOL (WINAPI * WTSETA) (HCTX, LPLOGCONTEXT);
typedef BOOL (WINAPI * WTCLOSE) (HCTX);
typedef BOOL (WINAPI * WTENABLE) (HCTX, BOOL);
typedef BOOL (WINAPI * WTPACKET) (HCTX, UINT, LPVOID);
typedef BOOL (WINAPI * WTOVERLAP) (HCTX, BOOL);
typedef BOOL (WINAPI * WTSAVE) (HCTX, LPVOID);
typedef BOOL (WINAPI * WTCONFIG) (HCTX, HWND);
typedef HCTX (WINAPI * WTRESTORE) (HWND, LPVOID, BOOL);
typedef BOOL (WINAPI * WTEXTSET) (HCTX, UINT, LPVOID);
typedef BOOL (WINAPI * WTEXTGET) (HCTX, UINT, LPVOID);
typedef BOOL (WINAPI * WTQUEUESIZESET) (HCTX, int);
typedef int  (WINAPI * WTDATAPEEK) (HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  (WINAPI * WTPACKETSGET) (HCTX, int, LPVOID);

struct FWinTab
{
	WTINFOA WTInfoA;
	WTOPENA WTOpenA;
	WTGETA WTGetA;
	WTSETA WTSetA;
	WTCLOSE WTClose;
	WTPACKET WTPacket;
	WTENABLE WTEnable;
	WTOVERLAP WTOverlap;
	WTSAVE WTSave;
	WTCONFIG WTConfig;
	WTRESTORE WTRestore;
	WTEXTSET WTExtSet;
	WTEXTGET WTExtGet;
	WTQUEUESIZESET WTQueueSizeSet;
	WTDATAPEEK WTDataPeek;
	WTPACKETSGET WTPacketsGet;
		

#pragma warning( push )
#pragma warning( disable: 4191 ) // unsafe conversion from 'type of expression' to 'type required'

	static FWinTab* Init()
	{
		FWinTab* WinTab = new FWinTab;

		WinTab->WintabDLLHandle = LoadLibrary(TEXT("Wintab32.dll"));

		if( !WinTab->WintabDLLHandle )
		{
			delete WinTab;
			return NULL;
		}

		WinTab->WTInfoA = (WTINFOA)GetProcAddress(WinTab->WintabDLLHandle,"WTInfoA");
		WinTab->WTOpenA = (WTOPENA)GetProcAddress(WinTab->WintabDLLHandle,"WTOpenA");
		WinTab->WTGetA = (WTGETA)GetProcAddress(WinTab->WintabDLLHandle,"WTGetA");
		WinTab->WTSetA = (WTSETA)GetProcAddress(WinTab->WintabDLLHandle,"WTSetA");
		WinTab->WTClose = (WTCLOSE)GetProcAddress(WinTab->WintabDLLHandle,"WTClose");
		WinTab->WTPacket = (WTPACKET)GetProcAddress(WinTab->WintabDLLHandle,"WTPacket");
		WinTab->WTEnable = (WTENABLE)GetProcAddress(WinTab->WintabDLLHandle,"WTEnable");
		WinTab->WTOverlap = (WTOVERLAP)GetProcAddress(WinTab->WintabDLLHandle,"WTOverlap");
		WinTab->WTSave = (WTSAVE)GetProcAddress(WinTab->WintabDLLHandle,"WTSave");
		WinTab->WTConfig = (WTCONFIG)GetProcAddress(WinTab->WintabDLLHandle,"WTConfig");
		WinTab->WTRestore = (WTRESTORE)GetProcAddress(WinTab->WintabDLLHandle,"WTRestore");
		WinTab->WTExtSet = (WTEXTSET)GetProcAddress(WinTab->WintabDLLHandle,"WTExtSet");
		WinTab->WTExtGet = (WTEXTGET)GetProcAddress(WinTab->WintabDLLHandle,"WTExtGet");
		WinTab->WTQueueSizeSet = (WTQUEUESIZESET)GetProcAddress(WinTab->WintabDLLHandle,"WTQueueSizeSet");
		WinTab->WTDataPeek = (WTDATAPEEK)GetProcAddress(WinTab->WintabDLLHandle,"WTDataPeek");
		WinTab->WTPacketsGet = (WTPACKETSGET)GetProcAddress(WinTab->WintabDLLHandle,"WTPacketsGet");

		if( WinTab->WTInfoA &&
			WinTab->WTOpenA &&
			WinTab->WTGetA &&
			WinTab->WTSetA &&
			WinTab->WTClose &&
			WinTab->WTPacket &&
			WinTab->WTEnable &&
			WinTab->WTOverlap &&
			WinTab->WTSave &&
			WinTab->WTConfig &&
			WinTab->WTRestore &&
			WinTab->WTExtSet &&
			WinTab->WTExtGet &&
			WinTab->WTQueueSizeSet &&
			WinTab->WTDataPeek &&
			WinTab->WTPacketsGet )
		{
			return WinTab;
		}
		else
		{
			delete WinTab;
			return NULL;
		}
	}

#pragma warning( pop )

	~FWinTab()
	{
		if( WintabDLLHandle )
		{
			FreeLibrary( WintabDLLHandle );
		}
	}

private:

	FWinTab()
		:	WintabDLLHandle(NULL)
		,	WTInfoA(NULL)
		,	WTOpenA(NULL)
		,	WTGetA(NULL)
		,	WTSetA(NULL)
		,	WTClose(NULL)
		,	WTPacket(NULL)
		,	WTEnable(NULL)
		,	WTOverlap(NULL)
		,	WTSave(NULL)
		,	WTConfig(NULL)
		,	WTRestore(NULL)
		,	WTExtSet(NULL)
		,	WTExtGet(NULL)
		,	WTQueueSizeSet(NULL)
		,	WTDataPeek(NULL)
		,	WTPacketsGet(NULL)
	{}

	HMODULE WintabDLLHandle;
};

extern FWinTab* GWinTab;

#endif /* _WINTAB_SUPPORT_H_ */