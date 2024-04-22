/**********************************************************************

Filename    :   GFxUIEditorFactories.cpp
Content     :   Factory used to create UGFxMovieInfo.

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUIEditor.h"

#if WITH_GFx

#include "ScaleformEngine.h"
#include "GFxUI.h"



#include "UnObjectTools.h"

/*------------------------------------------------------------------------------
	 UGFxMovieFactory.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGFxMovieFactory);

/**
 *	Return a new GFx Movie object from a chunk of binary data.
 */
UObject* UGFxMovieFactory::FactoryCreateBinary
(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	const TCHAR* Type,
	const BYTE*& Buffer,
	const BYTE* BufferEnd,
	FFeedbackContext* Warn
)
{
#if WITH_GFx
	check(Warn);        
	check( Class->IsChildOf(USwfMovie::StaticClass()) );	
	FString filename(CurrentFilename);

	FString GFxExportCmdLine;
	//generate command line from parameters
	//Texture format
	GFxExportCmdLine += FString::Printf(TEXT(" -i %s"), *TextureFormat);
	//append on rescale parameters if the user didn't specify no scaling
	if ( TextureRescale != FlashTextureScale_None )
	{
		GFxExportCmdLine += TEXT(" -rescale ");
		switch (TextureRescale)
		{
			case FlashTextureScale_High:
				GFxExportCmdLine += TEXT(" hi ");
				break;
			case FlashTextureScale_Low:
				GFxExportCmdLine += TEXT(" low ");
				break;
			case FlashTextureScale_NextLow:
				GFxExportCmdLine += TEXT(" nextlow ");
				break;
			case FlashTextureScale_Mult4:
				GFxExportCmdLine += TEXT(" mult4 ");
				break;
		}
	}
	if (bPackTextures)
	{
		// '-pack' meansa pack images into text ures
		// '-packsize' attempt to fit into texture of size PackTextureSize.
		GFxExportCmdLine += FString::Printf(TEXT("-pack -packsize %d "), PackTextureSize);

		if (bForceSquarePacking)
		{
			GFxExportCmdLine += FString::Printf(TEXT("-ptsquare "));
		}
	}

	USwfMovie* pmovieInfo;
	if (!appStricmp(*FFilename(filename).GetExtension(), TEXT("swf")))
	{
		// SWF movies must be processed with GFxExport first
		pmovieInfo = Cast<USwfMovie>(CreateMovieGFxExport(InParent, Name, filename, Flags, Buffer, BufferEnd, GFxExportCmdLine, Warn));
		if (!pmovieInfo)
		{
			return NULL;
		}
	}
	else
	{
		// GFX files are already processed and loaded as raw data
		pmovieInfo = ConstructObject<USwfMovie>(Class, InParent, Name, Flags);
		pmovieInfo->SetRawData(Buffer, BufferEnd - Buffer);
	}

	// Copy the factory's import settings into the new movie so we can use them for reimporting later
	pmovieInfo->bSetSRGBOnImportedTextures = bSetSRGBOnImportedTextures;
	pmovieInfo->TextureRescale = TextureRescale;
	pmovieInfo->bPackTextures = bPackTextures;
	pmovieInfo->PackTextureSize = PackTextureSize;
	pmovieInfo->bForceSquarePacking = bForceSquarePacking;
	pmovieInfo->TextureFormat = TextureFormat;

	// Update transient time stamp for imported asset
	pmovieInfo->ImportTimeStamp = appCycles();
	pmovieInfo->SourceFile = GFileManager->ConvertToRelativePath(*filename);

	TArray<FString> OutMissingReferences;
	CheckImportTags(pmovieInfo, &OutMissingReferences, FALSE);

	// Warn about any references to other SWFs that we were unable to resolve during import.
	if ( OutMissingReferences.Num() > 0 )
	{
		FString ErrorString = FString::Printf(TEXT("Warning. The following referenced assets were not found while importing %s:\n\n"), *filename);
		for(TArray<FString>::TConstIterator MissingRefsIterator(OutMissingReferences); MissingRefsIterator; ++MissingRefsIterator)
		{
			ErrorString += TEXT("    ");
			ErrorString += *(*MissingRefsIterator);
		}
		
		appMsgf( AMT_OK, *ErrorString );		
	}

	if (pmovieInfo->GetOuter())
	{
		pmovieInfo->GetOuter()->MarkPackageDirty();
	}
	else
	{
		pmovieInfo->MarkPackageDirty();
	}

	// Update the timestamp for the movie.
	FFileManager::FTimeStamp TimeStamp;
	if ( GFileManager->GetTimestamp( *pmovieInfo->SourceFile, TimeStamp ) )
	{
		FFileManager::FTimeStamp::TimestampToFString( TimeStamp, pmovieInfo->SourceFileTimestamp );
	}

	return pmovieInfo;    
#else
	return NULL;
#endif
}

