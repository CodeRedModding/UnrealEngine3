/**********************************************************************

Filename    :   ScaleformMovie.cpp
Content     :   UGFxMoviePlayer class implementation for GFx

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

#include "ScaleformEngine.h"
#include "ScaleformAllocator.h"

#include "EngineSequenceClasses.h"
#include "GFxUIUISequenceClasses.h"
#include "EngineUIPrivateClasses.h"
#include "GFxUIUIPrivateClasses.h"
#include "GFxUIClasses.h"

#include "TranslatorTags.h"

#if WITH_GFx

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif
#include "Kernel/SF_Memory.h"
#include "GFx/GFx_Player.h"
#include "GFx/GFx_ImageResource.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

IMPLEMENT_CLASS ( UGFxMoviePlayer );
IMPLEMENT_CLASS ( UGFxObject );
IMPLEMENT_CLASS ( UGFxFSCmdHandler );
IMPLEMENT_CLASS ( UGFxRawData );
IMPLEMENT_CLASS ( USwfMovie );
IMPLEMENT_CLASS ( UFlashMovie );

using namespace GFx;
using namespace Render;

void UGFxRawData::SetRawData ( const BYTE *data, UINT size )
{
	RawData.Empty();
	RawData.Add ( size );
	appMemcpy ( &RawData ( 0 ), data, RawData.Num() );
}

/** Cleans up the allocated resources */
void UGFxMoviePlayer::Cleanup()
{
	if ( GGFxEngine && pMovie )
	{
		Close();
	}
}

/** Overwrite the base class definition to do custom cleanup **/
void UGFxMoviePlayer::FinishDestroy()
{
	Cleanup();
	Super::FinishDestroy();
}

UGFxMoviePlayer::UGFxMoviePlayer()
{
	pMovie = 0;
}

void UGFxMoviePlayer::Serialize ( FArchive& Ar )
{
	Super::Serialize ( Ar );
}

UBOOL UGFxMoviePlayer::Load ( const FString& filename, UBOOL InitFirstFrame )
{
	if ( NULL == FGFxEngine::GetEngine() )
	{
		return 0;
	}
	else if ( pMovie )
	{
		Close ( TRUE );
	}

	pMovie = GGFxEngine->LoadMovie ( *filename, InitFirstFrame );
	if ( pMovie )
	{
		//set receive input flags of movie based on player settings
		SetMovieCanReceiveInput ( bAllowInput );

		//set receive focus flags of movie based on player settings
		SetMovieCanReceiveFocus ( bAllowFocus );

		check ( pMovie->pUMovie == 0 );
		check ( pMovie->pView->GetUserData() == 0 );
		pMovie->pUMovie = this;
		pMovie->TimingMode = TimingMode;
		pMovie->pView->SetUserData ( this );
		if ( ExternalInterface == NULL )
		{
			ExternalInterface = this;
		}

		for ( INT i = 0; i < ExternalTextures.Num(); i++ )
		{
			SetExternalTexture ( ExternalTextures ( i ).Resource, ExternalTextures ( i ).Texture );
		}
		if ( CaptureKeys.Num() )
		{
			pCaptureKeys = new TSet<NAME_INDEX>;
			for ( INT i = 0; i < CaptureKeys.Num(); i++ )
			{
				pCaptureKeys->Add ( CaptureKeys ( i ).GetIndex() );
			}
		}
		if ( FocusIgnoreKeys.Num() )
		{
			pFocusIgnoreKeys = new TSet<NAME_INDEX>;
			for ( INT i = 0; i < FocusIgnoreKeys.Num(); i++ )
			{
				pFocusIgnoreKeys->Add ( FocusIgnoreKeys ( i ).GetIndex() );
			}
		}
		
		// Resolve string links to UISoundTheme objects
		for( int i=0; i < SoundThemes.Num(); i++ )
		{
			if( SoundThemes(i).Theme == NULL )
			{
				SoundThemes(i).Theme = LoadObject<UUISoundTheme>(NULL, *SoundThemes(i).ThemeClassName, NULL, LOAD_None, NULL);

				if ( SoundThemes(i).Theme == NULL ) 
				{
					debugf ( NAME_DevGFxUI, TEXT ( "SoundThemes[%d] No valid Theme or ThemeClassName(%s) in GFxMovie %s" ), i, *SoundThemes(i).ThemeClassName, *GetName() );
				}
			}
		}

		return 1;
	}
	return 0;
}

/** Support class to manage OnLoad() and OnUnload() ActionScript callbacks from GFx */
class FGFxCLIKObjectEventCallback : public FunctionHandler
{
	protected:
		/** Reference to the GFxMoviePlayer that is responsible for these ActionScript objects and handling of their callbacks */
		UGFxMoviePlayer* Movie;

	public:
		FGFxCLIKObjectEventCallback ( UGFxMoviePlayer* InMovie ) : Movie ( InMovie ) {}

		/** Calls the appropriate UnrealScript event after the arguments have been extracted from the UnrealScript params.
		    If PathHandlingWidget != NULL, the event will be called on that widget instead of on Movie */
		virtual UBOOL CallEventHandler ( const FName& WidgetName, const FName& WidgetPath, UGFxObject* Widget, UGFxObject* PathHandlingWidget = NULL ) = 0;

		/** Called from the GFx runtime */
		virtual void Call ( const Params& params )
		{
			if ( Movie != NULL && !Movie->IsPendingKill() && !Movie->HasAnyFlags ( RF_Unreachable ) )
			{
				// Widget name, as an ASstring is the first parameter
				const FName WidgetName = FName ( FUTF8ToTCHAR ( params.pArgs[0].GetString() ) );

				// Widget path, as an ASstring is the second parameter
				FString WidgetPathString = FString ( FUTF8ToTCHAR ( params.pArgs[1].GetString() ) );
				const FName WidgetPath = FName ( *WidgetPathString );

				// See if the current path is registered with a path binding.
				// Split the string on the . delimiter, and keep going until we can split no more.
				// Note that we split from right to left, so that we always take the most derived path
				UGFxObject* PathHandlingWidget = NULL;
				FString Tmp;
				while ( WidgetPathString.Split ( TEXT ( "." ), &WidgetPathString, &Tmp, TRUE ) )
				{
					PathHandlingWidget = Movie->WidgetPathBindings.FindRef ( FName ( *WidgetPathString ) );
					if ( PathHandlingWidget != NULL )
					{
						// Found a match in the path binding list.  Search no more, and pass the init call to that widget
						debugf ( NAME_DevGFxUI, TEXT ( "Found path binding for path: %s" ), *WidgetPathString );
						break;
					}
				}

				// Determine where we should get our widget binding information from:  either the PathHandlingWidget if available, or the movie itself
				TArrayNoInit<FGFxWidgetBinding>* WidgetBindings = ( PathHandlingWidget != NULL ) ? & ( PathHandlingWidget->SubWidgetBindings ) : & ( Movie->WidgetBindings );

				// See if the widget name is bound to a specific subclass of UGFxObject, so we can create the proper type to pass into the WidgetInitialized function
				UClass* WidgetClass = UGFxObject::StaticClass();
				for ( INT i = 0; i < WidgetBindings->Num(); i++ )
				{
					if ( ( *WidgetBindings ) ( i ).WidgetName == WidgetName )
					{
						WidgetClass = ( *WidgetBindings ) ( i ).WidgetClass;
						break;
					}
				}

				// Create a widget of the appropriate type (defaults to UGFxObject) with the reference passed in through the ExternalInterface call
				UGFxObject* Widget = Movie->CreateValueAddRef ( &params.pArgs[2], WidgetClass );

				// Call the appropriate Unreal event corresponding to the ActionScript event
				const UBOOL bWidgetInitHandled = CallEventHandler ( WidgetName, WidgetPath, Widget, PathHandlingWidget );

				// If we care about warnings for widgets that weren't handed via WidgetInitialized, throw a log out
				if ( !bWidgetInitHandled && Movie->bLogUnhandedWidgetInitializations )
				{
					debugf ( NAME_DevGFxUIWarning, TEXT ( "Warning - Widget %s (%s) is making an init callback that is not being handled by WidgetInitialized()!" ), *WidgetName.ToString(), *WidgetPath.ToString() );
				}
			}
		}
};

/** Handler for the ActionScript OnLoad */
class FGFxCLIKObjectOnLoadEventCallback : public FGFxCLIKObjectEventCallback
{
	public:
		FGFxCLIKObjectOnLoadEventCallback ( UGFxMoviePlayer* InMovie ) : FGFxCLIKObjectEventCallback ( InMovie ) {}

		/** Calls the appropriate UnrealScript event after the arguments have been extracted from the UnrealScript params */
		virtual UBOOL CallEventHandler ( const FName& WidgetName, const FName& WidgetPath, UGFxObject* Widget, UGFxObject* PathHandlingWidget = NULL )
		{
			Movie->bWidgetsInitializedThisFrame = TRUE;

			if ( PathHandlingWidget != NULL )
			{
				return PathHandlingWidget->eventWidgetInitialized ( WidgetName, WidgetPath, Widget );
			}
			else
			{
				return Movie->eventWidgetInitialized ( WidgetName, WidgetPath, Widget );
			}
		}
};

/** Handler for the ActionScript OnUnload */
class FGFxCLIKObjectOnUnloadEventCallback : public FGFxCLIKObjectEventCallback
{
	public:
		FGFxCLIKObjectOnUnloadEventCallback ( UGFxMoviePlayer* InMovie ) : FGFxCLIKObjectEventCallback ( InMovie ) {}

		/** Calls the appropriate UnrealScript event after the arguments have been extracted from the UnrealScript params */
		virtual UBOOL CallEventHandler ( const FName& WidgetName, const FName& WidgetPath, UGFxObject* Widget, UGFxObject* PathHandlingWidget = NULL )
		{
			if ( PathHandlingWidget != NULL )
			{
				return PathHandlingWidget->eventWidgetUnloaded ( WidgetName, WidgetPath, Widget );
			}
			else
			{
				return Movie->eventWidgetUnloaded ( WidgetName, WidgetPath, Widget );
			}
		}
};

/** Class to handle the callbacks from ActionScript's gfxProcessSound() event.  Detects which UISoundTheme to use, and then sends the event name off to the UISoundTheme class for processing */
class FGFxSoundEventCallback : public FunctionHandler
{
		/** Movie responsible for the sounds generated by this movie.  This movie should store bindings for SoundTheme -> UISoundTheme  mapping in its pUMovie->SoundThemes array */
		FGFxMovie* Movie;

	public:
		FGFxSoundEventCallback ( FGFxMovie* InMovie ) : Movie ( InMovie ) {}

		/** Called when gfxProcessSound is called in ActionScript */
		virtual void Call ( const Params& params )
		{
			if ( Movie && Movie->pUMovie && !Movie->pUMovie->IsPendingKill() && !Movie->pUMovie->HasAnyFlags ( RF_Unreachable ) )
			{
				// Note:  pArgs[0] is an ActionScript reference which we do not care about

				// Grab the arguments from the function parameters
				const FName SoundThemeName = FName ( FUTF8ToTCHAR ( params.pArgs[1].GetString() ) );
				const FName SoundEventName = FName ( FUTF8ToTCHAR ( params.pArgs[2].GetString() ) );

				debugf ( NAME_DevGFxUI, TEXT ( "gfcProcessSound Callback - Sound Theme: %s SoundEvent: %s" ), *SoundThemeName.ToString(), *SoundEventName.ToString() );

				// Iterate over the sound themes available for this movie, and see if we have a match
				for ( INT i = 0; i < Movie->pUMovie->SoundThemes.Num(); i++ )
				{
					FSoundThemeBinding* CurrTheme = &Movie->pUMovie->SoundThemes ( i );
					if ( ( CurrTheme->ThemeName == SoundThemeName ) && ( CurrTheme->Theme != NULL ) )
					{
						// Found a match, call the event
						Movie->pUMovie->SoundThemes ( i ).Theme->eventProcessSoundEvent ( SoundEventName, Movie->pUMovie->eventGetPC() );
					}
				}
			}
		}
};

