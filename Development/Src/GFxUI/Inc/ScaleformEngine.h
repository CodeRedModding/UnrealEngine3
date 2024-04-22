/**********************************************************************

Filename    :   ScaleformEngine.h
Content     :   Movie manager / GFxMovieView access wrapper for UE3

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#ifndef GFxEngine_h
#define GFxEngine_h

#if WITH_GFx

class FViewport;
class FGFxImageCreateInfo;
class FGFxImageCreator;
class UGFxInteraction;
class UGFxMoviePlayer;
struct FGFxMovieRenderParams;

#include "GFxUIClasses.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_Types.h"
#include "Kernel/SF_RefCount.h"
#include "Render/Render_HAL.h"
#include "Render/Renderer2D.h"
#include "GFx/GFx_Player.h"
#include "Kernel/SF_File.h"
#include "GFx/GFx_Loader.h"
#include "GFx/GFx_FontLib.h"

#if WITH_GFx_IME
#if defined(WIN32)
#include "GFx/IME/GFx_IMEManager.h"
#else
#undef WITH_GFx_IME
#endif
#endif

#if WITH_GFx_AUDIO
#include <fmod.hpp>
#include "Sound/Sound_SoundRendererFMOD.h"
#endif

#if WITH_GFx_VIDEO
#include "Video/Video_Video.h"
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#if !defined(GFX_AS_ENABLE_GFXVALUE_CLEANUP)
#error Must build GFx with GFX_AS_ENABLE_GFXVALUE_CLEANUP
#endif

namespace Scaleform
{
namespace Render
{
namespace RHI
{
class HAL;
}
}
}

using namespace Scaleform;

static const float                           DEFAULT_CURVE_ERROR = 2.0f;
//static const GFxRenderConfig::RenderFlagType DEFAULT_STROKE_TYPE = GFxRenderConfig::RF_StrokeNormal;

#define PACKAGE_PATH     "/ package/"
#define PACKAGE_PATH_U   TEXT("/ package/")
#define PACKAGE_PATH_LEN 10
#define GAMEDIR_PATH     "gamedir://"
#define GAMEDIR_PATH_LEN 10

inline const char *IsPackagePath ( const char *ppath )
{
	if ( !strncmp ( ppath, PACKAGE_PATH, PACKAGE_PATH_LEN ) )
	{
		return ppath + PACKAGE_PATH_LEN;
	}
	else
	{
		return NULL;
	}
}

/**
* Info re: the movie which was loaded.
*/
class FGFxMovie
{
	public:
		FString             FileName;
		GFx::MovieInfo      Info;
		Ptr<GFx::MovieDef>  pDef;
		Ptr<GFx::Movie>     pView;
		GFx::MovieDisplayHandle DispHandle;
		DOUBLE              LastTime;
		UBOOL               Playing;
		UBOOL		        fVisible;
		UBOOL		        fUpdate;
		UBOOL               fViewportSet;
		UBOOL				bCanReceiveFocus;
		UBOOL               bCanReceiveInput;
		INT                 TimingMode;
		UGFxMoviePlayer*          pUMovie;
		UTextureRenderTarget2D* pRenderTexture;
		Render::RenderTarget**  GFxRenderTarget; // Render thread only

		FRenderCommandFence RenderCmdFence;
};

struct FGFxLocalPlayerState
{
	class FGFxMovie*	FocusedMovie;
};


//=============================================================================
// FGFxEngine  -- A class responsible for playing scaleform movies as well as
//    managing resources required to play these movies such as the Renderer and
//    file loader
//=============================================================================

class FGFxEngineBase
{
	protected:
		GFx::System System;

	public:
		FGFxEngineBase();
		virtual ~FGFxEngineBase();
};

class FGFxEngine : public FGFxEngineBase
{
		/** Default constructor, allocates the required resources */
		FGFxEngine();

#if WITH_GFx_FULLSCREEN_MOVIE
		friend class FFullScreenMovieGFx;
#endif

