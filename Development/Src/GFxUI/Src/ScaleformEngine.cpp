/**********************************************************************

Filename    :   ScaleformEngine.cpp
Content     :   Movie manager / GFxMovieView access wrapper for UE3,
				implements FGFxEngine class.

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#include "ScaleformEngine.h"
#include "GFxUIClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "EngineUIPrivateClasses.h"
//#include "ScaleformImageLoader.h"
#include "ScaleformFile.h"
#include "ScaleformFont.h"
#include "ScaleformFullscreenMovie.h"
#include "ScaleformStats.h"
#include "ScaleformAllocator.h"
#include "../../Engine/Src/SceneRenderTargets.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_Types.h"
#include "Kernel/SF_Memory.h"
#include "Kernel/SF_RefCount.h"
#include "Render/Render_HAL.h"
#include "GFx/GFx_Player.h"
#include "GFx/GFx_AS2Support.h"
#include "GFx/GFx_AS3Support.h"
#include "Kernel/SF_File.h"
#include "Kernel/SF_SysFile.h"
#include "GFx/GFx_Loader.h"
#include "GFx/GFx_ImageResource.h"
#include "GFx/GFx_Log.h"
#include "GFx/AMP/Amp_Server.h"
#include "Render/Render_ImageFiles.h"
#include "GFx/GFx_ImageCreator.h"

#include "GFx/AS3/AS3_Global.h"

#include "Render/RHI_HAL.h"

#if XBOX
#include "Render/RHI_XeBufferMemory.h"
#endif

#if WITH_REALD
	#include "RealD/RealD.h"
#endif

#if WITH_GFx_IME
#if defined(WIN32)
#include "GFxIME/GFx_IMEManagerWin32.h"
#endif
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#if defined(SF_OS_WIN32)
#include "D3D9Drv.h"
#elif defined(SF_OS_PS3)
#include "UnPS3.h"
#include "FFileManagerPS3.h"
#endif

#if WITH_GFx_VIDEO
// Core video support
#include "Video/Video_Video.h"
#if defined(SF_OS_WIN32)
#include "Video/Video_VideoPC.h"
#elif defined(SF_OS_XBOX360)
#include "Video/Video_VideoXbox360.h"
#elif defined(SF_OS_PS3)
#include "Video/Video_VideoPS3.h"
#endif
#endif

using namespace Render;
using namespace GFx;

// Define to enable gamepad mouse emulation. Only use if necessary because of event processing overhead.
#define GAMEPAD_MOUSE

// Define to enable logging ActionScript traces and other messages with debugf
#define GFx_LOG

FGFxEngine* GGFxEngine = NULL;
UGFxEngine* GGFxGCManager = NULL;
UBOOL       GGFxRendererInitialized = FALSE;

FGFxAllocator GGFxAllocator;

extern INT		GScreenWidth;
extern INT		GScreenHeight;

IMPLEMENT_CLASS ( UGFxEngine );


UGFxEngine::UGFxEngine()
{
}

void UGFxEngine::FinishDestroy()
{
	Super::FinishDestroy();
}

void UGFxEngine::Release()
{
	if ( --RefCount == 0 )
	{
		RemoveFromRoot();
		if ( this == GGFxGCManager )
		{
			GGFxGCManager = NULL;
		}
	}
}

// ***** FGFxFileOpener

// Override GFxFileOpener to allow using Unreal package files.
// Unlike regular GFxFileOpener, this class also supports Unreal-style path
// names that look like 'Package.FileName'. For disk files, use absolute paths.

static inline UBOOL IsFilesystemPath ( const char *ppath )
{
	if ( *ppath == '/' || *ppath == '\\' )
	{
		return TRUE;
	}
	const char* p = ppath + 1;
	while ( *p )
	{
		if ( *p == '/' || *p == '\\' )
		{
			return TRUE;
		}
		else if ( *p == ':' && ( p[1] == '/' || p[1] == '\\' ) )
		{
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

void FGFxEngine::InsertMovieIntoList ( FGFxMovie* Movie, TArray<FGFxMovie*>* List )
{
	INT iter;

	List->RemoveItem ( Movie );

	//Insert the new movie into the list in priority order (lowest at index 0)
	for ( iter = 0; iter < List->Num(); iter++ )
	{
		if ( ( *List ) ( iter )->pUMovie->Priority > Movie->pUMovie->Priority )
		{
			List->InsertItem ( Movie, iter );
			break;
		}
	}
	//If you reached the end then you are the highest, so add yourself to the end
	if ( iter == List->Num() )
	{
		List->AddItem ( Movie );
	}
}

void FGFxEngine::InsertMovie ( FGFxMovie* Movie, BYTE DPG )
{
	INT index;
	if ( !AllMovies.FindItem ( Movie, index ) )
	{
		AllMovies.AddItem ( Movie );
	}

	InsertMovieIntoList ( Movie, &OpenMovies );
	InsertMovieIntoList ( Movie, & ( DPGOpenMovies[DPG] ) );
	ReevaluateFocus();
}


/**
	* Given a relative path that may contain tokens such as /./ and ../, collapse these
	* relative tokens into a an absolute reference.
	*
	* @param InString A relative path that may contain relative tokens.
	*
	* @return A path with all relative tokens collapsed
	*/
FFilename FGFxEngine::CollapseRelativePath ( const FFilename& InString )
{
	FFilename ReturnString = InString;

	// Make sure the path only uses the PATH_SEPARATOR and not some weird combination of slashes and back slashes
	FPackageFileCache::NormalizePathSeparators ( ReturnString );

	// Reduce all instances of "/./" to "/"
	const FString SequenceToCollapse = FString ( PATH_SEPARATOR ) + TEXT ( "." ) + PATH_SEPARATOR;
	ReturnString.ReplaceInline ( *SequenceToCollapse, PATH_SEPARATOR );

	// Resolve all ../ in this relative path to build an absolute path.
	FString LeftString, RightString;
	const FString SearchString = FString ( TEXT ( ".." ) ) + PATH_SEPARATOR;
	while ( ReturnString.Split ( SearchString, &LeftString, &RightString ) )
	{
		INT Index = LeftString.Len() - 1;
		// Eat the slash on the end of left if there is one.
		if ( Index >= 0 && LeftString[Index] == PATH_SEPARATOR[0] )
		{
			--Index;
		}
		// Eat leftwards until a slash is hit.
		while ( Index >= 0 && LeftString[Index] != PATH_SEPARATOR[0] )
		{
			LeftString[Index--] = 0;
		}
		ReturnString = FFilename ( *LeftString ) + FString ( *RightString );
	}
	return ReturnString;
}


UBOOL FGFxEngine::GetPackagePath ( const char *ppath, FFilename& strFilename )
{
	const char *pkgpath = IsPackagePath ( ppath );
	if ( !pkgpath )
	{
		return FALSE;
	}

	strFilename = FFilename ( pkgpath );

	if ( !appStricmp ( *strFilename.GetExtension(), TEXT ( "gfx" ) ) ||
			!appStricmp ( *strFilename.GetExtension(), TEXT ( "swf" ) ) )
	{
		strFilename = strFilename.GetBaseFilename ( FALSE );
	}

	strFilename = CollapseRelativePath ( strFilename );
	ReplaceCharsInFString ( strFilename, PATH_SEPARATOR, L'.' );
	ReplaceCharsInFString ( strFilename, TEXT ( "\\" ), L'.' );
	ReplaceCharsInFString ( strFilename, TEXT ( "/" ), L'.' );
	return TRUE;
}

TextureImage* FGFxEngine::CreateUTextureImage ( UTexture* Texture )
{
	if ( Texture )
	{
		UTexture2D* Texture2D = Cast<UTexture2D> ( Texture );
		ImageSize   size;
		if ( Texture2D )
		{
			size = ImageSize ( Texture2D->SizeX, Texture2D->SizeY );
		}
		else
		{
			UTextureRenderTarget2D* TextureRT2D = Cast<UTextureRenderTarget2D> ( Texture );
			if ( TextureRT2D )
			{
				size = ImageSize ( TextureRT2D->SizeX, TextureRT2D->SizeY );
			}
		}
		Ptr<Render::Texture>  NewTexture = *RenderHal->GetTextureManager()->CreateTexture ( Texture, size );
		Render::TextureImage* NewImage = SF_NEW TextureImage ( NewTexture->GetFormat(), size, ImageUse_Wrap | ImageUse_NoDataLoss, NewTexture );

		return NewImage;
	}
	return NULL;
}


class FGFxFileOpener : public GFx::FileOpener
{
	public:
		// Callback used for opening swf files either from disk or from Unreal packages
		File*   OpenFile ( const char *ppath, int flags, int mode )
		{
			checkMsg ( ppath, "ppath to GFxFileOpener was NULL" );

			FFilename strFilename;

			if ( !FGFxEngine::GetPackagePath ( ppath, strFilename ) )
			{
				return SF_NEW BufferedFile ( Ptr<SysFile> ( *SF_NEW SysFile ( ppath ) ) );
			}

			File* pfile = NULL;

			// Notes: Since the movie is being created within the frame tick, the USwfMovie object
			// should be properly garbage collected.  If this were to change - say due to multi-threaded
			// garbage collection or the gfx movie instance being created over multiple frames, then
			// this "raw" movie data needs to be referenced by some persistent UObject in order to avoid
			// being cleaned by the GC prior to the gfx movie being instantiated.

			USwfMovie* pmovieInfo = LoadObject<USwfMovie> ( NULL, *strFilename, NULL, LOAD_None, NULL );

#if CONSOLE
			if ( !pmovieInfo )
			{
				// Load entire package that a movie is requested from (for seekfree loading).
				// This is needed for loading movies for FontLib.
				// The package can also be loaded in advance at an appropriate time.
				FString strPath = FString ( strFilename );
				int i = strPath.InStr ( TEXT ( "." ) );
				if ( i != -1 )
				{
					strPath = strPath.Mid ( 0, i );
					UPackage* package = UObject::LoadPackage ( NULL, *strPath, LOAD_None );
					if ( package )
					{
						package->FullyLoad();
					}
				}
				pmovieInfo = LoadObject<USwfMovie> ( NULL, *strFilename, NULL, LOAD_None, NULL );
			}
#endif

			if ( pmovieInfo )
			{
				pfile = SF_NEW FGFxFile ( ppath, &pmovieInfo->RawData ( 0 ), pmovieInfo->RawData.Num() );
			}

			if ( !pfile )
			{
				//TODO: REPLACE SO DESIGNERS CAN SEE THIS ERROR IN-GAME
				debugf ( TEXT ( "GFx attempted to load missing object [%s]" ), *strFilename );
			}
			return pfile;
		}

		SInt64      GetFileModifyTime ( const char* purl )
		{
#if !CONSOLE
			// In the editor we'll actually return a time stamp for the file modified time here.
			// Of course because the file is actually a chunk of raw data in memory it doesn't really
			// have a time stamp, so we'll instead return a time stamp that we rememeber from the
			// last time the movie was reimported within this editor session.  This is crucial for
			// dynamic reloading of flash movies, as GFx uses file time stamps to determine whether
			// existing instances of flash movies need to be discarded and replaced by new content.
#if WITH_EDITORONLY_DATA
			if ( GIsEditor )
			{
				FFilename strFilename;
				if ( FGFxEngine::GetPackagePath ( purl, strFilename ) )
				{
					USwfMovie* pmovieInfo = LoadObject<USwfMovie> ( NULL, *strFilename, NULL, LOAD_None, NULL );
					if ( pmovieInfo != NULL )
					{
						return pmovieInfo->ImportTimeStamp;
					}
				}
			}
#endif
#endif
			return 0;
		}
};

class FGFxURLBuilder : public GFx::URLBuilder
{
		void    BuildURL ( String *ppath, const LocationInfo& loc )
		{
			if ( IsPathAbsolute ( loc.FileName ) )
			{
				if ( !strncmp ( loc.FileName, GAMEDIR_PATH, GAMEDIR_PATH_LEN ) )
				{
#if CONSOLE
					*ppath = FTCHARToUTF8 ( * ( GFileManager->GetPlatformFilepath ( * ( appGameDir() +
												FString ( FUTF8ToTCHAR ( loc.FileName.Substring ( GAMEDIR_PATH_LEN, loc.FileName.GetLength() + 1 ) ) ) ) ) ) );
#else
					*ppath = FTCHARToUTF8 ( *appGameDir() );
					*ppath += loc.FileName.Substring ( GAMEDIR_PATH_LEN, loc.FileName.GetLength() + 1 );
#endif
				}
				else
				{
					*ppath = loc.FileName;
				}
			}
			else
			{
				URLBuilder::BuildURL ( ppath, loc );
			}
		}
};

// ***** FGFxImageCreator

// Override image creator to allow loading up Textures stripped from the swf movies.

class FGFxImageCreator : public GFx::ImageCreator
{
	public:
		FGFxImageCreator ( RHI::TextureManager* InTexMan ) : ImageCreator ( InTexMan )
		{
		}

		virtual Image* LoadProtocolImage ( const ImageCreateInfo& info, const String& url )
		{
			const char* purl = url.ToCStr();

			// starts with img:// or imgps://, then a full package path
			while ( *purl && *purl != '/' )
			{
				purl++;
			}
			while ( *purl && *purl == '/' )
			{
				purl++;
			}
			if ( *purl == 0 )
			{
				return NULL;
			}
			FString strFilename ( purl );
			FGFxEngine::ReplaceCharsInFString ( strFilename, TEXT ( "/" ), L'.' );

			UTexture* Texture = LoadObject<UTexture> ( NULL, *strFilename, NULL, LOAD_None, NULL );

			return GGFxEngine->CreateUTextureImage ( Texture );
		}

