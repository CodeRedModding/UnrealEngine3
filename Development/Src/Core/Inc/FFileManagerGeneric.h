/*=============================================================================
	FFileManagerGeneric.h: Unreal generic file manager support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	This base class simplifies FFileManager implementations by providing
	simple, unoptimized implementations of functions whose implementations
	can be derived from other functions.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// for compression
#include "../../zlib/zlib.h"

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

#define COPYBLOCKSIZE	32768
#define MAXCOMPSIZE		33096		// 32768 + 1%

class FFileManagerGeneric : public FFileManager
{
public:
	INT FileSize( const TCHAR* Filename )
	{
		// Create a generic file reader, get its size, and return it.
		FArchive* Ar = CreateFileReader( Filename );
		if( !Ar )
			return -1;
		INT Result = Ar->TotalSize();
		delete Ar;
		return Result;
	}
	DWORD Copy( const TCHAR* InDestFile, const TCHAR* InSrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, DWORD Compress, FCopyProgress* Progress )
	{
		// Direct file copier.
		if( Progress && !Progress->Poll( 0.0 ) )
			return COPY_Canceled;
		DWORD Result = COPY_OK;
		FString SrcFile = InSrcFile;
		FString DestFile = InDestFile;
		switch( Compress )
		{
		case FILECOPY_Compress:
			DestFile = DestFile + COMPRESSED_EXTENSION;
			break;
		case FILECOPY_Decompress:
			SrcFile = SrcFile + COMPRESSED_EXTENSION;
			break;
		}

		FArchive* Src = CreateFileReader( *SrcFile );
		if( !Src )
			Result = COPY_ReadFail;
		else
		{
			INT Size = Src->TotalSize();
			FArchive* Dest = CreateFileWriter( *DestFile, (ReplaceExisting?0:FILEWRITE_NoReplaceExisting) | (EvenIfReadOnly?FILEWRITE_EvenIfReadOnly:0) );
			if( !Dest )
				Result = COPY_WriteFail;
			else
			{
				INT Percent=0, NewPercent=0;
				switch( Compress )
				{
				case FILECOPY_Normal:
					{
						BYTE Buffer[COPYBLOCKSIZE];
						for( INT Total=0; Total<Size; Total+=sizeof(Buffer) )
						{
							INT Count = Min( Size-Total, (INT)sizeof(Buffer) );
							Src->Serialize( Buffer, Count );
							if( Src->IsError() )
							{
								Result = COPY_ReadFail;
								break;
							}
							Dest->Serialize( Buffer, Count );
							if( Dest->IsError() )
							{
								Result = COPY_WriteFail;
								break;
							}
							NewPercent = Total * 100 / Size;
							if( Progress && Percent != NewPercent && !Progress->Poll( (FLOAT)NewPercent / 100.f ) )
							{
								Result = COPY_Canceled;
								break;
							}
							Percent = NewPercent;
						}
					}
					break;
				case FILECOPY_Decompress:
					{
						BYTE UncBuffer[COPYBLOCKSIZE];
						BYTE ComBuffer[MAXCOMPSIZE];
						while( !Src->AtEnd() )
						{
							DWORD ComSize, UncSize;
							Src->Serialize( &ComSize, sizeof(ComSize) );
							Src->Serialize( &UncSize, sizeof(UncSize) );
							if( ComSize > MAXCOMPSIZE || UncSize > COPYBLOCKSIZE )
							{
								Result = COPY_DecompFail;
								break;
							}
							Src->Serialize( ComBuffer, ComSize );
							if( Src->IsError() )
							{
								Result = COPY_ReadFail;
								break;
							}
                            if( uncompress( UncBuffer, (uLongf *) &UncSize, ComBuffer, ComSize ) != Z_OK )
							{
								Result = COPY_DecompFail;
								break;
							}
							Dest->Serialize( UncBuffer, UncSize );
							if( Dest->IsError() )
							{
								Result = COPY_WriteFail;
								break;
							}
							NewPercent = Src->Tell() * 100 / Size;
							if( Progress && Percent != NewPercent && !Progress->Poll( (FLOAT)NewPercent / 100.f ) )
							{
								Result = COPY_Canceled;
								break;
							}
							Percent = NewPercent;
						}
					}
					break;
				case FILECOPY_Compress:
					{
						BYTE UncBuffer[COPYBLOCKSIZE];
						BYTE ComBuffer[MAXCOMPSIZE];

						for( INT Total=0; Total<Size; Total+=sizeof(UncBuffer) )
						{
							DWORD UncSize = Min( Size-Total, (INT)sizeof(UncBuffer) );
							Src->Serialize( UncBuffer, UncSize );
							if( Src->IsError() )
							{
								Result = COPY_ReadFail;
								break;
							}
							DWORD ComSize = MAXCOMPSIZE;
							if( compress( ComBuffer, (uLongf *) &ComSize, UncBuffer, UncSize ) != Z_OK )
							{
								Result = COPY_CompFail;
								break;
							}
							Dest->Serialize( &ComSize, sizeof(ComSize) );
							Dest->Serialize( &UncSize, sizeof(UncSize) );
							Dest->Serialize( ComBuffer, ComSize );
							if( Dest->IsError() )
							{
								Result = COPY_WriteFail;
								break;
							}
							NewPercent = Total * 100 / Size;
							if( Progress && Percent != NewPercent && !Progress->Poll( (FLOAT)NewPercent / 100.f ) )
							{
								Result = COPY_Canceled;
								break;
							}
							Percent = NewPercent;
						}
					}
					break;
				}
				if( Result == COPY_OK )
					if( !Dest->Close() )
						Result = COPY_WriteFail;
				delete Dest;
				if( Result != COPY_OK )
					Delete( *DestFile );
			}
			if( Result == COPY_OK )
				if( !Src->Close() )
					Result = COPY_ReadFail;
			delete Src;
		}
		if( Progress && Result==COPY_OK && !Progress->Poll( 1.0 ) )
			Result = COPY_Canceled;
		return Result;
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		// Support code for making a directory tree.
		check(Tree);
		INT SlashCount=0, CreateCount=0;
		for( TCHAR Full[256]=TEXT(""), *Ptr=Full; ; *Ptr++=*Path++ )
		{
			if( *Path==PATH_SEPARATOR[0] || *Path==0 )
			{
				if( SlashCount++>0 && !IsDrive(Full) )
				{
					*Ptr = 0;
					if( !MakeDirectory( Full, 0 ) )
						return 0;
					CreateCount++;
				}
			}
			if( *Path==0 )
				break;
		}
		return CreateCount!=0;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		// Support code for removing a directory tree.
		check(Tree);
		if( !appStrlen(Path) )
			return 0;
		FString Spec = FString(Path) * TEXT("*");
		TArray<FString> List;
		FindFiles( List, *Spec, 1, 0 );
		for( INT i=0; i<List.Num(); i++ )
			if( !Delete(*(FString(Path) * List(i)),1,1) )
				return 0;
		FindFiles( List, *Spec, 0, 1 );
		for( INT i=0; i<List.Num(); i++ )
			if( !DeleteDirectory(*(FString(Path) * List(i)),1,1) )
				return 0;
		return DeleteDirectory( Path, RequireExists, 0 );
	}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL ReplaceExisting=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
	{
		// Move file manually.
		if( Copy(Dest,Src,ReplaceExisting,EvenIfReadOnly,Attributes,FILECOPY_Normal,NULL) != COPY_OK )
			return 0;
		Delete( Src, 1, 1 );
		return 1;
	}
private:
	UBOOL IsDrive( const TCHAR* Path )
	{
		// Does Path refer to a drive letter or BNC path?
		if( appStricmp(Path,TEXT(""))==0 )
			return 1;
		else if( appToUpper(Path[0])!=appToLower(Path[0]) && Path[1]==':' && Path[2]==0 )
			return 1;
		else if( appStricmp(Path,TEXT("\\"))==0 )
			return 1;
		else if( appStricmp(Path,TEXT("\\\\"))==0 )
			return 1;
		else if( Path[0]=='\\' && Path[1]=='\\' && !appStrchr(Path+2,'\\') )
			return 1;
		else if( Path[0]=='\\' && Path[1]=='\\' && appStrchr(Path+2,'\\') && !appStrchr(appStrchr(Path+2,'\\')+1,'\\') )
			return 1;
		else
			return 0;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