UBOOL UGFxMoviePlayer::Start ( UBOOL bStartPaused )
{
	if ( pMovie )
	{
		if ( !pMovie->Playing )
		{
			FGFxEngine::GetEngine()->StartScene ( pMovie, RenderTexture, TRUE, !bStartPaused );
		}
		bMovieIsOpen = TRUE;
		return TRUE;
	}
	else if ( MovieInfo )
	{
		const FString FullName = MovieInfo->GetOuter()->GetOuter()
		                         ? FString::Printf ( TEXT ( "%s.%s.%s" ), *MovieInfo->GetOutermost()->GetName(), *MovieInfo->GetFullGroupName ( 1 ), *MovieInfo->GetName() )
		                         : FString::Printf ( TEXT ( "%s.%s" ), *MovieInfo->GetOutermost()->GetName(), *MovieInfo->GetName() );

		if ( !Load ( FullName, 0 ) )
		{
			return 0;
		}
		GGFxEngine->StartScene ( pMovie, RenderTexture );

		bMovieIsOpen = TRUE;
		Movie* pmovie = pMovie->pView;
		int avmVer = pmovie->GetAVMVersion();

		// Set up the sound system callbacks.  This will bind our callback class to the ActionScript function "gfxProcessSound,"
		// which is called by CLIK widgets when certain events are fired.
		GFx::Value soundFunc;
		Ptr<FGFxSoundEventCallback> SoundEventHandler = *SF_NEW FGFxSoundEventCallback ( pMovie );
		pMovie->pView->CreateFunction ( &soundFunc, SoundEventHandler );
		if ( avmVer == 1 )
		{
			pmovie->SetVariable ( "_global.gfxProcessSound", soundFunc );
		}
		else if ( avmVer == 2 )
		{
			GFx::Value arg ( "scaleform.gfx.Extensions" );
			GFx::Value extensionsClass;
			pmovie->Invoke ( "flash.utils.getDefinitionByName", &extensionsClass, &arg, 1 );
			extensionsClass.SetMember ( "gfxProcessSound", soundFunc );
		}

		// Set up the OnLoad and OnUnload callbacks for this movie player.  These bind our callbacks to the ActionScript functions
		// "CLIK_loadCallback" and "CLIK_unloadCallback"
		GFx::Value loadFunc;
		Ptr<FGFxCLIKObjectOnLoadEventCallback> LoadCallback = *SF_NEW FGFxCLIKObjectOnLoadEventCallback ( this );
		pmovie->CreateFunction ( &loadFunc, LoadCallback, NULL );

		GFx::Value unloadFunc;
		Ptr<FGFxCLIKObjectOnUnloadEventCallback> UnloadCallback = *SF_NEW FGFxCLIKObjectOnUnloadEventCallback ( this );
		pmovie->CreateFunction ( &unloadFunc, UnloadCallback, NULL );

		if ( avmVer == 1 )
		{
			pmovie->SetVariable ( "_global.CLIK_loadCallback", loadFunc );
			pmovie->SetVariable ( "_global.CLIK_unloadCallback", unloadFunc );
		}
		else if ( avmVer == 2 )
		{
			GFx::Value arg ( "scaleform.gfx.Extensions" );
			GFx::Value extensionsClass;
			pmovie->Invoke ( "flash.utils.getDefinitionByName", &extensionsClass, &arg, 1 );
			extensionsClass.SetMember ( "CLIK_addedToStageCallback", loadFunc );
		}

		return TRUE;
	}
	debugf ( TEXT ( "GFxMoviePlayer::Start called with no movie loaded" ) );
	return FALSE;
}

void UGFxMoviePlayer::Close ( UBOOL bUnload )
{
	if ( GGFxEngine && pMovie )
	{

		// call OnClose unless garbage collected
		if ( !HasAnyFlags ( RF_Unreachable ) )
		{
			eventOnClose();
		}

		// Remove movie from all-movie list in case of unload
		INT index;
		if ( bUnload && GGFxEngine->AllMovies.FindItem ( pMovie, index ) )
		{
			GGFxEngine->AllMovies.Remove ( index );
		}

		// call Cleanup() unless garbage collected
		if ( !HasAnyFlags ( RF_Unreachable ) )
		{
			eventOnCleanup();
		}

		// Close the scene after the close event has been called,
		// so that the close event can actually operate on the scene if desired.
		GGFxEngine->CloseScene ( pMovie, bUnload );
		bMovieIsOpen = FALSE;

		if ( !HasAnyFlags ( RF_Unreachable ) )
		{
			eventConditionalClearPause();
		}

		if ( bUnload )
		{
			pMovie = 0;
			LocalPlayerOwnerIndex = 0;

			//Mark the scene as pending kill
			MarkPendingKill();
		}

		// Clear up any render target memory left over from filters
		/* XXX
		FGFxRenderer* Renderer = GGFxEngine->GetRenderer();
		if( Renderer )
		{
			GGFxEngine->GetRenderer()->ReleaseTempRenderTargets(0);
		}
		*/
	}
}

/**Sets whether or not a movie is allowed to receive focus.  Defaults to true*/
void UGFxMoviePlayer::SetMovieCanReceiveFocus ( UBOOL bCanReceiveFocus )
{
	if ( pMovie )
	{
		pMovie->bCanReceiveFocus = bCanReceiveFocus;
	}
}

/**Sets whether or not a movie is allowed to receive input.  Defaults to true*/
void UGFxMoviePlayer::SetMovieCanReceiveInput ( UBOOL bCanReceiveInput )
{
	if ( pMovie )
	{
		pMovie->bCanReceiveInput = bCanReceiveInput;
	}
}

void UGFxMoviePlayer::SetPause ( UBOOL bPausePlayback )
{
	if ( pMovie )
	{
		pMovie->fUpdate = !bPausePlayback;
	}
}

void UGFxMoviePlayer::SetTimingMode ( BYTE Mode )
{
	TimingMode = Mode;
	if ( pMovie )
	{
		pMovie->TimingMode = Mode;
		pMovie->LastTime = 0.0;
	}
}

void UGFxMoviePlayer::ClearCaptureKeys()
{
	delete pCaptureKeys;
	pCaptureKeys = NULL;
}

void UGFxMoviePlayer::AddCaptureKey ( FName ukey )
{
	if ( !pCaptureKeys )
	{
		pCaptureKeys = new TSet<NAME_INDEX>;
	}
	pCaptureKeys->Add ( ukey.GetIndex() );
}

void UGFxMoviePlayer::ClearFocusIgnoreKeys()
{
	delete pFocusIgnoreKeys;
	pFocusIgnoreKeys = NULL;
}

void UGFxMoviePlayer::AddFocusIgnoreKey ( FName ukey )
{
	if ( !pFocusIgnoreKeys )
	{
		pFocusIgnoreKeys = new TSet<NAME_INDEX>;
	}
	pFocusIgnoreKeys->Add ( ukey.GetIndex() );
}

void UGFxMoviePlayer::FlushPlayerInput ( UBOOL CaptureKeysOnly )
{
	if ( GGFxEngine )
	{
		if ( !CaptureKeysOnly )
		{
			GGFxEngine->FlushPlayerInput ( NULL );
		}
		else if ( pCaptureKeys )
		{
			GGFxEngine->FlushPlayerInput ( pCaptureKeys );
		}
	}
}

UGameViewportClient *UGFxMoviePlayer::GetGameViewportClient()
{
	return GEngine ? GEngine->GameViewport : NULL;
}

void UGFxMoviePlayer::SetViewport ( INT X, INT Y, INT Width, INT Height )
{
	if ( GGFxEngine && pMovie )
	{
		GFx::Viewport vp;
		// Retrieve old viewport so the ViewFlags (for RenderTexture and gamma) are preserved.
		pMovie->pView->GetViewport ( &vp );
		vp.Width = Width;
		vp.Height = Height;
		vp.Left = X;
		vp.Top = Y;
		pMovie->pView->SetViewport ( vp );
		pMovie->fViewportSet = 1;
	}
}

void UGFxMoviePlayer::SetViewScaleMode ( BYTE scale )
{
	if ( pMovie )
	{
		pMovie->pView->SetViewScaleMode ( ( Movie::ScaleModeType ) scale );
	}
}

void UGFxMoviePlayer::SetAlignment ( BYTE align )
{
	if ( pMovie )
	{
		pMovie->pView->SetViewAlignment ( ( Movie::AlignType ) align );
	}
}

void UGFxMoviePlayer::GetVisibleFrameRect ( FLOAT& x0, FLOAT& y0, FLOAT& x1, FLOAT& y1 )
{
	if ( pMovie )
	{
		RectF rect = pMovie->pView->GetVisibleFrameRect();
		x0 = rect.x1;
		x1 = rect.x2;
		y0 = rect.y1;
		y1 = rect.y2;
	}
}

void UGFxMoviePlayer::SetView3D ( const FMatrix &matView )
{
	if ( pMovie )
	{
		Matrix3F m ( matView.M );
		pMovie->pView->SetViewMatrix3D ( m );
	}
}

void UGFxMoviePlayer::SetPerspective3D ( const FMatrix &matPersp )
{
	if ( pMovie )
	{
		Matrix4F m ( matPersp.M );
		pMovie->pView->SetProjectionMatrix3D ( m );
	}
}

UBOOL UGFxMoviePlayer::SetExternalTexture ( const FString& Name, class UTexture* Texture )
{
	if ( !GGFxEngine || !pMovie )
	{
		return FALSE;
	}

	Resource* pres = pMovie->pView->GetMovieDef()->GetResource ( FTCHARToUTF8 ( *Name ) );
	ImageResource* pimageRes = 0;
	if ( pres && pres->GetResourceType() == Resource::RT_Image )
	{
		pimageRes = ( ImageResource* ) pres;
	}

	if ( pimageRes )
	{
		ImageBase* pimageOrig = pimageRes->GetImage();
		if ( pimageOrig )
		{
			Ptr<TextureImage> pti = * GGFxEngine->CreateUTextureImage ( Texture );

			// If the texture is different size than the image it replaces, set the image matrix to their relative scales.
			ImageSize originalSize = pimageOrig->GetSize();
			ImageSize newSize      = pti->GetSize();
			Size<float> scaleParameters ( ( ( float ) originalSize.Width ) / newSize.Width, ( ( float ) originalSize.Height ) / newSize.Height );
			Matrix2F textureMatrix = Matrix2F::Scaling ( scaleParameters.Width, scaleParameters.Height );
			pti->SetMatrix ( textureMatrix );

			pimageRes->SetImage ( pti );

			pMovie->pView->ForceUpdateImages();

			return TRUE;
		}
	}

	return FALSE;
}

static inline void ConvertFASValueToGFxValue(GFx::Value& OutValue, const FASValue& InValue)
{
    switch ( InValue.Type )
    {
    case AS_Null:
        OutValue.SetNull();
        break;
    case AS_Number:
        OutValue.SetNumber ( InValue.N );
        break;
    case AS_Int:
        OutValue.SetInt ( InValue.I );
        break;
    case AS_Boolean:
        OutValue.SetBoolean ( InValue.B );
        break;
    case AS_String:
#if TCHAR_IS_1_BYTE
		OutValue.SetString ( *InValue.S );
#else
        OutValue.SetStringW ( *InValue.S );
#endif
        break;
    default:
        OutValue.SetUndefined();
        break;
    }
}

static inline void ConvertGFxValueToFASValue(FASValue& OutValue, const GFx::Value& InValue)
{
    switch ( InValue.GetType() )
    {
    case GFx::Value::VT_Number:
        OutValue.Type = AS_Number;
        OutValue.N = InValue.GetNumber();
        break;
    case GFx::Value::VT_Int:
        OutValue.Type = AS_Int;
        OutValue.I = InValue.GetInt();
        break;
    case GFx::Value::VT_UInt:
        OutValue.Type = AS_Int;
        OutValue.I = (int) InValue.GetUInt();
        break;
    case GFx::Value::VT_Boolean:
        OutValue.Type = AS_Boolean;
        OutValue.B = InValue.GetBool();
        break;
#if !TCHAR_IS_1_BYTE
    case GFx::Value::VT_StringW:
        OutValue.Type = AS_String;
		OutValue.S = InValue.GetStringW();
        break;
#endif
    case GFx::Value::VT_String:
        OutValue.Type = AS_String;
        OutValue.S = FUTF8ToTCHAR ( InValue.GetString() );
        break;
    case GFx::Value::VT_Null:
        OutValue.Type = AS_Null;
        break;
    default:
        OutValue.Type = AS_Undefined;
        break;
    }

}

FASValue UGFxMoviePlayer::Invoke ( const FString &method, const TArray<struct FASValue> &args )
{
	FASValue result;

	if ( GGFxEngine && pMovie )
	{
		AutoGFxValueArray ( gargs, args.Num() );
		for ( int i = 0; i < args.Num(); i++ )
		{
            ConvertFASValueToGFxValue( gargs[i], args ( i ) );
		}

		GFx::Value gresult;
		if ( pMovie->pView->Invoke ( FTCHARToUTF8 ( *method ), &gresult, gargs, args.Num() ) )
		{
            ConvertGFxValueToFASValue ( result, gresult );
		}
	}

	return result;
}

/** Binds a widget to an object path to receive WidgetInitialized() callbacks for all child widgets of that path.
    Passing in NULL for WidgetToBind will remove bindings for the specified path, if any */
void UGFxMoviePlayer::SetWidgetPathBinding ( UGFxObject* WidgetToBind, FName Path )
{
	if ( !WidgetToBind )
	{
		// Passing in a NULL reference to a widget will remove the binding, if there is one
		WidgetPathBindings.Remove ( Path );
	}
	else
	{
		// Bind the passed in widget to the specified path.
		WidgetPathBindings.Set ( Path, WidgetToBind );
	}
}

int UGFxMoviePlayer::GetAVMVersion ()
{
	if ( pMovie )
	{
		return pMovie->pView->GetAVMVersion ();
	}
	else
	{
		return 0;
	}
}

UGFxObject* UGFxMoviePlayer::CreateValue ( const void* gfxvalue, UClass* Type )
{
	check ( 0 );

	UGFxObject* Result = ConstructObject<UGFxObject> ( Type, this );
	appMemcpy ( Result->Value, gfxvalue, sizeof ( GFx::Value ) );
	return Result;
}