		virtual Image*  LoadImageFile ( const ImageCreateInfo& info, const String& url )
		{
			const char *pkgpath = IsPackagePath ( url.ToCStr() );
			check ( pkgpath );

			FString strFilename = FFilename ( pkgpath ).GetBaseFilename ( FALSE );
			strFilename = FGFxEngine::CollapseRelativePath ( strFilename );
			FGFxEngine::ReplaceCharsInFString ( strFilename, PATH_SEPARATOR, L'.' );

			UTexture* Texture = LoadObject<UTexture> ( NULL, *strFilename, NULL, LOAD_None, NULL );

			return GGFxEngine->CreateUTextureImage ( Texture );
		}

		// Loads exported image file.
		virtual Image*  LoadExportedImage ( const ImageCreateExportInfo& info, const String& url )
		{
			return LoadImageFile ( info, url );
		}
};

// ***** FGFxFSCommandHandler

// Install GFxFSCommandHandler to handle fscommand() calls from ActionScript.
class FGFxFSCommandHandler : public GFx::FSCommandHandler
{
	public:
		/** Callback through which the movie sends messages to the game */
		virtual void Callback ( Movie* pmovie, const char* pcommand, const char* parg )
		{
			check ( pmovie );

			if ( pmovie->GetUserData() && GWorld )
			{
				UGFxMoviePlayer *pumovie = ( UGFxMoviePlayer* ) pmovie->GetUserData();

				//NOTE: This code will NEED to be changed to deal with multiple instances of movies, but FSCommands should not be used for production
				//Go through all events and trigger and that references the same movie as this movie player.

				TArray<USequenceObject*> EventsToActivate;
				USequence* GameSeq;

				UBOOL bHandled = FALSE;

				GameSeq = GWorld->GetGameSequence();
				if ( GameSeq != NULL )
				{
					GameSeq->FindSeqObjectsByClass ( UGFxEvent_FSCommand::StaticClass(), EventsToActivate );
					for ( INT i = 0; i < EventsToActivate.Num(); i++ )
					{
						UGFxEvent_FSCommand* TempFSCommand = Cast<UGFxEvent_FSCommand> ( EventsToActivate ( i ) );
						if ( TempFSCommand && ( TempFSCommand->Movie == pumovie->MovieInfo ) && ( TempFSCommand->FSCommand == pcommand ) )
						{
							//match for this event!
							TempFSCommand->Handler->eventFSCommand ( pumovie, TempFSCommand, pcommand, FString ( FUTF8ToTCHAR ( parg ) ) );
							bHandled = TRUE;
						}
					}
				}
				if ( !bHandled )
				{
					debugf ( NAME_DevGFxUI, TEXT ( "FSCommand received, but GFxMoviePlayer has no FSCmdHandler!" ) );
				}

				return;
			}

			// Display the passed command to the screen
			for ( AController *Controller = GWorld->GetFirstController();
					Controller != NULL; Controller = Controller->NextController )
			{
				if ( Controller->IsA ( APlayerController::StaticClass() ) )
				{
					FString FSMessage ( pcommand );
					( ( APlayerController* ) Controller )->eventClientMessage ( FSMessage, NAME_None );
				}
			}
		}
};

class FGFxExternalInterface : public GFx::ExternalInterface
{
	public:
		virtual void Callback ( Movie* pmovie, const char* methodName, const GFx::Value* args, unsigned argCount )
		{
			check ( pmovie );
			if ( pmovie->GetUserData() )
			{
				UGFxMoviePlayer* pumovie = ( UGFxMoviePlayer* ) pmovie->GetUserData();

#if !(ANDROID || IPHONE)
				if ( methodName[0] == '_' )
				{
					//XXX pumovie->ProcessDataStoreCall(methodName, args, argCount);
					return;
				}
#endif

				UObject* pextern = pumovie->ExternalInterface;
				if ( pextern && !pextern->IsPendingKill() && !pextern->HasAnyFlags ( RF_Unreachable ) )
				{
					FString methodNames ( methodName );
					FName umethodName ( *methodNames, FNAME_Find );
					if ( umethodName == NAME_None )
					{
						return;
					}

					UFunction* pfunc = pextern->FindFunction ( umethodName );
					if ( !pfunc )
					{
						return;
					}

					BYTE* puargs = ( BYTE* ) appAlloca ( pfunc->ParmsSize );
					appMemzero ( puargs, pfunc->ParmsSize );

					UINT i = 0;
					for ( TFieldIterator<UProperty> It ( pfunc ); i < argCount && It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It, ++i )
					{
						FGFxEngine::ConvertGFxToUProp ( *It, puargs + It->Offset, args[i], pumovie );
					}

					pextern->ProcessEvent ( pfunc, puargs );

					UProperty *retprop = pfunc->GetReturnProperty();
					if ( retprop )
					{
						GFx::Value retval;

						FGFxEngine::ConvertUPropToGFx ( retprop, puargs + pfunc->ReturnValueOffset, retval, pmovie, false, true );

						if ( retval.GetType() != GFx::Value::VT_Undefined )
						{
							pmovie->SetExternalInterfaceRetVal ( retval );
						}
					}

					for ( TFieldIterator<UProperty> It ( pfunc ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
					{
						It->DestroyValue ( puargs + It->Offset );
					}
				}
			}
		}
};

#ifdef GFx_LOG
class GFxPlayerLog : public GFx::Log
{
	public:
		virtual void    LogMessageVarg ( GFx::LogMessageId messageType, const char* pfmt, va_list argList )
		{
			char buffer[1024];
#if IPHONE
			vsnprintf(buffer, 1024, pfmt, argList);
#else
			SFvsprintf ( buffer, 1024, pfmt, argList );
#endif
			FString out ( buffer );
			debugf ( NAME_DevGFxUI, TEXT ( "%s" ), *out );
		}
};
#endif

// ***** FGFxEngineBase

FGFxEngineBase::FGFxEngineBase()
	: System ( &GGFxAllocator )
{
}

/** Virtual destructor for subclassing, cleans up the allocated resources */
FGFxEngineBase::~FGFxEngineBase()
{
	Thread::FinishAllThreads();
}

// ***** FGFxEngine

FGFxEngine* FGFxEngine::GetEngine()
{
	if ( GGFxRendererInitialized )
	{
		return GGFxEngine;
	}

	GetEngineNoRender();
	GGFxEngine->InitRenderer();
	GGFxRendererInitialized = TRUE;

	return GGFxEngine;
}

#if ANDROID || IPHONE
class FGFxMultitouchInterface : public GFx::MultitouchInterface
{
public:
	virtual unsigned GetMaxTouchPoints() const { return 5; }

	virtual UInt32   GetSupportedGesturesMask() const { return MTG_Pan | MTG_Zoom | MTG_Rotate | MTG_Swipe; }

	virtual bool     SetMultitouchInputMode(MultitouchInputMode) { return true; }
};
#endif

FGFxEngine* FGFxEngine::GetEngineNoRender()
{
	if ( GGFxEngine )
	{
		return GGFxEngine;
	}

	GGFxEngine = new FGFxEngine;

	if ( NULL == GGFxGCManager )
	{
		GGFxGCManager = ConstructObject<UGFxEngine> ( UGFxEngine::StaticClass() );
		// prevent garbage collection
		GGFxGCManager->AddToRoot();
		// Initialize reference count
		GGFxGCManager->RefCount = 1;
	}
	else
	{
		GGFxGCManager->RefCount++;
	}

	return GGFxEngine;
}

void FGFxEngine::InitGFxLoaderCommon ( GFx::Loader& mLoader )
{
	Ptr<FGFxFileOpener> pfileOpener = *SF_NEW FGFxFileOpener();
	mLoader.SetFileOpener ( pfileOpener );
	Ptr<FGFxURLBuilder> purlBuilder = *SF_NEW FGFxURLBuilder();
	mLoader.SetURLBuilder ( purlBuilder );

#ifdef GFx_LOG
	mLoader.SetLog ( Ptr<GFx::Log> ( *SF_NEW GFxPlayerLog() ) );
#endif

	Ptr<GFx::ASSupport> pAS2Support = *SF_NEW GFx::AS2Support();
	mLoader.SetAS2Support ( pAS2Support );
	Ptr<GFx::ASSupport> pAS3Support = *SF_NEW GFx::AS3Support();
	mLoader.SetAS3Support ( pAS3Support );

#if ANDROID || IPHONE
	mLoader.SetMultitouchInterface(Scaleform::Ptr<GFx::MultitouchInterface>(*new FGFxMultitouchInterface()));
#endif
}

class FGFxThreadCommandQueue : public Render::ThreadCommandQueue
{
	public:
		void PushThreadCommand ( Render::ThreadCommand* command )
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER (
				FGFxRenderCommand,
				Ptr<Render::ThreadCommand>, command, command,
			{
				command->Execute();
			} );
		}
};

void FGFxEngine::InitCommonRT ( GFx::Loader& mLoader )
{
	// dynamic cache
	GlyphCacheParams fontCacheConfig;
	fontCacheConfig.TextureWidth   = 1024;
	fontCacheConfig.TextureHeight  = 1024;
	fontCacheConfig.MaxSlotHeight  = 48;
	//    fontCacheConfig.SlotPadding    = 2;
	//    fontCacheConfig.MaxNumTextures = 1;
	fontCacheConfig.TexUpdWidth = 128;
	fontCacheConfig.TexUpdHeight = 256;

	Renderer2D->GetGlyphCacheConfig()->SetParams ( fontCacheConfig );
}

void FGFxEngine::InitKeyMap()
{
    SF_ASSERT ( 'N' == Key::N );
    SF_ASSERT ( '4' == Key::Num4 );
    TCHAR ch[3];
    ch[1] = 0;
    for ( ch[0] = '0'; ch[0] <= '9'; ch[0]++ )
    {
        KeyCodes.Set ( FName ( ch ).GetIndex(), ( Key::Code ) ch[0] );
    }
    for ( ch[0] = 'A'; ch[0] <= 'Z'; ch[0]++ )
    {
        KeyCodes.Set ( FName ( ch ).GetIndex(), ( Key::Code ) ch[0] );
    }
    ch[0] = L'F';
    ch[2] = 0;
    for ( ch[1] = '1'; ch[1] <= '9'; ch[1]++ )
    {
        KeyCodes.Set ( FName ( ch ).GetIndex(), ( Key::Code ) ( Key::F1 + ch[1] - '1' ) );
    }
    KeyCodes.Set ( FName ( TEXT ( "F10" ) ).GetIndex(), Key::F10 );
    KeyCodes.Set ( FName ( TEXT ( "F11" ) ).GetIndex(), Key::F11 );
    KeyCodes.Set ( FName ( TEXT ( "F12" ) ).GetIndex(), Key::F12 );

    KeyCodes.Set ( FName ( TEXT ( "Zero" ) ).GetIndex(), ( Key::Code ) '0' );
    KeyCodes.Set ( FName ( TEXT ( "One" ) ).GetIndex(), ( Key::Code ) '1' );
    KeyCodes.Set ( FName ( TEXT ( "Two" ) ).GetIndex(), ( Key::Code ) '2' );
    KeyCodes.Set ( FName ( TEXT ( "Three" ) ).GetIndex(), ( Key::Code ) '3' );
    KeyCodes.Set ( FName ( TEXT ( "Four" ) ).GetIndex(), ( Key::Code ) '4' );
    KeyCodes.Set ( FName ( TEXT ( "Five" ) ).GetIndex(), ( Key::Code ) '5' );
    KeyCodes.Set ( FName ( TEXT ( "Six" ) ).GetIndex(), ( Key::Code ) '6' );
    KeyCodes.Set ( FName ( TEXT ( "Seven" ) ).GetIndex(), ( Key::Code ) '7' );
    KeyCodes.Set ( FName ( TEXT ( "Eight" ) ).GetIndex(), ( Key::Code ) '8' );
    KeyCodes.Set ( FName ( TEXT ( "Nine" ) ).GetIndex(), ( Key::Code ) '9' );

    KeyCodes.Set ( FName ( TEXT ( "NumPadZero" ) ).GetIndex(), Key::KP_0 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadOne" ) ).GetIndex(), Key::KP_1 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadTwo" ) ).GetIndex(), Key::KP_2 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadThree" ) ).GetIndex(), Key::KP_3 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadFour" ) ).GetIndex(), Key::KP_4 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadFive" ) ).GetIndex(), Key::KP_5 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadSix" ) ).GetIndex(), Key::KP_6 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadSeven" ) ).GetIndex(), Key::KP_7 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadEight" ) ).GetIndex(), Key::KP_8 );
    KeyCodes.Set ( FName ( TEXT ( "NumPadNine" ) ).GetIndex(), Key::KP_9 );
    KeyCodes.Set ( FName ( TEXT ( "Multiply" ) ).GetIndex(), Key::KP_Multiply );
    KeyCodes.Set ( FName ( TEXT ( "Add" ) ).GetIndex(), Key::KP_Add );
    KeyCodes.Set ( FName ( TEXT ( "Subtract" ) ).GetIndex(), Key::KP_Subtract );
    KeyCodes.Set ( FName ( TEXT ( "Decimal" ) ).GetIndex(), Key::KP_Decimal );
    KeyCodes.Set ( FName ( TEXT ( "Divide" ) ).GetIndex(), Key::KP_Divide );

    KeyCodes.Set ( FName ( TEXT ( "Backspace" ) ).GetIndex(), Key::Backspace );
    KeyCodes.Set ( FName ( TEXT ( "Tab" ) ).GetIndex(), Key::Tab );
    KeyCodes.Set ( FName ( TEXT ( "Clear" ) ).GetIndex(), Key::Clear );
    KeyCodes.Set ( FName ( TEXT ( "Enter" ) ).GetIndex(), Key::Return );
    KeyCodes.Set ( FName ( TEXT ( "LeftShift" ) ).GetIndex(), Key::Shift );
    KeyCodes.Set ( FName ( TEXT ( "LeftControl" ) ).GetIndex(), Key::Control );
    KeyCodes.Set ( FName ( TEXT ( "LeftAlt" ) ).GetIndex(), Key::Alt );
    KeyCodes.Set ( FName ( TEXT ( "RightShift" ) ).GetIndex(), Key::Shift );
    KeyCodes.Set ( FName ( TEXT ( "RightControl" ) ).GetIndex(), Key::Control );
    KeyCodes.Set ( FName ( TEXT ( "RightAlt" ) ).GetIndex(), Key::Alt );
    KeyCodes.Set ( FName ( TEXT ( "CapsLock" ) ).GetIndex(), Key::CapsLock );
    KeyCodes.Set ( FName ( TEXT ( "Escape" ) ).GetIndex(), Key::Escape );
    KeyCodes.Set ( FName ( TEXT ( "SpaceBar" ) ).GetIndex(), Key::Space );
    KeyCodes.Set ( FName ( TEXT ( "PageUp" ) ).GetIndex(), Key::PageUp );
    KeyCodes.Set ( FName ( TEXT ( "PageDown" ) ).GetIndex(), Key::PageDown );
    KeyCodes.Set ( FName ( TEXT ( "End" ) ).GetIndex(), Key::End );
    KeyCodes.Set ( FName ( TEXT ( "Home" ) ).GetIndex(), Key::Home );
    KeyCodes.Set ( FName ( TEXT ( "Left" ) ).GetIndex(), Key::Left );
    KeyCodes.Set ( FName ( TEXT ( "Up" ) ).GetIndex(), Key::Up );
    KeyCodes.Set ( FName ( TEXT ( "Right" ) ).GetIndex(), Key::Right );
    KeyCodes.Set ( FName ( TEXT ( "Down" ) ).GetIndex(), Key::Down );
    KeyCodes.Set ( FName ( TEXT ( "Insert" ) ).GetIndex(), Key::Insert );
    KeyCodes.Set ( FName ( TEXT ( "Delete" ) ).GetIndex(), Key::Delete );
    KeyCodes.Set ( FName ( TEXT ( "Help" ) ).GetIndex(), Key::Help );
    KeyCodes.Set ( FName ( TEXT ( "NumLock" ) ).GetIndex(), Key::NumLock );
    KeyCodes.Set ( FName ( TEXT ( "ScrollLock" ) ).GetIndex(), Key::ScrollLock );
    KeyCodes.Set ( FName ( TEXT ( "Semicolon" ) ).GetIndex(), Key::Semicolon );
    KeyCodes.Set ( FName ( TEXT ( "Equal" ) ).GetIndex(), Key::Equal );
    KeyCodes.Set ( FName ( TEXT ( "Comma" ) ).GetIndex(), Key::Comma );
    KeyCodes.Set ( FName ( TEXT ( "Underscore" ) ).GetIndex(), Key::Minus );
    KeyCodes.Set ( FName ( TEXT ( "Period" ) ).GetIndex(), Key::Period );
    KeyCodes.Set ( FName ( TEXT ( "Slash" ) ).GetIndex(), Key::Slash );
    KeyCodes.Set ( FName ( TEXT ( "Bar" ) ).GetIndex(), Key::Bar );
    KeyCodes.Set ( FName ( TEXT ( "LeftBracket" ) ).GetIndex(), Key::BracketLeft );
    KeyCodes.Set ( FName ( TEXT ( "Backslash" ) ).GetIndex(), Key::Backslash );
    KeyCodes.Set ( FName ( TEXT ( "RightBracket" ) ).GetIndex(), Key::BracketRight );
    KeyCodes.Set ( FName ( TEXT ( "Quote" ) ).GetIndex(), Key::Quote );

    KeyCodes.Set ( FName ( TEXT ( "LeftMouseButton" ) ).GetIndex(), UGFxInput ( Key::None, 0 ) );
    KeyCodes.Set ( FName ( TEXT ( "RightMouseButton" ) ).GetIndex(), UGFxInput ( Key::None, 1 ) );
    KeyCodes.Set ( FName ( TEXT ( "MiddleMouseButton" ) ).GetIndex(), UGFxInput ( Key::None, 2 ) );
    KeyCodes.Set ( FName ( TEXT ( "MouseScrollUp" ) ).GetIndex(), UGFxInput ( Key::None, 4 ) );
    KeyCodes.Set ( FName ( TEXT ( "MouseScrollDown" ) ).GetIndex(), UGFxInput ( Key::None, 3 ) );

    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_RS_Up" ) ).GetIndex(), Key::F1 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_RS_Down" ) ).GetIndex(), Key::F2 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_RS_Left" ) ).GetIndex(),	Key::F3 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_RS_Right" ) ).GetIndex(), Key::F4 );

    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_A" ) ).GetIndex(), Key::KP_0 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_B" ) ).GetIndex(), Key::KP_1 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_X" ) ).GetIndex(), Key::KP_2 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_Y" ) ).GetIndex(), Key::KP_3 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_L1" ) ).GetIndex(), Key::KP_4 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_L2" ) ).GetIndex(), Key::KP_5 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_L3" ) ).GetIndex(), Key::KP_6 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_R1" ) ).GetIndex(), Key::KP_7 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_R2" ) ).GetIndex(), Key::KP_8 );
    KeyCodes.Set ( FName ( TEXT ( "GAMEPAD_R3" ) ).GetIndex(), Key::KP_9 );
    KeyCodes.Set ( FName ( TEXT ( "START" ) ).GetIndex(), Key::KP_Multiply );
    KeyCodes.Set ( FName ( TEXT ( "BACK" ) ).GetIndex(), Key::KP_Add );

    TArray<FString> Config;
    if ( GConfig->GetSection ( TEXT ( "Scaleform.KeyMap" ), Config, GInputIni ) )
    {
        for ( INT line = 0; line < Config.Num(); line++ )
        {
            FString label, ffont;
            for ( INT pos = 0; pos < Config ( line ).Len(); pos++ )
            {
                if ( Config ( line ) [pos] == TEXT ( '=' ) )
                {
                    if ( Config ( line ).Mid ( 0, pos ) == TEXT ( "FullKeyboard" ) && Config ( line ) [pos + 1] == TEXT ( '1' ) )
                    {
                        KeyMap = KeyCodes;
                    }
                    else
                    {
                        UGFxInput* key = KeyCodes.Find ( FName ( *Config ( line ).Mid ( pos + 1 ) ).GetIndex() );
                        if ( key )
                        {
                            KeyMap.Set ( FName ( *Config ( line ).Mid ( 0, pos ) ).GetIndex(), *key );
                        }
                        else
                        {
                            debugf ( TEXT ( "bad key name %s in Scaleform keymap" ), *Config ( line ).Mid ( pos + 1 ) );
                        }
                    }
                }
            }
        }
    }
    else
    {
        KeyMap = KeyCodes;
    }
    KeyCodes.Empty();
}

