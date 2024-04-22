/*

This is RAD's high level API for using 3D hardware to do color conversion.
It is supported on PS3, Xbox, Xbox 360, Wii, Windows and GameCube.

It's a nice and simple API, but you should see your platform's example code
to see all the fine details.


There are three main cross platform functions:

  Create_Bink_textures:  This function takes a FBinkTextureSet structure and
    creates the texture resources to render it quickly.
  
  Free_Bink_textures:  Frees the resources allocated in Create_Bink_textures.

  Draw_Bink_textures:  Renders the textures onto the screen.


There are also a few platform specific functions:

  Wait_for_Bink_textures:  On Wii, Xbox, Xbox 360 and PS3, this function 
    waits for the GPU to finish using the given texture set. Call
    before BinkOpen.

  Sync_Bink_textures:  On Wii, Xbox 360, and PS3, this function stores the 
    CPU L2 cache entries for the texture memory, so that the GPU can see it.
    Call this function after BinkDoFrame.
    

  Create_Bink_shaders:  On PS3, Xbox 360 and Windows, this function creates
    the pixel shaders we use to do the color conversion. Call this function
    before the first call to Create_Bink_textures.

  Free_Bink_shaders:  Frees the pixel shaders created in Create_Bink_shaders.


  Lock_Bink_textures:  On Windows, locks the textures so that BinkDoFrame can
    decompress into them.

  Unlock_Bink_textures:  On Windows, unlocks the textures after BinkDoFrame.


So, basically, playback works like this:

  1) Create the pixel shaders on the platforms that need it (PS3, Xbox 360, Win).
  
  2) Open the Bink file with the BINKNOFRAMEBUFFERS flag.  We will use our API
     to create the frame buffers that Bink will use.

  3) Call BinkGetFrameBuffersInfo to get the details on the frame buffers 
     that we need.

  4) Call Create_Bink_textures to create the textures.

  5) Call BinkRegisterFrameBuffers to register these new textures with Bink.

  6) Call Wait_for_Bink_textures before BinkDoFrame (or Lock_Bink_textures 
     on Windows).

  7) Call BinkDoFrame to decompress a video frame.

  8) Call Sync_Bink_textures to flush the cache for the GPU (or 
     Unlock_Bink_textures on Windows).

  9) Draw the frame using Draw_Bink_textures.


And that's it! (Skipping over a few details - see the examples for all 
the details...)

Should drop in really quickly and it hides a ton of platform specific ugliness!

*/

#if USE_BINK_CODEC

/** xenon and windows use the UE3 RHI */
#define BINK_USING_UNREAL_RHI (XBOX || _WINDOWS || USE_NULL_RHI)

typedef struct BINKFRAMETEXTURES
{
#ifdef __RADPS3__
    CellGcmTexture Ytexture;
    CellGcmTexture cRtexture;
    CellGcmTexture cBtexture;
    CellGcmTexture Atexture;
    DWORD fence;
    DWORD Ysize;
    DWORD cRsize;
    DWORD cBsize;
    DWORD Asize;
#elif BINK_USING_UNREAL_RHI
	TArray<BYTE> Ytexture;
	TArray<BYTE> cRtexture;
	TArray<BYTE> cBtexture;
	TArray<BYTE> Atexture;
#endif
} BINKFRAMETEXTURES;


class FBinkTextureSet : public FRenderResource
{
public:

	// this is the GPU info for the textures
	BINKFRAMETEXTURES textures[ BINKMAXFRAMEBUFFERS ];

	// this is the Bink info on the textures
	BINKFRAMEBUFFERS bink_buffers;

#if BINK_USING_UNREAL_RHI
	FTexture2DRHIRef Ytexture;
	FTexture2DRHIRef cRtexture;
	FTexture2DRHIRef cBtexture;
	FTexture2DRHIRef Atexture;
	UBOOL bTriggerMovieRefresh;

	// FRenderResource interface.
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();
#endif
};

//=============================================================================