	public:
		static const int INVALID_LEVEL_NUM = -1;

		static FGFxEngine* GetEngineNoRender();
		static FGFxEngine* GetEngine();

		/** Virtual destructor for subclassing, cleans up the allocated resources */
		virtual ~FGFxEngine();

		/** Set the viewport for use with HUD (overlay) movies */
		void        SetRenderViewport ( FViewport* inViewport );
		FViewport*  GetRenderViewport()
		{
			return HudViewport;
		}

		GFx::Loader* GetLoader()
		{
			return &mLoader;
		}

		/**
			* Called once a frame to update the played movie's state.
			* @param	DeltaTime - The time since the last frame.
			*/
		void        Tick ( FLOAT DeltaTime );

		/** RenderThread implementation for RenderUI function */
		void		RenderUI_RenderThread(struct FGFxMovieRenderParams* Params);
		/** Render all the scaleform movies that are currently playing */
		void        RenderUI ( UBOOL bRenderToSceneColor, INT DPG = SDPG_Foreground );
		void        RenderTextures();

		/** Get the next DPG that will have a Flash movie over it. */
		INT         GetNextMovieDPG ( INT FirstDPG );

		/** Loads a flash movie, it does not start playing */
		FGFxMovie*  LoadMovie ( const TCHAR* Path, UBOOL fInitFirstFrame = TRUE );

		/** Start playing a flash movie */
		void        StartScene ( FGFxMovie* Movie, UTextureRenderTarget2D* RenderTexture = NULL, UBOOL fVisible = TRUE, UBOOL fUpdate = TRUE );

		/** Stops the playback of the topmost scene on the stack and unloads */
		void        CloseTopmostScene();

		/** Stops the playback of any scene currently playing, cleans up resources allocated to it if requested */
		void        CloseScene ( FGFxMovie* pMovie, UBOOL fDelete );

		/** Stops and unloads all movies (used at end of map) */
		void        NotifyGameSessionEnded();

		/** Delete any movies scheduled for deletion by Close. */
		void        DeleteQueuedMovies ( UBOOL wait = TRUE );

		/** Closes all movies */
		void		CloseAllMovies ( UBOOL bOnlyCloseOnLevelChangeMovies = FALSE );

		/** Closes all texturemovies */
		void		CloseAllTextureMovies();

		FGFxMovie*  GetFocusMovie ( INT ControllerId );

		/**Sets whether or not a movie is allowed to receive focus.  Defaults to true*/
		void SetMovieCanReceiveFocus ( FGFxMovie* InMovie, UBOOL bCanReceiveFocus );
		/**Sets whether or not a movie is allowed to receive input.  Defaults to true*/
		void SetMovieCanReceiveInput ( FGFxMovie* InMovie, UBOOL bCanReceiveInput );

		// Helper function to insert a movie into a list of movies in priority order while removing any existing reference to the same movie
		void InsertMovieIntoList ( FGFxMovie* Movie, TArray<FGFxMovie*>* List );
		// Insert movie into OpenMovies and DPG in priority order
		void InsertMovie ( FGFxMovie* Movie, BYTE DPG );

#if WITH_GFx_IME
		GFx::IMEManagerBase* GetIMEManager()
		{
			return pImeManager;
		}
#endif

		/** Returns the topmost movie from the stack */
		FGFxMovie*    GetTopmostMovie() const;

		/** Returns the number of movies in the open movie list */
		INT GetNumOpenMovies() const;
		/** Returns an open movie from the open movie list at index Idx */
		FGFxMovie* GetOpenMovie ( INT Idx ) const;
		/** Returns the number of all movies */
		INT GetNumAllMovies() const;

		Render::RHI::HAL*   GetRenderHAL()
		{
			return RenderHal.GetPtr();
		}
		Render::Renderer2D* GetRenderer2D()
		{
			return Renderer2D.GetPtr();
		}