void FGFxEngine::InitGamepadMouse()
{
    FString MouseX, MouseY;
    if ( GConfig->GetString ( TEXT ( "Scaleform.GamepadMouse" ), TEXT ( "X" ), MouseX, GInputIni ) &&
         GConfig->GetString ( TEXT ( "Scaleform.GamepadMouse" ), TEXT ( "Y" ), MouseY, GInputIni ) )
    {
        const TCHAR* pMouseX = *MouseX;
        const TCHAR* pMouseY = *MouseY;
        if ( *pMouseX == TEXT ( '-' ) )
        {
            pMouseX++;
            AxisMouseInvert[0] = 1;
        }
        else
        {
            AxisMouseInvert[0] = 0;
        }
        AxisMouseEmulation[0] = FName ( pMouseX );

        if ( *pMouseY == TEXT ( '-' ) )
        {
            pMouseY++;
            AxisMouseInvert[1] = 1;
        }
        else
        {
            AxisMouseInvert[1] = 0;
        }
        AxisMouseEmulation[1] = FName ( pMouseY );
    }
    else
    {
        AxisMouseEmulation[0] = AxisMouseEmulation[1] = NAME_None;
    }

    // use same axis emulation configuration as UIScene
    const TArray<FUIAxisEmulationDefinition>& EmulationDefinitions =
        UUIInteraction::StaticClass()->GetDefaultObject<UUIInteraction>()->ConfiguredAxisEmulationDefinitions;

    AxisEmulationDefinitions.Empty();
    for ( INT KeyIndex = 0; KeyIndex < EmulationDefinitions.Num(); KeyIndex++ )
    {
        const FUIAxisEmulationDefinition& Definition = EmulationDefinitions ( KeyIndex );
        AxisEmulationDefinitions.Set ( Definition.AxisInputKey, Definition );
    }

    AxisRepeatDelay = UUIInteraction::StaticClass()->GetDefaultObject<UUIInteraction>()->AxisRepeatDelay;
    UIJoystickDeadZone = UUIInteraction::StaticClass()->GetDefaultObject<UUIInteraction>()->UIJoystickDeadZone;
}

/** Default constructor, allocates the required resources */
FGFxEngine::FGFxEngine()
	: CurveError ( DEFAULT_CURVE_ERROR ), MousePos ( 0, 0 ), Time( 0.0 )
{
	check ( GGFxEngine == NULL );
	GGFxEngine = this;

	InitGFxLoaderCommon ( mLoader );

	Ptr<FGFxFSCommandHandler> pfscommandHandler = *SF_NEW FGFxFSCommandHandler();
	mLoader.SetFSCommandHandler ( pfscommandHandler );
	Ptr<FGFxExternalInterface> pexternalinterface = *SF_NEW FGFxExternalInterface();
	mLoader.SetExternalInterface ( pexternalinterface );

#if 0       // not using libjpeg currently
	Ptr<GFx::ImageFileHandlerRegistry> pimgReg = *SF_NEW GFx::ImageFileHandlerRegistry();
	pimgReg->AddHandler ( &Render::JPEG::FileReader::Instance );
	mLoader.SetImageFileHandlerRegistry ( pimgReg );
#endif

	HudViewport     = NULL;
	HudRenderTarget = NULL;
	SceneColorRT    = NULL;

    InitKeyMap();
    InitGamepadMouse();

#ifndef GFX_NO_LOCALIZATION
	InitLocalization();
#endif

	Ptr<FGFxUFontProvider> fontProvider = *SF_NEW FGFxUFontProvider();
	mLoader.SetFontProvider ( fontProvider );

#if WITH_GFx_FULLSCREEN_MOVIE
	FullscreenMovie = NULL;
#endif

#if WITH_REALD
	bStereoEnabledLastFrame = !RealD::IsStereoEnabled();
	StereoScreenWidth = -1;
#endif
}

void FGFxEngine::InitRenderer()
{
	GGFxRendererInitialized = TRUE;
	RenderHal = *SF_NEW Render::RHI::HAL (new FGFxThreadCommandQueue);

#if WITH_MOBILE_RHI || !CONSOLE
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	(GFxInitHalCommand,
		Ptr<Render::RHI::HAL>, Hal, RenderHal,
	{
		Hal->InitHAL(Render::RHI::HALInitParams());
	});

#if !EXPERIMENTAL_FAST_BOOT_IPHONE
	// this blocks the game thread during boot up when there are long RT comands (like shader compiling)
	// so we block later to shave a couple seconds off startup time
	FlushRenderingCommands();
#endif

#else
	RenderHal->InitHAL(Render::RHI::HALInitParams());
#endif

	Renderer2D = *SF_NEW Render::Renderer2D(RenderHal);

	Ptr<FGFxImageCreator> ImageCreator = *SF_NEW FGFxImageCreator ( RenderHal->GetTextureManager() );
	mLoader.SetImageCreator ( ImageCreator );

#if WITH_GFx_AUDIO
    pSoundDevice = NULL;
    InitSound();
#endif

#if WITH_GFx_VIDEO
	// Core video support
#if defined(SF_OS_WIN32)
	UInt32 affinityMasks[] =
	{
		DEFAULT_VIDEO_DECODING_AFFINITY_MASK,
		DEFAULT_VIDEO_DECODING_AFFINITY_MASK,
		DEFAULT_VIDEO_DECODING_AFFINITY_MASK
	};
	Ptr<Video::Video> pvideo = *SF_NEW Video::VideoPC ( Video::VideoVMSupportAll(),
								Thread::NormalPriority, MAX_VIDEO_DECODING_THREADS, affinityMasks );
#elif defined(SF_OS_XBOX360)
	Ptr<Video::Video> pvideo = *SF_NEW Video::VideoXbox360 ( Video::VideoVMSupportAll(),
								Video::Xbox360_Proc4 | Video::Xbox360_Proc5, Thread::NormalPriority,
								Video::Xbox360_Proc3 );
#elif defined(SF_OS_PS3)
    const UInt8 videoSpuNum = 3;
    UInt8 spursPrios[8];
    for (int i = 0; i < 8; i++)
    {
        spursPrios[i] = ((i < videoSpuNum) ? 1 : 0);
    }
    Ptr<Video::Video> pvideo = *SF_NEW Video::VideoPS3 ( Video::VideoVMSupportAll(),
                                GSPURS, 1, Thread::NormalPriority, videoSpuNum, spursPrios );
#else
	Ptr<Video::Video> pvideo = *SF_NEW Video::Video ( Video::VideoVMSupportAll(), Thread::NormalPriority );
#endif

    InitVideoSound( pvideo );
    pVideo = pvideo;

	mLoader.SetVideo ( pVideo );

#endif // WITH_GFx_VIDEO

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER (
		FGFxRenderCreateRHI,
		FGFxEngine*, pEngine, this,
	{
		pEngine->InitCommonRT ( pEngine->mLoader );
	} );
}