//
// allocate the textures that we'll need
//
RADDEFFUNC S32 Create_Bink_textures( FBinkTextureSet * set_textures );

// frees the textures
RADDEFFUNC void Free_Bink_textures( FBinkTextureSet * set_textures );

// draws the textures with D3D
RADDEFFUNC void Draw_Bink_textures( FBinkTextureSet * set_textures,
                                    U32 width,
                                    U32 height,
                                    F32 x_offset,
                                    F32 y_offset,
                                    F32 x_scale,
                                    F32 y_scale,
                                    F32 alpha_level,
                                    UBOOL is_premultiplied_alpha,
									UBOOL bIsFullscreen);

//=============================================================================


#if BINK_USING_UNREAL_RHI

  // On Windows, we need to use lock and unlock semantics for best performance

  // Lock the textures for use by D3D
  RADDEFFUNC void Lock_Bink_textures( FBinkTextureSet * set_textures );

  // Unlock the textures after rendering
  RADDEFFUNC void Unlock_Bink_textures( FBinkTextureSet * set_textures, HBINK Bink );  

#else

  // on Xbox, Xenon, Wii, NGC and PS3, we use flush and wait semantics - 
  //   we flush the CPU memory out to the GPU and then
  //   we wait before using the frame again
  
  // make sure the GPU isn't using the next set of textures
  RADDEFFUNC void Wait_for_Bink_textures( FBinkTextureSet * set_textures );

  #ifdef __RADXBOX__
    //
    // on Xbox 1, you can only flush all memory (rather than just a 
    //   specific texture)
    //

    // sync the textures out to main memory (so that the GPU can see them)
    RADDEFFUNC void Sync_all_Bink_textures( void );

  #else

    //
    // on Wii, NGC, Xenon and PS3, we can flush specific textures
    //
  
    // sync the textures out to main memory (so that the GPU can see them)
    RADDEFFUNC void Sync_Bink_textures( FBinkTextureSet * set_textures );

  #endif

#endif

//=============================================================================

#if defined(__RADPS3__)

  //
  // we only have to compile the shaders on Xenon, PS3 and PC, they 
  //   are statically defined on Xbox1, Wii, and NGC
  //

  // creates the couple of shaders that we use
  RADDEFFUNC S32 Create_Bink_shaders( );

  // free our shaders
  RADDEFFUNC void Free_Bink_shaders( void );

#endif

//=============================================================================

// needed in order to compile for all platforms
#if PS3
#define CreateBinkTextures(Param1) Create_Bink_textures(Param1)
#define FreeBinkTextures(Param1) Free_Bink_textures(Param1)
#define CreateBinkShaders() Create_Bink_shaders()
#define FreeBinkShaders() Free_Bink_shaders()
#define LockBinkTextures(Param1)
#define UnlockBinkTextures(Param1,Param2)
#define WaitForBinkTextures(Param1) Wait_for_Bink_textures(Param1)
#define SyncBinkTextures(Param1) Sync_Bink_textures(Param1)
#define DrawBinkTextures(Param1,Param2,Param3,Param4,Param5,Param6,Param7,Param8,Param9,Param10) \
	Draw_Bink_textures(Param1,Param2,Param3,Param4,Param5,Param6,Param7,Param8,Param9,Param10)
#else
#define CreateBinkTextures(Param1) Create_Bink_textures(Param1)
#define FreeBinkTextures(Param1) Free_Bink_textures(Param1)
#define CreateBinkShaders()
#define FreeBinkShaders()
#define DrawBinkTextures(Param1,Param2,Param3,Param4,Param5,Param6,Param7,Param8,Param9,Param10) \
	Draw_Bink_textures(Param1,Param2,Param3,Param4,Param5,Param6,Param7,Param8,Param9,Param10)
#define LockBinkTextures(Param1) Lock_Bink_textures(Param1)
#define UnlockBinkTextures(Param1,Param2) Unlock_Bink_textures(Param1,Param2)
#define WaitForBinkTextures(Param1)
#define SyncBinkTextures(Param1)
#endif

#endif //USE_BINK_CODEC