/**
* Provides a mechanism for looking up existing UObjects that observe GFx::Value (action script values).
*/
class FGFxObjectHolder : public GFx::ASUserData
{
	public:
		UGFxObject*      Obj;
		UGFxMoviePlayer* Movie;

		FGFxObjectHolder ( UGFxObject* InObj, UGFxMoviePlayer* InMovie ) : Obj ( InObj ), Movie ( InMovie ) {}

		void OnDestroy ( GFx::Movie* pmovie, void* pobject )
		{
			Release();
		}
};

/**
* Create a UObject proxy for a GFx::Value (Action Script variable). If an existing UObject already exists for this movie,
* return the existing object.
*
* @param gfxvalue   The GFx::Value for which to make a UObject proxy.
* @param Type       The desired type of the UObject proxy.
*
* @return a UObject proxy for the GFx::Value
*/
UGFxObject* UGFxMoviePlayer::CreateValueAddRef ( const void* gfxvalue, UClass* Type )
{
	GFx::Value* InValue = ( GFx::Value* ) gfxvalue;
	if ( !InValue->IsObject() )
	{
		// We don't have a sane GFx::Value. An appropriate UObject representation is None.
		return NULL;
	}
	else
	{
		// In case this variable already exists, try to return the existing one.
		GFx::ASUserData* UserData = InValue->GetUserData();
		if ( UserData )
		{
			FGFxObjectHolder* Holder = ( FGFxObjectHolder* ) UserData;
			check ( Holder->Obj );
			// The UObject proxy for this GFx::Value already exists, but its type
			// may be different than requested.
			if ( Holder->Obj->GetClass() == Type )
			{
				// We have an identical proxy: same UObject type and GFx::Value.
				return Holder->Obj;
			}
			else
			{
				Holder->Release();
			}
		}

		// Create a new UObject proxy for the GFx::Value.
		UGFxObject* Result = ConstructObject<UGFxObject> ( Type, this );
		new ( Result->Value ) GFx::Value ( *InValue );

		GFx::Value *NewValue = ( GFx::Value* ) Result->Value;
		NewValue->SetUserData ( SF_NEW FGFxObjectHolder ( Result, this ) );

		return Result;
	}
}

void UGFxObject::SetValue ( void* gfxvalue )
{
	Clear();
	new ( Value ) GFx::Value ( * ( GFx::Value* ) gfxvalue );
	( ( GFx::Value* ) Value )->SetUserData ( SF_NEW FGFxObjectHolder ( this, NULL ) );
}

UGFxObject* UGFxMoviePlayer::GetVariableObject ( const FString& Path, class UClass* Type )
{
	GFx::Value gresult;
	if ( pMovie && pMovie->pView && pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *Path ) ) )
	{
		return CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
	}
	return 0;
}

UGFxObject* UGFxMoviePlayer::CreateObject ( const FString& ASClass, class UClass* Type, const TArray<FASValue>& args )
{
	GFx::Value gresult;
	if ( pMovie && pMovie->pView )
	{
		AutoGFxValueArray ( gargs, args.Num() );
		for ( int i = 0; i < args.Num(); i++ )
		{
			ConvertFASValueToGFxValue ( gargs[i], args ( i ) );
		}

		pMovie->pView->CreateObject ( &gresult, FTCHARToUTF8 ( *ASClass ), gargs, args.Num() );
		return CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
	}
	return 0;
}

UGFxObject* UGFxMoviePlayer::CreateArray()
{
	GFx::Value gresult;
	if ( pMovie && pMovie->pView )
	{
		pMovie->pView->CreateArray ( &gresult );
		return CreateValueAddRef ( &gresult, UGFxObject::StaticClass() );
	}
	return 0;
}

void UGFxMoviePlayer::SetVariableObject ( const FString& Path, UGFxObject* Value )
{
	if ( pMovie && pMovie->pView )
	{
		if ( Value )
		{
			pMovie->pView->SetVariable ( FTCHARToUTF8 ( *Path ), * ( GFx::Value* ) Value->Value );
		}
		else
		{
			GFx::Value undef;
			pMovie->pView->SetVariable ( FTCHARToUTF8 ( *Path ), undef );
		}
	}
}

FASValue UGFxMoviePlayer::GetVariable ( const FString &path )
{
	FASValue result;
	GFx::Value gresult;

	if ( pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *path ) ) )
	{
        ConvertGFxValueToFASValue ( result, gresult );
	}
	return result;
}

UBOOL UGFxMoviePlayer::GetVariableBool ( const FString &path )
{
	if ( !GGFxEngine || !pMovie )
	{
		return FALSE;
	}

	GFx::Value gresult ( GFx::Value::VT_ConvertBoolean );

	pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *path ) );
	if ( gresult.GetType() != GFx::Value::VT_Boolean )
	{
		warnf ( TEXT ( "UGFxMoviePlayer::GetVariableBool(): value is not a bool. Returning FALSE." ) );
		return FALSE;
	}
	else
	{
		return gresult.GetBool();
	}
}
FLOAT UGFxMoviePlayer::GetVariableNumber ( const FString &path )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	GFx::Value gresult ( GFx::Value::VT_ConvertNumber );

	pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *path ) );
	if ( gresult.GetType() != GFx::Value::VT_Number )
	{
		warnf ( TEXT ( "UGFxMoviePlayer::GetVariableNumber(): value is not a number. Returning 0." ) );
		return 0;
	}
    else
	{
		return gresult.GetNumber();
	}
}
INT UGFxMoviePlayer::GetVariableInt ( const FString &path )
{
    if ( !GGFxEngine || !pMovie )
    {
        return 0;
    }

    GFx::Value gresult ( GFx::Value::VT_ConvertInt );

    pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *path ) );
    if ( gresult.GetType() != GFx::Value::VT_Int )
    {
        warnf ( TEXT ( "UGFxMoviePlayer::GetVariableInt(): value is not a number. Returning 0." ) );
        return 0;
    }
    else
    {
        return gresult.GetInt();
    }
}
FString UGFxMoviePlayer::GetVariableString ( const FString &path )
{
	if ( !GGFxEngine || !pMovie )
	{
		return FString();
	}

#if TCHAR_IS_1_BYTE
	GFx::Value gresult ( GFx::Value::VT_ConvertString );

	pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *path ) );
#else
	GFx::Value gresult ( GFx::Value::VT_ConvertStringW );

	pMovie->pView->GetVariable ( &gresult, FTCHARToUTF8 ( *path ) );
	if ( gresult.GetType() == GFx::Value::VT_StringW )
	{
		return gresult.GetStringW();
	}
	else
#endif
	if ( gresult.GetType() == GFx::Value::VT_String )
	{
		return FString ( FUTF8ToTCHAR ( gresult.GetString() ) );
	}
	else
	{
		return FString();
	}
}

void UGFxMoviePlayer::SetVariable ( const FString &path, FASValue arg )
{
	if ( GGFxEngine && pMovie )
	{
		GFx::Value garg;
        ConvertFASValueToGFxValue( garg, arg );

		pMovie->pView->SetVariable ( FTCHARToUTF8 ( *path ), garg );
	}
}

void UGFxMoviePlayer::SetVariableBool ( const FString &path, UBOOL b )
{
	if ( GGFxEngine && pMovie )
	{
		GFx::Value v;
		v.SetBoolean ( b ? 1 : 0 );
		pMovie->pView->SetVariable ( FTCHARToUTF8 ( *path ), v );
	}
}
void UGFxMoviePlayer::SetVariableNumber ( const FString &path, float f )
{
	if ( GGFxEngine && pMovie )
	{
		pMovie->pView->SetVariable ( FTCHARToUTF8 ( *path ), GFx::Value ( f ) );
	}
}
void UGFxMoviePlayer::SetVariableInt ( const FString &path, int i )
{
    if ( GGFxEngine && pMovie )
    {
        GFx::Value val;
        val.SetInt ( i );
        pMovie->pView->SetVariable ( FTCHARToUTF8 ( *path ), val );
    }
}
void UGFxMoviePlayer::SetVariableString ( const FString &path, const FString& s )
{
	if ( GGFxEngine && pMovie )
	{
		pMovie->pView->SetVariable ( FTCHARToUTF8 ( *path ), GFx::Value ( *s ) );
	}
}

UBOOL UGFxMoviePlayer::GetVariableArray ( const FString& path, INT index, TArray<struct FASValue>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	String pathutf8 = ( const char* ) FTCHARToUTF8 ( *path );
	INT count = pMovie->pView->GetVariableArraySize ( pathutf8 );
	AutoGFxValueArray ( gvals, count );

	if ( !pMovie->pView->GetVariableArray ( Movie::SA_Value, pathutf8, index, gvals, count ) )
	{
		return 0;
	}

	arg.Empty();
	for ( INT i = 0; i < count; i++ )
	{
		FASValue val;
        ConvertGFxValueToFASValue( val, gvals[i] );
		arg.AddItem ( val );
	}
	return 1;
}

UBOOL UGFxMoviePlayer::GetVariableIntArray ( const FString& Path, INT Index, TArray<INT>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	String pathutf8 = ( const char* ) FTCHARToUTF8 ( *Path );
	INT count = pMovie->pView->GetVariableArraySize ( pathutf8 );
	AutoGFxValueArray ( gvals, count );

	arg.Empty();
	arg.Add ( count );
	return pMovie->pView->GetVariableArray ( Movie::SA_Int, pathutf8, Index, &arg ( 0 ), count );
}

UBOOL UGFxMoviePlayer::GetVariableFloatArray ( const FString& Path, INT Index, TArray<FLOAT>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	String pathutf8 = ( const char* ) FTCHARToUTF8 ( *Path );
	INT count = pMovie->pView->GetVariableArraySize ( pathutf8 );
	AutoGFxValueArray ( gvals, count );

	arg.Empty();
	arg.Add ( count );
	return pMovie->pView->GetVariableArray ( Movie::SA_Float, pathutf8, Index, &arg ( 0 ), count );
}

UBOOL UGFxMoviePlayer::GetVariableStringArray ( const FString& path, INT index, TArray<FString>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	String pathutf8 = ( const char* ) FTCHARToUTF8 ( *path );
	INT count = pMovie->pView->GetVariableArraySize ( pathutf8 );

#if TCHAR_IS_1_BYTE
	char** outs = ( char** ) appAlloca ( sizeof ( char* ) * count );
	if ( !pMovie->pView->GetVariableArray ( Movie::SA_String, pathutf8, index, outs, count ) )
#else
	wchar_t** outs = ( wchar_t** ) appAlloca ( sizeof ( wchar_t* ) * count );
	if ( !pMovie->pView->GetVariableArray ( Movie::SA_StringW, pathutf8, index, outs, count ) )
#endif
	{
		return 0;
	}

	arg.Empty();
	for ( INT i = 0; i < count; i++ )
	{
		arg.AddItem ( FString ( outs[i] ) );
	}
	return 1;
}

UBOOL UGFxMoviePlayer::SetVariableArray ( const FString& Path, INT Index, const TArray<struct FASValue>& args )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	AutoGFxValueArray ( gargs, args.Num() );
	for ( int i = 0; i < args.Num(); i++ )
	{
        ConvertFASValueToGFxValue ( gargs[i], args ( i ) );
	}

	return pMovie->pView->SetVariableArray ( Movie::SA_Value, FTCHARToUTF8 ( *Path ), Index, gargs, args.Num() );
}
UBOOL UGFxMoviePlayer::SetVariableIntArray ( const FString& Path, INT Index, const TArray<INT>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	return pMovie->pView->SetVariableArray ( Movie::SA_Int, FTCHARToUTF8 ( *Path ), Index, &arg ( 0 ), arg.Num() );
}
UBOOL UGFxMoviePlayer::SetVariableFloatArray ( const FString& Path, INT Index, const TArray<FLOAT>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

	return pMovie->pView->SetVariableArray ( Movie::SA_Float, FTCHARToUTF8 ( *Path ), Index, &arg ( 0 ), arg.Num() );
}
UBOOL UGFxMoviePlayer::SetVariableStringArray ( const FString& Path, INT Index, const TArray<FString>& arg )
{
	if ( !GGFxEngine || !pMovie )
	{
		return 0;
	}

#if TCHAR_IS_1_BYTE
	const char** wstrs = ( const char** ) appAlloca ( sizeof ( GFx::Value ) * arg.Num() );
	for ( int i = 0; i < arg.Num(); i++ )
	{
		wstrs[i] = * ( arg ( i ) );
	}

	return pMovie->pView->SetVariableArray ( Movie::SA_String, FTCHARToUTF8 ( *Path ), Index, wstrs, arg.Num() );
#else
	const wchar_t** wstrs = ( const wchar_t** ) appAlloca ( sizeof ( GFx::Value ) * arg.Num() );
	for ( int i = 0; i < arg.Num(); i++ )
	{
		wstrs[i] = * ( arg ( i ) );
	}

	return pMovie->pView->SetVariableArray ( Movie::SA_StringW, FTCHARToUTF8 ( *Path ), Index, wstrs, arg.Num() );
#endif
}

void UGFxMoviePlayer::Advance ( FLOAT time )
{
	if ( GGFxEngine && pMovie )
	{
		pMovie->pView->Advance ( time );
		PostAdvance ( time );
	}
}