FGFxEngine::~FGFxEngine()
{
	// Close all outstanding scenes
	CloseAllMovies ( FALSE );
	CloseAllTextureMovies();

	DeleteQueuedMovies ( FALSE );

	// Make sure that the TextureManager is shutdown with the HAL.
	mLoader.SetImageCreator ( NULL );

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
	( GFxShutdownHalCommand,
		Ptr<Render::RHI::HAL>, Hal, RenderHal,
	{
		Hal->SetRenderTarget ( 0, false );
		Hal->ShutdownHAL();
	} );

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER
	( GFxDestroyRenderBuffers,
		Render::RenderTarget*, Target0, HudRenderTarget,
		Render::RenderTarget*, Target1, SceneColorRT,
	{
		delete Target0;
		delete Target1;
	} );


#if WITH_GFx_FULLSCREEN_MOVIE
	if ( FullscreenMovie )
	{
		FFullScreenMovieGFx::Shutdown();
		ENQUEUE_UNIQUE_RENDER_COMMAND
		(
			FShutdownFullScreenMovieGFxCommand,
			FFullScreenMovieGFx::Shutdown();
		);
	}
#endif

	// Wait until the HAL, targets and FullScreenMovie are deleted on the render thread.
	// ~GFxEngineBase will destroy the main heap, so they need to complete before it is executed.
	FlushRenderingCommands();

	Renderer2D = NULL;
	RenderHal = NULL;

#if WITH_GFx_AUDIO
    ShutdownSound();
#endif
}

void FGFxEngine::NotifyGameSessionEnded()
{
	CloseAllMovies ( TRUE );
	CloseAllTextureMovies();

	while ( DeleteMovies.Num() )
	{
		DeleteQueuedMovies();
	}

	for ( INT i = 0; i < SDPG_MAX; i++ )
	{
		for ( INT j = 0; j < DPGOpenMovies[i].Num(); j++ )
		{
			// if the movie pointer is bad, or it should be closed, or it is running in game time (only
			// real time movies can persist across level loads) then close it!
			if ( ! ( DPGOpenMovies[i] ( j )->pUMovie ) ||
					DPGOpenMovies[i] ( j )->pUMovie->bCloseOnLevelChange ||
					DPGOpenMovies[i] ( j )->pUMovie->TimingMode == TM_Game )
			{
				DPGOpenMovies[i].Remove ( j );
				j--; //So we stay in place for the next one
			}
		}
	}
}

/**
	*	Close all movies
	*
	*	@param bOnlyCloseOnLevelChangeMovies - If TRUE, only close movies that have their bCloseOnLevelChange flags set to true
	*/
void FGFxEngine::CloseAllMovies ( UBOOL bOnlyCloseOnLevelChangeMovies )
{
	// Iterate over all open movies, closing the appropriate ones
	for ( INT i = OpenMovies.Num() - 1; i >= 0; i-- )
	{
		FGFxMovie* Movie = OpenMovies ( i );
		if ( Movie->pUMovie )
		{
			check ( Movie->pUMovie->pMovie );
			if ( !bOnlyCloseOnLevelChangeMovies || Movie->pUMovie->bCloseOnLevelChange )
			{
				Movie->pUMovie->Close ( 1 );
			}
		}
		else
		{
			CloseScene ( Movie, 1 );
		}
	}

	// Close all other (inactive) movies if any
	for ( INT i = AllMovies.Num() - 1; i >= 0; i-- )
	{
		FGFxMovie* Movie = AllMovies ( i );
		if ( Movie->pUMovie )
		{
			check ( Movie->pUMovie->pMovie );
			if ( !bOnlyCloseOnLevelChangeMovies || Movie->pUMovie->bCloseOnLevelChange )
			{
				Movie->pUMovie->Close ( 1 );
			}
		}
	}
}

void FGFxEngine::CloseAllTextureMovies()
{
	while ( TextureMovies.Num() )
	{
		FGFxMovie* Movie = TextureMovies ( TextureMovies.Num() - 1 );
		if ( Movie->pUMovie )
		{
			check ( Movie->pUMovie->pMovie );
			Movie->pUMovie->Close ( 1 );
		}
		else
		{
			CloseScene ( Movie, 1 );
		}
	}
}

/** Closes the topmost scene of the stack */
void FGFxEngine::CloseTopmostScene()
{
	if ( OpenMovies.Num() > 0 )
	{
		if ( OpenMovies ( OpenMovies.Num() - 1 )->pUMovie )
		{
			check ( OpenMovies ( OpenMovies.Num() - 1 )->pUMovie->pMovie );
			OpenMovies ( OpenMovies.Num() - 1 )->pUMovie->Close ( 1 );
		}
		else
		{
			CloseScene ( OpenMovies ( OpenMovies.Num() - 1 ), 0 );
		}
	}
}

/** Stops the playback of any scene currently playing, cleans up resources allocated to it */
void FGFxEngine::CloseScene ( FGFxMovie* pMovie, UBOOL fDelete )
{
	check ( pMovie != NULL );

	pMovie->Playing = FALSE;

	INT index;
	if ( OpenMovies.FindItem ( pMovie, index ) )
	{
		OpenMovies.Remove ( index );
		for ( INT i = 0; i <= SDPG_PostProcess; i++ )
		{
			DPGOpenMovies[i].RemoveItem ( pMovie );
		}
	}
	else if ( TextureMovies.FindItem ( pMovie, index ) )
	{
		TextureMovies.Remove ( index );
	}

	if ( fDelete || pMovie->pUMovie == NULL )
	{
		DeleteMovies.AddItem ( pMovie );
		pMovie->RenderCmdFence.BeginFence();
	}

	//Must reevaluate focus before the linkage to the pUMovie is lost, as focus events will be thrown
	ReevaluateFocus();

	if ( fDelete && pMovie->pUMovie )
	{
		pMovie->pUMovie->pMovie = 0;
		pMovie->pUMovie = 0;
	}
}

/** Set the Renderer's viewport to the viewport specified */
void FGFxEngine::SetRenderViewport ( FViewport* pinViewport )
{
	HudViewport = pinViewport;
	if ( !HudViewport )
	{
		if ( HudRenderTarget )
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER (
				FGFxDeleteHudRenderTarget,
				FGFxEngine*, pEngine, this,
			{
				pEngine->RenderHal->SetRenderTarget ( NULL, FALSE );
				delete pEngine->HudRenderTarget;
				pEngine->HudRenderTarget = NULL;
			} );
		}
		return;
	}

#if (WITH_GFx_IME && defined(WIN32))
	if ( pImeManager )
	{
		GFx::IMEManagerBase *pime = pImeManager;
		( ( GFx::IME::GFxIMEManagerWin32* ) pime )->SetWndHandle ( ( HWND ) HudViewport->GetWindow() );
	}
#endif

	// Handle the nasty case where movies can be created before viewports are set up...
	for ( TArray<FGFxMovie*>::TIterator it ( OpenMovies ); it; ++it )
	{
		GFx::Viewport vp;

		if ( ( *it )->fViewportSet )
		{
			( *it )->pView->GetViewport ( &vp );
		}
		else
		{
			UINT SizeX = pinViewport->GetSizeX();
			vp.Left = vp.Top = 0;
			vp.Width = SizeX;
			vp.Height = pinViewport->GetSizeY();

			vp.Flags |= ( *it )->pUMovie && ( *it )->pUMovie->bEnableGammaCorrection == 0 ? Render::RHI::Viewport_NoGamma : 0;
		}

#if WITH_REALD
		if (RealD::IsStereoEnabled())
		{
			vp.Flags |= Render::Viewport::View_Stereo_SplitH;
		}
#endif

		vp.BufferWidth = pinViewport->GetSizeX();
		vp.BufferHeight = pinViewport->GetSizeY();
		( *it )->pView->SetViewport ( vp );
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER (
		FGFxSetHudRenderTarget,
		FGFxEngine*, pEngine, this,
		FViewport*, HudViewport, HudViewport,
	{
		if ( pEngine->HudRenderTarget )
		{
			pEngine->RenderHal->SetRenderTarget ( NULL, FALSE );
			delete pEngine->HudRenderTarget;
		}
		GSceneRenderTargets.Allocate(HudViewport->GetSizeX(),HudViewport->GetSizeY());
		pEngine->HudRenderTarget = pEngine->RenderHal->CreateRenderTargetFromViewport ( HudViewport, true );
		pEngine->RenderHal->SetRenderTarget ( pEngine->HudRenderTarget, TRUE );
	} );
}

INT  FGFxEngine::GetNextMovieDPG ( INT FirstDPG )
{
	for ( INT i = FirstDPG + 1; i < SDPG_PostProcess; i++ )
	{
		if ( DPGOpenMovies[i].Num() )
		{
			return i;
		}
	}

	return SDPG_PostProcess;
}

void FGFxEngine::DeleteQueuedMovies ( UBOOL wait )
{
	if ( DeleteMovies.Num() == 0 )
	{
		return;
	}

	TArray<FGFxMovie*> NewDeleteMovies;
	for ( INT i = 0; i < DeleteMovies.Num(); ++i )
	{
		FGFxMovie* pMovie = DeleteMovies ( i );

		// Release movies.
		if ( pMovie )
		{
			if ( wait == FALSE || pMovie->RenderCmdFence.GetNumPendingFences() == 0 )
			{
				pMovie->pView = 0;
				pMovie->pDef  = 0;
				if ( pMovie->GFxRenderTarget && *pMovie->GFxRenderTarget )
				{
					ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER (
						FGFxDeleteTextureRenderTarget,
						FGFxEngine*, pEngine, this,
						RenderTarget*, RT, *pMovie->GFxRenderTarget,
					{
						RT->Release();
					} );
				}
				delete pMovie->GFxRenderTarget;
				// UMovie is garbage collected
				delete pMovie;
			}
			else
			{
				NewDeleteMovies.AddItem ( pMovie );
			}
		}
	}
	DeleteMovies = NewDeleteMovies;
}

DECLARE_CYCLE_COUNTER ( STAT_GFxTickUI );
DECLARE_CYCLE_COUNTER ( STAT_GFxTickRTT );

/**
	* Called once a frame to update the played movie's state.
	* @param	DeltaTime - The time since the last frame.
	*/
void FGFxEngine::Tick ( FLOAT DeltaTime )
{
	Time = Time + (DOUBLE)(DeltaTime);

	// Tick all loaded movies from back to front...
	const DOUBLE gtime = GWorld->GetTimeSeconds();
	const DOUBLE rtime = Time;

	{
		for ( INT i = 0; i < OpenMovies.Num(); ++i )
		{
			//moved inside the loop to track the number of open movies
		SCOPE_CYCLE_COUNTER ( STAT_GFxTickUI );

			FGFxMovie& movie = *OpenMovies ( i );
			DOUBLE      time = ( movie.TimingMode == TM_Real ? rtime : gtime );
			DOUBLE     delta = ( movie.LastTime == 0.0 ) ? 0.0 : ( time - movie.LastTime );

			movie.LastTime = time;

			if ( movie.fUpdate && IsRenderingEnabled())
			{
				if (IsHudEnabled() || (movie.pUMovie && movie.pUMovie->bDisplayWithHudOff))
				{
					movie.pView->Advance ( delta );
				}
				else
				{
					movie.pView->Advance(delta, 2, false);
				}

				UGFxMoviePlayer* MoviePlayer = movie.pUMovie;
				if ( MoviePlayer != NULL )
				{
					MoviePlayer->PostAdvance ( delta );
				}
			}
		}
	}
	{

		for ( INT i = 0; i < TextureMovies.Num(); ++i )
		{
			//moved inside the loop to track the number of open movies
			SCOPE_CYCLE_COUNTER ( STAT_GFxTickRTT );

			FGFxMovie& movie = *TextureMovies ( i );
			DOUBLE     time = ( movie.TimingMode == TM_Real ? rtime : gtime );
			DOUBLE     delta = ( movie.LastTime == 0.0 ) ? 0.0 : ( time - movie.LastTime );

			movie.LastTime = time;

			if ( movie.fUpdate && IsRenderingEnabled() )
			{
				movie.pView->Advance ( delta );

				UGFxMoviePlayer* MoviePlayer = movie.pUMovie;
				if ( MoviePlayer != NULL )
				{
					MoviePlayer->PostAdvance ( delta );
				}
			}
		}
	}
}

void FGFxEngine::UpdateRenderStats()
{
#if STATS
    bool resetStats = true;
    Render::HAL::Stats RenderStats;
    SF_AMP_CODE(resetStats = !AmpServer::GetInstance().IsValidConnection());
    RenderHal->GetStats(&RenderStats, resetStats);

    SET_DWORD_STAT(STAT_GFxMeshesDrawn, RenderStats.Meshes );
    SET_DWORD_STAT(STAT_GFxTrianglesDrawn, RenderStats.Triangles );
    SET_DWORD_STAT(STAT_GFxPrimitivesDrawn, RenderStats.Primitives );

#endif // STATS
}

struct FGFxMovieRenderParams
{
	FGFxMovieRenderParams() :
		bRenderToSceneColor ( FALSE ),
		bDoNextCapture ( TRUE ) { };

	TArray<FGFxMovie>   Movies;
	UBOOL               bRenderToSceneColor;
	UBOOL               bDoNextCapture;
};