		Render::TextureImage*  CreateUTextureImage ( UTexture* Texture );

		GFx::MovieDef*  LoadMovieDef ( const TCHAR* Path, GFx::MovieInfo& Info );

		UBOOL InputKey ( INT ControllerId, FName key, EInputEvent event );
		UBOOL InputChar ( INT ControllerId, TCHAR ch );
		UBOOL InputAxis ( INT ControllerId, FName key, float delta, float DeltaTime, UBOOL bGamepad );
		UBOOL InputTouch ( INT ControllerId, const FIntPoint& Location, ETouchType Event, UINT TouchId );
		void FlushPlayerInput ( TSet<NAME_INDEX>* Keys = NULL );

		/** Replaces characters in an FString with alternate chars. Put here instead of FString for now, to avoid
		messing too much with the engine */
		static inline INT ReplaceCharsInFString ( FString& strInput, TCHAR const * pCharsToReplace, TCHAR const pReplacement );

		// translate GFxLoader path to Unreal path or return false if filesystem path
		static UBOOL GetPackagePath ( const char *ppath, FFilename& pkgpath );

		static inline void ConvertUPropToGFx ( UProperty* up, BYTE* paddr, GFx::Value& val, GFx::Movie* Movie, bool overwrite = false, bool isReturnValue = false );
		static inline void ConvertGFxToUProp ( UProperty* up, BYTE* paddr, const GFx::Value& val, UGFxMoviePlayer* Movie );

		/**
			* Given a relative path that may contain tokens such as /./ and ../, collapse these
			* relative tokens into a an absolute reference.
			*
			* @param InString A relative path that may contain relative tokens.
			*
			* @return A path with all relative tokens collapsed
			*/
		static FFilename CollapseRelativePath ( const FFilename& InString );

		FRenderCommandFence  RenderCmdFence;
#if WITH_GFx_FULLSCREEN_MOVIE
		class FFullScreenMovieGFx* FullscreenMovie;
#endif
		int                 GameHasRendered;

		// Currently playing movies
		TArray<FGFxMovie*> OpenMovies;
		// All movies, including not currently playing (inactive) movies
		TArray<FGFxMovie*> AllMovies;

		//Current GFx State for all Local Players
		TArray<FGFxLocalPlayerState> PlayerStates;

		//Helper function to translate ControllerId's to LocalPlayer Indexes
		INT GetLocalPlayerIndexFromControllerID ( INT ControllerId );

		//Helper function to translate ControllerId's to that controller's focused movie
		FGFxMovie* GetFocusedMovieFromControllerID ( INT ControllerId );

		//Function that determines what the focusable movies should be for all of the local players.
		void ReevaluateFocus();

		//Add a player state struct to the PlayerStates array
		void AddPlayerState();

		//Remove the player state struct from the PlayerStates array at index Index
		void RemovePlayerState ( INT Index );

		//Reevaluate all movies sizes for a potential change
		void ReevaluateSizes();

        //RenderStats
		void UpdateRenderStats();

        //KeyCodes, Scaleform.KeyMap, Scaleform.GamepadMouse, etc.
		void InitKeyMap();
		void InitGamepadMouse();

	private:

		//Reevaluate the specific size for a movie
		void SetMovieSize ( FGFxMovie* Movie );

		/** Disallow copy and assignment */
		FGFxEngine& operator= ( const FGFxEngine& );
		FGFxEngine ( const FGFxEngine& );

		// Used for loading up swf resources
		GFx::Loader        mLoader;

		// Currently playing movies.
		FViewport*         HudViewport;
		Render::RenderTarget* HudRenderTarget;
		Render::RenderTarget* SceneColorRT;

		TArray<FGFxMovie*> DPGOpenMovies[SDPG_PostProcess + 1];
		// Movies to delete at start of next frame
		TArray<FGFxMovie*> DeleteMovies;
		// Movies to render to textures
		TArray<FGFxMovie*> TextureMovies;

