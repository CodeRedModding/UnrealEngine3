/*=============================================================================
	UnPenLev.cpp: Unreal pending level class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	UPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UPendingLevel::UPendingLevel( UEngine* InEngine, const FURL& InURL )
:	ULevelBase( InEngine, InURL )
{}
IMPLEMENT_CLASS(UPendingLevel);


/*-----------------------------------------------------------------------------
	UNetPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UNetPendingLevel::UNetPendingLevel( UEngine* InEngine, const FURL& InURL )
:	UPendingLevel( InEngine, InURL )
{
	// Init.
	Error     = TEXT("");
	NetDriver = NULL;

	// Try to create network driver.
	UClass* NetDriverClass = StaticLoadClass( UNetDriver::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.NetworkDevice"), NULL, LOAD_NoFail, NULL );
	NetDriver = ConstructObject<UNetDriver>( NetDriverClass );
	check(NetDriver);
	if( NetDriver->InitConnect( this, URL, Error ) )
	{
		// Send initial message.
		NetDriver->ServerConnection->Logf( TEXT("HELLO REVISION=0 MINVER=%i VER=%i"), GEngineMinNetVersion, GEngineVersion );
		NetDriver->ServerConnection->FlushNet();
	}
	else
	{
		delete NetDriver;
		NetDriver=NULL;
	}

}

//
// FNetworkNotify interface.
//
EAcceptConnection UNetPendingLevel::NotifyAcceptingConnection()
{
	return ACCEPTC_Reject;
}
void UNetPendingLevel::NotifyAcceptedConnection( class UNetConnection* Connection )
{
}
UBOOL UNetPendingLevel::NotifyAcceptingChannel( class UChannel* Channel )
{
	return 0;
}
ULevel* UNetPendingLevel::NotifyGetLevel()
{
	return NULL;
}
void UNetPendingLevel::NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )
{
	check(Connection==NetDriver->ServerConnection);
	debugf( NAME_DevNet, TEXT("PendingLevel received: %s"), Text );

	// This client got a response from the server.
	if( ParseCommand(&Text,TEXT("UPGRADE")) )
	{
		// Report mismatch.
		INT RemoteMinVer=0, RemoteVer=0;
		Parse( Text, TEXT("MINVER="), RemoteMinVer );
		Parse( Text, TEXT("VER="),    RemoteVer    );
		if( GEngineVersion < RemoteMinVer )
		{
			// Upgrade message.
			Engine->SetProgress( TEXT(""), TEXT(""), -1.f );
		}
		else
		{
			// Downgrade message.
			Engine->SetProgress( *LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), *LocalizeError(TEXT("ServerOutdated"),TEXT("Engine")), 6.f );//!!localize
		}
	}
	else if( ParseCommand(&Text,TEXT("FAILURE")) )
	{
		// Report problem to user.
		Engine->SetProgress( TEXT("Rejected By Server"), Text, 10.f );
	}
	else if( ParseCommand(&Text,TEXT("FAILCODE")) )
	{
#if VIEWPORT_ACTOR_DISABLED
		// Notify console.
		if( Engine->Client &&
			Engine->Client->Viewports(0) &&
			Engine->Client->Viewports(0)->Console )
		{
			FURL NewURL(URL);

			for( INT i=NewURL.Op.Num()-1; i>=0; i-- )
				if( NewURL.Op(i).Left(9).Caps() ==TEXT("PASSWORD=") )
					NewURL.Op.Remove( i );

// @@FixMeJoe			Engine->Client->Viewports(0)->Console->eventConnectFailure( Text, *NewURL.String() );
		}
#endif
	}
	else if( ParseCommand( &Text, TEXT("USES") ) )
	{
		// Dependency information.
		FPackageInfo& Info = *new(Connection->PackageMap->List)FPackageInfo(NULL);
		TCHAR PackageName[NAME_SIZE]=TEXT("");
		Parse( Text, TEXT("GUID=" ), Info.Guid );
		Parse( Text, TEXT("GEN=" ),  Info.RemoteGeneration );
		Parse( Text, TEXT("SIZE="),  Info.FileSize );
		Parse( Text, TEXT("FLAGS="), Info.PackageFlags );
		Parse( Text, TEXT("PKG="), PackageName, ARRAY_COUNT(PackageName) );
		Parse( Text, TEXT("FNAME="), Info.URL );
		Info.Parent = CreatePackage(NULL,PackageName);
	}
	else if( ParseCommand(&Text,TEXT("USERFLAG")) )
	{
		Connection->UserFlags = appAtoi(Text);
	}
	else if( ParseCommand( &Text, TEXT("CHALLENGE") ) )
	{
		// Challenged by server.
		INT i=0;
		Parse( Text, TEXT("VER="), Connection->NegotiatedVer );
		Parse( Text, TEXT("CHALLENGE="), Connection->Challenge );

		FURL PartialURL(URL);
		PartialURL.Host = TEXT("");
		for( i=URL.Op.Num()-1; i>=0; i-- )
			if( URL.Op(i).Left(5)==TEXT("game=") )
				URL.Op.Remove( i );
		NetDriver->ServerConnection->Logf( TEXT("NETSPEED %i"), Connection->CurrentNetSpeed );
		NetDriver->ServerConnection->Logf( TEXT("LOGIN RESPONSE=%i URL=%s"), Engine->ChallengeResponse(Connection->Challenge), *PartialURL.String() );
		NetDriver->ServerConnection->FlushNet();
	}
	else if( ParseCommand( &Text, TEXT("DLMGR") ) )
	{
		INT i = Connection->DownloadInfo.AddZeroed();
		Parse( Text, TEXT("CLASS="), Connection->DownloadInfo(i).ClassName );
		Parse( Text, TEXT("PARAMS="), Connection->DownloadInfo(i).Params );
		ParseUBOOL( Text, TEXT("COMPRESSION="), Connection->DownloadInfo(i).Compression );
		Connection->DownloadInfo(i).Class = StaticLoadClass( UDownload::StaticClass(), NULL, *Connection->DownloadInfo(i).ClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
		if( !Connection->DownloadInfo(i).Class )
			Connection->DownloadInfo.Remove(i);
	}
	else if( ParseCommand( &Text, TEXT("WELCOME") ) )
	{
		// Server accepted connection.
		debugf( NAME_DevNet, TEXT("Welcomed by server: %s"), Text );

		// Parse welcome message.
		Parse( Text, TEXT("LEVEL="), URL.Map );
		ParseUBOOL( Text, TEXT("LONE="), LonePlayer );
		Parse( Text, TEXT("CHALLENGE="), Connection->Challenge );

		// Make sure all packages we need are downloadable.
		for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
		{
			FString Filename;
			FPackageInfo& Info = Connection->PackageMap->List(i);
			if( !GPackageFileCache->FindPackageFile( Info.Parent->GetName(), &Info.Guid, Filename ) )
			{
				Filename = FString(Info.Parent->GetName()) + DLLEXT;
				if( GFileManager->FileSize(*Filename) <= 0 )
				{
					// We need to download this package.
					FilesNeeded++;
					Info.PackageFlags |= PKG_Need;
					if( !NetDriver->AllowDownloads || !(Info.PackageFlags & PKG_AllowDownload) )
					{
						Error = FString::Printf( TEXT("Downloading '%s' not allowed"), Info.Parent->GetName() );
						NetDriver->ServerConnection->State = USOCK_Closed;
						return;
					}
				}
			}
		}

		// Send first download request.
		ReceiveNextFile( Connection );

		// We have successfully connected.
		Success = 1;
	}
	else
	{
		// Other command.
	}
}
UBOOL UNetPendingLevel::TrySkipFile()
{
	return NetDriver->ServerConnection->Download && NetDriver->ServerConnection->Download->TrySkipFile();
}
void UNetPendingLevel::NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* InError, UBOOL Skipped )
{
	check(Connection->PackageMap->List.IsValidIndex(PackageIndex));

	// Map pack to package.
	FPackageInfo& Info = Connection->PackageMap->List(PackageIndex);
	if( *InError )
	{
		if( Connection->DownloadInfo.Num() > 1 )
		{
			// Try with the next download method.
			Connection->DownloadInfo.Remove(0);
			ReceiveNextFile( Connection );
		}
		else
		{
			// If transfer failed, so propagate error.
			if( Error==TEXT("") )
				Error = FString::Printf( *LocalizeError(TEXT("DownloadFailed"),TEXT("Engine")), Info.Parent->GetName(), InError );
		}
	}
	else
	{
		// Now that a file has been successfully received, mark its package as downloaded.
		check(Connection==NetDriver->ServerConnection);
		check(Info.PackageFlags&PKG_Need);
		Info.PackageFlags &= ~PKG_Need;
		FilesNeeded--;
		if( Skipped )
			Connection->PackageMap->List.Remove( PackageIndex );
		// Send next download request.
		ReceiveNextFile( Connection );
	}
}
void UNetPendingLevel::ReceiveNextFile( UNetConnection* Connection )
{
	for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
		if( Connection->PackageMap->List(i).PackageFlags & PKG_Need )
			{Connection->ReceiveFile( i ); return;}
	if( Connection->Download )
		delete Connection->Download;
}

UBOOL UNetPendingLevel::NotifySendingFile( UNetConnection* Connection, FGuid Guid )
{
	// Server has requested a file from this client.
	debugf( NAME_DevNet, *LocalizeError(TEXT("RequestDenied"),TEXT("Engine")) );
	return 0;

}

//
// Update the pending level's status.
//
void UNetPendingLevel::Tick( FLOAT DeltaTime )
{
	check(NetDriver);
	check(NetDriver->ServerConnection);

	// Handle timed out or failed connection.
	if( NetDriver->ServerConnection->State==USOCK_Closed && Error==TEXT("") )
	{
		Error = LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine"));
		return;
	}

	// Update network driver.
	NetDriver->TickDispatch( DeltaTime );
	NetDriver->TickFlush();

}
//
// Send JOIN to other end
//
void UNetPendingLevel::SendJoin()
{
	SentJoin = 1;
	NetDriver->ServerConnection->Logf( TEXT("JOIN") );
	NetDriver->ServerConnection->FlushNet();
}
IMPLEMENT_CLASS(UNetPendingLevel);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