/**
 * Called after every Advance() of the GFx movie.
 *
 * @param	DeltaTime	Time that the movie was advanced by
 */
void UGFxMoviePlayer::PostAdvance ( FLOAT DeltaTime )
{
	// If the OnPostAdvance delegate is set, call it here
	if ( DELEGATE_IS_SET ( OnPostAdvance ) )
	{
		delegateOnPostAdvance ( DeltaTime );
	}

	// See if any widgets were initialized during the above Advance() call, and make the callback if so
	if ( bWidgetsInitializedThisFrame )
	{
		eventPostWidgetInit();
		bWidgetsInitializedThisFrame = FALSE;
	}
}

/**
 * Update the splitscreen layout object, if it is set
 */
void UGFxMoviePlayer::UpdateSplitscreenLayout()
{
	// Modify Y
	if ( SplitscreenLayoutObject != NULL )
	{
		// Find the real values from the viewport
		UBOOL bIsFullscreen = FALSE;
		INT BaseSizeX, BaseSizeY;
		FViewport* HudViewport  = GGFxEngine->GetRenderViewport();
		if ( HudViewport != NULL )
		{
			BaseSizeX = HudViewport->GetSizeX();
			BaseSizeY = HudViewport->GetSizeY();
			GFx::Viewport vp;
			pMovie->pView->GetViewport ( &vp );
			if ( vp.Width == BaseSizeX && vp.Height == BaseSizeY )
			{
				bIsFullscreen = TRUE;
			}
		}

		// Modify ONLY if: This is the second player AND the movie will be splitscreened AND the movie is actually being splitscreened at the moment
		UBOOL bShouldBeModified = LocalPlayerOwnerIndex != 0 && bOnlyOwnerFocusable && !bIsFullscreen;

		if ( bShouldBeModified && !bIsSplitscreenLayoutModified )
		{
			FASDisplayInfo DisplayInfo;
			DisplayInfo = SplitscreenLayoutObject->GetDisplayInfo();
			DisplayInfo.Y += SplitscreenLayoutYAdjust;
			SplitscreenLayoutObject->SetDisplayInfo(DisplayInfo);
			bIsSplitscreenLayoutModified = TRUE;
		}
	}
}

class FOutputToGfxPolicy
{
	public:
		FOutputToGfxPolicy()
			: bContainsMarkup ( FALSE )
			, bContainsErrors ( FALSE )
		{
		}

		void OnBeginFont ( const FString& FontName )
		{
			bContainsMarkup = TRUE;
			GfxMarkupString += FString::Printf ( TEXT ( "<FONT FACE='%s'>" ), *FontName );
		}

		void OnEndFont()
		{
			GfxMarkupString += TEXT ( "</FONT>" );
		}

		void OnBeginColor ( FLinearColor InColor )
		{
			bContainsMarkup = TRUE;
			FColor LDRColor = InColor.ToFColor ( FALSE );
			GfxMarkupString += FString::Printf ( TEXT ( "<FONT COLOR='#%02X%02X%02X'>" ), LDRColor.R, LDRColor.G, LDRColor.B );
		}

		void OnEndColor()
		{
			GfxMarkupString += TEXT ( "</FONT>" );
		}

		void OnPlainText ( const FString& InPlainText )
		{
			GfxMarkupString += InPlainText;
		}

		void OnNewLine()
		{
			GfxMarkupString += TEXT ( "\n" );
		}

		void OnError ( const FString& InMalformedToken )
		{
			GfxMarkupString += InMalformedToken;
			bContainsErrors = TRUE;
		}

		const FString& GetGfxString()
		{
			return GfxMarkupString;
		}

		UBOOL ShouldShowMarkup()
		{
			return bContainsMarkup && !bContainsErrors;
		}

	private:
		UBOOL bContainsMarkup;
		UBOOL bContainsErrors;
		FString GfxMarkupString;

};

/** Does an action script function returns a value or is it void */
enum ActionScriptReturnSignature { FunctionIsVoid, FunctionReturnsValue };

/**
 * Utility function for invoking an Action Script function.
 *
 * @param InInvoker            The object that will call Invoke on the actual function.
 * @param MoviePlayer          The GFxMoviePlayer that is executing the action script.
 * @param DefaultReturnValue   The default return value in case the Invocation fails.
 * @param Stack                The UnrealScript execution context
 * @param Result               Result of the unreal script wrapper function call
 * @param ReturnSig            Does the ActionScript function return a value or is it void?
 */
template<typename InvokerType>
void ExecuteActionScript ( InvokerType InInvoker, UGFxMoviePlayer* MoviePlayer, GFx::Value& DefaultReturnValue, FFrame& Stack, RESULT_DECL, ActionScriptReturnSignature ReturnSig = FunctionReturnsValue )
{
	P_GET_STR ( path );
	P_FINISH;

	UFunction* Function = Cast<UFunction> ( Stack.Node );
	if ( Function == NULL )
	{
		warnf ( TEXT ( " Failed to find function '%s'" ), *Stack.Node->GetName() );
		return;
	}

	UINT i = 0, numArgs = 0;
	for ( TFieldIterator<UProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
	{
		numArgs++;
	}

	AutoGFxValueArray ( args, numArgs );

	for ( TFieldIterator<UProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It, ++i )
	{
		FGFxEngine::ConvertUPropToGFx ( *It, Stack.Locals + It->Offset, args[i], MoviePlayer->pMovie->pView );
	}

	if ( MoviePlayer->pMovie )
	{
		InInvoker->Invoke ( FTCHARToUTF8 ( *path ), &DefaultReturnValue, args, numArgs );

		// If the function is not void, translate the return value into a value that UnrealScript understands.
		if ( ReturnSig != FunctionIsVoid )
		{
			UProperty* ReturnProperty = Function->GetReturnProperty();
			if ( ReturnProperty != NULL )
			{
				FGFxEngine::ConvertGFxToUProp ( ReturnProperty, ( BYTE* ) Result, DefaultReturnValue, MoviePlayer );
			}
		}
	}
}

void UGFxMoviePlayer::execActionScriptVoid ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult;
	ExecuteActionScript ( this->pMovie->pView, this, CallResult, Stack, Result, FunctionIsVoid );
}

void UGFxMoviePlayer::execActionScriptInt ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult ( GFx::Value::VT_ConvertInt );
	ExecuteActionScript ( this->pMovie->pView, this, CallResult, Stack, Result );
}

void UGFxMoviePlayer::execActionScriptFloat ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult ( GFx::Value::VT_ConvertNumber );
	ExecuteActionScript ( this->pMovie->pView, this, CallResult, Stack, Result );
}

void UGFxMoviePlayer::execActionScriptString ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult ( GFx::Value::VT_ConvertStringW );
	ExecuteActionScript ( this->pMovie->pView, this, CallResult, Stack, Result );
}

void UGFxMoviePlayer::execActionScriptObject ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult;
	ExecuteActionScript ( this->pMovie->pView, this, CallResult, Stack, Result );
}

struct CreateObjectWrapper
{
	GFx::Movie* View;

	CreateObjectWrapper ( GFx::Movie *InView ) : View ( InView ) {}

	void Invoke ( const char* InClass, GFx::Value* OutResult, const GFx::Value* InArgs, int InNumArgs)
	{
		View->CreateObject ( OutResult, InClass, InArgs, InNumArgs );
	}
};

void UGFxMoviePlayer::execActionScriptConstructor ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult;
	CreateObjectWrapper Constructor ( pMovie->pView );
	ExecuteActionScript ( &Constructor, this, CallResult, Stack, Result );
}


void UGFxObject::Clear()
{
	checkAtCompileTime ( sizeof ( Value ) >= sizeof ( GFx::Value ), GFxObjectValueIsTooSmall );
	( ( GFx::Value* ) Value )->~Value();
	appMemset ( Value, 0, sizeof ( GFx::Value ) );
}

void UGFxObject::BeginDestroy()
{
	Super::BeginDestroy();

	GFx::Value* CurValue = ( GFx::Value* ) Value;
	if ( CurValue->IsObject() )
	{
		GFx::ASUserData* UserData = CurValue->GetUserData();
		if ( UserData )
		{
			FGFxObjectHolder* Holder = ( FGFxObjectHolder* ) UserData;
			CurValue->SetUserData ( NULL );
			Holder->Release();
		}
	}

	Clear();
}

struct FASValue UGFxObject::Invoke ( const FString& method, const TArray<struct FASValue>& args )
{
	FASValue result;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() )
	{
		AutoGFxValueArray ( gargs, args.Num() );
		for ( int i = 0; i < args.Num(); i++ )
		{
            ConvertFASValueToGFxValue ( gargs[i], args ( i ) );
		}

		GFx::Value gresult;
		if ( v->Invoke ( FTCHARToUTF8 ( *method ), &gresult, gargs, args.Num() ) )
		{
            ConvertGFxValueToFASValue ( result, gresult );
		}
	}
	return result;
}

void UGFxObject::GotoAndPlay ( const FString& frame )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GotoAndPlay ( FTCHARToUTF8 ( *frame ) );
	}
}

void UGFxObject::GotoAndStop ( const FString& frame )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GotoAndStop ( FTCHARToUTF8 ( *frame ) );
	}
}

void UGFxObject::GotoAndPlayI ( INT frame )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GotoAndPlay ( frame );
	}
}

void UGFxObject::GotoAndStopI ( INT frame )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GotoAndStop ( frame );
	}
}

struct FASValue UGFxObject::Get ( const FString& member )
{
	FASValue result;
	GFx::Value gresult;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() && v->GetMember ( FTCHARToUTF8 ( *member ), &gresult ) )
	{
        ConvertGFxValueToFASValue ( result, gresult );
	}
	return result;
}
class UGFxObject* UGFxObject::GetObject ( const FString& member, class UClass* Type )
{
		GFx::Value *v = ( GFx::Value* ) Value;
		GFx::Value gresult;
		if ( v->IsObject() && v->GetMember ( FTCHARToUTF8 ( *member ), &gresult ) )
		{
			return ( ( UGFxMoviePlayer* ) GetOuter() )->CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
		}
		return 0;
}

static void GFxDisplayInfoToU ( FASDisplayInfo& dest, const GFx::Value::DisplayInfo& src )
{
	dest.Alpha = src.GetAlpha();
	dest.Visible = src.GetVisible();
	dest.X = src.GetX();
	dest.Y = src.GetY();
	dest.Rotation = src.GetRotation();
	dest.XScale = src.GetXScale();
	dest.YScale = src.GetYScale();
	dest.hasAlpha = src.IsFlagSet ( GFx::Value::DisplayInfo::V_alpha );
	dest.hasVisible = src.IsFlagSet ( GFx::Value::DisplayInfo::V_visible );
	dest.hasX = src.IsFlagSet ( GFx::Value::DisplayInfo::V_x );
	dest.hasY = src.IsFlagSet ( GFx::Value::DisplayInfo::V_y );
	dest.hasXScale = src.IsFlagSet ( GFx::Value::DisplayInfo::V_xscale );
	dest.hasYScale = src.IsFlagSet ( GFx::Value::DisplayInfo::V_yscale );
	dest.hasRotation = src.IsFlagSet ( GFx::Value::DisplayInfo::V_rotation );
	dest.Z = src.GetZ();
	dest.ZScale = src.GetZScale();
	dest.XRotation = src.GetXRotation();
	dest.YRotation = src.GetYRotation();
	dest.hasZ = src.IsFlagSet ( GFx::Value::DisplayInfo::V_z );
	dest.hasZScale = src.IsFlagSet ( GFx::Value::DisplayInfo::V_zscale );
	dest.hasXRotation = src.IsFlagSet ( GFx::Value::DisplayInfo::V_xrotation );
	dest.hasYRotation = src.IsFlagSet ( GFx::Value::DisplayInfo::V_yrotation );
}

static void UDisplayInfoToGFx ( GFx::Value::DisplayInfo& dest, const FASDisplayInfo& src )
{
	dest.Initialize (
	    ( src.hasAlpha ? GFx::Value::DisplayInfo::V_alpha : 0 )
	    | ( src.hasVisible ? GFx::Value::DisplayInfo::V_visible : 0 )
	    | ( src.hasX ? GFx::Value::DisplayInfo::V_x : 0 )
	    | ( src.hasY ? GFx::Value::DisplayInfo::V_y : 0 )
	    | ( src.hasXScale ? GFx::Value::DisplayInfo::V_xscale : 0 )
	    | ( src.hasYScale ? GFx::Value::DisplayInfo::V_yscale : 0 )
	    | ( src.hasZ ? GFx::Value::DisplayInfo::V_z : 0 )
	    | ( src.hasZScale ? GFx::Value::DisplayInfo::V_zscale : 0 )
	    | ( src.hasXRotation ? GFx::Value::DisplayInfo::V_xrotation : 0 )
	    | ( src.hasYRotation ? GFx::Value::DisplayInfo::V_yrotation : 0 )
	    | ( src.hasRotation ? GFx::Value::DisplayInfo::V_rotation : 0 )
	    , src.X, src.Y, src.Rotation, src.XScale, src.YScale, src.Alpha, src.Visible
	    , src.Z, src.XRotation, src.YRotation, src.ZScale
	    , 55, NULL, NULL, Render::EdgeAA_Inherit
	);
}