/** RenderThread implementation for RenderUI function */
void FGFxEngine::RenderUI_RenderThread(struct FGFxMovieRenderParams* Params)
{
	// Extra scoping for Scaleform AMP
	{
		// Scope timer should not include AmpServer::AdvanceFrame, where stats are reported
		SF_AMP_SCOPE_RENDER_TIMER_ID("RenderThread::drawFrame", Amp_Native_Function_Id_Display);

		SCOPE_CYCLE_COUNTER( STAT_GFxRenderUIRT );
		if ( GEmitDrawEvents )
		{
			appBeginDrawEvent ( DEC_SCENE_ITEMS, TEXT ( "RenderScaleform" ) );
		}

		// trigger to stop playing loading screen movies
		GGFxEngine->GameHasRendered++;

		// ensure depth buffer is large enough
		GSceneRenderTargets.Allocate(HudViewport->GetSizeX(), HudViewport->GetSizeY());

		if (Params->bRenderToSceneColor)
		{
			GSceneRenderTargets.BeginRenderingSceneColor();
			RenderHal->UpdateRenderTarget(SceneColorRT);
			RenderHal->SetRenderTarget(SceneColorRT, FALSE);
		}
		else
		{
			RenderHal->UpdateRenderTarget(HudRenderTarget);
			RenderHal->SetRenderTarget(HudRenderTarget, TRUE);
		}

#if WITH_REALD
		if (StereoScreenWidth != RealD::RealDStereo::ScreenWidth)
		{
			Render::StereoParams stereo;
			stereo.DisplayDiagInches = sqrt(RealD::RealDStereo::ScreenWidth * RealD::RealDStereo::ScreenWidth * FLOAT(GScreenHeight)/FLOAT(GScreenWidth) ) / 2.54f;
			stereo.EyeSeparationCm = 6.5f;
			stereo.Distortion = 0.4f;

			RenderHal->SetStereoParams( stereo );
			StereoScreenWidth = RealD::RealDStereo::ScreenWidth;
		}

		INT MaxRenderPass = ((GEngine != NULL) && GEngine->IsRealDStereoEnabled()) ? 2 : 1;
		for (INT RenderPass = 0; RenderPass < MaxRenderPass; ++RenderPass) 
		{
			if ( RenderPass == 0)
			{
				if ( MaxRenderPass == 1 )
				{
					RenderHal->SetStereoDisplay(Render::StereoCenter);
				}
				else
				{
					RenderHal->SetStereoDisplay(Render::StereoRight);
				}
			}
			else
			{
				RenderHal->SetStereoDisplay(Render::StereoLeft);
			}
#else
		{
#endif
			RenderHal->BeginScene();

			for (INT i = 0; i < Params->Movies.Num(); ++i)
			{
				FGFxMovie& movie = Params->Movies( i );
				UBOOL bRenderIt = (Params->bDoNextCapture == FALSE)
#if WITH_REALD
					|| RenderPass > 0
#endif
					|| movie.DispHandle.NextCapture(Renderer2D->GetContextNotify());

				if (bRenderIt == TRUE)
			{
				bool hasViewport = movie.DispHandle.GetRenderEntry()->GetDisplayData()->HasViewport();
				if ( hasViewport )
				{
					const Render::Viewport &vp3d = movie.DispHandle.GetRenderEntry()->GetDisplayData()->VP;
					Render::Rect<int> viewRect ( vp3d.Left, vp3d.Top, vp3d.Left + vp3d.Width, vp3d.Top + vp3d.Height );
						RenderHal->SetFullViewRect( viewRect );
				}

					Renderer2D->Display(movie.DispHandle);
				}
			}
			RenderHal->EndScene();
		}

		ReleaseOwnershipOfRenderTargets(RenderHal);

		if ( Params->bRenderToSceneColor )
		{
			GSceneRenderTargets.FinishRenderingSceneColor ( TRUE );
		}

		if ( GEmitDrawEvents )
		{
			appEndDrawEvent();
		}
	}

	UpdateRenderStats();

	// Placing the AMP update call here avoids the need 
	// to explicitly call it every frame from the main loop
	SF_AMP_CODE(AmpServer::GetInstance().AdvanceFrame();)
}

/** Render all the scaleform overlay movies that are currently playing */
void FGFxEngine::RenderUI ( UBOOL bRenderToSceneColor, INT DPG )
{
	if ( !GDrawGFx )
	{
		return;
	}

	if ( !Renderer2D || !HudViewport || DPGOpenMovies[DPG].Num() == 0 )
	{
		return;
	}

	FGFxMovieRenderParams Params;

#if WITH_REALD
	if ( bStereoEnabledLastFrame != GEngine->IsRealDStereoEnabled() )
	{
		bStereoEnabledLastFrame = GEngine->IsRealDStereoEnabled();

		for ( INT i = 0; i < OpenMovies.Num(); ++i )
		{
			FGFxMovie& movie = *OpenMovies ( i );
			Scaleform::GFx::Viewport vp;
			movie.pView->GetViewport(&vp);
			vp.Flags = (vp.Flags & ~Render::Viewport::View_Stereo_SplitH) | (bStereoEnabledLastFrame ? Render::Viewport::View_Stereo_SplitH : 0);
			movie.pView->SetViewport(vp);
		}
	}
#endif

	// Render all loaded movies from back to front...
	for ( INT i = 0; i < DPGOpenMovies[DPG].Num(); ++i )
	{
		FGFxMovie& movie = *DPGOpenMovies[DPG] ( i );
		if ( movie.fVisible && IsHudEnabled() || ( movie.pUMovie && movie.pUMovie->bDisplayWithHudOff ) )
		{
			Params.Movies.AddItem ( movie );
		}
	}

	Params.bDoNextCapture      = TRUE;
#if XBOX
	// Prepass (certain systems only)
	Params.bRenderToSceneColor = FALSE;
	DrawPrepass ( Params );
	Params.bDoNextCapture      = FALSE;
#endif

	Params.bRenderToSceneColor = bRenderToSceneColor;

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER (
		FGFxRenderUI,
		FGFxEngine*, pEngine, this,
		FGFxMovieRenderParams, Params, Params,
	{
		pEngine->RenderUI_RenderThread(&Params);
	} );
}

FGFxMovie* FGFxEngine::GetTopmostMovie() const
{
	if ( OpenMovies.Num() == 0 )
	{
		return NULL;
	}

	return OpenMovies ( OpenMovies.Num() - 1 );
}

/** Returns the number of movies in the open movie list */
INT FGFxEngine::GetNumOpenMovies() const
{
	return OpenMovies.Num();
}

/** Returns an open movie from the open movie list at index Idx */
FGFxMovie* FGFxEngine::GetOpenMovie ( INT Idx ) const
{
	return ( OpenMovies.IsValidIndex ( Idx ) ) ? OpenMovies ( Idx ) : NULL;
}

/** Returns the number of all movies */
INT FGFxEngine::GetNumAllMovies() const
{
	return AllMovies.Num();
}


DWORD GFxRenderTexturesRTStartCycles = 0;

void FGFxEngine::RenderTextures()
{
	DeleteQueuedMovies();

	if ( !Renderer2D )
	{
		return;
	}

	// Render textures
	FGFxMovieRenderParams Params;


	for ( INT i = 0; i < TextureMovies.Num(); ++i )
	{
		FGFxMovie& movie = *TextureMovies ( i );
		if ( movie.pRenderTexture )
		{
			Params.Movies.AddItem ( movie );
		}
	}

#if XBOX
	// Prepass (certain systems only)
	Params.bRenderToSceneColor = FALSE;
	Params.bDoNextCapture = TRUE;
	DrawPrepass ( Params );
	Params.bDoNextCapture = FALSE;
#endif

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER (
		FGFxRenderUITextures,
		FGFxEngine*, pEngine, this,
		FGFxMovieRenderParams, Params, Params,
	{
		SCOPE_CYCLE_COUNTER ( STAT_GFxRenderTexturesRT );

		for ( INT i = 0; i < Params.Movies.Num(); ++i )
		{
			FGFxMovie& movie = Params.Movies ( i );

			if ( !Params.bDoNextCapture || movie.DispHandle.NextCapture ( pEngine->Renderer2D->GetContextNotify() ) )
			{
				if ( GEmitDrawEvents )
				{
					appBeginDrawEvent ( DEC_SCENE_ITEMS, * ( FString ( TEXT ( "Texture " ) ) + movie.pUMovie->GetName() ) );
				}

				FTextureRenderTargetResource* pRTResource = movie.pRenderTexture->GetRenderTargetResource();
				if ( pRTResource )
				{
					// ensure depth buffer is large enough
					GSceneRenderTargets.Allocate ( pRTResource->GetSizeX(), pRTResource->GetSizeY() );

					if ( !*movie.GFxRenderTarget )
					{
						*movie.GFxRenderTarget = pEngine->RenderHal->CreateRenderTarget ( pRTResource, true );
					}
                    Render::RHI::RenderTargetData* pRTData = (Render::RHI::RenderTargetData*)(*movie.GFxRenderTarget)->GetHALData();
                    Render::RHI::RenderTargetData::UpdateData(*movie.GFxRenderTarget, pRTResource, 0, 0, pRTData->DepthStencil );

					pEngine->RenderHal->SetRenderTarget ( *movie.GFxRenderTarget );
					RHISetViewport ( 0, 0, 0.0f, pRTResource->GetSizeX(), pRTResource->GetSizeY(), 1.0f );
					RHIClear ( TRUE, FLinearColor ( 0, 0, 0, 0 ), FALSE, 0.f, FALSE, 0 );
				}

				const Render::TreeRoot::NodeData* nodeData = movie.DispHandle.GetRenderEntry()->GetDisplayData();
				bool hasViewport = nodeData->HasViewport();
				if ( hasViewport )
				{
					const Render::Viewport &vp3d = movie.DispHandle.GetRenderEntry()->GetDisplayData()->VP;
					Render::Rect<int> viewRect ( vp3d.Left, vp3d.Top, vp3d.Left + vp3d.Width, vp3d.Top + vp3d.Height );
					pEngine->RenderHal->SetFullViewRect ( viewRect );
				}

				pEngine->Renderer2D->Display ( movie.DispHandle );

				if ( pRTResource )
				{
					RHICopyToResolveTarget ( pRTResource->GetRenderTargetSurface(), TRUE, FResolveParams() );
				}

				if ( GEmitDrawEvents )
				{
					appEndDrawEvent();
				}
			}
		}

		// main pass only for HUD
		pEngine->ReleaseOwnershipOfRenderTargets ( pEngine->RenderHal );
		pEngine->RenderHal->SetDisplayPass ( Display_Final );
	} );
}

GFx::MovieDef*    FGFxEngine::LoadMovieDef ( const TCHAR* ppath, GFx::MovieInfo& Info )
{
	check ( NULL != ppath );
	if ( !ppath )
	{
		return NULL;
	}

	FString moviePath;

	// Determine package
	if ( !IsFilesystemPath ( TCHAR_TO_ANSI ( ppath ) ) )
	{
#ifndef GFX_NO_LOCALIZATION
		if ( !FontlibInitialized )
		{
			USwfMovie* pmovieInfo = LoadObject<USwfMovie> ( NULL, ppath, NULL, LOAD_None, NULL );

			// Fontlib must be configured before attempting to load movies that use it.
			if ( pmovieInfo && pmovieInfo->bUsesFontlib )
			{
				InitFontlib();
			}
		}
#endif

		// mLoader uses / as path separator. PACKAGE_PATH indicates package loading.
		moviePath = FString ( PACKAGE_PATH_U ) + FString ( ppath );
		ReplaceCharsInFString ( moviePath, TEXT ( "." ), TEXT ( '/' ) );
		ppath = *moviePath;
	}

	unsigned loadConstants = GFx::Loader::LoadAll;

	// Get info about the width & height of the movie.
	if ( !mLoader.GetMovieInfo ( TCHAR_TO_ANSI ( ppath ), &Info ) )
	{
		debugf ( TEXT ( "FGFxEngine::LoadMovieDef Error - failed to get info about %s\n" ), ppath );
		return NULL;
	}

	return mLoader.CreateMovie ( TCHAR_TO_ANSI ( ppath ), loadConstants );
}

FGFxMovie*  FGFxEngine::LoadMovie ( const TCHAR* ppath, UBOOL fInitFirstFrame )
{
	FGFxMovie* pnewMovie = new FGFxMovie();
	pnewMovie->pUMovie  = 0;
	pnewMovie->fVisible = 0;
	pnewMovie->fUpdate  = 0;
	pnewMovie->Playing  = 0;
	pnewMovie->FileName = ppath;
	pnewMovie->pRenderTexture = 0;
	pnewMovie->GFxRenderTarget = 0;
	pnewMovie->fViewportSet = 0;
	pnewMovie->bCanReceiveFocus = true;
	pnewMovie->bCanReceiveInput = true;
	pnewMovie->TimingMode = TM_Game;
	pnewMovie->LastTime = 0.0;

	pnewMovie->pDef = *LoadMovieDef ( ppath, pnewMovie->Info );
	if ( !pnewMovie->pDef )
	{
		debugf ( TEXT ( "FGFxEngine::LoadMovie Error - failed to create a movie from '%s'.\n" ), ppath );
		delete pnewMovie;
		return NULL;
	}

#if (WITH_GFx_IME && defined(WIN32))
	if ( !pImeManager )
	{
		FString ImeMoviePath = Localize ( TEXT ( "IME" ), TEXT ( "MoviePath" ), TEXT ( "GFxUI" ) );
		if ( ImeMoviePath.Len() > 0 )
		{
			pImeManager = *SF_NEW GFx::IME::GFxIMEManagerWin32 ( ( HWND ) ( HudViewport ? HudViewport->GetWindow() : NULL ) );

			FString ImeMoviePath = Localize ( TEXT ( "IME" ), TEXT ( "MoviePath" ), TEXT ( "GFxUI" ) );
			ReplaceCharsInFString ( ImeMoviePath, TEXT ( "." ), TEXT ( '/' ) );
			pImeManager->Init ( mLoader.GetLog() );
			pImeManager->SetIMEMoviePath ( FTCHARToUTF8 ( * ( FString ( PACKAGE_PATH_U ) + ImeMoviePath ) ) );

			mLoader.SetIMEManager ( pImeManager );
		}
	}
#endif

	pnewMovie->pView = *pnewMovie->pDef->CreateInstance ( fInitFirstFrame ? 1 : 0 );
	if ( !pnewMovie->pView )
	{
		// Clean up!
		pnewMovie->pDef = 0;
		delete pnewMovie;

		debugf ( TEXT ( "FGFxEngine::LoadMovie Error - failed to create movie instance\n" ) );
		return NULL;
	}
	pnewMovie->DispHandle = pnewMovie->pView->GetDisplayHandle();
	return pnewMovie;
}

void  FGFxEngine::StartScene ( FGFxMovie* Movie, UTextureRenderTarget2D* RenderTexture, UBOOL fVisible, UBOOL fUpdate )
{
	Movie->Playing  = TRUE;
	Movie->fVisible = fVisible;
	Movie->fUpdate  = fUpdate;
	Movie->pRenderTexture = RenderTexture;

	if ( !RenderTexture )
	{
		// Background Alpha = 0 for transparent background,
		// and playback view spanning the entire window.
		const FLOAT Alpha = 0.0f;
		Movie->pView->SetBackgroundAlpha ( Alpha );

		// TODO: Shouldn't these be user configurable - on a per-movie-basis?

		SetMovieSize ( Movie );

#ifdef GAMEPAD_MOUSE
#if ((GFC_FX_MAJOR_VERSION >= 2 && GFC_FX_MINOR_VERSION >= 2) || GFC_FX_MAJOR_VERSION >= 3 || GFX_MAJOR_VERSION >= 4)
		Movie->pView->SetMouseCursorCount ( 1 );
#else
		Movie->pView->EnableMouseSupport ( 1 );
#endif
#endif

		InsertMovie ( Movie, SDPG_Foreground );
	}
	else
	{
		Scaleform::UInt32 ViewFlags = Render::Viewport::View_IsRenderTexture;
		if ( Movie->pUMovie )
		{
			switch ( Movie->pUMovie->RenderTextureMode )
			{
				case RTM_AlphaComposite:
					ViewFlags = Render::Viewport::View_AlphaComposite;
				case RTM_Alpha:
					Movie->pView->SetBackgroundAlpha ( 0 );
			}
			if ( !Movie->pUMovie->bEnableGammaCorrection )
			{
				ViewFlags |= Render::RHI::Viewport_NoGamma;
			}
		}

		Movie->GFxRenderTarget = new RenderTarget* ( NULL );
		Movie->pView->SetViewport ( ( INT ) RenderTexture->GetSurfaceWidth(), ( INT ) RenderTexture->GetSurfaceHeight(), 0, 0,
									( INT ) RenderTexture->GetSurfaceWidth(), ( INT ) RenderTexture->GetSurfaceHeight(), ViewFlags );

		INT i = TextureMovies.AddItem ( Movie );
	}

	// Store initial timing, so that we can determine
	// how much to advance Flash playback.
	Movie->LastTime = 0.0;

	//Let UE know that a player's paused state should be reevaluated
	if ( Movie->pUMovie->bPauseGameWhileActive )
	{
		UGameUISceneClient* SceneClient = UUIRoot::GetSceneClient();

		for ( INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
		{
			if ( PlayerIndex == Movie->pUMovie->LocalPlayerOwnerIndex )
			{
				SceneClient->UpdatePausedState ( PlayerIndex );
				break;
			}
		}
	}
}

/** From UGameUISceneClient::FlushPlayerInput
* Clears the arrays of pressed keys for all local players in the game; used when the UI begins processing input.  Also
* updates the InitialPressedKeys maps for all players.
*/
void FGFxEngine::FlushPlayerInput ( TSet<NAME_INDEX>* Keys )
{
	for ( INT ControllerId = 0; ControllerId < GEngine->GamePlayers.Num(); ControllerId++ )
	{
		ULocalPlayer* Player = GEngine->GamePlayers ( ControllerId );
		if ( Player != NULL && Player->Actor != NULL && Player->Actor->PlayerInput != NULL )
		{
			//@todo ronp - in some cases, we only want to do this for the player that opened the scene

			// record each key that was pressed when the UI began processing input so we can ignore the released event that will be generated when that key is released
			TArray<FName>* PressedPlayerKeys = InitialPressedKeys.Find ( Player->ControllerId );
			if ( PressedPlayerKeys == NULL )
			{
				PressedPlayerKeys = &InitialPressedKeys.Set ( Player->ControllerId, TArray<FName>() );
			}

			if ( PressedPlayerKeys != NULL )
			{
				for ( INT KeyIndex = 0; KeyIndex < Player->Actor->PlayerInput->PressedKeys.Num(); KeyIndex++ )
				{
					FName Key = Player->Actor->PlayerInput->PressedKeys ( KeyIndex );
					if ( Keys == NULL || Keys->Contains ( Key.GetIndex() ) )
					{
						PressedPlayerKeys->AddUniqueItem ( Key );
					}
				}
			}

			if ( Keys )
			{
				// from UPlayerInput::ResetInput
				TArray<FName> PressedKeyCopy = Player->Actor->PlayerInput->PressedKeys;
				for ( INT KeyIndex = 0; KeyIndex < PressedKeyCopy.Num(); KeyIndex++ )
				{
					FName Key = PressedKeyCopy ( KeyIndex );
					if ( Keys->Contains ( Key.GetIndex() ) )
					{
						// simulate a release event for this key so that the PlayerInput code can perform any cleanup required
						if ( Player->Actor->PlayerInput->__OnReceivedNativeInputKey__Delegate.IsCallable ( Player->Actor->PlayerInput ) )
						{
							Player->Actor->PlayerInput->delegateOnReceivedNativeInputKey ( Player->ControllerId, Key, IE_Released, 0 );
						}

						Player->Actor->PlayerInput->InputKey ( Player->ControllerId, Key, IE_Released, 0 );
					}
				}
			}
			else
			{
				Player->Actor->PlayerInput->ResetInput();
			}
		}
	}
}

FGFxMovie*  FGFxEngine::GetFocusMovie ( INT ControllerId )
{
	return GetFocusedMovieFromControllerID ( ControllerId );
}

UBOOL FGFxEngine::InputKey ( INT ControllerId, FGFxMovie* pFocusMovie, FName ukey, EInputEvent uevent )
{
	UBOOL CaptureInput = pFocusMovie->pUMovie->bCaptureInput;

	// first check to see if this is a key-release event we should ignore (see FlushPlayerInput)
	if ( InitialPressedKeys.Num() > 0 && ( uevent == IE_Repeat || uevent == IE_Released ) )
	{
		TArray<FName>* PressedPlayerKeys = InitialPressedKeys.Find ( ControllerId );
		if ( PressedPlayerKeys != NULL )
		{
			INT KeyIndex = PressedPlayerKeys->FindItemIndex ( ukey );
			if ( KeyIndex != INDEX_NONE )
			{
				// this key was found in the InitialPressedKeys array for this player, which means that the key was already
				// pressed when the UI began processing input - ignore this key event
				if ( uevent == IE_Released )
				{
					// if the player has released the key, remove the key from the list of keys to ignore
					PressedPlayerKeys->Remove ( KeyIndex );
				}

				// and swallow this input event
				return TRUE;
			}
		}
	}


	UGFxMoviePlayer* FocusedMoviePlayer = pFocusMovie->pUMovie;
	if ( FocusedMoviePlayer && ( FocusedMoviePlayer->IsPendingKill() || FocusedMoviePlayer->HasAnyFlags ( RF_Unreachable ) ) )
	{
		// If this movie is unavailable because it's being cleaned up, bail now.
		return TRUE;
	}

	//If the movie discards non-owner input and this input does not come from the owner, discard it.
	if ( FocusedMoviePlayer->bDiscardNonOwnerInput && GetLocalPlayerIndexFromControllerID ( ControllerId ) != FocusedMoviePlayer->LocalPlayerOwnerIndex )
	{
		return TRUE;
	}

	// give unrealscript a chance to look at the input, and see if it should be filtered out.
	if ( FocusedMoviePlayer->eventFilterButtonInput ( ControllerId, ukey, uevent ) )
	{
		return TRUE;
	}

	UGFxInput* key = KeyMap.Find ( ukey.GetIndex() );
	if ( key )
	{
		GFx::Event::EventType etype;
		if ( key->Key != Key::None )
		{
			bool bIsKeyHold = false;
			switch ( uevent )
			{
				case IE_Pressed:
					etype = GFx::Event::KeyDown;
					break;
				case IE_Repeat:
					etype = GFx::Event::KeyDown;
					bIsKeyHold = true;
					break;
				case IE_Released:
					etype = GFx::Event::KeyUp;
					break;
				default:
					return 0;
			}

			if ( pFocusMovie->bCanReceiveInput )
			{
				//update the keyboard input as to whether this is a keyhold or key press (irrelevant for key up)
				FocusedMoviePlayer->SetVariableBool ( TEXT ( "_global.bIsKeyHold" ), bIsKeyHold );

				GFx::KeyEvent keyevent ( etype, key->Key, 0, 0, ControllerId );
				pFocusMovie->pView->HandleEvent ( keyevent );

				if ( FocusedMoviePlayer && FocusedMoviePlayer->pCaptureKeys &&
						FocusedMoviePlayer->pCaptureKeys->Contains ( ukey.GetIndex() ) )
				{
					return 1;
				}
			}
		}
		else if ( key->MouseButton < 3 )
		{
			CaptureInput = CaptureInput || pFocusMovie->pUMovie->bCaptureMouseInput;
			switch ( uevent )
			{
				case IE_Pressed:
					etype = GFx::Event::MouseDown;
					break;
				case IE_Released:
					etype = GFx::Event::MouseUp;
					break;
				default:
					return 0;
			}

			GFx::MouseEvent mevent ( etype, key->MouseButton, MousePos.X, MousePos.Y, 0.0f );

			if ( CaptureInput ||
					(FocusedMoviePlayer && FocusedMoviePlayer->pCaptureKeys &&
					FocusedMoviePlayer->pCaptureKeys->Contains ( ukey.GetIndex() ) ) )
			{
				pFocusMovie->pView->HandleEvent ( mevent );
				return 1;
			}
			else
			{
				for ( INT i = 0; i < OpenMovies.Num(); ++i )
				{
					FGFxMovie& movie = *OpenMovies ( i );
					if ( movie.fVisible && movie.bCanReceiveInput )
					{
						movie.pView->HandleEvent ( mevent );
					}
				}
			}
		}
		else if ( uevent == IE_Pressed || uevent == IE_Repeat )
		{
			GFx::MouseEvent mevent ( GFx::Event::MouseWheel, 0, MousePos.X, MousePos.Y, key->MouseButton * 6 - 21 );
			if ( CaptureInput ||
				(FocusedMoviePlayer && FocusedMoviePlayer->pCaptureKeys &&
				FocusedMoviePlayer->pCaptureKeys->Contains ( ukey.GetIndex() ) ) )
			{
				pFocusMovie->pView->HandleEvent ( mevent );
				return 1;
			}
			else
			{
				for ( INT i = 0; i < OpenMovies.Num(); ++i )
				{
					FGFxMovie& movie = *OpenMovies ( i );
					if ( movie.fVisible && movie.bCanReceiveInput )
					{
						movie.pView->HandleEvent ( mevent );
					}
				}
			}
		}
	}
	return CaptureInput;
}

UBOOL FGFxEngine::InputKey ( INT ControllerId, FName ukey, EInputEvent uevent )
{
	//Find the focused movie for this controllerId
	FGFxMovie* pFocusMovie = GetFocusedMovieFromControllerID ( ControllerId );

	if ( pFocusMovie )
	{
		UBOOL result;
		if ( ( pFocusMovie->pUMovie->pFocusIgnoreKeys && pFocusMovie->pUMovie->pFocusIgnoreKeys->Contains ( ukey.GetIndex() ) ) ||
				!pFocusMovie->bCanReceiveInput )
		{
			result = 0;
		}
		else
		{
			result = InputKey ( ControllerId, pFocusMovie, ukey, uevent );
		}
		if ( result )
		{
			return 1;
		}
	}
	for ( INT i = 0; i < TextureMovies.Num(); ++i )
	{
		FGFxMovie& movie = *TextureMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput && movie.pUMovie && movie.pUMovie->pCaptureKeys &&
				movie.pUMovie->pCaptureKeys->Contains ( ukey.GetIndex() ) )
		{
			InputKey ( ControllerId, &movie, ukey, uevent );
			return 1;
		}
	}
	return 0;
}

inline UBOOL FGFxEngine::IsKeyCaptured ( FName ukey )
{
	for ( INT i = 0; i < OpenMovies.Num(); ++i )
	{
		FGFxMovie& movie = *OpenMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput && movie.pUMovie && movie.pUMovie->pCaptureKeys &&
				movie.pUMovie->pCaptureKeys->Contains ( ukey.GetIndex() ) )
		{
			return TRUE;
		}
	}
	for ( INT i = 0; i < TextureMovies.Num(); ++i )
	{
		FGFxMovie& movie = *TextureMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput && movie.pUMovie && movie.pUMovie->pCaptureKeys &&
				movie.pUMovie->pCaptureKeys->Contains ( ukey.GetIndex() ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

UBOOL FGFxEngine::InputChar ( INT ControllerId, TCHAR ch )
{
	TCHAR str[2];
	str[0] = ch;
	str[1] = 0;
	FName ukey ( str );

	//Find the focused movie for this controllerId
	FGFxMovie* pFocusMovie = GetFocusedMovieFromControllerID ( ControllerId );
	//If we can't find a focused movie, we can't handle it.
	if ( !pFocusMovie )
	{
		return FALSE;
	}
	UBOOL CaptureInput = PlayerStates ( GetLocalPlayerIndexFromControllerID ( ControllerId ) ).FocusedMovie->pUMovie->bCaptureInput;

	if ( pFocusMovie && pFocusMovie->bCanReceiveInput )
	{
		if ( pFocusMovie->pUMovie->pFocusIgnoreKeys == NULL ||
				!pFocusMovie->pUMovie->pFocusIgnoreKeys->Contains ( ukey.GetIndex() ) )
		{
			GFx::CharEvent event ( ch );
			pFocusMovie->pView->HandleEvent ( event );
			if ( CaptureInput )
			{
				return TRUE;
			}
		}
	}

	// pass captured keys
	for ( INT i = 0; i < OpenMovies.Num(); ++i )
	{
		FGFxMovie& movie = *OpenMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput && movie.pUMovie && movie.pUMovie->pCaptureKeys &&
				movie.pUMovie->pCaptureKeys->Contains ( ukey.GetIndex() ) )
		{
			GFx::CharEvent event ( ch );
			movie.pView->HandleEvent ( event );
			return 1;
		}
	}
	for ( INT i = 0; i < TextureMovies.Num(); ++i )
	{
		FGFxMovie& movie = *TextureMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput && movie.pUMovie && movie.pUMovie->pCaptureKeys &&
				movie.pUMovie->pCaptureKeys->Contains ( ukey.GetIndex() ) )
		{
			GFx::CharEvent event ( ch );
			movie.pView->HandleEvent ( event );
			return 1;
		}
	}

	return 0;
}

UBOOL FGFxEngine::InputAxis ( INT ControllerId, FName key, float delta, float DeltaTime, UBOOL bGamepad )
{
	//Find the focused movie for this controllerId
	FGFxMovie* pFocusMovie = GetFocusedMovieFromControllerID ( ControllerId );
	//If we can't find a focused movie, we can't handle it.
	if ( !pFocusMovie )
	{
		return FALSE;
	}
	UBOOL CaptureInput = pFocusMovie->pUMovie->bCaptureInput || pFocusMovie->pUMovie->bCaptureMouseInput || IsKeyCaptured ( key );
	UBOOL bIgnoreMouse = pFocusMovie->pUMovie->bIgnoreMouseInput;

	if ( !Renderer2D || !pFocusMovie || bIgnoreMouse )
	{
		return 0;
	}

	if ( bGamepad && pFocusMovie->bCanReceiveInput )
	{
#ifdef GAMEPAD_MOUSE
		INT idelta = INT ( delta * 10.0f );
		if ( idelta == 0 )
		{
			return CaptureInput;
		}
		else if ( idelta > 0 )
		{
			idelta--;
		}
		else
		{
			idelta++;
		}

		if ( key == AxisMouseEmulation[0] )
		{
			MousePos.X += AxisMouseInvert[0] ? -idelta : idelta;
		}
		else if ( key == AxisMouseEmulation[1] )
		{
			MousePos.Y += AxisMouseInvert[1] ? -idelta : idelta;
		}
		else
#endif
		{
			// from UnInteraction.cpp
			UBOOL bResult = FALSE;
			FUIAxisEmulationDefinition* EmulationDef = AxisEmulationDefinitions.Find ( key );

			// If this axis input event was generated by an axis we can emulate, check that it is outside the UI's dead zone.
			if ( EmulationDef != NULL && EmulationDef->bEmulateButtonPress == TRUE )
			{
				if ( ControllerId >= 0 && ControllerId < ARRAY_COUNT ( AxisInputEmulation ) && AxisInputEmulation[ControllerId].bEnabled == TRUE )
				{
					FInputEventParameters EmulatedEventParms ( ControllerId, ControllerId, EmulationDef->InputKeyToEmulate[delta > 0 ? 0 : 1], IE_MAX, 0, 0, 0 );

					UBOOL bValidDelta = Abs<FLOAT> ( delta ) >= UIJoystickDeadZone;

					// if the current delta is within the dead-zone, and this key is set as the CurrentRepeatKey for that gamepad,
					// generate a "release" event
					if ( bValidDelta == FALSE )
					{
						// if this key was the key that was being held down, simulate the release event
						// Only signal a release if this is the last key pressed
						if ( AxisInputEmulation[ControllerId].CurrentRepeatKey == key )
						{
							// change the event type to "release"
							EmulatedEventParms.EventType = IE_Released;
							EmulatedEventParms.InputKeyName = AxisInputEmulationLastKey[ControllerId];

							// and clear the emulated repeat key for this player
							AxisInputEmulation[ControllerId].CurrentRepeatKey = NAME_None;
						}
						else
						{
							bResult = CaptureInput || IsKeyCaptured ( EmulationDef->InputKeyToEmulate[0] ) ||
										IsKeyCaptured ( EmulationDef->InputKeyToEmulate[1] );
						}
					}

					// we have a valid delta for this axis; need to determine what to do with it
					else
					{
						// if this is the same key as the current repeat key, it means the user is still holding the joystick in the same direction
						// so we'll need to determine whether enough time has passed to generate another button press event
						if ( AxisInputEmulation[ControllerId].CurrentRepeatKey == key )
						{
							// we might need to simulate another "repeat" event
							EmulatedEventParms.EventType = IE_Repeat;
						}

						else
						{
							// if the new key isn't the same as the current repeat key, but the new key is another axis input, ignore it
							// this basically means that as long as we have a valid delta on one joystick axis, we're going to ignore all other joysticks for that player
							if ( AxisInputEmulation[ControllerId].CurrentRepeatKey != NAME_None && key != EmulationDef->AdjacentAxisInputKey )
							{
								bResult = CaptureInput || IsKeyCaptured ( EmulationDef->InputKeyToEmulate[0] ) ||
											IsKeyCaptured ( EmulationDef->InputKeyToEmulate[1] );
							}
							else
							{
								EmulatedEventParms.EventType = IE_Pressed;
								AxisInputEmulation[ControllerId].CurrentRepeatKey = key;
							}
						}
					}

					DOUBLE CurrentTimeInSeconds = appSeconds();
					if ( EmulatedEventParms.EventType == IE_Repeat )
					{
						// this key hasn't been held long enough to generate the "repeat" keypress, so just swallow the input event
						if ( CurrentTimeInSeconds < AxisInputEmulation[ControllerId].NextRepeatTime )
						{
							EmulatedEventParms.EventType = IE_MAX;
							bResult = CaptureInput || IsKeyCaptured ( EmulationDef->InputKeyToEmulate[0] ) ||
										IsKeyCaptured ( EmulationDef->InputKeyToEmulate[1] );
						}
						else
						{
							// it's time to generate another key press; subsequence repeats should take a little less time than the initial one
							AxisInputEmulation[ControllerId].NextRepeatTime = CurrentTimeInSeconds + AxisRepeatDelay * 0.5f;
						}
					}
					else if ( EmulatedEventParms.EventType == IE_Pressed )
					{
						// this is the first time we're sending a keypress event for this axis's emulated button, so set the initial repeat delay
						AxisInputEmulation[ControllerId].NextRepeatTime = CurrentTimeInSeconds + AxisRepeatDelay * 1.5f;
					}

					// we're supposed to generate the emulated button input key event if the EmulatedEventParms.EventType is not IE_MAX
					if ( EmulatedEventParms.EventType != IE_MAX )
					{
						bResult = InputKey ( ControllerId, EmulatedEventParms.InputKeyName, ( EInputEvent ) EmulatedEventParms.EventType );
						AxisInputEmulationLastKey[ControllerId] = EmulatedEventParms.InputKeyName;
					}
				}
			}
			return bResult;
		}
	}
	else if ( HudViewport )
	{
		HudViewport->GetMousePos ( MousePos );
	}
	else
	{
		return CaptureInput;
	}

	GFx::MouseEvent mevent ( GFx::Event::MouseMove, 0, MousePos.X, MousePos.Y, 0 );

	//If the input is to be captured, only let the focused movie look at it.
	if ( CaptureInput )
	{
		pFocusMovie->pView->HandleEvent ( mevent );
		return 1;
	}

	for ( INT i = 0; i < OpenMovies.Num(); ++i )
	{
		FGFxMovie& movie = *OpenMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput )
		{
			movie.pView->HandleEvent ( mevent );
		}
	}
	for ( INT i = 0; i < TextureMovies.Num(); ++i )
	{
		FGFxMovie& movie = *TextureMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput )
		{
			movie.pView->HandleEvent ( mevent );
		}
	}
	return CaptureInput;
}