		// Implementation of the renderer used by all the scaleform movies.
		Ptr<Render::RHI::HAL>   RenderHal;
		Ptr<Render::Renderer2D> Renderer2D;

		//< Maximum amount of error in curve segments when tessellating.
		float              CurveError;

		// mapping of Unreal keys (and mouse buttons) to GFx keys
		struct UGFxInput
		{
			KeyCode    Key;
			int             MouseButton;

			UGFxInput ( KeyCode k ) : Key ( k ), MouseButton ( -1 ) { }
			UGFxInput ( KeyCode k, int mb ) : Key ( k ), MouseButton ( mb ) { }
		};
		struct UGFxInputAxis
		{
			KeyCode    Key1, Key2;

			UGFxInputAxis ( KeyCode a, KeyCode b ) : Key1 ( a ), Key2 ( b ) { }
		};
		TMap<NAME_INDEX, UGFxInput>                     KeyCodes, KeyMap;
		FName                                           AxisMouseEmulation[2]; // X, Y
		UBOOL                                           AxisMouseInvert[2];
		TMap<FName, struct FUIAxisEmulationDefinition>	AxisEmulationDefinitions;
		FLOAT                                           AxisRepeatDelay;
		FLOAT                                           UIJoystickDeadZone;
		DOUBLE                                          Time;

		FIntPoint                            MousePos;

		// send keyboard input to this movie (mouse goes to all visible movies if any FocusMovie is set)
		//FGFxMovie*          pFocusMovie;
		// if false, pass input through to game even when pFocusMovie is set.
		//UBOOL               CaptureInput;

		// keys to ignore because they were pressed before input capture began
		TMap<INT, TArray<FName> > InitialPressedKeys;

		FUIAxisEmulationData	 AxisInputEmulation[4];
		FName                    AxisInputEmulationLastKey[4];

	private:
		// Handle input for a movie.
		UBOOL InputKey ( INT ControllerId, FGFxMovie* pFocusMovie, FName ukey, EInputEvent uevent );
		inline UBOOL IsKeyCaptured ( FName ukey );

#ifndef GFX_NO_LOCALIZATION
		UBOOL                FontlibInitialized;
		Ptr<GFx::FontLib>    pFontLib;
		Ptr<GFx::FontMap>    pFontMap;
		Ptr<GFx::Translator> pTranslator;

		void InitLocalization();
		void InitFontlib();
		void InitCommonRT ( GFx::Loader& );
#endif

#if WITH_GFx_IME
		Ptr<GFx::IMEManagerBase> pImeManager;
#endif

#if WITH_GFx_AUDIO
        FMOD::System *pSoundDevice;
        Ptr<Sound::SoundRendererFMOD> pSoundRenderer;
        UBOOL InitSound();
        void  ShutdownSound();
#endif

#if WITH_GFx_VIDEO
        Ptr<GFx::Video::Video> pVideo;
        UBOOL InitVideoSound ( GFx::Video::Video* pvideo );
#endif

		static void InitGFxLoaderCommon ( GFx::Loader& );

		void    InitRenderer();

		UGFxInput* GetInputKey ( FName Key )
		{
			return KeyMap.Find ( Key.GetIndex() );
		}

		void AssumeOwnershipOfRenderTargets ( Render::RHI::HAL* RenderHAL );
		void ReleaseOwnershipOfRenderTargets ( Render::RHI::HAL* RenderHAL );
		void DrawPrepass ( const FGFxMovieRenderParams& PrepassParams );

		UBOOL IsRenderingEnabled()
		{
		    return HudViewport ? HudViewport->IsGameRenderingEnabled(): FALSE;
		}

		UBOOL IsHudEnabled()
		{
			ULocalPlayer& Player = *GEngine->GamePlayers(0);

			if ((Player.Actor == NULL) || (Player.Actor->myHUD == NULL) || !Player.Actor->myHUD->bShowHUD)
			{
				return FALSE;
			}

			return TRUE;
		}

#if WITH_REALD
		UBOOL bStereoEnabledLastFrame;
		INT   StereoScreenWidth;
#endif
};