UBOOL UGFxMovieFactory::Reimport(UObject* InObject)
{
#if WITH_GFx
	USwfMovie* MovieInfo = Cast<USwfMovie>(InObject);
	if (MovieInfo && MovieInfo->SourceFile.Len())
	{
		// use same settings as first import
		TArray<UObject*> OldUserRefs (MovieInfo->UserReferences);

		FFilename Filename = MovieInfo->SourceFile;
		// If there is no file path provided, can't reimport from source
		if ( !Filename.Len() )
		{
			// Since this is a new system most sound node waves don't have paths, so logging has been commented out
			//GWarn->Log( TEXT("-- cannot reimport: sound node wave resource does not have path stored."));
			return FALSE;
		}

		// Copy the movie's reimport settings to factory settings
		bSetSRGBOnImportedTextures = MovieInfo->bSetSRGBOnImportedTextures;
		TextureRescale = MovieInfo->TextureRescale;
		// Work-around for packed textures being broken. Packad textures will be re-enabled.
		bPackTextures = MovieInfo->bPackTextures;
		PackTextureSize = MovieInfo->PackTextureSize;
		bForceSquarePacking = MovieInfo->bForceSquarePacking;
		TextureFormat = MovieInfo->TextureFormat;

		FFileManager::FTimeStamp FileTimeStamp, StoredTimeStamp;

		GWarn->Log( FString::Printf(TEXT("Performing atomic reimport of [%s]"), *Filename ) );
		// Ensure that the file provided by the path exists; if it doesn't, prompt the user for a new file
		if ( !GFileManager->GetTimestamp( *Filename, FileTimeStamp ) )
		{
			GWarn->Log( TEXT("-- cannot reimport: source file cannot be found.") );

			if ( GIsUCC )
			{
				return FALSE;
			}
			else
			{
				UBOOL bNewSourceFound = FALSE;
				FString NewFileName;
				if ( ObjectTools::FindFileFromFactory ( this , LocalizeUnrealEd("Import_SourceFileNotFound"), NewFileName ) )
				{
					MovieInfo->SourceFile = GFileManager->ConvertToRelativePath( *NewFileName );
					bNewSourceFound = GFileManager->GetTimestamp( *( MovieInfo->SourceFile ), FileTimeStamp );
				}
				// If a new source wasn't found or the user canceled out of the dialog, we cannot proceed, but this reimport factory 
				// has still technically "handled" the reimport, so return TRUE instead of FALSE
				if ( !bNewSourceFound )
				{
					return TRUE;
				}
			}
		}

		FFileManager::FTimeStamp::FStringToTimestamp( MovieInfo->SourceFileTimestamp, StoredTimeStamp );
		
		UBOOL bImport = FALSE;
		if (StoredTimeStamp >= FileTimeStamp)
		{
			// Allow the user to import older files if desired, this allows for issues 
			// that would arise when reverting to older assets via version control.
			GWarn->Log( TEXT("-- file on disk exists but has an equal or older timeStamp."));
			
			WxSuppressableWarningDialog ReimportingOlderFileWarning( FString::Printf(LocalizeSecure( LocalizeUnrealEd("Error_Reimport_GfxMovieFactory_FileOlderThanCurrentWarning"), *(MovieInfo->GetName()) ))		,
																	 LocalizeUnrealEd("Error_Reimport_FileOlderThanCurrentTitle"), 
																	 "Warning_ReimportingOlderGfxMovieFactory", 
																	 TRUE );

			if (ReimportingOlderFileWarning.ShowModal() != wxID_CANCEL)
			{
				bImport = TRUE;
				GWarn->Log( TEXT("-- The user has opted to import regardless."));
			}
			else
			{
				GWarn->Log( TEXT("-- The user has opted to NOT import."));
			}
		}
		else
		{
			// if the file is newer, perform import.
			bImport = TRUE;
			GWarn->Log( TEXT("-- file on disk exists and is newer.  Performing import."));
		}

		// Import the new file if it is newer or the user wishes too regardless. 
		if (bImport)
		{
			GWarn->Log( TEXT("-- file on disk exists.  Performing import."));
			if( StoredTimeStamp == FileTimeStamp )
			{
				GWarn->Log( TEXT("-- file on disk is up to date; still performing import in case properties have changed"));
			}
		
			if (NULL == UFactory::StaticImportObject(InObject->GetClass(), InObject->GetOuter(), *InObject->GetName(), RF_Public|RF_Standalone, *MovieInfo->SourceFile, NULL, this))
			{
				return FALSE;
			}

			MovieInfo->UserReferences.Empty();
			MovieInfo->UserReferences.Append(OldUserRefs);
			//MovieInfo->SourceFile = OriginalSourceFile;

			// Update transient time stamp for imported asset
			MovieInfo->ImportTimeStamp = appCycles();

			if (MovieInfo->GetOuter())
			{
				MovieInfo->GetOuter()->MarkPackageDirty();
			}
			else
			{
				MovieInfo->MarkPackageDirty();
			}

			// Find all of the currently-playing Gfx Movie Players using this Swf Movie and
			// restart them!
			if( GEditor->GetUserSettings().bAutoRestartReimportedFlashMovies )
			{
				for( TObjectIterator< UGFxMoviePlayer > MoviePlayerIt; MoviePlayerIt; ++MoviePlayerIt )
				{
					UGFxMoviePlayer* CurMoviePlayer = *MoviePlayerIt;
					if( CurMoviePlayer != NULL )
					{
						// Is the flash movie we just reimported assigned to this movie player?
						if( CurMoviePlayer->MovieInfo == MovieInfo )
						{
							// Does this player currently have a movie loaded (and potentially playing)?
							if( CurMoviePlayer->pMovie != NULL )
							{
								// Don't bother reloading the movie if it wasn't even playing
								// @todo: Note that because of this test, non-playing movies won't be affected by changes
								const UBOOL bWasMoviePlaying = CurMoviePlayer->pMovie->Playing;
								if( bWasMoviePlaying )
								{
									debugf( TEXT( "GFx Movie Reimport: Restarting GFx movie player [%s] because GFx movie [%s] was reimported" ), *CurMoviePlayer->GetName(), *MovieInfo->GetName() );

									const UBOOL bWasMoviePaused = !CurMoviePlayer->pMovie->fUpdate;

									// Stop and unload the movie
									const UBOOL bUnloadOnClose = TRUE;
									CurMoviePlayer->Close( bUnloadOnClose );

									// Reload the movie and start it playing again!
									const UBOOL bStartPaused = bWasMoviePaused;
									CurMoviePlayer->Start( bStartPaused );
								}
							}
						}
					}
				}
			}
		}

		return TRUE;
	}
#endif
	return FALSE;
}


#endif // WITH_GFx