UBOOL FGFxEngine::InputTouch ( INT ControllerId, const FIntPoint& Location, ETouchType Event, UINT TouchId )
{
	//Find the focused movie for this controllerId
	FGFxMovie* pFocusMovie = GetFocusedMovieFromControllerID ( ControllerId );
	//If we can't find a focused movie, we can't handle it.
	if ( !pFocusMovie )
	{
		return FALSE;
	}

	UBOOL bIgnoreMouse = pFocusMovie->pUMovie->bIgnoreMouseInput;

	if ( !Renderer2D || !pFocusMovie || bIgnoreMouse )
	{
		return FALSE;
	}

	// position the mouse
	MousePos = Location;

	GFx::MouseEvent mevent ( GFx::Event::MouseMove, 0, MousePos.X, MousePos.Y, 0 );
	for ( INT i = 0; i < OpenMovies.Num(); ++i )
	{
		FGFxMovie& movie = *OpenMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput )
		{
			movie.pView->HandleEvent ( mevent );
		}
	}
	for ( INT i = 0; i < TextureMovies.Num(); ++i )
	{
		FGFxMovie& movie = *TextureMovies ( i );
		if ( movie.fVisible && movie.bCanReceiveInput )
		{
			movie.pView->HandleEvent ( mevent );
		}
	}

	// Touch events
	GFx::Event::EventType TEventType;
	switch (Event)
	{
	case Touch_Began:
		TEventType = GFx::Event::TouchBegin;
		break;
	case Touch_Ended:
		TEventType = GFx::Event::TouchEnd;
		break;
	default:
		TEventType = GFx::Event::TouchMove;
		break;
	}
	GFx::TouchEvent TEvent(TEventType, TouchId, Location.X, Location.Y, 0, 0, /*Primary*/ false);
	pFocusMovie->pView->HandleEvent(TEvent);

	// send click
	EInputEvent KeyEvent = IE_Repeat;
	if (Event == Touch_Began)
	{
		KeyEvent = IE_Pressed;
	}
	else if (Event == Touch_Ended || Event == Touch_Cancelled)
	{
		KeyEvent = IE_Released;
	}

	return InputKey(ControllerId, KEY_LeftMouseButton, KeyEvent);
}