inline INT FGFxEngine::ReplaceCharsInFString ( FString& strInput, TCHAR const * pCharsToReplace, TCHAR const pReplacement )
{
	INT num = 0 ;
	TCHAR* pData = ( TCHAR* ) strInput.GetCharArray().GetTypedData();
	TCHAR const *pChars;
	while ( *pData != TEXT ( '\0' ) )
	{
		pChars = pCharsToReplace;
		while ( *pChars != TEXT ( '\0' ) )
		{
			if ( *pData == *pChars )
			{
				*pData = pReplacement;
				num++ ;
				break;
			}
			++pChars;
		}
		++pData;
	}
	return num;
}

inline void FGFxEngine::ConvertUPropToGFx ( UProperty* up, BYTE* paddr, GFx::Value& val, GFx::Movie* Movie, bool overwrite, bool isReturnValue )
{
	if ( up->ArrayDim > 1 && Movie )
	{
		Movie->CreateArray ( &val );
		for ( int i = 0; i < up->ArrayDim; i++ )
		{
			GFx::Value elem;
			ConvertUPropToGFx ( up, paddr + i * up->ElementSize, elem, NULL );
			val.PushBack ( elem );
		}
	}

	UBoolProperty* upbool = ExactCast<UBoolProperty> ( up );
	if ( upbool )
	{
		INT *TempValue = reinterpret_cast<INT*> ( paddr );

		if (isReturnValue)
		{
			val.SetBoolean(*TempValue == TRUE);
		}
		else
		{
			val.SetBoolean((*TempValue & upbool->BitMask) ? TRUE : FALSE);
		}
	}
	else if ( ExactCast<UArrayProperty> ( up ) && Movie )
	{
		UArrayProperty* Array = ( UArrayProperty* ) up;
		FScriptArray*   Data = ( FScriptArray* ) paddr;
		BYTE* pelements = ( BYTE* ) Data->GetData();

		if ( overwrite )
		{
			if ( !val.IsArray() )
			{
				return;
			}

			for ( int i = 0; i < Data->Num(); i++ )
			{
				GFx::Value elem;
				val.GetElement ( i, &elem );
				ConvertUPropToGFx ( Array->Inner, pelements + i * Array->Inner->ElementSize, elem, Movie, 1 );
				val.SetElement ( i, elem );
			}
		}
		else
		{
			Movie->CreateArray ( &val );

			for ( int i = 0; i < Data->Num(); i++ )
			{
				GFx::Value elem;
				ConvertUPropToGFx ( Array->Inner, pelements + i * Array->Inner->ElementSize, elem, Movie );
				val.PushBack ( elem );
			}
		}
	}
	else if ( ExactCast<UStructProperty> ( up ) && Movie )
	{
		UStructProperty* Struct = ( UStructProperty* ) up;

		if ( overwrite )
		{
			if ( !val.IsObject() )
			{
				return;
			}

			for ( TFieldIterator<UProperty> It ( Struct->Struct ); It; ++It )
			{
				GFx::Value elem;
				FTCHARToUTF8 fname ( *It->GetName() );
				if ( val.GetMember ( fname, &elem ) )
				{
					ConvertUPropToGFx ( *It, paddr + It->Offset, elem, Movie );
				}
				else
				{
					ConvertUPropToGFx ( *It, paddr + It->Offset, elem, Movie );
					val.SetMember ( fname, elem );
				}
			}
		}
		else
		{
			Movie->CreateObject ( &val );

			for ( TFieldIterator<UProperty> It ( Struct->Struct ); It; ++It )
			{
				GFx::Value elem;
				ConvertUPropToGFx ( *It, paddr + It->Offset, elem, Movie );
				val.SetMember ( FTCHARToUTF8 ( *It->GetName() ), elem );
			}
		}
	}
	else
	{
		UPropertyValue v;
		up->GetPropertyValue ( paddr, v );
		if ( ExactCast<UByteProperty> ( up ) )
		{
			val.SetInt ( v.ByteValue );
		}
		else if ( ExactCast<UIntProperty> ( up ) )
		{
			val.SetInt ( v.IntValue );
		}
		else if ( ExactCast<UFloatProperty> ( up ) )
		{
			val.SetNumber ( v.FloatValue );
		}
		else if ( ExactCast<UStrProperty> ( up ) )
		{
#if TCHAR_IS_1_BYTE
			val.SetString ( **v.StringValue );
#else
			val.SetStringW ( **v.StringValue );
#endif
		}
		else if ( Cast<UObjectProperty> ( up ) )
		{
			if (v.ObjectValue == NULL)
			{
				val.SetNull();
			}
			else
			{
				UObjectProperty* objp = Cast<UObjectProperty> ( up );
				if (objp->PropertyClass->IsChildOf ( UGFxObject::StaticClass() ) )
				{
					val = *( ( GFx::Value* ) ( ( UGFxObject* ) v.ObjectValue )->Value );
				}
				else
				{
					val.SetNull();
				}
			}
		}
		else if ( ExactCast<UBoolProperty> ( up ) )
		{
			val.SetBoolean ( v.BoolValue ? 1 : 0 );
		}
		else
		{
			val.SetUndefined();
		}
	}
}