static void ConvertMatrix4To2 ( Matrix2F& dest, const FMatrix& src )
{
	dest.M[0][0] = src.M[0][0];
	dest.M[0][1] = src.M[1][0];
	dest.M[0][3] = src.M[3][0];
	dest.M[1][0] = src.M[0][1];
	dest.M[1][1] = src.M[1][1];
	dest.M[1][3] = src.M[3][1];
}

static void ConvertMatrix4To3 ( Matrix3F& dest, const FMatrix& src )
{
	dest.M[0][0] = src.M[0][0];
	dest.M[0][1] = src.M[1][0];
	dest.M[0][2] = src.M[2][0];
	dest.M[0][3] = src.M[3][0];		// TX

	dest.M[1][0] = src.M[0][1];
	dest.M[1][1] = src.M[1][1];
	dest.M[1][2] = src.M[2][1];
	dest.M[1][3] = src.M[3][1];		// TY

	dest.M[2][0] = src.M[0][2];
	dest.M[2][1] = src.M[1][2];
	dest.M[2][2] = src.M[2][2];
	dest.M[2][3] = src.M[3][2];		// TZ
}

static void ConvertMatrix2To4 ( FMatrix& dest, const Matrix2F& src )
{
	dest.M[0][0] = src.M[0][0];
	dest.M[1][0] = src.M[0][1];
	dest.M[2][0] = 0;
	dest.M[3][0] = src.M[1][3];
	dest.M[0][1] = src.M[1][0];
	dest.M[1][1] = src.M[1][1];
	dest.M[2][1] = 0;
	dest.M[3][1] = src.M[1][3];
	dest.M[0][2] = 0;
	dest.M[1][2] = 0;
	dest.M[2][2] = 1;
	dest.M[3][2] = 0;
	dest.M[0][3] = 0;
	dest.M[1][3] = 0;
	dest.M[2][3] = 0;
	dest.M[3][3] = 1;
}

static void ConvertMatrix3To4 ( FMatrix& dest, const Matrix3F& src )
{
	dest.M[0][0] = src.M[0][0];
	dest.M[1][0] = src.M[0][1];
	dest.M[2][0] = src.M[0][2];
	dest.M[3][0] = src.M[0][3];		// TX

	dest.M[0][1] = src.M[1][0];
	dest.M[1][1] = src.M[1][1];
	dest.M[2][1] = src.M[1][2];
	dest.M[3][1] = src.M[1][3];		// TY

	dest.M[0][2] =src.M[2][0];
	dest.M[1][2] =src.M[2][1];
	dest.M[2][2] =src.M[2][2];
	dest.M[3][2] =src.M[2][3];		// TZ

	dest.M[0][3] = 0;
	dest.M[1][3] = 0;
	dest.M[2][3] = 0;
	dest.M[3][3] = 1;
}

UBOOL UGFxObject::GetBool ( const FString& member )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( !v->IsObject() )
	{
		return FALSE;
	}

	GFx::Value gresult ( GFx::Value::VT_ConvertBoolean );
	v->GetMember ( FTCHARToUTF8 ( *member ), &gresult );
	if ( gresult.GetType() != GFx::Value::VT_Boolean )
	{
		warnf ( TEXT ( "UGFxMoviePlayer::GetBool(): value is not a bool. Returning FALSE." ) );
		return FALSE;
	}
	else
	{
		return gresult.GetBool();
	}
}
FLOAT UGFxObject::GetFloat ( const FString& member )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( !v->IsObject() )
	{
		return 0;
	}

	GFx::Value gresult ( GFx::Value::VT_ConvertNumber );
	v->GetMember ( FTCHARToUTF8 ( *member ), &gresult );
	if ( gresult.GetType() != GFx::Value::VT_Number )
	{
		warnf ( TEXT ( "UGFxMoviePlayer::GetFloat(): value is not a float. Returning 0." ) );
		return 0;
	}
	else
	{
		return gresult.GetNumber();
	}
}
INT UGFxObject::GetInt ( const FString& member )
{
    GFx::Value *v = ( GFx::Value* ) Value;
    if ( !v->IsObject() )
    {
        return 0;
    }

    GFx::Value gresult ( GFx::Value::VT_ConvertInt );
    v->GetMember ( FTCHARToUTF8 ( *member ), &gresult );
    if ( gresult.GetType() != GFx::Value::VT_Int )
    {
        warnf ( TEXT ( "UGFxMoviePlayer::GetInt(): value is not an integer. Returning 0." ) );
        return 0;
    }
    else
    {
        return gresult.GetInt();
    }
}
FString UGFxObject::GetString ( const FString& member )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( !v->IsObject() )
	{
		return FString();
	}

#if TCHAR_IS_1_BYTE
	GFx::Value gresult ( GFx::Value::VT_ConvertString );
	v->GetMember ( FTCHARToUTF8 ( *member ), &gresult );
#else
	GFx::Value gresult ( GFx::Value::VT_ConvertStringW );
	v->GetMember ( FTCHARToUTF8 ( *member ), &gresult );
	if ( gresult.GetType() == GFx::Value::VT_StringW )
	{
		return gresult.GetStringW();
	}
	else
#endif
		if ( gresult.GetType() == GFx::Value::VT_String )
	{
		return FString ( FUTF8ToTCHAR ( gresult.GetString() ) );
	}
	else
	{
		return FString();
	}
}

FString UGFxObject::GetText()
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( !v->IsDisplayObject() )
	{
		return FString();
	}

#if TCHAR_IS_1_BYTE
	GFx::Value gresult ( GFx::Value::VT_ConvertString );
	v->GetText ( &gresult );
#else
	GFx::Value gresult ( GFx::Value::VT_ConvertStringW );
	v->GetText ( &gresult );
	if ( gresult.GetType() == GFx::Value::VT_StringW )
	{
		return gresult.GetStringW();
	}
	else
#endif
		if ( gresult.GetType() == GFx::Value::VT_String )
	{
		return FString ( FUTF8ToTCHAR ( gresult.GetString() ) );
	}
	else
	{
		return FString();
	}
}

struct FASDisplayInfo UGFxObject::GetDisplayInfo()
{
	FASDisplayInfo dinfo;
	GFx::Value::DisplayInfo ginfo;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GetDisplayInfo ( &ginfo );
		GFxDisplayInfoToU ( dinfo, ginfo );
	}
	return dinfo;
}
FMatrix UGFxObject::GetDisplayMatrix()
{
	FMatrix m4;
	Matrix2F m2;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GetDisplayMatrix ( &m2 );
		ConvertMatrix2To4 ( m4, m2 );
	}
	return m4;
}

FMatrix UGFxObject::GetDisplayMatrix3D()
{
	FMatrix m4;
	Matrix3F m3;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GetMatrix3D ( &m3 );
		ConvertMatrix3To4 ( m4, m3 );
	}
	return m4;
}

FASColorTransform UGFxObject::GetColorTransform()
{
	FASColorTransform cxfout;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		Cxform cxform;
		v->GetColorTransform ( &cxform );
		cxfout.Multiply.R = cxform.M[0][0];
		cxfout.Multiply.G = cxform.M[0][1];
		cxfout.Multiply.B = cxform.M[0][2];
		cxfout.Multiply.A = cxform.M[0][3];
		cxfout.Add.R = cxform.M[1][0];
		cxfout.Add.G = cxform.M[1][1];
		cxfout.Add.B = cxform.M[1][2];
		cxfout.Add.A = cxform.M[1][3];
	}
	return cxfout;
}
UBOOL UGFxObject::GetPosition ( FLOAT& X, FLOAT& Y )
{
	GFx::Value::DisplayInfo ginfo;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		v->GetDisplayInfo ( &ginfo );
		X = ginfo.GetX();
		Y = ginfo.GetY();
		return TRUE;
	}
	return FALSE;
}

void UGFxObject::Set ( const FString& member, struct FASValue arg )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() )
	{
		GFx::Value garg;
        ConvertFASValueToGFxValue( garg, arg );
		v->SetMember ( FTCHARToUTF8 ( *member ), garg );
	}
}

void UGFxObject::SetObject ( const FString& member, class UGFxObject* inval )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() )
	{
		if ( inval )
		{
			v->SetMember ( FTCHARToUTF8 ( *member ), * ( GFx::Value* ) inval->Value );
		}
		else
		{
			GFx::Value undef;
			v->SetMember ( FTCHARToUTF8 ( *member ), undef );
		}
	}
}
void UGFxObject::SetBool ( const FString& member, UBOOL B )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() )
	{
		GFx::Value arg;
		arg.SetBoolean ( B ? 1 : 0 );
		v->SetMember ( FTCHARToUTF8 ( *member ), arg );
	}
}
void UGFxObject::SetFloat ( const FString& member, FLOAT F )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() )
	{
		v->SetMember ( FTCHARToUTF8 ( *member ), GFx::Value ( F ) );
	}
}
void UGFxObject::SetInt ( const FString& member, INT I )
{
    GFx::Value *v = ( GFx::Value* ) Value;
    if ( v->IsObject() )
    {
        GFx::Value val;
        val.SetInt ( I );
        v->SetMember ( FTCHARToUTF8 ( *member ), val );
    }
}

/**
  * Convert Markup in strings
  * Currently only converts localized strings
  * Fixme - need to support font markup, etc
  */
UBOOL ConvertMarkupString ( const FString& S, FString& outString )
{
	return FALSE;
}

/**
 * Set the field specified by 'member' on this object.
 *
 * @param Member      The member of set object to set; e.g. "text" or "htmlText" or "foo"
 * @param s           Value of the string that is to be assigned to the member
 * @param InContext   The TranslationContext to use when resolving any tags enocuntered in the text.
 */
void UGFxObject::SetString ( const FString& member, const FString& S, UTranslationContext* TranslationContext )
{
	FOutputToGfxPolicy Translator;
	TTranslator::TranslateMarkup ( TranslationContext, S, Translator );

	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() )
	{
		v->SetMember ( FTCHARToUTF8 ( *member ), GFx::Value ( *Translator.GetGfxString() ) );
	}
}

/**
 * Set the text field on this object.
 *
 * @param text        The text to set.
 * @param InContext   The TranslationContext to use when resolving any tags enocuntered in the text.
 */
void UGFxObject::SetText ( const FString& S, UTranslationContext* TranslationContext )
{
	FOutputToGfxPolicy Translator;
	TTranslator::TranslateMarkup ( TranslationContext, S, Translator );

	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		if ( Translator.ShouldShowMarkup() )
		{
			v->SetMember ( FTCHARToUTF8 ( TEXT ( "htmlText" ) ), GFx::Value ( *Translator.GetGfxString() ) );
		}
		else
		{
			v->SetText ( FTCHARToUTF8 ( *Translator.GetGfxString() ) );
		}

	}
}

/**
 * Translate a string for handling markup
 *
 * @param StringToTranslate - The text to set.
 * @param TranslationContext - The TranslationContext to use when resolving any tags enocountered in the text.
 */
FString UGFxObject::TranslateString ( const FString& StringToTranslate, UTranslationContext* TranslationContext )
{
	FOutputToGfxPolicy Translator;
	TTranslator::TranslateMarkup ( TranslationContext, StringToTranslate, Translator );
	return Translator.GetGfxString();
}

void UGFxObject::SetDisplayInfo ( struct FASDisplayInfo D )
{
	GFx::Value::DisplayInfo ginfo;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		UDisplayInfoToGFx ( ginfo, D );
		v->SetDisplayInfo ( ginfo );
	}
}

void UGFxObject::SetPosition ( FLOAT X, FLOAT Y )
{
	GFx::Value::DisplayInfo ginfo;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		ginfo.SetPosition ( X, Y );
		v->SetDisplayInfo ( ginfo );
	}
}

void UGFxObject::SetDisplayMatrix ( const FMatrix& m4 )
{
	Matrix2F m2;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		ConvertMatrix4To2 ( m2, m4 );
		v->SetDisplayMatrix ( m2 );
	}
}
void UGFxObject::execSetDisplayMatrix ( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT ( FMatrix, M );
	P_FINISH;
	SetDisplayMatrix ( M );
}

void UGFxObject::SetDisplayMatrix3D ( const FMatrix& m4 )
{
	Matrix3F m3;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		ConvertMatrix4To3 ( m3, m4 );
		v->SetMatrix3D ( m3 );
	}
}

void UGFxObject::execSetDisplayMatrix3D ( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT ( FMatrix, M );
	P_FINISH;
	SetDisplayMatrix3D ( M );
}

void UGFxObject::SetVisible ( UBOOL visible )
{
	GFx::Value::DisplayInfo ginfo;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		ginfo.SetVisible ( visible ? 1 : 0 );
		v->SetDisplayInfo ( ginfo );
	}
}

