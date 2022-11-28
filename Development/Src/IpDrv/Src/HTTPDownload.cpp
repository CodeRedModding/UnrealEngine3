/*=============================================================================
	HTTPDownload.cpp: Unreal HTTP File Download
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter
=============================================================================*/

#include "UnIpDrv.h"
#include "HTTPDownload.h"

/*-----------------------------------------------------------------------------
	UHTTPDownload implementation.
-----------------------------------------------------------------------------*/
UHTTPDownload::UHTTPDownload()
{
	// Init.
	ServerSocket = INVALID_SOCKET;
	ResolveInfo = NULL;
	HTTPState = HTTP_Initialized;
}
void UHTTPDownload::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << ReceivedData << Headers << DownloadURL;
}
void UHTTPDownload::StaticConstructor()
{
	// Config.
	new(GetClass(),TEXT("ProxyServerHost"),		RF_Public)UStrProperty(CPP_PROPERTY(ProxyServerHost		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("ProxyServerPort"),		RF_Public)UIntProperty(CPP_PROPERTY(ProxyServerPort		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("RedirectToURL"),		RF_Public)UStrProperty(CPP_PROPERTY(DownloadParams		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("UseCompression"),		RF_Public)UBoolProperty(CPP_PROPERTY(UseCompression		), TEXT("Settings"), CPF_Config );
}
void UHTTPDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression )
{
	UDownload::ReceiveFile( InConnection, InPackageIndex );

	if( !*Params )
		return;
	IsCompressed = InCompression;
	FPackageInfo& Info = Connection->PackageMap->List( PackageIndex );
	FURL Base(NULL, TEXT(""), TRAVEL_Absolute);
	Base.Port = 80;
	if( IsCompressed )
		DownloadURL = FURL(&Base, *(FString(Params)+(*Info.URL)+COMPRESSED_EXTENSION), TRAVEL_Relative);
	else
		DownloadURL = FURL(&Base, *(FString(Params)+*Info.URL), TRAVEL_Relative);

	const TCHAR *h = (*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host;
	DWORD addr = inet_addr(TCHAR_TO_ANSI(h));
	if( addr==INADDR_NONE )
	{
		ResolveInfo = new FResolveInfo(h);
		HTTPState = HTTP_Resolving;
	}
	else
	{
		ServerAddr.sin_addr	= *((in_addr*)&addr);
		debugf( TEXT("Decoded IP %s (%s)"), h, *IpString(ServerAddr.sin_addr) );
		HTTPState = HTTP_Resolved;	
	}
	// Show progress while connecting.
	ReceiveData( NULL, 0 );
}
void UHTTPDownload::Tick(void)
{
	switch( HTTPState )
	{
	case HTTP_Resolving:
		check(ResolveInfo);
		// If destination address isn't resolved yet, send nowhere.
		if( !ResolveInfo->Resolved() )
		{
			// Host name still resolving.
			break;
		}
		else if( ResolveInfo->GetError() )
		{
			// Host name resolution just now failed.
			DownloadError( *FString::Printf( *LocalizeError( TEXT("InvalidUrl"), TEXT("Engine") ), (*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host ) );
			HTTPState  = HTTP_Closed;
			delete ResolveInfo;
			ResolveInfo = NULL;
			return;
		}
		else
		{
			// Host name resolution just now succeeded.
			debugf( TEXT("Resolved %s (%s)"), ResolveInfo->GetHostName(), *IpString(ResolveInfo->GetAddr()) );
			ServerAddr.sin_addr	= ResolveInfo->GetAddr();
			delete ResolveInfo;
			ResolveInfo = NULL;
			HTTPState = HTTP_Resolved;
		}
		break;
	case HTTP_Resolved:
		{
			ServerAddr.sin_family	= AF_INET;
			ServerAddr.sin_port		= htons( (*(*ProxyServerHost)) ? ProxyServerPort : DownloadURL.Port );

			// Connect	
			ServerSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
			SetSocketReuseAddr( ServerSocket );
			if( ServerSocket == INVALID_SOCKET )
			{
				DownloadError( *LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
				return;
			}
			SetSocketLinger( ServerSocket );
			LocalAddr.sin_family      = AF_INET;
			LocalAddr.sin_addr        = getlocalbindaddr( *GLog );
			if( !bindnextport( ServerSocket, &LocalAddr, 20, 1 ) )
			{
				debugf( NAME_DevNet, TEXT("HTTPDownload: bind() failed") );
				DownloadError( *LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
				return;
			}
			SetNonBlocking( ServerSocket );
			INT err = connect( ServerSocket, (SOCKADDR*) &ServerAddr, sizeof(ServerAddr) );
			if ( err == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK )
			{
				debugf( NAME_DevNet, TEXT("HTTPDownload: connect() failed") );
				DownloadError( *LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
				return;
			}
			ConnectStartTime = appSeconds();
			HTTPState  = HTTP_Connecting;
		}
		break;
	case HTTP_Connecting:
		INT err;
		{
			fd_set SocketSet;
			TIMEVAL SelectTime = {0, 0};
			FD_ZERO( &SocketSet );
			FD_SET( ServerSocket, &SocketSet );
			// Check for writability.  If the socket is writable, the
			// connection attempt succeeded.
			err = select( ServerSocket + 1, 0, &SocketSet, 0, &SelectTime);
		}
		if ( err == SOCKET_ERROR )
		{
			debugf( NAME_DevNet, TEXT("HTTPDownload: select() failed") );
			DownloadError( *LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
			HTTPState  = HTTP_Closed;
			return;
		}
		else if ( err == 0 )
		{
			if( appSeconds() - ConnectStartTime > 30 )
			{
				debugf( NAME_DevNet, TEXT("HTTPDownload: connection timed out") );
				DownloadError( *LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
			}
			//!! Still connecting - timeout?
			return;
		}
		// Socket is writable, so we are connected.  Send HTTP request.
		{
			FString Request = FString::Printf
			(
				TEXT("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Unreal\r\nConnection: close\r\n\r\n"),
				(*(*ProxyServerHost)) ? *DownloadURL.String() : *(FString(TEXT("/"))+DownloadURL.Map),
				(*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host
			);
			send( ServerSocket, TCHAR_TO_ANSI(*Request), Request.Len(), 0 );
		}
		HTTPState = HTTP_ReceivingHeader;
		break;
	case HTTP_ReceivingHeader:
		{
			FetchData();
			for( INT i=0;i<ReceivedData.Num();i++ )
			{
				if( ReceivedData(i)==13 || ReceivedData(i)==10 )
				{				
					UBOOL EOH = i==0;
					ReceivedData(i) = 0;
					Headers(Headers.AddZeroed()) = FString((char*)&ReceivedData(0));
					if( i<ReceivedData.Num()-1 && (ReceivedData(i+1)==13 || ReceivedData(i+1)==10) )
						i++;					
					ReceivedData.Remove(0, i+1);
					if( EOH ) // end of headers
					{
						// check we got a 200 OK.
						if( Headers(0).Mid(Headers(0).InStr(TEXT(" ")), 5) != TEXT(" 200 "))
						{
							DownloadError( *FString::Printf( *LocalizeError( TEXT("InvalidUrl"), TEXT("Engine") ), *DownloadURL.String() ) );
							HTTPState  = HTTP_Closed;
							return;
						}
						if( !IsCompressed )
						{
							for( i=0; i<Headers.Num();i++ )
							{
								if( Headers(i).Caps().Left(16) == TEXT("CONTENT-LENGTH: ") &&
									appAtoi( *Headers(i).Mid(16) ) != Info->FileSize )
								{
									DownloadError( *LocalizeError( TEXT("NetSize"), TEXT("Engine")) );
									HTTPState  = HTTP_Closed;
									return;
								}
							}
						}
						HTTPState = HTTP_ReceivingData;
						break;
					}
					FetchData();
					i = -1;
				}
			}
		}
		break;
	case HTTP_ReceivingData:
		while( ReceivedData.Num() || FetchData() )
		{
			if( ReceivedData.Num() > 0 )
			{
				INT Count = Transfered + ReceivedData.Num() > Info->FileSize ? Info->FileSize - Transfered : ReceivedData.Num();
				if( Count > 0 )
					ReceiveData( &ReceivedData(0), Count );
				else
					HTTPState = HTTP_Closed;
				ReceivedData.Empty();
			}
		}
		break;
	case HTTP_Closed:
		DownloadDone();
		return;
	}
}
UBOOL UHTTPDownload::FetchData()
{
	BYTE Buf[1024];
	INT Bytes = recv( ServerSocket, (char *)Buf, 1024, 0 );
	if( Bytes == 0 )
	{
		// Close received.
		HTTPState = HTTP_Closed;
		return 0;
	}
	if( Bytes != SOCKET_ERROR )
	{
		appMemcpy( &ReceivedData(ReceivedData.Add(Bytes)), Buf, Bytes );
		return 1;
	}
	if( Bytes == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK )
		return 0; // No data

	debugf( NAME_DevNet, TEXT("HTTPDownload: recv() failed") );
	DownloadError( *LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
	HTTPState  = HTTP_Closed;
	return 0;
}
UBOOL UHTTPDownload::TrySkipFile()
{
	if( Super::TrySkipFile() )
	{
		HTTPState = HTTP_Closed;
		Connection->Logf( TEXT("SKIP GUID=%s"), *Info->Guid.String() );
		return 1;
	}
	return 0;
}
void UHTTPDownload::Destroy()
{
	if( ServerSocket != INVALID_SOCKET )
	{
		closesocket( ServerSocket );
		ServerSocket = INVALID_SOCKET;
	}
	Super::Destroy();
}
IMPLEMENT_CLASS(UHTTPDownload)
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