#define CONVERT_GFX_NUMBERS(Type)\
    else if ( val.GetType() == GFx::Value::VT_##Type )      \
    {                                                       \
        if ( ExactCast<UByteProperty> ( up ) )                   \
        {                                                   \
            v.ByteValue = ( BYTE ) val.Get##Type();         \
        }                                                   \
        else if ( ExactCast<UIntProperty> ( up ) )               \
        {                                                   \
            v.IntValue = ( INT ) val.Get##Type();           \
        }                                                   \
        else if ( ExactCast<UFloatProperty> ( up ) )             \
        {                                                   \
            v.FloatValue = ( FLOAT ) val.Get##Type();       \
        }                                                   \
        else                                                \
        {                                                   \
            return;                                         \
        }                                                   \
        up->SetPropertyValue ( paddr, v );                  \
    }


inline void FGFxEngine::ConvertGFxToUProp ( UProperty* up, BYTE* paddr, const GFx::Value& val, UGFxMoviePlayer* Movie )
{
	UPropertyValue v;

	if ( up->ArrayDim > 1 && val.IsArray() )
	{
		unsigned count = up->ArrayDim;
		if ( val.GetArraySize() < count )
		{
			count = val.GetArraySize();
		}
		GFx::Value elem;
		for ( unsigned i = 0; i < count; i++ )
		{
			val.GetElement ( i, &elem );
			ConvertGFxToUProp ( up, paddr + i * up->ElementSize, elem, Movie );
		}
	}
	else if ( val.GetType() == GFx::Value::VT_Boolean && Cast<UBoolProperty> ( up ) )
	{
		v.BoolValue = val.GetBool() ? FIRST_BITFIELD : 0;
		up->SetPropertyValue ( paddr, v );
	}
    CONVERT_GFX_NUMBERS(Number)
    CONVERT_GFX_NUMBERS(Int)
    CONVERT_GFX_NUMBERS(UInt)
	else if ( Cast<UStrProperty> ( up ) )
	{
		if ( val.GetType() == GFx::Value::VT_String )
		{
			FString str ( FUTF8ToTCHAR( val.GetString() ) );
			v.StringValue = &str;
			up->SetPropertyValue ( paddr, v );
		}
#if !TCHAR_IS_1_BYTE
		else if ( val.GetType() == GFx::Value::VT_StringW )
		{
			FString str ( val.GetStringW() );
			v.StringValue = &str;
			up->SetPropertyValue ( paddr, v );
		}
#endif
		else
		{
			FString str;
			v.StringValue = &str;
			up->SetPropertyValue ( paddr, v );
		}
	}
	else if ( Cast<UArrayProperty> ( up ) && val.IsArray() )
	{
		UArrayProperty* Array = ( UArrayProperty* ) up;
		FScriptArray*   Data = ( FScriptArray* ) paddr;
		Data->Empty ( 0, Array->Inner->ElementSize );
		Data->AddZeroed ( val.GetArraySize(), Array->Inner->ElementSize );
		BYTE* pelements = ( BYTE* ) Data->GetData();
		GFx::Value elem;
		for ( unsigned i = 0; i < val.GetArraySize(); i++ )
		{
			val.GetElement ( i, &elem );
			ConvertGFxToUProp ( Array->Inner, pelements + i * Array->Inner->ElementSize, elem, Movie );
		}
	}
	else if ( Cast<UStructProperty> ( up ) && val.IsObject() )
	{
		class ObjVisitor : public GFx::Value::ObjectVisitor
		{
			UGFxMoviePlayer* Movie;
			BYTE*            paddr;
			UStructProperty* Struct;

		public:
			ObjVisitor ( UGFxMoviePlayer* m, BYTE* p, UStructProperty* s ) : Movie ( m ), paddr ( p ), Struct ( s ) {}
			void    Visit ( const char* name, const GFx::Value& val )
			{
				FName fname ( FUTF8ToTCHAR ( name ), FNAME_Find );
				for ( TFieldIterator<UProperty> It ( Struct->Struct ); It; ++It )
				{
					if ( It->GetFName() == fname )
					{
						ConvertGFxToUProp ( *It, paddr + It->Offset, val, Movie );
					}
				}
			}
		};

		FName fname ( TEXT("_this") );
		for ( TFieldIterator<UProperty> It ( (( UStructProperty* ) up)->Struct ); It; ++It )
		{
			if ( It->GetFName() == fname )
			{
				ConvertGFxToUProp ( *It, paddr + It->Offset, val, Movie );
			}
		}
		ObjVisitor visitor ( Movie, paddr, ( UStructProperty* ) up );
		val.VisitMembers ( &visitor );

	}
	else if ( Cast<UObjectProperty> ( up ) )
	{
		UObjectProperty* objp = Cast<UObjectProperty> ( up );
		if ( objp->PropertyClass->IsChildOf ( UGFxObject::StaticClass() ) )
		{
			v.ObjectValue = Movie->CreateValueAddRef ( &val, objp->PropertyClass );
			up->SetPropertyValue ( paddr, v );
		}
	}
}

#undef CONVERT_GFX_NUMBERS

extern FGFxEngine* GGFxEngine;
extern UGFxEngine* GGFxGCManager;

#define UGALLOC(s) GALLOC(s, GStat_Default_Mem)

// Helper for dynamic arrays of GFx::Value on stack.
struct FAutoGFxValueArray
{
	unsigned Count;
	GFx::Value *Array;

	FAutoGFxValueArray ( unsigned count, void *p ) : Count ( count ), Array ( ( GFx::Value* ) p )
	{
		for ( unsigned i = 0; i < Count; i++ )
		{
			new ( &Array[i] ) GFx::Value;
		}
	}
	~FAutoGFxValueArray()
	{
		for ( unsigned i = 0; i < Count; i++ )
		{
			Array[i].~Value();
		}
	}
	operator GFx::Value* ()
	{
		return Array;
	}
	GFx::Value& operator[] ( unsigned i )
	{
		return Array[i];
	}
	GFx::Value& operator[] ( int i )
	{
		return Array[i];
	}
};
#define AutoGFxValueArray(name,count) FAutoGFxValueArray name (count,appAlloca((count) * sizeof(GFx::Value)))

#endif // WITH_GFx

#endif // GFxEngine_h