void UGFxObject::SetColorTransform ( struct FASColorTransform cxfin )
{
	Cxform cxform;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsDisplayObject() )
	{
		cxform.M[0][0] = cxfin.Multiply.R;
		cxform.M[0][1] = cxfin.Multiply.G;
		cxform.M[0][2] = cxfin.Multiply.B;
		cxform.M[0][3] = cxfin.Multiply.A;
		cxform.M[1][0] = cxfin.Add.R;
		cxform.M[1][1] = cxfin.Add.G;
		cxform.M[1][2] = cxfin.Add.B;
		cxform.M[1][3] = cxfin.Add.A;
		v->SetColorTransform ( cxform );
	}
}

UGFxObject* UGFxObject::CreateEmptyMovieClip ( const FString& instancename, INT Depth, class UClass* Type )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	GFx::Value gresult;
	if ( v->IsDisplayObject() && v->CreateEmptyMovieClip ( &gresult, FTCHARToUTF8 ( *instancename ), Depth ) )
	{
		return ( ( UGFxMoviePlayer* ) GetOuter() )->CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
	}
	return 0;
}

UGFxObject* UGFxObject::AttachMovie ( const FString& symbolname, const FString& instancename, INT Depth, class UClass* Type )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	GFx::Value gresult;
	if ( v->IsDisplayObject() && v->AttachMovie ( &gresult, FTCHARToUTF8 ( *symbolname ), FTCHARToUTF8 ( *instancename ), Depth ) )
	{
		return ( ( UGFxMoviePlayer* ) GetOuter() )->CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
	}
	return 0;
}

class UGFxObject* UGFxObject::GetElementObject ( INT I, class UClass* Type )
{
		GFx::Value *v = ( GFx::Value* ) Value;
		GFx::Value gresult;
		if ( v->IsArray() && v->GetElement ( I, &gresult ) )
		{
			return ( ( UGFxMoviePlayer* ) GetOuter() )->CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
		}
		return 0;
}

void UGFxObject::SetElementObject ( INT I, class UGFxObject* inval )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		if ( inval )
		{
			v->SetElement ( I, * ( GFx::Value* ) inval->Value );
		}
		else
		{
			GFx::Value undef;
			v->SetElement ( I, undef );
		}
	}
}

struct FASValue UGFxObject::GetElement ( INT Index )
{
	FASValue result;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value gresult;
		if ( v->GetElement ( Index, &gresult ) )
		{
            ConvertGFxValueToFASValue( result, gresult );
		}
	}
	return result;
}

UBOOL UGFxObject::GetElementBool ( INT Index )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value gresult ( GFx::Value::VT_ConvertBoolean );
		if ( v->GetElement ( Index, &gresult ) )
		{
			if ( gresult.GetType() != GFx::Value::VT_Boolean )
			{
				warnf ( TEXT ( "UGFxMoviePlayer::GetElementBool(): value is not a bool. Returning FALSE." ) );
				return FALSE;
			}
			else
			{
				return gresult.GetBool();
			}
		}
	}
	return 0;
}

FLOAT UGFxObject::GetElementFloat ( INT Index )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value gresult ( GFx::Value::VT_ConvertNumber );
		if ( v->GetElement ( Index, &gresult ) )
		{
			if ( gresult.GetType() != GFx::Value::VT_Number )
			{
				warnf ( TEXT ( "UGFxMoviePlayer::GetElementFloat(): value is not a float. Returning 0." ) );
				return 0;
			}
			else
			{
				return gresult.GetNumber();
			}
		}
	}
	return 0;
}

INT UGFxObject::GetElementInt ( INT Index )
{
    GFx::Value *v = ( GFx::Value* ) Value;
    if ( v->IsArray() )
    {
        GFx::Value gresult ( GFx::Value::VT_ConvertInt );
        if ( v->GetElement ( Index, &gresult ) )
        {
            if ( gresult.GetType() != GFx::Value::VT_Int )
            {
                warnf ( TEXT ( "UGFxMoviePlayer::GetElementInt(): value is not a number. Returning 0." ) );
                return 0;
            }
            else
            {
                return gresult.GetInt();
            }
        }
    }
    return 0;
}

FString UGFxObject::GetElementString ( INT Index )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
#if TCHAR_IS_1_BYTE
		GFx::Value gresult ( GFx::Value::VT_ConvertString );
		if ( v->GetElement ( Index, &gresult ) )
		{
#else
		GFx::Value gresult ( GFx::Value::VT_ConvertStringW );
		if ( v->GetElement ( Index, &gresult ) )
		{
			if ( gresult.GetType() == GFx::Value::VT_StringW )
			{
				return gresult.GetStringW();
			}
			else
#endif
				if ( gresult.GetType() == GFx::Value::VT_String )
			{
				return FString ( FUTF8ToTCHAR ( gresult.GetString() ) );
			}
		}
	}
	return FString();
}

void UGFxObject::SetElement ( INT Index, struct FASValue arg )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value garg;
		switch ( arg.Type )
		{
			case AS_Null:
				garg.SetNull();
				break;
			case AS_Number:
				garg.SetNumber ( arg.N );
				break;
            case AS_Int:
                garg.SetInt ( arg.I );
                break;
			case AS_Boolean:
				garg.SetBoolean ( arg.B );
				break;
			case AS_String:
#if TCHAR_IS_1_BYTE
				garg.SetString ( *arg.S );
#else
				garg.SetStringW ( *arg.S );
#endif
				break;
			default:
				garg.SetUndefined();
				break;
		}
		v->SetElement ( Index, garg );
	}
}

void UGFxObject::SetElementBool ( INT Index, UBOOL B )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value arg;
		arg.SetBoolean ( B ? 1 : 0 );
		v->SetElement ( Index, arg );
	}
}

void UGFxObject::SetElementFloat ( INT Index, FLOAT F )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		v->SetElement ( Index, GFx::Value ( F ) );
	}
}

void UGFxObject::SetElementInt ( INT Index, INT I )
{
    GFx::Value *v = ( GFx::Value* ) Value;
    if ( v->IsArray() )
    {
        GFx::Value val;
        val.SetInt ( I );
        v->SetElement ( Index, val );
    }
}

void UGFxObject::SetElementString ( INT Index, const FString& S )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		v->SetElement ( Index, GFx::Value ( *S ) );
	}
}

void UGFxObject::SetElementVisible ( INT Index, UBOOL Visible )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			GFx::Value::DisplayInfo ginfo;
			ginfo.SetVisible ( Visible ? 1 : 0 );
			elem.SetDisplayInfo ( ginfo );
		}
	}
}

void UGFxObject::SetElementPosition ( INT Index, FLOAT X, FLOAT Y )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			GFx::Value::DisplayInfo ginfo;
			ginfo.SetPosition ( X, Y );
			elem.SetDisplayInfo ( ginfo );
		}
	}
}

void UGFxObject::SetElementColorTransform ( INT Index, FASColorTransform cxfin )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			Cxform cxform;
			cxform.M[0][0] = cxfin.Multiply.R;
			cxform.M[0][1] = cxfin.Multiply.G;
			cxform.M[0][2] = cxfin.Multiply.B;
			cxform.M[0][3] = cxfin.Multiply.A;
			cxform.M[1][0] = cxfin.Add.R;
			cxform.M[1][1] = cxfin.Add.G;
			cxform.M[1][2] = cxfin.Add.B;
			cxform.M[1][3] = cxfin.Add.A;
			elem.SetColorTransform ( cxform );
		}
	}
}

struct FASValue UGFxObject::GetElementMember ( INT Index, const FString& member )
{
	FASValue result;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem, gresult;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() && elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult ) )
		{
            ConvertGFxValueToFASValue( result, gresult );
		}
	}
	return result;
}

class UGFxObject* UGFxObject::GetElementMemberObject ( INT Index, const FString& member, class UClass* Type )
{
		GFx::Value *v = ( GFx::Value* ) Value;
		if ( v->IsArray() )
		{
			GFx::Value elem, gresult;
			if ( v->GetElement ( Index, &elem ) && elem.IsObject() && elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult ) )
			{
				return ( ( UGFxMoviePlayer* ) GetOuter() )->CreateValueAddRef ( &gresult, Type ? Type : UGFxObject::StaticClass() );
			}
		}
		return 0;
}

UBOOL UGFxObject::GetElementMemberBool ( INT Index, const FString& member )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			GFx::Value gresult ( GFx::Value::VT_ConvertBoolean );
			elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult );
			if ( gresult.GetType() != GFx::Value::VT_Boolean )
			{
				warnf ( TEXT ( "UGFxMoviePlayer::GetElementMemberBool(): value is not a bool. Returning FALSE." ) );
				return FALSE;
			}
			else
			{
				return gresult.GetBool();
			}
		}
	}
	return 0;
}

FLOAT UGFxObject::GetElementMemberFloat ( INT Index, const FString& member )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			GFx::Value gresult ( GFx::Value::VT_ConvertNumber );
			elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult );
			if ( gresult.GetType() != GFx::Value::VT_Number )
			{
				warnf ( TEXT ( "UGFxMoviePlayer::GetElementMemberFloat(): value is not a float. Returning 0.0." ) );
				return 0;
			}
			else
			{
				return gresult.GetNumber();
			}
		}
	}
	return 0;
}
INT UGFxObject::GetElementMemberInt ( INT Index, const FString& member )
{
    GFx::Value *v = ( GFx::Value* ) Value;
    if ( v->IsArray() )
    {
        GFx::Value elem;
        if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
        {
            GFx::Value gresult ( GFx::Value::VT_ConvertInt );
            elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult );
            if ( gresult.GetType() != GFx::Value::VT_Int )
            {
                warnf ( TEXT ( "UGFxMoviePlayer::GetElementMemberInt(): value is not an integer. Returning 0." ) );
                return 0;
            }
            else
            {
                return gresult.GetInt();
            }
        }
    }
    return 0;
}
FString UGFxObject::GetElementMemberString ( INT Index, const FString& member )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
#if TCHAR_IS_1_BYTE
			GFx::Value gresult ( GFx::Value::VT_ConvertString );
			elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult );
#else
			GFx::Value gresult ( GFx::Value::VT_ConvertStringW );
			elem.GetMember ( FTCHARToUTF8 ( *member ), &gresult );
			if ( gresult.GetType() == GFx::Value::VT_StringW )
			{
				return gresult.GetStringW();
			}
			else
#endif
				if ( gresult.GetType() == GFx::Value::VT_String )
			{
				return FString ( FUTF8ToTCHAR ( gresult.GetString() ) );
			}
		}
	}
	return FString();
}

struct FASDisplayInfo UGFxObject::GetElementDisplayInfo ( INT Index )
{
	FASDisplayInfo dinfo;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			GFx::Value::DisplayInfo ginfo;
			elem.GetDisplayInfo ( &ginfo );
			GFxDisplayInfoToU ( dinfo, ginfo );
		}
	}
	return dinfo;
}

FMatrix UGFxObject::GetElementDisplayMatrix ( INT Index )
{
	FMatrix m4;
	Matrix2F m2;
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			elem.GetDisplayMatrix ( &m2 );
			ConvertMatrix2To4 ( m4, m2 );
		}
	}
	return m4;
}

void UGFxObject::SetElementMember ( INT Index, const FString& member, struct FASValue arg )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			GFx::Value garg;
            ConvertFASValueToGFxValue( garg, arg );
			elem.SetMember ( FTCHARToUTF8 ( *member ), garg );
		}
	}
}

void UGFxObject::SetElementMemberObject ( INT Index, const FString& member, class UGFxObject* inval )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			if ( inval )
			{
				elem.SetMember ( FTCHARToUTF8 ( *member ), * ( GFx::Value* ) inval->Value );
			}
			else
			{
				GFx::Value undef;
				elem.SetMember ( FTCHARToUTF8 ( *member ), undef );
			}
		}
	}
}

void UGFxObject::SetElementMemberBool ( INT Index, const FString& member, UBOOL B )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			GFx::Value arg;
			arg.SetBoolean ( B ? 1 : 0 );
			elem.SetMember ( FTCHARToUTF8 ( *member ), arg );
		}
	}
}

void UGFxObject::SetElementMemberFloat ( INT Index, const FString& member, FLOAT F )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			elem.SetMember ( FTCHARToUTF8 ( *member ), GFx::Value ( F ) );
		}
	}
}

void UGFxObject::SetElementMemberInt ( INT Index, const FString& member, INT I )
{
    GFx::Value *v = ( GFx::Value* ) Value;
    if ( v->IsArray() )
    {
        GFx::Value elem;
        if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
        {
            GFx::Value val;
            val.SetInt ( I );
            elem.SetMember ( FTCHARToUTF8 ( *member ), val );
        }
    }
}

void UGFxObject::SetElementMemberString ( INT Index, const FString& member, const FString& S )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsObject() )
		{
			elem.SetMember ( FTCHARToUTF8 ( *member ), GFx::Value ( *S ) );
		}
	}
}

void UGFxObject::SetElementDisplayInfo ( INT Index, struct FASDisplayInfo D )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			GFx::Value::DisplayInfo ginfo;
			UDisplayInfoToGFx ( ginfo, D );
			elem.SetDisplayInfo ( ginfo );
		}
	}
}

