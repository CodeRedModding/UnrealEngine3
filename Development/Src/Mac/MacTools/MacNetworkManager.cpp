/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "MacNetworkManager.h"

/**
 * Gets an CMacTarget from a TARGETHANDLE
 */
CMacTarget* FMacNetworkManager::ConvertTarget( TARGETHANDLE Handle )
{
	TargetPtr Target = GetTarget(Handle);

	if( Target)
	{
		return (CMacTarget*)(Target.GetHandle());
	}

	return NULL;
}

/**
 * Gets an CIPhoneTarget from a sockaddr_in
 */
CMacTarget* FMacNetworkManager::ConvertTarget( const sockaddr_in &Address )
{
	TargetPtr Target = GetTarget(Address);

	if( Target)
	{
		return (CMacTarget*)(Target.GetHandle());
	}

	return NULL;
}

/**
 * Gets an CMacTarget from a TargetPtr
 */
CMacTarget* FMacNetworkManager::ConvertTarget( TargetPtr InTarget )
{
	return (CMacTarget*)(InTarget.GetHandle());
}

/**
 * Initalizes sock and the FConsoleNetworkManager instance.
 */
void FMacNetworkManager::Initialize()
{
	FConsoleNetworkManager::Initialize();

	// open a file in the Binaries\Mac directory that lists non-broadcastable targets
	FILE* TargetFile = fopen("Mac\\MacSubnet.txt", "r");
	if (!TargetFile)
	{
		TargetFile = fopen("..\\Mac\\MacSubnet.txt", "r");
	}

	// read in the file if it exists
	if (TargetFile)
	{
		// just a line which is the subnet to search in
		char Line[1024];
		// make sure we get a valid IP addr
		if (fgets(Line, 1023, TargetFile) != NULL && inet_addr(Line) != INADDR_NONE)
		{
			SetSubnetSearch(Line);
		}
	}
}

/**
 * Sets up the target with the information it needs
 */
void FMacNetworkManager::SetupTarget( TargetPtr InTarget, const char* CompName, const char* GameName, const char* GameType )
{
	FConsoleNetworkManager::SetupTarget( InTarget, CompName, GameName, GameType );

	CMacTarget* Target = ConvertTarget( InTarget );
	if ( Target )
	{
		Target->OSVersion = L"<remote>";
	}
}

/**
 * Forces a stub target to be created to await connection
 *
 * @Param TargetAddress IP of target to add
 *
 * @returns Handle of new stub target
 */
TARGETHANDLE FMacNetworkManager::ForceAddTarget( const wchar_t* TargetAddress )
{
	FConsoleNetworkManager::ForceAddTarget( TargetAddress );

	char AnsiTargetAddress[32] = { 0 };
	sockaddr_in SocketAddress = { 0 };
	WideCharToMultiByte( CP_ACP, 0, TargetAddress, ( int )wcslen( TargetAddress ), AnsiTargetAddress, ( int )wcslen( TargetAddress ), NULL, FALSE );
	SocketAddress.sin_family = AF_INET;
	SocketAddress.sin_addr.s_addr = inet_addr( AnsiTargetAddress );
	SocketAddress.sin_port = htons( 13650 );

	CMacTarget* Target = CreateTarget( &SocketAddress );
	Target->Name = TargetAddress;
	Target->ComputerName = TargetAddress;
	Target->GameName = L"";
	Target->OSVersion = L"<remote>";
	Target->Configuration = GetConfiguration();
	Target->GameTypeName = L"";
	Target->GameType = EGameType_Unknown;
	Target->PlatformType = GetPlatform();

	TargetPtr NewTarget( Target );
	TargetsLock.Lock();
	Targets[NewTarget.GetHandle()] = NewTarget;
	TargetsLock.Unlock();

	return( NewTarget.GetHandle() );
}