void FGFxEngine::AddPlayerState()
{
	PlayerStates.Add();
	PlayerStates ( PlayerStates.Num() - 1 ).FocusedMovie = NULL;
	ReevaluateFocus();
	ReevaluateSizes();
}

void FGFxEngine::RemovePlayerState ( INT Index )
{
	PlayerStates.Remove ( Index, 1 );
	//Close all movies associated with this player.
	for ( INT i = OpenMovies.Num() - 1; i >= 0; i-- )
	{
		FGFxMovie& movie = *OpenMovies ( i );
		if ( movie.pUMovie != NULL && movie.pUMovie->LocalPlayerOwnerIndex == Index )
		{
			movie.pUMovie->Close();
		}
	}
	ReevaluateSizes();
}

void FGFxEngine::ReevaluateSizes()
{
	for ( INT i = 0; i < OpenMovies.Num(); i++ )
	{
		SetMovieSize ( OpenMovies ( i ) );
	}
}

void FGFxEngine::SetMovieSize ( FGFxMovie* Movie )
{
	//Defaults
	ULocalPlayer* LP = NULL;
	UINT BaseSizeX, SizeX;
	UINT BaseSizeY, SizeY;
	BaseSizeX = SizeX = 1280;
	BaseSizeY = SizeY = 720;
	UINT X = 0;
	UINT Y = 0;

	//Find the real values from the viewport
	if ( HudViewport != NULL )
	{
		BaseSizeX = SizeX = HudViewport->GetSizeX();
		BaseSizeY = SizeY = HudViewport->GetSizeY();
	}

	//Only use the player specific sizing if you can find the player and it is a player-only focusable movie
	if ( Movie->pUMovie &&
			Movie->pUMovie->LocalPlayerOwnerIndex < GEngine->GamePlayers.Num() &&
			Movie->pUMovie->bOnlyOwnerFocusable )
	{
		LP = GEngine->GamePlayers ( Movie->pUMovie->LocalPlayerOwnerIndex );
	}
	if ( LP != NULL )
	{
		X = ( UINT ) ( BaseSizeX * LP->Origin.X );
		Y = ( UINT ) ( BaseSizeY * LP->Origin.Y );
		SizeX = ( UINT ) ( BaseSizeX * LP->Size.X );
		SizeY = ( UINT ) ( BaseSizeY * LP->Size.Y );
	}
	//HACK: handle the fact that the viewport can get set up after the rest of the game in PIE.

	Movie->pView->SetViewport ( BaseSizeX, BaseSizeY, X, Y, SizeX, SizeY,
								(Movie->pUMovie && Movie->pUMovie->bEnableGammaCorrection == 0 ? Render::RHI::Viewport_NoGamma : 0)
#if WITH_REALD
								| (RealD::IsStereoEnabled() ? Render::Viewport::View_Stereo_SplitH : 0)
#endif
								);
}