void UGFxObject::SetElementDisplayMatrix ( INT Index, const FMatrix& m4 )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsArray() )
	{
		GFx::Value elem;
		if ( v->GetElement ( Index, &elem ) && elem.IsDisplayObject() )
		{
			Matrix2F m2;
			ConvertMatrix4To2 ( m2, m4 );
			elem.SetDisplayMatrix ( m2 );
		}
	}
}
void UGFxObject::execSetElementDisplayMatrix ( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT ( Index );
	P_GET_STRUCT ( FMatrix, M );
	P_FINISH;
	SetElementDisplayMatrix ( Index, M );
}

void UGFxObject::execActionScriptVoid ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult;
	ExecuteActionScript ( reinterpret_cast<GFx::Value*> ( this->Value ),
	                      Cast<UGFxMoviePlayer> ( this->GetOuter() ), CallResult, Stack, Result, FunctionIsVoid );
}

void UGFxObject::execActionScriptInt ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult ( GFx::Value::VT_ConvertInt );
	ExecuteActionScript ( reinterpret_cast<GFx::Value*> ( this->Value ),
	                      Cast<UGFxMoviePlayer> ( this->GetOuter() ), CallResult, Stack, Result );
}

void UGFxObject::execActionScriptFloat ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult ( GFx::Value::VT_ConvertNumber );
	ExecuteActionScript ( reinterpret_cast<GFx::Value*> ( this->Value ),
	                      Cast<UGFxMoviePlayer> ( this->GetOuter() ), CallResult, Stack, Result );
}

void UGFxObject::execActionScriptString ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult ( GFx::Value::VT_ConvertStringW );
	ExecuteActionScript ( reinterpret_cast<GFx::Value*> ( this->Value ),
	                      Cast<UGFxMoviePlayer> ( this->GetOuter() ), CallResult, Stack, Result );
}

void UGFxObject::execActionScriptObject ( FFrame& Stack, RESULT_DECL )
{
	GFx::Value CallResult;
	ExecuteActionScript ( reinterpret_cast<GFx::Value*> ( this->Value ),
	                      Cast<UGFxMoviePlayer> ( this->GetOuter() ), CallResult, Stack, Result );
}

void UGFxObject::execActionScriptArray ( FFrame& Stack, RESULT_DECL )
{
	execActionScriptObject ( Stack, Result );
}


class FGFxDelegateCallback : public FunctionHandler
{
		FScriptDelegate Delegate;

	public:
		FGFxDelegateCallback ( const FScriptDelegate& InDelegate ) : Delegate ( InDelegate )
		{
		}

		~FGFxDelegateCallback()
		{
		}

