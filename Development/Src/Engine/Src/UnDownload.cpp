/*=============================================================================
	UnDownload.cpp: Unreal file-download interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
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
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UDownload, Connection ) );
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
		debugf( NAME_DevNet, TEXT("Receiving package '%s'"), *Info->PackageName.ToString() );

		// if we need to get the filesize from the data, it's the first 4 bytes
		if (bDownloadSendsFileSizeInData)
		{
//@todo JoshA -- Please fix this properly
			if (Count >= sizeof(INT))
			{
				// the first 4 bytes are the file size, which we need here
				check(Count >= sizeof(INT));

				// copy the filesize out of the stream
				appMemcpy(&FileSize, Data, sizeof(INT));

				// now skip over the filesize
				Data += 4;
				Count -= 4;
			}
			else
			{
				FileSize = 0;
			}
		}

		// make sure the cache directory exists
		GFileManager->MakeDirectory( *GSys->CachePath, 0 );

//@todo JoshA -- Please fix this properly
		if (FileSize > 0)
		{
			// make sure there's enough room in the cache
			GSys->CleanCacheForNeededSpace(FileSize);

			// create the temp file to download to
			appCreateTempFilename( *GSys->CachePath, TempFilename, ARRAY_COUNT(TempFilename) );
			RecvFileAr = GFileManager->CreateFileWriter( TempFilename, 0, GNull, FileSize );

#if PS3
			// @todo ship: Handle not having enough HD space to download the file
			if (!RecvFileAr)
			{
				DownloadError( *LocalizeError(TEXT("NetHDSpace"),TEXT("Engine")) );
				return;
			}
#endif
		}
	}

	// Receive.
	if( !RecvFileAr )
	{
		// Opening file failed.
		DownloadError( *LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
	}
	else
	{
		// write the received data to the file
		if( Count > 0 )
		{
			RecvFileAr->Serialize( Data, Count );
		}

		if( RecvFileAr->IsError() )
		{
			// Write failed.
			DownloadError( *FString::Printf( *LocalizeError(TEXT("NetWrite"),TEXT("Engine")), TempFilename ) );
		}
		else
		{
			// Successful.
			Transfered += Count;
			FString ProgressTitle;
			if ( (Info->PackageFlags&PKG_ClientOptional) != 0 )
			{
				ProgressTitle = FString::Printf( LocalizeSecure(LocalizeProgress(TEXT("ReceiveOptionalFile"),TEXT("Engine")), *Info->PackageName.ToString()) );
			}
			else
			{
				ProgressTitle = FString::Printf( LocalizeSecure(LocalizeProgress(TEXT("ReceiveFile"),TEXT("Engine")), *Info->PackageName.ToString()) );
			}
			FString ProgressMessage = FString::Printf( LocalizeSecure(LocalizeProgress(TEXT("ReceiveSize"),TEXT("Engine")), FileSize/1024, 100.f*Transfered/FileSize, TEXT('%')) );
			Connection->Driver->Notify->NotifyProgress( PMT_DownloadProgress, ProgressTitle, ProgressMessage );
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
		debugf( TEXT("Skipped download of '%s'"), *Info->Parent->GetName() );
		GFileManager->Delete( TempFilename );
		TCHAR Msg[MAX_SPRINTF]=TEXT("");
		appSprintf( Msg, TEXT("Skipped '%s'"), *Info->Parent->GetName() );
		Connection->Driver->Notify->NotifyProgress( PMT_DownloadProgress, LocalizeProgress(TEXT("Success"), TEXT("Engine")), Msg );
		Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, TEXT(""), 1 );
	}
	else
	{
		TCHAR Dest[MAX_SPRINTF]=TEXT("");
		appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("%s%s"), *GSys->CachePath, *Info->Guid.String(), *GSys->CacheExt );
		if( !*Error && Transfered==0 )
			DownloadError( *FString::Printf( LocalizeSecure(LocalizeError(TEXT("NetRefused"),TEXT("Engine")), *Info->PackageName.ToString()) ) );
		if( !*Error && IsCompressed )
		{
			TCHAR CFilename[1024];
			appStrcpy( CFilename, TempFilename );
			appCreateTempFilename( *GSys->CachePath, TempFilename, ARRAY_COUNT(TempFilename) );
			debugf(TEXT("Opening compressed file!!!!!!!!!!!!!!!! %ls"), CFilename);
			FArchive* CFileAr = GFileManager->CreateFileReader( CFilename );
			FArchive* UFileAr = GFileManager->CreateFileWriter( TempFilename, 0, GNull, FileSize );
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

		// verify the download
		if( !*Error && GFileManager->FileSize(TempFilename) != FileSize )
		{
			DownloadError( *LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
		}

		// move the temp file to final name
		if( !*Error && !GFileManager->Move( Dest, TempFilename ) )
		{
			DownloadError( *LocalizeError(TEXT("NetMove"),TEXT("Engine")) );
		}

		if( *Error )
		{
			if (*TempFilename)
			{
				GFileManager->Delete( TempFilename );
			}
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );
		}
		else
		{
			// temporarily allow the GConfigCache to perform file operations if they were off
			UBOOL bWereFileOpsDisabled = GConfig->AreFileOperationsDisabled();
			GConfig->EnableFileOperations();

			// Success.
			FString IniName = GSys->CachePath * TEXT("Cache.ini");
			FString Message = FString::Printf(TEXT("Received '%s'"), *Info->PackageName.ToString());

			FConfigCacheIni CacheIni;
			CacheIni.SetString(TEXT("Cache"), *Info->Guid.String(), *Info->PackageName.ToString(), *IniName);
			// flush the ini to disk, as below code may try to read it
			CacheIni.Flush(TRUE);

			Connection->Driver->Notify->NotifyProgress( PMT_DownloadProgress, LocalizeProgress(TEXT("Success"), TEXT("Engine")), *Message );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0 );

			// re-disable file ops if they were before
			if (bWereFileOpsDisabled)
			{
				GConfig->DisableFileOperations();
			}
		}
	}
}

void UDownload::CleanUp()
{
	if (RecvFileAr != NULL)
	{
		delete RecvFileAr;
		RecvFileAr = NULL;
		GFileManager->Delete(TempFilename);
	}
	if (Connection != NULL && Connection->Download == this)
	{
		Connection->Download = NULL;
	}
	Connection = NULL;
}

void UDownload::FinishDestroy()
{
	CleanUp();
	Super::FinishDestroy();
}

IMPLEMENT_CLASS(UDownload)

/*-----------------------------------------------------------------------------
	UChannelDownload implementation.
-----------------------------------------------------------------------------*/

