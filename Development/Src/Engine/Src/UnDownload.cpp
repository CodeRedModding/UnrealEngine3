/*=============================================================================
	UnDownload.cpp: Unreal file-download interface
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"
#include "FCodec.h"

BYTE* FCodecBWT::CompressBuffer;
INT   FCodecBWT::CompressLength;

/*-----------------------------------------------------------------------------
	UDownload implementation.
-----------------------------------------------------------------------------*/

void UDownload::StaticConstructor()
{
	DownloadParams = TEXT("");
	UseCompression = 0;
}
void UDownload::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Connection;
}
UBOOL UDownload::TrySkipFile()
{
	if( RecvFileAr && Info->PackageFlags & PKG_ClientOptional )
	{
		SkippedFile = 1;
		return 1;
	}
	return 0;
}
void UDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression )
{
	Connection = InConnection;
	PackageIndex = InPackageIndex;
	Info = &Connection->PackageMap->List( PackageIndex );
}
void UDownload::ReceiveData( BYTE* Data, INT Count )
{
	// Receiving spooled file data.
	if( Transfered==0 && !RecvFileAr )
	{
		// Open temporary file initially.
		debugf( NAME_DevNet, TEXT("Receiving package '%s'"), Info->Parent->GetName() );
		GFileManager->MakeDirectory( *GSys->CachePath, 0 );
		appCreateTempFilename( *GSys->CachePath, TempFilename );
		RecvFileAr = GFileManager->CreateFileWriter( TempFilename );
	}

	// Receive.
	if( !RecvFileAr )
	{
		// Opening file failed.
		DownloadError( *LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
	}
	else
	{
		if( Count > 0 )
			RecvFileAr->Serialize( Data, Count );
		if( RecvFileAr->IsError() )
		{
			// Write failed.
			DownloadError( *FString::Printf( *LocalizeError(TEXT("NetWrite"),TEXT("Engine")), TempFilename ) );
		}
		else
		{
			// Successful.
			Transfered += Count;
			FString Msg1 = FString::Printf( (Info->PackageFlags&PKG_ClientOptional)?*LocalizeProgress(TEXT("ReceiveOptionalFile"),TEXT("Engine")):*LocalizeProgress(TEXT("ReceiveFile"),TEXT("Engine")), Info->Parent->GetName() );
			FString Msg2 = FString::Printf( *LocalizeProgress(TEXT("ReceiveSize"),TEXT("Engine")), Info->FileSize/1024, 100.f*Transfered/Info->FileSize );
			Connection->Driver->Notify->NotifyProgress( *Msg1, *Msg2, 4.f );
		}
	}	
}
void UDownload::DownloadError( const TCHAR* InError )
{
	// Failure.
	appStrcpy( Error, InError );
}
void UDownload::DownloadDone()
{
	if( RecvFileAr )
	{
		delete RecvFileAr;
		RecvFileAr = NULL;
	}
	if( SkippedFile )
	{
		debugf( TEXT("Skipped download of '%s'"), Info->Parent->GetName() );
		GFileManager->Delete( TempFilename );
		TCHAR Msg[256];
		appSprintf( Msg, TEXT("Skipped '%s'"), Info->Parent->GetName() );
		Connection->Driver->Notify->NotifyProgress( TEXT("Success"), Msg, 4.f );
		Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, TEXT(""), 1 );
	}
	else
	{
		TCHAR Dest[256];
		appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("%s.uxx"), *GSys->CachePath, *Info->Guid.String() );
		if( !*Error && Transfered==0 )
			DownloadError( *FString::Printf( *LocalizeError(TEXT("NetRefused"),TEXT("Engine")), Info->Parent->GetName() ) );
		if( !*Error && IsCompressed )
		{
			TCHAR CFilename[256];
			appStrcpy( CFilename, TempFilename );
			appCreateTempFilename( *GSys->CachePath, TempFilename );
			FArchive* CFileAr = GFileManager->CreateFileReader( CFilename );
			FArchive* UFileAr = GFileManager->CreateFileWriter( TempFilename );
			if( !CFileAr || !UFileAr )
				DownloadError( *LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
			else
			{
				INT Signature;
				FString OrigFilename;
				*CFileAr << Signature;
				if( Signature != 5678 )
					DownloadError( *LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
				else
				{
					*CFileAr << OrigFilename;
					FCodecFull Codec;
					Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecBWT);
					Codec.AddCodec(new FCodecMTF);
					Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecHuffman);
					Codec.Decode( *CFileAr, *UFileAr );
				}
			}
			if( CFileAr )
			{
				GFileManager->Delete( CFilename );
				delete CFileAr;
			}
			if( UFileAr )
				delete UFileAr;
		}
		if( !*Error && GFileManager->FileSize(TempFilename)!=Info->FileSize )
			DownloadError( *LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
		if( !*Error && !GFileManager->Move( Dest, TempFilename ) )
			DownloadError( *LocalizeError(TEXT("NetMove"),TEXT("Engine")) );
		if( *Error )
		{
			GFileManager->Delete( TempFilename );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
		else
		{
			// Success.
			TCHAR Msg[256];
			TCHAR IniName[256];
			FConfigCacheIni CacheIni;
			appSprintf( IniName, TEXT("%s") PATH_SEPARATOR TEXT("cache.ini"), *GSys->CachePath );
			CacheIni.SetString( TEXT("Cache"), *Info->Guid.String(), *(*Info->URL) ? *Info->URL : Info->Parent->GetName(), IniName );

			appSprintf( Msg, TEXT("Received '%s'"), Info->Parent->GetName() );
			Connection->Driver->Notify->NotifyProgress( TEXT("Success"), Msg, 4.f );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
	}
}
void UDownload::Destroy()
{
	if( RecvFileAr )
	{
		delete RecvFileAr;
		RecvFileAr = NULL;
		GFileManager->Delete( TempFilename );
	}
	if( Connection && Connection->Download == this )
		Connection->Download = NULL;
	Connection = NULL;
	Super::Destroy();
}
IMPLEMENT_CLASS(UDownload)

/*-----------------------------------------------------------------------------
	UChannelDownload implementation.
-----------------------------------------------------------------------------*/

void UChannelDownload::StaticConstructor()
{
	DownloadParams = TEXT("Enabled");
}
void UChannelDownload::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << Ch;
}
UBOOL UChannelDownload::TrySkipFile()
{
	if( Ch && Super::TrySkipFile() )
	{
		FOutBunch Bunch( Ch, 1 );
		FString Cmd = TEXT("SKIP");
		Bunch << Cmd;
		Bunch.bReliable = 1;
		Ch->SendBunch( &Bunch, 0 );
		return 1;
	}
	return 0;
}
void UChannelDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression )
{
	UDownload::ReceiveFile( InConnection, InPackageIndex, Params, InCompression );

	// Create channel.
	Ch = (UFileChannel *)Connection->CreateChannel( CHTYPE_File, 1 );
	if( !Ch )
	{
		DownloadError( *LocalizeError(TEXT("ChAllocate"),TEXT("Engine")) );
		DownloadDone();
		return;
	}

	// Set channel properties.
	Ch->Download = this;
	Ch->PackageIndex = PackageIndex;

	// Send file request.
	FOutBunch Bunch( Ch, 0 );
	Bunch << Info->Guid;
	Bunch.bReliable = 1;
	check(!Bunch.IsError());
	Ch->SendBunch( &Bunch, 0 );
	
}
void UChannelDownload::Destroy()
{
	if( Ch && Ch->Download == this )
		Ch->Download = NULL;
	Ch = NULL;
	Super::Destroy();
}
IMPLEMENT_CLASS(UChannelDownload)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