		virtual void Call ( const Params& params )
		{
			if ( Delegate.Object && Delegate.Object->HasAnyFlags ( RF_Unreachable ) && Delegate.Object->IsPendingKill() )
			{
				Delegate.Object = NULL;
				Delegate.FunctionName = NAME_None;
				return;
			}

			UGFxMoviePlayer *Movie = NULL;
			if ( params.pMovie->GetUserData() )
			{
				Movie = ( UGFxMoviePlayer* ) params.pMovie->GetUserData();
			}
			if ( Movie == NULL )
			{
				return;
			}

			UObject *Object = Delegate.Object;
			if ( Object == NULL )
			{
				Object = Movie;
			}

			UFunction* Function = Object->FindFunction ( Delegate.FunctionName );
			if ( !Function )
			{
				return;
			}

			BYTE* puargs = ( BYTE* ) appAlloca ( Function->ParmsSize );
			appMemzero ( puargs, Function->ParmsSize );

			UINT i = 0;
			for ( TFieldIterator<UProperty> It ( Function ); i < params.ArgCount && It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It, ++i )
			{
				FGFxEngine::ConvertGFxToUProp ( *It, puargs + It->Offset, params.pArgs[i], Movie );
			}

			Object->ProcessEvent ( Function, puargs );

			UProperty *retprop = Function->GetReturnProperty();
			if ( retprop )
			{
				FGFxEngine::ConvertUPropToGFx ( retprop, puargs + Function->ReturnValueOffset, *params.pRetVal, params.pMovie );
			}

			for ( TFieldIterator<UProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
			{
				It->DestroyValue ( puargs + It->Offset );
			}
		}
};

void UGFxObject::SetFunction ( const FString& member, class UObject* context, FName fname )
{
	GFx::Value *v = ( GFx::Value* ) Value;
	if ( v->IsObject() && fname != NAME_None && context )
	{
		Movie* pmovie = ( ( UGFxMoviePlayer* ) GetOuter() )->pMovie->pView;
		FScriptDelegate Delegate;
		Delegate.Object = context;
		Delegate.FunctionName = fname;
		Ptr<FGFxDelegateCallback> Callback = *new FGFxDelegateCallback ( Delegate );
		GFx::Value func;
		pmovie->CreateFunction ( &func, Callback, NULL );
		v->SetMember ( FTCHARToUTF8 ( *member ), func );
	}
}

void UGFxObject::execActionScriptSetFunction ( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR ( member );
	P_FINISH;

	UFunction* Function = Cast<UFunction> ( Stack.Node );
	if ( Function == NULL || ! ( ( GFx::Value* ) Value )->IsObject() )
	{
		return;
	}

	UProperty* DelegateProp = NULL;
	for ( TFieldIterator<UDelegateProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
	{
		DelegateProp = *It;
		break;
	}

	if ( DelegateProp )
	{
		Movie* pmovie = ( ( UGFxMoviePlayer* ) GetOuter() )->pMovie->pView;
		FScriptDelegate* Delegate = ( FScriptDelegate* ) ( Stack.Locals + DelegateProp->Offset );
		if ( Delegate->FunctionName != NAME_None )
		{
			Ptr<FGFxDelegateCallback> Callback = *SF_NEW FGFxDelegateCallback ( *Delegate );
			GFx::Value func;
			pmovie->CreateFunction ( &func, Callback, NULL );
			( ( GFx::Value* ) Value )->SetMember ( FTCHARToUTF8 ( *member ), func );
		}
	}
}

void UGFxObject::execActionScriptSetFunctionOn ( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT ( UGFxObject, Obj );
	P_GET_STR ( member );
	P_FINISH;

	UFunction* Function = Cast<UFunction> ( Stack.Node );
	if ( Function == NULL || ! ( ( GFx::Value* ) Obj->Value )->IsObject() )
	{
		return;
	}

	UProperty* DelegateProp = NULL;
	for ( TFieldIterator<UDelegateProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
	{
		DelegateProp = *It;
		break;
	}

	if ( DelegateProp )
	{
		Movie* pmovie = ( ( UGFxMoviePlayer* ) GetOuter() )->pMovie->pView;
		FScriptDelegate* Delegate = ( FScriptDelegate* ) ( Stack.Locals + DelegateProp->Offset );
		if ( Delegate->FunctionName != NAME_None )
		{
			Ptr<FGFxDelegateCallback> Callback = *SF_NEW FGFxDelegateCallback ( *Delegate );
			GFx::Value func;
			pmovie->CreateFunction ( &func, Callback, NULL );
			( ( GFx::Value* ) Obj->Value )->SetMember ( FTCHARToUTF8 ( *member ), func );
		}
	}
}

void UGFxMoviePlayer::execActionScriptSetFunction ( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT ( UGFxObject, Obj );
	P_GET_STR ( member );
	P_FINISH;

	UFunction* Function = Cast<UFunction> ( Stack.Node );
	if ( Obj == NULL || Function == NULL || ! ( ( GFx::Value* ) Obj->Value )->IsObject() )
	{
		return;
	}

	UProperty* DelegateProp = NULL;
	for ( TFieldIterator<UDelegateProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
	{
		DelegateProp = *It;
		break;
	}

	if ( DelegateProp )
	{
		FScriptDelegate* Delegate = ( FScriptDelegate* ) ( Stack.Locals + DelegateProp->Offset );
		if ( Delegate->FunctionName != NAME_None )
		{
			Ptr<FGFxDelegateCallback> Callback = *SF_NEW FGFxDelegateCallback ( *Delegate );
			GFx::Value func;
			pMovie->pView->CreateFunction ( &func, Callback, NULL );
			( ( GFx::Value* ) Obj->Value )->SetMember ( FTCHARToUTF8 ( *member ), func );
		}
	}
}

/**
 * Translate any weak references to SwfMovies into real UE3 refs.
 */
void USwfMovie::PostLoad()
{
	for ( TArrayNoInit<FString>::TConstIterator SwfRefIt ( ReferencedSwfs ); SwfRefIt; ++SwfRefIt )
	{
		UObject* SwfRefAsObj = StaticLoadObject ( USwfMovie::StaticClass(), NULL, * ( *SwfRefIt ), NULL, LOAD_NoWarn | LOAD_Quiet, NULL, FALSE );
		USwfMovie* SwfRef = Cast<USwfMovie> ( SwfRefAsObj );
		if ( SwfRef == NULL )
		{
			if ( GIsUCC )
			{
				warnf ( NAME_Warning, TEXT ( "PostLoading %s is unable to resolve Swf reference to %s ." ), * ( this->GetFullName() ), * ( *SwfRefIt ) );
			}
		}
		else
		{
			this->References.AddUniqueItem ( SwfRef );
		}
	}

	Super::PostLoad();
}

#if 0

class FGFxASUFunction : public GFxFunctionContext
{
	public:
		virtual void Call ( const Params& params )
		{
			UGFxMoviePlayer *Movie = ( UGFxMoviePlayer* ) params.pMovie->GetUserData();

			GFx::Value thisIndex;
			if ( Movie && params.pThis->GetMember ( "__obj", &thisIndex ) && thisIndex.GetType() == GFx::Value::VT_Number )
			{
				int Index = thisIndex.GetNumber();
				UObject** ObjP = Movie->ASUObjects.Find ( Index );
				if ( ObjP )
				{
					UFunction* Function = ( UFunction* ) params.pUserData;
					UObject* Obj = *ObjP;
					UClass* Class = Cast<UClass> ( Function->GetOuter() );
					if ( Class && Obj->GetClass()->IsChildOf ( Class ) )
					{
						BYTE* puargs = ( BYTE* ) appAlloca ( Function->ParmsSize );
						appMemzero ( puargs, Function->ParmsSize );

						UINT i = 0;
						for ( TFieldIterator<UProperty> It ( Function ); i < params.ArgCount && It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It, ++i )
						{
							FGFxEngine::ConvertGFxToUProp ( *It, puargs + It->Offset, params.pArgs[i], Movie );
						}

						Obj->ProcessEvent ( Function, puargs );

						UProperty *retprop = Function->GetReturnProperty();
						if ( retprop )
						{
							FGFxEngine::ConvertUPropToGFx ( retprop, puargs + Function->ReturnValueOffset, *params.pRetVal, Movie->pMovie->pView );
						}

						for ( TFieldIterator<UProperty> It ( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
						{
							It->DestroyValue ( puargs + It->Offset );
						}
					}
				}
			}
		}
};
static FGFxASUFunction UFunction_Call;

UBOOL UGFxMoviePlayer::GetPrototype ( UClass* Class, void* InProto )
{
	if ( !pMovie )
	{
		return 0;
	}

	GFx::Value* Proto = ( GFx::Value* ) InProto;
	void** proto = ASUClasses.Find ( Class );
	if ( proto )
	{
		Proto = * ( GFx::Value** ) proto;
		return 1;
	}
	pMovie->pView->CreateObject ( Proto );
	for ( TFieldIterator<UFunction, 0> It ( Class ); It; ++It )
	{
		GFx::Value func;
		pMovie->pView->CreateFunction ( &func, &UFunction_Call, ( UFunction* ) *It );
		Proto->SetMember ( FTCHARToUTF8 ( *It->GetName() ), func );
	}
	ASUClasses.Set ( Class, new GFx::Value ( *Proto ) );
	return 1;
}

UGFxObject* UGFxMoviePlayer::ExportObject ( UObject* InObj, UClass* Type )
{
	GFx::Value proto;
	if ( !GetPrototype ( InObj->GetClass(), &proto ) )
	{
		return NULL;
	}

	int Index = ++NextASUObject;
	ASUObjects.Set ( Index, InObj );

	GFx::Value gobj;
	pMovie->pView->CreateObject ( &gobj );
	gobj.SetMember ( "prototype", proto );
	gobj.SetMember ( "__obj", GFx::Value ( ( Double ) Index ) );

	UGFxObject* Result = ConstructObject<UGFxObject> ( Type, this );
	new ( Result->Value ) GFx::Value ( gobj );
	return Result;
}

#endif // 0


#else // WITH_GFx = 0


IMPLEMENT_CLASS ( UGFxMoviePlayer );
IMPLEMENT_CLASS ( UGFxObject );
IMPLEMENT_CLASS ( UGFxFSCmdHandler );
IMPLEMENT_CLASS ( UGFxRawData );
IMPLEMENT_CLASS ( USwfMovie );
IMPLEMENT_CLASS ( UFlashMovie );

void UGFxRawData::SetRawData ( const BYTE *data, UINT size ) {}

void UGFxMoviePlayer::Cleanup() {}

void UGFxMoviePlayer::FinishDestroy()
{
	Super::FinishDestroy();
}

UGFxMoviePlayer::UGFxMoviePlayer() { }

void UGFxMoviePlayer::Serialize ( FArchive& Ar )
{
	Super::Serialize ( Ar );
}

UBOOL UGFxMoviePlayer::Load ( const FString& filename, UBOOL InitFirstFrame )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::Start ( UBOOL bStartPaused )
{
	return FALSE;
}

void UGFxMoviePlayer::Close ( UBOOL unload ) {}

void UGFxMoviePlayer::SetMovieCanReceiveFocus ( UBOOL bCanReceiveFocus ) {}

void UGFxMoviePlayer::SetMovieCanReceiveInput ( UBOOL bCanReceiveInput ) {}

void UGFxMoviePlayer::SetPause ( UBOOL bPausePlayback ) {}

void UGFxMoviePlayer::SetTimingMode ( BYTE Mode ) {}

void UGFxMoviePlayer::ClearCaptureKeys() {}

void UGFxMoviePlayer::AddCaptureKey ( FName ukey ) {}

void UGFxMoviePlayer::ClearFocusIgnoreKeys() {}

void UGFxMoviePlayer::AddFocusIgnoreKey ( FName ukey ) {}

void UGFxMoviePlayer::FlushPlayerInput ( UBOOL CaptureKeysOnly ) {}

UGameViewportClient *UGFxMoviePlayer::GetGameViewportClient()
{
	return NULL;
}

void UGFxMoviePlayer::SetViewport ( INT X, INT Y, INT Width, INT Height ) {}

void UGFxMoviePlayer::SetViewScaleMode ( BYTE scale ) {}

void UGFxMoviePlayer::SetAlignment ( BYTE align ) {}

void UGFxMoviePlayer::GetVisibleFrameRect ( FLOAT& x0, FLOAT& y0, FLOAT& x1, FLOAT& y1 ) {}

void UGFxMoviePlayer::SetView3D ( const FMatrix &matView ) {}

void UGFxMoviePlayer::SetPerspective3D ( const FMatrix &matPersp ) {}

UBOOL UGFxMoviePlayer::SetExternalTexture ( const FString& Name, class UTexture* Texture )
{
	return FALSE;
}

FASValue UGFxMoviePlayer::Invoke ( const FString &method, const TArray<struct FASValue> &args )
{
	FASValue result;
	result.Type = AS_Null;
	return result;
}

void UGFxMoviePlayer::SetWidgetPathBinding ( UGFxObject* WidgetToBind, FName Path ) {}

int UGFxMoviePlayer::GetAVMVersion ()
{
	return 0;
}

UGFxObject* UGFxMoviePlayer::CreateValue ( const void* gfxvalue, UClass* Type )
{
	return NULL;
}

UGFxObject* UGFxMoviePlayer::CreateValueAddRef ( const void* gfxvalue, UClass* Type )
{
	return NULL;
}

void UGFxObject::SetValue ( void* gfxvalue ) {}

UGFxObject* UGFxMoviePlayer::GetVariableObject ( const FString& Path, class UClass* Type )
{
	return NULL;
}

UGFxObject* UGFxMoviePlayer::CreateObject ( const FString& ASClass, class UClass* Type, const TArray<FASValue>& args )
{
	return NULL;
}

UGFxObject* UGFxMoviePlayer::CreateArray()
{
	return NULL;
}

void UGFxMoviePlayer::SetVariableObject ( const FString& Path, UGFxObject* Value ) {}

FASValue UGFxMoviePlayer::GetVariable ( const FString &path )
{
	FASValue result;
	result.Type = AS_Null;
	return result;
}

UBOOL UGFxMoviePlayer::GetVariableBool ( const FString &path )
{
	return FALSE;
}

FLOAT UGFxMoviePlayer::GetVariableNumber ( const FString &path )
{
	return 0.0f;
}
INT UGFxMoviePlayer::GetVariableInt ( const FString &path )
{
	return 0;
}
FString UGFxMoviePlayer::GetVariableString ( const FString &path )
{
	return FString();
}

void UGFxMoviePlayer::SetVariable ( const FString &path, FASValue arg ) { }

void UGFxMoviePlayer::SetVariableBool ( const FString &path, UBOOL b ) { }

void UGFxMoviePlayer::SetVariableNumber ( const FString &path, float f ) { }

void UGFxMoviePlayer::SetVariableInt ( const FString &path, int i ) { }

void UGFxMoviePlayer::SetVariableString ( const FString &path, const FString& s ) { }

UBOOL UGFxMoviePlayer::GetVariableArray ( const FString& path, INT index, TArray<struct FASValue>& arg )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::GetVariableIntArray ( const FString& Path, INT Index, TArray<INT>& arg )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::GetVariableFloatArray ( const FString& Path, INT Index, TArray<FLOAT>& arg )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::GetVariableStringArray ( const FString& path, INT index, TArray<FString>& arg )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::SetVariableArray ( const FString& Path, INT Index, const TArray<struct FASValue>& args )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::SetVariableIntArray ( const FString& Path, INT Index, const TArray<INT>& arg )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::SetVariableFloatArray ( const FString& Path, INT Index, const TArray<FLOAT>& arg )
{
	return FALSE;
}

UBOOL UGFxMoviePlayer::SetVariableStringArray ( const FString& Path, INT Index, const TArray<FString>& arg )
{
	return FALSE;
}

void UGFxMoviePlayer::Advance ( FLOAT time ) { }

void UGFxMoviePlayer::PostAdvance ( FLOAT DeltaTime ) { }

void UGFxMoviePlayer::UpdateSplitscreenLayout() {}

void UGFxMoviePlayer::execActionScriptVoid ( FFrame& Stack, RESULT_DECL ) { }

void UGFxMoviePlayer::execActionScriptInt ( FFrame& Stack, RESULT_DECL ) { }

void UGFxMoviePlayer::execActionScriptFloat ( FFrame& Stack, RESULT_DECL ) { }

void UGFxMoviePlayer::execActionScriptString ( FFrame& Stack, RESULT_DECL ) { }

void UGFxMoviePlayer::execActionScriptObject ( FFrame& Stack, RESULT_DECL ) { }

void UGFxMoviePlayer::execActionScriptConstructor ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::Clear() { }

void UGFxObject::BeginDestroy()
{
	Super::BeginDestroy();
}

struct FASValue UGFxObject::Invoke ( const FString& method, const TArray<struct FASValue>& args )
{
	FASValue result;
	result.Type = AS_Null;
	return result;
}

void UGFxObject::GotoAndPlay ( const FString& frame ) {}

void UGFxObject::GotoAndStop ( const FString& frame ) {}

void UGFxObject::GotoAndPlayI ( INT frame ) {}

void UGFxObject::GotoAndStopI ( INT frame ) {}

struct FASValue UGFxObject::Get ( const FString& member )
{
	FASValue result;
	result.Type = AS_Null;
	return result;
}

class UGFxObject* UGFxObject::GetObject ( const FString& member, class UClass* Type )
{
		return NULL;
}

UBOOL UGFxObject::GetBool ( const FString& member )
{
	return FALSE;
}

FLOAT UGFxObject::GetFloat ( const FString& member )
{
	return 0.0f;
}

INT UGFxObject::GetInt ( const FString& member )
{
	return 0;
}

FString UGFxObject::GetString ( const FString& member )
{
	return FString();
}

FString UGFxObject::GetText()
{
	return FString();
}

struct FASDisplayInfo UGFxObject::GetDisplayInfo()
{
	return FASDisplayInfo();
}

FMatrix UGFxObject::GetDisplayMatrix()
{
	FMatrix m;
	return m;
}

FMatrix UGFxObject::GetDisplayMatrix3D()
{
	return FMatrix();
}

FASColorTransform UGFxObject::GetColorTransform()
{
	return FASColorTransform();
}

UBOOL UGFxObject::GetPosition ( FLOAT& X, FLOAT& Y )
{
	return FALSE;
}

void UGFxObject::Set ( const FString& member, struct FASValue arg ) { }

void UGFxObject::SetObject ( const FString& member, class UGFxObject* inval ) { }

void UGFxObject::SetBool ( const FString& member, UBOOL B ) { }

void UGFxObject::SetFloat ( const FString& member, FLOAT F ) { }

void UGFxObject::SetInt ( const FString& member, INT I ) { }

FString ConvertMarkupString ( const FString& S )
{
	return FString();
}

void UGFxObject::SetString ( const FString& member, const FString& S, UTranslationContext* TranslationContext ) { }

void UGFxObject::SetText ( const FString& S, UTranslationContext* TranslationContext ) { }

FString UGFxObject::TranslateString ( const FString& StringToTranslate, UTranslationContext* TranslationContext )
{
	return FString();
}

void UGFxObject::SetDisplayInfo ( struct FASDisplayInfo D ) { }

void UGFxObject::SetPosition ( FLOAT X, FLOAT Y ) { }

void UGFxObject::SetDisplayMatrix ( const FMatrix& m4 ) { }

void UGFxObject::execSetDisplayMatrix ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::SetDisplayMatrix3D ( const FMatrix& m4 ) { }

void UGFxObject::execSetDisplayMatrix3D ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::SetVisible ( UBOOL visible ) { }

void UGFxObject::SetColorTransform ( struct FASColorTransform cxfin ) { }

UGFxObject* UGFxObject::CreateEmptyMovieClip ( const FString& instancename, INT Depth, class UClass* Type )
{
	return NULL;
}

UGFxObject* UGFxObject::AttachMovie ( const FString& symbolname, const FString& instancename, INT Depth, class UClass* Type )
{
	return NULL;
}

class UGFxObject* UGFxObject::GetElementObject ( INT I, class UClass* Type )
{
		return NULL;
}

void UGFxObject::SetElementObject ( INT I, class UGFxObject* inval ) { }

struct FASValue UGFxObject::GetElement ( INT Index )
{
	FASValue result;
	result.Type = AS_Null;
	return result;
}

UBOOL UGFxObject::GetElementBool ( INT Index )
{
	return FALSE;
}

FLOAT UGFxObject::GetElementFloat ( INT Index )
{
	return 0.0f;
}

INT UGFxObject::GetElementInt ( INT Index )
{
    return 0;
}

FString UGFxObject::GetElementString ( INT Index )
{
	return FString();
}

void UGFxObject::SetElement ( INT Index, struct FASValue arg ) { }

void UGFxObject::SetElementBool ( INT Index, UBOOL B ) { }

void UGFxObject::SetElementFloat ( INT Index, FLOAT F ) { }

void UGFxObject::SetElementInt ( INT Index, INT I ) { }

void UGFxObject::SetElementString ( INT Index, const FString& S ) { }

void UGFxObject::SetElementVisible ( INT Index, UBOOL Visible ) { }

void UGFxObject::SetElementPosition ( INT Index, FLOAT X, FLOAT Y ) { }

void UGFxObject::SetElementColorTransform ( INT Index, FASColorTransform cxfin ) { }

struct FASValue UGFxObject::GetElementMember ( INT Index, const FString& member )
{
	FASValue result;
	result.Type = AS_Null;
	return result;
}

class UGFxObject* UGFxObject::GetElementMemberObject ( INT Index, const FString& member, class UClass* Type )
{
	return NULL;
}

UBOOL UGFxObject::GetElementMemberBool ( INT Index, const FString& member )
{
	return FALSE;
}

FLOAT UGFxObject::GetElementMemberFloat ( INT Index, const FString& member )
{
	return 0.0f;
}

INT UGFxObject::GetElementMemberInt ( INT Index, const FString& member )
{
    return 0;
}

FString UGFxObject::GetElementMemberString ( INT Index, const FString& member )
{
	return FString();
}

struct FASDisplayInfo UGFxObject::GetElementDisplayInfo ( INT Index )
{
	FASDisplayInfo dinfo;
	return dinfo;
}

FMatrix UGFxObject::GetElementDisplayMatrix ( INT Index )
{
	FMatrix m4;
	return m4;
}

void UGFxObject::SetElementMember ( INT Index, const FString& member, struct FASValue arg ) { }

void UGFxObject::SetElementMemberObject ( INT Index, const FString& member, class UGFxObject* inval ) { }

void UGFxObject::SetElementMemberBool ( INT Index, const FString& member, UBOOL B ) { }

void UGFxObject::SetElementMemberFloat ( INT Index, const FString& member, FLOAT F ) { }

void UGFxObject::SetElementMemberInt ( INT Index, const FString& member, INT I ) { }

void UGFxObject::SetElementMemberString ( INT Index, const FString& member, const FString& S ) { }

void UGFxObject::SetElementDisplayInfo ( INT Index, struct FASDisplayInfo D ) { }

void UGFxObject::SetElementDisplayMatrix ( INT Index, const FMatrix& m4 ) { }

void UGFxObject::execSetElementDisplayMatrix ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptVoid ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptInt ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptFloat ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptString ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptObject ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptArray ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::SetFunction ( const FString& member, class UObject* context, FName fname ) { }

void UGFxObject::execActionScriptSetFunction ( FFrame& Stack, RESULT_DECL ) { }

void UGFxObject::execActionScriptSetFunctionOn ( FFrame& Stack, RESULT_DECL ) { }

void UGFxMoviePlayer::execActionScriptSetFunction ( FFrame& Stack, RESULT_DECL ) { }

void USwfMovie::PostLoad()
{
	Super::PostLoad();
}

#endif // WITH_GFx