void UChannelDownload::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UChannelDownload, Ch ) );
}

UChannelDownload::UChannelDownload()
{
	// this download types sends filesize
	bDownloadSendsFileSizeInData = TRUE;
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
#if WITH_UE3_NETWORKING
		FOutBunch Bunch( Ch, 1 );
		FString Cmd = TEXT("SKIP");
		Bunch << Cmd;
		Bunch.bReliable = 1;
		Ch->SendBunch( &Bunch, 0 );
#endif	//#if WITH_UE3_NETWORKING
		return 1;
	}
	return 0;
}

void UChannelDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression )
{
	UDownload::ReceiveFile( InConnection, InPackageIndex, Params, InCompression );

	// Create channel.
	Ch = (UFileChannel *)Connection->CreateChannel( CHTYPE_File, 1 );
	if( !Ch || InPackageIndex >= Connection->PackageMap->List.Num())
	{
		DownloadError( *LocalizeError(TEXT("ChAllocate"),TEXT("Engine")) );
		DownloadDone();
		return;
	}

	// Set channel properties.
	Ch->Download = this;

	// NOTE: There is an inconsistency between the UDownload class (which stores PackageMap index) and UFileChannel (which stores GUID);
	//			this does not matter, as it is safe for UDownload to track PackageIndex clientside, but it is not safe for the server to track by index,
	//			it must use the GUID (this value is not even used clientside, but is set here regardless)
	Ch->PackageGUID = Connection->PackageMap->List(PackageIndex).Guid;

	// Send file request.
#if WITH_UE3_NETWORKING
	FOutBunch Bunch( Ch, 0 );
	Bunch << Info->Guid;
	Bunch.bReliable = 1;
	check(!Bunch.IsError());
	Ch->SendBunch( &Bunch, 0 );
#endif	//#if WITH_UE3_NETWORKING
}

void UChannelDownload::FinishDestroy()
{
	if( Ch && Ch->Download == this )
	{
		Ch->Download = NULL;
	}
	Ch = NULL;
	Super::FinishDestroy();
}

IMPLEMENT_CLASS(UChannelDownload)

