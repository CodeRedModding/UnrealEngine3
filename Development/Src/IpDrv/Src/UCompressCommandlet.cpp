/*=============================================================================
	UCompressCommandlet.cpp: Unreal Engine file commpression
	Copyright 1999-2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter

=============================================================================*/

#include "UnIpDrv.h"
#include "UnIpDrvCommandlets.h"

/*-----------------------------------------------------------------------------
	UCompressCommandlet.
-----------------------------------------------------------------------------*/
INT UCompressCommandlet::Main( const TCHAR* Parms )
{
	FString Wildcard;
	if( !ParseToken(Parms,Wildcard,0) )
		appErrorf(TEXT("Source file(s) not specified"));
	do
	{
        // skip "-nohomedir", etc... --ryan.
        if ((Wildcard.Len() > 0) && (Wildcard[0] == '-'))
            continue;

		FString Dir;
		INT i = Wildcard.InStr( PATH_SEPARATOR, 1 );
		if( i != -1 )
			Dir = Wildcard.Left( i+1 );
		TArray<FString> Files;
		GFileManager->FindFiles( Files, *Wildcard, 1, 0 );
		if( Files.Num() == 0 )
			appErrorf(TEXT("Source %s not found"), *Wildcard);
		for( INT j=0;j<Files.Num();j++)
		{
			FString Src = Dir + Files(j);
			DWORD Result = GFileManager->Copy( *Src, *Src, 1, 1, 0, FILECOPY_Compress, NULL );
			switch( Result )
			{
			case COPY_OK:
				break;
			case COPY_ReadFail:
				appErrorf(TEXT("Error occurred opening %s"), *Src);
				break;
			case COPY_WriteFail:
				appErrorf(TEXT("Error occurred creating %s%s"), *Src, COMPRESSED_EXTENSION);
				break;
			case COPY_CompFail:
			case COPY_MiscFail:
			default:
				appErrorf(TEXT("Error occurred compressing %s"), *Src);
				break;
			}

			INT SrcSize = GFileManager->FileSize(*Src);
			INT DstSize = GFileManager->FileSize(*(Src+COMPRESSED_EXTENSION));
			warnf(TEXT("Compressed %s -> %s%s (%d%%)"), *Src, *Src, COMPRESSED_EXTENSION, 100*DstSize / SrcSize);
		}
	}
	while( ParseToken(Parms,Wildcard,0) );
	return 0;
}
IMPLEMENT_CLASS(UCompressCommandlet)
/*-----------------------------------------------------------------------------
	UDecompressCommandlet.
-----------------------------------------------------------------------------*/
INT UDecompressCommandlet::Main( const TCHAR* Parms )
{
	FString Src, Dst;
	if( !ParseToken(Parms,Src,0) )
		appErrorf(TEXT("Compressed file not specified"));
	FString Dest;
    if( Src.Right(appStrlen(COMPRESSED_EXTENSION)) == COMPRESSED_EXTENSION )
		Dest = Src.Left( Src.Len() - appStrlen(COMPRESSED_EXTENSION) );
	else
		appErrorf(TEXT("Compressed files must end in %s"), COMPRESSED_EXTENSION);
	DWORD Result = GFileManager->Copy( *Dest, *Dest, 1, 1, 0, FILECOPY_Decompress, NULL );
	switch( Result )
	{
	case COPY_OK:
		break;
	case COPY_ReadFail:
		appErrorf(TEXT("Error occurred opening %s%s"), *Dest, COMPRESSED_EXTENSION);
		break;
	case COPY_WriteFail:
		appErrorf(TEXT("Error occurred creating %s"), *Dest);
		break;
	case COPY_DecompFail:
	case COPY_MiscFail:
	default:
		appErrorf(TEXT("Error occurred decompressing %s"), *Dest);
		break;
	}

	warnf(TEXT("Decompressed %s -> %s"), *Src, *Dest);
	return 0;
}
IMPLEMENT_CLASS(UDecompressCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