void FGFxEngine::ReevaluateFocus()
{
	//Early out. If there are no playerstates, there's no focus!
	if ( PlayerStates.Num() <= 0 )
	{
		return;
	}

	//Whether or not the first player had a focusable movie before the reevaluation
	UBOOL bHadFocus = PlayerStates ( 0 ).FocusedMovie != NULL;
	TArray<class FGFxMovie*> OldPlayerStates;

	//Record last focused movie for all players and null out the focused movie references
	for ( INT j = 0; j < PlayerStates.Num(); j++ )
	{
		OldPlayerStates.AddItem ( PlayerStates ( j ).FocusedMovie );
		PlayerStates ( j ).FocusedMovie = NULL;
	}

	//Go backwards, as the highest priorities are on the end
	for ( INT i = OpenMovies.Num() - 1; i >= 0; i-- )
	{
		FGFxMovie* CurrMovie = OpenMovies ( i );
		if ( CurrMovie->pUMovie && CurrMovie->bCanReceiveFocus )
		{
			//If it is not only owner focusable, then we have to iterate over all players to see if this movie has their focus
			if ( !CurrMovie->pUMovie->bOnlyOwnerFocusable )
			{
				for ( INT j = 0; j < PlayerStates.Num(); j++ )
				{
					if ( PlayerStates ( j ).FocusedMovie == NULL ||
							( PlayerStates ( j ).FocusedMovie->pUMovie &&
								CurrMovie->pUMovie &&
								PlayerStates ( j ).FocusedMovie->pUMovie->Priority < CurrMovie->pUMovie->Priority ) )
					{
						PlayerStates ( j ).FocusedMovie = CurrMovie;
					}
				}
				//Early-out - Once we've hit a generally focusable movie, we can guarantee that every player should have a movie
				break;
			}
			//Otherwise we only need to check the owner of said movie
			else if ( PlayerStates ( CurrMovie->pUMovie->LocalPlayerOwnerIndex ).FocusedMovie == NULL ||
						PlayerStates ( CurrMovie->pUMovie->LocalPlayerOwnerIndex ).FocusedMovie->pUMovie->Priority < CurrMovie->pUMovie->Priority )
			{
				PlayerStates ( CurrMovie->pUMovie->LocalPlayerOwnerIndex ).FocusedMovie = CurrMovie;
			}
		}
	}

	//IME Hack - Active IME movie is always player 0's focused movie
#if WITH_GFx_IME
	if ( pImeManager && PlayerStates ( 0 ).FocusedMovie )
	{
		pImeManager->SetActiveMovie ( PlayerStates ( 0 ).FocusedMovie->pView );
	}
#endif

	//Update mouse cursor visibility
	for (INT i = 0; i < PlayerStates.Num(); i++)
	{
		if (PlayerStates(i).FocusedMovie != NULL && PlayerStates(i).FocusedMovie->pUMovie != NULL && 
			PlayerStates(i).FocusedMovie->pUMovie->bShowHardwareMouseCursor)
		{
			GEngine->GamePlayers(i)->ViewportClient->eventSetHardwareMouseCursorVisibility(TRUE);
		}
		else
		{
			GEngine->GamePlayers(i)->ViewportClient->eventSetHardwareMouseCursorVisibility(FALSE);
		}
	}

	//If player 1 had a focused movie but no longer has one, then we need to move the mouse
	if ( bHadFocus && PlayerStates.Num() > 0 && PlayerStates ( 0 ).FocusedMovie == NULL )
	{
		// move mouse cursor offscreen when GFx input is disabled
		GFx::MouseEvent mevent ( GFx::Event::MouseMove, 0, -256, -256, 0 );
		for ( INT i = 0; i < OpenMovies.Num(); ++i )
		{
			FGFxMovie& movie = *OpenMovies ( i );
			if ( movie.fVisible && movie.bCanReceiveFocus )
			{
				movie.pView->HandleEvent ( mevent );
			}
		}
	}

	//Send Focus Changed Events
	for ( INT CurMovieIndex = 0; CurMovieIndex < PlayerStates.Num(); ++CurMovieIndex )
	{
		if ( OldPlayerStates ( CurMovieIndex ) != PlayerStates ( CurMovieIndex ).FocusedMovie )
		{
			if ( OldPlayerStates ( CurMovieIndex ) && OldPlayerStates ( CurMovieIndex )->pUMovie )
			{
				const UBOOL bMovieBeingDeleted = OldPlayerStates ( CurMovieIndex )->pUMovie->HasAnyFlags ( RF_PendingKill | RF_Unreachable );
				if ( !bMovieBeingDeleted )
				{
					OldPlayerStates ( CurMovieIndex )->pUMovie->eventOnFocusLost ( CurMovieIndex );
				}
			}

			if ( PlayerStates ( CurMovieIndex ).FocusedMovie && PlayerStates ( CurMovieIndex ).FocusedMovie->pUMovie )
			{
				const UBOOL bMovieBeingDeleted = PlayerStates ( CurMovieIndex ).FocusedMovie->pUMovie->HasAnyFlags ( RF_PendingKill | RF_Unreachable );
				if ( !bMovieBeingDeleted )
				{
					PlayerStates ( CurMovieIndex ).FocusedMovie->pUMovie->eventOnFocusGained ( CurMovieIndex );
				}
			}
		}
	}
	OldPlayerStates.Empty();

	// Default to both being off
	INT BlurBelowPriority = -1;
	INT HideBelowPriority = -1;

	TArray<INT> PlayerBlurBelowPriority;
	TArray<INT> PlayerHideBelowPriority;

	for ( INT i = 0; i < PlayerStates.Num(); i++ )
	{
		PlayerBlurBelowPriority.AddItem ( -1 );
		PlayerHideBelowPriority.AddItem ( -1 );
	}

#if DWTRIOVIZSDK
	UBOOL bFSMovieIsBlurred = FALSE;
	AWorldInfo* WorldInfo = GWorld ? GWorld->GetWorldInfo() : NULL;
	UBOOL bIsMainMenu = WorldInfo ? WorldInfo->IsMenuLevel() : TRUE;
#endif//DWTRIOVIZSDK

	// Iterate over all of the movies and figure out if they should be hidden/blurred
	for ( INT i = OpenMovies.Num() - 1; i >= 0; i-- )
	{
		// Apply the current settings to this scene...

		UGFxMoviePlayer* Movie = OpenMovies ( i )->pUMovie;
		if ( Movie && !Movie->IsPendingKill() && !Movie->HasAnyFlags ( RF_Unreachable ) )
		{
			UBOOL bNeedsBlur = ( ( Movie->Priority <= BlurBelowPriority ) || Movie->Priority <= PlayerBlurBelowPriority ( Movie->LocalPlayerOwnerIndex ) );
			UBOOL bNeedsHide = ( ( Movie->Priority <= HideBelowPriority ) || Movie->Priority <= PlayerHideBelowPriority ( Movie->LocalPlayerOwnerIndex ) );

#if DWTRIOVIZSDK
			// This implies that the movie is fullviewport, is getting blurred
			if ( bNeedsBlur && !bNeedsHide && ( !Movie->bOnlyOwnerFocusable || Movie->bForceFullViewport ) )
			{
				bFSMovieIsBlurred = TRUE;
			}
#endif//DWTRIOVIZSDK


			// Apply the settings
			OpenMovies ( i )->pUMovie->eventApplyPriorityEffect ( bNeedsBlur, bNeedsHide );

			if ( Movie->bBlurLesserMovies && Movie->Priority > BlurBelowPriority && !Movie->bOnlyOwnerFocusable )
			{
				BlurBelowPriority = Movie->Priority;
			}

			if ( Movie->bBlurLesserMovies && Movie->Priority > PlayerBlurBelowPriority ( Movie->LocalPlayerOwnerIndex ) )
			{
				PlayerBlurBelowPriority ( Movie->LocalPlayerOwnerIndex ) = Movie->Priority;
			}

			if ( Movie->bHideLesserMovies && Movie->Priority > HideBelowPriority && !Movie->bOnlyOwnerFocusable )
			{
				HideBelowPriority = Movie->Priority;
			}

			if ( Movie->bHideLesserMovies && Movie->Priority > PlayerHideBelowPriority ( Movie->LocalPlayerOwnerIndex ) )
			{
				PlayerHideBelowPriority ( Movie->LocalPlayerOwnerIndex ) = Movie->Priority;
			}
		}
	}

#if DWTRIOVIZSDK
	if ( bIsMainMenu == TRUE )
	{
		DwTriovizImpl_Set3dMovieHackery ( bFSMovieIsBlurred );
	}
#endif//DWTRIOVIZSDK

	//FlushPlayerInput();
}


//Helper function to translate ControllerId's to LocalPlayer Indexes
INT FGFxEngine::GetLocalPlayerIndexFromControllerID ( INT ControllerId )
{
	//Find the corresponding local player index for this controllerId
	INT LocalPlayerIndex = 0;
	for ( INT i = 0; i < GEngine->GamePlayers.Num(); i++ )
	{
		if ( GEngine->GamePlayers ( i )->ControllerId == ControllerId )
		{
			LocalPlayerIndex = i;
			break;
		}
	}

	return LocalPlayerIndex;
}

//Helper function to translate ControllerId's to LocalPlayer Indexes
FGFxMovie* FGFxEngine::GetFocusedMovieFromControllerID ( INT ControllerId )
{
	INT LocalPlayerIndex = GetLocalPlayerIndexFromControllerID ( ControllerId );

	if ( LocalPlayerIndex < PlayerStates.Num() &&
			LocalPlayerIndex >= 0 )
	{
		return PlayerStates ( LocalPlayerIndex ).FocusedMovie;
	}
	return NULL;
}

static const ESceneRenderTargetTypes AcquiredTargetTextures[] =
{
	LightAttenuation0,
	LightAttenuation1,
	TranslucencyBuffer,
	HalfResPostProcess,
	TranslucencyDominantLightAttenuation,
	AOInput,
	AOOutput,
	AOHistory,
	MAX_SCENE_RENDERTARGETS
};

void FGFxEngine::AssumeOwnershipOfRenderTargets ( Render::RHI::HAL* RenderHAL )
{
#if SF_RENDERBUFFERMEMORY_ENABLE
	Render::RHI::RenderBufferManagerMemory* pRenderBufferManager =
		( Render::RHI::RenderBufferManagerMemory* ) RenderHal->GetRenderBufferManager();
	if ( !pRenderBufferManager )
	{
		return;
	}

	INT TargetTexture = 0;
	while ( AcquiredTargetTextures[TargetTexture] != MAX_SCENE_RENDERTARGETS )
	{
		const FTexture2DRHIRef& texture = GSceneRenderTargets.GetRenderTargetTexture ( AcquiredTargetTextures[TargetTexture] );
		pRenderBufferManager->AddResolveMemory ( texture );
		const FSurfaceRHIRef& surface = GSceneRenderTargets.GetRenderTargetSurface ( AcquiredTargetTextures[TargetTexture] );

		// Surface memory is currently hardcoded in the buffer manager.
		//pRenderBufferManager->AddSurfaceMemory(surface);
		++TargetTexture;
	}
	pRenderBufferManager->AcquireVideoMemory();
#endif
}

void FGFxEngine::ReleaseOwnershipOfRenderTargets ( Render::RHI::HAL* RenderHAL )
{
#if SF_RENDERBUFFERMEMORY_ENABLE
	Render::RHI::RenderBufferManagerMemory* pRenderBufferManager =
		( Render::RHI::RenderBufferManagerMemory* ) RenderHal->GetRenderBufferManager();
	if ( !pRenderBufferManager )
	{
		return;
	}
	pRenderBufferManager->ReleaseVideoMemory();
#endif
}

void FGFxEngine::DrawPrepass ( const FGFxMovieRenderParams& PrepassParams )
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER (
		FGFxRenderUIPrepass,
		FGFxEngine*, pEngine, this,
		FGFxMovieRenderParams, Params, PrepassParams,
	{
		SCOPE_CYCLE_COUNTER ( STAT_GFxRenderUIRT );
		if ( GEmitDrawEvents )
		{
			appBeginDrawEvent ( DEC_SCENE_ITEMS, TEXT ( "RenderScaleformPrepass" ) );
		}

		pEngine->AssumeOwnershipOfRenderTargets ( pEngine->RenderHal );
		pEngine->RenderHal->SetDisplayPass ( Display_Prepass );
		pEngine->RenderHal->BeginScene();

		for ( INT i = 0; i < Params.Movies.Num(); ++i )
		{
			FGFxMovie& movie = Params.Movies ( i );
			if ( !Params.bDoNextCapture || movie.DispHandle.NextCapture ( pEngine->Renderer2D->GetContextNotify() ) )
			{
				const Render::TreeRoot::NodeData* nodeData = movie.DispHandle.GetRenderEntry()->GetDisplayData();
				bool hasViewport = nodeData->HasViewport();
				if ( hasViewport )
				{
					const Render::Viewport &vp3d = movie.DispHandle.GetRenderEntry()->GetDisplayData()->VP;
					Render::Rect<int> viewRect ( vp3d.Left, vp3d.Top, vp3d.Left + vp3d.Width, vp3d.Top + vp3d.Height );
					pEngine->RenderHal->SetFullViewRect ( viewRect );
				}

				pEngine->Renderer2D->Display ( movie.DispHandle );
			}
		}

		pEngine->RenderHal->EndScene();

		if ( GEmitDrawEvents )
		{
			appEndDrawEvent();
		}

		// render both passes for textures
		pEngine->RenderHal->SetDisplayPass ( Display_Final );
	} );
}

#else // WITH_GFx = 0


IMPLEMENT_CLASS ( UGFxEngine );


UGFxEngine::UGFxEngine()
{
}

void UGFxEngine::FinishDestroy()
{
	Super::FinishDestroy();
}

void UGFxEngine::Release()
{
}

#endif // WITH_GFx
